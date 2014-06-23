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
Print_t* PrintTable[] = { &PrintError, &PrintWarning, &PrintInfo, &PrintDebug, &PrintExtra };

/* Global driver verbosity level */
#if !defined(DEFAULT_LOGLEVEL)
#define DEFAULT_LOGLEVEL FS_LOGLEVEL_INFO
#endif
static INTN LogLevel = DEFAULT_LOGLEVEL;

/* Handle for our custom protocol/mutex instance */
static EFI_HANDLE MutexHandle = NULL;

/* Custom protocol/mutex definition */
typedef struct {
	INTN Unused;
} EFI_MUTEX_PROTOCOL;
static EFI_MUTEX_PROTOCOL MutexProtocol = { 0 };

// TODO: we'll need a separate rootfile instance for each FS
static EFI_GRUB_FILE RootFile;

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
static const CHAR16 
*FileName(EFI_GRUB_FILE *File)
{
	static CHAR16 Name[256];

	grub_utf8_to_utf16(Name, ARRAYSIZE(Name), File->grub_file.name,
		grub_strlen(File->grub_file.name), NULL);
	return Name;
}

static int
fs_hook(const char *filename,
	const struct grub_dirhook_info *info, void *data)
{
	if (filename[0] != '$')
		grub_printf("  %s%s\n", filename, info->dir?"/":"");
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
	/* Why oh why doesn't GRUB provide a grub_get_num_of_utf8_bytes for UTF16
	 * or even an UTF16 to UTF8 that does the allocation?
	 * TODO: Use dynamic buffers...
	 */
	char name[256];
	INTN i;

	PrintDebug(L"FileOpen '%s'\n", Name);

	/* Initial '\' indicates opening from the root directory */
	while (*Name == L'\\') {
		File = &RootFile;
		Name++;
	}

	/* Allow root directory itself to be opened */
	if ((Name[0] == L'\0') || (Name[0] == L'.')) {
		*New = &RootFile.EfiFile;
		return EFI_SUCCESS;
	}

	/* Fail unless opening read-only */
	if (Mode != EFI_FILE_MODE_READ) {
		PrintError(L"File '%s' cannot be opened in mode %#08llx\n", Name, Mode);
		return EFI_WRITE_PROTECTED;
	}

	/* Fail unless opening from the root */
	// TODO: eliminate this
	if (!File->IsRoot) {
		PrintError(L"WIP: Only the root directory can be opened for now\n");
		return EFI_NOT_FOUND;
	}

	/* Convert an UTF16 file name with backslash delimiters to UTF8 with forward slash delimiters */
	/* BEWARE: the GRUB conversion routines don't check the target size and the length is for the source!! */
	grub_utf16_to_utf8(name, Name, StrLen(Name)+1);
	for (i=0; i<grub_strlen(name); i++) {
		if (name[i] == '\\')
			name[i] = '/';
	}

	/* TODO: Check that the file exists */

	/* Allocate and initialise file */
	NewFile = AllocateZeroPool(sizeof(*NewFile));
	if (NewFile == NULL) {
		PrintError(L"Could not allocate file resource\n");
		return EFI_OUT_OF_RESOURCES;
	}
	CopyMem(&NewFile->EfiFile, &RootFile.EfiFile, sizeof(NewFile->EfiFile));

	NewFile->grub_file.name = AllocatePool(grub_strlen(name)+1);
	if (NewFile->grub_file.name == NULL) {
		FreePool(NewFile);
		PrintError(L"Could not allocate grub filename\n");
		return EFI_OUT_OF_RESOURCES;
	}

	CopyMem(NewFile->grub_file.name, name, grub_strlen(name)+1);
	NewFile->grub_file.device = RootFile.grub_file.device;
	NewFile->grub_file.fs = RootFile.grub_file.fs;

	*New = &NewFile->EfiFile;
	PrintDebug(L"File '%s' opened\n", FileName(NewFile));

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
	EFI_GRUB_FILE *File = container_of(This, EFI_GRUB_FILE, EfiFile);

	PrintDebug(L"FileClose '%s'\n", FileName(File));

	/* Do nothing if this is the root */
	if (File->IsRoot)
		return EFI_SUCCESS;

	/* Close file */
	PrintDebug(L"Non root file closed\n");
	FreePool(File->grub_file.name);
	FreePool(File);

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

	PrintError(L"EFIFILE %s cannot be deleted\n", FileName(File));

	/* Close file */
	FileClose(This);

	/* Warn of failure to delete */
	return EFI_WARN_DELETE_FAILURE;
}

