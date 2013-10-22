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

#include "ssl.h"
#include "sslMemory.h"
#include "sslDebug.h"
#include "appleSession.h"

#include <CoreFoundation/CFDate.h>
#include <pthread.h>
#include <string.h>

#include <utilities/SecIOFormat.h>

/* default time-to-live in cache, in seconds */
#define QUICK_CACHE_TEST	0
#if		QUICK_CACHE_TEST
#define SESSION_CACHE_TTL	((CFTimeInterval)5)
#else
#define SESSION_CACHE_TTL	((CFTimeInterval)(10 * 60))
#endif	/* QUICK_CACHE_TEST */

#define CACHE_PRINT			0
#if		CACHE_PRINT
#define DUMP_ALL_CACHE		0

static void cachePrint(
	const void *entry,
	const SSLBuffer *key,
	const SSLBuffer *data)
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

/*
 * One entry (value) in SessionCache.
 */
typedef struct SessionCacheEntry SessionCacheEntry;
struct SessionCacheEntry {
    /* Linked list of SessionCacheEntries. */
    SessionCacheEntry *next;

	SSLBuffer		mKey;
	SSLBuffer		mSessionData;

	/* this entry to be removed from session map at this time */
	CFAbsoluteTime	mExpiration;
};

/*
 * Note: the caller passes in the expiration time solely to accomodate the
 * instantiation of a single const Time::Interval for use in calculating
 * TTL. This const, SessionCache.mTimeToLive, is in the singleton gSession Cache.
 */
/*
 * This constructor, the only one, allocs copies of the key and value
 * SSLBuffers.
 */
static SessionCacheEntry *SessionCacheEntryCreate(
	const SSLBuffer *key,
	const SSLBuffer *sessionData,
	CFAbsoluteTime expirationTime)
{
    OSStatus serr;

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
    const SSLBuffer *key)
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
    CFAbsoluteTime now)
{
	return now > entry->mExpiration;
}

/* has this expired? */
static bool SessionCacheEntryIsStaleNow(SessionCacheEntry *entry)
{
	return SessionCacheEntryIsStale(entry, CFAbsoluteTimeGetCurrent());
}

/* replace existing mSessionData */
static OSStatus SessionCacheEntrySetSessionData(SessionCacheEntry *entry,
	const SSLBuffer *data)
{
	SSLFreeBuffer(&entry->mSessionData);
	return SSLCopyBuffer(data, &entry->mSessionData);
}

/*
 * Global list of sessions and associated state. We maintain a singleton of
 * this.
 */
typedef struct SessionCache {
    SessionCacheEntry *head;
    CFTimeInterval mTimeToLive;		/* default time-to-live in seconds */
} SessionCache;

static pthread_mutex_t gSessionCacheLock = PTHREAD_MUTEX_INITIALIZER;
static SessionCache *gSessionCache = NULL;

static void SessionCacheInit(void) {
    gSessionCache = sslMalloc(sizeof(SessionCache));
    gSessionCache->head = NULL;
    gSessionCache->mTimeToLive = SESSION_CACHE_TTL;
}

static SessionCache *SessionCacheGetLockedInstance(void) {
    pthread_mutex_lock(&gSessionCacheLock);
    if (!gSessionCache) {
        /* We could use pthread_once, but we already have a mutex for other
           reasons. */
        SessionCacheInit();
    }

    return gSessionCache;
}

/* these three correspond to the C functions exported by this file */
static OSStatus SessionCacheAddEntry(
    SessionCache *cache,
	const SSLBuffer *sessionKey,
	const SSLBuffer *sessionData,
	uint32_t timeToLive)			/* optional time-to-live in seconds; 0 ==> default */
{
    SessionCacheEntry *entry = NULL;
    SessionCacheEntry **current;
	CFTimeInterval expireTime;

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
                return errSecSuccess;
            }
            else {
                sslLogSessCacheDebug("SessionCache::addEntry CACHE REPLACE "
                    "entry = %p", entry);
                return SessionCacheEntrySetSessionData(entry, sessionData);
            }
        }
    }

	expireTime = CFAbsoluteTimeGetCurrent();
	if(timeToLive) {
		/* caller-specified */
		expireTime += (CFTimeInterval)timeToLive;
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

	return errSecSuccess;
}

