/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#include "FWDebugging.h"
#include <IOKit/sbp2/IOFireWireSBP2UserClient.h>
#include <IOKit/sbp2/IOFireWireSBP2LUN.h>
#include <IOKit/sbp2/IOFireWireSBP2LSIWorkaroundDescriptor.h>
#include <IOKit/IOMessage.h>

OSDefineMetaClassAndStructors(IOFireWireSBP2UserClient, IOUserClient)

IOExternalMethod IOFireWireSBP2UserClient::sMethods[kIOFWSBP2UserClientNumCommands] =
{
    { //    kIOFWSBP2UserClientOpen
        0,
        (IOMethod) &IOFireWireSBP2UserClient::open,
        kIOUCScalarIScalarO,
        0,
        0
    },
    { //    kIOFWSBP2UserClientClose
        0,
        (IOMethod) &IOFireWireSBP2UserClient::close,
        kIOUCScalarIScalarO,
        0,
        0
    },
    { //    kIOFWSBP2UserClientCreateLogin
        0,
        (IOMethod) &IOFireWireSBP2UserClient::createLogin,
        kIOUCScalarIScalarO,
        0,
        1
    },
    { //    kIOFWSBP2UserClientReleaseLogin
        0,
        (IOMethod) &IOFireWireSBP2UserClient::releaseLogin,
        kIOUCScalarIScalarO,
        1,
        0
    },
    { //    kIOFWSBP2UserClientSubmitLogin
        0,
        (IOMethod) &IOFireWireSBP2UserClient::submitLogin,
        kIOUCScalarIScalarO,
        1,
        0
    },
    { //    kIOFWSBP2UserClientSubmitLogout
        0,
        (IOMethod) &IOFireWireSBP2UserClient::submitLogout,
        kIOUCScalarIScalarO,
        1,
        0
    },
    { //    kIOFWSBP2UserClientSetLoginFlags
        0,
        (IOMethod) &IOFireWireSBP2UserClient::setLoginFlags,
        kIOUCScalarIScalarO,
        2,
        0
    },
    { //    kIOFWSBP2UserClientGetMaxCommandBlockSize
        0,
        (IOMethod) &IOFireWireSBP2UserClient::getMaxCommandBlockSize,
        kIOUCScalarIScalarO,
        1,
        1
    },
    { //    kIOFWSBP2UserClientGetLoginID
        0,
        (IOMethod) &IOFireWireSBP2UserClient::getLoginID,
        kIOUCScalarIScalarO,
        1,
        1
    },
    { //    kIOFWSBP2UserClientSetReconnectTime
        0,
        (IOMethod) &IOFireWireSBP2UserClient::setReconnectTime,
        kIOUCScalarIScalarO,
        2,
        0
    },
    { //    kIOFWSBP2UserClientSetMaxPayloadSize
        0,
        (IOMethod) &IOFireWireSBP2UserClient::setMaxPayloadSize,
        kIOUCScalarIScalarO,
        2,
        0
    },
    { //    kIOFWSBP2UserClientCreateORB
        0,
        (IOMethod) &IOFireWireSBP2UserClient::createORB,
        kIOUCScalarIScalarO,
        0,
        1
    },
    { //    kIOFWSBP2UserClientReleaseORB
        0,
        (IOMethod) &IOFireWireSBP2UserClient::releaseORB,
        kIOUCScalarIScalarO,
        1,
        0
    },
    { //    kIOFWSBP2UserClientSubmitORB
        0,
        (IOMethod) &IOFireWireSBP2UserClient::submitORB,
        kIOUCScalarIScalarO,
        1,
        0
    },
    { //    kIOFWSBP2UserClientSetCommandFlags
        0,
        (IOMethod) &IOFireWireSBP2UserClient::setCommandFlags,
        kIOUCScalarIScalarO,
        2,
        0
    },
    { //    kIOFWSBP2UserClientSetMaxORBPayloadSize
        0,
        (IOMethod) &IOFireWireSBP2UserClient::setMaxORBPayloadSize,
        kIOUCScalarIScalarO,
        2,
        0
    },
    { //    kIOFWSBP2UserClientSetCommandTimeout
        0,
        (IOMethod) &IOFireWireSBP2UserClient::setCommandTimeout,
        kIOUCScalarIScalarO,
        2,
        0
    },
    { //    kIOFWSBP2UserClientSetCommandGeneration
        0,
        (IOMethod) &IOFireWireSBP2UserClient::setCommandGeneration,
        kIOUCScalarIScalarO,
        2,
        0
    },
    { //    kIOFWSBP2UserClientSetToDummy
        0,
        (IOMethod) &IOFireWireSBP2UserClient::setToDummy,
        kIOUCScalarIScalarO,
        1,
        0
    },
    { //    kIOFWSBP2UserClientSetCommandBuffersAsRanges
        0,
        (IOMethod) &IOFireWireSBP2UserClient::setCommandBuffersAsRanges,
        kIOUCScalarIScalarO,
        6,
        0
    },
    { //    kIOFWSBP2UserClientReleaseCommandBuffers
        0,
        (IOMethod) &IOFireWireSBP2UserClient::releaseCommandBuffers,
        kIOUCScalarIScalarO,
        1,
        0
    },
    { //    kIOFWSBP2UserClientSetCommandBlock
        0,
        (IOMethod) &IOFireWireSBP2UserClient::setCommandBlock,
        kIOUCScalarIScalarO,
        3,
        0
    },
    { //    kIOFWSBP2UserClientCreateMgmtORB
        0,
        (IOMethod) &IOFireWireSBP2UserClient::createMgmtORB,
        kIOUCScalarIScalarO,
        0,
        1
    },
    { //    kIOFWSBP2UserClientReleaseMgmtORB
        0,
        (IOMethod) &IOFireWireSBP2UserClient::releaseMgmtORB,
        kIOUCScalarIScalarO,
        1,
        0
    },
    { //    kIOFWSBP2UserClientSubmitMgmtORB
        0,
        (IOMethod) &IOFireWireSBP2UserClient::submitMgmtORB,
        kIOUCScalarIScalarO,
        1,
        0
    },
	{ //    kIOFWSBP2UserClientMgmtORBSetCommandFunction
        0,
        (IOMethod) &IOFireWireSBP2UserClient::setMgmtORBCommandFunction,
        kIOUCScalarIScalarO,
        2,
        0
    },
    { //    kIOFWSBP2UserClientMgmtORBSetManageeORB
        0,
        (IOMethod) &IOFireWireSBP2UserClient::setMgmtORBManageeORB,
        kIOUCScalarIScalarO,
        2,
        0
    },
    { //    kIOFWSBP2UserClientMgmtORBSetManageeLogin
        0,
        (IOMethod) &IOFireWireSBP2UserClient::setMgmtORBManageeLogin,
        kIOUCScalarIScalarO,
        2,
        0
    },
    { //    kIOFWSBP2UserClientMgmtORBSetResponseBuffer
        0,
        (IOMethod) &IOFireWireSBP2UserClient::setMgmtORBResponseBuffer,
        kIOUCScalarIScalarO,
        3,
        0
    },
    { //    kIOFWSBP2UserClientLSIWorkaroundSetCommandBuffersAsRanges
        0,
        (IOMethod) &IOFireWireSBP2UserClient::LSIWorkaroundSetCommandBuffersAsRanges,
        kIOUCScalarIScalarO,
        6,
        0
    },
    { //    kIOFWSBP2UserClientMgmtORBLSIWorkaroundSyncBuffersForOutput
        0,
        (IOMethod) &IOFireWireSBP2UserClient::LSIWorkaroundSyncBuffersForOutput,
        kIOUCScalarIScalarO,
        1,
        0
    },
    { //    kIOFWSBP2UserClientMgmtORBLSIWorkaroundSyncBuffersForInput
        0,
        (IOMethod) &IOFireWireSBP2UserClient::LSIWorkaroundSyncBuffersForInput,
        kIOUCScalarIScalarO,
        1,
        0
    },
	{ //    kIOFWSBP2UserClientOpenWithSessionRef
        0,
        (IOMethod) &IOFireWireSBP2UserClient::openWithSessionRef,
        kIOUCScalarIScalarO,
        1,
        0
    },
	{ //    kIOFWSBP2UserClientGetSessionRef
        0,
        (IOMethod) &IOFireWireSBP2UserClient::getSessionRef,
        kIOUCScalarIScalarO,
        0,
        1
    },
	{   //    kIOFWSBP2UserClientRingDoorbell
         0,
         (IOMethod) &IOFireWireSBP2UserClient::ringDoorbell,
         kIOUCScalarIScalarO,
         1,
         0
    },
    {   //    kIOFWSBP2UserClientEnableUnsolicitedStatusEnable
         0,
         (IOMethod) &IOFireWireSBP2UserClient::enableUnsolicitedStatus,
         kIOUCScalarIScalarO,
         1,
         0
    },
    {   //    kIOFWSBP2UserClientSetBusyTimeoutRegisterValue
         0,
         (IOMethod) &IOFireWireSBP2UserClient::setBusyTimeoutRegisterValue,
         kIOUCScalarIScalarO,
         2,
         0
    },
    {   //    kIOFWSBP2UserClientSetORBRefCon
         0,
         (IOMethod) &IOFireWireSBP2UserClient::setORBRefCon,
         kIOUCScalarIScalarO,
         2,
         0
    },
	{   //    kIOFWSBP2UserClientSetPassword
        0,
        (IOMethod) &IOFireWireSBP2UserClient::setPassword,
        kIOUCScalarIScalarO,
        3,
        0
    }
};

