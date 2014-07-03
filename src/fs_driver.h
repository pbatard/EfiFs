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
#include <grub/file.h>

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
#define FS_DRIVER_VERSION_MINOR 5

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
#define FS_LOGLEVEL_NONE        0
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

#define MAX_PATH 256

#define MINIMUM_INFO_LENGTH     (sizeof(EFI_FILE_INFO) + MAX_PATH * sizeof(CHAR16))
#define MINIMUM_FS_INFO_LENGTH  (sizeof(EFI_FILE_SYSTEM_INFO) + MAX_PATH * sizeof(CHAR16))

#ifndef MIN
#define MIN(x,y) ((x)<(y)?(x):(y))
#endif

#define _STRINGIFY(s) #s
#define STRINGIFY(s) _STRINGIFY(s)

#define _WIDEN(s) L ## s 
#define WIDEN(s) _WIDEN(s)

/* Forward declaration */
struct _EFI_FS;

/* A file instance */
typedef struct _EFI_GRUB_FILE {
	BOOLEAN                IsDir;
	INT32                  Mtime;
	// TODO: have a copy of grub_file->name here, as a CHAR8*
	char                  *basename;
	INTN                   RefCount;
	EFI_FILE               EfiFile;
	// TODO: use a VOID *GrubFile here
	struct grub_file       grub_file;
	struct _EFI_FS        *FileSystem;
} EFI_GRUB_FILE;

/* A file system instance */
typedef struct _EFI_FS {
	EFI_FILE_IO_INTERFACE FileIOInterface;
	EFI_DISK_IO           *DiskIo;
	EFI_BLOCK_IO          *BlockIo;
	CHAR16                 DevicePath[MAX_PATH];
	VOID                  *GrubDevice;
	EFI_GRUB_FILE          RootFile;
} EFI_FS;

/* Mirror similar constructs from GRUB, using an EFI sauce */
typedef struct _GRUB_DIRHOOK_INFO {
	UINT32                 Dir:1;
	UINT32                 MtimeSet:1;
	UINT32                 CaseInsensitive:1;
	UINT32                 InodeSet:1;
	INT32                  Mtime;
	UINT64                 Inode;
} GRUB_DIRHOOK_INFO;

typedef INT32 (*GRUB_DIRHOOK) (const CHAR8 *name,
		const GRUB_DIRHOOK_INFO *Info, VOID *Data);

/* Setup generic function calls for grub_<fs>_init and grub_<fs>_exit */
#define MAKE_FN_NAME(drivername, suffix) grub_ ## drivername ## _ ## suffix
#define GRUB_FS_CALL(drivername, suffix) MAKE_FN_NAME(drivername, suffix)
extern void GRUB_FS_CALL(DRIVERNAME, init)(void);
extern void GRUB_FS_CALL(DRIVERNAME, fini)(void);
#if defined(EXTRAMODULE)
extern void GRUB_FS_CALL(EXTRAMODULE, init)(void);
extern void GRUB_FS_CALL(EXTRAMODULE, fini)(void);
#endif

extern INTN LogLevel;
extern EFI_HANDLE EfiImageHandle;
extern EFI_GUID ShellVariable;

extern CHAR16 *GrubGetUUID(EFI_FS *This);
extern BOOLEAN GrubFSProbe(EFI_FS *This);
extern EFI_STATUS GrubDeviceInit(EFI_FS *This);
extern EFI_STATUS GrubDeviceExit(EFI_FS *This);
extern VOID GrubTimeToEfiTime(const INT32 t, EFI_TIME *tp);
extern VOID copy_path_relative(char *dest, char *src, INTN len);
extern EFI_STATUS GrubOpen(EFI_GRUB_FILE *File);
extern EFI_STATUS GrubDir(EFI_GRUB_FILE *File, const CHAR8 *path,
		GRUB_DIRHOOK Hook, VOID *HookData);
extern VOID GrubClose(EFI_GRUB_FILE *File);
extern EFI_STATUS GrubRead(EFI_GRUB_FILE *File, VOID *Data, UINTN *Len);
extern EFI_STATUS GrubLabel(EFI_GRUB_FILE *File, CHAR8 **label);
