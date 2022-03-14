/*
 * ntfs_mft.c - NTFS kernel mft record operations.
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
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ucred.h>
#include <sys/ubc.h>
#include <sys/vnode.h>

#include <string.h>

#include <libkern/libkern.h>
#include <libkern/OSAtomic.h>
#include <IOKit/IOLib.h>

#include <kern/debug.h>
#include <kern/locks.h>

#include "ntfs.h"
#include "ntfs_attr.h"
#include "ntfs_bitmap.h"
#include "ntfs_debug.h"
#include "ntfs_dir.h"
#include "ntfs_endian.h"
#include "ntfs_hash.h"
#include "ntfs_inode.h"
#include "ntfs_layout.h"
#include "ntfs_lcnalloc.h"
#include "ntfs_mft.h"
#include "ntfs_page.h"
#include "ntfs_secure.h"
#include "ntfs_time.h"
#include "ntfs_types.h"
#include "ntfs_volume.h"

/**
 * ntfs_mft_record_map_ext - map an mft record
 * @ni:			ntfs inode whose mft record to map
 * @mrec:		destination pointer for the mapped mft record
 * @mft_is_locked:	if true the caller holds the mft lock (@mft_ni->lock)
 *
 * The buffer containing the mft record belonging to the ntfs inode @ni is
 * mapped which on OS X means it is held for exclusive via the BL_BUSY flag in
 * the buffer.  The mapped mft record is returned in *@m.
 *
 * If @mft_is_locked is true the caller holds the mft lock (@mft_ni->lock) thus
 * ntfs_mft_record_map_ext() will not try to take the same lock.  It is then
 * the responsibility of the caller that the mft is consistent and stable for
 * the duration of the call.
 *
 * Return 0 on success and errno on error.
 *
 * Note: Caller must hold an iocount reference on the vnode of the base inode
 * of @ni.
 */
errno_t ntfs_mft_record_map_ext(ntfs_inode *ni, MFT_RECORD **mrec,
		const BOOL mft_is_locked)
{
	ntfs_volume *vol;
	ntfs_inode *mft_ni;
	u8 *dbuf = NULL;
	buf_t buf;
	MFT_RECORD *m;
	errno_t err;
	ino64_t buf_mft_no;
	ino64_t buf_mft_record;
	u32 buf_read_size;

	ntfs_debug("Entering for mft_no 0x%llx (mft is %slocked).",
			(unsigned long long)ni->mft_no,
			mft_is_locked ? "" : "not ");
	if (NInoAttr(ni))
		panic("%s(): Called for attribute inode.\n", __FUNCTION__);
	vol = ni->vol;
	mft_ni = vol->mft_ni;
	/*
	 * If the volume is in the process of being unmounted then @vol->mft_ni
	 * may have become NULL in which case we need to bail out.
	 */
	if (!mft_ni) {
		/*
		 * @vol->mp may be NULL now which is ok.  ntfs_error() deals
		 * with this case gracefully.
		 */
		ntfs_error(vol->mp, "The volume is being unmounted, bailing "
				"out (you can ignore any errors following "
				"this one).");
		return EINVAL;
	}
	/* Get an iocount reference on the $MFT vnode. */
	err = vnode_get(mft_ni->vn);
	if (err) {
		ntfs_error(vol->mp, "Failed to get vnode for $MFT.");
		return err;
	}
	if (!mft_is_locked)
		lck_rw_lock_shared(&mft_ni->lock);
	/*
	 * If the wanted mft record number is out of bounds the mft record does
	 * not exist.
	 */
	lck_spin_lock(&mft_ni->size_lock);
	if (ni->mft_no > (ino64_t)(mft_ni->data_size >>
			vol->mft_record_size_shift)) {
		lck_spin_unlock(&mft_ni->size_lock);
		ntfs_error(vol->mp, "Attempt to read mft record 0x%llx, which "
				"is beyond the end of the mft.",
				(unsigned long long)ni->mft_no);
		err = ENOENT;
		goto err;
	}
	lck_spin_unlock(&mft_ni->size_lock);
	/*
	 * We implement access to $MFT/$DATA by mapping the buffer containing
	 * the mft record into memory using buf_meta_bread() which takes care
	 * of reading the buffer in if it is not in memory already and removing
	 * the mst protection fixups.
	 *
	 * In case we ever care, we know whether buf_meta_bread() found the
	 * buffer already in memory or whether it read it in because in the
	 * former case buf_fromcache(buf) will be true and in the latter case
	 * it will be false.
	 *
	 * Similarly we know if the buffer was already dirty or not by checking
	 * buf_flags(buf) & B_DELWRI.
	 */
	if (vol->mft_record_size < vol->sector_size) {
		/* If the MFT record size is smaller than a sector, we must
		 * align the buffer read to the sector size. */
		buf_mft_no = ni->mft_no & ~vol->mft_records_per_sector_mask;
		buf_mft_record = ni->mft_no & vol->mft_records_per_sector_mask;
		buf_read_size = vol->sector_size;

		dbuf = IOMallocData(vol->mft_record_size);
		if (!dbuf) {
			ntfs_error(vol->mp, "Error while allocating %lu bytes "
					"for mft record double buffer.",
					(unsigned long)vol->mft_record_size);
			err = ENOMEM;
			goto err;
		}

		/* Lock the inode's buffer for concurrent access until unmap
		 * time since we cannot rely on the mapped buffer to provide
		 * locking. */
		lck_mtx_lock(&ni->buf_lock);
	} else {
		buf_mft_no = ni->mft_no;
		buf_mft_record = 0;
		buf_read_size = vol->mft_record_size;
	}

	ntfs_debug("Calling buf_meta_bread().");
	err = buf_meta_bread(mft_ni->vn, buf_mft_no, buf_read_size,
			NOCRED, &buf);
	ntfs_debug("After buf_meta_bread().");
	if (err) {
		ntfs_error(vol->mp, "Failed to read buffer of mft record "
				"0x%llx (error %d).",
				(unsigned long long)ni->mft_no, err);
		goto buf_err;
	}
	err = buf_map(buf, (caddr_t*)&m);
	if (err) {
		ntfs_error(vol->mp, "Failed to map buffer of mft record "
				"0x%llx (error %d).",
				(unsigned long long)ni->mft_no, err);
		goto buf_err;
	}
	if (!m)
		panic("%s(): buf_map() returned NULL.\n", __FUNCTION__);
	if (ni->m_buf || ni->m_dbuf || ni->m)
		panic("%s(): Mft record 0x%llx is already mapped.\n",
				__FUNCTION__, (unsigned long long)ni->mft_no);
	if (dbuf) {
		/* Copy the part of the sector containing our mft record to our
		 * allocated buffer to avoid keeping a reference to a shared
		 * sector's buffer. */
		memcpy(dbuf,
			&((char*) m)[buf_mft_record * vol->mft_record_size],
			vol->mft_record_size);
		m = (MFT_RECORD*) dbuf;
	}
	/* Catch multi sector transfer fixup errors. */
	if ((ntfs_is_mft_record(m->magic)) && (le32_to_cpu(m->bytes_allocated) <= vol->mft_record_size)) {
		if (dbuf) {
			/* We are now finished with 'buf' as we have the content
			 * that matters to us stored in 'dbuf'. */
			err = buf_unmap(buf);
			if (err) {
				ntfs_error(vol->mp, "Failed to unmap buffer of "
						"mft record 0x%llx (error %d).",
						(unsigned long long)ni->mft_no,
						err);
				goto buf_err;
			}
			buf_brelse(buf);
			buf = NULL;
		}
		if (!mft_is_locked)
			lck_rw_unlock_shared(&mft_ni->lock);
		ni->mft_ni = mft_ni;
		ni->m_buf = buf;
		ni->m_dbuf = dbuf;
		ni->m = m;
		*mrec = m;
		ntfs_debug("Done.");
		return 0;
	}
	ntfs_error(vol->mp, "Mft record 0x%llx is corrupt.  Run chkdsk.",
			(unsigned long long)ni->mft_no);
	NVolSetErrors(vol);
	/* Error, release the buffer. */
	err = buf_unmap(buf);
	if (err)
		ntfs_error(vol->mp, "Failed to unmap buffer of mft record "
				"0x%llx (error %d).",
				(unsigned long long)ni->mft_no, err);
	err = EIO;
buf_err:
	buf_brelse(buf);
err:
	if (dbuf) {
		lck_mtx_unlock(&ni->buf_lock);
		IOFreeData(dbuf, vol->mft_record_size);
	}
	/*
	 * Release the iocount reference on the $MFT vnode.  We can ignore the
	 * return value as it always is zero.
	 */
	if (!mft_is_locked)
		lck_rw_unlock_shared(&mft_ni->lock);
	(void)vnode_put(mft_ni->vn);
	return err;
}

/**
 * ntfs_mft_record_unmap - release a mapped mft record
 * @ni:		ntfs inode whose mft record to unmap
 *
 * Unmap the buffer containing the mft record.
 */
void ntfs_mft_record_unmap(ntfs_inode *ni)
{
	ntfs_volume *const vol = ni->vol;

	ntfs_inode *mft_ni;
	buf_t buf;
	u8 *dbuf;
	errno_t err;

	ntfs_debug("Entering for mft_no 0x%llx.",
			(unsigned long long)ni->mft_no);
	mft_ni = ni->mft_ni;
	buf = ni->m_buf;
	dbuf = ni->m_dbuf;
	if (!mft_ni || !(buf || dbuf) || (buf && dbuf) || !ni->m)
		panic("%s(): Mft record 0x%llx is not mapped.\n", __FUNCTION__,
				(unsigned long long)ni->mft_no);

#if NTFS_SUB_SECTOR_MFT_RECORD_SIZE_RW
	if (dbuf) {
		const ino64_t buf_mft_no =
				ni->mft_no & ~vol->mft_records_per_sector_mask;
		const ino64_t buf_mft_record =
				ni->mft_no & vol->mft_records_per_sector_mask;
		const u32 buf_read_size = vol->sector_size;

		MFT_RECORD *m_tmp = NULL;

		err = buf_meta_bread(mft_ni->vn, buf_mft_no, buf_read_size,
				NOCRED, &buf);
		if (err) {
			ntfs_error(vol->mp, "Failed to read buffer of mft "
					"record 0x%llx (error %d, buf=%p).",
					(unsigned long long)ni->mft_no, err,
					buf);
		} else if ((err = buf_map(buf, (caddr_t*)&m_tmp))) {
			ntfs_error(vol->mp, "Failed to map buffer of mft "
					"record 0x%llx (error %d).",
					(unsigned long long)ni->mft_no, err);
			buf_brelse(buf);
			buf = NULL;
		} else {
			/* Transfer cached data to mapped buffer. */
			memcpy(&((char*) m_tmp)[buf_mft_record *
					vol->mft_record_size],
					dbuf, vol->mft_record_size);
		}
	}
#endif

	ni->mft_ni = NULL;
	ni->m_buf = NULL;
	ni->m_dbuf = NULL;
	ni->m = NULL;
	if (buf) {
		err = buf_unmap(buf);
		if (err)
			ntfs_error(vol->mp, "Failed to unmap buffer of mft "
					"record 0x%llx (error %d).",
					(unsigned long long)ni->mft_no, err);
		if (NInoTestClearMrecNeedsDirtying(ni)) {
			err = buf_bdwrite(buf);
			if (err) {
				ntfs_error(vol->mp, "Failed to write "
						"buffer of mft record 0x%llx "
						"(error %d).  Run chkdsk.",
						(unsigned long long)ni->mft_no,
						err);
				NVolSetErrors(vol);
			}
		} else
			buf_brelse(buf);
	}
	if (dbuf) {
		IOFreeData(dbuf, vol->mft_record_size);
		lck_mtx_unlock(&ni->buf_lock);
	}
	/*
	 * Release the iocount reference on the $MFT vnode.  We can ignore the
	 * return value as it always is zero.
	 */
	(void)vnode_put(mft_ni->vn);
	ntfs_debug("Done.");
}

/**
 * ntfs_extent_mft_record_map_ext - load an extent inode
 * @base_ni:		base ntfs inode
 * @mref:		mft reference of the extent inode to load
 * @ext_ni:		destination pointer for the loaded ntfs inode
 * @ext_mrec:		destination pointer for the mapped mft record
 * @mft_is_locked:	if true the caller holds the mft lock (@mft_ni->lock)
 *
 * Load the extent mft record @mref and attach it to its base inode @base_ni.
 *
 * On success *@ext_ni contains a pointer to the ntfs inode structure of the
 * mapped extent inode and *@ext_mrec contains a pointer to the mft record
 * structure of the mapped extent inode.
 *
 * If @mft_is_locked is true the caller holds the mft lock thus
 * ntfs_extent_mft_record_map_ext() will not try to take the same lock.  It is
 * then the responsibility of the caller that the mft is consistent and stable
 * for the duration of the call.
 *
 * Return 0 on success and errno on error.
 *
 * Note: The caller must hold an iocount reference on the vnode of the base
 * inode.
 */
