/* $Xorg: miWks.h,v 1.4 2001/02/09 02:04:08 xorgcvs Exp $ */

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

#include "mipex.h"
#include "ddpex4.h"
#include "miNS.h"
#include "miLUT.h"
#include "miInfo.h"
#include "miPick.h"

#ifndef MIWKS_H
#define MIWKS_H

typedef struct _ddOrdStruct {
	diStructHandle	pstruct;
	ddFLOAT	priority;
	struct _ddOrdStruct	*next;
} ddOrdStruct;
	
typedef struct {
	ddULONG		numStructs;
	ddOrdStruct	*postruct;	/* the first element in the list is a dummy */
} listofOrdStruct;

/**********************************************************************
 View numbers sparsely fill the range 0 - 65534 
 Since there is no way to predetermine which views will be defined,
 all views must be in the original priority list. The original
 list contains all views possible prioritized in numerical
 order, 0 is highest priority and 65534 is lowest.
 Each entry in the priority list contains a range of 
 views. All views in that range are prioritized numerically.
 When a view is defined, it is put in an entry by itself with
 first_view = last_view and defined = T.  Only defined views can
 have their priority changed, so putting them in their own entry
 makes this easier. If a view is deleted, its entry in the priority
 list has defined=F and stays in the list. If the view is set again, 
 defined is set to T again and it stays in the list whereever it is.

 The higher and lower fields contain the index of the entry containing
 the higher and lower priority views, resp. When view priorities
 are set, these values are changed. The highest priority view has
 higher = -1. The lowest priority view has lower = -1. Invalid
 entries are kept in a free list.
 NOTE: higher and lower contain index values into the view priority
 tables. These values do not correspond to view numbers.

 The max number of defined views is possible.  With defined views
 using one entry and all other views defined in ranges which at
 most will use one entry between and around every defined view,
 the max number of entries will be MAX_DEFINED_VIEWS *2 +1.
 Then, add 2 dummies entries as the head and tail of the list.
**********************************************************************/

#define MIWKS_MAX_VIEWS	6	/* same as in miViewLUT.c */
#define	MIWKS_MAX_ORD_VIEWS	( MIWKS_MAX_VIEWS * 2 + 3)

typedef struct _ddOrdView {
	short		defined;
	ddUSHORT	first_view;	/* view number */
	ddUSHORT	last_view;	/* view number */
	struct _ddOrdView *higher;	/* next higher view */
	struct _ddOrdView *lower;	/* next lower view */
} ddOrdView;

typedef struct {
	ddULONG		defined_views;	/* number of defined views */
	ddOrdView	*highest;	/* highest pri view */
	ddOrdView	*lowest;	/* lowest pri view */
	ddOrdView	*free;		/* first unused entry */
	ddOrdView	entries[MIWKS_MAX_ORD_VIEWS];
} listofOrdView;

#define	MIWKS_NEW_OV_ENTRY( plist, index )				\
	(index) = plist->free;					\
	if ((index) != NULL) {					\
		plist->free = index->lower;	\
		plist->free->higher = NULL; }

typedef struct _miWks {
	/* the resource id is in the dipex resource structure */
	ddEnumTypeIndex		displayUpdate;
	ddBYTE			visualState;
	ddBYTE			displaySurface;
	ddBYTE			viewUpdate;
	/* list of defined views and their priorities */
	/* highest priority is first on the list */
	listofOrdView	views;
	diLUTHandle		reqViewTable;
	/* deltaviewMask tells which entries in view table are pending */
	/* VIEW MASKS ONLY GOOD FOR VIEW TABLES WHOSE MAX SIZE IS 32 */
	ddULONG			deltaviewMask;
	/* current view table is in renderer */
	ddBYTE			wksUpdate;
	/* wksMask  tells if wks window or viewport is pending */
	ddBYTE			wksMask;
	ddNpcSubvolume		reqNpcSubvolume;
	/* current NPCsubvolume is in renderer */
	ddViewport		reqviewport;
	/* current Viewport is in renderer */
	ddBYTE			hlhsrUpdate;
	ddEnumTypeIndex		reqhlhsrMode;
	/* current HLHSR mode is in renderer */
	ddRendererPtr		pRend;
	/* stuff in renderer:			*
	 *	render id (same as wks id ) *
	 *	pointer to pc (NULL)		*
	 *	example drawable info		*
	 *	rendering drawable			*
	 *	current path 				*
	 *	renderer state 				*
	 *	marker bundle lut handle	*
	 *	text bundle lut handle 		*
	 *	line bundle lut handle		*
	 *	interior bundle lut handle 	*
	 *	edge bundle lut handle 		*
	 *	(current) view table handle	*
	 *	color table lut handle 		*
	 *	depth cue table lut handle 	*
	 *	light table lut handle 		*
	 *	approx tables lut handles 	*
	 *	pattern table lut handle 	*
	 *	font table lut handle 		*
	 *	highlight incl name set		*
	 *	highlight excl name set		*
	 *	invis incl name set			*
	 *	invis excl name set			*
	 *	current hlhsr mode			*
	 *	current npc subvolume		*
	 *	current viewport			*/
	ddBYTE			bufferUpdate;
	ddUSHORT	    	curBufferMode;
	ddUSHORT	    	reqBufferMode;
	listofOrdStruct		postedStructs;
	ddBYTE			dynamics[MAX_DYNAMIC];
	/* pick measures use workstation, so keep a free flage & reference count */
	ddBOOL			freeFlag;
	ddLONG			refCount;
	miPickDevice		devices[MIWKS_NUM_PICK_DEVICES];
	/* pwksList is extra object used by deal with dynamics */
	listofObj		*pwksList;
	DrawablePtr		doubleDrawables[2];
	int			curDoubleBuffer;
	int			hasDoubleBuffer;
	int			usingDoubleBuffer;
	DrawablePtr		pCurDrawable;
} miWksStr, *miWksPtr;

#endif	/* MIWKS_H */
