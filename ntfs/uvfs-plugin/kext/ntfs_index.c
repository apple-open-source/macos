/*
 * ntfs_index.c - NTFS kernel index handling.
 *
 * Copyright (c) 2006-2008 Anton Altaparmakov.  All Rights Reserved.
 * Portions Copyright (c) 2006-2008 Apple Inc.  All Rights Reserved.
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

#include <string.h>

#ifdef KERNEL
#include <libkern/libkern.h>
#include <libkern/OSMalloc.h>

#include <kern/debug.h>
#include <kern/locks.h>
#endif

#include "ntfs.h"
#include "ntfs_attr.h"
#include "ntfs_attr_list.h"
#include "ntfs_bitmap.h"
#include "ntfs_collate.h"
#include "ntfs_debug.h"
#include "ntfs_dir.h"
#include "ntfs_endian.h"
#include "ntfs_index.h"
#include "ntfs_inode.h"
#include "ntfs_layout.h"
#include "ntfs_lcnalloc.h"
#include "ntfs_mft.h"
#include "ntfs_page.h"
#include "ntfs_runlist.h"
#include "ntfs_types.h"
#include "ntfs_unistr.h"
#include "ntfs_volume.h"

/**
 * ntfs_index_ctx_unlock - unlock an index context
 * @ictx:	index context to unlock
 *
 * Unlock the index context @ictx.  We also unmap the mft record (in index root
 * case) or the page (in index allocation block case) thus all pointers into
 * the index node need to be revalidated when the mft record or page is mapped
 * again in ntfs_index_ctx_relock().
 *
 * Locking: - Caller must hold @ictx->idx_ni->lock on the index inode.
 *	    - The index context @ictx must be locked.
 */
void ntfs_index_ctx_unlock(ntfs_index_context *ictx)
{
	if (!ictx)
		panic("%s(): !ictx\n", __FUNCTION__);
	if (!ictx->is_locked)
		panic("%s(): !ictx->is_locked\n", __FUNCTION__);
	if (ictx->is_root) {
		if (ictx->actx) {
			ntfs_attr_search_ctx_put(ictx->actx);
			if (!ictx->base_ni)
				panic("%s(): !ictx->base_ni\n", __FUNCTION__);
			ntfs_mft_record_unmap(ictx->base_ni);
			ictx->actx = NULL;
		}
	} else {
		if (ictx->upl) {
			ntfs_page_unmap(ictx->idx_ni, ictx->upl, ictx->pl,
					ictx->is_dirty);
			ictx->upl = NULL;
			ictx->is_dirty = 0;
		}
	}
	ictx->is_locked = 0;
}

/**
 * ntfs_index_ctx_path_unlock - unlock an entire B+tree index context path
 * @index_ctx:	index context whose whole path to unlock
 *
 * Unlock all index contexts attached to the B+tree path to which @index_ctx
 * belongs.
 * 
 * This function is only called in error handling to ensure nothing is held
 * busy so the error handling code cannot deadlock.
 *
 * Locking: Caller must hold @index_ctx->idx_ni->lock on the index inode.
 */
static void ntfs_index_ctx_path_unlock(ntfs_index_context *index_ctx)
{
	ntfs_index_context *ictx;

	ictx = index_ctx;
	if (!ictx)
		panic("%s(): !ictx\n", __FUNCTION__);
	/*
	 * Note we traverse the tree path backwards (upwards) because @up is
	 * the first element in the index_context structure thus doing things
	 * this way generates faster/better machine code.
	 */
	do {
		if (ictx->is_locked)
			ntfs_index_ctx_unlock(ictx);
	} while ((ictx = ictx->up) != index_ctx);
}

/**
 * ntfs_index_ctx_relock - relock an index context that was unlocked earlier
 * @ictx:	index context to relock
 *
 * Relock the index context @ictx after it was unlocked with
 * ntfs_index_ctx_unlock().  We also remap the mft record (in index root case)
 * or the page (in index allocation block case) after which we revalidate all
 * pointers into the index node because the page may have been mapped into a
 * different virtual address and the mft record may have been changed with the
 * result that the index root attribute is moved within the mft record or even
 * to a completely different mft record.
 *
 * Note the check whether to revalidate or not is very simple because the index
 * node content cannot have changed thus all points change by a fixed offset
 * delta which once determined can be applied to all pointers.
 *
 * In the index root case, there is also a non-pointer index context field that
 * can have changed (and it does so irrespective of the index root position).
 * This is @ictx->bytes_free as that is dependent on the other attributes in
 * the mft record which can have changed legitimately under our feet which of
 * course is the reason why the index root can have moved about, too.
 *
 * Return 0 on success and errno on error.
 *
 * Locking: - Caller must hold @ictx->idx_ni->lock on the index inode.
 *	    - The index context @ictx must not be locked.
 */
errno_t ntfs_index_ctx_relock(ntfs_index_context *ictx)
{
	long delta;
	errno_t err;

	if (!ictx)
		panic("%s(): !ictx\n", __FUNCTION__);
	if (ictx->is_locked)
		panic("%s(): ictx->is_locked\n", __FUNCTION__);
	if (ictx->is_root) {
		MFT_RECORD *m;
		ntfs_attr_search_ctx *actx;

		if (ictx->actx)
			panic("%s(): ictx->actx\n", __FUNCTION__);
		if (!ictx->base_ni)
			panic("%s(): !ictx->base_ni\n", __FUNCTION__);
		err = ntfs_mft_record_map(ictx->base_ni, &m);
		if (err) {
			ntfs_error(ictx->idx_ni->vol->mp, "Failed to lock "
					"index root because "
					"ntfs_mft_record_map() failed (error "
					"%d).", err);
			return err;
		}
		ictx->actx = actx = ntfs_attr_search_ctx_get(ictx->base_ni, m);
		if (!actx) {
			ntfs_error(ictx->idx_ni->vol->mp, "Failed to allocate "
					"attribute search context.");
			err = ENOMEM;
			goto actx_err;
		}
		err = ntfs_attr_lookup(AT_INDEX_ROOT, ictx->idx_ni->name,
				ictx->idx_ni->name_len, 0, NULL, 0, actx);
		if (err) {
			if (err == ENOENT)
				panic("%s(): err == ENOENT\n", __FUNCTION__);
			ntfs_error(ictx->idx_ni->vol->mp, "Failed to look up "
					"index root attribute (error %d).",
					err);
			goto actx_err;
		}
		ictx->bytes_free = le32_to_cpu(actx->m->bytes_allocated) -
				le32_to_cpu(actx->m->bytes_in_use);
		/* Get to the index root value. */
		ictx->ir = (INDEX_ROOT*)((u8*)actx->a +
				le16_to_cpu(actx->a->value_offset));
		delta = (u8*)&ictx->ir->index - (u8*)ictx->index;
		ictx->index = &ictx->ir->index;
	} else {
		u8 *addr;

		if (ictx->upl)
			panic("%s(): ictx->upl\n", __FUNCTION__);
		if (ictx->is_dirty)
			panic("%s(): ictx->is_dirty\n", __FUNCTION__);
		err = ntfs_page_map(ictx->idx_ni, ictx->upl_ofs, &ictx->upl,
				&ictx->pl, &addr, TRUE);
		if (err) {
			ntfs_error(ictx->idx_ni->vol->mp, "Failed to map "
					"index page (error %d).", err);
			ictx->upl = NULL;
			return err;
		}
		/* Get to the index allocation block. */
		delta = addr - ictx->addr;
		if (delta) {
			ictx->addr = addr;
			ictx->ia = (INDEX_ALLOCATION*)((u8*)ictx->ia + delta);
			ictx->index = &ictx->ia->index;
		}
	}
	if (delta) {
		INDEX_ENTRY **entries;
		unsigned u;

		/*
		 * The index node has moved thus we have to update all stored
		 * pointers so they point into the new memory location.
		 */
		ictx->entry = (INDEX_ENTRY*)((u8*)ictx->entry + delta);
		entries = ictx->entries;
		for (u = 0; u < ictx->nr_entries; u++)
			entries[u] = (INDEX_ENTRY*)((u8*)entries[u] + delta);
	}
	ictx->is_locked = 1;
	return 0;
actx_err:
	if (ictx->actx) {
		ntfs_attr_search_ctx_put(ictx->actx);
		ictx->actx = NULL;
	}
	ntfs_mft_record_unmap(ictx->base_ni);
	return err;
}

/**
 * ntfs_index_ctx_get - allocate and initialize a new index context
 * @idx_ni:	ntfs index inode with which to initialize the context
 *
 * Allocate a new index context, initialize it with @idx_ni and return it.
 *
 * Return NULL if the allocation failed.
 *
 * Locking: Caller must hold @idx_ni->lock on the index inode.
 */
ntfs_index_context *ntfs_index_ctx_get(ntfs_inode *idx_ni)
{
	ntfs_index_context *ictx;

	ictx = ntfs_index_ctx_alloc();
	if (ictx)
		ntfs_index_ctx_init(ictx, idx_ni);
	return ictx;
}

/**
 * ntfs_index_ctx_put_reuse_single - release an index context but do not free it
 * @ictx:	index context to release
 *
 * Release the index context @ictx, releasing all associated resources but keep
 * the index context itself allocated so it can be reused with a call to
 * ntfs_index_ctx_init().
 *
 * This function ignores the tree path which this entry may be a part of and is
 * only a helper function for ntfs_index_ctx_put_reuse().
 *
 * Locking: Caller must hold @ictx->idx_ni->lock on the index inode.
 */
void ntfs_index_ctx_put_reuse_single(ntfs_index_context *ictx)
{
	if (ictx->entry && ictx->is_locked)
		ntfs_index_ctx_unlock(ictx);
	if (ictx->entries)
		OSFree(ictx->entries, ictx->max_entries * sizeof(INDEX_ENTRY*),
				ntfs_malloc_tag);
}

/**
 * ntfs_index_ctx_put_reuse - release an index context but do not free it
 * @index_ctx:	index context to release
 *
 * Release the index context @index_ctx, releasing all associated resources but
 * keep the index context itself allocated so it can be reused with a call to
 * ntfs_index_ctx_init().
 *
 * Locking: Caller must hold @index_ctx->idx_ni->lock on the index inode.
 */
void ntfs_index_ctx_put_reuse(ntfs_index_context *index_ctx)
{
	ntfs_index_context *ictx, *next;

	/*
	 * Destroy all tree path components except @index_ctx itself.  We need
	 * the temporary index context pointer @next because we deallocate the
	 * current index context in each iteration of the loop thus we would no
	 * longer have any means of finding the next index context in the tree
	 * path if we had not already stored a pointer to it in @next.  Note we
	 * actually traverse the tree path backwards (upwards) because @up is
	 * the first element in the index_context structure thus doing things
	 * this way generates faster/better machine code.
	 */
	for (ictx = index_ctx->up, next = ictx->up; ictx != index_ctx;
			ictx = next, next = next->up) {
		/*
		 * Disconnect the current index context from the tree and
		 * release it and all its resources.
		 */
		ntfs_index_ctx_put_single(ictx);
	}
	/* Reuse the only remaining, bottom entry. */
	ntfs_index_ctx_put_reuse_single(index_ctx);
}

/**
 * ntfs_index_get_entries - get the entries for the index node into the context
 * @ictx:	index context for which to get the entries
 *
 * Loop through the entries in the index node described by @ictx and gather all
 * the index entries into the @ictx->entries array which is also allocated by
 * this function.
 *
 * Return 0 on success and errno on error.
 *
 * Locking: - Caller must hold @ictx->idx_ni->lock on the index inode.
 *	    - The index context @ictx must be locked.
 */
static errno_t ntfs_index_get_entries(ntfs_index_context *ictx)
{
	u8 *index_end;
	INDEX_HEADER *index;
	INDEX_ENTRY *ie, **entries;
	unsigned nr_entries, max_entries;
	errno_t err;
	BOOL is_view;

	ntfs_debug("Entering.");
	if (!ictx->is_locked)
		panic("%s(): !ictx->is_locked\n", __FUNCTION__);
	is_view = FALSE;
	if (ictx->idx_ni->name != I30)
		is_view = TRUE;
	nr_entries = 0;
	max_entries = ictx->max_entries;
	/* Allocate memory for the index entry pointers in the index node. */
	entries = OSMalloc(max_entries * sizeof(INDEX_ENTRY*), ntfs_malloc_tag);
	if (!entries) {
		ntfs_error(ictx->idx_ni->vol->mp, "Failed to allocate index "
				"entry pointer array.");
		return ENOMEM;
	}
	index = ictx->index;
	index_end = (u8*)index + le32_to_cpu(index->index_length);
	/* The first index entry. */
	ie = (INDEX_ENTRY*)((u8*)index + le32_to_cpu(index->entries_offset));
	/*
	 * Loop until we exceed valid memory (corruption case) or until we
	 * reach the last entry and for each entry place a pointer to it into
	 * our array of entry pointers.
	 */
	for (;; ie = (INDEX_ENTRY*)((u8*)ie + le16_to_cpu(ie->length))) {
		/* Bounds checks. */
		if ((u8*)ie < (u8*)index || (u8*)ie +
				sizeof(INDEX_ENTRY_HEADER) > index_end ||
				(u8*)ie + le16_to_cpu(ie->length) > index_end ||
				(u32)sizeof(INDEX_ENTRY_HEADER) +
				le16_to_cpu(ie->key_length) >
				le16_to_cpu(ie->length))
			goto err;
		/* Add this entry to the array of entry pointers. */
		if (nr_entries >= max_entries)
			panic("%s(): nr_entries >= max_entries\n",
					__FUNCTION__);
		entries[nr_entries++] = ie;
		if (ie->flags & INDEX_ENTRY_END)
			break;
		/* Further bounds checks for view indexes. */
		if (is_view && ((u32)sizeof(INDEX_ENTRY_HEADER) +
				le16_to_cpu(ie->key_length) >
				le16_to_cpu(ie->data_offset) ||
				(u32)le16_to_cpu(ie->data_offset) +
				le16_to_cpu(ie->data_length) >
				le16_to_cpu(ie->length)))
			goto err;
	}
	/* Except for the index root, leaf nodes are not allowed to be empty. */
	if (nr_entries < 2 && !ictx->is_root && !(index->flags & INDEX_NODE)) {
		ntfs_error(ictx->idx_ni->vol->mp, "Illegal empty leaf node "
				"found in index.");
		err = EIO;
		goto err;
	}
	ictx->entries = entries;
	ictx->nr_entries = nr_entries;
	ntfs_debug("Done.");
	return 0;
err:
	OSFree(entries, max_entries * sizeof(INDEX_ENTRY*), ntfs_malloc_tag);
	ntfs_error(ictx->idx_ni->vol->mp, "Corrupt index in inode 0x%llx.  "
			"Run chkdsk.",
			(unsigned long long)ictx->idx_ni->mft_no);
	NVolSetErrors(ictx->idx_ni->vol);
	return EIO;
}

/**
 * ntfs_index_lookup_init - prepare an index context for a lookup
 * @ictx:	[IN] index context to prepare
 * @key_len:	[IN] size of the key ntfs_index_lookup() is called with in bytes
 *
 * Prepare the index context @ictx for a call to ntfs_index_lookup() or
 * ntfs_index_lookup_by_position().
 *
 * @key_len is the length of the key ntfs_index_lookup() will be called with.
 * If the index @ictx is a directory index this is ignored.
 *
 * Return 0 on success and errno on error.
 */
static errno_t ntfs_index_lookup_init(ntfs_index_context *ictx,
		const int key_len)
{
	ntfs_inode *idx_ni;
	MFT_RECORD *m;
	ntfs_attr_search_ctx *actx;
	INDEX_ROOT *ir;
	unsigned min_entry_size, max_entries;
	errno_t err;

	ntfs_debug("Entering.");
	if (!ictx->base_ni)
		panic("%s(): !ictx->base_ni\n", __FUNCTION__);
	idx_ni = ictx->idx_ni;
	if (idx_ni->type != AT_INDEX_ALLOCATION)
		panic("%s(): idx_ni->type != AT_INDEX_ALLOCATION\n",
				__FUNCTION__);
	/*
	 * Ensure the index context is still uninitialized, i.e. it is not
	 * legal to call ntfs_index_lookup*() with a search context that has
	 * been used already.
	 */
	if (ictx->up != ictx || ictx->max_entries)
		panic("%s(): Called for already used index context.\n",
				__FUNCTION__);
	if (!ntfs_is_collation_rule_supported(idx_ni->collation_rule)) {
		ntfs_error(idx_ni->vol->mp, "Index uses unsupported collation "
				"rule 0x%x.  Aborting.",
				le32_to_cpu(idx_ni->collation_rule));
		return ENOTSUP;
	}
	/* Get hold of the mft record for the index inode. */
	err = ntfs_mft_record_map(ictx->base_ni, &m);
	if (err) {
		ntfs_error(idx_ni->vol->mp, "Failed to map mft record (error "
				"%d.", err);
		return err;
	}
	actx = ntfs_attr_search_ctx_get(ictx->base_ni, m);
	if (!actx) {
		err = ENOMEM;
		goto err;
	}
	/* Find the index root attribute in the mft record. */
	err = ntfs_attr_lookup(AT_INDEX_ROOT, idx_ni->name, idx_ni->name_len,
			0, NULL, 0, actx);
	if (err) {
		if (err == ENOENT) {
			ntfs_error(idx_ni->vol->mp, "Index root attribute "
					"missing in inode 0x%llx.  Inode is "
					"corrupt.  Run chkdsk.",
					(unsigned long long)idx_ni->mft_no);
			/*
			 * This will cause the returned error to be EIO and the
			 * volume to be marked as containing errors.
			 */
			err = 0;
		}
		goto err;
	}
	/*
	 * The entry size is made up of the index entry header plus the index
	 * key plus the index data plus optionally the sub-node vcn pointer.
	 *
	 * For most index types the index key is constant size and is simply
	 * given by the caller supplied @key_len.
	 *
	 * The only collation types with variable sized keys are
	 * COLLATION_FILENAME and COLLATION_NTOFS_SID.
	 *
	 * Determining the index data size is more complicated than that so we
	 * simply ignore it.  This means we waste some memory but it is not too
	 * bad as only directory indexes appear more than once on a volume thus
	 * only they can have more than one lookup in progress at any one time.
	 */
	min_entry_size = sizeof(INDEX_ENTRY_HEADER) + sizeof(FILENAME_ATTR);
	if (idx_ni->collation_rule != COLLATION_FILENAME) {
		if (idx_ni->collation_rule == COLLATION_NTOFS_SID)
			min_entry_size = sizeof(INDEX_ENTRY_HEADER) +
					sizeof(SID);
		else
			min_entry_size = sizeof(INDEX_ENTRY_HEADER) + key_len;
	}
	/*
	 * Work out the absolute maximum number of entries there can be in an
	 * index allocation block.  We add one for the end entry which does not
	 * contain a key.
	 */
	max_entries = 1 + ((idx_ni->block_size - sizeof(INDEX_BLOCK) -
			sizeof(INDEX_ENTRY_HEADER)) / min_entry_size);
	/*
	 * Should the mft record size exceed the size of an index block (this
	 * should never really happen) then calculate the maximum number of
	 * entries there can be in the index root and if they are more than the
	 * ones in the index block use that as the maximum value.
	 */
	if (idx_ni->vol->mft_record_size > idx_ni->block_size) {
		unsigned max_root_entries;

		max_root_entries = 1 + ((idx_ni->vol->mft_record_size -
				le16_to_cpu(m->attrs_offset) -
				offsetof(ATTR_RECORD, reservedR) -
				sizeof(((ATTR_RECORD*)NULL)->reservedR) -
				sizeof(INDEX_ROOT) -
				sizeof(INDEX_ENTRY_HEADER)) / min_entry_size);
		if (max_root_entries > max_entries)
			max_entries = max_root_entries;
	}
	ictx->max_entries = max_entries;
	/*
	 * Get to the index root value (it has been verified when the inode was
	 * read in ntfs_index_inode_read()).
	 */
	ir = (INDEX_ROOT*)((u8*)actx->a + le16_to_cpu(actx->a->value_offset));
	ictx->index = &ir->index;
	/*
	 * Gather the index entry pointers and finish setting up the index
	 * context.
	 */
	ictx->is_root = 1;
	ictx->is_locked = 1;
	err = ntfs_index_get_entries(ictx);
	if (err) {
		ictx->is_locked = 0;
		goto err;
	}
	ictx->ir = ir;
	ictx->actx = actx;
	ictx->bytes_free = le32_to_cpu(actx->m->bytes_allocated) -
			le32_to_cpu(actx->m->bytes_in_use);
	ntfs_debug("Done.");
	return 0;
err:
	if (actx)
		ntfs_attr_search_ctx_put(actx);
	ntfs_mft_record_unmap(ictx->base_ni);
	ntfs_error(idx_ni->vol->mp, "Failed (error %d).", err);
	return err;
}

/**
 * ntfs_index_descend_into_child_node - index node whose child node to return
 * @index_ctx:	pointer to index context whose child node to return
 *
 * Descend into the child node pointed to by (*@index_ctx)->entry and return
 * its fully set up index context in *@index_ctx.
 *
 * Return 0 on success and errno on error.
 *
 * Locking: - Caller must hold @ictx->idx_ni->lock on the index inode.
 *	    - The index context @ictx must be locked.
 */
static errno_t ntfs_index_descend_into_child_node(
		ntfs_index_context **index_ctx)
{
	VCN vcn;
	s64 ofs;
	ntfs_index_context *ictx, *new_ictx;
	ntfs_inode *idx_ni;
	upl_t upl;
	upl_page_info_array_t pl;
	u8 *addr;
	INDEX_ALLOCATION *ia;
	errno_t err = 0;
	int is_dirty = 0;
	static const char es[] = "%s.  Inode 0x%llx is corrupt.  Run chkdsk.";

	ntfs_debug("Entering.");
	if (!index_ctx)
		panic("%s(): !index_ctx\n", __FUNCTION__);
	ictx = *index_ctx;
	if (!ictx)
		panic("%s(): !ictx\n", __FUNCTION__);
	idx_ni = ictx->idx_ni;
	if (!ictx->is_locked)
		panic("%s(): !ictx->is_locked\n", __FUNCTION__);
	if (!(ictx->entry->flags & INDEX_ENTRY_NODE))
		panic("%s(): !(ictx->entry->flags & INDEX_ENTRY_NODE)\n",
				__FUNCTION__);
	/*
	 * Since INDEX_NODE == LARGE_INDEX this check is ok for the index root
	 * as well.
	 */
	if (!(ictx->index->flags & INDEX_NODE)) {
		ntfs_error(idx_ni->vol->mp, es, "Index entry with child node "
				"found in a leaf node",
				(unsigned long long)idx_ni->mft_no);
		goto err;
	}
	/* Get the starting vcn of the child index block to descend into. */
	vcn = sle64_to_cpup((sle64*)((u8*)ictx->entry +
			le16_to_cpu(ictx->entry->length) - sizeof(VCN)));
	if (vcn < 0) {
		ntfs_error(idx_ni->vol->mp, es, "Negative child node VCN",
				(unsigned long long)idx_ni->mft_no);
		goto err;
	}
	/* Determine the offset of the page containing the child index block. */
	ofs = (vcn << idx_ni->vcn_size_shift) & ~PAGE_MASK_64;
	/*
	 * If the entry whose sub-node we are descending into is in the index
	 * root, release the index root unlocking its node or we can deadlock
	 * with ntfs_page_map().
	 */
	if (ictx->is_root) {
		/*
		 * As a sanity check verify that the index allocation attribute
		 * exists.
		 */
		if (!NInoIndexAllocPresent(idx_ni)) {
			ntfs_error(idx_ni->vol->mp, "No index allocation "
					"attribute but index root entry "
					"requires one.  Inode 0x%llx is "
					"corrupt.  Run chkdsk.",
					(unsigned long long)idx_ni->mft_no);
			goto err;
		}
		ntfs_index_ctx_unlock(ictx);
	} else /* if (!ictx->is_root) */ {
		/*
		 * If @vcn is in the same VM page as the existing page we reuse
		 * the mapped page, otherwise we release the page so we can get
		 * the new one.
		 */
		upl = ictx->upl;
		pl = ictx->pl;
		addr = ictx->addr;
		is_dirty = ictx->is_dirty;
		ictx->upl = NULL;
		ictx->is_dirty = 0;
		ictx->is_locked = 0;
		if (ofs == ictx->upl_ofs)
			goto have_page;
		ntfs_page_unmap(idx_ni, upl, pl, is_dirty);
		is_dirty = 0;
	}
	/* We did not reuse the old page, get the new page. */
	err = ntfs_page_map(idx_ni, ofs, &upl, &pl, &addr, TRUE);
	if (err) {
		ntfs_error(idx_ni->vol->mp, "Failed to map index page (error "
				"%d).", err);
		goto err;
	}
have_page:
	/* Get to the index allocation block. */
	ia = (INDEX_ALLOCATION*)(addr + ((unsigned)(vcn <<
			idx_ni->vcn_size_shift) & PAGE_MASK));
	/* Bounds checks. */
	if ((u8*)ia < addr || (u8*)ia > addr + PAGE_SIZE) {
		ntfs_error(idx_ni->vol->mp, es, "Out of bounds check failed",
				(unsigned long long)idx_ni->mft_no);
		goto unm_err;
	}
	/* Catch multi sector transfer fixup errors. */
	if (!ntfs_is_indx_record(ia->magic)) {
		ntfs_error(idx_ni->vol->mp, "Index record with VCN 0x%llx is "
				"corrupt.  Inode 0x%llx is corrupt.  Run "
				"chkdsk.", (unsigned long long)vcn,
				(unsigned long long)idx_ni->mft_no);
		goto unm_err;
	}
	if (sle64_to_cpu(ia->index_block_vcn) != vcn) {
		ntfs_error(idx_ni->vol->mp, "Actual VCN (0x%llx) of index "
				"buffer is different from expected VCN "
				"(0x%llx).  Inode 0x%llx is corrupt.  Run "
				"chkdsk.", (unsigned long long)
				sle64_to_cpu(ia->index_block_vcn),
				(unsigned long long)vcn,
				(unsigned long long)idx_ni->mft_no);
		goto unm_err;
	}
	if (offsetof(INDEX_BLOCK, index) + le32_to_cpu(
			ia->index.allocated_size) != idx_ni->block_size) {
		ntfs_error(idx_ni->vol->mp, "Index buffer (VCN 0x%llx) of "
				"inode 0x%llx has a size (%u) differing from "
				"the index specified size (%u).  Inode is "
				"corrupt.  Run chkdsk.",
				(unsigned long long)vcn,
				(unsigned long long)idx_ni->mft_no,
				(unsigned)offsetof(INDEX_BLOCK, index) +
				le32_to_cpu(ia->index.allocated_size),
				(unsigned)idx_ni->block_size);
		goto unm_err;
	}
	if ((u8*)ia + idx_ni->block_size > addr + PAGE_SIZE)
		panic("%s(): (u8*)ia + idx_ni->block_size > kaddr + "
				"PAGE_SIZE\n", __FUNCTION__);
	if ((u8*)&ia->index + le32_to_cpu(ia->index.index_length) >
			(u8*)ia + idx_ni->block_size) {
		ntfs_error(idx_ni->vol->mp, "Size of index buffer (VCN "
				"0x%llx) of inode 0x%llx exceeds maximum "
				"size.", (unsigned long long)vcn,
				(unsigned long long)idx_ni->mft_no);
		goto unm_err;
	}
	/* Allocate a new index context. */
	new_ictx = ntfs_index_ctx_alloc();
	if (!new_ictx) {
		err = ENOMEM;
		ntfs_error(idx_ni->vol->mp, "Failed to allocate index "
				"context.");
		goto unm_err;
	}
	/*
	 * Attach the new index context between the current index context
	 * (which is the bottom of the tree) and the index context below it
	 * (which is the top of the tree), i.e. place the new index context at
	 * the bottom of the tree.
	 *
	 * Gcc is broken so it fails to see both the members of the anonymous
	 * union(s) and the bitfield inside the C99 initializer.  Work around
	 * this by setting @is_locked, @is_dirty, @ia, and @vcn afterwards.
	 */
	*new_ictx = (ntfs_index_context) {
		.up = ictx,
		.down = ictx->down,
		.idx_ni = idx_ni,
		.base_ni = ictx->base_ni,
		.index = &ia->index,
		.bytes_free = le32_to_cpu(ia->index.allocated_size) -
				le32_to_cpu(ia->index.index_length),
		.max_entries = ictx->max_entries,
	};
	new_ictx->is_locked = 1;
	new_ictx->is_dirty = is_dirty;
	new_ictx->ia = ia;
	new_ictx->vcn = vcn;
	new_ictx->upl_ofs = ofs;
	new_ictx->upl = upl;
	new_ictx->pl = pl;
	new_ictx->addr = addr;
	ictx->down->up = new_ictx;
	ictx->down = new_ictx;
	/*
	 * Gather the index entry pointers and finish setting up the new index
	 * context.
	 */
	err = ntfs_index_get_entries(new_ictx);
	if (!err) {
		*index_ctx = new_ictx;
		ntfs_debug("Done.");
		return 0;
	}
	new_ictx->is_locked = 0;
	new_ictx->is_dirty = 0;
	new_ictx->upl = NULL;
unm_err:
	ntfs_page_unmap(idx_ni, upl, pl, is_dirty);
err:
	if (!err) {
		NVolSetErrors(idx_ni->vol);
		err = EIO;
	}
	return err;
}

/**
 * ntfs_index_lookup_in_node - search for an entry in an index node
 * @ictx:		index context in which to search for the entry
 * @match_key:		index entry key data to search for
 * @match_key_len:	length of @match_key in bytes
 * @key:		index entry key to search for
 * @key_len:		length of @key in bytes
 *
 * Perform a binary search through the index entries in the index node
 * described by @ictx looking for the correct entry or if not found for the
 * index entry whose sub-node pointer to descend into.
 *
 * @key and @key_len is the complete key to search for.  This is used when
 * doing the full blown collation.
 *
 * When doing exact matching for filenames we need to compare the actual
 * filenames rather than the filename attributes whereas for view indexes we
 * need to compare the whole key.  To make this function simpler we let the
 * caller specify the appropriate data to use for exact matching in @match_key
 * and @match_key_len.  For view indexes @match_key and @match_key_len are the
 * same as @key and @key_len respectively.
 *
 * Return 0 on success and errno on error.
 *
 * Locking: - Caller must hold @ictx->idx_ni->lock on the index inode.
 *	    - The index context @ictx must be locked.
 */
