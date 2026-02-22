#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <cmath>

using namespace winrt;

constexpr float TARGET_VOLUME = 1.0f;
constexpr float EPSILON = 0.001f;

const GUID VOLUME_CTX_GUID = {0xf4d69381, 0xc6d1, 0x43d5, {0xbf, 0x2b, 0xc4, 0x9a, 0x4f, 0xaa, 0xe4, 0xda}};

class VolumeCallback : public winrt::implements<VolumeCallback, IAudioEndpointVolumeCallback>
{
public:
    VolumeCallback(winrt::com_ptr<IAudioEndpointVolume> pVol) : m_pVol(pVol) {}

    STDMETHODIMP OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA pNotify) override
    {
        if (!pNotify)
            return S_OK;

        if (pNotify->guidEventContext == VOLUME_CTX_GUID)
            return S_OK;

        if (fabsf(pNotify->fMasterVolume - TARGET_VOLUME) < EPSILON)
            return S_OK;

        HRESULT hr = m_pVol->SetMasterVolumeLevelScalar(TARGET_VOLUME, &VOLUME_CTX_GUID);

        if (FAILED(hr))
            return S_FALSE;

        return S_OK;
    }

private:
    winrt::com_ptr<IAudioEndpointVolume> m_pVol;
};

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int)
{
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Global\\AudioVolumeLockMutex");

    if (!hMutex || GetLastError() == ERROR_ALREADY_EXISTS)
        return 0;

    init_apartment(apartment_type::multi_threaded);

    winrt::com_ptr<IMMDeviceEnumerator> enumerator;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(enumerator.put()));

    if (FAILED(hr))
        return 1;

    winrt::com_ptr<IMMDevice> device;
    hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, device.put());

    if (FAILED(hr))
        return 2;

    winrt::com_ptr<IAudioEndpointVolume> pEndpointVol;
    hr = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, pEndpointVol.put_void());

    if (FAILED(hr))
        return 3;

    auto callback = winrt::make_self<VolumeCallback>(pEndpointVol);

    hr = pEndpointVol->SetMasterVolumeLevelScalar(TARGET_VOLUME, &VOLUME_CTX_GUID);

    if (FAILED(hr))
        return 4;

    hr = pEndpointVol->RegisterControlChangeNotify(callback.get());

    if (FAILED(hr))
        return 5;

    Sleep(INFINITE);

    pEndpointVol->UnregisterControlChangeNotify(callback.get());

    return 0;
}
