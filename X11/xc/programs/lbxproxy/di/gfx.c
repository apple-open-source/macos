/* $Xorg: gfx.c,v 1.4 2001/02/09 02:05:31 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/lbxproxy/di/gfx.c,v 1.6 2001/12/14 20:00:51 dawes Exp $ */

/*
 * graphics acceleration
 */

#include	<stdio.h>
#include	"assert.h"
#include	"misc.h"
#include	"lbx.h"
#include	"util.h"
#include	"resource.h"
#include	"wire.h"
#include        "lbximage.h"
#include	"proxyopts.h"
#include	"swap.h"

static Bool GetLbxImageReply();

static int  pad[4] = {0, 3, 2, 1};

/*
 * Routines for re-encoding line, rectangle and arc requests
 */

#define REENCODE_PSHORT(val, out) \
    if ((val) >= 0 && (val) < 0xf0) \
	*(out)++ = (val); \
    else if ((val) >= 0xef0 || (val) < (short)0xfe00) \
	return 0; \
    else if ((val) < 0) { \
	*(out)++ = 0xf0 | (((val) + 0x1000) >> 8); \
	*(out)++ = (val) + 0x1000; \
    } else { \
	*(out)++ = 0xf0 | (((val) - 0xf0) >> 8); \
	*(out)++ = (val) - 0xf0; \
    }


#define REENCODE_SHORT(val, out) \
    if ((val) >= (short)0xff90 && (val) < 0x80) \
	*(out)++ = (val); \
    else if ((val) >= (short)0x0880 || (val) < (short)0xf790) \
	return 0; \
    else if ((val) < 0) { \
	*(out)++ = 0x80 | ((((val) + 0x70) >> 8) & 0x0f); \
	*(out)++ = (val) + 0x70; \
    } else { \
	*(out)++ = 0x80 | ((((val) - 0x80) >> 8) & 0x0f); \
	*(out)++ = (val) - 0x80; \
    }

#define REENCODE_USHORT(val, out) \
    if ((val) < 0xf0) \
	*(out)++ = (val); \
    else if ((unsigned short)(val) >= 0x10f0) \
	return 0; \
    else { \
	*(out)++ = 0xf0 | (((val) - 0xf0) >> 8); \
	*(out)++ = (val) - 0xf0; \
    }

#define REENCODE_ANGLE(val, out) \
    if ((val) > (360 << 6) || (val) < -(360 << 6)) \
	return 0; \
    else if ((val) >= 0 && (val) <= (95 << 6) && !((val) % (5 << 6))) \
	*(out)++ = 0x5a + ((val) / (5 << 6)); \
    else if ((val) >= (105 << 6) && !((val) % (15 << 6))) \
	*(out)++ = 0x67 + ((val) / (15 << 6)); \
    else if ((val) >= -(100 << 6) && (val) <= -(5 << 6) && \
	     !((val) % (5 << 6))) \
	*(out)++ = 0xa6 + ((val) / (5 << 6)); \
    else if ((val) <= -(105 << 6) && !((val) % (15 << 6))) \
	*(out)++ = 0x98 + ((val) / (15 << 6)); \
    else { \
	*(out)++ = (val) >> 8; \
	*(out)++ = (val); \
    }

#define COPY_AND_SWAPS(oval, ival) \
    { \
	register int n; \
	(oval) = (ival); \
	if (client->swapped) \
	    swaps(&(oval), n); \
    }

#define SWAP_REENCODE_PSHORT(ival, out) { \
    register int    _n; \
    short	    _val; \
    _val = (ival); \
    if (client->swapped) swaps (&_val, _n); \
    REENCODE_PSHORT(_val, out); \
}

#define SWAP_REENCODE_SHORT(ival, out) { \
    register int    _n; \
    short	    _val; \
    _val = (ival); \
    if (client->swapped) swaps (&_val, _n); \
    REENCODE_SHORT(_val, out); \
}

#define SWAP_REENCODE_USHORT(ival, out) { \
    register int    _n; \
    unsigned short  _val; \
    _val = (ival); \
    if (client->swapped) swaps (&_val, _n); \
    REENCODE_USHORT(_val, out); \
}

int gfx_gc_hit;
int gfx_gc_miss;
int gfx_draw_hit;
int gfx_draw_miss;
int gfx_total;

static void
push(cache, xid)
    XID         cache[GFX_CACHE_SIZE];
    XID         xid;
{
    memmove(cache + 1, cache, (GFX_CACHE_SIZE - 1) * sizeof(cache[0]));
    cache[0] = xid;
}

static void
use(cache, i)
    XID         cache[GFX_CACHE_SIZE];
    int         i;
{
    XID         tmp;

    if (i == 0)
	return;
    tmp = cache[i];
    memmove(cache + 1, cache, i * sizeof(cache[0]));
    cache[0] = tmp;
}

static int
match(cache, xid)
    XID         cache[GFX_CACHE_SIZE];
    XID         xid;
{
    int         j;

    for (j = 0; j < GFX_CACHE_SIZE; j++)
	if (cache[j] == xid)
	    return j;
    return GFXCacheNone;
}

