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
 *  @header mDNSPlugin
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "CNSLHeaders.h"

#include <CoreFoundation/CoreFoundation.h>

#include "mDNSPlugin.h"
#include "DNSBrowserThread.h"
#include "DNSRegistrationThread.h"
#include "mDNSNodeLookupThread.h"
#include "mDNSServiceLookupThread.h"

const CFStringRef	gBundleIdentifier = CFSTR("com.apple.DirectoryService.Bonjour");
const char*			gProtocolPrefixString = "Bonjour";

#pragma warning "Need to get our default Node String from our resource"

extern "C" {
CFUUIDRef ModuleFactoryUUID = CFUUIDGetConstantUUIDWithBytes ( NULL, \
								0x36, 0x33, 0xF4, 0x12, 0xD4, 0x83, 0x11, 0xD5, \
								0x88, 0xC9, 0x00, 0x03, 0x93, 0x4F, 0xB0, 0x10 );

}

static CDSServerModule* _Creator ( void )
{
	DBGLOG( "Creating new DNS Plugin\n" );
    return( new mDNSPlugin );
}

CDSServerModule::tCreator CDSServerModule::sCreator = _Creator;

mDNSPlugin::mDNSPlugin( void )
    : CNSLPlugin()
{
	DBGLOG( "mDNSPlugin::mDNSPlugin\n" );
    mLocalNodeString = NULL;
    mTopLevelContainerName = "NULL";
    mServiceTypeString = "NULL";
    mWorkgroupLookupString = "NULL";
    mListClassPath = "NULL";
    mLookupThread = NULL;
    mRegistrationThread = NULL;
    mListOfServicesToRegisterManually = NULL;
	mStartedLocalNodeLookups = false;
	mStartedNodeLookups = false;
	mRegisteredHostedServices = false;
	mComputerNameRef = NULL;
	mComputerMACAddressNameRef = NULL;
}

mDNSPlugin::~mDNSPlugin( void )
{
	DBGLOG( "mDNSPlugin::~mDNSPlugin\n" );
    
    if ( mLocalNodeString )
        free( mLocalNodeString );
    
    mLocalNodeString = NULL;
    
    if ( mRegistrationThread )
        mRegistrationThread->Cancel();
    
    mRegistrationThread = NULL;
    
    if ( mListOfServicesToRegisterManually )
        CFRelease( mListOfServicesToRegisterManually );
    mListOfServicesToRegisterManually = NULL;
	
	if ( mComputerNameRef )
		CFRelease( mComputerNameRef );
	mComputerNameRef = NULL;
	
	if ( mComputerMACAddressNameRef )
		CFRelease( mComputerMACAddressNameRef );
	mComputerMACAddressNameRef = NULL;
}

sInt32 mDNSPlugin::InitPlugin( void )
{
    sInt32				siResult	= eDSNoErr;
    // need to see if this is installed!
    
	DBGLOG( "mDNSPlugin::InitPlugin\n" );
	
//	mActivatedByNSL = true;	// we should always be active

    return siResult;
}

sInt32 mDNSPlugin::SetServerIdleRunLoopRef( CFRunLoopRef idleRunLoopRef )
{
	sInt32		siResult	= 0;

	CNSLPlugin::SetServerIdleRunLoopRef( idleRunLoopRef );		// let parent take care of business
	
	// now we can start some stuff running.

    if ( !mLookupThread )
	{
        mLookupThread = new DNSBrowserThread( this );
        
		if ( mLookupThread )
		{
			mLookupThread->Initialize( idleRunLoopRef );
		}
    }
	
    if ( !mRegistrationThread )
	{
		mRegistrationThread= new DNSRegistrationThread( this );

		if ( mRegistrationThread )
		{
			mRegistrationThread->Initialize( idleRunLoopRef );
		}
	}

	return siResult;
}

