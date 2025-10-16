#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <iostream>
#include <wrl/client.h>
#include <atomic>

#pragma comment(lib, "Ole32.lib")

using Microsoft::WRL::ComPtr;

const float VOICE_SIZE = 1.0f;

static const GUID GUID_MyContext = {0x1a2b3c4d, 0x5e6f, 0x7a8b, {0x9c, 0x0d, 0x1e, 0x2f, 0x3a, 0x4b, 0x5c, 0x6d}};

class VolumeCallback : public IAudioEndpointVolumeCallback
{
public:
    VolumeCallback(ComPtr<IAudioEndpointVolume> endpointVolume)
        : m_endpointVolume(endpointVolume),
          m_refCount(1),
          m_lastSetTime(0),
          m_settingInProgress(false) {}

    STDMETHODIMP QueryInterface(REFIID riid, void **ppv)
    {
        if (riid == IID_IUnknown || riid == __uuidof(IAudioEndpointVolumeCallback))
        {
            *ppv = static_cast<IAudioEndpointVolumeCallback *>(this);
            AddRef();
            return S_OK;
        }
        *ppv = nullptr;
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG)
    AddRef() { return InterlockedIncrement(&m_refCount); }
    STDMETHODIMP_(ULONG)
    Release()
    {
        ULONG refs = InterlockedDecrement(&m_refCount);
        if (refs == 0)
        {
            delete this;
            return 0;
        }
        return refs;
    }

    STDMETHODIMP OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA pNotify)
    {
        if (IsEqualGUID(pNotify->guidEventContext, GUID_MyContext))
            return S_OK;

        if (pNotify->fMasterVolume == VOICE_SIZE)
            return S_OK;

        DWORD currentTime = GetTickCount64();

        if (currentTime - m_lastSetTime < 100)
            return S_OK;

        if (m_settingInProgress.exchange(true))
            return S_OK;

        HRESULT hr = m_endpointVolume->SetMasterVolumeLevelScalar(VOICE_SIZE, const_cast<GUID *>(&GUID_MyContext));

        m_lastSetTime = currentTime;
        m_settingInProgress = false;

        return S_OK;
    }

private:
    ComPtr<IAudioEndpointVolume> m_endpointVolume;
    LONG m_refCount;
    DWORD m_lastSetTime;
    std::atomic<bool> m_settingInProgress;
};

int main()
{
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr))
        return 1;

    ComPtr<IMMDeviceEnumerator> pEnumerator;
    ComPtr<IMMDevice> pDevice;
    ComPtr<IAudioEndpointVolume> pEndpointVolume;
    VolumeCallback *callback = nullptr;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pEnumerator));
    if (FAILED(hr))
        goto Cleanup;

    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    if (FAILED(hr))
        goto Cleanup;

    hr = pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, reinterpret_cast<void **>(pEndpointVolume.GetAddressOf()));
    if (FAILED(hr))
        goto Cleanup;

    callback = new VolumeCallback(pEndpointVolume);
    hr = pEndpointVolume->RegisterControlChangeNotify(callback);
    if (FAILED(hr))
        goto Cleanup;

    std::cout << "Listening for volume changes...\n";
    std::cin.get();

    if (pEndpointVolume && callback)
    {
        pEndpointVolume->UnregisterControlChangeNotify(callback);
    }

Cleanup:
    if (callback)
    {
        callback->Release();
    }

    CoUninitialize();
    return 0;
}
