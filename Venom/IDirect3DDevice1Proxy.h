// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#define WIN32_LEAN_AND_MEAN
#include "ProxyCommon.h"
#include <Windows.h>

#include <d3dtypes.h>
#include <d3d.h>
#include <atomic>

#pragma once
#ifndef DIRECT3D_VERSION
#define DIRECT3D_VERSION 0x0700
#endif

inline constexpr DWORD MAX_TEXTURE_HEIGHT = 16384;
inline constexpr DWORD MAX_TEXTURE_WIDTH = 16384;
inline constexpr DWORD MAX_TEXTURE_RATIO = 16384;

class IDirect3DDevice1Proxy : public IDirect3DDevice {
private:
	IDirect3DDevice* m_inner;
	std::atomic<ULONG> m_refCount = 0;

public:
	IDirect3DDevice1Proxy(IDirect3DDevice* inner);
	virtual ~IDirect3DDevice1Proxy();

	// IUnknown
	HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObj) override;
	ULONG __stdcall AddRef() override;
	ULONG __stdcall Release() override;

	// IDirect3DDevice
	HRESULT __stdcall Initialize(LPDIRECT3D lpDirect3D, LPGUID lpGUID, LPD3DDEVICEDESC lpD3DHWDevDesc) override;
	HRESULT __stdcall GetCaps(LPD3DDEVICEDESC lpD3DHWDevDesc, LPD3DDEVICEDESC lpD3DHELDevDesc) override;
	HRESULT __stdcall SwapTextureHandles(LPDIRECT3DTEXTURE lpD3DTex1, LPDIRECT3DTEXTURE lpD3DTex2) override;
	HRESULT __stdcall CreateExecuteBuffer(LPD3DEXECUTEBUFFERDESC lpDesc, LPDIRECT3DEXECUTEBUFFER* lplpDirect3DExecuteBuffer, IUnknown* pUnkOuter) override;
	HRESULT __stdcall GetStats(LPD3DSTATS lpD3DStats) override;
	HRESULT __stdcall Execute(LPDIRECT3DEXECUTEBUFFER lpDirect3DExecuteBuffer, LPDIRECT3DVIEWPORT lpDirect3DViewport, DWORD dwFlags) override;
	HRESULT __stdcall AddViewport(LPDIRECT3DVIEWPORT lpDirect3DViewport) override;
	HRESULT __stdcall DeleteViewport(LPDIRECT3DVIEWPORT lpDirect3DViewport) override;
	HRESULT __stdcall NextViewport(LPDIRECT3DVIEWPORT lpDirect3DViewport, LPDIRECT3DVIEWPORT* lplpDirect3DViewport, DWORD dwFlags) override;
	HRESULT __stdcall Pick(LPDIRECT3DEXECUTEBUFFER lpDirect3DExecuteBuffer, LPDIRECT3DVIEWPORT lpDirect3DViewport, DWORD dwFlags, LPD3DRECT lpRect) override;
	HRESULT __stdcall GetPickRecords(LPDWORD lpCount, LPD3DPICKRECORD lpD3DPickRec) override;
	HRESULT __stdcall EnumTextureFormats(LPD3DENUMTEXTUREFORMATSCALLBACK lpD3DEnumTextureProc, LPVOID lpArg) override;
	HRESULT __stdcall CreateMatrix(LPD3DMATRIXHANDLE lpD3DMatHandle) override;
	HRESULT __stdcall SetMatrix(D3DMATRIXHANDLE d3dMatHandle, const LPD3DMATRIX lpD3DMatrix) override;
	HRESULT __stdcall GetMatrix(D3DMATRIXHANDLE d3dMatHandle, LPD3DMATRIX lpD3DMatrix) override;
	HRESULT __stdcall DeleteMatrix(D3DMATRIXHANDLE d3dMatHandle) override;
	HRESULT __stdcall BeginScene() override;
	HRESULT __stdcall EndScene() override;
	HRESULT __stdcall GetDirect3D(LPDIRECT3D* lplpDirect3D) override;
};