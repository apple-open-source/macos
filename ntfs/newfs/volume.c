/**
 * volume.c - NTFS volume handling code.
 *
 * Copyright (c) 2000-2006 Anton Altaparmakov
 * Copyright (c) 2002-2009 Szabolcs Szakacsits
 * Copyright (c) 2004-2005 Richard Russon
 * Copyright (c) 2010      Jean-Pierre Andre
 * Copyright (c) 2008-2012 Tuxera Inc.
 *
 * See LICENSE file for licensing information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#include "param.h"
#include "compat.h"
#include "volume.h"
#include "attrib.h"
#include "mft.h"
#include "bootsect.h"
#include "device.h"
#include "debug.h"
#include "inode.h"
#include "runlist.h"
#include "dir.h"
#include "logging.h"
#include "misc.h"

const char *ntfs_home = 
"Company Web Site:  http://tuxera.com\n";

/**
 * ntfs_volume_alloc - Create an NTFS volume object and initialise it
 *
 * Description...
 *
 * Returns:
 */
ntfs_volume *ntfs_volume_alloc(void)
{
	return ntfs_calloc(sizeof(ntfs_volume));
}

/**
 * ntfs_check_if_mounted - check if an ntfs volume is currently mounted
 * @file:	device file to check
 * @mnt_flags:	pointer into which to return the ntfs mount flags (see volume.h)
 *
 * If the running system does not support the {set,get,end}mntent() calls,
 * just return 0 and set *@mnt_flags to zero.
 *
 * When the system does support the calls, ntfs_check_if_mounted() first tries
 * to find the device @file in /etc/mtab (or wherever this is kept on the
 * running system). If it is not found, assume the device is not mounted and
 * return 0 and set *@mnt_flags to zero.
 *
 * If the device @file is found, set the NTFS_MF_MOUNTED flags in *@mnt_flags.
 *
 * Further if @file is mounted as the file system root ("/"), set the flag
 * NTFS_MF_ISROOT in *@mnt_flags.
 *
 * Finally, check if the file system is mounted read-only, and if so set the
 * NTFS_MF_READONLY flag in *@mnt_flags.
 *
 * On success return 0 with *@mnt_flags set to the ntfs mount flags.
 *
 * On error return -1 with errno set to the error code.
 */
int ntfs_check_if_mounted(const char *file __attribute__((unused)),
		unsigned long *mnt_flags)
{
	*mnt_flags = 0;
#ifdef HAVE_MNTENT_H
	return ntfs_mntent_check(file, mnt_flags);
#else
	return 0;
#endif
}
