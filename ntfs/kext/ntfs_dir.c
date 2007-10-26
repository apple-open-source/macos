/*
 * ntfs_dir.c - NTFS kernel directory operations.
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

#include <sys/buf.h>
#include <sys/param.h>
#include <sys/dirent.h>
#include <sys/errno.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/vnode.h>
/*
 * struct nameidata is defined in <sys/namei.h>, but it is private so put in a
 * forward declaration for now so that vnode_internal.h can compile.  All we
 * need vnode_internal.h for is the declaration of vnode_getparent() which is
 * exported in Unsupported.exports...
 */
// #include <sys/namei.h>
struct nameidata;
#include <sys/vnode_internal.h>

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
#include "ntfs_inode.h"
#include "ntfs_layout.h"
#include "ntfs_mft.h"
#include "ntfs_page.h"
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
 * Locking:  - Caller must hold @dir_ni->lock on the directory shared.
 *	     - Each buffer underlying page in the index allocation must be
 *	       locked whilst being accessed otherwise we may find a corrupt
 *	       buffer due to it being written at the moment, during which the
 *	       mst protection fixups are applied before writing out and then
 *	       removed again after the write is complete after which the buffer
 *	       underlying page is unlocked again.  TODO: As we do not support
 *	       writing yet, we do not lock the buffer underlying pages yet.
 *
 * TODO: From Mark's review comments: pull the iteration code into a separate
 * function and call it both for the index root and index allocation iteration.
 * See new ntfs driver for Linux index lookup function...
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
	err = ntfs_attr_lookup(AT_INDEX_ROOT, I30, 4, CASE_SENSITIVE, 0, NULL,
			0, ctx);
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
	 * ntfs_inode_read()).
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
				(u8*)ie + le16_to_cpu(ie->key_length) >
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
				ie->key.filename.filename_length,
				CASE_SENSITIVE, vol->upcase,
				vol->upcase_len)) {
found_it:
			/*
			 * We have a perfect match, so we do not need to care
			 * about having matched imperfectly before, so we can
			 * free name and set *res_name to NULL.
			 *
			 * However, if the perfect match is a short filename,
			 * we need to signal this through *res_name, so that
			 * ntfs_lookup() can deal with the name cache
			 * effectively.
			 *
			 * As an optimization we just reuse an existing
			 * allocation of *res_name.
			 */
			if (ie->key.filename.filename_type == FILENAME_DOS) {
				if (!name) {
					name = OSMalloc(sizeof(
							ntfs_dir_lookup_name),
							ntfs_malloc_tag);
					if (!name) {
						err = ENOMEM;
						goto put_err;
					}
				}
				name->mref = le64_to_cpu(ie->indexed_file);
				name->type = FILENAME_DOS;
				name->len = 0;
				*res_name = name;
			} else {
				if (name)
					OSFree(name, sizeof(
							ntfs_dir_lookup_name),
							ntfs_malloc_tag);
				*res_name = NULL;
			}
			*res_mref = le64_to_cpu(ie->indexed_file);
			ntfs_attr_search_ctx_put(ctx);
			ntfs_mft_record_unmap(dir_ni);
			return 0;
		}
		/*
		 * For a case insensitive mount, we also perform a case
		 * insensitive comparison (provided the filename is not in the
		 * POSIX namespace).  If the comparison matches, and the name
		 * is in the WIN32 namespace, we cache the filename in
		 * *res_name so that the caller, ntfs_vnop_lookup(), can work
		 * on it.  If the comparison matches, and the name is in the
		 * DOS namespace, we only cache the mft reference and the
		 * filename type (we set the name length to zero for
		 * simplicity).
		 *
		 * TODO/FIXME: Need to change this so when there is no matching
		 * WIN32/DOS name we still match the first(?) matching POSIX
		 * name and return that instead of the WIN32/DOS name...
		 */
		if (!NVolCaseSensitive(vol) &&
				ie->key.filename.filename_type &&
				ntfs_are_names_equal(uname, uname_len,
				(ntfschar*)&ie->key.filename.filename,
				ie->key.filename.filename_length,
				IGNORE_CASE, vol->upcase, vol->upcase_len)) {
			u8 type = ie->key.filename.filename_type;
			u8 len = ie->key.filename.filename_length;

			/* Only one case insensitive matching name allowed. */
			if (name) {
				ntfs_error(mp, "Found already allocated name "
						"in phase 1.  Please run "
						"chkdsk and if that does not "
						"find any errors please "
						"report you saw this message "
						"to %s.", ntfs_dev_email);
				goto dir_err;
			}
			name = OSMalloc(sizeof(ntfs_dir_lookup_name),
					ntfs_malloc_tag);
			if (!name) {
				err = ENOMEM;
				goto put_err;
			}
			name->mref = le64_to_cpu(ie->indexed_file);
			name->type = type;
			if (type != FILENAME_DOS) {
				name->len = len;
				memcpy(name->name, ie->key.filename.filename,
						len * sizeof(ntfschar));
			} else
				name->len = 0;
			*res_name = name;
		}
		/*
		 * Not a perfect match, need to do full blown collation so we
		 * know which way in the B+tree we have to go.
		 */
		rc = ntfs_collate_names(uname, uname_len,
				(ntfschar*)&ie->key.filename.filename,
				ie->key.filename.filename_length, 1,
				IGNORE_CASE, vol->upcase, vol->upcase_len);
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
				ie->key.filename.filename_length, 1,
				CASE_SENSITIVE, vol->upcase, vol->upcase_len);
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
	if (!NInoIndexAllocPresent(dir_ni)) {
		ntfs_error(mp, "No index allocation attribute but index entry "
				"requires one.  Directory inode 0x%llx is "
				"corrupt or driver bug.",
				(unsigned long long)dir_ni->mft_no);
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
	/* Get the index allocation inode. */
	err = ntfs_index_inode_get(dir_ni, I30, 4, &ia_ni);
	if (err) {
		ntfs_error(mp, "Failed to get index vnode (error %d).", err);
		goto err;
	}
	ia_vn = ia_ni->vn;
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
				(u8*)ie + le16_to_cpu(ie->key_length) >
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
				ie->key.filename.filename_length,
				CASE_SENSITIVE, vol->upcase,
				vol->upcase_len)) {
found_it2:
			/*
			 * We have a perfect match, so we do not need to care
			 * about having matched imperfectly before, so we can
			 * free name and set *res_name to NULL.
			 *
			 * However, if the perfect match is a short filename,
			 * we need to signal this through *res_name, so that
			 * ntfs_lookup() can deal with the name cache
			 * effectively.
			 *
			 * As an optimization we just reuse an existing
			 * allocation of *res_name.
			 */
			if (ie->key.filename.filename_type == FILENAME_DOS) {
				if (!name) {
					name = OSMalloc(sizeof(
							ntfs_dir_lookup_name),
							ntfs_malloc_tag);
					if (!name) {
						err = ENOMEM;
						goto page_err;
					}
				}
				name->mref = le64_to_cpu(ie->indexed_file);
				name->type = FILENAME_DOS;
				name->len = 0;
				*res_name = name;
			} else {
				if (name)
					OSFree(name, sizeof(
							ntfs_dir_lookup_name),
							ntfs_malloc_tag);
				*res_name = NULL;
			}
			*res_mref = le64_to_cpu(ie->indexed_file);
			ntfs_page_unmap(ia_ni, upl, pl, FALSE);
			(void)vnode_put(ia_vn);
			return 0;
		}
		/*
		 * For a case insensitive mount, we also perform a case
		 * insensitive comparison (provided the filename is not in the
		 * POSIX namespace).  If the comparison matches, and the name
		 * is in the WIN32 namespace, we cache the filename in
		 * *res_name so that the caller, ntfs_vnop_lookup(), can work
		 * on it.  If the comparison matches, and the name is in the
		 * DOS namespace, we only cache the mft reference and the
		 * filename type (we set the name length to zero for
		 * simplicity).
		 *
		 * TODO/FIXME: Need to change this so when there is no matching
		 * WIN32/DOS name we still match the first(?) matching POSIX
		 * name and return that instead of the WIN32/DOS name...
		 */
		if (!NVolCaseSensitive(vol) &&
				ie->key.filename.filename_type &&
				ntfs_are_names_equal(uname, uname_len,
				(ntfschar*)&ie->key.filename.filename,
				ie->key.filename.filename_length,
				IGNORE_CASE, vol->upcase, vol->upcase_len)) {
			u8 type = ie->key.filename.filename_type;
			u8 len = ie->key.filename.filename_length;

			/* Only one case insensitive matching name allowed. */
			if (name) {
				ntfs_error(mp, "Found already allocated name "
						"in phase 2.  Please run "
						"chkdsk and if that does not "
						"find any errors please "
						"report you saw this message "
						"to %s.", ntfs_dev_email);
				goto page_err;
			}
			name = OSMalloc(sizeof(ntfs_dir_lookup_name),
					ntfs_malloc_tag);
			if (!name) {
				err = ENOMEM;
				goto page_err;
			}
			name->mref = le64_to_cpu(ie->indexed_file);
			name->type = type;
			if (type != FILENAME_DOS) {
				name->len = len;
				memcpy(name->name, ie->key.filename.filename,
						len * sizeof(ntfschar));
			} else
				name->len = 0;
			*res_name = name;
		}
		/*
		 * Not a perfect match, need to do full blown collation so we
		 * know which way in the B+tree we have to go.
		 */
		rc = ntfs_collate_names(uname, uname_len,
				(ntfschar*)&ie->key.filename.filename,
				ie->key.filename.filename_length, 1,
				IGNORE_CASE, vol->upcase, vol->upcase_len);
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
				ie->key.filename.filename_length, 1,
				CASE_SENSITIVE, vol->upcase, vol->upcase_len);
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
	(void)vnode_put(ia_vn);
