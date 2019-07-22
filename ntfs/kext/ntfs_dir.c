/*
 * ntfs_dir.c - NTFS kernel directory operations.
 *
 * Copyright (c) 2006-2010 Anton Altaparmakov.  All Rights Reserved.
 * Portions Copyright (c) 2006-2010 Apple Inc.  All Rights Reserved.
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
#include <sys/param.h>
#include <sys/dirent.h>
#include <sys/errno.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/ucred.h>
#include <sys/uio.h>
#include <sys/vnode.h>

#include <string.h>

#include <libkern/OSAtomic.h>
#include <libkern/OSMalloc.h>

#include <kern/debug.h>
#include <kern/locks.h>

#include "ntfs.h"
#include "ntfs_attr.h"
#include "ntfs_debug.h"
#include "ntfs_dir.h"
#include "ntfs_endian.h"
#include "ntfs_index.h"
#include "ntfs_inode.h"
#include "ntfs_layout.h"
#include "ntfs_mft.h"
#include "ntfs_page.h"
#include "ntfs_time.h"
#include "ntfs_types.h"
#include "ntfs_unistr.h"
#include "ntfs_volume.h"

/**
 * The little endian Unicode string $I30 as a global constant.
 */
ntfschar I30[5] = { const_cpu_to_le16('$'), const_cpu_to_le16('I'),
		const_cpu_to_le16('3'), const_cpu_to_le16('0'), 0 };

/**
 * ntfs_lookup_inode_by_name - find an inode in a directory given its name
 * @dir_ni:	ntfs inode of the directory in which to search for the name
 * @uname:	Unicode name for which to search in the directory
 * @uname_len:	length of the name @uname in Unicode characters
 * @res_mref:	return the mft reference of the inode of the found name
 * @res_name:	return the found filename if necessary (see below)
 *
 * Look for an inode with name @uname of length @uname_len Unicode characters
 * in the directory with inode @dir_ni.  This is done by walking the contents
 * of the directory B+tree looking for the Unicode name.
 *
 * If the name is found in the directory, 0 is returned and the corresponding
 * inode number (>= 0) is returned as a mft reference in cpu format, i.e. it is
 * a 64-bit number containing the sequence number, in *@res_mref.
 *
 * On error, the error code is returned.  In particular if the inode is not
 * found ENOENT is returned which is not an error as such.
 *
 * Note, @uname_len does not include the (optional) terminating NUL character.
 *
 * Note, we look for a case sensitive match first but we also look for a case
 * insensitive match at the same time.  If we find a case insensitive match, we
 * save that for the case that we do not find an exact match, where we return
 * the case insensitive match and setup *@res_name (which we allocate) with the
 * mft reference, the filename type, length and with a copy of the little
 * endian Unicode filename itself.  If we match a filename which is in the DOS
 * namespace, we only return the mft reference and filename type in *@res_name.
 * ntfs_vnop_lookup() then uses this to find the long filename in the inode
 * itself.  This is so it can use the name cache effectively.
 *
 * Locking: Caller must hold @dir_ni->lock.
 *
 * TODO: From Mark's review comments: pull the iteration code into a separate
 * function and call it both for the index root and index allocation iteration.
 * See the ntfs_index_lookup() function in ntfs_index.c...
 */
