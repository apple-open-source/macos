/*
 * ntfs_page.c - NTFS kernel page operations.
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

#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/ucred.h>
#ifdef KERNEL
#include <sys/ubc.h>
#endif
#include <sys/vnode.h>

#ifdef KERNEL
#include <kern/debug.h>
#include <kern/locks.h>
#else
#include "ntfs_xpl.h"
#endif

#include "ntfs_attr.h"
#include "ntfs_compress.h"
#include "ntfs_debug.h"
#include "ntfs_inode.h"
#include "ntfs_layout.h"
#include "ntfs_page.h"
#include "ntfs_types.h"
#include "ntfs_volume.h"

/**
 * ntfs_pagein - read a range of pages into memory
 * @ni:		ntfs inode whose data to read into the page range
 * @attr_ofs:	byte offset in the inode at which to start
 * @size:	number of bytes to read from the inode
 * @upl:	page list describing destination page range
 * @upl_ofs:	byte offset into page list at which to start
 * @flags:	flags further describing the pagein request
 *
 * Read @size bytes from the ntfs inode @ni, starting at byte offset @attr_ofs
 * into the inode, into the range of pages specified by the page list @upl,
 * starting at byte offset @upl_ofs into the page list.
 *
 * The @flags further describe the pagein request.  The following pagein flags
 * are currently defined in OSX kernel:
 *	UPL_IOSYNC	- Perform synchronous i/o.
 *	UPL_NOCOMMIT	- Do not commit/abort the page range.
 *	UPL_NORDAHEAD	- Do not perform any speculative read-ahead.
 *	IO_PASSIVE	- This is background i/o so do not throttle other i/o.
 *
 * Inside the ntfs driver we have the need to perform pageins whilst the inode
 * is locked for writing (@ni->lock) thus we cheat and set UPL_NESTED_PAGEOUT
 * in @flags when this is the case.  We make sure to clear it in @flags before
 * calling into the cluster layer so we do not accidentally cause confusion.
 *
 * For encrypted attributes we abort for now as we do not support them yet.
 *
 * For non-resident, non-compressed attributes we use cluster_pagein_ext()
 * which deals with both normal and multi sector transfer protected attributes.
 *
 * For resident attributes and non-resident, compressed attributes we read the
 * data ourselves by mapping the page list, and in the resident case, mapping
 * the mft record, looking up the attribute in it, and copying the requested
 * data from the mapped attribute into the page list, then unmapping the mft
 * record, whilst for non-resident, compressed attributes, we get the raw inode
 * and use it with ntfs_read_compressed() to read and decompress the data into
 * our mapped page list.  We then unmap the page list and finally, if
 * UPL_NOCOMMIT is not specified, we commit (success) or abort (error) the page
 * range.
 *
 * Return 0 on success and errno on error.
 *
 * Note the pages in the page list are marked busy on entry and the busy bit is
 * cleared when we commit the page range.  Thus it is perfectly safe for us to
 * fill the pages with encrypted or mst protected data and to decrypt or mst
 * deprotect in place before committing the page range.
 *
 * Adapted from cluster_pagein_ext().
 *
 * Locking: - Caller must hold an iocount reference on the vnode of @ni.
 *	    - Caller must not hold @ni->lock or if it is held it must be for
 *	      reading unless UPL_NESTED_PAGEOUT is set in @flags in which case
 *	      the caller must hold @ni->lock for reading or writing.
 */
