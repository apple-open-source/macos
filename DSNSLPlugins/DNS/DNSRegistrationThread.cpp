/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
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
/*
 *  DNSRegistrationThread.cpp
 *  DSNSLPlugins
 *
 *  Created by Kevin Arnold on Tue Mar 19 2002.
 *  Copyright (c) 2002 Apple Computer. All rights reserved.
 *
 */

#include "DNSRegistrationThread.h"
#include "mDNSPlugin.h"

#include "LinkAddresses.h"
#include "CNSLTimingUtils.h"

#define kOurSpecialRegRef -1

typedef struct DNSRegData {
	CFNetServiceRef		fCFNetServiceRef;
	UInt32				fCount;

} DNSRegData;

const CFStringRef	kDNSSCDynamicStoreKeySAFE_CFSTR = CFSTR("com.apple.DirectoryServices.DNS");
const CFStringRef	kWorkstationTypeSAFE_CFSTR = CFSTR("_workstation._tcp.");
const CFStringRef	kWorkstationPortSAFE_CFSTR = CFSTR("9");

const CFStringRef	kSpaceLeftBracketSAFE_CFSTR = CFSTR(" [");
const CFStringRef	kRightBracketSAFE_CFSTR = CFSTR("]");
const CFStringRef	kZeroedMACAddressSAFE_CFSTR = CFSTR("0:0:0:0:0:0");

static void RegisterEntityCallBack(CFNetServiceRef theEntity, CFStreamError* error, void* info);
CFStringRef CopyCancelRegDescription( const void* info );
CFStringRef CopyRegistrationDescription( const void* info );
boolean_t SystemConfigurationNameChangedCallBack(SCDynamicStoreRef session, void *callback_argument);

DNSRegistrationThread::DNSRegistrationThread(	mDNSPlugin* parentPlugin )
//    : DSLThread()
{
    mParentPlugin = parentPlugin;
    mRunLoopRef = 0;
    mSCRef = NULL;
    mRegisteredServicesTable = NULL;
    mMachineService = NULL;
    mCanceled = false;
	mOurSpecialRegKey = NULL;
}

DNSRegistrationThread::~DNSRegistrationThread()
{
    mParentPlugin = NULL;
    mRunLoopRef = 0;

    if ( mRegisteredServicesTable )
    {
        ::CFDictionaryRemoveAllValues( mRegisteredServicesTable );
        ::CFRelease( mRegisteredServicesTable );
        mRegisteredServicesTable = NULL;
    }
    
    if ( mSCRef )
        CFRelease( mSCRef );
    mSCRef = NULL;
	
	if ( mOurSpecialRegKey )
		CFRelease( mOurSpecialRegKey );
	mOurSpecialRegKey = NULL;
}

void DNSRegistrationThread::Cancel( void )
{
/*    if ( mRunLoopRef )
        CFRunLoopStop( mRunLoopRef );
*/
    mCanceled = true;
}

