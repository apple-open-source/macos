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
#include "IOSCSIBlockCommandsDevice.h"

#define SCSI_SBC_DEVICE_DEBUGGING_LEVEL 0

#if ( SCSI_SBC_DEVICE_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_SBC_DEVICE_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_SBC_DEVICE_DEBUGGING_LEVEL >= 3 )
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

static IOPMPowerState sPowerStates[kSBCNumPowerStates] =
{
	{ kIOPMPowerStateVersion1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },			/* never entered voluntarily */
	{ kIOPMPowerStateVersion1, 0, IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ kIOPMPowerStateVersion1, (IOPMDeviceUsable | kIOPMPreventIdleSleep), IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ kIOPMPowerStateVersion1, (IOPMDeviceUsable | kIOPMPreventIdleSleep), IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ kIOPMPowerStateVersion1, (IOPMDeviceUsable | IOPMMaxPerformance | kIOPMPreventIdleSleep), IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 }
};


//---------------------------------------------------------------------------
// ¥ GetInitialPowerState - Asks the driver which power state the device is
//							in at startup time. This function is only called
//							once, right after InitializePowerManagement().
//---------------------------------------------------------------------------

UInt32
IOSCSIBlockCommandsDevice::GetInitialPowerState ( void )
{

	STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::%s called%s\n", "\033[33m",
						__FUNCTION__, "\033[0m" ) );
	
	return kSBCPowerStateActive;
	
}


//---------------------------------------------------------------------------
// ¥ GetNumberOfPowerStateTransitions - Asks the driver for the number of state
//					transitions between sleep state and
//					the highest power state.
//---------------------------------------------------------------------------

UInt32
IOSCSIBlockCommandsDevice::GetNumberOfPowerStateTransitions ( void )
{
    return ( kSBCPowerStateActive - kSBCPowerStateSleep );
}


//---------------------------------------------------------------------------
// ¥ InitializePowerManagement - 	Register the driver with our policy-maker
//									(also in the same class).
//---------------------------------------------------------------------------

void
IOSCSIBlockCommandsDevice::InitializePowerManagement ( IOService * provider )
{
	
	STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::InitializePowerManagement called\n" ) );	
	
	fCurrentPowerState = kSBCPowerStateActive;
	
	// Call our super to get us into the power management tree
	super::InitializePowerManagement ( provider );
	
	// Register ourselves as a "policy maker" for this device. We use
	// the number of default power states defined by SBC-2.
	registerPowerDriver ( this, sPowerStates, kSBCNumPowerStates );
	
	// Make sure we clamp the lowest power setting that we voluntarily go
	// into is state kSBCPowerStateSleep. We only enter kSBCPowerStateSystemSleep
	// if told by the power manager during a system sleep.
	changePowerStateTo ( kSBCPowerStateSleep );
	
}


//---------------------------------------------------------------------------
// ¥ HandleCheckPowerState - 
//---------------------------------------------------------------------------

void
IOSCSIBlockCommandsDevice::HandleCheckPowerState ( void )
{

	if ( IsDeviceAccessEnabled ( ) )
	{
	
		super::HandleCheckPowerState ( kSBCPowerStateActive );
	
	}
	
}


//---------------------------------------------------------------------------
// ¥ TicklePowerManager - 	Calls activityTickle to tell the power manager we
//							need to be in a certain state to fulfill I/O
//---------------------------------------------------------------------------

void
IOSCSIBlockCommandsDevice::TicklePowerManager ( void )
{
	
	// Tell the power manager we must be in active state to handle requests
	// "active" state means the minimal possible state in which the driver can
	// handle I/O. This may be set to standby, but there is no gain to setting
	// the drive to standby and then issuing an I/O, it just requires more time.
	// Also, if the drive was asleep, it might need a reset which could put it
	// in standby mode anyway, so we usually request the max state from the power
	// manager 
	( void ) super::TicklePowerManager ( kSBCPowerStateActive );
	
}


//---------------------------------------------------------------------------
// ¥ HandlePowerChange - Handles the state machine for power management
//---------------------------------------------------------------------------