void mDNSPlugin::ActivateSelf( void )
{	
	CNSLPlugin::ActivateSelf();
	
	AddNode( kLocalSAFE_CFSTR, true );	// local node
	
	CFStringEncoding	encoding;
	mComputerNameRef = SCDynamicStoreCopyComputerName(NULL, &encoding);
	mComputerMACAddressNameRef = CreateComputerNameEthernetString(mComputerNameRef);

	// we are going to simulate a network transition event here that will cause internal services to be registered
	sHeader		header = {kHandleNetworkTransition, 0, NULL};
	
	DBGLOG( "mDNSPlugin::ActivateSelf, calling HandleRequest with a kHandleNetworkTransition event\n" );
	HandleRequest( &header );
}

CFStringRef mDNSPlugin::GetBundleIdentifier( void )
{
    return gBundleIdentifier;
}

// this is used for top of the node's path "NSL"
const char*	mDNSPlugin::GetProtocolPrefixString( void )
{		
    return gProtocolPrefixString;
}

// this maps to the group we belong to  (i.e. local)
const char*	mDNSPlugin::GetLocalNodeString( void )
{		
    if ( !mLocalNodeString )
        DBGLOG( "mDNSPlugin::GetLocalNodeString, mLocalNodeString not found yet\n" );

    return mLocalNodeString;
}


Boolean mDNSPlugin::IsLocalNode( const char *inNode )
{
    Boolean 	result = false;

    DBGLOG( "mDNSPlugin::IsLocalNode called checking %s\n", inNode );

    if ( mLocalNodeString )
    {
        result = ( strcmp( inNode, mLocalNodeString ) == 0 );

        DBGLOG( "mDNSPlugin::IsLocalNode result:%d (strcmp(%s,%s))\n", result, inNode, mLocalNodeString );
    
    }
	else
		result = ( strcmp( inNode, "local" ) == 0 );

    return result;
}

void mDNSPlugin::NewNodeLookup( void )
{
	if ( (!mStartedNodeLookups || !mStartedLocalNodeLookups) && mLookupThread )	// only need to do this once, CFNetServices calls us if node info changes
	{
		DBGLOG( "mDNSPlugin::NewNodeLookup - start a new node lookup\n" );
		
		// we are going to simulate a network transition event here that will cause internal lookups to fire if needed
		sHeader		header = {kHandleNetworkTransition, 0, NULL};
		
		DBGLOG( "mDNSPlugin::NewNodeLookup, calling HandleRequest with a kHandleNetworkTransition event\n" );
		HandleRequest( &header );
	}
	else
		DBGLOG( "mDNSPlugin::NewNodeLookup - ignore new node lookup, Bonjour will notifiy us of any changes\n" );
}

Boolean mDNSPlugin::OKToOpenUnPublishedNode	( const char* parentNodeName )
{
    return true;
}

void mDNSPlugin::NewSubNodeLookup( char* parentNodeName )
{
	DBGLOG( "mDNSPlugin::NewSubNodeLookup in %s\n", parentNodeName );
}

void mDNSPlugin::NewServiceLookup( char* serviceType, CNSLDirNodeRep* nodeDirRep )
{
	DBGLOG( "mDNSPlugin::NewServicesLookup\n" );

    mDNSServiceLookupThread* newLookup = new mDNSServiceLookupThread( this, serviceType, nodeDirRep );
        
    // if we have too many threads running, just queue this search object and run it later
    if ( OKToStartNewSearch() )
        newLookup->Resume();
    else
        QueueNewSearch( newLookup );
}

