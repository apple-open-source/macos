/* $Xorg: pexUtils.c,v 1.4 2001/02/09 02:04:14 xorgcvs Exp $ */
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


Copyright 1989, 1990, 1991 by Sun Microsystems, Inc. and The Open Group.

                        All Rights Reserved

Permission to use, copy, modify, and distribute this software and its
documentation for any purpose and without fee is hereby granted,
provided that the above copyright notice appear in all copies and that
both that copyright notice and this permission notice appear in
supporting documentation, and that the name of Sun Microsystems,
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
/* $XFree86: xc/programs/Xserver/PEX5/dipex/dispatch/pexUtils.c,v 3.8 2001/12/14 19:57:41 dawes Exp $ */

#include "ddpex.h"
#include "pexUtils.h"
#include "PEX.h"
#include "misc.h"
#include "pexos.h"


/*++
 |
 |  Function Name:	puBufferRealloc
 |
 |  Function Description:
 |	increses the size of the buffer provided by diPEX.
 |
 |  Input Description:
	ddBufferPtr	pBuffer;
	ddULONG		minSize;
 |
 |  Output Description:
	ddBufferPtr	pBuffer;
 |
 |  Note(s):
 |	For now only does increase to min size given (in bytes).  Maybe
 |	it should make buffer size multiple of 2?
 |
 --*/

int
puBuffRealloc(pBuffer, minSize)
    ddBufferPtr         pBuffer;
    ddULONG             minSize;
{
    ddPointer           newbuf;
    int                 hsiz = PU_BUF_HDR_SIZE(pBuffer);

    minSize += hsiz;

    if ((newbuf = (ddPointer) xrealloc(	(pointer)(pBuffer->pHead),
					(unsigned long)minSize))
	== NULL)
    {
	pBuffer->dataSize = 0;
	return (BadAlloc);
    }
    pBuffer->pHead = newbuf;
    pBuffer->bufSize = minSize;
    pBuffer->pBuf = (ddPointer) ((char *) pBuffer->pHead) + hsiz;
    return (Success);
}

/****** STUFF FOR MANAGING LISTOFOBJ *************/
/* listofObj is a generic structure defined in ddpex.h for storing
 * a list of items in.  The enum 'ddtype' specifies the possible items
 * that a listofObj can hold.  The following routines are used to manage the
 * lists.  These lists can be used by dipex, ddpex, or both.  They are
 * NOT intended to be completely opaque objects!  dipex or ddpex can
 * manipulate the lists themselves, without using any of these routines.
 * The routines are provided as a convenience. However, since dipex does
 * use them, they must be provided and must behave as expected.
 * The lists are maintained as an array. The array is reallocated if it
 * is not large enough.  Array is used instead of a linked list because
 * a list of items can be easily copied into the array as a chunk and
 * the chunk can be easily traversed by incrementing a pointer to it.
 * One time when the array may be a disadvantage over linked lists
 * is if the array is not large enough and has to be realloced.  Then
 * the current contents are copied into the new array.  It should be
 * an efficient copy since the system realloc routined does this, however
 * it is a copy.  The size of the array is tunable (see object_array_sizes
 * below) and may be adjusted for better performance.
 * Also, when an item is removed, the end of the list
 * is moved up one slot (copied) to fill in where the item is taken
 * out. This should be evaluated to determine if it's effect on system
 * performance is significant enough to change the method used.
 * An alternative way to implement the lists would be to use a list of
 * pointers to the items. This may or may not be better.
 */

/* the arrays are lists of the following elements
 * this provides the size of on element
 * this list is indexed by the list type
 */
static unsigned	long obj_struct_sizes[] = {
	sizeof(ddElementRef),
	sizeof(ddHalfSpace),
	sizeof(ddPickPath),
	sizeof(ddRendererPtr),
	sizeof(diWKSHandle),
	sizeof(diNSHandle),
	sizeof(diStructHandle),
	sizeof(ddDeviceRect),
	sizeof(ddULONG),
	sizeof(ddUSHORT),
	sizeof(ddPointer)
};

