/*
 * Copyright (c) 1998-2001 Apple Computer, Inc. All rights reserved.
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

#include <IOKit/scsi-commands/IOSCSIProtocolInterface.h>

// For debugging, set SCSI_PROTOCOL_INTERFACE_DEBUGGING_LEVEL to one
// of the following values:
//		0	No debugging 	(GM release level)
// 		1 	PANIC_NOW only
//		2	PANIC_NOW and ERROR_LOG
//		3	PANIC_NOW, ERROR_LOG and STATUS_LOG


#if ( SCSI_PROTOCOL_INTERFACE_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_PROTOCOL_INTERFACE_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_PROTOCOL_INTERFACE_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif

#define SERIAL_STATUS_LOG(x)	STATUS_LOG(x)


#define super IOService
OSDefineMetaClass ( IOSCSIProtocolInterface, IOService );
OSDefineAbstractStructors ( IOSCSIProtocolInterface, IOService );


enum
{
	kThreadDelayInterval		= 500, //( 0.5 second in ms )
	k100SecondsInMicroSeconds 	= 100 * 1000 * 1000
};


/*
 *	Since most, if not all, IOSCSIArchitectureModelFamily drivers
 *	must support power management in some way or another, the support
 *	has been moved up into the highest layers of the object hierarchy.
 *	This mechanism allows for many different power management schemes,
 *	depending on how the specification defines power management. Each
 *	protocol layer and application layer driver is responsible for
 *	implementing power management as the governing specification defines
 *	it. From there, developers can subclass and add any vendor-specific
 *	workarounds necessary for power management or they can make the
 *	power management scheme more elaborate or more simplistic.
 *
 */


//---------------------------------------------------------------------------
// ¥ start - Used to allocate any resources
//---------------------------------------------------------------------------

bool
IOSCSIProtocolInterface::start ( IOService * provider )
{
	
	IOWorkLoop *	workLoop;
	
	STATUS_LOG ( ( "IOSCSIProtocolInterface::start.\n" ) );
	
	workLoop = getWorkLoop ( );
	
	if ( workLoop != NULL )
	{
		
		fCommandGate = IOCommandGate::commandGate ( this );
		
		if ( fCommandGate != NULL )
		{
			
			workLoop->addEventSource ( fCommandGate );
			
		}
		
		else
		{
			return false;
		}
		
	}
	
	else
	{
		return false;
	}
	
	fPowerAckInProgress			= false;
	fPowerTransitionInProgress 	= false;
	fPowerManagementInitialized = false;
	
	// Allocate the thread on which to do power management
	fPowerManagementThread = thread_call_allocate (
					( thread_call_func_t ) IOSCSIProtocolInterface::sPowerManagement,
					( thread_call_param_t ) this );
	
	if ( fPowerManagementThread == NULL )
	{
		
		STATUS_LOG ( ( "%s::thread allocation failed.\n", getName ( ) ) );
		return false;
		
	}
	
	return true;
	
}


//---------------------------------------------------------------------------
// ¥ free - Used to deallocate any resources
//---------------------------------------------------------------------------

void
IOSCSIProtocolInterface::free ( void )
{
	
	IOWorkLoop *	workLoop;
	
	if ( fCommandGate != NULL )
	{
		
		workLoop = getWorkLoop ( );
		if ( workLoop != NULL )
		{
			
			// Remove our command gate as an event source on
			// the workloop
			workLoop->removeEventSource ( fCommandGate );
			
		}
		
		fCommandGate->release ( );
		fCommandGate = NULL;
		
	}
	
	if ( fPowerManagementThread != NULL )
	{
		
		// Free the thread we allocated for power management
		thread_call_free ( fPowerManagementThread );
		fPowerManagementThread = NULL;
		
	}
	
	super::free ( );

}


#pragma mark -
#pragma mark - Power Management Support


