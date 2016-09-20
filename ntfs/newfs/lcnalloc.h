/*
 * lcnalloc.h - Exports for cluster (de)allocation. 
 *
 * Copyright (c) 2002 Anton Altaparmakov
 * Copyright (c) 2004 Yura Pakhuchiy
 * Copyright (c) 2008-2012 Tuxera Inc.
 *
 * See LICENSE file for licensing information.
 */

#ifndef _NTFS_LCNALLOC_H
#define _NTFS_LCNALLOC_H

#include "types.h"
#include "runlist.h"
#include "volume.h"

/**
 * enum NTFS_CLUSTER_ALLOCATION_ZONES -
 */
typedef enum {
	FIRST_ZONE	= 0,	/* For sanity checking. */
	MFT_ZONE	= 0,	/* Allocate from $MFT zone. */
	DATA_ZONE	= 1,	/* Allocate from $DATA zone. */
	LAST_ZONE	= 1,	/* For sanity checking. */
} NTFS_CLUSTER_ALLOCATION_ZONES;

extern runlist *ntfs_cluster_alloc(ntfs_volume *vol, VCN start_vcn, s64 count,
		LCN start_lcn, const NTFS_CLUSTER_ALLOCATION_ZONES zone);

extern int ntfs_cluster_free_from_rl(ntfs_volume *vol, runlist *rl);
extern int ntfs_cluster_free_basic(ntfs_volume *vol, s64 lcn, s64 count);

extern int ntfs_cluster_free(ntfs_volume *vol, ntfs_attr *na, VCN start_vcn,
		s64 count);

#endif /* defined _NTFS_LCNALLOC_H */
