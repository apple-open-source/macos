/* $Xorg: options.c,v 1.4 2001/02/09 02:05:31 xorgcvs Exp $ */
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
/* $XFree86: xc/programs/lbxproxy/di/options.c,v 1.9 2003/12/19 02:05:39 dawes Exp $ */

#include <stdio.h>
#include "X.h"
#include "misc.h"
#include "XLbx.h"
#include "lbxopts.h"
#include "proxyopts.h"
#include "lbximage.h"
#include "util.h"
#include "colormap.h"
#ifndef NO_ZLIB
#include "lbxzlib.h"
#endif /* NO_ZLIB */
#include "wire.h"

static int LbxDeltaReply();
static int LbxProxyDeltaReq();
static int LbxServerDeltaReq();
static int LbxStreamCompReq();
static int LbxBitmapCompReq();
static int LbxPixmapCompReq();
static int LbxSquishReq();
static int LbxUseTagsReq();
static int LbxCmapAllReq();

static int LbxProxyDeltaReply();
static int LbxServerDeltaReply();
static int LbxStreamCompReply();
static int LbxBitmapCompReply();
static int LbxPixmapCompReply();
static int LbxSquishReply();
static int LbxUseTagsReply();
static int LbxCmapAllReply();

static int OptZlibReq();
static int OptZlibReply();

#define OPT_INDEX_STREAM_COMP		2

/*
 * Local variables that get set when the command line options
 * are parsed.
 */
static Bool called_LbxNoComp;
static Bool called_LbxNoDelta;


static void
InitializeOptionsList (server, idx, optcode, negotiate, req, reply)
    XServerPtr 	server;
    int		idx;
    CARD8	optcode;
    Bool	negotiate;
    int		(*req)();
    int		(*reply)();
{
    server->LbxOptions[idx].optcode   = optcode;
    server->LbxOptions[idx].negotiate = negotiate;
    server->LbxOptions[idx].req	      = req;
    server->LbxOptions[idx].reply     = reply;
}

static void
InitializeDeltaOptions (server)
    XServerPtr server;
{
    server->proxyDeltaOpt.minN		= 0;
    server->proxyDeltaOpt.maxN		= 0xff;
    server->proxyDeltaOpt.prefN		= LBX_OPT_DELTA_NCACHE_DFLT;
    server->proxyDeltaOpt.minMaxMsgLen  = LBX_OPT_DELTA_MSGLEN_MIN >> 2;
    server->proxyDeltaOpt.maxMaxMsgLen  = 0xff;
    server->proxyDeltaOpt.prefMaxMsgLen = LBX_OPT_DELTA_MSGLEN_DFLT >> 2;

    server->serverDeltaOpt.minN		 = 0;
    server->serverDeltaOpt.maxN		 = 0xff;
    server->serverDeltaOpt.prefN	 = LBX_OPT_DELTA_NCACHE_DFLT;
    server->serverDeltaOpt.minMaxMsgLen  = LBX_OPT_DELTA_MSGLEN_MIN >> 2;
    server->serverDeltaOpt.maxMaxMsgLen  = 0xff;
    server->serverDeltaOpt.prefMaxMsgLen = LBX_OPT_DELTA_MSGLEN_DFLT >> 2;
}
    
static void
InitializeStreamComp (server)
    XServerPtr server;
{
    server->LbxStreamComp[0].typelen = ZLIB_STRCOMP_OPT_LEN;
    server->LbxStreamComp[0].type    = ZLIB_STRCOMP_OPT;
    server->LbxStreamComp[0].req     = OptZlibReq;
    server->LbxStreamComp[0].reply   = OptZlibReply;
}

