/* $Xorg: ddpex.h,v 1.4 2001/02/09 02:04:18 xorgcvs Exp $ */

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

#ifndef DDPEX_H
#define DDPEX_H

#include "X.h"
#include "PEX.h"
#include "pixmapstr.h"
#include "dix.h"
#include "Xprotostr.h"

/* Basic data types */
/* Many of the typdefs in this file look like the protocol structures 
 * This is intentional.  The protocol structures specify bit fields
 * and the SI ddpex does not use bit fields.  Therefore, ddpex uses its
 * own data definitions, not the ones used by the protocol.
 ****************************************************************************
 * THE SIZE AND ALIGNMENT OF THESE STRUCTURES IS EXPECTED MATCH THE PROTOCOL
 ****************************************************************************
 * This is VERY important.  Although ddpex does not use bit fields, it does
 * assume that short ints are 16 bits, int/long ints are 32 bits.  Pads and
 * unused fields have been kept in the ddpex structures so that they match
 * the protocol structures exactly, allowing type casting and fast copies
 * between the structures.  
 * ddFLOAT is for server internal floating point representations
 */

typedef char    ddCHAR;
typedef short   ddSHORT;
typedef long    ddLONG;
typedef float   ddFLOAT;

typedef unsigned char   ddUCHAR;
typedef unsigned short  ddUSHORT;
typedef unsigned long   ddULONG;

typedef unsigned char    *ddPointer;
typedef unsigned char   ddBYTE;
typedef unsigned char   ddBOOL;

/* Let the default font that gets opened up be Monospaced Roman */
#define DEFAULT_PEX_FONT_NAME	"Roman_M"

/* Resource structures passed between dipex and ddpex. Most of the resources
 * data is kept in dd structures which are opaque to dipex.  Info needed by
 * dipex is kept visible to it in these structures
 */
typedef struct {
	XID	id;
	ddPointer	deviceData;
} ddWKSResource;

typedef struct {
	XID	id;
	ddUSHORT	lutType;
	ddPointer	deviceData;
} ddLUTResource;

typedef ddWKSResource ddStructResource;
typedef ddWKSResource ddNSResource;
typedef ddWKSResource ddFontResource;
typedef ddWKSResource ddPMResource;

#define GetNSId(HANDLE) HANDLE->id

#define GetLUTId(HANDLE) HANDLE->id

typedef ddWKSResource		*diWKSHandle;
typedef ddLUTResource		*diLUTHandle;
typedef ddStructResource	*diStructHandle;
typedef ddNSResource		*diNSHandle;
typedef ddFontResource		*diFontHandle;
typedef ddPMResource		*diPMHandle;
typedef ddPointer		diResourceHandle;

/* PEXprotost.h equivalents */

typedef ddULONG		ddBitmask;
typedef	ddUSHORT	ddBitmaskShort;
typedef	ddSHORT		ddEnumTypeIndex;
typedef	XID		ddResourceId;
typedef	ddBYTE		ddSwitch;
typedef	ddUSHORT	ddTableIndex;

typedef struct {
    ddFLOAT	x;
    ddFLOAT	y;
} ddVector2D;

typedef struct {
    ddFLOAT	x;
    ddFLOAT	y;
    ddFLOAT	z;
} ddVector3D;

typedef struct {
    ddFLOAT	x;
    ddFLOAT	y;
} ddCoord2D;

typedef struct {
    ddLONG	x;
    ddLONG	y;
} ddCoord2DL;

typedef xPoint ddCoord2DS; /* must be the same type for compiler's sake */
/* typedef struct { ddSHORT x; ddSHORT y; } ddCoord2DS; */

typedef struct {
    ddFLOAT	x;
    ddFLOAT	y;
    ddFLOAT	z;
} ddCoord3D;

typedef struct {
    ddLONG	x;
    ddLONG	y;
    ddLONG	z;
} ddCoord3DL;

typedef struct {
    ddSHORT	x;
    ddSHORT	y;
    ddSHORT	z;
} ddCoord3DS;

typedef struct {
    ddFLOAT	x;
    ddFLOAT	y;
    ddFLOAT	z;
    ddFLOAT	w;
} ddCoord4D;

typedef struct {
	ddSHORT		x;
	ddSHORT 	y;
	ddFLOAT 	z;
} ddDeviceCoord;

typedef struct {
	ddSHORT		xmin;
	ddSHORT		ymin;
	ddSHORT		xmax;
	ddSHORT		ymax;
} ddDeviceRect;

