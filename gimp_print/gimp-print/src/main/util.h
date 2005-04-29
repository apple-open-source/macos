/*
 * "$Id: util.h,v 1.1.1.1 2004/07/23 06:26:32 jlovell Exp $"
 *
 *   libgimpprint header.
 *
 *   Copyright 1997-2000 Michael Sweet (mike@easysw.com) and
 *	Robert Krawitz (rlk@alum.mit.edu)
 *
 *   This program is free software; you can redistribute it and/or modify it
 *   under the terms of the GNU General Public License as published by the Free
 *   Software Foundation; either version 2 of the License, or (at your option)
 *   any later version.
 *
 *   This program is distributed in the hope that it will be useful, but
 *   WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 *   or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *   for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Revision History:
 *
 *   See ChangeLog
 */

/**
 * @file src/main/util.h
 * @brief Utility functions (internal).
 */

/*
 * This file must include only standard C header files.  The core code must
 * compile on generic platforms that don't support glib, gimp, gtk, etc.
 */

#ifndef GIMP_PRINT_INTERNAL_UTIL_H
#define GIMP_PRINT_INTERNAL_UTIL_H

#ifdef __cplusplus
extern "C" {
#endif

#ifndef __GNUC__
#ifndef __attribute__
#define __attribute__(ignore)
#endif
#endif

/**
 * Utility functions (internal).
 *
 * @defgroup util_internal util-internal
 * @{
 */


/*
 * FIXME Need somewhere else to put these initialization routines
 * which users shouldn't call.
 */

extern void stpi_init_paper(void);
extern void stpi_init_dither(void);
extern void stpi_init_printer(void);

/** @} */

#ifdef __cplusplus
  }
#endif

#endif /* GIMP_PRINT_INTERNAL_UTIL_H */
/*
 * End of "$Id: util.h,v 1.1.1.1 2004/07/23 06:26:32 jlovell Exp $".
 */
