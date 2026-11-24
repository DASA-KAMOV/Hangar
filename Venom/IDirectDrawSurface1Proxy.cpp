// SPDX-License-Identifier: GPL-3.0-or-later

#include "IDirectDrawSurface1Proxy.h"
#include "IDirect3DDevice1Proxy.h"
#include <ddraw.h>
#include <Windows.h>
#include <spdlog/spdlog.h>
#include "logging.h"
#include <d3d.h>
#include "ProxyCommon.h"
#include <string.h>

IDirectDrawSurface1Proxy::IDirectDrawSurface1Proxy(IDirectDrawSurface* inner) : m_inner(inner), m_refCount(1) {
    //spdlog::trace("IDirectDrawSurface1Proxy created");
}

IDirectDrawSurface1Proxy::~IDirectDrawSurface1Proxy() {} // Intentionally empty. m_inner->Release() is handled by Release().

// IUnknown

HRESULT __stdcall IDirectDrawSurface1Proxy::QueryInterface(REFIID riid, LPVOID* ppvObj) {
    //spdlog::trace("IDirectDrawSurface1Proxy::QueryInterface called with riid: {}", REFIIDToString(riid));
    if (!ppvObj) { return E_POINTER; }

    if (riid == IID_IUnknown || riid == IID_IDirectDrawSurface) {
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
    /*
    else if (riid == IID_IDirect3DHALDevice) {
        *ppvObj = new IDirect3DDevice1Proxy(static_cast<IDirect3DDevice*>(rawInner));
        return S_OK;
    }
    */
    
    //spdlog::warn("IDirectDrawSurface1Proxy: Un-Proxied interface requested: {}", REFIIDToString(riid));
    *ppvObj = rawInner;
    return S_OK;
}

ULONG __stdcall IDirectDrawSurface1Proxy::AddRef() { return (ULONG)InterlockedIncrement(reinterpret_cast<LONG*>(&m_refCount)); }

ULONG __stdcall IDirectDrawSurface1Proxy::Release() {
    ULONG count = (ULONG)InterlockedDecrement(reinterpret_cast<LONG*>(&m_refCount));
    if (count == 0) {
        if (m_inner) {
            m_inner->Release();
            m_inner = nullptr;
        }
        //spdlog::trace("IDirectDrawSurface1Proxy destroyed");
        delete this;
    }
    return count;
}

// IDirectDrawSurface

HRESULT __stdcall IDirectDrawSurface1Proxy::AddAttachedSurface(LPDIRECTDRAWSURFACE lpDDSAttachedSurface) {
    return m_inner->AddAttachedSurface(UnwrapProxy(lpDDSAttachedSurface));
}

HRESULT __stdcall IDirectDrawSurface1Proxy::AddOverlayDirtyRect(LPRECT lpRect) { return m_inner->AddOverlayDirtyRect(lpRect); }


HRESULT __stdcall IDirectDrawSurface1Proxy::Blt(LPRECT lpDestRect, LPDIRECTDRAWSURFACE lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwFlags, LPDDBLTFX lpDDBltFx) {
    return m_inner->Blt(lpDestRect, UnwrapProxy(lpDDSrcSurface), lpSrcRect, dwFlags, lpDDBltFx);
}

HRESULT __stdcall IDirectDrawSurface1Proxy::BltBatch(LPDDBLTBATCH lpDDBltBatch, DWORD dwCount, DWORD dwFlags) { return m_inner->BltBatch(lpDDBltBatch, dwCount, dwFlags); }

HRESULT __stdcall IDirectDrawSurface1Proxy::BltFast(DWORD dwX, DWORD dwY, LPDIRECTDRAWSURFACE lpDDSrcSurface, LPRECT lpSrcRect, DWORD dwTrans) {
    return m_inner->BltFast(dwX, dwY, UnwrapProxy(lpDDSrcSurface), lpSrcRect, dwTrans);
}

HRESULT __stdcall IDirectDrawSurface1Proxy::DeleteAttachedSurface(DWORD dwFlags, LPDIRECTDRAWSURFACE lpDDSAttachedSurface) {
    return m_inner->DeleteAttachedSurface(dwFlags, UnwrapProxy(lpDDSAttachedSurface));
}

struct EnumSurfaceCtx1 {
    LPVOID userContext;
    LPDDENUMSURFACESCALLBACK userCallback;
};

static HRESULT __stdcall FilteredEnumSurfacesCallback1(LPDIRECTDRAWSURFACE lpDDSurface, LPDDSURFACEDESC lpDDSurfaceDesc, LPVOID lpContext) {
    EnumSurfaceCtx1* ctx = static_cast<EnumSurfaceCtx1*>(lpContext);
    if (!lpDDSurface) { return ctx->userCallback(nullptr, lpDDSurfaceDesc, ctx->userContext); }

    lpDDSurface->AddRef();
    IDirectDrawSurface1Proxy* proxySurface = new IDirectDrawSurface1Proxy(lpDDSurface);
    HRESULT result = ctx->userCallback(reinterpret_cast<LPDIRECTDRAWSURFACE>(proxySurface), lpDDSurfaceDesc, ctx->userContext);
    proxySurface->Release();

    return result;
}

HRESULT __stdcall IDirectDrawSurface1Proxy::EnumAttachedSurfaces(LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpEnumSurfacesCallback) {
    if (!lpEnumSurfacesCallback) return m_inner->EnumAttachedSurfaces(lpContext, nullptr);
    EnumSurfaceCtx1 interceptCtx = { lpContext, lpEnumSurfacesCallback };
    return m_inner->EnumAttachedSurfaces(&interceptCtx, FilteredEnumSurfacesCallback1);
}

HRESULT __stdcall IDirectDrawSurface1Proxy::EnumOverlayZOrders(DWORD dwFlags, LPVOID lpContext, LPDDENUMSURFACESCALLBACK lpfnCallback) { return m_inner->EnumOverlayZOrders(dwFlags, lpContext, lpfnCallback); }

HRESULT __stdcall IDirectDrawSurface1Proxy::Flip(LPDIRECTDRAWSURFACE lpDDSurfaceTargetOverride, DWORD dwFlags) {
    IDirectDrawSurface* realSurf = lpDDSurfaceTargetOverride ? static_cast<IDirectDrawSurface1Proxy*>(lpDDSurfaceTargetOverride)->m_inner : nullptr;
    return m_inner->Flip(realSurf, dwFlags);
}

HRESULT __stdcall IDirectDrawSurface1Proxy::GetAttachedSurface(LPDDSCAPS lpDDSCaps, LPDIRECTDRAWSURFACE* lplpDDSAttachedSurface) {
    HRESULT hr = m_inner->GetAttachedSurface(lpDDSCaps, lplpDDSAttachedSurface);
    if (SUCCEEDED(hr) && lplpDDSAttachedSurface && *lplpDDSAttachedSurface) {
        *lplpDDSAttachedSurface = new IDirectDrawSurface1Proxy(*lplpDDSAttachedSurface);
    }
    return hr;
}

HRESULT __stdcall IDirectDrawSurface1Proxy::GetBltStatus(DWORD dwFlags) { return m_inner->GetBltStatus(dwFlags); }
HRESULT __stdcall IDirectDrawSurface1Proxy::GetCaps(LPDDSCAPS lpDDSCaps) { return m_inner->GetCaps(lpDDSCaps); }
HRESULT __stdcall IDirectDrawSurface1Proxy::GetClipper(LPDIRECTDRAWCLIPPER* lplpDDClipper) { return m_inner->GetClipper(lplpDDClipper); }
HRESULT __stdcall IDirectDrawSurface1Proxy::GetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) { return m_inner->GetColorKey(dwFlags, lpDDColorKey); }
HRESULT __stdcall IDirectDrawSurface1Proxy::GetDC(HDC* lphDC) { return m_inner->GetDC(lphDC); }
HRESULT __stdcall IDirectDrawSurface1Proxy::GetFlipStatus(DWORD dwFlags) { return m_inner->GetFlipStatus(dwFlags); }
HRESULT __stdcall IDirectDrawSurface1Proxy::GetOverlayPosition(LPLONG lplX, LPLONG lplY) { return m_inner->GetOverlayPosition(lplX, lplY); }
HRESULT __stdcall IDirectDrawSurface1Proxy::GetPalette(LPDIRECTDRAWPALETTE* lplpDDPalette) { return m_inner->GetPalette(lplpDDPalette); }
HRESULT __stdcall IDirectDrawSurface1Proxy::GetPixelFormat(LPDDPIXELFORMAT lpDDPixelFormat) { return m_inner->GetPixelFormat(lpDDPixelFormat); }
HRESULT __stdcall IDirectDrawSurface1Proxy::GetSurfaceDesc(LPDDSURFACEDESC lpDDSurfaceDesc) { return m_inner->GetSurfaceDesc(lpDDSurfaceDesc); }
HRESULT __stdcall IDirectDrawSurface1Proxy::Initialize(LPDIRECTDRAW lpDD, LPDDSURFACEDESC lpDDSurfaceDesc) { return m_inner->Initialize(lpDD, lpDDSurfaceDesc); }
HRESULT __stdcall IDirectDrawSurface1Proxy::IsLost() { return m_inner->IsLost(); }

