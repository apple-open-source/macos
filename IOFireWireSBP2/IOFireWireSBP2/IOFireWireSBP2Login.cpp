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

#include <IOKit/IOMessage.h>

#include <IOKit/sbp2/IOFireWireSBP2Login.h>
#include <IOKit/sbp2/IOFireWireSBP2LUN.h>
#include <IOKit/sbp2/IOFireWireSBP2Target.h>

#define FIREWIREPRIVATE
#include <IOKit/firewire/IOFireWireController.h>
#undef FIREWIREPRIVATE

#include <IOKit/firewire/IOConfigDirectory.h>

#include "FWDebugging.h"
#include "IOFireWireSBP2Diagnostics.h"
#include "IOFWSBP2PseudoAddressSpace.h"

#define kFetchAgentSplitTimeout (3*125*1000)		// 275 milliseconds
#define kFetchAgentRetryInterval (125*1000)			// 125 milliseconds

extern const OSSymbol *gUnit_Characteristics_Symbol;
extern const OSSymbol *gManagement_Agent_Offset_Symbol;
extern const OSSymbol *gFast_Start_Symbol;

OSDefineMetaClassAndStructors( IOFireWireSBP2Login, OSObject );

OSMetaClassDefineReservedUnused(IOFireWireSBP2Login, 0);
OSMetaClassDefineReservedUnused(IOFireWireSBP2Login, 1);
OSMetaClassDefineReservedUnused(IOFireWireSBP2Login, 2);
OSMetaClassDefineReservedUnused(IOFireWireSBP2Login, 3);
OSMetaClassDefineReservedUnused(IOFireWireSBP2Login, 4);
OSMetaClassDefineReservedUnused(IOFireWireSBP2Login, 5);
OSMetaClassDefineReservedUnused(IOFireWireSBP2Login, 6);
OSMetaClassDefineReservedUnused(IOFireWireSBP2Login, 7);
OSMetaClassDefineReservedUnused(IOFireWireSBP2Login, 8);

#if   defined(__BIG_ENDIAN__)
#define SBP2SwapHostToBigInt32Array(array, bytes)
#define SBP2SwapBigToHostInt32Array(array, bytes)
#elif defined(__LITTLE_ENDIAN__)
static void SBP2SwapInt32Array(void * array, int bytes)
{
    if ( array && bytes )
    {
        UInt32 * quads = (UInt32 *) array;
        bytes >>= 2;
        while (bytes--)
            quads[bytes] = OSSwapInt32(quads[bytes]);
    }
}
static inline void SBP2SwapHostToBigInt32Array(void * array, int bytes)
{ SBP2SwapInt32Array(array, bytes); }
static inline void SBP2SwapBigToHostInt32Array(void * array, int bytes)
{ SBP2SwapInt32Array(array, bytes); }
#else
#error Unknown endianess
#endif

#pragma mark -

// initWithLUN
//
// initializer

bool IOFireWireSBP2Login::initWithLUN( IOFireWireSBP2LUN * lun )
{
	IOReturn status = kIOReturnSuccess;
    
    // store LUN & Unit
    fLUN 	= lun;
	fTarget = getTarget();
    fUnit 	= fLUN->getFireWireUnit();
	fControl = fUnit->getController();
     
    fRefCon	= NULL;
	
    fLoginGeneration 		= 0;
	fLoginNodeID			= 0;
    fLoginFlags				= 0;
    fMaxPayloadSize			= 4;
    
    // NULL out fields we may allocate
	fStatusBlockAddressSpace 			= NULL;
	fStatusNotifyCallback				= NULL;
    fStatusNotifyRefCon					= NULL;
    fUnsolicitedStatusNotifyCallback 	= NULL;
    fUnsolicitedStatusNotifyRefCon	 	= NULL;

    fLoginORBAddressSpace 			= NULL;
	fLoginResponseAddressSpace 		= NULL;
	fLoginTimeoutTimerSet			= false;
    fLoginWriteCommand 				= NULL;
    fLoginWriteCommandMemory 		= NULL;
    fLoginWriteInProgress			= false;
	fLoginCompletionCallback 		= NULL;
    fLoginCompletionRefCon 			= NULL;

    fReconnectORBAddressSpace 			= NULL;
    fReconnectStatusBlockAddressSpace 	= NULL;
	fReconnectTime						= 0;
	fReconnectTimeoutTimerSet			= false;
	fReconnectWriteCommand 				= NULL;
    fReconnectWriteCommandMemory 		= NULL;
    fReconnectWriteInProgress 			= false;
    fReconnectWriteInterrupted			= false;
    
	fLogoutORBAddressSpace 			= NULL;
	fLogoutTimeoutTimerSet			= false;
	fLogoutWriteInProgress			= false;
	fLogoutWriteCommand 			= NULL;
    fLogoutWriteCommandMemory 		= NULL;
	fLogoutCompletionCallback		= NULL;
    fLogoutCompletionRefCon			= NULL;

	fLoginRetryMax = 32;
	fLoginRetryDelay = 1000000;

	fUnsolicitedStatusEnableRequested = false;
				
	fFetchAgentWriteCommandInUse 	= false;
	fFetchAgentWriteCommand			= NULL;
    fFetchAgentWriteCommandMemory	= NULL;
	fFetchAgentWriteCompletion 		= NULL;
	
	fFetchAgentResetInProgress		= false;	
	fFetchAgentResetCommand 		= NULL;
	fFetchAgentResetRefCon 			= NULL;
	fFetchAgentResetCompletion		= NULL;

	fDoorbellInProgress		= false;
	fDoorbellRingAgain		= false;	
	fDoorbellCommand 		= NULL;

	fUnsolicitedStatusEnableInProgress		= false;	
	fUnsolicitedStatusEnableCommand 		= NULL;
	
	fSetBusyTimeoutBuffer = OSSwapHostToBigInt32(0x0000000f);
	fSetBusyTimeoutInProgress = false;
	fSetBusyTimeoutAddress = FWAddress( 0x0000FFFF, 0xF0000210 );
	fSetBusyTimeoutCommand = NULL;

    fPasswordBuf = NULL;
    fPasswordLen = 0;
    fPasswordAddressSpace = NULL;
    fPasswordAddress = NULL;
	fPasswordDescriptor = NULL;
	fSuspended = false;
	
	fARDMAMax = 0;
	
	fLastORBAddress = FWAddress(0,0);
	
	//
	// set up command gate
	//
	
	IOWorkLoop * workLoop = NULL;
	if( status == kIOReturnSuccess )
	{
		workLoop = fLUN->getWorkLoop();
		if( !workLoop ) 
			status = kIOReturnNoResources;
	}
	
	if( status == kIOReturnSuccess )
	{
		fGate = IOCommandGate::commandGate( this );
		if( !fGate )
			status = kIOReturnNoMemory;
	}
	
	if( status == kIOReturnSuccess )
	{
		workLoop->retain();
		workLoop->addEventSource( fGate );
	}
	
	if( status == kIOReturnSuccess )
	{
		// init state machine
		fLoginState = kLoginStateIdle;
	
		// scan unit
		status = getUnitInformation();
		FWKLOG( ( "IOFireWireSBP2Login<%p> : fManagementOffset = 0x%lx, fMaxORBSize = %d, fMaxCommandBlockSize = %d\n", this, fManagementOffset, fMaxORBSize, fMaxCommandBlockSize ) );
	}
	
	if( status == kIOReturnSuccess )
	{
		status = allocateResources();
	}

    if( status == kIOReturnSuccess )
    {
        fORBSet = OSSet::withCapacity(1);
		if( fORBSet == NULL )
			status = kIOReturnNoMemory;
	}
	
	if( status == kIOReturnSuccess )
	{
		fORBSetIterator = OSCollectionIterator::withCollection( fORBSet );
    }
	
	if( status == kIOReturnSuccess )
	{		
		OSObject * prop = fLUN->getProperty( "FWARDMAMax", gIOServicePlane );
		if( prop != NULL )
		{
			fARDMAMax = ((OSNumber*)prop)->unsigned32BitValue();
		}
	}

    // the LUN that's creating us will call release() on a non-successful init
    // we'll clean up there from what ever half-allocated state we are in
    
    return (status == kIOReturnSuccess);
}

/////////////////////////////////////////////////////////////////////
// getUnitInformation
//
// gathers SBP2 specific information from unit's ROM
// specifically it reads, the management agent, and unit dependent
// characteristics

IOReturn IOFireWireSBP2Login::getUnitInformation( void )
{
    IOReturn			status = kIOReturnSuccess;
    UInt32				unitCharacteristics = 0;
	OSObject *			prop = NULL;
	
	//
	// get management agent info
	//
	
	prop = fLUN->getProperty( gManagement_Agent_Offset_Symbol );
	if( prop == NULL )
		status = kIOReturnError;
		
	if( status == kIOReturnSuccess )
	{
		fManagementOffset = ((OSNumber*)prop)->unsigned32BitValue();
	}
	
    FWKLOG( ("IOFireWireSBP2Login<%p> : status = %d, fManagementOffset = %d\n", this, status, fManagementOffset) );

    //
    // get unit characteristics
    //
	
	if( status == kIOReturnSuccess )
	{
		prop = fLUN->getProperty( gUnit_Characteristics_Symbol );
		if( prop == NULL )
			status = kIOReturnError;
	}
	
	if( status == kIOReturnSuccess )
	{
		unitCharacteristics = ((OSNumber*)prop)->unsigned32BitValue();
	
        // extract management timeout, max ORB size, max command block size
        
        fManagementTimeout = ((unitCharacteristics >> 8) & 0xff) * 500;   // in milliseconds
		UInt32 orbSize = (unitCharacteristics & 0xff) * 4;
        if( orbSize < 32 )
            orbSize = 32;
        fMaxORBSize = orbSize;
        fMaxCommandBlockSize = orbSize - (sizeof(IOFireWireSBP2ORB::FWSBP2ORB) - 4);
    }
 
	//
	// get fast start info
	//
	
	if( status == kIOReturnSuccess )
	{
		prop = fLUN->getProperty( gFast_Start_Symbol );
		if( prop != NULL )
		{
			UInt32 fastStartInfo = ((OSNumber*)prop)->unsigned32BitValue();
			
			fFastStartSupported = true;
			fFastStartOffset = fastStartInfo & 0x000000ff;
			fFastStartMaxPayload = (fastStartInfo >> 8) & 0x000000ff;
		}
	}

    FWKLOG( ("IOFireWireSBP2Login<%p> : fFastStartSupported = %d, fFastStartOffset = 0x%02lx, fFastStartMaxPayload = %d\n", this, fFastStartSupported, fFastStartOffset, fFastStartMaxPayload ) );
	
    return status;
}

// allocateResources
//
// allocate addressSpaces (ORBs, status registers, response registers)
// and command objects for writing registers, etc.

