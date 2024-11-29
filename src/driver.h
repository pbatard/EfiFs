/* fs_driver.h - Wrapper for standalone EFI filesystem drivers */
/*
 *  Copyright © 2014-2020 Pete Batard <pete@akeo.ie>
 *
 *  Note: This file has been relicensed from GPLv3+ to GPLv2+ by formal
 *  agreement of all of its contributors.
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
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

#if defined(__MAKEWITH_GNUEFI)
#include <efi.h>
#include <efilib.h>
#include <efidebug.h>	/* ASSERT */
#else
#include <Base.h>
#include <Uefi.h>

#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/DevicePathLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiLib.h>

#include <Protocol/UnicodeCollation.h>
#include <Protocol/LoadedImage.h>
#include <Protocol/DriverBinding.h>
#include <Protocol/DevicePathFromText.h>
#include <Protocol/DevicePathToText.h>
#include <Protocol/DebugPort.h>
#include <Protocol/DebugSupport.h>
#include <Protocol/SimpleFileSystem.h>
#include <Protocol/BlockIo.h>
#include <Protocol/BlockIo2.h>
#include <Protocol/DiskIo.h>
#include <Protocol/DiskIo2.h>
#include <Protocol/ComponentName.h>
#include <Protocol/ComponentName2.h>

#include <Guid/FileSystemInfo.h>
#include <Guid/FileInfo.h>
#include <Guid/FileSystemVolumeLabelInfo.h>
#include <Guid/ShellVariableGuid.h>
#endif

#pragma once

#if !defined(_MSC_VER)
#if !defined(__GNUC__) || (__GNUC__ < 4) || (__GNUC__ == 4 && __GNUC_MINOR__ < 7)
#error gcc 4.7 or later is required for the compilation of this driver.
#endif

/* Having GNU_EFI_USE_MS_ABI should avoid the need for that ugly uefi_call_wrapper */
#if defined(_GNU_EFI) && !defined(GNU_EFI_USE_MS_ABI)
#error gnu-efi, with option GNU_EFI_USE_MS_ABI, is required for the compilation of this driver.
#endif
#endif

/* Sort out the platform specifics */
#if defined(_M_ARM64) || defined(__aarch64__) || defined (_M_X64) || defined(__x86_64__) || defined(_M_RISCV64) || (defined (__riscv) && (__riscv_xlen == 64)) || defined (_M_LOONGARCH64) || defined (__loongarch__)
#define PERCENT_P L"%llx"
#else
#define PERCENT_P L"%x"
#endif

/* Sort out the differences between EDK2 and gnu-efi */
#if defined(_GNU_EFI)
#define STUPID_CLANG_REF(a,b) a.b
#define FORWARD_LINK_REF(list) STUPID_CLANG_REF(list,Flink)
#else
#define STUPID_CLANG_REF(a,b) a.b
#define FORWARD_LINK_REF(list) STUPID_CLANG_REF(list, ForwardLink)
#define Atoi (INTN)StrDecimalToUintn
#define APrint AsciiPrint
#define strlena AsciiStrLen
#define strcmpa AsciiStrCmp
#define BS gBS
#define RT gRT
#define ST gST
#define _CR BASE_CR
#define PROTO_NAME(x) gEfi ## x ## Guid
#define GUID_NAME(x) gEfi ## x ## Guid
#define SIZE_OF_EFI_FILE_SYSTEM_VOLUME_LABEL_INFO SIZE_OF_EFI_FILE_SYSTEM_VOLUME_LABEL
#define EFI_FILE_SYSTEM_VOLUME_LABEL_INFO EFI_FILE_SYSTEM_VOLUME_LABEL
#define EFI_SIGNATURE_32(a,b,c,d) SIGNATURE_32(a,b,c,d)
#endif

/* Driver version */
#define FS_DRIVER_VERSION_MAJOR 1
#define FS_DRIVER_VERSION_MINOR 10

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

