/* $Xorg: miLUT.c,v 1.4 2001/02/09 02:04:13 xorgcvs Exp $ */
/*

Copyright 1989, 1990, 1991, 1998  The Open Group

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


Copyright 1989, 1990, 1991 by Sun Microsystems, Inc. 
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/shared/miLUT.c,v 1.11 2001/12/14 19:57:38 dawes Exp $ */

#include "miLUT.h"
#include "pexUtils.h"
#include "PEX.h"
#include "PEXErr.h"
#include "PEXprotost.h"
#include "pexos.h"


/*  Level 4 Shared Resources  */
/* General Lookup Table Procedures */

/* A flag to know if predefined entries are initialized. 
 * Most of them can't be automatically initialized because
 * they contain unions, so they are set dynamically
 * The flag is initialized in ddpexInit() 
 */
int          predef_initialized;

/* used by other LUT modules */
unsigned     colour_type_sizes[] = {
    sizeof(ddIndexedColour),
    sizeof(ddRgbFloatColour),
    sizeof(ddCieColour),
    sizeof(ddHsvColour),
    sizeof(ddHlsColour),
    sizeof(ddRgb8Colour),
    sizeof(ddRgb16Colour)
};

static	void InitializePDEs();

static unsigned     entry_size[] = {
    0,				  /* dummy so table type indexes into table */
    sizeof(miLineBundleEntry),	  /* line bundle */
    sizeof(miMarkerBundleEntry),  /* marker bundle */
    sizeof(miTextBundleEntry),	  /* text bundle */
    sizeof(miInteriorBundleEntry),/* interior bundle */
    sizeof(miEdgeBundleEntry),	  /* edge bundle */
    sizeof(miPatternEntry),	  /* pattern table */
    sizeof(miTextFontEntry),	  /* font table */
    sizeof(miColourEntry),	  /* colour table */
    sizeof(miViewEntry),	  /* view table */
    sizeof(miLightEntry),	  /* light table */
    sizeof(miDepthCueEntry),	  /* depth cue table */
    sizeof(miColourApproxEntry),  /* colour approx table */
};

/*++
 |
 |  Function Name:	CreateLUT
 |
 |  Function Description:
 |	 Handles the PEXCreateLookupTable request.
 |
 |  Note(s):
	dipex checks for bad id, drawable, table type
 |
 --*/

/* create procs are called through this table instead of through
 * ops table in lut header.  
 */
extern ddpex43rtn	LineBundleLUT_create(),
			MarkerBundleLUT_create(),
			TextBundleLUT_create(),
			InteriorBundleLUT_create(),
			EdgeBundleLUT_create(),
			PatternLUT_create(),
			TextFontLUT_create(),
			ColourLUT_create(),
			ViewLUT_create(),
			LightLUT_create(),
			DepthCueLUT_create(),
			ColourApproxLUT_create();

miOpsTableType	createLUTtable[] = {
	LineBundleLUT_create,
	MarkerBundleLUT_create,
	TextBundleLUT_create,
	InteriorBundleLUT_create,
	EdgeBundleLUT_create,
	PatternLUT_create,
	TextFontLUT_create,
	ColourLUT_create,
	ViewLUT_create,
	LightLUT_create,
	DepthCueLUT_create,
	ColourApproxLUT_create,
};

ddpex43rtn
CreateLUT(pDrawable, pLUT)
/* in */
    DrawablePtr         pDrawable;/* pointer to example drawable */
    diLUTHandle         pLUT;	  /* lut handle */
