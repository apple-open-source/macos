/* $Xorg: miPattLUT.c,v 1.4 2001/02/09 02:04:13 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/shared/miPattLUT.c,v 1.9 2001/12/14 19:57:39 dawes Exp $ */

#include "miLUT.h"
#include "miWks.h"
#include "miInfo.h"
#include "PEXErr.h"
#include "PEXprotost.h"
#include "pexos.h"


/*  Level 4 Shared Resources  */
/* Lookup Table Procedures */

/* pattern tables don't have to be implemented. define PEX_PATTERN_LUT
 * here if they are
 */
/* PEX-SI officially doesn't implement pattern tables, but the
 * API doesn't handle it correctly yet if they aren't implemented,
 * so pretend that they are
 */
#define	PEX_PATTERN_LUT

extern	unsigned	colour_type_sizes[];	/* in miLUT.c */

/* definitions used by miLUTProcs.ci */
#define	LUT_TYPE	PEXPatternLUT

/* devPriv data structure */
#define	DD_LUT_ENTRY_STR	ddPatternEntry 
/* table entry data structure */
#define	MI_LUT_ENTRY_STR	miPatternEntry
/* pex data */
#define	PEX_LUT_ENTRY_STR	pexPatternEntry

#define LUT_REND_DYN_BIT	PEXDynPatternTableContents

#define LUT_START_INDEX          1
#define LUT_DEFAULT_INDEX        1
#define LUT_0_DEFINABLE_ENTRIES  0
#define LUT_0_NUM_PREDEFINED     0
#define LUT_0_PREDEFINED_MIN     0
#define LUT_0_PREDEFINED_MAX     0

#define LUT_TABLE_START(pheader)	(pheader)->plut.pattern

#define	DYNAMIC		PATTERN_TABLE_DYNAMIC

/* predefined entries table: change this to work with your devPriv data */

#define	PDE_NUMX	1
#define	PDE_NUMY	1

#if LUT_0_NUM_PREDEFINED
static	DD_LUT_ENTRY_STR	pdePatternEntry[LUT_0_NUM_PREDEFINED];
static	ddIndexedColour		pdeColours[PDE_NUMX][PDE_NUMY] = {};
#else	/* use dummies so things compile */
static	DD_LUT_ENTRY_STR	pdePatternEntry[1];
static	ddIndexedColour		pdeColours[PDE_NUMX][PDE_NUMY] = {1};
#endif	/* LUT_0_NUM_PREDEFINED */

#define	LUT_PDE_ENTRIES		pdePatternEntry[0]
#define	LUT_SET_PDE_ENTRY(pentry, pdeentry)		\
	(pentry)->entry = *(pdeentry);		\
	(pentry)->entry.colours.indexed = (ddIndexedColour *)xalloc(	\
	(pentry)->entry.numx * (pentry)->entry.numy * sizeof(colour_type_sizes[(int)(pentry)->entry.colourType]));	\
	mibcopy((pdeentry)->colours.indexed, (pentry)->entry.colours.indexed, \
	(pentry)->entry.numx * (pentry)->entry.numy * sizeof(colour_type_sizes[(int)(pentry)->entry.colourType]))

/* predefined entry 0 is set to the default values 
 * change the XXX_DEFAULT_YYY macros below to use something else
 * if you don't want the default values defined in the pde table
 */
#define PDE_DEFAULT_ENTRY_NUM	0
#define	LUT_DEFAULT_VALUES	pdePatternEntry[PDE_DEFAULT_ENTRY_NUM]
#define	LUT_SET_DEFAULT_VALUES(pentry)		\
		(pentry)->entry.numx = 0;	\
		(pentry)->entry.numy = 0

/* which procedure definitions in miLUTProcs.h to use and their names
 * take out USE flags if you're defining those procs in here
 * but leave the name definitions
 */

/* if pattern tables are implemented, use these procedures in 
 * miLUTProcs.h; the other procs are defined below.
 * if pattern tables aren't implemented, then the create proc
 * returns an error and so there won't be a pattern table
 * (dipex will get bad XID for other requests on the table)
 */
