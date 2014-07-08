/* fs_driver.c - Wrapper for standalone EFI filesystem drivers */
/*
 *  Copyright © 2014 Pete Batard <pete@akeo.ie>
 *  Based on iPXE's efi_driver.c and efi_file.c:
 *  Copyright © 2011,2013 Michael Brown <mbrown@fensystems.co.uk>.
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
#include <efilib.h>
#include <efistdarg.h>
#include <edk2/DriverBinding.h>
#include <edk2/ComponentName.h>
#include <edk2/ComponentName2.h>
#include <edk2/ShellVariableGuid.h>

// TODO: split this file between Driver and SimpleFileIO protocol calls

#include "fs_driver.h"
#include "fs_guid.h"

/* These ones are not defined in gnu-efi yet */
EFI_GUID DriverBindingProtocol = EFI_DRIVER_BINDING_PROTOCOL_GUID;
EFI_GUID ComponentNameProtocol = EFI_COMPONENT_NAME_PROTOCOL_GUID;
EFI_GUID ComponentName2Protocol = EFI_COMPONENT_NAME2_PROTOCOL_GUID;
EFI_GUID ShellVariable = SHELL_VARIABLE_GUID;

/* We'll try to instantiate a custom protocol as a mutex, so we need a GUID */
EFI_GUID *MutexGUID = NULL;

/* Keep a global copy of our ImageHanle */
EFI_HANDLE EfiImageHandle = NULL;

/* Logging */
static UINTN PrintNone(IN CHAR16 *fmt, ... ) { return 0; }
Print_t PrintError = PrintNone;
Print_t PrintWarning = PrintNone;
Print_t PrintInfo = PrintNone;
Print_t PrintDebug = PrintNone;
Print_t PrintExtra = PrintNone;
Print_t* PrintTable[] = { &PrintError, &PrintWarning, &PrintInfo,
		&PrintDebug, &PrintExtra };

/* Global driver verbosity level */
#if !defined(DEFAULT_LOGLEVEL)
#define DEFAULT_LOGLEVEL FS_LOGLEVEL_NONE
#endif
INTN LogLevel = DEFAULT_LOGLEVEL;

/* Handle for our custom protocol/mutex instance */
static EFI_HANDLE MutexHandle = NULL;

/* Custom protocol/mutex definition */
typedef struct {
	INTN Unused;
} EFI_MUTEX_PROTOCOL;
static EFI_MUTEX_PROTOCOL MutexProtocol = { 0 };

// TODO: move this into a separate header/source
static __inline VOID
strcpya(IN CHAR8 *dst, IN CONST CHAR8 *src)
{
	INTN len = strlena(src) + 1;
	CopyMem(dst, src, len);
}

/**
 * Print an error message along with a human readable EFI status code
 *
 * @v Status		EFI status code
 * @v Format		A non '\n' terminated error message string
 * @v ...			Any extra parameters
 */
VOID
PrintStatusError(EFI_STATUS Status, const CHAR16 *Format, ...)
{
	CHAR16 StatusString[64];
	va_list ap;

	if (LogLevel < FS_LOGLEVEL_ERROR)
		return;

	StatusToString(StatusString, Status);
	va_start(ap, Format);
	VPrint((CHAR16 *)Format, ap);
	va_end(ap);
	Print(L": [%d] %s\n", Status, StatusString); 
}

/**
 * Get EFI file name (for debugging)
 *
 * @v file			EFI file
 * @ret Name		Name
 */
static const CHAR16 
*FileName(EFI_GRUB_FILE *File)
{
	EFI_STATUS Status;
	static CHAR16 Path[MAX_PATH];

	Status = Utf8ToUtf16NoAlloc(File->path, Path, sizeof(Path));
	if (EFI_ERROR(Status)) {
		PrintStatusError(Status, L"Could not convert filename to UTF16");
		return NULL;
	}

	return Path;
}

/* Simple hook to populate the timestamp and directory flag when opening a file */
static INT32
InfoHook(const CHAR8 *name, const GRUB_DIRHOOK_INFO *Info, VOID *Data)
{
	EFI_GRUB_FILE *File = (EFI_GRUB_FILE *) Data;

	/* Look for a specific file */
	if (strcmpa(name, File->basename) != 0)
		return 0;

	File->IsDir = (Info->Dir);
	if (Info->MtimeSet)
		File->Mtime = Info->Mtime;

	return 0;
}

/**
 * Open file
 *
 * @v This			File handle
 * @ret new			New file handle
 * @v Name			File name
 * @v Mode			File mode
 * @v Attributes	File attributes (for newly-created files)
 * @ret Status		EFI status code
 */
