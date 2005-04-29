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

#include <SystemConfiguration/SystemConfiguration.h>

#include "CNSLHeaders.h"

#include "CommandLineUtilities.h"
#include "SLPSystemConfiguration.h"

#include "CSLPPlugin.h"
#include "CSLPNodeLookupThread.h"
#include "CSLPServiceLookupThread.h"
#include "CNSLTimingUtils.h"

void AddToAttributeList( const void* key, const void* value, void* context );
CFStringRef CreateHexEncodedString( CFStringRef rawStringRef );
CFStringRef CreateEncodedString( CFStringRef rawStringRef );
CFStringRef	CreateSLPTypeFromDSType ( CFStringRef inDSType );

#define kCommandParamsID				128

const CFStringRef	kDSStdAttrTypePrefixSAFE_CFSTR = CFSTR(kDSStdAttrTypePrefix);
const CFStringRef	kDSNativeAttrTypePrefixSAFE_CFSTR = CFSTR(kDSNativeAttrTypePrefix);

const CFStringRef	kDSSLPPluginTagSAFE_CFSTR = CFSTR("com.apple.DirectoryService.DSSLPPlugIn");
const CFStringRef	kSLPTagSAFE_CFSTR = CFSTR("com.apple.slp");
const CFStringRef	gBundleIdentifier = CFSTR("com.apple.DirectoryService.SLP");
const CFStringRef	kSLPDefaultRegistrationScopeTagSAFE_CFSTR = CFSTR("com.apple.slp.defaultRegistrationScope");
const CFStringRef	kAttributeListSAFE_CFSTR = CFSTR("attributeList");
const CFStringRef	kAttributeListForURLSAFE_CFSTR = CFSTR("attributeForURL");

const char*			gProtocolPrefixString = "SLP";
static char*		gLocalNodeString = NULL;				// get this from SCPreferences

extern "C" {
CFUUIDRef ModuleFactoryUUID = CFUUIDGetConstantUUIDWithBytes ( NULL, \
								0x55, 0xD9, 0xE4, 0x58, 0x9B, 0x3B, 0x11, 0xD5, \
								0xAB, 0xFD, 0x00, 0x30, 0x65, 0x3D, 0x61, 0xE4 );

}

static CDSServerModule* _Creator ( void )
{
	DBGLOG( "Creating new SLP Plugin\n" );
    return( new CSLPPlugin );
}

CDSServerModule::tCreator CDSServerModule::sCreator = _Creator;

CSLPPlugin::CSLPPlugin( void )
    : CNSLPlugin()
{
	DBGLOG( "CSLPPlugin::CSLPPlugin\n" );
	mNodeLookupThread = NULL;
	mSLPRef = NULL;
}

CSLPPlugin::~CSLPPlugin( void )
{
	DBGLOG( "CSLPPlugin::~CSLPPlugin\n" );
	if ( mSLPRef )
		SLPClose( mSLPRef );
}

sInt32 CSLPPlugin::InitPlugin( void )
{
    sInt32				siResult	= eDSNoErr;
#ifdef TRACK_FUNCTION_TIMES
	CFAbsoluteTime		startTime = CFAbsoluteTimeGetCurrent();
#endif
	DBGLOG( "CSLPPlugin::InitPlugin\n" );
    
   if ( getenv( "NSLDEBUG" ) )
        SLPSetProperty("com.apple.slp.logAll", "true" );
        
    if ( getenv( "com.apple.slp.interface" ) != 0 )
    {
        SLPSetProperty("com.apple.slp.interface", getenv( "com.apple.slp.interface" ) );
        DBGLOG( "Setting interface to: %s", getenv( "com.apple.slp.interface" ) );
    }
    
    if ( getenv( "net.slp.DAAddresses" ) != 0 )
    {
        SLPSetProperty("net.slp.DAAddresses", getenv( "net.slp.DAAddresses" ) );
        DBGLOG( "Setting da to: %s", getenv( "net.slp.DAAddresses" ) );
    }

    SCPreferencesRef prefRef = ::SCPreferencesCreate( NULL, kDSSLPPluginTagSAFE_CFSTR, kSLPTagSAFE_CFSTR );
    
	executecommand( "rm /var/slp.regfile" );
	
	DBGLOG( "CSLPPlugin::InitPlugin is deleting regfile:/var/slp.regfile" );
	
	CFPropertyListRef	propertyList = ::SCPreferencesGetValue( prefRef, kSLPDefaultRegistrationScopeTagSAFE_CFSTR );

	// let's free the global if it is set already...
	if( gLocalNodeString )
	{
		free( gLocalNodeString );
		gLocalNodeString = NULL;
	}
	
    if ( propertyList )
    {
        if ( ::CFGetTypeID(propertyList) == ::CFStringGetTypeID() )
        {
            // cool, just a single scope registration        
            CFIndex		tempPtrSize = ::CFStringGetMaximumSizeForEncoding(CFStringGetLength((CFStringRef)propertyList), kCFStringEncodingUTF8)+1;        	
			char* 		tempPtr = (char*)malloc( tempPtrSize );
            
            if ( ::CFStringGetCString( (CFStringRef)propertyList, tempPtr, tempPtrSize, kCFStringEncodingUTF8 ) )
            {
                gLocalNodeString = tempPtr;
            }
            else
                free( tempPtr );
        }
        else if ( ::CFGetTypeID(propertyList) == ::CFArrayGetTypeID() )
        {
            // ah, we are configured with multiple scopes to register in...
            DBGLOG( "CSLPPlugin::InitPlugin we don't support multiple default registration scopes yet!\n" );
        }
    }
    else
    {
        CFStringRef		localNodeRef = NULL;
        // look in the config file and register the preference
        gLocalNodeString = CreateLocalNodeFromConfigFile();
        
        localNodeRef = ::CFStringCreateWithCString( NULL, gLocalNodeString, kCFStringEncodingUTF8 );
        
        if ( ::SCPreferencesLock( prefRef, true ) )
        {
            ::SCPreferencesSetValue( prefRef, kSLPDefaultRegistrationScopeTagSAFE_CFSTR, localNodeRef );
            ::SCPreferencesUnlock( prefRef );
        }
        
        ::CFRelease( localNodeRef );
    }
	::CFRelease( prefRef );

#ifdef USE_DEFAULT_SA_ONLY_SCOPE    
    if ( gLocalNodeString && strcmp( gLocalNodeString, v2_Default_Scope ) == 0 )
    {
        free( gLocalNodeString );
        gLocalNodeString = (char*)malloc( strlen( SLP_DEFAULT_SA_ONLY_SCOPE )+1 );
        strcpy( gLocalNodeString, SLP_DEFAULT_SA_ONLY_SCOPE );
    }
#endif
    DBGLOG( "CSLPPlugin::InitPlugin we are currently located in scope %s\n", gLocalNodeString );
    
    AddNode( GetLocalNodeString() );		// always register the default registration node
    
#ifdef TRACK_FUNCTION_TIMES
	ourLog( "CSLPPlugin::InitPlugin() took %f seconds\n", CFAbsoluteTimeGetCurrent()-startTime );
#endif
    return siResult;
}

