/* $Xorg: pexExtract.h,v 1.4 2001/02/09 02:04:18 xorgcvs Exp $ */


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

#ifndef PEX_EXTRACT_H
#define PEX_EXTRACT_H	1

/**  Note that these extract macros, when dealing with data items which
 ** require more than a few bytes of storage (the only exceptions being
 ** CARD8, CARD16, and CARD32), simply set the destination to point into
 ** the packet from which the data is being "extracted".  This is 
 ** legal since the data is in the correct format (PEX format) in
 ** that packet.  The pointer into the packet is then
 ** incremented by the appropriate number of bytes.
 **/
 
#define REFER_COORD3D(dstPtr, srcPtr) {	    \
    (dstPtr) = (ddCoord3D *)(srcPtr);	    \
    (srcPtr) = ((CARD8 *) (srcPtr)) + sizeof(pexCoord3D); }

#define REFER_LISTOF_COORD3D(num, dstPtr, srcPtr) {	\
    (dstPtr) = (ddCoord3D *)(srcPtr);			\
    (srcPtr) = ((CARD8 *) (srcPtr)) + num * sizeof(pexCoord3D); }

#define REFER_LISTOF_COORD2D(num, dstPtr, srcPtr) {	\
    (dstPtr) = (ddCoord2D *)(srcPtr);			\
    (srcPtr) = ((CARD8 *) (srcPtr)) + num * sizeof(pexCoord2D); }

#define REFER_LISTOF_CARD16(num, dstPtr, srcPtr) {	\
    (dstPtr) = (CARD16 *)(srcPtr);			\
    (srcPtr) = ((CARD8 *) (srcPtr)) + num * sizeof(CARD16); }

#define REFER_CARD8(dst, srcPtr) {	    \
    (dst) = *((CARD8 *)(srcPtr));	    \
    (srcPtr) = ((CARD8 *) (srcPtr)) + sizeof(CARD8); }

#define REFER_CARD16(dst, srcPtr) {	    \
    (dst) = *((CARD16 *)(srcPtr));	    \
    (srcPtr) = ((CARD8 *) (srcPtr)) + sizeof(CARD16); }

#define REFER_CARD32(dst, srcPtr) {	    \
    (dst) = *((CARD32 *)(srcPtr));	    \
    (srcPtr) = ((CARD8 *) (srcPtr)) + sizeof(CARD32); }

#define REFER_FLOAT(dst, srcPtr) {	    \
    (dst) = *((PEXFLOAT *)(srcPtr));	    \
    (srcPtr) = ((CARD8 *) (srcPtr)) + sizeof(PEXFLOAT); }

/** This is a very general macro which will set the destination pointer to
 ** be whatever the src pointer is, typecast to the specified type.  The
 ** src pointer is then incremented by the size of the specified type
 ** multiplied by a factor representing the number of such structures to 
 ** be skipped over.
 **/

#define REFER_STRUCT(num, data_type, dstPtr, srcPtr) {	    \
    (dstPtr) = (data_type *)(srcPtr);			    \
    (srcPtr) = ((CARD8 *) (srcPtr)) + num * sizeof(data_type); }


/*
    The next set of macros actually copy the data from a packet
    into the destination data structure.
*/

#define EXTRACT_COORD3D(dstPtr, srcPtr) {  \
    (dstPtr)->x = ((pexCoord3D *)((srcPtr)))->x;  \
    (dstPtr)->y = ((pexCoord3D *)(srcPtr))->y;  \
    (dstPtr)->z = ((pexCoord3D *)(srcPtr))->z;  \
    (srcPtr) = ((CARD8 *) (srcPtr)) + sizeof(pexCoord3D); }

#define EXTRACT_VECTOR3D(dstPtr, srcPtr) {  \
    (dstPtr)->x = ((pexVector3D *)((srcPtr)))->x;  \
    (dstPtr)->y = ((pexVector3D *)(srcPtr))->y;  \
    (dstPtr)->z = ((pexVector3D *)(srcPtr))->z;  \
    (srcPtr) = ((CARD8 *) (srcPtr)) + sizeof(pexVector3D); }

#define EXTRACT_LISTOF_COORD3D(num, dstPtr, srcPtr) \
    EXTRACT_STRUCT(num, ddCoord3D, dstPtr, srcPtr)

