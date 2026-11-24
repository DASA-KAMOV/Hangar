// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#define WIN32_LEAN_AND_MEAN
#include <ddraw.h>
#include <atomic>

class IDirectDrawSurface1Proxy : public IDirectDrawSurface {
private:
    IDirectDrawSurface* m_inner = nullptr;
    std::atomic<ULONG> m_refCount = 0;

    DDSURFACEDESC m_lockedDesc = {};
    bool m_isLocked = false;

public:
    explicit IDirectDrawSurface1Proxy(IDirectDrawSurface* inner);
    virtual ~IDirectDrawSurface1Proxy();
    IDirectDrawSurface* GetInner() const { return m_inner; }

    // IUnknown
    HRESULT __stdcall QueryInterface(REFIID riid, LPVOID* ppvObj) override;
    ULONG   __stdcall AddRef() override;
    ULONG   __stdcall Release() override;

    // IDirectDrawSurface
    HRESULT __stdcall AddAttachedSurface(LPDIRECTDRAWSURFACE lpDDAttachedSurface) override;
    HRESULT __stdcall AddOverlayDirtyRect(LPRECT lpRect) override;
    HRESULT __stdcall Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx) override;
    HRESULT __stdcall BltBatch(LPDDBLTBATCH lpDDBltBatch, DWORD dwCount, DWORD dwFlags) override;
    HRESULT __stdcall BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans) override;
    HRESULT __stdcall DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE lpDDAttachedSurface) override;
    HRESULT __stdcall EnumAttachedSurfaces(LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback) override;
    HRESULT __stdcall EnumOverlayZOrders(DWORD dwFlags, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpfnCallback) override;
    HRESULT __stdcall Flip(LPDIRECTDRAWSURFACE lpDDSurfaceTargetOverride, DWORD dwFlags) override;
    HRESULT __stdcall GetAttachedSurface(LPDDSCAPS lpDDSCaps, LPDIRECTDRAWSURFACE* lplpDDAttachedSurface) override;
    HRESULT __stdcall GetBltStatus(DWORD dwFlags) override;
    HRESULT __stdcall GetCaps(LPDDSCAPS lpDDSCaps) override;
    HRESULT __stdcall GetClipper(LPDIRECTDRAWCLIPPER* lplpDDClipper) override;
    HRESULT __stdcall GetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) override;
    HRESULT __stdcall GetDC(HDC* lphDC) override;
    HRESULT __stdcall GetFlipStatus(DWORD dwFlags) override;
    HRESULT __stdcall GetOverlayPosition(LPLONG lplX, LPLONG lplY) override;
    HRESULT __stdcall GetPalette(LPDIRECTDRAWPALETTE* lplpDDPalette) override;
    HRESULT __stdcall GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat) override;
    HRESULT __stdcall GetSurfaceDesc(LPDDSURFACEDESC lpDDSurfaceDesc) override;
    HRESULT __stdcall Initialize(LPDIRECTDRAW lpDD, LPDDSURFACEDESC lpDDSurfaceDesc) override;
    HRESULT __stdcall IsLost() override;
    HRESULT __stdcall Lock(LPRECT lpDestRect, LPDDSURFACEDESC lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent) override;
    HRESULT __stdcall ReleaseDC(HDC hDC) override;
    HRESULT __stdcall Restore() override;
    HRESULT __stdcall SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper) override;
    HRESULT __stdcall SetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) override;
    HRESULT __stdcall SetOverlayPosition(LONG lX, LONG lY) override;
    HRESULT __stdcall SetPalette(LPDIRECTDRAWPALETTE lpDDPalette) override;
    HRESULT __stdcall Unlock(LPVOID lpSurfaceData) override;
    HRESULT __stdcall UpdateOverlay(LPRECT lpSrcRect, LPDIRECTDRAWSURFACE lpDDDestSurface, LPRECT lpDestRect, DWORD dwFlags, LPDDOVERLAYFX lpDDOverlayFx) override;
    HRESULT __stdcall UpdateOverlayDisplay(DWORD dwFlags) override;
    HRESULT __stdcall UpdateOverlayZOrder(DWORD dwFlags, LPDIRECTDRAWSURFACE lpDDSReference) override;
};