static errno_t ntfs_index_lookup_in_node(ntfs_index_context *ictx,
		const void *match_key, const int match_key_len,
		const void *key, const int key_len)
{
	ntfs_inode *idx_ni;
	INDEX_ENTRY *ie, **entries;
	unsigned min_left, max_right, cur_entry;
	int rc;
	BOOL is_view;

	ntfs_debug("Entering.");
	idx_ni = ictx->idx_ni;
	is_view = FALSE;
	if (idx_ni->name != I30)
		is_view = TRUE;
	entries = ictx->entries;
	/*
	 * If there is only one entry, i.e. the index node is empty, we need to
	 * descend into the sub-node pointer of the end entry if present thus
	 * return the end entry.
	 */
	if (ictx->nr_entries == 1) {
		cur_entry = 0;
		goto not_found;
	}
	/*
	 * Now do a binary search through the index entries looking for the
	 * correct entry or if not found for the index entry whose sub-node
	 * pointer to descend into.
	 *
	 * Note we exclude the end entry from the search as it does not include
	 * a key we can compare.  That makes the search algorithm simpler and
	 * more efficient.
	 */
	min_left = 0;
	max_right = ictx->nr_entries - 2;
	cur_entry = max_right / 2;
	do {
		void *ie_match_key;
		int ie_match_key_len;

		ie = entries[cur_entry];
		if (!is_view) {
			ie_match_key_len = ie->key.filename.filename_length <<
					NTFSCHAR_SIZE_SHIFT;
			ie_match_key = &ie->key.filename.filename;
		} else {
			ie_match_key_len = le16_to_cpu(ie->key_length);
			ie_match_key = &ie->key;
		}
		/*
		 * If the keys match perfectly, we setup @ictx to point to the
		 * matching entry and return.
		 */
		if ((match_key_len == ie_match_key_len) &&
				!bcmp(match_key, ie_match_key,
				ie_match_key_len)) {
found:
			ictx->entry = ie;
			ictx->entry_nr = cur_entry;
			ictx->is_match = 1;
			ntfs_debug("Done (found).");
			return 0;
		}
		/*
		 * Not a perfect match, need to do full blown collation so we
		 * know which way in the B+tree we have to go.
		 */
		rc = ntfs_collate(idx_ni->vol, idx_ni->collation_rule, key,
				key_len, &ie->key, le16_to_cpu(ie->key_length));
		/*
		 * If @key collates before the key of the current entry, need
		 * to search on the left.
		 */
		if (rc == -1) {
			/*
			 * If this is the left-most entry we need to descend
			 * into it if it has a sub-node.
			 */
			if (cur_entry == min_left)
				break;
			/* Exclude the wrong half or remaining entries. */
			max_right = cur_entry - 1;
			/* Find the middle of the remaining entries. */
			cur_entry = (min_left + max_right) / 2;
			continue;
		}
		/*
		 * A match should never happen as the bcmp() call should have
		 * caught it, but we still treat it correctly.
		 */
		if (!rc)
			goto found;
		/*
		 * @key collates after the key of the current entry, need to
		 * search on the right.
		 *
		 * If this is the right-most entry we need to descend into the
		 * end entry on the right if it has a sub-node.
		 */
		if (cur_entry == max_right) {
			cur_entry++;
			break;
		}
		/* Exclude the wrong half or remaining entries. */
		min_left = cur_entry + 1;
		/* Find the middle of the remaining entries. */
		cur_entry = (min_left + max_right) / 2;
	} while (1);
not_found:
	ictx->follow_entry = entries[cur_entry];
	ictx->entry_nr = cur_entry;
	ntfs_debug("Done (not found).");
	return ENOENT;
}

/**
 * ntfs_index_lookup - find a key in an index and return its index entry
 * @key:	[IN] key for which to search in the index
 * @key_len:	[IN] length of @key in bytes
 * @index_ctx:	[IN/OUT] context describing the index and the returned entry
 *
 * Before calling ntfs_index_lookup(), *@index_ctx must have been obtained
 * from a call to ntfs_index_ctx_get().
 *
 * Look for the @key in the index specified by the index lookup context
 * @index_ctx.  ntfs_index_lookup() walks the contents of the index looking for
 * the @key.  As it does so, it records the path taken through the B+tree
 * together with other useful information it obtains along the way.  This is
 * needed if the lookup is going to be followed by a complex index tree
 * operation such as an index entry addition requiring one or more index block
 * splits for example.
 *
 * If the @key is found in the index, 0 is returned and *@index_ctx is set up
 * to describe the index entry containing the matching @key.  The matching
 * entry is pointed to by *@index_ctx->entry.
 *
 * If the @key is not found in the index, ENOENT is returned and *@index_ctx is
 * setup to describe the index entry whose key collates immediately after the
 * search @key, i.e. this is the position in the index at which an index entry
 * with a key of @key would need to be inserted.
 *
 * If an error occurs return the error code.  In this case *@index_ctx is
 * undefined and must be freed via a call to ntfs_index_ctx_put() or
 * reinitialized via a call to ntfs_index_ctx_put_reuse().
 *
 * When finished with the entry and its data, call ntfs_index_ctx_put() to free
 * the context and other associated resources.
 *
 * If the index entry was modified, call ntfs_index_entry_mark_dirty() before
 * the call to ntfs_index_ctx_put() to ensure that the changes are written to
 * disk.
 *
 * Locking: - Caller must hold @index_ctx->idx_ni->lock on the index inode.
 *	    - Caller must hold an iocount reference on the index inode.
 *
 * TODO: An optimization would be to take two new parameters, say @add and
 * @del, which allow our caller to tell us if the search is for purposes of
 * adding an entry (@add is true) or for removing an entry (@del is true) or if
 * it is a simple lookup (read-only or overwrite without change in length, both
 * @add and @del are false).  For the lookup case we do not need to record the
 * path so we can just use one single index context and only one array of index
 * entry pointers and we keep reusing both instead of getting new ones each
 * time we descend into a sub-node.  Alternatively take a single parameter
 * @reason or @intent or something and define some constants like NTFS_LOOKUP,
 * NTFS_ADD, and NTFS_DEL or something to go with it to serve the same purpose
 * as above.
 */
errno_t ntfs_index_lookup(const void *key, const int key_len,
		ntfs_index_context **index_ctx)
{
	ntfs_index_context *ictx;
	const void *match_key;
	int match_key_len;
	errno_t err;

	ntfs_debug("Entering.");
	if (!key)
		panic("%s(): !key\n", __FUNCTION__);
	if (key_len <= 0)
		panic("%s(): key_len <= 0\n", __FUNCTION__);
	ictx = *index_ctx;
	/*
	 * When doing exact matching for filenames we need to compare the
	 * actual filenames rather than the filename attributes whereas for
	 * view indexes we need to compare the whole key.
	 */
	if (ictx->idx_ni->name == I30) {
		match_key_len = ((FILENAME_ATTR*)key)->filename_length <<
				NTFSCHAR_SIZE_SHIFT;
		match_key = &((FILENAME_ATTR*)key)->filename;
	} else {
		match_key_len = key_len;
		match_key = key;
	}
	/* Prepare the search context for its first lookup. */
	err = ntfs_index_lookup_init(ictx, key_len);
	if (err)
		goto err;
	do {
		/*
		 * Look for the @key in the current index node.  If found, the
		 * index context points to the found entry so we are done.  If
		 * not found, the index context points to the entry whose
		 * sub-node pointer we need to descend into if it is present
		 * and if not we have failed to find the entry and are also
		 * done.
		 */
		err = ntfs_index_lookup_in_node(ictx, match_key, match_key_len,
				key, key_len);
		if (err && err != ENOENT)
			panic("%s(): err && err != ENOENT\n", __FUNCTION__);
		if (!err || !(ictx->entry->flags & INDEX_ENTRY_NODE))
			break;
		/* Not found but child node present, descend into it. */
		err = ntfs_index_descend_into_child_node(&ictx);
		if (err)
			goto err;
		/*
		 * Replace the caller's index context with the new one so the
		 * caller always gets the bottom-most index context.
		 */
		*index_ctx = ictx;
	} while (1);
	ntfs_debug("Done (%s in index %s).",
			!err ? "found matching entry" : "entry not found",
			ictx->is_root ?  "root" : "allocation block");
	return err;
err:
	ntfs_error(ictx->idx_ni->vol->mp, "Failed (error %d).", err);
	return err;
}

/**
 * ntfs_index_lookup_by_position - find an entry by its position in the B+tree
 * @pos:	[IN] position of index entry to find in the B+tree
 * @key_len:	[IN] size of the key ntfs_index_lookup() is called with in bytes
 * @index_ctx:	[IN/OUT] context describing the index and the returned entry
 *
 * Before calling ntfs_index_lookup_by_position(), *@index_ctx must have been
 * obtained from a call to ntfs_index_ctx_get().
 *
 * Start searching at the beginning of the B+tree, counting each of the entries
 * and when the entry with position number @pos has been reached return that in
 * *@index_ctx.
 *
 * @key_len is the length of the key ntfs_index_lookup() would be called with.
 * If the index *@index_ictx is a directory index this is ignored.
 *
 * As the search progresses, the path taken through the B+tree is recorded
 * together with other useful information obtained along the way.  This is
 * needed so the lookup can proceed to the next entry if/when desired, for
 * example during a ntfs_readdir() call.
 *
 * If the position @pos is found in the index, 0 is returned and *@index_ctx is
 * set up to describe the index entry containing at position @pos.  The
 * matching entry is pointed to by *@index_ctx->entry.
 *
 * If the position @pos is not found in the index, ENOENT is returned.
 *
 * If an error occurs the error code is returned (cannot be ENOENT).
 *
 * If any values other than 0 are returned, *@index_ctx is undefined and must
 * be freed via a call to ntfs_index_ctx_put() or reinitialized via a call to
 * ntfs_index_ctx_put_reuse().
 *
 * When finished with the entry and its data, call ntfs_index_ctx_put() to free
 * the context and other associated resources.
 *
 * If the index entry was modified, call ntfs_index_entry_mark_dirty() before
 * the call to ntfs_index_ctx_put() to ensure that the changes are written to
 * disk.
 *
 * Locking: - Caller must hold @index_ctx->idx_ni->lock on the index inode.
 *	    - Caller must hold an iocount reference on the index inode.
 */
errno_t ntfs_index_lookup_by_position(const s64 pos, const int key_len,
		ntfs_index_context **index_ctx)
{
	s64 current_pos;
	ntfs_index_context *ictx;
	errno_t err;

	ntfs_debug("Entering for position 0x%llx.", (unsigned long long)pos);
	if (pos < 0)
		panic("%s(): pos < 0\n", __FUNCTION__);
	ictx = *index_ctx;
	/* Prepare the search context for its first lookup. */
	err = ntfs_index_lookup_init(ictx, key_len);
	if (err)
		goto err;
	/* Start at the first index entry in the index root. */
	ictx->entry = ictx->entries[0];
	ictx->entry_nr = 0;
	current_pos = 0;
	/*
	 * Iterate over the entries in the B+tree counting them up until we
	 * reach entry position @pos in which case we are done.
	 */
	do {
		/*
		 * Keep descending into the sub-node pointers until a leaf node
		 * is found.
		 */
		while (ictx->entry->flags & INDEX_ENTRY_NODE) {
			/* Child node present, descend into it. */
			err = ntfs_index_descend_into_child_node(&ictx);
			if (err)
				goto err;
			/* Start at the first index entry in the index node. */
			ictx->entry = ictx->entries[0];
			ictx->entry_nr = 0;
		}
		/*
		 * We have reached the next leaf node.  If @pos is in this node
		 * skip to the correct entry and we are done.
		 *
		 * Note we need the -1 because @ictx->nr_entries counts the end
		 * entry which does not contain any data thus we do not count
		 * it as a real entry.
		 */
		if (current_pos + ictx->nr_entries - 1 > pos) {
			/*
			 * No need to skip any entries if the current entry is
			 * the one matching @pos.
			 */
			if (current_pos < pos) {
				s64 nr;

				nr = ictx->entry_nr + (pos - current_pos);
				if (nr >= ictx->nr_entries - 1)
					panic("%s(): nr >= ictx->nr_entries - "
							"1\n", __FUNCTION__);
				ictx->entry_nr = (unsigned)nr;
				ictx->entry = ictx->entries[nr];
				current_pos = pos;
			} else if (current_pos > pos)
				panic("%s(): current_pos > pos\n",
						__FUNCTION__);
			break;
		}
		/*
		 * @pos is not in this node.  Skip the whole node, i.e. skip
		 * @ictx->nr_entries - 1 real index entries.
		 */
		current_pos += ictx->nr_entries - 1;
		/*
		 * Keep moving up the B+tree until we find an index node that
		 * we have not finished yet.
		 */
		do {
			ntfs_index_context *itmp;

			/* If we are in the index root, we are done. */
			if (ictx->is_root)
				goto not_found;
			/* Save the current index context so we can free it. */
			itmp = ictx;
			/* Move up to the parent node. */
			ictx = ictx->up;
			/*
			 * Disconnect the old index context from its path and
			 * free it.
			 */
			ntfs_index_ctx_disconnect(itmp);
			ntfs_index_ctx_put_reuse_single(itmp);
			ntfs_index_ctx_free(itmp);
		} while (ictx->entry_nr == ictx->nr_entries - 1);
		/*
		 * We have reached a node which we have not finished with yet.
		 * Lock it so we can work on it.
		 */
		err = ntfs_index_ctx_relock(ictx);
		if (err)
			goto err;
		/*
		 * Check if the current entry is the entry matching @pos and if
		 * so we are done.
		 */
		if (current_pos == pos)
			break;
		/*
		 * Move to the next entry in this node and continue descending
		 * into the sub-node pointers until a leaf node is found.  The
		 * first entry in the leaf node will be the next entry thus
		 * increment @current_pos now.
		 */
		ictx->entry = ictx->entries[++ictx->entry_nr];
		current_pos++;
	} while (1);
	/*
	 * Indicate that the current entry is the one the caller was looking
	 * for and return the index context containing it to the caller.
	 */
	ictx->is_match = 1;
	*index_ctx = ictx;
	ntfs_debug("Done (entry with position 0x%llx found in index %s).",
			(unsigned long long)pos,
			ictx->is_root ? "root" : "allocation block");
	return 0;
not_found:
	ntfs_debug("Done (entry with position 0x%llx not found in index, "
			"returning ENOENT).", (unsigned long long)pos);
	/* Ensure we return a valid index context. */
	*index_ctx = ictx;
	return ENOENT;
err:
	ntfs_error(ictx->idx_ni->vol->mp, "Failed (error %d).", err);
	/* Ensure we return a valid index context. */
	*index_ctx = ictx;
	return err;
}

/**
 * ntfs_index_lookup_next - find the next entry in the B+tree
 * @index_ctx:	[IN/OUT] context describing the index and the returned entry
 *
 * Before calling ntfs_index_lookup_next(), *@index_ctx must have been obtained
 * from a call to ntfs_index_lookup() or ntfs_index_lookup_by_position().
 *
 * If the next entry is found in the index, 0 is returned and *@index_ctx is
 * set up to describe the next index entry.  The matching entry is pointed to
 * by *@index_ctx->entry.
 *
 * If the position @pos is not found in the index, ENOENT is returned.
 *
 * If an error occurs the error code is returned (cannot be ENOENT).
 *
 * If any values other than 0 are returned, *@index_ctx is undefined and must
 * be freed via a call to ntfs_index_ctx_put() or reinitialized via a call to
 * ntfs_index_ctx_put_reuse().
 *
 * When finished with the entry and its data, call ntfs_index_ctx_put() to free
 * the context and other associated resources.
 *
 * If the index entry was modified, call ntfs_index_entry_mark_dirty() before
 * the call to ntfs_index_ctx_put() to ensure that the changes are written to
 * disk.
 *
 * Locking: - Caller must hold @index_ctx->idx_ni->lock on the index inode.
 *	    - Caller must hold an iocount reference on the index inode.
 */
errno_t ntfs_index_lookup_next(ntfs_index_context **index_ctx)
{
	ntfs_index_context *ictx;
	errno_t err;

	ntfs_debug("Entering.");
	ictx = *index_ctx;
	if (!ictx->base_ni)
		panic("%s(): !ictx->base_ni\n", __FUNCTION__);
	/*
	 * Ensure the index context is initialized, i.e. it is not legal to
	 * call ntfs_index_lookup_next() with a search context that has not
	 * been used for a call to ntfs_index_lookup_by_position() or
	 * ntfs_index_lookup() yet.
	 */
	if (!ictx->max_entries)
		panic("%s(): Called for uninitialized index context.\n",
				__FUNCTION__);
	/*
	 * @ictx must currently point to real entry thus @ictx->nr_entries must
	 * be greater or equal to 2 as it includes the end entry which cannot
	 * contain any data.
	 */
	if (ictx->nr_entries < 2)
		panic("%s(): ictx->nr_entries < 2\n", __FUNCTION__);
	if (!(ictx->entry->flags & INDEX_ENTRY_NODE)) {
		/*
		 * The current entry is in a leaf node.
		 *
		 * If it is not the last entry in the node return the next
		 * entry in the node.
		 */
		if (ictx->entry_nr < ictx->nr_entries - 2) {
			ictx->entry = ictx->entries[++ictx->entry_nr];
			ntfs_debug("Done (same leaf node).");
			return 0;
		}
		/*
		 * The current entry is in a leaf node and it is the last entry
		 * in the node.  The next entry is the first real entry above
		 * the current node thus keep moving up the B+tree until we
		 * find a real entry.
		 */
		do {
			ntfs_index_context *itmp;

			/* If we are in the index root, we are done. */
			if (ictx->is_root) {
				ntfs_debug("Done (was at last entry already, "
						"returning ENOENT).");
				/* Ensure we return a valid index context. */
				*index_ctx = ictx;
				return ENOENT;
			}
			/* Save the current index context so we can free it. */
			itmp = ictx;
			/* Move up to the parent node. */
			ictx = ictx->up;
			/*
			 * Disconnect the old index context from its path and
			 * free it.
			 */
			ntfs_index_ctx_disconnect(itmp);
			ntfs_index_ctx_put_reuse_single(itmp);
			ntfs_index_ctx_free(itmp);
		} while (ictx->entry_nr == ictx->nr_entries - 1);
		/*
		 * We have reached a node with a real index entry.  Lock it so
		 * we can work on it.
		 */
		err = ntfs_index_ctx_relock(ictx);
		if (err)
			goto err;
		/*
		 * Indicate that the current entry is the next entry and return
		 * the index context containing it to the caller.
		 */
		ictx->is_match = 1;
		*index_ctx = ictx;
		ntfs_debug("Done (upper index node).");
		return 0;
	}
	/*
	 * The current entry is in an index node.
	 *
	 * To find the next entry we need to switch to the next entry in this
	 * node and then descend into the sub-node pointers until a leaf node
	 * is found.  The first entry of that leaf node is the next entry.
	 *
	 * First, unmark this node as being the matching one and switch to the
	 * next entry in this node.
	 */
	ictx->is_match = 0;
	ictx->entry = ictx->entries[++ictx->entry_nr];
	/*
	 * Keep descending into the sub-node pointers until a leaf node is
	 * found.
	 */
	while (ictx->entry->flags & INDEX_ENTRY_NODE) {
		/* Child node present, descend into it. */
		err = ntfs_index_descend_into_child_node(&ictx);
		if (err)
			goto err;
		/* Start at the first index entry in the index node. */
		ictx->entry = ictx->entries[0];
		ictx->entry_nr = 0;
	}
	/*
	 * We have reached the next leaf node.  The next entry is the first
	 * entry in this node which we have already set up to be the current
	 * entry.
	 *
	 * Indicate that the current entry is the one the caller was looking
	 * for and return the index context containing it to the caller.
	 */
	ictx->is_match = 1;
	*index_ctx = ictx;
	ntfs_debug("Done (next leaf node).");
	return 0;
err:
	ntfs_error(ictx->idx_ni->vol->mp, "Failed (error %d).", err);
	/* Ensure we return a valid index context. */
	*index_ctx = ictx;
	return err;
}

/**
 * ntfs_index_entry_mark_dirty - mark an index entry dirty
 * @ictx:	ntfs index context describing the index entry
 *
 * Mark the index entry described by the index entry context @ictx dirty.
 *
 * If the index entry is in the index root attribute, simply mark the mft
 * record containing the index root attribute dirty.  This ensures the mft
 * record, and hence the index root attribute, will be written out to disk
 * later.
 *
 * If the index entry is in an index block belonging to the index allocation
 * attribute, mark the page the index block is in dirty.
 *
 * Locking: - Caller must hold @ictx->idx_ni->lock on the index inode for
 *	      writing.
 *	    - The index context @ictx must be locked.
 */
void ntfs_index_entry_mark_dirty(ntfs_index_context *ictx)
{
	if (!ictx->is_locked)
		panic("%s(): Index context is not locked.\n", __FUNCTION__);
	if (ictx->is_root) {
		if (!ictx->actx)
			panic("%s(): Attribute context is missing from index "
					"context.\n", __FUNCTION__);
		NInoSetMrecNeedsDirtying(ictx->actx->ni);
	} else {
		if (!ictx->upl)
			panic("%s(): Page list is missing from index "
					"context.\n", __FUNCTION__);
		ictx->is_dirty = 1;
	}
}

/**
 * ntfs_insert_index_allocation_attribute - add the index allocation attribute
 * @ni:		base ntfs inode to which the attribute is being added
 * @ctx:	search context describing where to insert the attribute
 * @idx_ni:	index inode for which to add the index allocation attribute
 *
 * Insert a new index allocation attribute in the base ntfs inode @ni at the
 * position indicated by the attribute search context @ctx and add an attribute
 * list attribute entry for it if the inode uses the attribute list attribute.
 *
 * The new attribute type and name are given by the index inode @idx_ni. 
 *
 * If @idx_ni contains a runlist, it is used to create the mapping pairs array
 * for the non-resident index allocation attribute.
 *
 * Return 0 on success and errno on error.
 *
 * WARNING: Regardless of whether success or failure is returned, you need to
 *	    check @ctx->is_error and if true the @ctx is no longer valid, i.e.
 *	    you need to either call ntfs_attr_search_ctx_reinit() or
 *	    ntfs_attr_search_ctx_put() on it.  In that case @ctx->error will
 *	    give the error code for why the mapping of the inode failed.
 *
 * Locking: Caller must hold @idx_ni->lock on the index inode for writing.
 */
static errno_t ntfs_insert_index_allocation_attribute(ntfs_inode *ni,
		ntfs_attr_search_ctx *ctx, ntfs_inode *idx_ni)
{
	ntfs_volume *vol;
	MFT_RECORD *base_m, *m;
	ATTR_RECORD *a;
	ATTR_LIST_ENTRY *al_entry;
	unsigned mp_ofs, mp_size, al_entry_used, al_entry_len;
	unsigned new_al_size, new_al_alloc;
	errno_t err;
	BOOL al_entry_added;

	ntfs_debug("Entering for mft_no 0x%llx, name_len 0x%x.",
			(unsigned long long)ni->mft_no, idx_ni->name_len);
	vol = idx_ni->vol;
	/*
	 * Calculate the offset into the new attribute at which the mapping
	 * pairs array begins.  The mapping pairs array is placed after the
	 * name aligned to an 8-byte boundary which in turn is placed
	 * immediately after the non-resident attribute record itself.
	 */
	mp_ofs = offsetof(ATTR_RECORD, compressed_size) +
			(((idx_ni->name_len << NTFSCHAR_SIZE_SHIFT) + 7) & ~7);
	/* Work out the size for the mapping pairs array. */
	err = ntfs_get_size_for_mapping_pairs(vol,
			idx_ni->rl.elements ? idx_ni->rl.rl : NULL, 0, -1,
			&mp_size);
	if (err)
		panic("%s(): err\n", __FUNCTION__);
	/*
	 * Work out the size for the attribute record.  We simply take the
	 * offset to the mapping pairs array we worked out above and add the
	 * size of the mapping pairs array in bytes aligned to an 8-byte
	 * boundary.  Note we do not need to do the alignment as
	 * ntfs_attr_record_make_space() does it anyway.
	 *
	 * The current implementation of ntfs_attr_lookup() will always return
	 * pointing into the base mft record when an attribute is not found.
	 */
	base_m = ctx->m;
retry:
	if (ni != ctx->ni)
		panic("%s(): ni != ctx->ni\n", __FUNCTION__);
	m = ctx->m;
	a = ctx->a;
	err = ntfs_attr_record_make_space(m, a, mp_ofs + mp_size);
	if (err) {
		ntfs_inode *eni;

		if (err != ENOSPC)
			panic("%s(): err != ENOSPC\n", __FUNCTION__);
		/*
		 * There was not enough space in the mft record to insert the
		 * new attribute record which means we will need to insert it
		 * into an extent mft record.
		 *
		 * To avoid bugs and impossible situations, check that the
		 * attribute is not already the only attribute in the mft
		 * record otherwise moving it would not give us anything.
		 */
		if (ntfs_attr_record_is_only_one(m, a))
			panic("%s(): ntfs_attr_record_is_only_one()\n",
					__FUNCTION__);
		/*
		 * Before we can allocate an extent mft record, we need to
		 * ensure that the inode has an attribute list attribute.
		 */
		if (!NInoAttrList(ni)) {
			err = ntfs_attr_list_add(ni, m, NULL);
			if (err) {
				ntfs_error(vol->mp, "Failed to add attribute "
						"list attribute to mft_no "
						"0x%llx (error %d).",
						(unsigned long long)ni->mft_no,
						err);
				return err;
			}
			/*
			 * Adding the attribute list attribute may have
			 * generated enough space in the base mft record to
			 * fit the attribute so try again.
			 */
			ntfs_attr_search_ctx_reinit(ctx);
			err = ntfs_attr_lookup(AT_INDEX_ALLOCATION,
					idx_ni->name, idx_ni->name_len, 0,
					NULL, 0, ctx);
			if (err == ENOENT) {
				/*
				 * The current implementation of
				 * ntfs_attr_lookup() will always return
				 * pointing into the base mft record when an
				 * attribute is not found.
				 */
				if (m != ctx->m)
					panic("%s(): m != ctx->m\n",
							__FUNCTION__);
				goto retry;
			}
			/*
			 * We cannot have found the attribute as we have
			 * exclusive access and know that it does not exist
			 * already.
			 */
			if (!err)
				panic("%s() !err\n", __FUNCTION__);
			/*
			 * Something has gone wrong.  Note we have to bail out
			 * as a failing attribute lookup indicates corruption
			 * and/or disk failure and/or not enough memory all of
			 * which would prevent us from rolling back the
			 * attribute list attribute addition.
			 */
			ntfs_error(vol->mp, "Failed to add index allocation "
					"attribute to mft_no 0x%llx because "
					"looking up the attribute failed "
					"(error %d).",
					(unsigned long long)ni->mft_no, err);
			return err;
		}
		/*
		 * We now need to allocate a new extent mft record, attach it
		 * to the base ntfs inode and set up the search context to
		 * point to it, then insert the new attribute into it.
		 */
		err = ntfs_mft_record_alloc(vol, NULL, NULL, ni, &eni, &m, &a);
		if (err) {
			ntfs_error(vol->mp, "Failed to add index allocation "
					"attribute to mft_no 0x%llx because "
					"allocating a new extent mft record "
					"failed (error %d).",
					(unsigned long long)ni->mft_no, err);
			/*
			 * TODO: If we added the attribute list attribute above
			 * we could now remove it again but this may require
			 * moving attributes back into the base mft record so
			 * is not a trivial amount of work and in the end it
			 * does not really matter if we leave an inode with an
			 * attribute list attribute that does not really need
			 * it.  Especially so since this is error handling and
			 * it is not an error to have an attribute list
			 * attribute when not strictly required.
			 */
			return err;
		}
		ctx->ni = eni;
		ctx->m = m;
		ctx->a = a;
		/*
		 * Make space for the new attribute.  This cannot fail as we
		 * now have an empty mft record which by definition can hold
		 * a maximum size resident attribute record.
		 */
		err = ntfs_attr_record_make_space(m, a, mp_ofs + mp_size);
		if (err)
			panic("%s(): err (ntfs_attr_record_make_space())\n",
					__FUNCTION__);
	}
	/*
	 * Now setup the new attribute record.  The entire attribute has been
	 * zeroed and the length of the attribute record has been set up by
	 * ntfs_attr_record_make_space().
	 */
	a->type = AT_INDEX_ALLOCATION;
	a->non_resident = 1;
	a->name_length = idx_ni->name_len;
	a->name_offset = const_cpu_to_le16(offsetof(ATTR_RECORD,
			compressed_size));
	a->instance = m->next_attr_instance;
	/*
	 * Increment the next attribute instance number in the mft record as we
	 * consumed the old one.
	 */
	m->next_attr_instance = cpu_to_le16(
			(le16_to_cpu(m->next_attr_instance) + 1) & 0xffff);
	a->highest_vcn = cpu_to_sle64((idx_ni->allocated_size >>
			vol->cluster_size_shift) - 1);
	a->mapping_pairs_offset = cpu_to_le16(mp_ofs);
	lck_spin_lock(&idx_ni->size_lock);
	a->allocated_size = cpu_to_sle64(idx_ni->allocated_size);
	a->data_size = cpu_to_sle64(idx_ni->data_size);
	a->initialized_size = cpu_to_sle64(idx_ni->initialized_size);
	lck_spin_unlock(&idx_ni->size_lock);
	/* Copy the attribute name into place. */
	if (idx_ni->name_len)
		memcpy((u8*)a + offsetof(ATTR_RECORD, compressed_size),
				idx_ni->name, idx_ni->name_len <<
				NTFSCHAR_SIZE_SHIFT);
	/* Generate the mapping pairs array into place. */
	err = ntfs_mapping_pairs_build(vol, (s8*)a + mp_ofs, mp_size,
			idx_ni->rl.elements ? idx_ni->rl.rl : NULL, 0,
			-1, NULL);
	if (err)
		panic("%s(): err (ntfs_mapping_pairs_build())\n", __FUNCTION__);
	/*
	 * If the inode does not use the attribute list attribute we are done.
	 *
	 * If the inode uses the attribute list attribute (including the case
	 * where we just created it), we need to add an attribute list
	 * attribute entry for the attribute.
	 */
	if (!NInoAttrList(ni))
		goto done;
	/* Add an attribute list attribute entry for the inserted attribute. */
	al_entry = ctx->al_entry;
	al_entry_used = offsetof(ATTR_LIST_ENTRY, name) +
			(idx_ni->name_len << NTFSCHAR_SIZE_SHIFT);
	al_entry_len = (al_entry_used + 7) & ~7;
	new_al_size = ni->attr_list_size + al_entry_len;
	/* Out of bounds checks. */
	if ((u8*)al_entry < ni->attr_list ||
			(u8*)al_entry > ni->attr_list + new_al_size ||
			(u8*)al_entry + al_entry_len >
			ni->attr_list + new_al_size) {
		/* Inode is corrupt. */
		ntfs_error(vol->mp, "Mft_no 0x%llx is corrupt.  Run chkdsk.",
				(unsigned long long)ni->mft_no);
		err = EIO;
		goto undo;
	}
	err = ntfs_attr_size_bounds_check(vol, AT_ATTRIBUTE_LIST, new_al_size);
	if (err) {
		if (err == ERANGE) {
			ntfs_error(vol->mp, "Cannot insert attribute into "
					"mft_no 0x%llx because the attribute "
					"list attribute would become too "
					"large.  You need to defragment your "
					"volume and then try again.",
					(unsigned long long)ni->mft_no);
			err = ENOSPC;
		} else {
			ntfs_error(vol->mp, "Attribute list attribute is "
					"unknown on the volume.  The volume "
					"is corrupt.  Run chkdsk.");
			NVolSetErrors(vol);
			err = EIO;
		}
		goto undo;
	}
	new_al_alloc = (new_al_size + NTFS_ALLOC_BLOCK - 1) &
			~(NTFS_ALLOC_BLOCK - 1);
	/*
	 * Reallocate the memory buffer if needed and create space for the new
	 * entry.
	 */
	if (new_al_alloc > ni->attr_list_alloc) {
		u8 *tmp, *al, *al_end;
		unsigned al_entry_ofs;

		tmp = OSMalloc(new_al_alloc, ntfs_malloc_tag);
		if (!tmp) {
			ntfs_error(vol->mp, "Not enough memory to extend "
					"attribute list attribute of mft_no "
					"0x%llx.",
					(unsigned long long)ni->mft_no);
			err = ENOMEM;
			goto undo;
		}
		al = ni->attr_list;
		al_entry_ofs = (unsigned)((u8*)al_entry - al);
		al_end = al + ni->attr_list_size;
		memcpy(tmp, al, al_entry_ofs);
		if ((u8*)al_entry < al_end)
			memcpy(tmp + al_entry_ofs + al_entry_len,
					al + al_entry_ofs,
					ni->attr_list_size - al_entry_ofs);
		al_entry = ctx->al_entry = (ATTR_LIST_ENTRY*)(tmp +
				al_entry_ofs);
		OSFree(ni->attr_list, ni->attr_list_alloc, ntfs_malloc_tag);
		ni->attr_list_alloc = new_al_alloc;
		ni->attr_list = tmp;
	} else if ((u8*)al_entry < ni->attr_list + ni->attr_list_size)
		memmove((u8*)al_entry + al_entry_len, al_entry,
				ni->attr_list_size - ((u8*)al_entry -
				ni->attr_list));
	ni->attr_list_size = new_al_size;
	/* Set up the attribute list entry. */
	al_entry->type = AT_INDEX_ALLOCATION;
	al_entry->length = cpu_to_le16(al_entry_len);
	al_entry->name_length = idx_ni->name_len;
	al_entry->name_offset = offsetof(ATTR_LIST_ENTRY, name);
	al_entry->lowest_vcn = 0;
	al_entry->mft_reference = MK_LE_MREF(ctx->ni->mft_no, ctx->ni->seq_no);
	al_entry->instance = a->instance;
	/* Copy the attribute name into place. */
	if (idx_ni->name_len)
		memcpy((u8*)&al_entry->name, idx_ni->name,
				idx_ni->name_len << NTFSCHAR_SIZE_SHIFT);
	/* For tidyness, zero any unused space. */
	if (al_entry_len != al_entry_used) {
		if (al_entry_len < al_entry_used)
			panic("%s(): al_entry_len < al_entry_used\n",
					__FUNCTION__);
		memset((u8*)al_entry + al_entry_used, 0,
				al_entry_len - al_entry_used);
	}
	/*
	 * Extend the attribute list attribute and copy in the modified
	 * value from the cache.
	 */
	err = ntfs_attr_list_sync_extend(ni, base_m,
			(unsigned)((u8*)al_entry - ni->attr_list), ctx);
	if (err) {
		ntfs_error(vol->mp, "Failed to extend attribute list "
				"attribute of mft_no 0x%llx (error %d).",
				(unsigned long long)ni->mft_no, err);
		al_entry_added = TRUE;
		goto undo_al;
	}
done:
	ntfs_debug("Done.");
	return 0;
undo:
	al_entry_added = FALSE;
undo_al:
	/*
	 * Need to remove the attribute again or free the extent mft record if
	 * there are no attributes remaining in it.
	 */
	if (m == base_m || !ntfs_attr_record_is_only_one(m, a)) {
		ntfs_attr_record_delete_internal(m, a);
		/*
		 * If the attribute was not in the base mft record mark the
		 * extent mft record dirty so it gets written out later.  If
		 * the attribute was in the base mft record it will be marked
		 * dirty later.
		 *
		 * We also unmap the extent mft record and we set @ctx->ni to
		 * equal the base inode @ni so that the search context is
		 * initialized from scratch or simply freed if the caller
		 * reinitializes or releases the search context respectively.
		 */
		if (m != base_m) {
			NInoSetMrecNeedsDirtying(ctx->ni);
			ntfs_extent_mft_record_unmap(ctx->ni);
			ctx->ni = ni;
		}
	} else {
		errno_t err2;
		BOOL al_needed;

		err2 = ntfs_extent_mft_record_free(ni, ctx->ni, m);
		if (err2) {
			/*
			 * Ignore the error as we just end up with an unused
			 * mft record that is marked in use.
			 */
			ntfs_error(vol->mp, "Failed to free extent mft_no "
					"0x%llx (error %d).  Unmount and run "
					"chkdsk to recover the lost inode.",
					(unsigned long long)ctx->ni->mft_no,
					err2);
			NVolSetErrors(vol);
			/*
			 * Relese the extent mft record after dirtying it thus
			 * simulating the effect of freeing it.
			 */
			NInoSetMrecNeedsDirtying(ctx->ni);
			ntfs_extent_mft_record_unmap(ctx->ni);
		}
		/*
		 * The attribute search context still points to the no longer
		 * mapped extent inode thus we need to change it to point to
		 * the base inode instead so the context can be reinitialized
		 * or released safely.
		 */
		ctx->ni = ni;
		/*
		 * Check the attribute list attribute.  If there are no other
		 * attribute list attribute entries referencing extent mft
		 * records delete the attribute list attribute altogether.
		 *
		 * If this fails it does not matter as we simply retain the
		 * attribute list attribute so we ignore the error and go on to
		 * delete the attribute list attribute entry instead.
		 *
		 * If there are other attribute list attribute entries
		 * referencing extent mft records we still need the attribute
		 * list attribute thus we go on to delete the attribute list
		 * entry corresponding to the attribute record we just deleted
		 * by freeing its extent mft record.
		 */
		err2 = ntfs_attr_list_is_needed(ni,
				al_entry_added ? al_entry : NULL, &al_needed);
		if (err2)
			ntfs_warning(vol->mp, "Failed to determine if "
					"attribute list attribute of mft_no "
					"0x%llx if still needed (error %d).  "
					"Assuming it is still needed and "
					"continuing.",
					(unsigned long long)ni->mft_no, err2);
		else if (!al_needed) {
			/*
			 * No more extent mft records are in use.  Delete the
			 * attribute list attribute.
			 */
			ntfs_attr_search_ctx_reinit(ctx);
			err2 = ntfs_attr_list_delete(ni, ctx);
			if (!err2) {
				/*
				 * We deleted the attribute list attribute and
				 * this will have updated the base inode
				 * appropriately thus we have restored
				 * everything as it was before.
				 */
				return err;
			}
			ntfs_warning(vol->mp, "Failed to delete attribute "
					"list attribute of mft_no 0x%llx "
					"(error %d).  Continuing using "
					"alternative error recovery method.",
					(unsigned long long)ni->mft_no, err2);
		}
	}
	/*
	 * Both @ctx and @ni are now invalid and cannot be used any more which
	 * is fine as we have finished dealing with the attribute record.
	 *
	 * We now need to delete the corresponding attribute list attribute
	 * entry if we created it.
	 *
	 * Then we need to rewrite the attribute list attribute again because
	 * ntfs_attr_list_sync_extend() may have left it in an indeterminate
	 * state.
	 */
	if (al_entry_added) {
		errno_t err2;

		ntfs_attr_list_entry_delete(ni, al_entry);
		ntfs_attr_search_ctx_reinit(ctx);
		err2 = ntfs_attr_list_sync_shrink(ni, 0, ctx);
		if (err2) {
			ntfs_error(vol->mp, "Failed to restore attribute list "
					"attribute in base mft_no 0x%llx "
					"(error %d).  Leaving inconsistent "
					"metadata.  Unmount and run chkdsk.",
					(unsigned long long)ni->mft_no, err2);
			NVolSetErrors(vol);
		}
	}
	/* Make sure any changes are written out. */
	NInoSetMrecNeedsDirtying(ni);
	return err;
}

