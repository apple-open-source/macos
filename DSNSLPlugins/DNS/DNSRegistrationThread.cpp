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

#define kOurSpecialRegRef -1

typedef struct DNSRegData {
	CFNetServiceRef		fCFNetServiceRef;
	UInt32				fCount;

} DNSRegData;

static void RegisterEntityCallBack(CFNetServiceRef theEntity, CFStreamError* error, void* info);
CFStringRef CopyCancelRegDescription( void* info );
CFStringRef CopyRegistrationDescription( void* info );
void CancelRegThread(CFRunLoopTimerRef timer, void *info);
boolean_t SystemConfigurationNetworkChangedCallBack(SCDynamicStoreRef session, void *callback_argument);

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
        mSCRef = ::SCDynamicStoreCreate(NULL, CFSTR("com.apple.DirectoryServices.DNS"), NULL, NULL);
}
/*
void* DNSRegistrationThread::Run( void )
{
    // We just want to set up and do a CFRunLoop
    Initialize();
    DBGLOG("DNSRegistrationThread::Run called, just going to start up a CFRunLoop\n" );
    mRunLoopRef = CFRunLoopGetCurrent();
        
    if ( !mSCRef )
        mSCRef = ::SCDynamicStoreCreate(NULL, CFSTR("com.apple.DirectoryServices.DNS"), NULL, NULL);
    
    CFRunLoopTimerContext 		c = {0, this, NULL, NULL, NULL};
    CFRunLoopTimerRef 			timer = CFRunLoopTimerCreate(NULL, 1.0e20, 0, 0, 0, CancelRegThread, (CFRunLoopTimerContext*)&c);
    CFRunLoopAddTimer(mRunLoopRef, timer, kCFRunLoopDefaultMode);
    
    CFRunLoopRun();
    
    DBGLOG("DNSRegistrationThread::Run, CFRunLoop finished - exiting thread\n" );
    
    return NULL;
}
*/
// we want to set this up so that we will keep some special services registered (e.g. MacManager's workstation
// and we want to also keep track of the machine name since we'll want to keep this changing its registration when
// the machine changes.
void DNSRegistrationThread::RegisterHostedServices( void )
{
//    if ( !mListOfServicesToRegisterManually )
    {
        // just create a service for MacManager
//        mListOfServicesToRegisterManually = CFArrayCreateMutable( NULL, 1, &kCFTypeArrayCallBacks );		// only allowing 1 item for now!
        
//        CFArrayAddValue( mListOfServicesToRegisterManually, newService );

        CFStringEncoding	encoding;
        CFStringRef			computerName = SCDynamicStoreCopyComputerName(NULL, &encoding);
        
        if ( computerName )
        {
            CFStringRef	ethernetAddress = CreateComputerNameEthernetString(computerName);
            
            if ( ethernetAddress )
            {
                PerformRegistration( ethernetAddress, CFSTR("_workstation._tcp."), CFSTR(""), NULL, CFSTR("9"), &mOurSpecialRegKey );
//                PerformRegistration( ethernetAddress, CFSTR("_workstation._tcp."), CFSTR("local."), NULL, CFSTR("9"), &mOurSpecialRegKey );
                ::CFRelease( ethernetAddress );
            }
            else
                DBGLOG("DNSRegistrationThread::RegisterHostedServices Could't get Ethernet Address!\n" );
            
            ::CFRelease( computerName );
        }
        else
            DBGLOG("DNSRegistrationThread::RegisterHostedServices Could't get computer name!\n" );
    }
}

CFStringRef	DNSRegistrationThread::CreateComputerNameEthernetString( CFStringRef computerName )
{
    CFMutableStringRef		modString = CFStringCreateMutableCopy( NULL, 0, computerName );
    CFStringRef				macAddress = CreateMacAddressString();
    
    CFStringAppend( modString, CFSTR(" [") );
    if ( macAddress )
        CFStringAppend( modString, macAddress );
    else
        CFStringAppend( modString, CFSTR("0:0:0:0:0:0") );
        
    CFStringAppend( modString, CFSTR("]") );

    if ( macAddress )
        CFRelease( macAddress );
        
    if ( getenv( "NSLDEBUG" ) )
    {
        DBGLOG( "DNSRegistrationThread::CreateComputerNameEthernetString created new composite name\n" );
        CFShow( modString );
    }
    
    return modString;
}