/* out */
{
    register miLUTHeader *pheader;
    ddUSHORT            LUTtype = pLUT->lutType;
    ddpex43rtn		err;

#ifdef DDTEST
    ErrorF( "\nCreateLUT %d type %d\n", pLUT->id, pLUT->lutType);
#endif

    pLUT->deviceData = NULL;

    if ((pheader = (miLUTHeader *) xalloc(sizeof(miLUTHeader))) == NULL)
	return (BadAlloc);

    /* the id and table type are already in the di resource structure */
    pheader->freeFlag = MI_FALSE;

    /* set drawable example and type which supports this drawable */
    MI_SETDRAWEXAMPLE(pDrawable, &(pheader->drawExample));
    MI_WHICHDRAW(pDrawable, pheader->drawType);

    if (!(pheader->wksRefList = puCreateList(DD_WKS)))
    {
	xfree( pheader);
	return (BadAlloc);
    }

    if (!(pheader->rendRefList = puCreateList(DD_RENDERER)))
    {
	puDeleteList(pheader->wksRefList);
	xfree(pheader);
	return (BadAlloc);
    }

    /*
     * now create the predefined entries. for now,
     * predefined entries don't depend on the drawable type.  If they every
     * do, make the pde vars a 2-d array - the first dimension based on
     * drawable type and the second one the entries for that drawable type.
     * (see InquireEnumTypeInfo for an example of this)
     */
    if (!predef_initialized)
    {
	InitializePDEs();
	predef_initialized = 1;
    }

    err = Success;
    err = createLUTtable[LUTtype-1](pLUT, pheader);
    if (err != Success)
    {
	MILUT_DESTROY_HEADER(pheader);
    }
	
    return(err);

}				  /* CreateLUT */


/*++
 |
 |  Function Name: 	FreeLUT
 |
 |  Function Description:
 |	 Frees all of the storage for the lookup table if no resource is using
 |	 it, otherwise it sets the free flag in the structure.  This is
 |	 registered with the resource id and handle by diPEX with AddResource.
 |
 |  Note(s):
	dipex checks for bad id
 |
 --*/

ddpex43rtn
FreeLUT(pLUT, LUTid)
/* in */
    diLUTHandle         pLUT;	  /* lut handle */
    ddResourceId        LUTid;	  /* lookup table resource id */
/* out */
{
	MILUT_DEFINE_HEADER(pLUT, pheader);

#ifdef DDTEST
    ErrorF( "\nFreeLUT %d type %d\n", pLUT->id, pLUT->lutType);
#endif

    pheader->freeFlag = MI_TRUE;
    pLUT->id = PEXAlreadyFreed;
    MILUT_CHECK_DESTROY(pLUT, pheader);

    return (Success);
}				  /* FreeLUT */

/*++
 |
 |  Function Name:	CopyLUT
 |
 |  Function Description:
 |	 Handles the PEXCopyLookupTable request.
 |
 |  Note(s):
	dipex checks for bad ids
 |
 --*/

#define COMPARE_DRAWABLE_EXAMPLES(Ex1, Ex2, Op) \
   ((Ex1).type Op (Ex2).type) \
&& ((Ex1).depth Op (Ex2).depth) \
&& ((Ex1).rootDepth Op (Ex2).rootDepth)

ddpex43rtn
CopyLUT(pSrcLUT, pDestLUT)
/* in */
    diLUTHandle         pSrcLUT;  /* source lookup table */
    diLUTHandle         pDestLUT; /* destination lookup table */
/* out */
{
    MILUT_DEFINE_HEADER(pSrcLUT, srcHeader);
    MILUT_DEFINE_HEADER(pDestLUT, destHeader);

#ifdef DDTEST
    ErrorF( "\nCopyLUT src %d type %d\n", pSrcLUT->id, pSrcLUT->lutType);
    ErrorF( "\nCopyLUT dest %d type %d\n", pDestLUT->id, pDestLUT->lutType);
#endif

    if (pSrcLUT->lutType != pDestLUT->lutType)
	return (BadMatch);

    /* compare the drawable examples. */
/*
 * Here's one way
 *
 *  if (! (COMPARE_DRAWABLE_EXAMPLES(srcHeader->drawExample,
 *		                     destHeader->drawExample,==))
 *
 * (see the macro definition above; it compares most of the struct members)
 * but the one below is more restrictive, I think (it probably means both
 * drawables are on the same framebuffer) and is likely safe for now.
 */
    if (srcHeader->drawExample.rootVisual != destHeader->drawExample.rootVisual)
	return (BadMatch);

    return(srcHeader->ops[MILUT_REQUEST_OP(PEX_CopyLookupTable)]
	(pSrcLUT, pDestLUT));
}				  /* CopyLUT */

