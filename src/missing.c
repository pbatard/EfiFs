/* missing.c - Missing convenience calls from the EFI interface */
/*
 *  Copyright © 2014-2021 Pete Batard <pete@akeo.ie>
 *
 *  NB: This file was relicensed from GPLv3+ to GPLv2+ by its author.
 *  Per https://github.com/pbatard/efifs/commit/6472929565970fc88fbad479c5973898c3275235#diff-e4e188b933de262cfdf4c2be771da37b2ec89ba97d353a16352c6eebb41a286f
 *  you can confirm that we did remove the initial 3 function calls
 *  that were derived from GPLv3+ work, thus granting us the ability
 *  to relicense this file.
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

#include "driver.h"

// Needed to avoid a LNK2043 error with EDK2/MSVC/IA32
#if !defined(__MAKEWITH_GNUEFI) && defined(_M_IX86)
#pragma comment(linker, "/INCLUDE:_MultS64x64")
#endif

EFI_STATUS
PrintGuid(EFI_GUID *Guid)
{
	if (Guid == NULL) {
		Print(L"ERROR: PrintGuid called with a NULL value.\n");
		return EFI_INVALID_PARAMETER;
	}

	Print(L"%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x\n",
		Guid->Data1,
		Guid->Data2,
		Guid->Data3,
		Guid->Data4[0],
		Guid->Data4[1],
		Guid->Data4[2],
		Guid->Data4[3],
		Guid->Data4[4],
		Guid->Data4[5],
		Guid->Data4[6],
		Guid->Data4[7]
	);
	return EFI_SUCCESS;
}

INTN
CompareDevicePaths(CONST EFI_DEVICE_PATH* dp1, CONST EFI_DEVICE_PATH* dp2)
{
	if (dp1 == NULL || dp2 == NULL)
		return -1;

	while (1) {
		UINT8 type1, type2;
		UINT8 subtype1, subtype2;
		UINT16 len1, len2;
		INTN ret;

		type1 = DevicePathType(dp1);
		type2 = DevicePathType(dp2);

		if (type1 != type2)
			return (int)type2 - (int)type1;

		subtype1 = DevicePathSubType(dp1);
		subtype2 = DevicePathSubType(dp2);

		if (subtype1 != subtype2)
			return (int)subtype1 - (int)subtype2;

		len1 = DevicePathNodeLength(dp1);
		len2 = DevicePathNodeLength(dp2);
		if (len1 != len2)
			return (int)len1 - (int)len2;

		ret = CompareMem(dp1, dp2, len1);
		if (ret != 0)
			return ret;

		if (IsDevicePathEnd(dp1))
			break;

		dp1 = (EFI_DEVICE_PATH*)((char*)dp1 + len1);
		dp2 = (EFI_DEVICE_PATH*)((char*)dp2 + len2);
	}

	return 0;
}

CHAR16*
StrDup(CONST CHAR16* Src)
{
	UINTN Size = StrSize(Src);
	CHAR16* Dest = AllocatePool(Size);
	if (Dest != NULL)
		CopyMem (Dest, Src, Size);
	return Dest;
}

/* Convert a Device Path to a string. Must be freed with FreePool(). */
CHAR16 *
ToDevicePathString(CONST EFI_DEVICE_PATH* DevicePath)
{
	CHAR16* DevicePathString = NULL;
	EFI_STATUS Status;
	EFI_DEVICE_PATH_TO_TEXT_PROTOCOL* DevicePathToText;

	Status = BS->LocateProtocol(&gEfiDevicePathToTextProtocolGuid, NULL, (VOID**)&DevicePathToText);
	if (Status == EFI_SUCCESS)
		DevicePathString = DevicePathToText->ConvertDevicePathToText(DevicePath, FALSE, FALSE);
	else
#if defined(_GNU_EFI)
		DevicePathString = DevicePathToStr((EFI_DEVICE_PATH *)DevicePath);
#else
		DevicePathString = StrDup(L"N/A (This platform is missing the DevicePathToText Protocol)");
#endif
	return DevicePathString;
}