typedef struct {
        ddFLOAT         xmin;
        ddFLOAT         ymin;
        ddFLOAT         zmin;
        ddFLOAT         wmin;
        ddFLOAT         xmax;
        ddFLOAT         ymax;
        ddFLOAT         zmax;
        ddFLOAT         wmax;
} ddListBounds;

typedef struct {
    ddSHORT		approxMethod;
    ddUSHORT		unused;
    ddFLOAT		tolerance;
} ddCurveApprox;

typedef struct {
    ddSHORT		approxMethod;
    ddUSHORT		unused;
    ddFLOAT		uTolerance;
    ddFLOAT		vTolerance;
} ddSurfaceApprox;

typedef struct {
    ddUSHORT		vertical;
    ddUSHORT		horizontal;
} ddTextAlignmentData;

typedef struct {
    ddDeviceCoord	minval;
    ddDeviceCoord	maxval;
    ddUCHAR		useDrawable;
    ddBYTE		pad[3];
} ddViewport;

typedef struct {
    ddCoord2DS      position;
    ddFLOAT         distance;
} ddDC_HitBox;

typedef struct {
    ddCoord3D	minval;
    ddCoord3D	maxval;
} ddNpcSubvolume;

typedef  ddNpcSubvolume ddNPC_HitVolume;

typedef struct {
	ddUSHORT	clipFlags;
	ddUSHORT	unused;
	ddNpcSubvolume	clipLimits;
	ddFLOAT		orientation[4][4];
	ddFLOAT		mapping[4][4];
} ddViewEntry;

typedef struct {
	ddUSHORT	definableEntries;
	ddUSHORT	numPredefined;
	ddUSHORT	predefinedMin;
	ddUSHORT	predefinedMax;
} ddTableInfo;


typedef struct {
        diStructHandle    structure;	/* the structure id is replaced with */
        ddULONG         offset;		/* the handle */
} ddElementRef;

typedef struct {
        ddCoord4D       orig_point;	/* original PEX ref point */
        ddCoord4D       point;
        ddVector3D      orig_vector;	/* original PEX ref vector*/	
        ddVector3D      vector;		/* normalized vector */
	ddFLOAT		dist;		/* Hessian form distance func */
} ddHalfSpace; 				

typedef struct {
        diStructHandle    structure;	/* the structure id is replaced with the */
        ddULONG         offset;		/* handle by diPEX */
        ddULONG         pickid;
} ddPickPath;

typedef struct {
    ddULONG		sid;
    ddULONG             offset;
    ddULONG             pickid;
} ddPickElementRef;

typedef struct {
      ddUSHORT        pickType;
      union {
	  ddDC_HitBox        DC_HitBox;
	  ddNPC_HitVolume    NPC_HitVolume;
      } hit_box;
} ddPickRecord;

typedef struct {
	ddUSHORT	elementType;
	ddUSHORT	length;
} ddElementInfo;

/* Colour structures */
typedef struct {
    ddFLOAT	red;
    ddFLOAT	green;
    ddFLOAT	blue;
} ddRgbFloatColour;

typedef struct {
    ddFLOAT	hue;
    ddFLOAT	saturation;
    ddFLOAT	value;
} ddHsvColour;

typedef struct {
    ddFLOAT	hue;
    ddFLOAT	lightness;
    ddFLOAT	saturation;
} ddHlsColour;

typedef struct {
    ddFLOAT	x;
    ddFLOAT	y;
    ddFLOAT	z;
} ddCieColour;

typedef struct {
    ddUCHAR	red;
    ddUCHAR	green;
    ddUCHAR	blue;
    ddUCHAR	pad;
} ddRgb8Colour;

typedef struct {
    ddUSHORT	red;
    ddUSHORT	green;
    ddUSHORT	blue;
    ddUSHORT	pad;
} ddRgb16Colour;

typedef struct {
    ddUSHORT		index;
    ddUSHORT		pad;
} ddIndexedColour;

typedef struct {
    ddSHORT       colourType;
    ddSHORT       unused;
    union {
	ddIndexedColour        indexed;
	ddRgb8Colour           rgb8;
	ddRgb16Colour          rgb16;
	ddRgbFloatColour           rgbFloat;
	ddCieColour            cieFloat;
	ddHlsColour            hlsFloat;
	ddHsvColour            hsvFloat;
    }  colour;
} ddColourSpecifier;

