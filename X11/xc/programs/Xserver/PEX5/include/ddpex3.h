/* $Xorg: ddpex3.h,v 1.4 2001/02/09 02:04:18 xorgcvs Exp $ */

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
supporting documentation, and that the name of Sun Microsystems,
not be used in advertising or publicity pertaining to distribution of 
the software without specific, written prior permission.  

SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, 
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT 
SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL 
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

#include "ddpex.h"

#ifndef DDPEX3_H
#define DDPEX3_H

/* just a reminder of what colours there are
        IndexedColour
        Rgb8Colour
        Rgb16Colour
        RgbFloatColour
        HsvColour
        HlsColour
        CieColour
*/

/* First, some basic point type definitions */
/* #define		DDPT_FLOAT		(0<<0) */
#define		DDPT_SHORT		(1<<0)
#define		DDPT_2D			(1<<1)
#define		DDPT_3D			(2<<1)
#define		DDPT_4D			(3<<1)
#define		DDPT_NORMAL		(1<<3)
#define		DDPT_EDGE		(1<<4)
#define		DDPT_COLOUR		(7<<5)
#define		DDPT_INDEXEDCOLOUR	(1<<5)
#define		DDPT_RGB8COLOUR 	(2<<5)
#define		DDPT_RGB16COLOUR 	(3<<5)
#define		DDPT_RGBFLOATCOLOUR 	(4<<5)
#define		DDPT_HSVCOLOUR 		(5<<5)
#define		DDPT_HLSCOLOUR 		(6<<5)
#define		DDPT_CIECOLOUR 		(7<<5)

/* 
 * Now, some access macros 
 * It is strongly recommended that these macros be used instead
 * of accessing the fields directly.
 */
#define	DD_IsVertFloat(type)	(!((type) & DDPT_SHORT))
#define DD_IsVertShort(type)	((type) & DDPT_SHORT)
#define DD_IsVert2D(type)	(((type) & DDPT_4D) == DDPT_2D)
#define DD_IsVert3D(type)	(((type) & DDPT_4D) == DDPT_3D)
#define DD_IsVert4D(type)	(((type) & DDPT_4D) == DDPT_4D)
#define DD_IsVertNormal(type)	((type) & DDPT_NORMAL)
#define DD_IsVertEdge(type)	((type) & DDPT_EDGE)
#define DD_IsVertColour(type)	((type) & DDPT_COLOUR)
#define DD_IsVertIndexed(type)	(((type) & DDPT_COLOUR) == DDPT_INDEXEDCOLOUR)
#define DD_IsVertRGB8(type)	(((type) & DDPT_COLOUR) == DDPT_RGB8COLOUR)
#define DD_IsVertRGB16(type)	(((type) & DDPT_COLOUR) == DDPT_RGB16COLOUR)
#define DD_IsVertRGBFLOAT(type)	(((type)&DDPT_COLOUR) == DDPT_RGBFLOATCOLOUR)
#define DD_IsVertHSV(type)	(((type) & DDPT_COLOUR) == DDPT_HSVCOLOUR)
#define DD_IsVertHLS(type)	(((type) & DDPT_COLOUR) == DDPT_HLSCOLOUR)
#define DD_IsVertCIE(type)	(((type) & DDPT_COLOUR) == DDPT_CIECOLOUR)

#define DD_IsVertCoordsOnly(type) 					\
	!((type & DDPT_COLOUR) || (type & DDPT_EDGE) || (type & DDPT_NORMAL)) 

/*
 * These macros are used to change a vertex type
 */
#define	DD_SetVertFloat(type)	((type) &= ~DDPT_SHORT)
#define DD_SetVertShort(type)	((type) |= DDPT_SHORT)
#define DD_SetVert2D(type)	((type) = (((type) & ~DDPT_4D) | DDPT_2D))
#define DD_SetVert3D(type)	((type) = (((type) & ~DDPT_4D) | DDPT_3D))
#define DD_SetVert4D(type)	((type) = (((type) & ~DDPT_4D) | DDPT_4D))
#define DD_SetVertNormal(type)	((type) |= DDPT_NORMAL)
#define DD_SetVertEdge(type)	((type) |= DDPT_EDGE)
#define DD_SetVertIndexed(type)	((type)=(((type) & ~DDPT_COLOUR) | DDPT_INDEXEDCOLOUR))
#define DD_SetVertRGB8(type)	((type)=(((type) & ~DDPT_COLOUR) | DDPT_RGB8COLOUR))
#define DD_SetVertRGB16(type)	((type)=(((type) & ~DDPT_COLOUR) | DDPT_RGB16COLOUR))
#define DD_SetVertRGBFLOAT(type) ((type)=(((type) & ~DDPT_COLOUR) | DDPT_RGBFLOATCOLOUR))
#define DD_SetVertHSV(type)	((type)=(((type) & ~DDPT_COLOUR) | DDPT_HSVCOLOUR))
#define DD_SetVertHLS(type)	((type)=(((type) & ~DDPT_COLOUR) | DDPT_HLSCOLOUR))
#define DD_SetVertCIE(type)	((type)=(((type) & ~DDPT_COLOUR) | DDPT_CIECOLOUR))

#define	DD_UnSetVertFloat(type)		((type) |= DDPT_SHORT)
#define DD_UnSetVertShort(type)		((type) &= ~DDPT_SHORT)
#define DD_UnSetVert2D(type)		((type) &= ~DDPT_4D)
#define DD_UnSetVert3D(type)		((type) &= ~DDPT_4D)
#define DD_UnSetVert4D(type)		((type) &= ~DDPT_4D)
#define DD_UnSetVertCoord(type)		((type) &= ~(DDPT_4D | DDPT_SHORT))
#define DD_UnSetVertNormal(type) 	((type) &= ~DDPT_NORMAL)
#define DD_UnSetVertEdge(type)		((type) &= ~DDPT_EDGE)
#define DD_UnSetColour(type)		((type) &= ~DDPT_COLOUR)
#define DD_UnSetVertIndexed(type) 	((type) &= ~DDPT_COLOUR)
#define DD_UnSetVertRGB8(type)		((type) &= ~DDPT_COLOUR)
#define DD_UnSetVertRGB16(type)		((type) &= ~DDPT_COLOUR)
#define DD_UnSetVertRGBFLOAT(type) 	((type) &= ~DDPT_COLOUR)
#define DD_UnSetVertHSV(type)		((type) &= ~DDPT_COLOUR)
#define DD_UnSetVertHLS(type)		((type) &= ~DDPT_COLOUR)
#define DD_UnSetVertCIE(type)		((type) &= ~DDPT_COLOUR)