IOReturn IOFireWireSBP2Login::allocateResources( void )
{
    IOReturn					status = kIOReturnSuccess;

    //
    // allocate and register an address space for the login ORB
    //

	FWAddress host_address;
    if( status == kIOReturnSuccess )
    {
        fLoginORBAddressSpace = IOFWSBP2PseudoAddressSpace::simpleRead( fControl, &host_address, sizeof(FWSBP2LoginORB), &fLoginORB );
    	if ( fLoginORBAddressSpace == NULL )
        	status = kIOReturnNoMemory;
    }

    if( status == kIOReturnSuccess )
    {
		fLoginORBAddress.nodeID = OSSwapHostToBigInt16( host_address.nodeID );
		fLoginORBAddress.addressHi = OSSwapHostToBigInt16( host_address.addressHi );
		fLoginORBAddress.addressLo = OSSwapHostToBigInt32( host_address.addressLo );

        status = fLoginORBAddressSpace->activate();
    }
 
    //
    // allocate and register an address space for the login response
    //

    if( status == kIOReturnSuccess )
    {
        fLoginResponseAddressSpace = IOFWSBP2PseudoAddressSpace::simpleRW( 	fControl, &fLoginResponseAddress,
																			sizeof(FWSBP2LoginResponse), 
																			&fLoginResponse );
        if ( fLoginResponseAddressSpace == NULL )
            status = kIOReturnNoMemory;
    }

    if( status == kIOReturnSuccess )
    {
        status = fLoginResponseAddressSpace->activate();
    }

    //
    // allocate and register an address space for the reconnect ORB
    //

    if( status == kIOReturnSuccess )
    {
        fReconnectORBAddressSpace = IOFWPseudoAddressSpace::simpleRead( fControl, &host_address, 
																		sizeof(FWSBP2ReconnectORB),
                                                                        &fReconnectORB );
    	if ( fReconnectORBAddressSpace == NULL )
            status = kIOReturnNoMemory;
    }

    if( status == kIOReturnSuccess )
    {
        fReconnectORBAddress.nodeID = OSSwapHostToBigInt16( host_address.nodeID );
		fReconnectORBAddress.addressHi = OSSwapHostToBigInt16( host_address.addressHi );
		fReconnectORBAddress.addressLo = OSSwapHostToBigInt32( host_address.addressLo );

        status = fReconnectORBAddressSpace->activate();
    } 

    //
    // allocate and register an address space for the status block
    //

    if( status == kIOReturnSuccess )
    {
        fStatusBlockAddressSpace = fUnit->createPseudoAddressSpace(	&fStatusBlockAddress, 
																	sizeof(FWSBP2StatusBlock),
																	NULL, 
																	IOFireWireSBP2Login::statusBlockWriteStatic,
																	this );
        if ( fStatusBlockAddressSpace == NULL )
            status = kIOReturnNoMemory;
    }

    if( status == kIOReturnSuccess )
    {
        status = fStatusBlockAddressSpace->activate();
    }

    //
    // allocate and register an address space for the reconnect status block
    //

    if( status == kIOReturnSuccess )
    {
        fReconnectStatusBlockAddressSpace =
            fUnit->createPseudoAddressSpace( &fReconnectStatusBlockAddress, sizeof(FWSBP2StatusBlock),
                                             NULL, IOFireWireSBP2Login::reconnectStatusBlockWriteStatic, this );
        
        if ( fReconnectStatusBlockAddressSpace == NULL )
            status = kIOReturnNoMemory;
    }

    if( status == kIOReturnSuccess )
    {
        status = fReconnectStatusBlockAddressSpace->activate();
    }

    //
    // allocate and register an address space for the logout ORB
    //

    if( status == kIOReturnSuccess )
    {
        fLogoutORBAddressSpace = IOFWPseudoAddressSpace::simpleRead( fControl, &host_address, sizeof(FWSBP2LogoutORB),
                                                                     &fLogoutORB );
    	if ( fLogoutORBAddressSpace == NULL )
        	status = kIOReturnNoMemory;
    }
  
    if( status == kIOReturnSuccess )
    {
		fLogoutORBAddress.nodeID = OSSwapHostToBigInt16( host_address.nodeID );
		fLogoutORBAddress.addressHi = OSSwapHostToBigInt16( host_address.addressHi );
		fLogoutORBAddress.addressLo = OSSwapHostToBigInt32( host_address.addressLo );

        status = fLogoutORBAddressSpace->activate();
    }

    //
    // prepare parts of the ORBs
    //
    
    fLoginORB.loginResponseAddressHi = OSSwapHostToBigInt32((fLoginResponseAddress.nodeID << 16) | fLoginResponseAddress.addressHi);
    fLoginORB.loginResponseAddressLo = OSSwapHostToBigInt32(fLoginResponseAddress.addressLo);
    fLoginORB.loginResponseLength = OSSwapHostToBigInt16(sizeof( FWSBP2LoginResponse ));
    fLoginORB.lun = OSSwapHostToBigInt16(fLUN->getLUNumber());
    FWKLOG( ("IOFireWireSBP2Login<%p>::allocateResources lun number = %d\n", this, OSSwapBigToHostInt16(fLoginORB.lun)) );
    fLoginORB.statusFIFOAddressHi = OSSwapHostToBigInt32((fStatusBlockAddress.nodeID << 16) | fStatusBlockAddress.addressHi);
    fLoginORB.statusFIFOAddressLo = OSSwapHostToBigInt32(fStatusBlockAddress.addressLo);

    fReconnectORB.options = OSSwapHostToBigInt16(3 |  0x8000);   // reconnect | notify
    fReconnectORB.statusFIFOAddressHi = OSSwapHostToBigInt32((fReconnectStatusBlockAddress.nodeID << 16) | fReconnectStatusBlockAddress.addressHi);
    fReconnectORB.statusFIFOAddressLo = OSSwapHostToBigInt32(fReconnectStatusBlockAddress.addressLo);

    fLogoutORB.options = OSSwapHostToBigInt16(7 | 0x8000);  	// logout | notify
    fLogoutORB.statusFIFOAddressHi = OSSwapHostToBigInt32((fStatusBlockAddress.nodeID << 16) | fStatusBlockAddress.addressHi);
    fLogoutORB.statusFIFOAddressLo = OSSwapHostToBigInt32(fStatusBlockAddress.addressLo);
    
	//
    // create command for writing the management agent
    //
    
    if( status == kIOReturnSuccess )
    {
        fLoginWriteCommandMemory = IOMemoryDescriptor::withAddress( &fLoginORBAddress, 8, kIODirectionOut);
    	if( fLoginWriteCommandMemory == NULL )
    		status = kIOReturnNoMemory;
    }

    if( status == kIOReturnSuccess )
    {
        fLoginWriteCommand = fUnit->createWriteCommand( FWAddress(0x0000ffff, 0xf0000000 + (fManagementOffset << 2)),
                                                        fLoginWriteCommandMemory,
                                                        IOFireWireSBP2Login::loginWriteCompleteStatic, this, true );
        if( fLoginWriteCommand == NULL )
        	status = kIOReturnNoMemory;
    }

    if( status == kIOReturnSuccess )
    {
		fLoginTimeoutCommand = fControl->createDelayedCmd( fManagementTimeout * 1000,
                                                           IOFireWireSBP2Login::loginTimeoutStatic, this);
	}


    if( status == kIOReturnSuccess )
    {
		fLoginRetryTimeoutCommand = fControl->createDelayedCmd( 1000000, // 1 second
                                                                IOFireWireSBP2Login::loginRetryTimeoutStatic, this);
	}
	
	if( status == kIOReturnSuccess )
    {
		fReconnectRetryTimeoutCommand = fControl->createDelayedCmd( 100000, // 1/10th of a second
                                                                IOFireWireSBP2Login::reconnectRetryTimeoutStatic, this);
	}
	
    //
    // create command for writing the management agent during reconnect
    //
    
    if( status == kIOReturnSuccess )
    {
        fReconnectWriteCommandMemory = IOMemoryDescriptor::withAddress( &fReconnectORBAddress, 8, kIODirectionOut);
    	if( fReconnectWriteCommandMemory == NULL )
    		status = kIOReturnNoMemory;
    }

    if( status == kIOReturnSuccess )
    {
        fReconnectWriteCommand = fUnit->createWriteCommand( FWAddress(0x0000ffff, 0xf0000000 + (fManagementOffset << 2)),
                                                            fReconnectWriteCommandMemory,
                                                            IOFireWireSBP2Login::reconnectWriteCompleteStatic, this, true );
        if( fReconnectWriteCommand == NULL )
        	status = kIOReturnNoMemory;
    }

   if( status == kIOReturnSuccess )
    {
		fReconnectTimeoutCommand = (IOFWDelayCommand*)fControl->createDelayedCmd( ((fManagementTimeout + 1000) * 1000),
                                                                IOFireWireSBP2Login::reconnectTimeoutStatic, this);
		if( !fReconnectTimeoutCommand )
			status = kIOReturnNoMemory;
	}

    //
    // create command for writing the management agent during logout
    //
    
    if( status == kIOReturnSuccess )
    {
        fLogoutWriteCommandMemory = IOMemoryDescriptor::withAddress( &fLogoutORBAddress, 8, kIODirectionOut);
    	if( fLogoutWriteCommandMemory == NULL )
    		status = kIOReturnNoMemory;
    }

    if( status == kIOReturnSuccess )
    {
        fLogoutWriteCommand = fUnit->createWriteCommand( FWAddress(0x0000ffff, 0xf0000000 + (fManagementOffset << 2)),
                                                         fLogoutWriteCommandMemory,
                                                         IOFireWireSBP2Login::logoutWriteCompleteStatic, this, true );
        if( fLogoutWriteCommand == NULL )
        	status = kIOReturnNoMemory;
    }

	//
	// create command timeout during logout
	//

    if( status == kIOReturnSuccess )
    {
		fLogoutTimeoutCommand = fControl->createDelayedCmd( fManagementTimeout * 1000,
                                                            IOFireWireSBP2Login::logoutTimeoutStatic, this);
	}

	//
	// create command for fetch agent write retries
	//

    if( status == kIOReturnSuccess )
    {
		fFetchAgentRetryTimerCommand = fControl->createDelayedCmd( kFetchAgentRetryInterval,
                                                            IOFireWireSBP2Login::fetchAgentRetryTimerStatic, this);
	}
	
    //
    // create command for writing the fetch agent
    //

	//
    // create command for writing the fast start register
    //
    
	if( fFastStartSupported )
	{
		if( status == kIOReturnSuccess )
		{
			UInt32 size = fFastStartMaxPayload * 4;
			if( size == 0 )
				size = 4096;
			fFetchAgentWriteCommandMemory = IOBufferMemoryDescriptor::withOptions( kIODirectionOutIn | kIOMemoryUnshared, size, PAGE_SIZE );
			if( fFetchAgentWriteCommandMemory == NULL )
				status = kIOReturnNoMemory;
		}
	
		if( status == kIOReturnSuccess )
		{
			fFetchAgentWriteCommand = fUnit->createWriteCommand( FWAddress(0,0),  // tbd later
																fFetchAgentWriteCommandMemory,
																IOFireWireSBP2Login::fetchAgentWriteCompleteStatic, this, true );
			if( fFetchAgentWriteCommand == NULL )
				status = kIOReturnNoMemory;
		}
        
        if( status == kIOReturnSuccess )
        {
            // extra long timeout to support a slow to respond still camera
            fFetchAgentWriteCommand->setTimeout( kFetchAgentSplitTimeout );
			fFetchAgentWriteCommand->setRetries( 0 );
        }
	}
    else
	{
		if( status == kIOReturnSuccess )
		{
			fFetchAgentWriteCommandMemory = IOMemoryDescriptor::withAddress( &fLastORBAddress, 8, kIODirectionOut );
			if( fFetchAgentWriteCommandMemory == NULL )
				status = kIOReturnNoMemory;
		}
	
		if( status == kIOReturnSuccess )
		{
			fFetchAgentWriteCommand = fUnit->createWriteCommand( FWAddress(0,0),  // tbd later
																fFetchAgentWriteCommandMemory,
																IOFireWireSBP2Login::fetchAgentWriteCompleteStatic, this, true );
			if( fFetchAgentWriteCommand == NULL )
				status = kIOReturnNoMemory;
		}
        
        if( status == kIOReturnSuccess )
        {
            // extra long timeout to support a slow to respond still camera
            fFetchAgentWriteCommand->setTimeout( kFetchAgentSplitTimeout );
			fFetchAgentWriteCommand->setRetries( 0 );
		}
	}
	
	//
    // create command for reseting the fetch agent
    //
    
    if( status == kIOReturnSuccess )
    {
        fFetchAgentResetCommand = fUnit->createWriteQuadCommand( FWAddress(0,0),  // tbd later
                                                             &fFetchAgentResetBuffer, 1,
                                                             IOFireWireSBP2Login::fetchAgentResetCompleteStatic, 
															 this, true );
        if( fFetchAgentResetCommand == NULL )
        	status = kIOReturnNoMemory;
    }
    
	//
    // create command for ringing the doorbell
    //
    
    if( status == kIOReturnSuccess )
    {
        fDoorbellCommand = fUnit->createWriteQuadCommand( FWAddress(0,0),  // tbd later
                                                             &fDoorbellBuffer, 1,
                                                             IOFireWireSBP2Login::doorbellCompleteStatic, 
															 this, true );
        if( fDoorbellCommand == NULL )
        	status = kIOReturnNoMemory;
    }

	//
    // create command for setting busy timeout
    //
    
    if( status == kIOReturnSuccess )
    {
        fSetBusyTimeoutCommand = fUnit->createWriteQuadCommand( fSetBusyTimeoutAddress,
                                                             &fSetBusyTimeoutBuffer, 1,
                                                             IOFireWireSBP2Login::setBusyTimeoutCompleteStatic, 
															 this, true );
        if( fSetBusyTimeoutCommand == NULL )
        	status = kIOReturnNoMemory;
    }

	//
    // create command for enabling unsolicited status
    //
    
    if( status == kIOReturnSuccess )
    {
        fUnsolicitedStatusEnableCommand = fUnit->createWriteQuadCommand( FWAddress(0,0),  // tbd later
                                                             &fUnsolicitedStatusEnableBuffer, 1,
                                                             IOFireWireSBP2Login::unsolicitedStatusEnableCompleteStatic, 
															 this, true );
        if( fUnsolicitedStatusEnableCommand == NULL )
        	status = kIOReturnNoMemory;
    }

    return status;
}

void IOFireWireSBP2Login::release() const
{
	if( getRetainCount() >= 2 ) 
		OSObject::release(2);
}

void IOFireWireSBP2Login::free( void )
{
    IOReturn						status = kIOReturnSuccess;

    FWKLOG( ( "IOFireWireSBP2Login<%p> : free called\n", this ) );

	if( fControl && fTarget )
	{
		fControl->closeGate();

		if( fReconnectWriteCommand )
		{
			fTarget->cancelMgmtAgentAccess( fReconnectWriteCommand );
		}
		
		if( fLoginWriteCommand )
		{
			fTarget->cancelMgmtAgentAccess( fLoginWriteCommand );
		}
		
		fControl->openGate();
	}
	
    // disconnect from lun
    removeLogin();

	///////////////////////////////////////////

	//
	// release command for reseting fetch agent
	//
	
	if( fFetchAgentResetInProgress )
		fFetchAgentResetCommand->cancel( kIOReturnAborted );
		
	if( fFetchAgentResetCommand )
		fFetchAgentResetCommand->release();

	//
	// release command for ringing the doorbell
	//
	
	if( fDoorbellInProgress )
		fDoorbellCommand->cancel( kIOReturnAborted );
		
	if( fDoorbellCommand )
		fDoorbellCommand->release();

	//
	// release command for enabling unsolicited status
	//
	
	if( fUnsolicitedStatusEnableInProgress )
		fUnsolicitedStatusEnableCommand->cancel( kIOReturnAborted );
		
	if( fUnsolicitedStatusEnableCommand )
		fUnsolicitedStatusEnableCommand->release();
	
	//
	// release command for setting busy timeout
	//
	
	if( fSetBusyTimeoutInProgress )
		fSetBusyTimeoutCommand->cancel( kIOReturnAborted );
		
	if( fSetBusyTimeoutCommand )
		fSetBusyTimeoutCommand->release();
		
    //
    // release command for writing the management agent
    //

    if( fLoginWriteInProgress )
        fLoginWriteCommand->cancel( kIOReturnAborted );

    if( fLoginWriteCommand != NULL )
        fLoginWriteCommand->release();

    if( fLoginWriteCommandMemory != NULL )
        fLoginWriteCommandMemory->release();

    // cancel timer
    if( fLoginTimeoutTimerSet )
        fLoginTimeoutCommand->cancel(kIOReturnAborted);

	if( fLoginTimeoutCommand )
		fLoginTimeoutCommand->release();

	// cancel timer
	stopLoginRetryTimer();
	
	if( fLoginRetryTimeoutCommand )
		fLoginRetryTimeoutCommand->release();

	// cancel timer
	stopReconnectRetryTimer();
	
	if( fReconnectRetryTimeoutCommand )
		fReconnectRetryTimeoutCommand->release();
		
    //
    // deallocate login orb address space
    //

    if( fLoginORBAddressSpace != NULL && status == kIOReturnSuccess )
    {
        fLoginORBAddressSpace->deactivate();
        fLoginORBAddressSpace->release();
    }

    //
    // deallocate login response address space
    //

    if( fLoginResponseAddressSpace != NULL && status == kIOReturnSuccess )
    {
        fLoginResponseAddressSpace->deactivate();
        fLoginResponseAddressSpace->release();
    }

	///////////////////////////////////////////

    //
    // release command for writing the management agent during reconnect
    //

    if( fReconnectWriteInProgress )
        fReconnectWriteCommand->cancel(kIOReturnAborted);

    if( fReconnectWriteCommand != NULL )
        fReconnectWriteCommand->release();

    if( fReconnectWriteCommandMemory != NULL )
        fReconnectWriteCommandMemory->release();

    // cancel timer
    if( fReconnectTimeoutTimerSet )
        fReconnectTimeoutCommand->cancel(kIOReturnAborted);

	if( fReconnectTimeoutCommand )
		fReconnectTimeoutCommand->release();

    //
    // deallocate reconnect orb address space
    //

    if( fReconnectORBAddressSpace != NULL && status == kIOReturnSuccess )
    {
        fReconnectORBAddressSpace->deactivate();
        fReconnectORBAddressSpace->release();
     }

    //
    // deallocate reconnect status block address space
    //

    if( fReconnectStatusBlockAddressSpace != NULL && status == kIOReturnSuccess )
    {
        fReconnectStatusBlockAddressSpace->deactivate();
        fReconnectStatusBlockAddressSpace->release();
    }
  
  	///////////////////////////////////////////
  
    //
    // release command for writing the management agent during logout
    //

    if( fLogoutWriteInProgress )
        fLogoutWriteCommand->cancel(kIOReturnAborted);

    if( fLogoutWriteCommand != NULL )
        fLogoutWriteCommand->release();

    if( fLogoutWriteCommandMemory != NULL )
        fLogoutWriteCommandMemory->release();

	// cancel timer
    if( fLogoutTimeoutTimerSet )
        fLogoutTimeoutCommand->cancel(kIOReturnAborted);

	// release logout timer
	if( fLogoutTimeoutCommand )
		fLogoutTimeoutCommand->release();

    //
    // deallocate logout orb address space
    //

    if( fLogoutORBAddressSpace != NULL && status == kIOReturnSuccess )
    {
        fLogoutORBAddressSpace->deactivate();
        fLogoutORBAddressSpace->release();
     }

	///////////////////////////////////////////

    //
    // release command for writing the fetch agent
    //
	
	// cancel timer
	stopFetchAgentRetryTimer();
	
	if( fFetchAgentRetryTimerCommand )
	{
		fFetchAgentRetryTimerCommand->release();
		fFetchAgentRetryTimerCommand = NULL;
	}
	
    if( fFetchAgentWriteCommandInUse )
    {
	    fFetchAgentWriteCommand->cancel(kIOReturnAborted);
	}
	
    if( fFetchAgentWriteCommand != NULL )
    {
	    fFetchAgentWriteCommand->release();
		fFetchAgentWriteCommand = NULL;
	}
	
    if( fFetchAgentWriteCommandMemory != NULL )
    {
	    fFetchAgentWriteCommandMemory->release();
		fFetchAgentWriteCommandMemory = NULL;
    }
	 
    //
    // deallocate status block address space
    //

    if( fStatusBlockAddressSpace != NULL && status == kIOReturnSuccess )
    {
        fStatusBlockAddressSpace->deactivate();
        fStatusBlockAddressSpace->release();
    }

    //
    // deallocate old password address space
    //
	
    if( fPasswordAddressSpace != NULL )
    {
        fPasswordAddressSpace->deactivate();
        fPasswordAddressSpace->release();
        fPasswordBuf = NULL;
        fPasswordLen = 0;
    }

	if( fPasswordDescriptor != NULL )
	{
		fPasswordDescriptor->release();
		fPasswordDescriptor = NULL;
	}

	//
	// free unreleased orbs
	//
	
	if( fORBSetIterator )
	{
		IOFireWireSBP2ORB * item = NULL;
		do
		{
            fORBSetIterator->reset();	
			item = (IOFireWireSBP2ORB *)fORBSetIterator->getNextObject();
			if( item )
				item->release();
		} while( item );
		
		fORBSetIterator->release();
	}
	
	if( fORBSet )
		fORBSet->release();
    
	// clean up any dangling critical section disables
	for( int i = 0; i < fCriticalSectionCount; i++ )
	{
		fTarget->endIOCriticalSection();
	}
	
	//
	// destroy command gate
	//
	
	if( fGate != NULL )
	{
		IOWorkLoop * workLoop = NULL;

		workLoop = fGate->getWorkLoop();
		workLoop->removeEventSource( fGate );
		workLoop->release();

		fGate->release();
		fGate = NULL;
	}

	OSObject::free();
}