errno_t ntfs_lookup_inode_by_name(ntfs_inode *dir_ni, const ntfschar *uname,
		const signed uname_len, MFT_REF *res_mref,
		ntfs_dir_lookup_name **res_name)
{
	VCN vcn, old_vcn;
	ntfs_volume *vol = dir_ni->vol;
	mount_t mp = vol->mp;
	ntfs_inode *ia_ni;
	vnode_t ia_vn = NULL;
	MFT_RECORD *m;
	INDEX_ROOT *ir;
	INDEX_ENTRY *ie;
	ntfs_dir_lookup_name *name = NULL;
	upl_t upl;
	upl_page_info_array_t pl;
	u8 *kaddr;
	INDEX_ALLOCATION *ia;
	u8 *index_end;
	ntfs_attr_search_ctx *ctx;
	int rc;
	errno_t err;

	if (!S_ISDIR(dir_ni->mode))
		panic("%s(): !S_ISDIR(dir_ni->mode\n", __FUNCTION__);
	if (NInoAttr(dir_ni))
		panic("%s(): NInoAttr(dir_ni)\n", __FUNCTION__);
	/* Get the index allocation inode. */
	err = ntfs_index_inode_get(dir_ni, I30, 4, FALSE, &ia_ni);
	if (err) {
		ntfs_error(mp, "Failed to get index vnode (error %d).", err);
		return err;
	}
	ia_vn = ia_ni->vn;
	lck_rw_lock_shared(&ia_ni->lock);
	/* Get hold of the mft record for the directory. */
	err = ntfs_mft_record_map(dir_ni, &m);
	if (err) {
		ntfs_error(mp, "Failed to map mft record for directory (error "
				"%d).", err);
		goto err;
	}
	ctx = ntfs_attr_search_ctx_get(dir_ni, m);
	if (!ctx) {
		ntfs_error(mp, "Failed to get attribute search context.");
		err = ENOMEM;
		goto unm_err;
	}
	/* Find the index root attribute in the mft record. */
	err = ntfs_attr_lookup(AT_INDEX_ROOT, I30, 4, 0, NULL, 0, ctx);
	if (err) {
		if (err == ENOENT) {
			ntfs_error(mp, "Index root attribute missing in "
					"directory inode 0x%llx.",
					(unsigned long long)dir_ni->mft_no);
			err = EIO;
		}
		goto put_err;
	}
	/*
	 * Get to the index root value (it has been verified in
	 * ntfs_index_inode_read()).
	 */
	ir = (INDEX_ROOT*)((u8*)ctx->a + le16_to_cpu(ctx->a->value_offset));
	index_end = (u8*)&ir->index + le32_to_cpu(ir->index.index_length);
	/* The first index entry. */
	ie = (INDEX_ENTRY*)((u8*)&ir->index +
			le32_to_cpu(ir->index.entries_offset));
	/*
	 * Loop until we exceed valid memory (corruption case) or until we
	 * reach the last entry.
	 */
	for (;; ie = (INDEX_ENTRY*)((u8*)ie + le16_to_cpu(ie->length))) {
		ntfs_debug("In index root, offset 0x%x.",
				(unsigned)((u8*)ie - (u8*)ir));
		/* Bounds checks. */
		if ((u8*)ie < (u8*)&ir->index || (u8*)ie +
				sizeof(INDEX_ENTRY_HEADER) > index_end ||
				(u8*)ie + le16_to_cpu(ie->length) >
				index_end)
			goto dir_err;
		/*
		 * The last entry cannot contain a name.  It can however
		 * contain a pointer to a child node in the B+tree so we just
		 * break out.
		 */
		if (ie->flags & INDEX_ENTRY_END)
			break;
		/*
		 * We perform a case sensitive comparison and if that matches
		 * we are done and return the mft reference of the inode (i.e.
		 * the inode number together with the sequence number for
		 * consistency checking).  We convert it to cpu format before
		 * returning.
		 */
		if (ntfs_are_names_equal(uname, uname_len,
				(ntfschar*)&ie->key.filename.filename,
				ie->key.filename.filename_length, TRUE,
				vol->upcase, vol->upcase_len)) {
found_it:
			/*
			 * We have a perfect match, so we do not need to care
			 * about having matched imperfectly before, so we can
			 * free name and set *res_name to NULL.
			 *
			 * However, if the perfect match is a short filename,
			 * we need to signal this through *res_name, so that
			 * the caller can deal with the name cache effectively.
			 *
			 * As an optimization we just reuse an existing
			 * allocation of *res_name.
			 */
			if (ie->key.filename.filename_type == FILENAME_DOS) {
				u8 len;

				if (!name) {
					*res_name = name = OSMalloc(
							sizeof(*name),
							ntfs_malloc_tag);
					if (!name) {
						err = ENOMEM;
						goto put_err;
					}
				}
				name->mref = le64_to_cpu(ie->indexed_file);
				name->type = FILENAME_DOS;
				name->len = len = ie->key.filename.
						filename_length;
				memcpy(name->name, ie->key.filename.filename,
						len * sizeof(ntfschar));
			} else {
				if (name)
					OSFree(name, sizeof(*name),
							ntfs_malloc_tag);
				*res_name = NULL;
			}
			*res_mref = le64_to_cpu(ie->indexed_file);
			ntfs_attr_search_ctx_put(ctx);
			ntfs_mft_record_unmap(dir_ni);
			lck_rw_unlock_shared(&ia_ni->lock);
			(void)vnode_put(ia_vn);
			return 0;
		}
		/*
		 * For a case insensitive mount, we also perform a case
		 * insensitive comparison.  If the comparison matches, we cache
		 * the filename in *res_name so that the caller can work on it.
		 */
		if (!NVolCaseSensitive(vol) &&
				ntfs_are_names_equal(uname, uname_len,
				(ntfschar*)&ie->key.filename.filename,
				ie->key.filename.filename_length, FALSE,
				vol->upcase, vol->upcase_len)) {
			u8 type;

			/*
			 * If no name is cached yet, cache it or if the current
			 * name is the WIN32 name, replace the already cached
			 * name with the WIN32 name.  Otherwise continue
			 * caching the first match.
			 */
			type = ie->key.filename.filename_type;
			if (!name || type == FILENAME_WIN32 || type ==
					FILENAME_WIN32_AND_DOS) {
				u8 len;

				if (!name) {
					*res_name = name = OSMalloc(
							sizeof(*name),
							ntfs_malloc_tag);
					if (!name) {
						err = ENOMEM;
						goto put_err;
					}
				}
				name->mref = le64_to_cpu(ie->indexed_file);
				name->type = type;
				name->len = len = ie->key.filename.
						filename_length;
				memcpy(name->name, ie->key.filename.filename,
						len * sizeof(ntfschar));
			}
		}
		/*
		 * Not a perfect match, need to do full blown collation so we
		 * know which way in the B+tree we have to go.
		 */
		rc = ntfs_collate_names(uname, uname_len,
				(ntfschar*)&ie->key.filename.filename,
				ie->key.filename.filename_length, 1, FALSE,
				vol->upcase, vol->upcase_len);
		/*
		 * If uname collates before the name of the current entry,
		 * there is definitely no such name in this index but we might
		 * need to descend into the B+tree so we just break out of the
		 * loop.
		 */
		if (rc == -1)
			break;
		/* The names are not equal, continue the search. */
		if (rc)
			continue;
		/*
		 * Names match with case insensitive comparison, now try the
		 * case sensitive comparison, which is required for proper
		 * collation.
		 */
		rc = ntfs_collate_names(uname, uname_len,
				(ntfschar*)&ie->key.filename.filename,
				ie->key.filename.filename_length, 1, TRUE,
				vol->upcase, vol->upcase_len);
		if (rc == -1)
			break;
		if (rc)
			continue;
		/*
		 * Perfect match, this will never happen as the
		 * ntfs_are_names_equal() call will have gotten a match but we
		 * still treat it correctly.
		 */
		goto found_it;
	}
	/*
	 * We have finished with this index without success.  Check for the
	 * presence of a child node and if not present return ENOENT, unless we
	 * have got a matching name cached in @name in which case return the
	 * mft reference associated with it.
	 */
	if (!(ie->flags & INDEX_ENTRY_NODE)) {
		ntfs_attr_search_ctx_put(ctx);
		ntfs_mft_record_unmap(dir_ni);
		goto not_found;
	} /* Child node present, descend into it. */
	/* Consistency check: Verify that an index allocation exists. */
	if (!NInoIndexAllocPresent(ia_ni)) {
		ntfs_error(mp, "No index allocation attribute but index entry "
				"requires one.  Directory inode 0x%llx is "
				"corrupt or driver bug.",
				(unsigned long long)dir_ni->mft_no);
		NVolSetErrors(vol);
		goto put_err;
	}
	/* Get the starting vcn of the index block holding the child node. */
	vcn = sle64_to_cpup((sle64*)((u8*)ie + le16_to_cpu(ie->length) - 8));
	/*
	 * We are done with the index root and the mft record.  Release them,
	 * otherwise we deadlock with ntfs_page_map().
	 */
	ntfs_attr_search_ctx_put(ctx);
	ntfs_mft_record_unmap(dir_ni);
	m = NULL;
	ctx = NULL;
descend_into_child_node:
	/*
	 * Convert vcn to byte offset in the index allocation attribute and map
	 * the corresponding page.
	 */
	err = ntfs_page_map(ia_ni, (vcn << ia_ni->vcn_size_shift) &
			~PAGE_MASK_64, &upl, &pl, &kaddr, FALSE);
	if (err) {
		ntfs_error(mp, "Failed to map directory index page (error "
				"%d).", err);
		goto err;
	}
fast_descend_into_child_node:
	/* Get to the index allocation block. */
	ia = (INDEX_ALLOCATION*)(kaddr + ((vcn << ia_ni->vcn_size_shift) &
			PAGE_MASK));
	/* Bounds checks. */
	if ((u8*)ia < kaddr || (u8*)ia > kaddr + PAGE_SIZE) {
		ntfs_error(mp, "Out of bounds check failed.  Corrupt "
				"directory inode 0x%llx or driver bug.",
				(unsigned long long)dir_ni->mft_no);
		goto page_err;
	}
	/* Catch multi sector transfer fixup errors. */
	if (!ntfs_is_indx_record(ia->magic)) {
		ntfs_error(mp, "Directory index record with VCN 0x%llx is "
				"corrupt.  Corrupt inode 0x%llx.  Run chkdsk.",
				(unsigned long long)vcn,
				(unsigned long long)dir_ni->mft_no);
		goto page_err;
	}
	if (sle64_to_cpu(ia->index_block_vcn) != vcn) {
		ntfs_error(mp, "Actual VCN (0x%llx) of index buffer is "
				"different from expected VCN (0x%llx).  "
				"Directory inode 0x%llx is corrupt or driver "
				"bug.", (unsigned long long)
				sle64_to_cpu(ia->index_block_vcn),
				(unsigned long long)vcn,
				(unsigned long long)dir_ni->mft_no);
		goto page_err;
	}
	if (offsetof(INDEX_BLOCK, index) +
			le32_to_cpu(ia->index.allocated_size) !=
			ia_ni->block_size) {
		ntfs_error(mp, "Index buffer (VCN 0x%llx) of directory inode "
				"0x%llx has a size (%u) differing from the "
				"directory specified size (%u).  Directory "
				"inode is corrupt or driver bug.",
				(unsigned long long)vcn,
				(unsigned long long)dir_ni->mft_no, (unsigned)
				(offsetof(INDEX_BLOCK, index) +
				le32_to_cpu(ia->index.allocated_size)),
				(unsigned)ia_ni->block_size);
		goto page_err;
	}
	index_end = (u8*)ia + ia_ni->block_size;
	if (index_end > kaddr + PAGE_SIZE) {
		ntfs_error(mp, "Index buffer (VCN 0x%llx) of directory inode "
				"0x%llx crosses page boundary.  Impossible! "
				"Cannot access!  This is probably a bug in "
				"the driver.", (unsigned long long)vcn,
				(unsigned long long)dir_ni->mft_no);
		goto page_err;
	}
	index_end = (u8*)&ia->index + le32_to_cpu(ia->index.index_length);
	if (index_end > (u8*)ia + ia_ni->block_size) {
		ntfs_error(mp, "Size of index buffer (VCN 0x%llx) of directory "
				"inode 0x%llx exceeds maximum size.",
				(unsigned long long)vcn,
				(unsigned long long)dir_ni->mft_no);
		goto page_err;
	}
	/* The first index entry. */
	ie = (INDEX_ENTRY*)((u8*)&ia->index +
			le32_to_cpu(ia->index.entries_offset));
	/*
	 * Iterate similar to above big loop but applied to index buffer, thus
	 * loop until we exceed valid memory (corruption case) or until we
	 * reach the last entry.
	 */
	for (;; ie = (INDEX_ENTRY*)((u8*)ie + le16_to_cpu(ie->length))) {
		/* Bounds check. */
		if ((u8*)ie < (u8*)&ia->index || (u8*)ie +
				sizeof(INDEX_ENTRY_HEADER) > index_end ||
				(u8*)ie + le16_to_cpu(ie->length) >
				index_end) {
			ntfs_error(mp, "Index entry out of bounds in "
					"directory inode 0x%llx.",
					(unsigned long long)dir_ni->mft_no);
			goto page_err;
		}
		/*
		 * The last entry cannot contain a name.  It can however
		 * contain a pointer to a child node in the B+tree so we just
		 * break out.
		 */
		if (ie->flags & INDEX_ENTRY_END)
			break;
		/*
		 * We perform a case sensitive comparison and if that matches
		 * we are done and return the mft reference of the inode (i.e.
		 * the inode number together with the sequence number for
		 * consistency checking).  We convert it to cpu format before
		 * returning.
		 */
		if (ntfs_are_names_equal(uname, uname_len,
				(ntfschar*)&ie->key.filename.filename,
				ie->key.filename.filename_length, TRUE,
				vol->upcase, vol->upcase_len)) {
found_it2:
			/*
			 * We have a perfect match, so we do not need to care
			 * about having matched imperfectly before, so we can
			 * free name and set *res_name to NULL.
			 *
			 * However, if the perfect match is a short filename,
			 * we need to signal this through *res_name, so that
			 * the caller can deal with the name cache effectively.
			 *
			 * As an optimization we just reuse an existing
			 * allocation of *res_name.
			 */
			if (ie->key.filename.filename_type == FILENAME_DOS) {
				u8 len;

				if (!name) {
					*res_name = name = OSMalloc(
							sizeof(*name),
							ntfs_malloc_tag);
					if (!name) {
						err = ENOMEM;
						goto page_err;
					}
				}
				name->mref = le64_to_cpu(ie->indexed_file);
				name->type = FILENAME_DOS;
				name->len = len = ie->key.filename.
						filename_length;
				memcpy(name->name, ie->key.filename.filename,
						len * sizeof(ntfschar));
			} else {
				if (name)
					OSFree(name, sizeof(*name),
							ntfs_malloc_tag);
				*res_name = NULL;
			}
			*res_mref = le64_to_cpu(ie->indexed_file);
			ntfs_page_unmap(ia_ni, upl, pl, FALSE);
			lck_rw_unlock_shared(&ia_ni->lock);
			(void)vnode_put(ia_vn);
			return 0;
		}
		/*
		 * For a case insensitive mount, we also perform a case
		 * insensitive comparison.  If the comparison matches, we cache
		 * the filename in *res_name so that the caller can work on it.
		 * If the comparison matches, and the name is in the DOS
		 * namespace, we only cache the mft reference and the filename
		 * type (we set the name length to zero for simplicity).
		 */
		if (!NVolCaseSensitive(vol) &&
				ntfs_are_names_equal(uname, uname_len,
				(ntfschar*)&ie->key.filename.filename,
				ie->key.filename.filename_length, FALSE,
				vol->upcase, vol->upcase_len)) {
			u8 type;

			/*
			 * If no name is cached yet, cache it or if the current
			 * name is the WIN32 name, replace the already cached
			 * name with the WIN32 name.  Otherwise continue
			 * caching the first match.
			 */
			type = ie->key.filename.filename_type;
			if (!name || type == FILENAME_WIN32 || type ==
					FILENAME_WIN32_AND_DOS) {
				u8 len;

				if (!name) {
					*res_name = name = OSMalloc(
							sizeof(*name),
							ntfs_malloc_tag);
					if (!name) {
						err = ENOMEM;
						goto page_err;
					}
				}
				name->mref = le64_to_cpu(ie->indexed_file);
				name->type = type;
				name->len = len = ie->key.filename.
						filename_length;
				memcpy(name->name, ie->key.filename.filename,
						len * sizeof(ntfschar));
			}
		}
		/*
		 * Not a perfect match, need to do full blown collation so we
		 * know which way in the B+tree we have to go.
		 */
		rc = ntfs_collate_names(uname, uname_len,
				(ntfschar*)&ie->key.filename.filename,
				ie->key.filename.filename_length, 1, FALSE,
				vol->upcase, vol->upcase_len);
		/*
		 * If uname collates before the name of the current entry,
		 * there is definitely no such name in this index but we might
		 * need to descend into the B+tree so we just break out of the
		 * loop.
		 */
		if (rc == -1)
			break;
		/* The names are not equal, continue the search. */
		if (rc)
			continue;
		/*
		 * Names match with case insensitive comparison, now try the
		 * case sensitive comparison, which is required for proper
		 * collation.
		 */
		rc = ntfs_collate_names(uname, uname_len,
				(ntfschar*)&ie->key.filename.filename,
				ie->key.filename.filename_length, 1, TRUE,
				vol->upcase, vol->upcase_len);
		if (rc == -1)
			break;
		if (rc)
			continue;
		/*
		 * Perfect match, this will never happen as the
		 * ntfs_are_names_equal() call will have gotten a match but we
		 * still treat it correctly.
		 */
		goto found_it2;
	}
	/*
	 * We have finished with this index buffer without success.  Check for
	 * the presence of a child node.
	 */
	if (ie->flags & INDEX_ENTRY_NODE) {
		if ((ia->index.flags & NODE_MASK) == LEAF_NODE) {
			ntfs_error(mp, "Index entry with child node found in "
					"a leaf node in directory inode "
					"0x%llx.",
					(unsigned long long)dir_ni->mft_no);
			goto page_err;
		}
		/* Child node present, descend into it. */
		old_vcn = vcn;
		vcn = sle64_to_cpup((sle64*)((u8*)ie +
				le16_to_cpu(ie->length) - 8));
		if (vcn >= 0) {
			/*
			 * If @vcn is in the same page cache page as @old_vcn
			 * we recycle the mapped page.
			 */
			if (old_vcn << ia_ni->vcn_size_shift >> PAGE_SHIFT ==
					vcn << ia_ni->vcn_size_shift >>
					PAGE_SHIFT)
				goto fast_descend_into_child_node;
			ntfs_page_unmap(ia_ni, upl, pl, FALSE);
			goto descend_into_child_node;
		}
		ntfs_error(mp, "Negative child node vcn in directory inode "
				"0x%llx.", (unsigned long long)dir_ni->mft_no);
		goto page_err;
	}
	/*
	 * No child node present, return ENOENT, unless we have got a matching
	 * name cached in @name in which case return the mft reference
	 * associated with it.
	 */
	ntfs_page_unmap(ia_ni, upl, pl, FALSE);
not_found:
	lck_rw_unlock_shared(&ia_ni->lock);
	(void)vnode_put(ia_vn);
	if (name) {
		*res_mref = name->mref;
		return 0;
	}
	ntfs_debug("Entry not found.");
	return ENOENT;
page_err:
	ntfs_page_unmap(ia_ni, upl, pl, FALSE);
	goto err;
dir_err:
	ntfs_error(mp, "Corrupt directory inode 0x%llx.  Run chkdsk.",
			(unsigned long long)dir_ni->mft_no);
put_err:
	ntfs_attr_search_ctx_put(ctx);
unm_err:
	ntfs_mft_record_unmap(dir_ni);
err:
	if (name)
		OSFree(name, sizeof(*name), ntfs_malloc_tag);
	lck_rw_unlock_shared(&ia_ni->lock);
	(void)vnode_put(ia_vn);
	if (!err)
		err = EIO;
	ntfs_debug("Failed (error %d).", err);
	return err;
}

