/*
 *  SLPSystemConfiguration.cpp
 *  NSLPlugins
 *
 *  Created by imlucid on Fri Sep 21 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
 */
#include <stdio.h>
#include <string.h>
#include <sys/un.h>

#include "mslp_sd.h"
#include "slp.h"
#include "mslp.h"
#include "mslpd_store.h"
#include "mslp_dat.h"
#include "mslplib.h"     /* Definitions specific to the mslplib  */
//#include "mslpd.h"
//#include "mslpd_mask.h"

//#include "mslpd_parse.h"

#include "SLPSystemConfiguration.h"
#include "SLPDALocator.h"

#define DHCPTAG_SLP_DIRECTORY_AGENT		78
#define DHCPTAG_SLP_SERVICE_SCOPE		79

boolean_t SLPSystemConfigurationNetworkChangedCallBack(SCDynamicStoreRef session, void *callback_argument);

// C Wrapper functions
EXPORT const char* GetEncodedScopeToRegisterIn( void )
{
    return SLPSystemConfiguration::TheSLPSC()->GetEncodedScopeToRegisterIn();
}

EXPORT void InitializeSLPSystemConfigurator( CFRunLoopRef runLoopRef )
{
    SLPSystemConfiguration::TheSLPSC(runLoopRef)->Initialize();
}

EXPORT void DeleteRegFileIfFirstStartupSinceBoot( void )
{
// We are just going to have Directory Services delete the file
//	SLPSystemConfiguration::TheSLPSC()->CheckIfFirstLaunchSinceReboot();	// if this is first time, clean up some stuff
}

EXPORT CFStringRef CopyCurrentActivePrimaryInterfaceName( void )
{
    return SLPSystemConfiguration::TheSLPSC()->CopyCurrentActivePrimaryInterfaceName();
}

EXPORT CFStringRef CopyConfiguredInterfaceToUse( void )
{
    return SLPSystemConfiguration::TheSLPSC()->CopyConfiguredInterfaceToUse();
}

EXPORT bool OnlyUsePreConfiguredDAs( void )
{
    return SLPSystemConfiguration::TheSLPSC()->OnlyUsePreConfiguredDAs();
}

EXPORT bool ServerScopeSponsoringEnabled( void )
{
    return SLPSystemConfiguration::TheSLPSC()->ServerScopeSponsoringEnabled();
}

EXPORT int SizeOfServerScopeSponsorData( void )
{
    return SLPSystemConfiguration::TheSLPSC()->SizeOfServerScopeSponsorData();
}

EXPORT const char* GetServerScopeSponsorData( void )
{
    return SLPSystemConfiguration::TheSLPSC()->GetServerScopeSponsorData();
}

SLPSystemConfiguration*	SLPSystemConfiguration::msSLPSC = NULL;

SLPSystemConfiguration* SLPSystemConfiguration::TheSLPSC( CFRunLoopRef runLoopRef )
{
    if ( !msSLPSC )
    {
        msSLPSC = new SLPSystemConfiguration(runLoopRef);
        msSLPSC->Initialize();
    }
    
    return msSLPSC;
}

void SLPSystemConfiguration::FreeSLPSC( void )
{
    if ( msSLPSC )
        free( msSLPSC );
    
    msSLPSC = NULL;
}

SLPSystemConfiguration::SLPSystemConfiguration( CFRunLoopRef runLoopRef )
{
    SLP_LOG( SLP_LOG_CONFIG, "New SLPSystemConfiguration created" );
    
    mSCRef = 0;
    mEncodedScopeToRegisterIn = NULL;
    mUseOnlyPreConfiguredDAs = false;
    mServerScopeSponsoringEnabled = false;
    mServerScopeSponsorData = NULL;
    mSizeOfServerScopeSponsorData = 0;
    mConfiguredInterfaceToUse = NULL;
	mMainRunLoopRef = runLoopRef;
}