IOExternalAsyncMethod IOFireWireSBP2UserClient::sAsyncMethods[kIOFWSBP2UserClientNumAsyncCommands] =
{
    {   //    kIOFWSBP2UserClientSetMessageCallback
        0,
        (IOAsyncMethod) &IOFireWireSBP2UserClient::setMessageCallback,
        kIOUCScalarIScalarO,
        2,
        0
    },
    {   //    kIOFWSBP2UserClientSetLoginCallback
         0,
         (IOAsyncMethod) &IOFireWireSBP2UserClient::setLoginCallback,
         kIOUCScalarIScalarO,
         2,
         0
    },
    {   //    kIOFWSBP2UserClientSetLogoutCallback
         0,
         (IOAsyncMethod) &IOFireWireSBP2UserClient::setLogoutCallback,
         kIOUCScalarIScalarO,
         2,
         0
    },
    {   //    kIOFWSBP2UserClientSetUnsolicitedStatusNotify
         0,
         (IOAsyncMethod) &IOFireWireSBP2UserClient::setUnsolicitedStatusNotify,
         kIOUCScalarIScalarO,
         2,
         0
    },
    {   //    kIOFWSBP2UserClientSetStatusNotify
         0,
         (IOAsyncMethod) &IOFireWireSBP2UserClient::setStatusNotify,
         kIOUCScalarIScalarO,
         2,
         0
    },
    {   //    kIOFWSBP2UserClientSetMgmtORBCallback
         0,
         (IOAsyncMethod) &IOFireWireSBP2UserClient::setMgmtORBCallback,
         kIOUCScalarIScalarO,
         3,
         0
    },
    {   //    kIOFWSBP2UserClientSubmitFetchAgentReset
         0,
         (IOAsyncMethod) &IOFireWireSBP2UserClient::submitFetchAgentReset,
         kIOUCScalarIScalarO,
         3,
         0
    },
    {   //    kIOFWSBP2UserClientSetFetchAgentWriteCompletion
         0,
         (IOAsyncMethod) &IOFireWireSBP2UserClient::setFetchAgentWriteCompletion,
         kIOUCScalarIScalarO,
         2,
         0
    }
};

//////////////////////////////////////////////////////////////////////////////////////////////////

// ctor
//
//

IOFireWireSBP2UserClient *IOFireWireSBP2UserClient::withTask(task_t owningTask)
{
    IOFireWireSBP2UserClient* me = new IOFireWireSBP2UserClient;

    if( me )
    {
        if( !me->init() )
        {
            me->release();
            return NULL;
        }
        me->fTask = owningTask;
    }

    return me;
}

bool IOFireWireSBP2UserClient::init( OSDictionary * dictionary )
{
    bool res = IOService::init( dictionary );

    fOpened = false;
	fStarted = false;
    fLogin = NULL;
    
    fMessageCallbackAsyncRef[0] = 0;
    fLoginCallbackAsyncRef[0] = 0;
    fLogoutCallbackAsyncRef[0] = 0;
    fUnsolicitedStatusNotifyAsyncRef[0] = 0;
    fStatusNotifyAsyncRef[0] = 0;
    fFetchAgentWriteAsyncRef[0] = 0;
	fFetchAgentResetAsyncRef[0] = 0;
	
    return res;
}

bool IOFireWireSBP2UserClient::start( IOService * provider )
{
    FWKLOG(( "IOFireWireSBP2UserClient : starting\n" ));
    
	if( fStarted )
		return false;
		
    fProviderLUN = OSDynamicCast(IOFireWireSBP2LUN, provider);
    if (fProviderLUN == NULL)
        return false;

     if( !IOUserClient::start(provider) )
         return false;
  
	 fStarted = true;
	 
     return true;
}

// target/method lookups
//
//

IOExternalMethod* IOFireWireSBP2UserClient::getTargetAndMethodForIndex(IOService **target, UInt32 index)
{
    if( index >= kIOFWSBP2UserClientNumCommands )
        return NULL;
    else
    {
        *target = this;
        return &sMethods[index];
    }
}

IOExternalAsyncMethod* IOFireWireSBP2UserClient::getAsyncTargetAndMethodForIndex(IOService **target, UInt32 index)
{
    if( index >= kIOFWSBP2UserClientNumAsyncCommands )
       return NULL;
   else
   {
       *target = this;
       return &sAsyncMethods[index];
   }
    return NULL;
}

// clientClose / clientDied
//
//

IOReturn IOFireWireSBP2UserClient::clientClose( void )
{
    FWKLOG(( "IOFireWireSBP2UserClient : clientClose\n" ));
	
    if( fLogin )
    {
		// releasing the login flushes all orbs
        fLogin->release();
        fLogin = NULL;
    }

    if( fOpened )
    {
		// as long as we have the provider open we should not get terminated
		
		IOService * provider = getProvider();
		
		if( provider )
		{
			flushAllManagementORBs();
			
			IOFireWireController * control = (((IOFireWireSBP2LUN*)provider)->getFireWireUnit())->getController();

			// reset bus - aborts orbs
			control->resetBus();
		
			provider->close(this);
        }
		
		fOpened = false;
    }
    
	// from here on we cannot assume our provider is valid
	
	fStarted = false;
	
    terminate( kIOServiceRequired );
	
	return kIOReturnSuccess;
}

IOReturn IOFireWireSBP2UserClient::clientDied( void )
{
    FWKLOG(( "IOFireWireSBP2UserClient : clientDied\n" ));

    return clientClose();
}


//////////////////////////////////////////////////////////////////////////////////////////////////
//
// IOFireWireSBP2LUN
//

IOReturn IOFireWireSBP2UserClient::open
	( void *, void *, void *, void *, void *, void * )
{
    IOReturn status = kIOReturnSuccess;

    FWKLOG(( "IOFireWireSBP2UserClient : open\n" ));

    if( fOpened )
        status = kIOReturnError;

    if( status == kIOReturnSuccess )
    {
        if( fProviderLUN->open(this) )
		{
            fOpened = true;
		}
		else
            status = kIOReturnExclusiveAccess;
    }
    
     return status;
}

IOReturn IOFireWireSBP2UserClient::openWithSessionRef( IOFireWireSessionRef sessionRef, void *, void *, void *, void *, void * )
{
    IOReturn status = kIOReturnSuccess;
	IOService * service = NULL;

    FWKLOG(( "IOFireWireSBP2UserClient : open\n" ));

    if( fOpened || !fProviderLUN->isOpen() )
        status = kIOReturnError;
	
	if( status == kIOReturnSuccess )
	{
		service = OSDynamicCast( IOService, (OSObject*)sessionRef );
		if( service == NULL )
			status = kIOReturnBadArgument;
	}
	
	if( status == kIOReturnSuccess )
	{
		// look for us in provider chain
		while( fProviderLUN != service && service != NULL )
			service = service->getProvider();
		
		// were we found	
		if( service == NULL )
			status = kIOReturnBadArgument;
	}
	
	return status;
}

IOReturn IOFireWireSBP2UserClient::getSessionRef( IOFireWireSessionRef * sessionRef, void *, void *, void *, void *, void * )
{
    IOReturn status = kIOReturnSuccess;

    FWKLOG(( "IOFireWireSBP2UserClient : getSessionRef\n" ));

    if( !fOpened )
        status = kIOReturnError;

    if( status == kIOReturnSuccess )
    {
		*sessionRef = (IOFireWireSessionRef)this;
	}
    
	return status;
}

IOReturn IOFireWireSBP2UserClient::close
	( void *, void *, void *, void *, void *, void * )
{
    FWKLOG(( "IOFireWireSBP2UserClient : close\n" ));
    
    if( fOpened )
    {
        fProviderLUN->close(this);
        fOpened = false;
    }

    return kIOReturnSuccess;
}

