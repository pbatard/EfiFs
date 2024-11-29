/*
 * minimal GRUB config.h for efifs compilation
 */

#if defined(_MSC_VER)
#pragma warning(disable: 4146)	// "unary minus operator applied to unsigned type, result still unsigned"
#pragma warning(disable: 4244)	// "Conversion from X to Y, possible loss of data"
#pragma warning(disable: 4267)	// "Conversion from X to Y, possible loss of data"
#pragma warning(disable: 4334)	// "Result of 32-bit shift implicitly converted to 64 bits"
#endif

#if defined(_M_X64) || defined(__x86_64__)
#define GRUB_TARGET_CPU "x86_64"
#elif defined(_M_IX86) || defined(__i386__)
#define GRUB_TARGET_CPU "i386"
#elif defined (_M_ARM) || defined(__arm__)
#define GRUB_TARGET_CPU "arm"
#elif defined (_M_ARM64) || defined(__aarch64__)
#define GRUB_TARGET_CPU "arm64"
#elif defined (_M_RISCV64) || (defined (__riscv) && (__riscv_xlen == 64))
#define GRUB_TARGET_CPU "riscv64"
#elif defined (_M_LOONGARCH64) || defined (__loongarch__)
#define GRUB_TARGET_CPU "loongarch64"
#else
#error Usupported architecture
#endif

#if !defined(_LARGEFILE_SOURCE)
#define _LARGEFILE_SOURCE
#endif
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

#if !defined(__CHAR_BIT__)
#define __CHAR_BIT__ 8
#endif