SLPSystemConfiguration::~SLPSystemConfiguration()
{
    SLP_LOG( SLP_LOG_CONFIG, "SLPSystemConfiguration deleted" );
    
    if ( mEncodedScopeToRegisterIn )
        free( mEncodedScopeToRegisterIn );
    
    if ( mConfiguredInterfaceToUse )
        CFRelease( mConfiguredInterfaceToUse );
    
    UnRegisterForNetworkChange();
        
    if ( mSCRef )
        CFRelease( mSCRef );
    
    mSCRef = 0;
}


const char* SLPSystemConfiguration::GetEncodedScopeToRegisterIn( void )
{
    return mEncodedScopeToRegisterIn;
}

void SLPSystemConfiguration::SetEncodedScopeToRegisterIn( const char* scope, bool encodedAlready )
{
    if (	mEncodedScopeToRegisterIn 
        &&	scope
        &&	strlen(mEncodedScopeToRegisterIn) == strlen(scope)
        &&	memcmp( mEncodedScopeToRegisterIn, scope, strlen(scope) == 0 ) )
    {
        // cool, we already have this thank you.  Just return
        return;
    }
    
    if ( mEncodedScopeToRegisterIn )
    {
        free( mEncodedScopeToRegisterIn );
    }
        
    if ( scope )
    {
        if ( encodedAlready )
        {
            mEncodedScopeToRegisterIn = (char*)malloc( strlen(scope) + 1 );
            strcpy( mEncodedScopeToRegisterIn, scope );
        }
        else
            SLPEscape( scope, &mEncodedScopeToRegisterIn );
        
        // we should do our reg/dereg thing with all DAs
    }
    else
        mEncodedScopeToRegisterIn = NULL;
}

void SLPSystemConfiguration::Initialize( void )
{
	SLP_LOG( SLP_LOG_CONFIG, "SLPSystemConfiguration::Initialize\n" );
    if ( !mSCRef )
    {
        mSCRef = ::SCDynamicStoreCreate(NULL, CFSTR("com.apple.slp"), NULL, NULL);
        
        if ( !mSCRef )
            SLP_LOG( SLP_LOG_ERR, "SLPSystemConfiguration, mSCRef is NULL after a call to SCDynamicStoreCreate!" );
            
        DeterminePreconfiguredDAAgentInformation();
        
        DeterminePreconfiguredRegistrationScopeInformation();
        
        DeterminePreconfiguredInterfaceInformation();
        
		CalculateOurIPAddress( &mCurIPAddr, &mCurInterface );
        RegisterForNetworkChange();
    }
}

int SLPSystemConfiguration::GetOurIPAdrs( struct in_addr* ourIPAddr, const char** pcInterf )
{
	if ( ourIPAddr )
		*ourIPAddr = mCurIPAddr;
	
	if ( pcInterf )
		*pcInterf = strdup( mCurInterface );
	
	return 0;
}

SInt32 SLPSystemConfiguration::RegisterForNetworkChange( void )
{
	SInt32				scdStatus			= 0;
	CFStringRef			ipKey				= 0;	//ip changes key
//	CFStringRef			computerNameKey 	= 0;	//computer name changes key
	CFMutableArrayRef	notifyKeys			= 0;
	CFMutableArrayRef	notifyPatterns		= 0;
	Boolean				setStatus			= FALSE;
	
    SLP_LOG( SLP_LOG_DEBUG, "RegisterForNetworkChange" );

	if (mSCRef != 0)
	{
		if ( !mMainRunLoopRef )
			mMainRunLoopRef = ::CFRunLoopGetCurrent();
        
		notifyKeys		= CFArrayCreateMutable(	kCFAllocatorDefault,
												0,
												&kCFTypeArrayCallBacks);
		notifyPatterns	= CFArrayCreateMutable(	kCFAllocatorDefault,
												0,
												&kCFTypeArrayCallBacks);
		//CFArrayAppendValue(notifyPatterns, kSCCompAnyRegex); //formerly kSCDRegexKey
												
        // ip changes
//        if ( WantIPChangeNotification() )
        {
            SLP_LOG( SLP_LOG_DEBUG, "RegisterForNetworkChange for kSCEntNetIPv4:\n" );
            ipKey = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetIPv4);
            CFArrayAppendValue(notifyKeys, ipKey);
            CFRelease(ipKey);
        }
#if 0        
        // AppleTalk changes
//		if ( WantAppleTalkChangeNotification() )
        {
            SLP_LOG( SLP_LOG_DEBUG, "RegisterForNetworkChange for kSCEntNetAppleTalk:\n" );
            atKey = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetAppleTalk);
            CFArrayAppendValue(notifyKeys, atKey);
            CFRelease(atKey);
        }
        
        // computer name changes
