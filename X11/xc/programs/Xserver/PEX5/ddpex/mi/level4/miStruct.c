/* $Xorg: miStruct.c,v 1.4 2001/02/09 02:04:11 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level4/miStruct.c,v 3.7 2001/12/14 19:57:35 dawes Exp $ */

#include "mipex.h"
#include "ddpex4.h"
#include "miStruct.h"
#include "PEXErr.h"
#include "PEXproto.h"
#include "pexError.h"
#include "pexUtils.h"
#include "miStrMacro.h"
#include "pexos.h"


/*  Level 4 Workstation Support */
/*  Structure Procedures  */

extern cssTableType CreateCSSElementTable[];
extern cssTableType DestroyCSSElementTable[];
extern cssTableType CopyCSSElementTable[];
extern cssTableType ReplaceCSSElementTable[];
extern cssTableType InquireCSSElementTable[];

extern ddBOOL   miGetStructurePriority();

extern ddpex4rtn miDealWithStructDynamics();
extern ddpex4rtn miDealWithDynamics();

void            miPrintPath();

#define SET_STR_HEADER(pStruct, pheader)				\
    register miStructPtr pheader = (miStructPtr) pStruct->deviceData

#define DESTROY_STR_HEADER(pheader)					\
	if (pheader->parents)	puDeleteList(pheader->parents);		\
	if (pheader->children)	puDeleteList(pheader->children);	\
	if (pheader->wksPostedTo)  puDeleteList(pheader->wksPostedTo);	\
	if (pheader->wksAppearOn)  puDeleteList(pheader->wksAppearOn);	\
	if (MISTR_ZERO_EL(pheader))  xfree(MISTR_ZERO_EL(pheader));	\
	if (MISTR_LAST_EL(pheader))  xfree(MISTR_LAST_EL(pheader));	\
	xfree(pheader)

#define	CHECK_DELETE(pHandle, pheader)					\
	if ((pheader)->freeFlag && !(pheader)->refCount) {		\
		DESTROY_STR_HEADER(pheader);				\
		xfree(pHandle);						\
	}

/*++
 |
 |  Function Name:	pos2offset
 |
 |  Function Description:
 |	  a utility function for converting whence, offset values to a valid
 |	  structure offset
 |
 |  Note(s):
 |
 --*/

static          ddpex4rtn
pos2offset(pstruct, ppos, poffset)
/* in */
	miStructStr    *pstruct;/* pointer to the structure involved */
	ddElementPos   *ppos;	/* the position information */
/* out */
	ddULONG        *poffset;/* the valid offset calculated from the
				 * postition */
{
	ddUSHORT        whence = ppos->whence;
	ddLONG          offset = ppos->offset, temp;

#ifdef DDTEST
	ErrorF("    POSITION : ");
#endif

	switch (whence) {
	case PEXBeginning:

#ifdef DDTEST
		ErrorF("PEXBeginning, ");
#endif

		temp = offset;
		break;

	case PEXCurrent:

#ifdef DDTEST
		ErrorF("PEXCurrent, ");
#endif

		temp = MISTR_CURR_EL_OFFSET(pstruct) + offset;
		break;

	case PEXEnd:

#ifdef DDTEST
		ErrorF("End, ");
#endif

		/* numElements is the same as the last elements offset */
		temp = MISTR_NUM_EL(pstruct) + offset;
		break;

	default:

#ifdef DDTEST
		ErrorF("Bad Value\n ");
#endif

		/* value error */
		return (BadValue);
		break;
	}

#ifdef DDTEST
	ErrorF("%d", offset);
#endif

	/* now check that the new offset is in range of the structure */
	if (temp < 0)
		*poffset = 0;
	else if (temp > MISTR_NUM_EL(pstruct))
		*poffset = MISTR_NUM_EL(pstruct);
	else
		*poffset = temp;

#ifdef DDTEST
	ErrorF(" = %d\n", *poffset);
#endif

	return (0);
}

/*++
 |
 |  Function Name:	CreateStructure
 |
 |  Function Description:
 |	 Handles the PEXCreateStructure request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
CreateStructure(pStruct)
/* in */
	diStructHandle  pStruct;/* structure handle */
/* out */
{
    register miStructStr *pheader;
    register miGenericElementPtr pelement;

#ifdef DDTEST
    ErrorF("\nCreateStructure %d\n", pStruct->id);
#endif

    pStruct->deviceData = NULL;

    if ((pheader = (miStructStr *) xalloc(sizeof(miStructStr))) == NULL)
	return (BadAlloc);

    MISTR_EDIT_MODE(pheader) = PEXStructureInsert;
    MISTR_NUM_EL(pheader) = 0;
    MISTR_LENGTH(pheader) = 0;

    pheader->refCount = 0;
    pheader->freeFlag = MI_FALSE;

    pheader->parents = pheader->children = pheader->wksPostedTo = pheader->wksAppearOn = NULL;

    pheader->parents = puCreateList(DD_STRUCT);
    pheader->children = puCreateList(DD_STRUCT);
    pheader->wksPostedTo = puCreateList(DD_WKS);
    pheader->wksAppearOn = puCreateList(DD_WKS);
    if (    !pheader->parents	    || !pheader->children
	 || !pheader->wksPostedTo   || !pheader->wksAppearOn) {
	DESTROY_STR_HEADER(pheader);
	return (BadAlloc);
    }
    /* create dummy first and last elements */
    if ((pelement = (miGenericElementPtr)xalloc(sizeof(miGenericElementStr)))
	    == NULL) {
	DESTROY_STR_HEADER(pheader);
	return (BadAlloc);
    }
    MISTR_PREV_EL(pelement) = NULL;
    MISTR_EL_TYPE(pelement) = PEXOCNil;
    MISTR_EL_LENGTH(pelement) = 1;
    MISTR_ZERO_EL(pheader) = MISTR_CURR_EL_PTR(pheader) = pelement;
    MISTR_CURR_EL_OFFSET(pheader) = 0;

    if ((pelement = (miGenericElementPtr) xalloc(sizeof(miGenericElementStr)))
	    == NULL) {
	DESTROY_STR_HEADER(pheader);
	return (BadAlloc);
    }
    MISTR_EL_TYPE(pelement) = PEXOCNil;
    MISTR_EL_LENGTH(pelement) = 1;
    MISTR_PREV_EL(pelement) = MISTR_ZERO_EL(pheader);
    MISTR_NEXT_EL(pelement) = NULL;
    MISTR_NEXT_EL(MISTR_ZERO_EL(pheader)) = pelement;
    MISTR_LAST_EL(pheader) = pelement;

    pStruct->deviceData = (ddPointer) pheader;
    return (Success);
}				/* CreateStructure */

/*++
 |
 |  Function Name:	CopyStructure
 |
 |  Function Description:
 |	 Handles the PEXCopyStructure request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
CopyStructure(pSrcStruct, pDestStruct)
/* in */
    diStructHandle  pSrcStruct;	/* source structure */
    diStructHandle  pDestStruct;	/* destination structure */
{
    SET_STR_HEADER(pSrcStruct, psource);
    SET_STR_HEADER(pDestStruct, pdest);
    ddpex4rtn       err = Success;
    ddElementRange  sourceRange;
    ddElementPos    destPos;
    miGenericElementPtr pel;
    register ddULONG i;

#ifdef DDTEST
    ErrorF("\nCopyStructure\n");
#endif

    /* to do: make this smarter so it can use Replace if possible */

    i = MISTR_NUM_EL(pdest);
    MISTR_DEL_ELS(pDestStruct, pdest, 1, i);	/* can't use NUM_EL macro here 
						 * because the num of els
						 * changes as they are deleted */

    MISTR_CURR_EL_OFFSET(pdest) = 0;
    MISTR_CURR_EL_PTR(pdest) = MISTR_ZERO_EL(pdest);

    sourceRange.position1.whence = PEXBeginning;
    sourceRange.position1.offset = 0;
    sourceRange.position2.whence = PEXEnd;
    sourceRange.position2.offset = 0;
    destPos.whence = PEXBeginning;
    destPos.offset = 0;

    /* Copy Elements will redraw picture if nec */
    if (err = CopyElements(pSrcStruct, &sourceRange, pDestStruct, &destPos)
	    != Success)
	return (err);

    MISTR_EDIT_MODE(pdest) = MISTR_EDIT_MODE(psource);
    MISTR_CURR_EL_OFFSET(pdest) = MISTR_CURR_EL_OFFSET(psource);

    MISTR_FIND_EL(pdest, MISTR_CURR_EL_OFFSET(pdest), pel);
    MISTR_CURR_EL_PTR(pdest) = pel;

    return (Success);
}				/* CopyStructure */

/* find_execute_structure looks for the specified execute structure element
 * If the specified structure is NULL, then it finds the next one.
 * If the element is found, its offset from the start is returned in poffset
 * When calling this repeatedly to look for all occurrences of the element,
 * be sure to check that you've reached the end of the structure. Otherwise
 * you would infinitely loop on finding the last element if it is a match.
 */
