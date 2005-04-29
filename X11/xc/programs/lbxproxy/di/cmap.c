/* $Xorg: cmap.c,v 1.5 2001/02/09 02:05:31 xorgcvs Exp $ */

/*
Copyright 1996, 1998  The Open Group

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
/* $XFree86: xc/programs/lbxproxy/di/cmap.c,v 1.7 2003/11/17 22:20:48 dawes Exp $ */

#include	<stdio.h>
#include	"assert.h"
#include	"misc.h"
#include	"lbx.h"
#include	"atomcache.h"
#include	"util.h"
#include	"tags.h"
#include	"colormap.h"
#include	"resource.h"
#include	"wire.h"
#include	"swap.h"
#include	"reqtype.h"
#include	"lbxext.h"

static void LocalAllocColor ();
static void FoundPixel ();
static Bool grab_cmap_reply();
static Bool alloc_named_color_reply();
static Bool alloc_color_cells_reply();
static Bool alloc_color_planes_reply();
static Bool lookup_color_reply();

/* ------------------------------------------------------------------------- */

/*
 * ResolveColor scales down an rgb to the specified number of bits.
 */

void
ResolveColor (pVisual, red, green, blue)
    LbxVisualPtr pVisual;
    CARD16 *red, *green, *blue;
{
    int shift = 16 - pVisual->bitsPerRGB;
    unsigned lim = (1 << pVisual->bitsPerRGB) - 1;
    unsigned limr, limg, limb;

    switch (pVisual->class) {
    case PseudoColor:
    case DirectColor:
    case StaticColor:
	/* rescale to rgb bits */
	*red = ((*red >> shift) * 65535) / lim;
	*green = ((*green >> shift) * 65535) / lim;
	*blue = ((*blue >> shift) * 65535) / lim;
	break;
    case GrayScale:
	/* rescale to gray then rgb bits */
	*red = (30L * *red + 59L * *green + 11L * *blue) / 100;
	*blue = *green = *red = ((*red >> shift) * 65535) / lim;
	break;
    case StaticGray:
	limg = pVisual->colormapEntries - 1;
	/* rescale to gray then [0..limg] then [0..65535] then rgb bits */
	*red = (30L * *red + 59L * *green + 11L * *blue) / 100;
	*red = ((((*red * (limg + 1))) >> 16) * 65535) / limg;
	*blue = *green = *red = ((*red >> shift) * 65535) / lim;
	break;
    case TrueColor:
	limr = NUMRED(pVisual) - 1;
	limg = NUMGREEN(pVisual) - 1;
	limb = NUMBLUE(pVisual) - 1;
	/* rescale to [0..limN] then [0..65535] then rgb bits */
	*red = ((((((*red * (limr + 1)) >> 16) *
		   65535) / limr) >> shift) * 65535) / lim;
	*green = ((((((*green * (limg + 1)) >> 16) *
		     65535) / limg) >> shift) * 65535) / lim;
	*blue = ((((((*blue * (limb + 1)) >> 16) *
		    65535) / limb) >> shift) * 65535) / lim;
	break;
    }
}

void (* LbxResolveColor)(
    LbxVisualPtr /* pVisual */,
    CARD16* /* red */,
    CARD16* /* green */,
    CARD16* /* blue */
) = ResolveColor;

static Pixel
find_cell(pent, num, rgb, channel)
    Entry *pent;
    int num;
    CARD32 rgb;
    int channel;
{
    Pixel pixel, freep;

    freep = ~((Pixel)0);
    for (pixel = 0; pixel < num; pixel++, pent++) {
	if (pent->status == PIXEL_SHARED) {
	    switch (channel) {
	    case DoRed:
		if (pent->red == rgb)
		    return pixel;
		break;
	    case DoGreen:
		if (pent->green == rgb)
		    return pixel;
		break;
	    case DoBlue:
		if (pent->blue == rgb)
		    return pixel;
		break;
	    }
	}
	if (pent->status == PIXEL_FREE && freep == ~((Pixel)0))
	    freep = pixel;
    }
    return freep;
}

/*
 * Find a free pixel in the colormap.
 */

Pixel
FindFreePixel (pmap, red, green, blue)
    ColormapPtr pmap;
    CARD32 red, green, blue;
{
    Entry      *pent;
    Pixel	pixel, p;

    if (pmap->pVisual->class != DirectColor) {
	for (pixel = 0, pent = pmap->red;
	     pixel < pmap->pVisual->colormapEntries;
	     pixel++, pent++)
	{
	    if (pent->status == PIXEL_FREE)
		return pixel;
	}
	return ~((Pixel)0);
    }
    p = find_cell(pmap->red, NUMRED(pmap->pVisual), red, DoRed);
    if (p == ~((Pixel)0))
	return p;
    pixel = p << pmap->pVisual->offsetRed;
    p = find_cell(pmap->green, NUMGREEN(pmap->pVisual), green, DoGreen);
    if (p == ~((Pixel)0))
	return p;
    pixel |= p << pmap->pVisual->offsetGreen;
    p = find_cell(pmap->blue, NUMBLUE(pmap->pVisual), blue, DoBlue);
    if (p == ~((Pixel)0))
	return p;
    pixel |= p << pmap->pVisual->offsetBlue;
    return pixel;
}

Pixel (* LbxFindFreePixel)(
    ColormapPtr /* pmap */,
    CARD32 /* red */,
    CARD32 /* green */,
    CARD32 /* blue */
) = FindFreePixel;

typedef unsigned short	BigNumUpper;
typedef unsigned long	BigNumLower;

#define BIGNUMLOWERBITS	24
#define BIGNUMUPPERBITS	16
#define BIGNUMLOWER (1 << BIGNUMLOWERBITS)
#define BIGNUMUPPER (1 << BIGNUMUPPERBITS)
#define UPPERPART(i)	((i) >> BIGNUMLOWERBITS)
#define LOWERPART(i)	((i) & (BIGNUMLOWER - 1))

typedef struct _bignum {
    BigNumUpper	upper;
    BigNumLower	lower;
} BigNumRec, *BigNumPtr;

#define BigNumGreater(x,y) (((x)->upper > (y)->upper) ||\
			    (((x)->upper == (y)->upper) &&\
			     ((x)->lower > (y)->lower)))

#define UnsignedToBigNum(u,r)	(((r)->upper = UPPERPART(u)), \
				 ((r)->lower = LOWERPART(u)))

#define MaxBigNum(r)		(((r)->upper = BIGNUMUPPER-1), \
				 ((r)->lower = BIGNUMLOWER-1))

static void
BigNumAdd (x, y, r)
    BigNumPtr	x, y, r;
{
    BigNumLower	lower, carry = 0;

    lower = x->lower + y->lower;
    if (lower >= BIGNUMLOWER) {
	lower -= BIGNUMLOWER;
	carry = 1;
    }
    r->lower = lower;
    r->upper = x->upper + y->upper + carry;
}

