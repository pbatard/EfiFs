efifs - EFI File System Drivers
===============================

This is a GPLv3+ implementation of standalone EFI File System drivers, based on the
[GRUB 2.0](http://www.gnu.org/software/grub/) __read-only__ drivers.

For additional info as well as precompiled drivers, see http://efi.akeo.ie

* __Requirements__:
  * gcc v4.7 or later
  * [Visual Studio 2013](http://www.visualstudio.com/products/visual-studio-community-vs) or later
  * [QEMU](http://www.qemu.org) for testing in Visual Studio

* __Compilation__:
  * [_Common_] Fetch the git submodules with `git submodule init` and `git submodule update`.
  * [_Visual Studio_] Apply the respective patches to the `grub\` and `gnu-efi\` subdirectories.
  * [_Visual Studio_] Open the solution file and hit `F5` to compile and debug the NTFS driver.
  * [_gcc_] Run `make` in the top directory. This creates the gnu-efi and grub libraries.
  * [_gcc_] Go to the `src` directory and run `make` or `make install`.

* __Testing__:  
  The Visual Studio solution automatically sets QEMU up to run and test the drivers.
  For gcc, make sure you have at least one disk with a target partition using the target
  filesystem, and that is not being handled by other EFI filesystem drivers.
  Boot into the EFI shell and run the following:
  * `load fs0:\<fs_name>_x64.efi` or wherever your driver was copied
  * `map -r` this should make a new `fs#` available, eg `fs2:`
  * You should now be able to navigate and access content (in read-only mode)
  * For logging output, set the `FS_LOGGING` shell variable to 1 or more
  * To unload use the `drivers` command, then `unload` with the driver ID

* __Notes__:  
  This is a pure GPLv3+ implementation of an EFI driver. Great care was taken
  not to use non GPLv3 compatible sources, such as rEFInd's `fsw_efi` (GPLv2 only)
  or Intel's FAT driver (requires an extra copyright notice).  
  Also note that the EDK2 files from the include directory use a BSD 2-Clause
  license, which is compatible with the GPL.