static void
InitializeBitmapCompMethod (server)
    XServerPtr server;
{
    /* compression method name */
    server->LbxBitmapCompMethods[0].methodName	= "XC-FaxG42D";	

    /* inited */
    server->LbxBitmapCompMethods[0].inited	= 0;		

    /* method opcode - to be filled on reply */
    server->LbxBitmapCompMethods[0].methodOpCode= -1;

    /* init function */
    server->LbxBitmapCompMethods[0].compInit	= NULL;

    /* encode function */
    server->LbxBitmapCompMethods[0].compFunc	= LbxImageEncodeFaxG42D;

    /* decode function */
    server->LbxBitmapCompMethods[0].decompFunc	= LbxImageDecodeFaxG42D; 
}

/*ARGSUSED*/
static void
InitializePixmapCompMethod (server)
    XServerPtr server;
{
    /*
     * Currently, we don't support any pixmap compression algorithms
     * because regular stream compression does much better than PackBits.
     * If we want to plug in a better pixmap image compression algorithm,
     * it would go here.
     */

#define NUM_PIXMAP_METHODS 0

}

static void
InitializeCmpaAllMethods (server)
    XServerPtr server;
{
    /* colormap allocation method name */
    server->LbxCmapAllMethods[0].methodName = "XC-CMAP";

    /* rgb resolver */
    server->LbxCmapAllMethods[0].resolver = ResolveColor;

    /* allocator */
    server->LbxCmapAllMethods[0].findfree = FindFreePixel;

    /* matcher */
    server->LbxCmapAllMethods[0].findbest = FindBestPixel;
}

void
LbxOptInit(server)
    XServerPtr server;
{
    bzero((char *)&server->lbxNegOpt, sizeof(LbxNegOptsRec));

    server->lbxNegOpt.proxyDeltaN = LBX_OPT_DELTA_NCACHE_DFLT;
    server->lbxNegOpt.proxyDeltaMaxLen = LBX_OPT_DELTA_MSGLEN_DFLT;
    server->lbxNegOpt.serverDeltaN = LBX_OPT_DELTA_NCACHE_DFLT;
    server->lbxNegOpt.serverDeltaMaxLen = LBX_OPT_DELTA_MSGLEN_DFLT;
    server->lbxNegOpt.numBitmapCompMethods = 0;
    server->lbxNegOpt.bitmapCompMethods = NULL;
    server->lbxNegOpt.numPixmapCompMethods = 0;
    server->lbxNegOpt.pixmapCompMethods = NULL;
    server->lbxNegOpt.squish = TRUE;
    server->lbxNegOpt.useTags = TRUE;

    /*
     * Fill in the server's LbxOptions array.
     */
    InitializeOptionsList (server, 0, LBX_OPT_DELTA_PROXY, TRUE, 
	LbxProxyDeltaReq, LbxProxyDeltaReply);
    InitializeOptionsList (server, 1,  LBX_OPT_DELTA_SERVER, TRUE,  
	LbxServerDeltaReq, LbxServerDeltaReply);
    InitializeOptionsList (server, 2,  LBX_OPT_STREAM_COMP, TRUE,  
	LbxStreamCompReq, LbxStreamCompReply);
    InitializeOptionsList (server, 3,  LBX_OPT_BITMAP_COMP, TRUE,  
	LbxBitmapCompReq, LbxBitmapCompReply);
    InitializeOptionsList (server, 4,  LBX_OPT_PIXMAP_COMP, TRUE,  
	LbxPixmapCompReq, LbxPixmapCompReply);
    InitializeOptionsList (server, 5,  LBX_OPT_MSG_COMP, TRUE,  
	LbxSquishReq, LbxSquishReply);
    InitializeOptionsList (server, 6,  LBX_OPT_USE_TAGS, TRUE,  
	LbxUseTagsReq, LbxUseTagsReply);
    InitializeOptionsList (server, 7,  LBX_OPT_CMAP_ALL,	TRUE,  
	LbxCmapAllReq, LbxCmapAllReply);

    InitializeDeltaOptions (server);

    InitializeStreamComp (server);

    InitializeBitmapCompMethod (server);

    InitializePixmapCompMethod (server);

    InitializeCmpaAllMethods (server);

    /*
     * Now update this server's LbxOptions that were set when
     * command line was parsed.
     */
    if (called_LbxNoComp)
        server->LbxOptions[OPT_INDEX_STREAM_COMP].negotiate = FALSE;
    if (called_LbxNoDelta) {
        server->proxyDeltaOpt.prefN = 0;
        server->serverDeltaOpt.prefN = 0;
    }
}

