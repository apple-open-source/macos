/* $Xorg: lbxfuncs.c,v 1.3 2000/08/17 19:53:55 cpqbld Exp $ */
/*
 * Copyright 1994 Network Computing Devices, Inc.
 * Copyright 1996 X Consortium, Inc.
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
/* $XFree86: xc/programs/lbxproxy/di/lbxfuncs.c,v 1.6 2002/09/18 17:11:50 tsi Exp $ */

/*
 * top level LBX request & reply handling
 */

/*
 * requests copy out interesting stuff and then swap so original data
 * is left alone as much as possible.  note that the length field
 * is *not* swapped
 *
 * replied data is copied yet again before swapping because the data
 * may be stored as a tag result and we don't want to change that.
 */


#include	<stdio.h>
#include	"assert.h"
#include	"lbx.h"
#include	"atomcache.h"
#include	"util.h"
#include	"init.h"
#include	"tags.h"
#include	"resource.h"
#include	"wire.h"
#include	"swap.h"
#include	"reqtype.h"
#include	"lbxext.h"
#include	"proxyopts.h"

#ifdef DEBUG
extern int lbxDebug;
#endif

static Bool intern_atom_reply();
static Bool get_atom_name_reply();
static Bool get_mod_map_reply();
static Bool get_key_map_reply();
static Bool sync_reply();
static Bool get_queryfont_reply();
static Bool GetWinAttrAndGeomReply();

#define reply_length(cp,rep) ((rep)->type==X_Reply ? \
        32 + (HostUnswapLong((cp),(rep)->length) << 2) \
	: 32)

char protocolMode = PROTOCOL_FULL;

/* ARGSUSED */
static void 
get_connection_info(client, cs, cs_len, change_type, changes, changes_len)
    ClientPtr   client;
    xConnSetup *cs;
    int		cs_len;
    int		change_type;
    CARD32     *changes;
    int		changes_len;
{
    xWindowRoot *root;
    xDepth     *depth;
    xVisualType *vis;
    char       *dp = (char *) cs;
    int         i,
                j,
                k;
    static int  pad[4] = {0, 3, 2, 1};

    if (!changes)
	return;

    cs->ridBase = changes[0];
    dp += sizeof(xConnSetup);
    /* skip vendor string & pixmap formats */
    dp += cs->nbytesVendor + pad[cs->nbytesVendor & 3];
    for (i = 0; i < cs->numFormats; i++) {
	if (((xPixmapFormat *)dp)->depth == 1)
	    client->ZbitsPerPixel1 = ((xPixmapFormat *)dp)->bitsPerPixel;
	dp += sizeof(xPixmapFormat);
    }
    /* process screens */
    root = (xWindowRoot *) dp;
    if (change_type == 2) {
	if (changes[2])
	    root->rootVisualID = (VisualID) changes[2];
	if (changes[3]) {
	    root->defaultColormap = (Colormap) changes[3];
	    root->whitePixel = changes[4];
	    root->blackPixel = changes[5];
	}
    }
    client->rootWindow = root->windowId;
    for (i = 0; i < cs->numRoots; i++) {
	if (change_type != 2) {
	    root->currentInputMask = changes[i + 1];
	} else {
	    root->currentInputMask = changes[i + 6];
	}
	dp += sizeof(xWindowRoot);
	for (j = 0; j < root->nDepths; j++) {
	    depth = (xDepth *) dp;
	    dp += sizeof(xDepth);
	    for (k = 0; k < depth->nVisuals; k++) {
		vis = (xVisualType *) dp;
		CreateVisual(depth->depth, vis);
		dp += (sizeof(xVisualType));
		vis = (xVisualType *) dp;
	    }
	}
	if (!LookupIDByType(client, root->defaultColormap, RT_COLORMAP))
	    CreateColormap(client,
			   root->defaultColormap, root->rootVisualID);
    }
}

static void
send_setup_reply(client, success, majorVer, minorVer, cs, cs_len)
    ClientPtr client;
    Bool success;
    int majorVer, minorVer;
    void *cs;
    int cs_len;
{
    xConnSetupPrefix reply;

    reply.success = success;
    if (!success) {
	reply.lengthReason = strlen((char *)cs);
	cs_len = reply.lengthReason + 3;
    }
    reply.majorVersion = majorVer;
    reply.minorVersion = minorVer;
    reply.length = cs_len >> 2;

    if (client->swapped)
	WriteSConnSetupPrefix(client, &reply);
    else
	(void)WriteToClient(client, sizeof(xConnSetupPrefix), (char *) &reply);
    if (success && client->swapped)
	WriteSConnectionInfo(client, (unsigned long)(reply.length << 2), cs);
    else
	(void)WriteToClient(client, (int)(reply.length << 2), cs);
}

static void
finish_setup_reply(client, cs, cs_len, change_type, changes, changes_len, majorVer, minorVer)
    ClientPtr   client;
    xConnSetup *cs;
    int         cs_len;
    int		change_type;
    CARD32*     changes;
    int		changes_len;
    int		majorVer,
                minorVer;
{
    get_connection_info(client, cs, cs_len, change_type, changes, changes_len);
    client->minKeyCode = cs->minKeyCode;
    client->maxKeyCode = cs->maxKeyCode;
    client->imageByteOrder = cs->imageByteOrder;
    client->bitmapBitOrder = cs->bitmapBitOrder;
    client->bitmapScanlineUnit = cs->bitmapScanlineUnit;
    client->bitmapScanlinePad = cs->bitmapScanlinePad;
    client->ridBase = cs->ridBase;
    client->ridMask = cs->ridMask;
    FinishInitClientResources (client, cs->ridBase, cs->ridMask);
    send_setup_reply(client, TRUE, majorVer, minorVer, cs, cs_len);
}