#pragma mark -
/////////////////////////////////////////////////////////////////////
// accessors
//

// get unit

IOFireWireUnit * IOFireWireSBP2Login::getFireWireUnit( void )
{
    return fUnit;
}

// get lun

IOFireWireSBP2LUN * IOFireWireSBP2Login::getFireWireLUN( void )
{
    return fLUN;
}

// get command block size

UInt32 IOFireWireSBP2Login::getMaxCommandBlockSize( void )
{
    return fMaxCommandBlockSize;
}

UInt32 IOFireWireSBP2Login::getLoginID( void )
{
    return fLoginID;
}

// get / set refCon

void IOFireWireSBP2Login::setRefCon( void * refCon )
{
    fRefCon = refCon;
}

void * IOFireWireSBP2Login::getRefCon( void )
{
    return fRefCon;
}

// get / set login flags

void IOFireWireSBP2Login::setLoginFlags( UInt32 loginFlags )
{
    fLoginFlags = loginFlags;
    FWKLOG(( "IOFireWireSBP2Login<%p> : setLoginFlags : 0x%08lx\n", this, fLoginFlags ));
}

UInt32 IOFireWireSBP2Login::getLoginFlags( void )
{
    return fLoginFlags;
}

// set login and logout completion routines

void IOFireWireSBP2Login::setLoginCompletion( void * refCon, FWSBP2LoginCallback completion )
{
    fLoginCompletionRefCon = refCon;
    fLoginCompletionCallback = completion;
}

void IOFireWireSBP2Login::setLogoutCompletion( void * refCon, FWSBP2LogoutCallback completion )
{
    fLogoutCompletionRefCon = refCon;
    fLogoutCompletionCallback = completion;
}

// get / set reconnect time

void IOFireWireSBP2Login::setReconnectTime( UInt32 reconnectTime )
{
    fReconnectTime = reconnectTime & 0x0000000f;
}

UInt32 IOFireWireSBP2Login::getReconnectTime( void )
{
    return fReconnectTime;
}

// get / set status notify proc

void IOFireWireSBP2Login::setStatusNotifyProc( void * refCon, FWSBP2NotifyCallback callback )
{
    fStatusNotifyCallback = callback;
    fStatusNotifyRefCon = refCon;
}

void IOFireWireSBP2Login::getStatusNotifyProc( void ** refCon, FWSBP2NotifyCallback * callback )
{
    *callback = fStatusNotifyCallback;
    *refCon = fStatusNotifyRefCon;
}

// get / set status notify proc

void IOFireWireSBP2Login::setUnsolicitedStatusNotifyProc( void * refCon, FWSBP2NotifyCallback callback )
{
    fUnsolicitedStatusNotifyCallback = callback;
    fUnsolicitedStatusNotifyRefCon = refCon;
}

void IOFireWireSBP2Login::getUnsolicitedStatusNotifyProc( void ** refCon, FWSBP2NotifyCallback * callback )
{
    *callback = fStatusNotifyCallback;
    *refCon = fStatusNotifyRefCon;
}

// get / set max payload size

void IOFireWireSBP2Login::setMaxPayloadSize( UInt32 maxPayloadSize )
{
    fMaxPayloadSize = maxPayloadSize;
}

UInt32 IOFireWireSBP2Login::getMaxPayloadSize( void )
{
    return fMaxPayloadSize;
}

void IOFireWireSBP2Login::setLoginRetryCountAndDelayTime( UInt32 retryCount, UInt32 uSecs )
{
	fLoginRetryMax = retryCount;
	fLoginRetryDelay = uSecs;

	if( fLoginRetryTimeoutCommand )
	{
		fLoginRetryTimeoutCommand->reinit( fLoginRetryDelay, IOFireWireSBP2Login::loginRetryTimeoutStatic, this );
	}
}
	

// set password
//
//

IOReturn IOFireWireSBP2Login::setPassword( IOMemoryDescriptor * memory )
{
    IOReturn status = kIOReturnSuccess;
	IOByteCount len = 0;
	
    //
    // deallocate old password address space
    //
	
    if( fPasswordAddressSpace != NULL )
    {
        fPasswordAddressSpace->deactivate();
        fPasswordAddressSpace->release();
        fPasswordBuf = NULL;
        fPasswordLen = 0;
    }

	if( fPasswordDescriptor != NULL )
	{
		fPasswordDescriptor->release();
		fPasswordDescriptor = NULL;
	}
	
	if( status == kIOReturnSuccess )
	{
		fPasswordDescriptor = memory;
		if( memory == NULL )
			status = kIOReturnError;
	}
	
	if( status == kIOReturnSuccess )
	{
		fPasswordDescriptor->retain();
		len = fPasswordDescriptor->getLength();
		status = fPasswordDescriptor->prepare();  // for readBytes
	}
	
	if( status == kIOReturnSuccess )
	{	
		if( len <= 8 )
		{
			fLoginORB.password[0] = 0;
			fLoginORB.password[1] = 0;
			fLoginORB.passwordLength = 0;			
			status = memory->readBytes( 0, &(fLoginORB.password), len );
		}
		else
		{
	
			//
			// allocate and register an address space for the password
			//
			
			if( status == kIOReturnSuccess )
			{
				fPasswordAddressSpace = IOFWPseudoAddressSpace::simpleRW( fControl, &fPasswordAddress, memory );
				if( fPasswordAddressSpace == NULL )
					status = kIOReturnNoMemory;
			}
	
			if( status == kIOReturnSuccess )
			{
				status = fPasswordAddressSpace->activate();
			}
	
			if( status == kIOReturnSuccess )
			{
				fLoginORB.passwordLength = OSSwapHostToBigInt16(len);
				fLoginORB.password[0] = OSSwapHostToBigInt32(0x0000ffff & fPasswordAddress.addressHi);
				fLoginORB.password[1] = OSSwapHostToBigInt32(fPasswordAddress.addressLo);
			}
			
		}
    }
	
    return status;
}

// set password
//
//

IOReturn IOFireWireSBP2Login::setPassword( void * buf, UInt32 len )
{
    IOReturn						status 			= kIOReturnSuccess;

    //
    // deallocate old password address space
    //

	
    if( fPasswordAddressSpace != NULL )
    {
        fPasswordAddressSpace->deactivate();
        fPasswordAddressSpace->release();
        fPasswordBuf = NULL;
        fPasswordLen = 0;
    }

	if( fPasswordDescriptor != NULL )
	{
		fPasswordDescriptor->release();
		fPasswordDescriptor = NULL;
	}
	
    if( len <= 8 )
    {
        fPasswordBuf = buf;
        fPasswordLen = len;

		fLoginORB.password[0] = 0;
		fLoginORB.password[1] = 0;
		
        bcopy( buf, &(fLoginORB.password), len );
        fLoginORB.passwordLength = 0;
    }
    else
    {

        //
        // allocate and register an address space for the password
        //

        if( status == kIOReturnSuccess )
        {
			fPasswordAddressSpace = IOFWPseudoAddressSpace::simpleRW( fControl, &fPasswordAddress, len, buf );
			if( fPasswordAddressSpace == NULL )
				status = kIOReturnNoMemory;
        }

        if( status == kIOReturnSuccess )
        {
            status = fPasswordAddressSpace->activate();
        }

        if( status == kIOReturnSuccess )
        {
            fPasswordBuf = buf;
            fPasswordLen = len;
            fLoginORB.passwordLength = OSSwapHostToBigInt16(len);
            fLoginORB.password[0] = OSSwapHostToBigInt32(0x0000ffff & fPasswordAddress.addressHi);
            fLoginORB.password[1] = OSSwapHostToBigInt32(fPasswordAddress.addressLo);
        }
        
    }
    
    return status;
}

// setLoginGeneration
//
//

void IOFireWireSBP2Login::setLoginGeneration( UInt32 generation )
{
	fUserLoginGeneration = generation;
	fUserLoginGenerationSet = true;
}

// clearLoginGeneration
//
//

void IOFireWireSBP2Login::clearLoginGeneration( void )
{
	fUserLoginGenerationSet = false;	
}
	
#pragma mark -
/////////////////////////////////////////////////////////////////////
//
// login path
//

// submitLogin
//
//

IOReturn IOFireWireSBP2Login::submitLogin( void )
{
    IOReturn status = kIOReturnSuccess;
		
	status = fGate->runAction( staticExecuteLogin );
	
	return status;
}

IOReturn IOFireWireSBP2Login::staticExecuteLogin( OSObject *self, void *, void *, void *, void * )
{
	return ((IOFireWireSBP2Login *)self)->initialExecuteLogin();
}

IOReturn IOFireWireSBP2Login::initialExecuteLogin( void )
{
	IOReturn status = kIOReturnSuccess;
	
	if( fLoginState != kLoginStateIdle || fLoginRetryTimeoutTimerSet )
	{
		return kIOReturnExclusiveAccess;
	}
	
	fLoginRetryCount = fLoginRetryMax;

	status = executeLogin();
	if( status != kIOReturnSuccess && 
	    ((status != kIOFireWireBusReset) || !fUserLoginGenerationSet) &&
		fLoginRetryCount != 0 )
	{
		fLoginRetryCount--;
		startLoginRetryTimer();
		status = kIOReturnSuccess;
	}
	
	return status;
}

IOReturn IOFireWireSBP2Login::executeLogin( void )
{
    IOReturn status = kIOReturnSuccess;
    
	FWKLOG( ( "IOFireWireSBP2Login<%p> : executeLogin\n", this ) );

	if( fSuspended )
	{
		return kIOFireWireBusReset;
	}
	
	if( fLoginState != kLoginStateIdle )
	{
		return kIOReturnExclusiveAccess;
	}
	
    fLoginState = kLoginStateLoggingIn;  		// set state
	
    if( fLastORB )
    {
        fLastORB->release();
        fLastORB = NULL;
    }

	fLastORBAddress = FWAddress(0,0);
	
	// to quote sbp-2 : ... truncated login response data 
	// shall be interpreted as if the omitted fields
	// had been stored as zeros. ... 
	
    fLoginResponse.reserved = 0;
    fLoginResponse.reconnectHold = 0;

    // set options
    FWKLOG(( "IOFireWireSBP2Login<%p> : fLoginFlags : 0x%08lx\n", this, fLoginFlags ));

    fLoginORB.options = OSSwapHostToBigInt16(0x0000 | 0x8000);		// login | notify
    fLoginORB.options |= OSSwapHostToBigInt16(fReconnectTime << 4);
    if( fLoginFlags & kFWSBP2ExclusiveLogin )
        fLoginORB.options |= OSSwapHostToBigInt16(0x1000);

    // set to correct generation
    fLoginWriteCommand->reinit( FWAddress(0x0000ffff, 0xf0000000 + (fManagementOffset << 2)),
                                fLoginWriteCommandMemory, IOFireWireSBP2Login::loginWriteCompleteStatic,
                                this, true );

    // get local node and generation
    // note: these two are latched affter command gen is set and before submit is called
    // if these are old command will fail
    fUnit->getNodeIDGeneration(fLoginGeneration, fLoginNodeID, fLocalNodeID);	
		
	if( fUserLoginGenerationSet && fUserLoginGeneration != fLoginGeneration )
	{
		status = kIOFireWireBusReset;
	}
	
	if( status == kIOReturnSuccess )
	{
		fLoginWriteInProgress = true;
		fLoginStatusReceived = false;
		status = fTarget->beginIOCriticalSection();
	}
	
	if( status == kIOReturnSuccess )
	{
		fInCriticalSection = true;
		fCriticalSectionCount++;
		
		if( fLoginFlags & kFWSBP2DontSynchronizeMgmtAgent )
			fLoginWriteCommand->submit();
		else
			fTarget->synchMgmtAgentAccess( fLoginWriteCommand );
	}
	
	if( status != kIOReturnSuccess )
	{		
		fLoginWriteInProgress = false;
		fLoginState = kLoginStateIdle;
    }
    
    return status;
}

