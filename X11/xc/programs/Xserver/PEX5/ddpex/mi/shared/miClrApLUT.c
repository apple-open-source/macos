/* $Xorg: miClrApLUT.c,v 1.4 2001/02/09 02:04:12 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/shared/miClrApLUT.c,v 1.9 2001/12/14 19:57:37 dawes Exp $ */

#include "miLUT.h"
#include "miWks.h"
#include "miInfo.h"
#include "PEXErr.h"
#include "PEXprotost.h"
#include "pexos.h"


/*  Level 4 Shared Resources  */
/* Lookup Table Procedures */

/* definitions used by miLUTProcs.ci */
#define	LUT_TYPE	PEXColourApproxLUT

/* devPriv data structure */
#define	DD_LUT_ENTRY_STR	ddColourApproxEntry 
/* table entry data structure */
#define	MI_LUT_ENTRY_STR	miColourApproxEntry
/* pex data */
#define	PEX_LUT_ENTRY_STR	pexColourApproxEntry

#define LUT_REND_DYN_BIT	PEXDynColourApproxContents

#define LUT_START_INDEX          0
#define LUT_DEFAULT_INDEX        0
#define LUT_0_DEFINABLE_ENTRIES  6
#define LUT_0_NUM_PREDEFINED     0
#define LUT_0_PREDEFINED_MIN     0
#define LUT_0_PREDEFINED_MAX     0

#define LUT_TABLE_START(pheader)	(pheader)->plut.colourApprox

#define	DYNAMIC		COLOUR_APPROX_TABLE_DYNAMIC

/* predefined entries table: change this to work with your devPriv data */
/* there are no predefined entries, but define 1 entry for default */
static	DD_LUT_ENTRY_STR	pdeColourApproxEntry[1];
#define	LUT_PDE_ENTRIES		pdeColourApproxEntry[0]
#define	LUT_SET_PDE_ENTRY(pentry, pdeentry)	\
	(pentry)->entry = *(pdeentry)

/* predefined entry 0 is set to the default values 
 * change the XXX_DEFAULT_YYY macros below to use something else
 * if you don't want the default values defined in the pde table
 */
#define PDE_DEFAULT_ENTRY_NUM	0
#define	LUT_DEFAULT_VALUES	pdeColourApproxEntry[PDE_DEFAULT_ENTRY_NUM]
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
#define LUT_FREE	ColourApproxLUT_free
#define LUT_INQ_PREDEF	ColourApproxLUT_inq_predef
#define LUT_INQ_ENTRIES	ColourApproxLUT_inq_entries
*/
#define LUT_COPY	ColourApproxLUT_copy
#define LUT_INQ_INFO	ColourApproxLUT_inq_info
#define LUT_INQ_IND	ColourApproxLUT_inq_ind
#define LUT_INQ_ENTRY	ColourApproxLUT_inq_entry
#define LUT_SET_ENTRIES	ColourApproxLUT_set_entries
#define LUT_DEL_ENTRIES	ColourApproxLUT_del_entries
#define LUT_INQ_ENTRY_ADDRESS	ColourApproxLUT_inq_entry_address
#define LUT_CREATE	ColourApproxLUT_create
#define LUT_ENTRY_CHECK	ColourApproxLUT_entry_check
#define LUT_COPY_PEX_MI	ColourApproxLUT_copy_pex_to_mi
#define LUT_COPY_MI_PEX	ColourApproxLUT_copy_mi_to_pex
#define LUT_MOD_CALL_BACK	ColourApproxLUT_mod_call_back

/* copy from an mi entry to a pex entry and increment ppbuf */
ddpex43rtn
LUT_COPY_MI_PEX( pheader, valueType, pentry, ppbuf )
	miLUTHeader		*pheader;
	ddUSHORT		valueType;
	MI_LUT_ENTRY_STR	*pentry;
	ddPointer		*ppbuf;
{
	if (pentry == NULL)
		mibcopy(&(LUT_DEFAULT_VALUES), *ppbuf, sizeof(PEX_LUT_ENTRY_STR));
	else if (pentry->entry_info.status == MILUT_UNDEFINED)
		mibcopy(&(LUT_DEFAULT_VALUES), *ppbuf, sizeof(PEX_LUT_ENTRY_STR));
	else
		mibcopy(&pentry->entry, *ppbuf, sizeof(PEX_LUT_ENTRY_STR));

	*ppbuf += sizeof(PEX_LUT_ENTRY_STR);

	return(Success);
}

/* copy from a pex entry to an mi entry and increment ppsrc */
ddpex43rtn
LUT_COPY_PEX_MI( pheader, ppsrc, pentry )
	miLUTHeader		*pheader;
	ddPointer		*ppsrc;
	MI_LUT_ENTRY_STR	*pentry;
{
	mibcopy(*ppsrc, &(pentry->entry), sizeof(PEX_LUT_ENTRY_STR));
	*ppsrc += sizeof(PEX_LUT_ENTRY_STR);

	return(Success);
}

/* check for bad values and increment ppPexEntry */

ddpex43rtn
LUT_ENTRY_CHECK (pheader, ppPexEntry)
	miLUTHeader		*pheader;
	PEX_LUT_ENTRY_STR	**ppPexEntry;
{
    extern miEnumType   miColourApproxTypeET[][SI_CLR_APPROX_TYPE_NUM];
    extern miEnumType   miColourApproxModelET[][SI_CLR_APPROX_MODEL_NUM];

	if (((*ppPexEntry)->approxType < miColourApproxTypeET[pheader->drawType][0].index) ||
	    ((*ppPexEntry)->approxType > miColourApproxTypeET[pheader->drawType][SI_CLR_APPROX_TYPE_NUM - 1].index))
		return(BadValue);
	if (((*ppPexEntry)->approxModel < miColourApproxModelET[pheader->drawType][0].index) ||
	    ((*ppPexEntry)->approxModel > miColourApproxModelET[pheader->drawType][SI_CLR_APPROX_MODEL_NUM - 1].index))
		return(BadValue);
	if (((*ppPexEntry)->dither != PEXOff) && ((*ppPexEntry)->dither != PEXOn))
		return(BadValue);

    (*ppPexEntry)++;
    return(Success);
}

void
ColourApproxLUT_init_pde()
{
    /* Having default values for this makes absolutely no sense.
     * There is no way for this to know what colors are defined in
     * the coloramp, and therefor no way to know how to map to them.
     * This is provided only because something has to be done, but
     * the likelyhood of this turning out correct colors is small.
     * The client side MUST set at least one entry in this table
     * for correct functionality.
     * These are the sample values in the protocol spec.  
     * They assume a 6x6x6 color cube beginning at location 16.
     */
    pdeColourApproxEntry [0].approxType = PEXColourSpace;
    pdeColourApproxEntry [0].approxModel = PEXColourApproxRGB;
    pdeColourApproxEntry [0].max1 = 5;
    pdeColourApproxEntry [0].max2 = 5;
    pdeColourApproxEntry [0].max3 = 5;
    pdeColourApproxEntry [0].dither = PEXOff;
    pdeColourApproxEntry [0].mult1 = 1;
    pdeColourApproxEntry [0].mult2 = 6;
    pdeColourApproxEntry [0].mult3 = 36;
    pdeColourApproxEntry [0].weight1 = 1.0;
    pdeColourApproxEntry [0].weight2 = 1.0;
    pdeColourApproxEntry [0].weight3 = 1.0;
    pdeColourApproxEntry [0].basePixel = 16;
}

#include "miLUTProcs.ci"