/* 
 * ConnectionSetup data can come from three places: 1) a fully qualified
 * connection-setup in the reply, 2) from the tag store, and 3) built 
 * from the Display* returned by the call the proxy made to XOpenDisplay.
 */

static void
get_setup_reply(client, data, len)
    ClientPtr   client;
    char       *data;
    int		len;
{
    register xLbxConnSetupPrefix *rep;
    TagData     td;
    xConnSetup *tag_data;
    Bool	free_td = FALSE;
    CARD32*     changes = NULL;
    int		changes_len = 0;

    rep = (xLbxConnSetupPrefix *) data;
    if (client->swapped) {
	data = (char *)ALLOCATE_LOCAL(len);
	memcpy(data, (char *)rep, len);
	rep = (xLbxConnSetupPrefix *) data;
	SwapLbxConnSetupPrefix(rep);
    }
    switch (rep->changeType) {
    default:
    case 0:	/* full ConnectionSetup, possibly with tag */
#ifdef LBX_STATS
	getsetup_full++;
#endif
	tag_data = (xConnSetup *) &rep[1];
	if (rep->tag != 0) {
	    if (!TagStoreData(client->server, client->server->global_cache, 
			      rep->tag, rep->length,
			      LbxTagTypeConnInfo, tag_data)) {
		/* tell server we lost it */
		SendInvalidateTag(client, rep->tag);
	    }
	}
	len = rep->length << 2;
	break;

    case 1:	/* LbxNormalDeltas */
    case 2:	/* LbxEmbeddedAppGroup */
	changes = (CARD32*) &rep[1];
	changes_len = rep->length - 1;
	if (client->swapped)
	    SwapLongs(changes, changes_len);

	if (rep->tag != 0) {
	    td = TagGetTag(client->server, client->server->global_cache, 
			   rep->tag);
	    if (!td) {
		fprintf(stderr, "no data for setup tag 0x%lx\n",
			(long)rep->tag);
		send_setup_reply(client, FALSE, 0, 0, "bad tag data from server", 0);
		if (client->swapped)
		    DEALLOCATE_LOCAL(data);
		return;
	    }
#ifdef LBX_STATS
	    getsetup_tag++;
	    tag_bytes_unsent += td->size - len;
#endif
	    tag_data = td->tdata;
	    len = td->size;
	} else {
	    DisplayGetConnSetup (client->server->dpy, &tag_data, &len, 
				 rep->changeType, changes, changes_len);
	    free_td = TRUE;
	}
	break;

    }
    finish_setup_reply(client, tag_data, len, 
		       rep->changeType, changes, changes_len,
		       (int) rep->majorVersion, (int) rep->minorVersion);
    if (free_td) xfree (tag_data);
    if (client->swapped)
	DEALLOCATE_LOCAL(data);
}

int
ProcLBXInternAtom(client)
    ClientPtr   client;
{
    REQUEST(xInternAtomReq);
    char       *s;
    Atom        atom,
                a = None;
    xInternAtomReply reply;
    ReplyStuffPtr nr;
    char        n;
    CARD16      nbytes;

    nbytes = stuff->nbytes;
    if (client->swapped) {
	swaps(&nbytes, n);
    }
    if (nbytes > MAX_ATOM_LENGTH)
	return ProcStandardRequest(client);

    s = (char *)stuff + sizeof(xInternAtomReq);

    atom = LbxMakeAtom(client->server, s, nbytes, a, FALSE);
    if (atom != None) {
	reply.type = X_Reply;
	reply.length = 0;
	reply.sequenceNumber = LBXSequenceNumber(client);
	reply.atom = atom;
	if (client->swapped)
	    SwapInternAtomReply(&reply);
	if (LBXCacheSafe(client)) {
	    FinishLBXRequest(client, REQ_YANK);
	    WriteToClient(client, sizeof(xInternAtomReply), &reply);
	} else {			/* store for later */
	    if (!LBXCanDelayReply(client))
		SendLbxSync(client);
	    FinishLBXRequest(client, REQ_YANKLATE);
	    SaveReplyData(client, (xReply *) & reply, 0, NULL);
	}

#ifdef LBX_STATS
	intern_good++;
#endif

	return Success;
    } else if (nbytes < MAX_ATOM_LENGTH) {
	nr = NewReply(client, X_InternAtom, 0, intern_atom_reply);
	if (!nr)
	    return ProcStandardRequest(client);
	strncpy(nr->request_info.xintern.str, s, nbytes);
	nr->request_info.xintern.str[nbytes] = '\0';
	nr->request_info.xintern.len = nbytes;

#ifdef LBX_STATS
	intern_miss++;
#endif

	return ProcStandardRequest(client);
    }

    return 0;		/* ?!?!? */
}

static Bool
intern_atom_reply(client, nr, data)
    ClientPtr   client;
    ReplyStuffPtr nr;
    char       *data;
{
    Atom        atom;
    char       *str;
    xInternAtomReply *reply;
    int         len;
    char        n;

    reply = (xInternAtomReply *) data;

    str = nr->request_info.xintern.str;
    len = nr->request_info.xintern.len;

    atom = reply->atom;
    if (client->swapped) {
	swapl(&atom, n);
    }
    if (atom != None)
	/* make sure it gets stuffed in the DB */
	(void) LbxMakeAtom(client->server, str, len, atom, TRUE);
    return TRUE;
}