//---------------------------------------------------------------------------
// ¥ initialPowerStateForDomainState - 	Returns to the power manager what initial
//										state the device should be in.
//---------------------------------------------------------------------------

UInt32
IOSCSIProtocolInterface::initialPowerStateForDomainState ( IOPMPowerFlags flags )
{
	
	STATUS_LOG ( ( "%s::%s\n", getName ( ), __FUNCTION__ ) );
	
	// We ignore the flags since we are always active at startup time and we
	// just report what our initial power state is.
	return GetInitialPowerState ( );
	
}


//---------------------------------------------------------------------------
// ¥ InitializePowerManagement - Joins PM tree and initializes pm vars.
//---------------------------------------------------------------------------

void
IOSCSIProtocolInterface::InitializePowerManagement ( IOService * provider )
{
	
	STATUS_LOG ( ( "IOSCSIProtocolInterface::InitializePowerManagement.\n" ) );
	
	PMinit ( );
	provider->joinPMtree ( this );
	setIdleTimerPeriod ( 5 * 60 ); 	// set 5 minute idle timer until system sends us
									// new values via setAggressiveness()
	
	// Call makeUsable here to tell the power manager to put us in our
	// highest power state when we call registerPowerDriver().
	makeUsable ( );
	fPowerManagementInitialized = true;
	
}


//---------------------------------------------------------------------------
// ¥ IsPowerManagementIntialized - Returns fPowerManagementInitialized.
//---------------------------------------------------------------------------

bool
IOSCSIProtocolInterface::IsPowerManagementIntialized ( void )
{
	
	STATUS_LOG ( ( "IOSCSIProtocolInterface::IsPowerManagementIntialized.\n" ) );
	return fPowerManagementInitialized;
	
}


//---------------------------------------------------------------------------
// ¥ finalize - Terminates all power management.
//---------------------------------------------------------------------------

bool
IOSCSIProtocolInterface::finalize ( IOOptionBits options )
{
	
	STATUS_LOG ( ( "IOSCSIProtocolInterface::finalize this = %p\n", this ) );
	
	if ( fPowerManagementInitialized )
	{
		bool	powerTransistionStillInProgress  =  true;
		
		// need to wait for power transistion in progress to
		// finish before finalizing or the transistion will
		// attempt to complete after we are gone
		for ( ; ; )
		{
			
			powerTransistionStillInProgress = ( bool ) fCommandGate->runAction ( ( IOCommandGate::Action )
							&IOSCSIProtocolInterface::sGetPowerTransistionInProgress );
			
			if ( powerTransistionStillInProgress )
			{
				IOSleep ( 1 );
			}
			else
			{
				break;
			}
			
		}

		fCommandGate->commandWakeup ( &fCurrentPowerState, false );
		fCommandGate->runAction ( ( IOCommandGate::Action )
							&IOSCSIProtocolInterface::sGetPowerTransistionInProgress );			

		STATUS_LOG ( ( "PMstop about to be called.\n" ) );

		PMstop ( );
		
		if ( fPowerManagementThread != NULL )
		{
			
			// If the power management thread is scheduled, unschedule it.
			thread_call_cancel ( fPowerManagementThread );
			
		}
		
		fPowerManagementInitialized = false;

	}
	
	return super::finalize ( options );

}


//---------------------------------------------------------------------------
// ¥ sGetPowerTransistionInProgress - 
//---------------------------------------------------------------------------

bool
IOSCSIProtocolInterface::sGetPowerTransistionInProgress ( IOSCSIProtocolInterface * self )
{
	return ( self->fPowerTransitionInProgress );
}


//---------------------------------------------------------------------------
// ¥ setPowerState - Set/Change the power state of the ATA hard-drive
//---------------------------------------------------------------------------

