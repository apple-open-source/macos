/* $Xorg: wire.c,v 1.4 2001/02/09 02:05:32 xorgcvs Exp $ */
/*

Copyright 1998  The Open Group

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

*/
/*
 * Copyright 1992 Network Computing Devices
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
/* $XFree86: xc/programs/lbxproxy/di/wire.c,v 1.14 2002/09/19 13:22:03 tsi Exp $ */

#include "lbx.h"
#include <stdio.h>
#include <stdlib.h>
#include "wire.h"
#include "tags.h"
#include "colormap.h"
#include "init.h"
#ifndef Lynx
#include <sys/uio.h>
#else
#include <uio.h>
#endif
#include <errno.h>
#include "proxyopts.h"
#include "swap.h"
#include "assert.h"
#include "os.h"
#include "resource.h"
#include "colormap.h"
#include "lbxext.h"
#include "atomcache.h"
#include "util.h"
#include "pm.h"
#include "misc.h"

#include <X11/ICE/ICElib.h>
#ifdef BIGREQS
#include "bigreqstr.h"
#endif

/*
 * The following include for utsname.h is from lib/xtrans
 */
#if (defined(_POSIX_SOURCE) && !defined(AIXV3) && !defined(__QNX__)) || defined(hpux) || defined(USG) || defined(SVR4)
#define NEED_UTSNAME
#include <sys/utsname.h>	/* uname() */
#else
#include <unistd.h>		/* gethostname() */
#endif

#ifdef LBX_STATS
extern int  delta_out_total;
extern int  delta_out_attempts;
extern int  delta_out_hits;
extern int  delta_in_total;
extern int  delta_in_attempts;
extern int  delta_in_hits;
#endif

/*
 * Local constants
 */
#define MAXBYTESDIFF		8
#define PM_Unable  0
#define PM_Success 1
#define PM_Failure 2

/*
 * Global vars
 */
int lbxDebug = 0;

/*
 * Local functions
 */
static void LbxOnlyListenToOneClient();
static void LbxListenToAllClients();

/*
 * Any request that could be delta compressed comes through here
 */
void
WriteReqToServer(client, len, buf, checkLargeRequest)
    ClientPtr   client;
    int         len;
    char       *buf;
    Bool	checkLargeRequest;
{
    XServerPtr  server = client->server;
    xLbxDeltaReq *p = (xLbxDeltaReq *) server->tempdeltabuf;
    int         diffs;
    int         cindex;
    int         newlen;
    Bool        written = FALSE;

#ifdef LBX_STATS
    delta_out_total++;
#endif

    if (DELTA_CACHEABLE(&server->outdeltas, len)) {

#ifdef LBX_STATS
	delta_out_attempts++;
#endif

	if ((diffs = LBXDeltaMinDiffs(&server->outdeltas, (unsigned char *)buf,
			len, min(MAXBYTESDIFF, (len - sz_xLbxDeltaReq) >> 1),
				      &cindex)) >= 0) {

#ifdef LBX_STATS
	    delta_out_hits++;
#endif

	    LBXEncodeDelta(&server->outdeltas, (unsigned char *)buf, diffs,
			   cindex, &server->tempdeltabuf[sz_xLbxDeltaReq]);
	    p->reqType = server->lbxReq;
	    p->lbxReqType = X_LbxDelta;
	    p->diffs = diffs;
	    p->cindex = cindex;
	    newlen = sz_xLbxDeltaReq + sz_xLbxDiffItem * diffs;
	    p->length = (newlen + 3) >> 2;
	    /* Don't byte swap -- lengths are always in proxy order */
	    WriteToServer(client, newlen, (char *) p, TRUE, checkLargeRequest);
	    written = TRUE;
	}
	LBXAddDeltaOut(&server->outdeltas, (unsigned char *)buf, len);
    }
    if (!written) {
#ifdef BIGREQS
	if (len > (0xffff << 2)) {
	    xBigReq bigreq;
	    bigreq.reqType = ((xReq *)buf)->reqType;
	    bigreq.data = ((xReq *)buf)->data;
	    bigreq.zero = 0;
	    bigreq.length = (len + sizeof(xBigReq) - sizeof(xReq)) >> 2;
	    WriteToServer(client, sizeof(xBigReq), (char *)&bigreq, 
			  TRUE, checkLargeRequest);
	    WriteToServer(client, len - sizeof(xReq), buf + sizeof(xReq),
			  FALSE, checkLargeRequest);
	    return;
	}
#endif
	WriteToServer(client, len, buf, TRUE, checkLargeRequest);
    }
}

