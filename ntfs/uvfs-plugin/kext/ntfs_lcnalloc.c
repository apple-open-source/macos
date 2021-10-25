/*
 * ntfs_lcnalloc.c - NTFS kernel cluster (de)allocation code.
 *
 * Copyright (c) 2006-2011 Anton Altaparmakov.  All Rights Reserved.
 * Portions Copyright (c) 2006-2011 Apple Inc.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution. 
 * 3. Neither the name of Apple Inc. ("Apple") nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ALTERNATIVELY, provided that this notice and licensing terms are retained in
 * full, this file may be redistributed and/or modified under the terms of the
 * GNU General Public License (GPL) Version 2, in which case the provisions of
 * that version of the GPL will apply to you instead of the license terms
 * above.  You can obtain a copy of the GPL Version 2 at
 * http://developer.apple.com/opensource/licenses/gpl-2.txt.
 */

#include <sys/buf.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/ucred.h>
#include <sys/vnode.h>

#include <string.h>

#ifdef KERNEL
#include <libkern/OSMalloc.h>

#include <kern/debug.h>
#include <kern/locks.h>
#endif

#include "ntfs.h"
#include "ntfs_attr.h"
#include "ntfs_bitmap.h"
#include "ntfs_debug.h"
#include "ntfs_inode.h"
#include "ntfs_lcnalloc.h"
#include "ntfs_page.h"
#include "ntfs_runlist.h"
#include "ntfs_types.h"
#include "ntfs_volume.h"

static errno_t ntfs_cluster_free_from_rl_nolock(ntfs_volume *vol,
		ntfs_rl_element *rl, const VCN start_vcn, s64 count,
		s64 *nr_freed);

/**
 * ntfs_cluster_alloc - allocate clusters on an ntfs volume
 * @vol:		mounted ntfs volume on which to allocate the clusters
 * @start_vcn:		vcn to use for the first allocated cluster
 * @count:		number of clusters to allocate
 * @start_lcn:		starting lcn at which to allocate the clusters (or -1)
 * @zone:		zone from which to allocate the clusters
 * @is_extension:	if true, this is an attribute extension
 * @runlist:		destination runlist to return the allocated clusters in
 *
 * Allocate @count clusters preferably starting at cluster @start_lcn or at the
 * current allocator position if @start_lcn is -1, on the mounted ntfs volume
 * @vol.  @zone is either DATA_ZONE for allocation of normal clusters or
 * MFT_ZONE for allocation of clusters for the master file table, i.e. the
 * $MFT/$DATA attribute.
 *
 * @start_vcn specifies the vcn of the first allocated cluster.  This makes
 * merging the resulting runlist with the old runlist easier.
 *
 * If @is_extension is true, the caller is allocating clusters to extend an
 * attribute and if it is false, the caller is allocating clusters to fill a
 * hole in an attribute.  Practically the difference is that if @is_extension
 * is true the returned runlist will be terminated with LCN_ENOENT and if
 * @is_extension is false the runlist will be terminated with
 * LCN_RL_NOT_MAPPED.
 *
 * On success return 0 and set up @runlist to describe the allocated clusters.
 *
 * On error return the error code.
 *
 * Notes on the allocation algorithm
 * =================================
 *
 * There are two data zones.  First is the area between the end of the mft zone
 * and the end of the volume, and second is the area between the start of the
 * volume and the start of the mft zone.  On unmodified/standard NTFS 1.x
 * volumes, the second data zone does not exist due to the mft zone being
 * expanded to cover the start of the volume in order to reserve space for the
 * mft bitmap attribute.
 *
 * This is not the prettiest function but the complexity stems from the need of
 * implementing the mft vs data zoned approach and from the fact that we have
 * access to the lcn bitmap in portions of up to 8192 bytes at a time, so we
 * need to cope with crossing over boundaries of two buffers.  Further, the
 * fact that the allocator allows for caller supplied hints as to the location
 * of where allocation should begin and the fact that the allocator keeps track
 * of where in the data zones the next natural allocation should occur,
 * contribute to the complexity of the function.  But it should all be
 * worthwhile, because this allocator should: 1) be a full implementation of
 * the MFT zone approach used by Windows NT, 2) cause reduction in
 * fragmentation, and 3) be speedy in allocations (the code is not optimized
 * for speed, but the algorithm is, so further speed improvements are probably
 * possible).
 *
 * FIXME: We should be monitoring cluster allocation and increment the MFT zone
 * size dynamically but this is something for the future.  We will just cause
 * heavier fragmentation by not doing it and I am not even sure Windows would
 * grow the MFT zone dynamically, so it might even be correct not to do this.
 * The overhead in doing dynamic MFT zone expansion would be very large and
 * unlikely worth the effort. (AIA)
 *
 * TODO: I have added in double the required zone position pointer wrap around
 * logic which can be optimized to having only one of the two logic sets.
 * However, having the double logic will work fine, but if we have only one of
 * the sets and we get it wrong somewhere, then we get into trouble, so
 * removing the duplicate logic requires _very_ careful consideration of _all_
 * possible code paths.  So at least for now, I am leaving the double logic -
 * better safe than sorry... (AIA)
 *
 * Locking: - The volume lcn bitmap must be unlocked on entry and is unlocked
 *	      on return.
 *	    - This function takes the volume lcn bitmap lock for writing and
 *	      modifies the bitmap contents.
 *	    - The lock of the runlist @runlist is not touched thus the caller
 *	      is responsible for locking it for writing if needed.
 */
