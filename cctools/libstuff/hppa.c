/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
#include "stuff/hppa.h"
/*
 * calc_hppa_HILO() is the specific calculation for all left/right type relocs
 * for the hppa relocation for the hp field selectors LR% RR% which allow
 * sharing of the LR% when the round value of the offset is the same.
 * See hppa/reloc.h for more infomation.
 */
__private_extern__
void
calc_hppa_HILO(
unsigned long base,
unsigned long offset,
unsigned long *left21,
unsigned long *right14)
{
    unsigned long rounded;

	rounded = (offset + (0x2000/2)) & ~(0x2000 - 1);

	*left21   = (base + rounded) & 0xfffff800;
	*right14 = ((base + rounded) & 0x000007ff) + (offset - rounded);
}

/* 
 * 2 helper routines for branch displacement calculations on hppa
 */
__private_extern__
unsigned long
assemble_17(
unsigned long x,
unsigned long y,
unsigned long z)
{
    unsigned long temp;

	temp = ( ( z &     1 ) << 16 ) |
	       ( ( x &  0x1f ) << 11 ) |
	       ( ( y &     1 ) << 10 ) |
	       ( ( y & 0x7fe ) >> 1);
	if(z)
	    temp |= 0xfffe0000;   /* sign extend it */
	return(temp);
}

__private_extern__
unsigned long
assemble_21(
unsigned long x)
{
    unsigned long temp;

	temp = ( ( x &        1 ) << 20 ) |
	       ( ( x &    0xffe ) <<  8 ) |
	       ( ( x &   0xc000 ) >>  7 ) |
	       ( ( x & 0x1f0000 ) >> 14 ) |
	       ( ( x & 0x003000 ) >> 12 );
	return(temp & 0x1fffff);
}

/*
 * The following functions are all from hppa_ctrl_funcs.c in the assembler.
 */
__private_extern__
unsigned long
assemble_12(
unsigned long x,
unsigned long y)
{
    unsigned long temp;

	temp = ( ( y     & 1 ) << 11 ) |
	       ( ( x     & 1 ) << 10 ) |
	       ( ( x & 0x7fe ) >> 1);
	return(temp & 0xfff);
}

__private_extern__
unsigned long
assemble_3(
unsigned long x)
{
    unsigned long temp;

	temp = ( ( x & 1 ) << 2 ) |
	       ( ( x & 6 ) >> 1 );
	return(temp & 7);
}

__private_extern__
unsigned long
sign_ext(
unsigned long x,
unsigned long len)
{
    unsigned long sign;
    unsigned long result;
    unsigned long len_ones;
    unsigned long i;

	i = 0;
	len_ones = 0;
	while(i < len){
	    len_ones = (len_ones << 1) | 1;
	    i++;
	}

	sign = (x >> (len-1)) & 1;

	if(sign)
	    result = ( ~0 ^ len_ones ) | ( len_ones & x );
	else
	    result = len_ones & x;

	return(result);
}

static
unsigned long 
ones(
unsigned long n)
{
    unsigned long len_ones;
    unsigned long i;

	i = 0;
	len_ones = 0;
	while(i < n){
	    len_ones = (len_ones << 1) | 1;
	    i++;
	}
	return(len_ones);
}

__private_extern__
unsigned long
low_sign_ext(
unsigned long x,
unsigned long len)
{
    unsigned long temp1, temp2;
    unsigned long len_ones;

	len_ones = ones(len);

	temp1 = ( x & 1 ) << (len-1);
	temp2 = ( ( x & 0xfffffffe ) & len_ones ) >> 1;
	return(sign_ext( (temp1 | temp2),len));
}

__private_extern__
unsigned long
dis_assemble_21(
unsigned long as21)
{
    unsigned long temp;

	temp  = ( as21 & 0x100000 ) >> 20;
	temp |= ( as21 & 0x0ffe00 ) >> 8;
	temp |= ( as21 & 0x000180 ) << 7;
	temp |= ( as21 & 0x00007c ) << 14;
	temp |= ( as21 & 0x000003 ) << 12;
	return(temp);
}

__private_extern__
unsigned long
low_sign_unext(
unsigned long x,
unsigned long len)
{
    unsigned long temp;
    unsigned long sign;
    unsigned long rest;
    unsigned long one_bit_at_len;
    unsigned long len_ones;

	len_ones = ones(len);
	one_bit_at_len = 1 << (len-1);

	temp = sign_unext(x, len);
	sign = temp & one_bit_at_len;
	sign >>= (len - 1);

	rest = temp & ( len_ones ^ one_bit_at_len );
	rest <<= 1;

	return(rest | sign);
}

__private_extern__
void
dis_assemble_17(
unsigned long as17,
unsigned long *x,
unsigned long *y,
unsigned long *z)
{
	*z =   ( as17 & 0x10000 ) >> 16;
	*x =   ( as17 & 0x0f800 ) >> 11;
	*y = ( ( as17 & 0x00400 ) >> 10 ) | ( ( as17 & 0x3ff ) << 1 );
}

__private_extern__
unsigned long
sign_unext(
unsigned long x,
unsigned long len)
{
    unsigned long len_ones;

	len_ones = ones(len);
	return(x & len_ones);
}

__private_extern__
unsigned long
dis_assemble_3(
unsigned long x)
{
    unsigned long r;

	r = ( ( (x & 4 ) >> 2 ) | ( ( x & 3 ) << 1 ) ) & 7;
	return(r);
}

__private_extern__
void
dis_assemble_12(
unsigned long as12,
unsigned long *x,
unsigned long *y)
{
	*y =   ( as12 & 0x800 ) >> 11;
	*x = ( ( as12 & 0x3ff ) << 1 ) | ( ( as12 & 0x400 ) >> 10 );
}
