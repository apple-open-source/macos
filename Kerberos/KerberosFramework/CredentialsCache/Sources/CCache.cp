/*
 * CCICache.c
 *
 * Implementation of credentials cache structures for CCache API.
 *
 * Internally, ccaches are kept in a linked list of cci_ccache_data
 * structures. 
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/CCache.cp,v 1.22 2003/03/17 20:46:12 lxs Exp $
 */

#include <Kerberos/krb.h>  // For krb_life_to_time

#include "CCache.h"
#include "CCacheString.h"
#include "CredentialsIterator.h"
 
#include "Pointer.h"

const	cc_ccache_f	CCICCache::sFunctionTable = {
	CCECCache::Release,
	CCECCache::Destroy,
	CCECCache::SetDefault,
	CCECCache::GetCredentialsVersion,
	CCECCache::GetName,
	CCECCache::GetPrincipal,
	CCECCache::SetPrincipal,
	CCECCache::StoreCredentials,
	CCECCache::RemoveCredentials,
	CCECCache::NewCredentialsIterator,
	CCECCache::Move,
	CCECCache::Lock,
	CCECCache::Unlock,
	CCECCache::GetLastDefaultTime,
	CCECCache::GetChangeTime,
	CCECCache::Compare,
    CCECCache::GetKDCTimeOffset,
    CCECCache::SetKDCTimeOffset,
    CCECCache::ClearKDCTimeOffset
};

// Release a ccache
cc_int32 CCECCache::Release (
	cc_ccache_t			inCCache) {
	
	CCIResult	result = ccNoError;
	
	CCIBeginSafeTry_ {
		StCCache				ccache (inCCache);
		
		delete ccache.Get ();
	} CCIEndSafeTry_ (result, ccErrBadParam)
	
	CCIAssert_ ((result == ccNoError)
	            || (result == ccErrInvalidCCache)
	            || (result == ccErrServerUnavailable));
	
	return result;
}

// Destroy a ccache
cc_int32 CCECCache::Destroy (
	cc_ccache_t			inCCache) {
	
	CCIResult	result = ccNoError;

	CCIBeginSafeTry_ {
		StCCache					ccache (inCCache);
		ccache -> Destroy ();
		delete ccache.Get ();
	} CCIEndSafeTry_ (result, ccErrBadParam) 
	
	CCIAssert_ ((result == ccNoError)
	            || (result == ccErrInvalidCCache)
	            || (result == ccErrServerUnavailable));

	return result;
}

// Make a ccache default
cc_int32 CCECCache::SetDefault (
	cc_ccache_t			inCCache) {
	
	CCIResult	result = ccNoError;

	CCIBeginSafeTry_ {
		StCCache					ccache (inCCache);
		ccache -> SetDefault ();
	} CCIEndSafeTry_ (result, ccErrBadParam) 
	
	CCIAssert_ ((result == ccNoError)
				|| (result == ccErrCCacheNotFound)
	            || (result == ccErrInvalidCCache)
	            || (result == ccErrServerUnavailable));

	return result;
}

// Get the version of creds stored in a ccache
cc_int32 CCECCache::GetCredentialsVersion (
	cc_ccache_t			inCCache,
	cc_uint32*			outVersion) {
	
	CCIResult	result = ccNoError;

	CCIBeginSafeTry_ {
		StCCache					ccache (inCCache);
		StPointer <cc_uint32>		version (outVersion);
		version = ccache -> GetCredentialsVersion ();
	} CCIEndSafeTry_ (result, ccErrBadParam) 
	
	CCIAssert_ ((result == ccNoError)
				|| (result == ccErrCCacheNotFound)
				|| (result == ccErrBadParam)
	            || (result == ccErrInvalidCCache)
	            || (result == ccErrServerUnavailable));

	return result;
}
	
// Get the principal of a ccache	
cc_int32 CCECCache::GetPrincipal (
	cc_ccache_t			inCCache,
	cc_uint32			inVersion,
	cc_string_t*		outPrincipal) {
	
	CCIResult	result = ccNoError;

	CCIBeginSafeTry_ {
		StCCache					ccache (inCCache);
		StPointer <cc_string_t>		newPrincipal (outPrincipal);
		StString					principal =
			new CCIString (ccache -> GetPrincipal (inVersion));
		
		newPrincipal = principal;
	} CCIEndSafeTry_ (result, ccErrBadParam) 
	
	CCIAssert_ ((result == ccNoError)
				|| (result == ccErrCCacheNotFound)
				|| (result == ccErrNoMem)
				|| (result == ccErrBadParam)
				|| (result == ccErrBadCredentialsVersion)
	            || (result == ccErrServerUnavailable));

	return result;
}

