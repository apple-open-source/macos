/*
 * ntfs_vfsops.c - NTFS kernel vfs operations.
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

#include <sys/cdefs.h>
#include <sys/attr.h>
#include <sys/buf.h>
#include <sys/disk.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/kauth.h>
#include <sys/kernel_types.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ubc.h>
#include <sys/ucred.h>
#include <sys/vnode.h>

#include <mach/kern_return.h>
#include <mach/kmod.h>
#include <mach/machine/vm_param.h>

#include <string.h>

#include <libkern/libkern.h>
#include <libkern/OSMalloc.h>
#include <libkern/OSKextLib.h>

#include <kern/debug.h>
#include <kern/locks.h>

#include <miscfs/specfs/specdev.h>

#include "ntfs.h"
#include "ntfs_attr.h"
#include "ntfs_attr_list.h"
#include "ntfs_debug.h"
#include "ntfs_dir.h"
#include "ntfs_hash.h"
#include "ntfs_inode.h"
#include "ntfs_layout.h"
#include "ntfs_logfile.h"
#include "ntfs_mft.h"
#include "ntfs_mst.h"
#include "ntfs_page.h"
#include "ntfs_quota.h"
#include "ntfs_secure.h"
#include "ntfs_time.h"
#include "ntfs_unistr.h"
#include "ntfs_usnjrnl.h"
#include "ntfs_version.h"
#include "ntfs_vnops.h"
#include "ntfs_volume.h"

// FIXME: Change email address but to what?
const char ntfs_dev_email[] = "linux-ntfs-dev@lists.sourceforge.net";
const char ntfs_please_email[] = "Please email "
		"linux-ntfs-dev@lists.sourceforge.net and say that you saw "
		"this message.  Thank you.";

/* A driver wide lock protecting the below global data structures. */
static lck_mtx_t ntfs_lock;

/* Number of mounted file systems which have compression enabled. */
static unsigned long ntfs_compression_users;
static u8 *ntfs_compression_buffer;
#define ntfs_compression_buffer_size (16 * 4096)

/* The global default upcase table and corresponding reference count. */
static unsigned long ntfs_default_upcase_users;
static ntfschar *ntfs_default_upcase;
#define ntfs_default_upcase_size (64 * 1024 * sizeof(ntfschar))

static errno_t ntfs_blocksize_set(mount_t mp, vnode_t dev_vn, u32 blocksize,
		vfs_context_t context)
{
	errno_t err;
	struct vfsioattr ia;

	err = VNOP_IOCTL(dev_vn, DKIOCSETBLOCKSIZE, (caddr_t)&blocksize,
			FWRITE, context);
	if (err)
		return ENXIO;
	/*
	 * Update the cached block size in the mount point, i.e. the value
	 * returned by vfs_devblocksize().
	 */
	ntfs_debug("Updating io attributes with new block size.");
	vfs_ioattr(mp, &ia);
	ia.io_devblocksize = blocksize;
	vfs_setioattr(mp, &ia);
	/*
	 * Update the block size in the block device, i.e. the
	 * v_specsize of the device vnode.
	 */
	ntfs_debug("Updating device vnode with new block size.");
	set_fsblocksize(dev_vn);
	return 0;
}

/**
 * ntfs_boot_sector_is_valid - check if @b contains a valid ntfs boot sector
 * @mp:		Mount of the device to which @b belongs.
 * @b:		Boot sector of device @mp to check.
 *
 * Check whether the boot sector @b is a valid ntfs boot sector.
 *
 * Return TRUE if it is valid and FALSE if not.
 *
 * @mp is only needed for warning/error output, i.e. it can be NULL.
 */
static BOOL ntfs_boot_sector_is_valid(const mount_t mp,
		const NTFS_BOOT_SECTOR *b)
{
	ntfs_debug("Entering.");
	/*
	 * Check that checksum == sum of u32 values from b to the checksum
	 * field.  If checksum is zero, no checking is done.  We will work when
	 * the checksum test fails, since some utilities update the boot sector
	 * ignoring the checksum which leaves the checksum out-of-date.  We
	 * report a warning if this is the case.
	 */
	if ((void*)b < (void*)&b->checksum && b->checksum) {
		le32 *u;
		u32 i;

		for (i = 0, u = (le32*)b; u < (le32*)(&b->checksum); ++u)
			i += le32_to_cpup(u);
		if (le32_to_cpu(b->checksum) != i)
			ntfs_warning(mp, "Invalid boot sector checksum.");
	}
	/* Check OEMidentifier is "NTFS    " */
	if (b->oem_id != magicNTFS)
		goto not_ntfs;
	/*
	 * Check bytes per sector value is between 256 and
	 * NTFS_MAX_SECTOR_SIZE.
	 */
	if (le16_to_cpu(b->bpb.bytes_per_sector) < 0x100 ||
			le16_to_cpu(b->bpb.bytes_per_sector) >
			NTFS_MAX_SECTOR_SIZE)
		goto not_ntfs;
	/* Check sectors per cluster value is valid. */
	switch (b->bpb.sectors_per_cluster) {
	case 1: case 2: case 4: case 8: case 16: case 32: case 64: case 128:
		break;
	default:
		goto not_ntfs;
	}
	/* Check the cluster size is not above the maximum (64kiB). */
	if ((u32)le16_to_cpu(b->bpb.bytes_per_sector) *
			b->bpb.sectors_per_cluster > NTFS_MAX_CLUSTER_SIZE)
		goto not_ntfs;
	/* Check reserved/unused fields are really zero. */
	if (le16_to_cpu(b->bpb.reserved_sectors) ||
			le16_to_cpu(b->bpb.root_entries) ||
			le16_to_cpu(b->bpb.sectors) ||
			le16_to_cpu(b->bpb.sectors_per_fat) ||
			le32_to_cpu(b->bpb.large_sectors) || b->bpb.fats)
		goto not_ntfs;
	/*
	 * Check clusters per file mft record value is valid.  It can be either
	 * between -31 and -9 (in which case the actual mft record size is
	 * -log2() of the absolute value) or a positive power of two.
	 */
	if ((u8)b->clusters_per_mft_record < 0xe1 ||
			(u8)b->clusters_per_mft_record > 0xf7)
		switch (b->clusters_per_mft_record) {
		case 1: case 2: case 4: case 8: case 16: case 32: case 64:
			break;
		default:
			goto not_ntfs;
		}
	/* Check clusters per index block value is valid. */
	if ((u8)b->clusters_per_index_block < 0xe1 ||
			(u8)b->clusters_per_index_block > 0xf7)
		switch (b->clusters_per_index_block) {
		case 1: case 2: case 4: case 8: case 16: case 32: case 64:
			break;
		default:
			goto not_ntfs;
		}
	/*
	 * Check for valid end of sector marker.  We will work without it, but
	 * many BIOSes will refuse to boot from a bootsector if the magic is
	 * incorrect, so we emit a warning.
	 */
	if (b->end_of_sector_marker != const_cpu_to_le16(0xaa55))
		ntfs_warning(mp, "Invalid end of sector marker.");
	ntfs_debug("Done.");
	return TRUE;
not_ntfs:
	ntfs_debug("Not an NTFS boot sector.");
	return FALSE;
}

/**
 * ntfs_boot_sector_read - read the ntfs boot sector of a device
 * @vol:	ntfs_volume of device to read the boot sector from
 * @cred:	credentials of running process
 * @buf:	destination pointer for buffer containing boot sector
 * @bs:		destination pointer for boot sector data
 *
 * Read the boot sector from the device and validate it.  If that fails, try to
 * read the backup boot sector, first from the end of the device a-la NT4 and
 * later and then from the middle of the device a-la NT3.51 and earlier.
 *
 * If a valid boot sector is found but it is not the primary boot sector, we
 * repair the primary boot sector silently (unless the device is read-only or
 * the primary boot sector is not accessible).
 *
 * On success return 0 and set *@buf to the buffer containing the boot sector
 * and *@bs to the boot sector data.  The caller then has to buf_unmap() and
 * buf_brelse() the buffer.
 *
 * On error return the error code.
 *
 * Note: We set the B_NOCACHE flag on the buffer(s), thus effectively
 * invalidating them when we release them.  This is needed because the
 * buffer(s) may get read later using a different vnode ($Boot for example).
 */
static errno_t ntfs_boot_sector_read(ntfs_volume *vol, kauth_cred_t cred,
		buf_t *buf, NTFS_BOOT_SECTOR **bs)
{
	daddr64_t nr_blocks = vol->nr_blocks;
	static const char read_err_str[] =
			"Unable to read %s boot sector (error %d).";
	mount_t mp = vol->mp;
	vnode_t dev_vn = vol->dev_vn;
	buf_t primary, backup;
	NTFS_BOOT_SECTOR *bs1, *bs2;
	errno_t err, err2;
	u32 blocksize = vfs_devblocksize(mp);

	ntfs_debug("Entering.");
	/* Try to read primary boot sector. */
	err = buf_meta_bread(dev_vn, 0, blocksize, cred, &primary);
	buf_setflags(primary, B_NOCACHE);
	if (!err) {
		err = buf_map(primary, (caddr_t*)&bs1);
		if (err) {
			ntfs_error(mp, "Failed to map buffer of primary boot "
					"sector (error %d).", err);
			bs1 = NULL;
		} else {
			if (ntfs_boot_sector_is_valid(mp, bs1)) {
				*buf = primary;
				*bs = bs1;
				ntfs_debug("Done.");
				return 0;
			}
			ntfs_error(mp, "Primary boot sector is invalid.");
			err = EIO;
		}
	} else {
		ntfs_error(mp, read_err_str, "primary", err);
		bs1 = NULL;
	}
	if (!(vol->on_errors & ON_ERRORS_RECOVER)) {
		ntfs_error(mp, "Mount option errors=recover not used.  "
				"Aborting without trying to recover.");
		if (bs1) {
			err2 = buf_unmap(primary);
			if (err2)
				ntfs_error(mp, "Failed to unmap buffer of "
						"primary boot sector (error "
						"%d).", err2);
		}
		buf_brelse(primary);
		return err;
	}
	/* Try to read NT4+ backup boot sector. */
	err = buf_meta_bread(dev_vn, nr_blocks - 1, blocksize, cred, &backup);
	buf_setflags(backup, B_NOCACHE);
	if (!err) {
		err = buf_map(backup, (caddr_t*)&bs2);
		if (err)
			ntfs_error(mp, "Failed to map buffer of backup boot "
					"sector (error %d).", err);
		else {
			if (ntfs_boot_sector_is_valid(mp, bs2))
				goto hotfix_primary_boot_sector;
			err = buf_unmap(backup);
			if (err)
				ntfs_error(mp, "Failed to unmap buffer of "
						"backup boot sector (error "
						"%d).", err);
		}
	} else
		ntfs_error(mp, read_err_str, "backup", err);
	buf_brelse(backup);
	/* Try to read NT3.51- backup boot sector. */
	err = buf_meta_bread(dev_vn, nr_blocks >> 1, blocksize, cred, &backup);
	buf_setflags(backup, B_NOCACHE);
	if (!err) {
		err = buf_map(backup, (caddr_t*)&bs2);
		if (err)
			ntfs_error(mp, "Failed to map buffer of old backup "
					"boot sector (error %d).", err);
		else {
			if (ntfs_boot_sector_is_valid(mp, bs2))
				goto hotfix_primary_boot_sector;
			err = buf_unmap(backup);
			if (err)
				ntfs_error(mp, "Failed to unmap buffer of old "
						"backup boot sector (error "
						"%d).", err);
			err = EIO;
		}
		ntfs_error(mp, "Could not find a valid backup boot sector.");
	} else
		ntfs_error(mp, read_err_str, "backup", err);
	buf_brelse(backup);
	/* We failed.  Clean up and return. */
	if (bs1) {
		err2 = buf_unmap(primary);
		if (err2)
			ntfs_error(mp, "Failed to unmap buffer of primary "
					"boot sector (error %d).", err2);
	}
	buf_brelse(primary);
	return err;
hotfix_primary_boot_sector:
	ntfs_warning(mp, "Using backup boot sector.");
	/*
	 * If we managed to read sector zero and the volume is not read-only,
	 * copy the found, valid, backup boot sector to the primary boot
	 * sector.  Note we copy the complete sector, not just the boot sector
	 * structure as the sector size may be bigger and in this case it
	 * contains the correct boot loader code in the backup boot sector.
	 */
	if (bs1 && !NVolReadOnly(vol)) {
		ntfs_warning(mp, "Hot-fix: Recovering invalid primary boot "
				"sector from backup copy.");
		memcpy(bs1, bs2, blocksize);
		err = buf_bwrite(primary);
		if (err)
			ntfs_error(mp, "Hot-fix: Device write error while "
					"recovering primary boot sector "
					"(error %d).", err);
	} else {
		if (bs1) {
			ntfs_warning(mp, "Hot-fix: Recovery of primary boot "
					"sector failed: Read-only mount.");
			err = buf_unmap(primary);
			if (err)
				ntfs_error(mp, "Failed to unmap buffer of "
						"primary boot sector (error "
						"%d).", err);
		} else
			ntfs_warning(mp, "Hot-fix: Recovery of primary boot "
					"sector failed as it could not be "
					"mapped.");
		buf_brelse(primary);
	}
	*buf = backup;
	*bs = bs2;
	return 0;
}

/**
 * ntfs_boot_sector_parse - parse the boot sector and store the data in @vol
 * @vol:	volume structure to initialise with data from boot sector
 * @b:		boot sector to parse
 *
 * Parse the ntfs boot sector @b and store all imporant information therein in
 * the ntfs_volume @vol.
 *
 * Return 0 on success and errno on error.  The following error codes are
 * defined:
 *	EINVAL	- Boot sector is invalid.
 *	ENOTSUP - Volume is not supported by this ntfs driver.
 */
static errno_t ntfs_boot_sector_parse(ntfs_volume *vol,
		const NTFS_BOOT_SECTOR *b)
{
	s64 ll;
	mount_t mp = vol->mp;
	unsigned sectors_per_cluster_shift, nr_hidden_sects;
	int clusters_per_mft_record, clusters_per_index_block;

	ntfs_debug("Entering.");
	vol->sector_size = le16_to_cpu(b->bpb.bytes_per_sector);
	vol->sector_size_mask = vol->sector_size - 1;
	vol->sector_size_shift = ffs(vol->sector_size) - 1;
	ntfs_debug("vol->sector_size = %u (0x%x)", vol->sector_size,
			vol->sector_size);
	ntfs_debug("vol->sector_size_shift = %u", vol->sector_size_shift);
	if (vol->sector_size < (u32)vfs_devblocksize(mp)) {
		ntfs_error(mp, "Sector size (%u) is smaller than the device "
				"block size (%d).  This is not supported.  "
				"Sorry.", vol->sector_size,
				vfs_devblocksize(mp));
		return ENOTSUP;
	}
	ntfs_debug("sectors_per_cluster = %u", b->bpb.sectors_per_cluster);
	sectors_per_cluster_shift = ffs(b->bpb.sectors_per_cluster) - 1;
	ntfs_debug("sectors_per_cluster_shift = %u", sectors_per_cluster_shift);
	nr_hidden_sects = le32_to_cpu(b->bpb.hidden_sectors);
	ntfs_debug("number of hidden sectors = 0x%x", nr_hidden_sects);
	vol->cluster_size = vol->sector_size << sectors_per_cluster_shift;
	vol->cluster_size_mask = vol->cluster_size - 1;
	vol->cluster_size_shift = ffs(vol->cluster_size) - 1;
	ntfs_debug("vol->cluster_size = %u (0x%x)", vol->cluster_size,
			vol->cluster_size);
	ntfs_debug("vol->cluster_size_mask = 0x%x", vol->cluster_size_mask);
	ntfs_debug("vol->cluster_size_shift = %u", vol->cluster_size_shift);
	if (vol->cluster_size < vol->sector_size) {
		ntfs_error(mp, "Cluster size (%u) is smaller than the sector "
				"size (%u).  This is not supported.  Sorry.",
				vol->cluster_size, vol->sector_size);
		return ENOTSUP;
	}
	clusters_per_mft_record = b->clusters_per_mft_record;
	ntfs_debug("clusters_per_mft_record = %u (0x%x)",
			clusters_per_mft_record, clusters_per_mft_record);
	if (clusters_per_mft_record > 0)
		vol->mft_record_size = vol->cluster_size *
				clusters_per_mft_record;
	else
		/*
		 * When mft_record_size < cluster_size, clusters_per_mft_record
		 * = -log2(mft_record_size) bytes.  mft_record_size normaly is
		 * 1024 bytes, which is encoded as 0xF6 (-10 in decimal).
		 */
		vol->mft_record_size = 1 << -clusters_per_mft_record;
	vol->mft_record_size_mask = vol->mft_record_size - 1;
	vol->mft_record_size_shift = ffs(vol->mft_record_size) - 1;
	ntfs_debug("vol->mft_record_size = %u (0x%x)", vol->mft_record_size,
			vol->mft_record_size);
	ntfs_debug("vol->mft_record_size_mask = 0x%x",
			vol->mft_record_size_mask);
	ntfs_debug("vol->mft_record_size_shift = %u)",
			vol->mft_record_size_shift);
	/*
	 * We cannot support mft record sizes above the PAGE_SIZE since we
	 * store $MFT/$DATA, i.e. the table of mft records, in the unified
	 * buffer cache and thus in pages.
	 */
	if (vol->mft_record_size > PAGE_SIZE) {
		ntfs_error(mp, "Mft record size (%u) exceeds the PAGE_SIZE on "
				"your system (%u).  This is not supported.  "
				"Sorry.", vol->mft_record_size, PAGE_SIZE);
		return ENOTSUP;
	}
	/* We cannot support mft record sizes below the sector size. */
	if (vol->mft_record_size < vol->sector_size) {
		ntfs_error(mp, "Mft record size (%u) is smaller than the "
				"sector size (%u).  This is not supported.  "
				"Sorry.", vol->mft_record_size,
				vol->sector_size);
		return ENOTSUP;
	}
	clusters_per_index_block = b->clusters_per_index_block;
	ntfs_debug("clusters_per_index_block = %d (0x%x)",
			clusters_per_index_block, clusters_per_index_block);
	if (clusters_per_index_block > 0) {
		vol->index_block_size = vol->cluster_size *
				clusters_per_index_block;
		vol->blocks_per_index_block = clusters_per_index_block;
	} else {
		/*
		 * When index_block_size < cluster_size,
		 * clusters_per_index_block = -log2(index_block_size) bytes.
		 * index_block_size normaly equals 4096 bytes, which is encoded
		 * as 0xF4 (-12 in decimal).
		 */
		vol->index_block_size = 1 << -clusters_per_index_block;
		vol->blocks_per_index_block = vol->index_block_size /
				vol->sector_size;
	}
	vol->index_block_size_mask = vol->index_block_size - 1;
	vol->index_block_size_shift = ffs(vol->index_block_size) - 1;
	ntfs_debug("vol->index_block_size = %u (0x%x)",
			vol->index_block_size, vol->index_block_size);
	ntfs_debug("vol->index_block_size_mask = 0x%x",
			vol->index_block_size_mask);
	ntfs_debug("vol->index_block_size_shift = %u",
			vol->index_block_size_shift);
	ntfs_debug("vol->blocks_per_index_block = %u",
			vol->blocks_per_index_block);
	/* We cannot support index block sizes below the sector size. */
	if (vol->index_block_size < vol->sector_size) {
		ntfs_error(mp, "Index block size (%u) is smaller than the "
				"sector size (%u).  This is not supported.  "
				"Sorry.", vol->index_block_size,
				vol->sector_size);
		return ENOTSUP;
	}
	/*
	 * Get the size of the volume in clusters and check for 64-bit-ness.
	 * Windows currently only uses 32 bits to save the clusters so we do
	 * the same as we do not really want to break compatibility.  We could
	 * perhaps add a mount option to allow this one day but it would render
	 * such volumes incompatible with Windows.
	 */
	ll = sle64_to_cpu(b->number_of_sectors) >> sectors_per_cluster_shift;
	if ((u64)ll >= (u64)1 << 32) {
		ntfs_error(mp, "Volume specifies 64-bit clusters but only "
				"32-bit clusters are allowed by Microsoft "
				"Windows.  Weird.");
		return EINVAL;
	}
	vol->nr_clusters = ll;
	ntfs_debug("vol->nr_clusters = 0x%llx",
			(unsigned long long)vol->nr_clusters);
	ll = sle64_to_cpu(b->mft_lcn);
	if (ll >= vol->nr_clusters) {
		ntfs_error(mp, "MFT LCN (%lld, 0x%llx) is beyond end of "
				"volume.  Weird.", (unsigned long long)ll,
				(unsigned long long)ll);
		return EINVAL;
	}
	vol->mft_lcn = ll;
	ntfs_debug("vol->mft_lcn = 0x%llx", (unsigned long long)vol->mft_lcn);
	ll = sle64_to_cpu(b->mftmirr_lcn);
	if (ll >= vol->nr_clusters) {
		ntfs_error(mp, "MFTMirr LCN (%lld, 0x%llx) is beyond end of "
				"volume.  Weird.", (unsigned long long)ll,
				(unsigned long long)ll);
		return EINVAL;
	}
	vol->mftmirr_lcn = ll;
	ntfs_debug("vol->mftmirr_lcn = 0x%llx",
			(unsigned long long)vol->mftmirr_lcn);
	/*
	 * Work out the size of the mft mirror in number of mft records.  If
	 * the cluster size is less than or equal to the size taken by four mft
	 * records, the mft mirror stores the first four mft records.  If the
	 * cluster size is bigger than the size taken by four mft records, the
	 * mft mirror contains as many mft records as will fit into one
	 * cluster.
	 *
	 * Having said that Windows only keeps in sync and cares about the
	 * consistency of the first four mft records so we do the same.
	 */
#if 0
	if (vol->cluster_size <= ((u32)4 << vol->mft_record_size_shift))
		vol->mftmirr_size = 4;
	else
		vol->mftmirr_size = vol->cluster_size >>
				vol->mft_record_size_shift;
#else
	vol->mftmirr_size = 4;
#endif
	ntfs_debug("vol->mftmirr_size = 0x%x", vol->mftmirr_size);
	vol->serial_no = le64_to_cpu(b->volume_serial_number);
	ntfs_debug("vol->serial_no = 0x%llx",
			(unsigned long long)vol->serial_no);
	ntfs_debug("Done.");
	return 0;
}

/**
 * ntfs_setup_allocators - initialize the cluster and mft allocators
 * @vol:	volume structure for which to setup the allocators
 *
 * Setup the cluster (lcn) and mft allocators to the starting values.
 */
static void ntfs_setup_allocators(ntfs_volume *vol)
{
	LCN mft_zone_size, mft_lcn;

	ntfs_debug("Entering.");
	ntfs_debug("vol->mft_zone_multiplier = 0x%x",
			vol->mft_zone_multiplier);
	/* Determine the size of the MFT zone. */
	mft_zone_size = vol->nr_clusters;
	switch (vol->mft_zone_multiplier) {  /* % of volume size in clusters */
	case 4:
		mft_zone_size >>= 1;			/* 50%   */
		break;
	case 3:
		mft_zone_size = (mft_zone_size +
				(mft_zone_size >> 1)) >> 2;	/* 37.5% */
		break;
	case 2:
		mft_zone_size >>= 2;			/* 25%   */
		break;
	/* case 1: */
	default:
		mft_zone_size >>= 3;			/* 12.5% */
		break;
	}
	/* Setup the mft zone. */
	vol->mft_zone_start = vol->mft_zone_pos = vol->mft_lcn;
	ntfs_debug("vol->mft_zone_pos = 0x%llx",
			(unsigned long long)vol->mft_zone_pos);
	/*
	 * Calculate the mft_lcn for an unmodified ntfs volume (see mkntfs
	 * source) and if the actual mft_lcn is in the expected place or even
	 * further to the front of the volume, extend the mft_zone to cover the
	 * beginning of the volume as well.  This is in order to protect the
	 * area reserved for the mft bitmap as well within the mft_zone itself.
	 * On non-standard volumes we do not protect it as the overhead would
	 * be higher than the speed increase we would get by doing it.
	 */
	mft_lcn = (8192 + 2 * vol->cluster_size - 1) / vol->cluster_size;
	if (mft_lcn * vol->cluster_size < 16 * 1024)
		mft_lcn = (16 * 1024 + vol->cluster_size - 1) /
				vol->cluster_size;
	if (vol->mft_zone_start <= mft_lcn)
		vol->mft_zone_start = 0;
	ntfs_debug("vol->mft_zone_start = 0x%llx",
			(unsigned long long)vol->mft_zone_start);
	/*
	 * Need to cap the mft zone on non-standard volumes so that it does
	 * not point outside the boundaries of the volume.  We do this by
	 * halving the zone size until we are inside the volume.
	 */
	vol->mft_zone_end = vol->mft_lcn + mft_zone_size;
	while (vol->mft_zone_end >= vol->nr_clusters) {
		mft_zone_size >>= 1;
		vol->mft_zone_end = vol->mft_lcn + mft_zone_size;
	}
	ntfs_debug("vol->mft_zone_end = 0x%llx",
			(unsigned long long)vol->mft_zone_end);
	/*
	 * Set the current position within each data zone to the start of the
	 * respective zone.
	 */
	vol->data1_zone_pos = vol->mft_zone_end;
	ntfs_debug("vol->data1_zone_pos = 0x%llx",
			(unsigned long long)vol->data1_zone_pos);
	vol->data2_zone_pos = 0;
	ntfs_debug("vol->data2_zone_pos = 0x%llx",
			(unsigned long long)vol->data2_zone_pos);

	/* Set the mft data allocation position to mft record 24. */
	vol->mft_data_pos = 24;
	ntfs_debug("vol->mft_data_pos = 0x%llx",
			(unsigned long long)vol->mft_data_pos);
	ntfs_debug("Done.");
}

/**
 * ntfs_mft_inode_get - obtain the ntfs inode for $MFT at mount time
 * @vol:	ntfs volume being mounted
 *
 * Obtain the ntfs inode corresponding to the system file $MFT (unnamed $DATA
 * attribute) in the process bootstrapping the volume so that further inodes
 * can be obtained and (extent) mft records can be mapped.
 *
 * A new ntfs inode is allocated and initialized, the base mft record of $MFT
 * is read by hand from the device and this is then used to bootstrap the
 * volume so that mft record mapping/unmapping is working and therefore inodes
 * can be read in general.  To do so a new vnode is created and attached to the
 * new ntfs inode and the runlist for the $DATA attribute is fully mapped.
 *
 * Return 0 on success and errno on error.
 */
