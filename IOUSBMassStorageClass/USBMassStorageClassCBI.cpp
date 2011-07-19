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


//--------------------------------------------------------------------------------------------------
//	Includes
//--------------------------------------------------------------------------------------------------

#include "IOUSBMassStorageClass.h"
#include "IOUSBMassStorageClassTimestamps.h"
#include "Debugging.h"


//--------------------------------------------------------------------------------------------------
//	Macros
//--------------------------------------------------------------------------------------------------


// CBI State Machine States
enum
{
	kCBIExecuteCommand	= 1,			// Begin execution of user command
	kCBIExecuteCommandCompletion,		// Complete the user command
	kCBIBulkIOComplete,					// Complete the bulk I/O
	kCBIReadInterruptComplete,
	kCBIGetStatusControlEndpointComplete,
	kCBIClearControlEndpointComplete,
	kCBIGetStatusBulkEndpointComplete,
	kCBIClearBulkEndpointComplete
};

#pragma mark -
#pragma mark Protocol Services Methods


//--------------------------------------------------------------------------------------------------
//	AbortSCSICommandForCBIProtocol -	The AbortSCSICommand helper method for
//										CBI and CB protocol devices.					 [PROTECTED]
//--------------------------------------------------------------------------------------------------

IOReturn 
IOUSBMassStorageClass::AbortSCSICommandForCBIProtocol (
							SCSITaskIdentifier abortTask )
{

	UNUSED( abortTask );
	
	return kIOReturnError;
	
}


//--------------------------------------------------------------------------------------------------
//	SendSCSICommandForCBIProtocol - The SendSCSICommand helper method for CBI and CB protocol 
//									devices.											 [PROTECTED]
//--------------------------------------------------------------------------------------------------

IOReturn 
IOUSBMassStorageClass::SendSCSICommandForCBIProtocol ( SCSITaskIdentifier request )
{

	IOReturn			status;
	CBIRequestBlock *	theCBIRequestBlock;
	
	
	if ( fTerminating == true )
	{
 		// We have an invalid interface, the device has probably been removed.
 		// Nothing else to do except to report an error.
 		return kIOReturnDeviceError;
		
	}

	theCBIRequestBlock = GetCBIRequestBlock();

	bzero ( theCBIRequestBlock, sizeof ( CBIRequestBlock ) );
    
    // After having bzero'd the struct we need reset the cbiPhaseDesc to fCBIMemoryDescriptor.
    fCBICommandRequestBlock.cbiPhaseDesc = fCBIMemoryDescriptor;

	// Get a local copy of the callers cdb
	GetCommandDescriptorBlock ( request, &theCBIRequestBlock->cbiCDB );
	
	// Save the SCSI Task
	theCBIRequestBlock->request = request; 	
	
	// Set up the IOUSBCompletion structure
	theCBIRequestBlock->cbiCompletion.target 		= this;
	theCBIRequestBlock->cbiCompletion.action 		= &this->CBIProtocolUSBCompletionAction;
	theCBIRequestBlock->cbiCompletion.parameter 	= theCBIRequestBlock;

	theCBIRequestBlock->currentState 				= kCBIExecuteCommand;

	// Build the USB command	
    theCBIRequestBlock->cbiDevRequest.bmRequestType 	= USBmakebmRequestType ( kUSBOut, kUSBClass, kUSBInterface );	
   	theCBIRequestBlock->cbiDevRequest.bRequest 			= 0;
   	theCBIRequestBlock->cbiDevRequest.wValue			= 0;
	theCBIRequestBlock->cbiDevRequest.wIndex			= GetInterfaceReference()->GetInterfaceNumber();
	theCBIRequestBlock->cbiDevRequest.wLength			= 12; //kCommandMaxCDBSize
   	theCBIRequestBlock->cbiDevRequest.pData				= &theCBIRequestBlock->cbiCDB;

	// Send the command over the control endpoint
	status = GetInterfaceReference()->GetDevice()->DeviceRequest ( 	
												&theCBIRequestBlock->cbiDevRequest, 
												GetTimeoutDuration( theCBIRequestBlock->request ),  // Use the client's timeout
												GetTimeoutDuration( theCBIRequestBlock->request ),  // Use the client's timeout
												&theCBIRequestBlock->cbiCompletion );
   	STATUS_LOG ( ( 5, "%s[%p]: SendSCSICommandForCBIProtocol DeviceRequest returned %x", getName(), this, status ) );
   	
	return status;
	
}