//		if ( WantAppleTalkChangeNotification() )
        {
            SLP_LOG( SLP_LOG_DEBUG, "RegisterForNetworkChange for kSCPropNetAppleTalkComputerName:\n" );
            atKey = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCPropNetAppleTalkComputerName);
            CFArrayAppendValue(notifyKeys, atKey);
            CFRelease(atKey);
        }
        // DNS changed
		//dnsKey = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetDNS);
		//CFArrayAppendValue(notifyKeys, dnsKey);
		//CFRelease(dnsKey);
               
        // NIS changed
		//nisKey = SCDynamicStoreKeyCreateNetworkGlobalEntity(NULL, kSCDynamicStoreDomainState, kSCEntNetNIS);
		//CFArrayAppendValue(notifyKeys, nisKey);
		//CFRelease(nisKey);

		//same mechanism that lookupd daemon currently uses to restart itself
		//although in our case we simply stop and allow any client through our
		//framework to restart us when required
		//CFArrayAppendValue(notifyKeys, CFSTR("File:/var/run/nibindd.pid"));
#endif
        setStatus = SCDynamicStoreSetNotificationKeys(mSCRef, notifyKeys, notifyPatterns);
		CFRelease(notifyKeys);
		CFRelease(notifyPatterns);

        if ( mMainRunLoopRef )
        {
            ::CFRunLoopAddCommonMode( mMainRunLoopRef, kCFRunLoopDefaultMode );
            scdStatus = ::SCDynamicStoreNotifyCallback( mSCRef, mMainRunLoopRef, SLPSystemConfigurationNetworkChangedCallBack, this );
            SLP_LOG( SLP_LOG_DEBUG, "SCDynamicStoreNotifyCallback returned %ld\n", scdStatus );
        }
        else
            SLP_LOG( SLP_LOG_DEBUG, "No Current Run Loop, couldn't store Notify callback\n" );
//		setStatus = SCDynamicStoreNotifySignal( mSCDStore, getpid(), SIGHUP);
		
//		aPIDString = CFStringCreateWithFormat(kCFAllocatorDefault, NULL, CFSTR("%d"), (uInt32)getpid());
//		setStatus = SCDynamicStoreAddTemporaryValue( mSCDStore, CFSTR("DirectoryService:PID"), aPIDString );
//   		CFRelease(aPIDString);
		
	} // SCDSessionRef okay

	return scdStatus;
	
} // RegisterForNetworkChange


SInt32 SLPSystemConfiguration::UnRegisterForNetworkChange ( void )
{
	SInt32		scdStatus = 0;

	SLP_LOG( SLP_LOG_DEBUG, "UnRegisterForNetworkChange():\n" );
	
	return scdStatus;

} // UnRegisterForNetworkChange

