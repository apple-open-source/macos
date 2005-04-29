/*
 * "$Id: bit-ops.c,v 1.1.1.1 2004/07/23 06:26:31 jlovell Exp $"
 *
 *   Softweave calculator for gimp-print.
 *
 *   Copyright 2000 Charles Briscoe-Smith <cpbs@debian.org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <string.h>
#include <gimp-print/gimp-print.h>
#include "gimp-print-internal.h"
#include <gimp-print/gimp-print-intl-internal.h>
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

void
stp_fold(const unsigned char *line,
	 int single_length,
	 unsigned char *outbuf)
{
  int i;
  memset(outbuf, 0, single_length * 2);
  for (i = 0; i < single_length; i++)
    {
      unsigned char l0 = line[0];
      unsigned char l1 = line[single_length];
      if (l0 || l1)
	{
	  outbuf[0] =
	    ((l0 & (1 << 7)) >> 1) +
	    ((l0 & (1 << 6)) >> 2) +
	    ((l0 & (1 << 5)) >> 3) +
	    ((l0 & (1 << 4)) >> 4) +
	    ((l1 & (1 << 7)) >> 0) +
	    ((l1 & (1 << 6)) >> 1) +
	    ((l1 & (1 << 5)) >> 2) +
	    ((l1 & (1 << 4)) >> 3);
	  outbuf[1] =
	    ((l0 & (1 << 3)) << 3) +
	    ((l0 & (1 << 2)) << 2) +
	    ((l0 & (1 << 1)) << 1) +
	    ((l0 & (1 << 0)) << 0) +
	    ((l1 & (1 << 3)) << 4) +
	    ((l1 & (1 << 2)) << 3) +
	    ((l1 & (1 << 1)) << 2) +
	    ((l1 & (1 << 0)) << 1);
	}
      line++;
      outbuf += 2;
    }
}

static void
stpi_split_2_1(int length,
	       const unsigned char *in,
	       unsigned char *outhi,
	       unsigned char *outlo)
{
  unsigned char *outs[2];
  int i;
  int row = 0;
  int limit = length;
  outs[0] = outhi;
  outs[1] = outlo;
  memset(outs[1], 0, limit);
  for (i = 0; i < limit; i++)
    {
      unsigned char inbyte = in[i];
      outs[0][i] = 0;
      if (inbyte == 0)
	continue;
      /* For some reason gcc isn't unrolling this, even with -funroll-loops */
      if (inbyte & 1)
	{
	  outs[row][i] |= 1 & inbyte;
	  row = row ^ 1;
	}
      if (inbyte & (1 << 1))
	{
	  outs[row][i] |= (1 << 1) & inbyte;
	  row = row ^ 1;
	}
      if (inbyte & (1 << 2))
	{
	  outs[row][i] |= (1 << 2) & inbyte;
	  row = row ^ 1;
	}
      if (inbyte & (1 << 3))
	{
	  outs[row][i] |= (1 << 3) & inbyte;
	  row = row ^ 1;
	}
      if (inbyte & (1 << 4))
	{
	  outs[row][i] |= (1 << 4) & inbyte;
	  row = row ^ 1;
	}
      if (inbyte & (1 << 5))
	{
	  outs[row][i] |= (1 << 5) & inbyte;
	  row = row ^ 1;
	}
      if (inbyte & (1 << 6))
	{
	  outs[row][i] |= (1 << 6) & inbyte;
	  row = row ^ 1;
	}
      if (inbyte & (1 << 7))
	{
	  outs[row][i] |= (1 << 7) & inbyte;
	  row = row ^ 1;
	}
    }
}

static void
stp_split_2_2(int length,
	      const unsigned char *in,
	      unsigned char *outhi,
	      unsigned char *outlo)
{
  unsigned char *outs[2];
  int i;
  unsigned row = 0;
  int limit = length * 2;
  outs[0] = outhi;
  outs[1] = outlo;
  memset(outs[1], 0, limit);
  for (i = 0; i < limit; i++)
    {
      unsigned char inbyte = in[i];
      outs[0][i] = 0;
      if (inbyte == 0)
	continue;
      /* For some reason gcc isn't unrolling this, even with -funroll-loops */
      if (inbyte & 3)
	{
	  outs[row][i] |= (3 & inbyte);
	  row = row ^ 1;
	}
      if (inbyte & (3 << 2))
	{
	  outs[row][i] |= ((3 << 2) & inbyte);
	  row = row ^ 1;
	}
      if (inbyte & (3 << 4))
	{
	  outs[row][i] |= ((3 << 4) & inbyte);
	  row = row ^ 1;
	}
      if (inbyte & (3 << 6))
	{
	  outs[row][i] |= ((3 << 6) & inbyte);
	  row = row ^ 1;
	}
    }
}

