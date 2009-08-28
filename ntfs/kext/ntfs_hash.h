/*
 * ntfs_hash.h - Defines for inode hash structures and operations for the NTFS
 *		 kernel driver.
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

#ifndef _OSX_NTFS_HASH_H
#define _OSX_NTFS_HASH_H

#include <sys/cdefs.h>
#include <sys/errno.h>
#include <sys/mount.h>
#include <sys/queue.h>

#include <kern/locks.h>

#include "ntfs_inode.h"
#include "ntfs_volume.h"

__private_extern__ lck_mtx_t ntfs_inode_hash_lock;

__private_extern__ errno_t ntfs_inode_hash_init(void);
__private_extern__ void ntfs_inode_hash_deinit(void);

__private_extern__ ntfs_inode *ntfs_inode_hash_lookup(ntfs_volume *vol,
		const ntfs_attr *na);
__private_extern__ ntfs_inode *ntfs_inode_hash_get(ntfs_volume *vol,
		const ntfs_attr *na);

/**
 * ntfs_inode_hash_rm_nolock - remove an ntfs inode from the ntfs inode hash
 * @ni:		ntfs inode to remove from the hash
 *
 * Remove the ntfs inode @ni from the ntfs inode hash.
 */
static inline void ntfs_inode_hash_rm_nolock(ntfs_inode *ni)
{
	LIST_REMOVE(ni, hash);
}

__private_extern__ void ntfs_inode_hash_add(ntfs_inode *ni);
__private_extern__ void ntfs_inode_hash_rm(ntfs_inode *ni);

#endif /* !_OSX_NTFS_HASH_H */