void DNSRegistrationThread::Initialize( CFRunLoopRef idleRunLoopRef )
{
    // use these for the reftable dictionary
	CFDictionaryKeyCallBacks	keyCallBack;
    CFDictionaryValueCallBacks	valueCallBack;

    keyCallBack.version = 0;
    keyCallBack.retain = NULL;
    keyCallBack.release = NULL;
    keyCallBack.copyDescription = NULL;
    keyCallBack.equal = NULL;
    keyCallBack.hash = NULL;		// this is fine
    
    valueCallBack.version = 0;
    valueCallBack.retain = NULL;
    valueCallBack.release = NULL;
    valueCallBack.copyDescription = NULL;
    valueCallBack.equal = NULL;
    
    mRegisteredServicesTable = ::CFDictionaryCreateMutable( NULL, 0, &kCFCopyStringDictionaryKeyCallBacks, &valueCallBack );

	mRunLoopRef = idleRunLoopRef;

    if ( !mSCRef )
        mSCRef = ::SCDynamicStoreCreate(NULL, kDNSSCDynamicStoreKeySAFE_CFSTR, NULL, NULL);

	SInt32				scdStatus			= 0;
	CFStringRef			key					= 0;
	Boolean				setStatus			= FALSE;
    CFMutableArrayRef	notifyKeys			= CFArrayCreateMutable(	kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    CFMutableArrayRef	notifyPatterns		= CFArrayCreateMutable(	kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks);
    
    // name changes
    DBGLOG( "RegisterForNetworkChange for SCDynamicStoreKeyCreateComputerName:\n" );
    key = SCDynamicStoreKeyCreateComputerName(NULL);
    CFArrayAppendValue(notifyKeys, key);
    CFRelease(key);
/*
    DBGLOG( "RegisterForNetworkChange for SCDynamicStoreKeyCreateHostNames:\n" );
    key = SCDynamicStoreKeyCreateHostNames(NULL);
    CFArrayAppendValue(notifyKeys, key);
    CFRelease(key);
*/	
    setStatus = SCDynamicStoreSetNotificationKeys(mSCRef, notifyKeys, notifyPatterns);

    CFRelease(notifyKeys);
    CFRelease(notifyPatterns);

    if ( mRunLoopRef )
    {
        ::CFRunLoopAddCommonMode( mRunLoopRef, kCFRunLoopDefaultMode );
        scdStatus = ::SCDynamicStoreNotifyCallback( mSCRef, mRunLoopRef, SystemConfigurationNameChangedCallBack, this );
        DBGLOG( "NSLRequestMgrThread::RegisterForNetworkChanges, SCDynamicStoreNotifyCallback returned %ld\n", scdStatus );
    }
    else
        DBGLOG( "NSLRequestMgrThread::RegisterForNetworkChanges, No Current Run Loop, couldn't store Notify callback\n" );
}

// we want to set this up so that we will keep some special services registered (e.g. MacManager's workstation
// and we want to also keep track of the machine name since we'll want to keep this changing its registration when
// the machine changes.
tDirStatus DNSRegistrationThread::RegisterHostedServices( void )
{
	tDirStatus			registrationStatus = eDSNoErr;
	CFStringEncoding	encoding;
	CFStringRef			computerName = SCDynamicStoreCopyComputerName(NULL, &encoding);
	
	if ( computerName )
	{
		CFStringRef	ethernetAddress = CreateComputerNameEthernetString(computerName);
		
		if ( ethernetAddress )
		{
			char	ethernetAddressStr[1024] = {0,};
			CFStringGetCString( ethernetAddress, ethernetAddressStr, sizeof(ethernetAddressStr), kCFStringEncodingUTF8 );
			DBGLOG( "DNSRegistrationThread::RegisterHostedServices, registering %s\n", ethernetAddressStr );
			
			registrationStatus = PerformRegistration( ethernetAddress, kWorkstationTypeSAFE_CFSTR, kEmptySAFE_CFSTR, NULL, kWorkstationPortSAFE_CFSTR, &mOurSpecialRegKey );
			
			if ( mOurSpecialRegKey )
			{
				CFStringGetCString( mOurSpecialRegKey, ethernetAddressStr, sizeof(ethernetAddressStr), kCFStringEncodingUTF8 );
				DBGLOG( "DNSRegistrationThread::RegisterHostedServices set mOurSpecialRegKey to %s\n", ethernetAddressStr );
			}

			::CFRelease( ethernetAddress );
		}
		else
			DBGLOG("DNSRegistrationThread::RegisterHostedServices Could't get Ethernet Address!\n" );
		
		::CFRelease( computerName );
	}
	else
		DBGLOG("DNSRegistrationThread::RegisterHostedServices Could't get computer name!\n" );
	
	return registrationStatus;
}

tDirStatus DNSRegistrationThread::PerformRegistration( 	CFStringRef nameRef, 
														CFStringRef typeRef,
														CFStringRef domainRef,
														CFStringRef protocolSpecificData,
														CFStringRef portRef,
														CFStringRef* serviceKeyRef )
{
    Boolean useOldAPICalls	 = false;
	
	*serviceKeyRef = NULL;
	
    while (!mRunLoopRef)
    {
        DBGLOG("DNSRegistrationThread::PerformRegistration, waiting for mRunLoopRef\n");
        SmartSleep(500000);
    }    
    
    CFStringRef		modDomainRef = NULL;
    CFStringRef		modTypeRef = NULL;
    
    if ( CFStringCompare( domainRef, kEmptySAFE_CFSTR, 0 ) != kCFCompareEqualTo && !CFStringHasSuffix( domainRef, kDotSAFE_CFSTR ) )
    {
		DBGLOG( "DNSRegistrationThread::PerformRegistration appending \".\" to domain\n" );
        // we need to pass fully qualified domains (i.e. local. not local)
        modDomainRef = CFStringCreateMutableCopy( NULL, 0, domainRef );
        CFStringAppendCString( (CFMutableStringRef)modDomainRef, ".", kCFStringEncodingUTF8 );
    }

    if ( !CFStringHasSuffix( typeRef, kDotUnderscoreTCPSAFE_CFSTR ) )
    {
        // need to convert this to the appropriate DNS style.  I.E. _afp._tcp. not afp
        modTypeRef = CFStringCreateMutableCopy( NULL, 0, kUnderscoreSAFE_CFSTR );
        CFStringAppend( (CFMutableStringRef)modTypeRef, typeRef );
        CFStringAppend( (CFMutableStringRef)modTypeRef, kDotUnderscoreTCPSAFE_CFSTR );
    }
    
	CFStreamError error = {(CFStreamErrorDomain)0, 0};
	CFNetServiceRef entity = NULL;
	
    UInt32 port = CFStringGetIntValue( portRef );
    
	CFMutableStringRef		serviceKey = CFStringCreateMutable( NULL, 0 );
	
	if ( serviceKey )
	{
		// we are just going to make the key be the name.type.port.location
		CFStringAppend( serviceKey, nameRef );
		CFStringAppend( serviceKey, kDotSAFE_CFSTR );
		CFStringAppend( serviceKey, (modTypeRef)?modTypeRef:typeRef );
		CFStringAppend( serviceKey, (modDomainRef)?modDomainRef:domainRef );
	}
	
	DNSRegData*		regData = NULL;
    if ( (regData = (DNSRegData*)::CFDictionaryGetValue( mRegisteredServicesTable, serviceKey )) != NULL )
	{
#define USE_REF_COUNT_FOR_DUP_REGISTRATIONS
#ifdef USE_REF_COUNT_FOR_DUP_REGISTRATIONS
		regData->fCount++;			// we will just bump the counter, if we don't do this, then multiple registrations be ignored
									// and the first deregistration will deregister all previous registrations
		char	serviceKeyStr[1024] = {0,};
		CFStringGetCString( serviceKey, serviceKeyStr, sizeof(serviceKeyStr), kCFStringEncodingUTF8 );
		DBGLOG( "DNSRegistrationThread::PerformRegistration, service: %s (0x%x) is now registered %ld times\n", serviceKeyStr, regData->fCFNetServiceRef, regData->fCount );
#endif
	}
	else
	{
		if ( DEBUGGING_NSL )
		{
			char*		name = (char*)malloc( CFStringGetMaximumSizeForEncoding( CFStringGetLength(nameRef), kCFStringEncodingUTF8 ) + 1 );
			char*		location = (char*)malloc( CFStringGetMaximumSizeForEncoding( CFStringGetLength((modDomainRef)?modDomainRef:domainRef), kCFStringEncodingUTF8 ) + 1 );
			char*		service = (char*)malloc( CFStringGetMaximumSizeForEncoding( CFStringGetLength(typeRef), kCFStringEncodingUTF8 ) + 1 );
			
			if ( name && location && service )
			{
				CFStringGetCString( nameRef, name, CFStringGetMaximumSizeForEncoding( CFStringGetLength(nameRef), kCFStringEncodingUTF8 ) + 1, kCFStringEncodingUTF8 );

				CFStringGetCString( (modDomainRef)?modDomainRef:domainRef, location, CFStringGetMaximumSizeForEncoding( CFStringGetLength((modDomainRef)?modDomainRef:domainRef), kCFStringEncodingUTF8 ) + 1, kCFStringEncodingUTF8 );

				CFStringGetCString( typeRef, service, CFStringGetMaximumSizeForEncoding( CFStringGetLength(typeRef), kCFStringEncodingUTF8 ) + 1, kCFStringEncodingUTF8 );
				DBGLOG("DNSRegistrationThread::PerformRegistration for [%s] for service [%s] in domain [%s] using port %ld\n", name, service, location, port );
			}
			
			if ( name )
				free( name );

			if ( location )
				free( location );

			if ( service )
				free( service );
		}
		
        if ( CFStringCompare( nameRef, CFSTR(kUseMachineName), 0 ) == kCFCompareEqualTo || GetParentPlugin()->GetComputerNameString() && CFStringCompare( GetParentPlugin()->GetComputerNameString(), nameRef, 0 ) == kCFCompareEqualTo )
			nameRef = kEmptySAFE_CFSTR;	// use default

		entity = CFNetServiceCreate(NULL, (modDomainRef)?modDomainRef:domainRef, (modTypeRef)?modTypeRef:typeRef, nameRef, port);
		
		if ( protocolSpecificData && CFGetTypeID(protocolSpecificData) == CFStringGetTypeID() )
		{
			
			CFDictionaryRef		txtDataAsDictionary = CreateMutableDictionaryFromXMLString( (CFStringRef)protocolSpecificData );
			
			if ( txtDataAsDictionary )
			{
				CFDataRef         txtRecord = NULL;
				
				DBGLOG("DNSRegistrationThread::PerformRegistration adding TXT data dictionary type data to registration\n" );
				txtRecord = CFNetServiceCreateTXTDataWithDictionary( NULL, txtDataAsDictionary );
				
				CFNetServiceSetTXTData( entity, txtRecord );
				CFRelease( txtRecord );
			}
			else
			{
				CFNetServiceSetProtocolSpecificInformation( entity, protocolSpecificData );
				DBGLOG("DNSRegistrationThread::PerformRegistration adding TXT data text type data to registration\n" );
				
				useOldAPICalls = true;
			}
		}
		
		CFNetServiceClientContext c = {0, NULL, NULL, NULL, CopyRegistrationDescription};
		if ( !CFNetServiceSetClient( entity, RegisterEntityCallBack, &c) )
			syslog( LOG_ERR, "DS Bonjour was unable to register a service with CFNetService!\n" );
		CFNetServiceScheduleWithRunLoop(entity, mRunLoopRef, kCFRunLoopDefaultMode);
		
		if ( useOldAPICalls )
		{
			if (CFNetServiceRegister(entity, &error))
			{
				CFRunLoopWakeUp( mRunLoopRef );
				DBGLOG("CFNetServiceRegister returned TRUE!\n");
			}
			else
				DBGLOG("CFNetServiceRegister returned FALSE (%d, %ld).\n", error.domain, error.error);
		}
		else
		{
			if (CFNetServiceRegisterWithOptions(entity, 0, &error))
			{
				DBGLOG("CFNetServiceRegister started\n");
			}
			else
				DBGLOG("CFNetServiceRegister returned FALSE (%d, %ld).\n", error.domain, error.error);
		}
		
		if ( !error.error && serviceKey )
		{
			*serviceKeyRef = serviceKey;
			CFRetain( *serviceKeyRef );
			
			regData = (DNSRegData*)malloc(sizeof(DNSRegData));
			regData->fCount = 1;
			regData->fCFNetServiceRef = entity;
			::CFDictionaryAddValue( mRegisteredServicesTable, serviceKey, (const void*)regData );

			char	serviceKeyStr[1024] = {0,};
			CFStringGetCString( serviceKey, serviceKeyStr, sizeof(serviceKeyStr), kCFStringEncodingUTF8 );
			DBGLOG( "DNSRegistrationThread::PerformRegistration, registering with CFNetService: %s (0x%x)\n", serviceKeyStr, regData->fCFNetServiceRef );
		}
	}
			
	if ( serviceKey )
		CFRelease( serviceKey );
			
	if ( modDomainRef )
		CFRelease( modDomainRef );

	if ( modTypeRef )
		CFRelease( modTypeRef );
	
    return (tDirStatus)error.error;
}

tDirStatus DNSRegistrationThread::PerformDeregistration( CFDictionaryRef service )
{
    tDirStatus		status = eDSRecordNotFound;
    
	if ( !service )
		return status;
		
    CFStringRef		modDomainRef = NULL;
    CFStringRef		modTypeRef = NULL;
    CFStringRef		domainRef = NULL;
    CFStringRef		nameOfService = NULL;			/* Service's name (must be unique per domain (should be UTF8) */
    CFStringRef		typeOfService = NULL;			/* Service Type (i.e. afp, lpr etc) */

	nameOfService = (CFStringRef)::CFDictionaryGetValue( service, kDSNAttrRecordNameSAFE_CFSTR );
	if ( !nameOfService )
		nameOfService = kEmptySAFE_CFSTR;		// just deregister Copmuter Name

	domainRef = (CFStringRef)::CFDictionaryGetValue( service, kDS1AttrLocationSAFE_CFSTR );
	if ( !domainRef )
		domainRef = kEmptySAFE_CFSTR;		// just deregister local
	else
	if ( CFStringCompare( domainRef, CFSTR("local"), 0 ) == kCFCompareEqualTo )
	{
		DBGLOG( "DNSRegistrationThread::PerformDeregistration, deregistering in local, use \"\"\n" );
		domainRef = kEmptySAFE_CFSTR;		// just deregister local
	}
		
	typeOfService = (CFStringRef)::CFDictionaryGetValue( service, kDS1AttrServiceTypeSAFE_CFSTR );
	if ( !typeOfService )
		typeOfService = (CFStringRef)::CFDictionaryGetValue( service, kDSNAttrRecordTypeSAFE_CFSTR );

    if ( CFStringCompare( domainRef, kEmptySAFE_CFSTR, 0 ) != kCFCompareEqualTo && !CFStringHasSuffix( domainRef, kDotSAFE_CFSTR ) )
    {
        // we need to pass fully qualified domains (i.e. local. not local)
        modDomainRef = CFStringCreateMutableCopy( NULL, 0, domainRef );
        CFStringAppendCString( (CFMutableStringRef)modDomainRef, ".", kCFStringEncodingUTF8 );
    }

    if ( !CFStringHasSuffix( typeOfService, kDotUnderscoreTCPSAFE_CFSTR ) )
    {
        // need to convert this to the appropriate DNS style.  I.E. _afp._tcp. not afp
        modTypeRef = CFStringCreateMutableCopy( NULL, 0, kUnderscoreSAFE_CFSTR );
        CFStringAppend( (CFMutableStringRef)modTypeRef, typeOfService );
        CFStringAppend( (CFMutableStringRef)modTypeRef, kDotUnderscoreTCPSAFE_CFSTR );
    }
    
	CFMutableStringRef		serviceKey = CFStringCreateMutable( NULL, 0 );
	
	if ( serviceKey )
	{
		// we are just going to make the key be the name.type.port.location
		CFStringAppend( serviceKey, nameOfService );
		CFStringAppend( serviceKey, kDotSAFE_CFSTR );
		CFStringAppend( serviceKey, (modTypeRef)?modTypeRef:typeOfService );
		CFStringAppend( serviceKey, (modDomainRef)?modDomainRef:domainRef );
	
		status = PerformDeregistration( serviceKey );
		
		CFRelease( serviceKey );
	}
	
	if ( modTypeRef != NULL )
	{
		CFRelease( modTypeRef );
		modTypeRef = NULL;
	}
	if ( modDomainRef != NULL )
	{
		CFRelease( modDomainRef );
		modDomainRef = NULL;
	}
    	
    return status;
}

tDirStatus DNSRegistrationThread::PerformDeregistration( CFStringRef serviceKey )
{
	DNSRegData*		regData = NULL;
	CFNetServiceRef entity = NULL;
    tDirStatus		status = eDSRecordNotFound;

    if ( serviceKey && (regData = (DNSRegData*)::CFDictionaryGetValue( mRegisteredServicesTable, serviceKey )) != NULL )
    {
		{
			char	serviceKeyStr[1024] = {0,};
			CFStringGetCString( serviceKey, serviceKeyStr, sizeof(serviceKeyStr), kCFStringEncodingUTF8 );
			DBGLOG( "DNSRegistrationThread::PerformDeregistration, deregistering CFNetService: %s (0x%x)\n", serviceKeyStr, regData->fCFNetServiceRef );
		}
		
		entity = regData->fCFNetServiceRef;
		
		if ( entity && regData->fCount == 1 )
        {
			::CFDictionaryRemoveValue( mRegisteredServicesTable, serviceKey );
        
			CFNetServiceUnscheduleFromRunLoop( entity, mRunLoopRef, kCFRunLoopDefaultMode );		// need to unschedule from run loop
			if ( !CFNetServiceSetClient( entity, NULL, NULL ) )
				syslog( LOG_ERR, "DS Bonjour was unable to unregister a service with CFNetService!\n" );
				
			CFNetServiceCancel( entity );
			CFRelease( entity );

			free( regData );
            status = eDSNoErr;
        }
		else
		{
			regData->fCount--;	// decrement this
			DBGLOG( "DNSRegistrationThread::PerformDeregistration, service is now registered %ld times\n", regData->fCount );
		}
    }
	else if ( serviceKey )
	{
		char	serviceKeyStr[1024] = {0,};
		CFStringGetCString( serviceKey, serviceKeyStr, sizeof(serviceKeyStr), kCFStringEncodingUTF8 );
		DBGLOG( "DNSRegistrationThread::PerformDeregistration, unable to deregister CFNetService: %s as there no match in our registered services table!\n", serviceKeyStr );
	}
    
    return status;
}

boolean_t SystemConfigurationNameChangedCallBack(SCDynamicStoreRef session, void *callback_argument)
{                       
    DNSRegistrationThread* regThread = (DNSRegistrationThread*)callback_argument;
    
	DBGLOG( "SystemConfigurationNameChangedCallBack called\n" );

    regThread->PerformDeregistration( regThread->GetOurSpecialRegKey() );		// deregister old service, since the name isn't valid
    regThread->RegisterHostedServices();						// register with current name
    
    return true;
}

static void RegisterEntityCallBack(CFNetServiceRef theEntity, CFStreamError* error, void* info)
{
    if ( error->error )
		DBGLOG( "Registration is finished error: (%d, %ld).\n", error->domain, error->error );
	else
		DBGLOG( "Registration has started\n" );
}

CFStringRef CopyCancelRegDescription( const void* info )
{
    DBGLOG( "CopyCancelRegDescription called\n" );
    CFNetServiceRef	theEntity = (CFNetServiceRef)info;
	CFStringRef		description = CFNetServiceGetName(theEntity);
    
    CFRetain( description );
    return description;
}

CFStringRef CopyRegistrationDescription( const void* info )
{
    CFStringRef	description = kDNSSCDynamicStoreKeySAFE_CFSTR;
    DBGLOG( "CopyRegistrationDescription called\n" );
    
    CFRetain( description );
    return description;
}

#define	kMaxLengthOfNamePortionOfString			64-sizeof(" [00:00:00:00:00:00]")
CFStringRef	CreateComputerNameEthernetString( CFStringRef computerName )
{
	CFMutableStringRef		modString = NULL;
	
	if ( computerName )
	{
		char					testDNSStringBuf[kMaxLengthOfNamePortionOfString];		// we need to test the actual string
		CFStringRef				macAddress = CreateMacAddressString();
		modString = CFStringCreateMutableCopy( NULL, 0, computerName );
    
		while ( CFStringGetLength(modString)>0 && !CFStringGetCString( modString, testDNSStringBuf, sizeof(testDNSStringBuf), kCFStringEncodingUTF8 ) )
		{
			DBGLOG( "DNSRegistrationThread::CreateComputerNameEthernetString name is too long, need to try trimming...\n" );
			CFStringDelete( modString, CFRangeMake(CFStringGetLength(modString)-1, 1) );
		}
			
		CFStringAppend( modString, kSpaceLeftBracketSAFE_CFSTR );
		if ( macAddress )
			CFStringAppend( modString, macAddress );
		else
			CFStringAppend( modString, kZeroedMACAddressSAFE_CFSTR );
			
		CFStringAppend( modString, kRightBracketSAFE_CFSTR );
	
		if ( macAddress )
			CFRelease( macAddress );
			
		if ( getenv( "NSLDEBUG" ) )
		{
			DBGLOG( "DNSRegistrationThread::CreateComputerNameEthernetString created new composite name\n" );
			CFShow( modString );
		}
    }
	
    return modString;
}

CFStringRef	CreateMacAddressString( void )
{
    LinkAddresses_t *	link_addrs;
    char*				macAddrCString = NULL;
    CFStringRef			macAddrStringRef = NULL;
    
    link_addrs = LinkAddresses_create();
    if (link_addrs) 
    {
        int i;
        for (i = 0; i < link_addrs->count; i++) 
        {
            struct sockaddr_dl * sdl = link_addrs->list[i];

            macAddrCString = sockaddr_dl_create_macaddr_string( sdl, "en0" );
            
            if ( macAddrCString )
                break;
        }
        LinkAddresses_free(&link_addrs);
    }
    
    if ( macAddrCString )
    {
        macAddrStringRef = CFStringCreateWithCString( NULL, macAddrCString, kCFStringEncodingUTF8 );
        free( macAddrCString );
    }

    return macAddrStringRef;
}

CFMutableDictionaryRef CreateMutableDictionaryFromXMLString( CFStringRef xmlplist )
{
	if ( !xmlplist )
		return NULL;
		
	CFDataRef						xmlData						= NULL;
	CFMutableDictionaryRef			newDataRef					= NULL;
	CFStringRef						errorString					= NULL;
	char*							rawDataPtr					= NULL;
	bool							rawDataAlloced				= false;
	
	rawDataPtr = (char*)CFStringGetCStringPtr( xmlplist, kCFStringEncodingUTF8 );
	
	if ( !rawDataPtr )
	{
		int		bufLen = CFStringGetMaximumSizeForEncoding( CFStringGetLength(xmlplist), kCFStringEncodingUTF8 ) + 1;
		
		rawDataPtr = (char*)malloc( bufLen );
		CFStringGetCString( xmlplist, rawDataPtr, bufLen, kCFStringEncodingUTF8 );
		rawDataAlloced = true;
	}
	
	DBGLOG( "CreateMutableDictionaryFromXMLString called on\n%s\n", rawDataPtr );

	xmlData = CFDataCreate(NULL,(UInt8 *)rawDataPtr, strlen(rawDataPtr));
	
	if ( xmlData )
	{
		newDataRef = (CFMutableDictionaryRef)CFPropertyListCreateFromXMLData(	NULL,
																				xmlData,
																				kCFPropertyListMutableContainersAndLeaves,
																				&errorString);
		if ( errorString )
		{
			char*		errorStr = (char*)malloc(CFStringGetLength( errorString ) + 1);
			
			if ( errorStr )
			{
				CFStringGetCString( errorString, errorStr, CFStringGetLength( errorString ) + 1, kCFStringEncodingUTF8 );
				DBGLOG( "CreateMutableDictionaryFromXMLString got the error [%s] trying to parse the XML: [%s]\n", errorStr, rawDataPtr );
				free( errorStr );
			}

			CFRelease( errorString );
		}
		else if ( newDataRef && CFGetTypeID(newDataRef) == CFDictionaryGetTypeID() && CFPropertyListIsValid( newDataRef, kCFPropertyListXMLFormat_v1_0 ) )
		{
			DBGLOG( "CreateMutableDictionaryFromXMLString successfully created newDataRef: 0x%x\n", newDataRef );
		}
		else
		{
			DBGLOG( "CreateMutableDictionaryFromXMLString: newDataRef is not a valid dictionary, returning NULL\n" );
			CFRelease( newDataRef );
			newDataRef = NULL;
		}
		
		CFRelease( xmlData );
	}

	if ( rawDataAlloced )
		free( rawDataPtr );
		
	return newDataRef;
}



