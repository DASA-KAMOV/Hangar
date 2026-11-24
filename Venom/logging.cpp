// SPDX-License-Identifier: GPL-3.0-or-later

#define DIRECTINPUT_VERSION 0x0700
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include "logging.h"
#include <ddraw.h>
#include <Windows.h>
#include <cstdio>
#include <excpt.h>
#include <memory>
#include <d3d.h>
#include <dsound.h>
#include <dinput.h>
#include "ProxyCommon.h"
#include <spdlog/common.h>
#include <spdlog/logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog-inl.h>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/bundled/format.h>
#include <spdlog/fmt/std.h>
#include <cstdarg>

void CreateConsole(std::wstring title) {
    if (AllocConsole()) {
        FILE *fpstdin = stdin, *fpstdout = stdout, *fpstderr = stderr;
        freopen_s(&fpstdin, "CONIN$", "r", stdin);
        freopen_s(&fpstdout, "CONOUT$", "w", stdout);
        freopen_s(&fpstderr, "CONOUT$", "w", stderr);

        std::ios::sync_with_stdio();
        std::cin.clear();
        std::cout.clear();
        std::cerr.clear();

        SetConsoleTitleW(title.c_str());
    }
}

void InitSpdlog(const std::string &filePath, spdlog::level::level_enum fileLevel, spdlog::level::level_enum consoleLevel) {
    std::vector<spdlog::sink_ptr> sinks;
    std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> consoleSink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    consoleSink->set_level(consoleLevel);
    consoleSink->set_pattern("[%^%l%$] %v");
    sinks.push_back(consoleSink);

    std::string lowerFilePath = filePath;
    std::transform(lowerFilePath.begin(), lowerFilePath.end(), lowerFilePath.begin(), [](unsigned char c) { return std::tolower(c); });

    if (!lowerFilePath.empty() && lowerFilePath != "none" && lowerFilePath != "false") {
        std::shared_ptr<spdlog::sinks::basic_file_sink_mt>  fileSink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(filePath, true);
        fileSink->set_level(fileLevel);
        //fileSink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
        fileSink->set_pattern("[%l] %v");
        sinks.push_back(fileSink);
    }

    std::shared_ptr<spdlog::logger>  logger = std::make_shared<spdlog::logger>("multi_logger", sinks.begin(), sinks.end());
    logger->set_level((fileLevel < consoleLevel) ? fileLevel : consoleLevel);
    logger->flush_on(spdlog::level::err);
    spdlog::register_logger(logger);
    spdlog::set_default_logger(logger);
}

std::string WideStringToString(LPCWSTR wide_string) {
    if (!wide_string) { return "[nullptr]"; }
    int bufferSize = WideCharToMultiByte(CP_UTF8, 0, wide_string, -1, NULL, 0, NULL, NULL);
    if (bufferSize <= 0) { return "[Conversion of Wide String to String failed]"; }
    std::vector<char> buffer(bufferSize);
    WideCharToMultiByte(CP_UTF8, 0, wide_string, -1, buffer.data(), bufferSize, NULL, NULL);
    return std::string(buffer.data());
}

void LogLastError(DWORD err) {
    if (err == 0) { return; }
    LPVOID msgBuf = nullptr;
    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), reinterpret_cast<LPSTR>(&msgBuf), 0, nullptr);
    spdlog::error("GetLastError: {} {}", err, msgBuf ? reinterpret_cast<LPSTR>(msgBuf) : "[No message available]");
    LocalFree(msgBuf);
}

void LogLastError() { LogLastError(GetLastError()); }

void DumpBytes(const BYTE* address, int length) {
    std::string hex;
    hex.reserve(length * 3);
    for (int i = 0; i < length; i++) {
        hex += fmt::format("{:02X} ", address[i]);
    }
    spdlog::info("Bytes @ 0x{:08X}: {}", reinterpret_cast<uintptr_t>(address), hex);
}