void
_write_to_server(client, compressed, len, buf, checkLarge, startOfRequest)
    ClientPtr   client;
    Bool        compressed;
    int         len;
    char       *buf;
    Bool	checkLarge;
    Bool	startOfRequest;
{
    XServerPtr  server = client->server;
    unsigned reqSize;

    if (server->serverClient->clientGone)
	return;

    if (checkLarge && client != clients[0] && nClients > 1 &&
	((client != server->prev_exec) || numLargeRequestsInQueue)) {
	/*
	 * Check if this is a large request only if there is more than
	 * one client and a different client was the last to execute or
	 * there are already large requests queued.
	 *
	 * If it is a large request, and there is room in the large request
	 * queue, add it to the queue.  lbxproxy will send the large request
	 * in chunks, preventing this client from hogging the wire.
	 *
	 * By checking that the previous client was someone else,
	 * we hope that we can prevent splitting a large request
	 * when the other clients appear to be idle (based on their past
	 * history).
	 */

	if (startOfRequest &&
	    (!(reqSize = (((xReq *) buf)->length) << 2) ||
	     reqSize >= LBX_LARGE_REQUEST_MIN_SIZE) &&
	    numLargeRequestsInQueue < LARGE_REQUEST_QUEUE_LEN) {
	    LbxLargeRequestRec *largeRequest;

	    if (!reqSize)
		reqSize = ((xBigReq *)buf)->length << 2;
	    largeRequest = (LbxLargeRequestRec *)
		xalloc (sizeof (LbxLargeRequestRec) + reqSize);

	    /*
	     * Add this large request to the queue
	     */

	    largeRequest->client = client;
	    largeRequest->compressed = compressed;
	    largeRequest->buf = (char *) largeRequest +
		sizeof (LbxLargeRequestRec);
	    memcpy (largeRequest->buf, buf, len);
	    largeRequest->totalBytes = reqSize;
	    largeRequest->bytesRead = len;
	    largeRequest->bytesWritten = 0;
	    client->largeRequest = largeRequest;

	    largeRequestQueue[numLargeRequestsInQueue++] = largeRequest;

	    /*
	     * Once we have the whole large request, we want to disable
	     * input from this client - we don't want to read new requests
	     * until we are done sending the whole large request.
	     */

	    if (reqSize == len)
		IgnoreClient(client);
	    
	    return;
	} else if (client->largeRequest) {

	    /*
	     * Append to the large request
	     */

	    char *dst = client->largeRequest->buf +
	        client->largeRequest->bytesRead;
	    memcpy (dst, buf, len);
	    client->largeRequest->bytesRead += len;

	    /*
	     * Once we have the whole large request, we want to disable
	     * input from this client - we don't want to read new requests
	     * until we are done sending the whole large request.
	     */

	    if (client->largeRequest->bytesRead ==
		client->largeRequest->totalBytes)
		IgnoreClient(client);

	    return;
	}
    }

    if (server->send != client) {
	xLbxSwitchReq s;

	DBG(DBG_SWITCH, (stderr, "switch downstream to %d\n", client->index));
	s.reqType = server->lbxReq;
	s.lbxReqType = X_LbxSwitch;
	s.length = 2;
	s.client = client->index;
	WriteToClient(server->serverClient, sizeof(s), &s);
	server->send = client;
    }
    DBG(DBG_IO, (stderr, "downstream %d len %d\n", client->index, len));
    if (compressed || !server->compHandle)
	WriteToClient(server->serverClient, len, buf);
    else
	UncompressWriteToClient(server->serverClient, len, buf);
}

void
WriteToServer(client, len, buf, startOfRequest, checkLargeRequest)
    ClientPtr   client;
    int         len;
    char       *buf;
    Bool	startOfRequest;
    Bool	checkLargeRequest;
{
    _write_to_server(client, TRUE, len, buf, checkLargeRequest, startOfRequest);
}

void
WriteToServerUncompressed(client, len, buf, startOfRequest)
    ClientPtr   client;
    int         len;
    char       *buf;
    Bool	startOfRequest;
{
    _write_to_server(client, FALSE, len, buf, TRUE, startOfRequest);
}

/* all these requests may need to be swapped back to the order of
 * the client they're being executed for
 */
Bool
NewClient(client, setuplen)
    ClientPtr   client;
    int         setuplen;
{
    xLbxNewClientReq n;
    XServerPtr  server = client->server;

    DBG(DBG_CLIENT, (stderr, "new client %d\n", client->index));
    n.reqType = server->lbxReq;
    n.lbxReqType = X_LbxNewClient;
    n.length = 2 + (setuplen >> 2);
    n.client = client->index;
    WriteToServer(server->serverClient, sizeof(n), (char *) &n, TRUE, FALSE);
    ++server->serverClient->sequence;
    return TRUE;
}

void
CloseClient(client)
    ClientPtr   client;
{
    xLbxCloseClientReq n;
    XServerPtr  server = client->server;

    if (!client->server)
	return;

    if (client->server->serverClient == client)
	return;
    if (client->server->serverClient->clientGone)
	return;
    DBG(DBG_CLIENT, (stderr, "closing down client %d\n", client->index));
    if (client->closeDownMode != DestroyAll) {
	n.reqType = server->lbxReq;
	n.lbxReqType = X_LbxCloseClient;
	n.length = 2;
	n.client = client->index;
	if (client->swapped) {
	    SwapCloseClient(&n);
	}
	WriteReqToServer(client, sizeof(n), (char *) &n, TRUE);
    }
}

void
ModifySequence(client, num)
    ClientPtr   client;
    int         num;
{
    xLbxModifySequenceReq req;
    XServerPtr  server = client->server;

    if (client->server->serverClient == client)
	return;
    req.reqType = server->lbxReq;
    req.lbxReqType = X_LbxModifySequence;
    req.length = 2;
    req.adjust = num;
    if (client->swapped) {
	SwapModifySequence(&req);
    }
    WriteReqToServer(client, sizeof(req), (char *) &req, TRUE);
}

void
AllowMotion(client, num)
    ClientPtr   client;
    int         num;
{
    client->server->motion_allowed += num;
}

void
SendIncrementPixel(client, cmap, pixel)
    ClientPtr   client;
    XID         cmap;
    unsigned long pixel;
{
    xLbxIncrementPixelReq req;
    XServerPtr  server = client->server;

    if (client->server->serverClient == client)
	return;
    req.reqType = server->lbxReq;
    req.lbxReqType = X_LbxIncrementPixel;
    req.length = 3;
    req.cmap = cmap;
    req.pixel = pixel;
    if (client->swapped) {
	SwapIncrementPixel(&req);
    }
    WriteReqToServer(client, sizeof(req), (char *) &req, TRUE);
}

void
#ifdef NeedFunctionPrototypes
SendAllocColor(
    ClientPtr	client,
    XID		cmap,
    CARD32	pixel,
    CARD16	red,
    CARD16	green,
    CARD16	blue)
#else
SendAllocColor(client, cmap, pixel, red, green, blue)
    ClientPtr   client;
    XID         cmap;
    CARD32      pixel;
    CARD16	red, green, blue;
#endif
{
    xLbxAllocColorReq req;

    req.reqType = client->server->lbxReq;
    req.lbxReqType = X_LbxAllocColor;
    req.length = sz_xLbxAllocColorReq >> 2;
    req.cmap = cmap;
    req.pixel = pixel;
    req.red = red;
    req.green = green;
    req.blue = blue;
    req.pad = 0;

    if (client->swapped)
	SwapAllocColor (&req);

    WriteReqToServer (client, sizeof(req), (char *) &req, TRUE);
}