#define EXTRACT_COORD2D(dstPtr, srcPtr) {  \
    (dstPtr)->x = ((pexCoord2D *)(srcPtr))->x;  \
    (dstPtr)->y = ((pexCoord2D *)(srcPtr))->y;  \
    (srcPtr) = ((CARD8 *) (srcPtr)) + sizeof(pexCoord2D); }

#define EXTRACT_VECTOR2D(dstPtr, srcPtr) {  \
    (dstPtr)->x = ((pexVector2D *)(srcPtr))->x;  \
    (dstPtr)->y = ((pexVector2D *)(srcPtr))->y;  \
    (srcPtr) = ((CARD8 *) (srcPtr)) + sizeof(pexVector2D); }

#define EXTRACT_LISTOF_COORD2D(num, dstPtr, srcPtr) \
    EXTRACT_STRUCT(num, ddCoord2D, dstPtr, srcPtr)

/* Takes a CARD8 from a 4 byte Protocol Field  */
#define EXTRACT_CARD8_FROM_4B(dst, srcPtr)	{   \
    (dst) = (CARD8) (*((CARD32 *)(srcPtr)));	    \
    (srcPtr) = ((CARD8 *) (srcPtr)) + sizeof(CARD32); }

#define EXTRACT_CARD8(dst, srcPtr)	{   \
    (dst) = *((CARD8 *)(srcPtr));	    \
    (srcPtr) = ((CARD8 *) (srcPtr)) + sizeof(CARD8); }

/* Takes a CARD16 from a 4 byte Protocol Field  */
#define EXTRACT_CARD16_FROM_4B(dst, srcPtr)	{   \
    (dst) = (CARD16) (*((CARD32 *)(srcPtr)));	    \
    (srcPtr) = ((CARD8 *) (srcPtr)) + sizeof(CARD32); }

/* Takes a INT16 from a 4 byte Protocol Field  */
#define EXTRACT_INT16_FROM_4B(dst, srcPtr)	{   \
    (dst) = (INT16) (*((CARD32 *)(srcPtr)));	    \
    (srcPtr) = ((CARD8 *) (srcPtr)) + sizeof(CARD32); }

#define EXTRACT_CARD16(dst, srcPtr)	{   \
    (dst) = *((CARD16 *)(srcPtr));	    \
    (srcPtr) = ((CARD8 *) (srcPtr)) + sizeof(CARD16); }

#define EXTRACT_INT16(dst, srcPtr)	{   \
    (dst) = *((INT16 *)(srcPtr));	    \
    (srcPtr) = ((CARD8 *) (srcPtr)) + sizeof(INT16); }

#define EXTRACT_CARD32(dst, srcPtr)	{   \
    (dst) = *((CARD32 *)(srcPtr));	    \
    (srcPtr) = ((CARD8 *) (srcPtr)) + sizeof(CARD32); }

#define EXTRACT_FLOAT(dst, srcPtr)	{   \
    (dst) = *((PEXFLOAT *)(srcPtr));	    \
    (srcPtr) = ((CARD8 *) (srcPtr)) + sizeof(PEXFLOAT); }


#define EXTRACT_COLOUR_SPECIFIER(dst, srcPtr) { \
	EXTRACT_CARD16 ((dst).colourType, (srcPtr));\
	SKIP_PADDING ((srcPtr), 2);\
	switch ((dst).colourType) {\
	    case PEXIndexedColour: {\
		EXTRACT_CARD16((dst).colour.indexed.index, (srcPtr));\
		SKIP_PADDING((srcPtr),2);\
		break;\
	    }\
	    case PEXRgbFloatColour: {\
		EXTRACT_FLOAT((dst).colour.rgbFloat.red,(srcPtr));\
		EXTRACT_FLOAT((dst).colour.rgbFloat.green,(srcPtr));\
		EXTRACT_FLOAT((dst).colour.rgbFloat.blue,(srcPtr));\
		break;\
	    }\
	    case PEXCieFloatColour: {\
		EXTRACT_FLOAT((dst).colour.cieFloat.x,(srcPtr));\
		EXTRACT_FLOAT((dst).colour.cieFloat.y,(srcPtr));\
		EXTRACT_FLOAT((dst).colour.cieFloat.z,(srcPtr));\
		break;\
	    }\
	    case PEXHsvFloatColour: {\
		EXTRACT_FLOAT((dst).colour.hsvFloat.hue,(srcPtr));\
		EXTRACT_FLOAT((dst).colour.hsvFloat.saturation,(srcPtr));\
		EXTRACT_FLOAT((dst).colour.hsvFloat.value,(srcPtr));\
		break;\
	    }\
	    case PEXHlsFloatColour: {\
		EXTRACT_FLOAT((dst).colour.hlsFloat.hue,(srcPtr));\
		EXTRACT_FLOAT((dst).colour.hlsFloat.lightness,(srcPtr));\
		EXTRACT_FLOAT((dst).colour.hlsFloat.saturation,(srcPtr));\
		break;\
	    }\
	    case PEXRgb8Colour: {\
		EXTRACT_CARD8((dst).colour.rgb8.red,(srcPtr));\
		EXTRACT_CARD8((dst).colour.rgb8.green,(srcPtr));\
		EXTRACT_CARD8((dst).colour.rgb8.blue,(srcPtr));\
		SKIP_PADDING((srcPtr),1);\
		break;\
	    }\
	    case PEXRgb16Colour: {\
		EXTRACT_CARD16((dst).colour.rgb16.red,(srcPtr));\
		EXTRACT_CARD16((dst).colour.rgb16.green,(srcPtr));\
		EXTRACT_CARD16((dst).colour.rgb16.blue,(srcPtr));\
		SKIP_PADDING((srcPtr),2);\
		break;\
	    }\
	}}

