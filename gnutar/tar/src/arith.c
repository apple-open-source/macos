/* Arithmetic for numbers greater than a unsigned long, for GNU tar.
   Copyright (C) 1996, 1997 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify it
   under the terms of the GNU General Public License as published by the
   Free Software Foundation; either version 2, or (at your option) any later
   version.

   This program is distributed in the hope that it will be useful, but
   WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
   Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

#include "system.h"

/* common.h is needed to define FATAL_ERROR.  It also includes arith.h.  */
#include "common.h"

/* GNU tar needs handling numbers exceeding 32 bits, which is the size of
   unsigned long ints for many C compilers.  This module should provide
   machinery for handling at least BITS_PER_TARLONG bits per number.  If
   `long long' ints are available and are sufficient for the task, they will
   be used preferrably.

   Russell Cattelan reports 165 Gb single tapes (digital video D2 tapes on
   Ampex drives), so requiring 38 bits for the tape length in bytes.  He
   also reports breaking the terabyte limit with a single file (using SGI
   xFS file system over 37 28GB disk arrays attached to a Power Challenge
   XL; check out http://www.lcse.umn.edu/ for a picture), so requiring a
   little more than 40 bits for the file size in bytes.  The POSIX header
   structure allows for 12 octal digits to represent file lengths, that is,
   up to 36 bits for the byte size of files.

   If `long long' is not supported by the compiler, SIZEOF_LONG_LONG will be
   set to zero by configure.  In this case, or if `long long' ints does not
   have enough bits, then huge numbers are rather represented by an array of
   longs, with the least significant super-digit at position 0.  For making
   multiplication and decimal input/output easy, the base of a super-digit
   is an exact exponent of 10, and is such that base*base fits in a long.  */

#if SUPERDIGIT

/*-------------------------------.
| Check if ACCUMULATOR is zero.	 |
`-------------------------------*/

int
zerop_tarlong_helper (unsigned long *accumulator)
{
  int counter;

  for (counter = LONGS_PER_TARLONG - 1; counter >= 0; counter--)
    if (accumulator[counter])
      return 0;

  return 1;
}

/*----------------------------------------------.
| Check if FIRST is strictly less than SECOND.  |
`----------------------------------------------*/

int
lessp_tarlong_helper (unsigned long *first, unsigned long *second)
{
  int counter;

  for (counter = LONGS_PER_TARLONG - 1; counter >= 0; counter--)
    if (first[counter] != second[counter])
      return first[counter] < second[counter];

  return 0;
}

/*----------------------------.
| Reset ACCUMULATOR to zero.  |
`----------------------------*/

void
clear_tarlong_helper (unsigned long *accumulator)
{
  int counter;

  for (counter = 0; counter < LONGS_PER_TARLONG; counter++)
    accumulator[counter] = 0;
}

/*----------------------------.
| To ACCUMULATOR, add VALUE.  |
`----------------------------*/

void
add_to_tarlong_helper (unsigned long *accumulator, unsigned long value)
{
  int counter;

  for (counter = 0; counter < LONGS_PER_TARLONG; counter++)
    {
      if (accumulator[counter] + value < SUPERDIGIT)
	{
	  accumulator[counter] += value;
	  return;
	}
      accumulator[counter] += value - SUPERDIGIT;
      value = 1;
    }

  FATAL_ERROR ((0, 0, _("Arithmetic overflow")));
}

/*--------------------------------.
| Multiply ACCUMULATOR by VALUE.  |
`--------------------------------*/

void
mult_tarlong_helper (unsigned long *accumulator, unsigned long value)
{
  int carry = 0;
  int counter;

  for (counter = 0; counter < LONGS_PER_TARLONG; counter++)
    {
      carry += accumulator[counter] * value;
      accumulator[counter] = carry % SUPERDIGIT;
      carry /= SUPERDIGIT;
    }
  if (carry)
    FATAL_ERROR ((0, 0, _("Arithmetic overflow")));
}

/*----------------------------------------------------------.
| Print the decimal representation of ACCUMULATOR on FILE.  |
`----------------------------------------------------------*/

void
print_tarlong_helper (unsigned long *accumulator, FILE *file)
{
  int counter = LONGS_PER_TARLONG - 1;

  while (counter > 0 && accumulator[counter] == 0)
    counter--;

  fprintf (file, "%uld", accumulator[counter]);
  while (counter > 0)
    fprintf (file, TARLONG_FORMAT, accumulator[--counter]);
}

#endif /* SUPERDIGIT */
