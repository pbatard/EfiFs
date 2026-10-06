/* diskcache_host.c - host unit test for src/diskcache.c
 *
 * Drives the real diskcache.c against a fake DiskIo backed by memory and
 * compares every read against one that goes straight to the medium: every
 * alignment and size, hits, evictions, reads past the end of the medium, a
 * medium whose identity changes underneath the cache, flushes, and allocation
 * failure (which must fall back to pass-through, not fail).
 *
 *   cc -O2 -Wall -Wextra -DDISKCACHE_HOST_TEST -o diskcache_host diskcache_host.c
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

typedef uint64_t grub_uint64_t;
typedef uint32_t grub_uint32_t;
typedef uint32_t UINT32;
typedef uint64_t UINT64;
typedef uint32_t UINT16;
typedef uint8_t  UINT8;
typedef size_t   UINTN;
typedef int64_t  INT64;
typedef int      CHAR16;   /* host: so L"..." literals match CHAR16 */
typedef int      BOOLEAN;
typedef void     VOID;

#define GRUB_DISK_SECTOR_SIZE 512

typedef int EFI_STATUS;
#define EFI_SUCCESS            0
#define EFI_ERROR(s)           ((s) != 0)
#define EFI_DEVICE_ERROR       10

/* --- the slice of EFI_FS that diskcache.c uses --- */
typedef struct {
	UINT32 BlockSize;
	UINT64 LastBlock;
	UINT32 MediaId;
	BOOLEAN RemovableMedia;
} EFI_BLOCK_IO_MEDIA;

typedef struct { UINT32 Token; } EFI_DISK_IO2_TOKEN;

typedef struct {
	EFI_STATUS (*ReadDisk)(void *This, UINT32 MediaId, UINT64 Offset,
		UINTN Size, VOID *Buffer);
} EFI_DISK_IO_PROTOCOL;

typedef struct {
	EFI_STATUS (*ReadDiskEx)(void *This, UINT32 MediaId, UINT64 Offset,
		EFI_DISK_IO2_TOKEN *Token, UINTN Size, VOID *Buffer);
	EFI_BLOCK_IO_MEDIA *Media;
} EFI_DISK_IO2_PROTOCOL;

typedef struct { EFI_BLOCK_IO_MEDIA *Media; } EFI_BLOCK_IO_PROTOCOL;
typedef struct { EFI_BLOCK_IO_MEDIA *Media; } EFI_BLOCK_IO2_PROTOCOL;

typedef struct _EFI_FS {
	EFI_BLOCK_IO_PROTOCOL   *BlockIo;
	EFI_BLOCK_IO2_PROTOCOL  *BlockIo2;
	EFI_DISK_IO_PROTOCOL    *DiskIo;
	EFI_DISK_IO2_PROTOCOL   *DiskIo2;
	EFI_DISK_IO2_TOKEN       DiskIo2Token;
} EFI_FS;

/* --- pool allocation, with a switch to fail on demand --- */
static int FailAlloc;
static UINTN AllocTotal;

static VOID *AllocateZeroPoolImpl(UINTN Size);
#define AllocateZeroPool(n) AllocateZeroPoolImpl(n)
static VOID *AllocateZeroPoolImpl(UINTN Size) {
	if (FailAlloc) return NULL;
	UINTN *p = calloc(1, Size + sizeof(UINTN));
	if (!p) return NULL;
	*p = Size;
	AllocTotal += Size;
	return (VOID *)(p + 1);
}

VOID FreePool(VOID *Ptr) {
	if (!Ptr) return;
	UINTN *p = (UINTN *) Ptr - 1;
	AllocTotal -= *p;
	free(p);
}

VOID CopyMem(VOID *Dst, VOID *Src, UINTN Len) { memcpy(Dst, Src, Len); }
VOID ZeroMem(VOID *Dst, UINTN Len) { memset(Dst, 0, Len); }

/* grub/disk.h and grub/err.h pull in a lot we do not need here */
#define grub_error(e, ...) (e)

/* only referenced by DiskCachePrintStats, which the test never calls */
static void PrintDebug(const CHAR16 *Fmt, ...) { (void) Fmt; }

#include "../src/diskcache.c"

/* ------------------------------------------------------------------ */

#define MEDIUM_SIZE (8u * 1024 * 1024)

static UINT8 *Medium;
static UINT64 RawReads;          /* how many times the fake medium was hit */
static EFI_FS Fs;
static EFI_BLOCK_IO_MEDIA Media;
static EFI_DISK_IO_PROTOCOL  DiskIo;
static EFI_DISK_IO2_PROTOCOL DiskIo2;
static EFI_BLOCK_IO_PROTOCOL BlockIo;
static EFI_BLOCK_IO2_PROTOCOL BlockIo2;

