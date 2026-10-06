/* diskcache.c - a read cache for grub_disk_read()
 *
 *  Copyright © 2026 IO-ZetZor
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
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

/* EfiFs reads straight through to the firmware, one request at a time, with no
 * cache in between - GRUB normally puts its sector cache in kern/disk.c, which
 * EfiFs does not link.  Metadata is what suffers: an Open() on btrfs re-walks
 * the chunk, root, FS and directory trees, re-reading the same nodes each time
 * (#56).  Bulk reads on the same volume are unaffected.
 *
 * Direct-mapped, one slot per hashed 4 KiB block, no chains: a miss simply
 * overwrites its slot.  64 lazily allocated slots, so 256 KiB at worst and
 * nothing at all until something is read.
 *
 * Invariants:
 *  - a read is never widened.  A block off the end of the medium, or a medium
 *    of unknown size, is served straight through;
 *  - a read is never failed by the cache.  A failed allocation degrades to
 *    pass-through;
 *  - a fill publishes its key after the buffer is complete, so no reader sees
 *    a partial block;
 *  - no locking.  grub_disk_read() is a leaf and firmware I/O does not
 *    re-enter it;
 *  - read-only filesystem, so nothing to invalidate on write.
 */

/* The host unit test stands in for these; see tests/diskcache_host.c. */
#ifndef DISKCACHE_HOST_TEST
#include <grub/err.h>
#include <grub/misc.h>
#include <grub/disk.h>
#include "driver.h"
#endif
#include "diskcache.h"

#ifndef DISKCACHE_BLOCK_SIZE
#define DISKCACHE_BLOCK_SIZE    (8 * GRUB_DISK_SECTOR_SIZE)
#endif
#ifndef DISKCACHE_SLOTS
#define DISKCACHE_SLOTS	        64
#endif
#ifndef DISKCACHE_BULK_BYPASS
#define DISKCACHE_BULK_BYPASS   (128 * 1024)
#endif

typedef struct {
	UINT8  *Data;
	UINT64  Block;
	UINT64  MediaBytes;
	UINT32  MediaId;
	UINT32  BlockSize;
	UINT8   Valid;
} DISKCACHE_SLOT;

static grub_uint64_t CacheHits, CacheMisses, CacheReads, CacheBypassed;
#ifndef DISKCACHE_DISABLE
static DISKCACHE_SLOT CacheSlots[DISKCACHE_SLOTS];
#endif

/* Multiply-shift: Block indices arrive in clusters, and a plain mask would
 * collide two clusters DISKCACHE_SLOTS apart. */
#ifndef DISKCACHE_DISABLE
static UINTN CacheIndex(UINT64 Block)
{
	UINT64 h = Block * 0x9E3779B97F4A7C15ULL;
	return (UINTN)((h >> 47) & (DISKCACHE_SLOTS - 1));
}
#endif

/* Media info comes from BlockIo, reads from DiskIo - as before. */
static EFI_BLOCK_IO_MEDIA *CacheMedia(EFI_FS *Fs)
{
	return (Fs->BlockIo2 != NULL) ? Fs->BlockIo2->Media : Fs->BlockIo->Media;
}

#ifndef DISKCACHE_DISABLE
static UINT64 CacheMediaBytes(EFI_FS *Fs)
{
	EFI_BLOCK_IO_MEDIA *Media = CacheMedia(Fs);
	if (Media == NULL || Media->BlockSize == 0)
		return 0;
	/* LastBlock counts BlockSize units, not 512-byte sectors. */
	return ((UINT64)Media->LastBlock + 1) * Media->BlockSize;
}
#endif

VOID DiskCacheFlush()
{
#ifndef DISKCACHE_DISABLE
	for (UINTN i = 0; i < DISKCACHE_SLOTS; i++) {
		if (CacheSlots[i].Data != NULL)
			FreePool(CacheSlots[i].Data);
		ZeroMem(&CacheSlots[i], sizeof(CacheSlots[i]));
	}
#endif
	CacheHits = CacheMisses = CacheReads = CacheBypassed = 0;
}

VOID DiskCacheGetStats(grub_uint64_t *Hits, grub_uint64_t *Misses,
                       grub_uint64_t *Reads, grub_uint64_t *Bypassed)
{
	if (Hits != NULL)
		*Hits = CacheHits;
	if (Misses != NULL)
		*Misses = CacheMisses;
	if (Reads != NULL)
		*Reads = CacheReads;
	if (Bypassed != NULL)
		*Bypassed = CacheBypassed;
}

/* Report what the cache did in terms of reads, hits and misses.
 * Requires FS_LOGGING set to DEBUG or higher. */
VOID DiskCachePrintStats()
{
#ifdef DISKCACHE_DISABLE
	return;
#else
	if (CacheReads == 0 && CacheHits == 0 && CacheBypassed == 0)
		return;
	PrintDebug(L"DiskCache: reads %llu, hits %llu, misses %llu, uncached %llu\n",
		CacheReads, CacheHits, CacheMisses, CacheBypassed);
#endif
}

