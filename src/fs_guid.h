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
		{ L"affs",      { 0x5CAA9E30, 0x860C, 0x4E38, { 0xB3, 0x84, 0x3D, 0xC9, 0x11, 0x42, 0x2F, 0xBE } } },
		{ L"bfs",       { 0x2B193E65, 0xDE98, 0x4E46, { 0x9F, 0xCE, 0x11, 0x46, 0x0A, 0xB1, 0x14, 0x3D } } },
		{ L"btrfs",     { 0x330C2595, 0x055C, 0x46E5, { 0x8A, 0x11, 0x5E, 0x7A, 0x52, 0x34, 0xDC, 0x92 } } },
		{ L"exfat",     { 0xC5372182, 0x1AD1, 0x4955, { 0xBD, 0xC9, 0x4A, 0xBC, 0xC8, 0x2B, 0x20, 0x43 } } },
		{ L"hfs",       { 0x32BFB12F, 0x18C0, 0x4478, { 0x90, 0x4B, 0xE4, 0x66, 0x31, 0x49, 0x65, 0x39 } } },
		{ L"hfsplus",   { 0xFF3D9105, 0xE595, 0x4818, { 0x80, 0xFC, 0xB1, 0xB1, 0x5B, 0xDE, 0x15, 0x86 } } },
		{ L"jfs",       { 0x90970AA7, 0xCA99, 0x45C4, { 0xB1, 0x61, 0x15, 0x95, 0xDC, 0x63, 0x1F, 0xBA } } },
		{ L"ntfs",      { 0x3AD33E69, 0x7966, 0x4081, { 0x9A, 0x66, 0x9B, 0xA8, 0xE5, 0x4E, 0x06, 0x4B } } },
		{ L"xfs",       { 0xB1EC46ED, 0x896B, 0x4838, { 0x8B, 0x39, 0x66, 0xFF, 0x9F, 0xEE, 0x3A, 0x9A } } },
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