/*
 * A macro to compute the point size - very usefull when
 * walking through a list of points and one isn't concerned
 * about the actual details of the point data
 */
#define DD_VertPointSize(type, size)					\
       {								\
	if (DD_IsVertFloat(type)) {					\
	  if (DD_IsVert2D(type)) size = sizeof(ddCoord2D);		\
	  else if (DD_IsVert3D(type)) size = sizeof(ddCoord3D);		\
	  else size = sizeof(ddCoord4D);				\
	} else {							\
	  if (DD_IsVert2D(type)) size = sizeof(ddCoord2DS);		\
	  else size = sizeof(ddCoord3DS);				\
	} 								\
	if (DD_IsVertNormal(type)) size += sizeof(ddVector3D);		\
	if (DD_IsVertColour(type)) {					\
	  if (DD_IsVertIndexed(type)) size += sizeof(ddIndexedColour);	\
	  else if (DD_IsVertRGB8(type)) size += sizeof(ddRgb8Colour);	\
	  else if (DD_IsVertRGB16(type)) size += sizeof(ddRgb16Colour);	\
	  else size += sizeof(ddRgbFloatColour);			\
	} 								\
	if (DD_IsVertEdge(type)) size += sizeof(ddULONG);		\
       }
/*
 * The following macros find offets from the start
 * of a structure in bytes to the desired vertex component.
 * again, very usefull macros for routines that wish
 * to access individual vertex components without necessarily
 * knowing all the details of the vertex. Offset is set to -1 
 * if the component does not exist in the vertex.
 */

/* 
 * Note that the color is always the first component following
 * the vertex data.
 */
#define DD_VertOffsetColor(type, offset)				\
	{								\
	 if (!DD_IsVertColour((type))) (offset) = -1;			\
	 else 								\
	   DD_VertPointSize(((type) & (DDPT_4D | DDPT_SHORT)),(offset));\
	}

/*
 * Note that the edge flag is always the last component in
 * a vertex. Thus the offset is the size of the point minus
 * the size of the edge field (a ddULONG).
 */
#define DD_VertOffsetEdge(type, offset)					\
	{								\
	 if (!DD_IsVertEdge((type))) (offset) = -1;			\
	 else { 							\
	   DD_VertPointSize((type), (offset));				\
	   (offset) -= sizeof(ddULONG);					\
	 }								\
	}

/*
 * This one is the most complex as the normal is last unless
 * there is an edge in which case it preceeds the edge flag.
 */
#define DD_VertOffsetNormal(type, offset)				\
	{								\
	 if (!DD_IsVertNormal((type))) (offset) = -1;			\
	 else { 							\
	   DD_VertPointSize((type), (offset));				\
	   if (DD_IsVertEdge((type))) (offset) -= sizeof(ddULONG);	\
	   (offset) -= sizeof(ddVector3D);				\
	 }								\
	}

