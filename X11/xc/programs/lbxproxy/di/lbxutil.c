/* $Xorg: lbxutil.c,v 1.4 2000/08/17 19:53:55 cpqbld Exp $ */
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
/* $XFree86: xc/programs/lbxproxy/di/lbxutil.c,v 1.5 2001/10/28 03:34:22 tsi Exp $ */

/*
 * utility routines for LBX requests
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
#include	"colormap.h"

Bool compStats = FALSE;		/* report stream compression statistics */

#ifdef DEBUG
extern int lbxDebug;
#endif

extern int lbxMaxMotionEvents;

ReplyStuffPtr
NewReply(client, major, minor, reply_func)
    ClientPtr   client;
    int		major;
    int		minor;
    ReplyFunc	reply_func;
{
    ReplyStuffPtr new,
                *end;

    new = (ReplyStuffPtr) xalloc(sizeof(ReplyStuffRec));
    if (!new)
	return new;
    new->sequenceNumber = LBXSequenceNumber(client);
    new->major = major;
    new->minor = minor;
    new->reply_func = reply_func;
    new->next = NULL;
    end = &LBXReplyList(client);
    while (*end)
	end = &(*end)->next;
    *end = new;
    return new;
}

void
RemoveReply(client, rp)
    ClientPtr   client;
    ReplyStuffPtr rp;
{
    ReplyStuffPtr cur, *prev;

    prev = &LBXReplyList(client);

    while ((cur = *prev) != rp)
	prev = &cur->next;
    *prev = cur->next;
    if (cur->major == client->server->lbxReq) {
	if (CacheTrimNeeded(client->server, client->server->global_cache) &&
	    !AnyTagBearingReplies(client->server, client->server->global_cache))
	    CacheTrim(client->server, client->server->global_cache);
	if (CacheTrimNeeded(client->server, client->server->prop_cache) &&
	    !AnyTagBearingReplies(client->server, client->server->prop_cache))
	    CacheTrim(client->server, client->server->prop_cache);
    }
    xfree(cur);
}

ReplyStuffPtr
GetMatchingReply(client, seqno, flush_older)
    ClientPtr   client;
    int         seqno;
    Bool	flush_older;
{
    ReplyStuffPtr t, old;

    seqno &= 0xffff;
    for (t = LBXReplyList(client); t; t = t->next) {
	if ((t->sequenceNumber & 0xffff) == seqno)
	    break;
    }
#ifdef SEQ_DEBUG
    if (t)
	fprintf(stderr, "matched reply for seq 0x%x\n", seqno);
    else
	fprintf(stderr, "no reply for seq 0x%x\n", seqno);
#endif
    if (t && flush_older) {
	while ((old = LBXReplyList(client)) != t) {
	    fprintf(stderr,
		    "unclaimed reply: maj %d min %d seq 0x%x curseq 0x%x\n",
		    old->major, old->minor, old->sequenceNumber, seqno);
	    LBXReplyList(client) = old->next;
	    xfree(old);
	}
    }
    return t;
}

Bool
AnyReplies(client)
    ClientPtr   client;
{
    return (LBXReplyList(client) != NULL);
}

Bool
AnyTagBearingReplies(server, cache)
    XServerPtr server;
    Cache cache;
{
    int i;
    int lbxreq;
    ReplyStuffPtr t;
    Bool found = FALSE;

    /* assume this is called while a reply is being processed, so need two */
    for (i = 1; i < currentMaxClients; i++)
    {
        if (!clients[i])
	    continue;
	lbxreq = clients[i]->server->lbxReq;
	for (t = LBXReplyList(clients[i]); t; t = t->next) {
	    if (t->major == lbxreq) {
		switch (t->minor) {
		case X_LbxGetModifierMapping:
		case X_LbxGetKeyboardMapping:
		case X_LbxQueryFont:
		    if (cache == server->global_cache) {
			if (found)
			    return TRUE;
			found = TRUE;
		    }
		    break;
		case X_LbxGetProperty:
		    if (cache == server->prop_cache) {
			if (found)
			    return TRUE;
			found = TRUE;
		    }
		    break;
		}
	    }
	}
    }
    return FALSE;
}

/*
 * this is used for stashing short-circuited replies for later.
 * it currently assumes that all of them will be 32 bytes for the reply
 * plus some amount of extra data
 */

Bool
SaveReplyData(client, rep, len, data)
    ClientPtr   client;
    xReply     *rep;
    int		len;
    pointer     data;
{
    ReplyDataPtr new, *end;

    new = (ReplyDataPtr) xalloc(sizeof(ReplyDataRec));
    if (!new)
	return FALSE;
    if (len) {
	new->data = (pointer) xalloc(len);
	if (!new->data) {
	    xfree(new);
	    return FALSE;
	} else {
	    memcpy((char *) new->data, (char *) data, len);
	}
    }
    new->reply = *rep;
    new->dlen = len;
    new->delay_seq_no = LBXSequenceNumber(client);
    new->next = NULL;

    end = &LBXReplyData(client);
    while (*end)
	end = &(*end)->next;
    *end = new;
#ifdef SEQ_DEBUG
    fprintf(stderr, "saving reply seq 0x%x\n", LBXSequenceNumber(client));
#endif
    return TRUE;
}