#define kMaxNumRetries	5		// wait up to 10 times for interface name
CFStringRef SLPSystemConfiguration::CopyCurrentActivePrimaryInterfaceName( void )
{
    CFDictionaryRef	dict = NULL;
    CFStringRef		key = NULL;
    CFStringRef		primary = NULL;
    UInt8			retryNum = 0;
    
    key = SCDynamicStoreKeyCreateNetworkGlobalEntity(	NULL, 
                                                        kSCDynamicStoreDomainState, 
                                                        kSCEntNetIPv4);
    if (key == NULL)
        return NULL;
    
    while ( !dict && retryNum < kMaxNumRetries )
    {
        dict = (CFDictionaryRef)SCDynamicStoreCopyValue(mSCRef, key);
        if ( !dict )
        {
            SLP_LOG( SLP_LOG_DROP, "CopyCurrentActivePrimaryInterfaceName, coudn't get the interface dictionary, sleep a second and try again\n" );
            sleep(1);
            retryNum++;
        }
    }
    
    CFRelease(key);
    
    if (dict) 
    {
        primary = (CFStringRef)CFDictionaryGetValue(dict, kSCDynamicStorePropNetPrimaryInterface);
        
        if (primary) 
        {
            CFRetain( primary );
        }
    }
    else 
    {
        SLP_LOG( SLP_LOG_DROP, "No primary interface was found!");
    }
    
    if (dict) 
    {
        CFRelease(dict);
    }

    return primary;
}

CFStringRef SLPSystemConfiguration::CopyConfiguredInterfaceToUse( void )
{
    if ( mConfiguredInterfaceToUse )
        CFRetain(mConfiguredInterfaceToUse);
    
    return mConfiguredInterfaceToUse;
}

bool SLPSystemConfiguration::OnlyUsePreConfiguredDAs( void )
{
    return mUseOnlyPreConfiguredDAs;
}

bool SLPSystemConfiguration::ServerScopeSponsoringEnabled( void )
{
    return mServerScopeSponsoringEnabled;
}

UInt32 SLPSystemConfiguration::SizeOfServerScopeSponsorData( void )
{
    return mSizeOfServerScopeSponsorData;
}

const char* SLPSystemConfiguration::GetServerScopeSponsorData( void )
{
    return mServerScopeSponsorData;
}

void SLPSystemConfiguration::CheckIfFirstLaunchSinceReboot( void )
{
    CFDateRef		dateRef = ::CFDateCreate( kCFAllocatorDefault, CFAbsoluteTimeGetCurrent() );
/*!
	@function SCDynamicStoreAddValue
	@discussion Adds the key-value pair to the "dynamic store" if no
		such key already exists.
	@param store The "dynamic store" session.
	@param key The key of the value to add to the "dynamic store".
	@param value The value to add to the "dynamic store".
	@result TRUE if the key was added; FALSE if the key was already
		present in the "dynamic store" or if an error was encountered.
 */
	if ( SCDynamicStoreAddValue( mSCRef, CFSTR("com.apple.slp.timeOfFirstStart"), dateRef ) )
    {
        // this wasn't there before
        if ( SLPGetProperty("com.sun.slp.regfile") )
        {
            char		msg[1200];
            
            sprintf( msg, "rm %s", SLPGetProperty("com.sun.slp.regfile") );
            system( msg );
            
            SLP_LOG( SLP_LOG_DEBUG, "First time up, slp is deleting regfile: %s", SLPGetProperty("com.sun.slp.regfile") );
        }
    }
    else
    {
        SLP_LOG( SLP_LOG_DEBUG, "Not first time up this reboot, ignoring regfile" );
    }

    CFRelease( dateRef );
}

