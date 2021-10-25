/*
 * Copyright (c) 2006-2008 Apple Inc. All rights reserved.
 *
 * This file contains Original Code and/or Modifications of Original Code as
 * defined in and that are subject to the Apple Public Source License Version
 * 2.0 (the 'License'). You may not use this file except in compliance with the
 * License.
 *
 * Please obtain a copy of the License at http://www.opensource.apple.com/apsl/
 * and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR
 * A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the
 * License for the specific language governing rights and limitations under the
 * License.
 */

#include <sys/loadable_fs.h>
#ifndef FSUC_GETUUID
#define FSUC_GETUUID 'k'
#endif

#include <sys/disk.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <CoreFoundation/CFString.h>

/* Define this if you want debug output to go to syslog. */
//#define NTFS_UTIL_DEBUG

#ifdef NTFS_UTIL_DEBUG
#include <syslog.h>
#endif /* NTFS_UTIL_DEBUG */

#include "ntfs.h"
#include "ntfs_types.h"
#include "ntfs_endian.h"
#include "ntfs_layout.h"

/*
 * The NTFS in-memory mount point structure.
 *
 * Adapted from ../kext/ntfs_volume.h.
 */
typedef struct {
	/* NTFS bootsector provided information. */
	u32 sector_size;		/* in bytes */
	u32 cluster_size;		/* in bytes */
	u32 mft_record_size;		/* in bytes */
	u32 index_block_size;		/* in bytes */
	LCN mft_lcn;			/* Cluster location of mft data. */
	LCN mftmirr_lcn;		/* Cluster location of copy of mft. */
	int mftmirr_size;		/* Size of mft mirror in mft records. */
	LCN nr_clusters;		/* Volume size in clusters == number of
					   bits in lcn bitmap. */
} ntfs_volume;

typedef struct {
	MFT_RECORD *m;
	ATTR_RECORD *a;
} ntfs_attr_search_ctx;

typedef struct { /* In memory vcn to lcn mapping structure element. */
	VCN vcn;	/* vcn = Starting virtual cluster number. */
	LCN lcn;	/* lcn = Starting logical cluster number. */
	s64 length;	/* Run length in clusters. */
} ntfs_rl_element;

/* Runlist allocations happen in multiples of this value in bytes. */
#define NTFS_RL_ALLOC_BLOCK 1024

typedef enum {
	LCN_HOLE		= -1,	/* Keep this as highest value or die! */
	LCN_RL_NOT_MAPPED	= -2,
	LCN_ENOENT		= -3,
	LCN_ENOMEM		= -4,
	LCN_EIO			= -5,
} LCN_SPECIAL_VALUES;

static void usage(const char *progname) __attribute__((noreturn));
static void usage(const char *progname)
{
	fprintf(stderr, "usage: %s action_arg device_arg [mount_point_arg] [Flags]\n", progname);
	fprintf(stderr, "action_arg:\n");
	fprintf(stderr, "       -%c (Get UUID Key)\n", FSUC_GETUUID);
	fprintf(stderr, "       -%c (Mount)\n", FSUC_MOUNT);
	fprintf(stderr, "       -%c (Probe)\n", FSUC_PROBE);
	fprintf(stderr, "       -%c (Unmount)\n", FSUC_UNMOUNT);
	fprintf(stderr, "device_arg:\n");
	fprintf(stderr, "       device we are acting upon (for example, 'disk0s2')\n");
	fprintf(stderr, "mount_point_arg:\n");
	fprintf(stderr, "       required for Mount and Unmount\n");
	fprintf(stderr, "Flags:\n");
	fprintf(stderr, "       required for Mount and Probe\n");
	fprintf(stderr, "       indicates removable or fixed (for example 'fixed')\n");
	fprintf(stderr, "       indicates readonly or writable (for example 'readonly')\n");
	fprintf(stderr, "Flags (Mount only):\n");
	fprintf(stderr, "       indicates suid or nosuid (for example 'nosuid')\n");
	fprintf(stderr, "       indicates dev or nodev (for example 'nodev')\n");
	fprintf(stderr, "Examples:\n");
	fprintf(stderr, "       %s -p disk0s2 fixed writable\n", progname);
	fprintf(stderr, "       %s -m disk0s2 /my/hfs removable readonly nosuid nodev\n", progname);
	exit(FSUR_INVAL);
}

/**
 * ntfs_boot_sector_is_valid - check if @b contains a valid ntfs boot sector
 * @b:		Boot sector of device @mp to check.
 *
 * Check whether the boot sector @b is a valid ntfs boot sector.
 *
 * Return TRUE if it is valid and FALSE if not.
 *
 * Copied from ../kext/ntfs_vfsops.c.
 */
static BOOL ntfs_boot_sector_is_valid(const NTFS_BOOT_SECTOR *b)
{
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
	/* All checks passed, this boot sector is an NTFS boot sector. */
	return TRUE;
not_ntfs:
	return FALSE;
}

/**
 * ntfs_boot_sector_parse - parse the boot sector and store the data in @vol
 * @vol:	volume structure to initialise with data from boot sector
 * @b:		boot sector to parse
 *
 * Parse the ntfs boot sector @b and store all imporant information therein in
 * the ntfs_volume @vol.
 *
 * Return 0 on success and error code on error.  The following error codes are
 * defined:
 *	FSUR_UNRECOGNIZED - Boot sector is invalid/volume is not supported.
 *
 * Adapted from ../kext/ntfs_vfsops.c.
 */
