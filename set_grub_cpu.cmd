@echo off

set ARCH=
if "%1"=="Win32" set ARCH=i386
if "%1"=="x64" set ARCH=x86_64
if "%1"=="ARM" set ARCH=arm
if "%ARCH%"=="" (
  echo Unsupported arch %1
  exit 1
)
echo %ARCH%

if not exist "cpu\%ARCH%" (
  echo Duplicating GRUB include\grub\%ARCH%\ to include\grub\cpu\
  rmdir /S /Q cpu >NUL 2>&1
  xcopy "%ARCH%" "cpu" /i /q /s /y /z
  echo %ARCH% > cpu/%ARCH%
)