std::string REFIIDToString(REFIID riid) {
    // DirectDraw
    if (IsEqualIID(riid, IID_IDirectDraw)) return "IID_IDirectDraw";
    if (IsEqualIID(riid, IID_IDirectDraw2)) return "IID_IDirectDraw2";
    if (IsEqualIID(riid, IID_IDirectDraw4)) return "IID_IDirectDraw4";
    if (IsEqualIID(riid, IID_IDirectDraw7)) return "IID_IDirectDraw7";
    if (IsEqualIID(riid, IID_IDirectDrawSurface)) return "IID_IDirectDrawSurface";
    if (IsEqualIID(riid, IID_IDirectDrawSurface2)) return "IID_IDirectDrawSurface2";
    if (IsEqualIID(riid, IID_IDirectDrawSurface3)) return "IID_IDirectDrawSurface3";
    if (IsEqualIID(riid, IID_IDirectDrawSurface4)) return "IID_IDirectDrawSurface4";
    if (IsEqualIID(riid, IID_IDirectDrawSurface7)) return "IID_IDirectDrawSurface7";
    if (IsEqualIID(riid, IID_IDirectDrawPalette)) return "IID_IDirectDrawPalette";
    if (IsEqualIID(riid, IID_IDirectDrawClipper)) return "IID_IDirectDrawClipper";
    if (IsEqualIID(riid, IID_IDirectDrawColorControl)) return "IID_IDirectDrawColorControl";
    if (IsEqualIID(riid, IID_IDirectDrawGammaControl)) return "IID_IDirectDrawGammaControl";

    // Direct3D
    if (IsEqualIID(riid, IID_IDirect3D)) return "IID_IDirect3D";
    if (IsEqualIID(riid, IID_IDirect3D2)) return "IID_IDirect3D2";
    if (IsEqualIID(riid, IID_IDirect3D3)) return "IID_IDirect3D3";
    if (IsEqualIID(riid, IID_IDirect3D7)) return "IID_IDirect3D7";
    if (IsEqualIID(riid, IID_IDirect3DDevice)) return "IID_IDirect3DDevice";
    if (IsEqualIID(riid, IID_IDirect3DDevice2)) return "IID_IDirect3DDevice2";
    if (IsEqualIID(riid, IID_IDirect3DDevice3)) return "IID_IDirect3DDevice3";
    if (IsEqualIID(riid, IID_IDirect3DDevice7)) return "IID_IDirect3DDevice7";
    if (IsEqualIID(riid, IID_IDirect3DHALDevice)) return "IID_IDirect3DHALDevice";
    if (IsEqualIID(riid, IID_IDirect3DTnLHalDevice)) return "IID_IDirect3DTnLHalDevice";
    if (IsEqualIID(riid, IID_IDirect3DRampDevice)) return "IID_IDirect3DRampDevice";
    if (IsEqualIID(riid, IID_IDirect3DRGBDevice)) return "IID_IDirect3DRGBDevice";
    if (IsEqualIID(riid, IID_IDirect3DMMXDevice)) return "IID_IDirect3DMMXDevice";
    if (IsEqualIID(riid, IID_IDirect3DRefDevice)) return "IID_IDirect3DRefDevice";
    if (IsEqualIID(riid, IID_IDirect3DNullDevice)) return "IID_IDirect3DNullDevice";
    if (IsEqualIID(riid, IID_IDirect3DViewport)) return "IID_IDirect3DViewport";
    if (IsEqualIID(riid, IID_IDirect3DViewport2)) return "IID_IDirect3DViewport2";
	if (IsEqualIID(riid, IID_IDirect3DViewport3)) return "IID_IDirect3DViewport3";
    if (IsEqualIID(riid, IID_IDirect3DMaterial)) return "IID_IDirect3DMaterial";
    if (IsEqualIID(riid, IID_IDirect3DMaterial2)) return "IID_IDirect3DMaterial2";
    if (IsEqualIID(riid, IID_IDirect3DMaterial3)) return "IID_IDirect3DMaterial3";
    if (IsEqualIID(riid, IID_IDirect3DLight)) return "IID_IDirect3DLight";
    if (IsEqualIID(riid, IID_IDirect3DTexture)) return "IID_IDirect3DTexture";
    if (IsEqualIID(riid, IID_IDirect3DTexture2)) return "IID_IDirect3DTexture2";
    if (IsEqualIID(riid, IID_IDirect3DExecuteBuffer)) return "IID_IDirect3DExecuteBuffer";
    if (IsEqualIID(riid, IID_IDirect3DVertexBuffer)) return "IID_IDirect3DVertexBuffer";
    if (IsEqualIID(riid, IID_IDirect3DVertexBuffer7)) return "IID_IDirect3DVertexBuffer7";

    // DirectSound
    if (IsEqualIID(riid, IID_IDirectSound)) return "IID_IDirectSound";
    if (IsEqualIID(riid, IID_IDirectSoundBuffer)) return "IID_IDirectSoundBuffer";
    if (IsEqualIID(riid, IID_IDirectSound3DListener)) return "IID_IDirectSound3DListener";
    if (IsEqualIID(riid, IID_IDirectSound3DBuffer)) return "IID_IDirectSound3DBuffer";
    if (IsEqualIID(riid, IID_IDirectSoundCapture)) return "IID_IDirectSoundCapture";
    if (IsEqualIID(riid, IID_IDirectSoundNotify)) return "IID_IDirectSoundNotify";

    // DirectInput
    if (IsEqualIID(riid, IID_IDirectInputA)) return "IID_IDirectInputA";
    if (IsEqualIID(riid, IID_IDirectInputW)) return "IID_IDirectInputW";
    if (IsEqualIID(riid, IID_IDirectInput2A)) return "IID_IDirectInput2A";
    if (IsEqualIID(riid, IID_IDirectInput2W)) return "IID_IDirectInput2W";
    if (IsEqualIID(riid, IID_IDirectInput7A)) return "IID_IDirectInput7A";
    if (IsEqualIID(riid, IID_IDirectInput7W)) return "IID_IDirectInput7W";
    if (IsEqualIID(riid, IID_IDirectInputDeviceA)) return "IID_IDirectInputDeviceA";
    if (IsEqualIID(riid, IID_IDirectInputDeviceW)) return "IID_IDirectInputDeviceW";
    if (IsEqualIID(riid, IID_IDirectInput8A)) return "IID_IDirectInput8A";
    if (IsEqualIID(riid, IID_IDirectInput8W)) return "IID_IDirectInput8W";
	if (IsEqualIID(riid, IID_IDirectInputDevice8A)) return "IID_IDirectInputDevice8A";
	if (IsEqualIID(riid, IID_IDirectInputDevice8W)) return "IID_IDirectInputDevice8W";
	if (IsEqualIID(riid, IID_IDirectInputEffect)) return "IID_IDirectInputEffect";

    // Other
    if (IsEqualIID(riid, IID_IUnknown)) return "IID_IUnknown";
    if (IsEqualIID(riid, IID_IDispatch)) return "IID_IDispatch";

    if (IsEqualIID(riid, IID_IProxyUnwrap)) return "IID_ProxyUnwrap";

    // Fallback: Return the canonical GUID string
    wchar_t wszGuid[40] = { 0 };
    if (StringFromGUID2(riid, wszGuid, ARRAYSIZE(wszGuid)) > 0) {
        return WideStringToString(wszGuid);
    }
    return std::string();
}

