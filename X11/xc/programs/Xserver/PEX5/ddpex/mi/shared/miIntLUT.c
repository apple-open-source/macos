/* $Xorg: miIntLUT.c,v 1.4 2001/02/09 02:04:12 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/shared/miIntLUT.c,v 1.9 2001/12/14 19:57:38 dawes Exp $ */

#include "miLUT.h"
#include "miWks.h"
#include "miInfo.h"
#include "PEXErr.h"
#include "PEXprotost.h"
#include "pexos.h"


/* useful definition when testing */
typedef struct {
    pexEnumTypeIndex    interiorStyle;
    INT16               interiorStyleIndex;
    pexEnumTypeIndex    reflectionModel;
    pexEnumTypeIndex    surfaceInterp;
    pexEnumTypeIndex    bfInteriorStyle;
    INT16               bfInteriorStyleIndex;
    pexEnumTypeIndex    bfReflectionModel;
    pexEnumTypeIndex    bfSurfaceInterp;
    pexSurfaceApprox    surfaceApprox;
    pexColourSpecifier  surfaceColour;
    pexIndexedColour	index1;
    pexReflectionAttr   reflectionAttr;
    pexIndexedColour	index2;
    pexColourSpecifier  bfSurfaceColour;
    pexIndexedColour	index3;
    pexReflectionAttr   bfReflectionAttr;
    pexIndexedColour	index4;
} bogus_pexInteriorBundleEntry;

/*  Level 4 Shared Resources  */
/* Lookup Table Procedures */

extern	unsigned	colour_type_sizes[];	/* in miLUT.c */

/* definitions used by miLUTProcs.ci */
#define	LUT_TYPE	PEXInteriorBundleLUT

/* devPriv data structure */
#define	DD_LUT_ENTRY_STR	ddInteriorBundleEntry 
/* table entry data structure */
#define	MI_LUT_ENTRY_STR	miInteriorBundleEntry
/* pex data */
#define	PEX_LUT_ENTRY_STR	pexInteriorBundleEntry

#define LUT_REND_DYN_BIT	PEXDynInteriorBundleContents

#define LUT_START_INDEX          1
#define LUT_DEFAULT_INDEX        1
#define LUT_0_DEFINABLE_ENTRIES  20
#define LUT_0_NUM_PREDEFINED     1
#define LUT_0_PREDEFINED_MIN     1
#define LUT_0_PREDEFINED_MAX     1

#define LUT_TABLE_START(pheader)	(pheader)->plut.interior

#define	DYNAMIC		INTERIOR_BUNDLE_DYNAMIC

/* predefined entries table: change this to work with your devPriv data */ 
static	DD_LUT_ENTRY_STR	pdeInteriorBundleEntry[LUT_0_NUM_PREDEFINED];
#define	LUT_PDE_ENTRIES		pdeInteriorBundleEntry[0]
#define	LUT_SET_PDE_ENTRY(pentry, pdeentry)	\
	(pentry)->entry = *(pdeentry);		\
	(pentry)->real_entry = *(pdeentry)

/* predefined entry 0 is set to the default values 
 * change the XXX_DEFAULT_YYY macros below to use something else
 * if you don't want the default values defined in the pde table
 */
#define PDE_DEFAULT_ENTRY_NUM	0
#define	LUT_DEFAULT_VALUES	pdeInteriorBundleEntry[PDE_DEFAULT_ENTRY_NUM]
#define	LUT_SET_DEFAULT_VALUES(pentry)		\
	(pentry)->entry = LUT_DEFAULT_VALUES;	\
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
#define LUT_FREE	InteriorBundleLUT_free
#define LUT_INQ_PREDEF	InteriorBundleLUT_inq_predef
#define LUT_INQ_ENTRIES	InteriorBundleLUT_inq_entries
*/
#define LUT_COPY	InteriorBundleLUT_copy
#define LUT_INQ_INFO	InteriorBundleLUT_inq_info
#define LUT_INQ_IND	InteriorBundleLUT_inq_ind
#define LUT_INQ_ENTRY	InteriorBundleLUT_inq_entry
#define LUT_SET_ENTRIES	InteriorBundleLUT_set_entries
#define LUT_DEL_ENTRIES	InteriorBundleLUT_del_entries
#define LUT_INQ_ENTRY_ADDRESS	InteriorBundleLUT_inq_entry_address
#define LUT_CREATE	InteriorBundleLUT_create
#define LUT_ENTRY_CHECK	InteriorBundleLUT_entry_check
#define LUT_COPY_PEX_MI	InteriorBundleLUT_copy_pex_to_mi
#define LUT_COPY_MI_PEX	InteriorBundleLUT_copy_mi_to_pex
#define LUT_MOD_CALL_BACK	InteriorBundleLUT_mod_call_back
#define	LUT_REALIZE_ENTRY	InteriorBundleLUT_realize_entry

