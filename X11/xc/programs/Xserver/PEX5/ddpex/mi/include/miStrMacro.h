/* $Xorg: miStrMacro.h,v 1.4 2001/02/09 02:04:08 xorgcvs Exp $ */

/***********************************************************

Copyright 1989, 1990, 1991, 1998  The Open Group

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that
copyright notice and this permission notice appear in supporting
documentation.

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

Except as contained in this notice, the name of The Open Group shall not be
used in advertising or otherwise to promote the sale, use or other dealings
in this Software without prior written authorization from The Open Group.

Copyright 1989, 1990, 1991 by Sun Microsystems, Inc. 

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its 
documentation for any purpose and without fee is hereby granted, 
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in 
supporting documentation, and that the name of Sun Microsystems,
not be used in advertising or publicity pertaining to distribution of 
the software without specific, written prior permission.  

SUN MICROSYSTEMS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, 
INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT 
SHALL SUN MICROSYSTEMS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL 
DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

******************************************************************/

/* some macros to use */

#include "miStruct.h"
#include "mipex.h"

#ifndef MISTRMACRO_H
#define MISTRMACRO_H

/* structure information */
#define MISTR_NUM_EL(PS)		(PS)->numElements

#define MISTR_EDIT_MODE(PS)		(PS)->editMode

#define MISTR_LENGTH(PS)		(PS)->totalSize

#define MISTR_NUM_CHILDREN(PS)		(PS)->children->numObj

#define MISTR_NUM_PARENTS(PS)		(PS)->parents->numObj

#define MISTR_CURR_EL_OFFSET(PS)	(PS)->currElementOffset

#define MISTR_CURR_EL_PTR(PS)		(PS)->pCurrElement

#define MISTR_ZERO_EL(PS)		(PS)->pZeroElement

#define MISTR_LAST_EL(PS)		(PS)->pLastElement

#define MISTR_NEXT_EL(PE)		(PE)->next

#define MISTR_PREV_EL(PE)		(PE)->prev

#define MISTR_EL_DATA(PE)		((PE)->element)

#define	MISTR_EL_TYPE(PE)		(PE)->element.elementType

#define	MISTR_EL_LENGTH(PE)		(PE)->element.pexOClength

/* todo: make this more efficient by searching backwards if it's closer */
/*
		if (offset < current && offset < curr - off)
			then start at 0 and go forward
		else if (offset < current && offset > curr - off)
			then start at curr and go backward
		else if (offset > current && offset - curr < off - last)
			then start at curr and go forward
		else if (offset > current && offset - curr > off - last)
			then start at last and go backward
*/

#define MISTR_FIND_EL(PSTRUCT, OFFSET, PE)				\
    if ((OFFSET) <= 0)							\
    	(PE) = MISTR_ZERO_EL(PSTRUCT);					\
    else if ((OFFSET) >= MISTR_NUM_EL(PSTRUCT))				\
    	(PE) = MISTR_PREV_EL(MISTR_LAST_EL(PSTRUCT));			\
    else if ((OFFSET) == MISTR_CURR_EL_OFFSET(PSTRUCT))			\
    	(PE) = MISTR_CURR_EL_PTR(PSTRUCT);				\
    else {								\
	register int	_i, _start;					\
    	if ((OFFSET) < MISTR_CURR_EL_OFFSET(PSTRUCT)) {			\
	    (PE) = MISTR_ZERO_EL(PSTRUCT);				\
	    _start = 0;							\
    	} else {							\
	    _start = MISTR_CURR_EL_OFFSET(PSTRUCT);			\
	    (PE) = MISTR_CURR_EL_PTR(PSTRUCT);				\
	}								\
	for (_i=_start; _i<(OFFSET); _i++, (PE) = MISTR_NEXT_EL(PE));	\
    }

/* given pointer to element, find out what its OFFSET is in the struct */
/* the element better be in the struct! */
/* for now, don't know whether the element is near the beginning or end of
 * the structure. todo: add hint to improve efficiency such as 
 * whether to start from the beginning or end or maybe some element
 * and its known OFFSET from whic to start at (and go forwards)
 */
