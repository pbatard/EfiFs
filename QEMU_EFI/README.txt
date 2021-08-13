* Because EBC support for ARM is not yet integrated into EDK2, QEMU_EFI-ARM.zip is a
  repackaging of a QEMU_EFI.fd firmware produced with gcc 5.4 on a Debian Sid machine,
  from EDK2 custom branch: https://github.com/pbatard/edk2/tree/ebc-arm5 and using
  the instructions from https://wiki.linaro.org/LEG/UEFIforQEMU.

* QEMU_EFI-AA64.zip on the other hand is a repackaged version of the QEMU_EFI.fd Linaro
  firmware image downloaded from:
  http://snapshots.linaro.org/components/kernel/leg-virt-tianocore-edk2-upstream/1631/QEMU-AARCH64/RELEASE_GCC5/

* The NTFS versions are the same firmwares, but with the Fat driver replaced with a
  read-only NTFS driver. These are meant to be used for testing the EBC Fat module.
