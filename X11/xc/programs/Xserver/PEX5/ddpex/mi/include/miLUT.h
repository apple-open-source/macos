/* $Xorg: miLUT.h,v 1.4 2001/02/09 02:04:08 xorgcvs Exp $ */

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


#ifndef MILUT_H
#define MILUT_H

#include "mipex.h"

/****** devPriv data structures ******/
/* pex-si uses data which looks like pex protocol format */

typedef struct {
    ddEnumTypeIndex	lineType;
    ddEnumTypeIndex	polylineInterp;
    ddCurveApprox	curveApprox;
    ddFLOAT		lineWidth;	/* this is really a scale */
    ddColourSpecifier	lineColour;	    
} ddLineBundleEntry;

typedef struct {
    ddEnumTypeIndex	markerType;
    ddSHORT		unused;
    ddFLOAT		markerScale;	/* this really is a scale */
    ddColourSpecifier	markerColour;	    
} ddMarkerBundleEntry;

typedef struct {
    ddUSHORT		textFontIndex;
    ddUSHORT		textPrecision;
    ddFLOAT		charExpansion;
    ddFLOAT		charSpacing;
    ddColourSpecifier	textColour;	    
} ddTextBundleEntry;

typedef struct {
    ddEnumTypeIndex    interiorStyle;
    ddSHORT	    interiorStyleIndex;
    ddEnumTypeIndex    reflectionModel;
    ddEnumTypeIndex    surfaceInterp;
    ddEnumTypeIndex    bfInteriorStyle;
    ddSHORT	    bfInteriorStyleIndex;
    ddEnumTypeIndex    bfReflectionModel;
    ddEnumTypeIndex    bfSurfaceInterp;
    ddSurfaceApprox	surfaceApprox;
    ddColourSpecifier	surfaceColour; 
    ddReflectionAttr	reflectionAttr; 
    ddColourSpecifier	bfSurfaceColour;
    ddReflectionAttr	bfReflectionAttr;
} ddInteriorBundleEntry;

typedef struct {
    ddSwitch		edges;
    ddUCHAR		unused;
    ddEnumTypeIndex	edgeType;
    ddFLOAT		edgeWidth;	/* this is really a scale */
    ddColourSpecifier	edgeColour;	    
} ddEdgeBundleEntry;

typedef struct {
    ddSHORT		colourType; 
    ddUSHORT		numx;
    ddUSHORT		numy;
    ddUSHORT		unused;
    /* LISTof Colour(numx, numy) 2D array of colours */
    union {
	ddIndexedColour	*indexed;
	ddRgb8Colour	*rgb8;
	ddRgb16Colour	*rgb16;
	ddRgbFloatColour	*rgbFloat;
	ddHsvColour	*hsvFloat;
	ddHlsColour	*hlsFloat;
	ddCieColour	*cieFloat;
   }			colours;
} ddPatternEntry;

/* a ddColourEntry is just ddColourSpecifier */

#define MILUT_MAX_CS_PER_ENTRY    16
/* Much easier to code if this is a fixed size entry, thus we'll have it
 * max out at MAX_CS_PER_ENTRY charsets per entry, MAX_CS_PER_ENTRY character 
 * sets per font group seems plenty reasonable. */
 
typedef struct {
    ddULONG		numFonts;
    diFontHandle        fonts[MILUT_MAX_CS_PER_ENTRY];    /* list of fonts */
} ddTextFontEntry;

/* a  ddViewEntry is defined in ddpex.h */

typedef struct {
    ddEnumTypeIndex	lightType;
    ddSHORT		unused;
    ddVector3D		direction;
    ddCoord3D		point;
    ddFLOAT		concentration;
    ddFLOAT		spreadAngle;
    ddFLOAT		attenuation1;
    ddFLOAT		attenuation2;
    ddColourSpecifier	lightColour;	    
} ddLightEntry;

typedef struct {
    ddUCHAR		mode;
    ddUCHAR		unused;
    ddUSHORT		unused2;
    ddFLOAT		frontPlane;
    ddFLOAT		backPlane;
    ddFLOAT		frontScaling;
    ddFLOAT		backScaling;
    ddColourSpecifier	depthCueColour;	    
} ddDepthCueEntry;

typedef struct {
    ddEnumTypeIndex	approxType;
    ddEnumTypeIndex	approxModel;
    ddUSHORT	max1;
    ddUSHORT	max2;
    ddUSHORT	max3;
    ddUCHAR	dither;
    ddUCHAR	unused;
    ddULONG	mult1;
    ddULONG	mult2;
    ddULONG	mult3;
    ddFLOAT	weight1;
    ddFLOAT	weight2;
    ddFLOAT	weight3;
    ddULONG	basePixel;
} ddColourApproxEntry;

/* table entry definitions */
/* some definitions contain a set and realized entry. this happens when
 * realized data can differ from set data. 
 */

/* status values  */
#define	MILUT_UNDEFINED	0
#define MILUT_PREDEFINED	1
#define MILUT_DEFINED	2