void SLPSystemConfiguration::DeterminePreconfiguredRegistrationScopeInformation( void )
{
    // we had better figure this out then
    SLP_LOG( SLP_LOG_DEBUG, "SLPSystemConfiguration::DeterminePreconfiguredRegistrationScopeInformation" );
    // this data should be gotten in the following priority:
    // 1) Are we specifically configured via DirectoryServices
    // 2) Do we have info from DHCP?
    // 2) Have we been set locally via com.apple.slp.defaultRegistrationScope (lower priority as it is older tech)
    // 3) use DEFAULT
    
    if ( false )
    {
        // skip #1 for now 
        SLP_LOG( SLP_LOG_DEBUG, "Using scope info from DirectoryServices: %s", mEncodedScopeToRegisterIn );      
    }
    else if ( GetRegistrationScopeFromDCHP() )
    {
        // cool, we were able to get it from here
        SLP_LOG( SLP_LOG_DEBUG, "Using scope info from DHCP: %s", mEncodedScopeToRegisterIn );      
    }
    else if ( SLPGetProperty("com.apple.slp.defaultRegistrationScope") )
    {
        SetEncodedScopeToRegisterIn( SLPGetProperty("com.apple.slp.defaultRegistrationScope"), false );
        SLP_LOG( SLP_LOG_DEBUG, "Using scope info from config file: %s", mEncodedScopeToRegisterIn );      
    }
    else
    {
        // ok, we should just use the special "DEFAULT" scope, it doesn't need to be encoded
        SetEncodedScopeToRegisterIn( SLP_DEFAULT_SCOPE, true );
        SLP_LOG( SLP_LOG_DEBUG, "Using default scope info: %s", mEncodedScopeToRegisterIn );      
    }
}

void SLPSystemConfiguration::DeterminePreconfiguredDAAgentInformation( void )
{
    // we had better figure this out then
    SLP_LOG( SLP_LOG_DEBUG, "SLPSystemConfiguration::DeterminePreconfiguredDAAgentInformation" );
    
    // this data should be gotten in the following priority:
    // 1) Are we specifically configured via DirectoryServices
    // 2) Do we have info from DHCP?
    // 2) Have we been set locally via com.apple.slp.defaultRegistrationScope (lower priority as it is older tech)
    // 3) use DEFAULT
    
    if ( false )
    {
        // skip #1 for now       
        SLP_LOG( SLP_LOG_DEBUG, "Using DAAgent info from Directory Services" );      
        mUseOnlyPreConfiguredDAs = true;
    }
    else if ( GetDAAgentInformationFromDCHP() )
    {
        // cool, we were able to get it from here
        SLP_LOG( SLP_LOG_DEBUG, "Using DAAgent info from DHCP" );
        mUseOnlyPreConfiguredDAs = true;  
    }
    else if ( SLPGetProperty("net.slp.DAAddresses") )
    {
        char			*pcDA = NULL, *pcScopes = NULL;
        const char 		*pcDAs = SLPGetProperty("net.slp.DAAddresses");
        char			cDelim;
        int   			iOffset = 0;
    	// if there are any preconfigured DAs, add them to the DA table.
        SLP_LOG( SLP_LOG_DEBUG, "Using DAAgent info from config file" );      

        do {
        
            if ( !pcDAs )
                break;			// just bail
                
            SLPFree(pcDA);     /* clean up after previous iteration */
            pcDA = NULL;
            
            SLPFree(pcScopes);
            pcScopes = NULL;
            
            pcDA = get_next_string("(",pcDAs,&iOffset,&cDelim);
            
             if ( pcDA )
            {
                pcScopes = get_next_string(")",pcDAs,&iOffset,&cDelim);

                SLP_LOG( SLP_LOG_DEBUG, "...Using DAAgent info from config file, da: %s", pcDA );      
               
                if ( !pcScopes )
                {
                    pcScopes = (char*)malloc(1);
                    pcScopes[0] = '\0';
                }
                
                SLP_LOG( SLP_LOG_DEBUG, "...Using DAAgent info from config file, scope: %s", pcScopes );      
            //    if (!pcDA || !pcScopes) { /* eventually we will run out: clean up & exit */
                if (!pcDA) { /* eventually we will run out: clean up & exit */
                SLPFree(pcDA);
                SLPFree(pcScopes);
                break;
                }
                
                /* skip over the ',' between terms */
                SLPFree(get_next_string(",",pcDAs,&iOffset,&cDelim));
            }
            else
            {
                pcDA = get_next_string(",",pcDAs,&iOffset,&cDelim);			// they are perhaps just using a comma delimited list (da1,da2 etc)
                SLP_LOG( SLP_LOG_DEBUG, "...Using DAAgent info from config file, da: %s", pcDA );      
            }
            
        //    sin.sin_addr = get_in_addr_by_name(pcDA);
            if ( pcDA )
            {
                struct in_addr addr = get_in_addr_by_name(pcDA);
                
                SLP_LOG( SLP_LOG_DEBUG, "...Using DAAgent calling LocateAndAddDA for da: %s", pcDA );      
                LocateAndAddDA( addr.s_addr );		// ignore any scope info and talk to the DA itself
                mUseOnlyPreConfiguredDAs = true;
            }
            else
                break;
            /*
            * Choose now as the boot time, as we don't know when the DA actually
            * started.  We will forward all our adverts to it anyway, as we start
            * up.
            */
        //    if (dat_daadvert_in(pdat,sin,pcScopes,SDGetTime()) < 0) {
        //      LOG(SLP_LOG_ERR,"dat_init: could not add a preconfigured DA");
        //    }
        
        } while (1);
    }
}

