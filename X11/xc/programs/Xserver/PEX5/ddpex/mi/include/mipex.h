/* $Xorg: mipex.h,v 1.4 2001/02/09 02:04:08 xorgcvs Exp $ */

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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/include/mipex.h,v 3.3 2001/12/14 19:57:13 dawes Exp $ */

#ifndef MI_H
#define MI_H

#include "ddpex.h"
#include "X.h"
#include "scrnintstr.h"

#define MI_TRUE 1
#define MI_FALSE 0

#define MI_MAXUSHORT 65535	/* max value for unsigned short int: 2**16 */
#define MI_PI	3.14159265358979323846	/* from values.h */

#define MI_FIRSTTABLETYPE 1  /* a useful constant that's not in PEX.h */

#define MI_BACKGROUND 0
#define MI_FOREGROUND 1

/* high bit mask for proprietary OCs */
#define MI_OC_HIGHBIT 0x8000

/* a redefinable location for use in branching on proprietary OCs */
#define MI_OC_PROP 0

/* see if propietary bit is set in OC Type */
#define MI_HIGHBIT_ON(octype)   ((octype) & MI_OC_HIGHBIT)

/* see if OC Type (or element) is in range of PEX OCs */
#define	MI_IS_PEX_OC(octype)  (((octype) > PEXOCAll) && ((octype) <= PEXMaxOC))

/**  redefine ASSURE even if it's already defined **/
#ifdef ASSURE
#undef ASSURE
#endif


typedef void	(*destroyTableType)();


#ifdef DDTEST
#define ASSURE(test) \
    if (!(test)) {  \
        ErrorF( "test \n"); \
        ErrorF( "Failed: Line %d, File %s\n\n", __LINE__, __FILE__); \
    }
#else
#define ASSURE(test)
#endif /* DDTEST */

/* the WHICDRAW macro looks at the given drawable and determines which
 * of the suported drawable types it matches
 * WHICHDRAW should compare the data in the given drawable with the
 * drawable types defined here to define a impe dep drawable type id
 * For now, only one drawable type is supported and it
 * handles all drawables.  
 */
#define MI_MAXDRAWABLES	1
/*			type	depth	rootDepth	rootVisual */
#define MI_DRAWABLE0 0/*ANY	ANY	ANY		ANY	*/

#define MI_SETDRAWEXAMPLE( pdraw, peg ) \
	(peg)->type = (pdraw)->type; \
	(peg)->class = (pdraw)->class; \
	(peg)->depth = (pdraw)->depth; \
	(peg)->bitsPerPixel = (pdraw)->bitsPerPixel; \
	(peg)->rootDepth = (pdraw)->pScreen->rootDepth; \
	(peg)->rootVisual = (pdraw)->pScreen->rootVisual

#define MI_WHICHDRAW( pdraw, type ) \
		type = MI_DRAWABLE0

/* dd internal error codes */
#define MI_SUCCESS 0
#define MI_ALLOCERR 1
#define MI_EXISTERR 2

#define MI_DEFAULT_COLOUR_FORMAT	PEXRgbFloatColour

/* only do indexed & rgb floats now */
#define MI_BADCOLOURTYPE( format ) \
		( (format!=PEXIndexedColour) && \
		 (format!=PEXRgbFloatColour) ) /* && \
		  (format!=PEXCieFloatColour) && \
		  (format!=PEXHsvFloatColour) && \
		  (format!=PEXHlsFloatColour) && \
		  (format!=PEXRgbInt8Colour) && \
		  (format!=PEXRgbInt16Colour) )
		 */

/* JSH - assuming copy may overlap */
#define mibcopy(pfrom, pto, size)		\
	memmove( (char *)(pto), (char *)(pfrom), (int)(size))

extern int PexErrorBase;
#define PEXERR( pexerrnum )  (pexerrnum) + PexErrorBase 

#ifndef ABS
#define ABS(x)	((x) < 0 ? -(x) : (x))
#endif

#define MI_ZERO_TOLERANCE	1.0e-30	
#define MI_NEAR_ZERO( s )	(ABS(s) < MI_ZERO_TOLERANCE)
#define MI_ZERO_MAG( s )        ((s) < MI_ZERO_TOLERANCE)

#define MI_MAT_IDENTITY( mat, dim )						\
	{	register int	i,j;					\
		for (i=0; i<dim; i++)					\
			for (j=0; j<dim; j++)				\
				(mat)[i][j] = ( (i==j) ? 1.0 : 0.0 );	\
	}
	

typedef struct {
	ddEnumTypeIndex index;
	char            *name;
} miEnumType;