/**
 * ntfs_do_dirent - generate a dirent structure and copy it to the destination
 * @vol:	ntfs volume the index entry belongs to
 * @ie:		index entry to return after conversion to a dirent structure
 * @de:		buffer to generate the dirent structure in
 * @uio:	destination in which to return the generated dirent structure
 * @entries:	[IN/OUT] pointer to number of entries that have been returned
 *
 * This is a helper function for ntfs_readdir().
 *
 * First, check if we want to return this index entry @ie and if not return 0.
 *
 * Assuming we want to return this index entry, convert the NTFS specific
 * directory index entry @ie into the file system independent dirent structure
 * and store it in the supplied buffer @de.
 *
 * If there is not enough space in the destination @uio to return the converted
 * dirent structure @de, return -1.
 *
 * Return the converted dirent structure @de in the destination @uio and return
 * 0 on success or errno on error.
 *
 * If we successfully returned an entry *@entries is incremented.
 */
static inline int ntfs_do_dirent(ntfs_volume *vol, INDEX_ENTRY *ie,
		struct dirent *de, uio_t uio, int *entries)
{
	ino64_t mref;
	u8 *utf8_name;
	size_t utf8_size;
	signed res_size, padding;
	int err;
	FILENAME_TYPE_FLAGS name_type;
#ifdef DEBUG
	static const char *dts[15] = { "UNKNOWN", "FIFO", "CHR", "UNKNOWN",
			"DIR", "UNKNOWN", "BLK", "UNKNOWN", "REG", "UNKNOWN",
			"LNK", "UNKNOWN", "SOCK", "UNKNOWN", "WHT" };
#endif

	name_type = ie->key.filename.filename_type;
	if (name_type == FILENAME_DOS) {
		ntfs_debug("Skipping DOS namespace entry.");
		return 0;
	}
	mref = MREF_LE(ie->indexed_file);
	/*
	 * Remove all NTFS core system files from the name space so we do not
	 * need to worry about users damaging a volume by writing to them or
	 * deleting/renaming them and so that we can return fsRtParID (1) as
	 * the inode number of the parent of the volume root directory and
	 * fsRtDirID (2) as the inode number of the volume root directory which
	 * are both expected by Carbon and various applications.
	 */
	if (mref < FILE_first_user) {
		ntfs_debug("Removing core NTFS system file (mft_no 0x%x) from "
				"name space.", (unsigned)mref);
		return 0;
	}
	if (sizeof(de->d_ino) < 8 && mref & 0xffffffff00000000ULL) {
		ntfs_warning(vol->mp, "Skipping dirent because its inode "
				"number 0x%llx does not fit in 32-bits.",
				(unsigned long long)mref);
		return 0;
	}
	utf8_name = (u8*)de->d_name;
	utf8_size = sizeof(de->d_name);
	res_size = ntfs_to_utf8(vol, (ntfschar*)&ie->key.filename.filename,
			ie->key.filename.filename_length << NTFSCHAR_SIZE_SHIFT,
			&utf8_name, &utf8_size);
	if (res_size <= 0) {
		ntfs_warning(vol->mp, "Skipping unrepresentable inode 0x%llx "
				"(error %d).", (unsigned long long)mref,
				-res_size);
		return 0;
	}
	/*
	 * The name is now in @de->d_name.  Set up the remainder of the dirent
	 * structure.
	 */
	de->d_ino = mref;
	/*
	 * If a filename index is present it must be a directory.  Otherwise it
	 * could be a file or a symbolic link (or something else but we do not
	 * support anything else yet).
	 */
	if (ie->key.filename.file_attributes &
			FILE_ATTR_DUP_FILENAME_INDEX_PRESENT)
		de->d_type = DT_DIR;
	else {
		/*
		 * If the file size is less than or equal to MAXPATHLEN it
		 * could be a symbolic link so return DT_UNKNOWN as it would be
		 * too expensive to get the inode to check what it is exactly.
		 *
		 * Also, system files need to be returned as DT_UNKNOWN as they
		 * could be fifos, sockets, or block or character device
		 * special files.  Note that the size check will actually catch
		 * all relevant system files so we do not need to check for
		 * them specifically here.
		 */
		if (ie->key.filename.data_size > MAXPATHLEN)
			de->d_type = DT_REG;
		else
			de->d_type = DT_UNKNOWN;
	}
	/*
	 * Note @de->d_namlen is only 8-bit thus @res_size may not be above
	 * 255.  This is not a problem since sizeof(de->d_name) is 256 which
	 * includes the terminating NUL byte thus ntfs_to_utf8() would have
	 * aborted if the name translated to something longer than 255 bytes.
	 *
	 * As a little BUG check test it anyway...
	 */
	if (res_size > 0xff)
		panic("%s(): res_size (0x%x) does not fit in 8 bits.  This is "
				"a bug!", __FUNCTION__, res_size);
	de->d_namlen = res_size;
	/* Add the NUL terminator byte to the name length. */
	res_size += offsetof(struct dirent, d_name) + 1;
	de->d_reclen = (u16)(res_size + 3) & (u16)~3;
	padding = de->d_reclen - res_size;
	if (padding)
		bzero((u8*)de + res_size, padding);
	/*
	 * If the remaining buffer space is not big enough to store the dirent
	 * structure, return -1 to indicate that fact.
	 */
	if (uio_resid(uio) < de->d_reclen)
		return -1;
	ntfs_debug("Returning dirent with d_ino 0x%llx, d_reclen 0x%x, d_type "
			"DT_%s, d_namlen %d, d_name \"%s\".",
			(unsigned long long)mref, (unsigned)de->d_reclen,
			de->d_type < 15 ? dts[de->d_type] : dts[0],
			(unsigned)de->d_namlen, de->d_name);
	/*
	 * Copy the dirent structure to the result buffer.  uiomove() returns
	 * zero to indicate success and the (positive) error code on error so
	 * it can be clearly distinguished from us returning -1 to indicate
	 * that the buffer does not have enough space remaining.
	 */
	err = uiomove((caddr_t)de, de->d_reclen, uio);
	if (!err) {
		/* We have successfully returned another dirent structure. */
		(*entries)++;
	}
	return err;
}

/**
 * ntfs_dirhint_get - get a directory hint
 * @ni:		ntfs index inode of directory index for which to get a hint
 * @ofs:	offset (containing tag and B+tree position) for hint to get
 *
 * Search through the list of hints attached to the ntfs directory index inode
 * @ni for a directory hint with an offset @ofs.  If found return that hint and
 * if not found either allocate a new directory hint or recycle an the oldest
 * directory hint in the list and set it up ready to use.
 *
 * Return the directory hint or NULL if allocating a new hint failed and no
 * hints are present in the list so a hint could not be recycled either.
 *
 * The caller can tell if the hint matched by the fact that if it matched it
 * will have a filename attached to it, i.e. ->fn_size and thus also ->fn will
 * be non-zero and non-NULL, respectively.
 *
 * Locking: Caller must hold @ni->lock for writing.
 */
