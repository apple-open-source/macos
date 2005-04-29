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
 *  @header CNBPPlugin
 */

#include "CNBPPlugin.h"
#include "CNBPNodeLookupThread.h"
#include "CNBPServiceLookupThread.h"
#include "CNSLTimingUtils.h"

const CFStringRef	gBundleIdentifier = CFSTR("com.apple.DirectoryService.AppleTalk");
const char*		gProtocolPrefixString = "AppleTalk";
pthread_mutex_t	gNBPLocalZoneLock = PTHREAD_MUTEX_INITIALIZER;

#pragma warning "Need to get our default Node String from our resource"

extern "C" {
CFUUIDRef ModuleFactoryUUID = CFUUIDGetConstantUUIDWithBytes ( NULL, \
								0xBB, 0x49, 0xB1, 0x1D, 0x9B, 0x3B, 0x11, 0xD5, \
								0x8D, 0xF5, 0x00, 0x30, 0x65, 0x3D, 0x61, 0xE4 );

}

static CDSServerModule* _Creator ( void )
{
	DBGLOG( "Creating new NBP Plugin\n" );
    return( new CNBPPlugin );
}

CDSServerModule::tCreator CDSServerModule::sCreator = _Creator;

CNBPPlugin::CNBPPlugin( void )
    : CNSLPlugin()
{
	DBGLOG( "CNBPPlugin::CNBPPlugin\n" );
    mLocalNodeString = NULL;
	mNBPSCRef = NULL;
}

CNBPPlugin::~CNBPPlugin( void )
{
	DBGLOG( "CNBPPlugin::~CNBPPlugin\n" );
    
    if ( mLocalNodeString );
        free( mLocalNodeString );

    mLocalNodeString = NULL;
	
	if ( mNBPSCRef )
		CFRelease( mNBPSCRef );
		
	mNBPSCRef = NULL;
}

boolean_t AppleTalkChangedNotificationCallback(SCDynamicStoreRef session, void *callback_argument);
boolean_t AppleTalkChangedNotificationCallback(SCDynamicStoreRef session, void *callback_argument)
{                       
    CNBPPlugin*		plugin = (CNBPPlugin*)callback_argument;
    
	if ( plugin->IsActive() )
	{
		// do nothing by default
		DBGLOG( "*****AppleTalk Network Change Detected******\n" );
		
		SmartSleep(1*USEC_PER_SEC);
		CFArrayRef	changedKeys;
		CFIndex		numKeys = 0;
		
		changedKeys = SCDynamicStoreCopyNotifiedKeys(session);
		
		if ( changedKeys )
		{
			numKeys = ::CFArrayGetCount(changedKeys);
			
			for ( CFIndex i = 0; i < numKeys; i++ )
			{
				if ( CFStringHasSuffix( (CFStringRef)::CFArrayGetValueAtIndex( changedKeys, i ), kSCEntNetAppleTalk ) )
				{
					plugin->StartNodeLookup();			// and then start all over
					
					break;								// this is all we care about
				}
			}
			
			::CFRelease( changedKeys );
		}
	}
	
	return true;				// return whether everything went ok
} // AppleTalkChangedNotificationCallback

sInt32 CNBPPlugin::InitPlugin( void )
{
    sInt32				siResult	= eDSNoErr;
	
    DBGLOG( "CNBPPlugin::InitPlugin\n" );

	return siResult;
}

CFStringRef CNBPPlugin::GetBundleIdentifier( void )
{
    return gBundleIdentifier;
}

// this is used for top of the node's path "NSL"
const char*	CNBPPlugin::GetProtocolPrefixString( void )
{		
    return gProtocolPrefixString;
}


Boolean CNBPPlugin::IsLocalNode( const char *inNode )
{
    Boolean result = false;
    
    pthread_mutex_lock(&gNBPLocalZoneLock);
    
	if ( mLocalNodeString )
    {
        result = ( strcmp( inNode, mLocalNodeString ) == 0 );
    }
    
	pthread_mutex_unlock(&gNBPLocalZoneLock);
    
    return result;
}

void CNBPPlugin::SetLocalZone( const char* zone )
{
    pthread_mutex_lock(&gNBPLocalZoneLock);
    
	if ( zone )
    {
        if ( mLocalNodeString )
            free( mLocalNodeString );
        
        mLocalNodeString = (char*)malloc( strlen(zone) + 1 );
        strcpy( mLocalNodeString, zone );
    }

    pthread_mutex_unlock(&gNBPLocalZoneLock);
}