typedef union {
	ddRgb8Colour           rgb8;
	ddRgb16Colour          rgb16;
	ddRgbFloatColour           rgbFloat;
	ddCieColour            cieFloat;
	ddHlsColour            hlsFloat;
	ddHsvColour            hsvFloat;
}  ddDirectColour;

typedef struct {
    ddFLOAT		ambient;
    ddFLOAT		diffuse;
    ddFLOAT		specular;
    ddFLOAT		specularConc;
    ddFLOAT		transmission;  /* 0.0 = opaque, 1.0 = transparent */
    ddColourSpecifier  specularColour;	    
} ddReflectionAttr;

/* end of PEXprotost.h equivalent structures */

/* error return values */
typedef int ddpex4rtn;
typedef int ddpex43rtn;
typedef int ddpex3rtn;
typedef int ddpex2rtn;
typedef int ddpex1rtn;

/* ddBuffer is used to pass variable length data from ddpex to dipex 
 * there is a utility called miBuffRealloc which can be used by either
 * dipex or ddpex to reallocate space in the buffer.
 * The buffer itself is just an array of bytes.  The beginning of the
 * buffer is pointed to by pHead.  ddpex copies the info into the
 * buffer beginning at the location pointed to by pBuf.  This allows
 * dipex to control where in the buffer the data is located.  dipex
 * sets pBuf, pHead and bufSize before calling the ddpex procedure.
 * ddpex copies data into the buffer starting at pBuf and puts the
 * number of bytes of data copied into dataSize.  ddpex does not change
 * pHead, pBuf, or data Size (pBuf continues pointing to the beginning
 * of the info ddpex copies to the buffer).  ddpex MUST check that the
 * buffer is large enough to hold the data (macro DD_BUF_TOO_SMALL is
 * useful for this) and should call miBufRealloc to increase the buffer
 * size if it's too small.  pHead, pBuf and bufSize will be adjusted
 * for the realloc correctly in that procedure.
 */
typedef struct {
    ddULONG	bufSize;	/* number of total bytes in buffer */
    ddULONG	dataSize;	/* number of bytes used by the new data */
    ddPointer   pBuf;		/* pointer to buffer where new data starts */
    ddPointer   pHead;		/* pointer to true head of buffer */
} ddBuffer, *ddBufferPtr;

/* lists of objects are needed in several places in the SI server.  The following
 * data structure are intended to be used for those lists. The enum type defines
 * what the possible objects in a list are.  listofObj defines the list itself.
 * The list is an array of objects.  Procedures for doing operations on these
 * lists are provided in server/dipex/utilities.  Declarations of the procedures and
 * useful macros are defined in server/include/pexUtils.h.
 * dipex and ddpex both use these lists.  The lists are not intended
 * to be opaque.  dipex and ddpex can use them directly, although we've tried
 * to use the macros and procedures as much as possible.  
 * More info is given with the procedures.
 */
typedef enum {
        DD_ELEMENT_REF=0,
        DD_HALF_SPACE=1,
        DD_PICK_PATH=2,
        DD_RENDERER=3,
        DD_WKS=4,
        DD_NS=5,
	DD_STRUCT=6,
        DD_DEVICE_RECT=7,
	DD_NAME=8,
	DD_INDEX=9,
        DD_LIST_OF_LIST=10,
	DD_NUM_TYPES=11
} ddListType;

typedef struct {
        ddListType      type;
        ddLONG     numObj;
        ddLONG     misc;
        ddLONG     maxObj;
        ddPointer  pList;
        /* pList is an array of foos (see ddListType for possible foos)
         * it is allocated as an array of footypes and reallocated as needed
         * in multiples of a defined size.
	 * footypes and array sizes are stated explicitely in
	 * server/dipex/dispatch/pexUtils.c
         * yes, this could be a union, but by using a generic char *,
         * the list can be handled generically and not have to be 
         * specified differently for each type if it isn't necessary
         */
} listofObj;

/* Pipeline Context and Renderer Resource structures: shared by dipex and ddpex */

typedef	struct {
	ddUSHORT	placementType;
	ddUSHORT	numUcurves;
	ddUSHORT	numVcurves;
} ddPSC_IsoparametricCurves;

typedef	struct {
	ddCoord3D	origin;
	ddVector3D	direction;
	ddUSHORT	numberIntersections;
	ddFLOAT 	*pPoints;
} ddPSC_LevelCurves;