static errno_t ntfs_mft_inode_get(ntfs_volume *vol)
{
	daddr64_t block;
	VCN next_vcn, last_vcn, highest_vcn;
	ntfs_inode *ni;
	MFT_RECORD *m = NULL;
	vnode_t dev_vn = vol->dev_vn;
	buf_t buf;
	ntfs_attr_search_ctx *ctx = NULL;
	ATTR_RECORD *a;
	STANDARD_INFORMATION *si;
	errno_t err;
	const int block_size = vol->sector_size;
	unsigned nr_blocks, u;
	ntfs_attr na;
	char *es = "  $MFT is corrupt.  Run chkdsk.";
	const u8 block_size_shift = vol->sector_size_shift;

	ntfs_debug("Entering.");
	na = (ntfs_attr) {
		.mft_no = FILE_MFT,
		.type = AT_UNUSED,
		.raw = FALSE,
	};
	ni = ntfs_inode_hash_get(vol, &na);
	if (!ni) {
		ntfs_error(vol->mp, "Failed to allocate new inode.");
		return ENOMEM;
	}
	if (!NInoAlloc(ni)) {
		ntfs_error(vol->mp, "Failed (found stale inode in cache).");
		err = ESTALE;
		goto err;
	}
	/*
	 * We allocated a new inode, now set it up as the unnamed data
	 * attribute.  It is special as it is mst protected.
	 */
	NInoSetNonResident(ni);
	NInoSetMstProtected(ni);
	NInoSetSparseDisabled(ni);
	ni->type = AT_DATA;
	ni->block_size = vol->mft_record_size;
	ni->block_size_shift = vol->mft_record_size_shift;
	/* No-one is allowed to access $MFT directly. */
	ni->uid = 0;
	ni->gid = 0;
	ni->mode = S_IFREG;
	/* Allocate enough memory to read the first mft record. */
	m = OSMalloc(vol->mft_record_size, ntfs_malloc_tag);
	if (!m) {
		ntfs_error(vol->mp, "Failed to allocate buffer for $MFT "
				"record 0.");
		err = ENOMEM;
		goto err;
	}
	/* Determine the first physical block of the $MFT/$DATA attribute. */
	block = vol->mft_lcn << (vol->cluster_size_shift - block_size_shift);
	nr_blocks = vol->mft_record_size >> block_size_shift;
	if (!nr_blocks)
		nr_blocks = 1;
	/* Load $MFT/$DATA's first mft record, one block at a time. */
	for (u = 0; u < nr_blocks; u++, block++) {
		u8 *src;

		err = buf_meta_bread(dev_vn, block, block_size, NOCRED, &buf);
		/*
		 * We set the B_NOCACHE flag on the buffer(s), thus effectively
		 * invalidating them when we release them.  This is needed
		 * because the buffer(s) will get read later using the $MFT
		 * base vnode.
		 */
		buf_setflags(buf, B_NOCACHE);
		if (err) {
			ntfs_error(vol->mp, "Failed to read $MFT record 0 "
					"(block %u, physical block 0x%llx, "
					"physical block size %d).", u,
					(unsigned long long)block, block_size);
			buf_brelse(buf);
			goto err;
		}
		err = buf_map(buf, (caddr_t*)&src);
		if (err) {
			ntfs_error(vol->mp, "Failed to map buffer of mft "
					"record 0 (block %u, physical block "
					"0x%llx, physical block size %d).", u,
					(unsigned long long)block, block_size);
			buf_brelse(buf);
			goto err;
		}
		memcpy((u8*)m + (u << block_size_shift), src, block_size);
		err = buf_unmap(buf);
		if (err)
			ntfs_error(vol->mp, "Failed to unmap buffer of mft "
					"record 0 (error %d).", err);
		buf_brelse(buf);
	}
	/* Apply the mst fixups. */
	err = ntfs_mst_fixup_post_read((NTFS_RECORD*)m, vol->mft_record_size);
	if (err) {
		/* TODO: Try to use the $MFTMirr now. */
		ntfs_error(vol->mp, "MST fixup failed.%s", es);
		goto io_err;
	}
	/*
	 * Need this to be able to sanity check attribute list references to
	 * $MFT.
	 */
	ni->seq_no = le16_to_cpu(m->sequence_number);
	/* Get the number of hard links, too. */
	ni->link_count = le16_to_cpu(m->link_count);
	ctx = ntfs_attr_search_ctx_get(ni, m);
	if (!ctx) {
		err = ENOMEM;
		goto err;
	}
	/*
	 * Find the standard information attribute in the mft record.  At this
	 * stage we have not setup the attribute list stuff yet, so this could
	 * in fact fail if the standard information is in an extent record, but
	 * this is not allowed hence not a problem.
	 */
	err = ntfs_attr_lookup(AT_STANDARD_INFORMATION, AT_UNNAMED, 0, 0, NULL,
			0, ctx);
	a = ctx->a;
	if (err || a->non_resident || a->flags) {
		if (err) {
			if (err == ENOENT) {
				/*
				 * TODO: We should be performing a hot fix here
				 * (if the recover mount option is set) by
				 * creating a new attribute.
				 */
				ntfs_error(vol->mp, "Standard information "
						"attribute is missing.");
			} else
				ntfs_error(vol->mp, "Failed to lookup "
						"standard information "
						"attribute.");
		} else {
info_err:
			ntfs_error(vol->mp, "Standard information attribute "
					"is corrupt.");
			err = EIO;
		}
		goto err;
	}
	si = (STANDARD_INFORMATION*)((u8*)a +
			le16_to_cpu(a->value_offset));
	/* Some bounds checks. */
	if ((u8*)si < (u8*)a || (u8*)si + le32_to_cpu(a->value_length) >
			(u8*)a + le32_to_cpu(a->length) ||
			(u8*)a + le32_to_cpu(a->length) > (u8*)ctx->m +
			vol->mft_record_size)
		goto info_err;
	/*
	 * Cache the create, the last data and mft modified, and the last
	 * access times in the ntfs inode.
	 */
	ni->creation_time = ntfs2utc(si->creation_time);
	ni->last_data_change_time = ntfs2utc(si->last_data_change_time);
	ni->last_mft_change_time = ntfs2utc(si->last_mft_change_time);
	ni->last_access_time = ntfs2utc(si->last_access_time);
	/* Find the attribute list attribute if present. */
	ntfs_attr_search_ctx_reinit(ctx);
	err = ntfs_attr_lookup(AT_ATTRIBUTE_LIST, AT_UNNAMED, 0, 0, NULL, 0,
			ctx);
	if (err) {
		if (err != ENOENT) {
			ntfs_error(vol->mp, "Failed to lookup attribute list "
					"attribute.%s", es);
			goto err;
		}
		ntfs_debug("$MFT does not have an attribute list attribute.");
	} else /* if (!err) */ {
		ATTR_LIST_ENTRY *al_entry, *next_al_entry;
		u8 *al_end;

		ntfs_debug("Attribute list attribute found in $MFT.");
		NInoSetAttrList(ni);
		a = ctx->a;
		if (a->flags & ATTR_COMPRESSION_MASK) {
			ntfs_error(vol->mp, "Attribute list attribute is "
					"compressed.  Not allowed.%s", es);
			goto io_err;
		}
		if (a->flags & (ATTR_IS_ENCRYPTED | ATTR_IS_SPARSE)) {
			if (a->non_resident) {
				ntfs_error(vol->mp, "Non-resident attribute "
						"list attribute is encrypted/"
						"sparse.  Not allowed.%s", es);
				goto io_err;
			}
			ntfs_warning(vol->mp, "Resident attribute list "
					"attribute is marked encrypted/sparse "
					"which is not true.  However, Windows "
					"allows this and chkdsk does not "
					"detect or correct it so we will just "
					"ignore the invalid flags and pretend "
					"they are not set.");
		}
		/* Now allocate memory for the attribute list. */
		ni->attr_list_size = (u32)ntfs_attr_size(a);
		ni->attr_list_alloc = (ni->attr_list_size + NTFS_ALLOC_BLOCK -
				1) & ~(NTFS_ALLOC_BLOCK - 1);
		ni->attr_list = OSMalloc(ni->attr_list_alloc, ntfs_malloc_tag);
		if (!ni->attr_list) {
			ni->attr_list_alloc = 0;
			ntfs_error(vol->mp, "Not enough memory to allocate "
					"buffer for attribute list.");
			err = ENOMEM;
			goto err;
		}
		if (a->non_resident) {
			NInoSetAttrListNonResident(ni);
			if (a->lowest_vcn) {
				ntfs_error(vol->mp, "Attribute list has non-"
						"zero lowest_vcn.%s", es);
				goto io_err;
			}
			/* Setup the runlist. */
			err = ntfs_mapping_pairs_decompress(vol, a,
					&ni->attr_list_rl);
			if (err) {
				ntfs_error(vol->mp, "Mapping pairs "
						"decompression failed with "
						"error code %d.%s", err, es);
				goto err;
			}
			/* Now read in the attribute list. */
			err = ntfs_rl_read(vol, &ni->attr_list_rl,
					ni->attr_list, (s64)ni->attr_list_size,
					sle64_to_cpu(a->initialized_size));
			if (err) {
				ntfs_error(vol->mp, "Failed to load attribute "
						"list attribute with error "
						"code %d.", err);
				goto err;
			}
		} else /* if (!a->non_resident) */ {
			u8 *al = (u8*)a + le16_to_cpu(a->value_offset);
			u8 *a_end = (u8*)a + le32_to_cpu(a->length);
			if (al < (u8*)a || al + le32_to_cpu(a->value_length) >
					a_end || (u8*)a_end > (u8*)ctx->m +
					vol->mft_record_size) {
				ntfs_error(vol->mp, "Corrupt attribute list "
						"attribute.%s", es);
				goto io_err;
			}
			/* Now copy the attribute list. */
			memcpy(ni->attr_list, (u8*)a +
					le16_to_cpu(a->value_offset),
					ni->attr_list_size);
		}
		/* The attribute list is now setup in memory. */
		/*
		 * FIXME: I do not know if this case is actually possible.
		 * According to logic it is not possible but I have seen too
		 * many weird things in MS software to rely on logic.  Thus we
		 * perform a manual search and make sure the first $MFT/$DATA
		 * extent is in the base inode.  If it is not we abort with an
		 * error and if we ever see a report of this error we will need
		 * to do some magic in order to have the necessary mft record
		 * loaded and in the right place.  But hopefully logic will
		 * prevail and this never happens...
		 */
		al_entry = (ATTR_LIST_ENTRY*)ni->attr_list;
		al_end = (u8*)al_entry + ni->attr_list_size;
		for (;; al_entry = next_al_entry) {
			/* Out of bounds check. */
			if ((u8*)al_entry < ni->attr_list ||
					(u8*)al_entry > al_end)
				goto em_err;
			/* Catch the end of the attribute list. */
			if ((u8*)al_entry == al_end)
				goto em_err;
			if (!al_entry->length)
				goto em_err;
			if ((u8*)al_entry + 6 > al_end || (u8*)al_entry +
					le16_to_cpu(al_entry->length) > al_end)
				goto em_err;
			next_al_entry = (ATTR_LIST_ENTRY*)((u8*)al_entry +
					le16_to_cpu(al_entry->length));
			if (le32_to_cpu(al_entry->type) >
					const_le32_to_cpu(AT_DATA))
				goto em_err;
			if (al_entry->type != AT_DATA)
				continue;
			/* We want an unnamed attribute. */
			if (al_entry->name_length)
				goto em_err;
			/* Want the first entry, i.e. lowest_vcn == 0. */
			if (al_entry->lowest_vcn)
				goto em_err;
			/* First entry has to be in the base mft record. */
			if (MREF_LE(al_entry->mft_reference) != ni->mft_no) {
				/* MFT references do not match, logic fails. */
				ntfs_error(vol->mp, "BUG: The first $DATA "
						"extent of $MFT is not in the "
						"base mft record.  Please "
						"report you saw this message "
						"to %s.", ntfs_dev_email);
				goto io_err;
			}
			/* Sequence numbers must match. */
			if (MSEQNO_LE(al_entry->mft_reference) != ni->seq_no)
				goto em_err;
			/* Done: Found first extent of $DATA as expected. */
			break;
		}
	}
	ntfs_attr_search_ctx_reinit(ctx);
	/* Now load all attribute extents. */
	a = NULL;
	next_vcn = last_vcn = highest_vcn = 0;
	while (!(err = ntfs_attr_lookup(AT_DATA, AT_UNNAMED, 0, next_vcn, NULL,
			0, ctx))) {
		/* Cache the current attribute. */
		a = ctx->a;
		/* $MFT must be non-resident. */
		if (!a->non_resident) {
			ntfs_error(vol->mp, "$MFT must be non-resident but a "
					"resident extent was found.%s", es);
			goto io_err;
		}
		/* $MFT must be uncompressed and unencrypted. */
		if (a->flags & ATTR_COMPRESSION_MASK ||
				a->flags & ATTR_IS_ENCRYPTED ||
				a->flags & ATTR_IS_SPARSE) {
			ntfs_error(vol->mp, "$MFT must be uncompressed, "
					"non-sparse, and unencrypted but a "
					"compressed/sparse/encrypted extent "
					"was found.%s", es);
			goto io_err;
		}
		/*
		 * Decompress the mapping pairs array of this extent and merge
		 * the result into the existing runlist.  No need for locking
		 * as we have exclusive access to the inode at this time and we
		 * are a mount in progress task, too.
		 */
		err = ntfs_mapping_pairs_decompress(vol, a, &ni->rl);
		if (err) {
			ntfs_error(vol->mp, "Mapping pairs decompression "
					"failed with error code %d.%s", err,
					es);
			goto err;
		}
		/* Get the lowest vcn for the next extent. */
		highest_vcn = sle64_to_cpu(a->highest_vcn);
		/*
		 * If we are in the first extent, bootstrap the volume so we
		 * can load other inodes and map (extent) mft records.
		 */
		if (!next_vcn) {
			if (a->lowest_vcn) {
				ntfs_error(vol->mp, "First extent of $DATA "
						"attribute has non zero "
						"lowest_vcn.%s", es);
				goto io_err;
			}
			/* Get the last vcn in the $DATA attribute. */
			last_vcn = sle64_to_cpu(a->allocated_size)
					>> vol->cluster_size_shift;
			/* Fill in the sizes. */
			ni->allocated_size = sle64_to_cpu(a->allocated_size);
			ni->data_size = sle64_to_cpu(a->data_size);
			ni->initialized_size = sle64_to_cpu(
					a->initialized_size);
			/*
			 * Verify the sizes are sane.  In particular both the
			 * data size and the initialized size must be multiples
			 * of the mft record size or we will panic() when
			 * reading the boundary in ntfs_cluster_iodone().
			 *
			 * Also the allocated size must be a multiple of the
			 * volume cluster size.
			 */
			if (ni->allocated_size & vol->cluster_size_mask ||
					ni->data_size &
					vol->mft_record_size_mask ||
					ni->initialized_size &
					vol->mft_record_size_mask) {
				ntfs_error(vol->mp, "$DATA attribute contains "
						"invalid size.%s", es);
				goto io_err;
			}
			/*
			 * Verify the number of mft records does not exceed
			 * 2^32 - 1.
			 */
			if (ni->data_size >> vol->mft_record_size_shift >=
					1LL << 32) {
				ntfs_error(vol->mp, "$MFT is too big.  "
						"Aborting.");
				goto io_err;
			}
			/* We have the size now so we can add the vnode. */
			err = ntfs_inode_add_vnode(ni, TRUE, NULL, NULL);
			if (err) {
				ntfs_error(vol->mp, "Failed to create a "
						"system vnode for $MFT (error "
						"%d).", err);
				goto err;
			}
			/*
			 * We will hold on to the $MFT inode for the duration
			 * of the mount thus we need to take a reference on the
			 * vnode.  Note we need to attach the inode to the
			 * volume here so that ntfs_read_inode() can call
			 * ntfs_attr_lookup() which needs to be able to map
			 * extent mft records which requires vol->mft_ni to be
			 * setup.
			 */
			err = vnode_ref(ni->vn);
			if (err)
				ntfs_error(vol->mp, "vnode_ref() failed!");
			OSIncrementAtomic(&ni->nr_refs);
			vol->mft_ni = ni;
			/* The $MFT inode is fully setup now, so unlock it. */
			ntfs_inode_unlock_alloc(ni);
			/*
			 * We can release the iocount reference now.  It will
			 * be taken as and when required in the low level code.
			 * We can ignore the return value as it always is zero.
			 */
			(void)vnode_put(ni->vn);
			/* If $MFT/$DATA has only one extent, we are done. */
			if (highest_vcn == last_vcn - 1)
				break;
		}
		next_vcn = highest_vcn + 1;
		if (next_vcn <= 0) {
			ntfs_error(vol->mp, "Invalid highest vcn in attribute "
					"extent.%s", es);
			goto io_err;
		}
		/* Avoid endless loops due to corruption. */
		if (next_vcn < sle64_to_cpu(a->lowest_vcn)) {
			ntfs_error(vol->mp, "Corrupt attribute extent would "
					"cause endless loop, aborting.%s", es);
			goto io_err;
		}
	}
	if (err && err != ENOENT) {
		ntfs_error(vol->mp, "Failed to lookup $MFT/$DATA attribute "
				"extent.%s", es); 
		goto err;
	}
	if (!a) {
		ntfs_error(vol->mp, "$MFT/$DATA attribute not found.%s", es);
		err = ENOENT;
		goto err;
	}
	if (highest_vcn != last_vcn - 1) {
		ntfs_error(vol->mp, "Failed to load the complete runlist for "
				"$MFT/$DATA.  Driver bug or corrupt $MFT.  "
				"Run chkdsk.");
		ntfs_debug("highest_vcn = 0x%llx, last_vcn - 1 = 0x%llx",
				(unsigned long long)highest_vcn,
				(unsigned long long)(last_vcn - 1));
		goto io_err;
	}
	ntfs_attr_search_ctx_put(ctx);
	OSFree(m, vol->mft_record_size, ntfs_malloc_tag);
	ntfs_debug("Done.");
	return 0;
em_err:
	ntfs_error(vol->mp, "Could not find first extent of $DATA attribute "
			"in attribute list.%s", es);
io_err:
	err = EIO;
err:
	if (ctx)
		ntfs_attr_search_ctx_put(ctx);
	if (m)
		OSFree(m, vol->mft_record_size, ntfs_malloc_tag);
	/* vol->mft_ni will be cleaned up by the caller. */
	if (!vol->mft_ni)
		ntfs_inode_reclaim(ni);
	return err;
}

/**
 * ntfs_inode_attach - load and attach an inode to an ntfs structure
 * @vol:	ntfs volume to which the inode to load belongs
 * @mft_no:	mft record number / inode number to obtain
 * @ni:		pointer in which to return the obtained ntfs inode
 * @parent_vn:	vnode of directory containing the inode to return or NULL
 *
 * Load the ntfs inode @mft_no from the mounted ntfs volume @vol, attach it by
 * getting a reference on it and return the ntfs inode in @ni.
 *
 * The created vnode is marked as a system vnoded so that the volume can be
 * unmounted.  (VSYSTEM vnodes are skipped during vflush()).)
 *
 * If @parent_vn is not NULL, it is set up as the parent directory vnode of the
 * vnode of the obtained inode.
 *
 * Return 0 on success and errno on error.  On error *@ni is set to NULL.
 */
static errno_t ntfs_inode_attach(ntfs_volume *vol, const ino64_t mft_no,
		ntfs_inode **ni, vnode_t parent_vn)
{
	vnode_t vn;
	errno_t err;

	ntfs_debug("Entering.");
	err = ntfs_inode_get(vol, mft_no, TRUE, LCK_RW_TYPE_SHARED, ni,
			parent_vn, NULL);
	if (err) {
		ntfs_error(vol->mp, "Failed to load inode 0x%llx.",
				(unsigned long long)mft_no);
		*ni = NULL;
		return err;
	}
	/*
	 * Take an internal reference on the parent inode to balance the
	 * reference taken on the parent vnode in vnode_create().
	 */
	if (parent_vn)
		OSIncrementAtomic(&NTFS_I(parent_vn)->nr_refs);
	vn = (*ni)->vn;
	err = vnode_ref(vn);
	if (err)
		ntfs_error(vol->mp, "vnode_ref() failed!");
	OSIncrementAtomic(&(*ni)->nr_refs);
	lck_rw_unlock_shared(&(*ni)->lock);
	(void)vnode_put(vn);
	ntfs_debug("Done.");
	return 0;
}

/**
 * ntfs_attr_inode_attach - load and attach an attribute inode to a structure
 * @base_ni:	ntfs base inode containing the attribute
 * @type:	attribute type
 * @name:	Unicode name of the attribute (NULL if unnamed)
 * @name_len:	length of @name in Unicode characters (0 if unnamed)
 * @ni:		pointer in which to return the obtained ntfs inode
 *
 * Load the attribute inode described by @type, @name, and @name_len belonging
 * to the base inode @base_ni, attach it by getting a reference on it and
 * return the ntfs inode in @ni.
 *
 * The created vnode is marked as a system vnode so that the volume can be
 * unmounted.  (VSYSTEM vnodes are skipped during vflush()).)
 *
 * The vnode of the base inode @base_ni is set up as the parent vnode of the
 * vnode of the obtained inode.
 *
 * Return 0 on success and errno on error.  On error *@ni is set to NULL.
 */
static errno_t ntfs_attr_inode_attach(ntfs_inode *base_ni,
		const ATTR_TYPE type, ntfschar *name, const u32 name_len,
		ntfs_inode **ni)
{
	vnode_t vn;
	errno_t err;

	ntfs_debug("Entering.");
	err = ntfs_attr_inode_get(base_ni, type, name, name_len, TRUE,
			LCK_RW_TYPE_SHARED, ni);
	if (err) {
		ntfs_error(base_ni->vol->mp, "Failed to load attribute inode "
				"0x%llx, attribute type 0x%x, name length "
				"0x%x.", (unsigned long long)base_ni->mft_no,
				(unsigned)le32_to_cpu(type),
				(unsigned)name_len);
		*ni = NULL;
		return err;
	}
	/*
	 * Take an internal reference on the base inode @base_ni (which is also
	 * the parent inode) to balance the reference taken on the parent vnode
	 * in vnode_create().
	 */
	OSIncrementAtomic(&base_ni->nr_refs);
	vn = (*ni)->vn;
	err = vnode_ref(vn);
	if (err)
		ntfs_error(base_ni->vol->mp, "vnode_ref() failed!");
	OSIncrementAtomic(&(*ni)->nr_refs);
	lck_rw_unlock_shared(&(*ni)->lock);
	(void)vnode_put(vn);
	ntfs_debug("Done.");
	return 0;
}

/**
 * ntfs_index_inode_attach - load and attach an attribute inode to a structure
 * @base_ni:	ntfs base inode containing the index
 * @name:	Unicode name of the index
 * @name_len:	length of @name in Unicode characters
 * @ni:		pointer in which to return the obtained ntfs inode
 *
 * Load the index inode described by @name and @name_len belonging to the base
 * inode @base_ni, attach it by getting a reference on it and return the ntfs
 * inode in @ni.
 *
 * The created vnode is marked as a system vnode so that the volume can be
 * unmounted.  (VSYSTEM vnodes are skipped during vflush()).)
 *
 * The vnode of the base inode @base_ni is set up as the parent vnode of the
 * vnode of the obtained inode.
 *
 * Return 0 on success and errno on error.  On error *@ni is set to NULL.
 */
static errno_t ntfs_index_inode_attach(ntfs_inode *base_ni, ntfschar *name,
		const u32 name_len, ntfs_inode **ni)
{
	vnode_t vn;
	errno_t err;

	ntfs_debug("Entering.");
	err = ntfs_index_inode_get(base_ni, name, name_len, TRUE, ni);
	if (err) {
		ntfs_error(base_ni->vol->mp, "Failed to load index inode "
				"0x%llx, name length 0x%x.",
				(unsigned long long)base_ni->mft_no,
				(unsigned)name_len);
		*ni = NULL;
		return err;
	}
	/*
	 * Take an internal reference on the base inode @base_ni (which is also
	 * the parent inode) to balance the reference taken on the parent vnode
	 * in vnode_create().
	 */
	OSIncrementAtomic(&base_ni->nr_refs);
	vn = (*ni)->vn;
	err = vnode_ref(vn);
	if (err)
		ntfs_error(base_ni->vol->mp, "vnode_ref() failed!");
	OSIncrementAtomic(&(*ni)->nr_refs);
	(void)vnode_put(vn);
	ntfs_debug("Done.");
	return 0;
}

/**
 * ntfs_mft_mirror_load - load and setup the mft mirror inode
 * @vol:	ntfs volume describing device whose mft mirror to load
 *
 * Return 0 on success and errno on error.
 */
static errno_t ntfs_mft_mirror_load(ntfs_volume *vol)
{
	ntfs_inode *ni;
	vnode_t vn;
	errno_t err;

	ntfs_debug("Entering.");
	err = ntfs_inode_get(vol, FILE_MFTMirr, TRUE, LCK_RW_TYPE_SHARED, &ni,
			vol->root_ni->vn, NULL);
	if (err) {
		ntfs_error(vol->mp, "Failed to load inode 0x%llx.",
				(unsigned long long)FILE_MFTMirr);
		return err;
	}
	vn = ni->vn;
	/*
	 * Re-initialize some specifics about the inode of $MFTMirr as
	 * ntfs_inode_get() will have set up the default ones.
	 */
	/* Set uid and gid to root. */
	ni->uid = 0;
	ni->gid = 0;
	/* Regular file.  No access for anyone. */
	ni->mode = S_IFREG;
	/*
	 * The $MFTMirr, like the $MFT is multi sector transfer protected but
	 * we do not mark it as such as we want to have the buffers directly
	 * copied from the mft thus we do not want to mess about with MST
	 * fixups on the mft mirror.
	 */
	NInoSetSparseDisabled(ni);
	ni->block_size = vol->mft_record_size;
	ni->block_size_shift = vol->mft_record_size_shift;
	/*
	 * Verify the sizes are sane.  In particular both the data size and the
	 * initialized size must be multiples of the mft record size or we will
	 * panic() when reading the boundary in ntfs_cluster_iodone().
	 *
	 * Also the allocated size must be a multiple of the volume cluster
	 * size.
	 */
	if (ni->allocated_size & vol->cluster_size_mask ||
			ni->data_size & vol->mft_record_size_mask ||
			ni->initialized_size & vol->mft_record_size_mask) {
		ntfs_error(vol->mp, "$DATA attribute contains invalid size.  "
				"$MFTMirr is corrupt.  Run chkdsk.");
		(void)vnode_recycle(vn);
		(void)vnode_put(vn);
		return EIO;
	}
	OSIncrementAtomic(&vol->root_ni->nr_refs);
	err = vnode_ref(vn);
	if (err)
		ntfs_error(vol->mp, "vnode_ref() failed!");
	OSIncrementAtomic(&ni->nr_refs);
	lck_rw_unlock_shared(&ni->lock);
	(void)vnode_put(vn);
	vol->mftmirr_ni = ni;
	ntfs_debug("Done.");
	return 0;
}

/**
 * ntfs_mft_mirror_check - compare contents of the mft mirror with the mft
 * @vol:	ntfs volume describing device whose mft mirror to check
 *
 * Return 0 on success and errno on error.
 *
 * Note, this function also results in the mft mirror runlist being completely
 * mapped into memory.  The mft mirror write code requires this and will
 * panic() should it find an unmapped runlist element.
 */