#define GFX_REQ_PAD	8

#define GFX_SRC_DST_REQ_PAD	12

#define GFX_CACHE_DECLARE   int		_dcache, _gcache; \
			    Drawable	_drawable; \
			    GContext	_gcontext

#define GFX_SRC_DST_CACHE_DECLARE	int _srcDcache, _dstDcache, _gcache; \
					Drawable _srcDrawable, _dstDrawable; \
					GContext _gcontext

#define GFX_SETUP_DRAWABLE_AND_GC(after) {\
    _drawable = stuff->drawable; \
    _gcontext = stuff->gc; \
    if (client->swapped) { \
	register int _n; \
	swapl(&_drawable, _n); \
	swapl(&_gcontext, _n); \
    } \
    _dcache = match (LBXDrawableCache(client), _drawable); \
    if (_dcache == GFXCacheNone) \
    { \
	memcpy (after, &stuff->drawable, 4); \
	after += 4; \
    } \
    _gcache = match (LBXGContextCache(client), _gcontext); \
    if (_gcache == GFXCacheNone) \
    { \
	memcpy (after, &stuff->gc, 4); \
	after += 4; \
    } \
}

#define GFX_SETUP_SRC_DST_DRAWABLE_AND_GC(after) {\
    Drawable	tmpDrawableCache[GFX_CACHE_SIZE]; \
    memcpy (tmpDrawableCache, LBXDrawableCache(client), sizeof (LBXDrawableCache(client))); \
    _srcDrawable = stuff->srcDrawable; \
    _dstDrawable = stuff->dstDrawable; \
    _gcontext = stuff->gc; \
    if (client->swapped) { \
	register int _n; \
	swapl (&_srcDrawable, _n); \
	swapl (&_dstDrawable, _n); \
	swapl (&_gcontext, _n); \
    } \
    _srcDcache = match (LBXDrawableCache(client), _srcDrawable); \
    if (_srcDcache == GFXCacheNone) \
    { \
	memcpy (after, &stuff->srcDrawable, 4); \
	after += 4; \
	push (tmpDrawableCache, _srcDrawable); \
    } else \
	use (tmpDrawableCache, _srcDcache); \
    _dstDcache = match (tmpDrawableCache, _dstDrawable); \
    if (_dstDcache == GFXCacheNone) \
    { \
	memcpy (after, &stuff->dstDrawable, 4); \
	after += 4; \
    } \
    _gcache = match (LBXGContextCache(client), _gcontext); \
    if (_gcache == GFXCacheNone) \
    { \
	memcpy (after, &stuff->gc, 4); \
	after += 4; \
    } \
}

#define GFX_SET_DRAWABLE_AND_GC {\
    if (_dcache == GFXCacheNone) { \
	gfx_draw_miss++; \
	push (LBXDrawableCache(client), _drawable); \
    } else { \
	gfx_draw_hit++; \
	use (LBXDrawableCache(client), _dcache); \
    } \
    if (_gcache == GFXCacheNone) { \
	gfx_gc_miss++; \
	push (LBXGContextCache(client), _gcontext);\
    } else { \
	gfx_gc_hit++; \
	use (LBXGContextCache(client), _gcache); \
    } \
    newreq->cacheEnts = GFXCacheEnts (_dcache, _gcache); \
    gfx_total++; \
}
#define GFX_SET_SRC_DST_DRAWABLE_AND_GC {\
    if (_srcDcache == GFXCacheNone) { \
	gfx_draw_miss++; \
	push (LBXDrawableCache(client), _srcDrawable); \
    } else { \
	gfx_draw_hit++; \
	use (LBXDrawableCache(client), _srcDcache); \
    } \
    newreq->srcCache = GFXCacheEnts (_srcDcache, 0); \
    if (_dstDcache == GFXCacheNone) { \
	gfx_draw_miss++; \
	push (LBXDrawableCache(client), _dstDrawable); \
    } else { \
	gfx_draw_hit++; \
	use (LBXDrawableCache(client), _dstDcache); \
    } \
    if (_gcache == GFXCacheNone) { \
	gfx_gc_miss++; \
	push (LBXGContextCache(client), _gcontext);\
    } else { \
	gfx_gc_hit++; \
	use (LBXGContextCache(client), _gcache); \
    } \
    newreq->cacheEnts = GFXCacheEnts (_dstDcache, _gcache); \
    gfx_total++; \
}