typedef struct {
	ddEnumTypeIndex		type;
	union {
		char				none;
		char				impDep;
		ddPSC_IsoparametricCurves	isoCurves;	
		ddPSC_LevelCurves		mcLevelCurves;	
		ddPSC_LevelCurves		wcLevelCurves;
	} data;
} ddPSurfaceChars;

	
typedef struct {
	ddEnumTypeIndex		markerType;
	ddFLOAT			markerScale;
	ddColourSpecifier	markerColour;
	ddUSHORT		markerIndex;
	ddUSHORT		textFont;
	ddUSHORT		textPrecision;
	ddFLOAT			charExpansion;
	ddFLOAT			charSpacing;
	ddColourSpecifier	textColour;
	ddFLOAT			charHeight;
	ddVector2D		charUp;
	ddUSHORT		textPath;
	ddTextAlignmentData	textAlignment;
	ddFLOAT			atextHeight;
	ddVector2D		atextUp;
	ddUSHORT		atextPath;
	ddTextAlignmentData	atextAlignment;
	ddEnumTypeIndex		atextStyle;
	ddUSHORT		textIndex;
	ddEnumTypeIndex		lineType;
	ddFLOAT			lineWidth;
	ddColourSpecifier	lineColour;
	ddCurveApprox		curveApprox;
	ddEnumTypeIndex		lineInterp;
	ddUSHORT		lineIndex;
	ddEnumTypeIndex		intStyle;
	ddSHORT			intStyleIndex;
	ddColourSpecifier	surfaceColour;
	ddReflectionAttr	reflAttr;
	ddEnumTypeIndex		reflModel;
	ddEnumTypeIndex		surfInterp;
	ddEnumTypeIndex		bfIntStyle;
	ddSHORT			bfIntStyleIndex;
	ddColourSpecifier	bfSurfColour;
	ddReflectionAttr	bfReflAttr;
	ddEnumTypeIndex		bfReflModel;
	ddEnumTypeIndex		bfSurfInterp;
	ddSurfaceApprox		surfApprox;
	ddUSHORT		cullMode;
	ddBOOL			distFlag;
	ddCoord2D		patternSize;
	ddCoord3D		patternRefPt;
	ddVector3D		patternRefV1;
	ddVector3D		patternRefV2;
	ddUSHORT		intIndex;
	ddUSHORT		edges;
	ddEnumTypeIndex		edgeType;
	ddFLOAT			edgeWidth;
	ddColourSpecifier	edgeColour;
	ddUSHORT		edgeIndex;
	ddFLOAT			localMat[4][4];
	ddFLOAT			globalMat[4][4];
	ddUSHORT		modelClip;
	listofObj		*modelClipVolume;
	ddUSHORT		viewIndex;
	listofObj		*lightState;
	ddUSHORT		depthCueIndex;
	ddUSHORT		colourApproxIndex;
	ddSHORT			rdrColourModel;
	ddPSurfaceChars		psc;
	ddULONG			asfs;
	ddULONG			pickId;
	ddULONG			hlhsrType;
	diNSHandle		pCurrentNS;	/* handle to name set */
} ddPCAttr;

typedef struct {
	ddULONG			PCid;
	listofObj		*rendRefs;
	ddPCAttr		*pPCAttr;
} ddPCStr, *ddPCPtr;

/*	Output Command Procedure Vector	used in renderer */
/*	The index of a procedure is the output command number	*/
#define OCTABLE_LENGTH (PEXMaxOC+1)
#define SEPROC_VECTOR_LENGTH (PEXMaxOC+1)

typedef	ddpex2rtn	(*ocTableType)();

typedef struct {
	ddBYTE		type;	/* drawable type: pixmap, window, undrawable_window? */
	ddBYTE		class;	
	ddBYTE		depth;	
	ddBYTE		bitsPerPixel;	
	ddBYTE		rootDepth;
	ddULONG		rootVisual;
} ddDrawableInfo;

typedef enum {
	DD_HIGH_INCL_NS=0,
	DD_HIGH_EXCL_NS=1,
	DD_INVIS_INCL_NS=2,
	DD_INVIS_EXCL_NS=3,
	DD_PICK_INCL_NS=4,
	DD_PICK_EXCL_NS=5,
	DD_MAX_FILTERS=6
} ddNSFilters; 

