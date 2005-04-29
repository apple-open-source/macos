/*
 * Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
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
 * tpOcspCache.cpp - local OCSP response cache.
 */
 
#include "tpOcspCache.h"
#include "tpdebugging.h"
#include "certGroupUtils.h"
#include <security_utilities/globalizer.h>
#include <security_utilities/threading.h>
#include <security_ocspd/ocspdUtils.h>
#include <assert.h>

/*
 * Set this flag nonzero to turn off this cache module. Generally used to debug
 * the ocspd disk cache.
 */
#ifndef	NDEBUG
#define TP_OCSP_CACHE_DISABLE	0
#else
/* cache always enabled in production build */
#define TP_OCSP_CACHE_DISABLE	0
#endif
 
#pragma mark ---- single cache entry ----

/* 
 * One cache entry, just a parsed OCSPResponse plus an optional URI and a 
 * "latest" nextUpdate time. An entry is stale when its nextUpdate time has 
 * come and gone. 
 */
class OcspCacheEntry : public OCSPResponse
{
public:
	OcspCacheEntry(
		const CSSM_DATA derEncoded,
		const CSSM_DATA *localResponder);		// optional
	~OcspCacheEntry();
	
	/* a trusting environment, this module...all public */
	CSSM_DATA		mLocalResponder;			// we new[]
};

OcspCacheEntry::OcspCacheEntry(
	const CSSM_DATA derEncoded,
	const CSSM_DATA *localResponder)			// optional
	: OCSPResponse(derEncoded, TP_OCSP_CACHE_TTL)
{
	if(localResponder) {
		mLocalResponder.Data = new uint8[localResponder->Length];
		mLocalResponder.Length = localResponder->Length;
		memmove(mLocalResponder.Data, localResponder->Data, localResponder->Length);
	}
	else {
		mLocalResponder.Data = NULL;
		mLocalResponder.Length = 0;
	}
}

OcspCacheEntry::~OcspCacheEntry()
{
	delete[] mLocalResponder.Data;
}

#pragma mark ---- global cache object ----

/*
 * The cache object; ModuleNexus provides each task with at most of of these.
 * All ops which affect the contents of the cache hold the (essentially) global
 * mCacheLock.
 */
class OcspCache
{
public:
	OcspCache();
	~OcspCache();

	/* The methods corresponding to this module's public interface */
	OCSPSingleResponse *lookup(
		OCSPClientCertID	&certID,
		const CSSM_DATA		*localResponderURI);	// optional 
	void addResponse(
		const CSSM_DATA		&ocspResp,				// we'll decode it
		const CSSM_DATA		*localResponderURI);	// optional 
	void flush(
		OCSPClientCertID	&certID);

private:
	void removeEntry(unsigned dex);
	void scanForStale();
	OCSPSingleResponse *lookupPriv(
		OCSPClientCertID	&certID,
		const CSSM_DATA		*localResponderURI,		// optional 
		unsigned			&rtnDex);				// RETURNED on success

	Mutex			mCacheLock;
	
	/*
	 * NOTE: I am aware that most folks would just use an array<> here, but
	 * gdb is so lame that it doesn't even let one examine the contents
	 * of an array<> (or just about anything else in the STL). I prefer
	 * debuggability over saving a few lines of trivial code.
	 */
	OcspCacheEntry	**mEntries;			// just an array of pointers
	unsigned		mNumEntries;		// valid entries in mEntries
	unsigned		mSizeofEntries;		// mallocd space in mEntries
};

OcspCache::OcspCache()
	: mEntries(NULL), mNumEntries(0), mSizeofEntries(0)
{

}

/* As of Tiger I believe that this code never runs */
OcspCache::~OcspCache()
{
	for(unsigned dex=0; dex<mNumEntries; dex++) {
		delete mEntries[dex];
	}
	if(mEntries) {
		free(mEntries);
	}
}

/* 
 * Private routine, remove entry 'n' from cache.
 * -- caller must hold mCacheLock
 * -- if caller is traversing mEntries, they must start over because we
 *    manipulate it.
 */
void OcspCache::removeEntry(
	unsigned dex)
{
	assert(dex <= (mNumEntries - 1));
	
	/* removed requested element and compact remaining array */
	delete mEntries[dex];
	for(unsigned i=dex; i<(mNumEntries - 1); i++) {
		mEntries[i] = mEntries[i+1];
	}
	mNumEntries--;
}

/* 
 * Private routine to scan cache, deleting stale entries.
 * Caller must hold mCacheLock, and not be traversing mEntries.
 */
