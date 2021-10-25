/*
 * debug.h - Debugging output functions. 
 *
 * Copyright (c) 2002-2004 Anton Altaparmakov
 * Copyright (c) 2008-2012 Tuxera Inc.
 *
 * See LICENSE file for licensing information.
 */

#ifndef _NTFS_DEBUG_H
#define _NTFS_DEBUG_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "logging.h"

struct _runlist_element;

#ifdef DEBUG
extern void ntfs_debug_runlist_dump(const struct _runlist_element *rl);
#else
static __inline__ void ntfs_debug_runlist_dump(const struct _runlist_element *rl __attribute__((unused))) {}
#endif

#define NTFS_BUG(msg)							\
{									\
	int ___i = 1;							\
	ntfs_log_critical("Bug in %s(): %s\n", __FUNCTION__, msg);	\
	ntfs_log_debug("Forcing segmentation fault!");			\
	___i = ((int*)NULL)[___i];					\
}

#endif /* defined _NTFS_DEBUG_H */
