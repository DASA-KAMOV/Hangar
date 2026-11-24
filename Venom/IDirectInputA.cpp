// SPDX-License-Identifier: GPL-3.0-or-later

#include "IDirectInputAProxy.h"
#include "logging.h"
#include <windows.h>
#include <dinput.h>
#include <vector>
#include <mutex>

std::vector<LPDIRECTINPUTDEVICEA> inputDevices;
std::mutex deviceMutex;
HWND gpWnd = NULL;
WNDPROC originalWndProc = NULL;

IDirectInputAProxy::IDirectInputAProxy(IDirectInputA* inner) : m_inner(inner), m_refCount(1) {}
IDirectInputAProxy::~IDirectInputAProxy() {} // Intentionally empty. m_inner->Release() is handled by Release().

HRESULT __stdcall IDirectInputAProxy::QueryInterface(REFIID riid, void** ppvObj) {
    if (!ppvObj) { return E_POINTER; }
    *ppvObj = nullptr;

    if (riid == IID_IUnknown || riid == IID_IDirectInputA) {
        *ppvObj = static_cast<IDirectInputA*>(this);
        AddRef();
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

    //spdlog::warn("IDirectDraw1Proxy: Un-Proxied interface requested: {}", REFIIDToString(riid));
    *ppvObj = rawInner;
    return S_OK;
}

ULONG __stdcall IDirectInputAProxy::AddRef() { return (ULONG)InterlockedIncrement(reinterpret_cast<LONG*>(&m_refCount)); }

ULONG __stdcall IDirectInputAProxy::Release() {
    ULONG count = (ULONG)InterlockedDecrement(reinterpret_cast<LONG*>(&m_refCount));
    if (count == 0) {
        if (m_inner) {
            m_inner->Release();
            m_inner = nullptr;
        }
        //spdlog::trace("IDirectInputAProxy destroyed");
        delete this;
    }
    return count;
}

HRESULT __stdcall IDirectInputAProxy::CreateDevice(REFGUID rguid, LPDIRECTINPUTDEVICEA* lplpDirectInputDevice, LPUNKNOWN pUnkOuter) {
    if (!lplpDirectInputDevice) { return E_POINTER; }

    HRESULT hr = m_inner->CreateDevice(rguid, lplpDirectInputDevice, pUnkOuter);

    if (SUCCEEDED(hr) && lplpDirectInputDevice && *lplpDirectInputDevice) {
        TrackCreatedDevice(*lplpDirectInputDevice);
    }
    return hr;
}

HRESULT __stdcall IDirectInputAProxy::EnumDevices(DWORD dwDevType, LPDIENUMDEVICESCALLBACKA lpCallback, LPVOID pvRef, DWORD dwFlags) { return m_inner->EnumDevices(dwDevType, lpCallback, pvRef, dwFlags); }
HRESULT __stdcall IDirectInputAProxy::GetDeviceStatus(REFGUID rguid) { return m_inner->GetDeviceStatus(rguid); }
HRESULT __stdcall IDirectInputAProxy::RunControlPanel(HWND hwndOwner, DWORD dwFlags) { return m_inner->RunControlPanel(hwndOwner, dwFlags); }
HRESULT __stdcall IDirectInputAProxy::Initialize(HINSTANCE hinst, DWORD dwVersion) { return m_inner->Initialize(hinst, dwVersion); }

void EnsureWindowIsSubclassed();

LRESULT __stdcall HookedWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_ACTIVATE || uMsg == WM_SETFOCUS) {
        if (uMsg != WM_ACTIVATE || LOWORD(wParam) != WA_INACTIVE) {
            SetForegroundWindow(hwnd);
            SetFocus(hwnd);
            std::vector<LPDIRECTINPUTDEVICEA> devicesToAcquire;
            {
                std::lock_guard<std::mutex> lock(deviceMutex);
                devicesToAcquire = inputDevices;
            }

            for (IDirectInputDeviceA* device : devicesToAcquire) {
                if (device) {  HRESULT hr = device->Acquire(); }
            }
        }
    }
    return CallWindowProc(originalWndProc, hwnd, uMsg, wParam, lParam);
}

BOOL __stdcall FindGameWindowEnum(HWND hwnd, LPARAM lParam) {
    HWND* outHwnd = reinterpret_cast<HWND*>(lParam);
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);

    if (processId == GetCurrentProcessId()) {
        if (IsWindowVisible(hwnd)) {
            *outHwnd = hwnd;
            return FALSE;
        }
    }
    return TRUE;
}

void EnsureWindowIsSubclassed() {
    HWND foundWindow = nullptr;
    EnumWindows(FindGameWindowEnum, reinterpret_cast<LPARAM>(&foundWindow));

    if (foundWindow && foundWindow != gpWnd) {
        gpWnd = foundWindow;
        originalWndProc = reinterpret_cast<WNDPROC>(SetWindowLongPtr(gpWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(HookedWndProc)));
    }
}

void IDirectInputAProxy::TrackCreatedDevice(LPDIRECTINPUTDEVICEA device) {
    if (!device) { return; }

    EnsureWindowIsSubclassed();

    std::lock_guard<std::mutex> lock(deviceMutex);

    for (IDirectInputDeviceA* d : inputDevices) {
        if (d == device) { return; }
    }

    device->AddRef();
    inputDevices.push_back(device);
}