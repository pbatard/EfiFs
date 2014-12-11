/* this.c - Reflection data that must be altered for each driver */
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
#include <efilib.h>

#include "config.h"
#include "driver.h"

CHAR16 *ShortDriverName = WIDEN(STRINGIFY(DRIVERNAME));
CHAR16 *FullDriverName = L"efifs " WIDEN(STRINGIFY(FS_DRIVER_VERSION_MAJOR)) L"."
		WIDEN(STRINGIFY(FS_DRIVER_VERSION_MINOR)) L"." WIDEN(STRINGIFY(FS_DRIVER_VERSION_MICRO))
		L" " WIDEN(STRINGIFY(DRIVERNAME)) L" driver (" WIDEN(PACKAGE_STRING) L")";

// Setup generic function calls for grub_<module>_init and grub_<module>_exit
#define MAKE_FN_NAME(module, suffix) grub_ ## module ## _ ## suffix
#define GRUB_FS_CALL(module, suffix) MAKE_FN_NAME(module, suffix)
#define GRUB_DECLARE_MOD(module) \
	extern void GRUB_FS_CALL(module, init)(void); \
	extern void GRUB_FS_CALL(module, fini)(void)

#define EVAL(x) x
#define APPEND(x, y) EVAL(x) ## y

// Declare all the modules we may need to access
GRUB_DECLARE_MOD(DRIVERNAME);
#if defined(ENABLE_COMPRESSION)
GRUB_DECLARE_MOD(APPEND(DRIVERNAME, comp));
#endif
#if defined(EXTRAMODULE)
GRUB_DECLARE_MOD(EXTRAMODULE);
#endif
#if defined(EXTRAMODULE2)
GRUB_DECLARE_MOD(EXTRAMODULE);
#endif

GRUB_MOD_INIT GrubModuleInit[] = {
	GRUB_FS_CALL(DRIVERNAME, init),
#if defined(ENABLE_COMPRESSION)
	GRUB_FS_CALL(APPEND(DRIVERNAME, comp), init),
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
#if defined(ENABLE_COMPRESSION)
	GRUB_FS_CALL(APPEND(DRIVERNAME, comp), fini),
#endif
#if defined(EXTRAMODULE)
	GRUB_FS_CALL(EXTRAMODULE, fini),
#endif
#if defined(EXTRAMODULE2)
	GRUB_FS_CALL(EXTRAMODULE, fini),
#endif
	NULL
};
