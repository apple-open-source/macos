
/* $Xorg: miRender.h,v 1.4 2001/02/09 02:04:08 xorgcvs Exp $ */

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

#ifndef MI_RENDER_H
#define MI_RENDER_H

#include "ddpex.h"
#include "ddpex3.h"
#include "ddpex4.h"
#include "gc.h"
#include "miNS.h"
#include "PEXprotost.h"
#include "miLUT.h"

/*
 * Create a data list header. THis list header is simply
 * used as a pointer to the start of the scratch data areas
 * maintained for transformation and clipping.
 */
typedef struct _miListHeader {
	ddPointType	type;		/* type of vertices in lists */
	ddUSHORT	flags;		/* various random path flags */
	ddULONG		numLists;	/* number of list headers */
	ddULONG		maxLists;	/* allocated number of list headers */
	listofddPoint	*ddList;	/* data area pointer */
} miListHeader;

/*
 * The DD context is divided into two parts: values that remain
 * static across structure and values that can vary within structures.
 * Only the dynamic values (those in miDynamicDDContext) must be stored
 * across structure references - such as in BeginStructure
 */
typedef struct _miDynamicDDContext {
	ddPCAttr	*pPCAttr;         /* Pipeline context for renderer */
	ddNamePiece	currentNames[MINS_NAMESET_WORD_COUNT];
	struct _miDynamicDDContext *next; /* Next pointer for stacking */
	ddFLOAT		mc_to_wc_xform[4][4];
	ddFLOAT		wc_to_npc_xform[4][4];
	ddFLOAT		mc_to_npc_xform[4][4];
	ddFLOAT		wc_to_cc_xform[4][4];
	ddFLOAT		cc_to_dc_xform[4][4];
	ddFLOAT		mc_to_cc_xform[4][4];
	ddFLOAT		mc_to_dc_xform[4][4];
	ddFLOAT		npc_to_cc_xform[4][4];
	ddUSHORT	clipFlags;
	ddUSHORT	filter_flags;
	ddUSHORT	do_prims;
} miDynamicDDContext;

/* definitions for filter_flags */
#define	MI_DDC_HIGHLIGHT_FLAG		1<<0
#define	MI_DDC_INVISIBLE_FLAG		1<<1
#define	MI_DDC_DETECTABLE_FLAG		1<<2

#define MI_DDC_DO_PRIMS(pRend)				\
	((miDDContext *)(pRend)->pDDContext)->Dynamic->do_prims

#define	MI_DDC_SET_DO_PRIMS(pRend, pddc)					\
	(pddc)->Dynamic->do_prims = 						\
		!((pddc)->Dynamic->filter_flags & MI_DDC_INVISIBLE_FLAG)	\
		&& ( !(pRend)->render_mode ||					\
		     ((pRend)->render_mode &&					\
		      ((pddc)->Dynamic->filter_flags & MI_DDC_DETECTABLE_FLAG)) )

#define	MI_DDC_IS_HIGHLIGHT(pddc)		\
	((pddc)->Dynamic->filter_flags & MI_DDC_HIGHLIGHT_FLAG)

#define	MI_DDC_SET_HIGHLIGHT(pddc)		\
	(pddc)->Dynamic->filter_flags  |= MI_DDC_HIGHLIGHT_FLAG

#define	MI_DDC_CLEAR_HIGHLIGHT(pddc)		\
	(pddc)->Dynamic->filter_flags  &= ~MI_DDC_HIGHLIGHT_FLAG

#define	MI_DDC_IS_INVISIBLE(pddc)		\
	((pddc)->Dynamic->filter_flags & MI_DDC_INVISIBLE_FLAG)

#define	MI_DDC_SET_INVISIBLE(pddc)		\
	(pddc)->Dynamic->filter_flags  |= MI_DDC_INVISIBLE_FLAG

#define	MI_DDC_CLEAR_INVISIBLE(pddc)		\
	(pddc)->Dynamic->filter_flags  &= ~MI_DDC_INVISIBLE_FLAG

#define	MI_DDC_IS_DETECTABLE(pddc)		\
	((pddc)->Dynamic->filter_flags & MI_DDC_DETECTABLE_FLAG)

#define	MI_DDC_SET_DETECTABLE(pddc)		\
	(pddc)->Dynamic->filter_flags  |= MI_DDC_DETECTABLE_FLAG

#define	MI_DDC_CLEAR_DETECTABLE(pddc)		\
	(pddc)->Dynamic->filter_flags  &= ~MI_DDC_DETECTABLE_FLAG