sInt32 mDNSPlugin::HandleNetworkTransition( sHeader *inData )
{
    sInt32					siResult			= eDSNoErr;
    
	DBGLOG( "mDNSPlugin::HandleNetworkTransition called\n" );
	if ( IsActive() )
    {
		if ( mActivatedByNSL && !mStartedLocalNodeLookups && mLookupThread )	// only need to do this once, CFNetServices calls us if node info changes
		{
            DBGLOG( "mDNSPlugin::HandleNetworkTransition calling StartNodeLookups for default nodes\n" );
			mStartedLocalNodeLookups = true;
			if ( mLookupThread->StartNodeLookups( true ) != eDSNoErr )			// start looking up default nodes
			{
				DBGLOG( "mDNSPlugin::HandleNetworkTransition StartNodeLookups failed, reset to try again later\n" );
				mStartedLocalNodeLookups = false;
				ResetNodeLookupTimer( 5 );			// try again in five seconds
			}
		}
		
		if ( mActivatedByNSL && !mStartedNodeLookups && mLookupThread )	// only need to do this once (but only if we've been activated by NSL), CFNetServices calls us if node info changes
		{
			DBGLOG( "mDNSPlugin::HandleNetworkTransition calling StartNodeLookups for non-default nodes\n" );

			mStartedNodeLookups = true;
			if ( mLookupThread->StartNodeLookups( false ) != eDSNoErr )	// start looking up non-default nodes
			{
				DBGLOG( "mDNSPlugin::HandleNetworkTransition StartNodeLookups failed, reset to try again later\n" );
				mStartedNodeLookups = false;
				ResetNodeLookupTimer( 5 );			// try again in five seconds
			}
		}

		if ( !mRegisteredHostedServices && mRegistrationThread )
		{
            DBGLOG( "mDNSPlugin::HandleNetworkTransition calling RegisterHostedServices\n" );
			mRegisteredHostedServices = true;
			if ( mRegistrationThread->RegisterHostedServices() != eDSNoErr )
			{
				DBGLOG( "mDNSPlugin::HandleNetworkTransition RegisterHostedServices failed, reset to try again later\n" );
				mRegisteredHostedServices = false;		// we'll try again later
				ResetNodeLookupTimer( 5 );			// try again in five seconds
			}
		}
    }
	else
		DBGLOG( "mDNSPlugin::HandleNetworkTransition IsActive is false, ignoring notification\n" );
    
    return ( siResult );
}

Boolean mDNSPlugin::IsClientAuthorizedToCreateRecords ( sCreateRecord *inData )
{
    const void*				dictionaryResult	= NULL;
    Boolean					okToCreate = false;
    
    if( ::CFDictionaryGetValueIfPresent( mOpenRefTable, (const void*)inData->fInNodeRef, &dictionaryResult ) )
    {
        if ( ::CFStringCompare( ((CNSLDirNodeRep*)dictionaryResult)->GetNodeName(), kEmptySAFE_CFSTR, 0 ) == kCFCompareEqualTo || ::CFStringCompare( ((CNSLDirNodeRep*)dictionaryResult)->GetNodeName(), kLocalSAFE_CFSTR, 0 ) == kCFCompareEqualTo || ::CFStringCompare( ((CNSLDirNodeRep*)dictionaryResult)->GetNodeName(), kLocalDotSAFE_CFSTR, 0 ) == kCFCompareEqualTo )
        {
            okToCreate = true;	// always ok to register local data
            DBGLOG( "mDNSPlugin::IsClientAuthorizedToCreateRecords returning true because client is registering in local or default.\n" );
        }
        else if ( ((CNSLDirNodeRep*)dictionaryResult)->GetUID() == 0 )
        {
            okToCreate = true;	// always ok for root processes to register
            DBGLOG( "mDNSPlugin::IsClientAuthorizedToCreateRecords returning true because client is root\n" );
        }
    }
    else
        DBGLOG( "mDNSPlugin::IsClientAuthorizedToCreateRecords called but we couldn't find the nodeDirRep!\n" );
    
    return okToCreate;
}

// ---------------------------------------------------------------------------
//	* CreateNSLTypeFromRecType
// ---------------------------------------------------------------------------

