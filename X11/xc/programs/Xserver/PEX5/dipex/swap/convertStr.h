/* $Xorg: convertStr.h,v 1.4 2001/02/09 02:04:17 xorgcvs Exp $ */

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

#ifndef CONVERTSTR_H
#define CONVERTSTR_H 1

#include "SwapConv.h"

extern unsigned char temp;	/* only used for conversions */

/*
	Contains macros to convert pex data structures
 */


#define	SWAP_CARD32(a)		SWAPINT(a)

#define	SWAP_INT32(a)		SWAPINT(a)

#define	SWAP_CARD16(a)		SWAPSHORT(a)

#define	SWAP_INT16(a)		SWAPSHORT(a)

#define	SWAP_FLOAT(a)		SWAPFLOAT(a)

#define	SWAP_DRAWABLE(a)	SWAP_CARD32(a)
#define	SWAP_ASF_ATTR(a)	SWAP_CARD32(a)
#define	SWAP_BITMASK(a)		SWAP_CARD32(a)
#define	SWAP_BITMASK_SHORT(a)	SWAP_CARD16(a)
#define	SWAP_COLOUR_TYPE(a)	SWAP_CARD16(a)
#define	SWAP_COORD_TYPE(a)	SWAP_CARD16(a)
#define	SWAP_COMPOSITION(a)	SWAP_CARD16(a)
#define	SWAP_CULL_MODE(a)	SWAP_CARD16(a)
#define	SWAP_ENUM_TYPE_INDEX(a)	SWAP_INT16(a)
#define	SWAP_LOOKUP_TABLE(a)	SWAP_CARD32(a)
#define	SWAP_NAME(a)		SWAP_CARD32(a)
#define	SWAP_NAMESET(a)		SWAP_CARD32(a)
#define	SWAP_PC(a)		SWAP_CARD32(a)
#define	SWAP_FONT(a)		SWAP_CARD32(a)

#define	SWAP_MATRIX(m) {\
	int i, j; \
	for (i=0; i<4; i++) \
	    for (j=0; j<4; j++) \
		SWAP_FLOAT((m)[i][j]); }

#define	SWAP_MATRIX_3X3(m) {\
	int i, j; \
	for (i=0; i<3; i++) \
	    for (j=0; j<3; j++) \
		SWAP_FLOAT((m)[i][j]); }


#define	SWAP_PHIGS_WKS(a)		SWAP_CARD32(a)
#define	SWAP_PICK_MEASURE(a)		SWAP_CARD32(a)
#define	SWAP_RENDERER(a)		SWAP_CARD32(a)
#define	SWAP_SC(a)			SWAP_CARD32(a)
#define	SWAP_STRUCTURE(a)		SWAP_CARD32(a)
#define	SWAP_TABLE_INDEX(a)		SWAP_CARD16(a)
#define	SWAP_TABLE_TYPE(a)		SWAP_CARD16(a)
#define	SWAP_TEXT_H_ALIGNMENT(a)	SWAP_CARD16(a)
#define	SWAP_TEXT_V_ALIGNMENT(a)	SWAP_CARD16(a)
#define	SWAP_TYPE_OR_TABLEINDEX(a)	SWAP_CARD16(a)

#define	SWAP_STRING(S) 		SWAP_CARD16 ((S).length)

#define	SWAP_VECTOR2D(V) {\
	SWAP_FLOAT ((V).x);\
	SWAP_FLOAT ((V).y); }

#define	SWAP_VECTOR3D(V) {\
	SWAP_FLOAT ((V).x);\
	SWAP_FLOAT ((V).y);\
	SWAP_FLOAT ((V).z);}

#define	SWAP_COORD2D(a)		SWAP_VECTOR2D(a)
#define	SWAP_COORD3D(a)		SWAP_VECTOR3D(a)

#define SWAP_COORD4D(V) {\
	SWAP_FLOAT ((V).x);\
	SWAP_FLOAT ((V).y);\
	SWAP_FLOAT ((V).z);\
	SWAP_FLOAT ((V).w);}

#define	SWAP_RGB_FLOAT_COLOUR(C) {\
	SWAP_FLOAT ((C).red);\
	SWAP_FLOAT ((C).green);\
	SWAP_FLOAT ((C).blue); }

#define	SWAP_HSV_COLOUR(C) {\
	SWAP_FLOAT ((C).hue);\
	SWAP_FLOAT ((C).saturation);\
	SWAP_FLOAT ((C).value); }

#define	SWAP_HLS_COLOUR(C) {\
	SWAP_FLOAT ((C).hue);\
	SWAP_FLOAT ((C).lightness);\
	SWAP_FLOAT ((C).saturation); }

#define	SWAP_CIE_COLOUR(a)		SWAP_VECTOR3D(a)

#define	SWAP_RGB16_COLOUR(C) {\
	SWAP_CARD16 ((C).red);\
	SWAP_CARD16 ((C).green);\
	SWAP_CARD16 ((C).blue); }


#define	SWAP_INDEXED_COLOUR(C)		SWAP_TABLE_INDEX ((C).index)

#define	SWAP_CURVE_APPROX(S) {\
	SWAP_ENUM_TYPE_INDEX ((S).approxMethod); \
	SWAP_FLOAT ((S).tolerance); }

#define	SWAP_DEVICE_COORD(C) {\
	SWAP_CARD16 ((C).x); \
	SWAP_CARD16 ((C).y);\
	SWAP_FLOAT ((C).z); }

#define	SWAP_ELEMENT_INFO(C) {\
	SWAP_CARD16 ((C).elementType);\
	SWAP_CARD16 ((C).length); }

#define	SWAP_ELEMENT_POS(C) {\
	SWAP_CARD16 ((C).whence);\
	SWAP_INT32 ((C).offset); }

#define	SWAP_ELEMENT_REF(C) {\
	SWAP_STRUCTURE ((C).structure);\
	SWAP_CARD32 ((C).offset); }

#define SWAP_EDGEOPT(E) 		SWAP_CARD16 ((E).edge)

#define	SWAP_ENUM_TYPE_DESC(D) {\
	SWAP_ENUM_TYPE_INDEX ((D).index); \
	SWAP_STRING ((D).descriptor); }

#define	SWAP_TEXT_ALIGN_DATA(A) { \
	SWAP_TEXT_H_ALIGNMENT((A).horizontal); \
	SWAP_TEXT_V_ALIGNMENT((A).vertical); }

#define SWAP_PICK_ELEMENT_REF(P) { \
	SWAP_STRUCTURE ((P).sid); \
	SWAP_CARD32 ((P).offset); \
	SWAP_CARD32 ((P).pickid); }


#define SWAP_STRUCT_INFO(S) { \
	SWAP_STRUCTURE((S).sid); \
	SWAP_FLOAT((S).priority);}

#define SWAP_RENDERER_TARGET(S) { \
	SWAP_CARD16((S).type); \
	SWAP_CARD32((S).visualID); }

#endif	/* CONVERTSTR_H */
