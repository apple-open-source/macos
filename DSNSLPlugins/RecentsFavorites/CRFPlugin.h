/*
 *  CRFPlugin.h
 *  DSNSLPlugins
 *
 *  Created by Kevin Arnold on Thu Feb 28 2002.
 *  Copyright (c) 2002 Apple Computer. All rights reserved.
 *
 */

#define SUPPORT_WRITE_ACCESS 0

#ifndef _CRFPlugin_
#define _CRFPlugin_ 1

#include "CNSLPlugin.h"

#define kLocalManagedDataName		"LocalManagedData"
#define kLocalManagedDataBasePath	"/Volumes/Data/LocalManagedData/"

class CRFServiceLookupThread;

class CRFPlugin : public CNSLPlugin
{
public:
                                CRFPlugin				( void );
                                ~CRFPlugin				( void );
    
    virtual sInt32				InitPlugin				( void );

            CFStringRef			GetRecentFolderName		( void )	{ return mRecentServersFolderName; }
            CFStringRef			GetFavoritesFolderName	( void )	{ return mFavoritesServersFolderName; }

protected:
    virtual CFStringRef			GetBundleIdentifier		( void );
    virtual const char*			GetProtocolPrefixString	( void );		// this is used for top of the node's path "NSL"
    virtual Boolean 			IsLocalNode				( const char *inNode );
    
    virtual void				NewNodeLookup			( void );		
    virtual	void				NewServiceLookup		( char* serviceType, CNSLDirNodeRep* nodeDirRep );
    virtual Boolean				OKToOpenUnPublishedNode	( const char* parentNodeName );        

#if SUPPORT_WRITE_ACCESS
    virtual	sInt32				OpenRecord				( sOpenRecord *inData );
    virtual	sInt32				CloseRecord				( sCloseRecord *inData );
    virtual	sInt32				CreateRecord			( sCreateRecord *inData );
    virtual	sInt32				DeleteRecord			( sDeleteRecord *inData );
    virtual	sInt32				FlushRecord				( sFlushRecord *inData );
    virtual	sInt32				AddAttributeValue		( sAddAttributeValue *inData );
    virtual	sInt32				RemoveAttribute			( sRemoveAttribute *inData );
    virtual	sInt32				RemoveAttributeValue	( sRemoveAttributeValue *inData );
    virtual	sInt32				SetAttributeValue		( sSetAttributeValue *inData );
    
            sInt32				RegisterService			( CFDictionaryRef service );
            sInt32				DeregisterService		( CFDictionaryRef service );
#endif // SUPPORT_WRITE_ACCESS    
    virtual	sInt32				HandleNetworkTransition	( sHeader *inData );
private:
        CFStringRef				mRecentServersFolderName;
        CFStringRef				mFavoritesServersFolderName;
};

#endif