#pragma mark -
#pragma mark SendSCSICommand Helper methods


//--------------------------------------------------------------------------------------------------
//	CBIProtocolUSBCompletionAction														 [PROTECTED]
//--------------------------------------------------------------------------------------------------

void 
IOUSBMassStorageClass::CBIProtocolUSBCompletionAction (
					                void *			target,
					                void *			parameter,
					                IOReturn		status,
					                UInt32			bufferSizeRemaining)
{

	IOUSBMassStorageClass *		theMSC;
	CBIRequestBlock	*			cbiRequestBlock;
	
	
	theMSC 				= ( IOUSBMassStorageClass * ) target;
	cbiRequestBlock 	= ( CBIRequestBlock * ) parameter;
	theMSC->CBIProtocolCommandCompletion ( 	cbiRequestBlock, 
											status, 
											bufferSizeRemaining );
											
}


//--------------------------------------------------------------------------------------------------
//	CBIProtocolTransferData																 [PROTECTED]
//--------------------------------------------------------------------------------------------------

IOReturn 
IOUSBMassStorageClass::CBIProtocolTransferData ( 
						CBIRequestBlock *		cbiRequestBlock,
						UInt32					nextExecutionState )
{

	IOReturn	status = kIOReturnError;
	
	
	// Set the next state to be executed
	cbiRequestBlock->currentState = nextExecutionState;

	// Start a bulk in or out transaction
	if ( GetDataTransferDirection ( cbiRequestBlock->request ) == kSCSIDataTransfer_FromTargetToInitiator )
	{
	
		status = GetBulkInPipe()->Read ( 
					GetDataBuffer ( cbiRequestBlock->request ), 
					GetTimeoutDuration ( cbiRequestBlock->request ),  // Use the client's timeout for both
					GetTimeoutDuration ( cbiRequestBlock->request ),
					GetRequestedDataTransferCount ( cbiRequestBlock->request ),
					&cbiRequestBlock->cbiCompletion );
					
	}
	else if ( GetDataTransferDirection ( cbiRequestBlock->request ) == kSCSIDataTransfer_FromInitiatorToTarget )
	{ 
	
		status = GetBulkOutPipe()->Write (
					GetDataBuffer ( cbiRequestBlock->request ), 
					GetTimeoutDuration ( cbiRequestBlock->request ),  // Use the client's timeout for both
					GetTimeoutDuration ( cbiRequestBlock->request ),
					GetRequestedDataTransferCount ( cbiRequestBlock->request ),
					&cbiRequestBlock->cbiCompletion );
					
	}

   	STATUS_LOG ( ( 5, "%s[%p]: CBIProtocolTransferData returned %x", getName(), this, status ) );
	return status;
	
}


//--------------------------------------------------------------------------------------------------
//	CBIProtocolReadInterrupt															 [PROTECTED]
//--------------------------------------------------------------------------------------------------

IOReturn 
IOUSBMassStorageClass::CBIProtocolReadInterrupt ( 
						CBIRequestBlock *		cbiRequestBlock,
						UInt32					nextExecutionState )
{

	IOReturn 			status  =   kIOReturnError;
	

    // Ensure we still have a valid IOMemoryDescriptor.
	require ( ( cbiRequestBlock->cbiPhaseDesc != NULL ), Exit );
	
	// Set the next state to be executed
	cbiRequestBlock->currentState = nextExecutionState;

	// Start a read from the interrupt pipe
	status = GetInterruptPipe()->Read ( cbiRequestBlock->cbiPhaseDesc, &cbiRequestBlock->cbiCompletion);
   	STATUS_LOG ( ( 5, "%s[%p]: CBIProtocolReadInterrupt returned %x", getName(), this, status ) );
	
    
Exit:
    
	return status;
	
}


