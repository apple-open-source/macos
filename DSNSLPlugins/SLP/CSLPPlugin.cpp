/*
 *  CSLPPlugin.cpp
 *  DSSLPPlugIn
 *
 *  Created by imlucid on Wed Aug 15 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
 */

#include <SystemConfiguration/SystemConfiguration.h>

#include "CNSLHeaders.h"

#include "CommandLineUtilities.h"

#include "CSLPPlugin.h"
#include "CSLPNodeLookupThread.h"
#include "CSLPServiceLookupThread.h"
#include "TGetCFBundleResources.h"


void AddToAttributeList( const void* key, const void* value, void* context );
CFStringRef CreateHexEncodedString( CFStringRef rawStringRef );
CFStringRef CreateEncodedString( CFStringRef rawStringRef );
CFStringRef	CreateSLPTypeFromDSType ( CFStringRef inDSType );


#define kCommandParamsID				128

const CFStringRef	gBundleIdentifier = CFSTR("com.apple.DirectoryService.SLP");
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

    SCPreferencesRef prefRef = ::SCPreferencesCreate( NULL, CFSTR("com.apple.DirectoryService.DSSLPPlugIn"), CFSTR("com.apple.slp") );
    
//	system( "rm /var/slp.regfile" );
	executecommand( "rm /var/slp.regfile" );
	
	DBGLOG( "CSLPPlugin::InitPlugin is deleting regfile:/var/slp.regfile" );
	
	CFPropertyListRef	propertyList = ::SCPreferencesGetValue( prefRef, CFSTR("com.apple.slp.defaultRegistrationScope") );
        
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
            ::SCPreferencesSetValue( prefRef, CFSTR("com.apple.slp.defaultRegistrationScope"), localNodeRef );
            ::SCPreferencesUnlock( prefRef );
        }
        
        ::CFRelease( localNodeRef );
//        gLocalNodeString = (char*)malloc( strlen( v2_Default_Scope ) +1 );
//        strcpy( gLocalNodeString, v2_Default_Scope );
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
    
    return siResult;
}

sInt32 CSLPPlugin::SetServerIdleRunLoopRef( CFRunLoopRef idleRunLoopRef )
{
	CNSLPlugin::SetServerIdleRunLoopRef( idleRunLoopRef );
	
	SLPInternalError status = SLPOpen( "en", SLP_FALSE, &mSLPRef, GetRunLoopRef() );
	
	return eDSNoErr;
}

char* CSLPPlugin::CreateLocalNodeFromConfigFile( void )
{
    char*	localNodeString = (char*)malloc( strlen( v2_Default_Scope ) +1 );
    strcpy( localNodeString, v2_Default_Scope );

    SLPReadConfigFile( "/etc/slpsa.conf" );
    
    if ( SLPGetProperty("com.apple.slp.defaultRegistrationScope") && strcmp(localNodeString,SLPGetProperty("com.apple.slp.defaultRegistrationScope")) != 0 )
    {
        free( localNodeString );
        localNodeString = (char*)malloc( strlen( SLPGetProperty("com.apple.slp.defaultRegistrationScope") ) +1 );
        strcpy( localNodeString, SLPGetProperty("com.apple.slp.defaultRegistrationScope") );
    }
    
    return localNodeString;
}

CFStringRef CSLPPlugin::GetBundleIdentifier( void )
{
    return gBundleIdentifier;
}

// this is used for top of the node's path "NSL"
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
    
    if ( mActivatedByNSL )
    {
		KickSLPDALocator();			// wake this guy
		ClearOutAllNodes();			// clear these out
		StartNodeLookup();			// and then start all over
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
	
	// First add our local scope
    AddNode( GetLocalNodeString() );		// always register the default registration node
	
	if ( !mNodeLookupThread )
	{
		mNodeLookupThread = new CSLPNodeLookupThread( this );
		
		mNodeLookupThread->Resume();
	}
	else
	{
		mNodeLookupThread->DoItAgain();		// we need to start a new one as soon as the old one finishes
	}
}

void CSLPPlugin::NewServiceLookup( char* serviceType, CNSLDirNodeRep* nodeDirRep )
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


