// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#define WIN32_LEAN_AND_MEAN
#include <ddraw.h>
#include <set>
#include <Windows.h>
#include <unordered_map>
#include <mutex>

extern const GUID IID_IProxyUnwrap;

template <typename T>
T* UnwrapProxy(T* ptr) {
	if (!ptr) return nullptr;
	void* unwrapped = nullptr;
	if (SUCCEEDED(ptr->QueryInterface(IID_IProxyUnwrap, &unwrapped))) {
		return static_cast<T*>(unwrapped);
	}
	return ptr;
}

struct ModeKey
{
	DWORD width;
	DWORD height;
	DWORD bitDepth;    // 0 = any
	DWORD refreshRate; // 0 = any
	bool operator<(const ModeKey &other) const noexcept;
};

struct EnumCtx {
	LPDDENUMMODESCALLBACK userCb;
	LPVOID userCtx;
};

const std::set<ModeKey> &GetModeWhitelist();
bool IsWhitelisted(DWORD width, DWORD height, DWORD bpp, DWORD refresh);

HRESULT __stdcall FilteredEnumCallback(LPDDSURFACEDESC mode, LPVOID ctx);

DWORD SafeBpp(const DDSURFACEDESC &d);
DWORD SafeBpp(const DDSURFACEDESC2 &d);
DWORD SafeRefresh(const DDSURFACEDESC  &d);
DWORD SafeRefresh(const DDSURFACEDESC2 &d);
DWORD SafePixelFlags(const DDSURFACEDESC &d);
DWORD SafePixelFlags(const DDSURFACEDESC2 &d);

template <typename InnerT>
HRESULT FilteredEnumDisplayModes(InnerT* inner, DWORD dwFlags, LPDDSURFACEDESC lpDDSD, LPVOID lpContext, LPDDENUMMODESCALLBACK lpEnumCallback) {
	EnumCtx ctx{ lpEnumCallback, lpContext };
	return inner->EnumDisplayModes(dwFlags, lpDDSD, &ctx, FilteredEnumCallback);
}
