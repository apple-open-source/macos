/* $Xorg: miLevel1.c,v 1.4 2001/02/09 02:04:09 xorgcvs Exp $ */
/*

Copyright 1990, 1991, 1998  The Open Group

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


Copyright 1990, 1991 by Sun Microsystems, Inc.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Sun Microsystems,
not be used in advertising or publicity pertaining to distribution
of the software without specific, written prior permission.

SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT
SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

*/

#include "ddpex.h"

ddpex1rtn
PexTriangle(pRend, pPt1, pPt2, pPt3, pColour1, pColour2, pColour3)
/* in */
	ddRendererPtr		 pRend;
	ddDeviceCoord		*pPt1;
	ddDeviceCoord		*pPt2;
	ddDeviceCoord		*pPt3;
	ddColourSpecifier	*pColour1;
	ddColourSpecifier	*pColour2;
	ddColourSpecifier	*pColour3;
/* out */
{
	return(Success);
}

ddpex1rtn
PexTrapezoid(pRend, pPt1, pPt2, pPt3, pPt4, pColour1, pColour2, pColour3, pColour4)
/* in */
	ddRendererPtr		 pRend;
	ddDeviceCoord		*pPt1;
	ddDeviceCoord		*pPt2;
	ddDeviceCoord		*pPt3;
	ddDeviceCoord		*pPt4;
	ddColourSpecifier	*pColour1;
	ddColourSpecifier	*pColour2;
	ddColourSpecifier	*pColour3;
	ddColourSpecifier	*pColour4;
/* out */
{
	return(Success);
}

ddpex1rtn
PexInterpSpan1(pRend, pPt1, pPt2, startColour, numSteps)
/* in */
	ddRendererPtr		 pRend;
	ddDeviceCoord		*pPt1;
	ddDeviceCoord		*pPt2;
	ddTableIndex		 startColour;
	ddUSHORT		 numSteps;
/* out */
{
	return(Success);
}

ddpex1rtn
PexInterpSpan2(pRend, pPt1, pPt2, startColour, endColour)
/* in */
	ddRendererPtr		 pRend;
	ddDeviceCoord		*pPt1;
	ddDeviceCoord		*pPt2;
	ddTableIndex		 startColour;
	ddTableIndex		 endColour;
/* out */
{
	return(Success);
}

typedef	struct {
	ddULONG		width;
	ddULONG		height;
	ddULONG		depth;
	ddULONG		numBytes;
	ddPointer	pBytes;
} ddGlyphStr;

ddpex1rtn
PexGlyph(pRend, pUpperLeft, pForeground, pBackground, pGlyph)
/* in */
	ddRendererPtr		 pRend;
	ddDeviceCoord		*pUpperLeft;
	ddColourSpecifier	*pForeground;
	ddColourSpecifier	*pBackground;
	ddGlyphStr		*pGlyph;
/* out */
{
	return(Success);
}