void
stp_split_2(int length,
	    int bits,
	    const unsigned char *in,
	    unsigned char *outhi,
	    unsigned char *outlo)
{
  if (bits == 2)
    stp_split_2_2(length, in, outhi, outlo);
  else
    stpi_split_2_1(length, in, outhi, outlo);
}

static void
stpi_split_4_1(int length,
	       const unsigned char *in,
	       unsigned char *out0,
	       unsigned char *out1,
	       unsigned char *out2,
	       unsigned char *out3)
{
  unsigned char *outs[4];
  int i;
  int row = 0;
  int limit = length;
  outs[0] = out0;
  outs[1] = out1;
  outs[2] = out2;
  outs[3] = out3;
  memset(outs[1], 0, limit);
  memset(outs[2], 0, limit);
  memset(outs[3], 0, limit);
  for (i = 0; i < limit; i++)
    {
      unsigned char inbyte = in[i];
      outs[0][i] = 0;
      if (inbyte == 0)
	continue;
      /* For some reason gcc isn't unrolling this, even with -funroll-loops */
      if (inbyte & 1)
	{
	  outs[row][i] |= 1 & inbyte;
	  row = (row + 1) & 3;
	}
      if (inbyte & (1 << 1))
	{
	  outs[row][i] |= (1 << 1) & inbyte;
	  row = (row + 1) & 3;
	}
      if (inbyte & (1 << 2))
	{
	  outs[row][i] |= (1 << 2) & inbyte;
	  row = (row + 1) & 3;
	}
      if (inbyte & (1 << 3))
	{
	  outs[row][i] |= (1 << 3) & inbyte;
	  row = (row + 1) & 3;
	}
      if (inbyte & (1 << 4))
	{
	  outs[row][i] |= (1 << 4) & inbyte;
	  row = (row + 1) & 3;
	}
      if (inbyte & (1 << 5))
	{
	  outs[row][i] |= (1 << 5) & inbyte;
	  row = (row + 1) & 3;
	}
      if (inbyte & (1 << 6))
	{
	  outs[row][i] |= (1 << 6) & inbyte;
	  row = (row + 1) & 3;
	}
      if (inbyte & (1 << 7))
	{
	  outs[row][i] |= (1 << 7) & inbyte;
	  row = (row + 1) & 3;
	}
    }
}

static void
stpi_split_4_2(int length,
	       const unsigned char *in,
	       unsigned char *out0,
	       unsigned char *out1,
	       unsigned char *out2,
	       unsigned char *out3)
{
  unsigned char *outs[4];
  int i;
  int row = 0;
  int limit = length * 2;
  outs[0] = out0;
  outs[1] = out1;
  outs[2] = out2;
  outs[3] = out3;
  memset(outs[1], 0, limit);
  memset(outs[2], 0, limit);
  memset(outs[3], 0, limit);
  for (i = 0; i < limit; i++)
    {
      unsigned char inbyte = in[i];
      outs[0][i] = 0;
      if (inbyte == 0)
	continue;
      /* For some reason gcc isn't unrolling this, even with -funroll-loops */
      if (inbyte & 3)
	{
	  outs[row][i] |= 3 & inbyte;
	  row = (row + 1) & 3;
	}
      if (inbyte & (3 << 2))
	{
	  outs[row][i] |= (3 << 2) & inbyte;
	  row = (row + 1) & 3;
	}
      if (inbyte & (3 << 4))
	{
	  outs[row][i] |= (3 << 4) & inbyte;
	  row = (row + 1) & 3;
	}
      if (inbyte & (3 << 6))
	{
	  outs[row][i] |= (3 << 6) & inbyte;
	  row = (row + 1) & 3;
	}
    }
}

void
stp_split_4(int length,
	    int bits,
	    const unsigned char *in,
	    unsigned char *out0,
	    unsigned char *out1,
	    unsigned char *out2,
	    unsigned char *out3)
{
  if (bits == 2)
    stpi_split_4_2(length, in, out0, out1, out2, out3);
  else
    stpi_split_4_1(length, in, out0, out1, out2, out3);
}


#if __BYTE_ORDER == __LITTLE_ENDIAN
#define SH20 0
#define SH21 8
#else
#define SH20 8
#define SH21 0
#endif

