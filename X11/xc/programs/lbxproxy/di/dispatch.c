/* $Xorg: dispatch.c,v 1.3 2000/08/17 19:53:55 cpqbld Exp $ */
/*
 * Copyright 1992 Network Computing Devices
 * Copyright 1996 X Consortium, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of NCD. not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  NCD. makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * NCD. DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL NCD.
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN 
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */
/* $XFree86: xc/programs/lbxproxy/di/dispatch.c,v 1.6 2001/10/28 03:34:22 tsi Exp $ */

#include <stdio.h>
#include "assert.h"
#include "lbx.h"
#include "wire.h"
#include "swap.h"
#include "lbxext.h"
#include "util.h"
#include "resource.h"
#include "pm.h"

extern int (* InitialVector[3]) ();

static void KillAllClients(
#if NeedFunctionPrototypes
    void
#endif
);

static void
HandleLargeRequest(
#if NeedFunctionPrototypes
    void
#endif
);

int nextFreeClientID; /* always MIN free client ID */
int nClients;	/* number active clients */
char *display_name = 0;
char dispatchException = 0;
char isItTimeToYield;
Bool lbxUseLbx = TRUE;
Bool lbxCompressImages = TRUE;
Bool lbxDoAtomShortCircuiting = TRUE;
Bool lbxDoLbxGfx = TRUE;

extern Bool lbxWinAttr;
extern Bool lbxDoCmapGrabbing;
extern char *atomsFile;

#define MAJOROP ((xReq *)client->requestBuffer)->reqType
#define MINOROP ((xReq *)client->requestBuffer)->data

int
Dispatch ()
{
    register int        *clientReady;     /* array of request ready clients */
    register int	result = 0;
    register ClientPtr	client;
    register int	nready;

    nextFreeClientID = 2;
    nClients = 0;

    clientReady = (int *) xalloc(sizeof(int) * MaxClients);
    if (!clientReady)
	FatalError("couldn't create client ready array");

    while (!dispatchException)
    {
	if (numLargeRequestsInQueue == 0) {
	    /*
	     * There are no pending large requests, so do blocking read.
	     */

	    nready = WaitForSomething(clientReady, FALSE /* block */);

	} else {
	    /*
	     * If there is no input from any clients (the only way we can
	     * check this is by polling rather than blocking), handle a
	     * large request.
	     */

	    nready = WaitForSomething(clientReady, TRUE  /* poll */);

	    if (!nready && numLargeRequestsInQueue)
		HandleLargeRequest ();
	}

       /***************** 
	*  Handle events in round robin fashion, doing input between 
	*  each round 
	*****************/

	while (!dispatchException && (--nready >= 0))
	{
	    client = clients[clientReady[nready]];
	    if (! client)
	    {
		/* KillClient can cause this to happen */
		continue;
	    }
	    isItTimeToYield = FALSE;
 
	    while (!isItTimeToYield)
	    {
		/* now, finally, deal with client requests */

	        result = ReadRequestFromClient(client);
	        if (result <= 0) 
	        {
		    if (result < 0)
			CloseDownClient(client);
		    break;
	        }

		client->sequence++;
#ifdef DEBUG
		if (client->requestLogIndex == MAX_REQUEST_LOG)
		    client->requestLogIndex = 0;
		client->requestLog[client->requestLogIndex] = MAJOROP;
		client->requestLogIndex++;
#endif
                client->sequenceNumber++;
		result = (* client->requestVector[MAJOROP])(client);
	    
		if (result != Success) 
		{
		    if (client->noClientException != Success)
                        CloseDownClient(client);
                    else
		        SendErrorToClient(client, MAJOROP, MINOROP,
					  client->errorValue, result);
		    break;
	        }
	    }
	    if (result >= 0 && client != client->server->serverClient)
		client->server->prev_exec = client;
	    FlushAllOutput();
	}
    }
    KillAllClients();
    LbxCleanupSession();
    xfree (clientReady);
    dispatchException &= ~DE_RESET;
    return (dispatchException & DE_TERMINATE);
}

void
SendErrorToClient(client, majorCode, minorCode, resId, errorCode)
    ClientPtr client;
    unsigned int majorCode;
    unsigned int minorCode;
    XID resId;
    int errorCode;
{
    xError      rep;
    int		n;

    rep.type = X_Error;
    rep.sequenceNumber = LBXSequenceNumber(client);
    rep.errorCode = errorCode;
    rep.majorCode = majorCode;
    rep.minorCode = minorCode;
    rep.resourceID = resId;

    if (client->swapped) {
	swaps(&rep.sequenceNumber, n);
	swaps(&rep.minorCode, n);
	swaps(&rep.resourceID, n);
    }

    if (LBXCacheSafe(client)) {
	FinishLBXRequest(client, REQ_YANK);
	WriteToClient(client, sizeof(rep), (char *)&rep);
    } else {
	if (!LBXCanDelayReply(client))
	    SendLbxSync(client);
	FinishLBXRequest(client, REQ_YANKLATE);
	SaveReplyData(client, (xReply *) &rep, 0, NULL);
    }
}

