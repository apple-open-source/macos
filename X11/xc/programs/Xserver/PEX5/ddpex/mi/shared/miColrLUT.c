/* $Xorg: miColrLUT.c,v 1.4 2001/02/09 02:04:12 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/shared/miColrLUT.c,v 1.9 2001/12/14 19:57:37 dawes Exp $ */

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
#define	LUT_TYPE	PEXColourLUT

/* devPriv data structure */
#define	DD_LUT_ENTRY_STR	ddColourSpecifier 
/* table entry data structure */
#define	MI_LUT_ENTRY_STR	miColourEntry
/* pex data */
#define	PEX_LUT_ENTRY_STR	pexColourSpecifier

#define LUT_REND_DYN_BIT	PEXDynColourTableContents

#define LUT_START_INDEX          0
#define LUT_DEFAULT_INDEX        1
#define LUT_0_DEFINABLE_ENTRIES  256
#define LUT_0_NUM_PREDEFINED     8
#define LUT_0_PREDEFINED_MIN     0
#define LUT_0_PREDEFINED_MAX     7

#define LUT_TABLE_START(pheader)	(pheader)->plut.colour

#define	DYNAMIC		COLOUR_TABLE_DYNAMIC

/* predefined entries table: change this to work with your devPriv data */ 
static	DD_LUT_ENTRY_STR	pdeColourEntry[LUT_0_NUM_PREDEFINED];
#define	LUT_PDE_ENTRIES		pdeColourEntry[0]
#define	LUT_SET_PDE_ENTRY(pentry, pdeentry)	\
	(pentry)->entry = *(pdeentry);

/* predefined entry 1 is set to the default values 
 * change the XXX_DEFAULT_YYY macros below to use something else
 * if you don't want the default values defined in the pde table
 */
#define PDE_DEFAULT_ENTRY_NUM	1
#define	LUT_DEFAULT_VALUES	pdeColourEntry[PDE_DEFAULT_ENTRY_NUM]
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
#define LUT_FREE	ColourLUT_free
#define LUT_INQ_PREDEF	ColourLUT_inq_predef
#define LUT_INQ_ENTRIES	ColourLUT_inq_entries
*/
#define LUT_COPY	ColourLUT_copy
#define LUT_INQ_INFO	ColourLUT_inq_info
#define LUT_INQ_IND	ColourLUT_inq_ind
#define LUT_INQ_ENTRY	ColourLUT_inq_entry
#define LUT_SET_ENTRIES	ColourLUT_set_entries
#define LUT_DEL_ENTRIES	ColourLUT_del_entries
#define LUT_INQ_ENTRY_ADDRESS	ColourLUT_inq_entry_address
#define LUT_CREATE	ColourLUT_create
#define LUT_ENTRY_CHECK	ColourLUT_entry_check
#define LUT_COPY_PEX_MI	ColourLUT_copy_pex_to_mi
#define LUT_COPY_MI_PEX	ColourLUT_copy_mi_to_pex
#define LUT_MOD_CALL_BACK	ColourLUT_mod_call_back

/* copy from an mi entry to a pex entry and increment ppbuf */
ddpex43rtn
LUT_COPY_MI_PEX ( pheader, valueType, pentry, ppbuf )	
	miLUTHeader		*pheader;
	ddUSHORT		valueType;
	MI_LUT_ENTRY_STR	*pentry;
	ddPointer		*ppbuf;
{
	ddPointer	pb = *ppbuf;
	DD_LUT_ENTRY_STR	*pdev_entry;

	if (pentry == NULL)
		pdev_entry = &(LUT_DEFAULT_VALUES);
	else if (pentry->entry_info.status == MILUT_UNDEFINED)
		pdev_entry = &(LUT_DEFAULT_VALUES);
	else
		pdev_entry = &pentry->entry;

	mibcopy(pdev_entry, pb, sizeof(PEX_LUT_ENTRY_STR));

	pb+= sizeof(PEX_LUT_ENTRY_STR);

	MILUT_COPY_COLOUR(&pdev_entry->colour, pb, pdev_entry->colourType);

	pb += colour_type_sizes[(int)pdev_entry->colourType];
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

	pentry->entry.colourType = ((PEX_LUT_ENTRY_STR *)ps)->colourType;
	ps+= sizeof(PEX_LUT_ENTRY_STR);

	MILUT_COPY_COLOUR(ps, &(pentry->entry.colour), pentry->entry.colourType);

	ps += colour_type_sizes[(int)pentry->entry.colourType];
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

        /* colours: only accept supported colour types */
        if (MI_BADCOLOURTYPE((*ppPexEntry)->colourType))
                return(PEXERR(PEXColourTypeError));

	pe += sizeof(PEX_LUT_ENTRY_STR) + 
		colour_type_sizes[(int)(*ppPexEntry)->colourType];
	*ppPexEntry = (PEX_LUT_ENTRY_STR *)pe;
	return(Success);
}


