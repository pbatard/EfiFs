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
#include <efilink.h>

#pragma once

#if !defined(_MSC_VER)
#if !defined(__GNUC__) || (__GNUC__ < 4) || (__GNUC__ == 4 && __GNUC_MINOR__ < 7)
#error gcc 4.7 or later is required for the compilation of this driver.
#endif

/* Having GNU_EFI_USE_MS_ABI should avoid the need for that ugly uefi_call_wrapper */
#if !defined(__MAKEWITH_GNUEFI) || !defined(GNU_EFI_USE_MS_ABI)
#error gnu-efi, with option GNU_EFI_USE_MS_ABI, is required for the compilation of this driver.
#endif
#endif

/* Driver version */
#define FS_DRIVER_VERSION_MAJOR 0
#define FS_DRIVER_VERSION_MINOR 7
#define FS_DRIVER_VERSION_MICRO 0

#ifndef ARRAYSIZE
#define ARRAYSIZE(A)            (sizeof(A)/sizeof((A)[0]))
#endif

#ifndef MIN
#define MIN(x,y)                ((x)<(y)?(x):(y))
#endif

#define _STRINGIFY(s)           #s
#define STRINGIFY(s)            _STRINGIFY(s)

#define _WIDEN(s)               L ## s
#define WIDEN(s)                _WIDEN(s)

#define MAX_PATH 256
#define MINIMUM_INFO_LENGTH     (sizeof(EFI_FILE_INFO) + MAX_PATH * sizeof(CHAR16))
#define MINIMUM_FS_INFO_LENGTH  (sizeof(EFI_FILE_SYSTEM_INFO) + MAX_PATH * sizeof(CHAR16))
#define IS_ROOT(File)           (File == File->FileSystem->RootFile)

/* Logging */
#define FS_LOGLEVEL_NONE        0
#define FS_LOGLEVEL_ERROR       1
#define FS_LOGLEVEL_WARNING     2
#define FS_LOGLEVEL_INFO        3
#define FS_LOGLEVEL_DEBUG       4
#define FS_LOGLEVEL_EXTRA       5

typedef UINTN (*Print_t)        (IN CHAR16 *fmt, ... );
extern Print_t PrintError;
extern Print_t PrintWarning;
extern Print_t PrintInfo;
extern Print_t PrintDebug;
extern Print_t PrintExtra;

/* Forward declaration */
struct _EFI_FS;

/* A file instance */
typedef struct _EFI_GRUB_FILE {
	EFI_FILE               EfiFile;
	BOOLEAN                IsDir;
	INT64                  DirIndex;
	INT32                  Mtime;
	CHAR8                 *path;
	CHAR8                 *basename;
	INTN                   RefCount;
	VOID                  *GrubFile;
	struct _EFI_FS        *FileSystem;
} EFI_GRUB_FILE;

/* A file system instance */
typedef struct _EFI_FS {
	LIST_ENTRY            *Flink;
	LIST_ENTRY            *Blink;
	EFI_FILE_IO_INTERFACE  FileIoInterface;
	EFI_BLOCK_IO          *BlockIo;
	EFI_DISK_IO           *DiskIo;
	EFI_GRUB_FILE         *RootFile;
	VOID                  *GrubDevice;
	CHAR16                *DevicePathString;
} EFI_FS;

/* Mirrors a similar construct from GRUB, while EFI-zing it */
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
typedef VOID(*GRUB_MOD_INIT)(VOID);
typedef VOID(*GRUB_MOD_EXIT)(VOID);

extern UINTN LogLevel;
extern EFI_HANDLE EfiImageHandle;
extern EFI_GUID ShellVariable;
extern LIST_ENTRY FsListHead;
extern CHAR16 *ShortDriverName, *FullDriverName;
extern GRUB_MOD_INIT GrubModuleInit[];
extern GRUB_MOD_EXIT GrubModuleExit[];

extern VOID strcpya(CHAR8 *dst, CONST CHAR8 *src);
extern CHAR8 *strchra(const CHAR8 *s, INTN c);
extern CHAR8 *strrchra(const CHAR8 *s, INTN c);
extern VOID SetLogging(VOID);
extern VOID PrintStatusError(EFI_STATUS Status, const CHAR16 *Format, ...);
extern VOID GrubDriverInit(VOID);
extern VOID GrubDriverExit(VOID);
extern CHAR16 *GrubGetUuid(EFI_FS *This);
extern BOOLEAN GrubFSProbe(EFI_FS *This);
extern EFI_STATUS GrubDeviceInit(EFI_FS *This);
extern EFI_STATUS GrubDeviceExit(EFI_FS *This);
extern VOID GrubTimeToEfiTime(const INT32 t, EFI_TIME *tp);
extern VOID CopyPathRelative(CHAR8 *dest, CHAR8 *src, INTN len);
extern EFI_STATUS GrubOpen(EFI_GRUB_FILE *File);
extern EFI_STATUS GrubDir(EFI_GRUB_FILE *File, const CHAR8 *path,
		GRUB_DIRHOOK Hook, VOID *HookData);
extern VOID GrubClose(EFI_GRUB_FILE *File);
extern EFI_STATUS GrubRead(EFI_GRUB_FILE *File, VOID *Data, UINTN *Len);
extern EFI_STATUS GrubLabel(EFI_GRUB_FILE *File, CHAR8 **label);
extern EFI_STATUS GrubCreateFile(EFI_GRUB_FILE **File, EFI_FS *This);
extern VOID GrubDestroyFile(EFI_GRUB_FILE *File);
extern UINT64 GrubGetFileSize(EFI_GRUB_FILE *File);
extern UINT64 GrubGetFileOffset(EFI_GRUB_FILE *File);
extern VOID GrubSetFileOffset(EFI_GRUB_FILE *File, UINT64 Offset);
extern CHAR16 *Utf8ToUtf16Alloc(CHAR8 *src);
extern EFI_STATUS Utf8ToUtf16NoAlloc(CHAR8 *src, CHAR16 *Dst, UINTN Len);
extern CHAR8 *Utf16ToUtf8Alloc(CHAR16 *Src);
extern EFI_STATUS Utf16ToUtf8NoAlloc(CHAR16 *Src, CHAR8 *dst, UINTN len);
extern EFI_STATUS FSInstall(EFI_FS *This, EFI_HANDLE ControllerHandle);
extern VOID FSUninstall(EFI_FS *This, EFI_HANDLE ControllerHandle);
extern EFI_STATUS EFIAPI FileOpenVolume(EFI_FILE_IO_INTERFACE *This,
		EFI_FILE_HANDLE *Root);
extern EFI_STATUS EFIAPI FSDriverInstall(EFI_HANDLE ImageHandle,
		EFI_SYSTEM_TABLE* SystemTable);