Bool
FlushDelayedReplies(client)
    ClientPtr   client;
{
    ReplyDataPtr *prev, cur;

#ifdef SEQ_DEBUG
    fprintf(stderr, "flushing replies seq 0x%x:", LBXLastResponse(client));
#endif
    for (prev = &LBXReplyData(client); (cur = *prev); ) {
#ifdef SEQ_DEBUG
	fprintf(stderr, " 0x%x", cur->delay_seq_no);
#endif
	if ((cur->delay_seq_no & 0xffff) == LBXLastResponse(client) + 1) {
	    WriteToClient(client, sizeof(xReply), (char *) &cur->reply);
	    if (cur->dlen)
		WriteToClient(client, cur->dlen, (char *) cur->data);
	    LBXLastResponse(client) = cur->delay_seq_no;
	    *prev = cur->next;
	    xfree(cur);
	}
	else
	    prev = &cur->next;
    }
#ifdef SEQ_DEBUG
    fprintf(stderr, "\n");
#endif
    return TRUE;
}

void
BumpSequence(client)
    ClientPtr   client;
{
    DBG(DBG_CLIENT, (stderr, "bumping client %d sequence by %d to %d\n",
	 client->index, LBXSequenceLost(client), LBXSequenceNumber(client)));
    ModifySequence(client, LBXSequenceLost(client));
    LBXSequenceLost(client) = 0;
}

void
ForceSequenceUpdate(client)
    ClientPtr   client;
{
    if (LBXSequenceLost(client)) {
	BumpSequence(client);
    }
}

void
LbxFreeTag(server, tag, tagtype)
    XServerPtr	server;
    XID         tag;
    int         tagtype;

{
    Cache       tag_cache;

    switch (tagtype) {
    case LbxTagTypeProperty:
	tag_cache = server->prop_cache;
	break;
    case LbxTagTypeFont:
    case LbxTagTypeModmap:
    case LbxTagTypeKeymap:
    case LbxTagTypeConnInfo:
	tag_cache = server->global_cache;
	break;
    default:
	fprintf(stderr,
		"unknown type in InvalidateTag request: tag 0x%lx type %d\n",
		(long)tag, tagtype);
	return;
    }
    TagFreeData(server, tag_cache, tag, TRUE);
}

void
LbxSendTagData(client, tag, tagtype)
    ClientPtr   client;
    XID         tag;
    int         tagtype;
{
    TagData     td;
    unsigned long len;
    pointer     tdata;
    PropertyTagDataPtr ptdp;

    if (tagtype == LbxTagTypeProperty && (td = TagGetTag(client->server, 
		client->server->prop_cache, tag))) {
	ptdp = (PropertyTagDataPtr) td->tdata;
	tdata = ptdp->data;
	len = ptdp->length;
    } else {
	fprintf(stderr, "invalid SendTagData request: tag 0x%lx type %d\n",
		(long)tag, tagtype);
	len = 0;
	tdata = NULL;
    }
    SendTagData(client, tag, len, tdata);
}

extern unsigned long  stream_out_compressed;
extern unsigned long  stream_out_uncompressed;
extern unsigned long  stream_out_plain;
extern unsigned long  stream_in_compressed;
extern unsigned long  stream_in_uncompressed;
extern unsigned long  stream_in_plain;
extern unsigned long  raw_stream_out;
extern unsigned long  raw_stream_in;

void
DumpCompressionStats()
{
    if (raw_stream_out && stream_out_plain) {
	fprintf(stderr, "Requests:  normal = %ld, reencoded = %ld",
		(long)raw_stream_out, (long)stream_out_plain);
	stream_out_compressed += stream_out_uncompressed;
	if (stream_out_compressed)
	    fprintf(stderr, ", compressed = %ld", (long)stream_out_compressed);
	else
	    stream_out_compressed = stream_out_plain;
	fprintf(stderr, "\n           %.2f:1 overall reduction ratio\n",
		(float)raw_stream_out / (float)stream_out_compressed);
    }
    if (raw_stream_in && stream_in_plain) {
	fprintf(stderr, "Responses: normal = %ld, reencoded = %ld",
		(long)raw_stream_in, (long)stream_in_plain);
	stream_in_compressed += stream_in_uncompressed;
	if (stream_in_compressed)
	    fprintf(stderr, ", compressed = %ld", (long)stream_in_compressed);
	else
	    stream_in_compressed = stream_in_plain;
	fprintf(stderr, "\n           %.2f:1 overall reduction ratio\n",
		(float)raw_stream_in / (float)stream_in_compressed);
    }
}

void
ZeroCompressionStats()
{
    stream_out_compressed = 0;
    stream_out_uncompressed = 0;
    stream_out_plain = 0;
    stream_in_compressed = 0;
    stream_in_uncompressed = 0;
    stream_in_plain = 0;
    raw_stream_out = 0;
    raw_stream_in = 0;
}



#ifdef LBX_STATS
int         intern_good,
            intern_miss;