void
IOSCSIBlockCommandsDevice::HandlePowerChange ( void )
{
	
	SCSIServiceResponse		serviceResponse;
	SCSITaskIdentifier		request = NULL;
	
	STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::HandlePowerChange called\n" ) );
	
	request = GetSCSITask ( );
	
	while ( ( fProposedPowerState != fCurrentPowerState ) && ( isInactive ( ) == false ) )
	{
		
		STATUS_LOG ( ( "fProposedPowerState = %ld, fCurrentPowerState = %ld\n",
						fProposedPowerState, fCurrentPowerState ) );

		if ( ( fCurrentPowerState <= kSBCPowerStateSleep ) && ( fProposedPowerState > kSBCPowerStateSleep ) )
		{
			
			STATUS_LOG ( ( "We think we're in sleep\n" ) );
			
			if ( START_STOP_UNIT ( request, 0x00, 0x00, 0x00, 0x01, 0x00 ) == true )
			{
				
				serviceResponse = SendCommand ( request, 0 );
				
			}
			
			// First, if we were in sleep mode ( fCurrentPowerState <= kSBCPowerStateSleep )
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
			
			// Now, test if media is present, if so we lock it back in so the user
			// can't eject it, and if s/he removed the medium we notify the layer
			// above that the medium is gone and tear down the stack.
			if ( fMediaIsRemovable == true )
			{
				
				if ( fMediumPresent == true )
				{
					
					bool mediaPresent = VerifyMediumPresence ( );
					
					if ( mediaPresent != fMediumPresent )
					{
						PANIC_NOW ( ( "Unexpected Media Removal when waking up from sleep\n" ) );
					}
					
					else
					{
						
						// Media is present, so lock it down
						if ( PREVENT_ALLOW_MEDIUM_REMOVAL ( request, 1, 0 ) == true )
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
 					fPollingMode = kPollingMode_NewMedia;
					EnablePolling ( );
				}
				
			}
			
		}
		
		switch ( fProposedPowerState )
		{
			
			case kSBCPowerStateSystemSleep:
			{
				UInt32 previousPowerState;
				STATUS_LOG ( ( "case kSBCPowerStateSystemSleep\n" ) );
				
				// Immediately disable polling if media is not present
				if ( fMediumPresent == false )
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
				if ( previousPowerState != kSBCPowerStateSleep )
				{
					
					if ( fDeviceSupportsPowerConditions )
					{
					
						STATUS_LOG ( ( "Sending START_STOP_UNIT to drive to turn it off\n" ) );
						
						if ( START_STOP_UNIT ( request, 0, 0x05, 0, 0, 0 ) == true )
						{
							
							serviceResponse = SendCommand ( request, 0 );
							
						}
						
					}
									
					else
					{
	
						STATUS_LOG ( ( "Power conditions not supported, make sure drive is spun down\n" ) );
					
						// At a minimum, make sure the drive is spun down
						if ( START_STOP_UNIT ( request, 0, 0, 0, 0, 0 ) == true )
						{
							
							serviceResponse = SendCommand ( request, 0 );
							
						}
						
					}
					
				}
				
				fCurrentPowerState = kSBCPowerStateSystemSleep;
				
			}	
			break;
			
			case kSBCPowerStateSleep:
			{
				
				STATUS_LOG ( ( "case kSBCPowerStateSleep\n" ) );
				
				// are we waking up from system sleep?
				if ( fCurrentPowerState < kSBCPowerStateSleep )
				{
					
					// If we have woken up from system sleep, the drive
					// should get a power on reset or a reset which will cause it
					// to truly be in active mode. We can simply tickle the power
					// manager here to bring us into active state and just pretend
					// we went back to sleep here.
					STATUS_LOG ( ( "TicklePowerManager.\n" ) );
					TicklePowerManager ( );
					STATUS_LOG ( ( "Wakeup path completed.\n" ) );

					fCurrentPowerState = fProposedPowerState;
					break;
					
				}
				
				if ( fCurrentPowerState > kSBCPowerStateSleep )
				{
					
					STATUS_LOG ( ( "At minimum, make sure drive is spun down.\n" ) );
					
					// At a minimum, make sure the drive is spun down
					if ( START_STOP_UNIT ( request, 0, 0, 0, 0, 0 ) == true )
					{
						
						serviceResponse = SendCommand ( request, 0 );
						
					}
					
				}
				
				fCurrentPowerState = kSBCPowerStateSleep;
				
			}
			break;
			
			case kSBCPowerStateStandby:
			{
				
				STATUS_LOG ( ( "case kSBCPowerStateStandby\n" ) );

				STATUS_LOG ( ( "At minimum, make sure drive is spun down.\n" ) );
				if ( fCurrentPowerState > kSBCPowerStateStandby )
				{

					// At a minimum, make sure the drive is spun down
					if ( START_STOP_UNIT ( request, 0, 0, 0, 0, 0 ) == true )
					{
						
						serviceResponse = SendCommand ( request, 0 );
						
					}
				
				}
				
				fCurrentPowerState = kSBCPowerStateStandby;
				
			}
			break;
			
			case kSBCPowerStateIdle:
			{
				
				STATUS_LOG ( ( "case kSBCPowerStateIdle\n" ) );				
				fCurrentPowerState = kSBCPowerStateIdle;
				
			}
			break;
			
			case kSBCPowerStateActive:
			{
				
				STATUS_LOG ( ( "case kSBCPowerStateActive\n" ) );

				if ( START_STOP_UNIT ( request, 0, 0, 0, 1, 0 ) == true )
				{
							
					serviceResponse = SendCommand ( request, 0 );
							
				}
				
				fCurrentPowerState = kSBCPowerStateActive;
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
// ¥ VerifyMediumPresence - Checks if media is present. If so, it locks it down
//							again, else it requests a dialog to be displayed
//							and tears the stack down
//---------------------------------------------------------------------------

bool
IOSCSIBlockCommandsDevice::VerifyMediumPresence ( void )
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
		
		if ( TEST_UNIT_READY ( request, 0 ) == true )
	    {
			
			STATUS_LOG ( ( "sending TUR.\n" ) );
	    	// The command was successfully built, now send it
	    	serviceResponse = SendCommand ( request, 0 );
	    	
		}
		
		else
		{
			PANIC_NOW ( ( "IOSCSIBlockCommandsDevice::VerifyMediumPresence malformed command" ) );
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
					
					if ( REQUEST_SENSE ( request, bufferDesc, kSenseDefaultSize, 0  ) == true )
				    {
				    	
				    	STATUS_LOG ( ( "sending REQ_SENSE.\n" ) );
				    	// The command was successfully built, now send it
				    	serviceResponse = SendCommand ( request, 0 );
				    	
					}
					
					else
					{
						PANIC_NOW ( ( "IOSCSIBlockCommandsDevice::VerifyMediumPresence malformed command" ) );
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
							if ( START_STOP_UNIT ( request, 0x00, 0x00, 0x00, 0x01, 0x00 ) == true )
							{
								
								STATUS_LOG ( ( "Sending START_STOP_UNIT.\n" ) );
								serviceResponse = SendCommand ( request, 0 );
								
							}
							
							else
							{
								PANIC_NOW ( ( "IOSCSIBlockCommandsDevice::VerifyMediumPresence malformed command" ) );
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