/* Now create the point types */
#define  DD_2DS_POINT   		(DDPT_SHORT | DDPT_2D)
#define  DD_2D_POINT    		(DDPT_2D)
#define  DD_3DS_POINT   		(DDPT_SHORT | DDPT_3D)
#define  DD_3D_POINT    		(DDPT_3D)
#define  DD_INDEX_POINT  		(DDPT_3D | DDPT_INDEXEDCOLOUR)
#define  DD_INDEX_POINT4D  		(DDPT_4D | DDPT_INDEXEDCOLOUR)
#define  DD_RGB8_POINT  		(DDPT_3D | DDPT_RGB8COLOUR)
#define  DD_RGB8_POINT4D  		(DDPT_4D | DDPT_RGB8COLOUR)
#define  DD_RGB16_POINT  		(DDPT_3D | DDPT_RGB16COLOUR)
#define  DD_RGB16_POINT4D  		(DDPT_4D | DDPT_RGB16COLOUR)
#define  DD_RGBFLOAT_POINT2DS  		(DDPT_SHORT | DDPT_2D | DDPT_RGBFLOATCOLOUR)
#define  DD_RGBFLOAT_POINT  		(DDPT_3D | DDPT_RGBFLOATCOLOUR)
#define  DD_RGBFLOAT_POINT4D  		(DDPT_4D | DDPT_RGBFLOATCOLOUR)
#define  DD_HSV_POINT2DS  		(DDPT_SHORT | DDPT_2D | DDPT_HSVCOLOUR)
#define  DD_HSV_POINT  			(DDPT_3D | DDPT_HSVCOLOUR)
#define  DD_HSV_POINT4D  		(DDPT_4D | DDPT_HSVCOLOUR)
#define  DD_HLS_POINT2DS  		(DDPT_SHORT | DDPT_2D | DDPT_HLSCOLOUR)
#define  DD_HLS_POINT  			(DDPT_3D | DDPT_HLSCOLOUR)
#define  DD_HLS_POINT4D  		(DDPT_4D | DDPT_HLSCOLOUR)
#define  DD_CIE_POINT2DS  		(DDPT_SHORT | DDPT_2D | DDPT_CIECOLOUR)
#define  DD_CIE_POINT  			(DDPT_3D | DDPT_CIECOLOUR)
#define  DD_CIE_POINT4D  		(DDPT_4D | DDPT_CIECOLOUR)
#define  DD_NORM_POINT2DS  		(DDPT_SHORT | DDPT_2D | DDPT_NORMAL)
#define  DD_NORM_POINT  		(DDPT_3D | DDPT_NORMAL)
#define  DD_NORM_POINT4D  		(DDPT_4D | DDPT_NORMAL)
#define  DD_EDGE_POINT2DS  		(DDPT_SHORT | DDPT_2D | DDPT_EDGE)
#define  DD_EDGE_POINT  		(DDPT_3D | DDPT_EDGE)
#define  DD_EDGE_POINT4D  		(DDPT_4D | DDPT_EDGE)
#define  DD_INDEX_NORM_POINT  		(DDPT_3D | DDPT_NORMAL | DDPT_INDEXEDCOLOUR)
#define  DD_INDEX_NORM_POINT4D  	(DDPT_4D | DDPT_NORMAL | DDPT_INDEXEDCOLOUR)
#define  DD_RGB8_NORM_POINT  		(DDPT_3D | DDPT_NORMAL | DDPT_RGB8COLOUR)
#define  DD_RGB8_NORM_POINT4D  		(DDPT_4D | DDPT_NORMAL | DDPT_RGB8COLOUR)
#define  DD_RGB16_NORM_POINT   		(DDPT_3D | DDPT_NORMAL | DDPT_RGB16COLOUR)
#define  DD_RGB16_NORM_POINT4D   	(DDPT_4D | DDPT_NORMAL | DDPT_RGB16COLOUR)
#define  DD_RGBFLOAT_NORM_POINT2DS   	(DDPT_SHORT | DDPT_2D | DDPT_NORMAL | DDPT_RGBFLOATCOLOUR)
#define  DD_RGBFLOAT_NORM_POINT   	(DDPT_3D | DDPT_NORMAL | DDPT_RGBFLOATCOLOUR)
#define  DD_RGBFLOAT_NORM_POINT4D   	(DDPT_4D | DDPT_NORMAL | DDPT_RGBFLOATCOLOUR)
#define  DD_HSV_NORM_POINT2DS  		(DDPT_SHORT | DDPT_2D | DDPT_NORMAL | DDPT_HSVCOLOUR)
#define  DD_HSV_NORM_POINT  		(DDPT_3D | DDPT_NORMAL | DDPT_HSVCOLOUR)
#define  DD_HSV_NORM_POINT4D  		(DDPT_4D | DDPT_NORMAL | DDPT_HSVCOLOUR)
#define  DD_HLS_NORM_POINT2DS  		(DDPT_SHORT | DDPT_2D | DDPT_NORMAL | DDPT_HLSCOLOUR)
#define  DD_HLS_NORM_POINT  		(DDPT_3D | DDPT_NORMAL | DDPT_HLSCOLOUR)
#define  DD_HLS_NORM_POINT4D  		(DDPT_4D | DDPT_NORMAL | DDPT_HLSCOLOUR)
#define  DD_CIE_NORM_POINT2DS  		(DDPT_SHORT | DDPT_2D | DDPT_NORMAL | DDPT_CIECOLOUR)
#define  DD_CIE_NORM_POINT  		(DDPT_3D | DDPT_NORMAL | DDPT_CIECOLOUR)
#define  DD_CIE_NORM_POINT4D  		(DDPT_4D | DDPT_NORMAL | DDPT_CIECOLOUR)
#define  DD_INDEX_EDGE_POINT  		(DDPT_3D | DDPT_EDGE | DDPT_INDEXEDCOLOUR)
#define  DD_INDEX_EDGE_POINT4D  	(DDPT_4D | DDPT_EDGE | DDPT_INDEXEDCOLOUR)
#define  DD_RGB8_EDGE_POINT  		(DDPT_3D | DDPT_EDGE | DDPT_RGB8COLOUR)
#define  DD_RGB8_EDGE_POINT4D  		(DDPT_4D | DDPT_EDGE | DDPT_RGB8COLOUR)
#define  DD_RGB16_EDGE_POINT  		(DDPT_3D | DDPT_EDGE | DDPT_RGB16COLOUR)
#define  DD_RGB16_EDGE_POINT4D  	(DDPT_4D | DDPT_EDGE | DDPT_RGB16COLOUR)
#define  DD_RGBFLOAT_EDGE_POINT2DS   	(DDPT_SHORT | DDPT_2D | DDPT_EDGE | DDPT_RGBFLOATCOLOUR)
#define  DD_RGBFLOAT_EDGE_POINT   	(DDPT_3D | DDPT_EDGE | DDPT_RGBFLOATCOLOUR)
#define  DD_RGBFLOAT_EDGE_POINT4D   	(DDPT_4D | DDPT_EDGE | DDPT_RGBFLOATCOLOUR)
#define  DD_HSV_EDGE_POINT2DS  		(DDPT_SHORT | DDPT_2D | DDPT_EDGE | DDPT_HSVCOLOUR)
#define  DD_HSV_EDGE_POINT  		(DDPT_3D | DDPT_EDGE | DDPT_HSVCOLOUR)
#define  DD_HSV_EDGE_POINT4D  		(DDPT_4D | DDPT_EDGE | DDPT_HSVCOLOUR)
#define  DD_HLS_EDGE_POINT2DS  		(DDPT_SHORT | DDPT_2D | DDPT_EDGE | DDPT_HLSCOLOUR)
#define  DD_HLS_EDGE_POINT  		(DDPT_3D | DDPT_EDGE | DDPT_HLSCOLOUR)
#define  DD_HLS_EDGE_POINT4D  		(DDPT_4D | DDPT_EDGE | DDPT_HLSCOLOUR)
#define  DD_CIE_EDGE_POINT2DS  		(DDPT_SHORT | DDPT_2D | DDPT_EDGE | DDPT_CIECOLOUR)
#define  DD_CIE_EDGE_POINT  		(DDPT_3D | DDPT_EDGE | DDPT_CIECOLOUR)
#define  DD_CIE_EDGE_POINT4D  		(DDPT_4D | DDPT_EDGE | DDPT_CIECOLOUR)
#define  DD_NORM_EDGE_POINT2DS  	(DDPT_SHORT | DDPT_2D | DDPT_NORMAL | DDPT_EDGE )
#define  DD_NORM_EDGE_POINT  		(DDPT_3D | DDPT_NORMAL | DDPT_EDGE )
#define  DD_NORM_EDGE_POINT4D  		(DDPT_4D | DDPT_NORMAL | DDPT_EDGE )
#define  DD_INDEX_NORM_EDGE_POINT  	(DDPT_3D | DDPT_NORMAL | DDPT_EDGE | DDPT_INDEXEDCOLOUR)
#define  DD_INDEX_NORM_EDGE_POINT4D  	(DDPT_4D | DDPT_NORMAL | DDPT_EDGE | DDPT_INDEXEDCOLOUR)
#define  DD_RGB8_NORM_EDGE_POINT  	(DDPT_3D | DDPT_NORMAL | DDPT_EDGE | DDPT_RGB8COLOUR)
#define  DD_RGB8_NORM_EDGE_POINT4D  	(DDPT_4D | DDPT_NORMAL | DDPT_EDGE | DDPT_RGB8COLOUR)
#define  DD_RGB16_NORM_EDGE_POINT  	(DDPT_3D | DDPT_NORMAL | DDPT_EDGE | DDPT_RGB16COLOUR)
#define  DD_RGB16_NORM_EDGE_POINT4D  	(DDPT_4D | DDPT_NORMAL | DDPT_EDGE | DDPT_RGB16COLOUR)
#define  DD_RGBFLOAT_NORM_EDGE_POINT2DS  	(DDPT_SHORT | DDPT_2D | DDPT_NORMAL | DDPT_EDGE | DDPT_RGBFLOATCOLOUR)
#define  DD_RGBFLOAT_NORM_EDGE_POINT  	(DDPT_3D | DDPT_NORMAL | DDPT_EDGE | DDPT_RGBFLOATCOLOUR)
#define  DD_RGBFLOAT_NORM_EDGE_POINT4D 	(DDPT_4D | DDPT_NORMAL | DDPT_EDGE | DDPT_RGBFLOATCOLOUR)
#define  DD_HSV_NORM_EDGE_POINT2DS   	(DDPT_SHORT | DDPT_2D | DDPT_NORMAL | DDPT_EDGE | DDPT_HSVCOLOUR)
#define  DD_HSV_NORM_EDGE_POINT   	(DDPT_3D | DDPT_NORMAL | DDPT_EDGE | DDPT_HSVCOLOUR)
#define  DD_HSV_NORM_EDGE_POINT4D   	(DDPT_4D | DDPT_NORMAL | DDPT_EDGE | DDPT_HSVCOLOUR)
#define  DD_HLS_NORM_EDGE_POINT2DS   	(DDPT_SHORT | DDPT_2D | DDPT_NORMAL | DDPT_EDGE | DDPT_HLSCOLOUR)
#define  DD_HLS_NORM_EDGE_POINT   	(DDPT_3D | DDPT_NORMAL | DDPT_EDGE | DDPT_HLSCOLOUR)
#define  DD_HLS_NORM_EDGE_POINT4D   	(DDPT_4D | DDPT_NORMAL | DDPT_EDGE | DDPT_HLSCOLOUR)
#define  DD_CIE_NORM_EDGE_POINT2DS   	(DDPT_SHORT | DDPT_2D | DDPT_NORMAL | DDPT_EDGE | DDPT_CIECOLOUR)
#define  DD_CIE_NORM_EDGE_POINT   	(DDPT_3D | DDPT_NORMAL | DDPT_EDGE | DDPT_CIECOLOUR)
#define  DD_CIE_NORM_EDGE_POINT4D   	(DDPT_4D | DDPT_NORMAL | DDPT_EDGE | DDPT_CIECOLOUR)
#define  DD_HOMOGENOUS_POINT  		(DDPT_4D)