/*
 * The static portion of the DD context is itself divided into several parts: 
 * immediate rendering attributes from the pipeline context, a jump
 * table for the level 1 rendering routines, and other assorted intermediate 
 * values that are used during rendering.
 * Note that immediate rendering attributes means that bundled attributes
 * have been extracted from the appropriate LUTs. Code that uses the
 * DDC rendering attributes, therefore, do not need to check the ASF's.
 */

/* 
 *Level 1 rendering Procedure Vector used in DDC 
 * The index of a procedure is arbitrarily specified
 * the following defines.
 */
#define POLYLINE_RENDER_TABLE_INDEX     0
#define FILLAREA_RENDER_TABLE_INDEX     1
#define TEXT_RENDER_TABLE_INDEX         2
#define MARKER_RENDER_TABLE_INDEX       3
#define TRISTRIP_RENDER_TABLE_INDEX     4
#define RENDER_TABLE_LENGTH             5

typedef ddpex2rtn       (*RendTableType)();

/*
 * This structure is a copy of the attributes in the PipeLine context
 * with the exception that all references to bundles and bundled
 * attributes have been removed. Furthermore, this list contains
 * references to no attributes that are no bundled: they are
 * just as easily accessed from the dynamic copy of the PC. 
 */
typedef struct _miDDContextRendAttrs {
	ddEnumTypeIndex		markerType;
	ddFLOAT			markerScale;
	ddColourSpecifier	markerColour;
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
	ddEnumTypeIndex		lineType;
	ddFLOAT			lineWidth;
	ddColourSpecifier	lineColour;
	ddCurveApprox		curveApprox;
	ddEnumTypeIndex		lineInterp;
	ddEnumTypeIndex		intStyle;
	ddColourSpecifier	surfaceColour;
	ddReflectionAttr	reflAttr;
	ddEnumTypeIndex		reflModel;
	ddEnumTypeIndex		surfInterp;
	ddEnumTypeIndex		bfIntStyle;
	ddColourSpecifier	bfSurfColour;
	ddReflectionAttr	bfReflAttr;
	ddEnumTypeIndex		bfReflModel;
	ddEnumTypeIndex		bfSurfInterp;
	ddSurfaceApprox		surfApprox;
	ddCoord2D		patternSize;
	ddCoord3D		patternRefPt;
	ddVector3D		patternRefV1;
	ddVector3D		patternRefV2;
	ddUSHORT		edges;
	ddEnumTypeIndex		edgeType;
	ddFLOAT			edgeWidth;
	ddColourSpecifier	edgeColour;
	ddUSHORT		modelClip;
	ddULONG			pickId;
	ddULONG			hlhsrType;
	ddColourSpecifier       backgroundColour;
	ddUSHORT                clearI;
	ddUSHORT                clearZ;
	ddUSHORT                echoMode;
	ddColourSpecifier       echoColour;
} miDDContextRendAttrs;

#define MI_MAXTEMPDATALISTS 4 /* Note, must be 2^n for macros to work */
#define MI_MAXTEMPFACETLISTS 4 /* Note, must be 2^n for macros to work */

/*
 * the following defines are bit mask flags for the flags field
 * in the miDDContextMisc struct.
 *
 * Note that all these flag are true when the corresponding entry
 * is INVALID. ie the flag is set if the entry must be updated!
 */

#define POLYLINEGCFLAG	(1<<0)	/* change flag for polyline GC */
#define FILLAREAGCFLAG	(1<<1)	/* change flag for fill area GC */
#define EDGEGCFLAG	(1<<2)	/* change flag for F.A. edge GC */
#define MARKERGCFLAG	(1<<3)	/* change flag for marker GC */
#define TEXTGCFLAG	(1<<4)	/* change flag for text GC */
#define NOLINEDASHFLAG	(1<<5)	/* No Line Dash storage allocated in line GC */
#define CC_DCUEVERSION	(1<<6)	/* invalid cc version of depth cue entry */
#define MCVOLUMEFLAG	(1<<7)	/* invalid model clip planes entry */

#define INVTRMCTOWCXFRMFLAG	(1<<8)	/* invalid flag - inverse mc2wc xform */
#define INVTRWCTOCCXFRMFLAG	(1<<9)	/* invalid flag - inverse wc2cc xform */
#define INVTRMCTOCCXFRMFLAG	(1<<10)	/* invalid flag - inverse mc2cc xform */
#define INVTRCCTODCXFRMFLAG	(1<<11)	/* invalid flag - inverse cc2dc xform */
#define INVVIEWXFRMFLAG		(1<<12)	/* invalid flag - inverse view xform */

