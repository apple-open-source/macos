/*
 * bootsect.h - Exports for bootsector record handling. 
 *
 * Copyright (c) 2000-2002 Anton Altaparmakov
 * Copyright (c) 2006 Szabolcs Szakacsits
 * Copyright (c) 2008-2012 Tuxera Inc.
 *
 * See LICENSE file for licensing information.
 */

#ifndef _NTFS_BOOTSECT_H
#define _NTFS_BOOTSECT_H

#include "types.h"
#include "layout.h"

/**
 * ntfs_boot_sector_is_ntfs - check a boot sector for describing an ntfs volume
 * @b:		buffer containing the boot sector
 *
 * This function checks the boot sector in @b for describing a valid ntfs
 * volume. Return TRUE if @b is a valid NTFS boot sector or FALSE otherwise.
 */
extern BOOL ntfs_boot_sector_is_ntfs(NTFS_BOOT_SECTOR *b);

#endif /* defined _NTFS_BOOTSECT_H */

