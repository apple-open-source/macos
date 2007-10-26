/*
 * ntfs_mft.c - NTFS kernel mft record operations.
 *
 * Copyright (c) 2006, 2007 Anton Altaparmakov.  All Rights Reserved.
 * Portions Copyright (c) 2006, 2007 Apple Inc.  All Rights Reserved.
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

#include <sys/errno.h>
#include <sys/stat.h>
// #include <sys/namei.h>
/*
 * struct nameidata is defined in <sys/namei.h>, but it is private so put in a
 * forward declaration for now so that vnode_internal.h can compile.  All we
 * need vnode_internal.h for is the declaration of vnode_getwithref() which is
 * even exported in BSDKernel.exports.
 */
struct nameidata;
#include <sys/vnode_internal.h>

#include <string.h>

#include <libkern/OSAtomic.h>
#include <libkern/OSMalloc.h>

#include <kern/debug.h>
#include <kern/locks.h>

#include "ntfs_debug.h"
#include "ntfs_dir.h"
#include "ntfs_endian.h"
#include "ntfs_inode.h"
#include "ntfs_layout.h"
#include "ntfs_mft.h"
#include "ntfs_page.h"
#include "ntfs_types.h"
#include "ntfs_volume.h"

static inline errno_t ntfs_mft_record_map_nolock(ntfs_inode *ni,
		MFT_RECORD **mrec)
{
	s64 mft_size, mft_ofs;
	ntfs_volume *vol = ni->vol;
	ntfs_inode *base_ni, *mft_ni = vol->mft_ni;
	upl_t upl;
	upl_page_info_array_t pl;
	u8 *kaddr;
	errno_t err;

	if (NInoAttr(ni))
		panic("%s(): Called for attribute inode.\n", __FUNCTION__);
	if (ni->mrec_upl || ni->mrec_pl || ni->mrec_kaddr)
		panic("%s(): Mft record is already mapped.\n", __FUNCTION__);
	/* Get an iocount reference on the $MFT vnode. */
	err = vnode_getwithref(mft_ni->vn);
	if (err) {
		ntfs_error(vol->mp, "Failed to get vnode for $MFT.");
		return err;
	}
	/*
	 * If the wanted mft record number is out of bounds the mft record does
	 * not exist.
	 */
	lck_spin_lock(&mft_ni->size_lock);
	mft_size = mft_ni->data_size;
	lck_spin_unlock(&mft_ni->size_lock);
	if (ni->mft_no > (ino64_t)(mft_size >> vol->mft_record_size_shift)) {
		ntfs_error(vol->mp, "Attempt to read mft record 0x%llx, which "
				"is beyond the end of the mft.  This is "
				"probably a bug in the ntfs driver.",
				(unsigned long long)ni->mft_no);
		err = ENOENT;
		goto err;
	}
	/*
	 * We implement access to $MFT/$DATA by mapping the page containing the
	 * mft record into memory using ntfs_page_map() which takes care or
	 * reading the page in if it is not in memory already and removing the
	 * mst protection fixups.
	 *
	 * In case we ever care, we know whether ntfs_page_map() found the page
	 * already in memory or whether it read it in because in the former
	 * case upl_valid_page(pl, 0) will be TRUE and in the latter case it
	 * will be FALSE.
	 *
	 * Similarly we know if the page was already dirty or not by checking
	 * upl_dirty_page(pl, 0).  The dirty bit is only valid if the page was
	 * valid, too.
	 *
	 * There is a complication and that is that we cannot map the same page
	 * twice thus reading two adjacent mft records is not possible that
	 * way.  But when mapping an extent mft record we have to allow for the
	 * fact that the base mft record and the extent are in the same page
	 * thus we have to work around the deadlock by reusing the same page as
	 * is already mapped for the base mft record for the extent mft record.
	 */
	mft_ofs = (s64)ni->mft_no << vol->mft_record_size_shift;
	base_ni = NULL;
	lck_mtx_lock(&ni->extent_lock);
	if (ni->nr_extents == -1)
		base_ni = ni->base_ni;
	lck_mtx_unlock(&ni->extent_lock);
	if (base_ni) {
		ino64_t mft_page_mask;

		mft_page_mask = PAGE_SIZE >> vol->mft_record_size_shift;
		if (!mft_page_mask)
			panic("%s(): PAGE_SIZE < MFT record size!\n",
					__FUNCTION__);
		mft_page_mask = ~(mft_page_mask - 1);
		if ((base_ni->mft_no & mft_page_mask) ==
				(ni->mft_no & mft_page_mask)) {
			upl = base_ni->mrec_upl;
			pl = base_ni->mrec_pl;
			kaddr = base_ni->mrec_kaddr;
			if (!upl || !pl || !kaddr)
				panic("%s(): Base mft record 0x%llx "
						"of mft record 0x%llx "
						"is not mapped.\n",
						__FUNCTION__,
						(unsigned long long)
						base_ni->mft_no,
						(unsigned long long)
						ni->mft_no);
			NInoSetNotMrecPageOwner(ni);
		} else
			base_ni = NULL;
	}
	/*
	 * We do not know if the caller intends to modify the MFT record or not
	 * thus we always have to state that we will modify it when mapping the
	 * page containing the record.  It should not really matter as all that
	 * does is to ensure COW semantics are observed and for $MFT the NTFS
	 * driver has the only mapping thus COW should never need to happen...
	 */
	if (base_ni || !(err = ntfs_page_map(mft_ni, mft_ofs & ~PAGE_MASK_64,
			&upl, &pl, &kaddr, TRUE))) {
		MFT_RECORD *m = (MFT_RECORD*)(kaddr + (mft_ofs & PAGE_MASK));
		/* Catch multi sector transfer fixup errors. */
		if (ntfs_is_mft_record(m->magic)) {
			ni->mrec_upl = upl;
			ni->mrec_pl = pl;
			ni->mrec_kaddr = kaddr;
			*mrec = m;
			return err;
		}
		ntfs_error(vol->mp, "Mft record 0x%llx is corrupt.  Run "
				"chkdsk.", (unsigned long long)ni->mft_no);
		err = EIO;
		/* Error, release the page if we mappedd it. */
		if (!NInoTestClearNotMrecPageOwner(ni))
			ntfs_page_unmap(mft_ni, upl, pl, FALSE);
	}
err:
	/*
	 * Release the iocount reference on the $MFT vnode.  We can ignore the
	 * return value as it always is zero.
	 */
	(void)vnode_put(mft_ni->vn);
	return err;
}

