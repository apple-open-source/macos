/*
 * ntfs_attr_list.h - Defines for attribute list attribute handling in the NTFS
 *		      kernel driver.
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

#ifndef _OSX_NTFS_ATTR_LIST_H
#define _OSX_NTFS_ATTR_LIST_H

#include <sys/errno.h>

#include "ntfs_attr.h"
#include "ntfs_endian.h"
#include "ntfs_inode.h"
#include "ntfs_layout.h"
#include "ntfs_types.h"

__private_extern__ errno_t ntfs_attr_list_is_needed(ntfs_inode *ni,
		ATTR_LIST_ENTRY *skip_entry, BOOL *attr_list_is_needed);

__private_extern__ errno_t ntfs_attr_list_delete(ntfs_inode *ni,
		ntfs_attr_search_ctx *ctx);

__private_extern__ errno_t ntfs_attr_list_add(ntfs_inode *ni, MFT_RECORD *m,
		ntfs_attr_search_ctx *ctx);

__private_extern__ errno_t ntfs_attr_list_sync_shrink(ntfs_inode *ni,
		const unsigned start_ofs, ntfs_attr_search_ctx *ctx);

__private_extern__ errno_t ntfs_attr_list_sync_extend(ntfs_inode *base_ni,
		MFT_RECORD *base_m, unsigned al_ofs,
		ntfs_attr_search_ctx *ctx);

/**
 * ntfs_attr_list_sync - update the attribute list content of an ntfs inode
 * @ni:		base ntfs inode whose attribute list attribugte to update
 * @start_ofs:	byte offset into attribute list attribute from which to write
 * @ctx:	initialized attribute search context
 *
 * Write the attribute list attribute value cached in @ni starting at byte
 * offset @start_ofs into it to the attribute list attribute record (if the
 * attribute list attribute is resident) or to disk as specified by the runlist
 * of the attribute list attribute.
 *
 * This function only works when the attribute list content but not its size
 * has changed.
 *
 * @ctx is an initialized, ready to use attribute search context that we use to
 * look up the attribute list attribute in the mapped, base mft record.
 *
 * Return 0 on success and -errno on error.
 */
static inline int ntfs_attr_list_sync(ntfs_inode *ni, const unsigned start_ofs,
		ntfs_attr_search_ctx *ctx)
{
	return ntfs_attr_list_sync_shrink(ni, start_ofs, ctx);
}

__private_extern__ void ntfs_attr_list_entries_delete(ntfs_inode *ni,
		ATTR_LIST_ENTRY *start_entry, ATTR_LIST_ENTRY *end_entry);

/**
 * ntfs_attr_list_entry_delete - delete an attribute list entry
 * @ni:			base ntfs inode whose attribute list to delete from
 * @target_entry:	attribute list entry to be deleted
 *
 * Delete the attribute list attribute entry @target_entry from the attribute
 * list attribute belonging to the base ntfs inode @ni.
 *
 * This function cannot fail.
 */
static inline void ntfs_attr_list_entry_delete(ntfs_inode *ni,
		ATTR_LIST_ENTRY *target_entry)
{
	ntfs_attr_list_entries_delete(ni, target_entry,
			(ATTR_LIST_ENTRY*)((u8*)target_entry +
			le16_to_cpu(target_entry->length)));
}

#endif /* !_OSX_NTFS_ATTR_LIST_H */