/* pick state for Renderer picking */
#define DD_PICK_ONE 1
#define DD_PICK_ALL 2
#define DD_SERVER	1
#define DD_CLIENT	2
#define DD_NEITHER	3

typedef struct {
	ddUSHORT	  state;        /* pick state one or all */
	ddUSHORT	  server;       /* client or server traversal */
	ddSHORT           pick_method;
	ddBOOL            send_event;
	ddULONG           max_hits;
	ddULONG           more_hits;
	ClientPtr         client;       /* need to send the event */
        diStructHandle    strHandle;	/* struct handle for PickOne */
	diPMHandle	  pseudoPM;      /* fake PM for Renderer Pick */
	listofObj	  *list;	/* list of list for pick all */
	listofObj	  *fakeStrlist;	/* list of fake struct handle for
					   picking */
	listofObj	  *sIDlist;	/* list of IDs, struct handles  and
					   offsets for reverse mapping
					   BeginStructures when picking */
} ddRdrPickStr, *ddRdrPickPtr;       /* need to send the event */ 

typedef struct {
	ddULONG 		rendId;		/* renderer id */
	ddPCPtr			pPC;		/* pipeline context handle */
	ddDrawableInfo		drawExample;	/* info from drawable example */
	DrawablePtr		pDrawable;	/* rendering drawable */
	ddULONG			drawableId;	/* id of rendering drawable */
	listofObj		*curPath;	/* current path */
	ddUSHORT		state;		/* renderer state */
	diLUTHandle		lut[PEXMaxTableType+1];/* lookup table handles */
	diNSHandle		ns[DD_MAX_FILTERS];/* name set handles */
	ddSHORT			hlhsrMode;	/* you guessed it */
	ddNpcSubvolume		npcSubvolume;
	ddViewport		viewport;
	listofObj		*clipList;	 /* clip list */
						 /* Begin 5.1 additions */
						 /* pick_inclusion is in ns */
						 /* pick_exclusion is in ns */
	listofObj		*pickStartPath;	 /* pick start path */
	ddColourSpecifier	backgroundColour;
	ddBOOL			clearI;
	ddBOOL			clearZ;
	ddUSHORT		echoMode;
	ddColourSpecifier	echoColour;
						 /* End 5.1 additions */
	ddBitmask		tablesMask;	/* renderer dynamics */
	ddBitmask		namesetsMask;	/* renderer dynamics */
	ddBitmask		attrsMask;	/* renderer dynamics */
	ddBitmask		tablesChanges;	/* changed attributes */
	ddBitmask		namesetsChanges;/* changed attributes */
	ddBitmask		attrsChanges;	/* changed attributes */
	ocTableType		executeOCs[OCTABLE_LENGTH];	
	ddBOOL			immediateMode;
	ddUSHORT		render_mode;
	ddPointer		pDDContext;	/* device dependent attribute context */
	ddRdrPickStr		pickstr;
} ddRendererStr, *ddRendererPtr;

typedef struct {
	ddULONG			numElRefs;	/* number of element refs */
	listofObj		*Path;		/* path */
} ddAccStStr, *ddAccStPtr;

/* render_mode values */
#define	MI_REND_DRAWING 0
#define	MI_REND_PICKING 1
#define	MI_REND_SEARCHING 2

	/* enum type for specifying resources  */
typedef enum { 
	WORKSTATION_RESOURCE=0,
	STRUCTURE_RESOURCE=1,
	PARENT_STRUCTURE_RESOURCE=2,
	CHILD_STRUCTURE_RESOURCE=3,
	SEARCH_CONTEXT_RESOURCE=4,
	PICK_RESOURCE=5,
	LOOKUP_TABLE_RESOURCE=6,
	NAME_SET_RESOURCE=7,
	FONT_RESOURCE=8,
	RENDERER_RESOURCE=9,
	PIPELINE_CONTEXT_RESOURCE=10
} ddResourceType;

typedef enum {
	ADD=0,
	REMOVE=1
} ddAction;

typedef enum {
	X_WINDOW_RESOURCE=0,
	X_DRAWABLE_RESOURCE=1,
	X_FONT_RESOURCE=2,
	X_PIXMAP_RESOURCE=3,
	X_CURSOR_RESOURCE=4,
	X_COLORMAP_RESOURCE=5,
	X_GCONTEXT_RESOURCE=6,
	X_KEYSYM=7
} ddXResourceType;

#endif  /* DDPEX_H */