//--------------------------------------------------------------------------------------------------
//	CBIGetStatusEndpointStatus															 [PROTECTED]
//--------------------------------------------------------------------------------------------------

IOReturn 
IOUSBMassStorageClass::CBIGetStatusEndpointStatus( 
						IOUSBPipe *				targetPipe,
						CBIRequestBlock *		cbiRequestBlock,
						UInt32					nextExecutionState )
{

	IOReturn 			status;
	
	
	if( targetPipe == NULL )
	{
		// We need to check if the pipe is NULL
		status = kIOReturnError;
		goto ErrorExit;
	}
	
	// Set the next state to be executed
	cbiRequestBlock->currentState = nextExecutionState;
	
	// Call the default GetStatusEndpointStatus method
	status = GetStatusEndpointStatus ( targetPipe, &cbiRequestBlock->cbiGetStatusBuffer, &cbiRequestBlock->cbiCompletion );
   	STATUS_LOG ( ( 5, "%s[%p]: CBIGetStatusEndpointStatus returned %x", getName(), this, status ) );
	
	
ErrorExit:
	
	return status;
	
}


//--------------------------------------------------------------------------------------------------
//	CBIClearFeatureEndpointStall														 [PROTECTED]
//--------------------------------------------------------------------------------------------------

IOReturn 
IOUSBMassStorageClass::CBIClearFeatureEndpointStall( 
						IOUSBPipe *				targetPipe,
						CBIRequestBlock *		cbiRequestBlock,
						UInt32					nextExecutionState )
{

	IOReturn 			status;
	
	
	if( targetPipe == NULL )
	{
		// We need to check if the pipe is NULL (ie if we are being terminated).
		status = kIOReturnError;
		goto ErrorExit;
	}
	
	// Set the next state to be executed
	cbiRequestBlock->currentState = nextExecutionState;
	
	// Call the default ClearFeatureEndpointStall method
	status = ClearFeatureEndpointStall ( targetPipe, &cbiRequestBlock->cbiCompletion );
   	STATUS_LOG ( ( 5, "%s[%p]: CBIClearFeatureEndpointStall returned %x", getName(), this, status ) );
	
	
ErrorExit:
	
	return status;
	
}


//--------------------------------------------------------------------------------------------------
//	CBIProtocolCommandCompletion														 [PROTECTED]
//--------------------------------------------------------------------------------------------------

