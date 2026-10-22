# UnReal World Launcher

Windows launcher for the UnReal World translation project, version 0.21.
Choose ENG, RUS or FIN, switch interface labels between English, Russian and
Finnish, and configure the translation console and session logging.

## Build

Install Visual Studio 2022 with Desktop development with C++ and the Windows SDK.
Run `build.cmd` from an **x86 Native Tools Command Prompt**. The build produces
`URW_Launcher.exe`; this project has no dependency on MinHook or game files.

## Install

Put `URW_Launcher.exe` beside `urw.exe`. Translated launches also require
`URWTrace.exe` and `URWTraceHook.dll` from **[UnRealWorld_HookTranslate](https://github.com/Librianets/UnRealWorld_HookTranslate)**, and
the language folders plus `URWTrace.ini` from **[UnRealWorld_MultiTranslate](https://github.com/Librianets/UnRealWorld_MultiTranslate)**.
Use the same release version for all components. The shared INI is beside
the executable, not a separate per-language settings file.

ENG launches the configured original game directly. RUS/FIN start the common
translation bootstrap. The GUI itself does not inject the DLL or patch game files.
ConsoleMode0/1/2 selects no console, basic output or detailed output;
LoggingEnabled0/1 disables/enables session files under `logs` on the next launch.

## Development and licensing

Game code, saves and game EXEs are not included. Build products are ignored by
Git. See `LICENSE_SCOPE.txt` for the scope and current licensing status.
The launcher source is kept separate so GUI work does not require rebuilding
the translation DLL. See [Russian instructions](README_RU.md).
