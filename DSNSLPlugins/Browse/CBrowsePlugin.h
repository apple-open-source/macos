/*
 *  CBrowsePlugin.h
 *
 *  Created by imlucid on Wed Aug 15 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
 */

#ifndef _CBrowsePlugin_
#define _CBrowsePlugin_

#include "CNSLPlugin.h"

#define kProtocolPrefixPlainStr		"Browse"
#define kProtocolPrefixStr			"/Browse"
#define kProtocolPrefixSlashStr		"/Browse/"

#define kBrowsePluginThreadUSleepOnCount					25				// thread yield after reporting this number of items
#define kBrowsePluginThreadUSleepInterval				2500			// number of miliseconds to pause the thread

#define kMaxZonesOnTryOne			1000
#define kMaxServicesOnTryOne		1000

class CBrowsePlugin : public CNSLPlugin
{
public:
                                CBrowsePlugin				( void );
                                ~CBrowsePlugin				( void );
    
    virtual sInt32				InitPlugin				( void );

            Boolean				IsScopeInReturnList		( const char* scope );
            void				AddResult				( const char* url );
    
            uInt32				fSignature;

protected:
    virtual CFStringRef			GetBundleIdentifier		( void );
    virtual const char*			GetProtocolPrefixString	( void );		// this is used for top of the node's path "NSL"
    virtual Boolean 			IsLocalNode				( const char *inNode );
    
    virtual void				NewNodeLookup			( void );		// this should fire off some threads in the subclass
    virtual	void				NewServiceLookup		( char* serviceType, CNSLDirNodeRep* nodeDirRep );
    virtual Boolean				OKToOpenUnPublishedNode	( const char* parentNodeName );    

private:
            char*				mLocalNodeString;		

};

#endif