void CSLPPlugin::ActivateSelf( void )
{
#ifdef TRACK_FUNCTION_TIMES
	CFAbsoluteTime		startTime = CFAbsoluteTimeGetCurrent();
#endif
    DBGLOG( "CSLPPlugin::ActivateSelf called, runloopRef = 0x%x\n", GetRunLoopRef() );
	
	InitPlugin();
	
	if ( !mSLPRef && GetRunLoopRef() && IsNetworkSetToTriggerDialup() == false )
		SLPOpen( "en", SLP_FALSE, &mSLPRef, GetRunLoopRef() );
		
	// we are getting activated.  If we have anything registered, we will want to
	// kick off slpd
    
#ifdef TRACK_FUNCTION_TIMES
	ourLog( "CSLPPlugin::ActivateSelf() took %f seconds\n", CFAbsoluteTimeGetCurrent()-startTime );
#endif

	CNSLPlugin::ActivateSelf();
}

void CSLPPlugin::DeActivateSelf( void )
{
	// we are getting deactivated.  We want to tell the slpd daemon to shutdown as
	// well as deactivate our DA Listener
    DBGLOG( "CSLPPlugin::DeActivateSelf called\n" );
	
	StopSLPDALocator();

	TellSLPdToQuit();
	
	if ( mNodeLookupThread )
		mNodeLookupThread->Cancel();
		
	if ( mSLPRef )
		SLPClose( mSLPRef );
	mSLPRef = NULL;

	CNSLPlugin::DeActivateSelf();
}

void CSLPPlugin::TellSLPdToQuit( void )
{
	uInt32		bufLen = 0, returnBufLen = 0;
	char*		sendBuf = MakeSLPIOCallBuffer( false, &bufLen );
	char*		returnBuf = NULL;
	
	SendDataToSLPd( sendBuf, bufLen, &returnBuf, &returnBufLen );
	
	if ( sendBuf )
		free( sendBuf );
	
	if ( returnBuf )
		free( returnBuf );
}

sInt32 CSLPPlugin::SetServerIdleRunLoopRef( CFRunLoopRef idleRunLoopRef )
{
	CNSLPlugin::SetServerIdleRunLoopRef( idleRunLoopRef );
	
	return eDSNoErr;
}

char* CSLPPlugin::CreateLocalNodeFromConfigFile( void )
{
    const char*	defaultRegScope = NULL;
	char*	localNodeString = (char*)malloc( strlen( v2_Default_Scope ) +1 );
    strcpy( localNodeString, v2_Default_Scope );

    SLPReadConfigFile( "/etc/slpsa.conf" );
    
	defaultRegScope = SLPGetProperty("com.apple.slp.defaultRegistrationScope");
	
    if ( defaultRegScope && strcmp(localNodeString,defaultRegScope) != 0 )
    {
        free( localNodeString );
        localNodeString = (char*)malloc( strlen( defaultRegScope ) +1 );
        strcpy( localNodeString, defaultRegScope );
    }
    
    return localNodeString;
}

CFStringRef CSLPPlugin::GetBundleIdentifier( void )
{
    return gBundleIdentifier;
}

// this is used for top of the node's path "SLP"
const char*	CSLPPlugin::GetProtocolPrefixString( void )
{		
    return gProtocolPrefixString;
}

