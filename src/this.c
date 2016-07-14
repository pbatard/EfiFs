/* this.c - Reflection data that gets altered for each driver */
/*
 *  Copyright © 2014-2016 Pete Batard <pete@akeo.ie>
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

#include "config.h"
#include "driver.h"

// Uncomment the following to disable compression support for NTFS and HFS+
//#define DISABLE_COMPRESSION

/*
 * The following converts our GRUB driver name to a more official file system name
 */
#define affs         1
#define bfs          2
#define btrfs        3
#define exfat        4
#define ext2         5
#define hfs          6
#define hfsplus      7
#define iso9660      8
#define jfs          9
#define nilfs2      10
#define ntfs        11
#define reiserfs    12
#define sfs         13
#define udf         14
#define ufs1        15
#define ufs2        16
#define xfs         17
#define zfs         18

#if (DRIVERNAME==affs)
#define DRIVERNAME_STR L"Amiga FFS"
#elif (DRIVERNAME==bfs)
#define DRIVERNAME_STR L"BFS"
#elif (DRIVERNAME==btrfs)
#define DRIVERNAME_STR L"Btrfs"
#elif (DRIVERNAME==exfat)
#define DRIVERNAME_STR L"exFAT"
#elif (DRIVERNAME==ext2)
#define DRIVERNAME_STR L"ext2/3/4"
#elif (DRIVERNAME==hfs)
#define DRIVERNAME_STR L"HFS"
#elif (DRIVERNAME==hfsplus)
#define DRIVERNAME_STR L"HFS+"
#elif (DRIVERNAME==iso9660)
#define DRIVERNAME_STR L"ISO9660"
#elif (DRIVERNAME==jfs)
#define DRIVERNAME_STR L"JFS"
#elif (DRIVERNAME==nilfs2)
#define DRIVERNAME_STR L"NILFS2"
#elif (DRIVERNAME==ntfs)
#define DRIVERNAME_STR L"NTFS"
#elif (DRIVERNAME==reiserfs)
#define DRIVERNAME_STR L"ReiserFS"
#elif (DRIVERNAME==sfs)
#define DRIVERNAME_STR L"Amiga SFS"
#elif (DRIVERNAME==udf)
#define DRIVERNAME_STR L"UDF"
#elif (DRIVERNAME==ufs1)
#define DRIVERNAME_STR L"UFS"
#elif (DRIVERNAME==ufs2)
#define DRIVERNAME_STR L"UFS2"
#elif (DRIVERNAME==xfs)
#define DRIVERNAME_STR L"XFS"
#elif (DRIVERNAME==zfs)
#define DRIVERNAME_STR L"ZFS"
#else
#define DRIVERNAME_STR WIDEN(STRINGIFY(DRIVERNAME))
#endif

// Must be undefined for the rest of the macros to work
#undef affs
#undef bfs
#undef btrfs
#undef exfat
#undef ext2
#undef hfs
#undef hfsplus
#undef iso9660
#undef jfs
#undef nilfs2
#undef ntfs
#undef reiserfs
#undef sfs
#undef udf
#undef ufs1
#undef ufs2
#undef xfs
#undef zfs

CHAR16 *ShortDriverName = WIDEN(STRINGIFY(DRIVERNAME));
CHAR16 *FullDriverName = L"efifs " DRIVERNAME_STR
		L" driver v" WIDEN(STRINGIFY(FS_DRIVER_VERSION_MAJOR)) L"."
		WIDEN(STRINGIFY(FS_DRIVER_VERSION_MINOR)) L" ("  WIDEN(PACKAGE_STRING) L")";

// Setup generic function calls for grub_<module>_init and grub_<module>_exit
#define MAKE_FN_NAME(module, suffix) grub_ ## module ## _ ## suffix
#define GRUB_FS_CALL(module, suffix) MAKE_FN_NAME(module, suffix)
#define GRUB_DECLARE_MOD(module) \
	extern void GRUB_FS_CALL(module, init)(void); \
	extern void GRUB_FS_CALL(module, fini)(void)

// Declare all the modules we may need to access
GRUB_DECLARE_MOD(DRIVERNAME);
#if defined(COMPRESSED_DRIVERNAME) && !defined(DISABLE_COMPRESSION)
GRUB_DECLARE_MOD(COMPRESSED_DRIVERNAME);
#endif
#if defined(EXTRAMODULE)
GRUB_DECLARE_MOD(EXTRAMODULE);
#endif
#if defined(EXTRAMODULE2)
GRUB_DECLARE_MOD(EXTRAMODULE);
#endif

GRUB_MOD_INIT GrubModuleInit[] = {
	GRUB_FS_CALL(DRIVERNAME, init),
#if defined(COMPRESSED_DRIVERNAME) && !defined(DISABLE_COMPRESSION)
	GRUB_FS_CALL(COMPRESSED_DRIVERNAME, init),
#endif
#if defined(EXTRAMODULE)
	GRUB_FS_CALL(EXTRAMODULE, init),
#endif
#if defined(EXTRAMODULE2)
	GRUB_FS_CALL(EXTRAMODULE, init),
#endif
	NULL
};

GRUB_MOD_EXIT GrubModuleExit[] = {
	GRUB_FS_CALL(DRIVERNAME, fini),
#if defined(COMPRESSED_DRIVERNAME) && !defined(DISABLE_COMPRESSION)
	GRUB_FS_CALL(COMPRESSED_DRIVERNAME, fini),
#endif
#if defined(EXTRAMODULE)
	GRUB_FS_CALL(EXTRAMODULE, fini),
#endif
#if defined(EXTRAMODULE2)
	GRUB_FS_CALL(EXTRAMODULE, fini),
#endif
	NULL
};

// Designate the driver entrypoint
EFI_DRIVER_ENTRY_POINT(FSDriverInstall)