Boolean CSLPPlugin::OKToOpenUnPublishedNode( const char* nodeName )
{
    return true;	// allow users to create their own nodes?
}

sInt32 CSLPPlugin::RegisterService( tRecordReference recordRef, CFDictionaryRef service )
{
    sInt32		status = eDSNoErr;
    CFStringRef	urlRef = NULL;
    CFStringRef	scopeRef = NULL;
	CFTypeRef	urlResultRef = NULL;
	
    if ( service )
		urlResultRef = (CFTypeRef)::CFDictionaryGetValue( service, CFSTR(kDSNAttrURL));

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
        UInt32		scopePtrLength;
        char*		scopePtr = NULL;
        
        DBGLOG( "CSLPPlugin::RegisterService, check for specified location to register in\n" );
        if ( (scopeRef = (CFStringRef)::CFDictionaryGetValue( service, CFSTR(kDS1AttrLocation) )) )
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
            
            ::CFDictionaryAddValue( attributesDictRef, CFSTR("attributeList"), attributeRef );
            ::CFDictionaryAddValue( attributesDictRef, CFSTR("attributeForURL"), attributeForURLRef );
            
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
            
        free( scopePtr );
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
		urlResultRef = (CFTypeRef)::CFDictionaryGetValue( service, CFSTR(kDSNAttrURL));

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
        
        if ( (scopeRef = (CFStringRef)::CFDictionaryGetValue( service, CFSTR(kDS1AttrLocation))) )
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
            
            ::CFDictionaryAddValue( attributesDictRef, CFSTR("attributeList"), attributeRef );
            ::CFDictionaryAddValue( attributesDictRef, CFSTR("attributeForURL"), attributeForURLRef );
            
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

OSStatus CSLPPlugin::DoSLPRegistration( char* scopeList, char* url, char* attributeList )
{
	char*		dataBuffer = NULL;
	char*		returnBuffer = NULL;
	UInt32		dataBufferLen = 0;
	UInt32		returnBufferLen = 0;
	OSStatus	status = noErr;
	
    DBGLOG( "CSLPPlugin::DoSLPRegistration called, scope: %s, url: %s, attributeList: %s\n", scopeList, url, attributeList );
    
	dataBuffer = MakeSLPRegistrationDataBuffer ( scopeList, strlen(scopeList), url, strlen(url), attributeList, strlen(attributeList), &dataBufferLen );

	if ( dataBuffer )
	{
		status = SendDataToSLPd( dataBuffer, dataBufferLen, &returnBuffer, &returnBufferLen );

		if ( status )
        {
            if ( returnBuffer )
                free( returnBuffer );
            returnBuffer = NULL;
            
            sleep(1);		// try again
            status = SendDataToSLPd( dataBuffer, dataBufferLen, &returnBuffer, &returnBufferLen );
            
        }
	}
	else
		status = memFullErr;
			
	// now check for any message status
	if ( !status && returnBuffer && returnBufferLen > 0 )
		status = ((SLPdMessageHeader*)returnBuffer)->messageStatus;
		
    if ( dataBuffer )
        free( dataBuffer );
        
    if ( returnBuffer )
        free( returnBuffer );
        
	return status;
}

OSStatus CSLPPlugin::DoSLPDeregistration( char* scopeList, char* url )
{
	char*		dataBuffer = NULL;
	char*		returnBuffer = NULL;
	UInt32		dataBufferLen = 0;
	UInt32		returnBufferLen = 0;
	OSStatus	status = noErr;
	
    DBGLOG( "CSLPPlugin::DoSLPDeregistration called, scope: %s, url: %s\n", scopeList, url );
    
	dataBuffer = MakeSLPDeregistrationDataBuffer( scopeList, strlen(scopeList), url, strlen(url), &dataBufferLen );
	
	if ( dataBuffer )
		status = SendDataToSLPd( dataBuffer, dataBufferLen, &returnBuffer, &returnBufferLen );
	else
		status = memFullErr;
		
	// now check for any message status
	if ( !status && returnBuffer && returnBufferLen > 0 )
		status = ((SLPdMessageHeader*)returnBuffer)->messageStatus;
		
    if ( dataBuffer )
        free( dataBuffer );
        
    if ( returnBuffer )
        free( returnBuffer );
		
	return status;
}


void AddToAttributeList( const void* key, const void* value, void* context )
{
    // for each key value we need to wrap it like one of these:
    // (key),
    // (key=value),
    // (key=value1,value2),
DBGLOG( "AddToAttributeList called with key:%s value:%s", (char*)key, (char*)value );
    // when we are done, we will end up with a string with an extra comma at the end that we will delete later
    if ( key /*&& value*/ && context )
    {
        CFDictionaryRef				attrDict = (CFDictionaryRef)context;
        CFStringRef					keyRef = (CFStringRef) key;
        CFMutableStringRef			attributeListRef = (CFMutableStringRef)CFDictionaryGetValue( attrDict, CFSTR("attributeList") );
        CFMutableStringRef			attributeListForURLRef = (CFMutableStringRef)CFDictionaryGetValue( attrDict, CFSTR("attributeForURL") );
        CFPropertyListRef			valueRef = (CFPropertyListRef) value;
        
        // first check to see if this key is one to ignore
        if ( ::CFStringCompare( CFSTR(kDSNAttrURL), keyRef, 0 ) == kCFCompareEqualTo )
            return;
        else if ( ::CFStringCompare( CFSTR(kDS1AttrLocation), keyRef, 0 ) == kCFCompareEqualTo )
            return;

        if ( attributeListRef )
        {
            ::CFStringAppendCString( attributeListRef, "(", kCFStringEncodingUTF8 );
            ::CFStringAppendCString( attributeListForURLRef, ";", kCFStringEncodingASCII );		// url needs to be encoded ASCII

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
                    for ( CFIndex i=0; i<CFArrayGetCount((CFArrayRef)valueRef); i++ )
                    {
                        CFStringRef valueStringRef = (CFStringRef)::CFArrayGetValueAtIndex( (CFArrayRef)valueRef, i );
                            
                        if ( valueStringRef )
                        {
                            CFStringRef		encodedValueRef = CreateEncodedString(valueStringRef);
                            CFStringRef		hexEncodedValueRef = CreateHexEncodedString(valueStringRef);
                            ::CFStringAppend( attributeListRef, encodedValueRef );
                            ::CFStringAppend( attributeListForURLRef, hexEncodedValueRef );
                            
                            if ( i+1 < CFArrayGetCount((CFArrayRef)valueRef) )
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
        
        //CFStringAppendCString( newStringRef, "\\FF", kCFStringEncodingASCII );
        
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
    
/*    if ( ::CFStringGetCString( rawStringRef, buffer, bufferSize, kCFStringEncodingASCII ) )
    {
        // no encoding needed, just pass back a copy
        return CFStringCreateCopy( NULL, rawStringRef );
    }
    else 
*/    if ( ::CFStringGetCString( rawStringRef, buffer, bufferSize, kCFStringEncodingUTF8 ) )
    {
        CFMutableStringRef		newStringRef = ::CFStringCreateMutable( NULL, 0 );
        char*					curPtr = buffer;
        char					temp[4] = {0};                
        Boolean					encode = false;
        
        DBGLOG( "CreateHexEncodedString parsing: %s\n", buffer );
        
        //CFStringAppendCString( newStringRef, "\\FF", kCFStringEncodingASCII );
        
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
    
    if ( CFStringCompare( inDSType, CFSTR(kDSNAttrRecordName), 0 ) == kCFCompareEqualTo )
    {
        result = CFStringCreateWithCString( NULL, kSLPNameKey, kCFStringEncodingUTF8 );
    }    
    else if ( CFStringHasPrefix( inDSType, CFSTR(kDSStdAttrTypePrefix) ) )
    {
        result = CFStringCreateWithSubstring( NULL, inDSType, CFRangeMake( strlen(kDSStdAttrTypePrefix), CFStringGetLength(inDSType) - strlen(kDSStdAttrTypePrefix) ) );
    }
    else if ( CFStringHasPrefix( inDSType, CFSTR(kDSNativeAttrTypePrefix) ) )
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