static int ntfs_boot_sector_parse(ntfs_volume *vol, const NTFS_BOOT_SECTOR *b)
{
	s64 ll;
	int clusters_per_mft_record;

	vol->sector_size = le16_to_cpu(b->bpb.bytes_per_sector);
	vol->cluster_size = vol->sector_size * b->bpb.sectors_per_cluster;
	if (vol->cluster_size < vol->sector_size)
		return FSUR_UNRECOGNIZED;
	clusters_per_mft_record = b->clusters_per_mft_record;
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
	/*
	 * Get the size of the volume in clusters and check for 64-bit-ness.
	 * Windows currently only uses 32 bits to save the clusters so we do
	 * the same as we do not really want to break compatibility.  We could
	 * perhaps add a mount option to allow this one day but it would render
	 * such volumes incompatible with Windows.
	 */
	ll = sle64_to_cpu(b->number_of_sectors) / b->bpb.sectors_per_cluster;
	if ((u64)ll >= (u64)1 << 32)
		return FSUR_UNRECOGNIZED;
	vol->nr_clusters = ll;
	ll = sle64_to_cpu(b->mft_lcn);
	if (ll >= vol->nr_clusters)
		return FSUR_UNRECOGNIZED;
	vol->mft_lcn = ll;
	ll = sle64_to_cpu(b->mftmirr_lcn);
	if (ll >= vol->nr_clusters)
		return FSUR_UNRECOGNIZED;
	vol->mftmirr_lcn = ll;
	return 0;
}

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
 * Return 0 on success and FSUR_IO_FAIL on error ("BAAD" magic will be
 * present).
 *
 * NOTE: We consider the absence / invalidity of an update sequence array to
 * mean that the structure is not protected at all and hence does not need to
 * be fixed up.  Thus, we return success and not failure in this case.  This is
 * in contrast to ntfs_mst_fixup_pre_write(), see below.
 *
 * Adapted from ../kext/ntfs_mst.c.
 */
static int ntfs_mst_fixup_post_read(NTFS_RECORD *b, const u32 size)
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
			return FSUR_IO_FAIL;
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
 * ntfs_attr_search_ctx_init - initialize an attribute search context
 * @ctx:	attribute search context to initialize
 * @m:		mft record with which to initialize the search context
 *
 * Initialize the attribute search context @ctx with @m.
 *
 * Adapted from ../kext/ntfs_attr.c.
 */
static void ntfs_attr_search_ctx_init(ntfs_attr_search_ctx *ctx, MFT_RECORD *m)
{
	*ctx = (ntfs_attr_search_ctx) {
		.m = m,
		.a = (ATTR_RECORD*)((u8*)m + le16_to_cpu(m->attrs_offset)),
	};
}

/**
 * ntfs_attr_find_in_mft_record - find attribute in mft record
 * @type:	attribute type to find
 * @ctx:	search context with mft record and attribute to search from
 *
 * ntfs_attr_find_in_mft_record() takes a search context @ctx as parameter and
 * searches the mft record specified by @ctx->m, beginning at @ctx->a, for an
 * attribute of @type, the attribute must be unnamed.
 *
 * If the attribute is found, ntfs_attr_find_in_mft_record() returns 0 and
 * @ctx->a is set to point to the found attribute.
 *
 * If the attribute is not found, FSUR_UNRECOGNIZED is returned and @ctx->a is
 * set to point to the attribute before which the attribute being searched for
 * would need to be inserted if such an action were to be desired.
 *
 * On actual error, ntfs_attr_find_in_mft_record() returns FSUR_IO_FAIL.  In
 * this case @ctx->a is undefined and in particular do not rely on it not
 * having changed.
 *
 * Adapted from ../kext/ntfs_attr.c.
 */
static int ntfs_attr_find_in_mft_record(const ATTR_TYPE type,
		ntfs_attr_search_ctx *ctx)
{
	ATTR_RECORD *a;

	a = ctx->a;
	for (;;	a = (ATTR_RECORD*)((u8*)a + le32_to_cpu(a->length))) {
		if ((u8*)a < (u8*)ctx->m || (u8*)a > (u8*)ctx->m +
				le32_to_cpu(ctx->m->bytes_allocated))
			break;
		ctx->a = a;
		if (le32_to_cpu(a->type) > le32_to_cpu(type) ||
				a->type == AT_END)
			return FSUR_UNRECOGNIZED;
		if (!a->length)
			break;
		if (a->type != type)
			continue;
		/* The search failed if the found attribute is named. */
		if (a->name_length)
			return FSUR_UNRECOGNIZED;
		return 0;
	}
	return FSUR_IO_FAIL;
}

/**
 * ntfs_mapping_pairs_decompress - convert mapping pairs array to runlist
 * @vol:	ntfs volume on which the attribute resides
 * @a:		attribute record whose mapping pairs array to decompress
 * @runlist:	runlist in which to insert @a's runlist
 *
 * It is up to the caller to serialize access to the runlist @runlist.
 *
 * Decompress the attribute @a's mapping pairs array into a runlist.  On
 * success, return the decompressed runlist in @runlist.
 *
 * If @runlist already contains a runlist, the decompressed runlist is inserted
 * into the appropriate place in @runlist and the resultant, combined runlist
 * is returned in @runlist.
 *
 * On error, return error code (FSUR_UNRECOGNIZED, FSUR_IO_FAIL, or FSUR_INVAL).
 *
 * Adapted from ../kext/ntfs_runlist.c.
 */
