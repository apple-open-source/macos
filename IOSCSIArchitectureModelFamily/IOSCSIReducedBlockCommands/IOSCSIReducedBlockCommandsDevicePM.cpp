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
 
#include <IOKit/scsi-commands/SCSITask.h>
#include <IOKit/IOMessage.h>
#include <IOKit/pwr_mgt/RootDomain.h>
#include "IOSCSIReducedBlockCommandsDevice.h"


#if ( SCSI_RBC_DEVICE_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_RBC_DEVICE_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_RBC_DEVICE_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif


#define super IOSCSIPrimaryCommandsDevice

//---------------------------------------------------------------------------
// We have 4 default power states. ACTIVE, IDLE, STANDBY, and SLEEP in order
// of most power to least power. In SLEEP, the Vcc can be removed, so we must
// trap for that in our power management code.
//---------------------------------------------------------------------------

static IOPMPowerState sPowerStates[kRBCNumPowerStates] =
{
	{ kIOPMPowerStateVersion1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },			/* never entered voluntarily */
	{ kIOPMPowerStateVersion1, 0, IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ kIOPMPowerStateVersion1, (IOPMDeviceUsable | kIOPMPreventIdleSleep), IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ kIOPMPowerStateVersion1, (IOPMDeviceUsable | kIOPMPreventIdleSleep), IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ kIOPMPowerStateVersion1, (IOPMDeviceUsable | IOPMMaxPerformance | kIOPMPreventIdleSleep), IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 }
};


// Static prototypes
static IOReturn
IOSCSIReducedBlockCommandsDevicePowerDownHandler ( void * 			target,
												   void * 			refCon,
												   UInt32 			messageType,
												   IOService *		provider,
												   void * 			messageArgument,
												   vm_size_t 		argSize );


//---------------------------------------------------------------------------
// ¥ IOSCSIReducedBlockCommandsDevicePowerDownHandler -
// C->C++ Glue code for Power Down Notifications.
//---------------------------------------------------------------------------


static IOReturn
IOSCSIReducedBlockCommandsDevicePowerDownHandler ( void * 			target,
												   void * 			refCon,
												   UInt32 			messageType,
												   IOService *		provider,
												   void * 			messageArgument,
												   vm_size_t 		argSize )
{
	
	return ( ( IOSCSIReducedBlockCommandsDevice * ) target )->PowerDownHandler (
														refCon,
														messageType,
														provider,
														messageArgument,
														argSize );
	
}


//---------------------------------------------------------------------------
// ¥ PowerDownHandler - Method called at sleep/restart/shutdown time.
//---------------------------------------------------------------------------

IOReturn
IOSCSIReducedBlockCommandsDevice::PowerDownHandler ( void * 		refCon,
													 UInt32 		messageType,
													IOService * 	provider,
													void * 			messageArgument,
													vm_size_t 		argSize )
{
	
	SCSIServiceResponse		serviceResponse;
	IOReturn				status 		= kIOReturnUnsupported;
	SCSITaskIdentifier		request		= NULL;
	
	switch ( messageType )
	{
		
		case kIOMessageSystemWillPowerOff:
			if ( fMediaPresent == true )
			{
				
				// Media is present (but may be spinning). Make sure the drive is spun down.
				if ( fCurrentPowerState > kRBCPowerStateSleep )
				{
					
					request = GetSCSITask ( );
					
					// Make sure the drive is spun down
					if ( START_STOP_UNIT ( request, 0, 0, 0, 0 ) == true )
					{
						
						serviceResponse = SendCommand ( request, 0 );
						
					}
					
					ReleaseSCSITask ( request );
					
				}
				
			}
			break;
			
		case kIOMessageSystemWillSleep:
		case kIOMessageSystemWillRestart:
		default:
			// We don't do anything at sleep or restart time. That is handled via
			// standard power management functions. See HandlePowerChange() for more
			// information.
			break;
		
	}
	
	return status;
	
}