void
SendGetModifierMapping(client)
    ClientPtr   client;
{
    xLbxGetModifierMappingReq req;
    XServerPtr  server = client->server;

    if (client->server->serverClient == client)
	return;

    req.reqType = server->lbxReq;
    req.lbxReqType = X_LbxGetModifierMapping;
    req.length = 1;
    if (client->swapped) {
	SwapGetModifierMapping(&req);
    }
    WriteReqToServer(client, sizeof(req), (char *) &req, TRUE);
}

void
SendGetKeyboardMapping(client)
    ClientPtr   client;
{
    xLbxGetKeyboardMappingReq req;
    XServerPtr  server = client->server;

    if (client->server->serverClient == client)
	return;

    /*
     * always ask for entire thing so tag always works, and pass on requested
     * subset
     */
    req.reqType = server->lbxReq;
    req.lbxReqType = X_LbxGetKeyboardMapping;
    req.length = 2;
    req.firstKeyCode = LBXMinKeyCode(client);
    req.count = LBXMaxKeyCode(client) - LBXMinKeyCode(client) + 1;
    req.pad1 = 0;
    if (client->swapped) {
	SwapGetKeyboardMapping(&req);
    }
    WriteReqToServer(client, sizeof(req), (char *) &req, TRUE);
}

void
SendQueryFont(client, fid)
    ClientPtr   client;
    XID         fid;
{
    xLbxQueryFontReq req;
    XServerPtr  server = client->server;

    if (client->server->serverClient == client)
	return;

    req.reqType = server->lbxReq;
    req.lbxReqType = X_LbxQueryFont;
    req.length = 2;
    req.fid = fid;
    if (client->swapped) {
	SwapQueryFont(&req);
    }
    WriteReqToServer(client, sizeof(req), (char *) &req, TRUE);
}

void
SendChangeProperty(client, win, prop, type, format, mode, num)
    ClientPtr   client;
    Window      win;
    Atom        prop,
                type;
    int         format,
                mode;
    unsigned long num;
{
    xLbxChangePropertyReq req;
    XServerPtr  server = client->server;

    if (client->server->serverClient == client)
	return;

    req.reqType = server->lbxReq;
    req.lbxReqType = X_LbxChangeProperty;
    req.length = 6;
    req.window = win;
    req.property = prop;
    req.type = type;
    req.format = format;
    req.mode = mode;
    req.nUnits = num;
    req.pad[0] = req.pad[1] = 0;
    if (client->swapped) {
	SwapChangeProperty(&req);
    }
    WriteReqToServer(client, sizeof(req), (char *) &req, TRUE);
}

void
SendGetProperty(client, win, prop, type, delete, off, len)
    ClientPtr   client;
    Window      win;
    Atom        prop,
                type;
    Bool        delete;
    unsigned long off,
                len;
{
    xLbxGetPropertyReq req;
    XServerPtr  server = client->server;

    if (client->server->serverClient == client)
	return;

    req.reqType = server->lbxReq;
    req.lbxReqType = X_LbxGetProperty;
    req.length = 7;
    req.window = win;
    req.property = prop;
    req.type = type;
    req.delete = delete;
    req.longOffset = off;
    req.longLength = len;
    req.pad[0] = req.pad[1] = req.pad[2] = 0;
    if (client->swapped) {
	SwapGetProperty(&req);
    }
    WriteReqToServer(client, sizeof(req), (char *) &req, TRUE);
}

void
SendInvalidateTag(client, tag)
    ClientPtr   client;
    XID         tag;
{
    xLbxInvalidateTagReq req;
    XServerPtr  server;

    if (!servers[0])		/* proxy resetting */
	return;

    server = client->server;

    req.reqType = server->lbxReq;
    req.lbxReqType = X_LbxInvalidateTag;
    req.length = 2;
    req.tag = tag;
    /* need tag type ? */
    if (client->swapped) {
	SwapInvalidateTag(&req);
    }
    WriteReqToServer(client, sizeof(req), (char *) &req, TRUE);
}

void
SendTagData(client, tag, len, data)
    ClientPtr   client;
    XID         tag;
    unsigned long len;
    pointer     data;
{
    xLbxTagDataReq req,
               *reqp;
    int         req_len;
    XServerPtr  server;

    server = client->server;

    req_len = 3 + ((len + 3) >> 2);
    if (DELTA_CACHEABLE(&server->outdeltas, req_len << 2)) {
	reqp = (xLbxTagDataReq *) xalloc(req_len << 2);
	memcpy((pointer) (reqp + 1), data, len);
    } else {
	reqp = &req;
    }
    reqp->reqType = server->lbxReq;
    reqp->lbxReqType = X_LbxTagData;
    reqp->length = req_len;
    reqp->real_length = len;
    reqp->tag = tag;
    /* need tag type ? */
    if (reqp == &req) {
	WriteToServer(server->serverClient, 
		      sizeof(req), (char *) &req, TRUE, FALSE);
	if (len)
	    WriteToServer(server->serverClient, 
			  len, (char *) data, FALSE, FALSE);
    } else {
	WriteReqToServer(server->serverClient, 
			 req_len << 2, (char *) reqp, FALSE);
	xfree(reqp);
    }
}

void
SendGetImage(client, drawable, x, y, width, height, planeMask, format)
    ClientPtr   client;
    Drawable    drawable;
    int         x;
    int         y;
    unsigned int width;
    unsigned int height;
    unsigned long planeMask;
    int         format;
{
    xLbxGetImageReq req;
    XServerPtr  server = client->server;

    if (client->server->serverClient == client)
	return;

    req.reqType = server->lbxReq;
    req.lbxReqType = X_LbxGetImage;
    req.length = 6;
    req.drawable = drawable;
    req.x = x;
    req.y = y;
    req.width = width;
    req.height = height;
    req.planeMask = planeMask;
    req.format = format;
    req.pad1 = 0;
    req.pad2 = 0;

    if (client->swapped) {
	SwapGetImage(&req);
    }
    WriteReqToServer(client, sizeof(req), (char *) &req, TRUE);
}