// message callback methods
//
//

IOReturn IOFireWireSBP2UserClient::setMessageCallback
	( OSAsyncReference asyncRef, void * refCon, void * callback, void *, void *, void *, void * )
{
    mach_port_t wakePort = (mach_port_t) asyncRef[0];

    IOUserClient::setAsyncReference( asyncRef, wakePort, callback, refCon );
    bcopy( asyncRef, fMessageCallbackAsyncRef, sizeof(OSAsyncReference) );

    return kIOReturnSuccess;
}

IOReturn IOFireWireSBP2UserClient::message( UInt32 type, IOService * provider, void * arg )
{
    IOReturn status = kIOReturnUnsupported;
    UInt32 entries;
    FWSBP2ReconnectParams * params;
    void * args[16];

    FWKLOG(( "IOFireWireSBP2UserClient : message 0x%x, arg 0x%08lx\n", type, arg ));

    status = IOService::message( type, provider, arg );
    if( status == kIOReturnUnsupported )
    {
        switch( type )
        {
			case kIOFWMessageServiceIsRequestingClose:
				args[11] = (void*)kIOFWMessageServiceIsRequestingClose;
                sendAsyncResult( fMessageCallbackAsyncRef, kIOReturnSuccess, args, 12 );
				// for compatibility
				args[11] = (void*)kIOMessageServiceIsRequestingClose;
                sendAsyncResult( fMessageCallbackAsyncRef, kIOReturnSuccess, args, 12 );
                status = kIOReturnSuccess;
                break;

            case kIOMessageServiceIsRequestingClose:
            case kIOMessageServiceIsTerminated:
            case kIOMessageServiceIsSuspended:
            case kIOMessageServiceIsResumed:
                args[11] = (void*)type;
                sendAsyncResult( fMessageCallbackAsyncRef, kIOReturnSuccess, args, 12 );
                status = kIOReturnSuccess;
                break;

            case kIOMessageFWSBP2ReconnectComplete:
            case kIOMessageFWSBP2ReconnectFailed:
                params = (FWSBP2ReconnectParams*)arg;
                entries = params->reconnectStatusBlockLength;
                if( entries > sizeof(FWSBP2StatusBlock) )
                {
                    entries = sizeof(FWSBP2StatusBlock);
                }
                entries /= sizeof( UInt32 );

#if 0
                FWKLOG(( "IOFireWireSBP2UserClient : login 0x%08lx\n", (UInt32)params->login ));
                FWKLOG(( "IOFireWireSBP2UserClient : generation %ld\n", params->generation ));
                FWKLOG(( "IOFireWireSBP2UserClient : status 0x%08x\n", params->status ));

                if( params->reconnectStatusBlock)
                {
                    FWKLOG(( "IOFireWireSBP2UserClient : details 0x%02x sbpStatus 0x%02x orbOffsetHi 0x%04x\n",
                        ((FWSBP2StatusBlock*)params->reconnectStatusBlock)->details,
                        ((FWSBP2StatusBlock*)params->reconnectStatusBlock)->sbpStatus,
                        ((FWSBP2StatusBlock*)params->reconnectStatusBlock)->orbOffsetHi ));
                    FWKLOG(( "IOFireWireSBP2UserClient : orbOffsetLo 0x%08lx\n",
                        ((FWSBP2StatusBlock*)params->reconnectStatusBlock)->orbOffsetLo ));
                }
#endif
                
                if( fMessageCallbackAsyncRef[0] != 0 )
                {
                    UInt32 i;

                    // fill out return parameters
                    args[0] = (void*)params->generation;
                    args[1] = (void*)params->status;
                    args[2] = (void*)(entries * sizeof(UInt32));

                    // load up status block
                    UInt32 * statusBlock = (UInt32 *)params->reconnectStatusBlock;
                    for( i = 0; i < entries; i++ )
                    {
                        if( statusBlock )
                            args[3+i] = (void*)statusBlock[i];
                        else
                            args[3+i] = 0;
                    }
                    args[11] = (void*)type;
                    sendAsyncResult( fMessageCallbackAsyncRef, kIOReturnSuccess, args, 12 );
                }

                status = kIOReturnSuccess;
                break;
              
            default: // default the action to return kIOReturnUnsupported
                break;
        }
    }
    
    return status;
}

/////////////////////////////////////////////////
// IOFireWireSBP2Login


// login callback methods
//
//

IOReturn IOFireWireSBP2UserClient::setLoginCallback
	( OSAsyncReference asyncRef, void * refCon, void * callback, void *, void *, void *, void * )
{
    mach_port_t wakePort = (mach_port_t) asyncRef[0];

    FWKLOG(( "IOFireWireSBP2UserClient : setLoginCallback\n" ));
    
    IOUserClient::setAsyncReference( asyncRef, wakePort, callback, refCon );
    bcopy( asyncRef, fLoginCallbackAsyncRef, sizeof(OSAsyncReference) );

    return kIOReturnSuccess;
}

void IOFireWireSBP2UserClient::staticLoginCallback( void * refCon, FWSBP2LoginCompleteParamsPtr params )
{
    ((IOFireWireSBP2UserClient*)refCon)->loginCallback( params );
}

void IOFireWireSBP2UserClient::loginCallback( FWSBP2LoginCompleteParamsPtr params )
{
    UInt32 entries = params->statusBlockLength;
    if( entries > sizeof(FWSBP2StatusBlock) )
        entries = sizeof(FWSBP2StatusBlock);
    entries /= sizeof( UInt32 );

    FWKLOG(( "IOFireWireSBP2UserClient : loginCompletion\n" ));

#if 0
    FWKLOG(( "IOFireWireSBP2UserClient : login 0x%08lx\n", (UInt32)params->login ));
    FWKLOG(( "IOFireWireSBP2UserClient : generation %ld\n", params->generation ));
    FWKLOG(( "IOFireWireSBP2UserClient : status 0x%08x\n", params->status ));

    if( params->loginResponse )
    {
    FWKLOG(( "IOFireWireSBP2UserClient : length %d login %d\n", params->loginResponse->length,
                                                                                       params->loginResponse->loginID ));
    FWKLOG(( "IOFireWireSBP2UserClient : commandBlockAgentAddressHi 0x%08lx\n",
                                                                    params->loginResponse->commandBlockAgentAddressHi ));
    FWKLOG(( "IOFireWireSBP2UserClient : commandBlockAgentAddressLo 0x%08lx\n",
                                                                    params->loginResponse->commandBlockAgentAddressLo ));
    FWKLOG(( "IOFireWireSBP2UserClient : reserved %d reconnectHold %d\n", params->loginResponse->reserved,
                                                                    params->loginResponse->reconnectHold ));

    }

    if( params->statusBlock )
    {
    FWKLOG(( "IOFireWireSBP2UserClient : details 0x%02x sbpStatus 0x%02x orbOffsetHi 0x%04x\n",
                 ((FWSBP2StatusBlock*)params->statusBlock)->details,
                 ((FWSBP2StatusBlock*)params->statusBlock)->sbpStatus,
                 ((FWSBP2StatusBlock*)params->statusBlock)->orbOffsetHi ));
    FWKLOG(( "IOFireWireSBP2UserClient : orbOffsetLo 0x%08lx\n", ((FWSBP2StatusBlock*)params->statusBlock)->orbOffsetLo ));
    }
#endif
    
    if( fLoginCallbackAsyncRef[0] != 0 )
    {
        void * args[16];
        UInt32 i;
        
        // fill out return parameters
        args[0] = (void*)params->generation;
        args[1] = (void*)params->status;

        // load up loginResponse
        UInt32 * loginResponse = (UInt32 *)params->loginResponse;
        for( i = 0; i < sizeof(FWSBP2LoginResponse) / sizeof(UInt32); i++ )
        {
            if( loginResponse )
                args[2+i] = (void*)loginResponse[i];
            else
                args[2+i] = 0;
        }

        args[6] = (void*)(entries * sizeof(UInt32));
        
        // load up status block
        UInt32 * statusBlock = (UInt32 *)params->statusBlock;
        for( i = 0; i < 8; i++ )
        {
            if( statusBlock && i < entries )
                args[7+i] = (void*)statusBlock[i];
            else
                args[7+i] = 0;
        }

        sendAsyncResult( fLoginCallbackAsyncRef, kIOReturnSuccess, args, 15 );
    }

}

// logout callback methods
//
//