typedef ddUSHORT ddPointType;

/* 
 * The point types correspond to the point types in the PEX protocol 
 */
typedef struct {
	ddCoord3D	pt;
	ddIndexedColour	colour;
} ddIndexPoint;

typedef struct {
	ddCoord3D	pt;
	ddRgb8Colour	colour;
} ddRgb8Point;

typedef struct {
	ddCoord3D	pt;
	ddRgb16Colour	colour;
} ddRgb16Point;

typedef struct {
	ddCoord2DS	pt;
	ddRgbFloatColour	colour;
} ddRgbFloatPoint2DS;

typedef struct {
	ddCoord3D	pt;
	ddRgbFloatColour	colour;
} ddRgbFloatPoint;

typedef struct {
	ddCoord2DS	pt;
	ddHsvColour	colour;
} ddHsvPoint2DS;

typedef struct {
	ddCoord3D	pt;
	ddHsvColour	colour;
} ddHsvPoint;

typedef struct {
	ddCoord2DS	pt;
	ddHlsColour	colour;
} ddHlsPoint2DS;

typedef struct {
	ddCoord3D	pt;
	ddHlsColour	colour;
} ddHlsPoint;

typedef struct {
	ddCoord2DS	pt;
	ddCieColour	colour;
} ddCiePoint2DS;

typedef struct {
	ddCoord3D	pt;
	ddCieColour	colour;
} ddCiePoint;

typedef struct {
	ddCoord2DS	pt;
	ddVector3D	normal;
} ddNormalPoint2DS;

typedef struct {
	ddCoord3D	pt;
	ddVector3D	normal;
} ddNormalPoint;

typedef struct {
	ddCoord2DS	pt;
	ddULONG		edge;
} ddEdgePoint2DS;