static UINT32 RandState = 12345;
static UINT32 NextRand(void) {
	RandState = RandState * 1103515245u + 12345u;
	return RandState >> 8;
}

static EFI_STATUS FakeReadDisk(void *This, UINT32 MediaId, UINT64 Offset,
                               UINTN Size, VOID *Buffer) {
	(void)This; (void)MediaId;
	RawReads++;
	if (Offset > MEDIUM_SIZE || Size > MEDIUM_SIZE - Offset)
		return EFI_DEVICE_ERROR;
	memcpy(Buffer, Medium + Offset, Size);
	return EFI_SUCCESS;
}

static EFI_STATUS FakeReadDiskEx(void *This, UINT32 MediaId, UINT64 Offset,
                                 EFI_DISK_IO2_TOKEN *Token, UINTN Size, VOID *Buffer) {
	(void)Token;
	return FakeReadDisk(This, MediaId, Offset, Size, Buffer);
}

/* The uncached behaviour the cache must reproduce. */
static EFI_STATUS ReferenceRead(UINT64 Offset, UINTN Size, VOID *Buf) {
	if (Fs.DiskIo2)
		return FakeReadDiskEx(&DiskIo2, Media.MediaId, Offset, &Fs.DiskIo2Token, Size, Buf);
	return FakeReadDisk(&DiskIo, Media.MediaId, Offset, Size, Buf);
}

static void SetupMedium(void) {
	Medium = malloc(MEDIUM_SIZE);
	for (UINTN i = 0; i < MEDIUM_SIZE; i++) Medium[i] = (UINT8)NextRand();
	Media.BlockSize = 512;
	Media.LastBlock = MEDIUM_SIZE / 512 - 1;
	Media.MediaId = 7;
	DiskIo.ReadDisk = FakeReadDisk;
	DiskIo2.ReadDiskEx = FakeReadDiskEx;
	DiskIo2.Media = &Media;
	BlockIo.Media = &Media;
	BlockIo2.Media = &Media;
	Fs.BlockIo = &BlockIo;
	Fs.BlockIo2 = &BlockIo2;
	Fs.DiskIo = &DiskIo;
	Fs.DiskIo2 = &DiskIo2;
	DiskCacheFlush();
}

static int Failures;
static int Checks;

static void ExpectEq(const char *What, UINT64 Off, UINTN Len,
                     EFI_STATUS Got, EFI_STATUS Want, const UINT8 *GotBuf,
                     const UINT8 *WantBuf) {
	Checks++;
	int ok = (Got == Want);
	if (ok && Want == EFI_SUCCESS)
		ok = (memcmp(GotBuf, WantBuf, Len) == 0);
	if (!ok) {
		Failures++;
		printf("  FAIL %s off=%llu len=%zu status got=%d want=%d%s\n",
		       What, (unsigned long long) Off, (size_t) Len, Got, Want,
		       (Want == EFI_SUCCESS && Got == Want) ? " (data differs)" : "");
	}
}

/* One read, compared against the uncached reference. */
static void CheckRead(const char *What, UINT64 Off, UINTN Len) {
	UINT8 *a = malloc(Len ? Len : 1), *b = malloc(Len ? Len : 1);
	EFI_STATUS got = DiskCacheRead(&Fs, Off, Len, a);
	EFI_STATUS want = ReferenceRead(Off, Len, b);
	ExpectEq(What, Off, Len, got, want, a, b);
	free(a); free(b);
}

static void Matrix(void) {
	static const UINT64 Offs[] = {
		0, 1, 7, 511, 512, 513, 1023, 1024, 2048, 4095, 4096, 4097,
		8191, 8192, 8193, 12288, 65535, 65536, 100000, 1u << 20
	};
	static const UINTN Sizes[] = {
		1, 2, 7, 64, 511, 512, 513, 1024, 4095, 4096, 4097, 8191,
		8192, 8193, 65536, 131071, 131072, 131073, 262144, 1048576
	};
	printf("- offset x size matrix\n");
	for (UINTN i = 0; i < sizeof(Offs) / sizeof(Offs[0]); i++) {
		for (UINTN j = 0; j < sizeof(Sizes) / sizeof(Sizes[0]); j++) {
			if (Offs[i] + Sizes[j] > MEDIUM_SIZE) continue;
			CheckRead("matrix", Offs[i], Sizes[j]);
			/* again: the second pass must be served identically, from cache */
			CheckRead("matrix-repeat", Offs[i], Sizes[j]);
		}
	}
}

