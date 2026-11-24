// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#define WIN32_LEAN_AND_MEAN
#define DIRECTINPUT_VERSION 0x0700
#include <dinput.h>
#include <Windows.h>
#include <atomic>
#include "ProxyCommon.h"

class IDirectInputAProxy : public IDirectInputA {

private:
    IDirectInputA* m_inner;
    std::atomic<ULONG> m_refCount = 0;
    void TrackCreatedDevice(LPDIRECTINPUTDEVICEA device);

public:
    IDirectInputAProxy(IDirectInputA* inner);
    virtual ~IDirectInputAProxy();
    auto GetInner() const -> auto { return m_inner; }

    // IUnknown
    HRESULT __stdcall QueryInterface(REFIID riid, void** ppvObj) override;
    ULONG __stdcall AddRef() override;
    ULONG __stdcall Release() override;

    // IDirectInputA
    HRESULT __stdcall CreateDevice(REFGUID rguid, LPDIRECTINPUTDEVICEA* lplpDirectInputDevice, LPUNKNOWN pUnkOuter) override;
    HRESULT __stdcall EnumDevices(DWORD dwDevType, LPDIENUMDEVICESCALLBACKA lpCallback, LPVOID pvRef, DWORD dwFlags) override;
    HRESULT __stdcall GetDeviceStatus(REFGUID rguid) override;
    HRESULT __stdcall RunControlPanel(HWND hwndOwner, DWORD dwFlags) override;
    HRESULT __stdcall Initialize(HINSTANCE hinst, DWORD dwVersion) override;
};