void LogSurfaceDetails(const char* functionName, DWORD flags, LPDDCOLORKEY lpDDColorKey, LPDDSURFACEDESC lpDDSurfaceDesc) {
    if (lpDDColorKey != nullptr) {
        spdlog::trace("Color Key Flags: {:#010x}", flags);
        spdlog::trace("Color Space Low: {:#010x}", lpDDColorKey->dwColorSpaceLowValue);
        spdlog::trace("Color Space High: {:#010x}", lpDDColorKey->dwColorSpaceHighValue);
    }
    if (lpDDSurfaceDesc != nullptr) {
        spdlog::trace("Surface Width: {} | Height: {}", lpDDSurfaceDesc->dwWidth, lpDDSurfaceDesc->dwHeight);
        spdlog::trace("Pixel Format Flags: {:#010x}", lpDDSurfaceDesc->ddpfPixelFormat.dwFlags);
        spdlog::trace("RGB Bit Count: {}", lpDDSurfaceDesc->ddpfPixelFormat.dwRGBBitCount);

        if (lpDDSurfaceDesc->ddpfPixelFormat.dwFlags & DDPF_PALETTEINDEXED8) {
            spdlog::trace("Note: This is an 8-bit Palettized Surface!");
        }
    }
}

static const char* TimeFormatName(UINT fmt) {
    // Standard exact formats
    switch (fmt) {
    case MCI_FORMAT_MILLISECONDS: return "MILLISECONDS";
    case MCI_FORMAT_TMSF:         return "TMSF";
    case MCI_FORMAT_MSF:          return "MSF";
    case MCI_FORMAT_FRAMES:       return "FRAMES";
    case MCI_FORMAT_HMS:          return "HMS";
    case MCI_FORMAT_BYTES:        return "BYTES";
    case MCI_FORMAT_SAMPLES:      return "SAMPLES";
    }

    // SMPTE families (bitmask ranges)
    if ((fmt & 0xFF00) == 0x0100) return "SMPTE_24";
    if ((fmt & 0xFF00) == 0x0200) return "SMPTE_25";
    if ((fmt & 0xFF00) == 0x0300) return "SMPTE_30";
    if ((fmt & 0xFF00) == 0x0400) return "SMPTE_30DROP";

    // Vendor-specific / undefined formats
    if (fmt >= 0x200 && fmt <= 0x2FF) { return "VENDOR_SPECIFIC_0x2xx"; }

    return "UNKNOWN";
}