errno_t ntfs_cluster_alloc(ntfs_volume *vol, const VCN start_vcn,
		const s64 count, const LCN start_lcn,
		const NTFS_CLUSTER_ALLOCATION_ZONES zone,
		const BOOL is_extension, ntfs_runlist *runlist)
{
	LCN zone_start, zone_end, bmp_pos, bmp_initial_pos, last_read_pos, lcn;
	LCN prev_lcn = 0, prev_run_len = 0, mft_zone_size;
	s64 clusters, data_size;
	ntfs_inode *lcnbmp_ni;
	ntfs_rl_element *rl = NULL;
	upl_t upl = NULL;
	upl_page_info_array_t pl = NULL;
	u8 *b, *byte;
	int rlpos, rlsize, bsize;
	errno_t err;
	u8 pass, done_zones, search_zone, bit;
	BOOL need_writeback = FALSE;

	ntfs_debug("Entering for start_vcn 0x%llx, count 0x%llx, start_lcn "
			"0x%llx, zone %s_ZONE.", (unsigned long long)start_vcn,
			(unsigned long long)count,
			(unsigned long long)start_lcn,
			zone == MFT_ZONE ? "MFT" : "DATA");
	if (!vol)
		panic("%s(): !vol\n", __FUNCTION__);
	lcnbmp_ni = vol->lcnbmp_ni;
	if (!lcnbmp_ni)
		panic("%s(): !lcnbmp_ni\n", __FUNCTION__);
	if (start_vcn < 0)
		panic("%s(): start_vcn < 0\n", __FUNCTION__);
	if (count < 0)
		panic("%s(): count < 0\n", __FUNCTION__);
	if (start_lcn < -1)
		panic("%s(): start_lcn < -1\n", __FUNCTION__);
	if (zone < FIRST_ZONE)
		panic("%s(): zone < FIRST_ZONE\n", __FUNCTION__);
	if (zone > LAST_ZONE)
		panic("%s(): zone > LAST_ZONE\n", __FUNCTION__);
	/* Return NULL if @count is zero. */
	if (!count) {
		if (runlist->alloc)
			OSFree(runlist->rl, runlist->alloc, ntfs_malloc_tag);
		runlist->rl = NULL;
		runlist->elements = 0;
		runlist->alloc = 0;
		return 0;
	}
	/* Take the lcnbmp lock for writing. */
	lck_rw_lock_exclusive(&vol->lcnbmp_lock);
	err = vnode_get(lcnbmp_ni->vn);
	if (err) {
		ntfs_error(vol->mp, "Failed to get vnode for $Bitmap.");
		lck_rw_unlock_exclusive(&vol->lcnbmp_lock);
		return err;
	}
	lck_rw_lock_shared(&lcnbmp_ni->lock);
	/*
	 * If no specific @start_lcn was requested, use the current data zone
	 * position, otherwise use the requested @start_lcn but make sure it
	 * lies outside the mft zone.  Also set done_zones to 0 (no zones done)
	 * and pass depending on whether we are starting inside a zone (1) or
	 * at the beginning of a zone (2).  If requesting from the MFT_ZONE,
	 * we either start at the current position within the mft zone or at
	 * the specified position.  If the latter is out of bounds then we start
	 * at the beginning of the MFT_ZONE.
	 */
	done_zones = 0;
	pass = 1;
	/*
	 * zone_start and zone_end are the current search range.  search_zone
	 * is 1 for mft zone, 2 for data zone 1 (end of mft zone till end of
	 * volume) and 4 for data zone 2 (start of volume till start of mft
	 * zone).
	 */
	zone_start = start_lcn;
	if (zone_start < 0) {
		if (zone == DATA_ZONE)
			zone_start = vol->data1_zone_pos;
		else
			zone_start = vol->mft_zone_pos;
		if (!zone_start) {
			/*
			 * Zone starts at beginning of volume which means a
			 * single pass is sufficient.
			 */
			pass = 2;
		}
	} else if (zone == DATA_ZONE && zone_start >= vol->mft_zone_start &&
			zone_start < vol->mft_zone_end) {
		zone_start = vol->mft_zone_end;
		/*
		 * Starting at beginning of data1_zone which means a single
		 * pass in this zone is sufficient.
		 */
		pass = 2;
	} else if (zone == MFT_ZONE && (zone_start < vol->mft_zone_start ||
			zone_start >= vol->mft_zone_end)) {
		zone_start = vol->mft_lcn;
		if (!vol->mft_zone_end)
			zone_start = 0;
		/*
		 * Starting at beginning of volume which means a single pass
		 * is sufficient.
		 */
		pass = 2;
	}
	if (zone == MFT_ZONE) {
		zone_end = vol->mft_zone_end;
		search_zone = 1;
	} else /* if (zone == DATA_ZONE) */ {
		/* Skip searching the mft zone. */
		done_zones |= 1;
		if (zone_start >= vol->mft_zone_end) {
			zone_end = vol->nr_clusters;
			search_zone = 2;
		} else {
			zone_end = vol->mft_zone_start;
			search_zone = 4;
		}
	}
	/*
	 * bmp_pos is the current bit position inside the bitmap.  We use
	 * bmp_initial_pos to determine whether or not to do a zone switch.
	 */
	bmp_pos = bmp_initial_pos = zone_start;
	/* Loop until all clusters are allocated, i.e. clusters == 0. */
	clusters = count;
	rlpos = rlsize = 0;
	lck_spin_lock(&lcnbmp_ni->size_lock);
	data_size = ubc_getsize(lcnbmp_ni->vn);
	if (data_size != lcnbmp_ni->data_size)
		panic("%s(): data_size != lcnbmp_ni->data_size\n",
				__FUNCTION__);
	lck_spin_unlock(&lcnbmp_ni->size_lock);
	while (1) {
		ntfs_debug("Start of outer while loop: done_zones 0x%x, "
				"search_zone %d, pass %d, zone_start 0x%llx, "
				"zone_end 0x%llx, bmp_initial_pos 0x%llx, "
				"bmp_pos 0x%llx, rlpos %d, rlsize %d.",
				done_zones, search_zone, pass,
				(unsigned long long)zone_start,
				(unsigned long long)zone_end,
				(unsigned long long)bmp_initial_pos,
				(unsigned long long)bmp_pos, rlpos, rlsize);
		/* Loop until we run out of free clusters. */
		last_read_pos = bmp_pos >> 3;
		ntfs_debug("last_read_pos 0x%llx.",
				(unsigned long long)last_read_pos);
		if (last_read_pos > data_size) {
			ntfs_debug("End of attribute reached.  "
					"Skipping to zone_pass_done.");
			goto zone_pass_done;
		}
		if (upl) {
			ntfs_page_unmap(lcnbmp_ni, upl, pl, need_writeback);
			if (need_writeback) {
				ntfs_debug("Marking page dirty.");
				need_writeback = FALSE;
			}
		}
		err = ntfs_page_map(lcnbmp_ni, last_read_pos & ~PAGE_MASK_64,
				&upl, &pl, &b, TRUE);
		if (err) {
			ntfs_error(vol->mp, "Failed to map page.");
			upl = NULL;
			goto out;
		}
		bsize = last_read_pos & PAGE_MASK;
		b += bsize;
		bsize = PAGE_SIZE - bsize;
		if (last_read_pos + bsize > data_size)
			bsize = (int)(data_size - last_read_pos);
		bsize <<= 3;
		lcn = bmp_pos & 7;
		bmp_pos &= ~(LCN)7;
		ntfs_debug("Before inner while loop: bsize %d, lcn 0x%llx, "
				"bmp_pos 0x%llx, need_writeback is %s.", bsize,
				(unsigned long long)lcn,
				(unsigned long long)bmp_pos,
				need_writeback ? "true" : "false");
		while (lcn < bsize && lcn + bmp_pos < zone_end) {
			byte = b + (lcn >> 3);
			ntfs_debug("In inner while loop: bsize %d, lcn "
					"0x%llx, bmp_pos 0x%llx, "
					"need_writeback is %s, byte ofs 0x%x, "
					"*byte 0x%x.", bsize,
					(unsigned long long)lcn,
					(unsigned long long)bmp_pos,
					need_writeback ? "true" : "false",
					(unsigned)(lcn >> 3), (unsigned)*byte);
			/* Skip full bytes. */
			if (*byte == 0xff) {
				lcn = (lcn + 8) & ~(LCN)7;
				ntfs_debug("Continuing while loop 1.");
				continue;
			}
			bit = 1 << (lcn & 7);
			ntfs_debug("bit 0x%x.", bit);
			/* If the bit is already set, go onto the next one. */
			if (*byte & bit) {
				lcn++;
				ntfs_debug("Continuing while loop 2.");
				continue;
			}
			/*
			 * Allocate more memory if needed, including space for
			 * the terminator element.
			 * ntfs_malloc_nofs() operates on whole pages only.
			 */
			if ((rlpos + 2) * (int)sizeof(*rl) > rlsize) {
				ntfs_rl_element *rl2;

				ntfs_debug("Reallocating memory.");
				rl2 = OSMalloc(rlsize + NTFS_ALLOC_BLOCK,
						ntfs_malloc_tag);
				if (!rl2) {
					err = ENOMEM;
					ntfs_error(vol->mp, "Failed to "
							"allocate memory.");
					goto out;
				}
				if (!rl)
					ntfs_debug("First free bit is at LCN "
							"0x%llx.",
							(unsigned long long)
							(lcn + bmp_pos));
				else {
					memcpy(rl2, rl, rlsize);
					OSFree(rl, rlsize, ntfs_malloc_tag);
				}
				rl = rl2;
				rlsize += NTFS_ALLOC_BLOCK;
				ntfs_debug("Reallocated memory, rlsize 0x%x.",
						rlsize);
			}
			/* Allocate the bitmap bit. */
			*byte |= bit;
			vol->nr_free_clusters--;
			if (vol->nr_free_clusters < 0)
				vol->nr_free_clusters = 0;
			/* We need to write this bitmap page to disk. */
			need_writeback = TRUE;
			ntfs_debug("*byte 0x%x, need_writeback is set.",
					(unsigned)*byte);
			/*
			 * Coalesce with previous run if adjacent LCNs.
			 * Otherwise, append a new run.
			 */
			ntfs_debug("Adding run (lcn 0x%llx, len 0x%llx), "
					"prev_lcn 0x%llx, lcn 0x%llx, "
					"bmp_pos 0x%llx, prev_run_len 0x%llx, "
					"rlpos %d.",
					(unsigned long long)(lcn + bmp_pos),
					1ULL, (unsigned long long)prev_lcn,
					(unsigned long long)lcn,
					(unsigned long long)bmp_pos,
					(unsigned long long)prev_run_len,
					rlpos);
			if (prev_lcn == lcn + bmp_pos - prev_run_len && rlpos) {
				ntfs_debug("Coalescing to run (lcn 0x%llx, "
						"len 0x%llx).",
						(unsigned long long)
						rl[rlpos - 1].lcn,
						(unsigned long long)
						rl[rlpos - 1].length);
				rl[rlpos - 1].length = ++prev_run_len;
				ntfs_debug("Run now (lcn 0x%llx, len 0x%llx), "
						"prev_run_len 0x%llx.",
						(unsigned long long)
						rl[rlpos - 1].lcn,
						(unsigned long long)
						rl[rlpos - 1].length,
						(unsigned long long)
						prev_run_len);
			} else {
				if (rlpos) {
					ntfs_debug("Adding new run, (previous "
							"run lcn 0x%llx, "
							"len 0x%llx).",
							(unsigned long long)
							rl[rlpos - 1].lcn,
							(unsigned long long)
							rl[rlpos - 1].length);
					rl[rlpos].vcn = rl[rlpos - 1].vcn +
							prev_run_len;
				} else {
					ntfs_debug("Adding new run, is first "
							"run.");
					rl[rlpos].vcn = start_vcn;
				}
				rl[rlpos].lcn = prev_lcn = lcn + bmp_pos;
				rl[rlpos].length = prev_run_len = 1;
				rlpos++;
			}
			/* Done? */
			if (!--clusters) {
				LCN tc;
				/*
				 * Update the current zone position.  Positions
				 * of already scanned zones have been updated
				 * during the respective zone switches.
				 */
				tc = lcn + bmp_pos + 1;
				ntfs_debug("Done. Updating current zone "
						"position, tc 0x%llx, "
						"search_zone %d.",
						(unsigned long long)tc,
						search_zone);
				switch (search_zone) {
				case 1:
					ntfs_debug("Before checks, "
							"vol->mft_zone_pos "
							"0x%llx.",
							(unsigned long long)
							vol->mft_zone_pos);
					if (tc >= vol->mft_zone_end) {
						vol->mft_zone_pos =
								vol->mft_lcn;
						if (!vol->mft_zone_end)
							vol->mft_zone_pos = 0;
					} else if ((bmp_initial_pos >=
							vol->mft_zone_pos ||
							tc > vol->mft_zone_pos)
							&& tc >= vol->mft_lcn)
						vol->mft_zone_pos = tc;
					ntfs_debug("After checks, "
							"vol->mft_zone_pos "
							"0x%llx.",
							(unsigned long long)
							vol->mft_zone_pos);
					break;
				case 2:
					ntfs_debug("Before checks, "
							"vol->data1_zone_pos "
							"0x%llx.",
							(unsigned long long)
							vol->data1_zone_pos);
					if (tc >= vol->nr_clusters)
						vol->data1_zone_pos =
							     vol->mft_zone_end;
					else if ((bmp_initial_pos >=
						    vol->data1_zone_pos ||
						    tc > vol->data1_zone_pos)
						    && tc >= vol->mft_zone_end)
						vol->data1_zone_pos = tc;
					ntfs_debug("After checks, "
							"vol->data1_zone_pos "
							"0x%llx.",
							(unsigned long long)
							vol->data1_zone_pos);
					break;
				case 4:
					ntfs_debug("Before checks, "
							"vol->data2_zone_pos "
							"0x%llx.",
							(unsigned long long)
							vol->data2_zone_pos);
					if (tc >= vol->mft_zone_start)
						vol->data2_zone_pos = 0;
					else if (bmp_initial_pos >=
						      vol->data2_zone_pos ||
						      tc > vol->data2_zone_pos)
						vol->data2_zone_pos = tc;
					ntfs_debug("After checks, "
							"vol->data2_zone_pos "
							"0x%llx.",
							(unsigned long long)
							vol->data2_zone_pos);
					break;
				default:
					panic("%s(): Reached default case in "
							"switch!\n",
							__FUNCTION__);
				}
				ntfs_debug("Finished.  Going to out.");
				goto out;
			}
			lcn++;
		}
		bmp_pos += bsize;
		ntfs_debug("After inner while loop: bsize 0x%x, lcn 0x%llx, "
				"bmp_pos 0x%llx, need_writeback is %s.", bsize,
				(unsigned long long)lcn,
				(unsigned long long)bmp_pos,
				need_writeback ? "true" : "false");
		if (bmp_pos < zone_end) {
			ntfs_debug("Continuing outer while loop, "
					"bmp_pos 0x%llx, zone_end 0x%llx.",
					(unsigned long long)bmp_pos,
					(unsigned long long)zone_end);
			continue;
		}
zone_pass_done:	/* Finished with the current zone pass. */
		ntfs_debug("At zone_pass_done, pass %d.", pass);
		if (pass == 1) {
			/*
			 * Now do pass 2, scanning the first part of the zone
			 * we omitted in pass 1.
			 */
			pass = 2;
			zone_end = zone_start;
			switch (search_zone) {
			case 1: /* mft_zone */
				zone_start = vol->mft_zone_start;
				break;
			case 2: /* data1_zone */
				zone_start = vol->mft_zone_end;
				break;
			case 4: /* data2_zone */
				zone_start = 0;
				break;
			default:
				panic("%s(): Reached default case in switch "
						"(2)!\n", __FUNCTION__);
			}
			/* Sanity check. */
			if (zone_end < zone_start)
				zone_end = zone_start;
			bmp_pos = zone_start;
			ntfs_debug("Continuing outer while loop, pass 2, "
					"zone_start 0x%llx, zone_end 0x%llx, "
					"bmp_pos 0x%llx.",
					(unsigned long long)zone_start,
					(unsigned long long)zone_end,
					(unsigned long long)bmp_pos);
			continue;
		} /* pass == 2 */
done_zones_check:
		ntfs_debug("At done_zones_check, search_zone %d, done_zones "
				"before 0x%x, done_zones after 0x%x.",
				search_zone, done_zones,
				done_zones | search_zone);
		done_zones |= search_zone;
		if (done_zones < 7) {
			ntfs_debug("Switching zone.");
			/* Now switch to the next zone we haven't done yet. */
			pass = 1;
			switch (search_zone) {
			case 1:
				ntfs_debug("Switching from mft zone to data1 "
						"zone.");
				/* Update mft zone position. */
				if (rlpos) {
					LCN tc;

					ntfs_debug("Before checks, "
							"vol->mft_zone_pos "
							"0x%llx.",
							(unsigned long long)
							vol->mft_zone_pos);
					tc = rl[rlpos - 1].lcn +
							rl[rlpos - 1].length;
					if (tc >= vol->mft_zone_end) {
						vol->mft_zone_pos =
								vol->mft_lcn;
						if (!vol->mft_zone_end)
							vol->mft_zone_pos = 0;
					} else if ((bmp_initial_pos >=
							vol->mft_zone_pos ||
							tc > vol->mft_zone_pos)
							&& tc >= vol->mft_lcn)
						vol->mft_zone_pos = tc;
					ntfs_debug("After checks, "
							"vol->mft_zone_pos "
							"0x%llx.",
							(unsigned long long)
							vol->mft_zone_pos);
				}
				/* Switch from mft zone to data1 zone. */
switch_to_data1_zone:		search_zone = 2;
				zone_start = bmp_initial_pos =
						vol->data1_zone_pos;
				zone_end = vol->nr_clusters;
				if (zone_start == vol->mft_zone_end)
					pass = 2;
				if (zone_start >= zone_end) {
					vol->data1_zone_pos = zone_start =
							vol->mft_zone_end;
					pass = 2;
				}
				break;
			case 2:
				ntfs_debug("Switching from data1 zone to "
						"data2 zone.");
				/* Update data1 zone position. */
				if (rlpos) {
					LCN tc;

					ntfs_debug("Before checks, "
							"vol->data1_zone_pos "
							"0x%llx.",
							(unsigned long long)
							vol->data1_zone_pos);
					tc = rl[rlpos - 1].lcn +
							rl[rlpos - 1].length;
					if (tc >= vol->nr_clusters)
						vol->data1_zone_pos =
							     vol->mft_zone_end;
					else if ((bmp_initial_pos >=
						    vol->data1_zone_pos ||
						    tc > vol->data1_zone_pos)
						    && tc >= vol->mft_zone_end)
						vol->data1_zone_pos = tc;
					ntfs_debug("After checks, "
							"vol->data1_zone_pos "
							"0x%llx.",
							(unsigned long long)
							vol->data1_zone_pos);
				}
				/* Switch from data1 zone to data2 zone. */
				search_zone = 4;
				zone_start = bmp_initial_pos =
						vol->data2_zone_pos;
				zone_end = vol->mft_zone_start;
				if (!zone_start)
					pass = 2;
				if (zone_start >= zone_end) {
					vol->data2_zone_pos = zone_start =
							bmp_initial_pos = 0;
					pass = 2;
				}
				break;
			case 4:
				ntfs_debug("Switching from data2 zone to "
						"data1 zone.");
				/* Update data2 zone position. */
				if (rlpos) {
					LCN tc;

					ntfs_debug("Before checks, "
							"vol->data2_zone_pos "
							"0x%llx.",
							(unsigned long long)
							vol->data2_zone_pos);
					tc = rl[rlpos - 1].lcn +
							rl[rlpos - 1].length;
					if (tc >= vol->mft_zone_start)
						vol->data2_zone_pos = 0;
					else if (bmp_initial_pos >=
						      vol->data2_zone_pos ||
						      tc > vol->data2_zone_pos)
						vol->data2_zone_pos = tc;
					ntfs_debug("After checks, "
							"vol->data2_zone_pos "
							"0x%llx.",
							(unsigned long long)
							vol->data2_zone_pos);
				}
				/* Switch from data2 zone to data1 zone. */
				goto switch_to_data1_zone;
			default:
				panic("%s(): Reached default case in switch "
						"(3)!\n", __FUNCTION__);
			}
			ntfs_debug("After zone switch, search_zone %d, "
					"pass %d, bmp_initial_pos 0x%llx, "
					"zone_start 0x%llx, zone_end 0x%llx.",
					search_zone, pass,
					(unsigned long long)bmp_initial_pos,
					(unsigned long long)zone_start,
					(unsigned long long)zone_end);
			bmp_pos = zone_start;
			if (zone_start == zone_end) {
				ntfs_debug("Empty zone, going to "
						"done_zones_check.");
				/* Empty zone. Don't bother searching it. */
				goto done_zones_check;
			}
			ntfs_debug("Continuing outer while loop.");
			continue;
		} /* done_zones == 7 */
		ntfs_debug("All zones are finished.");
		/*
		 * All zones are finished!  If DATA_ZONE, shrink mft zone.  If
		 * MFT_ZONE, we have really run out of space.
		 */
		mft_zone_size = vol->mft_zone_end - vol->mft_zone_start;
		ntfs_debug("vol->mft_zone_start 0x%llx, vol->mft_zone_end "
				"0x%llx, mft_zone_size 0x%llx.",
				(unsigned long long)vol->mft_zone_start,
				(unsigned long long)vol->mft_zone_end,
				(unsigned long long)mft_zone_size);
		if (zone == MFT_ZONE || mft_zone_size <= 0) {
			ntfs_debug("No free clusters left, going to out.");
			/* Really no more space left on device. */
			err = ENOSPC;
			goto out;
		} /* zone == DATA_ZONE && mft_zone_size > 0 */
		ntfs_debug("Shrinking mft zone.");
		zone_end = vol->mft_zone_end;
		mft_zone_size >>= 1;
		if (mft_zone_size > 0)
			vol->mft_zone_end = vol->mft_zone_start + mft_zone_size;
		else /* mft zone and data2 zone no longer exist. */
			vol->data2_zone_pos = vol->mft_zone_start =
					vol->mft_zone_end = 0;
		if (vol->mft_zone_pos >= vol->mft_zone_end) {
			vol->mft_zone_pos = vol->mft_lcn;
			if (!vol->mft_zone_end)
				vol->mft_zone_pos = 0;
		}
		bmp_pos = zone_start = bmp_initial_pos =
				vol->data1_zone_pos = vol->mft_zone_end;
		search_zone = 2;
		pass = 2;
		done_zones &= ~2;
		ntfs_debug("After shrinking mft zone, mft_zone_size 0x%llx, "
				"vol->mft_zone_start 0x%llx, "
				"vol->mft_zone_end 0x%llx, "
				"vol->mft_zone_pos 0x%llx, search_zone 2, "
				"pass 2, dones_zones 0x%x, zone_start 0x%llx, "
				"zone_end 0x%llx, vol->data1_zone_pos 0x%llx, "
				"continuing outer while loop.",
				(unsigned long long)mft_zone_size,
				(unsigned long long)vol->mft_zone_start,
				(unsigned long long)vol->mft_zone_end,
				(unsigned long long)vol->mft_zone_pos,
				done_zones, (unsigned long long)zone_start,
				(unsigned long long)zone_end,
				(unsigned long long)vol->data1_zone_pos);
	}
	ntfs_debug("After outer while loop.");
out:
	ntfs_debug("At out.");
	/* Add runlist terminator element. */
	if (rl) {
		rl[rlpos].vcn = rl[rlpos - 1].vcn + rl[rlpos - 1].length;
		rl[rlpos].lcn = is_extension ? LCN_ENOENT : LCN_RL_NOT_MAPPED;
		rl[rlpos].length = 0;
	}
	if (upl) {
		ntfs_page_unmap(lcnbmp_ni, upl, pl, need_writeback);
		if (need_writeback) {
			ntfs_debug("Marking page dirty.");
			need_writeback = FALSE;
		}
	}
	if (!err) {
		/*
		 * We allocated new clusters thus we need to unmap any
		 * underlying i/os that may be inprogress so our new data
		 * cannot get trampled by old data from the previous owner of
		 * the clusters.
		 */
		if (rl) {
			ntfs_rl_element *trl;
			vnode_t dev_vn = vol->dev_vn;
			const u8 cluster_to_block_shift =
					vol->cluster_size_shift -
					vol->sector_size_shift;

			/* Iterate over the runs in the allocated runlist. */
			for (trl = rl; trl->length; trl++) {
				daddr64_t block, end_block;

				if (trl->lcn < 0)
					continue;
				/* Determine the starting block of this run. */
				block = trl->lcn << cluster_to_block_shift;
				/* Determine the last block of this run. */
				end_block = block + (trl->length <<
						cluster_to_block_shift);
				/*
				 * For each block in this run, invoke
				 * buf_invalblkno() to ensure no i/o against
				 * the block device can happen once we have
				 * allocated the buffers for other purposes.
				 *
				 * FIXME:/TODO: buf_invalblkno() currently
				 * aborts if msleep() returns an error so we
				 * keep calling it until it returns success.
				 */
				for (; block < end_block; block++) {
					do {
						err = buf_invalblkno(dev_vn,
								block,
								BUF_WAIT);
					} while (err);
				}
			}
		}
		lck_rw_unlock_shared(&lcnbmp_ni->lock);
		(void)vnode_put(lcnbmp_ni->vn);
		lck_rw_unlock_exclusive(&vol->lcnbmp_lock);
		if (runlist->alloc)
			OSFree(runlist->rl, runlist->alloc, ntfs_malloc_tag);
		runlist->rl = rl;
		runlist->elements = rlpos + 1;
		runlist->alloc = rlsize;
		ntfs_debug("Done.");
		return 0;
	}
	ntfs_error(vol->mp, "Failed to allocate clusters, aborting (error "
			"%d).", err);
	if (rl) {
		errno_t err2;

		if (err == ENOSPC)
			ntfs_debug("Not enough space to complete allocation, "
					"err ENOSPC, first free lcn 0x%llx, "
					"could allocate up to 0x%llx "
					"clusters.",
					(unsigned long long)rl[0].lcn,
					(unsigned long long)(count - clusters));
		/* Deallocate all allocated clusters. */
		ntfs_debug("Attempting rollback...");
		err2 = ntfs_cluster_free_from_rl_nolock(vol, rl, 0, -1, NULL);
		if (err2) {
			ntfs_error(vol->mp, "Failed to rollback (error %d).  "
					"Leaving inconsistent metadata!  "
					"Unmount and run chkdsk.", err2);
			NVolSetErrors(vol);
		}
		/* Free the runlist. */
		OSFree(rl, rlsize, ntfs_malloc_tag);
	} else if (err == ENOSPC)
		ntfs_debug("No space left at all, err ENOSPC, first free lcn "
				"0x%llx.", (long long)vol->data1_zone_pos);
	lck_rw_unlock_shared(&lcnbmp_ni->lock);
	(void)vnode_put(lcnbmp_ni->vn);
	lck_rw_unlock_exclusive(&vol->lcnbmp_lock);
	return err;
}