/* JSH - assuming copy may overlap */
#define EXTRACT_STRUCT(num, data_type, dstPtr, srcPtr) {\
	memmove(	(char *)(dstPtr), (char *)(srcPtr), \
		(int)(num * sizeof(data_type)));\
	(srcPtr) = ((CARD8 *) (srcPtr)) + num * sizeof(data_type); }

/*
    The next set of macros actually copy the data from a structure
    into the destination reply packet.
*/

#define PACK_COORD3D(srcPtr, dstPtr)	{       \
    ((pexCoord3D *)(dstPtr))->x = (srcPtr)->x;  \
    ((pexCoord3D *)(dstPtr))->y = (srcPtr)->y;  \
    ((pexCoord3D *)(dstPtr))->z = (srcPtr)->z;  \
    (dstPtr) = ((CARD8 *) (dstPtr)) + sizeof(pexCoord3D); }

#define PACK_LISTOF_COORD3D(NUM, SRC, DST)  \
    PACK_LISTOF_STRUCT(NUM, pexCoord3D, SRC, DST)

#define PACK_COORD2D(srcPtr, dstPtr)	{       \
    ((pexCoord2D *)(dstPtr))->x = (srcPtr)->x;  \
    ((pexCoord2D *)(dstPtr))->y = (srcPtr)->y;  \
    (dstPtr) = ((CARD8 *) (dstPtr)) + sizeof(pexCoord2D); }

#define PACK_LISTOF_COORD2D(NUM, SRC, DST)  \
    PACK_LISTOF_STRUCT(NUM, pexCoord2D, SRC, DST)

#define PACK_VECTOR3D(srcPtr, dstPtr)	{       \
    ((pexVector3D *)(dstPtr))->x = (srcPtr)->x; \
    ((pexVector3D *)(dstPtr))->y = (srcPtr)->y; \
    ((pexVector3D *)(dstPtr))->z = (srcPtr)->z; \
    (dstPtr) = ((CARD8 *) (dstPtr)) + sizeof(pexCoord3D); }

#define PACK_VECTOR2D(srcPtr, dstPtr) {         \
    ((pexVector2D *)(dstPtr))->x = (srcPtr)->x; \
    ((pexVector2D *)(dstPtr))->y = (srcPtr)->y; \
    (dstPtr) = ((CARD8 *) (dstPtr)) + sizeof(pexCoord2D); }


#define PACK_CARD8(src, dstPtr)	{\
    *((CARD8 *)(dstPtr)) = (CARD8)(src);\
    (dstPtr) = ((CARD8 *) (dstPtr)) + sizeof(CARD8); }

#define PACK_CARD16(src, dstPtr)	{   \
    *((CARD16 *)(dstPtr)) = (CARD16)(src);  \
    (dstPtr) = ((CARD8 *) (dstPtr)) + sizeof(CARD16); }

#define PACK_INT16(src, dstPtr)	{   	\
    *((INT16 *)(dstPtr)) = (INT16)(src);\
    (dstPtr) = ((CARD8 *) (dstPtr)) + sizeof(INT16); }

