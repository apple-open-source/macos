/*
 * ntfs_mft.h - Defines for mft record handling in the NTFS kernel driver.
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

#ifndef _OSX_NTFS_MFT_H
#define _OSX_NTFS_MFT_H

#include <sys/errno.h>

#include "ntfs_inode.h"
#include "ntfs_layout.h"
#include "ntfs_types.h"
#include "ntfs_volume.h"

__private_extern__ errno_t ntfs_mft_record_map_ext(ntfs_inode *ni,
		MFT_RECORD **m, const BOOL mft_is_locked);

/**
 * ntfs_mft_record_map - map and lock an mft record
 * @ni:		ntfs inode whose mft record to map
 * @m:		destination pointer for the mapped mft record
 *
 * The buffer containing the mft record belonging to the ntfs inode @ni is
 * mapped which on OS X means it is held for exclusive via the BL_BUSY flag in
 * the buffer.  The mapped mft record is returned in *@m.
 *
 * Return 0 on success and errno on error.
 *
 * Note: Caller must hold an iocount reference on the vnode of the base inode
 * of @ni.
 */
static inline errno_t ntfs_mft_record_map(ntfs_inode *ni, MFT_RECORD **m)
{
	return ntfs_mft_record_map_ext(ni, m, FALSE); 
}

__private_extern__ void ntfs_mft_record_unmap(ntfs_inode *ni);

__private_extern__ errno_t ntfs_extent_mft_record_map_ext(ntfs_inode *base_ni,
		MFT_REF mref, ntfs_inode **nni, MFT_RECORD **nm,
		const BOOL mft_is_locked);

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
static inline errno_t ntfs_extent_mft_record_map(ntfs_inode *base_ni,
		MFT_REF mref, ntfs_inode **ext_ni, MFT_RECORD **ext_mrec)
{
	return ntfs_extent_mft_record_map_ext(base_ni, mref, ext_ni, ext_mrec,
			FALSE); 
}

static inline void ntfs_extent_mft_record_unmap(ntfs_inode *ni)
{
	ntfs_mft_record_unmap(ni);
}

__private_extern__ errno_t ntfs_mft_record_sync(ntfs_inode *ni);

__private_extern__ errno_t ntfs_mft_mirror_sync(ntfs_volume *vol,
		const s64 rec_no, const MFT_RECORD *m, const BOOL sync);

__private_extern__ errno_t ntfs_mft_record_alloc(ntfs_volume *vol,
		struct vnode_attr *va, struct componentname *cn,
		ntfs_inode *base_ni, ntfs_inode **new_ni, MFT_RECORD **new_m,
		ATTR_RECORD **new_a);

__private_extern__ errno_t ntfs_extent_mft_record_free(ntfs_inode *base_ni,
		ntfs_inode *ni, MFT_RECORD *m);

#endif /* !_OSX_NTFS_MFT_H */
