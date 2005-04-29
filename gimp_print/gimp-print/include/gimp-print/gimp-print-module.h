/*		-*- Mode: C -*-
 *  $Id: gimp-print-module.h,v 1.1.1.1 2004/07/23 06:26:27 jlovell Exp $
 *
 *   Gimp-Print module header file
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
 * @file gimp-print-module.h
 * @brief Gimp-Print module header.
 * This header includes all of the public headers used by modules.
 */

/*
 * This file must include only standard C header files.  The core code must
 * compile on generic platforms that don't support glib, gimp, gtk, etc.
 */

#ifndef GIMP_PRINT_GIMP_PRINT_MODULE_H
#define GIMP_PRINT_GIMP_PRINT_MODULE_H

#ifdef __cplusplus
extern "C" {
#endif

#define STP_MODULE 1

#include <gimp-print/gimp-print.h>

#include <gimp-print/bit-ops.h>
#include <gimp-print/channel.h>
#include <gimp-print/color.h>
#include <gimp-print/dither.h>
#include <gimp-print/list.h>
#include <gimp-print/module.h>
#include <gimp-print/path.h>
#include <gimp-print/weave.h>
#include <gimp-print/xml.h>

#ifdef __cplusplus
  }
#endif

#endif /* GIMP_PRINT_MODULE_H */
/*
 * End of $Id: gimp-print-module.h,v 1.1.1.1 2004/07/23 06:26:27 jlovell Exp $
 */