/**
 * ntfs_mft_record_map - map, pin, and lock an mft record
 * @ni:		ntfs inode whose mft record to map
 * @m:		destination pointer for the mapped mft record
 *
 * The page containing the desired mft record is mapped and the mrec_lock
 * mutex is taken.
 *
 * Return 0 on success and errno on error.
 *
 * Note: Caller must hold an iocount reference on the vnode of the base inode
 * of @ni.
 */
errno_t ntfs_mft_record_map(ntfs_inode *ni, MFT_RECORD **m)
{
	errno_t err;

	ntfs_debug("Entering for mft_no 0x%llx.",
			(unsigned long long)ni->mft_no);
	/* Serialize access to this mft record. */
	lck_mtx_lock(&ni->mrec_lock);
	err = ntfs_mft_record_map_nolock(ni, m);
	if (!err) {
		ntfs_debug("Done.");
		return err;
	}
	lck_mtx_unlock(&ni->mrec_lock);
	ntfs_error(ni->vol->mp, "Failed with error code %d.", (int)err);
	return err;
}

/**
 * ntfs_mft_record_unmap - release a mapped mft record
 * @ni:		ntfs inode whose mft record to unmap
 *
 * Unmap the page containing the mft record and release the mrec_mutex.
 */
void ntfs_mft_record_unmap(ntfs_inode *ni)
{
	ntfs_inode *mft_ni = ni->vol->mft_ni;
	upl_t upl;
	upl_page_info_array_t pl;
	BOOL dirty;

	ntfs_debug("Entering for mft_no 0x%llx.",
			(unsigned long long)ni->mft_no);
	upl = ni->mrec_upl;
	pl = ni->mrec_pl;
	if (!upl || !pl)
		panic("%s(): Mft record 0x%llx is not mapped.\n", __FUNCTION__,
				(unsigned long long)ni->mft_no);
	ni->mrec_upl = NULL;
	ni->mrec_pl = NULL;
	ni->mrec_kaddr = NULL;
	dirty = NInoTestClearMrecPageNeedsDirtying(ni);
	if (!NInoTestClearNotMrecPageOwner(ni))
		ntfs_page_unmap(mft_ni, upl, pl, dirty);
	else if (dirty) {
		ntfs_inode *base_ni = NULL;
		lck_mtx_lock(&ni->extent_lock);
		if (ni->nr_extents == -1)
			base_ni = ni->base_ni;
		lck_mtx_unlock(&ni->extent_lock);
		if (!base_ni)
			panic("%s(): No base inode present but it must be.\n",
					__FUNCTION__);
		NInoSetMrecPageNeedsDirtying(base_ni);
	}
	/*
	 * Release the iocount reference on the $MFT vnode.  We can ignore the
	 * return value as it always is zero.
	 */
	(void)vnode_put(mft_ni->vn);
	lck_mtx_unlock(&ni->mrec_lock);
	ntfs_debug("Done.");
}

