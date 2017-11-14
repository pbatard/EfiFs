@rem This script builds all the drivers using the EDK2.
@rem Requires an EDK2 that has been updated for VS2017 support,
@rem such as the one from https://github.com/pbatard/edk2/commits/vs2017

@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

if not exist Edk2Setup.bat (
  echo ERROR: This script must be run from the EDK2 directory
  pause
  exit 1
)

if not exist BaseTools\Bin\Win32\nasm.exe (
  echo ERROR: You must have nasm.exe in BaseTools\Bin\Win32
  pause
  exit 1
)

if not exist EfiFsPkg\set_grub_cpu.cmd (
  mklink /D EfiFsPkg C:\efifs
  if not !ERRORLEVEL! equ 0 (
    echo ERROR: Could not create EfiFsPkg link - Are you running this script as Administrator?
    pause
    exit 1
  )
)

if exist "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsamd64_arm.bat" (
  call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsamd64_arm.bat"
  call edk2setup.bat
  call EfiFsPkg\set_grub_cpu.cmd ARM
  build -a ARM -b RELEASE -t VS2017 -p EfiFsPkg/EfiFsPkg.dsc
)

if exist "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsamd64_arm64.bat" (
  call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvarsamd64_arm64.bat"
  call edk2setup.bat
  call EfiFsPkg\set_grub_cpu.cmd AARCH64
  build -a AARCH64 -b RELEASE -t VS2017 -p EfiFsPkg/EfiFsPkg.dsc
)

if exist "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars32.bat" (
  call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars32.bat"
  call edk2setup.bat
  call EfiFsPkg\set_grub_cpu.cmd IA32
  build -a IA32 -b RELEASE -t VS2017 -p EfiFsPkg/EfiFsPkg.dsc
)

if exist "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat" (
  call "C:\Program Files (x86)\Microsoft Visual Studio\2017\Community\VC\Auxiliary\Build\vcvars64.bat"
  call edk2setup.bat
  call EfiFsPkg\set_grub_cpu.cmd X64
  build -a X64 -b RELEASE -t VS2017 -p EfiFsPkg/EfiFsPkg.dsc
)

pause