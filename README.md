efifs - An EFI FileSystem Driver
================================

This is a __WORK IN PROGRESS__ GPLv3+ implementation of standalone EFI File System
drivers, based on the GRUB 2.0 read-only ones.

* Requirements:
  * gnu-efi v3.0s or later 
  * gcc v4.7 or later

* Compilation:
  * Fetch the git grub submodule
  * Go to the `grub` directory, run `autogen.sh` and then something like:
    `./configure --disable-nls --with-platform=efi --target=x86_64`
    This is necessary to create the `config.h` required to compile grub files.
  * Edit Make.common and set the variables at the top of the file to the location
  where gnu-efi is installed as well as your target directory for make install.
  * Run `make` in the top directory. This will create the grub fs library we need.
  * Go to the `src` directory and run `make` or `make install`

* Testing:
  Make sure you have at least one disk with an NTFS partition that is not being
  handled by any EFI filesystem driver.
  Boot into the EFI shell and run the following:
  * `load fs0:\fs_driver_x64.efi` or wherever your driver was copied
  * `map -r` this should make a new `fs#` available, eg `fs2:`
  * `dir fs2:` this should list a single read-only `file.txt` file
  * `edit fs2:\file.txt` this should display the content of the file
  * For logging output, set the `FS_LOGGING` shell variable to 1 or more
  * To unload use the `drivers` command and `unload` with the driver ID
    

* Notes:
  This is a __pure__ GPLv3+ implementation of an EFI driver. Great care was taken
  not to use non GPLv3 compatible sources, such as rEFInd's fsw_efi (GPLv2 only)
  or Intel's FAT driver (requires an extra copyright notice).
  Also note that the EDK2 files from the include directory use a BSD 2-Clause
  license, which is compatible with the GPL.
