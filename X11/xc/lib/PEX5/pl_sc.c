/* $Xorg: pl_sc.c,v 1.4 2001/02/09 02:03:29 xorgcvs Exp $ */
/*

Copyright 1992, 1998  The Open Group

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

*/

/******************************************************************************
Copyright 1987,1991 by Digital Equipment Corporation, Maynard, Massachusetts
Copyright 1992 by ShoGraphics, Inc., Mountain View, California

                        All Rights Reserved

Permission to use, copy, modify, distribute, and sell this software and its
documentation for any purpose is hereby granted without fee, provided that
the above copyright notice appear in all copies and that both that copyright
notice and this permission notice appear in supporting documentation, and that
the name of Digital or ShowGraphics not be used in advertising or
publicity pertaining to distribution of the software without specific, written
prior permission.  Digital and ShowGraphics make no representations
about the suitability of this software for any purpose.  It is provided "as is"
without express or implied warranty.

DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.

SHOGRAPHICS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
SHOGRAPHICS BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
SOFTWARE.
*************************************************************************/

#include "PEXlib.h"
#include "PEXlibint.h"

static void _PEXGenerateSCList();


PEXSearchContext
PEXCreateSearchContext (display, valueMask, values)

INPUT Display		*display;
INPUT unsigned long	valueMask;
INPUT PEXSCAttributes	*values;

{
    register pexCreateSearchContextReq	*req;
    char				*pBuf;
    PEXSearchContext			id;
    int					size = 0;
    char				*pList;
    int					fpConvert;
    int					fpFormat;


    /*
     * Get a search context resource id from X.
     */

    id = XAllocID (display);


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (CreateSearchContext, pBuf);

    BEGIN_REQUEST_HEADER (CreateSearchContext, pBuf, req);
    CHECK_FP (fpConvert, fpFormat);

    PEXStoreFPReqHead (CreateSearchContext, fpFormat, req);
    req->sc = id;
    req->itemMask = valueMask;

    if (valueMask != 0)
    {
	_PEXGenerateSCList (display, fpConvert, fpFormat,
	    valueMask, values, &size, &pList);

	req->length += NUMWORDS (size);
    }

    END_REQUEST_HEADER (CreateSearchContext, pBuf, req);


    /*
     * Send the list of values.
     */

    if (size > 0)
	Data (display, pList, size);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (id);
}


void
PEXFreeSearchContext (display, sc)

INPUT Display		*display;
INPUT PEXSearchContext	sc;