/*++
 |
 |  Function Name:	InquireLUTInfo
 |
 |  Function Description:
 |	 Handles the PEXGetTableInfo request.
 |
 |  Note(s):
	dipex checks for bad drawable id and type
 |
 --*/
extern ddpex43rtn	LineBundleLUT_inq_info(),
			MarkerBundleLUT_inq_info(),
			TextBundleLUT_inq_info(),
			InteriorBundleLUT_inq_info(),
			EdgeBundleLUT_inq_info(),
			PatternLUT_inq_info(),
			TextFontLUT_inq_info(),
			ColourLUT_inq_info(),
			ViewLUT_inq_info(),
			LightLUT_inq_info(),
			DepthCueLUT_inq_info(),
			ColourApproxLUT_inq_info();

miOpsTableType	inq_info_LUTtable[] = {
	LineBundleLUT_inq_info,
	MarkerBundleLUT_inq_info,
	TextBundleLUT_inq_info,
	InteriorBundleLUT_inq_info,
	EdgeBundleLUT_inq_info,
	PatternLUT_inq_info,
	TextFontLUT_inq_info,
	ColourLUT_inq_info,
	ViewLUT_inq_info,
	LightLUT_inq_info,
	DepthCueLUT_inq_info,
	ColourApproxLUT_inq_info,
};


ddpex43rtn
InquireLUTInfo(pDrawable, LUTtype, pLUTinfo)
/* in */
    DrawablePtr         pDrawable;/* pointer to example drawable */
    ddUSHORT            LUTtype;  /* lookup table type */
/* out */
    ddTableInfo        *pLUTinfo; /* table information */
{

#ifdef DDTEST
    ErrorF( "\nInquireLUTInfo type %d\n", LUTtype);
#endif

    return (inq_info_LUTtable[LUTtype-1](pDrawable, pLUTinfo));
}				  /* InquireLUTInfo */


/*++
 |
 |  Function Name:	InquireLUTPredEntries
 |
 |  Function Description:
 |	 Handles the PEXGetPredefinedEntries request.
 |
 |  Note(s):
	dipex checks for bad drawable and type
 |
 --*/

