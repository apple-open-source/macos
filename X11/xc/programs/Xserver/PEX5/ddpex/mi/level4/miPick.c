/* $Xorg: miPick.c,v 1.4 2001/02/09 02:04:11 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level4/miPick.c,v 1.9 2001/12/14 19:57:35 dawes Exp $ */

#include "miWks.h"		/* miPickMeasureStr is defined in here */
#include "miNS.h"
#include "miStruct.h"
#include "miStrMacro.h"
#include "pexUtils.h"
#include "PEXErr.h"
#include "pexExtract.h"
#include "pexos.h"


extern ddpex4rtn    UpdateStructRefs();
extern ddpex43rtn   UpdateNSRefs();
extern ddpex3rtn    BeginStructure();
extern ddpex3rtn    EndStructure();
static unsigned char * copy_pick_path_to_buffer();
void	    path_update_struct_refs();

/*  Level 4 Workstation Support */
/*  picking Procedures  */

/*++
 |
 |  Function Name:	InquirePickDevice
 |
 |  Function Description:
 |	 Handles the PEXGetPickDevice request.
 |
 |  Note(s):
	don't need pNumItems anymore, but it's not worth changing
	the interface for now (post alpha release)
 |
 --*/

/* depends on mask, num, and dsize being declared */
#define	COUNTBYTES(type,bytes)		\
	if (mask & (type))		\
	{				\
		num++;			\
		dsize += (bytes);	\
	}

ddpex4rtn
InquirePickDevice(pWKS, devType, mask, pNumItems, pBuffer)
/* in */
    diWKSHandle     pWKS;	/* workstation handle */
    ddEnumTypeIndex devType;	/* pick device type */
    ddBitmask       mask;	/* item bit mask */
/* out */
    ddULONG	*pNumItems;	/* number of items in list */
    ddBufferPtr     pBuffer;	/* list of items */
{

    ddULONG	 num, dsize;
    int	     dev_index;
    register ddPointer pbuf;

    miPickDevice   *pPickDevice;

#ifdef DDTEST
    ErrorF(" InquirePickDevice\n");
#endif

    /* convert devType to index into devices array */
    MIWKS_PICK_DEV_INDEX(dev_index, devType);

    pPickDevice = &(((miWksPtr) pWKS->deviceData)->devices[dev_index]);
    *pNumItems = 0;

    num = 0;
    dsize = 0;
    COUNTBYTES(PEXPDPickStatus, 4);
    COUNTBYTES(PEXPDPickPath, 4 + (pPickDevice->path->numObj * 12));
    COUNTBYTES(PEXPDPickPathOrder, 4);
    COUNTBYTES(PEXPDPickIncl, 4);
    COUNTBYTES(PEXPDPickExcl, 4);
    if (mask & PEXPDPickDataRec)
	switch (dev_index) {
	    case 0:
		num++;
		dsize += MIWKS_SIZE_DATA_REC_1;
		break;
	    case 1:
		num++;
		dsize += MIWKS_SIZE_DATA_REC_2;
		break;
	}
    COUNTBYTES(PEXPDPickPromptEchoType, 4);
    COUNTBYTES(PEXPDPickEchoVolume, sizeof(ddViewport));
    COUNTBYTES(PEXPDPickEchoSwitch, 4);

    /* Check the buffer size, and realloc if needed. */
    PU_CHECK_BUFFER_SIZE(pBuffer, dsize);

    *pNumItems = num;
    pBuffer->dataSize = dsize;
    pbuf = pBuffer->pBuf;

    if (mask & PEXPDPickStatus) {
	PACK_CARD32(pPickDevice->status, pbuf);
    }

    if (mask & PEXPDPickPath) {
	PACK_CARD32(pPickDevice->path->numObj, pbuf);
	pbuf = copy_pick_path_to_buffer(pPickDevice->path, pbuf);
    }

    if (mask & PEXPDPickPathOrder) {
	PACK_CARD32(pPickDevice->pathOrder, pbuf);
    }

    if (mask & PEXPDPickIncl) {
	ddULONG	 nsid;
	if (pPickDevice->inclusion)
	    nsid = pPickDevice->inclusion->id;
	else
	    nsid = 0;
	PACK_CARD32(nsid, pbuf);
    }

    if (mask & PEXPDPickExcl) {
	ddULONG	 nsid;
	if (pPickDevice->inclusion)
	    nsid = pPickDevice->exclusion->id;
	else
	    nsid = 0;
	PACK_CARD32(nsid, pbuf);
    }

    /*
     * no data recs are defined, so skip this - noone should expect to
     * get data for this 
    */
     if (mask & PEXPDPickDataRec) {
	/* Sun says no data records defined, this is dummy code
	* switch (dev_index) { 
	*    case 0: bcopy((char *) &(MIWKS_PICK_DATA_REC_1(pPickDevice)),
	*		   (char *) pbuf, MIWKS_SIZE_DATA_REC_1); 
	*	     pbuf += MIWKS_SIZE_DATA_REC_1; 
	*	     break; 
	*    case 1: bcopy((char *) &(MIWKS_PICK_DATA_REC_2(pPickDevice)), 
	*		   (char *) pbuf, MIWKS_SIZE_DATA_REC_2); 
	*	     pbuf += MIWKS_SIZE_DATA_REC_2; 
	*	     break; 
	* }
	*/
	/* if for some reason this bitflag is set return length
	   of zero bytes
	*/
	PACK_CARD32(0,pbuf);
     }
       
    if (mask & PEXPDPickPromptEchoType) {
	PACK_CARD32(pPickDevice->pet, pbuf);
    }
    if (mask & PEXPDPickEchoVolume) {
	PACK_STRUCT(ddViewport,&(pPickDevice->echoVolume),pbuf);
    }
    if (mask & PEXPDPickEchoSwitch) {
	PACK_CARD32(pPickDevice->echoSwitch,pbuf);
    }
    return (Success);
}				/* InquirePickDevice */