typedef enum {
	VIEW_REP_DYNAMIC=0,
	MARKER_BUNDLE_DYNAMIC=1,
	TEXT_BUNDLE_DYNAMIC=2,
	LINE_BUNDLE_DYNAMIC=3,
	INTERIOR_BUNDLE_DYNAMIC=4,
	EDGE_BUNDLE_DYNAMIC=5,
	COLOUR_TABLE_DYNAMIC=6,
	PATTERN_TABLE_DYNAMIC=7,
	WKS_TRANSFORM_DYNAMIC=8,
	HIGH_FILTER_DYNAMIC=9,
	INVIS_FILTER_DYNAMIC=10,
	HLHSR_MODE_DYNAMIC=11,
	STR_MODIFY_DYNAMIC=12,
	POST_STR_DYNAMIC=13,
	UNPOST_STR_DYNAMIC=14,
	DELETE_STR_DYNAMIC=15,
	REF_MODIFY_DYNAMIC=16,
	BUFFER_MODIFY_DYNAMIC=17,
	LIGHT_TABLE_DYNAMIC=18,
	DEPTH_CUE_DYNAMIC=19,
	COLOUR_APPROX_TABLE_DYNAMIC=20,
	MAX_DYNAMIC = 21
} ddDynamic;

#ifdef DDTEST
#define PRINTOC( ptr ) { \
	ddUSHORT elType, length; \
 \
    elType = ((pexElementInfo *)ptr)->elementType; \
    length = ((pexElementInfo *)ptr)->length; \
 \
	ErrorF("Element Type: %3d - %s    Length: %d\n", \
    elType, ocNames[elType], length ); \
	}
#else	/* DDTEST */
#define PRINTOC( ptr )
#endif

/* dd clip routine status returns */
#define MI_CLIP_LEFT		(1<<0)
#define MI_CLIP_TOP		(1<<2)
#define MI_CLIP_RIGHT		(1<<1)
#define MI_CLIP_BOTTOM		(1<<3)
#define MI_CLIP_FRONT		(1<<4)
#define MI_CLIP_BACK		(1<<5)

#define MI_CLIP_TRIVIAL_ACCEPT	0
#define MI_CLIP_POINT_1		1
#define MI_CLIP_POINT_2		2
#define MI_CLIP_TRIVIAL_REJECT	4

/* 
 * Memory management macros for use with data lists in static ddcontext 
 */

/*
 * MI_ROUND_LISTHEADERCOUNT is used by the clip routines to round up the 
 * header block count by 16 - in other words to allocated headerblocks
 * in increment of 16 and thus reduce calls to xrealloc.
 * Note that this doesn't work for beans w/ negative numbers (although
 * allocating a negative number of header blocks doesn't work well either!).
 */
#define MI_ROUND_LISTHEADERCOUNT(val) (((val) + 15) & ~15)

/* 
 * MI_ALLOCLISTHEADER insures that there are numlists headers in the
 * header array. It also returns either a pointer to the base of the
 * new header array, or 0 in the event of an xrealloc error.
 */
#define MI_ALLOCLISTHEADER(list, numlists)				\
   if ((list)->maxLists < (numlists)) {					\
     int i;								\
     listofddPoint *listptr;						\
     if ((list)->maxLists)		 				\
       (list)->ddList =							\
           (listofddPoint *)xrealloc((list)->ddList,			\
				     (numlists)*sizeof(listofddPoint));	\
     else								\
       (list)->ddList =							\
           (listofddPoint *)xalloc((numlists)*sizeof(listofddPoint));	\
     listptr = &(list)->ddList[(list)->maxLists];			\
     for (i = (list)->maxLists; i < (numlists); i++) { 			\
	listptr->numPoints = listptr->maxData = 0;			\
	(listptr++)->pts.p2Dpt = 0;					\
     }									\
     (list)->maxLists=(numlists);					\
   }

/* 
 * MI_FREELISTHEADER xfree's all allocated data associated with
 * a ListHeader data structure.
 */
#define MI_FREELISTHEADER(list)						\
   if ((list)->maxLists)						\
    {									\
     ddULONG		maxlists = (list)->maxLists;			\
     listofddPoint	*listptr = (list)->ddList;			\
     int		mi_freelistheader_counter;			\
     for (mi_freelistheader_counter = 0;				\
	  mi_freelistheader_counter < maxlists;				\
	  mi_freelistheader_counter++) { 				\
	if (listptr->maxData) xfree(listptr->pts.p2Dpt);		\
	listptr++;							\
     }									\
     xfree((list)->ddList);						\
     (list)->maxLists = 0;						\
    }

/* 
 * MI_ALLOCLISTOFDDPOINT insures that there are numpoints in the
 * vertex array of type type. It also returns either a pointer 
 * to the base of the new data array, or 0 in the event of 
 * an xrealloc error.
 */
#define MI_ALLOCLISTOFDDPOINT(buff, numpoints, bytes_per_vert)		\
  if ((buff)->maxData) {						\
    if ((buff)->maxData < (numpoints)*bytes_per_vert) { 		\
      (buff)->maxData = (numpoints) * bytes_per_vert;		 	\
      (buff)->pts.p2Dpt=(ddCoord2D *)xrealloc((buff)->pts.p2Dpt, 	\
					      (buff)->maxData);		\
    }									\
  } else {								\
    (buff)->maxData = (numpoints) * bytes_per_vert;			\
    (buff)->pts.p2Dpt=(ddCoord2D *)xalloc((buff)->maxData);		\
  }