Entry *
FindBestPixel(pmap, red, green, blue, channels)
    ColormapPtr	pmap;
    CARD32	red,
		green,
		blue;
    int		channels;
{
    Entry      *pent;
    int		num;
    Entry	*final;
    Pixel	pixel;
    long	dr, dg, db;
    unsigned long   sq;
    BigNumRec	minval, sum, temp;

    switch(channels)
    {
    case DoRed:
	pent = pmap->red;
	num = NUMRED(pmap->pVisual);
	break;
    case DoGreen:
	pent = pmap->green;
	num = NUMGREEN(pmap->pVisual);
	break;
    case DoBlue:
	pent = pmap->blue;
	num = NUMBLUE(pmap->pVisual);
	break;
    default:
	pent = pmap->red;
	num = pmap->pVisual->colormapEntries;
	break;
    }
    final = pent;
    MaxBigNum(&minval);
    /* look for the minimal difference */
    for (pixel = 0; pixel < num; pent++, pixel++)
    {
	dr = dg = db = 0;
	switch(channels) {
	case DoRed:
	    dr = pent->red - red;
	    break;
	case DoGreen:
	    dg = pent->green - green;
	    break;
	case DoBlue:
	    db = pent->blue - blue;
	    break;
	default:
	    dr = pent->red - red;
	    dg = pent->green - green;
	    db = pent->blue - blue;
	    break;
	}
	sq = dr * dr;
	UnsignedToBigNum (sq, &sum);
	sq = dg * dg;
	UnsignedToBigNum (sq, &temp);
	BigNumAdd (&sum, &temp, &sum);
	sq = db * db;
	UnsignedToBigNum (sq, &temp);
	BigNumAdd (&sum, &temp, &sum);
	if (BigNumGreater (&minval, &sum))
	{
	    final = pent;
	    minval = sum;
	}
    }
    return final;
}

Entry * (* LbxFindBestPixel)(
    ColormapPtr /* pmap */,
    CARD32	/* red */,
    CARD32	/* green */,
    CARD32	/* blue */,
    int		/* channels */
) = FindBestPixel;


/* ------------------------------------------------------------------------- */

#define PIX_IN(ptr,is2,dst) \
    if (is2) { \
        dst = *ptr++ << 8; dst |= *ptr++; \
    } else \
        dst = *ptr++

#define RGB_IN(ptr,is2,lim,dst) \
    if (is2) { \
        dst = *ptr++ << 8; dst |= *ptr++; \
    } else \
        dst = (*ptr++ * 65535) / lim

static CARD8 *
DecodeChannel(pmap, pent, flags, channels, data)
    ColormapPtr pmap;
    Entry *pent;
    CARD8 flags;
    CARD8 channels;
    CARD8 *data;
{
    Bool px2;
    Bool rgb2;
    CARD16 pixel1, pixel2;
    int i;
    int lim;
    CARD8 code;

    px2 = (flags & LBX_2BYTE_PIXELS) != 0;
    rgb2 = (flags & LBX_RGB_BITS_MASK) > 7;
    lim = (1 << ((flags & LBX_RGB_BITS_MASK) + 1)) - 1;
    /*
     * All pixels other than the ones specified in the GrabCmap reply
     * are implied to be free.  Mark all the cells free now, and then
     * mark the cells specified in the GrabCmap reply either:
     *
     * PIXEL_PRIVATE - a read/write cell - proxy can't do local alloc
     *		       in this cell.  We don't care about the contents.
     * PIXEL_SHARED - a read only cell that can be shared.
     */

    for (i = 0; i < pmap->pVisual->colormapEntries; i++)
    {
	if ((pent[i].status == PIXEL_SHARED && pent[i].refcnt == 0) ||
	    (pent[i].status == PIXEL_PRIVATE && pent[i].server_ref))
	    pent[i].status = PIXEL_FREE;
    }

    while ((code = *data++))
    {
	if (code == LBX_PIXEL_PRIVATE)
	{
	    PIX_IN(data, px2, pixel1);

	    pent[pixel1].pixel = pixel1;
	    if (pent[pixel1].status != PIXEL_PRIVATE)
		pent[pixel1].server_ref = 1;
	    pent[pixel1].status = PIXEL_PRIVATE;

#ifdef COLOR_DEBUG
	    fprintf (stderr, "Got PIXEL_PRIVATE, pixel = %d\n", pixel1);
#endif
	}
	else if (code == LBX_PIXEL_RANGE_PRIVATE)
	{
	    PIX_IN(data, px2, pixel1);
	    PIX_IN(data, px2, pixel2);

	    for (i = pixel1; i <= pixel2; i++)
	    {
		pent[i].pixel = i;
		if (pent[i].status != PIXEL_PRIVATE)
		    pent[i].server_ref = 1;
		pent[i].status = PIXEL_PRIVATE;
	    }

#ifdef COLOR_DEBUG
	    fprintf (stderr, "Got PIXEL_RANGE_PRIVATE, pixels = %d...%d\n",
		     pixel1, pixel2);
#endif
	}
	else if (code == LBX_PIXEL_SHARED)
	{
	    PIX_IN(data, px2, pixel1);

	    pent[pixel1].pixel = pixel1;
	    if (pent[pixel1].status != PIXEL_SHARED)
		pent[pixel1].refcnt = 0;		
	    pent[pixel1].status = PIXEL_SHARED;
	    pent[pixel1].server_ref = 1;
	    if (channels & DoRed) {
		RGB_IN(data, rgb2, lim, pent[pixel1].red);
	    }
	    if (channels & DoGreen) {
		RGB_IN(data, rgb2, lim, pent[pixel1].green);
	    }
	    if (channels & DoBlue) {
		RGB_IN(data, rgb2, lim, pent[pixel1].blue);
	    }

#ifdef COLOR_DEBUG
	    fprintf (stderr,
		     "Got PIXEL_SHARED, pixel = %d, rgb = (%d,%d,%d)\n",
		     pixel1, pent[pixel1].red, pent[pixel1].green,
		     pent[pixel1].blue);
#endif
	}
	else if (code == LBX_PIXEL_RANGE_SHARED)
	{
	    PIX_IN(data, px2, pixel1);
	    PIX_IN(data, px2, pixel2);

#ifdef COLOR_DEBUG
	    fprintf (stderr, "Got PIXEL_RANGE_SHARED, pixels = %d...%d\n",
		     pixel1, pixel2);
#endif

	    for (i = pixel1; i <= pixel2; i++)
	    {
		pent[i].pixel = i;
		if (pent[i].status != PIXEL_SHARED)
		    pent[i].refcnt = 0;
		pent[i].status = PIXEL_SHARED;
		pent[i].server_ref = 1;
		if (channels & DoRed) {
		    RGB_IN(data, rgb2, lim, pent[i].red);
		}
		if (channels & DoGreen) {
		    RGB_IN(data, rgb2, lim, pent[i].green);
		}
		if (channels & DoBlue) {
		    RGB_IN(data, rgb2, lim, pent[i].blue);
		}

#ifdef COLOR_DEBUG
		fprintf (stderr, "    pixel = %d, rgb = (%d,%d,%d)\n",
			 i, pent[i].red, pent[i].green, pent[i].blue);
#endif
	    }
	}
	else
	    break;
    }
    return data;
}

