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

            void			RegisterHostedServices		( void );
            CFStringRef		CreateComputerNameEthernetString( CFStringRef computerName );
            CFStringRef		CreateMacAddressString		( void );
            
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
#endif		// #ifndef