static errno_t ntfs_mft_mirror_check(ntfs_volume *vol)
{
	ntfs_inode *ni;
	buf_t buf;
	u8 *mirr_start;
	MFT_RECORD *mirr, *m;
	unsigned nr_mirr_recs, alloc_size, rec_size, i;
	errno_t err, err2;

	ntfs_debug("Entering.");
	if (!vol->mftmirr_size)
		panic("%s(): !vol->mftmirr_size\n", __FUNCTION__);
	nr_mirr_recs = vol->mftmirr_size;
	if (!nr_mirr_recs)
		panic("%s(): !nr_mirr_recs\n", __FUNCTION__);
	rec_size = vol->mft_record_size;
	/* Allocate a buffer and read all mft mirror records into it. */
	alloc_size = nr_mirr_recs << vol->mft_record_size_shift;
	mirr_start = OSMalloc(alloc_size, ntfs_malloc_tag);
	if (!mirr_start) {
		ntfs_error(vol->mp, "Failed to allocate temporary mft mirror "
				"buffer.");
		return ENOMEM;
	}
	mirr = (MFT_RECORD*)mirr_start;
	ni = vol->mftmirr_ni;
	err = vnode_get(ni->vn);
	if (err) {
		ntfs_error(vol->mp, "Failed to get vnode for $MFTMirr.");
		goto err;
	}
	lck_rw_lock_shared(&ni->lock);
	for (i = 0; i < nr_mirr_recs; i++) {
		/* Get the next $MFTMirr record. */
		err = buf_meta_bread(ni->vn, i, rec_size, NOCRED, &buf);
		if (err) {
			ntfs_error(vol->mp, "Failed to read $MFTMirr record "
					"%d (error %d).", i, err);
			goto brelse;
		}
		err = buf_map(buf, (caddr_t*)&m);
		if (err) {
			ntfs_error(vol->mp, "Failed to map buffer of $MFTMirr "
					"record %d (error %d).", i, err);
			goto brelse;
		}
		/*
		 * Copy the mirror record, drop the buffer, and remove the MST
		 * fixups.
		 */
		memcpy(mirr, m, rec_size);
		err = buf_unmap(buf);
		if (err) {
			ntfs_error(vol->mp, "Failed to unmap buffer of "
					"$MFTMirr record %d (error %d).", i,
					err);
			goto brelse;
		}
		buf_brelse(buf);
		err = ntfs_mst_fixup_post_read((NTFS_RECORD*)mirr, rec_size);
		/* Do not check the mirror record if it is not in use. */
		if (mirr->flags & MFT_RECORD_IN_USE) {
			if (err || ntfs_is_baad_record(mirr->magic)) {
				ntfs_error(vol->mp, "Incomplete multi sector "
						"transfer detected in mft "
						"mirror record %d.", i);
				if (!err)
					err = EIO;
				goto unlock;
			}
		}
		mirr = (MFT_RECORD*)((u8*)mirr + rec_size);
	}
	/*
	 * Because we have just read at least the beginning of the mft mirror,
	 * we know we have mapped at least the beginning of the runlist for it.
	 */
	lck_rw_lock_shared(&ni->rl.lock);
	/*
	 * The runlist for the mft mirror must contain at least @nr_mirr_recs
	 * mft records and they must be in the first run, i.e. consecutive on
	 * disk.
	 */
	if (ni->rl.rl->lcn != vol->mftmirr_lcn ||
			ni->rl.rl->length < (((s64)vol->mftmirr_size <<
			vol->mft_record_size_shift) +
			vol->cluster_size_mask) >> vol->cluster_size_shift) {
		ntfs_error(vol->mp, "$MFTMirr location mismatch.  Run "
				"chkdsk.");
		err = EIO;
	} else
		ntfs_debug("Done.");
	lck_rw_unlock_shared(&ni->rl.lock);
	lck_rw_unlock_shared(&ni->lock);
	(void)vnode_put(ni->vn);
	/*
	 * Now read the $MFT records one at a time and compare each against the
	 * already read $MFTMirr records.
	 */
	ni = vol->mft_ni;
	err = vnode_get(ni->vn);
	if (err) {
		ntfs_error(vol->mp, "Failed to get vnode for $MFT.");
		goto err;
	}
	lck_rw_lock_shared(&ni->lock);
	mirr = (MFT_RECORD*)mirr_start;
	for (i = 0; i < nr_mirr_recs; i++) {
		unsigned bytes;

		/* Get the current $MFT record. */
		err = buf_meta_bread(ni->vn, i, rec_size, NOCRED, &buf);
		if (err) {
			ntfs_error(vol->mp, "Failed to read $MFT record %d "
					"(error %d).", i, err);
			goto brelse;
		}
		err = buf_map(buf, (caddr_t*)&m);
		if (err) {
			ntfs_error(vol->mp, "Failed to map buffer of $MFT "
					"record %d (error %d).", i, err);
			goto brelse;
		}
		/* Do not check the mft record if it is not in use. */
		if (m->flags & MFT_RECORD_IN_USE) {
			/* Make sure the record is ok. */
			if (ntfs_is_baad_record(m->magic)) {
				ntfs_error(vol->mp, "Incomplete multi sector "
						"transfer detected in mft "
						"record %d.", i);
				err = EIO;
				goto unmap;
			}
		}
		/* Get the amount of data in the current record. */
		bytes = le32_to_cpu(m->bytes_in_use);
		if (bytes < sizeof(MFT_RECORD_OLD) || bytes > rec_size ||
				ntfs_is_baad_record(m->magic)) {
			bytes = le32_to_cpu(mirr->bytes_in_use);
			if (bytes < sizeof(MFT_RECORD_OLD) ||
					bytes > rec_size ||
					ntfs_is_baad_record(mirr->magic))
				bytes = rec_size;
		}
		/* Compare the two records. */
		if (bcmp(m, mirr, bytes)) {
			ntfs_error(vol->mp, "$MFT and $MFTMirr (record %d) do "
					"not match.  Run chkdsk.", i);
			err = EIO;
			goto unmap;
		}
		mirr = (MFT_RECORD*)((u8*)mirr + rec_size);
		err = buf_unmap(buf);
		if (err) {
			ntfs_error(vol->mp, "Failed to unmap buffer of $MFT "
					"record %d (error %d).", i, err);
			goto brelse;
		}
		buf_brelse(buf);
	}
unlock:
	lck_rw_unlock_shared(&ni->lock);
	(void)vnode_put(ni->vn);
err:
	OSFree(mirr_start, alloc_size, ntfs_malloc_tag);
	return err;
unmap:
	err2 = buf_unmap(buf);
	if (err2)
		ntfs_error(vol->mp, "Failed to unmap buffer of mft record %d "
				"in error code path (error %d).", i, err2);
brelse:
	buf_brelse(buf);
	goto unlock;
}

/**
 * ntfs_upcase_load - load the upcase table for an ntfs volume
 * @vol:	ntfs volume whose upcase to load
 *
 * Read the upcase table and setup @vol->upcase and @vol->upcase_len.
 *
 * Return 0 on success and errno on error.
 */
static errno_t ntfs_upcase_load(ntfs_volume *vol)
{
	s64 ofs, data_size = 0;
	ntfs_inode *ni;
	upl_t upl;
	upl_page_info_array_t pl;
	u8 *kaddr;
	errno_t err;
	unsigned u;

	ntfs_debug("Entering.");
	err = ntfs_inode_get(vol, FILE_UpCase, TRUE, LCK_RW_TYPE_SHARED, &ni,
			vol->root_ni->vn, NULL);
	if (err) {
		ni = NULL;
		goto err;
	}
	/*
	 * The upcase size must not be above 64k Unicode characters, must not
	 * be zero, and must be a multiple of sizeof(ntfschar).
	 */
	lck_spin_lock(&ni->size_lock);
	data_size = ni->data_size;
	lck_spin_unlock(&ni->size_lock);
	if (data_size <= 0 || data_size & (sizeof(ntfschar) - 1) ||
			data_size > (s64)(64 * 1024 * sizeof(ntfschar))) {
		err = EINVAL;
		goto err;
	}
	/* Allocate memory to hold the $UpCase data. */
	vol->upcase = OSMalloc(data_size, ntfs_malloc_tag);
	if (!vol->upcase) {
		err = ENOMEM;
		goto err;
	}
	/*
	 * Read the whole $UpCase file a page at a time and copy the contents
	 * over.
	 */
	u = PAGE_SIZE;
	for (ofs = 0; ofs < data_size; ofs += PAGE_SIZE) {
		err = ntfs_page_map(ni, ofs, &upl, &pl, &kaddr, FALSE);
		if (err)
			goto err;
		if (ofs + u > data_size)
			u = data_size - ofs;
		memcpy((u8*)vol->upcase + ofs, kaddr, u);
		ntfs_page_unmap(ni, upl, pl, FALSE);
	}
	lck_rw_unlock_shared(&ni->lock);
	(void)vnode_recycle(ni->vn);
	(void)vnode_put(ni->vn);
	vol->upcase_len = data_size >> NTFSCHAR_SIZE_SHIFT;
	ntfs_debug("Read %lld bytes from $UpCase (expected %lu bytes).",
			(long long)data_size, 64LU * 1024 * sizeof(ntfschar));
	lck_mtx_lock(&ntfs_lock);
	if (!ntfs_default_upcase) {
		ntfs_debug("Using volume specified $UpCase since default is "
				"not present.");
	} else {
		unsigned max_size;

		max_size = ntfs_default_upcase_size >> NTFSCHAR_SIZE_SHIFT;
		if (max_size > vol->upcase_len)
			max_size = vol->upcase_len;
		for (u = 0; u < max_size; u++)
			if (vol->upcase[u] != ntfs_default_upcase[u])
				break;
		if (u == max_size) {
			OSFree(vol->upcase, data_size, ntfs_malloc_tag);
			vol->upcase = ntfs_default_upcase;
			vol->upcase_len = ntfs_default_upcase_size >>
					NTFSCHAR_SIZE_SHIFT;
			ntfs_default_upcase_users++;
			ntfs_debug("Volume specified $UpCase matches "
					"default.  Using default.");
		} else
			ntfs_debug("Using volume specified $UpCase since it "
					"does not match the default.");
	}
	lck_mtx_unlock(&ntfs_lock);
	ntfs_debug("Done.");
	return 0;
err:
	if (vol->upcase) {
		OSFree(vol->upcase, data_size, ntfs_malloc_tag);
		vol->upcase = NULL;
		vol->upcase_len = 0;
	}
	if (ni) {
		lck_rw_unlock_shared(&ni->lock);
		(void)vnode_recycle(ni->vn);
		(void)vnode_put(ni->vn);
	}
	lck_mtx_lock(&ntfs_lock);
	if (ntfs_default_upcase) {
		vol->upcase = ntfs_default_upcase;
		vol->upcase_len = ntfs_default_upcase_size >>
				NTFSCHAR_SIZE_SHIFT;
		ntfs_default_upcase_users++;
		ntfs_error(vol->mp, "Failed to load $UpCase from the volume "
				"(error %d).  Using NTFS driver default "
				"upcase table instead.", err);
		err = 0;
	} else
		ntfs_error(vol->mp, "Failed to initialize upcase table.");
	lck_mtx_unlock(&ntfs_lock);
	return err;
}

/**
 * ntfs_attrdef_load - load the attribute definitions table for a volume
 * @vol:	ntfs volume whose attrdef to load
 *
 * Read the attribute definitions table and setup @vol->attrdef and
 * @vol->attrdef_size.
 *
 * Return 0 on success and errno on error.
 */
static errno_t ntfs_attrdef_load(ntfs_volume *vol)
{
	s64 ofs, data_size = 0;
	ntfs_inode *ni;
	upl_t upl;
	upl_page_info_array_t pl;
	u8 *kaddr;
	errno_t err;
	unsigned u;

	ntfs_debug("Entering.");
	err = ntfs_inode_get(vol, FILE_AttrDef, TRUE, LCK_RW_TYPE_SHARED, &ni,
			vol->root_ni->vn, NULL);
	if (err) {
		ni = NULL;
		goto err;
	}
	/*
	 * The attribute definitions size must be above 0 and fit inside 31
	 * bits.
	 */
	lck_spin_lock(&ni->size_lock);
	data_size = ni->data_size;
	lck_spin_unlock(&ni->size_lock);
	if (data_size <= 0 || data_size > 0x7fffffff) {
		err = EINVAL;
		goto err;
	}
	vol->attrdef = OSMalloc(data_size, ntfs_malloc_tag);
	if (!vol->attrdef) {
		err = ENOMEM;
		goto err;
	}
	/*
	 * Read the whole attribute definitions table a page at a time and copy
	 * the contents over.
	 */
	u = PAGE_SIZE;
	for (ofs = 0; ofs < data_size; ofs += PAGE_SIZE) {
		err = ntfs_page_map(ni, ofs, &upl, &pl, &kaddr, FALSE);
		if (err)
			goto err;
		if (ofs + u > data_size)
			u = data_size - ofs;
		memcpy((u8*)vol->attrdef + ofs, kaddr, u);
		ntfs_page_unmap(ni, upl, pl, FALSE);
	}
	lck_rw_unlock_shared(&ni->lock);
	(void)vnode_recycle(ni->vn);
	(void)vnode_put(ni->vn);
	vol->attrdef_size = data_size;
	ntfs_debug("Done.  Read %lld bytes from $AttrDef.",
			(long long)data_size);
	return 0;
err:
	if (vol->attrdef) {
		OSFree(vol->attrdef, data_size, ntfs_malloc_tag);
		vol->attrdef = NULL;
	}
	if (ni) {
		lck_rw_unlock_shared(&ni->lock);
		(void)vnode_recycle(ni->vn);
		(void)vnode_put(ni->vn);
	}
	ntfs_error(vol->mp, "Failed to initialize attribute definitions "
			"table.");
	return err;
}

/**
 * ntfs_volume_load - load the $Volume inode and setup the ntfs volume
 * @vol:	ntfs volume whose $Volume to load
 *
 * Load the $Volume system file and setup the volume flags (@vol->flags), the
 * volume major and minor version (@vol->major_ver and @vol->minor_ver,
 * respectively), and the volume name converted to decomposed utf-8 (@vol->name
 * and @vol->name_size).
 *
 * Return 0 on success and errno on error.
 */
static errno_t ntfs_volume_load(ntfs_volume *vol)
{
	ntfs_inode *ni;
	MFT_RECORD *m;
	ntfs_attr_search_ctx *ctx;
	ATTR_RECORD *a;
	VOLUME_INFORMATION *vi;
	errno_t err;

	ntfs_debug("Entering.");
	err = ntfs_inode_attach(vol, FILE_Volume, &ni, vol->root_ni->vn);
	if (err) {
		ntfs_error(vol->mp, "Failed to load $Volume.");
		return err;
	}
	vol->vol_ni = ni;
	err = vnode_get(ni->vn);
	if (err) {
		ntfs_error(vol->mp, "Failed to get vnode for $Volume.");
		return err;
	}
	err = ntfs_mft_record_map(ni, &m);
	if (err) {
		ntfs_error(vol->mp, "Failed to map mft record for $Volume.");
		goto err;
	}
	ctx = ntfs_attr_search_ctx_get(ni, m);
	if (!ctx) {
		ntfs_error(vol->mp, "Failed to get attribute search context "
				"for $Volume.");
		err = ENOMEM;
		goto unm_err;
	}
	err = ntfs_attr_lookup(AT_VOLUME_INFORMATION, AT_UNNAMED, 0, 0, NULL,
			0, ctx);
	a = ctx->a;
	if (err || a->non_resident || a->flags) {
		if (err)
			ntfs_error(vol->mp, "Failed to lookup volume "
					"information attribute in $Volume.");
		else {
info_err:
			ntfs_error(vol->mp, "Volume information attribute in "
					"$Volume is corrupt.  Run chkdsk.");
		}
		goto put_err;
	}
	vi = (VOLUME_INFORMATION*)((u8*)a + le16_to_cpu(a->value_offset));
	/* Some bounds checks. */
	if ((u8*)vi < (u8*)a || (u8*)vi + le32_to_cpu(a->value_length) >
			(u8*)a + le32_to_cpu(a->length) ||
			(u8*)a + le32_to_cpu(a->length) > (u8*)ctx->m +
			vol->mft_record_size)
		goto info_err;
	/* Copy the volume flags and version to the ntfs_volume structure. */
	vol->vol_flags = vi->flags;
	vol->major_ver = vi->major_ver;
	vol->minor_ver = vi->minor_ver;
	ntfs_attr_search_ctx_reinit(ctx);
	err = ntfs_attr_lookup(AT_VOLUME_NAME, AT_UNNAMED, 0, 0, NULL, 0, ctx);
	if (err == ENOENT) {
		ntfs_debug("Volume has no name, using empty string.");
no_name:
		/* No volume name, i.e. the name is "". */
		vol->name = OSMalloc(sizeof(char), ntfs_malloc_tag);
		if (!vol->name) {
			ntfs_error(vol->mp, "Failed to allocate memory for "
					"volume name.");
			err = ENOMEM;
			goto put_err;
		}
		vol->name[0] = '\0';
	} else {
		ntfschar *ntfs_name;
		u8 *utf8_name;
		size_t ntfs_size, utf8_size;
		signed res_size;

		a = ctx->a;
		if (err || a->non_resident || a->flags) {
			if (err)
				ntfs_error(vol->mp, "Failed to lookup volume "
						"name attribute in $Volume.");
			else {
name_err:
				ntfs_error(vol->mp, "Volume name attribute in "
						"$Volume is corrupt.  Run "
						"chkdsk.");
			}
put_err:
			ntfs_attr_search_ctx_put(ctx);
			if (!err)
				err = EIO;
			goto unm_err;
		}
		ntfs_name = (ntfschar*)((u8*)a + le16_to_cpu(a->value_offset));
		ntfs_size = le32_to_cpu(a->value_length);
		if (!ntfs_size) {
			ntfs_debug("Volume has empty name, using empty "
					"string.");
			goto no_name;
		}
		/* Some bounds checks. */
		if ((u8*)ntfs_name < (u8*)a || (u8*)ntfs_name + ntfs_size >
				(u8*)a + le32_to_cpu(a->length) ||
				(u8*)a + le32_to_cpu(a->length) > (u8*)ctx->m +
				vol->mft_record_size)
			goto name_err;
		/* Convert the name to decomposed utf-8 (NUL terminated). */
		utf8_name = NULL;
		res_size = ntfs_to_utf8(vol, ntfs_name, ntfs_size, &utf8_name,
				&utf8_size);
		if (res_size < 0) {
			err = -res_size;
			ntfs_error(vol->mp, "Failed to convert volume name to "
					"decomposed UTF-8 (error %d).",
					(int)err);
			goto put_err;
		}
		vol->name = (char*)utf8_name;
		vol->name_size = utf8_size;
	}
	ntfs_attr_search_ctx_put(ctx);
	ntfs_mft_record_unmap(ni);
	(void)vnode_put(ni->vn);
	ntfs_debug("Done.");
	return 0;
unm_err:
	ntfs_mft_record_unmap(ni);
err:
	(void)vnode_put(ni->vn);
	/* Obtained inode will be released by the call to ntfs_unmount(). */
	return err;
}

#define NTFS_HIBERFIL_HEADER_SIZE	4096

/**
 * ntfs_windows_hibernation_status_check - check if Windows is suspended
 * @vol:		ntfs volume to check
 * @is_hibernated:	pointer in which to return the hibernation status
 *
 * Check if Windows is hibernated on the ntfs volume @vol.  This is done by
 * looking for the file hiberfil.sys in the root directory of the volume.  If
 * the file is not present Windows is definitely not suspended and if it is
 * then the $LogFile will be marked dirty/still open so we will already have
 * caught that case.
 *
 * If hiberfil.sys exists and is less than 4kiB in size it means Windows is
 * definitely suspended (this volume is not the system volume).  Caveat:  on a
 * system with many volumes it is possible that the < 4kiB check is bogus but
 * for now this should do fine.
 *
 * If hiberfil.sys exists and is larger than 4kiB in size, we need to read the
 * hiberfil header (which is the first 4kiB).  If this begins with "hibr",
 * Windows is definitely suspended.  If it is completely full of zeroes,
 * Windows is definitely not hibernated.  Any other case is treated as if
 * Windows is suspended.  This caters for the above mentioned caveat of a
 * system with many volumes where no "hibr" magic would be present and there is
 * no zero header.
 *
 * If Windows is not hibernated on the volume *@is_hibernated is false and if
 * Windows is hibernated on the volume it is set to true.
 *
 * Return 0 on success and errno on error.  On error, *@is_hibernated is
 * undefined.
 */
static errno_t ntfs_windows_hibernation_status_check(ntfs_volume *vol,
		BOOL *is_hibernated)
{
	s64 data_size;
	MFT_REF mref;
	ntfs_dir_lookup_name *name = NULL;
	ntfs_inode *ni;
	upl_t upl = NULL;
	upl_page_info_array_t pl;
	le32 *kaddr, *kend;
	errno_t err;
	static const ntfschar hiberfil[13] = { const_cpu_to_le16('h'),
			const_cpu_to_le16('i'), const_cpu_to_le16('b'),
			const_cpu_to_le16('e'), const_cpu_to_le16('r'),
			const_cpu_to_le16('f'), const_cpu_to_le16('i'),
			const_cpu_to_le16('l'), const_cpu_to_le16('.'),
			const_cpu_to_le16('s'), const_cpu_to_le16('y'),
			const_cpu_to_le16('s'), 0 };

	ntfs_debug("Entering.");
	*is_hibernated = FALSE;
	/*
	 * Find the inode number for the hibernation file by looking up the
	 * filename hiberfil.sys in the root directory.
	 */
	lck_rw_lock_shared(&vol->root_ni->lock);
	err = ntfs_lookup_inode_by_name(vol->root_ni, hiberfil, 12, &mref,
			&name);
	lck_rw_unlock_shared(&vol->root_ni->lock);
	if (err) {
		/* If the file does not exist, Windows is not hibernated. */
		if (err == ENOENT) {
			ntfs_debug("hiberfil.sys not present.  Windows is not "
					"hibernated on the volume.");
			return 0;
		}
		/* A real error occured. */
		ntfs_error(vol->mp, "Failed to find inode number for "
				"hiberfil.sys.");
		return err;
	}
	/* We do not care for the type of match that was found. */
	if (name)
		OSFree(name, sizeof(*name), ntfs_malloc_tag);
	/* Get the inode. */
	err = ntfs_inode_get(vol, MREF(mref), FALSE, LCK_RW_TYPE_SHARED, &ni,
			vol->root_ni->vn, NULL);
	if (err) {
		ntfs_error(vol->mp, "Failed to load hiberfil.sys.");
		return err;
	}
	lck_spin_lock(&ni->size_lock);
	data_size = ni->data_size;
	lck_spin_unlock(&ni->size_lock);
	if (data_size < NTFS_HIBERFIL_HEADER_SIZE) {
		ntfs_debug("Hiberfil.sys is present and smaller than the "
				"hibernation header size.  Windows is "
				"hibernated on the volume.  This is not the "
				"system volume.");
		*is_hibernated = TRUE;
		goto put;
	}
	err = ntfs_page_map(ni, 0, &upl, &pl, (u8**)&kaddr, FALSE);
	if (err) {
		ntfs_error(vol->mp, "Failed to read from hiberfil.sys.");
		goto put;
	}
	if (*kaddr == const_cpu_to_le32(0x72626968)/*'hibr'*/) {
		ntfs_debug("Magic \"hibr\" found in hiberfil.sys.  Windows is "
				"hibernated on the volume.  This is the "
				"system volume.");
		*is_hibernated = TRUE;
		goto unm;
	}
	kend = kaddr + NTFS_HIBERFIL_HEADER_SIZE/sizeof(*kaddr);
	do {
		if (*kaddr) {
			ntfs_debug("hiberfil.sys is larger than 4kiB "
					"(0x%llx), does not contain the "
					"\"hibr\" magic, and does not have a "
					"zero header.  Windows is hibernated "
					"on the volume.  This is not the "
					"system volume.", data_size);
			*is_hibernated = TRUE;
			goto unm;
		}
	} while (++kaddr < kend);
	ntfs_debug("hiberfil.sys contains a zero header.  Windows is not "
			"hibernated on the volume.  This is the system "
			"volume.");
	/* @err is currently zero. */
unm:
	ntfs_page_unmap(ni, upl, pl, FALSE);
put:
	lck_rw_unlock_shared(&ni->lock);
	(void)vnode_recycle(ni->vn);
	(void)vnode_put(ni->vn);
	return err;
}

/**
 * ntfs_volume_flags_write - write new flags to the volume information flags
 * @vol:	ntfs volume on which to modify the flags
 * @flags:	new flags value for the volume information flags
 *
 * Internal function.  You probably want to use ntfs_volume_flags_{set,clear}()
 * instead (see below).
 *
 * Replace the volume information flags on the volume @vol with the value
 * supplied in @flags.  Note, this overwrites the volume information flags, so
 * make sure to combine the flags you want to modify with the old flags and use
 * the result when calling ntfs_volume_flags_write().
 *
 * Return 0 on success and errno on error.
 */
static errno_t ntfs_volume_flags_write(ntfs_volume *vol,
		const VOLUME_FLAGS flags)
{
	ntfs_inode *ni;
	MFT_RECORD *m;
	VOLUME_INFORMATION *vi;
	ntfs_attr_search_ctx *ctx;
	errno_t err;

	ntfs_debug("Entering, old flags = 0x%x, new flags = 0x%x.",
			le16_to_cpu(vol->vol_flags), le16_to_cpu(flags));
	if (vol->vol_flags == flags)
		goto done;
	ni = vol->vol_ni;
	if (!ni)
		panic("%s(): Volume inode is not loaded.\n", __FUNCTION__);
	err = ntfs_mft_record_map(ni, &m);
	if (err)
		goto err;
	ctx = ntfs_attr_search_ctx_get(ni, m);
	if (!ctx) {
		err = ENOMEM;
		goto put;
	}
	err = ntfs_attr_lookup(AT_VOLUME_INFORMATION, AT_UNNAMED, 0, 0, NULL,
			0, ctx);
	if (err)
		goto put;
	vi = (VOLUME_INFORMATION*)((u8*)ctx->a +
			le16_to_cpu(ctx->a->value_offset));
	vol->vol_flags = vi->flags = flags;
	/* Mark the mft record dirty to ensure it gets written out. */
	NInoSetMrecNeedsDirtying(ctx->ni);
	ntfs_attr_search_ctx_put(ctx);
	ntfs_mft_record_unmap(ni);
done:
	ntfs_debug("Done.");
	return 0;
put:
	if (ctx)
		ntfs_attr_search_ctx_put(ctx);
	ntfs_mft_record_unmap(ni);
err:
	ntfs_error(vol->mp, "Failed with error code %d.", err);
	return err;
}

/**
 * ntfs_volume_flags_set - set bits in the volume information flags
 * @vol:	ntfs volume on which to modify the flags
 * @flags:	flags to set on the volume
 *
 * Set the bits in @flags in the volume information flags on the volume @vol.
 *
 * Return 0 on success and errno on error.
 */
static inline errno_t ntfs_volume_flags_set(ntfs_volume *vol,
		VOLUME_FLAGS flags)
{
	flags &= VOLUME_FLAGS_MASK;
	return ntfs_volume_flags_write(vol, vol->vol_flags | flags);
}

/**
 * ntfs_volume_flags_clear - clear bits in the volume information flags
 * @vol:	ntfs volume on which to modify the flags
 * @flags:	flags to clear on the volume
 *
 * Clear the bits in @flags in the volume information flags on the volume @vol.
 *
 * Return 0 on success and errno on error.
 */
static inline errno_t ntfs_volume_flags_clear(ntfs_volume *vol,
		VOLUME_FLAGS flags)
{
	flags &= VOLUME_FLAGS_MASK;
	return ntfs_volume_flags_write(vol, vol->vol_flags & ~flags);
}

/**
 * ntfs_secure_load - load and setup the security file for a volume
 * @vol:	ntfs volume whose security file to load
 *
 * Return 0 on success and errno on error.
 */