// Set the principal of a ccache
cc_int32 CCECCache::SetPrincipal (
	cc_ccache_t			inCCache,
	cc_uint32			inVersion,
	const char*			inPrincipal) {
	
	CCIResult	result = ccNoError;

	CCIBeginSafeTry_ {
		StCCache					ccache (inCCache);
		ccache -> SetPrincipal (inVersion, inPrincipal);

	} CCIEndSafeTry_ (result, ccErrBadParam) 
	
	CCIAssert_ ((result == ccNoError)
				|| (result == ccErrCCacheNotFound)
				|| (result == ccErrNoMem)
				|| (result == ccErrBadParam)
	            || (result == ccErrServerUnavailable));

	return result;
}

// Get the name of a ccache
cc_int32 CCECCache::GetName (
	cc_ccache_t			inCCache,
	cc_string_t*		outName) {
	
	CCIResult	result = ccNoError;

	CCIBeginSafeTry_ {
		StCCache					ccache (inCCache);
		StPointer <cc_string_t>		newName (outName);
		StString					name =
			new CCIString (ccache -> GetName ());
		
		newName = name;
	} CCIEndSafeTry_ (result, ccErrBadParam) 
	
	CCIAssert_ ((result == ccNoError)
				|| (result == ccErrCCacheNotFound)
				|| (result == ccErrInvalidCCache)
				|| (result == ccErrNoMem)
				|| (result == ccErrBadParam)
				|| (result == ccErrBadCredentialsVersion)
	            || (result == ccErrServerUnavailable));

	return result;
}

// Store credentials in a ccache
cc_int32 CCECCache::StoreCredentials (
	cc_ccache_t					inCCache,
	const cc_credentials_union*	inCreds) {
	
	CCIResult	result = ccNoError;

	CCIBeginSafeTry_ {
		StCCache					ccache (inCCache);
		ccache -> StoreCredentials (inCreds);
	} CCIEndSafeTry_ (result, ccErrBadParam) 
	
	CCIAssert_ ((result == ccNoError)
				|| (result == ccErrCCacheNotFound)
				|| (result == ccErrInvalidCCache)
				|| (result == ccErrNoMem)
				|| (result == ccErrBadParam)
				|| (result == ccErrBadCredentialsVersion)
	            || (result == ccErrServerUnavailable));

	return result;
}

// Remove credentials from a ccache
cc_int32 CCECCache::RemoveCredentials (
	cc_ccache_t					inCCache,
	cc_credentials_t			inCreds) {
	
	CCIResult	result = ccNoError;

	CCIBeginSafeTry_ {
		StCCache					ccache (inCCache);
		StCredentials				credentials (inCreds);
		ccache -> RemoveCredentials (*(credentials.Get ()));
	} CCIEndSafeTry_ (result, ccErrBadParam) 
	
	CCIAssert_ ((result == ccNoError)
				|| (result == ccErrCCacheNotFound)
				|| (result == ccErrInvalidCCache)
				|| (result == ccErrInvalidCredentials)
				|| (result == ccErrCredentialsNotFound)
				|| (result == ccErrBadParam)
	            || (result == ccErrServerUnavailable));

	return result;
}

// Get a new credentials iterator for a ccache
cc_int32 CCECCache::NewCredentialsIterator (
	cc_ccache_t					inCCache,
	cc_credentials_iterator_t*	outIterator) {
	
	CCIResult	result = ccNoError;

	CCIBeginSafeTry_ {
		StCCache								ccache (inCCache);
		StPointer <cc_credentials_iterator_t>	newIterator (outIterator);
		StCredentialsIterator					iterator =
			new CCICredentialsIterator (*ccache.Get (), ccache -> GetAPIVersion ());
			
		newIterator = iterator;
	} CCIEndSafeTry_ (result, ccErrBadParam)
	
	CCIAssert_ ((result == ccNoError)
	            || (result == ccErrInvalidCCache)
	            || (result == ccErrNoMem)
	            || (result == ccErrBadParam)
	            || (result == ccErrServerUnavailable));

	return result;
}

