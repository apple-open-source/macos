/* $Xorg: miNS.c,v 1.4 2001/02/09 02:04:13 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/shared/miNS.c,v 1.10 2001/12/14 19:57:39 dawes Exp $ */

#include	"mipex.h"
#include	"miNS.h"
#include	"miWks.h"
#include	"PEXErr.h"
#include	"pexUtils.h"
#include	"pexos.h"


/*  Level 4 Shared Resources  */
/*  Name Set Procedures  */

/* DELETE_DD_NS is used to delete just the dd part of a resource */

/* CHECK_DELETE is used to see if the resource has been freed and is
 * no longer being used.  If so, then the entire resource storage is
 * freed, including the ddNSResource part
 */

#define DELETE_DD_NS( phead )			\
	puDeleteList((phead)->wksRefList);	\
	puDeleteList((phead)->rendRefList);	\
	xfree(phead)

#define CHECK_DELETE(pns,phead)					\
    if ( (phead)->freeFlag && !((phead)->refCount) &&		\
	!((phead)->wksRefList->numObj) && !((phead)->rendRefList->numObj) )	\
    {								\
	DELETE_DD_NS( phead );					\
	xfree(pns);						\
    }
static ddpex4rtn    dynerr;
extern ddpex4rtn    miDealWithNSDynamics();

/*++
 |
 |  Function Name:	CreateNameSet
 |
 |  Function Description:
 |	 Handles the PEXCreateNameSet request.
 |
 |  Note(s):
 |
 --*/

ddpex43rtn
CreateNameSet(pNS)
/* in */
    diNSHandle          pNS;	  /* name set handle */
/* out */
{
    miNSHeader *pheader;

#ifdef DDTEST
    ErrorF( "\nCreateNameSet \n");
#endif

    pNS->deviceData = NULL;

    if ((pheader = (miNSHeader *) xalloc(sizeof(miNSHeader))) == NULL)
    {
	pNS->deviceData = NULL;
	return (BadAlloc);
    }
    if (!(pheader->wksRefList = puCreateList(DD_WKS)))
    {
	xfree(pheader);
	return (BadAlloc);
    }

    if (!(pheader->rendRefList = puCreateList(DD_RENDERER)))
    {
	puDeleteList(pheader->wksRefList);
	xfree(pheader);
	return (BadAlloc);
    }

    pheader->nameCount = 0;
    pheader->refCount = 0;
    pheader->freeFlag = MI_FALSE;
    MINS_EMPTY_NAMESET(pheader->names);

    pNS->deviceData = (ddPointer) pheader;
    return (Success);
}				  /* CreateNameSet */


static void
mins_rend_changes(pNS, pheader)
	diNSHandle	pNS;
	miNSHeader	*pheader;
{
	register int	i;
	ddRendererPtr	*pprend;

	pprend = (ddRendererPtr *)pheader->rendRefList->pList;
	for (i=0; i<pheader->rendRefList->numObj; i++, pprend++) 
	{
		if ((pNS == (*pprend)->ns[DD_HIGH_INCL_NS]) ||
	    	    (pNS == (*pprend)->ns[DD_HIGH_EXCL_NS]))
			(*pprend)->namesetsChanges |= PEXDynHighlightNamesetContents;
	else
		if ((pNS == (*pprend)->ns[DD_INVIS_INCL_NS]) ||
	    	    (pNS == (*pprend)->ns[DD_INVIS_EXCL_NS]))
			(*pprend)->namesetsChanges |= PEXDynInvisibilityNamesetContents;
	}
}

