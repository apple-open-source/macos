/*
 *  CSLPPlugin.h
 *
 *  Created by imlucid on Wed Aug 15 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
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
    
private:
        CFStringRef				mRecentServersFolderName;
        CFStringRef				mFavoritesServersFolderName;
		Boolean					mNeedToStartNewLookup;
		CSLPNodeLookupThread*	mNodeLookupThread;
		SLPHandle				mSLPRef;
};

#endif