IOReturn
IOSCSIProtocolInterface::setPowerState (
							UInt32	  powerStateOrdinal,
							IOService * whichDevice )
{
		
	SERIAL_STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	SERIAL_STATUS_LOG ( ( "powerStateOrdinal = %ld\n", powerStateOrdinal ) );
	
	return fCommandGate->runAction ( ( IOCommandGate::Action )
							&IOSCSIProtocolInterface::sHandleSetPowerState,
							( void * ) powerStateOrdinal );
	
}


//---------------------------------------------------------------------------
// ¥ sHandleSetPowerState - Static member function called by the
//							IOCommandGate to translate setPowerState() calls
//							to the synchronized handleSetPowerState() call
//---------------------------------------------------------------------------

IOReturn
IOSCSIProtocolInterface::sHandleSetPowerState (
								IOSCSIProtocolInterface * 	self,
								UInt32		 				powerStateOrdinal )
{
	
	IOReturn returnValue = 0;
	
	if ( self->isInactive ( ) == false )
	{ 
		
		self->HandleSetPowerState ( powerStateOrdinal );
		returnValue = k100SecondsInMicroSeconds;
		
	}
	
	return returnValue;
	
}


//---------------------------------------------------------------------------
// ¥ HandleSetPowerState - Synchronized form of changing a power state
//---------------------------------------------------------------------------

void
IOSCSIProtocolInterface::HandleSetPowerState ( UInt32 powerStateOrdinal )
{
	
	AbsoluteTime	time;

	SERIAL_STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	fProposedPowerState = powerStateOrdinal;

	if ( ( fPowerTransitionInProgress == false ) || fPowerAckInProgress )
	{

		STATUS_LOG ( ( "IOSCSIProtocolInterface::%s starting transition\n", __FUNCTION__ ) );

		// mark us as being in progress, then call the thread which is
		// the power management state machine
		fPowerTransitionInProgress = true;
		
		// We use a delayed thread call in order to allow the thread we're
		// on (the power manager thread) to get back to business doing
		// whatever it needs to with the rest of the system
		clock_interval_to_deadline ( kThreadDelayInterval, kMillisecondScale, &time );
		( void ) thread_call_enter_delayed ( fPowerManagementThread, time );
		
	}

	STATUS_LOG ( ( "IOSCSIProtocolInterface::%s exiting\n", __FUNCTION__ ) );
	
}


//---------------------------------------------------------------------------
// ¥ sPowerManagement - Called on its own thread to handle power management
//---------------------------------------------------------------------------

void
IOSCSIProtocolInterface::sPowerManagement ( thread_call_param_t whichDevice )
{
	
	IOSCSIProtocolInterface *	self;
	
	STATUS_LOG ( ( "IOSCSIProtocolInterface::sPowerManagement called\n" ) );
	
	self = ( IOSCSIProtocolInterface * ) whichDevice;
	if ( self != NULL && ( self->isInactive ( ) == false ) )
	{
		
		self->retain ( );
		self->HandlePowerChange ( );
		
		self->fPowerAckInProgress = true;	
		
		self->acknowledgeSetPowerState ( );
		
		self->fPowerAckInProgress = false;
		self->fPowerTransitionInProgress = false;
		self->release ( );
		
	}
	else
        {
            
            self->fPowerTransitionInProgress = false;
            
        }
}


//---------------------------------------------------------------------------
// ¥ CheckPowerState - 	Checks if it is ok for I/O to come through. If it is
//						not ok, then we block the thread.
//---------------------------------------------------------------------------

void
IOSCSIProtocolInterface::CheckPowerState ( void )
{
	
	SERIAL_STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	// Tell the power manager we must be in active state to handle requests
	// "active" state means the minimal possible state in which the driver can
	// handle I/O. This may be set to standby, but there is no gain to setting
	// the drive to standby and then issuing an I/O, it just requires more time.
	// Also, if the drive was asleep, it might need a reset which could put it
	// in standby mode anyway, so we usually request the max state from the power
	// manager 
	TicklePowerManager ( );
	
	// Now run an action behind a command gate to block the threads if necessary
	fCommandGate->runAction ( ( IOCommandGate::Action )
						  		&IOSCSIProtocolInterface::sHandleCheckPowerState );
	
}