/************************
 * int NextAvailableClient(ospriv)
 *
 * OS dependent portion can't assign client id's because of CloseDownModes.
 * Returns NULL if there are no free clients.
 *************************/

ClientPtr
NextAvailableClient(ospriv, connect_fd)
    pointer ospriv;
    int connect_fd;
{
    register int i;
    register ClientPtr client;
    xReq data;
    static int been_there;

    if (!been_there) {
       nextFreeClientID = 1; /* The first client is serverClient */
       been_there++;
    }
    i = nextFreeClientID;
    if (i == MAXCLIENTS)
	return (ClientPtr)NULL;
    clients[i] = client = (ClientPtr)xcalloc(sizeof(ClientRec));
    if (!client)
	return (ClientPtr)NULL;
    client->index = i;
    client->closeDownMode = DestroyAll;
    client->awaitingSetup = TRUE;
    client->saveSet = (pointer *)NULL;
    client->noClientException = Success;
    client->public.requestLength = StandardRequestLength;
    client->requestVector = InitialVector;
    client->osPrivate = ospriv;
    client->big_requests = TRUE;

    /*
     * Use the fd the client connected on as a search key to find the 
     * associated display for this client
     */
    if (connect_fd != -1)
    {
	int j, k, found = 0;
	for (j=0; j < lbxMaxServers; j++)
	{
	    if (servers[j])
	    {
	        for (k=0; k < MAXTRANSPORTS; k++)
		   if (servers[j]->listen_fds[k] == connect_fd)
		   {
		       found = 1;
		       break;
		   }
	    }
	    if (found)
		break;
	}
	if (!found) {
	    fprintf (stderr, "Cannot determine a client's transport connection\n");
	    return (ClientPtr) NULL;
	}
        client->server = servers[j];
    }

    if (client->server && !InitClientResources(client))
    {
	xfree(client);
	return (ClientPtr)NULL;
    }

    if (i == currentMaxClients)
	currentMaxClients++;
    while ((nextFreeClientID < MAXCLIENTS) && clients[nextFreeClientID])
	nextFreeClientID++;
    if (client->server)
    {
	data.reqType = 1;
	data.length = (sz_xReq + sz_xConnClientPrefix) >> 2;
	if (!InsertFakeRequest(client, (char *)&data, sz_xReq))
	{
	    xfree (client);
	    return (ClientPtr) NULL;
	}
    }
    return(client);
}

int
ProcInitialConnection(client)
    register ClientPtr client;
{
    REQUEST(xReq);
    register xConnClientPrefix *prefix;
    int whichbyte = 1;

    prefix = (xConnClientPrefix *)((char *)stuff + sz_xReq);
    if ((prefix->byteOrder != 'l') && (prefix->byteOrder != 'B'))
	return (client->noClientException = -1);
    if (((*(char *) &whichbyte) && (prefix->byteOrder == 'B')) ||
	(!(*(char *) &whichbyte) && (prefix->byteOrder == 'l')))
    {
	client->swapped = TRUE;
	SwapConnClientPrefix(prefix);
    }
    stuff->reqType = 2;
    stuff->length += ((prefix->nbytesAuthProto + 3) >> 2) +
		     ((prefix->nbytesAuthString + 3) >> 2);
    if (client->swapped) {
	swaps(&stuff->length, whichbyte);
    }
    ResetCurrentRequest(client);
    return (client->noClientException);
}

int
ProcEstablishConnection(client)
    register ClientPtr client;
{
    register xConnClientPrefix *prefix;
    register int i;
    int         len;

    REQUEST(xReq);

    prefix = (xConnClientPrefix *) ((char *) stuff + sz_xReq);

    nClients++;
    client->requestVector = client->server->requestVector;
    client->sequence = 0;
    client->sequenceNumber = 0;
    client->largeRequest = NULL;

    /* wait for X server to kill client */
    client->closeDownMode = RetainPermanent;

    /*
     * NewClient outputs the LbxNewClient request header - have to follow it
     * up with the setup connection info.
     */
    /* length is still swapped */
    if (client->swapped) {
	swaps(&stuff->length, i);
	/* put data back to the way server will expect it */
	SwapConnClientPrefix((xConnClientPrefix *) prefix);
    }
    len = (stuff->length << 2) - sz_xReq;
    if (!NewClient(client, len))
	return (client->noClientException = -1);

    WriteToServer(client->server->serverClient, len, (char *) prefix, 
		  TRUE, FALSE);

    /*
     * Can't allow any requests to be passed on to the server until the
     * connection setup reply has been received.
     */
    IgnoreClient(client);

    return (client->noClientException);
}