//
// login write completion
//

void IOFireWireSBP2Login::loginWriteCompleteStatic( void *refcon, IOReturn status, IOFireWireNub *device,
                                                    IOFWCommand *fwCmd )
{
    ((IOFireWireSBP2Login*)refcon)->loginWriteComplete( status, device, fwCmd );
}

void IOFireWireSBP2Login::loginWriteComplete( IOReturn status, IOFireWireNub *device, IOFWCommand *fwCmd )
{
    FWKLOG( ("IOFireWireSBP2Login<%p> : login write complete\n", this) );

    fLoginWriteInProgress = false;
 
	if( fLoginStatusReceived )
	{
		processLoginWrite();
	}   
    else
	{
		if( status == kIOReturnSuccess )
		{
			// we wrote the management agent, now set a timer and wait for response & status
			fLoginTimeoutTimerSet = true;
			if( fLoginTimeoutCommand->submit() != kIOReturnSuccess )
				fLoginTimeoutTimerSet = false;  
		}
		else
		{
			fLoginState = kLoginStateIdle;
			completeLogin( status );  // complete with error
		}
	}
}

//
// login timeout
//

void IOFireWireSBP2Login::loginTimeoutStatic( void *refcon, IOReturn status,
                                               IOFireWireBus *bus, IOFWBusCommand *fwCmd)
{
    ((IOFireWireSBP2Login*)refcon)->loginTimeout( status, bus, fwCmd );
}

void IOFireWireSBP2Login::loginTimeout( IOReturn status, IOFireWireBus *bus, IOFWBusCommand *fwCmd)
{
    fLoginTimeoutTimerSet = false;
    if( status != kIOReturnSuccess )
    {
        FWKLOG( ("IOFireWireSBP2Login<%p> : login timed out\n", this) );
        
        // init state machine
        fLoginState = kLoginStateIdle;
        completeLogin( kIOReturnTimeout );
    }
}

// login retry timeout
//
//

void IOFireWireSBP2Login::startLoginRetryTimer( void )
{
	stopLoginRetryTimer();
	
	fLoginRetryTimeoutTimerSet = true;
	if( fLoginRetryTimeoutCommand->submit() != kIOReturnSuccess )
		fLoginRetryTimeoutTimerSet = false;  
}

void IOFireWireSBP2Login::stopLoginRetryTimer( void )
{
	// cancel timeout
    if( fLoginRetryTimeoutTimerSet )
        fLoginRetryTimeoutCommand->cancel( kIOReturnAborted );
}

void IOFireWireSBP2Login::loginRetryTimeoutStatic( void *refcon, IOReturn status,
                                                   IOFireWireBus *bus, IOFWBusCommand *fwCmd)
{
    ((IOFireWireSBP2Login*)refcon)->loginRetryTimeout( status, bus, fwCmd );
}

void IOFireWireSBP2Login::loginRetryTimeout( IOReturn status, IOFireWireBus *bus, IOFWBusCommand *fwCmd)
{
    fLoginRetryTimeoutTimerSet = false;
    if( status == kIOReturnTimeout )
    {
        FWKLOG( ("IOFireWireSBP2Login<%p> : login retry timer fired\n", this) );
		
		IOReturn login_status = executeLogin();
		if( login_status != kIOReturnSuccess )
		{
			completeLogin( login_status );
		}
	}
}

void IOFireWireSBP2Login::abortLogin( void )
{
    // if we're in the middle of a login write, we need to cancel it
    // before aborting the command
    if( fLoginWriteInProgress )
        fLoginWriteCommand->cancel( kIOReturnAborted );

    // cancel timeout
    if( fLoginTimeoutTimerSet )
        fLoginTimeoutCommand->cancel( kIOReturnAborted );

    // set state
    fLoginState = kLoginStateIdle;

    // complete command
//    completeLogin( kIOReturnTimeout );  //zzz is this a good error?, this is what OS9 returns
}

void IOFireWireSBP2Login::completeLogin( IOReturn state, const void *buf, UInt32 len, void * buf2 )
{
	FWKLOG( ( "IOFireWireSBP2Login<%p> : completeLogin\n", this ) );
	
	if( (fLoginFlags & kFWSBP2DontSynchronizeMgmtAgent) == 0 )
		fTarget->completeMgmtAgentAccess(  );
	
	if( fInCriticalSection )
	{
		fInCriticalSection = false;
		if( fCriticalSectionCount > 0 )
		{
			fCriticalSectionCount--;
		}
		fTarget->endIOCriticalSection();
	}
	
	if( (state != kIOReturnSuccess) && 
		((state != kIOFireWireBusReset) || !fUserLoginGenerationSet) && 
		(state != kIOReturnNotAttached) &&
		(fLoginRetryCount != 0) )
	{
		fLoginRetryCount--;
		startLoginRetryTimer();
	}
	else if( fLoginCompletionCallback != NULL )
    {
        FWSBP2LoginCompleteParams		params;

		// try enabling unsolicited status now
		if( fUnsolicitedStatusEnableRequested )
		{
			fUnsolicitedStatusEnableRequested = false;
			executeUnsolicitedStatusEnable();
		}
		
        params.login = this;
        params.generation = fLoginGeneration;

        params.status = state;
        params.loginResponse = (FWSBP2LoginResponsePtr)buf2;
        params.statusBlock = (FWSBP2StatusBlock*)buf;
        params.statusBlockLength = len;
        
         (*fLoginCompletionCallback)(fLoginCompletionRefCon, &params);        
    }
}

//
// status block handlers
//

UInt32 IOFireWireSBP2Login::statusBlockWriteStatic(void *refcon, UInt16 nodeID, IOFWSpeed &speed,
                FWAddress addr, UInt32 len, const void *buf, IOFWRequestRefCon lockRead)
{
   	return ((IOFireWireSBP2Login*)refcon)->statusBlockWrite( nodeID, speed, addr, len, buf, lockRead );
}

UInt32 IOFireWireSBP2Login::statusBlockWrite( UInt16 nodeID, IOFWSpeed &speed, FWAddress addr, UInt32 len,
                                              const void *buf, IOFWRequestRefCon lockRead )
{
    FWKLOG( ( "IOFireWireSBP2Login<%p> : status block write\n", this ) );

    
	if( len < 8 )
	{
	//	IOLog( "IOFireWireSBP2Login<0x%08lx> : illegal status block length = %d\n", this, len );
		return kFWResponseComplete;
	}

    bzero( &fStatusBlock, sizeof(fStatusBlock) );
	
    //еееееееееееееееее
    if( len < sizeof(fStatusBlock) )
        bcopy( buf, &fStatusBlock, len);
    else
        bcopy( buf, &fStatusBlock, sizeof(fStatusBlock));

	const void * client_status_block = &fStatusBlock;

#if 0
	// compatibility hack for Craig's layer
	
	FWSBP2StatusBlock host_status_block;
	
	bzero( &host_status_block, sizeof(host_status_block) );
	bcopy( &fStatusBlock, &host_status_block, sizeof(host_status_block));

    host_status_block.orbOffsetHi = OSSwapBigToHostInt16(fStatusBlock.orbOffsetHi);
    host_status_block.orbOffsetLo = OSSwapBigToHostInt32(fStatusBlock.orbOffsetLo);
	
    SBP2SwapBigToHostInt32Array( &host_status_block.status, sizeof(host_status_block.status) );
	
	client_status_block = &host_status_block;	
#endif

    switch( fLoginState )
    {
        case kLoginStateLoggingIn:

            // complete login

            // cancel timeout
            if( fLoginTimeoutTimerSet )
            {
                fLoginTimeoutCommand->cancel(kIOReturnSuccess);
            }

			if( !fLoginStatusReceived )
			{
				bzero( &fLoginStatusBlock, sizeof(fLoginStatusBlock) );
		
				//еееееееееееееееее
				if( len < sizeof(fLoginStatusBlock) )
					bcopy( buf, &fLoginStatusBlock, len);
				else
					bcopy( buf, &fLoginStatusBlock, sizeof(fLoginStatusBlock));
				
				fLoginStatusBlockLen = len;
				
				if( fLoginWriteInProgress )
				{
					fLoginStatusReceived = true;
				}
				else
				{
					processLoginWrite();
				}
			}
	            
            break;

        case kLoginStateConnected:
            // orb related or unsolicited
            
            if( (fStatusBlock.details & 0xc0) == 0x80 )
            {
                // send unsolicited status
                if( fUnsolicitedStatusNotifyCallback != NULL )
                {
                    FWSBP2NotifyParams		params;

                    params.message = client_status_block;
                    params.length = len;
                    params.notificationEvent = kFWSBP2UnsolicitedStatus;
                    params.generation = fLoginGeneration;
                    (*fUnsolicitedStatusNotifyCallback)(fUnsolicitedStatusNotifyRefCon, &params );
                }

            }
            else
            {
                // normal command orb

                // find ORB
                bool found = false;
				
                FWAddress notificationAddress;
				notificationAddress.nodeID = 0x0000;
				notificationAddress.addressHi = OSSwapBigToHostInt16(fStatusBlock.orbOffsetHi);
				notificationAddress.addressLo = OSSwapBigToHostInt32(fStatusBlock.orbOffsetLo);

                IOFireWireSBP2ORB * item = NULL;
                fORBSetIterator->reset();
                while( (found != true) && (item = (IOFireWireSBP2ORB *) fORBSetIterator->getNextObject())  )
                {
                    FWAddress address;
                    item->getORBAddress( &address );
                    if( (notificationAddress.addressHi & 0x0000ffff) == (address.addressHi & 0x0000ffff) &&
                         notificationAddress.addressLo == address.addressLo ) 
                    {
                        found = true;
                    }                   
                }
                
                FWKLOG( ( "IOFireWireSBP2Login<%p> : solicited found = %d\n", this, found ) );
				
				bool deadBitIsSet = fStatusBlock.details & 0x08;
                
                if( found )
                {
					UInt32 flags = item->getCommandFlags();
					
                    // cancel timer if set
                    if( isORBTimerSet( item ) )
                    {
                        cancelORBTimer( item);
						
						if( isORBAppended( item ) )
						{
							setORBIsAppended( item, false );
							if( fCriticalSectionCount > 0 )
							{
								fCriticalSectionCount--;
							}
							fTarget->endIOCriticalSection();
						}
					}
                    else if( flags & kFWSBP2CommandCompleteNotify )
                    {
                        // if timer went off and then we get status, just ignore status.
                        // late arriving status will be discarded unless timeout duration is changed to zero
                        // at which point we can't tell that there ever was a timer running
                        break;
                    }
					
					// send solicited status
                                    
                    if( fStatusNotifyCallback != NULL )
                    {
                        FWSBP2NotifyParams		params;
     
                        params.message = client_status_block;
                        params.length = len;
                        params.notificationEvent = kFWSBP2NormalCommandStatus;
                        params.generation = fLoginGeneration;
                        params.commandObject = item;
                        (*fStatusNotifyCallback)(fStatusNotifyRefCon, &params );
                    }
                }
				
				if( deadBitIsSet )
				{
					// all tasks are aborted
					clearAllTasksInSet();
				}
            }
            
            break;

        case kLoginStateLoggingOut:

            // cancel timeout
            if( fLogoutTimeoutTimerSet )
            {
                fLogoutTimeoutCommand->cancel(kIOReturnSuccess);
             }
       
            // success or not
            if( ((fStatusBlock.details >> 4) & 3) == kFWSBP2RequestComplete &&
                fStatusBlock.sbpStatus == kFWSBP2NoSense )
            {
                FWKLOG( ( "IOFireWireSBP2Login<%p> : successful logout\n", this ) );
                fLoginState = kLoginStateIdle;
				fUnsolicitedStatusEnableRequested = false;
				
                // get login ID and fetch agent address
                fLoginID = OSSwapBigToHostInt16(fLoginResponse.loginID);
                completeLogout( kIOReturnSuccess, client_status_block, len );
            }
            else
            {
                FWKLOG( ( "IOFireWireSBP2Login<%p> : logout failed!?\n", this ) );
                fLoginState = kLoginStateIdle;
				completeLogout( kIOReturnError, client_status_block, len );
            }
			
			// all tasks are aborted
			clearAllTasksInSet();
			
            break;

        case kLoginStateReconnect:
        case kLoginStateIdle:
        default:
            FWKLOG( ( "IOFireWireSBP2Login<%p> : status block write on illegal state\n", this ) );
            break;
    }

    return kFWResponseComplete;
}

// processLoginWrite
//
//