int ntfs_pagein(ntfs_inode *ni, s64 attr_ofs, unsigned size, upl_t upl,
		upl_offset_t upl_ofs, int flags)
{
	s64 attr_size;
	u8 *kaddr;
	kern_return_t kerr;
	unsigned to_read;
	int err;
	BOOL locked = FALSE;

	ntfs_debug("Entering for mft_no 0x%llx, offset 0x%llx, size 0x%x, "
			"pagein flags 0x%x, page list offset 0x%llx.",
			(unsigned long long)ni->mft_no,
			(unsigned long long)attr_ofs, size, flags,
			(unsigned long long)upl_ofs);
	/*
	 * If the caller did not specify any i/o, then we are done.  We cannot
	 * issue an abort because we do not have a upl or we do not know its
	 * size.
	 */
	if (!upl) {
		ntfs_error(ni->vol->mp, "NULL page list passed in (error "
				"EINVAL).");
		return EINVAL;
	}
	if (S_ISDIR(ni->mode)) {
		ntfs_error(ni->vol->mp, "Called for directory vnode.");
		err = EISDIR;
		goto err;
	}
	/*
	 * Protect against changes in initialized_size and thus against
	 * truncation also unless UPL_NESTED_PAGEOUT is set in which case the
	 * caller has already taken @ni->lock for exclusive access.  We simply
	 * leave @locked to be FALSE in this case so we do not try to drop the
	 * lock later on.
	 *
	 * If UPL_NESTED_PAGEOUT is set we clear it in @flags to ensure we do
	 * not cause confusion in the cluster layer or the VM.
	 */
	if (flags & UPL_NESTED_PAGEOUT)
		flags &= ~UPL_NESTED_PAGEOUT;
	else {
		locked = TRUE;
		lck_rw_lock_shared(&ni->lock);
	}
	/* Do not allow messing with the inode once it has been deleted. */
	if (NInoDeleted(ni)) {
		/* Remove the inode from the name cache. */
		cache_purge(ni->vn);
		err = ENOENT;
		goto err;
	}
retry_pagein:
	/*
	 * We guarantee that the size in the ubc will be smaller or equal to
	 * the size in the ntfs inode thus no need to check @ni->data_size.
	 */
	attr_size = ubc_getsize(ni->vn);
	/*
	 * Only $DATA attributes can be encrypted/compressed.  Index root can
	 * have the flags set but this means to create compressed/encrypted
	 * files, not that the attribute is compressed/encrypted.  Note we need
	 * to check for AT_INDEX_ALLOCATION since this is the type of directory
	 * index inodes.
	 */
	if (ni->type != AT_INDEX_ALLOCATION) {
		/* TODO: Deny access to encrypted attributes, just like NT4. */
		if (NInoEncrypted(ni)) {
			if (ni->type != AT_DATA)
				panic("%s(): Encrypted non-data attribute.\n",
						__FUNCTION__);
			ntfs_warning(ni->vol->mp, "Denying access to "
					"encrypted attribute (EACCES).");
			err = EACCES;
			goto err;
		}
		/* Compressed data streams need special handling. */
		if (NInoNonResident(ni) && NInoCompressed(ni) && !NInoRaw(ni)) {
			if (ni->type != AT_DATA)
				panic("%s(): Compressed non-data attribute.\n",
						__FUNCTION__);
			goto compressed;
		}
	}
	/* NInoNonResident() == NInoIndexAllocPresent() */
	if (NInoNonResident(ni)) {
		int (*callback)(buf_t, void *);

		callback = NULL;
		if (NInoMstProtected(ni) || NInoEncrypted(ni))
			callback = ntfs_cluster_iodone;
		/* Non-resident, possibly mst protected, attribute. */
		err = cluster_pagein_ext(ni->vn, upl, upl_ofs, attr_ofs, size,
				attr_size, flags, callback, NULL);
		if (!err)
			ntfs_debug("Done (cluster_pagein_ext()).");
		else
			ntfs_error(ni->vol->mp, "Failed (cluster_pagein_ext(), "
					"error %d).", err);
		if (locked)
			lck_rw_unlock_shared(&ni->lock);
		return err;
	}
compressed:
	/*
	 * The attribute is resident and/or compressed.
	 *
	 * Cannot pagein from a negative offset or if we are starting beyond
	 * the end of the attribute or if the attribute offset is not page
	 * aligned or the size requested is not a multiple of PAGE_SIZE.
	 */
	if (attr_ofs < 0 || attr_ofs >= attr_size || attr_ofs & PAGE_MASK_64 ||
			size & PAGE_MASK || upl_ofs & PAGE_MASK) {
		err = EINVAL;
		goto err;
	}
	to_read = size;
	attr_size -= attr_ofs;
	if (to_read > attr_size)
		to_read = (unsigned)attr_size;
	/*
	 * We do not need @attr_size any more so reuse it to hold the number of
	 * bytes available in the attribute starting at offset @attr_ofs up to
	 * a maximum of the requested number of bytes rounded up to a multiple
	 * of the system page size.
	 */
	attr_size = (to_read + PAGE_MASK) & ~PAGE_MASK;
	/* Abort any pages outside the end of the attribute. */
	if (size > attr_size && !(flags & UPL_NOCOMMIT)) {
		ubc_upl_abort_range(upl, upl_ofs + (unsigned)attr_size,
				size - (unsigned)attr_size,
				UPL_ABORT_FREE_ON_EMPTY | UPL_ABORT_ERROR);
		/* Update @size. */
		size = (unsigned)attr_size;
	}
	/* To access the page list contents, we need to map the page list. */
	kerr = ubc_upl_map(upl, (vm_offset_t*)&kaddr);
	if (kerr != KERN_SUCCESS) {
		ntfs_error(ni->vol->mp, "ubc_upl_map() failed (error %d).",
				(int)kerr);
		err = EIO;
		goto err;
	}
	if (!NInoNonResident(ni)) {
		/*
		 * Read the data from the resident attribute into the page
		 * list.
		 */
		err = ntfs_resident_attr_read(ni, attr_ofs, size,
				kaddr + upl_ofs);
		if (err && err != EAGAIN)
			ntfs_error(ni->vol->mp, "ntfs_resident_attr_read() "
					"failed (error %d).", err);
	} else {
		ntfs_inode *raw_ni;
		int ioflags;

		/*
		 * Get the raw inode.  We take the inode lock shared to protect
		 * against concurrent writers as the compressed data is invalid
		 * whilst a write is in progress.
		 */
		err = ntfs_raw_inode_get(ni, LCK_RW_TYPE_SHARED, &raw_ni);
		if (err)
			ntfs_error(ni->vol->mp, "Failed to get raw inode "
					"(error %d).", err);
		else {
			if (!NInoRaw(raw_ni))
				panic("%s(): Requested raw inode but got "
						"non-raw one.\n", __FUNCTION__);
			ioflags = 0;
			if (vnode_isnocache(ni->vn) ||
					vnode_isnocache(raw_ni->vn))
				ioflags |= IO_NOCACHE;
			if (vnode_isnoreadahead(ni->vn) ||
					vnode_isnoreadahead(raw_ni->vn))
				ioflags |= IO_RAOFF;
			err = ntfs_read_compressed(ni, raw_ni, attr_ofs, size,
					kaddr + upl_ofs, NULL, ioflags);
			if (err)
				ntfs_error(ni->vol->mp,
						"ntfs_read_compressed() "
						"failed (error %d).", err);
			lck_rw_unlock_shared(&raw_ni->lock);
			(void)vnode_put(raw_ni->vn);
		}
	}
	kerr = ubc_upl_unmap(upl);
	if (kerr != KERN_SUCCESS) {
		ntfs_error(ni->vol->mp, "ubc_upl_unmap() failed (error %d).",
				(int)kerr);
		if (!err)
			err = EIO;
	}
	if (!err) {
		if (!(flags & UPL_NOCOMMIT)) {
			/* Commit the page range we brought up to date. */
			ubc_upl_commit_range(upl, upl_ofs, size,
					UPL_COMMIT_FREE_ON_EMPTY);
		}
		ntfs_debug("Done (%s).", !NInoNonResident(ni) ?
				"ntfs_resident_attr_read()" :
				"ntfs_read_compressed()");
	} else /* if (err) */ {
		/*
		 * If the attribute was converted to non-resident under our
		 * nose, retry the pagein.
		 *
		 * TODO: This may no longer be possible to happen now that we
		 * lock against changes in initialized size and thus
		 * truncation...  Revisit this issue when the write code has
		 * been written and remove the check + goto if appropriate.
		 */
		if (err == EAGAIN)
			goto retry_pagein;
err:
		if (!(flags & UPL_NOCOMMIT)) {
			int upl_flags = UPL_ABORT_FREE_ON_EMPTY;
			if (err != ENOMEM)
				upl_flags |= UPL_ABORT_ERROR;
			ubc_upl_abort_range(upl, upl_ofs, size, upl_flags);
		}
		ntfs_error(ni->vol->mp, "Failed (error %d).", err);
	}
	if (locked)
		lck_rw_unlock_shared(&ni->lock);
	return err;
}

