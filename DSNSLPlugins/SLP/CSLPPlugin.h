/*
 * Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
 *  @header CSLPPlugin
 */

#ifndef _CSLPPlugin_

#include "mslp_sd.h"
#include "slp.h"
#include "mslp.h"
#include "SLPDefines.h"

#include "CNSLPlugin.h"

#include "SLPComm.h"

class CSLPServiceLookupThread;
class CSLPNodeLookupThread;

class CSLPPlugin : public CNSLPlugin
{
public:
                                CSLPPlugin				( void );
                                ~CSLPPlugin				( void );
    
    virtual sInt32				InitPlugin				( void );
	virtual	void				ActivateSelf			( void );
	virtual	void				DeActivateSelf			( void );
			void				TellSLPdToQuit			( void );
			
	virtual sInt32				SetServerIdleRunLoopRef	( CFRunLoopRef idleRunLoopRef );
            char*				CreateLocalNodeFromConfigFile( void );

            Boolean				IsScopeInReturnList		( const char* scope );
    
            uInt32				fSignature;
            
            CFStringRef			GetRecentFolderName		( void )	{ return mRecentServersFolderName; }
            CFStringRef			GetFavoritesFolderName	( void )	{ return mFavoritesServersFolderName; }

	virtual	void				NodeLookupComplete		( void );		// node lookup thread calls this when it finishes
protected:
    virtual CFStringRef			GetBundleIdentifier		( void );
    virtual const char*			GetProtocolPrefixString	( void );		// this is used for top of the node's path "NSL"
    virtual const char*			GetLocalNodeString		( void );		// this is the user's "Local" location
    virtual Boolean 			IsLocalNode				( const char *inNode );
    virtual Boolean				IsADefaultOnlyNode		( const char *inNode );
    
    virtual void				NewNodeLookup			( void );		// this should fire off some threads in the subclass
    virtual	void				NewServiceLookup		( char* serviceType, CNSLDirNodeRep* nodeDirRep );
    virtual Boolean				OKToOpenUnPublishedNode	( const char* parentNodeName );        

    virtual	sInt32				HandleNetworkTransition	( sHeader *inData );
    
    virtual	Boolean				ReadOnlyPlugin			( void ) { return false; }
    virtual	Boolean				IsClientAuthorizedToCreateRecords ( sCreateRecord *inData ) { return true; }

    virtual	sInt32				RegisterService			( tRecordReference recordRef, CFDictionaryRef service );
    virtual	sInt32				DeregisterService		( tRecordReference recordRef, CFDictionaryRef service );

            OSStatus			DoSLPRegistration		( char* scopeList, char* url, char* attributeList );
            OSStatus			DoSLPDeregistration		( char* scopeList, char* url );
    
	virtual	UInt32				GetTimeBetweenNodeLookups	( void ) { return kOncePerDay; }
private:
        CFStringRef				mRecentServersFolderName;
        CFStringRef				mFavoritesServersFolderName;
		Boolean					mNeedToStartNewLookup;
		CSLPNodeLookupThread*	mNodeLookupThread;
		SLPHandle				mSLPRef;
};

#endif
