/* $Xorg: tags.c,v 1.4 2001/02/09 02:05:32 xorgcvs Exp $ */
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
 * Copyright 1993 Network Computing Devices, Inc.
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

#include	"misc.h"
#include	"util.h"
#include	"cache.h"
#include	"tags.h"
#include	"assert.h"
#include	"wire.h"
#include	"proxyopts.h"

int         lbxTagCacheSize = 1 << 20;

void
TagsInit(server, useTags)
    XServerPtr server;
    Bool useTags;
{
    if (!useTags) {
	lbxTagCacheSize = 0;
    }
    server->global_cache = CacheInit(server, lbxTagCacheSize);
    server->prop_cache = CacheInit(server, lbxTagCacheSize);
}

void
FreeTags()
{
    int 	i;

    for (i=0; i < lbxMaxServers; i++) {
	if (servers[i] && servers[i]->caches[servers[i]->global_cache])
	    CacheFreeCache(servers[i], servers[i]->global_cache);
	if (servers[i] && servers[i]->caches[servers[i]->prop_cache])
	    CacheFreeCache(servers[i], servers[i]->prop_cache);
    }
}

/* ARGSUSED */
static void
cache_free(id, client, data, reason)
    CacheID     id;
    ClientPtr	client;
    pointer     data;
    int         reason;
{
    TagData     tag = (TagData) data;

    /* tell server we toasted this one */
    if (reason != CacheEntryFreed)
	SendInvalidateTag(client, tag->tid);
    xfree(tag->tdata);
    xfree(data);
}

Bool
TagStoreData(server, cache, id, size, dtype, data)
    XServerPtr  server;
    Cache       cache;
    CacheID     id;
    int         size;
    int         dtype;
    pointer     data;
{
    pointer	tdata;

    tdata = (pointer) xalloc(size);
    if (!tdata)
	return FALSE;
    memcpy((char *) tdata, (char *) data, size);
    return TagStoreDataNC(server, cache, id, size, dtype, tdata);
}

Bool
TagStoreDataNC(server, cache, id, size, dtype, data)
    XServerPtr  server;
    Cache       cache;
    CacheID     id;
    int         size;
    int         dtype;
    pointer     data;
{
    TagData     tag;
    Bool	ret;

    assert(server->lbxNegOpt.useTags);

    tag = (TagData) xalloc(sizeof(TagDataRec));
    if (!tag)
	return FALSE;
    tag->tdata = data;
    tag->tid = id;
    tag->data_type = dtype;
    tag->size = size;

    ret = CacheStoreMemory(server, cache, id, (pointer) tag, size, cache_free,
			   !AnyTagBearingReplies(server, cache));
    if (!ret) {
    	xfree(tag->tdata);
        xfree(tag);
    }
    return ret;
}

TagData
TagGetTag(server, cache, id)
    XServerPtr	server;
    Cache       cache;
    CacheID     id;
{
    TagData     tag;

    assert(server->lbxNegOpt.useTags);

    tag = (TagData) CacheFetchMemory(server, cache, id, TRUE);
    return tag;
}

pointer
TagGetData(server, cache, id)
    XServerPtr	server;
    Cache       cache;
    CacheID     id;
{
    TagData     tag;

    assert(server->lbxNegOpt.useTags);

    tag = (TagData) CacheFetchMemory(server, cache, id, TRUE);
    if (tag)
	return tag->tdata;
    else
	return (pointer) NULL;
}

void
TagFreeData(server, cache, id, notify)
    XServerPtr	server;
    Cache       cache;
    CacheID     id;
    Bool        notify;
{
    assert(server->lbxNegOpt.useTags);

    CacheFreeMemory(server, cache, id, notify);
}
