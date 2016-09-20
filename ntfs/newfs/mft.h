/*
 * mft.h - Exports for MFT record handling. 
 *
 * Copyright (c) 2000-2002 Anton Altaparmakov
 * Copyright (c) 2004-2005 Richard Russon
 * Copyright (c) 2006-2008 Szabolcs Szakacsits
 * Copyright (c) 2008-2012 Tuxera Inc.
 *
 * See LICENSE file for licensing information.
 */

#ifndef _NTFS_MFT_H
#define _NTFS_MFT_H

#include "volume.h"
#include "inode.h"
#include "layout.h"
#include "logging.h"

extern int ntfs_mft_records_read(const ntfs_volume *vol, const MFT_REF mref,
		const s64 count, MFT_RECORD *b);

/**
 * ntfs_mft_record_read - read a record from the mft
 * @vol:	volume to read from
 * @mref:	mft record number to read
 * @b:		output data buffer
 *
 * Read the mft record specified by @mref from volume @vol into buffer @b.
 * Return 0 on success or -1 on error, with errno set to the error code.
 *
 * The read mft record is mst deprotected and is hence ready to use. The caller
 * should check the record with is_baad_record() in case mst deprotection
 * failed.
 *
 * NOTE: @b has to be at least of size vol->mft_record_size.
 */
static __inline__ int ntfs_mft_record_read(const ntfs_volume *vol,
		const MFT_REF mref, MFT_RECORD *b)
{
	int ret; 
	
	ntfs_log_enter("Entering for inode %lld\n", (long long)MREF(mref));
	ret = ntfs_mft_records_read(vol, mref, 1, b);
	ntfs_log_leave("\n");
	return ret;
}

extern int ntfs_mft_record_check(const ntfs_volume *vol, const MFT_REF mref, 
		MFT_RECORD *m);

extern int ntfs_file_record_read(const ntfs_volume *vol, const MFT_REF mref,
		MFT_RECORD **mrec, ATTR_RECORD **attr);

extern int ntfs_mft_records_write(const ntfs_volume *vol, const MFT_REF mref,
		const s64 count, MFT_RECORD *b);

/**
 * ntfs_mft_record_write - write an mft record to disk
 * @vol:	volume to write to
 * @mref:	mft record number to write
 * @b:		data buffer containing the mft record to write
 *
 * Write the mft record specified by @mref from buffer @b to volume @vol.
 * Return 0 on success or -1 on error, with errno set to the error code.
 *
 * Before the mft record is written, it is mst protected. After the write, it
 * is deprotected again, thus resulting in an increase in the update sequence
 * number inside the buffer @b.
 *
 * NOTE: @b has to be at least of size vol->mft_record_size.
 */
static __inline__ int ntfs_mft_record_write(const ntfs_volume *vol,
		const MFT_REF mref, MFT_RECORD *b)
{
	int ret; 
	
	ntfs_log_enter("Entering for inode %lld\n", (long long)MREF(mref));
	ret = ntfs_mft_records_write(vol, mref, 1, b);
	ntfs_log_leave("\n");
	return ret;
}

extern int ntfs_mft_record_layout(const ntfs_volume *vol, const MFT_REF mref,
		MFT_RECORD *mrec);

extern int ntfs_mft_record_format(const ntfs_volume *vol, const MFT_REF mref);

extern ntfs_inode *ntfs_mft_record_alloc(ntfs_volume *vol, ntfs_inode *base_ni);

extern int ntfs_mft_record_free(ntfs_volume *vol, ntfs_inode *ni);

extern int ntfs_mft_usn_dec(MFT_RECORD *mrec);

#endif /* defined _NTFS_MFT_H */
