/* $Xorg: css_ex_str.c,v 1.4 2001/02/09 02:04:11 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/Xserver/PEX5/ddpex/mi/level4/css_ex_str.c,v 1.10 2001/12/14 19:57:33 dawes Exp $ */
/*
 * this file contains the element handling procedures for
 * the execute structure element.  It is stored in PEX format
 * but needs special handling
 */

#include "ddpex.h"
#include "miStruct.h"
#include "miStrMacro.h"
#include "pexUtils.h"
#include "pexos.h"


extern ocTableType ParseOCTable[];
extern ocTableType CopyOCTable[];
extern ocTableType InquireOCTable[];

extern void     miRemoveWksFromAppearLists();
extern int      miAddWksToAppearLists();
extern ddpex4rtn destroyCSS_Plain();

#define SET_STR_HEADER(pStruct, pheader)                                \
    register miStructPtr pheader = (miStructPtr) pStruct->deviceData

#define PEX_EL_TYPE(POC) ((ddElementInfo *)(POC))->elementType

/* must have err, i and ppwks defined to use this macro */
#define	ADD_WKS( plist, pstr )							\
    if (puAddToList((ddPointer)(plist)->pList, (ddULONG)(plist)->numObj,	\
	    ((miStructPtr)(pstr)->deviceData)->wksAppearOn) == MI_ALLOCERR)	\
	return(MI_ALLOCERR);							\
    for (   i=(plist)->numObj, ppwks=(diWKSHandle *)(plist)->pList;		\
	    i > 0;								\
	    i--, ppwks++)							\
	if ((err = miAddWksToAppearLists( (pstr), *ppwks )) != MI_SUCCESS) 	\
	    return(err)

/* must have i and ppwks defined to use this macro */
#define	REMOVE_WKS( plist, pstr )						\
    for (   i=(plist)->numObj, ppwks=(diWKSHandle *)(plist)->pList;		\
	    i > 0;								\
	    i--, ppwks++) {							\
	puRemoveFromList(   (ddPointer)ppwks,					\
			    ((miStructPtr)(pstr)->deviceData)->wksAppearOn);	\
	miRemoveWksFromAppearLists((pstr), *ppwks);	}

ddpex4rtn
createCSS_Exec_Struct(pStruct, pPEXOC, ppCSSElement)
/* in */
	diStructHandle  pStruct;
	pexExecuteStructure *pPEXOC;
/* out */
	miGenericElementStr **ppCSSElement;
{
	SET_STR_HEADER(pStruct, pheader);
	diStructHandle  pexecstr;
	ddpex4rtn       err;
	register ddULONG    i;
	register diWKSHandle *ppwks;

	*ppCSSElement = (miGenericElementPtr) NULL;

	err = (*ParseOCTable[PEX_EL_TYPE(pPEXOC)]) (pPEXOC, ppCSSElement);

	if (err != Success)
		return (err);

	/*
	 * update the structure cross reference lists the id in execute
	 * structure elements was replaced with the diStructHandle by diPEX
	 */
	pexecstr = *((diStructHandle *)&(pPEXOC->id));

	/* update pStructs children list */
	err = UpdateStructRefs(pStruct, (diResourceHandle) pexecstr,
			       CHILD_STRUCTURE_RESOURCE, ADD);
	if (err != Success)
		return (err);

	/* update the executed structs parent list */
	err = UpdateStructRefs(pexecstr, (diResourceHandle) pStruct,
			       PARENT_STRUCTURE_RESOURCE, ADD);
	if (err != Success)
		return (err);

	/* update the wks appears on lists of the subordinate structure */
	ADD_WKS(pheader->wksPostedTo, pexecstr);
	ADD_WKS(pheader->wksAppearOn, pexecstr);

	(*ppCSSElement)->pStruct = pStruct;
	(*ppCSSElement)->element.pexOClength =
		((ddElementInfo *) pPEXOC)->length;	/* protocol size, not
							 * parsed size */
	(*ppCSSElement)->element.elementType =
		((ddElementInfo *) pPEXOC)->elementType;

	MISTR_NUM_EL(pheader)++;
	MISTR_LENGTH(pheader) += ((ddElementInfo *) pPEXOC)->length;

	return (Success);
}

ddpex4rtn
destroyCSS_Exec_Struct(pStruct, pCSSElement)
/* in */
	diStructHandle  pStruct;
	miGenericElementPtr pCSSElement;
/* out */
{
	SET_STR_HEADER(pStruct, pheader);
	diStructHandle  pexecstr;
	register int    i;
	register diWKSHandle *ppwks;

	pexecstr = (diStructHandle) MISTR_GET_EXSTR_STR(pCSSElement);
	/* Now remove this child from the parent's list of children */

	UpdateStructRefs(pStruct, (diResourceHandle) pexecstr,
			 CHILD_STRUCTURE_RESOURCE, REMOVE);

	/* and remove the parent from the childs list */

	UpdateStructRefs(pexecstr, (diResourceHandle) pStruct,
			 PARENT_STRUCTURE_RESOURCE, REMOVE);

	/* update the wks appears on lists of the subordinate structure */
	REMOVE_WKS(pheader->wksPostedTo, pexecstr);
	REMOVE_WKS(pheader->wksAppearOn, pexecstr);

	return (destroyCSS_Plain(pStruct, pCSSElement));
}

