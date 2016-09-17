efifs - EFI File System Drivers
===============================

This is a GPLv3+ implementation of standalone EFI File System drivers, based on the
[GRUB 2.0](http://www.gnu.org/software/grub/) __read-only__ drivers.

For additional info as well as precompiled drivers, see http://efi.akeo.ie

## Requirements

* [Visual Studio 2015](http://www.visualstudio.com/products/visual-studio-community-vs) or MinGW/gcc
* A git client able to initialize/update submodules
* [QEMU](http://www.qemu.org) __v2.7 or later__ if debugging with Visual Studio
  (NB: You can find QEMU Windows binaries [here](https://qemu.weilnetz.de/w64/))

## Compilation

* [_Common_] Fetch the git submodules with `git submodule init` and `git submodule update`.
* [_Common_] Apply the included f2fs patch to the `grub\` subdirectory. This adds F2FS support,
  which is not yet included in GRUB2.
* [_Visual Studio_] Apply the other patches to the `grub\` subdirectory. If you are using Clang/C2
  you can apply the first patch only. If you are using MSVC, then you must apply both patches.
* [_Visual Studio_] Open the solution file and hit `F5` to compile and debug the default driver.
* [_gcc_] Run `make` in the top directory. If needed you can also issue something like
  `make ARCH=<arch> CROSS_COMPILE=<tuple>` where `<arch>` is one of `ia32`, `x64` or `arm`
  and tuple is the one for your cross-compiler (e.g. `arm-linux-gnueabihf-`).

## Testing

If QEMU is installed, the Visual Studio solution will set up and test the drivers using QEMU
(by also downloading a sample image for each target file system).  
Note however that VS debugging expects a 64-bit version of QEMU to be installed in
`C:\Program Files\qemu\` (which you can download [here](https://qemu.weilnetz.de/w64/)).  
If that is not the case, you should edit `.msvc\debug.vbs` accordingly.

For testing outside of Visual Studio, make sure you have at least one disk with a target
partition using the target filesystem, that is not being handled by other EFI filesystem
drivers.  
Then boot into the EFI shell and run the following:
* `load fs0:\<fs_name>_<arch>.efi` or wherever your driver was copied
* `map -r` this should make a new `fs#` available, eg `fs2:`
* You should now be able to navigate and access content (in read-only mode)
* For logging output, set the `FS_LOGGING` shell variable to 1 or more
* To unload use the `drivers` command, then `unload` with the driver ID

## Visual Studio and ARM support

To enable ARM compilation in Visual Studio 2015, you must perform the following:
* Make sure Visual Studio is fully closed.
* Navigate to `C:\Program Files (x86)\MSBuild\Microsoft.Cpp\v4.0\V140\Platforms\ARM` and
  remove the read-only attribute on `Platform.Common.props`.
* With a text editor __running with Administrative privileges__ open:  
  `C:\Program Files (x86)\MSBuild\Microsoft.Cpp\v4.0\V140\Platforms\ARM\Platform.Common.props`.
* Under the `<PropertyGroup>` section add the following:  
  `<WindowsSDKDesktopARMSupport>true</WindowsSDKDesktopARMSupport>`
  
##Notes

This is a pure GPLv3+ implementation of EFI drivers. Great care was taken
not to use non GPLv3 compatible sources, such as rEFInd's `fsw_efi` (GPLv2 only)
or Intel's FAT driver (requires an extra copyright notice).