void OcspCache::scanForStale()
{
	CFAbsoluteTime now = CFAbsoluteTimeGetCurrent();
	bool foundOne;
	do {
		/* restart every time we delete a stale entry */
		foundOne = false;
		for(unsigned dex=0; dex<mNumEntries; dex++) {
			OcspCacheEntry *entry = mEntries[dex];
			if(entry->expireTime() < now) {
				tpOcspCacheDebug("OcspCache::scanForStale: deleting stale entry %p",
					entry);
				removeEntry(dex);
				foundOne = true;
				break;
			}
		}
	} while(foundOne);
}

/* 
 * Private lookup routine. Caller holds mCacheLock. We return both an
 * OCSPSingleResponse and the index into mEntries at which we found it. 
 */
 OCSPSingleResponse *OcspCache::lookupPriv(
	OCSPClientCertID	&certID,
	const CSSM_DATA		*localResponderURI,		// optional 
	unsigned			&rtnDex)				// RETURNED on success
{
	OCSPSingleResponse *resp = NULL;
	for(unsigned dex=0; dex<mNumEntries; dex++) {
		OcspCacheEntry *entry = mEntries[dex];
		if(localResponderURI) {
			/* if caller specifies, it must match */
			if(entry->mLocalResponder.Data == NULL) {
				/* came from somewhere else, skip it */
				tpOcspCacheDebug("OcspCache::lookup: uri mismatch (1) on entry %p",
					entry);
				continue;
			}
			if(!tpCompareCssmData(localResponderURI, &entry->mLocalResponder)) {
				tpOcspCacheDebug("OcspCache::lookup: uri mismatch (2) on entry %p",
					entry);
				continue;
			}
		}
		resp = entry->singleResponseFor(certID);
		if(resp) {
			tpOcspCacheDebug("OcspCache::lookupPriv: cache HIT on entry %p", entry);
			rtnDex=dex;
			return resp;
		}
	}
	tpOcspCacheDebug("OcspCache::lookupPriv: cache MISS");
	return NULL;
}

OCSPSingleResponse *OcspCache::lookup(
	OCSPClientCertID	&certID,
	const CSSM_DATA		*localResponderURI)		// optional 
{
	StLock<Mutex> _(mCacheLock);
	
	/* take care of stale entries right away */
	scanForStale();
	
	unsigned rtnDex;
	return lookupPriv(certID, localResponderURI, rtnDex);
}

void OcspCache::addResponse(
	const CSSM_DATA		&ocspResp,				// we'll decode it
	const CSSM_DATA		*localResponderURI)		// optional 
{
	StLock<Mutex> _(mCacheLock);

	OcspCacheEntry *entry = new OcspCacheEntry(ocspResp, localResponderURI);
	if(mNumEntries == mSizeofEntries) {
		if(mSizeofEntries == 0) {
			/* appending to empty array */
			mSizeofEntries = 1;
		}
		else {
			mSizeofEntries *= 2;
		}
		mEntries = (OcspCacheEntry **)realloc(mEntries, 
			mSizeofEntries * sizeof(OcspCacheEntry *));
	}
	mEntries[mNumEntries++] = entry;
	tpOcspCacheDebug("OcspCache::addResponse: add entry %p", entry);
}

void OcspCache::flush(
	OCSPClientCertID	&certID)
{
	StLock<Mutex> _(mCacheLock);
	
	/* take care of all stale entries */
	scanForStale();
	
	unsigned rtnDex;
	OCSPSingleResponse *resp;
	do {
		/* execute as until we find no more entries matching */
		resp = lookupPriv(certID, NULL, rtnDex);
		if(resp) {
			assert((rtnDex >= 0) && (rtnDex < mNumEntries));
			tpOcspCacheDebug("OcspCache::flush: deleting entry %p", mEntries[rtnDex]);
			removeEntry(rtnDex);
		}
	} while(resp != NULL);
}


static ModuleNexus<OcspCache> tpOcspCache;

#pragma mark ---- Public API ----
/*
 * Lookup locally cached response. Caller must free the returned OCSPSingleResponse.
 */
OCSPSingleResponse *tpOcspCacheLookup(
	OCSPClientCertID	&certID,
	const CSSM_DATA		*localResponderURI)			// optional 
{
	return tpOcspCache().lookup(certID, localResponderURI);
}

/* 
 * Add a fully verified OCSP response to cache. 
 */
void tpOcspCacheAdd(
	const CSSM_DATA		&ocspResp,				// we'll decode it, not keep a ref
	const CSSM_DATA		*localResponderURI)		// optional 
{
	#if	TP_OCSP_CACHE_DISABLE
	return;
	#endif
	tpOcspCache().addResponse(ocspResp, localResponderURI);
}

/*
 * Delete any entry associated with specified certID from cache.
 */
void tpOcspCacheFlush(
	OCSPClientCertID	&certID)
{
	tpOcspCache().flush(certID);
}