//HRESULT __stdcall IDirectDrawSurface1Proxy::Lock(LPRECT lpDestRect, LPDDSURFACEDESC lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent) { return m_inner->Lock(lpDestRect, lpDDSurfaceDesc, dwFlags, hEvent); }
HRESULT __stdcall IDirectDrawSurface1Proxy::Lock(LPRECT lpDestRect, LPDDSURFACEDESC lpDDSurfaceDesc, DWORD dwFlags, HANDLE hEvent) {
    HRESULT hr = m_inner->Lock(lpDestRect, lpDDSurfaceDesc, dwFlags, hEvent);
    if (SUCCEEDED(hr) && lpDDSurfaceDesc != nullptr) {
        m_isLocked = true;
        m_lockedDesc = *lpDDSurfaceDesc;
    }
    return hr;
}

HRESULT __stdcall IDirectDrawSurface1Proxy::ReleaseDC(HDC hDC) { return m_inner->ReleaseDC(hDC); }
HRESULT __stdcall IDirectDrawSurface1Proxy::Restore() { return m_inner->Restore(); }
HRESULT __stdcall IDirectDrawSurface1Proxy::SetClipper(LPDIRECTDRAWCLIPPER lpDDClipper) { return m_inner->SetClipper(lpDDClipper); }