static int
reencode_poly(client, lbxreq, reencode_rtn)
    ClientPtr   client;
    CARD8       lbxreq;
    int         (*reencode_rtn) ();
{
    REQUEST(xPolyPointReq);
    XServerPtr  server = client->server;
    int         len = client->req_len << 2;
    int         space;
    xLbxPolyPointReq *newreq = NULL;
    char       *after;
    int         bytes;

    GFX_CACHE_DECLARE;

    space = len - sz_xPolyPointReq + sz_xLbxPolyPointReq + GFX_REQ_PAD + 3;
    newreq = (xLbxPolyPointReq *) xalloc(space);
    if (newreq == NULL)
	goto bail;
    after = ((char *) newreq) + sz_xLbxPolyPointReq;
    GFX_SETUP_DRAWABLE_AND_GC(after);
    bytes = (*reencode_rtn) (client, (short *) &stuff[1],
			     (char *) after,
			     (len - sz_xPolyPointReq) >> 2);
    if (bytes == 0)
	goto bail;
    FinishLBXRequest(client, REQ_PASSTHROUGH);
    newreq->reqType = server->lbxReq;
    newreq->lbxReqType = lbxreq;
    GFX_SET_DRAWABLE_AND_GC;
    len = after - ((char *) newreq) + bytes;
    newreq->padBytes = pad[len & 3];
    bzero(((char *) newreq) + len, newreq->padBytes);
    len += newreq->padBytes;
    newreq->length = len >> 2;
    WriteReqToServer(client, len, (char *) newreq, TRUE);
    xfree(newreq);
    return Success;
bail:
    if (newreq)
	xfree(newreq);
    return ProcStandardRequest(client);

}


/* ARGSUSED */
static int
reencode_points_origin(client, in, out, count)
    ClientPtr   client;
    register short *in;
    register char *out;
    int         count;
{
    register short diff;
    int         i;
    short       inval;
    short       last_x = 0;
    short       last_y = 0;
    char       *start_out = out;

    for (i = 0; i < count; i++, in += 2) {
	COPY_AND_SWAPS(inval, in[0]);
	diff = inval - last_x;
	last_x = inval;
	REENCODE_SHORT(diff, out);
	COPY_AND_SWAPS(inval, in[1]);
	diff = inval - last_y;
	last_y = inval;
	REENCODE_SHORT(diff, out);
    }
    return out - start_out;
}

/* ARGSUSED */
static int
reencode_points_previous(client, in, out, count)
    ClientPtr   client;
    register short *in;
    register char *out;
    int         count;
{
    int         i;
    short       inval;
    char       *start_out = out;

    for (i = 0; i < count; i++, in += 2) {
	COPY_AND_SWAPS(inval, in[0]);
	REENCODE_SHORT(inval, out);
	COPY_AND_SWAPS(inval, in[1]);
	REENCODE_SHORT(inval, out);
    }
    return out - start_out;
}

/* ARGSUSED */
static int
reencode_segment(client, in, out, count)
    ClientPtr   client;
    register short *in;
    register char *out;
    int         count;
{
    register short diff;
    int         i;
    short       inval;
    short       last_x = 0;
    short       last_y = 0;
    char       *start_out = out;

    count >>= 1;
    for (i = 0; i < count; i++, in += 4) {
	/* reencode (x1, y1) */
	COPY_AND_SWAPS(inval, in[0]);
	diff = inval - last_x;
	last_x = inval;
	REENCODE_SHORT(diff, out);
	COPY_AND_SWAPS(inval, in[1]);
	diff = inval - last_y;
	last_y = inval;
	REENCODE_SHORT(diff, out);

	/* reencode (x2, y2) */
	COPY_AND_SWAPS(inval, in[2]);
	diff = inval - last_x;
	REENCODE_SHORT(diff, out);
	COPY_AND_SWAPS(inval, in[3]);
	diff = inval - last_y;
	REENCODE_SHORT(diff, out);
    }
    return out - start_out;
}

/* ARGSUSED */
static int
reencode_rectangle(client, in, out, count)
    ClientPtr   client;
    register short *in;
    register char *out;
    int         count;
{
    register short diff;
    int         i;
    short       inval;
    short       last_x = 0;
    short       last_y = 0;
    char       *start_out = out;

    count >>= 1;
    for (i = 0; i < count; i++, in += 4) {
	/* reencode (x, y) origin */
	COPY_AND_SWAPS(inval, in[0]);
	diff = inval - last_x;
	last_x = inval;
	REENCODE_SHORT(diff, out);
	COPY_AND_SWAPS(inval, in[1]);
	diff = inval - last_y;
	last_y = inval;
	REENCODE_SHORT(diff, out);

	/* reencode (width, height) */
	COPY_AND_SWAPS(inval, in[2]);
	REENCODE_USHORT(inval, out);
	COPY_AND_SWAPS(inval, in[3]);
	REENCODE_USHORT(inval, out);
    }
    return out - start_out;
}

/* ARGSUSED */
static int
reencode_arc(client, in, out, count)
    ClientPtr   client;
    register short *in;
    register char *out;
    int         count;
{
    register short diff;
    int         i;
    short       inval;
    short       last_x = 0;
    short       last_y = 0;
    char       *start_out = out;

    count = count / 3;
    for (i = 0; i < count; i++, in += 6) {
	/* reencode (x, y) origin */
	COPY_AND_SWAPS(inval, in[0]);
	diff = inval - last_x;
	last_x = inval;
	REENCODE_SHORT(diff, out);
	COPY_AND_SWAPS(inval, in[1]);
	diff = inval - last_y;
	last_y = inval;
	REENCODE_SHORT(diff, out);

	/* reencode (width, height) */
	COPY_AND_SWAPS(inval, in[2]);
	REENCODE_USHORT(inval, out);
	COPY_AND_SWAPS(inval, in[3]);
	REENCODE_USHORT(inval, out);

	/* reencode (angle1, angle2) */
	COPY_AND_SWAPS(inval, in[4]);
	REENCODE_ANGLE(inval, out);
	COPY_AND_SWAPS(inval, in[5]);
	REENCODE_ANGLE(inval, out);
    }
    return out - start_out;
}