/* sizes of chunks that arrays in the lists are allocated and grow in 
 * are specified here
 * separate sizes are defined for each obj type so allocations
 * for the arrays can be tuned individually 
 * this list is indexed by the list type
 */
/* here is my philosophy for setting these sizes.  This is based on my
 * uneducated ideas of what the clients may do, not on any real 
 * evaluation of what happens.
 *	element ref: used for current path in the renderer.  Could be a long
 *		list if the (client) structure network is large
 *	half space: for model clip volume.  I'm not sure how this will be
 *		used, so I'll leave it kind of small - maybe the clients
 *		don't know how to use it either (boo, hiss, bad assumption)
 *	pick path: for the initial pick path. Could be large for large structure
 *		network
 *	renderer pointer: used by PC, LUT, NS for lists of renderers which 
 *		reference them.  I don't expect any one of these resources to be 
 *		shared by many renderers, so this is very small.  
 *	wks handle:  used by LUT, NS, Structures for cross-reference list.
 *		Since structure networks could be large and a single structre
 *		may be indirectly posted to a workstation multiple times,
 *		this is largish.
 *	name set handle:  I don't remember
 *	structure handle: used by other structures for ancestors and descendants
 *		lists (a.k.a. parents & children).  Since this only keeps
 *		track one level above or below the structure, it probably won't
 *		get very large, but it will probably have several entries.
 *	device rect: for the clip list.  I think this is used for window 
 *		clipping (?), so it may get large.
 *	index: is used for list state in the pc. 10 lights is a lot
 *	list of list: used for doing list of pick path for PickAll 
 *  You can adjust these any way you'd like.  Maybe you want them all to be
 *	built in small chunks so not so much (possibly unused) memory is
 *	laying around and are willing to take the performance hit whenever
 *	the array needs to be reallocated.
 */

static unsigned long obj_array_sizes[] = {
	50,	/* element ref */
	10,	/* half space */
	50,	/* pick path */
	5,	/* renderer pointer */
	30,	/* wks handle */
	10,	/* name set handle */
	20,	/* structure handle */
	30,	/* device rect */
	10,	/* name */
	10,	/* index */
	50	/* list of list */
};

#define PU_CHECK_LIST( plist )	if (!plist)	 return( PU_BAD_LIST )

/*
 * XXX - calls FatalError if passed a pList which has had the objects
 * allocated right after the header
 */

#define	PU_GROW_LIST( plist, atleast ) \
{	\
    register int newmax; \
    ddPointer pList;	\
	\
    newmax = obj_array_sizes[(int)(plist->type)] + plist->maxObj; \
    if (newmax < (atleast)) \
	newmax = (atleast); \
    if (plist->pList == (ddPointer) (plist + 1)) \
	FatalError("PU_GROW_LIST passed a pList which has had the objects"\
		   "allocated right after the header"); \
    pList = (ddPointer)xrealloc( (pointer)(plist->pList), 	\
		(unsigned long)(newmax * obj_struct_sizes[(int)(plist->type)] ));	\
    if (!pList ) return( BadAlloc );	\
	\
    plist->maxObj = (ddLONG)newmax;	\
    plist->pList = pList;	\
}

#define PU_ELREF_COMPARE( p1, p2 ) \
	( (p1)->structure == (p2)->structure ) &&  \
	( (p1)->offset == (p2)->offset )

#define PU_SPACE_COMPARE( p1, p2 ) \
	( (p1)->point.x == (p2)->point.x ) &&  \
	( (p1)->point.y == (p2)->point.y ) &&  \
	( (p1)->point.z == (p2)->point.z ) &&  \
	( (p1)->vector.x == (p2)->vector.x ) &&  \
	( (p1)->vector.y == (p2)->vector.y ) &&  \
	( (p1)->vector.z == (p2)->vector.z )

#define PU_PATH_COMPARE( p1, p2 ) \
	( (p1)->structure == (p2)->structure ) &&	\
	( (p1)->offset == (p2)->offset ) &&	\
	( (p1)->pickid == (p2)->pickid )