/* 
 * MI_FREELISTOFDDPOINT frees the data area associated with
 * the specified list header. 
 * It also sets the list pointer to 0, and the max data count to 0.
 */
#define MI_FREELISTOFDDPOINT(buff, numpoints, bytes_per_vert)		\
 if (buff) 								\
   {									\
    xfree((buff)->pts.p2Dpt);						\
    (buff)->pts.p2Dpt = 0;						\
    (buff)->maxData = 0;						\
   }


/* 
 * Memory management macros for use with facet lists in static ddcontext 
 */

/* 
 * MI_ALLOCLISTOFDDFACET insures that there are numfacets in the
 * facet array of type type. It also returns either a pointer 
 * to the base of the new data array, or 0 in the event of 
 * an xrealloc error.
 */
#define MI_ALLOCLISTOFDDFACET(buff, numfacets, bytes_per_facet)		\
  if ((buff)->maxData) {						\
    if ((buff)->maxData < (numfacets)*bytes_per_facet) { 		\
      (buff)->maxData = (numfacets) * bytes_per_facet;		 	\
      (buff)->facets.pFacetRgbFloatN = 					\
	(ddRgbFloatNormal *)xrealloc((buff)->facets.pFacetRgbFloatN, 	\
				     (buff)->maxData);			\
    }									\
  } else {								\
    (buff)->maxData = (numfacets) * bytes_per_facet;			\
    (buff)->facets.pFacetRgbFloatN = 					\
			(ddRgbFloatNormal *)xalloc((buff)->maxData);	\
  }

/* 
 * MI_FREELISTOFDDFACET frees the data area associated with
 * the specified facet list. 
 * It also sets the list pointer to 0, and the max data count to 0.
 */
#define MI_FREELISTOFDDFACET(buff)					\
 if (buff) 								\
   {									\
    xfree((buff)->pts.p2Dpt);						\
    (buff)->facets.pFacetRgbFloatN = 0;					\
    (buff)->maxData = 0;						\
   }


/*    bit handling macros for renderer dynamics change flags */

#define MI_SET_ALL_CHANGES(prend)			\
	prend->tablesChanges = ~0;		\
	prend->namesetsChanges = ~0;		\
	prend->attrsChanges = ~0

#define MI_ZERO_ALL_CHANGES(prend)			\
	prend->tablesChanges = 0;		\
	prend->namesetsChanges = 0;		\
	prend->attrsChanges = 0


/* Inverse transform validation routines */

#define VALIDATEINVTRMCTOWCXFRM(pddc)					 \
	if (pddc->Static.misc.flags & INVTRMCTOWCXFRMFLAG) {		 \
	  miMatCopy(pddc->Dynamic->mc_to_wc_xform,			 \
		    pddc->Static.misc.inv_tr_mc_to_wc_xform);		 \
	  miMatInverseTranspose(pddc->Static.misc.inv_tr_mc_to_wc_xform);\
	  pddc->Static.misc.flags &= ~INVTRMCTOWCXFRMFLAG;		 \
	}

#define VALIDATEINVTRWCTOCCXFRM(pddc)					 \
	if (pddc->Static.misc.flags & INVTRWCTOCCXFRMFLAG) {		 \
	  miMatCopy(pddc->Dynamic->wc_to_cc_xform,			 \
		    pddc->Static.misc.inv_tr_wc_to_cc_xform);		 \
	  miMatInverseTranspose(pddc->Static.misc.inv_tr_wc_to_cc_xform);\
	  pddc->Static.misc.flags &= ~INVTRWCTOCCXFRMFLAG;		 \
	}

#define VALIDATEINVTRMCTOCCXFRM(pddc)					 \
	if (pddc->Static.misc.flags & INVTRMCTOCCXFRMFLAG) {		 \
	  miMatCopy(pddc->Dynamic->mc_to_cc_xform,			 \
		    pddc->Static.misc.inv_tr_mc_to_cc_xform);		 \
	  miMatInverseTranspose(pddc->Static.misc.inv_tr_mc_to_cc_xform);\
	  pddc->Static.misc.flags &= ~INVTRMCTOCCXFRMFLAG;		 \
	}

#define VALIDATEINVTRCCTODCXFRM(pddc)					 \
	if (pddc->Static.misc.flags & INVTRCCTODCXFRMFLAG) {		 \
	  miMatCopy(pddc->Dynamic->cc_to_dc_xform,			 \
		    pddc->Static.misc.inv_tr_cc_to_dc_xform);		 \
	  miMatInverseTranspose(pddc->Static.misc.inv_tr_cc_to_dc_xform);\
	  pddc->Static.misc.flags &= ~INVTRCCTODCXFRMFLAG;		 \
	}

#endif	/* MI_H */
