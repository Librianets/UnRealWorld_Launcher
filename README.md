# UnReal World Launcher

Windows launcher for the UnReal World translation project, version 0.22.
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
translation bootstrap. The GUI does not inject the DLL or edit game code bytes.
ConsoleMode0/1/2 selects no console, basic output or detailed output;
LoggingEnabled0/1 disables/enables session files under `logs` on the next launch.

## Development and licensing

Game code, saves and game EXEs are not included. Build products are ignored by
Git. See `LICENSE_SCOPE.txt` for the scope and current licensing status.
The launcher source is kept separate so GUI work does not require rebuilding
the translation DLL. See [Russian instructions](README_RU.md).

## Steam launch mode (0.22)

In the launcher, choose the game language under **Launch from Steam**, click
**Change game language**, then **Enable**. Use Steam's Play button afterwards.
The clean game is saved as `urw-original.exe`; a copy of `URWTrace.exe` takes
its place as `urw.exe`. The master `URWTrace.exe` stays beside it.
All three direct launch buttons are disabled while this mode is installed.
Change the game language separately with the game closed, or use **Restore
original**. English runs without loading the translation DLL.

**Repair after update** uses Steam's new original, checks compatibility, then
reinstalls the loader. Unknown game versions are refused. Rollback never
replaces a newer Steam EXE with an older backup. Differing old originals are
preserved under SHA256-based filenames. Do not delete `URWSteam.state` manually.

Build the HookTranslate bootstrap first, then this GUI. `build.cmd` derives
the master-loader hash from local `URWTrace.exe` or the sibling hook build.
Without either, the committed header targets the published0.22 bootstrap.
Installation checks this hash; do not mix an unrelated GUI and bootstrap.
Game code bytes are not patched. The GUI does rename/replace installation
files only when the user explicitly enables, repairs or restores Steam mode.