#define PU_HANDLE_COMPARE( p1, p2 ) \
	( *(p1) == *(p2) ) 

#define PU_RECT_COMPARE( p1, p2 ) \
	( (p1)->xmin == (p2)->xmin ) &&  \
	( (p1)->xmax == (p2)->xmax ) &&  \
	( (p1)->ymin == (p2)->ymin ) &&  \
	( (p1)->ymax == (p2)->ymax )

#define PU_NAME_COMPARE( p1, p2 ) \
	( *(p1) == *(p2) ) 

#define PU_INDEX_COMPARE( p1, p2 ) \
	( *(p1) == *(p2) ) 

#define PU_LIST_COMPARE( p1, p2 ) \
	( *(p1) == *(p2) ) 

/*
    Returns the number of bytes needed to create a list of this type
 */
int
puCountList( type, at_least_num )
ddListType	type;
int		at_least_num;
{
    return(sizeof(listofObj) + at_least_num * obj_struct_sizes[(int)type] );
}	/* puCountList */

/*
    Initializes fields in listofObj
 */
void
puInitList( pList, type, maxEntries)
listofObj   *pList;
ddListType  type;
ddULONG	    maxEntries;
{
    pList->type = type;
    pList->numObj = 0;
    pList->maxObj = maxEntries;
    if (maxEntries == 0) pList->pList = 0;
    else pList->pList  = (ddPointer)(pList + 1);    /* allocated in one chunk */

}

listofObj *
puCreateList( type )
ddListType	type;
{
    listofObj	*pList;

    /* allocate in two chunks */
    pList = (listofObj *)xalloc( sizeof(listofObj) );
    if ( !pList ) return NULL;

    pList->type = type;
    pList->numObj = 0;
    pList->maxObj = obj_array_sizes[(int)type];

    if (!pList->maxObj)
	pList->pList = (ddPointer) NULL;
    else
	pList->pList = (ddPointer) xalloc (pList->maxObj * obj_struct_sizes[(int)type]);

    if (!pList->pList)
    {
	xfree (pList);
	return NULL;
    }

    return( pList );
}	/* puCreateList */

void
puDeleteList( pList )
listofObj	*pList;
{
    if ( pList ) 
    {
	if (pList->pList && pList->pList != (ddPointer) (pList + 1))
	    xfree ((pointer) pList->pList);
	xfree( (pointer)pList );
    }
    return;
}	/* puDeleteList */

/* notice that this macro depends on i, plist, pi, and pl being defined */
#define PU_COMPARE_LOOP( compare_macro )	\
	for ( i=0; i<plist->numObj; i++, pl++ )	\
		if ( compare_macro )	\
			return( PU_TRUE )

/* see if an item is in the list. return TRUE if it is, else return FALSE
 */