not_found:
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
		OSFree(name, sizeof(ntfs_dir_lookup_name), ntfs_malloc_tag);
	if (ia_vn)
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
	if (mref == FILE_root) {
		ntfs_debug("Skipping root directory self reference entry.");
		return 0;
	}
	if (mref < FILE_first_user && !NVolShowSystemFiles(vol)) {
		ntfs_debug("Skipping core NTFS system file.");
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
	/* We only support normal files and directories at present. */
	if (ie->key.filename.file_attributes &
			FILE_ATTR_DUP_FILENAME_INDEX_PRESENT)
		de->d_type = DT_DIR;
	else
		de->d_type = DT_REG;
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
		memset((u8*)de + res_size, '\0', padding);
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
 * The name (d_name) must be at least MAXNAMELEN + 1 bytes long including the
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
 * storage hardware that is NTFS formatted and accessed by OSX.  Further, up to
 * and including Windows XP, Windows itself limits the maximum number of inodes
 * to 2^32.
 *
 * When the current position (uio_offset()) is zero, we parse the index root
 * entries and then the entries in each index allocation block that is marked
 * as in use in the index bitmap in linear on-disk sequence, i.e. we do not
 * follow the B+tree at all.
 *
 * This means we return the names in random order which does not matter for
 * VNOP_READDIR() but OTOH results in a much more efficient and faster
 * VNOP_READDIR().
 *
 * The current position (uio_offset()) refers to the next block of entries to
 * be returned.  The offset can only be set to a value previously returned by
 * ntfs_vnop_readdir() or zero.  This offset does not have to match the number
 * of bytes returned (in uio_resid()).
 *
 * Note that uio_offset() is of type off_t which is 64-bit in OSX whilst
 * uio_resid() is only 32-bit thus uio_offset() can store the byte offset into
 * the directory at which to continue which makes resuming the directory
 * listing particularly simple.
 *
 * If @eofflag is not NULL, set *eofflag to 0 if we have not reached the end of
 * the directory yet and set it to 1 if we have reached the end of the
 * directory, i.e. @uio either contains nothing or it contains the last entry
 * in the directory.
 *
 * If @numdirent is not NULL, set *@numdirent to the number of directory
 * entries returned in the buffer described by @uio.
 *
 * Locking:  - Caller must hold @dir_ni->lock on the directory shared.
 *	     - Each buffer underlying page in the index allocation must be
 *	       locked whilst being accessed otherwise we may find a corrupt
 *	       buffer due to it being written at the moment, during which the
 *	       mst protection fixups are applied before writing out and then
 *	       removed again after the write is complete after which the buffer
 *	       underlying page is unlocked again.  TODO: As we do not support
 *	       writing yet, we do not lock the buffer underlying pages yet.
 */
errno_t ntfs_readdir(ntfs_inode *dir_ni, uio_t uio, int *eofflag,
		int *numdirent)
{
	s64 ia_size, bmp_size, bmp_pos, ia_ofs;
	off_t ofs, prev_ofs;
	ntfs_volume *vol = dir_ni->vol;
	mount_t mp = vol->mp;
	struct dirent *de;
	MFT_RECORD *m;
	ntfs_attr_search_ctx *ctx = NULL;
	INDEX_ROOT *ir;
	u8 *index_end;
	INDEX_ENTRY *ie;
	ntfs_inode *bmp_ni, *ia_ni;
	vnode_t bmp_vn, ia_vn;
	upl_t bmp_upl, ia_upl = NULL;
	upl_page_info_array_t bmp_pl, ia_pl;
	u8 *bmp, *kaddr = NULL;
	INDEX_ALLOCATION *ia;
	int eof, entries, err, start_ofs, bmp_ofs;
	/*
	 * This is quite big to go on the stack but only half the size of the
	 * buffers placed on the stack in ntfs_vnop_lookup() so if they are ok
	 * so should this be.
	 */
	u8 de_buf[sizeof(struct dirent) + 4];

	de = (struct dirent*)&de_buf;
	ia_vn = bmp_vn = NULL;
	err = entries = eof = 0;
	ofs = uio_offset(uio);
	ntfs_debug("Entering for directory inode 0x%llx, offset 0x%llx, count "
			"0x%llx.",
			(unsigned long long)dir_ni->mft_no,
			(unsigned long long)ofs,
			(unsigned long long)uio_resid(uio));
	/*
	 * Sanity check the uio data.  The absolute minimum buffer size
	 * required is the number of bytes taken by the entries in the dirent
	 * structure up to the beginning of the name plus the minimum length
	 * for a filename of one byte plus we need to align each dirent record
	 * to a multiple of four bytes thus effectovely the minimum name length
	 * is four and not one.
	 */
	if (uio_resid(uio) < offsetof(struct dirent, d_name) + 4) {
		err = EINVAL;
		goto err;
	}
	lck_spin_lock(&dir_ni->size_lock);
	ia_size = dir_ni->data_size;
	lck_spin_unlock(&dir_ni->size_lock);
	/* Are we at or beyond the end of the directory? */
	if (ofs >= ia_size + vol->mft_record_size)
		goto EOD;
	/* Are we jumping straight into the index allocation attribute? */
	if (ofs >= vol->mft_record_size)
		goto skip_index_root;
	/* Emulate "." and ".." for all directories. */
	while (ofs < 2) {
		if (sizeof(de->d_ino) < 8 &&
				dir_ni->mft_no & 0xffffffff00000000ULL)
			ntfs_warning(mp, "Skipping emulated dirent for "
					"\".%s\" because its inode number "
					"0x%llx does not fit in 32-bits.",
					ofs ? "." : "",
					(unsigned long long)dir_ni->mft_no);
		else {
			*(u32*)de->d_name = 0;
			de->d_name[0] = '.';
			if (ofs) {
				ino64_t parent_ino;
				vnode_t parent_vn;

				/*
				 * If there is no parent vnode we cannot
				 * synthesize the ".." dirent unless we are
				 * working on the root directory of the volume
				 * in which case we make ".." be equal to ".",
				 * i.e. to the root directory of the volume.
				 */
				parent_vn = vnode_getparent(dir_ni->vn);
				if (parent_vn) {
					parent_ino = NTFS_I(parent_vn)->mft_no;
					(void)vnode_put(parent_vn);
				}
				if (parent_vn)
					de->d_ino = parent_ino;
				else if (dir_ni->mft_no == FILE_root)
					de->d_ino = FILE_root;
				else {
					ntfs_warning(mp, "Skipping emulated "
							"dirent for \"..\" "
							"because its inode "
							"number is not "
							"available.");
					goto skip_dotdot;
				}
				de->d_namlen = 2;
				de->d_name[1] = '.';
			} else {
				de->d_ino = dir_ni->mft_no;
				de->d_namlen = 1;
			}
			/*
			 * The name is one byte long but we need to align the
			 * entry record to a multiple of four bytes, thus add
			 * four instead of one to the name offset.
			 */
			de->d_reclen = offsetof(struct dirent, d_name) + 4;
			de->d_type = DT_DIR;
			ntfs_debug("Returning emulated \"%s\" dirent with "
					"d_ino 0x%llx, d_reclen 0x%x, d_type "
					"DT_DIR, d_namlen %d.", de->d_name,
					(unsigned long long)de->d_ino,
					(unsigned)de->d_reclen,
					(unsigned)de->d_namlen);
			err = uiomove((caddr_t)de, de->d_reclen, uio);
			if (err) {
				ntfs_error(mp, "uiomove() failed for emulated "
						"entry (error %d).", err);
				goto err;
			}
			entries++;
		}
skip_dotdot:
		ofs++;
		if (uio_resid(uio) < offsetof(struct dirent, d_name) + 4)
			goto done;
	}
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
	err = ntfs_attr_lookup(AT_INDEX_ROOT, I30, 4, CASE_SENSITIVE, 0, NULL,
			0, ctx);
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
	 * Get the offset into the index root attribute at which to begin
	 * returning directory entries.
	 */
	start_ofs = (int)ofs;
	/*
	 * Get to the index root value (it has been verified in
	 * ntfs_inode_read()).
	 */
	ir = (INDEX_ROOT*)((u8*)ctx->a + le16_to_cpu(ctx->a->value_offset));
	index_end = (u8*)&ir->index + le32_to_cpu(ir->index.index_length);
	/* The first index entry. */
	ie = (INDEX_ENTRY*)((u8*)&ir->index +
			le32_to_cpu(ir->index.entries_offset));
	/*
	 * Loop until we exceed valid memory (corruption case), we reach the
	 * last entry, we run out of space in the destination @uio, or
	 * uiomove() returns an error.
	 */
	for (;; ie = (INDEX_ENTRY*)((u8*)ie + le16_to_cpu(ie->length))) {
		/* Bounds checks. */
		if ((u8*)ie < (u8*)&ir->index || (u8*)ie +
				sizeof(INDEX_ENTRY_HEADER) > index_end ||
				(u8*)ie + le16_to_cpu(ie->key_length) >
				index_end)
			goto dir_err;
		/* The last entry cannot contain a name. */
		if (ie->flags & INDEX_ENTRY_END)
			break;
		ofs = (int)((u8*)ie - (u8*)ir);
		/* Skip index root entry if continuing previous readdir. */
		if (ofs < start_ofs)
			continue;
		/* Submit the directory entry to our helper function. */
		err = ntfs_do_dirent(vol, ie, de, uio, &entries);
		if (err) {
			/*
			 * A negative error code means the destination @uio
			 * does not have enough space for the next directory
			 * entry.
			 */
			if (err < 0)
				goto done;
			/* Positive error code; uiomove() returned error. */
			ntfs_error(mp, "uiomove() failed for index root entry "
					"(error %d).", err);
			goto put_err;
		}
	}
	/*
	 * We are done with the index root and the mft record.  Release them,
	 * otherwise we deadlock with ntfs_page_map().
	 */
	ntfs_attr_search_ctx_put(ctx);
	ntfs_mft_record_unmap(dir_ni);
	/* If there is no index allocation attribute we are finished. */
	if (!NInoIndexAllocPresent(dir_ni))
		goto EOD;
	/* Advance the offset to the beginning of the index allocation. */
	ofs = vol->mft_record_size;
skip_index_root:
	/* Get both the index bitmap and index allocation inodes. */
	err = ntfs_attr_inode_get(dir_ni, AT_BITMAP, I30, 4, FALSE, &bmp_ni);
	if (err) {
		ntfs_error(mp, "Failed to get index bitmap vnode (error %d).",
				err);
		goto err;
	}
	bmp_vn = bmp_ni->vn;
	err = ntfs_index_inode_get(dir_ni, I30, 4, &ia_ni);
	if (err) {
		ntfs_error(mp, "Failed to get index allocation vnode (error "
				"%d).", err);
		goto bmp_err;
	}
	ia_vn = ia_ni->vn;
	lck_spin_lock(&bmp_ni->size_lock);
	bmp_size = bmp_ni->data_size;
	lck_spin_unlock(&bmp_ni->size_lock);
	/*
	 * Get the starting bitmap bit position corresponding to the index
	 * allocation block in which to begin returning directory entries and
	 * sanity check it.
	 */
	bmp_pos = (ofs - vol->mft_record_size) >> ia_ni->block_size_shift;
	if (bmp_pos >> 3 >= bmp_size) {
		ntfs_error(mp, "Current index allocation position exceeds "
				"index bitmap size.  Run chkdsk.");
		goto ia_err;
	}
	/*
	 * Initialize the previous offset into the directory to an invalid
	 * value.
	 */
	prev_ofs = -1;
	/* Get the starting bit relative to the current bitmap block. */
	bmp_ofs = (int)bmp_pos & ((PAGE_SIZE * 8) - 1);
	bmp_pos &= ~(s64)((PAGE_SIZE * 8) - 1);
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
		ntfs_error(mp, "Failed to read directory index bitmap buffer "
				"(error %d).", err);
		bmp_upl = NULL;
		goto page_err;
	}
	/* Find the next index block which is marked in use. */
	while (!(bmp[bmp_ofs >> 3] & (1 << (bmp_ofs & 7)))) {
find_next_index_buffer:
		bmp_ofs++;
		/* If we have reached the end of the bitmap, we are done. */
		if (((bmp_pos + bmp_ofs) >> 3) >= bmp_size)
			goto page_EOD;
		/*
		 * Set the offset into the directory to the start of the index
		 * allocation block corresponding to the next bitmap bit adding
		 * the mft record size to maintain our offset due to the index
		 * root attribute.
		 */
		ofs = ((bmp_pos + bmp_ofs) << ia_ni->block_size_shift) +
				vol->mft_record_size;
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
	ia_ofs = ofs - vol->mft_record_size;
	/* If the current index block is in the same buffer we reuse it. */
	if ((prev_ofs & ~PAGE_MASK_64) != (ia_ofs & ~PAGE_MASK_64)) {
		prev_ofs = ia_ofs;
		if (ia_upl)
			ntfs_page_unmap(ia_ni, ia_upl, ia_pl, FALSE);
		/* Map the page containing the index allocation block. */
		err = ntfs_page_map(ia_ni, ia_ofs & ~PAGE_MASK_64, &ia_upl,
				&ia_pl, &kaddr, FALSE);
		if (err) {
			ntfs_error(mp, "Failed to read directory index "
					"allocation page (error %d).", err);
			ia_upl = NULL;
			goto page_err;
		}
	}
	/* Get the current index allocation block inside the mapped page. */
	ia = (INDEX_ALLOCATION*)(kaddr + ((u32)ia_ofs & PAGE_MASK &
			~(ia_ni->block_size - 1)));
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
				(unsigned long long)(ia_ofs &
				~(s64)(ia_ni->block_size - 1)) >>
				ia_ni->vcn_size_shift,
				(unsigned long long)dir_ni->mft_no);
		goto page_err;
	}
	if (sle64_to_cpu(ia->index_block_vcn) != (ia_ofs &
			~(s64)(ia_ni->block_size - 1)) >>
			ia_ni->vcn_size_shift) {
		ntfs_error(mp, "Actual VCN (0x%llx) of index block is "
				"different from expected VCN (0x%llx).  "
				"Directory inode 0x%llx is corrupt or driver "
				"bug.", (unsigned long long)
				sle64_to_cpu(ia->index_block_vcn),
				(unsigned long long)(ia_ofs &
				~(s64)(ia_ni->block_size - 1)) >>
				ia_ni->vcn_size_shift,
				(unsigned long long)dir_ni->mft_no);
		goto page_err;
	}
	if (offsetof(INDEX_BLOCK, index) +
			le32_to_cpu(ia->index.allocated_size) !=
			ia_ni->block_size) {
		ntfs_error(mp, "Index block (VCN 0x%llx) of directory inode "
				"0x%llx has a size (%u) differing from the "
				"directory specified size (%u).  Directory "
				"inode is corrupt or driver bug.",
				(unsigned long long)
				sle64_to_cpu(ia->index_block_vcn),
				(unsigned long long)dir_ni->mft_no, (unsigned)
				(offsetof(INDEX_BLOCK, index) +
				le32_to_cpu(ia->index.allocated_size)),
				(unsigned)ia_ni->block_size);
		goto page_err;
	}
	index_end = (u8*)ia + ia_ni->block_size;
	if (index_end > kaddr + PAGE_SIZE) {
		ntfs_error(mp, "Index block (VCN 0x%llx) of directory inode "
				"0x%llx crosses page boundary.  Impossible! "
				"Cannot access!  This is probably a bug in "
				"the driver.", (unsigned long long)
				sle64_to_cpu(ia->index_block_vcn),
				(unsigned long long)dir_ni->mft_no);
		goto page_err;
	}
	index_end = (u8*)&ia->index + le32_to_cpu(ia->index.index_length);
	if (index_end > (u8*)ia + ia_ni->block_size) {
		ntfs_error(mp, "Size of index block (VCN 0x%llx) of directory "
				"inode 0x%llx exceeds maximum size.",
				(unsigned long long)
				sle64_to_cpu(ia->index_block_vcn),
				(unsigned long long)dir_ni->mft_no);
		goto page_err;
	}
	/*
	 * Get the offset into this index allocation block at which to begin
	 * returning directory entries.
	 */
	start_ofs = (int)ia_ofs & (ia_ni->block_size - 1);
	/*
	 * Set @ia_ofs to the uio_offset() of the beginning of this index
	 * allocation block.
	 */
	ia_ofs = (ia_ofs & ~(s64)(ia_ni->block_size - 1)) +
			vol->mft_record_size;
	/* The first index entry. */
	ie = (INDEX_ENTRY*)((u8*)&ia->index +
			le32_to_cpu(ia->index.entries_offset));
	/*
	 * Loop until we exceed valid memory (corruption case), we reach the
	 * last entry, we run out of space in the destination @uio, or
	 * uiomove() returns an error.
	 */
	for (;; ie = (INDEX_ENTRY*)((u8*)ie + le16_to_cpu(ie->length))) {
		int idx_ofs;

		/* Bounds checks. */
		if ((u8*)ie < (u8*)&ia->index || (u8*)ie +
				sizeof(INDEX_ENTRY_HEADER) > index_end ||
				(u8*)ie + le16_to_cpu(ie->key_length) >
				index_end)
			goto dir_err;
		/* The last entry cannot contain a name. */
		if (ie->flags & INDEX_ENTRY_END)
			goto find_next_index_buffer;
		idx_ofs = (int)((u8*)ie - (u8*)ia);
		ofs = ia_ofs + idx_ofs;
		/* Skip index block entry if continuing previous readdir. */
		if (idx_ofs < start_ofs)
			continue;
		/* Submit the directory entry to our helper function. */
		err = ntfs_do_dirent(vol, ie, de, uio, &entries);
		if (err) {
			/*
			 * A negative error code means the destination @uio
			 * does not have enough space for the next directory
			 * entry.
			 */
			if (err < 0)
				break;
			/* Positive error code; uiomove() returned error. */
			ntfs_error(mp, "uiomove() failed for index allocation "
					"entry (error %d).", err);
			goto page_err;
		}
	}
	/*
	 * @err is less than zero to indicate that the destination @uio does
	 * not have enough space for the next directory entry.
	 */
