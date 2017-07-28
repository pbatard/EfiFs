/* this.c - Reflection data that gets altered for each driver */
/*
 *  Copyright © 2014-2017 Pete Batard <pete@akeo.ie>
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

#include "config.h"
#include "driver.h"

// Uncomment the following to disable compression support for NTFS and HFS+
//#define DISABLE_COMPRESSION

#if !defined(DRIVERNAME_STR)
#define DRIVERNAME_STR STRINGIFY(DRIVERNAME)
#endif

CHAR16 *ShortDriverName = WIDEN(STRINGIFY(DRIVERNAME));
CHAR16 *FullDriverName = L"EfiFs " WIDEN(DRIVERNAME_STR)
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
GRUB_DECLARE_MOD(EXTRAMODULE2);
#endif
#if defined(EXTRAMODULE3)
GRUB_DECLARE_MOD(EXTRAMODULE3);
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
	GRUB_FS_CALL(EXTRAMODULE2, init),
#endif
#if defined(EXTRAMODULE3)
	GRUB_FS_CALL(EXTRAMODULE3, init),
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
	GRUB_FS_CALL(EXTRAMODULE2, fini),
#endif
#if defined(EXTRAMODULE3)
	GRUB_FS_CALL(EXTRAMODULE3, fini),
#endif
	NULL
};

#if defined(__MAKEWITH_GNUEFI)
// Designate the driver entrypoint
EFI_DRIVER_ENTRY_POINT(FSDriverInstall)
#endif
