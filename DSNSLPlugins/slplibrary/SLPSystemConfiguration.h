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
 *  @header SLPSystemConfiguration
 */
 
#include <netinet/in.h>

#include <CoreFoundation/CoreFoundation.h>
#include <SystemConfiguration/SystemConfiguration.h>
#include <SystemConfiguration/SCDynamicStoreCopyDHCPInfo.h>

#include <SystemConfiguration/SCDynamicStorePrivate.h>
#ifndef _SLPSystemConfiguration_
#define _SLPSystemConfiguration_

Boolean IsNetworkSetToTriggerDialup( void );

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
			const char*				mCurInterface;
};

#endif