IOReturn IOFireWireSBP2UserClient::setLogoutCallback
	( OSAsyncReference asyncRef, void * refCon, void * callback, void *, void *, void *, void * )
{
    mach_port_t wakePort = (mach_port_t) asyncRef[0];

    FWKLOG(( "IOFireWireSBP2UserClient : setLogoutCallback\n" ));

    IOUserClient::setAsyncReference( asyncRef, wakePort, callback, refCon );
    bcopy( asyncRef, fLogoutCallbackAsyncRef, sizeof(OSAsyncReference) );

    return kIOReturnSuccess;
}

void IOFireWireSBP2UserClient::staticLogoutCallback( void * refCon, FWSBP2LogoutCompleteParamsPtr params )
{
    ((IOFireWireSBP2UserClient*)refCon)->logoutCallback( params );
}

void IOFireWireSBP2UserClient::logoutCallback( FWSBP2LogoutCompleteParamsPtr params )
{
    FWKLOG(( "IOFireWireSBP2UserClient : logoutCompletion\n" ));

    UInt32 entries = params->statusBlockLength;
    if( entries > sizeof(FWSBP2StatusBlock) )
        entries = sizeof(FWSBP2StatusBlock);
    entries /= sizeof( UInt32 );

#if 0
    FWKLOG(( "IOFireWireSBP2UserClient : login 0x%08lx\n", (UInt32)params->login ));
    FWKLOG(( "IOFireWireSBP2UserClient : generation %ld\n", params->generation ));
    FWKLOG(( "IOFireWireSBP2UserClient : status 0x%08x\n", params->status ));

    if( params->statusBlock )
    {
    FWKLOG(( "IOFireWireSBP2UserClient : details 0x%02x sbpStatus 0x%02x orbOffsetHi 0x%04x\n",
              ((FWSBP2StatusBlock*)params->statusBlock)->details,
              ((FWSBP2StatusBlock*)params->statusBlock)->sbpStatus,
              ((FWSBP2StatusBlock*)params->statusBlock)->orbOffsetHi ));
    FWKLOG(( "IOFireWireSBP2UserClient : orbOffsetLo 0x%08lx\n", ((FWSBP2StatusBlock*)params->statusBlock)->orbOffsetLo ));
    }
#endif
    
    if( fLogoutCallbackAsyncRef[0] != 0 )
    {
        void * args[16];
        UInt32 i;

        // fill out return parameters
        args[0] = (void*)params->generation;
        args[1] = (void*)params->status;
        args[2] = (void*)(entries * sizeof(UInt32));

        // load up status block
        UInt32 * statusBlock = (UInt32 *)params->statusBlock;
        for( i = 0; i < entries; i++ )
        {
            if( statusBlock )
                args[3+i] = (void*)statusBlock[i];
            else
                args[3+i] = 0;
        }

        sendAsyncResult( fLogoutCallbackAsyncRef, kIOReturnSuccess, args, 11 );
    }

}

// unsolicited status notify callback
//
//

IOReturn IOFireWireSBP2UserClient::setUnsolicitedStatusNotify
	( OSAsyncReference asyncRef, void * refCon, void * callback, void *, void *, void *, void * )
{
    mach_port_t wakePort = (mach_port_t) asyncRef[0];

    FWKLOG(( "IOFireWireSBP2UserClient : setUnsolicitedStatusNotify\n" ));

    IOUserClient::setAsyncReference( asyncRef, wakePort, callback, refCon );
    bcopy( asyncRef, fUnsolicitedStatusNotifyAsyncRef, sizeof(OSAsyncReference) );

    return kIOReturnSuccess;
}

void IOFireWireSBP2UserClient::staticUnsolicitedNotify( void * refCon, FWSBP2NotifyParams * params )
{
    ((IOFireWireSBP2UserClient*)refCon)->unsolicitedNotify( params );
}

void IOFireWireSBP2UserClient::unsolicitedNotify( FWSBP2NotifyParams * params )
{
    UInt32 entries = params->length;
    if( entries > sizeof(FWSBP2StatusBlock) )
        entries = sizeof(FWSBP2StatusBlock);
    entries /= sizeof( UInt32 );

    FWKLOG(( "IOFireWireSBP2UserClient : unsolicitedNotify\n" ));

#if 0
    FWKLOG(( "IOFireWireSBP2UserClient : notificationEvent 0x%08x\n", params->notificationEvent ));
    FWKLOG(( "IOFireWireSBP2UserClient : generation %ld\n", params->generation ));
#endif

    if( fUnsolicitedStatusNotifyAsyncRef[0] != 0 )
    {
        void * args[16];
        UInt32 i;

        // fill out return parameters
        args[0] = (void*)params->notificationEvent;
        args[1] = (void*)params->generation;
        args[2] = (void*)(entries * sizeof(UInt32));

        // load up status block
        UInt32 * statusBlock = (UInt32 *)params->message;
        for( i = 0; i < 8; i++ )
        {
            if( statusBlock && i < entries )
                args[3+i] = (void*)statusBlock[i];
            else
                args[3+i] = 0;
        }

        sendAsyncResult( fUnsolicitedStatusNotifyAsyncRef, kIOReturnSuccess, args, 11 );
    }
}

// status notify static
//
//

IOReturn IOFireWireSBP2UserClient::setStatusNotify
	( OSAsyncReference asyncRef, void * refCon, void * callback, void *, void *, void *, void * )
{
    mach_port_t wakePort = (mach_port_t) asyncRef[0];

    FWKLOG(( "IOFireWireSBP2UserClient : setStatusNotify\n" ));

    IOUserClient::setAsyncReference( asyncRef, wakePort, callback, refCon );
    bcopy( asyncRef, fStatusNotifyAsyncRef, sizeof(OSAsyncReference) );

    return kIOReturnSuccess;
}

void IOFireWireSBP2UserClient::staticStatusNotify( void * refCon, FWSBP2NotifyParams * params )
{
    FWKLOG(( "IOFireWireSBP2UserClient : staticStatusNotify\n" ));

    ((IOFireWireSBP2UserClient*)refCon)->statusNotify( params );
}

void IOFireWireSBP2UserClient::statusNotify( FWSBP2NotifyParams * params )
{
    UInt32 entries = params->length;
    if( entries > sizeof(FWSBP2StatusBlock) )
        entries = sizeof(FWSBP2StatusBlock);
    entries /= sizeof( UInt32 );

    FWKLOG(( "IOFireWireSBP2UserClient : statusNotify\n" ));

#if 0
    FWKLOG(( "IOFireWireSBP2UserClient : notificationEvent 0x%08x\n", params->notificationEvent ));
    FWKLOG(( "IOFireWireSBP2UserClient : generation %ld\n", params->generation ));
#endif

    if( fStatusNotifyAsyncRef[0] != 0 )
    {
        void * args[16];
        UInt32 i;

        // fill out return parameters
        args[0] = (void*)params->notificationEvent;
        args[1] = (void*)params->generation;
        args[2] = (void*)(entries * sizeof(UInt32));
		args[3] = (void*)((IOFireWireSBP2ORB*)(params->commandObject))->getRefCon();
		
        // load up status block
        UInt32 * statusBlock = (UInt32 *)params->message;
        for( i = 0; i < 8; i++ )
        {
            if( statusBlock && i < entries )
                args[4+i] = (void*)statusBlock[i];
            else
                args[4+i] = 0;
        }

        sendAsyncResult( fStatusNotifyAsyncRef, kIOReturnSuccess, args, 12 );
    }
}

// createLogin / releaseLogin
//
//

IOReturn IOFireWireSBP2UserClient::createLogin
	( IOFireWireSBP2Login ** outLoginRef, void *, void *, void *, void *, void * )
{
    IOReturn status = kIOReturnSuccess;

    if( fLogin )
        status = kIOReturnError;

    if( status == kIOReturnSuccess )
    {
        fLogin = fProviderLUN->createLogin();
        if( !fLogin )
            status = kIOReturnError;
    }

    if( status == kIOReturnSuccess )
    {
        *outLoginRef = fLogin;
    }
    
    if( status == kIOReturnSuccess )
    {
        fLogin->setLoginCompletion( this, staticLoginCallback );
    }

    if( status == kIOReturnSuccess )
    {
        fLogin->setLogoutCompletion( this, staticLogoutCallback );
    }

    if( status == kIOReturnSuccess )
    {
        fLogin->setUnsolicitedStatusNotifyProc( this, staticUnsolicitedNotify );
    }

    if( status == kIOReturnSuccess )
    {
        fLogin->setStatusNotifyProc( this, staticStatusNotify );
    }

	if( status == kIOReturnSuccess )
	{
		fLogin->setFetchAgentWriteCompletion( this, staticFetchAgentWriteComplete );
	}

    return status;
}

