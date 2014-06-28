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

// TODO: split this file and move anything GRUB related there
#include <grub/file.h>
#include <grub/charset.h>
#include <grub/misc.h>

#include "fs_driver.h"

/* These ones are not defined in gnu-efi yet */
EFI_GUID DriverBindingProtocol = EFI_DRIVER_BINDING_PROTOCOL_GUID;
EFI_GUID ComponentNameProtocol = EFI_COMPONENT_NAME_PROTOCOL_GUID;
EFI_GUID ComponentName2Protocol = EFI_COMPONENT_NAME2_PROTOCOL_GUID;
EFI_GUID ShellVariable = SHELL_VARIABLE_GUID;

/* We'll try to instantiate a custom protocol as a mutex, so we need a GUID */
EFI_GUID MutexGUID = THIS_FS_GUID;

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
static INTN LogLevel = DEFAULT_LOGLEVEL;

/* Handle for our custom protocol/mutex instance */
static EFI_HANDLE MutexHandle = NULL;

/* Custom protocol/mutex definition */
typedef struct {
	INTN Unused;
} EFI_MUTEX_PROTOCOL;
static EFI_MUTEX_PROTOCOL MutexProtocol = { 0 };

// TODO: implement our own UTF8 <-> UTF16 conversion routines

/**
 * Print an error message along with a human readable EFI status code
 *
 * @v Status		EFI status code
 * @v Format		A non '\n' terminated error message string
 * @v ...			Any extra parameters
 */
void
PrintStatusError(EFI_STATUS Status, const CHAR16 *Format, ...)
{
	CHAR16 StatusString[64];
	va_list ap;

	StatusToString(StatusString, Status);
	va_start(ap, Format);
	VPrint((CHAR16 *)Format, ap);
	va_end(ap);
	PrintError(L": [%d] %s\n", Status, StatusString); 
}

/**
 * Get EFI file name (for debugging)
 *
 * @v file			EFI file
 * @ret Name		Name
 */
// TODO: we may want to keep a copy of the UTF16 Name in the file handle
static const CHAR16 
*FileName(EFI_GRUB_FILE *File)
{
	static CHAR16 Name[MAX_PATH];

	ZeroMem(Name, sizeof(Name));
	grub_utf8_to_utf16(Name, ARRAYSIZE(Name), File->grub_file.name,
		grub_strlen(File->grub_file.name), NULL);
	return Name;
}

