/*
 * acconfig.h:
 * $Id: acconfig.h,v 1.1.1.1 2003/01/27 19:05:31 jlovell Exp $
 * Extra definitions for autoheader
 * Copyright (C) 2000  Roger Leigh
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

/* Package name*/
#undef PACKAGE

/* Package version*/
#undef VERSION

/* Define for use of ioctl(2) system call */
#undef USE_IOCTL

/* Define if libreadline is present */
#undef HAVE_LIBREADLINE

/* Define if xmalloc is present */
#undef HAVE_XMALLOC

/* Definitions for GNU gettext (taken from gettext source and gettext.info */

/* Define if you have obstacks.  */
#undef HAVE_OBSTACK

/* Define if <stddef.h> defines ptrdiff_t.  */
#undef HAVE_PTRDIFF_T

/* Define to 1 if NLS is requested.  */
#undef ENABLE_NLS

/* Define as 1 if you have catgets and don't want to use GNU gettext.  */
#undef HAVE_CATGETS

/* Define as 1 if you have gettext and don't want to use GNU gettext.  */
#undef HAVE_GETTEXT

/* Define if your locale.h file contains LC_MESSAGES.  */
#undef HAVE_LC_MESSAGES

/* Define if you have the parse_printf_format function.  */
#undef HAVE_PARSE_PRINTF_FORMAT

/* Define as 1 if you have the stpcpy function.  */
#undef HAVE_STPCPY

/* Define as 1 if you have the mempcpy function.  */
#undef HAVE_MEMPCPY

/* Package locale directory */
#undef PACKAGE_LOCALE_DIR
