@rem This script builds all the drivers using EDK2 and VS2017.
@echo off
setlocal enabledelayedexpansion

set EDK2_PATH=D:\edk2
set EFIFS_PATH=%~dp0
set NASM_PREFIX=%EDK2_PATH%\BaseTools\Bin\Win32\

rem cd /d "%~dp0"

if not exist "%EDK2_PATH%\edksetup.bat" (
  echo ERROR: Please edit the script and make sure EDK2_PATH is set to your EDK2 directory
  pause
  exit 1
)

if not exist "%NASM_PREFIX%nasm.exe" (
  echo ERROR: You must have nasm.exe in %NASM_PREFIX%nasm.exe
  pause
  exit 1
)

if not exist "%EDK2_PATH%\EfiFsPkg\set_grub_cpu.cmd" (
  mklink /D "%EDK2_PATH%\EfiFsPkg" "%~dp0"
  if not !ERRORLEVEL! equ 0 (
    echo ERROR: Could not create EfiFsPkg link - Are you running this script as Administrator?
    pause
    exit 1
  )
)

cd /d "%EDK2_PATH%"

set ARCH=
if /I "%1"=="win32" set ARCH=ia32
if /I "%1"=="ia32" set ARCH=ia32
if /I "%1"=="x86" set ARCH=ia32
if /I "%1"=="x64" set ARCH=x64
if /I "%1"=="win64" set ARCH=x64
if /I "%1"=="arm" set ARCH=arm
if /I "%1"=="arm64" set ARCH=aarch64
if /I "%1"=="aa64" set ARCH=aarch64
if /I "%1"=="aarch64" set ARCH=aarch64
if not "%ARCH%"=="" goto %ARCH%

:arm
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsamd64_arm.bat" (
  call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsamd64_arm.bat"
  call edksetup.bat reconfig
  call EfiFsPkg\set_grub_cpu.cmd ARM
  build -a ARM -b RELEASE -t VS2017 -p EfiFsPkg/EfiFsPkg.dsc
)
if not "%ARCH%"=="" goto out

:aarch64
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsamd64_arm64.bat" (
  call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsamd64_arm64.bat"
  call edksetup.bat reconfig
  call EfiFsPkg\set_grub_cpu.cmd AARCH64
  build -a AARCH64 -b RELEASE -t VS2017 -p EfiFsPkg/EfiFsPkg.dsc
)
if not "%ARCH%"=="" goto out

:ia32
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars32.bat" (
  call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars32.bat"
  call edksetup.bat reconfig
  call EfiFsPkg\set_grub_cpu.cmd IA32
  build -a IA32 -b RELEASE -t VS2017 -p EfiFsPkg/EfiFsPkg.dsc
)
if not "%ARCH%"=="" goto out

:x64
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat" (
  call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat"
  call edksetup.bat reconfig
  call EfiFsPkg\set_grub_cpu.cmd X64
  build -a X64 -b RELEASE -t VS2017 -p EfiFsPkg/EfiFsPkg.dsc
)

:out
pause