static ntfs_dirhint *ntfs_dirhint_get(ntfs_inode *ni, unsigned ofs)
{
	ntfs_dirhint *dh;
	BOOL need_init, need_remove;
	struct timeval tv;

	microuptime(&tv);
	/*
	 * Look for an existing hint first.  If not found, create a new one
	 * (when the list is not full) or recycle the oldest hint.  Since new
	 * hints are always added to the head of the list, the last hint is
	 * always the oldest.
	 */
	dh = NULL;
	if (ofs & ~NTFS_DIR_POS_MASK) {
		TAILQ_FOREACH(dh, &ni->dirhint_list, link) {
			if (dh->ofs == ofs)
				break;
		}
	}
	/* Assume we found a directory hint in which case it is initialized. */
	need_init = FALSE;
	need_remove = TRUE;
	if (!dh) {
		/* No directory hint matched. */
		need_init = TRUE;
		if (ni->nr_dirhints < NTFS_MAX_DIRHINTS) {
			/*
			 * Allocate a new directory hint.  If the allocation
			 * fails try to recycle an existing directory hint.
			 */
			dh = OSMalloc(sizeof(*dh), ntfs_malloc_tag);
			if (dh) {
				ni->nr_dirhints++;
				need_remove = FALSE;
			}
		}
		if (!dh) {
			/* Recycle the last, i.e. oldest, directory hint. */
			dh = TAILQ_LAST(&ni->dirhint_list, ntfs_dirhint_head);
			if (dh && dh->fn_size)
				OSFree(dh->fn, dh->fn_size, ntfs_malloc_tag);
		}
	}
	/*
	 * If we managed to get a hint, move it to (or place it at if we
	 * allocated it above) the head of the list of dircetory hints of the
	 * index inode.
	 */
	if (dh) {
		if (need_remove)
			TAILQ_REMOVE(&ni->dirhint_list, dh, link);
		TAILQ_INSERT_HEAD(&ni->dirhint_list, dh, link);
		/*
		 * Set up the hint if it is a new hint or we recycled an old
		 * hint.
		 */
		if (need_init) {
			dh->ofs = ofs;
			dh->fn_size = 0;
		}
		dh->time = tv.tv_sec;
	}
	return dh;
}

/**
 * ntfs_dirhint_put - put a directory hint
 * @ni:		ntfs index inode to which the directory hint belongs
 * @dh:		directory hint to free
 *
 * Detach the directory hint @dh from the ntfs directory index inode @ni and
 * free it and all its resources.
 *
 * Locking: Caller must hold @ni->lock for writing.
 */
static void ntfs_dirhint_put(ntfs_inode *ni, ntfs_dirhint *dh)
{
	TAILQ_REMOVE(&ni->dirhint_list, dh, link);
	ni->nr_dirhints--;
	if (dh->fn_size)
		OSFree(dh->fn, dh->fn_size, ntfs_malloc_tag);
	OSFree(dh, sizeof(*dh), ntfs_malloc_tag);
}

/**
 * ntfs_dirhints_put - put all directory hints
 * @ni:		ntfs index inode whose directory hints to release
 * @stale_only:	if true only release expired directory hints
 *
 * If @stale_only is false release all directory hints from the ntfs directory
 * index inode @ni freeing them and all their resources.
 *
 * If @stale_only is true do the same as above but only release expired hints.
 *
 * Note we iterate from the oldest to the newest so we can stop when we reach
 * the first valid hint if @stale_only is true.
 *
 * Locking: Caller must hold @ni->lock for writing.
 */
void ntfs_dirhints_put(ntfs_inode *ni, BOOL stale_only)
{
	ntfs_dirhint *dh, *tdh;
	struct timeval tv;

	if (stale_only)
		microuptime(&tv);
	TAILQ_FOREACH_REVERSE_SAFE(dh, &ni->dirhint_list, ntfs_dirhint_head,
			link, tdh) {
		if (stale_only) {
			/* Stop here if this entry is too new. */
			if (tv.tv_sec - dh->time < NTFS_DIRHINT_TTL)
				break;
		}
		ntfs_dirhint_put(ni, dh);
	}
}

/**
 * ntfs_readdir - read directory entries into a supplied buffer
 * @dir_ni:	directory inode to read directory entries from
 * @uio:	destination in which to return the read entries
 * @eofflag:	return end of file status (can be NULL)
 * @numdirent:	return number of entries returned (can be NULL)
 *
 * ntfs_readdir() reads directory entries starting at the position described by
 * uio_offset() into the buffer pointed to by @uio in a file system independent
 * format.  Up to uio_resid() bytes of data can be returned.  The data in the
 * buffer is a series of packed dirent structures where each contains the
 * following elements:
 *
 *	ino_t	d_ino;			inode number of this entry
 *	u16	d_reclen;		length of this entry record
 *	u8	d_type;			inode type (see below)
 *	u8	d_namlen;		length of string in d_name
 *	char	d_name[MAXNAMELEN + 1];	null terminated filename
 *
 * The length of the record (d_reclen) must be a multiple of four.
 *
 * The following file types are defined:
 *	DT_UNKNOWN, DT_FIFO, DT_CHR, DT_DIR, DT_BLK, DT_REG, DT_LNK, DT_SOCK,
 *	DT_WHT
 *
 * The name (d_name) must be at most MAXNAMELEN + 1 bytes long including the
 * compulsory NUL terminator.
 *
 * If the name length (d_namlen) is not a multiple of four, the unused space
 * between the NUL terminator of the name and the end of the record (as
 * specified by d_reclen which is aligned to four bytes) is filled with NUL
 * bytes.
 *
 * Note how the inode number (d_ino) is only 32 bits.  Thus we do not return
 * directory entries for inodes with an inode number that does not fit in 32
 * bits.  In practice (at the present time) this is not a problem as 2^32
 * inodes are a lot of inodes so are unlikely to be reached with existing data
 * storage hardware that is NTFS formatted and accessed by OS X.  Further, up
 * to and including Windows XP, Windows itself limits the maximum number of
 * inodes to 2^32.
 *
 * When the current position (uio_offset()) is zero, we start at the first
 * entry in the B+tree and then follow the entries in the B+tree in sequence.
 * We cannot ignore the B+tree and just return all the index root entries
 * followed by all the entries from each of the in-use index allocation blocks
 * because when an entry is added to or deleted from the directory this can
 * reshape the B+tree thus making it impossible to continue where we left of
 * between two VNOP_READDIR() calls and thus makes it impossible to implement
 * POSIX seekdir()/telldir()/readdir() semantics.
 *
 * The current position (uio_offset()) refers to the next block of entries to
 * be returned.  The offset can only be set to a value previously returned by
 * ntfs_vnop_readdir() or zero.  This offset does not have to match the number
 * of bytes returned (in uio_resid()).
 *
 * Note that whilst uio_resid() is 32-bit, uio_offset() is of type off_t which
 * is 64-bit in OS X but it gets cast down to a 32-bit long on ppc and i386 by
 * the getdirentries() system call before it is returned to user space so we
 * cannot use more than the lower 32-bits of the uio_offset().
 *
 * In fact, the offset used by NTFS is essentially a numerical position as
 * described above (26 bits) with a tag (6 bits).  The tag is for associating
 * the next request with the current request.  This enables us to have multiple
 * threads reading the directory while the directory is also being modified.
 *
 * Each tag/position pair is tied to a unique directory hint.  The hint
 * contains information (filename) needed to build the B+tree index context
 * path for finding the next set of entries.
 *
 * The reason not to just use a unique tag each time that identifies a
 * directory hint is that we have no way to expire tags/directory hints when a
 * directory file descriptor is closed and instead only find out when all users
 * of the directory have closed it via our VNOP_INACTIVE() being called.  Thus,
 * we only can afford to keep a bounded number of tags/directory hints per
 * vnode thus we have to expire old tags/directory hints as new ones are added.
 * And when ntfs_readdir() is called with an expired tag we would have no way
 * of knowing where in the directory to proceed without the associated
 * numerical offset into the B+tree which tells us the position at which to
 * continue if there had not been any modifications since the tag and position
 * were returned by ntfs_readdir().  In practice in most cases this will still
 * be approximately the same location as where we left off unless a lot of
 * files have been created in/deleted from the directory.  This is not perfect
 * as it means we are only POSIX compliant when a tag/directory hint has not
 * expired but it is a lot better than nothing so is worth doing.  Also, using
 * only 26 bits for the numerical position in the B+tree still alows for
 * directories with up to 2^26-1 entries, i.e. over 67 million entries which is
 * likely to be quite sufficient for most intents and purposes.
 *
 * If @eofflag is not NULL, set *eofflag to 0 if we have not reached the end of
 * the directory yet and set it to 1 if we have reached the end of the
 * directory, i.e. @uio either contains nothing or it contains the last entry
 * in the directory.
 *
 * If @numdirent is not NULL, set *@numdirent to the number of directory
 * entries returned in the buffer described by @uio.
 *
 * If the directory has been deleted, i.e. @dir_ni->link_count is zero, do not
 * synthesize entries for "." and "..".
 *
 * Locking: Caller must hold @dir_ni->lock.
 */
