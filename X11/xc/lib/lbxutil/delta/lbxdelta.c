/*
 * $NCDXorg: @(#)lbxdelta.c,v 1.2 1994/01/22 02:23:40 dct Exp $
 * $Xorg: lbxdelta.c,v 1.5 2000/08/17 19:46:40 cpqbld Exp $
 * Copyright 1993 Network Computing Devices
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
 * Author:  Dale Tonogai, Network Computing Devices
 */
/* $XFree86: xc/lib/lbxutil/delta/lbxdelta.c,v 1.8 2001/08/27 19:01:07 dawes Exp $ */

#include "X.h"
#include "Xproto.h"
#include "os.h"
#define _XLBX_SERVER_
#include "lbxstr.h"
#include "lbxdeltastr.h"

#include <stddef.h>

#ifdef LBXREQSTATS
#include "../lbx_zlib/reqstats.h"
extern int LbxWhoAmI;
extern struct ReqStats CoreRequestStats[128];
extern struct ReqStats LbxRequestStats[LbxNumberReqs];

#define LBX_CODE 136 /* XXX - this should not be hardcoded - on todo list */
#endif

/*
 * Allocate data structures needed for doing Delta compaction
 */
int
LBXInitDeltaCache(LBXDeltasPtr	pcache,
		  int		nDeltas,
		  int		maxDeltasize)
{
    int			i;
    unsigned char	*mem;

    if ((pcache->nDeltas = nDeltas)) {
	pcache->maxDeltasize = maxDeltasize;
	if ((pcache->deltas = (LBXDeltaElemPtr)
	    xalloc(pcache->nDeltas * sizeof(LBXDeltaElemRec) +
		pcache->nDeltas * pcache->maxDeltasize)) == NULL) {
	    return -1;
	}

	mem = (unsigned char *) (pcache->deltas + pcache->nDeltas);
	for (i = 0; i < pcache->nDeltas; i++) {
	    pcache->deltas[i].buf = mem;
	    mem += pcache->maxDeltasize;
	}
    }
    else
	pcache->maxDeltasize = 0;

    pcache->nextDelta = 0;
    pcache->activeDeltas = 0;
    return 0;
}

/*
 * Free data structures used for doing Delta compaction
 */
void
LBXFreeDeltaCache(LBXDeltasPtr pcache)
{
    if (pcache->nDeltas && pcache->deltas)
	xfree(pcache->deltas);
}

static int 
BytesDiff(unsigned char *ptr1, unsigned char *ptr2,
	  int  n,
	  int  maxn)
{
    int  result = 0;

    while (n--)
	if (*(ptr1++) != *(ptr2++))
	    if (++result >= maxn)
		break;
    return (result);
}

/*
 * Find the message in the outgoing delta cache with the least number of 
 * differing bytes and return the number of differences.  If all
 * messages have greater than maxdiff differences, return -1.
 */
int
LBXDeltaMinDiffs(LBXDeltasPtr	pcache,
		 unsigned char	*inmsg,
		 int		inmsglen,
		 int		maxdiff,
		 int		*pindex)
{
    int			i, j, k = 0, l = maxdiff + 1;
    int		   	m;
    LBXDeltaElemPtr	dm;

    for (m = pcache->nextDelta-1, dm = &pcache->deltas[m], i = 0;
	    i < pcache->activeDeltas;
	    i++, m--, dm--
	) {
	if (m < 0) {
	    m = pcache->nDeltas - 1;
	    dm = &pcache->deltas[m];
	}
	if (dm->length == inmsglen) {
	    j = BytesDiff(inmsg, dm->buf, inmsglen, l);
	    if (j < l) {
		k = m;
		l = j;
	    }
	}
    }

    if (l > maxdiff)
	return -1;
    else {
	*pindex = k;
	return l;
    }
}

/*
 * Delta compact a given message
 */
void
LBXEncodeDelta(LBXDeltasPtr	pcache,
	       unsigned char	*inmsg,
	       int		ndiff,
	       int		index,
	       unsigned char	*buf)
{
    int			i, off, diff;
    xLbxDiffItem	*deltas = (xLbxDiffItem *)buf;

    for (off = i = 0; i < ndiff; off++) {
	if ((diff = inmsg[off] - pcache->deltas[index].buf[off])) {
	    deltas[i].offset = off;
	    deltas[i++].diff = diff;
	}
    }
}

/*
 * Uncompact a message
 */
int
LBXDecodeDelta(LBXDeltasPtr	pcache,
	       xLbxDiffItem	*deltas,
	       int		ndiff,
	       int		index,
	       unsigned char	**buf)
{
    int			i;
    int			newindex = pcache->nextDelta;
    int			len = pcache->deltas[index].length;
    unsigned char	*p = pcache->deltas[newindex].buf;
#ifdef LBXREQSTATS
    xReq		*req;
#endif

    pcache->nextDelta = (pcache->nextDelta + 1) % pcache->nDeltas;
    if (index != newindex) {
	memcpy(p, pcache->deltas[index].buf, len);
	pcache->deltas[newindex].length = len;
    }
    for (i = 0; i < ndiff; i++)
	p[deltas[i].offset] += deltas[i].diff;
    *buf = p;

#ifdef LBXREQSTATS
    req = (xReq *) p;

    if (LbxWhoAmI == 1)		/* server */
    {
	struct ReqStats *reqStat = NULL;

	if (req->reqType == LBX_CODE)
	    reqStat = &LbxRequestStats[req->data];
	else if (req->reqType < 128)
	    reqStat = &CoreRequestStats[req->reqType];

	if (reqStat)
	{
	    reqStat->delta_count++;
	    reqStat->pre_delta_bytes += (req->length << 2);
	    reqStat->post_delta_bytes +=
		(((sz_xLbxDeltaReq + sz_xLbxDiffItem * ndiff + 3) >> 2) << 2);
	}
    }
#endif

    return len;
}

/*
 * Add a message to the outgoing delta cache
 */
void
LBXAddDeltaOut(LBXDeltasPtr	pcache,
	       unsigned char	*inmsg,
	       int		inmsglen)
{
    memcpy(pcache->deltas[pcache->nextDelta].buf, inmsg, inmsglen);
    pcache->deltas[pcache->nextDelta].length = inmsglen;
    pcache->nextDelta = (pcache->nextDelta + 1) % pcache->nDeltas;
    if (pcache->activeDeltas < pcache->nDeltas)
	pcache->activeDeltas++;
}

/*
 * Add a message to the incoming delta cache
 */
void
LBXAddDeltaIn(LBXDeltasPtr	pcache,
	      unsigned char	*inmsg,
	      int		inmsglen)
{
    memcpy(pcache->deltas[pcache->nextDelta].buf, inmsg, inmsglen);
    pcache->deltas[pcache->nextDelta].length = inmsglen;
    pcache->nextDelta = (pcache->nextDelta + 1) % pcache->nDeltas;
}
