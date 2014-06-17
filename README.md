efifs - An EFI FileSystem Driver
================================

This is a __WORK IN PROGRESS__ GPLv3+ implementation of an EFI File System driver,
aimed at providing some more useful functionality in the near future.

* Requirements:
  * gnu-efi v3.0s or later 
  * gcc v4.7 or later

* Compilation:
  * Edit Make.common and set the variables at the top of the file to the location
  where gnu-efi is installed as well as your target directory for make install.
  * Go to the 'src' directory and run make/make install

* Testing:
  Make sure you have at least one disk with a partition that is not being
  handled by any EFI filesystem driver (eg NTFS)
  Boot into the EFI shell and run the following:
  * `load fs0:\fs_driver_x64.efii` # or wherever your driver was copied
  * `map -r` # this should make a new fs# available, eg `fs2:`
  * `dir fs2: # this should list a single read-only `file.txt` file
  * `edit fs2:\file.txt` # this should display the content of the file

* Notes:
  This is a __pure__ GPLv3+ implementation of an EFI driver. Great care was taken
  not to use non GPLv3 compatible sources, such as rEFInd's fsw_efi (GPLv2 only)
  or Intel's FAT driver (requires an extra copyright notice).
  Also note that the EDK2 files from the include directory use a BSD 2-Clause
  license, which is compatible with the GPL.