/**
 * Return variable-length data structure
 *
 * @v Base			Base data structure (starting with UINT64)
 * @v BaseLen		Length of base data structure
 * @v Name			Name to append to base data structure
 * @v Len			Length of data buffer
 * @v Data			Data buffer
 * @ret Status		EFI status code
 */
static EFI_STATUS
FileVarlen(UINT64 *Base, UINT64 BaseLen,
		const CHAR16 *Name, UINT64 *Len, VOID *Data)
{
	UINTN NameLen;

	/* Calculate structure length */
	NameLen = StrLen(Name);
	*Base = (BaseLen + (NameLen + 1 /* NUL */) * sizeof(CHAR16));
	if (*Len < *Base) {
		*Len = *Base;
		return EFI_BUFFER_TOO_SMALL;
	}

	/* Copy data to buffer */
	*Len = *Base;
	CopyMem(Data, Base, BaseLen);
	StrCpy(Data + BaseLen, Name);

	return EFI_SUCCESS;
}

/**
 * Return file information structure
 *
 * @v image			Image, or NULL for the root directory
 * @v Len			Length of data buffer
 * @v Data			Data buffer
 * @ret Status		EFI status code
 */
static EFI_STATUS 
FileInfo(BOOLEAN Dir, UINTN *Len, VOID *Data)
{
	EFI_FILE_INFO Info;
	const CHAR16 *Name;
	const EFI_TIME Time = { 1970, 01, 01, 00, 00, 00, 0, 0, 0, 0, 0};

	PrintDebug(L"FileInfo for %s\n", Dir?L"dir":L"reg");

	/* Populate file information */
	SetMem(&Info, 0, sizeof(Info));
	if (!Dir) {
		Info.FileSize = 16;
		Info.PhysicalSize = 16;
		Info.Attribute = EFI_FILE_READ_ONLY;
		CopyMem(&Info.CreateTime, &Time, sizeof(Time));
		CopyMem(&Info.LastAccessTime, &Time, sizeof(Time));
		CopyMem(&Info.ModificationTime, &Time, sizeof(Time));
		Name = L"file.txt";
	} else {
		Info.Attribute = EFI_FILE_READ_ONLY | EFI_FILE_DIRECTORY;
		Name = L"";
	}

	return FileVarlen(&Info.Size, SIZE_OF_EFI_FILE_INFO, Name, Len, Data);
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
	EFI_STATUS Status;
	UINT64 Index;
	grub_fs_t p = grub_fs_list;
	grub_device_t device = (grub_device_t) File->FileSystem->GrubDevice;

	PrintDebug(L"FileReadDir\n");

	// Test the GRUB dir hook
	p->dir(device, "/", fs_hook, NULL);

	/* Construct directory entry at current position */
	Index = File->grub_file.offset;

	if (Index-- == 0) {
		Status = FileInfo(FALSE, Len, Data);
		if (Status == EFI_SUCCESS)
			File->grub_file.offset++;
		return Status;
	}

	/* No more entries */
	*Len = 0;
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
	EFI_GRUB_FILE *File = container_of(This, EFI_GRUB_FILE, EfiFile);

	PrintDebug(L"FileRead '%s'\n", FileName(File));

	/* If this is the root directory, then construct a directory entry */
	if (File->IsRoot)
		return FileReadDir(File, Len, Data);

	/* Read from a file */
	// TODO
	return EFI_ACCESS_DENIED;
/*
	Remaining = (file->image->len - File->grub_file->offset);
	if (*Len > Remaining)
		*Len = Remaining;
	PrintDebug(L"EFIFILE %s read [%#08zx,%#08zx]\n", FileName(File),
			File->grub_file.offset, File->grub_file.offset + *Len );
	CopyMem(Data, &file->image->data[File->grub_file.offset], *Len);
	File->grub_file.offset += *Len;
	return EFI_SUCCESS;
*/
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

	PrintDebug(L"FileSetPosition '%s'\n", FileName(File));

	/* If this is the root directory, reset to the start */
	if (File->IsRoot) {
		PrintDebug(L"Root directory rewound\n");
		File->grub_file.offset = 0;
		return EFI_SUCCESS;
	}

	/* Fail if we attempt to seek past the end of the file (since
	 * we do not support writes).
	 */
	if (Position > File->grub_file.size) {
		PrintError(L"EFIFILE %s cannot seek to %#08llx of %#08zx\n",
				FileName(File), Position, File->grub_file.size);
		return EFI_UNSUPPORTED;
	}

	/* Set position */
	File->grub_file.offset = Position;
	PrintDebug(L"EFIFILE %s position set to %#08zx\n",
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

	PrintDebug(L"FileGetPosition\n");

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
	EFI_FILE_SYSTEM_INFO FSInfo;
	CHAR16 GuidString[36];

	PrintDebug(L"FileGetInfo '%s'\n", FileName(File));

	/* Determine information to return */
	if (CompareMem(Type, &GenericFileInfo, sizeof(*Type)) == 0) {

		/* Get file information */
		PrintDebug(L"Get regular file information\n");
		return FileInfo(File->IsRoot, Len, Data);

	} else if (CompareMem(Type, &FileSystemInfo, sizeof(*Type)) == 0) {

		/* Get file system information */
		PrintDebug(L"Get file system information\n");
		SetMem(&FSInfo, 0, sizeof(FSInfo));
		FSInfo.ReadOnly = 1;
		FSInfo.VolumeSize = (File->FileSystem->BlockIo->Media->LastBlock + 1) *
			File->FileSystem->BlockIo->Media->BlockSize;
		// TODO: get volume label from GRUB
		return FileVarlen(&FSInfo.Size, SIZE_OF_EFI_FILE_SYSTEM_INFO,
				L"Test Volume Label", Len, Data);
	} else {
		GuidToString(GuidString, Type);
		PrintError(L"EFIFILE %s cannot get information of type %s\n",
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
	PrintError(L"EFIFILE %s cannot set information of type %s\n",
			FileName(File), GuidString);
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
	PrintDebug(L"EFIFILE %s flushed\n", FileName(File));
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
	PrintDebug(L"FileOpenVolume\n");
	*Root = &RootFile.EfiFile;

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
		StrCpy(This->Path, DevicePathToStr(DevicePath));
		PrintInfo(L"FSInstall: %s\n", DevicePathToStr(DevicePath));
	}

	/* Initialize the root handle */
	ZeroMem(&RootFile, sizeof(RootFile));

	/* Setup the EFI part */
	RootFile.EfiFile.Revision = EFI_FILE_HANDLE_REVISION;
	RootFile.EfiFile.Open = FileOpen;
	RootFile.EfiFile.Close = FileClose;
	RootFile.EfiFile.Delete = FileDelete;
	RootFile.EfiFile.Read = FileRead;
	RootFile.EfiFile.Write = FileWrite;
	RootFile.EfiFile.GetPosition = FileGetPosition;
	RootFile.EfiFile.SetPosition = FileSetPosition;
	RootFile.EfiFile.GetInfo = FileGetInfo;
	RootFile.EfiFile.SetInfo = FileSetInfo;
	RootFile.EfiFile.Flush = FileFlush;

	/* Setup the GRUB part */
	RootFile.grub_file.name = "/";
	RootFile.grub_file.device = (grub_device_t) This->GrubDevice;
	RootFile.grub_file.fs = grub_fs_list;

	/* Setup extra data */
	RootFile.IsRoot = TRUE;
	/* We could pick it up from GrubDevice, but it's more convenient this way */
	RootFile.FileSystem = This;

	return EFI_SUCCESS;
}

/* Uninstall EFI simple file system protocol */
static void
FSUninstall(EFI_FS *This, EFI_HANDLE ControllerHandle)
{
	PrintDebug(L"FSUninstall: %s\n", This->Path);

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
