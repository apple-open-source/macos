/*
 * "$Id: print-version.c,v 1.1.1.1 2003/01/27 19:05:32 jlovell Exp $"
 *
 *   Print plug-in driver utility functions for the GIMP.
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
 */

/*
 * This file must include only standard C header files.  The core code must
 * compile on generic platforms that don't support glib, gimp, gimpprint, etc.
 */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <gimp-print/gimp-print.h>
#include "gimp-print-internal.h"
#include <gimp-print/gimp-print-intl-internal.h>

const unsigned int gimpprint_major_version = GIMPPRINT_MAJOR_VERSION;
const unsigned int gimpprint_minor_version = GIMPPRINT_MINOR_VERSION;
const unsigned int gimpprint_micro_version = GIMPPRINT_MICRO_VERSION;
const unsigned int gimpprint_current_interface = GIMPPRINT_CURRENT_INTERFACE;
const unsigned int gimpprint_binary_age = GIMPPRINT_BINARY_AGE;
const unsigned int gimpprint_interface_age = GIMPPRINT_INTERFACE_AGE;


const char *
stp_check_version (unsigned int required_major,
		   unsigned int required_minor, unsigned int required_micro)
{
  if (required_major > GIMPPRINT_MAJOR_VERSION)
    return "gimpprint version too old (major mismatch)";
  if (required_major < GIMPPRINT_MAJOR_VERSION)
    return "gimpprint version too new (major mismatch)";
  if (required_minor > GIMPPRINT_MINOR_VERSION)
    return "gimpprint version too old (minor mismatch)";
  if (required_minor < GIMPPRINT_MINOR_VERSION)
    return "gimpprint version too new (minor mismatch)";
  if (required_micro < GIMPPRINT_MICRO_VERSION - GIMPPRINT_BINARY_AGE)
    return "gimpprint version too new (micro mismatch)";
  if (required_micro > GIMPPRINT_MICRO_VERSION)
    return "gimpprint version too old (micro mismatch)";
  return NULL;
}
