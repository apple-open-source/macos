/*
 * "$Id: gimp-print-internal.h,v 1.1.1.2 2004/07/23 06:26:31 jlovell Exp $"
 *
 *   Print plug-in header file for the GIMP.
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

/*
 * This file must include only standard C header files.  The core code must
 * compile on generic platforms that don't support glib, gimp, gtk, etc.
 */

#ifndef GIMP_PRINT_INTERNAL_INTERNAL_H
#define GIMP_PRINT_INTERNAL_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gimp-print/gimp-print-module.h>

#include "util.h"


#ifdef __cplusplus
  }
#endif

#endif /* GIMP_PRINT_INTERNAL_INTERNAL_H */
/*
 * End of "$Id: gimp-print-internal.h,v 1.1.1.2 2004/07/23 06:26:31 jlovell Exp $".
 */