static void
GotColormapGrab (pmap, flags, data)
    ColormapPtr pmap;
    CARD8 flags;
    CARD8 *data;
{

    pmap->grab_status = CMAP_GRABBED;

    if (flags & LBX_SMART_GRAB)
    {
	/*
	 * We have a SMART GRAB : since this proxy last ungrabbed the
	 * colormap, no color cell was alloc'd by an entity other than
	 * this proxy (this includes other proxies as well as clients
	 * directly connected to the X server without a proxy).
	 *
	 * We want to optimize this special case because a proxy may give
	 * up a grab because it got a request that it could not handle
	 * (e.g. AllocNamedColor or LookupColor).  When it asks back for
	 * the grab, there is no need for the server to send the colormap
	 * state, because the proxy is already up to date on the state of
	 * the colormap.
	 *
	 * In order for this to work, the following assumptions are made
	 * about the proxy:
	 *
	 * - the proxy is kept up to date on all cell allocations made on its
	 *   behalf resulting from the following requests: AllocNamedColor, 
	 *   AllocColorCells, AllocColorPlanes
	 * - the proxy is kept up to date on all cells freed by any client
	 *   via the LbxFreeCell event.
	 */

	return;
    }

    if ((pmap->pVisual->class | DynamicClass) != DirectColor)
	DecodeChannel(pmap, pmap->red, flags, DoRed|DoGreen|DoBlue, data);
    else {
	data = DecodeChannel(pmap, pmap->red, flags, DoRed, data);
	data = DecodeChannel(pmap, pmap->green, flags, DoGreen, data);
	DecodeChannel(pmap, pmap->blue, flags, DoBlue, data);
    }
}

static void
GrabCmap (client, pmap, red, green, blue, alloc_named, xred, xgreen, xblue)
    ClientPtr client;
    ColormapPtr pmap;
    CARD16 red, green, blue;
    Bool alloc_named;
    CARD16 xred, xgreen, xblue;
{
    xLbxGrabCmapReq req;
    ReplyStuffPtr nr;

#ifdef COLOR_DEBUG
    fprintf (stderr, "LbxGrabCmapReq: cmap = 0x%x, seq = 0x%x\n",
	     pmap->id, LBXSequenceNumber(client));
    fprintf (stderr, "suspending %s (%d, %d, %d)\n",
	     alloc_named ? "AllocNamedColor" : "AllocColor",
	     red, green, blue);
#endif

    nr = NewReply(client, client->server->lbxReq, X_LbxGrabCmap,
		  grab_cmap_reply);
    if (nr)
    {
	--nr->sequenceNumber;
	nr->request_info.lbxgrabcmap.pmap = pmap;
	nr->request_info.lbxgrabcmap.alloc_named = alloc_named;
	nr->request_info.lbxgrabcmap.vred = red;
	nr->request_info.lbxgrabcmap.vgreen = green;
	nr->request_info.lbxgrabcmap.vblue = blue;
	nr->request_info.lbxgrabcmap.xred = xred;
	nr->request_info.lbxgrabcmap.xgreen = xgreen;
	nr->request_info.lbxgrabcmap.xblue = xblue;
    }

    req.reqType = client->server->lbxReq;
    req.lbxReqType = X_LbxGrabCmap;
    req.length = sz_xLbxGrabCmapReq >> 2;
    req.cmap = pmap->id;

    if (client->swapped)
	SwapGrabCmap (&req);

    WriteReqToServer (client, sz_xLbxGrabCmapReq, (char *) &req, TRUE);

    pmap->grab_status = CMAP_GRAB_REQUESTED;
}

static Bool
grab_cmap_reply(client, nr, data)
    ClientPtr   client;
    ReplyStuffPtr nr;
    char       *data;
{
    xLbxGrabCmapReply *reply;
    Entry      *pent;

    reply = (xLbxGrabCmapReply *) data;

#ifdef COLOR_DEBUG
    fprintf (stderr, "LbxGrabCmapReply: cmap = 0x%x, seq = 0x%x, ",
	     nr->request_info.lbxgrabcmap.pmap->id, reply->sequenceNumber);
    fprintf (stderr, "flags = %x\n", reply->flags);
    fprintf (stderr, "resuming %s (%d, %d, %d)\n",
	     nr->request_info.lbxgrabcmap.alloc_named ?
	     "AllocNamedColor" : "AllocColor",
	     nr->request_info.lbxgrabcmap.vred,
	     nr->request_info.lbxgrabcmap.vgreen,
	     nr->request_info.lbxgrabcmap.vblue);
#endif

    GotColormapGrab (nr->request_info.lbxgrabcmap.pmap, reply->flags,
		     (CARD8 *) reply + sz_xLbxGrabCmapReplyHdr);

    /*
     * We suspended an AllocColor request so we could grab the colormap.
     * Now that the colormap is grabbed we resume handling the AllocColor.
     * We first check to see if the color is already allocated.
     */

    FindPixel (client,
	nr->request_info.lbxgrabcmap.pmap,
	nr->request_info.lbxgrabcmap.vred,
	nr->request_info.lbxgrabcmap.vgreen,
	nr->request_info.lbxgrabcmap.vblue,
	&pent);

    if (pent)
    {
	/*
	 * We found the pixel in the proxy's colormap.  We can
	 * immediately short circuit this AllocColor.
	 */

	FoundPixel (client, TRUE, nr->request_info.lbxgrabcmap.pmap, pent,
		    nr->request_info.lbxgrabcmap.alloc_named,
		    nr->request_info.lbxgrabcmap.xred,
		    nr->request_info.lbxgrabcmap.xgreen,
		    nr->request_info.lbxgrabcmap.xblue);
    }
    else
    {
	/*
	 * We didn't find the pixel, but we just grabbed the color map,
	 * so we can handle the color allocation locally now, then do
	 * the short circuit.
	 */

	LocalAllocColor (client, TRUE,
			 nr->request_info.lbxgrabcmap.pmap,
			 nr->request_info.lbxgrabcmap.vred,
			 nr->request_info.lbxgrabcmap.vgreen,
			 nr->request_info.lbxgrabcmap.vblue,
			 nr->request_info.lbxgrabcmap.alloc_named,
			 nr->request_info.lbxgrabcmap.xred,
			 nr->request_info.lbxgrabcmap.xgreen,
			 nr->request_info.lbxgrabcmap.xblue);
    }

    return TRUE;
}