int
LbxOptBuildReq(server, buf)
    XServerPtr server;
    register char *buf;
{
    int		  i;
    char *bufstart = buf;
    char *pnopts = buf++;

    server->optcount = 0;

    for (i = 0; i < (sizeof(server->LbxOptions) / sizeof (LbxOptionsListRec)); 
			i++) {
	int reqlen;

	if (server->LbxOptions[i].negotiate) {
	    reqlen = (*server->LbxOptions[i].req) (server, 
						   buf + LBX_OPT_SMALLHDR_LEN);
	    *buf++ = server->LbxOptions[i].optcode;
	    *buf++ = LBX_OPT_SMALLHDR_LEN + reqlen;
	    buf += reqlen;
	    server->optlist[server->optcount++] = i;
	}
    }

    *pnopts = server->optcount;
    return (buf - bufstart);
}

int
LbxOptParseReply(server, nopts, preply, replylen)
    XServerPtr server;
    int		  nopts;
    unsigned char *preply;
    int		  replylen;
{
    int		  i;

    for (i = 0; i < nopts; i++) {
	int len;
	int hdrlen;
	int result;
	int optindex = *preply;

	if (optindex >= server->optcount)
	    return -1;
	LBX_OPT_DECODE_LEN(preply + 1, len, hdrlen);
	if (len < ++hdrlen || len > replylen) {
#ifdef OPTDEBUG
	    fprintf(stderr, "bad length, len = %d, hdrlen = %d, optlen = %d\n",
			    len, hdrlen, replylen);
#endif
	    return -1;
	}

	result = (*server->LbxOptions[server->optlist[optindex]].reply) (server,
				preply + hdrlen, len - hdrlen);
	if (result < 0)
	    return -1;

	preply += len;
	replylen -= len;
    }
    return 0;
}

void
LbxNoDelta()
{
    /* 
     * All we can do now is to flag that this was called
     * since the associated server structure may not be
     * initialized.
     */
    called_LbxNoDelta = TRUE;
}

static int
LbxProxyDeltaReq(server, buf)
    XServerPtr server;
    unsigned char *buf;
{
    memcpy(buf, (char *)&server->proxyDeltaOpt, sizeof(LbxDeltaOptionsRec));
    return LBX_OPT_DELTA_REQLEN;
}

static int
LbxServerDeltaReq(server, buf)
    XServerPtr server;
    unsigned char *buf;
{
    memcpy(buf, (char *)&server->serverDeltaOpt, sizeof(LbxDeltaOptionsRec));
    return LBX_OPT_DELTA_REQLEN;
}

static int
LbxProxyDeltaReply(server, preply, replylen)
    XServerPtr server;
    unsigned char *preply;
    int		  replylen;
{
    return LbxDeltaReply(preply, 
			 replylen,
			 &server->lbxNegOpt.proxyDeltaN,
			 &server->lbxNegOpt.proxyDeltaMaxLen);
}

static int
LbxServerDeltaReply(server, preply, replylen)
    XServerPtr server;
    unsigned char *preply;
    int		  replylen;
{
    return LbxDeltaReply(preply, 
			 replylen,
			 &server->lbxNegOpt.serverDeltaN,
			 &server->lbxNegOpt.serverDeltaMaxLen);
}

