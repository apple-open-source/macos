/*
 *  mDNSPlugin.cpp
 *  mDNSPlugin
 *
 *  Created by imlucid on Wed Aug 15 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "CNSLHeaders.h"

#include <CoreServices/CoreServices.h>

#include "mDNSPlugin.h"
#include "DNSBrowserThread.h"
#include "DNSRegistrationThread.h"
#include "mDNSNodeLookupThread.h"
#include "mDNSServiceLookupThread.h"
#include "TGetCFBundleResources.h"

#define kLocalizedStringsID				128
#define kNetworkNeighborhoodStrID		1

#define kCommandParamsID				129
#define kServiceTypeStrID				1
#define kListClassPathStrID				2
#define kLookupWorkgroupsJCIFSCommand	3
#define kDefaultGroupName			"WORKGROUP"

const CFStringRef	gBundleIdentifier = CFSTR("com.apple.DirectoryService.Rendezvous");
const char*			gProtocolPrefixString = "Rendezvous";

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
}

sInt32 mDNSPlugin::InitPlugin( void )
{
    sInt32				siResult	= eDSNoErr;
    // need to see if this is installed!
    
	DBGLOG( "mDNSPlugin::InitPlugin\n" );

    return siResult;
}

sInt32 mDNSPlugin::SetServerIdleRunLoopRef( CFRunLoopRef idleRunLoopRef )
{
	CNSLPlugin::SetServerIdleRunLoopRef( idleRunLoopRef );		// let parent take care of business
	
	// now we can start some stuff running.

    if ( !mLookupThread )
	{
        mLookupThread = new DNSBrowserThread( this );
        
		if ( mLookupThread )
		{
			mLookupThread->Initialize( idleRunLoopRef );
//			mLookupThread->Resume();
	
			mLookupThread->StartNodeLookups( true );	// start looking default nodes
	
			mLookupThread->StartNodeLookups( false );	// start looking all nodes
		}
    }
	
    if ( !mRegistrationThread )
	{
		mRegistrationThread= new DNSRegistrationThread( this );

		if ( mRegistrationThread )
		{
			mRegistrationThread->Initialize( idleRunLoopRef );
//			mRegistrationThread->Resume();
			
			mRegistrationThread->RegisterHostedServices();
		}
	}
	
	return eDSNoErr;
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

// this maps to the group we belong to  (i.e. WORKGROUP)
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
    
    return result;
}

void mDNSPlugin::NewNodeLookup( void )
{
	DBGLOG( "mDNSPlugin::NewNodeLookup - start a new node lookup\n" );
    
    if ( mLookupThread )
	{
		mLookupThread->StartNodeLookups( true );	// start looking up default nodes
		mLookupThread->StartNodeLookups( false );	// start looking up non-default nodes
	}
}

Boolean mDNSPlugin::OKToOpenUnPublishedNode	( const char* parentNodeName )
{
    return true;
}

void mDNSPlugin::NewSubNodeLookup( char* parentNodeName )
{
	DBGLOG( "mDNSPlugin::NewSubNodeLookup in %s\n", parentNodeName );
    
/*    mDNSNodeLookupThread* newLookup = new mDNSNodeLookupThread( this, parentNodeName );
    
    newLookup->Resume();
*/
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