errno_t ntfs_extent_mft_record_map_ext(ntfs_inode *base_ni, MFT_REF mref,
		ntfs_inode **ext_ni, MFT_RECORD **ext_mrec,
		const BOOL mft_is_locked)
{
	ino64_t mft_no;
	ntfs_inode **extent_nis = NULL;
	ntfs_inode *ni = NULL;
	MFT_RECORD *m;
	errno_t err;
	unsigned seq_no;
	int i;
	BOOL need_reclaim;

	mft_no = MREF(mref);
	seq_no = MSEQNO(mref);
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
		err = ntfs_mft_record_map_ext(ni, &m, mft_is_locked);
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
					"reference!  Corrupt file system.  "
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
	err = ntfs_mft_record_map_ext(ni, &m, mft_is_locked);
	if (err) {
		lck_mtx_unlock(&base_ni->extent_lock);
		ntfs_inode_reclaim(ni);
		goto map_err_out;
	}
	need_reclaim = FALSE;
	/* Verify the sequence number if it is present. */
	if (seq_no) {
		if (le16_to_cpu(m->sequence_number) != seq_no) {
			ntfs_error(base_ni->vol->mp, "Found stale extent mft "
					"reference!  Corrupt file system.  "
					"Run chkdsk.");
			need_reclaim = TRUE;
			err = EIO;
			goto unm_err_out;
		}
	} else {
		/*
		 * No sequence number was specified by the caller thus set the
		 * sequence number in the ntfs inode to the one in the mft
		 * record.
		 */
		ni->seq_no = le16_to_cpu(m->sequence_number);
	}
	/* Attach extent inode to base inode, reallocating memory if needed. */
	if ((base_ni->nr_extents + 1) * sizeof(ntfs_inode *) >
			base_ni->extent_alloc) {
		ntfs_inode **tmp;
		int new_size;

		new_size = base_ni->extent_alloc + 4 * sizeof(ntfs_inode *);
		tmp = IONewZero(ntfs_inode*, new_size);
		if (!tmp) {
			ntfs_error(base_ni->vol->mp, "Failed to allocate "
					"internal buffer.");
			need_reclaim = TRUE;
			err = ENOMEM;
			goto unm_err_out;
		}
		if (base_ni->extent_alloc) {
			if (base_ni->nr_extents > 0)
				memcpy(tmp, base_ni->extent_nis,
						base_ni->nr_extents *
						sizeof(ntfs_inode *));
			IODelete(base_ni->extent_nis, ntfs_inode*, base_ni->extent_alloc / sizeof(ntfs_inode*));
		}
		base_ni->extent_alloc = new_size;
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

static const char es[] = "  Leaving inconsistent metadata.  Unmount and run "
		"chkdsk.";

/**
 * ntfs_mft_record_sync - synchronize an inode's mft record with that on disk
 * @ni:		ntfs inode whose mft record to synchronize to disk
 *
 * If the mft record belonging to the ntfs inode @ni is cached in memory and is
 * dirty write it out.
 *
 * Note this function can only be called for real, base or extent, inodes, i.e.
 * not for synthetic, attribute or index, inodes.  Failure to obey this will
 * result in a panic.
 *
 * Return 0 on success and errno on error.
 *
 * Locking: The mft record must not be mapped or a deadlock will occur.
 */
errno_t ntfs_mft_record_sync(ntfs_inode *ni)
{
	ntfs_volume *vol;
	ntfs_inode *mft_ni;
	ino64_t buf_mft_no;
	ino64_t buf_mft_record;
	u32 buf_read_size;
	buf_t buf;
	errno_t err;

	if (NInoAttr(ni))
		panic("%s(): Called for attribute inode.\n", __FUNCTION__);
	ntfs_debug("Entering for mft record of %s inode 0x%llx.",
			(ni->nr_extents >= 0) ? "base" : "extent",
			(unsigned long long)ni->mft_no);
	vol = ni->vol;
	mft_ni = vol->mft_ni;
	if (!mft_ni) {
		ntfs_warning(vol->mp, "$MFT inode is missing from volume.");
		return ENOTSUP;
	}
	/* Get an iocount reference on the $MFT vnode. */
	err = vnode_get(mft_ni->vn);
	if (err) {
		ntfs_error(vol->mp, "Failed to get vnode for $MFT.");
		return err;
	}
	lck_rw_lock_shared(&mft_ni->lock);
#if NTFS_SUB_SECTOR_MFT_RECORD_SIZE_RW
	if (vol->mft_record_size < vol->sector_size) {
		/* If the MFT record size is smaller than a sector, we must
		 * align the buffer read to the sector size. */
		buf_mft_no = ni->mft_no & ~vol->mft_records_per_sector_mask;
		buf_mft_record = ni->mft_no & vol->mft_records_per_sector_mask;
		buf_read_size = vol->sector_size;
	} else {
#endif
		buf_mft_no = ni->mft_no;
		buf_mft_record = 0;
		buf_read_size = vol->mft_record_size;
#if NTFS_SUB_SECTOR_MFT_RECORD_SIZE_RW
	}
#endif
	/*
	 * Get the buffer if it is cached.  If it is not cached then it cannot
	 * be dirty either thus we do not need to write it.
	 */
	buf = buf_getblk(mft_ni->vn, buf_mft_no, buf_read_size, 0, 0,
			BLK_META | BLK_ONLYVALID);
	lck_rw_unlock_shared(&mft_ni->lock);
	(void)vnode_put(mft_ni->vn);
	if (!buf) {
		ntfs_debug("Mft record 0x%llx is not in cache, nothing to do.",
				(unsigned long long)ni->mft_no);
		return 0;
	}
	/* The buffer must be the right size. */
	if (buf_size(buf) != buf_read_size)
		panic("%s(): Buffer containing mft record 0x%llx has wrong "
				"size (0x%x instead of 0x%x).", __FUNCTION__,
				(unsigned long long)ni->mft_no,
				buf_size(buf), buf_read_size);
#if NTFS_SUB_SECTOR_MFT_RECORD_SIZE_RW
	if (ni->m_dbuf) {
		/* Need to map buffer and transfer data from the double buffer
		 * in order to update. */
		/* Note: Not sure if this is necessary... do we ever sync in
		 * between a map/unmap session? Actually we can't even get here
		 * if we are between a map/unmap can we, as we wouldn't be able
		 * to lock ni->buf_lock. So all this code is possibly
		 * pointless. */
		char *m_sec = NULL;
		MFT_RECORD *m = NULL;

		err = buf_map(buf, (caddr_t*)&m_sec);
		if (err) {
			ntfs_error(vol->mp, "Failed to map buffer of mft record "
					"0x%llx (error %d).",
					(unsigned long long)ni->mft_no, err);
			buf_brelse(buf);
			return err;
		}
		/* Transfer cached data to mapped buffer. */
		m = (MFT_RECORD*)&m_sec[buf_mft_record * vol->mft_record_size];
		if (memcmp(m, ni->m_dbuf, vol->mft_record_size)) {
			memcpy(m, ni->m_dbuf, vol->mft_record_size);
		}
		err = buf_unmap(buf);
		if (err) {
			/* Not sure if it makes sense to catch this and error
			 * out. The only way this can happen appears to be if
			 * the buffer isn't mapped or if it's NULL, both of
			 * which cannot happen if we are here. */
			ntfs_error(vol->mp, "Failed to unmap buffer of mft "
					"record 0x%llx (error %d).",
					(unsigned long long)ni->mft_no, err);
			buf_brelse(buf);
			return err;
		}
	}
#endif
	/* If the buffer is clean there is nothing to do. */
	if (!(buf_flags(buf) & B_DELWRI)) {
		ntfs_debug("Mft record 0x%llx is in cache but not dirty, "
				"nothing to do.",
				(unsigned long long)ni->mft_no);
		buf_brelse(buf);
		return 0;
	}
	/* The buffer is dirty, write it now. */
	err = buf_bwrite(buf);
	if (!err)
		ntfs_debug("Done.");
	else
		ntfs_error(vol->mp, "Failed to write mft record 0x%llx (error "
				"%d).", (unsigned long long)ni->mft_no, err);
	return err;
}

/**
 * ntfs_mft_mirror_sync - synchronize an mft record to the mft mirror
 * @vol:	ntfs volume on which the mft record to synchronize resides
 * @rec_no:	mft record number to synchronize
 * @m:		mapped, mst protected (extent) mft record to synchronize
 * @sync:	if true perform synchronous i/o otherwise use async i/o
 *
 * Write the mapped, mst protected (extent) mft record number @rec_no with data
 * @m to the mft mirror ($MFTMirr) of the ntfs volume @vol.
 *
 * On success return 0.  On error return errno and set the volume errors flag
 * in the ntfs volume @vol.
 */
errno_t ntfs_mft_mirror_sync(ntfs_volume *vol, const s64 rec_no,
		const MFT_RECORD *m, const BOOL sync)
{
	s64 data_size;
	ntfs_inode *mirr_ni;
	vnode_t mirr_vn;
	ino64_t buf_mft_no;
	ino64_t buf_mft_record;
	u32 buf_read_size;
	buf_t buf;
	char *mirr_sec;
	MFT_RECORD *mirr;
	errno_t err;

	ntfs_debug("Entering for rec_no 0x%llx.", (unsigned long long)rec_no);
	mirr_ni = vol->mftmirr_ni;
	if (!mirr_ni) {
		/* This could happen during umount... */
		ntfs_error(vol->mp, "Umount time mft mirror syncing is not "
				"implemented yet.  %s", ntfs_please_email);
		return ENOTSUP;
	}
	mirr_vn = mirr_ni->vn;
	/*
	 * Protect against changes in initialized_size and thus against
	 * truncation also.
	 */
	lck_rw_lock_shared(&mirr_ni->lock);
	if (rec_no >= vol->mftmirr_size)
		panic("%s(): rec_no >= vol->mftmirr_size\n", __FUNCTION__);
	err = vnode_get(mirr_vn);
	if (err) {
		ntfs_error(vol->mp, "Failed to get vnode for mft mirror.");
		goto err;
	}
	lck_spin_lock(&mirr_ni->size_lock);
	data_size = ubc_getsize(mirr_vn);
	if (data_size > mirr_ni->data_size)
		data_size = mirr_ni->data_size;
	/* Byte offset of the mft record. */
	if ((rec_no << vol->mft_record_size_shift) + vol->mft_record_size >
			mirr_ni->initialized_size) {
		lck_spin_unlock(&mirr_ni->size_lock);
		ntfs_error(vol->mp, "Write past the initialized size of mft "
				"mirror.");
		err = EIO;
		goto put;
	}
	lck_spin_unlock(&mirr_ni->size_lock);
#if NTFS_SUB_SECTOR_MFT_RECORD_SIZE_RW
	if (vol->mft_record_size < vol->sector_size) {
		/* If the MFT record size is smaller than a sector, we must
		 * align the buffer read to the sector size. */
		buf_mft_no = rec_no & ~vol->mft_records_per_sector_mask;
		buf_mft_record = rec_no & vol->mft_records_per_sector_mask;
		buf_read_size = vol->sector_size;
	} else {
#endif
		buf_mft_no = rec_no;
		buf_mft_record = 0;
		buf_read_size = vol->mft_record_size;
#if NTFS_SUB_SECTOR_MFT_RECORD_SIZE_RW
	}
#endif
	/*
	 * Map the buffer containing the mft mirror record.
	 *
	 * Note we use buf_getblk() as we do not care whether the record is
	 * up-to-date in memory or not as we are about to overwrite it.
	 */
	buf = buf_getblk(mirr_vn, buf_mft_no, buf_read_size, 0, 0, BLK_META);
	if (!buf)
		panic("%s(): buf_getblk() returned NULL.\n", __FUNCTION__);
	err = buf_map(buf, (caddr_t*)&mirr_sec);
	if (err) {
		ntfs_error(vol->mp, "Failed to map buffer of mft mirror "
				"record %lld (error %d).",
				(unsigned long long)rec_no, err);
		buf_brelse(buf);
		goto put;
	}
	mirr = (MFT_RECORD*)&mirr_sec[buf_mft_record * vol->mft_record_size];
	memcpy(mirr, m, vol->mft_record_size);
	err = buf_unmap(buf);
	if (err)
		ntfs_error(vol->mp, "Failed to unmap buffer of mft mirror "
				"record %lld (error %d).",
				(unsigned long long)rec_no, err);
	/*
	 * If the i/o is synchronous use a synchronous write for the mft mirror
	 * as well.  If the i/o is asynchronous then do the write
	 * asynchronously.  Note we do not use a delayed write because we want
	 * to ensure that the mft mirror will be brought up-to-date as soon as
	 * possible because we are using delayed writes on the mft itself thus
	 * in case of a crash we want to have a valid and up-to-date mft mirror
	 * on disk that we can recover from even when the mft is not valid or
	 * up-to-date.
	 *
	 * FIXME: For maximum performance we could delete the above comment and
	 * change the buf_bawrite() to buf_bdwrite().
	 */
	if (sync)
		err = buf_bwrite(buf);
	else
		err = buf_bawrite(buf);
	if (err)
		ntfs_error(vol->mp, "Failed to write buffer of mft mirror "
				"record %lld (error %d).",
				(unsigned long long)rec_no, err);
put:
	(void)vnode_put(mirr_vn);
err:
	lck_rw_unlock_shared(&mirr_ni->lock);
	if (!err)
		ntfs_debug("Done.");
	else {
		ntfs_error(vol->mp, "Failed to synchronize mft mirror (error "
				"code %d).  Volume will be left marked dirty "
				"on unmount.  Run chkdsk.", err);
		NVolSetErrors(vol);
	}
	return err;
}

/**
 * ntfs_mft_bitmap_find_and_alloc_free_rec_nolock - see name
 * @vol:	volume on which to search for a free mft record
 * @base_ni:	open base inode if allocating an extent mft record or NULL
 * @mft_no:	destination in which to return the allocated mft record number
 *
 * Search for a free mft record in the mft bitmap attribute on the ntfs volume
 * @vol and return the allocated mft record number in *@mft_no.
 *
 * If @base_ni is NULL start the search at the default allocator position.
 *
 * If @base_ni is not NULL start the search at the mft record after the base
 * mft record @base_ni.
 *
 * Return 0 on success and errno on error.  An error code of ENOSPC means that
 * there are no free mft records in the currently initialized mft bitmap.
 *
 * Locking: - Caller must hold @vol->mftbmp_lock for writing.
 *	    - Caller must hold @vol->mftbmp_ni->lock.
 */
static errno_t ntfs_mft_bitmap_find_and_alloc_free_rec_nolock(ntfs_volume *vol,
		ntfs_inode *base_ni, s64 *mft_no)
{
	s64 pass_end, ll, data_pos, pass_start, ofs;
	ntfs_inode *mftbmp_ni;
	upl_t upl;
	upl_page_info_array_t pl;
	u8 *buf, *byte;
	unsigned page_ofs, size, bit;
	u8 pass, b;

	ntfs_debug("Searching for free mft record in the currently "
			"initialized mft bitmap.");
	mftbmp_ni = vol->mftbmp_ni;
	if (!mftbmp_ni)
		panic("%s: !mftbmp_ni\n", __FUNCTION__);
	/*
	 * Set the end of the pass making sure we do not overflow the mft
	 * bitmap.
	 */
	if (!vol->mft_ni)
		panic("%s: !mft_ni\n", __FUNCTION__);
	lck_spin_lock(&vol->mft_ni->size_lock);
	pass_end = vol->mft_ni->allocated_size >> vol->mft_record_size_shift;
	lck_spin_unlock(&vol->mft_ni->size_lock);
	lck_spin_lock(&mftbmp_ni->size_lock);
	ll = mftbmp_ni->initialized_size << 3;
	lck_spin_unlock(&mftbmp_ni->size_lock);
	if (pass_end > ll)
		pass_end = ll;
	pass = 1;
	if (!base_ni)
		data_pos = vol->mft_data_pos;
	else
		data_pos = base_ni->mft_no + 1;
	if (data_pos < 24)
		data_pos = 24;
	if (data_pos >= pass_end) {
		data_pos = 24;
		pass = 2;
		/* This happens on a freshly formatted volume. */
		if (data_pos >= pass_end)
			goto no_space;
	}
	pass_start = data_pos;
	ntfs_debug("Starting bitmap search: pass %u, pass_start 0x%llx, "
			"pass_end 0x%llx, data_pos 0x%llx.", (unsigned)pass,
			(unsigned long long)pass_start,
			(unsigned long long)pass_end,
			(unsigned long long)data_pos);
	/* Loop until a free mft record is found. */
	do {
		/* Cap size to pass_end. */
		ofs = data_pos >> 3;
		page_ofs = (unsigned)ofs & PAGE_MASK;
		size = PAGE_SIZE - page_ofs;
		ll = ((pass_end + 7) >> 3) - ofs;
		if (size > ll)
			size = ll;
		size <<= 3;
		/*
		 * If we are still within the active pass, search the next page
		 * for a zero bit.
		 */
		if (size) {
			errno_t err;

			err = ntfs_page_map(mftbmp_ni, ofs & ~PAGE_MASK_64,
					&upl, &pl, &buf, TRUE);
			if (err) {
				ntfs_error(vol->mp, "Failed to read mft "
						"bitmap, aborting.");
				return err;
			}
			buf += page_ofs;
			bit = (unsigned)data_pos & 7;
			data_pos &= ~7ULL;
			ntfs_debug("Before inner for loop: size 0x%x, "
					"data_pos 0x%llx, bit 0x%x", size,
					(unsigned long long)data_pos, bit);
			for (; bit < size && data_pos + bit < pass_end;
					bit &= ~7, bit += 8) {
				byte = buf + (bit >> 3);
				if (*byte == 0xff)
					continue;
				/*
				 * TODO: There does not appear to be a ffz()
				 * function in the kernel. )-:  If/when the
				 * kernel has an ffz() function, switch the
				 * below code to use it.
				 *
				 * So emulate "ffz(x)" using "ffs(~x) - 1"
				 * which gives the same result but incurs extra
				 * CPU overhead.
				 */
				b = ffs(~(unsigned long)*byte) - 1;
				if (b < 8 && b >= (bit & 7)) {
					ll = data_pos + (bit & ~7) + b;
					if (ll > (1LL << 32)) {
						ntfs_page_unmap(mftbmp_ni,
								upl, pl, FALSE);
						goto no_space;
					}
					*byte |= 1 << b;
					ntfs_page_unmap(mftbmp_ni, upl, pl,
							TRUE);
					ntfs_debug("Done.  (Found and "
							"allocated mft record "
							"0x%llx.)",
							(unsigned long long)ll);
					*mft_no = ll;
					return 0;
				}
			}
			ntfs_debug("After inner for loop: size 0x%x, "
					"data_pos 0x%llx, bit 0x%x", size,
					(unsigned long long)data_pos, bit);
			data_pos += size;
			ntfs_page_unmap(mftbmp_ni, upl, pl, FALSE);
			/*
			 * If the end of the pass has not been reached yet,
			 * continue searching the mft bitmap for a zero bit.
			 */
			continue;
		}
		/* If we just did the second pass we are done. */
		if (pass >= 2)
			break;
		/*
		 * Do the second pass, in which we scan the first part of the
		 * zone which we omitted earlier.
		 */
		pass++;
		pass_end = pass_start;
		data_pos = pass_start = 24;
		ntfs_debug("pass %u, pass_start 0x%llx, pass_end 0x%llx.",
				(unsigned)pass, (unsigned long long)pass_start,
				(unsigned long long)pass_end);
		/*
		 * If the end of the pass has not been reached yet, continue
		 * searching the mft bitmap for a zero bit.
		 */
	} while (data_pos < pass_end);
no_space:
	ntfs_debug("Done.  (No free mft records left in currently initialized "
			"mft bitmap.)");
	return ENOSPC;
}

/**
 * ntfs_mft_bitmap_extend_allocation_nolock - extend mft bitmap by a cluster
 * @vol:	volume on which to extend the mft bitmap attribute
 *
 * Extend the mft bitmap attribute allocation on the ntfs volume @vol by one
 * cluster.
 *
 * Note: Only changes allocated_size, i.e. does not touch initialized_size or
 * data_size.
 *
 * Return 0 on success and errno on error.
 *
 * Locking: - Caller must hold @vol->mftbmp_lock for writing.
 *	    - Caller must hold @vol->mftbmp_ni->lock for writing.
 *	    - This function takes @vol->mftbmp_ni->rl.lock for writing and
 *	      releases it before returning.
 *	    - This function takes @vol->lcnbmp_lock for writing and releases it
 *	      before returning.
 */
static errno_t ntfs_mft_bitmap_extend_allocation_nolock(ntfs_volume *vol)
{
	VCN vcn, lowest_vcn = 0;
	LCN lcn;
	s64 allocated_size, ll;
	ntfs_inode *mft_ni, *mftbmp_ni, *lcnbmp_ni;
	ntfs_rl_element *rl;
	upl_t upl;
	upl_page_info_array_t pl;
	u8 *kaddr, *b;
	MFT_RECORD *m;
	ntfs_attr_search_ctx *ctx;
	ATTR_RECORD *a;
	unsigned mp_size, attr_len = 0;
	errno_t err, err2;
	BOOL mp_rebuilt = FALSE;
	u8 tb;

	ntfs_debug("Extending mft bitmap allocation.");
	mft_ni = vol->mft_ni;
	mftbmp_ni = vol->mftbmp_ni;
	lcnbmp_ni = vol->lcnbmp_ni;
	/*
	 * Determine the last lcn of the mft bitmap.  The allocated size of the
	 * mft bitmap cannot be zero so we are ok to not check for it being
	 * zero first.
	 */
	lck_rw_lock_exclusive(&mftbmp_ni->rl.lock);
	lck_spin_lock(&mftbmp_ni->size_lock);
	allocated_size = mftbmp_ni->allocated_size;
	lck_spin_unlock(&mftbmp_ni->size_lock);
	vcn = (allocated_size - 1) >> vol->cluster_size_shift;
	err = ntfs_attr_find_vcn_nolock(mftbmp_ni, vcn, &rl, NULL);
	if (err || !rl || !rl->length || rl->lcn < 0 || rl[1].length ||
			rl[1].vcn != vcn + 1) {
		lck_rw_unlock_exclusive(&mftbmp_ni->rl.lock);
		ntfs_error(vol->mp, "Failed to determine last allocated "
				"cluster of mft bitmap attribute.");
		if (!err)
			err = EIO;
		return err;
	}
	lcn = rl->lcn + rl->length;
	ntfs_debug("Last lcn of mft bitmap attribute is 0x%llx.",
			(unsigned long long)lcn);
	lck_rw_lock_exclusive(&vol->lcnbmp_lock);
	err = vnode_get(lcnbmp_ni->vn);
	if (err) {
		ntfs_error(vol->mp, "Failed to get vnode for $Bitmap.");
		lck_rw_unlock_exclusive(&vol->lcnbmp_lock);
		lck_rw_unlock_exclusive(&mftbmp_ni->rl.lock);
		return err;
	}
	lck_rw_lock_shared(&lcnbmp_ni->lock);
	/*
	 * Attempt to get the cluster following the last allocated cluster by
	 * hand as it may be in the MFT zone so the allocator would not give it
	 * to us.
	 */
	ll = lcn >> 3;
	err = ntfs_page_map(lcnbmp_ni, ll & ~PAGE_MASK_64, &upl, &pl, &kaddr,
			TRUE);
	if (err) {
		lck_rw_unlock_shared(&lcnbmp_ni->lock);
		(void)vnode_put(lcnbmp_ni->vn);
		lck_rw_unlock_exclusive(&vol->lcnbmp_lock);
		lck_rw_unlock_exclusive(&mftbmp_ni->rl.lock);
		ntfs_error(vol->mp, "Failed to read from lcn bitmap.");
		return err;
	}
	b = kaddr + ((unsigned)ll & PAGE_MASK);
	tb = 1 << ((unsigned)lcn & 7);
	if (*b != 0xff && !(*b & tb)) {
		/* Next cluster is free, allocate it. */
		*b |= tb;
		vol->nr_free_clusters--;
		if (vol->nr_free_clusters < 0)
			vol->nr_free_clusters = 0;
		ntfs_page_unmap(lcnbmp_ni, upl, pl, TRUE);
		lck_rw_unlock_shared(&lcnbmp_ni->lock);
		(void)vnode_put(lcnbmp_ni->vn);
		lck_rw_unlock_exclusive(&vol->lcnbmp_lock);
		/* Update the mft bitmap runlist. */
		rl->length++;
		rl[1].vcn++;
		ntfs_debug("Appending one cluster to mft bitmap.");
	} else {
		ntfs_runlist runlist;

		ntfs_page_unmap(lcnbmp_ni, upl, pl, FALSE);
		lck_rw_unlock_shared(&lcnbmp_ni->lock);
		(void)vnode_put(lcnbmp_ni->vn);
		lck_rw_unlock_exclusive(&vol->lcnbmp_lock);
		/* Allocate a cluster from the DATA_ZONE. */
		runlist.rl = NULL;
		runlist.alloc_count = runlist.elements = 0;
		err = ntfs_cluster_alloc(vol, vcn + 1, 1, lcn, DATA_ZONE,
				TRUE, &runlist);
		if (err) {
			lck_rw_unlock_exclusive(&mftbmp_ni->rl.lock);
			ntfs_error(vol->mp, "Failed to allocate a cluster for "
					"the mft bitmap.");
			if (err != ENOMEM && err != ENOSPC)
				err = EIO;
			return err;
		}
		err = ntfs_rl_merge(&mftbmp_ni->rl, &runlist);
		if (err) {
			lck_rw_unlock_exclusive(&mftbmp_ni->rl.lock);
			ntfs_error(vol->mp, "Failed to merge runlists for mft "
					"bitmap.");
			if (err != ENOMEM)
				err = EIO;
			err2 = ntfs_cluster_free_from_rl(vol, runlist.rl, 0,
					-1, NULL);
			if (err2) {
				ntfs_error(vol->mp, "Failed to release "
						"allocated cluster (error "
						"%d).%s", err2, es);
				NVolSetErrors(vol);
			}
			IODeleteData(runlist.rl, ntfs_rl_element, runlist.alloc_count);
			return err;
		}
		ntfs_debug("Adding one run to mft bitmap.");
	}
	/* Update the attribute record as well. */
	err = ntfs_mft_record_map(mft_ni, &m);
	if (err) {
		ntfs_error(vol->mp, "Failed to map mft record.");
		m = NULL;
		ctx = NULL;
		goto undo_alloc;
	}
	ctx = ntfs_attr_search_ctx_get(mft_ni, m);
	if (!ctx) {
		ntfs_error(vol->mp, "Failed to get search context.");
		err = ENOMEM;
		goto undo_alloc;
	}
	err = ntfs_attr_lookup(mftbmp_ni->type, mftbmp_ni->name,
			mftbmp_ni->name_len, vcn, NULL, 0, ctx);
	if (err) {
		ntfs_error(vol->mp, "Failed to find last attribute extent of "
				"mft bitmap attribute.");
		if (err == ENOENT)
			err = EIO;
		goto undo_alloc;
	}
	m = ctx->m;
	a = ctx->a;
	/* Find the runlist element with which the attribute extent starts. */
	lowest_vcn = sle64_to_cpu(a->lowest_vcn);
	rl = ntfs_rl_find_vcn_nolock(mftbmp_ni->rl.rl, lowest_vcn);
	if (!rl)
		panic("%s(): !rl\n", __FUNCTION__);
	if (!rl->length)
		panic("%s(): !rl->length\n", __FUNCTION__);
	if (rl->lcn < LCN_HOLE)
		panic("%s(): rl->lcn < LCN_HOLE\n", __FUNCTION__);
	/* Get the size for the new mapping pairs array for this extent. */
	err = ntfs_get_size_for_mapping_pairs(vol, rl, lowest_vcn, -1,
			&mp_size);
	if (err) {
		ntfs_error(vol->mp, "Get size for mapping pairs failed for "
				"mft bitmap attribute extent.");
		goto undo_alloc;
	}
	/* Extend the attribute record to fit the bigger mapping pairs array. */
	attr_len = le32_to_cpu(a->length);
	err = ntfs_attr_record_resize(m, a, mp_size +
			le16_to_cpu(a->mapping_pairs_offset));
	if (err) {
		if (err != ENOSPC) {
			ntfs_error(vol->mp, "Failed to resize attribute "
					"record for mft bitmap attribute.");
			goto undo_alloc;
		}
		// TODO: Deal with this by moving this extent to a new mft
		// record or by starting a new extent in a new mft record or by
		// moving other attributes out of this mft record.
		// Note: It will need to be a special mft record and if none of
		// those are available it gets rather complicated...
		ntfs_error(vol->mp, "Not enough space in this mft record to "
				"accomodate extended mft bitmap attribute "
				"extent.  Cannot handle this yet.");
		err = ENOTSUP;
		goto undo_alloc;
	}
	mp_rebuilt = TRUE;
	/* Generate the mapping pairs array directly into the attr record. */
	err = ntfs_mapping_pairs_build(vol, (s8*)a +
			le16_to_cpu(a->mapping_pairs_offset), mp_size, rl,
			lowest_vcn, -1, NULL);
	if (err) {
		ntfs_error(vol->mp, "Failed to build mapping pairs array for "
				"mft bitmap attribute (error %d).", err);
		err = EIO;
		goto dirty_undo_alloc;
	}
	/* Update the highest_vcn. */
	a->highest_vcn = cpu_to_sle64(vcn + 1);
	/*
	 * We now have extended the mft bitmap allocated_size by one cluster.
	 * Reflect this in the ntfs_inode structure and the attribute record.
	 */
	if (a->lowest_vcn) {
		/*
		 * We are not in the first attribute extent, switch to it, but
		 * first ensure the changes will make it to disk later.
		 */
		NInoSetMrecNeedsDirtying(ctx->ni);
		ntfs_attr_search_ctx_reinit(ctx);
		err = ntfs_attr_lookup(mftbmp_ni->type, mftbmp_ni->name,
				mftbmp_ni->name_len, 0, NULL, 0, ctx);
		if (err)
			goto restore_undo_alloc;
		/* @m is not used any more so no need to set it. */
		a = ctx->a;
	}
	lck_spin_lock(&mftbmp_ni->size_lock);
	mftbmp_ni->allocated_size += vol->cluster_size;
	a->allocated_size = cpu_to_sle64(mftbmp_ni->allocated_size);
	lck_spin_unlock(&mftbmp_ni->size_lock);
	/* Ensure the changes make it to disk. */
	NInoSetMrecNeedsDirtying(ctx->ni);
	ntfs_attr_search_ctx_put(ctx);
	ntfs_mft_record_unmap(mft_ni);
	lck_rw_unlock_exclusive(&mftbmp_ni->rl.lock);
	ntfs_debug("Done.");
	return 0;
restore_undo_alloc:
	ntfs_error(vol->mp, "Failed to find first attribute extent of mft "
			"bitmap attribute.");
	if (err == ENOENT)
		err = EIO;
	ntfs_attr_search_ctx_reinit(ctx);
	err2 = ntfs_attr_lookup(mftbmp_ni->type, mftbmp_ni->name,
			mftbmp_ni->name_len, vcn, NULL, 0, ctx);
	if (err2) {
		ntfs_error(vol->mp, "Failed to find last attribute extent of "
				"mft bitmap attribute (error %d).%s", err2, es);
		lck_spin_lock(&mftbmp_ni->size_lock);
		mftbmp_ni->allocated_size += vol->cluster_size;
		lck_spin_unlock(&mftbmp_ni->size_lock);
		ntfs_attr_search_ctx_put(ctx);
		ntfs_mft_record_unmap(mft_ni);
		lck_rw_unlock_exclusive(&mftbmp_ni->rl.lock);
		/*
		 * The only thing that is now wrong is the allocated size of the
		 * base attribute extent which chkdsk should be able to fix.
		 */
		NVolSetErrors(vol);
		return err;
	}
	ctx->a->highest_vcn = cpu_to_sle64(vcn);
dirty_undo_alloc:
	/*
	 * Need to mark the mft record for dirtying because ntfs_cluster_free()
	 * may drop the mft record on the floor otherwise.
	 */
	NInoSetMrecNeedsDirtying(ctx->ni);
undo_alloc:
	err2 = ntfs_cluster_free(mftbmp_ni, vcn + 1, -1, ctx, NULL);
	if (err2 || ctx->is_error) {
		ntfs_error(vol->mp, "Failed to release allocated cluster in "
				"error code path (error %d).%s",
				ctx->is_error ? ctx->error : err2, es);
		NVolSetErrors(vol);
	}
	/*
	 * If the runlist truncation fails and/or the search context is no
	 * longer valid, we cannot resize the attribute record or build the
	 * mapping pairs array thus we mark the volume dirty and tell the user
	 * to run chkdsk.
	 */
	err2 = ntfs_rl_truncate_nolock(vol, &mftbmp_ni->rl, vcn + 1);
	if (err2) {
		ntfs_error(vol->mp, "Failed to truncate attribute runlist s "
				"in error code path (error %d).%s", err2, es);
		NVolSetErrors(vol);
	} else if (mp_rebuilt) {
		a = ctx->a;
		err2 = ntfs_attr_record_resize(ctx->m, a, attr_len);
		if (err2) {
			ntfs_error(vol->mp, "Failed to restore attribute "
					"record in error code path (error "
					"%d).%s", err2, es);
			NVolSetErrors(vol);
		} else /* if (!err2) */ {
			u16 mp_ofs = le16_to_cpu(a->mapping_pairs_offset);
			err2 = ntfs_mapping_pairs_build(vol, (s8*)a + mp_ofs,
					attr_len - mp_ofs, mftbmp_ni->rl.rl,
					lowest_vcn, -1, NULL);
			if (err2) {
				ntfs_error(vol->mp, "Failed to restore "
						"mapping pairs array in error "
						"code path (error %d).%s",
						err2, es);
				NVolSetErrors(vol);
			}
			NInoSetMrecNeedsDirtying(ctx->ni);
		}
	}
	if (ctx)
		ntfs_attr_search_ctx_put(ctx);
	if (m)
		ntfs_mft_record_unmap(mft_ni);
	lck_rw_unlock_exclusive(&mftbmp_ni->rl.lock);
	return err;
}

/**
 * ntfs_mft_bitmap_extend_initialized_nolock - extend mftbmp initialized data
 * @vol:	volume on which to extend the mft bitmap attribute
 *
 * Extend the initialized portion of the mft bitmap attribute on the ntfs
 * volume @vol by 8 bytes.
 *
 * Note: Only changes initialized_size and data_size, i.e. requires that
 * allocated_size is big enough to fit the new initialized_size.
 *
 * Return 0 on success and error on error.
 *
 * Locking: - Caller must hold @vol->mftbmp_lock for writing.
 *	    - Caller must hold @vol->mftbmp_ni->lock for writing.
 */
static errno_t ntfs_mft_bitmap_extend_initialized_nolock(ntfs_volume *vol)
{
	s64 old_data_size, old_initialized_size;
	ntfs_inode *mft_ni, *mftbmp_ni;
	MFT_RECORD *m;
	ntfs_attr_search_ctx *ctx;
	ATTR_RECORD *a;
	errno_t err, err2;

	ntfs_debug("Extending mft bitmap initiailized (and data) size.");
	mft_ni = vol->mft_ni;
	mftbmp_ni = vol->mftbmp_ni;
	/* Get the attribute record. */
	err = ntfs_mft_record_map(mft_ni, &m);
	if (err) {
		ntfs_error(vol->mp, "Failed to map mft record.");
		return err;
	}
	ctx = ntfs_attr_search_ctx_get(mft_ni, m);
	if (!ctx) {
		ntfs_error(vol->mp, "Failed to get search context.");
		err = ENOMEM;
		goto unm_err;
	}
	err = ntfs_attr_lookup(mftbmp_ni->type, mftbmp_ni->name,
			mftbmp_ni->name_len, 0, NULL, 0, ctx);
	if (err) {
		ntfs_error(vol->mp, "Failed to find first attribute extent of "
				"mft bitmap attribute.");
		if (err == ENOENT)
			err = EIO;
		goto put_err;
	}
	a = ctx->a;
	lck_spin_lock(&mftbmp_ni->size_lock);
	old_data_size = mftbmp_ni->data_size;
	old_initialized_size = mftbmp_ni->initialized_size;
	/*
	 * We can simply update the initialized_size before filling the space
	 * with zeroes because the caller is holding the mft bitmap lock for
	 * writing which ensures that no one else is trying to access the data.
	 */
	mftbmp_ni->initialized_size += 8;
	a->initialized_size = cpu_to_sle64(mftbmp_ni->initialized_size);
	if (mftbmp_ni->initialized_size > old_data_size) {
		const s64 init_size = mftbmp_ni->initialized_size;
		mftbmp_ni->data_size = init_size;
		a->data_size = cpu_to_sle64(init_size);
		lck_spin_unlock(&mftbmp_ni->size_lock);
		if (!ubc_setsize(mftbmp_ni->vn, init_size))
			panic("%s(): !ubc_setsize(mftbmp_ni->vn, init_size)\n",
					__FUNCTION__);
	} else
		lck_spin_unlock(&mftbmp_ni->size_lock);
	/* Ensure the changes make it to disk. */
	NInoSetMrecNeedsDirtying(ctx->ni);
	ntfs_attr_search_ctx_put(ctx);
	ntfs_mft_record_unmap(mft_ni);
	/* Initialize the mft bitmap attribute value with zeroes. */
	err = ntfs_attr_set(mftbmp_ni, old_initialized_size, 8, 0);
	if (!err) {
		ntfs_debug("Done.  (Wrote eight initialized bytes to mft "
				"bitmap.");
		return 0;
	}
	ntfs_error(vol->mp, "Failed to write to mft bitmap.");
	/* Try to recover from the error. */
	err2 = ntfs_mft_record_map(mft_ni, &m);
	if (err2) {
		ntfs_error(vol->mp, "Failed to map mft record in error code "
				"path (error %d).%s", err2, es);
		NVolSetErrors(vol);
		return err;
	}
	ctx = ntfs_attr_search_ctx_get(mft_ni, m);
	if (!ctx) {
		ntfs_error(vol->mp, "Failed to get search context.%s", es);
		NVolSetErrors(vol);
		goto unm_err;
	}
	err2 = ntfs_attr_lookup(mftbmp_ni->type, mftbmp_ni->name,
			mftbmp_ni->name_len, 0, NULL, 0, ctx);
	if (err2) {
		ntfs_error(vol->mp, "Failed to find first attribute extent of "
				"mft bitmap attribute in error code path "
				"(error %d).%s", err2, es);
		NVolSetErrors(vol);
		goto put_err;
	}
	a = ctx->a;
	lck_spin_lock(&mftbmp_ni->size_lock);
	mftbmp_ni->initialized_size = old_initialized_size;
	a->initialized_size = cpu_to_sle64(old_initialized_size);
	if (ubc_getsize(mftbmp_ni->vn) != old_data_size) {
		mftbmp_ni->data_size = old_data_size;
		a->data_size = cpu_to_sle64(old_data_size);
		lck_spin_unlock(&mftbmp_ni->size_lock);
		if (!ubc_setsize(mftbmp_ni->vn, old_data_size))
			ntfs_error(vol->mp, "Failed to restore UBC size.  "
					"Leaving UBC size out of sync with "
					"attribute data size.");
	} else
		lck_spin_unlock(&mftbmp_ni->size_lock);
	NInoSetMrecNeedsDirtying(ctx->ni);
#ifdef DEBUG
	lck_spin_lock(&mftbmp_ni->size_lock);
	ntfs_debug("Restored status of mftbmp: allocated_size 0x%llx, "
			"data_size 0x%llx, initialized_size 0x%llx.",
			(unsigned long long)mftbmp_ni->allocated_size,
			(unsigned long long)mftbmp_ni->data_size,
			(unsigned long long)mftbmp_ni->initialized_size);
	lck_spin_unlock(&mftbmp_ni->size_lock);
#endif /* DEBUG */
put_err:
	ntfs_attr_search_ctx_put(ctx);
unm_err:
	ntfs_mft_record_unmap(mft_ni);
	return err;
}

/**
 * ntfs_mft_data_extend_allocation_nolock - extend mft data attribute
 * @vol:	volume on which to extend the mft data attribute
 *
 * Extend the mft data attribute on the ntfs volume @vol by 16 mft records
 * worth of clusters or if not enough space for this by one mft record worth
 * of clusters.
 *
 * Note: Only changes allocated_size, i.e. does not touch initialized_size or
 * data_size.
 *
 * Return 0 on success and errno on error.
 *
 * Locking: - Caller must hold @vol->mftbmp_lock for writing.
 *	    - Caller must hold @vol->mft_ni->lock for writing.
 *	    - This function takes @vol->mft_ni->rl.lock for writing and
 *	      releases it before returning.
 *	    - This function calls functions which take @vol->lcnbmp_lock for
 *	      writing and release it before returning.
 */
static errno_t ntfs_mft_data_extend_allocation_nolock(ntfs_volume *vol)
{
	VCN vcn, lowest_vcn = 0;
	LCN lcn;
	s64 allocated_size, min_nr, nr;
	ntfs_inode *mft_ni;
	ntfs_rl_element *rl;
	MFT_RECORD *m;
	ntfs_attr_search_ctx *ctx;
	ATTR_RECORD *a;
	unsigned mp_size, attr_len = 0;
	errno_t err, err2;
	BOOL mp_rebuilt = FALSE;
	ntfs_runlist runlist;

	ntfs_debug("Extending mft data allocation.");
	mft_ni = vol->mft_ni;
	lck_spin_lock(&mft_ni->size_lock);
	allocated_size = mft_ni->allocated_size;
	lck_spin_unlock(&mft_ni->size_lock);
	vcn = (allocated_size - 1) >> vol->cluster_size_shift;
	/*
	 * Determine the preferred allocation location, i.e. the last lcn of
	 * the mft data attribute.
	 */
	lck_rw_lock_exclusive(&mft_ni->rl.lock);
	if (mft_ni->rl.elements > 1)
		rl = &mft_ni->rl.rl[mft_ni->rl.elements - 2];
	else
		rl = mft_ni->rl.rl;
	if (!rl || !rl->length || rl->lcn < 0 || rl[1].length ||
			rl[1].vcn != vcn + 1) {
		ntfs_error(vol->mp, "Failed to determine last allocated "
				"cluster of mft data attribute.");
		lck_rw_unlock_exclusive(&mft_ni->rl.lock);
		return EIO;
	}
	lcn = rl->lcn + rl->length;
	ntfs_debug("Last lcn of mft data attribute is 0x%llx.",
			(unsigned long long)lcn);
	/* Minimum allocation is one mft record worth of clusters. */
	min_nr = vol->mft_record_size >> vol->cluster_size_shift;
	if (!min_nr)
		min_nr = 1;
	/* Want to allocate 16 mft records worth of clusters. */
	nr = (vol->mft_record_size * 16) / vol->cluster_size;
	if (!nr)
		nr = min_nr;
	/*
	 * To be in line with what Windows allows we restrict the total number
	 * of mft records to 2^32.
	 */
	if ((allocated_size + (nr << vol->cluster_size_shift)) >>
			vol->mft_record_size_shift >= (1LL << 32)) {
		nr = min_nr;
		if ((allocated_size + (nr << vol->cluster_size_shift)) >>
				vol->mft_record_size_shift >= (1LL << 32)) {
			ntfs_warning(vol->mp, "Cannot allocate mft record "
					"because the maximum number of inodes "
					"(2^32) has already been reached.");
			lck_rw_unlock_exclusive(&mft_ni->rl.lock);
			return ENOSPC;
		}
	}
	ntfs_debug("Trying mft data allocation with %s cluster count %lld.",
			nr > min_nr ? "default" : "minimal", (long long)nr);
	do {
		runlist.rl = NULL;
		runlist.alloc_count = runlist.elements = 0;
		/*
		 * We have taken the mft lock for writing.  This is not a
		 * problem as ntfs_cluster_alloc() only needs to access pages
		 * from the cluster bitmap (vol->lcnbmp_ni) and we have mapped
		 * the whole runlist for the cluster bitmap at mount time thus
		 * ntfs_page_map() will never need to map an mft record and
		 * hence will never need to take the mft lock.
		 */
		err = ntfs_cluster_alloc(vol, vcn + 1, nr, lcn, MFT_ZONE,
				TRUE, &runlist);
		if (!err)
			break;
		if (err != ENOSPC || nr == min_nr) {
			if (err != ENOMEM && err != ENOSPC)
				err = EIO;
			ntfs_error(vol->mp, "Failed to allocate the minimal "
					"number of clusters (%lld) for the "
					"mft data attribute.", (long long)nr);
			lck_rw_unlock_exclusive(&mft_ni->rl.lock);
			return err;
		}
		/*
		 * There is not enough space to do the allocation, but there
		 * might be enough space to do a minimal allocation so try that
		 * before failing.
		 */
		nr = min_nr;
		ntfs_debug("Retrying mft data allocation with minimal cluster "
				"count %lld.", (long long)nr);
	} while (1);
	/*
	 * Merge the existing runlist with the new one describing the allocated
	 * clusters.
	 */
	err = ntfs_rl_merge(&mft_ni->rl, &runlist);
	if (err) {
		lck_rw_unlock_exclusive(&mft_ni->rl.lock);
		ntfs_error(vol->mp, "Failed to merge runlists for mft data "
				"attribute.");
		if (err != ENOMEM)
			err = EIO;
		err2 = ntfs_cluster_free_from_rl(vol, runlist.rl, 0, -1, NULL);
		if (err2) {
			ntfs_error(vol->mp, "Failed to release allocated "
					"cluster(s) (error %d).%s", err2, es);
			NVolSetErrors(vol);
		}
		IODeleteData(runlist.rl, ntfs_rl_element, runlist.alloc_count);
		return err;
	}
	ntfs_debug("Allocated %lld clusters.", (long long)nr);
	lck_spin_lock(&mft_ni->size_lock);
	mft_ni->allocated_size += nr << vol->cluster_size_shift;
	lck_spin_unlock(&mft_ni->size_lock);
	/*
	 * We now have to drop the runlist lock again or we can deadlock with
	 * the below mapping of the mft record belonging to $MFT.
	 *
	 * Again as explained above the mft cannot change under us so we leave
	 * the runlist unlocked.
	 */
	lck_rw_unlock_exclusive(&mft_ni->rl.lock);
	/*
	 * Update the attribute record as well.
	 *
	 * When mapping the mft record for the mft we communicate the fact that
	 * we hold the lock on the mft inode @mft_ni->lock for writing so it
	 * does not try to take the lock.
	 */
	err = ntfs_mft_record_map_ext(mft_ni, &m, TRUE);
	if (err) {
		ntfs_error(vol->mp, "Failed to map mft record.");
		m = NULL;
		ctx = NULL;
		goto undo_alloc;
	}
	ctx = ntfs_attr_search_ctx_get(mft_ni, m);
	if (!ctx) {
		ntfs_error(vol->mp, "Failed to get search context.");
		err = ENOMEM;
		goto undo_alloc;
	}
	/*
	 * We have the mft lock taken for write.  Communicate this fact to
	 * ntfs_attr_lookup() and hence to ntfs_extent_mft_record_map_ext() and
	 * ntfs_mft_record_map_ext() so that they know not to try to take the
	 * same lock.
	 */
	ctx->is_mft_locked = 1;
	err = ntfs_attr_lookup(mft_ni->type, mft_ni->name, mft_ni->name_len,
			vcn, NULL, 0, ctx);
	if (err) {
		ntfs_error(vol->mp, "Failed to find last attribute extent of "
				"mft data attribute.");
		if (err == ENOENT)
			err = EIO;
		goto undo_alloc;
	}
	m = ctx->m;
	a = ctx->a;
	/* Find the runlist element with which the attribute extent starts. */
	lowest_vcn = sle64_to_cpu(a->lowest_vcn);
	rl = ntfs_rl_find_vcn_nolock(mft_ni->rl.rl, lowest_vcn);
	if (!rl)
		panic("%s(): !rl\n", __FUNCTION__);
	if (!rl->length)
		panic("%s(): !rl->length\n", __FUNCTION__);
	if (rl->lcn < LCN_HOLE)
		panic("%s(): rl->lcn < LCN_HOLE\n", __FUNCTION__);
	/* Get the size for the new mapping pairs array for this extent. */
	err = ntfs_get_size_for_mapping_pairs(vol, rl, lowest_vcn, -1,
			&mp_size);
	if (err) {
		ntfs_error(vol->mp, "Get size for mapping pairs failed for "
				"mft data attribute extent.");
		goto undo_alloc;
	}
	/* Extend the attribute record to fit the bigger mapping pairs array. */
	attr_len = (int)le32_to_cpu(a->length);
	err = ntfs_attr_record_resize(m, a, mp_size +
			le16_to_cpu(a->mapping_pairs_offset));
	if (err) {
		if (err != ENOSPC) {
			ntfs_error(vol->mp, "Failed to resize attribute "
					"record for mft data attribute.");
			goto undo_alloc;
		}
		// TODO: Deal with this by moving this extent to a new mft
		// record or by starting a new extent in a new mft record or by
		// moving other attributes out of this mft record.
		// Note: Use the special reserved mft records and ensure that
		// this extent is not required to find the mft record in
		// question.  If no free special records left we would need to
		// move an existing record away, insert ours in its place, and
		// then place the moved record into the newly allocated space
		// and we would then need to update all references to this mft
		// record appropriately.  This is rather complicated...
		ntfs_error(vol->mp, "Not enough space in this mft record to "
				"accomodate extended mft data attribute "
				"extent.  Cannot handle this yet.");
		err = ENOTSUP;
		goto undo_alloc;
	}
	mp_rebuilt = TRUE;
	/* Generate the mapping pairs array directly into the attr record. */
	err = ntfs_mapping_pairs_build(vol, (s8*)a +
			le16_to_cpu(a->mapping_pairs_offset), mp_size, rl,
			lowest_vcn, -1, NULL);
	if (err) {
		ntfs_error(vol->mp, "Failed to build mapping pairs array of "
				"mft data attribute (error %d).", err);
		err = EIO;
		goto dirty_undo_alloc;
	}
	/* Update the highest_vcn. */
	a->highest_vcn = cpu_to_sle64(vcn + nr);
	/*
	 * We now have extended the mft data allocated_size by @nr clusters.
	 * Reflect this in the ntfs_inode structure and the attribute record.
	 */
	if (a->lowest_vcn) {
		/*
		 * We are not in the first attribute extent, switch to it, but
		 * first ensure the changes will make it to disk later.
		 */
		NInoSetMrecNeedsDirtying(ctx->ni);
		/*
		 * The reinitialization will preserve the is_mft_locked flag in
		 * the search context thus we do not need to set it again.
		 */
		ntfs_attr_search_ctx_reinit(ctx);
		err = ntfs_attr_lookup(mft_ni->type, mft_ni->name,
				mft_ni->name_len, 0, NULL, 0, ctx);
		if (err)
			goto restore_undo_alloc;
		/* @m is not used any more so no need to set it. */
		a = ctx->a;
	}
	a->allocated_size = cpu_to_sle64(mft_ni->allocated_size);
	/* Ensure the changes make it to disk. */
	NInoSetMrecNeedsDirtying(ctx->ni);
	ntfs_attr_search_ctx_put(ctx);
	ntfs_mft_record_unmap(mft_ni);
	/*
	 * We have modified the size of the base inode, cause the sizes to be
	 * written to all the directory index entries pointing to the base
	 * inode when the inode is written to disk.
	 */
	NInoSetDirtySizes(mft_ni);
	ntfs_debug("Done.");
	return 0;
restore_undo_alloc:
	ntfs_error(vol->mp, "Failed to find first attribute extent of mft "
			"data attribute.");
	if (err == ENOENT)
		err = EIO;
	/*
	 * The reinitialization will preserve the is_mft_locked flag in the
	 * search context thus we do not need to set it again.
	 */
	ntfs_attr_search_ctx_reinit(ctx);
	err2 = ntfs_attr_lookup(mft_ni->type, mft_ni->name, mft_ni->name_len,
			vcn, NULL, 0, ctx);
	if (err2) {
		ntfs_error(vol->mp, "Failed to find last attribute extent of "
				"mft data attribute (error %d).%s", err2, es);
		ntfs_attr_search_ctx_put(ctx);
		ntfs_mft_record_unmap(mft_ni);
		/*
		 * The only thing that is now wrong is the allocated size of the
		 * base attribute extent which chkdsk should be able to fix.
		 */
		NVolSetErrors(vol);
		return err;
	}
	ctx->a->highest_vcn = cpu_to_sle64(vcn);
dirty_undo_alloc:
	/*
	 * Need to mark the mft record for dirtying because ntfs_cluster_free()
	 * may drop the mft record on the floor otherwise.
	 */
	NInoSetMrecNeedsDirtying(ctx->ni);
undo_alloc:
	err2 = ntfs_cluster_free(mft_ni, vcn + 1, -1, ctx, NULL);
	if (err2 || ctx->is_error) {
		ntfs_error(vol->mp, "Failed to release allocated cluster(s) "
				"in error code path (error %d).%s",
				ctx->is_error ? ctx->error : err2, es);
		NVolSetErrors(vol);
	}
	/*
	 * If the runlist truncation fails and/or the search context is no
	 * longer valid, we cannot resize the attribute record or build the
	 * mapping pairs array thus we mark the volume dirty and tell the user
	 * to run chkdsk.
	 *
	 * As before, we are going to update the runlist now so we need to take
	 * the runlist lock for writing.
	 */
	lck_rw_lock_exclusive(&mft_ni->rl.lock);
	lck_spin_lock(&mft_ni->size_lock);
	mft_ni->allocated_size -= nr << vol->cluster_size_shift;
	lck_spin_unlock(&mft_ni->size_lock);
	err2 = ntfs_rl_truncate_nolock(vol, &mft_ni->rl, vcn + 1);
	lck_rw_unlock_exclusive(&mft_ni->rl.lock);
	if (err2) {
		ntfs_error(vol->mp, "Failed to truncate attribute runlist s "
				"in error code path (error %d).%s", err2, es);
		NVolSetErrors(vol);
	} else if (mp_rebuilt) {
		a = ctx->a;
		err2 = ntfs_attr_record_resize(ctx->m, a, attr_len);
		if (err2) {
			ntfs_error(vol->mp, "Failed to restore attribute "
					"record in error code path (error "
					"%d).%s", err2, es);
			NVolSetErrors(vol);
		} else /* if (!err2) */ {
			u16 mp_ofs = le16_to_cpu(a->mapping_pairs_offset);
			err2 = ntfs_mapping_pairs_build(vol, (s8*)a + mp_ofs,
					attr_len - mp_ofs, mft_ni->rl.rl,
					lowest_vcn, -1, NULL);
			if (err2) {
				ntfs_error(vol->mp, "Failed to restore "
						"mapping pairs array in error "
						"code path (error %d).%s",
						err2, es);
				NVolSetErrors(vol);
			}
			NInoSetMrecNeedsDirtying(ctx->ni);
		}
	}
	if (ctx)
		ntfs_attr_search_ctx_put(ctx);
	if (m)
		ntfs_mft_record_unmap(mft_ni);
	return err;
}

/**
 * ntfs_mft_record_lay_out - lay out an mft record into a memory buffer
 * @vol:	volume to which the mft record will belong
 * @mft_no:	mft record number of record to lay out
 * @m:		destination buffer of size >= @vol->mft_record_size bytes
 *
 * Lay out an empty, unused mft record with the mft record number @mft_no into
 * the buffer @m.  The volume @vol is needed because the mft record structure
 * was modified in NTFS 3.1 so we need to know which volume version this mft
 * record will be used on and also we need to know the size of an mft record.
 *
 * Return 0 on success and errno on error.
 */
static errno_t ntfs_mft_record_lay_out(const ntfs_volume *vol,
		const s64 mft_no, MFT_RECORD *m)
{
	ATTR_RECORD *a;

	ntfs_debug("Entering for mft record 0x%llx.",
			(unsigned long long)mft_no);
	if (mft_no >= (1LL << 32)) {
		ntfs_error(vol->mp, "Mft record number 0x%llx exceeds "
				"maximum of 2^32.",
				(unsigned long long)mft_no);
		return ERANGE;
	}
	if (vol->mft_record_size < NTFS_BLOCK_SIZE)
		panic("%s(): vol->mft_record_size < NTFS_BLOCK_SIZE\n",
				__FUNCTION__);
	/* Start by clearing the whole mft record to give us a clean slate. */
	bzero(m, vol->mft_record_size);
	/* Aligned to 2-byte boundary. */
	if (vol->major_ver < 3 || (vol->major_ver == 3 && !vol->minor_ver))
		m->usa_ofs = cpu_to_le16((sizeof(MFT_RECORD_OLD) + 1) & ~1);
	else {
		m->usa_ofs = cpu_to_le16((sizeof(MFT_RECORD) + 1) & ~1);
		/*
		 * Set the NTFS 3.1+ specific fields while we know that the
		 * volume version is 3.1+.
		 */
		/* m->reserved = 0; */
		m->mft_record_number = cpu_to_le32((u32)mft_no);
	}
	m->magic = magic_FILE;
	m->usa_count = cpu_to_le16(1 + vol->mft_record_size / NTFS_BLOCK_SIZE);
	/* Set the update sequence number to 1. */
	*(le16*)((u8*)m + le16_to_cpu(m->usa_ofs)) = cpu_to_le16(1);
	/* m->lsn = 0; */
	m->sequence_number = cpu_to_le16(1);
	/* m->link_count = 0; */
	/*
	 * Place the attributes straight after the update sequence array,
	 * aligned to 8-byte boundary.
	 */
	m->attrs_offset = cpu_to_le16((le16_to_cpu(m->usa_ofs) +
			(le16_to_cpu(m->usa_count) << 1) + 7) & ~7);
	/* m->flags = 0; */
	/*
	 * Using attrs_offset plus eight bytes (for the termination attribute).
	 * attrs_offset is already aligned to 8-byte boundary, so no need to
	 * align again.
	 */
	m->bytes_in_use = cpu_to_le32(le16_to_cpu(m->attrs_offset) + 8);
	m->bytes_allocated = cpu_to_le32(vol->mft_record_size);
	/* m->base_mft_record = 0; */
	/* m->next_attr_instance = 0; */
	/* Add the termination attribute. */
	a = (ATTR_RECORD*)((u8*)m + le16_to_cpu(m->attrs_offset));
	a->type = AT_END;
	/* a->length = 0; */
	ntfs_debug("Done.");
	return 0;
}

/**
 * ntfs_mft_record_format - format an mft record on an ntfs volume
 * @vol:			volume on which to format the mft record
 * @mft_no:			mft record number to format
 * @new_initialized_size:	new initialized size to assign to @vol->mft_ni
 *
 * Format the mft record @mft_no in $MFT/$DATA, i.e. lay out an empty, unused
 * mft record into the appropriate place of the mft data attribute.  This is
 * used when extending the mft data attribute.
 *
 * Once the mft record is layed out the initialized size of @vol->mft_ni is
 * updated to @new_initalized_size.  This must be bigger or equal to the old
 * initialized size and smaller or equal to the data size.
 *
 * Return 0 on success and errno on error.
 *
 * Locking: Caller must hold @vol->mft_ni->lock.
 */
static errno_t ntfs_mft_record_format(ntfs_volume *vol, const s64 mft_no,
		const s64 new_initialized_size)
{
	ntfs_inode *mft_ni;
	ino64_t buf_mft_no;
	ino64_t buf_mft_record;
	u32 buf_read_size;
	buf_t buf;
	char *m_sec;
	MFT_RECORD *m;
	errno_t err, err2;

	ntfs_debug("Entering for mft record 0x%llx.",
			(unsigned long long)mft_no);
	mft_ni = vol->mft_ni;
	/* The maximum valid offset into the VM page cache for $MFT's data. */
	if ((mft_no << vol->mft_record_size_shift) + vol->mft_record_size >
			ubc_getsize(mft_ni->vn)) {
		ntfs_error(vol->mp, "Tried to format non-existing mft "
				"record 0x%llx.", (unsigned long long)mft_no);
		return ENOENT;
	}
	/* Read and map the buffer containing the mft record. */
#if NTFS_SUB_SECTOR_MFT_RECORD_SIZE_RW
	if (vol->mft_record_size < vol->sector_size) {
		/* If the MFT record size is smaller than a sector, we must
		 * align the buffer read to the sector size. */
		buf_mft_no = mft_no & ~vol->mft_records_per_sector_mask;
		buf_mft_record = mft_no & vol->mft_records_per_sector_mask;
		buf_read_size = vol->sector_size;
	} else {
#endif
		buf_mft_no = mft_no;
		buf_mft_record = 0;
		buf_read_size = vol->mft_record_size;
#if NTFS_SUB_SECTOR_MFT_RECORD_SIZE_RW
	}
#endif
	err = buf_meta_bread(mft_ni->vn, buf_mft_no, buf_read_size, NOCRED,
			&buf);
	if (err) {
		ntfs_error(vol->mp, "Failed to read buffer of mft record "
				"0x%llx (error %d).",
				(unsigned long long)mft_no, err);
		goto brelse;
	}
	err = buf_map(buf, (caddr_t*)&m_sec);
	if (err) {
		ntfs_error(vol->mp, "Failed to map buffer of mft record "
				"0x%llx (error %d).",
				(unsigned long long)mft_no, err);
		goto brelse;
	}
	m = (MFT_RECORD*)&m_sec[buf_mft_record * vol->mft_record_size];
	err = ntfs_mft_record_lay_out(vol, mft_no, m);
	if (err) {
		ntfs_error(vol->mp, "Failed to lay out mft record 0x%llx "
				"(error %d).", (unsigned long long)mft_no, err);
		goto unmap;
	}
	err = buf_unmap(buf);
	if (err) {
		ntfs_error(vol->mp, "Failed to unmap buffer of mft record "
				"0x%llx (error %d).",
				(unsigned long long)mft_no, err);
		goto brelse;
	}
	lck_spin_lock(&mft_ni->size_lock);
	if (new_initialized_size < mft_ni->initialized_size ||
			new_initialized_size > mft_ni->data_size)
		panic("%s(): new_initialized_size < mft_ni->initialized_size "
				"|| new_initialized_size > mft_ni->data_size\n",
				__FUNCTION__);
	mft_ni->initialized_size = new_initialized_size;
	lck_spin_unlock(&mft_ni->size_lock);
	err = buf_bdwrite(buf);
	if (!err) {
		ntfs_debug("Done.");
		return 0;
	}
	ntfs_error(vol->mp, "Failed to write buffer of mft record 0x%llx "
			"(error %d).  Run chkdsk.", (unsigned long long)mft_no,
			err);
	NVolSetErrors(vol);
	return err;
unmap:
	err2 = buf_unmap(buf);
	if (err2)
		ntfs_error(vol->mp, "Failed to unmap buffer of mft record "
				"0x%llx in error code path (error %d).",
				(unsigned long long)mft_no, err2);
brelse:
	buf_brelse(buf);
	return err;
}

/**
 * ntfs_standard_info_attribute_insert - add the standard information attribute
 * @m:			mft record in which to insert the attribute
 * @a:			attribute in front of which to insert the new attribute
 * @file_attrs:		file attribute flags to set in the attribute
 * @security_id:	security_id to set in the attribute
 * @create_time:	time to use for the times in the attribute
 *
 * Insert the standard information attribute into the mft record @m in front of
 * the attribute record @a.
 *
 * If @security_id is not zero, insert a Win2k+ style standard information
 * attribute and if it is zero, insert an NT4 style one.
 *
 * This function cannot fail.
 */
static void ntfs_standard_info_attribute_insert(MFT_RECORD *m, ATTR_RECORD *a,
		const FILE_ATTR_FLAGS file_attrs, const le32 security_id,
		struct timespec *create_time)
{
	STANDARD_INFORMATION *si;
	u32 size;

	ntfs_debug("Entering.");
	size = sizeof(STANDARD_INFORMATION);
	if (!security_id)
		size = offsetof(STANDARD_INFORMATION, reserved12) +
			sizeof(si->reserved12);
	/*
	 * Insert the attribute and initialize the value to zero.  This cannot
	 * fail as we are only called with an empty mft record so there must be
	 * enough space for the standard information attribute.
	 */
	if (ntfs_resident_attr_record_insert_internal(m, a,
			AT_STANDARD_INFORMATION, NULL, 0, size))
		panic("%s(): Failed to insert standard information "
				"attribute.\n", __FUNCTION__);
	/* Set up the attribute value. */
	si = (STANDARD_INFORMATION*)((u8*)a + le16_to_cpu(a->value_offset));
	si->last_access_time = si->last_mft_change_time =
			si->last_data_change_time = si->creation_time =
			utc2ntfs(*create_time);
	si->file_attributes = file_attrs;
	if (security_id)
		si->security_id = security_id;
	ntfs_debug("Done (used %s style standard information attribute).",
			security_id ? "Win2k+" : "NT4");
}

/**
 * ntfs_sd_attribute_insert - add the security descriptor attribute
 * @vol:	volume to which the mft record belongs
 * @m:		mft record in which to insert the attribute
 * @a:		attribute in front of which to insert the new attribute
 * @va:		vnode attributes
 *
 * Insert the security descriptor attribute into the mft record @m in front of
 * the attribute record @a.
 *
 * @vol is the volume the mft record @m belongs to and is used to determine
 * whether an NT4 security descriptor is needed (NTFS 1.x) or a Win2k+ security
 * descriptor is needed (NTFS 3.0+).
 *
 * @va are the vnode attributes to assign to the create inode and allows us to
 * distinguish whether we need to insert a directory security descriptor or a
 * file one.
 *
 * This function cannot fail.
 */
static void ntfs_sd_attribute_insert(ntfs_volume *vol, MFT_RECORD *m,
		ATTR_RECORD *a, const struct vnode_attr *va)
{
	SDS_ENTRY *sds;
	u32 sd_size;

	ntfs_debug("Entering.");
	if (vol->major_ver > 1) {
		if (va->va_type == VDIR)
			sds = ntfs_dir_sds_entry;
		else
			sds = ntfs_file_sds_entry;
	} else {
		if (va->va_type == VDIR)
			sds = ntfs_dir_sds_entry_old;
		else
			sds = ntfs_file_sds_entry_old;
	}
	sd_size = le32_to_cpu(sds->length) - sizeof(SDS_ENTRY_HEADER);
	/*
	 * Insert the attribute.  This cannot fail as we are only called with
	 * an empty mft record so there must be enough space for our default
	 * security descriptor attribute which is tiny.
	 */
	if (ntfs_resident_attr_record_insert_internal(m, a,
			AT_SECURITY_DESCRIPTOR, NULL, 0, sd_size))
		panic("%s(): Failed to insert security descriptor "
				"attribute.\n", __FUNCTION__);
	/* Copy the chosen security descriptor into place. */
	memcpy((u8*)a + le16_to_cpu(a->value_offset), &sds->sd, sd_size);
	ntfs_debug("Done.");
}

/**
 * ntfs_index_root_attribute_insert - add the empty, $I30 index root attribute
 * @vol:	volume to which the mft record belongs
 * @m:		mft record in which to insert the attribute
 * @a:		attribute in front of which to insert the new attribute
 *
 * Insert the empty, $I30 index root attribute into the mft record @m in front
 * of the attribute record @a.
 *
 * @vol is the volume the mft record @m belongs to and is used to determine the
 * the index block size as well as the number of clusters per index block.
 *
 * This function cannot fail.
 */
static void ntfs_index_root_attribute_insert(ntfs_volume *vol, MFT_RECORD *m,
		ATTR_RECORD *a)
{
	INDEX_ROOT *ir;
	INDEX_ENTRY_HEADER *ieh;

	ntfs_debug("Entering.");
	/*
	 * Insert the attribute and initialize the value to zero.  This cannot
	 * fail as we are only called with an empty mft record so there must be
	 * enough space for the empty index root attribute.
	 */
	if (ntfs_resident_attr_record_insert_internal(m, a, AT_INDEX_ROOT, I30,
			4, sizeof(INDEX_ROOT) + sizeof(INDEX_ENTRY_HEADER)))
		panic("%s(): Failed to insert index root attribute.\n",
				__FUNCTION__);
	/* Set up the attribute value. */
	ir = (INDEX_ROOT*)((u8*)a + le16_to_cpu(a->value_offset));
	ir->type = AT_FILENAME;
	ir->collation_rule = COLLATION_FILENAME;
	ir->index_block_size = cpu_to_le32(vol->index_block_size);
	ir->blocks_per_index_block = vol->blocks_per_index_block;
	ir->index.entries_offset = const_cpu_to_le32(sizeof(INDEX_HEADER));
	ir->index.allocated_size = ir->index.index_length = const_cpu_to_le32(
			sizeof(INDEX_HEADER) + sizeof(INDEX_ENTRY_HEADER));
	/* SMALL_INDEX is zero and the attribute value is already zeroed. */
	/* ir->index.flags = SMALL_INDEX; */
	ieh = (INDEX_ENTRY_HEADER*)((u8*)ir + sizeof(INDEX_ROOT));
	ieh->length = const_cpu_to_le16(sizeof(INDEX_ENTRY_HEADER));
	ieh->flags = INDEX_ENTRY_END;
	ntfs_debug("Done.");
}

/**
 * ntfs_mft_record_alloc - allocate an mft record on an ntfs volume
 * @vol:	[IN]  volume on which to allocate the mft record
 * @va:		[IN/OUT] vnode attributes to assign to the new inode or NULL
 * @cn:		[IN]  name of new inode (@va != NULL) or NULL (@va == NULL)
 * @base_ni:	[IN]  base inode (@va == NULL) or parent directory (@va != NULL)
 * @new_ni:	[OUT] on success this is the ntfs inode of the created inode
 * @new_m:	[OUT] on success this is the mapped mft record
 * @new_a:	[OUT] on success this is the attribute at which to insert
 *
 * Allocate an mft record in $MFT/$DATA of an open ntfs volume @vol and return
 * the ntfs inode of the created inode in *@new_ni, its mft record in *@new_m,
 * and *@new_a poinst to the attribute record in front of which the filename
 * attribute needs be inserted (if @va was not NULL, i.e. we allocated a base
 * mft record for a file or directory) or to the position at which the first
 * attribute in this mft record needs to be inserted (if @va is NULL, i.e. we
 * allocate an extent mft record).
 *
 * If @va is not NULL make the mft record a base mft record, i.e. a file or
 * directory inode, and allocate it at the default allocator position.  In this
 * case @va are the vnode attributes as given to us by the caller, @base_ni is
 * is the ntfs inode of the parent directory, and @cn is the name of the new
 * inode.
 *
 * When allocating a base mft record the caller needs to do an
 *	ntfs_inode_unlock_alloc(*@new_ni);
 * to make the inode a full member of society by unlocking it and waking up any
 * waiters.  We do not do it here as the caller is likely to want to do more
 * work before unlocking the inode.
 *
 * Note that we only support some of the attributes that can be specified in
 * @va and we update @va to reflect the values we actually end up using.
 *
 * We in particular use @va to distinguish what type of inode is being created 
 * (@va->va_type == VREG, VDIR, VLNK, VSOCK, VFIFO, VBLK, or VCHR,
 * respectively).  @va also gives us the creation_time to use
 * (@va->va_create_time) as well as the mode (@va->va_mode) and the file
 * attributes (@va->va_flags).  And for block and character device special file
 * nodes @va->va_rdev specifies the device.
 *
 * If @va is NULL, make the allocated mft record an extent record, allocate it
 * starting at the mft record after the base mft record and attach the
 * allocated and opened ntfs inode to the base inode @base_ni.  @cn is NULL.
 *
 * When allocating a base mft record, add the standard information attribute,
 * the security descriptor attribute (if needed) as well as the empty data
 * attribute (@va->va_type == VREG or VLNK), the empty index root attribute
 * (@va->va_type == VDIR) or the special flags and attributes for special
 * inodes (@va->va_type == VSOCK, VFIFO, VBLK, or VCHR).
 *
 * Return 0 on success and errno on error.  On error *@new_ni, *@new_m, and
 * *@new_a are not defined.
 *
 * Allocation strategy:
 *
 * To find a free mft record, we scan the mft bitmap for a zero bit.  To
 * optimize this we start scanning at the place specified by @base_ni or if
 * @base_ni is NULL we start where we last stopped and we perform wrap around
 * when we reach the end.  Note, we do not try to allocate mft records below
 * number 24 because numbers 0 to 15 are the defined system files anyway and 16
 * to 24 are special in that they are used for storing extension mft records
 * for the $DATA attribute of $MFT.  This is required to avoid the possibility
 * of creating a runlist with a circular dependency which once written to disk
 * can never be read in again.  Windows will only use records 16 to 24 for
 * normal files if the volume is completely out of space.  We never use them
 * which means that when the volume is really out of space we cannot create any
 * more files while Windows can still create up to 8 small files.  We can start
 * doing this at some later time, it does not matter much for now.
 *
 * When scanning the mft bitmap, we only search up to the last allocated mft
 * record.  If there are no free records left in the range 24 to number of
 * allocated mft records, then we extend the $MFT/$DATA attribute in order to
 * create free mft records.  We extend the allocated size of $MFT/$DATA by 16
 * records at a time or one cluster, if cluster size is above 16kiB.  If there
 * is not sufficient space to do this, we try to extend by a single mft record
 * or one cluster, if cluster size is above the mft record size.
 *
 * No matter how many mft records we allocate, we initialize only the first
 * allocated mft record, incrementing mft data size and initialized size
 * accordingly, open an ntfs_inode for it and return it to the caller, unless
 * there are less than 24 mft records, in which case we allocate and initialize
 * mft records until we reach record 24 which we consider as the first free mft
 * record for use by normal files.
 *
 * If during any stage we overflow the initialized data in the mft bitmap, we
 * extend the initialized size (and data size) by 8 bytes, allocating another
 * cluster if required.  The bitmap data size has to be at least equal to the
 * number of mft records in the mft, but it can be bigger, in which case the
 * superflous bits are padded with zeroes.
 *
 * Thus, when we return success (i.e. zero), we will have:
 *	- initialized / extended the mft bitmap if necessary,
 *	- initialized / extended the mft data if necessary,
 *	- set the bit corresponding to the mft record being allocated in the
 *	  mft bitmap,
 *	- opened an ntfs_inode for the allocated mft record, and we will have
 *	- returned the ntfs_inode as well as the allocated and mapped mft
 *	  record.
 *
 * On error, the volume will be left in a consistent state and no record will
 * be allocated.  If rolling back a partial operation fails, we may leave some
 * inconsistent metadata in which case we set NVolErrors() so the volume is
 * left dirty when unmounted.
 *
 * Note, this function cannot make use of most of the normal functions, like
 * for example for attribute resizing, etc, because when the run list overflows
 * the base mft record and an attribute list is used, it is very important that
 * the extension mft records used to store the $DATA attribute of $MFT can be
 * reached without having to read the information contained inside them, as
 * this would make it impossible to find them in the first place after the
 * volume is unmounted.  $MFT/$BITMAP probably does not need to follow this
 * rule because the bitmap is not essential for finding the mft records, but on
 * the other hand, handling the bitmap in this special way would make life
 * easier because otherwise there might be circular invocations of functions
 * when reading the bitmap.
 */
errno_t ntfs_mft_record_alloc(ntfs_volume *vol, struct vnode_attr *va,
		struct componentname *cn, ntfs_inode *base_ni,
		ntfs_inode **new_ni, MFT_RECORD **new_m,
		ATTR_RECORD **new_a)
{
	s64 bit, ll, old_data_initialized, old_data_size, old_mft_data_pos;
	s64 nr_mft_records_added;
	ntfs_inode *mft_ni, *mftbmp_ni, *ni;
	char *m_sec;
	MFT_RECORD *m;
	ntfs_attr_search_ctx *ctx;
	ATTR_RECORD *a;
	ino64_t buf_mft_no;
	ino64_t buf_mft_record;
	u32 buf_read_size;
	buf_t buf;
	errno_t err, err2;
	le16 seq_no, usn;
	BOOL record_formatted, mark_sizes_dirty, dirty_buf;
	BOOL mft_ni_write_locked;

	ntfs_debug("Entering (allocating a%s mft record, %s 0x%llx).",
			va ? " base" : "n extent",
			va ? "parent directory" : "base mft record",
			(unsigned long long)base_ni->mft_no);
	if (!new_ni || !new_m || !new_a)
		panic("%s(): !new_ni || !new_m || !new_a\n", __FUNCTION__);
	if (!base_ni)
		panic("%s(): !base_ni\n", __FUNCTION__);
	lck_rw_lock_exclusive(&vol->mftbmp_lock);
	/*
	 * Get an iocount reference on the mft and mftbmp vnodes.
	 *
	 * We do not bother with the iocount reference on the mft if @va is
	 * NULL, i.e. we are allocating an extent mft record, because in that
	 * case the base mft record @ni is already mapped thus an iocount
	 * reference is already held on the mft.
	 */
	mft_ni = vol->mft_ni;
	if (va) {
		err = vnode_get(mft_ni->vn);
		if (err) {
			ntfs_error(vol->mp, "Failed to get vnode for $MFT.");
			lck_rw_unlock_exclusive(&vol->mftbmp_lock);
			return err;
		}
	}
	mftbmp_ni = vol->mftbmp_ni;
	err = vnode_get(mftbmp_ni->vn);
	if (err) {
		ntfs_error(vol->mp, "Failed to get vnode for $MFT/$Bitmap.");
		if (va)
			(void)vnode_put(mft_ni->vn);
		lck_rw_unlock_exclusive(&vol->mftbmp_lock);
		return err;
	}
retry_mftbmp_alloc:
	record_formatted = mark_sizes_dirty = dirty_buf = FALSE;
	lck_rw_lock_exclusive(&mftbmp_ni->lock);
	err = ntfs_mft_bitmap_find_and_alloc_free_rec_nolock(vol,
			va ? NULL : base_ni, &bit);
	if (!err) {
		ntfs_debug("Found and allocated free record (#1), bit 0x%llx.",
				(unsigned long long)bit);
		goto have_alloc_rec;
	}
	if (err != ENOSPC)
		goto unl_err;
	/*
	 * No free mft records left.  If the mft bitmap already covers more
	 * than the currently used mft records, the next records are all free,
	 * so we can simply allocate the first unused mft record.
	 *
	 * Note: We also have to make sure that the mft bitmap at least covers
	 * the first 24 mft records as they are special and whilst they may not
	 * be in use, we do not allocate from them.
	 */
	lck_spin_lock(&mft_ni->size_lock);
	ll = mft_ni->initialized_size >> vol->mft_record_size_shift;
	lck_spin_unlock(&mft_ni->size_lock);
	lck_spin_lock(&mftbmp_ni->size_lock);
	old_data_initialized = mftbmp_ni->initialized_size;
	lck_spin_unlock(&mftbmp_ni->size_lock);
	if (old_data_initialized << 3 > ll && old_data_initialized > 3) {
		bit = ll;
		if (bit < 24)
			bit = 24;
		/*
		 * To be in line with what Windows allows we restrict the total
		 * number of mft records to 2^32.
		 */
		if (bit >= (1LL << 32))
			goto max_err;
		ntfs_debug("Found free record (#2), bit 0x%llx.",
				(unsigned long long)bit);
		goto found_free_rec;
	}
	/*
	 * The mft bitmap needs to be extended until it covers the first unused
	 * mft record that we can allocate.
	 *
	 * Note: The smallest mft record we allocate is mft record 24.
	 */
	bit = old_data_initialized << 3;
	/*
	 * To be in line with what Windows allows we restrict the total number
	 * of mft records to 2^32.
	 */
	if (bit >= (1LL << 32))
		goto max_err;
	lck_spin_lock(&mftbmp_ni->size_lock);
	old_data_size = mftbmp_ni->allocated_size;
	ntfs_debug("Status of mftbmp before extension: allocated_size 0x%llx, "
			"data_size 0x%llx, initialized_size 0x%llx.",
			(unsigned long long)old_data_size,
			(unsigned long long)mftbmp_ni->data_size,
			(unsigned long long)old_data_initialized);
	lck_spin_unlock(&mftbmp_ni->size_lock);
	if (old_data_initialized + 8 > old_data_size) {
		/* Need to extend bitmap by one more cluster. */
		ntfs_debug("mftbmp: initialized_size + 8 > allocated_size.");
		err = ntfs_mft_bitmap_extend_allocation_nolock(vol);
		if (err)
			goto unl_err;
#ifdef DEBUG
		lck_spin_lock(&mftbmp_ni->size_lock);
		ntfs_debug("Status of mftbmp after allocation extension: "
				"allocated_size 0x%llx, data_size 0x%llx, "
				"initialized_size 0x%llx.",
				(unsigned long long)mftbmp_ni->allocated_size,
				(unsigned long long)mftbmp_ni->data_size,
				(unsigned long long)
				mftbmp_ni->initialized_size);
		lck_spin_unlock(&mftbmp_ni->size_lock);
#endif /* DEBUG */
	}
	/*
	 * We now have sufficient allocated space, extend the initialized_size
	 * as well as the data_size if necessary and fill the new space with
	 * zeroes.
	 */
	err = ntfs_mft_bitmap_extend_initialized_nolock(vol);
	if (err)
		goto unl_err;
#ifdef DEBUG
	lck_spin_lock(&mftbmp_ni->size_lock);
	ntfs_debug("Status of mftbmp after initialized extension: "
			"allocated_size 0x%llx, data_size 0x%llx, "
			"initialized_size 0x%llx.",
			(unsigned long long)mftbmp_ni->allocated_size,
			(unsigned long long)mftbmp_ni->data_size,
			(unsigned long long)mftbmp_ni->initialized_size);
	lck_spin_unlock(&mftbmp_ni->size_lock);
#endif /* DEBUG */
	ntfs_debug("Found free record (#3), bit 0x%llx.",
			(unsigned long long)bit);
found_free_rec:
	/* @bit is the found free mft record, allocate it in the mft bitmap. */
	ntfs_debug("At found_free_rec.");
	err = ntfs_bitmap_set_bit(mftbmp_ni, bit);
	if (err) {
		ntfs_error(vol->mp, "Failed to allocate bit in mft bitmap.");
		goto unl_err;
	}
	ntfs_debug("Set bit 0x%llx in mft bitmap.", (unsigned long long)bit);
have_alloc_rec:
	lck_rw_unlock_exclusive(&mftbmp_ni->lock);
	/*
	 * The mft bitmap is now uptodate.  Deal with mft data attribute now.
	 * Note, we keep hold of the mft bitmap lock for writing until all
	 * modifications to the mft data attribute are complete, too, as they
	 * will impact decisions for mft bitmap and mft record allocation done
	 * by a parallel allocation and if the lock is not maintained a
	 * parallel allocation could decide to allocate the same mft record as
	 * this one.
	 */
	lck_rw_lock_shared(&mft_ni->lock);
	mft_ni_write_locked = FALSE;
mft_relocked:
	ll = (bit + 1) << vol->mft_record_size_shift;
	lck_spin_lock(&mft_ni->size_lock);
	old_data_initialized = mft_ni->initialized_size;
	lck_spin_unlock(&mft_ni->size_lock);
	if (ll <= old_data_initialized) {
		ntfs_debug("Allocated mft record already initialized.");
		goto mft_rec_already_initialized;
	}
	if (!mft_ni_write_locked) {
		mft_ni_write_locked = TRUE;
		if (!lck_rw_lock_shared_to_exclusive(&mft_ni->lock)) {
			lck_rw_lock_exclusive(&mft_ni->lock);
			goto mft_relocked;
		}
	}
	ntfs_debug("Initializing allocated mft record.");
	/*
	 * The mft record is outside the initialized data.  Extend the mft data
	 * attribute until it covers the allocated record.  The loop is only
	 * actually traversed more than once when a freshly formatted volume is
	 * first written to so it optimizes away nicely in the common case.
	 */
	lck_spin_lock(&mft_ni->size_lock);
	ntfs_debug("Status of mft data before extension: "
			"allocated_size 0x%llx, data_size 0x%llx, "
			"initialized_size 0x%llx.",
			(unsigned long long)mft_ni->allocated_size,
			(unsigned long long)mft_ni->data_size,
			(unsigned long long)mft_ni->initialized_size);
	while (ll > mft_ni->allocated_size) {
		lck_spin_unlock(&mft_ni->size_lock);
		err = ntfs_mft_data_extend_allocation_nolock(vol);
		if (err) {
			ntfs_error(vol->mp, "Failed to extend mft data "
					"allocation.");
			lck_rw_unlock_exclusive(&mft_ni->lock);
			goto undo_mftbmp_alloc_locked;
		}
		lck_spin_lock(&mft_ni->size_lock);
		ntfs_debug("Status of mft data after allocation extension: "
				"allocated_size 0x%llx, data_size 0x%llx, "
				"initialized_size 0x%llx.",
				(unsigned long long)mft_ni->allocated_size,
				(unsigned long long)mft_ni->data_size,
				(unsigned long long)mft_ni->initialized_size);
	}
	lck_spin_unlock(&mft_ni->size_lock);
	/*
	 * Extend mft data initialized size (and data size of course) to reach
	 * the allocated mft record, formatting the mft records allong the way.
	 *
	 * Note: We only modify the ntfs_inode structure as that is all that is
	 * needed by ntfs_mft_record_format().  We will update the attribute
	 * record itself in one fell swoop later on.
	 */
	lck_spin_lock(&mft_ni->size_lock);
	old_data_initialized = mft_ni->initialized_size;
	old_data_size = mft_ni->data_size;
	nr_mft_records_added = 0;
	if (old_data_size != ubc_getsize(mft_ni->vn))
		panic("%s(): old_data_size != ubc_getsize(mft_ni->vn)\n",
				__FUNCTION__);
	while (ll > mft_ni->initialized_size) {
		s64 new_initialized_size, mft_no;
		
		new_initialized_size = mft_ni->initialized_size +
				vol->mft_record_size;
		mft_no = mft_ni->initialized_size >> vol->mft_record_size_shift;
		ntfs_debug("mft_no 0x%llx, new_initialized_size 0x%llx, "
				"initialized_size 0x%llx, data_size 0x%llx.",
				(unsigned long long)mft_no,
				(unsigned long long)new_initialized_size,
				(unsigned long long)mft_ni->initialized_size,
				(unsigned long long)mft_ni->data_size);
		if (new_initialized_size > mft_ni->data_size) {
			/* Increment the number of newly added mft records. */
			nr_mft_records_added += (new_initialized_size -
					mft_ni->data_size) >>
					vol->mft_record_size_shift;
			ntfs_debug("Updating data size and ubc size, "
					"nr_mft_records_added %lld.",
					(long long)nr_mft_records_added);
			mft_ni->data_size = new_initialized_size;
			lck_spin_unlock(&mft_ni->size_lock);
			if (!ubc_setsize(mft_ni->vn, new_initialized_size))
				panic("%s(): ubc_setsize() failed.\n",
						__FUNCTION__);
			mark_sizes_dirty = TRUE;
		} else
			lck_spin_unlock(&mft_ni->size_lock);
		ntfs_debug("Initializing mft record 0x%llx.",
				(unsigned long long)mft_no);
		/*
		 * ntfs_mft_record_format() updates the initialized size in
		 * @mft_ni.
		 */
		err = ntfs_mft_record_format(vol, mft_no, new_initialized_size);
		if (err) {
			ntfs_error(vol->mp, "Failed to format mft record.");
			goto undo_data_init;
		}
		lck_spin_lock(&mft_ni->size_lock);
	}
	lck_spin_unlock(&mft_ni->size_lock);
	record_formatted = TRUE;
	/*
	 * Update the mft data attribute record to reflect the new sizes.
	 *
	 * When mapping the mft record for the mft we communicate the fact that
	 * we hold the lock on the mft inode @mft_ni->lock for writing so it
	 * does not try to take the lock.
	 */
	err = ntfs_mft_record_map_ext(mft_ni, &m, TRUE);
	if (err) {
		ntfs_error(vol->mp, "Failed to map mft record.");
		goto undo_data_init;
	}
	ctx = ntfs_attr_search_ctx_get(mft_ni, m);
	if (!ctx) {
		ntfs_error(vol->mp, "Failed to get search context.");
		err = ENOMEM;
		ntfs_mft_record_unmap(mft_ni);
		goto undo_data_init;
	}
	/*
	 * We have the mft lock taken for write.  Communicate this fact to
	 * ntfs_attr_lookup() and hence to ntfs_extent_mft_record_map_ext() and
	 * ntfs_mft_record_map_ext() so that they know not to try to take the
	 * same lock.
	 */
	ctx->is_mft_locked = 1;
	err = ntfs_attr_lookup(mft_ni->type, mft_ni->name, mft_ni->name_len,
			0, NULL, 0, ctx);
	if (err) {
		ntfs_error(vol->mp, "Failed to find first attribute extent of "
				"mft data attribute.");
		ntfs_attr_search_ctx_put(ctx);
		ntfs_mft_record_unmap(mft_ni);
		goto undo_data_init;
	}
	a = ctx->a;
	lck_spin_lock(&mft_ni->size_lock);
	a->initialized_size = cpu_to_sle64(mft_ni->initialized_size);
	a->data_size = cpu_to_sle64(mft_ni->data_size);
	/*
	 * We have created new mft records thus update the cached numbers of
	 * total and free mft records to reflect this.
	 */
	vol->nr_mft_records = mft_ni->data_size >> vol->mft_record_size_shift;
	vol->nr_free_mft_records += nr_mft_records_added;
	if (vol->nr_free_mft_records >= vol->nr_mft_records)
		panic("%s(): vol->nr_free_mft_records > vol->nr_mft_records\n",
				__FUNCTION__);
	lck_spin_unlock(&mft_ni->size_lock);
	/* Ensure the changes make it to disk. */
	NInoSetMrecNeedsDirtying(ctx->ni);
	ntfs_attr_search_ctx_put(ctx);
	ntfs_mft_record_unmap(mft_ni);
	/*
	 * If we have modified the size of the base inode, cause the sizes to
	 * be written to all the directory index entries pointing to the base
	 * inode when the inode is written to disk.
	 */
	if (mark_sizes_dirty)
		NInoSetDirtySizes(mft_ni);
	lck_spin_lock(&mft_ni->size_lock);
	ntfs_debug("Status of mft data after mft record initialization: "
			"allocated_size 0x%llx, data_size 0x%llx, "
			"initialized_size 0x%llx.",
			(unsigned long long)mft_ni->allocated_size,
			(unsigned long long)mft_ni->data_size,
			(unsigned long long)mft_ni->initialized_size);
	if (mft_ni->data_size != ubc_getsize(mft_ni->vn))
		panic("%s(): mft_ni->data_size != ubc_getsize(mft_ni->vn)\n",
				__FUNCTION__);
	if (mft_ni->data_size > mft_ni->allocated_size)
		panic("%s(): mft_ni->data_size > mft_ni->allocated_size\n",
				__FUNCTION__);
	if (mft_ni->initialized_size > mft_ni->data_size)
		panic("%s(): mft_ni->initialized_size > mft_ni->data_size\n",
				__FUNCTION__);
	lck_spin_unlock(&mft_ni->size_lock);
	lck_rw_lock_exclusive_to_shared(&mft_ni->lock);
mft_rec_already_initialized:
	/*
	 * Update the default mft allocation position.  We have to do this now
	 * even if we fail later and deallocate the mft record because we are
	 * about to drop the mftbmp_lock so we cannot touch vol->mft_data_pos
	 * later on.  We save the old value so we can restore it on error.
	 */
	old_mft_data_pos = vol->mft_data_pos;
	vol->mft_data_pos = bit + 1;
	/*
	 * We have allocated an mft record thus decrement the cached number of
	 * free mft records to reflect this.
	 */
	vol->nr_free_mft_records--;
	if (vol->nr_free_mft_records < 0)
		vol->nr_free_mft_records = 0;
	/*
	 * We can finally drop the mft bitmap lock as the mft data attribute
	 * has been fully updated.  The only disparity left is that the
	 * allocated mft record still needs to be marked as in use to match the
	 * set bit in the mft bitmap but this is actually not a problem since
	 * this mft record is not referenced from anywhere yet and the fact
	 * that it is allocated in the mft bitmap means that no-one will try to
	 * allocate it either.
	 */
	lck_rw_unlock_exclusive(&vol->mftbmp_lock);
	/*
	 * We now have allocated and initialized the mft record.
	 *
	 * Read and map the buffer containing the mft record.
	 */
#if NTFS_SUB_SECTOR_MFT_RECORD_SIZE_RW
	if (vol->mft_record_size < vol->sector_size) {
		/* If the MFT record size is smaller than a sector, we must
		 * align the buffer read to the sector size. */
		buf_mft_no = bit & ~vol->mft_records_per_sector_mask;
		buf_mft_record = bit & vol->mft_records_per_sector_mask;
		buf_read_size = vol->sector_size;
	} else {
#endif
		buf_mft_no = bit;
		buf_mft_record = 0;
		buf_read_size = vol->mft_record_size;
#if NTFS_SUB_SECTOR_MFT_RECORD_SIZE_RW
	}
#endif
	err = buf_meta_bread(mft_ni->vn, buf_mft_no, buf_read_size, NOCRED,
			&buf);
	if (err) {
		ntfs_error(vol->mp, "Failed to read buffer of mft record "
				"0x%llx (error %d).", (unsigned long long)bit,
				err);
		goto undo_mftbmp_alloc;
	}
	err = buf_map(buf, (caddr_t*)&m_sec);
	if (err) {
		ntfs_error(vol->mp, "Failed to map buffer of mft record "
				"0x%llx (error %d).", (unsigned long long)bit,
				err);
		goto undo_mftbmp_alloc;
	}
	m = (MFT_RECORD*)&m_sec[buf_mft_record * vol->mft_record_size];
	/* If we just formatted the mft record no need to do it again. */
	if (!record_formatted) {
		/*
		 * Sanity check that the mft record is really not in use.  If
		 * it is in use then warn the user about this inconsistency,
		 * mark the volume as dirty to force chkdsk to run, and try to
		 * allocate another mft record.  As we have already set the mft
		 * bitmap bit this means we have "repaired" the inconsistency.
		 * Of course we may now have an mft record that is marked in
		 * use correctly but that is not referenced from anywhere at
		 * all but chkdsk should hopefully fix this case by either
		 * recovering the mft record by linking it somewhere or by
		 * properly freeing the mft record.
		 *
		 * TODO: Need to test what chkdsk does exactly.  For example if
		 * it only clears the bit in the mft bitmap but leaves the mft
		 * record marked in use we would detect this here as corruption
		 * again and set the bitmap bit back to one and thus end up
		 * with a vicious circle.  So we need to figure out what chkdsk
		 * does and adjust our handling here appropriately.
		 */
		if (ntfs_is_file_record(m->magic) &&
				m->flags & MFT_RECORD_IN_USE) {
			ntfs_warning(vol->mp, "Mft record 0x%llx was marked "
					"free in mft bitmap but is marked "
					"used itself.  Marking it used in mft "
					"bitmap.  This indicates a corrupt "
					"file system.  Unmount and run "
					"chkdsk.", (unsigned long long)bit);
			err = buf_unmap(buf);
			if (err)
				ntfs_error(vol->mp, "Failed to unmap buffer "
						"of mft record 0x%llx (error "
						"%d).",
						(unsigned long long)bit, err);
			buf_brelse(buf);
			lck_rw_unlock_shared(&mft_ni->lock);
			lck_rw_lock_exclusive(&vol->mftbmp_lock);
			NVolSetErrors(vol);
			goto retry_mftbmp_alloc;
		}
		/*
		 * We need to (re-)format the mft record, preserving the
		 * sequence number if it is not zero as well as the update
		 * sequence number if it is not zero or -1 (0xffff).  This
		 * means we do not need to care whether or not something went
		 * wrong with the previous mft record.
		 */
		seq_no = m->sequence_number;
		usn = 0;
		if (le16_to_cpu(m->usa_ofs) < NTFS_BLOCK_SIZE - sizeof(u16))
			usn = *(le16*)((u8*)m + le16_to_cpu(m->usa_ofs));
		err = ntfs_mft_record_lay_out(vol, bit, m);
		if (err) {
			ntfs_error(vol->mp, "Failed to lay out allocated mft "
					"record 0x%llx.",
					(unsigned long long)bit);
			goto unmap_undo_mftbmp_alloc;
		}
		if (seq_no)
			m->sequence_number = seq_no;
		if (usn && usn != 0xffff)
			*(le16*)((u8*)m + le16_to_cpu(m->usa_ofs)) = usn;
	}
	/* Set the mft record itself in use. */
	m->flags |= MFT_RECORD_IN_USE;
	if (!va) {
		/*
		 * Record the sequence number so we can supply it as part of
		 * the mft reference when mapping the extent mft record below
		 * which ensures that we get back the same mft record we
		 * expected.
		 */
		seq_no = m->sequence_number;
		/*
		 * Setup the base mft record in the extent mft record.  This
		 * completes initialization of the allocated extent mft record
		 * and we can simply use it with ntfs_extent_mft_record_map().
		 */
		m->base_mft_record = MK_LE_MREF(base_ni->mft_no,
				base_ni->seq_no);
		/*
		 * Need to release the page so that we can call
		 * ntfs_extent_mft_record_map().  We also set the page dirty to
		 * ensure that it does not get thrown out under VM pressure
		 * before we get it with the ntfs_extent_mft_record_map() call.
		 *
		 * FIXME: This could be optimized by modifying
		 * ntfs_extent_mft_record_map() to take an optional mft record,
		 * i.e. @m, and if supplied using this instead of trying to map
		 * the extent mft record.   Alternatively we could unlock the
		 * page but not release it but this cannot be done in OS X
		 * (yet).
		 *
		 * Allocate an extent inode structure for the new mft record,
		 * attach it to the base inode @base_ni and map its, i.e. the
		 * allocated, mft record.
		 */
		err = buf_unmap(buf);
		if (err)
			ntfs_error(vol->mp, "Failed to unmap buffer of mft "
					"record 0x%llx (error %d).",
					(unsigned long long)bit, err);
		err = buf_bdwrite(buf);
		if (err) {
			ntfs_error(vol->mp, "Failed to write buffer of mft "
					"record 0x%llx (error %d).  Run "
					"chkdsk.", (unsigned long long)bit,
					err);
			NVolSetErrors(vol);
			lck_rw_unlock_shared(&mft_ni->lock);
			goto free_undo_mftbmp_alloc;
		}
		err = ntfs_extent_mft_record_map_ext(base_ni, MK_MREF(bit,
				le16_to_cpu(seq_no)), &ni, &m, TRUE);
		lck_rw_unlock_shared(&mft_ni->lock);
		if (err) {
			ntfs_error(vol->mp, "Failed to map allocated mft "
					"record 0x%llx (error %d).",
					(unsigned long long)bit, err);
			goto free_undo_mftbmp_alloc;
		}
		/* This is where the first attribute needs to be inserted. */
		*new_a = (ATTR_RECORD*)((u8*)m + le16_to_cpu(m->attrs_offset));
	} else {
		FILE_ATTR_FLAGS file_attrs;
		le32 security_id;
		ntfs_attr na;

		/*
		 * Mirror the file attribute flags we want to inherit from the
		 * parent directory.
		 */
		file_attrs = base_ni->file_attributes & (FILE_ATTR_ENCRYPTED |
				FILE_ATTR_NOT_CONTENT_INDEXED |
				FILE_ATTR_COMPRESSED | FILE_ATTR_SPARSE_FILE);
		switch (va->va_type) {
		case VDIR:
			m->flags |= MFT_RECORD_IS_DIRECTORY;
			break;
		case VSOCK:
		case VFIFO:
		case VBLK:
		case VCHR:
			/*
			 * We use the same way of implementing special inodes
			 * as Services For Unix uses on Windows thus we set the
			 * FILE_ATTR_SYSTEM file attribute.
			 */
			file_attrs |= FILE_ATTR_SYSTEM;
			/*
			 * It makes no sense for a special inode to be
			 * encrypted or compressed so clear those flags.
			 */
			file_attrs &= ~(FILE_ATTR_ENCRYPTED |
					FILE_ATTR_COMPRESSED);
		default:
			file_attrs |= FILE_ATTR_ARCHIVE;
			/*
			 * FIXME: We do not implement writing to compressed or
			 * encrypted files yet, so we clear the corresponding
			 * bits in the file attribute flags for now.
			 */
			file_attrs &= ~(FILE_ATTR_ENCRYPTED |
					FILE_ATTR_COMPRESSED);
		}
		/*
		 * Determine whether we need to insert a Win2k+ style standard
		 * information attribute or an NT4 style one.  For NTFS 1.x
		 * volumes, we always insert NT4 style standard information
		 * attributes whilst for newer volumes we decide depending on
		 * the value of NVolUseSDAttr().  If NVolUseSDAttr() is set, we
		 * are to specify security descriptors by creating security
		 * descriptor attributes and in this case we have to use the
		 * NT4 style standard information attribute.  If it is clear,
		 * we are to specify security descriptors by security_id
		 * reference into $Secure system file and in this case we have
		 * to use the Win2k+ style standard information attribute.
		 *
		 * To make things simpler, if this is an NTFS 1.x volume,
		 * NVolUseSDAttr() has been set so we only need to test for it.
		 */
		if (NVolUseSDAttr(vol))
			security_id = 0;
		else {
			BOOL is_retry = FALSE;
retry:
			lck_spin_lock(&vol->security_id_lock);
			if (va->va_type == VDIR)
				security_id = vol->default_dir_security_id;
			else
				security_id = vol->default_file_security_id;
			lck_spin_unlock(&vol->security_id_lock);
			/*
			 * If the default security_id is not initialized, try
			 * to initialize it now and should the initialization
			 * fail, use a security descriptor attribute and hence
			 * an NT4 style standard information attribute.
			 */
			if (!security_id && !is_retry) {
				if (!ntfs_default_security_id_init(vol, va)) {
					is_retry = TRUE;
					goto retry;
				}
			}
		}
		a = (ATTR_RECORD*)((u8*)m + le16_to_cpu(m->attrs_offset));
		/* Add the standard information attribute. */
		ntfs_standard_info_attribute_insert(m, a, file_attrs,
				security_id, &va->va_create_time);
		a = (ATTR_RECORD*)((u8*)a + le32_to_cpu(a->length));
		/*
		 * If @security_id is zero, add the security descriptor
		 * attribute.  If it is not zero, we have already set the
		 * security_id in the standard information attribute to
		 * reference our security descriptor in $Secure.
		 */
		if (!security_id) {
			/* Add the security descriptor attribute. */
			ntfs_sd_attribute_insert(vol, m, a, va);
			a = (ATTR_RECORD*)((u8*)a + le32_to_cpu(a->length));
		}
		if (va->va_type == VDIR) {
			/* Add the empty, $I30 index root attribute. */
			ntfs_index_root_attribute_insert(vol, m, a);
		} else {
			INTX_FILE *ix;
			u32 data_len;

			/*
			 * FIXME: For encrypted files, we need to add an empty,
			 * non-resident $DATA attribute and we need to add the
			 * $EFS attribute.  For now, we should never get here
			 * as we clear the encrypted bit above because we do
			 * not support creating encrypted files.
			 */
			if (file_attrs & FILE_ATTR_ENCRYPTED)
				panic("%s(): file_attrs & "
						"FILE_ATTR_ENCRYPTED\n",
						__FUNCTION__);
			switch (va->va_type) {
			case VBLK:
			case VCHR:
				/*
				 * In Services for Unix on Windows, a device
				 * special file is a system file whose $DATA
				 * attribute contains the INTX_FILE structure.
				 */
				data_len = offsetof(INTX_FILE, device) +
						sizeof(ix->device);
				break;
			case VSOCK:
				/*
				 * In Services for Unix on Windows, a socket is
				 * a system file with a $DATA attribute of
				 * length 1.
				 */
				data_len = 1;
				break;
			case VFIFO:
				/*
				 * On Services for Unix on Windows, a fifo is a
				 * system file with a zero-length $DATA
				 * attribute so fall through to the default
				 * case.
				 */
			default:
				data_len = 0;
				break;
			}
			/*
			 * Insert the empty, resident $DATA attribute.  This
			 * cannot fail as we are dealing with an empty mft
			 * record so there must be enough space for an empty
			 * $DATA attribute.
			 */
			if (ntfs_resident_attr_record_insert_internal(m, a,
					AT_DATA, NULL, 0, data_len))
				panic("%s(): Failed to insert resident data "
						"attribute.\n", __FUNCTION__);
			/*
			 * If this is a device special inode then set up the
			 * INTX_FILE structure inside the created $DATA
			 * attribute.
			 */
			if (va->va_type == VBLK || va->va_type == VCHR) {
				ix = (INTX_FILE*)((u8*)a +
						le16_to_cpu(a->value_offset));
				if (va->va_type == VBLK)
					ix->magic = INTX_BLOCK_DEVICE;
				else
					ix->magic = INTX_CHAR_DEVICE;
				ix->device.major = cpu_to_le64(
						major(va->va_rdev));
				ix->device.minor = cpu_to_le64(
						minor(va->va_rdev));
			}
		}
		/* Allocate a new ntfs inode and set it up. */
		na = (ntfs_attr) {
			.mft_no = bit,
			.type = AT_UNUSED,
			.raw = FALSE,
		};
		ni = ntfs_inode_hash_get(vol, &na);
		if (!ni) {
			ntfs_error(vol->mp, "Failed to allocate ntfs inode "
					"(ENOMEM).");
			err = ENOMEM;
			/* Set the mft record itself not in use. */
			m->flags &= ~MFT_RECORD_IN_USE;
			dirty_buf = TRUE;
			goto unmap_undo_mftbmp_alloc;
		}
		/*
		 * This inode cannot still be in the inode cache as we would
		 * have removed it when it was deleted last time.
		 */
		if (!NInoAlloc(ni))
			panic("%s(): !NInoAlloc(ni)\n", __FUNCTION__);
		ni->seq_no = le16_to_cpu(m->sequence_number);
		/*
		 * Set the appropriate mode, attribute type, and name.  For
		 * directories, also set up the index values to the defaults.
		 */
		ni->mode |= ACCESSPERMS;
		if (va->va_type == VDIR) {
			ni->mode |= S_IFDIR;
			ni->mode &= ~vol->dmask;
			NInoSetMstProtected(ni);
			ni->type = AT_INDEX_ALLOCATION;
			ni->name = I30;
			ni->name_len = 4;
			ni->vcn_size = 0;
			ni->collation_rule = 0;
			ni->vcn_size_shift = 0;
		} else /* if (va->va_type == VREG || va->va_type == VLNK) */ {
			switch (va->va_type) {
			case VREG:
				ni->mode |= S_IFREG;
				break;
			case VLNK:
				ni->mode |= S_IFLNK;
				break;
			case VSOCK:
				ni->mode |= S_IFSOCK;
				break;
			case VFIFO:
				ni->mode |= S_IFIFO;
				break;
			case VBLK:
				ni->mode |= S_IFBLK;
				ni->rdev = va->va_rdev;
				break;
			case VCHR:
				ni->mode |= S_IFCHR;
				ni->rdev = va->va_rdev;
				break;
			default:
				panic("%s(): Should never have gotten here "
						"for va->va_type 0x%x.\n",
						__FUNCTION__, va->va_type);
			}
			if (!S_ISLNK(ni->mode))
				ni->mode &= ~vol->fmask;
			ni->type = AT_DATA;
			/* ni->name = NULL; */
			/* ni->name_len = 0; */
			if (file_attrs & FILE_ATTR_COMPRESSED) {
				// TODO: Set up all the @ni->compress* fields...
				// For now it does not matter as we do not
				// allow creation of compressed files.
				panic("%s(): file_attrs & "
						"FILE_ATTR_COMPRESSED\n",
						__FUNCTION__);
			}
		}
		ni->file_attributes = file_attrs;
		if (file_attrs & FILE_ATTR_COMPRESSED)
			NInoSetCompressed(ni);
		if (file_attrs & FILE_ATTR_ENCRYPTED)
			NInoSetEncrypted(ni);
		if (file_attrs & FILE_ATTR_SPARSE_FILE)
			NInoSetSparse(ni);
		ni->last_access_time = ni->last_mft_change_time =
				ni->last_data_change_time = ni->creation_time =
				va->va_create_time;
		/* Initialize the backup time and Finder info cache. */
		ntfs_inode_afpinfo_cache(ni, NULL, 0);
		/*
		 * If it is a symbolic link set the Finder info type and
		 * creator appropriately and mark it dirty.  We will create the
		 * AFP_AfpInfo attribute later when the inode is ready for it.
		 */
		if (va->va_type == VLNK) {
			ni->finder_info.type = FINDER_TYPE_SYMBOLIC_LINK;
			ni->finder_info.creator = FINDER_CREATOR_SYMBOLIC_LINK;
			NInoSetDirtyFinderInfo(ni);
		}
		/* Tell the caller what mode and flags we actually used. */
		va->va_mode = ni->mode;
		va->va_flags = 0;
		if (file_attrs & FILE_ATTR_READONLY)
			va->va_flags |= UF_IMMUTABLE;
		if (file_attrs & FILE_ATTR_HIDDEN)
			va->va_flags |= UF_HIDDEN;
		if (!(file_attrs & FILE_ATTR_ARCHIVE))
			va->va_flags |= SF_ARCHIVED;
		/* The ntfs inode is now fully setup so we now add the vnode. */
		err = ntfs_inode_add_vnode(ni, FALSE, base_ni->vn, cn);
		if (err) {
			/* Destroy the allocated ntfs inode. */
			ntfs_inode_reclaim(ni);
			/* Set the mft record itself not in use. */
			m->flags &= ~MFT_RECORD_IN_USE;
			dirty_buf = TRUE;
			goto unmap_undo_mftbmp_alloc;
		}
		/*
		 * Need to release the buffer so that we can call
		 * ntfs_mft_record_map().
		 *
		 * FIXME: This could be optimized by modifying
		 * ntfs_mft_record_map() to take an optional mft record, i.e.
		 * @m, and if supplied using this instead of trying to map the
		 * extent mft record.
		 */
		err = buf_unmap(buf);
		if (err)
			ntfs_error(vol->mp, "Failed to unmap buffer of mft "
					"record 0x%llx (error %d).",
					(unsigned long long)bit, err);
		err = buf_bdwrite(buf);
		if (err) {
			ntfs_error(vol->mp, "Failed to write buffer of mft "
					"record 0x%llx (error %d).  Run "
					"chkdsk.", (unsigned long long)bit,
					err);
			NVolSetErrors(vol);
			lck_rw_unlock_shared(&mft_ni->lock);
			ntfs_inode_unlock_alloc(ni);
			(void)vnode_recycle(ni->vn);
			(void)vnode_put(ni->vn);
			goto free_undo_mftbmp_alloc;
		}
		err = ntfs_mft_record_map_ext(ni, &m, TRUE);
		lck_rw_unlock_shared(&mft_ni->lock);
		if (err) {
			ntfs_inode_unlock_alloc(ni);
			(void)vnode_recycle(ni->vn);
			(void)vnode_put(ni->vn);
			goto free_undo_mftbmp_alloc;
		}
		a = (ATTR_RECORD*)((u8*)m + le16_to_cpu(m->attrs_offset));
		if (a->type != AT_STANDARD_INFORMATION)
			panic("%s(): a->type != AT_STANDARD_INFORMATION\n",
					__FUNCTION__);
		a = (ATTR_RECORD*)((u8*)a + le32_to_cpu(a->length));
		if (le32_to_cpu(a->type) <= const_le32_to_cpu(AT_FILENAME))
			panic("%s(): a->type <= AT_FILENAME\n", __FUNCTION__);
		/* This is where the filename attribute needs to be inserted. */
		*new_a = a;
	}
	/* Make sure the (extent) inode is written out to disk. */
	NInoSetMrecNeedsDirtying(ni);
	/*
	 * Drop the taken iocount references on the mft and mftbmp vnodes.
	 *
	 * Note we still retain an iocount reference on the mft vnode due to
	 * the above call to ntfs_{,extent_}mft_record_map().
	 */
	(void)vnode_put(mftbmp_ni->vn);
	if (va)
		(void)vnode_put(mft_ni->vn);
	/*
	 * Return the opened, allocated inode of the allocated mft record as
	 * well as the mapped mft record.
	 */
	ntfs_debug("Returning allocated %sntfs inode (mft_no 0x%llx).",
			va ? "" : "extent ", (unsigned long long)bit);
	*new_ni = ni;
	*new_m = m;
	return err;
undo_data_init:
	lck_spin_lock(&mft_ni->size_lock);
	mft_ni->initialized_size = old_data_initialized;
	lck_spin_unlock(&mft_ni->size_lock);
	if (!ubc_setsize(mft_ni->vn, old_data_size))
		panic("%s(): !ubc_setsize(mft_ni->vn, old_data_size)\n",
				__FUNCTION__);
	lck_spin_lock(&mft_ni->size_lock);
	mft_ni->data_size = old_data_size;
	lck_spin_unlock(&mft_ni->size_lock);
	lck_rw_unlock_exclusive(&mft_ni->lock);
	goto undo_mftbmp_alloc_locked;
free_undo_mftbmp_alloc:
	lck_rw_lock_shared(&mft_ni->lock);
	err2 = buf_meta_bread(mft_ni->vn, buf_mft_no, buf_read_size, NOCRED,
			&buf);
	if (err2) {
		ntfs_error(vol->mp, "Failed to re-read buffer of mft record "
				"0x%llx in error code path (error %d).%s",
				(unsigned long long)bit, err2, es);
		NVolSetErrors(vol);
		goto undo_mftbmp_alloc;
	}
	err2 = buf_map(buf, (caddr_t*)&m_sec);
	if (err2) {
		ntfs_error(vol->mp, "Failed to re-map buffer of mft record "
				"0x%llx in error code path (error %d).%s",
				(unsigned long long)bit, err2, es);
		NVolSetErrors(vol);
		goto undo_mftbmp_alloc;
	}
	/* Set the mft record itself not in use. */
	m = (MFT_RECORD*)&m_sec[buf_mft_record * vol->mft_record_size];
	m->flags &= ~MFT_RECORD_IN_USE;
	dirty_buf = TRUE;
unmap_undo_mftbmp_alloc:
	err2 = buf_unmap(buf);
	if (err2)
		ntfs_error(vol->mp, "Failed to unmap buffer of mft record "
				"0x%llx (error %d).", (unsigned long long)bit,
				err2);
undo_mftbmp_alloc:
	if (dirty_buf) {
		err2 = buf_bdwrite(buf);
		if (err2)
			ntfs_error(vol->mp, "Failed to write buffer of mft "
					"record 0x%llx in error code path "
					"(error %d).", (unsigned long long)bit,
					err2);
	} else
		buf_brelse(buf);
	lck_rw_unlock_shared(&mft_ni->lock);
	lck_rw_lock_exclusive(&vol->mftbmp_lock);
	/*
	 * We decremented the cached number of free mft records thus we need to
	 * increment it again here now that we are not allocating the mft
	 * record after all.
	 */
	vol->nr_free_mft_records++;
	/*
	 * Restore the previous mft data position but only if no-one else has
	 * restored it to something even older whilst we had dropped the lock.
	 */
	if (old_mft_data_pos < vol->mft_data_pos)
		vol->mft_data_pos = old_mft_data_pos;
undo_mftbmp_alloc_locked:
	lck_rw_lock_shared(&mftbmp_ni->lock);
	if (ntfs_bitmap_clear_bit(mftbmp_ni, bit)) {
		ntfs_error(vol->mp, "Failed to clear bit in mft bitmap.%s", es);
		NVolSetErrors(vol);
		/*
		 * We failed to clear the bit thus we are wasting an mft record
		 * and since its bit is set in the mft bitmap it is effectively
		 * in use thus it is not free.  So decrement the number of free
		 * mft records again.
		 */
		vol->nr_free_mft_records--;
		if (vol->nr_free_mft_records < 0)
			vol->nr_free_mft_records = 0;
	}
	lck_rw_unlock_shared(&mftbmp_ni->lock);
err:
	lck_rw_unlock_exclusive(&vol->mftbmp_lock);
	(void)vnode_put(mftbmp_ni->vn);
	if (va)
		(void)vnode_put(mft_ni->vn);
	return err;
max_err:
	ntfs_warning(vol->mp, "Cannot allocate mft record because the maximum "
			"number of inodes (2^32) has already been reached.");
	err = ENOSPC;
unl_err:
	lck_rw_unlock_exclusive(&mftbmp_ni->lock);
	goto err;
}

/**
 * ntfs_extent_mft_record_free - free an extent mft record on an ntfs volume
 * @base_ni:	base ntfs inode to which the extent inode to be freed belongs
 * @ni:		ntfs inode of the mapped extent mft record to free
 * @m:		mapped extent mft record of the ntfs inode @ni
 *
 * Free the mapped extent mft record @m of the extent ntfs inode @ni belonging
 * to the base ntfs inode @base_ni.
 *
 * Note that this function unmaps the mft record and closes and destroys @ni
 * internally and hence you cannot use either the inode nor its mft record any
 * more after this function returns success.
 *
 * Return 0 on success and errno on error.  In the error case @ni and @m are
 * still valid and have not been freed.
 *
 * For some errors an error message is displayed and the success code 0 is
 * returned and the volume is then left dirty on umount.  This makes sense in
 * case we could not rollback the changes that were already done since the
 * caller no longer wants to reference this mft record so it does not matter to
 * the caller if something is wrong with it as long as it is properly detached
 * from the base inode.
 */
errno_t ntfs_extent_mft_record_free(ntfs_inode *base_ni, ntfs_inode *ni,
		MFT_RECORD *m)
{
	ino64_t mft_no = ni->mft_no;
	ntfs_volume *vol = ni->vol;
	ntfs_inode **extent_nis;
	int i;
	errno_t err;
	u16 seq_no;
	
	ntfs_debug("Entering for extent mft_no 0x%llx, base mft_no 0x%llx.\n",
			(unsigned long long)mft_no,
			(unsigned long long)base_ni->mft_no);
	if (NInoAttr(ni))
		panic("%s(): NInoAttr(ni)\n", __FUNCTION__);
	if (ni->nr_extents != -1)
		panic("%s(): ni->nr_extents != -1\n", __FUNCTION__);
	if (base_ni->nr_extents <= 0)
		panic("%s(): base_ni->nr_extents <= 0\n", __FUNCTION__);
	lck_mtx_lock(&base_ni->extent_lock);
	/* Dissociate the ntfs inode from the base inode. */
	extent_nis = base_ni->extent_nis;
	err = ENOENT;
	for (i = 0; i < base_ni->nr_extents; i++) {
		if (ni != extent_nis[i])
			continue;
		extent_nis += i;
		base_ni->nr_extents--;
		if (base_ni->nr_extents > 0) {
			/*
			 * We do not bother reallocating memory for the array
			 * to shrink it as in the worst case we are wasting a
			 * bit of memory until the inode is thrown out of the
			 * cache or until all extent mft records are removed in
			 * which case we will free the whole array below.
			 */
			memmove(extent_nis, extent_nis + 1,
					(base_ni->nr_extents - i) *
					sizeof(ntfs_inode*));
		} else {
			if (base_ni->nr_extents < 0)
				panic("%s(): base_ni->nr_extents < 0\n",
						__FUNCTION__);
            IODelete(base_ni->extent_nis, ntfs_inode*, base_ni->extent_alloc / sizeof(ntfs_inode*));
			base_ni->extent_alloc = 0;
		}
		err = 0;
		break;
	}
	lck_mtx_unlock(&base_ni->extent_lock);
	if (err)
		panic("%s(): Extent mft_no 0x%llx is not attached to "
				"its base mft_no 0x%llx.\n", __FUNCTION__,
				(unsigned long long)mft_no,
				(unsigned long long)base_ni->mft_no);
	/*
	 * The extent inode is no longer attached to the base inode so we can
	 * proceed to free it as no one can get a reference to it now because
	 * we still hold the base mft record mapped.
	 *
	 * Begin by setting the mft record itself not in use and then increment
	 * the sequence number, skipping zero, if it is not zero.
	 */
	m->flags &= ~MFT_RECORD_IN_USE;
	seq_no = le16_to_cpu(m->sequence_number);
	if (seq_no == 0xffff)
		seq_no = 1;
	else if (seq_no)
		seq_no++;
	m->sequence_number = cpu_to_le16(seq_no);
	/* Make sure the mft record is written out to disk. */
	NInoSetMrecNeedsDirtying(ni);
	/*
	 * Unmap and throw away the now freed extent inode.  The mft record
	 * will be written out later by the VM due to its page being marked
	 * dirty.
	 */
	ntfs_extent_mft_record_unmap(ni);
	ntfs_inode_reclaim(ni);
	/*
	 * Clear the bit in the $MFT/$BITMAP corresponding to this record thus
	 * making it available for someone else to allocate it.
	 */
	lck_rw_lock_exclusive(&vol->mftbmp_lock);
	err = vnode_get(vol->mftbmp_ni->vn);
	if (err)
		ntfs_error(vol->mp, "Failed to get vnode for $MFT/$BITMAP.");
	else {
		lck_rw_lock_shared(&vol->mftbmp_ni->lock);
		err = ntfs_bitmap_clear_bit(vol->mftbmp_ni, mft_no);
		lck_rw_unlock_shared(&vol->mftbmp_ni->lock);
		(void)vnode_put(vol->mftbmp_ni->vn);
		if (!err) {
			/*
			 * We cleared a bit in the mft bitmap thus we need to
			 * reflect this in the cached number of free mft
			 * records.
			 */
			vol->nr_free_mft_records++;
			if (vol->nr_free_mft_records >= vol->nr_mft_records)
				panic("%s(): vol->nr_free_mft_records > "
						"vol->nr_mft_records\n",
						__FUNCTION__);
		}
	}
	lck_rw_unlock_exclusive(&vol->mftbmp_lock);
	if (err) {
		/*
		 * The extent inode is gone but we failed to deallocate it in
		 * the mft bitmap.  Just emit a warning and leave the volume
		 * dirty on umount.
		 */
		ntfs_error(vol->mp, "Failed to mark extent mft record as "
				"unused in mft bitmap.%s", es);
		NVolSetErrors(vol);
	}
	return 0;
}
