/*
 * ntfs_bitmap.h - Defines for bitmap handling in the NTFS kernel driver.
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

#ifndef _OSX_NTFS_BITMAP_H
#define _OSX_NTFS_BITMAP_H

#include <sys/errno.h>

#include "ntfs_inode.h"
#include "ntfs_types.h"

__private_extern__ errno_t __ntfs_bitmap_set_bits_in_run(ntfs_inode *ni,
		const s64 start_bit, const s64 count, const u8 value,
		const BOOL is_rollback);

/**
 * ntfs_bitmap_set_bits_in_run - set a run of bits in a bitmap to a value
 * @ni:			ntfs inode describing the bitmap
 * @start_bit:		first bit to set
 * @count:		number of bits to set
 * @value:		value to set the bits to (i.e. 0 or 1)
 *
 * Set @count bits starting at bit @start_bit in the bitmap described by the
 * ntfs inode @ni to @value, where @value is either 0 or 1.
 *
 * Return 0 on success and errno on error.
 *
 * Locking: The caller must hold @ni->lock.
 */
static inline errno_t ntfs_bitmap_set_bits_in_run(ntfs_inode *ni,
		const s64 start_bit, const s64 count, const u8 value)
{
	return __ntfs_bitmap_set_bits_in_run(ni, start_bit, count, value,
			FALSE);
}

/**
 * ntfs_bitmap_set_run - set a run of bits in a bitmap
 * @ni:		ntfs inode describing the bitmap
 * @start_bit:	first bit to set
 * @count:	number of bits to set
 *
 * Set @count bits starting at bit @start_bit in the bitmap described by the
 * ntfs inode @ni.
 *
 * Return 0 on success and errno on error.
 *
 * Locking: The caller must hold @ni->lock.
 */
static inline errno_t ntfs_bitmap_set_run(ntfs_inode *ni, const s64 start_bit,
		const s64 count)
{
	return ntfs_bitmap_set_bits_in_run(ni, start_bit, count, 1);
}

/**
 * ntfs_bitmap_clear_run - clear a run of bits in a bitmap
 * @ni:		ntfs inode describing the bitmap
 * @start_bit:	first bit to clear
 * @count:	number of bits to clear
 *
 * Clear @count bits starting at bit @start_bit in the bitmap described by the
 * ntfs inode @ni.
 *
 * Return 0 on success and errno on error.
 *
 * Locking: The caller must hold @ni->lock.
 */
static inline errno_t ntfs_bitmap_clear_run(ntfs_inode *ni,
		const s64 start_bit, const s64 count)
{
	return ntfs_bitmap_set_bits_in_run(ni, start_bit, count, 0);
}

/**
 * ntfs_bitmap_set_bit - set a bit in a bitmap
 * @ni:		ntfs inode describing the bitmap
 * @bit:	bit to set
 *
 * Set bit @bit in the bitmap described by the ntfs inode @ni.
 *
 * Return 0 on success and errno on error.
 *
 * Locking: The caller must hold @ni->lock.
 */
static inline errno_t ntfs_bitmap_set_bit(ntfs_inode *ni, const s64 bit)
{
	return ntfs_bitmap_set_run(ni, bit, 1);
}

/**
 * ntfs_bitmap_clear_bit - clear a bit in a bitmap
 * @ni:		ntfs inode describing the bitmap
 * @bit:	bit to clear
 *
 * Clear bit @bit in the bitmap described by the ntfs inode @ni.
 *
 * Return 0 on success and errno on error.
 *
 * Locking: The caller must hold @ni->lock.
 */
static inline errno_t ntfs_bitmap_clear_bit(ntfs_inode *ni, const s64 bit)
{
	return ntfs_bitmap_clear_run(ni, bit, 1);
}

#endif /* !_OSX_NTFS_BITMAP_H */