void IOFireWireSBP2Login::processLoginWrite( void )
{
	// success or not
	if( ((fLoginStatusBlock.details >> 4) & 3) == kFWSBP2RequestComplete &&
		fLoginStatusBlock.sbpStatus == kFWSBP2NoSense )
	{
		FWKLOG( ( "IOFireWireSBP2Login<%p> : successful login\n", this ) );

		// get login ID and fetch agent address
		fLoginID = OSSwapBigToHostInt16(fLoginResponse.loginID);
		fReconnectORB.loginID = OSSwapHostToBigInt16(fLoginID);  // set id for reconnect;

		// set reconnect_hold, some devices indicate it
		if( OSSwapBigToHostInt16(fLoginResponse.length) >= 16 )
			fReconnectHold = (OSSwapBigToHostInt16(fLoginResponse.reconnectHold) & 0x7fff) + 1;
		else
			fReconnectHold = 1;

		UInt32 commandBlockAgentAddressHi = OSSwapBigToHostInt32(fLoginResponse.commandBlockAgentAddressHi);
		UInt32 commandBlockAgentAddressLo = OSSwapBigToHostInt32(fLoginResponse.commandBlockAgentAddressLo);
		
		// set fetch agent reset address
		fFetchAgentResetAddress = FWAddress( commandBlockAgentAddressHi & 0x0000ffff,
											 commandBlockAgentAddressLo + 0x00000004 );
		
		fFetchAgentResetCommand->reinit( fFetchAgentResetAddress,
										&fFetchAgentResetBuffer, 1,
										IOFireWireSBP2Login::fetchAgentResetCompleteStatic, 
										this, true );
		fFetchAgentResetCommand->updateNodeID( fLoginGeneration, fLoginNodeID );

		// set fetch agent address
		if( fFastStartSupported )
		{
			fFetchAgentAddress = FWAddress( commandBlockAgentAddressHi & 0x0000ffff,
											commandBlockAgentAddressLo + (fFastStartOffset << 2));
		}
		else
		{
			fFetchAgentAddress = FWAddress( commandBlockAgentAddressHi & 0x0000ffff,
											commandBlockAgentAddressLo + 0x00000008);
		}
		
		fFetchAgentWriteCommand->reinit( fFetchAgentAddress,  // tis determined
										 fFetchAgentWriteCommandMemory,
										 IOFireWireSBP2Login::fetchAgentWriteCompleteStatic, this, true );
		fFetchAgentWriteCommand->updateNodeID( fLoginGeneration, fLoginNodeID );
		fFetchAgentWriteCommand->setRetries( 0 );
		
		// set doorbell reset address
		fDoorbellAddress = FWAddress( commandBlockAgentAddressHi & 0x0000ffff,
									  commandBlockAgentAddressLo + 0x00000010 );
		
		fDoorbellCommand->reinit( fDoorbellAddress,
								  &fDoorbellBuffer, 1,
								  IOFireWireSBP2Login::doorbellCompleteStatic, 
								  this, true );
		fDoorbellCommand->updateNodeID( fLoginGeneration, fLoginNodeID );

		// set unsolicited status enable address
		fUnsolicitedStatusEnableAddress = FWAddress( commandBlockAgentAddressHi & 0x0000ffff,
													 commandBlockAgentAddressLo + 0x00000014 );
		
		fUnsolicitedStatusEnableCommand->reinit( fUnsolicitedStatusEnableAddress,
												 &fUnsolicitedStatusEnableBuffer, 1,
												 IOFireWireSBP2Login::unsolicitedStatusEnableCompleteStatic, 
												 this, true );
		fUnsolicitedStatusEnableCommand->updateNodeID( fLoginGeneration, fLoginNodeID );

		fLoginState = kLoginStateConnected;
		
		completeLogin( kIOReturnSuccess, &fLoginStatusBlock, fLoginStatusBlockLen, &fLoginResponse );
	}
	else
	{
		FWKLOG( ( "IOFireWireSBP2Login<%p> : login failed\n", this ) );
		fLoginState = kLoginStateIdle;

		completeLogin( kIOReturnError, &fLoginStatusBlock, fLoginStatusBlockLen, NULL );
	}
}

#pragma mark -
/////////////////////////////////////////////////////////////////////
//
// reconnect path
//

//
// suspendedNotify method
//
// called when a suspended message is received

void IOFireWireSBP2Login::suspendedNotify( void )
{
    FWKLOG( ( "IOFireWireSBP2Login<%p> : suspendedNotify\n", this ) );
    
	fSuspended = true;
	
    UInt32 generation = fControl->getGeneration();
	
	stopFetchAgentRetryTimer();		// better stop fetch agent retry timer on a bus reset
	
    switch( fLoginState )
    {
        case kLoginStateConnected:
            // start/restart timer
            startReconnectTimer();
            break;

        case kLoginStateReconnect:
			// cancel any pending reconnect retries
			stopReconnectRetryTimer();
			
			// start/restart timer
			startReconnectTimer();
            break;

        case kLoginStateLoggingIn:
			// login is valid until generation changes
			if( fLoginGeneration != generation )
			{
				abortLogin();
            }
			break;

        case kLoginStateIdle:
        case kLoginStateLoggingOut:
        default:
            FWKLOG( ("IOFireWireSBP2Login<%p> : suspended notify, nothing to do\n", this) );
            break;
    }
}

//
// resumeNotify method
//
// called when a resume message is received

void IOFireWireSBP2Login::resumeNotify( void )
{
    FWKLOG( ( "IOFireWireSBP2Login<%p> : resume notify\n", this ) );

    UInt32 generation = fControl->getGeneration();
	fPhysicalAccessEnabled = fUnit->isPhysicalAccessEnabled();
	
	fSuspended = false;
	
	executeSetBusyTimeout(); //can handle interruption
	
	if( fLogoutPending )
	{
		fLogoutPending = false;
		executeLogout();
	}
	else
	{
		switch( fLoginState )
		{
			case kLoginStateReconnect:
				if( fLoginGeneration != generation )
				{
					// start/restart reconnect
					restartReconnect();
				}
				else
				{
					// already logged in this generation
					fLoginState = kLoginStateConnected;
					if( fReconnectTimeoutTimerSet )
						fReconnectTimeoutCommand->cancel(kIOReturnAborted);
				}
				break;
	
			case kLoginStateLoggingIn:
				// login is valid until generation changes
				if( fLoginGeneration != generation)
				{
					abortLogin();
				}
				break;
	
			case kLoginStateIdle:            
			case kLoginStateLoggingOut:
			case kLoginStateConnected:
			default:
				FWKLOG( ("IOFireWireSBP2Login<%p> : resume notify, nothing to do\n", this) );
				break;
		}
	}
}

// terminateNotify method
//
// called when a terminated message is received

void IOFireWireSBP2Login::terminateNotify( void )
{
    FWKLOG( ( "IOFireWireSBP2Login<%p> : terminate notify\n", this ) );
	
	if( fLogoutPending )
	{
		fLogoutPending = false;
	}
	else
	{
		switch( fLoginState )
		{
			case kLoginStateReconnect:
				if( fReconnectTimeoutTimerSet )
					fReconnectTimeoutCommand->cancel(kIOReturnAborted);
				break;
	
			case kLoginStateLoggingIn:
				abortLogin();
				break;
	
			case kLoginStateIdle:            
			case kLoginStateLoggingOut:
			case kLoginStateConnected:
			case kLoginStateTerminated:
			default:
				FWKLOG( ("IOFireWireSBP2Login<%p> : terminate notify, nothing to do\n", this) );
				break;
		}
		
		// cancel timeout
		if( fLoginRetryTimeoutTimerSet )
		{
			fLoginRetryTimeoutCommand->cancel( kIOReturnAborted );
			fLoginState = kLoginStateTerminated;
			completeLogin( kIOReturnNotAttached );
		}
	}
}

// startReconnectTimer
//
//

void IOFireWireSBP2Login::startReconnectTimer( void )
{
	IOReturn status = kIOReturnSuccess;
	
	fLoginState = kLoginStateReconnect;
   
    if( fReconnectTimeoutTimerSet )
         fReconnectTimeoutCommand->cancel(kIOReturnAborted);
 
    FWKLOGASSERT( status == kIOReturnSuccess );
	
    // start a reconnect timer

    // even if a device went away
    // we want to keep trying for up to the reconnect timeout. maybe we'll get
    // another bus reset and the device will be back, and still willing to reconnect

    // this assumes that transient errors might cause us to loose sight of the device
    // for short periods (shorter than the timeout).  if the reconnect timeout is
    // really large (several seconds) then maybe we'll wait too long.

    // zzz some devices wait until the end of the reconnect interval before they
    // acknowledge a reconnect.  that seems bad, but for now add one second of fudge
    // factor so we can tolerate this.

	// reconnect hold is in seconds, createDelayed cmd expects microseconds	
	
	fReconnectTimeoutCommand->reinit( ((fManagementTimeout + 1000) * 1000),
									  IOFireWireSBP2Login::reconnectTimeoutStatic, this);

    // we wrote the management agent, now set a timer and wait for response & status
    fReconnectTimeoutTimerSet = true;
	
	status = fReconnectTimeoutCommand->submit();
	if( status != kIOReturnSuccess )
	{
		fReconnectTimeoutTimerSet = false;
	}
	
#if FWLOGGING
    if( fReconnectTimeoutTimerSet )
    FWKLOG( ("IOFireWireSBP2Login<%p> : reconnect timeout set for %d microseconds \n", this, ((fManagementTimeout + 1000) * 1000)) );
#endif
    
}

// doReconnect
//
// called when we recieve bus reset notification while we're logged in
// also called if we get a second reset while reconnecting, but only after
// the first reconnect attempt has been cleaned up.
//
// starts reconnect processs by writing a reconnect ORB to the target
// the remainder of the process occurs after the write command complete

void IOFireWireSBP2Login::doReconnect( void )
{
    FWKLOG( ("IOFireWireSBP2Login<%p> : reconnect\n", this) );
    
	IOReturn status = fTarget->beginIOCriticalSection();
	if( status != kIOReturnSuccess )
	{
		IOLog( "IOFireWireSBP2Login<0x%08lx>::doReconnect fTarget->beginIOCriticalSection() returned 0x%08lx\n", (UInt32)status);
		return;
	}
	
	fInCriticalSection = true;
	fCriticalSectionCount++;
	
    // set to correct generation
    fReconnectWriteCommand->reinit( FWAddress(0x0000ffff, 0xf0000000 + (fManagementOffset << 2)),
                                    fReconnectWriteCommandMemory, IOFireWireSBP2Login::reconnectWriteCompleteStatic,
                                    this, true );

    // get local node and generation
    // note: these two are latched after command gen is set and before submit is called
    // if these are old command will fail
    fUnit->getNodeIDGeneration(fLoginGeneration, fLoginNodeID, fLocalNodeID);
    
    fReconnectWriteInProgress = true;

	if( fLoginFlags & kFWSBP2DontSynchronizeMgmtAgent )
		status = fReconnectWriteCommand->submit();
	else
		status = fTarget->synchMgmtAgentAccess( fReconnectWriteCommand );
	
	if( status == kIOFireWireBusReset )
	{
		fReconnectWriteInProgress = false;
    }
	else if( status != kIOReturnSuccess )
    {
        fLoginState = kLoginStateIdle;
        fReconnectWriteInProgress = false;
		sendReconnectNotification( kIOMessageFWSBP2ReconnectFailed );
    }

}

// restartReconnect
//
// Called if we get a bus reset while we are already trying to reconnect.
// Per sbp-2, the entire reconnect process (and timer) starts over again.
// So we clean up the in-progress reconnect attempt and try again.

void IOFireWireSBP2Login::restartReconnect( void )
{
    // we may be in two possible states
    // (1) we are waiting for the reconnect write to complete
    // (2) we are waiting for either the timer to go off or a response from the target

    // case (1)

    if( fReconnectWriteInProgress )
    {
        // the write will fail with a bus reset error, unless it is already
        // complete and waiting for completion to happen.
        // (can that happen? does completion get called immediately, or is it q'd?)
        //
        // in any case, we know that it has not run yet, so we just make a note about this
        // new reset, so that when it does run, it can continue to work.  That way, even if
        // it thought the write completed, it can immediately start over.
        // the reset makes even a completed write pointless.
        
        fReconnectWriteInterrupted = true;
    }
    else
    {
        // case (2)
          
        // we already wrote the reconnect ORB to the target and we are waiting for either a
        // timeout or a response from the target.
        //

        // restart reconnect
        doReconnect();

        //zzz what if timer has gone off but not run?
    }
}

//
// reconnect write completion handler
//

void IOFireWireSBP2Login::reconnectWriteCompleteStatic( void *refcon, IOReturn status, IOFireWireNub *device,
                                                        IOFWCommand *fwCmd )
{
    ((IOFireWireSBP2Login*)refcon)->reconnectWriteComplete( status, device, fwCmd );
}

void IOFireWireSBP2Login::reconnectWriteComplete( IOReturn status, IOFireWireNub *device, IOFWCommand *fwCmd )
{
    FWKLOG( ("IOFireWireSBP2Login<%p> : reconnectWriteComplete \n", this) );

    fReconnectWriteInProgress = false;

	if( fReconnectWriteInterrupted )
    {
        fReconnectWriteInterrupted = false;
        doReconnect();
    }
	else if( status == kIOFireWireResponseBase+kFWResponseConflictError )
	{
		// retry reconnect 
		startReconnectRetryTimer();
	}
}

//
// reconnect timeout handler
//

void IOFireWireSBP2Login::reconnectTimeoutStatic( void *refcon, IOReturn status, IOFireWireBus *bus,
                                                  IOFWBusCommand *fwCmd )
{
    ((IOFireWireSBP2Login*)refcon)->reconnectTimeout( status, bus, fwCmd );
}

void IOFireWireSBP2Login::reconnectTimeout( IOReturn status, IOFireWireBus *bus, IOFWBusCommand *fwCmd )
{
    fReconnectTimeoutTimerSet = false;
    FWKLOG( ("IOFireWireSBP2Login<%p> : reconnect timeout proc, status = 0x%08lx\n", this, status) );
    
	if( fInCriticalSection )
	{
		fInCriticalSection = false;
		if( fCriticalSectionCount > 0 )
		{
			fCriticalSectionCount--;
		}
		fTarget->endIOCriticalSection();
	}
	
    if(status == kIOReturnTimeout)
    {
        FWKLOG( ("IOFireWireSBP2Login<0x%08lx> : reconnect timeout\n") );

        // only send failure notification if we really failed
        // zzz is this necessary
        if( fLoginState == kLoginStateReconnect )
        {
            // reset state machine
            fLoginState = kLoginStateIdle;
			
			// stop any retries that might have been in progress
			stopReconnectRetryTimer();
			
			if( fLogoutPending )
				executeLogout();
			else
				sendReconnectNotification( kIOMessageFWSBP2ReconnectFailed );
        }
     }
}

//
// reconnect status block handler
//

UInt32 IOFireWireSBP2Login::reconnectStatusBlockWriteStatic(void *refcon, UInt16 nodeID, IOFWSpeed &speed,
                                                            FWAddress addr, UInt32 len, const void *buf,
                                                            IOFWRequestRefCon lockRead)
{
    return ((IOFireWireSBP2Login*)refcon)->reconnectStatusBlockWrite( nodeID, speed, addr, len, buf, lockRead );
}

