/*
 * Copyright (c) 1999-2001,2005-2008,2010-2012 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*
 *  appleSession.c - Session storage module, Apple CDSA version.
 */

/*
 * The current implementation stores sessions in a linked list, a member of a
 * SessionCache object for which we keep a single global instance. It is
 * expected that at a given time, only a small number of sessions will be
 * cached, so the random insertion access provided by a map<> is unnecessary.
 * New entries are placed in the head of the list, assuming a LIFO usage
 * tendency.
 *
 * Entries in this cache have a time to live of SESSION_CACHE_TTL, currently
 * ten minutes. Entries are tested for being stale upon lookup; also, the global
 * sslCleanupSession() tests all entries in the cache, deleting entries which
 * are stale. This function is currently called whenever an SSLContext is deleted.
 * The current design does not provide any asynchronous timed callouts to perform
 * further cache cleanup; it was decided that the thread overhead of this would
 * outweight the benefits (again assuming a small number of entries in the
 * cache).
 *
 * When a session is added via sslAddSession, and a cache entry already
 * exists for the specifed key (sessionID), the sessionData for the existing
 * cache entry is updated with the new sessionData. The entry's expiration
 * time is unchanged (thus a given session entry can only be used for a finite
 * time no mattter how often it is re-used),
 */

#include <tls_types.h>
#include "sslMemory.h"
#include "appleSession.h"

#include <time.h>
#include <string.h>

/* default time-to-live in cache, in seconds */
#define QUICK_CACHE_TEST	0
#if		QUICK_CACHE_TEST
#define SESSION_CACHE_TTL	((time_t)5)
#else
#define SESSION_CACHE_TTL	((time_t)(10 * 60))
#endif	/* QUICK_CACHE_TEST */

#define CACHE_PRINT			0
#if		CACHE_PRINT
#define DUMP_ALL_CACHE		0

static void cachePrint(
	const void *entry,
	const tls_buffer *key,
	const tls_buffer *data)
{
	printf("entry: %p ", entry);
	unsigned char *kd = key->data;
	if(data != NULL) {
		unsigned char *dd = data->data;
		printf("  key: %02X%02X%02X%02X%02X%02X%02X%02X"
			"  data: %02X%02X%02X%02X... (len %d)\n",
			kd[0],kd[1],kd[2],kd[3], kd[4],kd[5],kd[6],kd[7],
			dd[0],dd[1],dd[2],dd[3], (unsigned)data->length);
	}
	else {
		/* just print key */
		printf("  key: %02X%02X%02X%02X%02X%02X%02X%02X\n",
			kd[0],kd[1],kd[2],kd[3], kd[4],kd[5],kd[6],kd[7]);
	}
}
#else	/* !CACHE_PRINT */
#define cachePrint(e, k, d)
#define DUMP_ALL_CACHE	0
#endif	/* CACHE_PRINT */

#if 	DUMP_ALL_CACHE
static void dumpAllCache(void);
#else
#define dumpAllCache()
#endif


#define sslLogSessCacheDebug(x...)


/*
 * One entry (value) in SessionCache.
 */
struct SessionCacheEntry {
    /* Linked list of SessionCacheEntries. */
    SessionCacheEntry *next;

	tls_buffer      mKey;
	tls_buffer		mSessionData;

	/* this entry to be removed from session map at this time */
	time_t          mExpiration;
};

/*
 * This constructor, the only one, allocs copies of the key and value
 * SSLBuffers.
 */
static SessionCacheEntry *SessionCacheEntryCreate(
	const tls_buffer *key,
	const tls_buffer *sessionData,
	time_t expirationTime)
{
    int serr;

    SessionCacheEntry *entry = sslMalloc(sizeof(SessionCacheEntry));
    if (entry == NULL)
        return NULL;

	serr = SSLCopyBuffer(key, &entry->mKey);
	if(serr) {
        sslFree (entry);
        return NULL;
	}
	serr = SSLCopyBuffer(sessionData, &entry->mSessionData);
	if(serr) {
        SSLFreeBuffer(&entry->mKey);
        sslFree (entry);
        return NULL;
	}

	sslLogSessCacheDebug("SessionCacheEntryCreate(buf,buf) %p", entry);
	entry->mExpiration = expirationTime;

    return entry;
}

static void SessionCacheEntryDelete(SessionCacheEntry *entry)
{
	sslLogSessCacheDebug("~SessionCacheEntryDelete() %p", entry);
	SSLFreeBuffer(&entry->mKey);		// no SSLContext
	SSLFreeBuffer(&entry->mSessionData);
    sslFree(entry);
}

/* basic lookup/match function */
static bool SessionCacheEntryMatchKey(SessionCacheEntry *entry,
    const tls_buffer *key)
{
	if(key->length != entry->mKey.length) {
		return false;
	}
	if((key->data == NULL) || (entry->mKey.data == NULL)) {
		return false;
	}
	return (memcmp(key->data, entry->mKey.data, entry->mKey.length) == 0);
}

static bool SessionCacheEntryIsStale(SessionCacheEntry *entry,
    time_t now)
{
	return now > entry->mExpiration;
}

/* has this expired? */
static bool SessionCacheEntryIsStaleNow(SessionCacheEntry *entry)
{
	return SessionCacheEntryIsStale(entry, time(NULL));
}

/* replace existing mSessionData */
static int SessionCacheEntrySetSessionData(SessionCacheEntry *entry,
	const tls_buffer *data)
{
	SSLFreeBuffer(&entry->mSessionData);
	return SSLCopyBuffer(data, &entry->mSessionData);
}


