/*
 * ntfs_attr.h - Defines for attribute handling in the NTFS kernel driver.
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

#ifndef _OSX_NTFS_ATTR_H
#define _OSX_NTFS_ATTR_H

#include <sys/errno.h>

/* Forward declaration. */
typedef struct _ntfs_attr_search_ctx ntfs_attr_search_ctx;

#include "ntfs_endian.h"
#include "ntfs_index.h"
#include "ntfs_inode.h"
#include "ntfs_layout.h"
#include "ntfs_types.h"

/*
 * The little endian Unicode empty string as a global constant.  This is used
 * when looking up attributes to specify that we want the unnamed attribute as
 * opposed to any attribute or a specific named attribute.
 */
__private_extern__ ntfschar AT_UNNAMED[1];

__private_extern__ errno_t ntfs_attr_map_runlist(ntfs_inode *ni);

__private_extern__ errno_t ntfs_map_runlist_nolock(ntfs_inode *ni, VCN vcn,
		ntfs_attr_search_ctx *ctx);

__private_extern__ LCN ntfs_attr_vcn_to_lcn_nolock(ntfs_inode *ni,
		const VCN vcn, const BOOL write_locked, s64 *clusters);

__private_extern__ errno_t ntfs_attr_find_vcn_nolock(ntfs_inode *ni,
		const VCN vcn, ntfs_rl_element **run,
		ntfs_attr_search_ctx *ctx);

static inline s64 ntfs_attr_size(const ATTR_RECORD *a)
{
	if (!a->non_resident)
		return (s64)le32_to_cpu(a->value_length);
	return sle64_to_cpu(a->data_size);
}

/**
 * ntfs_attr_search_ctx - used in attribute search functions
 * @m:			buffer containing mft record to search
 * @a:			attribute record in @m where to begin/continue search
 * @is_first:		if 1 the search begins search with @a, else after
 * @is_iteration:	if 1 this is not a search but an iteration
 * @is_error:		if 1 this search context is invalid and must be released
 * @is_mft_locked:	if 1 the mft is locked (@mft_ni->lock)
 *
 * Structure must be initialized to zero before the first call to one of the
 * attribute search functions.  Initialize @m to point to the mft record to
 * search, and @a to point to the first attribute within @m and set @is_first
 * to 1.
 *
 * If @is_first is 1, the search begins with @a.  If @is_first is 0, the search
 * begins after @a.  This is so that, after the first call to one of the search
 * attribute functions, we can call the function again, without any
 * modification of the search context, to automagically get the next matching
 * attribute.
 *
 * If @is_iteration is 1, all attributes are returned one after the other with
 * each call to ntfs_attr_find_in_mft_record().  Note this only works with
 * ntfs_attr_find_in_mft_record() and not with ntfs_attr_lookup() or
 * ntfs_attr_find_in_attribute_list().
 *
 * If @is_error is 1 this attribute search context has become invalid and must
 * either be reinitialized via ntfs_attr_search_ctx_reinit() or released via
 * ntfs_attr_search_ctx_put().  Functions to which you pass an attribute search
 * context may require you to check @is_error after calling the function.  If
 * this is the case the function will explicictly say so in the function
 * description.
 *
 * If @is_error is 1 you can see what the error code was that caused the
 * context to become invalid by looking at the @error member of the search
 * context.
 *
 * If @is_mft_locked is true the owner of the search context holds the mft lock
 * (@mft_ni->lock) thus ntfs_attr_lookup() will make sure to pass this fact
 * onto ntfs_extent_mft_record_map_ext() so that it will not try to take the
 * same lock.  It is then the responsibility of the caller that the mft is
 * consistent and stable for the duration of the life of the search context.
 */
struct _ntfs_attr_search_ctx {
	union {
		MFT_RECORD *m;
		errno_t error;
	};
	ATTR_RECORD *a;
	struct {
		unsigned is_first:1;	/* If 1 this is the first search. */
		unsigned is_iteration:1;/* If 1 this is an iteration of all
					   attributes in the mft record. */
		unsigned is_error:1;
		unsigned is_mft_locked:1;
	};
	ntfs_inode *ni;
	ATTR_LIST_ENTRY *al_entry;
	ntfs_inode *base_ni;
	MFT_RECORD *base_m;
	ATTR_RECORD *base_a;
};

/**
 * ntfs_attr_search_ctx_init - initialize an attribute search context
 * @ctx:	attribute search context to initialize
 * @ni:		ntfs inode with which to initialize the search context
 * @m:		mft record with which to initialize the search context
 *
 * Initialize the attribute search context @ctx with @ni and @m.
 */