static int ntfs_mapping_pairs_decompress(ntfs_volume *vol,
		const ATTR_RECORD *a, ntfs_rl_element **runlist)
{
	VCN vcn;		/* Current vcn. */
	LCN lcn;		/* Current lcn. */
	s64 deltaxcn;		/* Change in [vl]cn. */
	u8 *buf;		/* Current position in mapping pairs array. */
	u8 *a_end;		/* End of attribute. */
	ntfs_rl_element *rl;
	unsigned rlsize;	/* Size of runlist buffer. */
	int err;
	u16 rlpos;		/* Current runlist position in units of
				   runlist_elements. */
	u8 b;			/* Current byte offset in buf. */

	/* Make sure @a exists and is non-resident. */
	if (!a || !a->non_resident || sle64_to_cpu(a->lowest_vcn) < (VCN)0)
		return FSUR_INVAL;
	/* Start at vcn = lowest_vcn and lcn 0. */
	vcn = sle64_to_cpu(a->lowest_vcn);
	lcn = 0;
	/* Get start of the mapping pairs array. */
	buf = (u8*)a + le16_to_cpu(a->mapping_pairs_offset);
	a_end = (u8*)a + le32_to_cpu(a->length);
	if (buf < (u8*)a || buf > a_end)
		return FSUR_IO_FAIL;
	/* If the mapping pairs array is valid but empty, nothing to do. */
	if (!vcn && !*buf)
		return FSUR_UNRECOGNIZED;
	/* Current position in runlist array. */
	rlpos = 0;
	rlsize = NTFS_RL_ALLOC_BLOCK;
	/* Allocate NTFS_RL_ALLOC_BLOCK bytes for the runlist. */
	rl = malloc(rlsize);
	if (!rl)
		return FSUR_INVAL;
	/* Insert unmapped starting element if necessary. */
	if (vcn) {
		rl->vcn = 0;
		rl->lcn = LCN_RL_NOT_MAPPED;
		rl->length = vcn;
		rlpos++;
	}
	while (buf < a_end && *buf) {
		/*
		 * Allocate more memory if needed, including space for the
		 * not-mapped and terminator elements.
		 */
		if (((rlpos + 3) * sizeof(*rl)) > rlsize) {
			ntfs_rl_element *rl2;

			rl2 = malloc(rlsize + NTFS_RL_ALLOC_BLOCK);
			if (!rl2) {
				err = FSUR_INVAL;
				goto err;
			}
			memcpy(rl2, rl, rlsize);
			free(rl);
			rl = rl2;
			rlsize += NTFS_RL_ALLOC_BLOCK;
		}
		/* Enter the current vcn into the current runlist element. */
		rl[rlpos].vcn = vcn;
		/*
		 * Get the change in vcn, i.e. the run length in clusters.
		 * Doing it this way ensures that we sign-extend negative
		 * values.  A negative run length does not make any sense, but
		 * hey, I did not design NTFS...
		 */
		b = *buf & 0xf;
		if (b) {
			if (buf + b >= a_end) {
				err = FSUR_IO_FAIL;
				goto err;
			}
			for (deltaxcn = (s8)buf[b--]; b; b--)
				deltaxcn = (deltaxcn << 8) + buf[b];
		} else /* The length entry is compulsory. */
			deltaxcn = (s64)-1;
		/*
		 * Assume a negative length to indicate data corruption and
		 * hence clean-up and return NULL.
		 */
		if (deltaxcn < 0) {
			err = FSUR_IO_FAIL;
			goto err;
		}
		/*
		 * Enter the current run length into the current runlist
		 * element.
		 */
		rl[rlpos].length = deltaxcn;
		/* Increment the current vcn by the current run length. */
		vcn += deltaxcn;
		/*
		 * There might be no lcn change at all, as is the case for
		 * sparse clusters on NTFS 3.0+, in which case we set the lcn
		 * to LCN_HOLE.
		 */
		if (!(*buf & 0xf0))
			rl[rlpos].lcn = LCN_HOLE;
		else {
			/* Get the lcn change which really can be negative. */
			u8 b2 = *buf & 0xf;
			b = b2 + ((*buf >> 4) & 0xf);
			if (buf + b >= a_end) {
				err = FSUR_IO_FAIL;
				goto err;
			}
			for (deltaxcn = (s8)buf[b--]; b > b2; b--)
				deltaxcn = (deltaxcn << 8) + buf[b];
			/* Change the current lcn to its new value. */
			lcn += deltaxcn;
			/* Check lcn is not below -1. */
			if (lcn < (LCN)-1) {
				err = FSUR_IO_FAIL;
				goto err;
			}
			/* Enter the current lcn into the runlist element. */
			rl[rlpos].lcn = lcn;
		}
		/* Get to the next runlist element. */
		rlpos++;
		/* Increment the buffer position to the next mapping pair. */
		buf += (*buf & 0xf) + ((*buf >> 4) & 0xf) + 1;
	}
	if (buf >= a_end) {
		err = FSUR_IO_FAIL;
		goto err;
	}
	/*
	 * If there is a highest_vcn specified, it must be equal to the final
	 * vcn in the runlist - 1, or something has gone badly wrong.
	 */
	deltaxcn = sle64_to_cpu(a->highest_vcn);
	if (deltaxcn && vcn - 1 != deltaxcn) {
		err = FSUR_IO_FAIL;
		goto err;
	}
	/* Setup not mapped runlist element if this is the base extent. */
	if (!a->lowest_vcn) {
		VCN max_cluster;

		max_cluster = ((sle64_to_cpu(a->allocated_size) +
				vol->cluster_size - 1) / vol->cluster_size) - 1;
		/*
		 * A highest_vcn of zero means this is a single extent
		 * attribute so simply terminate the runlist with LCN_ENOENT).
		 */
		if (deltaxcn) {
			/*
			 * If there is a difference between the highest_vcn and
			 * the highest cluster, the runlist is either corrupt
			 * or, more likely, there are more extents following
			 * this one.
			 */
			if (deltaxcn < max_cluster) {
				rl[rlpos].vcn = vcn;
				vcn += rl[rlpos].length = max_cluster -
						deltaxcn;
				rl[rlpos].lcn = LCN_RL_NOT_MAPPED;
				rlpos++;
			} else if (deltaxcn > max_cluster) {
				err = FSUR_IO_FAIL;
				goto err;
			}
		}
		rl[rlpos].lcn = LCN_ENOENT;
	} else /* Not the base extent.  There may be more extents to follow. */
		rl[rlpos].lcn = LCN_RL_NOT_MAPPED;
	/* Setup terminating runlist element. */
	rl[rlpos].vcn = vcn;
	rl[rlpos].length = (s64)0;
	*runlist = rl;
	return 0;
err:
	free(rl);
	return err;
}