// this is where our machine is currently located
const char* CSLPPlugin::GetLocalNodeString( void )
{
    return gLocalNodeString;
}

Boolean CSLPPlugin::IsLocalNode( const char *inNode )
{
    Boolean result = false;
    
    if ( gLocalNodeString )
    {
        result = ( strcmp( inNode, gLocalNodeString ) == 0 );
    }
    
    return result;
}

Boolean CSLPPlugin::IsADefaultOnlyNode( const char *inNode )
{
    Boolean result = false;
    
#ifdef USE_DEFAULT_SA_ONLY_SCOPE    
	DBGLOG( "CSLPPlugin::IsADefaultOnlyNode called on (%s)\n", inNode );

    if ( strcmp( inNode, SLP_DEFAULT_SA_ONLY_SCOPE ) == 0 )
        result = true;
#endif
        
    return result;
}

sInt32 CSLPPlugin::HandleNetworkTransition( sHeader *inData )
{
    // we not only need to reset the scopes we have published, but we need to reset our thread that is looking for DAs
    sInt32					siResult			= eDSNoErr;
	
	siResult = CNSLPlugin::HandleNetworkTransition( inData );
    
	if ( !siResult )
	{
		if ( mActivatedByNSL && IsActive() && IsNetworkSetToTriggerDialup() == false )
		{
#ifdef TRACK_FUNCTION_TIMES
	CFAbsoluteTime		startTime = CFAbsoluteTimeGetCurrent();
#endif
			KickSLPDALocator();			// wake this guy
    
#ifdef TRACK_FUNCTION_TIMES
	ourLog( "CSLPPlugin::HandleNetworkTransition(), KickSLPDALocator() took %f seconds\n", CFAbsoluteTimeGetCurrent()-startTime );
#endif
		}
	}
	
    return ( siResult );
}

void CSLPPlugin::NodeLookupComplete( void )
{
	CNSLPlugin::NodeLookupComplete();
	DBGLOG( "CSLPPlugin::NodeLookupComplete\n" );
	mNodeLookupThread = NULL;
} // NodeLookupComplete

void CSLPPlugin::NewNodeLookup( void )
{
	DBGLOG( "CSLPPlugin::NewNodeLookup\n" );
	
	if ( mActivatedByNSL && IsActive() && IsNetworkSetToTriggerDialup() == false )
	{
		// First add our local scope
		AddNode( GetLocalNodeString() );		// always register the default registration node
		
		if ( !mNodeLookupThread )
		{
			DBGLOG( "CSLPPlugin::NewNodeLookup creating a new CSLPNodeLookupThread\n" );
			mNodeLookupThread = new CSLPNodeLookupThread( this );
			
			mNodeLookupThread->Resume();
		}
		else
		{
			DBGLOG( "CSLPPlugin::NewNodeLookup telling mNodeLookupThread to DoItAgain\n" );
			mNodeLookupThread->DoItAgain();		// we need to start a new one as soon as the old one finishes
		}
	}
}

void CSLPPlugin::NewServiceLookup( char* serviceType, CNSLDirNodeRep* nodeDirRep )
{
	if ( serviceType && (strcmp( serviceType, "cifs" ) == 0 || strcmp( serviceType, "webdav" ) == 0) )
	{
		// ignore these types
		DBGLOG( "CSLPPlugin::NewServicesLookup ignoring lookup on %s\n", serviceType );
	}
	else if ( mActivatedByNSL && IsActive() && IsNetworkSetToTriggerDialup() == false )
	{
		DBGLOG( "CSLPPlugin::NewServicesLookup on %s\n", serviceType );
		
		CSLPServiceLookupThread* newLookup = new CSLPServiceLookupThread( this, serviceType, nodeDirRep );
		
		// if we have too many threads running, just queue this search object and run it later
		if ( OKToStartNewSearch() )
		{
			DBGLOG( "CSLPPlugin::NewServicesLookup on %s is being started.\n", serviceType );
			newLookup->Resume();
		}
		else
		{
			DBGLOG( "CSLPPlugin::NewServicesLookup on %s is being queued.\n", serviceType );
			QueueNewSearch( newLookup );
		}
	}
}


Boolean CSLPPlugin::OKToOpenUnPublishedNode( const char* nodeName )
{
    return true;	// allow users to create their own nodes?
}