#define	MISTR_FIND_OFFSET(PSTRUCT, PEL, OFFSET)				\
    if (PEL == MISTR_PREV_EL(MISTR_LAST_EL(PSTRUCT)))			\
	(OFFSET) = MISTR_NUM_EL(PSTRUCT);				\
    else {								\
	register int i;							\
	register miGenericElementPtr	ptemp;				\
	for (i = 0, ptemp = MISTR_ZERO_EL(PSTRUCT);			\
		((i < MISTR_NUM_EL(PSTRUCT)) && (ptemp != (PEL)));	\
		i++, ptemp = MISTR_NEXT_EL(ptemp));			\
	(OFFSET) = i;							\
    }
   
/* Must check Proprietary and in Range to avoid Null function ptrs */
#define	MISTR_DEL_ONE_EL(PSTRUCT, PPREV, PEL)	{			\
    MISTR_NEXT_EL(PPREV) = MISTR_NEXT_EL(PEL);				\
    MISTR_PREV_EL(MISTR_NEXT_EL(PEL)) = (PPREV);			\
    if (MI_HIGHBIT_ON(MISTR_EL_TYPE(PEL)))				\
	(*DestroyCSSElementTable[MI_OC_PROP])((PSTRUCT), (PEL));        \
    else								\
      if (MI_IS_PEX_OC(MISTR_EL_TYPE(PEL)))				\
	(*DestroyCSSElementTable[MISTR_EL_TYPE(PEL)])((PSTRUCT), (PEL));\
    }

#define	MISTR_INSERT_ONE_EL(PPREV, PEL)				\
	MISTR_NEXT_EL(PEL) = MISTR_NEXT_EL(PPREV);		\
	MISTR_PREV_EL(MISTR_NEXT_EL(PEL)) = PEL;		\
	MISTR_NEXT_EL(PPREV) = (PEL);				\
	MISTR_PREV_EL(PEL) = (PPREV)

/* PSTRUCT is structure handle; DDSTRUCT is dd structure header */
/* first can't be 0, last can't be more than number in structure */
/* inclusive delete, does not update structure header info */
/* Must check Proprietary and in Range to avoid Null function ptrs */
#define	MISTR_DEL_ELS(PSTRUCT, DDSTRUCT, FIRST, LAST)		\
    if ((int)((LAST) - (FIRST)) >= 0) {				\
	register int	num;					\
	register miGenericElementPtr	pe, pe1, pe2;		\
	MISTR_FIND_EL((DDSTRUCT), (FIRST), pe1);		\
	pe = MISTR_PREV_EL(pe1);				\
	for (num = (FIRST); num <= (LAST); num++) {		\
	    pe2 = MISTR_NEXT_EL(pe1);				\
	    if (MI_HIGHBIT_ON(MISTR_EL_TYPE(pe1)))		\
		(*DestroyCSSElementTable[MI_OC_PROP])           \
				       ((PSTRUCT), pe1);	\
	    else						\
		if (MI_IS_PEX_OC(MISTR_EL_TYPE(pe1)))		\
		  (*DestroyCSSElementTable[MISTR_EL_TYPE(pe1)])	\
				((PSTRUCT), pe1);		\
	    pe1 = pe2;					\
	}							\
	MISTR_NEXT_EL(pe) = pe1;				\
	MISTR_PREV_EL(pe1) = pe;				\
    }

/* macros for accessing specific data in some elements
 * these MUST be changed to reflect the storage format of the elements
 */

/* this macro returns the structure id in an execute structure element.
 * for the SI, this element has the structure handle in the id field
 * can't use these on left side of statement because of casting */
#define	MISTR_GET_EXSTR_STR(PEL)				\
	((pexExecuteStructure *)((PEL)+1))->id

#define	MISTR_GET_EXSTR_ID(PEL)		\
    ((ddStructResource *)((pexExecuteStructure *)((PEL)+1))->id)->id
	
/* use this to change structure in ChangeStructureRefs */
#define	MISTR_PUT_EXSTR_STR(PEL, PSTRUCT)			\
	{   pexExecuteStructure	*pexstr;			\
	    pexstr = (pexExecuteStructure *)((PEL)+1);\
	    pexstr->id = (pexStructure)(PSTRUCT);		\
	}

#define	MISTR_GET_LABEL(PEL)	    ((pexLabel *)((PEL)+1))->label

#define	MISTR_GET_PICK_ID(PEL)	    ((pexPickId *)((PEL)+1))->pickId

#endif /* MISTRMACRO_H */