typedef struct {
	ddCoord3D	pt;
	ddULONG		edge;
} ddEdgePoint;

typedef struct {
	ddCoord3D	pt;
	ddIndexedColour	colour;
	ddVector3D	normal;
} ddIndexNormalPoint;

typedef struct {
	ddCoord3D	pt;
	ddRgb8Colour	colour;
	ddVector3D	normal;
} ddRgb8NormalPoint;

typedef struct {
	ddCoord3D	pt;
	ddRgb16Colour	colour;
	ddVector3D	normal;
} ddRgb16NormalPoint;

typedef struct {
	ddCoord2DS	pt;
	ddRgbFloatColour	colour;
	ddVector3D	normal;
} ddRgbFloatNormalPoint2DS;

typedef struct {
	ddCoord3D	pt;
	ddRgbFloatColour	colour;
	ddVector3D	normal;
} ddRgbFloatNormalPoint;

typedef struct {
	ddCoord2DS	pt;
	ddHsvColour	colour;
	ddVector3D	normal;
} ddHsvNormalPoint2DS;

typedef struct {
	ddCoord3D	pt;
	ddHsvColour	colour;
	ddVector3D	normal;
} ddHsvNormalPoint;

typedef struct {
	ddCoord2DS	pt;
	ddHlsColour	colour;
	ddVector3D	normal;
} ddHlsNormalPoint2DS;

typedef struct {
	ddCoord3D	pt;
	ddHlsColour	colour;
	ddVector3D	normal;
} ddHlsNormalPoint;

typedef struct {
	ddCoord2DS	pt;
	ddCieColour	colour;
	ddVector3D	normal;
} ddCieNormalPoint2DS;

typedef struct {
	ddCoord3D	pt;
	ddCieColour	colour;
	ddVector3D	normal;
} ddCieNormalPoint;

typedef struct {
	ddCoord3D	pt;
	ddIndexedColour	colour;
	ddULONG		edge;
} ddIndexEdgePoint;

typedef struct {
	ddCoord3D	pt;
	ddRgb8Colour	colour;
	ddULONG		edge;
} ddRgb8EdgePoint;

typedef struct {
	ddCoord3D	pt;
	ddRgb16Colour	colour;
	ddULONG		edge;
} ddRgb16EdgePoint;

typedef struct {
	ddCoord2DS	pt;
	ddRgbFloatColour	colour;
	ddULONG		edge;
} ddRgbFloatEdgePoint2DS;

typedef struct {
	ddCoord3D	pt;
	ddRgbFloatColour	colour;
	ddULONG		edge;
} ddRgbFloatEdgePoint;

typedef struct {
	ddCoord2DS	pt;
	ddHsvColour	colour;
	ddULONG		edge;
} ddHsvEdgePoint2DS;

typedef struct {
	ddCoord3D	pt;
	ddHsvColour	colour;
	ddULONG		edge;
} ddHsvEdgePoint;

typedef struct {
	ddCoord2DS	pt;
	ddHlsColour	colour;
	ddULONG		edge;
} ddHlsEdgePoint2DS;

typedef struct {
	ddCoord3D	pt;
	ddHlsColour	colour;
	ddULONG		edge;
} ddHlsEdgePoint;

typedef struct {
	ddCoord2DS	pt;
	ddCieColour	colour;
	ddULONG		edge;
} ddCieEdgePoint2DS;

typedef struct {
	ddCoord3D	pt;
	ddCieColour	colour;
	ddULONG		edge;
} ddCieEdgePoint;

typedef struct {
	ddCoord2DS       pt;
	ddVector3D	normal;
	ddULONG		edge;
} ddNormEdgePoint2DS;

typedef struct {
	ddCoord3D       pt;
	ddVector3D	normal;
	ddULONG		edge;
} ddNormEdgePoint;

typedef struct {
	ddCoord3D	pt;
	ddIndexedColour	colour;
	ddVector3D	normal;
	ddULONG		edge;
} ddIndexNormEdgePoint;

typedef struct {
	ddCoord3D	pt;
	ddRgb8Colour	colour;
	ddVector3D	normal;
	ddULONG		edge;
} ddRgb8NormEdgePoint;

typedef struct {
	ddCoord3D	pt;
	ddRgb16Colour	colour;
	ddVector3D	normal;
	ddULONG		edge;
} ddRgb16NormEdgePoint;

typedef struct {
	ddCoord2DS	pt;
	ddRgbFloatColour	colour;
	ddVector3D	normal;
	ddULONG		edge;
} ddRgbFloatNormEdgePoint2DS;

typedef struct {
	ddCoord3D	pt;
	ddRgbFloatColour	colour;
	ddVector3D	normal;
	ddULONG		edge;
} ddRgbFloatNormEdgePoint;

typedef struct {
	ddCoord2DS	pt;
	ddHsvColour	colour;
	ddVector3D	normal;
	ddULONG		edge;
} ddHsvNormEdgePoint2DS;

typedef struct {
	ddCoord3D	pt;
	ddHsvColour	colour;
	ddVector3D	normal;
	ddULONG		edge;
} ddHsvNormEdgePoint;

typedef struct {
	ddCoord2DS	pt;
	ddHlsColour	colour;
	ddVector3D	normal;
	ddULONG		edge;
} ddHlsNormEdgePoint2DS;

typedef struct {
	ddCoord3D	pt;
	ddHlsColour	colour;
	ddVector3D	normal;
	ddULONG		edge;
} ddHlsNormEdgePoint;

typedef struct {
	ddCoord2DS	pt;
	ddCieColour	colour;
	ddVector3D	normal;
	ddULONG		edge;
} ddCieNormEdgePoint2DS;

typedef struct {
	ddCoord3D	pt;
	ddCieColour	colour;
	ddVector3D	normal;
	ddULONG		edge;
} ddCieNormEdgePoint;

/* 
 * The point types are internal point types only.
 */

typedef struct {
	ddCoord4D	pt;
	ddIndexedColour	colour;
} ddIndexPoint4D;