static          ddpex4rtn
find_execute_structure(pStruct, pStartPos, structHandle, poffset)
diStructHandle  pStruct;/* search this structure */
ddElementPos   *pStartPos;
diStructHandle  structHandle;	/* for this exec structure element */
ddULONG        *poffset;
{
    SET_STR_HEADER(pStruct, pstruct);
    ddUSHORT        foundExecuteElement;
    ddUSHORT        executeStructureElement = PEXOCExecuteStructure;
    miGenericElementPtr pel;
    ddpex4rtn	    err = Success;

    while (err == Success) {
	/** Get the position of the next execute structure element **/
	err = ElementSearch(	pStruct, pStartPos, (ddULONG) PEXForward, 
				(ddULONG) 1, (ddULONG) 0,
				&executeStructureElement, (ddUSHORT *) NULL,
				&foundExecuteElement, poffset);

	if (foundExecuteElement == PEXFound) {
	    MISTR_FIND_EL(pstruct, *poffset, pel);

	    if (    (structHandle == (diStructHandle) MISTR_GET_EXSTR_STR(pel))
		 || (structHandle == (diStructHandle) NULL))
			return (PEXFound);

	    /*
	     * continue searching at the next element unless this one 
	     * was the last
	     */
	    if (*poffset == MISTR_NUM_EL(pstruct)) return (PEXNotFound);

	    pStartPos->whence = PEXBeginning;
	    pStartPos->offset = *poffset + 1;
	} else
	    return (PEXNotFound);

    }
    if (err != Success) return (PEXNotFound);
    return (PEXFound);
}

/*++
 |
 |  Function Name:	DeleteStructureRefs
 |
 |  Function Description:
 |	 Handles the PEXDeleteStructures request.
 |
 |          This routine deletes all structure elements in all of the
 |       structures which reference the specified structure. It is called
 |       by DeleteStructure.
 |
 |  Note(s):
 |	This does not correct the picture because it is called by
 |	DeleteStructure, which does correct it
 |
 --*/

ddpex4rtn
DeleteStructureRefs(pStruct)
/* in */
    diStructHandle  pStruct;/* structure handle */
/* out */
{

    SET_STR_HEADER(pStruct, pstruct);
    diStructHandle  parentHandle;
    miStructPtr     pparentStruct;
    ddElementPos    position;
    miGenericElementPtr newPointer;
    miGenericElementPtr pel, pprevel;
    ddLONG          newOffset;
    ddULONG         offsetFromStart, numParents;

#ifdef DDTEST
    ErrorF("\nDeleteStructureRefs of structure %d\n", pStruct->id);
#endif				/* DDTEST */

    /** Search through each of the structure's parents **/

    /*
     * The tricky part here is that each time the execute structure
     * element is deleted from the parent, the structure's parent list
     * changes (in DeleteElements) so, remember how many parents there
     * originally are and look at that many.  Each time a parent is
     * deleted from the list, it's always deleted from the front of the
     * list, so the next parent will be at the front of the list the next
     * time through
     */
    for (numParents = pstruct->parents->numObj; numParents > 0;) {
	parentHandle = *(diStructHandle *) pstruct->parents->pList;

	/*
	 * look through all of this structure's elements to delete
	 * all references to the structure being deleted
	 */
	pparentStruct = (miStructPtr) (parentHandle)->deviceData;

	newOffset = 0;
	newPointer = NULL;

	/*
	 * look for all execute structure (child) elements in the
	 * parent structure and delete them this could do only one
	 * element at a time. any other elements would be gotten
	 * later because the parent is duplicated in the childs list
	 * for each occurrence and the outer loop would find the
	 * parent again.
	 */
	/* start looking at the beginning of the parent structure */
	position.whence = PEXBeginning;
	position.offset = 0;

	/*
	 * dont' forget we're really comparing the structure handles
	 * because the id was replaced with the handle in the exec
	 * str elements
	 */
	while (find_execute_structure(	parentHandle, &position, pStruct,
					&offsetFromStart)
		== PEXFound) {
	    if (offsetFromStart == MISTR_CURR_EL_OFFSET(pparentStruct)) {
		newOffset = MISTR_CURR_EL_OFFSET(pparentStruct) - 1;
		newPointer = MISTR_PREV_EL(MISTR_CURR_EL_PTR(pparentStruct));
	    } else if (offsetFromStart < MISTR_CURR_EL_OFFSET(pparentStruct)) {
		newOffset = MISTR_CURR_EL_OFFSET(pparentStruct) - 1;
		newPointer = MISTR_CURR_EL_PTR(pparentStruct);
	    } else {
		newOffset = MISTR_CURR_EL_OFFSET(pparentStruct);
		newPointer = MISTR_CURR_EL_PTR(pparentStruct);
	    }

	    MISTR_FIND_EL(pparentStruct, offsetFromStart, pel);
	    pprevel = MISTR_PREV_EL(pel);

	    MISTR_DEL_ONE_EL(parentHandle, pprevel, pel);

	    MISTR_CURR_EL_PTR(pparentStruct) = newPointer;
	    MISTR_CURR_EL_OFFSET(pparentStruct) = newOffset;

	    numParents--;

	    /*
	     * continue looking for other execute structures
	     * after the one found but it just got deleted, so
	     * the next one is now the same offset as the old one
	     */
	    position.whence = PEXBeginning;
	    position.offset = offsetFromStart;
	}

    }

    return (Success);
}				/* DeleteStructureRefs */

/*++
 |
 |  Function Name: DeleteStructure
 |
 |  Function Description:
 |	Deletes all storage associated with the structure
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
DeleteStructure(pStruct, Sid)
/* in */
    diStructHandle  pStruct;/* structure handle */
    ddResourceId    Sid;	/* structure resource id */
/* out */
{
    SET_STR_HEADER(pStruct, pheader);
    register ddULONG i, imax;
    diWKSHandle     pwks;
    ddpex4rtn       err = Success, next_err = Success;
    listofObj      *pwksToLookAt;

#ifdef DDTEST
    ErrorF("\nDeleteStructure %d\n", pStruct->id);
#endif

    /*
     * Errors are ignored to try to delete and update as much info as possible;
     * however, if an error is found, the last error detected is returned.
     */

    /*
     * Save the posted to list before unposting this structure so wks
     * pictures can be updated if necessary.
     */
    /* Do this here before the parent and children lists are changed. */

    /** Build up a list of workstations from the PostedTo and AppearOn
     ** lists in the structure structure.  They are inserted in such a
     ** manner so that duplicates between the lists are eliminated.
     **/

    pwksToLookAt = (listofObj *) NULL;

    if (pheader->wksPostedTo->numObj || pheader->wksAppearOn->numObj) {
	pwksToLookAt = puCreateList(DD_WKS);
	if (!pwksToLookAt) err = BadAlloc;
	else
	    err = puMergeLists(	pheader->wksPostedTo, pheader->wksAppearOn, 
				pwksToLookAt);
    }

    /*
     * This changes the structures posted to list (because unpost removes
     * the wks from this list) so always get the first wks from the list.
     */
    imax = pheader->wksPostedTo->numObj;
    for (i = 0; i < imax; i++) {
	pwks = ((diWKSHandle *) pheader->wksPostedTo->pList)[0];
	next_err = UnpostStructure(pwks, pStruct);
    }

    /*
     * Now, delete all of the references to this struct (i.e. remove this
     * structure from its parents).
     */
    next_err = DeleteStructureRefs(pStruct);
    if (next_err != Success) err = next_err;

    /* loop through to delete all of the elements */
    i = MISTR_NUM_EL(pheader);
    MISTR_DEL_ELS(pStruct, pheader, 1, i);

    /* now redraw picture for all workstations (determined above) */
    if (pwksToLookAt) {
	next_err = miDealWithDynamics(DELETE_STR_DYNAMIC, pwksToLookAt);
	if (next_err != Success) err = next_err;
	puDeleteList(pwksToLookAt);
    }

    /*
     * Don't delete the structure until sc and pick resources aren't using it.
     */
    pStruct->id = PEXAlreadyFreed;
    pheader->freeFlag = MI_TRUE;
    CHECK_DELETE(pStruct, pheader);

    return (err);
}				/* DeleteStructure */

/*++
 |
 |  Function Name:	InquireStructureInfo
 |
 |  Function Description:
 |	 Handles the PEXGetStructureInfo request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
InquireStructureInfo(fpFormat, pStruct, itemMask, pEditMode, pElOffset, pNumElements, pLength, pHasRefs)
/* in */
	ddEnumTypeIndex fpFormat;
	diStructHandle  pStruct;	/* structure handle */
	ddBitmask       itemMask;
/* out */
	ddUSHORT       *pEditMode;	/* edit mode */
	ddULONG        *pElOffset;	/* current element pointer */
	ddULONG        *pNumElements;	/* number of elements in structure */
	ddULONG        *pLength;	/* total size of structure */
	ddUSHORT       *pHasRefs;	/* is structure referenced by others */
{
	SET_STR_HEADER(pStruct, pheader);

#ifdef DDTEST
	ErrorF("\nInquireStructureInfo of %d\n", pStruct->id);
#endif

	/* Since all info is easily available and this is a fixed-length
	 * request, ignore itemMask (to the dismay of lint).
	 */
	*pEditMode = MISTR_EDIT_MODE(pheader);
	*pElOffset = MISTR_CURR_EL_OFFSET(pheader);
	*pNumElements = MISTR_NUM_EL(pheader);

	/*
	 * test here: if fpFormat is double precision, then recalculate
	 * length
	 */
	*pLength = MISTR_LENGTH(pheader);
	*pHasRefs = MISTR_NUM_PARENTS(pheader) != 0;
	return (Success);
}				/* InquireStructureInfo */

/*++
 |
 |  Function Name:	InquireElementInfo
 |
 |  Function Description:
 |	 Handles the PEXGetElementInfo request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
InquireElementInfo(pStruct, pRange, pNumElements, pBuffer)
/* in */
	diStructHandle  pStruct;/* structure handle */
	ddElementRange *pRange;	/* element range */
