/*
 * ntfs_mst.c - NTFS kernel multi sector transfer protection operations.
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

#include "ntfs.h"
#include "ntfs_endian.h"
#include "ntfs_layout.h"
#include "ntfs_mst.h"
#include "ntfs_types.h"

/**
 * ntfs_mst_fixup_post_read - deprotect multi sector transfer protected data
 * @b:		pointer to the data to deprotect
 * @size:	size in bytes of @b
 *
 * Perform the necessary post read multi sector transfer fixup and detect the
 * presence of incomplete multi sector transfers.
 *
 * In the case of an incomplete transfer being detected, overwrite the magic of
 * the ntfs record header being processed with "BAAD" and abort processing.
 *
 * Return 0 on success and EIO on error ("BAAD" magic will be present).
 *
 * NOTE: We consider the absence / invalidity of an update sequence array to
 * mean that the structure is not protected at all and hence does not need to
 * be fixed up.  Thus, we return success and not failure in this case.  This is
 * in contrast to ntfs_mst_fixup_pre_write(), see below.
 */
errno_t ntfs_mst_fixup_post_read(NTFS_RECORD *b, const u32 size)
{
	u16 *usa_pos, *data_pos;
	u16 usa_ofs, usa_count, usn;

	/* Setup the variables. */
	usa_ofs = le16_to_cpu(b->usa_ofs);
	/* Decrement usa_count to get number of fixups. */
	usa_count = le16_to_cpu(b->usa_count) - 1;
	/* Size and alignment checks. */
	if (size & (NTFS_BLOCK_SIZE - 1) || usa_ofs & 1 ||
			(u32)usa_ofs + ((u32)usa_count * 2) > size ||
			(size >> NTFS_BLOCK_SIZE_SHIFT) != usa_count)
		return 0;
	/* Position of usn in update sequence array. */
	usa_pos = (u16*)b + usa_ofs/sizeof(u16);
	/*
	 * The update sequence number which has to be equal to each of the u16
	 * values before they are fixed up.  Note no need to care for
	 * endianness since we are comparing and moving data for on disk
	 * structures which means the data is consistent.  If it is consistenty
	 * the wrong endianness it does not make any difference.
	 */
	usn = *usa_pos;
	/* Position in protected data of first u16 that needs fixing up. */
	data_pos = (u16*)b + NTFS_BLOCK_SIZE/sizeof(u16) - 1;
	/* Check for incomplete multi sector transfer(s). */
	while (usa_count--) {
		if (*data_pos != usn) {
			/*
			 * Incomplete multi sector transfer detected!  )-:
			 * Set the magic to "BAAD" and return failure.
			 * Note that magic_BAAD is already little endian.
			 */
			b->magic = magic_BAAD;
			return EIO;
		}
		data_pos += NTFS_BLOCK_SIZE/sizeof(u16);
	}
	/* Re-setup the variables. */
	usa_count = le16_to_cpu(b->usa_count) - 1;
	data_pos = (u16*)b + NTFS_BLOCK_SIZE/sizeof(u16) - 1;
	/* Fixup all sectors. */
	while (usa_count--) {
		/*
		 * Increment position in usa and restore original data from
		 * the usa into the data buffer.
		 */
		*data_pos = *(++usa_pos);
		/* Increment position in data as well. */
		data_pos += NTFS_BLOCK_SIZE/sizeof(u16);
	}
	return 0;
}

/**
 * ntfs_mst_fixup_pre_write - apply multi sector transfer protection
 * @b:		pointer to the data to protect
 * @size:	size in bytes of @b
 *
 * Perform the necessary pre write multi sector transfer fixup on the data
 * pointed to by @b of @size.
 *
 * Return 0 if fixup applied (success) or EINVAL if no fixup was performed
 * (assumed not needed).  This is in contrast to ntfs_mst_fixup_post_read()
 * above.
 *
 * NOTE: We consider the absence / invalidity of an update sequence array to
 * mean that the structure is not subject to protection and hence does not need
 * to be fixed up.  This means that you have to create a valid update sequence
 * array header in the ntfs record before calling this function, otherwise it
 * will fail (the header needs to contain the position of the update sequence
 * array together with the number of elements in the array).  You also need to
 * initialise the update sequence number before calling this function
 * otherwise a random word will be used (whatever was in the record at that
 * position at that time).
 */
errno_t ntfs_mst_fixup_pre_write(NTFS_RECORD *b, const u32 size)
{
	le16 *usa_pos, *data_pos;
	u16 usa_ofs, usa_count, usn;
	le16 le_usn;

	/* Sanity check + only fixup if it makes sense. */
	if (!b || ntfs_is_baad_record(b->magic) ||
			ntfs_is_hole_record(b->magic))
		return EINVAL;
	/* Setup the variables. */
	usa_ofs = le16_to_cpu(b->usa_ofs);
	/* Decrement usa_count to get number of fixups. */
	usa_count = le16_to_cpu(b->usa_count) - 1;
	/* Size and alignment checks. */
	if (size & (NTFS_BLOCK_SIZE - 1) || usa_ofs & 1 ||
			(u32)usa_ofs + ((u32)usa_count * 2) > size ||
			(size >> NTFS_BLOCK_SIZE_SHIFT) != usa_count)
		return EINVAL;
	/* Position of usn in update sequence array. */
	usa_pos = (le16*)((u8*)b + usa_ofs);
	/*
	 * Cyclically increment the update sequence number (skipping 0 and -1,
	 * i.e. 0xffff).
	 */
	usn = le16_to_cpup(usa_pos) + 1;
	if (usn == 0xffff || !usn)
		usn = 1;
	le_usn = cpu_to_le16(usn);
	*usa_pos = le_usn;
	/* Position in data of first u16 that needs fixing up. */
	data_pos = (le16*)b + NTFS_BLOCK_SIZE/sizeof(le16) - 1;
	/* Fixup all sectors. */
	while (usa_count--) {
		/*
		 * Increment the position in the usa and save the
		 * original data from the data buffer into the usa.
		 */
		*(++usa_pos) = *data_pos;
		/* Apply fixup to data. */
		*data_pos = le_usn;
		/* Increment position in data as well. */
		data_pos += NTFS_BLOCK_SIZE/sizeof(le16);
	}
	return 0;
}

/**
 * ntfs_mst_fixup_post_write - fast deprotect mst protected data
 * @b:		pointer to the data to deprotect
 *
 * Perform the necessary post write multi sector transfer fixup, not checking
 * for any errors, because we assume we have just successfully used
 * ntfs_mst_fixup_pre_write(), thus the data will be fine or we would never
 * have gotten here.
 */
void ntfs_mst_fixup_post_write(NTFS_RECORD *b)
{
	le16 *usa_pos, *data_pos;
	u16 usa_ofs = le16_to_cpu(b->usa_ofs);
	u16 usa_count = le16_to_cpu(b->usa_count) - 1;
	/* Position of usn in update sequence array. */
	usa_pos = (le16*)b + usa_ofs/sizeof(le16);
	/* Position in protected data of first u16 that needs fixing up. */
	data_pos = (le16*)b + NTFS_BLOCK_SIZE/sizeof(le16) - 1;
	/* Fixup all sectors. */
	while (usa_count--) {
		/*
		 * Increment position in usa and restore original data from
		 * the usa into the data buffer.
		 */
		*data_pos = *(++usa_pos);
		/* Increment position in data as well. */
		data_pos += NTFS_BLOCK_SIZE/sizeof(le16);
	}
}