//---------------------------------------------------------------------------
// ¥ GetInitialPowerState - Asks the driver which power state the device is
//							in at startup time. This function is only called
//							once, right after InitializePowerManagement().
//---------------------------------------------------------------------------

UInt32
IOSCSIReducedBlockCommandsDevice::GetInitialPowerState ( void )
{
	
	return kRBCPowerStateActive;
	
}


//---------------------------------------------------------------------------
// ¥ GetNumberOfPowerStateTransitions - Asks the driver for the number of state
//					transitions between sleep state and
//					the highest power state.
//---------------------------------------------------------------------------

UInt32
IOSCSIReducedBlockCommandsDevice::GetNumberOfPowerStateTransitions ( void )
{
    return ( kRBCPowerStateActive - kRBCPowerStateSleep );
}


//---------------------------------------------------------------------------
// ¥ InitializePowerManagement - 	Register the driver with our policy-maker
//									(also in the same class).
//---------------------------------------------------------------------------

void
IOSCSIReducedBlockCommandsDevice::InitializePowerManagement ( IOService * provider )
{
	
	STATUS_LOG ( ( "IOSCSIReducedBlockCommandsDevice::initForPM called\n" ) );	
	
	fCurrentPowerState = kRBCPowerStateActive;

	// Call our super to get us into the power management tree
	super::InitializePowerManagement ( provider );
	
	// Register ourselves as a "policy maker" for this device. We use
	// the number of default power states defined by RBC.
	registerPowerDriver ( this, sPowerStates, kRBCNumPowerStates );
	
	// Install handler for shutdown notifications
	fPowerDownNotifier = registerPrioritySleepWakeInterest (
					( IOServiceInterestHandler ) IOSCSIReducedBlockCommandsDevicePowerDownHandler,
					this );
	
	// Make sure we clamp the lowest power setting that we voluntarily go
	// into is state kRBCPowerStateSleep. We only enter kRBCPowerStateSystemSleep
	// if told by the power manager during a system sleep.
    changePowerStateTo ( kRBCPowerStateSleep );

}


//---------------------------------------------------------------------------
// ¥ HandleCheckPowerState - 
//---------------------------------------------------------------------------

void
IOSCSIReducedBlockCommandsDevice::HandleCheckPowerState ( void )
{

	if ( IsDeviceAccessEnabled ( ) )
	{
		
		super::HandleCheckPowerState ( kRBCPowerStateActive );
		
	}
	
}


//---------------------------------------------------------------------------
// ¥ TicklePowerManager - 	Calls activityTickle to tell the power manager we
//							need to be in a certain state to fulfill I/O
//---------------------------------------------------------------------------

void
IOSCSIReducedBlockCommandsDevice::TicklePowerManager ( void )
{
	
	// Tell the power manager we must be in active state to handle requests
	// "active" state means the minimal possible state in which the driver can
	// handle I/O. This may be set to standby, but there is no gain to setting
	// the drive to standby and then issuing an I/O, it just requires more time.
	// Also, if the drive was asleep, it might need a reset which could put it
	// in standby mode anyway, so we usually request the max state from the power
	// manager 
	( void ) super::TicklePowerManager ( kRBCPowerStateActive );
	
}


//---------------------------------------------------------------------------
// ¥ HandlePowerChange - Handles the state machine for power management
//---------------------------------------------------------------------------