sInt32 CNBPPlugin::SetServerIdleRunLoopRef( CFRunLoopRef idleRunLoopRef )
{
	CNSLPlugin::SetServerIdleRunLoopRef( idleRunLoopRef );		// let parent take care of business
	
	// now we can start some stuff running.
	if ( !mNBPSCRef && GetRunLoopRef() )
	{
		SInt32				scdStatus	= 0;
		mNBPSCRef = ::SCDynamicStoreCreate(NULL, gBundleIdentifier, NULL, NULL);
		
		if ( !mNBPSCRef )
			DBGLOG( "CNBPPlugin::SetServerIdleRunLoopRef, mNBPSCRef is NULL after a call to SCDynamicStoreCreate!" );
		
		CFMutableArrayRef notifyKeys		= CFArrayCreateMutable(	kCFAllocatorDefault,
																	0,
																	&kCFTypeArrayCallBacks);
		CFMutableArrayRef notifyPatterns	= CFArrayCreateMutable(	kCFAllocatorDefault,
																	0,
																	&kCFTypeArrayCallBacks);
	
		DBGLOG( "CNBPPlugin::SetServerIdleRunLoopRef adding key kSCEntNetAppleTalk:\n" );
		CFStringRef atKey = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetAppleTalk);
		CFArrayAppendValue(notifyKeys, atKey);
		CFRelease(atKey);
	
		SCDynamicStoreSetNotificationKeys(mNBPSCRef, notifyKeys, notifyPatterns);
	
		CFRelease(notifyKeys);
		CFRelease(notifyPatterns);
	
		::CFRunLoopAddCommonMode( GetRunLoopRef(), kCFRunLoopDefaultMode );
		scdStatus = ::SCDynamicStoreNotifyCallback( mNBPSCRef, GetRunLoopRef(), AppleTalkChangedNotificationCallback, this );
		DBGLOG( "CNBPPlugin::SetServerIdleRunLoopRef, SCDynamicStoreNotifyCallback returned %ld\n", scdStatus );
	}
	else
		DBGLOG( "CNBPPlugin::SetServerIdleRunLoopRef, No Current Run Loop, couldn't store Notify callback\n" );

	
	return eDSNoErr;
}

Boolean CNBPPlugin::PluginSupportsServiceType( const char* serviceType )
{
	Boolean		serviceTypeSupported = false;		// default to true except for the following
	
	if ( serviceType && strcmp( serviceType, kDSStdRecordTypeAFPServer ) == 0 )
		serviceTypeSupported = true;
		
	return serviceTypeSupported;
}

void CNBPPlugin::NewNodeLookup( void )
{
	DBGLOG( "CNBPPlugin::NewNodeLookup - **** ATStack: %d ****\n", checkATStack() );
    
    if ( GetATStackState() == noErr )
    {
        CNBPNodeLookupThread* newLookup = new CNBPNodeLookupThread( this );
        
        newLookup->Resume();
    }
    else
	{
        DBGLOG( "CNBPPlugin::NewNodeLookup, ignoring lookup and clearing out all Zones as GetATStackState didn't return noErr\n" );
		ClearOutAllNodes();
	}
}

void CNBPPlugin::NewServiceLookup( char* serviceType, CNSLDirNodeRep* nodeDirRep )
{
	DBGLOG( "CNBPPlugin::NewServicesLookup\n" );

    if ( GetATStackState() == noErr )
    {
        if ( OKToSearchThisType(serviceType) )
        {
            CNBPServiceLookupThread* newLookup = new CNBPServiceLookupThread( this, serviceType, nodeDirRep );
            
            // if we have too many threads running, just queue this search object and run it later
            if ( OKToStartNewSearch() )
                newLookup->Resume();
            else
                QueueNewSearch( newLookup );
        }
        else
            DBGLOG( "CNBPPlugin::NewServiceLookup, ignoring lookup as we don't support NBP lookups on type: %s\n", serviceType );
    }
    else
        DBGLOG( "CNBPPlugin::NewServiceLookup, ignoring lookup as GetATStackState didn't return noErr\n" );
}

Boolean CNBPPlugin::OKToSearchThisType( char* serviceType )
{
    // there are some common types we'll see that we don't even care to search for...
    Boolean 	okToSearch = true;
    
    if ( strcmp( serviceType, "smb" ) == 0 )
        okToSearch = false;
    else if ( strcmp( serviceType, "cifs" ) == 0 )
        okToSearch = false;
    else if ( strcmp( serviceType, "webdav" ) == 0 )
        okToSearch = false;
    else if ( strcmp( serviceType, "nfs" ) == 0 )
        okToSearch = false;
    else if ( strcmp( serviceType, "ftp" ) == 0 )
        okToSearch = false;
    else if ( strcmp( serviceType, "http" ) == 0 )
        okToSearch = false;
    else if ( strcmp( serviceType, "radmin" ) == 0 )
        okToSearch = false;
    else if ( strcmp( serviceType, "radminx" ) == 0 )
        okToSearch = false;
    else if ( strcmp( serviceType, "file" ) == 0 )
        okToSearch = false;
    
    return okToSearch;
}

Boolean CNBPPlugin::OKToOpenUnPublishedNode( const char* parentNodeName )
{
    return false;
}