/* copy from an mi entry to a pex entry and increment ppbuf */
ddpex43rtn
LUT_COPY_MI_PEX ( pheader, valueType, pentry, ppbuf )	
        miLUTHeader             *pheader;
	ddUSHORT                valueType;
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
    pb += sizeof(PEX_LUT_ENTRY_STR);
	    
    /* surface colour */
    mibcopy(&pdev_entry->surfaceColour, pb, sizeof(pexColourSpecifier));
    pb += sizeof(pexColourSpecifier);
    MILUT_COPY_COLOUR(&pdev_entry->surfaceColour.colour,
	pb, pdev_entry->surfaceColour.colourType);
    pb += colour_type_sizes[(int)pdev_entry->surfaceColour.colourType];

    /* refl attrs */
    mibcopy(&pdev_entry->reflectionAttr, pb, sizeof(pexReflectionAttr));
    pb += sizeof(pexReflectionAttr);
    MILUT_COPY_COLOUR(&pdev_entry->reflectionAttr.specularColour.colour,
	pb, pdev_entry->reflectionAttr.specularColour.colourType);
    pb += colour_type_sizes[(int)pdev_entry->reflectionAttr.specularColour.colourType];

    /* bf surface colour */
    mibcopy(&pdev_entry->bfSurfaceColour, pb, sizeof(pexColourSpecifier));
    pb += sizeof(pexColourSpecifier);
    MILUT_COPY_COLOUR(&pdev_entry->bfSurfaceColour.colour,
	pb, pdev_entry->bfSurfaceColour.colourType);
    pb += colour_type_sizes[(int)pdev_entry->bfSurfaceColour.colourType];

    /* bf refl attrs */
    mibcopy(&pdev_entry->bfReflectionAttr, pb, sizeof(pexReflectionAttr));
    pb += sizeof(pexReflectionAttr);
    MILUT_COPY_COLOUR(&pdev_entry->bfReflectionAttr.specularColour.colour,
	pb, pdev_entry->bfReflectionAttr.specularColour.colourType);
    pb += colour_type_sizes[(int)pdev_entry->bfReflectionAttr.specularColour.colourType];

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
	ddSHORT		colourType;
	ddPointer	psrc = *ppsrc;

	mibcopy(psrc, &(pentry->entry), sizeof(PEX_LUT_ENTRY_STR));
	psrc += sizeof(PEX_LUT_ENTRY_STR);

	pentry->entry.surfaceColour.colourType = colourType = 
		((pexColourSpecifier *)psrc)->colourType;
	psrc += sizeof(pexColourSpecifier);
	MILUT_COPY_COLOUR(psrc, &(pentry->entry.surfaceColour.colour), colourType);
	psrc += colour_type_sizes[(int)colourType];

	mibcopy(psrc, &(pentry->entry.reflectionAttr), sizeof(pexReflectionAttr));
	psrc += sizeof(pexReflectionAttr);

	colourType = pentry->entry.reflectionAttr.specularColour.colourType;
	MILUT_COPY_COLOUR(psrc, &(pentry->entry.reflectionAttr.specularColour.colour), 
		colourType);
	psrc += colour_type_sizes[(int)colourType];

	pentry->entry.bfSurfaceColour.colourType = colourType = 
		((pexColourSpecifier *)psrc)->colourType;
	psrc += sizeof(pexColourSpecifier);
	MILUT_COPY_COLOUR(psrc, &(pentry->entry.bfSurfaceColour.colour), colourType);
	psrc += colour_type_sizes[(int)colourType];

	mibcopy(psrc, &(pentry->entry.bfReflectionAttr), sizeof(pexReflectionAttr));
	psrc += sizeof(pexReflectionAttr);

	colourType = pentry->entry.bfReflectionAttr.specularColour.colourType;
	MILUT_COPY_COLOUR(psrc, &(pentry->entry.bfReflectionAttr.specularColour.colour), 
		colourType);
	psrc += colour_type_sizes[(int)colourType];

	LUT_REALIZE_ENTRY( pheader, pentry );

	*ppsrc = psrc;

	return(Success);
}

/* check for bad values and increment ppPexEntry */