static EFI_STATUS EFIAPI
FileOpen(EFI_FILE_HANDLE This, EFI_FILE_HANDLE *New,
		CHAR16 *Name, UINT64 Mode, UINT64 Attributes)
{
	EFI_STATUS Status;
	EFI_GRUB_FILE *File = _CR(This, EFI_GRUB_FILE, EfiFile);
	EFI_GRUB_FILE *NewFile;

	// TODO: Use dynamic buffers?
	char path[MAX_PATH], clean_path[MAX_PATH], *dirname;
	INTN i, len;
	BOOLEAN AbsolutePath = (*Name == L'\\');

	PrintInfo(L"Open(%llx%s, \"%s\")\n", (UINT64) This,
			IS_ROOT(File)?L" <ROOT>":L"", Name);

	/* Fail unless opening read-only */
	if (Mode != EFI_FILE_MODE_READ) {
		PrintWarning(L"File '%s' can only be opened in read-only mode\n", Name);
		return EFI_WRITE_PROTECTED;
	}

	/* Additional failures */
	if ((StrCmp(Name, L"..") == 0) && IS_ROOT(File)) {
		PrintInfo(L"Trying to open <ROOT>'s parent\n");
		return EFI_NOT_FOUND;
	}

	/* See if we're trying to reopen current (which the EFI Shell insists on doing) */
	if ((*Name == 0) || (StrCmp(Name, L".") == 0)) {
		PrintInfo(L"  Reopening %s\n", IS_ROOT(File)?L"<ROOT>":FileName(File));
		File->RefCount++;
		*New = This;
		PrintInfo(L"  RET: %llx\n", (UINT64) *New);
		return EFI_SUCCESS;
	}

	/* If we have an absolute path, don't bother completing with the parent */
	if (AbsolutePath) {
		len = 0;
	} else {
		strcpya(path, File->path);
		len = strlena(path);
		/* Add delimiter if needed */
		if ((len == 0) || (path[len-1] != '/'))
			path[len++] = '/';
	}

	/* Copy the rest of the path (converted to UTF-8) */
	Status = Utf16ToUtf8NoAlloc(Name, &path[len], sizeof(path) - len);
	if (EFI_ERROR(Status)) {
		PrintStatusError(Status, L"Could not convert path to UTF-8");
		return Status;
	}
	/* Convert the delimiters */
	for (i = strlena(path) - 1 ; i >= len; i--) {
		if (path[i] == '\\')
			path[i] = '/';
	}

	/* We only want to handle with absolute paths */
	clean_path[0] = '/';
	/* Find out if we're dealing with root by removing the junk */
	copy_path_relative(&clean_path[1], path, MAX_PATH - 1);
	if (clean_path[1] == 0) {
		/* We're dealing with the root */
		PrintInfo(L"  Reopening <ROOT>\n");
		*New = &File->FileSystem->RootFile->EfiFile;
		PrintInfo(L"  RET: %llx\n", (UINT64) *New);
		return EFI_SUCCESS;
	}

	// TODO: eventually we should seek for already opened files and increase RefCount */
	/* Allocate and initialise an instance of a file */
	Status = GrubCreateFile(&NewFile, File->FileSystem);
	if (EFI_ERROR(Status)) {
		PrintStatusError(Status, L"Could not instantiate file");
		return Status;
	}

	NewFile->path = AllocatePool(strlena(clean_path)+1);
	if (NewFile->path == NULL) {
		GrubDestroyFile(NewFile);
		PrintError(L"Could not instantiate path\n");
		return EFI_OUT_OF_RESOURCES;
	}
	strcpya(NewFile->path, clean_path);

	/* Isolate the basename and dirname */
	for (i = strlena(clean_path) - 1; i >= 0; i--) {
		if (clean_path[i] == '/') {
			clean_path[i] = 0;
			break;
		}
	}
	dirname = (i <= 0) ? "/" : clean_path;
	NewFile->basename = &NewFile->path[i+1];

	/* Find if we're working with a directory and fill the grub timestamp */
	Status = GrubDir(NewFile, dirname, InfoHook, (VOID *) NewFile);
	if (EFI_ERROR(Status)) {
		PrintStatusError(Status, L"Could not get file attributes for '%s'", Name);
		FreePool(NewFile->path);
		GrubDestroyFile(NewFile);
		return EFI_NOT_FOUND;
	}

	/* Finally we can call on GRUB open() if it's a regular file */
	if (!NewFile->IsDir) {
		Status = GrubOpen(NewFile);
		if (EFI_ERROR(Status)) {
			PrintStatusError(Status, L"Could not open file '%s'", Name);
			FreePool(NewFile->path);
			GrubDestroyFile(NewFile);
			return EFI_NOT_FOUND;
		}
	}

	NewFile->RefCount++;
	*New = &NewFile->EfiFile;

	PrintInfo(L"  RET: %llx\n", (UINT64) *New);
	return EFI_SUCCESS;
}

/**
 * Close file
 *
 * @v This			File handle
 * @ret Status		EFI status code
 */
static EFI_STATUS EFIAPI
FileClose(EFI_FILE_HANDLE This)
{
	EFI_GRUB_FILE *File = _CR(This, EFI_GRUB_FILE, EfiFile);

	PrintInfo(L"Close(%llx|'%s') %s\n", (UINT64) This, FileName(File),
		IS_ROOT(File)?L"<ROOT>":L"");

	/* Nothing to do it this is the root */
	if (IS_ROOT(File))
		return EFI_SUCCESS;

	if (--File->RefCount == 0) {
		/* Close the file if it's a regular one */
		if (!File->IsDir)
			GrubClose(File);
		/* NB: basename points into File->path and does not need to be freed */
		FreePool(File->path);
		GrubDestroyFile(File);
	}

	return EFI_SUCCESS;
}

/**
 * Close and delete file
 *
 * @v This			File handle
 * @ret Status		EFI status code
 */
static EFI_STATUS EFIAPI
FileDelete(EFI_FILE_HANDLE This)
{
	EFI_GRUB_FILE *File = _CR(This, EFI_GRUB_FILE, EfiFile);

	PrintError(L"Cannot delete '%s'\n", FileName(File));

	/* Close file */
	FileClose(This);

	/* Warn of failure to delete */
	return EFI_WARN_DELETE_FAILURE;
}

