/*
 * param.h - Parameter values for ntfs-3g
 *
 * Copyright (c) 2009-2010 Jean-Pierre Andre
 * Copyright (c) 2008-2012 Tuxera Inc.
 *
 * See LICENSE file for licensing information.
 */

#ifndef _NTFS_PARAM_H
#define _NTFS_PARAM_H

/*
 *		Parameters for compression
 */

	/* (log2 of) number of clusters in a compression block for new files */
#define STANDARD_COMPRESSION_UNIT 4
	/* maximum cluster size for allowing compression for new files */
#define MAX_COMPRESSION_CLUSTER_SIZE 4096

/*
 *		Parameters for runlists
 */

	/* only update the final extent of a runlist when appending data */
#define PARTIAL_RUNLIST_UPDATING 1

#endif /* defined _NTFS_PARAM_H */
