/*
 * $Id: libprintut.h,v 1.1.1.1 2003/01/27 19:05:32 jlovell Exp $
 * Header for utility library functions.
 * Copyright (C) 1999,2000  Roger Leigh
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *****************************************************************************/


#ifndef __LIBPRINTUT_H__
#define __LIBPRINTUT_H__

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef HAVE_ASPRINTF
#if defined(HAVE_VARARGS_H) && !defined(HAVE_STDARG_H)
#include <varargs.h>
#else
#include <stdarg.h>
#endif
extern int vasprintf (char **result, const char *format, va_list args);
extern int asprintf (char **result, const char *format, ...);
#endif

#ifndef HAVE_XMALLOC
#include "xmalloc.h"
#endif

#ifndef HAVE_XGETCWD
extern char *xgetcwd (void);
#endif

#ifndef HAVE_GETOPT_LONG
#include "getopt.h"
#else
#include <getopt.h>
#endif

#endif /* __LIBPRINTUT_H__ */