char* mDNSPlugin::CreateNSLTypeFromRecType ( char *inRecType, Boolean* needToFreeRecType )
{
    char				   *outResult	= nil;
    uInt32					uiStrLen	= 0;
    uInt32					uiStdLen	= ::strlen( kDSStdRecordTypePrefix );
	
	DBGLOG( "mDNSPlugin::CreateNSLTypeFromRecType called on %s\n", inRecType );
	*needToFreeRecType = false;
	
    if ( ( inRecType != nil ) )
    {
        uiStrLen = ::strlen( inRecType );

        if ( ::strncmp( inRecType, kDSStdRecordTypePrefix, uiStdLen ) == 0 )
		{
            DBGLOG( "mDNSPlugin::CreateNSLTypeFromRecType kDSStdRecordTypePrefix, uiStrLen:%ld uiStdLen:%ld\n", uiStrLen, uiStdLen );
            if ( strcmp( inRecType, kDSStdRecordTypeAFPServer ) == 0 )
            {
                outResult = kAFPoverTCPServiceType;

				DBGLOG( "mDNSPlugin::CreateNSLTypeFromRecType mapping %s to %s\n", inRecType, outResult );
            }
		}
	}// ( inRecType != nil )
    
	if ( !outResult )
		outResult = CNSLPlugin::CreateNSLTypeFromRecType( inRecType, needToFreeRecType );		// let the parent handle it
		
	return( outResult );

} // CreateNSLTypeFromRecType

// ---------------------------------------------------------------------------
//	* CreateNSLTypeFromRecType
// ---------------------------------------------------------------------------

CFStringRef mDNSPlugin::CreateRecTypeFromNativeType ( char *inNativeType )
{
    CFMutableStringRef	   	outResultRef	= NULL;
	
	DBGLOG( "mDNSPlugin::CreateRecTypeFromNativeType called on %s\n", inNativeType );

    if ( ( inNativeType != nil ) )
    {
		if ( ::strcmp( inNativeType, kAFPoverTCPServiceType ) == 0 )
		{
			outResultRef = CFStringCreateMutable( NULL, 0 );
        
			CFStringAppend( outResultRef, kDSStdRecordTypeAFPServerSAFE_CFSTR );
		}
	}// ( inRecType != nil )
    
	if ( !outResultRef )
		outResultRef = (CFMutableStringRef)CNSLPlugin::CreateRecTypeFromNativeType( inNativeType );		// let the parent handle it
		
	return( outResultRef );

} // CreateNSLTypeFromRecType

