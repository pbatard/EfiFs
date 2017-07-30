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

#include "driver.h"

// Microsoft's intrinsics are a major pain in the ass
// https://stackoverflow.com/a/2945619/1069307
#if defined(_MSC_VER) || defined(__c2__)
#if !defined(__MAKEWITH_GNUEFI) || defined(_M_X64)
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

INT64 _allmul(INT64 a, INT64 b)
{
	INT64 _a = (a>=0)?a:-a, _b = (b>=0)?b:-b;
	if (((a > 0) & (b < 0)) || ((a < 0) && (b > 0)))
		return -1LL * MultU64x32(_a, (UINTN)_b);
	return MultU64x32(_a, (UINTN)_b);
}

INT64 _allshl(INT64 a, INTN b)
{
	return (b >= 0) ? (INT64)LShiftU64((UINT64)a, (UINTN)b) :
		(INT64)RShiftU64((UINT64)a, (UINTN)-b);
}

INT64 _allshr(INT64 a, INTN b)
{
	return (b >= 0) ? (INT64)RShiftU64((UINT64)a, (UINTN)b) :
		(INT64)LShiftU64((UINT64)a, (UINTN)-b);
}

UINT64 _aullshr(UINT64 a, INTN b)
{
	return (b >= 0) ? RShiftU64(a, (UINTN)b) :
		LShiftU64(a, (UINTN)-b);
}
#endif
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