/**
 * ntfs_cluster_free_from_rl_nolock - free clusters from runlist
 * @vol:	mounted ntfs volume on which to free the clusters
 * @rl:		runlist describing the clusters to free
 * @start_vcn:	vcn in the runlist @rl at which to start freeing clusters
 * @count:	number of clusters to free or -1 for all clusters
 * @nr_freed:	if not NULL return the number of real clusters freed
 *
 * Free @count clusters starting at the cluster @start_vcn in the runlist @rl
 * on the volume @vol.  If @nr_freed is not NULL, *@nr_freed is set to the
 * number of real clusters (i.e. not counting sparse clusters) that have been
 * freed.
 *
 * If @count is -1, all clusters from @start_vcn to the end of the runlist are
 * deallocated.  Thus, to completely free all clusters in a runlist, use
 * @start_vcn = 0 and @count = -1.
 *
 * Note, ntfs_cluster_free_from_rl_nolock() does not modify the runlist, so you
 * have to remove from the runlist or mark sparse the freed runs later.
 *
 * Return 0 on success and errno on error.  Note that if at least some clusters
 * were freed success is always returned even though not all runs have been
 * freed yet.  This does not matter as it just means that some clusters are
 * lost until chkdsk is next run and as we schedule chkdsk to run at next boot
 * this should happen soon.  We do emit a warning about this happenning
 * however.
 *
 * Locking: - The volume lcn bitmap must be locked for writing on entry and is
 *	      left locked on return.
 *	    - The caller must have locked the runlist @rl for reading or
 *	      writing.
 *	    - The caller must have taken an iocount reference on the lcnbmp
 *	      vnode.
 */