/* out */
	ddULONG        *pNumElements;	/* number of items in list */
	ddBufferPtr     pBuffer;/* list of element information */
{
	SET_STR_HEADER(pStruct, pheader);
	ddULONG         offset1, offset2, needbytes, i;
	int             peisize;
	ddPointer       pbuf;
	miGenericElementPtr pel;

#ifdef DDTEST
	ErrorF("\nInquireElementInfo %d\n", pStruct->id);
#endif

	peisize = sizeof(pexElementInfo);

	if (pos2offset(pheader, &(pRange->position1), &offset1))
		return (BadValue);	/* bad whence value */

	if (pos2offset(pheader, &(pRange->position2), &offset2))
		return (BadValue);	/* bad whence value */

	if (offset1 > offset2) {
		i = offset1;
		offset1 = offset2;
		offset2 = i;
	}

	if (offset1 == 0)
		if (offset2 == 0)
			return(Success);
		else
			offset1 = 1;


	/* make sure buffer is large enough */
	needbytes = (offset2 - offset1 + 1) * peisize;
	PU_CHECK_BUFFER_SIZE(pBuffer, needbytes);

	pbuf = pBuffer->pBuf;

	MISTR_FIND_EL(pheader, offset1, pel);

	/*
	 * remember that element data is required to have the type & length
	 * first so this is portable
	 */
	for (i = offset1; i <= offset2; i++, pbuf += peisize) {
		mibcopy(&MISTR_EL_DATA(pel), pbuf, peisize);
		pel = MISTR_NEXT_EL(pel);
	}

	*pNumElements = offset2 - offset1 + 1;
	pBuffer->dataSize = *pNumElements * peisize;

	return (Success);
}				/* InquireElementInfo */

static          ddpex4rtn
get_structure_net(pStruct, plist)
	diStructHandle  pStruct;
	listofObj      *plist;
{
	SET_STR_HEADER(pStruct, pheader);
	register int    i;
	register diStructHandle *pchild;

	/* put this structure on the list  */
	if (puAddToList((ddPointer) & pStruct, (ddULONG) 1, plist) ==
	    MI_ALLOCERR)
		return (BadAlloc);

	/* loop through all of the children of this structure */
	pchild = (diStructHandle *) pheader->children->pList;
	for (i = 0; i < pheader->children->numObj; i++, pchild++)
		if (get_structure_net(*pchild, plist) == BadAlloc)
			return (BadAlloc);

	return (Success);
}

/*++
 |
 |  Function Name:	InquireStructureNetwork
 |
 |  Function Description:
 |	 Handles the PEXGetStructuresInNetwork request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
InquireStructureNetwork(pStruct, which, pNumSids, pBuffer)
/* in */
    diStructHandle  pStruct;	/* structure handle */
    ddUSHORT        which;	/* which structures to inquire */
/* out */
    ddULONG        *pNumSids;	/* number of ids in list */
    ddBufferPtr     pBuffer;	/* list of structure ids */
{
    register int	i, j, num;
    ddResourceId	*pbuf;
    ddStructResource	**pstruct, **pparent;
    listofObj		*plist1, *plist2;
    ddBOOL		removing;

#ifdef DDTEST
    ErrorF("\nInquireStructureNetwork\n");
#endif

    pBuffer->dataSize = 0;
    *pNumSids = 0;

    plist1 = puCreateList(DD_STRUCT);
    if (!plist1) return (BadAlloc);

    plist2 = puCreateList(DD_STRUCT);
    if (!plist2) {
	puDeleteList(plist1);
	return (BadAlloc);
    }

    if (get_structure_net(pStruct, plist1) != Success) {
	puDeleteList(plist1);
	puDeleteList(plist2);
	return (BadAlloc);
    }
    /* now, make the list unique */
    puMergeLists(plist1, plist2, plist2);

    /* adjust for orphans if requested */
    if (which == PEXOrphans) {

	/*
	 * Look at the parents of each structure in list 3 if any parent is 
	 * not in the list, then that structure is not an orphan (i.e., it 
	 * is referenced by a structure not in this net), so remove it from 
	 * the list.
	 * 
	 * This is a pain though, because we aren't guaranteed that all
	 * parents of a structure precede it in the list; e.g., if plist2 
	 * for the network below starting at A is A, B, C, D, then when C
	 * is reached, both of its parents (B & D) are in the list, so it
	 * won't be deleted.  But when D is reached, (after C) it will have 
	 * parent E not in the list, so D will be deleted. Now C should be 
	 * deleted also.
	 *
	 * A   E / \ / B   D \ / C
	 * 
	 * My solution to this is to loop through plist2 multiple times until 
	 * it is gone through once without any structs being deleted from it. 
	 * If you have a better algorithm for this, then tell me about it.
	 * One solution is to guarantee the list has all parents in it before 
	 * their children.  I can't think of a way to do this, however.
	 */
	removing = MI_TRUE;
	while (removing) {
	    removing = MI_FALSE;
	    pstruct = (ddStructResource **) plist2->pList;

	    /*
	     * Note, while going through the list, it may be changed, so be
	     * careful of when pstruct gets incremented and be sure num
	     * reflects the original size of the list, not any changes made 
	     * to it.
	     */
	    num = plist2->numObj;
	    for (i = 0; i < num; i++, pstruct++) {
		pparent = (ddStructResource **)((miStructPtr)(*pstruct)->deviceData)->parents->pList;
		for ( j = 0;
		      j < ((miStructPtr)(*pstruct)->deviceData)->parents->numObj;
		      j++, pparent++) {
		    if (!puInList((ddPointer) pparent, plist2)) {

			/*
			 * This struct is not an orphan.
			 */
			puRemoveFromList((ddPointer) pstruct, plist2);
			removing = MI_TRUE;

			/*
			 * Decrement the pointer so when it gets incremented
			 * in the for loop, it points to where the deleted
			 * struct was,
			 */
			pstruct--;
			break;
		    }
		}
	    }
	}
    }
    /* now, return the structure ids */
    if (PU_BUF_TOO_SMALL(pBuffer, plist2->numObj * sizeof(ddResourceId))) {
	if (puBuffRealloc(pBuffer, (ddULONG) plist2->numObj) != Success) {
	    pBuffer->dataSize = 0;
	    puDeleteList(plist1);
	    puDeleteList(plist2);
	    return (BadAlloc);
	}
    }
    *pNumSids = plist2->numObj;
    pbuf = (ddResourceId *) pBuffer->pBuf;
    pstruct = (ddStructResource **) plist2->pList;
    for (i = 0; i < plist2->numObj; i++, pbuf++, pstruct++)
	*pbuf = (*pstruct)->id;

    pBuffer->dataSize = plist2->numObj * sizeof(ddResourceId);

    puDeleteList(plist1);
    puDeleteList(plist2);
    return (Success);
}				/* InquireStructureNetwork */


/* bufSave is the size of the buffer header when the buffer is
 * passed into inq ancestors/descendants.  the pBuf pointer
 * is changed as paths are added to the buffers and bufSave
 * is used to find where pBuf originally started
 */
static int      bufSave;

#define MI_ANCESTORS 0
#define MI_DESCENDANTS 1

/* given a path, which is a list of descendants (in top-down order)
 * or ancestors (in bottom-down order), see if the depth-length
 * pathPart part of the list matches any of the lists already
 * put in the buffer.  If it doesn't, then it's unique and this
 * proc returns MI_TRUE, else it returns MI_FALSE
 */
static          ddBYTE
path_unique(pathPart, depth, pNumLists, pBuffer, pPath, which)
    ddUSHORT        pathPart;
    ddULONG         depth;
    ddULONG        *pNumLists;
    ddBufferPtr     pBuffer;
    listofObj      *pPath;	/* current path */
    ddSHORT         which;
{
    register int    i, j;
    ddPointer       pb;
    ddULONG        *ll;
    ddElementRef   *pref, *ppath, *pathstart;
    ddBYTE          match;

    if (!depth || pPath->numObj < depth) depth = pPath->numObj;

    pb = pBuffer->pHead + bufSave;
    pathstart = (ddElementRef *) pPath->pList;
    if (which == MI_DESCENDANTS)
	pathstart += (pathPart == PEXTopPart) ? 0 : pPath->numObj - depth;
    else
	pathstart += ((pathPart == PEXTopPart) ? pPath->numObj - 1 : depth - 1);

    for (i = 0; i < *pNumLists; i++) {
	ll = (ddULONG *) pb;
	pb += 4;
	pref = (ddElementRef *) pb;
	ppath = pathstart;
	match = MI_TRUE;
	if (*ll == depth) {
	    if (which==MI_DESCENDANTS) {/* descendants: increment through path */
		for (j = 0; (j < *ll && match); j++, pref++, ppath++) {
		    if (    (ppath->structure != pref->structure)
			 || (ppath->offset != pref->offset))
			match = MI_FALSE;
		}
	    } else {/* ancestors: decrement through path */
		for (j = 0; (j < *ll && match); j++, pref++, ppath--) {
		    if (    (ppath->structure != pref->structure)
			 || (ppath->offset != pref->offset))
			match = MI_FALSE;
		}
	    }
	}
	pb += *ll * sizeof(ddElementRef);
	if (match) return (MI_FALSE);
    }

    return (MI_TRUE);
}