ddpex43rtn
InquireLUTPredEntries(pDrawable, LUTtype, start, count, pNumEntries, pBuffer)
/* in */
    DrawablePtr	    pDrawable;	    /* pointer to example drawable */
    ddUSHORT	    LUTtype;	    /* table type */
    ddTableIndex    start;	    /* start index */
    ddUSHORT	    count;	    /* number of entries to return */ /* out */
    ddULONG	    *pNumEntries;   /* number of entries */
    ddBufferPtr     pBuffer;	    /* table entries */
{
    ddLUTResource   lut;
    ddpex43rtn	    err43;
    unsigned long   hdrSiz = pBuffer->pBuf - pBuffer->pHead;
    unsigned long   dataSiz = 0;
    int		    reply_size = entry_size[LUTtype] * count;
    int		    i;
    ddUSHORT	    status;	

#ifdef DDTEST
    ErrorF( "\nInquireLUTPredEntries type %d\n", LUTtype);
#endif

    *pNumEntries = 0;
    pBuffer->dataSize = 0;

    /*
     * reply_size is an upper-bound on the size of the stuff that
     * will actually be returned, so once we do this, InquireLUTEntry
     * shouldn't have to reallocate the buffer
     */
    PU_CHECK_BUFFER_SIZE(pBuffer, reply_size);

    /*
     * CreateLUT and InquireLUTEntries have a lot of smarts about 
     * predefined entries and default entries, so use them.  
     */
    lut.id = 0;
    lut.lutType = LUTtype;
    if ((err43 = CreateLUT(pDrawable, &lut)) != Success)
	return (err43);

    /* see if start and count are in range of predefined entries 
     * or if entry 0 is requested for table which doesn't use 0 
     */
#if 0
    if (( start < MILUT_PREMIN(MILUT_HEADER(&lut)) ) || 
	( (start + count - 1) > MILUT_PREMAX(MILUT_HEADER(&lut)) ) ||
        ( !start && MILUT_START_INDEX(MILUT_HEADER(&lut)) ))
    {
        MILUT_DESTROY_HEADER((miLUTHeader *) lut.deviceData);
	return(BadValue);
    }
#endif

    for (i = 0; i < count; i++)
    {
	/* call get entry op instead of calling InquireLUTEntry */
    	err43 = MILUT_HEADER(&lut)->ops[MILUT_REQUEST_OP(PEX_GetTableEntry)]
		(&lut, i + start, PEXSetValue, &status, pBuffer);

	if (err43 != Success)
	{
	    /* reset data buffer pointer */
	    pBuffer->pBuf = pBuffer->pHead + hdrSiz;
	    pBuffer->dataSize = 0;
	    return (err43);
	}

	/*
	 * move data buffer pointer to put next entry after the one just
	 * gotten
	 */
	dataSiz += pBuffer->dataSize;
	pBuffer->pBuf = pBuffer->pHead + hdrSiz + dataSiz;
    }

    /* reset data buffer pointer */
    pBuffer->pBuf = pBuffer->pHead + hdrSiz;
    pBuffer->dataSize = dataSiz;
    *pNumEntries = count;

    MILUT_DESTROY_HEADER((miLUTHeader *) lut.deviceData);

    return (Success);
}				  /* InquireLUTPredEntries */


/*++
 |
 |  Function Name:	InquireLUTIndices
 |
 |  Function Description:
 |	 Handles the PEXGetDefinedIndices request.
 |
 |  Note(s):
	dipex checks for bad lut
 |
 --*/

ddpex43rtn
InquireLUTIndices(pLUT, pNumIndices, pBuffer)
/* in */
    diLUTHandle         pLUT;	  /* lut handle */
/* out */
    ddULONG            *pNumIndices;	/* number of indices in list */
    ddBufferPtr         pBuffer;  /* list of table indices */
{
#ifdef DDTEST
    ErrorF( "\nInquireLUTIndices %d type %d\n", pLUT->id, pLUT->lutType);
#endif

    *pNumIndices = 0;
    return(MILUT_HEADER(pLUT)->ops[MILUT_REQUEST_OP(PEX_GetDefinedIndices)]
	(pLUT, pNumIndices, pBuffer));
}				  /* InquireLUTIndices */



/*++
 |
 |  Function Name:	InquireLUTEntry
 |
 |  Function Description:
 |	 Handles the PEXGetTableEntry request.
 |
 |  Note(s):
	dipex checks for bad lut
 |
 --*/

ddpex43rtn
InquireLUTEntry(pLUT, index,  valueType, pStatus, pBuffer)
/* in */
    diLUTHandle         pLUT;	  /* lut handle */
    ddTableIndex        index;	  /* index of entry to get */
    ddUSHORT		valueType;	/* SET or REALIZED */
/* out */
    ddUSHORT           *pStatus;  /* entry status */
    ddBufferPtr         pBuffer;  /* table entry */
{
#ifdef DDTEST
    ErrorF( "\nInquireLUTEntry %d type %d\n", pLUT->id, pLUT->lutType);
#endif

    *pStatus = PEXDefaultEntry;
    pBuffer->dataSize = 0;

    /* see if entry 0 is requested for table which doesn't use 0 */
    if (!index && MILUT_START_INDEX(MILUT_HEADER(pLUT)))
	return(BadValue);

    if ((valueType != PEXRealizedValue) && (valueType != PEXSetValue))
	return(BadValue);

    return(MILUT_HEADER(pLUT)->ops[MILUT_REQUEST_OP(PEX_GetTableEntry)]
	(pLUT, index, valueType, pStatus, pBuffer));
}				  /* InquireLUTEntry */


