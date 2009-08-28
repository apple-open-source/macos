/*
 * ntfs_index.h - Defines for index handling in the NTFS kernel driver.
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

#ifndef _OSX_NTFS_INDEX_H
#define _OSX_NTFS_INDEX_H

#include <sys/errno.h>

#include <libkern/OSMalloc.h>

#include <kern/debug.h>

/* Foward declaration. */
typedef struct _ntfs_index_context ntfs_index_context;

#include "ntfs_attr.h"
#include "ntfs_inode.h"
#include "ntfs_layout.h"
#include "ntfs_types.h"

/**
 * @up:		pointer to index context located directly above in the tree
 * @down:	pointer to index context located directly below in the tree
 * @idx_ni:	inode containing the @entry described by this context
 * @base_ni:	base inode
 * @entry:	if @is_locked this is the index entry (points into @ir or @ia)
 * @is_root:	1 if @entry is in @ir and 0 if it is in @ia
 * @ir:		index root if @is_root and @is_locked and NULL otherwise
 * @actx:	attribute search context if @is_root and @is_locked or NULL
 * @ia:		index block if @is_root is 0 and @is_locked or NULL
 * @upl_ofs:	byte offset into the index allocation at which @upl begins
 * @upl:	page list if @is_root is 0 and @is_locked or NULL
 * @pl:		array of pages containing the page itself if @is_root is 0
 * @addr:	mapped address of the page data if @is_root is 0 and @is_locked
 *
 * @up and @down are pointers used to chain the various index contexts together
 * into a path through the B+tree index.  This path is used to describe the
 * index nodes traversed during a lookup.
 *
 * @up points to the index context that is immediately above in the tree or if
 * this is the top of the tree then @up is the bottom of the tree.
 *
 * @down points to the index context that is immediately below in the tree or
 * if this is the bottom of the tree then @down is the top of the tree.
 *
 * If this index context is the only node then @up == @down == this index
 * context.
 *
 * @idx_ni is the index inode this context belongs to and @base_ni is the base
 * inode to which @idx_ni belongs.
 *
 * @entry is the index entry described by this context.  It is only valid when
 * @is_locked is true.
 *
 * If @is_rootis 1, @entry is in the index root attribute @ir described by the
 * attribute search context @actx and the base inode @base_ni.  @ia, @upl, @pl,
 * and @addr do not exist in this case as they are in a union with @ir and
 * @actx.
 *
 * If @is_root is 0, @entry is in the index allocation attribute and @ia,
 * @upl_ofs, @upl, @pl, and @addr point to the index allocation block and the
 * mapped, locked page it is in, respectively.  @ir and @actx do not exist in
 * this case.  Note @upl_ofs is the byte offset inside the index allocation
 * attribute at which the page list @upl begins.  This is used when an index
 * context is relocked/remapped to determine if it now is at a different
 * virtual memory address and thus all pointers in the index context need to be
 * updated for the new page address @addr.
 *
 * To obtain a context call ntfs_index_ctx_get().
 *
 * We use this context to allow ntfs_index_lookup() to return the found index
 * @entry without having to allocate a buffer and copy the @entry and/or its
 * data into it.
 *
 * When finished with the @entry and its data, call ntfs_index_ctx_put() to
 * free the context and other associated resources.
 *
 * If the index entry was modified, call ntfs_index_entry_mark_dirty() before
 * the call to ntfs_index_ctx_put() to ensure that the changes are written to
 * disk.
 */