static          ddpex4rtn
copy_list_to_buf(pathPart, depth, pNumLists, pBuffer, pPath, which)
	ddUSHORT        pathPart;
	ddULONG         depth;
	ddULONG        *pNumLists;
	ddBufferPtr     pBuffer;
	listofObj      *pPath;
	ddSHORT         which;
{
	ddUSHORT        listsize;
	ddULONG        *pb;
	ddElementRef   *pref, *pbref;

	if (!depth || (pPath->numObj < depth))
		depth = pPath->numObj;
	listsize = depth * sizeof(ddElementRef);
	PU_CHECK_BUFFER_SIZE(pBuffer, listsize + 4);

	pb = (ddULONG *) pBuffer->pBuf;
	*pb++ = depth;
	pref = (ddElementRef *) pPath->pList;
	if (which == MI_DESCENDANTS) {
		if (pathPart == PEXTopPart)
			mibcopy(pref, pb, listsize);
		else {
			pbref = (ddElementRef *) pb;
			pref += pPath->numObj - 1;
			while (depth--)
				*pbref++ = *pref--;
		}
	} else {
		if (pathPart == PEXBottomPart)
			mibcopy(pref, pb, listsize);
		else {
			pbref = (ddElementRef *) pb;
			pref += pPath->numObj - 1;
			while (depth--)
				*pbref++ = *pref--;
		}
	}
	(*pNumLists)++;
	pBuffer->pBuf += listsize + 4;
	pBuffer->dataSize += listsize + 4;

	return (Success);
}

static          ddpex4rtn
get_ancestors(pStruct, pathPart, depth, pNumLists, pBuffer, pPath)
    diStructHandle  pStruct;
    ddUSHORT        pathPart;
    ddULONG         depth;
    ddULONG        *pNumLists;
    ddBufferPtr     pBuffer;
    listofObj      *pPath;	/* current path */
{
    SET_STR_HEADER(pStruct, pheader);
    diStructHandle  pParent;
    miStructPtr     pparent;
    register int    num;
    ddElementRef    newref;
    ddULONG         offset;
    ddElementPos    position;
    ddpex4rtn       err;
    listofObj      *singleparents;

    /* start out with the current struct */
    if (!pPath->numObj) {
	newref.structure = (diStructHandle) pStruct->id;
	newref.offset = 0;
	if (puAddToList((ddPointer) & newref, (ddULONG) 1, pPath) != Success)
	    return (BadAlloc);
    }
    /* we're at the root or have gone far enough */
    num = pheader->parents->numObj;
    if (!num || ((pathPart==PEXBottomPart) && depth && (pPath->numObj==depth))) {
	if (	(pathPart == PEXTopPart) && depth && (pPath->numObj > depth)
	     && !path_unique(	pathPart, depth, pNumLists, pBuffer, pPath, 
				MI_ANCESTORS))

	    /*
	     * if path is top first and has to be truncated to depth, don't 
	     * add it to the buffer unless it's unique
	     */
	    err = Success;
	else
	    err = copy_list_to_buf( pathPart, depth, pNumLists, pBuffer, pPath,
				    MI_ANCESTORS);

/*>>>	pPath->numObj--; */
	return (err);
    }

    /* take duplicates out of the list of parents */
    singleparents = puCreateList(DD_STRUCT);
    if (!singleparents) return (BadAlloc);

    if (puMergeLists(pheader->parents, singleparents, singleparents) != Success)
	return (BadAlloc);

    num = singleparents->numObj;
    while (num--) {
	pParent = ((diStructHandle *) singleparents->pList)[num];
	pparent = (miStructPtr) pParent->deviceData;

	/*
	 * now, look for each execute structure of this structure in the parent
	 */
	position.whence = PEXBeginning;
	position.offset = 0;

	while (find_execute_structure(pParent, &position, pStruct, &offset)
		       == PEXFound) {

	    newref.structure = (diStructHandle) pParent->id;
	    newref.offset = offset;
	    if (puAddToList((ddPointer) & newref, (ddULONG) 1, pPath) != Success)
		return (BadAlloc);

	    /*
	     * get the ancestors of this parent struct
	     */
	    if (err = get_ancestors(	pParent, pathPart, depth, pNumLists,
					pBuffer, pPath) != Success)
		return (err);

	    /*
	     * go on to get the next exec struct element in the parent
	     */
	    position.whence = PEXBeginning;
	    position.offset = offset + 1;

	    /*
	     * Remove previous parent/ofset from the pathlist so the same 
	     * pathlist can be used for the next path.
	     */
	    pPath->numObj--;

	    /*
	     * if the last one found was the last element in the
	     * struct, don't continue
	     */
	    if (offset == MISTR_NUM_EL(pparent)) break;

	}	/* end while finding execute structure elements in the parent */
    }		/* end while (num--): while this child has parents to look at */

    puDeleteList(singleparents);

    return (Success);
}

static          ddpex4rtn
get_descendants(pStruct, pathPart, depth, pNumLists, pBuffer, pPath)
    diStructHandle  pStruct;
    ddUSHORT        pathPart;
    ddULONG         depth;
    ddULONG        *pNumLists;
    ddBufferPtr     pBuffer;
    listofObj      *pPath;	/* current path */
{
    SET_STR_HEADER(pStruct, pheader);
    register int    num;
    ddElementRef    newref;
    diStructHandle  newstruct;
    ddULONG         offset;
    ddElementPos    position;
    ddpex4rtn       err;
    miGenericElementPtr pel;

    /* if we're at the end of the path put the path in the buffer  */
    num = pheader->children->numObj;
    if (!num || ((pathPart == PEXTopPart) && depth && (pPath->numObj == depth))){
	/* add this structure  to the path */

	/*
	 * don't need to do this if (pathPart == PEXTopPart) && depth
	 *  && (pPath->numObj == depth), but it's ok to do it because
	 * it won't get put into the buffer
	 */
	newref.structure = (diStructHandle) pStruct->id;
	newref.offset = 0;
	if (puAddToList((ddPointer) & newref, (ddULONG) 1, pPath) != Success)
	    return (BadAlloc);

	if (	(pathPart == PEXBottomPart) && depth && (pPath->numObj > depth)
	     && !path_unique(	pathPart, depth, pNumLists, pBuffer, pPath,
				MI_DESCENDANTS))

	    /*
	     * If path is bottom first and has to be truncated to depth, 
	     * don't add it to the buffer unless it's unique.
	     */
	    err = Success;
	else
	    err = copy_list_to_buf( pathPart, depth, pNumLists, pBuffer, pPath,
				    MI_DESCENDANTS);

	pPath->numObj--;
	return (err);
    }

    /* now, look for each execute structure element in the structure  */
    position.whence = PEXBeginning;
    position.offset = 0;
    while (find_execute_structure(  pStruct, &position, (diStructHandle) NULL,
				    &offset)
	       == PEXFound) {
	newref.structure = (diStructHandle) pStruct->id;
	newref.offset = offset;
	if (puAddToList((ddPointer) & newref, (ddULONG) 1, pPath) != Success)
	    return (BadAlloc);

	/*
	 * get the descendants of this child struct remember,
	 */
	MISTR_FIND_EL(pheader, offset, pel);

	newstruct = (diStructHandle) MISTR_GET_EXSTR_STR(pel);
	if (err = get_descendants(  newstruct, pathPart, depth, pNumLists,
				    pBuffer, pPath) != Success)
	    return (err);

	/* go on to get the next child */
	position.whence = PEXBeginning;
	position.offset = offset + 1;

	/*
	 * remove previous child from the pathlist so the same
	 * pathlist can be used for the next path
	 */
	pPath->numObj--;

	/*
	 * if the last one found was the last element in the struct,
	 * don't continue
	 */
	if (offset == MISTR_NUM_EL(pheader)) break;
    }

    return (Success);
}

/*++
 |
 |  Function Name:	InquireAncestors
 |
 |  Function Description:
 |	 Handles the PEXGetAncestors request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
InquireAncestors(pStruct, pathPart, depth, pNumLists, pBuffer)
/* in */
    diStructHandle  pStruct;/* structure handle */
    ddUSHORT        pathPart;	/* which paths to return */
    ddULONG         depth;	/* how deep to search */
/* out */
    ddULONG        *pNumLists;	/* number of lists returned */
    ddBufferPtr     pBuffer;/* list of lists of element refs */
{
    listofObj      *pathlist;
    ddpex4rtn       err;

#ifdef DDTEST
    ErrorF("\nInquireAncestors\n");
#endif

    bufSave = PU_BUF_HDR_SIZE(pBuffer);
    pBuffer->dataSize = 0;
    *pNumLists = 0;

    pathlist = puCreateList(DD_ELEMENT_REF);
    if (!pathlist) return (BadAlloc);

    err = get_ancestors(pStruct, pathPart, depth, pNumLists, pBuffer, pathlist);

    pBuffer->pBuf = pBuffer->pHead + bufSave;
    puDeleteList(pathlist);
    return (err);
}				/* InquireAncestors */

/*++
 |
 |  Function Name:	InquireDescendants
 |
 |  Function Description:
 |	 Handles the PEXGetDescendants request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
InquireDescendants(pStruct, pathPart, depth, pNumLists, pBuffer)
/* in */
    diStructHandle  pStruct;	/* structure handle */
    ddUSHORT        pathPart;	/* which paths to return */
    ddULONG         depth;	/* how deep to search */
/* out */
    ddULONG        *pNumLists;	/* number of lists returned */
    ddBufferPtr     pBuffer;	/* list of lists of element refs */
{
    listofObj      *pathlist;
    ddpex4rtn       err;

#ifdef DDTEST
    ErrorF("\nInquireDescendants\n");
#endif

    bufSave = PU_BUF_HDR_SIZE(pBuffer);
    pBuffer->dataSize = 0;
    *pNumLists = 0;

    pathlist = puCreateList(DD_ELEMENT_REF);
    if (!pathlist) return (BadAlloc);

    err = get_descendants(pStruct, pathPart, depth, pNumLists, pBuffer,pathlist);

    pBuffer->pBuf = pBuffer->pHead + bufSave;
    puDeleteList(pathlist);
    return (err);
}				/* InquireDescendants */