/**********************
 * CloseDownClient
 *
 *  Client can either mark his resources destroy or retain.  If retained and
 *  then killed again, the client is really destroyed.
 *********************/

Bool resetAfterLastClient = FALSE;
Bool terminateAfterLastClient = FALSE;

void
CloseDownClient(client)
    register ClientPtr client;
{
    if (!client->clientGone)
    {
	CloseClient (client);
	
	
	/* X server is telling us this client is dead */
	if (client->closeDownMode == DestroyAll)
	{
	    client->clientGone = TRUE;  /* so events aren't sent to client */
	    FreeClientResources(client);
	    CloseDownConnection(client);
	    if (ClientIsAsleep (client))
		ClientSignal (client);
	    if (client->index < nextFreeClientID)
		nextFreeClientID = client->index;
	    clients[client->index] = NullClient;
	    if ((client->requestVector != InitialVector) &&
		(client->server && client->server->serverClient != client) &&
		(--nClients == 0))
	    {
		if (resetAfterLastClient)
		    dispatchException |= DE_RESET;
		else if (terminateAfterLastClient)
		    dispatchException |= DE_TERMINATE;
	    }
	    xfree(client);
	}
	else
	{
	    client->clientGone = TRUE;
	    CloseDownConnection(client);
	    --nClients;
	}
    }
    else
    {
	/* really kill resources this time */
        FreeClientResources(client);
	if (ClientIsAsleep (client))
	    ClientSignal (client);
	if (client->index < nextFreeClientID)
	    nextFreeClientID = client->index;
	clients[client->index] = NullClient;
        xfree(client);
	if (nClients == 0)
	{
	    if (resetAfterLastClient)
		dispatchException |= DE_RESET;
	    else if (terminateAfterLastClient)
		dispatchException |= DE_TERMINATE;
	}
    }

    while (!clients[currentMaxClients-1])
      currentMaxClients--;
}

static void
KillAllClients()
{
    int i;
    for (i=1; i<currentMaxClients; i++)
    {
        if (clients[i])
	{
	    clients[i]->closeDownMode = DestroyAll;   
            CloseDownClient(clients[i]);
	}
    }
}

extern void (*ZeroPadReqVector[128]) ();

int
ProcStandardRequest (client)
    ClientPtr	client;
{
    REQUEST(xReq);
    void (*zeroPadProc)();
    extern int lbxZeroPad;

    if (lbxZeroPad &&
	(MAJOROP < 128) && (zeroPadProc = ZeroPadReqVector[MAJOROP]))
	(*zeroPadProc) ((void *) stuff);
    FinishLBXRequest(client, REQ_PASSTHROUGH);
    WriteReqToServer(client, client->req_len << 2, (char *) stuff, TRUE);
    return Success;
}

/* ARGSUSED */
int
ProcBadRequest (client)
    ClientPtr	client;
{
    return BadRequest;
}

/*
 * Turn off optional features.  Some features, like tags, will be turned
 * off after option negotiation.
 */

