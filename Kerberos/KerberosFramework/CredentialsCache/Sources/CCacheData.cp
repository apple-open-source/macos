/*
 * CCICCacheData.cp
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/CCacheData.cp,v 1.20 2003/03/17 20:46:29 lxs Exp $
 */

#include "CCacheData.h"
#include "ContextData.h"
#include "CCacheUtil.h"

// Create a new empty ccache
CCICCacheData::CCICCacheData () {
}

// Create a new ccache in a context
CCICCacheData::CCICCacheData (
			CCIContextData*		inContext,
	const	std::string&		inName,
	CCIUInt32					inVersion,
	const	std::string&		inPrincipal):
	
	mContext (inContext),
	mHasBeenDefault (false),
	mName (inName.c_str ()),
	mHavePrincipalV4 (false),
	mHavePrincipalV5 (false),
    mHaveKDCTimeOffsetV4 (false),
    mKDCTimeOffsetV4 (0),
    mHaveKDCTimeOffsetV5 (false),
    mKDCTimeOffsetV5 (0) {
	
	if (inVersion == cc_credentials_v4) {
		mHavePrincipalV4 = true;
		mPrincipalV4 = inPrincipal.c_str ();
	} else if (inVersion == cc_credentials_v5) {
		mHavePrincipalV5 = true;
		mPrincipalV5 = inPrincipal.c_str ();
	} else {
		throw CCIException (ccErrBadCredentialsVersion);
	}
	
	Changed ();
}

// Destroy a ccache and remove it from the owning context
CCICCacheData::~CCICCacheData () {
    // Delete our credentials before deleting the cache
	SharedCredentialsVector::iterator iterator = mCredentials.begin();

	for (; iterator != mCredentials.end(); iterator++) {
		delete (*iterator);
		*iterator = nil;
	}
	mCredentials.clear ();

	mContext -> RemoveCCache (*this);
}

// Bump the timestamp for the ccache and the parent context
void
CCICCacheData::Changed () {
	StWriteLock	__attribute__ ((unused)) lock (Lock ());
	
	mChangeTime = mContext -> NewTimeStamp ();
	mContext -> Changed ();
}

// Destroy a ccache and remove it from the parent context
void
CCICCacheData::Destroy () {
	delete this;
}

// Make the ccache default in the parent context
void
CCICCacheData::SetDefault () {
	mContext -> SetDefault (*this);
}

// What versions of creds can be stored in here?
CCIUInt32
CCICCacheData::GetCredentialsVersion () const {
	StReadLock	__attribute__ ((unused)) lock (Lock ());
	
	CCIUInt32	result = 0;
	if (mHavePrincipalV4)
		result |= cc_credentials_v4;
	if (mHavePrincipalV5)
		result |= cc_credentials_v5;
	return result;
}

// Get the ccache name
std::string
CCICCacheData::GetName () const {
	StReadLock	__attribute__ ((unused)) lock (Lock ());
	
	return mName.c_str ();
}

// Get the ccache principal
std::string
CCICCacheData::GetPrincipal (
	CCIUInt32		inVersion) const {
	StReadLock	__attribute__ ((unused)) lock (Lock ());
	
	if (inVersion == cc_credentials_v4) {
		if (mHavePrincipalV4)
			return mPrincipalV4.c_str ();
	} else if (inVersion == cc_credentials_v5) {
		if (mHavePrincipalV5)
			return mPrincipalV5.c_str ();
	}

	throw CCIException (ccErrBadCredentialsVersion);
}


// Last time the ccache was default
CCITime
CCICCacheData::GetLastDefaultTime () const {
	StReadLock	__attribute__ ((unused)) lock (Lock ());
	
	if (mHasBeenDefault)
		return mLastDefaultTime;
	
	throw CCIException (ccErrNeverDefault);
}

// Last time ccache was changed
CCITime
CCICCacheData::GetChangeTime () const {
	StReadLock	__attribute__ ((unused)) lock (Lock ());
	
	return mChangeTime;
}

// Store creds in ccache
void
CCICCacheData::StoreCredentials (
	const	cc_credentials_union*	inCredentials) {
	
	StWriteLock	__attribute__ ((unused)) lock (Lock ());
	
	mCredentials.push_back (new CCICredentialsData (inCredentials));
	
	Changed ();
}