//---------------------------------------------------------------------------
// ¥ sHandleCheckPowerState - 	Static routine which calls through to other
//								side of command gate
//---------------------------------------------------------------------------

void
IOSCSIProtocolInterface::sHandleCheckPowerState ( IOSCSIProtocolInterface * self )
{
	
	self->HandleCheckPowerState ( );
	
}


//---------------------------------------------------------------------------
// ¥ HandleCheckPowerState - 	Called on safe side of command gate to serialize
//								access to the sleep related variables
//---------------------------------------------------------------------------

void
IOSCSIProtocolInterface::HandleCheckPowerState ( UInt32 maxPowerState )
{

	STATUS_LOG ( ( "IOSCSIProtocolInterface::%s called\n", __FUNCTION__ ) );
	
	// A while loop is used here in case the power is changed on us while
	// the power state-machine is running. If the power manager tells us to
	// wake up, we unblock the thread by calling fCommandGate->commandWakeup, but
	// while we do this we may block and inbetween getting rescheduled, the
	// power manager might tell us to change states again, so we guard against
	// this scenario by looping and verifying that the state does indeed go to
	// active.
	while ( fCurrentPowerState != maxPowerState )
	{
		
		fCommandGate->commandSleep ( &fCurrentPowerState, THREAD_UNINT );
		
	}

}


//---------------------------------------------------------------------------
// ¥ TicklePowerManager - 	Convenience function to call power manager's
//							activity tickle routine.
//---------------------------------------------------------------------------

bool
IOSCSIProtocolInterface::TicklePowerManager ( UInt32 maxPowerState )
{

	STATUS_LOG ( ( "IOSCSIProtocolInterface::%s called\n", __FUNCTION__ ) );
	
	// Tell the power manager we must be in active state to handle requests
	// "active" state means the minimal possible state in which the driver can
	// handle I/O. This may be set to standby, but there is no gain to setting
	// the drive to standby and then issuing an I/O, it just requires more time.
	// Also, if the drive was asleep, it might need a reset which could put it
	// in standby mode anyway, so we usually request the max state from the power
	// manager 
	return ( activityTickle ( kIOPMSuperclassPolicy1, maxPowerState ) );
	
}


//---------------------------------------------------------------------------
// ¥ GetUserClientExclusivityState - Call to see if user client is in
//									 exclusive mode.
//---------------------------------------------------------------------------

bool
IOSCSIProtocolInterface::GetUserClientExclusivityState ( void )
{
	
	bool	state;
	
	fCommandGate->runAction ( ( IOCommandGate::Action )
							&IOSCSIProtocolInterface::sGetUserClientExclusivityState,
							( void * ) &state );
	return state;
	
}


//---------------------------------------------------------------------------
// ¥ sGetUserClientExclusivityState - C->C++ glue.
//---------------------------------------------------------------------------

void
IOSCSIProtocolInterface::sGetUserClientExclusivityState ( IOSCSIProtocolInterface * self,
														  bool * state )
{
	
	*state = self->HandleGetUserClientExclusivityState ( );
	
}


//---------------------------------------------------------------------------
// ¥ HandleGetUserClientExclusivityState - Checks to see what state the user client is in
//							( exclusive vs. non-exclusive ).
//---------------------------------------------------------------------------

bool
IOSCSIProtocolInterface::HandleGetUserClientExclusivityState ( void )
{
	
	STATUS_LOG ( ( "IOSCSIProtocolInterface::%s called\n", __FUNCTION__ ) );	
	return fUserClientExclusiveControlled;
	
}

//---------------------------------------------------------------------------
// ¥ SetUserClientExclusivityState - Call to see if user client is in
//									 exclusive mode.
//---------------------------------------------------------------------------