/* Simple hook to populate the timestamp and directory flag when opening a file */
static int
grub_infohook(const char *name, const struct grub_dirhook_info *info, void *data)
{
	EFI_GRUB_FILE *File = (EFI_GRUB_FILE *) data;

	/* Look for a specific file */
	if (grub_strcmp(name, File->basename) != 0)
		return 0;

	File->IsDir = (info->dir);
	if (info->mtimeset)
		File->grub_time = info->mtime;

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
	EFI_GRUB_FILE *File = container_of(This, EFI_GRUB_FILE, EfiFile);
	EFI_GRUB_FILE *NewFile;
	grub_fs_t p = grub_fs_list;
	grub_err_t rc;
	/* Why oh why doesn't GRUB provide a grub_get_num_of_utf8_bytes for UTF16
	 * or even an UTF16 to UTF8 that does the allocation?
	 * TODO: Use dynamic buffers...
	 */
	char path[MAX_PATH], clean_path[MAX_PATH], *dirname;
	INTN i, len;
	BOOLEAN AbsolutePath = (*Name == L'\\');

	PrintInfo(L"Open(%llx%s, \"%s\")\n", (UINT64) This,
			(File == &File->FileSystem->RootFile)?L" <ROOT>":L"", Name);

	/* Fail unless opening read-only */
	if (Mode != EFI_FILE_MODE_READ) {
		PrintError(L"File '%s' can only be opened in read-only mode\n", Name);
		return EFI_WRITE_PROTECTED;
	}

	/* Additional failures */
	if ((StrCmp(Name, L"..") == 0) && (File == &File->FileSystem->RootFile)) {
		PrintWarning(L"Trying to open <ROOT>'s parent\n");
		return EFI_NOT_FOUND;
	}

	/* See if we're trying to reopen current (which the EFI Shell insists on doing) */
	if (StrCmp(Name, L".") == 0) {
		PrintInfo(L"  Reopening %s\n",
				(File == &File->FileSystem->RootFile)?L"<ROOT>":FileName(File));
		File->refcount++;
		*New = This;
		PrintInfo(L"  RET: %llx\n", (UINT64) *New);
		return EFI_SUCCESS;
	}

	/* If we have an absolute path, don't bother completing with the parent */
	if (AbsolutePath) {
		len = 0;
	} else {
		grub_strcpy(path, File->grub_file.name);
		len = grub_strlen(path);
		/* Add delimiter if needed */
		if ((len == 0) || (path[len-1] != '/'))
			path[len++] = '/';
	}

	/* Copy the rest of the path (converted to UTF-8) */
	grub_utf16_to_utf8(&path[len], Name, StrLen(Name) + 1);
	/* Convert the delimiters */
	for (i = grub_strlen(path) - 1 ; i >= len; i--) {
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
		*New = &File->FileSystem->RootFile.EfiFile;
		PrintInfo(L"  RET: %llx\n", (UINT64) *New);
		return EFI_SUCCESS;
	}

	// TODO: eventually we should seek for already opened files and increase refcount */
	/* Allocate and initialise file */
	NewFile = AllocateZeroPool(sizeof(*NewFile));
	if (NewFile == NULL) {
		PrintError(L"Could not allocate file resource\n");
		return EFI_OUT_OF_RESOURCES;
	}
	CopyMem(&NewFile->EfiFile, &File->EfiFile, sizeof(NewFile->EfiFile));

	NewFile->grub_file.name = AllocatePool(grub_strlen(clean_path)+1);
	if (NewFile->grub_file.name == NULL) {
		FreePool(NewFile);
		PrintError(L"Could not allocate grub filename\n");
		return EFI_OUT_OF_RESOURCES;
	}
	CopyMem(NewFile->grub_file.name, clean_path, grub_strlen(path)+1);

	/* Isolate the basename and dirname */
	for (i = grub_strlen(clean_path) - 1; i >= 0; i--) {
		if (clean_path[i] == '/') {
			clean_path[i] = 0;
			break;
		}
	}
	dirname = (i <= 0) ? "/" : clean_path;
	NewFile->basename = &NewFile->grub_file.name[i+1];

	/* Duplicate the attributes needed */
	NewFile->grub_file.device = File->grub_file.device;
	NewFile->grub_file.fs = File->grub_file.fs;
	NewFile->FileSystem = File->FileSystem;

	/* Find if we're working with a directory and fill the grub timestamp */
	rc = p->dir(NewFile->grub_file.device, dirname, grub_infohook, (void *) NewFile);
	if (rc) {
		PrintError(L"Could not get file attributes for '%s' - GRUB error %d\n", Name, rc);
		FreePool(NewFile->grub_file.name);
		FreePool(NewFile);
		return EFI_NOT_FOUND;
	}

	/* Finally we can call on GRUB to open the file */
	rc = p->open(&NewFile->grub_file, NewFile->grub_file.name);
	if ((rc != GRUB_ERR_NONE) && (rc != GRUB_ERR_BAD_FILE_TYPE)) {
		PrintError(L"Could not open file '%s' - GRUB error %d\n", Name, rc);
		FreePool(NewFile->grub_file.name);
		FreePool(NewFile);
		return EFI_NOT_FOUND;
	}

	NewFile->refcount++;
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
	grub_fs_t p = grub_fs_list;
	EFI_GRUB_FILE *File = container_of(This, EFI_GRUB_FILE, EfiFile);

	PrintInfo(L"Close(%llx|'%s') %s\n", (UINT64) This, FileName(File),
		(File == &File->FileSystem->RootFile)?L"<ROOT>":L"");

	/* Nothing to do it this is the root */
	if (File == &File->FileSystem->RootFile)
		return EFI_SUCCESS;

	if (--File->refcount == 0) {
		/* Close file */
		p->close(&File->grub_file);
		FreePool(File->grub_file.name);
		FreePool(File);
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
	EFI_GRUB_FILE *File = container_of(This, EFI_GRUB_FILE, EfiFile);

	PrintError(L"Cannot delete '%s'\n", FileName(File));

	/* Close file */
	FileClose(This);

	/* Warn of failure to delete */
	return EFI_WARN_DELETE_FAILURE;
}

/* GRUB uses a callback for each directory entry, whereas EFI uses repeated
 * firmware generated calls to FileReadDir() to get the info for each entry,
 * so we have to reconcile the twos. For now, we'll re-issue a call to grub
 * dir(), and run through all the entries (to find the one we
 * are interested in)  multiple times. Maybe later we'll try to optimize this
 * by building a one-off chained list of entries that we can parse...
 */
static int
grub_dirhook(const char *name, const struct grub_dirhook_info *info, void *data)
{
	EFI_FILE_INFO *Info = (EFI_FILE_INFO *) data;
	INT64 *Index = (INT64 *) &Info->FileSize;
	char *filename = (char *) Info->PhysicalSize;
	EFI_TIME Time = { 1970, 01, 01, 00, 00, 00, 0, 0, 0, 0, 0};

	// Eliminate '.' or '..'
	if ((name[0] ==  '.') && ((name[1] == 0) || ((name[1] == '.') && (name[2] == 0))))
		return 0;

	/* Ignore any entry that doesn't match our index */
	if ((*Index)-- != 0)
		return 0;

//	if (*name != '$')
//		grub_printf("PRO: %s\n", name);
	grub_strcpy(filename, name);

	// TODO: check for overflow and return EFI_BUFFER_TOO_SMALL
	grub_utf8_to_utf16(Info->FileName, Info->Size - sizeof(EFI_FILE_INFO),
			filename, -1, NULL);
	/* The Info struct size already accounts for the extra NUL */
	Info->Size = sizeof(*Info) + StrLen(Info->FileName) * sizeof(CHAR16);

	// Oh, and of course GRUB uses a 32 bit signed mtime value (seriously, wtf guys?!?)
	if (info->mtimeset)
		GrubTimeToEfiTime(info->mtime, &Time);
	CopyMem(&Info->CreateTime, &Time, sizeof(Time));
	CopyMem(&Info->LastAccessTime, &Time, sizeof(Time));
	CopyMem(&Info->ModificationTime, &Time, sizeof(Time));

	Info->Attribute = EFI_FILE_READ_ONLY;
	if (info->dir)
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
	/* We temporarily repurpose the FileSize as a *signed* entry index */
	INT64 *Index = (INT64 *) &Info->FileSize;
	/* And PhysicalSize as a pointer to our filename */
	char **basename = (char **) &Info->PhysicalSize;
	char path[MAX_PATH];
	struct grub_file file;
	grub_fs_t p = grub_fs_list;
	grub_err_t rc;
	INTN len;

	/* Unless we can fill at least a 16 chars filename, forget it */
	if (*Len < MINIMUM_INFO_LENGTH) {
		*Len = MINIMUM_INFO_LENGTH;
		return EFI_BUFFER_TOO_SMALL;
	}

	/* Populate our Info template */
	ZeroMem(Data, *Len);
	Info->Size = *Len;
	*Index = File->grub_file.offset;
	grub_strcpy(path, File->grub_file.name);
	len = grub_strlen(path);
	if (path[len-1] != '/')
		path[len++] = '/';
	*basename = &path[len];

	/* Invoke GRUB's directory listing */
	rc = p->dir(File->grub_file.device, File->grub_file.name, grub_dirhook, Data);

	if (*Index >= 0) {
		/* No more entries */
		*Len = 0;
		return EFI_SUCCESS;
	}

	if (rc == GRUB_ERR_OUT_OF_RANGE) {
		PrintError(L"Dir entry buffer too small\n");
		return EFI_BUFFER_TOO_SMALL;
	} else if (rc) {
		PrintError(L"GRUB dir failed with error %d\n", rc);
		return EFI_UNSUPPORTED;
	}

	/* Our Index/FileSize must be reset */
	Info->FileSize = 0;
	Info->PhysicalSize = 0;

	/* For regular files, we still need to fill the size */
	if (!(Info->Attribute & EFI_FILE_DIRECTORY)) {
		/* Open the file and read its size */
		file.device = File->grub_file.device;
		file.fs = File->grub_file.fs;
		rc = p->open(&file, path);
		if (rc == GRUB_ERR_NONE) {
			Info->FileSize = file.size;
			Info->PhysicalSize = file.size;
			p->close(&file);
		} else if (rc !=  GRUB_ERR_BAD_FILE_TYPE) {
			/* BAD_FILE_TYPE is returned for links */
			PrintError(L"Unable to obtain the size of '%s' - grub error: %d\n",
					Info->FileName, rc);
		}
	}

	*Len = (UINTN) Info->Size;
	/* Advance to the next entry */
	File->grub_file.offset++;

//	PrintInfo(L"  Entry: '%s' %s\n", Info->FileName,
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
	grub_fs_t p = grub_fs_list;
	grub_ssize_t len;
	EFI_GRUB_FILE *File = container_of(This, EFI_GRUB_FILE, EfiFile);
	INTN Remaining;

	PrintInfo(L"Read(%llx|'%s', %d) %s\n", This, FileName(File),
			*Len, File->IsDir?L"<DIR>":L"");

	/* If this is a directory, then fetch the directory entries */
	if (File->IsDir)
		return FileReadDir(File, Len, Data);

	/* GRUB may return an error if we request more data than available */
	Remaining = File->grub_file.size - File->grub_file.offset;
	if (*Len > Remaining)
		*Len = Remaining;
	len = p->read(&File->grub_file, (char *) Data, *Len);
	if (len < 0) {
		grub_print_error();
		PrintError(L"Could not read from file '%s': GRUB error %d\n",
				FileName(File), grub_errno);
		return EFI_DEVICE_ERROR;
	}
	*Len = len;
	return EFI_SUCCESS;
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
	EFI_GRUB_FILE *File = container_of(This, EFI_GRUB_FILE, EfiFile);

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
	EFI_GRUB_FILE *File = container_of(This, EFI_GRUB_FILE, EfiFile);

	PrintInfo(L"SetPosition(%llx|'%s', %lld) %s\n", (UINT64) This,
		FileName(File), Position, (File->IsDir)?L"<DIR>":L"");

	/* If this is a directory, reset to the start */
	if (File->IsDir) {
		File->grub_file.offset = 0;
		return EFI_SUCCESS;
	}

	/* Fail if we attempt to seek past the end of the file (since
	 * we do not support writes).
	 */
	if (Position > File->grub_file.size) {
		PrintError(L"'%s': Cannot seek to %#llx of %llx\n",
				FileName(File), Position, File->grub_file.size);
		return EFI_UNSUPPORTED;
	}

	/* Set position */
	File->grub_file.offset = Position;
	PrintDebug(L"'%s': Position set to %llx\n",
			FileName(File), File->grub_file.offset);

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
	EFI_GRUB_FILE *File = container_of(This, EFI_GRUB_FILE, EfiFile);

	PrintInfo(L"GetPosition(%llx|'%s', %lld)\n", This, FileName(File));

	*Position = File->grub_file.offset;
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
	EFI_GRUB_FILE *File = container_of(This, EFI_GRUB_FILE, EfiFile);
	EFI_FILE_SYSTEM_INFO *FSInfo = (EFI_FILE_SYSTEM_INFO *) Data;
	EFI_FILE_INFO *Info = (EFI_FILE_INFO *) Data;
	CHAR16 GuidString[36];
	EFI_TIME Time;
	grub_fs_t p = grub_fs_list;
	grub_err_t rc;
	char* label;

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

		// TODO: only zero the INFO part
		ZeroMem(Data, *Len);

		Info->Attribute = EFI_FILE_READ_ONLY;
		GrubTimeToEfiTime(File->grub_time, &Time);
		CopyMem(&Info->CreateTime, &Time, sizeof(Time));
		CopyMem(&Info->LastAccessTime, &Time, sizeof(Time));
		CopyMem(&Info->ModificationTime, &Time, sizeof(Time));

		if (File->IsDir) {
			Info->Attribute |= EFI_FILE_DIRECTORY;
		} else {
			Info->FileSize = File->grub_file.size;
			Info->PhysicalSize = File->grub_file.size;
		}

		// TODO: check for overflow and return EFI_BUFFER_TOO_SMALL
		grub_utf8_to_utf16(Info->FileName, Info->Size - sizeof(EFI_FILE_INFO),
				File->basename, -1, NULL);
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

		// TODO: only zero the INFO part
		ZeroMem(Data, *Len);
		FSInfo->Size = *Len;
		FSInfo->ReadOnly = 1;
		/* NB: This should really be cluster size, but we don't have access to that */
		FSInfo->BlockSize = File->FileSystem->BlockIo->Media->BlockSize;
		FSInfo->VolumeSize = (File->FileSystem->BlockIo->Media->LastBlock + 1) *
			FSInfo->BlockSize;
		/* No idea if we can easily get this for GRUB, and the device is RO anyway */
		FSInfo->FreeSpace = 0;
		rc = p->label(File->grub_file.device, &label);
		if (rc) {
			PrintError(L"Could not read disk label - GRUB error %d\n", rc);
		} else {
			// TODO: check for overflow and return EFI_BUFFER_TOO_SMALL
			grub_utf8_to_utf16(FSInfo->VolumeLabel,
					FSInfo->Size - sizeof(EFI_FILE_SYSTEM_INFO), label, -1, NULL);
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
	EFI_GRUB_FILE *File = container_of(This, EFI_GRUB_FILE, EfiFile);
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
	EFI_GRUB_FILE *File = container_of(This, EFI_GRUB_FILE, EfiFile);

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
	EFI_FS *FSInstance = container_of(This, EFI_FS, FileIOInterface);

	PrintInfo(L"OpenVolume\n");
	*Root = &FSInstance->RootFile.EfiFile;

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

	/* Check if it's a filesystem we can handle */
	if (!GrubFSProbe(This))
		return EFI_UNSUPPORTED;

	/* Install the simple file system protocol. */
	Status = LibInstallProtocolInterfaces(&ControllerHandle,
			&FileSystemProtocol, &This->FileIOInterface,
			NULL);
	if (EFI_ERROR(Status)) {
		PrintStatusError(Status, L"Could not install simple file system protocol");
		return Status;
	}

	DevicePath = DevicePathFromHandle(ControllerHandle);
	if (DevicePath != NULL) {
		StrCpy(This->DevicePath, DevicePathToStr(DevicePath));
		PrintInfo(L"FSInstall: %s\n", DevicePathToStr(DevicePath));
	}

	/* Initialize the root handle */
	ZeroMem(&This->RootFile, sizeof(This->RootFile));

	/* Setup the EFI part */
	This->RootFile.EfiFile.Revision = EFI_FILE_HANDLE_REVISION;
	This->RootFile.EfiFile.Open = FileOpen;
	This->RootFile.EfiFile.Close = FileClose;
	This->RootFile.EfiFile.Delete = FileDelete;
	This->RootFile.EfiFile.Read = FileRead;
	This->RootFile.EfiFile.Write = FileWrite;
	This->RootFile.EfiFile.GetPosition = FileGetPosition;
	This->RootFile.EfiFile.SetPosition = FileSetPosition;
	This->RootFile.EfiFile.GetInfo = FileGetInfo;
	This->RootFile.EfiFile.SetInfo = FileSetInfo;
	This->RootFile.EfiFile.Flush = FileFlush;

	/* Setup the GRUB part */
	This->RootFile.grub_file.name = "/";
	This->RootFile.grub_file.device = (grub_device_t) This->GrubDevice;
	This->RootFile.grub_file.fs = grub_fs_list;
	This->RootFile.basename = &This->RootFile.grub_file.name[1];

	/* Setup extra data */
	This->RootFile.IsDir = TRUE;
	/* We could pick it up from GrubDevice, but it's more convenient this way */
	This->RootFile.FileSystem = This;

	return EFI_SUCCESS;
}

/* Uninstall EFI simple file system protocol */
static void
FSUninstall(EFI_FS *This, EFI_HANDLE ControllerHandle)
{
	PrintInfo(L"FSUninstall: %s\n", This->DevicePath);

	LibUninstallProtocolInterfaces(ControllerHandle,
			&FileSystemProtocol, &This->FileIOInterface,
			NULL);
}

/*
 * Global Driver Part
 */

/* Return the driver name */
static EFI_STATUS EFIAPI
FSGetDriverName(EFI_COMPONENT_NAME_PROTOCOL *This,
		CHAR8 *Language, CHAR16 **DriverName)
{
	*DriverName = L"FS driver";
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
	Instance->FileIOInterface.Revision = EFI_FILE_IO_INTERFACE_REVISION;
	Instance->FileIOInterface.OpenVolume = FileOpenVolume,

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
	EFI_FS *Instance;

	PrintDebug(L"FSBindingStop\n");

	/* Get a pointer back to our FS instance through its installed protocol */
	Status = BS->OpenProtocol(ControllerHandle,
			&FileSystemProtocol, (VOID **) &Instance,
			This->DriverBindingHandle, ControllerHandle,
			EFI_OPEN_PROTOCOL_GET_PROTOCOL);
	if (EFI_ERROR(Status)) {
		PrintStatusError(Status, L"Could not locate our instance");
		return Status;
	}

	FSUninstall(Instance, ControllerHandle);

	Status = GrubDeviceExit(Instance);
	if (EFI_ERROR(Status)) {
		PrintStatusError(Status, L"Could not destroy grub device");
	}

	FreePool(Instance);

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
	GRUB_FS_FINI();

	/* Uninstall our mutex (we're the only instance that can run this code) */
	LibUninstallProtocolInterfaces(MutexHandle,
				&MutexGUID, &MutexProtocol,
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
	 * instantiate a custom protocol which we use as a global mutex.
	 */
	Status = BS->LocateProtocol(&MutexGUID, NULL, &Interface);
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
			&MutexGUID, &MutexProtocol,
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

	/* Register the relevant grub module */
	GRUB_FS_INIT();

	PrintDebug(L"FS driver installed.\n");
	return EFI_SUCCESS;
}

/* Designate the driver entrypoint */
EFI_DRIVER_ENTRY_POINT(FSDriverInstall)