static errno_t ntfs_cluster_free_from_rl_nolock(ntfs_volume *vol,
		ntfs_rl_element *rl, const VCN start_vcn, s64 count,
		s64 *nr_freed)
{
	s64 delta, to_free, real_freed;
	ntfs_inode *lcnbmp_ni = vol->lcnbmp_ni;
	errno_t err;

	ntfs_debug("Entering for start_vcn 0x%llx, count 0x%llx.",
			(unsigned long long)start_vcn,
			(unsigned long long)count);
	if (!lcnbmp_ni)
		panic("%s(): !lcnbmp_ni\n", __FUNCTION__);
	if (start_vcn < 0)
		panic("%s(): start_vcn < 0\n", __FUNCTION__);
	if (count < -1)
		panic("%s(): count < -1\n", __FUNCTION__);
	real_freed = 0;
	if (nr_freed)
		*nr_freed = 0;
	rl = ntfs_rl_find_vcn_nolock(rl, start_vcn);
	if (!rl)
		return 0;
	if (rl->lcn < LCN_HOLE) {
		ntfs_error(vol->mp, "First runlist element has invalid lcn, "
				"aborting.");
		return EIO;
	}
	/* Find the starting cluster inside the run that needs freeing. */
	delta = start_vcn - rl->vcn;
	/* The number of clusters in this run that need freeing. */
	to_free = rl->length - delta;
	if (count >= 0 && to_free > count)
		to_free = count;
	if (rl->lcn >= 0) {
		/* Do the actual freeing of the clusters in this run. */
		err = ntfs_bitmap_clear_run(lcnbmp_ni, rl->lcn + delta,
				to_free);
		if (err) {
			ntfs_error(vol->mp, "Failed to clear first run "
					"(error %d), aborting.", err);
			return err;
		}
		/* We have freed @to_free real clusters. */
		real_freed = to_free;
		vol->nr_free_clusters += to_free;
		if (vol->nr_free_clusters > vol->nr_clusters)
			vol->nr_free_clusters = vol->nr_clusters;
	}
	/* Go to the next run and adjust the number of clusters left to free. */
	++rl;
	if (count >= 0)
		count -= to_free;
	/*
	 * Loop over the remaining runs, using @count as a capping value, and
	 * free them.
	 */
	for (; rl->length && count; ++rl) {
		if (rl->lcn < LCN_HOLE) {
			ntfs_error(vol->mp, "Runlist element has invalid lcn "
					"%lld at vcn 0x%llx, not freeing.  "
					"Run chkdsk to recover the lost "
					"space.", (long long)rl->lcn,
					(unsigned long long)rl->vcn);
			NVolSetErrors(vol);
		}
		/* The number of clusters in this run that need freeing. */
		to_free = rl->length;
		if (count >= 0 && to_free > count)
			to_free = count;
		if (rl->lcn >= 0) {
			/* Do the actual freeing of the clusters in the run. */
			err = ntfs_bitmap_clear_run(lcnbmp_ni, rl->lcn,
					to_free);
			if (err) {
				ntfs_warning(vol->mp, "Failed to free "
						"clusters in subsequent run.  "
						"Run chkdsk to recover the "
						"lost space.");
				NVolSetErrors(vol);
			} else {
				vol->nr_free_clusters += to_free;
				if (vol->nr_free_clusters > vol->nr_clusters)
					vol->nr_free_clusters =
							vol->nr_clusters;
			}
			/* We have freed @to_free real clusters. */
			real_freed += to_free;
		}
		/* Adjust the number of clusters left to free. */
		if (count >= 0)
			count -= to_free;
	}
	if (count > 0)
		panic("%s(): count > 0\n", __FUNCTION__);
	/* We are done.  Return the number of actually freed clusters. */
	ntfs_debug("Done.");
	if (nr_freed)
		*nr_freed = real_freed;
	return 0;
}

