/*
 * ntfs_usnjrnl.c - NTFS kernel transaction log ($UsnJrnl) handling.
 *
 * Copyright (c) 2006-2011 Anton Altaparmakov.  All Rights Reserved.
 * Portions Copyright (c) 2006-2011 Apple Inc.  All Rights Reserved.
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

#include <kern/locks.h>

#include "ntfs_debug.h"
#include "ntfs_endian.h"
#include "ntfs_page.h"
#include "ntfs_time.h"
#include "ntfs_types.h"
#include "ntfs_usnjrnl.h"
#include "ntfs_volume.h"

/**
 * ntfs_usnjrnl_stamp - stamp the transaction log ($UsnJrnl) on an ntfs volume
 * @vol:	ntfs volume on which to stamp the transaction log
 *
 * Stamp the transaction log ($UsnJrnl) on the ntfs volume @vol and return 0
 * on success and errno on error.
 *
 * This function assumes that the transaction log has already been loaded and
 * consistency checked by a call to ntfs_vfsops.c::ntfs_usnjrnl_load().
 */
errno_t ntfs_usnjrnl_stamp(ntfs_volume *vol)
{
	ntfs_debug("Entering.");
	if (!NVolUsnJrnlStamped(vol)) {
		sle64 j_size, stamp;
		upl_t upl;
		upl_page_info_array_t pl;
		USN_HEADER *uh;
		ntfs_inode *max_ni;
		errno_t err;

		lck_spin_lock(&vol->usnjrnl_j_ni->size_lock);
		j_size = vol->usnjrnl_j_ni->data_size;
		lck_spin_unlock(&vol->usnjrnl_j_ni->size_lock);
		max_ni = vol->usnjrnl_max_ni;
		err = vnode_get(max_ni->vn);
		if (err) {
			ntfs_error(vol->mp, "Failed to get vnode for "
					"$UsnJrnl/$DATA/$Max.");
			return err;
		}
		lck_rw_lock_shared(&max_ni->lock);
		err = ntfs_page_map(max_ni, 0, &upl, &pl, (u8**)&uh, TRUE);
		if (err) {
			ntfs_error(vol->mp, "Failed to read from "
					"$UsnJrnl/$DATA/$Max attribute.");
			(void)vnode_put(max_ni->vn);
			return err;
		}
		stamp = ntfs_current_time();
		ntfs_debug("Stamping transaction log ($UsnJrnl): old "
				"journal_id 0x%llx, old lowest_valid_usn "
				"0x%llx, new journal_id 0x%llx, new "
				"lowest_valid_usn 0x%llx.",
				(unsigned long long)
				sle64_to_cpu(uh->journal_id),
				(unsigned long long)
				sle64_to_cpu(uh->lowest_valid_usn),
				(unsigned long long)sle64_to_cpu(stamp),
				(unsigned long long)j_size);
		uh->lowest_valid_usn = cpu_to_sle64(j_size);
		uh->journal_id = stamp;
		ntfs_page_unmap(max_ni, upl, pl, TRUE);
		lck_rw_unlock_shared(&max_ni->lock);
		(void)vnode_put(max_ni->vn);
		/* Set the flag so we do not have to do it again on remount. */
		NVolSetUsnJrnlStamped(vol);
		// TODO: Should we mark any times on the base inode $UsnJrnl
		// for update here?
	}
	ntfs_debug("Done.");
	return 0;
}
