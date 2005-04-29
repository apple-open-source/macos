/* $Xorg: props.c,v 1.4 2001/02/09 02:05:32 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/lbxproxy/di/props.c,v 1.7 2003/10/24 20:38:12 tsi Exp $ */

/*
 * property handling
 */


#include	<stdio.h>
#include	"misc.h"
#include	"assert.h"
#include	"lbx.h"
#include	"util.h"
#include	"tags.h"
#include	"resource.h"
#include	"wire.h"
#include	"swap.h"
#include	"proxyopts.h"
#include	"atomcache.h"

static Bool GetLbxChangePropertyReply();
static Bool GetLbxGetPropertyReply();

/*
 * wrapper for tags
 *
 * since this stuff has pointers, have to package it for
 * tag cache
 */
static Bool
propTagStoreData(server, tid, dlen, swapit, ptdp)
    XServerPtr	server;
    XID         tid;
    unsigned long dlen;
    Bool	swapit;
    PropertyTagDataPtr ptdp;
{
    PropertyTagDataPtr new;

    new = (PropertyTagDataPtr) xalloc(dlen + sizeof(PropertyTagDataRec));
    if (!new)
	return FALSE;
    *new = *ptdp;
    new->data = (pointer) (new + 1);
    memcpy((char *) new->data, (char *) ptdp->data, dlen);
    /* save data in proxy format */
    if (swapit) {
	switch (new->format) {
	case 32:
	    SwapLongs((CARD32 *) new->data, dlen >> 2);
	    break;
	case 16:
	    SwapShorts((short *) new->data, dlen >> 1);
	    break;
	}
    }
    return TagStoreDataNC(server, server->prop_cache, tid,
			  (dlen + sizeof(PropertyTagDataRec)),
			  LbxTagTypeProperty, new);
}

/*
 * lots of brains need to live here.  this decides whether or not
 * to use an LbxChangeProperty
 *
 * some possible heuristics:
 *
 *	- don't rewrite if it's small -- roundtrip swamps bandwidth
 *	- don't rewrite if it's a WM property and the WM isn't sharing
 *	  the proxy
 *	- don't rewrite if the prop is on a non-root window owned by
 *	  a non-proxy client
 *
 *	- do rewrite if it's a prop on one of the client's windows
 *	- do rewrite if it's a Selection property (hope is that
 *	  receiver is also using proxy)
 */
/*ARGSUSED*/
static Bool
rewrite_change_prop(
    ClientPtr client,
    Window win,
    Atom property,
    Atom type,
    int format,
    int mode,
    unsigned long nUnits
)
{
    unsigned flags;

    /* if tags are turned off, don't try */
    if (!client || !client->server)
	return FALSE;
    if (!client->server->lbxNegOpt.useTags)
	return FALSE;
    /* we aren't nearly clever enough to know what data we have */
    if (mode != PropModeReplace)
	return FALSE;
    if ((nUnits * (format >> 3)) < min_keep_prop_size)
	return FALSE;
    flags = FlagsForAtom(client->server, property);
    if (flags & AtomNoCacheFlag)
	return FALSE;
    if ((flags & AtomWMCacheFlag) && !client->server->wm_running)
	return FALSE;
    return TRUE;
}

int
ProcLBXChangeProperty(client)
    ClientPtr   client;
{
    REQUEST(xChangePropertyReq);
    ReplyStuffPtr nr;
    int         size;
    pointer     datacopy;
    char	n;
    Atom	type, property;
    Window	win;
    CARD32	nUnits;

    win = stuff->window;
    property = stuff->property;
    type = stuff->type;
    nUnits = stuff->nUnits;

    if (client->swapped) {
    	swapl(&win, n);
    	swapl(&property, n);
    	swapl(&type, n);
    	swapl(&nUnits, n);
    }

    /* we may want to leave it as X_ChangeProperty */
    if (!rewrite_change_prop(client, win, property, type,
			     stuff->format, stuff->mode, nUnits))
	return ProcStandardRequest(client);

    size = nUnits * (stuff->format >> 3);
    datacopy = (pointer) xalloc(size);
    if (!datacopy)
	return ProcStandardRequest(client);
    memcpy((char *) datacopy, (char *) &stuff[1], size);

    nr = NewReply(client, client->server->lbxReq, X_LbxChangeProperty,
		  GetLbxChangePropertyReply);
    if (!nr) {
	xfree(datacopy);
	return ProcStandardRequest(client);
    }
    nr->request_info.lbxchangeprop.ptd.length = size;
    nr->request_info.lbxchangeprop.ptd.type = type;
    nr->request_info.lbxchangeprop.ptd.format = stuff->format;
    nr->request_info.lbxchangeprop.ptd.data = datacopy;

    FinishLBXRequest(client, REQ_REPLACELATE);
    SendChangeProperty(client, win, property, type,
		       stuff->format, stuff->mode, nUnits);
    return Success;
}

static Bool
GetLbxChangePropertyReply(client, nr, data)
    ClientPtr   client;
    ReplyStuffPtr nr;
    char       *data;
{
    xLbxChangePropertyReply *rep;
    PropertyTagDataPtr ptdp;
    char	n;
    CARD32	tag;

    rep = (xLbxChangePropertyReply *) data;
    tag = rep->tag;
    if (client->swapped) {
    	swapl(&tag, n);
    }

    ptdp = &nr->request_info.lbxchangeprop.ptd;
    if (tag) {
	if (!propTagStoreData(client->server, tag, (unsigned long)ptdp->length,
			      client->swapped, ptdp)) {
	    if (client->swapped) {
		switch (ptdp->format) {
		case 32:
		    SwapLongs((CARD32 *) ptdp->data, ptdp->length >> 2);
		    break;
		case 16:
		    SwapShorts((short *) ptdp->data, ptdp->length >> 1);
		    break;
		}
	    }
	    SendTagData(client, tag, ptdp->length, ptdp->data);
	    SendInvalidateTag(client, tag);
	    WriteError(client, X_ChangeProperty, 0, 0, BadAlloc);
	}
    }
    xfree(ptdp->data);		/* propStore gets its own copy */
    return TRUE;
}