/**
 * ntfs_index_block_lay_out - lay out an index block into a memory buffer
 * @idx_ni:	index inode to which the index block will belong
 * @vcn:	vcn of index block
 * @ia:		destination buffer of size >= @idx_ni->block_size bytes
 *
 * Lay out an empty index allocation block with the @vcn into the buffer @ia.
 * The index inode @idx_ni is needed because we need to know the size of an
 * index block and the @vcn is needed because we need to record it in the index
 * block.
 *
 * Locking: Caller must hold @idx_ni->lock on the index inode for writing.
 */
static void ntfs_index_block_lay_out(ntfs_inode *idx_ni, VCN vcn,
		INDEX_BLOCK *ia)
{
	INDEX_HEADER *ih;
	INDEX_ENTRY *ie;
	u32 ie_ofs;

	bzero(ia, idx_ni->block_size);
	ia->magic = magic_INDX;
	ia->usa_ofs = const_cpu_to_le16(sizeof(INDEX_BLOCK));
	ia->usa_count = cpu_to_le16(1 + (idx_ni->block_size / NTFS_BLOCK_SIZE));
	/* Set the update sequence number to 1. */
	*(le16*)((u8*)ia + le16_to_cpu(ia->usa_ofs)) = cpu_to_le16(1);
	ia->index_block_vcn = cpu_to_sle64(vcn);
	ih = &ia->index;
	ie_ofs = (sizeof(INDEX_HEADER) +
			(le16_to_cpu(ia->usa_count) << 1) + 7) & ~7;
	ih->entries_offset = cpu_to_le32(ie_ofs);
	ih->index_length = cpu_to_le32(ie_ofs + sizeof(INDEX_ENTRY_HEADER));
	ih->allocated_size = cpu_to_le32(idx_ni->block_size -
			offsetof(INDEX_BLOCK, index));
	ih->flags = LEAF_NODE;
	ie = (INDEX_ENTRY*)((u8*)ih + ie_ofs);
	ie->length = const_cpu_to_le16(sizeof(INDEX_ENTRY_HEADER));
	ie->flags = INDEX_ENTRY_END;
}

/**
 * ntfs_index_block_alloc - allocate and return an index allocation block
 * @ictx:	index context of the index for which to allocate a block
 * @dst_vcn:	pointer in which to return the VCN of the allocated index block
 * @dst_ia:	pointer in which to return the allocated index block
 * @dst_upl_ofs: pointer in which to return the mapped address of the page data
 * @dst_upl:	pointer in which to return the page list containing @ia
 * @dst_pl:	pointer in which to return the array of pages containing @ia
 * @dst_addr:	pointer in which to return the mapped address
 *
 * Allocate an index allocation block for the index described by the index
 * context @ictx and return it in *@dst_ia.  *@dst_vcn, *@dst_upl_ofs,
 * *@dst_upl, *@dst_pl, and *@dst_addr are set to the VCN of the allocated
 * index block and the mapped page list and array of pages containing the
 * returned index block respectively.
 *
 * Return 0 on success and errno on error.  On error *@dst_vcn, *@dst_ia,
 * *@dst_upl_ofs, *@dst_upl, *@dst_pl, and *@dst_addr are left untouched.
 *
 * Locking: - Caller must hold @ictx->idx_ni->lock on the index inode for
 *	      writing.
 *	    - All of the index contexts in the index must be unlocked (this
 *	      includes @ictx, i.e. @ictx must not be locked).
 */
static errno_t ntfs_index_block_alloc(ntfs_index_context *ictx, VCN *dst_vcn,
		INDEX_BLOCK **dst_ia, s64 *dst_upl_ofs, upl_t *dst_upl,
		upl_page_info_array_t *dst_pl, u8 **dst_addr)
{
	s64 bmp_pos, end_pos, init_size, upl_ofs;
	ntfs_inode *bmp_ni, *idx_ni = ictx->idx_ni;
	upl_t upl;
	upl_page_info_array_t pl;
	u8 *bmp, *bmp_end;
	INDEX_ALLOCATION *ia;
	errno_t err, err2;
	lck_rw_type_t lock;
	le16 usn;
	BOOL have_resized;
	u8 bit;

	ntfs_debug("Entering for inode 0x%llx.",
			(unsigned long long)idx_ni->mft_no);
	/*
	 * Get the index bitmap inode.
	 *
	 * Note we do not lock the bitmap inode as the caller holds an
	 * exclusive lock on the index inode thus the bitmap cannot change
	 * under us as the only places the index bitmap is modified is as a
	 * side effect of a locked index inode.
	 *
	 * The inode will be locked later if/when we are going to modify its
	 * size to ensure any relevant VM activity that may be happening at the
	 * same time is excluded.
	 *
	 * We need to take the ntfs inode lock on the bitmap inode for writing.
	 * This causes a complication because there is a valid case in which we
	 * can recurse (once) back into ntfs_index_block_alloc() by calling:
	 * ntfs_attr_resize(bmp_ni) ->
	 *   ntfs_index_move_root_to_allocation_block() ->
	 *     ntfs_index_block_alloc()
	 * In this case we have to not take the lock again or we will deadlock
	 * (or with lock debugging enabled the kernel will panic()).  Thus we
	 * set @ictx->bmp_is_locked so that when we get back here we know not
	 * to take the lock again.
	 */
	lock = 0;
	if (!ictx->bmp_is_locked) {
		lock = LCK_RW_TYPE_EXCLUSIVE;
		ictx->bmp_is_locked = 1;
	}
	err = ntfs_attr_inode_get(ictx->base_ni, AT_BITMAP, idx_ni->name,
			idx_ni->name_len, FALSE, lock, &bmp_ni);
	if (err) {
		ntfs_error(idx_ni->vol->mp, "Failed to get index bitmap inode "
				"(error %d).", err);
		if (lock)
			ictx->bmp_is_locked = 0;
		goto err;
	}
	/* Find the first zero bit in the index bitmap. */
	bmp_pos = 0;
	lck_spin_lock(&bmp_ni->size_lock);
	end_pos = bmp_ni->initialized_size;
	lck_spin_unlock(&bmp_ni->size_lock);
	for (bmp_pos = 0; bmp_pos < end_pos;) {
		err = ntfs_page_map(bmp_ni, bmp_pos, &upl, &pl, &bmp, TRUE);
		if (err) {
			ntfs_error(idx_ni->vol->mp, "Failed to read index "
					"bitmap (error %d).", err);
			goto put_err;
		}
		bmp_end = bmp + PAGE_SIZE;
		if (bmp_pos + PAGE_SIZE > end_pos)
			bmp_end = bmp + (end_pos - bmp_pos);
		/* Check the next bit(s). */
		for (; bmp < bmp_end; bmp++, bmp_pos++) {
			if (*bmp == 0xff)
				continue;
			/*
			 * TODO: There does not appear to be a ffz() function
			 * in the kernel. )-:  If/when the kernel has an ffz()
			 * function, switch the below code to use it.
			 *
			 * So emulate "ffz(x)" using "ffs(~x) - 1" which gives
			 * the same result but incurs extra CPU overhead.
			 */
			bit = ffs(~(unsigned)*bmp) - 1;
			if (bit < 8)
				goto allocated_bit;
		}
		ntfs_page_unmap(bmp_ni, upl, pl, FALSE);
	}
	/*
	 * There are no zero bits in the initialized part of the bitmap.  Thus
	 * we extend it by 8 bytes and allocate the first bit of the extension.
	 */
	bmp_pos = end_pos;
	lck_spin_lock(&bmp_ni->size_lock);
	end_pos = bmp_ni->data_size;
	lck_spin_unlock(&bmp_ni->size_lock);
	/* If we are exceeding the bitmap size need to extend it. */
	have_resized = FALSE;
	if (bmp_pos + 8 >= end_pos) {
		ntfs_debug("Extending index bitmap, old size 0x%llx, "
				"requested size 0x%llx.",
				(unsigned long long)end_pos,
				(unsigned long long)end_pos + 8);
		err = ntfs_attr_resize(bmp_ni, end_pos + 8, 0, ictx);
		if (err) {
			ntfs_error(idx_ni->vol->mp, "Failed to extend index "
					"bitmap (error %d).", err);
			goto put_err;
		}
		have_resized = TRUE;
	}
	/*
	 * Get the page containing the bit we are allocating.  Note this has to
	 * happen before we call ntfs_attr_set_initialized_size() as the latter
	 * only sets the initialized size without zeroing the area between the
	 * old initialized size and the new one thus we need to map the page
	 * now so that its tail is zeroed due to the old value of the
	 * initialized size.
	 */
	err = ntfs_page_map(bmp_ni, bmp_pos & ~PAGE_MASK_64, &upl, &pl, &bmp,
			TRUE);
	if (err) {
		ntfs_error(idx_ni->vol->mp, "Failed to read index bitmap "
				"(error %d).", err);
		/*
		 * There is no need to resize the bitmap back to its old size.
		 * It does not change the metadata consistency to have a bigger
		 * bitmap.
		 */
		goto put_err;
	}
	bmp += (unsigned)bmp_pos & PAGE_MASK;
	/*
	 * Extend the initialized size if the bitmap is non-resident.  If it is
	 * resident this was already done by the ntfs_attr_resize() call.
	 */
	if (NInoNonResident(bmp_ni)) {
#ifdef DEBUG
		lck_spin_lock(&bmp_ni->size_lock);
		init_size = bmp_ni->initialized_size;
		lck_spin_unlock(&bmp_ni->size_lock);
		ntfs_debug("Setting initialized size of index bitmap, old "
				"size 0x%llx, requested size 0x%llx.",
				(unsigned long long)init_size,
				(unsigned long long)end_pos + 8);
#endif
		err = ntfs_attr_set_initialized_size(bmp_ni, end_pos + 8);
		if (err) {
			ntfs_error(idx_ni->vol->mp, "Failed to update "
					"initialized size of index bitmap "
					"(error %d).", err);
			ntfs_page_unmap(bmp_ni, upl, pl, FALSE);
			goto put_err;
		}
	}
	/* Finally, allocate the bit in the page. */
	bit = 0;
	/*
	 * If we used ntfs_attr_resize() to extend the bitmap attribute it is
	 * possible that this caused the index root node to be moved out to an
	 * index allocation block which very likely will have allocated the
	 * very bit we want to allocate so we test for this case and if it has
	 * happened we allocate the next bit along which must be free or
	 * something has gone wrong.
	 */
	if (have_resized && *bmp & 1) {
		if (*bmp & 2)
			panic("%s(): *bmp & 2\n", __FUNCTION__);
		bit++;
	}
allocated_bit:
	*bmp |= 1 << bit;
	ntfs_page_unmap(bmp_ni, upl, pl, TRUE);
	/* Set @bmp_pos to the allocated index bitmap bit. */
	bmp_pos = (bmp_pos << 3) + bit;
	/*
	 * If we are caching the last set bit in the bitmap in the index inode
	 * and we allocated beyond the last set bit, update the cached value.
	 */
	if (idx_ni->last_set_bit >= 0 && bmp_pos > idx_ni->last_set_bit)
		idx_ni->last_set_bit = bmp_pos;
	ntfs_debug("Allocated index bitmap bit 0x%llx.",
			(unsigned long long)bmp_pos);
	/*
	 * We are done with the bitmap and have the index allocation to go.
	 *
	 * If the allocated bit is outside the data size need to extend it.
	 */
	lck_spin_lock(&idx_ni->size_lock);
	end_pos = idx_ni->data_size;
	lck_spin_unlock(&idx_ni->size_lock);
	if (bmp_pos >= end_pos >> idx_ni->block_size_shift) {
		ntfs_debug("Extending index allocation, old size 0x%llx, "
				"requested size 0x%llx.", (unsigned long long)
				end_pos, (unsigned long long)(bmp_pos + 1) <<
				idx_ni->block_size_shift);
		err = ntfs_attr_resize(idx_ni, (bmp_pos + 1) <<
				idx_ni->block_size_shift, 0, ictx);
		if (err) {
			ntfs_error(idx_ni->vol->mp, "Failed to extend index "
					"allocation (error %d).", err);
			goto alloc_err;
		}
	}
	/*
	 * Map the page containing the allocated index block.  As above for the
	 * bitmap we need to get the page before we set the initialized size so
	 * that the tail of the page is zeroed for us.
	 */
	upl_ofs = (bmp_pos << idx_ni->block_size_shift) & ~PAGE_MASK_64;
	err = ntfs_page_map(idx_ni, upl_ofs, &upl, &pl, &bmp, TRUE);
	if (err) {
		ntfs_error(idx_ni->vol->mp, "Failed to read index allocation "
				"block (error %d).", err);
		/*
		 * There is no need to truncate the index allocation back to
		 * its old size.  It does not change the metadata consistency
		 * to have a bigger index allocation.
		 */
		goto alloc_err;
	}
	/* Extend the initialized size if needed. */
	lck_spin_lock(&idx_ni->size_lock);
	init_size = idx_ni->initialized_size;
	lck_spin_unlock(&idx_ni->size_lock);
	if (bmp_pos >= init_size >> idx_ni->block_size_shift) {
		ntfs_debug("Setting initialized size of index allocation, old "
				"size 0x%llx, requested size 0x%llx.",
				(unsigned long long)init_size,
				(unsigned long long)(bmp_pos + 1) <<
				idx_ni->block_size_shift);
		err = ntfs_attr_set_initialized_size(idx_ni,
				(bmp_pos + 1) << idx_ni->block_size_shift);
		if (err) {
			ntfs_error(idx_ni->vol->mp, "Failed to update "
					"initialized size of index allocation "
					"(error %d).", err);
			goto unm_err;
		}
	}
	if (lock) {
		ictx->bmp_is_locked = 0;
		lck_rw_unlock_exclusive(&bmp_ni->lock);
	}
	(void)vnode_put(bmp_ni->vn);
	/* Lay out an empty index block into the allocated space. */
	ia = (INDEX_ALLOCATION*)(bmp + (((unsigned)bmp_pos <<
			idx_ni->block_size_shift) & PAGE_MASK));
	/* Preserve the update sequence number across the layout. */
	usn = 0;
	if (le16_to_cpu(ia->usa_ofs) < NTFS_BLOCK_SIZE - sizeof(u16))
		usn = *(le16*)((u8*)ia + le16_to_cpu(ia->usa_ofs));
	/* Calculate the index block vcn from the index block number. */
	*dst_vcn = bmp_pos = bmp_pos << idx_ni->block_size_shift >>
			idx_ni->vcn_size_shift;
	ntfs_index_block_lay_out(idx_ni, bmp_pos, ia);
	if (usn && usn != const_cpu_to_le16(0xffff))
		*(le16*)((u8*)ia + le16_to_cpu(ia->usa_ofs)) = usn;
	*dst_ia = ia;
	*dst_upl_ofs = upl_ofs;
	*dst_upl = upl;
	*dst_pl = pl;
	*dst_addr = bmp;
	ntfs_debug("Done (allocated index block with vcn 0x%llx.",
			(unsigned long long)bmp_pos);
	return 0;
unm_err:
	ntfs_page_unmap(idx_ni, upl, pl, FALSE);
alloc_err:
	/* Free the index bitmap bit that we allocated. */
	err2 = ntfs_bitmap_clear_bit(bmp_ni, bmp_pos);
	if (err2) {
		ntfs_error(idx_ni->vol->mp, "Failed to undo index block "
				"allocation in index bitmap (error %d).  "
				"Leaving inconsistent metadata.  Run chkdsk.",
				err2);
		NVolSetErrors(idx_ni->vol);
	}
put_err:
	if (lock) {
		ictx->bmp_is_locked = 0;
		lck_rw_unlock_exclusive(&bmp_ni->lock);
	}
	(void)vnode_put(bmp_ni->vn);
err:
	ntfs_error(idx_ni->vol->mp, "Failed (error %d).", err);
	return err;
}

/**
 * ntfs_index_ctx_disconnect_reinit - disconnect and reinit an index context
 * @ictx:	index context to disconnect
 *
 * Disconnect the index context @ictx from its tree path and reinitialize its
 * @up and @down pointers to point to itself.
 *
 * Locking: - Caller must hold @ictx->idx_ni->lock on the index inode.
 */
static inline void ntfs_index_ctx_disconnect_reinit(ntfs_index_context *ictx)
{
	ntfs_index_ctx_disconnect(ictx);
	ictx->down = ictx->up = ictx;
}

/**
 * ntfs_index_move_root_to_allocation_block - move index root to allocation
 * @ictx:	index lookup context describing index root to move
 *
 * Move the index root to an index allocation block in the process switching
 * the index from a small to a large index if necessary.
 *
 * If no index allocation and/or bitmap attributes exist they are created.
 *
 * An index block is then allocated and if necessary the index bitmap and/or
 * the allocation attributes are extended in the process.
 *
 * Finally all index entries contained in the index root attribute are moved
 * into the allocated index block in the index allocation attribute.
 *
 * We need to potentially create the index allocation and/or bitmap attributes
 * so we can move the entries from the index root attribute to the index
 * allocation attribute and then shrink the index root attribute.  However,
 * there is not enough space in the mft record to do this.  Also we already
 * have the index root attribute looked up so it makes sense to deal with it
 * first.
 *
 * Thus, if there is no index allocation attribute, we allocate the space to be
 * used by the index allocation attribute and setup the directory inode for a
 * large index including the allocated space but leaving the initialized size
 * to zero.  We then map and lock the first page containing the now allocated
 * first index block and move the index entries from the index root into it.
 * We then shrink the index root and set it up to point to the allocated index
 * block.
 *
 * Having shrunk the index root attribute there is now hopefully enough space
 * in the mft record to create the index allocation attribute and the index
 * bitmap attribute in the mft record and the conversion is complete.
 *
 * If there is not enough space we create the index allocation and/or index
 * bitmap attributes in an extent mft record.  TODO: This is not implemented
 * yet.
 *
 * If there is an index allocation attribute already, we allocate a temporary
 * buffer to hold the index block and then copy from there into the allocated
 * index block later.
 *
 * Return 0 on success and errno on error.  On error the index context is no
 * longer usable and must be released or reinitialized.
 *
 * Locking: - Caller must hold @ictx->idx_ni->lock on the index inode for
 *	      writing.
 *	    - The index context @ictx must be locked.
 */