int
ProcLBXGetAtomName(client)
    ClientPtr   client;
{
    REQUEST(xResourceReq);
    char       *str;
    xGetAtomNameReply reply;
    int         len;
    ReplyStuffPtr nr;
    char        n;
    Atom        id;

    id = stuff->id;
    if (client->swapped) {
	swapl(&id, n);
    }
    str = NameForAtom(client->server, id);

    if (str) {	/* found the value */
	len = strlen(str);
	reply.type = X_Reply;
	reply.length = (len + 3) >> 2;
	reply.sequenceNumber = LBXSequenceNumber(client);
	reply.nameLength = len;
	if (client->swapped)
	    SwapGetAtomNameReply(&reply);
	if (LBXCacheSafe(client)) {
	    FinishLBXRequest(client, REQ_YANK);
	    WriteToClient(client, sizeof(xGetAtomNameReply), &reply);
	    WriteToClient(client, len, str);
	} else {
	    if (!LBXCanDelayReply(client))
		SendLbxSync(client);
	    FinishLBXRequest(client, REQ_YANKLATE);
	    SaveReplyData(client, (xReply *) & reply, len, str);
	}

#ifdef LBX_STATS
	getatom_good++;
#endif

	return Success;
    } else {
	nr = NewReply(client, X_GetAtomName, 0, get_atom_name_reply);
	if (!nr)
	    return ProcStandardRequest(client);
	nr->request_info.xgetatom.atom = id;

#ifdef LBX_STATS
	getatom_miss++;
#endif

	return ProcStandardRequest(client);
    }
}

static Bool
get_atom_name_reply(client, nr, data)
    ClientPtr   client;
    ReplyStuffPtr nr;
    char       *data;
{
    Atom        atom;
    char       *s;
    xGetAtomNameReply *reply;
    CARD16	len;
    char	n;

    reply = (xGetAtomNameReply *) data;

    len = reply->nameLength;

    if (client->swapped) {
	swaps(&len, n);
    }

    if (len > MAX_ATOM_LENGTH)
	return FALSE;

    s = data + sizeof(xGetAtomNameReply);

    atom = nr->request_info.xgetatom.atom;
    /* make sure it gets stuffed in the DB */
    if (atom)
	(void) LbxMakeAtom(client->server, s, (unsigned) len, atom, TRUE);
    return TRUE;
}

/*
 * Send an LbxSync request to the server.  After the SyncReply comes
 * back, the proxy will flush any replies it saved for the client.
 */

void
SendLbxSync (client)
    ClientPtr client;
{
    xLbxSyncReq req;
    ReplyStuffPtr nr;

#ifdef SEQ_DEBUG
    fprintf(stderr, "sending LbxSync, seq 0x%x\n", LBXSequenceNumber(client) - 1);
#endif

    ForceSequenceUpdate(client);
    nr = NewReply(client, client->server->lbxReq, X_LbxSync, sync_reply);
    --nr->sequenceNumber; /* executed BEFORE the real request */
    req.reqType = client->server->lbxReq;
    req.lbxReqType = X_LbxSync;
    req.length = sz_xLbxSyncReq >> 2;

    WriteReqToServer (client, sz_xLbxSyncReq, (char *) &req, TRUE);
}

/* ARGSUSED */
static Bool
sync_reply(client, nr, data)
    ClientPtr   client;
    ReplyStuffPtr nr;
    char       *data;
{
    /* do nothing; just here to catch and discard the reply */
    return TRUE;
}

void
WriteError(client, majorCode, minorCode, resId, errorCode)
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

    WriteToClient(client, sizeof(rep), (char *)&rep);
}

int
ProcLBXGetModifierMapping(client)
    ClientPtr   client;
{
    ReplyStuffPtr nr;

    nr = NewReply(client, client->server->lbxReq, X_LbxGetModifierMapping,
		  get_mod_map_reply);
    if (!nr)
	return ProcStandardRequest(client);

    FinishLBXRequest(client, REQ_PASSTHROUGH);
    SendGetModifierMapping(client);
    return Success;
}

/*ARGSUSED*/
static Bool
get_mod_map_reply(client, nr, data)
    ClientPtr   client;
    ReplyStuffPtr nr;
    char       *data;
{
    xLbxGetModifierMappingReply *rep;
    int         len;
    pointer     tag_data;
    CARD32      tag;
    char        n;
    xGetModifierMappingReply reply;

    rep = (xLbxGetModifierMappingReply *) data;

    tag = rep->tag;
    if (client->swapped) {
	swapl(&tag, n);
    }
    len = rep->keyspermod << 3;
    if (rep->tag) {
	if (rep->length) {	/* first time, set tag */

#ifdef LBX_STATS
	    getmodmap_full++;
#endif

	    tag_data = (pointer) &rep[1];
	    if (!TagStoreData(client->server, client->server->global_cache, 
			      tag, len,
			      LbxTagTypeModmap, tag_data)) {
		/* tell server we lost it */
		SendInvalidateTag(client, tag);
	    }
	} else {
	    tag_data = TagGetData(client->server, client->server->global_cache, 
				  tag);
	    if (!tag_data) {
		fprintf(stderr, "no data for mod map tag 0x%lx\n", (long)tag);
		WriteError(client, X_GetModifierMapping, 0, 0, BadAlloc);
		return TRUE;
	    }

#ifdef LBX_STATS
	    getmodmap_tag++;
	    tag_bytes_unsent += (rep->keyspermod << 3);
#endif
	}
    } else {

#ifdef LBX_STATS
	getmodmap_full++;
#endif

	/* server didn't send us a tag for some reason -- just pass on data */
	tag_data = (pointer) &rep[1];
    }

    reply.type = X_Reply;
    reply.numKeyPerModifier = rep->keyspermod;
    reply.sequenceNumber = rep->sequenceNumber;
    reply.length = len >> 2;
    if (client->swapped)
	SwapModmapReply(&reply);
    WriteToClient(client, sizeof(xGetModifierMappingReply), &reply);
    WriteToClient(client, len, tag_data);

    return TRUE;
}