/**
 * ntfs_cluster_free_from_rl - free clusters from runlist
 * @vol:	mounted ntfs volume on which to free the clusters
 * @rl:		runlist describing the clusters to free
 * @start_vcn:	vcn in the runlist @rl at which to start freeing clusters
 * @count:	number of clusters to free or -1 for all clusters
 * @nr_freed:	if not NULL return the number of real clusters freed
 *
 * Free @count clusters starting at the cluster @start_vcn in the runlist @rl
 * on the volume @vol.  If @nr_freed is not NULL, *@nr_freed is set to the
 * number of real clusters (i.e. not counting sparse clusters) that have been
 * freed.
 *
 * If @count is -1, all clusters from @start_vcn to the end of the runlist are
 * deallocated.  Thus, to completely free all clusters in a runlist, use
 * @start_vcn = 0 and @count = -1.
 *
 * Note, ntfs_cluster_free_from_rl_nolock() does not modify the runlist, so you
 * have to remove from the runlist or mark sparse the freed runs later.
 *
 * Return 0 on success and errno on error.  Note that if at least some clusters
 * were freed success is always returned even though not all runs have been
 * freed yet.  This does not matter as it just means that some clusters are
 * lost until chkdsk is next run and as we schedule chkdsk to run at next boot
 * this should happen soon.  We do emit a warning about this happenning
 * however.
 *
 * Locking: - This function takes the volume lcn bitmap lock for writing and
 *	      modifies the bitmap contents.
 *	    - The caller must have locked the runlist @rl for reading or
 *	      writing.
 */