void
IOSCSIReducedBlockCommandsDevice::HandlePowerChange ( void )
{
	
	SCSIServiceResponse		serviceResponse;
	SCSITaskIdentifier		request = NULL;
	
	STATUS_LOG ( ( "IOSCSIReducedBlockCommandsDevice::HandlePowerChange called\n" ) );
	
	request = GetSCSITask ( );
	
	while ( ( fProposedPowerState != fCurrentPowerState ) && ( isInactive ( ) == false ) )
	{
		
		STATUS_LOG ( ( "fProposedPowerState = %ld, fCurrentPowerState = %ld\n",
						fProposedPowerState, fCurrentPowerState ) );
		
		if ( ( fCurrentPowerState <= kRBCPowerStateSleep ) && ( fProposedPowerState > kRBCPowerStateSleep ) )
		{
			
			STATUS_LOG ( ( "We think we're in sleep\n" ) );
			
			// First, if we were in sleep mode ( fCurrentPowerState <= kRBCPowerStateSleep )
			// then we need to do some stuff to recover the device. If power was pulled, we
			// need to clear the power-on reset sense data.
			STATUS_LOG ( ( "calling ClearPowerOnReset\n" ) );
			
			// /!\ WARNING! we may bail out of ClearPowerOnReset because
			// the device was hot unplugged while we were sleeping - do sanity check here
			// and break out of loop if we have been terminated.
			if ( ClearPowerOnReset ( ) == false )
				break;
			
			STATUS_LOG ( ( "calling ClearNotReadyStatus\n" ) );
			
			// /!\ WARNING! we may bail out of ClearNotReadyStatus because
			// the device was hot unplugged while we were sleeping - do sanity check here
			// and break out of loop if we have been terminated.
			if ( ClearNotReadyStatus ( ) == false )
				break;
            
			if ( fMediaIsRemovable == true )
			{
				
				if ( fMediaPresent == true )
				{
					
					bool mediaPresent = CheckMediaPresence ( );
					
					if ( mediaPresent != fMediaPresent )
					{
						PANIC_NOW ( ( "Unexpected Media Removal when waking up from sleep\n" ) );
					}
					
					else
					{
						
						// Media is present, so lock it down
						if ( PREVENT_ALLOW_MEDIUM_REMOVAL ( request, 1 ) == true )
					    {
					    	// The command was successfully built, now send it
					    	serviceResponse = SendCommand ( request, 0 );
						
						}
						else
						{
							PANIC_NOW ( ( "IOSCSIBlockCommandsDevice::PollForMedia malformed command" ) );
						}
					
					}
					
				}
				
				else
				{
					EnablePolling ( );
				}
			
			}
		
		}
		
		switch ( fProposedPowerState )
		{
			
			case kRBCPowerStateSystemSleep:
			{
				
				UInt32 previousPowerState;
				
				STATUS_LOG ( ( "case kRBCPowerStateSystemSleep\n" ) );
				
				if ( fMediaPresent == false )
				{
					STATUS_LOG ( ( "Disabling polling\n" ) );
					DisablePolling ( );
				}
		
				// let outstanding commands complete
				previousPowerState = fCurrentPowerState;
				fCurrentPowerState = fProposedPowerState;

				while ( fNumCommandsOutstanding > 1 )
				{
					
					// Sleep this thread for 1ms until all outstanding commands
					// have completed. This prevents a sleep command from entering
					// the queue before an I/O and causing a problem on wakeup.
					IOSleep ( 1 );
					
				}
				
				// If the device supports the power conditions mode page, and we haven't already
				// put it to sleep using the START_STOP_UNIT command, issue one to the drive.
				if ( previousPowerState != kRBCPowerStateSleep )
				{
					
					if ( fDeviceSupportsPowerConditions )
					{
						
						STATUS_LOG ( ( "Sending START_STOP_UNIT to drive to turn it off\n" ) );
						
						if ( START_STOP_UNIT ( request, 1, 0x05, 0, 0 ) == true )
						{
							
							( void ) SendCommand ( request, 0 );
							
						}
						
					}
					
					else
					{
						
						STATUS_LOG ( ( "Power conditions not supported, make sure drive is spun down\n" ) );
						
						// At a minimum, make sure the drive is spun down
						if ( START_STOP_UNIT ( request, 1, 0, 0, 0 ) == true )
						{
							
							serviceResponse = SendCommand ( request, 0 );
							
						}
						
					}
					
				}
				
				fCurrentPowerState = kRBCPowerStateSystemSleep;
				
			}
			break;
			
			case kRBCPowerStateSleep:
			{
				
				STATUS_LOG ( ( "case kRBCPowerStateSleep\n" ) );
				
				if ( fCurrentPowerState > kRBCPowerStateSleep )
				{

					STATUS_LOG ( ( "At minimum, make sure drive is spun down.\n" ) );
					
					// At a minimum, make sure the drive is spun down
					if ( START_STOP_UNIT ( request, 1, 0, 0, 0 ) == true )
					{
						
						serviceResponse = SendCommand ( request, 0 );
						
					}
					
				}
				
				fCurrentPowerState = kRBCPowerStateSleep;
				
			}
			break;
			
			case kRBCPowerStateStandby:
			{
				
				STATUS_LOG ( ( "case kRBCPowerStateStandby\n" ) );
				
				if ( fDeviceSupportsPowerConditions )
				{

					STATUS_LOG ( ( "Sending START_STOP_UNIT to put drive in standby mode\n" ) );
					
					if ( START_STOP_UNIT ( request, 1, 0x03, 0, 0 ) == true )
					{
						
						serviceResponse = SendCommand ( request, 0 );
												
					}
					
				}
				
				fCurrentPowerState = kRBCPowerStateStandby;
				
			}
			break;
			
			case kRBCPowerStateIdle:
			{
				
				STATUS_LOG ( ( "case kRBCPowerStateIdle\n" ) );
				
				if ( fDeviceSupportsPowerConditions )
				{
					
					if ( START_STOP_UNIT ( request, 1, 0x02, 0, 0 ) == true )
					{
						
						serviceResponse = SendCommand ( request, 0 );
						
					}
					
				}
				
				fCurrentPowerState = kRBCPowerStateIdle;
				
			}
			break;
			
			case kRBCPowerStateActive:
			{
				
				STATUS_LOG ( ( "case kRBCPowerStateActive\n" ) );
				
				if ( fDeviceSupportsPowerConditions )
				{
					
					STATUS_LOG ( ( "Sending START_STOP_UNIT to drive to put it in active mode\n" ) );
					
					if ( START_STOP_UNIT ( request, 1, 0x01, 0, 0 ) == true )
					{
						
						serviceResponse = SendCommand ( request, 0 );
						
					}
				
				}
				
				if ( fMediaPresent )
				{
					
					// Don't forget to start the media
					if ( START_STOP_UNIT ( request, 0x00, 0x00, 0x00, 0x01 ) == true )
					{
						
						STATUS_LOG ( ( "Sending START_STOP_UNIT.\n" ) );
						serviceResponse = SendCommand ( request, 0 );
						
					}
					
				}
				
				fCurrentPowerState = kRBCPowerStateActive;
				fCommandGate->commandWakeup ( &fCurrentPowerState, false );
				
			}
			break;
			
			default:
				PANIC_NOW ( ( "Undefined power state issued\n" ) );
				break;
			
		}
		
	}

	ReleaseSCSITask ( request );
	
}