int
ProcLBXGetKeyboardMapping(client)
    ClientPtr   client;
{
    REQUEST(xGetKeyboardMappingReq);
    ReplyStuffPtr nr;

    nr = NewReply(client, client->server->lbxReq, X_LbxGetKeyboardMapping,
		  get_key_map_reply);
    if (!nr)
	return ProcStandardRequest(client);
    nr->request_info.lbxgetkeymap.count = stuff->count;
    nr->request_info.lbxgetkeymap.first = stuff->firstKeyCode;


    FinishLBXRequest(client, REQ_PASSTHROUGH);
    SendGetKeyboardMapping(client);
    return Success;
}

/*
 * always ask for the whole map from server, and send requested subset to
 * client
 */

static Bool
get_key_map_reply(client, nr, data)
    ClientPtr   client;
    ReplyStuffPtr nr;
    char       *data;
{
    xLbxGetKeyboardMappingReply *rep;
    int         len;
    pointer     tag_data;
    CARD32      tag;
    char        n;
    xGetKeyboardMappingReply reply;
    char	*sdata;

    rep = (xLbxGetKeyboardMappingReply *) data;

    tag = rep->tag;
    if (client->swapped) {
	swapl(&tag, n);
    }
    if (tag) {
	if (rep->length) {	/* first time, set tag */

#ifdef LBX_STATS
	    getkeymap_full++;
#endif

	    tag_data = (pointer) &rep[1];
	    len = rep->keysperkeycode *
		(LBXMaxKeyCode(client) - LBXMinKeyCode(client) + 1) * 4;
	    /* data always swapped, because reswapped when written */
	    if (client->swapped)
		SwapLongs((CARD32 *) tag_data, len / 4);
	    if (!TagStoreData(client->server, client->server->global_cache, 
			      tag, len,
			      LbxTagTypeKeymap, tag_data)) {
		/* tell server we lost it */
		SendInvalidateTag(client, tag);
	    }
	} else {
	    tag_data = TagGetData(client->server, 
				  client->server->global_cache, tag);
	    if (!tag_data) {
		fprintf(stderr, "no data for key map tag 0x%lx\n", (long)tag);
		WriteError(client, X_GetKeyboardMapping, 0, 0, BadAlloc);
		return TRUE;
	    }

#ifdef LBX_STATS
	    getkeymap_tag++;
	    tag_bytes_unsent += (rep->keysperkeycode *
		    (LBXMaxKeyCode(client) - LBXMinKeyCode(client) + 1) * 4);
#endif
	}
    } else {

#ifdef LBX_STATS
	getkeymap_full++;
#endif

	/* server didn't send us a tag for some reason -- just pass on data */
	tag_data = (pointer) &rep[1];
	/* data always swapped, because reswapped when written */
	if (client->swapped) {
	    len = rep->keysperkeycode *
		(LBXMaxKeyCode(client) - LBXMinKeyCode(client) + 1) * 4;
	    SwapLongs((CARD32 *) tag_data, len / 4);
	}
    }

    len = (rep->keysperkeycode * nr->request_info.lbxgetkeymap.count) << 2;

    reply.type = X_Reply;
    reply.keySymsPerKeyCode = rep->keysperkeycode;
    reply.sequenceNumber = rep->sequenceNumber;
    reply.length = len >> 2;

    tag_data = (char *)tag_data + (rep->keysperkeycode *
				   (nr->request_info.lbxgetkeymap.first -
				    LBXMinKeyCode(client)));
    if (client->swapped) {
	SwapKeymapReply(&reply);
	/* have to copy data because we could be handed the tag storage */
	sdata = (char *) ALLOCATE_LOCAL(len);
	if (sdata) {
	    memcpy(sdata, tag_data, len);
	    SwapLongs((CARD32 *) sdata, len / 4);
	}
    } else
	sdata = tag_data;
    WriteToClient(client, sizeof(xGetKeyboardMappingReply), &reply);
    WriteToClient(client, len, sdata);
    if (sdata != tag_data)
	DEALLOCATE_LOCAL(sdata);

    return TRUE;
}

int
ProcLBXQueryFont(client)
    ClientPtr   client;
{
    REQUEST(xResourceReq);
    ReplyStuffPtr nr;
    Font        fid;
    char        n;

    fid = stuff->id;
    if (client->swapped) {
	swapl(&fid, n);
    }
    nr = NewReply(client, client->server->lbxReq, X_LbxQueryFont,
		  get_queryfont_reply);
    if (!nr)
	return ProcStandardRequest(client);

    FinishLBXRequest(client, REQ_PASSTHROUGH);
    SendQueryFont(client, fid);
    return Success;
}

static INT16
unpack_val(val, mask, sft, bts)
    CARD32      val;
    CARD32      mask;
    int         sft,
                bts;
{
    CARD16      utmp;
    INT16       sval;

    /* get the proper value */
    utmp = (val & mask) >> sft;
    /* push the sign bit to the right spot */
    utmp <<= (16 - bts);
    /* cast it so sign bit takes effect */
    sval = (INT16) utmp;
    /* shift back down */
    sval >>= (16 - bts);

    return sval;
}