typedef struct {
	ddCoord4D	pt;
	ddRgb8Colour	colour;
} ddRgb8Point4D;

typedef struct {
	ddCoord4D	pt;
	ddRgb16Colour	colour;
} ddRgb16Point4D;

typedef struct {
	ddCoord4D	pt;
	ddRgbFloatColour	colour;
} ddRgbFloatPoint4D;

typedef struct {
	ddCoord4D	pt;
	ddHsvColour	colour;
} ddHsvPoint4D;

typedef struct {
	ddCoord4D	pt;
	ddHlsColour	colour;
} ddHlsPoint4D;

typedef struct {
	ddCoord4D	pt;
	ddCieColour	colour;
} ddCiePoint4D;

typedef struct {
	ddCoord4D	pt;
	ddVector3D	normal;
} ddNormalPoint4D;

typedef struct {
	ddCoord4D	pt;
	ddULONG		edge;
} ddEdgePoint4D;

typedef struct {
	ddCoord4D	pt;
	ddIndexedColour	colour;
	ddVector3D	normal;
} ddIndexNormalPoint4D;

typedef struct {
	ddCoord4D	pt;
	ddRgb8Colour	colour;
	ddVector3D	normal;
} ddRgb8NormalPoint4D;

typedef struct {
	ddCoord4D	pt;
	ddRgb16Colour	colour;
	ddVector3D	normal;
} ddRgb16NormalPoint4D;

typedef struct {
	ddCoord4D	pt;
	ddRgbFloatColour	colour;
	ddVector3D	normal;
} ddRgbFloatNormalPoint4D;

typedef struct {
	ddCoord4D	pt;
	ddHsvColour	colour;
	ddVector3D	normal;
} ddHsvNormalPoint4D;

typedef struct {
	ddCoord4D	pt;
	ddHlsColour	colour;
	ddVector3D	normal;
} ddHlsNormalPoint4D;

typedef struct {
	ddCoord4D	pt;
	ddCieColour	colour;
	ddVector3D	normal;
} ddCieNormalPoint4D;

typedef struct {
	ddCoord4D	pt;
	ddIndexedColour	colour;
	ddULONG		edge;
} ddIndexEdgePoint4D;

typedef struct {
	ddCoord4D	pt;
	ddRgb8Colour	colour;
	ddULONG		edge;
} ddRgb8EdgePoint4D;

typedef struct {
	ddCoord4D	pt;
	ddRgb16Colour	colour;
	ddULONG		edge;
} ddRgb16EdgePoint4D;

typedef struct {
	ddCoord4D	pt;
	ddRgbFloatColour	colour;
	ddULONG		edge;
} ddRgbFloatEdgePoint4D;

typedef struct {
	ddCoord4D	pt;
	ddHsvColour	colour;
	ddULONG		edge;
} ddHsvEdgePoint4D;

typedef struct {
	ddCoord4D	pt;
	ddHlsColour	colour;
	ddULONG		edge;
} ddHlsEdgePoint4D;

typedef struct {
	ddCoord4D	pt;
	ddCieColour	colour;
	ddULONG		edge;
} ddCieEdgePoint4D;

typedef struct {
	ddCoord4D       pt;
	ddVector3D	normal;
	ddULONG		edge;
} ddNormEdgePoint4D;

typedef struct {
	ddCoord4D	pt;
	ddIndexedColour	colour;
	ddVector3D	normal;
	ddULONG		edge;
} ddIndexNormEdgePoint4D;

typedef struct {
	ddCoord4D	pt;
	ddRgb8Colour	colour;
	ddVector3D	normal;
	ddULONG		edge;
} ddRgb8NormEdgePoint4D;

typedef struct {
	ddCoord4D	pt;
	ddRgb16Colour	colour;
	ddVector3D	normal;
	ddULONG		edge;
} ddRgb16NormEdgePoint4D;

typedef struct {
	ddCoord4D	pt;
	ddRgbFloatColour	colour;
	ddVector3D	normal;
	ddULONG		edge;
} ddRgbFloatNormEdgePoint4D;

typedef struct {
	ddCoord4D	pt;
	ddHsvColour	colour;
	ddVector3D	normal;
	ddULONG		edge;
} ddHsvNormEdgePoint4D;

typedef struct {
	ddCoord4D	pt;
	ddHlsColour	colour;
	ddVector3D	normal;
	ddULONG		edge;
} ddHlsNormEdgePoint4D;

typedef struct {
	ddCoord4D	pt;
	ddCieColour	colour;
	ddVector3D	normal;
	ddULONG		edge;
} ddCieNormEdgePoint4D;