/**
 * ntfs_page_map_ext - map a page of a vnode into memory
 * @ni:		ntfs inode of which to map a page
 * @ofs:	byte offset into @ni of which to map a page
 * @upl:	destination page list for the page
 * @pl:		destination array of pages containing the page itself
 * @kaddr:	destination pointer for the address of the mapped page contents
 * @uptodate:	if true return an uptodate page and if false return it as is
 * @rw:		if true we intend to modify the page and if false we do not
 *
 * Map the page corresponding to byte offset @ofs into the ntfs inode @ni into
 * memory and return the page list in @upl, the array of pages containing the
 * page in @pl and the address of the mapped page contents in @kaddr.
 *
 * If @uptodate is true the page is returned uptodate, i.e. if the page is
 * currently not valid, it will be brought uptodate via a call to ntfs_pagein()
 * before it is returned.  And if @uptodate is false, the page is just returned
 * ignoring its state.  This means the page may or may not be uptodate.
 *
 * The caller must set @rw to true if the page is going to be modified and to
 * false otherwise.
 *
 * Note: @ofs must be page aligned.
 *
 * Locking: - Caller must hold an iocount reference on the vnode of @ni.
 *	    - Caller must hold @ni->lock for reading or writing.
 *
 * Return 0 on success and errno on error in which case *@upl is set to NULL.
 */
