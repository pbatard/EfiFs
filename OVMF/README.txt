* OVMF-IA32.zip is a repackaged version of the OVMF found in:
  https://www.kraxel.org/repos/jenkins/edk2/edk2.git-ovmf-ia32-0-20200309.1367.gbd6aa93296.noarch.rpm
  under ./usr/share/edk2.git/ovmf-ia32/OVMF-pure-efi.fd

* OVMF-X64.zip is a repackaged version of the unmodified OVMF from Debian 11 downloaded from:
  http://ftp.ie.debian.org/debian/pool/main/e/edk2/ovmf_2020.11-2_all.deb
  and found under that package at ./usr/share/OVMF/OVMF.fd

* OVMF-AA64.zip is a repackaged version of the Linaro firmware image from:
  https://snapshots.linaro.org/components/kernel/leg-virt-tianocore-edk2-upstream/4194/QEMU-AARCH64/RELEASE_GCC5/QEMU_EFI.fd

* Because EBC support for ARM is not integrated into EDK2, OVMF-ARM.zip is a repackaging of
  a firmware that was custom produced with gcc 5 on a Debian 8 machine, from EDK2 @490acf89
  (https://github.com/tianocore/edk2/commit/490acf8908f797982f367bfeb4bdf3ebe0764e42) with
  MdeModulePkg-EbcDxe-add-ARM-support.patch applied, and using the instructions from
  https://wiki.linaro.org/LEG/UEFIforQEMU.