UInt32 IOFireWireSBP2Login::reconnectStatusBlockWrite( UInt16 nodeID, IOFWSpeed &speed, FWAddress addr,
                                                       UInt32 len, const void *buf, IOFWRequestRefCon lockRead )
{
    FWKLOG( ("IOFireWireSBP2Login<%p> : reconnect status block write\n", this) );

//IOLog("length %ld contents %08lx %08lx\n", len, *((UInt32 *)buf),  *(1+(UInt32 *)buf) );

    // this is possibly a belated reconnect acknowledgement. if so ignore it
    if( !fReconnectTimeoutTimerSet )
    {
        FWKLOG( ("IOFireWireSBP2Login<%p> : reconnect status block write after timer went off\n", this) );
        return kFWResponseComplete;
    }

    // this is possibly a write that came in before the most recent bus reset
    // if so ignore it
    if( fLoginGeneration != fControl->getGeneration() )
    {
        FWKLOG( ("IOFireWireSBP2Login<%p> : reconnect status block write for wrong generation\n", this) );
        return kFWResponseComplete;
    }

    bzero( &fReconnectStatusBlock, sizeof(fReconnectStatusBlock) );

    if( len < sizeof(fReconnectStatusBlock) )
        bcopy( buf, &fReconnectStatusBlock, len);
    else
        bcopy( buf, &fReconnectStatusBlock, sizeof(fReconnectStatusBlock));
	
    // erfolgreich Ъder nicht?
    if( ( ( ( fReconnectStatusBlock.details >> 4 ) & 3 ) == kFWSBP2RequestComplete ) &&
        ( fReconnectStatusBlock.sbpStatus == kFWSBP2NoSense ) )
    {
        FWKLOG( ( "IOFireWireSBP2Login<%p> : successful reconnect\n", this ) );

		// cancel timer, won't get here if its not set
		fReconnectTimeoutCommand->cancel(kIOReturnAborted);

        if( fLastORB )
        {
            fLastORB->release();
            fLastORB = NULL;
        }
		fLastORBAddress = FWAddress(0,0);
        fLoginState = kLoginStateConnected;

        // set generation
        fFetchAgentWriteCommand->reinit( fFetchAgentAddress,  
                                         fFetchAgentWriteCommandMemory,
                                         IOFireWireSBP2Login::fetchAgentWriteCompleteStatic, this, true );
		fFetchAgentWriteCommand->updateNodeID( fLoginGeneration, fLoginNodeID );
		fFetchAgentWriteCommand->setRetries( 0 );
		
		// try enabling unsolicited status now
		if( fUnsolicitedStatusEnableRequested )
		{
			fUnsolicitedStatusEnableRequested = false;
			executeUnsolicitedStatusEnable();
		}
		
		if( fLogoutPending )
		{
			fLogoutPending = false;
			executeLogout();
		}
		else
			sendReconnectNotificationWithStatusBlock( kIOMessageFWSBP2ReconnectComplete );
	}
	else if( ( ( ( fReconnectStatusBlock.details >> 4 ) & 3 ) == kFWSBP2RequestComplete ) &&
			 ( fReconnectStatusBlock.sbpStatus == kFWSBP2UnspecifiedError ) ) 
	{
		// retry reconnect
		startReconnectRetryTimer();
	}
    else
    {
        FWKLOG( ( "IOFireWireSBP2Login<%p> : reconnect failed\n", this ) );

		// cancel timer, won't get here if its not set
		fReconnectTimeoutCommand->cancel(kIOReturnAborted);

        fLoginState = kLoginStateIdle;
		if( fLogoutPending )
		{
			fLogoutPending = false;
			executeLogout();
		}
		else
			sendReconnectNotificationWithStatusBlock( kIOMessageFWSBP2ReconnectFailed );
    }

    return kFWResponseComplete;
}

// reconnect retry timeout
//
//

void IOFireWireSBP2Login::startReconnectRetryTimer( void )
{
	stopReconnectRetryTimer();
	
	fReconnectRetryTimeoutTimerSet = true;
	if( fReconnectRetryTimeoutCommand->submit() != kIOReturnSuccess )
		fReconnectRetryTimeoutTimerSet = false;  
}

void IOFireWireSBP2Login::stopReconnectRetryTimer( void )
{
	// cancel timeout
    if( fReconnectRetryTimeoutTimerSet )
        fReconnectRetryTimeoutCommand->cancel( kIOReturnAborted );
}

void IOFireWireSBP2Login::reconnectRetryTimeoutStatic( void *refcon, IOReturn status,
                                                   IOFireWireBus *bus, IOFWBusCommand *fwCmd)
{
    ((IOFireWireSBP2Login*)refcon)->reconnectRetryTimeout( status, bus, fwCmd );
}

void IOFireWireSBP2Login::reconnectRetryTimeout( IOReturn status, IOFireWireBus *bus, IOFWBusCommand *fwCmd)
{
    fReconnectRetryTimeoutTimerSet = false;
    if( status == kIOReturnTimeout )
    {
        FWKLOG( ("IOFireWireSBP2Login<%p> : reconnect retry timer fired\n", this) );

		doReconnect();
	}
}

//
// send reconnect notification
//

void IOFireWireSBP2Login::sendReconnectNotification( UInt32 event )
{
    FWSBP2ReconnectParams		params;

   	if( (fLoginFlags & kFWSBP2DontSynchronizeMgmtAgent) == 0 )
		fTarget->completeMgmtAgentAccess(  );

    params.login = this;
    params.generation = fLoginGeneration;

    params.reconnectStatusBlock = NULL;
    params.reconnectStatusBlockLength = 0;

    FWKLOG( ( "IOFireWireSBP2Login<%p> : arg address 0x%08lx\n", this, &params ) );

    FWKLOG( ( "IOFireWireSBP2Login<%p> : reconnectStatusBlock 0x%08lx, reconnectStatusBlockLength 0x%08lx\n", this, params.reconnectStatusBlock, params.reconnectStatusBlockLength ) );

    fTarget->messageClients( event, &params );
	fLUN->messageClients( event, &params );
}

void IOFireWireSBP2Login::sendReconnectNotificationWithStatusBlock( UInt32 event )
{
    FWSBP2ReconnectParams		params;

     if( (fLoginFlags & kFWSBP2DontSynchronizeMgmtAgent) == 0 )
		fTarget->completeMgmtAgentAccess(  );

    params.login = this;
    params.generation = fLoginGeneration;

    params.reconnectStatusBlock = &fReconnectStatusBlock;
    params.reconnectStatusBlockLength = sizeof(FWSBP2StatusBlock);

    FWKLOG( ( "IOFireWireSBP2Login<%p> : arg address 0x%08lx\n", this, &params ) );
    FWKLOG( ( "IOFireWireSBP2Login<%p> : reconnectStatusBlock 0x%08lx, reconnectStatusBlockLength 0x%08lx\n", this, params.reconnectStatusBlock, params.reconnectStatusBlockLength ) );
    fTarget->messageClients( event, &params );
	fLUN->messageClients( event, &params );
}

#pragma mark -
/////////////////////////////////////////////////////////////////////
//
// logout path
//

// submitLogout
//
//

IOReturn IOFireWireSBP2Login::submitLogout( void )
{
    IOReturn status = kIOReturnSuccess;
	
	status = fGate->runAction( staticExecuteLogout );
	
	return status;
}

IOReturn IOFireWireSBP2Login::staticExecuteLogout( OSObject *self, void *, void *, void *, void * )
{
	return ((IOFireWireSBP2Login *)self)->executeLogout();
}

IOReturn IOFireWireSBP2Login::executeLogout( void )
{
    FWKLOG( ( "IOFireWireSBP2Login<%p> : executeLogout\n", this ) );

	// are we already processing a logout
	if( fLoginState == kLoginStateLoggingOut || fLogoutPending )
	{
		return kIOReturnBusy;		
	}

	// logout while suspended
	if( fSuspended )
	{
		fLogoutPending = true;
	}
	
	// logout during login
	if( fLoginState == kLoginStateLoggingIn )
	{
		return kIOReturnBusy;		
	}

	// logout during reconnect
	if( fLoginState == kLoginStateReconnect )
	{
		fLogoutPending = true;
	}
	
	if( fLoginState == kLoginStateIdle )
	{
//		completeLogout( kIOReturnSuccess );
// zzz what do I do here?
		return kIOReturnSuccess;
	}
	
	if( fLoginState == kLoginStateConnected )
	{
		fLoginState = kLoginStateLoggingOut;
		fLogoutORB.loginID = OSSwapHostToBigInt16(fLoginID);
	
		// set to correct generation
		fLogoutWriteCommand->reinit( FWAddress(0x0000ffff, 0xf0000000 + (fManagementOffset << 2)),
									fLogoutWriteCommandMemory, IOFireWireSBP2Login::logoutWriteCompleteStatic,
									this, true );
		fLogoutWriteCommand->updateNodeID( fLoginGeneration, fLoginNodeID );
		
		IOReturn status = fTarget->beginIOCriticalSection();
	
		if( status == kIOReturnSuccess )
		{
			fInCriticalSection = true;
			fCriticalSectionCount++;
			fLogoutWriteInProgress = true;
			status = fLogoutWriteCommand->submit();
		}
		
		if( status != kIOReturnSuccess ) 
		{
			// fLoginState = kLoginStateIdle;   //zzz what do I do here?
			fLogoutWriteInProgress = false;
			
			if( fInCriticalSection )
			{
				fInCriticalSection = false;
				if( fCriticalSectionCount > 0 )
				{
					fCriticalSectionCount--;
				}
				fTarget->endIOCriticalSection();
			}
			
			return status;
		}
	}
		
    return kIOReturnSuccess;
}

//
// logout write complete handler
//

void IOFireWireSBP2Login::logoutWriteCompleteStatic( void *refcon, IOReturn status, IOFireWireNub *device,
                                                     IOFWCommand *fwCmd )
{
    ((IOFireWireSBP2Login*)refcon)->logoutWriteComplete( status, device, fwCmd );
}

void IOFireWireSBP2Login::logoutWriteComplete( IOReturn status, IOFireWireNub *device, IOFWCommand *fwCmd )
{
    FWKLOG( ( "IOFireWireSBP2Login<%p> : logoutWriteComplete \n", this ) );

    fLogoutWriteInProgress = false;
    
    if( status == kIOReturnSuccess )
    {
        // we wrote the management agent, now set a timer and wait for response & status
        fLogoutTimeoutTimerSet = true;
		if( fLogoutTimeoutCommand->submit() != kIOReturnSuccess )
			fLogoutTimeoutTimerSet = false;
    }
    else
    {
        fLoginState = kLoginStateIdle;
        completeLogout( status );  // complete with error
    }
}

//
// logout timeout handler
//

void IOFireWireSBP2Login::logoutTimeoutStatic( void *refcon, IOReturn status, IOFireWireBus *bus, IOFWBusCommand *fwCmd )
{
    ((IOFireWireSBP2Login*)refcon)->logoutTimeout( status, bus, fwCmd );
}

void IOFireWireSBP2Login::logoutTimeout( IOReturn status, IOFireWireBus *bus, IOFWBusCommand *fwCmd )
{
    FWKLOG( ( "IOFireWireSBP2Login<%p> : logoutTimeout \n", this ) );
    
    fLogoutTimeoutTimerSet = false;
    if( status != kIOReturnSuccess )
    {
        FWKLOG( ("IOFireWireSBP2Login<%p> : logout timed out\n", this) );

        // back to idle
        fLoginState = kLoginStateIdle;
        completeLogout( status );
     }
}

void IOFireWireSBP2Login::completeLogout( IOReturn state, const void *buf, UInt32 len )
{
	FWKLOG( ( "IOFireWireSBP2Login<%p> : completeLogout\n", this ) );
	
	if( fInCriticalSection )
	{
		fInCriticalSection = false;
		if( fCriticalSectionCount > 0 )
		{
			fCriticalSectionCount--;
		}
		fTarget->endIOCriticalSection();
	}
	
    if( fLogoutCompletionCallback != NULL )
    {
        FWSBP2LogoutCompleteParams		params;

        params.login = this;
        params.generation = fLoginGeneration;

        params.status = state;
        params.statusBlock = (FWSBP2StatusBlock*)buf;
        params.statusBlockLength = len;

        (*fLogoutCompletionCallback)(fLogoutCompletionRefCon, &params);
    }
}

void IOFireWireSBP2Login::clearAllTasksInSet( void )
{
	stopFetchAgentRetryTimer();

	// send reset notification for each orb
	// find ORB
	IOFireWireSBP2ORB * item;
	fORBSetIterator->reset();
	while( (item = (IOFireWireSBP2ORB *) fORBSetIterator->getNextObject()) )
	{
		if( isORBTimerSet( item ) )
		{
			cancelORBTimer( item );
			
			if( isORBAppended( item ) )
			{
				setORBIsAppended( item, false );
				if( fCriticalSectionCount > 0 )
				{
					fCriticalSectionCount--;
				}
                fTarget->endIOCriticalSection();
            }		
			
			// send solicited status
			if( fStatusNotifyCallback != NULL )
			{
				FWSBP2NotifyParams		params;

				params.message = 0;
				params.length = 0;
				params.notificationEvent = kFWSBP2NormalCommandReset;
				params.generation = fLoginGeneration;
				params.commandObject = item;
				(*fStatusNotifyCallback)(fStatusNotifyRefCon, &params );
			}
		}
	}
	
	// if we've set aside an orb we have not set its timer yet
	if( fORBToWrite )
	{
		item = fORBToWrite;
		fORBToWrite = 0;
		
		if( item->getCommandTimeout() != 0 )
		{
			// send solicited status
			if( fStatusNotifyCallback != NULL )
			{
				FWSBP2NotifyParams		params;

				params.message = 0;
				params.length = 0;
				params.notificationEvent = kFWSBP2NormalCommandReset;
				params.generation = fLoginGeneration;
				params.commandObject = item;
				(*fStatusNotifyCallback)(fStatusNotifyRefCon, &params );
			}
		}
	}
	
    if( fLastORB )
    {
        fLastORB->release();
        fLastORB = NULL;
    }
	fLastORBAddress = FWAddress(0,0);
}

#pragma mark -
/////////////////////////////////////////////////////////////////////
//
// command block ORB routines

// createORB
//
// create an orb object

IOFireWireSBP2ORB * IOFireWireSBP2Login::createORB( void )
{
    IOFireWireSBP2ORB * orb  = new IOFireWireSBP2ORB;
    if( orb != NULL && !initORBWithLogin( orb, this ) )
    {
    	orb->release();
    	orb = NULL;
    }
	else
	{
		addORB( orb );
    }
	
    return orb;
}

// addORB
//
//

IOReturn IOFireWireSBP2Login::addORB( IOFireWireSBP2ORB * orb )
{
    IOReturn status = kIOReturnSuccess;
	
	status = fGate->runAction( staticExecuteAddORB, (void*)orb );
	
	return status;
}

