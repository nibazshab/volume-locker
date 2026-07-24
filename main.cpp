#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <winrt/Windows.Foundation.h>

constexpr float VOL = 1.0f;
constexpr GUID UID = {
    .Data1 = 0xf4d69381, .Data2 = 0xc6d1, .Data3 = 0x43d5, .Data4 = {0xbf, 0x2b, 0xc4, 0x9a, 0x4f, 0xaa, 0xe4, 0xda}
};

namespace {
    class VolumeCallback : public winrt::implements<VolumeCallback, IAudioEndpointVolumeCallback> {
    public:
        explicit VolumeCallback(winrt::com_ptr<IAudioEndpointVolume> pVol) : m_vol(std::move(pVol)) {
        }

        STDMETHODIMP OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA pNotify) override {
            if (!pNotify)
                return S_OK;

            if (pNotify->guidEventContext == UID)
                return S_OK;

            if (std::fabs(pNotify->fMasterVolume - VOL) < 0.001f)
                return S_OK;

            return m_vol->SetMasterVolumeLevelScalar(VOL, &UID);
        }

    private:
        winrt::com_ptr<IAudioEndpointVolume> m_vol;
    };
}

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
    HANDLE hMutex = CreateMutexW(nullptr, TRUE, L"Global\\AudioVolumeLockMutex");
    if (!hMutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hMutex) CloseHandle(hMutex);
        return 0;
    }

    winrt::init_apartment(winrt::apartment_type::multi_threaded);

    winrt::com_ptr<IMMDeviceEnumerator> enumerator;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(enumerator.put())))) {
        CloseHandle(hMutex);
        return 1;
    }

    winrt::com_ptr<IMMDevice> device;
    if (FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, device.put()))) {
        CloseHandle(hMutex);
        return 2;
    }

    winrt::com_ptr<IAudioEndpointVolume> pEndpointVol;
    if (FAILED(device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, pEndpointVol.put_void()))) {
        CloseHandle(hMutex);
        return 3;
    }

    if (FAILED(pEndpointVol->SetMasterVolumeLevelScalar(VOL, &UID))) {
        CloseHandle(hMutex);
        return 4;
    }

    const auto callback = winrt::make_self<VolumeCallback>(pEndpointVol);
    if (FAILED(pEndpointVol->RegisterControlChangeNotify(callback.get()))) {
        CloseHandle(hMutex);
        return 5;
    }

    Sleep(INFINITE);
    if (FAILED(pEndpointVol->UnregisterControlChangeNotify(callback.get()))) {
        CloseHandle(hMutex);
        return 6;
    }

    CloseHandle(hMutex);
    return 0;
}