/*++
 |
 |  Function Name:	ChangePickDevice
 |
 |  Function Description:
 |	 Handles the PEXChangePickDevice request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
ChangePickDevice(pWKS, devType, mask, pItems)
/* in */
    diWKSHandle     pWKS;	/* workstation handle */
    ddEnumTypeIndex devType;/* pick device type */
    ddBitmask       mask;	/* item bit mask */
    ddPointer       pItems;	/* list of items */
{

    register ddPointer	pbuf;
    int			dev_index;
    miPickDevice	*pPickDevice;
    CARD16              pickStatus;
    CARD16              pickPathOrder;
    CARD16              pickEchoSwitch;
    INT16               pickPromptEchoType;
    ddULONG		numPickPath;
    ddpex4rtn		err;
    extern ddpex4rtn	ValidateStructurePath();

#ifdef DDTEST
    ErrorF(" ChangePickDevice\n");
#endif

    /* convert devType to index into devices array */
    MIWKS_PICK_DEV_INDEX(dev_index, devType);

    pPickDevice = &(((miWksPtr) pWKS->deviceData)->devices[dev_index]);

    pbuf = pItems;

    /* go through and check for errors first */
    if (mask & PEXPDPickStatus) {
	EXTRACT_CARD16_FROM_4B(pickStatus,pbuf);
	if (!((pickStatus == PEXNoPick) || (pickStatus == PEXOk)))
	    return (BadValue);
    }

    if (mask & PEXPDPickPath) {
	/* valid path is checked when it's set */
	EXTRACT_CARD32(numPickPath,pbuf);
	SKIP_STRUCT(pbuf,numPickPath,ddPickPath);
    }

    if (mask & PEXPDPickPathOrder) {
	EXTRACT_CARD16_FROM_4B(pickPathOrder,pbuf);
	if (!((pickPathOrder == PEXTopFirst)||(pickPathOrder == PEXBottomFirst)))
	    return (BadValue);
    }
    if (mask & PEXPDPickIncl)
	SKIP_PADDING(pbuf,4);

    if (mask & PEXPDPickExcl)
	SKIP_PADDING(pbuf,4);

    /*
     * no data recs are defined, so skip the bytes if they're there  
     */
     if (mask & PEXPDPickDataRec) {
	CARD32 i, skip;
	EXTRACT_CARD32(i,pbuf);
	skip = (i+3)/4;
	SKIP_PADDING(pbuf,skip);
     }

    if (mask & PEXPDPickPromptEchoType) {
	EXTRACT_INT16_FROM_4B(pickPromptEchoType,pbuf);
	switch (pickPromptEchoType) {
	    case PEXEchoPrimitive:
	    case PEXEchoStructure:
	    case PEXEchoNetwork:
		break;
	    default:
		return (BadValue);
	}
    }

    if (mask & PEXPDPickEchoVolume)
	SKIP_PADDING(pbuf,sizeof(ddViewport));

    if (mask & PEXPDPickEchoSwitch) {
	EXTRACT_CARD16_FROM_4B(pickEchoSwitch,pbuf);
	if (!((pickEchoSwitch == PEXOff) || (pickEchoSwitch == PEXOn)))
	    return (BadValue);
    }

    /* now set the values */
    pbuf = pItems;

    if (mask & PEXPDPickStatus) {
	pPickDevice->status = pickStatus;
	SKIP_PADDING(pbuf,4);
    }

    if (mask & PEXPDPickPath) {
	SKIP_PADDING(pbuf,4);

	/* before putting this path in, remove structure ref
	 * count of current path
	 */
	if (pPickDevice->path->numObj)
	    path_update_struct_refs(pPickDevice->path, (diResourceHandle) NULL,
				    PICK_RESOURCE, REMOVE);

	PU_EMPTY_LIST(pPickDevice->path);

	/* dipex changes struct ids to handles */
	puAddToList((ddPointer) pbuf, numPickPath, pPickDevice->path);
	SKIP_STRUCT(pbuf,numPickPath,ddPickPath);

	/*
	 * now go through the path and update the structures's ref. count
	 */
	path_update_struct_refs(    pPickDevice->path, (diResourceHandle) NULL,
				    PICK_RESOURCE, ADD);
	/* make sure it's a valid path */
	err = ValidateStructurePath(pPickDevice->path);
	if (err != Success)
	    return (err);
    }

    if (mask & PEXPDPickPathOrder) {
	pPickDevice->pathOrder = pickPathOrder;
	SKIP_PADDING(pbuf,4);
    }

    if (mask & PEXPDPickIncl) {
	/*
	 * dipex looked up the nameset handle and passed it to here
	 */
	if (pPickDevice->inclusion != *(diNSHandle *)pbuf) {
	    if (pPickDevice->inclusion)
		UpdateNSRefs(	pPickDevice->inclusion, (diResourceHandle) NULL,
				PICK_RESOURCE, REMOVE);

	    pPickDevice->inclusion = *(diNSHandle *) pbuf;
	    UpdateNSRefs(	pPickDevice->inclusion, (diResourceHandle) NULL,
				PICK_RESOURCE, ADD);
	}
	SKIP_PADDING(pbuf,4);
    }

    if (mask & PEXPDPickExcl) {
	/*
	 * dipex looked up the nameset handle and passed it to here
	 */
	if (pPickDevice->exclusion != *(diNSHandle *)pbuf) {
	    if (pPickDevice->exclusion)
		UpdateNSRefs(	pPickDevice->exclusion, (diResourceHandle) NULL,
				PICK_RESOURCE, REMOVE);

	    pPickDevice->exclusion = *(diNSHandle *)pbuf;
	    UpdateNSRefs(	pPickDevice->exclusion, (diResourceHandle) NULL,
				    PICK_RESOURCE, ADD);
	}
	SKIP_PADDING(pbuf,4);
    }

    /*
     * no data recs are defined, so skip this - there'd better not be any
     * data there for these 
    */
     if (mask & PEXPDPickDataRec) { 
	/* This is dummy code from Sun, 
	 * I am adding code to skip bytes if present - JSH 
	 * switch (dev_index) { case 0: bcopy((char *) pbuf, (char *)
	 * &(MIWKS_PICK_DATA_REC_1(pPickDevice)), MIWKS_SIZE_DATA_REC_1);
	 * pbuf += MIWKS_SIZE_DATA_REC_1; break; case 1: bcopy((char *) pbuf,
	 * (char *) &(MIWKS_PICK_DATA_REC_2(pPickDevice)),
	 * MIWKS_SIZE_DATA_REC_2); pbuf += MIWKS_SIZE_DATA_REC_2; break; } 
	*/

	CARD32 i, skip;
	EXTRACT_CARD32(i,pbuf);
	skip = (i+3)/4;
	SKIP_PADDING(pbuf,skip);
    }

    if (mask & PEXPDPickPromptEchoType) {
	pPickDevice->pet = pickPromptEchoType;
	SKIP_PADDING(pbuf,4);
    }

    if (mask & PEXPDPickEchoVolume) {
	EXTRACT_STRUCT(1, ddViewport, &(pPickDevice->echoVolume), pbuf);
    }

    if (mask & PEXPDPickEchoSwitch) {
	pPickDevice->echoSwitch = pickEchoSwitch;
    }

    return (Success);
}				/* ChangePickDevice */

