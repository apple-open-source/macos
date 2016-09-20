/*
 * utils.h - Part of the Linux-NTFS project.
 *
 * Copyright (c) 2002-2005 Richard Russon
 * Copyright (c) 2004 Anton Altaparmakov
 *
 * See LICENSE file for licensing information.
 */

#ifndef _NTFS_UTILS_H_
#define _NTFS_UTILS_H_

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "types.h"
#include "layout.h"
#include "volume.h"

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

int utils_set_locale(void);

/**
 * linux-ntfs's ntfs_mbstoucs has different semantics, so we emulate it with
 * ntfs-3g's.
 */
int ntfs_mbstoucs_libntfscompat(const char *ins,
		ntfschar **outs, int outs_len);

#endif /* _NTFS_UTILS_H_ */
