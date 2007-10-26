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

#include <IOKit/sbp2/IOFireWireSBP2ManagementORB.h>
#include <IOKit/sbp2/IOFireWireSBP2LUN.h>

#define FIREWIREPRIVATE
#include <IOKit/firewire/IOFireWireController.h>
#undef FIREWIREPRIVATE

#include <IOKit/firewire/IOConfigDirectory.h>

#include "FWDebugging.h"

extern const OSSymbol *gUnit_Characteristics_Symbol;
extern const OSSymbol *gManagement_Agent_Offset_Symbol;

OSDefineMetaClassAndStructors( IOFireWireSBP2ManagementORB, IOFWCommand );

//OSMetaClassDefineReservedUnused(IOFireWireSBP2ManagementORB, 0);
OSMetaClassDefineReservedUnused(IOFireWireSBP2ManagementORB, 1);
OSMetaClassDefineReservedUnused(IOFireWireSBP2ManagementORB, 2);
OSMetaClassDefineReservedUnused(IOFireWireSBP2ManagementORB, 3);
OSMetaClassDefineReservedUnused(IOFireWireSBP2ManagementORB, 4);
OSMetaClassDefineReservedUnused(IOFireWireSBP2ManagementORB, 5);
OSMetaClassDefineReservedUnused(IOFireWireSBP2ManagementORB, 6);
OSMetaClassDefineReservedUnused(IOFireWireSBP2ManagementORB, 7);
OSMetaClassDefineReservedUnused(IOFireWireSBP2ManagementORB, 8);

// init
//
//

bool IOFireWireSBP2ManagementORB::initWithLUN( IOFireWireSBP2LUN * lun, void * refCon, FWSBP2ManagementCallback completion )
{
    bool res = true;
    IOReturn status	= kIOReturnSuccess;
 
	// we want the expansion data member to be zeroed if it's available 
	// so create and zero in a local then assign to the member when were done
	
	ExpansionData * exp_data = (ExpansionData*) IOMalloc( sizeof(ExpansionData) );
	if( !exp_data )
	{
		return false;
	}

	bzero( exp_data, sizeof(ExpansionData) );
	
	fExpansionData = exp_data;
   
    // store LUN & Unit
    fLUN = lun;
	fUnit = fLUN->getFireWireUnit();

    fExpansionData->fInCriticalSection = false;

    // init command fields
    fControl = fUnit->getController();
    fTimeout = 0;
    fSync = false;
	
    // set completion routine and refcon
    fCompletionCallback = completion;
    fCompletionRefCon = refCon;

    // timer flags
    fTimeoutTimerSet = false;

    fStatusBlockAddressSpace 	= NULL;
    fManagementORBAddressSpace	= NULL;
    fWriteCommand 				= NULL;
    fWriteCommandMemory 		= NULL;

    fFunction 				= 0;
    fResponseBuf 			= NULL;
    fResponseLen 			= 0;
    fResponseAddressSpace 	= NULL;
    fResponseAddress		= NULL;
    
	fCompleting = false;
	
    // init super
    res = IOFWCommand::initWithController( fControl );

    // scan unit
    if( res )
    {
        status = getUnitInformation();
        res = ( status == kIOReturnSuccess );
    }

    // allocate resources
    if( res )
    {
        status = allocateResources();
        res = ( status == kIOReturnSuccess );
    }
    
    return res;
}

/////////////////////////////////////////////////////////////////////
// getUnitInformation
//
// gathers SBP2 specific information from unit's ROM
// specifically it reads, the management agent, and unit dependent
// characterisitcs

IOReturn IOFireWireSBP2ManagementORB::getUnitInformation( void )
{
    IOReturn			status = kIOReturnSuccess;
    UInt32				unitCharacteristics = 0;
	OSObject *			prop = NULL;
	
	prop = fLUN->getProperty( gManagement_Agent_Offset_Symbol );
	if( prop == NULL )
		status = kIOReturnError;
		
	if( status == kIOReturnSuccess )
	{
		fManagementOffset = ((OSNumber*)prop)->unsigned32BitValue();
	}
	
    FWKLOG( ("IOFireWireSBP2ManagementORB<0x%08lx> : status = %d, fManagementOffset = %d\n", (UInt32)this, status, fManagementOffset) );

    //
    // find unit characteristics
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
   }
 
    return status;
}