/*++
 |
 |  Function Name:	UpdatePickMeasure
 |
 |  Function Description:
 |	 Handles the PEXUpdatePickMeasure request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
UpdatePickMeasure(pPM, size, pInput)
/* in */
    diPMHandle	pPM;	/* pick measure */
    ddULONG	size;	/* size of input record */
    ddPointer   pInput;	/* input record */
/* out */
{
    miPickMeasureStr	    *ppm = (miPickMeasureStr *) pPM->deviceData;
    miWksStr		    *pwks = (miWksStr *) ppm->pWks->deviceData;
    MIWKS_PM_INPUT_STR_1    *pIn1 = (MIWKS_PM_INPUT_STR_1 *)pInput;
    MIWKS_PM_INPUT_STR_2    *pIn2 = (MIWKS_PM_INPUT_STR_2 *)pInput;
    miTraverserState	    trav_state;
    register ddOrdStruct    *pos;
    diStructHandle	    pstr;
    ddULONG		    start_el;
    ddULONG		    num_els;
    ddpex4rtn		    err;
    extern ddpex3rtn EndPicking();
    extern ddpex4rtn traverser();

#ifdef DDTEST
    ErrorF(" UpdatePickMeasure\n");
#endif

    if ((pwks->pRend->pDrawable == NULL) || 
	(pwks->pRend->drawableId == PEXAlreadyFreed))
		return (BadDrawable);

    if (!pwks->postedStructs.numStructs)
	return (Success);

    switch (ppm->type) {
	case PEXPickDeviceDC_HitBox:
	    MIWKS_PM_INPUT_REC_1(ppm) = *pIn1;
	    break;
	case PEXPickDeviceNPC_HitVolume:
	    MIWKS_PM_INPUT_REC_2(ppm) = *pIn2;
	    break;
	}
    ppm->status = PEXNoPick;

    trav_state.exec_str_flag = ES_YES;
    trav_state.p_curr_pick_el = (ddPickPath *) NULL;
    trav_state.p_curr_sc_el = (ddElementRef *) NULL;
    /* set to traverse all posted structs */
    pos = pwks->postedStructs.postruct;
    pos = pos->next;
    pstr = pos->pstruct;
    start_el = 1;
    num_els = MISTR_NUM_EL((miStructPtr) pstr->deviceData);


    BeginPicking(pwks->pRend, pPM);

    /* traverse posted structs */
    do {
	/* reset for each structure */
	trav_state.max_depth = 0;
	trav_state.pickId = 0;
        trav_state.ROCoffset =  0;

	if (MISTR_NUM_EL((miStructPtr) pstr->deviceData)) {
	    BeginStructure(pwks->pRend, pstr->id);

	    err = traverser(	pwks->pRend, pstr, start_el, num_els, pPM,
				(ddSCStr *)NULL, &trav_state);

	    EndStructure(pwks->pRend);
	}
	if (pos)
	    if (pos = pos->next) {
	    	pstr = pos->pstruct;
	    	num_els = MISTR_NUM_EL((miStructPtr) pstr->deviceData);
		}
    } while (pos);

    EndPicking(pwks->pRend);

    if (ppm->status == PEXOk) {
      /* now, update the structure ref counts */
        path_update_struct_refs(ppm->path, (diResourceHandle) NULL,
				PICK_RESOURCE, ADD);
    } else {
	/* reset pick path??? */
	return(err);
    }

    return (Success);
}				/* UpdatePickMeasure */