struct _ntfs_index_context {
	ntfs_index_context *up;		/* Pointer to the index context that is
					   immediately above in the tree. */
	ntfs_index_context *down;	/* Pointer to the index context that is
					   immediately below in the tree. */
	ntfs_inode *idx_ni;		/* Index inode. */
	ntfs_inode *base_ni;		/* Base inode of the index inode
					   @idx_ni. */
	union {
		/* Use @ie if @is_match is 1 and @follow_ie if it is 0. */
		INDEX_ENTRY *entry;		/* Index entry matched by
						   lookup. */
		INDEX_ENTRY *follow_entry;	/* Index entry whose sub-node
						   needs to be descended into
						   if present or index entry in
						   front of which to insert the
						   new entry. */
	};
	unsigned entry_nr;		/* Index of the @entry in the @entries
					   array. */
	struct {
		unsigned is_dirty:1;	/* If 1 the page has been modified. */
		unsigned is_root:1;	/* If 1 the node is the index root. */
		unsigned is_locked:1;	/* If 1 the node is locked. */
		unsigned is_match:1;	/* If 1 @ie matches the looked up
					   entry. */
		unsigned promote_inserted:1; /* If 0 insert the @insert_entry
					   index entry in front of @entry. */
		unsigned split_node:1;	/* If 1 the node needs to be split. */
		unsigned right_is_locked:1; /* If 1 the right node, i.e.
					   @right_page is locked. */
		unsigned insert_to_add:1; /* If 1 the entry to be inserted is
					   the entry to be added, i.e. the
					   entry with which the function was
					   invoked. */
		unsigned bmp_is_locked:1; /* If 1 the index bitmap inode is
					   locked for writing.  This is set in
					   ntfs_index_block_alloc() to cope
					   with the need for recursion into
					   itself. */
	};
	union {
		/* Use @ir if @is_root is 1 and @ia if it is 0. */
		INDEX_ROOT *ir;		/* The index root attribute value. */
		struct {
			INDEX_ALLOCATION *ia; /* The index allocation block. */
			VCN vcn;	/* The vcn of this index block. */
		};
	};
	INDEX_HEADER *index;	/* The index header of the node. */
	union {
		/*
		 * If @is_root is 1 then use @actx if @is_locked is also 1.
		 * If @is_locked is 0, then @actx is NULL.  In the unlocked
		 * case we have to revalidate the pointers after mapping the
		 * mft record and looking up the index root attribute as it is
		 * possible that some attribute resizing operation caused the
		 * index root to be moved to a different mft record or within
		 * the same mft record and it is also possible that the VM
		 * paged out the page and then loaded it into a different place
		 * in memory.
		 *
		 * If @is_root is 0 then use @upl_ofs, @upl, @pl, and @addr.
		 */
		ntfs_attr_search_ctx *actx;
		struct {
			s64 upl_ofs;
			upl_t upl;
			upl_page_info_array_t pl;
			u8 *addr;
		};
	};
	unsigned bytes_free;	/* Number of bytes free in this node. */
	INDEX_ENTRY **entries;	/* Pointers to the index entries in the node. */
	unsigned nr_entries;	/* Current number of entries in @entries. */
	unsigned max_entries;	/* Maximum number of entries in @entries. */
	/*
	 * These fields are used when splitting nodes when inserting new index
	 * entries.
	 */
	/*
	 * If @promote_inserted is 0 then @insert_entry_nr, @insert_entry_size,
	 * and @insert_ictx describe the index entry of a child index node to
	 * be inserted into this index block in front of the index entry
	 * @entry.
	 *
	 * If @insert_to_add is 1 then the entry that is to be inserted into
	 * this node is the entry being added to the index with the current
	 * call to ntfs_index_entry_add(), i.e. it does not come from a child
	 * node.  In that case @insert_ictx and @insert_entry_nr are not valid.
	 *
	 * If @promote_inserted is 1 then nothing needs to be inserted into
	 * this node, i.e. the node just needs to be split.  This happens when
	 * the index entry to be inserted is the median entry and it is being
	 * promoted to our parent node.
	 */
	ntfs_index_context *insert_ictx;
	unsigned insert_entry_nr;
	u32 insert_entry_size;
	/*
	 * If @split_node is 1 then @promote_entry_nr and @promote_inserted
	 * describe the index entry being promoted and thus describe the
	 * position in the index at which the node is to be split.
	 *
	 * If @cur_ictx->promote_inserted is 1 we have to keep the entry
	 * @cur_ictx->promote_entry_nr and move it together with all following
	 * entries to the right-hand node.  And if @cur_ictx->promote_inserted
	 * is 0 we have to remove the entry @cur_ictx->promote_entry_nr and
	 * move all following entries to the right-hand node.
	 * 
	 * @right_vcn, @right_ia, @right_upl_ofs, @right_upl, @right_pl, and
	 * @right_addr in turn describe the allocated index block to be used as
	 * the destination for the index entries that are split away from the
	 * right-hand side of the median of the index block as described above.
	 */
	unsigned promote_entry_nr;
	VCN right_vcn;
	INDEX_ALLOCATION *right_ia;
	s64 right_upl_ofs;
	upl_t right_upl;
	upl_page_info_array_t right_pl;
	u8 *right_addr;
};