#pragma mark -
sInt32 CSLPPlugin::RegisterService( tRecordReference recordRef, CFDictionaryRef service )
{
    sInt32		status = eDSNoErr;
    CFStringRef	urlRef = NULL;
    CFStringRef	scopeRef = NULL;
	CFTypeRef	urlResultRef = NULL;
	
    if ( service )
		urlResultRef = (CFTypeRef)::CFDictionaryGetValue( service, kDSNAttrURLSAFE_CFSTR);

	if ( urlResultRef )
	{
		if ( CFGetTypeID(urlResultRef) == CFStringGetTypeID() )
		{
			urlRef = (CFStringRef)urlResultRef;
		}
		else if ( CFGetTypeID(urlResultRef) == CFArrayGetTypeID() )
		{
            DBGLOG( "CSLPPlugin::RegisterService, we have more than one URL (%ld) in this service! Just register the first one\n", CFArrayGetCount((CFArrayRef)urlResultRef) );
			urlRef = (CFStringRef)::CFArrayGetValueAtIndex( (CFArrayRef)urlResultRef, 0 );
		}
	}
    
	DBGLOG( "CSLPPlugin::RegisterService\n" );
    if ( getenv("NSLDEBUG") && service )
        CFShow(service);
	
	if ( service && urlRef && CFGetTypeID(urlRef) == CFStringGetTypeID() && CFStringGetLength( urlRef ) > 0 )
    {
        UInt32		scopePtrLength = 0;
        char*		scopePtr = NULL;
        
        DBGLOG( "CSLPPlugin::RegisterService, check for specified location to register in\n" );
        if ( (scopeRef = (CFStringRef)::CFDictionaryGetValue( service, kDS1AttrLocationSAFE_CFSTR )) )
        {
        	if ( CFGetTypeID( scopeRef ) == CFArrayGetTypeID() )
            {
                scopeRef = (CFStringRef)::CFArrayGetValueAtIndex( (CFArrayRef)scopeRef, 0 );	// just get the first one for now
            }
                
            if ( scopeRef != NULL )
			{
				scopePtrLength = ::CFStringGetMaximumSizeForEncoding( ::CFStringGetLength( scopeRef ), kCFStringEncodingUTF8 ) + 1;
				scopePtr = (char*)malloc( scopePtrLength );

				::CFStringGetCString( scopeRef, scopePtr, scopePtrLength, kCFStringEncodingUTF8 );
			}
        }

		if ( scopePtr == NULL )
        {
            DBGLOG( "CSLPPlugin::RegisterService, no location specified, using empty scope for default\n" );
            scopePtr = (char*)malloc(1);
            scopePtr[0] = '\0';
        }
        
        UInt32		urlPtrLength = ::CFStringGetMaximumSizeForEncoding( ::CFStringGetLength( urlRef ), kCFStringEncodingUTF8 ) + 1;
        char*		urlPtr = (char*)malloc( urlPtrLength );
        
        ::CFStringGetCString( urlRef, urlPtr, urlPtrLength, kCFStringEncodingUTF8 );
        
        if ( urlPtr[0] != '\0' )
        {
            char*					attributePtr = NULL;            
#ifdef REGISTER_WITH_ATTRIBUTE_DATA_IN_URL
            CFMutableStringRef		attributeRef = ::CFStringCreateMutable( NULL, 0 );
            CFMutableStringRef		attributeForURLRef = ::CFStringCreateMutable( NULL, 0 );	// mod this for appending to URL
            CFMutableDictionaryRef	attributesDictRef = ::CFDictionaryCreateMutable( NULL, 2, &kCFTypeDictionaryKeyCallBacks, & kCFTypeDictionaryValueCallBacks );
            
            ::CFDictionaryAddValue( attributesDictRef, kAttributeListSAFE_CFSTR, attributeRef );
            ::CFDictionaryAddValue( attributesDictRef, kAttributeListForURLSAFE_CFSTR, attributeForURLRef );
            
            ::CFDictionaryApplyFunction( service, AddToAttributeList, attributesDictRef );

            CFStringInsert( attributeForURLRef, 0, urlRef );
            
            free( urlPtr );
            
            urlPtrLength = ::CFStringGetMaximumSizeForEncoding( ::CFStringGetLength( attributeForURLRef ), kCFStringEncodingUTF8 ) + 1;
            urlPtr = (char*)malloc( urlPtrLength );

            ::CFStringGetCString( attributeForURLRef, urlPtr, urlPtrLength, kCFStringEncodingUTF8 );
            
            CFIndex		attributePtrSize = ::CFStringGetMaximumSizeForEncoding( CFStringGetLength(attributeRef), kCFStringEncodingUTF8) + 1;
            attributePtr = (char*)malloc( attributePtrSize );
            attributePtr[0] = '\0';
            
            ::CFStringGetCString( attributeRef, attributePtr, attributePtrSize, kCFStringEncodingUTF8 );
            
            if ( attributePtr && attributePtr[strlen(attributePtr)-1] == ',' )
                attributePtr[strlen(attributePtr)] = '\0';
                
            ::CFRelease( attributeRef );
#else
			attributePtr = (char*)malloc( 1 );
            attributePtr[0] = '\0';
#endif            
            status = DoSLPRegistration( scopePtr, urlPtr, attributePtr );		// we should be putting together an attribute list from the other key/value pairs in service
            
            if ( attributePtr )
                free( attributePtr );
        }
            
        if ( scopePtr )
			free( scopePtr );
        
		if ( urlPtr )
			free( urlPtr );
    }
    else
        status = eDSNullAttribute;
        
    return status;
}

