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


//////////////////////////////////////////////////////////////////////////////////////////////////

// ctor
//
//

bool IOFireWireSBP2UserClient::initWithTask(
                    task_t owningTask, void * securityToken, UInt32 type,
                    OSDictionary * properties)
{
    if( properties )
		properties->setObject( "IOUserClientCrossEndianCompatible" , kOSBooleanTrue);

    bool res = IOUserClient::initWithTask(owningTask, securityToken, type, properties);
	
    fOpened = false;
	fStarted = false;
    fLogin = NULL;
    fTask = owningTask;
	
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

// externalMethod
//
//

 
IOReturn IOFireWireSBP2UserClient::externalMethod(	uint32_t selector, 
										IOExternalMethodArguments * args,
										IOExternalMethodDispatch * dispatch, 
										OSObject * target, 
										void * reference )
{
	IOReturn status = kIOReturnSuccess;

//	IOLog( "IOFireWireSBP2UserClient::externalMethod - selector = %d, scalarIn = %d, scalarOut = %d structInDesc = 0x%08lx structIn = %d structOutDesc = 0x%08lx structOut = %d\n", 
//					selector, args->scalarInputCount, args->scalarOutputCount, args->structureInputDescriptor, args->structureInputSize, args->structureOutputDescriptor, args->structureOutputSize);
	
    switch( selector )
    {
    	case kIOFWSBP2UserClientOpen:
    		status = open( args );
    		break;
    	
    	case kIOFWSBP2UserClientClose:
    		status = close( args );
    		break;
    	
    	case kIOFWSBP2UserClientCreateLogin:
    		status = createLogin( args );
    		break;
    
    	case kIOFWSBP2UserClientReleaseLogin:
    		status = releaseLogin( args );
    		break;
    		
    	case kIOFWSBP2UserClientSubmitLogin:
    		status = submitLogin( args );
    		break;
    		
    	case kIOFWSBP2UserClientSubmitLogout:
    		status = submitLogout( args );
    		break;
    		
  		case kIOFWSBP2UserClientSetLoginFlags:
  			status = setLoginFlags( args );
  			break;
  			
    	case kIOFWSBP2UserClientGetMaxCommandBlockSize:
    		status = getMaxCommandBlockSize( args );
    		break;
    		
    	case kIOFWSBP2UserClientGetLoginID:
    		status = getLoginID( args );
    		break;
    		
    	case kIOFWSBP2UserClientSetReconnectTime:
    		status = setReconnectTime( args );
    		break;
    		
    	case kIOFWSBP2UserClientSetMaxPayloadSize:
    		status = setMaxPayloadSize( args );
    		break;
    		
    	case kIOFWSBP2UserClientCreateORB:
    		status = createORB( args );
    		break;
    		
    	case kIOFWSBP2UserClientReleaseORB:
    		status = releaseORB( args );
    		break;
    		
    	case kIOFWSBP2UserClientSubmitORB:
    		status = submitORB( args );
    		break;
    		
    	case kIOFWSBP2UserClientSetCommandFlags:
    		status = setCommandFlags( args );
    		break;
    		
    	case kIOFWSBP2UserClientSetMaxORBPayloadSize:
    		status = setMaxORBPayloadSize( args );
    		break;
    		
    	case kIOFWSBP2UserClientSetCommandTimeout:
    		status = setCommandTimeout( args );
    		break;
    		
    	case kIOFWSBP2UserClientSetCommandGeneration:
    		status = setCommandGeneration( args );
    		break;
    		
    	case kIOFWSBP2UserClientSetToDummy:
    		status = setToDummy( args );
    		break;
    		
    	case kIOFWSBP2UserClientSetCommandBuffersAsRanges:
    		status = setCommandBuffersAsRanges( args );
    		break;
    		
    	case kIOFWSBP2UserClientReleaseCommandBuffers:
    		status = releaseCommandBuffers( args );
    		break;
    		
    	case kIOFWSBP2UserClientSetCommandBlock:
    		status = setCommandBlock( args );
    		break;
    		
		case kIOFWSBP2UserClientCreateMgmtORB:
			status = createMgmtORB( args );
			break;
			
		case kIOFWSBP2UserClientReleaseMgmtORB:
			status = releaseMgmtORB( args );
			break;
			
		case kIOFWSBP2UserClientSubmitMgmtORB:
			status = submitMgmtORB( args );
			break;
			
		case kIOFWSBP2UserClientMgmtORBSetCommandFunction:
			status = setMgmtORBCommandFunction( args );
			break;
			
		case kIOFWSBP2UserClientMgmtORBSetManageeORB:
			status = setMgmtORBManageeORB( args );
			break;
			
		case kIOFWSBP2UserClientMgmtORBSetManageeLogin:
			status = setMgmtORBManageeLogin( args );
			break;
			
		case kIOFWSBP2UserClientMgmtORBSetResponseBuffer:
			status = setMgmtORBResponseBuffer( args );
			break;
			
		case kIOFWSBP2UserClientLSIWorkaroundSetCommandBuffersAsRanges:
			status = LSIWorkaroundSetCommandBuffersAsRanges( args );
			break;
			
		case kIOFWSBP2UserClientMgmtORBLSIWorkaroundSyncBuffersForOutput:
			status = LSIWorkaroundSyncBuffersForOutput( args );
			break;
			
		case kIOFWSBP2UserClientMgmtORBLSIWorkaroundSyncBuffersForInput:
			status = LSIWorkaroundSyncBuffersForInput( args );
			break;
			
    	case kIOFWSBP2UserClientOpenWithSessionRef:
    		status = openWithSessionRef( args );
    		break;
    		
		case kIOFWSBP2UserClientGetSessionRef:
			status = getSessionRef( args );
			break;
			
		case kIOFWSBP2UserClientRingDoorbell:
			status = ringDoorbell( args );
			break;
			
		case kIOFWSBP2UserClientEnableUnsolicitedStatus:
			status = enableUnsolicitedStatus( args );
			break;
			
		case kIOFWSBP2UserClientSetBusyTimeoutRegisterValue:
			status = setBusyTimeoutRegisterValue( args );
			break;
			
		case kIOFWSBP2UserClientSetORBRefCon:
			status = setORBRefCon( args );
			break;
			
		case kIOFWSBP2UserClientSetPassword:
			status = setPassword( args );
			break;
			
    	case kIOFWSBP2UserClientSetMessageCallback:
    		status = setMessageCallback( args );
    		break;
    		
   		case kIOFWSBP2UserClientSetLoginCallback:
   			status = setLoginCallback( args );
   			break;
   			
    	case kIOFWSBP2UserClientSetLogoutCallback:
    		status = setLogoutCallback( args );
    		break;
    		
    	case kIOFWSBP2UserClientSetUnsolicitedStatusNotify:
    		status = setUnsolicitedStatusNotify( args );
    		break;
    		
    	case kIOFWSBP2UserClientSetStatusNotify:
    		status = setStatusNotify( args );
    		break;
    		
		case kIOFWSBP2UserClientSetMgmtORBCallback:
			status = setMgmtORBCallback( args );
			break;
			
		case kIOFWSBP2UserClientSubmitFetchAgentReset:
			status = submitFetchAgentReset( args );
			break;
			
		case kIOFWSBP2UserClientSetFetchAgentWriteCompletion:
			status = setFetchAgentWriteCompletion( args );
			break;
		
		default:
			status = kIOReturnBadArgument;
	}
	
//	IOLog( "IOFireWireSBP2UserClient::externalMethod - selector = %d, status = 0x%08lx\n", selector, status );
	
	return status;
}

// checkArguments
//
//

IOReturn IOFireWireSBP2UserClient::checkArguments( IOExternalMethodArguments * args, 
												uint32_t scalarInCount, uint32_t structInCount, 
    										    uint32_t scalarOutCount, uint32_t structOutCount )
{
	IOReturn status = kIOReturnSuccess;
	
	if( (kIOUCVariableStructureSize != scalarInCount) && (scalarInCount != args->scalarInputCount) )
	{
		status = kIOReturnBadArgument;
		//IOLog( "IOFireWireSBP2UserClient::checkArguments - (1) failed\n" );
	}

	if( (kIOUCVariableStructureSize != structInCount) 
		&& (structInCount != ((args->structureInputDescriptor) 
				? args->structureInputDescriptor->getLength() : args->structureInputSize)) )
	{
		status = kIOReturnBadArgument;
		//IOLog( "IOFireWireSBP2UserClient::checkArguments - (2) failed\n" );
	}

	if ((kIOUCVariableStructureSize != scalarOutCount) && (scalarOutCount != args->scalarOutputCount))
	{
		status = kIOReturnBadArgument;
		//IOLog( "IOFireWireSBP2UserClient::checkArguments - (3) failed\n" );
	}

	if ((kIOUCVariableStructureSize != structOutCount) 
		&& (structOutCount != ((args->structureOutputDescriptor) 
				? args->structureOutputDescriptor->getLength() : args->structureOutputSize)))
	{
		status = kIOReturnBadArgument;
		//IOLog( "IOFireWireSBP2UserClient::checkArguments - (4) failed\n" );
	}
			
	return status;
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

IOReturn IOFireWireSBP2UserClient::open(  IOExternalMethodArguments * arguments )
{
    IOReturn status = kIOReturnSuccess;

	FWKLOG(( "IOFireWireSBP2UserClient : open\n" ));

	status = checkArguments( arguments, 0, 0, 0, 0 );
	
    if( status == kIOReturnSuccess )
    {
    	if( fOpened )
        	status = kIOReturnError;
	}
	
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

IOReturn IOFireWireSBP2UserClient::openWithSessionRef(  IOExternalMethodArguments * arguments )
{
    IOReturn status = checkArguments( arguments, 1, 0, 0, 0 );

	IOService * service = NULL;

    FWKLOG(( "IOFireWireSBP2UserClient : open\n" ));

	if( status == kIOReturnSuccess )
	{
		if( fOpened || !fProviderLUN->isOpen() )
			status = kIOReturnError;
	}
	
	if( status == kIOReturnSuccess )
	{
		service = OSDynamicCast( IOService, (OSObject*)arguments->scalarInput[0] );
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

IOReturn IOFireWireSBP2UserClient::getSessionRef(  IOExternalMethodArguments * arguments )
{
    IOReturn status = checkArguments( arguments, 0, 0, 1, 0 );

    FWKLOG(( "IOFireWireSBP2UserClient : getSessionRef\n" ));

	if( status == kIOReturnSuccess )
	{
		if( !fOpened )
			status = kIOReturnError;
	}
	
    if( status == kIOReturnSuccess )
    {
		arguments->scalarOutput[0] = (uint64_t)this;
	}
    
	return status;
}

IOReturn IOFireWireSBP2UserClient::close(  IOExternalMethodArguments * arguments )
{
    IOReturn status = kIOReturnSuccess;
    
    FWKLOG(( "IOFireWireSBP2UserClient : close\n" ));
    
    status = checkArguments( arguments, 0, 0, 0, 0 );
	
	if( status == kIOReturnSuccess )
	{
		if( fOpened )
		{
			fProviderLUN->close(this);
			fOpened = false;
		}
	}
	
    return status;
}

// message callback methods
//
//

IOReturn IOFireWireSBP2UserClient::setMessageCallback
	(  IOExternalMethodArguments * arguments )
{
	IOReturn status = checkArguments( arguments, 0, 0, 0, 0 );
	
	if( status == kIOReturnSuccess )
	{
		bcopy( arguments->asyncReference, fMessageCallbackAsyncRef, sizeof(OSAsyncReference64) );
	}
	
    return status;
}

IOReturn IOFireWireSBP2UserClient::message( UInt32 type, IOService * provider, void * arg )
{
    IOReturn status = kIOReturnUnsupported;
    UInt32 entries;
    FWSBP2ReconnectParams * params;
    io_user_reference_t args[16];

    FWKLOG(( "IOFireWireSBP2UserClient : message 0x%x, arg 0x%08lx\n", type, arg ));

    status = IOService::message( type, provider, arg );
    if( status == kIOReturnUnsupported )
    {
        switch( type )
        {
			case kIOFWMessageServiceIsRequestingClose:
				args[11] = (io_user_reference_t)kIOFWMessageServiceIsRequestingClose;
                sendAsyncResult64( fMessageCallbackAsyncRef, kIOReturnSuccess, args, 12 );

				// for compatibility
				args[11] = (io_user_reference_t)kIOMessageServiceIsRequestingClose;
                sendAsyncResult64( fMessageCallbackAsyncRef, kIOReturnSuccess, args, 12 );
                status = kIOReturnSuccess;
                break;

            case kIOMessageServiceIsRequestingClose:
            case kIOMessageServiceIsTerminated:
            case kIOMessageServiceIsSuspended:
            case kIOMessageServiceIsResumed:
                args[11] = (io_user_reference_t)type;
                sendAsyncResult64( fMessageCallbackAsyncRef, kIOReturnSuccess, args, 12 );
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
                    args[0] = (io_user_reference_t)params->generation;
                    args[1] = (io_user_reference_t)params->status;
                    args[2] = (io_user_reference_t)(entries * sizeof(UInt64));

                    // load up status block
                    UInt32 * statusBlock = (UInt32 *)params->reconnectStatusBlock;
                    for( i = 0; i < entries; i++ )
                    {
                        if( statusBlock )
                            args[3+i] = (io_user_reference_t)statusBlock[i];
                        else
                            args[3+i] = 0;
                    }
                    args[11] = (io_user_reference_t)type;
                    sendAsyncResult64( fMessageCallbackAsyncRef, kIOReturnSuccess, args, 12 );
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
	(  IOExternalMethodArguments * arguments )
{
	IOReturn status = checkArguments( arguments, 0, 0, 0, 0 );
	
	if( status == kIOReturnSuccess )
	{
		bcopy( arguments->asyncReference, fLoginCallbackAsyncRef, sizeof(OSAsyncReference64) );
	}
	
    return status;
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

#if 1
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
    
//    if( fLoginCallbackAsyncRef[0] != 0 )
    {
        UInt64 args[16];
        UInt32 i;
        
        // fill out return parameters
        args[0] = (UInt64)params->generation;
        args[1] = (UInt64)params->status;

        // load up loginResponse
        UInt32 * loginResponse = (UInt32 *)params->loginResponse;
        for( i = 0; i < sizeof(FWSBP2LoginResponse) / sizeof(UInt32); i++ )
        {
            if( loginResponse )
                args[2+i] = (UInt64)loginResponse[i];
            else
                args[2+i] = 0;
        }

        args[6] = (UInt64)(entries * sizeof(UInt32));
        
        // load up status block
        UInt32 * statusBlock = (UInt32 *)params->statusBlock;
        for( i = 0; i < 8; i++ )
        {
            if( statusBlock && i < entries )
                args[7+i] = (UInt64)statusBlock[i];
            else
                args[7+i] = 0;
        }

        sendAsyncResult64( fLoginCallbackAsyncRef, kIOReturnSuccess, args, 15 );
    }

}

// logout callback methods
//
//

IOReturn IOFireWireSBP2UserClient::setLogoutCallback
	(  IOExternalMethodArguments * arguments )
{
	IOReturn status = checkArguments( arguments, 0, 0, 0, 0 );
	
	if( status == kIOReturnSuccess )
	{
		bcopy( arguments->asyncReference, fLogoutCallbackAsyncRef, sizeof(OSAsyncReference64) );
	}
	
    return status;
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
        UInt64 args[16];
        UInt32 i;

        // fill out return parameters
        args[0] = (UInt64)params->generation;
        args[1] = (UInt64)params->status;
        args[2] = (UInt64)(entries * sizeof(UInt32));

        // load up status block
        UInt32 * statusBlock = (UInt32 *)params->statusBlock;
        for( i = 0; i < entries; i++ )
        {
            if( statusBlock )
                args[3+i] = (UInt64)statusBlock[i];
            else
                args[3+i] = 0;
        }

        sendAsyncResult64( fLogoutCallbackAsyncRef, kIOReturnSuccess, args, 11 );
    }

}

// unsolicited status notify callback
//
//

IOReturn IOFireWireSBP2UserClient::setUnsolicitedStatusNotify
	(  IOExternalMethodArguments * arguments )
{
	IOReturn status = checkArguments( arguments, 0, 0, 0, 0 );
	
    FWKLOG(( "IOFireWireSBP2UserClient : setUnsolicitedStatusNotify\n" ));
	
	if( status == kIOReturnSuccess )
	{
		bcopy( arguments->asyncReference, fUnsolicitedStatusNotifyAsyncRef, sizeof(OSAsyncReference64) );
	}
	
    return status;
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
        UInt64 args[16];
        UInt32 i;

        // fill out return parameters
        args[0] = (UInt64)params->notificationEvent;
        args[1] = (UInt64)params->generation;
        args[2] = (UInt64)(entries * sizeof(UInt32));

        // load up status block
        UInt32 * statusBlock = (UInt32 *)params->message;
        for( i = 0; i < 8; i++ )
        {
            if( statusBlock && i < entries )
                args[3+i] = (UInt32)statusBlock[i];
            else
                args[3+i] = 0;
        }

        sendAsyncResult64( fUnsolicitedStatusNotifyAsyncRef, kIOReturnSuccess, args, 11 );
    }
}

// status notify static
//
//

IOReturn IOFireWireSBP2UserClient::setStatusNotify
	(  IOExternalMethodArguments * arguments )
{
	IOReturn status = checkArguments( arguments, 0, 0, 0, 0 );
	
    FWKLOG(( "IOFireWireSBP2UserClient : setStatusNotify\n" ));

	if( status == kIOReturnSuccess )
	{
		bcopy( arguments->asyncReference, fStatusNotifyAsyncRef, sizeof(OSAsyncReference64) );
	}
	
    return status;
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
        UInt64 args[16];
        UInt32 i;

        // fill out return parameters
        args[0] = (UInt64)params->notificationEvent;
        args[1] = (UInt64)params->generation;
        args[2] = (UInt64)(entries * sizeof(UInt32));
		args[3] = ((IOFireWireSBP2ORB*)(params->commandObject))->getRefCon64();
		
        // load up status block
        UInt32 * statusBlock = (UInt32 *)params->message;
        for( i = 0; i < 8; i++ )
        {
            if( statusBlock && i < entries )
                args[4+i] = (UInt32)statusBlock[i];
            else
                args[4+i] = 0;
        }

        sendAsyncResult64( fStatusNotifyAsyncRef, kIOReturnSuccess, args, 12 );
    }
}

// createLogin / releaseLogin
//
//

IOReturn IOFireWireSBP2UserClient::createLogin
	(  IOExternalMethodArguments * arguments )
{
    IOReturn status = kIOReturnSuccess;

    status = checkArguments( arguments, 0, 0, 1, 0 );
    
    if( status == kIOReturnSuccess )
    {
		if( fLogin )
			status = kIOReturnError;
	}
	
    if( status == kIOReturnSuccess )
    {
        fLogin = fProviderLUN->createLogin();
        if( !fLogin )
            status = kIOReturnError;
    }

    if( status == kIOReturnSuccess )
    {
		arguments->scalarOutput[0] = (uint64_t)fLogin;
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
	(  IOExternalMethodArguments * arguments )
{
    IOReturn status = checkArguments( arguments, 1, 0, 0, 0 );

	if( status == kIOReturnSuccess )
	{
		IOFireWireSBP2Login * login = OSDynamicCast( IOFireWireSBP2Login, (OSObject*)arguments->scalarInput[0] );
		if( login && fLogin == login )
		{
			fLogin->release();
			fLogin = NULL;
		}
	}
	
    return status;
}

IOReturn IOFireWireSBP2UserClient::submitLogin
	(  IOExternalMethodArguments * arguments )
{
    IOReturn status = kIOReturnSuccess;

	FWKLOG(( "IOFireWireSBP2UserClient : submitLogin\n" ));
	
	status = checkArguments( arguments, 1, 0, 0, 0 );

	IOFireWireSBP2Login * login = NULL;
    if( status == kIOReturnSuccess )
    {
    	login = OSDynamicCast( IOFireWireSBP2Login, (OSObject*)arguments->scalarInput[0] );
    	if( !login || fLogin != login )
        	status = kIOReturnError;
    }
    
    if( status == kIOReturnSuccess )
    {
    	status = login->submitLogin();
	}
	
    return status;
}

IOReturn IOFireWireSBP2UserClient::submitLogout
	(  IOExternalMethodArguments * arguments )
{
    IOReturn status = checkArguments( arguments, 1, 0, 0, 0 );

    FWKLOG(( "IOFireWireSBP2UserClient : submitLogout\n" ));

	if( status == kIOReturnSuccess )
	{
		IOFireWireSBP2Login * login = OSDynamicCast( IOFireWireSBP2Login, (OSObject*)arguments->scalarInput[0] );
		if( !login || fLogin != login )
		  status = kIOReturnError;
	}
	
    if( status == kIOReturnSuccess )
        status = fLogin->submitLogout();

    return status;
}

IOReturn IOFireWireSBP2UserClient::setLoginFlags
	(  IOExternalMethodArguments * arguments )
{
	IOReturn status = checkArguments( arguments, 2, 0, 0, 0 );

	if( status == kIOReturnSuccess )
	{
		FWKLOG(( "IOFireWireSBP2UserClient : setLoginFlags : 0x%08lx\n", (UInt32)arguments->scalarInput[1] ));
		
		IOFireWireSBP2Login * login = OSDynamicCast( IOFireWireSBP2Login, (OSObject*)arguments->scalarInput[0] );
		if( !login || fLogin != login )
			status = kIOReturnError;
	}
	
    if( status == kIOReturnSuccess )
    {
        UInt32 flags = (UInt32)arguments->scalarInput[1];
        fLogin->setLoginFlags( flags );
    }

    return status;
}

IOReturn IOFireWireSBP2UserClient::getMaxCommandBlockSize
	(  IOExternalMethodArguments * arguments )
{
	IOReturn status = checkArguments( arguments, 1, 0, 1, 0 );

    FWKLOG(( "IOFireWireSBP2UserClient : getMaxCommandBlockSize\n" ));

	if( status == kIOReturnSuccess )
	{
		IOFireWireSBP2Login * login = OSDynamicCast( IOFireWireSBP2Login, (OSObject*)arguments->scalarInput[0] );
		if( !login || fLogin != login )
			status = kIOReturnError;
	}
	
    if( status == kIOReturnSuccess )
    {
        arguments->scalarOutput[0] = fLogin->getMaxCommandBlockSize();        
    }

    return status;
}

IOReturn IOFireWireSBP2UserClient::getLoginID
	(  IOExternalMethodArguments * arguments )
{
	IOReturn status = checkArguments( arguments, 1, 0, 1, 0 );

    FWKLOG(( "IOFireWireSBP2UserClient : getLoginID\n" ));

	if( status == kIOReturnSuccess )
	{
		IOFireWireSBP2Login * login = OSDynamicCast( IOFireWireSBP2Login, (OSObject*)arguments->scalarInput[0] );
		if( !login || fLogin != login )
			status = kIOReturnError;
	}
	
    if( status == kIOReturnSuccess )
    {
		arguments->scalarOutput[0] = fLogin->getLoginID();
    }

    return status;
}

IOReturn IOFireWireSBP2UserClient::setReconnectTime
	(  IOExternalMethodArguments * arguments )
{
	IOReturn status = checkArguments( arguments, 2, 0, 0, 0 );

	if( status == kIOReturnSuccess )
	{
		FWKLOG(( "IOFireWireSBP2UserClient : setReconnectTime = %d\n", (UInt32)arguments->scalarInput[1] ));
		
		IOFireWireSBP2Login * login = OSDynamicCast( IOFireWireSBP2Login, (OSObject*)arguments->scalarInput[0] );
		if( !login || fLogin != login )
			status = kIOReturnError;
	}
	
    if( status == kIOReturnSuccess )
    {
        fLogin->setReconnectTime( (UInt32)arguments->scalarInput[1] );
    }
    
    return status;
}

IOReturn IOFireWireSBP2UserClient::setMaxPayloadSize
	(  IOExternalMethodArguments * arguments )
{
	IOReturn status = checkArguments( arguments, 2, 0, 0, 0 );

	if( status == kIOReturnSuccess )
	{
		FWKLOG(( "IOFireWireSBP2UserClient : setMaxPayloadSize = %d\n", (UInt32)arguments->scalarInput[1] ));
	
		IOFireWireSBP2Login * login = OSDynamicCast( IOFireWireSBP2Login, (OSObject*)arguments->scalarInput[0] );
		if( !login || fLogin != login )
			status = kIOReturnError;
	}
	
    if( status == kIOReturnSuccess )
    {
        fLogin->setMaxPayloadSize( (UInt32)arguments->scalarInput[1] );
    }

    return status;
}

// fetch agent reset
//
//

IOReturn IOFireWireSBP2UserClient::submitFetchAgentReset(  IOExternalMethodArguments * arguments )
{
	IOReturn status = checkArguments( arguments, 1, 0, 0, 0 );
	
    FWKLOG(( "IOFireWireSBP2UserClient : submitFetchAgentReset\n" ));
	
	IOFireWireSBP2Login * login = NULL;
	if( status == kIOReturnSuccess )
	{
		login = OSDynamicCast( IOFireWireSBP2Login, (OSObject*)arguments->scalarInput[0] );
		if( !login || fLogin != login )
			status = kIOReturnError;
	}
	
	if( status == kIOReturnSuccess )
	{
		bcopy( arguments->asyncReference, fFetchAgentResetAsyncRef, sizeof(OSAsyncReference64) );
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
        UInt64 args[1];
		args[0] = (UInt64)status;
        sendAsyncResult64( fFetchAgentResetAsyncRef, kIOReturnSuccess, args, 1 );
    }

}

// ringDoorbell
//
//

IOReturn IOFireWireSBP2UserClient::ringDoorbell(  IOExternalMethodArguments * arguments )
{
	IOReturn status = checkArguments( arguments, 1, 0, 0, 0 );
	
    FWKLOG(( "IOFireWireSBP2UserClient : ringDoorbell\n" ));
	
	IOFireWireSBP2Login * login = NULL;
	if( status == kIOReturnSuccess )
	{
		login = OSDynamicCast( IOFireWireSBP2Login, (OSObject*)arguments->scalarInput[0] );
		if( !login || fLogin != login )
			status = kIOReturnError;
	}
	
	if( status == kIOReturnSuccess )
	{
		login->ringDoorbell();
	}

    return status;

}

// enableUnsolicitedStatus
//
//

IOReturn IOFireWireSBP2UserClient::enableUnsolicitedStatus(  IOExternalMethodArguments * arguments )
{
	IOReturn status = checkArguments( arguments, 1, 0, 0, 0 );

    FWKLOG(( "IOFireWireSBP2UserClient : enableUnsolicitedStatus\n" ));

	IOFireWireSBP2Login * login = NULL;
	if( status == kIOReturnSuccess )
	{
		login = OSDynamicCast( IOFireWireSBP2Login, (OSObject*)arguments->scalarInput[0] );
		if( !login || fLogin != login )
			status = kIOReturnError;
	}
	
	if( status == kIOReturnSuccess )
	{
		login->enableUnsolicitedStatus();
	}

    return status;

}

// setBusyTimeoutRegisterValue
//
//

IOReturn IOFireWireSBP2UserClient::setBusyTimeoutRegisterValue(  IOExternalMethodArguments * arguments )
{
	IOReturn status = checkArguments( arguments, 2, 0, 0, 0 );

    FWKLOG(( "IOFireWireSBP2UserClient : setBusyTimeoutRegisterValue\n" ));
	
	IOFireWireSBP2Login * login = NULL;
	if( status == kIOReturnSuccess )
	{
		login = OSDynamicCast( IOFireWireSBP2Login, (OSObject*)arguments->scalarInput[0] );
		if( !login || fLogin != login )
			status = kIOReturnError;
	}
	
	if( status == kIOReturnSuccess )
	{
		login->setBusyTimeoutRegisterValue( (UInt32)arguments->scalarInput[1] );
	}

    return status;
}

// fetch agent write completion
//
//

IOReturn IOFireWireSBP2UserClient::setFetchAgentWriteCompletion(  IOExternalMethodArguments * arguments )
{
	IOReturn status = checkArguments( arguments, 0, 0, 0, 0 );
	
	if( status == kIOReturnSuccess )
	{
		bcopy( arguments->asyncReference, fFetchAgentWriteAsyncRef, sizeof(OSAsyncReference64) );
	}
	
    FWKLOG(( "IOFireWireSBP2UserClient : setFetchAgentWriteCompletion\n" ));

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
        uint64_t args[2];
		args[0] = (uint64_t)status;
		args[1] = (uint64_t)orb;
        sendAsyncResult64( fFetchAgentWriteAsyncRef, kIOReturnSuccess, args, 2 );
    }

}

// setPassword
//
//

IOReturn IOFireWireSBP2UserClient::setPassword(  IOExternalMethodArguments * arguments )
{
    IOReturn status = checkArguments( arguments, 3, 0, 0, 0 );

    IOMemoryDescriptor *	memory = NULL;

    FWKLOG(( "IOFireWireSBP2UserClient : setPassword\n" ));

	IOFireWireSBP2Login * login = NULL;
	if( status == kIOReturnSuccess )
	{
		login = OSDynamicCast( IOFireWireSBP2Login, (OSObject*)arguments->scalarInput[0] );
		if( !login )
		  status = kIOReturnError;
	}
	
	mach_vm_address_t buffer = 0;
	mach_vm_size_t length = 0;
	
	if( status == kIOReturnSuccess )
	{
		buffer = arguments->scalarInput[1];
		length = arguments->scalarInput[2];
	}
	
	if( status == kIOReturnSuccess )
    {
		memory = IOMemoryDescriptor::withAddressRange(	buffer, 
														length,
														kIODirectionOutIn, fTask );
        if( !memory )
            status = kIOReturnNoMemory;
    }
	
    if( status == kIOReturnSuccess )
    {
        login->setPassword( memory );
    }

    if( memory )
    {
	    memory->release();
	}
	
    return status;
}

//////////////////////////////////////////////////////////////////////////////////////////////////
//
// IOFireWireSBP2ORB
//

IOReturn IOFireWireSBP2UserClient::createORB
	(  IOExternalMethodArguments * arguments )
{
	IOReturn status = checkArguments( arguments, 0, 0, 1, 0 );
	IOFireWireSBP2ORB * orb = NULL;
    
    if( status == kIOReturnSuccess )
    {
		if( !fLogin )
			status = kIOReturnError;
	}
	
    if( status == kIOReturnSuccess )
    {
        orb = fLogin->createORB();
        if( !orb )
            status = kIOReturnError;
    }

    if( status == kIOReturnSuccess )
    {
        arguments->scalarOutput[0] = (uint64_t)orb;
    }

    return status;
}

IOReturn IOFireWireSBP2UserClient::releaseORB
	(  IOExternalMethodArguments * arguments )
{
	IOReturn status = checkArguments( arguments, 1, 0, 0, 0 );
	
	FWKLOG(( "IOFireWireSBP2UserClient : releaseORB\n" ));
    
    if( status == kIOReturnSuccess )
    {
		IOFireWireSBP2ORB * orb = OSDynamicCast( IOFireWireSBP2ORB, (OSObject*)arguments->scalarInput[0] );
		if( orb )
			orb->release();
	}
	
    return kIOReturnSuccess;
}

// submitORB
//
//

IOReturn IOFireWireSBP2UserClient::submitORB
	(  IOExternalMethodArguments * arguments )
{
	IOReturn status = checkArguments( arguments, 1, 0, 0, 0 );

    FWKLOG(( "IOFireWireSBP2UserClient : submitORB\n" ));

	IOFireWireSBP2ORB * orb = NULL;
	if( status == kIOReturnSuccess )
	{
		orb = OSDynamicCast( IOFireWireSBP2ORB, (OSObject*)arguments->scalarInput[0] );
		if( !orb )
		  status = kIOReturnError;
	}
	
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
	(  IOExternalMethodArguments * arguments )
{
	IOReturn status = checkArguments( arguments, 2, 0, 0, 0 );

    FWKLOG(( "IOFireWireSBP2UserClient : setCommandFlags\n" ));

	IOFireWireSBP2ORB * orb = NULL;
	if( status == kIOReturnSuccess )
	{
		orb = OSDynamicCast( IOFireWireSBP2ORB, (OSObject*)arguments->scalarInput[0] );
		if( !orb )
		  status = kIOReturnError;
	}
	
    if( status == kIOReturnSuccess )
    {
         orb->setCommandFlags( (UInt32)arguments->scalarInput[1] );
    }

    return status;
}

// setMaxORBPayloadSize
//
//

IOReturn IOFireWireSBP2UserClient::setMaxORBPayloadSize
	(  IOExternalMethodArguments * arguments )
{
	IOReturn status = checkArguments( arguments, 2, 0, 0, 0 );
	
    FWKLOG(( "IOFireWireSBP2UserClient : setMaxPayloadSize\n" ));

	IOFireWireSBP2ORB * orb = NULL;
	if( status == kIOReturnSuccess )
	{
		orb = OSDynamicCast( IOFireWireSBP2ORB, (OSObject*)arguments->scalarInput[0] );
		if( !orb )
		  status = kIOReturnError;
	}
	
    if( status == kIOReturnSuccess )
    {
        orb->setMaxPayloadSize( (UInt32)arguments->scalarInput[1] );
    }

    return status;
}

// setORBRefCon
//
//

IOReturn IOFireWireSBP2UserClient::setORBRefCon
	(  IOExternalMethodArguments * arguments )
{
    IOReturn status = checkArguments( arguments, 2, 0, 0, 0 );

    FWKLOG(( "IOFireWireSBP2UserClient : setORBRefCon\n" ));

	IOFireWireSBP2ORB * orb = NULL;
	if( status == kIOReturnSuccess )
	{
		orb = OSDynamicCast( IOFireWireSBP2ORB, (OSObject*)arguments->scalarInput[0] );
		if( !orb )
		  status = kIOReturnError;
	}
	
    if( status == kIOReturnSuccess )
    {
        orb->setRefCon64( arguments->scalarInput[1] );
    }

    return status;
}

// setCommandTimeout
//
//

IOReturn IOFireWireSBP2UserClient::setCommandTimeout
	(  IOExternalMethodArguments * arguments )
{
    IOReturn status = checkArguments( arguments, 2, 0, 0, 0 );

    FWKLOG(( "IOFireWireSBP2UserClient : setCommandTimeout\n" ));

	IOFireWireSBP2ORB * orb = NULL;
	if( status == kIOReturnSuccess )
	{
		orb = OSDynamicCast( IOFireWireSBP2ORB, (OSObject*)arguments->scalarInput[0] );
		if( !orb )
		  status = kIOReturnError;
	}
	
    if( status == kIOReturnSuccess )
    {
        orb->setCommandTimeout( (UInt32)arguments->scalarInput[1] );
    }

    return status;
}

// setCommandGeneration
//
//

IOReturn IOFireWireSBP2UserClient::setCommandGeneration
	(  IOExternalMethodArguments * arguments )
{
    IOReturn status = checkArguments( arguments, 2, 0, 0, 0 );

    FWKLOG(( "IOFireWireSBP2UserClient : setCommandGeneration\n" ));

	IOFireWireSBP2ORB * orb = NULL;
	if( status == kIOReturnSuccess )
	{
		orb = OSDynamicCast( IOFireWireSBP2ORB, (OSObject*)arguments->scalarInput[0] );
		if( !orb )
		  status = kIOReturnError;
	}
	
    if( status == kIOReturnSuccess )
    {
        orb->setCommandGeneration( (UInt32)arguments->scalarInput[1] );
    }

    return status;
}

// setToDummy
//
//

IOReturn IOFireWireSBP2UserClient::setToDummy
	(  IOExternalMethodArguments * arguments )
{
	IOReturn status = checkArguments( arguments, 1, 0, 0, 0 );

    FWKLOG(( "IOFireWireSBP2UserClient : setToDummy\n" ));

	if( status == kIOReturnSuccess )
	{
		IOFireWireSBP2ORB * orb = OSDynamicCast( IOFireWireSBP2ORB, (OSObject*)arguments->scalarInput[0] );
		if( !orb )
		  status = kIOReturnError;
	}
	
    if( status == kIOReturnSuccess )
    {
		//zzz why is this turned off?
		
		// orb->setToDummy();
    }

    return status;
}

// setCommandBuffersAsRanges
//
//

IOReturn IOFireWireSBP2UserClient::setCommandBuffersAsRanges
		(  IOExternalMethodArguments * arguments )
{
	IOReturn status = checkArguments( arguments, 6, 0, 0, 0 );
	
 	IOMemoryDescriptor * 	rangeDesc = NULL;
	IOFireWireSBP2ORB * 	orb = NULL;
	IOAddressRange * 		rangeBytes = NULL;
	
    FWKLOG(( "IOFireWireSBP2UserClient : setCommandBuffersAsRanges\n" ));

	if( status == kIOReturnSuccess )
	{
		orb = OSDynamicCast( IOFireWireSBP2ORB, (OSObject*)arguments->scalarInput[0] );
		if( !orb )
			status = kIOReturnError;
	}
	
	mach_vm_address_t ranges = 0;
	uint64_t withCount = 0;
	uint64_t offset = 0;
	uint64_t length = 0;
	vm_size_t rangeSize = 0;
	
	if( status == kIOReturnSuccess )
	{
		ranges = arguments->scalarInput[1];
		withCount = arguments->scalarInput[2];
	//	IODirection withDirection = (IODirection)arguments->scalarInput[3];
		offset = arguments->scalarInput[4];
		length = arguments->scalarInput[5];
		rangeSize = sizeof(IOAddressRange) * withCount;

	//	IOLog( "IOFWSBPUC::setBuf - withCount = %lld length = %lld\n", withCount, length );

	}
	
	if( status == kIOReturnSuccess )
	{
		rangeDesc = IOMemoryDescriptor::withAddressRange(	ranges, 
															rangeSize,
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
		rangeBytes = (IOAddressRange*)IOMalloc( rangeSize );
		if( rangeBytes == NULL )
			status = kIOReturnNoMemory;
	}
	
	if( status == kIOReturnSuccess )
	{
		rangeDesc->readBytes( 0, rangeBytes, rangeSize );
	}
	
	if( status == kIOReturnSuccess )
	{
	#if 0
		for( UInt32 i = 0; i < withCount; i++ )
		{
			IOLog( "IOFWSBPUC::setBuf - %d : addr = 0x%016llx len =  0x%016llx\n", i, rangeBytes[i].address, rangeBytes[i].length );
		}
	#endif
	}
	
    if( status == kIOReturnSuccess )
    {
		status = orb->setCommandBuffersAsRanges64(	rangeBytes,
													withCount,
													kIODirectionOutIn,
													fTask,
													offset,
													length );
    }

	if( rangeBytes )
	{
		IOFree( rangeBytes, rangeSize );
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
	(  IOExternalMethodArguments * arguments )
{
    IOReturn status = checkArguments( arguments, 1, 0, 0, 0 );

    FWKLOG(( "IOFireWireSBP2UserClient : releaseCommandBuffers\n" ));

	IOFireWireSBP2ORB * orb = NULL;
	if( status == kIOReturnSuccess )
	{
		orb = OSDynamicCast( IOFireWireSBP2ORB, (OSObject*)arguments->scalarInput[0] );
		if( !orb )
		  status = kIOReturnError;
	}
	
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
	(  IOExternalMethodArguments * arguments )
{
    IOReturn status = checkArguments( arguments, 3, 0, 0, 0 );

    IOMemoryDescriptor *	memory = NULL;
	
	IOFireWireSBP2ORB * orb = NULL;
	if( status == kIOReturnSuccess )
	{
		FWKLOG(( "IOFireWireSBP2UserClient : setCommandBlock - ORBRef = 0x%08lx buffer = 0x%08lx length = %d\n", (UInt32)arguments->scalarInput[0], (UInt32)arguments->scalarInput[1], (UInt32)arguments->scalarInput[2] ));

		orb = OSDynamicCast( IOFireWireSBP2ORB, (OSObject*)arguments->scalarInput[0] );
		if( !orb )
		  status = kIOReturnError;
	}
	
    if( status == kIOReturnSuccess )
    {
		mach_vm_address_t buffer = arguments->scalarInput[1];
		mach_vm_size_t length = arguments->scalarInput[2];
		memory = IOMemoryDescriptor::withAddressRange(	buffer, 
														length,
														kIODirectionOut, fTask );
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
// setCommandBuffersAsRanges
//
//

IOReturn IOFireWireSBP2UserClient::LSIWorkaroundSetCommandBuffersAsRanges
	(  IOExternalMethodArguments * arguments )
{
	IOReturn status = checkArguments( arguments, 6, 0, 0, 0 );
	
	IOFireWireSBP2ORB * 	orb = NULL;
	
    FWKLOG(( "IOFireWireSBP2UserClient : LSIWorkaroundSetCommandBuffersAsRanges\n" ));

	if( status == kIOReturnSuccess )
	{
		orb = OSDynamicCast( IOFireWireSBP2ORB, (OSObject*)arguments->scalarInput[0] );
		if( !orb )
			status = kIOReturnError;
	}
	
	if( status == kIOReturnSuccess )
	{
		orb->setBufferConstraints( kFWSBP2MaxPageClusterSize, PAGE_SIZE, kFWSBP2ConstraintForceDoubleBuffer );
	}
	
	if( status == kIOReturnSuccess )
	{
		status = setCommandBuffersAsRanges( arguments );
	}
	
    return status;
}


IOReturn IOFireWireSBP2UserClient::LSIWorkaroundSyncBuffersForOutput
						(  IOExternalMethodArguments * arguments )
{
	IOReturn status = checkArguments( arguments, 1, 0, 0, 0 );

	// NOP
	
	return status;
}

IOReturn IOFireWireSBP2UserClient::LSIWorkaroundSyncBuffersForInput
					(  IOExternalMethodArguments * arguments )
{
	IOReturn status = checkArguments( arguments, 1, 0, 0, 0 );


	// NOP
	
	return status;
}

/////////////////////////////////////////////////
// IOFireWireSBP2MgmtORB

IOReturn IOFireWireSBP2UserClient::createMgmtORB
	(  IOExternalMethodArguments * arguments )
{
    IOReturn status = checkArguments( arguments, 0, 0, 1, 0 );

    IOFireWireSBP2ManagementORB * orb = NULL;
    
    if( status == kIOReturnSuccess )
    {
		if( !fProviderLUN )
			status = kIOReturnError;
	}
	
    if( status == kIOReturnSuccess )
    {
        orb = fProviderLUN->createManagementORB( this, staticMgmtORBCallback );
        if( !orb )
            status = kIOReturnError;
    }

    if( status == kIOReturnSuccess )
    {
        arguments->scalarOutput[0] = (uint64_t)orb;
    }

    return status;
}

IOReturn IOFireWireSBP2UserClient::setMgmtORBCallback
	(  IOExternalMethodArguments * arguments )
{
    IOReturn status = checkArguments( arguments, 1, 0, 0, 0 );
    IOFireWireSBP2ManagementORB * orb;

    FWKLOG(( "IOFireWireSBP2UserClient : setMgmtORBCallback\n" ));

    if( status == kIOReturnSuccess )
    {
        orb = OSDynamicCast( IOFireWireSBP2ManagementORB, (OSObject*)arguments->scalarInput[0] );
        if( !orb )
            status = kIOReturnError;
    }

    if( status == kIOReturnSuccess )
    {
		OSAsyncReference64 asyncRef;
		bcopy( arguments->asyncReference, asyncRef, sizeof(OSAsyncReference64) );
        setMgmtORBAsyncCallbackReference( orb, asyncRef );
    }

    return status;
}

IOReturn IOFireWireSBP2UserClient::releaseMgmtORB
	(  IOExternalMethodArguments * arguments )
{
	IOReturn status = checkArguments( arguments, 1, 0, 0, 0 );

    FWKLOG(( "IOFireWireSBP2UserClient : releaseMgmtORB\n" ));

	if( status == kIOReturnSuccess )
	{
		IOFireWireSBP2ManagementORB * orb = OSDynamicCast( IOFireWireSBP2ManagementORB, (OSObject*)arguments->scalarInput[0] );
		if( orb )
			orb->release();
	}
	
    return status;
}

// submitORB
//
//

IOReturn IOFireWireSBP2UserClient::submitMgmtORB
	(  IOExternalMethodArguments * arguments )
{
    IOReturn status = checkArguments( arguments, 1, 0, 0, 0 );

    FWKLOG(( "IOFireWireSBP2UserClient : submitManagementORB\n" ));

	IOFireWireSBP2ManagementORB * orb = NULL;
	if( status == kIOReturnSuccess )
	{
		IOFireWireSBP2ManagementORB * orb = OSDynamicCast( IOFireWireSBP2ManagementORB, (OSObject*)arguments->scalarInput[0] );
		if( !orb )
		  status = kIOReturnError;
	}
	
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
			(  IOExternalMethodArguments * arguments )
{
  	IOReturn status = checkArguments( arguments, 2, 0, 0, 0 );

    FWKLOG(( "IOFireWireSBP2UserClient : setMgmtORBCommandFunction\n" ));

	IOFireWireSBP2ManagementORB * orb = NULL;
	if( status == kIOReturnSuccess )
	{
		orb = OSDynamicCast
						( IOFireWireSBP2ManagementORB, (OSObject*)arguments->scalarInput[0] );
		if( !orb )
		  status = kIOReturnError;
	}
	
    if( status == kIOReturnSuccess )
    {
         status = orb->setCommandFunction( (UInt32)arguments->scalarInput[1] );
    }
    
    return status;
}

// setMgmtORBManageeORB
//
//

IOReturn IOFireWireSBP2UserClient::setMgmtORBManageeORB
			(  IOExternalMethodArguments * arguments )
{
   IOReturn status = checkArguments( arguments, 2, 0, 0, 0 );

    FWKLOG(( "IOFireWireSBP2UserClient : setMgmtORBManageeORB\n" ));
	IOFireWireSBP2ManagementORB * mgmtORB;
	IOFireWireSBP2ORB * manageeORB = NULL;
		
	if( status == kIOReturnSuccess )
	{
		mgmtORB = OSDynamicCast( IOFireWireSBP2ManagementORB, (OSObject*)arguments->scalarInput[0] );
		if( !mgmtORB )
			status = kIOReturnError;
	}
	
	if( status == kIOReturnSuccess )
	{
		manageeORB = OSDynamicCast( IOFireWireSBP2ORB, (OSObject*)arguments->scalarInput[1] );
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
			(  IOExternalMethodArguments * arguments )
{
   IOReturn status = checkArguments( arguments, 2, 0, 0, 0 );


    FWKLOG(( "IOFireWireSBP2UserClient : setMgmtORBManageeLogin\n" ));
	IOFireWireSBP2ManagementORB * orb;
	IOFireWireSBP2Login * manageeLogin = NULL;
		
	if( status == kIOReturnSuccess )
	{
		orb = OSDynamicCast( IOFireWireSBP2ManagementORB, (OSObject*)arguments->scalarInput[0] );
		if( !orb )
		status = kIOReturnError;
	}
	
	if( status == kIOReturnSuccess )
	{
		manageeLogin = OSDynamicCast( IOFireWireSBP2Login, (OSObject*)arguments->scalarInput[1] );
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
			(  IOExternalMethodArguments * arguments )
{
	IOReturn status = checkArguments( arguments, 3, 0, 0, 0 );

    FWKLOG(( "IOFireWireSBP2UserClient : setMgmtORBCommandFunction\n" ));

	IOFireWireSBP2ManagementORB * orb = NULL;
	if( status == kIOReturnSuccess )
	{
		orb = OSDynamicCast
						( IOFireWireSBP2ManagementORB, (OSObject*)arguments->scalarInput[0] );
		if( !orb )
			status = kIOReturnError;
	}
	
	mach_vm_address_t buffer = arguments->scalarInput[1];
	mach_vm_size_t length = arguments->scalarInput[2];

	if( status == kIOReturnSuccess )
	{
		if( !buffer || length == 0 )
		{
			status = orb->setResponseBuffer( NULL );
			return status;
		}
	}
	
	IOMemoryDescriptor * memory = NULL;
	
	if( status == kIOReturnSuccess )
    {
		memory = IOMemoryDescriptor::withAddressRange(	buffer, 
														length,
														kIODirectionOutIn, fTask );
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

    OSAsyncReference64 asyncRef;
	
	orb->setResponseBuffer( NULL );
    getMgmtORBAsyncCallbackReference( orb, asyncRef );

    if( asyncRef[0] != 0 )
    {
		uint64_t args[1];
		args[0] = status;
        sendAsyncResult64( asyncRef, kIOReturnSuccess, args, 1 );
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