static int
LbxDeltaReply(preply, replylen, pn, pmaxlen)
    unsigned char *preply;
    int		  replylen;
    short	  *pn;
    short	  *pmaxlen;
{
    if (replylen < 2)
	return -1;
    *pn = *preply++;
    *pmaxlen = *preply;
    if ((*pmaxlen <<= 2) == 0)
	*pn = 0;
    else if (*pmaxlen < 32) {
#ifdef DEBUG
	printf("bad delta max msg length %d\n", *pmaxlen);
#endif
	return -1;
     }
    return 0;
}


void
LbxNoComp()
{
    /* 
     * All we can do now is to flag that this was called
     * since the associated server structure may not be
     * initialized.
     */
    called_LbxNoComp = TRUE;
}

static int
LbxStreamCompReq(server, buf)
    XServerPtr server;
    unsigned char *buf;
{
    int		  i;
    int		  reqlen;
    int		  LBX_N_STRCOMP;
    
    LBX_N_STRCOMP = sizeof (server->LbxStreamComp) /
		    sizeof (LbxStreamCompRec);

    *buf++ = LBX_N_STRCOMP;
    reqlen = 1;

    for (i = 0; i < LBX_N_STRCOMP; i++) {
	int optdatalen;
	int typelen = server->LbxStreamComp[i].typelen;

	*buf++ = typelen;
	memcpy(buf, server->LbxStreamComp[i].type, typelen);
	buf += typelen;
	reqlen += 1 + typelen;

	optdatalen = (*server->LbxStreamComp[i].req) (server, buf + 1);
	*buf = optdatalen + 1;
	buf += optdatalen + 1;
	reqlen += optdatalen + 1;
    }

    return reqlen;
}

static int
LbxStreamCompReply(server, preply, replylen)
    XServerPtr server;
    unsigned char *preply;
    int		  replylen;
{
    int		  optindex;
    int		  LBX_N_STRCOMP;
    
    LBX_N_STRCOMP = sizeof(server->LbxStreamComp) /
		    sizeof (LbxStreamCompRec);

    if (replylen < 1 || (optindex = *preply) >= LBX_N_STRCOMP)
	return -1;
    return (*server->LbxStreamComp[optindex].reply) (server, 
						     preply + 1, 
						     replylen - 1);
	
}

extern int  zlevel;

/*ARGSUSED*/
static int
OptZlibReq(server, buf)
    XServerPtr server;
    unsigned char *buf;
{
    *buf++ = 1; /* len */
    *buf = zlevel;
    return (2);
}

/*ARGSUSED*/
static int
OptZlibReply(server, preply, replylen)
    XServerPtr server;
    unsigned char *preply;
    int		  replylen;
{
    server->lbxNegOpt.streamOpts.streamCompInit =
	(LbxStreamCompHandle (*)(int, pointer))ZlibInit;
    server->lbxNegOpt.streamOpts.streamCompArg = (pointer)(long) zlevel;
    server->lbxNegOpt.streamOpts.streamCompStuffInput = ZlibStuffInput;
    server->lbxNegOpt.streamOpts.streamCompInputAvail = ZlibInputAvail;
    server->lbxNegOpt.streamOpts.streamCompFlush = ZlibFlush;
    server->lbxNegOpt.streamOpts.streamCompRead = ZlibRead;
    server->lbxNegOpt.streamOpts.streamCompWriteV = ZlibWriteV;
    server->lbxNegOpt.streamOpts.streamCompOn = ZlibCompressOn;
    server->lbxNegOpt.streamOpts.streamCompOff = ZlibCompressOff;
    server->lbxNegOpt.streamOpts.streamCompFreeHandle =
	(void (*)(LbxStreamCompHandle))ZlibFree;

    return 0;
}

Bool lbxDoSquishing = TRUE;

/*ARGSUSED*/
static int
LbxSquishReq(server, buf)
    XServerPtr server;
    unsigned char *buf;
{
    *buf = lbxDoSquishing;
    return 1;
}

static int
LbxSquishReply(server, preply, replylen)
    XServerPtr server;
    unsigned char *preply;
    int		  replylen;
{
    if (replylen < 1)
	return -1;
    server->lbxNegOpt.squish = *preply;
    return 0;
}


