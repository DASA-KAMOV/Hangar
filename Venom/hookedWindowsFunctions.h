// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0700
#include <ddraw.h>
#include <windows.h>
#include <string>
#include <unordered_map>
#include <cstdint>
#include <dinput.h>

#pragma comment(lib, "kernel32.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "ddraw.lib")
#pragma comment(lib, "Dbghelp.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "mmdevapi.lib")
#pragma comment(lib, "dsound.lib")

extern "C" FARPROC original_CloseHandle;
extern "C" FARPROC original_CreateFileA;
extern "C" FARPROC original_CreateMutexA;
extern "C" FARPROC original_CreateWindowExA;
extern "C" FARPROC original_DirectDrawCreate;
extern "C" FARPROC original_DirectInputCreateA;
extern "C" FARPROC original_GetProcAddress;
extern "C" HMODULE original_LoadLibraryA;
extern "C" HMODULE original_LoadLibraryW;
extern "C" FARPROC original_LoadIconA;
extern "C" FARPROC original_mciSendCommandA;
extern "C" FARPROC original_OpenMutexA;
extern "C" FARPROC original_ReadFile;
extern "C" FARPROC original_SetFilePointer;
extern "C" FARPROC original_WriteConsoleA;
extern "C" FARPROC original_WriteFile;

inline std::string GetBaseName(const std::string &path) {
    size_t p1 = path.find_last_of("\\/");
    if (p1 == std::string::npos) return path;
    return path.substr(p1 + 1);
}

struct OverrideContext {
    std::string overridePath;
    HANDLE      overrideHandle = INVALID_HANDLE_VALUE;
    int64_t     headerOffset = 0;
    int32_t     assetSize = 0;
    int32_t     linkedAssetCount = 0;
    bool        active = false;
    int64_t     indicesStart = 0;
    int64_t     indicesEnd = 0;
    int64_t     assetStart = 0;
    int64_t     assetEnd = 0;
};

struct FileContext {
    std::string originalPath;
    std::string baseName;
    std::string overrideDir;
    int64_t     currentOffset = 0;
    std::unordered_map<int64_t, OverrideContext> activeOverrides;
    DWORD       cachedFileType = 0;
    int64_t     cachedFileSize = -1;
    bool        fileTypeCached = false;
    bool        fileSizeCached = false;
    FileContext() = default;
    FileContext(const std::string &path, const std::string &appRoot): originalPath(path), baseName(GetBaseName(path)), currentOffset(0) {
        overrideDir = appRoot + "\\override\\" + baseName;
    }
};

bool PatchIATEntry(HMODULE hModule, const char* targetDLL, const char* targetFunc, PROC newFunc, PROC &originalFuncOut);

BOOL __stdcall hooked_CloseHandle(HANDLE hObject);
HANDLE __stdcall hooked_CreateFileA(LPCSTR lpFileName, DWORD dwDesiredAccess, DWORD dwShareMode, LPSECURITY_ATTRIBUTES lpSecurityAttributes, DWORD dwCreationDisposition, DWORD dwFlagsAndAttributes, HANDLE hTemplateFile);
HANDLE __stdcall hooked_CreateMutexA(LPSECURITY_ATTRIBUTES lpMutexAttributes, BOOL bInitialOwner, LPCSTR lpName);
HWND __stdcall hooked_CreateWindowExA(DWORD dwExStyle, LPCSTR lpClassName, LPCSTR lpWindowName, DWORD dwStyle, int x, int y, int nWidth, int nHeight, HWND hWndParent, HMENU hMenu,	HINSTANCE hInstance, LPVOID lpParam);
HRESULT __stdcall hooked_DirectDrawCreate(GUID* lpGUID, IDirectDraw** lplpDD, IUnknown* pUnkOuter);
HRESULT __stdcall hooked_DirectInputCreateA(HINSTANCE hinst, DWORD dwVersion, LPDIRECTINPUTA* lplpDirectInput, LPUNKNOWN punkOuter);
FARPROC __stdcall hooked_GetProcAddress(HMODULE hModule, LPCSTR lpProcName);
HMODULE __stdcall hooked_LoadLibraryA(LPCSTR lpLibFileName);
HMODULE __stdcall hooked_LoadLibraryW(LPCWSTR lpLibFileName);
HICON __stdcall hooked_LoadIconA(HINSTANCE hInstance, LPCSTR lpIconName);
MCIERROR __stdcall hooked_mciSendCommandA(MCIDEVICEID IDDevice, UINT uMsg, DWORD_PTR fdwCommand, DWORD_PTR dwParam2);
HANDLE __stdcall hooked_OpenMutexA(DWORD dwDesiredAccess, BOOL bInheritHandle, LPCSTR lpName);
BOOL __stdcall hooked_ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesToRead, LPOVERLAPPED lpOverlapped);
DWORD __stdcall hooked_SetFilePointer(HANDLE  hFile, LONG lDistanceToMove, PLONG lpDistanceToMoveHigh, DWORD dwMoveMethod);
BOOL __stdcall hooked_WriteConsoleA(HANDLE hConsoleOutput, LPCVOID lpBuffer, DWORD nNumberOfCharsToWrite, LPDWORD lpNumberOfCharsWritten, LPVOID lpReserved);
BOOL __stdcall hooked_WriteFile(HANDLE hFile, LPCVOID lpBuffer, DWORD nNumberOfBytesToWrite, LPDWORD lpNumberOfBytesWritten, LPOVERLAPPED lpOverlapped);