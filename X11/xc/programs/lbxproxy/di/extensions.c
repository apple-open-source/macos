/* $Xorg: extensions.c,v 1.5 2001/02/09 02:05:31 xorgcvs Exp $ */
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
 * Copyright 1994 Network Computing Devices, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name Network Computing Devices, Inc. not be
 * used in advertising or publicity pertaining to distribution of this
 * software without specific, written prior permission.
 *
 * THIS SOFTWARE IS PROVIDED `AS-IS'.  NETWORK COMPUTING DEVICES, INC.,
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING WITHOUT
 * LIMITATION ALL IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE, OR NONINFRINGEMENT.  IN NO EVENT SHALL NETWORK
 * COMPUTING DEVICES, INC., BE LIABLE FOR ANY DAMAGES WHATSOEVER, INCLUDING
 * SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES, INCLUDING LOSS OF USE, DATA,
 * OR PROFITS, EVEN IF ADVISED OF THE POSSIBILITY THEREOF, AND REGARDLESS OF
 * WHETHER IN AN ACTION IN CONTRACT, TORT OR NEGLIGENCE, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

/* tracks server extensions */

#include	"lbx.h"
#include	"assert.h"
#include	"wire.h"
#include	"lbxext.h"
#include	"reqtype.h"
#include	"swap.h"

typedef struct _extinfo {
    int		nlen;
    char       *name;
    int         idx;
    int         opcode;
    int         event;
    int         error;
    CARD8      *rep_mask,
               *ev_mask;
    int         num_reqs;
    ExtensionInfoPtr next;
}           ExtensionInfoRec;

static Bool
AddExtension(server, name, reply, rep_mask, ev_mask)
    XServerPtr  server;
    char       *name;
    xLbxQueryExtensionReplyPtr reply;
    CARD8      *rep_mask,
               *ev_mask;
{
    ExtensionInfoPtr eip = NULL;
    int         req_mask_len;

    for (eip = server->extensions; eip; eip = eip->next) {
	if (!strcmp(eip->name, name))
	    return TRUE;
    }
    eip = (ExtensionInfoPtr) xalloc(sizeof(ExtensionInfoRec));
    if (!eip)
	return FALSE;
    eip->nlen = strlen(name);
    eip->name = (char *) xalloc(eip->nlen + 1);
    if (!eip->name) {
	xfree(eip);
	return FALSE;
    }
    strcpy(eip->name, name);
    req_mask_len = (reply->numReqs + 7) >> 3;
    eip->rep_mask = (CARD8 *) xalloc(req_mask_len);
    eip->ev_mask = (CARD8 *) xalloc(req_mask_len);
    if (req_mask_len && (!eip->rep_mask || !eip->ev_mask)) {
	xfree(eip->rep_mask);
	xfree(eip->ev_mask);
	xfree(eip->name);
	xfree(eip);
	return FALSE;
    }
    if (reply->numReqs) {
	memcpy((char *) eip->rep_mask, (char *) rep_mask, req_mask_len);
	memcpy((char *) eip->ev_mask, (char *) ev_mask, req_mask_len);
    }
    eip->num_reqs = reply->numReqs;
    eip->opcode = reply->major_opcode;
    eip->event = reply->first_event;
    eip->error = reply->first_error;
    eip->next = server->extensions;
    server->extensions = eip;
    return TRUE;
}

void
DeleteExtensions(server)
    XServerPtr server;
{
    ExtensionInfoPtr eip;

    while ((eip = server->extensions)) {
	server->extensions = eip->next;
	xfree(eip->name);
	xfree(eip->rep_mask);
	xfree(eip->ev_mask);
	xfree(eip);
    }
}

/*ARGSUSED*/
void
HandleExtensionError(client, err, nr)
    ClientPtr   client;
    xError     *err;
    ReplyStuffPtr nr;
{
}

/*ARGSUSED*/
void
HandleExtensionEvent(client, ev)
    ClientPtr   client;
    xEvent     *ev;
{
}

static Bool
check_mask(mask, minorop)
    CARD8      *mask;
    int         minorop;
{
    if (mask[minorop >> 3] & (1 << (minorop & 7)))
	return REQ_TYPE_YES;
    else
	return REQ_TYPE_NO;
}

Bool
CheckExtensionForEvents(client, req)
    ClientPtr client;
    xReq       *req;
{
    int         opcode = req->reqType;
    int         minorop = req->data;
    ExtensionInfoPtr eip;

    for (eip = client->server->extensions; eip; eip = eip->next) {
	if (eip->opcode == opcode) {
	    if (!eip->ev_mask || !eip->num_reqs)
		return TRUE;	/* assume worst */
	    return check_mask(eip->ev_mask, minorop);
	}
    }
    /* assume the worst */
    return TRUE;
}

Bool
CheckExtensionForReplies(client, req)
    ClientPtr client;
    xReq       *req;
{
    int         opcode = req->reqType;
    int         minorop = req->data;
    ExtensionInfoPtr eip;

    for (eip = client->server->extensions; eip; eip = eip->next) {
	if (eip->opcode == opcode) {
	    if (!eip->rep_mask || !eip->num_reqs)
		return REQ_TYPE_MAYBE;	/* assume worst */
	    return check_mask(eip->rep_mask, minorop);
	}
    }
    /* assume the worst */
    return REQ_TYPE_MAYBE;
}

static Bool
HandleLbxQueryExtensionReply(client, nr, data)
    ClientPtr   client;
    ReplyStuffPtr nr;
    char       *data;
{
    xLbxQueryExtensionReply *reply;
    xQueryExtensionReply crep;
    char       *ename;
    CARD8       *rep_mask = NULL,
               *ev_mask = NULL;

    reply = (xLbxQueryExtensionReply *) data;

    ename = nr->request_info.lbxqueryextension.name;

    if (reply->length) {
	rep_mask = (CARD8 *) &reply[1];
	ev_mask = rep_mask + ((((reply->numReqs + 7) >> 3) + 3) & ~3);
    }
    if (reply->present)
	AddExtension(client->server, ename, reply, rep_mask, ev_mask);
    xfree(ename);

    /* and finally tell client about it */
    crep.type = X_Reply;
    crep.length = 0;
    crep.sequenceNumber = reply->sequenceNumber;
    crep.present = reply->present;
    crep.major_opcode = reply->major_opcode;
    crep.first_event = reply->first_event;
    crep.first_error = reply->first_error;
    if (client->swapped)
	SwapQueryExtensionReply(&crep);
    WriteToClient(client, sizeof(xQueryExtensionReply), (char *) &crep);

    return TRUE;
}

static int
QueryExtensionReply(client, present, opcode, event, error)
    ClientPtr client;
    Bool present;
    int opcode, event, error;
{
    xQueryExtensionReply reply;

    reply.type = X_Reply;
    reply.length = 0;
    reply.sequenceNumber = LBXSequenceNumber(client);
    reply.present = present;
    reply.major_opcode = opcode;
    reply.first_event = event;
    reply.first_error = error;
    if (client->swapped)
	SwapQueryExtensionReply(&reply);
    if (LBXCacheSafe(client)) {
	FinishLBXRequest(client, REQ_YANK);
	WriteToClient(client, sizeof(xQueryExtensionReply), (char *)&reply);
    } else {			/* store for later */
	if (!LBXCanDelayReply(client))
	    SendLbxSync(client);
	FinishLBXRequest(client, REQ_YANKLATE);
	SaveReplyData(client, (xReply *) &reply, 0, NULL);
    }
    return Success;
}

int
ProcLBXQueryExtension(client)
    ClientPtr   client;
{
    REQUEST(xQueryExtensionReq);
    char        n;
    char       *ename;
    xLbxQueryExtensionReq req;
    CARD16      nlen;
    ReplyStuffPtr nr;
    ExtensionInfoPtr eip;

    nlen = stuff->nbytes;
    if (client->swapped) {
	swaps(&nlen, n);
    }
    if (nlen == (sizeof(LBXNAME) - 1) &&
	!strncmp((char *)&stuff[1], LBXNAME, nlen))
	return QueryExtensionReply(client, FALSE, 0, 0, 0);
    for (eip = client->server->extensions; eip; eip = eip->next) {
	if (nlen == eip->nlen && !strncmp((char *)&stuff[1], eip->name, nlen))
	    return QueryExtensionReply(client, TRUE, eip->opcode,
				       eip->event, eip->error);
    }
    ename = (char *) xalloc(nlen + 1);
    if (!ename)
	return BadAlloc;
    memcpy(ename, (char *) &stuff[1], nlen);
    ename[nlen] = '\0';

    req.reqType = client->server->lbxReq;
    req.lbxReqType = X_LbxQueryExtension;
    req.length = 2 + ((nlen + 3) >> 2);
    req.nbytes = nlen;
    if (client->swapped)
	SwapQueryExtension(&req);
    nr = NewReply(client, client->server->lbxReq, X_LbxQueryExtension,
		  HandleLbxQueryExtensionReply);
    if (!nr) {
	xfree(ename);
	return ProcStandardRequest(client);
    }
    nr->request_info.lbxqueryextension.name = ename;
    FinishLBXRequest(client, REQ_PASSTHROUGH);
    WriteToServer(client, sizeof(req), (char *) &req, TRUE, TRUE);
    WriteToServer(client, nlen, ename, FALSE, TRUE);

    return Success;
}
