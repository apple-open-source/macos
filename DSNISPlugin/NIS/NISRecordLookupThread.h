/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
 
/*!
 *  @header NISRecordLookupThread.h
 */
 
#ifndef _NISRecordLookupThread_
#define _NISRecordLookupThread_ 1

#include <CoreFoundation/CoreFoundation.h>

#include <DirectoryServiceCore/DSLThread.h>

class NISDirNodeRep;
class CNSLPlugin;
class NISResult;

class NISRecordLookupThread : public DSLThread
{
public:
                NISRecordLookupThread					( CNSLPlugin* parentPlugin, char* serviceType, NISDirNodeRep* nodeDirRep );
    virtual		~NISRecordLookupThread				();
    
	virtual void			Resume						( void );
	virtual void*			Run							( void ) = 0;
    virtual void			Cancel						( void ) { mCanceled = true; }	// This should only be called by the dir node rep!
            
            void			AddResult					( NISResult* newResult );		// will take responsibility for disposing this
            Boolean			AreWeCanceled				( void ) { return mCanceled; }

            CNSLPlugin*		GetParentPlugin				( void ) { return mParentPlugin; }
            NISDirNodeRep*	GetNodeToSearch				( void ) { return mNodeToSearch; }
protected:
            CFStringRef		GetNodeName					( void ) { return mNodeName; }
            CFStringRef		GetServiceTypeRef			( void ) { return mServiceType; }

        Boolean				mCanceled;
private:
        CFStringRef			mNodeName;
		Boolean				mNeedToNotifyNodeToSearchWhenComplete;
        CNSLPlugin*			mParentPlugin;
        NISDirNodeRep*		mNodeToSearch;
        CFStringRef			mServiceType;
};

#endif		// #ifndef
