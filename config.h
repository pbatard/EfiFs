/*
 * minimal GRUB config.h for efifs compilation
 */

#if defined(_M_X64) || defined(__x86_64__)
#define GRUB_TARGET_CPU "x86_64"
#elif defined(_M_IX86) || defined(__i386__)
#define GRUB_TARGET_CPU "i386"
#elif defined (_M_ARM) || defined(__arm__)
#define GRUB_TARGET_CPU "arm"
#else
#error Usupported architecture
#endif

#define _LARGEFILE_SOURCE
#define _FILE_OFFSET_BITS 64
#define PACKAGE_STRING "GRUB 2.0"

#define GRUB_PLATFORM "efi"
#define GRUB_MACHINE_EFI
#define GRUB_KERNEL

#if defined(__GNUC__)
#define _GNU_SOURCE 1
#define size_t grub_size_t
#endif

#if defined(_MSC_VER)
#define inline __inline
#define __attribute__(x)
#define __attribute(x)

// Silence warnings that are triggered by GNU extensions not being available
#pragma warning(disable: 4716)	// Some of the grub function calls are set not to return a value
#endif
