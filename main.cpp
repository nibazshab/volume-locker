#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <algorithm>
#include <cmath>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <windows.h>
#include <wrl/client.h>
#include <wrl/implements.h>

using namespace Microsoft::WRL;

constexpr GUID LOCK = {
    .Data1 = 0xf4d69381,
    .Data2 = 0xc6d1,
    .Data3 = 0x43d5,
    .Data4 = {0xbf, 0x2b, 0xc4, 0x9a, 0x4f, 0xaa, 0xe4, 0xda}
};

constexpr auto MUT = L"Local\\volume-locker";
constexpr auto REG_K = L"Software\\volume-locker";
constexpr auto REG_V = L"Volume";

static float LoadVolume() {
    DWORD v = 0, n = sizeof(v);

    if (RegGetValueW(HKEY_CURRENT_USER, REG_K, REG_V, RRF_RT_REG_DWORD, nullptr, &v, &n) != ERROR_SUCCESS) {
        v = 0;
        HKEY k;

        if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_K, 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
            RegSetValueExW(k, REG_V, 0, REG_DWORD, reinterpret_cast<const BYTE *>(&v), sizeof(v));
            RegCloseKey(k);
        }
    }

    return static_cast<float>(std::clamp(v, 0UL, 100UL)) / 100.0f;
}

namespace {
class Callback : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IAudioEndpointVolumeCallback> {
    HANDLE wake;
    const float target;

public:
    Callback(HANDLE w, const float t) : wake(w), target(t) {}

    STDMETHODIMP OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA d) noexcept override {
        if (d && d->guidEventContext != LOCK && std::fabs(d->fMasterVolume - target) > 0.001f) {
            SetEvent(wake);
        }
        return S_OK;
    }
};
}

int WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    HANDLE mutex = CreateMutexW(nullptr, TRUE, MUT);

    if (!mutex || GetLastError() == ERROR_ALREADY_EXISTS) {
        if (mutex) {
            CloseHandle(mutex);
        }
        return 0;
    }

    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {
        CloseHandle(mutex);
        return 1;
    }

    {
        ComPtr<IMMDeviceEnumerator> enumerator;
        ComPtr<IMMDevice> device;
        ComPtr<IAudioEndpointVolume> volume;

        HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));

        if (SUCCEEDED(hr)) {
            hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        }

        if (SUCCEEDED(hr)) {
            hr = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, reinterpret_cast<void **>(volume.GetAddressOf()));
        }

        if (SUCCEEDED(hr)) {
            const float target = LoadVolume();
            (void)volume->SetMasterVolumeLevelScalar(target, &LOCK);

            if (HANDLE wake = CreateEventW(nullptr, FALSE, FALSE, nullptr); wake) {
                if (const auto cb = Make<Callback>(wake, target); cb && SUCCEEDED(volume->RegisterControlChangeNotify(cb.Get()))) {
                    while (WaitForSingleObject(wake, INFINITE) == WAIT_OBJECT_0) {
                        (void)volume->SetMasterVolumeLevelScalar(target, &LOCK);
                    }
                }

                CloseHandle(wake);
            }
        }
    }

    CoUninitialize();
    CloseHandle(mutex);
    return 0;
}
