/* $Xorg: miLightLUT.c,v 1.4 2001/02/09 02:04:13 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/shared/miLightLUT.c,v 1.9 2001/12/14 19:57:39 dawes Exp $ */

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
#define	LUT_TYPE	PEXLightLUT

/* devPriv data structure */
#define	DD_LUT_ENTRY_STR	ddLightEntry 
/* table entry data structure */
#define	MI_LUT_ENTRY_STR	miLightEntry
/* pex data */
#define	PEX_LUT_ENTRY_STR	pexLightEntry

#define LUT_REND_DYN_BIT	PEXDynLightTableContents

#define LUT_START_INDEX          1
#define LUT_DEFAULT_INDEX        1
#define LUT_0_DEFINABLE_ENTRIES  16
#define LUT_0_NUM_PREDEFINED     1
#define LUT_0_PREDEFINED_MIN     1
#define LUT_0_PREDEFINED_MAX     1

#define LUT_TABLE_START(pheader)	(pheader)->plut.light

#define	DYNAMIC		LIGHT_TABLE_DYNAMIC

/* predefined entries table: change this to work with your devPriv data */
static	DD_LUT_ENTRY_STR	pdeLightEntry[LUT_0_NUM_PREDEFINED];
#define	LUT_PDE_ENTRIES		pdeLightEntry[0]
#define	LUT_SET_PDE_ENTRY(pentry, pdeentry)			\
	(pentry)->entry	= *(pdeentry);				\
	if ((pentry)->entry.lightType == PEXLightWcsSpot)	\
			(pentry)->cosSpreadAngle = 		\
			   cos((double)(pentry)->entry.spreadAngle);	\
		else (pentry)->cosSpreadAngle = 0.0

/* predefined entry 0 is set to the default values 
 * change the XXX_DEFAULT_YYY macros below to use something else
 * if you don't want the default values defined in the pde table
 */
#define PDE_DEFAULT_ENTRY_NUM	0
#define	LUT_DEFAULT_VALUES	pdeLightEntry[PDE_DEFAULT_ENTRY_NUM]
#define	LUT_SET_DEFAULT_VALUES(pentry)		\
	(pentry)->entry	= LUT_DEFAULT_VALUES


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
#define LUT_FREE	LightLUT_free
#define LUT_INQ_PREDEF	LightLUT_inq_predef
#define LUT_INQ_ENTRIES	LightLUT_inq_entries
*/
#define LUT_COPY	LightLUT_copy
#define LUT_INQ_INFO	LightLUT_inq_info
#define LUT_INQ_IND	LightLUT_inq_ind
#define LUT_INQ_ENTRY	LightLUT_inq_entry
#define LUT_SET_ENTRIES	LightLUT_set_entries
#define LUT_DEL_ENTRIES	LightLUT_del_entries
#define LUT_INQ_ENTRY_ADDRESS	LightLUT_inq_entry_address
#define LUT_CREATE	LightLUT_create
#define LUT_ENTRY_CHECK	LightLUT_entry_check
#define LUT_COPY_PEX_MI	LightLUT_copy_pex_to_mi
#define LUT_COPY_MI_PEX	LightLUT_copy_mi_to_pex
#define LUT_MOD_CALL_BACK	LightLUT_mod_call_back

/* copy from an mi entry to a pex entry and increment ppbuf */
ddpex43rtn
LUT_COPY_MI_PEX ( pheader, valueType, pentry, ppbuf )	
        miLUTHeader             *pheader;
	ddUSHORT                valueType;
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

	MILUT_COPY_COLOUR(&pdev_entry->lightColour.colour,
		pb, pdev_entry->lightColour.colourType);

	pb += colour_type_sizes[(int)pdev_entry->lightColour.colourType];
	*ppbuf = pb;
	return(Success);
}

/* copy from a pex entry to an mi entry and increment ppsrc */
ddpex43rtn
LUT_COPY_PEX_MI ( pheader, ppsrc, pentry )	
        miLUTHeader             *pheader;
	ddPointer		*ppsrc;
	MI_LUT_ENTRY_STR	*pentry;
{
	ddPointer	ps = *ppsrc;

	mibcopy(ps, &(pentry->entry), sizeof(PEX_LUT_ENTRY_STR));

	ps+= sizeof(PEX_LUT_ENTRY_STR);

	MILUT_COPY_COLOUR(ps, &(pentry->entry.lightColour.colour),
		 pentry->entry.lightColour.colourType);

	ps += colour_type_sizes[(int)pentry->entry.lightColour.colourType];
	if (pentry->entry.lightType == PEXLightWcsSpot)
		pentry->cosSpreadAngle = cos((double)pentry->entry.spreadAngle);
	else pentry->cosSpreadAngle = 0.0;

	*ppsrc = ps;
	return(Success);
}

/* check for bad values and increment ppPexEntry */

ddpex43rtn
LUT_ENTRY_CHECK (pheader, ppPexEntry)
	miLUTHeader		*pheader;
	PEX_LUT_ENTRY_STR	**ppPexEntry;
{
	extern miEnumType	miLightTypeET[][SI_LIGHT_NUM];
	ddPointer	pe = (ddPointer)*ppPexEntry;

	/* lightType: only use supported lights */
	if (((*ppPexEntry)->lightType < miLightTypeET[pheader->drawType][0].index) ||
	    ((*ppPexEntry)->lightType > miLightTypeET[pheader->drawType][SI_LIGHT_NUM - 1].index))
		return(BadValue);
	/* direction: any value OK. */
	/* point: any value OK. */
	/* concentration: any value OK. */
	/* spreadAngle: must be in range [0,pi] */
	if ((*ppPexEntry)->lightType == PEXLightWcsSpot)
		if (((*ppPexEntry)->spreadAngle < 0.0) || 
		    ((*ppPexEntry)->spreadAngle > MI_PI))
			return(BadValue);
	/* attenuaton1: any value OK. */
	/* attenuaton2: any value OK. */
	/* colours: only accept supported colour types */
	if (MI_BADCOLOURTYPE((*ppPexEntry)->lightColour.colourType))
		return(PEXERR(PEXColourTypeError));

	pe += sizeof(PEX_LUT_ENTRY_STR) + colour_type_sizes[(int)(*ppPexEntry)->lightColour.colourType];

	*ppPexEntry = (PEX_LUT_ENTRY_STR *)pe;
	return(Success);
}

void
LightLUT_init_pde()
{
    pdeLightEntry[0].lightType = PEXLightAmbient;
    pdeLightEntry[0].direction.x = 0.0;
    pdeLightEntry[0].direction.y = 0.0;
    pdeLightEntry[0].direction.z = 0.0;
    pdeLightEntry[0].point.x = 0.0;
    pdeLightEntry[0].point.y = 0.0;
    pdeLightEntry[0].point.z = 0.0;
    pdeLightEntry[0].concentration = 0.0;
    pdeLightEntry[0].spreadAngle = 0.0;
    pdeLightEntry[0].attenuation1 = 0.0;
    pdeLightEntry[0].attenuation2 = 0.0;
    pdeLightEntry[0].lightColour.colourType = PEXRgbFloatColour;
    pdeLightEntry[0].lightColour.colour.rgbFloat.red = 1.0;
    pdeLightEntry[0].lightColour.colour.rgbFloat.green = 1.0;
    pdeLightEntry[0].lightColour.colour.rgbFloat.blue = 1.0;
}

#include "miLUTProcs.ci"

