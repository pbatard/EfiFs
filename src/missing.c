/* missing.c - Missing convenience calls from the EFI interface */
/*
 *  Copyright © 2014 Pete Batard <pete@akeo.ie>
 *  Based on GRUB  --  GRand Unified Bootloader
 *  Copyright © 1999-2010 Free Software Foundation, Inc.
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

VOID
strcpya(CHAR8 *dst, CONST CHAR8 *src)
{
	INTN len = strlena(src) + 1;
	CopyMem(dst, src, len);
}

CHAR8 *
strchra(const CHAR8 *s, INTN c)
{
	do {
		if (*s == c)
			return (CHAR8 *) s;
	}
	while (*s++);

	return NULL;
}

CHAR8 *
strrchra(const CHAR8 *s, INTN c)
{
	CHAR8 *p = NULL;

	do {
		if (*s == c)
			p = (CHAR8 *) s;
	}
	while (*s++);

	return p;
}