/* GRUB uses a callback for each directory entry, whereas EFI uses repeated
 * firmware generated calls to FileReadDir() to get the info for each entry,
 * so we have to reconcile the twos. For now, we'll re-issue a call to GRUB
 * dir(), and run through all the entries (to find the one we
 * are interested in)  multiple times. Maybe later we'll try to optimize this
 * by building a one-off chained list of entries that we can parse...
 */
static INT32
DirHook(const CHAR8 *name, const GRUB_DIRHOOK_INFO *DirInfo, VOID *Data)
{
	EFI_STATUS Status;
	EFI_FILE_INFO *Info = (EFI_FILE_INFO *) Data;
	INT64 *Index = (INT64 *) &Info->FileSize;
	CHAR8 *filename = (CHAR8 *) Info->PhysicalSize;
	EFI_TIME Time = { 1970, 01, 01, 00, 00, 00, 0, 0, 0, 0, 0};

	// Eliminate '.' or '..'
	if ((name[0] ==  '.') && ((name[1] == 0) || ((name[1] == '.') && (name[2] == 0))))
		return 0;

	/* Ignore any entry that doesn't match our index */
	if ((*Index)-- != 0)
		return 0;

	strcpya(filename, name);

	Status = Utf8ToUtf16NoAlloc(filename, Info->FileName, Info->Size - sizeof(EFI_FILE_INFO));
	if (EFI_ERROR(Status)) {
		if (Status != EFI_BUFFER_TOO_SMALL)
			PrintStatusError(Status, L"Could not convert directory entry to UTF-8");
		return Status;
	}
	/* The Info struct size already accounts for the extra NUL */
	Info->Size = sizeof(*Info) + StrLen(Info->FileName) * sizeof(CHAR16);

	// Oh, and of course GRUB uses a 32 bit signed mtime value (seriously, wtf guys?!?)
	if (DirInfo->MtimeSet)
		GrubTimeToEfiTime(DirInfo->Mtime, &Time);
	CopyMem(&Info->CreateTime, &Time, sizeof(Time));
	CopyMem(&Info->LastAccessTime, &Time, sizeof(Time));
	CopyMem(&Info->ModificationTime, &Time, sizeof(Time));

	Info->Attribute = EFI_FILE_READ_ONLY;
	if (DirInfo->Dir)
		Info->Attribute |= EFI_FILE_DIRECTORY;

	return 0;
}

/**
 * Read directory entry
 *
 * @v file			EFI file
 * @v Len			Length to read
 * @v Data			Data buffer
 * @ret Status		EFI status code
 */
static EFI_STATUS
FileReadDir(EFI_GRUB_FILE *File, UINTN *Len, VOID *Data)
{
	EFI_FILE_INFO *Info = (EFI_FILE_INFO *) Data;
	EFI_STATUS Status;
	/* We temporarily repurpose the FileSize as a *signed* entry index */
	INT64 *Index = (INT64 *) &Info->FileSize;
	/* And PhysicalSize as a pointer to our filename */
	CHAR8 **basename = (CHAR8 **) &Info->PhysicalSize;
	CHAR8 path[MAX_PATH];
	EFI_GRUB_FILE *TmpFile = NULL;
	INTN len;

	/* Unless we can fit MAX_PATH chars, forget it */
	if (*Len < MINIMUM_INFO_LENGTH) {
		*Len = MINIMUM_INFO_LENGTH;
		return EFI_BUFFER_TOO_SMALL;
	}

	/* Populate our Info template */
	ZeroMem(Data, *Len);
	Info->Size = *Len;
	*Index = File->DirIndex;
	strcpya(path, File->path);
	len = strlena(path);
	if (path[len-1] != '/')
		path[len++] = '/';
	*basename = &path[len];

	/* Invoke GRUB's directory listing */
	Status = GrubDir(File, File->path, DirHook, Data);
	if (*Index >= 0) {
		/* No more entries */
		*Len = 0;
		return EFI_SUCCESS;
	}

	if (EFI_ERROR(Status)) {
		PrintStatusError(Status, L"Directory listing failed");
		return Status;
	}

	/* Our Index/FileSize must be reset */
	Info->FileSize = 0;
	Info->PhysicalSize = 0;

	/* For regular files, we still need to fill the size */
	if (!(Info->Attribute & EFI_FILE_DIRECTORY)) {
		/* Open the file and read its size */
		Status = GrubCreateFile(&TmpFile, File->FileSystem);
		if (EFI_ERROR(Status)) {
			PrintStatusError(Status, L"Unable to create temporary file");
			return Status;
		}
		TmpFile->path = path;

		Status = GrubOpen(TmpFile);
		if (EFI_ERROR(Status)) {
			// TODO: EFI_NO_MAPPING is returned for links...
			PrintStatusError(Status, L"Unable to obtain the size of '%s'", Info->FileName);
			/* Non fatal error */
		} else {
			Info->FileSize = GrubGetFileSize(TmpFile);
			Info->PhysicalSize = GrubGetFileSize(TmpFile);
			GrubClose(TmpFile);
		}
		GrubDestroyFile(TmpFile);
	}

	*Len = (UINTN) Info->Size;
	/* Advance to the next entry */
	File->DirIndex++;

//	PrintInfo(L"  Entry[%d]: '%s' %s\n", File->DirIndex-1, Info->FileName,
//			(Info->Attribute&EFI_FILE_DIRECTORY)?L"<DIR>":L"");

	return EFI_SUCCESS;
}