/**
 * ntfs_extent_mft_record_map - load an extent inode and attach it to its base
 * @base_ni:	base ntfs inode
 * @mref:	mft reference of the extent inode to load
 * @ext_ni:	on successful return, pointer to the ntfs inode structure
 * @ext_mrec:	on successful return, pointer to the mft record structure
 *
 * Load the extent mft record @mref and attach it to its base inode @base_ni.
 * Return the mapped extent mft record if success.
 *
 * On successful return, @ext_ni contains a pointer to the ntfs inode structure
 * of the mapped extent inode and @ext_mrec contains a pointer to the mft
 * record structure of the mapped extent inode.
 *
 * Note: The caller must hold an iocount reference on the vnode of the base
 * inode.
 */
errno_t ntfs_extent_mft_record_map(ntfs_inode *base_ni, MFT_REF mref,
		ntfs_inode **ext_ni, MFT_RECORD **ext_mrec)
{
	ino64_t mft_no = MREF(mref);
	ntfs_inode **extent_nis = NULL;
	ntfs_inode *ni = NULL;
	MFT_RECORD *m;
	errno_t err;
	int i;
	u16 seq_no = MSEQNO(mref);
	BOOL need_reclaim;

	ntfs_debug("Mapping extent mft record 0x%llx (base mft record "
			"0x%llx).", (unsigned long long)mft_no,
			(unsigned long long)base_ni->mft_no);
	/*
	 * Check if this extent inode has already been added to the base inode,
	 * in which case just return it.  If not found, add it to the base
	 * inode before returning it.
	 */
	lck_mtx_lock(&base_ni->extent_lock);
	if (base_ni->nr_extents > 0) {
		extent_nis = base_ni->extent_nis;
		for (i = 0; i < base_ni->nr_extents; i++) {
			if (mft_no != extent_nis[i]->mft_no)
				continue;
			ni = extent_nis[i];
			break;
		}
	}
	if (ni) {
		lck_mtx_unlock(&base_ni->extent_lock);
		/* We found the record.  Map and return it. */
		err = ntfs_mft_record_map(ni, &m);
		if (!err) {
			/* Verify the sequence number if present. */
			if (!seq_no || le16_to_cpu(m->sequence_number) ==
					seq_no) {
				ntfs_debug("Done 1.");
				*ext_ni = ni;
				*ext_mrec = m;
				return err;
			}
			ntfs_mft_record_unmap(ni);
			ntfs_error(base_ni->vol->mp, "Found stale extent mft "
					"reference!  Corrupt filesystem.  "
					"Run chkdsk.");
			return EIO;
		}
map_err_out:
		ntfs_error(base_ni->vol->mp, "Failed to map extent mft "
				"record (error %d).", (int)err);
		return err;
	}
	/* Record was not there.  Get a new ntfs inode and initialize it. */
	err = ntfs_extent_inode_get(base_ni, mref, &ni);
	if (err) {
		lck_mtx_unlock(&base_ni->extent_lock);
		return err;
	}
	/* Now map the extent mft record. */
	err = ntfs_mft_record_map(ni, &m);
	if (err) {
		lck_mtx_unlock(&base_ni->extent_lock);
		ntfs_inode_reclaim(ni);
		goto map_err_out;
	}
	need_reclaim = FALSE;
	/* Verify the sequence number if it is present. */
	if (seq_no && le16_to_cpu(m->sequence_number) != seq_no) {
		ntfs_error(base_ni->vol->mp, "Found stale extent mft "
				"reference!  Corrupt filesystem.  Run chkdsk.");
		need_reclaim = TRUE;
		err = EIO;
		goto unm_err_out;
	}
	/* Attach extent inode to base inode, reallocating memory if needed. */
	if (!(base_ni->nr_extents & 3)) {
		ntfs_inode **tmp;
		int new_size = (base_ni->nr_extents + 4) * sizeof(ntfs_inode *);

		tmp = (ntfs_inode **)OSMalloc(new_size, ntfs_malloc_tag);
		if (!tmp) {
			ntfs_error(base_ni->vol->mp, "Failed to allocate "
					"internal buffer.");
			need_reclaim = TRUE;
			err = ENOMEM;
			goto unm_err_out;
		}
		if (base_ni->nr_extents) {
			int old_size = new_size - 4 * sizeof(ntfs_inode *);
			memcpy(tmp, base_ni->extent_nis, old_size);
			OSFree(base_ni->extent_nis, old_size, ntfs_malloc_tag);
		}
		base_ni->extent_nis = tmp;
	}
	base_ni->extent_nis[base_ni->nr_extents++] = ni;
	lck_mtx_unlock(&base_ni->extent_lock);
	ntfs_debug("Done 2.");
	*ext_ni = ni;
	*ext_mrec = m;
	return err;
unm_err_out:
	ntfs_mft_record_unmap(ni);
	lck_mtx_unlock(&base_ni->extent_lock);
	/*
	 * If the extent inode was not attached to the base inode we need to
	 * release it or we will leak memory.
	 */
	if (need_reclaim)
		ntfs_inode_reclaim(ni);
	return err;
}