errno_t ntfs_readdir(ntfs_inode *dir_ni, uio_t uio, int *eofflag,
		int *numdirent)
{
	off_t ofs;
	ntfs_volume *vol;
	struct dirent *de;
	ntfs_inode *ia_ni;
	ntfs_index_context *ictx;
	ntfs_dirhint *dh;
	int eof, entries, err;
	unsigned tag;
	/*
	 * This is quite big to go on the stack but only half the size of the
	 * buffers placed on the stack in ntfs_vnop_lookup() so if they are ok
	 * so should this be.
	 */
	u8 de_buf[sizeof(struct dirent) + 4];

	ofs = uio_offset(uio);
	vol = dir_ni->vol;
	de = (struct dirent*)&de_buf;
	ia_ni = NULL;
	ictx = NULL;
	dh = NULL;
	err = entries = eof = tag = 0;
	ntfs_debug("Entering for directory inode 0x%llx, offset 0x%llx, count "
			"0x%llx.", (unsigned long long)dir_ni->mft_no,
			(unsigned long long)ofs,
			(unsigned long long)uio_resid(uio));
	/*
	 * If we already reached the end of the directory, there is nothing to
	 * do.
	 */
	if ((unsigned)ofs == (unsigned)-1)
		goto eof;
	tag = (unsigned)ofs & NTFS_DIR_TAG_MASK;
	ofs &= NTFS_DIR_POS_MASK;
	/*
	 * Sanity check the uio data.  The absolute minimum buffer size
	 * required is the number of bytes taken by the entries in the dirent
	 * structure up to the beginning of the name plus the minimum length
	 * for a filename of one byte plus we need to align each dirent record
	 * to a multiple of four bytes thus effectovely the minimum name length
	 * is four and not one.
	 */
	if (uio_resid(uio) < (unsigned)offsetof(struct dirent, d_name) + 4) {
		err = EINVAL;
		goto err;
	}
	/*
	 * Emulate "." and ".." for all directories unless the directory has
	 * been deleted but not closed yet.
	 */
	while (ofs < 2) {
		if (!dir_ni->link_count) {
			ofs = 2;
			break;
		}
		*(u32*)de->d_name = 0;
		de->d_name[0] = '.';
		if (!ofs) {
			/*
			 * We have to remap the root directory inode to inode
			 * number 2, i.e. fsRtDirID, for compatibility with
			 * Carbon.
			 */
			if (dir_ni->mft_no == FILE_root)
				de->d_ino = 2;
			else {
				if (sizeof(de->d_ino) < 8 && dir_ni->mft_no &
						0xffffffff00000000ULL) {
					ntfs_warning(vol->mp, "Skipping "
							"emulated dirent for "
							"\".\" because its "
							"inode number 0x%llx "
							"does not fit in "
							"32-bits.",
							(unsigned long long)
							dir_ni->mft_no);
					goto do_next;
				}
				de->d_ino = dir_ni->mft_no;
			}
			de->d_namlen = 1;
		} else {
			vnode_t parent_vn;

			/*
			 * We have to return 1, i.e. fsRtParID, for the parent
			 * inode number of the root directory inode for
			 * compatibility with Carbon.
			 */
			if (dir_ni->mft_no == FILE_root)
				de->d_ino = 1;
			else if ((parent_vn = vnode_getparent(dir_ni->vn))) {
				if (sizeof(de->d_ino) < 8 &&
						NTFS_I(parent_vn)->mft_no &
						0xffffffff00000000ULL) {
					ntfs_warning(vol->mp, "Skipping "
							"emulated dirent for "
							"\"..\" because its "
							"inode number 0x%llx "
							"does not fit in "
							"32-bits.",
							(unsigned long long)
							NTFS_I(parent_vn)->
							mft_no);
					goto do_next;
				}
				de->d_ino = NTFS_I(parent_vn)->mft_no;
				/*
				 * Remap the root directory inode to inode
				 * number 2 (see above).
				 */
				if (de->d_ino == FILE_root)
					de->d_ino = 2;
				(void)vnode_put(parent_vn);
			} else {
				MFT_REF mref;

				/*
				 * Look up a filename attribute in the mft
				 * record of the directory @dir_ni and use its
				 * parent mft reference for "..".
				 */
				err = ntfs_inode_get_name_and_parent_mref(
						dir_ni, FALSE, &mref, NULL);
				if (err) {
					ntfs_warning(vol->mp, "Skipping "
							"emulated dirent for "
							"\"..\" because its "
							"inode number could "
							"not be determined "
							"(error %d).", err);
					goto do_next;
				}
				if (sizeof(de->d_ino) < 8 && MREF(mref) &
						0xffffffff00000000ULL) {
					ntfs_warning(vol->mp, "Skipping "
							"emulated dirent for "
							"\"..\" because its "
							"inode number 0x%llx "
							"does not fit in "
							"32-bits.",
							(unsigned long long)
							MREF(mref));
					goto do_next;
				}
				de->d_ino = MREF(mref);
				/*
				 * Remap the root directory inode to inode
				 * number 2 (see above).
				 */
				if (de->d_ino == FILE_root)
					de->d_ino = 2;
			}
			de->d_namlen = 2;
			de->d_name[1] = '.';
		}
		/*
		 * The name is one or two bytes long but we need to align the
		 * entry record to a multiple of four bytes, thus add four
		 * instead of one or two to the name offset.
		 */
		de->d_reclen = offsetof(struct dirent, d_name) + 4;
		de->d_type = DT_DIR;
		ntfs_debug("Returning emulated \"%s\" dirent with d_ino "
				"0x%llx, d_reclen 0x%x, d_type DT_DIR, "
				"d_namlen %d.", de->d_name,
				(unsigned long long)de->d_ino,
				(unsigned)de->d_reclen,
				(unsigned)de->d_namlen);
		err = uiomove((caddr_t)de, de->d_reclen, uio);
		if (err) {
			ntfs_error(vol->mp, "uiomove() failed for emulated "
					"entry (error %d).", err);
			goto err;
		}
		entries++;
do_next:
		/* We are done with this entry. */
		ofs++;
		if (uio_resid(uio) < (unsigned)offsetof(struct dirent, d_name)
				+ 4) {
			err = -1;
			goto done;
		}
	}
	/* Get the index allocation inode. */
	err = ntfs_index_inode_get(dir_ni, I30, 4, FALSE, &ia_ni);
	if (err) {
		ntfs_error(vol->mp, "Failed to get index vnode (error %d).",
				err);
		ia_ni = NULL;
		goto err;
	}
	/* We need the lock exclusive because of the directory hints code. */
	lck_rw_lock_exclusive(&ia_ni->lock);
	ictx = ntfs_index_ctx_get(ia_ni);
	if (!ictx) {
		ntfs_error(vol->mp, "Not enough memory to allocate index "
				"context.");
		err = ENOMEM;
		goto err;
	}
	/*
	 * Get the directory hint matching the current tag and offset if it
	 * exists and if not get a new directory hint.
	 */
	dh = ntfs_dirhint_get(ia_ni, ofs | tag);
	if (!dh) {
		/*
		 * We have run out of memory and failed to allocate a new hint.
		 * This also implies that the hint was not found thus we might
		 * as well reset the tag to zero so we do not bother searching
		 * for it next time.  We will just use the numerical position
		 * in the directory in order to determine where to continue the
		 * directory lookup.
		 */
		tag = 0;
		goto lookup_by_position;
	}
	/*
	 * If there is no filename attached to the directory hint, use lookup
	 * by position in stead of by filename.
	 */
	if (!dh->fn_size)
		goto lookup_by_position;
	/*
	 * The directory hint contains a filename, look it up and return it
	 * to the caller.  Then, continue iterating over the directory B+tree
	 * returning each entry.  If the directory entry has been deleted, the
	 * lookup up will return the next entry in the B+tree.  This needs
	 * special handling because the found entry could be an end entry in
	 * which case we need to switch to the next real entry.
	 */
	if (!dh->fn)
		panic("%s(): !dh->fn\n", __FUNCTION__);
	/* If the lookup fails fall back to looking up by position. */
	err = ntfs_index_lookup(dh->fn, dh->fn_size, &ictx);
	if (!err)
		goto do_dirent;
	if (err != ENOENT) {
		ntfs_warning(vol->mp, "Failed to look up filename from "
				"directory hint (error %d), using position in "
				"the B+tree to continue the lookup.", err);
		ntfs_index_ctx_reinit(ictx, ia_ni);
		goto lookup_by_position;
	}
	err = 0;
	/*
	 * Entry was not found, but the next one was returned.  If this is a
	 * real entry pretend that this is the entry we were looking for.
	 */
	if (!(ictx->entry->flags & INDEX_ENTRY_END)) {
		ictx->is_match = 1;
		goto do_dirent;
	}
	/*
	 * This is an end entry which does not contain a filename.  Switch to
	 * the next real entry in the B+tree.
	 *
	 * Note by definition we must be in a leaf node.
	 */
	if (ictx->entry->flags & INDEX_ENTRY_NODE)
		panic("%s(): ictx->entry->flags & INDEX_ENTRY_NODE\n",
				__FUNCTION__);
	/*
	 * The next entry is the first real entry above the current node thus
	 * keep moving up the B+tree until we find a real entry.
	 */
	do {
		ntfs_index_context *itmp;

		/* If we are in the index root, we are done. */
		if (ictx->is_root)
			goto eof;
		/* Save the current index context so we can free it. */
		itmp = ictx;
		/* Move up to the parent node. */
		ictx = ictx->up;
		/*
		 * Disconnect the old index context from its path and free it
		 * and all its resources.
		 */
		ntfs_index_ctx_put_single(itmp);
	} while (ictx->entry_nr == ictx->nr_entries - 1);
	/*
	 * We have reached a node with a real index entry.  Lock it so we can
	 * work on it.
	 */
	err = ntfs_index_ctx_relock(ictx);
	if (err)
		goto err;
	ictx->is_match = 1;
	goto do_dirent;
lookup_by_position:
	/*
	 * Start a search at the beginning of the B+tree and look for the entry
	 * number @ofs - 2.
	 *
	 * We need the -2 to account for the synthesized ".." and "." entries.
	 */
	err = ntfs_index_lookup_by_position(ofs - 2, 0, &ictx);
	/*
	 * Starting with the current entry, iterate over all remaining entries,
	 * returning each via a call to ntfs_do_dirent().
	 */
	while (!err) {
do_dirent:
		/* Submit the current directory entry to our helper function. */
		err = ntfs_do_dirent(vol, ictx->entry, de, uio, &entries);
		if (err) {
			/*
			 * A negative error code means the destination @uio
			 * did not have enough space for the directory entry.
			 */
			if (err < 0)
				goto done;
			/* Positive error code; uiomove() returned error. */
			ntfs_error(vol->mp, "uiomove() failed for index %s "
					"entry (error %d).",
					ictx->is_root ? "root" : "allocation",
					err);
			goto err;
		}
		/* We are done with this entry. */
		ofs++;
		/* Go to the next directory entry. */
		err = ntfs_index_lookup_next(&ictx);
	}
	if (err != ENOENT) {
		ntfs_error(vol->mp, "Failed to look up index entry with "
				"position 0x%llx.",
				(unsigned long long)(ofs - 2));
		goto err;
	}
eof:
	eof = 1;
	ofs = (unsigned)-1;
done:
	/*
	 * If @err is less than zero, we got here because the @uio does not
	 * have enough space for the next directory entry.  If we have not
	 * returned any directory entries yet, this means the buffer is too
	 * small for even one single entry so return the appropriate error code
	 * instead of zero.
	 */
	if (err < 0 && !entries)
		err = EINVAL;
	else
		err = 0;
err:
	/*
	 * If the offset has overflown NTFS_DIR_POS_MASK we cannot record it so
	 * just set it to the maximum we can return.  This is not a problem
	 * when we record a directory hint as is the common case and then later
	 * use it to continue as the offset is then not actually used and
	 * instead the name is used which is independent of its location.  In
	 * this case however do update the tag so that we return a different
	 * apparent offset to the caller between invocations.
	 *
	 * Note we have to avoid @ofs becomming (unsigned)-1 because we use
	 * that to denote end of directory.
	 */
	if (!eof && ofs & ~(off_t)NTFS_DIR_POS_MASK) {
		ofs = NTFS_DIR_POS_MASK;
		tag = (unsigned)(++ia_ni->dirhint_tag) << NTFS_DIR_TAG_SHIFT;
		if (!tag || (tag | NTFS_DIR_POS_MASK) == (unsigned)-1) {
			ia_ni->dirhint_tag = 1;
			tag = (unsigned)1 << NTFS_DIR_TAG_SHIFT;
		}
	}
	/*
	 * If we have a directory hint, update it with the current search state
	 * so the next call can continue where we stopped.
	 */
	if (dh) {
		unsigned size;

		if (eof || err) {
			/*
			 * The end of the directory was reached or an error
			 * occurred.  Discard the directory hint.
			 */
			ntfs_dirhint_put(ia_ni, dh);
			goto dh_done;
		}
		/*
		 * Add the current name to the directory hint.  This is the
		 * next name we need to return to the caller.  If there is an
		 * old name then reuse its buffer if the two are the same size
		 * and otherwise free the old name first.
		 */
		size = le16_to_cpu(ictx->entry->key_length);
		if (dh->fn_size != size) {
			if (dh->fn_size)
				OSFree(dh->fn, dh->fn_size, ntfs_malloc_tag);
			dh->fn = OSMalloc(size, ntfs_malloc_tag);
			if (!dh->fn) {
				/*
				 * Not enough memory to set up the directory
				 * hint.  Just throw it away and set the tag to
				 * zero so we continue by position next time.
				 */
				dh->fn_size = 0;
				ntfs_dirhint_put(ia_ni, dh);
				tag = 0;
				goto dh_done;
			}
			dh->fn_size = size;
		}
		memcpy(dh->fn, &ictx->entry->key.filename, size);
		/*
		 * If the current tag is zero, we need to assign a new tag.
		 *
		 * Note we have to avoid @ofs becomming (unsigned)-1 because we
		 * use that to denote end of directory.
		 */
		if (!tag) {
			tag = (unsigned)(++ia_ni->dirhint_tag) <<
					NTFS_DIR_TAG_SHIFT;
			if (!tag || (tag | NTFS_DIR_POS_MASK) == (unsigned)-1) {
				ia_ni->dirhint_tag = 1;
				tag = (unsigned)1 << NTFS_DIR_TAG_SHIFT;
			}
		}
		/* Finally set the directory hint to the current offset. */
		dh->ofs = ofs | tag;
	}
dh_done:
	if (ictx)
		ntfs_index_ctx_put(ictx);
	if (ia_ni) {
		lck_rw_unlock_exclusive(&ia_ni->lock);
		(void)vnode_put(ia_ni->vn);
	}
	ntfs_debug("%s (returned 0x%x entries, %s, now at offset 0x%llx).",
			err ? "Failed" : "Done", entries, eof ?
			"reached end of directory" : "more entries to follow",
			(unsigned long long)ofs);
	if (eofflag)
		*eofflag = eof;
	if (numdirent)
		*numdirent = entries;
	uio_setoffset(uio, ofs | tag);
	return err;
}

