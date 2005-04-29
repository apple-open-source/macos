/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
	File:		appleSession.cpp

	Contains:	Session storage module, Apple CDSA version. 

	Written by:	Doug Mitchell

	Copyright: (c) 1999 by Apple Computer, Inc., all rights reserved.

*/

/* 
 * The current implementation stores sessions in a deque<>, a member of a 
 * SessionCache object for which we keep a ModuleNexus-ized instance. It is 
 * expected that at a given time, only a small number of sessions will be 
 * cached, so the random insertion access provided by a map<> is unnecessary. 
 * New entries are placed in the head of the queue, assuming a LIFO usage
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

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/MacErrors.h>

#include <deque>
#include <stdexcept>
#include <security_utilities/threading.h>
#include <security_utilities/globalizer.h>
#include <security_utilities/timeflow.h>

/* time-to-live in cache, in seconds */
#define QUICK_CACHE_TEST	0
#if		QUICK_CACHE_TEST
#define SESSION_CACHE_TTL	((int)5)
#else
#define SESSION_CACHE_TTL	((int)(10 * 60))
#endif	/* QUICK_CACHE_TEST */

#define CACHE_PRINT			0
#if		CACHE_PRINT
#define DUMP_ALL_CACHE		0

static void cachePrint(
	const SSLBuffer *key, 
	const SSLBuffer *data)
{
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
#define cachePrint(k, d)
#define DUMP_ALL_CACHE	0
#endif	/* CACHE_PRINT */

#if 	DUMP_ALL_CACHE
static void dumpAllCache();
#else
#define dumpAllCache()
#endif

/*
 * One entry (value) in SessionCache.  
 */
class SessionCacheEntry {
public:
	/*
	 * This constructor, the only one, allocs copies of the key and value
	 * SSLBuffers.
	 */
	SessionCacheEntry(
		const SSLBuffer &key, 
		const SSLBuffer &sessionData,
		const Time::Absolute &expirationTime);
	~SessionCacheEntry();
		
	/* basic lookup/match function */
	bool			matchKey(const SSLBuffer &key) const;
	
	/* has this expired? */
	bool			isStale();							// calculates "now" 
	bool			isStale(const Time::Absolute &now);	// when you know it
	
	/* key/data accessors */
	SSLBuffer		&key()			{ return mKey; }
	SSLBuffer		&sessionData()	{ return mSessionData; }
	
	/* replace existing mSessionData */
	OSStatus			sessionData(const SSLBuffer &data);
	
private:
	SSLBuffer		mKey;
	SSLBuffer		mSessionData;

	/* this entry to be removed from session map at this time */
	Time::Absolute	mExpiration;
};

/*
 * Note: the caller passes in the expiration time solely to accomodate the 
 * instantiation of a single const Time::Interval for use in calculating
 * TTL. This const, SessionCache.mTimeToLive, is in the singleton gSession Cache.
 */
SessionCacheEntry::SessionCacheEntry(
	const SSLBuffer &key, 
	const SSLBuffer &sessionData,
	const Time::Absolute &expirationTime)
		: mExpiration(expirationTime)
{
	OSStatus serr;
	
	serr = SSLCopyBuffer(key, mKey);
	if(serr) {
		throw runtime_error("memory error");
	}
	serr = SSLCopyBuffer(sessionData, mSessionData);
	if(serr) {
		throw runtime_error("memory error");
	}
	sslLogSessCacheDebug("SessionCacheEntry(buf,buf) this %p", this);
	mExpiration += Time::Interval(SESSION_CACHE_TTL);
}

SessionCacheEntry::~SessionCacheEntry()
{
	sslLogSessCacheDebug("~SessionCacheEntry() this %p", this);
	SSLFreeBuffer(mKey, NULL);		// no SSLContext
	SSLFreeBuffer(mSessionData, NULL);
}

/* basic lookup/match function */
bool SessionCacheEntry::matchKey(const SSLBuffer &key) const
{
	if(key.length != mKey.length) {
		return false;
	}
	if((key.data == NULL) || (mKey.data == NULL)) {
		return false;
	}
	return (memcmp(key.data, mKey.data, mKey.length) == 0);
}
	
/* has this expired? */
bool SessionCacheEntry::isStale()
{
	return isStale(Time::now());
}

bool SessionCacheEntry::isStale(const Time::Absolute &now)
{
	if(now > mExpiration) {
		return true;
	}
	else {
		return false;
	}
}

/* replace existing mSessionData */
OSStatus SessionCacheEntry::sessionData(
	const SSLBuffer &data)
{
	SSLFreeBuffer(mSessionData, NULL);
	return SSLCopyBuffer(data, mSessionData);
}

/* Types for the actual deque and its iterator */
typedef std::deque<SessionCacheEntry *> SessionCacheType;
typedef SessionCacheType::iterator SessionCacheIter;

/* 
 * Global map and associated state. We maintain a singleton of this.
 */
class SessionCache
{
public:
	SessionCache()
	  : mTimeToLive(SESSION_CACHE_TTL) {}
	~SessionCache();
	
	/* these correspond to the C functions exported by this file */
	OSStatus addEntry(
		const SSLBuffer sessionKey, 
		const SSLBuffer sessionData);
	OSStatus lookupEntry(
		const SSLBuffer sessionKey, 
		SSLBuffer *sessionData); 
	OSStatus deleteEntry(
		const SSLBuffer sessionKey);
		
	/* cleanup, delete stale entries */
	bool cleanup();
	SessionCacheType		&sessMap() { return mSessionCache; }
	
private:
	SessionCacheIter lookupPriv(
		const SSLBuffer *sessionKey);
	void deletePriv(
		const SSLBuffer *sessionKey);
	SessionCacheIter deletePriv(
		SessionCacheIter iter);
	SessionCacheType		mSessionCache;
	Mutex					mSessionLock;
	const Time::Interval	mTimeToLive;
};

SessionCache::~SessionCache()
{
	/* free all entries */
	StLock<Mutex> _(mSessionLock);
	for(SessionCacheIter iter = mSessionCache.begin(); iter != mSessionCache.end(); ) {
		iter = deletePriv(iter);
	}
}

/* these three correspond to the C functions exported by this file */
OSStatus SessionCache::addEntry(
	const SSLBuffer sessionKey, 
	const SSLBuffer sessionData)
{
	StLock<Mutex> _(mSessionLock);
	
	SessionCacheIter existIter = lookupPriv(&sessionKey);
	if(existIter != mSessionCache.end()) {
		/* cache hit - just update this entry's sessionData if necessary */
		/* Note we leave expiration time and position in deque unchanged - OK? */
		SessionCacheEntry *existEntry = *existIter;
		SSLBuffer &existBuf = existEntry->sessionData();
		if((existBuf.length == sessionData.length) &&
		   (memcmp(existBuf.data, sessionData.data, sessionData.length) == 0)) {
			/* 
			 * These usually match, and a memcmp is a lot cheaper than 
			 * a malloc and a free, hence this quick optimization.....
			 */
			sslLogSessCacheDebug("SessionCache::addEntry CACHE HIT "
				"entry = %p", existEntry);
			return noErr;
		}
		else {
			sslLogSessCacheDebug("SessionCache::addEntry CACHE REPLACE "
				"entry = %p", existEntry);
			return existEntry->sessionData(sessionData);
		}
	}
	
	/* this allocs new copy of incoming sessionKey and sessionData */
	SessionCacheEntry *entry = new SessionCacheEntry(sessionKey, 
		sessionData,
		Time::now() + mTimeToLive);

	sslLogSessCacheDebug("SessionCache::addEntry %p", entry);
	cachePrint(&sessionKey, &sessionData);
	dumpAllCache();

	/* add to head of queue for LIFO caching */
	mSessionCache.push_front(entry);
	assert(lookupPriv(&sessionKey) != mSessionCache.end());
	return noErr;
}

OSStatus SessionCache::lookupEntry(
	const SSLBuffer sessionKey, 
	SSLBuffer *sessionData)
{
	StLock<Mutex> _(mSessionLock);
	
	SessionCacheIter existIter = lookupPriv(&sessionKey);
	if(existIter == mSessionCache.end()) {
		return errSSLSessionNotFound;
	}
	SessionCacheEntry *entry = *existIter;
	if(entry->isStale()) {
		sslLogSessCacheDebug("SessionCache::lookupEntry %p: STALE "
			"entry, deleting", entry);
		cachePrint(&sessionKey, &entry->sessionData());
		deletePriv(existIter);
		return errSSLSessionNotFound;
	}
	/* alloc/copy sessionData from existing entry (caller must free) */
	return SSLCopyBuffer(entry->sessionData(), *sessionData);
}

OSStatus SessionCache::deleteEntry(
	const SSLBuffer sessionKey)
{
	StLock<Mutex> _(mSessionLock);
	deletePriv(&sessionKey);
	return noErr;
}
	
/* cleanup, delete stale entries */
bool SessionCache::cleanup()
{
	StLock<Mutex> _(mSessionLock);
	bool brtn = false;
	Time::Absolute rightNow = Time::now();
	SessionCacheIter iter;
	
	for(iter = mSessionCache.begin(); iter != mSessionCache.end(); ) {
		SessionCacheEntry *entry = *iter;
		if(entry->isStale(rightNow)) {
			#ifndef	DEBUG
			sslLogSessCacheDebug("...SessionCache::cleanup: deleting "
				"cached session (%p)", entry);
			cachePrint(&entry->key(), &entry->sessionData());
			#endif
			iter = deletePriv(iter);
		}
		else {
			iter++;
			/* we're leaving one in the map */
			brtn = true;
		}
	}
	return brtn;
}

/* private methods, mSessionLock held on entry and exit */
SessionCacheIter SessionCache::lookupPriv(
	const SSLBuffer *sessionKey)
{
	SessionCacheIter it;
	
	for(it = mSessionCache.begin(); it != mSessionCache.end(); it++) {
		SessionCacheEntry *entry = *it;
		if(entry->matchKey(*sessionKey)) {
			return it;
		}
	}
	/* returning map.end() */
	return it;
}

void SessionCache::deletePriv(
	const SSLBuffer *sessionKey)
{
	SessionCacheIter iter = lookupPriv(sessionKey);
	if(iter != mSessionCache.end()) {
		/* 
		 * delete from map 
		 * free underlying SSLBuffer.data pointers
		 * destruct the stored map entry 
		 */
		#if	CACHE_PRINT
		SessionCacheEntry *entry = *iter;
		sslLogSessCacheDebug("SessionCache::deletePriv %p", entry);
		cachePrint(sessionKey, &entry->sessionData());
		dumpAllCache();
		#endif
		deletePriv(iter);
	}
	assert(lookupPriv(sessionKey) == mSessionCache.end());
}

/* common erase, given a SessionCacheIter; returns next iter */
SessionCacheIter SessionCache::deletePriv(
	SessionCacheIter iter)
{
	assert(iter != mSessionCache.end());
	SessionCacheEntry *entry = *iter;
	SessionCacheIter nextIter = mSessionCache.erase(iter);
	delete entry;
	return nextIter;
}

/* the single global thing */
static ModuleNexus<SessionCache> gSessionCache;

#if		DUMP_ALL_CACHE
static void dumpAllCache()
{
	SessionCacheIter it;
	SessionCacheType &smap = gSessionCache().sessMap();
	
	printf("Contents of sessionCache:\n");
	for(it = smap.begin(); it != smap.end(); it++) {
		SessionCacheEntry *entry = *it;
		cachePrint(&entry->key(), &entry->sessionData());
	}
}
#endif	/* DUMP_ALL_CACHE */

/*
 * Store opaque sessionData, associated with opaque sessionKey.
 */
OSStatus sslAddSession (
	const SSLBuffer sessionKey, 
	const SSLBuffer sessionData)
{
	OSStatus serr;
	try {
		serr = gSessionCache().addEntry(sessionKey, sessionData);
	}
	catch(...) {
		serr = unimpErr;
	}
	dumpAllCache();
	return serr;
}

/*
 * Given an opaque sessionKey, alloc & retrieve associated sessionData.
 */
OSStatus sslGetSession (
	const SSLBuffer sessionKey, 
	SSLBuffer *sessionData)
{
	OSStatus serr;
	try {
		serr = gSessionCache().lookupEntry(sessionKey, sessionData);
	}
	catch(...) {
		serr = errSSLSessionNotFound;
	}
	sslLogSessCacheDebug("sslGetSession(%d, %p): %ld", 
		(int)sessionKey.length, sessionKey.data,
		serr);
	if(serr == noErr) {
		cachePrint(&sessionKey, sessionData);
	}
	else {
		cachePrint(&sessionKey, NULL);
	}
	dumpAllCache();
	return serr;
}

OSStatus sslDeleteSession (
	const SSLBuffer sessionKey)
{
	OSStatus serr;
	try {
		serr = gSessionCache().deleteEntry(sessionKey);
	}
	catch(...) {
		serr = errSSLSessionNotFound;
	}
	return serr;
}

/* cleanup up session cache, deleting stale entries. */
OSStatus sslCleanupSession ()
{
	OSStatus serr = noErr;
	bool moreToGo = false;
	try {
		moreToGo = gSessionCache().cleanup();
	}
	catch(...) {
		serr = errSSLSessionNotFound;
	}
	/* Possible TBD: if moreToGo, schedule a timed callback to this function */
	return serr;
}
