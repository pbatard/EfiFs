/* diskcache.h - read cache for grub_disk_read()
 *
 *  Copyright © 2026 IO-ZetZor
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef DISKCACHE_HOST_TEST
#include <grub/types.h>
#endif

#pragma once

struct _EFI_FS;

/* Serve Size bytes at absolute ByteOffset on the medium, identically to
 * reading through to the firmware, only with fewer calls. */
EFI_STATUS DiskCacheRead(struct _EFI_FS *Fs, UINT64 ByteOffset, UINTN Size, VOID *Buf);

/* Drop every cached block; called on mount and unmount, since firmware can
 * reuse a MediaId for a different medium. */
VOID DiskCacheFlush(VOID);

/* Print the counters on one line. */
VOID DiskCachePrintStats(VOID);

/* Counters, for measuring what the cache is actually doing. */
VOID DiskCacheGetStats(grub_uint64_t *Hits, grub_uint64_t *Misses,
                       grub_uint64_t *Reads, grub_uint64_t *Bypassed);