/**
 * Read from file
 *
 * @v This			File handle
 * @v Len			Length to read
 * @v Data			Data buffer
 * @ret Status		EFI status code
 */
static EFI_STATUS EFIAPI
FileRead(EFI_FILE_HANDLE This, UINTN *Len, VOID *Data)
{
	EFI_GRUB_FILE *File = _CR(This, EFI_GRUB_FILE, EfiFile);

	PrintInfo(L"Read(%llx|'%s', %d) %s\n", This, FileName(File),
			*Len, File->IsDir?L"<DIR>":L"");

	/* If this is a directory, then fetch the directory entries */
	if (File->IsDir)
		return FileReadDir(File, Len, Data);

	return GrubRead(File, Data, Len);
}

/**
 * Write to file
 *
 * @v This			File handle
 * @v Len			Length to write
 * @v Data			Data buffer
 * @ret Status		EFI status code
 */
static EFI_STATUS EFIAPI
FileWrite(EFI_FILE_HANDLE This, UINTN *Len, VOID *Data)
{
	EFI_GRUB_FILE *File = _CR(This, EFI_GRUB_FILE, EfiFile);

	PrintError(L"Cannot write to '%s'\n", FileName(File));
	return EFI_WRITE_PROTECTED;
}

/**
 * Set file position
 *
 * @v This			File handle
 * @v Position		New file position
 * @ret Status		EFI status code
 */
static EFI_STATUS EFIAPI
FileSetPosition(EFI_FILE_HANDLE This, UINT64 Position)
{
	EFI_GRUB_FILE *File = _CR(This, EFI_GRUB_FILE, EfiFile);
	UINT64 FileSize;

	PrintInfo(L"SetPosition(%llx|'%s', %lld) %s\n", (UINT64) This,
		FileName(File), Position, (File->IsDir)?L"<DIR>":L"");

	/* If this is a directory, reset the Index to the start */
	if (File->IsDir) {
		if (Position != 0)
			return EFI_INVALID_PARAMETER;
		File->DirIndex = 0;
		return EFI_SUCCESS;
	}

	/* Fail if we attempt to seek past the end of the file (since
	 * we do not support writes).
	 */
	FileSize = GrubGetFileSize(File);
	if (Position > FileSize) {
		PrintError(L"'%s': Cannot seek to %#llx of %llx\n",
				FileName(File), Position, FileSize);
		return EFI_UNSUPPORTED;
	}

	/* Set position */
	GrubSetFileOffset(File, Position);
	PrintDebug(L"'%s': Position set to %llx\n",
			FileName(File), Position);

	return EFI_SUCCESS;
}

/**
 * Get file position
 *
 * @v This			File handle
 * @ret Position	New file position
 * @ret Status		EFI status code
 */
static EFI_STATUS EFIAPI
FileGetPosition(EFI_FILE_HANDLE This, UINT64 *Position)
{
	EFI_GRUB_FILE *File = _CR(This, EFI_GRUB_FILE, EfiFile);

	PrintInfo(L"GetPosition(%llx|'%s', %lld)\n", This, FileName(File));

	if (File->IsDir)
		*Position = File->DirIndex;
	else
		*Position = GrubGetFileOffset(File);
	return EFI_SUCCESS;
}

/**
 * Get file information
 *
 * @v This			File handle
 * @v Type			Type of information
 * @v Len			Buffer size
 * @v Data			Buffer
 * @ret Status		EFI status code
 */
