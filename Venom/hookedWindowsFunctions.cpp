// SPDX-License-Identifier: GPL-3.0-or-later

#define DIRECTINPUT_VERSION 0x0700
#include <Windows.h>
#include <dinput.h>
#include <ddraw.h>
#include "hookedWindowsFunctions.h"
#include "IDirectDraw1Proxy.h"
#include "IDirectInputAProxy.h"
#include "logging.h"
#include <shared_mutex>
#include <unordered_map>
#include <string>
#include <cstdint>
#include <limits>
#include <excpt.h>
#include <new>
#include <utility>
#include <vector>
#include <spdlog/spdlog.h>
#include "ProxyCommon.h"

static std::string appRoot;
static std::unordered_map<HANDLE, FileContext> fileMap;

HANDLE gpoliceMutex = 0;

static std::shared_mutex fileMapMutex;
static void LockShared() { fileMapMutex.lock_shared(); }
static void UnlockShared() { fileMapMutex.unlock_shared(); }
static void LockExclusive() { fileMapMutex.lock(); }
static void UnlockExclusive() { fileMapMutex.unlock(); }

static void InitAppRoot() {
    static bool initDone = false;
    if (!initDone) {
        char buffer[MAX_PATH] = { 0 };
        DWORD n = GetModuleFileNameA(nullptr, buffer, MAX_PATH);
        if (n > 0) {
            std::string exePath(buffer);
            appRoot = exePath.substr(0, exePath.find_last_of("\\/"));
        } else { appRoot = "."; }
        initDone = true;
    }
}

static bool IsReadWithinOverrideRanges(const OverrideContext &ctx, int64_t offset, DWORD len) {
    if (!ctx.active || offset < 0 || offset > INT64_MAX - static_cast<int64_t>(len)) { return false; }
    int64_t end = offset + static_cast<int64_t>(len);
    if (offset == ctx.headerOffset && len == 0x14) { return true; }
    if (offset >= ctx.assetStart && end <= ctx.assetEnd) { return true; }
    if (offset >= ctx.indicesStart && end <= ctx.indicesEnd) { return true; }
    return false;
}

static void CloseOverride(OverrideContext& ctx) {
    if (ctx.overrideHandle != INVALID_HANDLE_VALUE) {
        ((decltype(&CloseHandle))original_CloseHandle)(ctx.overrideHandle);
        ctx.overrideHandle = INVALID_HANDLE_VALUE;
    }
    ctx.active = false;
    ctx.overridePath.clear();
    ctx.assetSize = 0;
    ctx.linkedAssetCount = 0;
    ctx.headerOffset = 0;
    ctx.indicesStart = ctx.indicesEnd = 0;
    ctx.assetStart = ctx.assetEnd = 0;
}

static bool FindOverridePath(const std::string &overrideDir, int64_t offset, std::string &outPath) {
    std::string decimalName = std::to_string(offset);

    std::stringstream ss;
    ss << std::hex << offset;
    std::string hexName = ss.str();

    std::string hexPrefix = "0x" + hexName;

    std::string variations[] = { decimalName, hexName, hexPrefix };

    for (const std::string &base : variations) {
        std::string exact = overrideDir + "\\" + base;
        if (GetFileAttributesA(exact.c_str()) != INVALID_FILE_ATTRIBUTES) {
            outPath = exact;
            return true;
        }

        std::string pattern = overrideDir + "\\" + base + ".*";
        WIN32_FIND_DATAA fd;
        HANDLE hnd = FindFirstFileA(pattern.c_str(), &fd);
        if (hnd != INVALID_HANDLE_VALUE) {
            outPath = overrideDir + "\\" + fd.cFileName;
            FindClose(hnd);
            return true;
        }
    }
    return false;
}

static BOOL ReadFromOverride(OverrideContext &ctx, int64_t originalReadOffset, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead) {
    if (!ctx.active) return FALSE;
    if (ctx.overrideHandle == INVALID_HANDLE_VALUE) {
        HANDLE h = ((decltype(&CreateFileA))original_CreateFileA)(ctx.overridePath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) { return FALSE; }
        ctx.overrideHandle = h;
    }
    int64_t relative = originalReadOffset - ctx.headerOffset;
    
    if (relative < 0) { return FALSE; }
    
    LONG low = static_cast<LONG>(relative & 0xFFFFFFFF);
    LONG high = static_cast<LONG>((relative >> 32) & 0xFFFFFFFF);
    DWORD res = ((decltype(&SetFilePointer))original_SetFilePointer)(ctx.overrideHandle, low, &high, FILE_BEGIN);
    
    if (res == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) { return FALSE; }
    
    DWORD bytesRead = 0;
    BOOL result = ((decltype(&ReadFile))original_ReadFile)(ctx.overrideHandle, lpBuffer, nNumberOfBytesToRead, &bytesRead, nullptr);
    
    if (lpNumberOfBytesRead) { *lpNumberOfBytesRead = bytesRead; }
    return result;
}


