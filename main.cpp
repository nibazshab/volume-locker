#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <winrt/Windows.Foundation.h>

using namespace winrt;

constexpr float TARGET_VOLUME = 1.0f;
const GUID VOLUME_CTX_GUID = {0x1a2b3c4d, 0x5e6f, 0x7a8b, {0x9c, 0x0d, 0x1e, 0x2f, 0x3a, 0x4b, 0x5c, 0x6d}};

class VolumeCallback : public winrt::implements<VolumeCallback, IAudioEndpointVolumeCallback>
{
public:
    VolumeCallback(IAudioEndpointVolume *pVol) : m_pVol(pVol) {}

    STDMETHODIMP OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA pNotify) override
    {
        if (!pNotify)
            return S_OK;
        if (pNotify->guidEventContext == VOLUME_CTX_GUID)
            return S_OK;
        if (pNotify->fMasterVolume == TARGET_VOLUME)
            return S_OK;

        m_pVol->SetMasterVolumeLevelScalar(TARGET_VOLUME, &VOLUME_CTX_GUID);
        return S_OK;
    }

private:
    IAudioEndpointVolume *m_pVol;
};

int WINAPI WinMain(
    _In_ HINSTANCE,
    _In_opt_ HINSTANCE,
    _In_ LPSTR,
    _In_ int)
{
    init_apartment(apartment_type::multi_threaded);

    auto pEndpointVol = []
    {
        winrt::com_ptr<IAudioEndpointVolume> endpoint;
        winrt::com_ptr<IMMDeviceEnumerator> enumerator{winrt::create_instance<IMMDeviceEnumerator>(__uuidof(MMDeviceEnumerator))};
        winrt::com_ptr<IMMDevice> device;

        enumerator->GetDefaultAudioEndpoint(eRender, eConsole, device.put());
        device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, endpoint.put_void());
        return endpoint;
    }();

    pEndpointVol->SetMasterVolumeLevelScalar(TARGET_VOLUME, &VOLUME_CTX_GUID);
    auto callback = winrt::make_self<VolumeCallback>(pEndpointVol.get());
    pEndpointVol->RegisterControlChangeNotify(callback.get());

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    pEndpointVol->UnregisterControlChangeNotify(callback.get());
    return 0;
}