static EFI_STATUS EFIAPI
FileGetInfo(EFI_FILE_HANDLE This, EFI_GUID *Type, UINTN *Len, VOID *Data)
{
	EFI_STATUS Status;
	EFI_GRUB_FILE *File = _CR(This, EFI_GRUB_FILE, EfiFile);
	EFI_FILE_SYSTEM_INFO *FSInfo = (EFI_FILE_SYSTEM_INFO *) Data;
	EFI_FILE_INFO *Info = (EFI_FILE_INFO *) Data;
	CHAR16 GuidString[36];
	EFI_TIME Time;
	CHAR8* label;

	PrintInfo(L"GetInfo(%llx|'%s', %d) %s\n", (UINT64) This,
		FileName(File), *Len, File->IsDir?L"<DIR>":L"");

	/* Determine information to return */
	if (CompareMem(Type, &GenericFileInfo, sizeof(*Type)) == 0) {

		/* Fill file information */
		PrintExtra(L"Get regular file information\n");
		if (*Len < MINIMUM_INFO_LENGTH) {
			*Len = MINIMUM_INFO_LENGTH;
			return EFI_BUFFER_TOO_SMALL;
		}

		ZeroMem(Data, sizeof(EFI_FILE_INFO));

		Info->Attribute = EFI_FILE_READ_ONLY;
		GrubTimeToEfiTime(File->Mtime, &Time);
		CopyMem(&Info->CreateTime, &Time, sizeof(Time));
		CopyMem(&Info->LastAccessTime, &Time, sizeof(Time));
		CopyMem(&Info->ModificationTime, &Time, sizeof(Time));

		if (File->IsDir) {
			Info->Attribute |= EFI_FILE_DIRECTORY;
		} else {
			Info->FileSize = GrubGetFileSize(File);
			Info->PhysicalSize = GrubGetFileSize(File);
		}

		Status = Utf8ToUtf16NoAlloc(File->basename, Info->FileName,
				Info->Size - sizeof(EFI_FILE_INFO));
		if (EFI_ERROR(Status)) {
			if (Status != EFI_BUFFER_TOO_SMALL)
				PrintStatusError(Status, L"Could not convert basename to UTF-8");
			return Status;
		}

		/* The Info struct size already accounts for the extra NUL */
		Info->Size = sizeof(EFI_FILE_INFO) + 
				StrLen(Info->FileName) * sizeof(CHAR16);
		return EFI_SUCCESS;

	} else if (CompareMem(Type, &FileSystemInfo, sizeof(*Type)) == 0) {

		/* Get file system information */
		PrintExtra(L"Get file system information\n");
		if (*Len < MINIMUM_FS_INFO_LENGTH) {
			*Len = MINIMUM_FS_INFO_LENGTH;
			return EFI_BUFFER_TOO_SMALL;
		}

		ZeroMem(Data, sizeof(EFI_FILE_INFO));
		FSInfo->Size = *Len;
		FSInfo->ReadOnly = 1;
		/* NB: This should really be cluster size, but we don't have access to that */
		FSInfo->BlockSize = File->FileSystem->BlockIo->Media->BlockSize;
		if (FSInfo->BlockSize  == 0) {
			PrintWarning(L"Corrected Media BlockSize\n");
			FSInfo->BlockSize = 512;
		}
		FSInfo->VolumeSize = (File->FileSystem->BlockIo->Media->LastBlock + 1) *
			FSInfo->BlockSize;
		/* No idea if we can easily get this for GRUB, and the device is RO anyway */
		FSInfo->FreeSpace = 0;

		Status = GrubLabel(File, &label);
		if (EFI_ERROR(Status)) {
			PrintStatusError(Status, L"Could not read disk label");
		} else {
			Status = Utf8ToUtf16NoAlloc(label, FSInfo->VolumeLabel,
					FSInfo->Size - sizeof(EFI_FILE_SYSTEM_INFO));
			if (EFI_ERROR(Status)) {
				if (Status != EFI_BUFFER_TOO_SMALL)
					PrintStatusError(Status, L"Could not convert label to UTF-8");
				return Status;
			}
			Info->Size = sizeof(EFI_FILE_SYSTEM_INFO) +
					StrLen(FSInfo->VolumeLabel) * sizeof(CHAR16);
		}
		return EFI_SUCCESS;

	} else {

		GuidToString(GuidString, Type);
		PrintError(L"'%s': Cannot get information of type %s\n",
				FileName(File), GuidString);
		return EFI_UNSUPPORTED;

	}
}

/**
 * Set file information
 *
 * @v This			File handle
 * @v Type			Type of information
 * @v Len			Buffer size
 * @v Data			Buffer
 * @ret Status		EFI status code
 */
static EFI_STATUS EFIAPI
FileSetInfo(EFI_FILE_HANDLE This, EFI_GUID *Type, UINTN Len, VOID *Data)
{
	EFI_GRUB_FILE *File = _CR(This, EFI_GRUB_FILE, EfiFile);
	CHAR16 GuidString[36];

	GuidToString(GuidString, Type);
	PrintError(L"Cannot set information of type %s for file '%s'\n",
			GuidString, FileName(File));
	return EFI_WRITE_PROTECTED;
}

/**
 * Flush file modified data
 *
 * @v This			File handle
 * @v Type			Type of information
 * @v Len			Buffer size
 * @v Data			Buffer
 * @ret Status		EFI status code
 */
static EFI_STATUS EFIAPI
FileFlush(EFI_FILE_HANDLE This)
{
	EFI_GRUB_FILE *File = _CR(This, EFI_GRUB_FILE, EfiFile);

	PrintInfo(L"Flush(%llx|'%s')\n", (UINT64)This, FileName(File));
	return EFI_SUCCESS;
}

/**
 * Open root directory
 *
 * @v This			EFI simple file system
 * @ret Root		File handle for the root directory
 * @ret Status		EFI status code
 */
static EFI_STATUS EFIAPI
FileOpenVolume(EFI_FILE_IO_INTERFACE *This, EFI_FILE_HANDLE *Root)
{
	EFI_FS *FSInstance = _CR(This, EFI_FS, FileIoInterface);

	PrintInfo(L"OpenVolume\n");
	*Root = &FSInstance->RootFile->EfiFile;

	return EFI_SUCCESS;
}

/**
 * Install the EFI simple file system protocol
 * If successful this call instantiates a new FS#: drive, that is made
 * available on the next 'map -r'. Note that all this call does is add
 * the FS protocol. OpenVolume won't be called until a process tries
 * to access a file or the root directory on the volume.
 */
