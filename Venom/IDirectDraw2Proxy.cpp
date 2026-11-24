// SPDX-License-Identifier: GPL-3.0-or-later

#include "ProxyCommon.h"
#include "IDirectDraw1Proxy.h"
#include "IDirectDraw2Proxy.h"
#include "IDirectDrawSurface1Proxy.h"
#include "IDirect3D1Proxy.h"
#include "IDirect3DDevice1Proxy.h"
#include <d3d.h>
#include <ddraw.h>
#include <Windows.h>
#include <spdlog/spdlog.h>
#include "logging.h"

IDirectDraw2Proxy::IDirectDraw2Proxy(IDirectDraw2* inner) : m_inner(inner), m_refCount(1) {
	//spdlog::trace("IDirectDraw2ProxyProxy created");
}

IDirectDraw2Proxy::~IDirectDraw2Proxy() {} // Intentionally empty. m_inner->Release() is handled by Release().

// IUnknown
HRESULT __stdcall IDirectDraw2Proxy::QueryInterface(REFIID riid, void** ppvObj) {
	//spdlog::trace("IDirectDraw2Proxy::QueryInterface called with riid: {}", REFIIDToString(riid));
	if (!ppvObj) { return E_POINTER; }

	if (riid == IID_IUnknown || riid == IID_IDirectDraw2) {
		*ppvObj = this;
		this->AddRef();
		return S_OK;
	}
	else if (riid == IID_IProxyUnwrap) {
		*ppvObj = m_inner;
		return S_OK;
	}

	void* rawInner = nullptr;
	HRESULT hr = m_inner->QueryInterface(riid, &rawInner);
	if (FAILED(hr)) {
		*ppvObj = nullptr;
		return hr;
	}

	if (riid == IID_IDirectDraw) {
		*ppvObj = new IDirectDraw1Proxy(static_cast<IDirectDraw*>(rawInner));
		return S_OK;
	}
	else if (riid == IID_IDirectDrawSurface) {
		*ppvObj = new IDirectDrawSurface1Proxy(static_cast<IDirectDrawSurface*>(rawInner));
		return S_OK;
	}
	else if (riid == IID_IDirect3D) {
		*ppvObj = new IDirect3D1Proxy(static_cast<IDirect3D*>(rawInner));
		return S_OK;
	}
	/*
	else if (riid == IID_IDirect3DHALDevice) {
		*ppvObj = new IDirect3DDevice1Proxy(static_cast<IDirect3DDevice*>(rawInner));
		return S_OK;
	}
	*/
	//spdlog::warn("IDirectDraw2Proxy: Un-Proxied interface requested: {}", REFIIDToString(riid));
	*ppvObj = rawInner;
	return S_OK;
}

ULONG __stdcall IDirectDraw2Proxy::AddRef() { return (ULONG)InterlockedIncrement(reinterpret_cast<LONG*>(&m_refCount)); }

ULONG __stdcall IDirectDraw2Proxy::Release() {
	ULONG count = (ULONG)InterlockedDecrement(reinterpret_cast<LONG*>(&m_refCount));
	if (count == 0) {
		if (m_inner) {
			m_inner->Release();
			m_inner = nullptr;
		}
		//spdlog::trace("IDirectDraw2Proxy destroyed");
		delete this;
	}
	return count;
}

// IDirectDraw2
HRESULT __stdcall IDirectDraw2Proxy::Compact() { return m_inner ? m_inner->Compact() : DDERR_GENERIC; }
HRESULT __stdcall IDirectDraw2Proxy::CreateClipper(DWORD dwFlags, LPDIRECTDRAWCLIPPER* lplpDDClipper, IUnknown* pUnkOuter) { return m_inner ? m_inner->CreateClipper(dwFlags, lplpDDClipper, pUnkOuter) : DDERR_GENERIC; }
HRESULT __stdcall IDirectDraw2Proxy::CreatePalette(DWORD dwFlags, LPPALETTEENTRY lpColorTable, LPDIRECTDRAWPALETTE* lplpDDPalette, IUnknown* pUnkOuter) { return m_inner ? m_inner->CreatePalette(dwFlags, lpColorTable, lplpDDPalette, pUnkOuter) : DDERR_GENERIC; }

HRESULT __stdcall IDirectDraw2Proxy::CreateSurface(LPDDSURFACEDESC lpDDSurfaceDesc, LPDIRECTDRAWSURFACE* lplpDDSurface, IUnknown* pUnkOuter) {
	if (!m_inner) { return DDERR_GENERIC; }
	if (!lplpDDSurface) { return E_POINTER; }

	HRESULT hr = m_inner->CreateSurface(lpDDSurfaceDesc, lplpDDSurface, UnwrapProxy(pUnkOuter));
	if (SUCCEEDED(hr) && lplpDDSurface && *lplpDDSurface) {
		*lplpDDSurface = new IDirectDrawSurface1Proxy(*lplpDDSurface);
	}
	return hr;
}