static Bool
SendInternAtoms (server)
    XServerPtr server;
{
    xLbxInternAtomsReq *req;
    int reqSize, i, num;
    char lenbuf[2];
    char *ptr;

    reqSize = sz_xLbxInternAtomsReq;
    num = 0;
    for (i = 0; i < server->atom_control_count; i++) {
	if (server->atom_control[i].flags & AtomPreInternFlag) {
	    reqSize += (2 + server->atom_control[i].len);
	    num++;
	}
    }
    if (!num)
	return FALSE;

    if (!(req = (xLbxInternAtomsReq *) xalloc (reqSize)))
	return FALSE;

    req->reqType = server->lbxReq;
    req->lbxReqType = X_LbxInternAtoms;
    req->length = (reqSize + 3) >> 2;
    req->num = num;

    ptr = (char *) req + sz_xLbxInternAtomsReq;

    for (i = 0; i < server->atom_control_count; i++)
    {
	if (server->atom_control[i].flags & AtomPreInternFlag) {
	    *((CARD16 *) lenbuf) = server->atom_control[i].len;
	    ptr[0] = lenbuf[0];
	    ptr[1] = lenbuf[1];
	    ptr += 2;
	    memcpy (ptr, 
		    server->atom_control[i].name, 
		    server->atom_control[i].len);
	    ptr += server->atom_control[i].len;
	}
    }

    WriteToClient(server->serverClient, reqSize, (char *) req);

    xfree (req);
    return TRUE;
}

/*ARGSUSED*/
static void
InternAtomsReply (server, rep)
    XServerPtr  server;
    xLbxInternAtomsReply *rep;
{
    Atom *atoms = (Atom *) ((char *) rep + sz_xLbxInternAtomsReplyHdr);
    int i;

    for (i = 0; i < server->atom_control_count; i++) {
	if (server->atom_control[i].flags & AtomPreInternFlag)
	    (void) LbxMakeAtom (server,
				server->atom_control[i].name, 
				server->atom_control[i].len,
				*atoms++, TRUE);
    }

    SendInitLBXPackets(server);

    /*
     * Now the proxy is ready to accept connections from clients.
     */

    (void) ListenWellKnownSockets ();
}


static unsigned long pendingServerReplySequence;
static void (*serverReplyFunc) ();

static void
ServerReply(server, rep)
    XServerPtr  server;
    xReply     *rep;
{
    if (serverReplyFunc &&
	    rep->generic.sequenceNumber == pendingServerReplySequence) {
	/*
	 * We got the reply we were waiting from the server
	 */

	(*serverReplyFunc) (server, rep);

	/*
	 * ExpectServerReply() might have been called within the server
	 * reply func just processed.
	 */

	if (rep->generic.sequenceNumber == pendingServerReplySequence)
	    serverReplyFunc = 0;
    }
}

static void
ExpectServerReply(server, func)
    XServerPtr  server;
    void        (*func) ();
{
    pendingServerReplySequence = server->serverClient->sequence;
    serverReplyFunc = func;
}

extern int  (*ServerVector[]) ();

static unsigned long
ServerRequestLength(req, sc, gotnow, partp)
    xReq       *req;
    ClientPtr   sc;
    int         gotnow;
    Bool       *partp;
{
    XServerPtr  server = servers[sc->lbxIndex];
    ClientPtr   client = server->recv;
    xReply     *rep;
    xConnSetupPrefix *pre;

    if (!req)
	req = (xReq *) sc->requestBuffer;
    if (gotnow < sizeof(xReq)) {
	*partp = TRUE;
	return sizeof(xReq);
    }
    if (req->reqType == server->lbxEvent && req->data == LbxDeltaEvent) {
	*partp = FALSE;
	return req->length << 2;
    }
    if (req->reqType == server->lbxEvent && req->data == LbxSwitchEvent) {
	*partp = FALSE;
	return sz_xLbxSwitchEvent;
    }
    if (req->reqType == server->lbxEvent + LbxQuickMotionDeltaEvent) {
	*partp = FALSE;
	return sz_lbxQuickMotionDeltaEvent;
    }
    if (req->reqType == server->lbxEvent && req->data == LbxMotionDeltaEvent) {
	*partp = FALSE;
	return sz_lbxMotionDeltaEvent;
    }
    if (client->awaitingSetup) {
	if (gotnow < 8) {
	    *partp = TRUE;
	    return 8;
	}
	pre = (xConnSetupPrefix *) req;
	*partp = FALSE;
	return 8 + (pre->length << 2);
    }
    if (gotnow < 8) {
	*partp = TRUE;
	return 8;
    }
    *partp = FALSE;
    rep = (xReply *) req;
    if (rep->generic.type != X_Reply) {
	return EventLength((xEvent *)rep, server->lbxNegOpt.squish);
    }
    return sz_xReply + (rep->generic.length << 2);
}

