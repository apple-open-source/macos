/* $Xorg: pl_convert.h,v 1.4 2001/02/09 02:03:27 xorgcvs Exp $ */
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

/* -------------------------------------------------------------------------
 * Structures for fiddling with 16 and 32 bit data on 64 bit machines
 * ------------------------------------------------------------------------- */

#ifdef WORD64

typedef struct {
    int value   :32;
} Long;

typedef struct {
    int value   :16;
    int pad     :16;
} Short;
 
typedef struct {
    int value   :8;
    int pad     :24;
} Char;

typedef struct {
    unsigned int value :32;
} ULong;

typedef struct {
    unsigned int value :16;
} UShort;

typedef struct {
    unsigned char value;
} UChar;

#endif /* WORD64 */



/* -------------------------------------------------------------------------
 * Floating point conversion macros
 * ------------------------------------------------------------------------- */

/*
 * Host->Network
 * _srcVal = host floating point value
 * _dstBuf = buffer to store network float
 */

#define FP_CONVERT_HTON_BUFF(_srcVal, _dstBuf, _fpFormat) \
{ \
    PEX_fp_convert[NATIVE_FP_FORMAT-1][_fpFormat-1](&(_srcVal), _dstBuf); \
}


/*
 * Host->Network
 * _srcVal = host floating point value
 * _dstVal = store network float in this value (on 64 bit machines,
 *           _dstVal is most probably a bitfield, so we can't take &_dstVal.
 */

#ifndef WORD64

#define FP_CONVERT_HTON(_srcVal, _dstVal, _fpFormat) \
{ \
    FP_CONVERT_HTON_BUFF (_srcVal, &(_dstVal), _fpFormat); \
}

#define FP_CONVERT_DHTON(_srcVal, _dstVal, _fpFormat) \
{ \
    float single = _srcVal; \
    FP_CONVERT_HTON_BUFF (single, &(_dstVal), _fpFormat); \
}

#else /* WORD64 */

#define FP_CONVERT_HTON(_srcVal, _dstVal, _fpFormat) \
{ \
    Long temp; \
    FP_CONVERT_HTON_BUFF (_srcVal, &temp, _fpFormat); \
    _dstVal = temp.value; \
}

#define FP_CONVERT_DHTON(_srcVal, _dstVal, _fpFormat) \
{ \
    float single = _srcVal; \
    Long temp; \
    FP_CONVERT_HTON_BUFF (single, &temp, _fpFormat); \
    _dstVal = temp.value; \
}

#endif /* WORD64 */


/*
 * Network->Host
 * _srcBuf = buffer containing network floating point value
 * _dstVal = store host float in this value
 */

#define FP_CONVERT_NTOH_BUFF(_srcBuf, _dstVal, _fpFormat) \
{ \
    PEX_fp_convert[_fpFormat-1][NATIVE_FP_FORMAT-1](_srcBuf, &_dstVal); \
}


/*
 * Network->Host
 * _srcVal = network floating point value
 * _dstVal = store host float in this value
 */

#ifndef WORD64

#define FP_CONVERT_NTOH(_srcVal, _dstVal, _fpFormat) \
{ \
    FP_CONVERT_NTOH_BUFF (&(_srcVal), _dstVal, _fpFormat); \
}

#else /* WORD64 */

#define FP_CONVERT_NTOH(_srcVal, _dstVal, _fpFormat) \
{ \
    Long temp; \
    temp.value = _srcVal; \
    FP_CONVERT_NTOH_BUFF (&temp, _dstVal, _fpFormat); \
}

#endif /* WORD64 */



/* -------------------------------------------------------------------------
 * Macros for 64 bit conversion
 * ------------------------------------------------------------------------- */

#ifdef WORD64

#define CARD64_TO_32(_val, _pBuf) \
{ \
    Long _d; \
    _d.value = _val; \
    memcpy (_pBuf, &_d, SIZEOF (CARD32)); \
}

#define CARD64_TO_16(_val, _pBuf) \
{ \
    Short _d; \
    _d.value = _val; \
    memcpy (_pBuf, &_d, SIZEOF (CARD16)); \
}

#define INT64_TO_32(_val, _pBuf) \
{ \
    Long _d; \
    _d.value = _val; \
    memcpy (_pBuf, &_d, SIZEOF (INT32)); \
}

#define INT64_TO_16(_val, _pBuf) \
{ \
    Short _d; \
    _d.value = _val; \
    memcpy (_pBuf, &_d, SIZEOF (INT16)); \
}


#define CARD32_TO_64(_pBuf, _val) \
{ \
    _val = *(_pBuf + 0) & 0xff; 	/* 0xff incase _pBuf is signed */ \
    _val <<= 8; \
    _val |= *(_pBuf + 1) & 0xff;\
    _val <<= 8; \
    _val |= *(_pBuf + 2) & 0xff;\
    _val <<= 8; \
    _val |= *(_pBuf + 3) & 0xff;\
}