// Get the last time this ccache was the default one
cc_int32 CCECCache::GetLastDefaultTime (
	cc_ccache_t				inCCache,
	cc_time_t*				outTime) {

	CCIResult	result = ccNoError;

	CCIBeginSafeTry_ {
		StCCache				ccache (inCCache);
		StPointer <cc_time_t>	time (outTime);
		
		time = ccache -> GetLastDefaultTime ();
	} CCIEndSafeTry_ (result, ccErrBadParam)
	
	CCIAssert_ ((result == ccNoError)
				|| (result == ccErrCCacheNotFound)
				|| (result == ccErrNeverDefault)
	            || (result == ccErrInvalidCCache)
	            || (result == ccErrBadParam)
	            || (result == ccErrServerUnavailable));

	return result;
}
	
// Get the last time this ccache was changed	
cc_int32 CCECCache::GetChangeTime (
	cc_ccache_t				inCCache,
	cc_time_t*				outTime) {

	CCIResult	result = ccNoError;

	CCIBeginSafeTry_ {
		StCCache				ccache (inCCache);
		StPointer <cc_time_t>	time (outTime);
		
		time = ccache -> GetChangeTime ();
	} CCIEndSafeTry_ (result, ccErrBadParam)
	
	CCIAssert_ ((result == ccNoError)
				|| (result == ccErrCCacheNotFound)
	            || (result == ccErrInvalidCCache)
	            || (result == ccErrBadParam)
	            || (result == ccErrServerUnavailable));

	return result;
}
	
// Lock a ccache
cc_int32 CCECCache::Lock (
	cc_ccache_t			/* inContext */,
	cc_uint32			/* inLockType */,
	cc_uint32			/* inLock */) {
#pragma message (CCIMessage_Warning_ "CCECCache::Lock not implemented")
	return ccNoError;
}

// Unlock a ccache
cc_int32 CCECCache::Unlock (
	cc_ccache_t			/* inContext */) {
#pragma message (CCIMessage_Warning_ "CCECCache::Unlock not implemented")
	return ccNoError;
}

// Compare two ccaches for identity (not equality!)
cc_int32 CCECCache::Compare (
	cc_ccache_t			inCCache,
	cc_ccache_t			inCompareTo,
	cc_uint32*			outEqual) {
	
	CCIResult	result = ccNoError;

	CCIBeginSafeTry_ {
		StCCache								ccache (inCCache);
		StCCache								compareTo (inCompareTo);
		StPointer <cc_uint32>					equal (outEqual);
		equal = ccache -> Compare (*compareTo.Get ());

	} CCIEndSafeTry_ (result, ccErrBadParam)
	
	CCIAssert_ ((result == ccNoError)
	            || (result == ccErrInvalidContext)
	            || (result == ccErrNoMem)
	            || (result == ccErrBadParam)
	            || (result == ccErrServerUnavailable));

	return result;
}

// Move the contents of a ccache into another ccache
cc_int32 CCECCache::Move (
	cc_ccache_t				inSource,
	cc_ccache_t				inDestination) {

	CCIResult	result = ccNoError;

	CCIBeginSafeTry_ {
		StCCache				source (inSource);
		StCCache				destination (inDestination);
		
		source -> Move (*(destination.Get ()));
		delete source.Get ();
	} CCIEndSafeTry_ (result, ccErrBadParam)
	
	CCIAssert_ ((result == ccNoError)
				|| (result == ccErrCCacheNotFound)
	            || (result == ccErrInvalidCCache)
	            || (result == ccErrServerUnavailable));

	return result;
}

// Get the KDC offset for the ccache, if there is one
cc_int32 CCECCache::GetKDCTimeOffset (
    cc_ccache_t   inCCache, 
    cc_int32   inVersion,
	cc_time_t *outTimeOffset)
{
	CCIResult	result = ccNoError;

	CCIBeginSafeTry_ {
		StCCache				ccache (inCCache);
		StPointer <cc_time_t>	timeOffset (outTimeOffset);
        
		timeOffset = ccache -> GetKDCTimeOffset (inVersion);
	} CCIEndSafeTry_ (result, ccErrBadParam)
	
	CCIAssert_ ((result == ccNoError)
				|| (result == ccErrTimeOffsetNotSet)
				|| (result == ccErrCCacheNotFound)
	            || (result == ccErrInvalidCCache)
	            || (result == ccErrBadParam)
	            || (result == ccErrServerUnavailable)
                || (result == ccErrBadCredentialsVersion));

	return result;
}