errno_t ntfs_cluster_free_from_rl(ntfs_volume *vol, ntfs_rl_element *rl,
		const VCN start_vcn, s64 count, s64 *nr_freed)
{
	ntfs_inode *lcnbmp_ni;
	vnode_t lcnbmp_vn;
	errno_t err;

	lcnbmp_ni = vol->lcnbmp_ni;
	lcnbmp_vn = lcnbmp_ni->vn;
	lck_rw_lock_exclusive(&vol->lcnbmp_lock);
	err = vnode_get(lcnbmp_vn);
	if (!err) {
		lck_rw_lock_shared(&lcnbmp_ni->lock);
		err = ntfs_cluster_free_from_rl_nolock(vol, rl, start_vcn,
				count, nr_freed);
		lck_rw_unlock_shared(&lcnbmp_ni->lock);
		(void)vnode_put(lcnbmp_vn);
		lck_rw_unlock_exclusive(&vol->lcnbmp_lock);
		return err;
	}
	lck_rw_unlock_exclusive(&vol->lcnbmp_lock);
	ntfs_error(vol->mp, "Failed to get vnode for $Bitmap.");
	return err;
}

/**
 * ntfs_cluster_free_nolock - free clusters on an ntfs volume
 * @ni:		ntfs inode whose runlist describes the clusters to free
 * @start_vcn:	vcn in the runlist of @ni at which to start freeing clusters
 * @count:	number of clusters to free or -1 for all clusters
 * @ctx:	active attribute search context if present or NULL if not
 * @nr_freed:	if not NULL return the number of real clusters freed
 * @is_rollback:	true if this is a rollback operation
 *
 * Free @count clusters starting at the cluster @start_vcn in the runlist
 * described by the ntfs inode @ni.  If @nr_freed is not NULL, *@nr_freed is
 * set to the number of real clusters (i.e. not counting sparse clusters) that
 * have been freed.
 *
 * If @count is -1, all clusters from @start_vcn to the end of the runlist are
 * deallocated.  Thus, to completely free all clusters in a runlist, use
 * @start_vcn = 0 and @count = -1.
 *
 * If @ctx is specified, it is an active search context of @ni and its base mft
 * record.  This is needed when ntfs_cluster_free_nolock() encounters unmapped
 * runlist fragments and allows their mapping.  If you do not have the mft
 * record mapped, you can specify @ctx as NULL and ntfs_cluster_free_nolock()
 * will perform the necessary mapping and unmapping.
 *
 * Note, ntfs_cluster_free_unlock() saves the state of @ctx on entry and
 * restores it before returning.  Thus, @ctx will be left pointing to the same
 * attribute on return as on entry.  However, the actual pointers in @ctx may
 * point to different memory locations on return, so you must remember to reset
 * any cached pointers from the @ctx, i.e. after the call to
 * ntfs_cluster_free_nolock(), you will probably want to do:
 *	m = ctx->m;
 *	a = ctx->a;
 * Assuming you cache ctx->a in a variable @a of type ATTR_RECORD * and that
 * you cache ctx->m in a variable @m of type MFT_RECORD *.
 *
 * @is_rollback should always be false, it is for internal use to rollback
 * errors.  You probably want to use ntfs_cluster_free() instead.
 *
 * Note, ntfs_cluster_free_nolock() does not modify the runlist, so you have to
 * remove from the runlist or mark sparse the freed runs later.
 *
 * Return 0 on success and errno on error.
 *
 * WARNING: If @ctx is supplied, regardless of whether success or failure is
 *	    returned, you need to check @ctx->is_error and if 1 the @ctx is no
 *	    longer valid, i.e. you need to either call
 *	    ntfs_attr_search_ctx_reinit() or ntfs_attr_search_ctx_put() on it.
 *	    In that case @ctx->error will give you the error code for why the
 *	    mapping of the old inode failed.
 *	    Also if @ctx is supplied and the current attribute (or the mft
 *	    record it is in) has been modified then the caller must call
 *	    NInoSetMrecNeedsDirtying(ctx->ni); before calling
 *	    ntfs_map_runlist_nolock() or the changes may be lost.
 *
 * Locking: - The volume lcn bitmap must be locked for writing on entry and is
 *	      left locked on return.
 *	    - The runlist described by @ni must be locked for writing on entry
 *	      and is locked on return.  Note the runlist may be modified when
 *	      needed runlist fragments need to be mapped.
 *	    - If @ctx is NULL, the base mft record of @ni must not be mapped on
 *	      entry and it will be left unmapped on return.
 *	    - If @ctx is not NULL, the base mft record must be mapped on entry
 *	      and it will be left mapped on return.
 *	    - The caller must have taken an iocount reference on the lcnbmp
 *	      vnode.
 */
