/*
 *  DNSBrowserThread.h
 *  DSNSLPlugins
 *
 *  Created by Kevin Arnold on Tue Feb 26 2002.
 *  Copyright (c) 2002 Apple Computer. All rights reserved.
 *
 */


#ifndef _DNSBrowserThread_
#define _DNSBrowserThread_ 1

#include "CNSLHeaders.h"

class mDNSPlugin;

class DNSBrowserThread /*: public DSLThread*/
{
public:
                            DNSBrowserThread			( mDNSPlugin* parentPlugin );
    virtual					~DNSBrowserThread			();
    
            void			Initialize					( CFRunLoopRef idleRunLoopRef );
//	virtual void*			Run							( void );
            void			Cancel						( void );

            sInt32			StartNodeLookups			( Boolean onlyLookForRegistrationDomains );
            sInt32			StartServiceLookup			( CFStringRef domain, CFStringRef serviceType );
            mDNSPlugin*		GetParentPlugin				( void ) { return mParentPlugin; }
private:
    		mDNSPlugin*					mParentPlugin;
            CFRunLoopRef				mRunLoopRef;
            CFMutableArrayRef			mListOfSearches;
            Boolean						mCanceled;
			CFNetServiceBrowserRef 		mLocalDomainSearchingBrowserRef;
			CFNetServiceBrowserRef 		mDomainSearchingBrowserRef;
			
};
#endif		// #ifndef