static bool ParseHeaderAndSetRanges(OverrideContext &ctx, int64_t headerOffset, const BYTE* buf20) {
    auto read_le32 = [](const BYTE* p) -> uint32_t {
        return uint32_t(p[0]) | (uint32_t(p[1]) << 8) |
            (uint32_t(p[2]) << 16) | (uint32_t(p[3]) << 24);
    };
    const uint32_t assetSize = read_le32(buf20 + 4);
    const uint32_t linkedCount = read_le32(buf20 + 16);
    constexpr int64_t maxOffset = INT64_MAX;
    constexpr uint32_t maxField = INT32_MAX;
    if (headerOffset < 0 || headerOffset > maxOffset - 20 ||
        assetSize > maxField || linkedCount > maxField) {
        return false;
    }

    const int64_t assetStart = headerOffset + 20;
    if (assetStart > maxOffset - assetSize) { return false; }
    const int64_t assetEnd = assetStart + assetSize;
    if (linkedCount > static_cast<uint64_t>(maxOffset - assetEnd) / 4) { return false; }

    ctx.headerOffset = headerOffset;
    ctx.assetSize = static_cast<int32_t>(assetSize);
    ctx.linkedAssetCount = static_cast<int32_t>(linkedCount);
    ctx.assetStart = assetStart;
    ctx.assetEnd = assetEnd;
    ctx.indicesStart = ctx.assetEnd;
    ctx.indicesEnd = ctx.indicesStart + static_cast<int64_t>(4ULL * linkedCount);
    ctx.active = true;
    return true;
}

static void MaybeDeactivateOverrides(FileContext &fc) {
    std::vector<int64_t> toErase;
    for (auto &kv : fc.activeOverrides) {
        OverrideContext& ctx = kv.second;
        if (ctx.active && fc.currentOffset >= ctx.indicesEnd) {
            CloseOverride(ctx);
            toErase.push_back(kv.first);
        }
    }
    for (auto off : toErase) { fc.activeOverrides.erase(off); }
}

extern "C" FARPROC original_CreateFileA = nullptr;
HANDLE __stdcall hooked_CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile) {
    InitAppRoot();
    HANDLE h = ((decltype(&CreateFileA))original_CreateFileA)(lpFileName, dwDesiredAccess, dwShareMode, lpSecurityAttributes, dwCreationDisposition, dwFlagsAndAttributes, hTemplateFile);
    if (h == INVALID_HANDLE_VALUE) { return h; }
    std::string path = lpFileName ? lpFileName : "";
    LockExclusive();
    fileMap.emplace(h, FileContext(path, appRoot));
    UnlockExclusive();
    return h;
}

extern "C" FARPROC original_SetFilePointer = nullptr;
DWORD __stdcall hooked_SetFilePointer(HANDLE hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod) {
    std::unique_lock<std::shared_mutex> stateLock(fileMapMutex);
    DWORD result = ((decltype(&SetFilePointer))original_SetFilePointer)(hFile, lDistanceToMove, lpDistanceToMoveHigh, dwMoveMethod);
    if (result == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR) { return result; }

    auto it = fileMap.find(hFile);
    if (it == fileMap.end()) { return result; }

    FileContext& fc = it->second;
    if (lpDistanceToMoveHigh) {
        const uint64_t position = (uint64_t(static_cast<uint32_t>(*lpDistanceToMoveHigh)) << 32) | result;
        if (position <= static_cast<uint64_t>(INT64_MAX)) {
            fc.currentOffset = static_cast<int64_t>(position);
        }
    } else if (dwMoveMethod == FILE_BEGIN) {
        fc.currentOffset = result;
    } else if (dwMoveMethod == FILE_CURRENT) {
        fc.currentOffset += lDistanceToMove;
    } else if (dwMoveMethod == FILE_END) {
        if (!fc.fileSizeCached) {
            BY_HANDLE_FILE_INFORMATION info{};
            if (GetFileInformationByHandle(hFile, &info)) {
                const uint64_t fileSize = (uint64_t(info.nFileSizeHigh) << 32) | info.nFileSizeLow;
                if (fileSize <= static_cast<uint64_t>(INT64_MAX)) {
                    fc.cachedFileSize = static_cast<int64_t>(fileSize);
                    fc.fileSizeCached = true;
                }
            }
        }
        const int64_t base = fc.fileSizeCached ? fc.cachedFileSize : 0;
        fc.currentOffset = base + lDistanceToMove;
    }
    return result;
}

