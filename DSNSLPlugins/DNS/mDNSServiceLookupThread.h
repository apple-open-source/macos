/*
 *  mDNSServiceLookupThread.h
 *
 *  Created by imlucid on Tue Aug 27 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
 */
#ifndef _mDNSServiceLookupThread_
#define _mDNSServiceLookupThread_ 1

#include "CNSLServiceLookupThread.h"

#define	kMaxTimeToWaitBetweenServices	5		// 5 seconds?

class mDNSServiceLookupThread : public CNSLServiceLookupThread
{
public:
                            mDNSServiceLookupThread		( CNSLPlugin* parentPlugin, char* serviceType, CNSLDirNodeRep* nodeDirRep );
    virtual					~mDNSServiceLookupThread	();
        
	virtual void*			Run							( void );
    virtual void			Cancel						( void );
            void			AddResult					( CNSLResult* newResult );
            Boolean			IsSearchTimedOut			( void );

protected:
            sInt32			StartServiceLookup			( CFStringRef domain, CFStringRef serviceType );
            CFRunLoopRef				mRunLoopRef;
            CFAbsoluteTime				mLastResult;
			CFNetServiceBrowserRef 		mSearchingBrowserRef;
private:
};
#endif		// #ifndef