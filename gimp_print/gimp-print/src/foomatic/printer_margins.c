/*
 * "$Id: printer_margins.c,v 1.1.1.1 2003/01/27 19:05:32 jlovell Exp $"
 *
 *   Dump the per-printer margins for Grant Taylor's *-omatic database
 *
 *   Copyright 2000 Robert Krawitz (rlk@alum.mit.edu)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#ifdef INCLUDE_GIMP_PRINT_H
#include INCLUDE_GIMP_PRINT_H
#else
#include <gimp-print/gimp-print.h>
#endif
#include "../../lib/libprintut.h"
#include <string.h>

int
main(int argc, char **argv)
{
  int i, k;
  for (i = 0; i < stp_known_printers(); i++)
    {
      stp_vars_t v;
      const stp_printer_t p = stp_get_printer_by_index(i);
      const stp_vars_t printvars = stp_printer_get_printvars(p);
      const stp_printfuncs_t* printfuncs = stp_printer_get_printfuncs(p);
      int		num_opts;
      stp_param_t	*opts;
      const char	*defopt;
      int		width, height,
	                bottom, left,
	                top, right;
      printf("# Printer model %s, long name `%s'\n",
	     stp_printer_get_driver(p), stp_printer_get_long_name(p));
      v = stp_allocate_copy(printvars);
      opts = (*(printfuncs->parameters))(p, NULL, "PageSize", &num_opts);
      defopt = (*(printfuncs->default_parameters))(p, NULL, "PageSize");
      for (k = 0; k < num_opts; k++) {
	const stp_papersize_t papersize = stp_get_papersize_by_name(opts[k].name);
	
	if (!papersize) {
	  printf("Unable to look up size %s!\n", opts[k].name);
	  continue;
	}
	  
	width  = stp_papersize_get_width(papersize);
	height = stp_papersize_get_height(papersize);
	  
	/* if (width <= 0 || height <= 0)
	   continue; */
	  
	stp_set_media_size(v, opts[k].name);
	  
	(*(printfuncs->media_size))(p, v, &width, &height);
	(*(printfuncs->imageable_area))(p, v, &left, &right, &bottom, &top);
	  
	if (strcmp(opts[k].name, "Custom") == 0) {
	  /* Use relative values for the custom size */
	  right = width - right;
	  top = height - top;
	}
	  
	printf("$imageableareas{'%s'}{'%s'} = {\n",
	       stp_printer_get_driver(p), opts[k].name);
	printf("  'left' => '%d',\n", left);
	printf("  'right' => '%d',\n", right);
	printf("  'top' => '%d',\n", top);
	printf("  'bottom' => '%d',\n", bottom);
	printf("  'width' => '%d',\n", width);
	printf("  'height' => '%d'\n", height);
	printf("};\n");
      }
    }
  return 0;
}