sInt32 CSLPPlugin::DeregisterService( tRecordReference recordRef, CFDictionaryRef service )
{
    sInt32			status = eDSNoErr;
    CFStringRef		urlRef = NULL;
    CFStringRef		scopeRef = NULL;
	CFTypeRef		urlResultRef = NULL;
	
    if ( service )
		urlResultRef = (CFTypeRef)::CFDictionaryGetValue( service, kDSNAttrURLSAFE_CFSTR);

	if ( urlResultRef )
	{
		if ( CFGetTypeID(urlResultRef) == CFStringGetTypeID() )
		{
			urlRef = (CFStringRef)urlResultRef;
		}
		else if ( CFGetTypeID(urlResultRef) == CFArrayGetTypeID() )
		{
            DBGLOG( "CSLPPlugin::DeregisterService, we have more than one URL (%ld) in this service! Just deregister the first one\n", CFArrayGetCount((CFArrayRef)urlResultRef) );
			urlRef = (CFStringRef)::CFArrayGetValueAtIndex( (CFArrayRef)urlResultRef, 0 );
		}
	}
	
	if ( urlRef && CFGetTypeID(urlRef) == CFStringGetTypeID() && CFStringGetLength( urlRef ) > 0 )
    {
        UInt32		scopePtrLength;
        char*		scopePtr = NULL;
        
        if ( (scopeRef = (CFStringRef)::CFDictionaryGetValue( service, kDS1AttrLocationSAFE_CFSTR)) )
        {
        	if ( CFGetTypeID( scopeRef ) == CFArrayGetTypeID() )
            {
                scopeRef = (CFStringRef)::CFArrayGetValueAtIndex( (CFArrayRef)scopeRef, 0 );	// just get the first one for now
            }

        	scopePtrLength = ::CFStringGetMaximumSizeForEncoding( ::CFStringGetLength( scopeRef ), kCFStringEncodingUTF8 ) + 1;
        	scopePtr = (char*)malloc( scopePtrLength );

            ::CFStringGetCString( scopeRef, scopePtr, scopePtrLength, kCFStringEncodingUTF8 );
        }
        else
        {
            scopePtr = (char*)malloc(1);
            scopePtr[0] = '\0';
        }
        
        UInt32		urlPtrLength = ::CFStringGetMaximumSizeForEncoding( ::CFStringGetLength( urlRef ), kCFStringEncodingUTF8 ) + 1;
        char*		urlPtr = (char*)malloc( urlPtrLength );
        
        ::CFStringGetCString( urlRef, urlPtr, urlPtrLength, kCFStringEncodingUTF8 );
        
        if ( urlPtr[0] != '\0' )
        {
#ifdef REGISTER_WITH_ATTRIBUTE_DATA_IN_URL
            char*					attributePtr = NULL;            
            CFMutableStringRef		attributeRef = ::CFStringCreateMutable( NULL, 0 );
            CFMutableStringRef		attributeForURLRef = ::CFStringCreateMutable( NULL, 0 );	// mod this for appending to URL
            CFMutableDictionaryRef	attributesDictRef = ::CFDictionaryCreateMutable( NULL, 2, &kCFTypeDictionaryKeyCallBacks, & kCFTypeDictionaryValueCallBacks );
            
            ::CFDictionaryAddValue( attributesDictRef, kAttributeListSAFE_CFSTR, attributeRef );
            ::CFDictionaryAddValue( attributesDictRef, kAttributeListForURLSAFE_CFSTR, attributeForURLRef );
            
            ::CFDictionaryApplyFunction( service, AddToAttributeList, attributesDictRef );

            CFStringInsert( attributeForURLRef, 0, urlRef );
            
            free( urlPtr );
            
            urlPtrLength = ::CFStringGetMaximumSizeForEncoding( ::CFStringGetLength( attributeForURLRef ), kCFStringEncodingUTF8 ) + 1;
            urlPtr = (char*)malloc( urlPtrLength );

            ::CFStringGetCString( attributeForURLRef, urlPtr, urlPtrLength, kCFStringEncodingASCII );
            
            CFIndex		attributePtrSize = ::CFStringGetMaximumSizeForEncoding( CFStringGetLength(attributeRef), kCFStringEncodingUTF8 ) + 1;
            attributePtr = (char*)malloc( attributePtrSize );
            attributePtr[0] = '\0';
            
            ::CFStringGetCString( attributeRef, attributePtr, attributePtrSize, kCFStringEncodingUTF8 );
            
            if ( attributePtr && attributePtr[strlen(attributePtr)-1] == ',' )
                attributePtr[strlen(attributePtr)] = '\0';
                
            ::CFRelease( attributeRef );
#endif            
        	status = DoSLPDeregistration( scopePtr, urlPtr );
        }
                        
        free( scopePtr );
        free( urlPtr );

    }
    else
        status = eDSNullAttribute;

    return status;
}

typedef struct SLPSendDataContext {
	char*		dataBuffer;
	UInt32		dataBufferLen;
};

