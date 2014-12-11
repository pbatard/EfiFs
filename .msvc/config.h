/*
 * minimal GRUB config.h for Visual Studio compilation
 */
#define _LARGEFILE_SOURCE
#define _FILE_OFFSET_BITS 64
#define PACKAGE_STRING "GRUB 2.02~beta2"
#define GRUB_TARGET_CPU "x86_64"
#define GRUB_PLATFORM "efi"
#define GRUB_MACHINE_EFI
#define GRUB_KERNEL

// GNU extensions
#define inline __inline
#define __attribute__(x)

// Silence warnings that are triggered by GNU extensions not being available
#pragma warning(disable: 4716)	// Some of the grub function calls are set not to return a value