errno_t ntfs_index_move_root_to_allocation_block(ntfs_index_context *ictx)
{
	VCN vcn;
	ntfs_inode *base_ni, *idx_ni;
	ntfs_volume *vol;
	ntfs_index_context *ir_ictx;
	upl_t upl;
	upl_page_info_array_t pl;
	INDEX_ALLOCATION *ia;
	INDEX_HEADER *ih;
	INDEX_ROOT *ir;
	INDEX_ENTRY *ie;
	ntfs_attr_search_ctx *actx;
	MFT_RECORD *m;
	u32 u, ia_ie_ofs, ir_ie_ofs, clusters = 0;
	errno_t err, err2;
	struct {
		unsigned is_large_index:1;
	} old;
	BOOL need_ubc_setsize = FALSE;

	ntfs_debug("Entering.");
	if (!ictx->is_locked)
		panic("%s(): !ictx->is_locked\n", __FUNCTION__);
	base_ni = ictx->base_ni;
	idx_ni = ictx->idx_ni;
	vol = base_ni->vol;
	/*
	 * The current index context is going to turn from describing the index
	 * root to describing the newly allocated index block.  Thus, allocate
	 * a new index context for the emptied index root.
	 */
	ir_ictx = ntfs_index_ctx_get(idx_ni);
	if (!ir_ictx)
		return ENOMEM;
	ir_ictx->max_entries = ictx->max_entries;
	ir_ictx->entries = OSMalloc(ir_ictx->max_entries * sizeof(INDEX_ENTRY*),
			ntfs_malloc_tag);
	if (!ir_ictx->entries) {
		err = ENOMEM;
		goto err;
	}
	/*
	 * If there is no index allocation attribute, we need to allocate an
	 * index block worth of clusters.  Thus if the cluster size is less
	 * than the index block size the number of clusters we need to allocate
	 * is the index block size divided by the cluster size and if the
	 * cluster size is greater than or equal to the index block size we
	 * simply allocate one cluster.
	 *
	 * Otherwise we allocate a temporary buffer for the index block.
	 */
	if (!NInoIndexAllocPresent(idx_ni)) {
		clusters = idx_ni->block_size >> vol->cluster_size_shift;
		if (!clusters)
			clusters = 1;
		/*
		 * We cannot lock the runlist as we are holding the mft record
		 * lock.  That is fortunately ok as we also have @idx_ni->lock
		 * on the index inode and there is no index allocation
		 * attribute at present so no-one can be using the runlist yet.
		 */
		err = ntfs_cluster_alloc(vol, 0, clusters, -1, DATA_ZONE, TRUE,
				&idx_ni->rl);
		if (err) {
			ntfs_error(vol->mp, "Failed to allocate %u clusters "
					"for the index allocation block "
					"(error %d).", (unsigned)clusters,
					err);
			goto err;
		}
		/* Allocate/get the first page and zero it out. */
		err = ntfs_page_grab(idx_ni, 0, &upl, &pl, (u8**)&ia, TRUE);
		if (err) {
			ntfs_error(vol->mp, "Failed to grab first page.");
			goto page_err;
		}
		bzero(ia, PAGE_SIZE);
	} else {
		/* Allocate a temporary buffer and zero it out. */
		ia = OSMalloc(idx_ni->block_size, ntfs_malloc_tag);
		if (!ia) {
			err = ENOMEM;
			goto err;
		}
		bzero(ia, idx_ni->block_size);
		upl = NULL;
		pl = NULL;
	}
	/*
	 * Set up the index block and copy the index entries from the index
	 * root.
	 */
	ia->magic = magic_INDX;
	ia->usa_ofs = const_cpu_to_le16(sizeof(INDEX_BLOCK));
	ia->usa_count = cpu_to_le16(1 + (idx_ni->block_size / NTFS_BLOCK_SIZE));
	/* Set the update sequence number to 1. */
	*(le16*)((u8*)ia + le16_to_cpu(ia->usa_ofs)) = cpu_to_le16(1);
	ih = &ia->index;
	ia_ie_ofs = (sizeof(INDEX_HEADER) +
			(le16_to_cpu(ia->usa_count) << 1) + 7) & ~7;
	ih->entries_offset = cpu_to_le32(ia_ie_ofs);
	/* Work out the size of the index entries in the index root. */
	ir = ictx->ir;
	ir_ie_ofs = le32_to_cpu(ir->index.entries_offset);
	/*
	 * FIXME: Should we scan through the index root looking for the last
	 * entry and use that to determine the size instead?  Then we could fix
	 * any space being wasted due to a too long index for its entries.
	 */
	u = le32_to_cpu(ir->index.index_length) - ir_ie_ofs;
	ih->index_length = cpu_to_le32(ia_ie_ofs + u);
	ih->allocated_size = cpu_to_le32(idx_ni->block_size -
			offsetof(INDEX_BLOCK, index));
	ih->flags = LEAF_NODE;
	ie = (INDEX_ENTRY*)((u8*)&ir->index + ir_ie_ofs);
	memcpy((u8*)ih + ia_ie_ofs, ie, u);
	/*
	 * If the index root is a large index, then the index block is an index
	 * node, i.e. not a leaf node.
	 */
	if (ir->index.flags & LARGE_INDEX) {
		old.is_large_index = 1;
		ih->flags |= INDEX_NODE;
	}
	/*
	 * Set up the index context to point into the index allocation block
	 * instead of into the index root.
	 */
	ictx->entry = (INDEX_ENTRY*)((u8*)ih + ia_ie_ofs +
			((u8*)ictx->entry - (u8*)ie));
	ictx->is_root = 0;
	actx = ictx->actx;
	ictx->ia = ia;
	ictx->vcn = 0;
	ictx->index = ih;
	ictx->upl_ofs = 0;
	ictx->upl = upl;
	ictx->pl = pl;
	ictx->addr = (u8*)ia;
	ictx->bytes_free = le32_to_cpu(ia->index.allocated_size) -
			le32_to_cpu(ia->index.index_length);
	/*
	 * We have copied the index entries and switched the index context so
	 * we can go ahead and shrink the index root by moving the end entry on
	 * top of the first entry.  We only need to do the move if the first
	 * index root entry is not the end entry, i.e. the index is not empty.
	 */
	ih = &ir->index;
	if (!(ie->flags & INDEX_ENTRY_END)) {
		u8 *index_end;
		INDEX_ENTRY *end_ie;

		index_end = (u8*)&ir->index + le32_to_cpu(ih->index_length);
		for (end_ie = ie;; end_ie = (INDEX_ENTRY*)((u8*)end_ie +
				le16_to_cpu(end_ie->length))) {
			/* Bounds checks. */
			if ((u8*)end_ie < (u8*)ie || (u8*)end_ie +
					sizeof(INDEX_ENTRY_HEADER) >
					index_end || (u8*)end_ie +
					le16_to_cpu(end_ie->length) >
					index_end ||
					(u32)sizeof(INDEX_ENTRY_HEADER) +
					le16_to_cpu(end_ie->key_length) >
					le16_to_cpu(end_ie->length)) {
				ntfs_error(vol->mp, "Corrupt index.  Run "
						"chkdsk.");
				NVolSetErrors(vol);
				err = EIO;
				goto ictx_err;
			}
			/* Stop when we have found the last entry. */
			if (end_ie->flags & INDEX_ENTRY_END)
				break;
		}
		memmove(ie, end_ie, le16_to_cpu(end_ie->length));
	}
	/*
	 * If the end entry is a leaf we need to extend it by 8 bytes in order
	 * to turn it into a node entry.  To do this we need to extend the
	 * index root attribute itself in the case that the index root was
	 * empty to start with or we need to do nothing if the index root was
	 * not empty as we have not shrunk the index root attribute yet and we
	 * moved the end index entry forward by at least one index entry which
	 * is much bigger than 8 bytes.
	 *
	 * We do the index root attribute resize unconditionally because it
	 * takes care of the size increase needed in the empty index root case
	 * as well as of the size decrease needed in the non-empty index root
	 * case.
	 */
retry_resize:
	u = le16_to_cpu(ie->length);
	if (!(ie->flags & INDEX_ENTRY_NODE))
		u += sizeof(VCN);
	err = ntfs_resident_attr_value_resize(actx->m, actx->a,
			offsetof(INDEX_ROOT, index) +
			le32_to_cpu(ih->entries_offset) + u);
	if (err) {
		leMFT_REF mref;
		ATTR_RECORD *a;
		ntfschar *a_name;
		ATTR_LIST_ENTRY *al_entry;
		u8 *al_end;
		ntfs_attr_search_ctx ctx;

		/*
		 * This can only happen when the first entry is the last entry,
		 * i.e. this is an empty directory and thus the above memmove()
		 * has not been done, in which case we only need eight bytes
		 * (sizeof(VCN)) more space which means that if we manage to
		 * move out a single attribute out of the mft record we would
		 * gain that much space.  In the worst case scenario we can
		 * always move out the index root attribute itself in which
		 * case we are guaranteed to have enough space as an empty
		 * index root is much smaller than an mft record.  The only
		 * complication is when there is no attribute list attribute as
		 * we have to add it first.  On the bright side that in itself
		 * can cause some space to be freed up an we may have enough to
		 * extend the index root by eight bytes.
		 *
		 * Add an attribute list attribute if it is not present.
		 */
		if (!NInoAttrList(base_ni)) {
			/*
			 * Take a copy of the current attribute record pointer
			 * so we can update all our pointers below.
			 */
			a = actx->a;
			err = ntfs_attr_list_add(base_ni, actx->m, actx);
			if (err || actx->is_error) {
				if (!err)
					err = actx->error;
				ntfs_error(vol->mp, "Failed to add attribute "
						"list attribute to mft_no "
						"0x%llx (error %d).",
						(unsigned long long)
						base_ni->mft_no, err);
				goto ictx_err;
			}
update_and_retry_resize:
			/*
			 * Need to update our cached pointers as the index root
			 * attribute is likely to have moved.
			 */
			ir = (INDEX_ROOT*)((u8*)actx->a +
					le16_to_cpu(actx->a->value_offset));
			ih = &ir->index;
			ir_ie_ofs = le32_to_cpu(ir->index.entries_offset);
			ie = (INDEX_ENTRY*)((u8*)&ir->index + ir_ie_ofs);
			/*
			 * Retry the resize in case we freed eight bytes in the
			 * mft record which is all we need.
			 */
			goto retry_resize;
		}
		/*
		 * If this is the only attribute record in the mft record we
		 * cannot gain anything by moving it or anything else.  This
		 * really cannot happen as we have emptied the index root thus
		 * have made the attribute as small as possible thus it must
		 * fit just fine in an otherwise empty mft record thus we must
		 * have enough space to do the resize thus we cannot have
		 * gotten here.
		 */
		if (ntfs_attr_record_is_only_one(actx->m, actx->a))
			panic("%s(): ntfs_attr_record_is_only_one()\n",
					__FUNCTION__);
		/*
		 * We now know we have an attribute list attribute and that we
		 * still do not have enough space to extend the index root
		 * attribute by eight bytes.
		 *
		 * First, if the index root attribute is already in an extent
		 * mft record move it into a new, empty extent mft record.
		 * This is guaranteed to give us the needed eight bytes.
		 *
		 * Second, look through the mft record to see if there are any
		 * attribute records we can move out into an extent mft record
		 * and if yes move one.  That will also definitely give us the
		 * needed eight bytes of space.
		 *
		 * Finally, if no attributes are available for moving then move
		 * the index root attribute itself.
		 */
		if (actx->ni != base_ni) {
move_idx_root:
			lck_rw_lock_shared(&base_ni->attr_list_rl.lock);
			err = ntfs_attr_record_move(actx);
			lck_rw_unlock_shared(&base_ni->attr_list_rl.lock);
			if (err) {
				ntfs_error(vol->mp, "Failed to move index "
						"root attribute from mft "
						"record 0x%llx to an extent "
						"mft record (error %d).",
						(unsigned long long)
						actx->ni->mft_no, err);
				goto ictx_err;
			}
			/*
			 * Need to update our cached pointers as the index root
			 * attribute has now moved after which we need to retry
			 * the resize which will now succeed.
			 */
			goto update_and_retry_resize;
		}
		/*
		 * We know the index root is in the base mft record.
		 *
		 * Look through the base mft record to see if there are any
		 * attribute records we can move out into an extent mft record
		 * and if yes move one.  That will also definitely give us the
		 * needed eight bytes of space.
		 */
		ntfs_attr_search_ctx_init(&ctx, base_ni, actx->m);
		ctx.is_iteration = 1;
		do {
			err = ntfs_attr_find_in_mft_record(AT_UNUSED, NULL, 0,
					NULL, 0, &ctx);
			if (err) {
				if (err == ENOENT) {
					/*
					 * No attributes are available for
					 * moving thus move the index root
					 * attribute out of the base mft record
					 * into a new extent.
					 *
					 * TODO: Need to get this case
					 * triggered and then need to run
					 * chkdsk to check for validity of
					 * moving the index root attribute out
					 * of the base mft record.
					 */
					goto move_idx_root;
				}
				ntfs_error(vol->mp, "Failed to iterate over "
						"attribute records in base "
						"mft record 0x%llx (error %d).",
						(unsigned long long)
						base_ni->mft_no, err);
				goto ictx_err;
			}
			/*
			 * Skip the standard information attribute, the
			 * attribute list attribute, and the index root
			 * attribute as we are looking for lower priority
			 * attributes to move out and the attribute list
			 * attribute is of course not movable.
			 */
			a = ctx.a;
		} while (a->type == AT_STANDARD_INFORMATION ||
				a->type == AT_ATTRIBUTE_LIST ||
				a->type == AT_INDEX_ROOT);
		/*
		 * Move the found attribute out to an extent mft record and
		 * update its attribute list entry.
		 *
		 * But first find the attribute list entry matching the
		 * attribute record so it can be updated.
		 */
		a_name = (ntfschar*)((u8*)a + le16_to_cpu(a->name_offset));
		al_entry = (ATTR_LIST_ENTRY*)base_ni->attr_list;
		al_end = (u8*)al_entry + base_ni->attr_list_size;
		mref = MK_LE_MREF(base_ni->mft_no, base_ni->seq_no);
		do {
			if ((u8*)al_entry >= al_end || !al_entry->length) {
				ntfs_error(vol->mp, "Attribute list attribute "
						"in mft_no 0x%llx is "
						"corrupt.  Run chkdsk.",
						(unsigned long long)
						base_ni->mft_no);
				err = EIO;
				goto ictx_err;
			}
			if (al_entry->mft_reference == mref &&
					al_entry->instance == a->instance) {
				/*
				 * We found the entry, stop looking but first
				 * perform a quick sanity check that we really
				 * do have the correct attribute record.
				 */
				if (al_entry->type == a->type &&
						ntfs_are_names_equal(
						(ntfschar*)((u8*)al_entry +
						al_entry->name_offset),
						al_entry->name_length, a_name,
						a->name_length, TRUE,
						vol->upcase, vol->upcase_len))
					break;
				ntfs_error(vol->mp, "Found corrupt attribute "
						"list attribute when looking "
						"for attribute type 0x%x in "
						"attribute list attribute of "
						"base mft record 0x%llx.  Run "
						"chkdsk.",
						(unsigned)le32_to_cpu(a->type),
						(unsigned long long)
						base_ni->mft_no);
				NVolSetErrors(vol);
				err = EIO;
				goto ictx_err;
			}
			/* Go to the next attribute list entry. */
			al_entry = (ATTR_LIST_ENTRY*)((u8*)al_entry +
					le16_to_cpu(al_entry->length));
		} while (1);
		/* Finally, move the attribute to an extent record. */
		err = ntfs_attr_record_move_for_attr_list_attribute(&ctx,
				al_entry, actx, NULL);
		if (err) {
			ntfs_error(vol->mp, "Failed to move attribute type "
					"0x%x out of base mft record 0x%llx "
					"and into an extent mft record (error "
					"%d).  Run chkdsk.",
					(unsigned)le32_to_cpu(a->type),
					(unsigned long long)base_ni->mft_no,
					err);
			NVolSetErrors(vol);
			goto ictx_err;
		}
		/*
		 * Sync the modified attribute list attribute value to
		 * metadata/disk.
		 */
		ntfs_attr_search_ctx_reinit(&ctx);
		err = ntfs_attr_list_sync(base_ni, (unsigned)((u8*)al_entry -
				base_ni->attr_list), &ctx);
		if (!err) {
			/*
			 * Need to update our cached pointers as the index root
			 * attribute has likely moved after which we need to
			 * retry the resize which will now succeed.
			 */
			goto update_and_retry_resize;
		}
		/*
		 * FIXME: We could try and revert the move of the attribute and
		 * the attribute list attribute value change but that is a lot
		 * of work and the only real errors that could happen at this
		 * stage are either that we have run out of memory or that the
		 * disk has gone bad or has been disconnected or there must be
		 * bad memory corruption.  In all those cases we are extremely
		 * unlikely to be able to undo what we have done so far due to
		 * further errors so at least for now we do not bother trying.
		 */
		ntfs_error(vol->mp, "Failed to sync attribute list attribute "
				"in mft_no 0x%llx (error %d).  Leaving "
				"corrupt metadata.  Run chkdsk.",
				(unsigned long long)base_ni->mft_no, err);
		/*
		 * Need to update our cached pointers as the index root
		 * attribute has likely moved.
		 */
		ir = (INDEX_ROOT*)((u8*)actx->a +
				le16_to_cpu(actx->a->value_offset));
		ih = &ir->index;
		ir_ie_ofs = le32_to_cpu(ir->index.entries_offset);
		ie = (INDEX_ENTRY*)((u8*)&ir->index + ir_ie_ofs);
		goto ictx_err;
	}
	/*
	 * Update the end index entry and the index header to reflect the
	 * changes in size.
	 */
	ie->length = cpu_to_le16(u);
	ih->allocated_size = ih->index_length = cpu_to_le32(
			le32_to_cpu(ih->entries_offset) + u);
	/*
	 * Update the index root and end index entry to reflect the fact that
	 * the directory now is a large index and that the end index entry
	 * points to a sub-node and set the sub-node pointer to vcn 0.
	 */
	ih->flags |= LARGE_INDEX;
	ie->flags |= INDEX_ENTRY_NODE;
	*(leVCN*)((u8*)ie + le16_to_cpu(ie->length) - sizeof(VCN)) = 0;
	/*
	 * Now setup the new index context for the emptied index root and
	 * attach it at the start of the tree path.
	 */
	ir_ictx->entries[0] = ir_ictx->entry = ie;
	ir_ictx->is_root = 1;
	ir_ictx->is_locked = 1;
	ir_ictx->ir = ir;
	ir_ictx->index = ih;
	ir_ictx->actx = actx;
	ir_ictx->bytes_free = le32_to_cpu(actx->m->bytes_allocated) -
			le32_to_cpu(actx->m->bytes_in_use);
	ir_ictx->max_entries = ictx->max_entries;
	ir_ictx->nr_entries = 1;
	/* Ensure the modified index root attribute is written to disk. */
	ntfs_index_entry_mark_dirty(ir_ictx);
	/*
	 * Attach the new index context between the current index context
	 * (which is the top of the tree) and the index context above it (which
	 * is the bottom of the tree), i.e. make the new index context the top
	 * of the tree.
	 */
	ictx->up->down = ir_ictx;
	ir_ictx->down = ictx;
	ir_ictx->up = ictx->up;
	ictx->up = ir_ictx;
	/*
	 * If the index allocation attribute is not present yet, we need to
	 * create it and the index bitmap attribute.
	 */
	if (upl) {
		/*
		 * The page is now done, mark the index context as dirty so it
		 * gets written out later.
		 */
		ntfs_index_entry_mark_dirty(ictx);
		/*
		 * Set up the index inode to reflect that it has an index
		 * allocation attribute.
		 */
		NInoSetIndexAllocPresent(idx_ni);
		lck_spin_lock(&idx_ni->size_lock);
		idx_ni->allocated_size = (s64)clusters <<
				vol->cluster_size_shift;
		idx_ni->initialized_size = idx_ni->data_size =
				idx_ni->block_size;
		lck_spin_unlock(&idx_ni->size_lock);
		if (idx_ni->name == I30) {
			lck_spin_lock(&base_ni->size_lock);
			base_ni->allocated_size = idx_ni->allocated_size;
			base_ni->initialized_size = base_ni->data_size =
					idx_ni->block_size;
			lck_spin_unlock(&base_ni->size_lock);
		}
		if (!ubc_setsize(idx_ni->vn, idx_ni->data_size))
			panic("%s(): ubc_setsize() failed.\n", __FUNCTION__);
		/*
		 * Find the position at which we need to insert the index
		 * allocation attribute.
		 */
		err = ntfs_attr_lookup(AT_INDEX_ALLOCATION, idx_ni->name,
				idx_ni->name_len, 0, NULL, 0, actx);
		if (err != ENOENT) {
			if (!err) {
				ntfs_error(vol->mp, "Index allocation "
						"attribute already present "
						"when it should not be.  "
						"Corrupt index or driver "
						"bug.  Run chkdsk.");
				NVolSetErrors(vol);
				err = EIO;
			} else
				ntfs_error(vol->mp, "Failed to look up "
						"position at which to insert "
						"the index allocation "
						"attribute (error %d).", err);
			goto ia_err;
		}
		/* Insert the index allocation attribute. */
		err = ntfs_insert_index_allocation_attribute(base_ni, actx,
				idx_ni);
		if (err || actx->is_error) {
			ntfs_error(vol->mp, "Failed to %s mft_no 0x%llx "
					"(error %d).", actx->is_error ?
					"remap extent mft record of" :
					"insert index allocation attribute in ",
					(unsigned long long)base_ni->mft_no,
					err);
			goto ia_err;
		}
		/*
		 * Ensure the created index allocation attribute is written to
		 * disk.
		 */
		NInoSetMrecNeedsDirtying(actx->ni);
		/*
		 * Find the position at which we need to insert the index
		 * bitmap attribute.
		 */
		ntfs_attr_search_ctx_reinit(actx);
		err = ntfs_attr_lookup(AT_BITMAP, idx_ni->name,
				idx_ni->name_len, 0, NULL, 0, actx);
		if (err != ENOENT) {
			if (!err) {
				ntfs_error(vol->mp, "Index bitmap attribute "
						"attribute already present "
						"when it should not be.  "
						"Corrupt index or driver "
						"bug.  Run chkdsk.");
				NVolSetErrors(vol);
				err = EIO;
			} else
				ntfs_error(vol->mp, "Failed to look up "
						"position at which to insert "
						"the index bitmap attribute "
						"(error %d).", err);
			goto bmp_err;
		}
		/*
		 * Insert the index bitmap attribute as a resident attribute
		 * with a value length sufficient to cover the number of
		 * allocated index blocks rounded up to a multiple of 8 bytes
		 * which are initialized to zero.  We then set the first bit in
		 * the bitmap to reflect the fact that the first index
		 * allocation block is in use.
		 */
		err = ntfs_resident_attr_record_insert(base_ni, actx,
				AT_BITMAP, idx_ni->name, idx_ni->name_len,
				NULL, (((idx_ni->allocated_size >>
				idx_ni->block_size_shift) + 63) >> 3) &
				~(s64)7);
		if (err || actx->is_error) {
			if (!err)
				err = actx->error;
			ntfs_error(vol->mp, "Failed to %s mft_no 0x%llx "
					"(error %d).", actx->is_error ?
					"remap extent mft record of" :
					"insert index bitmap attribute in",
					(unsigned long long)base_ni->mft_no,
					err);
			goto bmp_err;
		}
		*((u8*)actx->a + le16_to_cpu(actx->a->value_offset)) = 1;
		/*
		 * Ensure the created index bitmap attribute is written to
		 * disk.
		 */
		NInoSetMrecNeedsDirtying(actx->ni);
		/*
		 * We successfully created the index allocation and bitmap
		 * attributes thus we can set up our cache of the last set bit
		 * in the bitmap in the index inode to 0 to reflect the fact
		 * that we just allocated the first index block.
		 */
		idx_ni->last_set_bit = 0;
	}
	/*
	 * We are either completely done and can release the attribute search
	 * context and the mft record or we now need to call functions which
	 * will deadlock with us if we do not release the attribute search
	 * context and the mft record so we have to release them now thus we
	 * now unlock the index root context.
	 */
	ntfs_index_ctx_unlock(ir_ictx);
	if (upl) {
		INDEX_ENTRY **entries;
		long delta;
		unsigned i;

update_ie_pointers:
		/*
		 * Note we still hold the locked and referenced index
		 * allocation page which is now attached to the index context
		 * and will be released later when the index context is
		 * released.
		 *
		 * We need to update the index entry pointers in the array to
		 * point into the index block instead of the old index root.
		 */
		entries = ictx->entries;
		delta = (u8*)ictx->entry - (u8*)entries[ictx->entry_nr];
		for (i = 0; i < ictx->nr_entries; i++)
			entries[i] = (INDEX_ENTRY*)((u8*)entries[i] + delta);
		if (ictx->entry != entries[ictx->entry_nr])
			panic("%s(): ictx->entry != entries[ictx->entry_nr]\n",
					__FUNCTION__);
		ntfs_debug("Done.");
		return 0;
	}
	/*
	 * We have an index allocation attribute already thus we need to
	 * allocate an index block from it for us to transfer the temporary
	 * index block to.
	 *
	 * We need to set @is_locked to zero because we are using the temporary
	 * buffer for @ia and hence @upl and @pl are zero thus as far as the
	 * index context code is concerned the context is not actually locked
	 * at all.  Not doing this can lead to a panic() in
	 * ntfs_index_block_alloc() under some circumstances due to an
	 * assertion check that verifies that @is_locked is zero in
	 * ntfs_attr_resize().
	 */
	ictx->is_locked = 0;
	err = ntfs_index_block_alloc(ictx, &vcn, &ia, &ictx->upl_ofs, &upl,
			&ictx->pl, &ictx->addr);
	if (err) {
		ntfs_error(vol->mp, "Failed to allocate index allocation "
				"block (error %d).", err);
		goto alloc_err;
	}
	/* Copy the update sequence number into our temporary buffer. */
	*(le16*)((u8*)ictx->ia + le16_to_cpu(ictx->ia->usa_ofs)) =
			*(le16*)((u8*)ia + le16_to_cpu(ia->usa_ofs));
	/*
	 * Copy our temporary buffer into the allocated index block, free the
	 * temporary buffer and setup the index context to point to the
	 * allocated index block instead of the temporary one.
	 */
	memcpy(ia, ictx->ia, idx_ni->block_size);
	OSFree(ictx->ia, idx_ni->block_size, ntfs_malloc_tag);
	ictx->entry = ie = (INDEX_ENTRY*)((u8*)ia +
			((u8*)ictx->entry - (u8*)ictx->ia));
	ictx->ia = ia;
	ictx->index = &ia->index;
	ictx->upl = upl;
	ictx->is_locked = 1;
	/*
	 * If the vcn of the allocated index block is not zero, we need to
	 * update the vcn in the index block itself as well as the sub-node
	 * vcn pointer in the index root attribute.
	 */
	if (vcn) {
		ictx->vcn = vcn;
		ia->index_block_vcn = cpu_to_sle64(vcn);
		ictx->upl_ofs = (vcn << idx_ni->vcn_size_shift) &
				~PAGE_MASK_64;
		/* Get hold of the mft record for the index inode. */
		err = ntfs_mft_record_map(base_ni, &m);
		if (err) {
			/*
			 * The only thing that is now incorrect is the vcn
			 * sub-node pointer in the empty index root attribute
			 * but we cannot correct it as we are failing to map
			 * the mft record which we need to be able to rollback.
			 * We leave it to chkdsk to sort this out later.
			 */
			ntfs_error(vol->mp, "Cannot rollback partial index "
					"upgrade because "
					"ntfs_mft_record_map() failed (error "
					"%d).  Leaving inconsistent "
					"metadata.  Run chkdsk.", err);
			goto map_err;
		}
		actx = ntfs_attr_search_ctx_get(base_ni, m);
		if (!actx) {
			err = ENOMEM;
			ntfs_error(vol->mp, "Cannot rollback partial index "
					"upgrade because there was not enough "
					"memory to obtain an attribute search "
					"context.  Leaving inconsistent "
					"metadata.  Run chkdsk.");
			goto actx_err;
		}
		/* Find the index root attribute in the mft record. */
		err = ntfs_attr_lookup(AT_INDEX_ROOT, idx_ni->name,
				idx_ni->name_len, 0, NULL, 0, actx);
		if (err)
			goto lookup_err;
		/* Get to the index root value. */
		ir = (INDEX_ROOT*)((u8*)actx->a +
				le16_to_cpu(actx->a->value_offset));
		/* The first index entry which is also the last one. */
		ie = (INDEX_ENTRY*)((u8*)&ir->index +
				le32_to_cpu(ir->index.entries_offset));
		if (!(ie->flags & INDEX_ENTRY_END))
			panic("%s(): !(ie->flags & INDEX_ENTRY_END)\n",
					__FUNCTION__);
		if (!(ie->flags & INDEX_ENTRY_NODE))
			panic("%s(): !(ie->flags & INDEX_ENTRY_NODE)\n",
					__FUNCTION__);
		/* Finally, update the vcn pointer of the index entry. */
		*(leVCN*)((u8*)ie + le16_to_cpu(ie->length) - sizeof(VCN)) =
				cpu_to_sle64(vcn);
		/*
		 * Ensure the updated index entry is written to disk and
		 * release the attribute search context and the mft record.
		 */
		NInoSetMrecNeedsDirtying(actx->ni);
		ntfs_attr_search_ctx_put(actx);
		ntfs_mft_record_unmap(base_ni);
	}
	/*
	 * The page is now done, mark the index context as dirty so it gets
	 * written out later.
	 */
	ntfs_index_entry_mark_dirty(ictx);
	goto update_ie_pointers;
lookup_err:
	ntfs_error(vol->mp, "Cannot rollback partial index upgrade because we "
			"failed to look up the index root attribute (error "
			"%d).  Leaving inconsistent metadata.  Run chkdsk.",
			err);
	if (err == ENOENT)
		err = EIO;
	ntfs_attr_search_ctx_put(actx);
actx_err:
	ntfs_mft_record_unmap(base_ni);
map_err:
	/*
	 * The page is now done, mark the index context as dirty so it gets
	 * written out later.
	 */
	ntfs_index_entry_mark_dirty(ictx);
	goto err;
alloc_err:
	/*
	 * We need to get hold of the mft record and get a new search context
	 * so we can restore the index root attribute.
	 */
	err2 = ntfs_mft_record_map(base_ni, &m);
	if (err2) {
		ntfs_error(vol->mp, "Cannot rollback partial index upgrade "
				"because ntfs_mft_record_map() failed (error "
				"%d).  Leaving inconsistent metadata.  Run "
				"chkdsk.", err2);
undo_alloc_err2:
		/*
		 * Mark the index context invalid given it neither has an index
		 * block and page nor an index root and mft record attached.
		 */
		ictx->entry = NULL;
		goto undo_alloc_err;
	}
	actx = ntfs_attr_search_ctx_get(base_ni, m);
	if (!actx) {
		ntfs_error(vol->mp, "Cannot rollback partial index upgrade "
				"because there was not enough memory to "
				"obtain an attribute search context.  Leaving "
				"inconsistent metadata.  Run chkdsk.");
		ntfs_mft_record_unmap(base_ni);
		goto undo_alloc_err2;
	}
	goto restore_ir;
bmp_err:
	ntfs_attr_search_ctx_reinit(actx);
	err2 = ntfs_attr_lookup(AT_INDEX_ALLOCATION, idx_ni->name,
			idx_ni->name_len, 0, NULL, 0, actx);
	if (err2) {
		ntfs_error(vol->mp, "Cannot rollback partial index upgrade "
				"because looking up the index allocation "
				"attribute failed (error %d).  Leaving "
				"inconsistent metadata.  Run chkdsk.", err2);
bmp_err_err:
		NVolSetErrors(vol);
		/*
		 * Everything is actually consistent except for the fact that
		 * the index bitmap attribute is missing so it is better if we
		 * do not do any kind of rollback.  Hopefully, chkdsk will fix
		 * this by adding the missing index bitmap attribute.
		 */
		goto err;
	}
	/* Remove the added index allocation attribute. */
	err2 = ntfs_attr_record_delete(base_ni, actx);
	if (err2) {
		ntfs_error(vol->mp, "Cannot rollback partial index upgrade "
				"because deleting the index allocation "
				"attribute failed (error %d).  Leaving "
				"inconsistent metadata.  Run chkdsk.", err2);
		goto bmp_err_err;
	}
ia_err:
	/* Reset the inode. */
	lck_spin_lock(&idx_ni->size_lock);
	idx_ni->initialized_size = idx_ni->data_size = idx_ni->allocated_size =
			0;
	lck_spin_unlock(&idx_ni->size_lock);
	if (idx_ni->name == I30) {
		lck_spin_lock(&base_ni->size_lock);
		base_ni->initialized_size = base_ni->data_size =
				base_ni->allocated_size = 0;
		lck_spin_unlock(&base_ni->size_lock);
	}
	NInoClearIndexAllocPresent(idx_ni);
	/*
	 * Cannot call ubc_setsize() because we have the page pinned so delay
	 * until we dump the page later on.
	 */
	need_ubc_setsize = TRUE;
	/*
	 * Pretend the @ir_ictx is not locked as we are going to transfer the
	 * index root back to @ictx.
	 */
	ir_ictx->is_locked = 0;
	ir_ictx->actx = NULL;
	/* Restore the index root attribute. */
	ntfs_attr_search_ctx_reinit(actx);
restore_ir:
	err2 = ntfs_attr_lookup(AT_INDEX_ROOT, idx_ni->name, idx_ni->name_len,
			0, NULL, 0, actx);
	if (err2) {
		ntfs_error(vol->mp, "Cannot rollback partial index upgrade "
				"because looking up the index root attribute "
				"failed (error %d).  Leaving inconsistent "
				"metadata.  Run chkdsk.", err2);
		NVolSetErrors(vol);
		goto ictx_err;
	}
	ir = (INDEX_ROOT*)((u8*)actx->a + le16_to_cpu(actx->a->value_offset));
	ih = &ir->index;
	ie = (INDEX_ENTRY*)((u8*)ih + ir_ie_ofs);
	u = ir_ie_ofs + (le32_to_cpu(ia->index.index_length) - ia_ie_ofs);
	err2 = ntfs_resident_attr_value_resize(actx->m, actx->a,
			offsetof(INDEX_ROOT, index) + u);
	if (err2) {
		ntfs_error(vol->mp, "Cannot rollback partial index upgrade "
				"because resizing the index root attribute "
				"failed (error %d).  Leaving inconsistent "
				"metadata.  Run chkdsk.", err2);
		NVolSetErrors(vol);
		goto ictx_err;
	}
	if (!old.is_large_index)
		ih->flags &= ~LARGE_INDEX;
	ih->allocated_size = ih->index_length = cpu_to_le32(u);
	memcpy(ie, (u8*)&ia->index + ia_ie_ofs,
			le32_to_cpu(ia->index.index_length) - ia_ie_ofs);
	/* Ensure the restored index root attribute is written to disk. */
	NInoSetMrecNeedsDirtying(actx->ni);
ictx_err:
	/*
	 * Reset the index context.  We may be setting @ictx->entry to a bogus
	 * value but it does not matter because we are returning an error code
	 * thus the caller must not use the index context and while the value
	 * may be bogus it is correctly non-NULL thus the index context will be
	 * cleaned up correctly when the caller releases it.
	 */
	ictx->entry = ictx->entries[ictx->entry_nr];
	ictx->is_root = 1;
	ictx->is_locked = 1;
	ictx->ir = ir;
	ictx->index = ih;
	ictx->actx = actx;
	if (!actx->is_error)
		ir_ictx->bytes_free = le32_to_cpu(actx->m->bytes_allocated) -
				le32_to_cpu(actx->m->bytes_in_use);
	if (!upl) {
undo_alloc_err:
		OSFree(ia, idx_ni->block_size, ntfs_malloc_tag);
	} else {
		/* Destroy the page. */
		ntfs_page_dump(idx_ni, upl, pl);
		if (need_ubc_setsize && !ubc_setsize(idx_ni->vn, 0))
			panic("%s(): ubc_setsize() failed.\n", __FUNCTION__);
page_err:
		err2 = ntfs_cluster_free_from_rl(vol, idx_ni->rl.rl, 0, -1,
				NULL);
		if (err2) {
			ntfs_error(vol->mp, "Failed to rollback cluster "
					"allocation (error %d).  Run chkdsk "
					"to recover the lost space.", err2);
			NVolSetErrors(vol);
		}
		err2 = ntfs_rl_truncate_nolock(vol, &idx_ni->rl, 0);
		if (err2)
			panic("%s(): err2\n", __FUNCTION__);
	}
err:
	/*
	 * Dissociate the allocated index root context from the tree path and
	 * throw it away.  We do this here unconditionally as it works both for
	 * the case where we have not attached it to the tree path yet and for
	 * the case where we have already attached it to the tree path.  We
	 * have to deal with the former case here so cannot defer the cleanup
	 * to the ntfs_index_ctx_put() of the index context @ictx that will be
	 * done by the caller.
	 */
	ntfs_index_ctx_disconnect_reinit(ir_ictx);
	ntfs_index_ctx_put(ir_ictx);
	ntfs_error(vol->mp, "Failed (error %d).", err);
	return err;
}

/**
 * ntfs_index_root_make_space - make space for an index entry in the index root
 * @ictx:	index entry in front of which to make space
 * @ie_size:	size of the index entry to make space for
 *
 * Return 0 on success and ENOSPC if there is not enough space in the mft
 * record to insert an index entry of size @ie_size in the index root
 * attribute.
 *
 * Note that we do not update the array of index entry pointers nor the number
 * of entries in the array because on success it means that the index entry
 * being added will be created and the function is then done, i.e. the array of
 * index entry pointers will not be used any more and on error we have not done
 * anything at all so there is no need to update any of the pointers.
 *
 * Locking: - Caller must hold @ictx->idx_ni->lock on the index inode for
 *	      writing.
 *	    - The index context @ictx must be locked.
 */
static errno_t ntfs_index_root_make_space(ntfs_index_context *ictx,
		const u32 ie_size)
{
	MFT_RECORD *m = ictx->actx->m;
	const u32 muse = le32_to_cpu(m->bytes_in_use);
	const u32 new_muse = muse + ie_size;

	ntfs_debug("Entering.");
	if (new_muse <= le32_to_cpu(m->bytes_allocated)) {
		INDEX_ENTRY *ie = ictx->entry;
		ATTR_RECORD *a;
		INDEX_HEADER *ih;

		/*
		 * Extend the index root attribute so it has enough space for
		 * the new index entry.  As an optimization we combine the
		 * resizing of the index root attribute and the moving of index
		 * entries within the attribute into a single operation.
		 */
		memmove((u8*)ie + ie_size, ie, muse - ((u8*)ie - (u8*)m));
		/* Adjust the mft record to reflect the change in used space. */
		m->bytes_in_use = cpu_to_le32(new_muse);
		/*
		 * Adjust the attribute record to reflect the changes in the
		 * size of the attribute record and in the size of the
		 * attribute value.
		 */
		a = ictx->actx->a;
		a->length = cpu_to_le32(le32_to_cpu(a->length) + ie_size);
		a->value_length = cpu_to_le32(le32_to_cpu(a->value_length) +
				ie_size);
		/* Adjust the index header to reflect the change in length. */
		ih = ictx->index;
		ih->allocated_size = ih->index_length = cpu_to_le32(
				le32_to_cpu(ih->index_length) + ie_size);
		ntfs_debug("Done.");
		return 0;
	}
	ntfs_debug("Failed (not enough space in mft record to insert index "
			"entry into index root attribute).");
	return ENOSPC;
}

/**
 * ntfs_index_block_make_space - make space for an index entry in an index block
 * @ictx:	index entry in front of which to make space
 * @ie_size:	size of the index entry to make space for
 *
 * Return 0 on success and ENOSPC if there is not enough space in the index
 * block to insert an index entry of size @ie_size.
 *
 * Note that we do not update the array of index entry pointers nor the number
 * of entries in the array because on success it means that the index entry
 * being added will be created and the function is then done, i.e. the array of
 * index entry pointers will not be used any more and on error we have not done
 * anything at all so there is no need to update any of the pointers.
 *
 * Locking: - Caller must hold @ictx->idx_ni->lock on the index inode for
 *	      writing.
 *	    - The index context @ictx must be locked.
 */