/*++
 |
 |  Function Name:	InquireLUTEntries
 |
 |  Function Description:
 |	 Handles the PEXGetTableEntries request.
 |
 |  Note(s):
	dipex checks for bad lut
 |
 --*/

ddpex43rtn
InquireLUTEntries(pLUT, start, count, valueType, pNumEntries, pBuffer)
/* in */
    diLUTHandle	    pLUT;	    /* lut handle */
    ddTableIndex    start;	    /* index of first entry to get */
    ddUSHORT	    count;	    /* number of entries requested */
    ddUSHORT	    valueType;	    /* SET or REALIZED */
/* out */
    ddULONG	    *pNumEntries;   /* number of entries in list */
    ddBufferPtr	    pBuffer;	    /* list of table entries */
{
    unsigned long   hdrSiz = pBuffer->pBuf - pBuffer->pHead;
    unsigned long   dataSiz = 0;
    int		    reply_size = entry_size[MILUT_TYPE(pLUT)] * count;
    int		    i;
    ddUSHORT	    status;	
    ddpex43rtn	    err;

    /*
     * reply_size is an upper-bound on the size of the stuff that
     * will actually be returned, so once we do this, InquireLUTEntry
     * shouldn't have to reallocate the buffer
     */

    *pNumEntries = 0;
    PU_CHECK_BUFFER_SIZE(pBuffer, reply_size);

    /* see if entry 0 is requested for table which doesn't use 0 
     * or if start + count is greater than 65535 
     */
    if ((!start && MILUT_START_INDEX(MILUT_HEADER(pLUT))) ||
        ((ddULONG)(start + count) > MILUT_MAX_INDEX))
	return(BadValue);

    if ((valueType != PEXRealizedValue) && (valueType != PEXSetValue))
	return(BadValue);

    for (i = 0; i < count; i++)
    {
	/* call get entry op instead of calling InquireLUTEntry */
    	err = MILUT_HEADER(pLUT)->ops[MILUT_REQUEST_OP(PEX_GetTableEntry)]
		(pLUT, i + start, valueType, &status, pBuffer);

	if (err != Success)
	{
	    /* reset data buffer pointer */
	    pBuffer->pBuf = pBuffer->pHead + hdrSiz;
	    pBuffer->dataSize = 0;
	    return (err);
	}

	/*
	 * move data buffer pointer to put next entry after the one just
	 * gotten
	 */
	dataSiz += pBuffer->dataSize;
	pBuffer->pBuf = pBuffer->pHead + hdrSiz + dataSiz;
    }

    /* reset data buffer pointer */
    pBuffer->pBuf = pBuffer->pHead + hdrSiz;
    pBuffer->dataSize = dataSiz;
    *pNumEntries = count;

    return (Success);
}				  /* InquireLUTEntries */


/*++
 |
 |  Function Name:	SetLUTEntries
 |
 |  Function Description:
 |	 Handles the PEXSetTableEntries request.
 |
 |  Note(s):
 |
 --*/