int
ProcLBXGetProperty(client)
    ClientPtr   client;
{
    REQUEST(xGetPropertyReq);
    ReplyStuffPtr nr;

    nr = NewReply(client, client->server->lbxReq, X_LbxGetProperty,
		  GetLbxGetPropertyReply);
    if (!nr)
	return ProcStandardRequest(client);

    if (client->swapped) {
	char        n;

	swapl(&stuff->window, n);
	swapl(&stuff->property, n);
	swapl(&stuff->type, n);
	swapl(&stuff->longOffset, n);
	swapl(&stuff->longLength, n);
    }
    nr->request_info.lbxgetprop.offset = stuff->longOffset;
    nr->request_info.lbxgetprop.length = stuff->longLength;

    FinishLBXRequest(client, REQ_PASSTHROUGH);

    SendGetProperty(client, stuff->window, stuff->property, stuff->type,
		    stuff->delete, stuff->longOffset, stuff->longLength);
    return Success;
}

static Bool
GetLbxGetPropertyReply(client, nr, data)
    ClientPtr   client;
    ReplyStuffPtr nr;
    char       *data;
{
    xLbxGetPropertyReply *rep;
    unsigned long len;
    PropertyTagDataRec ptd;
    PropertyTagDataPtr ptdp;
    pointer     pdata = NULL;
    char	*sdata;
    char        n;
    xGetPropertyReply reply;
    CARD32	tag, nItems, type, bytesAfter;

    rep = (xLbxGetPropertyReply *) data;

    tag = rep->tag;
    nItems = rep->nItems;
    type = rep->propertyType;
    bytesAfter = rep->bytesAfter;
    if (client->swapped) {
	swapl(&tag, n);
	swapl(&nItems, n);
	swapl(&type, n);
	swapl(&bytesAfter, n);
    }
    ptd.type = type;
    ptd.format = rep->format;
    ptd.data = NULL;
    len = rep->length << 2;
    if (len)
	ptd.length = nItems * (rep->format >> 3);
    else
	ptd.length = 0;
    ptdp = &ptd;

    if (tag) {
	if (len) {	/* first time, set tag */

#ifdef LBX_STATS
	    getprop_full++;
#endif

	    ptd.data = (pointer) &rep[1];
	    if (!propTagStoreData(client->server, tag, len, client->swapped, 
			&ptd)) {
		/* tell server we lost it */
		SendInvalidateTag(client, tag);
	    }
	} else {

	    ptdp = (PropertyTagDataPtr) TagGetData(client->server, 
			client->server->prop_cache, tag);

	    if (!ptdp) {
		fprintf(stderr, "no data for property tag 0x%lx\n", (long)tag);
		WriteError(client, X_GetProperty, 0, 0, BadAlloc);
		return TRUE;
	    }
	    if (client->swapped && rep->format != 8) {
		/*
		 * Make a copy, because we will need to swap the property data
		 * and we don't want to alter the tag database.
		 */
		pdata = (pointer) ALLOCATE_LOCAL(ptdp->length);
		memcpy((char *) pdata, (char *) ptdp->data, ptdp->length);
		switch (rep->format) {
		case 32:
		    SwapLongs((CARD32 *) pdata, ptdp->length >> 2);
		    break;
		case 16:
		    SwapShorts((short *) pdata, ptdp->length >> 1);
		    break;
		}
	    }

#ifdef LBX_STATS
	    getprop_tag++;
	    tag_bytes_unsent += ptdp->length;
#endif

	}
    } else if (len) {

#ifdef LBX_STATS
	getprop_full++;
#endif

	/*
	 * server didn't send us a tag -- either can't store it or no prop
	 * data
	 */
	ptd.data = (pointer) &rep[1];
    }

    if (pdata)
	sdata = pdata;
    else
	sdata = ptdp->data;
    if (rep->tag) {
	len = ptdp->length;
	reply.bytesAfter = bytesAfter;
    } else {
	len = ptdp->length - (nr->request_info.lbxgetprop.offset << 2);
	len = min(len, nr->request_info.lbxgetprop.length << 2);
	reply.bytesAfter = (ptdp->length -
			    (len + (nr->request_info.lbxgetprop.offset << 2)));
	sdata = sdata + (nr->request_info.lbxgetprop.offset << 2);
    }

    reply.type = X_Reply;
    reply.sequenceNumber = rep->sequenceNumber;
    reply.format = ptdp->format;
    reply.length = (len + 3) >> 2;
    reply.propertyType = ptdp->type;
    if (len)
	reply.nItems = len / (ptdp->format >> 3);
    else
	reply.nItems = 0;

    if (client->swapped)
	SwapGetPropertyReply(&reply);
    WriteToClient(client, sizeof(xGetPropertyReply), (char *) &reply);
    if (len)
	WriteToClient(client, len, sdata);

    if (pdata)
	DEALLOCATE_LOCAL(pdata);

    return TRUE;
}