/*ARGSUSED*/
static int
UnsquishFontInfo(compression, fdata, dlen, qfr)
    int	compression;
    xLbxFontInfo	*fdata;
    int		dlen;
    pointer	*qfr;
{
    int         len,
                hlen,
                junklen = sizeof(BYTE) * 2 + sizeof(CARD16) + sizeof(CARD32);
    char *t;
    int         nchars;
    int         i;
    xCharInfo  *maxb;
    xQueryFontReply *new;
    xLbxCharInfo *lci;
    xCharInfo  *ci;
    CARD16       attrs;

    maxb = &fdata->maxBounds;

    nchars = fdata->nCharInfos;
    hlen = sizeof(xQueryFontReply) + fdata->nFontProps * sizeof(xFontProp);
    len = hlen + nchars * sizeof(xCharInfo);

    new = (xQueryFontReply *) xalloc(len);
    *qfr = (pointer) new;

    if (!new)			/* XXX bad stuff... */
	return 0;

    /* copy the header & props parts */
    t = (char *) new;
    t += junklen;
    if (compression) {
	memcpy((char *) t, (char *) fdata, hlen - junklen);
    } else {
	memcpy((char *) t, (char *) fdata, len - junklen);
	return len;
    }

    attrs = maxb->attributes;

    t = (char *) fdata;
    t += hlen - junklen;
    lci = (xLbxCharInfo *) t;

    t = (char *) new;
    t += hlen;
    ci = (xCharInfo *) t;

    /* now expand the chars */
    for (i = 0; i < nchars; i++, lci++, ci++) {
	if (lci->metrics == 0) {
	    /* empty char */
	    ci->characterWidth = ci->leftSideBearing = ci->rightSideBearing =
		ci->ascent = ci->descent = ci->attributes = 0;
	} else {
	    ci->characterWidth =
	        unpack_val(lci->metrics, LBX_WIDTH_MASK,
		       LBX_WIDTH_SHIFT, LBX_WIDTH_BITS);
	    ci->leftSideBearing = unpack_val(lci->metrics, LBX_LEFT_MASK,
					 LBX_LEFT_SHIFT, LBX_LEFT_BITS);
	    ci->rightSideBearing = unpack_val(lci->metrics, LBX_RIGHT_MASK,
					  LBX_RIGHT_SHIFT, LBX_RIGHT_BITS);
	    ci->ascent = unpack_val(lci->metrics, LBX_ASCENT_MASK,
				LBX_ASCENT_SHIFT, LBX_ASCENT_BITS);
	    ci->descent = unpack_val(lci->metrics, LBX_DESCENT_MASK,
				 LBX_DESCENT_SHIFT, LBX_DESCENT_BITS);
	    ci->attributes = attrs;
	}
    }

    return len;
}

/*ARGSUSED*/
static Bool
get_queryfont_reply(client, nr, data)
    ClientPtr   client;
    ReplyStuffPtr nr;
    char       *data;
{
    xLbxQueryFontReply *rep;
    int         len,
                sqlen;
    pointer     tag_data,
                sqtag_data;
    TagData     td;
    CARD32      tag;
    char        n;
    xQueryFontReply *reply;

    rep = (xLbxQueryFontReply *) data;

    tag = rep->tag;
    if (client->swapped) {
	swapl(&tag, n);
    }
    if (tag) {
	if (rep->length) {	/* first time, set tag */

#ifdef LBX_STATS
	    queryfont_full++;
#endif

	    sqtag_data = (pointer) &rep[1];
	    sqlen = rep->length << 2;
	    if (client->swapped)
		LbxSwapFontInfo(sqtag_data, rep->compression);

	    /*
	     * store squished version of data
	     */
	    if (!TagStoreData(client->server, client->server->global_cache, 
				      tag, sqlen,
				      LbxTagTypeFont, sqtag_data)) {
		/* tell server we lost it */
		SendInvalidateTag(client, tag);
	    }
	    len = UnsquishFontInfo(rep->compression, sqtag_data, sqlen,
				   &tag_data);
	} else {
	    td = TagGetTag(client->server, client->server->global_cache, tag);
	    if (!td) {
		fprintf(stderr, "no data for font tag 0x%lx\n", (long)tag);
		WriteError(client, X_QueryFont, 0, 0, BadAlloc);
		return TRUE;
	    }
	    sqlen = td->size;
	    sqtag_data = td->tdata;
	    len = UnsquishFontInfo(rep->compression, sqtag_data, sqlen,
						&tag_data);

#ifdef LBX_STATS
	    queryfont_tag++;
	    tag_bytes_unsent += len;
#endif
	}
    } else {

#ifdef LBX_STATS
	queryfont_full++;
#endif

	/* server didn't send us a tag for some reason -- just pass on data */
	sqtag_data = (pointer) &rep[1];
	if (client->swapped)
	    LbxSwapFontInfo(sqtag_data, FALSE);
	sqlen = rep->length << 2;
	len = UnsquishFontInfo(rep->compression, sqtag_data, sqlen, &tag_data);
    }

    reply = (xQueryFontReply *) ALLOCATE_LOCAL(len);
    if (reply) {
	memcpy((char *) reply, (char *) tag_data, len);
	/* patch up certain fields */
	reply->type = X_Reply;
	reply->sequenceNumber = rep->sequenceNumber;
	reply->length = (len - sizeof(xGenericReply)) >> 2;

	len -= sizeof(xQueryFontReply);
	if (client->swapped)
	    SwapFont(reply, TRUE);
	WriteToClient(client, sizeof(xQueryFontReply), reply);
	WriteToClient(client, len, reply + 1);
	DEALLOCATE_LOCAL(reply);
    }

    xfree(tag_data);		/* free unsquished version */

    return TRUE;
}