IOReturn IOFireWireSBP2Login::staticExecuteAddORB( OSObject *self, void * orb, void *, void *, void * )
{
	return ((IOFireWireSBP2Login *)self)->executeAddORB( (IOFireWireSBP2ORB*)orb );
}

IOReturn IOFireWireSBP2Login::executeAddORB( IOFireWireSBP2ORB * orb )
{
	return fORBSet->setObject( orb );
}

// removeORB
//
//

IOReturn IOFireWireSBP2Login::removeORB( IOFireWireSBP2ORB * orb )
{
    IOReturn status = kIOReturnSuccess;
	
	status = fGate->runAction( staticExecuteRemoveORB, (void*)orb );
	
	return status;
}

IOReturn IOFireWireSBP2Login::staticExecuteRemoveORB( OSObject *self, void * orb, void *, void *, void * )
{
	return ((IOFireWireSBP2Login *)self)->executeRemoveORB( (IOFireWireSBP2ORB*)orb );
}

IOReturn IOFireWireSBP2Login::executeRemoveORB( IOFireWireSBP2ORB * orb )
{
	fORBSet->removeObject( orb );
	
	return kIOReturnSuccess;
}

// submitORB
//
//

IOReturn IOFireWireSBP2Login::submitORB( IOFireWireSBP2ORB * orb )
{
    IOReturn status = kIOReturnSuccess;
	
	status = fGate->runAction( staticExecuteORB, (void*)orb );

    return status;
}

IOReturn IOFireWireSBP2Login::staticExecuteORB( OSObject *self, void * orb, void *, void *, void * )
{
	return ((IOFireWireSBP2Login *)self)->executeORB( ( IOFireWireSBP2ORB *)orb );
}

IOReturn IOFireWireSBP2Login::executeORB( IOFireWireSBP2ORB * orb )
{
	IOReturn 	status = kIOReturnSuccess;
	UInt32		commandFlags = 0;
    
	if( !isConnected() )
    {
		status = kIOFireWireBusReset; //zzz better error
	}

    if( status == kIOReturnSuccess )
    {
        commandFlags = orb->getCommandFlags();
    }
	    
    if( status == kIOReturnSuccess )
    {
        if( (fFetchAgentWriteCommandInUse || fFetchAgentRetryTimerSet) &&	// if we're still writing the fetch agent
			( commandFlags & kFWSBP2CommandImmediate ) &&					// and this is an immediate orb
			fORBToWrite )													// and we've already got another orb
        {
            FWKLOG(("IOFireWireSBP2Login<%p> : fetchAgentWriteCommand still in use\n", this ));
            status = kIOReturnNoResources;
        }
    }

    if( status == kIOReturnSuccess )
    {
        UInt32 generation = orb->getCommandGeneration();
        
        // check generation
        if( commandFlags & kFWSBP2CommandCheckGeneration && !fControl->checkGeneration(generation) )
            status = kIOFireWireBusReset;
    }

	if( status == kIOReturnSuccess )
	{
		UInt32 targetFlags = fTarget->getTargetFlags();
		if( targetFlags & kIOFWSBP2FailsOnBusResetsDuringIO )
		{
			// sorry, no silent orbs if your device 
			// can't handle bus resets at any time
			UInt32 orbFlags = orb->getCommandFlags();
			orb->setCommandFlags( orbFlags | kFWSBP2CommandCompleteNotify );
		}
	}
	
    if( status == kIOReturnSuccess )
    {
        // retries failed fetch agent writes up to four times
        orb->setFetchAgentWriteRetries( 20 );
		orb->setFetchAgentWriteRetryInterval( 1000 );
        prepareORBForExecution(orb);
    }

	if( status == kIOReturnSuccess && fFastStartSupported )
	{
		// setup fast start data
			
		//
		// set fast start write length
		//
		
		UInt32 maxPacketBytes = 1 << fUnit->maxPackLog(true,fFetchAgentAddress);
		UInt32 fastStartPacketBytes = fFastStartMaxPayload << 2;
		if( fastStartPacketBytes == 0 )
		{
			fastStartPacketBytes = maxPacketBytes;
		}
		else if( maxPacketBytes < fastStartPacketBytes )
		{
			fastStartPacketBytes = maxPacketBytes;
		}

		IOBufferMemoryDescriptor * descriptor = OSDynamicCast(IOBufferMemoryDescriptor, fFetchAgentWriteCommandMemory);
		
		if( descriptor == NULL )
		{
			panic( "IOFireWireSBP2Login<0x%08lx>::executeORB() - fFetchAgentWriteCommandMemory is not an IOBufferMemoryDescriptor!\n", this );
		}
		
		descriptor->setLength( fastStartPacketBytes );

		//
		// write previous orb
		//
		
		{
			FWAddress orbAddress(0,0);
			if( commandFlags & kFWSBP2CommandImmediate )
			{
				orbAddress = FWAddress(0,0);
			}
			else
			{
				orbAddress = fLastORBAddress;
			}
		
			if( orbAddress.addressHi == 0 && orbAddress.addressLo == 0 )
			{
				orbAddress.nodeID = OSSwapHostToBigInt16(0x8000);
			}
			else
			{
				orbAddress.nodeID = OSSwapHostToBigInt16(fLocalNodeID);
			}
			descriptor->writeBytes( 0, &orbAddress, sizeof(FWAddress) );
		}

		//
		// write this orb
		//
		{
			FWAddress orbAddress(0,0);
			orb->getORBAddress( &orbAddress );
			
			orbAddress.nodeID = OSSwapHostToBigInt16(fLocalNodeID);
			orbAddress.addressHi = OSSwapHostToBigInt16(orbAddress.addressHi);
			orbAddress.addressLo = OSSwapHostToBigInt32(orbAddress.addressLo);

			descriptor->writeBytes( 8, &orbAddress, sizeof(FWAddress) );
		}
		
		//
		// prepare orb and page table
		//
		
		orb->prepareFastStartPacket( descriptor );
	
		fastStartPacketBytes = descriptor->getLength();	
	}
	
#if FWDIAGNOSTICS
	((IOFireWireSBP2Diagnostics*)(fLUN->getDiagnostics()))->incrementExecutedORBCount();
#endif
    
    if( status == kIOReturnSuccess )
    {
        if( commandFlags & kFWSBP2CommandImmediate )
        {
            if( fLastORB )
            {
                fLastORB->release();
                fLastORB = NULL;
            }
            
			FWAddress orbAddress(0,0);
			orb->getORBAddress( &orbAddress );
			
			fLastORBAddress.nodeID = OSSwapHostToBigInt16(orbAddress.nodeID);
			fLastORBAddress.addressHi = OSSwapHostToBigInt16(orbAddress.addressHi);
			fLastORBAddress.addressLo = OSSwapHostToBigInt32(orbAddress.addressLo);
            orb->retain();
			fLastORB = orb;

			if( fFetchAgentWriteCommandInUse )
			{
				fORBToWrite = orb;
		//		IOLog( "IOFireWireSBP2Login : fetch agent write command busy, putting aside orb 0x%08lx\n", orb );
			}
			else
			{
				startORBTimer(orb);
				if( fTarget->beginIOCriticalSection() == kIOReturnSuccess )
				{
#if PANIC_ON_DOUBLE_APPEND
					if( isORBAppended( orb ) )
					{
						panic( "IOFireWireSBP2Login::executeORB - double appending orb!\n" );
					}
#endif
					fCriticalSectionCount++;
					setORBIsAppended( orb, true );
					status = appendORBImmediate( orb );
				}
			}
		}
        else
        {
			startORBTimer(orb);
            if( fTarget->beginIOCriticalSection() == kIOReturnSuccess )
			{
#if PANIC_ON_DOUBLE_APPEND
					if( isORBAppended( orb ) )
					{
						panic( "IOFireWireSBP2Login::executeORB - double appending orb!\n" );
					}
#endif

				fCriticalSectionCount++;
				setORBIsAppended( orb, true );
				
				status = appendORB( orb );
				
				// clean up if there are no ORBs to chain to or if we
				// tried to chain to ourselves
				
				if( status != kIOReturnSuccess )
				{
					cancelORBTimer( orb );
					setORBIsAppended( orb, false );
					if( fCriticalSectionCount > 0 )
					{
						fCriticalSectionCount--;
					}
					fTarget->endIOCriticalSection();
				}
			}
		}
    }

    return status;
}


// are we connected?
bool IOFireWireSBP2Login::isConnected( void )
{
    return (fLoginState == kLoginStateConnected);
}

void IOFireWireSBP2Login::setFetchAgentWriteCompletion( void * refCon, FWSBP2FetchAgentWriteCallback completion )
{
	fFetchAgentWriteCompletion = completion;
	fFetchAgentWriteRefCon = refCon;
}

//
// fetchAgentWrite completion handler
//

void IOFireWireSBP2Login::fetchAgentWriteCompleteStatic( void *refcon, IOReturn status, IOFireWireNub *device, IOFWCommand *fwCmd )
{
    ((IOFireWireSBP2Login*)refcon)->fetchAgentWriteComplete( status, device, fwCmd );
}

void IOFireWireSBP2Login::fetchAgentWriteComplete( IOReturn status, IOFireWireNub *device, IOFWCommand *fwCmd )
{   
	IOFireWireSBP2ORB * orb = fORBToWrite;
    
	fFetchAgentWriteCommandInUse = false;

	if( fLastORB != NULL )
	{
	
		UInt32 retries = fLastORB->getFetchAgentWriteRetries();
		int ack = fFetchAgentWriteCommand->getAckCode();
		int rcode = fFetchAgentWriteCommand->getResponseCode();
	
		if(  status != kIOReturnSuccess && 
			 status != kIOFireWireBusReset && 
			 retries != 0 )
		{
			if( (ack == kFWAckBusyX) || 
				(ack == kFWAckBusyA) ||
				(ack == kFWAckBusyB) )
			{
	
	//			IOLog( "IOFireWireSBP2Login::fetchAgentWriteComplete fetch agent write failed! retrying\n" );
	
	#if PANIC_ON_DOUBLE_APPEND			
				panic( "IOFireWireSBP2Login::fetchAgentWriteComplete fetch agent write failed!\n" );
	#endif			

				// retry
				retries--;
				fLastORB->setFetchAgentWriteRetries( retries );

				// on busy family already delays for split timeout
				
				if( kFetchAgentRetryInterval > kFetchAgentSplitTimeout )
				{
					// wait a bit logner
					startFetchAgentRetryTimer( kFetchAgentRetryInterval - kFetchAgentSplitTimeout );
				}
				else
				{
					// waited long enough, append
					appendORBImmediate( fLastORB );
				}
				
				return;
			}
			else if ( (ack == kFWAckDataError) ||
					  (rcode == kFWResponseConflictError) ||
					  (rcode == kFWResponseDataError) )
			{
			
	//			IOLog( "IOFireWireSBP2Login::fetchAgentWriteComplete fetch agent write failed! retrying\n" );
	#if PANIC_ON_DOUBLE_APPEND			
				panic( "IOFireWireSBP2Login::fetchAgentWriteComplete fetch agent write failed!\n" );
	#endif			

				// retry
				retries--;
				fLastORB->setFetchAgentWriteRetries( retries );
				
				// arithmetic increase in retry time
				UInt32 interval = fLastORB->getFetchAgentWriteRetryInterval();
				fLastORB->setFetchAgentWriteRetryInterval( interval + 1000 );
 
				// family does not delay on these acks and rcodes
				startFetchAgentRetryTimer( interval );
			
				return;
			}
		}
    }
	
	//
	// check if we have an orb waiting to be appended
	//
	
	fORBToWrite = 0; // no more orb pending a fetch agent write
	
	// theoretically fORBToWrite should already be cleared to zero 
	// before this fails with kIOFireWireBusReset
	
	if( orb && status != kIOFireWireBusReset ) 
	{	
	//	IOLog( "IOFireWireSBP2Login : fetch agent write command free, submitting orb 0x%08lx\n", orb );
	
		// actually do the write if this completed for normal reasons
		startORBTimer( orb );
		if( fTarget->beginIOCriticalSection() == kIOReturnSuccess )
		{
#if PANIC_ON_DOUBLE_APPEND
			if( isORBAppended( orb ) )
			{
				panic( "IOFireWireSBP2Login::fetchAgentWriteComplete - double appending orb!\n" );
			}
#endif
			fCriticalSectionCount++;
			setORBIsAppended( orb, true );
			appendORBImmediate( orb );
		}
	}	

	// send complete notification here	
	if( fFetchAgentWriteCompletion != NULL )
        (*fFetchAgentWriteCompletion)( fFetchAgentWriteRefCon, status, fLastORB );
}

//
// fetch agent retry timer
//

// startFetchAgentRetryTimer
//
//

void IOFireWireSBP2Login::startFetchAgentRetryTimer( UInt32 duration )
{
	// shouldn't already be set
    FWKLOGASSERT( fFetchAgentRetryTimerSet == false );
	
	// stop it if it is
	stopFetchAgentRetryTimer();
	
	fFetchAgentRetryTimerCommand->reinit( duration,
									  IOFireWireSBP2Login::fetchAgentRetryTimerStatic, this );

    // wait a little bit before retrying
    fFetchAgentRetryTimerSet = true;
	
	IOReturn status = fFetchAgentRetryTimerCommand->submit();
	if( status != kIOReturnSuccess )
	{
		fFetchAgentRetryTimerSet = false;
	}
}

// stopFetchAgentRetryTimer
//
//

void IOFireWireSBP2Login::stopFetchAgentRetryTimer( void )
{
	if( fFetchAgentRetryTimerSet )
	{
		fFetchAgentRetryTimerCommand->cancel(kIOReturnAborted);
	}
}

// fetchAgentRetryTimer
//
//

void IOFireWireSBP2Login::fetchAgentRetryTimerStatic( void *refcon, IOReturn status, IOFireWireBus *bus, IOFWBusCommand *fwCmd )
{
    ((IOFireWireSBP2Login*)refcon)->fetchAgentRetryTimer( status, bus, fwCmd );
}

void IOFireWireSBP2Login::fetchAgentRetryTimer( IOReturn status, IOFireWireBus *bus, IOFWBusCommand *fwCmd )
{
    FWKLOG( ( "IOFireWireSBP2Login<%p>IOFireWireSBP2Login::fetchAgentRetryTimer\n", this ) );
    
    fFetchAgentRetryTimerSet = false;

	if( status == kIOReturnTimeout )
    {
		appendORBImmediate( fLastORB );
	}
}

// isFetchAgentWriteInProgress
//
// do we have a fetch agent write in progress

bool IOFireWireSBP2Login::isFetchAgentWriteInProgress( void )
{
    return (fFetchAgentWriteCommandInUse || fFetchAgentRetryTimerSet);
}