/*++
 |
 |  Function Name:	CreatePickMeasure
 |
 |  Function Description:
 |	Handle the PEXCreatePickMeasure request
 |
 |  Note(s):
 |
 --*/

extern void     UpdateWksRefs();

ddpex4rtn
CreatePickMeasure(pWKS, devType, pPM)
/* in */
    diWKSHandle     pWKS;
    ddEnumTypeIndex devType;/* pick device type */
    diPMHandle      pPM;	/* pick measure handle */
/* out */
{
    register miWksPtr pwks = (miWksPtr) pWKS->deviceData;
    register miPickMeasureStr *ppm;
    int	     dev_index;
    register miPickDevice *pPickDevice;

#ifdef DDTEST
    ErrorF(" CreatePickMeasure\n");
#endif

    MIWKS_PICK_DEV_INDEX(dev_index, devType);

    ppm = (miPickMeasureStr *) xalloc(sizeof(miPickMeasureStr));
    if (!ppm) return (BadAlloc);

    ppm->path = puCreateList(DD_PICK_PATH);
    if (!ppm->path) {
	xfree(ppm);
	return (BadAlloc);
    }
    pPickDevice = &(pwks->devices[dev_index]);

    ppm->pWks = pWKS;
    ppm->type = devType;
    ppm->status = pPickDevice->status;
    ppm->pathOrder = pPickDevice->pathOrder;
    ppm->incl_handle = pPickDevice->inclusion;
    ppm->excl_handle = pPickDevice->exclusion;

    if (ppm->incl_handle)
	UpdateNSRefs(	ppm->incl_handle, (diResourceHandle) NULL,
			PICK_RESOURCE, ADD);

    if (ppm->excl_handle)
	UpdateNSRefs(	ppm->excl_handle, (diResourceHandle) NULL,
			PICK_RESOURCE, ADD);


    if (puCopyList(pPickDevice->path, ppm->path)) {
	puDeleteList(ppm->path);
	xfree(ppm);
	return (BadAlloc);
    }
    /* now go through the path and update the structures' ref. count */
    path_update_struct_refs(	ppm->path, (diResourceHandle) NULL,
				PICK_RESOURCE, ADD);

    switch (dev_index) {
	case 0:
	    MIWKS_PM_DATA_REC_1(ppm) = MIWKS_PD_DATA_REC_1(pPickDevice);
	    break;
	case 1:
	    MIWKS_PM_DATA_REC_2(ppm) = MIWKS_PD_DATA_REC_2(pPickDevice);
	    break;
    }

    /* no extra data for PEX-SI */
    ppm->devPriv = (ddPointer) NULL;

    UpdateWksRefs(pWKS, (diResourceHandle) ppm, PICK_RESOURCE, ADD);
    pPM->deviceData = (ddPointer) ppm;
    return (Success);
}


