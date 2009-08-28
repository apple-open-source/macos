/* $XdotOrg: $ */
/* $Xorg: cache.c,v 1.4 2001/02/09 02:05:42 xorgcvs Exp $ */
/*
Copyright 1987, 1998  The Open Group

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
 * @(#)cache.c	4.2	91/05/02
 *
 */
/* $XFree86: xc/programs/xfs/difs/cache.c,v 1.6 2001/12/14 20:01:33 dawes Exp $ */

#include	"cachestr.h"
#include	"misc.h"

#define INITBUCKETS 64
#define INITHASHSIZE 6
#define MAXHASHSIZE 11


#define	ENTRYOFFSET		22
#define CACHE_ENTRY_MASK	0x3FFFFF
#define	CACHE_ENTRY_BITS(id)	((id) & 0x1fc00000)
#define	CACHE_ID(id)		((int)(CACHE_ENTRY_BITS(id) >> ENTRYOFFSET))

#define	NullCacheEntry	((CacheEntryPtr) 0)

#define	MAX_NUM_CACHES	32
/* XXX make this dynamic? */
static CachePtr caches[MAX_NUM_CACHES];
static int  num_caches = 1;

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
CacheInit(unsigned long maxsize)
{
    Cache       id = (Cache) num_caches;
    CachePtr    cache;

    cache = (CachePtr) fsalloc(sizeof(CacheRec));
    if (!cache)
	return (Cache) 0;
    cache->entries = (CacheEntryPtr *)
	fsalloc(INITBUCKETS * sizeof(CacheEntryPtr));
    bzero((char *) cache->entries, (INITBUCKETS * sizeof(CacheEntryPtr)));
    if (!cache->entries) {
	fsfree(cache);
	return (Cache) 0;
    }
    caches[id] = cache;
    cache->elements = 0;
    cache->buckets = INITBUCKETS;
    cache->hashsize = INITHASHSIZE;
    cache->maxsize = maxsize;
    cache->cursize = 0;
    cache->nextid = id << ENTRYOFFSET;
    cache->id = id;
    num_caches++;
    return id;
}

static int
hash(CacheID cid)
{
    CachePtr    cache = caches[CACHE_ID(cid)];

    switch (cache->hashsize) {
#ifdef DEBUG	/* only need this if INITHASHSIZE < 6 */
    case 2:
	return ((int) (0x03 & (cid ^ (cid >> 2) ^ (cid >> 8))));
    case 3:
	return ((int) (0x07 & (cid ^ (cid >> 3) ^ (cid >> 9))));
    case 4:
	return ((int) (0x0F & (cid ^ (cid >> 4) ^ (cid >> 10))));
    case 5:
	return ((int) (0x01F & (cid ^ (cid >> 5) ^ (cid >> 11))));
#endif
    case 6:
	return ((int) (0x03F & (cid ^ (cid >> 6) ^ (cid >> 12))));
    case 7:
	return ((int) (0x07F & (cid ^ (cid >> 7) ^ (cid >> 13))));
    case 8:
	return ((int) (0x0FF & (cid ^ (cid >> 8) ^ (cid >> 16))));
    case 9:
	return ((int) (0x1FF & (cid ^ (cid >> 9))));
    case 10:
	return ((int) (0x3FF & (cid ^ (cid >> 10))));
    case 11:
	return ((int) (0x7FF & (cid ^ (cid >> 11))));
    }
    return -1;
}

static void
rebuild_cache(CachePtr cache)
{
    int         j;
    CacheEntryPtr cp,
                next,
              **tails,
               *entries,
              **tptr,
               *cptr;

    assert(cache);
    j = 2 * cache->buckets;
    tails = (CacheEntryPtr **) ALLOCATE_LOCAL(j * sizeof(CacheEntryPtr *));
    if (!tails)
	return;
    entries = (CacheEntryPtr *) fsalloc(j * sizeof(CacheEntryPtr));
    if (!entries) {
	DEALLOCATE_LOCAL(tails);
	return;
    }
    for (cptr = entries, tptr = tails; --j >= 0; cptr++, tptr++) {
	*cptr = NullCacheEntry;
	*tptr = cptr;
    }
    cache->hashsize++;
    for (j = cache->buckets, cptr = cache->entries; --j >= 0; cptr++) {
	for (cp = *cptr; cp; cp = next) {
	    next = cp->next;
	    cp->next = NullCacheEntry;
	    tptr = &tails[hash(cp->id)];
	    **tptr = cp;
	    *tptr = &cp->next;
	}
    }
    DEALLOCATE_LOCAL(tails);
    cache->buckets *= 2;
    fsfree(cache->entries);
    cache->entries = entries;
}

/*
 * throws out all existing entries
 */
