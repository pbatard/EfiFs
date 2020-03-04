rmdir /s /q release
mkdir release

set EDK2_BUILD=D:\edk2\Build\EfiFs\RELEASE_VS2019

mkdir release\aa64
for /f %%f in ('dir /b %EDK2_BUILD%\AARCH64\*.efi') do copy %EDK2_BUILD%\AARCH64\%%f release\aa64\%%~nf_aa64%%~xf

mkdir release\arm
for /f %%f in ('dir /b %EDK2_BUILD%\AARCH64\*.efi') do copy %EDK2_BUILD%\ARM\%%f release\arm\%%~nf_arm%%~xf

mkdir release\ia32
for /f %%f in ('dir /b %EDK2_BUILD%\IA32\*.efi') do copy %EDK2_BUILD%\AARCH64\%%f release\ia32\%%~nf_ia32%%~xf

mkdir release\x64
for /f %%f in ('dir /b %EDK2_BUILD%\X64\*.efi') do copy %EDK2_BUILD%\X64\%%f release\x64\%%~nf_x64%%~xf
