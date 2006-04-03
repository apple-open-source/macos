/*
 * CCIContextData.cp
 *
 * $Header$
 */

#include "ContextData.h"
#include "CCacheUtil.h"

#if CCache_ContainsSharedStaticData
CCISharedStaticData <CCIContextData> CCIContextDataInterface::sGlobalContext;
#endif

CCISharedStaticDataProxy <CCIContextData> CCIContextDataInterface::sGlobalContextProxy = CCIContextDataInterface::sGlobalContext;

const char	CCIContextData::sInitialDefaultCCacheName [] = kInitialDefaultCCacheName;

// Create a context, no ccaches in it
CCIContextData::CCIContextData ():
	mCCaches (),
	mLastTimeStamp (0) {
	
	Changed ();
}

CCIContextData::~CCIContextData () {
}

// Get the ID of a ccache given its name
const CCICCacheData::UniqueID&
CCIContextData::GetCCacheID (
	const std::string&		inName) const {
	
	StReadLock __attribute__ ((unused)) lock (Lock ());;
	CCICCacheData*	ccache;

	if (FindCCache (inName, ccache)) {
		return ccache -> GetGloballyUniqueID ();
	}
	
	throw CCIException (ccErrCCacheNotFound);
}

// Get the ID of the default ccache
const CCICCacheData::UniqueID&
CCIContextData::GetDefaultCCacheID () const {

	StReadLock __attribute__ ((unused)) lock (Lock ());
	if (mCCaches.size () == 0) {
		throw CCIException (ccErrCCacheNotFound);
	}
	
	return mCCaches [0];
}

// Get the name of the default ccache
std::string
CCIContextData::GetDefaultCCacheName () const {

	StReadLock __attribute__ ((unused)) lock (Lock ());
	if (mCCaches.size () == 0) {
		return sInitialDefaultCCacheName;
	} else {
		return CCICCacheData::Resolve (mCCaches [0]) -> GetName ();
	}
}

// Create a new ccache
const CCICCacheData::UniqueID&
CCIContextData::CreateCCache (
	const std::string&		inName,
	CCIUInt32				inVersion,
	const std::string&		inPrincipal) {
	
	StWriteLock __attribute__ ((unused)) lock (Lock ());
	
	CCICCacheData*		ccache;
	
	if (FindCCache (inName, ccache)) {
		ccache -> SetPrincipal (inVersion, inPrincipal);
		Changed ();
		return ccache -> GetGloballyUniqueID ();
	}

	CCICCacheData*	newCCache = new CCICCacheData (this, inName, inVersion, inPrincipal);

	// Note that the default ccache is the first ccache in the list, so
	// if it doesn't exist, this sets a new default, but if a default
	// exists, it's unchanged
        if (mCCaches.size() == 0) {
            // About to become default
            newCCache -> UpdateLastDefaultTime ();
        }
	mCCaches.push_back (newCCache -> GetGloballyUniqueID ());
        
	Changed ();
	
	return newCCache -> GetGloballyUniqueID ();
}

// Create the default ccache
const CCICCacheData::UniqueID&
CCIContextData::CreateDefaultCCache (
	CCIUInt32				inVersion,
	const std::string&		inPrincipal) {
	
	StWriteLock __attribute__ ((unused)) lock (Lock ());
	
	return CreateCCache (GetDefaultCCacheName (), inVersion, inPrincipal);
}

// Create a new ccache with a unique name
const CCICCacheData::UniqueID&
CCIContextData::CreateNewCCache (
	CCIUInt32				inVersion,
	const std::string&		inPrincipal) {
	
	StWriteLock __attribute__ ((unused)) lock (Lock ());
	
	if (mCCaches.size () == 0) {
		return CreateDefaultCCache (inVersion, inPrincipal);
	} else {
		for (CCIUInt32 index = 0; ; index++) {
			char	newName [64];
			std::strstream	newNameStream (newName, 64);
			newNameStream << index << std::ends;

			CCICCacheData*	ccache;

			if (!FindCCache (newName, ccache)) {
				return CreateCCache (newName, inVersion, inPrincipal);
			}
		}
	}
}

// Get the change time of the context
CCITime
CCIContextData::GetChangeTime () const {

	StReadLock __attribute__ ((unused)) lock (Lock ());
	return mChangeTime;
}

// Get the list of all ccaches in the context
void			
CCIContextData::GetCCacheIDs (
	std::vector <CCIObjectID>&	outCCacheIDs) const {
	
	StReadLock __attribute__ ((unused)) lock (Lock ());
	
	outCCacheIDs.reserve (mCCaches.size ());
	outCCacheIDs.clear ();
	SharedCCacheCollection::const_iterator		iterator = mCCaches.begin ();
	for (; iterator != mCCaches.end (); iterator++) {
		outCCacheIDs.push_back ((*iterator).object);
	}
}

// Lock the context
CCILockID
CCIContextData::Lock () const {
#warning CCIContextData::Lock unimplemented
	return 0;
}

// Unlock the context
void
CCIContextData::Unlock (
	const CCILockID&		/* inLock */) {
#warning CCIContextData::Unlock unimplemented
}

// Compare for identity with another context
bool
CCIContextData::Compare (
	const CCIContextData::UniqueID&		inCompareTo) {
	return GetGloballyUniqueID () == inCompareTo;
}

// Bump the timestamp
void
CCIContextData::Changed () {
	StWriteLock __attribute__ ((unused)) lock (Lock ());
	mChangeTime = NewTimeStamp ();
}

// Remove a ccache from the list
void
CCIContextData::RemoveCCache (
	const CCICCacheData&	inCCache) {
	
	StWriteLock __attribute__ ((unused)) lock (Lock ());
	mCCaches.erase (std::find (mCCaches.begin (), mCCaches.end (), inCCache.GetGloballyUniqueID ()));
	Changed ();
}

// Set the default ccache
void
CCIContextData::SetDefault (
	const CCICCacheData&	inCCache) {
	
	StWriteLock __attribute__ ((unused)) lock (Lock ());
	RemoveCCache (inCCache);
	mCCaches.push_front (inCCache.GetGloballyUniqueID ());
	Changed ();
}

// Find a ccache by name
bool
CCIContextData::FindCCache (
	const std::string&	inName,
	CCICCacheData*&		outCCache) const {
	
	StReadLock __attribute__ ((unused)) lock (Lock ());
	
	SharedCCacheCollection::const_iterator	iter;
	
	for (iter = mCCaches.begin (); iter < mCCaches.end (); iter++) {
		CCICCacheData*	ccache = CCICCacheData::Resolve (*iter);
		if (ccache -> GetName () == inName) {
			outCCache = ccache;
			return true;
		}
	}
	
	return false;
}

/* The last timestamp must be remembered per-context so we can ensure it's monotonically
   increasing across processes */
CCITime
CCIContextData::NewTimeStamp () {
	CCITime		now = static_cast <CCITime> (time (NULL));
	
#if TARGET_RT_MAC_CFM
	msl_time_to_unix_time ((time_t*) &now);
#endif
	
	mLastTimeStamp = std::max (now, mLastTimeStamp + 1);
	return mLastTimeStamp;
}