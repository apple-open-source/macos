/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
 
/*!
 *  @header CNSLServiceLookupThread.h
 */
 
#ifndef _CNSLServiceLookupThread_
#define _CNSLServiceLookupThread_ 1

#include <CoreFoundation/CoreFoundation.h>

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