/**
 * ntfs_rl_vcn_to_lcn - convert a vcn into a lcn given a runlist
 * @rl:		runlist to use for conversion
 * @vcn:	vcn to convert
 * @clusters:	optional return pointer for the number of contiguous clusters
 *
 * Convert the virtual cluster number @vcn of an attribute into a logical
 * cluster number (lcn) of a device using the runlist @rl to map vcns to their
 * corresponding lcns.
 *
 * If @clusters is not NULL, on success (i.e. we return >= LCN_HOLE) we return
 * the number of contiguous clusters after the returned lcn in *@clusters.
 *
 * Since lcns must be >= 0, we use negative return codes with special meaning:
 *
 * Return code		Meaning / Description
 * ==================================================
 *  LCN_HOLE		Hole / not allocated on disk.
 *  LCN_RL_NOT_MAPPED	This is part of the runlist which has not been
 *			inserted into the runlist yet.
 *  LCN_ENOENT		There is no such vcn in the attribute.
 *
 * Locking: - The caller must have locked the runlist (for reading or writing).
 *	    - This function does not touch the lock.
 *	    - The runlist is not modified.
 *
 * Adapted from ../kext/ntfs_runlist.c.
 */
static LCN ntfs_rl_vcn_to_lcn(const ntfs_rl_element *rl, const VCN vcn,
		s64 *clusters)
{
	unsigned i;

	/*
	 * If @rl is NULL, assume that we have found an unmapped runlist.  The
	 * caller can then attempt to map it and fail appropriately if
	 * necessary.
	 */
	if (!rl)
		return LCN_RL_NOT_MAPPED;
	/* Catch out of lower bounds vcn. */
	if (vcn < rl[0].vcn)
		return LCN_ENOENT;
	for (i = 0; rl[i].length; i++) {
		if (vcn < rl[i + 1].vcn) {
			const s64 ofs = vcn - rl[i].vcn;
			if (clusters)
				*clusters = rl[i].length - ofs;
			if (rl[i].lcn >= (LCN)0)
				return rl[i].lcn + ofs;
			return rl[i].lcn;
		}
	}
	/*
	 * Set *@clusters just in case rl[i].lcn is LCN_HOLE.  That should
	 * never happen since the terminator element should never be of type
	 * LCN_HOLE but better be safe than sorry.
	 */
	if (clusters)
		*clusters = 0;
	/*
	 * The terminator element is setup to the correct value, i.e. one of
	 * LCN_HOLE, LCN_RL_NOT_MAPPED, or LCN_ENOENT.
	 */
	if (rl[i].lcn < (LCN)0)
		return rl[i].lcn;
	/* Just in case...  We could replace this with panic() some day. */
	return LCN_ENOENT;
}

/**
 * ntfs_pread - Read from a raw device file descriptor.
 * @f:			file descriptor to read from
 * @buf:		temporary buffer of size sector_size bytes
 * @sector_size:	sector size in bytes
 * @dst:		destination buffer to read into
 * @to_read:		how many bytes to read into the destination buffer
 * @ofs:		offset into the raw device at which to read
 *
 * We are working with the raw device thus we need to do sector aligned i/o.
 */
static ssize_t ntfs_pread(int f, u8 *buf, long sector_size, void *dst,
		ssize_t to_read, off_t ofs)
{
	off_t io_pos;
	ssize_t buf_ofs, to_copy, copied;

	copied = 0;
	while (to_read > 0) {
		io_pos = ofs & ~((off_t)sector_size - 1);
		buf_ofs = ofs & ((off_t)sector_size - 1);
		to_copy = sector_size - buf_ofs;
		if (to_copy > to_read)
			to_copy = to_read;
		/*
		 * Accept partial reads as long as they contain the data we
		 * want.
		 */
		if (pread(f, buf, sector_size, io_pos) < buf_ofs + to_copy) {
			if (!copied)
				copied = -1;
			break;
		}
		memcpy((u8*)dst + copied, buf + buf_ofs, to_copy);
		to_read -= to_copy;
		copied += to_copy;
	}
	return copied;
}

/**
 * get_volume_mft_record - Get the mft record for $Volume system file.
 * @rdev:	raw device containing NTFS volume
 * @vol:	ntfs_volume structure to set up describing the NTFS volume
 * @mrec:	pointer in which to return the mft record of $Volume
 *
 * Check whether the volume is an NTFS volume and if so load and return the
 * initialized ntfs_volume structure in @vol as well as the mft record for the
 * system file $Volume in @mrec and return FSUR_RECOGNIZED.  The caller is
 * responsible for freeing the returned mft record @mrec using free().
 *
 * If not an NTFS volume return FSUR_UNRECOGNIZED.
 *
 * On error return FSUR_INVAL or FSUR_IO_FAIL.
 */