extern "C" FARPROC original_ReadFile = nullptr;
BOOL __stdcall hooked_ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped) {
    InitAppRoot();
    if (lpOverlapped) { return ((decltype(&ReadFile))original_ReadFile)(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, lpOverlapped); }
    std::unique_lock<std::shared_mutex> stateLock(fileMapMutex);
    auto fileIt = fileMap.find(hFile);
    if (fileIt == fileMap.end()) {
        return ((decltype(&ReadFile))original_ReadFile)(hFile, lpBuffer, nNumberOfBytesToRead, lpNumberOfBytesRead, nullptr);
    }
    FileContext& fc = fileIt->second;
    const int64_t currentOffset = fc.currentOffset;

    BOOL result = FALSE;
    DWORD bytesRead = 0;
    if (nNumberOfBytesToRead == 20) {
        std::string overridePath;
        if (FindOverridePath(fc.overrideDir, currentOffset, overridePath)) {
            OverrideContext ctx;
            ctx.overridePath = overridePath;
            ctx.overrideHandle = INVALID_HANDLE_VALUE;
            ctx.headerOffset = currentOffset;
            ctx.active = true;
            result = ReadFromOverride(ctx, currentOffset, lpBuffer, nNumberOfBytesToRead, &bytesRead);

            if (!result || bytesRead != nNumberOfBytesToRead ||
                !ParseHeaderAndSetRanges(ctx, currentOffset, reinterpret_cast<const BYTE*>(lpBuffer))) {
                CloseOverride(ctx);
                bytesRead = 0;
                result = ((decltype(&ReadFile))original_ReadFile)(hFile, lpBuffer, nNumberOfBytesToRead, &bytesRead, nullptr);
            } else {
                auto existing = fc.activeOverrides.find(currentOffset);
                if (existing != fc.activeOverrides.end()) { CloseOverride(existing->second); }
                fc.activeOverrides.insert_or_assign(currentOffset, std::move(ctx));
            }
        }
        else { result = ((decltype(&ReadFile))original_ReadFile)(hFile, lpBuffer, nNumberOfBytesToRead, &bytesRead, nullptr); }
    }
    else {
        auto match = fc.activeOverrides.end();
        for (auto it = fc.activeOverrides.begin(); it != fc.activeOverrides.end(); ++it) {
            if (IsReadWithinOverrideRanges(it->second, currentOffset, nNumberOfBytesToRead)) {
                match = it;
                break;
            }
        }
        if (match != fc.activeOverrides.end()) {
            result = ReadFromOverride(match->second, currentOffset, lpBuffer, nNumberOfBytesToRead, &bytesRead);
            if (!result) {
                CloseOverride(match->second);
                fc.activeOverrides.erase(match);
                bytesRead = 0;
                result = ((decltype(&ReadFile))original_ReadFile)(hFile, lpBuffer, nNumberOfBytesToRead, &bytesRead, nullptr);
            }
        }
        else { result = ((decltype(&ReadFile))original_ReadFile)(hFile, lpBuffer, nNumberOfBytesToRead, &bytesRead, nullptr); }
    }

    if (lpNumberOfBytesRead) { *lpNumberOfBytesRead = bytesRead; }

    if (result) {
        fc.currentOffset += bytesRead;
        MaybeDeactivateOverrides(fc);
    }
    return result;
}

extern "C" FARPROC original_CloseHandle = nullptr;
BOOL __stdcall hooked_CloseHandle(HANDLE hObject) {
    LockExclusive();
    auto it = fileMap.find(hObject);
    if (it != fileMap.end()) {
        for (auto &kv : it->second.activeOverrides) { CloseOverride(kv.second); }
        it->second.activeOverrides.clear();
        fileMap.erase(it);
    }
    UnlockExclusive();
    return ((decltype(&CloseHandle))original_CloseHandle)(hObject);
}

extern "C" FARPROC original_CreateWindowExA = nullptr;
HWND __stdcall hooked_CreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int x, int y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance, LPVOID lpParam) {
	spdlog::trace("hooked_CreateWindowExA called: ClassName={}, WindowName={}, Style=0x{:X}, ExStyle=0x{:X}, x={}, y={}, Width={}, Height={}", lpClassName ? lpClassName : "[null]", lpWindowName ? lpWindowName : "[null]", dwStyle, dwExStyle, x, y, nWidth, nHeight);
	return ((decltype(&CreateWindowExA))original_CreateWindowExA)(dwExStyle, lpClassName, lpWindowName, dwStyle, x, y, nWidth, nHeight, hWndParent, hMenu, hInstance, lpParam);
}

