/**
 * debug.c - Debugging output functions. 
 *
 * Copyright (c) 2002-2004 Anton Altaparmakov
 * Copyright (c) 2004-2006 Szabolcs Szakacsits
 * Copyright (c) 2008-2012 Tuxera Inc.
 *
 * See LICENSE file for licensing information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "types.h"
#include "runlist.h"
#include "debug.h"
#include "logging.h"

#ifdef DEBUG
/**
 * ntfs_debug_runlist_dump - Dump a runlist.
 * @rl:
 *
 * Description...
 *
 * Returns:
 */
void ntfs_debug_runlist_dump(const runlist_element *rl)
{
	int i = 0;
	const char *lcn_str[5] = { "LCN_HOLE         ", "LCN_RL_NOT_MAPPED",
				   "LCN_ENOENT       ", "LCN_EINVAL       ",
				   "LCN_unknown      " };

	ntfs_log_debug("NTFS-fs DEBUG: Dumping runlist (values in hex):\n");
	if (!rl) {
		ntfs_log_debug("Run list not present.\n");
		return;
	}
	ntfs_log_debug("VCN              LCN               Run length\n");
	do {
		LCN lcn = (rl + i)->lcn;

		if (lcn < (LCN)0) {
			int idx = -lcn - 1;

			if (idx > -LCN_EINVAL - 1)
				idx = 4;
			ntfs_log_debug("%-16lld %s %-16lld%s\n", 
				       (long long)rl[i].vcn, lcn_str[idx], 
				       (long long)rl[i].length, 
				       rl[i].length ? "" : " (runlist end)");
		} else
			ntfs_log_debug("%-16lld %-16lld  %-16lld%s\n", 
				       (long long)rl[i].vcn, (long long)rl[i].lcn, 
				       (long long)rl[i].length, 
				       rl[i].length ? "" : " (runlist end)");
	} while (rl[i++].length);
}

#endif