int
ServerProcStandardEvent(sc)
    ClientPtr   sc;
{
    xReply     *rep;
    XServerPtr  server = servers[sc->lbxIndex];
    ClientPtr   client = server->recv;
    int         len;
    Bool        part;
    Bool        cacheable = (server->initialized) ? TRUE : FALSE;

    rep = (xReply *) sc->requestBuffer;

    /* need to calculate length up from for Delta cache */
    len = RequestLength(rep, sc, 8, &part);

#ifdef LBX_STATS
    delta_in_total++;
#endif

    if (rep->generic.type == server->lbxEvent &&
	    rep->generic.data1 == LbxDeltaEvent) {
	xLbxDeltaReq *delta = (xLbxDeltaReq *) rep;

#ifdef LBX_STATS
	delta_in_attempts++;
	delta_in_hits++;
#endif

	/* Note that LBXDecodeDelta decodes and adds current msg to the cache */
	len = LBXDecodeDelta(&server->indeltas,
			     (xLbxDiffItem *)((char *) rep + sz_xLbxDeltaReq),
			     delta->diffs, delta->cindex,
			     (unsigned char **)&rep);

	/* Make local copy in case someone writes to the request buffer */
	memcpy(server->tempdeltabuf, (char *) rep, len);
	rep = (xReply *) server->tempdeltabuf;

	cacheable = FALSE;
    }

    /* stick in delta buffer before LBX code modified things */
    if (cacheable && DELTA_CACHEABLE(&server->indeltas, len)) {

#ifdef LBX_STATS
	delta_in_attempts++;
#endif

	LBXAddDeltaIn(&server->indeltas, (unsigned char *) rep, len);
    }
    if (rep->generic.type == server->lbxEvent &&
	rep->generic.data1 != LbxMotionDeltaEvent) {
	switch (rep->generic.data1) {
	case LbxSwitchEvent:
	    DBG(DBG_SWITCH, (stderr, "switch upstream to %d\n",
			     ((xLbxSwitchEvent *)rep)->client));
	    client = clients[((xLbxSwitchEvent *)rep)->client];
	    server->recv = client;
	    (void) CheckPendingClientInput(sc);
	    break;
	case LbxCloseEvent:
	    DBG(DBG_CLIENT, (stderr, "close client %d\n",
			     ((xLbxCloseEvent *)rep)->client));
	    client = clients[((xLbxCloseEvent *)rep)->client];
	    client->closeDownMode = DestroyAll;
	    CloseDownClient(client);
	    break;
	case LbxInvalidateTagEvent:
	    DBG(DBG_CLIENT, (stderr, "invalidate tag %d type %d\n",
			     ((xLbxInvalidateTagEvent *)rep)->tag,
			     ((xLbxInvalidateTagEvent *)rep)->tagType));
	    LbxFreeTag(server,
		       ((xLbxInvalidateTagEvent *)rep)->tag,
		       ((xLbxInvalidateTagEvent *)rep)->tagType);
	    break;
	case LbxSendTagDataEvent:
	    DBG(DBG_CLIENT, (stderr, "send tag data %d type %d\n",
			     ((xLbxSendTagDataEvent *)rep)->tag,
			     ((xLbxSendTagDataEvent *)rep)->tagType));
	    LbxSendTagData(sc,
			   ((xLbxSendTagDataEvent *)rep)->tag,
			   ((xLbxSendTagDataEvent *)rep)->tagType);
	    break;
	case LbxListenToOne:
	    DBG(DBG_CLIENT, (stderr, "listen to one client %d\n",
			     ((xLbxListenToOneEvent *)rep)->client));
	    if (((xLbxListenToOneEvent *)rep)->client == 0xffffffff)
		LbxOnlyListenToOneClient(server->serverClient);
	    else
		LbxOnlyListenToOneClient(clients[((xLbxListenToOneEvent *)rep)->client]);
	    break;
	case LbxListenToAll:
	    DBG(DBG_CLIENT, (stderr, "listen to all clients\n"));
	    LbxListenToAllClients(server);
	    break;
	case LbxReleaseCmapEvent:
	{
	    Colormap cmap = ((xLbxReleaseCmapEvent *)rep)->colormap;
	    ColormapPtr pmap;

	    pmap = (ColormapPtr) LookupIDByType (client, cmap, RT_COLORMAP);
#ifdef COLOR_DEBUG
	    fprintf (stderr, "\nGot LbxReleaseCmapEvent, cmap = 0x%x\n\n", cmap);
#endif
	    if (pmap && pmap->grab_status == CMAP_GRABBED)
		ReleaseCmap (client, pmap);
	    break;
	}
	case LbxFreeCellsEvent:
	{
	    Colormap cmap = ((xLbxFreeCellsEvent *)rep)->colormap;
	    Pixel start = ((xLbxFreeCellsEvent *)rep)->pixelStart;
	    Pixel end = ((xLbxFreeCellsEvent *)rep)->pixelEnd;
	    ColormapPtr pmap;

	    pmap = (ColormapPtr) LookupIDByType (client, cmap, RT_COLORMAP);
#ifdef COLOR_DEBUG
	    fprintf (stderr, "\nGot LbxFreeCellsEvent, cmap = 0x%x, ", cmap);
	    fprintf (stderr, "startPixel = %d, endPixel = %d\n\n",
		     start, end);
#endif
	    if (pmap && pmap->grab_status == CMAP_GRABBED)
		GotServerFreeCellsEvent (pmap, start, end);
	    break;
	}
	}
    } else if ((rep->generic.type == server->lbxEvent &&
		rep->generic.data1 == LbxMotionDeltaEvent) ||
	       (rep->generic.type == server->lbxEvent + LbxQuickMotionDeltaEvent))
    {
	lbxMotionCache *motionCache = &server->motionCache;

	/*
	 * We use the motion delta event to generate a real MotionNotify event.
	 *
	 * The motion cache contains the last motion event we got from
	 * the server.
	 *
	 * The following are always stored in the cache in the proxy's
	 * byte order:
	 *     sequenceNumber, time, rootX, rootY, eventX, eventY
	 * This is because when constructing the MotionNotify event using
	 * the delta event, we must do arithmetic in the proxy's byte order.
	 *
	 * The following are stored in the byte order of the latest client
	 * receiving a motion event (indicated by motionCache->swapped):
	 *     root, event, child, state
	 * The assumption is that a client will receive a series of motion
	 * events, and we don't want to unnecessarily swap these fields.
	 * If the next motion event goes to a client with a byte order
	 * different from the previous client, we will have to swap these
	 * fields.
	 */

	AllowMotion(client, 1);

	if (rep->generic.type == server->lbxEvent)
	{
	    lbxMotionDeltaEvent *mev = (lbxMotionDeltaEvent *) rep;

	    motionCache->sequenceNumber += mev->deltaSequence;
	    motionCache->time += mev->deltaTime;
	    motionCache->rootX += mev->deltaX;
	    motionCache->rootY += mev->deltaY;
	    motionCache->eventX += mev->deltaX;
	    motionCache->eventY += mev->deltaY;
	}
	else
	{
	    lbxQuickMotionDeltaEvent *qmev = (lbxQuickMotionDeltaEvent *) rep;

	    motionCache->time += qmev->deltaTime;
	    motionCache->rootX += qmev->deltaX;
	    motionCache->rootY += qmev->deltaY;
	    motionCache->eventX += qmev->deltaX;
	    motionCache->eventY += qmev->deltaY;
	}

	if (!client->clientGone) {
	    xEvent ev;
	    int n;

	    if (motionCache->swapped != client->swapped)
	    {
		swapl (&motionCache->root, n);
		swapl (&motionCache->event, n);
		swapl (&motionCache->child, n);
		swaps (&motionCache->state, n);
		motionCache->swapped = client->swapped;
	    }

	    ev.u.u.type = MotionNotify;
	    ev.u.u.detail = motionCache->detail;
	    ev.u.u.sequenceNumber = motionCache->sequenceNumber;
	    ev.u.keyButtonPointer.time = motionCache->time;
	    ev.u.keyButtonPointer.rootX = motionCache->rootX;
	    ev.u.keyButtonPointer.rootY = motionCache->rootY;
	    ev.u.keyButtonPointer.eventX = motionCache->eventX;
	    ev.u.keyButtonPointer.eventY = motionCache->eventY;
	    ev.u.keyButtonPointer.root = motionCache->root;
	    ev.u.keyButtonPointer.event = motionCache->event;
	    ev.u.keyButtonPointer.child = motionCache->child;
	    ev.u.keyButtonPointer.state = motionCache->state;
	    ev.u.keyButtonPointer.sameScreen = motionCache->sameScreen;

	    if (client->swapped) {
		swaps (&ev.u.u.sequenceNumber, n);
		swapl (&ev.u.keyButtonPointer.time, n);
		swaps (&ev.u.keyButtonPointer.rootX, n);
		swaps (&ev.u.keyButtonPointer.rootY, n);
		swaps (&ev.u.keyButtonPointer.eventX, n);
		swaps (&ev.u.keyButtonPointer.eventY, n);
	    }
	    /*
	     * Write the reply
	     */

	    DoLBXReply (client, (char *) &ev, sz_xEvent);
	}

    } else {
	len = RequestLength(rep, sc, 8, &part);
	DBG(DBG_IO, (stderr, "upstream %d len %d\n", client->index, len));
	if (client->index == 0) {
	    ServerReply(server, rep);
	} else {
	    xEvent      ev;
	    char       *rp;
	    int		n;

	    if (!client->awaitingSetup && UnsquishEvent(rep, &ev, &len))
		rp = (char *) &ev;
	    else
		rp = (char *) rep;

	    if (rep->generic.type == MotionNotify) {
		xEvent *mev = (xEvent *) rp;
		lbxMotionCache *motionCache = &server->motionCache;

		AllowMotion(client, 1);
		motionCache->swapped = client->swapped;
		motionCache->detail = mev->u.u.detail;
		motionCache->root = mev->u.keyButtonPointer.root;
		motionCache->event = mev->u.keyButtonPointer.event;
		motionCache->child = mev->u.keyButtonPointer.child;
		motionCache->state = mev->u.keyButtonPointer.state;
		motionCache->sameScreen = mev->u.keyButtonPointer.sameScreen;
		motionCache->sequenceNumber = mev->u.u.sequenceNumber;
		motionCache->time = mev->u.keyButtonPointer.time;
		motionCache->rootX = mev->u.keyButtonPointer.rootX;
		motionCache->rootY = mev->u.keyButtonPointer.rootY;
		motionCache->eventX = mev->u.keyButtonPointer.eventX;
		motionCache->eventY = mev->u.keyButtonPointer.eventY;
		if (client->swapped)
		{
		    swaps(&motionCache->sequenceNumber, n);
		    swapl(&motionCache->time, n);
		    swaps(&motionCache->rootX, n);
		    swaps(&motionCache->rootY, n);
		    swaps(&motionCache->eventX, n);
		    swaps(&motionCache->eventY, n);
		}
	    }

	    /*
	     * Write the reply
	     */

	    if (!client->clientGone)
		DoLBXReply (client, rp, len);
	    client->awaitingSetup = FALSE;
	}
    }

    return Success;
}