void
CacheReset(void)
{
    CacheEntryPtr cp;
    CachePtr    cache;
    int         i,
                j;

    for (j = 0; j < num_caches; j++) {
	cache = caches[j];
	if (!cache)
	    continue;
	for (i = 0; i < cache->buckets; i++) {
	    for (cp = cache->entries[i]; cp; cp = cp->next) {
		cache->elements--;
		cache->cursize -= cp->size;
		(*cp->free_func) (cp->id, cp->data, CacheWasReset);
		fsfree(cp);
	    }
	    cache->entries[i] = (CacheEntryPtr) 0;
	}
	assert(cache->cursize == 0);
    }
}

static void
flush_cache(CachePtr cache, unsigned long needed)
{
/* XXX -- try to set oldprev properly inside search loop */
    CacheEntryPtr cp,
                oldest,
               *oldprev;
    int         oldbucket = -1,
                i;

    while ((cache->cursize + needed) > cache->maxsize) {
	oldest = (CacheEntryPtr) 0;
	/* find oldest */
	for (i = 0; i < cache->buckets; i++) {
	    cp = cache->entries[i];
	    if (!cp)
		continue;
	    if (!oldest) {
		oldbucket = i;
		oldest = cp;
	    }
	    while (cp) {
		if (cp->timestamp < oldest->timestamp) {
		oldest = cp;
		    oldbucket = i;
		}
		cp = cp->next;
	    }
	}
	/* fixup list */
	oldprev = &cache->entries[oldbucket];
	cp = *oldprev;
	for (; (cp = *oldprev) != 0; oldprev = &cp->next) {
	    if (cp == oldest) {
		*oldprev = oldest->next;
		break;
	    }
	}
	/* clobber it */
	cache->elements--;
	cache->cursize -= oldest->size;
	(*oldest->free_func) (oldest->id, oldest->data, CacheEntryOld);
	fsfree(oldest);
    }
}

void
CacheResize(Cache cid, unsigned newsize)
{
    CachePtr    cache = caches[cid];

    if (!cache)
    	return;

    if (newsize < cache->maxsize) {
	/* have to toss some stuff */
	flush_cache(cache, cache->maxsize - newsize);
    }
    cache->maxsize = newsize;
}

CacheID
CacheStoreMemory(
    Cache       cid,
    pointer     data,
    unsigned long size,
    CacheFree   free_func)
{
    CacheID     id;
    CacheEntryPtr cp,
               *head;
    CachePtr    cache = caches[cid];

    if (size > cache->maxsize)	/* beyond cache limits */
	return (CacheID) 0;

    if ((cache->elements >= 4 * cache->buckets) &&
	    (cache->hashsize < MAXHASHSIZE)) {
	rebuild_cache(cache);
    }
    id = cache->nextid++;

    if ((cache->cursize + size) > cache->maxsize) {
	flush_cache(cache, size);
    }
    head = &cache->entries[hash(id)];
    cp = (CacheEntryPtr) fsalloc(sizeof(CacheEntryRec));
    if (!cp) {
	return (CacheID) 0;
    }
    cp->next = *head;
    cp->id = id;
    cp->timestamp = GetTimeInMillis();
    cp->free_func = free_func;
    cp->size = size;
    cp->data = data;
    cache->cursize += size;
    cache->elements++;
    *head = cp;

    return id;
}

pointer
CacheFetchMemory(
    CacheID     cid,
    Bool        update)
{
    CachePtr    cache = caches[CACHE_ID(cid)];
    CacheEntryPtr cp,
               *head;

    head = &cache->entries[hash(cid)];
    for (cp = *head; cp; cp = cp->next) {
	if (cp->id == cid) {
	    if (update) {
		cp->timestamp = GetTimeInMillis();
		if (cp != *head) {	/* put it in the front */
		    cp->next = *head;
		    *head = cp;
		}
	    }
	    return cp->data;
	}
    }
    return (pointer) 0;
}

void
CacheFreeMemory(
    CacheID     cid,
    Bool        notify)
{
    CachePtr    cache = caches[CACHE_ID(cid)];
    CacheEntryPtr cp,
               *prev,
               *head;
    int        *elptr;
    int         elements;
    Bool        found = FALSE;

    head = &cache->entries[hash(cid)];
    elptr = &cache->elements;
    prev = head;
    while ((cp = *prev) != NullCacheEntry) {
	if (cp->id == cid) {
	    *prev = cp->next;
	    elements = --*elptr;
	    if (notify) {
		(*(cp->free_func)) (cid, cp->data, CacheEntryFreed);
	    }
	    cache->cursize -= cp->size;
	    fsfree(cp);
	    if (*elptr != elements)
		prev = head;
	    found = TRUE;
	} else {
	    prev = &cp->next;
	}
    }
    if (!found)
	FatalError("freeing cache entry %d which isn't there\n", cid);
}

/* ARGSUSED */
void
CacheSimpleFree(
    CacheID     cid,
    pointer     data,
    int         reason)
{
    fsfree(data);
}