static const char* GetDevCapsItemName(DWORD item) {
    switch (item) {
    case MCI_GETDEVCAPS_CAN_RECORD:         return "CAN_RECORD";
    case MCI_GETDEVCAPS_HAS_AUDIO:          return "HAS_AUDIO";
    case MCI_GETDEVCAPS_HAS_VIDEO:          return "HAS_VIDEO";
    case MCI_GETDEVCAPS_DEVICE_TYPE:        return "DEVICE_TYPE";
    case MCI_GETDEVCAPS_USES_FILES:         return "USES_FILES";
    case MCI_GETDEVCAPS_COMPOUND_DEVICE:    return "COMPOUND_DEVICE";
    case MCI_GETDEVCAPS_CAN_EJECT:          return "CAN_EJECT";
    case MCI_GETDEVCAPS_CAN_PLAY:           return "CAN_PLAY";
    case MCI_GETDEVCAPS_CAN_SAVE:           return "CAN_SAVE";
    default: return nullptr;
    }
}

static void LogMciNotifyCallback(DWORD_PTR dwParam) {
    if (!dwParam) return;
    HWND hwnd = reinterpret_cast<HWND>(dwParam);
    if (!hwnd || !IsWindow(hwnd)) {
		spdlog::trace(" |- MCI_NOTIFY hwndCallback: 0x{:X} (null or not a valid window)", (UINT_PTR)hwnd); // G-Police has this problem with some callbacks, so log it but don’t treat it as an error.
        return;
    }
    else {
        char cls[128] = { 0 };
        char title[256] = { 0 };
        GetClassNameA(hwnd, cls, sizeof(cls));
        GetWindowTextA(hwnd, title, sizeof(title));
        spdlog::trace(" |- MCI_NOTIFY hwndCallback: {} class='{}' title='{}'", (void*)hwnd, cls, title);
    }
}