/*++
 |
 |  Function Name:	FreePickMeasure
 |
 |  Function Description:
 |	Handles the PEXFreePickMeasure request
 |
 |  Note(s):
	pick measure is not used by other resources, so delete it now
 |
 --*/

ddpex4rtn
FreePickMeasure(pPM, PMid)
/* in */
    diPMHandle      pPM;	/* pick measure */
    ddResourceId    PMid;	/* pick measure id */
/* out */
{
    register miPickMeasureStr *ppm = (miPickMeasureStr *) pPM->deviceData;

    if (ppm->devPriv) xfree(ppm->devPriv);

    /* go through the path and update the structures' ref. count */
    if (ppm->path) path_update_struct_refs( ppm->path, (diResourceHandle) NULL,
					    PICK_RESOURCE, REMOVE);

    if (ppm->path) puDeleteList(ppm->path);

    if (ppm->pWks)
    UpdateWksRefs(ppm->pWks, (diResourceHandle) ppm, PICK_RESOURCE, REMOVE);

    if (ppm->incl_handle)
	UpdateNSRefs(	ppm->incl_handle, (diResourceHandle) NULL,
			PICK_RESOURCE, REMOVE);

    if (ppm->excl_handle)
	UpdateNSRefs(	ppm->excl_handle, (diResourceHandle) NULL,
			PICK_RESOURCE, REMOVE);

    xfree(ppm);
    xfree(pPM);

    return (Success);
}				/* FreePickMeasure */

