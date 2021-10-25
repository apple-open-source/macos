/*
 * attrlist.h - Exports for attribute list attribute handling.  
 *
 * Copyright (c) 2004 Anton Altaparmakov
 * Copyright (c) 2004 Yura Pakhuchiy
 * Copyright (c) 2008-2012 Tuxera Inc.
 *
 * See LICENSE file for licensing information.
 */

#ifndef _NTFS_ATTRLIST_H
#define _NTFS_ATTRLIST_H

#include "attrib.h"

extern int ntfs_attrlist_need(ntfs_inode *ni);

extern int ntfs_attrlist_entry_add(ntfs_inode *ni, ATTR_RECORD *attr);
extern int ntfs_attrlist_entry_rm(ntfs_attr_search_ctx *ctx);

/**
 * ntfs_attrlist_mark_dirty - set the attribute list dirty
 * @ni:		ntfs inode which base inode contain dirty attribute list
 *
 * Set the attribute list dirty so it is written out later (at the latest at
 * ntfs_inode_close() time).
 *
 * This function cannot fail.
 */
static __inline__ void ntfs_attrlist_mark_dirty(ntfs_inode *ni)
{
	if (ni->nr_extents == -1)
		NInoAttrListSetDirty(ni->base_ni);
	else
		NInoAttrListSetDirty(ni);
}

#endif /* defined _NTFS_ATTRLIST_H */
