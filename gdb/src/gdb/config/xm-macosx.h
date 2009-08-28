/* Host support for Mac OS X for GDB, the GNU debugger.
   Copyright (C) 1997-2002,
   Free Software Foundation, Inc.

This file is part of GDB.

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#ifndef _XM_MACOSX_H_
#define _XM_MACOSX_H_

#include <mach/mach.h>

/* Need this for NSIG */
#include <signal.h>

#if (!defined (_NSIG) && defined (NSIG))
#define _NSIG NSIG
#endif

#if (NS_TARGET_MAJOR < 5)
#undef HAVE_TERMIOS_H
#endif

#include <limits.h>

extern void macosx_resize_window_handler (void *);
extern void macosx_resize_window (int *width, int *height);

#define	SIGWINCH_HANDLER macosx_resize_window_handler

#define	SIGWINCH_HANDLER_BODY \
void macosx_resize_window_handler (void *d) \
{ \
  macosx_resize_window (&lines_per_page, &chars_per_line); \
}

char *strchr (const char *s, int c);
char *strpbrk (const char *s1, const char *s2);

#endif /* _XM_MACOSX_H_ */