/* the device independent information for all table entries */
typedef struct {
        ddUSHORT                status;
	ddTableIndex            index;
} miTableEntry;

typedef struct _miLineBundleEntry {
	miTableEntry		entry_info;
/* device dependent data */
	ddLineBundleEntry	entry;		/* set entry */
	ddLineBundleEntry	real_entry;	/* realized entry */
} miLineBundleEntry;

typedef struct _miMarkerBundleEntry {
	miTableEntry		entry_info;
/* device dependent data */
	ddMarkerBundleEntry	entry;		/* set entry */
	ddMarkerBundleEntry	real_entry;	/* realized entry */
} miMarkerBundleEntry;

typedef struct _miTextBundleEntry {
	miTableEntry		entry_info;
/* device dependent data */
	ddTextBundleEntry	entry;		/* set entry */
	ddTextBundleEntry	real_entry;	/* realized entry */
} miTextBundleEntry;

typedef struct _miInteriorBundleEntry {
	miTableEntry		entry_info;
/* device dependent data */
	ddInteriorBundleEntry	entry;		/* set entry */
	ddInteriorBundleEntry	real_entry;	/* realized entry */
} miInteriorBundleEntry;

typedef struct _miEdgeBundleEntry {
	miTableEntry		entry_info;
/* device dependent data */
	ddEdgeBundleEntry	entry;		/* set entry */
	ddEdgeBundleEntry	real_entry;	/* realized entry */
} miEdgeBundleEntry;

typedef struct _miPatternEntry {
	miTableEntry		entry_info;
/* device dependent data */
	ddPatternEntry		entry;		/* set entry  = realized entry */
} miPatternEntry;

/* each entry in the font table is a list of PEX or X fonts. */
typedef struct _miTextFontEntry {
	miTableEntry		entry_info;
/* device dependent data */
	ddTextFontEntry		entry;		/* set entry = realized entry */
} miTextFontEntry;

typedef struct _miColourEntry {
	miTableEntry		entry_info;
/* device dependent data */
	ddColourSpecifier	entry;		/* set entry = realized entry */
} miColourEntry;

typedef struct _miViewEntry {
	miTableEntry		entry_info;
/* device dependent data */
	ddViewEntry		entry;		/* set entry = realized entry */
	ddFLOAT			vom[4][4];	/* concated mats */
	ddFLOAT			vom_inv[4][4];	/* inverse of vom */
	ddBOOL			inv_flag;	/* is vom_inv current */
} miViewEntry;

typedef struct _miLightEntry {
	miTableEntry		entry_info;
/* device dependent data */
	ddLightEntry		entry;		/* set entry = realized entry */
    	double			cosSpreadAngle;	/* cosine */
} miLightEntry;

typedef struct _miDepthCueEntry {
	miTableEntry		entry_info;
/* device dependent data */
	ddDepthCueEntry		entry;		/* set entry = realized entry */
} miDepthCueEntry;

typedef struct _miColourApproxEntry {
	miTableEntry		entry_info;
/* device dependent data */
	ddColourApproxEntry	entry;		/* set entry = realized entry */
} miColourApproxEntry;

typedef	ddpex43rtn		(*miOpsTableType)();

typedef struct _miLUTHeader {
	/* the resource id is in the dipex resource structure */
	/* the lut type is also in the dipex resource structure */
	ddDrawableInfo		drawExample;
	ddSHORT			drawType;
	/* there are macros defined in this file to get at these: */
	ddUSHORT		startIndex;
	ddUSHORT		defaultIndex;
	ddUSHORT		numDefined;
	ddTableInfo		tableInfo;
	/**/
	listofObj		*wksRefList;
	listofObj		*rendRefList;
	ddBOOL			freeFlag;
/* the lut entries are just an array of entries. this works OK
 * for small tables. if you want a large table, you'll probably
 * want to change this scheme to use a hash table into the array
 * or maybe n-ary trees to improve performance of searching for
 * entries (see macros below).  PEX-SI does linear search
 */
	union {		
		miLineBundleEntry	*line;
		miMarkerBundleEntry	*marker;
		miTextBundleEntry	*text;
		miInteriorBundleEntry	*interior;
		miEdgeBundleEntry	*edge;
		miPatternEntry		*pattern;
		miTextFontEntry		*font;
		miColourEntry		*colour;
		miViewEntry		*view;
		miLightEntry		*light;
		miDepthCueEntry		*depthCue;
		miColourApproxEntry 	*colourApprox;
	 } plut;
/* Table of operation procs for luts. One for each lut request & some
 * internal ones.
 * Individual procedures can be loaded depending on the lut type.
 * These procs are defined in the files mi<type>LUT.c.
 * diPEX always calls 'general' procedures (defined in miLUT.c)
 * directly. Those procedures then call the individual procs through
 * this table as needed.  Not all requests need to be handled 
 * differently for each table type. For those requests, the
 * general procedure does all of the work and there is no individual
 * procedure. The general procedure is loaded into the table for
 * that op.
 */
#define	MILUT_MIN_REQUEST	PEX_CreateLookupTable
#define	MILUT_MAX_REQUEST	PEX_DeleteTableEntries
#define	MILUT_IMPDEP_REQUESTS	6
#define	MILUT_NUM_REQUESTS	(MILUT_MAX_REQUEST - MILUT_MIN_REQUEST + 1 + MILUT_IMPDEP_REQUESTS)
/* map pex request opcode to index into op table */
#define MILUT_REQUEST_OP(req)	((req) - MILUT_MIN_REQUEST)
/* imp dep opcodes to use to map into op table */
#define milut_InquireEntryAddress	(MILUT_MAX_REQUEST + 1)
#define milut_entry_check	(milut_InquireEntryAddress + 1)
#define milut_copy_pex_to_mi	(milut_entry_check + 1)
#define milut_copy_mi_to_pex	(milut_copy_pex_to_mi + 1)
#define milut_realize_entry	(milut_copy_mi_to_pex + 1)
#define milut_mod_call_back	(milut_realize_entry + 1)

	miOpsTableType		ops[MILUT_NUM_REQUESTS];
} miLUTHeader;