static void
LbxIgnoreAllClients(server)
    XServerPtr  server;
{
    if (!server->lbxIgnoringAll) {
	if (GrabInProgress) {
	    server->lbxGrabInProgress = GrabInProgress;
	    ListenToAllClients();
	}
	OnlyListenToOneClient(server->serverClient);
	server->lbxIgnoringAll = TRUE;
    }
}

/* ARGSUSED */
static void
LbxAttendAllClients(server)
    XServerPtr  server;
{
    if (server->lbxIgnoringAll) {
	ListenToAllClients();
	server->lbxIgnoringAll = FALSE;
	if (server->lbxGrabInProgress) {
	    OnlyListenToOneClient(clients[server->lbxGrabInProgress]);
	    server->lbxGrabInProgress = 0;
	}
    }
}

/* ARGSUSED */
static void
LbxOnlyListenToOneClient(client)
    ClientPtr   client;
{
    /*
     * For a multi-display proxy, there is no need to do anything -
     * don't want one server grab to impact the clients for a
     * different server.
     */
    return;
}

/* ARGSUSED */
static void
LbxListenToAllClients(server)
    XServerPtr server;
{
    /*
     * For a multi-display proxy, there is no need to do anything -
     * don't want one server grab to impact the clients for a
     * different server.
     */
    return;
}

/* ARGSUSED */
static Bool
ProxyWorkProc(dummy, index)
    pointer     dummy;
    int         index;
{
    XServerPtr  server;
    xLbxAllowMotionReq req;

    if ((server = servers[index]) == 0)
	return TRUE;
    if (!server->initialized)
	return TRUE;

    if (server->motion_allowed) {
	DBG(DBG_CLIENT, (stderr, "allow %d motion events\n",
			 server->motion_allowed));
	req.reqType = server->lbxReq;
	req.lbxReqType = X_LbxAllowMotion;
	req.length = 2;
	req.num = server->motion_allowed;
	server->motion_allowed = 0;
	WriteToClient(server->serverClient, sizeof(req), &req);
    }
    /* Need to flush the output buffers before we flush compression buffer */
    if (NewOutputPending)
	FlushAllOutput();

    if (server->compHandle) {
	if (server->lbxNegOpt.streamOpts.streamCompInputAvail(server->fd))
	    AvailableClientInput(server->serverClient);
	if (server->lbxNegOpt.streamOpts.streamCompFlush(server->fd) != 0)
	    MarkConnectionWriteBlocked(server->serverClient);
    }
    /*
     * If we've got stuff remaining in the output buffers to the server, then
     * don't allow reads from any other clients, otherwise we could overflow.
     */
    if (PendingClientOutput(server->serverClient))
	LbxIgnoreAllClients(server);
    else
	LbxAttendAllClients(server);

    return FALSE;
}