void SendDataToSLPdOnRunLoop(CFRunLoopTimerRef timer, void *info)
{
#ifdef TRACK_FUNCTION_TIMES
	CFAbsoluteTime	startTime = CFAbsoluteTimeGetCurrent();
#endif
	SLPSendDataContext*		context = (SLPSendDataContext*)info;
	
	if ( context )
	{
		char*					returnBuffer = NULL;
		UInt32					returnBufferLen = 0;
		OSStatus				status = noErr;

		status = SendDataToSLPd( context->dataBuffer, context->dataBufferLen, &returnBuffer, &returnBufferLen );

		if ( status )
		{
			if ( returnBuffer )
				free( returnBuffer );
			returnBuffer = NULL;
			
	#ifdef TRACK_FUNCTION_TIMES
	ourLog( "SendDataToSLPdOnRunLoop received a status of %d, sleeping a second and will try again\n", status );
	#endif
			SmartSleep(1*USEC_PER_SEC);		// try again
			status = SendDataToSLPd( context->dataBuffer, context->dataBufferLen, &returnBuffer, &returnBufferLen );

			if ( returnBuffer )
				free( returnBuffer );
			returnBuffer = NULL;
		}
				
		// now check for any message status
		if ( !status && returnBuffer && returnBufferLen > 0 )
			status = ((SLPdMessageHeader*)returnBuffer)->messageStatus;
			
		if ( context->dataBuffer )
			free( context->dataBuffer );
			
		if ( returnBuffer )
			free( returnBuffer );
	
		free( context );
	}
#ifdef TRACK_FUNCTION_TIMES
	syslog( LOG_ERR, "(%s) SendDataToSLPdOnRunLoop took %f seconds\n", "SLP", CFAbsoluteTimeGetCurrent() - startTime );
#endif
}

OSStatus CSLPPlugin::DoSLPRegistration( char* scopeList, char* url, char* attributeList )
{
	char*		dataBuffer = NULL;
	UInt32		dataBufferLen = 0;
	OSStatus	status = noErr;

#ifdef TRACK_FUNCTION_TIMES
	CFAbsoluteTime		startTime = CFAbsoluteTimeGetCurrent();
#endif
	
    DBGLOG( "CSLPPlugin::DoSLPRegistration called, scope: %s, url: %s, attributeList: %s\n", scopeList, url, attributeList );
    
	dataBuffer = MakeSLPRegistrationDataBuffer ( scopeList, strlen(scopeList), url, strlen(url), attributeList, strlen(attributeList), &dataBufferLen );

	if ( dataBuffer )
	{
		// let's do the send on our runloop
		SLPSendDataContext*		context = new SLPSendDataContext();
		context->dataBuffer = dataBuffer;
		context->dataBufferLen = dataBufferLen;
		
		CFRunLoopTimerContext c = {0, (void*)context, NULL, NULL, NULL};

		CFRunLoopTimerRef timerRef = CFRunLoopTimerCreate(	NULL,
											CFAbsoluteTimeGetCurrent() + 0,
											0,
											0,
											0,
											SendDataToSLPdOnRunLoop,
											(CFRunLoopTimerContext*)&c);

		CFRunLoopAddTimer( GetRunLoopRef(), timerRef, kCFRunLoopDefaultMode );
		CFRelease( timerRef );
	}
	else
		status = eMemoryAllocError;
        
#ifdef TRACK_FUNCTION_TIMES
	ourLog( "CSLPPlugin::DoSLPRegistration took %f seconds\n", CFAbsoluteTimeGetCurrent()-startTime );
#endif

	return status;
}

OSStatus CSLPPlugin::DoSLPDeregistration( char* scopeList, char* url )
{
	char*		dataBuffer = NULL;
	UInt32		dataBufferLen = 0;
	OSStatus	status = noErr;
	
    DBGLOG( "CSLPPlugin::DoSLPDeregistration called, scope: %s, url: %s\n", scopeList, url );
    
	dataBuffer = MakeSLPDeregistrationDataBuffer( scopeList, strlen(scopeList), url, strlen(url), &dataBufferLen );
	
	if ( dataBuffer )
	{
		// let's do the send on our runloop
		SLPSendDataContext*		context = new SLPSendDataContext();
		context->dataBuffer = dataBuffer;
		context->dataBufferLen = dataBufferLen;
		
		CFRunLoopTimerContext c = {0, (void*)context, NULL, NULL, NULL};

		CFRunLoopTimerRef timerRef = CFRunLoopTimerCreate(	NULL,
											CFAbsoluteTimeGetCurrent() + 0,
											0,
											0,
											0,
											SendDataToSLPdOnRunLoop,
											(CFRunLoopTimerContext*)&c);

		CFRunLoopAddTimer( GetRunLoopRef(), timerRef, kCFRunLoopDefaultMode );
		CFRelease( timerRef );
	}
		
	return status;
}

