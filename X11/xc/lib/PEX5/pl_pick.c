/* $Xorg: pl_pick.c,v 1.4 2001/02/09 02:03:29 xorgcvs Exp $ */
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

/*
 *  NEED EVENTS needs to be defined when including Xproto.h so xEvent
 *  can be sucked in.
 */ 

#define NEED_EVENTS

#include "PEXlib.h"
#include "PEXlibint.h"

#define GetPickRecordSize(_pickType) \
    (_pickType == PEXPickDeviceNPCHitVolume ? SIZEOF (pexPD_NPC_HitVolume) : \
    (_pickType == PEXPickDeviceDCHitBox ? SIZEOF (pexPD_DC_HitBox) : 0))



PEXPickMeasure
PEXCreatePickMeasure (display, wks, devType)

INPUT Display		*display;
INPUT PEXWorkstation	wks;
INPUT int		devType;

{
    register pexCreatePickMeasureReq	*req;
    char				*pBuf;
    PEXPickMeasure			pm;


    /*
     * Get a Pick Measure resource id from X.
     */

    pm = XAllocID (display);


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (CreatePickMeasure, pBuf);

    BEGIN_REQUEST_HEADER (CreatePickMeasure, pBuf, req);

    PEXStoreReqHead (CreatePickMeasure, req);
    req->wks = wks;
    req->pm = pm;
    req->devType = devType;

    END_REQUEST_HEADER (CreatePickMeasure, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (pm);
}


void
PEXFreePickMeasure (display, pm)

INPUT Display		*display;
INPUT PEXPickMeasure	pm;

{
    register pexFreePickMeasureReq	*req;
    char				*pBuf;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (FreePickMeasure, pBuf);

    BEGIN_REQUEST_HEADER (FreePickMeasure, pBuf, req);

    PEXStoreReqHead (FreePickMeasure, req);
    req->id = pm;

    END_REQUEST_HEADER (FreePickMeasure, pBuf, req);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


PEXPMAttributes *
PEXGetPickMeasure (display, pm, valueMask)

INPUT Display			*display;
INPUT PEXPickMeasure		pm;
INPUT unsigned long		valueMask;

{
    register pexGetPickMeasureReq	*req;
    char				*pBuf, *pBufSave;
    pexGetPickMeasureReply		rep;
    PEXPMAttributes			*ppmi;
    unsigned long			f;
    int					i;
    unsigned				count;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    PEXGetReq (GetPickMeasure, pBuf);

    BEGIN_REQUEST_HEADER (GetPickMeasure, pBuf, req);

    PEXStoreReqHead (GetPickMeasure, req);
    req->pm = pm;
    req->itemMask = valueMask;

    END_REQUEST_HEADER (GetPickMeasure, pBuf, req);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
	UnlockDisplay (display);
	PEXSyncHandle (display);
	return (NULL);		  /* return an error */
    }


    /*
     * Read the reply data into a scratch buffer.
     */

    XREAD_INTO_SCRATCH (display, pBufSave, rep.length << 2);
    pBuf = pBufSave;

    /*
     * Allocate a buffer for the replies to pass back to the client.
     */

    ppmi = (PEXPMAttributes *) Xmalloc (sizeof (PEXPMAttributes));

    ppmi->pick_path.count = 0;
    ppmi->pick_path.elements = NULL;

    for (i = 0; i < (PEXPMMaxShift + 1); i++)
    {
	f = (1L << i);
	if (valueMask & f)
	{
	    switch (f)
	    {
	    case PEXPMStatus:

		EXTRACT_LOV_CARD16 (pBuf, ppmi->status);
		break;

	    case PEXPMPath:

		EXTRACT_CARD32 (pBuf, count);
		ppmi->pick_path.count = count;

		ppmi->pick_path.elements = (PEXPickElementRef *)
		    Xmalloc (count * sizeof (PEXPickElementRef));

		EXTRACT_LISTOF_PICKELEMREF (count,
		    pBuf, ppmi->pick_path.elements);
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

    return (ppmi);
}


void
PEXUpdatePickMeasure (display, pick_measure, pick_device_type, pick_record)

INPUT Display		*display;
INPUT PEXPickMeasure	pick_measure;
INPUT int		pick_device_type;
INPUT PEXPickRecord	*pick_record;

{
    register pexUpdatePickMeasureReq	*req;
    char				*pBuf;
    int					numBytes;
    int					fpConvert;
    int					fpFormat;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    numBytes = GetPickRecordSize (pick_device_type);

    PEXGetReqExtra (UpdatePickMeasure, numBytes, pBuf);

    BEGIN_REQUEST_HEADER (UpdatePickMeasure, pBuf, req);
    CHECK_FP (fpConvert, fpFormat);

    PEXStoreReqHead (UpdatePickMeasure, req);
    req->pm = pick_measure;
    req->numBytes = numBytes;

    END_REQUEST_HEADER (UpdatePickMeasure, pBuf, req);

    STORE_PICK_RECORD (pick_device_type, numBytes, pick_record, pBuf,
	fpConvert, fpFormat);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


PEXPDAttributes *
PEXGetPickDevice (display, wks, devType, valueMask)

INPUT Display			*display;
INPUT PEXWorkstation		wks;
INPUT int			devType;
INPUT unsigned long		valueMask;

{
    register pexGetPickDeviceReq	*req;
    char				*pBuf, *pBufSave;
    pexGetPickDeviceReply		rep;
    PEXPDAttributes			*ppdi;
    unsigned long			f;
    int					i;
    unsigned				count;
    int					size;
    int					fpConvert;
    int					fpFormat;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    PEXGetReq (GetPickDevice, pBuf);

    BEGIN_REQUEST_HEADER (GetPickDevice, pBuf, req);
    CHECK_FP (fpConvert, fpFormat);

    PEXStoreFPReqHead (GetPickDevice, fpFormat, req);
    req->wks = wks;
    req->itemMask = valueMask;
    req->devType = devType;

    END_REQUEST_HEADER (GetPickDevice, pBuf, req);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
	UnlockDisplay (display);
	PEXSyncHandle (display);
	return (NULL);		  /* return an error */
    }


    /*
     * Read the reply data into a scratch buffer.
     */

    XREAD_INTO_SCRATCH (display, pBufSave, rep.length << 2);
    pBuf = pBufSave;

    /*
     * Allocate a buffer for the replies to pass back to the client.
     */

    ppdi = (PEXPDAttributes *) Xmalloc (sizeof (PEXPDAttributes));

    ppdi->path.count = 0;
    ppdi->path.elements = NULL;

    for (i = 0; i < (PEXPDMaxShift + 1); i++)
    {
	f = (1L << i);
	if (valueMask & f)
	{
	    switch (f)
	    {
	    case PEXPDPickStatus:

		EXTRACT_LOV_CARD16 (pBuf, ppdi->status);
		break;

	    case PEXPDPickPath:

		EXTRACT_CARD32 (pBuf, count);
		ppdi->path.count = count;

		ppdi->path.elements = (PEXPickElementRef *)
		    Xmalloc (count * sizeof (PEXPickElementRef));

		EXTRACT_LISTOF_PICKELEMREF (count, pBuf, ppdi->path.elements);
		break;

	    case PEXPDPickPathOrder:

		EXTRACT_LOV_CARD16 (pBuf, ppdi->path_order);
		break;

	    case PEXPDPickIncl:

		EXTRACT_CARD32 (pBuf, ppdi->inclusion);
		break;

	    case PEXPDPickExcl:

		EXTRACT_CARD32 (pBuf, ppdi->exclusion);
		break;

	    case PEXPDPickDataRec:

		EXTRACT_CARD32 (pBuf, size);
		EXTRACT_PICK_RECORD (pBuf, devType, size, ppdi->pick_record,
		    fpConvert, fpFormat);
		break;

	    case PEXPDPromptEchoType:

		EXTRACT_LOV_INT16 (pBuf, ppdi->prompt_echo_type);
		break;

	    case PEXPDEchoVolume:

		EXTRACT_VIEWPORT (pBuf, ppdi->echo_volume,
		    fpConvert, fpFormat);
		break;

	    case PEXPDEchoSwitch:

		EXTRACT_LOV_CARD16 (pBuf, ppdi->echo_switch);
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

    return (ppdi);
}


void
PEXChangePickDevice (display, wks, devType, valueMask, values)

INPUT Display		*display;
INPUT PEXWorkstation	wks;
INPUT int		devType;
INPUT unsigned long	valueMask;
INPUT PEXPDAttributes	*values;

{
    register pexChangePickDeviceReq	*req;
    char				*pBuf;
    char				*pSend;
    unsigned long			f;
    unsigned long			size;
    PEXPickRecord			*pick_record;
    int					i;
    int					fpConvert;
    int					fpFormat;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    PEXGetReq (ChangePickDevice, pBuf);

    BEGIN_REQUEST_HEADER (ChangePickDevice, pBuf, req);
    CHECK_FP (fpConvert, fpFormat);

    PEXStoreFPReqHead (ChangePickDevice, fpFormat, req);
    req->wks = wks;
    req->itemMask = valueMask;
    req->devType = devType;


    /*
     * Allocate a scratch buffer used to pack the attributes.  It's not
     * worth computing the exact amount of memory needed, so assume the
     * worse case.
     */

    size = 8 * SIZEOF (CARD32) +
	(SIZEOF (pexPickElementRef) * ((valueMask & PEXPDPickPath) ?
	    values->path.count : 0)) +
	sizeof (PEXPickRecord) +
	SIZEOF (pexViewport);

    pBuf = pSend = (char *) _XAllocScratch (display, size);

    for (i = 0; i < (PEXPDMaxShift + 1); i++)
    {
	f = (1L << i);
	if (valueMask & f)
	{
	    switch (f)
	    {
	    case PEXPDPickStatus:

		STORE_CARD32 (values->status, pBuf);
		break;

	    case PEXPDPickPath:

		STORE_CARD32 (values->path.count, pBuf);
		STORE_LISTOF_PICKELEMREF (values->path.count,
		    values->path.elements, pBuf);
		break;

	    case PEXPDPickPathOrder:

		STORE_CARD32 (values->path_order, pBuf);
		break;

	    case PEXPDPickIncl:

		STORE_CARD32 (values->inclusion, pBuf);
		break;

	    case PEXPDPickExcl:

		STORE_CARD32 (values->exclusion, pBuf);
		break;

	    case PEXPDPickDataRec:

		size = GetPickRecordSize (devType);
		STORE_CARD32 (size, pBuf);

		pick_record = (PEXPickRecord *) &(values->pick_record);
		STORE_PICK_RECORD (devType, size, pick_record, pBuf,
		    fpConvert, fpFormat);
		break;

	    case PEXPDPromptEchoType:

		STORE_CARD32 (values->prompt_echo_type, pBuf);
		break;

	    case PEXPDEchoVolume:

		STORE_VIEWPORT (values->echo_volume, pBuf,
		    fpConvert, fpFormat);
		break;

	    case PEXPDEchoSwitch:

		STORE_CARD32 (values->echo_switch, pBuf);
		break;
	    }
	}
    }

    size = pBuf - pSend;
    req->length += NUMWORDS (size);

    END_REQUEST_HEADER (ChangePickDevice, pBuf, req);

    if (size > 0)
	Data (display, pSend, size);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}




/*
 * Routine to convert a protocol-format event (wire) to a client event
 * structure (client) for a PEXMaxHitsReachedEvent.  XESetWireToEvent is
 * called in PEXInitialize to set this up with Xlib.
 */

Status
_PEXConvertMaxHitsEvent (display, client, wire)

INPUT	Display		*display;
INPUT	XEvent		*client;
INPUT	xEvent		*wire;

{
    pexMaxHitsReachedEvent	*wireevent;
    PEXMaxHitsReachedEvent	*clientevent;


    /*
     * Set up the pointers.
     */

    wireevent = (pexMaxHitsReachedEvent *) wire;
    clientevent = (PEXMaxHitsReachedEvent *) client;


    /*
     * Now fill in the client structure.
     */

    clientevent->type = wireevent->type & 0x7f;
    clientevent->serial = wireevent->sequenceNumber;
    clientevent->send_event = (wireevent->type & 0x80) != 0;


    /*
     * MSB set if event came from SendEvent request.
     */

    clientevent->display = display;
    clientevent->renderer = wireevent->rdr;


    /*
     * Tell Xlib to add this to the event queue.
     */

    return (True);
}


void
PEXBeginPickOne (display, drawable, renderer, structure_id,
    method, pick_device_type, pick_record)

INPUT Display		*display;
INPUT Drawable		drawable;
INPUT PEXRenderer	renderer;
INPUT long		structure_id;
INPUT int		method;
INPUT int		pick_device_type;
INPUT PEXPickRecord	*pick_record;
{
    register pexBeginPickOneReq		*req;
    char				*pBuf;
    unsigned int 			rec_size;
    int					fpConvert;
    int					fpFormat;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    rec_size = GetPickRecordSize (pick_device_type);

    PEXGetReqExtra (BeginPickOne, (4 + rec_size), pBuf);

    BEGIN_REQUEST_HEADER (BeginPickOne, pBuf, req);
    CHECK_FP (fpConvert, fpFormat);

    PEXStoreFPReqExtraHead (BeginPickOne, fpFormat, (4 + rec_size), req);

    req->method = method;
    req->rdr = renderer;
    req->drawable = drawable;
    req->sid = structure_id;

    END_REQUEST_HEADER (BeginPickOne, pBuf, req);

    STORE_INT16 (pick_device_type, pBuf);
    pBuf += 2;					/* pad */

    STORE_PICK_RECORD (pick_device_type, rec_size,
	pick_record, pBuf, fpConvert, fpFormat);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


PEXPickPath *
PEXEndPickOne (display, renderer, status_return, undetectable_return)

INPUT Display			*display;
INPUT PEXRenderer		renderer;
OUTPUT int			*status_return;
OUTPUT int			*undetectable_return;

{
    register pexEndPickOneReq	*req;
    char			*pBuf;
    pexEndPickOneReply		rep;
    PEXPickPath			*pathRet;
    unsigned int		size;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    PEXGetReq (EndPickOne, pBuf);

    BEGIN_REQUEST_HEADER (EndPickOne, pBuf, req);

    PEXStoreReqHead (EndPickOne, req);
    req->rdr = renderer;

    END_REQUEST_HEADER (EndPickOne, pBuf, req);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
        UnlockDisplay (display);
        PEXSyncHandle (display);
        return (NULL);               /* return an error */
    }

    *status_return = rep.pickStatus;
    *undetectable_return = rep.betterPick;


    /*
     * Allocate a buffer for the path to pass back to the client.
     * If possible, use the pick path cache.
     */

    size = sizeof (PEXPickPath) + 
	rep.numPickElRefs * sizeof (PEXPickElementRef);

    if (!PEXPickCacheInUse && size <= PEXPickCacheSize)
    {
	pathRet = PEXPickCache;
	PEXPickCacheInUse = 1;
    }
    else
	pathRet = (PEXPickPath *) Xmalloc (size);

    pathRet->elements = (PEXPickElementRef *) (pathRet + 1);
    pathRet->count = rep.numPickElRefs;

    XREAD_LISTOF_PICKELEMREF (display, rep.numPickElRefs, pathRet->elements);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (pathRet);
}


PEXPickPath *
PEXPickOne (display, drawable, renderer, structure, method,
    pick_device_type, pick_record, status_return, undetectable_return)

INPUT Display			*display;
INPUT Drawable			drawable;
INPUT PEXRenderer		renderer;
INPUT PEXStructure      	structure;
INPUT int			method;
INPUT int			pick_device_type;
INPUT PEXPickRecord		*pick_record;
OUTPUT int			*status_return;
OUTPUT int			*undetectable_return;

{
    register pexPickOneReq	*req;
    char			*pBuf;
    pexPickOneReply		rep;
    unsigned int 		rec_size;
    unsigned int 		size;
    int				fpConvert;
    int				fpFormat;
    PEXPickPath			*pathRet;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    rec_size = GetPickRecordSize (pick_device_type);

    PEXGetReqExtra (PickOne, (4 + rec_size), pBuf);

    BEGIN_REQUEST_HEADER (PickOne, pBuf, req);
    CHECK_FP (fpConvert, fpFormat);

    PEXStoreFPReqExtraHead (PickOne, fpFormat, (4 + rec_size), req);

    req->method = method;
    req->rdr = renderer;
    req->drawable = drawable;
    req->sid = structure;

    END_REQUEST_HEADER (PickOne, pBuf, req);

    STORE_INT16 (pick_device_type, pBuf);
    pBuf += 2;					/* pad */

    STORE_PICK_RECORD (pick_device_type, rec_size,
	pick_record, pBuf, fpConvert, fpFormat);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
        UnlockDisplay (display);
        PEXSyncHandle (display);
        return (NULL);               /* return an error */
    }

    *status_return = rep.pickStatus;
    *undetectable_return = rep.betterPick;


    /*
     * Allocate a buffer for the path to pass back to the client.
     * If possible, use the pick path cache.
     */

    size = sizeof (PEXPickPath) + 
	rep.numPickElRefs * sizeof (PEXPickElementRef);

    if (!PEXPickCacheInUse && size <= PEXPickCacheSize)
    {
	pathRet = PEXPickCache;
	PEXPickCacheInUse = 1;
    }
    else
	pathRet = (PEXPickPath *) Xmalloc (size);

    pathRet->elements = (PEXPickElementRef *) (pathRet + 1);
    pathRet->count = rep.numPickElRefs;

    XREAD_LISTOF_PICKELEMREF (display, rep.numPickElRefs, pathRet->elements);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (pathRet);
}


void
PEXBeginPickAll (display, drawable, renderer, structure_id, method,
    send_event, max_hits, pick_device_type, pick_record)

INPUT Display		*display;
INPUT Drawable		drawable;
INPUT PEXRenderer	renderer;
INPUT long		structure_id;
INPUT int		method;
INPUT int		send_event;
INPUT int		max_hits;
INPUT int		pick_device_type;
INPUT PEXPickRecord	*pick_record;
{
    register pexBeginPickAllReq		*req;
    char				*pBuf;
    unsigned int			rec_size;
    int					fpConvert;
    int					fpFormat;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer.
     */

    rec_size = GetPickRecordSize (pick_device_type);

    PEXGetReqExtra (BeginPickAll, (4 + rec_size), pBuf);

    BEGIN_REQUEST_HEADER (BeginPickAll, pBuf, req);
    CHECK_FP (fpConvert, fpFormat);

    PEXStoreFPReqExtraHead (BeginPickAll, fpFormat, (4 + rec_size), req);

    req->method = method;
    req->rdr = renderer;
    req->drawable = drawable;
    req->sid = structure_id;
    req->sendEvent = send_event;
    req->pickMaxHits = max_hits;

    END_REQUEST_HEADER (BeginPickAll, pBuf, req);

    STORE_INT16 (pick_device_type, pBuf);
    pBuf += 2;					/* pad */

    STORE_PICK_RECORD (pick_device_type, rec_size,
	pick_record, pBuf, fpConvert, fpFormat);


    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);
}


PEXPickPath *
PEXEndPickAll (display, renderer, status_return, more_return, count_return)

INPUT Display		*display;
INPUT PEXRenderer	renderer;
OUTPUT int		*status_return;
OUTPUT int		*more_return;
OUTPUT unsigned long	*count_return;

{
    register pexEndPickAllReq	*req;
    char			*pBuf, *pBufSave;
    pexEndPickAllReply		rep;
    PEXPickPath			*pPath;
    PEXPickPath			*pPathRet;
    PEXPickElementRef		*pElemRef;
    int				numElements, i;
    unsigned int		total_size;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    PEXGetReq (EndPickAll, pBuf);

    BEGIN_REQUEST_HEADER (EndPickAll, pBuf, req);

    PEXStoreReqHead (EndPickAll, req);
    req->rdr = renderer;

    END_REQUEST_HEADER (EndPickAll, pBuf, req);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
        UnlockDisplay (display);
        PEXSyncHandle (display);
	*count_return = 0;
	return (NULL);           /* return an error */
    }

    *status_return = rep.pickStatus;
    *more_return = rep.morePicks;
    *count_return = rep.numPicked;


    /*
     * Read the reply data into a scratch buffer.
     */

    XREAD_INTO_SCRATCH (display, pBufSave, rep.length << 2);
    pBuf = pBufSave;

    /*
     * Allocate a buffer for the replies to pass back to the client.
     * If possible, use the pick path cache.
     */

    total_size = rep.numPicked * sizeof (PEXPickPath);

    for (i = 0; i < rep.numPicked; i++)
    {
	EXTRACT_CARD32 (pBuf, numElements);
	total_size += (numElements * sizeof (PEXPickElementRef));
	pBuf += (numElements * SIZEOF (pexPickElementRef));
    }

    if (!PEXPickCacheInUse && total_size <= PEXPickCacheSize)
    {
	pPathRet = PEXPickCache;
	PEXPickCacheInUse = 1;
    }
    else
	pPathRet = (PEXPickPath *) Xmalloc (total_size);

    pPath = pPathRet;
    pBuf = pBufSave;
    pElemRef = (PEXPickElementRef *) ((char *) pPath +
	rep.numPicked * sizeof (PEXPickPath));

    for (i = 0; i < rep.numPicked; i++)
    {
	EXTRACT_CARD32 (pBuf, numElements);
	EXTRACT_LISTOF_PICKELEMREF (numElements, pBuf, pElemRef);

	pPath->count = numElements;
	pPath->elements = pElemRef;
	pPath++;
	pElemRef += numElements;
    }

    FINISH_WITH_SCRATCH (display, pBufSave, rep.length << 2);

    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (pPathRet);
}


PEXPickPath *
PEXPickAll (display, drawable, renderer, method, max_hits, pick_device_type,
    pick_record, status_return, more_return, count_return)

INPUT Display		*display;
INPUT Drawable		drawable;
INPUT PEXRenderer	renderer;
INPUT int		method;
INPUT int		max_hits;
INPUT int		pick_device_type;
INPUT PEXPickRecord	*pick_record;
OUTPUT int		*status_return;
OUTPUT int		*more_return;
OUTPUT unsigned long	*count_return;


{
    register pexPickAllReq	*req;
    char			*pBuf, *pBufSave;
    pexPickAllReply		rep;
    PEXPickPath			*pPath;
    PEXPickPath			*pPathRet;
    PEXPickElementRef		*pElemRef;
    int				numElements, i;
    unsigned int		rec_size;
    unsigned int		total_size;
    int				fpConvert;
    int				fpFormat;


    /*
     * Lock around the critical section, for multithreading.
     */

    LockDisplay (display);


    /*
     * Put the request in the X request buffer and get a reply.
     */

    rec_size = GetPickRecordSize (pick_device_type);

    PEXGetReqExtra (PickAll, (4 + rec_size), pBuf);

    BEGIN_REQUEST_HEADER (PickAll, pBuf, req);
    CHECK_FP (fpConvert, fpFormat);

    PEXStoreFPReqExtraHead (PickAll, fpFormat, (4 + rec_size), req);

    req->method = method;
    req->rdr = renderer;
    req->drawable = drawable;
    req->pickMaxHits = max_hits;

    END_REQUEST_HEADER (PickAll, pBuf, req);

    STORE_INT16 (pick_device_type, pBuf);
    pBuf += 2;					/* pad */

    STORE_PICK_RECORD (pick_device_type, rec_size,
	pick_record, pBuf, fpConvert, fpFormat);

    if (_XReply (display, (xReply *)&rep, 0, xFalse) == 0)
    {
        UnlockDisplay (display);
        PEXSyncHandle (display);
	*count_return = 0;
	return (NULL);           /* return an error */
    }

    *status_return = rep.pickStatus;
    *more_return = rep.morePicks;
    *count_return = rep.numPicked;


    /*
     * Read the reply data into a scratch buffer.
     */

    XREAD_INTO_SCRATCH (display, pBufSave, rep.length << 2);
    pBuf = pBufSave;

    /*
     * Allocate a buffer for the replies to pass back to the client.
     * If possible, use the pick path cache.
     */

    total_size = rep.numPicked * sizeof (PEXPickPath);

    for (i = 0; i < rep.numPicked; i++)
    {
	EXTRACT_CARD32 (pBuf, numElements);
	total_size += (numElements * sizeof (PEXPickElementRef));
	pBuf += (numElements * SIZEOF (pexPickElementRef));
    }

    if (!PEXPickCacheInUse && total_size <= PEXPickCacheSize)
    {
	pPathRet = PEXPickCache;
	PEXPickCacheInUse = 1;
    }
    else
	pPathRet = (PEXPickPath *) Xmalloc (total_size);

    pPath = pPathRet;
    pBuf = pBufSave;
    pElemRef = (PEXPickElementRef *) ((char *) pPath +
	rep.numPicked * sizeof (PEXPickPath));

    for (i = 0; i < rep.numPicked; i++)
    {
	EXTRACT_CARD32 (pBuf, numElements);
	EXTRACT_LISTOF_PICKELEMREF (numElements, pBuf, pElemRef);

	pPath->count = numElements;
	pPath->elements = pElemRef;
	pPath++;
	pElemRef += numElements;
    }

    FINISH_WITH_SCRATCH (display, pBufSave, rep.length << 2);

    /*
     * Done, so unlock and check for synchronous-ness.
     */

    UnlockDisplay (display);
    PEXSyncHandle (display);

    return (pPathRet);
}
