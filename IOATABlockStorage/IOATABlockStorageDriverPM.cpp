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
 
/*
 * Copyright (c) 2000-2001 Apple Computer, Inc.  All rights reserved. 
 *
 * IOATABlockStorageDevicePM.cpp - Power management support.
 *
 * HISTORY
 * 	2000.12.11	CJS  	Added power management to IOATABlockStorage driver
 */

/*
 *	ATA power management as defined in T13:1321-D Revision 3 2000.02.29,
 *	AT Attachment with Packet Interface - 5 (ATA/ATAPI-5), states that there
 *	are four (4) basic power states: Sleep, Standby, Idle, and Active. This
 *	implementation does not yet include support for the Advanced Power
 *	Management modes defined in the ATA-5 specification.
 *
 */

#include "IOATABlockStorageDriver.h"
#define	super IOService


#if ( ATA_BLOCK_STORAGE_DRIVER_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)			IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( ATA_BLOCK_STORAGE_DRIVER_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)			IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( ATA_BLOCK_STORAGE_DRIVER_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)			IOLog x
#else
#define STATUS_LOG(x)
#endif

#if ( ATA_BLOCK_STORAGE_DRIVER_PM_DEBUGGING_LEVEL >= 1 )
#define SERIAL_STATUS_LOG(x) 	kprintf x
#else
#define SERIAL_STATUS_LOG(x)
#endif


enum
{
	kThreadDelayInterval		= 500, //( 0.5 second in ms )
	k100SecondsInMicroSeconds 	= 100 * 1000 * 1000
};

enum
{
	kATAIdleDevice		= 1,
	kATAStandbyDevice	= 2,
	kATASleepDevice		= 3
};

enum
{
	kPowerManagementScaleFactor = kSecondsInAMinute / ( kIOATAPowerStateActive - kIOATAPowerStateSleep )
};


//---------------------------------------------------------------------------
// Inform the policy-maker that an ATA hard-drive is capable of five power
// states. The sPowerStates[] array encodes information about each state
//---------------------------------------------------------------------------


static IOPMPowerState sPowerStates[kIOATAPowerStates] =
{
	{ kIOPMPowerStateVersion1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },			/* never entered voluntarily */
	{ kIOPMPowerStateVersion1, 0, IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ kIOPMPowerStateVersion1, (IOPMDeviceUsable | kIOPMPreventIdleSleep), IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ kIOPMPowerStateVersion1, (IOPMDeviceUsable | kIOPMPreventIdleSleep), IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ kIOPMPowerStateVersion1, (IOPMDeviceUsable | IOPMMaxPerformance | kIOPMPreventIdleSleep), IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 }
};

#if ( ATA_MASS_STORAGE_DEBUG == 1 )
static const char * sPowerStateNames[] =
{
	"System Sleep",
	"Sleep",
	"Standby",
	"Idle",
	"Active"
};
#endif	/* ATA_MASS_STORAGE_DEBUG */


//---------------------------------------------------------------------------
// ¥ initialPowerStateForDomainState - 	Returns to the power manager what
//										initial state the device should be in.
//---------------------------------------------------------------------------

UInt32
IOATABlockStorageDriver::initialPowerStateForDomainState (
											IOPMPowerFlags	flags )
{
	
	// We ignore the flags since we are always active at startup time and we
	// just report what our initial power state is.
	return kIOATAPowerStateActive;
	
}


//---------------------------------------------------------------------------
// ¥ initForPM - 	Register the driver with our policy-maker
//					(also in the same class).
//---------------------------------------------------------------------------

void
IOATABlockStorageDriver::initForPM ( void )
{
	
	SERIAL_STATUS_LOG ( ( "IOATABlockStorageDriver::initForPM called\n" ) );
	
	registerPowerDriver ( this, sPowerStates, kIOATAPowerStates );
	SERIAL_STATUS_LOG ( ( "registerPowerDriver\n" ) );
	
	changePowerStateTo ( kIOATAPowerStateSleep );
	SERIAL_STATUS_LOG ( ( "changePowerStateTo\n" ) );
	
}


