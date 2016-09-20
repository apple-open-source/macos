/*
 * collate.h - Defines for NTFS collation handling. 
 *
 * Copyright (c) 2004 Anton Altaparmakov
 * Copyright (c) 2005 Yura Pakhuchiy
 * Copyright (c) 2008-2012 Tuxera Inc.
 *
 * See LICENSE file for licensing information.
 */

#ifndef _NTFS_COLLATE_H
#define _NTFS_COLLATE_H

#include "types.h"
#include "volume.h"

#define NTFS_COLLATION_ERROR -2

extern COLLATE ntfs_get_collate_function(COLLATION_RULES);

#endif /* _NTFS_COLLATE_H */
