/*
 * mst.h - Exports for multi sector transfer fixup functions.
 *
 * Copyright (c) 2000-2002 Anton Altaparmakov
 * Copyright (c) 2008-2012 Tuxera Inc.
 *
 * See LICENSE file for licensing information.
 */

#ifndef _NTFS_MST_H
#define _NTFS_MST_H

#include "types.h"
#include "layout.h"
#include "volume.h"

extern int ntfs_mst_post_read_fixup_warn(NTFS_RECORD *b, const u32 size,
					BOOL warn);
extern int ntfs_mst_pre_write_fixup(NTFS_RECORD *b, const u32 size);
extern void ntfs_mst_post_write_fixup(NTFS_RECORD *b);

#endif /* defined _NTFS_MST_H */
