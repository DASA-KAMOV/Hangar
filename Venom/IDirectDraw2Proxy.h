// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#define WIN32_LEAN_AND_MEAN
#include <ddraw.h>
#include <atomic>
#include <Windows.h>

class IDirectDraw2Proxy : public IDirectDraw2 {
private:
	IDirectDraw2* m_inner = nullptr;
	std::atomic<ULONG> m_refCount = 0;

public:
	explicit IDirectDraw2Proxy(IDirectDraw2* inner);
	virtual ~IDirectDraw2Proxy();

	// IUnknown
	HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObj) override;
	ULONG   __stdcall AddRef() override;
	ULONG   __stdcall Release() override;

	// IDirectDraw
	HRESULT __stdcall Compact() override;
	HRESULT __stdcall CreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER* lplpDDClipper, IUnknown* pUnkOuter) override;
	HRESULT __stdcall CreatePalette(DWORD dwFlags, LPPALETTEENTRY lpColorTable, LPDIRECTDRAWPALETTE* lplpDDPalette, IUnknown* pUnkOuter) override;
	HRESULT __stdcall CreateSurface(LPDDSURFACEDESC lpDDSurfaceDesc, LPDIRECTDRAWSURFACE* lplpDDSurface, IUnknown* pUnkOuter) override;
	HRESULT __stdcall DuplicateSurface(LPDIRECTDRAWSURFACE lpDDSurface, LPDIRECTDRAWSURFACE* lplpDupDDSurface) override;
	HRESULT __stdcall EnumDisplayModes(DWORD dwFlags, LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext, LPDDENUMMODESCALLBACK lpEnumModesCallback) override;
	HRESULT __stdcall EnumSurfaces(DWORD dwFlags, LPDDSURFACEDESC lpDDSD, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback) override;
	HRESULT __stdcall FlipToGDISurface() override;
	HRESULT __stdcall GetCaps(LPDDCAPS lpDDDriverCaps, LPDDCAPS lpHElCaps) override;
	HRESULT __stdcall GetDisplayMode(LPDDSURFACEDESC lpDDSurfaceDesc) override;
	HRESULT __stdcall GetFourCCCodes(LPDWORD lpNumCodes, LPDWORD lpCodes) override;
	HRESULT __stdcall GetGDISurface(LPDIRECTDRAWSURFACE* lplpGDIDDSurface) override;
	HRESULT __stdcall GetMonitorFrequency(LPDWORD lpdwFrequency) override;
	HRESULT __stdcall GetScanLine(LPDWORD lpdwScanLine) override;
	HRESULT __stdcall GetVerticalBlankStatus(LPBOOL lpbInVerticalBlank) override;
	HRESULT __stdcall Initialize(GUID* lpGUID) override;
	HRESULT __stdcall RestoreDisplayMode() override;
	HRESULT __stdcall SetCooperativeLevel(HWND hWnd, DWORD dwFlags) override;
	HRESULT __stdcall SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP, DWORD dwRefreshRate, DWORD dwFlags) override;
	HRESULT __stdcall WaitForVerticalBlank(DWORD dwFlags, HANDLE hEvent) override;

	// IDirectDraw2
	HRESULT __stdcall GetAvailableVidMem(LPDDSCAPS lpDDCaps, LPDWORD lpdwTotal, LPDWORD lpdwFree) override;
};