static int get_volume_mft_record(char *rdev, ntfs_volume *vol,
		MFT_RECORD **mrec)
{
	VCN vcn;
	LCN lcn;
	s64 clusters, io_size;
	void *buf;
	NTFS_BOOT_SECTOR *bs;
	MFT_RECORD *m;
	ntfs_rl_element *rl;
	long sector_size;
	unsigned vcn_ofs;
	int f, err, to_read;
	u32 dev_block_size;
	ntfs_attr_search_ctx ctx;

	/*
	 * Read the boot sector.  We are working with the raw device thus we
	 * need to do sector aligned i/o.
	 *
	 * The maximum supported sector size for the NTFS driver is the system
	 * page size thus query the system for the page size and use that for
	 * the sector size.  If the querying fails then use 32768 which is the
	 * maximum value that can be set in the NTFS boot sector (it is stored
	 * in a 16-bit unsigned variable).
	 *
	 * The minumum sector size is the ntfs block size (512 bytes).
	 */
	sector_size = sysconf(_SC_PAGE_SIZE);
	if (sector_size < 0)
		sector_size = 32768;
	if (sector_size < NTFS_BLOCK_SIZE)
		sector_size = NTFS_BLOCK_SIZE;
	f = open(rdev, O_RDONLY);
	if (f == -1) {
		return FSUR_IO_FAIL;
	}
	/*
	 * Get the native block size of the device.  If it is bigger than
	 * @sector_size we need to do i/o in multiples of the native block
	 * size.
	 */
	if (ioctl(f, DKIOCGETBLOCKSIZE, &dev_block_size) < 0) {
		err = FSUR_IO_FAIL;
		buf = NULL;
		goto err;
	}
	if (dev_block_size > (u32)sector_size)
		sector_size = dev_block_size;
	buf = malloc(sector_size);
	if (!buf) {
		err = FSUR_IO_FAIL;
		goto err;
	}
	/*
	 * We can cope with partial reads as long as we have at least the boot
	 * sector which fits inside the first NTFS_BLOCK_SIZE bytes.
	 */
	if (read(f, buf, sector_size) < NTFS_BLOCK_SIZE) {
		err = FSUR_IO_FAIL;
		goto err;
	}
	/* Check if the boot sector is a valid NTFS boot sector. */
	bs = buf;
	if (!ntfs_boot_sector_is_valid(bs)) {
		err = FSUR_UNRECOGNIZED;
		goto err;
	}
	/* Parse the boot sector and initialize @vol with its information. */
	err = ntfs_boot_sector_parse(vol, bs);
	if (err)
		goto err;
	m = malloc(vol->mft_record_size);
	if (!m) {
		err = FSUR_INVAL;
		goto err;
	}
	/* Load the mft record for $MFT. */
	if (ntfs_pread(f, buf, sector_size, m, vol->mft_record_size,
			vol->mft_lcn * vol->cluster_size) !=
			(ssize_t)vol->mft_record_size) {
		err = FSUR_IO_FAIL;
		goto free_err;
	}
	/* Apply the multi sector transfer protection fixups. */
	err = ntfs_mst_fixup_post_read((NTFS_RECORD*)m, vol->mft_record_size);
	if (err) {
		err = FSUR_IO_FAIL;
		goto free_err;
	}
	/* Lookup $DATA attribute. */
	ntfs_attr_search_ctx_init(&ctx, m);
	err = ntfs_attr_find_in_mft_record(AT_DATA, &ctx);
	if (err)
		goto free_err;
	/* Decompress the mapping pairs array of the attribute. */
	rl = NULL;
	err = ntfs_mapping_pairs_decompress(vol, ctx.a, &rl);
	if (err)
		goto free_err;
	vcn = FILE_Volume * vol->mft_record_size;
	vcn_ofs = vcn & (vol->cluster_size - 1);
	vcn /= vol->cluster_size;
	to_read = vol->mft_record_size;
	/*
	 * Determine location of $Volume mft record from mft data runlist and
	 * read it into memory.  We can safely assume that the first fragment
	 * of $DATA must specify $Volume's location or the volume would not be
	 * boot strappable.
	 */
	do {
		lcn = ntfs_rl_vcn_to_lcn(rl, vcn, &clusters);
		if (lcn < 0) {
			err = FSUR_IO_FAIL;
			goto rl_free_err;
		}
		io_size = (clusters * vol->cluster_size) - vcn_ofs;
		if (io_size > to_read)
			io_size = to_read;
		if (ntfs_pread(f, buf, sector_size, m, io_size, (lcn *
				vol->cluster_size) + vcn_ofs) != io_size) {
			err = FSUR_IO_FAIL;
			goto rl_free_err;
		}
		to_read -= io_size;
		vcn += clusters;
		vcn_ofs = 0;
	} while (to_read > 0);
	free(rl);
	/* Apply the multi sector transfer protection fixups. */
	err = ntfs_mst_fixup_post_read((NTFS_RECORD*)m, vol->mft_record_size);
	if (err) {
		err = FSUR_IO_FAIL;
		goto free_err;
	}
	(void)close(f);
	free(buf);
	/* Finally got the mft record for $Volume. */
	*mrec = m;
	return FSUR_RECOGNIZED;
rl_free_err:
	free(rl);
free_err:
	free(m);
err:
	if (buf)
		free(buf);
	(void)close(f);
	return err;
}

/**
 * do_getuuid - Get the UUID key of an NTFS volume.
 *
 * If the volume is recognized as an NTFS volume look up its UUID key if it
 * exists and output it to stdout then return FSUR_RECOGNIZED.
 *
 * If there is no UUID then return FSUR_INVAL and do not output anything to
 * stdout.
 *
 * If the volume is not an NTFS volume return FSUR_UNRECOGNIZED.
 *
 * On error return FSUR_INVAL or FSUR_IO_FAIL.
 */