static errno_t ntfs_index_block_make_space(ntfs_index_context *ictx,
		const u32 ie_size)
{
	INDEX_HEADER *ih = ictx->index;
	const u32 ilen = le32_to_cpu(ih->index_length);
	const u32 new_ilen = ilen + ie_size;

	ntfs_debug("Entering.");
	if (new_ilen <= le32_to_cpu(ih->allocated_size)) {
		INDEX_ENTRY *ie = ictx->entry;

		/* Move the index entries to make space for the new entry. */
		memmove((u8*)ie + ie_size, ie, ilen - ((u8*)ie - (u8*)ih));
		/* Adjust the index header to reflect the change in length. */
		ih->index_length = cpu_to_le32(new_ilen);
		ntfs_debug("Done.");
		return 0;
	}
	ntfs_debug("Failed (not enough space in index block to insert index "
			"entry).");
	return ENOSPC;
}

/**
 * ntfs_index_node_make_space - make space for an index entry in an index node
 * @ictx:	index entry in front of which to make space
 * @ie_size:	size of the index entry to make space for
 *
 * Return 0 on success and ENOSPC if there is not enough space in the index
 * node to insert an index entry of size @ie_size.
 *
 * Note that we do not update the array of index entry pointers nor the number
 * of entries in the array because on success it means that the index entry
 * being added will be created and the function is then done, i.e. the array of
 * index entry pointers will not be used any more and on error we have not done
 * anything at all so there is no need to update any of the pointers.
 *
 * Locking: - Caller must hold @ictx->idx_ni->lock on the index inode for
 *	      writing.
 *	    - The index context @ictx must be locked.
 */
static errno_t ntfs_index_node_make_space(ntfs_index_context *ictx,
		const u32 ie_size)
{
	errno_t err;

	if (ictx->is_root)
		err = ntfs_index_root_make_space(ictx, ie_size);
	else
		err = ntfs_index_block_make_space(ictx, ie_size);
	return err;
}

/**
 * ntfs_index_ctx_revalidate - revalidate the pointers in an index context
 * @new:	index context with the new mapped virtual address
 * @old:	index context to revalidate
 *
 * Revalidate all pointers in the index context @old and adjust them for the
 * changed mapped virtual address in the index context @new.
 *
 * Note both @new and @old must be in the same VM page.
 * 
 * We need to revalidate all the pointers in @old just as if we were locking it
 * because @new may have been mapped into a different virtual address than @old
 * was last time thus the pointers in @old may be stale.
 *
 * To check if revalidation is needed is done by comparing @old->addr and
 * @new->addr.  If they match all is ok and if not then need to revalidate @old
 * with the new address of @new->addr.
 *
 * Locking: - Caller must hold @new->idx_ni->lock on the index inode.
 *	    - The index context @old must be locked.
 *	    - The index context @new must not be locked.
 */
static void ntfs_index_ctx_revalidate(ntfs_index_context *new,
		ntfs_index_context *old)
{
	long delta;
	INDEX_ENTRY **entries;
	unsigned u;

	if (!new->is_locked)
		panic("%s(): !new->is_locked\n", __FUNCTION__);
	if (new->is_root)
		panic("%s(): new->is_root\n", __FUNCTION__);
	if (old->is_locked)
		panic("%s(): old->is_locked\n", __FUNCTION__);
	if (old->is_root)
		panic("%s(): old->is_root\n", __FUNCTION__);
	delta = new->addr - old->addr;
	if (!delta)
		return;
	/* Revalidation is needed. */
	old->entry = (INDEX_ENTRY*)((u8*)old->entry + delta);
	old->ia = (INDEX_ALLOCATION*)((u8*)old->ia + delta);
	old->index = &old->ia->index;
	old->addr = new->addr;
	entries = old->entries;
	for (u = 0; u < old->nr_entries; u++)
		entries[u] = (INDEX_ENTRY*)((u8*)entries[u] + delta);
}

/**
 * ntfs_index_ctx_lock_two - lock two index contexts in a deadlock-free fashion
 * @a:	first index context to lock (this may be locked)
 * @b:	second index context to lock (this must not be locked)
 *
 * Lock the two index contexts @a and @b.  @a may already be locked and it may
 * need to be unlocked if it has to be locked after @b is locked.
 *
 * The following lock ordering rules are applied:
 *
 * - An index block node must be locked before an index root node.
 *
 * - Two index block nodes must be locked in ascending page offset order.
 *
 * - Two index block nodes that are physically in the same VM page must share
 *   the lock.  The way this is implemented is that @a is really locked and @b
 *   is not actually locked but all its pointers are revalidated as if it were
 *   locked.  This is ok because @a is locked and therefore the VM page is
 *   mapped, locked, and pinned and will not go anywhere until we are done.
 *   The reason we need to do the revalidation is because @b when it was mapped
 *   previously may have been mapped at a different virtual address than the
 *   one @a is mapped at now.  This means that when you are unlocking @b you
 *   must check if @b is locked and only unlock it if so.
 *
 * Return 0 on success or errno on error.  On error @a and @b may be both left
 * unlocked or one can be left locked and one unlocked.
 */
static errno_t ntfs_index_ctx_lock_two(ntfs_index_context *a,
		ntfs_index_context *b)
{
	errno_t err;

	if (b->is_locked)
		panic("%s(): b->is_locked\n", __FUNCTION__);
	if (a->is_root) {
		/*
		 * @a is the index root so it has to be locked second.
		 *
		 * Unlock @a if it is already locked.
		 */
		if (a->is_locked)
			ntfs_index_ctx_unlock(a);
	} else if (b->is_root) {
		/* @b is the index root.  If @a is not locked, lock it. */
		if (!a->is_locked) {
			err = ntfs_index_ctx_relock(a);
			if (err)
				return err;
		}
	} else {
		/*
		 * Both @a and @b are index blocks.
		 *
		 * Do we need to share the lock because both index nodes are in
		 * the same VM page?
		 */
		if (a->upl_ofs == b->upl_ofs) {
			if (!a->is_locked) {
				err = ntfs_index_ctx_relock(a);
				if (err)
					return err;
			}
			ntfs_index_ctx_revalidate(a, b);
			return 0;
		}
		if (a->is_locked) {
			/* Do we have to unlock @a before locking @b? */
			if (a->upl_ofs > b->upl_ofs)
				ntfs_index_ctx_unlock(a);
		} else {
			/* Do we need to lock @a first? */
			if (a->upl_ofs < b->upl_ofs) {
				err = ntfs_index_ctx_relock(a);
				if (err)
					return err;
			}
		}
	}
	/* Lock @b. */
	err = ntfs_index_ctx_relock(b);
	/* If @a is currently locked or there was an error we are done. */
	if (a->is_locked || err)
		return err;
	/*
	 * We unlocked @a so we could lock @b or @a was not locked.
	 *
	 * Lock @a and we are done.
	 */
	return ntfs_index_ctx_relock(a);
}

/**
 * ntfs_index_entry_add_or_node_split - add a key to an index
 * @ictx:	index context specifying the node to split/position to add at
 * @split_only:	if true do not insert, only split the index node
 * @entry_size:	size of the entry that will be added after the split
 * @key:	key to add to the directory or view index
 * @key_len:	size of key @key in bytes
 * @data:	data to associate with the key @key if a view index
 * @data_len:	size of data @data in bytes if a view index
 *
 * If @split_only is true, split the index node pointed to by @ictx.  @ictx
 * also points to the entry in the index node at which an entry will be
 * inserted after the split is completed.  @entry_size is the size of the entry
 * that will be added at that position.  These are used so that the split is
 * performed with consideration with the insertion that is likely to come.  And
 * if the insertion does not happen it does not matter much, it just means the
 * split was not quite on the median entry but off by one.
 *
 * In this case @key, @key_len, @data, and @data_len are ignored.
 *
 * If @split_only is false @entry_size is ignored and @key, @key_len, @data,
 * and @data_len are used.
 *
 * In this case, if @ictx belongs to a directory index, insert the filename
 * attribute @key of length @key_len bytes in the directory index at the
 * position specified by the index context @ictx and point the inserted index
 * entry at the mft reference *@data which is the mft reference of the inode to
 * which the filename @fn belongs.  @data_len must be zero in this case.
 * 
 * If @ictx belongs to a view index, insert the key @key of length @key_len
 * bytes in the view index at the position specified by the index context @ictx
 * and associate the data @data of size @data_len bytes with the key @key.
 *
 * Return 0 on success and errno on error.  On error the index context is no
 * longer usable and must be released or reinitialized.
 *
 * Note that we do not update the array of index entry pointers nor the number
 * of entries in the array because on success it means that the index node has
 * been split and the function is done, i.e. the array of index entry pointers
 * will not be used any more and on error the index context becomes invalid so
 * there is no need to update any of the pointers.  The caller is expected to
 * restart its operations by doing a new index lookup if it wants to continue
 * working on the index for that reason.
 *
 * Locking: - Caller must hold @ictx->idx_ni->lock on the index inode for
 *	      writing.
 *	    - The index context @ictx must be locked.
 */
errno_t ntfs_index_entry_add_or_node_split(ntfs_index_context *ictx,
		const BOOL split_only, u32 entry_size, const void *key,
		const u32 key_len, const void *data, const u32 data_len)
{
	VCN vcn = 0;
	ntfs_index_context *cur_ictx, *child_ictx;
	ntfs_inode *bmp_ni, *idx_ni = ictx->idx_ni;
	u32 data_ofs = 0;
	errno_t err, err2;
	const BOOL is_view = (idx_ni->name != I30);

	ntfs_debug("Entering.");
	if (!ictx->is_locked)
		panic("%s(): !ictx->is_locked\n", __FUNCTION__);
	/*
	 * If this is only a split @entry_size contains the size of the entry
	 * and the @key and @data are not set (and not needed/used).
	 * 
	 * If this is an addition, @entry_size is not set and we need to work
	 * it out from the supplied @key and @data.
	 */
	if (!split_only) {
		/*
		 * Calculate the size for the index entry to be added.  For a
		 * directory index, the mft reference @data is embedded inside
		 * the directory index entry thus we only need space for the
		 * index entry header and the filename attribute @key.
		 *
		 * For a view index, the data follows the key aligned to a
		 * 4-byte boundary.
		 *
		 * As an additional complication, the $SDH index in the $Secure
		 * system file contains an extra magic which is not counted in
		 * the data length @data_len so we need to add it by hand here.
		 */
		entry_size = sizeof(INDEX_ENTRY_HEADER) + key_len;
		if (is_view) {
			data_ofs = (entry_size + 4) & ~4;
			entry_size = data_ofs + data_len;
			if (idx_ni == idx_ni->vol->secure_sdh_ni)
				entry_size += sizeof(((SDH_INDEX_DATA*)NULL)->
						magic);
		}
		/*
		 * Align the index entry size to an 8-byte boundary and add
		 * another 8 bytes to the entry size if the insertion is to
		 * happen in an index node.
		 */
		entry_size = ((entry_size + 7) & ~7) +
				((ictx->index->flags & INDEX_NODE) ?
				sizeof(leVCN): 0);
	}
	/* Set the current entry to be the entry to be added to the index. */
	cur_ictx = ictx;
	cur_ictx->promote_inserted = 0;
	cur_ictx->insert_to_add = 1;
	cur_ictx->insert_entry_size = entry_size;
	/*
	 * We need to loop over each index node on the tree path starting at
	 * the bottom.  For each traversed node, we check if the current entry
	 * to be inserted fits and if it does not we need to split that node.
	 * We make the arrangements to be able to do so and then move onto the
	 * next node up the tree and so on until we find a node which does have
	 * enough space to fit the index entry to be inserted.  We can then
	 * break out of the loop and begin work on the actual addition of the
	 * entry and the splitting of the so marked nodes.
	 */
	while (cur_ictx->insert_entry_size > cur_ictx->bytes_free) {
		ntfs_index_context *insert_ictx;
		unsigned median_entry_nr, insert_entry_nr, entry_nr;
		u32 insert_entry_size;
		BOOL insert_to_add;

		/*
		 * The entry to be inserted into this node @cur_ictx is
		 * specified by its @insert_entry and @insert_entry_size.
		 */
		if (!cur_ictx->is_locked)
			panic("%s(): !cur_ictx->is_locked\n", __FUNCTION__);
		/*
		 * We do not have enough space in the current node to insert
		 * the entry to be inserted.
		 *
		 * If the current node is the index root we need to move the
		 * index root to an index block and if successful restart the
		 * loop to determine if we now have enough space to insert the
		 * entry.
		 *
		 * This causes the B+tree to grow in height by one and is in
		 * fact the only operation that can cause the tree to grow in
		 * height.  All other operations can only cause the tree to
		 * grow in width.
		 */
		if (cur_ictx->is_root) {
			ntfs_debug("Moving index root into index allocation "
					"block.");
			/*
			 * If we speculatively locked the index node containing
			 * the index entry to insert into the index root,
			 * unlock it now so we can move the index root to an
			 * index block.
			 */
			insert_ictx = cur_ictx->insert_ictx;
			if (insert_ictx && insert_ictx->is_locked) {
				if (insert_ictx->is_root)
					panic("%s(): insert_ictx->is_root\n",
							__FUNCTION__);
				ntfs_index_ctx_unlock(insert_ictx);
			}
			err = ntfs_index_move_root_to_allocation_block(
					cur_ictx);
			if (!err)
				continue;
			ntfs_error(idx_ni->vol->mp, "Failed to move index "
					"root to index allocation block "
					"(error %d).", err);
			goto err;
		}
		ntfs_debug("Need to split index block with VCN 0x%llx.",
				(unsigned long long)cur_ictx->vcn);
		/*
		 * We do not have enough space in the current node which we now
		 * know is an index allocation block.  We have to split the
		 * node and promote the median entry into the parent node which
		 * may in turn involve a split.  We do not perform the split
		 * but instead work out what needs to be done and allocate any
		 * needed index blocks in advance so we cannot run out of disk
		 * space and/or memory half-way through and only then do we do
		 * the actual work on the index nodes.  This preparation
		 * involves the following steps:
		 *
		 * 1. Work out the median entry to be promoted into the parent
		 *    node of the current node and unlock the current node.
		 * 2. If the entry to be promoted is not the entry to be
		 *    inserted, make a note of the fact that the entry to be
		 *    inserted is to be inserted into the current node in the
		 *    index context.
		 * 3. Allocate a new index block and make a note of it in the
		 *    current index context.
		 * 4. Set the parent node as the current node.
		 * 5. Set the median entry up as the current entry to be
		 *    inserted.
		 * 
		 * Then go round the loop again checking whether there is
		 * enough space to insert the now current entry into the now
		 * current node...
		 *
		 * First of all, mark the node as needing to be split.
		 */
		cur_ictx->split_node = 1;
		/*
		 * 1. Determine the median index entry.
		 *
		 * Note we exclude the end entry from the median calculation
		 * because it is not eligible for promotion thus we should use
		 * @ictx->nr_entries - 1 but we need to take into account the
		 * not yet inserted entry for which we are doing the split in
		 * the first place so that is a + 1 and we also start counting
		 * entries at 0 and not 1 thus we apply a - 1 for that.  Thus
		 * we have - 1 + 1 - 1 = -1.
		 */
		median_entry_nr = (cur_ictx->nr_entries - 1) / 2;
		/*
		 * @entry_nr is the index into the array of index entry
		 * pointers of the index entry in front of which the entry to
		 * be inserted @cur_ictx->insert_entry needs to be inserted.
		 */
		entry_nr = cur_ictx->entry_nr;
		/*
		 * If the position at which to insert the entry to be inserted
		 * is the median promote the entry to be inserted.  If the
		 * number of entries is even there are two possible medians.
		 * We choose the first one for simplicity.
		 *
		 * If the entry to be inserted is before the median, subtract 1
		 * from the median.
		 *
		 * Otherwise promote the median entry.
		 *
		 * The only exception is when @split_only is true and the
		 * current node is the node we are meant to split in which
		 * case there is no entry to be inserted and thus it cannot be
		 * promoted.  In this case we recalculate the median ignoring
		 * the entry to be inserted and promote that.
		 */
		if (entry_nr == median_entry_nr && (!split_only ||
				cur_ictx != ictx)) {
			insert_to_add = cur_ictx->insert_to_add;
			insert_entry_nr = cur_ictx->insert_entry_nr;
			insert_entry_size = cur_ictx->insert_entry_size;
			insert_ictx = cur_ictx->insert_ictx;
			/*
			 * 2. The entry to be promoted is the entry to be
			 *    inserted, make a note of that fact.
			 */
			cur_ictx->promote_inserted = 1;
		} else {
			if (entry_nr < median_entry_nr)
				median_entry_nr--;
			else if (entry_nr == median_entry_nr) {
				if (!split_only)
					panic("%s(): !split_only\n",
							__FUNCTION__);
				if (cur_ictx != ictx)
					panic("%s(): cur_ictx != ictx\n",
							__FUNCTION__);
				/*
				 * We must have at least one real entry or
				 * there is nothing to promote.  This cannot
				 * happen unless something has gone wrong.
				 */
				if (cur_ictx->nr_entries < 2)
					panic("%s(): cur_ictx->nr_entries < "
							"2\n", __FUNCTION__);
				median_entry_nr = (cur_ictx->nr_entries - 2) /
						2;
			}
			insert_to_add = FALSE;
			insert_entry_nr = median_entry_nr;
			insert_entry_size = le16_to_cpu(cur_ictx->
					entries[median_entry_nr]->length);
			insert_ictx = cur_ictx;
		}
		/*
		 * If this is the very first promotion and we are promoting a
		 * leaf entry we need to add 8 bytes to the size of the entry
		 * being promoted to allow for the VCN sub-node pointer that
		 * will be added at the end of the entry when it is inserted
		 * into the parent index node.
		 */
		if (cur_ictx == ictx &&
				(!(cur_ictx->index->flags & INDEX_NODE) ?
				sizeof(leVCN): 0))
			insert_entry_size += sizeof(VCN);
		/*
		 * Record which entry is being promoted, i.e. where we need to
		 * perform the split of the node.
		 */
		cur_ictx->promote_entry_nr = median_entry_nr;
		// TODO: Possible optimization: Allow ntfs_index_block_alloc()
		// to do the unlocking and thus to consume the lock if the new
		// index block is in the same page as the current index block.
		if (!cur_ictx->is_locked)
			panic("%s(): !cur_ictx->is_locked\n", __FUNCTION__);
		ntfs_index_ctx_unlock(cur_ictx);
		/*
		 * 3. Allocate a new index block and make a note of it in the
		 *    current index context.
		 *
		 * The call may cause the index root attribute to have its
		 * entries moved out to an index block.  That is fine as we
		 * have not looked at any of our parent entries yet and the
		 * index root must be above us given we are a child node.
		 */
		err = ntfs_index_block_alloc(cur_ictx, &cur_ictx->right_vcn,
				&cur_ictx->right_ia, &cur_ictx->right_upl_ofs,
				&cur_ictx->right_upl, &cur_ictx->right_pl,
				&cur_ictx->right_addr);
		if (err) {
			ntfs_error(idx_ni->vol->mp, "Failed to allocate index "
					"allocation block (error %d).", err);
			goto err;
		}
		ntfs_debug("Allocated index block for right-hand node with "
				"VCN 0x%llx.", (unsigned long long)
				cur_ictx->right_vcn);
		/* We need to unmap the newly allocated index block. */
		ntfs_page_unmap(idx_ni, cur_ictx->right_upl,
				cur_ictx->right_pl, TRUE);
		cur_ictx->right_upl = NULL;
		/* 4. Set the parent node as the current node and lock it. */
		cur_ictx = cur_ictx->up;
		if (cur_ictx->is_locked)
			panic("%s(): cur_ictx->is_locked\n", __FUNCTION__);
		if (cur_ictx->is_root) {
			/*
			 * We have reached the index root.  We speculatively
			 * lock the index node containing the index entry to
			 * insert into the index root, as we cannot lock it
			 * once we have locked the mft record (which happens
			 * when we lock the current index context below) as
			 * that would cause lock reversal and thus deadlocks.
			 */
			if (insert_ictx) {
				if (insert_ictx->is_root)
					panic("%s(): insert_ictx->is_root\n",
							__FUNCTION__);
				if (insert_ictx->is_locked)
					panic("%s(): insert_ictx->is_locked\n",
							__FUNCTION__);
				err = ntfs_index_ctx_relock(insert_ictx);
				if (err)
					goto err;
			}
		}
		/* Lock the current index context. */
		err = ntfs_index_ctx_relock(cur_ictx);
		if (err)
			goto err;
		/*
		 * 5. Set the median entry up as the current entry to be
		 *    inserted.
		 */
		cur_ictx->insert_to_add = insert_to_add;
		cur_ictx->insert_entry_nr = insert_entry_nr;
		cur_ictx->insert_entry_size = insert_entry_size;
		cur_ictx->insert_ictx = insert_ictx;
	}
	/*
	 * Get the child node index context if we are not at the bottom of the
	 * tree already or rather at the node for which we were called (which
	 * may not actually be at the bottom of the tree).
	 */
	child_ictx = NULL;
	if (cur_ictx != ictx) {
		child_ictx = cur_ictx->down;
		if (child_ictx->is_root)
			panic("%s(): child_ictx->is_root\n", __FUNCTION__);
	}
	/*
	 * If the node we were called for was the index root then it will have
	 * been moved to an index allocation block which will likely have
	 * created enough space to fit the entry to be inserted thus if this is
	 * a split only call nothing further needs to be done.
	 */
	if (cur_ictx == ictx && split_only) {
		ntfs_debug("Done (index root was upgraded to index "
				"allocation, no further split needed).");
		return 0;
	}
	/*
	 * We now have prepared everything so we are guaranteed not to run out
	 * of disk space and can begin doing the work.
	 *
	 * TODO: We can still fail if relocking of an index context fails but
	 * that can really only happen under extreme memory pressure and there
	 * is not much we can do about that without <rdar://problem/4992358>
	 * being implemented.  Thus for now we simply bomb out with an error
	 * message and leave the index potentially badly corrupted and also we
	 * potentially leave some allocated index blocks actually unused.  This
	 * is highly unsatisfactory but rolling back once the modifications
	 * have begun is pretty much impossible without keeping a lot more
	 * state so we do not implement rollback.  Once the aforementioned
	 * radar is implemented we will no longer suffer from this problem and
	 * it will no longer be possible to fail once we get here.
	 *
	 * We start with the current node which is the top-most node we need to
	 * deal with.  We know from above it has enough space and does not need
	 * to be split.
	 */
	do {
		ntfs_index_context *insert_ictx;
		INDEX_ENTRY *entry, *right_entry, *end_entry;
		INDEX_HEADER *right_index;
		unsigned u;

		if (!cur_ictx->is_locked)
			panic("%s(): !cur_ictx->is_locked\n", __FUNCTION__);
		/*
		 * The current node is either the top-most node we need to deal
		 * with thus we know it has enough space or we have just split
		 * it and thus also know that it must have enough space.
		 *
		 * Thus the insertion is now a matter of doing a simple insert
		 * into the node unless the current node has had its entry to
		 * be inserted promoted into the parent node in which case
		 * there is nothing to insert into this node.
		 *
		 * Note: Use goto to reduce indentation.
		 */
		insert_ictx = cur_ictx->insert_ictx;
		if (cur_ictx->promote_inserted)
			goto skip_insert;
		/*
		 * We now need to begin to do the insertion but there is one
		 * complication we need to deal with first and that is that if
		 * we are inserting a promoted entry, we have to lock the index
		 * node it is in also.
		 *
		 * If the current node is an index block we may need to unlock
		 * it so that we can ensure to always take the node lock in
		 * ascending page offset order.  Also we have to consider the
		 * case where the two index nodes are in the same page in which
		 * case we need to share the index lock.
		 *
		 * If the current node is the index root and we are inserting a
		 * promoted entry, we speculatively locked the index node
		 * containing the entry in the above loop thus we do not need
		 * to do anything here.
		 */
		if (!cur_ictx->insert_to_add) {
			if (!insert_ictx)
				panic("%s(): !insert_ictx\n", __FUNCTION__);
			if (cur_ictx->is_root) {
				if (!insert_ictx->is_locked)
					panic("%s(): !insert_ictx->"
							"is_locked\n",
							__FUNCTION__);
			} else {
				err = ntfs_index_ctx_lock_two(cur_ictx,
						insert_ictx);
				if (err)
					goto relock_err;
			}
		}
		entry_size = cur_ictx->insert_entry_size;
		/*
		 * Everything we need is locked and we know there is enough
		 * space in the index node thus make space for the entry to be
		 * inserted at the appropriate place and then insert the index
		 * entry by either copying it from the appropriate, locked
		 * child node or by creating it in place.  The latter happens
		 * when the entry being inserted is the entry being added with
		 * this call to ntfs_index_entry_add(), i.e. it is the reason
		 * we have been called in the first place and thus there is no
		 * index entry to copy from.
		 */
		err = ntfs_index_node_make_space(cur_ictx, entry_size);
		if (err)
			panic("%s(): err\n", __FUNCTION__);
		/*
		 * We have modified the index node.  Make sure it is written
		 * out later.
		 */
		ntfs_index_entry_mark_dirty(cur_ictx);
		if (!cur_ictx->insert_to_add) {
			entry = insert_ictx->entries[
					cur_ictx->insert_entry_nr];
			/* Copy the index entry into the created space. */
			memcpy(cur_ictx->entry, entry,
					le16_to_cpu(entry->length));
			entry = cur_ictx->entry;
		} else {
			if (split_only)
				panic("%s(): split_only\n", __FUNCTION__);
			entry = cur_ictx->entry;
			/*
			 * Clear the created space so we start with a clean
			 * slate and do not need to worry about initializing
			 * all the zero fields.
			 */
			bzero(entry, entry_size);
			/* Create the index entry in the created space. */
			if (!is_view)
				entry->indexed_file = *(leMFT_REF*)data;
			else {
				u8 *new_data;

				new_data = (u8*)entry + data_ofs;
				entry->data_offset = cpu_to_le16(data_ofs);
				entry->data_length = cpu_to_le16(data_len);
				if (data_len)
					memcpy(new_data, data, data_len);
				/*
				 * In the case of $Secure/$SDH we leave the
				 * extra magic to zero rather than setting it
				 * to "II" in Unicode.  This could easily be
				 * changed if deemed better and/or necessary by
				 * uncommenting the below code.
				 */
#if 0
				if (idx_ni == idx_ni->vol->secure_sdh_ni) {
					static const ntfschar SDH_magic[2] = {
							const_cpu_to_le16('I'),
							const_cpu_to_le16('I')
					};

					memcpy(((SDH_INDEX_DATA*)data)->magic,
							SDH_magic,
							sizeof(SDH_magic));
				}
#endif
			}
			entry->key_length = cpu_to_le16(key_len);
			memcpy(&entry->key, key, key_len);
		}
		/*
		 * If the copied entry is a leaf entry and it is being inserted
		 * into a non-leaf node, we need to rewrite its size with the
		 * new size which includes the VCN sub-node pointer.
		 *
		 * We just overwrite the length unconditionally and use it to
		 * set the length of the just created index entry, too.
		 *
		 * There is no harm in doing this as we are either updating the
		 * size which we must do or we are overwriting the size with
		 * the same value it already has which has no effect.
		 */
		entry->length = cpu_to_le16(entry_size);
		/*
		 * If the current node is not a leaf node, we have to fix up
		 * the VCN sub-node pointers both in the entry we just inserted
		 * and in the entry in front of which we inserted.
		 *
		 * The inserted index entry needs to point to what will be the
		 * left-hand index node after our child node is split.
		 *
		 * The index entry in front of which we inserted needs to point
		 * to what will be the right-hand index node after our child
		 * node is split.
		 *
		 * The INDEX_NODE check is fine for the index root, too,
		 * because as it happens LARGE_INDEX == INDEX_NODE.
		 */
		if (cur_ictx->index->flags & INDEX_NODE) {
			if (!child_ictx)
				panic("%s(): !child_ictx\n", __FUNCTION__);
			if (entry->flags & INDEX_ENTRY_END)
				panic("%s(): entry->flags & INDEX_ENTRY_END\n",
						__FUNCTION__);
			entry->flags |= INDEX_ENTRY_NODE;
			*(leVCN*)((u8*)entry + entry_size - sizeof(VCN)) =
					cpu_to_sle64(child_ictx->vcn);
			ntfs_debug("Setting VCN sub-node pointer of inserted "
					"index entry to 0x%llx.",
					(unsigned long long)child_ictx->vcn);
			entry = (INDEX_ENTRY*)((u8*)entry + entry_size);
			if (!(entry->flags & INDEX_ENTRY_NODE))
				panic("%s(): !(entry->flags & "
						"INDEX_ENTRY_NODE)\n",
						__FUNCTION__);
			*(leVCN*)((u8*)entry + le16_to_cpu(entry->length) -
					sizeof(VCN)) = cpu_to_sle64(
					child_ictx->right_vcn);
			ntfs_debug("Setting VCN sub-node pointer of index "
					"entry after inserted entry to "
					"0x%llx.", (unsigned long long)
					child_ictx->right_vcn);
		}
skip_insert:
		/*
		 * If there are no child nodes left we are done.  We will dirty
		 * the index node once we have broken out of the loop.  When
		 * the index context is released later all locked nodes will be
		 * unlocked so no need to do it now.
		 */
		if (!child_ictx)
			break;
		if (!child_ictx->split_node)
			panic("%s(): !child_ictx->split_node\n", __FUNCTION__);
		/*
		 * TODO: @child_ictx->right_upl and @child_ictx->right_pl are
		 * currently not valid as @child_ictx is not locked.  Once
		 * <rdar://problem/4992358> is implemented we can re-enable
		 * this check and change the code to leave the right_upl mapped
		 * at all times.
		 */
#if 0
		if (!child_ictx->right_upl || !child_ictx->right_pl)
			panic("%s(): !child_ictx->right_upl || "
					"!child_ictx->right_pl\n",
					__FUNCTION__);
#endif
		/*
		 * We are done with the current node and have a child node.
		 * Switch to the child node, and sort out the needed locks.
		 *
		 * First, unlock the @insert_ictx node if it exists and is
		 * locked.
		 *
		 * Note we do not bother with trying to transfer the lock from
		 * @insert_ictx onto @child_ictx or @child_ictx->right_*
		 * because index blocks are 4096 bytes in size in majority of
		 * cases and the PAGE_SIZE is 4096 bytes both on x86 and PPC
		 * thus in majority of cases each page will contain a separate
		 * index block thus no sharing will be possible and there is no
		 * point in adding extra complexity for an extremely unlikely
		 * event.
		 */
		if (insert_ictx && insert_ictx->is_locked)
			ntfs_index_ctx_unlock(insert_ictx);
		/*
		 * Unlock the current node.  Again we do not bother with trying
		 * to share the lock with @child_ictx or @child_ictx->right_*.
		 */
		ntfs_index_ctx_unlock(cur_ictx);
		/* Set the child node to be the current node. */
		cur_ictx = child_ictx;
		/*
		 * We need to ensure both the current node and its right-hand
		 * node are locked.  Both are currently unlocked.
		 *
		 * If both nodes share the same page, lock the current node and
		 * share the lock with the right one.
		 */
		if (cur_ictx->is_locked)
			panic("%s(): cur_ictx->is_locked\n", __FUNCTION__);
		if (cur_ictx->right_is_locked)
			panic("%s(): cur_ictx->right_is_locked\n",
					__FUNCTION__);
		if (cur_ictx->upl_ofs <= cur_ictx->right_upl_ofs) {
			err = ntfs_index_ctx_relock(cur_ictx);
			if (err)
				goto relock_err;
		} 
		if (cur_ictx->upl_ofs == cur_ictx->right_upl_ofs) {
			cur_ictx->right_ia = (INDEX_ALLOCATION*)(
					(u8*)cur_ictx->right_ia +
					(cur_ictx->addr -
					cur_ictx->right_addr));
			cur_ictx->right_addr = cur_ictx->addr;
		} else {
			u8 *addr;

			err = ntfs_page_map(idx_ni, cur_ictx->right_upl_ofs,
					&cur_ictx->right_upl,
					&cur_ictx->right_pl, &addr, TRUE);
			if (err) {
				ntfs_error(idx_ni->vol->mp, "Failed to map "
						"index page (error %d).", err);
				cur_ictx->right_upl = NULL;
				goto relock_err;
			}
			cur_ictx->right_is_locked = 1;
			cur_ictx->right_ia = (INDEX_ALLOCATION*)(
					(u8*)cur_ictx->right_ia + (addr -
					cur_ictx->right_addr));
			cur_ictx->right_addr = addr;
		}
		if (!cur_ictx->is_locked) {
			err = ntfs_index_ctx_relock(cur_ictx);
			if (err) {
				if (cur_ictx->right_is_locked) {
					ntfs_page_unmap(idx_ni,
							cur_ictx->right_upl,
							cur_ictx->right_pl,
							FALSE);
					cur_ictx->right_upl = NULL;
					cur_ictx->right_is_locked = 0;
				}
				goto relock_err;
			}
		}
		/*
		 * Having obtained the needed locks, we can now split the
		 * current node.
		 *
		 * We have recorded the split point in @promote_entry_nr and
		 * @promote_inserted tells us whether to remove the entry
		 * specified by @promote_entry_nr and move all entries after it
		 * to the right-hand node (@promote_inserted is 0) or whether
		 * to move the entry specified by @promote_entry_nr and all
		 * entries after it to the right-hand node (@promote_inserted
		 * is 1).
		 *
		 * The split results in the creation of a new end entry in the
		 * left-hand node as its old end entry is moved to the right
		 * hand node.  This means we need to determine what we need to
		 * set the VCN sub-node pointer to if this is an index node.
		 *
		 * If @promote_iserted is 0, i.e. we are promoting an existing
		 * entry from this node, we need to use the VCN sub-node
		 * pointer of the entry we are about to promote.  Because we
		 * are promoting the entry it is going to disappear altogether
		 * from this node thus we need to make a note of its sub-node
		 * VCN pointer if it has one now before doing the actual split.
		 *
		 * In this case we do not need to modify the VCN sub-node
		 * pointer of the first entry in the (new) right-hand node as
		 * it does not change.
		 *
		 * If @promote_inserted is 1, i.e. we are promoting the entry
		 * that was going to be inserted into this node, we need to use
		 * the VCN of our child node which is the VCN of the entry in
		 * front of which we are inserting and splitting, i.e. the VCN
		 * of the first entry we are about to move to the right-hand
		 * node.
		 *
		 * In this case we also need to modify the VCN sub-node pointer
		 * of the first entry in the (new) right-hand node to point to
		 * the (new) right-hand child node.  We do not know what that
		 * is yet so we determine it later once we have obtained our
		 * child node index context containing the needed information.
		 *
		 * First, copy the appropriate entries to the right-hand node
		 * and switch the node to be an index, i.e. non-leaf, node if
		 * the node being split is an index node.
		 */
		right_index = &cur_ictx->right_ia->index;
		right_entry = (INDEX_ENTRY*)((u8*)right_index +
				le32_to_cpu(right_index->entries_offset));
		u = cur_ictx->promote_entry_nr;
		entry = cur_ictx->entries[u];
		if (!cur_ictx->promote_inserted) {
			if (entry->flags & INDEX_ENTRY_NODE) {
				if (!(cur_ictx->index->flags & INDEX_NODE))
					panic("%s(): !(cur_ictx->index->flags "
							"& INDEX_NODE)\n",
							__FUNCTION__);
				vcn = sle64_to_cpu(*(leVCN*)((u8*)entry +
						le16_to_cpu(entry->length) -
						sizeof(VCN)));
			}
			u++;
			entry = cur_ictx->entries[u];
		}
		end_entry = cur_ictx->entries[cur_ictx->nr_entries - 1];
		if (!(end_entry->flags & INDEX_ENTRY_END))
			panic("%s(): !(end_entry->flags & INDEX_ENTRY_END)\n",
					__FUNCTION__);
		u = (unsigned)((u8*)end_entry - (u8*)entry) +
				le16_to_cpu(end_entry->length);
		memcpy(right_entry, entry, u);
		right_index->index_length = cpu_to_le32(
				le32_to_cpu(right_index->entries_offset) + u);
		right_index->flags = cur_ictx->index->flags;
		/*
		 * Move the end entry of the left-hand node forward thus
		 * truncating the left-hand node.
		 */
		if (!cur_ictx->promote_inserted)
			entry = cur_ictx->entries[cur_ictx->promote_entry_nr];
		if (entry != end_entry) {
			u = le16_to_cpu(end_entry->length);
			memmove(entry, end_entry, u);
			u += (u8*)entry - (u8*)cur_ictx->index;
			cur_ictx->index->index_length = cpu_to_le32(u);
		}
		/*
		 * If the current, left-hand node is not a leaf node, we have
		 * to replace the VCN sub-node pointer in its end entry.
		 *
		 * If @promote_iserted is 0, we use the VCN we made a note of
		 * above.
		 *
		 * If @promote_inserted is 1, we take the VCN of the left-hand
		 * node of our child node.
		 *
		 * A side effect of getting the child node in loop scope here
		 * is that we do not need to reget it when we go round the loop
		 * again which is when we need to know the child node in order
		 * to be able to insert the appropriate entry into the current
		 * node.
		 */
		child_ictx = NULL;
		if (entry->flags & INDEX_ENTRY_NODE) {
			if (cur_ictx != ictx) {
				child_ictx = cur_ictx->down;
				if (child_ictx->is_root)
					panic("%s(): child_ictx->is_root\n",
							__FUNCTION__);
			}
			if (cur_ictx->promote_inserted) {
				if (!child_ictx)
					panic("%s(): !child_ictx\n",
							__FUNCTION__);
				/*
				 * As described take the VCN of the left-hand
				 * node of our child node index context for the
				 * new end entry.
				 */
				vcn = child_ictx->vcn;
				/*
				 * Again, as described, update the VCN of the
				 * first entry we just moved to the (new) right
				 * hand node to the right-hand node of our
				 * child node index context.
				 */
				*(leVCN*)((u8*)right_entry + le16_to_cpu(
						right_entry->length) -
						sizeof(VCN)) = cpu_to_sle64(
						child_ictx->right_vcn);
				ntfs_debug("Setting VCN sub-node pointer of "
						"first index entry of "
						"right-hand index block node "
						"after splitting it off from "
						"the left-hand node to "
						"0x%llx.", (unsigned long long)
						child_ictx->right_vcn);
			}
			if (!(cur_ictx->index->flags & INDEX_NODE))
				panic("%s(): !(cur_ictx->index->flags & "
						"INDEX_NODE)\n", __FUNCTION__);
			*(leVCN*)((u8*)entry + le16_to_cpu(entry->length) -
					sizeof(VCN)) = cpu_to_sle64(vcn);
			ntfs_debug("Setting VCN sub-node pointer of end index "
					"entry of left-hand index block node "
					"after splitting off the right-hand "
					"node to 0x%llx.", (unsigned long long)
					vcn);
		}
		/*
		 * The index context still describes a single node thus if we
		 * are going to insert an entry into either of the two split
		 * nodes we have to update the index context appropriately.
		 *
		 * If @cur_ictx->entry_nr <= @cur_ictx->promote_entry_nr, we
		 * have to insert the entry into the left-hand node and if
		 * @cur_ictx->entry_nr > @cur_ictx->promote_entry_nr we have to
		 * insert the entry into the right-hand node.
		 *
		 * If inserting into the left-hand node we do not need to do
		 * anything as the insertion process does not make use of
		 * anything in the index context that has changed.
		 *
		 * If inserting into the right-hand node we have to switch the
		 * index context to describe the right-hand node and place the
		 * left-hand node into the place of the right-hand node, i.e.
		 * swap the left- and right-hand nodes in the index context.
		 *
		 * If we are switching the left- and right-hand nodes, we also
		 * have to update the index entry pointer to point to the
		 * correct insertion location in the now current page.
		 *
		 * Note we do not bother to update the array of index entry
		 * pointers as that is no longer used.
		 */
		if (!cur_ictx->promote_inserted && cur_ictx->entry_nr >
				cur_ictx->promote_entry_nr) {
			union {
				VCN vcn;
				INDEX_BLOCK *ia;
				s64 upl_ofs;
				upl_t upl;
				upl_page_info_array_t pl;
				u8 *addr;
			} tmp;

			tmp.vcn = cur_ictx->right_vcn;
			cur_ictx->right_vcn = cur_ictx->vcn;
			cur_ictx->vcn = tmp.vcn;
			tmp.ia = cur_ictx->right_ia;
			cur_ictx->ia = tmp.ia;
			cur_ictx->right_ia = cur_ictx->ia;
			cur_ictx->index = right_index = &tmp.ia->index;
			if (cur_ictx->right_is_locked) {
				tmp.upl_ofs = cur_ictx->right_upl_ofs;
				cur_ictx->right_upl_ofs = cur_ictx->upl_ofs;
				cur_ictx->upl_ofs = tmp.upl_ofs;
				tmp.upl = cur_ictx->right_upl;
				cur_ictx->right_upl = cur_ictx->upl;
				cur_ictx->upl = tmp.upl;
				tmp.pl = cur_ictx->right_pl;
				cur_ictx->right_pl = cur_ictx->pl;
				cur_ictx->pl = tmp.pl;
				tmp.addr = cur_ictx->right_addr;
				cur_ictx->right_addr = cur_ictx->addr;
				cur_ictx->addr = tmp.addr;
			}
			/*
			 * Get the location in the left-hand page of the first
			 * entry that was moved to the right-hand page.
			 */
			entry = cur_ictx->entries[cur_ictx->promote_entry_nr +
					1];
			cur_ictx->entry = (INDEX_ENTRY*)((u8*)right_index +
					le32_to_cpu(right_index->
					entries_offset) +
					((u8*)cur_ictx->entry - (u8*)entry));
		}
		/*
		 * We are done with the node that is now set up as the
		 * right-hand node.  Unless it is sharing the lock with the
		 * left-hand node, unmap/release it marking it dirty so it gets
		 * written out later.
		 */
		if (cur_ictx->right_is_locked) {
			ntfs_page_unmap(idx_ni, cur_ictx->right_upl,
					cur_ictx->right_pl, TRUE);
			cur_ictx->right_upl = NULL;
			cur_ictx->right_is_locked = 0;
		}
		/*
		 * We may be done with the current node.  Mark it dirty so it
		 * gets written out later.
		 */
		ntfs_index_entry_mark_dirty(cur_ictx);
		/*
		 * If we have reached the original node (@child_ictx will be
		 * NULL) and we are only splitting it there is nothing to
		 * insert and hence nothing at all left to do so we break out
		 * of the loop.
		 */
	} while (child_ictx || !split_only);
	ntfs_debug("Done (%s).", cur_ictx->split_node ?
			"insert with split" : "simple insert");
	return 0;
err:
	if (!NInoIndexAllocPresent(idx_ni))
		goto err_out;
	/*
	 * Unlock all index contexts in the B+tree path otherwise the call to
	 * ntfs_attr_inode_get() can deadlock.
	 */
	ntfs_index_ctx_path_unlock(ictx);
	/* Get the index bitmap inode. */
	err2 = ntfs_attr_inode_get(ictx->base_ni, AT_BITMAP, idx_ni->name,
			idx_ni->name_len, FALSE, LCK_RW_TYPE_SHARED, &bmp_ni);
	if (err2) {
		ntfs_error(idx_ni->vol->mp, "Failed to get index bitmap inode "
				"(error %d).  Cannot undo index block "
				"allocation.  Leaving inconsistent metadata.  "
				"Run chkdsk.", err2);
		NVolSetErrors(idx_ni->vol);
		goto err_out;
	}
	/* Free all the index block allocations we have done. */
	do {
		if (cur_ictx->right_addr) {
			if (!cur_ictx->right_ia)
				panic("%s(): !cur_ictx->right_ia\n",
						__FUNCTION__);
			if (cur_ictx->right_vcn < 0)
				panic("%s(): cur_ictx->right_vcn < 0\n",
						__FUNCTION__);
			err2 = ntfs_bitmap_clear_bit(bmp_ni,
					cur_ictx->right_vcn <<
					idx_ni->vcn_size_shift >>
					idx_ni->block_size_shift);
			if (err2) {
				ntfs_error(idx_ni->vol->mp, "Failed to undo "
						"index block allocation in "
						"(error %d).  Leaving "
						"inconsistent metadata.  Run "
						"chkdsk.", err2);
				NVolSetErrors(idx_ni->vol);
			}
		}
		/* When we have dealt with the bottom entry we are done. */
		if (cur_ictx == ictx)
			break;
		cur_ictx = cur_ictx->down;
	} while (1);
	lck_rw_unlock_shared(&bmp_ni->lock);
	(void)vnode_put(bmp_ni->vn);
err_out:
	ntfs_error(idx_ni->vol->mp, "Failed (error %d).", err);
	return err;
relock_err:
	ntfs_error(idx_ni->vol->mp, "Failed to relock index context (error "
			"%d).  Leaving corrupt index.  Run chkdsk.", err);
	NVolSetErrors(idx_ni->vol);
	return err;
}