#define LUT_USE_FREE
#define LUT_USE_INQ_PREDEF
#define LUT_USE_INQ_ENTRIES
/* #define LUT_USE_COPY */
#define LUT_USE_INQ_INFO
#define LUT_USE_INQ_IND
#define LUT_USE_INQ_ENTRY
#define LUT_USE_SET_ENTRIES
#define LUT_USE_DEL_ENTRIES
#define LUT_USE_INQ_ENTRY_ADDRESS
/* #define LUT_USE_CREATE  */
#define LUT_USE_MOD_CALL_BACK

/* these three are redefined in miLUTProcs.h
#define LUT_FREE	PatternLUT_free
#define LUT_INQ_PREDEF	PatternLUT_inq_predef
#define LUT_INQ_ENTRIES	PatternLUT_inq_entries
*/
#define LUT_COPY	PatternLUT_copy
#define LUT_INQ_INFO	PatternLUT_inq_info
#define LUT_INQ_IND	PatternLUT_inq_ind
#define LUT_INQ_ENTRY	PatternLUT_inq_entry
#define LUT_SET_ENTRIES	PatternLUT_set_entries
#define LUT_DEL_ENTRIES	PatternLUT_del_entries
#define LUT_INQ_ENTRY_ADDRESS	PatternLUT_inq_entry_address
#define LUT_CREATE	PatternLUT_create
#define LUT_ENTRY_CHECK	PatternLUT_entry_check
#define LUT_COPY_PEX_MI	PatternLUT_copy_pex_to_mi
#define LUT_COPY_MI_PEX	PatternLUT_copy_mi_to_pex
#define LUT_MOD_CALL_BACK	PatternLUT_mod_call_back

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

	mibcopy(pdev_entry->colours.indexed, pb, 
		pdev_entry->numx * pdev_entry->numy * colour_type_sizes[(int)pdev_entry->colourType]);

	pb += pdev_entry->numx * pdev_entry->numy * colour_type_sizes[(int)pdev_entry->colourType];
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
	ddULONG		xy1, xy2;
	ddSHORT		colourType;

	xy1 = pentry->entry.numx * pentry->entry.numy;
	colourType = pentry->entry.colourType;

	mibcopy(ps, &(pentry->entry), sizeof(PEX_LUT_ENTRY_STR));

	xy2 = pentry->entry.numx * pentry->entry.numy;

	ps+= sizeof(PEX_LUT_ENTRY_STR);

	if ((xy1 * colour_type_sizes[(int)colourType]) <
	    (xy2 * colour_type_sizes[(int)pentry->entry.colourType]))
	{
		pentry->entry.colours.indexed = (ddIndexedColour *)xrealloc(
			pentry->entry.colours.indexed, 
			xy2 * colour_type_sizes[(int)pentry->entry.colourType]);
		if (!pentry->entry.colours.indexed)
			return(BadAlloc);
	}

	mibcopy(ps, pentry->entry.colours.indexed, 
		xy2 * colour_type_sizes[(int)pentry->entry.colourType]);

	ps += xy2 * colour_type_sizes[(int)pentry->entry.colourType];
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
	ddULONG		xy;

        /* colours: only accept supported colour types */
        if (MI_BADCOLOURTYPE((*ppPexEntry)->colourType))
                return(PEXERR(PEXColourTypeError));

	xy = (*ppPexEntry)->numx * (*ppPexEntry)->numy;

	pe += sizeof(PEX_LUT_ENTRY_STR) + 
		xy * colour_type_sizes[(int)(*ppPexEntry)->colourType];
	*ppPexEntry = (PEX_LUT_ENTRY_STR *)pe;

	return(Success);
}

void
PatternLUT_init_pde()
{
#ifdef PEX_PATTERN_LUT
#if	LUT_0_NUM_PREDEFINED
	pdePatternEntry[0].colourType = PEXIndexedColour;
	pdePatternEntry[0].numx = PDE_NUMX;
	pdePatternEntry[0].numy = PDE_NUMY;
	pdePatternEntry[0].colours.indexed = &pdeColours[0][0];
#endif /* LUT_0_NUM_PREDEFINED */
#endif /* PEX_PATTERN_LUT */
}

#include "miLUTProcs.ci"

