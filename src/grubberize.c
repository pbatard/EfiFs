/* grubberize.c - The elastic binding between grub and standalone EFI */
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

#include <grub/err.h>
#include <grub/misc.h>
#include <grub/disk.h>
#include <grub/fs.h>
#include <grub/dl.h>
#include <grub/charset.h>

#include "fs_driver.h"

void grub_exit(void)
{
	ST->BootServices->Exit(EfiImageHandle, EFI_SUCCESS, 0, NULL);
	for (;;) ;
}

/* Screen I/O */
int grub_term_inputs = 0;

void grub_refresh(void) { }

int grub_getkey(void)
{
	EFI_INPUT_KEY Key;

	while (ST->ConIn->ReadKeyStroke(ST->ConIn, &Key) == EFI_NOT_READY);
	return (int) Key.UnicodeChar;
}

static void grub_xputs_dumb(const char *str)
{
	APrint((CHAR8 *)str);
}

void (*grub_xputs)(const char *str) = grub_xputs_dumb;

/* Read an EFI shell variable */
const char *grub_env_get(const char *var)
{
	EFI_STATUS Status;
	CHAR16 wVar[64], wVal[128];
	UINTN wValSize = sizeof(wVal);	/* EFI uses the size in byte... */
	static char val[128] = { 0 };

	/* ...whereas GRUB uses the size in characters */
	grub_utf8_to_utf16(wVar, ARRAYSIZE(wVar), var, grub_strlen(var), NULL);

	Status = RT->GetVariable(wVar, &ShellVariable, NULL, &wValSize, wVal);
	if (EFI_ERROR(Status))
		return NULL;

	grub_utf16_to_utf8(val, wVal, sizeof(val));

	return val;
}

/* Memory management */
void *grub_malloc(grub_size_t size)
{
	return AllocatePool(size);
}

void *grub_zalloc(grub_size_t size)
{
	return AllocateZeroPool(size);
}

void grub_free(void *ptr)
{
	FreePool(ptr);
}

/* Don't care about refcounts for a standalone EFI FS driver */
int grub_dl_ref(grub_dl_t mod) {
	return 0;
}

int grub_dl_unref(grub_dl_t mod) {
	return 0;
};

/*
 * TODO: implement the following calls
 */
grub_disk_read_hook_t grub_file_progress_hook;

grub_err_t grub_disk_read(grub_disk_t disk, grub_disk_addr_t sector,
		grub_off_t offset, grub_size_t size, void *buf)
{
	return 0;
}

/* We need to instantiate this too */
grub_fs_t grub_fs_list;
