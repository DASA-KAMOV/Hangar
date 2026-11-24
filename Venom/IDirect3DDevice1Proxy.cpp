// SPDX-License-Identifier: GPL-3.0-or-later

#include "ProxyCommon.h"
#include "IDirect3DDevice1Proxy.h"
#include "IDirectDraw1Proxy.h"
#include "IDirectDraw2Proxy.h"
#include "IDirectDrawSurface1Proxy.h"
#include "IDirect3D1Proxy.h"
#include <d3d.h>
#include <d3dtypes.h>
#include <Windows.h>
#include <spdlog/spdlog.h>
#include "logging.h"

IDirect3DDevice1Proxy::IDirect3DDevice1Proxy(IDirect3DDevice* inner) : m_inner(inner), m_refCount(1) {
	//spdlog::debug("IDirect3DDevice1Proxy created");
}

IDirect3DDevice1Proxy::~IDirect3DDevice1Proxy() {} // Intentionally empty. m_inner->Release() is handled by Release().

HRESULT __stdcall IDirect3DDevice1Proxy::QueryInterface(REFIID riid, void** ppvObj) {
	//spdlog::trace("IDirect3DDevice1Proxy::QueryInterface called with riid: {}", REFIIDToString(riid));
	if (!ppvObj) { return E_POINTER; }
	
	if (riid == IID_IUnknown || riid == IID_IDirect3DDevice || riid == IID_IDirect3DHALDevice) {
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
	else if (riid == IID_IDirectDraw2) {
		*ppvObj = new IDirectDraw2Proxy(static_cast<IDirectDraw2*>(rawInner));
		return S_OK;
	}
	else if (riid == IID_IDirectDrawSurface) {
		*ppvObj = new IDirectDrawSurface1Proxy(static_cast<IDirectDrawSurface*>(rawInner));
		return S_OK;
	}

	//spdlog::warn("IDirect3DDevice1Proxy: Un-Proxied interface requested: {}", REFIIDToString(riid));
	*ppvObj = rawInner;
	return S_OK;
}

ULONG __stdcall IDirect3DDevice1Proxy::AddRef() { return (ULONG)InterlockedIncrement(reinterpret_cast<LONG*>(&m_refCount)); }

ULONG __stdcall IDirect3DDevice1Proxy::Release() {
	ULONG count = (ULONG)InterlockedDecrement(reinterpret_cast<LONG*>(&m_refCount));
	if (count == 0) {
		if (m_inner) {
			m_inner->Release();
			m_inner = nullptr;
		}
		//spdlog::trace("IDirect3DDevice1Proxy destroyed");
		delete this;
	}
	return count;
}

HRESULT __stdcall IDirect3DDevice1Proxy::Initialize(LPDIRECT3D lpDirect3D, LPGUID lpGUID, LPD3DDEVICEDESC lpD3DHWDevDesc) { return m_inner->Initialize(lpDirect3D, lpGUID, lpD3DHWDevDesc); }

HRESULT __stdcall IDirect3DDevice1Proxy::GetCaps(LPD3DDEVICEDESC lpD3DHWDevDesc, LPD3DDEVICEDESC lpD3DHELDevDesc) {
	HRESULT hr = m_inner->GetCaps(lpD3DHWDevDesc, lpD3DHELDevDesc);
	if (SUCCEEDED(hr)) {
		if (lpD3DHWDevDesc) { // Hardware renderer caps
			lpD3DHWDevDesc->dwMaxTextureWidth = MAX_TEXTURE_WIDTH;
			lpD3DHWDevDesc->dwMaxTextureHeight = MAX_TEXTURE_HEIGHT;
			lpD3DHWDevDesc->dwMaxTextureAspectRatio = MAX_TEXTURE_RATIO;
		}

		if (lpD3DHELDevDesc) { // Software renderer caps
			lpD3DHELDevDesc->dwMaxTextureWidth = MAX_TEXTURE_WIDTH;
			lpD3DHELDevDesc->dwMaxTextureHeight = MAX_TEXTURE_HEIGHT;
			lpD3DHELDevDesc->dwMaxTextureAspectRatio = MAX_TEXTURE_RATIO;
		}
	}
	return hr;
}

HRESULT __stdcall IDirect3DDevice1Proxy::SwapTextureHandles(LPDIRECT3DTEXTURE lpD3DTex1, LPDIRECT3DTEXTURE lpD3DTex2) { return m_inner->SwapTextureHandles(lpD3DTex1, lpD3DTex2); }
HRESULT __stdcall IDirect3DDevice1Proxy::CreateExecuteBuffer(LPD3DEXECUTEBUFFERDESC lpDesc, LPDIRECT3DEXECUTEBUFFER* lplpDirect3DExecuteBuffer, IUnknown* pUnkOuter) { return m_inner->CreateExecuteBuffer(lpDesc, lplpDirect3DExecuteBuffer, pUnkOuter); }
HRESULT __stdcall IDirect3DDevice1Proxy::GetStats(LPD3DSTATS lpD3DStats) { return m_inner->GetStats(lpD3DStats); }
HRESULT __stdcall IDirect3DDevice1Proxy::Execute(LPDIRECT3DEXECUTEBUFFER lpExecuteBuffer, LPDIRECT3DVIEWPORT lpViewport, DWORD dwFlags) { return m_inner->Execute(lpExecuteBuffer, lpViewport, dwFlags); }
HRESULT __stdcall IDirect3DDevice1Proxy::AddViewport(LPDIRECT3DVIEWPORT lpDirect3DViewport) { return m_inner->AddViewport(lpDirect3DViewport); }
HRESULT __stdcall IDirect3DDevice1Proxy::DeleteViewport(LPDIRECT3DVIEWPORT lpDirect3DViewport) { return m_inner->DeleteViewport(lpDirect3DViewport); }
HRESULT __stdcall IDirect3DDevice1Proxy::NextViewport(LPDIRECT3DVIEWPORT lpDirect3DViewport, LPDIRECT3DVIEWPORT* lplpDirect3DViewport, DWORD dwFlags) { return m_inner->NextViewport(lpDirect3DViewport, lplpDirect3DViewport, dwFlags); }
HRESULT __stdcall IDirect3DDevice1Proxy::Pick(LPDIRECT3DEXECUTEBUFFER lpDirect3DExecuteBuffer, LPDIRECT3DVIEWPORT lpDirect3DViewport, DWORD dwFlags, LPD3DRECT lpRect) { return m_inner->Pick(lpDirect3DExecuteBuffer, lpDirect3DViewport, dwFlags, lpRect); }
HRESULT __stdcall IDirect3DDevice1Proxy::GetPickRecords(LPDWORD lpCount, LPD3DPICKRECORD lpD3DPickRec) { return m_inner->GetPickRecords(lpCount, lpD3DPickRec); }
HRESULT __stdcall IDirect3DDevice1Proxy::EnumTextureFormats(LPD3DENUMTEXTUREFORMATSCALLBACK lpD3DEnumTextureProc, LPVOID lpArg) { return m_inner->EnumTextureFormats(lpD3DEnumTextureProc, lpArg); }
HRESULT __stdcall IDirect3DDevice1Proxy::CreateMatrix(LPD3DMATRIXHANDLE lpD3DMatHandle) { return m_inner->CreateMatrix(lpD3DMatHandle); }
HRESULT __stdcall IDirect3DDevice1Proxy::SetMatrix(D3DMATRIXHANDLE d3dMatHandle, const LPD3DMATRIX lpD3DMatrix) { return m_inner->SetMatrix(d3dMatHandle, lpD3DMatrix); }
HRESULT __stdcall IDirect3DDevice1Proxy::GetMatrix(D3DMATRIXHANDLE d3dMatHandle, LPD3DMATRIX lpD3DMatrix) { return m_inner->GetMatrix(d3dMatHandle, lpD3DMatrix); }
HRESULT __stdcall IDirect3DDevice1Proxy::DeleteMatrix(D3DMATRIXHANDLE d3dMatHandle) { return m_inner->DeleteMatrix(d3dMatHandle); }
HRESULT __stdcall IDirect3DDevice1Proxy::BeginScene() { return m_inner->BeginScene(); }
HRESULT __stdcall IDirect3DDevice1Proxy::EndScene() { return m_inner->EndScene(); }

HRESULT __stdcall IDirect3DDevice1Proxy::GetDirect3D(LPDIRECT3D* lplpDirect3D) {
	HRESULT hr = m_inner->GetDirect3D(lplpDirect3D);
	if (SUCCEEDED(hr) && lplpDirect3D && *lplpDirect3D) { *lplpDirect3D = new IDirect3D1Proxy(*lplpDirect3D); }
	return hr;
}