{
    register pexFreeSearchContextReq	*req;
    char				*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (FreeSearchContext, pBuf);

    BEGIN_REQUEST_HEADER (FreeSearchContext, pBuf, req);

    PEXStoreReqHead (FreeSearchContext, req);
    req->id = sc;

    END_REQUEST_HEADER (FreeSearchContext, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


void
PEXCopySearchContext (display, valueMask, srcSc, destSc)

INPUT Display		*display;
INPUT unsigned long	valueMask;
INPUT PEXSearchContext	srcSc;
INPUT PEXSearchContext	destSc;

{
    register pexCopySearchContextReq	*req;
    char				*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (CopySearchContext, pBuf);

    BEGIN_REQUEST_HEADER (CopySearchContext, pBuf, req);

    PEXStoreReqHead (CopySearchContext, req);
    req->src = srcSc;
    req->dst = destSc;
    req->itemMask = valueMask;

    END_REQUEST_HEADER (CopySearchContext, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


PEXSCAttributes *
PEXGetSearchContext (display, sc, valueMask)

INPUT Display		*display;
INPUT PEXSearchContext	sc;
INPUT unsigned long	valueMask;

{
    register pexGetSearchContextReq	*req;
    register char			*pBuf, *pBufSave;
    pexGetSearchContextReply		rep;
    PEXSCAttributes			*scattr;
    unsigned long			f;
    unsigned				count;
    int					i;
    PEXListOfNameSetPair		*pList;
    int					fpConvert;
    int					fpFormat;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    PEXGetReq (GetSearchContext, pBuf);

    BEGIN_REQUEST_HEADER (GetSearchContext, pBuf, req);
    CHECK_FP (fpConvert, fpFormat);

    PEXStoreFPReqHead (GetSearchContext, fpFormat, req);
    req->sc = sc;
    req->itemMask = valueMask;

    END_REQUEST_HEADER (GetSearchContext, pBuf, req);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
	UnlockDisplay (display);
  	PEXSyncHandle (display);
 	return (NULL);               /* return an error */
    }


    /*
     * Read the reply data into a scratch buffer.
     */

    XREAD_INTO_SCRATCH (display, pBufSave, rep.length << 2);
    pBuf = pBufSave;

    /*
     * Allocate a buffer for the replies to pass back to the client.
     */

    scattr = (PEXSCAttributes *) Xmalloc (sizeof (PEXSCAttributes));

    scattr->start_path.count = 0;
    scattr->start_path.elements = NULL;
    scattr->normal.count = 0;
    scattr->normal.pairs = NULL;
    scattr->inverted.count = 0;
    scattr->inverted.pairs = NULL;

    for (i = 0; i < (PEXSCMaxShift + 1); i++)
    {
	f = (1L << i);
	if (valueMask & f)
	{
	    switch (f)
	    {
	    case PEXSCPosition:

		EXTRACT_COORD3D (pBuf, scattr->position, fpConvert, fpFormat);
	  	break;

	    case PEXSCDistance:
		
		EXTRACT_FLOAT32 (pBuf, scattr->distance, fpConvert, fpFormat);
		break;

	    case PEXSCCeiling:

	        EXTRACT_LOV_CARD16 (pBuf, scattr->ceiling);
		break;

	    case PEXSCModelClipFlag:

	        EXTRACT_LOV_CARD8 (pBuf, scattr->model_clip_flag);
		break;

	    case PEXSCStartPath:

	        EXTRACT_CARD32 (pBuf, count);
	        scattr->start_path.count = count;

		scattr->start_path.elements = (PEXElementRef *) Xmalloc (
		    count * sizeof (PEXElementRef));

		EXTRACT_LISTOF_ELEMREF (count, pBuf,
		    scattr->start_path.elements);
		break;

	    case PEXSCNormalList:
	    case PEXSCInvertedList:

	        if (f == PEXSCNormalList)
                    pList = &scattr->normal;
                else
                    pList = &scattr->inverted;

	        EXTRACT_CARD32 (pBuf, count);
		pList->count = count;

		pList->pairs = (PEXNameSetPair *) Xmalloc (
		    count * sizeof (PEXNameSetPair));

		EXTRACT_LISTOF_NAMESET_PAIR (count, pBuf, pList->pairs);
		break;
	    }
	}
    }

    FINISH_WITH_SCRATCH (display, pBufSave, rep.length << 2);

    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (scattr);
}


void
PEXChangeSearchContext (display, sc, valueMask, values)

INPUT Display		*display;
INPUT PEXSearchContext	sc;
INPUT unsigned long	valueMask;
OUTPUT PEXSCAttributes	*values;

{
    register pexChangeSearchContextReq	*req;
    char				*pBuf;
    int					size = 0;
    char				*pList;
    int					fpConvert;
    int					fpFormat;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (ChangeSearchContext, pBuf);

    BEGIN_REQUEST_HEADER (ChangeSearchContext, pBuf, req);
    CHECK_FP (fpConvert, fpFormat);

    PEXStoreFPReqHead (ChangeSearchContext, fpFormat, req);
    req->sc = sc;
    req->itemMask = valueMask;

    if (valueMask != 0)
    {
	_PEXGenerateSCList (display, fpConvert, fpFormat,
	    valueMask, values, &size, &pList);

	req->length += NUMWORDS (size);
    }

    END_REQUEST_HEADER (ChangeSearchContext, pBuf, req);


    /*
     * Send the list of values.
     */

    if (size > 0)
	Data (display, pList, size);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


Status
PEXSearchNetwork (display, sc, path_return)

INPUT Display			*display;
INPUT PEXSearchContext		sc;
OUTPUT PEXStructurePath		**path_return;

{
    register pexSearchNetworkReq	*req;
    char				*pBuf;
    pexSearchNetworkReply		rep;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    PEXGetReq (SearchNetwork, pBuf);

    BEGIN_REQUEST_HEADER (SearchNetwork, pBuf, req);

    PEXStoreReqHead (SearchNetwork, req);
    req->id = sc;

    END_REQUEST_HEADER (SearchNetwork, pBuf, req);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
        UnlockDisplay (display);
        PEXSyncHandle (display);
	*path_return = NULL;
        return (0);               /* return an error */
    }


    /*
     * Allocate a buffer for the path to pass back to the client.
     */

    *path_return = (PEXStructurePath *)
	Xmalloc (sizeof (PEXStructurePath));

    (*path_return)->count = rep.numItems;
    (*path_return)->elements = (PEXElementRef *)
	Xmalloc ((unsigned) (rep.numItems * sizeof (PEXElementRef)));

    XREAD_LISTOF_ELEMREF (display, rep.numItems, (*path_return)->elements);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (1);
}


/*
 * Routine to write a packed list of SC attributes into the transport buf.
 */

static void
_PEXGenerateSCList (display, fpConvert, fpFormat,
    valueMask, values, sizeRet, listRet)

INPUT Display		*display;
INPUT int		fpConvert;
INPUT int		fpFormat;
INPUT unsigned long	valueMask;
INPUT PEXSCAttributes	*values;
OUTPUT int		*sizeRet;
OUTPUT char		**listRet;

{
    register char		*pBuf;
    int				size;
    int				count;
    int				i;
    unsigned long       	f;
    PEXListOfNameSetPair	*pList;


    /*
     * It's not worth the time of determining exactly how much
     * scratch space to allocate, so assume worse case.
     */

    size =
	SIZEOF (pexCoord3D) +
	SIZEOF (float) +
	(5 * SIZEOF (CARD32)) +
	(SIZEOF (pexElementRef) * ((valueMask & PEXSCStartPath) ?
	    values->start_path.count : 0)) +
	(SIZEOF (pexNameSetPair) * ((valueMask & PEXSCNormalList) ?
	    values->normal.count : 0)) +
	(SIZEOF (pexNameSetPair) * ((valueMask & PEXSCInvertedList) ?
	    values->inverted.count : 0));

    pBuf = *listRet = (char *) _XAllocScratch (display, size);

    for (i = 0; i < (PEXSCMaxShift + 1); i++)
    {
	f = (1L << i);
	if (valueMask & f)
	{
	    switch (f)
	    {
	    case PEXSCPosition:

		STORE_COORD3D (values->position, pBuf, fpConvert, fpFormat);
		break;

	    case PEXSCDistance:

		STORE_FLOAT32 (values->distance, pBuf, fpConvert, fpFormat);
		break;

	    case PEXSCCeiling:

		STORE_CARD32 (values->ceiling, pBuf);
		break;

            case PEXSCModelClipFlag:

		STORE_CARD32 (values->model_clip_flag, pBuf);
		break;

	    case PEXSCStartPath:

	        count = values->start_path.count;
	        STORE_CARD32 (count, pBuf);

		STORE_LISTOF_ELEMREF (count,
		    values->start_path.elements, pBuf);
		break;

	    case PEXSCNormalList:
	    case PEXSCInvertedList:

	        if (f == PEXSCNormalList)
                    pList = &values->normal;
                else
                    pList = &values->inverted;

		count = pList->count;
	        STORE_CARD32 (count, pBuf);

		STORE_LISTOF_NAMESET_PAIR (count, pList->pairs, pBuf);
		break;
	    }
	}
    }

    *sizeRet = pBuf - *listRet;
}