#pragma mark -
sInt32 mDNSPlugin::RegisterService( tRecordReference recordRef, CFDictionaryRef service )
{
    sInt32				status = eDSNoErr;
    CFStringRef			nameOfService = NULL;			/* Service's name (must be unique per domain (should be UTF8) */
    CFStringRef			typeOfService = NULL;			/* Service Type (i.e. afp, lpr etc) */
    CFStringRef			locationOfService = NULL;		/* Service's Location (domain) */
    CFStringRef			protocolSpecificData = NULL;	/* extra data (like printer info) */
    CFStringRef			portOfService = NULL;			/* Service's Port */
    
    if ( service )
    {
        nameOfService = (CFStringRef)::CFDictionaryGetValue( service, kDSNAttrRecordNameSAFE_CFSTR );
        if ( !nameOfService )
            nameOfService = kEmptySAFE_CFSTR;		// just register Computer Name
            
        locationOfService = (CFStringRef)::CFDictionaryGetValue( service, kDS1AttrLocationSAFE_CFSTR );
        if ( !locationOfService )
		{
			DBGLOG( "mDNSPlugin::RegisterService, no location specified, use \"\"\n" );
            locationOfService = kEmptySAFE_CFSTR;		// just register local
		}
		else
		if ( CFStringCompare( locationOfService, CFSTR("local"), 0 ) == kCFCompareEqualTo )
		{
			DBGLOG( "mDNSPlugin::RegisterService, registering in local, use \"\"\n" );
            locationOfService = kEmptySAFE_CFSTR;		// just register local
		}
		else
		if ( DEBUGGING_NSL )
		{
			char*		location = (char*)malloc( CFStringGetMaximumSizeForEncoding( CFStringGetLength(locationOfService), kCFStringEncodingUTF8 ) + 1 );
			
			if ( location )
			{
				CFStringGetCString( locationOfService, location, CFStringGetMaximumSizeForEncoding( CFStringGetLength(locationOfService), kCFStringEncodingUTF8 ) + 1, kCFStringEncodingUTF8 );

				DBGLOG("mDNSPlugin::RegisterService using specified location [%s]\n", location );
			}
		
		}
		
        typeOfService = (CFStringRef)::CFDictionaryGetValue( service, kDS1AttrServiceTypeSAFE_CFSTR );
        if ( !typeOfService )
            typeOfService = (CFStringRef)::CFDictionaryGetValue( service, kDSNAttrRecordTypeSAFE_CFSTR );
        protocolSpecificData = (CFStringRef)::CFDictionaryGetValue( service, kDNSTextRecordSAFE_CFSTR );
        portOfService = (CFStringRef)::CFDictionaryGetValue( service, kDS1AttrPtrSAFE_CFSTR );
        
        if ( nameOfService && locationOfService && typeOfService /*&& protocolSpecificData*/ && portOfService )
        {
            if ( !mRegistrationThread )
            {
			// If this hasn't been created yet this is bad...
				char		name[1024];
				char		type[256];
				
				CFStringGetCString( nameOfService, name, sizeof(name), kCFStringEncodingUTF8 );
				CFStringGetCString( typeOfService, type, sizeof(type), kCFStringEncodingUTF8 );
				syslog( LOG_ERR, "DS Bonjour couldn't register %s (%s) since our Registration Thread hasn't been created!\n", name, type );
				status = ePlugInError;
            }
            
            if ( mRegistrationThread )
            {
                 CFStringRef		serviceKey = NULL;
				 status = mRegistrationThread->PerformRegistration( nameOfService, typeOfService, locationOfService, protocolSpecificData, portOfService, &serviceKey );
				 
				 if ( serviceKey )
					CFRelease( serviceKey );
            }
        }
        else
        {
            DBGLOG( "mDNSPlugin::RegisterService, we didn't receive all the valid registration parameters\n" );
            status = eDSNullAttribute;
        }
    }
    else
        status = eDSNullAttribute;
        
    return status;
}

sInt32 mDNSPlugin::DeregisterService( tRecordReference recordRef, CFDictionaryRef service )
{
    sInt32				status = eDSNoErr;

    if ( mRegistrationThread )
    {
		DBGLOG( "mDNSPlugin::DeregisterService, calling PerformDeregistraton with the mRegistrationThread\n" );
        status = mRegistrationThread->PerformDeregistration( service );
    }
	else
	{
	// If this hasn't been created yet this is bad...
		char				name[1024] = {0,};
		char				type[256] = {0,};
		
		CFStringRef	nameOfService = (CFStringRef)::CFDictionaryGetValue( service, kDSNAttrRecordNameSAFE_CFSTR );
		if ( nameOfService )
			CFStringGetCString( nameOfService, name, sizeof(name), kCFStringEncodingUTF8 );
			
		CFStringRef	typeOfService = (CFStringRef)::CFDictionaryGetValue( service, kDS1AttrServiceTypeSAFE_CFSTR );
		if ( !typeOfService )
			typeOfService = (CFStringRef)::CFDictionaryGetValue( service, kDSNAttrRecordTypeSAFE_CFSTR );
	
		if ( typeOfService )
			CFStringGetCString( typeOfService, type, sizeof(type), kCFStringEncodingUTF8 );
	
		syslog( LOG_ERR, "DS Bonjour couldn't deregister %s (%s) since our Registration Thread hasn't been created!\n", name, type );
		status = ePlugInError;
	}
        
    return status;
}