static void
stpi_unpack_2_1(int length,
		const unsigned char *in,
		unsigned char *out0,
		unsigned char *out1)
{
  unsigned char	tempin, bit, temp0, temp1;

  if (length <= 0)
    return;
  for (bit = 128, temp0 = 0, temp1 = 0;
       length > 0;
       length --)
    {
      tempin = *in++;

      if (tempin & 128)
        temp0 |= bit;
      if (tempin & 64)
        temp1 |= bit;
      bit >>= 1;
      if (tempin & 32)
        temp0 |= bit;
      if (tempin & 16)
        temp1 |= bit;
      bit >>= 1;
      if (tempin & 8)
        temp0 |= bit;
      if (tempin & 4)
        temp1 |= bit;
      bit >>= 1;
      if (tempin & 2)
        temp0 |= bit;
      if (tempin & 1)
        temp1 |= bit;

      if (bit > 1)
        bit >>= 1;
      else
      {
        bit     = 128;
	*out0++ = temp0;
	*out1++ = temp1;

	temp0   = 0;
	temp1   = 0;
      }
    }

  if (bit < 128)
    {
      *out0++ = temp0;
      *out1++ = temp1;
    }
}

static void
stpi_unpack_2_2(int length,
	       const unsigned char *in,
	       unsigned char *out0,
	       unsigned char *out1)
{
  if (length <= 0)
    return;

  for (;length;length --)
    {
      unsigned char ti0, ti1;
      ti0 = in[0];
      ti1 = in[1];

      *out0++  = (ti0 & 0xc0) << 0
	| (ti0 & 0x0c) << 2
	| (ti1 & 0xc0) >> 4
	| (ti1 & 0x0c) >> 2;
      *out1++  = (ti0 & 0x30) << 2
	| (ti0 & 0x03) << 4
	| (ti1 & 0x30) >> 2
	| (ti1 & 0x03) >> 0;
      in += 2;
    }
}

void
stp_unpack_2(int length,
	     int bits,
	     const unsigned char *in,
	     unsigned char *outlo,
	     unsigned char *outhi)
{
  if (bits == 1)
    stpi_unpack_2_1(length, in, outlo, outhi);
  else
    stpi_unpack_2_2(length, in, outlo, outhi);
}

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define SH40 0
#define SH41 8
#define SH42 16
#define SH43 24
#else
#define SH40 24
#define SH41 16
#define SH42 8
#define SH43 0
#endif

static void
stpi_unpack_4_1(int length,
		 const unsigned char *in,
		 unsigned char *out0,
		 unsigned char *out1,
		 unsigned char *out2,
		 unsigned char *out3)
{
  unsigned char	tempin, bit, temp0, temp1, temp2, temp3;

  if (length <= 0)
    return;
  for (bit = 128, temp0 = 0, temp1 = 0, temp2 = 0, temp3 = 0;
       length > 0;
       length --)
    {
      tempin = *in++;

      if (tempin & 128)
        temp0 |= bit;
      if (tempin & 64)
        temp1 |= bit;
      if (tempin & 32)
        temp2 |= bit;
      if (tempin & 16)
        temp3 |= bit;
      bit >>= 1;
      if (tempin & 8)
        temp0 |= bit;
      if (tempin & 4)
        temp1 |= bit;
      if (tempin & 2)
        temp2 |= bit;
      if (tempin & 1)
        temp3 |= bit;

      if (bit > 1)
        bit >>= 1;
      else
      {
        bit     = 128;
	*out0++ = temp0;
	*out1++ = temp1;
	*out2++ = temp2;
	*out3++ = temp3;

	temp0   = 0;
	temp1   = 0;
	temp2   = 0;
	temp3   = 0;
      }
    }

  if (bit < 128)
    {
      *out0++ = temp0;
      *out1++ = temp1;
      *out2++ = temp2;
      *out3++ = temp3;
    }
}

static void
stpi_unpack_4_2(int length,
		 const unsigned char *in,
		 unsigned char *out0,
		 unsigned char *out1,
		 unsigned char *out2,
		 unsigned char *out3)
{
  unsigned char	tempin,
		shift,
		temp0,
		temp1,
		temp2,
		temp3;

  length *= 2;

  for (shift = 0, temp0 = 0, temp1 = 0, temp2 = 0, temp3 = 0;
       length > 0;
       length --)
    {
     /*
      * Note - we can't use (tempin & N) >> (shift - M) since negative
      * right-shifts are not always implemented.
      */

      tempin = *in++;

      if (tempin & 192)
        temp0 |= (tempin & 192) >> shift;
      if (tempin & 48)
        temp1 |= ((tempin & 48) << 2) >> shift;
      if (tempin & 12)
        temp2 |= ((tempin & 12) << 4) >> shift;
      if (tempin & 3)
        temp3 |= ((tempin & 3) << 6) >> shift;

      if (shift < 6)
        shift += 2;
      else
      {
        shift   = 0;
	*out0++ = temp0;
	*out1++ = temp1;
	*out2++ = temp2;
	*out3++ = temp3;

	temp0   = 0;
	temp1   = 0;
	temp2   = 0;
	temp3   = 0;
      }
    }

  if (shift)
    {
      *out0++ = temp0;
      *out1++ = temp1;
      *out2++ = temp2;
      *out3++ = temp3;
    }
}

