/*
 * KClient bottom level interface to CCache API
 */

#pragma once

class KClientCCacheInterface {
	public:
									KClientCCacheInterface ();
									KClientCCacheInterface (
										cc_context_t				inCCacheContext);
									~KClientCCacheInterface ();

		UCCache						GetApplicationDefaultCCache ();
		UCCache						GetCCacheForPrincipal (
										const UPrincipal&	inPrincipal);

									operator UCCacheContext& ();
		UCCacheContext&				Get () { return mContext; }
										
#ifdef KClientDeprecated_

		SInt32						CountCCaches ();
		UCCache						GetNthCCache (
										SInt32						inIndex);
		UCCache						GetPrincipalCCache (
										const UPrincipal&	inPrincipal);

		SInt32						CountCredentials (
										const UCCache&				inCCache);

		UCredentials				GetNthCredentials (
										const UCCache&				inCCache,
										SInt32						inIndex);
		
		

#endif // KClientDeprecated_

	private:
		UCCacheContext	mContext;
		
									KClientCCacheInterface (
										KClientCCacheInterface&	inOriginal);
		KClientCCacheInterface&		operator = (
										KClientCCacheInterface&	inOriginal);
};