/*
 * Release the colormap currently grabbed by the proxy.
 */

void
ReleaseCmap (client, pmap)
    ClientPtr client;
    ColormapPtr pmap;
{
    xLbxReleaseCmapReq req;

    pmap->grab_status = CMAP_NOT_GRABBED;

    req.reqType = client->server->lbxReq;
    req.lbxReqType = X_LbxReleaseCmap;
    req.length = sz_xLbxReleaseCmapReq >> 2;
    req.cmap = pmap->id;

    /* write the request on the proxy control connection */

    WriteReqToServer (client->server->serverClient, 
		      sz_xLbxReleaseCmapReq, 
		      (char *) &req,
		      FALSE);
}

/* ------------------------------------------------------------------------- */

static void
DoAllocColorReply (client, in_reply, red, green, blue, pixel)
    ClientPtr client;
    Bool in_reply;
    CARD16 red, green, blue;
    Pixel pixel;
{
    /*
     * Prepare the AllocColor reply for the client.
     */

    xAllocColorReply reply;

    reply.type = X_Reply;
    reply.length = 0;
    reply.sequenceNumber = LBXSequenceNumber(client);

    reply.red = red;
    reply.green = green;
    reply.blue = blue;
    reply.pixel = pixel;

    if (client->swapped)
	SwapAllocColorReply (&reply);

    if (LBXCacheSafe (client))
    {
	/*
	 * We can write the AllocColor reply now.
	 */

	if (!in_reply)
	    FinishLBXRequest(client, REQ_REPLACE);

	WriteToClient (client, sizeof (xAllocColorReply), &reply);

#ifdef LBX_STATS
	ac_good++;
#endif
    }
    else
    {
	/*
	 * We can't write the AllocColor reply now, we must first sync.
	 */

	if (!LBXCanDelayReply(client))
	    SendLbxSync (client);
	if (!in_reply)
	    FinishLBXRequest(client, REQ_REPLACELATE);

	/*
	 * Save the AllocColor reply.  We will write it when the
	 * LbxSync reply comes back.
	 */

	SaveReplyData (client, (xReply *) & reply, 0, NULL);
    }
}

static void
DoAllocNamedColorReply (client, in_reply, red, green, blue, pixel,
			xred, xgreen, xblue)
    ClientPtr client;
    Bool in_reply;
    CARD16 red, green, blue;
    Pixel pixel;
    CARD16 xred, xgreen, xblue;
{
    /*
     * Prepare the AllocNamedColor reply for the client.
     */

    xAllocNamedColorReply reply;

    reply.type = X_Reply;
    reply.length = 0;
    reply.sequenceNumber = LBXSequenceNumber(client);

    reply.exactRed = xred;
    reply.exactGreen = xgreen;
    reply.exactBlue = xblue;
    reply.screenRed = red;
    reply.screenGreen = green;
    reply.screenBlue = blue;
    reply.pixel = pixel;

    if (client->swapped)
	SwapAllocNamedColorReply (&reply);

    if (LBXCacheSafe (client))
    {
	/*
	 * We can write the AllocColor reply now.
	 */

	if (!in_reply)
	    FinishLBXRequest(client, REQ_REPLACE);

	WriteToClient (client, sizeof (xAllocNamedColorReply), &reply);

#ifdef LBX_STATS
	anc_good++;
#endif
    }
    else
    {
	/*
	 * We can't write the AllocColor reply now, we must first sync.
	 */

	if (!LBXCanDelayReply(client))
	    SendLbxSync (client);
	if (!in_reply)
	    FinishLBXRequest(client, REQ_REPLACELATE);

	/*
	 * We can't write the AllocNamedColor reply now, we must first
	 * sync.  You might ask why we didn't just send the AllocNamedColor
	 * request to the server?  The answer is that if the colormap is
	 * grabbed, we don't want to send the AllocNamedColor to the server
	 * because that would force the proxy to give up control over the
	 * colormap.  So the proxy generates the reply on its own, but must
	 * force a round trip sync before the reply is written.
	 */

	SaveReplyData (client, (xReply *) & reply, 0, NULL);

#ifdef LBX_STATS
	anc_miss++;
#endif
    }
}

/*
 * LocalAllocColor is called when the specified color is not already
 * allocated in the colormap, and the proxy has the colormap grabbed.
 * The proxy handles the AllocColor locally, and tells the server
 * what allocation it made.
 *
 * (red, green, blue) are actual rgb values, not requested ones -
 * they have already been put through ResolveColor.
 */

static void
LocalAllocColor (client, in_reply, pmap, red, green, blue, alloc_named,
		 xred, xgreen, xblue)
    ClientPtr   client;
    Bool	in_reply;
    ColormapPtr pmap;
    CARD16      red, green, blue;
    Bool	alloc_named;
    CARD16      xred, xgreen, xblue;
{
    Pixel pixel;

    /*
     * First find a free cell.
     */

    pixel = (*LbxFindFreePixel) (pmap, red, green, blue);

    if (pixel == ~((Pixel)0))
    {
#ifdef COLOR_DEBUG
	fprintf(stderr, "alloc failed: (%d, %d, %d)\n", red, green, blue);
#endif
	if (in_reply)
	    WriteError(client,
		       alloc_named ? X_AllocNamedColor : X_AllocColor,
		       0, pmap->id, BadAlloc);
	else
	    SendErrorToClient(client,
			      alloc_named ? X_AllocNamedColor : X_AllocColor,
			      0, pmap->id, BadAlloc);
	return;
    }

#ifdef COLOR_DEBUG
    if (LBXCacheSafe (client))
	fprintf (stderr,
		 "X %s: short circuiting (allocating color locally):\n",
		 alloc_named ? "AllocNamedColor" : "AllocColor");
    else
	fprintf (stderr,
		 "X %s: allocating color locally, but need Sync:\n",
		 alloc_named ? "AllocNamedColor" : "AllocColor");

    fprintf (stderr,
	"    seq = 0x%x, cmap = 0x%x, pixel = %d, rgb = (%d,%d,%d)\n",
	LBXSequenceNumber(client), pmap->id, pixel, red, green, blue);
#endif

    /*
     * Now handle the X AllocColor reply to the client.
     */

    if (alloc_named)
	DoAllocNamedColorReply (client, in_reply, red, green, blue, pixel,
				xred, xgreen, xblue);
    else
	DoAllocColorReply (client, in_reply, red, green, blue, pixel);

    /*
     * Store the rgb in the cell.
     */

    StorePixel (client, pmap, red, green, blue, pixel, FALSE);
}