int
ProcLBXPolyPoint(client)
    ClientPtr   client;
{
    REQUEST(xPolyPointReq);
    int         (*reencode_rtn) ();

    reencode_rtn = (stuff->coordMode) ? reencode_points_previous :
	reencode_points_origin;
    return reencode_poly(client, X_LbxPolyPoint, reencode_rtn);
}

int
ProcLBXPolyLine(client)
    ClientPtr   client;
{
    REQUEST(xPolyLineReq);
    int         (*reencode_rtn) ();

    reencode_rtn = (stuff->coordMode) ? reencode_points_previous :
	reencode_points_origin;
    return reencode_poly(client, X_LbxPolyLine, reencode_rtn);
}

int
ProcLBXPolySegment(client)
    ClientPtr   client;
{
    return reencode_poly(client, X_LbxPolySegment, reencode_segment);
}

int
ProcLBXPolyRectangle(client)
    ClientPtr   client;
{
    return reencode_poly(client, X_LbxPolyRectangle, reencode_rectangle);
}

int
ProcLBXPolyArc(client)
    ClientPtr   client;
{
    return reencode_poly(client, X_LbxPolyArc, reencode_arc);
}

int
ProcLBXFillPoly(client)
    ClientPtr   client;
{
    REQUEST(xFillPolyReq);
    XServerPtr  server = client->server;
    int         len = client->req_len << 2;
    xLbxFillPolyReq *newreq = NULL;
    char       *after;
    int         bytes;
    int         space;
    int         (*reencode_rtn) ();

    GFX_CACHE_DECLARE;

    reencode_rtn = (stuff->coordMode) ? reencode_points_previous :
	reencode_points_origin;

    space = len - sz_xFillPolyReq + sz_xLbxPolyPointReq + GFX_REQ_PAD + 3;
    newreq = (xLbxFillPolyReq *) xalloc(space);
    if (newreq == NULL)
	goto bail;
    after = ((char *) newreq) + sz_xLbxFillPolyReq;
    GFX_SETUP_DRAWABLE_AND_GC(after);
    bytes = (*reencode_rtn) (client, (short *) &stuff[1], after,
			     (len - sz_xFillPolyReq) >> 2);
    if (bytes == 0)
	goto bail;

    FinishLBXRequest(client, REQ_PASSTHROUGH);
    newreq->reqType = server->lbxReq;
    newreq->lbxReqType = X_LbxFillPoly;
    newreq->shape = stuff->shape;
    GFX_SET_DRAWABLE_AND_GC;
    len = after - ((char *) newreq) + bytes;
    newreq->padBytes = pad[len & 3];
    bzero(((char *) newreq) + len, newreq->padBytes);
    len += newreq->padBytes;
    newreq->length = len >> 2;
    WriteReqToServer(client, len, (char *) newreq, TRUE);
    xfree(newreq);
    return Success;
bail:
    if (newreq)
	xfree(newreq);
    return ProcStandardRequest(client);
}

int
ProcLBXPolyFillRectangle(client)
    ClientPtr   client;
{
    return reencode_poly(client, X_LbxPolyFillRectangle, reencode_rectangle);
}

int
ProcLBXPolyFillArc(client)
    ClientPtr   client;
{
    return reencode_poly(client, X_LbxPolyFillArc, reencode_arc);
}

static int
reencode_copy(client, in, out)
    ClientPtr   client;
    register xCopyAreaReq *in;
    register char *out;
{
    char       *start_out = out;

    SWAP_REENCODE_PSHORT(in->srcX, out);
    SWAP_REENCODE_PSHORT(in->srcY, out);
    SWAP_REENCODE_PSHORT(in->dstX, out);
    SWAP_REENCODE_PSHORT(in->dstY, out);
    SWAP_REENCODE_USHORT(in->width, out);
    SWAP_REENCODE_USHORT(in->height, out);
    return out - start_out;
}