IOReturn IOFireWireSBP2UserClient::releaseLogin
	( IOFireWireSBP2Login* loginRef, void *, void *, void *, void *, void * )
{
    IOFireWireSBP2Login * login = OSDynamicCast( IOFireWireSBP2Login, loginRef );
    if( login && fLogin == login )
    {
        fLogin->release();
        fLogin = NULL;
    }
    return kIOReturnSuccess;
}

IOReturn IOFireWireSBP2UserClient::submitLogin
	( IOFireWireSBP2Login * loginRef, void *, void *, void *, void *, void * )
{
    IOReturn status = kIOReturnSuccess;

    FWKLOG(( "IOFireWireSBP2UserClient : submitLogin\n" ));

    IOFireWireSBP2Login * login = OSDynamicCast( IOFireWireSBP2Login, loginRef );
    if( !login || fLogin != login )
        status = kIOReturnError;
    
    if( status == kIOReturnSuccess )
        status = login->submitLogin();

    return status;
}

IOReturn IOFireWireSBP2UserClient::submitLogout
	( IOFireWireSBP2Login * loginRef, void *, void *, void *, void *, void * )
{
    IOReturn status = kIOReturnSuccess;

    FWKLOG(( "IOFireWireSBP2UserClient : submitLogout\n" ));

    IOFireWireSBP2Login * login = OSDynamicCast( IOFireWireSBP2Login, loginRef );
    if( !login || fLogin != login )
      status = kIOReturnError;

    if( status == kIOReturnSuccess )
        status = fLogin->submitLogout();

    return status;
}

IOReturn IOFireWireSBP2UserClient::setLoginFlags
	( IOFireWireSBP2Login * loginRef, UInt32 loginFlags, void *, void *, void *, void * )
{
    IOReturn status = kIOReturnSuccess;

    FWKLOG(( "IOFireWireSBP2UserClient : setLoginFlags : 0x%08lx\n", loginFlags ));

    IOFireWireSBP2Login * login = OSDynamicCast( IOFireWireSBP2Login, loginRef );
    if( !login || fLogin != login )
        status = kIOReturnError;

    if( status == kIOReturnSuccess )
    {
        UInt32 flags = loginFlags;
        fLogin->setLoginFlags( flags );
    }

    return status;
}

IOReturn IOFireWireSBP2UserClient::getMaxCommandBlockSize
	( IOFireWireSBP2Login * loginRef, UInt32 * blockSize, void *, void *, void *, void * )
{
    IOReturn status = kIOReturnSuccess;

    FWKLOG(( "IOFireWireSBP2UserClient : getMaxCommandBlockSize\n" ));

    IOFireWireSBP2Login * login = OSDynamicCast( IOFireWireSBP2Login, loginRef );
    if( !login || fLogin != login )
        status = kIOReturnError;

    if( status == kIOReturnSuccess )
    {
        *blockSize = fLogin->getMaxCommandBlockSize();        
    }

    return status;
}

IOReturn IOFireWireSBP2UserClient::getLoginID
	( IOFireWireSBP2Login * loginRef, UInt32 * loginID, void *, void *, void *, void * )
{
    IOReturn status = kIOReturnSuccess;

    FWKLOG(( "IOFireWireSBP2UserClient : getLoginID\n" ));

    IOFireWireSBP2Login * login = OSDynamicCast( IOFireWireSBP2Login, loginRef );
    if( !login || fLogin != login )
        status = kIOReturnError;

    if( status == kIOReturnSuccess )
    {
        *loginID = fLogin->getLoginID();
    }

    return status;
}

IOReturn IOFireWireSBP2UserClient::setReconnectTime
	( IOFireWireSBP2Login * loginRef, UInt32 time, void *, void *, void *, void * )
{
    IOReturn status = kIOReturnSuccess;

    FWKLOG(( "IOFireWireSBP2UserClient : setReconnectTime = %d\n", time ));

    IOFireWireSBP2Login * login = OSDynamicCast( IOFireWireSBP2Login, loginRef );
    if( !login || fLogin != login )
        status = kIOReturnError;

    if( status == kIOReturnSuccess )
    {
        fLogin->setReconnectTime( time );
    }
    
    return status;
}

IOReturn IOFireWireSBP2UserClient::setMaxPayloadSize
	( IOFireWireSBP2Login * loginRef, UInt32 size, void *, void *, void *, void * )
{
    IOReturn status = kIOReturnSuccess;

    FWKLOG(( "IOFireWireSBP2UserClient : setMaxPayloadSize = %d\n", size ));

    IOFireWireSBP2Login * login = OSDynamicCast( IOFireWireSBP2Login, loginRef );
    if( !login || fLogin != login )
        status = kIOReturnError;

    if( status == kIOReturnSuccess )
    {
        fLogin->setMaxPayloadSize( size );
    }

    return status;
}

// fetch agent reset
//
//

IOReturn IOFireWireSBP2UserClient::submitFetchAgentReset( OSAsyncReference asyncRef, IOFireWireSBP2Login * loginRef,  void * refCon, void * callback, void *, void *, void * )
{
	IOReturn status = kIOReturnSuccess;
	
	mach_port_t wakePort = (mach_port_t) asyncRef[0];

    FWKLOG(( "IOFireWireSBP2UserClient : submitFetchAgentReset\n" ));
		
	IOFireWireSBP2Login * login = OSDynamicCast( IOFireWireSBP2Login, loginRef );
    if( !login || fLogin != login )
        status = kIOReturnError;

	if( status == kIOReturnSuccess )
	{
		IOUserClient::setAsyncReference( asyncRef, wakePort, callback, refCon );
		bcopy( asyncRef, fFetchAgentResetAsyncRef, sizeof(OSAsyncReference) );
	}

	if( status == kIOReturnSuccess )
	{
		login->setFetchAgentResetCompletion( this, staticFetchAgentResetComplete );
		login->submitFetchAgentReset();
	}

    return status;

}

void IOFireWireSBP2UserClient::staticFetchAgentResetComplete( void * refCon, IOReturn status )
{
   ((IOFireWireSBP2UserClient*)refCon)->fetchAgentResetComplete( status );
}

void IOFireWireSBP2UserClient::fetchAgentResetComplete( IOReturn status )
{
	if( fFetchAgentResetAsyncRef[0] != 0 )
    {
        void * args[1];
		args[0] = (void*)status;
        sendAsyncResult( fFetchAgentResetAsyncRef, kIOReturnSuccess, args, 1 );
    }

}

// ringDoorbell
//
//

IOReturn IOFireWireSBP2UserClient::ringDoorbell( IOFireWireSBP2Login * loginRef, void *, void *,  void *, void *, void * )
{
	IOReturn status = kIOReturnSuccess;
	
    FWKLOG(( "IOFireWireSBP2UserClient : ringDoorbell\n" ));
		
	IOFireWireSBP2Login * login = OSDynamicCast( IOFireWireSBP2Login, loginRef );
    if( !login || fLogin != login )
        status = kIOReturnError;

	if( status == kIOReturnSuccess )
	{
		login->ringDoorbell();
	}

    return status;

}

// enableUnsolicitedStatus
//
//

IOReturn IOFireWireSBP2UserClient::enableUnsolicitedStatus( IOFireWireSBP2Login * loginRef, void *, void *,  void *, void *, void * )
{
	IOReturn status = kIOReturnSuccess;
	
    FWKLOG(( "IOFireWireSBP2UserClient : enableUnsolicitedStatus\n" ));
		
	IOFireWireSBP2Login * login = OSDynamicCast( IOFireWireSBP2Login, loginRef );
    if( !login || fLogin != login )
        status = kIOReturnError;

	if( status == kIOReturnSuccess )
	{
		login->enableUnsolicitedStatus();
	}

    return status;

}

// setBusyTimeoutRegisterValue
//
//

IOReturn IOFireWireSBP2UserClient::setBusyTimeoutRegisterValue( IOFireWireSBP2Login * loginRef, UInt32 timeout, void *,  void *, void *, void * )
{
	IOReturn status = kIOReturnSuccess;
	
    FWKLOG(( "IOFireWireSBP2UserClient : setBusyTimeoutRegisterValue\n" ));
		
	IOFireWireSBP2Login * login = OSDynamicCast( IOFireWireSBP2Login, loginRef );
    if( !login || fLogin != login )
        status = kIOReturnError;

	if( status == kIOReturnSuccess )
	{
		login->setBusyTimeoutRegisterValue( timeout );
	}

    return status;
}

// fetch agent write completion
//
//

