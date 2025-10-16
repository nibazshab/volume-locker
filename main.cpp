#include <windows.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <iostream>
#include <wrl/client.h>

#pragma comment(lib, "Ole32.lib")

using Microsoft::WRL::ComPtr;

int main()
{
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr))
    {
        return 1;
    }

    ComPtr<IMMDeviceEnumerator> pEnumerator;
    ComPtr<IMMDevice> pDevice;
    ComPtr<IAudioEndpointVolume> pEndpointVolume;

    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&pEnumerator));
    if (FAILED(hr))
        goto Cleanup;

    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    if (FAILED(hr))
        goto Cleanup;

    hr = pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, nullptr, reinterpret_cast<void **>(pEndpointVolume.GetAddressOf()));
    if (FAILED(hr))
        goto Cleanup;

    std::cout << "Starting loop to set system volume to maximum...\n";

    while (true)
    {
        pEndpointVolume->SetMasterVolumeLevelScalar(1.0f, nullptr);
        Sleep(500);
    }

Cleanup:
    CoUninitialize();
    return FAILED(hr);
}
