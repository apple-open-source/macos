/*
 * Copyright 1990, 1998  The Open Group
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation.
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 * 
 * Except as contained in this notice, the name of The Open Group shall
 * not be used in advertising or otherwise to promote the sale, use or
 * other dealings in this Software without prior written authorization
 * from The Open Group.
 *
 */

/* Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, and/or sell copies of the Software, and to permit persons
 * to whom the Software is furnished to do so, provided that the above
 * copyright notice(s) and this permission notice appear in all copies of
 * the Software and that both the above copyright notice(s) and this
 * permission notice appear in supporting documentation.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
 * OF THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * HOLDERS INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY SPECIAL
 * INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 * 
 * Except as contained in this notice, the name of a copyright holder
 * shall not be used in advertising or otherwise to promote the sale, use
 * or other dealings in this Software without prior written authorization
 * of the copyright holder.
 */

#include "config.h"

#include <sys/types.h>
#include <time.h>
#include <utmp.h>

#ifdef HAVE_UTMPX_H
# if HAVE_UTMPX_UT_SYSLEN
#  include <utmpx.h>
#  define USE_UTMPX
# endif
#endif

#ifdef HAVE_SYS_PARAM_H
# include <sys/param.h>
#endif

#ifndef HAVE_LASTLOG_H
# define NO_LASTLOG
#endif

#ifndef NO_LASTLOG
# ifdef CSRG_BASED
#  if (BSD < 199103)
#   include	<lastlog.h>
#  endif
# else
#  include	<lastlog.h>
# endif
# include	<pwd.h>
#endif

#ifdef CSRG_BASED
#if !defined(__FreeBSD__) && !defined(__OpenBSD__)
/* *BSD doesn't like a ':0' type entry in utmp */
#define NO_UTMP
#endif
#endif

#ifndef WTMP_FILE
# ifdef _PATH_WTMP
#  define WTMP_FILE	_PATH_WTMP
# else
#  define WTMP_FILE	"/usr/adm/wtmp"
# endif
#endif
#ifndef UTMP_FILE
# ifdef _PATH_UTMP
#  define UTMP_FILE	_PATH_UTMP
# else
#  define UTMP_FILE	"/etc/utmp"
# endif
#endif
#ifndef NO_LASTLOG
# ifndef LLOG_FILE
#  ifdef _PATH_LASTLOG
#   define LLOG_FILE	_PATH_LASTLOG
#  else
#   define LLOG_FILE	"/usr/adm/lastlog"
#  endif
# endif
#endif
#ifndef SYSV
# ifndef TTYS_FILE
#  define TTYS_FILE	"/etc/ttys"
# endif
#endif

