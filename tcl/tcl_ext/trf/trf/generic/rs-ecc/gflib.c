/*
    ecc Version 1.2  by Paul Flaherty (paulf@stanford.edu)
    Copyright (C) 1993 Free Software Foundation, Inc.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/


/* gflib.c
	Math Library for GF[256]

	This file contains a number of mathematical functions for GF[256].
	Entry and result are always assumed to be in vector notation, since
	said notation allows for the zero element.  Attempting to reciprocate
	the zero element results in process exit 42.
*/

#include "gf.h"


/* Multiply two field elements */

unsigned char
gfmul (mul1, mul2)

     unsigned char mul1, mul2;
{
  unsigned char mul3;
  if (mul1 == 0 || mul2 == 0)
    mul3 = 0;
  else
    mul3 = e2v[(v2e[mul1] + v2e[mul2]) % 255];
  return (mul3);
}


/* Add two field elements.  Subtraction and addition are equivalent */

unsigned char
gfadd (add1, add2)

     unsigned char add1, add2;
{
  unsigned char add3;
  add3 = add1 ^ add2;
  return (add3);
}


/* Invert a field element, for division */

unsigned char
gfinv (ivt)

     unsigned char ivt;
{
  unsigned char ivtd;
  if (ivt == 0)
    exit (42);
  ivtd = e2v[255 - v2e[ivt]];
  return (ivtd);
}


/* Exponentiation.  Convert to exponential notation, mod 255 */

unsigned char
gfexp (mant, powr)

     unsigned char mant, powr;
{
  unsigned char expt;
  if (mant == 0)
    expt = 0;
  else
    expt = e2v[(v2e[mant] * powr) % 255];
  return (expt);
}