/*++
 |
 |  Function Name:	InquirePickMeasure
 |
 |  Function Description:
 |	Handles the PEXInquirePickMeasure request
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
InquirePickMeasure(pPM, itemMask, pNumItems, pBuffer)
/* in */
    diPMHandle      pPM;	/* pick measure */
    ddBitmask       itemMask;	/* pick item bit mask */
/* out */
    ddULONG	*pNumItems;	/* number of items returned */
    ddBufferPtr     pBuffer;/* return buffer */
{
    register miPickMeasureStr *ppm = (miPickMeasureStr *) pPM->deviceData;
    register ddULONG dsize = 0;
    register ddPointer pbuf;

    *pNumItems = 0;
    pBuffer->dataSize = 0;

    if (itemMask & PEXPMStatus) {
	dsize += 4;
	*pNumItems++;
    }
    if (itemMask & PEXPMPath) {
	dsize += (4 + (ppm->path->numObj * 12));
	*pNumItems++;
    }
    PU_CHECK_BUFFER_SIZE(pBuffer, dsize);
    pBuffer->dataSize = dsize;
    pbuf = pBuffer->pBuf;

    if (itemMask & PEXPMStatus) {
	PACK_CARD32(ppm->status,pbuf);
    }
    if (itemMask & PEXPMPath) {
	PACK_CARD32(ppm->path->numObj,pbuf);
	pbuf = copy_pick_path_to_buffer(ppm->path, pbuf);
    }
    return (Success);
}

static unsigned char *
copy_pick_path_to_buffer(pPath, pBuf)
    listofObj      *pPath;
    ddPointer       pBuf;
{
    register int    i;
    ddPickPath	    *pp = (ddPickPath *)(pPath->pList);
    unsigned char   *ptr = pBuf;

    for (i = 0; i < pPath->numObj; i++, pp++) {
	PACK_CARD32(pp->structure->id, ptr);
	PACK_CARD32(pp->offset, ptr);
	PACK_CARD32(pp->pickid, ptr);
    }
    return (ptr);
}

/* pick devices  and pick measures are counted */
void
path_update_struct_refs(pPath, pResource, which, action)
    listofObj		*pPath;
    diResourceHandle	pResource;
    ddResourceType	which;
    ddAction	    action;
{
    ddPickPath     *pp = (ddPickPath *) pPath->pList;
    register int    i;

    for (i = 0; i < pPath->numObj; pp++, i++)
	UpdateStructRefs(pp->structure, pResource, which, action);
}