static int do_getuuid(char *rdev)
{
	MFT_RECORD *m;
	ATTR_RECORD *a;
	OBJECT_ID_ATTR *obj_id;
	GUID *guid;
	unsigned obj_id_len;
	int err;
	ntfs_volume vol;
	ntfs_attr_search_ctx ctx;
	char uuid[37];

	/* Obtain the mft record for $Volume. */
	err = get_volume_mft_record(rdev, &vol, &m);
	if (err != FSUR_RECOGNIZED)
		goto err;
	/* Lookup $OBJECT_ID attribute. */
	ntfs_attr_search_ctx_init(&ctx, m);
	err = ntfs_attr_find_in_mft_record(AT_OBJECT_ID, &ctx);
	if (err) {
		if (err != FSUR_UNRECOGNIZED)
			goto free_err;
		/* There is no volume UUID key which is fine. */
#ifdef NTFS_UTIL_DEBUG
		/* Log to syslog. */
		openlog("ntfs.util", LOG_PID, LOG_DAEMON);
		syslog(LOG_NOTICE, "Volume does not have a UUID key.\n");
		closelog();
#endif /* NTFS_UTIL_DEBUG */
		err = FSUR_INVAL;
		goto free_err;
	}
	a = ctx.a;
	if (a->non_resident) {
		err = FSUR_IO_FAIL;
		goto free_err;
	}
	obj_id = (OBJECT_ID_ATTR*)((u8*)a + le16_to_cpu(a->value_offset));
	obj_id_len = le32_to_cpu(a->value_length);
	if ((u8*)obj_id + obj_id_len > (u8*)m + vol.mft_record_size ||
			(u8*)obj_id + obj_id_len > (u8*)a +
			le32_to_cpu(a->length)) {
		err = FSUR_IO_FAIL;
		goto free_err;
	}
	guid = &obj_id->object_id;
	/* Convert guid to utf8 uuid. */
	if (snprintf(uuid, 37, "%08x-%04x-%04x-%02x%02x-"
			"%02x%02x%02x%02x%02x%02x", le32_to_cpu(guid->data1),
			le16_to_cpu(guid->data2), le16_to_cpu(guid->data3),
			guid->data4[0], guid->data4[1], guid->data4[2],
			guid->data4[3], guid->data4[4], guid->data4[5],
			guid->data4[6], guid->data4[7]) != 36) {
		err = FSUR_IO_FAIL;
		goto free_err;
	}
	/* Output utf8 uuid to stdout. */
#ifdef NTFS_UTIL_DEBUG
	/* Log to syslog. */
	openlog("ntfs.util", LOG_PID, LOG_DAEMON);
	syslog(LOG_NOTICE, "Volume UUID Key: %s\n", uuid);
	closelog();
#endif /* NTFS_UTIL_DEBUG */
	/*
	 * If this fails it is not a problem and there is nothing wrong with
	 * the volume so ignore the return value.
	 */
	(void)write(STDOUT_FILENO, uuid, 36);
	/* Finally done! */
	err = FSUR_IO_SUCCESS;
free_err:
	free(m);
err:
	return err;
}

/*
 * Invalid NTFS filename characters are encodeded using the
 * SFM (Services for Macintosh) private use Unicode characters.
 *
 * These should only be used for SMB, MSDOS or NTFS.
 *
 *    Illegal NTFS Char   SFM Unicode Char
 *  ----------------------------------------
 *    0x01-0x1f           0xf001-0xf01f
 *    '"'                 0xf020
 *    '*'                 0xf021
 *    '/'                 0xf022
 *    '<'                 0xf023
 *    '>'                 0xf024
 *    '?'                 0xf025
 *    '\'                 0xf026
 *    '|'                 0xf027
 *    ' '                 0xf028  (Only if last char of the name)
 *    '.'                 0xf029  (Only if last char of the name)
 *  ----------------------------------------
 *
 *  Reference: http://support.microsoft.com/kb/q117258/
 */

/*
 * In the Mac OS 9 days the colon was illegal in a file name. For that reason
 * SFM had no conversion for the colon. There is a conversion for the
 * slash. In Mac OS X the slash is illegal in a file name. So for us the colon
 * is a slash and a slash is a colon. So we can just replace the slash with the
 * colon in our tables and everything will just work. 
 *
 * SFM conversion code adapted from xnu/bsd/vfs/vfs_utfconf.c.
 */
static u8 sfm2mac[0x30] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,	/* 00 - 07 */
	0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,	/* 08 - 0F */
	0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17,	/* 10 - 17 */
	0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f,	/* 18 - 1F */
	0x22, 0x2a, 0x3a, 0x3c, 0x3e, 0x3f, 0x5c, 0x7c,	/* 20 - 27 */
	0x20, 0x2e					/* 28 - 29 */
};

/**
 * do_probe - Examine a volume to see if we recognize it as an NTFS volume.
 *
 * If the volume is recognized as an NTFS volume look up its volume label and
 * output it to stdout then return FSUR_RECOGNIZED.
 *
 * If the volume is not an NTFS volume return FSUR_UNRECOGNIZED.
 *
 * On error return FSUR_INVAL or FSUR_IO_FAIL.
 */