void
stp_unpack_4(int length,
	     int bits,
	     const unsigned char *in,
	     unsigned char *out0,
	     unsigned char *out1,
	     unsigned char *out2,
	     unsigned char *out3)
{
  if (bits == 1)
    stpi_unpack_4_1(length, in, out0, out1, out2, out3);
  else
    stpi_unpack_4_2(length, in, out0, out1, out2, out3);
}

static void
stpi_unpack_8_1(int length,
		const unsigned char *in,
		unsigned char *out0,
		unsigned char *out1,
		unsigned char *out2,
		unsigned char *out3,
		unsigned char *out4,
		unsigned char *out5,
		unsigned char *out6,
		unsigned char *out7)
{
  unsigned char	tempin, bit, temp0, temp1, temp2, temp3, temp4, temp5, temp6,
    temp7;

  if (length <= 0)
    return;

  for (bit = 128, temp0 = 0, temp1 = 0, temp2 = 0,
       temp3 = 0, temp4 = 0, temp5 = 0, temp6 = 0, temp7 = 0;
       length > 0;
       length --)
    {
      tempin = *in++;

      if (tempin & 128)
        temp0 |= bit;
      if (tempin & 64)
        temp1 |= bit;
      if (tempin & 32)
        temp2 |= bit;
      if (tempin & 16)
        temp3 |= bit;
      if (tempin & 8)
        temp4 |= bit;
      if (tempin & 4)
        temp5 |= bit;
      if (tempin & 2)
        temp6 |= bit;
      if (tempin & 1)
        temp7 |= bit;

      if (bit > 1)
        bit >>= 1;
      else
      {
        bit     = 128;
	*out0++ = temp0;
	*out1++ = temp1;
	*out2++ = temp2;
	*out3++ = temp3;
	*out4++ = temp4;
	*out5++ = temp5;
	*out6++ = temp6;
	*out7++ = temp7;

	temp0   = 0;
	temp1   = 0;
	temp2   = 0;
	temp3   = 0;
	temp4   = 0;
	temp5   = 0;
	temp6   = 0;
	temp7   = 0;
      }
    }

  if (bit < 128)
    {
      *out0++ = temp0;
      *out1++ = temp1;
      *out2++ = temp2;
      *out3++ = temp3;
      *out4++ = temp4;
      *out5++ = temp5;
      *out6++ = temp6;
      *out7++ = temp7;
    }
}

static void
stpi_unpack_8_2(int length,
		const unsigned char *in,
		unsigned char *out0,
		unsigned char *out1,
		unsigned char *out2,
		unsigned char *out3,
		unsigned char *out4,
		unsigned char *out5,
		unsigned char *out6,
		unsigned char *out7)
{
  unsigned char	tempin,
		shift,
		temp0,
		temp1,
		temp2,
		temp3,
		temp4,
		temp5,
		temp6,
		temp7;


  for (shift = 0, temp0 = 0, temp1 = 0,
       temp2 = 0, temp3 = 0, temp4 = 0, temp5 = 0, temp6 = 0, temp7 = 0;
       length > 0;
       length --)
    {
     /*
      * Note - we can't use (tempin & N) >> (shift - M) since negative
      * right-shifts are not always implemented.
      */

      tempin = *in++;

      if (tempin & 192)
        temp0 |= (tempin & 192) >> shift;
      if (tempin & 48)
        temp1 |= ((tempin & 48) << 2) >> shift;
      if (tempin & 12)
        temp2 |= ((tempin & 12) << 4) >> shift;
      if (tempin & 3)
        temp3 |= ((tempin & 3) << 6) >> shift;

      tempin = *in++;

      if (tempin & 192)
        temp4 |= (tempin & 192) >> shift;
      if (tempin & 48)
        temp5 |= ((tempin & 48) << 2) >> shift;
      if (tempin & 12)
        temp6 |= ((tempin & 12) << 4) >> shift;
      if (tempin & 3)
        temp7 |= ((tempin & 3) << 6) >> shift;

      if (shift < 6)
        shift += 2;
      else
      {
        shift   = 0;
	*out0++ = temp0;
	*out1++ = temp1;
	*out2++ = temp2;
	*out3++ = temp3;
	*out4++ = temp4;
	*out5++ = temp5;
	*out6++ = temp6;
	*out7++ = temp7;

	temp0   = 0;
	temp1   = 0;
	temp2   = 0;
	temp3   = 0;
	temp4   = 0;
	temp5   = 0;
	temp6   = 0;
	temp7   = 0;
      }
    }

  if (shift)
    {
      *out0++ = temp0;
      *out1++ = temp1;
      *out2++ = temp2;
      *out3++ = temp3;
      *out4++ = temp4;
      *out5++ = temp5;
      *out6++ = temp6;
      *out7++ = temp7;
    }
}

