/**
 * utils.c - Part of the Linux-NTFS project.
 *
 * Copyright (c) 2002-2005 Richard Russon
 * Copyright (c) 2003-2006 Anton Altaparmakov
 * Copyright (c) 2003 Lode Leroy
 * Copyright (c) 2005-2007 Yura Pakhuchiy
 *
 * See LICENSE file for licensing information.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDIO_H
#include <stdio.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif
#ifdef HAVE_LIBINTL_H
#include <libintl.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#ifdef HAVE_CTYPE_H
#include <ctype.h>
#endif

#include "utils.h"
#include "types.h"
#include "volume.h"
#include "debug.h"
#include "dir.h"
/* #include "version.h" */
#include "logging.h"
#include "misc.h"

/**
 * utils_set_locale
 */
int utils_set_locale(void)
{
	const char *locale;

	locale = setlocale(LC_ALL, "");
	if (!locale) {
		locale = setlocale(LC_ALL, NULL);
		ntfs_log_error("Failed to set locale, using default '%s'.\n",
				locale);
		return 1;
	} else {
		return 0;
	}
}

/**
 * linux-ntfs's ntfs_mbstoucs has different semantics, so we emulate it with
 * ntfs-3g's.
 */
int ntfs_mbstoucs_libntfscompat(const char *ins,
		ntfschar **outs, int outs_len)
{
	if(!outs) {
		errno = EINVAL;
		return -1;
	}
	else if(*outs != NULL) {
		/* Note: libntfs's mbstoucs implementation allows the caller to
		 * specify a preallocated buffer while libntfs-3g's always
		 * allocates the output buffer.
		 */
		ntfschar *tmpstr = NULL;
		int tmpstr_len;

		tmpstr_len = ntfs_mbstoucs(ins, &tmpstr);
		if(tmpstr_len >= 0) {
			if((tmpstr_len + 1) > outs_len) {
				/* Doing a realloc instead of reusing tmpstr
				 * because it emulates libntfs's mbstoucs more
				 * closely. */
				ntfschar *re_outs = realloc(*outs,
					sizeof(ntfschar)*(tmpstr_len + 1));
				if(!re_outs)
					tmpstr_len = -1;
				else
					*outs = re_outs;
			}

			if(tmpstr_len >= 0) {
				/* The extra character is the \0 terminator. */
				memcpy(*outs, tmpstr,
					sizeof(ntfschar)*(tmpstr_len + 1));
			}

			free(tmpstr);
		}

		return tmpstr_len;
	}
	else
		return ntfs_mbstoucs(ins, outs);
}
