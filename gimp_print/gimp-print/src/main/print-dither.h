/*
 * "$Id: print-dither.h,v 1.1.1.1 2003/01/27 19:05:32 jlovell Exp $"
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

#ifndef _GIMP_PRINT_DITHER_H_
#define _GIMP_PRINT_DITHER_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gimp-print/gimp-print.h>
#include "gimp-print-internal.h"

extern const stp_dither_matrix_short_t stp_1_1_matrix;
extern const stp_dither_matrix_short_t stp_2_1_matrix;
extern const stp_dither_matrix_short_t stp_4_1_matrix;

typedef struct dither_matrix
{
  int base;
  int exp;
  int x_size;
  int y_size;
  int total_size;
  int last_x;
  int last_x_mod;
  int last_y;
  int last_y_mod;
  int index;
  int i_own;
  int x_offset;
  int y_offset;
  unsigned fast_mask;
  unsigned *matrix;
} dither_matrix_t;

extern void stp_init_iterated_matrix(dither_matrix_t *mat, size_t size,
				     size_t exp, const unsigned *array);
extern void stp_shear_matrix(dither_matrix_t *mat, int x_shear, int y_shear);
extern void stp_init_matrix(dither_matrix_t *mat, int x_size, int y_size,
			    const unsigned int *array, int transpose,
			    int prescaled);
extern void stp_init_matrix_short(dither_matrix_t *mat, int x_size, int y_size,
				  const unsigned short *array, int transpose,
				  int prescaled);
extern void stp_destroy_matrix(dither_matrix_t *mat);
extern void stp_clone_matrix(const dither_matrix_t *src, dither_matrix_t *dest,
			     int x_offset, int y_offset);
extern void stp_copy_matrix(const dither_matrix_t *src, dither_matrix_t *dest);
extern void stp_exponential_scale_matrix(dither_matrix_t *mat,double exponent);
extern void stp_matrix_set_row(dither_matrix_t *mat, int y);

#endif /* _GIMP_PRINT_DITHER_H_ */
