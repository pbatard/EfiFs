/* missing.c - Missing convenience calls from the EFI interface */
/*
 *  Copyright © 2014-2017 Pete Batard <pete@akeo.ie>
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

 // Microsoft's intrinsics are a major pain in the ass
 // https://stackoverflow.com/a/2945619/1069307
#if defined(_M_X64) && (defined(_MSC_VER) || defined(__c2__))
#include <stddef.h>		// For size_t

void* memset(void *, int, size_t);
#pragma intrinsic(memset)
#pragma function(memset)
void* memset(void *s, int c, size_t n)
{
	SetMem(s, (UINTN)n, (UINT8)c);
	return s;
}

void* memcpy(void *, const void *, size_t);
#pragma intrinsic(memcpy)
#pragma function(memcpy)
void* memcpy(void *s1, const void *s2, size_t n)
{
	CopyMem(s1, s2, (UINTN)n);
	return s1;
}

int memcmp(const void*, const void *, size_t);
#pragma intrinsic(memcmp)
#pragma function(memcmp)
int memcmp(const void *s1, const void *s2, size_t n)
{
	return (int)CompareMem(s1, s2, (UINTN)n);
}

void* memmove(void *s1, const void *s2, size_t n)
{
	CopyMem(s1, s2, n);
	return s1;
}
#endif

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