static void
FoundPixel (client, in_reply, pmap, pent, alloc_named, xred, xgreen, xblue)
    ClientPtr client;
    Bool in_reply;
    ColormapPtr pmap;
    Entry *pent;
    Bool alloc_named;
    CARD16 xred, xgreen, xblue;
{
#ifdef COLOR_DEBUG
    if (LBXCacheSafe (client))
	fprintf (stderr,
		 "X %s: short circuiting (color already exists):\n",
		 alloc_named ? "AllocNamedColor" : "AllocColor");
    else
    {
	fprintf (stderr, 
		 "X %s: color already exists locally, but need Sync:\n",
		 alloc_named ? "AllocNamedColor" : "AllocColor");
    }

    fprintf (stderr,
	"    seq = 0x%x, cmap = 0x%x, pixel = %d, rgb = (%d,%d,%d)\n",
	LBXSequenceNumber(client), pmap->id, pent->pixel,
	pent->red, pent->green, pent->blue);
#endif

    /*
     * Now handle the X AllocColor reply to the client.
     */

    if (alloc_named)
	DoAllocNamedColorReply (client, in_reply,
				pent->red, pent->green, pent->blue,
				pent->pixel, xred, xgreen, xblue);
    else
	DoAllocColorReply (client, in_reply,
			   pent->red, pent->green, pent->blue, pent->pixel);

    /*
     * We found a match.  Increment our ref count and tell the
     * server to bump up its own ref count.
     */

    IncrementPixel (client, pmap, pent, FALSE);
}


/* ------------------------------------------------------------------------- */

int
ProcLBXCreateColormap(client)
    ClientPtr   client;
{
    REQUEST(xCreateColormapReq);
    Colormap    cmap;
    VisualID    vis;
    char        n;

    /* AllocAll are read/write, so ignore */
    if (stuff->alloc) {
	cmap = stuff->mid;
	vis = stuff->visual;
	if (client->swapped) {
	    swapl(&cmap, n);
	    swapl(&vis, n);
	}
	CreateColormap(client, cmap, vis);
    }

    return ProcStandardRequest(client);
}

static      ColormapPtr
create_colormap(cmap, visual)
    Colormap    cmap;
    VisualID    visual;
{
    ColormapPtr pmap;
    LbxVisualPtr pvis;
    int         tsize, csize;
    Pixel     **pptr;

    pvis = GetVisual(visual);
    csize = pvis->colormapEntries;
    tsize = (csize * sizeof(Entry)) + (MAXCLIENTS * sizeof(Pixel *)) +
	(MAXCLIENTS * sizeof(int));
    if ((pvis->class | DynamicClass) == DirectColor)
	tsize *= 3;
    tsize += sizeof(ColormapRec);
    pmap = (ColormapPtr) xalloc(tsize);
    if (!pmap)
	return pmap;
    bzero((char *) pmap, tsize);
    pmap->id = cmap;
    pmap->pVisual = pvis;
    pmap->grab_status = CMAP_NOT_GRABBED;
    pmap->red = (Entry *) ((char *) pmap + sizeof(ColormapRec));
    pmap->clientPixelsRed = (Pixel **)
	((char *) pmap->red + (csize * sizeof(Entry)));
    pmap->numPixelsRed = (int *) ((char *) pmap->clientPixelsRed +
				  (MAXCLIENTS * sizeof(Pixel *)));
    bzero((char *) pmap->red, (csize * sizeof(Entry)));
    bzero((char *) pmap->numPixelsRed, (MAXCLIENTS * sizeof(int)));
    for (pptr = &pmap->clientPixelsRed[MAXCLIENTS];
	 --pptr >= pmap->clientPixelsRed; )
	*pptr = (Pixel *) NULL;
    if ((pvis->class | DynamicClass) != DirectColor) {
	pmap->green = NULL;
	pmap->numPixelsGreen = NULL;
	pmap->clientPixelsGreen = NULL;
	pmap->blue = NULL;
	pmap->numPixelsBlue = NULL;
	pmap->clientPixelsBlue = NULL;
	return pmap;
    }
    pmap->green = (Entry *) ((char *) pmap->numPixelsRed +
			     (MAXCLIENTS * sizeof(int)));
    pmap->clientPixelsGreen = (Pixel **)
	((char *) pmap->green + (csize * sizeof(Entry)));
    pmap->numPixelsGreen = (int *) ((char *) pmap->clientPixelsGreen +
				    (MAXCLIENTS * sizeof(Pixel *)));
    bzero((char *) pmap->green, (csize * sizeof(Entry)));
    bzero((char *) pmap->numPixelsGreen, (MAXCLIENTS * sizeof(int)));
    for (pptr = &pmap->clientPixelsGreen[MAXCLIENTS];
	 --pptr >= pmap->clientPixelsGreen; )
	*pptr = (Pixel *) NULL;
    pmap->blue = (Entry *) ((char *) pmap->numPixelsGreen +
			    (MAXCLIENTS * sizeof(int)));
    pmap->clientPixelsBlue = (Pixel **)
	((char *) pmap->blue + (csize * sizeof(Entry)));
    pmap->numPixelsBlue = (int *) ((char *) pmap->clientPixelsBlue +
				   (MAXCLIENTS * sizeof(Pixel *)));
    bzero((char *) pmap->blue, (csize * sizeof(Entry)));
    bzero((char *) pmap->numPixelsBlue, (MAXCLIENTS * sizeof(int)));
    for (pptr = &pmap->clientPixelsBlue[MAXCLIENTS];
	 --pptr >= pmap->clientPixelsBlue; )
	*pptr = (Pixel *) NULL;
    return pmap;
}

/* ARGSUSED */
Bool
CreateColormap(client, cmap, visual)
    ClientPtr   client;
    Colormap    cmap;
    VisualID    visual;
{
    ColormapPtr pmap;

    pmap = create_colormap(cmap, visual);
    if (!pmap)
	return FALSE;
    return AddResource(client, cmap, RT_COLORMAP, (pointer) pmap);
}

/* ------------------------------------------------------------------------- */


int
ProcLBXFreeColormap(client)
    ClientPtr   client;
{
    REQUEST(xResourceReq);
    Colormap    cmap;
    char        n;

    cmap = stuff->id;
    if (client->swapped) {
	swapl(&cmap, n);
    }
    FreeColormap(client, cmap);

    return ProcStandardRequest(client);
}

/* ARGSUSED */
Bool
FreeColormap(client, cmap)
    ClientPtr   client;
    Colormap    cmap;
{
    ColormapPtr pmap;

    pmap = (ColormapPtr) LookupIDByType(client, cmap, RT_COLORMAP);
    if (!pmap)
	return FALSE;
    FreeResource(client, cmap, RT_NONE);
    return TRUE;
}

/* ------------------------------------------------------------------------- */

/*
 * cop out: don't try to track the new colormap
 */
