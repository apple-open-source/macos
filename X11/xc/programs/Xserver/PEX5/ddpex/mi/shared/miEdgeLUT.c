/* $Xorg: miEdgeLUT.c,v 1.4 2001/02/09 02:04:12 xorgcvs Exp $ */
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
supporting documentation, and that the name of Sun Microsystems
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/shared/miEdgeLUT.c,v 1.9 2001/12/14 19:57:37 dawes Exp $ */

#include "miLUT.h"
#include "miWks.h"
#include "miInfo.h"
#include "PEXErr.h"
#include "PEXprotost.h"
#include "pexos.h"


/*  Level 4 Shared Resources  */
/* Lookup Table Procedures */

extern	unsigned	colour_type_sizes[];	/* in miLUT.c */

/* definitions used by miLUTProcs.ci */
#define	LUT_TYPE	PEXEdgeBundleLUT

/* devPriv data structure */
#define	DD_LUT_ENTRY_STR	ddEdgeBundleEntry 
/* table entry data structure */
#define	MI_LUT_ENTRY_STR	miEdgeBundleEntry
/* pex data */
#define	PEX_LUT_ENTRY_STR	pexEdgeBundleEntry

#define LUT_REND_DYN_BIT	PEXDynEdgeBundleContents

#define LUT_START_INDEX          1
#define LUT_DEFAULT_INDEX        1
#define LUT_0_DEFINABLE_ENTRIES  20
#define LUT_0_NUM_PREDEFINED     1
#define LUT_0_PREDEFINED_MIN     1
#define LUT_0_PREDEFINED_MAX     1

#define LUT_TABLE_START(pheader)	(pheader)->plut.edge

#define	DYNAMIC		EDGE_BUNDLE_DYNAMIC

/* predefined entries table: change this to work with your devPriv data 
 * change the XXX_DEFAULT_YYY macros below to use something else
 * if you don't want the default values defined in the pde table
 */
static	DD_LUT_ENTRY_STR	pdeEdgeBundleEntry[LUT_0_NUM_PREDEFINED];
#define	LUT_PDE_ENTRIES		pdeEdgeBundleEntry[0]
#define	LUT_SET_PDE_ENTRY(pentry, pdeentry)		\
	(pentry)->entry = *(pdeentry);			\
	(pentry)->real_entry = *(pdeentry)


/* predefined entry 0 is set to the default values */
#define PDE_DEFAULT_ENTRY_NUM	0
#define	LUT_DEFAULT_VALUES	pdeEdgeBundleEntry[PDE_DEFAULT_ENTRY_NUM]
#define	LUT_SET_DEFAULT_VALUES(pentry)		\
	(pentry)->entry	= LUT_DEFAULT_VALUES;	\
	(pentry)->real_entry = LUT_DEFAULT_VALUES

/* which procedure definitions in miLUTProcs.h to use and their names
 * take out USE flags if you're defining those procs in here
 * but leave the name definitions
 */

#define LUT_USE_FREE
#define LUT_USE_INQ_PREDEF
#define LUT_USE_INQ_ENTRIES
#define LUT_USE_COPY
#define LUT_USE_INQ_INFO
#define LUT_USE_INQ_IND
#define LUT_USE_INQ_ENTRY
#define LUT_USE_SET_ENTRIES
#define LUT_USE_DEL_ENTRIES
#define LUT_USE_INQ_ENTRY_ADDRESS
#define LUT_USE_CREATE
#define LUT_USE_MOD_CALL_BACK

/* these three are redefined in miLUTProcs.h
#define LUT_FREE	EdgeBundleLUT_free
#define LUT_INQ_PREDEF	EdgeBundleLUT_inq_predef
#define LUT_INQ_ENTRIES	EdgeBundleLUT_inq_entries
*/
#define LUT_COPY	EdgeBundleLUT_copy
#define LUT_INQ_INFO	EdgeBundleLUT_inq_info
#define LUT_INQ_IND	EdgeBundleLUT_inq_ind
#define LUT_INQ_ENTRY	EdgeBundleLUT_inq_entry
#define LUT_SET_ENTRIES	EdgeBundleLUT_set_entries
#define LUT_DEL_ENTRIES	EdgeBundleLUT_del_entries
#define LUT_INQ_ENTRY_ADDRESS	EdgeBundleLUT_inq_entry_address
#define LUT_CREATE	EdgeBundleLUT_create
#define LUT_ENTRY_CHECK	EdgeBundleLUT_entry_check
#define LUT_COPY_PEX_MI	EdgeBundleLUT_copy_pex_to_mi
#define LUT_COPY_MI_PEX	EdgeBundleLUT_copy_mi_to_pex
#define LUT_MOD_CALL_BACK	EdgeBundleLUT_mod_call_back
#define	LUT_REALIZE_ENTRY	EdgeBundleLUT_realize_entry

