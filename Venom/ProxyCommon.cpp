// SPDX-License-Identifier: GPL-3.0-or-later

#include "ProxyCommon.h"
#include <ddraw.h>
#include <Windows.h>
#include <algorithm>
#include <set>
#include <tuple>

constexpr GUID IID_IProxyUnwrap = { 0x00C0FFEE, 0xC0DE, 0xDEAD, { 0xD1, 0xAB, 0x01, 0x1C, 0xAL, 0x00, 0x00, 0x00 } };

DWORD SafeBpp(const DDSURFACEDESC &d) { return (d.dwFlags & DDSD_PIXELFORMAT) ? d.ddpfPixelFormat.dwRGBBitCount : 0; }
DWORD SafeBpp(const DDSURFACEDESC2 &d) { return (d.dwFlags & DDSD_PIXELFORMAT) ? d.ddpfPixelFormat.dwRGBBitCount : 0; }
DWORD SafeRefresh(const DDSURFACEDESC &d) { return (d.dwFlags & DDSD_REFRESHRATE) ? d.dwRefreshRate : 0; }
DWORD SafeRefresh(const DDSURFACEDESC2 &d) { return (d.dwFlags & DDSD_REFRESHRATE) ? d.dwRefreshRate : 0; }
DWORD SafePixelFlags(const DDSURFACEDESC &d) { return (d.dwFlags & DDSD_PIXELFORMAT) ? d.ddpfPixelFormat.dwFlags : 0; }
DWORD SafePixelFlags(const DDSURFACEDESC2 &d) { return (d.dwFlags & DDSD_PIXELFORMAT) ? d.ddpfPixelFormat.dwFlags : 0; }

bool ModeKey::operator<(const ModeKey &other) const noexcept { return std::tie(width, height, bitDepth, refreshRate) < std::tie(other.width, other.height, other.bitDepth, other.refreshRate); }
// Future improvement: Allow this whitelist to be customised via Venom.cfg, only use this as a default if not defined in Venom.cfg
const std::set<ModeKey> &GetModeWhitelist() {
    static std::set<ModeKey> whitelist = [] {
        std::set<ModeKey> wl;
		wl.insert({ 640,  480, 0, 0 });    // 4:3 VGA / 480p
		wl.insert({ 800,  600, 0, 0 });    // 4:3 SVGA
		wl.insert({ 1024, 768, 0, 0 });    // 4:3 XGA
		
		wl.insert({ 1280, 720, 0, 0 });    // 16:9 HD / 720p
		wl.insert({ 1600, 900, 0, 0 });    // 16:9 HD+
		wl.insert({ 1920, 1080, 0, 0 });   // 16:9 FHD / 1080p
		wl.insert({ 2560, 1440, 0, 0 });   // 16:9 QHD / 1440p
		wl.insert({ 3840, 2160, 0, 0 });   // 16:9 UHD / 4K

		wl.insert({ 1280, 800, 0, 0 });    // 16:10 WXGA
		wl.insert({ 1440, 900, 0, 0 });    // 16:10 WXGA+
		wl.insert({ 1680, 1050, 0, 0 });   // 16:10 WSXGA+
		wl.insert({ 1920, 1200, 0, 0 });   // 16:10 WUXGA

		wl.insert({ 2560, 1080, 0, 0 });   // 21:9 Ultrawide FHD
		wl.insert({ 3440, 1440, 0, 0 });   // 21:9 Ultrawide QHD
		wl.insert({ 5120, 2160, 0, 0 });   // 21:9 Ultrawide UHD / 4K
		
        DWORD screenW = GetSystemMetrics(SM_CXSCREEN);
        DWORD screenH = GetSystemMetrics(SM_CYSCREEN);

        auto iterator = std::find_if(wl.begin(), wl.end(), [screenW, screenH](const ModeKey& k) { return k.width == screenW && k.height == screenH; });
        if (iterator == wl.end()) { wl.insert({ screenW, screenH, 0, 0 }); }

        return wl;
        }();
    return whitelist;
}

bool IsWhitelisted(DWORD width, DWORD height, DWORD bpp, DWORD refresh) {
	const auto &wl = GetModeWhitelist();
	for (auto &entry : wl) {
		if (entry.width == width && entry.height == height && (entry.bitDepth == 0 || entry.bitDepth == bpp) && (entry.refreshRate == 0 || entry.refreshRate == refresh)) { return true; }
	}
	return false;
}

HRESULT __stdcall FilteredEnumCallback(LPDDSURFACEDESC mode, LPVOID ctx) {
	EnumCtx* w = static_cast<EnumCtx*>(ctx);
	if (IsWhitelisted(mode->dwWidth, mode->dwHeight, SafeBpp(*mode), SafeRefresh(*mode))) { return w->userCb(mode, w->userCtx); }
	return DDENUMRET_OK;
}