#define MILUT_HEADER(handle)			\
	((miLUTHeader *)(handle)->deviceData)

#define	MILUT_DEFINE_HEADER(handle, phead)		\
	miLUTHeader	*(phead) = MILUT_HEADER(handle)

#define MILUT_DESTROY_HEADER( phead )                  \
    puDeleteList( (phead)->wksRefList );        \
    puDeleteList( (phead)->rendRefList );       \
    xfree((phead)->plut.line);               \
    xfree((phead))

#define MILUT_CHECK_DESTROY( handle, phead )             \
    if ( (phead)->freeFlag &&                   \
        !(phead)->wksRefList->numObj &&         \
                !(phead)->rendRefList->numObj ) \
    {                                           \
        MILUT_DESTROY_HEADER( phead );                 \
        xfree(handle);                \
    }
	
#define	MILUT_MAX_INDEX	65535

#define	MILUT_TYPE(handle)	(handle->lutType)

#define MILUT_START_INDEX( pheader ) \
                (pheader)->startIndex

/* max number of definable entries */
#define MILUT_DEF_ENTS( pheader ) \
                (pheader)->tableInfo.definableEntries

#define MILUT_ALLOC_ENTS( pheader )		\
	(MILUT_DEF_ENTS(pheader))

/* number of predefined entries */
#define MILUT_PRENUM( pheader ) \
                (pheader)->tableInfo.numPredefined

/* index of first predefined entry */
#define MILUT_PREMIN( pheader ) \
                (pheader)->tableInfo.predefinedMin

/* index of last predefined entry: premax = premin + prenum - 1 */
#define MILUT_PREMAX( pheader ) \
                (pheader)->tableInfo.predefinedMax

/* number of defined entries (includes predefined ones) */
#define MILUT_NUM_ENTS( pheader ) \
                (pheader)->numDefined

/* index of default entry */
#define MILUT_DEFAULT_INDEX( pheader ) \
                (pheader)->defaultIndex

#define MILUT_COPY_COLOUR(Colour, Buf, Type)                  \
        mibcopy((Colour), (Buf), colour_type_sizes[(int) (Type)])

/* set status of entries
 * i must be declared when using this 
 * and pentry must be pointing to the first entry to set status
 * if the flag is TRUE, the entry's index is also set
 * The flag is used to 'set up' the table to be used with a full table
 * of contiguous indices (as apposed to a sparse table where entries
 * can have any index value) most efficiently. Sparse entries will
 * work, but continguous entries are used more often (I know - sparse
 * entries haven't worked for almost 2 years and no-one has complained)
 */
#define MILUT_SET_STATUS( pentry, num_ents, stat, flag )	\
                for ( i=0;  i<(num_ents); i++, (pentry)++ ) {  	\
                        (pentry)->entry_info.status = (stat);		\
                        if (flag)	(pentry)->entry_info.index = i; }

/* return pointer to entry with index 'index'. if that entry
 * is not defined, return NULL
 * pentry must be initially pointing to entry where search is to
 * begin 
 * plast must be pointing to the last definable entry in the lut
 * This does a linear search. Change this to a more efficient
 * method for large tables
 */
#define	MILUT_GET_ENTRY(ind, pentry, plast)				\
	while ( ((pentry) < (plast)) && ((ind) != (pentry)->entry_info.index) )	\
		(pentry)++;						\
	if ((pentry) == (plast))	pentry = NULL;			\
	else if ((ind) != (pentry)->entry_info.index)	pentry = NULL

#define MILUT_INIT_COLOUR(Colour) \
    (Colour).colourType = PEXIndexedColour; \
    (Colour).colour.indexed.index = 1

/* modification types passed to call back procedure */
#define	MILUT_COPY_MOD	0
#define	MILUT_SET_MOD	1
#define	MILUT_DEL_MOD	2

#endif /* MILUT_H */

