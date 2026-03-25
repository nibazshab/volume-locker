#![windows_subsystem = "windows"]

use windows::Win32::Foundation::{GetLastError, ERROR_ALREADY_EXISTS};
use windows::Win32::Media::Audio::Endpoints::{
    IAudioEndpointVolume, IAudioEndpointVolumeCallback, IAudioEndpointVolumeCallback_Impl,
};
use windows::Win32::Media::Audio::{
    eConsole, eRender, IMMDeviceEnumerator, MMDeviceEnumerator, AUDIO_VOLUME_NOTIFICATION_DATA,
};
use windows::Win32::System::Com::{
    CoCreateInstance, CoInitializeEx, CLSCTX_ALL, COINIT_MULTITHREADED,
};
use windows::Win32::System::Threading::{
    CreateMutexW, GetCurrentProcess, SetProcessWorkingSetSize, Sleep, INFINITE,
};
use windows_core::{implement, w, Result, GUID};

const TARGET_VOLUME: f32 = 1.0;
const VOLUME_CTX_GUID: GUID = GUID::from_u128(0xf4d69381_c6d1_43d5_bf2b_c49a4faae4da);

#[implement(IAudioEndpointVolumeCallback)]
struct VolumeCallback {
    vol: IAudioEndpointVolume,
}

impl IAudioEndpointVolumeCallback_Impl for VolumeCallback_Impl {
    fn OnNotify(&self, data: *mut AUDIO_VOLUME_NOTIFICATION_DATA) -> Result<()> {
        unsafe {
            if data.is_null() {
                return Ok(());
            }

            let data = &*data;

            if data.guidEventContext == VOLUME_CTX_GUID {
                return Ok(());
            }

            if (data.fMasterVolume - TARGET_VOLUME).abs() < 0.001 {
                return Ok(());
            }

            self.vol
                .SetMasterVolumeLevelScalar(TARGET_VOLUME, &VOLUME_CTX_GUID)
        }
    }
}

fn main() -> Result<()> {
    unsafe {
        let _mutex = CreateMutexW(None, true, w!("Global\\AudioVolumeLockMutex"))?;
        if GetLastError() == ERROR_ALREADY_EXISTS {
            return Ok(());
        }

        CoInitializeEx(None, COINIT_MULTITHREADED).ok()?;

        let enumerator: IMMDeviceEnumerator =
            CoCreateInstance(&MMDeviceEnumerator, None, CLSCTX_ALL)?;

        let vol: IAudioEndpointVolume = enumerator
            .GetDefaultAudioEndpoint(eRender, eConsole)?
            .Activate(CLSCTX_ALL, None)?;

        vol.SetMasterVolumeLevelScalar(TARGET_VOLUME, &VOLUME_CTX_GUID)?;

        let _ = SetProcessWorkingSetSize(GetCurrentProcess(), !0, !0);

        let callback: IAudioEndpointVolumeCallback = VolumeCallback { vol: vol.clone() }.into();
        vol.RegisterControlChangeNotify(&callback)?;

        Sleep(INFINITE);

        Ok(())
    }
}