int         getatom_good,
            getatom_miss;
int         luc_good,
            luc_miss;
int         ac_good,
            ac_miss;
int         anc_good,
            anc_miss;

int         getmodmap_tag,	/* tag only */
            getmodmap_full;
int         getkeymap_tag,	/* tag only */
            getkeymap_full;
int         queryfont_tag,	/* tag only */
            queryfont_full;
int         getsetup_tag,	/* tag only */
            getsetup_full;

int         getprop_tag,
            getprop_full;


int         tag_bytes_unsent;	/* approx data kept off wire by tags */

int         delta_out_total;
int         delta_out_attempts;
int         delta_out_hits;
int         delta_in_total;
int         delta_in_attempts;
int         delta_in_hits;

extern int	    gfx_gc_hit;
extern int	    gfx_gc_miss;
extern int	    gfx_draw_hit;
extern int	    gfx_draw_miss;
extern int	    gfx_total;

void
DumpOtherStats()
{
    fprintf(stderr, "Short-circuit stats\n");
    fprintf(stderr, "InternAtom cache hits %d misses %d\n", intern_good, intern_miss);
    fprintf(stderr, "GetAtomName cache hits %d misses %d\n", getatom_good, getatom_miss);
    fprintf(stderr, "LookupColor cache hits %d misses %d\n", luc_good, luc_miss);
    fprintf(stderr, "AllocColor cache hits %d misses %d\n", ac_good, ac_miss);
    fprintf(stderr, "AllocNamedColor cache hits %d misses %d\n", anc_good, anc_miss);

    fprintf(stderr, "Tag stats\n");
    fprintf(stderr, "GetModifierMapping used tag %d, full data %d\n", getmodmap_tag, getmodmap_full);
    fprintf(stderr, "GetKeyboardMapping used tag %d, full data %d\n", getkeymap_tag, getkeymap_full);
    fprintf(stderr, "QueryFont used tag %d, full data %d\n", queryfont_tag, queryfont_full);
    fprintf(stderr, "GetProperty used tag %d, full data %d\n", getprop_tag, getprop_full);
    fprintf(stderr, "ConnectionSetup used tag %d, full data %d\n", getsetup_tag, getsetup_full);

    fprintf(stderr, "Approx bytes kept off wire by tags %d\n", tag_bytes_unsent);

    fprintf(stderr, "Delta Compressor stats\n");
    fprintf(stderr, "Sent: total msgs = %d, cacheable = %d, cache hits = %d\n",
	    delta_out_total, delta_out_attempts, delta_out_hits);
    fprintf(stderr, "Received: total = %d, cacheable = %d, cache hits = %d\n",
	    delta_in_total, delta_in_attempts, delta_in_hits);

    fprintf(stderr, "GFX Cache stats\n");
    fprintf(stderr, "Reencoded = %d\n", gfx_total);
#define percent(s,t)	((t) ? ((s) * 100) / (t) : 0)
    
#define ratios(h,m)	(h), percent (h, (h)+(m)), (m), percent (m, (h) + (m))
    fprintf(stderr, "Draw hit = %d (%d%%) miss = %d (%d%%) GC hit = %d (%d%%) miss = %d (%d%%)\n",
	    ratios (gfx_draw_hit, gfx_draw_miss), 
	    ratios (gfx_gc_hit, gfx_gc_miss));
#define savings(h,m)	(((h) + (m)) * 4) - ((h) + (m) * 5)
    fprintf(stderr, "Total bytes saved = %d Draw = %d GC = %d\n",
	    savings (gfx_gc_hit + gfx_draw_hit, gfx_gc_miss + gfx_draw_miss),
	    savings (gfx_draw_hit, gfx_draw_miss),
	    savings (gfx_gc_hit, gfx_gc_miss));
}

void
ZeroOtherStats()
{
    intern_good = intern_miss = 0;
    getatom_good = getatom_miss = 0;
    luc_good = luc_miss = 0;
    ac_good = ac_miss = 0;
    anc_good = anc_miss = 0;

    getmodmap_tag = 0;
    getmodmap_full = 0;
    getkeymap_tag = 0;
    getkeymap_full = 0;
    getprop_tag = 0;
    getprop_full = 0;
    getsetup_tag = 0;
    getsetup_full = 0;

    delta_out_total = delta_out_attempts = delta_out_hits = 0;
    delta_in_total = delta_in_attempts = delta_in_hits = 0;

    gfx_gc_hit = 0;
    gfx_gc_miss = 0;
    gfx_draw_hit = 0;
    gfx_draw_miss = 0;
    gfx_total = 0;
}

#endif

void
SendInitLBXPackets(server)
    XServerPtr server;
{

    ZeroCompressionStats();
#ifdef LBX_STATS
    ZeroOtherStats();
#endif

    AllowMotion(server->serverClient, lbxMaxMotionEvents);
}

void
LbxCleanupSession()
{
    if (compStats)
    {
	DumpCompressionStats();
	ZeroCompressionStats();
    }

#ifdef LBX_STATS
    DumpOtherStats();
    ZeroOtherStats();
#endif
}