short
puInList( pitem, plist )
	ddPointer	pitem;
	listofObj	*plist;
{
	register int i;

	if (!plist) return( PU_FALSE );
	if (!plist->numObj) return(PU_FALSE);

	switch ( plist->type )
	{
		case	DD_ELEMENT_REF:
		{
			ddElementRef	*pi = (ddElementRef *)pitem;
			ddElementRef	*pl = (ddElementRef *)plist->pList;

			PU_COMPARE_LOOP( PU_ELREF_COMPARE( pi, pl ) );
		}
		break;
	
		case	DD_HALF_SPACE:
		{
			ddHalfSpace	*pi = (ddHalfSpace *)pitem;
			ddHalfSpace	*pl = (ddHalfSpace *)plist->pList;

			PU_COMPARE_LOOP( PU_SPACE_COMPARE( pi, pl ) );
		}
		break;
	
		case	DD_PICK_PATH:
		{
			ddPickPath	*pi = (ddPickPath *)pitem;
			ddPickPath	*pl = (ddPickPath *)plist->pList;

			PU_COMPARE_LOOP( PU_PATH_COMPARE( pi, pl ) );
		}
		break;
	
		case	DD_RENDERER:
		case	DD_WKS:
		case	DD_NS:
		case	DD_STRUCT:
		{
			ddPointer	*pi = (ddPointer *)pitem;
			ddPointer	*pl = (ddPointer *)plist->pList;

			PU_COMPARE_LOOP( PU_HANDLE_COMPARE( pi, pl ) );
		}
		break;
	
		case	DD_DEVICE_RECT:
		{
			ddDeviceRect	*pi = (ddDeviceRect *)pitem;
			ddDeviceRect	*pl = (ddDeviceRect *)plist->pList;

			PU_COMPARE_LOOP( PU_RECT_COMPARE( pi, pl ) );
		}
		break;

		case	DD_NAME:
		{
			ddULONG	*pi = (ddULONG *)pitem;
			ddULONG	*pl = (ddULONG *)plist->pList;

			PU_COMPARE_LOOP( PU_NAME_COMPARE( pi, pl ) );
		}
		break;
	
		case	DD_INDEX:
		{
			ddUSHORT	*pi = (ddUSHORT *)pitem;
			ddUSHORT	*pl = (ddUSHORT *)plist->pList;

			PU_COMPARE_LOOP( PU_INDEX_COMPARE( pi, pl ) );
		}
		break;
	
		case	DD_LIST_OF_LIST:
		{
			listofObj	**pi = (listofObj **)pitem;
			listofObj	**pl = (listofObj **)plist->pList;

			PU_COMPARE_LOOP( PU_LIST_COMPARE( pi, pl ) );
		}
		break;

		default:
			return( PU_FALSE );
	}

	return( PU_FALSE );
}	/* puInList */

/* add numItems items to the end of the list. 
 * duplicates the item if it's already in the list
 * returns Success (0) if successful
 * returns BadAlloc error if not
 */
short
puAddToList( pitem, numItems, plist )
	ddPointer	pitem;
	ddULONG 	numItems;
	listofObj	*plist;
{
	ddPointer	pi2;

	PU_CHECK_LIST( plist );

	if ( !numItems )
		return( Success );

	/* macro returns error if can't allocate space */
	if ( plist->numObj + numItems > plist->maxObj )
		PU_GROW_LIST( plist, plist->numObj + numItems );

	pi2 = &(plist->pList[ obj_struct_sizes[(int)(plist->type)] * plist->numObj ]);
	/* JSH - assuming copy may overlap */
	memmove( (char *)pi2, (char *)pitem, (int)(numItems * obj_struct_sizes[(int)(plist->type)]) );

	plist->numObj += numItems;

	return( Success );
}	/* puAddToList */

/* notice that this macro depends on vars i, numObj, pi, and pl to be defined */
#define	PU_REMOVE_LOOP( compare_macro )	\
	for ( i=0; i<numObj; i++, pl++ )	\
	{	\
		if ( compare_macro )	\
		{	\
			for ( ; i<numObj; i++, pl++ )	\
				*pl = *(pl+1);	\
	\
			/* put this here so it isn't 	\
			 * decremented if the item is	\
			 * not in the list	\
			 */	\
			plist->numObj--;	\
		}	\
	}

/* to remove the last element */
#define PU_REMOVE_LAST_ELEMENT( plist )	\
	(plist)->numObj--