/*++
 |
 |  Function Name:	InquireElements
 |
 |  Function Description:
 |	 Handles the PEXFetchElements request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
InquireElements(pStruct, pRange, pNumOCs, pBuffer)
/* in */
	diStructHandle  pStruct;/* structure handle */
	ddElementRange *pRange;	/* range of elements */
/* out */
	ddULONG        *pNumOCs;/* number of items in list */
	ddBufferPtr     pBuffer;/* list of element OCs */
{
	SET_STR_HEADER(pStruct, pheader);
	ddULONG         offset1, offset2, i;
	miGenericElementPtr pel;
	ddpex4rtn       err;

#ifdef DDTEST
	ErrorF("\nInquireElements of %d\n", pStruct->id);
#endif

	*pNumOCs = 0;

	if (pheader->numElements == 0) return(Success);

	if (pos2offset(pheader, &(pRange->position1), &offset1))
		return (BadValue);	/* bad whence value */

	if (pos2offset(pheader, &(pRange->position2), &offset2))
		return (BadValue);	/* bad whence value */

	if (offset1 > offset2) {
		i = offset1;
		offset1 = offset2;
		offset2 = i;
	}

	if (offset1 == 0)
		if (offset2 == 0)
			return(Success);
		else
			offset1 = 1;

	MISTR_FIND_EL(pheader, offset1, pel);

	for (i = offset1; i <= offset2; i++) {
	    /* Propreitary calls (and OCNil) through 0th Table Entry */
	    if (MI_HIGHBIT_ON(MISTR_EL_TYPE(pel)))
		err = (*InquireCSSElementTable[MI_OC_PROP])
				      (pel, pBuffer, &(pBuffer->pBuf));
	    else {
		/* not Proprietary see if valid PEX OC */
		if (MI_IS_PEX_OC(MISTR_EL_TYPE(pel)))
		    err = (*InquireCSSElementTable[MISTR_EL_TYPE(pel)])
				      (pel, pBuffer, &(pBuffer->pBuf));
		else
		    err = !Success;
	    }

	    if (err != Success) {
		*pNumOCs = i - offset1;
		return (err);
	    }
	    pBuffer->dataSize += sizeof(CARD32)
				*(((ddElementInfo *)(pBuffer->pBuf))->length);
	    pBuffer->pBuf += sizeof(CARD32)
				* (((ddElementInfo *)(pBuffer->pBuf))->length);

	    pel = MISTR_NEXT_EL(pel);
	}

	*pNumOCs = offset2 - offset1 + 1;
	return (Success);

}				/* InquireElements */

/*++
 |
 |  Function Name:	SetEditMode
 |
 |  Function Description:
 |	 Handles the PEXSetEditingMode request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
SetEditMode(pStruct, editMode)
/* in */
	diStructHandle  pStruct;/* structure handle */
	ddUSHORT        editMode;	/* edit mode */
/* out */
{
	SET_STR_HEADER(pStruct, pheader);

#ifdef DDTEST
	ErrorF("\nSetEditMode of %d\n", pStruct->id);
#endif

	switch (editMode) {
	case PEXStructureInsert:
	case PEXStructureReplace:
		MISTR_EDIT_MODE(pheader) = editMode;
		return (Success);

	default:
		return (BadValue);
	}
}				/* SetEditMode */

/*++
 |
 |  Function Name:	SetElementPointer
 |
 |  Function Description:
 |	 Handles the PEXSetElementPointer request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
SetElementPointer(pStruct, pPosition)
/* in */
	diStructHandle  pStruct;/* structure handle */
	ddElementPos   *pPosition;	/* position to set pointer at */
/* out */
{
	SET_STR_HEADER(pStruct, pstruct);
	register miGenericElementPtr pel;
	ddULONG         newoffset;


#ifdef DDTEST
	ErrorF("\nSetElementPointer of %d\n", pStruct->id);
#endif

	if (pos2offset(pstruct, pPosition, &newoffset)) {
		/* bad whence value */
		return (BadValue);
	}
	if (newoffset == MISTR_CURR_EL_OFFSET(pstruct))
		return (Success);

	/* special case */
	if (newoffset == 0) {
		MISTR_CURR_EL_OFFSET(pstruct) = 0;
		MISTR_CURR_EL_PTR(pstruct) = MISTR_ZERO_EL(pstruct);
		return (Success);
	}
	MISTR_FIND_EL(pstruct, newoffset, pel);

	MISTR_CURR_EL_OFFSET(pstruct) = newoffset;
	MISTR_CURR_EL_PTR(pstruct) = pel;
	return (Success);
}				/* SetElementPointer */

/* look for the next label */
static          ddpex4rtn
find_label(pStruct, label, startPos, poffset)
diStructHandle  pStruct;
ddLONG          label;
ddElementPos    startPos;
ddULONG        *poffset;
{
    ddUSHORT        foundLabelElement;
    SET_STR_HEADER(pStruct, pstruct);
    ddUSHORT        labelElement = PEXOCLabel;
    miGenericElementPtr pel;
    ddpex4rtn	    err = Success;

    do {
	err = ElementSearch(	pStruct, &startPos, (ddULONG) PEXForward,
				(ddULONG) 1, (ddULONG) 0, &labelElement,
				(ddUSHORT *) NULL, &foundLabelElement, poffset);

	if (foundLabelElement == PEXFound) {
	    MISTR_FIND_EL(pstruct, *poffset, pel);

	    if (label == MISTR_GET_LABEL(pel)) return (PEXFound);

	    if (*poffset == MISTR_NUM_EL(pstruct)) return (PEXNotFound);

	    /* continue searching after the new current element */
	    startPos.whence = PEXBeginning;
	    startPos.offset = *poffset + 1;

	} else return (PEXNotFound);

    } while (err == Success);

    if (err != Success) return (PEXNotFound);
    return (PEXFound);
}

/*++
 |
 |  Function Name:	SetElementPointerAtLabel
 |
 |  Function Description:
 |	 Handles the PEXSetElementPointerAtLabel request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
SetElementPointerAtLabel(pStruct, label, offset)
/* in */
    diStructHandle  pStruct;/* structure handle */
    ddLONG          label;	/* label id */
    ddLONG          offset;	/* offset from label */
/* out */
{
    SET_STR_HEADER(pStruct, pstruct);
    ddElementPos    position;
    ddULONG         offsetFromStart;
    miGenericElementPtr pel;

#ifdef DDTEST
    ErrorF("\nSetElementPointerAtLabel\n");
#endif

    position.whence = PEXCurrent;
    position.offset = 1;

    if (find_label(pStruct, label, position, &offsetFromStart) == PEXNotFound)
	return (PEXERR(PEXLabelError));

    offsetFromStart += offset;

    if (offsetFromStart > MISTR_NUM_EL(pstruct))
	offsetFromStart = MISTR_NUM_EL(pstruct);

    MISTR_FIND_EL(pstruct, offsetFromStart, pel);

    MISTR_CURR_EL_PTR(pstruct) = pel;
    MISTR_CURR_EL_OFFSET(pstruct) = offsetFromStart;

    return (Success);
}				/* SetElementPointerAtLabel */

static          ddBOOL
InList(val, numInList, list)
	register ddUSHORT val;
	ddULONG         numInList;
	register ddUSHORT list[];
{
	/** Just do a linear search **/
	register int    i;
	for (i = 0; i < numInList; i++) {
		if ((val == list[i]) || PEXOCAll == list[i])
			return (MI_TRUE);
	}
	return (MI_FALSE);
}


/*++
 |
 |  Function Name:	ElementSearch
 |
 |  Function Description:
 |	 Handles the PEXElementSearch request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
ElementSearch(pStruct, pPosition, direction, numIncl, numExcl,
	      pIncls, pExcls, pStatus, pOffset)
/* in */
    diStructHandle  pStruct;/* structure handle */
    ddElementPos   *pPosition;	/* search start position */
    ddULONG         direction;	/* search direction (forward/backward) */
    ddULONG         numIncl;/* number of types in incl list */
    ddULONG         numExcl;/* number of types in excl list */
    ddUSHORT       *pIncls;	/* list of included element types */
    ddUSHORT       *pExcls;	/* list of excluded element types */
/* out */
    ddUSHORT       *pStatus;/* (found/notfound) */
    ddULONG        *pOffset;/* offset from the start position */
{

    SET_STR_HEADER(pStruct, str);
    ddULONG         positionOffset;
    miGenericElementPtr pel;

#ifdef DDTEST
    ErrorF("\nElementSearch of %d\n", pStruct->id);
#endif

    /** An element is considered "being searched for" if it is in the
     ** include list and not in the exclude list.  Elements in both
     ** are excluded.  An OCAll element specifies that all elements
     ** match
     **/

    if (pos2offset(str, pPosition, &positionOffset)) return (BadValue);

    *pStatus = PEXNotFound;
    *pOffset = 0;

    MISTR_FIND_EL(str, positionOffset, pel);

    /*
     * search is either forwards or backwards, check for end of search
     * for both
     */
    while ((positionOffset >= 0) && (positionOffset <= MISTR_NUM_EL(str))) {
	ddUSHORT        elType;

	elType = MISTR_EL_TYPE(pel);

	/** If current element is in include list and not in exclude
	 ** list, then succeed PEXOCAll matches all elements, even PEXOCNil */
	if (	InList(elType, numIncl, pIncls) 
	    && !InList(elType, numExcl, pExcls)) {
		*pStatus = PEXFound;
		*pOffset = positionOffset;
		return (Success);
	} else {
	    if (direction == PEXForward) {
		positionOffset++;
		pel = MISTR_NEXT_EL(pel);
	    } else {
		positionOffset--;
		pel = MISTR_PREV_EL(pel);
	    }
	}
    }

    return (Success);
}				/* ElementSearch */