#define PACK_CARD32(src, dstPtr)	{   \
    *((CARD32 *)(dstPtr)) = (CARD32)(src);  \
    (dstPtr) = ((CARD8 *) (dstPtr)) + sizeof(CARD32); }

#define PACK_FLOAT(src, dstPtr)	{\
    *((PEXFLOAT *)(dstPtr)) = (PEXFLOAT)(src);\
    (dstPtr) = ((CARD8 *) (dstPtr)) + sizeof(PEXFLOAT); }


#define PACK_COLOUR_SPECIFIER(src, dstPtr) {\
	PACK_CARD16 ((src).colourType, (dstPtr));\
	SKIP_PADDING ((dstPtr), 2);\
	switch ((src).colourType) {\
	    case PEXIndexedColour: {\
		PACK_CARD16((src).colour.indexed.index,(dstPtr));\
		SKIP_PADDING((dstPtr),2);\
		break;\
	    }\
	    case PEXRgbFloatColour: {\
		PACK_FLOAT((src).colour.rgbFloat.red,(dstPtr));\
		PACK_FLOAT((src).colour.rgbFloat.green,(dstPtr));\
		PACK_FLOAT((src).colour.rgbFloat.blue,(dstPtr));\
		break;\
	    }\
	    case PEXCieFloatColour: {\
		PACK_FLOAT((src).colour.cieFloat.x,(dstPtr));\
		PACK_FLOAT((src).colour.cieFloat.y,(dstPtr));\
		PACK_FLOAT((src).colour.cieFloat.z,(dstPtr));\
		break;\
	    }\
	    case PEXHsvFloatColour: {\
		PACK_FLOAT((src).colour.hsvFloat.hue,(dstPtr));\
		PACK_FLOAT((src).colour.hsvFloat.saturation,(dstPtr));\
		PACK_FLOAT((src).colour.hsvFloat.value,(dstPtr));\
		break;\
	    }\
	    case PEXHlsFloatColour: {\
		PACK_FLOAT((src).colour.hlsFloat.hue,(dstPtr));\
		PACK_FLOAT((src).colour.hlsFloat.lightness,(dstPtr));\
		PACK_FLOAT((src).colour.hlsFloat.saturation,(dstPtr));\
		break;\
	    }\
	    case PEXRgb8Colour: {\
		PACK_CARD8((src).colour.rgb8.red,(dstPtr));\
		PACK_CARD8((src).colour.rgb8.green,(dstPtr));\
		PACK_CARD8((src).colour.rgb8.blue,(dstPtr));\
		SKIP_PADDING((dstPtr),1);\
		break;\
	    }\
	    case PEXRgb16Colour: {\
		PACK_CARD16((src).colour.rgb16.red,(dstPtr));\
		PACK_CARD16((src).colour.rgb16.green,(dstPtr));\
		PACK_CARD16((src).colour.rgb16.blue,(dstPtr));\
		SKIP_PADDING((dstPtr),2);\
		break;\
	    }\
	}}

/* JSH - assuming copy may overlap */
#define PACK_STRUCT(data_type,srcPtr,dstPtr)	{       \
    memmove(  (char *)(dstPtr),	(char *)(srcPtr), 	\
	    sizeof(data_type));				\
    SKIP_STRUCT(dstPtr, 1, data_type); }

/* JSH - assuming copy may overlap */
#define PACK_LISTOF_STRUCT(num,data_type,srcPtr,dstPtr){\
    memmove(  (char *)(dstPtr),	(char *)(srcPtr), 	\
	    (int)(num * sizeof(data_type)));	\
    SKIP_STRUCT(dstPtr, num, data_type); }
/*
	Other useful macros
 */

#define SKIP_PADDING(skipPtr, bytesToSkip) \
  (skipPtr) = ((CARD8 *) (skipPtr)) + bytesToSkip

#define SKIP_STRUCT(skipPtr, num, data_type) \
   (skipPtr) = (unsigned char *)(((data_type *)skipPtr) + (num))

#define SIZE_COLOURSPEC(cs) ColourSpecSizes[cs.colourType]

/*
###define SIZE_COLOURSPEC(cs) \
    (cs->colourType == PEXIndexedColour)?sizeof(CARD32):\
	((cs->colourType == PEXRgb8Colour)?sizeof(CARD32):\
	    ((cs->colourType == PEXRgb16Colour)?2*sizeof(CARD32):\
		3*sizeof(PEXFLOAT) ) )
*/

#endif /* PEX_EXTRACT_H */