typedef union {
	ddCoord2D			*p2Dpt;
	ddCoord3D			*p3Dpt;
	ddCoord2DL			*p2DLpt;
	ddCoord3DL			*p3DLpt;
	ddCoord2DS			*p2DSpt;
	ddCoord3DS			*p3DSpt;
	ddIndexedColour			*pIndexClr;
	ddRgb8Colour			*pRgb8Clr;
	ddRgb16Colour			*pRgb16Clr;
	ddRgbFloatColour		*pRgbFloatClr;
	ddHsvColour			*pHsvClr;
	ddHlsColour			*pHlsClr;
	ddCieColour			*pCieClr;
	ddIndexPoint			*pIndexpt;
	ddRgb8Point			*pRgb8pt;  
	ddRgb16Point			*pRgb16pt;
	ddRgbFloatPoint			*pRgbFloatpt;
	ddHsvPoint			*pHsvpt;
	ddHlsPoint			*pHlspt;
	ddCiePoint			*pCiept;
	ddVector3D			*pNormal;
	ddNormalPoint			*pNpt;
	ddULONG				*pEdge;
	ddEdgePoint			*pEpt;
	ddIndexNormalPoint		*pIndexNpt;
	ddRgb8NormalPoint		*pRgb8Npt;
	ddRgb16NormalPoint		*pRgb16Npt;
	ddRgbFloatNormalPoint		*pRgbFloatNpt;
	ddHsvNormalPoint		*pHsvNpt;
	ddHlsNormalPoint		*pHlsNpt;
	ddCieNormalPoint		*pCieNpt;
	ddIndexEdgePoint		*pIndexEpt;
	ddRgb8EdgePoint			*pRgb8Ept;
	ddRgb16EdgePoint		*pRgb16Ept;
	ddRgbFloatEdgePoint		*pRgbFloatEpt;
	ddHsvEdgePoint			*pHsvEpt;
	ddHlsEdgePoint			*pHlsEpt;
	ddCieEdgePoint			*pCieEpt;
	ddNormEdgePoint			*pNEpt;
	ddIndexNormEdgePoint		*pIndexNEpt;
	ddRgb8NormEdgePoint		*pRgb8NEpt;
	ddRgb16NormEdgePoint		*pRgb16NEpt;
	ddRgbFloatNormEdgePoint		*pRgbFloatNEpt;
	ddHsvNormEdgePoint		*pHsvNEpt;
	ddHlsNormEdgePoint		*pHlsNEpt;
	ddCieNormEdgePoint		*pCieNEpt;
	ddCoord4D			*p4Dpt;
	ddIndexPoint4D			*pIndexpt4D;
	ddRgb8Point4D			*pRgb8pt4D;  
	ddRgb16Point4D			*pRgb16pt4D;
	ddRgbFloatPoint4D		*pRgbFloatpt4D;
	ddHsvPoint4D			*pHsvpt4D;
	ddHlsPoint4D			*pHlspt4D;
	ddCiePoint4D			*pCiept4D;
	ddNormalPoint4D			*pNpt4D;
	ddEdgePoint4D			*pEpt4D;
	ddIndexNormalPoint4D		*pIndexNpt4D;
	ddRgb8NormalPoint4D		*pRgb8Npt4D;
	ddRgb16NormalPoint4D		*pRgb16Npt4D;
	ddRgbFloatNormalPoint4D		*pRgbFloatNpt4D;
	ddHsvNormalPoint4D		*pHsvNpt4D;
	ddHlsNormalPoint4D		*pHlsNpt4D;
	ddCieNormalPoint4D		*pCieNpt4D;
	ddIndexEdgePoint4D		*pIndexEpt4D;
	ddRgb8EdgePoint4D		*pRgb8Ept4D;
	ddRgb16EdgePoint4D		*pRgb16Ept4D;
	ddRgbFloatEdgePoint4D		*pRgbFloatEpt4D;
	ddHsvEdgePoint4D		*pHsvEpt4D;
	ddHlsEdgePoint4D		*pHlsEpt4D;
	ddCieEdgePoint4D		*pCieEpt4D;
	ddNormEdgePoint4D		*pNEpt4D;
	ddIndexNormEdgePoint4D		*pIndexNEpt4D;
	ddRgb8NormEdgePoint4D		*pRgb8NEpt4D;
	ddRgb16NormEdgePoint4D		*pRgb16NEpt4D;
	ddRgbFloatNormEdgePoint4D	*pRgbFloatNEpt4D;
	ddHsvNormEdgePoint4D		*pHsvNEpt4D;
	ddHlsNormEdgePoint4D		*pHlsNEpt4D;
	ddCieNormEdgePoint4D		*pCieNEpt4D;
	char				*ptr;
} ddPointUnion;

/*
 * Last - create a point header data structure
 *
 * Note: any changes to this structure MUST also be
 *       made to the MarkerlistofddPoint structure below.
 */
typedef struct {
	ddULONG		numPoints;	/* number of vertices in list */
	ddULONG		maxData;	/* allocated data in bytes */
	ddPointUnion	pts;		/* pointer to vertex data */
} listofddPoint;

/*
 * Create a parallel structure to listofddPoint for
 * pre-initializing Markers. Note that the listofddPoint
 * structure and the MarkerlistofddPoint structure MUST always
 * match with the exception of the union elememt.
 */
typedef struct {
	ddULONG		numPoints;	/* number of vertices in list */
	ddULONG		maxData;	/* allocated data in bytes */
	ddCoord2D	*pts;
} MarkerlistofddPoint;

typedef struct {
	ddUSHORT	order;
	ddFLOAT		uMin;
	ddFLOAT		uMax;
	ddULONG		numKnots;
	ddFLOAT		*pKnots;
	listofddPoint	points;
} ddNurbCurve;

typedef enum {
	DD_FACET_NONE=0,		/* no facet attributes */
	DD_FACET_INDEX=1,		/* facet colour */
	DD_FACET_RGB8=2,		/* facet colour */
	DD_FACET_RGB16=3,		/* facet colour */
	DD_FACET_RGBFLOAT=4,		/* facet colour */
	DD_FACET_HSV=5,			/* facet colour */
	DD_FACET_HLS=6,			/* facet colour */
	DD_FACET_CIE=7,			/* facet colour */
	DD_FACET_NORM=8,		/* facet normal */
	DD_FACET_INDEX_NORM=9,		/* facet colour & normal */
	DD_FACET_RGB8_NORM=10,		/* facet colour & normal */
	DD_FACET_RGB16_NORM=11,		/* facet colour & normal */
	DD_FACET_RGBFLOAT_NORM=12,	/* facet colour & normal */
	DD_FACET_HSV_NORM=13,		/* facet colour & normal */
	DD_FACET_HLS_NORM=14,		/* facet colour & normal */
	DD_FACET_CIE_NORM=15		/* facet colour & normal */
} ddFacetType;

/*
 * A useful macro to determine the size of a facet
 *
 */
