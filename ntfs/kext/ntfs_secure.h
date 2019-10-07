/*
 * ntfs_secure.h - Defines for security ($Secure) handling in the NTFS kernel
 *		   driver.
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

#ifndef _OSX_NTFS_SECURE_H
#define _OSX_NTFS_SECURE_H

#include <sys/errno.h>
#include <sys/ucred.h>
#include <sys/vnode.h>

#include "ntfs_types.h"
#include "ntfs_endian.h"
#include "ntfs_layout.h"
#include "ntfs_volume.h"

__attribute__((visibility("hidden"))) extern SDS_ENTRY *ntfs_file_sds_entry;
__attribute__((visibility("hidden"))) extern SDS_ENTRY *ntfs_dir_sds_entry;
__attribute__((visibility("hidden"))) extern SDS_ENTRY *ntfs_file_sds_entry_old;
__attribute__((visibility("hidden"))) extern SDS_ENTRY *ntfs_dir_sds_entry_old;

/**
 * ntfs_rol32 - rotate a value to the left
 * @x:		value whose bits to rotate to the left
 * @n:		number of bits to rotate @x by
 *
 * Rotate the bits of @x to the left by @n bits.
 *
 * Return the rotated value.
 */
static inline u32 ntfs_rol32(const u32 x, const unsigned n)
{
	return (x << n) | (x >> (32 - n));
}

/**
 * ntfs_security_hash - calculate the hash of a security descriptor
 * @sd:		self-relative security descriptor whose hash to calculate
 * @length:	size in bytes of the security descritor @sd
 *
 * Calculate the hash of the self-relative security descriptor @sd of length
 * @length bytes.
 *
 * This hash is used in the $Secure system file as the primary key for the $SDH
 * index and is also stored in the header of each security descriptor in the
 * $SDS data stream as well as in the index data of both the $SII and $SDH
 * indexes.  In all three cases it forms part of the SDS_ENTRY_HEADER
 * structure.
 *
 * Return the calculated security hash in little endian.
 */
static inline le32 ntfs_security_hash(SECURITY_DESCRIPTOR_RELATIVE *sd,
	const u32 length)
{
	le32 *pos, *end;
	u32 hash;

	pos = (le32*)sd;
	end = (le32*)sd + (length / sizeof(le32));
	for (hash = 0; pos < end; pos++)
		hash = le32_to_cpup(pos) + ntfs_rol32(hash, 3);
	return cpu_to_le32(hash);
}

__private_extern__ errno_t ntfs_default_sds_entries_init(void);

__private_extern__ errno_t ntfs_next_security_id_init(ntfs_volume *vol,
		le32 *next_security_id);

__private_extern__ errno_t ntfs_default_security_id_init(ntfs_volume *vol,
		struct vnode_attr *va);

#endif /* _OSX_NTFS_SECURE_H */