static void
mins_wks_changes(pNS, pheader)
	diNSHandle	pNS;
	miNSHeader	*pheader;
{
	register int 	i;
	diWKSHandle 	*phandle;
	miWksPtr	pwks;

	if (pheader->wksRefList->numObj) 
	{
		phandle = (diWKSHandle *)pheader->wksRefList->pList;
		for (i=0; i<pheader->wksRefList->numObj; i++, phandle++) 
		{	
        		pwks = (miWksPtr)((*phandle)->deviceData);
			if ((pNS == pwks->pRend->ns[DD_HIGH_INCL_NS]) ||
	    		    (pNS == pwks->pRend->ns[DD_HIGH_EXCL_NS]))
				pwks->pRend->namesetsChanges |= PEXDynHighlightNamesetContents;
			else if ((pNS == pwks->pRend->ns[DD_INVIS_INCL_NS]) ||
	    			 (pNS == pwks->pRend->ns[DD_INVIS_EXCL_NS]))
				pwks->pRend->namesetsChanges |= PEXDynInvisibilityNamesetContents;
		}
	}
}

/*++
 |
 |  Function Name:	CopyNameSet
 |
 |  Function Description:
 |	 Handles the PEXCopyNameSet request.
 |
 |  Note(s):
	both name set handles assumed to  be valid
 |
 --*/

ddpex43rtn
CopyNameSet(pSrcNS, pDestNS)
/* in */
    diNSHandle          pSrcNS;	  /* pointer to source name set */
    diNSHandle          pDestNS;  /* pointer to destination name set */
/* out */
{
    miNSHeader         *psource = (miNSHeader *) pSrcNS->deviceData;
    miNSHeader         *pdest = (miNSHeader *) pDestNS->deviceData;

#ifdef DDTEST
    ErrorF( "\nCopyNameSet \n");
#endif

    MINS_COPY_NAMESET(psource->names, pdest->names);
    pdest->nameCount = psource->nameCount;

    mins_wks_changes(pDestNS, pdest);
    mins_rend_changes(pDestNS, pdest);

    dynerr = miDealWithNSDynamics(pDestNS);
    return (Success);
}				  /* CopyNameSet */

/*++
 |
 |  Function Name:	FreeNameSet
 |
 |  Function Description:
 |	Deletes all of the storage used by the name set if there are no resources
 |	using it, otherwise it sets the free flag in the name set.  This function
 |	is registered with the resource id and handle by diPEX with AddResource.
 |
 |  Note(s):
 |
 --*/

ddpex43rtn
FreeNameSet(pNS, NSid)
/* in */
    diNSHandle          pNS;	  /* name set handle */
    ddResourceId        NSid;	  /* name set resource id */
/* out */
{
    miNSHeader *pheader = (miNSHeader *) pNS->deviceData;

#ifdef DDTEST
    ErrorF( "\nFreeNameSet \n");
#endif

    pheader->freeFlag = MI_TRUE;
    pNS->id = PEXAlreadyFreed;
    CHECK_DELETE(pNS, pheader);

    return (Success);
}				  /* FreeNameSet */

/*++
 |
 |  Function Name:	InquireNameSet
 |
 |  Function Description:
 |	 Handles the PEXGetNameSet request.
 |
 |  Note(s):
 |
 --*/

ddpex43rtn
InquireNameSet(pNS, pNumNames, pBuffer)
/* in */
    diNSHandle          pNS;	  /* name set handle */
/* out */
    ddULONG            *pNumNames;/* number of names in list */
    ddBufferPtr         pBuffer;  /* list of names */
{
    register short      i;
    register ddULONG   *pbuf;
    miNSHeader         *pheader = (miNSHeader *) pNS->deviceData;
    ddULONG             dsize;

#ifdef DDTEST
    ErrorF( "\nInquireNameSet \n");
#endif

    *pNumNames = 0;
    dsize = pheader->nameCount * sizeof(ddULONG);

    PU_CHECK_BUFFER_SIZE(pBuffer, dsize);

    *pNumNames = pheader->nameCount;
    pBuffer->dataSize = dsize;

    if (!pheader->nameCount)
	return(Success);

    pbuf = (ddULONG *)pBuffer->pBuf;
    for (i = MINS_MIN_NAME; i <= MINS_MAX_NAME; i++) 
	if ( MINS_IS_NAME_IN_SET(i, pheader->names) )
		*pbuf++ = i;

    return (Success);
}				  /* InquireNameSet */

/*++
 |
 |  Function Name:	ChangeNameSet
 |
 |  Function Description:
 |	 Handles the PEXChangeNameSet request.
 |
 |  Note(s):
 |
 --*/