// allocateResources
//
//

IOReturn IOFireWireSBP2ManagementORB::allocateResources( void )
{
    IOReturn					status = kIOReturnSuccess;

    //
    // allocate and register an address space for the management ORB
    //

	FWAddress host_address(0,0);
		
    if( status == kIOReturnSuccess )
    {
        fManagementORBAddressSpace = IOFWPseudoAddressSpace::simpleRead( fControl, &host_address,
                                                                         sizeof(FWSBP2TaskManagementORB), & fManagementORB );
    	if ( fManagementORBAddressSpace == NULL )
        	status = kIOReturnNoMemory;
    }

    if( status == kIOReturnSuccess )
    {
		fManagementORBAddress.nodeID = OSSwapHostToBigInt16(host_address.nodeID);
		fManagementORBAddress.addressHi = OSSwapHostToBigInt16(host_address.addressHi);
		fManagementORBAddress.addressLo = OSSwapHostToBigInt32(host_address.addressLo);
		
        status = fManagementORBAddressSpace->activate();
    }
    
    //
    // allocate and register an address space for the status block
    //

     if( status == kIOReturnSuccess )
    {
        fStatusBlockAddressSpace = fUnit->createPseudoAddressSpace( &fStatusBlockAddress, sizeof(FWSBP2StatusBlock),
                                                                    NULL, IOFireWireSBP2ManagementORB::statusBlockWriteStatic,
                                                                    this );
        if ( fStatusBlockAddressSpace == NULL )
            status = kIOReturnNoMemory;
    }

    if( status == kIOReturnSuccess )
    {
        status = fStatusBlockAddressSpace->activate();
    }

    //
    // create command for writing the management agent
    //
    
    if( status == kIOReturnSuccess )
    {
        fWriteCommandMemory = IOMemoryDescriptor::withAddress( &fManagementORBAddress, 8, kIODirectionOut );
    	if( fWriteCommandMemory == NULL )
    		status = kIOReturnNoMemory;
    }

    if( status == kIOReturnSuccess )
    {
        fWriteCommand = fUnit->createWriteCommand( FWAddress(0x0000ffff, 0xf0000000 + (fManagementOffset << 2)),
                                                   fWriteCommandMemory,
                                                   IOFireWireSBP2ManagementORB::writeCompleteStatic, this, true );
        if( fWriteCommand == NULL )
        	status = kIOReturnNoMemory;
    }

    if( status == kIOReturnSuccess )
    {
        fManagementORB.statusFIFOAddressHi = OSSwapHostToBigInt32((fStatusBlockAddress.nodeID << 16) | fStatusBlockAddress.addressHi);
        fManagementORB.statusFIFOAddressLo = OSSwapHostToBigInt32(fStatusBlockAddress.addressLo);
    }
	
	if( status == kIOReturnSuccess )
	{
		fTimeoutCommand = fControl->createDelayedCmd( fManagementTimeout * 1000,
                                                      IOFireWireSBP2ManagementORB::handleTimeoutStatic, this );
		if( !fTimeoutCommand )
			status = kIOReturnNoMemory;
	}
    
    return status;
}

// free
//
//

void IOFireWireSBP2ManagementORB::free( void )
{
    IOReturn						status = kIOReturnSuccess;
    
	removeManagementORB( this );

    // cancel timer
    if( fTimeoutTimerSet )
        fTimeoutCommand->cancel(kIOReturnAborted);

	if( fTimeoutCommand )
		fTimeoutCommand->release();

    //
    // deallocate management orb address space
    //

    if( fManagementORBAddressSpace != NULL && status == kIOReturnSuccess )
    {
        fManagementORBAddressSpace->deactivate();
        fManagementORBAddressSpace->release();
    }

    //
    // deallocate status block address space
    //

    if( fStatusBlockAddressSpace != NULL && status == kIOReturnSuccess )
    {
        fStatusBlockAddressSpace->deactivate();
        fStatusBlockAddressSpace->release();
    }

	if( fResponseMap != NULL )
	{
		fResponseMap->release();
		fResponseMap = NULL;
	}
	
    //
    // deallocate response address space
    //

    if( fResponseAddressSpace != NULL && status == kIOReturnSuccess )
    {
        fResponseAddressSpace->deactivate();
        fResponseAddressSpace->release();
    }

    //
    // release command for writing the management agent
    //

    if( fWriteCommand != NULL )
        fWriteCommand->release();

    if( fWriteCommandMemory != NULL )
        fWriteCommandMemory->release();

	//
	// free expansion data
	//
	
	if( fExpansionData )
	{
		IOFree( fExpansionData, sizeof(ExpansionData) );
		fExpansionData = NULL;
	}

    IOFWCommand::free();
}

