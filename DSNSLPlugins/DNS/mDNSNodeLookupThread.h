/*
 *  mDNSNodeLookupThread.h
 *
 *  Created by imlucid on Tue Aug 27 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
 */
#ifndef _mDNSNodeLookupThread_
#define _mDNSNodeLookupThread_ 1

#include <CoreServices/CoreServices.h>

#include "CNSLNodeLookupThread.h"

class mDNSNodeLookupThread : public CNSLNodeLookupThread
{
public:
                            mDNSNodeLookupThread			( CNSLPlugin* parentPlugin, const char* parentDomain );
    virtual					~mDNSNodeLookupThread			();
    
	virtual void*			Run								( void );
            void			Cancel							( void );
            
    CFNetServiceBrowserRef	GetBrowserRef					( void ) { return mSearchingBrowser; }
private:
    		char*						mParentDomain;
            CFRunLoopRef				mRunLoopRef;
            CFNetServiceBrowserRef 		mSearchingBrowser;
};
#endif		// #ifndef