int
ProcLBXCopyColormapAndFree(client)
    ClientPtr   client;
{
    REQUEST(xCopyColormapAndFreeReq);
    Colormap    srcmap;
    ColormapPtr	pmap;
    char        n;

    srcmap = stuff->srcCmap;
    if (client->swapped) {
	swapl(&srcmap, n);
    }
    pmap = (ColormapPtr) LookupIDByType(client, srcmap, RT_COLORMAP);
    if (pmap)
	FreeAllClientPixels(pmap, client->index);

    return ProcStandardRequest(client);
}

/* ------------------------------------------------------------------------- */

int
ProcLBXFreeColors(client)
    ClientPtr   client;
{
    REQUEST(xFreeColorsReq);
    int         num;
    Pixel      *pixels;
    CARD32      mask;
    Colormap    cmap;
    Bool        freepix = FALSE;
    int         n;
    CARD16	len;
    ColormapPtr pmap;

    mask = stuff->planeMask;
    cmap = stuff->cmap;
    len = stuff->length;

    if (client->swapped) {
	swapl(&cmap, n);
	swapl(&mask, n);
	swaps(&len, n);
    }
    pmap = (ColormapPtr) LookupIDByType(client, cmap, RT_COLORMAP);
    if (!pmap)
	return ProcStandardRequest(client);

    num = ((len << 2) - sizeof(xFreeColorsReq)) >> 2;

    if (client->swapped) {
	pixels = (Pixel *) ALLOCATE_LOCAL(num * sizeof(Pixel));
	if (pixels) {
	    memcpy((char *) pixels, (char *) &stuff[1], (num * sizeof(Pixel)));
	    SwapLongs((CARD32 *) pixels, num);
	}
	freepix = TRUE;
    } else
	pixels = (Pixel *) &stuff[1];

#ifdef COLOR_DEBUG
    fprintf(stderr, "freeing on cmap 0x%x mask: %d pixels:", cmap, mask);
    for (n = 0; n < num; n++)
	fprintf(stderr, " %d", pixels[n]);
    fprintf(stderr, "\n");
#endif
    FreePixels(client, pmap, num, pixels, mask);

    if (freepix)
	DEALLOCATE_LOCAL(pixels);

    return ProcStandardRequest(client);
}

/* ------------------------------------------------------------------------- */

int
ProcLBXAllocColor(client)
    ClientPtr   client;
{
    REQUEST(xAllocColorReq);
    Entry      *pent;
    Colormap    cmap;
    ColormapPtr pmap;
    CARD16      red, green, blue;
    char        n;

    cmap = stuff->cmap;

    if (client->swapped)
	swapl(&cmap, n);

    pmap = (ColormapPtr) LookupIDByType (client, cmap, RT_COLORMAP);

    if (!pmap)
	return ProcStandardRequest(client);

    red = stuff->red;
    green = stuff->green;
    blue = stuff->blue;

    if (client->swapped)
    {
	swaps(&red, n);
	swaps(&green, n);
	swaps(&blue, n);
    }


    /*
     * Resolve the color (requested rgb -> actual rgb)
     */

    (*LbxResolveColor)(pmap->pVisual, &red, &green, &blue);


    /*
     * Search for the pixel in the proxy's colormap.
     */

    FindPixel (client, pmap, red, green, blue, &pent);

    if (pent && (pent->refcnt || pmap->grab_status == CMAP_GRABBED))
    {
	/*
	 * We found the pixel in the proxy's colormap.  We can
	 * short circuit this AllocColor.
	 */

	FoundPixel (client, FALSE, pmap, pent, FALSE, 0, 0, 0);
    }
    else if (pmap->grab_status == CMAP_GRABBED)
    {
	/*
	 * The color map is already grabbed by the proxy, so we
	 * can handle the AllocColor locally now.
	 */

	LocalAllocColor(client, FALSE, pmap, red, green, blue, FALSE, 0, 0, 0);
    }
    else
    {
	/*
	 * The proxy must first grab the color map.  Then it can
	 * handle the AllocColor.
	 */

	FinishLBXRequest(client, REQ_REPLACELATE);

	GrabCmap(client, pmap, red, green, blue, FALSE, 0, 0, 0);
    }
    return Success;
}

/* ------------------------------------------------------------------------- */

int
ProcLBXAllocNamedColor(client)
    ClientPtr   client;
{
    REQUEST(xAllocNamedColorReq);
    Entry      *pent;
    RGBEntryPtr rgbe;
    ReplyStuffPtr nr;
    CARD16      nbytes;
    Colormap    cmap;
    ColormapPtr pmap;
    char        n;

    cmap = stuff->cmap;
    nbytes = stuff->nbytes;

    if (client->swapped) {
	swapl(&cmap, n);
	swaps(&nbytes, n);
    }

    pmap = (ColormapPtr) LookupIDByType (client, cmap, RT_COLORMAP);

    if (nbytes > MAX_COLORNAME_LENGTH || !pmap)
	return ProcStandardRequest(client);

    pent = NULL;
    rgbe = FindColorName(client->server, (char *) &stuff[1], nbytes, 
			 pmap->pVisual);
    if (rgbe) {
#ifdef COLOR_DEBUG
	fprintf(stderr, "looking for %.*s = (%d,%d,%d)\n", nbytes,
		(char *)&stuff[1], rgbe->vred, rgbe->vgreen, rgbe->vblue);
#endif
	FindPixel (client, pmap, rgbe->vred, rgbe->vgreen, rgbe->vblue, &pent);
    }
    if (pent && (pent->refcnt || pmap->grab_status == CMAP_GRABBED)) {
	FoundPixel (client, FALSE, pmap, pent, TRUE,
		    rgbe->xred, rgbe->xgreen, rgbe->xblue);
    } else if (rgbe) {
	if (pmap->grab_status == CMAP_GRABBED) {
	    /*
	     * The color map is already grabbed by the proxy, so we
	     * can handle the AllocNamedColor locally now.
	     */

	    LocalAllocColor (client, FALSE, pmap,
			     rgbe->vred, rgbe->vgreen, rgbe->vblue,
			     TRUE,
			     rgbe->xred, rgbe->xgreen, rgbe->xblue);
	} else {
	    FinishLBXRequest(client, REQ_REPLACELATE);
	    GrabCmap(client, pmap, 
		     rgbe->vred, rgbe->vgreen, rgbe->vblue,
		     TRUE, rgbe->xred, rgbe->xgreen, rgbe->xblue);
	}
    } else {

	/*
	 * We can't short circuit the AllocNamedColor request.
	 * The proxy will need control over the colormap to handle
	 * this request, so rather than wait for the server to ask
	 * the proxy to release the colormap, we release it now.
	 */

	if (pmap->grab_status == CMAP_GRABBED &&
	    (pmap->pVisual->class & DynamicClass))
	    ReleaseCmap (client, pmap);


	/*
	 * We need to catch the AllocNamedColor reply so we can cache
	 * the results for future short circuiting.
	 */

	nr = NewReply(client, X_AllocNamedColor, 0, alloc_named_color_reply);
	if (!nr)
	    return ProcStandardRequest(client);
	nr->request_info.xallocnamedcolor.pmap = pmap;
	strncpy(nr->request_info.xallocnamedcolor.name, (char *) &stuff[1],
		nbytes);
	nr->request_info.xallocnamedcolor.namelen = nbytes;

#ifdef COLOR_DEBUG
	fprintf (stderr, "X AllocNamedColor: could not short circuit\n");
	fprintf (stderr, "    seq = 0x%x, cmap = 0x%x\n",
		 LBXSequenceNumber(client), cmap);
#endif

#ifdef LBX_STATS
	anc_miss++;
#endif
	return ProcStandardRequest(client);
    }
    return Success;
}