Bool lbxUseTags = TRUE;

/*ARGSUSED*/
static int
LbxUseTagsReq(server, buf)
    XServerPtr server;
    unsigned char *buf;
{
    *buf = lbxUseTags;
    return 1;
}


static int
LbxUseTagsReply(server, preply, replylen)
    XServerPtr server;
    unsigned char *preply;
    int		  replylen;
{
    if (replylen < 1)
	return -1;
    server->lbxNegOpt.useTags = *preply;
    return 0;
}


/*
 * Option negotiation for image compression
 */

static int
LbxBitmapCompReq (server, buf)
    XServerPtr server;
    unsigned char *buf;
{
    unsigned char *bufStart = buf;
    int i;
    int NUM_BITMAP_METHODS;

    NUM_BITMAP_METHODS = sizeof(server->LbxBitmapCompMethods) / 
			 sizeof (LbxBitmapCompMethod);

    *buf++ = NUM_BITMAP_METHODS;

    for (i = 0; i < NUM_BITMAP_METHODS; i++)
    {
	int len = strlen (server->LbxBitmapCompMethods[i].methodName);
	*buf++ = len;
	memcpy (buf, server->LbxBitmapCompMethods[i].methodName, len);
	buf += len;
    }

    return (buf - bufStart);
}

/*ARGSUSED*/
static int
LbxBitmapCompReply (server, preply, replylen)
    XServerPtr server;
    unsigned char *preply;
    int	      replylen;
{
    int count = *preply++;
    int i;

    server->lbxNegOpt.numBitmapCompMethods = count;

    if (count > 0)
    {
	server->lbxNegOpt.bitmapCompMethods = (char *) xalloc (count);
	if (server->lbxNegOpt.bitmapCompMethods == NULL)
	{
	    server->lbxNegOpt.numBitmapCompMethods = 0;
	    return -1;
	}
    }

    for (i = 0; i < count; i++)
    {
	int index = *preply++;
	server->lbxNegOpt.bitmapCompMethods[i] = index;
	server->LbxBitmapCompMethods[index].methodOpCode = *preply++;
    }

    return 0;
}


static int
LbxPixmapCompReq (server, buf)
    XServerPtr server;
    unsigned char *buf;
{
    unsigned char *bufStart = buf;
    int i, j;

    *buf++ = NUM_PIXMAP_METHODS;

    for (i = 0; i < NUM_PIXMAP_METHODS; i++)
    {
	int len = strlen (server->LbxPixmapCompMethods[i].methodName);
	*buf++ = len;
	memcpy (buf, server->LbxPixmapCompMethods[i].methodName, len);
	buf += len;
	*buf++ = server->LbxPixmapCompMethods[i].formatMask;
	*buf++ = server->LbxPixmapCompMethods[i].depthCount;
	for (j = 0; j < server->LbxPixmapCompMethods[i].depthCount; j++)
	    *buf++ = server->LbxPixmapCompMethods[i].depths[j];
    }

    return (buf - bufStart);
}

/*ARGSUSED*/
static int
LbxPixmapCompReply (server, preply, replylen)
    XServerPtr server;
    unsigned char *preply;
    int	      replylen;
{
    int count = *preply++;
    int i, j;

    server->lbxNegOpt.numPixmapCompMethods = count;

    if (count > 0)
    {
	server->lbxNegOpt.pixmapCompMethods = (char *) xalloc (count);
	if (server->lbxNegOpt.pixmapCompMethods == NULL)
	{
	    server->lbxNegOpt.numPixmapCompMethods = 0;
	    return -1;
	}
    }

    for (i = 0; i < count; i++)
    {
	int index = *preply++;
	server->lbxNegOpt.pixmapCompMethods[i] = index;
	server->LbxPixmapCompMethods[index].methodOpCode = *preply++;
	server->LbxPixmapCompMethods[index].formatMask = *preply++;
	server->LbxPixmapCompMethods[index].depthCount = *preply++;
	for (j = 0; j < server->LbxPixmapCompMethods[index].depthCount; j++)
	    server->LbxPixmapCompMethods[index].depths[j] = *preply++;
    }

    return 0;
}