static void RepeatsAndEvictions(void) {
	printf("- repeated reads hit, interleaved reads evict\n");
	DiskCacheFlush();
	grub_uint64_t h0, m0, r0, b0, h1, m1, r1;
	DiskCacheGetStats(&h0, &m0, &r0, &b0);
	for (int i = 0; i < 200; i++) CheckRead("hot-block", 4096 * 8, 4096);
	DiskCacheGetStats(&h1, &m1, &r1, NULL);
	if (m1 - m0 != 1) {
		Failures++; printf("  FAIL hot block: %llu misses, expected 1\n",
		                   (unsigned long long)(m1 - m0));
	}
	Checks++;
	if (h1 - h0 != 199) {
		Failures++; printf("  FAIL hot block: %llu hits, expected 199\n",
		                   (unsigned long long)(h1 - h0));
	}
	Checks++;

	/* 400 distinct blocks: more than DISKCACHE_SLOTS, so eviction happens. */
	for (int i = 0; i < 400; i++)
		CheckRead("evicting", (UINT64)i * 4096, 4096);
	/* and the survivors must still be right */
	for (int i = 0; i < 400; i += 7)
		CheckRead("after-evict", (UINT64)i * 4096, 4096);
}

static void PastTheEnd(void) {
	printf("- reads that run off the medium behave as before\n");
	static const UINT64 Offs[] = {
		MEDIUM_SIZE - 1, MEDIUM_SIZE - 512, MEDIUM_SIZE - 4096,
		MEDIUM_SIZE, MEDIUM_SIZE + 1, MEDIUM_SIZE + 4096
	};
	static const UINTN Sizes[] = { 1, 512, 4096, 8192, 131072 };
	for (UINTN i = 0; i < sizeof(Offs) / sizeof(Offs[0]); i++)
		for (UINTN j = 0; j < sizeof(Sizes) / sizeof(Sizes[0]); j++)
			CheckRead("past-end", Offs[i], Sizes[j]);
}

static void MediumChanges(void) {
	printf("- a different medium never sees another's cached blocks\n");
	DiskCacheFlush();
	CheckRead("identity", 4096 * 3, 4096);

	/* Same MediaId, different content and size: must not alias. */
	Media.MediaId = 9;
	Media.BlockSize = 4096;
	Media.LastBlock = MEDIUM_SIZE / 4096 - 1;
	for (UINTN i = 0; i < MEDIUM_SIZE; i++) Medium[i] = (UINT8)(i * 7 + 3);
	CheckRead("identity-changed", 4096 * 3, 4096);
	CheckRead("identity-changed-2", 4096 * 3, 4096);
	CheckRead("identity-other", 4096 * 40, 4096);
}

static void Flushes(void) {
	printf("- flush mid-stream keeps every read correct\n");
	for (int i = 0; i < 30; i++) {
		CheckRead("pre-flush", (UINT64)i * 4096, 4096);
		if (i % 7 == 0) { DiskCacheFlush(); CheckRead("post-flush", (UINT64)i * 4096, 4096); }
	}
}

static void OutOfMemory(void) {
	printf("- allocation failure degrades to pass-through\n");
	DiskCacheFlush();
	FailAlloc = 1;
	for (UINTN i = 0; i < 12; i++)
		CheckRead("no-memory", 4096 * i, 4096);
	FailAlloc = 0;
	/* and it recovers once memory comes back */
	DiskCacheFlush();
	for (UINTN i = 0; i < 12; i++)
		CheckRead("memory-back", 4096 * i, 4096);
}

static void RandomSoak(void) {
	printf("- 20000 randomised reads against the reference\n");
	RandState = 999;
	for (int i = 0; i < 20000; i++) {
		UINT64 off = (UINT64)(NextRand() % (MEDIUM_SIZE - 300000));
		UINTN len = 1 + (NextRand() % 200000);
		CheckRead("soak", off, len);
	}
}

int main(void) {
	SetupMedium();
	Matrix();
	RepeatsAndEvictions();
	PastTheEnd();
	MediumChanges();
	Flushes();
	OutOfMemory();
	RandomSoak();

	printf("\n%d checks, %d failure(s)\n", Checks, Failures);
	printf("cache bytes still held: %lu (cap %d)\n",
	       (unsigned long) AllocTotal, 64 * 4096);
	return Failures ? 1 : 0;
}