#pragma mark -
void AddToAttributeList( const void* key, const void* value, void* context )
{
    // for each key value we need to wrap it like one of these:
    // (key),
    // (key=value),
    // (key=value1,value2),
DBGLOG( "AddToAttributeList called with key:%s value:%s", (char*)key, (char*)value );
    // when we are done, we will end up with a string with an extra comma at the end that we will delete later
    if ( key && context )
    {
        CFDictionaryRef				attrDict = (CFDictionaryRef)context;
        CFStringRef					keyRef = (CFStringRef) key;
        CFMutableStringRef			attributeListRef = (CFMutableStringRef)CFDictionaryGetValue( attrDict, kAttributeListSAFE_CFSTR );
        CFMutableStringRef			attributeListForURLRef = (CFMutableStringRef)CFDictionaryGetValue( attrDict, kAttributeListForURLSAFE_CFSTR );
        CFPropertyListRef			valueRef = (CFPropertyListRef) value;
        
        // first check to see if this key is one to ignore
        if ( ::CFStringCompare( kDSNAttrURLSAFE_CFSTR, keyRef, 0 ) == kCFCompareEqualTo )
            return;
        else if ( ::CFStringCompare( kDS1AttrLocationSAFE_CFSTR, keyRef, 0 ) == kCFCompareEqualTo )
            return;

        if ( attributeListRef )
        {
            ::CFStringAppendCString( attributeListRef, "(", kCFStringEncodingUTF8 );
            ::CFStringAppendCString( attributeListForURLRef, ";", kCFStringEncodingUTF8 );		// url needs to be encoded UTF8

            ::CFStringRef	keyConvertedRef = CreateSLPTypeFromDSType( (CFStringRef)keyRef );
            ::CFStringAppend( attributeListRef, keyConvertedRef );
            ::CFStringAppend( attributeListForURLRef, keyConvertedRef );
            ::CFRelease( keyConvertedRef );
            
            if ( valueRef )
            {
                ::CFStringAppendCString( attributeListRef, "=", kCFStringEncodingUTF8 );
                ::CFStringAppendCString( attributeListForURLRef, "=", kCFStringEncodingASCII );
            
                if ( ::CFGetTypeID( valueRef ) == ::CFArrayGetTypeID() )
                {
                    CFIndex		valueCount = CFArrayGetCount((CFArrayRef)valueRef);
					
					for ( CFIndex i=0; i<valueCount; i++ )
                    {
                        CFStringRef valueStringRef = (CFStringRef)::CFArrayGetValueAtIndex( (CFArrayRef)valueRef, i );
                            
                        if ( valueStringRef )
                        {
                            CFStringRef		encodedValueRef = CreateEncodedString(valueStringRef);
                            CFStringRef		hexEncodedValueRef = CreateHexEncodedString(valueStringRef);
                            ::CFStringAppend( attributeListRef, encodedValueRef );
                            ::CFStringAppend( attributeListForURLRef, hexEncodedValueRef );
                            
                            if ( i+1 < valueCount )
                            {
                                ::CFStringAppendCString( attributeListRef, ",", kCFStringEncodingUTF8 );
                                ::CFStringAppendCString( attributeListForURLRef, ",", kCFStringEncodingASCII );
                            }
                            ::CFRelease( encodedValueRef );
                            ::CFRelease( hexEncodedValueRef );
                        }
                    }
                }
                else if ( ::CFGetTypeID( valueRef ) == ::CFStringGetTypeID() )
                {
                    CFStringRef		encodedValueRef = CreateEncodedString((CFStringRef)valueRef);
                    CFStringRef		hexEncodedValueRef = CreateHexEncodedString((CFStringRef)valueRef);
                    ::CFStringAppend( attributeListRef, encodedValueRef );
                    ::CFStringAppend( attributeListForURLRef, hexEncodedValueRef );
                    
                    ::CFRelease( encodedValueRef );
                    ::CFRelease( hexEncodedValueRef );
                }
            }
            
            ::CFStringAppendCString( attributeListRef, "),", kCFStringEncodingUTF8 );
        }
    }
}

CFStringRef CreateEncodedString( CFStringRef rawStringRef )
{
    char*					buffer = NULL;
    UInt32					bufferSize = 0;
    
    bufferSize = ::CFStringGetMaximumSizeForEncoding(CFStringGetLength(rawStringRef), kCFStringEncodingUTF8) +1;	/* Max bytes a string of specified length (in UniChars) will take up if encoded */

    buffer = (char*)malloc( bufferSize );
    
    if ( !buffer )
    {
        DBGLOG( "CreateEncodedString, Couldn't malloc a buffer!\n" );
        return NULL;
    }
    
    if ( ::CFStringGetCString( rawStringRef, buffer, bufferSize, kCFStringEncodingASCII ) )
    {
        // no encoding needed, just pass back a copy
        return CFStringCreateCopy( NULL, rawStringRef );
    }
    else if ( ::CFStringGetCString( rawStringRef, buffer, bufferSize, kCFStringEncodingUTF8 ) )
    {
        CFMutableStringRef		newStringRef = ::CFStringCreateMutable( NULL, 0 );
        char*					curPtr = buffer;
        char					temp[4] = {0};                
        
        DBGLOG( "CreateEncodedString parsing: %s\n", buffer );
        
        while ( *curPtr != '\0' )
        {
            if (*curPtr == '(' || *curPtr == ')' || *curPtr == ',' || *curPtr == '\\' ||
                *curPtr == '!' || *curPtr == '<' || *curPtr == '=' || *curPtr == '>' ||
                *curPtr == '~' || (*curPtr>= 0x00 && *curPtr<= 0x1F) ) 
            {
                // Convert ascii to \xx equivalent
                div_t	result;
                UInt8	hexValue = *curPtr;
                char	c1, c2;
                
                result = div( hexValue, 16 );
                
                if ( (UInt8)(result.quot) < 0xA )
                    c1 = (UInt8)result.quot + '0';
                else
                    c1 = (UInt8)result.quot + 'A' - 10;
                
                if ( (UInt8)(result.rem) < 0xA )
                    c2 = (UInt8)result.rem + '0';
                else
                    c2 = (UInt8)result.rem + 'A' - 10;
                
                temp[0] = '\\';
                temp[1] = c1;
                temp[2] = c2;
    
                ::CFStringAppendCString( newStringRef, temp, kCFStringEncodingASCII );
            }
            else
            {
                sprintf( temp, "%c", *curPtr );
                ::CFStringAppendCString( newStringRef, temp, kCFStringEncodingUTF8 );
            }
            
            curPtr++;
        }
        
        return newStringRef;
    }
    else
        DBGLOG( "CreateEncodedString, Couldn't get UTF8 version of the original string!\n" );
        
    return NULL;
}