/**
 * ntfs_dir_is_empty - check if a directory is empty
 * @dir_ni:	ntfs inode of directory to check
 *
 * Check if the directory inode @ni is empty.
 *
 * Return 0 if empty, ENOTEMPTY if not empty, and errno (not ENOTEMPTY) on
 * error.
 *
 * Locking: Caller must hold @dir_ni->lock for writing.
 */
errno_t ntfs_dir_is_empty(ntfs_inode *dir_ni)
{
	s64 bmp_size, prev_ia_pos, bmp_pos, ia_pos;
	ntfs_inode *ia_ni, *bmp_ni = NULL;
	ntfs_volume *vol = dir_ni->vol;
	MFT_RECORD *m;
	ntfs_attr_search_ctx *ctx;
	INDEX_ROOT *ir;
	u8 *index_end, *bmp, *kaddr;
	INDEX_ENTRY *ie;
	upl_t bmp_upl, ia_upl = NULL;
	upl_page_info_array_t bmp_pl, ia_pl;
	INDEX_ALLOCATION *ia;
	errno_t err;
	int bmp_ofs;
	static const char es[] = "%s.  Directory mft_no 0x%llx is corrupt.  "
			"Run chkdsk.";
	static const char es1[] = ".  Directory mft_no 0x";
	static const char es2[] = " is corrupt.  Run chkdsk.";

	ntfs_debug("Entering for directory mft_no 0x%llx.",
			(unsigned long long)dir_ni->mft_no);
	if (!S_ISDIR(dir_ni->mode))
		return ENOTDIR;
	/* Get the index allocation inode. */
	err = ntfs_index_inode_get(dir_ni, I30, 4, FALSE, &ia_ni);
	if (err) {
		ntfs_error(vol->mp, "Failed to get index inode (error %d).",
				err);
		return err;
	}
	lck_rw_lock_shared(&ia_ni->lock);
	/* Get the index bitmap inode if there is one. */
	if (NInoIndexAllocPresent(ia_ni)) {
		err = ntfs_attr_inode_get(dir_ni, AT_BITMAP, I30, 4, FALSE,
				LCK_RW_TYPE_SHARED, &bmp_ni);
		if (err) {
			ntfs_error(vol->mp, "Failed to get index bitmap inode "
					"(error %d).", err);
			bmp_ni = NULL;
			goto err;
		}
	}
	/* Get hold of the mft record for the directory. */
	err = ntfs_mft_record_map(dir_ni, &m);
	if (err) {
		ntfs_error(vol->mp, "Failed to map mft record for directory "
				"(error %d).", err);
		goto err;
	}
	ctx = ntfs_attr_search_ctx_get(dir_ni, m);
	if (!ctx) {
		ntfs_error(vol->mp, "Failed to get attribute search context.");
		err = ENOMEM;
		goto unm_err;
	}
	/* Find the index root attribute in the mft record. */
	err = ntfs_attr_lookup(AT_INDEX_ROOT, I30, 4, 0, NULL, 0, ctx);
	if (err) {
		if (err == ENOENT) {
			ntfs_error(vol->mp, "Index root attribute missing in "
					"directory inode 0x%llx.",
					(unsigned long long)dir_ni->mft_no);
			NVolSetErrors(vol);
			err = EIO;
		} else
			ntfs_error(vol->mp, "Failed to lookup index root "
					"attribute in directory inode 0x%llx "
					"(error %d).",
					(unsigned long long)dir_ni->mft_no,
					err);
		goto put_err;
	}
	/*
	 * Get to the index root value (it has been verified in
	 * ntfs_inode_read()).
	 */
	ir = (INDEX_ROOT*)((u8*)ctx->a + le16_to_cpu(ctx->a->value_offset));
	index_end = (u8*)&ir->index + le32_to_cpu(ir->index.index_length);
	/* The first index entry. */
	ie = (INDEX_ENTRY*)((u8*)&ir->index +
			le32_to_cpu(ir->index.entries_offset));
	/* Bounds checks. */
	if ((u8*)ie < (u8*)&ir->index ||
			(u8*)ie + sizeof(INDEX_ENTRY_HEADER) > index_end ||
			(u8*)ie + le16_to_cpu(ie->length) > index_end)
		goto dir_err;
	/*
	 * If this is not the end node, it is a filename and thus the directory
	 * is not empty.
	 *
	 * If it is the end node, and there is no sub-node hanging off it, the
	 * directory is empty.
	 */
	if (!(ie->flags & INDEX_ENTRY_END))
		err = ENOTEMPTY;
	else if (!(ie->flags & INDEX_ENTRY_NODE)) {
		/* Set @err to 1 so we can detect that we are done below. */
		err = 1;
	}
	ntfs_attr_search_ctx_put(ctx);
	ntfs_mft_record_unmap(dir_ni);
	if (err) {
		/* Undo the setting of @err to 1 we did above. */
		if (err == 1)
			err = 0;
		goto done;
	}
	/*
	 * We only get here if the index root indicated that there is a
	 * sub-node thus there must be an index allocation attribute.
	 */
	if (!NInoIndexAllocPresent(ia_ni)) {
		ntfs_error(vol->mp, "No index allocation attribute but index "
				"entry requires one.  Directory inode 0x%llx "
				"is corrupt or driver bug.",
				(unsigned long long)dir_ni->mft_no);
		goto dir_err;
	}
	lck_spin_lock(&bmp_ni->size_lock);
	bmp_size = bmp_ni->data_size;
	lck_spin_unlock(&bmp_ni->size_lock);
	ia_pos = bmp_pos = bmp_ofs = 0;
	prev_ia_pos = -1;
get_next_bmp_page:
	ntfs_debug("Reading index bitmap offset 0x%llx, bit offset 0x%x.",
			(unsigned long long)bmp_pos >> 3, bmp_ofs);
	/*
	 * Convert bit position to byte offset in the index bitmap attribute
	 * and map the corresponding page.
	 */
	err = ntfs_page_map(bmp_ni, (bmp_pos >> 3) & ~PAGE_MASK_64, &bmp_upl,
			&bmp_pl, &bmp, FALSE);
	if (err) {
		ntfs_error(vol->mp, "Failed to read directory index bitmap "
				"buffer (error %d).", err);
		bmp_upl = NULL;
		goto page_err;
	}
	/* Find the next index block which is marked in use. */
	while (!(bmp[bmp_ofs >> 3] & (1 << (bmp_ofs & 7)))) {
find_next_index_buffer:
		bmp_ofs++;
		/*
		 * If we have reached the end of the bitmap, the directory is
		 * empty.
		 */
		if (((bmp_pos + bmp_ofs) >> 3) >= bmp_size)
			goto unm_done;
		ia_pos = (bmp_pos + bmp_ofs) << ia_ni->block_size_shift;
		/*
		 * If we have reached the end of the bitmap block get the next
		 * page and unmap away the old one.
		 */
		if ((bmp_ofs >> 3) >= PAGE_SIZE) {
			ntfs_page_unmap(bmp_ni, bmp_upl, bmp_pl, FALSE);
			bmp_pos += PAGE_SIZE * 8;
			bmp_ofs = 0;
			goto get_next_bmp_page;
		}
	}
	ntfs_debug("Handling index allocation block 0x%llx.",
			(unsigned long long)bmp_pos + bmp_ofs);
	/* If the current index block is in the same buffer we reuse it. */
	if ((prev_ia_pos & ~PAGE_MASK_64) != (ia_pos & ~PAGE_MASK_64)) {
		prev_ia_pos = ia_pos;
		if (ia_upl)
			ntfs_page_unmap(ia_ni, ia_upl, ia_pl, FALSE);
		/* Map the page containing the index allocation block. */
		err = ntfs_page_map(ia_ni, ia_pos & ~PAGE_MASK_64, &ia_upl,
				&ia_pl, &kaddr, FALSE);
		if (err) {
			ntfs_error(vol->mp, "Failed to read directory index "
					"allocation page (error %d).", err);
			ia_upl = NULL;
			goto page_err;
		}
	}
	/* Get the current index allocation block inside the mapped page. */
	ia = (INDEX_ALLOCATION*)(kaddr + ((u32)ia_pos & PAGE_MASK &
			~(ia_ni->block_size - 1)));
	/* Bounds checks. */
	if ((u8*)ia < kaddr || (u8*)ia > kaddr + PAGE_SIZE) {
		ntfs_error(vol->mp, es, "Out of bounds check failed",
				(unsigned long long)dir_ni->mft_no);
		goto vol_err;
	}
	/* Catch multi sector transfer fixup errors. */
	if (!ntfs_is_indx_record(ia->magic)) {
		ntfs_error(vol->mp, "Multi sector transfer error detected in "
				"index record vcn 0x%llx%s%llx%s",
				(unsigned long long)ia_pos >>
				ia_ni->vcn_size_shift, es1,
				(unsigned long long)dir_ni->mft_no, es2);
		goto vol_err;
	}
	if (sle64_to_cpu(ia->index_block_vcn) != (ia_pos &
			~(s64)(ia_ni->block_size - 1)) >>
			ia_ni->vcn_size_shift) {
		ntfs_error(vol->mp, "Actual VCN (0x%llx) of index record is "
				"different from expected VCN (0x%llx)%s%llx%s",
				(unsigned long long)
				sle64_to_cpu(ia->index_block_vcn),
				(unsigned long long)ia_pos >>
				ia_ni->vcn_size_shift, es1,
				(unsigned long long)dir_ni->mft_no, es2);
		goto vol_err;
	}
	if (offsetof(INDEX_BLOCK, index) +
			le32_to_cpu(ia->index.allocated_size) !=
			ia_ni->block_size) {
		ntfs_error(vol->mp, "Index buffer (VCN 0x%llx) has a size "
				"(%u) differing from the directory specified "
				"size (%u)%s%llx%s", (unsigned long long)
				(unsigned long long)
				sle64_to_cpu(ia->index_block_vcn),
				(unsigned)(offsetof(INDEX_BLOCK, index) +
				le32_to_cpu(ia->index.allocated_size)),
				(unsigned)ia_ni->block_size, es1,
				(unsigned long long)dir_ni->mft_no, es2);
		goto vol_err;
	}
	index_end = (u8*)ia + ia_ni->block_size;
	if (index_end > kaddr + PAGE_SIZE) {
		ntfs_error(vol->mp, "Index buffer (VCN 0x%llx) of directory "
				"inode 0x%llx crosses page boundary.  This "
				"cannot happen and points either to memory "
				"corruption or to a driver bug.",
				(unsigned long long)
				sle64_to_cpu(ia->index_block_vcn),
				(unsigned long long)dir_ni->mft_no);
		goto vol_err;
	}
	index_end = (u8*)&ia->index + le32_to_cpu(ia->index.index_length);
	if (index_end > (u8*)ia + ia_ni->block_size) {
		ntfs_error(vol->mp, "Size of index block (VCN 0x%llx) "
				"exceeds maximum size%s%llx%s",
				(unsigned long long)
				sle64_to_cpu(ia->index_block_vcn), es1,
				(unsigned long long)dir_ni->mft_no, es2);
		goto vol_err;
	}
	/* The first index entry. */
	ie = (INDEX_ENTRY*)((u8*)&ia->index +
			le32_to_cpu(ia->index.entries_offset));
	/* Bounds checks. */
	if ((u8*)ie < (u8*)&ia->index ||
			(u8*)ie + sizeof(INDEX_ENTRY_HEADER) > index_end ||
			(u8*)ie + le16_to_cpu(ie->length) > index_end)
		goto dir_err;
	/*
	 * If this is the end node, it is not a filename so we continue to the
	 * next index block.
	 */
	if (ie->flags & INDEX_ENTRY_END)
		goto find_next_index_buffer;
	/*
	 * This is not the end node, i.e. it is a filename and thus the
	 * directory is not empty.
	 */
	err = ENOTEMPTY;
unm_done:
	if (ia_upl)
		ntfs_page_unmap(ia_ni, ia_upl, ia_pl, FALSE);
	ntfs_page_unmap(bmp_ni, bmp_upl, bmp_pl, FALSE);
done:
	if (bmp_ni) {
		lck_rw_unlock_shared(&bmp_ni->lock);
		(void)vnode_put(bmp_ni->vn);
	}
	lck_rw_unlock_shared(&ia_ni->lock);
	(void)vnode_put(ia_ni->vn);
	ntfs_debug("Done (directory is%s empty).", !err ? "" : " not");
	return err;
dir_err:
	ntfs_error(vol->mp, "Corrupt directory inode 0x%llx.  Run chkdsk.",
			(unsigned long long)dir_ni->mft_no);
	NVolSetErrors(vol);
	/*
	 * If @ia_upl is not NULL we got here from the index allocation related
	 * code paths and if it is NULL we got here from the index root related
	 * code paths.
	 */
	if (ia_upl)
		goto page_err;
	err = EIO;
put_err:
	ntfs_attr_search_ctx_put(ctx);
unm_err:
	ntfs_mft_record_unmap(dir_ni);
err:
	if (bmp_ni) {
		lck_rw_unlock_shared(&bmp_ni->lock);
		(void)vnode_put(bmp_ni->vn);
	}
	lck_rw_unlock_shared(&ia_ni->lock);
	(void)vnode_put(ia_ni->vn);
	return err;
vol_err:
	NVolSetErrors(vol);
page_err:
	if (!err)
		err = EIO;
	if (ia_upl)
		ntfs_page_unmap(ia_ni, ia_upl, ia_pl, FALSE);
	if (bmp_upl)
		ntfs_page_unmap(bmp_ni, bmp_upl, bmp_pl, FALSE);
	goto err;
}