#define MAX_FILE_NAME_LEN       256
#define MINIMUM_INFO_LENGTH     (SIZE_OF_EFI_FILE_INFO + MAX_FILE_NAME_LEN * sizeof(CHAR16))
#define MINIMUM_FS_INFO_LENGTH  (SIZE_OF_EFI_FILE_SYSTEM_INFO + MAX_FILE_NAME_LEN * sizeof(CHAR16))
#define IS_ROOT(File)           (File == File->FileSystem->RootFile)

/* Logging */
#define FS_LOGLEVEL_NONE        0
#define FS_LOGLEVEL_ERROR       1
#define FS_LOGLEVEL_WARNING     2
#define FS_LOGLEVEL_INFO        3
#define FS_LOGLEVEL_DEBUG       4
#define FS_LOGLEVEL_EXTRA       5

typedef UINTN (EFIAPI *Print_t)        (IN CONST CHAR16 *fmt, ... );
extern Print_t PrintError;
extern Print_t PrintWarning;
extern Print_t PrintInfo;
extern Print_t PrintDebug;
extern Print_t PrintExtra;

#define FS_ASSERT(a)  if(!(a)) do { Print(L"*** ASSERT FAILED: %a(%d): %a ***\n", __FILE__, __LINE__, #a); while(1); } while(0)

/**
 * Print an error message along with a human readable EFI status code
 *
 * @v Status		EFI status code
 * @v Format		A non '\n' terminated error message string
 * @v ...			Any extra parameters
 */
#define PrintStatusError(Status, Format, ...) \
	if (LogLevel >= FS_LOGLEVEL_ERROR) { \
		Print(Format, ##__VA_ARGS__); PrintStatus(Status); }


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
	LIST_ENTRY                      *Flink;
	LIST_ENTRY                      *Blink;
	EFI_SIMPLE_FILE_SYSTEM_PROTOCOL FileIoInterface;
	EFI_BLOCK_IO_PROTOCOL           *BlockIo;
	EFI_BLOCK_IO2_PROTOCOL          *BlockIo2;
	EFI_BLOCK_IO2_TOKEN             BlockIo2Token;
	EFI_DEVICE_PATH                 *DevicePath;
	EFI_DISK_IO_PROTOCOL            *DiskIo;
	EFI_DISK_IO2_PROTOCOL           *DiskIo2;
	EFI_DISK_IO2_TOKEN              DiskIo2Token;
	EFI_GRUB_FILE                   *RootFile;
	VOID                            *GrubDevice;
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

#define strcpya(dst, src) CopyMem((VOID*)dst, (VOID*)src, strlena(src) + 1)
extern VOID SetLogging(VOID);
extern VOID PrintStatus(EFI_STATUS Status);
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
extern EFI_STATUS Utf8ToUtf16NoAllocUpdateLen(CHAR8 *src, CHAR16 *Dst, UINTN *Len);
extern CHAR8 *Utf16ToUtf8Alloc(CHAR16 *Src);
extern EFI_STATUS Utf16ToUtf8NoAlloc(CHAR16 *Src, CHAR8 *dst, UINTN len);
extern EFI_STATUS Utf16ToUtf8NoAllocUpdateLen(CHAR16 *Src, CHAR8 *dst, UINTN *len);
extern EFI_STATUS FSInstall(EFI_FS *This, EFI_HANDLE ControllerHandle);
extern VOID FSUninstall(EFI_FS *This, EFI_HANDLE ControllerHandle);
extern EFI_STATUS EFIAPI FileOpenVolume(EFI_SIMPLE_FILE_SYSTEM_PROTOCOL *This,
		EFI_FILE_HANDLE *Root);
extern EFI_GUID *GetFSGuid(VOID);
extern EFI_STATUS PrintGuid (EFI_GUID *Guid);
extern INTN CompareDevicePaths(CONST EFI_DEVICE_PATH* dp1, CONST EFI_DEVICE_PATH* dp2);
extern CHAR16* StrDup(CONST CHAR16* Src);
extern CHAR16* ToDevicePathString(CONST EFI_DEVICE_PATH* DevicePath);
extern EFI_STATUS EFIAPI FSDriverInstall(EFI_HANDLE ImageHandle,
		EFI_SYSTEM_TABLE* SystemTable);
