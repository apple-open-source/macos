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


/* rslib.c
	Library of Reed - Solomon Routines

	This file contains the actual routines to implement a Reed - Solomon
	(255,249,7) code.  The encoder uses a feedback shift register
	generator, which systematically encodes 249 bytes into a 255 byte
	block.  The decoder is a classic Peterson algorithm.
*/

#include "ecc.h"

/* aku: forward declaration of routines used before definition */
void
polysolve ();



/* Reed - Solomon Encoder.  The Encoder uses a shift register algorithm,
   as detailed in _Applied Modern Algebra_ by Dornhoff and Hohn (p.446).
   Note that the message is reversed in the code array; this was done to
   allow for (emergency) recovery of the message directly from the
   data stream.
*/

void /* <-- aku */
rsencode (m, c)

     unsigned char m[249], c[255];

{

  unsigned char r[6], rtmp;

  int i, j;

  for (i = 0; i < 6; i++)
    r[i] = 0;

  for (i = 0; i < 249; i++)
    {
      c[254 - i] = m[i];
      rtmp = gfadd (m[i], r[5]);
      for (j = 5; j > 0; j--)
	{
	  r[j] = gfadd (gfmul (rtmp, g[j]), r[j - 1]);
	}
      r[0] = gfmul (rtmp, g[0]);
    }
  for (i = 0; i < 6; i++)
    {
      c[i] = r[i];
    }
}


/* Polynomial Evaluator, used to determine the Syndrome Vector.  This is
   relatively straightforward, and there are faster algorithms.
*/

unsigned char
evalpoly (p, x)

     unsigned char p[255], x;

{

  unsigned char y;
  int i;
  y = 0;

  for (i = 0; i < 255; i++)
    {
      y = gfadd (y, gfmul (p[i], gfexp (x, i)));
    }
  return (y);

}


/* Determine the Syndrome Vector.  Note that in s[0] we return the OR of
   all of the syndromes; this allows for an easy check for the no - error
   condition.
*/

void /* <-- aku */
syndrome (c, s)

     unsigned char c[255], s[7];
{

  extern unsigned char e2v[256];
  int i;
  s[0] = 0;
  for (i = 1; i < 7; i++)
    {
      s[i] = evalpoly (c, e2v[i]);
      s[0] = s[0] | s[i];
    }

}


/* Determine the number of errors in a block.  Since we have to find the
   determinant of the S[] matrix in order to determine singularity, we
   also return the determinant to be used by the Cramer's Rule correction
   algorithm.
*/

void /* <-- aku */
errnum (s, det, errs)

     unsigned char s[7], *det;
     int *errs;

{


  *det = gfmul (s[2], gfmul (s[4], s[6]));
  *det = gfadd (*det, gfmul (s[2], gfmul (s[5], s[5])));
  *det = gfadd (*det, gfmul (s[6], gfmul (s[3], s[3])));
  *det = gfadd (*det, gfmul (s[4], gfmul (s[4], s[4])));
  *errs = 3;

  if (*det != 0)
    return;

  *det = gfadd (gfmul (s[2], s[4]), gfexp (s[3], 2));
  *errs = 2;
  if (*det != 0)
    return;

  *det = s[1];
  *errs = 1;
  if (*det != 0)
    return;

  *errs = 4;

}


/* Full impementation of the three error correcting Peterson decoder.  For
   t<6, it is faster than Massey - Berlekamp.  It is also somewhat more
   intuitive.
*/