ddpex43rtn
LUT_COPY (pSrcLUT, pDestLUT)
/* in */
    diLUTHandle         pSrcLUT;  /* source lookup table */
    diLUTHandle         pDestLUT; /* destination lookup table */
/* out */
{
    MILUT_DEFINE_HEADER(pSrcLUT, srcHeader);
    MILUT_DEFINE_HEADER(pDestLUT, destHeader);
    register int        i;
    MI_LUT_ENTRY_STR       *pDestEntry;
    MI_LUT_ENTRY_STR       *pSrcEntry;
    ddpex43rtn		err;
    ddIndexedColour	*psaveColours;
    ddULONG		xy;
    ddSHORT		saveType;

#ifdef DDTEST
    ErrorF( "\ncopy src lut %d type %d\n", pSrcLUT->id, pSrcLUT->lutType);
    ErrorF( "\ncopy dest lut %d type %d\n", pDestLUT->id, pDestLUT->lutType);
#endif

    /* set all entries to undefined */
    pDestEntry = LUT_TABLE_START(destHeader);
    MILUT_SET_STATUS(pDestEntry, MILUT_ALLOC_ENTS(destHeader), MILUT_UNDEFINED, MI_FALSE);

    /* copy entries */
    pDestEntry = LUT_TABLE_START(destHeader);
    pSrcEntry = LUT_TABLE_START(srcHeader);

    for (i = MILUT_START_INDEX(srcHeader); i < MILUT_ALLOC_ENTS(srcHeader); i++)
    {
    	xy = pDestEntry->entry.numx * pDestEntry->entry.numy;
    	psaveColours = pDestEntry->entry.colours.indexed; 
    	saveType = pDestEntry->entry.colourType;

    	mibcopy(pSrcEntry,  pDestEntry, sizeof(miPatternEntry));

    	/* copy colours */
	pDestEntry->entry.colours.indexed = psaveColours;
	if ( (xy * colour_type_sizes[(int)saveType]) <
		(pSrcEntry->entry.numx * pSrcEntry->entry.numy * 
		 colour_type_sizes[(int)pSrcEntry->entry.colourType]) )
	{
		pDestEntry->entry.colours.indexed = 
			(ddIndexedColour *)xrealloc(pDestEntry->entry.colours.indexed,
			(pSrcEntry->entry.numx * pSrcEntry->entry.numy * 
			 colour_type_sizes[(int)pSrcEntry->entry.colourType]));
		if (!pDestEntry->entry.colours.indexed)
			return(BadAlloc);
	}
	mibcopy( pSrcEntry->entry.colours.indexed, pDestEntry->entry.colours.indexed, 
		(pSrcEntry->entry.numx * pSrcEntry->entry.numy * 
		 colour_type_sizes[(int)pSrcEntry->entry.colourType]) );
	pSrcEntry++;
	pDestEntry++;
    }

    MILUT_NUM_ENTS(destHeader) = MILUT_NUM_ENTS(srcHeader);

   err = Success;

#ifdef	DYNAMIC
       if (destHeader->wksRefList->numObj)
	       err = miDealWithDynamics( DYNAMIC, destHeader->wksRefList );
#endif	/* DYNAMIC */

    err =  destHeader->ops[MILUT_REQUEST_OP(milut_mod_call_back)](pDestLUT,
		 MILUT_START_INDEX(destHeader), MILUT_DEF_ENTS(destHeader), 
		 MILUT_COPY_MOD);
    /* check err here if your call back proc can return an error */
    if (err != Success) return(err);

    return (err);
}                             

ddpex43rtn
LUT_CREATE (pLUT, pheader)
/* in */
    diLUTHandle         pLUT;	  /* lut handle */
    miLUTHeader 	*pheader; /* lut header */
