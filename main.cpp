#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <algorithm>
#include <cmath>
#include <endpointvolume.h>
#include <memory>
#include <mmdeviceapi.h>
#include <winrt/Windows.Foundation.h>
#include <windows.h>

constexpr GUID UID = {
    .Data1 = 0xf4d69381, .Data2 = 0xc6d1, .Data3 = 0x43d5, .Data4 = {0xbf, 0x2b, 0xc4, 0x9a, 0x4f, 0xaa, 0xe4, 0xda}
};

namespace {
    struct RegKeyDeleter {
        void operator()(HKEY key) const noexcept {
            if (key) RegCloseKey(key);
        }
    };

    using UniqueRegKey = std::unique_ptr<std::remove_pointer_t<HKEY>, RegKeyDeleter>;

    struct HandleDeleter {
        void operator()(HANDLE h) const noexcept {
            if (h && h != INVALID_HANDLE_VALUE) CloseHandle(h);
        }
    };

    using UniqueHandle = std::unique_ptr<std::remove_pointer_t<HANDLE>, HandleDeleter>;

    class VolumeRegistry {
    public:
        static constexpr DWORD MIN_VOLUME = 0;
        static constexpr DWORD MAX_VOLUME = 100;
        static constexpr DWORD DEFAULT_VOLUME = 100;

        static UniqueRegKey OpenKey(const REGSAM access) {
            HKEY key = nullptr;
            if (RegCreateKeyExW(HKEY_CURRENT_USER, L"Software\\AudioVolumeLocker",
                                0, nullptr, REG_OPTION_NON_VOLATILE, access,
                                nullptr, &key, nullptr) != ERROR_SUCCESS) {
                return nullptr;
            }
            return UniqueRegKey(key);
        }

        static float ReadVolume() {
            const auto key = OpenKey(KEY_READ | KEY_WRITE);
            if (!key) return 1.0f;

            DWORD vol = 0;
            DWORD size = sizeof(vol);
            DWORD type = 0;

            const auto status = RegQueryValueExW(key.get(), L"Volume", nullptr, &type,
                                                 reinterpret_cast<LPBYTE>(&vol), &size);

            if (status == ERROR_FILE_NOT_FOUND || type != REG_DWORD) {
                vol = DEFAULT_VOLUME;
                WriteVolume(key.get(), vol);
            } else {
                vol = std::clamp(vol, MIN_VOLUME, MAX_VOLUME);
            }

            return static_cast<float>(vol) / 100.0f;
        }

    private:
        static void WriteVolume(HKEY key, const DWORD vol) {
            RegSetValueExW(key, L"Volume", 0, REG_DWORD,
                           reinterpret_cast<const BYTE *>(&vol), sizeof(vol));
        }
    };

    class VolumeCallback : public winrt::implements<VolumeCallback, IAudioEndpointVolumeCallback> {
    public:
        explicit VolumeCallback(winrt::com_ptr<IAudioEndpointVolume> vol, const float target)
            : m_volume(std::move(vol)), m_target(target) {
        }

        STDMETHODIMP OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA data) noexcept override {
            if (!data || data->guidEventContext == UID) {
                return S_OK;
            }

            if (std::fabs(data->fMasterVolume - m_target) < 0.001f) {
                return S_OK;
            }

            return m_volume->SetMasterVolumeLevelScalar(m_target, &UID);
        }

    private:
        winrt::com_ptr<IAudioEndpointVolume> m_volume;
        float m_target;
    };
}

int WINAPI WinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {
    if (const auto mutex = UniqueHandle(CreateMutexW(nullptr, TRUE, L"Global\\AudioVolumeLockMutex"));
        !mutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        return 0;
    }

    try {
        winrt::init_apartment(winrt::apartment_type::multi_threaded);

        winrt::com_ptr<IMMDeviceEnumerator> enumerator;
        winrt::check_hresult(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr,
                                              CLSCTX_ALL, IID_PPV_ARGS(enumerator.put())));

        winrt::com_ptr<IMMDevice> device;
        winrt::check_hresult(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, device.put()));

        winrt::com_ptr<IAudioEndpointVolume> volume;
        winrt::check_hresult(device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL,
                                              nullptr, volume.put_void()));

        const float vol = VolumeRegistry::ReadVolume();
        winrt::check_hresult(volume->SetMasterVolumeLevelScalar(vol, &UID));

        const auto callback = winrt::make_self<VolumeCallback>(volume, vol);
        winrt::check_hresult(volume->RegisterControlChangeNotify(callback.get()));

        SetProcessWorkingSetSize(GetCurrentProcess(), static_cast<SIZE_T>(-1), static_cast<SIZE_T>(-1));

        Sleep(INFINITE);

        winrt::check_hresult(volume->UnregisterControlChangeNotify(callback.get()));
    } catch (const winrt::hresult_error &) {
        return 1;
    }

    return 0;
}