void
ColourLUT_init_pde()
{
    /* black */
    pdeColourEntry[0].colourType = PEXRgbFloatColour;
    pdeColourEntry[0].colour.rgbFloat.red = 0.0;
    pdeColourEntry[0].colour.rgbFloat.green = 0.0;
    pdeColourEntry[0].colour.rgbFloat.blue = 0.0;
    /* white */
    pdeColourEntry[1].colourType = PEXRgbFloatColour;
    pdeColourEntry[1].colour.rgbFloat.red = 1.0;
    pdeColourEntry[1].colour.rgbFloat.green = 1.0;
    pdeColourEntry[1].colour.rgbFloat.blue = 1.0;
    /* red */
    pdeColourEntry[2].colourType = PEXRgbFloatColour;
    pdeColourEntry[2].colour.rgbFloat.red = 1.0;
    pdeColourEntry[2].colour.rgbFloat.green = 0.0;
    pdeColourEntry[2].colour.rgbFloat.blue = 0.0;
    /* green */
    pdeColourEntry[3].colourType = PEXRgbFloatColour;
    pdeColourEntry[3].colour.rgbFloat.red = 0.0;
    pdeColourEntry[3].colour.rgbFloat.green = 1.0;
    pdeColourEntry[3].colour.rgbFloat.blue = 0.0;
    /* blue */
    pdeColourEntry[4].colourType = PEXRgbFloatColour;
    pdeColourEntry[4].colour.rgbFloat.red = 0.0;
    pdeColourEntry[4].colour.rgbFloat.green = 0.0;
    pdeColourEntry[4].colour.rgbFloat.blue = 1.0;
    /* yellow */
    pdeColourEntry[5].colourType = PEXRgbFloatColour;
    pdeColourEntry[5].colour.rgbFloat.red = 1.0;
    pdeColourEntry[5].colour.rgbFloat.green = 1.0;
    pdeColourEntry[5].colour.rgbFloat.blue = 0.0;
    /* cyan */
    pdeColourEntry[6].colourType = PEXRgbFloatColour;
    pdeColourEntry[6].colour.rgbFloat.red = 0.0;
    pdeColourEntry[6].colour.rgbFloat.green = 1.0;
    pdeColourEntry[6].colour.rgbFloat.blue = 1.0;
    /* magenta */
    pdeColourEntry[7].colourType = PEXRgbFloatColour;
    pdeColourEntry[7].colour.rgbFloat.red = 1.0;
    pdeColourEntry[7].colour.rgbFloat.green = 0.0;
    pdeColourEntry[7].colour.rgbFloat.blue = 1.0;
}

#include "miLUTProcs.ci"

/* utility proc used to get highlight colour 
 * the highlight colour is the last entry in
 * the table, i.e. the one with the highest index 
 */
void
inq_last_colour_entry( pLUT, pColour )
	diLUTHandle		pLUT;
	ddColourSpecifier	*pColour;
{
	miLUTHeader		*pheader;
	ddTableIndex		high_index = 0;
	ddColourSpecifier	*high_entry = (ddColourSpecifier *)NULL;
	miColourEntry		*pEntry;
	register int i;

	if (pLUT)
	{
		pheader = MILUT_HEADER(pLUT);
		/* since this supports sparse tables, 
		 * we don't know which entry is the last one.
		 * so, we have to do a search. a linear search
		 * is done. this should be optimized if the table
		 * is very large.
		 */
		for (i = 0, pEntry = pheader->plut.colour; 
			i < MILUT_ALLOC_ENTS(pheader); i++, pEntry++)
		{
			if (pEntry->entry_info.status != MILUT_UNDEFINED)
				if (pEntry->entry_info.index > high_index)
				{
					high_index = pEntry->entry_info.index;
					high_entry = &pEntry->entry;
				}
		}
	}
	if (high_entry)
		*pColour = *high_entry;
	else
	{
		/* hot pink */
		pColour->colourType = PEXRgbFloatColour;
		pColour->colour.rgbFloat.red = 1.0;
		pColour->colour.rgbFloat.green = 0.41;
		pColour->colour.rgbFloat.blue = 0.71;
	}
	return;
}
