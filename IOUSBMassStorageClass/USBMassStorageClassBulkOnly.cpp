/*
 * Copyright (c) 1998-2009 Apple Inc. All rights reserved.
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


//-----------------------------------------------------------------------------
//	Includes
//-----------------------------------------------------------------------------

// This class' header file
#include "IOUSBMassStorageClass.h"
#include "Debugging.h"

//-----------------------------------------------------------------------------
//	Macros
//-----------------------------------------------------------------------------

// Macros for printing debugging information
#if (USB_MASS_STORAGE_DEBUG == 1)
// Override the debug level for USBLog to make sure our logs make it out and then import
// the logging header.
#define DEBUG_LEVEL		1
#include <IOKit/usb/IOUSBLog.h>
#define STATUS_LOG(x)	USBLog x
#else
#define STATUS_LOG(x)
#endif

// Bulk Only State Machine States
enum
{
	kBulkOnlyCommandSent = 1,
	kBulkOnlyCheckCBWBulkStall,
	kBulkOnlyClearCBWBulkStall,
	kBulkOnlyBulkIOComplete,
	kBulkOnlyCheckBulkStall,
	kBulkOnlyClearBulkStall,
	kBulkOnlyCheckBulkStallPostCSW,
	kBulkOnlyClearBulkStallPostCSW,
	kBulkOnlyStatusReceived,
	kBulkOnlyStatusReceived2ndTime,
	kBulkOnlyResetCompleted,
	kBulkOnlyClearBulkInCompleted,
	kBulkOnlyClearBulkOutCompleted
};


#pragma mark -
#pragma mark Protocol Services Methods
#pragma mark -

//-----------------------------------------------------------------------------
//	- AbortSCSICommandForBulkOnlyProtocol - The AbortSCSICommand helper method
//											for Bulk Only protocol devices.
//																	[PROTECTED]
//-----------------------------------------------------------------------------

IOReturn IOUSBMassStorageClass::AbortSCSICommandForBulkOnlyProtocol(
                                        SCSITaskIdentifier request )
{
	UNUSED( request );
	
	return kIOReturnError;
}


//-----------------------------------------------------------------------------
//	- SendSCSICommandForBulkOnlyProtocol - 	The SendSCSICommand helper method
//											for Bulk Only protocol devices.
//																	[PROTECTED]
//-----------------------------------------------------------------------------

IOReturn IOUSBMassStorageClass::SendSCSICommandForBulkOnlyProtocol(
                                         SCSITaskIdentifier request )
{
	IOReturn					status;
	BulkOnlyRequestBlock *		theBulkOnlyRB;

	theBulkOnlyRB = GetBulkOnlyRequestBlock();
	
	// Clear out the CBW	
	bzero( theBulkOnlyRB, sizeof( BulkOnlyRequestBlock ) );

	// Save the SCSI Task
	theBulkOnlyRB->request = request; 	
	
	// Set up the IOUSBCompletion structure
	theBulkOnlyRB->boCompletion.target 		= this;
	theBulkOnlyRB->boCompletion.action 		= &this->BulkOnlyUSBCompletionAction;
	theBulkOnlyRB->boCompletion.parameter 	= theBulkOnlyRB;
	
   	STATUS_LOG(( 6, "%s[%p]: SendSCSICommandForBulkOnlyProtocol send CBW", getName(), this ));
	status = BulkOnlySendCBWPacket( theBulkOnlyRB, kBulkOnlyCommandSent );
   	STATUS_LOG(( 5, "%s[%p]: SendSCSICommandForBulkOnlyProtocol send CBW returned %x", getName(), this, status ));
    
   	if ( status != kIOReturnSuccess )
   	{
   		ReleaseBulkOnlyRequestBlock( theBulkOnlyRB );
   	}
   	
	return status;
}

#pragma mark -
#pragma mark Bulk Only Protocol Specific Commands


//-----------------------------------------------------------------------------
//	- BulkDeviceResetDevice											[PROTECTED]
//-----------------------------------------------------------------------------

IOReturn 
IOUSBMassStorageClass::BulkDeviceResetDevice(
						BulkOnlyRequestBlock *		boRequestBlock,
						UInt32						nextExecutionState )
{
	IOReturn			status;

	if ( fTerminating == true )
	{
 		// We have an invalid interface, the device has probably been removed.
 		// Nothing else to do except to report an error.
 		return kIOReturnDeviceError;
	}

	// Clear out the structure for the request
	bzero( &fUSBDeviceRequest, sizeof(IOUSBDevRequest));

	// Build the USB command	
    fUSBDeviceRequest.bmRequestType 	= USBmakebmRequestType(kUSBNone, kUSBClass, kUSBInterface);	
   	fUSBDeviceRequest.bRequest 			= 0xFF;
   	fUSBDeviceRequest.wValue			= 0;
	fUSBDeviceRequest.wIndex			= GetInterfaceReference()->GetInterfaceNumber();
	fUSBDeviceRequest.wLength			= 0;
   	fUSBDeviceRequest.pData				= NULL;

	// Set the next state to be executed
	boRequestBlock->currentState = nextExecutionState;

	// Send the command over the control endpoint
	status = GetInterfaceReference()->DeviceRequest( &fUSBDeviceRequest, &boRequestBlock->boCompletion );
   	STATUS_LOG(( 4, "%s[%p]: BulkDeviceResetDevice returned %x", getName(), this, status ));
    
	return status;
}


#pragma mark -
#pragma mark SendSCSICommand Helper methods


//-----------------------------------------------------------------------------
//	- BulkOnlyUSBCompletionAction									[PROTECTED]
//-----------------------------------------------------------------------------

void 
IOUSBMassStorageClass::BulkOnlyUSBCompletionAction(
					                void *			target,
					                void *			parameter,
					                IOReturn		status,
					                UInt32			bufferSizeRemaining)
{
	IOUSBMassStorageClass *		theMSC;
	BulkOnlyRequestBlock *		boRequestBlock;
	
	theMSC 			= (IOUSBMassStorageClass *) target;
	boRequestBlock 	= (BulkOnlyRequestBlock *) parameter;
	theMSC->BulkOnlyExecuteCommandCompletion( 	boRequestBlock, 
												status, 
												bufferSizeRemaining );
}


//-----------------------------------------------------------------------------
//	- BulkOnlySendCBWPacket -	Prepare the Command Block Wrapper packet for
//								Bulk Only Protocol
//																	[PROTECTED]
//-----------------------------------------------------------------------------

IOReturn 
IOUSBMassStorageClass::BulkOnlySendCBWPacket(
						BulkOnlyRequestBlock *		boRequestBlock,
						UInt32						nextExecutionState )
{
	IOReturn 			status = kIOReturnError;

    // Set our Bulk-Only phase descriptor.
	require ( ( fBulkOnlyCBWMemoryDescriptor != NULL ), Exit );
	boRequestBlock->boPhaseDesc = fBulkOnlyCBWMemoryDescriptor;

	boRequestBlock->boCBW.cbwSignature 			= kCommandBlockWrapperSignature;
	boRequestBlock->boCBW.cbwTag 				= GetNextBulkOnlyCommandTag();
	boRequestBlock->boCBW.cbwTransferLength 	= HostToUSBLong(
						GetRequestedDataTransferCount(boRequestBlock->request));
	if (GetDataTransferDirection(boRequestBlock->request) == 
							kSCSIDataTransfer_FromTargetToInitiator)
	{
		boRequestBlock->boCBW.cbwFlags 		= kCBWFlagsDataIn;
	}
	else if (GetDataTransferDirection(boRequestBlock->request) == 
							kSCSIDataTransfer_FromInitiatorToTarget)
	{
		boRequestBlock->boCBW.cbwFlags 		= kCBWFlagsDataOut;
	}
	else
	{
		boRequestBlock->boCBW.cbwFlags 		= 0;
	}

	// Set the LUN (not needed until LUN support is added).
	boRequestBlock->boCBW.cbwLUN 			= GetLogicalUnitNumber( boRequestBlock->request ) & kCBWLUNMask;				// Bits 0-3: LUN, 4-7: Reserved
	boRequestBlock->boCBW.cbwCDBLength 		= GetCommandDescriptorBlockSize( boRequestBlock->request );		// Bits 0-4: CDB Length, 5-7: Reserved
	GetCommandDescriptorBlock( boRequestBlock->request, &boRequestBlock->boCBW.cbwCDB );

	// Once timeouts are support, set the timeout value for the request 

	// Set the next state to be executed
	boRequestBlock->currentState = nextExecutionState;

	// Send the CBW to the device	
	if ( GetBulkOutPipe() == NULL )
	{
   		STATUS_LOG(( 4, "%s[%p]: BulkOnlySendCBWPacket Bulk Out is NULL", getName(), this ));
	}
	
   	STATUS_LOG(( 6, "%s[%p]: BulkOnlySendCBWPacket sent", getName(), this ));
	status = GetBulkOutPipe()->Write(	boRequestBlock->boPhaseDesc,
										GetTimeoutDuration( boRequestBlock->request ),  // Use the client's timeout for both
										GetTimeoutDuration( boRequestBlock->request ),
										&boRequestBlock->boCompletion );
   	STATUS_LOG(( 5, "%s[%p]: BulkOnlySendCBWPacket returned %x", getName(), this, status ));
	
	if ( status == kIOUSBPipeStalled )
    {
		STATUS_LOG(( 5, "%s[%p]: BulkOnlySendCBWPacket could not be queued, returned", getName(), this ));
		
		// The host is reporting a pipe stall. We'll need to address this if we ever wish to attempt a retry.
		// We're relying on higher elements of the storage stack to iniate the retry. 
		boRequestBlock->currentState = kBulkOnlyCheckCBWBulkStall;
		status = GetStatusEndpointStatus( GetBulkOutPipe(), &boRequestBlock->boGetStatusBuffer, &boRequestBlock->boCompletion );
				
	}
    
Exit:
    
	return status;
}


//-----------------------------------------------------------------------------
//	- BulkOnlyTransferData											[PROTECTED]
//-----------------------------------------------------------------------------

IOReturn 
IOUSBMassStorageClass::BulkOnlyTransferData( 
						BulkOnlyRequestBlock *		boRequestBlock,
						UInt32						nextExecutionState )
{
	IOReturn	status = kIOReturnError;
	
	// Set the next state to be executed
	boRequestBlock->currentState = nextExecutionState;

	// Start a bulk in or out transaction
	if (GetDataTransferDirection(boRequestBlock->request) == kSCSIDataTransfer_FromTargetToInitiator)
	{
		status = GetBulkInPipe()->Read(
					GetDataBuffer( boRequestBlock->request ), 
					GetTimeoutDuration( boRequestBlock->request ),  // Use the client's timeout for both
					GetTimeoutDuration( boRequestBlock->request ),
					GetRequestedDataTransferCount( boRequestBlock->request ),
					&boRequestBlock->boCompletion );
	}
	else if (GetDataTransferDirection(boRequestBlock->request) == kSCSIDataTransfer_FromInitiatorToTarget)
	{
		status = GetBulkOutPipe()->Write(
					GetDataBuffer( boRequestBlock->request ), 
					GetTimeoutDuration( boRequestBlock->request ),  // Use the client's timeout for both
					GetTimeoutDuration( boRequestBlock->request ),
					GetRequestedDataTransferCount( boRequestBlock->request ),
					&boRequestBlock->boCompletion );
	}

   	STATUS_LOG(( 5, "%s[%p]: BulkOnlyTransferData returned %x", getName(), this, status ));
    
	return status;
}


//-----------------------------------------------------------------------------
//	- BulkOnlyReceiveCSWPacket -	Prepare the Command Status Wrapper packet
//									for Bulk Only Protocol
//																	[PROTECTED]
//-----------------------------------------------------------------------------

// 
IOReturn 
IOUSBMassStorageClass::BulkOnlyReceiveCSWPacket(
						BulkOnlyRequestBlock *		boRequestBlock,
						UInt32						nextExecutionState )
{
	IOReturn 			status = kIOReturnInternalError;

	// Set our Bulk-Only phase descriptor.
	require ( ( fBulkOnlyCSWMemoryDescriptor != NULL ), Exit );
	boRequestBlock->boPhaseDesc = fBulkOnlyCSWMemoryDescriptor;
	
	// Set the next state to be executed
	boRequestBlock->currentState = nextExecutionState;

    // Retrieve the CSW from the device	
    status = GetBulkInPipe()->Read( boRequestBlock->boPhaseDesc,
                                    GetTimeoutDuration( boRequestBlock->request ), // Use the client's timeout for both
                                    GetTimeoutDuration( boRequestBlock->request ), 
                                    &boRequestBlock->boCompletion );		

   	STATUS_LOG(( 5, "%s[%p]: BulkOnlyReceiveCSWPacket returned %x", getName(), this, status ));

    
Exit:

	return status;
}


//-----------------------------------------------------------------------------
//	- BulkOnlyExecuteCommandCompletion								[PROTECTED]
//-----------------------------------------------------------------------------

void 
IOUSBMassStorageClass::BulkOnlyExecuteCommandCompletion(
						BulkOnlyRequestBlock *	boRequestBlock,
		                IOReturn				resultingStatus,
		                UInt32					bufferSizeRemaining)
{
	UNUSED( bufferSizeRemaining );
	
	IOReturn 		status = kIOReturnError;
	bool			commandInProgress = false;


	STATUS_LOG(( 4, "%s[%p]: BulkOnlyExecuteCommandCompletion Entered with boRequestBlock=%p resultingStatus=0x%x", getName(), this, boRequestBlock, resultingStatus ));

	if ( ( boRequestBlock->request == NULL ) || ( fBulkOnlyCommandStructInUse == false ) )
	{ 
		// The request field is NULL, this appears to  be a double callback, do nothing.
        // OR the command was aborted earlier, do nothing.
		STATUS_LOG(( 4, "%s[%p]: boRequestBlock->request is NULL, returned %x", getName(), this, resultingStatus ));
		return;
	}
	
	if ( (  GetInterfaceReference() == NULL ) || ( fTerminating == true ) )
	{
		// Our interface has been closed, probably because of an
		// unplug, return an error for the command since it can no 
		// longer be executed.
		SCSITaskIdentifier	request = boRequestBlock->request;
		
		STATUS_LOG(( 4, "%s[%p]: Interface object is NULL, returned %x", getName(), this, resultingStatus ));

		ReleaseBulkOnlyRequestBlock( boRequestBlock );
		CompleteSCSICommand( request, status );
		return;
	}		

	
	if ( ( resultingStatus == kIOReturnNotResponding ) || ( resultingStatus == kIOReturnAborted ) )
	{
	
		STATUS_LOG(( 5, "%s[%p]: BulkOnlyExecuteCommandCompletion previous command returned %x", getName(), this, resultingStatus ));
		
		// The transfer failed mid-transfer or was aborted by the USB layer. Either way the device will
        // be non-responsive until we reset it, or we discover it has been disconnected.
		ResetDeviceNow ( false );
		commandInProgress = true; 
		goto Exit;
		
	}
	
	switch( boRequestBlock->currentState )
	{
	
		case kBulkOnlyCommandSent:
		{
		
   			STATUS_LOG(( 5, "%s[%p]: kBulkOnlyCommandSent returned %x", getName(), this, resultingStatus ));
			
			if ( resultingStatus == kIOUSBPipeStalled )
			{
			
				// The device is so far gone that we couldn't even retry the CSW. Reset time.
				if ( fUseUSBResetNotBOReset )
				{
				
					// By passing this to Finish device recovery we ensure that the driver is still active,
					// and that the device is still connected to the Mac.
					ResetDeviceNow ( false );
					status = kIOReturnSuccess;
					
				}
				else
				{
				
					// The host is reporting a pipe stall. We'll need to address this if we ever wish to attempt a retry.
					// We're relying on higher elements of the storage stack to iniate the retry. 
					boRequestBlock->currentState = kBulkOnlyCheckCBWBulkStall;
					status = GetStatusEndpointStatus( GetBulkOutPipe(), &boRequestBlock->boGetStatusBuffer, &boRequestBlock->boCompletion );

				}
			
				if ( status == kIOReturnSuccess )
				{
					commandInProgress = true;
				}
				break;
				
			}
			
			if ( resultingStatus != kIOReturnSuccess )
			{
                
				// An error occurred, probably a timeout error,
				// and the command was not successfully sent to the device.
				ResetDeviceNow ( false );
				status = kIOReturnSuccess;
                
				if( status == kIOReturnSuccess )
				{
					commandInProgress = true;
				}
				break;
                
			}
			
			// If there is to be no data transfer then we are done and can return to the caller.
			if ( ( GetDataTransferDirection( boRequestBlock->request ) == kSCSIDataTransfer_NoDataTransfer ) || 
                 ( GetRequestedDataTransferCount( boRequestBlock->request ) == 0 ) )
			{
				
				// No data to transfer, get the Command Status Wrapper from the device.
				status = BulkOnlyReceiveCSWPacket( boRequestBlock, kBulkOnlyStatusReceived );
				if ( status == kIOReturnSuccess )
				{
					commandInProgress = true;
				}
				
			}
			else
			{
			
				// Start a bulk in or out transaction.
				status = BulkOnlyTransferData( boRequestBlock, kBulkOnlyBulkIOComplete ); 
				if ( status == kIOReturnSuccess )
				{
					commandInProgress = true;
				}
				
			}

		}
		break;
		
		
		case kBulkOnlyCheckCBWBulkStall:
		{
   			
			STATUS_LOG(( 5, "%s[%p]: IOUSBMassStorageClass::BulkOnlyExecuteCommandCompletion - kBulkOnlyCheckCBWBulkStall returned %x", getName(), this, resultingStatus ));

			// Check to see if the endpoint was stalled
			if ( (boRequestBlock->boGetStatusBuffer[0] & 1) == 1 )
			{
				STATUS_LOG(( 5, "%s[%p]: IOUSBMassStorageClass::BulkOnlyExecuteCommandCompletion - will try to clear endpoint", getName(), this ));
				// The endpoint was stalled. Clear the stall so we'll be able to retry sending the CBW.
				boRequestBlock->currentState = kBulkOnlyClearCBWBulkStall;
				status = ClearFeatureEndpointStall( GetBulkOutPipe(), &boRequestBlock->boCompletion );
				STATUS_LOG(( 5, "%s[%p]: IOUSBMassStorageClass::BulkOnlyExecuteCommandCompletion - ClearFeatureEndpointStall returned status = %x", getName(), this, status ));
				if ( status == kIOReturnSuccess )
				{	
					commandInProgress = true;
				}
				
			}
			else
			{
				STATUS_LOG(( 5, "%s[%p]: IOUSBMassStorageClass::BulkOnlyExecuteCommandCompletion - will abort bulk out pipe", getName(), this ));
				// Since the pipe was not stalled, but the host thought it was we should clear the host side of the pipe.
				GetBulkOutPipe()->Abort();
				
				// As we failed to successfully transmit the BO CBW we return an error up the stack so the command will be retried.
				SetRealizedDataTransferCount( boRequestBlock->request, 0 );
				status = kIOReturnError;
				
			}
			
		}
		break;
		

		case kBulkOnlyClearCBWBulkStall:
		{
   			STATUS_LOG(( 5, "%s[%p]: kBulkOnlyClearCBWBulkStall returned %x", getName(), this, resultingStatus ));

			// As we failed to successfully transmit the BO CBW we return an error up the stack so the command will be retried.
			SetRealizedDataTransferCount( boRequestBlock->request, 0 );
			status = kIOReturnError;
			
		}
		break;
		
		
		case kBulkOnlyBulkIOComplete:
		{
			status 		=	resultingStatus;			// and status

   			STATUS_LOG(( 5, "%s[%p]: kBulkOnlyBulkIOComplete returned %x", getName(), this, resultingStatus ));
			
			if ( ( resultingStatus == kIOUSBPipeStalled ) || ( resultingStatus == kIOReturnSuccess ) )
			{
				// Save the number of bytes tranferred in the request
				// Use the amount returned by USB to determine the amount of data transferred instead of
				// the data residue field in the CSW since some devices will report the wrong value.
				SetRealizedDataTransferCount( boRequestBlock->request, 
					GetRequestedDataTransferCount( boRequestBlock->request ) - bufferSizeRemaining);
            
            }
                    
			if ( resultingStatus == kIOReturnSuccess )
			{
				// Bulk transfer is done, get the Command Status Wrapper from the device
				status = BulkOnlyReceiveCSWPacket( boRequestBlock, kBulkOnlyStatusReceived );
				if ( status == kIOReturnSuccess )
				{
					commandInProgress = true;
				}
			}
			else if ( resultingStatus == kIOReturnOverrun )
			{
				
				// We set the data transfered to size of the request because the IOUSBFamily
				// discards the excess for us.
				
				SetRealizedDataTransferCount( boRequestBlock->request, 
					GetRequestedDataTransferCount( boRequestBlock->request ) );
					
				// Reset the device. We have to do a full device reset since a fair quantity of
				// stellar USB devices don't properly handle a mid I/O Bulk-Only device reset.
				ResetDeviceNow ( false );
				commandInProgress = true;
		
			}
			else
			{	
				// Either an error occurred on transfer or we did not get all the data we requested.
				// In either case, this transfer is complete, clean up and return an error to the client.
				
				if(( resultingStatus == kIOReturnDeviceError )
                    || ( resultingStatus == kIOReturnNotResponding )
                    || ( resultingStatus == kIOUSBHighSpeedSplitError ) )
                {
                	// Was there a device error? The device could have been removed or lost power.
           
                    ResetDeviceNow ( false );
                    status = kIOReturnSuccess;
                    if ( status == kIOReturnSuccess )
                    {
                        commandInProgress = true;
                    }
                }
				else if ( resultingStatus == kIOUSBTransactionTimeout )
				{
				
					// The device is so far gone that we couldn't even retry the CSW. Reset time.
                    if ( fUseUSBResetNotBOReset )
                    {
                    
                        // By passing this to Finish device recovery we ensure that the driver is still active,
                        // and that the device is still connected to the Mac.
                        ResetDeviceNow ( false );
                        status = kIOReturnSuccess;
                        
                    }
                    else
                    {
                        status = BulkDeviceResetDevice( boRequestBlock, kBulkOnlyResetCompleted );
                    }
					
					if( status == kIOReturnSuccess )
					{
						commandInProgress = true;
					}
					
				}
                else
                {
					// Check if the bulk endpoint was stalled.
                    
                    if ( GetDataTransferDirection( boRequestBlock->request ) == kSCSIDataTransfer_FromTargetToInitiator )
                    {
                        fPotentiallyStalledPipe = GetBulkInPipe();
                    }
                    else if ( GetDataTransferDirection( boRequestBlock->request ) == kSCSIDataTransfer_FromInitiatorToTarget )
                    {
                        fPotentiallyStalledPipe = GetBulkOutPipe();
                    }
                    else
                    {
                        fPotentiallyStalledPipe = GetControlPipe();
                    }
                    
                    boRequestBlock->currentState = kBulkOnlyCheckBulkStall;
                    status = GetStatusEndpointStatus( fPotentiallyStalledPipe, &boRequestBlock->boGetStatusBuffer, &boRequestBlock->boCompletion );

                    if ( status == kIOReturnSuccess )
                    {
                        commandInProgress = true;
                    }
				}
			}
		}
		break;
		
		
		case kBulkOnlyCheckBulkStall:
		case kBulkOnlyCheckBulkStallPostCSW:
		{
   			STATUS_LOG(( 5, "%s[%p]: kBulkOnlyCheckBulkStall returned %x", getName(), this, resultingStatus ));

			// Check to see if the endpoint was stalled
			if ( (boRequestBlock->boGetStatusBuffer[0] & 1) == 1 )
			{
				// Is this stall from the data or status phase?
				if ( boRequestBlock->currentState == kBulkOnlyCheckBulkStall )
				{
					boRequestBlock->currentState = kBulkOnlyClearBulkStall;
				}
				else
				{
					boRequestBlock->currentState = kBulkOnlyClearBulkStallPostCSW;
				}
				
				status = ClearFeatureEndpointStall( fPotentiallyStalledPipe, &boRequestBlock->boCompletion );
				if ( status == kIOReturnSuccess )
				{	
					fPotentiallyStalledPipe = NULL;
					commandInProgress = true;
				}
			}
			else
			{
				// If the endpoint was not stalled, attempt to get the CSW
				if ( fPotentiallyStalledPipe != NULL )
				{
					// Since the pipe was not stalled, but the host thought it was we should clear the host side of the pipe.
					fPotentiallyStalledPipe->Abort();
				}
				
				// Did we think we were stalled following the data or status phase?
				// We don't want to try to get the CSW again more than once.
				if ( boRequestBlock->currentState == kBulkOnlyCheckBulkStall )
				{
					// If the endpoint was not stalled, attempt to get the CSW	
					status = BulkOnlyReceiveCSWPacket( boRequestBlock, kBulkOnlyStatusReceived );
					
				}
				else
				{
					// If the endpoint was not stalled, attempt to get the CSW	
					status = BulkOnlyReceiveCSWPacket( boRequestBlock, kBulkOnlyStatusReceived2ndTime );
					
				}
				
				if ( status == kIOReturnSuccess )
				{
					commandInProgress = true;
				}
			}
		}
		break;
		
		case kBulkOnlyClearBulkStall:
		case kBulkOnlyClearBulkStallPostCSW:
		{
   			STATUS_LOG(( 5, "%s[%p]: kBulkOnlyClearBulkStall returned %x", getName(), this, resultingStatus ));

			// The pipe was stalled and an attempt to clear it was made
			// Try to get the CSW.  If the pipe was not successfully cleared, this will also
			// set off a device reset sequence.
			
			// If we arlready tried to get the CSW once, only try to get it once again.
			
			if ( boRequestBlock->currentState == kBulkOnlyClearBulkStall )
			{
				status = BulkOnlyReceiveCSWPacket( boRequestBlock, kBulkOnlyStatusReceived );
			}
			else
			{
				status = BulkOnlyReceiveCSWPacket( boRequestBlock, kBulkOnlyStatusReceived2ndTime );
			}
			
			if ( status == kIOReturnSuccess )
			{
				commandInProgress = true;
			}
		}
		break;
		
		case kBulkOnlyStatusReceived:
		{
   			STATUS_LOG(( 5, "%s[%p]: kBulkOnlyStatusReceived returned %x", getName(), this, resultingStatus ));
			
			// Bulk transfer is done, get the Command Status Wrapper from the device
			if ( resultingStatus == kIOUSBPipeStalled)
			{
				// An error occurred trying to get the CSW, we should clear any stalls and try to get the CSW again.
				boRequestBlock->currentState = kBulkOnlyCheckBulkStallPostCSW;
				status = GetStatusEndpointStatus( GetBulkInPipe(), &boRequestBlock->boGetStatusBuffer, &boRequestBlock->boCompletion );
				if ( status == kIOReturnSuccess )
				{	
					fPotentiallyStalledPipe = GetBulkInPipe();
					commandInProgress = true;
				}
			}
            else if ( resultingStatus != kIOReturnSuccess)
			{
				// An error occurred trying to get the first CSW, we should check and clear the stall,
				// and then try the CSW again
				status = BulkOnlyReceiveCSWPacket( boRequestBlock, kBulkOnlyStatusReceived2ndTime );
				if ( status != kIOReturnSuccess )
				{
                
                    // The device is so far gone that we couldn't even retry the CSW. Reset time.
                    if ( fUseUSBResetNotBOReset )
                    {
                    
                        // By passing this to Finish device recovery we ensure that the driver is still active,
                        // and that the device is still connected to the Mac.
                        ResetDeviceNow ( false );
                        status = kIOReturnSuccess;
                        
                    }
                    else
                    {
                        status = BulkDeviceResetDevice( boRequestBlock, kBulkOnlyResetCompleted );
                    }
                
                }
				
                if ( status == kIOReturnSuccess )
				{
					commandInProgress = true;
				}
                
			}
			else if ( ( boRequestBlock->boCSW.cswTag == boRequestBlock->boCBW.cbwTag ) || fKnownCSWTagMismatchIssues ) 
			{
				// Since the CBW and CSW tags match, process
				// the CSW to determine the appropriate response.
				switch( boRequestBlock->boCSW.cswStatus )
				{
					case kCSWCommandPassedError:
					{
						// The device reports no error on the command, and the realized data count was set after the bulk
						// data transfer completion state.  Return that the command was successfully completed.
						status = kIOReturnSuccess;
					}
					break;
					
					case kCSWCommandFailedError:
					{
						// The device reported an error for the command.
						STATUS_LOG(( 4, "%s[%p]: kBulkOnlyStatusReceived kCSWCommandFailedError", getName(), this ));
						status = kIOReturnError;
					}
					break;
					
					case kCSWPhaseError:
					{
						// The device reported a phase error on the command, perform the 
						// bulk reset on the device.
						STATUS_LOG(( 4, "%s[%p]: kBulkOnlyStatusReceived kCSWPhaseError", getName(), this ));
                        
                        if ( fUseUSBResetNotBOReset )
                        {
                            
                            // By passing this to Finish device recovery we ensure that the driver is still active,
                            // and that the device is still connected to the Mac.
                            ResetDeviceNow ( false );
                            status = kIOReturnSuccess;
                            
                        }
                        else
                        {
                            status = BulkDeviceResetDevice( boRequestBlock, kBulkOnlyResetCompleted );
                        }
                        
                        if( status == kIOReturnSuccess )
                        {
                            commandInProgress = true;
                        }
					}
					break;
					
					default:
					{
						STATUS_LOG(( 4, "%s[%p]: kBulkOnlyStatusReceived default", getName(), this ));
						// We received an unkown status, report an error to the client.
						status = kIOReturnError;
					}
					break;
				}
			}
			else
			{
				STATUS_LOG(( 5, "%s[%p]: kBulkOnlyStatusReceived tag mismatch", getName(), this ));
				// The only way to get to this point is if the command completes successfully,
				// but the CBW and CSW tags do not match.  Report an error to the client.
				status = kIOReturnError;
			}
		}
		break;
		
		case kBulkOnlyStatusReceived2ndTime:
		{
   			STATUS_LOG(( 5, "%s[%p]: kBulkOnlyStatusReceived2ndTime returned %x", getName(), this, resultingStatus ));

			// Second try for the CSW is done, if an error occurred, reset device.
			if ( resultingStatus != kIOReturnSuccess)
			{
                if ( fUseUSBResetNotBOReset )
                {
                
                    // By passing this to Finish device recovery we ensure that the driver is still active,
                    // and that the device is still connected to the Mac.
                    ResetDeviceNow ( false );
                    status = kIOReturnSuccess;
                    
                }
                else
                {
                    status = BulkDeviceResetDevice( boRequestBlock, kBulkOnlyResetCompleted );
                }
                
			}
			else
			{
				
				// Our second attempt to retrieve the CSW was successful. 
				// Re-enter the state machine to process the CSW packet.
				boRequestBlock->currentState = kBulkOnlyStatusReceived;
				BulkOnlyExecuteCommandCompletion(	boRequestBlock, 
													resultingStatus, 
													bufferSizeRemaining );
													
				status = kIOReturnSuccess;
						
			}
			
			if( status == kIOReturnSuccess )
			{
				commandInProgress = true;
			}
		}
		break;
			
		case kBulkOnlyResetCompleted:
		{
   			STATUS_LOG(( 5, "%s[%p]: kBulkOnlyResetCompleted returned %x", getName(), this, resultingStatus ));

			if ( resultingStatus != kIOReturnSuccess) 
			{
			
				// The Bulk-Only Reset failed. Try to recover the device.
				ResetDeviceNow ( false );
				commandInProgress = true;
				
				break;
			}
			
			boRequestBlock->currentState = kBulkOnlyClearBulkInCompleted;
			status = ClearFeatureEndpointStall( GetBulkInPipe(), &boRequestBlock->boCompletion );
			if ( status == kIOReturnSuccess )
			{
				commandInProgress = true;
			}
		}
		break;

		case kBulkOnlyClearBulkInCompleted:
		{
   			STATUS_LOG(( 5, "%s[%p]: kBulkOnlyClearBulkInCompleted returned %x", getName(), this, resultingStatus ));

			boRequestBlock->currentState = kBulkOnlyClearBulkOutCompleted;
			status = ClearFeatureEndpointStall( GetBulkOutPipe(), &boRequestBlock->boCompletion );
			if ( status == kIOReturnSuccess )
			{
				commandInProgress = true;
			}
		}
		break;
		
		case kBulkOnlyClearBulkOutCompleted:
		{
   			STATUS_LOG(( 5, "%s[%p]: kBulkOnlyClearBulkOutCompleted returned %x", getName(), this, resultingStatus ));

			SetRealizedDataTransferCount( boRequestBlock->request, 0 );
			status = kIOReturnError;
		}
		break;
		
		default:
		{
			SetRealizedDataTransferCount( boRequestBlock->request, 0 );
			status = kIOReturnError;
		}
		break;
	}
	
	
Exit:
	
	if ( commandInProgress == false )
	{	
		SCSITaskIdentifier	request = boRequestBlock->request;
		
		ReleaseBulkOnlyRequestBlock( boRequestBlock );
		CompleteSCSICommand( request, status );
	}
	STATUS_LOG(( 5, "%s[%p]: BulkOnlyExecuteCommandCompletion Returning", getName(), this ));
}
