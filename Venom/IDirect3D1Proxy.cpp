// SPDX-License-Identifier: GPL-3.0-or-later

#include "ProxyCommon.h"
#include "IDirect3D1Proxy.h"
#include "IDirect3DDevice1Proxy.h"
#include <d3d.h>
#include <Windows.h>
#include <spdlog/spdlog.h>
#include "logging.h"

IDirect3D1Proxy::IDirect3D1Proxy(IDirect3D* inner) : m_inner(inner), m_refCount(1) {
	//spdlog::trace("IDirect3D1Proxy created");
}

IDirect3D1Proxy::~IDirect3D1Proxy() {} // Intentionally empty. m_inner->Release() is handled by Release().

HRESULT __stdcall IDirect3D1Proxy::QueryInterface(REFIID riid, void** ppvObj) {
	//spdlog::trace("IDirect3D1Proxy::QueryInterface called with riid: {}", REFIIDToString(riid));
	if (!ppvObj) { return E_POINTER; }
	
	if (riid == IID_IUnknown || riid == IID_IDirect3D) {
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

	//spdlog::warn("IDirect3D1Proxy: Un-Proxied interface requested: {}", REFIIDToString(riid));
	*ppvObj = rawInner;
	return S_OK;
}

ULONG __stdcall IDirect3D1Proxy::AddRef() { return (ULONG)InterlockedIncrement(reinterpret_cast<LONG*>(&m_refCount)); }

ULONG __stdcall IDirect3D1Proxy::Release() {
	ULONG count = (ULONG)InterlockedDecrement(reinterpret_cast<LONG*>(&m_refCount));
	if (count == 0) {
		if (m_inner) {
			m_inner->Release();
			m_inner = nullptr;
		}
		//spdlog::trace("IDirect3D1DeviceProxy destroyed");
		delete this;
	}
	return count;
}

HRESULT __stdcall IDirect3D1Proxy::Initialize(REFCLSID rclsid) { return m_inner->Initialize(rclsid); }

struct EnumDevicesContext {
	LPD3DENUMDEVICESCALLBACK gameCallback;
	LPVOID gameArg;
};

static HRESULT CALLBACK EnumDevicesCallbackWrapper(LPGUID lpGuid, LPSTR lpDeviceDescription, LPSTR lpDeviceName, LPD3DDEVICEDESC lpD3DHWDeviceDesc, LPD3DDEVICEDESC lpD3DHELDeviceDesc, LPVOID lpContext) {
	EnumDevicesContext* ctx = static_cast<EnumDevicesContext*>(lpContext);

	if (lpD3DHWDeviceDesc) {
		lpD3DHWDeviceDesc->dwMaxTextureWidth = MAX_TEXTURE_WIDTH;
		lpD3DHWDeviceDesc->dwMaxTextureHeight = MAX_TEXTURE_HEIGHT;
		lpD3DHWDeviceDesc->dwMaxTextureAspectRatio = MAX_TEXTURE_RATIO;
	}
	if (lpD3DHELDeviceDesc) {
		lpD3DHELDeviceDesc->dwMaxTextureWidth = MAX_TEXTURE_WIDTH;
		lpD3DHELDeviceDesc->dwMaxTextureHeight = MAX_TEXTURE_HEIGHT;
		lpD3DHELDeviceDesc->dwMaxTextureAspectRatio = MAX_TEXTURE_RATIO;
	}
	return ctx->gameCallback(lpGuid, lpDeviceDescription, lpDeviceName, lpD3DHWDeviceDesc, lpD3DHELDeviceDesc, ctx->gameArg);
}

HRESULT __stdcall IDirect3D1Proxy::EnumDevices(LPD3DENUMDEVICESCALLBACK lpEnumDevicesCallback, LPVOID lpUserArg) {
	if (!lpEnumDevicesCallback) {
		return m_inner->EnumDevices(nullptr, lpUserArg);
	}
	EnumDevicesContext ctx{};
	ctx.gameCallback = lpEnumDevicesCallback;
	ctx.gameArg = lpUserArg;
	return m_inner->EnumDevices(EnumDevicesCallbackWrapper, &ctx);
}

HRESULT __stdcall IDirect3D1Proxy::CreateLight(LPDIRECT3DLIGHT* lplpDirect3DLight, IUnknown* pUnkOuter) { return m_inner->CreateLight(lplpDirect3DLight, pUnkOuter); }
HRESULT __stdcall IDirect3D1Proxy::CreateMaterial(LPDIRECT3DMATERIAL* lplpDirect3DMaterial, IUnknown* pUnkOuter) { return m_inner->CreateMaterial(lplpDirect3DMaterial, pUnkOuter); }
HRESULT __stdcall IDirect3D1Proxy::CreateViewport(LPDIRECT3DVIEWPORT* lplpD3DViewport, IUnknown* pUnkOuter) { return m_inner->CreateViewport(lplpD3DViewport, pUnkOuter); }
HRESULT __stdcall IDirect3D1Proxy::FindDevice(LPD3DFINDDEVICESEARCH lpD3DDFS, LPD3DFINDDEVICERESULT lpD3DFDR) { return m_inner->FindDevice(lpD3DDFS, lpD3DFDR); }
