/* $Xorg: ddpex4.h,v 1.4 2001/02/09 02:04:18 xorgcvs Exp $ */

/***********************************************************

Copyright 1989, 1990, 1991, 1998  The Open Group

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

Copyright 1989, 1990, 1991 by Sun Microsystems, Inc.

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

#include "ddpex.h"

#ifndef DDPEX4_H
#define DDPEX4_H

/*  structures for ddPEX Level 4 Workstation Support */

typedef struct {
	DrawablePtr     pDrawable;
	Drawable		drawableId;
	diLUTHandle     pMarkerLUT;        /* handles to LUTs and NS */
	diLUTHandle     pTextLUT;
	diLUTHandle     pLineLUT;
	diLUTHandle     pIntLUT;
	diLUTHandle     pEdgeLUT;
	diLUTHandle     pColourLUT;
	diLUTHandle     pDepthCueLUT;
	diLUTHandle     pLightLUT;
	diLUTHandle     pColourAppLUT;
	diLUTHandle     pPatternLUT;
	diLUTHandle     pFontLUT;
	diNSHandle      pHighInclSet;
	diNSHandle      pHighExclSet;
	diNSHandle      pInvisInclSet;
	diNSHandle      pInvisExclSet;
	ddEnumTypeIndex	bufferMode;
} ddWksInit;

typedef struct {
	ddBYTE	viewRep;
	ddBYTE	markerBundle;
	ddBYTE	textBundle;
	ddBYTE	lineBundle;
	ddBYTE	interiorBundle;
	ddBYTE	edgeBundle;
	ddBYTE	colourTable;
	ddBYTE	patternTable;
	ddBYTE	wksTransform;
	ddBYTE	highlightFilter;
	ddBYTE	invisFilter;
	ddBYTE	hlhsrMode;
	ddBYTE	strModify;
	ddBYTE	postStr;
	ddBYTE	unpostStr;
	ddBYTE	deleteStr;
	ddBYTE	refModify;
	ddBYTE	bufferModify;
	ddBYTE	lightTable;
	ddBYTE	depthCueTable;
	ddBYTE	colourApproxTable;
} ddWksDynamics;

typedef struct {
	diNSHandle	incl;
	diNSHandle	excl;
} ddNSPair;

typedef struct {
	ddULONG		numPairs;
	ddULONG		maxPairs;
	ddNSPair 	*pPairs;
} listofNSPair;

typedef struct {
	ddULONG		    id;
	ddUSHORT            status;
	ddCoord3D	    position;		/* search reference position */
	ddFLOAT		    distance;		/* search distance */	
	ddUSHORT	    ceiling;		/* search ceiling */
	ddBOOL		    modelClipFlag;	/* model clip flag */
	listofObj	    *startPath;		/* start path */
	listofNSPair	    normal;
	listofNSPair	    inverted;
} ddSCStr;

typedef struct {
	ddTableIndex	index;
	ddUSHORT	unused;
	ddViewEntry	view;
} ddViewRep;

typedef struct {
	ddUSHORT	whence;
	ddUSHORT	unused;
	ddLONG		offset;
} ddElementPos;

typedef struct {
	ddElementPos	position1;
	ddElementPos	position2;
} ddElementRange;

typedef struct {
        ddSHORT		x;
        ddSHORT		y;
} ddDeviceCoord2D;

typedef struct {
	ddDeviceCoord2D	position;
	ddFLOAT         distance;
} ddPC_DC_HitBox;

typedef  ddNpcSubvolume ddPC_NPC_HitVolume;

#endif  /* DDPEX4_H  */
