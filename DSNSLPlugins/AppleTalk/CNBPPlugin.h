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
 *  @header CNBPPlugin
 */

#ifndef _CNBPPlugin_
#define _CNBPPlugin_

#include "CNSLPlugin.h"

#define kProtocolPrefixPlainStr		"AppleTalk"
#define kProtocolPrefixStr			"/AppleTalk"
#define kProtocolPrefixSlashStr		"/AppleTalk/"

#define kNoZoneLabel					"*"

#define kNBPThreadUSleepOnCount					25				// thread yield after reporting this number of items
#define kNBPThreadUSleepInterval				2500			// number of microseconds to pause the thread

#define kMaxZonesOnTryOne			1000
#define kMaxServicesOnTryOne		1000

class CNBPPlugin : public CNSLPlugin
{
public:
                                CNBPPlugin				( void );
                                ~CNBPPlugin				( void );
    
    virtual sInt32				InitPlugin				( void );
	virtual sInt32				SetServerIdleRunLoopRef	( CFRunLoopRef idleRunLoopRef );

            Boolean				IsScopeInReturnList		( const char* scope );
            void				AddResult				( const char* url );
    
            uInt32				fSignature;

            void				SetLocalZone			( const char* zone );
    const	char*				GetLocalZone			( void ) { return mLocalNodeString; }
protected:
    virtual CFStringRef			GetBundleIdentifier		( void );
    virtual const char*			GetProtocolPrefixString	( void );		// this is used for top of the node's path "NSL"
    virtual Boolean 			IsLocalNode				( const char *inNode );
    
    virtual void				NewNodeLookup			( void );		// this should fire off some threads in the subclass
    virtual	void				NewServiceLookup		( char* serviceType, CNSLDirNodeRep* nodeDirRep );
            Boolean				OKToSearchThisType		( char* serviceType );
    virtual Boolean				OKToOpenUnPublishedNode	( const char* parentNodeName );    

private:
            char*				mLocalNodeString;		
            SCDynamicStoreRef	mNBPSCRef;

};

#endif