static inline void ntfs_attr_search_ctx_init(ntfs_attr_search_ctx *ctx,
		ntfs_inode *ni, MFT_RECORD *m)
{
	/*
	 * Gcc is broken so it fails to see both the members of the anonymous
	 * union and the bitfield inside the C99 initializer.  Work around this
	 * by setting @m and @is_first afterwards.
	 */
	*ctx = (ntfs_attr_search_ctx) {
		/* Sanity checks are performed elsewhere. */
		.a = (ATTR_RECORD*)((u8*)m + le16_to_cpu(m->attrs_offset)),
		.ni = ni,
	};
	ctx->m = m,
	ctx->is_first = 1;
}

__private_extern__ void ntfs_attr_search_ctx_reinit(ntfs_attr_search_ctx *ctx);
__private_extern__ ntfs_attr_search_ctx *ntfs_attr_search_ctx_get(
		ntfs_inode *ni, MFT_RECORD *m);
__private_extern__ void ntfs_attr_search_ctx_put(ntfs_attr_search_ctx *ctx);

__private_extern__ errno_t ntfs_attr_find_in_mft_record(const ATTR_TYPE type,
		const ntfschar *name, const u32 name_len,
		const void *val, const u32 val_len, ntfs_attr_search_ctx *ctx);

__private_extern__ errno_t ntfs_attr_lookup(const ATTR_TYPE type,
		const ntfschar *name, const u32 name_len, const VCN lowest_vcn,
		const void *val, const u32 val_len, ntfs_attr_search_ctx *ctx);

__private_extern__ errno_t ntfs_attr_size_bounds_check(const ntfs_volume *vol,
		const ATTR_TYPE type, const s64 size);

__private_extern__ errno_t ntfs_attr_can_be_resident(const ntfs_volume *vol,
		const ATTR_TYPE type);

__private_extern__ BOOL ntfs_attr_record_is_only_one(MFT_RECORD *m,
		ATTR_RECORD *a);

__private_extern__ void ntfs_attr_record_delete_internal(MFT_RECORD *m,
		ATTR_RECORD *a);
__private_extern__ errno_t ntfs_attr_record_delete(ntfs_inode *base_ni,
		ntfs_attr_search_ctx *ctx);

__private_extern__ errno_t ntfs_attr_record_make_space(MFT_RECORD *m,
		ATTR_RECORD *a, u32 size);

__private_extern__ errno_t ntfs_attr_record_resize(MFT_RECORD *m,
		ATTR_RECORD *a, u32 new_size);

__private_extern__ errno_t ntfs_attr_mapping_pairs_update(ntfs_inode *base_ni,
		ntfs_inode *ni, VCN first_vcn, VCN last_vcn,
		ntfs_attr_search_ctx *ctx);

__private_extern__ errno_t ntfs_resident_attr_record_insert_internal(
		MFT_RECORD *m, ATTR_RECORD *a, const ATTR_TYPE type,
		const ntfschar *name, const u8 name_len, const u32 val_len);
__private_extern__ errno_t ntfs_resident_attr_record_insert(ntfs_inode *ni,
		ntfs_attr_search_ctx *ctx, const ATTR_TYPE type,
		const ntfschar *name, const u8 name_len,
		const void *val, const u32 val_len);

__private_extern__ errno_t ntfs_resident_attr_value_resize(MFT_RECORD *m,
		ATTR_RECORD *a, const u32 new_size);

__private_extern__ errno_t ntfs_attr_make_non_resident(ntfs_inode *ni);

__private_extern__ errno_t ntfs_attr_record_move_for_attr_list_attribute(
		ntfs_attr_search_ctx *al_ctx, ATTR_LIST_ENTRY *al_entry,
		ntfs_attr_search_ctx *ctx, BOOL *remap_needed);

__private_extern__ errno_t ntfs_attr_record_move(ntfs_attr_search_ctx *ctx);

__private_extern__ errno_t ntfs_attr_set_initialized_size(ntfs_inode *ni,
		s64 new_init_size);

__private_extern__ errno_t ntfs_attr_extend_initialized(ntfs_inode *ni,
		const s64 new_init_size);

__private_extern__ errno_t ntfs_attr_instantiate_holes(ntfs_inode *ni,
		s64 start, s64 end, s64 *new_end, BOOL atomic);

__private_extern__ errno_t ntfs_attr_extend_allocation(ntfs_inode *ni,
		s64 new_alloc_size, const s64 new_data_size,
		const s64 data_start, ntfs_index_context *ictx,
		s64 *dst_alloc_size, const BOOL atomic);

__private_extern__ errno_t ntfs_attr_resize(ntfs_inode *ni, s64 new_size,
		int ioflags, ntfs_index_context *ictx);

__private_extern__ errno_t ntfs_attr_set(ntfs_inode *ni, s64 ofs,
		const s64 cnt, const u8 val);

__private_extern__ errno_t ntfs_resident_attr_read(ntfs_inode *ni,
		const s64 ofs, const u32 cnt, u8 *buf);
__private_extern__ errno_t ntfs_resident_attr_write(ntfs_inode *ni, u8 *buf,
		u32 cnt, const s64 ofs);

#endif /* !_OSX_NTFS_ATTR_H */