LbxBitmapCompMethod *
LbxLookupBitmapCompMethod (server, methodOpCode)
    XServerPtr server;
    int methodOpCode;

{
    int i;

    for (i = 0; i < server->lbxNegOpt.numBitmapCompMethods; i++)
    {
	LbxBitmapCompMethod *method;

	method = &server->LbxBitmapCompMethods[(int)(server->lbxNegOpt.bitmapCompMethods[i])];

	if (method->methodOpCode == methodOpCode)
	    return (method);
    }

    return (NULL);
}


LbxPixmapCompMethod *
LbxLookupPixmapCompMethod (server, methodOpCode)
    XServerPtr server;
    int methodOpCode;

{
    int i;

    for (i = 0; i < server->lbxNegOpt.numPixmapCompMethods; i++)
    {
	LbxPixmapCompMethod *method;

	method = &server->LbxPixmapCompMethods[(int)(server->lbxNegOpt.pixmapCompMethods[i])];

	if (method->methodOpCode == methodOpCode)
	    return (method);
    }

    return (NULL);
}


LbxBitmapCompMethod *
LbxFindPreferredBitmapCompMethod (server)
    XServerPtr server;

{
    if (server->lbxNegOpt.numBitmapCompMethods == 0)
	return NULL;
    else
	return (&server->LbxBitmapCompMethods[(int)(server->lbxNegOpt.bitmapCompMethods[0])]);
}


LbxPixmapCompMethod *
LbxFindPreferredPixmapCompMethod (server, format, depth)
    XServerPtr server;
    int format;
    int depth;

{
    if (server->lbxNegOpt.numPixmapCompMethods == 0)
	return NULL;
    else
    {
	LbxPixmapCompMethod *method;
	int i, j;

	for (i = 0; i < server->lbxNegOpt.numPixmapCompMethods; i++)
	{
	    method = &server->LbxPixmapCompMethods[(int)(server->lbxNegOpt.pixmapCompMethods[i])];

	    if ((method->formatMask & (1 << format)))
	    {
		for (j = 0; j < method->depthCount; j++)
		    if (depth == method->depths[j])
			return method;
	    }
	}

	return NULL;
    }
}

/*
 * Colormap methods
 */
static int
LbxCmapAllReq (server, buf)
    XServerPtr server;
    unsigned char *buf;
{
    unsigned char *bufStart = buf;
    int i;
    int NUM_CMAP_METHODS;

    NUM_CMAP_METHODS = sizeof (server->LbxCmapAllMethods) /
		       sizeof (LbxCmapAllMethod);

    *buf++ = NUM_CMAP_METHODS;

    for (i = 0; i < NUM_CMAP_METHODS; i++)
    {
	int len = strlen (server->LbxCmapAllMethods[i].methodName); 
	*buf++ = len;
	memcpy (buf, server->LbxCmapAllMethods[i].methodName, len);
	buf += len;
    }

    return (buf - bufStart);
}

static int
LbxCmapAllReply(server, preply, replylen)
    XServerPtr server;
    unsigned char *preply;
    int		  replylen;
{
    int NUM_CMAP_METHODS;

    NUM_CMAP_METHODS = sizeof (server->LbxCmapAllMethods) /
		       sizeof (LbxCmapAllMethod);

    if (replylen < 1)
	return -1;
    if (*preply < NUM_CMAP_METHODS) {
	LbxResolveColor  = server->LbxCmapAllMethods[*preply].resolver;
	LbxFindFreePixel = server->LbxCmapAllMethods[*preply].findfree;
	LbxFindBestPixel = server->LbxCmapAllMethods[*preply].findbest;
    }
    return 0;
}
