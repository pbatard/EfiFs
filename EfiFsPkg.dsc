## @file
#  EfiFs Driver Modules
#
#  Copyright (c) 2017-2019, Pete Batard <pete@akeo.ie>
#
##

[Defines]
  PLATFORM_NAME                  = EfiFs
  PLATFORM_GUID                  = 700d6096-1578-409e-a6c7-9acdf9f565b3
  PLATFORM_VERSION               = 1.3
  DSC_SPECIFICATION              = 0x00010005
  SUPPORTED_ARCHITECTURES        = IA32|X64|EBC|ARM|AARCH64|RISCV64|LOONGARCH64
  OUTPUT_DIRECTORY               = Build/EfiFs
  BUILD_TARGETS                  = DEBUG|RELEASE|NOOPT
  SKUID_IDENTIFIER               = DEFAULT

[BuildOptions]
  GCC:RELEASE_*_*_CC_FLAGS       = -DMDEPKG_NDEBUG
  INTEL:RELEASE_*_*_CC_FLAGS     = -DMDEPKG_NDEBUG
  MSFT:RELEASE_*_*_CC_FLAGS      = -DMDEPKG_NDEBUG
  RVCT:RELEASE_*_*_CC_FLAGS      = -DMDEPKG_NDEBUG
  *_*_*_CC_FLAGS                 = -DDISABLE_NEW_DEPRECATED_INTERFACES

!include MdePkg/MdeLibs.dsc.inc

[LibraryClasses]
  #
  # Entry Point Libraries
  #
  UefiDriverEntryPoint|MdePkg/Library/UefiDriverEntryPoint/UefiDriverEntryPoint.inf
  #
  # Common Libraries
  #
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  UefiLib|MdePkg/Library/UefiLib/UefiLib.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  DebugPrintErrorLevelLib|MdePkg/Library/BaseDebugPrintErrorLevelLib/BaseDebugPrintErrorLevelLib.inf  
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf

[LibraryClasses.ARM, LibraryClasses.AARCH64, LibraryClasses.RISCV64]
  NULL|ArmPkg/Library/CompilerIntrinsicsLib/CompilerIntrinsicsLib.inf
  NULL|MdePkg/Library/BaseStackCheckLib/BaseStackCheckLib.inf

[LibraryClasses.IA32, LibraryClasses.X64]
!if $(TOOLCHAIN) == "VS2019"
  NULL|EfiFsPkg/CompilerIntrinsicsLib/CompilerIntrinsicsLib.inf
!endif

###################################################################################################
#
# Components Section - list of the modules and components that will be processed by compilation
#                      tools and the EDK II tools to generate PE32/PE32+/Coff image files.
#
###################################################################################################

[Components]
  EfiFsPkg/EfiFsPkg/Afs.inf
  EfiFsPkg/EfiFsPkg/Affs.inf
  EfiFsPkg/EfiFsPkg/Bfs.inf
  EfiFsPkg/EfiFsPkg/Btrfs.inf
  EfiFsPkg/EfiFsPkg/Cbfs.inf
  EfiFsPkg/EfiFsPkg/Cpio.inf
  EfiFsPkg/EfiFsPkg/CpioBe.inf
  EfiFsPkg/EfiFsPkg/EroFs.inf
  EfiFsPkg/EfiFsPkg/Ext2.inf
  EfiFsPkg/EfiFsPkg/ExFat.inf
  EfiFsPkg/EfiFsPkg/F2fs.inf
  EfiFsPkg/EfiFsPkg/Fat.inf
  EfiFsPkg/EfiFsPkg/Hfs.inf
  EfiFsPkg/EfiFsPkg/HfsPlus.inf
  EfiFsPkg/EfiFsPkg/Iso9660.inf
  EfiFsPkg/EfiFsPkg/Jfs.inf
  EfiFsPkg/EfiFsPkg/Minix.inf
  EfiFsPkg/EfiFsPkg/MinixBe.inf
  EfiFsPkg/EfiFsPkg/Minix2.inf
  EfiFsPkg/EfiFsPkg/Minix2Be.inf
  EfiFsPkg/EfiFsPkg/Minix3.inf
  EfiFsPkg/EfiFsPkg/Minix3Be.inf
  EfiFsPkg/EfiFsPkg/NewC.inf
  EfiFsPkg/EfiFsPkg/NilFs2.inf
  EfiFsPkg/EfiFsPkg/Ntfs.inf
  EfiFsPkg/EfiFsPkg/Odc.inf
  EfiFsPkg/EfiFsPkg/ProcFs.inf
  EfiFsPkg/EfiFsPkg/ReiserFs.inf
  EfiFsPkg/EfiFsPkg/RomFs.inf
  EfiFsPkg/EfiFsPkg/Sfs.inf
  EfiFsPkg/EfiFsPkg/SquashFs.inf
  EfiFsPkg/EfiFsPkg/Tar.inf
  EfiFsPkg/EfiFsPkg/Udf.inf
  EfiFsPkg/EfiFsPkg/Ufs.inf
  EfiFsPkg/EfiFsPkg/UfsBe.inf
  EfiFsPkg/EfiFsPkg/Ufs2.inf
  EfiFsPkg/EfiFsPkg/Xfs.inf
  EfiFsPkg/EfiFsPkg/Zfs.inf
