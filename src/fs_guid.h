/* fs_guid.h - List individual filesystem GUIDs */
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

#pragma once

/* Each filesystem we service must have its own GUID */
static const struct {
	const CHAR16   *Name;
	const EFI_GUID  Guid;
	} FSGuid[] = {
		{ L"ntfs", { 0x3AD33E69, 0x7966, 0x4081, {0x9A, 0x66, 0x9B, 0xA8, 0xE5, 0x4E, 0x06, 0x4B } } },
	};

/**
 * Get the filesystem GUID according to the filesystem name
 */
static __inline const EFI_GUID 
*GetFSGuid(const CHAR16 *FSName)
{
	INTN i;

	for (i = 0; i < ARRAYSIZE(FSGuid); i++) {
		/* NB: gnu-efi's StriCmp() doesn't seem to ignore the case... */
		if (StriCmp(FSName, FSGuid[i].Name) == 0)
			return &FSGuid[i].Guid;
	}

	return NULL;
}