void SessionCacheInit(SessionCache *cache) {
    cache->head = NULL;
    cache->mTimeToLive = SESSION_CACHE_TTL;
}


/* these three correspond to the C functions exported by this file */
int SessionCacheAddEntry(
    SessionCache *cache,
	const tls_buffer *sessionKey,
	const tls_buffer *sessionData,
	time_t timeToLive)			/* optional time-to-live in seconds; 0 ==> default */
{
    SessionCacheEntry *entry = NULL;
    SessionCacheEntry **current;
	time_t expireTime;

	for (current = &(cache->head); *current; current = &((*current)->next)) {
        entry = *current;
		if (SessionCacheEntryMatchKey(entry, sessionKey)) {
            /* cache hit - just update this entry's sessionData if necessary */
            /* Note we leave expiration time and position in queue unchanged
               - OK? */
            /* What if the entry has already expired? */
            if((entry->mSessionData.length == sessionData->length) &&
               (memcmp(entry->mSessionData.data, sessionData->data,
                sessionData->length) == 0)) {
                /*
                 * These usually match, and a memcmp is a lot cheaper than
                 * a malloc and a free, hence this quick optimization.....
                 */
                sslLogSessCacheDebug("SessionCache::addEntry CACHE HIT "
                    "entry = %p", entry);
                return 0;
            }
            else {
                sslLogSessCacheDebug("SessionCache::addEntry CACHE REPLACE "
                    "entry = %p", entry);
                return SessionCacheEntrySetSessionData(entry, sessionData);
            }
        }
    }

	expireTime = time(NULL);
	if(timeToLive) {
		/* caller-specified */
		expireTime += timeToLive;
	}
	else {
		/* default */
		expireTime += cache->mTimeToLive;
	}
	/* this allocs new copy of incoming sessionKey and sessionData */
	entry = SessionCacheEntryCreate(sessionKey, sessionData, expireTime);

	sslLogSessCacheDebug("SessionCache::addEntry %p", entry);
	cachePrint(entry, sessionKey, sessionData);
	dumpAllCache();

	/* add to head of queue for LIFO caching */
    entry->next = cache->head;
    cache->head = entry;

	return 0;
}

int SessionCacheLookupEntry(
    SessionCache *cache,
	const tls_buffer *sessionKey,
	tls_buffer *sessionData)
{
    SessionCacheEntry *entry = NULL;
    SessionCacheEntry **current;
	for (current = &(cache->head); *current; current = &((*current)->next)) {
        entry = *current;
		if (SessionCacheEntryMatchKey(entry, sessionKey))
            break;
    }

	if (*current == NULL)
		return -9804; //errSSLSessionNotFound;

	if (SessionCacheEntryIsStaleNow(entry)) {
		sslLogSessCacheDebug("SessionCache::lookupEntry %p: STALE "
			"entry, deleting; current %p, entry->next %p",
			entry, current, entry->next);
		cachePrint(entry, sessionKey, &entry->mSessionData);
        *current = entry->next;
        SessionCacheEntryDelete(entry);
		return -9804; //errSSLSessionNotFound;
	}

    /* alloc/copy sessionData from existing entry (caller must free) */
    return SSLCopyBuffer(&entry->mSessionData, sessionData);
}

int SessionCacheDeleteEntry(
    SessionCache *cache,
	const tls_buffer *sessionKey)
{
	SessionCacheEntry **current;

	for (current = &(cache->head); *current; current = &((*current)->next)) {
		SessionCacheEntry *entry = *current;
		if (SessionCacheEntryMatchKey(entry, sessionKey)) {
			#ifndef	DEBUG
			sslLogSessCacheDebug("...SessionCacheDeleteEntry: deleting "
				"cached session (%p)", entry);
			cachePrint(entry, &entry->mKey, &entry->mSessionData);
			#endif
            *current = entry->next;
            SessionCacheEntryDelete(entry);
            return 0;
		}
	}

    return 0;
}

/* cleanup, delete stale entries */
bool SessionCacheCleanup(SessionCache *cache)
{
	bool brtn = false;
	time_t rightNow = time(NULL);
	SessionCacheEntry **current;

	for (current = &(cache->head); *current;) {
		SessionCacheEntry *entry = *current;
		if(SessionCacheEntryIsStale(entry, rightNow)) {
			#ifndef	DEBUG
			sslLogSessCacheDebug("...SessionCacheCleanup: deleting "
				"cached session (%p)", entry);
			cachePrint(entry, &entry->mKey, &entry->mSessionData);
			#endif
            *current = entry->next;
            SessionCacheEntryDelete(entry);
		}
		else {
			current = &((*current)->next);
			/* we're leaving one in the map */
			brtn = true;
		}
	}
	return brtn;
}

/* cleanup, delete stale entries */
void SessionCacheEmpty(SessionCache *cache)
{
    SessionCacheEntry **current;

    for (current = &(cache->head); *current;) {
        SessionCacheEntry *entry = *current;
        *current = entry->next;
        SessionCacheEntryDelete(entry);
    }
}

#if		DUMP_ALL_CACHE
static void dumpAllCache(void)
{
	SessionCache *cache = gSessionCache;
	SessionCacheEntry *entry;

	printf("Contents of sessionCache:\n");
	for(entry = cache->head; entry; entry = entry->next) {
		cachePrint(entry, &entry->mKey, &entry->mSessionData);
	}
}
#endif	/* DUMP_ALL_CACHE */