static EFI_STATUS
FSInstall(EFI_FS *This, EFI_HANDLE ControllerHandle)
{
	EFI_STATUS Status;
	EFI_DEVICE_PATH *DevicePath = NULL;
	CHAR16 *DevicePathString;

	/* Check if it's a filesystem we can handle */
	if (!GrubFSProbe(This))
		return EFI_UNSUPPORTED;

	/* Install the simple file system protocol. */
	Status = LibInstallProtocolInterfaces(&ControllerHandle,
			&FileSystemProtocol, &This->FileIoInterface,
			NULL);
	if (EFI_ERROR(Status)) {
		PrintStatusError(Status, L"Could not install simple file system protocol");
		return Status;
	}

	DevicePath = DevicePathFromHandle(ControllerHandle);
	if (DevicePath != NULL) {
		DevicePathString = DevicePathToStr(DevicePath);
		This->DevicePath = Utf16ToUtf8Alloc(DevicePathString);
		if (This->DevicePath == NULL)
			PrintWarning(L"Could not convert DevicePath %s to UTF-8\n", DevicePathString);
		PrintInfo(L"FSInstall: %s\n", DevicePathString);
	}

	/* Initialize the root handle */
	Status = GrubCreateFile(&This->RootFile, This);
	if (EFI_ERROR(Status)) {
		PrintStatusError(Status, L"Could not create root file");
		return Status;
	}

	/* Setup the EFI part */
	This->RootFile->EfiFile.Revision = EFI_FILE_HANDLE_REVISION;
	This->RootFile->EfiFile.Open = FileOpen;
	This->RootFile->EfiFile.Close = FileClose;
	This->RootFile->EfiFile.Delete = FileDelete;
	This->RootFile->EfiFile.Read = FileRead;
	This->RootFile->EfiFile.Write = FileWrite;
	This->RootFile->EfiFile.GetPosition = FileGetPosition;
	This->RootFile->EfiFile.SetPosition = FileSetPosition;
	This->RootFile->EfiFile.GetInfo = FileGetInfo;
	This->RootFile->EfiFile.SetInfo = FileSetInfo;
	This->RootFile->EfiFile.Flush = FileFlush;

	/* Setup the other attributes */
	This->RootFile->path = "/";
	This->RootFile->basename = &This->RootFile->path[1];
	This->RootFile->IsDir = TRUE;

	return EFI_SUCCESS;
}

/* Uninstall EFI simple file system protocol */
static void
FSUninstall(EFI_FS *This, EFI_HANDLE ControllerHandle)
{
	CHAR16 *DevicePathString = Utf8ToUtf16Alloc(This->DevicePath);
	PrintInfo(L"FSUninstall: %s\n", DevicePathString);
	FreePool(DevicePathString);

	LibUninstallProtocolInterfaces(ControllerHandle,
			&FileSystemProtocol, &This->FileIoInterface,
			NULL);

	GrubDestroyFile(This->RootFile);
	FreePool(This->DevicePath);
}

/*
 * Global Driver Part
 */

/* Return the driver name */
static EFI_STATUS EFIAPI
FSGetDriverName(EFI_COMPONENT_NAME_PROTOCOL *This,
		CHAR8 *Language, CHAR16 **DriverName)
{
	*DriverName = DriverNameString;
	return EFI_SUCCESS;
}

/* Return the controller name (unsupported for a filesystem) */
static EFI_STATUS EFIAPI
FSGetControllerName(EFI_COMPONENT_NAME_PROTOCOL *This,
		EFI_HANDLE ControllerHandle, EFI_HANDLE ChildHandle,
		CHAR8 *Language, CHAR16 **ControllerName)
{
	return EFI_UNSUPPORTED;
}

/*
 * http://sourceforge.net/p/tianocore/edk2-MdeModulePkg/ci/master/tree/Universal/Disk/DiskIoDxe/DiskIo.c
 * To check if your driver has a chance to apply to the controllers sent during
 * the supported detection phase, try to open the child protocols they are meant
 * to consume in exclusive access (here EFI_DISK_IO).
 */
static EFI_STATUS EFIAPI
FSBindingSupported(EFI_DRIVER_BINDING_PROTOCOL *This,
		EFI_HANDLE ControllerHandle,
		EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath)
{
	EFI_STATUS Status;
	EFI_DISK_IO *DiskIo;

	/* Don't handle this unless we can get exclusive access to DiskIO through it */
	Status = BS->OpenProtocol(ControllerHandle,
			&DiskIoProtocol, (VOID **) &DiskIo,
			This->DriverBindingHandle, ControllerHandle,
			EFI_OPEN_PROTOCOL_BY_DRIVER);
	if (EFI_ERROR(Status))
		return Status;

	PrintDebug(L"FSBindingSupported\n");

	/* The whole concept of BindingSupported is to hint at what we may
	 * actually support, but not check if the target is valid or
	 * initialize anything, so we must close all protocols we opened.
	 */
	BS->CloseProtocol(ControllerHandle, &DiskIoProtocol,
			This->DriverBindingHandle, ControllerHandle);

	return EFI_SUCCESS;
}