//---------------------------------------------------------------------------
// ¥ CheckMediaPresence - 	Checks if media is present. If so, it locks it down
//							again, else it requests a dialog to be displayed
//							and tears the stack down
//---------------------------------------------------------------------------

bool
IOSCSIReducedBlockCommandsDevice::CheckMediaPresence ( void )
{
	
	SCSI_Sense_Data				senseBuffer;
	IOMemoryDescriptor *		bufferDesc;
	SCSITaskIdentifier			request;
	bool						mediaPresent 	= false;
	bool						driveReady 		= false;
	SCSIServiceResponse 		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;

	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) &senseBuffer,
													kSenseDefaultSize,
													kIODirectionIn );
	
	request = GetSCSITask ( );
	
	do
	{
				
		if ( TEST_UNIT_READY ( request ) == true )
	    {
			
			STATUS_LOG ( ( "sending TUR.\n" ) );
	    	// The command was successfully built, now send it
	    	serviceResponse = SendCommand ( request, 0 );
	    	
		}
		
		else
		{
			PANIC_NOW ( ( "IOSCSIReducedBlockCommandsDevice::CheckMediaPresence malformed command" ) );
		}
		
		if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
		{
			
			STATUS_LOG ( ( "TUR completed.\n" ) );
			
			bool validSense = false;
			
			if ( GetTaskStatus ( request ) == kSCSITaskStatus_CHECK_CONDITION )
			{
				
				validSense = GetAutoSenseData ( request, &senseBuffer );
				if ( validSense == false )
				{
					
					if ( REQUEST_SENSE ( request, bufferDesc, kSenseDefaultSize ) == true )
				    {
				    	
				    	STATUS_LOG ( ( "sending REQ_SENSE.\n" ) );
				    	// The command was successfully built, now send it
				    	serviceResponse = SendCommand ( request, 0 );
				    	
					}
					
					else
					{
						PANIC_NOW ( ( "IOSCSIReducedBlockCommandsDevice::CheckMediaPresence malformed command" ) );
					}
					
					if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
					{
						
						STATUS_LOG ( ( "validSense data.\n" ) );
						validSense = true;
						
					}
						
				}
				
				if ( validSense == true )
				{
					
					STATUS_LOG ( ( "sense data: %01x, %02x, %02x\n",
								( senseBuffer.SENSE_KEY  & kSENSE_KEY_Mask ),
								  senseBuffer.ADDITIONAL_SENSE_CODE,
								  senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER ) );

					// Check the sense key to see if it is an error group we know how to handle
					if  ( ( ( senseBuffer.SENSE_KEY & kSENSE_KEY_Mask ) == kSENSE_KEY_NOT_READY ) || 
						  ( ( senseBuffer.SENSE_KEY & kSENSE_KEY_Mask ) == kSENSE_KEY_MEDIUM_ERROR ) )
					{
						
						// The SenseKey is an 02 ( Not Ready ) or 03 ( Medium Error ). Check to see
						// if we can do something about this
						
						if ( ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x04 ) && 
							 ( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x02 ) )
						{
							
							// Device requires a start command before we can tell if media is there
							if ( START_STOP_UNIT ( request, 0x00, 0x00, 0x00, 0x01 ) == true )
							{
								
								STATUS_LOG ( ( "Sending START_STOP_UNIT.\n" ) );
								serviceResponse = SendCommand ( request, 0 );
								
							}
							
							else
							{
								PANIC_NOW ( ( "IOSCSIReducedBlockCommandsDevice::CheckMediaPresence malformed command" ) );
							}
							
							STATUS_LOG ( ( "%s::drive NOT READY\n", getName ( ) ) );
							
							IOSleep ( 200 );
							continue;
							
						}
						
						else if ( ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x3A ) && 
							 	  ( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x00 ) )
						{
							
							STATUS_LOG ( ( "No Media.\n" ) );
							// No media is present, return false
							driveReady = true;
							mediaPresent = false;
							
						}
						
						else
						{
							
							STATUS_LOG ( ( "%s::drive NOT READY\n", getName ( ) ) );
							IOSleep ( 200 );
							continue;
							
						}
										
					}
										
					else
					{
						
						STATUS_LOG ( ( "%s::drive READY, media present\n", getName ( ) ) );
						// Media is present, return true
						driveReady = true;
						mediaPresent = true;
						
					}
					
				}
				
			}
			
			else
			{
				
				STATUS_LOG ( ( "%s::drive READY, media present\n", getName ( ) ) );
				// Media is present, return true
				driveReady = true;
				mediaPresent = true;
				
			}
			
		}
        
		else
		{
			
			// the command failed - perhaps the device was hot unplugged
			// give other threads some time to run.
			IOSleep ( 200 );
			
		}
		
	// check isInactive in case device was hot unplugged during sleep
	// and we are in an infinite loop here
	} while ( ( driveReady == false ) && ( isInactive ( ) == false ) );
	
	bufferDesc->release ( );
	ReleaseSCSITask ( request );
	
	return mediaPresent;
	
}