ddpex43rtn
SetLUTEntries(pLUT, start, numEntries, pEntries)
/* in */
    diLUTHandle         pLUT;	  /* lut handle */
    ddTableIndex        start;	  /* index of first entry to set */
    ddUSHORT            numEntries;	/* number of entries to set */
    ddPointer           pEntries; /* list of entries */
{

#ifdef DDTEST
    ErrorF( "\nSetLUTEntries %d type %d\n", pLUT->id, pLUT->lutType);
#endif

    /* see if entry 0 is requested for table which doesn't use 0 
     * or if start + count is greater than 65535 
     */
    if ((!start && MILUT_START_INDEX(MILUT_HEADER(pLUT))) ||
        ((ddULONG)(start + numEntries) > MILUT_MAX_INDEX))
	return(BadValue);

    return(MILUT_HEADER(pLUT)->ops[MILUT_REQUEST_OP(PEX_SetTableEntries)]
	(pLUT, start, numEntries, pEntries));
}				  /* SetLUTEntries */


/*++
 |
 |  Function Name:	DeleteLUTEntries
 |
 |  Function Description:
 |	 Handles the PEXDeleteTableEntries request.
 |
 |  Note(s):
 |
 --*/

#define SETSTATUS( ptr, value ) 		\
	for ( i=start; i<end; i++, (ptr)++ )	\
		(ptr)->entry_info.status = (value)

ddpex43rtn
DeleteLUTEntries(pLUT, start, numEntries)
/* in */
    diLUTHandle         pLUT;	  /* lut handle */
    ddUSHORT            start;	  /* index of first entry to delete */
    ddUSHORT            numEntries;	/* number of entries in range */
/* out */
{

#ifdef DDTEST
    ErrorF( "\nDeleteLUTEntries %d type %d\n", pLUT->id, pLUT->lutType);
#endif

    /* see if start + count is greater than 65535 
     * or if entry 0 is requested for table which doesn't use 0 
     */
    if (( (ddULONG)(numEntries + start) > MILUT_MAX_INDEX ) ||
        ( !start && MILUT_START_INDEX(MILUT_HEADER(pLUT)) ))
	return(BadValue);

    return(MILUT_HEADER(pLUT)->ops[MILUT_REQUEST_OP(PEX_DeleteTableEntries)]
	(pLUT, start, numEntries));
}				  /* DeleteLUTEntries */


/*++
 |
 |  Function Name:	UpdateLUTRefs
 |
 |  Function Description:
 |	 A utility procedure for updating the cross-reference lists in the
 |	lookup table.  The lookup table has two lists, one for renderers and
 |	one for workstations.  These lists tell which resources are using the
 |	table. Deletes the resource if it's been freed and is not referenced.
 |
 |  Note(s):
 |
 --*/

ddpex43rtn
UpdateLUTRefs(pLUT, pResource, which, action)
/* in */
    diLUTHandle         pLUT;	  /* lut handle */
    diResourceHandle    pResource;/* workstation or renderer handle */
    ddResourceType      which;	  /* workstation or renderer */
    ddAction            action;	  /* add or remove */
/* out */
{
    register miLUTHeader *pheader = (miLUTHeader *) pLUT->deviceData;

#ifdef DDTEST
    ErrorF( "\nUpdateLUTRefs %d type %d\n", pLUT->id, pLUT->lutType);
#endif

    switch (which)
    {
      case WORKSTATION_RESOURCE:
	if (action == ADD)
	{
	    if (puAddToList((ddPointer) &pResource, (ddULONG) 1, pheader->wksRefList) == MI_ALLOCERR)
		return (BadAlloc);
	} else
	    puRemoveFromList((ddPointer) &pResource, pheader->wksRefList);
	break;

      case RENDERER_RESOURCE:
	if (action == ADD)
	{
	    if (puAddToList((ddPointer) &pResource, (ddULONG) 1, pheader->rendRefList) == MI_ALLOCERR)
		return (BadAlloc);
	} else
	    puRemoveFromList((ddPointer) &pResource, pheader->rendRefList);
	break;

      default:			  /* better not get here */
	return (BadValue);
	break;
    }

    MILUT_CHECK_DESTROY(pLUT, pheader);
    return (Success);
}				  /* UpdateLUTRefs */