/**
 * ntfs_index_ctx_alloc - allocate an index context
 *
 * Allocate an index context and return it.
 */
static inline ntfs_index_context *ntfs_index_ctx_alloc(void)
{
	return OSMalloc(sizeof(ntfs_index_context), ntfs_malloc_tag);
}

/**
 * ntfs_index_ctx_init - initialize an index context
 * @ictx:	index context to initialize
 * @idx_ni:	ntfs index inode with which to initialize the context
 *
 * Initialize the index context @ictx with @idx_ni and its base inode and set
 * its @up and @down pointers to point to itself.
 *
 * Locking: Caller must hold @idx_ni->lock on the index inode.
 */
static inline void ntfs_index_ctx_init(ntfs_index_context *ictx,
		ntfs_inode *idx_ni)
{
	*ictx = (ntfs_index_context) {
		.up = ictx,
		.down = ictx,
		.idx_ni = idx_ni,
		.base_ni = NInoAttr(idx_ni) ? idx_ni->base_ni : idx_ni,
	};
}

__private_extern__ ntfs_index_context *ntfs_index_ctx_get(ntfs_inode *idx_ni);

__private_extern__ void ntfs_index_ctx_put_reuse_single(
		ntfs_index_context *ictx);

__private_extern__ void ntfs_index_ctx_put_reuse(ntfs_index_context *ictx);

/**
 * ntfs_index_ctx_reinit - re-initialize an index context
 * @ictx:	index context to re-initialize
 * @idx_ni:	ntfs index inode with which to initialize the context
 *
 * Re-initialize the index context @ictx with @idx_ni and its base inode.
 *
 * To do this the existing index context is first put for reuse and then
 * initialized from scratch with the index inode @idx_ni.
 *
 * Locking: Caller must hold @idx_ni->lock on the index inode.
 */
static inline void ntfs_index_ctx_reinit(ntfs_index_context *ictx,
		ntfs_inode *idx_ni)
{
	ntfs_index_ctx_put_reuse(ictx);
	ntfs_index_ctx_init(ictx, idx_ni);
}

/**
 * ntfs_index_ctx_disconnect - disconnect an index context from its tree path
 * @ictx:	index context to disconnect
 *
 * Disconnect the index context @ictx from its tree path.  This function leaves
 * @ictx in an invalid state.  We only use it when we are about to throw away
 * @ictx thus do not care what state it is left in.
 *
 * Locking: Caller must hold @ictx->idx_ni->lock on the index inode.
 */
static inline void ntfs_index_ctx_disconnect(ntfs_index_context *ictx)
{
	ictx->up->down = ictx->down;
	ictx->down->up = ictx->up;
}

/**
 * ntfs_index_ctx_free - free an index context
 * @ictx:	index context to free
 *
 * Free the index context @ictx.
 */
static inline void ntfs_index_ctx_free(ntfs_index_context *ictx)
{
	OSFree(ictx, sizeof(*ictx), ntfs_malloc_tag);
}

