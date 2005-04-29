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

/* $XFree86: xc/programs/xedit/lisp/helper.h,v 1.16 2003/04/27 18:17:33 tsi Exp $ */

#ifndef Lisp_helper_h
#define Lisp_helper_h

#include "lisp/private.h"

/*
 * Prototypes
 */
void LispCheckSequenceStartEnd(LispBuiltin*, LispObj*,
			       LispObj*, LispObj*, long*, long*, long*);
long LispLength(LispObj*);
LispObj *LispCharacterCoerce(LispBuiltin*, LispObj*);
LispObj *LispStringCoerce(LispBuiltin*, LispObj*);
LispObj *LispCoerce(LispBuiltin*, LispObj*, LispObj*);

/*
 do init test &rest body
 do* init test &rest body
 */
LispObj *LispDo(LispBuiltin*, int);

/*
 dolist init &rest body
 dotimes init &rest body
 */
LispObj *LispDoListTimes(LispBuiltin*, int);

#define FEQ	1
#define FEQL	2
#define FEQUAL	3
#define FEQUALP	4
LispObj *LispObjectCompare(LispObj*, LispObj*, int);
#define XEQ(x, y)	LispObjectCompare(x, y, FEQ)
#define XEQL(x, y)	LispObjectCompare(x, y, FEQL)
#define XEQUAL(x, y)	LispObjectCompare(x, y, FEQUAL)
#define XEQUALP(x, y)	LispObjectCompare(x, y, FEQUALP)

LispObj *LispLoadFile(LispObj*, int, int, int);

/*
 string= string1 string2 &key start1 end1 start2 end2
 string< string1 string2 &key start1 end1 start2 end2
 string> string1 string2 &key start1 end1 start2 end2
 string<= string1 string2 &key start1 end1 start2 end2
 string>= string1 string2 &key start1 end1 start2 end2
 string/= string1 string2 &key start1 end1 start2 end2
 string-equal string1 string2 &key start1 end1 start2 end2
 string-lessp string1 string2 &key start1 end1 start2 end2
 string-greaterp string1 string2 &key start1 end1 start2 end2
 string-not-lessp string1 string2 &key start1 end1 start2 end2
 string-not-greaterp string1 string2 &key start1 end1 start2 end2
 string-not-equal string1 string2 &key start1 end1 start2 end2
*/
void LispGetStringArgs(LispBuiltin*,
			char**,	/* string1 */
			char**,	/* string2 */
			long*,	/* start1 */
			long*,	/* end1 */
			long*,	/* start2 */
			long*);	/* end2 */

/*
 pathname-host pathname
 pathname-device pathname
 pathname-directory pathname
 pathname-name pathname
 pathname-type pathname
 pathname-version pathname
 */
LispObj *LispPathnameField(int, int);

/*
 truename pathname
 probe-file pathname
 */
LispObj *LispProbeFile(LispBuiltin*, int);

/*
 write-string string &optional output-stream &key start end
 write-line string &optional output-stream &key start end
 */
LispObj *LispWriteString_(LispBuiltin*, int);

#endif	/* Lisp_helper_h */