static errno_t ntfs_cluster_free_nolock(ntfs_inode *ni, const VCN start_vcn,
		s64 count, ntfs_attr_search_ctx *ctx, s64 *nr_freed,
		const BOOL is_rollback)
{
	s64 delta, to_free, total_freed, real_freed;
	ntfs_volume *vol;
	ntfs_inode *lcnbmp_ni;
	ntfs_rl_element *rl;
	errno_t err, err2;

	vol = ni->vol;
	lcnbmp_ni = vol->lcnbmp_ni;
	if (!lcnbmp_ni)
		panic("%s(): !lcnbmp_ni\n", __FUNCTION__);
	if (start_vcn < 0)
		panic("%s(): start_vcn < 0\n", __FUNCTION__);
	if (count < -1)
		panic("%s(): count < -1\n", __FUNCTION__);
	total_freed = real_freed = 0;
	if (nr_freed)
		*nr_freed = 0;
	err = ntfs_attr_find_vcn_nolock(ni, start_vcn, &rl, ctx);
	if (err) {
		if (!is_rollback)
			ntfs_error(vol->mp, "Failed to find first runlist "
					"element (error %d), aborting.", err);
		goto err;
	}
	if (rl->lcn < LCN_HOLE) {
		if (!is_rollback)
			ntfs_error(vol->mp, "First runlist element has "
					"invalid lcn, aborting.");
		err = EIO;
		goto err;
	}
	/* Find the starting cluster inside the run that needs freeing. */
	delta = start_vcn - rl->vcn;
	/* The number of clusters in this run that need freeing. */
	to_free = rl->length - delta;
	if (count >= 0 && to_free > count)
		to_free = count;
	if (rl->lcn >= 0) {
		/* Do the actual freeing of the clusters in this run. */
		err = ntfs_bitmap_set_bits_in_run(lcnbmp_ni, rl->lcn + delta,
				to_free, !is_rollback ? 0 : 1);
		if (err) {
			if (!is_rollback)
				ntfs_error(vol->mp, "Failed to clear first run "
						"(error %d), aborting.", err);
			goto err;
		}
		/* We have freed @to_free real clusters. */
		real_freed = to_free;
		if (is_rollback) {
			vol->nr_free_clusters -= to_free;
			if (vol->nr_free_clusters < 0)
				vol->nr_free_clusters = 0;
		} else {
			vol->nr_free_clusters += to_free;
			if (vol->nr_free_clusters > vol->nr_clusters)
				vol->nr_free_clusters = vol->nr_clusters;
		}
	}
	/* Go to the next run and adjust the number of clusters left to free. */
	++rl;
	if (count >= 0)
		count -= to_free;
	/* Keep track of the total "freed" clusters, including sparse ones. */
	total_freed = to_free;
	/*
	 * Loop over the remaining runs, using @count as a capping value, and
	 * free them.
	 */
	for (; count; ++rl) {
		if (rl->lcn < LCN_HOLE) {
			VCN vcn;

			/*
			 * If we have reached the end of the runlist we are
			 * done.  We need this when @count is -1 so that we
			 * detect the end of the runlist.
			 */
			if (rl->lcn == LCN_ENOENT)
				break;
			/* Attempt to map runlist. */
			vcn = rl->vcn;
			err = ntfs_attr_find_vcn_nolock(ni, vcn, &rl, ctx);
			if (err) {
				/*
				 * If we have reached the end of the runlist we
				 * are done.  We need this when @count is -1 so
				 * that we detect the end of the runlist.
				 */
				if (err == ENOENT)
					break;
				if (!is_rollback)
					ntfs_error(vol->mp, "Failed to map "
							"runlist fragment or "
							"failed to find "
							"subsequent runlist "
							"element.");
				goto err;
			}
			if (rl->lcn < LCN_HOLE) {
				if (!is_rollback)
					ntfs_error(vol->mp, "Runlist element "
							"has invalid lcn "
							"(0x%llx).",
							(unsigned long long)
							rl->lcn);
				err = EIO;
				goto err;
			}
		}
		/* The number of clusters in this run that need freeing. */
		to_free = rl->length;
		if (count >= 0 && to_free > count)
			to_free = count;
		if (rl->lcn >= 0) {
			/* Do the actual freeing of the clusters in the run. */
			err = ntfs_bitmap_set_bits_in_run(lcnbmp_ni, rl->lcn,
					to_free, !is_rollback ? 0 : 1);
			if (err) {
				if (!is_rollback)
					ntfs_error(vol->mp, "Failed to clear "
							"subsequent run.");
				goto err;
			}
			/* We have freed @to_free real clusters. */
			real_freed += to_free;
			if (is_rollback) {
				vol->nr_free_clusters -= to_free;
				if (vol->nr_free_clusters < 0)
					vol->nr_free_clusters = 0;
			} else {
				vol->nr_free_clusters += to_free;
				if (vol->nr_free_clusters > vol->nr_clusters)
					vol->nr_free_clusters =
							vol->nr_clusters;
			}
		}
		/* Adjust the number of clusters left to free. */
		if (count >= 0)
			count -= to_free;
		/* Update the total done clusters. */
		total_freed += to_free;
	}
	if (count > 0)
		panic("%s(): count > 0\n", __FUNCTION__);
	/* We are done.  Return the number of actually freed clusters. */
	ntfs_debug("Done.");
	if (nr_freed)
		*nr_freed = real_freed;
	return 0;
err:
	if (is_rollback)
		return err;
	/* If no real clusters were freed, no need to rollback. */
	if (!real_freed)
		return err;
	/*
	 * Attempt to rollback and if that succeeds just return the error code.
	 * If rollback fails, set the volume errors flag, emit an error
	 * message, and return the error code.
	 */
	err2 = ntfs_cluster_free_nolock(ni, start_vcn, total_freed, ctx, NULL,
			TRUE);
	if (err2) {
		ntfs_error(vol->mp, "Failed to rollback (error %d).  Leaving "
				"inconsistent metadata!  Unmount and run "
				"chkdsk.", err2);
		NVolSetErrors(vol);
	}
	ntfs_error(vol->mp, "Aborting (error %d).", err);
	return err;
}