#define DDFacetSIZE(type, size)                                         \
        switch((type)){                                                 \
          case(DD_FACET_NONE): (size) = 0;                              \
            break;                                                      \
          case(DD_FACET_INDEX): (size) = sizeof(ddIndexedColour);       \
            break;                                                      \
          case(DD_FACET_RGB8): (size) = sizeof(ddRgb8Colour);           \
            break;                                                      \
          case(DD_FACET_RGB16): (size) = sizeof(ddRgb16Colour);         \
            break;                                                      \
          case(DD_FACET_RGBFLOAT): (size) = sizeof(ddRgbFloatColour);   \
            break;                                                      \
          case(DD_FACET_HSV): (size) = sizeof(ddHsvColour);             \
            break;                                                      \
          case(DD_FACET_HLS): (size) = sizeof(ddHlsColour);             \
            break;                                                      \
          case(DD_FACET_CIE): (size) = sizeof(ddCieColour);             \
            break;                                                      \
          case(DD_FACET_NORM): (size) = sizeof(ddVector3D);             \
            break;                                                      \
          case(DD_FACET_INDEX_NORM): (size) = sizeof(ddIndexNormal);    \
            break;                                                      \
          case(DD_FACET_RGB8_NORM): (size) = sizeof(ddRgb8Normal);      \
            break;                                                      \
          case(DD_FACET_RGB16_NORM): (size) = sizeof(ddRgb16Normal);    \
            break;                                                      \
          case(DD_FACET_RGBFLOAT_NORM): (size) = sizeof(ddRgbFloatNormal);\
            break;                                                      \
          case(DD_FACET_HSV_NORM): (size) = sizeof(ddHsvNormal);        \
            break;                                                      \
          case(DD_FACET_HLS_NORM): (size) = sizeof(ddHlsNormal);        \
            break;                                                      \
          case(DD_FACET_CIE_NORM): (size) = sizeof(ddCieNormal);        \
            break;                                                      \
	  default: (size) = -1;						\
        }

/*
 * more facet macros for determining type.
 */
#define DD_IsFacetNormal(type)	\
((((int)type)>=((int)DD_FACET_NORM))&&(((int)type)<=((int)DD_FACET_CIE_NORM)))
#define DD_IsFacetColour(type)	\
( ((type) != DD_FACET_NONE) && ((type) != DD_FACET_NORM) )

typedef struct {
	ddIndexedColour	colour;
	ddVector3D	normal;
} ddIndexNormal;

typedef struct {
	ddRgb8Colour	colour;
	ddVector3D	normal;
} ddRgb8Normal;

typedef struct {
	ddRgb16Colour	colour;
	ddVector3D	normal;
} ddRgb16Normal;

typedef struct {
	ddRgbFloatColour	colour;
	ddVector3D	normal;
} ddRgbFloatNormal;

typedef struct {
	ddHsvColour	colour;
	ddVector3D	normal;
} ddHsvNormal;

typedef struct {
	ddHlsColour	colour;
	ddVector3D	normal;
} ddHlsNormal;

typedef struct {
	ddCieColour	colour;
	ddVector3D	normal;
} ddCieNormal;

typedef union {
	ddPointer		pNoFacet;
	ddIndexedColour		*pFacetIndex;
	ddRgb8Colour		*pFacetRgb8;
	ddRgb16Colour		*pFacetRgb16;
	ddRgbFloatColour	*pFacetRgbFloat;
	ddHsvColour		*pFacetHsv;
	ddHlsColour		*pFacetHls;
	ddCieColour		*pFacetCie;
	ddVector3D		*pFacetN;
	ddIndexNormal		*pFacetIndexN;
	ddRgb8Normal		*pFacetRgb8N;
	ddRgb16Normal		*pFacetRgb16N;
	ddRgbFloatNormal	*pFacetRgbFloatN;
	ddHsvNormal		*pFacetHsvN;
	ddHlsNormal		*pFacetHlsN;
	ddCieNormal		*pFacetCieN;
} ddFacetUnion;

typedef struct {
	ddFacetType	type;
	ddULONG		numFacets;	/* number of facets in list */
	ddULONG		maxData;	/* allocated data in bytes */
	ddFacetUnion	facets;		/* Pointer to facet data */
} listofddFacet;

typedef enum {
	DD_VERTEX=0,
	DD_VERTEX_EDGE=1
} ddVertIndexType;

typedef struct {
	ddUSHORT	index;
	ddULONG		edge;
} ddVertIndexEdge;
	
typedef struct {
	ddVertIndexType	type;
	ddULONG		numIndex;
	union {
		ddUSHORT	*pVertIndex;
		ddVertIndexEdge		*pVertIndexE;
	} index;
} listofddIndex;

typedef struct {
	ddUSHORT	uOrder;
	ddUSHORT	vOrder;
	ddFLOAT		uMin;
	ddFLOAT		uMax;
	ddFLOAT		vMin;
	ddFLOAT		vMax;
	ddFLOAT		mpts;
	ddFLOAT		npts;
	ddULONG		numUKnots;
	ddFLOAT		*pUKnot;
	ddULONG		numVKnots;
	ddFLOAT		*pVKnot;
	listofddPoint	points;
} ddNurbSurface;

typedef struct {
	ddBYTE		visibility;
	ddUSHORT	order;
	ddCurveApprox	curveApprox;
	ddFLOAT		uMin;
	ddFLOAT		uMax;
	ddULONG		numKnots;
	ddFLOAT		*pKnots;
	ddPointType	pttype;
	listofddPoint	points;
} ddTrimCurve;

typedef struct {
	ddULONG		count;
	ddTrimCurve	*pTC;
} listofTrimCurve;

/* for cell arrays */
typedef struct {
	ddSHORT	colourType;
	union {
		ddIndexedColour		*pIndex;
		ddRgb8Colour		*pRgb8;
		ddRgb16Colour		*pRgb16;
		ddRgbFloatColour	*pRgbFloat;
		ddHsvColour		*pHsv;
		ddHlsColour		*pHls;
		ddCieColour		*pCie;
	} colour;
} listofColour;

#endif   /* DDPEX3_H */