static EFI_STATUS EFIAPI
FSBindingStart(EFI_DRIVER_BINDING_PROTOCOL *This,
		EFI_HANDLE ControllerHandle,
		EFI_DEVICE_PATH_PROTOCOL *RemainingDevicePath)
{
	EFI_STATUS Status;
	EFI_FS *Instance;

	PrintDebug(L"FSBindingStart\n");

	/* Allocate a new instance of a filesystem */
	Instance = AllocateZeroPool(sizeof(EFI_FS));
	if (Instance == NULL) {
		Status = EFI_OUT_OF_RESOURCES;
		PrintStatusError(Status, L"Could not allocate a new file system instance");
		return Status;
	}
	Instance->FileIoInterface.Revision = EFI_FILE_IO_INTERFACE_REVISION;
	Instance->FileIoInterface.OpenVolume = FileOpenVolume,

	/* Get access to the Block IO protocol for this controller */
	Status = BS->OpenProtocol(ControllerHandle,
			&BlockIoProtocol, (VOID **) &Instance->BlockIo,
			This->DriverBindingHandle, ControllerHandle,
			/* http://wiki.phoenix.com/wiki/index.php/EFI_BOOT_SERVICES#OpenProtocol.28.29
			 * EFI_OPEN_PROTOCOL_BY_DRIVER returns Access Denied here, most likely
			 * because the disk driver has that protocol already open. So we use
			 * EFI_OPEN_PROTOCOL_GET_PROTOCOL (which doesn't require us to close it)
			 */
			EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	if (EFI_ERROR(Status)) {
		PrintStatusError(Status, L"Could not access BlockIO protocol");
		goto error;
	}

	/* Get exclusive access to the Disk IO protocol */
	Status = BS->OpenProtocol(ControllerHandle,
			&DiskIoProtocol, (VOID**) &Instance->DiskIo,
			This->DriverBindingHandle, ControllerHandle,
			EFI_OPEN_PROTOCOL_BY_DRIVER);
	if (EFI_ERROR(Status)) {
		PrintStatusError(Status, L"Could not access the DiskIo protocol");
		goto error;
	}

	Status = GrubDeviceInit(Instance);
	if (EFI_ERROR(Status)) {
		PrintStatusError(Status, L"Could not init grub device");
		goto error;
	}

	Status = FSInstall(Instance, ControllerHandle);
	/* Unless we close the DiskIO protocol in case of error, no other
	 * FS driver will be able to access this partition.
	 */
	if (EFI_ERROR(Status)) {
		BS->CloseProtocol(ControllerHandle, &DiskIoProtocol,
			This->DriverBindingHandle, ControllerHandle);
	}

error:
	if (EFI_ERROR(Status))
		FreePool(Instance);
	return Status;
}

static EFI_STATUS EFIAPI
FSBindingStop(EFI_DRIVER_BINDING_PROTOCOL *This,
		EFI_HANDLE ControllerHandle, UINTN NumberOfChildren,
		EFI_HANDLE *ChildHandleBuffer)
{
	EFI_STATUS Status;
	EFI_FS *FSInstance;
	EFI_FILE_IO_INTERFACE *FileIoInterface;

	PrintDebug(L"FSBindingStop\n");

	/* Get a pointer back to our FS instance through its installed protocol */
	Status = BS->OpenProtocol(ControllerHandle,
			&FileSystemProtocol, (VOID **) &FileIoInterface,
			This->DriverBindingHandle, ControllerHandle,
			EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	if (EFI_ERROR(Status)) {
		PrintStatusError(Status, L"Could not locate our instance");
		return Status;
	}

	FSInstance = _CR(FileIoInterface, EFI_FS, FileIoInterface);
	FSUninstall(FSInstance, ControllerHandle);

	Status = GrubDeviceExit(FSInstance);
	if (EFI_ERROR(Status)) {
		PrintStatusError(Status, L"Could not destroy grub device");
	}

	FreePool(FSInstance);

	BS->CloseProtocol(ControllerHandle, &DiskIoProtocol,
			This->DriverBindingHandle, ControllerHandle);

	return EFI_SUCCESS;
}

/*
 * The platform determines whether it will support the older Component
 * Name Protocol or the current Component Name2 Protocol, or both.
 * Because of this, it is strongly recommended that you implement both
 * protocols in your driver.
 * 
 * NB: From what I could see, the only difference between Name and Name2
 * is that Name uses ISO-639-2 ("eng") whereas Name2 uses RFC 4646 ("en")
 * See: http://www.loc.gov/standards/iso639-2/faq.html#6
 */
static EFI_COMPONENT_NAME_PROTOCOL FSComponentName = {
	.GetDriverName = FSGetDriverName,
	.GetControllerName = FSGetControllerName,
	.SupportedLanguages = (CHAR8 *) "eng"
};

static EFI_COMPONENT_NAME2_PROTOCOL FSComponentName2 = {
	.GetDriverName = (EFI_COMPONENT_NAME2_GET_DRIVER_NAME) FSGetDriverName,
	.GetControllerName = (EFI_COMPONENT_NAME2_GET_CONTROLLER_NAME) FSGetControllerName,
	.SupportedLanguages = (CHAR8 *) "en"
};

static EFI_DRIVER_BINDING_PROTOCOL FSDriverBinding = {
	.Supported = FSBindingSupported,
	.Start = FSBindingStart,
	.Stop = FSBindingStop,
	/* This field is used by the EFI boot service ConnectController() to determine the order
	 * that driver's Supported() service will be used when a controller needs to be started.
	 * EFI Driver Binding Protocol instances with higher Version values will be used before
	 * ones with lower Version values. The Version values of 0x0-0x0f and 
	 * 0xfffffff0-0xffffffff are reserved for platform/OEM specific drivers. The Version
	 * values of 0x10-0xffffffef are reserved for IHV-developed drivers. 
	 */
	.Version = 0x10,
	.ImageHandle = NULL,
	.DriverBindingHandle = NULL
};

/**
 * Uninstall EFI driver
 *
 * @v ImageHandle       Handle identifying the loaded image
 * @ret Status          EFI status code to return on exit
 */
EFI_STATUS EFIAPI
FSDriverUninstall(EFI_HANDLE ImageHandle)
{
	EFI_STATUS Status;
	UINTN NumHandles;
	EFI_HANDLE *Handles;
	UINTN i;

	/* Enumerate all handles */
	Status = BS->LocateHandleBuffer(AllHandles, NULL, NULL, &NumHandles, &Handles);

	/* Disconnect controllers linked to our driver. This action will trigger a call to BindingStop */
	if (Status == EFI_SUCCESS) {
		for (i=0; i<NumHandles; i++) {
			/* Make sure to filter on DriverBindingHandle,  else EVERYTHING gets disconnected! */
			Status = BS->DisconnectController(Handles[i], FSDriverBinding.DriverBindingHandle, NULL);
			if (Status == EFI_SUCCESS)
				PrintDebug(L"DisconnectController[%d]\n", i);
		}
	} else {
		PrintStatusError(Status, L"Unable to enumerate handles");
	}
	BS->FreePool(Handles);

	/* Now that all controllers are disconnected, we can safely remove our protocols */
	LibUninstallProtocolInterfaces(ImageHandle,
			&DriverBindingProtocol, &FSDriverBinding,
			&ComponentNameProtocol, &FSComponentName,
			&ComponentName2Protocol, &FSComponentName2,
			NULL);

	/* Unregister the relevant grub module */
	GRUB_FS_CALL(DRIVERNAME, fini)();
#if defined(EXTRAMODULE)
	GRUB_FS_CALL(EXTRAMODULE, fini)();
#endif

	/* Uninstall our mutex (we're the only instance that can run this code) */
	LibUninstallProtocolInterfaces(MutexHandle,
				MutexGUID, &MutexProtocol,
				NULL);

	PrintDebug(L"FS driver uninstalled.\n");
	return EFI_SUCCESS;
}

/* You can control the verbosity of the driver output by setting the shell environment
 * variable FS_LOGGING to one of the values defined in the FS_LOGLEVEL constants
 */
static VOID
SetLogging(VOID)
{
	EFI_STATUS Status;
	CHAR16 LogVar[4];
	UINTN i, LogVarSize = sizeof(LogVar);

	Status = RT->GetVariable(L"FS_LOGGING", &ShellVariable, NULL, &LogVarSize, LogVar);
	if (Status == EFI_SUCCESS)
		LogLevel = Atoi(LogVar);

	for (i=0; i<ARRAYSIZE(PrintTable); i++)
		*PrintTable[i] = (i < LogLevel)?Print:PrintNone;

	PrintExtra(L"LogLevel = %d\n", LogLevel);
}

/**
 * Install EFI driver - Will be the entrypoint for our driver executable
 * http://wiki.phoenix.com/wiki/index.php/EFI_IMAGE_ENTRY_POINT
 *
 * @v ImageHandle       Handle identifying the loaded image
 * @v SystemTable       Pointers to EFI system calls
 * @ret Status          EFI status code to return on exit
 */
EFI_STATUS EFIAPI
FSDriverInstall(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable)
{
	EFI_STATUS Status;
	EFI_LOADED_IMAGE *LoadedImage = NULL;
	VOID *Interface;

	InitializeLib(ImageHandle, SystemTable);
	SetLogging();
	EfiImageHandle = ImageHandle;

	/* Prevent the driver from being loaded twice by detecting and trying to
	 * instantiate a custom protocol, which we use as a global mutex.
	 */
	MutexGUID = (EFI_GUID *) GetFSGuid(WIDEN(STRINGIFY(DRIVERNAME)));
	if (MutexGUID == NULL) {
		PrintError(L"No GUID is defined for " WIDEN(STRINGIFY(DRIVERNAME)) 
				L". Please edit <fs_guid.h> to add one\n");
		return EFI_LOAD_ERROR;
	}

	Status = BS->LocateProtocol(MutexGUID, NULL, &Interface);
	if (Status == EFI_SUCCESS) {
		PrintError(L"This driver has already been installed\n");
		return EFI_LOAD_ERROR;
	}
	/* The only valid status we expect is NOT FOUND here */
	if (Status != EFI_NOT_FOUND) {
		PrintStatusError(Status, L"Could not locate global mutex");
		return Status;
	}
	Status = LibInstallProtocolInterfaces(&MutexHandle,
			MutexGUID, &MutexProtocol,
			NULL);
	if (EFI_ERROR(Status)) {
		PrintStatusError(Status, L"Could not install global mutex");
		return Status;
	}

	/* Grab a handle to this image, so that we can add an unload to our driver */
	Status = BS->OpenProtocol(ImageHandle, &LoadedImageProtocol,
			(VOID **) &LoadedImage, ImageHandle,
			NULL, EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	if (EFI_ERROR(Status)) {
		PrintStatusError(Status, L"Could not open loaded image protocol");
		return Status;
	}

	/* Configure driver binding protocol */
	FSDriverBinding.ImageHandle = ImageHandle;
	FSDriverBinding.DriverBindingHandle = ImageHandle;

	/* Install driver */
	Status = LibInstallProtocolInterfaces(&FSDriverBinding.DriverBindingHandle,
			&DriverBindingProtocol, &FSDriverBinding,
			&ComponentNameProtocol, &FSComponentName,
			&ComponentName2Protocol, &FSComponentName2,
			NULL);
	if (EFI_ERROR(Status)) {
		PrintStatusError(Status, L"Could not bind driver");
		return Status;
	}

	/* Register the uninstall callback */
	LoadedImage->Unload = FSDriverUninstall;

	// TODO: would be nicer to move the fs specific stuff out so we don't have to recompile fs_driver.c
	/* Register the relevant GRUB filesystem module */
	GRUB_FS_CALL(DRIVERNAME, init)();
	/* The GRUB compression routines are registered as an extra module */
	// TODO: Eventually, we could try to turn each GRUB module into their
	// own EFI driver, have them register their interface and consume that.
#if defined(EXTRAMODULE)
	GRUB_FS_CALL(EXTRAMODULE, init)();
#endif

	PrintDebug(L"FS driver installed.\n");
	return EFI_SUCCESS;
}

/* Designate the driver entrypoint */
EFI_DRIVER_ENTRY_POINT(FSDriverInstall)
