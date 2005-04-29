/*
 * CCIContext.c
 *
 * $Header: /cvs/kfm/KerberosFramework/CredentialsCache/Sources/Context.cp,v 1.16 2004/10/04 17:41:26 lxs Exp $
 */

#include "AbstractFactory.h"

#include "Context.h"
#include "CCacheString.h"
#include "CCache.h"
#include "Pointer.h"
#include "CCacheIterator.h"

#include "CCacheUtil.h"

static const char	ccapi_vendor[] = "MIT I/S MacDev";

const	cc_context_f	CCIContext::sFunctionTable = {
	CCEContext::Release,
	CCEContext::GetChangeTime,
	CCEContext::GetDefaultCCacheName,
	CCEContext::OpenCCache,
	CCEContext::OpenDefaultCCache,
	CCEContext::CreateCCache,
	CCEContext::CreateDefaultCCache,
	CCEContext::CreateNewCCache,
	CCEContext::NewCCacheIterator,
	CCEContext::Lock,
	CCEContext::Unlock,
	CCEContext::Compare
};
	
// Create a new library context	
cc_int32 cc_initialize (
	cc_context_t*		outContext,
	cc_int32			inVersion,
	cc_int32*			outSupportedVersion,
	char const**		outVendor) {
	
	CCIResult	result = ccNoError;
	
	CCIBeginSafeTry_ {
		if (outVendor != NULL) {
			*outVendor = ccapi_vendor;
		}

	
		if ((inVersion != ccapi_version_2) &&
		    (inVersion != ccapi_version_3) &&
		    (inVersion != ccapi_version_4) &&
            (inVersion != ccapi_version_5)) {

			if (outSupportedVersion != NULL) {
				*outSupportedVersion = ccapi_version_5;
			}

			throw CCIException (ccErrBadAPIVersion);
		}
		
		if (!CCIValidPointer (outContext))
			throw CCIException (ccErrBadParam);
		
		// For API version 2, we create a version 3 context, and let the compatiblity API deal
		cc_int32	contextVersion = inVersion;
		if (inVersion == ccapi_version_2) {
			contextVersion = ccapi_version_3;
		}
		
		StContext					context =
			CCIAbstractFactory::GetTheFactory () -> CreateContext (contextVersion);
		StPointer <cc_context_t>	newContext (outContext);
		
		newContext = context;
	} CCIEndSafeTry_ (result, ccErrBadParam)
		
	CCIAssert_ ((result == ccNoError)
	            || (result == ccErrNoMem)
	            || (result == ccErrBadAPIVersion)
	            || (result == ccErrBadParam));
	            
	return result;
}

#if PRAGMA_MARK
#pragma mark -
#endif

// Clone an existing library context
cc_int32 CCEContext::Clone (
	cc_context_t		inContext,
	cc_context_t*		outContext,
	cc_int32			inVersion,
	cc_int32*			outSupportedVersion,
	char const**		outVendor) {
	
	CCIResult	result = ccNoError;
	
	CCIBeginSafeTry_ {
		if (outVendor != NULL) {
			StPointer <const char*>	vendor (outVendor);
			vendor = ccapi_vendor;
		}
		
		if ((inVersion != ccapi_version_2) &&
		    (inVersion != ccapi_version_3)) {

			if (outSupportedVersion != NULL) {
				*outSupportedVersion = ccapi_version_3;
			}

			throw CCIException (ccErrBadAPIVersion);
		}
		
		// For API version 2, we create a version 3 context, and let the compatiblity API deal
		cc_int32	contextVersion = inVersion;
		if (inVersion == ccapi_version_2) {
			contextVersion = ccapi_version_3;
		}
		
		StContext					oldContext (inContext);
		StPointer <cc_context_t> 	newContext (outContext);
		StContext					context =
			CCIAbstractFactory::GetTheFactory () -> CreateContext (contextVersion);
		
		newContext = context;
	} CCIEndSafeTry_ (result, ccErrBadParam)
		
	CCIAssert_ ((result == ccNoError)
	            || (result == ccErrNoMem)
	            || (result == ccErrBadAPIVersion)
	            || (result == ccErrBadParam)
	            || (result == ccErrServerUnavailable));
	            
	return result;
}	

// Get the API version of a context
cc_int32 CCEContext::GetVersion (
	cc_context_t		inContext,
	cc_int32*			outVersion) {
	
	CCIResult	result = ccNoError;
	
	CCIBeginSafeTry_ {
		StContext				context (inContext);
		StPointer <cc_int32>	version (outVersion);
		
		version = context -> GetAPIVersion ();
	} CCIEndSafeTry_ (result, ccErrBadParam)
	
	CCIAssert_ ((result == ccNoError)
	            || (result == ccErrInvalidContext)
	            || (result == ccErrBadParam)
	            || (result == ccErrServerUnavailable));
	
	return result;
}