int
ProcLBXGetWindowAttributes(client)
    ClientPtr   client;
{
    REQUEST(xResourceReq);
    xLbxGetWinAttrAndGeomReq newreq;
    XServerPtr server = client->server;
    ReplyStuffPtr nr;

    /*
     * GetWindowAttributes is always followed by GetGeometry.
     * At this point, if lbxproxy requests the window attributes
     * *AND* the geometry in one request, we are guaranteed to be
     * able to short circuit the GetGeometry that follows.
     */

    nr = NewReply(client, client->server->lbxReq, X_LbxGetWinAttrAndGeom,
		  GetWinAttrAndGeomReply);

    if (!nr)
	return ProcStandardRequest(client);

    /*
     * R6 Xlib will not wait for the GetWindowAttributes reply before
     * sending the GetGeometry.  As a result, when lbxproxy gets the
     * GetGeometry we must defer short circuting until lbxproxy receives
     * the LbxGetWinAttrAndGeomReply.
     */

    nr->request_info.lbxWinGeom.got_geom_request = FALSE;
    nr->request_info.lbxWinGeom.got_geom_reply = FALSE;


    FinishLBXRequest(client, REQ_PASSTHROUGH);

    newreq.reqType = server->lbxReq;
    newreq.lbxReqType = X_LbxGetWinAttrAndGeom;
    newreq.length = sz_xLbxGetWinAttrAndGeomReq >> 2;
    newreq.id = stuff->id;

    WriteReqToServer (client, sz_xLbxGetWinAttrAndGeomReq, (char *) &newreq, TRUE);
    return Success;
}


static Bool
GetWinAttrAndGeomReply (client, nr, data)
    ClientPtr   client;
    ReplyStuffPtr nr;
    char       *data;
{
    xLbxGetWinAttrAndGeomReply *lbxrep;
    xGetWindowAttributesReply reply;
    char n;

    /*
     * We got the window attributes and geometry.  Write the
     * WindowAttributes reply now.
     */

    lbxrep = (xLbxGetWinAttrAndGeomReply *) data;

    reply.type = X_Reply;
    reply.sequenceNumber = lbxrep->sequenceNumber;
    reply.length = (sizeof(xGetWindowAttributesReply) -
	sizeof(xGenericReply)) >> 2;
    reply.backingStore = lbxrep->backingStore;
    reply.visualID = lbxrep->visualID;
#if defined(__cplusplus) || defined(c_plusplus)
    reply.c_class = lbxrep->c_class;
#else
    reply.class = lbxrep->class;
#endif
    reply.bitGravity = lbxrep->bitGravity;
    reply.winGravity = lbxrep->winGravity;
    reply.backingBitPlanes = lbxrep->backingBitPlanes;
    reply.backingPixel = lbxrep->backingPixel;
    reply.saveUnder = lbxrep->saveUnder;
    reply.mapInstalled = lbxrep->mapInstalled;
    reply.mapState = lbxrep->mapState;
    reply.override = lbxrep->override;
    reply.colormap = lbxrep->colormap;
    reply.allEventMasks = lbxrep->allEventMasks;
    reply.yourEventMask = lbxrep->yourEventMask;
    reply.doNotPropagateMask = lbxrep->doNotPropagateMask;

    if (client->swapped) {
	swaps (&reply.sequenceNumber, n);
	swapl (&reply.length, n);
    }

    WriteToClient(client, sizeof(xGetWindowAttributesReply), &reply);

    if (nr->request_info.lbxWinGeom.got_geom_request)
    {
	/*
	 * We can write the GetGeometry reply now too.
	 */

	xGetGeometryReply geomReply;

	geomReply.type = X_Reply;
	geomReply.sequenceNumber = lbxrep->sequenceNumber + 1;
	geomReply.length = 0;
	geomReply.depth = lbxrep->depth;
	geomReply.root = lbxrep->root;
	geomReply.x = lbxrep->x;
	geomReply.y = lbxrep->y;
	geomReply.width = lbxrep->width;
	geomReply.height = lbxrep->height;
	geomReply.borderWidth = lbxrep->borderWidth;

	if (client->swapped) {
	    swaps (&geomReply.sequenceNumber, n);
	}

	WriteToClient(client, sizeof(xGetGeometryReply), &geomReply);

	LBXLastResponse(client) = lbxrep->sequenceNumber + 1;
 
	return TRUE; /* remove matching reply record */
    }
    else
    {
	/*
	 * We didn't get the GetGeometry request yet, so we
	 * stash away the geometry.
	 */

	nr->request_info.lbxWinGeom.got_geom_reply = TRUE;
	nr->request_info.lbxWinGeom.depth = lbxrep->depth;
	nr->request_info.lbxWinGeom.root = lbxrep->root;
	nr->request_info.lbxWinGeom.x = lbxrep->x;
	nr->request_info.lbxWinGeom.y = lbxrep->y;
	nr->request_info.lbxWinGeom.width = lbxrep->width;
	nr->request_info.lbxWinGeom.height = lbxrep->height;
	nr->request_info.lbxWinGeom.borderWidth = lbxrep->borderWidth;

	return FALSE; /* don't remove matching reply record */
    }
}