/*++
 |
 |  Function Name:	MatchLUTDrawable
 |
 |  Function Description:
 |	 A utility procedure for  comparing a drawable with the
	drawable example of an LUT.  Returns BadMatch if they
	are not compatible.
 |
 |  Note(s):
 |
 --*/

ddpex43rtn
MatchLUTDrawable(pLUT, pDrawable)
	diLUTHandle	pLUT;
	DrawablePtr	pDrawable;
{
    register miLUTHeader *pheader = (miLUTHeader *) pLUT->deviceData;

	if ( (pheader->drawExample.type == pDrawable->type) &&
	     (pheader->drawExample.depth == pDrawable->depth) &&
	     (pheader->drawExample.rootDepth == pDrawable->pScreen->rootDepth) &&
	     (pheader->drawExample.rootVisual == pDrawable->pScreen->rootVisual) )
		return(Success);
	else
		return(BadMatch);
}

static   void
InitializePDEs()
{
extern	void LineBundleLUT_init_pde();
extern	void MarkerBundleLUT_init_pde();
extern	void TextBundleLUT_init_pde();
extern	void InteriorBundleLUT_init_pde();
extern	void EdgeBundleLUT_init_pde();
extern	void PatternLUT_init_pde();
extern	void TextFontLUT_init_pde();
extern	void ColourLUT_init_pde();
extern	void ViewLUT_init_pde();
extern	void LightLUT_init_pde();
extern	void DepthCueLUT_init_pde();
extern	void ColourApproxLUT_init_pde();

    LineBundleLUT_init_pde();
    MarkerBundleLUT_init_pde();
    TextBundleLUT_init_pde();
    InteriorBundleLUT_init_pde();
    EdgeBundleLUT_init_pde();
    PatternLUT_init_pde();
    TextFontLUT_init_pde();
    ColourLUT_init_pde();
    ViewLUT_init_pde();
    LightLUT_init_pde();
    DepthCueLUT_init_pde();
    ColourApproxLUT_init_pde();
}

extern ddpex43rtn	LineBundleLUT_inq_entry_address(),
			MarkerBundleLUT_inq_entry_address(),
			TextBundleLUT_inq_entry_address(),
			InteriorBundleLUT_inq_entry_address(),
			EdgeBundleLUT_inq_entry_address(),
			PatternLUT_inq_entry_address(),
			TextFontLUT_inq_entry_address(),
			ColourLUT_inq_entry_address(),
			ViewLUT_inq_entry_address(),
			LightLUT_inq_entry_address(),
			DepthCueLUT_inq_entry_address(),
			ColourApproxLUT_inq_entry_address();

miOpsTableType	inq_entry_address_LUTtable[] = {
	LineBundleLUT_inq_entry_address,
	MarkerBundleLUT_inq_entry_address,
	TextBundleLUT_inq_entry_address,
	InteriorBundleLUT_inq_entry_address,
	EdgeBundleLUT_inq_entry_address,
	PatternLUT_inq_entry_address,
	TextFontLUT_inq_entry_address,
	ColourLUT_inq_entry_address,
	ViewLUT_inq_entry_address,
	LightLUT_inq_entry_address,
	DepthCueLUT_inq_entry_address,
	ColourApproxLUT_inq_entry_address,
};

ddpex43rtn
InquireLUTEntryAddress(LUTtype, pLUT, index,  pStatus, ppEntry)
/* in */
    ddUSHORT            LUTtype;  /* lookup table type */
    diLUTHandle         pLUT;	  /* lut handle */
    ddTableIndex        index;	  /* index of entry to get */
/* out */
    ddUSHORT           *pStatus;  /* entry status */
    ddPointer         *ppEntry;  /* table entry */
{
#ifdef DDTEST
    ErrorF( "\nInquireLUTEntryAddress %d type \n", LUTtype);
#endif

    return(inq_entry_address_LUTtable[LUTtype-1] 
	(LUTtype, pLUT, index, pStatus, ppEntry));
}				  /* InquireLUTEntryAddress */