errno_t ntfs_page_map_ext(ntfs_inode *ni, s64 ofs, upl_t *upl,
		upl_page_info_array_t *pl, u8 **kaddr, const BOOL uptodate,
		const BOOL rw)
{
	s64 size;
	kern_return_t kerr;
	int abort_flags;
	errno_t err;

	ntfs_debug("Entering for inode 0x%llx, offset 0x%llx, rw is %s.",
			(unsigned long long)ni->mft_no,
			(unsigned long long)ofs,
			rw ? "true" : "false");
	if (ofs & PAGE_MASK)
		panic("%s() called with non page aligned offset (0x%llx).",
				__FUNCTION__, (unsigned long long)ofs);
	lck_spin_lock(&ni->size_lock);
	size = ubc_getsize(ni->vn);
	if (size > ni->data_size)
		size = ni->data_size;
	lck_spin_unlock(&ni->size_lock);

	// It seems that regular files can have zero size
	bool isZeroRegFile = (ofs == 0) && (size == 0) && (vnode_isreg(ni->vn));

	if ((ofs >= size) && (!isZeroRegFile)) {
		ntfs_error(ni->vol->mp, "Offset 0x%llx is outside the end of "
				"the attribute (0x%llx).",
				(unsigned long long)ofs,
				(unsigned long long)size);
		err = EINVAL;
		goto err;
	}
	/* Create a page list for the wanted page. */
	kerr = ubc_create_upl(ni->vn, ofs, PAGE_SIZE, upl, pl, UPL_SET_LITE |
			(rw ? UPL_WILL_MODIFY : 0));
	if (kerr != KERN_SUCCESS)
		panic("%s(): Failed to get page (error %d).\n", __FUNCTION__,
				(int)kerr);
	/*
	 * If the page is not valid, need to read it in from the vnode now thus
	 * making it valid.
	 *
	 * We set UPL_NESTED_PAGEOUT to let ntfs_pagein() know that we already
	 * have the inode locked (@ni->lock is held by the caller).
	 */
	if (uptodate && !upl_valid_page(*pl, 0)) {
		ntfs_debug("Reading page as it was not valid.");
		err = ntfs_pagein(ni, ofs, PAGE_SIZE, *upl, 0, UPL_IOSYNC |
				UPL_NOCOMMIT | UPL_NESTED_PAGEOUT);
		if (err) {
			ntfs_error(ni->vol->mp, "Failed to read page (error "
					"%d).", err);
			goto pagein_err;
		}
	}
	/* Map the page into the kernel's address space. */
	kerr = ubc_upl_map(*upl, (vm_offset_t*)kaddr);
	if (kerr == KERN_SUCCESS) {
		ntfs_debug("Done.");
		return 0;
	}
	ntfs_error(ni->vol->mp, "Failed to map page (error %d).",
			(int)kerr);
	err = EIO;
pagein_err:
	abort_flags = UPL_ABORT_FREE_ON_EMPTY;
	if (!upl_valid_page(*pl, 0) ||
			(vnode_isnocache(ni->vn) && !upl_dirty_page(*pl, 0)))
		abort_flags |= UPL_ABORT_DUMP_PAGES;
	ubc_upl_abort_range(*upl, 0, PAGE_SIZE, abort_flags);
err:
	*upl = NULL;
	return err;
}