void IOFireWireSBP2ManagementORB::release() const
{
	if( getRetainCount() >= 2 ) 
		IOFWCommand::release(2);
}

//
// accessors
//

// get / set command function

IOReturn IOFireWireSBP2ManagementORB::setCommandFunction( UInt32 function )
{
    // disallow login/reconnect/password/logout functions
    if( function == 0 || function == 3 || function == 4 || function == 7 )
    {
        return kIOReturnBadArgument;
    }
    else
    {
        fFunction = function;

        fManagementORB.options &= OSSwapHostToBigInt16(0xfff0);
        fManagementORB.options |= OSSwapHostToBigInt16(function | 0x8000);  // new and notify
    }

    return kIOReturnSuccess;
}

UInt32 IOFireWireSBP2ManagementORB::getCommandFunction( void )
{
    return fFunction;
}

// get / set managee command

void IOFireWireSBP2ManagementORB::setManageeCommand( OSObject * command )
{
    fManageeCommand = command;
}

OSObject* IOFireWireSBP2ManagementORB::getManageeCommand( void )
{
    return fManageeCommand;
}

// get / set response buffer

IOReturn IOFireWireSBP2ManagementORB::setResponseBuffer( void * buf, UInt32 len )
{
    IOReturn						status 			= kIOReturnSuccess;

    //
    // deallocate old response address space
    //

    if( fResponseAddressSpace != NULL )
    {
        fResponseAddressSpace->deactivate();
        fResponseAddressSpace->release();
    }
    
	fResponseBuf = buf;
	fResponseLen = len;

    //
    // allocate and register an address space for the response
    //

    if( status == kIOReturnSuccess && buf != NULL && len != 0 )
    {        
        fResponseAddressSpace = IOFWPseudoAddressSpace::simpleRW( fControl, 
																&fResponseAddress,
                                                                      len, buf );
        if ( fResponseAddressSpace == NULL )
            status = kIOReturnNoMemory;
			
		if( status == kIOReturnSuccess )
		{
			status = fResponseAddressSpace->activate();
		}
    }

    if( status != kIOReturnSuccess )
    {
        fResponseBuf = NULL;
        fResponseLen = 0;
    }

    return status;
}

void IOFireWireSBP2ManagementORB::getResponseBuffer( void ** buf, UInt32 * len )
{
    *buf = fResponseBuf;
    *len = fResponseLen;
}

IOReturn IOFireWireSBP2ManagementORB::setResponseBuffer
								( IOMemoryDescriptor * desc )
{
    IOReturn				status 			= kIOReturnSuccess;
    void * 					buf = NULL;
    UInt32 					len = 0;
    
    //
    // deallocate old response address space
    //

    if( fResponseAddressSpace != NULL )
    {
        fResponseAddressSpace->deactivate();
        fResponseAddressSpace->release();
    }

	if( fResponseMap != NULL )
	{
		fResponseMap->release();
	}
    
	if( desc != NULL )
    {
        len = desc->getLength();
		
		fResponseMap = desc->map();
		if( fResponseMap != NULL )
		{
			buf = (void*)fResponseMap->getVirtualAddress();
		}
     }
  
	fResponseBuf = buf;
	fResponseLen = len;

    //
    // allocate and register an address space for the response
    //

    if( status == kIOReturnSuccess && desc != NULL && buf != NULL && len != 0 )
    {
        fResponseAddressSpace = IOFWPseudoAddressSpace::simpleRW( fControl, 
																  &fResponseAddress,
																  desc );
        if ( fResponseAddressSpace == NULL )
            status = kIOReturnNoMemory;
			
		if( status == kIOReturnSuccess )
		{
			status = fResponseAddressSpace->activate();
		}
    }

    if( status != kIOReturnSuccess )
    {
        fResponseBuf = NULL;
        fResponseLen = 0;
    }

    return status;
}


//
// command execution
//

