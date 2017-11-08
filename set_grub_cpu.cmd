@echo off

set ARCH=
if /I "%1"=="win32" set ARCH=i386
if /I "%1"=="ia32" set ARCH=i386
if /I "%1"=="x86" set ARCH=i386
if /I "%1"=="x64" set ARCH=x86_64
if /I "%1"=="win64" set ARCH=x86_64
if /I "%1"=="arm" set ARCH=arm
if /I "%1"=="arm64" set ARCH=arm64
if /I "%1"=="aa64" set ARCH=arm64
if /I "%1"=="aarch64" set ARCH=arm64
if "%ARCH%"=="" (
  echo Unsupported arch %1
  exit 1
)
echo Setting GRUB for %ARCH%...

if not exist "%~dp0\grub\include\grub\cpu\%ARCH%" (
  echo Duplicating grub\include\grub\%ARCH%\ to grub\include\grub\cpu\
  rmdir /S /Q "%~dp0\grub\include\grub\cpu" >NUL 2>&1
  xcopy "%~dp0\grub\include\grub\%ARCH%" "%~dp0\grub\include\grub\cpu" /i /q /s /y /z
  echo %ARCH% > "%~dp0\grub\include\grub\cpu\%ARCH%"
)
