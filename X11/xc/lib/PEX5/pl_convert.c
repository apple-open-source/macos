/* $Xorg: pl_convert.c,v 1.4 2001/02/09 02:03:27 xorgcvs Exp $ */
/*

Copyright 1992, 1998  The Open Group

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

/******************************************************************************
FOR VAX/IEEE conversions:

Copyright 1988-1991
Center for Information Technology Integration (CITI)
Information Technology Division
University of Michigan
Ann Arbor, Michigan
                        All Rights Reserved
Permission to use, copy, modify, and distribute this software and
its documentation for any purpose and without fee is hereby
granted, provided that the above copyright notice appear in all
copies and that both that copyright notice and this permission
notice appear in supporting documentation, and that the names of
CITI or THE UNIVERSITY OF MICHIGAN not be used in advertising or
publicity pertaining to distribution of the software without
specific, written prior permission.

THE SOFTWARE IS PROVIDED "AS IS." CITI AND THE UNIVERSITY OF
MICHIGAN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
NO EVENT SHALL CITI OR THE UNIVERSITY OF MICHIGAN BE LIABLE FOR ANY
SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************************/

#include "PEXlib.h"
#include "PEXlibint.h"


void _PEXIEEE32toDECF (srcVal, dstVal)

INPUT char	*srcVal;
OUTPUT char	*dstVal;

{
    register CARD32 IEEEnum = *(CARD32 *) srcVal;
    CARD32 *IEEEnumP = (CARD32 *) dstVal;
    CARD32 result;

    if ((IEEE_SIGN_MASK & IEEEnum) == MAX_IEEE_POSITIVE)
    {
	*IEEEnumP = MAX_VAX_POSITIVE | (0x80000000&IEEEnum)>>16;
	return;
    };
    
    if ((IEEE_SIGN_MASK & IEEEnum) == MIN_IEEE_POSITIVE)
    {
	*IEEEnumP = MIN_VAX_POSITIVE;
	return;
    };

    /*
     * these bitfields should OR into mutually exclusive fields in
     * result field.
     */

    result = ((((BITMASK(8)<<23) & IEEEnum)>>23)+IEEE_TO_VAX_BIAS)<<7;
    result |= ((BITMASK(7)<<16)&IEEEnum)>>16;
    result |= (BITMASK(16)&IEEEnum)<<16;
    result |= (0x80000000&IEEEnum)>>16;
    *IEEEnumP = result;
}



void _PEXDECFtoIEEE32 (srcVal, dstVal)

INPUT char	*srcVal;
OUTPUT char	*dstVal;

{
    register CARD32 Vaxnum = *(CARD32 *) srcVal;
    CARD32 *VaxnumP = (CARD32 *) dstVal;
    CARD32 result;

    if ((VAX_SIGN_MASK & Vaxnum) == MAX_VAX_POSITIVE)
    {
	*VaxnumP = MAX_IEEE_POSITIVE |
	    (((0x00008000)&Vaxnum) ? 0x80000000 : 0L);
	return;
    }
    
    if ((VAX_SIGN_MASK & Vaxnum) == MIN_VAX_POSITIVE)
    {
	*VaxnumP = MIN_IEEE_POSITIVE;
	return;
    }

    /*
     * these bitfields should OR into mutually exclusive fields in
     * result field.
     */
    
    result = ((((BITMASK(8)<<7) & Vaxnum)>>7)+VAX_TO_IEEE_BIAS)<<23;
    result |= (((BITMASK(7) & Vaxnum)<<16) |
	       (((BITMASK(16)<<16) & Vaxnum)>>16));
    result |= ( (0x00008000 & Vaxnum) ? 0x80000000 : 0L);
    *VaxnumP = result;
}



#ifdef CRAY

void _PEXIEEE32toCRAY (srcVal, dstVal)

INPUT char	*srcVal;
OUTPUT char	*dstVal;

{
    unsigned char *PC = (unsigned char *) srcVal;
    float *result = (float *) dstVal;

    union {
	struct ieee_single is;
	long l;
    } c;

    union {
	struct cray_single vc;
	float iis;
    } ieee;


    c.l = PC[0] << 24 | PC[1] << 16 | PC[2] << 8 | PC[3];
    if (PC[0] & 0x80)
	c.l |= ~0xffffffff;

    if (c.is.exp == 0)
    {
	ieee.iis = 0.0;
    }
    else if (c.is.exp == 0xff)
    {
	/*
	 * If the IEEE float we are decoding indicates
	 * an IEEE overflow condition, we manufacture
	 * a Cray overflow condition.
	 */  

	SET_MAX_SNG_CRAY (ieee.vc);
    }
    else
    {
	ieee.vc.sign = c.is.sign;
	ieee.vc.exp = c.is.exp - IEEE_SNG_BIAS + CRAY_BIAS;
	ieee.vc.mantissa = c.is.mantissa | (1 << 23);
	ieee.vc.mantissa2 = 0;
    }

    *result = ieee.iis;
}



void _PEXCRAYtoIEEE32 (srcVal, dstVal)

INPUT char	*srcVal;
OUTPUT char	*dstVal;

{
    unsigned char *PC = (unsigned char *) dstVal;
    struct cray_single vc;
    float *fptr;

    struct ieee_single ais;
    union {
	struct ieee_single is;
	unsigned iis;
    } ieee;

    fptr = (float *) &vc;
    *fptr = *((float *) srcVal);

    if (vc.exp >= MAX_CRAY_SNG)
    {
	SET_MAX_SNG_IEEE (ieee.is);
    }
    else if (vc.exp < MIN_CRAY_SNG ||
        (vc.mantissa == 0 && vc.mantissa2 == 0))
    {
	/*
	 * On the Cray, there is no hidden mantissa bit.
	 * So, if the mantissa is zero, the number is zero.
	 */

	SET_MIN_SNG_IEEE (ieee.is);
    }
    else
    {
	ieee.is.exp = vc.exp - CRAY_BIAS + IEEE_SNG_BIAS;
	ieee.is.mantissa = vc.mantissa;
	/* Hidden bit removed by truncation */
    }

    ieee.is.sign = vc.sign;

    PC[0] = ieee.iis >> 24;
    PC[1] = ieee.iis >> 16;
    PC[2] = ieee.iis >> 8;
    PC[3] = ieee.iis;
}

#endif /* CRAY */
