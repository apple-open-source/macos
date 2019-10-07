/*
 * ntfs_dir.h - Defines for directory handling in the NTFS kernel driver.
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

#ifndef _OSX_NTFS_DIR_H
#define _OSX_NTFS_DIR_H

#include <sys/errno.h>
#include <sys/uio.h>

#include "ntfs.h"
#include "ntfs_inode.h"
#include "ntfs_layout.h"
#include "ntfs_types.h"

/*
 * ntfs_name is used to return the actual found filename to the caller of
 * ntfs_lookup_inode_by_name() in order for the caller
 * (ntfs_vnops.c::ntfs_vnop_lookup()) to be able to deal with the case
 * sensitive name cache effectively.
 */
typedef struct {
	MFT_REF mref;
	FILENAME_TYPE_FLAGS type;
	u8 len;
	ntfschar name[NTFS_MAX_NAME_LEN];
} ntfs_dir_lookup_name;

/* The little endian Unicode string $I30 as a global constant. */
__attribute__((visibility("hidden"))) extern ntfschar I30[5];

__private_extern__ errno_t ntfs_lookup_inode_by_name(ntfs_inode *dir_ni,
		const ntfschar *uname, const signed uname_len,
		MFT_REF *res_mref, ntfs_dir_lookup_name **res_name);

__private_extern__ errno_t ntfs_readdir(ntfs_inode *dir_ni, uio_t uio,
		int *eofflag, int *numdirent);

__private_extern__ errno_t ntfs_dir_is_empty(ntfs_inode *dir_ni);

__private_extern__ errno_t ntfs_dir_entry_delete(ntfs_inode *dir_ni,
		ntfs_inode *ni, const FILENAME_ATTR *fn, const u32 fn_len);

__private_extern__ errno_t ntfs_dir_entry_add(ntfs_inode *dir_ni,
		const FILENAME_ATTR *fn, const u32 fn_len,
		const leMFT_REF mref);

/**
 * struct _ntfs_dirhint - directory hint structure
 *
 * This is used to store state across directory enumerations, i.e. across calls
 * to ntfs_readdir().
 */
struct _ntfs_dirhint {
	TAILQ_ENTRY(_ntfs_dirhint) link;
	unsigned ofs;
	unsigned time;
	unsigned fn_size;
	FILENAME_ATTR *fn;
};
typedef struct _ntfs_dirhint ntfs_dirhint;

/*
 * NTFS_MAX_DIRHINTS cannot be larger than 63 without reducing
 * NTFS_DIR_POS_MASK, because given the 6-bit tag, at most 63 different tags
 * can exist.  When NTFS_MAX_DIRHINTS is larger than 63, the same list may
 * contain dirhints of the same tag, and a staled dirhint may be returned.
 */
#define NTFS_MAX_DIRHINTS 32
#define NTFS_DIRHINT_TTL 45
#define NTFS_DIR_POS_MASK 0x03ffffff
#define NTFS_DIR_TAG_MASK 0xfc000000
#define NTFS_DIR_TAG_SHIFT 26

__private_extern__ void ntfs_dirhints_put(ntfs_inode *ni, BOOL stale_only);

#endif /* !_OSX_NTFS_DIR_H */