// Store creds in ccache
void
CCICCacheData::StoreCredentials (
	CCICredentialsData*	inCredentials) {
	
	StWriteLock	__attribute__ ((unused)) lock (Lock ());
	
	mCredentials.push_back (inCredentials);
	
	Changed ();
}

#if CCache_v2_compat
// Store v2-style creds
void
CCICCacheData::CompatStoreCredentials (
	const	cred_union&				inCredentials) {
	
	StWriteLock	__attribute__ ((unused)) lock (Lock ());
	
	mCredentials.push_back (new CCICredentialsData (inCredentials));
	
	Changed ();
}
#endif


// Change the principal and delete all creds
void
CCICCacheData::SetPrincipal (
	CCIUInt32				inVersion,
	const std::string&		inPrincipal) {
			
	StWriteLock	__attribute__ ((unused)) lock (Lock ());
	
	if (inVersion == cc_credentials_v4) {
		mPrincipalV4 = inPrincipal.c_str ();
		mHavePrincipalV4 = true;
        
        // reset the v4 time offsets since they are per-KDC
        mHaveKDCTimeOffsetV4 = false;
        mKDCTimeOffsetV4 = 0;
        
	} else if (inVersion == cc_credentials_v5) {
		mPrincipalV5 = inPrincipal.c_str ();
		mHavePrincipalV5 = true;
        
        // reset the v5 time offsets since they are per-KDC
        mHaveKDCTimeOffsetV5 = false;
        mKDCTimeOffsetV5 = 0;
        
	} else {
		throw CCIException (ccErrBadCredentialsVersion);
	}

	
	CCICredentialsData::VersionMatch 	match (inVersion);
	
	SharedCredentialsVector::iterator	iterator = mCredentials.begin ();
	
	for (; iterator != mCredentials.end (); iterator++) {
		if (match (*iterator)) {
			delete (*iterator);
			*iterator = NULL;
		}
	}
	
 	
	// Based on eliminate_duplicates() on p.534 of The C++ Programming Language, 3rd ed:
 	SharedCredentialsVector::iterator	newEnd = 
		std::remove_if (mCredentials.begin (), mCredentials.end (),
			std::bind2nd(std::equal_to<CCICredentialsData*>(), NULL));
	mCredentials.erase(newEnd, mCredentials.end());
 	
	Changed ();
}

#if CCache_v2_compat
// Change the principal without removing the creds, for v2 compat
void
CCICCacheData::CompatSetPrincipal (
	CCIUInt32				inVersion,
	const std::string&		inPrincipal) {
			
	StWriteLock	__attribute__ ((unused)) lock (Lock ());
	
	if (inVersion == cc_credentials_v4) {
		mPrincipalV4 = inPrincipal.c_str ();
		mHavePrincipalV4 = true;
        
        // reset the v4 time offsets since they are per-KDC
        mHaveKDCTimeOffsetV4 = false;
        mKDCTimeOffsetV4 = 0;
        
	} else if (inVersion == cc_credentials_v5) {
		mPrincipalV5 = inPrincipal.c_str ();
		mHavePrincipalV5 = true;
        
        // reset the v5 time offsets since they are per-KDC
        mHaveKDCTimeOffsetV5 = false;
        mKDCTimeOffsetV5 = 0;
        
	} else {
		throw CCIException (ccErrBadCredentialsVersion);
	}
    
	Changed ();
}
#endif

// Remove creds
void
CCICCacheData::RemoveCredentials (
	const CCICredentialsData::UniqueID&		inCredentials) {
	
	StWriteLock	__attribute__ ((unused)) lock (Lock ());
	
	CCICredentialsData::UniqueIDMatch 	match (inCredentials);
	
	SharedCredentialsVector::iterator	iterator = mCredentials.begin ();
	
	for (; iterator != mCredentials.end (); iterator++) {
		if (match (*iterator)) {
			delete (*iterator);
			*iterator = NULL;
			mCredentials.erase (iterator);
			break;
		}
	}
	
	Changed ();
}