Bool reconnectAfterCloseServer = FALSE;

void
CloseServer(client)
    ClientPtr   client;	/* This client is connected to a display server */
{
    XServerPtr  server;
    int         i;
    int		found;

    DBG(DBG_CLOSE, (stderr, "closing down server\n"));

    server = client->server;
    servers[server->index] = 0;
    LBXFreeDeltaCache(&server->indeltas);
    LBXFreeDeltaCache(&server->outdeltas);
    if (server->compHandle)
	server->lbxNegOpt.streamOpts.streamCompFreeHandle(server->compHandle);

    /*
     * If another server is still active, don't terminate
     */
    for (found = 0, i = 0; i < lbxMaxServers; i++) {
	if (servers[i]) {
	    found = 1;
	    break;
	}
    }
    if (!found && !reconnectAfterCloseServer)
	dispatchException |= DE_TERMINATE;

    /*
     * Close all of the "real" clients for this server
     */
    for (i = 1; i < currentMaxClients; i++) {
       if (clients[i] && 
	   clients[i] != client && 
	   clients[i]->server == server) {	    

	    client->clientGone = TRUE;
	    CloseClient (clients[i]);
	    FreeClientResources (clients[i]);
	    CloseDownConnection (clients[i]);
	    if (clients[i]->index < nextFreeClientID)
		nextFreeClientID = clients[i]->index;
	    clients[i] = NullClient;
	    xfree (clients[i]);
	    --nClients;
	    while (!clients[currentMaxClients-1])
		currentMaxClients--;
	}
    }

    /*
     * Need to remove this server's listen port(s)
     */
    for (i=0; i < MAXTRANSPORTS; i++)
        if (server->listen_fds[i] != -1) {
	    close (server->listen_fds[i]);
	    FD_CLR (server->listen_fds[i], &WellKnownConnections);
	    FD_CLR (server->listen_fds[i], &AllSockets);
        }

    /* remove all back pointers */
    for (i = 1; i < currentMaxClients; i++) {
	if (clients[i] && clients[i]->server == server)
	    clients[i]->server = NULL;
    }

    /*
     * Try to reconnect to this server
     */
    if (reconnectAfterCloseServer && !ConnectToServer (server->display_name)) {

	fprintf (stderr, "could not reconnect to '%s'\n", server->display_name);

        if (!found && !proxyMngr)
	    /* 
	     * There is no need to continue if there is no proxyManager
	     */
	    dispatchException |= DE_TERMINATE;
    }

    if (server->display_name)
	free (server->display_name);
    if (server->proxy_name)
	free (server->proxy_name);
    xfree (server->requestVector);
    xfree(server);

    CloseDownFileDescriptor(client);

    isItTimeToYield = 1;
}


static void
StartProxyReply(server, rep)
    XServerPtr  server;
    xLbxStartReply *rep;
{
    int         replylen;

    replylen = (rep->length << 2) + sz_xLbxStartReply - sz_xLbxStartReplyHdr;
    if (rep->nOpts == 0xff) {
	fprintf(stderr, "WARNING: option negotiation failed - using defaults\n");
	LbxOptInit(server);
    } else if (LbxOptParseReply(server, rep->nOpts,
			(unsigned char *)&rep->optDataStart, replylen) < 0) {
	FatalError("Bad options from server");
    }

#ifdef OPTDEBUG
    fprintf(stderr, "server: N = %d, maxlen = %d, proxy: N = %d, maxlen = %d\n",
	    server->lbxNegOpt.serverDeltaN, server->lbxNegOpt.serverDeltaMaxLen,
	    server->lbxNegOpt.proxyDeltaN, server->lbxNegOpt.proxyDeltaMaxLen);
#endif

    LBXInitDeltaCache(&server->indeltas, server->lbxNegOpt.serverDeltaN,
		      server->lbxNegOpt.serverDeltaMaxLen);
    LBXInitDeltaCache(&server->outdeltas, server->lbxNegOpt.proxyDeltaN,
		      server->lbxNegOpt.proxyDeltaMaxLen);
    QueueWorkProc(ProxyWorkProc, NULL, (pointer)(long) server->index);

#ifdef OPTDEBUG
    fprintf(stderr, "squishing = %d\n", server->lbxNegOpt.squish);
    fprintf(stderr, "useTags = %d\n", server->lbxNegOpt.useTags);
#endif

    TagsInit(server, server->lbxNegOpt.useTags);

    if (!server->lbxNegOpt.useTags)
    {
	ProcVector[X_GetModifierMapping] = ProcStandardRequest;
	ProcVector[X_GetKeyboardMapping] = ProcStandardRequest;
	ProcVector[X_QueryFont] = ProcStandardRequest;
	ProcVector[X_ChangeProperty] = ProcStandardRequest;
	ProcVector[X_GetProperty] = ProcStandardRequest;
    }

    if (server->lbxNegOpt.streamOpts.streamCompInit) {
	unsigned char   *extra = (unsigned char *) rep;
	int		len = sizeof(xReply) + (rep->length << 2);
	int		left = BytesInClientBuffer(server->serverClient);

	server->compHandle =
	    (*server->lbxNegOpt.streamOpts.streamCompInit) (
		server->fd, server->lbxNegOpt.streamOpts.streamCompArg);
	SwitchConnectionFuncs(server->serverClient,
	    server->lbxNegOpt.streamOpts.streamCompRead,
	    server->lbxNegOpt.streamOpts.streamCompWriteV);
	extra += len;
	server->lbxNegOpt.streamOpts.streamCompStuffInput(server->fd, 
							  extra, 
							  left);
	SkipInClientBuffer(server->serverClient, left + len, 0);
	StartOutputCompression(server->serverClient,
	    server->lbxNegOpt.streamOpts.streamCompOn,
	    server->lbxNegOpt.streamOpts.streamCompOff);
    }
    server->initialized = TRUE;
    MakeClientGrabImpervious(server->serverClient);

    if (SendInternAtoms(server))
	ExpectServerReply (server, InternAtomsReply);
    else
    {
	SendInitLBXPackets(server);

	/*
	 * Now the proxy is ready to accept connections from clients.
	 */

	(void) ListenWellKnownSockets ();
    }
}