/**
 * ntfs_dir_entry_delete - delete a directory index entry
 * @dir_ni:	directory ntfs inode from which to delete the index entry
 * @ni:		base ntfs inode which the filename @fn links to
 * @fn:		filename attribute describing index entry to delete
 * @fn_len:	size of filename attribute in bytes
 *
 * Find the directory index entry corresponding to the filename attribute @fn
 * of size @fn_len bytes in the directory index of the directory ntfs inode
 * @dir_ni.
 *
 * Assuming the filename is present in the directory index, delete it from the
 * index.  @ni is the inode which the filename @fn links to.
 *
 * Return 0 on success and errno on error.
 *
 * Locking: Caller must hold both @dir_ni->lock and @ni->lock for writing.
 */
errno_t ntfs_dir_entry_delete(ntfs_inode *dir_ni, ntfs_inode *ni,
		const FILENAME_ATTR *fn, const u32 fn_len)
{
	ntfs_volume *vol = ni->vol;
	ntfs_inode *ia_ni;
	ntfs_index_context *ictx;
	INDEX_ENTRY *ie;
	int err;
	FILENAME_TYPE_FLAGS fn_type;

	ntfs_debug("Entering for mft_no 0x%llx, parent directory mft_no "
			"0x%llx.", (unsigned long long)ni->mft_no,
			(unsigned long long)dir_ni->mft_no);
	if (!S_ISDIR(dir_ni->mode))
		panic("%s(): !S_ISDIR(dir_ni->mode\n", __FUNCTION__);
	/*
	 * Verify that the mft reference of the parent directory specified in
	 * the filename to be removed matches the mft reference of the parent
	 * directory inode.
	 */
	if (fn->parent_directory != MK_LE_MREF(dir_ni->mft_no,
			dir_ni->seq_no)) {
		ntfs_error(vol->mp, "The reference of the parent directory "
				"specified in the filename to be removed does "
				"not match the reference of the parent "
				"directory inode.  Volume is corrupt.  Run "
				"chkdsk.");
		NVolSetErrors(vol);
		return EIO;
	}
	/*
	 * We are now ok to go ahead and delete the directory index entry.
	 *
	 * Get the index allocation inode.
	 */
	err = ntfs_index_inode_get(dir_ni, I30, 4, FALSE, &ia_ni);
	if (err) {
		ntfs_error(vol->mp, "Failed to get index vnode (error %d).",
				err);
		return EIO;
	}
	/* Need exclusive access to the index throughout. */
	lck_rw_lock_exclusive(&ia_ni->lock);
	ictx = ntfs_index_ctx_get(ia_ni);
	if (!ictx) {
		ntfs_error(vol->mp, "Not enough memory to allocate index "
				"context.");
		err = ENOMEM;
		goto err;
	}
restart:
	/* Get the index entry matching the filename @fn. */
	err = ntfs_index_lookup(fn, fn_len, &ictx);
	if (err) {
		if (err == ENOENT) {
			ntfs_error(vol->mp, "Failed to delete directory index "
					"entry of mft_no 0x%llx because the "
					"filename was not found in its parent "
					"directory index.  Directory 0x%llx "
					"is corrupt.  Run chkdsk.",
					(unsigned long long)ni->mft_no,
					(unsigned long long)dir_ni->mft_no);
			NVolSetErrors(vol);
		} else
			ntfs_error(vol->mp, "Failed to delete directory index "
					"entry of mft_no 0x%llx because "
					"looking up the filename in its "
					"parent directory 0x%llx failed "
					"(error %d).",
					(unsigned long long)ni->mft_no,
					(unsigned long long)dir_ni->mft_no,
					err);
		goto put_err;
	}
	ie = ictx->entry;
	/*
	 * Verify that the mft reference of the parent directory specified in
	 * the filename to be removed matches the mft reference of the parent
	 * directory specified in the found index entry.
	 */
	if (fn->parent_directory != ie->key.filename.parent_directory) {
		ntfs_error(vol->mp, "The reference of the parent directory "
				"(0x%llx) specified in the filename to be "
				"removed does not match the reference of the "
				"parent directory (0x%llx) specified in the "
				"matching directory index entry.  Volume is "
				"corrupt.  Run chkdsk.", (unsigned long long)
				le64_to_cpu(fn->parent_directory),
				(unsigned long long)le64_to_cpu(
				ie->key.filename.parent_directory));
		NVolSetErrors(vol);
		err = EIO;
		goto put_err;
	}
	/*
	 * Verify that the mft reference of the inode to which the filename to
	 * be removed belongs matches the mft reference of the inode pointed to
	 * by the found index entry.
	 */
	if (MK_LE_MREF(ni->mft_no, ni->seq_no) != ie->indexed_file) {
		ntfs_error(vol->mp, "The reference of the inode (0x%llx) to "
				"which the filename to be removed belongs "
				"does not match the reference of the inode "
				"(0x%llx) specified in the matching directory "
				"index entry.  Volume is corrupt.  Run "
				"chkdsk.", (unsigned long long)
				MK_MREF(ni->mft_no, ni->seq_no),
				(unsigned long long)
				le64_to_cpu(ie->indexed_file));
		NVolSetErrors(vol);
		err = EIO;
		goto put_err;
	}
	fn_type = ie->key.filename.filename_type;
	/* We now have the directory index entry, delete it. */
	err = ntfs_index_entry_delete(ictx);
	if (!err) {
		ntfs_index_ctx_put(ictx);
		/* Update the mtime and ctime in the parent directory inode. */
		dir_ni->last_mft_change_time = dir_ni->last_data_change_time =
				ntfs_utc_current_time();
		NInoSetDirtyTimes(dir_ni);
		lck_rw_unlock_exclusive(&ia_ni->lock);
		(void)vnode_put(ia_ni->vn);
		ntfs_debug("Done.");
		return 0;
	}
	/*
	 * If the tree got rearranged in some unpredictable way and we
	 * chickened out of working through it, we now reinitialize the index
	 * context (as it is now invalid) and then redo the lookup and delete.
	 *
	 * Note we use a negative -EAGAIN to distinguish from a potential real
	 * EAGAIN error.
	 */
	if (err == -EAGAIN) {
		ntfs_debug("Restarting delete as tree was rearranged.");
		ntfs_index_ctx_reinit(ictx, ia_ni);
		goto restart;
	}
	/*
	 * Failed to delete the directory index entry.
	 *
	 * If the filename @fn is in the POSIX namespace but the directory
	 * index entry is in the WIN32 namespace, convert the directory index
	 * entry to the POSIX namespace.  See comments above @restart_name
	 * label in ntfs_vnops.c::ntfs_vnop_remove() for an explanation of when
	 * this happens and why we need to do this.
	 */
	if (fn_type == FILENAME_WIN32 && fn->filename_type == FILENAME_POSIX) {
		errno_t err2;

		ntfs_debug("Switching namespace of directory index entry from "
				"WIN32 to POSIX to match the namespace of the "
				"corresponding filename attribute.");
		/*
		 * The old index context is now invalid, so need to redo the
		 * index lookup.
		 */
		ntfs_index_ctx_reinit(ictx, ia_ni);
		err2 = ntfs_index_lookup(fn, fn_len, &ictx);
		if (err2) {
			ntfs_error(vol->mp, "Failed to switch namespace of "
					"directory index entry of inode "
					"0x%llx from WIN32 to POSIX because "
					"re-looking up the filename in its "
					"parent directory inode 0x%llx failed "
					"(error %d).  Leaving inconsistent "
					"metadata.  Run chkdsk.",
					(unsigned long long)ni->mft_no,
					(unsigned long long)dir_ni->mft_no,
					err2);
			NVolSetErrors(vol);
			goto put_err;
		}
		ictx->entry->key.filename.filename_type = FILENAME_POSIX;
		ntfs_index_entry_mark_dirty(ictx);
		dir_ni->last_mft_change_time = dir_ni->last_data_change_time =
				ntfs_utc_current_time();
		NInoSetDirtyTimes(dir_ni);
	}
put_err:
	ntfs_index_ctx_put(ictx);
err:
	lck_rw_unlock_exclusive(&ia_ni->lock);
	(void)vnode_put(ia_ni->vn);
	ntfs_debug("Failed (error %d).", err);
	return err;
}

