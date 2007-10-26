/*
 * "$Id: bit-ops.h,v 1.3 2007/03/08 13:34:27 faust3 Exp $"
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

/**
 * @file gutenprint/bit-ops.h
 * @brief Bit operations.
 */

#ifndef GUTENPRINT_BIT_OPS_H
#define GUTENPRINT_BIT_OPS_H

#ifdef __cplusplus
extern "C" {
#endif

extern void	stp_fold(const unsigned char *line, int single_length,
			 unsigned char *outbuf);

extern void	stp_fold_3bit(const unsigned char *line, int single_length,
			 unsigned char *outbuf);

extern void	stp_fold_3bit_323(const unsigned char *line, int single_length,
			 unsigned char *outbuf);

extern void	stp_fold_4bit(const unsigned char *line, int single_length,
			 unsigned char *outbuf);

extern void	stp_split_2(int height, int bits, const unsigned char *in,
			    unsigned char *outhi, unsigned char *outlo);

extern void	stp_split_4(int height, int bits, const unsigned char *in,
			    unsigned char *out0, unsigned char *out1,
			    unsigned char *out2, unsigned char *out3);

extern void	stp_unpack_2(int height, int bits, const unsigned char *in,
			     unsigned char *outlo, unsigned char *outhi);

extern void	stp_unpack_4(int height, int bits, const unsigned char *in,
			     unsigned char *out0, unsigned char *out1,
			     unsigned char *out2, unsigned char *out3);

extern void	stp_unpack_8(int height, int bits, const unsigned char *in,
			     unsigned char *out0, unsigned char *out1,
			     unsigned char *out2, unsigned char *out3,
			     unsigned char *out4, unsigned char *out5,
			     unsigned char *out6, unsigned char *out7);

extern void	stp_unpack_16(int height, int bits, const unsigned char *in,
			      unsigned char *out0, unsigned char *out1,
			      unsigned char *out2, unsigned char *out3,
			      unsigned char *out4, unsigned char *out5,
			      unsigned char *out6, unsigned char *out7,
			      unsigned char *out8, unsigned char *out9,
			      unsigned char *out10, unsigned char *out11,
			      unsigned char *out12, unsigned char *out13,
			      unsigned char *out14, unsigned char *out15);

#ifdef __cplusplus
  }
#endif

#endif /* GUTENPRINT_BIT_OPS_H */
