/*
 * KClient bottom level interface to CCache API
 */
 
#include "KClientCCacheIntf.h"

KClientCCacheInterface::KClientCCacheInterface ()
{
}

KClientCCacheInterface::KClientCCacheInterface (
	cc_context_t				inContext):
	mContext (inContext)
{
}

KClientCCacheInterface::~KClientCCacheInterface ()
{
}


UCCache
KClientCCacheInterface::GetApplicationDefaultCCache ()
{
	return mContext.OpenCCache (tkt_string ());
}

UCCache
KClientCCacheInterface::GetCCacheForPrincipal (
	const UPrincipal&		inPrincipal)
{
	UCCacheIterator iterator = mContext.NewCCacheIterator ();
	UCCache 		ccache;

	while (iterator.Next (ccache)) {
		if ((ccache.GetCredentialsVersion () == UPrincipal::kerberosV4) ||
		    (ccache.GetCredentialsVersion () == UPrincipal::kerberosV4And5)) {
			if (ccache.GetPrincipal (UPrincipal::kerberosV4) == inPrincipal)
				return ccache;
		}
	}

	DebugThrow_ (UCCacheLogicError (ccErrCCacheNotFound));
	
	return UCCache (); // silence the warning
}

KClientCCacheInterface::operator UCCacheContext& ()
{
	return mContext;
}

#ifdef KClientDeprecated_

SInt32
KClientCCacheInterface::CountCCaches ()
{
	SInt32 count = 0;
	
	UCCacheIterator iterator = mContext.NewCCacheIterator ();
	UCCache 		ccache;

	while (iterator.Next (ccache)) {
		try {
			UPrincipal principal = ccache.GetPrincipal (UPrincipal::kerberosV4);
			count++;
		} catch (UCCacheLogicError&	e) {
			if (e.Error () != ccErrBadCredentialsVersion)
				throw;
		}
	}
	
	return count;
}

UCCache
KClientCCacheInterface::GetNthCCache (
	SInt32						inIndex)
{
	UCCacheIterator iterator = mContext.NewCCacheIterator ();
	UCCache			ccache;
	
	for (SInt32 count = 1; iterator.Next (ccache); count++) {
		if (count == inIndex)
			return ccache;
	}
	
	DebugThrow_ (UCCacheRuntimeError (ccErrCCacheNotFound));

	return UCCache (); // silence the warning
}
	
UCCache	
KClientCCacheInterface::GetPrincipalCCache (
	const UPrincipal&	inPrincipal)
{
	UCCacheIterator	iterator = mContext.NewCCacheIterator ();
	UCCache			ccache;

	while (iterator.Next (ccache)) {
		if (ccache.GetPrincipal (UPrincipal::kerberosV4) == inPrincipal)
			return ccache;
	}
	
	DebugThrow_ (UCCacheRuntimeError (ccErrCCacheNotFound));

	return UCCache (); // silence the warning
}
		
SInt32
KClientCCacheInterface::CountCredentials (
	const UCCache&				inCCache)
{
	SInt32 count = 0;
	
	UCredentialsIterator	iterator = inCCache.NewCredentialsIterator (UPrincipal::kerberosV4);
	UCredentials 			credentials;

	while (iterator.Next (credentials)) {
		count++;
	}
	
	return count;
}

UCredentials
KClientCCacheInterface::GetNthCredentials (
	const UCCache&				inCCache,
	SInt32						inIndex)
{
	UCredentialsIterator	iterator = inCCache.NewCredentialsIterator (UPrincipal::kerberosV4);
	UCredentials			credentials;
	
	for (SInt32 count = 1; iterator.Next (credentials); count++) {
		if (count == inIndex)
			return credentials;
	}
	
	DebugThrow_ (UCCacheRuntimeError (ccErrCredentialsNotFound));

	return UCredentials (); // silence the warning
}

#endif // KClientDeprecated_