#define CARD16_TO_64(_pBuf, _val) \
{ \
    _val = *(_pBuf + 0) & 0xff; 	/* 0xff incase _pBuf is signed */ \
    _val <<= 8; \
    _val |= *(_pBuf + 1) & 0xff;\
}

#define INT32_TO_64(_pBuf, _val) \
{ \
    _val = *(_pBuf + 0) & 0xff; 	/* 0xff incase _pBuf is signed */ \
    _val <<= 8; \
    _val |= *(_pBuf + 1) & 0xff;\
    _val <<= 8; \
    _val |= *(_pBuf + 2) & 0xff;\
    _val <<= 8; \
    _val |= *(_pBuf + 3) & 0xff;\
}

#define INT16_TO_64(_pBuf, _val) \
{ \
    _val = *(_pBuf + 0) & 0xff; 	/* 0xff incase _pBuf is signed */ \
    _val <<= 8; \
    _val |= *(_pBuf + 1) & 0xff;\
}

#endif /* WORD64 */



/* -------------------------------------------------------------------------
 * DEC/IEEE float conversions
 * ------------------------------------------------------------------------- */

/* 
 * Copyright 1988-1991
 * Center for Information Technology Integration (CITI)
 * Information Technology Division
 * University of Michigan
 * Ann Arbor, Michigan
 *
 *                         All Rights Reserved
 * 
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that the above copyright notice appear in all
 * copies and that both that copyright notice and this permission
 * notice appear in supporting documentation, and that the names of
 * CITI or THE UNIVERSITY OF MICHIGAN not be used in advertising or
 * publicity pertaining to distribution of the software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS." CITI AND THE UNIVERSITY OF
 * MICHIGAN DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
 * NO EVENT SHALL CITI OR THE UNIVERSITY OF MICHIGAN BE LIABLE FOR ANY
 * SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 */

#define BITMASK(n) ((((unsigned long)1)<<n)-1)
#define VAX_EXPONENT_BIAS	0x00000081
#define IEEE_EXPONENT_BIAS	0x0000007f

#define VAX_TO_IEEE_BIAS ((CARD32)-VAX_EXPONENT_BIAS + IEEE_EXPONENT_BIAS)
#define IEEE_TO_VAX_BIAS ((CARD32)-IEEE_EXPONENT_BIAS + VAX_EXPONENT_BIAS)

#define MAX_VAX_NEGATIVE 0xffffffff
#define MIN_VAX_NEGATIVE 0x00008000
#define MAX_VAX_POSITIVE 0xffff7fff
#define MIN_VAX_POSITIVE 0x00000000
#define VAX_SIGN_MASK 0xffff7fff

#define MIN_IEEE_NEGATIVE 0x80000000
#define MAX_IEEE_NEGATIVE 0xff800000
#define MIN_IEEE_POSITIVE 0x00000000
#define MAX_IEEE_POSITIVE 0x7f800000
#define IEEE_SIGN_MASK 0x7fffffff



/* -------------------------------------------------------------------------
 * Cray float conversions
 * ------------------------------------------------------------------------- */

#ifdef CRAY

struct	ieee_single {
	unsigned int	zero	: 32;	/* Upper 32 bits are junk */
	unsigned int	sign	: 1;
	unsigned int	exp	: 8;
	unsigned int	mantissa: 23;	/* 24-bit mantissa with 1 hidden bit */
};

/* Cray floating point, partitioned for easy conversion to IEEE single */

struct	cray_single {
	unsigned int	sign	 : 1;
	unsigned int	exp	 : 15;
	unsigned int	mantissa : 24;
	unsigned int	mantissa2: 24;
};

struct	cray_double {
	unsigned int	sign	 : 1;
	unsigned int	exp	 : 15;
	unsigned int	mantissa : 48;
};

#define SET_MIN_SNG_IEEE(_ieee) \
    _ieee.zero = 0; \
    _ieee.sign = 0; \
    _ieee.exp = 0x00; \
    _ieee.mantissa = 0;

#define SET_MAX_SNG_IEEE(_ieee) \
    _ieee.zero = 0; \
    _ieee.sign = 0; \
    _ieee.exp = 0xff; \
    _ieee.mantissa = 0;

#define SET_MAX_SNG_CRAY(_cray) \
    _cray.sign = 0; \
    _cray.exp = 0x6000; \
    _cray.mantissa = 0; \
    _cray.mantissa2 = 0;

#define SET_MAX_DBL_CRAY(_cray) \
    _cray.sign = 0; \
    _cray.exp = 0x6000; \
    _cray.mantissa = 0;


/* Cray exponent limits for conversion to IEEE single */

#define	MAX_CRAY_SNG	(0x100 + CRAY_BIAS - IEEE_SNG_BIAS)
#define	MIN_CRAY_SNG	(0x00 + CRAY_BIAS - IEEE_SNG_BIAS)

#define	CRAY_BIAS	040001
#define IEEE_SNG_BIAS   0x7f

#endif /* CRAY */