// fetch agent reset
//
//

void IOFireWireSBP2Login::setFetchAgentResetCompletion( void * refCon, FWSBP2StatusCallback completion )
{
	fFetchAgentResetCompletion = completion;
	fFetchAgentResetRefCon = refCon;
}

IOReturn IOFireWireSBP2Login::submitFetchAgentReset( void )
{
    IOReturn status = kIOReturnSuccess;
	
	status = fGate->runAction( staticExecuteFetchAgentReset );
	
	return status;
}

IOReturn IOFireWireSBP2Login::staticExecuteFetchAgentReset( OSObject *self, void *, void *, void *, void * )
{
	return ((IOFireWireSBP2Login *)self)->executeFetchAgentReset();
}

IOReturn IOFireWireSBP2Login::executeFetchAgentReset( void )
{
	IOReturn status = kIOReturnSuccess;
	
	switch( fLoginState )
	{
		case kLoginStateConnected:
			if( fFetchAgentResetInProgress )
				fFetchAgentResetCommand->cancel( kIOReturnAborted );
				
			fFetchAgentResetInProgress = true;
			fFetchAgentResetCommand->reinit( fFetchAgentResetAddress,
											&fFetchAgentResetBuffer, 1,
											IOFireWireSBP2Login::fetchAgentResetCompleteStatic, 
											this, true );
			fFetchAgentResetCommand->updateNodeID( fLoginGeneration, fLoginNodeID );
		
			fFetchAgentResetCommand->submit();
			break;
		
		case kLoginStateLoggingIn:
		case kLoginStateReconnect:
		case kLoginStateIdle:
		case kLoginStateLoggingOut:
		default:
			status = kIOReturnError;
			break;
	}
	
	return status;
}

void IOFireWireSBP2Login::fetchAgentResetCompleteStatic( void *refcon, IOReturn status, IOFireWireNub *device, IOFWCommand *fwCmd )
{
    ((IOFireWireSBP2Login*)refcon)->fetchAgentResetComplete( status, device, fwCmd );
}

void IOFireWireSBP2Login::fetchAgentResetComplete( IOReturn status, IOFireWireNub *device, IOFWCommand *fwCmd )
{
    FWKLOG( ("IOFireWireSBP2Login<%p> : fetch agent reset complete\n", this) );

    fFetchAgentResetInProgress = false;
	
	if( status == kIOReturnSuccess )
	{
		clearAllTasksInSet();
	} 

	if( status != kIOReturnAborted )
	{
		if( fFetchAgentResetCompletion )
			(*fFetchAgentResetCompletion)( fFetchAgentResetRefCon, status );
	}
}

// ringDoorbell
//
//

IOReturn IOFireWireSBP2Login::ringDoorbell( void )
{
    IOReturn status = kIOReturnSuccess;
	
	status = fGate->runAction( staticExecuteDoorbell );
	
	return status;
}

IOReturn IOFireWireSBP2Login::staticExecuteDoorbell( OSObject *self, void *, void *, void *, void * )
{
	return ((IOFireWireSBP2Login *)self)->executeDoorbell();
}

IOReturn IOFireWireSBP2Login::executeDoorbell( void )
{
	// must be logged to ring the doorbell
	if( isConnected() )
	{
		if( fDoorbellInProgress )
		{
			fDoorbellRingAgain = true;
			return kIOReturnSuccess;
		}
			
		fDoorbellInProgress = true;
		fDoorbellCommand->reinit( fDoorbellAddress,
								  &fDoorbellBuffer, 1,
								  IOFireWireSBP2Login::doorbellCompleteStatic, 
								  this, true );
		fDoorbellCommand->updateNodeID( fLoginGeneration, fLoginNodeID );
		
		fDoorbellCommand->submit();
	}
	else
		return kIOReturnError;
	
	return kIOReturnSuccess;
}

void IOFireWireSBP2Login::doorbellCompleteStatic( void *refcon, IOReturn status, IOFireWireNub *device, IOFWCommand *fwCmd )
{
    ((IOFireWireSBP2Login*)refcon)->doorbellComplete( status, device, fwCmd );
}

void IOFireWireSBP2Login::doorbellComplete( IOReturn status, IOFireWireNub *device, IOFWCommand *fwCmd )
{
    FWKLOG( ("IOFireWireSBP2Login<%p> : doorbell complete\n", this) );

    fDoorbellInProgress = false;
	
	if( fDoorbellRingAgain )
	{
		fDoorbellRingAgain = false;
		executeDoorbell();
	}
}

// enableUnsolicitedStatus
//
//

IOReturn IOFireWireSBP2Login::enableUnsolicitedStatus( void )
{
    IOReturn status = kIOReturnSuccess;
	
	status = fGate->runAction( staticExecuteUnsolicitedStatusEnable );
	
	return status;
}

IOReturn IOFireWireSBP2Login::staticExecuteUnsolicitedStatusEnable( OSObject *self, void *, void *, void *, void * )
{
	return ((IOFireWireSBP2Login *)self)->executeUnsolicitedStatusEnable();
}

IOReturn IOFireWireSBP2Login::executeUnsolicitedStatusEnable( void )
{
	IOReturn status = kIOReturnSuccess;
	
	switch( fLoginState )
	{
		case kLoginStateConnected:
			if( fUnsolicitedStatusEnableInProgress )
				fUnsolicitedStatusEnableCommand->cancel( kIOReturnAborted );
			
			fUnsolicitedStatusEnableInProgress = true;
			fUnsolicitedStatusEnableCommand->reinit( fUnsolicitedStatusEnableAddress,
									&fUnsolicitedStatusEnableBuffer, 1,
									IOFireWireSBP2Login::unsolicitedStatusEnableCompleteStatic, 
									this, true );
			fUnsolicitedStatusEnableCommand->updateNodeID( fLoginGeneration, fLoginNodeID );
		
			fUnsolicitedStatusEnableCommand->submit();
			break;
		
		case kLoginStateLoggingIn:
		case kLoginStateReconnect:
			fUnsolicitedStatusEnableRequested = true;	// try again after we're logged in
			break;
			
		case kLoginStateIdle:
		case kLoginStateLoggingOut:
		default:
			status = kIOReturnError;
			break;
	}
	
	return status;
}
	
void IOFireWireSBP2Login::unsolicitedStatusEnableCompleteStatic( void *refcon, IOReturn status, IOFireWireNub *device, IOFWCommand *fwCmd )
{
    ((IOFireWireSBP2Login*)refcon)->unsolicitedStatusEnableComplete( status, device, fwCmd );
}

void IOFireWireSBP2Login::unsolicitedStatusEnableComplete( IOReturn status, IOFireWireNub *device, IOFWCommand *fwCmd )
{
    FWKLOG( ("IOFireWireSBP2Login<%p> : unsolicitedStatusEnableComplete complete\n", this) );

    fUnsolicitedStatusEnableInProgress = false;

	if( status == kIOFireWireBusReset )
	{
		fUnsolicitedStatusEnableRequested = true; 	// try again after we're logged in
	}
}

// set busy timeout
//
//

void IOFireWireSBP2Login::setBusyTimeoutRegisterValue( UInt32 timeout )
{
	fSetBusyTimeoutBuffer = OSSwapHostToBigInt32(timeout);
	executeSetBusyTimeout();
}

IOReturn IOFireWireSBP2Login::executeSetBusyTimeout( void )
{
	if( fSetBusyTimeoutInProgress )
		fSetBusyTimeoutCommand->cancel( kIOReturnAborted );
			
	fSetBusyTimeoutInProgress = true;
	fSetBusyTimeoutCommand->reinit( fSetBusyTimeoutAddress,
								    &fSetBusyTimeoutBuffer, 1,
								    IOFireWireSBP2Login::setBusyTimeoutCompleteStatic, 
								    this, true );

	fSetBusyTimeoutCommand->submit();
			
	return kIOReturnSuccess;
}

void IOFireWireSBP2Login::setBusyTimeoutCompleteStatic( void *refcon, IOReturn status, IOFireWireNub *device, IOFWCommand *fwCmd )
{
    ((IOFireWireSBP2Login*)refcon)->setBusyTimeoutComplete( status, device, fwCmd );
}

void IOFireWireSBP2Login::setBusyTimeoutComplete( IOReturn status, IOFireWireNub *device, IOFWCommand *fwCmd )
{
    FWKLOG( ("IOFireWireSBP2Login<%p> : setBusyTimeoutComplete\n", this) );

    fSetBusyTimeoutInProgress = false;	
}

// appendORBImmediate
//
//

IOReturn IOFireWireSBP2Login::appendORBImmediate( IOFireWireSBP2ORB * orb )
{
	if( fFetchAgentWriteCommandInUse )
		 fFetchAgentWriteCommand->cancel( kIOReturnAborted );
		 
    fFetchAgentWriteCommandInUse = true;

	fFetchAgentWriteCommand->reinit( fFetchAgentAddress,
										fFetchAgentWriteCommandMemory,
										IOFireWireSBP2Login::fetchAgentWriteCompleteStatic, this, true );
	
	fFetchAgentWriteCommand->setRetries( 0 );
	fFetchAgentWriteCommand->updateNodeID( fLoginGeneration, fLoginNodeID );
							
    return fFetchAgentWriteCommand->submit();
}

// appendORB
//
//

IOReturn IOFireWireSBP2Login::appendORB( IOFireWireSBP2ORB * orb )
{
    IOReturn status = kIOReturnSuccess;
    if( fLastORB != NULL && fLastORB != orb )
    {
        if( fLastORB )
        {
            fLastORB->release();
            fLastORB = NULL;
        }
        
		FWAddress orb_address(0,0);
        orb->getORBAddress( &orb_address );
        setNextORBAddress( fLastORB, orb_address );
		
		fLastORBAddress.nodeID = OSSwapHostToBigInt16(orb_address.nodeID);
		fLastORBAddress.addressHi = OSSwapHostToBigInt16(orb_address.addressHi);
		fLastORBAddress.addressLo = OSSwapHostToBigInt32(orb_address.addressLo);
        orb->retain();
        fLastORB = orb;
    }
    else
        status = kIOReturnError;

    return status;
}

// sendTimeoutNotification
//
//

void IOFireWireSBP2Login::sendTimeoutNotification( IOFireWireSBP2ORB * orb )
{
	if( isORBAppended( orb ) )
	{
		setORBIsAppended( orb, false );
		if( fCriticalSectionCount > 0 )
		{
			fCriticalSectionCount--;
		}
		fTarget->endIOCriticalSection();
	}
	
    // send solicited status
    if( fStatusNotifyCallback != NULL )
    {
        FWSBP2NotifyParams		params;

        params.message = 0;
        params.length = 0;
        params.notificationEvent = kFWSBP2NormalCommandTimeout;
        params.generation = fLoginGeneration;
        params.commandObject = orb;
        (*fStatusNotifyCallback)(fStatusNotifyRefCon, &params );
    }
}

// setAddressLoForLoginORBAndResponse
//
//

void IOFireWireSBP2Login::setAddressLoForLoginORBAndResponse( UInt32 addressLoORB, UInt32 addressLoResponse )
{
	IOFWSBP2PseudoAddressSpace * space;
	UInt32 addressLoORBMasked = addressLoORB & 0xfffffff0;
	UInt32 addressLoResponseMasked = addressLoResponse & 0xfffffff0;
	
	fControl->closeGate();
	
	FWKLOG(( "IOFireWireSBP2Login::setAddressLoForLoginORBAndResponse<%p> - set ORB addressLo to 0x%08lx, set Response addressLo to 0x%08lx\n", this, addressLoORBMasked, addressLoResponseMasked ));
	
	space = OSDynamicCast( IOFWSBP2PseudoAddressSpace, fLoginORBAddressSpace );
	if( space != NULL )
	{
		fLoginORBAddress.addressLo = OSSwapHostToBigInt32(addressLoORBMasked);
		space->setAddressLo( addressLoORBMasked );
	}
	
	space = OSDynamicCast( IOFWSBP2PseudoAddressSpace, fLoginResponseAddressSpace );
	if( space != NULL )
	{
		fLoginResponseAddress.addressLo = addressLoResponseMasked;
		fLoginORB.loginResponseAddressLo = OSSwapHostToBigInt32(fLoginResponseAddress.addressLo);
		space->setAddressLo( addressLoResponseMasked );
	}
	
	fControl->openGate();
}

// isPhysicalAccessEnabled
//
//

bool IOFireWireSBP2Login::isPhysicalAccessEnabled( void )
{
	return fPhysicalAccessEnabled;
}

// getARDMMax
//
//

UInt32 IOFireWireSBP2Login::getARDMMax( void )
{
	return fARDMAMax;
}

#pragma mark -
//////////////////////////////////////////////////////////////////////////////////////////
// friend class wrappers

//
// IOFireWireSBP2ORB friend class wrappers
//

bool IOFireWireSBP2Login::initORBWithLogin( IOFireWireSBP2ORB * orb, IOFireWireSBP2Login * login )
{ 
	return orb->initWithLogin( login ); 
}

void IOFireWireSBP2Login::setNextORBAddress( IOFireWireSBP2ORB * orb, FWAddress address )
{ 
	orb->setNextORBAddress( address ); 
}

void IOFireWireSBP2Login::fetchAgentWriteComplete( IOFireWireSBP2ORB * orb, IOReturn status )
{ 
	// orb->fetchAgentWriteComplete( status ); 
}

bool IOFireWireSBP2Login::isORBTimerSet( IOFireWireSBP2ORB * orb )
{ 
	return orb->isTimerSet(); 
}

void IOFireWireSBP2Login::cancelORBTimer( IOFireWireSBP2ORB * orb )
{ 
	orb->cancelTimer(); 
}

void IOFireWireSBP2Login::startORBTimer( IOFireWireSBP2ORB * orb )
{ 
	orb->startTimer(); 
}

void IOFireWireSBP2Login::prepareORBForExecution( IOFireWireSBP2ORB * orb )
{ 
	orb->prepareORBForExecution(); 
}

bool IOFireWireSBP2Login::isORBAppended( IOFireWireSBP2ORB * orb )
{ 
	return orb->isAppended(); 
}

void IOFireWireSBP2Login::setORBIsAppended( IOFireWireSBP2ORB * orb, bool state )
{ 
	orb->setIsAppended( state ); 
}

//
// IOFireWireSBP2LUN friend class wrappers
//

void IOFireWireSBP2Login::removeLogin( void )
{ 
	fLUN->removeLogin( this);
}

IOFireWireSBP2Target * IOFireWireSBP2Login::getTarget( void )
{ 
	return fLUN->getTarget(); 
}