//---------------------------------------------------------------------------
// ¥ setAggressiveness -	Policy-maker code which intercepts
//							kPMMinutesToSpinDown settings and then call
//							setIdleTimerPeriod() to adjust the idle timer.
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver::setAggressiveness ( UInt32 type, UInt32 minutes )
{
	
	SERIAL_STATUS_LOG ( ( "IOATABlockStorageDriver::setAggressiveness called\n" ) );
	
	if ( type == kPMMinutesToSpinDown )
	{
		
		SERIAL_STATUS_LOG ( ( "IOATABlockStorageDriver: setting idle timer to %ld min\n", minutes ) );
		setIdleTimerPeriod ( minutes * kPowerManagementScaleFactor );
		
	}
	
	return ( super::setAggressiveness ( type, minutes ) );
	
}


//---------------------------------------------------------------------------
// ¥ checkPowerState - 	Checks if it is ok for I/O to come through. If it is
//						not ok, then we block the thread.
//---------------------------------------------------------------------------

void
IOATABlockStorageDriver::checkPowerState ( void )
{
	
	SERIAL_STATUS_LOG ( ( "IOATABlockStorageDriver::checkPowerState called\n" ) );
	
	activityTickle ( kIOPMSuperclassPolicy1, ( UInt32 ) kIOATAPowerStateActive );
	
	fCommandGate->runAction ( ( IOCommandGate::Action )
								&IOATABlockStorageDriver::sHandleCheckPowerState );
	
}


//---------------------------------------------------------------------------
// ¥ sHandleCheckPowerState - 	Static routine which calls through to other
//								side of command gate
//---------------------------------------------------------------------------

void
IOATABlockStorageDriver::sHandleCheckPowerState ( IOATABlockStorageDriver * self )
{
	
	self->handleCheckPowerState ( );
	
}


//---------------------------------------------------------------------------
// ¥ handleCheckPowerState - 	Called on safe side of command gate to serialize
//								access to the sleep related variables
//---------------------------------------------------------------------------

void
IOATABlockStorageDriver::handleCheckPowerState ( void )
{
	
	while ( fCurrentPowerState != kIOATAPowerStateActive )
	{
		
		fCommandGate->commandSleep ( &fCurrentPowerState, THREAD_UNINT );
		
	}
	
	fNumCommandsOutstanding++;
	
}


//---------------------------------------------------------------------------
// ¥ setPowerState - Set/Change the power state of the ATA hard-drive
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver::setPowerState (
							UInt32      powerStateOrdinal,
							IOService * whichDevice )
{
	
	SERIAL_STATUS_LOG ( ( "IOATABlockStorageDriver::setPowerState called\n" ) );
	SERIAL_STATUS_LOG ( ( "powerStateOrdinal = %ld\n", powerStateOrdinal ) );
		
	fCommandGate->runAction ( ( IOCommandGate::Action )
								&IOATABlockStorageDriver::sHandleSetPowerState,
								( void * ) powerStateOrdinal );
	
	return k100SecondsInMicroSeconds;
	
}


//---------------------------------------------------------------------------
// ¥ sHandleSetPowerState - Static member function called by the
//							IOCommandGate to translate setPowerState() calls
//							to the synchronized handleSetPowerState() call
//---------------------------------------------------------------------------

void
IOATABlockStorageDriver::sHandleSetPowerState (
								IOATABlockStorageDriver * 	self,
								UInt32         				powerStateOrdinal )
{
	
	if ( self->isInactive ( ) == false )
	{ 
		
		self->handleSetPowerState ( powerStateOrdinal );
		
	}
	
}


//---------------------------------------------------------------------------
// ¥ handleSetPowerState - Synchronized form of changing a power state
//---------------------------------------------------------------------------

void
IOATABlockStorageDriver::handleSetPowerState ( UInt32 powerStateOrdinal )
{
	
	AbsoluteTime	time;
	
	fProposedPowerState = powerStateOrdinal;
	if ( ( fPowerTransitionInProgress == false ) || fPowerAckInProgress )
	{
		
		// mark us as being in progress, then call the thread which is
		// the power management state machine
		fPowerTransitionInProgress = true;
		
		clock_interval_to_deadline ( kThreadDelayInterval, kMillisecondScale, &time );
		( void ) thread_call_enter_delayed ( fPowerManagementThread, time );
		
	}
	
}


//---------------------------------------------------------------------------
// ¥ sPowerManagement - Called on its own thread to handle power management
//---------------------------------------------------------------------------

