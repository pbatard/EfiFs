/* grub.c - The elastic binding between grub and standalone EFI */
/*
 *  Copyright © 2014-2017 Pete Batard <pete@akeo.ie>
 *  Based on GRUB, glibc and additional software:
 *  Copyright © 2001-2014 Free Software Foundation, Inc.
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

#include <grub/err.h>
#include <grub/misc.h>
#include <grub/file.h>
#include <grub/crypto.h>

#include "driver.h"

grub_file_filter_t EXPORT_VAR(grub_file_filters)[GRUB_FILE_FILTER_MAX];

void
grub_exit(void)
{
	ST->BootServices->Exit(EfiImageHandle, EFI_SUCCESS, 0, NULL);
	for (;;) ;
}

/* Screen I/O */
struct grub_term_input *EXPORT_VAR(grub_term_inputs) = NULL;

void
grub_refresh(void) { }

int
grub_getkey(void)
{
	EFI_INPUT_KEY Key;

	while (ST->ConIn->ReadKeyStroke(ST->ConIn, &Key) == EFI_NOT_READY);
	return (int) Key.UnicodeChar;
}

static void
grub_xputs_dumb(const char *str)
{
	APrint((CHAR8 *)str);
}

void (*grub_xputs)(const char *str) = grub_xputs_dumb;

/* Read an EFI shell variable */
const char *
grub_env_get(const char *var)
{
	EFI_STATUS Status;
	CHAR16 Var[64], Val[128];
	UINTN ValSize = sizeof(Val);
	static char val[128] = { 0 };

	Status = Utf8ToUtf16NoAlloc((CHAR8 *) var, Var, ARRAYSIZE(Var));
	if (EFI_ERROR(Status)) {
		PrintStatusError(Status, L"Could not convert variable name to UTF-16");
		return NULL;
	}

	Status = RT->GetVariable(Var, &ShellVariable, NULL, &ValSize, Val);
	if (EFI_ERROR(Status))
		return NULL;

	Status = Utf16ToUtf8NoAlloc(Val, (CHAR8 *) val, sizeof(val));
	if (EFI_ERROR(Status)) {
		PrintStatusError(Status, L"Could not convert value '%s' to UTF-8", Val);
		return NULL;
	}

	return val;
}

/* Memory management
 * NB: We must keep track of the size allocated for grub_realloc
 */
void *
grub_malloc(grub_size_t size)
{
	grub_size_t *ptr;

	ptr = (grub_size_t *) AllocatePool((UINTN)(size + sizeof(grub_size_t)));

	if (ptr != NULL)
		*ptr++ = size;

	return (void *) ptr;
}

void *
grub_zalloc(grub_size_t size)
{
	grub_size_t *ptr;

	ptr = (grub_size_t *) AllocateZeroPool((UINTN)(size + sizeof(grub_size_t)));

	if (ptr != NULL)
		*ptr++ = size;

	return (void *) ptr;
}

void
grub_free(void *p)
{
	grub_size_t *ptr = (grub_size_t *) p;

	if (ptr != NULL) {
		ptr = &ptr[-1];
		FreePool(ptr);
	}
}

void *
grub_realloc(void *p, grub_size_t new_size)
{
	grub_size_t *ptr = (grub_size_t *) p;

	if (ptr != NULL) {
		ptr = &ptr[-1];
#if defined(__MAKEWITH_GNUEFI)
		ptr = ReallocatePool(ptr, (UINTN)*ptr, (UINTN)(new_size + sizeof(grub_size_t)));
#else
		ptr = ReallocatePool((UINTN)*ptr, (UINTN)(new_size + sizeof(grub_size_t)), ptr);
#endif
		if (ptr != NULL)
			*ptr++ = new_size;
	}
	return ptr;
}

