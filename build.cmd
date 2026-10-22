@echo off
setlocal
cd /d "%~dp0"
if not defined VSCMD_ARG_TGT_ARCH (
  if exist "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars32.bat" (
    call "%ProgramFiles%\Microsoft Visual Studio\2022\Professional\VC\Auxiliary\Build\vcvars32.bat"
  ) else (
    echo Run from an x86 Visual Studio Native Tools Command Prompt.
    exit /b 1
  )
)
if /i not "%VSCMD_ARG_TGT_ARCH%"=="x86" exit /b 1
if not exist .build\gui mkdir .build\gui
cl /nologo /std:c++17 /utf-8 /O2 /MT /EHsc /W4 /Fo.build\gui\ /Fe:URW_Launcher.exe src\URW_Launcher.cpp /link /MACHINE:X86 /SUBSYSTEM:WINDOWS user32.lib gdi32.lib comctl32.lib
if errorlevel 1 exit /b 1
echo Built URW_Launcher.exe 0.21.