CFStringRef CreateHexEncodedString( CFStringRef rawStringRef )
{
    char*					buffer = NULL;
    UInt32					bufferSize = 0;
    
    bufferSize = ::CFStringGetMaximumSizeForEncoding(CFStringGetLength(rawStringRef), kCFStringEncodingUTF8) +1;	/* Max bytes a string of specified length (in UniChars) will take up if encoded */

    buffer = (char*)malloc( bufferSize );
    
    if ( !buffer )
    {
        DBGLOG( "CreateHexEncodedString, Couldn't malloc a buffer!\n" );
        return NULL;
    }
    
	if ( ::CFStringGetCString( rawStringRef, buffer, bufferSize, kCFStringEncodingUTF8 ) )
    {
        CFMutableStringRef		newStringRef = ::CFStringCreateMutable( NULL, 0 );
        char*					curPtr = buffer;
        char					temp[4] = {0};                
        Boolean					encode = false;
        
        DBGLOG( "CreateHexEncodedString parsing: %s\n", buffer );
        
        while ( *curPtr != '\0' )
        {
            char		c = *curPtr;
            
            if ( c <= 0x1F || c == 0x7F || (unsigned char)c >= 0x80 )
                encode = true;
            else
            {
                switch (c)
                {
                    case '<':
                    case '>':
                    case 0x22:	// double quote
                    case 0x5c:	// back slash
                    case '#':
                    case '{':
                    case '}':
                    case '|':
                    case '^':
                    case '~':
                    case '[':
                    case ']':
                    case '`':
                    case ';':
                    case '/':
                    case '?':
                    case ':':
                    case '@':
                    case '=':
                    case '&':
                    case ' ':
                    case ',':
                    case '%':
                        encode = true;
                    break;
                    
                    default:
                        encode = false;
                    break;
                }
            }
            
            if ( encode )
            {
                // Convert ascii to \xx equivalent
                div_t	result;
                UInt8	hexValue = *curPtr;
                char	c1, c2;
                
                result = div( hexValue, 16 );
                
                if ( (UInt8)(result.quot) < 0xA )
                    c1 = (UInt8)result.quot + '0';
                else
                    c1 = (UInt8)result.quot + 'A' - 10;
                
                if ( (UInt8)(result.rem) < 0xA )
                    c2 = (UInt8)result.rem + '0';
                else
                    c2 = (UInt8)result.rem + 'A' - 10;
                
                temp[0] = '\\';
                temp[1] = c1;
                temp[2] = c2;
    
                ::CFStringAppendCString( newStringRef, temp, kCFStringEncodingASCII );
            }
            else
            {
                sprintf( temp, "%c", *curPtr );
                ::CFStringAppendCString( newStringRef, temp, kCFStringEncodingASCII );
            }

            curPtr++;
        }
        
        return newStringRef;
    }
    else
        DBGLOG( "CreateHexEncodedString, Couldn't get UTF8 version of the original string!\n" );
        
    return NULL;
}

#define kSLPNameKey			"name"

// CreateSLPTypeFromDSType
//
// At first we just want to strip off the standard type here, we'll probably want some better mapping, especially between some of
// the standard types (i.e. "dsAttrTypeStandard:RecordName" to "Name", etc.)
CFStringRef	CreateSLPTypeFromDSType ( CFStringRef inDSType )
{
    CFStringRef		result = NULL;
    
    if ( CFStringCompare( inDSType, kDSNAttrRecordNameSAFE_CFSTR, 0 ) == kCFCompareEqualTo )
    {
        result = CFStringCreateWithCString( NULL, kSLPNameKey, kCFStringEncodingUTF8 );
    }    
    else if ( CFStringHasPrefix( inDSType, kDSStdAttrTypePrefixSAFE_CFSTR ) )
    {
        result = CFStringCreateWithSubstring( NULL, inDSType, CFRangeMake( strlen(kDSStdAttrTypePrefix), CFStringGetLength(inDSType) - strlen(kDSStdAttrTypePrefix) ) );
    }
    else if ( CFStringHasPrefix( inDSType, kDSNativeAttrTypePrefixSAFE_CFSTR ) )
    {
        result = CFStringCreateWithSubstring( NULL, inDSType, CFRangeMake( strlen(kDSNativeAttrTypePrefix), CFStringGetLength(inDSType) - strlen(kDSNativeAttrTypePrefix) ) );
    }
    else
    {
        // just pass the raw type
        result = CFStringCreateCopy( NULL, inDSType );
    }

    return( result );

} // CreateSLPTypeFromDSType