void
AdjustProcVector()
{
    int         i;

    /*
     * to turn off all LBX request reencodings, set all proc vectors to
     * ProcStandardRequest
     */
    if (!lbxUseLbx) {
    	for (i = 1; i < 256; i++) {
            ProcVector[i] = ProcStandardRequest;
        }
    }

    if (!atomsFile)
	ProcVector[X_ChangeWindowAttributes] = ProcStandardRequest;

    if (!lbxCompressImages) {
	ProcVector[X_PutImage] = ProcStandardRequest;
	ProcVector[X_GetImage] = ProcStandardRequest;
    }

    if (!lbxDoAtomShortCircuiting) {
	ProcVector[X_InternAtom] = ProcStandardRequest;
	ProcVector[X_GetAtomName] = ProcStandardRequest;
    }

    if (!lbxDoCmapGrabbing)
    {
	ProcVector[X_CreateColormap] = ProcStandardRequest;
	ProcVector[X_FreeColormap] = ProcStandardRequest;
	ProcVector[X_CopyColormapAndFree] = ProcStandardRequest;
	ProcVector[X_AllocColor] = ProcStandardRequest;
	ProcVector[X_AllocNamedColor] = ProcStandardRequest;
	ProcVector[X_AllocColorCells] = ProcStandardRequest;
	ProcVector[X_AllocColorPlanes] = ProcStandardRequest;
	ProcVector[X_FreeColors] = ProcStandardRequest;
	ProcVector[X_LookupColor] = ProcStandardRequest;
    }

    if (!lbxDoLbxGfx) {
	ProcVector[X_CopyArea] = ProcStandardRequest;
	ProcVector[X_CopyPlane] = ProcStandardRequest;
	ProcVector[X_PolyPoint] = ProcStandardRequest;
	ProcVector[X_PolyLine] = ProcStandardRequest;
	ProcVector[X_PolySegment] = ProcStandardRequest;
	ProcVector[X_PolyRectangle] = ProcStandardRequest;
	ProcVector[X_PolyArc] = ProcStandardRequest;
	ProcVector[X_FillPoly] = ProcStandardRequest;
	ProcVector[X_PolyFillRectangle] = ProcStandardRequest;
	ProcVector[X_PolyFillArc] = ProcStandardRequest;
	ProcVector[X_PolyText8] = ProcStandardRequest;
	ProcVector[X_PolyText16] = ProcStandardRequest;
	ProcVector[X_ImageText8] = ProcStandardRequest;
	ProcVector[X_ImageText16] = ProcStandardRequest;
    }

    if (!lbxWinAttr)
    {
	ProcVector[X_GetWindowAttributes] = ProcStandardRequest;
	ProcVector[X_GetGeometry] = ProcStandardRequest;
    }
}


static void
HandleLargeRequest ()

{
    LbxLargeRequestRec *largeRequest = largeRequestQueue[0];
    ClientPtr client = largeRequest->client;
    int bytesLeft, chunkSize;

    /*
     * Process the first large request on the queue.  If this is the first
     * chunk of a large request, send an LbxBeginLargeRequest message.
     */

    if (largeRequest->bytesWritten == 0) {
	xLbxBeginLargeRequestReq beginReq;
	int n;

	beginReq.reqType = client->server->lbxReq;
	beginReq.lbxReqType = X_LbxBeginLargeRequest;
	beginReq.length = 2;
	beginReq.largeReqLength = largeRequest->totalBytes >> 2;
	if (client->swapped) {
	    swapl(&beginReq.largeReqLength, n);
	}
	_write_to_server (client,
	    largeRequest->compressed,
	    sizeof (beginReq), (char *) &beginReq,
	    FALSE, TRUE);
    }

    /*
     * Send a chunk of the large request using the LbxLargeRequestData message.
     */

    bytesLeft = largeRequest->totalBytes - largeRequest->bytesWritten;

    if (bytesLeft > LBX_LARGE_REQUEST_CHUNK_SIZE)
	chunkSize = LBX_LARGE_REQUEST_CHUNK_SIZE;
    else
	chunkSize = bytesLeft;

    if (chunkSize > 0) {
	xLbxLargeRequestDataReq dataReq;

	dataReq.reqType = client->server->lbxReq;
	dataReq.lbxReqType = X_LbxLargeRequestData;
	dataReq.length = 1 + (chunkSize >> 2);

	_write_to_server (client,
	    largeRequest->compressed,
	    sizeof (dataReq), (char *) &dataReq,
	    FALSE, TRUE);

	_write_to_server (client,
	    largeRequest->compressed,
	    chunkSize,
	    largeRequest->buf + largeRequest->bytesWritten,
	    FALSE, FALSE);

	largeRequest->bytesWritten += chunkSize;
    }

    if (numLargeRequestsInQueue > 1) {
	/*
	 * Move this large request to the end of the queue - this way
	 * we can process large requests from other clients too.
	 */

	memmove((char *)&largeRequestQueue[0], (char *)&largeRequestQueue[1],
		(numLargeRequestsInQueue - 1) * sizeof(LbxLargeRequestRec *));
	largeRequestQueue[numLargeRequestsInQueue - 1] = largeRequest;
    }

    /*
     * See if the whole request has been sent.  If yes, send an
     * LbxEndLargeRequest and re-enable input for this client.
     */

    if (largeRequest->bytesWritten == largeRequest->totalBytes) {
	xLbxEndLargeRequestReq endReq;

	endReq.reqType = client->server->lbxReq;
	endReq.lbxReqType = X_LbxEndLargeRequest;
	endReq.length = 1;

	_write_to_server (client,
	    largeRequest->compressed,
	    sizeof (endReq), (char *) &endReq,
	    FALSE, TRUE);

	xfree ((char *) largeRequest);
	client->largeRequest = NULL;
	numLargeRequestsInQueue--;
		    
	AttendClient (client);
    }
}