/**
 * ntfs_index_node_split - split an index node
 * @ictx:	index context specifying the node to split
 * @entry_size:	size of the entry that will be added after the split
 *
 * Split the index node pointed to by @ictx.  @ictx also points to the entry
 * in the index node at which an entry will be inserted after the split is
 * completed.  @entry_size is the size of the entry that will be added at that
 * position.  These are used so that the split is performed with consideration
 * with the insertion that is likely to come.  And if the insertion does not
 * happen it does not matter much, it just means the split was not quite on the
 * median entry but off by one.
 *
 * Return 0 on success and errno on error.  On error the index context is no
 * longer usable and must be released or reinitialized.
 *
 * Note that we do not update the array of index entry pointers nor the number
 * of entries in the array because on success it means that the index node has
 * been split and the function is done, i.e. the array of index entry pointers
 * will not be used any more and on error the index context becomes invalid so
 * there is no need to update any of the pointers.  The caller is expected to
 * restart its operations by doing a new index lookup for that reason.
 *
 * Locking: - Caller must hold @ictx->idx_ni->lock on the index inode for
 *	      writing.
 *	    - The index context @ictx must be locked.
 */
static inline errno_t ntfs_index_node_split(ntfs_index_context *ictx,
		const u32 entry_size)
{
	return ntfs_index_entry_add_or_node_split(ictx, TRUE, entry_size,
			NULL, 0, NULL, 0);
}

/**
 * ntfs_index_lookup_predecessor - index node whose predecessor node to return
 * @ictx:	index context whose predecessor node to return
 * @pred_ictx:	pointer in which to return the found predecessor index context
 *
 * Descend into the child node pointed to by @ictx->entry and then keep
 * descending into the child node of the child node pointed to by the end entry
 * of the child node until we reach the bottom of the B+tree.
 *
 * On success return the predecessor entry, i.e. the last real (non-end) entry
 * in the found leaf index node in *@pred_ictx.
 *
 * Return 0 on success and errno on error.
 *
 * Locking: - Caller must hold @ictx->idx_ni->lock on the index inode.
 *	    - The index context @ictx must be locked.
 */
static errno_t ntfs_index_lookup_predecessor(ntfs_index_context *ictx,
		ntfs_index_context **pred_ictx)
{
	unsigned entry_nr;
	errno_t err;

	ntfs_debug("Entering.");
	if (!pred_ictx)
		panic("%s(): !pred_ictx\n", __FUNCTION__);
	if (!(ictx->entry->flags & INDEX_ENTRY_NODE))
		panic("%s(): !(ictx->entry->flags & INDEX_ENTRY_NODE)\n",
				__FUNCTION__);
	/*
	 * We must be at the bottom of the current tree path or things will get
	 * very confused.
	 */
	if (!ictx->down->is_root)
		panic("%s(): !ictx->down->is_root\n", __FUNCTION__);
	do {
		err = ntfs_index_descend_into_child_node(&ictx);
		if (err)
			goto err;
		/* If this child node is a leaf node we are done. */
		if (!(ictx->index->flags & INDEX_NODE))
			break;
		/*
		 * This child node is an index node, descend into its end
		 * entry.
		 */
		ictx->entry_nr = entry_nr = ictx->nr_entries - 1;
		ictx->follow_entry = ictx->entries[entry_nr];
	} while (1);
	/*
	 * We found the leaf node thus the predecessor entry we are looking for
	 * is the last entry before the end entry.
	 */
	if (ictx->nr_entries < 2)
		panic("%s(): ictx->nr_entries < 2\n", __FUNCTION__);
	ictx->entry_nr = entry_nr = ictx->nr_entries - 2;
	ictx->entry = ictx->entries[entry_nr];
	ictx->is_match = 1;
	*pred_ictx = ictx;
	ntfs_debug("Done (found).");
	return 0;
err:
	ntfs_error(ictx->idx_ni->vol->mp, "Failed to descend into child node "
			"(error %d).", err);
	return err;
}

/**
 * ntfs_index_ctx_move - move an index context from its tree path to another one
 * @ictx:	index context to move
 * @dst:	destination index context below which to insert @ictx
 *
 * Disconnect the index context @ictx from its tree path and insert it into the
 * tree path to which @dst belongs positioning it immediately below the index
 * context @dst.
 *
 * Locking: - Caller must hold @ictx->idx_ni->lock on the index inode.
 */
static inline void ntfs_index_ctx_move(ntfs_index_context *ictx,
		ntfs_index_context *dst)
{
	ntfs_index_ctx_disconnect(ictx);
	dst->down->up = ictx;
	ictx->down = dst->down;
	ictx->up = dst;
	dst->down = ictx;
}

/**
 * ntfs_index_root_prepare_replace - prepare an index entry to be replaced
 * @ictx:		existing index entry that is going to be replaced
 * @new_ie_size:	size in bytes of the new index entry
 *
 * Resize the existing index entry to the size of the new index entry so that
 * the index root is all set up and ready to receive the new entry.
 *
 * Return 0 on success and ENOSPC if there is not enough space in the index mft
 * record for the new entry.
 *
 * Locking: - Caller must hold @ictx->idx_ni->lock on the index inode for
 *	      writing.
 *	    - The index context @ictx must be locked.
 */
static errno_t ntfs_index_root_prepare_replace(ntfs_index_context *ictx,
		const unsigned new_ie_size)
{
	MFT_RECORD *m = ictx->actx->m;
	INDEX_ENTRY *ie = ictx->entry;
	const u32 muse = le32_to_cpu(m->bytes_in_use);
	const unsigned ie_size = le16_to_cpu(ie->length);
	const int size_change = (int)new_ie_size - (int)ie_size;
	const u32 new_muse = muse + size_change;

	ntfs_debug("Entering.");
	if (new_muse <= le32_to_cpu(m->bytes_allocated)) {
		u8 *vcn_addr;
		ATTR_RECORD *a;
		INDEX_HEADER *ih;

		/*
		 * Resize the index root attribute so it has the appropriate
		 * space for the new index entry to replace the existing entry.
		 * As an optimization we combine the resizing of the index root
		 * attribute and the moving of index entries within the
		 * attribute into a single operation.
		 */
		vcn_addr = (u8*)ie + ie_size - sizeof(VCN);
		memmove((u8*)ie + new_ie_size - sizeof(VCN), vcn_addr,
				muse - (vcn_addr - (u8*)m));
		/* Adjust the mft record to reflect the change in used space. */
		m->bytes_in_use = cpu_to_le32(new_muse);
		/*
		 * Adjust the attribute record to reflect the changes in the
		 * size of the attribute record and in the size of the
		 * attribute value.
		 */
		a = ictx->actx->a;
		a->length = cpu_to_le32(le32_to_cpu(a->length) + size_change);
		a->value_length = cpu_to_le32(le32_to_cpu(a->value_length) +
				size_change);
		/* Adjust the index header to reflect the change in length. */
		ih = ictx->index;
		ih->allocated_size = ih->index_length = cpu_to_le32(
				le32_to_cpu(ih->index_length) + size_change);
		ntfs_debug("Done.");
		return 0;
	}
	ntfs_debug("Failed (not enough space in mft record to replace index "
			"entry in index root attribute).");
	return ENOSPC;
}

/**
 * ntfs_index_block_prepare_replace - prepare an index entry to be replaced
 * @ictx:		existing index entry that is going to be replaced
 * @new_ie_size:	size in bytes of the new index entry
 *
 * Resize the existing index entry to the size of the new index entry so that
 * the index node is all set up and ready to receive the new entry.
 *
 * Return 0 on success and ENOSPC if there is not enough space in the index
 * node for the new entry.
 *
 * Locking: - Caller must hold @ictx->idx_ni->lock on the index inode for
 *	      writing.
 *	    - The index context @ictx must be locked.
 */
static errno_t ntfs_index_block_prepare_replace(ntfs_index_context *ictx,
		const unsigned new_ie_size)
{
	INDEX_HEADER *ih = ictx->index;
	INDEX_ENTRY *ie = ictx->entry;
	const u32 ilen = le32_to_cpu(ih->index_length);
	const unsigned ie_size = le16_to_cpu(ie->length);
	const u32 new_ilen = ilen + new_ie_size - ie_size;

	ntfs_debug("Entering.");
	if (new_ilen <= le32_to_cpu(ih->allocated_size)) {
		u8 *vcn_addr;

		/*
		 * Move the VCN of the index entry to be replaced and
		 * everything that follows it to adapt the space for the new
		 * entry.
		 */
		vcn_addr = (u8*)ie + ie_size - sizeof(VCN);
		memmove((u8*)ie + new_ie_size - sizeof(VCN), vcn_addr,
				ilen - (vcn_addr - (u8*)ih));
		/* Adjust the index header to reflect the change in length. */
		ih->index_length = cpu_to_le32(new_ilen);
		ntfs_debug("Done.");
		return 0;
	}
	ntfs_debug("Failed (not enough space in index block to replace index "
			"entry).");
	return ENOSPC;
}

/**
 * ntfs_index_entry_replace - replace an existing index entry with a new one
 * @ictx:	existing index entry to replace
 * @new_ictx:	new index entry to replace the existing entry with
 *
 * Replace the existing node index entry @ictx->entry with the leaf index entry
 * @new_ictx->entry.
 *
 * Return 0 on success and ENOSPC if there is not enough space for the new
 * entry.
 *
 * Locking: - Caller must hold @ictx->idx_ni->lock on the index inode for
 *	      writing.
 *	    - The index context @ictx must be locked.
 *	    - The index context @new_ictx must be locked.
 */
static errno_t ntfs_index_entry_replace(ntfs_index_context *ictx,
		ntfs_index_context *new_ictx)
{
	INDEX_ENTRY *new_ie = new_ictx->entry;
	INDEX_ENTRY *ie = ictx->entry;
	const unsigned new_ie_size = le16_to_cpu(new_ie->length) + sizeof(VCN);
	errno_t err;

	if (!ictx->is_match || !new_ictx->is_match)
		panic("%s(): !ictx->is_match || !new_ictx->is_match\n",
				__FUNCTION__);
	/* The destination entry is meant to be a node entry. */
	if (!(ictx->entry->flags & INDEX_ENTRY_NODE))
		panic("%s(): !(ictx->entry->flags & INDEX_ENTRY_NODE)\n",
				__FUNCTION__);
	/* The new entry is meant to be a leaf entry. */
	if (new_ictx->entry->flags & INDEX_ENTRY_NODE)
		panic("%s(): new_ictx->entry->flags & INDEX_ENTRY_NODE\n",
				__FUNCTION__);
	if (ictx->is_root)
		err = ntfs_index_root_prepare_replace(ictx, new_ie_size);
	else
		err = ntfs_index_block_prepare_replace(ictx, new_ie_size);
	if (!err) {
		/* Copy the new index entry into the adapted space. */
		memcpy(ie, new_ie, new_ie_size - sizeof(VCN));
		/*
		 * Update the copied index entry to reflect the fact that it is
		 * now an index node entry and has the VCN sub-node pointer at
		 * its tail.
		 */
		ie->length = cpu_to_le16(new_ie_size);
		ie->flags |= INDEX_ENTRY_NODE;
		/* Ensure the updates are written to disk. */
		ntfs_index_entry_mark_dirty(ictx);
	}
	return err;
}

/**
 * ntfs_index_block_free - free an index allocation block
 * @ictx:	index context of the index block to deallocate
 *
 * Deallocate the index allocation block for the index described by the index
 * context @ictx and invalidate the context so the caller can safely release
 * it.
 *
 * We also check if the index allocation attribute can be shrunk as a
 * consequence of the deallocation of the index allocation block and if so and
 * that would actually change the on-disk size of the attribute we shrink it
 * now.
 *
 * If we shrunk the index allocation attribute and the index bitmap attribute
 * is non-resident we shrink the index bitmap attribute also but again only if
 * it would actually change the on-disk size of the attribute.
 *
 * Return 0 on success and errno on error.
 *
 * Locking: - Caller must hold @ictx->idx_ni->lock on the index inode for
 *	      writing.
 *	    - All of the index contexts in the index must be unlocked (this
 *	      includes @ictx, i.e. @ictx must not be locked).
 */
static errno_t ntfs_index_block_free(ntfs_index_context *ictx)
{
	s64 target_pos, bmp_pos, alloc_size;
	ntfs_inode *bmp_ni, *idx_ni = ictx->idx_ni;
	ntfs_volume *vol = idx_ni->vol;
	unsigned page_ofs;
	errno_t err;

	ntfs_debug("Entering.");
	if (ictx->is_locked)
		panic("%s(): ictx->is_locked\n", __FUNCTION__);
	/* Get the index bitmap inode. */
	err = ntfs_attr_inode_get(ictx->base_ni, AT_BITMAP, idx_ni->name,
			idx_ni->name_len, FALSE, LCK_RW_TYPE_EXCLUSIVE,
			&bmp_ni);
	if (err) {
		ntfs_error(vol->mp, "Failed to get index bitmap inode (error "
				"%d).", err);
		return err;
	}
	/*
	 * Zero the bit in the index bitmap corresponding to the index block
	 * being deallocated.
	 */
	target_pos = ictx->vcn << idx_ni->vcn_size_shift >>
			idx_ni->block_size_shift;
	err = ntfs_bitmap_clear_bit(bmp_ni, target_pos);
	if (err) {
		ntfs_error(vol->mp, "Failed to deallocate index block in "
				"index bitmap (error %d).", err);
		lck_rw_unlock_exclusive(&bmp_ni->lock);
		(void)vnode_put(bmp_ni->vn);
		return err;
	}
	/* If this is not the last set bit, we are done. */
	if (target_pos < idx_ni->last_set_bit) {
done:
		ntfs_debug("Done (index records are allocated beyond the "
				"deallocated one).");
out:
		lck_rw_unlock_exclusive(&bmp_ni->lock);
		(void)vnode_put(bmp_ni->vn);
		return 0;
	}
	/*
	 * Scan backwards through the entire bitmap looking for the last set
	 * bit.  If we do not know the old last set bit (@idx_ni->last_set_bit
	 * is -1), start at the end of the bitmap and if we do know it but we
	 * cleared it just now, start at the old last set bit.
	 *
	 * Note we ignore any errors as the truncation is just a disk saving
	 * optimization and is not actually required.  However if an error
	 * occurs we invalidate the last set bit stored in the inode.
	 */
	if (idx_ni->last_set_bit >= 0)
		bmp_pos = idx_ni->last_set_bit >> 3;
	else {
		lck_spin_lock(&bmp_ni->size_lock);
		bmp_pos = bmp_ni->initialized_size - 1;
		lck_spin_unlock(&bmp_ni->size_lock);
	}
	do {
		upl_t upl;
		upl_page_info_array_t pl;
		u8 *bmp_start, *bmp;

		err = ntfs_page_map(bmp_ni, bmp_pos & ~PAGE_MASK_64, &upl,
				&pl, &bmp_start, FALSE);
		if (err) {
			ntfs_debug("Failed to read index bitmap (error %d).",
					err);
			idx_ni->last_set_bit = -1;
			goto out;
		}
		page_ofs = (unsigned)bmp_pos & PAGE_MASK;
		bmp = bmp_start + page_ofs;
		/* Scan backwards through the page. */
		do {
			unsigned bit, byte = *bmp;
			/* If this byte is zero skip it. */
			if (!byte)
				continue;
			/*
			 * Determine the last set bit in the byte.
			 *
			 * TODO: There does not appear to be a fls() function
			 * in the kernel. )-:  If/when the kernel has an fls()
			 * function, switch the below code to use it.
			 *
			 * So we do the "bit = fls(byte) - 1" by hand which is
			 * not very efficient but works.
			 */
			bit = 0;
			if (byte & 0xf0) {
				byte >>= 4;
				bit += 4;
			}
			if (byte & 0x0c) {
				byte >>= 2;
				bit += 2;
			}
			if (byte & 0x02)
				bit++;
			ntfs_page_unmap(bmp_ni, upl, pl, FALSE);
			/*
			 * @bit now contains the last set bit in the byte thus
			 * we can determine the last set bit in the bitmap.
			 */
			idx_ni->last_set_bit = (((bmp_pos & ~PAGE_MASK_64) +
					(bmp - bmp_start)) << 3) + bit;
			if (target_pos < idx_ni->last_set_bit)
				goto done;
			goto was_last_set_bit;
		} while (--bmp >= bmp_start);
		ntfs_page_unmap(bmp_ni, upl, pl, FALSE);
	} while ((bmp_pos -= page_ofs + 1) >= 0);
	/*
	 * We scanned the entire bitmap and it was all zero.  We do not do
	 * anything because truncation of indexes that become empty is done
	 * elsewhere.
	 */
	idx_ni->last_set_bit = -1;
	ntfs_debug("Done (index bitmap has no set bits left).");
	goto out;
was_last_set_bit:
	/*
	 * This was the last set bit.  Check if we would save disk space by
	 * truncating the index allocation attribute and if so do it.  To do
	 * this determine which the first unused cluster is and compare it
	 * against the currently allocated last cluster.
	 *
	 * Note we ignore any errors because it is not essential to resize the
	 * index allocation attribute.  In fact Windows and chkdsk are
	 * perfectly happy with it remaining allocated.  It just means the
	 * index is wasting space on disk and that will be reclaimed when the
	 * index is deleted or when the index is filled again with entries.
	 */
	target_pos = (((idx_ni->last_set_bit + 1) <<
			idx_ni->block_size_shift) + vol->cluster_size_mask) &
			~(s64)vol->cluster_size_mask;
	lck_spin_lock(&idx_ni->size_lock);
	alloc_size = idx_ni->allocated_size;
	lck_spin_unlock(&idx_ni->size_lock);
	if (target_pos >= alloc_size) {
		ntfs_debug("Done (no space would be freed on disk by "
				"truncating the index allocation attribute).");
		goto out;
	}
	err = ntfs_attr_resize(idx_ni, target_pos, 0, NULL);
	if (err) {
		ntfs_debug("Failed to truncate index allocation attribute "
				"(error %d) thus this index will be wasting "
				"space on disk until it is deleted or "
				"repopulated with entries.", err);
		goto out;
	}
	ntfs_debug("Truncated index allocation attribute to reclaim 0x%llx "
			"bytes of disk space.",
			(unsigned long long)(alloc_size - target_pos));
	/*
	 * If the bitmap attribute is non-resident check if we would save disk
	 * space by truncating it, too, and if so do it.  Again we ignore any
	 * errors as it is ok for the bitmap attribute to be left as it is.
	 */
	if (NInoNonResident(bmp_ni)) {
		target_pos = ((((target_pos >> idx_ni->block_size_shift) +
				7) >> 3) + vol->cluster_size_mask) &
				~(s64)vol->cluster_size_mask;
		lck_spin_lock(&bmp_ni->size_lock);
		alloc_size = bmp_ni->allocated_size;
		lck_spin_unlock(&bmp_ni->size_lock);
		if (target_pos >= alloc_size) {
			ntfs_debug("Done (truncated index allocation to free "
					"space on disk but not truncating "
					"index bitmap as no space would be "
					"freed on disk by doing so).");
			goto out;
		}
		err = ntfs_attr_resize(bmp_ni, target_pos, 0, NULL);
		if (err) {
			ntfs_debug("Failed to truncate index bitmap attribute "
					"(error %d) thus this index will be "
					"wasting space on disk until it is "
					"deleted or repopulated with entries.",
					err);
			goto out;
		}
		ntfs_debug("Truncated index bitmap attribute to reclaim "
				"0x%llx bytes of disk space.",
				(unsigned long long)(alloc_size - target_pos));
	}
	ntfs_debug("Done.");
	goto out;
}