// Release a context
cc_int32 CCEContext::Release (
	cc_context_t		inContext) {
	
	CCIResult	result = ccNoError;
	
	CCIBeginSafeTry_ {
		StContext				context (inContext);
		
		delete context.Get ();
	} CCIEndSafeTry_ (result, ccErrBadParam)
	
	CCIAssert_ ((result == ccNoError)
	            || (result == ccErrInvalidContext)
	            || (result == ccErrServerUnavailable));
	
	return result;
}

// Get the change time of a context
cc_int32 CCEContext::GetChangeTime (
	cc_context_t		inContext,
	cc_time_t*			outTime) {

	CCIResult	result = ccNoError;

	CCIBeginSafeTry_ {
		StContext				context (inContext);
		StPointer <cc_time_t>	time (outTime);
		
		time = context -> GetChangeTime ();
	} CCIEndSafeTry_ (result, ccErrBadParam)
	
	CCIAssert_ ((result == ccNoError)
	            || (result == ccErrInvalidContext)
	            || (result == ccErrBadParam)
	            || (result == ccErrServerUnavailable));

	return result;
}

// Get the default cache name of a context
cc_int32 CCEContext::GetDefaultCCacheName (
	cc_context_t		inContext,
	cc_string_t*		outName) {
	
	CCIResult	result = ccNoError;

	CCIBeginSafeTry_ {
		StContext					context (inContext);
		StPointer <cc_string_t>		newName (outName);
		StString					name =
			new CCIString (context -> GetDefaultCCacheName ());
		
		newName = name;
	} CCIEndSafeTry_ (result, ccErrBadParam) 
	
	CCIAssert_ ((result == ccNoError)
	            || (result == ccErrInvalidContext)
	            || (result == ccErrNoMem)
	            || (result == ccErrServerUnavailable));

	return result;
}

// Open a ccache in a context
cc_int32 CCEContext::OpenCCache (
	cc_context_t		inContext,
	const char*			inName,
	cc_ccache_t*		outCCache) {
	
	CCIResult	result = ccNoError;

	CCIBeginSafeTry_ {
		StContext					context (inContext);
		StPointer <cc_ccache_t>		newCCache (outCCache);
		StCCache					ccache =
			CCIAbstractFactory::GetTheFactory () -> CreateCCache (
				context -> OpenCCache (inName), context -> GetAPIVersion ());
		
		newCCache = ccache;
	} CCIEndSafeTry_ (result, ccErrBadParam) 
	
	CCIAssert_ ((result == ccNoError)
	            || (result == ccErrBadName)
	            || (result == ccErrInvalidContext)
	            || (result == ccErrCCacheNotFound)
	            || (result == ccErrNoMem)
	            || (result == ccErrBadParam)
	            || (result == ccErrServerUnavailable));

	return result;
}

// Open the default ccache in a context
cc_int32 CCEContext::OpenDefaultCCache (
	cc_context_t		inContext,
	cc_ccache_t*		outCCache) {
	
	CCIResult	result = ccNoError;

	CCIBeginSafeTry_ {
		StContext					context (inContext);
		StPointer <cc_ccache_t>		newCCache (outCCache);
		StCCache					ccache =
			CCIAbstractFactory::GetTheFactory () -> CreateCCache (
				context -> OpenDefaultCCache (), context -> GetAPIVersion ());
		
		newCCache = ccache;
	} CCIEndSafeTry_ (result, ccErrBadParam) 
	
	CCIAssert_ ((result == ccNoError)
	            || (result == ccErrBadName)
	            || (result == ccErrInvalidContext)
	            || (result == ccErrCCacheNotFound)
	            || (result == ccErrNoMem)
	            || (result == ccErrBadParam)
	            || (result == ccErrServerUnavailable));

	return result;
}

// Create a new ccache of a specific name in a context
cc_int32 CCEContext::CreateCCache (
	cc_context_t		inContext,
	const char*			inName,
	cc_uint32			inVersion,
	const char*			inPrincipal,
	cc_ccache_t*		outCCache) {
	
	CCIResult	result = ccNoError;

	CCIBeginSafeTry_ {
		StContext					context (inContext);
		StPointer <cc_ccache_t>		newCCache (outCCache);
		StCCache					ccache =
			CCIAbstractFactory::GetTheFactory () -> CreateCCache (
				context -> CreateCCache (inName, inVersion, inPrincipal), context -> GetAPIVersion ());
		
		newCCache = ccache;
	} CCIEndSafeTry_ (result, ccErrBadParam) 
	
	CCIAssert_ ((result == ccNoError)
	            || (result == ccErrBadName)
	            || (result == ccErrInvalidContext)
	            || (result == ccErrBadCredentialsVersion)
	            || (result == ccErrNoMem)
	            || (result == ccErrBadParam)
	            || (result == ccErrServerUnavailable));

	return result;
}

