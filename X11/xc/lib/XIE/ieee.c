/* $Xorg: ieee.c,v 1.4 2001/02/09 02:03:41 xorgcvs Exp $ */

/******************************************************************************
				NOTICE
							      
This software is being provided by AGE Logic, Inc. under the
following license.  By obtaining, using and/or copying this software,
you agree that you have read, understood, and will comply with these
terms and conditions:

Permission to use, copy, modify, distribute and sell this
software and its documentation for any purpose and without
fee or royalty and to grant others any or all rights granted
herein is hereby granted, provided that you agree to comply
with the following copyright notice and statements, including
the disclaimer, and that the same appears on all copies and
derivative works of the software and documentation you make.
								      
	"Copyright 1993 by AGE Logic, Inc.

THIS SOFTWARE IS PROVIDED "AS IS".  AGE LOGIC MAKES NO
REPRESENTATIONS OR WARRANTIES, EXPRESS OR IMPLIED.  By way of
example, but not limitation, AGE LOGIC MAKES NO
REPRESENTATIONS OR WARRANTIES OF MERCHANTABILITY OR FITNESS
FOR ANY PARTICULAR PURPOSE OR THAT THE SOFTWARE DOES NOT
INFRINGE THIRD-PARTY PROPRIETARY RIGHTS.  AGE LOGIC
SHALL BEAR NO LIABILITY FOR ANY USE OF THIS SOFTWARE.  IN NO
EVENT SHALL EITHER PARTY BE LIABLE FOR ANY INDIRECT,
INCIDENTAL, SPECIAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOSS
OF PROFITS, REVENUE, DATA OR USE, INCURRED BY EITHER PARTY OR
ANY THIRD PARTY, WHETHER IN AN ACTION IN CONTRACT OR TORT OR
BASED ON A WARRANTY, EVEN IF AGE LOGIC OR MIT OR LICENSEES
HEREUNDER HAVE BEEN ADVISED OF THE POSSIBILITY OF SUCH
DAMAGES.

The name AGE Logic, Inc. may not be used in
advertising or publicity pertaining to this software without
specific, written prior permission from AGE Logic.

Title to this software shall at all times remain with AGE
Logic, Inc.
****************************************************************************/

/*

Copyright 1993, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall
not be used in advertising or otherwise to promote the sale, use or
other dealings in this Software without prior written authorization
from The Open Group.

*/
/* $XFree86: xc/lib/XIE/ieee.c,v 1.5 2001/12/14 19:54:34 dawes Exp $ */

#include "XIElibint.h"
#include <math.h>

#define ieeeFloatSignMask       0x80000000
#define ieeeFloatExpMask        0x7F800000
#define ieeeFloatExpShift       23
#define ieeeFloatMantissaMask   0x007FFFFF
#define ieeeMantissaSize        23

/*
        From page 2-8 of spec, IEEE format. This is laid out as:

        bit     31 = 1 bit for sign (if bit on, negative)
        bits 23-39 = 8 bit "biased" exponent (see below)
        bits  0-22 = 23 bit mantissa

        A "normal" number is formed as

                (-1)^sign_bit * 2^(exp-127) * (1.0 + .mantissa)

        That is, the mantissa is interpreted as a fractional binary
        number between 0 and 2^-1 + 2^-2 + ... + 2^-23.

        If the exponent is 255, the value is taken as infinity.

        I stole definition out of TMS Family Code tools, pages 5-22, 5-23
*/

/**********************************************************************/
xieTypFloat _XieConvertToIEEE(double native)
{
#ifndef NATIVE_FP_FORMAT
    XieFloat really_float = native; /* stupid language */
    return *((xieTypFloat *)&really_float);
#else
xieTypFloat	value;
int sign;
int exponent;
int ieee_exp;
long ieee_mantissa;
double frac_part;


	if (native == 0.0) 
		return(0);	/* frexp() can't handle 0.0 reliably */


/*** 	frexp() breaks a double into the form 

		"frac_part * 2^exponent"

	where 1/2 <= |frac_part| < 1.

***/
	sign = (native < 0);
	frac_part = frexp(native,&exponent) * (sign? -1: 1);

/***
	In IEEE, a normal number is formed as:

	(-1)^sign_bit * 2^(exp-127) * (1.0 + .mantissa)

	It is easy for us to figure out the sign bit.  To convert to
	IEEE form,  we work with the absolute value of the fractional
	part, which is between 1/2 and 1. 

	To normalize for IEEE format, the mantissa must be converted
	to be between 1 and 2 instead of 1/2 and 1. In other words,
	we re-express 

		frac_part * 2^exponent
	as
		2*frac_part * 2^(exponent-1)

	Then the IEEE mantissa is 2*frac_part - 1, and the IEEE exponent
	is given by exponent-1 = exp-127, or exp = 126+exponent.

	example:	
		The number 0.75 is expressed as f * 2^0, f=0.75.
		We convert mantissa to 2*0.75 = 1.5 and subtract 
		one to get IEEE mantissa coding of 0.5. The exponent
		is downgraded to -1 so (1 + 0.5) * 2^-1 is 0.75, 
		which is coded with IEEE bias as -1 = exp-127, 
		yielding exp=126. 0.75 = (1+0.5) * 2^(126-127),
		and we code 0 for sign bit, 0.5 for mantissa, 126
		for the exponent.

	note:	if the exponent becomes larger than 128, then 
		exp+127>255, and we can't code it any more in
		8 bits. Therefore if the exponent is >= 128,
		we set exp=255, which means infinity.
***/
	frac_part = 2*frac_part;
	--exponent;

	frac_part -= 1;

	if (exponent >= 128)
		ieee_exp = 255;
	else
		ieee_exp = 127+exponent;
		/* notice we already decremented exponent by one, above */


/***	Now assemble the number		***/
	value = 0;
	if (sign)
		value |= ieeeFloatSignMask;

	value |= (ieee_exp << ieeeFloatExpShift);

/***	For the mantissa, we know we have a fractional part between 0 and 1.
	We want the most significant 23 bits. Just shift 23 places to the 
	left and truncate.
***/
	ieee_mantissa = (pow(2.0,23.0) * frac_part);
	value |= ieee_mantissa;

	return(value);
#endif
}
/**********************************************************************/