/* copy from an mi entry to a pex entry and increment ppbuf */
ddpex43rtn
LUT_COPY_MI_PEX ( pheader, valueType, pentry, ppbuf )	
	miLUTHeader		*pheader;
	ddUSHORT		valueType;
	MI_LUT_ENTRY_STR	*pentry;
	ddPointer		*ppbuf;
{
	ddPointer	pb = *ppbuf;
        DD_LUT_ENTRY_STR        *pdev_entry;

        if (pentry == NULL)
                pdev_entry = &(LUT_DEFAULT_VALUES);
        else if (pentry->entry_info.status == MILUT_UNDEFINED)
                pdev_entry = &(LUT_DEFAULT_VALUES);
        else
		if (valueType == PEXRealizedValue)
                	pdev_entry = &pentry->real_entry;
		else
                	pdev_entry = &pentry->entry;

	mibcopy(pdev_entry, pb, sizeof(PEX_LUT_ENTRY_STR));

	pb+= sizeof(PEX_LUT_ENTRY_STR);

	MILUT_COPY_COLOUR(&pdev_entry->edgeColour.colour,
		pb, pdev_entry->edgeColour.colourType);

	pb += colour_type_sizes[(int)pdev_entry->edgeColour.colourType];
	*ppbuf = pb;
	return(Success);
}

/* copy from a pex entry to an mi entry and increment ppsrc */
ddpex43rtn
LUT_COPY_PEX_MI ( pheader, ppsrc, pentry )	
	miLUTHeader		*pheader;
	ddPointer		*ppsrc;
	MI_LUT_ENTRY_STR	*pentry;
{
	ddPointer	ps = *ppsrc;

	mibcopy(ps, &(pentry->entry), sizeof(PEX_LUT_ENTRY_STR));

	ps+= sizeof(PEX_LUT_ENTRY_STR);

	MILUT_COPY_COLOUR(ps, &(pentry->entry.edgeColour.colour),
		 pentry->entry.edgeColour.colourType);

	LUT_REALIZE_ENTRY( pheader, pentry );

	ps += colour_type_sizes[(int)pentry->entry.edgeColour.colourType];
	*ppsrc = ps;
	return(Success);
}

/* check for bad values and increment ppPexEntry */

ddpex43rtn
LUT_ENTRY_CHECK (pheader, ppPexEntry)
	miLUTHeader		*pheader;
	PEX_LUT_ENTRY_STR	**ppPexEntry;
{
	ddPointer	pe = (ddPointer)*ppPexEntry;

	/* edges: PEXOff or PEXOn */
	if (((*ppPexEntry)->edges != PEXOff) && ((*ppPexEntry)->edges != PEXOn))
		return(BadValue);

	/* edgeType: any value OK. use edge type 1 if it's not supported */
	/* edgeWidth: any value is OK, it is used as a scale */

        /* colours only accept supported colour types */
        if (MI_BADCOLOURTYPE((*ppPexEntry)->edgeColour.colourType))
                return(PEXERR(PEXColourTypeError));

	pe += sizeof(PEX_LUT_ENTRY_STR) + 
		colour_type_sizes[(int)(*ppPexEntry)->edgeColour.colourType];
	*ppPexEntry = (PEX_LUT_ENTRY_STR *)pe;
	return(Success);
}

/* realize entry */
ddpex43rtn
LUT_REALIZE_ENTRY( pheader, pEntry )
	miLUTHeader		*pheader;
	MI_LUT_ENTRY_STR	*pEntry;
{
	extern miEnumType	miSurfaceEdgeTypeET[][SI_EDGE_NUM];

	/* edges: PEXOff or PEXOn */
	pEntry->real_entry.edges = pEntry->entry.edges;

	/* edgeType: any value OK. use edge type 1 if it's not supported */
	if ( (pEntry->entry.edgeType < 
		miSurfaceEdgeTypeET[pheader->drawType][0].index) ||
	      (pEntry->entry.edgeType > 
		miSurfaceEdgeTypeET[pheader->drawType][SI_EDGE_NUM - 1].index) )
		pEntry->real_entry.edgeType = 1;
	else
		pEntry->real_entry.edgeType = pEntry->entry.edgeType;

	/* edgeWidth: any value is OK, it is multiplied by the nomimal
	 * edge width and the nearest supported size is used
	 * The realized value is the scale for inquiry, not the size
	 */
	pEntry->real_entry.edgeWidth = pEntry->entry.edgeWidth;

	/* colourType: its an error if an unsupported colour type was
	 * specified. For supported colour types, should mapped colour
	 * be returned??
	 */
	pEntry->real_entry.edgeColour = pEntry->entry.edgeColour;

}

void
EdgeBundleLUT_init_pde()
{
    pdeEdgeBundleEntry[0].edges = PEXOff;
    pdeEdgeBundleEntry[0].edgeType = PEXSurfaceEdgeSolid;
    pdeEdgeBundleEntry[0].edgeWidth = 1.0;
    MILUT_INIT_COLOUR(pdeEdgeBundleEntry[0].edgeColour);
}

#include "miLUTProcs.ci"
