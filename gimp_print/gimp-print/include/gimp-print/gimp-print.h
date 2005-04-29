/*		-*- Mode: C -*-
 *  $Id: gimp-print.h,v 1.1.1.3 2004/07/23 06:26:27 jlovell Exp $
 *
 *   Gimp-Print header file
 *
 *   Copyright 1997-2002 Michael Sweet (mike@easysw.com) and
 *      Robert Krawitz (rlk@alum.mit.edu)
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
 * @file gimp-print.h
 * @brief Gimp-Print master header.
 * This header includes all of the public headers.
 */

/*
 * This file must include only standard C header files.  The core code must
 * compile on generic platforms that don't support glib, gimp, gtk, etc.
 */

#ifndef GIMP_PRINT_H
#define GIMP_PRINT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>     /* For size_t */
#include <stdio.h>	/* For FILE */

#include <gimp-print/array.h>
#include <gimp-print/curve.h>
#include <gimp-print/gimp-print-version.h>
#include <gimp-print/image.h>
#include <gimp-print/paper.h>
#include <gimp-print/printers.h>
#include <gimp-print/sequence.h>
#include <gimp-print/string-list.h>
#include <gimp-print/util.h>
#include <gimp-print/vars.h>

#ifdef __cplusplus
  }
#endif

#endif /* GIMP_PRINT_H */
/*
 * End of $Id: gimp-print.h,v 1.1.1.3 2004/07/23 06:26:27 jlovell Exp $
 */