/**
 * ntfs_dir_entry_add - add a directory index entry
 * @dir_ni:	directory ntfs inode to which to add the index entry
 * @fn:		filename attribute describing index entry to add
 * @fn_len:	size of filename attribute in bytes
 * @mref:	mft reference of the inode the filename @fn belongs to
 *
 * Find the directory index entry corresponding to the filename attribute @fn
 * of size @fn_len bytes in the directory index of the directory ntfs inode
 * @dir_ni.
 *
 * Assuming the filename is not already present in the directory index, add it
 * to the index and point the inserted index entry at the mft reference @mref
 * which is the little endian mft reference of the inode to which the filename
 * attribute @fn belongs.
 *
 * If the filename is already present in the directory index, abort and return
 * the error code EEXIST.
 *
 * Return 0 on success and errno on error.
 *
 * Locking: Caller must hold @dir_ni->lock for writing.
 */
errno_t ntfs_dir_entry_add(ntfs_inode *dir_ni, const FILENAME_ATTR *fn,
		const u32 fn_len, const leMFT_REF mref)
{
	const leMFT_REF tmp_mref = mref;
	ntfs_inode *ia_ni;
	ntfs_index_context *ictx;
	errno_t err;

	ntfs_debug("Entering for mft_no 0x%llx, parent directory mft_no "
			"0x%llx.", (unsigned long long)MREF_LE(tmp_mref),
			(unsigned long long)dir_ni->mft_no);
	if (!S_ISDIR(dir_ni->mode))
		panic("%s(): !S_ISDIR(dir_ni->mode\n", __FUNCTION__);
	/* Get the index allocation inode. */
	err = ntfs_index_inode_get(dir_ni, I30, 4, FALSE, &ia_ni);
	if (err) {
		ntfs_error(dir_ni->vol->mp, "Failed to get index vnode (error "
				"%d).", err);
		return err;
	}
	/* Need exclusive access to the index throughout. */
	lck_rw_lock_exclusive(&ia_ni->lock);
	ictx = ntfs_index_ctx_get(ia_ni);
	if (!ictx) {
		ntfs_error(dir_ni->vol->mp, "Not enough memory to allocate "
				"index context.");
		err = ENOMEM;
		goto err;
	}
	/*
	 * Get the index entry matching the filename @fn and if not present get
	 * the position at which the new index entry needs to be inserted.
	 */
	err = ntfs_index_lookup(fn, fn_len, &ictx);
	if (err != ENOENT) {
		if (!err) {
			ntfs_debug("Failed (filename already present in "
					"directory index).");
			err = EEXIST;
		} else
			ntfs_error(dir_ni->vol->mp, "Failed to add directory "
					"index entry of mft_no 0x%llx to "
					"directory mft_no 0x%llx because "
					"looking up the filename in the "
					"directory index failed (error %d).",
					(unsigned long long)MREF_LE(tmp_mref),
					(unsigned long long)dir_ni->mft_no,
					err);
		ntfs_index_ctx_put(ictx);
		goto err;
	}
	/*
	 * Create a new directory index entry inserting it in front of the
	 * entry described by the index context.
	 */
	err = ntfs_index_entry_add(ictx, fn, fn_len, &tmp_mref, 0);
	ntfs_index_ctx_put(ictx);
	if (!err) {
		lck_rw_unlock_exclusive(&ia_ni->lock);
		(void)vnode_put(ia_ni->vn);
		/* Update the mtime and ctime of the parent directory inode. */
		dir_ni->last_mft_change_time = dir_ni->last_data_change_time =
				ntfs_utc_current_time();
		NInoSetDirtyTimes(dir_ni);
		ntfs_debug("Done.");
		return 0;
	}
err:
	lck_rw_unlock_exclusive(&ia_ni->lock);
	(void)vnode_put(ia_ni->vn);
	return err;
}