void
stp_unpack_8(int length,
	     int bits,
	     const unsigned char *in,
	     unsigned char *out0,
	     unsigned char *out1,
	     unsigned char *out2,
	     unsigned char *out3,
	     unsigned char *out4,
	     unsigned char *out5,
	     unsigned char *out6,
	     unsigned char *out7)
{
  if (bits == 1)
    stpi_unpack_8_1(length, in, out0, out1, out2, out3,
		     out4, out5, out6, out7);
  else
    stpi_unpack_8_2(length, in, out0, out1, out2, out3,
		     out4, out5, out6, out7);
}

static void
find_first_and_last(const unsigned char *line, int length,
		    int *first, int *last)
{
  int i;
  int found_first = 0;
  if (!first || !last)
    return;
  *first = 0;
  *last = 0;
  for (i = 0; i < length; i++)
    {
      if (line[i] == 0)
	{
	  if (!found_first)
	    (*first)++;
	}
      else
	{
	  *last = i;
	  found_first = 1;
	}
    }
}

int
stp_pack_uncompressed(stp_vars_t *v,
		      const unsigned char *line,
		      int length,
		      unsigned char *comp_buf,
		      unsigned char **comp_ptr,
		      int *first,
		      int *last)
{
  find_first_and_last(line, length, first, last);
  memcpy(comp_buf, line, length);
  *comp_ptr = comp_buf + length;
  if (first > last)
    return 0;
  else
    return 1;
}

int
stp_pack_tiff(stp_vars_t *v,
	      const unsigned char *line,
	      int length,
	      unsigned char *comp_buf,
	      unsigned char **comp_ptr,
	      int *first,
	      int *last)
{
  const unsigned char *start;		/* Start of compressed data */
  unsigned char repeat;			/* Repeating char */
  int count;			/* Count of compressed bytes */
  int tcount;			/* Temporary count < 128 */
  register const unsigned char *xline = line;
  register int xlength = length;
  find_first_and_last(line, length, first, last);

  /*
   * Compress using TIFF "packbits" run-length encoding...
   */

  (*comp_ptr) = comp_buf;

  while (xlength > 0)
    {
      /*
       * Get a run of non-repeated chars...
       */

      start  = xline;
      xline   += 2;
      xlength -= 2;

      while (xlength > 0 && (xline[-2] != xline[-1] || xline[-1] != xline[0]))
	{
	  xline ++;
	  xlength --;
	}

      xline   -= 2;
      xlength += 2;

      /*
       * Output the non-repeated sequences (max 128 at a time).
       */

      count = xline - start;
      while (count > 0)
	{
	  tcount = count > 128 ? 128 : count;

	  (*comp_ptr)[0] = tcount - 1;
	  memcpy((*comp_ptr) + 1, start, tcount);

	  (*comp_ptr) += tcount + 1;
	  start    += tcount;
	  count    -= tcount;
	}

      if (xlength <= 0)
	break;

      /*
       * Find the repeated sequences...
       */

      start  = xline;
      repeat = xline[0];

      xline ++;
      xlength --;

      if (xlength > 0)
	{
	  int ylength = xlength;
	  while (ylength && *xline == repeat)
	    {
	      xline ++;
	      ylength --;
	    }
	  xlength = ylength;
	}

      /*
       * Output the repeated sequences (max 128 at a time).
       */

      count = xline - start;
      while (count > 0)
	{
	  tcount = count > 128 ? 128 : count;

	  (*comp_ptr)[0] = 1 - tcount;
	  (*comp_ptr)[1] = repeat;

	  (*comp_ptr) += 2;
	  count    -= tcount;
	}
    }
  if (first && last && *first > *last)
    return 0;
  else
    return 1;
}
