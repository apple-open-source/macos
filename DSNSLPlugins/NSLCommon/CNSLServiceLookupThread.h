/*
 *  CNSLServiceLookupThread.h
 *
 *	This is a wrapper class for service lookups of plugins of NSL
 *   
 *  Created by imlucid on Tue Aug 14 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
 */
#ifndef _CNSLServiceLookupThread_
#define _CNSLServiceLookupThread_ 1

#include <CoreFoundation/CoreFoundation.h>

//#include "DSLThread.h"
#include <DirectoryServiceCore/DSLThread.h>

class CNSLDirNodeRep;
class CNSLPlugin;
class CNSLResult;

class CNSLServiceLookupThread : public DSLThread
{
public:
                CNSLServiceLookupThread					( CNSLPlugin* parentPlugin, char* serviceType, CNSLDirNodeRep* nodeDirRep );
    virtual		~CNSLServiceLookupThread				();
    
	virtual void			Resume						( void );
	virtual void*			Run							( void ) = 0;
    virtual void			Cancel						( void ) { mCanceled = true; }	// This should only be called by the dir node rep!
            
            void			AddResult					( CNSLResult* newResult );		// will take responsibility for disposing this
            Boolean			AreWeCanceled				( void ) { return mCanceled; }

            CNSLPlugin*		GetParentPlugin				( void ) { return mParentPlugin; }
            CNSLDirNodeRep*	GetNodeToSearch				( void ) { return mNodeToSearch; }
protected:
            CFStringRef		GetNodeName					( void ) { return mNodeName; }
            CFStringRef		GetServiceTypeRef			( void ) { return mServiceType; }

        Boolean				mCanceled;
private:
        CFStringRef			mNodeName;
		Boolean				mNeedToNotifyNodeToSearchWhenComplete;
        CNSLPlugin*			mParentPlugin;
        CNSLDirNodeRep*		mNodeToSearch;
        CFStringRef			mServiceType;
};
#endif		// #ifndef