/*++
 |
 |  Function Name:	StoreElements
 |
 |  Function Description:
 |	 Handles the PEXStoreElements request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
StoreElements(pStruct, numOCs, pOCs, ppErr)
/* in */
    diStructHandle  pStruct;	/* structure handle */
    register ddULONG numOCs;	/* number of output commands */
    ddElementInfo  *pOCs;	/* list of output commands */
/* out */
    pexOutputCommandError   **ppErr;
{
    SET_STR_HEADER(pStruct, pstruct);
    register ddElementInfo	*poc;
    register miGenericElementPtr    pprevel,	/* insert new one after this */
				    preplel;	/* or replace this one: preplel =
						 * pprevel->next */
    miGenericElementPtr pnewel;	/* new el to insert */
    int			count;
    ddpex4rtn		err = Success;

#ifdef DDTEST
    ErrorF("\nStoreElements in %d\n", pStruct->id);
#endif

    switch (MISTR_EDIT_MODE(pstruct)) {
	case PEXStructureReplace:
	    for (   poc = pOCs, count = 0,
		    preplel = MISTR_CURR_EL_PTR(pstruct),
		    pprevel = MISTR_PREV_EL(preplel);
		    numOCs > 0;
		    numOCs--,
		    pprevel = MISTR_NEXT_EL(pprevel),
		    preplel = MISTR_NEXT_EL(pprevel), poc += poc->length) {

		/*
		 * replace iff
		 * we're not at the end
		 * * and the types match
		 * * and we're not at the beginning
		 * * * and elements are the same size 
		 */

		if ((preplel != MISTR_LAST_EL(pstruct))
		    && (poc->elementType == MISTR_EL_TYPE(preplel))
		    && (preplel != MISTR_ZERO_EL(pstruct)) 
		    && (MISTR_EL_LENGTH(preplel) == poc->length)) {

		      /*
		       * *  Replace calls Parse functions 
		       */

			/* Propreitary OC (and OCNil) through 0th Table Entry */
			if (MI_HIGHBIT_ON(poc->elementType))
			    err = (*ReplaceCSSElementTable[MI_OC_PROP])
						(pStruct, preplel, poc );
                        else {
			    /* not Proprietary see if valid PEX OC */
			    if (MI_IS_PEX_OC(poc->elementType))
			      err = (*ReplaceCSSElementTable[poc->elementType])
						    (pStruct, preplel, poc);
			    else {
			      /* Bad Element Type Exit Now */
			      err = !Success;
			      break;
			    }
			}
		} else
		    /* Bad Replace */
		    err = !Success;

		if (err != Success) {	/* create new el */
		    /* Propreitary OC (and OCNil) through 0th Table Entry */
		    if (MI_HIGHBIT_ON(poc->elementType))
			err = (*CreateCSSElementTable[MI_OC_PROP])
						(pStruct, poc, &pnewel);
		    else {
			/* not Proprietary see if valid PEX OC */
			if (MI_IS_PEX_OC(poc->elementType))
			    err = (*CreateCSSElementTable[poc->elementType])
						    (pStruct, poc, &pnewel);
			else
			    /* Bad Element Type */
			    err = !Success;
		    }

		if (err != Success) break;

		count++;
		if (	(preplel != MISTR_LAST_EL(pstruct))
		     && (preplel != MISTR_ZERO_EL(pstruct))) {
		    /* get rid of old el */
		    MISTR_DEL_ONE_EL(pStruct, pprevel, preplel);
		}
		if (preplel == MISTR_ZERO_EL(pstruct))
		    pprevel = preplel;

		MISTR_INSERT_ONE_EL(pprevel, pnewel);
		}
	    }

	    if (err != Success) break;
	    MISTR_CURR_EL_PTR(pstruct) = pprevel;
	    MISTR_FIND_OFFSET(pstruct, pprevel, MISTR_CURR_EL_OFFSET(pstruct));

	    break;


	case PEXStructureInsert:
	    for (   count = 0, poc = pOCs, pprevel = MISTR_CURR_EL_PTR(pstruct);
		    numOCs > 0;
		    numOCs--, pprevel = pnewel, poc += poc->length) {

		/* Propreitary OC (and OCNil) through 0th Table Entry */
		if (MI_HIGHBIT_ON(poc->elementType))
		    err = (*CreateCSSElementTable[MI_OC_PROP]) 
						 (pStruct, poc, &pnewel);
		else {
		    /* not Proprietary see if valid PEX OC */
		    if (MI_IS_PEX_OC(poc->elementType))
			err = (*CreateCSSElementTable[poc->elementType])
					    (pStruct, poc, &pnewel);
		    else
			/* Bad Element Type */
			err = !Success;
		}

		if (err != Success) break;

		count++;
		MISTR_INSERT_ONE_EL(pprevel, pnewel);
		}

	    if (err != Success) break;
	    if (count) {
		MISTR_CURR_EL_PTR(pstruct) = pprevel;
		MISTR_FIND_OFFSET(  pstruct, pprevel,
				    MISTR_CURR_EL_OFFSET(pstruct));
		}
	    break;

	default:
	    /* better not get here */
	    ErrorF("tsk, tsk, the edit mode was set wrong\n");
	    return (BadImplementation);
	    break;
	}

    if (err != Success) {
	*ppErr = (pexOutputCommandError *)xalloc(sizeof(pexOutputCommandError));
	(*ppErr)->type = 0;
	(*ppErr)->errorCode = PEX_ERROR_CODE(PEXOutputCommandError);
	(*ppErr)->resourceId = pStruct->id;
	(*ppErr)->opcode = poc->elementType;
	(*ppErr)->numCommands = count;
	return (err);
    }


    miDealWithStructDynamics(STR_MODIFY_DYNAMIC, pStruct);

    return (Success);
}				/* StoreElements */

/*++
 |
 |  Function Name:	DeleteElements
 |
 |  Function Description:
 |	 Handles the PEXDeleteElements request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
DeleteElements(pStruct, pRange)
/* in */
	diStructHandle  pStruct;/* structure handle */
	ddElementRange *pRange;	/* range of elements to delete */
/* out */
{
	ddULONG         low, high, temp;
	SET_STR_HEADER(pStruct, pstruct);
	ddElementPos    newElementPointer;
	ddpex4rtn       err;

#ifdef DDTEST
	ErrorF("\nDeleteElements in %d\n", pStruct->id);
#endif

	if (pos2offset(pstruct, &(pRange->position1), &low))
		return (BadValue);	/* bad whence value */
	if (pos2offset(pstruct, &(pRange->position2), &high))
		return (BadValue);	/* bad whence value */

	/**  first pos needn't be lower then second pos, so order them now **/
	if (low > high) {
		temp = low;
		low = high;
		high = temp;
	}
	/** deleting element 0 equivalent to a NO-OP **/
	if (low == 0) {
		if (high == 0)
			return (Success);
		else
			low = 1;
	}
	MISTR_DEL_ELS(pStruct, pstruct, low, high);

	/*
	 * the current element pointer may now be invalid, so set it back to
	 * the beginning so it can be set correctly
	 */
	MISTR_CURR_EL_PTR(pstruct) = MISTR_ZERO_EL(pstruct);
	MISTR_CURR_EL_OFFSET(pstruct) = 0;

	/** Now, according to PEX spec, set element pointer to element
         ** preceding the range of deletion **/
	newElementPointer.whence = PEXBeginning;
	newElementPointer.offset = low - 1;
	err = SetElementPointer(pStruct, &newElementPointer);

	err = miDealWithStructDynamics(STR_MODIFY_DYNAMIC, pStruct);

	return (err);
}				/* DeleteElements */

/*++
 |
 |  Function Name:	DeleteToLabel
 |
 |  Function Description:
 |	 Handles the PEXDeleteElementsToLabel request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
DeleteToLabel(pStruct, pPosition, label)
/* in */
    diStructHandle  pStruct;/* structure handle */
    ddElementPos   *pPosition;	/* starting position */
    ddLONG          label;	/* label id to delete to */
/* out */
{
    SET_STR_HEADER(pStruct, pstruct);
    ddElementPos    position;
    ddElementRange  range;
    ddULONG         labelOffset;
    ddULONG         start;

#ifdef DDTEST
    ErrorF("\nDeleteToLabel\n");
#endif

    if (pos2offset(pstruct, pPosition, &start))
	return (BadValue);	/* bad whence value */

    position.whence = PEXBeginning;
    position.offset = start + 1;

    if (find_label(pStruct, label, position, &labelOffset) == PEXNotFound)
	return (PEXERR(PEXLabelError));

    /*
     * Now call DeleteElements to delete the elements, but first adjust
     * the range since DeleteElements does an inclusive delete and
     * DeleteToLabel doesn't.
     */
    if ((start == labelOffset) || ((start + 1) == labelOffset)) {
	/* there are no elements between them */
	/* set the element pointer to point to the offset */
	return(SetElementPointer(pStruct, pPosition));
    }

    range.position1.whence = PEXBeginning;
    range.position1.offset = start + 1;
    range.position2.whence = PEXBeginning;
    range.position2.offset = labelOffset - 1;

    /* DeleteElements also updates picture if nec. */
    return (DeleteElements(pStruct, &range));

}				/* DeleteToLabel */

/*++
 |
 |  Function Name:	DeleteBetweenLabels
 |
 |  Function Description:
 |	 Handles the PEXDeleteElementsBetweenLabels request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
DeleteBetweenLabels(pStruct, label1, label2)
/* in */
	diStructHandle  pStruct;/* structure handle */
	ddLONG          label1;	/* first label id */
	ddLONG          label2;	/* second label id */