IOReturn IOFireWireSBP2ManagementORB::execute( void )
{
    FWKLOG( ( "IOFireWireSBP2ManagementORB<0x%08lx> : execute\n", (UInt32)this ) );
	IOReturn status = kIOReturnSuccess;
    
    IOFireWireSBP2Login * 	login 	= NULL;
    IOFireWireSBP2ORB * 	orb 	= NULL;
    
    switch( fFunction )
    {
        case kFWSBP2TargetReset:
        case kFWSBP2LogicalUnitReset:
        case kFWSBP2AbortTaskSet:
            login = OSDynamicCast( IOFireWireSBP2Login, fManageeCommand );
            if( login == NULL )
                status = kIOReturnBadArgument;
            break;

        case kFWSBP2AbortTask:
            orb = OSDynamicCast( IOFireWireSBP2ORB, fManageeCommand );
            if( orb == NULL )
                status = kIOReturnBadArgument;
            else
                login = orb->getLogin();
            break;

        case kFWSBP2QueryLogins:
            break;

        default:
            status = kIOReturnBadArgument;
    }

    // set login ID for all transactions except query logins
    if( status == kIOReturnSuccess && fFunction != kFWSBP2QueryLogins )
    {
        fManagementORB.loginID = OSSwapHostToBigInt16(login->getLoginID());
    }

    if( status == kIOReturnSuccess && fFunction == kFWSBP2AbortTask )
    {
        // set orb address
        FWAddress address;
        orb->getORBAddress( &address );
        fManagementORB.orbOffsetHi = OSSwapHostToBigInt32(0x0000ffff & address.addressHi);
        fManagementORB.orbOffsetLo = OSSwapHostToBigInt32(0xfffffffc & address.addressLo);

        // abort orb
		setORBToDummy( orb );
    }

    if( status == kIOReturnSuccess && fFunction == kFWSBP2QueryLogins )
    {
        FWSBP2QueryLoginsORB * queryLoginsORB = (FWSBP2QueryLoginsORB *)&fManagementORB;

        queryLoginsORB->lun = OSSwapHostToBigInt16(fLUN->getLUNumber());
        queryLoginsORB->queryResponseAddressHi = OSSwapHostToBigInt32(0x0000ffff & fResponseAddress.addressHi);
        queryLoginsORB->queryResponseAddressLo = OSSwapHostToBigInt32(fResponseAddress.addressLo);
        queryLoginsORB->queryResponseLength = OSSwapHostToBigInt16(fResponseLen);
    }
			
    if( status == kIOReturnSuccess )
    {
        fWriteCommand->reinit( FWAddress(0x0000ffff, 0xf0000000 + (fManagementOffset << 2)),
                               fWriteCommandMemory,
                               IOFireWireSBP2ManagementORB::writeCompleteStatic, this, true );
		
		status = fLUN->getTarget()->beginIOCriticalSection();
	}
   
	if( status == kIOReturnSuccess )
	{
		fExpansionData->fInCriticalSection = true;
		status = fWriteCommand->submit();
	}
	
    if( status != kIOReturnSuccess )
    {
		if( fExpansionData->fInCriticalSection )
		{
			fExpansionData->fInCriticalSection = false;
			fLUN->getTarget()->endIOCriticalSection();
		}
        
		return status;
    }
    
    return kIOReturnBusy;    // this means we are now busy working on this command
}

//
// write complete handler
//

void IOFireWireSBP2ManagementORB::writeCompleteStatic( void *refcon, IOReturn status, IOFireWireNub *device, IOFWCommand *fwCmd )
{
    ((IOFireWireSBP2ManagementORB*)refcon)->writeComplete( status, device, fwCmd );
}

void IOFireWireSBP2ManagementORB::writeComplete( IOReturn status, IOFireWireNub *device, IOFWCommand *fwCmd )
{
    FWKLOG( ( "IOFireWireSBP2ManagementORB<0x%08lx> : write complete\n", (UInt32)this ) );

    if( status == kIOReturnSuccess )
    {
        // we wrote the management agent, now set a timer and wait for response & status
        fTimeoutTimerSet = true;
		if( fTimeoutCommand->submit() != kIOReturnSuccess )
			fTimeoutTimerSet = false;
    }
    else
        complete( status );  // complete with error

}

//
// timeout handler
//

