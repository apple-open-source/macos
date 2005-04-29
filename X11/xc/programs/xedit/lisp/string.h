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

/* $XFree86: xc/programs/xedit/lisp/string.h,v 1.12 2003/04/27 18:17:34 tsi Exp $ */

#ifndef Lisp_string_h
#define Lisp_string_h

#include "lisp/internal.h"

LispObj *Lisp_AlphaCharP(LispBuiltin*);
LispObj *Lisp_BothCaseP(LispBuiltin*);
LispObj *Lisp_Char(LispBuiltin*);
LispObj *Lisp_CharLess(LispBuiltin*);
LispObj *Lisp_CharLessEqual(LispBuiltin*);
LispObj *Lisp_CharEqual_(LispBuiltin*);
LispObj *Lisp_CharGreater(LispBuiltin*);
LispObj *Lisp_CharGreaterEqual(LispBuiltin*);
LispObj *Lisp_CharNotEqual_(LispBuiltin*);
LispObj *Lisp_CharLessp(LispBuiltin*);
LispObj *Lisp_CharNotGreaterp(LispBuiltin*);
LispObj *Lisp_CharEqual(LispBuiltin*);
LispObj *Lisp_CharGreaterp(LispBuiltin*);
LispObj *Lisp_CharNotLessp(LispBuiltin*);
LispObj *Lisp_CharNotEqual(LispBuiltin*);
LispObj *Lisp_Character(LispBuiltin*);
LispObj *Lisp_Characterp(LispBuiltin*);
LispObj *Lisp_CharDowncase(LispBuiltin*);
LispObj *Lisp_CharInt(LispBuiltin*);
LispObj *Lisp_CharUpcase(LispBuiltin*);
LispObj *Lisp_DigitChar(LispBuiltin*);
LispObj *Lisp_DigitCharP(LispBuiltin*);
LispObj *Lisp_IntChar(LispBuiltin*);
LispObj *Lisp_GraphicCharP(LispBuiltin*);
LispObj *Lisp_LowerCaseP(LispBuiltin*);
LispObj *Lisp_MakeString(LispBuiltin*);
LispObj *Lisp_ParseInteger(LispBuiltin*);
LispObj *Lisp_ReadFromString(LispBuiltin*);
LispObj *Lisp_String(LispBuiltin*);
LispObj *Lisp_Stringp(LispBuiltin*);
LispObj *Lisp_StringTrim(LispBuiltin*);
LispObj *Lisp_StringLeftTrim(LispBuiltin*);
LispObj *Lisp_StringRightTrim(LispBuiltin*);
LispObj *Lisp_NstringTrim(LispBuiltin*);
LispObj *Lisp_NstringLeftTrim(LispBuiltin*);
LispObj *Lisp_NstringRightTrim(LispBuiltin*);
LispObj *Lisp_StringEqual_(LispBuiltin*);
LispObj *Lisp_StringLess(LispBuiltin*);
LispObj *Lisp_StringGreater(LispBuiltin*);
LispObj *Lisp_StringLessEqual(LispBuiltin*);
LispObj *Lisp_StringGreaterEqual(LispBuiltin*);
LispObj *Lisp_StringNotEqual_(LispBuiltin*);
LispObj *Lisp_StringEqual(LispBuiltin*);
LispObj *Lisp_StringGreaterp(LispBuiltin*);
LispObj *Lisp_StringLessp(LispBuiltin*);
LispObj *Lisp_StringNotLessp(LispBuiltin*);
LispObj *Lisp_StringNotGreaterp(LispBuiltin*);
LispObj *Lisp_StringNotEqual(LispBuiltin*);
LispObj *Lisp_NstringUpcase(LispBuiltin*);
LispObj *Lisp_StringUpcase(LispBuiltin*);
LispObj *Lisp_StringDowncase(LispBuiltin*);
LispObj *Lisp_NstringDowncase(LispBuiltin*);
LispObj *Lisp_StringCapitalize(LispBuiltin*);
LispObj *Lisp_NstringCapitalize(LispBuiltin*);
LispObj *Lisp_StringConcat(LispBuiltin*);
LispObj *Lisp_UpperCaseP(LispBuiltin*);
LispObj *Lisp_XeditCharStore(LispBuiltin*);

#endif /* Lisp_string_h */