/* out */
{
    register int        i;
    MI_LUT_ENTRY_STR	*pentry;
    DD_LUT_ENTRY_STR	*pdeentry;

#ifdef DDTEST
    ErrorF( "\ncreate lut %d type %d\n", pLUT->id, pLUT->lutType);
#endif

#ifndef	PEX_PATTERN_LUT
	return(PEXERR(PEXLookupTableError));
#else

    MILUT_START_INDEX(pheader) = LUT_START_INDEX;
    MILUT_DEFAULT_INDEX(pheader) = LUT_DEFAULT_INDEX;
    MILUT_NUM_ENTS(pheader) = 0;
    SET_TABLE_INFO( pheader->drawType, &(pheader->tableInfo) );

    if (MILUT_ALLOC_ENTS(pheader) == 0)
    {
      LUT_TABLE_START(pheader) = NULL;
    }
    else if ((LUT_TABLE_START(pheader) = (MI_LUT_ENTRY_STR *)
		xalloc(MILUT_ALLOC_ENTS(pheader) * sizeof(MI_LUT_ENTRY_STR))) 
		== NULL)
    {
	MILUT_DESTROY_HEADER(pheader);
	return(BadAlloc);
    }

    pentry = LUT_TABLE_START(pheader);
    MILUT_SET_STATUS(pentry, MILUT_ALLOC_ENTS(pheader), MILUT_UNDEFINED, MI_TRUE);

    pentry = LUT_TABLE_START(pheader);
    for (i=0; i<MILUT_ALLOC_ENTS(pheader); i++, pentry++)
    {
	pentry->entry.numx=0; pentry->entry.numy=0; 
	pentry->entry.colourType=0;
	pentry->entry.colours.indexed = (ddIndexedColour *)NULL;
    }

    /* if there are predefined entries, put them in */
    if (MILUT_PRENUM(pheader))
    {
	pentry = LUT_TABLE_START(pheader) + MILUT_PREMIN(pheader);
	pdeentry = &(LUT_PDE_ENTRIES);

        for (i=MILUT_PREMIN(pheader); 
		i<=MILUT_PREMAX(pheader); i++, pentry++, pdeentry++) 
	{
                pentry->entry_info.status = MILUT_PREDEFINED;
                pentry->entry_info.index = i;
                pentry->entry = *pdeentry;
		pentry->entry.colours.indexed = (ddIndexedColour *)xalloc(
			pentry->entry.numx * pentry->entry.numy * sizeof(colour_type_sizes[(int)pentry->entry.colourType]));
		mibcopy(pdeentry->colours.indexed, pentry->entry.colours.indexed, 
			pentry->entry.numx * pentry->entry.numy * sizeof(colour_type_sizes[(int)pentry->entry.colourType]));
		pheader->numDefined++;
	}
    }

    pheader->ops[MILUT_REQUEST_OP(PEX_CreateLookupTable)] = LUT_CREATE;
    pheader->ops[MILUT_REQUEST_OP(PEX_CopyLookupTable)] = LUT_COPY;
    pheader->ops[MILUT_REQUEST_OP(PEX_FreeLookupTable)] = LUT_FREE;
    pheader->ops[MILUT_REQUEST_OP(PEX_GetTableInfo)] = LUT_INQ_INFO;
    pheader->ops[MILUT_REQUEST_OP(PEX_GetPredefinedEntries)] = LUT_INQ_PREDEF;
    pheader->ops[MILUT_REQUEST_OP(PEX_GetDefinedIndices)] = LUT_INQ_IND;
    pheader->ops[MILUT_REQUEST_OP(PEX_GetTableEntry)] = LUT_INQ_ENTRY;
    pheader->ops[MILUT_REQUEST_OP(PEX_GetTableEntries)] = LUT_INQ_ENTRIES;
    pheader->ops[MILUT_REQUEST_OP(PEX_SetTableEntries)] = LUT_SET_ENTRIES;
    pheader->ops[MILUT_REQUEST_OP(PEX_DeleteTableEntries)] = LUT_DEL_ENTRIES;
    pheader->ops[MILUT_REQUEST_OP(milut_InquireEntryAddress)] = LUT_INQ_ENTRY_ADDRESS;
    pheader->ops[MILUT_REQUEST_OP(milut_entry_check)] = LUT_ENTRY_CHECK;
    pheader->ops[MILUT_REQUEST_OP(milut_copy_pex_to_mi)] = LUT_COPY_PEX_MI;
    pheader->ops[MILUT_REQUEST_OP(milut_copy_mi_to_pex)] = LUT_COPY_MI_PEX;

    pLUT->deviceData = (ddPointer) pheader;
    return (Success);
#endif 	/* PEX_PATTERN_LUT */
}