//HRESULT __stdcall IDirectDrawSurface1Proxy::SetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) { return m_inner->SetColorKey(dwFlags, lpDDColorKey); }

HRESULT __stdcall IDirectDrawSurface1Proxy::SetOverlayPosition(LONG lX, LONG lY) { return m_inner->SetOverlayPosition(lX, lY); }

HRESULT __stdcall IDirectDrawSurface1Proxy::SetPalette(LPDIRECTDRAWPALETTE lpDDPalette) { return m_inner->SetPalette(lpDDPalette); }

//HRESULT __stdcall IDirectDrawSurface1Proxy::Unlock(LPVOID lpSurfaceData) { return m_inner->Unlock(lpSurfaceData); }
HRESULT __stdcall IDirectDrawSurface1Proxy::Unlock(LPVOID lpSurface) {
    if (m_isLocked && m_lockedDesc.lpSurface != nullptr) {
        DDSURFACEDESC actualDesc;
        memset(&actualDesc, 0, sizeof(actualDesc));
        actualDesc.dwSize = sizeof(actualDesc);

        if (SUCCEEDED(m_inner->GetSurfaceDesc(&actualDesc))) {
            DWORD caps = actualDesc.ddsCaps.dwCaps;
            DDPIXELFORMAT& pf = actualDesc.ddpfPixelFormat;

            bool isTexture = (caps & DDSCAPS_TEXTURE) != 0;
            bool is16Bit = (pf.dwRGBBitCount == 16);

            bool isRGB565 = (pf.dwRBitMask == 0xF800 && pf.dwGBitMask == 0x07E0 && pf.dwBBitMask == 0x001F);
            bool isRGB555 = (pf.dwRBitMask == 0x7C00 && pf.dwGBitMask == 0x03E0 && pf.dwBBitMask == 0x001F);

            if (isTexture && is16Bit) {
                if (isRGB565 || isRGB555) {
                    BYTE* rowStart = reinterpret_cast<BYTE*>(m_lockedDesc.lpSurface);
                    LONG pitch = actualDesc.lPitch;
                    DWORD height = actualDesc.dwHeight;
                    DWORD width = actualDesc.dwWidth;

                    size_t fixedPixelCount = 0;
 
                    // RGB565: R=1, G=2, B=1 -> 0x0841
                    // RGB555: R=1, G=1, B=1 -> 0x0421 
                    WORD replacementColor = isRGB565 ? 0x0841 : 0x0421;

                    for (DWORD y = 0; y < height; ++y) {
                        WORD* pPixel = reinterpret_cast<WORD*>(rowStart);
                        for (DWORD x = 0; x < width; ++x) {
                            if (pPixel[x] == 0x0000) {
                                pPixel[x] = replacementColor;
                                fixedPixelCount++;
                            }
                        }
                        rowStart += pitch;
                    }
                }
            }
        }

        m_isLocked = false;
        m_lockedDesc.lpSurface = nullptr;
    }
    return m_inner->Unlock(lpSurface);
}