int
ProcLBXGetGeometry(client)
    ClientPtr   client;
{
    /* REQUEST(xResourceReq); */
    xGetGeometryReply reply;
    ReplyStuffPtr nr;
    char n;

    /*
     * If the previous request was GetWindowAttributes, then lbxproxy
     * requested the window attributes *AND* the geometry in one request.
     * That means we can short circuit this GetGeometry!
     */

    nr = GetMatchingReply (client, LBXSequenceNumber(client) - 1, FALSE);

    if (!nr || nr->major != client->server->lbxReq ||
	nr->minor != X_LbxGetWinAttrAndGeom)
    {
	/*
	 * This GetGeometry was not preceded by a GetWindowAttributes,
	 * so we can't short circuit.  Send the normal GetGeometry request.
	 */

	return ProcStandardRequest(client);
    }

    if (nr->request_info.lbxWinGeom.got_geom_reply)
    {
	/*
	 * We already have the geom information, so we
	 * can write the reply now.
	 */

	reply.type = X_Reply;
	reply.sequenceNumber = LBXSequenceNumber(client);
	reply.length = 0;
	reply.depth = nr->request_info.lbxWinGeom.depth;
	reply.root = nr->request_info.lbxWinGeom.root;
	reply.x = nr->request_info.lbxWinGeom.x;
	reply.y = nr->request_info.lbxWinGeom.y;
	reply.width = nr->request_info.lbxWinGeom.width;
	reply.height = nr->request_info.lbxWinGeom.height;
	reply.borderWidth = nr->request_info.lbxWinGeom.borderWidth;

	if (client->swapped) {
	    swaps (&reply.sequenceNumber, n);
	}

	FinishLBXRequest(client, REQ_YANK);
	WriteToClient(client, sizeof(xGetGeometryReply), &reply);
	RemoveReply (client, nr);
    }
    else
    {
	/*
	 * We must wait for the geom information before
	 * writing the GetGeometry reply.
	 */
      
	nr->request_info.lbxWinGeom.got_geom_request = TRUE;

	FinishLBXRequest(client, REQ_YANKLATE);
    }
    return Success;
}

/* We're just looking for signs of the window manager here */
int
ProcLBXChangeWindowAttributes(client)
    ClientPtr client;
{
    REQUEST(xChangeWindowAttributesReq);
    Window win;
    CARD32 mask;
    char n;

    win = stuff->window;
    mask = stuff->valueMask;
    if (client->swapped) {
	swapl(&win, n);
	swapl(&mask, n);
    }
    if (mask == CWEventMask && win == LBXRootWindow(client)) {
	mask = *(CARD32 *)&stuff[1];
	if (client->swapped) {
	    swapl(&mask, n);
	}
	if (mask & SubstructureRedirectMask)
	    client->server->wm_running = TRUE;
    }
    return ProcStandardRequest(client);
}

void
FinishLBXRequest(client, yank)
    ClientPtr   client;
    int         yank;
{
    REQUEST(xReq);
    char        n;

    if (yank != REQ_PASSTHROUGH) {
	LBXLastForReply(client) = LBXSequenceNumber(client);
	LBXLastForResponse(client) = LBXSequenceNumber(client);
	if (yank == REQ_YANK || yank == REQ_REPLACE)
	    LBXLastResponse(client) = LBXSequenceNumber(client);
    } else {
	switch (GeneratesReplies(client, stuff)) {
	case REQ_TYPE_YES:
	    LBXLastForReply(client) = LBXSequenceNumber(client);
	    LBXLastForResponse(client) = LBXSequenceNumber(client);
	    break;
	case REQ_TYPE_MAYBE:
	    LBXLastForResponse(client) = LBXSequenceNumber(client);
	    break;
	case REQ_TYPE_NO:
	    if (protocolMode != PROTOCOL_POOR &&
		GeneratesEvents(client, stuff))
		LBXLastForResponse(client) = LBXSequenceNumber(client);
	    break;
	}
    }
    switch (protocolMode) {
    case PROTOCOL_FULL:
	LBXCacheSafe(client) = (LBXSequenceNumber(client) ==
				LBXLastResponse(client));
	LBXCanDelayReply(client) = (LBXSequenceNumber(client) ==
				    LBXLastForReply(client));
	break;
    default:
	LBXCacheSafe(client) = (LBXLastForResponse(client) <=
				LBXLastResponse(client));
	LBXCanDelayReply(client) = (LBXLastForResponse(client) <=
				    LBXLastForReply(client));
	break;
    }
#ifdef SEQ_DEBUG
    fprintf(stderr,
	    "finished req %d, seq 0x%x, yank %d cache %d delay %d reply 0x%x response 0x%x\n",
	    stuff->reqType, LBXSequenceNumber(client), yank,
	    LBXCacheSafe(client), LBXCanDelayReply(client),
	    LBXLastForReply(client), LBXLastForResponse(client));
#endif

    if (yank == REQ_YANK || yank == REQ_YANKLATE) {
	LBXSequenceLost(client)++;
	LBXYanked(client)++;
	DBG(DBG_CLIENT, (stderr, "short-circuited client %d req %d\n",
			 client->index, stuff->reqType));
    }
    /* make sure server's sequence number is accurate */
    if (yank != REQ_YANK && yank != REQ_YANKLATE && LBXSequenceLost(client))
	BumpSequence(client);

    /* put request length in proxy byte order */
    if (client->swapped) {
	swaps(&stuff->length, n);
    }
}

/*
 * need to rewrite error codes for requests we've replaced.
 *
 * QueryFont regularly hits this in normal operation
 */
