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

#ifndef _mDNSPlugin_
#define _mDNSPlugin_ 1

#include "DNSBrowserThread.h"
#include "DNSRegistrationThread.h"
#include "CNSLPlugin.h"

#define kAFPoverTCPServiceType	"afpovertcp"
#define	kUseMachineName			"NSLUseMachineNameForRegistration"

const CFStringRef	kLocalSAFE_CFSTR = CFSTR("local");
const CFStringRef	kLocalDotSAFE_CFSTR = CFSTR("local.");

class mDNSPlugin : public CNSLPlugin
{
public:
                                mDNSPlugin				( void );
                                ~mDNSPlugin				( void );
    
    virtual sInt32				InitPlugin				( void );
			
            Boolean				IsScopeInReturnList		( const char* scope );
            void				AddResult				( const char* url );
    
            uInt32				fSignature;

	virtual sInt32				SetServerIdleRunLoopRef	( CFRunLoopRef idleRunLoopRef );
	virtual	void				ActivateSelf			( void );

    virtual char*				CreateNSLTypeFromRecType( char *inRecType, Boolean* needToFree );
	virtual CFStringRef			CreateRecTypeFromNativeType ( char *inNativeType );

			CFStringRef			GetComputerNameString	( void ) { return mComputerNameRef; }
			CFStringRef			GetComputerMacAddressNameString	( void ) { return mComputerMACAddressNameRef; }
protected:
    virtual CFStringRef			GetBundleIdentifier		( void );
    virtual const char*			GetProtocolPrefixString	( void );		// this is used for top of the node's path "NSL"
    virtual const char*			GetLocalNodeString		( void );		// this is the user's "Local" location
    virtual Boolean 			IsLocalNode				( const char *inNode );
    
    virtual	Boolean				ReadOnlyPlugin			( void ) { return false; }
    virtual	Boolean				IsClientAuthorizedToCreateRecords ( sCreateRecord *inData );

    virtual	sInt32				RegisterService			( tRecordReference recordRef, CFDictionaryRef service );
    virtual	sInt32				DeregisterService		( tRecordReference recordRef, CFDictionaryRef service );

    virtual void				NewNodeLookup			( void );		// this should fire off some threads in the subclass
    virtual Boolean				OKToOpenUnPublishedNode	( const char* parentNodeName );
    virtual void				NewSubNodeLookup		( char* parentNodeName );		
    virtual	void				NewServiceLookup		( char* serviceType, CNSLDirNodeRep* nodeDirRep );
    virtual	sInt32				HandleNetworkTransition	( sHeader *inData );
private:
            char*				mLocalNodeString;		
            char*				mTopLevelContainerName;
            char*				mServiceTypeString;
            char*				mWorkgroupLookupString;
            char*				mListClassPath;
            DNSBrowserThread*	mLookupThread;
            DNSRegistrationThread* mRegistrationThread;
            CFMutableArrayRef	mListOfServicesToRegisterManually;
			Boolean				mStartedNodeLookups;
			Boolean				mStartedLocalNodeLookups;
			Boolean				mRegisteredHostedServices;
			CFStringRef			mComputerNameRef;
			CFStringRef			mComputerMACAddressNameRef;
};

#endif