void 
IOUSBMassStorageClass::CBIProtocolCommandCompletion(
						CBIRequestBlock *		cbiRequestBlock,
		                IOReturn				resultingStatus,
		                UInt32					bufferSizeRemaining )
{

	IOReturn 		status = kIOReturnError;
	bool			commandInProgress = false;
	
	
	if ( ( cbiRequestBlock->request == NULL ) || ( fCBICommandStructInUse == false ) )
	{
		// The request field is NULL, this appears to be a double callback, do nothing.
        // OR the command was aborted earlier, do nothing.
		STATUS_LOG(( 4, "%s[%p]: cbiRequestBlock->request is NULL, returned %x", getName(), this, resultingStatus ));
		return;
		
	}
	
	if ( (  GetInterfaceReference() == NULL ) || ( fTerminating == true ) )
	{
		// Our interface has been closed, probably because of an
		// unplug, return an error for the command since there it
		// can no longer be executed.
		
		STATUS_LOG ( ( 4, "%s[%p]: Completion during termination", getName(), this ) );
		goto Exit;
		
	}
	
    
    if ( ( resultingStatus == kIOReturnNotResponding ) || ( resultingStatus == kIOReturnAborted ) )
	{
        
		STATUS_LOG(( 5, "%s[%p]: CBIProtocolCommandCompletion previous command returned %x", getName(), this, resultingStatus ));
		
		// The transfer failed mid-transfer or was aborted by the USB layer. Either way the device will
        // be non-responsive until we reset it, or we discover it has been disconnected.
		ResetDeviceNow ( false );
		commandInProgress = true; 
		goto Exit;
		
	}
    
	RecordUSBTimeStamp (	UMC_TRACE ( kCBICompletion ), ( uintptr_t ) this, resultingStatus, 
							( unsigned int ) cbiRequestBlock->currentState, ( uintptr_t ) cbiRequestBlock->request );
	
	switch ( cbiRequestBlock->currentState )
	{
	
		case kCBIExecuteCommand:		// Device request completion
		{
		
   			STATUS_LOG(( 5, "%s[%p]: kCBIExecuteCommand status %x", getName(), this, resultingStatus ));
			
#if defined (__i386__) || defined (__x86_64__)
			// For UHCI.
			// First check to see if an error occurred on sending the command to the device.
			if ( resultingStatus == kIOUSBPipeStalled )
			{
			
				status = CBIClearFeatureEndpointStall ( GetControlPipe(), cbiRequestBlock, kCBIClearBulkEndpointComplete );
				if ( status == kIOReturnSuccess )
				{
					commandInProgress = true;
				}
				
				break;
			}
#endif
			
			// First check to see if an error occurred on the command out
			if ( resultingStatus != kIOReturnSuccess )
			{
			
				status = CBIGetStatusEndpointStatus ( GetControlPipe(), cbiRequestBlock, kCBIGetStatusControlEndpointComplete );
				if ( status == kIOReturnSuccess )
				{
					commandInProgress = true;
				}
   				
   				STATUS_LOG ( ( 4, "%s[%p]: kCBIExecuteCommand GetStatusEndpointStatus status %x", getName(), this, status ) );
				
			}
			else
			{
				// If there is to be no data transfer then we are done and can return to the caller
				// We will only get to here if no Error occurred.
				if ( GetDataTransferDirection ( cbiRequestBlock->request ) == kSCSIDataTransfer_NoDataTransfer )
				{
				
					status = kIOReturnSuccess;
   					STATUS_LOG ( ( 5, "%s[%p]: kCBIExecuteCommand no data to transfer status %x", getName(), this, status ) );
					break;
					
				}
				
				status = CBIProtocolTransferData ( cbiRequestBlock, kCBIBulkIOComplete );
				if ( status == kIOReturnSuccess )
				{
					commandInProgress = true;
				}

   				STATUS_LOG ( ( 5, "%s[%p]: kCBIExecuteCommand CBIProtocolTransferData status %x", getName(), this, status ) );
				
			}
			
		}
		break;
		
		case kCBIBulkIOComplete:
		{
		
   			STATUS_LOG ( ( 5, "%s[%p]: kCBIBulkIOComplete status %x", getName(), this, resultingStatus ) );
			if ( resultingStatus == kIOReturnOverrun )
			{
				// If we got more data than expected, act like we got exactly the amount
				// requested.
				resultingStatus = kIOReturnSuccess;
				SetRealizedDataTransferCount ( cbiRequestBlock->request, GetRequestedDataTransferCount( cbiRequestBlock->request ) );

				// Clear the halt status on the host side for the pipe in use.
				if ( GetDataTransferDirection ( cbiRequestBlock->request ) == kSCSIDataTransfer_FromTargetToInitiator )
				{
					GetBulkInPipe()->Reset();
				}
				else if ( GetDataTransferDirection ( cbiRequestBlock->request ) == kSCSIDataTransfer_FromInitiatorToTarget )
				{
					GetBulkOutPipe()->Reset();
				}
				
			}
			else
			{
				SetRealizedDataTransferCount ( cbiRequestBlock->request, GetRequestedDataTransferCount( cbiRequestBlock->request ) - bufferSizeRemaining );
			}
			
#if defined (__i386__) || defined (__x86_64__)
			// For UHCI.
			if ( resultingStatus == kIOUSBPipeStalled )
			{
			
				IOUSBPipe * thePipe = NULL;
				
				if ( GetDataTransferDirection ( cbiRequestBlock->request ) == kSCSIDataTransfer_FromTargetToInitiator )
				{
					thePipe = GetBulkInPipe();
				}
				else if ( GetDataTransferDirection ( cbiRequestBlock->request ) == kSCSIDataTransfer_FromInitiatorToTarget )
				{
					thePipe = GetBulkOutPipe();
				}
				
				if ( thePipe != NULL )
				{
				
					status = CBIClearFeatureEndpointStall ( thePipe, cbiRequestBlock, kCBIClearBulkEndpointComplete );
					if ( status == kIOReturnSuccess )
					{
						commandInProgress = true;
						
					}
					break;
					
				}
				
			}
#endif

			if ( resultingStatus != kIOReturnSuccess )
			{
				// Check if the bulk endpoint was stalled
				IOUSBPipe * thePipe = NULL;

				if ( GetDataTransferDirection( cbiRequestBlock->request ) == kSCSIDataTransfer_FromTargetToInitiator )
				{
					thePipe = GetBulkInPipe();
				}
				else if ( GetDataTransferDirection( cbiRequestBlock->request ) == kSCSIDataTransfer_FromInitiatorToTarget )
				{
					thePipe = GetBulkOutPipe();
				}
				else
				{
					thePipe = GetControlPipe();
				}

				status = CBIGetStatusEndpointStatus ( thePipe, cbiRequestBlock, kCBIGetStatusBulkEndpointComplete );
				if ( status == kIOReturnSuccess )
				{
					commandInProgress = true;
				}
   					
   				STATUS_LOG ( ( 5, "%s[%p]: kCBIBulkIOComplete GetStatusEndpointStatus status %x", getName(), this, status ) );
				
			}
			else
			{
			
				if ( ( GetInterruptPipe() != NULL ) && ( GetDataTransferDirection ( cbiRequestBlock->request ) == kSCSIDataTransfer_FromInitiatorToTarget )
						&& ( ( GetInterfaceSubclass() == kUSBStorageSFF8070iSubclass ) || ( GetInterfaceSubclass() == kUSBStorageUFISubclass ) ) )
				{
					// We have an interrupt pipe, and device uses it to determine a command is done
					status = CBIProtocolReadInterrupt ( cbiRequestBlock, kCBIReadInterruptComplete );
					if ( status == kIOReturnSuccess )
					{
						commandInProgress = true;
					}

   					STATUS_LOG(( 5, "%s[%p]: kCBIBulkIOComplete CBIProtocolReadInterrupt status %x", getName(), this, status ));
					
				}
				else
				{
					status = kIOReturnSuccess;
				}
				
			}
			
		}
		break;

		case kCBIReadInterruptComplete:
		{
   			STATUS_LOG(( 5, "%s[%p]: kCBIReadInterruptComplete status %x", getName(), this, resultingStatus ));
			
			// What should the status really be, should probably process and return
			// a relevent error.
			if ( ( resultingStatus == kIOReturnSuccess ) && ( ( GetInterfaceSubclass() == kUSBStorageSFF8070iSubclass ) || ( GetInterfaceSubclass() == kUSBStorageUFISubclass ) ) )
			{
				if ( GetInterfaceSubclass() == kUSBStorageUFISubclass )
				{
					// Decide what error to return based on the Interrupt data
					if ( ( cbiRequestBlock->cbiGetStatusBuffer[0] == 0x00 ) && ( cbiRequestBlock->cbiGetStatusBuffer[1] == 0x00 ) )
					{
						status = kIOReturnSuccess;
					}
					else
					{
						status = kIOReturnError;
					}
					
				}
				else // This is probably a kUSBStorageSFF8070iSubclass device but in the future may include others as well
				{
					// As per the USB Mass Storage Class CBI Transport Specification 3.4.3.1.1 Common Interrupt Data Block
					if ( ( cbiRequestBlock->cbiGetStatusBuffer[0] == 0x00 ) &&
						 ( ( cbiRequestBlock->cbiGetStatusBuffer[1] & 0x3 ) != 0 ) )
					{
						status = kIOReturnError;
					}
					else
					{
						status = kIOReturnSuccess;
					}
					
				}
			}
			else
			{
				// The Class doesn't know how to interpret the data
				// return an error and mark interrupt data as invalid
				status = kIOReturnError;
				
			}
   			
   			STATUS_LOG ( ( 5, "%s[%p]: kCBIReadInterruptComplete ending status %x", getName(), this, status ) );
		}
		break;

		case kCBIGetStatusControlEndpointComplete:
		{
   			STATUS_LOG ( ( 5, "%s[%p]: kCBIGetStatusControlEndpointComplete status %x", getName(), this, resultingStatus ) );

			if ( resultingStatus == kIOReturnSuccess )
			{
			
				if ( ( cbiRequestBlock->cbiGetStatusBuffer[0] & 1 ) == 1 ) 
				{
					// This endpoint was stalled, go ahead and clear it
					status = CBIClearFeatureEndpointStall ( GetControlPipe(), cbiRequestBlock, kCBIClearControlEndpointComplete );
					if ( status == kIOReturnSuccess )
					{
						commandInProgress = true;
					}

   					STATUS_LOG ( ( 5, "%s[%p]: kCBIGetStatusControlEndpointComplete CBIClearFeatureEndpointStall status %x", getName(), this, status ) );
					
				}
				else
				{
				
					if ( GetDataTransferDirection ( cbiRequestBlock->request ) == kSCSIDataTransfer_NoDataTransfer )
					{
					
						SetRealizedDataTransferCount ( cbiRequestBlock->request, 0 );
						status = kIOReturnError;
						
					}
					else
					{
						// Check if the bulk endpoint was stalled
						IOUSBPipe * thePipe = NULL;

						if ( GetDataTransferDirection ( cbiRequestBlock->request ) == kSCSIDataTransfer_FromTargetToInitiator )
						{
							thePipe = GetBulkInPipe();
						}
						else if ( GetDataTransferDirection ( cbiRequestBlock->request ) == kSCSIDataTransfer_FromInitiatorToTarget )
						{
							thePipe = GetBulkOutPipe();
						}
						else
						{
							thePipe = GetControlPipe();
						}
				
						status = CBIGetStatusEndpointStatus ( GetControlPipe(), cbiRequestBlock, kCBIGetStatusBulkEndpointComplete );
						if ( status == kIOReturnSuccess )
						{
							commandInProgress = true;
						}

   						STATUS_LOG ( ( 5, "%s[%p]: kCBIGetStatusControlEndpointComplete CBIGetStatusEndpointStatus status %x", getName(), this, status ) );
					
					}
				}
			}
			else
			{
				// An error occurred to GET_STATUS ( shouldn't happen!!) reset the endpoint anyway
				status = CBIClearFeatureEndpointStall ( GetControlPipe(), cbiRequestBlock, kCBIClearControlEndpointComplete );
				if ( status == kIOReturnSuccess )
				{
					commandInProgress = true;
				}
   						
   				STATUS_LOG(( 5, "%s[%p]: kCBIGetStatusControlEndpointComplete CBIClearFeatureEndpointStall status %x", getName(), this, status ));
			
			}
		}
		break;
		
		case kCBIClearControlEndpointComplete:
		{
   			STATUS_LOG ( ( 5, "%s[%p]: kCBIClearControlEndpointComplete status %x", getName(), this, resultingStatus ) );

			if (resultingStatus == kIOReturnSuccess)
			{
			
				if ( GetDataTransferDirection ( cbiRequestBlock->request ) == kSCSIDataTransfer_NoDataTransfer )
				{
				
					SetRealizedDataTransferCount ( cbiRequestBlock->request, 0 );
					status = kIOReturnError;
					
				}
				else
				{
					// Check if the bulk endpoint was stalled
					IOUSBPipe * thePipe = NULL;

					if ( GetDataTransferDirection ( cbiRequestBlock->request ) == kSCSIDataTransfer_FromTargetToInitiator )
					{
						thePipe = GetBulkInPipe();
					}
					else if ( GetDataTransferDirection ( cbiRequestBlock->request ) == kSCSIDataTransfer_FromInitiatorToTarget )
					{
						thePipe = GetBulkOutPipe();
					}
					else
					{
						thePipe = GetControlPipe();
					}
			
					status = CBIGetStatusEndpointStatus ( thePipe, cbiRequestBlock, kCBIGetStatusBulkEndpointComplete );
					if ( status == kIOReturnSuccess )
					{
						commandInProgress = true;
					}

   					STATUS_LOG ( ( 5, "%s[%p]: kCBIClearControlEndpointComplete CBIGetStatusEndpointStatus status %x", getName(), this, status ) );
					
				}
				
			}
			else
			{
				status = resultingStatus;
			}
			
		}
		break;
		
		case kCBIGetStatusBulkEndpointComplete:
		{
   			STATUS_LOG ( ( 5, "%s[%p]: kCBIGetStatusBulkEndpointComplete status %x", getName(), this, resultingStatus ) );

			if ( resultingStatus == kIOReturnSuccess )
			{
			
				if ( ( cbiRequestBlock->cbiGetStatusBuffer[0] & 1 ) == 1 ) 
				{
				
					IOUSBPipe * thePipe = NULL;
					
					if ( GetDataTransferDirection( cbiRequestBlock->request ) == kSCSIDataTransfer_FromTargetToInitiator )
					{
						thePipe = GetBulkInPipe();
					}
					else if ( GetDataTransferDirection( cbiRequestBlock->request ) == kSCSIDataTransfer_FromInitiatorToTarget )
					{
						thePipe = GetBulkOutPipe();
					}
					else
					{
						thePipe = GetControlPipe();
					}
					
					status = CBIClearFeatureEndpointStall ( thePipe, cbiRequestBlock, kCBIClearBulkEndpointComplete );
					if ( status == kIOReturnSuccess )
					{
						commandInProgress = true;
					}
   					
   					STATUS_LOG(( 5, "%s[%p]: kCBIGetStatusBulkEndpointComplete CBIClearFeatureEndpointStall status %x", getName(), this, status ));
					
				}
				else
				{
				
					SetRealizedDataTransferCount( cbiRequestBlock->request, 0 );
					status = kIOReturnError;
					
				}
				
			}
			else
			{
			
				IOUSBPipe * thePipe = NULL;
				
				if ( GetDataTransferDirection( cbiRequestBlock->request ) == kSCSIDataTransfer_FromTargetToInitiator )
				{
					thePipe = GetBulkInPipe();
				}
				else if ( GetDataTransferDirection( cbiRequestBlock->request ) == kSCSIDataTransfer_FromInitiatorToTarget )
				{
					thePipe = GetBulkOutPipe();
				}
				else
				{
					thePipe = GetControlPipe();
				}
				
				status = CBIClearFeatureEndpointStall ( thePipe, cbiRequestBlock, kCBIClearBulkEndpointComplete );
				if ( status == kIOReturnSuccess )
				{
					commandInProgress = true;
				}

   				STATUS_LOG(( 5, "%s[%p]: kCBIGetStatusBulkEndpointComplete CBIClearFeatureEndpointStall status %x", getName(), this, status ));
				
			}
		}
		break;

		case kCBIClearBulkEndpointComplete:
		{
		
   			STATUS_LOG ( ( 5, "%s[%p]: kCBIClearBulkEndpointComplete status %x", getName(), this, resultingStatus ) );

			SetRealizedDataTransferCount ( cbiRequestBlock->request, 0 );
			status = kIOReturnError;
			
		}
		break;
		
		default:
		{
		
   			STATUS_LOG ( ( 5, "%s[%p]: default case status %x", getName(), this, resultingStatus ) );

			SetRealizedDataTransferCount ( cbiRequestBlock->request, 0 );
			status = kIOReturnError;
			
		}
		break;
		
	}
    

Exit:
    

	// If the command has been completed ( no longer pending ), call the clients completion routine.	
	if ( commandInProgress == false )
	{
	
		SCSITaskIdentifier	request = cbiRequestBlock->request;
		
		ReleaseCBIRequestBlock ( cbiRequestBlock );
		CompleteSCSICommand ( request, status );
		
	}
}