/* out */
{
	ddElementPos    position;
	ddULONG         labelOffset;

#ifdef DDTEST
	ErrorF("\nDeleteBetweenLabels\n");
#endif

	position.whence = PEXCurrent;
	position.offset = 1;

	if (find_label(pStruct, label1, position, &labelOffset) == PEXNotFound)
		return (PEXERR(PEXLabelError));

	position.whence = PEXBeginning;
	position.offset = labelOffset;

	/* DeleteToLabel also updates picture if nec. */
	return (DeleteToLabel(pStruct, &position, label2));

}				/* DeleteBetweenLabels */

/*++
 |
 |  Function Name:	CopyElements
 |
 |  Function Description:
 |	 Handles the PEXCopyElements request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
CopyElements(pSrcStruct, pSrcRange, pDestStruct, pDestPosition)
/* in */
	diStructHandle  pSrcStruct;	/* source structure handle */
	ddElementRange *pSrcRange;	/* element range to copy */
	diStructHandle  pDestStruct;	/* destination structure handle */
	ddElementPos   *pDestPosition;	/* destination position to put stuff */
/* out */
{
	SET_STR_HEADER(pSrcStruct, psource);
	SET_STR_HEADER(pDestStruct, pdest);
	ddULONG         src_low, src_high, dest_offset, i, count;
	miGenericElementPtr psrcel, pdestel, pdestprev;
	miGenericElementStr pfirst, plast;	/* dummies */
	ddpex4rtn       err4 = Success;


#ifdef DDTEST
	ErrorF("\nCopyElements\n");
#endif
	if (pos2offset(psource, &(pSrcRange->position1), &src_low))
		return (BadValue);	/* bad whence value */

	if (pos2offset(psource, &(pSrcRange->position2), &src_high))
		return (BadValue);	/* bad whence value */

	if (pos2offset(pdest, pDestPosition, &dest_offset))
		return (BadValue);	/* bad whence value */

	if (src_low > src_high) {
		i = src_low;
		src_low = src_high;
		src_high = i;
	}
	if (src_low == 0) {
		if (src_high == 0)
			return (Success);
		else
			src_low = 1;
	}
	MISTR_FIND_EL(psource, src_low, psrcel);

	/*
	 * copy els to dummy list, then add dummy list to dest NOTE:
	 * CopyCSSElement procedure is passed pDestStruct for the copy even
	 * though the element is not really being inserted into the structure
	 * yet. This should be OK, but beware!!
	 */
	MISTR_NEXT_EL(&pfirst) = &plast;
	MISTR_PREV_EL(&plast) = &pfirst;
	MISTR_PREV_EL(&pfirst) = MISTR_NEXT_EL(&plast) = NULL;
	pdestprev = &pfirst;

	for (i = src_low, count = 0; i <= src_high; i++) {

		/* Propreitary OC (and OCNil) through 0th Table Entry */
		if (MI_HIGHBIT_ON(MISTR_EL_TYPE(psrcel)))
			err4 = (*CopyCSSElementTable[MI_OC_PROP])
				(psrcel, pDestStruct, &pdestel);
		else {
		    /* not Proprietary see if valid PEX OC */
		    if (MI_IS_PEX_OC(MISTR_EL_TYPE(psrcel)))
			err4 = (*CopyCSSElementTable[MISTR_EL_TYPE(psrcel)])
				(psrcel, pDestStruct, &pdestel);
		    else
			/* Bad Element Type - Problem if you get here */
			err4 = !Success;
		}

		if (err4 != Success)
			break;

		count++;
		MISTR_INSERT_ONE_EL(pdestprev, pdestel);
		pdestprev = pdestel;
		psrcel = MISTR_NEXT_EL(psrcel);
	}

	if (count) {
		MISTR_FIND_EL(pdest, dest_offset, pdestprev);

		MISTR_NEXT_EL(MISTR_PREV_EL(&plast)) = MISTR_NEXT_EL(pdestprev);
		MISTR_PREV_EL(MISTR_NEXT_EL(pdestprev)) = MISTR_PREV_EL(&plast);

		MISTR_NEXT_EL(pdestprev) = MISTR_NEXT_EL(&pfirst);
		MISTR_PREV_EL(MISTR_NEXT_EL(&pfirst)) = pdestprev;

		MISTR_CURR_EL_PTR(pdest) = MISTR_PREV_EL(&plast);
		MISTR_FIND_OFFSET(pdest, MISTR_CURR_EL_PTR(pdest), MISTR_CURR_EL_OFFSET(pdest));
	}
	err4 = miDealWithStructDynamics(STR_MODIFY_DYNAMIC, pDestStruct);

	return (err4);
}				/* CopyElements */

/*++
 |
 |  Function Name:	ChangeStructureReferences
 |
 |  Function Description:
 |	 Handles the PEXChangeStructureReferences request.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
ChangeStructureReferences(pStruct, pNewStruct)
/* in */
    diStructHandle  pStruct;/* structure handle */
    diStructHandle  pNewStruct;	/* new structure resource */
/* out */
{

    SET_STR_HEADER(pStruct, pstruct);
    SET_STR_HEADER(pNewStruct, pnewstruct);
    diStructHandle  parentHandle;
    miStructPtr     pparentStruct;
    int             loopcount;
    ddElementPos    position;
    ddpex4rtn       foundExecuteElement, err;
    ddULONG         offsetFromStart;
    diWKSHandle     pWks;
    miGenericElementPtr pel;
    pexExecuteStructure execStrOC;
    ddFLOAT         prity;

#ifdef DDTEST
    ErrorF("\nChangeStructureReferences\n");
#endif				/* DDTEST */

    /* set up OC with new structure that will replace old  */
    execStrOC.head.elementType = PEXOCExecuteStructure;
    execStrOC.head.length = 2;
    execStrOC.id = (pexStructure) pNewStruct;

    /*
     * Update all references to this structure by walking through the
     * structure's parent list.  Note that if the structure is referenced
     * more than once by a parent structure, there will be more than one
     * occurence of the parent in the parent list.
     */
    
    for (loopcount = pstruct->parents->numObj; loopcount > 0; loopcount--) {

	/* The parent list changes in loop, so always get first parent */
	parentHandle = *(diStructHandle *) pstruct->parents->pList;
	pparentStruct = (miStructPtr) (parentHandle)->deviceData;

	/* start looking at the beginning of the parent structure */
        position.whence = PEXBeginning;
        position.offset = 0;
        offsetFromStart = 0;

    	foundExecuteElement = find_execute_structure (parentHandle, &position,
		pStruct, &offsetFromStart);

    	if (foundExecuteElement == PEXFound) {
		MISTR_FIND_EL(pparentStruct, offsetFromStart, pel);
		err = (*ReplaceCSSElementTable[PEXOCExecuteStructure])
					(parentHandle, pel, &execStrOC);
		if (err != Success) return (err);
	} else
		return (!Success);
    }

    /*
     * this changes the old structures posted to list (because unpost removes 
     * the wks from this list) so always get the first wks from the list and 
     * be sure to check the original number of posted wks
     */
    for (loopcount = pstruct->wksPostedTo->numObj; loopcount > 0; loopcount--) {
	pWks = ((diWKSHandle *) pstruct->wksPostedTo->pList)[0];
	if (puInList((ddPointer) pWks, pnewstruct->wksPostedTo))
	    err = UnpostStructure(pWks, pStruct);
	else {
	    miGetStructurePriority(pWks, pStruct, &prity);
	    err = PostStructure(pWks, pNewStruct, prity);
	}
	if (err) return (err);
    }

    err = miDealWithStructDynamics(REF_MODIFY_DYNAMIC, pNewStruct);

    return (Success);
}				/* ChangeStructureReferences */

int
miAddWksToAppearLists(pStruct, pWKS)
    diStructHandle  pStruct;
    diWKSHandle     pWKS;
{
    SET_STR_HEADER(pStruct, pheader);
    register ddULONG i, num;
    diStructHandle *ps;

    /* loop through the structures list of children */
    num = pheader->children->numObj;
    ps = (diStructHandle *) pheader->children->pList;
    for (i = 0; i < num; i++, ps++) {
	if (puAddToList(    (ddPointer) & pWKS, (ddULONG) 1,
			    ((miStructPtr) (*ps)->deviceData)->wksAppearOn)
	    == MI_ALLOCERR)
	    return (MI_ALLOCERR);

	    /* recur to do the children of this child */
	    if (miAddWksToAppearLists(*ps, pWKS) != MI_SUCCESS)
		return (MI_ALLOCERR);
    }
    return (MI_SUCCESS);
}

void
miRemoveWksFromAppearLists(pStruct, pWKS)
    diStructHandle  pStruct;
    diWKSHandle     pWKS;
{
    SET_STR_HEADER(pStruct, pheader);
    register ddULONG i, num;
    diStructHandle *ps;

#ifdef DDTEST
    ErrorF("\tmiRemoveWksFromAppearLists (of structure %d)\n", pStruct->id);
#endif

    num = pheader->children->numObj;
    ps = (diStructHandle *) pheader->children->pList;

    /* look at all children of this structure */
    for (i = 0; i < num; i++, ps++) {
	/* remove the wks from the child's list */
	puRemoveFromList(   (ddPointer) & pWKS,
			    ((miStructPtr) (*ps)->deviceData)->wksAppearOn);

	/* recur to do the children of this child */
	miRemoveWksFromAppearLists(*ps, pWKS);
    }
    return;
}

/*++
 |
 |  Function Name:	UpdateStructRefs
 |
 |  Function Description:
 |	A utility function to change the cross-reference lists in the structure.
 |	Each structure has a list of every workstation and structure which uses
 |	it.
 |
 |  Note(s):
 |
 --*/