CFStringRef DNSRegistrationThread::CreateMacAddressString( void )
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

tDirStatus DNSRegistrationThread::PerformRegistration( 	CFStringRef nameRef, 
														CFStringRef typeRef,
														CFStringRef domainRef,
														CFStringRef protocolSpecificData,
														CFStringRef portRef,
														CFStringRef* serviceKeyRef )
{
    char mode = 'A';
    
	*serviceKeyRef = NULL;
	
    while (!mRunLoopRef)
    {
        DBGLOG("DNSRegistrationThread::PerformRegistration, waiting for mRunLoopRef\n");
        usleep(500000);
    }    
    
    CFStringRef		modDomainRef = NULL;
    CFStringRef		modTypeRef = NULL;
    
    if ( CFStringCompare( domainRef, CFSTR(""), 0 ) != kCFCompareEqualTo && !CFStringHasSuffix( domainRef, CFSTR(".") ) )
    {
        // we need to pass fully qualified domains (i.e. local. not local)
        modDomainRef = CFStringCreateMutableCopy( NULL, 0, domainRef );
        CFStringAppendCString( (CFMutableStringRef)modDomainRef, ".", kCFStringEncodingUTF8 );
    }

    if ( !CFStringHasSuffix( typeRef, CFSTR("._tcp.") ) )
    {
        // need to convert this to the appropriate DNS style.  I.E. _afp._tcp. not afp
        modTypeRef = CFStringCreateMutableCopy( NULL, 0, CFSTR("_") );
        CFStringAppend( (CFMutableStringRef)modTypeRef, typeRef );
        CFStringAppend( (CFMutableStringRef)modTypeRef, CFSTR("._tcp.") );
    }
    
	CFStreamError error = {(CFStreamErrorDomain)0, 0};
	CFNetServiceRef entity = NULL;
	
    UInt32 port = CFStringGetIntValue( portRef );
    
	CFMutableStringRef		serviceKey = CFStringCreateMutable( NULL, 0 );
	
	if ( serviceKey )
	{
		// we are just going to make the key be the name.type.location
		CFStringAppend( serviceKey, nameRef );
		CFStringAppend( serviceKey, CFSTR(".") );
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
#endif
	}
	else
	{
		DBGLOG("DNSRegistrationThread::PerformRegistration, port is %ld\n", port );
		entity = CFNetServiceCreate(NULL, (modDomainRef)?modDomainRef:domainRef, (modTypeRef)?modTypeRef:typeRef, nameRef, port);
		
		if ( protocolSpecificData )
		{
			CFNetServiceSetProtocolSpecificInformation( entity, protocolSpecificData );
		}
	
		mode = 'A';
		{
			CFRetain( nameRef );
			CFNetServiceClientContext c = {0, (void*)nameRef, NULL, NULL, CopyRegistrationDescription};
			CFNetServiceSetClient(entity, RegisterEntityCallBack, &c);
			CFNetServiceScheduleWithRunLoop(entity, mRunLoopRef, kCFRunLoopDefaultMode);
		}
		
		if (CFNetServiceRegister(entity, &error))
		{
			CFRunLoopWakeUp( mRunLoopRef );
			DBGLOG("CFNetServiceRegister returned TRUE!\n");
		}
		else
			DBGLOG("CFNetServiceRegister returned FALSE (%d, %ld).\n", error.domain, error.error);
			
		if ( !error.error && serviceKey )
		{
			*serviceKeyRef = serviceKey;
			CFRetain( *serviceKeyRef );
			
			regData = (DNSRegData*)malloc(sizeof(DNSRegData));
			regData->fCount = 1;
			regData->fCFNetServiceRef = entity;
			::CFDictionaryAddValue( mRegisteredServicesTable, serviceKey, (const void*)regData );
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

	nameOfService = (CFStringRef)::CFDictionaryGetValue( service, CFSTR(kDSNAttrRecordName) );
	domainRef = (CFStringRef)::CFDictionaryGetValue( service, CFSTR(kDS1AttrLocation) );
	if ( !domainRef )
		domainRef = CFSTR("");		// just deregister local
//		domainRef = CFSTR("local.");		// just deregister local
		
	typeOfService = (CFStringRef)::CFDictionaryGetValue( service, CFSTR(kDS1AttrServiceType) );
	if ( !typeOfService )
		typeOfService = (CFStringRef)::CFDictionaryGetValue( service, CFSTR(kDSNAttrRecordType) );

    if ( CFStringCompare( domainRef, CFSTR(""), 0 ) != kCFCompareEqualTo && !CFStringHasSuffix( domainRef, CFSTR(".") ) )
    {
        // we need to pass fully qualified domains (i.e. local. not local)
        modDomainRef = CFStringCreateMutableCopy( NULL, 0, domainRef );
        CFStringAppendCString( (CFMutableStringRef)modDomainRef, ".", kCFStringEncodingUTF8 );
    }

    if ( !CFStringHasSuffix( typeOfService, CFSTR("._tcp.") ) )
    {
        // need to convert this to the appropriate DNS style.  I.E. _afp._tcp. not afp
        modTypeRef = CFStringCreateMutableCopy( NULL, 0, CFSTR("_") );
        CFStringAppend( (CFMutableStringRef)modTypeRef, typeOfService );
        CFStringAppend( (CFMutableStringRef)modTypeRef, CFSTR("._tcp.") );
    }
    
	CFMutableStringRef		serviceKey = CFStringCreateMutable( NULL, 0 );
	
	if ( serviceKey )
	{
		// we are just going to make the key be the name._type._tcp.location.
		CFStringAppend( serviceKey, nameOfService );
		CFStringAppend( serviceKey, CFSTR(".") );
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

    if ( (regData = (DNSRegData*)::CFDictionaryGetValue( mRegisteredServicesTable, serviceKey )) != NULL )
    {
		entity = regData->fCFNetServiceRef;
		
		if ( entity && regData->fCount == 1 )
        {
			::CFDictionaryRemoveValue( mRegisteredServicesTable, serviceKey );
        
			CFNetServiceUnscheduleFromRunLoop( entity, mRunLoopRef, kCFRunLoopDefaultMode );		// need to unschedule from run loop
			CFNetServiceSetClient( entity, NULL, NULL );
			CFNetServiceCancel( entity );
			CFRelease( entity );

			free( regData );
            status = eDSNoErr;
        }
		else
		{
			regData->fCount--;	// decrement this
			DBGLOG( "DNSRegistrationThread::PerformDeregistration, this service is now registered %ld times\n", regData->fCount );
		}
    }
	else
		DBGLOG( "DNSRegistrationThread::PerformDeregistration couldn't locate service in RegisteredServicesTable!\n" );
    
    return status;
}

boolean_t SystemConfigurationNetworkChangedCallBack(SCDynamicStoreRef session, void *callback_argument)
{                       
    DNSRegistrationThread* regThread = (DNSRegistrationThread*)callback_argument;
    
    regThread->PerformDeregistration( regThread->GetOurSpecialRegKey() );		// deregister old service, since the name isn't valid
    regThread->RegisterHostedServices();						// register with current name
    
    return true;
}

static void RegisterEntityCallBack(CFNetServiceRef theEntity, CFStreamError* error, void* info)
{
    DBGLOG( "Registration is finished error: (%d, %ld).\n", error->domain, error->error);
}

CFStringRef CopyCancelRegDescription( void* info )
{
    DBGLOG( "CopyCancelRegDescription called\n" );
    CFNetServiceRef	theEntity = (CFNetServiceRef)info;
	CFStringRef		description = CFNetServiceGetName(theEntity);
    
    CFRetain( description );
    return description;
}

CFStringRef CopyRegistrationDescription( void* info )
{
    CFStringRef	description = (CFStringRef)info;
    DBGLOG( "CopyRegistrationDescription called\n" );
    
    CFRetain( description );
    return description;
}

void CancelRegThread(CFRunLoopTimerRef timer, void *info) 
{
    DNSRegistrationThread* 		regThread = (DNSRegistrationThread*)info;
    
    DBGLOG("CancelBrowse called\n" );
    regThread->Cancel();
}



