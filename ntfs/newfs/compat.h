/*
 * compat.h - Tweaks for Windows compatibility.
 *
 * Copyright (c) 2002 Richard Russon
 * Copyright (c) 2002-2004 Anton Altaparmakov
 * Copyright (c) 2008-2009 Szabolcs Szakacsits
 * Copyright (c) 2008-2012 Tuxera Inc.
 *
 * See LICENSE file for licensing information.
 */

#ifndef _NTFS_COMPAT_H
#define _NTFS_COMPAT_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef O_BINARY
#define O_BINARY		0		/* unix is binary by default */
#endif

#endif /* defined _NTFS_COMPAT_H */