ddpex43rtn
ChangeNameSet(pNS, action, numNames, pNames)
/* in */
    diNSHandle          pNS;	  /* name set handle */
    ddUSHORT            action;	  /* (add/remove/replace) */
    ddUSHORT            numNames; /* number of names in list */
    ddULONG            *pNames;	  /* list of names */
/* out */
{
    miNSHeader         *pheader = (miNSHeader *) pNS->deviceData;
    ddULONG            *pn;

#ifdef DDTEST
    ErrorF( "\nChangeNameSet \n");
#endif

    switch (action)
    {
      case PEXNSReplace:

#ifdef DDTEST
	ErrorF( "\tREPLACE\n");
#endif
	pheader->nameCount = 0;
	MINS_EMPTY_NAMESET(pheader->names);
	/* continue to add */

      case PEXNSAdd:

#ifdef DDTEST
	ErrorF( "\tADD\n");
#endif

	/* ignores any values that are out of range */
	for (pn = pNames; numNames > 0; numNames--, pn++)
		if ( MINS_VALID_NAME(*pn) &&
			!MINS_IS_NAME_IN_SET(*pn, pheader->names) )
		{
			pheader->nameCount++;
			MINS_ADD_TO_NAMESET(*pn, pheader->names);
		}
	break;

      case PEXNSRemove:

#ifdef DDTEST
	ErrorF( "\tREMOVE\n");
#endif

	for (pn = pNames; numNames > 0; numNames--, pn++)
		if ( MINS_VALID_NAME(*pn) && 
			MINS_IS_NAME_IN_SET(*pn, pheader->names) )
		{
			pheader->nameCount--;
			MINS_REMOVE_FROM_NAMESET(*pn, pheader->names);
		}
	break;

      default:
	/* better not get here */
	return (BadValue);
	break;
    }

    mins_wks_changes(pNS, pheader);
    mins_rend_changes(pNS, pheader);

    /* update the picture if necessary */
    dynerr = miDealWithNSDynamics(pNS);
    return (Success);
}				  /* ChangeNameSet */

/*++
 |
 |  Function Name:	UpdateNSRefs
 |
 |  Function Description:
 |	Utility function to update a cross-reference list in the name set. Each
 |	name set has two lists, one for workstations and one for renderers. The
 |	lists are of handles of the resources which are using the name set.
 |
 |  Note(s):
 |
 --*/

ddpex43rtn
UpdateNSRefs(pNS, pResource, which, action)
/* in */
    diNSHandle          pNS;	  /* name set handle */
    diResourceHandle    pResource;/* workstation or renderer handle */
    ddResourceType      which;	  /* wks renderer or pick device */
    ddAction            action;	  /* add or remove */
/* out */
{
    miNSHeader *pheader = (miNSHeader *) pNS->deviceData;

#ifdef DDTEST
    ErrorF( "\nUpdateNSRefs \n");
#endif

    switch (which)
    {
      case WORKSTATION_RESOURCE:
	if (action == ADD)
	{
	    if (puAddToList((ddPointer) & pResource, (ddULONG) 1,
		    pheader->wksRefList) == MI_ALLOCERR)
		return (BadAlloc);
	} else
	    puRemoveFromList((ddPointer) & pResource, pheader->wksRefList);
	break;

      case SEARCH_CONTEXT_RESOURCE:
      case PICK_RESOURCE:
	if (action == ADD)
		pheader->refCount++;
	else
		if (pheader->refCount)
			pheader->refCount--;
	break;

      case RENDERER_RESOURCE:

	if (action == ADD)
	{
	    if (puAddToList((ddPointer) & pResource, (ddULONG) 1,
		    pheader->rendRefList))
		return (BadAlloc);
	} else
	    puRemoveFromList((ddPointer) & pResource, pheader->rendRefList);

	break;

      default:			  /* better not get here */
	return (BadValue);
	break;
    }
    CHECK_DELETE(pNS, pheader);
    return (Success);
}				  /* UpdateNSRefs */