void
IOATABlockStorageDriver::sPowerManagement ( thread_call_param_t whichDevice )
{
	
	IOATABlockStorageDriver *	self;
	
	self = ( IOATABlockStorageDriver * ) whichDevice;
	if ( ( self != NULL ) && ( self->isInactive ( ) == false ) )
	{
		
		self->retain ( );
		self->handlePowerChange ( );
		
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
// ¥ handlePowerChange - Handles the state machine for power management
//---------------------------------------------------------------------------

void
IOATABlockStorageDriver::handlePowerChange ( void )
{
	
	UInt32		count = 0;
        
	SERIAL_STATUS_LOG ( ( "IOATABlockStorageDriver::handlePowerChange called\n" ) );
	
	while ( fProposedPowerState != fCurrentPowerState )
	{
	 	
		// If we've gone through this loop a ton of times, there's something
		// wrong, so break out to avoid an infinite loop.
		if ( count > 32 )
		{
			break;
		}
                
	 	// We look at what state has been proposed. In some cases, the device
	 	// may be asleep and we need to hit it with a reset before issuing
	 	// the other power change command. The reset brings the device back
	 	// into standby mode.
	 	
		switch ( fProposedPowerState )
		{
			
			case kIOATAPowerStateSystemSleep:
				if ( fCurrentPowerState == kIOATAPowerStateSleep )
				{
					fCurrentPowerState = kIOATAPowerStateSystemSleep;
					break;
				}
				
				// else intentionally fall through to kIOATAPowerStateSleep
			
			case kIOATAPowerStateSleep:
			{
				
				bool	resetOccurred = false;
				// We never have to check if we are in sleep here since that
				// is handled by the while loop check. So we can always just
				// send the sleep command straight on through.
				fCurrentPowerState = fProposedPowerState;
				
				fCommandGate->runAction ( ( IOCommandGate::Action )
										  &IOATABlockStorageDriver::sSetWakeupResetOccurred,
										  ( void * ) resetOccurred );
								
				while ( fNumCommandsOutstanding != 0 )
				{
					
					// Sleep this thread for 1ms until all outstanding commands
					// have completed. This prevents a sleep command from entering
					// the queue before an I/O and causing a problem on wakeup.
					IOSleep ( 1 );
					
				}
				
				( void ) issuePowerTransition ( kATASleepDevice );
				
			}	
				break;
			
			case kIOATAPowerStateStandby:
			{
				
				if ( fCurrentPowerState <= kIOATAPowerStateSleep )
				{
					
					// issue the reset command
					( void ) resetATADevice ( );
					fCurrentPowerState = kIOATAPowerStateStandby;
					
				}
				
				else if ( fCurrentPowerState != kIOATAPowerStateStandby )
				{
					
					// Issue a STANDBY IMMED command to the drive
					( void ) issuePowerTransition ( kATAStandbyDevice );
					fCurrentPowerState = kIOATAPowerStateStandby;
					
				}
				
			}
				break;
			
			case kIOATAPowerStateIdle:
			{
				
				if ( fCurrentPowerState <= kIOATAPowerStateSleep )
				{
					
					// issue the reset command
					( void ) resetATADevice ( );
					fCurrentPowerState = kIOATAPowerStateStandby;
					
				}
				
				if ( fCurrentPowerState != kIOATAPowerStateIdle )
				{
					
					// Issue an IDLE IMMED command to the drive
					( void ) issuePowerTransition ( kATAIdleDevice );
					fCurrentPowerState = kIOATAPowerStateIdle;
										
				}
			
			}
				break;
			
			case kIOATAPowerStateActive:
			{
								
				if ( fCurrentPowerState <= kIOATAPowerStateSleep )
				{
					
					bool	resetOccurred = false;
					
					fCommandGate->runAction ( ( IOCommandGate::Action )
											&IOATABlockStorageDriver::sCheckWakeupResetOccurred,
											( void * ) &resetOccurred );
					
					// Check to see if a reset already occurred. Since there might be
					// a case where master/slave causes the reset to occur for both
					// devices, we don't want to do two resets on the bus.
					if ( resetOccurred == false )
					{
						
						( void ) resetATADevice ( );
						
					}
					
				}
				
				// We lie and say we are active, even though
				// we are in standby mode after a reset. It doesn't
				// really matter, since the next I/O will put
				// us in active mode. This avoids doing an I/O
				// and discarding the result.
				fCurrentPowerState = kIOATAPowerStateActive;
				
				// wake up any pending I/O
				fCommandGate->commandWakeup ( &fCurrentPowerState, false );
				
			}
				break;
			
			default:
				PANIC_NOW ( ( "Undefined power state issued\n" ) );
				break;
			
		}
		
		count++;
		
	}
	
}


//---------------------------------------------------------------------------
// ¥ issuePowerTransition - Issues a power change command
//---------------------------------------------------------------------------

IOReturn
IOATABlockStorageDriver::issuePowerTransition ( UInt32 function )
{
	
	IOReturn	status = kIOReturnSuccess;
	IOSyncer *	syncer;

	// Check if the device is CompactFlash. We wonÕt do power transitions for
	// CF since they don't seem to like them.
	if ( fATASocketType == kPCCardSocket )
	{
		return status;
		
	}	
	
	// We have found several drives that will hang the bus when going from
	// kATAStandbyDevice or kATAIdleDevice to kATASleepDevice. For now
	// to be safe we are going to simplify power transitions to kATASleepDevice
	if ( ( function == kATAStandbyDevice ) || ( function == kATAIdleDevice ) )
	{
		
		return status;
		
	}
	
	syncer = ( IOSyncer * ) fPowerManagementCommand->refCon;
	
	// Zero the command
	fPowerManagementCommand->zeroCommand ( );
	fPowerManagementCommand->setUnit ( fATAUnitID );	
	fPowerManagementCommand->setTimeoutMS ( kATATimeout10Seconds );
	fPowerManagementCommand->setDevice_Head ( fATAUnitID << 4 );
	fPowerManagementCommand->setOpcode ( kATAFnExecIO );
	fPowerManagementCommand->setCallbackPtr (
					&IOATABlockStorageDriver::sHandleSimpleSyncTransaction );
	
	switch ( function )
	{
		
		case kATAIdleDevice:
			fPowerManagementCommand->setCommand ( kATAcmdIdleImmed );
			break;
		
		case kATAStandbyDevice:
			fPowerManagementCommand->setCommand ( kATAcmdStandbyImmed );
			break;
		
		case kATASleepDevice:
			fPowerManagementCommand->setCommand ( kATAcmdSleep );
			break;
		
		default:
			PANIC_NOW ( ( "Invalid command in IOATABlockStorageDriver::issuePowerTransition\n" ) );
			break;
		
	}
	
	fATADevice->executeCommand ( fPowerManagementCommand );
	
	status = syncer->wait ( false );
	syncer->reinit ( );	
		
	return status;
	
}


//--------------------------------------------------------------------------------------
//	¥ setWakeupResetOccurred	-	Called on safe side of command gate to set state
//--------------------------------------------------------------------------------------

void
IOATABlockStorageDriver::setWakeupResetOccurred ( bool resetOccurred )
{
	
	fWakeUpResetOccurred = resetOccurred;
	
}


//--------------------------------------------------------------------------------------
//	¥ sSetWakeupResetOccurred	-	Called on safe side of command gate
//--------------------------------------------------------------------------------------

void
IOATABlockStorageDriver::sSetWakeupResetOccurred ( IOATABlockStorageDriver * driver,
												    bool resetOccurred )
{
	
	driver->setWakeupResetOccurred ( resetOccurred );
	
}


//--------------------------------------------------------------------------------------
//	¥ checkWakeupResetOccurred	-	Called on safe side of command gate to find out if
//									the driver has already received a reset message
//--------------------------------------------------------------------------------------

bool
IOATABlockStorageDriver::checkWakeupResetOccurred ( void )
{
		
	return fWakeUpResetOccurred;
	
}


//--------------------------------------------------------------------------------------
//	¥ sCheckWakeupResetOccurred	-	Called on safe side of command gate
//--------------------------------------------------------------------------------------

void
IOATABlockStorageDriver::sCheckWakeupResetOccurred ( IOATABlockStorageDriver * driver,
												 	  void * resetOccurred )
{
	
	bool *	reset = ( bool * ) resetOccurred;
	
	*reset = driver->checkWakeupResetOccurred ( );
	return;
	
}


//---------------------------------------------------------------------------
//					End				Of				File
//---------------------------------------------------------------------------