static int do_probe(char *rdev, const BOOL removable __attribute__((unused)),
		const BOOL writable __attribute__((unused)))
{
	MFT_RECORD *m;
	ATTR_RECORD *a;
	ntfschar *uname;
	CFStringRef s;
	char *label;
	size_t label_len;
	unsigned uname_len, i;
	int err;
	CFIndex label_size;
	ntfs_volume vol;
	ntfs_attr_search_ctx ctx;

	/* Obtain the mft record for $Volume. */
	err = get_volume_mft_record(rdev, &vol, &m);
	if (err != FSUR_RECOGNIZED)
		goto err;
	/* Lookup $VOLUME_NAME attribute. */
	ntfs_attr_search_ctx_init(&ctx, m);
	err = ntfs_attr_find_in_mft_record(AT_VOLUME_NAME, &ctx);
	if (err) {
		if (err != FSUR_UNRECOGNIZED)
			goto free_err;
		/* There is no volume label which is fine. */
#ifdef NTFS_UTIL_DEBUG
		/* Log to syslog. */
		openlog("ntfs.util", LOG_PID, LOG_DAEMON);
		syslog(LOG_NOTICE, "Volume is not labelled.\n");
		closelog();
#endif /* NTFS_UTIL_DEBUG */
		err = FSUR_RECOGNIZED;
		goto free_err;
	}
	a = ctx.a;
	if (a->non_resident) {
		err = FSUR_IO_FAIL;
		goto free_err;
	}
	uname = (ntfschar*)((u8*)a + le16_to_cpu(a->value_offset));
	uname_len = le32_to_cpu(a->value_length);
	if ((u8*)uname + uname_len > (u8*)m + vol.mft_record_size ||
			(u8*)uname + uname_len > (u8*)a +
			le32_to_cpu(a->length)) {
		err = FSUR_IO_FAIL;
		goto free_err;
	}
	/* Convert length to number of Unicode characters. */
	uname_len /= sizeof(ntfschar);
	/*
	 * Scan through the name looking for any Services For Macintosh encoded
	 * characters and if any are found replace them with the decoded ones.
	 * We need to do this as the Core Foundation string handling routines
	 * are not aware of the SFM encodings.
	 */
	for (i = 0; i < uname_len; i++) {
		ntfschar c;
		
		c = le16_to_cpu(uname[i]);
		if ((c & 0xffc0) == 0xf000) {
			c &= 0x003f;
			if (c <= 0x29)
				uname[i] = cpu_to_le16(sfm2mac[c]);
		}
	}
	/* Convert volume name to utf8. */
	s = CFStringCreateWithBytes(kCFAllocatorDefault, (UInt8*)uname,
			uname_len * sizeof(ntfschar), kCFStringEncodingUTF16LE,
			true);
	if (!s) {
		err = FSUR_IO_FAIL;
		goto free_err;
	}
	label_size = CFStringGetMaximumSizeOfFileSystemRepresentation(s);
	label = malloc(label_size);
	if (!label) {
		err = FSUR_IO_FAIL;
		goto s_free_err;
	}
	if (!CFStringGetFileSystemRepresentation(s, label, label_size)) {
		err = FSUR_IO_FAIL;
		goto label_free_err;
	}
	/* Output volume label (now in utf8) to stdout. */
	label_len = strlen(label);
	if (label_len > (size_t)label_size) {
		err = FSUR_IO_FAIL;
		goto label_free_err;
	}
#ifdef NTFS_UTIL_DEBUG
	/* Log to syslog. */
	openlog("ntfs.util", LOG_PID, LOG_DAEMON);
	syslog(LOG_NOTICE, "Volume label: %s\n", label);
	closelog();
#endif /* NTFS_UTIL_DEBUG */
	/*
	 * If this fails it is not a problem and there is nothing wrong with
	 * the volume so ignore the return value.
	 */
	(void)write(STDOUT_FILENO, label, label_len);
	/* Finally done! */
	err = FSUR_RECOGNIZED;
label_free_err:
	free(label);
s_free_err:
	CFRelease(s);
free_err:
	free(m);
err:
	return err;
}

/**
 * do_exec - Execute an external command.
 */
static int do_exec(const char *progname, char *const args[])
{
	pid_t pid;
	union wait status;
	int err;

	pid = fork();
	if (pid == -1) {
		fprintf(stderr, "%s: fork failed: %s\n", progname,
				strerror(errno));
		return FSUR_INVAL;
	}
	if (!pid) {
		/* In child process, execute external command. */
		(void)execv(args[0], args);
		/* We only get here if the execv() failed. */
		err = errno;
		fprintf(stderr, "%s: execv %s failed: %s\n", progname, args[0],
				strerror(err));
		exit(err);
	}
	/* In parent process, wait for exernal command to finish. */
	if (wait4(pid, (int*)&status, 0, NULL) != pid) {
		fprintf(stderr, "%s: BUG executing %s command.\n", progname,
				args[0]);
		return FSUR_INVAL;
	}
	if (!WIFEXITED(status)) {
		fprintf(stderr, "%s: %s command aborted by signal %d.\n",
				progname, args[0], WTERMSIG(status));
		return FSUR_INVAL;
	}
	err = WEXITSTATUS(status);
	if (err) {
		fprintf(stderr, "%s: %s command failed: %s\n", progname,
				args[0], strerror(err));
		return FSUR_IO_FAIL;
	}
	return FSUR_IO_SUCCESS;
}

/**
 * do_mount - Mount a file system.
 */
static int do_mount(const char *progname, char *dev, char *mp,
		const BOOL removable __attribute__((unused)),
		const BOOL readonly, const BOOL nosuid, const BOOL nodev)
{
	char *const kextargs[] = { "/sbin/kextload",
			"/System/Library/Extensions/ntfs.kext", NULL };
	char *mountargs[] = { "/sbin/mount", "-w", "-o",
			"suid", "-o", "dev", "-t", "ntfs", dev, mp, NULL };
	struct vfsconf vfc;

	if (!mp || !strlen(mp))
		return FSUR_INVAL;
	if (readonly)
		mountargs[1] = "-r";
	if (nosuid)
		mountargs[3] = "nosuid";
	if (nodev)
		mountargs[5] = "nodev";
	/*
	 * If the kext is not loaded, load it now.  Ignore any errors as the
	 * mount will fail appropriately if the kext is not loaded.
	 */
	if (getvfsbyname("ntfs", &vfc))
		(void)do_exec(progname, kextargs);
	return do_exec(progname, mountargs);
}

/**
 * do_unmount - Unmount a volume.
 */
static int do_unmount(const char *progname, char *mp)
{
	char *const umountargs[] = { "/sbin/umount", mp, NULL };

	if (!mp || !strlen(mp))
		return FSUR_INVAL;
	return do_exec(progname, umountargs);
}

/**
 * main - Main function, parse arguments and cause required action to be taken.
 */