static Bool
alloc_named_color_reply(client, nr, data)
    ClientPtr   client;
    ReplyStuffPtr nr;
    char       *data;
{
    xAllocNamedColorReply *reply;
    Pixel       pixel;
    char        n;
    RGBEntryRec rgbe;

    reply = (xAllocNamedColorReply *) data;

    rgbe.xred = reply->exactRed;
    rgbe.xgreen = reply->exactGreen;
    rgbe.xblue = reply->exactBlue;
    rgbe.vred = reply->screenRed;
    rgbe.vgreen = reply->screenGreen;
    rgbe.vblue = reply->screenBlue;
    pixel = reply->pixel;

    if (client->swapped) {
	swapl(&pixel, n);
	swaps(&rgbe.xred, n);
	swaps(&rgbe.xgreen, n);
	swaps(&rgbe.xblue, n);
	swaps(&rgbe.vred, n);
	swaps(&rgbe.vgreen, n);
	swaps(&rgbe.vblue, n);
    }

#ifdef COLOR_DEBUG
	fprintf (stderr,
	    "X AllocNamedColorReply: caching results\n");
	fprintf (stderr, "pixel = %d, rgb = (%d,%d,%d)\n",
		 pixel, rgbe.vred, rgbe.vgreen, rgbe.vblue);
#endif

    AddColorName(client->server,
		 nr->request_info.xallocnamedcolor.name,
		 nr->request_info.xallocnamedcolor.namelen,
		 &rgbe);

    return StorePixel(client, nr->request_info.xallocnamedcolor.pmap,
		      rgbe.vred, rgbe.vgreen, rgbe.vblue, pixel, TRUE);
}

/* ------------------------------------------------------------------------- */

int
ProcLBXAllocColorCells(client)
    ClientPtr   client;
{
    REQUEST(xAllocColorCellsReq);
    ReplyStuffPtr nr;
    Colormap    cmap;
    ColormapPtr pmap;
    char        n;

    cmap = stuff->cmap;
    if (client->swapped)
	swapl(&cmap, n);

    pmap = (ColormapPtr) LookupIDByType (client, cmap, RT_COLORMAP);

    if (!pmap)
	return ProcStandardRequest(client);
    if (!(pmap->pVisual->class & DynamicClass))
	return BadAlloc;

    /*
     * We don't short circuit AllocColorCells requests.
     * The server will need control over the colormap to handle
     * this request, so rather than wait for the server to ask
     * the proxy to release the colormap, we release it now.
     */

    if (pmap->grab_status == CMAP_GRABBED)
	ReleaseCmap (client, pmap);


    /*
     * The proxy needs to keep track of all read/write cells allocated
     * for its own clients.  By doing this, we can optimize the special
     * case of GrabCmapReply with smartGrab = TRUE.  This means
     * that the proxy asked back for a grab that it temporarily gave up
     * to the server, and no other client or proxy did a color allocation
     * in the colormap.
     */

    nr = NewReply(client, X_AllocColorCells, 0, alloc_color_cells_reply);

    if (nr)
	nr->request_info.xalloccolorcells.pmap = pmap;

    return ProcStandardRequest(client);
}

static Bool
alloc_color_cells_reply(client, nr, data)
    ClientPtr   client;
    ReplyStuffPtr nr;
    char       *data;
{
    xAllocColorCellsReply *reply;
    CARD16 nPixels, nMasks;
    CARD32 *pixels, *masks;
    ColormapPtr pmap;
    int i, j, k;
    char n;

    reply = (xAllocColorCellsReply *) data;

    pmap = nr->request_info.xalloccolorcells.pmap;

#ifdef COLOR_DEBUG
    fprintf (stderr, "AllocColorCells on cmap 0x%x:",
	     nr->request_info.xalloccolorcells.pmap->id);
#endif

    nPixels = reply->nPixels;
    nMasks = reply->nMasks;

    if (client->swapped)
    {
	swaps(&nPixels, n);
	swaps(&nMasks, n);

	pixels = (CARD32 *) xalloc (nPixels * sizeof (CARD32));
	masks = (CARD32 *) xalloc (nMasks * sizeof (CARD32));

	memcpy (pixels, (char *) (reply + 1), nPixels * sizeof (CARD32));
	memcpy (masks, ((char *) (reply + 1)) + nPixels * sizeof (CARD32),
	    nMasks * sizeof (CARD32));

	for (i = 0; i < nPixels; i++)
	    swapl (&pixels[i], n);
	for (i = 0; i < nMasks; i++)
	    swapl (&masks[i], n);
    }
    else
    {
	pixels = (CARD32 *) (reply + 1);
	masks = pixels + nPixels;
    }

    for (i = 0; i < nPixels; i++)
	for (j = 0; j < (1 << nMasks); j++)
	{
	    CARD32 pixel, plane_mask = 0;
	    int bits = j;

	    for (k = 0; k < nMasks; k++)
	    {
		if (bits & 1)
		    plane_mask |= masks[k];
		bits >>= 1;
	    }

	    pixel = pixels[i] | plane_mask;

	    AllocCell(client, pmap, pixel);

#ifdef COLOR_DEBUG
	    fprintf (stderr, " %d", pixel);
#endif
	}

#ifdef COLOR_DEBUG
    fprintf (stderr, "\n");
#endif
    if (client->swapped)
    {
	xfree (pixels);
	xfree (masks);
    }

    return TRUE;
}

/* ------------------------------------------------------------------------- */