static bool IsTypeId(LPCSTR lp) {
    return lp && HIWORD(lp) == 0;
}
static UINT DeviceTypeId(LPCSTR lp) {
    return static_cast<UINT>(LOWORD(lp));
}

static const char* StatusItemName(DWORD item) {
    switch (item) {
    case MCI_STATUS_CURRENT_TRACK:     return "CURRENT_TRACK";
    case MCI_STATUS_LENGTH:            return "LENGTH";
    case MCI_STATUS_MODE:              return "MODE";
    case MCI_STATUS_NUMBER_OF_TRACKS:  return "NUMBER_OF_TRACKS";
    case MCI_STATUS_POSITION:          return "POSITION";
    case MCI_STATUS_MEDIA_PRESENT:     return "MEDIA_PRESENT";
    case MCI_STATUS_TIME_FORMAT:       return "TIME_FORMAT";
    case MCI_STATUS_READY:             return "READY";
    default:                           return "UNKNOWN_ITEM";
    }
}

void LogMCICommand(MCIDEVICEID IDDevice, UINT uMsg, DWORD_PTR fdwCommand, DWORD_PTR dwParam)
{
    switch (uMsg) {
    case MCI_OPEN: {
        spdlog::debug("mciSendCommandA: MCI_OPEN (Device ID: {})", IDDevice);
        auto p = reinterpret_cast<LPMCI_OPEN_PARMSA>(dwParam);
        if (!p) break;

        if (fdwCommand & MCI_OPEN_TYPE)      spdlog::trace(" |- Open: TYPE");
        if (fdwCommand & MCI_OPEN_TYPE_ID)   spdlog::trace(" |- Open: TYPE_ID");
        if (fdwCommand & MCI_OPEN_ELEMENT)   spdlog::trace(" |- Open: ELEMENT");
        if (fdwCommand & MCI_OPEN_ALIAS)     spdlog::trace(" |- Open: ALIAS");
        if (fdwCommand & MCI_OPEN_SHAREABLE) spdlog::trace(" |- Open: SHAREABLE");

        if (p->lpstrDeviceType)
        {
            if (fdwCommand & MCI_OPEN_TYPE_ID || IsTypeId(p->lpstrDeviceType))
                spdlog::trace(" |- DeviceTypeId: {}", DeviceTypeId(p->lpstrDeviceType));
            else
                spdlog::trace(" |- DeviceType: {}", p->lpstrDeviceType);
        }

        if (p->lpstrElementName)
            spdlog::trace(" |- ElementName: {}", p->lpstrElementName);
        if (p->lpstrAlias)
            spdlog::trace(" |- Alias: {}", p->lpstrAlias);

        if (fdwCommand & MCI_NOTIFY)
            LogMciNotifyCallback((UINT_PTR)p->dwCallback);

        if (fdwCommand & MCI_WAIT)
            spdlog::trace(" |- MCI_WAIT");
        break;
    }

    case MCI_PLAY: {
        spdlog::debug("mciSendCommandA: MCI_PLAY (Device ID: {})", IDDevice); 
        auto p = reinterpret_cast<LPMCI_PLAY_PARMS>(dwParam);
        if (!p) break;

        bool fFrom = (fdwCommand & MCI_FROM) != 0;
        bool fTo = (fdwCommand & MCI_TO) != 0;
        bool fNotify = (fdwCommand & MCI_NOTIFY) != 0;
        bool fWait = (fdwCommand & MCI_WAIT) != 0;

        if (fFrom)
        {
            spdlog::trace(" |- From raw: 0x{:X}", (DWORD)p->dwFrom);
            LogMCITimeValue(GetMCITimeFormat(IDDevice), (DWORD)p->dwFrom, "From");
        }
        if (fTo)
        {
            spdlog::trace(" |- To raw: 0x{:X}", (DWORD)p->dwTo);
            LogMCITimeValue(GetMCITimeFormat(IDDevice), (DWORD)p->dwTo, "To");
        }

        if (fNotify) { LogMciNotifyCallback((UINT_PTR)p->dwCallback); }
        if (fWait) { spdlog::trace(" |- MCI_WAIT"); }

        break;
    }

    case MCI_SEEK: {
        spdlog::debug("mciSendCommandA: MCI_SEEK (Device ID: {})", IDDevice);
        auto p = reinterpret_cast<LPMCI_SEEK_PARMS>(dwParam);
        if (!p) { break; }

        if (fdwCommand & MCI_TO) { LogMCITimeValue(GetMCITimeFormat(IDDevice), (DWORD)p->dwTo, "Seek To"); }
        if (fdwCommand & MCI_SEEK_TO_START) { spdlog::trace(" |- MCI_SEEK_TO_START"); }
        if (fdwCommand & MCI_SEEK_TO_END) { spdlog::trace(" |- MCI_SEEK_TO_END"); }
        if (fdwCommand & MCI_WAIT) { spdlog::trace(" |- MCI_WAIT"); }
        
        break;
    }

    case MCI_STATUS: {
        spdlog::debug("mciSendCommandA: MCI_STATUS (Device ID: {})", IDDevice);;
        auto p = reinterpret_cast<LPMCI_STATUS_PARMS>(dwParam);
        if (!p) { break; }

        bool hasTrack = (fdwCommand & MCI_TRACK) != 0;
        bool hasItem = (fdwCommand & MCI_STATUS_ITEM) != 0;
        bool useWait = (fdwCommand & MCI_WAIT) != 0;

        if (hasTrack) { spdlog::trace(" |- Track: {}", p->dwTrack); }
        if (hasItem) { spdlog::trace(" |- MCI_STATUS_ITEM"); }
        if (fdwCommand & MCI_STATUS_START) { spdlog::trace(" |- MCI_STATUS_START"); }
        if (useWait) { spdlog::trace(" |- MCI_WAIT"); }

        DWORD item = hasItem ? p->dwItem : 0;
        const char* name = StatusItemName(item);
        if (hasTrack) { spdlog::trace(" |- Item: {} (0x{:X}) (Track={})", name, item, p->dwTrack); }
        else { spdlog::trace(" |- Item: {} (0x{:X})", name, item); }
        break;
    }

    case MCI_GETDEVCAPS: {
        spdlog::debug("mciSendCommandA: MCI_GETDEVCAPS (Device ID: {})", IDDevice);
        auto p = reinterpret_cast<LPMCI_GETDEVCAPS_PARMS>(dwParam);
        if (p) {
            const char* name = GetDevCapsItemName(p->dwItem);
            if (name) { spdlog::trace(" |- dwItem: {} [0x{:X}]", name, p->dwItem); }
            else { spdlog::trace(" |- dwItem: 0x{:X}", p->dwItem); }
            spdlog::trace(" |- dwReturn: 0x{:X}", p->dwReturn);
            break;
        }
        break;
    }

    case MCI_SYSINFO: {
        spdlog::debug("mciSendCommandA: MCI_SYSINFO (Device ID: {})", IDDevice);
        auto p = reinterpret_cast<LPMCI_SYSINFO_PARMSA>(dwParam);
        if (p) {
            DWORD low = (DWORD)(fdwCommand & 0xFFFF);
            if (low & 0x0100) spdlog::trace(" |- SYSINFO: QUANTITY");
            if (low & 0x0200) spdlog::trace(" |- SYSINFO: OPEN");
            if (low & 0x0400) spdlog::trace(" |- SYSINFO: NAME");
            if (low & 0x0800) spdlog::trace(" |- SYSINFO: INSTALLNAME");

            spdlog::trace(" |- dwNumber: {}", p->dwNumber);
            spdlog::trace(" |- wDeviceType: 0x{:X}", p->wDeviceType);
            if (p->lpstrReturn) spdlog::trace(" |- Return String: {}", p->lpstrReturn);
            spdlog::trace(" |- Buffer Size: {}", p->dwRetSize);
        }
        break;
    }

    case MCI_INFO: {
        spdlog::debug("mciSendCommandA: MCI_INFO (Device ID: {})", IDDevice);
        auto p = reinterpret_cast<LPMCI_INFO_PARMSA>(dwParam);
        if (p) {
            if (fdwCommand & MCI_INFO_PRODUCT) spdlog::trace(" |- MCI_INFO_PRODUCT");
            if (fdwCommand & MCI_INFO_FILE) spdlog::trace(" |- MCI_INFO_FILE");
            if (fdwCommand & MCI_INFO_MEDIA_UPC) spdlog::trace(" |- MCI_INFO_MEDIA_UPC");
            if (fdwCommand & MCI_INFO_MEDIA_IDENTITY) spdlog::trace(" |- MCI_INFO_MEDIA_IDENTITY");
            if (fdwCommand & MCI_INFO_NAME) spdlog::trace(" |- MCI_INFO_NAME");
            if (fdwCommand & MCI_INFO_COPYRIGHT) spdlog::trace(" |- MCI_INFO_COPYRIGHT");
            if (p->lpstrReturn) spdlog::trace(" |- Return String: {}", p->lpstrReturn);
        }
        break;
    }

    case MCI_STOP: {
        spdlog::debug("mciSendCommandA: MCI_STOP (Device ID: {})", IDDevice);;
        if (fdwCommand & MCI_WAIT) { spdlog::trace(" |- MCI_WAIT"); }
        break;
    }
    case MCI_SET:       spdlog::debug("mciSendCommandA: MCI_SET (Device ID: {})", IDDevice); break;
    case MCI_PAUSE:     spdlog::debug("mciSendCommandA: MCI_PAUSE (Device ID: {})", IDDevice); break;
    case MCI_CLOSE:     spdlog::debug("mciSendCommandA: MCI_CLOSE (Device ID: {})", IDDevice); break;
    case MCI_ESCAPE:    spdlog::debug("mciSendCommandA: MCI_ESCAPE (Device ID: {})", IDDevice); break;
    case MCI_SPIN:      spdlog::debug("mciSendCommandA: MCI_SPIN (Device ID : {})", IDDevice); break;
    case MCI_STEP:      spdlog::debug("mciSendCommandA: MCI_STEP (Device ID: {})", IDDevice); break;
    case MCI_RECORD:    spdlog::debug("mciSendCommandA: MCI_RECORD (Device ID: {})", IDDevice); break;
    case MCI_BREAK:     spdlog::debug("mciSendCommandA: MCI_BREAK (Device ID: {})", IDDevice); break;
    case MCI_SAVE:      spdlog::debug("mciSendCommandA: MCI_SAVE (Device ID: {})", IDDevice); break;
    case MCI_CUE:       spdlog::debug("mciSendCommandA: MCI_CUE (Device ID: {})", IDDevice); break;
    case MCI_REALIZE:   spdlog::debug("mciSendCommandA: MCI_REALIZE (Device ID: {})", IDDevice); break;
    case MCI_WINDOW:    spdlog::debug("mciSendCommandA: MCI_WINDOW (Device ID: {})", IDDevice); break;
    case MCI_PUT:       spdlog::debug("mciSendCommandA: MCI_PUT (Device ID: {})", IDDevice); break;
    case MCI_WHERE:     spdlog::debug("mciSendCommandA: MCI_WHERE (Device ID: {})", IDDevice); break;
    case MCI_FREEZE:    spdlog::debug("mciSendCommandA: MCI_FREEZE (Device ID: {})", IDDevice); break;
    case MCI_UNFREEZE:  spdlog::debug("mciSendCommandA: MCI_UNFREEZE (Device ID: {})", IDDevice); break;
    case MCI_LOAD:      spdlog::debug("mciSendCommandA: MCI_LOAD (Device ID: {})", IDDevice); break;
    case MCI_COPY:      spdlog::debug("mciSendCommandA: MCI_COPY (Device ID: {})", IDDevice); break;
    case MCI_CUT:       spdlog::debug("mciSendCommandA: MCI_CUT (Device ID: {})", IDDevice); break;
    case MCI_DELETE:    spdlog::debug("mciSendCommandA: MCI_DELETE (Device ID: {})", IDDevice); break;
    case MCI_PASTE:     spdlog::debug("mciSendCommandA: MCI_PASTE (Device ID: {})", IDDevice); break;
    default: {
        spdlog::debug("mciSendCommandA: (0x{:X}) (Device ID: {})", uMsg, IDDevice);
        if (dwParam) {
            const DWORD* raw = reinterpret_cast<const DWORD*>(dwParam);
            __try {
                spdlog::trace(" |- Raw param[0]: 0x{:08X}", raw[0]);
                spdlog::trace(" |- Raw param[1]: 0x{:08X}", raw[1]);
                spdlog::trace(" |- Raw param[2]: 0x{:08X}", raw[2]);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) { spdlog::trace(" |- Raw param: <unreadable>"); }
        }
        break;
    }
    }
}