IOReturn
IOSCSIProtocolInterface::SetUserClientExclusivityState ( IOService * userClient, bool state )
{
	
	IOReturn	status = kIOReturnExclusiveAccess;
	
	fCommandGate->runAction ( ( IOCommandGate::Action )
							&IOSCSIProtocolInterface::sSetUserClientExclusivityState,
							( void * ) &status,
							( void * ) userClient,
							( void * ) state );
	
	return status;
	
}


//---------------------------------------------------------------------------
// ¥ sSetUserClientExclusivityState - C->C++ glue.
//---------------------------------------------------------------------------

void
IOSCSIProtocolInterface::sSetUserClientExclusivityState ( IOSCSIProtocolInterface * self,
														  IOReturn * status,
														  IOService * userClient,
														  bool state )
{
	
	*status = self->HandleSetUserClientExclusivityState ( userClient, state );
	
}


//---------------------------------------------------------------------------
// ¥ HandleSetUserClientExclusivityState - Sets the state the user client is in
//							( exclusive vs. non-exclusive ).
//---------------------------------------------------------------------------

IOReturn
IOSCSIProtocolInterface::HandleSetUserClientExclusivityState ( IOService * userClient,
																bool state )
{
	
	IOReturn		status = kIOReturnExclusiveAccess;
	
	STATUS_LOG ( ( "IOSCSIProtocolInterface::%s called\n", __FUNCTION__ ) );	
	
	if ( fUserClient == NULL )
	{
		
		STATUS_LOG ( ( "fUserClient is NULL\n" ) );
		
		if ( state != false )
		{
			
			STATUS_LOG ( ( "state is true\n" ) );
			fUserClient = userClient;
			fUserClientExclusiveControlled = state;
			status = kIOReturnSuccess;
			
		}
		
		else
		{
			
			STATUS_LOG ( ( "state is false\n" ) );
			status = kIOReturnBadArgument;
			
		}
		
	}
	
	else if ( fUserClient == userClient )
	{
		
		STATUS_LOG ( ( "fUserClient is same as userClient\n" ) );
		
		if ( state == false )
		{
			
			STATUS_LOG ( ( "state is false\n" ) );
			fUserClient = NULL;
			fUserClientExclusiveControlled = state;
			status = kIOReturnSuccess;
			
		}
		
		status = kIOReturnSuccess;
		
	}
	
	STATUS_LOG ( ( "HandleSetUserClientExclusivityState status = %d\n", status ) );
	
	return status;
	
}

// Space reserved for future expansion.
OSMetaClassDefineReservedUnused( IOSCSIProtocolInterface, 1 );
OSMetaClassDefineReservedUnused( IOSCSIProtocolInterface, 2 );
OSMetaClassDefineReservedUnused( IOSCSIProtocolInterface, 3 );
OSMetaClassDefineReservedUnused( IOSCSIProtocolInterface, 4 );
OSMetaClassDefineReservedUnused( IOSCSIProtocolInterface, 5 );
OSMetaClassDefineReservedUnused( IOSCSIProtocolInterface, 6 );
OSMetaClassDefineReservedUnused( IOSCSIProtocolInterface, 7 );
OSMetaClassDefineReservedUnused( IOSCSIProtocolInterface, 8 );
OSMetaClassDefineReservedUnused( IOSCSIProtocolInterface, 9 );
OSMetaClassDefineReservedUnused( IOSCSIProtocolInterface, 10 );
OSMetaClassDefineReservedUnused( IOSCSIProtocolInterface, 11 );
OSMetaClassDefineReservedUnused( IOSCSIProtocolInterface, 12 );
OSMetaClassDefineReservedUnused( IOSCSIProtocolInterface, 13 );
OSMetaClassDefineReservedUnused( IOSCSIProtocolInterface, 14 );
OSMetaClassDefineReservedUnused( IOSCSIProtocolInterface, 15 );
OSMetaClassDefineReservedUnused( IOSCSIProtocolInterface, 16 );
