/* $Xorg: miLineDash.h,v 1.4 2001/02/09 02:04:08 xorgcvs Exp $ */

/*****************************************************************

Copyright 1989,1990, 1991, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

Copyright 1989,1990, 1991 by Sun Microsystems, Inc.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the names of Sun Microsystems,
and The Open Group, not be used in advertising or publicity 
pertaining to distribution of the software without specific, written 
prior permission.  

SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, 
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT 
SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL 
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

#ifndef MI_LINEDASH_H
#define MI_LINEDASH_H

/*
 * Define the pre-defined PEX line dash types for ddx
 *
 * PEX defines four line dash styles:
 *
 *	PEXLineTypeSolid 
 *		clearly no pattern needed here!
 *	PEXLineTypeDashed
 *		defined for this implementation as 7 pixels on, 7 off.....
 *	PEXLineTypeDotted
 *		defined for this implementation as 1 pixels on, 5 off.....
 *	PEXLineTypeDashDot
 *		defined for this implementation as 
 *		9 pixels on, 3 off, 1 on, 3 off....
 */
#define MAX_LINE_DASH_LENGTH_SIZE 4*sizeof(unsigned char)
static int		mi_line_dashed_length = 2;
static unsigned char	mi_line_dashed[] = {7,7};
static int		mi_line_dotted_length = 2;
static unsigned char	mi_line_dotted[] = {1,5};
static int		mi_line_dashdot_length = 4;
static unsigned char	mi_line_dashdot[] = {7,3,1,3};

#endif	/* MI_LINEDASH_H */
