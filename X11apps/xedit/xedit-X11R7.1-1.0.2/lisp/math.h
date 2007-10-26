/*
 * Copyright (c) 2001 by The XFree86 Project, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *  
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE XFREE86 PROJECT BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF
 * OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Except as contained in this notice, the name of the XFree86 Project shall
 * not be used in advertising or otherwise to promote the sale, use or other
 * dealings in this Software without prior written authorization from the
 * XFree86 Project.
 *
 * Author: Paulo César Pereira de Andrade
 */

/* $XFree86: xc/programs/xedit/lisp/math.h,v 1.6tsi Exp $ */

#ifndef Lisp_math_h
#define Lisp_math_h

#include "lisp/internal.h"
#include "mp.h"

void LispMathInit(void);
LispObj *LispFloatCoerce(LispBuiltin*, LispObj*);

LispObj *Lisp_Mul(LispBuiltin*);
LispObj *Lisp_Plus(LispBuiltin*);
LispObj *Lisp_Minus(LispBuiltin*);
LispObj *Lisp_Div(LispBuiltin*);
LispObj *Lisp_OnePlus(LispBuiltin*);
LispObj *Lisp_OneMinus(LispBuiltin*);
LispObj *Lisp_Less(LispBuiltin*);
LispObj *Lisp_LessEqual(LispBuiltin*);
LispObj *Lisp_Equal_(LispBuiltin*);
LispObj *Lisp_Greater(LispBuiltin*);
LispObj *Lisp_GreaterEqual(LispBuiltin*);
LispObj *Lisp_NotEqual(LispBuiltin*);
LispObj *Lisp_Max(LispBuiltin*);
LispObj *Lisp_Min(LispBuiltin*);
LispObj *Lisp_Mod(LispBuiltin*);
LispObj *Lisp_Abs(LispBuiltin*);
LispObj *Lisp_Complex(LispBuiltin*);
LispObj *Lisp_Complexp(LispBuiltin*);
LispObj *Lisp_Conjugate(LispBuiltin*);
LispObj *Lisp_Decf(LispBuiltin*);
LispObj *Lisp_Denominator(LispBuiltin*);
LispObj *Lisp_Evenp(LispBuiltin*);
LispObj *Lisp_Float(LispBuiltin*);
LispObj *Lisp_Floatp(LispBuiltin*);
LispObj *Lisp_Gcd(LispBuiltin*);
LispObj *Lisp_Imagpart(LispBuiltin*);
LispObj *Lisp_Incf(LispBuiltin*);
LispObj *Lisp_Integerp(LispBuiltin*);
LispObj *Lisp_Isqrt(LispBuiltin*);
LispObj *Lisp_Lcm(LispBuiltin*);
LispObj *Lisp_Logand(LispBuiltin*);
LispObj *Lisp_Logeqv(LispBuiltin*);
LispObj *Lisp_Logior(LispBuiltin*);
LispObj *Lisp_Lognot(LispBuiltin*);
LispObj *Lisp_Logxor(LispBuiltin*);
LispObj *Lisp_Minusp(LispBuiltin*);
LispObj *Lisp_Numberp(LispBuiltin*);
LispObj *Lisp_Numerator(LispBuiltin*);
LispObj *Lisp_Oddp(LispBuiltin*);
LispObj *Lisp_Plusp(LispBuiltin*);
LispObj *Lisp_Rational(LispBuiltin*);
#if 0
LispObj *Lisp_Rationalize(LispBuiltin*);
#endif
LispObj *Lisp_Rationalp(LispBuiltin*);
LispObj *Lisp_Realpart(LispBuiltin*);
LispObj *Lisp_Rem(LispBuiltin*);
LispObj *Lisp_Sqrt(LispBuiltin*);
LispObj *Lisp_Zerop(LispBuiltin*);
LispObj *Lisp_Ceiling(LispBuiltin*);
LispObj *Lisp_Fceiling(LispBuiltin*);
LispObj *Lisp_Floor(LispBuiltin*);
LispObj *Lisp_Ffloor(LispBuiltin*);
LispObj *Lisp_Round(LispBuiltin*);
LispObj *Lisp_Fround(LispBuiltin*);
LispObj *Lisp_Truncate(LispBuiltin*);
LispObj *Lisp_Ftruncate(LispBuiltin*);


#endif /* Lisp_math_h */
