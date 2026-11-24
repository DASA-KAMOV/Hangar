// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#define WIN32_LEAN_AND_MEAN
#include <d3d.h>
#include <Windows.h>
#include <atomic>

class IDirect3D1Proxy : public IDirect3D {
private:
	IDirect3D* m_inner;
	std::atomic<ULONG> m_refCount = 0;

public:
	IDirect3D1Proxy(IDirect3D* inner);
	virtual ~IDirect3D1Proxy();
	auto GetInner() const -> auto { return m_inner; }

	// IUnknown
	HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObj) override;
	ULONG __stdcall AddRef() override;
	ULONG __stdcall Release() override;

	// IDirect3D
	HRESULT __stdcall Initialize(REFCLSID rclsid) override;
	HRESULT __stdcall EnumDevices(LPD3DENUMDEVICESCALLBACK lpEnumDevicesCallback, LPVOID lpUserArg) override;
	HRESULT __stdcall CreateLight(LPDIRECT3DLIGHT* lplpDirect3DLight, IUnknown* pUnkOuter) override;
	HRESULT __stdcall CreateMaterial(LPDIRECT3DMATERIAL* lplpDirect3DMaterial, IUnknown* pUnkOuter) override;
	HRESULT __stdcall CreateViewport(LPDIRECT3DVIEWPORT* lplpD3DViewport, IUnknown* pUnkOuter) override;
	HRESULT __stdcall FindDevice(LPD3DFINDDEVICESEARCH lpD3DDFS, LPD3DFINDDEVICERESULT lpD3DFDR) override;
};
