/* $Xorg: miDCueLUT.c,v 1.4 2001/02/09 02:04:12 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/shared/miDCueLUT.c,v 1.9 2001/12/14 19:57:37 dawes Exp $ */

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
#define	LUT_TYPE	PEXDepthCueLUT

/* devPriv data structure */
#define	DD_LUT_ENTRY_STR	ddDepthCueEntry 
/* table entry data structure */
#define	MI_LUT_ENTRY_STR	miDepthCueEntry
/* pex data */
#define	PEX_LUT_ENTRY_STR	pexDepthCueEntry

#define LUT_REND_DYN_BIT	PEXDynDepthCueTableContents

#define LUT_START_INDEX          0
#define LUT_DEFAULT_INDEX        0
#define LUT_0_DEFINABLE_ENTRIES  6
#define LUT_0_NUM_PREDEFINED     1
#define LUT_0_PREDEFINED_MIN     0
#define LUT_0_PREDEFINED_MAX     0

#define LUT_TABLE_START(pheader)	(pheader)->plut.depthCue

#define	DYNAMIC		DEPTH_CUE_DYNAMIC

/* predefined entries table: change this to work with your devPriv data */
static	DD_LUT_ENTRY_STR	pdeDepthCueEntry[LUT_0_NUM_PREDEFINED];
#define	LUT_PDE_ENTRIES		pdeDepthCueEntry[0]
#define	LUT_SET_PDE_ENTRY(pentry, pdeentry)	\
	(pentry)->entry = *(pdeentry)

/* predefined entry 0 is set to the default values 
 * change the XXX_DEFAULT_YYY macros below to use something else
 * if you don't want the default values defined in the pde table
 */
#define PDE_DEFAULT_ENTRY_NUM	0
#define	LUT_DEFAULT_VALUES	pdeDepthCueEntry[PDE_DEFAULT_ENTRY_NUM]
#define	LUT_SET_DEFAULT_VALUES(pentry)		\
	(pentry)->entry = LUT_DEFAULT_VALUES

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
#define LUT_FREE	DepthCueLUT_free
#define LUT_INQ_PREDEF	DepthCueLUT_inq_predef
#define LUT_INQ_ENTRIES	DepthCueLUT_inq_entries
*/
#define LUT_COPY	DepthCueLUT_copy
#define LUT_INQ_INFO	DepthCueLUT_inq_info
#define LUT_INQ_IND	DepthCueLUT_inq_ind
#define LUT_INQ_ENTRY	DepthCueLUT_inq_entry
#define LUT_SET_ENTRIES	DepthCueLUT_set_entries
#define LUT_DEL_ENTRIES	DepthCueLUT_del_entries
#define LUT_INQ_ENTRY_ADDRESS	DepthCueLUT_inq_entry_address
#define LUT_CREATE	DepthCueLUT_create
#define LUT_ENTRY_CHECK	DepthCueLUT_entry_check
#define LUT_COPY_PEX_MI	DepthCueLUT_copy_pex_to_mi
#define LUT_COPY_MI_PEX	DepthCueLUT_copy_mi_to_pex
#define LUT_MOD_CALL_BACK	DepthCueLUT_mod_call_back

/* copy from an mi entry to a pex entry and increment ppbuf */
ddpex43rtn
LUT_COPY_MI_PEX ( pheader, valueType, pentry, ppbuf )	
	miLUTHeader		*pheader;
	ddUSHORT		valueType;
	MI_LUT_ENTRY_STR	*pentry;
	ddPointer		*ppbuf;
{
	DD_LUT_ENTRY_STR	*pdev_entry;
	ddPointer	pb = *ppbuf;

        if (pentry == NULL)
                pdev_entry = &(LUT_DEFAULT_VALUES);
        else if (pentry->entry_info.status == MILUT_UNDEFINED)
                pdev_entry = &(LUT_DEFAULT_VALUES);
        else
                pdev_entry = &pentry->entry;

	mibcopy(pdev_entry, pb, sizeof(PEX_LUT_ENTRY_STR));

	pb+= sizeof(PEX_LUT_ENTRY_STR);

	MILUT_COPY_COLOUR(&pdev_entry->depthCueColour.colour,
		pb, pdev_entry->depthCueColour.colourType);

	pb += colour_type_sizes[(int)pdev_entry->depthCueColour.colourType];
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

	MILUT_COPY_COLOUR(ps, &(pentry->entry.depthCueColour.colour),
		 pentry->entry.depthCueColour.colourType);

	ps += colour_type_sizes[(int)pentry->entry.depthCueColour.colourType];
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

	/* mode: Off or On */
	if (((*ppPexEntry)->mode != PEXOff) && ((*ppPexEntry)->mode != PEXOn))
		return(BadValue);
	/* frontPlane: must be in NPC */
	if (((*ppPexEntry)->frontPlane < 0.0) || ((*ppPexEntry)->frontPlane > 1.0))
		return(BadValue);
	/* backPlane: must be in NPC */
	if (((*ppPexEntry)->backPlane < 0.0) || ((*ppPexEntry)->backPlane > 1.0))
		return(BadValue);
	/* frontScaling: must be in NPC */
	if (((*ppPexEntry)->frontScaling < 0.0) || ((*ppPexEntry)->frontScaling > 1.0))
		return(BadValue);
	/* backScaling: must be in NPC */
	if (((*ppPexEntry)->backScaling < 0.0) || ((*ppPexEntry)->backScaling > 1.0))
		return(BadValue);
	/* colours: only accept supported colour types */
	if (MI_BADCOLOURTYPE((*ppPexEntry)->depthCueColour.colourType))
		return(PEXERR(PEXColourTypeError));

	pe += sizeof(PEX_LUT_ENTRY_STR) + 
		colour_type_sizes[(int)(*ppPexEntry)->depthCueColour.colourType];

	*ppPexEntry = (PEX_LUT_ENTRY_STR *)pe;
	return(Success);
}


void
DepthCueLUT_init_pde()
{
    pdeDepthCueEntry[0].mode = PEXOff;
    pdeDepthCueEntry[0].frontPlane = 1.0;
    pdeDepthCueEntry[0].backPlane = 0.0;
    pdeDepthCueEntry[0].frontScaling = 1.0;
    pdeDepthCueEntry[0].backScaling = 0.5;
    MILUT_INIT_COLOUR(pdeDepthCueEntry[0].depthCueColour);
}

#include "miLUTProcs.ci"
