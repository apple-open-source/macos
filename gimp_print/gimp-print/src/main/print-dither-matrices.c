/*
 * "$Id: print-dither-matrices.c,v 1.1.1.1 2003/01/27 19:05:32 jlovell Exp $"
 *
 *   Print plug-in driver utility functions for the GIMP.
 *
 *   Copyright 2001 Robert Krawitz (rlk@alum.mit.edu)
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "print-dither.h"
#include <math.h>

#ifdef __GNUC__
#define inline __inline__
#endif

static const unsigned short mat_1_1[] =
{
#include "quickmatrix257.h"
};

const stp_dither_matrix_short_t stp_1_1_matrix =
{
  257, 257, 2, 1, mat_1_1
};

static const unsigned short mat_2_1[] =
{
#include "ran.367.179.h"
};

const stp_dither_matrix_short_t stp_2_1_matrix =
{
  367, 179, 2, 1, mat_2_1
};

static const unsigned short mat_4_1[] =
{
#include "ran.509.131.h"
};

const stp_dither_matrix_short_t stp_4_1_matrix =
{
  509, 131, 2, 1, mat_4_1
};

static inline int
calc_ordered_point(unsigned x, unsigned y, int steps, int multiplier,
		   int size, const unsigned *map)
{
  int i, j;
  unsigned retval = 0;
  int divisor = 1;
  int div1;
  for (i = 0; i < steps; i++)
    {
      int xa = (x / divisor) % size;
      int ya = (y / divisor) % size;
      unsigned base;
      base = map[ya + (xa * size)];
      div1 = 1;
      for (j = i; j < steps - 1; j++)
	div1 *= size * size;
      retval += base * div1;
      divisor *= size;
    }
  return retval * multiplier;
}

static int
is_po2(size_t i)
{
  if (i == 0)
    return 0;
  return (((i & (i - 1)) == 0) ? 1 : 0);
}

void
stp_init_iterated_matrix(dither_matrix_t *mat, size_t size, size_t exp,
			 const unsigned *array)
{
  int i;
  int x, y;
  mat->base = size;
  mat->exp = exp;
  mat->x_size = 1;
  for (i = 0; i < exp; i++)
    mat->x_size *= mat->base;
  mat->y_size = mat->x_size;
  mat->total_size = mat->x_size * mat->y_size;
  mat->matrix = stp_malloc(sizeof(unsigned) * mat->x_size * mat->y_size);
  for (x = 0; x < mat->x_size; x++)
    for (y = 0; y < mat->y_size; y++)
      {
	mat->matrix[x + y * mat->x_size] =
	  calc_ordered_point(x, y, mat->exp, 1, mat->base, array);
	mat->matrix[x + y * mat->x_size] =
	  (double) mat->matrix[x + y * mat->x_size] * 65536.0 /
	  (double) (mat->x_size * mat->y_size);
      }
  mat->last_x = mat->last_x_mod = 0;
  mat->last_y = mat->last_y_mod = 0;
  mat->index = 0;
  mat->i_own = 1;
  if (is_po2(mat->x_size))
    mat->fast_mask = mat->x_size - 1;
  else
    mat->fast_mask = 0;
}

#define MATRIX_POINT(m, x, y, x_size, y_size) \
  ((m)[(((x) + (x_size)) % (x_size)) + ((x_size) * (((y) + (y_size)) % (y_size)))])

void
stp_shear_matrix(dither_matrix_t *mat, int x_shear, int y_shear)
{
  int i;
  int j;
  int *tmp = stp_malloc(mat->x_size * mat->y_size * sizeof(int));
  for (i = 0; i < mat->x_size; i++)
    for (j = 0; j < mat->y_size; j++)
      MATRIX_POINT(tmp, i, j, mat->x_size, mat->y_size) =
	MATRIX_POINT(mat->matrix, i, j * (x_shear + 1), mat->x_size,
		    mat->y_size);
  for (i = 0; i < mat->x_size; i++)
    for (j = 0; j < mat->y_size; j++)
      MATRIX_POINT(mat->matrix, i, j, mat->x_size, mat->y_size) =
	MATRIX_POINT(tmp, i * (y_shear + 1), j, mat->x_size, mat->y_size);
  stp_free(tmp);
}

void
stp_init_matrix(dither_matrix_t *mat, int x_size, int y_size,
		const unsigned int *array, int transpose, int prescaled)
{
  int x, y;
  mat->base = x_size;
  mat->exp = 1;
  mat->x_size = x_size;
  mat->y_size = y_size;
  mat->total_size = mat->x_size * mat->y_size;
  mat->matrix = stp_malloc(sizeof(unsigned) * mat->x_size * mat->y_size);
  for (x = 0; x < mat->x_size; x++)
    for (y = 0; y < mat->y_size; y++)
      {
	if (transpose)
	  mat->matrix[x + y * mat->x_size] = array[y + x * mat->y_size];
	else
	  mat->matrix[x + y * mat->x_size] = array[x + y * mat->x_size];
	if (!prescaled)
	  mat->matrix[x + y * mat->x_size] =
	    (double) mat->matrix[x + y * mat->x_size] * 65536.0 /
	    (double) (mat->x_size * mat->y_size);
      }
  mat->last_x = mat->last_x_mod = 0;
  mat->last_y = mat->last_y_mod = 0;
  mat->index = 0;
  mat->i_own = 1;
  if (is_po2(mat->x_size))
    mat->fast_mask = mat->x_size - 1;
  else
    mat->fast_mask = 0;
}

void
stp_init_matrix_short(dither_matrix_t *mat, int x_size, int y_size,
		      const unsigned short *array, int transpose,
		      int prescaled)
{
  int x, y;
  mat->base = x_size;
  mat->exp = 1;
  mat->x_size = x_size;
  mat->y_size = y_size;
  mat->total_size = mat->x_size * mat->y_size;
  mat->matrix = stp_malloc(sizeof(unsigned) * mat->x_size * mat->y_size);
  for (x = 0; x < mat->x_size; x++)
    for (y = 0; y < mat->y_size; y++)
      {
	if (transpose)
	  mat->matrix[x + y * mat->x_size] = array[y + x * mat->y_size];
	else
	  mat->matrix[x + y * mat->x_size] = array[x + y * mat->x_size];
	if (!prescaled)
	  mat->matrix[x + y * mat->x_size] =
	    (double) mat->matrix[x + y * mat->x_size] * 65536.0 /
	    (double) (mat->x_size * mat->y_size);
      }
  mat->last_x = mat->last_x_mod = 0;
  mat->last_y = mat->last_y_mod = 0;
  mat->index = 0;
  mat->i_own = 1;
  if (is_po2(mat->x_size))
    mat->fast_mask = mat->x_size - 1;
  else
    mat->fast_mask = 0;
}

void
stp_destroy_matrix(dither_matrix_t *mat)
{
  if (mat->i_own && mat->matrix)
    stp_free(mat->matrix);
  mat->matrix = NULL;
  mat->base = 0;
  mat->exp = 0;
  mat->x_size = 0;
  mat->y_size = 0;
  mat->total_size = 0;
  mat->i_own = 0;
}

void
stp_clone_matrix(const dither_matrix_t *src, dither_matrix_t *dest,
		 int x_offset, int y_offset)
{
  dest->base = src->base;
  dest->exp = src->exp;
  dest->x_size = src->x_size;
  dest->y_size = src->y_size;
  dest->total_size = src->total_size;
  dest->matrix = src->matrix;
  dest->x_offset = x_offset;
  dest->y_offset = y_offset;
  dest->last_x = 0;
  dest->last_x_mod = dest->x_offset % dest->x_size;
  dest->last_y = 0;
  dest->last_y_mod = dest->x_size * (dest->y_offset % dest->y_size);
  dest->index = dest->last_x_mod + dest->last_y_mod;
  dest->fast_mask = src->fast_mask;
  dest->i_own = 0;
}

void
stp_copy_matrix(const dither_matrix_t *src, dither_matrix_t *dest)
{
  int x;
  dest->base = src->base;
  dest->exp = src->exp;
  dest->x_size = src->x_size;
  dest->y_size = src->y_size;
  dest->total_size = src->total_size;
  dest->matrix = stp_malloc(sizeof(unsigned) * dest->x_size * dest->y_size);
  for (x = 0; x < dest->x_size * dest->y_size; x++)
    dest->matrix[x] = src->matrix[x];
  dest->x_offset = 0;
  dest->y_offset = 0;
  dest->last_x = 0;
  dest->last_x_mod = 0;
  dest->last_y = 0;
  dest->last_y_mod = 0;
  dest->index = 0;
  dest->fast_mask = src->fast_mask;
  dest->i_own = 1;
}

void
stp_exponential_scale_matrix(dither_matrix_t *mat, double exponent)
{
  int i;
  int mat_size = mat->x_size * mat->y_size;
  for (i = 0; i < mat_size; i++)
    {
      double dd = mat->matrix[i] / 65535.0;
      dd = pow(dd, exponent);
      mat->matrix[i] = 65535 * dd;
    }
}

void
stp_matrix_set_row(dither_matrix_t *mat, int y)
{
  mat->last_y = y;
  mat->last_y_mod = mat->x_size * ((y + mat->y_offset) % mat->y_size);
  mat->index = mat->last_x_mod + mat->last_y_mod;
}