typedef struct _miDDContextMisc {
	ddULONG		listIndex;	/* index into following array */
	miListHeader	list4D[MI_MAXTEMPDATALISTS];	/* temp data areas */
	miListHeader	list2D;		/* temp area for 2D data */
	ddULONG		facetIndex;	/* index into following array */
	listofddFacet	facets[MI_MAXTEMPFACETLISTS];	/* temp facet areas */
	ddFLOAT		viewport_xform[4][4]; /* from npc to viewport */
	ddULONG		flags;		/* valid flags for following fields */
	GCPtr		pPolylineGC;	/* ddx GC for rendering polylines */
	GCPtr		pFillAreaGC;	/* ddx GC for rendering fill areas */
	GCPtr		pEdgeGC;	/* ddx GC for rendering F. A. edges */
	GCPtr		pPolyMarkerGC;	/* ddx GC for rendering poly markers*/
	GCPtr		pTextGC;	/* ddx GC for rendering Text */
	ddFLOAT		inv_tr_mc_to_wc_xform[4][4]; /* inverse transpose  */
	ddFLOAT		inv_tr_wc_to_cc_xform[4][4]; /* for normal xforms */
	ddFLOAT		inv_tr_mc_to_cc_xform[4][4];
	ddFLOAT		inv_tr_cc_to_dc_xform[4][4];
	ddFLOAT		inv_vpt_xform[4][4];  /* from viewport to NPC */
	ddFLOAT		inv_view_xform[4][4];  /* from npc to WC */
        listofObj	*ms_MCV;      	/* modelling space version of the */
                                        /* model clipping volume. */
	ddCoord4D	eye_pt;		/* eye point in WC */
	ddColourSpecifier	highlight_colour;
	ddDepthCueEntry cc_dcue_entry; /* cc version of current 
					 * depth cue bundle entry */
} miDDContextMisc;

typedef struct {
	ddEnumTypeIndex	type;		/* DC_HitBox or NPC_HitVolume */
	ddUSHORT	status;		/* PEXOk or PEXNoPick */
	ddNamePiece	inclusion[MINS_NAMESET_WORD_COUNT];
	ddNamePiece	exclusion[MINS_NAMESET_WORD_COUNT];
	union {
		char		dc_data_rec;	/* none */
		char		npc_data_rec;	/* none */
	} data_rec;			/* place holder */
	union {
		ddPC_DC_HitBox		dc_hit_box;
		ddPC_NPC_HitVolume	npc_hit_volume;
	} input_rec;
	ddPointer	devPriv;
} ddPickDeviceStr;

typedef struct _ddSearchDevPriv {
	ddUSHORT	status;		/* PEXFound or PEXNotFound */
	ddCoord3D	position;
	ddFLOAT		distance;
	ddBOOL		modelClipFlag;
	ddNamePiece	norm_inclusion[MINS_NAMESET_WORD_COUNT];
	ddNamePiece	norm_exclusion[MINS_NAMESET_WORD_COUNT];
	ddNamePiece	invert_inclusion[MINS_NAMESET_WORD_COUNT];
	ddNamePiece	invert_exclusion[MINS_NAMESET_WORD_COUNT];
	ddPointer	devPriv;
} ddSearchDeviceStr;

typedef struct _miStaticDDContext {
   miDDContextRendAttrs	*attrs;	/* Immediate rendering attributes */
   miDDContextMisc	misc;	/* misc. rendering temp vals */
   RendTableType	RenderProcs[RENDER_TABLE_LENGTH]; /* lvl1 jmp table */
   ddPickDeviceStr	pick;
   ddSearchDeviceStr	search;
} miStaticDDContext;

/*
 * Finally! the ddContext itself....
 */
typedef struct _miDDContext {
	miStaticDDContext	Static;
	miDynamicDDContext	*Dynamic;
} miDDContext;

/*
 * the following macro return a pointer to the next free
 * work data area from the list4D array in the static portion
 * of the ddContext.
 */
#define MI_NEXTTEMPDATALIST(pddc)					   \
&((pddc)->Static.misc.list4D[(++(pddc)->Static.misc.listIndex)&(MI_MAXTEMPDATALISTS-1)])

/*
 * the following macro return a pointer to the next free
 * work facet area from the facets array in the static portion
 * of the ddContext.
 */
#define MI_NEXTTEMPFACETLIST(pddc)					   \
&((pddc)->Static.misc.facets[(++(pddc)->Static.misc.facetIndex)&(MI_MAXTEMPFACETLISTS-1)])

#define NULL4x4 (float (*)[4])0
extern ddFLOAT ident4x4[4][4];

#endif	/* MI_RENDER_H */