static errno_t ntfs_secure_load(ntfs_volume *vol)
{
	ntfs_inode *ni;
	MFT_RECORD *m;
	ntfs_attr_search_ctx *ctx;
	FILENAME_ATTR *fn;
	errno_t err;
	static const ntfschar Secure[8] = { const_cpu_to_le16('$'),
			const_cpu_to_le16('S'), const_cpu_to_le16('e'),
			const_cpu_to_le16('c'), const_cpu_to_le16('u'),
			const_cpu_to_le16('r'), const_cpu_to_le16('e'), 0 };
	static ntfschar SDS[5] = { const_cpu_to_le16('$'),
			const_cpu_to_le16('S'), const_cpu_to_le16('D'),
			const_cpu_to_le16('S'), 0 };
	static ntfschar SDH[5] = { const_cpu_to_le16('$'),
			const_cpu_to_le16('S'), const_cpu_to_le16('D'),
			const_cpu_to_le16('H'), 0 };
	static ntfschar SII[5] = { const_cpu_to_le16('$'),
			const_cpu_to_le16('S'), const_cpu_to_le16('I'),
			const_cpu_to_le16('I'), 0 };

	ntfs_debug("Entering.");
	/* Get the security descriptors inode. */
	err = ntfs_inode_attach(vol, FILE_Secure, &ni, vol->root_ni->vn);
	if (err) {
		ntfs_error(vol->mp, "Failed to load $Secure.");
		return err;
	}
	vol->secure_ni = ni;
	/*
	 * Check this really is $Secure rather than $Quota remaining from a
	 * partially converted ntfs 1.x volume.
	 */
	err = ntfs_mft_record_map(ni, &m);
	if (err) {
		ntfs_error(vol->mp, "Failed to map mft record for $Secure.");
		return err;
	}
	if (!(m->flags & MFT_RECORD_IN_USE)) {
not_in_use:
		ntfs_debug("Done ($Secure is not in use).");
		ntfs_mft_record_unmap(ni);
		NVolSetUseSDAttr(vol);
		return 0;
	}
	ctx = ntfs_attr_search_ctx_get(ni, m);
	if (!ctx) {
		ntfs_error(vol->mp, "Failed to allocate search context for "
				"$Secure.");
		ntfs_mft_record_unmap(ni);
		return ENOMEM;
	}
	err = ntfs_attr_lookup(AT_FILENAME, AT_UNNAMED, 0, 0, NULL, 0, ctx);
	if (err) {
		ntfs_error(vol->mp, "Failed to look up filename attribute in "
				"$Secure (error %d).", err);
		ntfs_attr_search_ctx_put(ctx);
		ntfs_mft_record_unmap(ni);
		return err;
	}
	fn = (FILENAME_ATTR*)((u8*)ctx->a + le16_to_cpu(ctx->a->value_offset));
	if (!ntfs_are_names_equal(fn->filename, fn->filename_length,
			Secure, 7, NVolCaseSensitive(vol), NULL, 0)) {
		ntfs_attr_search_ctx_put(ctx);
		goto not_in_use;
	}
	ntfs_attr_search_ctx_put(ctx);
	ntfs_mft_record_unmap(ni);
	ntfs_debug("Verified identity of $Secure system file.");
	/* Get the $SDS data attribute. */
	err = ntfs_attr_inode_attach(vol->secure_ni, AT_DATA, SDS, 4,
			&vol->secure_sds_ni);
	if (err) {
		ntfs_error(vol->mp, "Failed to load $Secure/$SDS data "
				"attribute (error %d).", err);
		return err;
	}
	/* Get the $SDH index attribute. */
	err = ntfs_index_inode_attach(vol->secure_ni, SDH, 4,
			&vol->secure_sdh_ni);
	if (err) {
		ntfs_error(vol->mp, "Failed to load $Secure/$SDH index "
				"(error %d).", err); 
		return err;
	}
	/* Get the $SII index attribute. */
	err = ntfs_index_inode_attach(vol->secure_ni, SII, 4,
			&vol->secure_sii_ni);
	if (err) {
		ntfs_error(vol->mp, "Failed to load $Secure/$SII index "
				"(error %d).", err); 
		return err;
	}
	/*
	 * We need to find the highest security_id on the volume by finding the
	 * last entry in the $SII index and record it so we know which
	 * security_id to assign to the next security descriptor.
	 */
	err = ntfs_next_security_id_init(vol, &vol->next_security_id);
	if (err) {
		ntfs_error(vol->mp, "Failed to determine next security_id "
				"(error %d).", err);
		return err;
	}
	// TODO: Initialize security.
	// 
	// We need to look for our default security descriptors (for creating
	// directories and files) and if present record their security_ids and
	// set the appropriate flag on the volume.  If not present they will be
	// added when the first file/directory is created and the volume flag
	// will be set then.  (Do we need two flags, one for files and one for
	// directories?)
	// 
	// Set up our default security descriptors for files and directories
	// so they can be used when creating files/directories on volumes
	// without $Secure and in the case that we fail to add our security
	// descriptors to $Secure in which case we just place them in the
	// old-style security descriptor attribute and do NVolSetUseSDAttr().
	// FIXME: We then need to use old-style standard information attribute!
	//
	// For now just always force creation of security descriptor attributes.
	NVolSetUseSDAttr(vol);
	ntfs_debug("Done.");
	return 0;
}

/**
 * ntfs_objid_load - load and setup the object id file for a volume if present
 * @vol:	ntfs volume whose object id file to load
 *
 * Return 0 on success and errno on error.  If $ObjId is not present, we leave
 * vol->objid_ni as NULL and return success.
 */
static errno_t ntfs_objid_load(ntfs_volume *vol)
{
	MFT_REF mref;
	ntfs_inode *ni;
	ntfs_dir_lookup_name *name = NULL;
	int err;
	static const ntfschar ObjId[7] = { const_cpu_to_le16('$'),
			const_cpu_to_le16('O'), const_cpu_to_le16('b'),
			const_cpu_to_le16('j'), const_cpu_to_le16('I'),
			const_cpu_to_le16('d'), 0 };
	static ntfschar O[3] = { const_cpu_to_le16('$'),
			const_cpu_to_le16('O'), 0 };

	ntfs_debug("Entering.");
	/*
	 * Find the inode number for the object id file by looking up the
	 * filename $ObjId in the extended system files directory $Extend.
	 */
	lck_rw_lock_shared(&vol->extend_ni->lock);
	err = ntfs_lookup_inode_by_name(vol->extend_ni, ObjId, 6, &mref,
			&name);
	lck_rw_unlock_shared(&vol->extend_ni->lock);
	if (err) {
		/*
		 * If the file does not exist, there are no object ids in use
		 * on this volume, just return success.
		 */
		if (err == ENOENT) {
			ntfs_debug("$ObjId not present.  Volume does not have "
					"any object ids present.");
			return 0;
		}
		/* A real error occured. */
		ntfs_error(vol->mp, "Failed to find inode number for $ObjId.");
		return err;
	}
	/* We do not care for the type of match that was found. */
	if (name)
		OSFree(name, sizeof(*name), ntfs_malloc_tag);
	/* Get the inode. */
	err = ntfs_inode_attach(vol, MREF(mref), &ni, vol->extend_ni->vn);
	if (err) {
		ntfs_error(vol->mp, "Failed to load $ObjId.");
		return err;
	}
	vol->objid_ni = ni;
	/* Get the $O index inode. */
	err = ntfs_index_inode_attach(vol->objid_ni, O, 2, &vol->objid_o_ni);
	if (err) {
		ntfs_error(vol->mp, "Failed to load $ObjId/$O index (error "
				"%d).", err);
		return err;
	}
	ntfs_debug("Done.");
	return 0;
}

/**
 * ntfs_quota_load - load and setup the quota file for a volume if present
 * @vol:	ntfs volume whose quota file to load
 *
 * Return 0 on success and errno on error.  If $Quota is not present, we leave
 * vol->quota_ni as NULL and return success.
 */
static errno_t ntfs_quota_load(ntfs_volume *vol)
{
	MFT_REF mref;
	ntfs_dir_lookup_name *name = NULL;
	int err;
	static const ntfschar Quota[7] = { const_cpu_to_le16('$'),
			const_cpu_to_le16('Q'), const_cpu_to_le16('u'),
			const_cpu_to_le16('o'), const_cpu_to_le16('t'),
			const_cpu_to_le16('a'), 0 };
	static ntfschar Q[3] = { const_cpu_to_le16('$'),
			const_cpu_to_le16('Q'), 0 };

	ntfs_debug("Entering.");
	/*
	 * Find the inode number for the quota file by looking up the filename
	 * $Quota in the extended system files directory $Extend.
	 */
	lck_rw_lock_shared(&vol->extend_ni->lock);
	err = ntfs_lookup_inode_by_name(vol->extend_ni, Quota, 6, &mref,
			&name);
	lck_rw_unlock_shared(&vol->extend_ni->lock);
	if (err) {
		/*
		 * If the file does not exist, quotas are disabled and have
		 * never been enabled on this volume, just return success.
		 */
		if (err == ENOENT) {
			ntfs_debug("$Quota not present.  Volume does not have "
					"quotas enabled.");
			/*
			 * No need to try to set quotas out of date if they are
			 * not enabled.
			 */
			NVolSetQuotaOutOfDate(vol);
			return 0;
		}
		/* A real error occured. */
		ntfs_error(vol->mp, "Failed to find inode number for $Quota.");
		return err;
	}
	/* We do not care for the type of match that was found. */
	if (name)
		OSFree(name, sizeof(*name), ntfs_malloc_tag);
	/* Get the inode. */
	err = ntfs_inode_attach(vol, MREF(mref), &vol->quota_ni,
			vol->extend_ni->vn);
	if (err) {
		ntfs_error(vol->mp, "Failed to load $Quota.");
		return err;
	}
	/* Get the $Q index inode. */
	err = ntfs_index_inode_attach(vol->quota_ni, Q, 2, &vol->quota_q_ni);
	if (err) {
		ntfs_error(vol->mp, "Failed to load $Quota/$Q index (error "
				"%d).", err);
		return err;
	}
	ntfs_debug("Done.");
	return 0;
}

/**
 * ntfs_usnjrnl_load - load and setup the transaction log if present
 * @vol:	ntfs volume whose usnjrnl file to load
 *
 * Return 0 on success and errno on error.  $UsnJrnl is not present or in the
 * process of being disabled, we set NVolUsnJrnlStamped() and return success.
 *
 * If the $UsnJrnl $DATA/$J attribute has a size equal to the lowest valid usn,
 * i.e. transaction logging has only just been enabled or the journal has been
 * stamped and nothing has been logged since, we also set NVolUsnJrnlStamped()
 * and return success.
 */
static errno_t ntfs_usnjrnl_load(ntfs_volume *vol)
{
	s64 data_size;
	MFT_REF mref;
	ntfs_inode *ni, *max_ni;
	ntfs_dir_lookup_name *name = NULL;
	upl_t upl;
	upl_page_info_array_t pl;
	USN_HEADER *uh;
	errno_t err;
	static const ntfschar UsnJrnl[9] = { const_cpu_to_le16('$'),
			const_cpu_to_le16('U'), const_cpu_to_le16('s'),
			const_cpu_to_le16('n'), const_cpu_to_le16('J'),
			const_cpu_to_le16('r'), const_cpu_to_le16('n'),
			const_cpu_to_le16('l'), 0 };
	static ntfschar Max[5] = { const_cpu_to_le16('$'),
			const_cpu_to_le16('M'), const_cpu_to_le16('a'),
			const_cpu_to_le16('x'), 0 };
	static ntfschar J[3] = { const_cpu_to_le16('$'),
			const_cpu_to_le16('J'), 0 };

	ntfs_debug("Entering.");
	/*
	 * Find the inode number for the transaction log file by looking up the
	 * filename $UsnJrnl in the extended system files directory $Extend.
	 */
	lck_rw_lock_shared(&vol->extend_ni->lock);
	err = ntfs_lookup_inode_by_name(vol->extend_ni, UsnJrnl, 8, &mref,
			&name);
	lck_rw_unlock_shared(&vol->extend_ni->lock);
	if (err) {
		/*
		 * If the file does not exist, transaction logging is disabled,
		 * just return success.
		 */
		if (err == ENOENT) {
			ntfs_debug("$UsnJrnl not present.  Volume does not "
					"have transaction logging enabled.");
not_enabled:
			/*
			 * No need to try to stamp the transaction log if
			 * transaction logging is not enabled.
			 */
			NVolSetUsnJrnlStamped(vol);
			return 0;
		}
		/* A real error occured. */
		ntfs_error(vol->mp, "Failed to find inode number for "
				"$UsnJrnl.");
		return err;
	}
	/* We do not care for the type of match that was found. */
	if (name)
		OSFree(name, sizeof(*name), ntfs_malloc_tag);
	/* Get the inode. */
	err = ntfs_inode_attach(vol, MREF(mref), &ni, vol->extend_ni->vn);
	if (err) {
		ntfs_error(vol->mp, "Failed to load $UsnJrnl.");
		return err;
	}
	vol->usnjrnl_ni = ni;
	/*
	 * If the transaction log is in the process of being deleted, we can
	 * ignore it.
	 */
	if (vol->vol_flags & VOLUME_DELETE_USN_UNDERWAY) {
		ntfs_debug("$UsnJrnl in the process of being disabled.  "
				"Volume does not have transaction logging "
				"enabled.");
		goto not_enabled;
	}
	/* Get the $DATA/$Max attribute. */
	err = ntfs_attr_inode_attach(vol->usnjrnl_ni, AT_DATA, Max, 4, &max_ni);
	if (err) {
		ntfs_error(vol->mp, "Failed to load $UsnJrnl/$DATA/$Max "
				"attribute.");
		return err;
	}
	vol->usnjrnl_max_ni = max_ni;
	lck_spin_lock(&max_ni->size_lock);
	data_size = max_ni->data_size;
	lck_spin_unlock(&max_ni->size_lock);
	if (data_size < (s64)sizeof(USN_HEADER)) {
		ntfs_error(vol->mp, "Found corrupt $UsnJrnl/$DATA/$Max "
				"attribute (size is 0x%llx but should be at "
				"least 0x%x bytes).",
				(unsigned long long)data_size,
				(unsigned)sizeof(USN_HEADER));
		return EIO;
	}
	err = vnode_get(max_ni->vn);
	if (err) {
		ntfs_error(vol->mp, "Failed to get vnode for "
				"$UsnJrnl/$DATA/$Max.");
		return err;
	}
	lck_rw_lock_shared(&max_ni->lock);
	/* Read the USN_HEADER from $DATA/$Max. */
	err = ntfs_page_map(max_ni, 0, &upl, &pl, (u8**)&uh, FALSE);
	if (err) {
		ntfs_error(vol->mp, "Failed to read from $UsnJrnl/$DATA/$Max "
				"attribute.");
		goto put_err;
	}
	/* Sanity check $Max. */
	if (sle64_to_cpu(uh->allocation_delta) >
			sle64_to_cpu(uh->maximum_size)) {
		ntfs_error(vol->mp, "Allocation delta (0x%llx) exceeds "
				"maximum size (0x%llx).  $UsnJrnl is corrupt.",
				(unsigned long long)
				sle64_to_cpu(uh->allocation_delta),
				(unsigned long long)
				sle64_to_cpu(uh->maximum_size));
		err = EIO;
		goto unm_err;
	}
	/* Get the $DATA/$J attribute. */
	err = ntfs_attr_inode_attach(vol->usnjrnl_ni, AT_DATA, J, 2, &ni);
	if (err) {
		ntfs_error(vol->mp, "Failed to load $UsnJrnl/$DATA/$J "
				"attribute.");
		goto unm_err;
	}
	vol->usnjrnl_j_ni = ni;
	/* Verify $J is non-resident and sparse. */
	if (!NInoNonResident(ni) || !NInoSparse(ni)) {
		ntfs_error(vol->mp, "$UsnJrnl/$DATA/$J attribute is resident "
				"and/or not sparse.");
		err = EIO;
		goto unm_err;
	}
	/*
	 * If the transaction log has been stamped and nothing has been written
	 * to it since, we do not need to stamp it.
	 */
	lck_spin_lock(&ni->size_lock);
	data_size = ni->data_size;
	lck_spin_unlock(&ni->size_lock);
	if (sle64_to_cpu(uh->lowest_valid_usn) >= data_size) {
		if (sle64_to_cpu(uh->lowest_valid_usn) == data_size) {
			ntfs_page_unmap(max_ni, upl, pl, FALSE);
			lck_rw_unlock_shared(&max_ni->lock);
			(void)vnode_put(max_ni->vn);
			ntfs_debug("$UsnJrnl is enabled but nothing has been "
					"logged since it was last stamped.  "
					"Treating this as if the volume does "
					"not have transaction logging "
					"enabled.");
			goto not_enabled;
		}
		ntfs_error(vol->mp, "$UsnJrnl has lowest valid usn (0x%llx) "
				"which is out of bounds (0x%llx).  $UsnJrnl "
				"is corrupt.", (unsigned long long)
				sle64_to_cpu(uh->lowest_valid_usn),
				(unsigned long long)data_size);
		err = EIO;
		goto unm_err;
	}
	ntfs_debug("Done.");
unm_err:
	ntfs_page_unmap(max_ni, upl, pl, FALSE);
put_err:
	lck_rw_unlock_shared(&max_ni->lock);
	(void)vnode_put(max_ni->vn);
	return err;
}

/**
 * ntfs_system_inodes_get - load the system files at mount time
 * @vol:	ntfs volume being mounted
 *
 * Obtain the ntfs inodes corresponding to the system files and directories
 * needed for operation of a mounted ntfs file system and process their data
 * setting up any relevant in-memory structures in the process.
 *
 * It is assumed that ntfs_mft_inode_get() has already been called successfully
 * thus allowing us to simply use ntfs_inode_get(), ntfs_mft_record_map(), and
 * friends to do the work rather than having to do things by hand as is the
 * case when bootstrapping the volume in ntfs_mft_inode_get().
 *
 * Return 0 on success and errno on error.
 */
static errno_t ntfs_system_inodes_get(ntfs_volume *vol)
{
	s64 size;
	ntfs_inode *root_ni, *ni;
	vnode_t root_vn;
	errno_t err;
	BOOL is_hibernated;

	ntfs_debug("Entering.");
	/*
	 * Get the root directory inode so we can do path lookups and so we can
	 * supply its vnode as the parent vnode for the other system vnodes.
	 */
	err = ntfs_inode_attach(vol, FILE_root, &root_ni, NULL);
	if (err) {
		ntfs_error(vol->mp, "Failed to load root directory.");
		goto err;
	}
	vol->root_ni = root_ni;
	root_vn = root_ni->vn;
	/*
	 * We already have the $MFT inode and vnode.  Add the root directory
	 * vnode as the parent vnode.  We also take an internal reference on
	 * the root inode because vnode_update_identity() takes a reference on
	 * the root vnode.
	 */
	vnode_update_identity(vol->mft_ni->vn, root_vn, NULL, 0, 0,
			VNODE_UPDATE_PARENT);
	OSIncrementAtomic(&root_ni->nr_refs);
	/*
	 * Get mft mirror inode and compare the contents of $MFT and $MFTMirr,
	 * then deal with any errors.
	 */
	err = ntfs_mft_mirror_load(vol);
	if (!err)
		err = ntfs_mft_mirror_check(vol);
	if (err) {
		static const char es1a[] = "Failed to load $MFTMirr";
		static const char es1b[] = "$MFTMirr does not match $MFT";
		static const char es2[] = ".  Run ntfsfix and/or chkdsk.";
		const char *es1;

		es1 = !vol->mftmirr_ni ? es1a : es1b;
		/* If a read-write mount, convert it to a read-only mount. */
		if (!NVolReadOnly(vol)) {
			if (!(vol->on_errors & (ON_ERRORS_REMOUNT_RO |
					ON_ERRORS_CONTINUE))) {
				ntfs_error(vol->mp, "%s and neither on_errors="
						"continue nor on_errors="
						"remount-ro was specified%s",
						es1, es2);
				err = EIO;
				goto err;
			}
			vfs_setflags(vol->mp, MNT_RDONLY);
			NVolSetReadOnly(vol);
			ntfs_error(vol->mp, "%s.  Mounting read-only%s", es1,
					es2);
		} else
			ntfs_warning(vol->mp, "%s.  Will not be able to "
					"remount read-write%s", es1, es2);
		/* This will prevent a read-write remount. */
		NVolSetErrors(vol);
	}
	/*
	 * Get mft bitmap attribute inode and again, take an internal reference
	 * on the root inode to balance the reference taken on the root vnode
	 * in ntfs_attr_inode_get() and also take a reference on the vnode as
	 * we will be holding onto it for the duration of the mount.  Finally,
	 * we also release the iocount reference.  It will be taken as and when
	 * required when accessing the $MFT/$BITMAP attribute.
	 */
	err = ntfs_attr_inode_attach(vol->mft_ni, AT_BITMAP, NULL, 0,
			&vol->mftbmp_ni);
	if (err) {
		ntfs_error(vol->mp, "Failed to load $MFT/$BITMAP attribute.");
		goto err;
	}
	NInoSetSparseDisabled(vol->mftbmp_ni);
	/*
	 * If the mft bitmap attribute is non-resident (which it must be), read
	 * in the complete runlist.  This simplifies things when we need to
	 * allocate mft records as it guarantees that accessing the mft bitmap
	 * will not cause any of its mft records to be mapped.
	 */
	err = ntfs_attr_map_runlist(vol->mftbmp_ni);
	if (err) {
		ntfs_error(vol->mp, "Failed to map runlist of $MFT/$BITMAP "
				"attribute.");
		goto err;
	}
	/* Read upcase table and setup @vol->upcase and @vol->upcase_len. */
	err = ntfs_upcase_load(vol);
	if (err)
		goto err;
	/*
	 * Read attribute definitions table and setup @vol->attrdef and
	 * @vol->attrdef_size.
	 */
	err = ntfs_attrdef_load(vol);
	if (err)
		goto err;
	/* Get the cluster allocation bitmap inode and verify the size. */
	err = ntfs_inode_attach(vol, FILE_Bitmap, &ni, root_vn);
	if (err) {
		ntfs_error(vol->mp, "Failed to load $Bitmap.");
		goto err;
	}
	NInoSetSparseDisabled(ni);
	vol->lcnbmp_ni = ni;
	lck_spin_lock(&ni->size_lock);
	size = ni->data_size;
	lck_spin_unlock(&ni->size_lock);
	if ((vol->nr_clusters + 7) >> 3 > size) {
		ntfs_error(vol->mp, "$Bitmap (%lld) is shorter than required "
				"length of volume (%lld) as specified in the "
				"boot sector.  Run chkdsk.", (long long)size,
				(long long)(vol->nr_clusters + 7) >> 3);
		err = EIO;
		goto err;
	}
	/*
	 * If the cluster bitmap data attribute is non-resident, read in the
	 * complete runlist.  This simplifies things when we need to allocate
	 * mft records as it guarantees that accessing the cluster bitmap will
	 * not cause any of its mft records to be mapped.
	 */
	err = ntfs_attr_map_runlist(ni);
	if (err) {
		ntfs_error(vol->mp, "Failed to map runlist of $Bitmap/$DATA "
				"attribute.");
		goto err;
	}
	/*
	 * Get the volume inode and setup our cache of the volume flags and
	 * version as well as of the volume name in decomposed utf-8.
	 */
	err = ntfs_volume_load(vol);
	if (err)
		goto err;
	printf("NTFS volume name %s, version %u.%u.\n", vol->name,
			(unsigned)vol->major_ver, (unsigned)vol->minor_ver);
	if (vol->major_ver < 3 && NVolSparseEnabled(vol)) {
		ntfs_warning(vol->mp, "Disabling sparse support due to NTFS "
				"volume version %u.%u (need at least "
				"version 3.0).", (unsigned)vol->major_ver,
				(unsigned)vol->minor_ver);
		NVolClearSparseEnabled(vol);
	}
	if (vol->vol_flags & VOLUME_IS_DIRTY) {
		ntfs_warning(vol->mp, "NTFS volume is dirty.  You should "
				"unmount it and run chkdsk.");
		NVolSetErrors(vol);
	}
	/* Make sure that no unsupported volume flags are set. */
	if (vol->vol_flags & VOLUME_MUST_MOUNT_RO_MASK) {
		static const char es1[] = "Volume has unsupported flags set";
		static const char es2[] = ".  To fix this problem boot into "
				"Windows, run chkdsk c: /f /v /x from the "
				"command prompt (replace c: with the drive "
				"letter of this volume), then reboot into Mac "
				"OS X and mount the volume again.";

		ntfs_warning(vol->mp, "Unsupported volume flags 0x%x "
				"encountered.",
				(unsigned)le16_to_cpu(vol->vol_flags));
		/* If a read-write mount, convert it to a read-only mount. */
		if (!NVolReadOnly(vol)) {
			if (!(vol->on_errors & (ON_ERRORS_REMOUNT_RO |
					ON_ERRORS_CONTINUE))) {
				ntfs_error(vol->mp, "%s and neither on_errors="
						"continue nor on_errors="
						"remount-ro was specified%s",
						es1, es2);
				err = EINVAL;
				goto err;
			}
			vfs_setflags(vol->mp, MNT_RDONLY);
			NVolSetReadOnly(vol);
			ntfs_error(vol->mp, "%s.  Mounting read-only%s", es1,
					es2);
		} else
			ntfs_warning(vol->mp, "%s.  Will not be able to "
					"remount read-write%s", es1, es2);
		/*
		 * Do not set NVolErrors() because ntfs_remount() re-checks the
		 * flags which we need to do in case any flags have changed.
		 */
	}
	/*
	 * Get the inode for the logfile, check it, and determine if the volume
	 * was shutdown cleanly, then deal with any errors.
	 */
	err = ntfs_inode_attach(vol, FILE_LogFile, &ni, root_vn);
	if (!err) {
		RESTART_PAGE_HEADER *rp;

		NInoSetSparseDisabled(ni);
		vol->logfile_ni = ni;
		err = ntfs_logfile_check(ni, &rp);
		if (!err) {
			if (!ntfs_logfile_is_clean(ni, rp))
				err = EINVAL;
			if (rp)
				OSFree(rp, le32_to_cpu(rp->system_page_size),
						ntfs_malloc_tag);
		}
	}
	if (err) {
		static const char es1a[] = "Failed to load $LogFile";
		static const char es1b[] = "$LogFile is not clean";
		static const char es2[] = ".  Mount in Windows.";
		const char *es1;

		es1 = !vol->logfile_ni ? es1a : es1b;
		/* If a read-write mount, convert it to a read-only mount. */
		if (!NVolReadOnly(vol)) {
			if (!(vol->on_errors & (ON_ERRORS_REMOUNT_RO |
					ON_ERRORS_CONTINUE))) {
				ntfs_error(vol->mp, "%s and neither on_errors="
						"continue nor on_errors="
						"remount-ro was specified%s",
						es1, es2);
				goto err;
			}
			vfs_setflags(vol->mp, MNT_RDONLY);
			NVolSetReadOnly(vol);
			ntfs_error(vol->mp, "%s.  Mounting read-only%s", es1,
					es2);
		} else
			ntfs_warning(vol->mp, "%s.  Will not be able to "
					"remount read-write%s", es1, es2);
		NVolSetErrors(vol);
	}
	/*
	 * Check if Windows is suspended to disk on the target volume.  If it
	 * is hibernated, we must not write *anything* to the disk so set
	 * NVolErrors() without setting the dirty volume flag and mount
	 * read-only.  This will prevent read-write remounting and it will also
	 * prevent all writes.
	 */
	err = ntfs_windows_hibernation_status_check(vol, &is_hibernated);
	if (err || is_hibernated) {
		static const char es1a[] = "Failed to determine if Windows is "
				"hibernated";
		static const char es1b[] = "Windows is hibernated";
		static const char es2[] = ".  Run chkdsk.";
		const char *es1;

		es1 = err ? es1a : es1b;
		/* If a read-write mount, convert it to a read-only mount. */
		if (!NVolReadOnly(vol)) {
			if (!(vol->on_errors & (ON_ERRORS_REMOUNT_RO |
					ON_ERRORS_CONTINUE))) {
				ntfs_error(vol->mp, "%s and neither on_errors="
						"continue nor on_errors="
						"remount-ro was specified%s",
						es1, es2);
				if (!err)
					err = EINVAL;
				goto err;
			}
			vfs_setflags(vol->mp, MNT_RDONLY);
			NVolSetReadOnly(vol);
			ntfs_error(vol->mp, "%s.  Mounting read-only%s", es1,
					es2);
		} else
			ntfs_warning(vol->mp, "%s.  Will not be able to "
					"remount read-write%s", es1, es2);
		NVolSetErrors(vol);
	}
	/* If (still) a read-write mount, mark the volume dirty. */
	if (!NVolReadOnly(vol) &&
			(err = ntfs_volume_flags_set(vol, VOLUME_IS_DIRTY))) {
		static const char es1[] = "Failed to set dirty bit in volume "
				"information flags";
		static const char es2[] = ".  Run chkdsk.";

		/* Convert to a read-only mount. */
		if (!(vol->on_errors & (ON_ERRORS_REMOUNT_RO |
				ON_ERRORS_CONTINUE))) {
			ntfs_error(vol->mp, "%s and neither on_errors="
					"continue nor on_errors=remount-ro "
					"was specified%s", es1, es2);
			goto err;
		}
		vfs_setflags(vol->mp, MNT_RDONLY);
		NVolSetReadOnly(vol);
		ntfs_error(vol->mp, "%s.  Mounting read-only%s", es1, es2);
		/*
		 * Do not set NVolErrors() because ntfs_remount() might manage
		 * to set the dirty flag in which case all would be well.
		 */
	}
	/* If (still) a read-write mount, empty the logfile. */
	if (!NVolReadOnly(vol) &&
			(err = ntfs_logfile_empty(vol->logfile_ni))) {
		static const char es1[] = "Failed to empty journal $LogFile";
		static const char es2[] = ".  Mount in Windows.";

		/* Convert to a read-only mount. */
		if (!(vol->on_errors & (ON_ERRORS_REMOUNT_RO |
				ON_ERRORS_CONTINUE))) {
			ntfs_error(vol->mp, "%s and neither on_errors="
					"continue nor on_errors=remount-ro "
					"was specified%s", es1, es2);
			goto err;
		}
		vfs_setflags(vol->mp, MNT_RDONLY);
		NVolSetReadOnly(vol);
		ntfs_error(vol->mp, "%s.  Mounting read-only%s", es1, es2);
		NVolSetErrors(vol);
	}
	/* If the ntfs volume version is below 3.0, we are done. */
	if (vol->major_ver < 3) {
		/*
		 * Set NVolUseSDAttr() so we do not need to check both the
		 * volume version and NVolUseSDAttr() when creating inodes.
		 */
		NVolSetUseSDAttr(vol);
		ntfs_debug("Done (NTFS version < 3.0).");
		return 0;
	}
	/* Ntfs 3.0+ specific initialization. */
	/*
	 * Read the security descriptors file and initialize security on the
	 * volume.
	 */
	err = ntfs_secure_load(vol);
	if (err)
		goto err;
	/* Get the extended system files directory inode. */
	err = ntfs_inode_attach(vol, FILE_Extend, &vol->extend_ni, root_vn);
	if (err) {
		ntfs_error(vol->mp, "Failed to load $Extend directory.");
		goto err;
	}
	/* Find the object id file, load it if present, and set it up. */
	err = ntfs_objid_load(vol);
	if (err) {
		static const char es1[] = "Failed to load $ObjId";
		static const char es2[] = ".  Run chkdsk.";

		/* If a read-write mount, convert it to a read-only mount. */
		if (!NVolReadOnly(vol)) {
			if (!(vol->on_errors & (ON_ERRORS_REMOUNT_RO |
					ON_ERRORS_CONTINUE))) {
				ntfs_error(vol->mp, "%s and neither on_errors="
						"continue nor on_errors="
						"remount-ro was specified%s",
						es1, es2);
				goto err;
			}
			vfs_setflags(vol->mp, MNT_RDONLY);
			NVolSetReadOnly(vol);
			ntfs_error(vol->mp, "%s.  Mounting read-only%s", es1,
					es2);
		} else
			ntfs_warning(vol->mp, "%s.  Will not be able to "
					"remount read-write%s", es1, es2);
		NVolSetErrors(vol);
	}
	/* Find the quota file, load it if present, and set it up. */
	err = ntfs_quota_load(vol);
	if (err) {
		static const char es1[] = "Failed to load $Quota";
		static const char es2[] = ".  Run chkdsk.";

		/* If a read-write mount, convert it to a read-only mount. */
		if (!NVolReadOnly(vol)) {
			if (!(vol->on_errors & (ON_ERRORS_REMOUNT_RO |
					ON_ERRORS_CONTINUE))) {
				ntfs_error(vol->mp, "%s and neither on_errors="
						"continue nor on_errors="
						"remount-ro was specified%s",
						es1, es2);
				goto err;
			}
			vfs_setflags(vol->mp, MNT_RDONLY);
			NVolSetReadOnly(vol);
			ntfs_error(vol->mp, "%s.  Mounting read-only%s", es1,
					es2);
		} else
			ntfs_warning(vol->mp, "%s.  Will not be able to "
					"remount read-write%s", es1, es2);
		NVolSetErrors(vol);
	}
	/* If (still) a read-write mount, mark the quotas out of date. */
	if (!NVolReadOnly(vol) && (err = ntfs_quotas_mark_out_of_date(vol))) {
		static const char es1[] = "Failed to mark quotas out of date";
		static const char es2[] = ".  Run chkdsk.";

		/* Convert to a read-only mount. */
		if (!(vol->on_errors & (ON_ERRORS_REMOUNT_RO |
				ON_ERRORS_CONTINUE))) {
			ntfs_error(vol->mp, "%s and neither on_errors="
					"continue nor on_errors=remount-ro "
					"was specified%s", es1, es2);
			goto err;
		}
		vfs_setflags(vol->mp, MNT_RDONLY);
		NVolSetReadOnly(vol);
		ntfs_error(vol->mp, "%s.  Mounting read-only%s", es1, es2);
		NVolSetErrors(vol);
	}
	/*
	 * Find the transaction log file ($UsnJrnl), load it if present, check
	 * it, and set it up.
	 */
	err = ntfs_usnjrnl_load(vol);
	if (err) {
		static const char es1[] = "Failed to load $UsnJrnl";
		static const char es2[] = ".  Run chkdsk.";

		/* If a read-write mount, convert it to a read-only mount. */
		if (!NVolReadOnly(vol)) {
			if (!(vol->on_errors & (ON_ERRORS_REMOUNT_RO |
					ON_ERRORS_CONTINUE))) {
				ntfs_error(vol->mp, "%s and neither on_errors="
						"continue nor on_errors="
						"remount-ro was specified%s",
						es1, es2);
				goto err;
			}
			vfs_setflags(vol->mp, MNT_RDONLY);
			NVolSetReadOnly(vol);
			ntfs_error(vol->mp, "%s.  Mounting read-only%s", es1,
					es2);
		} else
			ntfs_warning(vol->mp, "%s.  Will not be able to "
					"remount read-write%s", es1, es2);
		NVolSetErrors(vol);
	}
	/* If (still) a read-write mount, stamp the transaction log. */
	if (!NVolReadOnly(vol) && (err = ntfs_usnjrnl_stamp(vol))) {
		static const char es1[] = "Failed to stamp transaction log "
				"($UsnJrnl)";
		static const char es2[] = ".  Run chkdsk.";

		/* Convert to a read-only mount. */
		if (!(vol->on_errors & (ON_ERRORS_REMOUNT_RO |
				ON_ERRORS_CONTINUE))) {
			ntfs_error(vol->mp, "%s and neither on_errors="
					"continue nor on_errors=remount-ro "
					"was specified%s", es1, es2);
			goto err;
		}
		vfs_setflags(vol->mp, MNT_RDONLY);
		NVolSetReadOnly(vol);
		ntfs_error(vol->mp, "%s.  Mounting read-only%s", es1, es2);
		NVolSetErrors(vol);
	}
	ntfs_debug("Done (NTFS version >= 3.0).");
	return 0;
err:
	/* Obtained inodes will be released by the call to ntfs_unmount(). */
	return err;
}