void SLPSystemConfiguration::DeterminePreconfiguredInterfaceInformation( void )
{
	SLP_LOG( SLP_LOG_DEBUG, "SLPSystemConfiguration::DeterminePreconfiguredInterfaceInformation\n" );
    if ( SLPGetProperty("com.apple.slp.interface") )
    {
        mConfiguredInterfaceToUse = ::CFStringCreateWithCString( NULL, SLPGetProperty("com.apple.slp.interface"), kCFStringEncodingASCII );
        SLP_LOG( SLP_LOG_DEBUG, "Configured to use interface: %s", SLPGetProperty("com.apple.slp.interface") );
    }
}

bool SLPSystemConfiguration::GetRegistrationScopeFromDCHP( void )
{
    CFDictionaryRef			dhcp_info;
    CFDataRef				slp_service_scope;
    
    bool					configDone = false;

    dhcp_info = SCDynamicStoreCopyDHCPInfo(mSCRef, NULL);
    
    if (dhcp_info) 
    {
        slp_service_scope = DHCPInfoGetOptionData(dhcp_info, DHCPTAG_SLP_SERVICE_SCOPE);
    
        if (slp_service_scope) 
        {
            // parse information from the option being careful about network endianness
            ParseScopeListInfoFromDHCPData( slp_service_scope );
            
            configDone = true;
        }
        
        CFRelease(dhcp_info);
    }
    
    return configDone;
}

bool SLPSystemConfiguration::GetDAAgentInformationFromDCHP( void )
{
    CFDictionaryRef			dhcp_info;
    CFDataRef				da_service_scope;
    
    bool					configDone = false;

    dhcp_info = SCDynamicStoreCopyDHCPInfo(mSCRef, NULL);
    
    if (dhcp_info) 
    {
        da_service_scope = DHCPInfoGetOptionData(dhcp_info, DHCPTAG_SLP_DIRECTORY_AGENT);
    
        if (da_service_scope) 
        {
            // parse information from the option being careful about network endianness
            if ( ParseDAAgentInfoFromDHCPData( da_service_scope ) )
                configDone = true;
        }
        
        CFRelease(dhcp_info);
    }
    
    return configDone;
}

typedef struct DAOption {
    u_char			code;
    u_char			length;
    u_char			ignore;
    u_long			daList[];
};

typedef struct ScopeListOption {
    u_char			code;
    u_char			length;
    u_char			ignore;
    u_char			scopeList[];
};

bool SLPSystemConfiguration::ParseDAAgentInfoFromDHCPData( CFDataRef daAgentInfo )
{
    // code is byte 1 is option code (78), byte 2 is length, byte 3 is ignored (used to be manitory bit), a stream of addresses (4bytes each).
    struct DAOption*		daOption = (DAOption*)CFDataGetBytePtr( daAgentInfo );
    bool					daFound = false;
    
    if ( daOption->code == 78 && daOption->length > 6 )
    {
        int			numDAs = (daOption->length - 3)/4;

        for ( int i=0; i<numDAs; i++ )
        {
            SLPDALocator::TheSLPDAL()->LocateAndAddDA( daOption->daList[i] );
            daFound = true;
        }
    }
    
    return daFound;
}