static OSStatus SessionCacheLookupEntry(
    SessionCache *cache,
	const SSLBuffer *sessionKey,
	SSLBuffer *sessionData)
{
    SessionCacheEntry *entry = NULL;
    SessionCacheEntry **current;
	for (current = &(cache->head); *current; current = &((*current)->next)) {
        entry = *current;
		if (SessionCacheEntryMatchKey(entry, sessionKey))
            break;
    }

	if (*current == NULL)
		return errSSLSessionNotFound;

	if (SessionCacheEntryIsStaleNow(entry)) {
		sslLogSessCacheDebug("SessionCache::lookupEntry %p: STALE "
			"entry, deleting; current %p, entry->next %p",
			entry, current, entry->next);
		cachePrint(entry, sessionKey, &entry->mSessionData);
        *current = entry->next;
        SessionCacheEntryDelete(entry);
		return errSSLSessionNotFound;
	}

	/* alloc/copy sessionData from existing entry (caller must free) */
	return SSLCopyBuffer(&entry->mSessionData, sessionData);
}

static OSStatus SessionCacheDeleteEntry(
    SessionCache *cache,
	const SSLBuffer *sessionKey)
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
            return errSecSuccess;
		}
	}

    return errSecSuccess;
}

/* cleanup, delete stale entries */
static bool SessionCacheCleanup(SessionCache *cache)
{
	bool brtn = false;
	CFAbsoluteTime rightNow = CFAbsoluteTimeGetCurrent();
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

/*
 * Store opaque sessionData, associated with opaque sessionKey.
 */
OSStatus sslAddSession (
	const SSLBuffer sessionKey,
	const SSLBuffer sessionData,
	uint32_t timeToLive)			/* optional time-to-live in seconds; 0 ==> default */
{
    SessionCache *cache = SessionCacheGetLockedInstance();
	OSStatus serr;
    if (!cache)
        serr = errSSLSessionNotFound;
    else
    {
        serr = SessionCacheAddEntry(cache, &sessionKey, &sessionData, timeToLive);

        dumpAllCache();
    }

    pthread_mutex_unlock(&gSessionCacheLock);
    return serr;
}

/*
 * Given an opaque sessionKey, alloc & retrieve associated sessionData.
 */
OSStatus sslGetSession (
	const SSLBuffer sessionKey,
	SSLBuffer *sessionData)
{
    SessionCache *cache = SessionCacheGetLockedInstance();
	OSStatus serr;
    if (!cache)
        serr = errSSLSessionNotFound;
    else
    {
        serr = SessionCacheLookupEntry(cache, &sessionKey, sessionData);

        sslLogSessCacheDebug("sslGetSession(%d, %p): %d",
            (int)sessionKey.length, sessionKey.data,
            (int)serr);
        if(!serr) {
            cachePrint(NULL, &sessionKey, sessionData);
        }
        else {
            cachePrint(NULL, &sessionKey, NULL);
        }
        dumpAllCache();
    }

    pthread_mutex_unlock(&gSessionCacheLock);

	return serr;
}

OSStatus sslDeleteSession (
	const SSLBuffer sessionKey)
{
    SessionCache *cache = SessionCacheGetLockedInstance();
	OSStatus serr;
    if (!cache)
        serr = errSSLSessionNotFound;
    else
    {
        serr = SessionCacheDeleteEntry(cache, &sessionKey);
    }

    pthread_mutex_unlock(&gSessionCacheLock);
    return serr;
}

/* cleanup up session cache, deleting stale entries. */
OSStatus sslCleanupSession(void)
{
    SessionCache *cache = SessionCacheGetLockedInstance();
	OSStatus serr = errSecSuccess;
	bool moreToGo = false;

    if (!cache)
        serr = errSSLSessionNotFound;
    else
    {
        moreToGo = SessionCacheCleanup(cache);
    }
	/* Possible TBD: if moreToGo, schedule a timed callback to this function */

    pthread_mutex_unlock(&gSessionCacheLock);
    return serr;
}