/**
 * ntfs_index_ctx_put_single - release a single index context
 * @ictx:	index context to free
 *
 * Release the index context @ictx, disconnecting it from its tree path and
 * releasing all associated resources.
 *
 * Locking: Caller must hold @ictx->idx_ni->lock on the index inode.
 */
static inline void ntfs_index_ctx_put_single(ntfs_index_context *ictx)
{
	/* Disconnect the current index context from the tree. */
	ntfs_index_ctx_disconnect(ictx);
	/* Release all resources held by the current index context. */
	ntfs_index_ctx_put_reuse_single(ictx);
	/* Deallocate the current index context. */
	ntfs_index_ctx_free(ictx);
}

/**
 * ntfs_index_ctx_put - release an index context
 * @ictx:	index context to free
 *
 * Release the index context @ictx, releasing all associated resources.
 *
 * Locking: Caller must hold @ictx->idx_ni->lock on the index inode.
 */
static inline void ntfs_index_ctx_put(ntfs_index_context *ictx)
{
	ntfs_index_ctx_put_reuse(ictx);
	ntfs_index_ctx_free(ictx);
}

__private_extern__ void ntfs_index_ctx_unlock(ntfs_index_context *ictx);

__private_extern__ errno_t ntfs_index_ctx_relock(ntfs_index_context *ictx);

__private_extern__ errno_t ntfs_index_lookup(const void *key,
		const int key_len, ntfs_index_context **ictx);

__private_extern__ errno_t ntfs_index_lookup_by_position(const s64 pos,
		const int key_len, ntfs_index_context **index_ctx);

__private_extern__ errno_t ntfs_index_lookup_next(
		ntfs_index_context **index_ctx);

__private_extern__ void ntfs_index_entry_mark_dirty(ntfs_index_context *ictx);

__private_extern__ errno_t ntfs_index_move_root_to_allocation_block(
		ntfs_index_context *ictx);

__private_extern__ int ntfs_index_entry_delete(ntfs_index_context *ictx);

__private_extern__ errno_t ntfs_index_entry_add_or_node_split(
		ntfs_index_context *ictx, const BOOL split_only,
		u32 entry_size, const void *key, const u32 key_len,
		const void *data, const u32 data_len);

/**
 * ntfs_index_entry_add - add a key to an index
 * @ictx:	index context specifying the position at which to add
 * @key:	key to add to the directory or view index
 * @key_len:	size of key @key in bytes
 * @data:	data to associate with the key @key if a view index
 * @data_len:	size of data @data in bytes if a view index
 *
 * If @ictx belongs to a directory index, insert the filename attribute @key of
 * length @key_len bytes in the directory index at the position specified by
 * the index context @ictx and point the inserted index entry at the mft
 * reference *@data which is the mft reference of the inode to which the
 * filename @fn belongs.  @data_len must be zero in this case.
 * 
 * If @ictx belongs to a view index, insert the key @key of length @key_len
 * bytes in the view index at the position specified by the index context @ictx
 * and associate the data @data of size @data_len bytes with the key @key.
 *
 * Return 0 on success and errno on error.  On error the index context is no
 * longer usable and must be released or reinitialized.
 *
 * Note that we do not update the array of index entry pointers nor the number
 * of entries in the array because on success it means that the index entry has
 * been added and the function is done, i.e. the array of index entry pointers
 * will not be used any more and on error the index context becomes invalid so
 * there is no need to update any of the pointers.
 *
 * Locking: - Caller must hold @ictx->idx_ni->lock on the index inode for
 *	      writing.
 *	    - The index context @ictx must be locked.
 */
static inline errno_t ntfs_index_entry_add(ntfs_index_context *ictx,
		const void *key, const u32 key_len,
		const void *data, const u32 data_len)
{
	return ntfs_index_entry_add_or_node_split(ictx, FALSE, 0, key, key_len,
			data, data_len);
}

#endif /* _OSX_NTFS_INDEX_H */