ddpex4rtn
UpdateStructRefs(pStruct, pResource, which, action)
/* in */
    diStructHandle  pStruct;/* structure handle */
    diResourceHandle pResource;	/* wks, struct, sc handle */
    ddResourceType  which;	/* wks, struct, pick, sc */
    ddAction        action;	/* add or remove */
/* out */
{
    SET_STR_HEADER(pStruct, pheader);

#ifdef DDTEST
    ErrorF("\nUpdateStructRefs\n");
#endif

    switch (which) {
	case WORKSTATION_RESOURCE:

	    /*
	     * for each workstation, do it to the specified structures
	     * wksPostedTo list and do it to the wksAppearOn list of all
	     * of the structures children
	     */
	    if (action == ADD) {
		if (puAddToList(    (ddPointer) & pResource, (ddULONG) 1,
				    pheader->wksPostedTo) == MI_ALLOCERR)
		    return (BadAlloc);	/* couldn't add to list */
		if (miAddWksToAppearLists(pStruct, (diWKSHandle) pResource))
		    return (BadAlloc);	/* couldn't add to list */
	    } else {
		puRemoveFromList((ddPointer) & pResource, pheader->wksPostedTo);
		miRemoveWksFromAppearLists(pStruct, (diWKSHandle) pResource);
	    }

	    break;

	case PARENT_STRUCTURE_RESOURCE:
	    if (action == ADD) {
		if (puAddToList(    (ddPointer) & pResource, (ddULONG) 1,
				    pheader->parents) == MI_ALLOCERR)
		    return (BadAlloc);	/* couldn't add to list */
	    } else
		puRemoveFromList((ddPointer) & pResource, pheader->parents);

	    break;

	case CHILD_STRUCTURE_RESOURCE:
	    if (action == ADD) {
		if (puAddToList(    (ddPointer) & pResource, (ddULONG) 1,
				    pheader->children) == MI_ALLOCERR)
		    return (BadAlloc);	/* couldn't add to list */
	    } else
		puRemoveFromList((ddPointer) & pResource, pheader->children);

	    break;

	case SEARCH_CONTEXT_RESOURCE:
	case PICK_RESOURCE:	/* for both pick device & pick measure */
	    if (action == ADD)
		pheader->refCount++;
	    else {
		pheader->refCount--;
		CHECK_DELETE(pStruct, pheader);
	    }
	    break;

	default:		/* better not get here */
	    break;
    }
    return (Success);
}

/* get_wks_postings for InquireWksPostings
 * implement it here since it uses structure stuff
 */
ddpex4rtn
get_wks_postings(pStruct, pBuffer)
    diStructHandle  pStruct;
    ddBufferPtr     pBuffer;
{
    SET_STR_HEADER(pStruct, pheader);
    listofObj      *wkslist;
    diWKSHandle    *pwks;
    ddResourceId   *pbuf;
    register int    i;

    pBuffer->dataSize = 0;

    wkslist = pheader->wksPostedTo;

    if (PU_BUF_TOO_SMALL(pBuffer,(ddULONG)wkslist->numObj *sizeof(ddResourceId)))
	if (puBuffRealloc(  pBuffer,
			    (ddULONG)(wkslist->numObj * sizeof(ddResourceId)))
		!= Success) {
	    puDeleteList(wkslist);
	    return (BadAlloc);
	}
    pwks = (diWKSHandle *) wkslist->pList;
    pbuf = (ddResourceId *) pBuffer->pBuf;
    for (i = 0; i < wkslist->numObj; i++, pwks++, pbuf++) *pbuf = (*pwks)->id;
    pBuffer->dataSize = wkslist->numObj * sizeof(ddResourceId);

    return (Success);
}

/* make a generic puPrintList and put it in dipex/util/pexUtils.c sometime */
void
miPrintPath(pPath)
	listofObj      *pPath;
{
	register int    i;
	register ddElementRef *pref;

	ErrorF("\nELEMENT REF PATH\n");
	pref = (ddElementRef *) pPath->pList;
	for (i = 0; i < pPath->numObj; i++, pref++)
		ErrorF("\tstructure id: %d\toffset: %d\n",
		       pref->structure, pref->offset);
	ErrorF("\nEND PATH\n");
}

/*++
 |
 |  Function Name:	miPrintStructure
 |
 |  Function Description:
 |     Prints out the contents of a structure for debugging
 |     purposes.
 |
 |  Input Description:
 |	miStructPtr	pStruct;
 |	int		strLevel	    - amount of struct info to display
 |  Output Description:
 |	Switch strLevel :
 |	    case 0 :
 |		don't do anything.
 |          case 1 :
 |      	print structure header and nothing more
 |          case 2 :
 |      	print structure header and affil. structs and wks
 |
 --*/

static void     printWorkstations(), printStructures();

void
miPrintStructure(S, strLevel)
	diStructHandle  S;
	int             strLevel;
{
	miStructPtr     s = (miStructPtr) S->deviceData;

	if (strLevel > 0) {

		ErrorF("\n\n\n**********************************\n");
		ErrorF("* Printing Structure at 0x%x *\n", s);
		ErrorF("**********************************\n");
		ErrorF("ID = %ld\n", S->id);
		ErrorF("Edit Mode = %s\n", (s->editMode == PEXStructureReplace) ?
		       "REPLACE" : "INSERT");
		ErrorF("Num Elements = %ld\nTotal Size in 4 byte units = %ld\n",
		       s->numElements, s->totalSize);
		ErrorF("Curr Offset = %ld\nCurr Elt Ptr = 0x%x\n",
		       s->currElementOffset, s->pCurrElement);
		ErrorF("Zero El Ptr = 0x%x\nLast El Ptr = 0x%x\n",
		       s->pZeroElement, s->pLastElement);

		if (strLevel == 2) {
			ErrorF("\nParent Structures :\n");
			printStructures(s->parents);
			ErrorF("\nChild Structures :\n");
			printStructures(s->children);
			ErrorF("\nWKS posted to:\n");
			printWorkstations(s->wksPostedTo);
			ErrorF("\nWKS appearing on:\n");
			printWorkstations(s->wksAppearOn);
		}
	}
}

static void
printStructures(list)
	listofObj      *list;
{
	int             i;
	diStructHandle *str;

	str = (diStructHandle *) list->pList;
	for (i = 0; i < list->numObj; i++, str++) {
		ErrorF("\tStruct Address: 0x%x\t\tId: %ld\n",
		       (*str)->deviceData, (*str)->id);
	}
}

static void
printWorkstations(list)
	listofObj      *list;
{
	int             i;
	diWKSHandle    *wks;

	wks = (diWKSHandle *) list->pList;
	for (i = 0; i < list->numObj; i++, wks++) {
		ErrorF("\tWks Address: 0x%x\t\tId: %ld\n",
		       (*wks)->deviceData, (*wks)->id);
	}
}


/*++
 |
 |  Function Name:	ValidateStructurePath
 |
 |  Function Description:
	Follows the given search or pick path to see if it's valid
 |
 --*/

ddpex4rtn
ValidateStructurePath(pPath)
    listofObj      *pPath;
{
    miGenericElementPtr p_element;
    diStructHandle  pStruct, pNextStruct;
    miStructPtr     pstruct;
    ddULONG         offset;
    int             i, j;

    if (pPath->type == DD_ELEMENT_REF) {
	ddElementRef   *pSCPath;

	pSCPath = (ddElementRef *) pPath->pList;
	pNextStruct = pSCPath->structure;

	for (i = pPath->numObj; i > 0; i--, pSCPath++) {
	    pStruct = pSCPath->structure;
	    if (pNextStruct != pStruct)	return (PEXERR(PEXPathError));

	    pstruct = (miStructPtr) pStruct->deviceData;

	    offset = pSCPath->offset;
	    if (offset > MISTR_NUM_EL(pstruct)) return (PEXERR(PEXPathError));

	    /* dont' check what the last element is */
	    if (i == 1) break;

	    MISTR_FIND_EL(pstruct, offset, p_element);
	    if (MISTR_EL_TYPE(p_element) != PEXOCExecuteStructure)
		return (PEXERR(PEXPathError));

	    pNextStruct = (diStructHandle) MISTR_GET_EXSTR_STR(p_element);
	}
    } else {
	ddPickPath     *pPickPath;
	ddULONG         pickId;

	/*
	 * pick has to step through each element to check pick id also
	 */
	pPickPath = (ddPickPath *) pPath->pList;
	pNextStruct = pPickPath->structure;
	pickId = 0;

	for (i = pPath->numObj; i > 0; i--, pPickPath++) {

	    pStruct = pPickPath->structure;
	    if (pNextStruct != pStruct) return (PEXERR(PEXPathError));

	    pstruct = (miStructPtr) pStruct->deviceData;

	    offset = pPickPath->offset;
	    if (offset > MISTR_NUM_EL(pstruct)) return (PEXERR(PEXPathError));

	    /*
	     * start at the first element and look at each
	     * element until the offset is reached
	     */
	    MISTR_FIND_EL(pstruct, 1, p_element);

	    for (j = 1; j <  offset; j++ ) {
		if (MISTR_EL_TYPE(p_element) == PEXOCPickId)
		    pickId = MISTR_GET_PICK_ID(p_element);
		p_element = MISTR_NEXT_EL(p_element);
	    }

	    if (pickId != pPickPath->pickid) return (PEXERR(PEXPathError));

	    /* dont' check what the last element is */
	    if (i == 1) break;

	    if (MISTR_EL_TYPE(p_element) != PEXOCExecuteStructure)
		return (PEXERR(PEXPathError));

	    pNextStruct = (diStructHandle) MISTR_GET_EXSTR_STR(p_element);
	}
    }
    return (Success);
}