/* removes first occurence of pitem found in list */
short
puRemoveFromList( pitem, plist )
	ddPointer pitem;
	listofObj	*plist;
{
	register	unsigned long i;
	register	unsigned long numObj = plist->numObj;

	PU_CHECK_LIST( plist );

	if ( !numObj )
		return( Success );

	switch ( plist->type )
	{
		case	DD_ELEMENT_REF:
		{
			ddElementRef	*pi = (ddElementRef *)pitem;
			ddElementRef	*pl = (ddElementRef *)plist->pList;

			PU_REMOVE_LOOP( PU_ELREF_COMPARE( pi, pl ) );
				
		}
		break;
	
		case	DD_HALF_SPACE:
		{
			ddHalfSpace	*pi = (ddHalfSpace *)pitem;
			ddHalfSpace	*pl = (ddHalfSpace *)plist->pList;

			PU_REMOVE_LOOP( PU_SPACE_COMPARE( pi, pl ) );
		}
		break;
	
		case	DD_PICK_PATH:
		{
			ddPickPath	*pi = (ddPickPath *)pitem;
			ddPickPath	*pl = (ddPickPath *)plist->pList;

			PU_REMOVE_LOOP( PU_PATH_COMPARE( pi, pl ) );
		}
		break;
	
		case	DD_RENDERER:
		case	DD_WKS:
		case	DD_NS:
		case	DD_STRUCT:
		{
			ddPointer	*pi = (ddPointer *)pitem;
			ddPointer	*pl = (ddPointer *)plist->pList;
			PU_REMOVE_LOOP( PU_HANDLE_COMPARE( pi, pl ) );
		}
		break;
	
		case	DD_DEVICE_RECT:
		{
			ddDeviceRect	*pi = (ddDeviceRect *)pitem;
			ddDeviceRect	*pl = (ddDeviceRect *)plist->pList;

			PU_REMOVE_LOOP( PU_RECT_COMPARE( pi, pl ) );
		}
		break;

		case	DD_NAME:
		{
			ddULONG	*pi = (ddULONG *)pitem;
			ddULONG	*pl = (ddULONG *)plist->pList;

			PU_REMOVE_LOOP( PU_NAME_COMPARE( pi, pl ) );
		}
		break;
	
		case	DD_INDEX:
		{
			ddUSHORT	*pi = (ddUSHORT *)pitem;
			ddUSHORT	*pl = (ddUSHORT *)plist->pList;

			PU_REMOVE_LOOP( PU_INDEX_COMPARE( pi, pl ) );
		}
		break;
	
		case	DD_LIST_OF_LIST:
		{
			listofObj	**pi = (listofObj **)pitem;
			listofObj	**pl = (listofObj **)plist->pList;

			PU_REMOVE_LOOP( PU_LIST_COMPARE( pi, pl ) );

		}
		break;
	
		default:
			return( PU_BAD_LIST );
	}

	return( Success );
}	/* puRemoveFromList */

/* this macro assumes that pldest has enough memory */
/* JSH - assuming copy may overlap */
#define	PU_COPY_LIST_ELEMENTS( plsrc, pldest, bytes )	\
	memmove( (char *)(pldest), (char *)(plsrc), (int)(bytes) )

short
puCopyList( psrc, pdest )
	listofObj *psrc;
	listofObj *pdest;
{
	/* both lists must exist */
	if ( !psrc || !pdest )
		return( PU_BAD_LIST );

	/* and must be the same type */
	if ( !(psrc->type == pdest->type) )
		return( PU_BAD_LIST );

	if ( !psrc->numObj )
	{
		pdest->numObj = 0;
		return( Success );
	}

	if ( psrc->numObj > pdest->maxObj )
		PU_GROW_LIST( pdest , psrc->numObj );

	PU_COPY_LIST_ELEMENTS( psrc->pList, pdest->pList, 
			obj_struct_sizes[(int)(psrc->type)] * psrc->numObj );

	pdest->numObj = psrc->numObj;
	
	return( Success );
}	/* puCopyList */