/* One ReadDisk/ReadDiskEx. Offset and Size are absolute bytes. */
static EFI_STATUS DiskRawRead(EFI_FS *Fs, UINT64 ByteOffset, UINTN Size, VOID *Buf)
{
	UINT32 MediaId = CacheMedia(Fs)->MediaId;

	if (Fs->DiskIo2 != NULL) {
		return Fs->DiskIo2->ReadDiskEx(Fs->DiskIo2, MediaId,
			ByteOffset, &(Fs->DiskIo2Token), Size, Buf);
	}
	return Fs->DiskIo->ReadDisk(Fs->DiskIo, MediaId,
		ByteOffset, (UINTN)Size, Buf);
}

/* Serve [InBlk, InBlk + Len] of one block, caching the whole block when we can. */
#ifndef DISKCACHE_DISABLE
static EFI_STATUS DiskCacheBlock(EFI_FS *Fs, UINT64 Block, UINTN InBlk, UINTN Len,
                                 VOID *Dst, UINT64 MediaBytes)
{
	EFI_STATUS Status;
	DISKCACHE_SLOT *Slot = &CacheSlots[CacheIndex(Block)];
	EFI_BLOCK_IO_MEDIA *Media = CacheMedia(Fs);
	UINT64 BlockBase = Block * DISKCACHE_BLOCK_SIZE;

	if (Slot->Valid && Slot->Block == Block && Slot->MediaId == Media->MediaId &&
		Slot->BlockSize == Media->BlockSize && Slot->MediaBytes == MediaBytes) {
		CopyMem(Dst, Slot->Data + InBlk, Len);
		CacheHits++;
		return EFI_SUCCESS;
	}

	/* A block hanging off the end of the medium, or a medium of unknown size,
	 * is served straight through: never widen a read. */
	if (MediaBytes == 0 || BlockBase + DISKCACHE_BLOCK_SIZE > MediaBytes) {
		CacheBypassed++;
		CacheReads++;
		return DiskRawRead(Fs, BlockBase + InBlk, Len, Dst);
	}

	/* Free whatever this slot held before reusing the buffer. */
	if (Slot->Data != NULL && (!Slot->Valid || Slot->Block != Block ||
		Slot->MediaId != Media->MediaId || Slot->BlockSize != Media->BlockSize ||
		Slot->MediaBytes != MediaBytes)) {
		FreePool(Slot->Data);
		Slot->Data = NULL;
	}
	if (Slot->Data == NULL) {
		Slot->Data = AllocateZeroPool(DISKCACHE_BLOCK_SIZE);
		Slot->Valid = 0;
		if (Slot->Data == NULL) {
			/* Out of memory: pass through rather than fail. */
			CacheBypassed++;
			CacheReads++;
			return DiskRawRead(Fs, BlockBase + InBlk, Len, Dst);
		}
	}

	CacheMisses++;
	CacheReads++;
	Status = DiskRawRead(Fs, BlockBase, DISKCACHE_BLOCK_SIZE, Slot->Data);
	if (EFI_ERROR(Status)) {
		/* Never publish a half-read block. */
		FreePool(Slot->Data);
		Slot->Data = NULL;
		Slot->Valid = 0;
		return Status;
	}

	/* Publish only once the buffer is complete. */
	Slot->Block = Block;
	Slot->MediaId = Media->MediaId;
	Slot->BlockSize = Media->BlockSize;
	Slot->MediaBytes = MediaBytes;
	Slot->Valid = 1;

	CopyMem(Dst, Slot->Data + InBlk, Len);
	return EFI_SUCCESS;
}
#endif

/* Cached replacement for the body of grub_disk_read().
 * DISKCACHE_DISABLE builds a counting pass-through instead, so both variants
 * can report firmware reads for the same workload. */
EFI_STATUS DiskCacheRead(EFI_FS *Fs, UINT64 ByteOffset, UINTN Size, VOID *Buf)
{
	if (Size == 0)
		return EFI_SUCCESS;

#ifdef DISKCACHE_DISABLE
	CacheReads++;
	return DiskRawRead(Fs, ByteOffset, Size, Buf);
#else
	UINT8 *Out = (UINT8 *) Buf;
	UINT64 MediaBytes = CacheMediaBytes(Fs);

	if (MediaBytes != 0 && ByteOffset + Size > MediaBytes) {
		/* Past the end of the medium: let the firmware produce the error so
		 * the behaviour matches the uncached path exactly. */
		CacheBypassed++;
		CacheReads++;
		return DiskRawRead(Fs, ByteOffset, Size, Buf);
	}

	if (Size >= DISKCACHE_BULK_BYPASS) {
		CacheBypassed++;
		CacheReads++;
		return DiskRawRead(Fs, ByteOffset, Size, Buf);
	}

	UINT64 Block = ByteOffset / DISKCACHE_BLOCK_SIZE;
	UINT64 End = ByteOffset + Size;
	while (ByteOffset < End) {
		EFI_STATUS Status;
		UINTN InBlk = (UINTN)(ByteOffset - Block * DISKCACHE_BLOCK_SIZE);
		UINTN Len = DISKCACHE_BLOCK_SIZE - InBlk;
		if (Len > End - ByteOffset)
			Len = (UINTN)(End - ByteOffset);
		Status = DiskCacheBlock(Fs, Block, InBlk, Len, Out, MediaBytes);
		if (EFI_ERROR(Status))
			return Status;
		Out += Len;
		ByteOffset += Len;
		Block++;
	}

	return EFI_SUCCESS;
#endif /* DISKCACHE_DISABLE */
}
