/*
 *  SLPSystemConfiguration.h
 *  NSLPlugins
 *
 *  Created by imlucid on Fri Sep 21 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
 */
#include <netinet/in.h>

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCDynamicStoreCopyDHCPInfo.h>

#include <SystemConfiguration/SCDynamicStorePrivate.h>
#ifndef _SLPSystemConfiguration_
#define _SLPSystemConfiguration_

class SLPSystemConfiguration
{
public:
                                    SLPSystemConfiguration								( CFRunLoopRef runLoopRef = 0 );
            virtual                 ~SLPSystemConfiguration								();
                        
    static 	SLPSystemConfiguration*	TheSLPSC											( CFRunLoopRef runLoopRef = 0 );			// accessor to the class
    static	void					FreeSLPSC											( void );			
    
            void					Initialize											( void );
            
			int						GetOurIPAdrs										( struct in_addr* ourIPAddr, const char** pcInterf );
			
            SInt32					RegisterForNetworkChange							( void );
            SInt32					UnRegisterForNetworkChange							( void );
            
            CFStringRef             CopyCurrentActivePrimaryInterfaceName				( void );
            CFStringRef				CopyConfiguredInterfaceToUse						( void );
            
            bool					OnlyUsePreConfiguredDAs								( void );
            
            bool					ServerScopeSponsoringEnabled						( void );
            
            UInt32					SizeOfServerScopeSponsorData						( void );
    const	char*					GetServerScopeSponsorData							( void );
            
            void					CheckIfFirstLaunchSinceReboot						( void );
            
    const	char*					GetEncodedScopeToRegisterIn							( void );
            void					SetEncodedScopeToRegisterIn							( const char* scope, bool encodedAlready );
    
   virtual	void					HandleAppleTalkNotification							( void );
   virtual	void					HandleIPv4Notification								( void );
   virtual	void					HandleInterfaceNotification							( void );
   virtual	void					HandleDHCPNotification								( void );
    
protected:
            void					DeterminePreconfiguredRegistrationScopeInformation	( void );
            void					DeterminePreconfiguredDAAgentInformation			( void );
            void					DeterminePreconfiguredInterfaceInformation			( void );
            
            bool					GetRegistrationScopeFromDCHP						( void );
            bool					GetDAAgentInformationFromDCHP						( void );
            
            bool					ParseDAAgentInfoFromDHCPData						( CFDataRef daAgentInfo );
            void					ParseScopeListInfoFromDHCPData						( CFDataRef scopeListInfo );

            void					GetDHCPClientInfo									( void );

    static SLPSystemConfiguration*	msSLPSC;
    
            char*					mEncodedScopeToRegisterIn;		// encoded UTF-8 ready for the wire
            SCDynamicStoreRef		mSCRef;
            CFRunLoopRef			mMainRunLoopRef;
            bool					mUseOnlyPreConfiguredDAs;
            bool					mServerScopeSponsoringEnabled;
            char*					mServerScopeSponsorData;
            UInt32					mSizeOfServerScopeSponsorData;
            CFStringRef				mConfiguredInterfaceToUse;
	struct	in_addr					mCurIPAddr;
			char*					mCurInterface;
};

#endif