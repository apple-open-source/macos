/* $Xorg: cache.c,v 1.5 2001/02/09 02:05:31 xorgcvs Exp $ */
/*
Copyright 1994, 1998  The Open Group

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

 * Copyright 1990, 1991 Network Computing Devices;
 * Portions Copyright 1987 by Digital Equipment Corporation 
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the names of Network Computing Devices,
 * or Digital not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  Network Computing Devices, or Digital
 * make no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * NETWORK COMPUTING DEVICES, AND DIGITAL DISCLAIM ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS, IN NO EVENT SHALL NETWORK COMPUTING DEVICES, OR DIGITAL BE
 * LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */
/* $XFree86: xc/programs/lbxproxy/di/cache.c,v 1.6 2001/12/14 20:00:50 dawes Exp $ */
#include	"misc.h"
#include	"util.h"
#include	"cache.h"
#include	"wire.h"

#define INITBUCKETS 64
#define INITHASHSIZE 6
#define MAXHASHSIZE 11


#define	ENTRYOFFSET		22
#define CACHE_ENTRY_MASK	0x3FFFFF
#define	CACHE_ENTRY_BITS(id)	((id) & 0x1fc00000)
#define	CACHE_ID(id)		((int)(CACHE_ENTRY_BITS(id) >> ENTRYOFFSET))

typedef struct _cache_entry {
    pointer     		data;
    unsigned long 		timestamp;
    CacheID     		id;
    unsigned long 		size;
    CacheFree			free_func;
    ClientPtr			client;	      /* for a server's serverClient */
    struct _cache_entry 	*next;
} CacheEntryRec, *CacheEntryPtr;

typedef struct _cache {
    Cache       	id;
    CacheEntryPtr 	*entries;
    int         	elements;
    int         	buckets;
    int         	hashsize;
    CacheID     	nextid;
    unsigned long 	maxsize;
    unsigned long 	cursize;
} CacheRec;


/*-
 * Notes on cache implementation
 *
 * This is basically the X11 resource code, with some modifications
 * to handle aging.
 *
 * Its currently optimized for lookup & store.  Flushing old stuff
 * is a lot slower than it should probably be, but there's tradeoffs
 * in tuning.
 */

Cache
CacheInit(server, maxsize)
    XServerPtr server;
    unsigned long maxsize;
{
    Cache       id;
    CachePtr    cache;

    cache = (CachePtr) xalloc(sizeof(CacheRec));
    if (!cache)
	return (Cache) 0;
    cache->entries = (CacheEntryPtr *)
	xalloc(INITBUCKETS * sizeof(CacheEntryPtr));
    if (!cache->entries) {
	xfree(cache);
	return (Cache) 0;
    }
    bzero((char *) cache->entries, (INITBUCKETS * sizeof(CacheEntryPtr)));
    id = (Cache) ++server->num_caches;
    server->caches[id] = cache;
    cache->elements = 0;
    cache->buckets = INITBUCKETS;
    cache->hashsize = INITHASHSIZE;
    cache->maxsize = maxsize;
    cache->cursize = 0;
    cache->nextid = id << ENTRYOFFSET;
    cache->id = id;
    return id;
}

void
CacheFreeCache(server, cid)
    XServerPtr server;
    Cache       cid;
{
    CachePtr cache = server->caches[cid];
    int i;
    CacheEntryPtr cp;

    for (i = 0; i < cache->buckets; i++) {
	while ((cp = cache->entries[i])) {
	    cache->entries[i] = cp->next;
	    (*cp->free_func) (cp->id, cp->client, cp->data, CacheWasReset);
	    xfree(cp);
	}
    }
    server->caches[cid] = NULL;
    server->num_caches = 0;
    server->seed = 0;

    xfree(cache->entries);
    xfree(cache);
}

static int
hash(server, cid, id)
    XServerPtr server;
    Cache       cid;
    CacheID     id;
{
    CachePtr    cache = server->caches[cid];

    switch (cache->hashsize) {
#if INITHASHSIZE < 6
    case 2:
	return ((int) (0x03 & (id ^ (id >> 2) ^ (id >> 8))));
    case 3:
	return ((int) (0x07 & (id ^ (id >> 3) ^ (id >> 9))));
    case 4:
	return ((int) (0x0F & (id ^ (id >> 4) ^ (id >> 10))));
    case 5:
	return ((int) (0x01F & (id ^ (id >> 5) ^ (id >> 11))));
#endif
    case 6:
	return ((int) (0x03F & (id ^ (id >> 6) ^ (id >> 12))));
    case 7:
	return ((int) (0x07F & (id ^ (id >> 7) ^ (id >> 13))));
    case 8:
	return ((int) (0x0FF & (id ^ (id >> 8) ^ (id >> 16))));
    case 9:
	return ((int) (0x1FF & (id ^ (id >> 9))));
    case 10:
	return ((int) (0x3FF & (id ^ (id >> 10))));
    case 11:
	return ((int) (0x7FF & (id ^ (id >> 11))));
    }
    return -1;
}