int
ProcLBXCopyArea(client)
    ClientPtr   client;
{
    REQUEST(xCopyAreaReq);
    XServerPtr  server = client->server;
    int         len;
    xLbxCopyAreaReq *newreq = NULL;
    unsigned long space[(SIZEOF(xLbxCopyAreaReq) + GFX_SRC_DST_REQ_PAD + 7 * sizeof(unsigned long) - 1) / sizeof(unsigned long)];
    char       *after;
    int         extra;
    int         bytes;

    GFX_SRC_DST_CACHE_DECLARE;

    newreq = (xLbxCopyAreaReq *) space;
    after = ((char *) newreq) + sz_xLbxCopyAreaReq;
    GFX_SETUP_SRC_DST_DRAWABLE_AND_GC(after);
    bytes = reencode_copy(client, stuff, after);
    if (bytes == 0)
	goto bail;
    FinishLBXRequest(client, REQ_PASSTHROUGH);
    newreq->reqType = server->lbxReq;
    newreq->lbxReqType = X_LbxCopyArea;
    GFX_SET_SRC_DST_DRAWABLE_AND_GC;
    len = ((after + bytes) - ((char *) newreq));
    extra = pad[len & 3];
    bzero(((char *) newreq) + len, extra);
    len += extra;
    newreq->length = len >> 2;
    WriteReqToServer(client, len, (char *) newreq, TRUE);
    return Success;
bail:
    return ProcStandardRequest(client);
}

int
ProcLBXCopyPlane(client)
    ClientPtr   client;
{
    REQUEST(xCopyPlaneReq);
    XServerPtr  server = client->server;
    int         len;
    xLbxCopyPlaneReq *newreq = NULL;
    unsigned long space[(SIZEOF(xLbxCopyPlaneReq) + GFX_SRC_DST_REQ_PAD + 7 * sizeof(unsigned long) - 1) / sizeof(unsigned long)];
    char       *after;
    int         bytes;
    int         extra;

    GFX_SRC_DST_CACHE_DECLARE;

    newreq = (xLbxCopyPlaneReq *) space;
    after = ((char *) newreq) + sz_xLbxCopyPlaneReq;
    GFX_SETUP_SRC_DST_DRAWABLE_AND_GC(after);
    bytes = reencode_copy(client, (xCopyAreaReq *)stuff, after);
    if (bytes == 0)
	goto bail;
    FinishLBXRequest(client, REQ_PASSTHROUGH);
    newreq->reqType = server->lbxReq;
    newreq->bitPlane = stuff->bitPlane;
    newreq->lbxReqType = X_LbxCopyPlane;
    GFX_SET_SRC_DST_DRAWABLE_AND_GC;
    len = ((after + bytes) - ((char *) newreq));
    extra = pad[len & 3];
    bzero(((char *) newreq) + len, extra);
    len += extra;
    newreq->length = len >> 2;
    WriteReqToServer(client, len, (char *) newreq, TRUE);
    return Success;
bail:
    return ProcStandardRequest(client);
}

static int
reencode_text_pos(client, in, out)
    ClientPtr   client;
    xPolyTextReq *in;
    char       *out;
{
    char       *start_out;

    start_out = out;
    SWAP_REENCODE_PSHORT(in->x, out);
    SWAP_REENCODE_PSHORT(in->y, out);
    return out - start_out;
}

int
ProcLBXPolyText(client)
    ClientPtr   client;
{
    REQUEST(xPolyTextReq);
    XServerPtr  server = client->server;
    int         len = client->req_len << 2;
    xLbxPolyTextReq *newreq = NULL;
    int         space;
    char       *after;
    int         bytes;
    int         extra;

    GFX_CACHE_DECLARE;

    space = len - sz_xPolyTextReq + sz_xLbxPolyTextReq + GFX_REQ_PAD + 4 + 3;
    newreq = (xLbxPolyTextReq *) xalloc(space);
    if (newreq == NULL)
	goto bail;
    after = ((char *) newreq) + sz_xLbxPolyTextReq;
    GFX_SETUP_DRAWABLE_AND_GC(after);
    /* Reencode the text position */
    bytes = reencode_text_pos(client, stuff, after);
    if (bytes == 0)
	goto bail;
    /* copy the text elements */
    memcpy(after + bytes, (char *) &stuff[1], len - sz_xPolyTextReq);
    bytes += len - sz_xPolyTextReq;
    FinishLBXRequest(client, REQ_PASSTHROUGH);
    newreq->reqType = server->lbxReq;
    newreq->lbxReqType = stuff->reqType == X_PolyText8 ? X_LbxPolyText8 : X_LbxPolyText16;
    GFX_SET_DRAWABLE_AND_GC;
    len = ((after + bytes) - ((char *) newreq));
    extra = pad[len & 3];
    bzero(((char *) newreq) + len, extra);
    len += extra;
    newreq->length = len >> 2;
    WriteReqToServer(client, len, (char *) newreq, TRUE);
    xfree(newreq);
    return Success;
bail:
    if (newreq)
	xfree(newreq);
    return ProcStandardRequest(client);
}