/* merges two lists into one list without duplicates */
short
puMergeLists( psrc1, psrc2, pdest )
	listofObj *psrc1;
	listofObj *psrc2;
	listofObj *pdest;
{
	register int i,si;	
	ddPointer pi;
	listofObj *ptemp;

	if ((pdest==psrc1 && psrc1->numObj) ||
	    (pdest==psrc2 && psrc2->numObj))
	{
		ptemp = puCreateList( psrc1->type );
		if (!ptemp)
			return(BadAlloc);
	}
	else
		ptemp = pdest;

	/* all lists must exist */
	if ( !psrc1 || !psrc2 || !ptemp )
		return( PU_BAD_LIST );

	/* and must be the same type */
	if ( !((psrc1->type == psrc2->type) && (psrc2->type == ptemp->type)) )
		return( PU_BAD_LIST );

	/* just for kicks, let's make sure the destination list is empty */
	ptemp->numObj = 0;

	/* put the first list into the destination */
	if ( psrc1->numObj )
	{
		pi = psrc1->pList;
		si = obj_struct_sizes[(int)(psrc1->type)];
		for ( i=0; i<psrc1->numObj; i++, pi+=si )
			/* if the item is not already in the list , add it */
			if ( !puInList( pi, ptemp ) )
				if ( puAddToList( pi, (ddULONG)1, ptemp ) )
					return( BadAlloc );
	}

	/* now for the second list */
	if ( psrc2->numObj )
	{
		pi = psrc2->pList;
		si = obj_struct_sizes[(int)(psrc2->type)];
		for ( i=0; i<psrc2->numObj; i++, pi+=si )
			/* if the item is not already in the list , add it */
			if ( !puInList( pi, ptemp ) )
				if ( puAddToList( pi, (ddULONG)1, ptemp ) )
					return( BadAlloc );
	}

	if (ptemp != pdest)
	{
		if ( puCopyList( ptemp, pdest ) != Success )
			return(BadAlloc);
		puDeleteList( ptemp );
	}
	return( Success );
}	/* puMergeLists */

#ifdef DDTEST
short
printlist( plist )
	listofObj	*plist;
{
	register int i;

	PU_CHECK_LIST( plist );

	ErrorF("\nLIST type:  %d,  size:  %d,  num:  %d",
		plist->type, plist->maxObj, plist->numObj );

	switch ( plist->type )
	{
		case	DD_ELEMENT_REF:
		{
			ddElementRef	*pl=(ddElementRef *)plist->pList;

			ErrorF("  ELEMENT REF\n");
			for ( i=0; i<plist->numObj; i++, pl++ )
				ErrorF("structure: %d  offset:  %d\n",
					pl->structure, pl->offset );	
		}
		break;
	
		case	DD_HALF_SPACE:
		{
			ddHalfSpace	*pl=(ddHalfSpace *)plist->pList;

			ErrorF("  HALF SPACE\n");
			for ( i=0; i<plist->numObj; i++, pl++ )
				ErrorF("point: %f  %f  %f   vector:  %f  %f  %f\n",
					pl->point.x, pl->point.y, pl->point.z,
					pl->vector.x, pl->vector.y, pl->vector.z);
		}
		break;
	
		case	DD_PICK_PATH:
		{
			ddPickPath	*pl=(ddPickPath *)plist->pList;

			ErrorF("  PICK PATH\n");
			for ( i=0; i<plist->numObj; i++, pl++ )
				ErrorF("structure: %d  offset:  %d  pick id:  %d\n",
					pl->structure, pl->offset, pl->pickid );	
		}
		break;
	
		case	DD_RENDERER:
		case	DD_WKS:
		case	DD_NS:
		case	DD_STRUCT:
		{
			ddPointer   *pl=(ddPointer *)plist->pList;

			ErrorF("  HANDLE\n");
			for ( i=0; i<plist->numObj; i++, pl++ )
				ErrorF("handle: %d \n", *pl);
		}
		break;
	
		case	DD_DEVICE_RECT:
		{
			ddDeviceRect	*pl=(ddDeviceRect *)plist->pList;

			ErrorF("  DEVICE RECT\n");
			for ( i=0; i<plist->numObj; i++, pl++ )
				ErrorF("xmin: %d  xmax: %d  ymin: %d  ymax: %d\n",
					pl->xmin, pl->xmax, pl->ymin, pl->ymax );	
		}
		break;

		case	DD_NAME:
		{
			ddULONG   *pl=(ddULONG *)plist->pList;

			ErrorF("  NAME\n");
			for ( i=0; i<plist->numObj; i++, pl++ )
				ErrorF("name: %d \n", *pl);
		}
		break;
	
		case	DD_INDEX:
		{
			ddUSHORT   *pl=(ddUSHORT *)plist->pList;

			ErrorF("  INDEX\n");
			for ( i=0; i<plist->numObj; i++, pl++ )
				ErrorF("index: %d \n", *pl);
		}
		break;
	
		default:
			return( PU_BAD_LIST );
	}
}	/* printlist */
#endif