/**
 * ntfs_popcount32 - count the number of set bits in a 32-bit word
 * @v:		32-bit value whose set bits to count
 *
 * Count the number of set bits in the 32-bit word @v.  This should be the most
 * efficient C algorithm.  Implementation is as described in Chapter 8, Section
 * 6, "Efficient Implementation of Population-Count Function in 32-Bit Mode",
 * pages 179-180 of the "Software Optimization Guide for AMD64 Processors":
 * http://www.amd.com/us-en/assets/content_type/white_papers_and_tech_docs/25112.PDF
 *
 * TODO: Does xnu really not have asm optimized version of the popcount (aka
 * bitcount) function?  My searches have failed to find one...  If it exists or
 * gets added at some point we should switch to using it instead of ours.
 */
static inline u32 ntfs_popcount32(u32 v)
{
	const u32 w = v - ((v >> 1) & 0x55555555);
	const u32 x = (w & 0x33333333) + ((w >> 2) & 0x33333333);
	return (((x + (x >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

/**
 * ntfs_get_nr_set_bits - get the number of set bits in a bitmap
 * @vn:		vnode of bitmap for which to get the number of set bits
 * @nr_bits:	number of bits in the bitmap
 * @res:	pointer to where the result should be written
 *
 * Calculate the number of set bits in the bitmap vnode @vn and return the
 * result in @res.  We do not care about partial buffers as these will be just
 * zero filled and hence not be counted as set bits.
 *
 * If any buffers cannot be read we assume all bits in the erroring buffers are
 * set.  This means we return an overestimate on errors which is better than
 * an underestimate.
 *
 * Return 0 on success amd errno if an iocount reference could not be obtained
 * on the bitmap vnode.
 */
static errno_t ntfs_get_nr_set_bits(vnode_t vn, const s64 nr_bits, s64 *res)
{
	s64 max_ofs, ofs, nr_set;
	ntfs_inode *ni = NTFS_I(vn);
	errno_t err;

	ntfs_debug("Entering.");
	/* Get an iocount reference on the bitmap vnode. */
	err = vnode_get(vn);
	if (err)
		return err;
	lck_rw_lock_shared(&ni->lock);
	/* Convert the number of bits into bytes rounded up. */
	max_ofs = (nr_bits + 7) >> 3;
	ntfs_debug("Reading bitmap, max_ofs %lld.", (long long)max_ofs);
	for (nr_set = ofs = 0; ofs < max_ofs; ofs += PAGE_SIZE) {
		upl_t upl;
		upl_page_info_array_t pl;
		u32 *p;
		int i;

		/* Map the page. */
		err = ntfs_page_map(ni, ofs, &upl, &pl, (u8**)&p, FALSE);
		if (err) {
			ntfs_debug("Failed to map page from bitmap (offset "
					"%lld, size %d, error %d).  Skipping "
					"page.", (long long)ofs, PAGE_SIZE,
					(int)err);
			/* Count the whole buffer contents as set bits. */
			nr_set += PAGE_SIZE * 8;
			continue;
		}
		/*
		 * For each 32-bit word, add the number of set bits.  If this
		 * is the last block and it is partial we do not really care as
		 * it just means we do a little extra work but it will not
		 * affect the result as all out of range bytes are set to zero
		 * by ntfs_page_map().
		 *
		 * Use multiples of 4 bytes, thus max size is PAGE_SIZE / 4.
		 */
	  	for (i = 0; i < (PAGE_SIZE / 4); i++)
			nr_set += ntfs_popcount32(p[i]);
		ntfs_page_unmap(ni, upl, pl, FALSE);
	}
	/*
	 * Release the iocount reference on the bitmap vnode.  We can ignore
	 * the return value as it always is zero.
	 */
	lck_rw_unlock_shared(&ni->lock);
	(void)vnode_put(vn);
	ntfs_debug("Done (nr_bits %lld, nr_set %lld).", (long long)nr_bits,
			(long long)nr_set);
	*res = nr_set;
	return 0;
}

/**
 * ntfs_set_nr_free_clusters - set the number of free clusters on a volume
 * @vol:	ntfs volume for which to set the number of free clusters
 *
 * Calculate the number of free clusters on the mounted ntfs volume @vol and
 * cache the result in the @vol->nr_free_clusters.
 *
 * The only particularity is that clusters beyond the end of the logical ntfs
 * volume will be marked as in use to prevent errors which means we have to
 * discount those at the end.  This is important as the cluster bitmap always
 * has a size in multiples of 8 bytes, i.e. up to 63 clusters could be outside
 * the logical volume and marked in use when they are not as they do not exist.
 *
 * If any part of the bitmap cannot be read we assume all clusters in the
 * erroring part(s) are in use.  This means we return an underestimate of the
 * number of free clusters on errors which is better than an overrestimate.
 *
 * Return 0 on success or errno if an iocount reference could not be obtained
 * on the $Bitmap vnode.
 */
static errno_t ntfs_set_nr_free_clusters(ntfs_volume *vol)
{
	s64 nr_free;
	errno_t err;

	ntfs_debug("Entering.");
	lck_rw_lock_exclusive(&vol->lcnbmp_lock);
	err = ntfs_get_nr_set_bits(vol->lcnbmp_ni->vn, vol->nr_clusters,
			&nr_free);
	if (err) {
		ntfs_error(vol->mp, "Failed to get vnode for $Bitmap.");
		lck_rw_unlock_exclusive(&vol->lcnbmp_lock);
		return err;
	}
	/* Determine the number of zero bits from the number of set bits. */
	nr_free = vol->nr_clusters - nr_free;
	/*
	 * Fixup for eventual bits outside logical ntfs volume (see function
	 * description above).
	 */
	if (vol->nr_clusters & 63)
		nr_free += 64 - (vol->nr_clusters & 63);
	/* If errors occured we may have gone below zero, fix this. */
	if (nr_free < 0)
		nr_free = 0;
	vol->nr_free_clusters = nr_free;
	ntfs_debug("Done (nr_clusters %lld, nr_free_clusters %lld).",
			(long long)vol->nr_clusters, (long long)nr_free);
	lck_rw_unlock_exclusive(&vol->lcnbmp_lock);
	return 0;
}

/**
 * ntfs_set_nr_mft_records - set the number of total/free mft records
 * @vol:	volume for which to set the number of total/free mft records
 *
 * Calculate the number of mft records (inodes) as well as the number of free
 * mft records on the mounted ntfs volume @vol and cache the results in
 * @vol->nr_mft_records and @vol->nr_free_mft_records, respectively.
 *
 * If any part of the bitmap cannot be read we assume all mft records in the
 * erroring part(s) are in use.  This means we return an underestimate of the
 * number of free mft records on errors which is better than an overrestimate.
 *
 * FIXME: HFS uses the maximum ever possible by basing it on the volume size
 * rather than the current total/free.  Do we want to keep it the ntfsprogs and
 * Linux NTFS driver way or move to the HFS way?
 */
static errno_t ntfs_set_nr_mft_records(ntfs_volume *vol)
{
	s64 nr_free;
	errno_t err;

	ntfs_debug("Entering.");
	/*
	 * First, determine the total number of mft records from the size of
	 * the $MFT/$DATA attribute.
	 */
	lck_rw_lock_exclusive(&vol->mftbmp_lock);
	lck_spin_lock(&vol->mft_ni->size_lock);
	vol->nr_mft_records = vol->mft_ni->data_size >>
			vol->mft_record_size_shift;
	lck_spin_unlock(&vol->mft_ni->size_lock);
	err = ntfs_get_nr_set_bits(vol->mftbmp_ni->vn,
			vol->mft_ni->initialized_size >>
			vol->mft_record_size_shift, &nr_free);
	if (err) {
		ntfs_error(vol->mp, "Failed to get vnode for $MFT/$BITMAP.");
		lck_rw_unlock_exclusive(&vol->mftbmp_lock);
		return err;
	}
	/* Determine the number of zero bits from the number of set bits. */
	nr_free = vol->nr_mft_records - nr_free;
	/* If errors occured we may well have gone below zero, fix this. */
	if (nr_free < 0)
		nr_free = 0;
	vol->nr_free_mft_records = nr_free;
	ntfs_debug("Done (nr_mft_records %lld, nr_free_mft_records %lld).",
			(long long)vol->nr_mft_records, (long long)nr_free);
	lck_rw_unlock_exclusive(&vol->mftbmp_lock);
	return 0;
}

/**
 * ntfs_statfs - return information about a mounted ntfs volume
 * @vol:	ntfs volume about which to return information
 * @sfs:	vfsstatfs structure in which to return the information
 *
 * Return information about the mounted ntfs volume @vol in the vfsstatfs
 * structure @sfs.  We interpret the values to be correct of the moment in time
 * at which we are called.  Most values are variable otherwise and this is not
 * just the free values but the totals as well.  For example we can increase
 * the total number of file nodes if we run out and we can keep doing this
 * until there is no more space on the volume left at all.
 *
 * This is only called from ntfs_mount() hence we only need to set the
 * fields that are not already set.
 *
 * The mount() system call sets @sfs to zero and then sets up f_owner, f_flags,
 * f_fstypename, f_mntonname, f_mntfromname, and f_reserved.
 *
 * ntfs_mount() then sets f_fsid and calls ntfs_statfs() and the rest of @sfs
 * is set here.
 *
 * Note: No need for locking as this is only called from ntfs_mount().
 */
static void ntfs_statfs(ntfs_volume *vol, struct vfsstatfs *sfs)
{
	ntfs_debug("Entering.");
	/*
	 * Block size for the below size values.  We use the cluster size of
	 * the volume as that means we do not convert to a different unit.
	 * Alternatively, we could return the sector size instead.
	 */
	sfs->f_bsize = vol->cluster_size;
	/* Optimal transfer block size (in bytes). */
	sfs->f_iosize = ubc_upl_maxbufsize();
	/* Total data blocks in file system (in units of @f_bsize). */
	sfs->f_blocks = (u64)vol->nr_clusters;
	/* Free data blocks in file system (in units of @f_bsize). */
	sfs->f_bfree = (u64)vol->nr_free_clusters;
	/*
	 * Free blocks available to non-superuser (in units of @f_bsize), same
	 * as above for ntfs.
	 * FIXME: We could provide a mount option to cause a virtual, reserved
	 * percentage of total space for superuser and perhaps even use a
	 * non-zero default and enforce it in the cluster allocator.  If we do
	 * that we would need to subtract that percentage from
	 * @vol->nr_free_clusters and return the result in @sfs->f_bavail
	 * unless the result is below zero in which case we would just set
	 * @sfs->f_bavail to 0.
	 */ 
	sfs->f_bavail = (u64)vol->nr_free_clusters;
	/* Blocks in use (in units of @f_bsize). */
	sfs->f_bused = (u64)(vol->nr_clusters - vol->nr_free_clusters);
	/* Number of inodes in file system (at this point in time). */
	sfs->f_files = (u64)vol->nr_mft_records;
	/* Free inodes in file system (at this point in time). */
	sfs->f_ffree = (u64)vol->nr_free_mft_records;
	/*
	 * File system subtype.  Set this to the ntfs version encoded into 16
	 * bits, the high 8 bits being the major version and the low 8 bits
	 * being the minor version.  This is then extended to 32 bits, thus the
	 * higher 16 bits are currently zero.
	 */
	sfs->f_fssubtype = (u32)vol->major_ver << 8 | vol->minor_ver;
	ntfs_debug("Done.");
}

/**
 * ntfs_unmount_callback_recycle - callback for vnode iterate in ntfs_unmount()
 * @vn:		vnode the callback is invoked with (has iocount reference)
 * @data:	for us always NULL and ignored
 *
 * This callback is called from vnode_iterate() which is called from
 * ntfs_unmount() for all in-core, non-dead, non-suspend vnodes belonging to
 * the mounted volume that still have an ntfs inode attached.
 *
 * We mark all vnodes for termination so they are reclaimed as soon as all
 * references to them are released.
 */
static int ntfs_unmount_callback_recycle(vnode_t vn, void *data __unused)
{
#ifdef DEBUG
	if (NTFS_I(vn))
		ntfs_debug("Entering for mft_no 0x%llx.",
				(unsigned long long)NTFS_I(vn)->mft_no);
#endif
	(void)vnode_recycle(vn);
	ntfs_debug("Done.");
	return VNODE_RETURNED;
}

/**
 * ntfs_unmount_inode_detach - detach an inode at umount time
 * @pni:	pointer to the attached ntfs inode to detach
 * @parent_ni:	parent ntfs inode
 *
 * Mark the vnode of the ntfs inode *@pni for termination and detach the ntfs
 * inode *@pni from the mounted ntfs volume @vol by dropping the reference on
 * its vnode and setting *@pni to NULL.
 */
static void ntfs_unmount_inode_detach(ntfs_inode **pni, ntfs_inode *parent_ni)
{
	ntfs_inode *ni = *pni;
	if (ni) {
		ntfs_debug("Entering for mft_no 0x%llx.",
				(unsigned long long)ni->mft_no);
		/* Drop the internal reference on the parent inode. */
		if (parent_ni)
			OSDecrementAtomic(&parent_ni->nr_refs);
		OSDecrementAtomic(&ni->nr_refs);
		if (ni->vn) {
			(void)vnode_recycle(ni->vn);
			vnode_rele(ni->vn);
		} else
			ntfs_inode_reclaim(ni);
		*pni = NULL;
		ntfs_debug("Done.");
	}
}

/**
 * ntfs_unmount_attr_inode_detach - detach an attribute inode at umount time
 * @pni:	pointer to the attached ntfs inode to detach
 *
 * Mark the vnode of the ntfs inode *@pni for termination and detach the ntfs
 * inode *@pni from the mounted ntfs volume @vol by dropping the reference on
 * its vnode and setting *@pni to NULL.
 */
static void ntfs_unmount_attr_inode_detach(ntfs_inode **pni)
{
	ntfs_inode *ni = *pni;
	if (ni) {
		ntfs_debug("Entering for mft_no 0x%llx.",
				(unsigned long long)ni->mft_no);
		/*
		 * Drop the internal reference on the base inode @base_ni
		 * (which is also the parent inode).
		 */
		if (NInoAttr(ni) && ni->base_ni)
			OSDecrementAtomic(&ni->base_ni->nr_refs);
		OSDecrementAtomic(&ni->nr_refs);
		if (ni->vn) {
			(void)vnode_recycle(ni->vn);
			vnode_rele(ni->vn);
		} else
			ntfs_inode_reclaim(ni);
		*pni = NULL;
		ntfs_debug("Done.");
	}
}

/**
 * ntfs_unmount - unmount an ntfs file system
 * @mp:		mount point to unmount
 * @mnt_flags:	flags describing the unmount (MNT_FORCE is the only one)
 * @context:	vfs context
 *
 * The VFS calls this via VFS_UNMOUNT() when it wants to unmount an ntfs
 * volume.  We sync and release all held inodes as well as all other resources.
 *
 * For each held inode, if we have the vnode already, go through vfs reclaim
 * which will also get rid off the ntfs inode.  Otherwise kill the ntfs inode
 * directly.
 *
 * If the volume is successfully unmounted, we must call
 * OSKextReleaseKextWithLoadTag() to allow the KEXT to be unloaded when no
 * longer in use.
 *
 * Return 0 on success and errno on error.
 */
static int ntfs_unmount(mount_t mp, int mnt_flags,
		vfs_context_t context __unused)
{
	ntfs_volume *vol;
	int vflags, err;
	BOOL force;

	ntfs_debug("Entering.");
	vol = NTFS_MP(mp);
	if (!vol)
		goto unload;
	if (!vol->mft_ni) {
		/* Split our ntfs_volume away from the mount. */
		vfs_setfsprivate(mp, NULL);
		goto no_mft;
	}
	vflags = 0;
	force = FALSE;
	if (mnt_flags & MNT_FORCE) {
		vflags |= FORCECLOSE;
		force = TRUE;
	}
	if (!vol->root_ni)
		goto no_root;
	/*
	 * Try to reclaim all non-root and non-system vnodes.  For a non-forced
	 * unmount, this will fail if there are any open files.
	 */
	err = vflush(mp, NULLVP, vflags|SKIPROOT|SKIPSYSTEM);
	if (err) {
		ntfs_warning(mp, "Cannot unmount (vflush() returned error "
				"%d).  Are there open files keeping the "
				"volume busy?\n", err);
		goto abort;
	}
	/*
	 * Once we get here, the only vnodes left are our system vnodes, which
	 * we will detach and vnode_put below.  At this point, the system
	 * directories may still have index attributes with references on the
	 * directory vnodes.  And we might have other system vnodes still
	 * hanging around, with no references.  So we will explicitly try to
	 * recycle all remaining vnodes so that they will all be reclaimed as
	 * soon as their last references are dropped.
	 */
	(void)vnode_iterate(mp, 0, ntfs_unmount_callback_recycle, NULL);
	/*
	 * If a read-write mount and no volume errors have been detected, mark
	 * the volume clean.
	 */
	if (!NVolReadOnly(vol) && vol->vol_ni) {
		if (!NVolErrors(vol)) {
			if (ntfs_volume_flags_clear(vol, VOLUME_IS_DIRTY))
				ntfs_warning(mp, "Failed to clear dirty bit "
						"in volume information "
						"flags.  Run chkdsk.");
		} else
			ntfs_warning(mp, "Volume has errors.  Leaving volume "
					"marked dirty.  Run chkdsk.");
	}
	/* Ntfs 3.0+ specific clean up. */
	if (vol->vol_ni && vol->major_ver >= 3) {
		ntfs_unmount_attr_inode_detach(&vol->usnjrnl_j_ni);
		ntfs_unmount_attr_inode_detach(&vol->usnjrnl_max_ni);
		ntfs_unmount_inode_detach(&vol->usnjrnl_ni, vol->extend_ni);
		ntfs_unmount_attr_inode_detach(&vol->quota_q_ni);
		ntfs_unmount_inode_detach(&vol->quota_ni, vol->extend_ni);
		ntfs_unmount_attr_inode_detach(&vol->objid_o_ni);
		ntfs_unmount_inode_detach(&vol->objid_ni, vol->extend_ni);
		ntfs_unmount_inode_detach(&vol->extend_ni, vol->root_ni);
		ntfs_unmount_attr_inode_detach(&vol->secure_sds_ni);
		ntfs_unmount_attr_inode_detach(&vol->secure_sdh_ni);
		ntfs_unmount_attr_inode_detach(&vol->secure_sii_ni);
		ntfs_unmount_inode_detach(&vol->secure_ni, vol->root_ni);
	}
	ntfs_unmount_inode_detach(&vol->vol_ni, vol->root_ni);
	ntfs_unmount_inode_detach(&vol->lcnbmp_ni, vol->root_ni);
	ntfs_unmount_attr_inode_detach(&vol->mftbmp_ni);
	ntfs_unmount_inode_detach(&vol->logfile_ni, vol->root_ni);
	/*
	 * The root directory vnode is still held by the parent vnode
	 * references of the $MFT and $MFTMirr vnodes thus it will only be
	 * inactivated after those vnodes are reclaimed.  The problem with this
	 * is that when VNOP_INACTIVE() is called for the root directory vnode
	 * this in turn calls ntfs_inode_sync() which in turn calls
	 * ntfs_mft_record_sync() which in turn calls buf_getblk() followed by
	 * buf_bwrite() for the vnode of $MFT which fails as the vnode for $MFT
	 * has been reclaimed already.  The solution is thus to drop the parent
	 * vnode references held by $MFT and $MFTMirr now so that the root
	 * directory vnode can be recycled now.
	 */
	if (vol->mftmirr_ni && vol->mftmirr_ni->vn) {
		/* Drop the internal reference on the parent inode. */
		if (vol->root_ni)
			OSDecrementAtomic(&vol->root_ni->nr_refs);
		vnode_update_identity(vol->mftmirr_ni->vn, NULL, NULL, 0, 0,
				VNODE_UPDATE_PARENT);
	}
	if (vol->mft_ni && vol->mft_ni->vn) {
		/* Drop the internal reference on the parent inode. */
		if (vol->root_ni)
			OSDecrementAtomic(&vol->root_ni->nr_refs);
		vnode_update_identity(vol->mft_ni->vn, NULL, NULL, 0, 0,
				VNODE_UPDATE_PARENT);
	}
	/*
	 * Nothing references the root inode any more so we can release it.
	 * Note the VFS still holds a reference that it will drop after
	 * ntfs_unmount() completes thus the root vnode will be the last one to
	 * be reclaimed.
	 */
	ntfs_unmount_inode_detach(&vol->root_ni, NULL);
	/*
	 * Do a final flush to get rid of any vnodes that have not been
	 * inactivated/recycled yet.  Note this must be done without the force
	 * flag otherwise it blows away the mft mirror and mft inodes which we
	 * will recycle below.
	 */
	(void)vflush(mp, NULLVP, vflags & ~FORCECLOSE);
	ntfs_unmount_inode_detach(&vol->mftmirr_ni, NULL);
no_root:
	if (vol->mft_ni) {
		if (vol->mft_ni->vn)
			ntfs_unmount_inode_detach(&vol->mft_ni, NULL);
		else {
			/*
			 * There may be no vnode in the error code paths of
			 * ntfs_mount() which calls ntfs_unmount() to clean up.
			 */
			ntfs_inode_reclaim(vol->mft_ni);
			vol->mft_ni = NULL;
		}
	}
	/*
	 * We are holding no inodes at all now.  It is time to blow everything
	 * away that is remaining.  If this is a forced unmount, we immediately
	 * and forcibly blow everything away.  If not forced, we try to blow
	 * everything away that is not busy but if anything is busy vflush()
	 * does not do anything at all.  In that case we report an error, and
	 * then forcibly blow everything away anyway.  FIXME: We could undo the
	 * unmount by re-reading all the system inodes we just released, but do
	 * we want to?  It does not seem to be worth the hassle given it should
	 * never really happen...
	 */
	err = vflush(mp, NULLVP, vflags);
	if (err && !force) {
		ntfs_error(mp, "There are busy vnodes after unmounting!  "
				"Forcibly closing and reclaiming them.");
		(void)vflush(mp, NULLVP, FORCECLOSE);

	}
	/* Split our ntfs_volume away from the mount. */
	vfs_setfsprivate(mp, NULL);
	lck_mtx_lock(&ntfs_lock);
	if (vol->upcase && vol->upcase == ntfs_default_upcase) {
		vol->upcase = NULL;
		/*
		 * Drop our reference on the default upcase table and throw it
		 * away if we had the only reference.
		 */
		if (!--ntfs_default_upcase_users) {
			OSFree(ntfs_default_upcase, ntfs_default_upcase_size,
					ntfs_malloc_tag);
			ntfs_default_upcase = NULL;
		}
	}
	if (NVolCompressionEnabled(vol)) {
		/*
		 * Drop our reference on the compression buffer and throw it
		 * away if we had the only reference.
		 */
		if (!--ntfs_compression_users) {
			OSFree(ntfs_compression_buffer,
					ntfs_compression_buffer_size,
					ntfs_malloc_tag);
			ntfs_compression_buffer = NULL;
		}
	}
	lck_mtx_unlock(&ntfs_lock);
	/* If we loaded the attribute definitions table, throw it away now. */
	if (vol->attrdef)
		OSFree(vol->attrdef, vol->attrdef_size, ntfs_malloc_tag);
	/* If we used a volume specific upcase table, throw it away now. */
	if (vol->upcase)
		OSFree(vol->upcase, vol->upcase_len << NTFSCHAR_SIZE_SHIFT,
				ntfs_malloc_tag);
	/* If we cached a volume name, throw it away now. */
	if (vol->name)
		OSFree(vol->name, vol->name_size, ntfs_malloc_tag);
no_mft:
	/* Deinitialize the ntfs_volume locks. */
	lck_rw_destroy(&vol->mftbmp_lock, ntfs_lock_grp);
	lck_rw_destroy(&vol->lcnbmp_lock, ntfs_lock_grp);
	lck_mtx_destroy(&vol->rename_lock, ntfs_lock_grp);
	lck_rw_destroy(&vol->secure_lock, ntfs_lock_grp);
	lck_spin_destroy(&vol->security_id_lock, ntfs_lock_grp);
	/* Finally, free the ntfs volume. */
	OSFree(vol, sizeof(ntfs_volume), ntfs_malloc_tag);
unload:
	err = 0;
	OSKextReleaseKextWithLoadTag(OSKextGetCurrentLoadTag());
abort:
	ntfs_debug("Done.");
	return err;
}

/**
 * ntfs_sync_args - arguments for the ntfs_sync_callback (see below)
 * @sync:	if IO_SYNC wait for all i/o to complete
 * @err:	if an error occurred the error code is returned here
 */
struct ntfs_sync_args {
	int sync;
	int err;
};

/**
 * ntfs_sync_callback - callback for vnode iterate in ntfs_sync()
 * @vn:		vnode the callback is invoked with (has iocount reference)
 * @arg:	pointer to an ntfs_sync_args structure
 *
 * This callback is called from vnode_iterate() which is called from
 * ntfs_sync() for all in-core, non-dead, non-suspend vnodes belonging to the
 * mounted volume that still have an ntfs inode attached.
 *
 * We sync all dirty inodes to disk and if an error occurs we record it in the
 * @err field of the ntfs_sync_args structure pointed to by @arg.  Note we
 * preserve the old error code if an error is already recorded unless that
 * error code is ENOTSUP.
 *
 * If the @sync field of the ntfs_sync_args structure pointed to by @arg is
 * IO_SYNC, wait for all i/o to complete.
 */
static int ntfs_sync_callback(vnode_t vn, void *arg)
{
	ntfs_inode *ni = NTFS_I(vn);
	ntfs_volume *vol = ni->vol;
	struct ntfs_sync_args *args = (struct ntfs_sync_args*)arg;

	/*
	 * Skip the inodes for $MFT and $MFTMirr.  They are done separately as
	 * the last ones to be synced.
	 */
	if (ni != vol->mft_ni && ni != vol->mftmirr_ni) {
		errno_t err;

		/*
		 * Sync the inode data to disk and sync the ntfs inode to the
		 * mft record(s) but do not write the mft record(s) to disk.
		 */
		err = ntfs_inode_sync(ni, args->sync, TRUE);
		/*
		 * Only record the first error that is not ENOTSUP or record
		 * ENOTSUP if that is the only error.
		 *
		 * Skip deleted inodes.
		 */
		if (err && err != ENOENT) {
			if (!args->err || args->err == ENOTSUP)
				args->err = err;
		}
	}
	return VNODE_RETURNED;
}

/**
 * ntfs_sync_helper - helper for ntfs_sync()
 * @ni:				ntfs inode the helper is invoked for
 * @args:			pointer to an ntfs_sync_args structure
 * @skip_mft_record_sync:	do not sync the mft record(s) to disk
 *
 * This helper is called from ntfs_sync() when syncing the $MFT and $MFTMirr
 * inodes.
 *
 * Any errors are returned in @args->err.
 */
static void ntfs_sync_helper(ntfs_inode *ni, struct ntfs_sync_args *args,
		const BOOL skip_mft_record_sync)
{
	errno_t err;

	err = vnode_get(ni->vn);
	if (err) {
		ntfs_error(ni->vol->mp, "Failed to get vnode for $MFT%s "
				"(error %d).",
				(ni == ni->vol->mft_ni) ? "" : "Mirr",
				(int)err);
		goto err;
	}
	err = ntfs_inode_sync(ni, args->sync, skip_mft_record_sync);
	vnode_put(ni->vn);
	/* Skip deleted inodes. */
	if (err && err != ENOENT) {
		ntfs_error(ni->vol->mp, "Failed to sync $MFT%s (error %d).",
				(ni == ni->vol->mft_ni) ? "" : "Mirr",
				(int)err);
		goto err;
	}
	return;
err:
	if (!args->err || args->err == ENOTSUP)
		args->err = err;
	return;
}

/**
 * ntfs_sync - sync a mounted volume to disk
 * @mp:		mount point of ntfs file system
 * @waitfor:	if MNT_WAIT wait fo i/o to complete
 * @context:	vfs context
 *
 * The VFS calls this via VFS_SYNC() when it wants to sync all cached data of
 * the mounted ntfs volume described by the mount @mp.
 *
 * If @waitfor is MNT_WAIT, wait for all i/o to complete before returning.
 *
 * Return 0 on success and errno on error.
 *
 * Note this function is only called for r/w mounted volumes so no need to
 * check if the volume is read-only.
 */
static int ntfs_sync(struct mount *mp, int waitfor, vfs_context_t context)
{
	ntfs_volume *vol = NTFS_MP(mp);
	struct ntfs_sync_args args;

	/* If we are mounted read-only, we do not need to sync anything. */
	if (NVolReadOnly(vol))
		return 0;
	ntfs_debug("Entering.");
	args.sync = (waitfor == MNT_WAIT) ? IO_SYNC : 0;
	args.err = 0;
	/* Iterate over all vnodes and run ntfs_inode_sync() on each of them. */
	(void)vnode_iterate(mp, 0, ntfs_sync_callback, (void*)&args);
	/*
	 * Finally, sync the inodes for $MFT and $MFTMirr to disk.  Note we do
	 * the sync twice to ensure that any interdependent changes that are
	 * flushed from one inode to the other are actually written to disk.
	 */
	ntfs_sync_helper(vol->mftmirr_ni, &args, TRUE);
	ntfs_sync_helper(vol->mft_ni, &args, TRUE);
	ntfs_sync_helper(vol->mftmirr_ni, &args, FALSE);
	ntfs_sync_helper(vol->mft_ni, &args, FALSE);
	if (!args.err)
		ntfs_debug("Done.");
	else
		ntfs_error(mp, "Failed to sync volume (error %d).", args.err);
	return args.err;
}

/**
 * ntfs_remount - change the mount options of a mounted ntfs file system
 * @mp:		mount point of mounted ntfs file system
 * @opts:	ntfs specific mount options (already copied from user space)
 *
 * Change the mount options of an already mounted ntfs file system.
 *
 * Return 0 on success and errno on error.
 *
 * If the remount fails, we must call OSKextReleaseKextWithLoadTag
 * to allow the KEXT to be unloaded when no longer in use.
 *
 *
 * Note we are at mount protocol version 0.0 where we do not have any ntfs
 * specific mount options so we annotate @opts as __unused to make gcc happy.
 */
static errno_t ntfs_remount(mount_t mp,
		ntfs_mount_options_1_0 *opts)
{
	errno_t err = 0;
	ntfs_volume *vol = NTFS_MP(mp);

	ntfs_debug("Entering.");
	/*
	 * Check for a change in the case sensitivity semantics and abort if
	 * one is requested as things could get very confused if we allow a
	 * remount to switch from case sensitive to case insensitive or vice
	 * versa.
	 */
	if (((opts->flags & NTFS_MNT_OPT_CASE_SENSITIVE) &&
			!NVolCaseSensitive(vol)) ||
			(!(opts->flags & NTFS_MNT_OPT_CASE_SENSITIVE) &&
			NVolCaseSensitive(vol))) {
		ntfs_error(mp, "Cannot change case sensitivity semantics via "
				"remount.  You need to unmount and then mount "
				"again with the desired options.");
		err = EINVAL;
		goto err_exit;
	}
	/*
	 * If we are remounting read-write, make sure there are no volume
	 * errors and that no unsupported volume flags are set.  Also, empty
	 * the logfile journal as it would become stale as soon as something is
	 * written to the volume and mark the volume dirty so that chkdsk is
	 * run if the volume is not umounted cleanly.  Finally, mark the quotas
	 * out of date so Windows rescans the volume on boot and updates them.
	 *
	 * When remounting read-only, mark the volume clean if no volume errors
	 * have occured.
	 */
	if (vfs_iswriteupgrade(mp)) {
		/* We no longer allow (re-)mounting read/write. */
		ntfs_error(mp, "Remounting read/write is not supported");
		goto EROFS_exit;
#if 0 
		static const char es[] = ".  Cannot remount read-write.  To "
				"fix this problem boot into Windows, run "
				"chkdsk c: /f /v /x from the command prompt "
				"(replace c: with the drive letter of this "
				"volume), then reboot into Mac OS X and mount "
				"the volume again.";

		/* Remounting read-write. */
		if (NVolErrors(vol)) {
			ntfs_error(mp, "Volume has errors and is read-only%s",
					es);
			goto EROFS_exit;
		}
		if (vol->vol_flags & VOLUME_MUST_MOUNT_RO_MASK) {
			ntfs_error(mp, "Volume has unsupported flags set "
					"(0x%x) and is read-only%s",
					(unsigned)le16_to_cpu(vol->vol_flags),
					es);
			goto EROFS_exit;
		}
		if (ntfs_volume_flags_set(vol, VOLUME_IS_DIRTY)) {
			ntfs_error(mp, "Failed to set dirty bit in volume "
					"information flags%s", es);
			goto EROFS_exit;
		}
		if (ntfs_logfile_empty(vol->logfile_ni)) {
			ntfs_error(mp, "Failed to empty journal $LogFile%s",
					es);
			NVolSetErrors(vol);
			goto EROFS_exit;
		}
		if (ntfs_quotas_mark_out_of_date(vol)) {
			ntfs_error(mp, "Failed to mark quotas out of date%s",
					es);
			NVolSetErrors(vol);
			goto EROFS_exit;
		}
		if (ntfs_usnjrnl_stamp(vol)) {
			ntfs_error(mp, "Failed to stamp transation log "
					"($UsnJrnl)%s", es);
			NVolSetErrors(vol);
			goto EROFS_exit;
		}
		NVolClearReadOnly(vol);
#endif /* r/w upgrade not supported */
	} else if (!NVolReadOnly(vol) && vfs_isrdonly(mp)) {
		/* Remounting read-only, flush all pending writes. */
		err = ntfs_sync(mp, MNT_WAIT, NULL);
		if (err) {
			ntfs_error(mp, "Failed to sync volume (error %d).  "
					"Cannot remount read-only.", err);
			goto err_exit;
		}
		/* If no volume errors have occured, mark the volume clean. */
		if (!NVolErrors(vol)) {
			if (ntfs_volume_flags_clear(vol, VOLUME_IS_DIRTY))
				ntfs_warning(mp, "Failed to clear dirty bit "
						"in volume information "
						"flags.  Run chkdsk.");
			/* Flush the changes to disk. */
			err = ntfs_sync(mp, MNT_WAIT, NULL);
			if (err) {
				ntfs_error(mp, "Failed to sync volume (error "
						"%d).  Cannot remount "
						"read-only.", err);
				/*
				 * Try to set the dirty flag again in case we
				 * did clear it but something else failed.  We
				 * do not care about any errors as we almost
				 * expect them to happen if we got here.
				 */
				(void)ntfs_volume_flags_set(vol,
						VOLUME_IS_DIRTY);
				goto err_exit;
			}
		} else
			ntfs_warning(mp, "Volume has errors.  Leaving volume "
					"marked dirty.  Run chkdsk.");
		NVolSetReadOnly(vol);
	}
	/* Don't allow the user to clear MNT_DONTBROWSE for read/write volumes. */
	if (vfs_isrdwr(mp))
		vfs_setflags(mp, MNT_DONTBROWSE);
	// TODO: Copy mount options from @opts to @vol.
	ntfs_debug("Done.");
	return 0;
EROFS_exit:
	err = EROFS;
err_exit:
	OSKextReleaseKextWithLoadTag(OSKextGetCurrentLoadTag());
	return err;
}

/**
 * ntfs_mount - mount an ntfs file system
 * @mp:		mount point to initialize/mount
 * @dev_vn:	vnode of the device we are mounting
 * @data:	mount options (in user space)
 * @context:	vfs context
 *
 * The VFS calls this via VFS_MOUNT() when it wants to mount an ntfs volume.
 *
 * Note: @dev_vn is NULLVP if this is a MNT_UPDATE or MNT_RELOAD mount but of
 * course in those cases it can be retrieved from the NTFS_MP(mp)->dev_vn.
 *
 * Return 0 on success and errno on error.
 *
 * We call OSKextRetainKextWithLoadTag() to prevent the KEXT from being
 * unloaded automatically while in use.  If the mount fails, we must call
 * OSKextReleaseKextWithLoadTag() to allow the KEXT to be unloaded.
 */
static int ntfs_mount(mount_t mp, vnode_t dev_vn, user_addr_t data,
		vfs_context_t context)
{
	daddr64_t nr_blocks;
	struct vfsstatfs *sfs = vfs_statfs(mp);
	ntfs_volume *vol;
	buf_t buf;
	kauth_cred_t cred;
	dev_t dev;
	NTFS_BOOT_SECTOR *bs;
	errno_t err, err2;
	u32 blocksize;
	ntfs_mount_options_header opts_hdr;
	ntfs_mount_options_1_0 opts;

	ntfs_debug("Entering.");
	OSKextRetainKextWithLoadTag(OSKextGetCurrentLoadTag());
	/*
	 * FIXME: Not convinced that this is necessary.  It may well be
	 * sufficient to set cred = vfs_context_ucred(context) as some file
	 * systems do (e.g. msdosfs, old ntfs), but HFS does it this way so we
	 * follow suit.  Also, some file systems even simply set cred = NOCRED
	 * (e.g. udf).  Should investigate or ask someone...
	 */ 
	cred = vfs_context_proc(context) ? vfs_context_ucred(context) : NOCRED;
	/* Copy our mount options header from user space. */
	err = copyin(data, (caddr_t)&opts_hdr, sizeof(opts_hdr));
	if (err) {
		ntfs_error(mp, "Failed to copy mount options header from user "
				"space (error %d).", err);
		goto unload;
	}
	ntfs_debug("Mount options header version %d.%d.", opts_hdr.major_ver,
			opts_hdr.minor_ver);
	/* Get and check options. */
	switch (opts_hdr.major_ver) {
	case 1:
		if (opts_hdr.minor_ver != 0)
			ntfs_warning(mp, "Your version of /sbin/mount_ntfs is "
					"newer than this driver, ignoring any "
					"new options.");
		/* Version 1.x has one option so copy it from user space. */
		err = copyin((data + sizeof(opts_hdr) + 7) & ~7,
				(caddr_t)&opts, sizeof(opts));
		if (err) {
			ntfs_error(mp, "Failed to copy NTFS mount options "
					"from user space (error %d).", err);
			goto unload;
		}
		break;
	case 0:
		/* Version 0.x has no options at all. */
		bzero(&opts, sizeof(opts));
		break;
	default:
		ntfs_warning(mp, "Your version of /sbin/mount_ntfs is not "
				"compatible with this driver, ignoring NTFS "
				"specific mount options.");
		bzero(&opts, sizeof(opts));
		break;
	}
	/*
	 * We only allow read/write mounts if the "nobrowse" option was also
	 * given.  This is to discourage end users from mounting read/write,
	 * but still allows our utilities (such as an OS install) to make
	 * changes to an NTFS volume.  Without the "nobrowse" option, we force
	 * a read-only mount.  Note that we also check for non-update mounts
	 * here.  In the case of an update mount, ntfs_remount() will do the
	 * appropriate checking for changing the writability of the mount.
	 */
	if ((vfs_flags(mp) & MNT_DONTBROWSE) == 0 && !vfs_isupdate(mp))
		vfs_setflags(mp, MNT_RDONLY);
	/*
	 * TODO: For now we do not implement ACLs thus we force the "noowners"
	 * mount option.
	 */
	vfs_setflags(mp, MNT_IGNORE_OWNERSHIP);
	/*
	 * We do not support MNT_RELOAD yet.  Note, MNT_RELOAD implies the
	 * file system is currently read-only.
	 */
	if (vfs_isreload(mp)) {
		ntfs_error(mp, "MNT_RELOAD is not supported yet.");
		err = ENOTSUP;
		goto unload;
	}
	/*
	 * If this is a remount request, handle this elsewhere.  Note this
	 * check has to come after the vfs_isreload() check as vfs_isupdate()
	 * is always true when vfs_isreload() is true but this is not true the
	 * other way round.
	 */
	if (vfs_isupdate(mp))
		return ntfs_remount(mp, &opts);
	/* We know this is a real mount request thus @dev_vn is not NULL. */
	dev = vnode_specrdev(dev_vn);
	/* Let the VFS do advisory locking for us. */
	vfs_setlocklocal(mp);
	/*
	 * Tell old-style applications that we support VolFS style lookups.
	 *
	 * Note we do not set MNT_DOVOLFS because then various things start
	 * breaking like for example the Finder "Empty Trash" command always
	 * fails silently unless we also support va_nchildren in
	 * ntfs_vnop_getattr() and set ATTR_DIR_ENTRYCOUNT in our valid
	 * directory attributes in ntfs_getattr().
	 */
	//vfs_setflags(mp, MNT_DOVOLFS);
	/*
	 * Set the file system id in the fsstat part of the mount structure.
	 * We use the device @dev for the first 32-bit value and the dynamic
	 * file system number assigned by the VFS to us for the second 32-bit
	 * value.  This is important because the VFS uses the first 32-bit
	 * value to satisfy the ATTR_CMN_DEVID request in getattrlist() and
	 * getvolattrlist() thus it must be the device.
	 */
	sfs->f_fsid.val[0] = (int32_t)dev;
	sfs->f_fsid.val[1] = (int32_t)vfs_typenum(mp);
	/*
	 * Allocate and initialize an ntfs volume and attach it to the vfs
	 * mount.
	 */
	vol = OSMalloc(sizeof(ntfs_volume), ntfs_malloc_tag);
	if (!vol) {
		ntfs_error(mp, "Failed to allocate ntfs volume buffer.");
		err = ENOMEM;
		goto unload;
	}
	*vol = (ntfs_volume) {
		.mp = mp,
		.dev = dev,
		.dev_vn = dev_vn,
		/*
		 * Default is group and other have read-only access to files
		 * and directories while owner has full access.  Everyone gets
		 * directory search and file execute permission.  The latter is
		 * so people can execute binaries from NTFS volumes.
		 *
		 * In reality it does not matter as we set MNT_IGNORE_OWNERSHIP
		 * thus everyone can fully access the NTFS volume.  The only
		 * reason to set the umask this way is that when people copy
		 * files with the Finder or "cp -p" from an NTFS volume to a
		 * HFS for example, the file does not end up being world
		 * writable.
		 */
		.fmask = 0022,
		.dmask = 0022,
		.mft_zone_multiplier = 1,
		.on_errors = ON_ERRORS_CONTINUE,
	};
	lck_rw_init(&vol->mftbmp_lock, ntfs_lock_grp, ntfs_lock_attr);
	lck_rw_init(&vol->lcnbmp_lock, ntfs_lock_grp, ntfs_lock_attr);
	lck_mtx_init(&vol->rename_lock, ntfs_lock_grp, ntfs_lock_attr);
	lck_rw_init(&vol->secure_lock, ntfs_lock_grp, ntfs_lock_attr);
	lck_spin_init(&vol->security_id_lock, ntfs_lock_grp, ntfs_lock_attr);
	vfs_setfsprivate(mp, vol);
	if (vfs_isrdonly(mp))
		NVolSetReadOnly(vol);
	/* Check for the requested case sensitivity semantics. */
	if (opts.flags & NTFS_MNT_OPT_CASE_SENSITIVE) {
		ntfs_debug("Mounting volume case sensitive.");
		NVolSetCaseSensitive(vol);
	}
// FIXME: For now disable sparse support as it is not done yet...
#if 0
	/* By default, enable sparse support. */
	NVolSetSparseEnabled(vol);
#endif
	/* By default, enable compression support. */
	NVolSetCompressionEnabled(vol);
	blocksize = vfs_devblocksize(mp);
	/* We support device sector sizes up to the PAGE_SIZE. */
	if (blocksize > PAGE_SIZE) {
		ntfs_error(mp, "Device has unsupported sector size (%u).  "
				"The maximum supported sector size on this "
				"system is %u bytes.", blocksize, PAGE_SIZE);
		err = ENOTSUP;
		goto err;
	}
	/*
	 * If the block size of the device we are to mount is less than
	 * NTFS_BLOCK_SIZE, change the block size to NTFS_BLOCK_SIZE.
	 */
	if (blocksize < NTFS_BLOCK_SIZE) {
		ntfs_debug("Setting device block size to NTFS_BLOCK_SIZE.");
		err = ntfs_blocksize_set(mp, dev_vn, NTFS_BLOCK_SIZE, context);
		if (err) {
			ntfs_error(mp, "Failed to set device block size to "
					"NTFS_BLOCK_SIZE (512 bytes) because "
					"the DKIOCSETBLOCKSIZE ioctl returned "
					"error %d).", err);
			goto err;
		}
		blocksize = NTFS_BLOCK_SIZE;
	} else
		ntfs_debug("Device block size (%u) is greater than or equal "
				"to NTFS_BLOCK_SIZE.", blocksize);
	/* Get the size of the device in units of blocksize bytes. */
	err = VNOP_IOCTL(dev_vn, DKIOCGETBLOCKCOUNT, (caddr_t)&nr_blocks, 0,
			context);
	if (err) {
		ntfs_error(mp, "Failed to determine the size of the device "
				"(DKIOCGETBLOCKCOUNT ioctl returned error "
				"%d).", err);
		err = ENXIO;
		goto err;
	}
	vol->nr_blocks = nr_blocks;
#ifdef DEBUG
	{
		u64 dev_size, u;
		char *suffix;
		int shift = 0;
		u8 blocksize_shift = ffs(blocksize) - 1;
	
		dev_size = u = (u64)nr_blocks << blocksize_shift;
		while ((u >>= 10) > 10 && shift < 40)
			shift += 10;
		switch (shift) {
		case 0:
			suffix = "bytes";
			break;
		case 10:
			suffix = "kiB";
			break;
		case 20:
			suffix = "MiB";
			break;
		case 30:
			suffix = "GiB";
			break;
		default:
			suffix = "TiB";
			break;
		}
		ntfs_debug("Device size is %llu%s (%llu bytes).",
				(unsigned long long)dev_size >> shift, suffix,
				(unsigned long long)dev_size);
	}
#endif
	/* Read the boot sector and return the buffer containing it. */
	buf = NULL;
	bs = NULL;
	err = ntfs_boot_sector_read(vol, cred, &buf, &bs);
	if (err) {
		ntfs_error(mp, "Not an NTFS volume.");
		goto err;
	}
	/*
	 * Extract the data from the boot sector and setup the ntfs volume
	 * using it.
	 */
	err = ntfs_boot_sector_parse(vol, bs);
	err2 = buf_unmap(buf);
	if (err2)
		ntfs_error(mp, "Failed to unmap buffer of boot sector (error "
				"%d).", err2);
	buf_brelse(buf);
	if (err) {
		ntfs_error(mp, "%s NTFS file system.",
				err == ENOTSUP ? "Unsupported" : "Invalid");
		goto err;
	}
	/*
	 * If the boot sector indicates a sector size bigger than the current
	 * device block size, switch the device block size to the sector size.
	 * TODO: It may be possible to support this case even when the set
	 * below fails, we would just be breaking up the i/o for each sector
	 * into multiple blocks for i/o purposes but otherwise it should just
	 * work.  However it is safer to leave disabled until someone hits this
	 * error message and then we can get them to try it without the setting
	 * so we know for sure that it works.  We would then want to set
	 * vol->sector_size* to the current blocksize or add vol->blocksize*...
	 * No, cannot do that or will break directory operations.  We will need
	 * to move to using vol->blocksize* instead of vol->sector_size in most
	 * places and stick with vol->sector_size where we really want its
	 * actual value.
	 */
	if (vol->sector_size > blocksize) {
		ntfs_debug("Setting device block size to sector size.");
		err = ntfs_blocksize_set(mp, dev_vn, vol->sector_size, context);
		if (err) {
			ntfs_error(mp, "Failed to set device block size to "
					"sector size (%u bytes) because "
					"the DKIOCSETBLOCKSIZE ioctl returned "
					"error %d).", vol->sector_size, err);
			goto err;
		}
		blocksize = vol->sector_size;
	}
	/* Initialize the cluster and mft allocators. */
	ntfs_setup_allocators(vol);
	/*
	 * Get the $MFT inode and bootstrap the volume sufficiently so we can
	 * get other inodes and map (extent) mft records.
	 */
	err = ntfs_mft_inode_get(vol);
	if (err)
		goto err;
	lck_mtx_lock(&ntfs_lock);
	if (NVolCompressionEnabled(vol)) {
		/*
		 * The current mount may be a compression user if the cluster
		 * size is less than or equal to 4kiB.
		 */
		if (vol->cluster_size <= 4096) {
			if (!ntfs_compression_buffer) {
				ntfs_compression_buffer = OSMalloc(
						ntfs_compression_buffer_size,
						ntfs_malloc_tag);
				if (!ntfs_compression_buffer) {
					// FIXME: We could continue with
					// compression disabled.  But do we
					// want to do that given the system is
					// that low on memory?
					ntfs_error(mp, "Failed to allocate "
							"buffer for "
							"compression engine.");
					NVolClearCompressionEnabled(vol);
					lck_mtx_unlock(&ntfs_lock);
					goto err;
				}
			}
			ntfs_compression_users++;
		} else {
			ntfs_debug("Disabling compression because the cluster "
					"size of %u bytes is above the "
					"allowed maximum of 4096 bytes.",
					(unsigned)vol->cluster_size);
			NVolClearCompressionEnabled(vol);
		}
	}
	/* Generate the global default upcase table if necessary. */
	if (!ntfs_default_upcase) {
		ntfs_default_upcase = OSMalloc(ntfs_default_upcase_size,
				ntfs_malloc_tag);
		if (!ntfs_default_upcase) {
			// FIXME: We could continue without a default upcase
			// table.  But do we want to do that given the system
			// is that low on memory?
			ntfs_error(mp, "Failed to allocate memory for default "
					"upcase table.");
			lck_mtx_unlock(&ntfs_lock);
			err = ENOMEM;
			goto err;
		}
		ntfs_upcase_table_generate(ntfs_default_upcase,
				ntfs_default_upcase_size);
	}
	/*
	 * Temporarily take a reference on the default upcase table to avoid
	 * race conditions with concurrent (u)mounts.
	 */
	ntfs_default_upcase_users++;
	lck_mtx_unlock(&ntfs_lock);
	/* Process the system inodes. */
	err = ntfs_system_inodes_get(vol);
	/*
	 * We now have the volume upcase table (either having read it from disk
	 * or using the default, in which case we have taken a reference on the
	 * default upcase table) or there was an error and we are going to bail
	 * out.  In any case, we can drop our temporary reference on the
	 * default upcase table and throw it away if we had the only reference.
	 */
	lck_mtx_lock(&ntfs_lock);
	if (!--ntfs_default_upcase_users) {
		OSFree(ntfs_default_upcase, ntfs_default_upcase_size,
				ntfs_malloc_tag);
		ntfs_default_upcase = NULL;
	}
	lck_mtx_unlock(&ntfs_lock);
	/* If we failed to process the system inodes, abort the mount. */
	if (err) {
		ntfs_error(mp, "Failed to load system files (error %d).", err);
		goto err;
	}
	/*
	 * Determine the number of free clusters and cache it in the volume (in
	 * @vol->nr_free_clusters).
	 */
	err = ntfs_set_nr_free_clusters(vol);
	if (err)
		goto err;
	/*
	 * Determine the number of both total and free mft records and cache
	 * them in the volume (in @vol->nr_mft_records and
	 * @vol->nr_free_mft_records, respectively).
	 */
	err = ntfs_set_nr_mft_records(vol);
	if (err)
		goto err;
	/*
	 * Finally, determine the statfs information for the volume and cache
	 * it in the vfs mount structure.
	 */
	ntfs_statfs(vol, sfs);
	ntfs_debug("Done.");
	return 0;
unload:
	/* Ensure NTFS_MP(mp) is NULL so it is safe to call ntfs_unmount(). */
	vfs_setfsprivate(mp, NULL);
err:
	ntfs_error(mp, "Mount failed (error %d).", err);
	/*
	 * ntfs_unmount() will clean up everything we did until we encountered
	 * the error condition including calling OSKextReleaseKextWithLoadTag().
	 *
	 * Note we need to pass MNT_FORCE to ensure ntfs_unmount() definitely
	 * ends up calling OSKextReleaseKextWithLoadTag().
	 */
	ntfs_unmount(mp, MNT_FORCE, context);
	return err;
}

/**
 * ntfs_root - get the vnode of the root directory of an ntfs file system
 * @mp:		mount point of ntfs file system
 * @vpp:	destination pointer for the obtained file system root vnode
 * @context:	vfs context
 *
 * The VFS calls this via VFS_ROOT() when it wants to have the root directory
 * of a mounted ntfs volume.  We already have the root vnode/inode due to
 * ntfs_mount() so just get an iocount reference on the vnode and return the
 * vnode.
 *
 * Return 0 on success and errno on error.
 *
 * Warning: We get a panic() if we return error here!  Due to the function
 * checkdirs() which is called after ntfs_mount() but before VFS_START() (which
 * we do not implement).
 */
static int ntfs_root(mount_t mp, struct vnode **vpp,
		vfs_context_t context __unused)
{
	ntfs_volume *vol = NTFS_MP(mp);
	vnode_t vn;
	int err;

	ntfs_debug("Entering.");
	if (!vol || !vol->root_ni || !vol->root_ni->vn)
		panic("%s(): Mount and/or root inode and/or vnode is not "
				"loaded.\n", __FUNCTION__);
	vn = vol->root_ni->vn;
	/*
	 * Simulate an ntfs_inode_get() by taking an iocount reference on the
	 * vnode of the ntfs inode.  It is ok to do this here because we know
	 * the root directory is loaded and attached to the ntfs volume (thus
	 * we already hold a use count reference on the vnode).
	 */
	err = vnode_get(vn);
	if (!err) {
		*vpp = vn;
		ntfs_debug("Done.");
	} else {
		*vpp = NULL;
		ntfs_error(mp, "Cannot return root vnode because vnode_get() "
				"failed (error %d).", err);
	}
	return err;
}

/**
 * ntfs_vget - get the vnode corresponding to an inode number
 * @mp:		mount point of ntfs file system
 * @ino:	inode number / mft record number to obtain
 * @vpp:	destination pointer for the obtained vnode
 * @context:	vfs context
 *
 * Volfs and other strange places where no further path or name context is
 * available call this via VFS_VGET() to obtain the vnode with the inode number
 * @ino.
 *
 * The vnode is returned with an iocount reference.
 *
 * Return 0 on success and errno on error.
 *
 * FIXME: The only potential problem is that using only the inode / mft record
 * number only allows ntfs_vget() to return the file or directory vnode itself
 * but not for example the vnode of a named stream or other attribute.  Perhaps
 * this does not matter for volfs in which case everything is fine...
 */
static int ntfs_vget(mount_t mp, ino64_t ino, struct vnode **vpp,
		vfs_context_t context __unused)
{
	ntfs_inode *ni;
	errno_t err;

	ntfs_debug("Entering for ino 0x%llx.", (unsigned long long)ino);
	/*
	 * Remove all NTFS core system files from the name space so we do not
	 * need to worry about users damaging a volume by writing to them or
	 * deleting/renaming them and so that we can return fsRtParID (1) as
	 * the inode number of the parent of the volume root directory and
	 * fsRtDirID (2) as the inode number of the volume root directory which
	 * are both expected by Carbon and various applications.
	 *
	 * Note we thus have to remap inode number 2 (fsRtDirID) to FILE_root
	 * here.
	 */
	if (ino < FILE_first_user) {
		if (ino != 2) {
			ntfs_debug("Removing core NTFS system file (mft_no "
					"0x%x) from name space.",
					(unsigned)ino);
			err = ENOENT;
			goto err;
		}
		/*
		 * @ino is 2, i.e. fsRtDirID, thus return the vnode of the root
		 * directory inode (FILE_root).
		 *
		 * First try to use the already loaded root directory inode and
		 * if that fails for some reason go and get it the slow way.
		 */
		ni = NTFS_MP(mp)->root_ni;
		if (ni) {
			err = vnode_get(ni->vn);
			if (!err)
				goto done;
		}
		ino = FILE_root;
	}
	err = ntfs_inode_get(NTFS_MP(mp), ino, FALSE, LCK_RW_TYPE_SHARED, &ni,
			NULL, NULL);
	if (!err) {
		lck_rw_unlock_shared(&ni->lock);
done:
		ntfs_debug("Done.");
		*vpp = ni->vn;
		return err;
	}
err:
	*vpp = NULL;
	if (err != ENOENT)
		ntfs_error(mp, "Failed to get mft_no 0x%llx (error %d).",
				(unsigned long long)ino, err);
	else
		ntfs_debug("Mft_no 0x%llx does not exist, returning ENOENT.",
				(unsigned long long)ino);
	return err;
}

/**
 * ntfs_getattr - obtain information about a mounted ntfs volume
 * @mp:		mount point of ntfs file system
 * @fsa:	requested information and destination in which to return it
 * @context:	vfs context
 *
 * The VFS calls this via VFS_GETATTR() when it wants to obtain some
 * information about the mounted ntfs volume described by the mount @mp.
 *
 * Which information is requested is described by the vfs attribute structure
 * pointed to by @fsa, which is also the destination pointer in which the
 * requested information is returned.
 *
 * Return 0 on success and errno on error.
 *
 * Note: Further details are in the man page for the getattrlist function and
 * in the header files xnu/bsd/sys/{mount,attr}.h.
 */
static int ntfs_getattr(mount_t mp, struct vfs_attr *fsa,
		vfs_context_t context __unused)
{
	u64 nr_clusters, nr_free_clusters, nr_used_mft_records;
	u64 nr_free_mft_records;
	ntfs_volume *vol = NTFS_MP(mp);
	struct vfsstatfs *sfs = vfs_statfs(mp);
	ntfs_inode *ni;

	ntfs_debug("Entering.");
	/* Get a fully consistent snapshot of this point in time. */
	lck_rw_lock_shared(&vol->mftbmp_lock);
	lck_rw_lock_shared(&vol->lcnbmp_lock);
	nr_clusters = vol->nr_clusters;
	nr_free_clusters = vol->nr_free_clusters;
	lck_rw_unlock_shared(&vol->lcnbmp_lock);
	nr_free_mft_records = vol->nr_free_mft_records;
	nr_used_mft_records = vol->nr_mft_records - nr_free_mft_records;
	lck_rw_unlock_shared(&vol->mftbmp_lock);
	/* Number of file system objects on volume (at this point in time). */
	VFSATTR_RETURN(fsa, f_objcount, nr_used_mft_records);
	/*
	 * Number of files on volume (at this point in time).
	 * FIXME: We cannot easily support this and the number of directories,
	 * below) as these two fields require reading the entirety of
	 * $MFT/$DATA, and checking each record if it is in use and if so,
	 * check if it is a file or directory and then return that here.  Note
	 * we would take all special files as files, and only real directories
	 * as directories.  Instead of reading all of $MFT/$DATA it may be
	 * worth only reading mft records that are set as in use in the
	 * $MFT/$BITMAP.  Also, need to check if the mft record is a base mft
	 * record or not and only if it is one should it be marked as
	 * file/directory.  Or should it be counted towards files, just like
	 * other special files?
	 *
	 * A quote from ZFS:
	 *
	 * <quote>Carbon depends on f_filecount and f_dircount so make up some
	 * values based on total objects.</quote>
	 *
	 * Thus at least for now we behave like ZFS does.
	 */
	VFSATTR_RETURN(fsa, f_filecount, nr_used_mft_records -
			(nr_used_mft_records / 4));
	/* Number of directories on volume (at this point in time). */
	VFSATTR_RETURN(fsa, f_dircount, nr_used_mft_records / 4);
	/*
	 * Maximum number of file system objects given infinite free space.
	 * The actual number will be likely smaller as it is limited by the
	 * amount of free space but both HFS and ZFS return the theoretical
	 * maximum so we do the same.
	 */
	VFSATTR_RETURN(fsa, f_maxobjcount, NTFS_MAX_NR_MFT_RECORDS);
	/*
	 * Block size for the below size values.  We use the cluster size of
	 * the volume as that means we do not convert to a different unit.
	 * Alternatively, we could return the sector size instead.
	 */
	VFSATTR_RETURN(fsa, f_bsize, vol->cluster_size);
	/* Optimal transfer block size (in bytes). */
	VFSATTR_RETURN(fsa, f_iosize, ubc_upl_maxbufsize());
	/* Total data blocks in file system (in units of @f_bsize). */
	VFSATTR_RETURN(fsa, f_blocks, nr_clusters);
	/* Free data blocks in file system (in units of @f_bsize). */
	VFSATTR_RETURN(fsa, f_bfree, nr_free_clusters);
	/*
	 * Free blocks available to non-superuser (in units of @f_bsize), same
	 * as the free data blocks as NTFS, like ZFS, does not support root
	 * reservation.
	 */ 
	VFSATTR_RETURN(fsa, f_bavail, nr_free_clusters);
	/* Blocks in use (in units of @f_bsize). */
	VFSATTR_RETURN(fsa, f_bused, nr_clusters - nr_free_clusters);
	/*
	 * Free inodes in file system (at this point in time).  This is made up
	 * of both the current number of free mft records and the amount of
	 * available free space for new mft records.  The number is then capped
	 * to the maximum allowed number of mft records.  This is what ZFS
	 * does, too.
	 */
	nr_free_mft_records += (nr_free_clusters << vol->cluster_size_shift) >>
			vol->mft_record_size_shift;
	if (nr_free_mft_records > NTFS_MAX_NR_MFT_RECORDS - nr_used_mft_records)
		nr_free_mft_records = NTFS_MAX_NR_MFT_RECORDS -
			nr_used_mft_records;
	VFSATTR_RETURN(fsa, f_ffree, nr_free_mft_records);
	/*
	 * Number of inodes in file system (at this point in time).  This is
	 * the number of available files we returned above plus the number of
	 * mft records currently in use.
	 */
	VFSATTR_RETURN(fsa, f_files, nr_used_mft_records + nr_free_mft_records);
	/*
	 * We set the file system id in the statfs part of the mount structure
	 * in ntfs_mount(), so just return that.
	 */
	VFSATTR_RETURN(fsa, f_fsid, sfs->f_fsid);
	/*
	 * The mount syscall sets the f_owner in the statfs structure of the
	 * mount structure to the uid of the user performing the mount, so just
	 * return that.
	 */
	VFSATTR_RETURN(fsa, f_owner, sfs->f_owner);
	/*
	 * Optional features supported by the volume.  Note, ->valid indicates
	 * which bits in the ->capabilities are valid whilst ->capabilities
	 * indicates the capabilities of the driver implementation.  An
	 * example: Ntfs is journalled but we do not implement journalling so
	 * we do not set that bit in ->capabilities, but we do set it in
	 * ->valid thus stating that we do not support journalling.
	 */
	if (VFSATTR_IS_ACTIVE(fsa, f_capabilities)) {
		vol_capabilities_attr_t *ca = &fsa->f_capabilities;

		/* Volume format capabilities. */
		ca->capabilities[VOL_CAPABILITIES_FORMAT] =
				VOL_CAP_FMT_PERSISTENTOBJECTIDS |
				VOL_CAP_FMT_SYMBOLICLINKS |
				VOL_CAP_FMT_HARDLINKS |
				VOL_CAP_FMT_JOURNAL |
				/* We do not support journalling. */
				//VOL_CAP_FMT_JOURNAL_ACTIVE |
				VOL_CAP_FMT_SPARSE_FILES |
				VOL_CAP_FMT_ZERO_RUNS |
				/*
				 * Whether to be case sensitive or not is a
				 * mount option.
				 */
				(NVolCaseSensitive(vol) ?
					VOL_CAP_FMT_CASE_SENSITIVE : 0) |
				VOL_CAP_FMT_CASE_PRESERVING |
				VOL_CAP_FMT_FAST_STATFS |
				VOL_CAP_FMT_2TB_FILESIZE |
				// TODO: What do we need to do to implement
				// open deny modes?  And do we want to?
				// VOL_CAP_FMT_OPENDENYMODES |
				VOL_CAP_FMT_HIDDEN_FILES |
				VOL_CAP_FMT_PATH_FROM_ID |
				0;
		ca->valid[VOL_CAPABILITIES_FORMAT] =
				VOL_CAP_FMT_PERSISTENTOBJECTIDS |
				VOL_CAP_FMT_SYMBOLICLINKS |
				VOL_CAP_FMT_HARDLINKS |
				VOL_CAP_FMT_JOURNAL |
				VOL_CAP_FMT_JOURNAL_ACTIVE |
				VOL_CAP_FMT_NO_ROOT_TIMES |
				VOL_CAP_FMT_SPARSE_FILES |
				VOL_CAP_FMT_ZERO_RUNS |
				VOL_CAP_FMT_CASE_SENSITIVE |
				VOL_CAP_FMT_CASE_PRESERVING |
				VOL_CAP_FMT_FAST_STATFS |
				VOL_CAP_FMT_2TB_FILESIZE |
				VOL_CAP_FMT_OPENDENYMODES |
				VOL_CAP_FMT_HIDDEN_FILES |
				VOL_CAP_FMT_PATH_FROM_ID |
				0;
		/* File system driver capabilities. */
		ca->capabilities[VOL_CAPABILITIES_INTERFACES] =
				/* TODO: These are not implemented yet. */
				// VOL_CAP_INT_SEARCHFS |
				VOL_CAP_INT_ATTRLIST |
				// VOL_CAP_INT_NFSEXPORT |
				// VOL_CAP_INT_READDIRATTR |
				// VOL_CAP_INT_EXCHANGEDATA |
				/*
				 * Nothing supports copyfile in current xnu and
				 * it is not documented so we do not support it
				 * either.
				 */
				// VOL_CAP_INT_COPYFILE |
				// VOL_CAP_INT_ALLOCATE |
				VOL_CAP_INT_VOL_RENAME |
				VOL_CAP_INT_ADVLOCK |
				VOL_CAP_INT_FLOCK |
				// VOL_CAP_INT_EXTENDED_SECURITY |
				// VOL_CAP_INT_USERACCESS |
				// VOL_CAP_INT_MANLOCK |
				VOL_CAP_INT_NAMEDSTREAMS |
				VOL_CAP_INT_EXTENDED_ATTR |
				0;
		ca->valid[VOL_CAPABILITIES_INTERFACES] =
				VOL_CAP_INT_SEARCHFS |
				VOL_CAP_INT_ATTRLIST |
				VOL_CAP_INT_NFSEXPORT |
				VOL_CAP_INT_READDIRATTR |
				VOL_CAP_INT_EXCHANGEDATA |
				VOL_CAP_INT_COPYFILE |
				VOL_CAP_INT_ALLOCATE |
				VOL_CAP_INT_VOL_RENAME |
				VOL_CAP_INT_ADVLOCK |
				VOL_CAP_INT_FLOCK |
				VOL_CAP_INT_EXTENDED_SECURITY |
				VOL_CAP_INT_USERACCESS |
				VOL_CAP_INT_MANLOCK |
				VOL_CAP_INT_NAMEDSTREAMS |
				VOL_CAP_INT_EXTENDED_ATTR |
				0;
		/* Reserved, set to zero. */
		ca->capabilities[VOL_CAPABILITIES_RESERVED1] = 0;
		ca->valid[VOL_CAPABILITIES_RESERVED1] = 0;
		ca->capabilities[VOL_CAPABILITIES_RESERVED2] = 0;
		ca->valid[VOL_CAPABILITIES_RESERVED2] = 0;
		VFSATTR_SET_SUPPORTED(fsa, f_capabilities);
	}
	/*
	 * Attributes supported by the volume.  Note, ->validattr indicates the
	 * capabilities of the file system driver whilst ->nativeattr indicates
	 * the native capabilities of the volume format itself.
	 */
	if (VFSATTR_IS_ACTIVE(fsa, f_attributes)) {
		vol_attributes_attr_t *aa = &fsa->f_attributes;

		/*
		 * Common attribute group (these attributes apply to all of the
		 * below groups).
		 */
		aa->validattr.commonattr =
				ATTR_CMN_NAME |
				/*
				 * ATTR_CMN_DEVID, ATTR_CMN_OBJTYPE, and
				 * ATTR_CMN_OBJTAG are supplied by the VFS.
				 */
				ATTR_CMN_DEVID |
				ATTR_CMN_FSID |
				ATTR_CMN_OBJTYPE |
				ATTR_CMN_OBJTAG |
				ATTR_CMN_OBJID |
				ATTR_CMN_OBJPERMANENTID |
				ATTR_CMN_PAROBJID |
				ATTR_CMN_SCRIPT |
				ATTR_CMN_CRTIME |
				ATTR_CMN_MODTIME |
				ATTR_CMN_CHGTIME |
				ATTR_CMN_ACCTIME |
				ATTR_CMN_BKUPTIME |
				/*
				 * Supplied by the VFS via a call to
				 * vn_getxattr(XATTR_FINDERINFO_NAME).
				 */
				ATTR_CMN_FNDRINFO |
				ATTR_CMN_OWNERID |
				ATTR_CMN_GRPID |
				ATTR_CMN_ACCESSMASK |
				ATTR_CMN_FLAGS |
				//ATTR_CMN_NAMEDATTRCOUNT /* not implemented */ |
				//ATTR_CMN_NAMEDATTRLIST /* not implemented */ |
				/*
				 * Supplied by the VFS via calls to
				 * vnode_authorize().
				 */
				ATTR_CMN_USERACCESS |
				//ATTR_CMN_EXTENDED_SECURITY |
				//ATTR_CMN_UUID |
				//ATTR_CMN_GRPUUID |
				ATTR_CMN_FILEID |
				ATTR_CMN_PARENTID;
		aa->nativeattr.commonattr =
				ATTR_CMN_NAME |
				ATTR_CMN_DEVID |
				ATTR_CMN_FSID |
				ATTR_CMN_OBJTYPE |
				ATTR_CMN_OBJTAG |
				ATTR_CMN_OBJID |
				ATTR_CMN_OBJPERMANENTID |
				ATTR_CMN_PAROBJID |
				ATTR_CMN_SCRIPT |
				ATTR_CMN_CRTIME |
				ATTR_CMN_MODTIME |
				ATTR_CMN_CHGTIME |
				ATTR_CMN_ACCTIME |
				ATTR_CMN_BKUPTIME |
				ATTR_CMN_FNDRINFO |
				ATTR_CMN_OWNERID |
				ATTR_CMN_GRPID |
				ATTR_CMN_ACCESSMASK |
				ATTR_CMN_FLAGS |
				ATTR_CMN_NAMEDATTRCOUNT |
				ATTR_CMN_NAMEDATTRLIST |
				ATTR_CMN_USERACCESS |
				ATTR_CMN_EXTENDED_SECURITY |
				ATTR_CMN_UUID |
				ATTR_CMN_GRPUUID |
				ATTR_CMN_FILEID |
				ATTR_CMN_PARENTID;
		/* Volume attribute group. */
		aa->validattr.volattr =
				/*
				 * ATTR_VOL_FSTYPE, ATTR_VOL_MOUNTPOINT,
				 * ATTR_VOL_MOUNTFLAGS, ATTR_VOL_MOUNTEDDEVICE,
				 * and ATTR_VOL_ENCODINGSUSED are supplied by
				 * the VFS.
				 */
				ATTR_VOL_FSTYPE |
				ATTR_VOL_SIGNATURE |
				ATTR_VOL_SIZE |
				ATTR_VOL_SPACEFREE |
				ATTR_VOL_SPACEAVAIL |
				ATTR_VOL_MINALLOCATION |
				ATTR_VOL_ALLOCATIONCLUMP |
				ATTR_VOL_IOBLOCKSIZE |
				ATTR_VOL_OBJCOUNT |
				ATTR_VOL_FILECOUNT |
				ATTR_VOL_DIRCOUNT |
				ATTR_VOL_MAXOBJCOUNT |
				ATTR_VOL_MOUNTPOINT |
				ATTR_VOL_NAME |
				ATTR_VOL_MOUNTFLAGS |
				ATTR_VOL_MOUNTEDDEVICE |
				ATTR_VOL_ENCODINGSUSED |
				ATTR_VOL_CAPABILITIES |
				ATTR_VOL_ATTRIBUTES;
		aa->nativeattr.volattr =
				ATTR_VOL_FSTYPE |
				ATTR_VOL_SIGNATURE |
				ATTR_VOL_SIZE |
				ATTR_VOL_SPACEFREE |
				ATTR_VOL_SPACEAVAIL |
				ATTR_VOL_MINALLOCATION |
				ATTR_VOL_ALLOCATIONCLUMP |
				ATTR_VOL_IOBLOCKSIZE |
				ATTR_VOL_OBJCOUNT |
				/*
				 * NTFS does not provide ATTR_VOL_FILECOUNT and
				 * ATTR_VOL_DIRCOUNT on disk.
				 */
				//ATTR_VOL_FILECOUNT |
				//ATTR_VOL_DIRCOUNT |
				ATTR_VOL_MAXOBJCOUNT |
				ATTR_VOL_MOUNTPOINT |
				ATTR_VOL_NAME |
				ATTR_VOL_MOUNTFLAGS |
				ATTR_VOL_MOUNTEDDEVICE |
				ATTR_VOL_ENCODINGSUSED |
				ATTR_VOL_CAPABILITIES |
				ATTR_VOL_ATTRIBUTES;
		/* Directory attribute group. */
		aa->validattr.dirattr =
				/*
				 * ATTR_DIR_LINKCOUNT and ATTR_DIR_ENTRYCOUNT
				 * are hard to work out on NTFS and the
				 * getattrlist(2) man page states that a file
				 * system should not implement
				 * ATTR_DIR_LINKCOUNT in this case.  We choose
				 * not to implement ATTR_DIR_ENTRYCOUNT either.
				 */
				//ATTR_DIR_LINKCOUNT |
				//ATTR_DIR_ENTRYCOUNT |
				/* This is supplied by the VFS. */
				ATTR_DIR_MOUNTSTATUS;
		aa->nativeattr.dirattr =
				/*
				 * NTFS does not provide ATTR_DIR_LINKCOUNT and
				 * ATTR_DIR_ENTRYCOUNT on disk.
				 */
				//ATTR_DIR_LINKCOUNT |
				//ATTR_DIR_ENTRYCOUNT |
				ATTR_DIR_MOUNTSTATUS;
		/* File attribute group. */
		aa->validattr.fileattr =
				ATTR_FILE_LINKCOUNT |
				ATTR_FILE_TOTALSIZE |
				ATTR_FILE_ALLOCSIZE |
				ATTR_FILE_IOBLOCKSIZE |
				/* This is supplied by the VFS. */
				ATTR_FILE_CLUMPSIZE |
				ATTR_FILE_DEVTYPE |
				//ATTR_FILE_FILETYPE |
				//ATTR_FILE_FORKCOUNT |
				//ATTR_FILE_FORKLIST |
				ATTR_FILE_DATALENGTH |
				ATTR_FILE_DATAALLOCSIZE |
				//ATTR_FILE_DATAEXTENTS |
				/*
				 * Both ATTR_FILE_RSRCLENGTH and
				 * ATTR_FILE_RSRCALLOCSIZE are supplied by the
				 * VFS via a call to
				 * vn_getxattr(XATTR_RESOURCEFORK_NAME).
				 *
				 * FIXME: The VFS supplies
				 * ATTR_FILE_RSRCALLOCSIZE by rounding up
				 * ATTR_FILE_RSRCLENGTH to the the next logical
				 * block size boundary (for NTFS the cluster
				 * this is the next cluster boundary) which is
				 * not correct if the resource fork named
				 * stream is sparse which can be the case on
				 * NTFS.
				 */
				ATTR_FILE_RSRCLENGTH |
				ATTR_FILE_RSRCALLOCSIZE |
				//ATTR_FILE_RSRCEXTENTS |
				0;
		aa->nativeattr.fileattr =
				ATTR_FILE_LINKCOUNT |
				/*
				 * NTFS does not provide ATTR_FILE_TOTALSIZE
				 * and ATTR_FILE_ALLOCSIZE on disk or at least
				 * not in an easy to determine way.
				 */
				//ATTR_FILE_TOTALSIZE |
				//ATTR_FILE_ALLOCSIZE |
				ATTR_FILE_IOBLOCKSIZE |
				ATTR_FILE_CLUMPSIZE /* obsolete */ |
				ATTR_FILE_DEVTYPE |
				/*
				 * VFS does not allow setting of
				 * ATTR_FILE_FILETYPE, ATTR_FILE_FORKCOUNT,
				 * ATTR_FILE_FORKLIST, ATTR_FILE_DATAEXTENTS,
				 * and ATTR_FILE_RSRCEXTENTS.
				 */
				//ATTR_FILE_FILETYPE /* always zero */ |
				//ATTR_FILE_FORKCOUNT |
				//ATTR_FILE_FORKLIST |
				ATTR_FILE_DATALENGTH |
				ATTR_FILE_DATAALLOCSIZE |
				//ATTR_FILE_DATAEXTENTS /* obsolete, HFS-specific */ |
				ATTR_FILE_RSRCLENGTH |
				ATTR_FILE_RSRCALLOCSIZE |
				//ATTR_FILE_RSRCEXTENTS /* obsolete, HFS-specific */ |
				0;
		/* Fork attribute group. */
		aa->validattr.forkattr =
				/*
				 * getattrlist(2) man page says that we should
				 * not implement any fork attributes.
				 */
				//ATTR_FORK_TOTALSIZE |
				//ATTR_FORK_ALLOCSIZE |
				0;
		aa->nativeattr.forkattr =
				/* VFS does not allow setting of these. */
				//ATTR_FORK_TOTALSIZE |
				//ATTR_FORK_ALLOCSIZE |
				0;
		VFSATTR_SET_SUPPORTED(fsa, f_attributes);
	}
	ni = vol->root_ni;
	lck_rw_lock_shared(&ni->lock);
	/*
	 * For the volume times, we use the corresponding times from the
	 * standard information attribute of the root directory inode.
	 */
	/* Creation time. */
	VFSATTR_RETURN(fsa, f_create_time, ni->creation_time);
	/*
	 * Last modification time.  We use the last mft change time as this
	 * changes every time the directory is changed in any way, thus it
	 * reflects the volume change time the best.
	 */
	VFSATTR_RETURN(fsa, f_modify_time, ni->last_mft_change_time);
	/* Time of last access. */
	VFSATTR_RETURN(fsa, f_access_time, ni->last_access_time);
	/* Time of last backup. */
	if (VFSATTR_IS_ACTIVE(fsa, f_backup_time)) {
		if (NInoValidBackupTime(ni)) {
			VFSATTR_RETURN(fsa, f_backup_time, ni->backup_time);
			lck_rw_unlock_shared(&ni->lock);
		} else {
			errno_t err;

			if (!lck_rw_lock_shared_to_exclusive(&ni->lock))
				lck_rw_lock_exclusive(&ni->lock);
			/*
			 * Load the AFP_AfpInfo stream and initialize the
			 * backup time and Finder Info (if they are not already
			 * valid).
			 */
			err = ntfs_inode_afpinfo_read(ni);
			if (err) {
				ntfs_error(vol->mp, "Failed to obtain AfpInfo "
						"for mft_no 0x%llx (error "
						"%d).",
						(unsigned long long)ni->mft_no,
						err);
				lck_rw_unlock_exclusive(&ni->lock);
				return err;
			}
			if (!NInoValidBackupTime(ni))
				panic("%s(): !NInoValidBackupTime(base_ni)\n",
						__FUNCTION__);
			VFSATTR_RETURN(fsa, f_backup_time, ni->backup_time);
			lck_rw_unlock_exclusive(&ni->lock);
		}
	} else
		lck_rw_unlock_shared(&ni->lock);
	/*
	 * File system subtype.  Set this to the ntfs version encoded into 16
	 * bits, the high 8 bits being the major version and the low 8 bits
	 * being the minor version.  This is then extended to 32 bits, thus the
	 * higher 16 bits are currently zero.  The latter could be used at a
	 * later point in time to return more information about the mount
	 * options of the mounted volume (e.g. enable/disable sparse creation,
	 * compression, encryption, quotas, acls, usnjournal, case sensitivity,
	 * etc).
	 */
	VFSATTR_RETURN(fsa, f_fssubtype, (u32)vol->major_ver << 8 |
			vol->minor_ver);
	/* NUL terminated volume name in decomposed UTF-8. */
	if (VFSATTR_IS_ACTIVE(fsa, f_vol_name)) {
		/* Copy the cached name from the ntfs_volume structure. */
		(void)strlcpy(fsa->f_vol_name, vol->name, MAXPATHLEN - 1);
		VFSATTR_SET_SUPPORTED(fsa, f_vol_name);
	}
	/*
	 * Used for ATTR_VOL_SIGNATURE, Carbon's FSVolumeInfo.signature.  The
	 * kernel's getvolattrlist() function will default this to 'BD' which
	 * is apparently the generic signature that most Carbon file systems
	 * should be returning.
	 *
	 * ZFS returns 'Z!' so we return 'NT'.
	 */
	VFSATTR_RETURN(fsa, f_signature, 0x4e54); /* 'NT' */
	/*
	 * Same as Carbon's FSVolumeInfo.filesystemID.  HFS and HFS Plus use a
	 * value of zero.  ZFS also returns zero so we do that, too.
	 */
	VFSATTR_RETURN(fsa, f_carbon_fsid, 0);
	ntfs_debug("Done.");
	return 0;
}

/**
 * ntfs_volume_rename - rename an ntfs volume
 * @vol:	ntfs volume to rename
 * @name:	new name for the ntfs volume
 *
 * Rename the ntfs volume @vol to @name which is a decomposed, NUL-terminated,
 * UTF-8 string as used on OS X.
 *
 * Return 0 on success and errno on error.
 */
static errno_t ntfs_volume_rename(ntfs_volume *vol, char *name)
{
	ntfs_inode *ni = vol->vol_ni;
	MFT_RECORD *m;
	ntfs_attr_search_ctx *ctx;
	ATTR_RECORD *a;
	u8 *utf8_name = NULL;
	ntfschar *ntfs_name = NULL;
	size_t utf8_name_size, ntfs_name_size;
	signed ntfs_name_len = 0;
	errno_t err;

	ntfs_debug("Entering (old name: %s, new name: %s).", vol->name, name);
	/*
	 * We do not need to do anything if the new name is the same as the old
	 * name.
	 */
	utf8_name_size = strlen(name) + 1;
	if (utf8_name_size == vol->name_size &&
			!strncmp(vol->name, name, vol->name_size)) {
		ntfs_debug("The new name is the same as the old name, "
				"ignoring the rename request.");
		return 0;
	}
	/*
	 * If the new name is the empty string "", no need to convert it.  We
	 * will simply delete the $VOLUME_NAME attribute altogether.
	 *
	 * Otherwise, convert the name from the decomposed, UTF-8 format used
	 * by OS X into the little endian, 2-byte, composed Unicode format used
	 * by NTFS.
	 */
	if (utf8_name_size > 1) {
		ntfs_name_len = utf8_to_ntfs(vol, (u8*)name, utf8_name_size,
				&ntfs_name, &ntfs_name_size);
		if (ntfs_name_len < 0) {
			err = -ntfs_name_len;
			ntfs_error(vol->mp, "Failed to convert volume name to "
					"little endian, 2-byte, composed "
					"Unicode (error %d).", (int)err);
			goto err;
		}
		/* Switch @ntfs_name_len to be the name length in bytes. */
		ntfs_name_len <<= NTFSCHAR_SIZE_SHIFT;
		/*
		 * Verify that the length of the new name is in the allowed
		 * range.
		 */
		err = ntfs_attr_size_bounds_check(vol, AT_VOLUME_NAME,
				ntfs_name_len);
		if (err) {
			if (err == ERANGE) {
				ntfs_error(vol->mp, "Specified name is too "
						"long (%d little endian, "
						"2-byte, composed Unicode "
						"characters).",
						ntfs_name_len <<
						NTFSCHAR_SIZE_SHIFT);
				err = ENAMETOOLONG;
			} else {
				ntfs_error(vol->mp, "$VOLUME_NAME attribute "
						"is not defined on the NTFS "
						"volume.  Possible "
						"corruption!  You should run "
						"chkdsk.");
				err = EIO;
			}
			goto err;
		}
	}
	/* Make a copy of the new volume name to be placed in @vol->name. */
	utf8_name = OSMalloc(utf8_name_size, ntfs_malloc_tag);
	if (!utf8_name) {
		ntfs_error(vol->mp, "Not enough memory to make a copy of the "
				"new name.");
		err = ENOMEM;
		goto err;
	}
	if (strlcpy((char*)utf8_name, name, utf8_name_size) >= utf8_name_size)
		panic("%s(): strlcpy() failed\n", __FUNCTION__);
	err = vnode_get(ni->vn);
	if (err) {
		ntfs_error(vol->mp, "Failed to get vnode for $Volume.");
		goto err;
	}
	err = ntfs_mft_record_map(ni, &m);
	if (err) {
		ntfs_error(vol->mp, "Failed to map mft record for $Volume "
				"(error %d).", err);
		m = NULL;
		ctx = NULL;
		goto put_err;
	}
	ctx = ntfs_attr_search_ctx_get(ni, m);
	if (!ctx) {
		ntfs_error(vol->mp, "Not enough memory to get attribute "
				"search context.");
		err = ENOMEM;
		goto put_err;
	}
	err = ntfs_attr_lookup(AT_VOLUME_NAME, AT_UNNAMED, 0, 0, NULL, 0, ctx);
	m = ctx->m;
	a = ctx->a;
	if (err || a->non_resident || a->flags) {
		if (err != ENOENT) {
			/* Real lookup error or corrupt attribute. */
			if (!err)
				goto name_err;
			ntfs_error(vol->mp, "Failed to lookup volume name "
					"attribute (error %d).", err);
			goto put_err;
		}
		if (!ntfs_name) {
			ntfs_debug("Volume has no name and new name is the "
					"empty string, nothing to do.");
			goto done;
		}
		ntfs_debug("Volume has no name.  Creating new volume name "
				"attribute.");
		err = ntfs_resident_attr_record_insert(ni, ctx, AT_VOLUME_NAME,
				NULL, 0, ntfs_name, ntfs_name_len);
		if (err || ctx->is_error) {
			if (!err)
				err = ctx->error;
			ntfs_error(vol->mp, "Failed to %s $Volume (error %d).",
					ctx->is_error ?
					"remap extent mft record of" :
					"insert volume name attribute in", err);
			goto put_err;
		}
	} else {
		u8 *val = (u8*)a + le16_to_cpu(a->value_offset);
		/* Some bounds checks. */
		if (val < (u8*)a || val + le32_to_cpu(a->value_length) >
				(u8*)a + le32_to_cpu(a->length) ||
				(u8*)a + le32_to_cpu(a->length) >
				(u8*)m + vol->mft_record_size)
			goto name_err;
		if (!ntfs_name) {
			/*
			 * The new name is the empty string, thus remove the
			 * $VOLUME_NAME attribute altogether.
			 */
			ntfs_debug("New name is the empty string.  Removing "
					"the existing $VOLUME_NAME attribute.");
			err = ntfs_attr_record_delete(ni, ctx);
			if (!err)
				goto done;
			ntfs_warning(vol->mp, "Failed to delete volume name "
					"attribute (error %d).  Truncating it "
					"to zero length instead.", err);
		}
		/* Resize the existing attribute to fit the new name. */
retry_resize:
		err = ntfs_resident_attr_value_resize(m, a, ntfs_name_len);
		if (err) {
			if (err != ENOSPC)
				panic("%s(): err != ENOSPC\n", __FUNCTION__);
			/*
			 * If the base mft record does not have an attribute
			 * list attribute, add it now.
			 */
			if (!NInoAttrList(ni)) {
				err = ntfs_attr_list_add(ni, m, ctx);
				if (err || ctx->is_error) {
					if (!err)
						err = ctx->error;
					ntfs_error(vol->mp, "Failed to %s "
							"$Volume (error %d).",
							ctx->is_error ?
							"remap extent mft "
							"record of" :
							"add attribute list "
							"attribute to", err);
					goto put_err;
				}
				/*
				 * The attribute location will have changed so
				 * update it from the search context.
				 */
				m = ctx->m;
				a = ctx->a;
				/*
				 * We now have an attribute list attribute.
				 * This may have cause the attribute to be
				 * moved out to an extent mft record in which
				 * case there would now be enough space to
				 * resize the attribute.
				 *
				 * Alternatively some other large attribute may
				 * have been moved out to an extent mft record
				 * thus generating enough space in the base mft
				 * record to resize the attribute.
				 *
				 * In either case we simply want to retry the
				 * resize.
				 */
				goto retry_resize;
			}
			/*
			 * If the attribute record is the only one in the mft
			 * record then there must have been enough space.
			 */
			if (ntfs_attr_record_is_only_one(m, a))
				panic("%s(): err == ENOSPC && "
						"ntfs_attr_record_is_only_one"
						"()\n", __FUNCTION__);
			/*
			 * The attribute record is not the only one in the mft
			 * record.  Move it out to an extent mft record which
			 * will cause enough space to be generated.
			 */
			lck_rw_lock_shared(&ni->attr_list_rl.lock);
			err = ntfs_attr_record_move(ctx);
			lck_rw_unlock_shared(&ni->attr_list_rl.lock);
			if (err) {
				ntfs_error(vol->mp, "Failed to move volume "
						"name attribute to an extent "
						"mft record (error %d).", err);
				goto put_err;
			}
			/*
			 * The attribute location will have changed so update
			 * it from the search context.
			 */
			m = ctx->m;
			a = ctx->a;
			/*
			 * Retry the original attribute record resize as we
			 * will now have enough space to do it.
			 */
			goto retry_resize;
		}
		/* Copy the new name into the resized attribute record. */
		if (ntfs_name)
			memcpy((u8*)a + le16_to_cpu(a->value_offset),
					ntfs_name, ntfs_name_len);
	}
	/* Free the no longer needed temporary copy of the new name. */
	if (ntfs_name)
		OSFree(ntfs_name, ntfs_name_size, ntfs_malloc_tag);
	/* Mark the mft record dirty to ensure it gets written out. */
	NInoSetMrecNeedsDirtying(ctx->ni);
done:
	/*
	 * Finally set the new name to be the volume name releasing the old one
	 * first.  Since we have no locking around accesses to the volume name,
	 * we have to be careful about how we update it here, i.e. we have to
	 * set the size to the smaller of the two, then switch the pointers,
	 * then set the size to the new size and only then free the old
	 * pointer.  This is also why we do this under the protection of the
	 * mapped mft record so there cannot be two concurrent
	 * ntfs_volume_rename()s running.
	 */
	name = vol->name;
	ntfs_name_size = vol->name_size;
	if (utf8_name_size < vol->name_size)
		vol->name_size = utf8_name_size;
	vol->name = (char*)utf8_name;
	vol->name_size = utf8_name_size;
	ntfs_attr_search_ctx_put(ctx);
	ntfs_mft_record_unmap(ni);
	(void)vnode_put(ni->vn);
	OSFree(name, ntfs_name_size, ntfs_malloc_tag);
	ntfs_debug("Done.");
	return 0;
name_err:
	ntfs_error(vol->mp, "Volume name attribute is corrupt.  Run chkdsk.");
	NVolSetErrors(vol);
	err = EIO;
put_err:
	if (ctx)
		ntfs_attr_search_ctx_put(ctx);
	if (m)
		ntfs_mft_record_unmap(ni);
	(void)vnode_put(ni->vn);
err:
	if (utf8_name)
		OSFree(utf8_name, utf8_name_size, ntfs_malloc_tag);
	if (ntfs_name)
		OSFree(ntfs_name, ntfs_name_size, ntfs_malloc_tag);
	return err;
}

/**
 * ntfs_setattr - set information about a mounted ntfs volume
 * @mp:		mount point of ntfs file system
 * @fsa:	information to set
 * @context:	vfs context
 *
 * The VFS calls this via VFS_SETATTR() when it wants to set some information
 * about the mounted ntfs volume described by the mount @mp.
 *
 * Which information is to be set is described by the vfs attribute structure
 * pointed to by @fsa, which is also the source pointer from which the
 * information to be set is copied.
 *
 * At present the kernel will only ever call this function for ATTR_VOL_NAME,
 * i.e. to set the name of the volume.
 *
 * Return 0 on success and errno on error.
 *
 * Note: Further details are in the man pages for the getattrlist and
 * setattrlist functions and in the header files xnu/bsd/sys/{mount,attr}.h.
 *
 * Note this function is only called for r/w mounted volumes so no need to
 * check if the volume is read-only.
 */
static int ntfs_setattr(struct mount *mp, struct vfs_attr *fsa,
		vfs_context_t context)
{
	kauth_cred_t cred = vfs_context_ucred(context);
	errno_t err;

	ntfs_debug("Entering.");
	/*
	 * Must be superuser or owner of file system to change volume
	 * attributes.
	 */
	if (!kauth_cred_issuser(cred) && (kauth_cred_getuid(cred) !=
			vfs_statfs(mp)->f_owner))
		return EACCES;
	/*
	 * Only the volume name is settable (ATTR_VOL_NAME) at present so if
	 * this is not requested return success.  The VFS enforces that we are
	 * never called with any other flags set.
	 */
	if (!VFSATTR_IS_ACTIVE(fsa, f_vol_name))
		return 0;
	if (!fsa->f_vol_name)
		panic("%s(): !fsa->f_vol_name\n", __FUNCTION__);
	err = ntfs_volume_rename(NTFS_MP(mp), fsa->f_vol_name);
	if (err) {
		ntfs_error(mp, "Failed to set the name of the volume to %s "
				"(error %d).", fsa->f_vol_name, err);
		return err;
	}
	VFSATTR_SET_SUPPORTED(fsa, f_vol_name);
	ntfs_debug("Done.");
	return 0;
}

static struct vfsops ntfs_vfsops = {
	.vfs_mount	= ntfs_mount,
	.vfs_unmount	= ntfs_unmount,
	.vfs_root	= ntfs_root,
	.vfs_getattr	= ntfs_getattr,
	.vfs_sync	= ntfs_sync,
	.vfs_vget	= ntfs_vget,
	.vfs_setattr	= ntfs_setattr,
};

static struct vnodeopv_desc *ntfs_vnodeopv_desc_list[1] = {
	&ntfs_vnodeopv_desc,
};

/* Lock group and lock attribute for allocation and freeing of locks. */
static lck_grp_attr_t *ntfs_lock_grp_attr;
lck_grp_t *ntfs_lock_grp;
lck_attr_t *ntfs_lock_attr;

/* A tag to allow allocation and freeing of memory. */
OSMallocTag ntfs_malloc_tag;

static vfstable_t ntfs_vfstable;

extern kern_return_t ntfs_module_start(kmod_info_t *ki __unused,
		void *data __unused);
kern_return_t ntfs_module_start(kmod_info_t *ki __unused, void *data __unused)
{
	errno_t err;
	struct vfs_fsentry vfe;

	printf("NTFS driver " VERSION " [Flags: R/W"
#ifdef DEBUG
			" DEBUG"
#endif
			"].\n");
	/* This should never happen. */
	if (ntfs_lock_grp_attr || ntfs_lock_grp || ntfs_lock_attr ||
			ntfs_malloc_tag)
		panic("%s(): Lock(s) and/or malloc tag already initialized.\n",
				__FUNCTION__);
	/* First initialize the lock group so we can initialize debugging. */
	ntfs_lock_grp_attr = lck_grp_attr_alloc_init();
	if (!ntfs_lock_grp_attr) {
lck_err:
		printf("NTFS: Failed to allocate a lock element.\n");
		goto dbg_err;
	}
#ifdef DEBUG
	lck_grp_attr_setstat(ntfs_lock_grp_attr);
#endif
	ntfs_lock_grp = lck_grp_alloc_init("com.apple.filesystems.ntfs",
			ntfs_lock_grp_attr);
	if (!ntfs_lock_grp)
		goto lck_err;
	ntfs_lock_attr = lck_attr_alloc_init();
	if (!ntfs_lock_attr)
		goto lck_err;
#ifdef DEBUG
	lck_attr_setdebug(ntfs_lock_attr);
#endif
	/* Allocate a tag so we can allocate memory. */
	ntfs_malloc_tag = OSMalloc_Tagalloc("com.apple.filesystems.ntfs",
			OSMT_DEFAULT);
	if (!ntfs_malloc_tag) {
		printf("NTFS: OSMalloc_Tagalloc() failed.\n");
		goto dbg_err;
	}
	/* Initialize the driver wide lock. */
	lck_mtx_init(&ntfs_lock, ntfs_lock_grp, ntfs_lock_attr);
	/*
	 * This call must happen before we can use ntfs_debug(),
	 * ntfs_warning(), and ntfs_error().
	 */
	ntfs_debug_init();
	ntfs_debug("Debug messages are enabled.");
	err = ntfs_default_sds_entries_init();
	if (err)
		goto sds_err;
	err = ntfs_inode_hash_init();
	if (err)
		goto hash_err;
	vfe = (struct vfs_fsentry) {
		.vfe_vfsops	= &ntfs_vfsops,
		.vfe_vopcnt	= 1,	/* For now we just use one set of vnode
					   operations for all file types.
					   Note: Current max is 5 due to (not
					   needed) hard-coded limit in xnu. */
		.vfe_opvdescs	= ntfs_vnodeopv_desc_list,
		.vfe_fsname	= "ntfs",
// TODO: Implement VFS_TBLREADDIR_EXTENDED and set it here.
		.vfe_flags	= VFS_TBLNATIVEXATTR | VFS_TBL64BITREADY |
				  VFS_TBLLOCALVOL | VFS_TBLNOTYPENUM |
				  VFS_TBLFSNODELOCK | VFS_TBLTHREADSAFE,
	};
	err = vfs_fsadd(&vfe, &ntfs_vfstable);
	if (!err) {
		ntfs_debug("NTFS driver registered successfully.");
		return KERN_SUCCESS;
	}
	ntfs_error(NULL, "vfs_fsadd() failed (error %d).", (int)err);
	ntfs_inode_hash_deinit();
hash_err:
	OSFree(ntfs_file_sds_entry, 0x60 * 4, ntfs_malloc_tag);
	ntfs_file_sds_entry = NULL;
sds_err:
	ntfs_debug_deinit();
	lck_mtx_destroy(&ntfs_lock, ntfs_lock_grp);
dbg_err:
	if (ntfs_malloc_tag) {
		OSMalloc_Tagfree(ntfs_malloc_tag);
		ntfs_malloc_tag = NULL;
	}
	if (ntfs_lock_attr) {
		lck_attr_free(ntfs_lock_attr);
		ntfs_lock_attr = NULL;
	}
	if (ntfs_lock_grp) {
		lck_grp_free(ntfs_lock_grp);
		ntfs_lock_grp = NULL;
	}
	if (ntfs_lock_grp_attr) {
		lck_grp_attr_free(ntfs_lock_grp_attr);
		ntfs_lock_grp_attr = NULL;
	}
	printf("NTFS: Failed to register the NTFS driver.\n");
	return KERN_FAILURE;
}

extern kern_return_t ntfs_module_stop(kmod_info_t *ki __unused,
		void *data __unused);
kern_return_t ntfs_module_stop(kmod_info_t *ki __unused, void *data __unused)
{
	errno_t err;

	if (!ntfs_lock_grp_attr || !ntfs_lock_grp || !ntfs_lock_attr ||
			!ntfs_malloc_tag)
		panic("%s(): Lock(s) and/or malloc tag not yet initialized.\n",
				__FUNCTION__);
	ntfs_debug("Unregistering NTFS driver.");
	err = vfs_fsremove(ntfs_vfstable);
	if (err) {
		if (err == EBUSY)
			printf("NTFS: Failed to unregister the NTFS driver "
					"because there are mounted NTFS "
					"volumes.\n");
		else
			printf("NTFS: Failed to unregister the NTFS driver "
					"because vfs_fsremove() failed (error "
					"%d).\n", err);
		return KERN_FAILURE;
	}
	ntfs_inode_hash_deinit();
	OSFree(ntfs_file_sds_entry, 0x60 * 4, ntfs_malloc_tag);
	ntfs_file_sds_entry = NULL;
	ntfs_debug("Done.");
	/*
	 * Once this completes, we cannot use ntfs_debug(), ntfs_warning(), and
	 * ntfs_error() any more.  Since it cannot fail we cheat and report
	 * "Done." before the call.
	 */
	ntfs_debug_deinit();
	lck_mtx_destroy(&ntfs_lock, ntfs_lock_grp);
	OSMalloc_Tagfree(ntfs_malloc_tag);
	ntfs_malloc_tag = NULL;
	lck_attr_free(ntfs_lock_attr);
	ntfs_lock_attr = NULL;
	lck_grp_free(ntfs_lock_grp);
	ntfs_lock_grp = NULL;
	lck_grp_attr_free(ntfs_lock_grp_attr);
	ntfs_lock_grp_attr = NULL;
	return KERN_SUCCESS;
}