int
ProcLBXImageText(client)
    ClientPtr   client;
{
    REQUEST(xImageTextReq);
    XServerPtr  server = client->server;
    int         len = client->req_len << 2;
    xLbxImageTextReq *newreq = NULL;
    int         space;
    char       *after;
    int         bytes;
    int         extra;

    GFX_CACHE_DECLARE;

    space = len - sz_xImageTextReq + sz_xLbxImageTextReq + GFX_REQ_PAD + 4 + 3;
    newreq = (xLbxImageTextReq *) xalloc(space);
    if (newreq == NULL)
	goto bail;
    after = ((char *) newreq) + sz_xLbxImageTextReq;
    GFX_SETUP_DRAWABLE_AND_GC(after);
    /* Reencode the text position */
    bytes = reencode_text_pos(client, (xPolyTextReq *) stuff, after);
    if (bytes == 0)
	goto bail;
    /* copy the text elements */
    memcpy(after + bytes, (char *) &stuff[1], len - sz_xImageTextReq);
    bytes += len - sz_xImageTextReq;
    FinishLBXRequest(client, REQ_PASSTHROUGH);
    newreq->reqType = server->lbxReq;
    newreq->lbxReqType = stuff->reqType == X_ImageText8 ? X_LbxImageText8 : X_LbxImageText16;
    newreq->nChars = stuff->nChars;
    GFX_SET_DRAWABLE_AND_GC;
    len = ((after + bytes) - ((char *) newreq));
    extra = pad[len & 3];
    bzero(((char *) newreq) + len, extra);
    len += extra;
    newreq->length = len >> 2;
    WriteReqToServer(client, len, (char *) newreq, TRUE);
    xfree(newreq);
    return Success;
bail:
    if (newreq)
	xfree(newreq);
    return ProcStandardRequest(client);
}

static void
SwapXPutImage(req)
    xPutImageReq *req;
{
    char        n;

    swaps(&req->width, n);
    swaps(&req->height, n);
    swaps(&req->dstX, n);
    swaps(&req->dstY, n);
}

static char *
reencode_putimage(stuff, newreq)
    xPutImageReq *stuff;
    xLbxPutImageReq *newreq;
{
    char *out;

    if (stuff->format > ZPixmap || stuff->leftPad > 31 ||
	!stuff->depth || stuff->depth > 32 ||
	(stuff->format == ZPixmap && stuff->leftPad != 0))
	return 0;

    out = (char *)newreq + sz_xLbxPutImageReq;
    if (!stuff->leftPad && stuff->depth <= 8)
	newreq->bitPacked = 0x80 | (stuff->format << 5) |
	    		    ((stuff->depth - 1) << 2);
    else {
	newreq->bitPacked = ((stuff->depth - 1) << 2);
	*out++ = (stuff->format << 5) | stuff->leftPad;
    }
    REENCODE_USHORT(stuff->width, out);
    REENCODE_USHORT(stuff->height, out);
    REENCODE_PSHORT(stuff->dstX, out);
    REENCODE_PSHORT(stuff->dstY, out);

    return out;
}

#define MaxPutImageSz sz_xLbxPutImageReq + 1 + 2 + 2 + 2 + 2 + GFX_REQ_PAD