extern "C" FARPROC original_DirectDrawCreate = nullptr;
HRESULT __stdcall hooked_DirectDrawCreate(GUID* lpGUID, IDirectDraw** lplpDD, IUnknown* pUnkOuter)
{
    HRESULT hr = ((decltype(&DirectDrawCreate))original_DirectDrawCreate)(lpGUID, lplpDD, pUnkOuter);

    if (FAILED(hr) || !lplpDD || !*lplpDD) {
        spdlog::error("hooked_DirectDrawCreate: original_DirectDrawCreate failed");
        return hr;
    };

    IDirectDraw1Proxy* dd1Proxy = new (std::nothrow) IDirectDraw1Proxy(*lplpDD);
    if (!dd1Proxy) {
        spdlog::error("hooked_DirectDrawCreate: Failed to create IDirectDraw1Proxy");
        return hr;
    }
    *lplpDD = dd1Proxy;
    return hr;
}

extern "C" FARPROC original_DirectInputCreateA = nullptr;
HRESULT __stdcall hooked_DirectInputCreateA(HINSTANCE hinst, DWORD dwVersion, LPDIRECTINPUTA* lplpDirectInput, LPUNKNOWN punkOuter) {
    HRESULT hr = ((decltype(&DirectInputCreateA))original_DirectInputCreateA)(hinst, dwVersion, lplpDirectInput, punkOuter);
    if (FAILED(hr) || !lplpDirectInput || !*lplpDirectInput) {
        spdlog::error("hooked_DirectInputCreateA: original_DirectInputCreateA failed");
        return hr;
    };
    
    IDirectInputAProxy* diProxy = new (std::nothrow) IDirectInputAProxy(*lplpDirectInput);
    if (!diProxy) {
        spdlog::error("hooked_DirectInputCreateA: Failed to create IDirectInputAProxy");
        return hr;
    }
    *lplpDirectInput = diProxy;
    
    return hr;
}

extern "C" FARPROC original_LoadIconA = nullptr;
HICON __stdcall hooked_LoadIconA(HINSTANCE hInstance, LPCSTR lpIconName) {
    if (lpIconName == MAKEINTRESOURCEA(101)) {
        spdlog::debug("hooked_LoadIconA: Replacing null icon with ICON_GP");
        lpIconName = "ICON_GP";
    }
    return ((decltype(&LoadIconA))original_LoadIconA)(hInstance, lpIconName);
}

bool IsFirstInstance(LPCSTR lpName) {
    if (!gpoliceMutex) {
        gpoliceMutex = CreateMutexA(0, true, lpName);
        return (GetLastError() != ERROR_ALREADY_EXISTS);
    }
    return true;
}

extern "C" FARPROC original_OpenMutexA = nullptr;
HANDLE __stdcall hooked_OpenMutexA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpName) {
    if (!IsFirstInstance(lpName)) {
        spdlog::warn("Multiple G-Police instances. Disable the mutex fix to allow.");
        return OpenMutexA(MUTEX_ALL_ACCESS, false, lpName);
    }
    return 0;
}

extern "C" FARPROC original_CreateMutexA = nullptr;
HANDLE __stdcall hooked_CreateMutexA(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCSTR lpName) {
    IsFirstInstance(lpName);
    return ((decltype(&CreateMutexA))original_CreateMutexA)(lpMutexAttributes, bInitialOwner, lpName);
}

