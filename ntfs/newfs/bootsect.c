/**
 * bootsect.c - Boot sector handling code.
 *
 * Copyright (c) 2000-2006 Anton Altaparmakov
 * Copyright (c) 2003-2008 Szabolcs Szakacsits
 * Copyright (c)      2005 Yura Pakhuchiy
 * Copyright (c) 2008-2012 Tuxera Inc.
 *
 * See LICENSE file for licensing information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "compat.h"
#include "bootsect.h"
#include "debug.h"
#include "logging.h"

/**
 * ntfs_boot_sector_is_ntfs - check if buffer contains a valid ntfs boot sector
 * @b:		buffer containing putative boot sector to analyze
 * @silent:	if zero, output progress messages to stderr
 *
 * Check if the buffer @b contains a valid ntfs boot sector. The buffer @b
 * must be at least 512 bytes in size.
 *
 * If @silent is zero, output progress messages to stderr. Otherwise, do not
 * output any messages (except when configured with --enable-debug in which
 * case warning/debug messages may be displayed).
 *
 * Return TRUE if @b contains a valid ntfs boot sector and FALSE if not.
 */
BOOL ntfs_boot_sector_is_ntfs(NTFS_BOOT_SECTOR *b)
{
	u32 i;
	BOOL ret = FALSE;

	ntfs_log_debug("Beginning bootsector check.\n");

	ntfs_log_debug("Checking OEMid, NTFS signature.\n");
	if (b->oem_id != cpu_to_le64(0x202020205346544eULL)) { /* "NTFS    " */
		ntfs_log_error("NTFS signature is missing.\n");
		goto not_ntfs;
	}

	ntfs_log_debug("Checking bytes per sector.\n");
	if (le16_to_cpu(b->bpb.bytes_per_sector) <  256 ||
	    le16_to_cpu(b->bpb.bytes_per_sector) > 4096) {
		ntfs_log_error("Unexpected bytes per sector value (%d).\n", 
			       le16_to_cpu(b->bpb.bytes_per_sector));
		goto not_ntfs;
	}

	ntfs_log_debug("Checking sectors per cluster.\n");
	switch (b->bpb.sectors_per_cluster) {
	case 1: case 2: case 4: case 8: case 16: case 32: case 64: case 128:
		break;
	default:
		ntfs_log_error("Unexpected sectors per cluster value (%d).\n",
			       b->bpb.sectors_per_cluster);
		goto not_ntfs;
	}

	ntfs_log_debug("Checking cluster size.\n");
	i = (u32)le16_to_cpu(b->bpb.bytes_per_sector) * 
		b->bpb.sectors_per_cluster;
	if (i > 65536) {
		ntfs_log_error("Unexpected cluster size (%d).\n", i);
		goto not_ntfs;
	}

	ntfs_log_debug("Checking reserved fields are zero.\n");
	if (le16_to_cpu(b->bpb.reserved_sectors) ||
	    le16_to_cpu(b->bpb.root_entries) ||
	    le16_to_cpu(b->bpb.sectors) ||
	    le16_to_cpu(b->bpb.sectors_per_fat) ||
	    le32_to_cpu(b->bpb.large_sectors) ||
	    b->bpb.fats) {
		ntfs_log_error("Reserved fields aren't zero "
			       "(%d, %d, %d, %d, %d, %d).\n",
			       le16_to_cpu(b->bpb.reserved_sectors),
			       le16_to_cpu(b->bpb.root_entries),
			       le16_to_cpu(b->bpb.sectors),
			       le16_to_cpu(b->bpb.sectors_per_fat),
			       le32_to_cpu(b->bpb.large_sectors),
			       b->bpb.fats);
		goto not_ntfs;
	}

	ntfs_log_debug("Checking clusters per mft record.\n");
	if ((u8)b->clusters_per_mft_record < 0xe1 ||
	    (u8)b->clusters_per_mft_record > 0xf7) {
		switch (b->clusters_per_mft_record) {
		case 1: case 2: case 4: case 8: case 0x10: case 0x20: case 0x40:
			break;
		default:
			ntfs_log_error("Unexpected clusters per mft record "
				       "(%d).\n", b->clusters_per_mft_record);
			goto not_ntfs;
		}
	}

	ntfs_log_debug("Checking clusters per index block.\n");
	if ((u8)b->clusters_per_index_record < 0xe1 ||
	    (u8)b->clusters_per_index_record > 0xf7) {
		switch (b->clusters_per_index_record) {
		case 1: case 2: case 4: case 8: case 0x10: case 0x20: case 0x40:
			break;
		default:
			ntfs_log_error("Unexpected clusters per index record "
				       "(%d).\n", b->clusters_per_index_record);
			goto not_ntfs;
		}
	}

	if (b->end_of_sector_marker != cpu_to_le16(0xaa55))
		ntfs_log_debug("Warning: Bootsector has invalid end of sector "
			       "marker.\n");

	ntfs_log_debug("Bootsector check completed successfully.\n");

	ret = TRUE;
not_ntfs:
	return ret;
}