int
ProcLBXPutImage(client)
    ClientPtr   client;
{
    REQUEST(xPutImageReq);
    XServerPtr  server = client->server;
    unsigned long space[(MaxPutImageSz + sizeof(unsigned long) - 1) / sizeof(unsigned long)];
    xLbxPutImageReq *newreq;
    char	*data = NULL;
    char	*out;
    int         len,
		reqlen,
                method = 0,
                compBytes,
                status;
#ifdef LBX_STATS
    float       percentCompression;
    char	*methodName;
#endif
    LbxBitmapCompMethod *bcompMethod = NULL;
    LbxPixmapCompMethod *pcompMethod = NULL;
    GFX_CACHE_DECLARE;

    status = LBX_IMAGE_COMPRESS_SUCCESS;
    if (stuff->format != ZPixmap ||
	(stuff->depth == 1 && LBXZBitsPerPixel1(client) == 1))
    {
	bcompMethod = LbxFindPreferredBitmapCompMethod (client->server);
	if (!bcompMethod)
	    status = LBX_IMAGE_COMPRESS_NO_SUPPORT;
    } else {
	pcompMethod = LbxFindPreferredPixmapCompMethod (client->server,
	    (int) stuff->format, (int) stuff->depth);
	if (!pcompMethod)
	    status = LBX_IMAGE_COMPRESS_NO_SUPPORT;
    }

    if (status == LBX_IMAGE_COMPRESS_NO_SUPPORT &&
	client->req_len >= 64 + (sz_xPutImageReq >> 2))
	return ProcStandardRequest(client);

    if (client->swapped)
	SwapXPutImage(stuff);

    newreq = (xLbxPutImageReq *)space;
    newreq->reqType = server->lbxReq;
    newreq->lbxReqType = X_LbxPutImage;
    out = reencode_putimage(stuff, newreq);

    if (!out) {
	SwapXPutImage(stuff);
	return ProcStandardRequest(client);
    }

    GFX_SETUP_DRAWABLE_AND_GC(out);

    reqlen = out - (char *)space;
    len = client->req_len << 2;

    data = (char *) xalloc(len);
    if (!data || status == LBX_IMAGE_COMPRESS_NO_SUPPORT)
    {
	if (!data)
	    status = LBX_IMAGE_COMPRESS_BAD_MALLOC;
    }
    else if (stuff->format != ZPixmap ||
	     (stuff->depth == 1 && LBXZBitsPerPixel1(client) == 1))
    {
	int ppl, bpl, nbytes;

	if (!bcompMethod->inited)
	{
	    if (bcompMethod->compInit)
		(*bcompMethod->compInit)();
	    bcompMethod->inited = 1;
	}

	ppl = stuff->width + stuff->leftPad;
	if (LBXBitmapBitOrder(client) != LBXImageByteOrder(client))
	    ppl = ((ppl + LBXBitmapScanlineUnit(client) - 1) &
		   ~LBXBitmapScanlineUnit(client));
	bpl = (((ppl + LBXBitmapScanlinePad(client) - 1) /
		LBXBitmapScanlinePad(client)) *
	       (LBXBitmapScanlinePad(client) >> 3));
	nbytes = bpl * stuff->height;
	if (stuff->format == XYPixmap)
	    nbytes *= stuff->depth;
	status = (*bcompMethod->compFunc) (
	    (unsigned char *) &stuff[1],
	    (unsigned char *) data + reqlen,
	    len - reqlen,
	    nbytes,
	    ppl,
	    bpl,
	    LBXBitmapBitOrder(client) == LSBFirst,
	    &compBytes);

	method = bcompMethod->methodOpCode;
#ifdef LBX_STATS
	methodName = bcompMethod->methodName;
#endif
    }
    else
    {
	if (!pcompMethod->inited)
	{
	    if (pcompMethod->compInit)
		(*pcompMethod->compInit)();
	    pcompMethod->inited = 1;
	}

	status = (*pcompMethod->compFunc) (
	    (char *) &stuff[1],
	    (char *) data + reqlen,
	    len - reqlen,
	    (int) stuff->format,
	    (int) stuff->depth,
	    (int) stuff->height,
	    (int) (len - sz_xPutImageReq) / stuff->height,
	    &compBytes);

	method = pcompMethod->methodOpCode;
#ifdef LBX_STATS
	methodName = pcompMethod->methodName;
#endif
    }

#ifdef LBX_STATS
    fprintf(stderr, "PutImage (xid=%x, w=%d, h=%d, x=%d, y=%d): ",
	    stuff->drawable, stuff->width, stuff->height,
	    stuff->dstX, stuff->dstY);
#endif

    if (status != LBX_IMAGE_COMPRESS_SUCCESS)
    {
#ifdef LBX_STATS
	fprintf(stderr, "image not compressed - ");

	if (status == LBX_IMAGE_COMPRESS_NO_SUPPORT)
	    fprintf(stderr, "no support for compressing this image\n");
	else if (status == LBX_IMAGE_COMPRESS_NOT_WORTH_IT)
	    fprintf(stderr, "not worth it\n");
	else if (status == LBX_IMAGE_COMPRESS_BAD_MALLOC)
	    fprintf(stderr, "bad malloc\n");
#endif

	if (!data || len >= 256 || reqlen > sz_xPutImageReq) {
	    if (data)
		xfree(data);

	    if (client->swapped)	/* Swap it back */
		SwapXPutImage(stuff);

	    return ProcStandardRequest(client);
	}
	method = 0;
	compBytes = len - sz_xPutImageReq;
	memcpy(data + reqlen, (char *) &stuff[1], compBytes);
    }

    GFX_SET_DRAWABLE_AND_GC;

    newreq->compressionMethod = method;

    len = reqlen + compBytes;
    newreq->bitPacked |= pad[len & 3];
    len += pad[len & 3];
    newreq->length = len >> 2;
    memcpy(data, (char *)newreq, reqlen);

    FinishLBXRequest(client, REQ_PASSTHROUGH);

#ifdef LBX_STATS
    percentCompression = 100.0 * (1.0 -
				  ((float) (compBytes + pad[compBytes & 3]) /
			  (float) ((client->req_len << 2) - sz_xPutImageReq)));

    fprintf(stderr, "%f percent compression (%s)\n",
	percentCompression, methodName);
#endif

    /*
     * If the compressed image is greater that 25% of the original
     * image, or is small enough to fit into the delta cache, then
     * run the PutImage request through the regular stream
     * compressor.  Otherwise, just write the compressed image.
     */

    if (newreq->length > (client->req_len >> 2) ||
	DELTA_CACHEABLE(&server->outdeltas, len))
	WriteReqToServer(client, len, data, TRUE);
    else
	WriteToServerUncompressed(client, len, data, TRUE);

    xfree(data);
    return Success;
}


int
ProcLBXGetImage(client)
    ClientPtr   client;
{
    REQUEST(xGetImageReq);
    ReplyStuffPtr nr;

    nr = NewReply(client, client->server->lbxReq, X_LbxGetImage,
		  GetLbxImageReply);

    if (!nr)
	return ProcStandardRequest(client);

    if (client->swapped) {
	char        n;

	swapl(&stuff->drawable, n);
	swaps(&stuff->x, n);
	swaps(&stuff->y, n);
	swaps(&stuff->width, n);
	swaps(&stuff->height, n);
	swapl(&stuff->planeMask, n);
    }
    nr->request_info.lbxgetimage.width = stuff->width;
    nr->request_info.lbxgetimage.height = stuff->height;
    nr->request_info.lbxgetimage.format = stuff->format;

#ifdef LBX_STATS
    nr->request_info.lbxgetimage.drawable = stuff->drawable;
    nr->request_info.lbxgetimage.x = stuff->x;
    nr->request_info.lbxgetimage.y = stuff->y;
#endif

    FinishLBXRequest(client, REQ_PASSTHROUGH);
    SendGetImage(client, stuff->drawable, stuff->x, stuff->y,
		 stuff->width, stuff->height, stuff->planeMask,
		 stuff->format);
    return Success;
}