static constexpr DWORD MAX_LOG_BYTES = 512 * 512;
static bool IsPrintable(const char* data, DWORD length) {
    if (!data || length == 0 || length > MAX_LOG_BYTES) { return false; }
    __try {
        for (DWORD i = 0; i < length; ++i) {
            unsigned char c = static_cast<unsigned char>(data[i]);
            if (!(c == '\r' || c == '\n' || c == '\t' || (c >= 32 && c < 127))) { return false; }
        }
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

static bool IsConsoleHandle(HANDLE h) {
    if (h == nullptr || h == INVALID_HANDLE_VALUE) return false;
    DWORD type = GetFileType(h);
    return type == FILE_TYPE_CHAR;
}

extern "C" FARPROC original_WriteFile = nullptr;
BOOL __stdcall hooked_WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped) {
    const char* data = static_cast<const char*>(lpBuffer);

    bool isStdErr = (hFile == GetStdHandle(STD_ERROR_HANDLE));
    bool isStdOut = (hFile == GetStdHandle(STD_OUTPUT_HANDLE));

    if ((isStdOut || isStdErr || IsConsoleHandle(hFile)) && IsPrintable(data, nNumberOfBytesToWrite)) {
        DWORD to_log = nNumberOfBytesToWrite;
        if (to_log > MAX_LOG_BYTES) to_log = MAX_LOG_BYTES;
        std::string msg(data, data ? to_log : 0);
        if (isStdErr) { spdlog::error("[G-Police stderr] {}", msg); }
        else if (isStdOut) { spdlog::info("[G-Police stdout] {}", msg); }
        else { spdlog::info("[G-Police Console] {}", msg); }
    }
    return ((decltype(&WriteFile))original_WriteFile)(hFile, lpBuffer, nNumberOfBytesToWrite, lpNumberOfBytesWritten, lpOverlapped);
}

extern "C" FARPROC original_WriteConsoleA = nullptr;
BOOL __stdcall hooked_WriteConsoleA(HANDLE hConsoleOutput, LPCVOID lpBuffer, DWORD nNumberOfCharsToWrite, LPDWORD lpNumberOfCharsWritten, LPVOID lpReserved) {
    const char* data = static_cast<const char*>(lpBuffer);

    if (IsPrintable(data, nNumberOfCharsToWrite)) {
        DWORD to_log = nNumberOfCharsToWrite;
        if (to_log > MAX_LOG_BYTES) to_log = MAX_LOG_BYTES;
        std::string msg(data, data ? to_log : 0);
        spdlog::info("[G-Police Console] {}", msg);
    }
    return ((decltype(&WriteConsoleA))original_WriteConsoleA)(hConsoleOutput, lpBuffer, nNumberOfCharsToWrite, lpNumberOfCharsWritten, lpReserved);
}


extern "C" FARPROC original_mciSendCommandA = nullptr;
MCIERROR __stdcall hooked_mciSendCommandA(MCIDEVICEID IDDevice, UINT uMsg, DWORD_PTR fdwCommand, DWORD_PTR dwParam) {
    //LogMCICommand(IDDevice, uMsg, fdwCommand, dwParam);
    MCIERROR result = MMSYSERR_NOERROR;

    switch (uMsg) {
    case MCI_OPEN:
        result = decltype(&mciSendCommandA)(original_mciSendCommandA)(IDDevice, uMsg, fdwCommand, dwParam);
        break;

    case MCI_CLOSE:
        result = decltype(&mciSendCommandA)(original_mciSendCommandA)(IDDevice, uMsg, fdwCommand, dwParam);
        break;

    case MCI_PLAY:
        break;

    case MCI_STOP:
        break;

    case MCI_PAUSE:
        break;

    case MCI_RESUME:
        break;

    case MCI_SEEK:
        break;

    case MCI_SET:
        break;

    case MCI_STATUS:
        result = decltype(&mciSendCommandA)(original_mciSendCommandA)(IDDevice, uMsg, fdwCommand, dwParam);
        break;

    default:
        return MMSYSERR_NOERROR;

    }
    //result = decltype(&mciSendCommandA)(original_mciSendCommandA)(IDDevice, uMsg, fdwCommand, dwParam);
    //LogMCIResult(result);
    return result;
}

extern "C" FARPROC original_GetProcAddress = nullptr;
FARPROC __stdcall hooked_GetProcAddress(HMODULE hModule, LPCSTR lpProcName) {
    spdlog::debug("GetProcAddress called: hModule = {}, lpProcName = {}", hModule ? std::to_string((uintptr_t)hModule) : "nullptr", lpProcName);
    return ((decltype(&GetProcAddress))original_GetProcAddress)(hModule, lpProcName);
}

extern "C" HMODULE original_LoadLibraryA = nullptr;
HMODULE __stdcall hooked_LoadLibraryA(LPCSTR lpLibFileName) {
    spdlog::debug("LoadLibraryA called: lpLibFileName = {}", lpLibFileName);
    return ((decltype(&LoadLibraryA))original_LoadLibraryA)(lpLibFileName);
}

extern "C" HMODULE original_LoadLibraryW = nullptr;
HMODULE __stdcall hooked_LoadLibraryW(LPCWSTR lpLibFileName) {
    spdlog::debug("LoadLibraryW called: lpLibFileName = {}", WideStringToString(lpLibFileName));
    return ((decltype(&LoadLibraryW))original_LoadLibraryW)(lpLibFileName);
}