static void
rebuild_cache(server, cache)
    XServerPtr server;
    CachePtr    cache;
{
    int j;
    CacheEntryPtr cp, next, **tails, *entries, **tptr, *cptr;

    j = cache->buckets << 1;
    tails = (CacheEntryPtr **) ALLOCATE_LOCAL(j * sizeof(CacheEntryPtr *));
    if (!tails)
	return;
    entries = (CacheEntryPtr *) xalloc(j * sizeof(CacheEntryPtr));
    if (!entries) {
	DEALLOCATE_LOCAL(tails);
	return;
    }
    for (cptr = entries, tptr = tails; --j >= 0; cptr++, tptr++) {
	*cptr = NULL;
	*tptr = cptr;
    }
    cache->hashsize++;
    for (j = cache->buckets, cptr = cache->entries; --j >= 0; cptr++) {
	for (cp = *cptr; cp; cp = next) {
	    next = cp->next;
	    cp->next = NULL;
	    tptr = &tails[hash(server, cache->id, cp->id)];
	    **tptr = cp;
	    *tptr = &cp->next;
	}
    }
    DEALLOCATE_LOCAL(tails);
    cache->buckets <<= 1;
    xfree(cache->entries);
    cache->entries = entries;
}

static void
flush_cache(cache, needed)
    CachePtr    cache;
    unsigned long needed;
{
    CacheEntryPtr cp, *prev, oldest, *oldprev = NULL;
    int i;

    while ((cache->cursize + needed) > cache->maxsize) {
	oldest = NULL;
	/* find oldest */
	for (i = 0; i < cache->buckets; i++) {
	    for (prev = &cache->entries[i]; (cp = *prev); prev = &cp->next) {
		if (!oldest || (cp->timestamp < oldest->timestamp)) {
		    oldest = cp;
		    oldprev = prev;
		}
	    }
	}
	if (!oldest)
	    break;
	/* clobber it */
	cache->elements--;
	cache->cursize -= oldest->size;
	*oldprev = oldest->next;
	(*oldest->free_func) (oldest->id, oldest->client, oldest->data, CacheEntryOld);
	xfree(oldest);
    }
}

Bool
CacheTrimNeeded(server, cid)
    XServerPtr server;
    Cache       cid;
{
    CachePtr    cache = server->caches[cid];

    return cache->cursize > cache->maxsize;
}

void
CacheTrim(server, cid)
    XServerPtr	server;
    Cache       cid;
{
    flush_cache(server->caches[cid], 0);
}

Bool
CacheStoreMemory(server, cid, id, data, size, free_func, can_flush)
    XServerPtr server;
    Cache       cid;
    CacheID     id;
    pointer     data;
    unsigned long size;
    CacheFree   free_func;
    Bool	can_flush;
{
    CacheEntryPtr cp, *head;
    CachePtr    cache = server->caches[cid];

    if (can_flush && (size > cache->maxsize)) /* beyond cache limits */
	return FALSE;
    cp = (CacheEntryPtr) xalloc(sizeof(CacheEntryRec));
    if (!cp)
	return FALSE;
    if (can_flush && ((cache->cursize + size) > cache->maxsize))
	flush_cache(cache, size);
    if ((cache->elements >= (cache->buckets << 2)) &&
	(cache->hashsize < MAXHASHSIZE))
	rebuild_cache(server, cache);
    head = &cache->entries[hash(server, cid, id)];
    cp->next = *head;
    cp->id = id;
    cp->timestamp = ++server->seed;
    cp->free_func = free_func;
    cp->size = size;
    cp->data = data;
    cp->client = server->serverClient;
    cache->cursize += size;
    cache->elements++;
    *head = cp;
    return TRUE;
}

pointer
CacheFetchMemory(server, cid, id, update)
    XServerPtr server;
    Cache       cid;
    CacheID     id;
    Bool        update;
{
    CachePtr    cache = server->caches[cid];
    CacheEntryPtr cp,
               *head;

    head = &cache->entries[hash(server, cid, id)];
    for (cp = *head; cp; cp = cp->next) {
	if (cp->id == id) {
	    if (update)
		cp->timestamp = ++server->seed;
	    return cp->data;
	}
    }
    return NULL;
}

void
CacheFreeMemory(server, cacheid, cid, notify)
    XServerPtr server;
    Cache       cacheid;
    CacheID     cid;
    Bool        notify;
{
    CachePtr    cache = server->caches[cacheid];
    CacheEntryPtr cp, *prev;

    for (prev = &cache->entries[hash(server, cacheid, cid)];
	 (cp = *prev);
	 prev = &cp->next) {
	if (cp->id == cid) {
	    *prev = cp->next;
	    cache->elements--;
	    cache->cursize -= cp->size;
	    if (notify)
		(*cp->free_func)(cid, cp->client, cp->data, CacheEntryFreed);
	    xfree(cp);
	    break;
	}
    }
}