/* Convert a grub_err_t to EFI_STATUS */
EFI_STATUS
GrubErrToEFIStatus(grub_err_t err)
{
	// The following are defined in EFI but unused here:
	// EFI_BAD_BUFFER_SIZE
	// EFI_WRITE_PROTECTED
	// EFI_VOLUME_FULL
	// EFI_MEDIA_CHANGED
	// EFI_NO_MEDIA
	// EFI_NOT_STARTED
	// EFI_ALREADY_STARTED
	// EFI_ABORTED
	// EFI_END_OF_MEDIA
	// EFI_NO_RESPONSE
	// EFI_PROTOCOL_ERROR
	// EFI_INCOMPATIBLE_VERSION

	if ((grub_errno != 0) && (LogLevel > FS_LOGLEVEL_INFO))
		/* NB: Calling grub_print_error() will reset grub_errno */
		grub_print_error();

	switch (err) {
	case GRUB_ERR_NONE:
		return EFI_SUCCESS;

	case GRUB_ERR_BAD_MODULE:
		return EFI_LOAD_ERROR;

	case GRUB_ERR_OUT_OF_RANGE:
		return EFI_BUFFER_TOO_SMALL;

	case GRUB_ERR_OUT_OF_MEMORY:
	case GRUB_ERR_SYMLINK_LOOP:
		return EFI_OUT_OF_RESOURCES;

	case GRUB_ERR_BAD_FILE_TYPE:
		return EFI_NO_MAPPING;

	case GRUB_ERR_FILE_NOT_FOUND:
	case GRUB_ERR_UNKNOWN_DEVICE:
	case GRUB_ERR_UNKNOWN_FS:
		return EFI_NOT_FOUND;

	case GRUB_ERR_FILE_READ_ERROR:
	case GRUB_ERR_BAD_DEVICE:
	case GRUB_ERR_READ_ERROR:
	case GRUB_ERR_WRITE_ERROR:
	case GRUB_ERR_IO:
		return EFI_DEVICE_ERROR;

	case GRUB_ERR_BAD_PART_TABLE:
	case GRUB_ERR_BAD_FS:
		return EFI_VOLUME_CORRUPTED;

	case GRUB_ERR_BAD_FILENAME:
	case GRUB_ERR_BAD_ARGUMENT:
	case GRUB_ERR_BAD_NUMBER:
	case GRUB_ERR_UNKNOWN_COMMAND:
	case GRUB_ERR_INVALID_COMMAND:
		return EFI_INVALID_PARAMETER;

	case GRUB_ERR_NOT_IMPLEMENTED_YET:
		return EFI_UNSUPPORTED;

	case GRUB_ERR_TIMEOUT:
		return EFI_TIMEOUT;

	case GRUB_ERR_ACCESS_DENIED:
		return EFI_ACCESS_DENIED;

	case GRUB_ERR_WAIT:
		return EFI_NOT_READY;

	case GRUB_ERR_EXTRACTOR:
	case GRUB_ERR_BAD_COMPRESSED_DATA:
		return EFI_CRC_ERROR;

	case GRUB_ERR_EOF:
		return EFI_END_OF_FILE;

	case GRUB_ERR_BAD_SIGNATURE:
		return EFI_SECURITY_VIOLATION;

	default:
		return EFI_NOT_FOUND;
	}
}

/* The following is adapted from glibc's (offtime.c, etc.)
 */

