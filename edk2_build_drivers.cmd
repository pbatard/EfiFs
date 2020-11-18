@rem This script builds all the drivers using EDK2 and VS2019.
@echo off
setlocal enabledelayedexpansion

set EDK2_PATH=D:\edk2
set EFIFS_PATH=%~dp0
set NASM_PREFIX=%EDK2_PATH%\BaseTools\Bin\Win32\
set BUILD=RELEASE

rem cd /d "%~dp0"

if not exist "%EDK2_PATH%\edksetup.bat" (
  echo ERROR: Please edit the script and make sure EDK2_PATH is set to your EDK2 directory
  pause
  exit /b 1
)

if not exist "%NASM_PREFIX%nasm.exe" (
  echo ERROR: You must have nasm.exe in %NASM_PREFIX%nasm.exe
  pause
  exit /b 1
)

if not exist "%EDK2_PATH%\EfiFsPkg\set_grub_cpu.cmd" (
  mklink /D "%EDK2_PATH%\EfiFsPkg" "%~dp0"
  if not !ERRORLEVEL! equ 0 (
    echo ERROR: Could not create EfiFsPkg link - Are you running this script as Administrator?
    pause
    exit /b 1
  )
)

cd /d "%EDK2_PATH%"

if /I "%1"=="win32" goto ia32
if /I "%1"=="ia32" goto ia32
if /I "%1"=="x86" goto ia32
if /I "%1"=="x64" goto x64
if /I "%1"=="win64" goto x64
if /I "%1"=="arm" goto arm
if /I "%1"=="arm64" goto aarch64
if /I "%1"=="aa64" goto aarch64
if /I "%1"=="aarch64" goto aarch64

:arm
setlocal
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsamd64_arm.bat" (
  call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsamd64_arm.bat"
  call edksetup.bat reconfig
  call EfiFsPkg\set_grub_cpu.cmd ARM
  call build -a ARM -b %BUILD% -t VS2019 -p EfiFsPkg/EfiFsPkg.dsc
)
endlocal
if not "%1"=="" goto out

:aarch64
setlocal
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsamd64_arm64.bat" (
  call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsamd64_arm64.bat"
  call edksetup.bat reconfig
  call EfiFsPkg\set_grub_cpu.cmd AARCH64
  call build -a AARCH64 -b %BUILD% -t VS2019 -p EfiFsPkg/EfiFsPkg.dsc
)
endlocal
if not "%1"=="" goto out

:ia32
setlocal
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars32.bat" (
  call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars32.bat"
  call edksetup.bat reconfig
  call EfiFsPkg\set_grub_cpu.cmd IA32
  call build -a IA32 -b %BUILD% -t VS2019 -p EfiFsPkg/EfiFsPkg.dsc
)
endlocal
if not "%1"=="" goto out

:x64
setlocal
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" (
  call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
  call edksetup.bat reconfig
  call EfiFsPkg\set_grub_cpu.cmd X64
  call build -a X64 -b %BUILD% -t VS2019 -p EfiFsPkg/EfiFsPkg.dsc
)
endlocal

:out
pause