HRESULT __stdcall IDirectDrawSurface1Proxy::SetColorKey(DWORD dwFlags, LPDDCOLORKEY lpDDColorKey) {
    if (!lpDDColorKey) { return m_inner->SetColorKey(dwFlags, lpDDColorKey); }
    DDCOLORKEY modifiedKey = *lpDDColorKey;

    if (modifiedKey.dwColorSpaceLowValue == 0x0000 && modifiedKey.dwColorSpaceHighValue == 0x0000) {

        DDSURFACEDESC actualDesc;
        memset(&actualDesc, 0, sizeof(actualDesc));
        actualDesc.dwSize = sizeof(actualDesc);

        if (SUCCEEDED(m_inner->GetSurfaceDesc(&actualDesc))) {
            if (actualDesc.ddpfPixelFormat.dwRGBBitCount == 16) {

                bool isRGB565 = (actualDesc.ddpfPixelFormat.dwGBitMask == 0x07E0);
                WORD magicColor = isRGB565 ? 0x0841 : 0x0421;

                modifiedKey.dwColorSpaceLowValue = magicColor;
                modifiedKey.dwColorSpaceHighValue = magicColor;
            }
        }
    }
    return m_inner->SetColorKey(dwFlags, &modifiedKey);
}


HRESULT __stdcall IDirectDrawSurface1Proxy::UpdateOverlay(LPRECT lpSrcRect, LPDIRECTDRAWSURFACE lpDDDestSurface, LPRECT lpDestRect, DWORD dwFlags, LPDDOVERLAYFX lpDDOverlayFx) {
    IDirectDrawSurface* realDest = lpDDDestSurface ? static_cast<IDirectDrawSurface1Proxy*>(lpDDDestSurface)->m_inner : nullptr;
    return m_inner->UpdateOverlay(lpSrcRect, realDest, lpDestRect, dwFlags, lpDDOverlayFx);
}

HRESULT __stdcall IDirectDrawSurface1Proxy::UpdateOverlayDisplay(DWORD dwFlags) { return m_inner->UpdateOverlayDisplay(dwFlags); }

HRESULT __stdcall IDirectDrawSurface1Proxy::UpdateOverlayZOrder(DWORD dwFlags, LPDIRECTDRAWSURFACE lpDDSReference) {
    IDirectDrawSurface* realRef = lpDDSReference ? static_cast<IDirectDrawSurface1Proxy*>(lpDDSReference)->m_inner : nullptr;
    return m_inner->UpdateOverlayZOrder(dwFlags, realRef);
}