// Transplant contents into another ccache
void
CCICCacheData::Move (
	const UniqueID&		inDestination) {
	
	CCICCacheData*	destination = CCICCacheData::Resolve (inDestination);
	
	destination -> mContext = mContext;
    
	std::swap (mHavePrincipalV4, destination -> mHavePrincipalV4);
	mPrincipalV4.swap (destination -> mPrincipalV4);	
	std::swap (mHavePrincipalV5, destination -> mHavePrincipalV5);
	mPrincipalV5.swap (destination -> mPrincipalV5);
    	
    std::swap (mHaveKDCTimeOffsetV4, destination -> mHaveKDCTimeOffsetV4);
    std::swap (mKDCTimeOffsetV4, destination -> mKDCTimeOffsetV4);
    std::swap (mHaveKDCTimeOffsetV5, destination -> mHaveKDCTimeOffsetV5);
    std::swap (mKDCTimeOffsetV5, destination -> mKDCTimeOffsetV5);
    
	mCredentials.swap (destination -> mCredentials);
	
	destination -> Changed ();
	
	Destroy ();
}

// Lock the ccache
CCILockID 
CCICCacheData::Lock () const {
#pragma message (CCIMessage_Warning_ "CCICCacheData::Lock unimplemented")
	return 0;
}

// Unlock the ccache
void
CCICCacheData::Unlock (
	CCILockID			/* inLock */) {
#pragma message (CCIMessage_Warning_ "CCICCacheData::Unlock unimplemented")
}

// Compare for identity with another ccache
bool
CCICCacheData::Compare (
	const CCICCacheData::UniqueID&		inCompareTo) {
	return GetGloballyUniqueID () == inCompareTo;
}

// Get list of all credentials in ccache
void			
CCICCacheData::GetCredentialsIDs (
	std::vector <CCIObjectID>&	outCredentialsIDs) const {
	
	StReadLock	__attribute__ ((unused)) lock (Lock ());
	
	outCredentialsIDs.reserve (mCredentials.size ());
	outCredentialsIDs.clear ();
	SharedCredentialsVector::const_iterator		iterator = mCredentials.begin ();
	for (; iterator != mCredentials.end (); iterator++) {
		outCredentialsIDs.push_back ((*iterator) -> GetGloballyUniqueID ().object);
	}
}


// Get KDC time offsets:
CCITime
CCICCacheData::GetKDCTimeOffset (
    CCIUInt32			inVersion) const 
{
	StReadLock	__attribute__ ((unused)) lock (Lock ());

	if (inVersion == cc_credentials_v4) {
		if (mHaveKDCTimeOffsetV4) {
            return mKDCTimeOffsetV4;
        }
	} else if (inVersion == cc_credentials_v5) {
		if (mHaveKDCTimeOffsetV4) {
            return mKDCTimeOffsetV5;
        }
	} else {
		throw CCIException (ccErrBadCredentialsVersion);
	}
    
    // If we got here, time offset isn't set
    throw CCIException (ccErrTimeOffsetNotSet);
}

// Set KDC time offsets:
void
CCICCacheData::SetKDCTimeOffset (
    CCIUInt32			inVersion,
    CCITime				inTimeOffset)
{
    StWriteLock	__attribute__ ((unused)) lock (Lock ());
    
	if (inVersion == cc_credentials_v4) {
		mKDCTimeOffsetV4 = inTimeOffset;
		mHaveKDCTimeOffsetV4 = true;
	} else if (inVersion == cc_credentials_v5) {
		mKDCTimeOffsetV5 = inTimeOffset;
		mHaveKDCTimeOffsetV5 = true;
	} else if (inVersion == cc_credentials_v4_v5) {
		mKDCTimeOffsetV4 = inTimeOffset;
		mHaveKDCTimeOffsetV4 = true;
		mKDCTimeOffsetV5 = inTimeOffset;
		mHaveKDCTimeOffsetV5 = true;
	} else {
		throw CCIException (ccErrBadCredentialsVersion);
	}
    
	Changed ();
}

// Set KDC time offsets:
void
CCICCacheData::ClearKDCTimeOffset (
    CCIUInt32			inVersion)
{
    StWriteLock	__attribute__ ((unused)) lock (Lock ());
    
	if (inVersion == cc_credentials_v4) {
		mKDCTimeOffsetV4 = 0;
		mHaveKDCTimeOffsetV4 = false;
	} else if (inVersion == cc_credentials_v5) {
		mKDCTimeOffsetV5 = 0;
		mHaveKDCTimeOffsetV5 = false;
	} else if (inVersion == cc_credentials_v4_v5) {
		mKDCTimeOffsetV4 = 0;
		mHaveKDCTimeOffsetV4 = false;
		mKDCTimeOffsetV5 = 0;
		mHaveKDCTimeOffsetV5 = false;
    } else {
		throw CCIException (ccErrBadCredentialsVersion);
	}
    
	Changed ();
}