Boolean mDNSPlugin::IsClientAuthorizedToCreateRecords ( sCreateRecord *inData )
{
    const void*				dictionaryResult	= NULL;
    Boolean					okToCreate = false;
    
    if( ::CFDictionaryGetValueIfPresent( mOpenRefTable, (const void*)inData->fInNodeRef, &dictionaryResult ) )
    {
        if ( ::CFStringCompare( ((CNSLDirNodeRep*)dictionaryResult)->GetNodeName(), CFSTR("local"), 0 ) || ::CFStringCompare( ((CNSLDirNodeRep*)dictionaryResult)->GetNodeName(), CFSTR("local."), 0 ) )
        {
            okToCreate = true;	// always ok to register local data
            DBGLOG( "mDNSPlugin::IsClientAuthorizedToCreateRecords returning true because client is registering in local.\n" );
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

char* mDNSPlugin::CreateNSLTypeFromRecType ( char *inRecType )
{
    char				   *outResult	= nil;
    uInt32					uiStrLen	= 0;
    uInt32					uiStdLen	= ::strlen( kDSStdRecordTypePrefix );
	
	DBGLOG( "mDNSPlugin::CreateNSLTypeFromRecType called on %s\n", inRecType );

    if ( ( inRecType != nil ) )
    {
        uiStrLen = ::strlen( inRecType );

        if ( ::strncmp( inRecType, kDSStdRecordTypePrefix, uiStdLen ) == 0 )
		{
            DBGLOG( "mDNSPlugin::CreateNSLTypeFromRecType kDSStdRecordTypePrefix, uiStrLen:%ld uiStdLen:%ld\n", uiStrLen, uiStdLen );
            if ( strcmp( inRecType, kDSStdRecordTypeAFPServer ) == 0 )
            {
//                outResult = new char[1+::strlen(kAFPServiceType)];
//                ::strcpy(outResult, kAFPServiceType);
                outResult = new char[1+::strlen(kAFPoverTCPServiceType)];
                ::strcpy(outResult, kAFPoverTCPServiceType);

				DBGLOG( "mDNSPlugin::CreateNSLTypeFromRecType mapping %s to %s\n", inRecType, outResult );
            }
		}
	}// ( inRecType != nil )
    
	if ( !outResult )
		outResult = CNSLPlugin::CreateNSLTypeFromRecType( inRecType );		// let the parent handle it
		
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
        
			CFStringAppend( outResultRef, CFSTR(kDSStdRecordTypeAFPServer) );
		}
	}// ( inRecType != nil )
    
	if ( !outResultRef )
		outResultRef = (CFMutableStringRef)CNSLPlugin::CreateRecTypeFromNativeType( inNativeType );		// let the parent handle it
		
	return( outResultRef );

} // CreateNSLTypeFromRecType

sInt32 mDNSPlugin::RegisterService( tRecordReference recordRef, CFDictionaryRef service )
{
    sInt32				status = eDSNoErr;
    CFStringRef			nameOfService = NULL;			/* Service's name (must be unique per domain (should be UTF8) */
    CFStringRef			typeOfService = NULL;			/* Service Type (i.e. afp, lpr etc) */
    CFStringRef			locationOfService = NULL;		/* Service's Location (domain) */
    CFStringRef			protocolSpecificData = NULL;	/* extra data (like printer info) */
    CFStringRef			portOfService = NULL;			/* Service's Port */
//    CFStringRef			interfaceName = NULL;
    
    if ( service )
    {
        nameOfService = (CFStringRef)::CFDictionaryGetValue( service, CFSTR(kDSNAttrRecordName) );
        locationOfService = (CFStringRef)::CFDictionaryGetValue( service, CFSTR(kDS1AttrLocation) );
        if ( !locationOfService )
            locationOfService = CFSTR("");		// just register local
//            locationOfService = CFSTR("local.");		// just register local
            
        typeOfService = (CFStringRef)::CFDictionaryGetValue( service, CFSTR(kDS1AttrServiceType) );
        if ( !typeOfService )
            typeOfService = (CFStringRef)::CFDictionaryGetValue( service, CFSTR(kDSNAttrRecordType) );
        protocolSpecificData = (CFStringRef)::CFDictionaryGetValue( service, CFSTR("dsAttrTypeStandard:DNSTextRecord") );
        portOfService = (CFStringRef)::CFDictionaryGetValue( service, CFSTR(kDS1AttrPort) );
        
        if ( nameOfService && locationOfService && typeOfService /*&& protocolSpecificData*/ && portOfService )
        {
            if ( !mRegistrationThread )
            {
			// Ugh.  If this hasn't been created yet we are kinda screwed...
				DBGLOG( "mDNSPlugin::RegisterService, we don't have a registration object already set up!\n" );
/*            	mRegistrationThread = new DNSRegistrationThread( this );
            
                if ( mRegistrationThread )
                    mRegistrationThread->Resume();
*/
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
        status = mRegistrationThread->PerformDeregistration( service );
    }
        
    return status;
}

