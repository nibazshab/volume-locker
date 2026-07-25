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
    template<typename F>
    class ScopeExit {
    public:
        explicit ScopeExit(F &&f) : m_func(std::move(f)) {
        }

        ~ScopeExit() { m_func(); }

        ScopeExit(const ScopeExit &) = delete;

        ScopeExit &operator=(const ScopeExit &) = delete;

    private:
        F m_func;
    };

    template<typename F>
    auto scope_exit(F &&f) {
        return ScopeExit<std::decay_t<F> >(std::forward<F>(f));
    }

    class VolumeCallback : public winrt::implements<VolumeCallback, IAudioEndpointVolumeCallback> {
    public:
        explicit VolumeCallback(winrt::com_ptr<IAudioEndpointVolume> pVol) : m_vol(std::move(pVol)) {
        }

        STDMETHODIMP OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA p) override {
            if (!p)
                return S_OK;

            if (p->guidEventContext == UID)
                return S_OK;

            if (std::fabs(p->fMasterVolume - VOL) < 0.001f)
                return S_OK;

            return m_vol->SetMasterVolumeLevelScalar(VOL, &UID);
        }

    private:
        winrt::com_ptr<IAudioEndpointVolume> m_vol;
    };
}

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
    auto hm = CreateMutexW(nullptr, TRUE, L"Global\\AudioVolumeLockMutex");
    if (!hm || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (hm) CloseHandle(hm);
        return 0;
    }

    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);

        winrt::com_ptr<IMMDeviceEnumerator> enumerator;
        winrt::check_hresult(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                              IID_PPV_ARGS(enumerator.put())));

        winrt::com_ptr<IMMDevice> device;
        winrt::check_hresult(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, device.put()));

        winrt::com_ptr<IAudioEndpointVolume> pEndpointVol;
        winrt::check_hresult(device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr,
                                              pEndpointVol.put_void()));

        winrt::check_hresult(pEndpointVol->SetMasterVolumeLevelScalar(VOL, &UID));

        const auto callback = winrt::make_self<VolumeCallback>(pEndpointVol);
        winrt::check_hresult(pEndpointVol->RegisterControlChangeNotify(callback.get()));

        auto unregister_guard = scope_exit([&] {
            winrt::check_hresult(pEndpointVol->UnregisterControlChangeNotify(callback.get()));
        });

        Sleep(INFINITE);
    } catch (winrt::hresult_error const &) {
        return 1;
    }

    return 0;
}