int
ProcLBXAllocColorPlanes(client)
    ClientPtr   client;
{
    REQUEST(xAllocColorPlanesReq);
    ReplyStuffPtr nr;
    Colormap    cmap;
    ColormapPtr pmap;
    char        n;

    cmap = stuff->cmap;
    if (client->swapped)
	swapl(&cmap, n);

    pmap = (ColormapPtr) LookupIDByType (client, cmap, RT_COLORMAP);

    if (!pmap)
	return ProcStandardRequest(client);
    if (!(pmap->pVisual->class & DynamicClass))
	return BadAlloc;

    /*
     * We don't short circuit AllocColorPlanes requests.
     * The server will need control over the colormap to handle
     * this request, so rather than wait for the server to ask
     * the proxy to release the colormap, we release it now.
     */

    if (pmap->grab_status == CMAP_GRABBED)
	ReleaseCmap (client, pmap);


    /*
     * The proxy needs to keep track of all read/write cells allocated
     * for its own clients.  By doing this, we can optimize the special
     * case of GrabCmapReply with smartGrab = TRUE.  This means
     * that the proxy asked back for a grab that it temporarily gave up
     * to the server, and no other client or proxy did a color allocation
     * in the colormap.
     */

    nr = NewReply(client, X_AllocColorPlanes, 0, alloc_color_planes_reply);

    if (nr)
	nr->request_info.xalloccolorplanes.pmap = pmap;

    return ProcStandardRequest(client);
}

static Bool
alloc_color_planes_reply(client, nr, data)
    ClientPtr   client;
    ReplyStuffPtr nr;
    char       *data;
{
    xAllocColorPlanesReply *reply;
    CARD32 redMask, greenMask, blueMask, mask;
    CARD16 nPixels;
    CARD32 *pixels;
    ColormapPtr pmap;
    int i;
    char n;

    reply = (xAllocColorPlanesReply *) data;

    pmap = nr->request_info.xalloccolorplanes.pmap;

    nPixels = reply->nPixels;
    redMask = reply->redMask;
    greenMask = reply->greenMask;
    blueMask = reply->blueMask;

    if (client->swapped)
    {
	swaps(&nPixels, n);
	swapl(&redMask, n);
	swapl(&greenMask, n);
	swapl(&blueMask, n);

	pixels = (CARD32 *) xalloc (nPixels * sizeof (CARD32));

	memcpy (pixels, (char *) (reply + 1), nPixels * sizeof (CARD32));

	for (i = 0; i < nPixels; i++)
	    swapl (&pixels[i], n);

    }
    else
    {
	pixels = (CARD32 *) (reply + 1);
    }

#ifdef COLOR_DEBUG
    fprintf (stderr, "AllocColorPlanes on cmap 0x%x:",
	     nr->request_info.xalloccolorplanes.pmap->id);
#endif

    mask = redMask | greenMask | blueMask;

    for (i = 0; i < nPixels; i++)
    {
	Pixel pixel, x = 0;

	do
	{
	    x = (x + ~mask + 1) & mask;

	    pixel = x | pixels[i];

	    AllocCell(client, pmap, pixel);

#ifdef COLOR_DEBUG
	    fprintf(stderr, " %d", pixel);
#endif
	} while (x);
    }

#ifdef COLOR_DEBUG
    fprintf(stderr, "\n");
#endif
    if (client->swapped)
    {
	xfree (pixels);
    }

    return TRUE;
}

/* ------------------------------------------------------------------------- */

int
ProcLBXLookupColor(client)
    ClientPtr   client;
{
    REQUEST(xLookupColorReq);
    xLookupColorReply reply;
    ReplyStuffPtr nr;
    RGBEntryPtr rgbe;
    int         len;
    char        n;
    Colormap    cmap;
    ColormapPtr pmap;

    len = stuff->nbytes;
    cmap = stuff->cmap;

    if (client->swapped) {
	swapl(&cmap, n);
	swaps(&len, n);
    }

    pmap = (ColormapPtr) LookupIDByType (client, cmap, RT_COLORMAP);

    if (len > MAX_COLORNAME_LENGTH || !pmap)
	return ProcStandardRequest(client);

    rgbe = FindColorName(client->server, (char *) &stuff[1], len, 
			 pmap->pVisual);

    if (rgbe) {	/* found the value */
	reply.type = X_Reply;
	reply.length = 0;
	reply.sequenceNumber = LBXSequenceNumber(client);

	reply.exactRed = rgbe->xred;
	reply.exactBlue = rgbe->xblue;
	reply.exactGreen = rgbe->xgreen;

	reply.screenRed = rgbe->vred;
	reply.screenBlue = rgbe->vblue;
	reply.screenGreen = rgbe->vgreen;

#ifdef COLOR_DEBUG
	if (LBXCacheSafe (client))
	    fprintf(stderr, "X LookupColor: short circuiting:\n");
	else
	    fprintf(stderr,
		    "X LookupColor: short circuiting, but need Sync:\n");
	fprintf (stderr, "    seq = 0x%x, name = %.*s\n",
		 LBXSequenceNumber(client), len, (char *)&stuff[1]);
#endif

	if (client->swapped)
	    SwapLookupColorReply(&reply);
	if (LBXCacheSafe(client)) {
	    FinishLBXRequest(client, REQ_YANK);
	    WriteToClient(client, sizeof(xLookupColorReply), &reply);
	} else {
	    if (!LBXCanDelayReply(client))
		SendLbxSync(client);
	    FinishLBXRequest(client, REQ_YANKLATE);
	    SaveReplyData(client, (xReply *) & reply, 0, NULL);
	}

#ifdef LBX_STATS
	luc_good++;
#endif

	return Success;
    } else {
	nr = NewReply(client, X_LookupColor, 0, lookup_color_reply);
	if (!nr)
	    return ProcStandardRequest(client);
	strncpy(nr->request_info.xlookupcolor.name,
		(char *) &stuff[1], len);
	nr->request_info.xlookupcolor.namelen = len;
	nr->request_info.xlookupcolor.visual = pmap->pVisual->id;

#ifdef LBX_STATS
	luc_miss++;
#endif
	return ProcStandardRequest(client);
    }
}


static Bool
lookup_color_reply(client, nr, data)
    ClientPtr   client;
    ReplyStuffPtr nr;
    char       *data;
{
    xLookupColorReply *reply;
    RGBEntryRec rgbe;
    char        n;

    reply = (xLookupColorReply *) data;

    rgbe.xred = reply->exactRed;
    rgbe.xblue = reply->exactBlue;
    rgbe.xgreen = reply->exactGreen;
    rgbe.vred = reply->screenRed;
    rgbe.vgreen = reply->screenGreen;
    rgbe.vblue = reply->screenBlue;

    rgbe.visual = nr->request_info.xlookupcolor.visual;
    if (client->swapped) {
	swaps(&rgbe.xred, n);
	swaps(&rgbe.xgreen, n);
	swaps(&rgbe.xblue, n);
	swaps(&rgbe.vred, n);
	swaps(&rgbe.vgreen, n);
	swaps(&rgbe.vblue, n);
    }
    AddColorName(client->server, 
		 nr->request_info.xlookupcolor.name,
		 nr->request_info.xlookupcolor.namelen,
		 &rgbe);
    return TRUE;
}
