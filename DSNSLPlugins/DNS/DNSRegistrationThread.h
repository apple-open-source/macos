/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
 *  DNSRegistrationThread.h
 *  DSNSLPlugins
 *
 *  Created by Kevin Arnold on Tue Mar 19 2002.
 *  Copyright (c) 2002 Apple Computer. All rights reserved.
 *
 */

#ifndef _DNSRegistrationThread_
#define _DNSRegistrationThread_ 1

#include "CNSLHeaders.h"

class mDNSPlugin;

class DNSRegistrationThread /*: public DSLThread */
{
public:
                            DNSRegistrationThread		(	mDNSPlugin* parentPlugin );
    virtual					~DNSRegistrationThread		();
    
            void			Initialize					( CFRunLoopRef idleRunLoopRef );
//	virtual void*			Run							( void );
            void			Cancel						( void );

            tDirStatus		RegisterHostedServices		( void );
            
            tDirStatus		PerformRegistration			(	CFStringRef nameRef, 
                                                            CFStringRef typeRef,
                                                            CFStringRef domainRef,
                                                            CFStringRef protocolSpecificData,
                                                            CFStringRef portRef,
															CFStringRef* serviceKeyRef );
                                                            
            tDirStatus		PerformDeregistration		( CFDictionaryRef service );
			tDirStatus		PerformDeregistration		( CFStringRef serviceKey );
			
            mDNSPlugin*		GetParentPlugin				( void ) { return mParentPlugin; }
			CFStringRef		GetOurSpecialRegKey			( void ) { return mOurSpecialRegKey; }
private:
    		mDNSPlugin*				mParentPlugin;
            CFRunLoopRef			mRunLoopRef;
            SCDynamicStoreRef		mSCRef;
            CFMutableDictionaryRef	mRegisteredServicesTable;
            CFMutableDictionaryRef	mMachineService;
            Boolean					mCanceled;
			CFStringRef				mOurSpecialRegKey;
};
CFStringRef		CreateComputerNameEthernetString( CFStringRef computerName );
CFStringRef		CreateMacAddressString		( void );
#endif		// #ifndef
