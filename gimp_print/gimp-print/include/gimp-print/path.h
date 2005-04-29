/*
 * "$Id: path.h,v 1.1.1.1 2004/07/23 06:26:27 jlovell Exp $"
 *
 *   libgimpprint path functions header
 *
 *   Copyright 1997-2000 Michael Sweet (mike@easysw.com),
 *	Robert Krawitz (rlk@alum.mit.edu) and Michael Natterer (mitch@gimp.org)
 *   Copyright 2002 Roger Leigh (rleigh@debian.org)
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
 * compile on generic platforms that don't support glib, gimp, gtk, etc.
 */

#ifndef GIMP_PRINT_PATH_H
#define GIMP_PRINT_PATH_H

#ifdef __cplusplus
extern "C" {
#endif


extern stp_list_t *stp_path_search(stp_list_t *dirlist,
				   const char *suffix);

extern void stp_path_split(stp_list_t *list,
			   const char *path);


#ifdef __cplusplus
  }
#endif

#endif /* GIMP_PRINT_PATH_H */
/*
 * End of "$Id: path.h,v 1.1.1.1 2004/07/23 06:26:27 jlovell Exp $".
 */