HRESULT __stdcall IDirectDraw2Proxy::DuplicateSurface(LPDIRECTDRAWSURFACE lpDDSurface, LPDIRECTDRAWSURFACE* lplpDupDDSurface) {
	if (!m_inner) { return DDERR_GENERIC; }
	if (!lplpDupDDSurface) { return E_POINTER; }

	IDirectDrawSurface1Proxy* proxySurface = static_cast<IDirectDrawSurface1Proxy*>(lpDDSurface);
	LPDIRECTDRAWSURFACE realInputSurface = proxySurface ? proxySurface->GetInner() : lpDDSurface;

	LPDIRECTDRAWSURFACE realDupSurface = nullptr;
	HRESULT hr = m_inner->DuplicateSurface(realInputSurface, &realDupSurface);
	if (SUCCEEDED(hr) && realDupSurface != nullptr) { *lplpDupDDSurface = new IDirectDrawSurface1Proxy(realDupSurface); }
	else { *lplpDupDDSurface = nullptr; }

	return hr;
}

HRESULT __stdcall IDirectDraw2Proxy::EnumDisplayModes(DWORD dwFlags, LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext, LPDDENUMMODESCALLBACK lpEnumModesCallback) {
	return FilteredEnumDisplayModes(m_inner, dwFlags, lpDDSurfaceDesc, lpContext, lpEnumModesCallback);
}

HRESULT __stdcall IDirectDraw2Proxy::EnumSurfaces(DWORD dwFlags, LPDDSURFACEDESC lpDDSD, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback) { return m_inner ? m_inner->EnumSurfaces(dwFlags, lpDDSD, lpContext, lpEnumSurfacesCallback) : DDERR_GENERIC; }
HRESULT __stdcall IDirectDraw2Proxy::FlipToGDISurface() { return m_inner ? m_inner->FlipToGDISurface() : DDERR_GENERIC; }
HRESULT __stdcall IDirectDraw2Proxy::GetCaps(LPDDCAPS lpDDDriverCaps, LPDDCAPS lpHElCaps) { return m_inner ? m_inner->GetCaps(lpDDDriverCaps, lpHElCaps) : DDERR_GENERIC; }
HRESULT __stdcall IDirectDraw2Proxy::GetDisplayMode(LPDDSURFACEDESC lpDDSurfaceDesc) { return m_inner ? m_inner->GetDisplayMode(lpDDSurfaceDesc) : DDERR_GENERIC; }
HRESULT __stdcall IDirectDraw2Proxy::GetFourCCCodes(LPDWORD lpNumCodes, LPDWORD lpCodes) { return m_inner ? m_inner->GetFourCCCodes(lpNumCodes, lpCodes) : DDERR_GENERIC; }
HRESULT __stdcall IDirectDraw2Proxy::GetGDISurface(LPDIRECTDRAWSURFACE* lplpGDIDDSurface) { return m_inner ? m_inner->GetGDISurface(lplpGDIDDSurface) : DDERR_GENERIC; }
HRESULT __stdcall IDirectDraw2Proxy::GetMonitorFrequency(LPDWORD lpdwFrequency) { return m_inner ? m_inner->GetMonitorFrequency(lpdwFrequency) : DDERR_GENERIC; }
HRESULT __stdcall IDirectDraw2Proxy::GetScanLine(LPDWORD lpdwScanLine) { return m_inner ? m_inner->GetScanLine(lpdwScanLine) : DDERR_GENERIC; }
HRESULT __stdcall IDirectDraw2Proxy::GetVerticalBlankStatus(LPBOOL lpbInVerticalBlank) { return m_inner ? m_inner->GetVerticalBlankStatus(lpbInVerticalBlank) : DDERR_GENERIC; }
HRESULT __stdcall IDirectDraw2Proxy::Initialize(GUID* lpGUID) { return m_inner ? m_inner->Initialize(lpGUID) : DDERR_GENERIC; }
HRESULT __stdcall IDirectDraw2Proxy::RestoreDisplayMode() { return m_inner ? m_inner->RestoreDisplayMode() : DDERR_GENERIC; }
HRESULT __stdcall IDirectDraw2Proxy::SetCooperativeLevel(HWND hWnd, DWORD dwFlags) { return m_inner ? m_inner->SetCooperativeLevel(hWnd, dwFlags) : DDERR_GENERIC; }
HRESULT __stdcall IDirectDraw2Proxy::SetDisplayMode(DWORD dwWidth, DWORD dwHeight, DWORD dwBPP, DWORD dwRefreshRate, DWORD dwFlags) { return m_inner ? m_inner->SetDisplayMode(dwWidth, dwHeight, dwBPP, dwRefreshRate, dwFlags) : DDERR_GENERIC; }
HRESULT __stdcall IDirectDraw2Proxy::WaitForVerticalBlank(DWORD dwFlags, HANDLE hEvent) { return m_inner ? m_inner->WaitForVerticalBlank(dwFlags, hEvent) : DDERR_GENERIC; }
HRESULT __stdcall IDirectDraw2Proxy::GetAvailableVidMem(LPDDSCAPS lpDDSCaps, LPDWORD lpdwTotal, LPDWORD lpdwFree) { return m_inner ? m_inner->GetAvailableVidMem(lpDDSCaps, lpdwTotal, lpdwFree) : DDERR_GENERIC; }