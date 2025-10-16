#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <iostream>
#include <wrl/client.h>
#include <wrl/implements.h>

#pragma comment(lib, "Ole32.lib")

using Microsoft::WRL::ClassicCom;
using Microsoft::WRL::ComPtr;
using Microsoft::WRL::Make;
using Microsoft::WRL::RuntimeClass;
using Microsoft::WRL::RuntimeClassFlags;

const float TARGET_VOLUME_LEVEL = 1.0f;

static const GUID GUID_AppEventContext = {0x1a2b3c4d, 0x5e6f, 0x7a8b, {0x9c, 0x0d, 0x1e, 0x2f, 0x3a, 0x4b, 0x5c, 0x6d}};

class VolumeChangeCallback : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IAudioEndpointVolumeCallback>
{
public:
    VolumeChangeCallback(ComPtr<IAudioEndpointVolume> endpointVolume)
        : m_endpointVolume(endpointVolume), m_isSettingVolume(false)
    {
    }

    STDMETHODIMP OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA pNotify) override
    {
        if (pNotify && IsEqualGUID(pNotify->guidEventContext, GUID_AppEventContext))
        {
            return S_OK;
        }

        if (pNotify && pNotify->fMasterVolume == TARGET_VOLUME_LEVEL)
        {
            return S_OK;
        }

        bool expected = false;
        if (!m_isSettingVolume.compare_exchange_strong(expected, true))
        {
            return S_OK;
        }

        m_endpointVolume->SetMasterVolumeLevelScalar(TARGET_VOLUME_LEVEL, &GUID_AppEventContext);
        m_isSettingVolume = false;

        return S_OK;
    }

private:
    ComPtr<IAudioEndpointVolume> m_endpointVolume;
    std::atomic<bool> m_isSettingVolume;
};

class VolumeCallbackUnregister
{
public:
    VolumeCallbackUnregister(ComPtr<IAudioEndpointVolume> volume, IAudioEndpointVolumeCallback *callback)
        : m_volume(volume), m_callback(callback)
    {
    }

    ~VolumeCallbackUnregister()
    {
        if (m_volume && m_callback)
        {
            m_volume->UnregisterControlChangeNotify(m_callback);
        }
    }

    VolumeCallbackUnregister(const VolumeCallbackUnregister &) = delete;
    VolumeCallbackUnregister &operator=(const VolumeCallbackUnregister &) = delete;

private:
    ComPtr<IAudioEndpointVolume> m_volume;
    IAudioEndpointVolumeCallback *m_callback;
};

class COMInitializer
{
public:
    COMInitializer() : m_initialized(SUCCEEDED(CoInitialize(nullptr))) {}
    ~COMInitializer()
    {
        if (m_initialized)
        {
            CoUninitialize();
        }
    }
    explicit operator bool() const { return m_initialized; }

private:
    bool m_initialized;
};

int main()
{
    COMInitializer comInit;
    if (!comInit)
        return 1;

    ComPtr<IMMDeviceEnumerator> pEnumerator;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pEnumerator));
    if (FAILED(hr))
        return 1;

    ComPtr<IMMDevice> pDevice;
    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    if (FAILED(hr))
        return 1;

    ComPtr<IAudioEndpointVolume> pEndpointVolume;
    hr = pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, &pEndpointVolume);
    if (FAILED(hr))
        return 1;

    pEndpointVolume->SetMasterVolumeLevelScalar(TARGET_VOLUME_LEVEL, &GUID_AppEventContext);

    auto callback = Make<VolumeChangeCallback>(pEndpointVolume);
    if (!callback)
        return 1;

    hr = pEndpointVolume->RegisterControlChangeNotify(callback.Get());
    if (FAILED(hr))
        return 1;

    VolumeCallbackUnregister unregisterGuard(pEndpointVolume, callback.Get());

    std::cout << "Volume is locked..." << std::endl;
    std::cin.get();

    return 0;
}