/* How many days come before each month (0-12). */
static const unsigned short int __mon_yday[2][13] = {
	/* Normal years.  */
	{ 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
	/* Leap years.  */
	{ 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
};

/* Nonzero if YEAR is a leap year (every 4 years,
   except every 100th isn't, and every 400th is). */
#define __isleap(year) \
	((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))

#define SECS_PER_HOUR         (60 * 60)
#define SECS_PER_DAY          (SECS_PER_HOUR * 24)
#define DIV(a, b)             ((a) / (b) - ((a) % (b) < 0))
#define LEAPS_THRU_END_OF(y)  (DIV (y, 4) - DIV (y, 100) + DIV (y, 400))

/* Compute an EFI_TIME representation of a GRUB's mtime_t */
VOID
GrubTimeToEfiTime(const INT32 t, EFI_TIME *tp)
{
	INT32 days, rem, y;
	const unsigned short int *ip;

	days = t / SECS_PER_DAY;
	rem = t % SECS_PER_DAY;
	while (rem < 0) {
		rem += SECS_PER_DAY;
		--days;
	}
	while (rem >= SECS_PER_DAY) {
		rem -= SECS_PER_DAY;
		++days;
	}
	tp->Hour = (UINT8) (rem / SECS_PER_HOUR);
	rem %= SECS_PER_HOUR;
	tp->Minute = (UINT8) (rem / 60);
	tp->Second = (UINT8) (rem % 60);
	y = 1970;

	while (days < 0 || days >= (__isleap (y) ? 366 : 365)) {
		/* Guess a corrected year, assuming 365 days per year. */
		INT32 yg = y + days / 365 - (days % 365 < 0);

		/* Adjust DAYS and Y to match the guessed year. */
		days -= ((yg - y) * 365
			+ LEAPS_THRU_END_OF (yg - 1)
			- LEAPS_THRU_END_OF (y - 1));
		y = yg;
	}
	tp->Year = (UINT16) y;
	ip = __mon_yday[__isleap(y)];
	for (y = 11; days < (long int) ip[y]; --y)
		continue;
	days -= ip[y];
	tp->Month = (UINT8) (y + 1);
	tp->Day = (UINT8) (days + 1);
}

/* Need to reimplement a gcrypt compatible CRC32 for latest gzio.c
 * but heck if I'm going to lose 1 KB of space over it!
 * TODO: Move this to an EXTRAMODULE?
 */
static grub_int32_t *crc32_table;

typedef struct {
	grub_uint32_t CRC;
	grub_uint8_t buf[4];
} CRC_CONTEXT;

static grub_uint32_t
update_crc32(grub_uint32_t crc, const void *buf_arg, size_t len)
{
	const char *buf = buf_arg;
	size_t n;

	for (n = 0; n < len; n++)
		crc = crc32_table[(crc ^ buf[n]) & 0xff] ^ (crc >> 8);

	return crc;
}

static grub_uint32_t
reflect(grub_uint32_t ref, int len)
{
	grub_uint32_t result = 0;
	int i;

	for (i = 1; i <= len; i++) {
		if (ref & 1)
			result |= 1 << (len - i);
		ref >>= 1;
	}

	return result;
}

static void
crc32_init(void *context)
{
	CRC_CONTEXT *ctx = (CRC_CONTEXT *)context;
	ctx->CRC = 0 ^ 0xffffffffL;

	crc32_table = grub_malloc(256 * sizeof(grub_int32_t));
	grub_uint32_t polynomial = 0x1edc6f41;
	int i, j;
	for (i = 0; i < 256; i++) {
		crc32_table[i] = reflect(i, 8) << 24;
		for (j = 0; j < 8; j++)
			crc32_table[i] = (crc32_table[i] << 1) ^
			(crc32_table[i] & (1 << 31) ? polynomial : 0);
		crc32_table[i] = reflect(crc32_table[i], 32);
	}
}

static void
crc32_write(void *context, const void *inbuf, size_t inlen)
{
	CRC_CONTEXT *ctx = (CRC_CONTEXT *)context;
	if (!inbuf)
		return;
	ctx->CRC = update_crc32(ctx->CRC, inbuf, inlen);
}

static grub_uint8_t *
crc32_read(void *context)
{
	CRC_CONTEXT *ctx = (CRC_CONTEXT *)context;
	return ctx->buf;
}

static void
crc32_final(void *context)
{
	CRC_CONTEXT *ctx = (CRC_CONTEXT *)context;
	ctx->CRC ^= 0xffffffffL;
	ctx->buf[0] = (ctx->CRC >> 24) & 0xFF;
	ctx->buf[1] = (ctx->CRC >> 16) & 0xFF;
	ctx->buf[2] = (ctx->CRC >> 8) & 0xFF;
	ctx->buf[3] = (ctx->CRC) & 0xFF;
	grub_free(crc32_table);
}

gcry_md_spec_t _gcry_digest_spec_crc32 =
{
	"CRC32", NULL, 0, NULL, 4,
	crc32_init, crc32_write, crc32_final, crc32_read,
	sizeof(CRC_CONTEXT)
};