// Create the default ccache in a context
cc_int32 CCEContext::CreateDefaultCCache (
	cc_context_t		inContext,
	cc_uint32			inVersion,
	const char*			inPrincipal,
	cc_ccache_t*		outCCache) {
	
	CCIResult	result = ccNoError;

	CCIBeginSafeTry_ {
		StContext					context (inContext);
		StPointer <cc_ccache_t>		newCCache (outCCache);
		StCCache					ccache =
			CCIAbstractFactory::GetTheFactory () -> CreateCCache (
				context -> CreateDefaultCCache (inVersion, inPrincipal), context -> GetAPIVersion ());
		
		newCCache = ccache;
	} CCIEndSafeTry_ (result, ccErrBadParam)
	
	CCIAssert_ ((result == ccNoError)
	            || (result == ccErrInvalidContext)
	            || (result == ccErrBadCredentialsVersion)
	            || (result == ccErrNoMem)
	            || (result == ccErrBadParam)
	            || (result == ccErrServerUnavailable));

	return result;
}

// Create a new cache of a unique name in context
cc_int32 CCEContext::CreateNewCCache (
	cc_context_t		inContext,
	cc_uint32			inVersion,
	const char*			inPrincipal,
	cc_ccache_t*		outCCache) {
	
	CCIResult	result = ccNoError;

	CCIBeginSafeTry_ {
		StContext					context (inContext);
		StPointer <cc_ccache_t>		newCCache (outCCache);
		StCCache					ccache =
			CCIAbstractFactory::GetTheFactory () -> CreateCCache (
				context -> CreateNewCCache (inVersion, inPrincipal), context -> GetAPIVersion ());
			
		newCCache = ccache;
	} CCIEndSafeTry_ (result, ccErrBadParam)
	
	CCIAssert_ ((result == ccNoError)
	            || (result == ccErrInvalidContext)
	            || (result == ccErrBadCredentialsVersion)
	            || (result == ccErrNoMem)
	            || (result == ccErrBadParam)
	            || (result == ccErrServerUnavailable));

	return result;
}

// Lock a context
cc_int32 CCEContext::Lock (
	cc_context_t		/* inContext */,
	cc_uint32			/* inLockType */,
	cc_uint32			/* inLock */) {
#pragma message (CCIMessage_Warning_ "CCEContext::Lock not implemented")
	return ccNoError;
}

// Unlock a context
cc_int32 CCEContext::Unlock (
	cc_context_t		/* inContext */) {
#pragma message (CCIMessage_Warning_ "CCEContext::Unlock not implemented")
	return ccNoError;
}

// Get a new ccache iterator for a context
cc_int32 CCEContext::NewCCacheIterator (
	cc_context_t			inContext,
	cc_ccache_iterator_t*	outIterator) {
	
	CCIResult	result = ccNoError;

	CCIBeginSafeTry_ {
		StContext								context (inContext);
		StPointer <cc_ccache_iterator_t>		newIterator (outIterator);
		StCCacheIterator						iterator =
			new CCICCacheIterator (*context.Get (), context -> GetAPIVersion ());
			
		newIterator = iterator;
	} CCIEndSafeTry_ (result, ccErrBadParam)
	
	CCIAssert_ ((result == ccNoError)
	            || (result == ccErrInvalidContext)
	            || (result == ccErrNoMem)
	            || (result == ccErrBadParam)
	            || (result == ccErrServerUnavailable));

	return result;
}

// Compare two contexts
cc_int32 CCEContext::Compare (
	cc_context_t			inContext,
	cc_context_t			inCompareTo,
	cc_uint32*				outEqual) {
	
	CCIResult	result = ccNoError;

	CCIBeginSafeTry_ {
		StContext								context (inContext);
		StContext								compareTo (inCompareTo);
		StPointer <cc_uint32>					equal (outEqual);
		equal = context -> Compare (*compareTo.Get ());

	} CCIEndSafeTry_ (result, ccErrBadParam)
	
	CCIAssert_ ((result == ccNoError)
	            || (result == ccErrInvalidContext)
	            || (result == ccErrContextNotFound)
	            || (result == ccErrNoMem)
	            || (result == ccErrBadParam)
	            || (result == ccErrServerUnavailable));

	return result;
}

#if PRAGMA_MARK
#pragma mark -
#endif

// Create a new context
CCIContext::CCIContext (
	CCIUniqueID	inContextID,
	CCIInt32	inVersion):
	
	mContextID (inContextID),
	mAPIVersion (inVersion) {
}

// Validate integrity of a context
void CCIContext::Validate () {

	CCIMagic <CCIContext>::Validate ();

	CCIAssert_ ((CCIInternal <CCIContext, cc_context_d>::Valid ()));
	CCIAssert_ ((mAPIVersion == ccapi_version_3) 
                || (mAPIVersion == ccapi_version_4)
                || (mAPIVersion == ccapi_version_5));
}