void LogMCIResult(MCIERROR err) {
    if (err == MMSYSERR_NOERROR) { return; }
    char buf[256] = {};
    if (mciGetErrorStringA(err, buf, sizeof(buf))) { spdlog::warn("mciSendCommandA: MCIERROR = 0x{:X} ({})", err, buf); }
    else { spdlog::warn("mciSendCommandA: MCIERROR = 0x{:X} (Unknown error)", err); }
}

UINT GetMCITimeFormat(MCIDEVICEID deviceId) {
    MCI_GETDEVCAPS_PARMS caps = {};
    caps.dwItem = MCI_GETDEVCAPS_TIME_FORMAT;

    MCIERROR err = mciSendCommandA(deviceId, MCI_GETDEVCAPS, MCI_GETDEVCAPS_ITEM, (DWORD_PTR)&caps);
    if (err != 0) {
        char buf[256] = {};
        mciGetErrorStringA(err, buf, sizeof(buf));
        //spdlog::warn("GetMCITimeFormat: failed (0x{:X}): {}", err, buf);
        return MCI_FORMAT_MILLISECONDS;
    }
    return static_cast<UINT>(caps.dwReturn);
}

void LogMCITimeValue(UINT timeFormat, DWORD value, const char* label) {
    const char* fmtName = TimeFormatName(timeFormat);

    spdlog::trace(" |- {} raw: 0x{:08X} ({})", label, value, fmtName);

    switch (timeFormat) {
    case MCI_FORMAT_MILLISECONDS: {
        spdlog::trace("   |- {}: {} ms ({:.3f} s)", label, value, value / 1000.0);
        break;
    }
    case MCI_FORMAT_TMSF: {
        BYTE track = MCI_TMSF_TRACK(value);
        BYTE minute = MCI_TMSF_MINUTE(value);
        BYTE second = MCI_TMSF_SECOND(value);
        BYTE frame = MCI_TMSF_FRAME(value);
        spdlog::trace("   |- {} (TMSF): Track {}  {:02}:{:02}.{:02}", label, track, minute, second, frame);
        break;
    }

    case MCI_FORMAT_MSF: {
        BYTE minute = MCI_MSF_MINUTE(value);
        BYTE second = MCI_MSF_SECOND(value);
        BYTE frame = MCI_MSF_FRAME(value);
        spdlog::trace("   |- {} (MSF): {:02}:{:02}.{:02}", label, minute, second, frame);
        break;
    }

    default: {
        spdlog::trace("   |- {}: Unhandled time format {}", label, fmtName);
        break;
    }
    }
}