/**
 * ntfs_index_make_empty - make an index empty discarding all entries in it
 * @ictx:	index context describing the index to empty
 *
 * Empty the index described by the index context @ictx.  On failure, use the
 * tree described by @ictx to re-allocate the freed index blocks if any have
 * been freed at the point of failure.
 *
 * This is called when the last index entry in an index is being deleted.
 *
 * We need to remove the sub-node from the end entry of the index root and
 * switch the entry to be a leaf entry.
 *
 * We also need to deallocate all index blocks and if possible shrink the index
 * allocation attribute to zero size as well as the index bitmap attribute if
 * it is non-resident.
 *
 * Return 0 on success and errno on error.
 *
 * Locking: - Caller must hold @ictx->idx_ni->lock on the index inode for
 *	      writing.
 *	    - All of the index contexts in the index must be unlocked (this
 *	      includes @ictx, i.e. @ictx must not be locked).
 */
static errno_t ntfs_index_make_empty(ntfs_index_context *ictx)
{
	s64 data_size;
	ntfs_inode *bmp_ni, *idx_ni = ictx->idx_ni;
	MFT_RECORD *m;
	ntfs_attr_search_ctx *actx;
	INDEX_ROOT *ir;
	INDEX_HEADER *ih;
	INDEX_ENTRY *ie;
	ntfs_index_context *start_ictx;
	u32 new_ilen;
	errno_t err;

	ntfs_debug("Entering.");
	if (ictx->is_locked)
		panic("%s(): ictx->is_locked\n", __FUNCTION__);
	/*
	 * Start by zeroing the index bitmap bits corresponding to the index
	 * blocks being deallocated.  For simplicity, we just zero the entire
	 * index bitmap attribute.
	 */
	err = ntfs_attr_inode_get(ictx->base_ni, AT_BITMAP, idx_ni->name,
			idx_ni->name_len, FALSE, LCK_RW_TYPE_EXCLUSIVE,
			&bmp_ni);
	if (err) {
		ntfs_error(idx_ni->vol->mp, "Failed to get index bitmap inode "
				"(error %d).", err);
		return err;
	}
	lck_spin_lock(&bmp_ni->size_lock);
	data_size = bmp_ni->data_size;
	lck_spin_unlock(&bmp_ni->size_lock);
	err = ntfs_attr_set(bmp_ni, 0, data_size, 0);
	if (err) {
		ntfs_error(idx_ni->vol->mp, "Failed to deallocate index "
				"block(s) in index bitmap.");
		goto err;
	}
	/*
	 * We need to get hold of the index root attribute in order to convert
	 * its end entry to a leaf node.
	 */
	err = ntfs_mft_record_map(ictx->base_ni, &m);
	if (err) {
		ntfs_error(idx_ni->vol->mp, "ntfs_mft_record_map() failed "
				"(error %d).", err); 
		goto err;
	}
	actx = ntfs_attr_search_ctx_get(ictx->base_ni, m);
	if (!actx) {
		err = ENOMEM;
		goto unm_err;
	}
	/* Find the index root attribute in the mft record. */
	err = ntfs_attr_lookup(AT_INDEX_ROOT, idx_ni->name, idx_ni->name_len,
			0, NULL, 0, actx);
	if (err) {
		if (err == ENOENT) {
			ntfs_error(idx_ni->vol->mp, "Index root attribute "
					"missing in inode 0x%llx.  Run "
					"chkdsk.",
					(unsigned long long)idx_ni->mft_no);
			err = EIO;
			NVolSetErrors(idx_ni->vol);
		}
		goto put_err;
	}
	/* Get to the index root value. */
	ir = (INDEX_ROOT*)((u8*)actx->a + le16_to_cpu(actx->a->value_offset));
	ih = (INDEX_HEADER*)&ir->index;
	/* The first and last index entry. */
	ie = (INDEX_ENTRY*)((u8*)ih + le32_to_cpu(ih->entries_offset));
	if (!(ie->flags & INDEX_ENTRY_END))
		panic("%s(): !(ie->flags & INDEX_ENTRY_END)\n", __FUNCTION__);
	if (!(ie->flags & INDEX_ENTRY_NODE))
		panic("%s(): !(ie->flags & INDEX_ENTRY_NODE)\n", __FUNCTION__);
	/*
	 * Remove the sub-node pointer from the index entry and shrink the
	 * index root attribute appropriately.
	 */
	ie->length = cpu_to_le16(le16_to_cpu(ie->length) - sizeof(VCN));
	ie->flags &= ~INDEX_ENTRY_NODE;
	new_ilen = le32_to_cpu(ih->index_length) - sizeof(VCN);
	ih->allocated_size = ih->index_length = cpu_to_le32(new_ilen);
	ih->flags &= ~LARGE_INDEX;
	err = ntfs_resident_attr_value_resize(actx->m, actx->a,
			offsetof(INDEX_ROOT, index) + new_ilen);
	/* We are shrinking the index root so the resize cannot fail. */
	if (err)
		panic("%s(): err\n", __FUNCTION__);
	/* Ensure the changes are written to disk. */
	NInoSetMrecNeedsDirtying(actx->ni);
	ntfs_attr_search_ctx_put(actx);
	ntfs_mft_record_unmap(ictx->base_ni);
	/*
	 * Now deal with the index allocation attribute by truncating it to
	 * zero length.
	 *
	 * Note we ignore any errors because it is not essential to resize the
	 * index allocation attribute.  In fact Windows and chkdsk are
	 * perfectly happy with it remaining allocated.  It just means the
	 * index is wasting space on disk and that will be reclaimed when the
	 * index is deleted or when the index is filled again with entries.
	 */
	err = ntfs_attr_resize(idx_ni, 0, 0, ictx);
	if (err)
		ntfs_debug("Failed to truncate index allocation attribute to "
				"zero size thus this index will be wasting "
				"space on disk until it is deleted or "
				"repopulated with entries.");
	/*
	 * Finally, if the index bitmap attribute is non-resident truncate it
	 * to zero length, too.  Again we ignore any errors as it is ok for the
	 * bitmap attribute to be left as it is.
	 */
	if (!err && NInoNonResident(bmp_ni)) {
		err = ntfs_attr_resize(bmp_ni, 0, 0, ictx);
		if (err)
			ntfs_debug("Failed to truncate index bitmap attribute "
					"to zero size thus this index will be "
					"wasting space on disk until it is "
					"deleted or repopulated with "
					"entries.");
	}
	lck_rw_unlock_exclusive(&bmp_ni->lock);
	(void)vnode_put(bmp_ni->vn);
	/*
	 * We no longer have any index blocks allocated so invalidate our cache
	 * of the last set bit.
	 */
	idx_ni->last_set_bit = -1;
	ntfs_debug("Done.");
	return 0;
put_err:
	ntfs_attr_search_ctx_put(actx);
unm_err:
	ntfs_mft_record_unmap(ictx->base_ni);
err:
	/*
	 * Re-allocate the deallocated index block(s).  This is safe because
	 * the index inode mutex is held throughout.
	 */
	start_ictx = ictx;
	do {
		int err2;

		 /*
		  * Skip the index root as it does not have a bit in the
		  * bitmap.
		  */
		if (ictx->is_root)
			continue;
		err2 = ntfs_bitmap_set_bit(bmp_ni, ictx->vcn <<
				idx_ni->vcn_size_shift >>
				idx_ni->block_size_shift);
		if (err2) {
			ntfs_error(idx_ni->vol->mp, "Failed to undo "
					"deallocation of index block in index "
					"bitmap (error %d) of inode 0x%llx.  "
					"Leaving inconsistent metadata.  Run "
					"chkdsk.", err2,
					(unsigned long long)idx_ni->mft_no);
			NVolSetErrors(idx_ni->vol);
		}
	} while ((ictx = ictx->up) != start_ictx);
	lck_rw_unlock_exclusive(&bmp_ni->lock);
	(void)vnode_put(bmp_ni->vn);
	ntfs_error(idx_ni->vol->mp, "Failed (error %d).", err);
	return err;
}

/**
 * ntfs_index_entry_delete - delete an index entry
 * @ictx:	index context describing the index entry to delete
 *
 * Delete the index entry described by the index context @ictx from the index.
 *
 * Return 0 on success and errno on error.  A special case is a return code of
 * -EAGAIN (negative, not EAGAIN) which means that the B+tree was rearranged
 * into a different consistent state to make the deletion possible but now the
 * lookup and delete has to be repeated as the index entry to be deleted may
 * have changed its position in the tree thus a new lookup is required to be
 * able to delete it.  Doing this is not terribly efficient but we are only
 * talking about a handful of cases in a single delete of tens of thousands of
 * files so it does not matter if that is inefficient.  On the plus side doing
 * things this way means we do not need to keep track of the entry to be
 * deleted when rearranging the tree which saves time and makes the code much
 * simpler.
 *
 * Note that we do not update the array of index entry pointers nor the number
 * of entries in the array because on success it means that the index entry has
 * been removed and the function is done, i.e. the array of index entry
 * pointers will not be used any more and on error the index contect becomes
 * invalid so there is no need to update any of the pointers.
 *
 * Locking: - Caller must hold @ictx->idx_ni->lock on the index inode for
 *	      writing.
 *	    - The index context @ictx must be locked.
 */
