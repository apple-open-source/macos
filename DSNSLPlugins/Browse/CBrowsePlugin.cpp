/*
 *  CBrowsePlugin.cpp
 *  DSCBrowsePlugin
 *
 *  Created by imlucid on Wed Aug 27 2001.
 *  Copyright (c) 2002 Apple Computer. All rights reserved.
 *
 */

#include "CBrowsePlugin.h"
#include "CBrowseNodeLookupThread.h"
#include "CBrowseServiceLookupThread.h"
#include "TGetCFBundleResources.h"

#define kNoZoneLabel					"*"

const CFStringRef	gBundleIdentifier = CFSTR("com.apple.DirectoryService.Browse");
const char*			gProtocolPrefixString = "Browse";


#pragma warning "Need to get our default Node String from our resource"

extern "C" {
CFUUIDRef ModuleFactoryUUID = CFUUIDGetConstantUUIDWithBytes ( NULL, \
								0x29, 0xF2, 0xEC, 0xB7, 0x0A, 0xE2, 0x11, 0xD6, \
								0x93, 0x59, 0x00, 0x03, 0x93, 0x4F, 0xB0, 0x10 );

}

static CDSServerModule* _Creator ( void )
{
	DBGLOG( "Creating new Browse Plugin\n" );
    return( new CBrowsePlugin );
}

CDSServerModule::tCreator CDSServerModule::sCreator = _Creator;

CBrowsePlugin::CBrowsePlugin( void )
    : CNSLPlugin()
{
	DBGLOG( "CBrowsePlugin::CBrowsePlugin\n" );
    mLocalNodeString = NULL;
}

CBrowsePlugin::~CBrowsePlugin( void )
{
	DBGLOG( "CBrowsePlugin::~CBrowsePlugin\n" );
    
    if ( mLocalNodeString );
        free( mLocalNodeString );

    mLocalNodeString = NULL;

}

sInt32 CBrowsePlugin::InitPlugin( void )
{
    sInt32				siResult	= eDSNoErr;
	
    DBGLOG( "CBrowsePlugin::InitPlugin\n" );

    {
        mLocalNodeString = (char*)malloc( strlen("Browse") + 1 );
        strcpy( mLocalNodeString, "Browse" );
    }
    
    DBGLOG( "CBrowsePlugin::InitPlugin, setting our current Node to %s\n", mLocalNodeString );
    
    return siResult;
}

CFStringRef CBrowsePlugin::GetBundleIdentifier( void )
{
    return gBundleIdentifier;
}

// this is used for top of the node's path "NSL"
const char*	CBrowsePlugin::GetProtocolPrefixString( void )
{		
    return gProtocolPrefixString;
}


Boolean CBrowsePlugin::IsLocalNode( const char *inNode )
{
    Boolean result = false;
    
    if ( mLocalNodeString )
    {
        result = ( strcmp( inNode, mLocalNodeString ) == 0 );
    }
    
    return result;
}


void CBrowsePlugin::NewNodeLookup( void )
{
	DBGLOG( "CBrowsePlugin::NewNodeLookup\n" );

    CBrowseNodeLookupThread* newLookup = new CBrowseNodeLookupThread( this );
    
    newLookup->Resume();
}

void CBrowsePlugin::NewServiceLookup( char* serviceType, CNSLDirNodeRep* nodeDirRep )
{
	DBGLOG( "CBrowsePlugin::NewServicesLookup\n" );

    CBrowseServiceLookupThread* newLookup = new CBrowseServiceLookupThread( this, serviceType, nodeDirRep );
    
    newLookup->Resume();
}

Boolean CBrowsePlugin::OKToOpenUnPublishedNode( const char* parentNodeName )
{
    return false;
}