int main(int argc, char **argv)
{
	char *progname, *dev, *mp = NULL;
	int err;
	char opt;
	BOOL removable, readonly, nosuid, nodev;
	char rawdev[MAXPATHLEN];
	char blockdev[MAXPATHLEN];
	struct stat sb;

	nodev = nosuid = readonly = removable = FALSE;
	/* Save & strip off program name. */
	progname = argv[0];
	argc--;
	argv++;
#ifdef NTFS_UTIL_DEBUG
	/* Log to syslog. */
	openlog("ntfs.util", LOG_PID, LOG_DAEMON);
	if (argc == 0)
		syslog(LOG_NOTICE, "Called without arguments!");
	else if (argc == 1)
		syslog(LOG_NOTICE, "Called with %d arguments: %s", argc,
				argv[0]);
	else if (argc == 2)
		syslog(LOG_NOTICE, "Called with %d arguments: %s %s", argc,
				argv[0], argv[1]);
	else if (argc == 3)
		syslog(LOG_NOTICE, "Called with %d arguments: %s %s %s", argc,
				argv[0], argv[1], argv[2]);
	else if (argc == 4)
		syslog(LOG_NOTICE, "Called with %d arguments: %s %s %s %s",
				argc, argv[0], argv[1], argv[2], argv[3]);
	else if (argc == 5)
		syslog(LOG_NOTICE, "Called with %d arguments: %s %s %s %s %s",
				argc, argv[0], argv[1], argv[2], argv[3],
				argv[4]);
	else if (argc == 6)
		syslog(LOG_NOTICE, "Called with %d arguments: %s %s %s %s %s "
				"%s", argc, argv[0], argv[1], argv[2], argv[3],
				argv[4], argv[5]);
	else
		syslog(LOG_NOTICE, "Called with %d arguments: %s %s %s %s %s "
				"%s %s", argc, argv[0], argv[1], argv[2],
				argv[3], argv[4], argv[5], argv[6]);
	closelog();
#endif /* NTFS_UTIL_DEBUG */
	/*
	 * We support probe, mount, and unmount all of which need the command
	 * option and the device.
	 */
	if (argc < 2 || argv[0][0] != '-')
		usage(progname);
	opt = argv[0][1];
	dev = argv[1];
	argc -= 2;
	argv += 2;
	/* Check we have the right number of arguments. */
	switch (opt) {
	case FSUC_GETUUID:
		/* For get UUID key do not need any more arguments. */
		if (argc)
			usage(progname);
		break;
	case FSUC_PROBE:
		/* For probe need the two mountflags also. */
		if (argc != 2)
			usage(progname);
		break;
	case FSUC_MOUNT:
		/* For mount need the mount point and four mountflags also. */
		if (argc != 5)
			usage(progname);
		break;
	case FSUC_UNMOUNT:
		/* For unmount need the mount point also. */
		if (argc != 1)
			usage(progname);
		break;
	default:
		/* Unsupported command. */
		usage(progname);
		break;
	}
	/* Check the raw and block device special files exist. */
	err = snprintf(rawdev, sizeof(rawdev), "/dev/r%s", dev);
	if (err >= (int)sizeof(rawdev)) {
		fprintf(stderr, "%s: Specified device name is too long.\n",
				progname);
		exit(FSUR_INVAL);
	}
	if (stat(rawdev, &sb)) {
		fprintf(stderr, "%s: stat %s failed, %s\n", progname, rawdev,
				strerror(errno));
		exit(FSUR_INVAL);
	}
	err = snprintf(blockdev, sizeof(blockdev), "/dev/%s", dev);
	if (err >= (int)sizeof(blockdev)) {
		fprintf(stderr, "%s: Specified device name is too long.\n",
				progname);
		exit(FSUR_INVAL);
	}
	if (stat(blockdev, &sb)) {
		fprintf(stderr, "%s: stat %s failed, %s\n", progname, blockdev,
				strerror(errno));
		exit(FSUR_INVAL);
	}
	/* Get the mount point for the mount and unmount cases. */
	if (opt == FSUC_MOUNT || opt == FSUC_UNMOUNT) {
		mp = argv[0];
		argc--;
		argv++;
	}
	/* Get the mount flags for the probe and mount cases. */
	if (opt == FSUC_PROBE || opt == FSUC_MOUNT) {
		/* mountflag1: Removable or fixed. */
		if (!strcmp(argv[0], DEVICE_REMOVABLE))
			removable = TRUE;
		else if (strcmp(argv[0], DEVICE_FIXED))
			usage(progname);
		/* mountflag2: Readonly or writable. */
		if (!strcmp(argv[1], DEVICE_READONLY))
			readonly = TRUE;
		else if (strcmp(argv[1], DEVICE_WRITABLE))
			usage(progname);
		/* Only the mount command supports the third and fourth flag. */
		if (opt == FSUC_MOUNT) {
			/* mountflag3: Nosuid or suid. */
			if (!strcmp(argv[2], "nosuid"))
				nosuid = TRUE;
			else if (strcmp(argv[2], "suid"))
				usage(progname);
			/* mountflag4: Nodev or dev. */
			if (!strcmp(argv[3], "nodev"))
				nodev = TRUE;
			else if (strcmp(argv[3], "dev"))
				usage(progname);
		}
	}
	/* Finally execute the required command. */
	switch (opt) {
	case FSUC_GETUUID:
		err = do_getuuid(rawdev);
		break;
	case FSUC_PROBE:
		err = do_probe(rawdev, removable, readonly);
		break;
	case FSUC_MOUNT:
		err = do_mount(progname, blockdev, mp, removable, readonly,
				nosuid, nodev);
		break;
	case FSUC_UNMOUNT:
		err = do_unmount(progname, mp);
		break;
	default:
		/* Cannot happen... */
		usage(progname);
		break;
	}
	return err;
}