int ntfs_index_entry_delete(ntfs_index_context *ictx)
{
	leVCN vcn;
	ntfs_inode *idx_ni = ictx->idx_ni;
	ntfs_index_context *pred_ictx, *parent_ictx, *suc_ictx, *put_ictx;
	ntfs_index_context *deallocate_ictx;
	INDEX_HEADER *ih;
	INDEX_ENTRY *ie, *next_ie, *pull_down_entry;
	MFT_RECORD *m;
	unsigned ie_size, pred_ie_size, pull_down_entry_nr, pull_down_ie_size;
	unsigned old_parent_entry_nr, old_parent_ie_size, move_ie_size;
	errno_t err;
	u32 new_ilen;
	BOOL is_root = ictx->is_root;

	ntfs_debug("Entering.");
	if (!ictx->is_locked)
		panic("%s(): !ictx->is_locked\n", __FUNCTION__);
	deallocate_ictx = parent_ictx = put_ictx = NULL;
	ih = ictx->index;
	ie = ictx->entry;
	/* We cannot delete end entries as they do not contain any key/data. */
	if (ie->flags & INDEX_ENTRY_END)
		panic("%s(): ie->flags & INDEX_ENTRY_END\n", __FUNCTION__);
	ie_size = le16_to_cpu(ie->length);
	next_ie = (INDEX_ENTRY*)((u8*)ie + ie_size);
	/*
	 * There are two types of index entry: Either it is a leaf entry, i.e.
	 * it is in a leaf node and thus has no children or it is in an index
	 * node entry, i.e. it is in an index block node and thus has children
	 * which we need to deal with.  If it is a leaf entry, skip onto leaf
	 * entry deletion.
	 */
	if (!(ie->flags & INDEX_ENTRY_NODE))
		goto delete_leaf_entry;
	/*
	 * The node the entry to be deleted is in has sub-node(s), i.e. it is
	 * not a leaf node, and the entry to be deleted is not a leaf entry.
	 * We have to replace it with its predecessor (which by definition is a
	 * leaf entry).  There are three cases of increasing complexity:
	 *
	 * 1) The simplest case is when the predecessor is not the only entry
	 * in its (leaf) node thus it can be simply removed without any tree
	 * rebalancing needing to be done on its node and in addition the
	 * predecessor entry can replace the entry to be deleted without
	 * overflowing the node, i.e. there is enough space in the node we are
	 * deleting the entry from to fit its predecessor entry thus it is a
	 * matter of a simple replace without the need to change anything else
	 * in the tree.
	 *
	 * 2) The slightly more complicated case is the same as above case 1)
	 * except that there is not enough space in the node the entry to be
	 * deleted is in to fit its predecessor entry, thus we need to split
	 * the node the entry is being deleted from and promote the median
	 * entry to the parent node which may then overflow the parent node and
	 * so on up to the root node.  A particular annoyance here is that
	 * depending on the implementation details it is possible for the entry
	 * to be deleted to be the median entry and thus be promoted to its
	 * parent or alternatively it is possible for its predecessor entry
	 * that is to replace the entry to be deleted to have to be promoted.
	 * A further pitfall is that if the entry to be deleted is behind the
	 * median entry, i.e. it is on the right of the median entry, then it
	 * will be moved to a different node (the new right-hand node to the
	 * old node being split) thus the replace needs to happen in a
	 * different place.  If we use ntfs_index_entry_add() as it is to take
	 * care of the split&promote on insertion this last case would cause us
	 * the problem that the pointers would be all out of date.  We would
	 * need ntfs_index_entry_add() to update the pointers and to switch the
	 * right and left nodes in that case.  Perhaps we need an
	 * ntfs_index_entry_replace() that can share a lot of code with
	 * ntfs_index_entry_add() perhaps even just a flag to
	 * ntfs_index_entry_add() to indicate replace?  Then we would not need
	 * to worry about the pointers becoming out of date as the replace
	 * would be done already for us.
	 *
	 * 3) The more complicated case is when the predecessor entry is the
	 * last entry in its (leaf) node thus we cannot use it to replace the
	 * entry to be deleted without rebalancing the tree.  In this case we
	 * have to rebalance the tree so that the predecessor entry is no
	 * longer the last entry in its (leaf) node.  Once that is successfully
	 * done we have reduced the problem to either case 1) or case 2) above
	 * or in fact to a completely different case (see below).  The benefit
	 * of doing the rebalancing first without regard to the delete and
	 * replace that are to take place is that the tree ends up in a
	 * consistent state, just a different one to what it was before, thus
	 * we do not need to rollback to the original state any more thus error
	 * handling is greatly simplified.  The pitfall here is that doing the
	 * balancing may profoundly change the tree to the extent that the
	 * entry to be deleted may be moved to somewhere completely different
	 * which we somehow need to be able to cope with.  This can have
	 * positive side effects, too, as it for example can lead to the entry
	 * to be deleted being turned into a leaf entry thus its deletion turns
	 * into a delete of a leaf entry thus the planned replace does not need
	 * to happen at all.  This is the "completely different case" mentioned
	 * above.  Usually this case will have been caused by a merge of two
	 * neighbouring nodes with pulldown of the parent entry (which happens
	 * to be the entry to be deleted) thus the entry to be deleted will not
	 * only be a leaf entry but it will also not be the last entry in the
	 * leaf node thus it can be deleted with a simple delete.  Otherwise it
	 * is not a disaster and we just need to rebalance the tree again as we
	 * did just now but this time making sure that the entry to be deleted
	 * is not the only entry in the node (compare to above where we were
	 * making sure that the leaf predecessor entry is not the only entry in
	 * its leaf node) and then we can proceed with a simple delete of the
	 * leaf node.  Because the tree is balanced at all steps we do not
	 * actually care about handling the delete of the entry in the case
	 * that it turned into a leaf entry and we just leave it to the leaf
	 * entry deletion code to deal with.
	 *
	 * That is quite enough description now, let the games begin.  First,
	 * investigate the index entry to be deleted and its predecessor to
	 * determine which of the above categories the deletion falls into.
	 *
	 * Locate the predecessor entry.  Because the predecessor may be the
	 * only entry in its (leaf) node in which case it would cause the tree
	 * to be rebalanced, we record the path taken to the predecessor entry
	 * by simply extending the existing tree path that points to the entry
	 * to be deleted.
	 */
	err = ntfs_index_lookup_predecessor(ictx, &pred_ictx);
	if (err) {
		ntfs_error(idx_ni->vol->mp, "Failed to look up predecessor of "
				"index node entry to be deleted (error %d).",
				err);
		return err;
	}
	/*
	 * If the predecessor is not the only entry other than the end entry in
	 * its node we can simply remove it from its node and use it to replace
	 * the entry to be deleted.
	 */
	if (pred_ictx->nr_entries > 2)
		goto simple_replace;
	if (pred_ictx->nr_entries < 2)
		panic("%s(): pred_ictx->nr_entries < 2\n", __FUNCTION__);
	/*
	 * The predecessor is the only entry (other than the end entry) in its
	 * node thus we cannot simply remove it from its node as that would
	 * cause the B+tree to become imbalanced.
	 *
	 * There are two different cases we need to consider for which we need
	 * to look up the predecessor of the current predecessor leaf entry.
	 * To do this we ascend the tree looking for a node with entries in it.
	 * The first node containing any entries is the node containing the
	 * predecessor entry (which is not a leaf entry).
	 *
	 * 1) If we reach the original node containing the entry to be deleted
	 * without finding any non-empty nodes on the way the whole sub-tree
	 * below the entry to be deleted will become empty after using the
	 * predecessor leaf entry to replace the entry to be deleted.  Thus we
	 * can deallocate all nodes in this sub-tree and instead of replacing
	 * the entry to be deleted we remove it altogether and we then move the
	 * predecessor leaf entry that is now homeless in front of its new
	 * successor leaf entry, thus effectively merging the node containing
	 * the predecessor with its right-hand sibling.
	 *
	 * Note that the insertion of the predecessor leaf entry into the node
	 * containing its successor leaf entry may require a split of the node
	 * we are insert into.
	 *
	 * 2) If we find a predecessor for the predecessor entry before we
	 * reach the original node containing the entry to be deleted we break
	 * what we need to do into two operations.  First, we rebalance the
	 * tree so that the predecessor entry of the entry to be deleted is no
	 * longer the only entry in its leaf node.  This simplifies the
	 * deletion of the entry to a case we have already solved as we can now
	 * simply remove the predecessor entry from its node and use it to
	 * replace the entry to be deleted.  Thus, second, we use the already
	 * existing code to do a simple replace.  Should that fail it does not
	 * matter as we are leaving a fully consistent B+tree tree behind.
	 * Breaking this case up into two means we incur a little bit of
	 * overhead for the extra locking of the predecessor node and for the
	 * extra memmove()s and memcpy() of the predecessor entry.  But this
	 * simplifies error handling and makes the code so much simpler that it
	 * is definitely worth paying the price in overhead.
	 *
	 * Enough discussion, time to do it.  Start by going up the tree
	 * looking for a non-empty node or the original node containing the
	 * entry to be deleted so we can determine if we have the above case 1
	 * or 2.
	 *
	 * We must be at the bottom of the current tree path or things will get
	 * very confused.
	 */
	if (!pred_ictx->down->is_root)
		panic("%s(): !pred_ictx->down->is_root\n", __FUNCTION__);
	parent_ictx = pred_ictx->up;
	put_ictx = deallocate_ictx = pred_ictx;
	ntfs_index_ctx_disconnect_reinit(pred_ictx);
	/*
	 * Make a note of the size of the predecessor entry that we are going
	 * to move and unlock it for now.
	 */
	move_ie_size = le16_to_cpu(pred_ictx->entry->length);
	ntfs_index_ctx_unlock(pred_ictx);
	/* Now ascend the tree until we find a non-empty node. */
	while (parent_ictx->nr_entries <= 1) {
		if (parent_ictx->nr_entries != 1)
			panic("%s(): parent_ictx->nr_entries != 1\n",
					__FUNCTION__);
		/*
		 * Empty index node.  Move it to the list of index nodes to be
		 * deallocated and proceed to its parent.
		 */
		pred_ictx = parent_ictx;
		parent_ictx = parent_ictx->up;
		ntfs_index_ctx_move(pred_ictx, deallocate_ictx);
	}
	/*
	 * We have a non-empty parent node.  Check whether this is the node
	 * containing the entry to be deleted (case 1) or whether it is an
	 * intervening node (case 2).  In the latter case we need to rearrange
	 * the tree.
	 */
	if (parent_ictx != ictx)
		goto rearrange_tree;
	/*
	 * Now that we know we have case 1, we need to find the leaf node
	 * containing the successor entry of the entry to be deleted.  To do
	 * this we repoint the tree path to the entry on the right-hand side of
	 * the entry to be deleted and descend down into the first entry of
	 * each node until we reach the leaf-node.
	 *
	 * We need to lock the node containing the entry to be deleted and make
	 * a note of it as we are going to delete it later.
	 */
	err = ntfs_index_ctx_relock(parent_ictx);
	if (err) {
		ntfs_error(idx_ni->vol->mp, "Failed to relock the parent "
				"index node (error %d).", err);
		goto put_err;
	}
	/*
	 * NOTE: pull_down_* == to be deleted (we are just reusing existing
	 * code and variables hence the (mis)naming...
	 */
	pull_down_entry = parent_ictx->follow_entry;
	pull_down_entry_nr = parent_ictx->entry_nr;
	/*
	 * Repoint the path to the entry on the right-hand side of the entry to
	 * be deleted so we can descend to the leaf node containing the
	 * successor entry.
	 */
	parent_ictx->follow_entry = (INDEX_ENTRY*)((u8*)pull_down_entry +
			le16_to_cpu(pull_down_entry->length));
	parent_ictx->entry_nr++;
	/* Finally descend to the leaf node containing the successor entry. */
	suc_ictx = parent_ictx;
	do {
		err = ntfs_index_descend_into_child_node(&suc_ictx);
		if (err) {
			ntfs_error(idx_ni->vol->mp, "Failed to descend to "
					"node containing successor entry "
					"(error %d).", err);
			goto put_err;
		}
		if (!suc_ictx->is_locked)
			panic("%s(): !suc_ictx->is_locked\n", __FUNCTION__);
		suc_ictx->follow_entry = suc_ictx->entries[0];
	} while (suc_ictx->index->flags & INDEX_NODE);
	if (suc_ictx->follow_entry->flags & INDEX_ENTRY_NODE)
		panic("%s(): suc_ictx->follow_entry->flags & "
				"INDEX_ENTRY_NODE\n", __FUNCTION__);
	/*
	 * Can the predecessor entry simply be inserted in front of the
	 * successor leaf entry without overflowing the node?  If not we need
	 * to split the node and then restart the delete.
	 */
	if (move_ie_size > suc_ictx->bytes_free) {
split_and_restart_case_1:
		ntfs_debug("Splitting index node before add on delete (case "
				"1).");
		ie_size = move_ie_size;
		goto split_and_restart;
	}
	/*
	 * The predecessor entry can simply be inserted in front of the
	 * successor leaf entry.
	 *
	 * We need to have locked both nodes to be able to do the insert thus
	 * we may need to unlock the locked successor node to ensure that the
	 * node lock is always taken in ascending page offset order so we avoid
	 * deadlocks.  Also we have to consider the case where the two index
	 * nodes are in the same page in which case we need to share the index
	 * lock.
	 */
	if (deallocate_ictx->is_locked)
		panic("%s(): deallocate_ictx->is_locked\n", __FUNCTION__);
	err = ntfs_index_ctx_lock_two(suc_ictx, deallocate_ictx);
	if (err)
		goto put_err;
	/*
	 * We need to re-check the amount of free space as it can have changed
	 * whilst we dropped the lock on @suc_ictx if that was necessary.  When
	 * this happens simply drop the lock on @deallocate_ictx if we took it
	 * and go back to the previous code dealing with the not enough space
	 * case.
	 */
	if (move_ie_size > suc_ictx->bytes_free) {
		if (deallocate_ictx->is_locked)
			ntfs_index_ctx_unlock(deallocate_ictx);
		goto split_and_restart_case_1;
	}
	/*
	 * Both nodes are locked and this is a simple insert, so go ahead and
	 * do it.  We have already checked that there is enough space so it
	 * cannot fail.
	 *
	 * Move all the entries out of the way to make space for the
	 * predecessor entry to be copied in.
	 */
	err = ntfs_index_node_make_space(suc_ictx, move_ie_size);
	if (err)
		panic("%s(): err\n", __FUNCTION__);
	/*
	 * Copy the predecessor index entry into the created space in front of
	 * the successor entry.
	 */
	memcpy(suc_ictx->follow_entry, deallocate_ictx->entry, move_ie_size);
	/* Ensure the updates are written to disk. */
	ntfs_index_entry_mark_dirty(suc_ictx);
	/*
	 * We are done both with the predecessor and successor nodes so release
	 * their locks.
	 */
	if (deallocate_ictx->is_locked)
		ntfs_index_ctx_unlock(deallocate_ictx);
	if (!suc_ictx->is_locked)
		panic("%s(): !suc_ictx->is_locked\n", __FUNCTION__);
	ntfs_index_ctx_unlock(suc_ictx);
	/*
	 * We now have to lock the original node containing the entry to be
	 * deleted and we need to switch it to be the current index context and
	 * to point to the entry to be deleted.  Note this is a simple delete
	 * just like for a leaf entry as we have taken care of its child leaf
	 * node(s) already and will deallocate it(them) later.
	 */
	if (parent_ictx->is_locked)
		panic("%s(): parent_ictx->is_locked\n", __FUNCTION__);
	err = ntfs_index_ctx_relock(parent_ictx);
	if (err) {
		ntfs_error(idx_ni->vol->mp, "Failed to relock the parent "
				"index node (error %d).", err);
		/*
		 * Undo the insertion of the predecessor entry in front of the
		 * successor entry by moving the successor and all following
		 * entries back into their old place and resetting the index
		 * length to the old size.
		 */
		err = ntfs_index_ctx_relock(suc_ictx);
		if (err) {
			ntfs_error(idx_ni->vol->mp, "Failed to relock index "
					"context (error %d).  Leaving corrupt "
					"index.  Run chkdsk.", err);
			NVolSetErrors(idx_ni->vol);
		} else {
			/*
			 * @suc_ictx cannot be the index root or the below
			 * code would be wrong.
			 */
			if (suc_ictx->is_root)
				panic("%s(): suc_ctx->is_root\n", __FUNCTION__);
			ie = suc_ictx->follow_entry;
			ie_size = le16_to_cpu(ie->length);
			next_ie = (INDEX_ENTRY*)((u8*)ie + ie_size);
			ih = suc_ictx->index;
			new_ilen = le32_to_cpu(ih->index_length);
			memmove(ie, next_ie, new_ilen - ((u8*)next_ie -
					(u8*)ih));
			ih->index_length = cpu_to_le32(new_ilen - ie_size);
			ntfs_index_entry_mark_dirty(suc_ictx);
		}
		goto put_err;
	}
	ictx = parent_ictx;
	parent_ictx->entry = parent_ictx->entries[pull_down_entry_nr];
	parent_ictx->entry_nr = pull_down_entry_nr;
	goto prep_simple_delete;
rearrange_tree:
	/*
	 * We now know we have case 2, i.e. we have a non-empty, intervening
	 * node containing the predecessor (non-leaf) entry of the predecessor
	 * entry of the entry to be deleted and we need to use it to rearrange
	 * the tree such that the (leaf) predecessor entry of the entry to be
	 * deleted no longer is the only entry in its (leaf) node.
	 *
	 * To do this we merge the (leaf) node containing the predecessor entry
	 * of the entry to be deleted with its left-hand sibling (leaf) node.
	 * This involves pulling the (non-leaf) predecessor entry of the (leaf)
	 * predecessor entry of the entry to be deleted down to behind its
	 * (leaf) predecessor entry.
	 *
	 * Thus, we need to now locate the leaf node containing the predecessor
	 * entry behind which we need to insert the two predecessor entries.
	 *
	 * To be able to do this we need to lock the node containing the (non-
	 * leaf) predecessor of the predecessor of the entry to be deleted and
	 * make a note of it as we are going to delete it later.  We also need
	 * to repoint the tree path to descend into the (non-leaf) predecessor
	 * entry.
	 */
	if (parent_ictx->is_locked)
		panic("%s(): parent_ictx->is_locked\n", __FUNCTION__);
	if (parent_ictx->is_root)
		panic("%s(): parent_ictx->is_root\n", __FUNCTION__);
	err = ntfs_index_ctx_relock(parent_ictx);
	if (err)
		goto put_err;
	parent_ictx->entry_nr--;
	parent_ictx->follow_entry = parent_ictx->entries[parent_ictx->entry_nr];
	pull_down_ie_size = le16_to_cpu(parent_ictx->follow_entry->length) -
			sizeof(VCN);
	/* Now descend to the leaf predecessor entry. */
	suc_ictx = parent_ictx;
	do {
		err = ntfs_index_descend_into_child_node(&suc_ictx);
		if (err) {
			ntfs_error(idx_ni->vol->mp, "Failed to descend to "
					"node containing leaf predecessor "
					"entry of non-leaf predecessor entry "
					"of leaf predecessor entry of entry "
					"to be deleted (error %d).", err);
			goto put_err;
		}
		if (!suc_ictx->is_locked)
			panic("%s(): !suc_ictx->is_locked\n", __FUNCTION__);
		suc_ictx->entry_nr = suc_ictx->nr_entries - 1;
		suc_ictx->follow_entry = suc_ictx->entries[suc_ictx->entry_nr];
	} while (suc_ictx->index->flags & INDEX_NODE);
	if (suc_ictx->follow_entry->flags & INDEX_ENTRY_NODE)
		panic("%s(): suc_ictx->follow_entry->flags & "
				"INDEX_ENTRY_NODE\n", __FUNCTION__);
	/*
	 * Can the entry to be pulled down and the original predecessor entry
	 * to be merged in simply be inserted after the located predecessor
	 * entry without overflowing the node?  If not we need to split the
	 * node and then restart the delete.
	 */
	if (pull_down_ie_size + move_ie_size > suc_ictx->bytes_free) {
split_and_restart_case_2:
		ntfs_debug("Splitting index node before add on delete (case "
				"2).");
		ie_size = pull_down_ie_size + move_ie_size;
		goto split_and_restart;
	}
	/*
	 * We know we have enough space so we cannot fail.  Need to lock the
	 * non-leaf predecessor that we want to pull down.
	 */
	if (parent_ictx->is_locked)
		panic("%s(): parent_ictx->is_locked\n", __FUNCTION__);
	err = ntfs_index_ctx_lock_two(suc_ictx, parent_ictx);
	if (err)
		goto put_err;
	/*
	 * We need to re-check the amount of free space as it can have changed
	 * whilst we dropped the lock on @suc_ictx if that was necessary.  When
	 * this happens simply drop the lock on @parent_ictx if we took it and
	 * go back to the previous code dealing with the not enough space case.
	 */
	if (pull_down_ie_size + move_ie_size > suc_ictx->bytes_free) {
		if (parent_ictx->is_locked)
			ntfs_index_ctx_unlock(parent_ictx);
		goto split_and_restart_case_2;
	}
	/*
	 * Make enough space in the destination leaf node to accomodate both
	 * the entries.
	 */
	err = ntfs_index_node_make_space(suc_ictx, pull_down_ie_size +
			move_ie_size);
	if (err)
		panic("%s(): err\n", __FUNCTION__);
	/*
	 * Copy the non-leaf predecessor entry into place and convert it to a
	 * leaf entry.
	 */
	ie = suc_ictx->follow_entry;
	pull_down_entry = parent_ictx->follow_entry;
	memcpy(ie, pull_down_entry, pull_down_ie_size);
	ie->length = cpu_to_le16(pull_down_ie_size);
	ie->flags &= ~INDEX_ENTRY_NODE;
	/*
	 * Now delete the entry from its original node and transfer its VCN
	 * sub-node pointer to the next entry (which is the end entry).
	 */
	vcn = *(leVCN*)((u8*)pull_down_entry + pull_down_ie_size);
	ih = parent_ictx->index;
	new_ilen = le32_to_cpu(ih->index_length) - (pull_down_ie_size +
			sizeof(VCN));
	memmove(pull_down_entry, (u8*)pull_down_entry + pull_down_ie_size +
			sizeof(VCN), new_ilen - ((u8*)pull_down_entry -
			(u8*)ih));
	*(leVCN*)((u8*)pull_down_entry + le16_to_cpu(pull_down_entry->length) -
			sizeof(VCN)) = vcn;
	/* Update the index size, too. */
	ih->index_length = cpu_to_le32(new_ilen);
	/* Ensure the updates are written to disk. */
	ntfs_index_entry_mark_dirty(parent_ictx);
	/* Update the index context as well. */
	parent_ictx->bytes_free += pull_down_ie_size + sizeof(VCN);
	parent_ictx->nr_entries--;
	/* We are done with the non-leaf predecessor node so unlock it. */
	if (parent_ictx->is_locked)
		ntfs_index_ctx_unlock(parent_ictx);
	/*
	 * Now need to move over the original leaf predecessor entry.  Lock its
	 * node again.
	 */
	if (deallocate_ictx->is_locked)
		panic("%s(): deallocate_ictx->is_locked\n", __FUNCTION__);
	err = ntfs_index_ctx_lock_two(suc_ictx, deallocate_ictx);
	if (err)
		goto put_err;
	/* Copy the original leaf predecessor entry into place. */
	memcpy((u8*)suc_ictx->follow_entry + pull_down_ie_size,
			deallocate_ictx->follow_entry, move_ie_size);
	/* Ensure the updates are written to disk. */
	ntfs_index_entry_mark_dirty(suc_ictx);
	/* We are finished rearranging the tree so unlock both nodes. */
	ntfs_index_ctx_unlock(suc_ictx);
	if (deallocate_ictx->is_locked)
		ntfs_index_ctx_unlock(deallocate_ictx);
	/*
	 * The last thing to do is to deallocate the disconnected index node(s)
	 * and then we can restart the delete operation which now involves a
	 * simple replace to complete the delete.
	 */
	parent_ictx = deallocate_ictx;
	do {
		if (parent_ictx->is_root)
			panic("%s(): parent_ictx->is_root\n", __FUNCTION__);
		err = ntfs_index_block_free(parent_ictx);
		if (err) {
			ntfs_error(idx_ni->vol->mp, "Failed to deallocate no "
					"longer used index block vcn 0x%llx "
					"(error %d).  Run chkdsk to recover "
					"the lost index block.",
					(unsigned long long)parent_ictx->vcn,
					err);
			NVolSetErrors(idx_ni->vol);
		}
	} while ((parent_ictx = parent_ictx->up) != deallocate_ictx);
	/*
	 * Release the sub-tree paths that the caller no longer has a reference
	 * to.
	 */
	ntfs_index_ctx_put(put_ictx);
	/* Reset the state machine to original values. */
	put_ictx = deallocate_ictx = NULL;
	/*
	 * The tree is now fully consistent, no nodes are locked, and @suc_ictx
	 * is the leaf node containing the predecessor entry of the entry to
	 * be deleted and the predecessor entry no longer is the only entry
	 * other than the end entry thus it can simply be used to replace the
	 * entry to be deleted.
	 *
	 * Setup everything so we can jump straight into the simple replace
	 * code.  Note we lock the original entry to be deleted as well so that
	 * the simple replace code does not have to drop the lock on the
	 * predecessor node in order to lock the original node which it may
	 * have to do otherwise.
	 *
	 * The simple replace code requires the predecessor to be in
	 * @pred_ictx.
	 */
	pred_ictx = suc_ictx;
	err = ntfs_index_ctx_lock_two(suc_ictx, ictx);
	if (err)
		return err;
	/*
	 * Repoint the predecessor context to point to the correct entry and
	 * update it to reflect the addition of the two index entries.
	 */
	pred_ictx->entry = (INDEX_ENTRY*)((u8*)pred_ictx->entry +
			pull_down_ie_size);
	pred_ictx->entry_nr++;
	pred_ictx->is_match = 1;
	pred_ictx->bytes_free -= pull_down_ie_size + move_ie_size;
	pred_ictx->nr_entries += 2;
	if (pred_ictx->nr_entries > pred_ictx->max_entries)
		panic("%s(): pred_ictx->nr_entries > pred_ictx->max_entries\n",
				__FUNCTION__);
	pred_ictx->entries[pred_ictx->entry_nr] = pred_ictx->entry;
	pred_ictx->entries[pred_ictx->entry_nr + 1] = (INDEX_ENTRY*)(
			(u8*)pred_ictx->entry + move_ie_size);
	/* Everything is set up.  Do the simple replace. */
	/* goto simple_replace; */
simple_replace:
	/*
	 * The predecessor can simply be removed from its node.
	 *
	 * We need to have locked both nodes to be able to do the replace thus
	 * we may need to unlock the locked predecessor node to ensure that
	 * page locks are only taken in ascending page offset order so we avoid
	 * deadlocks.
	 */
	if (!pred_ictx->is_locked)
		panic("%s(): !pred_ictx->is_locked\n", __FUNCTION__);
	if (!ictx->is_locked) {
		err = ntfs_index_ctx_lock_two(pred_ictx, ictx);
		if (err)
			return err;
	}
	/*
	 * Can the predecessor simply replace the entry to be deleted without
	 * overflowing the node containing the entry to be deleted?  If not we
	 * need to split the node and then restart the delete.
	 */
	pred_ie_size = le16_to_cpu(pred_ictx->entry->length);
	ie_size = le16_to_cpu(ictx->entry->length) - sizeof(VCN);
	if ((int)pred_ie_size - (int)ie_size > (int)ictx->bytes_free) {
		ntfs_debug("Splitting index node before add on delete (case "
				"3).");
		/*
		 * Drop the lock on the predecessor or transfer it to the node
		 * containing the entry to be deleted if they share the same
		 * page (in which case @ictx is not marked locked).
		 */
		if (!pred_ictx->is_locked)
			panic("%s(): !pred_ictx->is_locked\n", __FUNCTION__);
		if (ictx->is_locked) {
			/*
			 * @ictx is locked thus it cannot be sharing the lock
			 * with @pred_ictx.  Unlock @pred_ictx.
			 */
			ntfs_index_ctx_unlock(pred_ictx);
		} else {
			/*
			 * @ictx is not locked thus it must be sharing the lock
			 * with @pred_ictx.  Transfer the lock across.
			 */
			if (ictx->is_root)
				panic("%s(): ictx->is_root\n", __FUNCTION__);
			if (ictx->upl_ofs != pred_ictx->upl_ofs)
				panic("%s(): ictx->upl_ofs != "
						"pred_ictx->upl_ofs\n",
						__FUNCTION__);
			ictx->is_locked = 1;
			pred_ictx->is_locked = 0;
		}
		if (!ictx->is_locked)
			panic("%s(): !ictx->is_locked\n", __FUNCTION__);
		suc_ictx = ictx;
		ie_size = pred_ie_size - ie_size;
		goto split_and_restart;
	}
	/*
	 * Both nodes are locked and this is a simple replace, so go ahead and
	 * do it.  We have already checked that there is enough space so it
	 * cannot fail.
	 */
	err = ntfs_index_entry_replace(ictx, pred_ictx);
	if (err)
		panic("%s(): err\n", __FUNCTION__);
	/* We are done with the current entry so release it. */
	if (ictx->is_locked)
		ntfs_index_ctx_unlock(ictx);
	/*
	 * We have now replaced the entry to be deleted but at the moment we
	 * have two copies of the predecessor entry, so delete the original
	 * copy from its (leaf) node.  We need to switch the predecessor index
	 * context to be the current context so we can just pretend it is the
	 * entry to be deleted.  We only need to setup the fields relevant to
	 * index block node deletion as the predecessor cannot be in the index
	 * root node.
	 */
	ictx = pred_ictx;
	goto prep_simple_delete;
delete_leaf_entry:
	/*
	 * If the entry is in the index root or it is not the only entry (other
	 * than the end entry) in the leaf node it can simply be removed.
	 */
	if (ictx->nr_entries > 2 || is_root)
		goto simple_delete;
	/*
	 * The entry to be deleted is the only entry and it is not in the index
	 * root.  We have to rebalance the tree so that the entry to be deleted
	 * is no longer the only entry in its node.  There are several cases we
	 * need to consider:
	 *
	 * 1) If the parent entry is not the end entry of its index node then
	 *    simply insert our parent entry at the front of the leaf node on
	 *    our right, i.e. the child node of the entry following our parent
	 *    entry.  Then remove our parent entry from our parent node and
	 *    free the leaf node the entry to be deleted is in (thus deleting
	 *    the entry).
	 *
	 *    What the above describes is effectively merging the leaf node
	 *    containing the entry to be deleted with its right-hand sibling
	 *    leaf node and in the process pulling down our parent entry into
	 *    the merged node and placing it in front of its successor entry.
	 *
	 *    Our parent node is of course turned from an index node entry to a
	 *    leaf entry as it is pulled down.
	 *
	 *    Note how if our parent entry is the only entry other than the end
	 *    entry in its node this results in our parent node being empty,
	 *    i.e. containing only the end entry.  This is fine for NTFS
	 *    B+trees, i.e. index nodes may be empty.  It is only leaf nodes
	 *    that may not be empty.
	 *
	 * 2) As point 1) above but the parent entry of the entry to be removed
	 *    does not fit in the leaf node containing its successor thus that
	 *    node needs to be split and the split may need to propagate up the
	 *    tree which is of course the job of ntfs_index_entry_add().
	 *
	 * 3) If the parent entry is the end entry of its index node, i.e.
	 *    there is no right-hand sibling leaf node then simply insert the
	 *    entry on the left of our parent entry at the end of its own child
	 *    node, i.e. the leaf node on our left.  Then remove the entry on
	 *    the left of our parent entry that we inserted into its child node
	 *    from our parent node and change the VCN sub-node pointer of our
	 *    parent node to point to our left-hand sibling.  Finally free the
	 *    leaf node the entry to be deleted is in which is no longer
	 *    referenced from anywhere (thus deleting the entry).
	 *
	 *    What the above describes is effectively merging the leaf node
	 *    containing the entry to be deleted with its left-hand sibling
	 *    leaf node and in the process pulling down the entry on the
	 *    left-hand side of our parent into the merged node and placing it
	 *    after its predecessor entry.
	 *
	 *    The left-hand sibling of our parent node is of course turned from
	 *    and index node entry to a leaf entry as it is pulled down.
	 *
	 *    Note how if the left-hand sibling of our parent entry is the only
	 *    entry other than the end entry, which is also our parent entry,
	 *    in its node this results in our parent node being empty, i.e.
	 *    containing only the end entry.  This is fine for NTFS B+trees,
	 *    i.e. index nodes may be empty.  It is only leaf nodes that may
	 *    not be empty.
	 *
	 * 4) As point 3) above but the left-hand sibling of the parent entry
	 *    of the entry to be removed does not fit in the leaf node
	 *    containing its predecessor thus that node needs to be split and
	 *    the split may need to propagate up the tree which is of course
	 *    the job of ntfs_index_entry_add().
	 *
	 * 5) If the parent entry is the end entry of its index node, i.e.
	 *    there is no right-hand sibling leaf node, and there is no entry
	 *    on the left of our parent entry, i.e. there is no left-hand
	 *    sibling leaf node, i.e. the parent node is completely empty, we
	 *    cannot merge with a sibling as there is none thus we need to
	 *    record the current state and repeat a very similar procedure as
	 *    above points 1-4 except applied to the parent node which is not
	 *    a leaf node so we need to be careful with VCN sub-node pointers.
	 *    The aim of doing this is to merge the parent node with one of its
	 *    siblings so that it is no longer empty so that our leaf node
	 *    containing the entry to be deleted has at least one sibling so
	 *    that we can merge the leaf node with one of its siblings.  If the
	 *    parent node of the parent node has no siblings either we need to
	 *    push the recorded state down into the stack of recorded states,
	 *    record the new current state, and then go to the next parent and
	 *    try to merge that with its sibling and so on until we find a
	 *    parent with a sibling that can be merged.  There are two possible
	 *    outcomes (not counting errors):
	 *    a) The going up and merging will eventually succeed in which case
	 *       we go backwards through the stack of recorded states merging
	 *       nodes as we go along until we reach the last recorded state at
	 *       which point we have reduced the problem to one of the above
	 *       points 1-4 thus we repeat them to finish.
	 *    b) We reach the index root which by definition cannot have any
	 *       siblings.  Also because we reached the index root it means
	 *       that the index root must be empty and the end entry is
	 *       pointing to the VCN sub-node we came from and because the
	 *       entry to be deleted is the last entry in its leaf-node and all
	 *       parent nodes including the index root are empty the entry to
	 *       be deleted is the very last entry of the directory thus it can
	 *       simply be removed by truncating the index allocation attribute
	 *       to zero size and if the index bitmap is non-resident
	 *       truncating it to zero size, too, and if the index bitmap is
	 *       resident zeroing its contents instead of truncating it and
	 *       finally the index root is switched to be a small index, which
	 *       is reflected in its index flags and in the removal of the VCN
	 *       sub-node pointer in the end entry of the index root.
	 *   NOTE: Instead of recursing, we do it in one go by simply freeing
	 *   all the index node blocks in the tree path until we find a
	 *   non-empty node, i.e. a node that can be merged, and we then
	 *   perform the merge by pulling down the appropriate index entry but
	 *   instead of pulling it down one level we pull it down to before its
	 *   successor (if merging to the right) or to after its predecessor (if
	 *   merging on the left) which by definition will be in a leaf node.
	 *   That give the same results as the recursion but it is much less
	 *   work to do and it affects less index nodes.
	 *   NOTE2: After implementing NOTE above, we ditch the code handling
	 *   cases 1-4 above as they are just special cases of case 5 when
	 *   implemented like NOTE.
	 *
	 * That is quite enough description now, let the games begin.  First,
	 * investigate the parent index entry of the leaf entry to be deleted
	 * to see whether it is the end entry or not to determine which of the
	 * above categories the deletion falls into.
	 *
	 * We must be at the bottom of the current tree path or things will get
	 * very confused.
	 */
	if (!ictx->down->is_root)
		panic("%s(): !ictx->down->is_root\n", __FUNCTION__);
	/*
	 * We are going to disconnect @ictx thus the tree starting at the index
	 * root is no longer referenced by the caller so we need to free it
	 * later.
	 */
	put_ictx = ictx->down;
	/*
	 * Obtain the parent node, unlock the current node, disconnect it from
	 * the tree, and mark it for deallocation later.
	 */
	parent_ictx = ictx->up;
	ntfs_index_ctx_unlock(ictx);
	ntfs_index_ctx_disconnect_reinit(ictx);
	deallocate_ictx = ictx;
	/*
	 * Investigate the parent node.  If it is not empty we can immediately
	 * proceed with the merge to the right or left.  If it is empty then we
	 * need to ascend the tree until we find a non-empty node or until we
	 * reach an empty index root in which case the index is becoming empty
	 * so we delete its contents by resetting it to zero size.
	 */
	while (parent_ictx->nr_entries <= 1) {
		if (parent_ictx->nr_entries != 1)
			panic("%s(): parent_ictx->nr_entries != 1\n",
					__FUNCTION__);
		if (parent_ictx->is_root) {
			/*
			 * The index is becoming empty.  To simplify things
			 * merge the two sub-trees back together and then empty
			 * the index.
			 */
			deallocate_ictx->up->down = parent_ictx->down;
			parent_ictx->down->up = deallocate_ictx->up;
			deallocate_ictx->up = parent_ictx;
			parent_ictx->down = deallocate_ictx;
			return ntfs_index_make_empty(deallocate_ictx);
		}
		/*
		 * Empty index node.  Move it to the list of index nodes to be
		 * deallocated and proceed to its parent.
		 */
		ictx = parent_ictx;
		parent_ictx = parent_ictx->up;
		ntfs_index_ctx_move(ictx, deallocate_ictx);
	}
	/*
	 * We have a non-empty parent node which we need to lock as we need to
	 * access its contents.
	 */
	if (parent_ictx->is_locked)
		panic("%s(): parent_ictx->is_locked\n", __FUNCTION__);
	err = ntfs_index_ctx_relock(parent_ictx);
	if (err) {
		ntfs_error(idx_ni->vol->mp, "Failed to relock the parent "
				"index node (error %d).", err);
		goto put_err;
	}
	/* INDEX_NODE == LARGE_INDEX */
	if (!(parent_ictx->follow_entry->flags & INDEX_ENTRY_NODE) ||
			!(parent_ictx->index->flags & INDEX_NODE))
		panic("%s(): !(parent_ictx->follow_entry->flags & "
				"INDEX_ENTRY_NODE) || "
				"!(parent_ictx->index->flags & INDEX_NODE)\n",
				__FUNCTION__);
	/*
	 * If the parent entry is the end entry in its node we cannot merge to
	 * the right so merge to the left instead.
	 */
	if (parent_ictx->follow_entry->flags & INDEX_ENTRY_END)
		goto merge_left;
	/*
	 * The parent entry is not the end entry in its node so merge on the
	 * right.  To do this we need to find the successor (leaf) entry for
	 * which we need to repoint the path so we descend into the sub-node of
	 * the entry on the right-hand side of the parent entry.
	 *
	 * We also make a note of the parent entry as we are going to pull it
	 * down in front of its successor entry and we then have to delete it
	 * from this node later.
	 */
	pull_down_entry = parent_ictx->follow_entry;
	pull_down_entry_nr = parent_ictx->entry_nr;
	pull_down_ie_size = le16_to_cpu(pull_down_entry->length);
	parent_ictx->follow_entry = (INDEX_ENTRY*)((u8*)pull_down_entry +
			pull_down_ie_size);
	pull_down_ie_size -= sizeof(VCN);
	parent_ictx->entry_nr++;
	suc_ictx = parent_ictx;
	do {
		err = ntfs_index_descend_into_child_node(&suc_ictx);
		if (err) {
			ntfs_error(idx_ni->vol->mp, "Failed to descend to "
					"node containing successor entry "
					"(error %d).", err);
			goto put_err;
		}
		if (!suc_ictx->is_locked)
			panic("%s(): !suc_ictx->is_locked\n", __FUNCTION__);
		suc_ictx->follow_entry = suc_ictx->entries[0];
	} while (suc_ictx->index->flags & INDEX_NODE);
	if (suc_ictx->follow_entry->flags & INDEX_ENTRY_NODE)
		panic("%s(): suc_ictx->follow_entry->flags & "
				"INDEX_ENTRY_NODE\n", __FUNCTION__);
	/*
	 * Can the parent entry simply be inserted in front of its successor
	 * leaf entry without overflowing the node?  If not we need to split
	 * the node and then restart the delete.
	 */
	if (pull_down_ie_size > suc_ictx->bytes_free) {
split_and_restart_case_4:
		ntfs_debug("Splitting index node before add on delete (case "
				"4).");
		ie_size = pull_down_ie_size;
		goto split_and_restart;
	}
	/*
	 * The parent entry can simply be inserted in front of its successor
	 * leaf entry.
	 *
	 * We need to have locked both nodes to be able to do the insert thus
	 * we may need to unlock the locked successor node.
	 */
	if (parent_ictx->is_locked)
		panic("%s(): parent_ictx->is_locked\n", __FUNCTION__);
	err = ntfs_index_ctx_lock_two(suc_ictx, parent_ictx);
	if (err)
		goto put_err;
	/*
	 * We need to re-check the amount of free space as it can have changed
	 * whilst we dropped the lock on @suc_ictx if that was necessary.  When
	 * this happens simply drop the lock on @parent_ictx if we took it and
	 * go back to the previous code dealing with the not enough space case.
	 */
	if (pull_down_ie_size > suc_ictx->bytes_free) {
		if (parent_ictx->is_locked)
			ntfs_index_ctx_unlock(parent_ictx);
		goto split_and_restart_case_4;
	}
	/*
	 * Both nodes are locked and this is a simple insert, so go ahead and
	 * do it.  We have already checked that there is enough space so it
	 * cannot fail.
	 *
	 * Move all the entries out of the way to make space for the parent
	 * entry to be copied in.
	 */
	err = ntfs_index_node_make_space(suc_ictx, pull_down_ie_size);
	if (err)
		panic("%s(): err\n", __FUNCTION__);
	/*
	 * Copy the parent index entry into the created space in front of its
	 * successor and switch the inserted entry to being a leaf entry.
	 */
	ie = suc_ictx->follow_entry;
	pull_down_entry = parent_ictx->entries[pull_down_entry_nr];
	memcpy(ie, pull_down_entry, pull_down_ie_size);
	ie->length = cpu_to_le16(pull_down_ie_size);
	ie->flags &= ~INDEX_ENTRY_NODE;
	/* Ensure the updates are written to disk. */
	ntfs_index_entry_mark_dirty(suc_ictx);
	/*
	 * We are done with the successor node so release it or transfer it to
	 * the parent node if they share the same page (in which case
	 * @parent_ictx is not marked locked).
	 */
	if (!suc_ictx->is_locked)
		panic("%s(): !suc_ictx->is_locked\n", __FUNCTION__);
	if (parent_ictx->is_locked) {
		/*
		 * @parent_ictx is locked thus it cannot be sharing the lock
		 * with @suc_ictx.  Unlock @suc_ictx.
		 */
		ntfs_index_ctx_unlock(suc_ictx);
	} else {
		/*
		 * @parent_ictx is not locked thus it must be sharing the lock
		 * with @suc_ictx.  Transfer the lock across.
		 */
		if (parent_ictx->is_root)
			panic("%s(): parent_ictx->is_root\n", __FUNCTION__);
		if (parent_ictx->upl_ofs != suc_ictx->upl_ofs)
			panic("%s(): parent_ictx->upl_ofs != "
					"suc_ictx->upl_ofs\n", __FUNCTION__);
		parent_ictx->is_locked = 1;
		suc_ictx->is_locked = 0;
	}
	if (!parent_ictx->is_locked)
		panic("%s(): parent_ictx->is_locked\n", __FUNCTION__);
	/*
	 * We have now inserted the parent entry in front of its successor but
	 * at the moment we have two copies of the parent entry, so delete the
	 * original copy from the parent node.  We need to switch the parent
	 * index context to be the current context and to point to the parent
	 * entry we pulled down so we can just pretend it is the entry to be
	 * deleted.  Note this is a simple delete just like for a leaf entry as
	 * we have taken care of its child leaf node(s) already and will
	 * deallocate it(them) later.
	 */
	ictx = parent_ictx;
	parent_ictx->entry = pull_down_entry;
	parent_ictx->entry_nr = pull_down_entry_nr;
	goto prep_simple_delete;
merge_left:
	/*
	 * The parent entry is the end entry in its node so merge on the left.
	 * To do this we need to find the predecessor (leaf) entry for which we
	 * need to repoint the path so we descend into the sub-node of the
	 * entry on the left-hand side of the parent entry.  Once found we need
	 * to pull down the new parent entry, i.e. the entry on the left of the
	 * current parent entry, behind its predecessor entry.
	 *
	 * We also make a note of the original parent entry as we have to
	 * change its VCN sub-node pointer later.
	 */
	old_parent_entry_nr = parent_ictx->entry_nr;
	old_parent_ie_size = le16_to_cpu(parent_ictx->follow_entry->length) -
			sizeof(VCN);
	parent_ictx->entry_nr--;
	parent_ictx->follow_entry =
			parent_ictx->entries[parent_ictx->entry_nr];
	pull_down_ie_size = le16_to_cpu(parent_ictx->follow_entry->length) -
			sizeof(VCN);
	suc_ictx = parent_ictx;
	do {
		err = ntfs_index_descend_into_child_node(&suc_ictx);
		if (err) {
			ntfs_error(idx_ni->vol->mp, "Failed to descend to "
					"node containing predecessor entry of "
					"left-hand sibling entry (error %d).",
					err);
			goto put_err;
		}
		if (!suc_ictx->is_locked)
			panic("%s(): !suc_ictx->is_locked\n", __FUNCTION__);
		suc_ictx->entry_nr = suc_ictx->nr_entries - 1;
		suc_ictx->follow_entry = suc_ictx->entries[suc_ictx->entry_nr];
	} while (suc_ictx->index->flags & INDEX_NODE);
	if (suc_ictx->follow_entry->flags & INDEX_ENTRY_NODE)
		panic("%s(): suc_ictx->follow_entry->flags & "
				"INDEX_ENTRY_NODE\n", __FUNCTION__);
	/*
	 * Can the entry on the left-hand side of the parent entry simply be
	 * inserted in after its predecessor leaf entry, i.e. before the end
	 * entry in the leaf node containing the predecessor leaf entry,
	 * without overflowing the node?  If not we need to split the node and
	 * then restart the delete.
	 */
	if (pull_down_ie_size > suc_ictx->bytes_free) {
split_and_restart_case_5:
		ntfs_debug("Splitting index node before add on delete (case "
				"5).");
		ie_size = pull_down_ie_size;
		goto split_and_restart;
	}
	/*
	 * The entry on the left-hand side of the parent entry can simply be
	 * inserted in front of the end entry of the leaf node containing its
	 * predecessor.
	 *
	 * We need to have locked both nodes to be able to do the insert thus
	 * we may need to unlock the locked predecessor node to ensure that
	 * page locks are only taken in ascending page index order so we avoid
	 * deadlocks.
	 */
	if (parent_ictx->is_locked)
		panic("%s(): parent_ictx->is_locked\n", __FUNCTION__);
	err = ntfs_index_ctx_lock_two(suc_ictx, parent_ictx);
	if (err)
		goto put_err;
	/*
	 * We need to re-check the amount of free space as it can have changed
	 * whilst we dropped the lock on @suc_ictx if that was necessary.  When
	 * this happens simply drop the lock on @parent_ictx if we took it and
	 * go back to the previous code dealing with the not enough space case.
	 */
	if (pull_down_ie_size > suc_ictx->bytes_free) {
		if (parent_ictx->is_locked)
			ntfs_index_ctx_unlock(parent_ictx);
		goto split_and_restart_case_5;
	}
	/*
	 * Both nodes are locked and this is a simple insert, so go ahead and
	 * do it.  We have already checked that there is enough space so it
	 * cannot fail.
	 *
	 * Move the end entry out of the way to make space for the entry to be
	 * copied in.
	 */
	err = ntfs_index_node_make_space(suc_ictx, pull_down_ie_size);
	if (err)
		panic("%s(): err\n", __FUNCTION__);
	/*
	 * Copy the parent index entry into the created space in front of its
	 * predecessor and switch the inserted entry to being a leaf entry.
	 */
	ie = suc_ictx->follow_entry;
	memcpy(ie, parent_ictx->follow_entry, pull_down_ie_size);
	ie->length = cpu_to_le16(pull_down_ie_size);
	ie->flags &= ~INDEX_ENTRY_NODE;
	/* Ensure the updates are written to disk. */
	ntfs_index_entry_mark_dirty(suc_ictx);
	/*
	 * We are done with the predecessor node so release it or transfer it
	 * to the parent node if they share the same page (in which case
	 * @parent_ictx is not marked locked).
	 */
	if (!suc_ictx->is_locked)
		panic("%s(): !suc_ictx->is_locked\n", __FUNCTION__);
	if (parent_ictx->is_locked) {
		/*
		 * @parent_ictx is locked thus it cannot be sharing the lock
		 * with @suc_ictx.  Unlock @suc_ictx.
		 */
		ntfs_index_ctx_unlock(suc_ictx);
	} else {
		/*
		 * @parent_ictx is not locked thus it must be sharing the lock
		 * with @suc_ictx.  Transfer the lock across.
		 */
		if (parent_ictx->is_root)
			panic("%s(): parent_ictx->is_root\n", __FUNCTION__);
		if (parent_ictx->upl_ofs != suc_ictx->upl_ofs)
			panic("%s(): parent_ictx->upl_ofs != "
					"suc_ictx->upl_ofs\n", __FUNCTION__);
		parent_ictx->is_locked = 1;
		suc_ictx->is_locked = 0;
	}
	if (!parent_ictx->is_locked)
		panic("%s(): parent_ictx->is_locked\n", __FUNCTION__);
	/*
	 * We have now inserted the left-hand sibling entry of the parent entry
	 * in front of the end entry of the leaf node containing its
	 * predecessor entry but at the moment we have two copies of the entry,
	 * so delete the original copy from its index node.
	 *
	 * Before we do that, update the VCN sub-node pointer of the original
	 * parent entry to point to the index node that is about to become
	 * parentless.  Note we do not mark the index node dirty as that will
	 * happen when the original copy of the entry we pulled down is
	 * deleted.
	 */
	*(leVCN*)((u8*)parent_ictx->entries[old_parent_entry_nr] +
			old_parent_ie_size) = *(leVCN*)(
			(u8*)parent_ictx->follow_entry + pull_down_ie_size);
	/*
	 * All that is left now is to delete the original copy of the entry we
	 * pulled down.  We need to switch the parent index context to be the
	 * current context as it already points to the entry we pulled down
	 * thus we can just pretend it is the entry to be deleted.  Note this
	 * is a simple delete just like for a leaf entry as we have taken care
	 * of transfering its child leaf node(s) to the original parent entry
	 * already and will deallocate the origial parent entry's child leaf
	 * node(s) later.
	 */
	ictx = parent_ictx;
prep_simple_delete:
	ie = ictx->entry;
	is_root = ictx->is_root;
	ictx->is_match = 1;
	ih = ictx->index;
	ie_size = le16_to_cpu(ie->length);
	next_ie = (INDEX_ENTRY*)((u8*)ie + ie_size);
	/* goto simple_delete; */
simple_delete:
	new_ilen = le32_to_cpu(ih->index_length) - ie_size;
	if (is_root) {
		ATTR_RECORD *a;
		u32 muse;

		m = ictx->actx->m;
		muse = le32_to_cpu(m->bytes_in_use);
		/*
		 * Move the index entries following @ie into @ie's position.
		 * As an optimization we combine the moving of the index
		 * entries and the resizing of the index root attribute into a
		 * single operation.
		 */
		memmove(ie, next_ie, muse - ((u8*)next_ie - (u8*)m));
		/* Adjust the mft record to reflect the change in used space. */
		m->bytes_in_use = cpu_to_le32(muse - ie_size);
		/*
		 * Adjust the attribute record to reflect the change in
		 * attribute value and attribute record size.
		 */
		a = ictx->actx->a;
		a->length = cpu_to_le32(le32_to_cpu(a->length) - ie_size);
		a->value_length = cpu_to_le32(le32_to_cpu(a->value_length) -
				ie_size);
		/* Adjust the index header to reflect the change in length. */
		ih->allocated_size = ih->index_length = cpu_to_le32(new_ilen);
	} else {
		/* Move index entries following @ie into @ie's position. */
		memmove(ie, next_ie, new_ilen - ((u8*)ie - (u8*)ih));
		/* Adjust the index header to reflect the change in length. */
		ih->index_length = cpu_to_le32(new_ilen);
	}
	/* Ensure the updates are written to disk. */
	ntfs_index_entry_mark_dirty(ictx);
	/*
	 * If we have scheduled any index block nodes to be deallocated do it
	 * now but first unlock the node we just deleted an entry from so we do
	 * not deadlock.
	 */
	if (deallocate_ictx) {
		ntfs_index_ctx_unlock(ictx);
		ictx = deallocate_ictx;
		do {
			if (ictx->is_root)
				panic("%s(): ictx->is_root\n", __FUNCTION__);
			err = ntfs_index_block_free(ictx);
			if (err) {
				ntfs_error(idx_ni->vol->mp, "Failed to "
						"deallocate no longer used "
						"index block VCN 0x%llx "
						"(error %d).  Run chkdsk to "
						"recover the lost index "
						"block.",
						(unsigned long long)ictx->vcn,
						err);
				NVolSetErrors(idx_ni->vol);
			}
		} while ((ictx = ictx->up) != deallocate_ictx);
	}
	/*
	 * Release any sub-tree paths that the caller no longer has a reference
	 * to.
	 */
	if (put_ictx)
		ntfs_index_ctx_put(put_ictx);
	ntfs_debug("Done.");
	return 0;
split_and_restart:
	/*
	 * Split the index node described by @suc_ictx taking into account the
	 * fact that it is very likely that an entry of @ie_size will be added
	 * to the index in front of the position pointed to by @suc_ictx.
	 */
	err = ntfs_index_node_split(suc_ictx, ie_size);
	if (!err) {
		/*
		 * Signal to the caller that they need to restart the delete
		 * from scratch.
		 */
		err = -EAGAIN;
	}
put_err:
	if (put_ictx)
		ntfs_index_ctx_put(put_ictx);
	return err;
}