void IOFireWireSBP2ManagementORB::handleTimeoutStatic( void *refcon, IOReturn status, IOFireWireBus *bus, IOFWBusCommand *fwCmd )
{
    ((IOFireWireSBP2ManagementORB*)refcon)->handleTimeout( status, bus, fwCmd );
}

void IOFireWireSBP2ManagementORB::handleTimeout( IOReturn status, IOFireWireBus *bus, IOFWBusCommand *fwCmd )
{
    fTimeoutTimerSet = false;

    if( status == kIOReturnTimeout )
    {
        FWKLOG( ( "IOFireWireSBP2ManagementORB<0x%08lx> : handle timeout\n", (UInt32)this ) );
        complete( kIOReturnTimeout );
    }
}

//
// status block write handler
//

UInt32 IOFireWireSBP2ManagementORB::statusBlockWriteStatic( void *refcon, UInt16 nodeID, IOFWSpeed &speed, FWAddress addr,
                                                            UInt32 len, const void *buf, IOFWRequestRefCon lockRead )
{
    return ((IOFireWireSBP2ManagementORB*)refcon)->statusBlockWrite( nodeID, addr, len, buf, lockRead );
}

UInt32 IOFireWireSBP2ManagementORB::statusBlockWrite( UInt16 nodeID, FWAddress addr, UInt32 len, const void *buf,
                                                      IOFWRequestRefCon lockRead )
{
    // if timer isn't running we should not have been called.  don't panic...
    if( !fTimeoutTimerSet )
        return kFWResponseComplete;

    // cancel timeout
    fTimeoutCommand->cancel(kIOReturnAborted);

	if( fFunction == kFWSBP2AbortTaskSet ||
		fFunction == kFWSBP2TargetReset ||
		fFunction == kFWSBP2LogicalUnitReset )
	{
		// all tasks aborted once these babies complete
		fCompleting = true;
		clearAllTasksInSet();
		fCompleting = false;
	}

    // complete command
    complete( kIOReturnSuccess );    
	
    return kFWResponseComplete;
}

//
// suspendedNotify method
//
// called when a suspended message is received

void IOFireWireSBP2ManagementORB::suspendedNotify( void )
{
    FWKLOG( ( "IOFireWireSBP2ManagementORB<0x%08lx> : suspendedNotify\n", (UInt32)this ) );

	if( fStatus == kIOReturnBusy && !fCompleting )
	{
		if( fTimeoutTimerSet )
		{    
			// cancel timeout
			fTimeoutCommand->cancel( kIOReturnAborted );
		}

		// complete command
		complete( kIOFireWireBusReset );
	}
}

//
// complete
//

IOReturn IOFireWireSBP2ManagementORB::complete( IOReturn state )
{
    state = IOFWCommand::complete( state );
    FWKLOG( ( "IOFireWireSBP2ManagementORB<0x%08lx> : complete\n", (UInt32)this ) );
 
	if( fExpansionData->fInCriticalSection )
	{
		fExpansionData->fInCriticalSection = false;
		fLUN->getTarget()->endIOCriticalSection();
	}

	if( fCompletionCallback != NULL )
        (*fCompletionCallback)(fCompletionRefCon, state, this);

    return state;
}

// async ref handling for user client
//
//

void IOFireWireSBP2ManagementORB::setAsyncCallbackReference( void * asyncRef )
{
     bcopy( asyncRef, fCallbackAsyncRef, sizeof(OSAsyncReference64) );
}

void IOFireWireSBP2ManagementORB::getAsyncCallbackReference( void * asyncRef )
{
    bcopy( fCallbackAsyncRef, asyncRef, sizeof(OSAsyncReference64) );
}

//////////////////////////////////////////////////////////////////////////////////////////
// friend class wrappers

// IOFireWireSBP2LUN friend class wrappers

void IOFireWireSBP2ManagementORB::clearAllTasksInSet( void )
{ 
	fLUN->clearAllTasksInSet(); 
}

void IOFireWireSBP2ManagementORB::removeManagementORB( IOFireWireSBP2ManagementORB * orb )
{ 
	fLUN->removeManagementORB( orb ); 
}

// IOFireWireSBP2ORB friend class wrappers

void IOFireWireSBP2ManagementORB::setORBToDummy( IOFireWireSBP2ORB * orb )
{
	orb->setToDummy();
}