IOReturn IOFireWireSBP2UserClient::setFetchAgentWriteCompletion( OSAsyncReference asyncRef,  void * refCon, void * callback, void *, void *, void *, void * )
{
	IOReturn status = kIOReturnSuccess;
	
	mach_port_t wakePort = (mach_port_t) asyncRef[0];

    FWKLOG(( "IOFireWireSBP2UserClient : setFetchAgentWriteCompletion\n" ));
		
	if( status == kIOReturnSuccess )
	{
		IOUserClient::setAsyncReference( asyncRef, wakePort, callback, refCon );
		bcopy( asyncRef, fFetchAgentWriteAsyncRef, sizeof(OSAsyncReference) );
	}

    return status;

}

void IOFireWireSBP2UserClient::staticFetchAgentWriteComplete( void * refCon, IOReturn status, IOFireWireSBP2ORB * orb )
{
   ((IOFireWireSBP2UserClient*)refCon)->fetchAgentWriteComplete( status, orb );
}

void IOFireWireSBP2UserClient::fetchAgentWriteComplete( IOReturn status, IOFireWireSBP2ORB * orb )
{
	if( fFetchAgentWriteAsyncRef[0] != 0 )
    {
        void * args[2];
		args[0] = (void*)status;
		args[1] = (void*)orb;
        sendAsyncResult( fFetchAgentWriteAsyncRef, kIOReturnSuccess, args, 2 );
    }

}

// setPassword
//
//

