#include "CCacheIterator.h"
#include "Pointer.h"
#include "AbstractFactory.h"

#include "Context.h"

const	cc_ccache_iterator_f	CCICCacheIterator::sFunctionTable = {
	CCECCacheIterator::Release,
	CCECCacheIterator::Next
};

// Release a ccache iterator
cc_int32 CCECCacheIterator::Release (
	cc_ccache_iterator_t		inIterator) {
	
	CCIResult	result = ccNoError;
	
	CCIBeginSafeTry_ {
		StCCacheIterator	iterator (inIterator);
		
		delete iterator.Get ();
	} CCIEndSafeTry_ (result, ccErrBadParam)
	
	CCIAssert_ ((result == ccNoError)
	            || (result == ccErrInvalidCCacheIterator)
	            || (result == ccErrServerUnavailable));
	
	return result;
}

// Advance a ccache
cc_int32 CCECCacheIterator::Next (
	cc_ccache_iterator_t		inIterator,
	cc_ccache_t*				outCCache) {
	
	CCIResult	result = ccNoError;
	
	CCIBeginSafeTry_ {
		StCCacheIterator			iterator (inIterator);
		
		if (iterator -> HasMore ()) {
			StPointer <cc_ccache_t>		newCCache (outCCache);
			StCCache					ccache =
				CCIAbstractFactory::GetTheFactory () -> CreateCCache (
					iterator -> Next (), iterator -> GetAPIVersion ());
			newCCache = ccache;
		} else {
			result = ccIteratorEnd;
		}
	} CCIEndSafeTry_ (result, ccErrBadParam)
	
	CCIAssert_ ((result == ccNoError)
	            || (result == ccErrNoMem)
	            || (result == ccIteratorEnd)
	            || (result == ccErrInvalidCCacheIterator)
	            || (result == ccErrServerUnavailable));
	
	return result;
}

#pragma mark -

// Create a ccache iterator
CCICCacheIterator::CCICCacheIterator (
	const CCIContext&			inContext,
	CCIInt32					inAPIVersion):
	mAPIVersion (inAPIVersion) {

	mContext.reset (
            CCIAbstractFactory::GetTheFactory () -> CreateContext (
		inContext.GetContextID (), inAPIVersion));

	try {
		mContextLock = mContext -> Lock ();
		mContext -> GetCCacheIDs (mIterationSet);
		mIterator = mIterationSet.begin ();
	} catch (...) {
		mContext -> Unlock (mContextLock);
                throw;
	}
#if CCache_v2_compat
	CompatResetRepeatCount ();
#endif
}

// Destroy a ccache iterator
CCICCacheIterator::~CCICCacheIterator () {
	try {
		mContext -> Unlock (mContextLock);
	} catch (...) {
	}
}

// Is the iterator at the end?
bool
CCICCacheIterator::HasMore () const {
	return mIterator < mIterationSet.end ();
}

// Advance the iterator
CCIUniqueID
CCICCacheIterator::Next () {
	if (mIterator == mIterationSet.end ()) {
		throw CCIException (ccIteratorEnd);
	}
	
	// Return current item and advance the iterator
	return *mIterator++;
}

CCIUniqueID
CCICCacheIterator::Current () {
	if (mIterator == mIterationSet.end ()) {
		throw CCIException (ccIteratorEnd);
	}
	
	// Return current item
	return *mIterator;
}

// Validate integrity of an iterator
void CCICCacheIterator::Validate () {

	CCIMagic <CCICCacheIterator>::Validate ();
	CCIAssert_ ((CCIInternal <CCICCacheIterator, cc_ccache_iterator_d>::Valid ()));
 }