// Set the KDC offset for the ccache
cc_int32 CCECCache::SetKDCTimeOffset (
    cc_ccache_t   inCCache, 
    cc_int32   inVersion,
	cc_time_t  inTimeOffset)
{
	CCIResult	result = ccNoError;

	CCIBeginSafeTry_ {
		StCCache				ccache (inCCache);
        
		ccache -> SetKDCTimeOffset (inVersion, inTimeOffset);
	} CCIEndSafeTry_ (result, ccErrBadParam)
	
	CCIAssert_ ((result == ccNoError)
				|| (result == ccErrCCacheNotFound)
	            || (result == ccErrInvalidCCache)
	            || (result == ccErrBadParam)
	            || (result == ccErrServerUnavailable)
                || (result == ccErrBadCredentialsVersion));

	return result;
}

// Set the KDC offset for the ccache
cc_int32 CCECCache::ClearKDCTimeOffset (
    cc_ccache_t   inCCache, 
    cc_int32   inVersion)
{
	CCIResult	result = ccNoError;

	CCIBeginSafeTry_ {
		StCCache				ccache (inCCache);
        
		ccache -> ClearKDCTimeOffset (inVersion);
	} CCIEndSafeTry_ (result, ccErrBadParam)
	
	CCIAssert_ ((result == ccNoError)
				|| (result == ccErrCCacheNotFound)
	            || (result == ccErrInvalidCCache)
	            || (result == ccErrBadParam)
	            || (result == ccErrServerUnavailable)
                || (result == ccErrBadCredentialsVersion));

	return result;
}

#pragma mark -

// Create a new ccache
CCICCache::CCICCache (
	CCIUniqueID		inCCache,
	CCIInt32		inAPIVersion):
#if CCache_v2_compat
	mVersion (cc_credentials_v4_v5), // By default, both v4 and v5
#endif
	mAPIVersion (inAPIVersion),
	mCCacheID (inCCache)
{
}

// Set the version this ccache stores (only for CCAPI v2 compat)
#if CCache_v2_compat
void
CCICCache::CompatSetVersion (
	CCIUInt32					inVersion) {
	
	if ((inVersion != cc_credentials_v4) && (inVersion != cc_credentials_v5)) {
		throw CCIException (ccErrBadCredentialsVersion);
	}
	
	mVersion = inVersion;
}
#endif

void CCICCache::Validate () {

	CCIMagic <CCICCache>::Validate ();
	
	CCIAssert_ ((CCIInternal <CCICCache, cc_ccache_d>::Valid ()));
}

// Overridden to translate krb4 lifetimes to time
// CCAPI v2 and v3 use krb4-style lifetime representation (0..255) which
// has to be converted to the standard UNIX epoch before being stored in the
// cache
void CCICCache::StoreCredentials (
	const cc_credentials_union*		inCredentials) {

	if ((inCredentials -> version == cc_credentials_v4) && (mAPIVersion < ccapi_version_4)) {
		cc_credentials_union		credentials = *inCredentials;
		cc_credentials_v4_t			v4credentials = *inCredentials -> credentials.credentials_v4;

		v4credentials.lifetime = static_cast <cc_int32> (krb_life_to_time (v4credentials.issue_date, v4credentials.lifetime) - v4credentials.issue_date);
		credentials.credentials.credentials_v4 = &v4credentials;
		StoreConvertedCredentials (&credentials);
	} else {
		StoreConvertedCredentials (inCredentials);
	}
}

#if CCache_v2_compat
void CCICCache::CompatStoreCredentials (
	const cred_union&		inCredentials) {
	
	if (inCredentials.cred_type == CC_CRED_V4) {
		cred_union					credentials = inCredentials;
		cc_credentials_v4_compat	v4credentials = *inCredentials.cred.pV4Cred;

		v4credentials.lifetime = static_cast <cc_int32> (krb_life_to_time (static_cast <cc_uint32> (v4credentials.lifetime) - v4credentials.issue_date, v4credentials.lifetime)) - v4credentials.issue_date;
		credentials.cred.pV4Cred = &v4credentials;
		CompatStoreConvertedCredentials (credentials);
	} else {
		CompatStoreConvertedCredentials (inCredentials);
	}
}

#endif