static int
patchup_error(client, err, nr)
    ClientPtr   client;
    xError     *err;
    ReplyStuffPtr nr;
{
    int         retval = 1;
    CARD16      minor_code;
    char        n;

    minor_code = err->minorCode;
    if (client->swapped) {
	swaps(&minor_code, n);
    }
    switch (minor_code) {
    case X_LbxGetModifierMapping:
	minor_code = X_GetModifierMapping;
	break;
    case X_LbxGetKeyboardMapping:
	minor_code = X_GetKeyboardMapping;
	break;
    case X_LbxGetProperty:
	minor_code = X_GetProperty;
	break;
    case X_LbxQueryFont:
	minor_code = X_QueryFont;
	break;
    case X_LbxGetWinAttrAndGeom:
	minor_code = X_GetWindowAttributes;
	if (nr->request_info.lbxWinGeom.got_geom_request) {
	    err->majorCode = minor_code;
	    err->minorCode = 0;
	    if (client->swapped) {
		swaps(&err->sequenceNumber, n);
	    }
	    WriteToClient (client, sizeof(xError), (char *)err);
	    if (client->swapped) {
		swaps(&err->sequenceNumber, n);
	    }
	    err->sequenceNumber++;
	    LBXLastResponse(client) = err->sequenceNumber;
	    minor_code = X_GetGeometry;
	}
	break;
    default:
	retval = 0;		/* error caused by some LBX req that shouldn't
				 * have an error, so eat it */
	break;
    }
    err->majorCode = minor_code;    /* err->majorCode is CARD8, don't swap */
    err->minorCode = 0;
    return retval;
}

static Bool
error_matches(client, nr, err)
    ClientPtr   client;
    ReplyStuffPtr nr;
    xError     *err;
{
    CARD16      mc;
    char        n;

    mc = err->minorCode;
    if (client->swapped) {
	swaps(&mc, n);
    }
    return (err->majorCode == nr->major && mc == nr->minor);
}

/*
 * returns TRUE if data (possibly modified) is to be sent on to client,
 * FALSE if data is replaced
 */
static Bool
HandleReply(client, data, len)
    ClientPtr   client;
    char       *data;
    int         len;
{
    xGenericReply *reply;
    xError     *err;
    ReplyStuffPtr nr;
    Bool        ret = TRUE;
    char        n;

    reply = (xGenericReply *) data;

    if (client->awaitingSetup) {
	xConnSetupPrefix *prefix = (xConnSetupPrefix *) reply;

	AttendClient(client);
	if (prefix->success) {
	    get_setup_reply(client, (char *) reply, len);
	    return FALSE;
	}
	return TRUE;
    }
    if (client->swapped && reply->type != KeymapNotify) {
	swaps(&reply->sequenceNumber, n);
    }

    if (reply->type != KeymapNotify &&
	reply->sequenceNumber < LBXLastResponse(client)) {
	/* spontaneous events, and events in PROTOCOL_POOR mode,
	 * may have sequence numbers that are earlier
	 * than short-circuited replies and errors */
	if (reply->type > X_Reply)
	    reply->sequenceNumber = LBXLastResponse(client);
    }

    if (reply->type != KeymapNotify &&
	reply->sequenceNumber > LBXSequenceNumber(client))
    {
#ifdef SEQ_DEBUG
	fprintf(stderr, "lbxproxy: reply seq #0x%x > internal seq 0x%x\n",
		reply->sequenceNumber, LBXSequenceNumber(client));
#endif
	reply->sequenceNumber = LBXSequenceNumber(client);
    }

    if (reply->type != X_Reply) {	/* event or error */
	/* clear out pending replies that resulted in errors */
	if (reply->type == X_Error) {
	    LBXLastResponse(client) = reply->sequenceNumber;
	    err = (xError *) reply;
	    nr = GetMatchingReply(client, reply->sequenceNumber, TRUE);
	    if (nr && error_matches(client, nr, err)) {
		if (err->majorCode == client->server->lbxReq) {
		    int         eret;

		    if ((eret = patchup_error(client, err, nr)) < 0) {
			CloseDownClient(client);
			return FALSE;
		    } else if (eret == 0) {
			/* error for proxy -- eat it */
			ret = FALSE;
		    }
		} else {
		    /* error in core X or other extension */
		    HandleExtensionError(client, err, nr);
		}
	    }
	    if (nr)
		RemoveReply(client, nr);
	} else if (reply->type >= 64)
	    HandleExtensionEvent(client, (xEvent *)reply);
	if (client->swapped && reply->type != KeymapNotify) {
	    swaps(&reply->sequenceNumber, n);
	}
	return ret;
    }

    LBXLastResponse(client) = reply->sequenceNumber;

    nr = GetMatchingReply(client, reply->sequenceNumber, TRUE);
    if (nr) {
	if (nr->major == client->server->lbxReq)
	    ret = FALSE;
	if ((*nr->reply_func)(client, nr, (char *)reply))
	    RemoveReply(client, nr);
    }

    if (client->swapped) {	/* put seq & length back */
	swaps(&reply->sequenceNumber, n);
	swapl(&reply->length, n);
    }
    return ret;
}

void
DoLBXReply(client, data, len)
    ClientPtr   client;
    char       *data;
    int         len;
{
    if (HandleReply(client, data, len))
	WriteToClient (client, len, data);

    FlushDelayedReplies (client);

    switch (protocolMode) {
    case PROTOCOL_FULL:
	LBXCacheSafe(client) = (LBXSequenceNumber(client) ==
				LBXLastResponse(client));
	break;
    default:
	LBXCacheSafe(client) = (LBXLastForResponse(client) <=
				LBXLastResponse(client));
	break;
    }
#ifdef SEQ_DEBUG
    fprintf(stderr, "finished reply 0x%x, cache %d\n",
	    ((xGenericReply *)data)->sequenceNumber, LBXCacheSafe(client));
#endif
}