void SLPSystemConfiguration::ParseScopeListInfoFromDHCPData( CFDataRef scopeListInfo )
{
    // code is byte 1 is option code (78), byte 2 is length, byte 3 is ignored (used to be manitory bit), followed by a encoded UTF-8 comma delimited list
    struct ScopeListOption*		scopeListOption = (ScopeListOption*)CFDataGetBytePtr( scopeListInfo );
    
    if ( scopeListOption->code == 79 && scopeListOption->length > 3 )
    {
        char*		scopeListCopy = (char*)calloc( 1, scopeListOption->length - 2 );		// we'll null terminate the end
        
        memcpy( scopeListCopy, scopeListOption->scopeList, scopeListOption->length - 3 );
        
        char*		curPtr = scopeListCopy;
        char*		curScope = curPtr;
        
        while ( curPtr < (char*)scopeListOption + scopeListOption->length )
        {
            if ( *curPtr == ',' || *curPtr == '\0' )
            {
                // delimiter or end
                if ( *curPtr == ',' )
                {
                    *curPtr = '\0';		// make it a terminator
                    
                    SetEncodedScopeToRegisterIn( curScope, true );
                    
                    // now we are only handling single scopes to register within, so we will log an error if someone tries to set more than one
                    if ( curPtr + 1 < (char*)scopeListOption + scopeListOption->length )
                        SLP_LOG( SLP_LOG_ERR, "SLP only accepts one scope to register in, will use first scope %s", mEncodedScopeToRegisterIn );
                        
                    break;
                }
            }
            
            curPtr++;
        }
        
        if ( scopeListCopy )
            free( scopeListCopy );
    }
}


void SLPSystemConfiguration::HandleAppleTalkNotification( void )
{

}

void SLPSystemConfiguration::HandleIPv4Notification( void )
{
	if ( mCurInterface )
		free( mCurInterface );
		
	CalculateOurIPAddress( &mCurIPAddr, &mCurInterface );
}

void SLPSystemConfiguration::HandleInterfaceNotification( void )
{
	if ( mCurInterface )
		free( mCurInterface );

	CalculateOurIPAddress( &mCurIPAddr, &mCurInterface );
}

void SLPSystemConfiguration::HandleDHCPNotification( void )
{

}



boolean_t SLPSystemConfigurationNetworkChangedCallBack(SCDynamicStoreRef session, void *callback_argument)
{                       
    SLPSystemConfiguration*		config = (SLPSystemConfiguration*)callback_argument;
    
    // do nothing by default
	SLP_LOG( SLP_LOG_NOTIFICATIONS, "*****Network Change Detected******\n" );
	CFArrayRef	changedKeys;
	
	changedKeys = SCDynamicStoreCopyNotifiedKeys(session);
	
    for ( CFIndex i = 0; i < ::CFArrayGetCount(changedKeys); i++ )
    {
        if ( CFStringHasSuffix( (CFStringRef)::CFArrayGetValueAtIndex( changedKeys, i ), kSCEntNetAppleTalk ) )
            config->HandleAppleTalkNotification();
        else if ( CFStringHasSuffix( (CFStringRef)::CFArrayGetValueAtIndex( changedKeys, i ), kSCEntNetIPv4 ) )
            config->HandleIPv4Notification();
        else if ( CFStringHasSuffix( (CFStringRef)::CFArrayGetValueAtIndex( changedKeys, i ), kSCEntNetInterface ) )
            config->HandleInterfaceNotification();
        else if ( CFStringHasSuffix( (CFStringRef)::CFArrayGetValueAtIndex( changedKeys, i ), kSCEntNetDHCP ) )
            config->HandleDHCPNotification();
    }
    
    if ( changedKeys )
		::CFRelease( changedKeys );

	return true;				// return whether everything went ok
}// SLPSystemConfigurationNetworkChangedCallBack







