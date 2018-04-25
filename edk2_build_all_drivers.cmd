@rem This script builds all the drivers using EDK2 and VS2017.
@echo off
setlocal enabledelayedexpansion

set EDK2_PATH=D:\edk2
set EFIFS_PATH=%~dp0

rem cd /d "%~dp0"

if not exist "%EDK2_PATH%\edksetup.bat" (
  echo ERROR: Please edit the script and make sure EDK2_PATH is set to your EDK2 directory
  pause
  exit 1
)

if not exist "%EDK2_PATH%\BaseTools\Bin\Win32\nasm.exe" (
  echo ERROR: You must have nasm.exe in BaseTools\Bin\Win32
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

if exist "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsamd64_arm.bat" (
  call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsamd64_arm.bat"
  call edksetup.bat
  call EfiFsPkg\set_grub_cpu.cmd ARM
  build -a ARM -b RELEASE -t VS2017 -p EfiFsPkg/EfiFsPkg.dsc
)

if exist "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsamd64_arm64.bat" (
  call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsamd64_arm64.bat"
  call edksetup.bat
  call EfiFsPkg\set_grub_cpu.cmd AARCH64
  build -a AARCH64 -b RELEASE -t VS2017 -p EfiFsPkg/EfiFsPkg.dsc
)

if exist "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars32.bat" (
  call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars32.bat"
  call edksetup.bat
  call EfiFsPkg\set_grub_cpu.cmd IA32
  build -a IA32 -b RELEASE -t VS2017 -p EfiFsPkg/EfiFsPkg.dsc
)

if exist "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat" (
  call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat"
  call edksetup.bat
  call EfiFsPkg\set_grub_cpu.cmd X64
  build -a X64 -b RELEASE -t VS2017 -p EfiFsPkg/EfiFsPkg.dsc
)

pause