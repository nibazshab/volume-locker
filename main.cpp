#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>
#include <wrl/implements.h>
#include <algorithm>
#include <cmath>

#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "user32.lib")

using namespace Microsoft::WRL;

constexpr GUID LOCK = {
    .Data1 = 0xf4d69381,
    .Data2 = 0xc6d1,
    .Data3 = 0x43d5,
    .Data4 = {0xbf, 0x2b, 0xc4, 0x9a, 0x4f, 0xaa, 0xe4, 0xda}
};

constexpr auto REG_K = L"Software\\AudioVolumeLocker";
constexpr auto REG_V = L"Volume";

static float LoadVolume() {
    HKEY k;
    DWORD v = 100, n = sizeof(v), t = 0;

    if (RegCreateKeyExW(HKEY_CURRENT_USER, REG_K, 0, nullptr, 0,KEY_READ | KEY_WRITE, nullptr, &k, nullptr)
        == ERROR_SUCCESS) {
        
        if (RegQueryValueExW(k, REG_V, nullptr, &t, reinterpret_cast<BYTE *>(&v), &n)
            != ERROR_SUCCESS
            || t != REG_DWORD) {
            
            RegSetValueExW(k, REG_V, 0, REG_DWORD, reinterpret_cast<BYTE *>(&v), sizeof(v));
        }

        RegCloseKey(k);
    }

    return static_cast<float>(std::clamp(v, 0UL, 100UL)) / 100.0f;
}

namespace {
    class Callback : public RuntimeClass<RuntimeClassFlags<ClassicCom>, IAudioEndpointVolumeCallback> {
        ComPtr<IAudioEndpointVolume> volume;
        float target;

    public:
        Callback(ComPtr<IAudioEndpointVolume> v, const float t) : volume(std::move(v)), target(t) {
        }

        STDMETHODIMP OnNotify(
            PAUDIO_VOLUME_NOTIFICATION_DATA d) noexcept override {
            if (d && d->guidEventContext != LOCK
                && std::fabs(d->fMasterVolume - target) > 0.001f) {
                
                volume->SetMasterVolumeLevelScalar(target, &LOCK);
            }

            return S_OK;
        }
    };
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"Global\\AudioVolumeLockMutex");

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

        HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,IID_PPV_ARGS(&enumerator));

        if (SUCCEEDED(hr)) {
            hr = enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        }

        if (SUCCEEDED(hr)) {
            hr = device->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, reinterpret_cast<void **>(volume.GetAddressOf()));
        }

        if (SUCCEEDED(hr)) {
            const float target = LoadVolume();
            volume->SetMasterVolumeLevelScalar(target, &LOCK);

            if (const auto cb = Make<Callback>(volume, target);
                cb && SUCCEEDED(volume->RegisterControlChangeNotify(cb.Get()))) {
                
                Sleep(INFINITE);
            }
        }
    }

    CoUninitialize();
    CloseHandle(mutex);
    return 0;
}
