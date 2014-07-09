/* grub_driver.c - Data that needs to be altered for each driver */
/*
 *  Copyright © 2014 Pete Batard <pete@akeo.ie>
 *  Based on GRUB  --  GRand Unified Bootloader
 *  Copyright © 2001-2014 Free Software Foundation, Inc.
 *  Path sanitation code by Ludwig Nussel <ludwig.nussel@suse.de>
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

#include <grub/err.h>

#include "driver.h"

CHAR16* DriverNameString = L"efifs " WIDEN(STRINGIFY(FS_DRIVER_VERSION_MAJOR)) L"."
		WIDEN(STRINGIFY(FS_DRIVER_VERSION_MINOR)) L"." WIDEN(STRINGIFY(FS_DRIVER_VERSION_MICRO))
		L" " WIDEN(STRINGIFY(DRIVERNAME)) L" driver (" WIDEN(PACKAGE_STRING) L")";

VOID
GrubDriverInit(VOID)
{
		// TODO: would be nicer to move the fs specific stuff out so we don't have to recompile fs_driver.c
	/* Register the relevant GRUB filesystem module */
	GRUB_FS_CALL(DRIVERNAME, init)();
	/* The GRUB compression routines are registered as an extra module */
	// TODO: Eventually, we could try to turn each GRUB module into their
	// own EFI driver, have them register their interface and consume that.
#if defined(EXTRAMODULE)
	GRUB_FS_CALL(EXTRAMODULE, init)();
#endif
}

VOID
GrubDriverExit(VOID)
{
	/* Unregister the relevant grub module */
	GRUB_FS_CALL(DRIVERNAME, fini)();
#if defined(EXTRAMODULE)
	GRUB_FS_CALL(EXTRAMODULE, fini)();
#endif
}
