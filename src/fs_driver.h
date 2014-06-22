/* fs_driver.h - Wrapper for standalone EFI filesystem drivers */
/*
 *  Copyright © 2014 Pete Batard <pete@akeo.ie>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <efi.h>

#pragma once

#if !defined(__GNUC__) || (__GNUC__ < 4) || (__GNUC__ == 4 && __GNUC_MINOR__ < 7)
#error gcc 4.7 or later is required for the compilation of this driver.
#endif

/* Having GNU_EFI_USE_MS_ABI should avoid the need for that ugly uefi_call_wrapper */
#if !defined(__MAKEWITH_GNUEFI) || !defined(GNU_EFI_USE_MS_ABI)
#error gnu-efi, with option GNU_EFI_USE_MS_ABI, is required for the compilation of this driver.
#endif

/* Driver version */
#define FS_DRIVER_VERSION_MAJOR 0
#define FS_DRIVER_VERSION_MINOR 3

#undef offsetof
#if defined(__GNUC__) && (__GNUC__ > 3)
#define offsetof(TYPE, MEMBER) __builtin_offsetof(TYPE, MEMBER)
#else
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif

#undef container_of
#define container_of(ptr, type, member) ({                  \
	const typeof( ((type *)0)->member ) *__mptr = (ptr);    \
	(type *)( (char *)__mptr - offsetof(type,member) );})

#ifndef ARRAYSIZE
#define ARRAYSIZE(A)     (sizeof(A)/sizeof((A)[0]))
#endif

/* Logging */
#define FS_LOGLEVEL_SILENT      0
#define FS_LOGLEVEL_ERROR       1
#define FS_LOGLEVEL_WARNING     2
#define FS_LOGLEVEL_INFO        3
#define FS_LOGLEVEL_DEBUG       4
#define FS_LOGLEVEL_EXTRA       5

typedef UINTN (*Print_t) (IN CHAR16 *fmt, ... );
extern Print_t PrintError;
extern Print_t PrintWarning;
extern Print_t PrintInfo;
extern Print_t PrintDebug;
extern Print_t PrintExtra;
extern void PrintStatusError(EFI_STATUS Status, const CHAR16 *Format, ...);

/* Set a GUID for this filesystem - Used for our global mutex among other things */
// TODO: we'll need a separate GUID for each filesystem we want to serve
#define THIS_FS_GUID { \
	0x3AD33E69, 0x7966, 0x4081, {0x9A, 0x66, 0x9B, 0xA8, 0xE5, 0x4E, 0x06, 0x4B } \
}

/* A file system instance */
typedef struct {
	EFI_FILE_IO_INTERFACE FileIOInterface;
	EFI_DISK_IO *DiskIo;
	EFI_BLOCK_IO *BlockIo;
	CHAR16 Path[256];
	VOID *GrubDevice;
} EFI_FS;

extern void GRUB_FS_INIT(void);
extern void GRUB_FS_FINI(void);

extern EFI_HANDLE EfiImageHandle;
extern EFI_GUID ShellVariable;

extern CHAR16 *GrubGetUUID(EFI_FS *This);
extern BOOLEAN GrubFSProbe(EFI_FS *This);
extern EFI_STATUS GrubDeviceInit(EFI_FS *This);
extern EFI_STATUS GrubDeviceExit(EFI_FS *This);