static void
StartProxy(server)
    XServerPtr  server;
{
    char        buf[1024];
    int         reqlen;
    xLbxStartProxyReq *n = (xLbxStartProxyReq *) buf;

    LbxOptInit(server);
    n->reqType = server->lbxReq;
    n->lbxReqType = X_LbxStartProxy;
    reqlen = LbxOptBuildReq(server, buf + sz_xLbxStartProxyReq);
    assert(reqlen > 0 && reqlen + sz_xLbxStartProxyReq <= 1024);
    n->length = (reqlen + sz_xLbxStartProxyReq + 3) >> 2;
    /*
     * Don't call WriteToServer because we don't want to switch.
     */
    WriteToClient(server->serverClient, n->length << 2, (char *) n);
    server->serverClient->sequence++;
    ExpectServerReply(server, StartProxyReply);
    while (NewOutputPending)
	FlushAllOutput();
}

static Bool
InitServer (dpy_name, i, server, sequencep)
    char*	dpy_name;
    int		i;
    XServerPtr	server;
    int*	sequencep;
{
    server->index = i;

    DBG(DBG_IO, (stderr, "making server connection\n"));
    server->dpy = DisplayOpen (dpy_name, 
			       &server->lbxReq, 
			       &server->lbxEvent, 
			       &server->lbxError, 
			       sequencep);
    if (!server->dpy) return FALSE;

    server->fd = DisplayConnectionNumber (server->dpy);
    server->compHandle = NULL;
    server->initialized = FALSE;
    server->prev_exec = clients[0];
    server->send = clients[0];
    server->recv = clients[0];
    server->motion_allowed = 0;
    server->wm_running = FALSE;
    server->extensions = NULL;

    /*
     * Initialize the atom fields
     */
    server->atom_control_count = 0;
    server->atom_control = NULL;
    LBXReadAtomsFile(server);

    /*
     * Initialize global and property caches
     */
    server->num_caches = 0;
    server->seed = 0;

    /*
     * The ProcVector table contains the default functions plus any
     * changes that were made when the command line options were
     * parsed.
     *
     * In multi-display lbxproxy, each server may have different
     * lbx options that are negotiated.  Consequently, a version
     * of ProcVector must be maintained for each server.  The field
     * requestVector is used for this purpose.
     *
     * When a client connects, its requestVector will be set to its
     * server's requestVector.
     */
    server->requestVector = (int (**)()) xalloc (sizeof (ProcVector));
    if (!server->requestVector) return FALSE;
    memcpy (server->requestVector, ProcVector, sizeof (ProcVector));

    /*
     * Initialize the resource fields
     */
    server->lastLbxClientIndexLookup = NULL;

    /*
     * Initialize the grab state variables
     */
    server->lbxIgnoringAll = 0;
    server->lbxGrabInProgress = 0;

    return TRUE;
}

Bool
ConnectToServer(dpy_name)
    char       *dpy_name;
{
    int         i, j;
    XServerPtr  server;
    int         sequence;
    ClientPtr   sc;
    static int 	been_there;
    static char my_host[250];
    char 	proxy_address[250+6];

#ifdef NEED_UTSNAME
    struct utsname name;
#endif

    for (i = 0; i < lbxMaxServers; i++)
	if (!servers[i])
	    break;
    if (i == lbxMaxServers) {
	if (proxyMngr)
	    SendGetProxyAddrReply( PM_iceConn, PM_Unable, NULL,
				   "too many servers" );
	return FALSE;
    }
    server = (XServerPtr) xalloc(sizeof(XServerRec));
    if (!server) {
	if (proxyMngr)
	    SendGetProxyAddrReply( PM_iceConn, PM_Unable, NULL,
				   "memory allocation failure");
	return FALSE;
    }
    bzero(server, sizeof(XServerRec));
    
    if (!InitServer (dpy_name, i, server, &sequence)) {
	if (proxyMngr) {
	    (void) snprintf (proxy_address, sizeof(proxy_address),
			    "could not connect to server '%s'",
			    dpy_name);
	    SendGetProxyAddrReply( PM_iceConn, PM_Failure, NULL, proxy_address);
	}
	xfree(server);
	return FALSE;
    }

    /*
     * Create the socket(s) this display will listen on
     */
    for (j=0; j < MAXTRANSPORTS; j++)
	server->listen_fds[j] = -1;
    CreateServerSockets(server->listen_fds);

    /*
     * Generate the proxy address and save the host name part
     */
    if (!been_there || i == 0) {
	been_there++;

	clients[0]->server = server;

#ifdef NEED_UTSNAME
	uname (&name);
	(void) snprintf(my_host,sizeof(my_host),"%s",name.nodename);
#else
        (void) gethostname (my_host,sizeof(my_host));
#endif
    }
    if (snprintf (proxy_address, sizeof(proxy_address) ,"%s:%s", my_host,
		  display) >= sizeof(proxy_address)) {
	(void) snprintf (proxy_address, sizeof(proxy_address),
			 "display name too long");
	SendGetProxyAddrReply( PM_iceConn, PM_Failure, NULL, proxy_address);
	return FALSE;
    }
    
    servers[i] = server;

    sc = AllocNewConnection(server->fd, -1, TRUE, NULL);
    sc->server = server;
    sc->public.requestLength = ServerRequestLength;
    sc->lbxIndex = i;
    sc->requestVector = ServerVector;
    sc->awaitingSetup = FALSE;
    sc->sequence = sequence;

    /*
     * AllocNewConn didn't initialize the client resources so
     * it needs to be done here
     */
    InitClientResources(sc);

    server->serverClient = sc;
    server->proxy_name = strdup (proxy_address);
    if (dpy_name)
	server->display_name = strdup (dpy_name);

    StartProxy(server);

    if (proxyMngr) {
	SendGetProxyAddrReply( PM_iceConn, PM_Success, proxy_address, NULL );
    }

    return TRUE;
}