ddpex43rtn
LUT_ENTRY_CHECK (pheader, ppPexEntry)
	miLUTHeader		*pheader;
	PEX_LUT_ENTRY_STR	**ppPexEntry;
{
    ddPointer		ps;
    ddSHORT		colourType;

	/* interiorStyle: any value OK. use style 1 if it's not supported */
	/* interiorStyleIndex: any value OK. this is used for patterns 
	 * and hatches which aren't supported */
	/* reflectionModel: any value OK. use method 1 if it's not supported */
	/* surfaceInterp: any value OK. use method 1 if it's not supported */
	/* same as above for bf values */

	/* surfaceApprox: any value OK. use method 1 if it's not supported */
	/* front and back reflection attrs: any values OK */

	/* colours: only accept supported colour types */
	ps = (ddPointer)(*ppPexEntry + 1);
	colourType = ((pexColourSpecifier *)ps)->colourType;
	if (MI_BADCOLOURTYPE(colourType))
		return(PEXERR(PEXColourTypeError));

	ps += sizeof(pexColourSpecifier) + colour_type_sizes[(int)colourType];
	colourType = ((pexReflectionAttr *)ps)->specularColour.colourType;
	if (MI_BADCOLOURTYPE(colourType))
		return(PEXERR(PEXColourTypeError));

	ps += sizeof(pexReflectionAttr) + colour_type_sizes[(int)colourType];
	colourType = ((pexColourSpecifier *)ps)->colourType;
	if (MI_BADCOLOURTYPE(colourType))
		return(PEXERR(PEXColourTypeError));

	ps += sizeof(pexColourSpecifier) + colour_type_sizes[(int)colourType];
	colourType = ((pexReflectionAttr *)ps)->specularColour.colourType;
	if (MI_BADCOLOURTYPE(colourType))
		return(PEXERR(PEXColourTypeError));

	ps += sizeof(pexReflectionAttr) + colour_type_sizes[(int)colourType];
	*ppPexEntry = (PEX_LUT_ENTRY_STR *)ps;

    return(Success);
}

