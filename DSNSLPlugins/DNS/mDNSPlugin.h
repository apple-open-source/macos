/*
 *  mDNSPlugin.h
 *
 *  Created by imlucid on Wed Aug 15 2001.
 *  Copyright (c) 2001 Apple Computer. All rights reserved.
 *
 */

#ifndef _mDNSPlugin_
#define _mDNSPlugin_ 1

#include "DNSBrowserThread.h"
#include "DNSRegistrationThread.h"
#include "CNSLPlugin.h"

#define kAFPoverTCPServiceType	"afpovertcp"
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
    virtual char*				CreateNSLTypeFromRecType( char *inRecType );
	virtual CFStringRef			CreateRecTypeFromNativeType ( char *inNativeType );

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

private:
            char*				mLocalNodeString;		
            char*				mTopLevelContainerName;
            char*				mServiceTypeString;
            char*				mWorkgroupLookupString;
            char*				mListClassPath;
            DNSBrowserThread*	mLookupThread;
            DNSRegistrationThread* mRegistrationThread;
            CFMutableArrayRef	mListOfServicesToRegisterManually;
};

#endif