/**
 * ntfs_page_unmap - unmap a page belonging to a vnode from memory
 * @ni:		ntfs inode to which the page belongs
 * @upl:	page list of the page
 * @pl:		array of pages containing the page itself
 * @mark_dirty:	mark the page dirty
 *
 * Unmap the page belonging to the ntfs inode @ni from memory releasing it back
 * to the vm.
 *
 * The page is described by the page list @upl, the array of pages containing
 * the page @pl and the address of the mapped page contents @kaddr.
 *
 * If @mark_dirty is TRUE, tell the vm to mark the page dirty when releasing
 * the page.
 *
 * Locking: Caller must hold an iocount reference on the vnode of @ni.
 */
void ntfs_page_unmap(ntfs_inode *ni, upl_t upl, upl_page_info_array_t pl,
		const BOOL mark_dirty)
{
	kern_return_t kerr;
	BOOL was_valid, was_dirty;

	was_valid = upl_valid_page(pl, 0);
	/* The page dirty bit is only valid if the page was valid. */
	was_dirty = (was_valid && upl_dirty_page(pl, 0));
	ntfs_debug("Entering for inode 0x%llx, page was %svalid %s %sdirty%s.",
			(unsigned long long)ni->mft_no,
			was_valid ? "" : "not ",
			(int)was_valid ^ (int)was_dirty ? "but" : "and",
			was_dirty ? "" : "not ",
			mark_dirty ? ", marking it dirty" : "");
	/* Unmap the page from the kernel's address space. */
	kerr = ubc_upl_unmap(upl);
	if (kerr != KERN_SUCCESS)
		ntfs_warning(ni->vol->mp, "ubc_upl_unmap() failed (error %d).",
				(int)kerr);
	/*
	 * If the page was valid and dirty or is being made dirty or if caching
	 * for the vnode is enabled (as it will usually be the case for all
	 * metadata files), commit it thus releasing it into the vm taking care
	 * to preserve the dirty state and marking the page dirty if requested
	 * when committing the page.
	 *
	 * If the page was not valid or was valid but not dirty, it is not
	 * being marked dirty, and caching is disabled on the vnode, dump the
	 * page.
	 */
	if (was_dirty || mark_dirty || !vnode_isnocache(ni->vn)) {
		int commit_flags;

		commit_flags = UPL_COMMIT_FREE_ON_EMPTY |
				UPL_COMMIT_INACTIVATE;
		if (!was_valid && !mark_dirty)
			commit_flags |= UPL_COMMIT_CLEAR_DIRTY;
		else if (was_dirty || mark_dirty)
			commit_flags |= UPL_COMMIT_SET_DIRTY;
		ubc_upl_commit_range(upl, 0, PAGE_SIZE, commit_flags);
		ntfs_debug("Done (committed page).");
	} else {
		ubc_upl_abort_range(upl, 0, PAGE_SIZE, UPL_ABORT_DUMP_PAGES |
				UPL_ABORT_FREE_ON_EMPTY);
		ntfs_debug("Done (dumped page).");
	}
}

/**
 * ntfs_page_dump - discard a page belonging to a vnode from memory
 * @ni:		ntfs inode to which the page belongs
 * @upl:	page list of the page
 * @pl:		array of pages containing the page itself
 *
 * Unmap the page belonging to the ntfs inode @ni from memory throwing it away.
 * Note that if the page is dirty all changes to the page will be lost as it
 * will be discarded so use this function with extreme caution.
 *
 * The page is described by the page list @upl, the array of pages containing
 * the page @pl and the address of the mapped page contents @kaddr.
 *
 * Locking: Caller must hold an iocount reference on the vnode of @ni.
 */
void ntfs_page_dump(ntfs_inode *ni, upl_t upl,
		upl_page_info_array_t pl __unused)
{
	kern_return_t kerr;

	ntfs_debug("Entering for inode 0x%llx, page is %svalid, %sdirty.",
			(unsigned long long)ni->mft_no,
			upl_valid_page(pl, 0) ? "" : "not ",
			upl_dirty_page(pl, 0) ? "" : "not ");
	/* Unmap the page from the kernel's address space. */
	kerr = ubc_upl_unmap(upl);
	if (kerr != KERN_SUCCESS)
		ntfs_warning(ni->vol->mp, "ubc_upl_unmap() failed (error %d).",
				(int)kerr);
	/* Dump the page. */
	ubc_upl_abort_range(upl, 0, PAGE_SIZE, UPL_ABORT_DUMP_PAGES |
			UPL_ABORT_FREE_ON_EMPTY);
	ntfs_debug("Done.");
}