static Bool
GetLbxImageReply(client, nr, data)
    ClientPtr   client;
    ReplyStuffPtr nr;
    char       *data;
{
    xLbxGetImageReply *rep;
    xGetImageReply reply;
    pointer     imageData;
    int         freeIt = 1;
#ifdef LBX_STATS
    char *methodName;
#endif

    rep = (xLbxGetImageReply *) data;

    if (client->swapped) {
	char        n;

	swapl(&rep->xLength, n);
	swapl(&rep->visual, n);
    }
    /* +1 is because fillspan in DecodeFaxG42D seems to go 1 byte too far,
     * and I don't want to mess with that code */
    if ((imageData = (pointer) xalloc((rep->xLength << 2) + 1)) == NULL)
	return FALSE;

    if (rep->compressionMethod == LbxImageCompressNone)
    {
	xfree((char *) imageData);
	imageData = (pointer) &rep[1];
	freeIt = 0;
    }
    else if (nr->request_info.lbxgetimage.format != ZPixmap ||
	     (rep->depth == 1 && LBXZBitsPerPixel1(client) == 1))
    {
	LbxBitmapCompMethod *compMethod;

	compMethod = LbxLookupBitmapCompMethod (client->server,
						rep->compressionMethod);

	if (!compMethod)
	{
	    xfree((char *) imageData);
	    return FALSE;
	}
	else
	{
	    int ppl, bpl, nbytes;

	    if (!compMethod->inited)
	    {
		if (compMethod->compInit)
		    (*compMethod->compInit)();
		compMethod->inited = 1;
	    }

	    ppl = nr->request_info.lbxgetimage.width;
	    if (LBXBitmapBitOrder(client) != LBXImageByteOrder(client))
		ppl = ((ppl + LBXBitmapScanlineUnit(client) - 1) &
		       ~LBXBitmapScanlineUnit(client));
	    bpl = (((ppl + LBXBitmapScanlinePad(client) - 1) /
		   LBXBitmapScanlinePad(client)) *
		   (LBXBitmapScanlinePad(client) >> 3));
	    nbytes = bpl * nr->request_info.lbxgetimage.height;
	    if (nr->request_info.lbxgetimage.format == XYPixmap)
		nbytes *= rep->depth;

	    (*compMethod->decompFunc) (
		(unsigned char *) &rep[1],
		(unsigned char *) imageData,
		nbytes,
		ppl,
		bpl,
		LBXBitmapBitOrder(client) == LSBFirst);

#ifdef LBX_STATS
	    methodName = compMethod->methodName;
#endif
	}
    }
    else
    {
	LbxPixmapCompMethod *compMethod;

	compMethod = LbxLookupPixmapCompMethod (client->server, 
						rep->compressionMethod);

	if (!compMethod)
	{
	    xfree((char *) imageData);
	    return FALSE;
	}
	else
	{
	    if (!compMethod->inited)
	    {
		if (compMethod->compInit)
		    (*compMethod->compInit)();
		compMethod->inited = 1;
	    }

	    (*compMethod->decompFunc) (
		(char *) &rep[1],
		(char *) imageData,
		(int) nr->request_info.lbxgetimage.height,
		(rep->xLength << 2) / nr->request_info.lbxgetimage.height);

#ifdef LBX_STATS
	    methodName = compMethod->methodName;
#endif
	}
    }

    reply.type = X_Reply;
    reply.depth = rep->depth;
    reply.sequenceNumber = rep->sequenceNumber;
    reply.length = rep->xLength;
    reply.visual = rep->visual;

    if (client->swapped)
	SwapGetImageReply(&reply);

#ifdef LBX_STATS
    fprintf (stderr, "GetImage (xid=%x, w=%d, h=%d, x=%d, y=%d): ",
			nr->request_info.lbxgetimage.drawable,
			nr->request_info.lbxgetimage.width,
			nr->request_info.lbxgetimage.height,
			nr->request_info.lbxgetimage.x,
			nr->request_info.lbxgetimage.y);
    if (rep->compressionMethod == LbxImageCompressNone) {
	fprintf(stderr, "image not compressed\n");
    } else {
	float       percentCompression = 100.0 * (1.0 -
			    ((float) rep->lbxLength / (float) rep->xLength));

	fprintf(stderr, "%f percent compression (%s)\n",
            percentCompression, methodName);
    }
#endif

    WriteToClient(client, sizeof(xGetImageReply), &reply);
    WriteToClient(client, rep->xLength << 2, imageData);

    if (freeIt)
	xfree((char *) imageData);

    return TRUE;
}