page_EOD:
	if (ia_upl)
		ntfs_page_unmap(ia_ni, ia_upl, ia_pl, FALSE);
	ntfs_page_unmap(bmp_ni, bmp_upl, bmp_pl, FALSE);
	(void)vnode_put(ia_vn);
	(void)vnode_put(bmp_vn);
	/*
	 * If @err is less than zero, we are not at the end of the directory
	 * but got here becuase the @uio does not have enough space for the
	 * next directory entry.
	 */
	if (err < 0)
		goto done;
EOD:
	/* We are finished, set offset to the end of the directory. */
	ofs = ia_size + vol->mft_record_size;
	eof = 1;
done:
	/*
	 * If @err is less than zero, we got here becuase the @uio does not
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
	ntfs_debug("%s (returned 0x%x entries, %s, now at offset 0x%llx).",
			err ? "Failed" : "Done", entries, eof ?
			"reached end of directory" : "more entries to follow",
			(unsigned long long)ofs);
	if (eofflag)
		*eofflag = eof;
	if (numdirent)
		*numdirent = entries;
	uio_setoffset(uio, ofs);
	return err;
dir_err:
	ntfs_error(mp, "Corrupt directory inode 0x%llx.  Run chkdsk.",
			(unsigned long long)dir_ni->mft_no);
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
	goto err;
page_err:
	if (!err)
		err = EIO;
	if (ia_upl)
		ntfs_page_unmap(ia_ni, ia_upl, ia_pl, FALSE);
	if (bmp_upl)
		ntfs_page_unmap(bmp_ni, bmp_upl, bmp_pl, FALSE);
ia_err:
	(void)vnode_put(ia_vn);
bmp_err:
	(void)vnode_put(bmp_vn);
	goto err;
}