IOReturn IOFireWireSBP2UserClient::setPassword( IOFireWireSBP2Login * loginRef, vm_address_t buffer, 
												IOByteCount length, void *, void *, void * )
{
    IOReturn 				status = kIOReturnSuccess;
    IOMemoryDescriptor *	memory = NULL;

    FWKLOG(( "IOFireWireSBP2UserClient : setPassword\n" ));

    IOFireWireSBP2Login * login = OSDynamicCast( IOFireWireSBP2Login, loginRef );
    if( !login )
      status = kIOReturnError;

    if( status == kIOReturnSuccess )
    {
        memory = IOMemoryDescriptor::withAddress( buffer, length,
												  kIODirectionOutIn,  fTask );
        if( !memory )
            status = kIOReturnNoMemory;
    }
	
	if( status == kIOReturnSuccess )
	{
		status = memory->prepare();
	}

    if( status == kIOReturnSuccess )
    {
        login->setPassword( memory );
    }

    if( memory )
    {
		memory->complete();
	    memory->release();
	}
	
    return status;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//
// IOFireWireSBP2ORB
//

IOReturn IOFireWireSBP2UserClient::createORB
	( IOFireWireSBP2ORB ** outORBRef, void *, void *, void *, void *, void * )
{
    IOReturn status = kIOReturnSuccess;
    IOFireWireSBP2ORB * orb = NULL;
    
    if( !fLogin )
        status = kIOReturnError;

    if( status == kIOReturnSuccess )
    {
        orb = fLogin->createORB();
        if( !orb )
            status = kIOReturnError;
    }

    if( status == kIOReturnSuccess )
    {
        *outORBRef = orb;
    }

    return status;
}

IOReturn IOFireWireSBP2UserClient::releaseORB
	( IOFireWireSBP2ORB * ORBRef, void *, void *, void *, void *, void * )
{
    FWKLOG(( "IOFireWireSBP2UserClient : releaseORB\n" ));
    
    IOFireWireSBP2ORB * orb = OSDynamicCast( IOFireWireSBP2ORB, ORBRef );
    if( orb )
        orb->release();
    
    return kIOReturnSuccess;
}

// submitORB
//
//

IOReturn IOFireWireSBP2UserClient::submitORB
	( IOFireWireSBP2ORB * ORBRef, void *, void *, void *, void *, void * )
{
    IOReturn status = kIOReturnSuccess;

    FWKLOG(( "IOFireWireSBP2UserClient : submitORB\n" ));

    IOFireWireSBP2ORB * orb = OSDynamicCast( IOFireWireSBP2ORB, ORBRef );
    if( !orb )
      status = kIOReturnError;

    if( status == kIOReturnSuccess )
    {
         status = fLogin->submitORB(orb);
    }
    
    return status;
}

// setCommandFlags
//
//

IOReturn IOFireWireSBP2UserClient::setCommandFlags
	( IOFireWireSBP2ORB * ORBRef, UInt32 flags, void *, void *, void *, void * )
{
    IOReturn status = kIOReturnSuccess;

    FWKLOG(( "IOFireWireSBP2UserClient : setCommandFlags\n" ));

    IOFireWireSBP2ORB * orb = OSDynamicCast( IOFireWireSBP2ORB, ORBRef );
    if( !orb )
      status = kIOReturnError;

    if( status == kIOReturnSuccess )
    {
         orb->setCommandFlags( flags );
    }

    return status;
}

// setMaxORBPayloadSize
//
//

IOReturn IOFireWireSBP2UserClient::setMaxORBPayloadSize
	( IOFireWireSBP2ORB * ORBRef, UInt32 maxPayloadSize, void *, void *, void *, void * )
{
    IOReturn status = kIOReturnSuccess;

    FWKLOG(( "IOFireWireSBP2UserClient : setMaxPayloadSize\n" ));

    IOFireWireSBP2ORB * orb = OSDynamicCast( IOFireWireSBP2ORB, ORBRef );
    if( !orb )
      status = kIOReturnError;

    if( status == kIOReturnSuccess )
    {
        orb->setMaxPayloadSize( maxPayloadSize );
    }

    return status;
}

// setORBRefCon
//
//

IOReturn IOFireWireSBP2UserClient::setORBRefCon
	( IOFireWireSBP2ORB * ORBRef, UInt32 refCon, void *, void *, void *, void * )
{
    IOReturn status = kIOReturnSuccess;

    FWKLOG(( "IOFireWireSBP2UserClient : setORBRefCon\n" ));

    IOFireWireSBP2ORB * orb = OSDynamicCast( IOFireWireSBP2ORB, ORBRef );
    if( !orb )
      status = kIOReturnError;

    if( status == kIOReturnSuccess )
    {
        orb->setRefCon( (void*)refCon );
    }

    return status;
}

// setCommandTimeout
//
//

IOReturn IOFireWireSBP2UserClient::setCommandTimeout
	( IOFireWireSBP2ORB * ORBRef, UInt32 timeout, void *, void *, void *, void * )
{
    IOReturn status = kIOReturnSuccess;

    FWKLOG(( "IOFireWireSBP2UserClient : setCommandTimeout\n" ));

    IOFireWireSBP2ORB * orb = OSDynamicCast( IOFireWireSBP2ORB, ORBRef );
    if( !orb )
      status = kIOReturnError;

    if( status == kIOReturnSuccess )
    {
        orb->setCommandTimeout( timeout );
    }

    return status;
}

// setCommandGeneration
//
//

IOReturn IOFireWireSBP2UserClient::setCommandGeneration
	(IOFireWireSBP2ORB * ORBRef, UInt32 gen, void *, void *, void *, void * )
{
    IOReturn status = kIOReturnSuccess;

    FWKLOG(( "IOFireWireSBP2UserClient : setCommandGeneration\n" ));

    IOFireWireSBP2ORB * orb = OSDynamicCast( IOFireWireSBP2ORB, ORBRef );
    if( !orb )
      status = kIOReturnError;

    if( status == kIOReturnSuccess )
    {
        orb->setCommandGeneration( gen );
    }

    return status;
}

// setToDummy
//
//

IOReturn IOFireWireSBP2UserClient::setToDummy
	( IOFireWireSBP2ORB * ORBRef, void *, void *, void *, void *, void * )
{
    IOReturn status = kIOReturnSuccess;

    FWKLOG(( "IOFireWireSBP2UserClient : setToDummy\n" ));

    IOFireWireSBP2ORB * orb = OSDynamicCast( IOFireWireSBP2ORB, ORBRef );
    if( !orb )
      status = kIOReturnError;

    if( status == kIOReturnSuccess )
    {
      //  orb->setToDummy();
    }

    return status;
}

// setCommandBuffersAsRanges
//
//

IOReturn IOFireWireSBP2UserClient::setCommandBuffersAsRanges
	( IOFireWireSBP2ORB* ORBRef, vm_address_t ranges, IOByteCount withCount, 
		IODirection withDirection, IOByteCount offset, IOByteCount length )
{
	IOReturn status = kIOReturnSuccess;
	
 	IOMemoryDescriptor * 	rangeDesc = NULL;
	IOFireWireSBP2ORB * 	orb = NULL;
	IOVirtualRange * 		rangeBytes = NULL;
	
    FWKLOG(( "IOFireWireSBP2UserClient : setCommandBuffersAsRanges\n" ));

	if( status == kIOReturnSuccess )
	{
		orb = OSDynamicCast( IOFireWireSBP2ORB, ORBRef );
		if( !orb )
			status = kIOReturnError;
	}
	
	if( status == kIOReturnSuccess )
	{
		rangeDesc = IOMemoryDescriptor::withAddress( ranges, 
													(sizeof(IOVirtualRange) * withCount),
													 kIODirectionOut, fTask );
		if( !rangeDesc )
            status = kIOReturnNoMemory;
	}
	
	if( status == kIOReturnSuccess )
	{
		status = rangeDesc->prepare();
	}
	
	if( status == kIOReturnSuccess )
	{
		rangeBytes = (IOVirtualRange*)IOMalloc( sizeof(IOVirtualRange) * withCount );
		if( rangeBytes == NULL )
			status = kIOReturnNoMemory;
	}
	
	if( status == kIOReturnSuccess )
	{
		rangeDesc->readBytes( 0, rangeBytes, sizeof(IOVirtualRange) * withCount );
	}
	
    if( status == kIOReturnSuccess )
    {
		status = orb->setCommandBuffersAsRanges(rangeBytes,
                                                withCount,
												kIODirectionOutIn,
												fTask,
												offset,
												length );
    }

	if( rangeBytes )
	{
		IOFree( rangeBytes, sizeof(IOVirtualRange) * withCount );
	}
	    
	if( rangeDesc )
	{
		rangeDesc->complete();
		rangeDesc->release();
	}

    return status;
}

// releaseCommandBuffers
//
//

IOReturn IOFireWireSBP2UserClient::releaseCommandBuffers
	( IOFireWireSBP2ORB * ORBRef, void *, void *, void *, void *, void * )
{
    IOReturn status = kIOReturnSuccess;

    FWKLOG(( "IOFireWireSBP2UserClient : releaseCommandBuffers\n" ));

    IOFireWireSBP2ORB * orb = OSDynamicCast( IOFireWireSBP2ORB, ORBRef );
    if( !orb )
      status = kIOReturnError;

    if( status == kIOReturnSuccess )
    {
        status = orb->releaseCommandBuffers();
    }

    return status;
}

// setCommandBlock
//
//

IOReturn IOFireWireSBP2UserClient::setCommandBlock
	( IOFireWireSBP2ORB * ORBRef, vm_address_t buffer, IOByteCount length, void *, void *, void * )
{
    IOReturn 				status = kIOReturnSuccess;
    IOMemoryDescriptor *	memory = NULL;

    FWKLOG(( "IOFireWireSBP2UserClient : setCommandBlock\n" ));

    IOFireWireSBP2ORB * orb = OSDynamicCast( IOFireWireSBP2ORB, ORBRef );
    if( !orb )
      status = kIOReturnError;

    if( status == kIOReturnSuccess )
    {
        memory = IOMemoryDescriptor::withAddress( buffer, length,
													kIODirectionOutIn,  fTask );
        if( !memory )
            status = kIOReturnNoMemory;
    }
	
	if( status == kIOReturnSuccess )
	{
		status = memory->prepare();
	}

    if( status == kIOReturnSuccess )
    {
        orb->setCommandBlock( memory );
    }

    if( memory )
	{
		memory->complete();
        memory->release();
	}
	
    return status;
}

IOReturn IOFireWireSBP2UserClient::LSIWorkaroundSetCommandBuffersAsRanges
				( IOFireWireSBP2ORB* ORBRef, vm_address_t ranges, IOByteCount withCount, 
						IODirection withDirection, IOByteCount offset, IOByteCount length )
{
	IOReturn status = kIOReturnSuccess;
    
	IOFireWireSBP2ORB * orb = NULL;
	
	IOMemoryDescriptor * 	rangeDesc = NULL;
	IOVirtualRange * 		rangeBytes = NULL;
	IOMemoryDescriptor *	memoryDesc = NULL;
    IOFireWireSBP2LSIWorkaroundDescriptor *	workaroundDesc = NULL;
	
    FWKLOG(( "IOFireWireSBP2UserClient : setCommandBuffersAsRangesWithLSIWorkaround\n" ));

	if( status == kIOReturnSuccess )
	{
		orb = OSDynamicCast( IOFireWireSBP2ORB, ORBRef );
		if( !orb )
			status = kIOReturnError;
	}
	
	if( status == kIOReturnSuccess )
	{
		rangeDesc = IOMemoryDescriptor::withAddress( ranges, sizeof(IOVirtualRange) * withCount,
													 kIODirectionOut, fTask );
		if( !rangeDesc )
            status = kIOReturnNoMemory;
	}
	
	if( status == kIOReturnSuccess )
	{
		status = rangeDesc->prepare();
	}

	if( status == kIOReturnSuccess )
	{
		rangeBytes = (IOVirtualRange*)IOMalloc( sizeof(IOVirtualRange) * withCount );
		if( rangeBytes == NULL )
			status = kIOReturnNoMemory;
	}
	
	if( status == kIOReturnSuccess )
	{
		rangeDesc->readBytes( 0, rangeBytes, sizeof(IOVirtualRange) * withCount );
	}
	
    if( status == kIOReturnSuccess )
    {
		memoryDesc = IOMemoryDescriptor::withRanges( rangeBytes, withCount,
														kIODirectionOutIn, fTask );
        if( !memoryDesc )
            status = kIOReturnNoMemory;
    }

	if( rangeBytes )
	{
		IOFree( rangeBytes, sizeof(IOVirtualRange) * withCount );
	}
	    
	if( rangeDesc )
	{
		rangeDesc->complete();
		rangeDesc->release();
	}
	
    if( status == kIOReturnSuccess )
    {
        status = memoryDesc->prepare();
    }

    if( status == kIOReturnSuccess )
    {
		workaroundDesc = IOFireWireSBP2LSIWorkaroundDescriptor::withDescriptor
													( memoryDesc, offset, length, kIODirectionOutIn ); 	   
		if( !workaroundDesc )
			status = kIOReturnError;
	}
	
	if( status == kIOReturnSuccess )
	{
		status = workaroundDesc->prepare();
	}
	
	if( status == kIOReturnSuccess )
	{
	    status = orb->setCommandBuffers( workaroundDesc, 0, 0 );
    }

	if( workaroundDesc )
	{
		workaroundDesc->release();
		workaroundDesc = NULL;
	}
	
	if( memoryDesc )
	{
		memoryDesc->release();
		memoryDesc = NULL;
	}
	
    return status;

}

IOReturn IOFireWireSBP2UserClient::LSIWorkaroundSyncBuffersForOutput
						( IOFireWireSBP2ORB * ORBRef, void *, void *, void *, void *, void * )
{
	IOReturn status = kIOReturnSuccess;
    
	IOFireWireSBP2ORB * orb = NULL;
	IOFireWireSBP2LSIWorkaroundDescriptor *	workaroundDesc = NULL;
	
    FWKLOG(( "IOFireWireSBP2UserClient : LSIWorkaroundSyncBuffersForOutput\n" ));

	if( status == kIOReturnSuccess )
	{
		orb = OSDynamicCast( IOFireWireSBP2ORB, ORBRef );
		if( !orb )
			status = kIOReturnError;
	}
	
	if( status == kIOReturnSuccess )
	{
		workaroundDesc = OSDynamicCast( IOFireWireSBP2LSIWorkaroundDescriptor,
										orb->getCommandBufferDescriptor() );
		if( !workaroundDesc )
			status = kIOReturnError;
	}
	
	if( status == kIOReturnSuccess )
	{
		workaroundDesc->syncBuffersForOutput();
	}
	
	return status;
}

IOReturn IOFireWireSBP2UserClient::LSIWorkaroundSyncBuffersForInput
					( IOFireWireSBP2ORB * ORBRef, void *, void *, void *, void *, void * )
{
	IOReturn status = kIOReturnSuccess;
    
	IOFireWireSBP2ORB * orb = NULL;
	IOFireWireSBP2LSIWorkaroundDescriptor *	workaroundDesc = NULL;
	
    FWKLOG(( "IOFireWireSBP2UserClient : LSIWorkaroundSyncBuffersForInput\n" ));

	if( status == kIOReturnSuccess )
	{
		orb = OSDynamicCast( IOFireWireSBP2ORB, ORBRef );
		if( !orb )
			status = kIOReturnError;
	}
	
	if( status == kIOReturnSuccess )
	{
		workaroundDesc = OSDynamicCast( IOFireWireSBP2LSIWorkaroundDescriptor,
										orb->getCommandBufferDescriptor() );
		if( !workaroundDesc )
			status = kIOReturnError;
	}
	
	if( status == kIOReturnSuccess )
	{
		workaroundDesc->syncBuffersForInput();
	}
	
	return status;}

/////////////////////////////////////////////////
// IOFireWireSBP2MgmtORB

IOReturn IOFireWireSBP2UserClient::createMgmtORB
	( IOFireWireSBP2ManagementORB ** outORBRef, void *, void *, void *, void *, void * )
{
    IOReturn status = kIOReturnSuccess;
    IOFireWireSBP2ManagementORB * orb = NULL;
    
    if( !fProviderLUN )
        status = kIOReturnError;

    if( status == kIOReturnSuccess )
    {
        orb = fProviderLUN->createManagementORB( this, staticMgmtORBCallback );
        if( !orb )
            status = kIOReturnError;
    }

    if( status == kIOReturnSuccess )
    {
        *outORBRef = orb;
    }

    return status;
}

IOReturn IOFireWireSBP2UserClient::setMgmtORBCallback
	( OSAsyncReference asyncRef, IOFireWireSBP2ManagementORB * ORBRef, void * refCon, 
								void * callback,  void *, void *, void * )
{
    IOReturn 		status = kIOReturnSuccess;
    IOFireWireSBP2ManagementORB * orb;

    FWKLOG(( "IOFireWireSBP2UserClient : setMgmtORBCallback\n" ));

    if( status == kIOReturnSuccess )
    {
        orb = OSDynamicCast( IOFireWireSBP2ManagementORB, ORBRef );
        if( !orb )
            status = kIOReturnError;
    }

    if( status == kIOReturnSuccess )
    {
        mach_port_t wakePort = (mach_port_t) asyncRef[0];
        IOUserClient::setAsyncReference( asyncRef, wakePort, callback, refCon );
        setMgmtORBAsyncCallbackReference( orb, asyncRef );
    }

    return status;
}

IOReturn IOFireWireSBP2UserClient::releaseMgmtORB
	( IOFireWireSBP2ManagementORB * ORBRef, void *, void *, void *, void *, void * )
{
    FWKLOG(( "IOFireWireSBP2UserClient : releaseMgmtORB\n" ));

    IOFireWireSBP2ManagementORB * orb = OSDynamicCast( IOFireWireSBP2ManagementORB, ORBRef );
    if( orb )
        orb->release();
    
    return kIOReturnSuccess;
}

// submitORB
//
//

IOReturn IOFireWireSBP2UserClient::submitMgmtORB
	( IOFireWireSBP2ManagementORB * ORBRef, void *, void *, void *, void *, void * )
{
    IOReturn status = kIOReturnSuccess;

    FWKLOG(( "IOFireWireSBP2UserClient : submitManagementORB\n" ));

    IOFireWireSBP2ManagementORB * orb = OSDynamicCast( IOFireWireSBP2ManagementORB, ORBRef );
    if( !orb )
      status = kIOReturnError;

    if( status == kIOReturnSuccess )
    {
         status = orb->submit();
    }
    
    return status;
}

// setMgmtORBCommandFunction
//
//

IOReturn IOFireWireSBP2UserClient::setMgmtORBCommandFunction
			( IOFireWireSBP2ManagementORB * ORBRef, UInt32 function, void *, void *, void *, void * )
{
   IOReturn status = kIOReturnSuccess;

    FWKLOG(( "IOFireWireSBP2UserClient : setMgmtORBCommandFunction\n" ));

    IOFireWireSBP2ManagementORB * orb = OSDynamicCast
					( IOFireWireSBP2ManagementORB, ORBRef );
    if( !orb )
      status = kIOReturnError;

    if( status == kIOReturnSuccess )
    {
         status = orb->setCommandFunction( function );
    }
    
    return status;
}

// setMgmtORBManageeORB
//
//

IOReturn IOFireWireSBP2UserClient::setMgmtORBManageeORB
			( IOFireWireSBP2ManagementORB * ORBRef, IOFireWireSBP2ORB * orb, void *, 
																void *, void *, void * )
{
   IOReturn status = kIOReturnSuccess;

    FWKLOG(( "IOFireWireSBP2UserClient : setMgmtORBManageeORB\n" ));
	IOFireWireSBP2ManagementORB * mgmtORB;
	IOFireWireSBP2ORB * manageeORB = NULL;
		
	if( status == kIOReturnSuccess )
	{
		mgmtORB = OSDynamicCast( IOFireWireSBP2ManagementORB, ORBRef );
		if( !mgmtORB )
			status = kIOReturnError;
	}
	
	if( status == kIOReturnSuccess )
	{
		manageeORB = OSDynamicCast( IOFireWireSBP2ORB, orb );
		if( !manageeORB )
			status = kIOReturnError;
	}
	
    if( status == kIOReturnSuccess )
    {
         mgmtORB->setManageeCommand( manageeORB );
    }
    
    return status;
}

// setMgmtORBManageeLogin
//
//

IOReturn IOFireWireSBP2UserClient::setMgmtORBManageeLogin
			( IOFireWireSBP2ManagementORB * ORBRef, IOFireWireSBP2Login * login, 
												void *, void *, void *, void * )
{
   IOReturn status = kIOReturnSuccess;

    FWKLOG(( "IOFireWireSBP2UserClient : setMgmtORBManageeLogin\n" ));
	IOFireWireSBP2ManagementORB * orb;
	IOFireWireSBP2Login * manageeLogin = NULL;
		
	if( status == kIOReturnSuccess )
	{
		orb = OSDynamicCast( IOFireWireSBP2ManagementORB, ORBRef );
		if( !orb )
		status = kIOReturnError;
	}
	
	if( status == kIOReturnSuccess )
	{
		manageeLogin = OSDynamicCast( IOFireWireSBP2Login, login );
		if( !manageeLogin )
			status = kIOReturnError;
	}
	
    if( status == kIOReturnSuccess )
    {
         orb->setManageeCommand( manageeLogin );
    }
    
    return status;
}

// setMgmtORBResponseBuffer
//
//

IOReturn IOFireWireSBP2UserClient::setMgmtORBResponseBuffer
			( IOFireWireSBP2ManagementORB * ORBRef, vm_address_t buf, IOByteCount len, 
															void *, void *, void * )
{
	IOReturn status = kIOReturnSuccess;

    FWKLOG(( "IOFireWireSBP2UserClient : setMgmtORBCommandFunction\n" ));

    IOFireWireSBP2ManagementORB * orb = OSDynamicCast
					( IOFireWireSBP2ManagementORB, ORBRef );
    if( !orb )
		status = kIOReturnError;

	if( status == kIOReturnSuccess )
	{
		if( !buf || len == 0 )
		{
			status = orb->setResponseBuffer( NULL );
			return status;
		}
	}
	
	IOMemoryDescriptor * memory = NULL;
	
	if( status == kIOReturnSuccess )
    {
        memory = IOMemoryDescriptor::withAddress
						( buf, len, kIODirectionOutIn, fTask );
        if( !memory )
            status = kIOReturnNoMemory;
    }

	if( status == kIOReturnSuccess )
	{
		status = memory->prepare();
	}
	
    if( status == kIOReturnSuccess )
    {
         status = orb->setResponseBuffer( memory );
    }
    
    return status;
}


// orb callback methods
//
//

void IOFireWireSBP2UserClient::staticMgmtORBCallback
	(void * refCon, IOReturn status, IOFireWireSBP2ManagementORB * orb)
{
    ((IOFireWireSBP2UserClient*)refCon)->mgmtORBCallback( status, orb );
}

void IOFireWireSBP2UserClient::mgmtORBCallback( IOReturn status, IOFireWireSBP2ManagementORB * orb )
{
    FWKLOG(( "IOFireWireSBP2UserClient : mgmtORBCallback\n" ));

    OSAsyncReference asyncRef;
	
	orb->setResponseBuffer( NULL );
    getMgmtORBAsyncCallbackReference( orb, asyncRef );

    if( asyncRef[0] != 0 )
    {
        sendAsyncResult( asyncRef, kIOReturnSuccess, (void**)&status, 1 );
    }
}

///////////////////////////////////////////////////////////////////////////////////////
// friend class wrapper functions

// IOFireWireSBP2ManagementORB friend class wrappers
void IOFireWireSBP2UserClient::flushAllManagementORBs( void )
{ 
	fProviderLUN->flushAllManagementORBs(); 
}

// IOFireWireSBP2MgmtORB friend class wrappers
void IOFireWireSBP2UserClient::setMgmtORBAsyncCallbackReference( IOFireWireSBP2ManagementORB * orb, void * asyncRef )
{ 
	orb->setAsyncCallbackReference( asyncRef ); 
}

void IOFireWireSBP2UserClient::getMgmtORBAsyncCallbackReference( IOFireWireSBP2ManagementORB * orb, void * asyncRef )
{ 
	orb->getAsyncCallbackReference( asyncRef ); 
}