void /* <-- aku */
rsdecode (code, mesg, errcode)

     unsigned char code[255], mesg[249];
     int *errcode;
{

  extern unsigned char v2e[256];
  unsigned char syn[7], deter, z[4], e0, e1, e2, n0, n1, n2, w0, w1, w2,
    x0, x[3];
  int i, sols;

  *errcode = 0;

  /* First, get the message out of the code, so that even if we can't correct
	   it, we return an estimate.
	*/

  for (i = 0; i < 249; i++)
    mesg[i] = code[254 - i];

  syndrome (code, syn);

  if (syn[0] == 0)
    return;

  /* We now know we have at least one error.  If there are no errors detected,
	   we assume that something funny is going on, and so return with errcode 4,
	   else pass the number of errors back via errcode.
	*/

  errnum (syn, &deter, errcode);

  if (*errcode == 4)
    return;

  /* Having obtained the syndrome, the number of errors, and the determinant,
	   we now proceed to correct the block.  If we do not find exactly the
	   number of solutions equal to the number of errors, we have exceeded our
	   error capacity, and return with the block uncorrected, and errcode 4.
	*/

  switch (*errcode)
    {

    case 1:

      x0 = gfmul (syn[2], gfinv (syn[1]));
      w0 = gfmul (gfexp (syn[1], 2), gfinv (syn[2]));
      if (v2e[x0] > 5)
	mesg[254 - v2e[x0]] = gfadd (mesg[254 - v2e[x0]], w0);
      return;

    case 2:

      z[0] = gfmul (gfadd (gfmul (syn[1], syn[3]), gfexp (syn[2], 2)), gfinv (deter));
      z[1] = gfmul (gfadd (gfmul (syn[2], syn[3]), gfmul (syn[1], syn[4])), gfinv (deter));
      z[2] = 1;
      z[3] = 0;

      polysolve (z, x, &sols);
      if (sols != 2)
	{
	  *errcode = 4;
	  return;
	}

      w0 = gfmul (z[0], syn[1]);
      w1 = gfadd (gfmul (z[0], syn[2]), gfmul (z[1], syn[1]));
      n0 = 254 - v2e[gfinv (x[0])];
      n1 = 254 - v2e[gfinv (x[1])];
      e0 = gfmul (gfadd (w0, gfmul (w1, x[0])), gfinv (z[1]));
      e1 = gfmul (gfadd (w0, gfmul (w1, x[1])), gfinv (z[1]));

      if (n0 < 249)
	mesg[n0] = gfadd (mesg[n0], e0);
      if (n1 < 249)
	mesg[n1] = gfadd (mesg[n1], e1);
      return;

    case 3:

      z[3] = 1;
      z[2] = gfmul (syn[1], gfmul (syn[4], syn[6]));
      z[2] = gfadd (z[2], gfmul (syn[1], gfmul (syn[5], syn[5])));
      z[2] = gfadd (z[2], gfmul (syn[5], gfmul (syn[3], syn[3])));
      z[2] = gfadd (z[2], gfmul (syn[3], gfmul (syn[4], syn[4])));
      z[2] = gfadd (z[2], gfmul (syn[2], gfmul (syn[5], syn[4])));
      z[2] = gfadd (z[2], gfmul (syn[2], gfmul (syn[3], syn[6])));
      z[2] = gfmul (z[2], gfinv (deter));

      z[1] = gfmul (syn[1], gfmul (syn[3], syn[6]));
      z[1] = gfadd (z[1], gfmul (syn[1], gfmul (syn[5], syn[4])));
      z[1] = gfadd (z[1], gfmul (syn[4], gfmul (syn[3], syn[3])));
      z[1] = gfadd (z[1], gfmul (syn[2], gfmul (syn[4], syn[4])));
      z[1] = gfadd (z[1], gfmul (syn[2], gfmul (syn[3], syn[5])));
      z[1] = gfadd (z[1], gfmul (syn[2], gfmul (syn[2], syn[6])));
      z[1] = gfmul (z[1], gfinv (deter));

      z[0] = gfmul (syn[2], gfmul (syn[3], syn[4]));
      z[0] = gfadd (z[0], gfmul (syn[3], gfmul (syn[2], syn[4])));
      z[0] = gfadd (z[0], gfmul (syn[3], gfmul (syn[5], syn[1])));
      z[0] = gfadd (z[0], gfmul (syn[4], gfmul (syn[4], syn[1])));
      z[0] = gfadd (z[0], gfmul (syn[3], gfmul (syn[3], syn[3])));
      z[0] = gfadd (z[0], gfmul (syn[2], gfmul (syn[2], syn[5])));
      z[0] = gfmul (z[0], gfinv (deter));

      polysolve (z, x, &sols);
      if (sols != 3)
	{
	  *errcode = 4;
	  return;
	}

      w0 = gfmul (z[0], syn[1]);
      w1 = gfadd (gfmul (z[0], syn[2]), gfmul (z[1], syn[1]));
      w2 = gfadd (gfmul (z[0], syn[3]), gfadd (gfmul (z[1], syn[2]), gfmul (z[2], syn[1])));

      n0 = 254 - v2e[gfinv (x[0])];
      n1 = 254 - v2e[gfinv (x[1])];
      n2 = 254 - v2e[gfinv (x[2])];

      e0 = gfadd (w0, gfadd (gfmul (w1, x[0]), gfmul (w2, gfexp (x[0], 2))));
      e0 = gfmul (e0, gfinv (gfadd (z[1], gfexp (x[0], 2))));
      e1 = gfadd (w0, gfadd (gfmul (w1, x[1]), gfmul (w2, gfexp (x[1], 2))));
      e1 = gfmul (e1, gfinv (gfadd (z[1], gfexp (x[1], 2))));
      e2 = gfadd (w0, gfadd (gfmul (w1, x[2]), gfmul (w2, gfexp (x[2], 2))));
      e2 = gfmul (e2, gfinv (gfadd (z[1], gfexp (x[2], 2))));

      if (n0 < 249)
	mesg[n0] = gfadd (mesg[n0], e0);
      if (n1 < 249)
	mesg[n1] = gfadd (mesg[n1], e1);
      if (n2 < 249)
	mesg[n2] = gfadd (mesg[n2], e2);

      return;

    default:

      *errcode = 4;
      return;

    }

}


/* Polynomial Solver.  Simple exhaustive search, as solving polynomials is
   generally NP - Complete anyway.
*/

void /* <-- aku */
polysolve (polynom, roots, numsol)

     unsigned char polynom[4], roots[3];
     int *numsol;

{
  extern unsigned char e2v[256];
  int i, j;
  unsigned char y;

  *numsol = 0;

  for (i = 0; i < 255; i++)
    {
      y = 0;
      for (j = 0; j < 4; j++)
	y = gfadd (y, gfmul (polynom[j], gfexp (e2v[i], j)));
      if (y == 0)
	{
	  roots[*numsol] = e2v[i];
	  *numsol = *numsol + 1;
	}
    }
}
