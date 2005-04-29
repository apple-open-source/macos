#include "CredentialsIterator.h"
#include "Pointer.h"
#include "Credentials.h"
#include "CCache.h"
#include "AbstractFactory.h"

#include "Context.h"

const	cc_credentials_iterator_f	CCICredentialsIterator::sFunctionTable = {
	CCECredentialsIterator::Release,
	CCECredentialsIterator::Next
};

// Release a credentials iterator
cc_int32 CCECredentialsIterator::Release (
	cc_credentials_iterator_t		inIterator) {
	
	CCIResult	result = ccNoError;
	
	CCIBeginSafeTry_ {
		StCredentialsIterator	iterator (inIterator);
		
		delete iterator.Get ();
	} CCIEndSafeTry_ (result, ccErrBadParam)
	
	CCIAssert_ ((result == ccNoError)
	            || (result == ccErrInvalidCredentialsIterator)
	            || (result == ccErrServerUnavailable));
	
	return result;
}

// Advance a credentials iterator
cc_int32 CCECredentialsIterator::Next (
	cc_credentials_iterator_t		inIterator,
	cc_credentials_t*				outCredentials) {
	
	CCIResult	result = ccNoError;
	
	CCIBeginSafeTry_ {
		StCredentialsIterator			iterator (inIterator);
		
		if (iterator -> HasMore ()) {
			StPointer <cc_credentials_t>		newCredentials (outCredentials);
			StCredentials					credentials =
				CCIAbstractFactory::GetTheFactory () -> CreateCredentials (
					iterator -> Next (), iterator -> GetAPIVersion ());
			newCredentials = credentials;
		} else {
			result = ccIteratorEnd;
		}
	} CCIEndSafeTry_ (result, ccErrBadParam)
	
	CCIAssert_ ((result == ccNoError)
	            || (result == ccErrNoMem)
	            || (result == ccIteratorEnd)
	            || (result == ccErrInvalidCredentialsIterator)
	            || (result == ccErrServerUnavailable));
	
	return result;
}

#pragma mark -
// Create a credentials iterator for a given ccache
CCICredentialsIterator::CCICredentialsIterator (
	const CCICCache&			inCCache,
	CCIInt32					inAPIVersion):
	mAPIVersion (inAPIVersion) {

	mCCache.reset (
            CCIAbstractFactory::GetTheFactory () -> CreateCCache (
		inCCache.GetCCacheID (), inAPIVersion));

	// Make the ccache only return one version of creds
#if CCache_v2_compat		
	CCIUInt32	version = inCCache.CompatGetVersion ();
	if ((version == cc_credentials_v4) ||
		(version == cc_credentials_v5)) {
		mCCache -> CompatSetVersion (version);
	}
#endif

	try {
		mCCacheLock = mCCache -> Lock ();
		mCCache -> GetCredentialsIDs (mIterationSet);
		mIterator = mIterationSet.begin ();
#if CCache_v2_compat
		// Remember which version we are returning
		mVersion = mCCache -> CompatGetVersion ();
#endif
	} catch (...) {
		mCCache -> Unlock (mCCacheLock);
                throw;
	}
}

// Destroy a creds iterator
CCICredentialsIterator::~CCICredentialsIterator () {
	try {
		mCCache -> Unlock (mCCacheLock);
	} catch (...) {
	}
}

// Is a creds iterator done?
bool
CCICredentialsIterator::HasMore () const {
	return mIterator < mIterationSet.end ();
}

// Advance a creds iterator
CCIUniqueID
CCICredentialsIterator::Next () {
	if (mIterator == mIterationSet.end ()) {
		throw CCIException (ccIteratorEnd);
	}
	return *mIterator++;
}

// Check integrity of a creds iterator
void CCICredentialsIterator::Validate () {

	CCIMagic <CCICredentialsIterator>::Validate ();
	CCIAssert_ ((CCIInternal <CCICredentialsIterator, cc_credentials_iterator_d>::Valid ()));
 }