ddpex4rtn
copyCSS_Exec_Struct(pSrcCSSElement, pDestStruct, ppDestCSSElement)
/* in */
	miGenericElementPtr pSrcCSSElement;
	diStructHandle  pDestStruct;
/* out */
	miGenericElementStr **ppDestCSSElement;
{
	SET_STR_HEADER(pDestStruct, pheader);
	diStructHandle  pexecstr;
	ddpex4rtn       err;
	register ddULONG    i;
	register diWKSHandle *ppwks;

	*ppDestCSSElement = (miGenericElementPtr) NULL;

	err = (*CopyOCTable[(int) (pSrcCSSElement->element.elementType)])
		(pSrcCSSElement, ppDestCSSElement);
	if (err != Success)
		return (err);

	/*
	 * update the structure cross reference lists the id in execute
	 * structure elements was replaced with the diStructHandle by diPEX
	 */
	pexecstr = (diStructHandle) MISTR_GET_EXSTR_STR(pSrcCSSElement);

	/* update pStructs children list */
	err = UpdateStructRefs(pDestStruct, (diResourceHandle) pexecstr,
			       CHILD_STRUCTURE_RESOURCE, ADD);
	if (err != Success)
		return (err);

	/* update the executed structs parent list */
	err = UpdateStructRefs(pexecstr, (diResourceHandle) pDestStruct,
			       PARENT_STRUCTURE_RESOURCE, ADD);
	if (err != Success)
		return (err);

	/* update the wks appears on lists of the subordinate structure */
	ADD_WKS(pheader->wksPostedTo, pexecstr);
	ADD_WKS(pheader->wksAppearOn, pexecstr);

	MISTR_NUM_EL(pheader)++;
	MISTR_LENGTH(pheader) += MISTR_EL_LENGTH(pSrcCSSElement);

	(*ppDestCSSElement)->pStruct = pDestStruct;

	MISTR_EL_LENGTH(*ppDestCSSElement) = MISTR_EL_LENGTH(pSrcCSSElement);
	MISTR_EL_TYPE(*ppDestCSSElement) = MISTR_EL_TYPE(pSrcCSSElement);

	return (Success);
}

ddpex4rtn
replaceCSS_Exec_Struct(pStruct, pCSSElement, pPEXOC)
/* in */
	diStructHandle  pStruct;
	miGenericElementPtr pCSSElement;
	pexExecuteStructure *pPEXOC;
{
	SET_STR_HEADER(pStruct, pheader);
	diStructHandle  pexecstr;
	ddpex4rtn       err;
	register ddULONG    i;
	register diWKSHandle *ppwks;

	pexecstr = (diStructHandle) MISTR_GET_EXSTR_STR(pCSSElement);
	if (pexecstr != *((diStructHandle *)&(pPEXOC->id))) {

		/*
		 * update the structure cross reference lists the id in
		 * execute structure elements was replaced with the
		 * diStructHandle by diPEX
		 */

		/* update pStructs children list  REMOVE the old element */
		UpdateStructRefs(pStruct, (diResourceHandle) pexecstr,
				 CHILD_STRUCTURE_RESOURCE, REMOVE);

		/* update the executed structs parent list REMOVE the parent */
		UpdateStructRefs(pexecstr, (diResourceHandle) pStruct,
				 PARENT_STRUCTURE_RESOURCE, REMOVE);

		/*
		 * update the wks appears on lists of the subordinate
		 * structure
		 */
		REMOVE_WKS(pheader->wksPostedTo, pexecstr);
		REMOVE_WKS(pheader->wksAppearOn, pexecstr);

		/* update the new executed structs parent list */
		err = UpdateStructRefs(	*((diStructHandle *)&(pPEXOC->id)),
					(diResourceHandle) pStruct,
					PARENT_STRUCTURE_RESOURCE, ADD);
		if (err != Success) return (err);

		/* update pStructs children list with the new child */
		err = UpdateStructRefs(	pStruct,
					*((diResourceHandle *)&(pPEXOC->id)),
					CHILD_STRUCTURE_RESOURCE, ADD);
		if (err != Success) return (err);

		/*
		 * update the wks appears on lists of the subordinate
		 * structure
		 */
		ADD_WKS(pheader->wksPostedTo,*((diStructHandle *)&(pPEXOC->id)));
		ADD_WKS(pheader->wksAppearOn,*((diStructHandle *)&(pPEXOC->id)));

		MISTR_PUT_EXSTR_STR(pCSSElement, pPEXOC->id);
	}
	return (Success);
}


ddpex4rtn
inquireCSS_Exec_Struct(pCSSElement, pBuf, ppPEXOC)
	miGenericElementPtr pCSSElement;
	ddBuffer       *pBuf;
	ddElementInfo  **ppPEXOC;
{
	ddpex4rtn       err = Success;
	pexExecuteStructure	*pexstr;

	err = (*InquireOCTable[(int) (pCSSElement->element.elementType)])
		(pCSSElement, pBuf, ppPEXOC);

	pexstr = (pexExecuteStructure *)*ppPEXOC;
	pexstr->id = (*((diStructHandle *)&(pexstr->id)))->id;
	return (err);
}