/* realize entry */
ddpex43rtn
LUT_REALIZE_ENTRY( pheader, pEntry )
	miLUTHeader	        *pheader;
	MI_LUT_ENTRY_STR	*pEntry;
{
	extern miEnumType	miInteriorStyleET[][SI_INT_NUM];
	extern miEnumType	miReflectionModelET[][SI_REFLECT_NUM];
	extern miEnumType	miSurfaceInterpMethodET[][SI_SURF_INTERP_NUM];
	extern miEnumType	miSurfaceApproxMethodET[][SI_SURF_APPROX_NUM];

	/* interiorStyle: any value OK. use interior style 1 if it's not supported */
	if ( (pEntry->entry.interiorStyle < 
		miInteriorStyleET[pheader->drawType][0].index) ||
	      (pEntry->entry.interiorStyle > 
		miInteriorStyleET[pheader->drawType][SI_INT_NUM - 1].index) )
		pEntry->real_entry.interiorStyle = 1;
	else
		pEntry->real_entry.interiorStyle = pEntry->entry.interiorStyle;

	/* interiorStyleIndex: any value OK. it's only for patterns & hatches
	 * which are supported */
	pEntry->real_entry.interiorStyleIndex = pEntry->entry.interiorStyleIndex;

	/* reflectionModel: any value OK. use model 1 if it's not supported */
	if ( (pEntry->entry.reflectionModel < 
		miReflectionModelET[pheader->drawType][0].index) ||
	      (pEntry->entry.reflectionModel > 
		miReflectionModelET[pheader->drawType][SI_REFLECT_NUM - 1].index) )
		pEntry->real_entry.reflectionModel = 1;
	else
		pEntry->real_entry.reflectionModel = pEntry->entry.reflectionModel;

	/* surfaceInterp: any value OK. use model 1 if it's not supported */
	if ( (pEntry->entry.surfaceInterp < 
		miSurfaceInterpMethodET[pheader->drawType][0].index) ||
	      (pEntry->entry.surfaceInterp > 
		miSurfaceInterpMethodET[pheader->drawType][SI_SURF_INTERP_NUM - 1].index) )
		pEntry->real_entry.surfaceInterp = 1;
	else
		pEntry->real_entry.surfaceInterp = pEntry->entry.surfaceInterp;

	/* bfinteriorStyle: any value OK. use style 1 if it's not supported */
	if ( (pEntry->entry.bfInteriorStyle < 
		miInteriorStyleET[pheader->drawType][0].index) ||
	      (pEntry->entry.bfInteriorStyle > 
		miInteriorStyleET[pheader->drawType][SI_INT_NUM - 1].index) )
		pEntry->real_entry.bfInteriorStyle = 1;
	else
		pEntry->real_entry.bfInteriorStyle = pEntry->entry.bfInteriorStyle;

	/* bfInteriorStyleIndex: any value OK. it's only for patterns & hatches
	 * which are supported */
	pEntry->real_entry.bfInteriorStyleIndex = pEntry->entry.bfInteriorStyleIndex;

	/* bfReflectionModel: any value OK. use model 1 if it's not supported */
	if ( (pEntry->entry.bfReflectionModel < 
		miReflectionModelET[pheader->drawType][0].index) ||
	      (pEntry->entry.bfReflectionModel > 
		miReflectionModelET[pheader->drawType][SI_REFLECT_NUM - 1].index) )
		pEntry->real_entry.bfReflectionModel = 1;
	else
		pEntry->real_entry.bfReflectionModel = pEntry->entry.bfReflectionModel;

	/* bfSurfaceInterp: any value OK. use model 1 if it's not supported */
	if ( (pEntry->entry.bfSurfaceInterp < 
		miSurfaceInterpMethodET[pheader->drawType][0].index) ||
	      (pEntry->entry.bfSurfaceInterp > 
		miSurfaceInterpMethodET[pheader->drawType][SI_SURF_INTERP_NUM - 1].index) )
		pEntry->real_entry.bfSurfaceInterp = 1;
	else
		pEntry->real_entry.bfSurfaceInterp = pEntry->entry.bfSurfaceInterp;

	/* surfaceApprox: any value OK. use model 1 if it's not supported */
	if ( (pEntry->entry.surfaceApprox.approxMethod < 
		miSurfaceApproxMethodET[pheader->drawType][0].index) ||
	      (pEntry->entry.surfaceApprox.approxMethod > 
		miSurfaceApproxMethodET[pheader->drawType][SI_SURF_APPROX_NUM - 1].index) )
		pEntry->real_entry.surfaceApprox.approxMethod = 1;
	else
		pEntry->real_entry.surfaceApprox.approxMethod = 
			pEntry->entry.surfaceApprox.approxMethod;

	pEntry->real_entry.surfaceApprox.uTolerance = pEntry->entry.surfaceApprox.uTolerance;
	pEntry->real_entry.surfaceApprox.vTolerance = pEntry->entry.surfaceApprox.vTolerance;

        /* front and back reflection attrs: any values OK (includes colours) */
	pEntry->real_entry.reflectionAttr = pEntry->entry.reflectionAttr;
	pEntry->real_entry.bfReflectionAttr = pEntry->entry.bfReflectionAttr;

	/* colourType: its an error if an unsupported colour type was
	 * specified. For supported colour types, should mapped colour
	 * be returned??
	 */
	pEntry->real_entry.surfaceColour = pEntry->entry.surfaceColour;
	pEntry->real_entry.bfSurfaceColour = pEntry->entry.bfSurfaceColour;

}
void
InteriorBundleLUT_init_pde()
{
    pdeInteriorBundleEntry[0].interiorStyle = PEXInteriorStyleHollow;
    pdeInteriorBundleEntry[0].interiorStyleIndex = 1;
    pdeInteriorBundleEntry[0].reflectionModel = PEXReflectionNoShading;
    pdeInteriorBundleEntry[0].surfaceInterp = PEXSurfaceInterpNone;
    pdeInteriorBundleEntry[0].bfInteriorStyle = PEXInteriorStyleHollow;
    pdeInteriorBundleEntry[0].bfInteriorStyleIndex = 1;
    pdeInteriorBundleEntry[0].bfReflectionModel = PEXReflectionNoShading;
    pdeInteriorBundleEntry[0].bfSurfaceInterp = PEXSurfaceInterpNone;
    pdeInteriorBundleEntry[0].surfaceApprox.approxMethod = 1;
    pdeInteriorBundleEntry[0].surfaceApprox.uTolerance = 1.0;
    pdeInteriorBundleEntry[0].surfaceApprox.vTolerance = 1.0;
    MILUT_INIT_COLOUR(pdeInteriorBundleEntry[0].surfaceColour);
    pdeInteriorBundleEntry[0].bfSurfaceColour = 
	pdeInteriorBundleEntry[0].surfaceColour;
    pdeInteriorBundleEntry[0].reflectionAttr.ambient = 1.0;
    pdeInteriorBundleEntry[0].reflectionAttr.diffuse = 1.0;
    pdeInteriorBundleEntry[0].reflectionAttr.specular = 1.0;
    pdeInteriorBundleEntry[0].reflectionAttr.specularConc = 0.0;
    pdeInteriorBundleEntry[0].reflectionAttr.transmission = 0.0;
    MILUT_INIT_COLOUR(pdeInteriorBundleEntry[0].reflectionAttr.specularColour);
    pdeInteriorBundleEntry[0].bfReflectionAttr = 
	pdeInteriorBundleEntry[0].reflectionAttr;
}

#include "miLUTProcs.ci"
