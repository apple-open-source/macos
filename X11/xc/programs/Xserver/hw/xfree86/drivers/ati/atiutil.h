/* $XFree86: xc/programs/Xserver/hw/xfree86/drivers/ati/atiutil.h,v 1.9 2004/01/05 16:42:05 tsi Exp $ */
/*
 * Copyright 1997 through 2004 by Marc Aurele La France (TSI @ UQV), tsi@xfree86.org
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of Marc Aurele La France not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Marc Aurele La France makes no representations
 * about the suitability of this software for any purpose.  It is provided
 * "as-is" without express or implied warranty.
 *
 * MARC AURELE LA FRANCE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO
 * EVENT SHALL MARC AURELE LA FRANCE BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef ___ATIUTIL_H___
#define ___ATIUTIL_H___ 1

#include "atiproto.h"

/*
 * Prevent the C standard's insistence on unsigned long sizeof's from causing
 * counter-intuitive results.
 */
#define SizeOf(_object) ((int)sizeof(_object))
#define NumberOf(_what) (SizeOf(_what) / SizeOf(_what[0]))

#define __ONE_MICROSECOND__ 100         /* This'll need calibration */

#define ATIDelay(_microseconds)                            \
    {                                                      \
        unsigned int _i, _j;                               \
        for (_i = 0;  _i < _microseconds;  _i++)           \
            for (_j = 0;  _j < __ONE_MICROSECOND__;  _j++) \
                /* Nothing */;                             \
    }

/*
 * Macros to get/set a contiguous bit field.  Arguments should not be
 * self-modifying.
 */
#define UnitOf(___Value)                                \
        (((((___Value) ^ ((___Value) - 1)) + 1) >> 1) | \
         ((((___Value) ^ ((___Value) - 1)) >> 1) + 1))

#define GetBits(__Value, _Mask) (((__Value) & (_Mask)) / UnitOf(_Mask))
#define SetBits(__Value, _Mask) (((__Value) * UnitOf(_Mask)) & (_Mask))

#define MaxBits(__Mask)         GetBits(__Mask, __Mask)

#define _ByteMask(__Byte)       ((CARD8)(-1) << (8 * (__Byte)))
#define GetByte(_Value, _Byte)  GetBits(_Value, _ByteMask(_Byte))
#define SetByte(_Value, _Byte)  SetBits(_Value, _ByteMask(_Byte))

#define _WordMask(__Word)       ((CARD16)(-1) << (16 * (__Word)))
#define GetWord(_Value, _Word)  GetBits(_Value, _WordMask(_Word))
#define SetWord(_Value, _Word)  SetBits(_Value, _WordMask(_Word))

extern void ATIReduceRatio FunctionPrototype((int *, int *));
extern int  ATIDivide      FunctionPrototype((int, int, int, const int));

#endif /* ___ATIUTIL_H___ */