/**
 * ntfs_cluster_free - free clusters on an ntfs volume
 * @ni:		ntfs inode whose runlist describes the clusters to free
 * @start_vcn:	vcn in the runlist of @ni at which to start freeing clusters
 * @count:	number of clusters to free or -1 for all clusters
 * @ctx:	active attribute search context if present or NULL if not
 * @nr_freed:	if not NULL return the number of real clusters freed
 *
 * Free @count clusters starting at the cluster @start_vcn in the runlist
 * described by the ntfs inode @ni.  If @nr_freed is not NULL, *@nr_freed is
 * set to the number of real clusters (i.e. not counting sparse clusters) that
 * have been freed.
 *
 * If @count is -1, all clusters from @start_vcn to the end of the runlist are
 * deallocated.  Thus, to completely free all clusters in a runlist, use
 * @start_vcn = 0 and @count = -1.
 *
 * If @ctx is specified, it is an active search context of @ni and its base mft
 * record.  This is needed when ntfs_cluster_free() encounters unmapped runlist
 * fragments and allows their mapping.  If you do not have the mft record
 * mapped, you can specify @ctx as NULL and ntfs_cluster_free() will perform
 * the necessary mapping and unmapping.
 *
 * Note, ntfs_cluster_free() saves the state of @ctx on entry and restores it
 * before returning.  Thus, @ctx will be left pointing to the same attribute on
 * return as on entry.  However, the actual pointers in @ctx may point to
 * different memory locations on return, so you must remember to reset any
 * cached pointers from the @ctx, i.e. after the call to ntfs_cluster_free(),
 * you will probably want to do:
 *	m = ctx->m;
 *	a = ctx->a;
 * Assuming you cache ctx->a in a variable @a of type ATTR_RECORD * and that
 * you cache ctx->m in a variable @m of type MFT_RECORD *.
 *
 * Note, ntfs_cluster_free() does not modify the runlist, so you have to remove
 * from the runlist or mark sparse the freed runs later.
 *
 * Return 0 on success and errno on error.
 *
 * WARNING: If @ctx is supplied, regardless of whether success or failure is
 *	    returned, you need to check @ctx->is_error and if 1 the @ctx is no
 *	    longer valid, i.e. you need to either call
 *	    ntfs_attr_search_ctx_reinit() or ntfs_attr_search_ctx_put() on it.
 *	    In that case @ctx->error will give you the error code for why the
 *	    mapping of the old inode failed.
 *	    Also if @ctx is supplied and the current attribute (or the mft
 *	    record it is in) has been modified then the caller must call
 *	    NInoSetMrecNeedsDirtying(ctx->ni); before calling
 *	    ntfs_map_runlist_nolock() or the changes may be lost.
 *
 * Locking: - The runlist described by @ni must be locked for writing on entry
 *	      and is locked on return.  Note the runlist may be modified when
 *	      needed runlist fragments need to be mapped.
 *	    - The volume lcn bitmap must be unlocked on entry and is unlocked
 *	      on return.
 *	    - This function takes the volume lcn bitmap lock for writing and
 *	      modifies the bitmap contents.
 *	    - If @ctx is NULL, the base mft record of @ni must not be mapped on
 *	      entry and it will be left unmapped on return.
 *	    - If @ctx is not NULL, the base mft record must be mapped on entry
 *	      and it will be left mapped on return.
 */
errno_t ntfs_cluster_free(ntfs_inode *ni, const VCN start_vcn, s64 count,
		ntfs_attr_search_ctx *ctx, s64 *nr_freed)
{
	ntfs_volume *vol;
	ntfs_inode *lcnbmp_ni;
	vnode_t lcnbmp_vn;
	errno_t err;

	if (!ni)
		panic("%s(): !ni\n", __FUNCTION__);
	ntfs_debug("Entering for mft_no 0x%llx, start_vcn 0x%llx, count "
			"0x%llx.", (unsigned long long)ni->mft_no,
			(unsigned long long)start_vcn,
			(unsigned long long)count);
	vol = ni->vol;
	lcnbmp_ni = vol->lcnbmp_ni;
	lcnbmp_vn = lcnbmp_ni->vn;
	lck_rw_lock_exclusive(&vol->lcnbmp_lock);
	err = vnode_get(lcnbmp_vn);
	if (!err) {
		lck_rw_lock_shared(&lcnbmp_ni->lock);
		err = ntfs_cluster_free_nolock(ni, start_vcn, count, ctx,
				nr_freed, FALSE);
		lck_rw_unlock_shared(&lcnbmp_ni->lock);
		(void)vnode_put(lcnbmp_vn);
		lck_rw_unlock_exclusive(&vol->lcnbmp_lock);
		return err;
	}
	lck_rw_unlock_exclusive(&vol->lcnbmp_lock);
	ntfs_error(vol->mp, "Failed to get vnode for $Bitmap.");
	return err;
}
