/*
 * ntfs_attr.h - Defines for attribute handling in the NTFS kernel driver.
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

#ifndef _OSX_NTFS_ATTR_H
#define _OSX_NTFS_ATTR_H

#include <sys/errno.h>

#include "ntfs_endian.h"
#include "ntfs_inode.h"
#include "ntfs_layout.h"
#include "ntfs_types.h"

__private_extern__ errno_t ntfs_map_runlist_nolock(ntfs_inode *ni, VCN vcn);

__private_extern__ LCN ntfs_attr_vcn_to_lcn_nolock(ntfs_inode *ni,
		const VCN vcn, const BOOL write_locked, s64 *clusters);

static inline s64 ntfs_attr_size(const ATTR_RECORD *a)
{
	if (!a->non_resident)
		return (s64)le32_to_cpu(a->value_length);
	return sle64_to_cpu(a->data_size);
}

/**
 * ntfs_attr_search_ctx - used in attribute search functions
 * @m:		buffer containing mft record to search
 * @a:		attribute record in @mrec where to begin/continue search
 * @is_first:	if true ntfs_attr_lookup() begins search with @a, else after
 *
 * If @is_first is TRUE, the search begins with @a.  If @is_first is FALSE, the
 * search begins after @a.  This is so that, after the first call to one of the
 * search attribute functions, we can call the function again, without any
 * modification of the search context, to automagically get the next matching
 * attribute.
 */
typedef struct {
	MFT_RECORD *m;
	ATTR_RECORD *a;
	BOOL is_first;
	ntfs_inode *ni;
	ATTR_LIST_ENTRY *al_entry;
	ntfs_inode *base_ni;
	MFT_RECORD *base_m;
	ATTR_RECORD *base_a;
} ntfs_attr_search_ctx;

__private_extern__ void ntfs_attr_search_ctx_reinit(ntfs_attr_search_ctx *ctx);
__private_extern__ ntfs_attr_search_ctx *ntfs_attr_search_ctx_get(
		ntfs_inode *ni, MFT_RECORD *m);
__private_extern__ void ntfs_attr_search_ctx_put(ntfs_attr_search_ctx *ctx);

__private_extern__ errno_t ntfs_attr_lookup(const ATTR_TYPE type,
		const ntfschar *name, const u32 name_len,
		const IGNORE_CASE_BOOL ic, const VCN lowest_vcn,
		const u8 *val, const u32 val_len, ntfs_attr_search_ctx *ctx);

__private_extern__ errno_t ntfs_resident_attr_read(ntfs_inode *ni,
		const s64 ofs, const u32 cnt, u8 *buf);
__private_extern__ errno_t ntfs_resident_attr_write(ntfs_inode *ni, u8 *buf,
		u32 cnt, const s64 ofs);

#endif /* !_OSX_NTFS_ATTR_H */
