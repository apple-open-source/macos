/*
 * ntfs_quota.c - NTFS kernel quota ($Quota) handling.
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
#include <sys/stat.h>
#include <sys/ucred.h>
#include <sys/vnode.h>

#include "ntfs_debug.h"
#include "ntfs_endian.h"
#include "ntfs_index.h"
#include "ntfs_inode.h"
#include "ntfs_layout.h"
#include "ntfs_quota.h"
#include "ntfs_time.h"
#include "ntfs_types.h"
#include "ntfs_volume.h"

/**
 * ntfs_quotas_mark_out_of_date - mark the quotas out of date on an ntfs volume
 * @vol:	ntfs volume on which to mark the quotas out of date
 *
 * Mark the quotas out of date on the ntfs volume @vol and return 0 on success
 * and errno on error.
 */
errno_t ntfs_quotas_mark_out_of_date(ntfs_volume *vol)
{
	ntfs_inode *quota_ni;
	ntfs_index_context *ictx;
	INDEX_ENTRY *ie;
	QUOTA_CONTROL_ENTRY *qce;
	const le32 qid = QUOTA_DEFAULTS_ID;
	errno_t err;

	ntfs_debug("Entering.");
	if (NVolQuotaOutOfDate(vol))
		goto done;
	quota_ni = vol->quota_ni;
	if (!quota_ni || !vol->quota_q_ni) {
		ntfs_error(vol->mp, "Quota inodes are not open.");
		return EINVAL;
	}
	err = vnode_get(vol->quota_q_ni->vn);
	if (err) {
		ntfs_error(vol->mp, "Failed to get index vnode for "
				"$Quota/$Q.");
		return err;
	}
	lck_rw_lock_exclusive(&vol->quota_q_ni->lock);
	ictx = ntfs_index_ctx_get(vol->quota_q_ni);
	if (!ictx) {
		ntfs_error(vol->mp, "Failed to get index context.");
		err = ENOMEM;
		goto err;
	}
	err = ntfs_index_lookup(&qid, sizeof(qid), &ictx);
	if (err) {
		if (err == ENOENT)
			ntfs_error(vol->mp, "Quota defaults entry is not "
					"present.");
		else
			ntfs_error(vol->mp, "Lookup of quota defaults entry "
					"failed.");
		goto err;
	}
	ie = ictx->entry;
	if (le16_to_cpu(ie->data_length) <
			offsetof(QUOTA_CONTROL_ENTRY, sid)) {
		ntfs_error(vol->mp, "Quota defaults entry size is invalid.  "
				"Run chkdsk.");
		err = EIO;
		goto err;
	}
	qce = (QUOTA_CONTROL_ENTRY*)((u8*)ie + le16_to_cpu(ie->data_offset));
	if (le32_to_cpu(qce->version) != QUOTA_VERSION) {
		ntfs_error(vol->mp, "Quota defaults entry version 0x%x is not "
				"supported.", le32_to_cpu(qce->version));
		err = EIO;
		goto err;
	}
	ntfs_debug("Quota defaults flags = 0x%x.", le32_to_cpu(qce->flags));
	/* If quotas are already marked out of date, no need to do anything. */
	if (qce->flags & QUOTA_FLAG_OUT_OF_DATE)
		goto set_done;
	/*
	 * If quota tracking is neither requested nor enabled and there are no
	 * pending deletes, no need to mark the quotas out of date.
	 */
	if (!(qce->flags & (QUOTA_FLAG_TRACKING_ENABLED |
			QUOTA_FLAG_TRACKING_REQUESTED |
			QUOTA_FLAG_PENDING_DELETES)))
		goto set_done;
	/*
	 * Set the QUOTA_FLAG_OUT_OF_DATE bit thus marking quotas out of date.
	 * This is verified on WinXP to be sufficient to cause windows to
	 * rescan the volume on boot and update all quota entries.
	 */
	qce->flags |= QUOTA_FLAG_OUT_OF_DATE;
	/* Ensure the modified flags are written to disk. */
	ntfs_index_entry_mark_dirty(ictx);
	/* Update the atime, mtime and ctime of the base inode @quota_ni. */
	quota_ni->last_access_time = quota_ni->last_mft_change_time =
			quota_ni->last_data_change_time =
			ntfs_utc_current_time();
	NInoSetDirtyTimes(quota_ni);
set_done:
	ntfs_index_ctx_put(ictx);
	lck_rw_unlock_exclusive(&vol->quota_q_ni->lock);
	(void)vnode_put(vol->quota_q_ni->vn);
	/*
	 * We set the flag so we do not try to mark the quotas out of date
	 * again on remount.
	 */
	NVolSetQuotaOutOfDate(vol);
done:
	ntfs_debug("Done.");
	return 0;
err:
	if (ictx)
		ntfs_index_ctx_put(ictx);
	lck_rw_unlock_exclusive(&vol->quota_q_ni->lock);
	(void)vnode_put(vol->quota_q_ni->vn);
	return err;
}
