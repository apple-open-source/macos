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

#include <libkern/OSByteOrder.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOCommandGate.h>

#include <IOKit/scsi-commands/IOSCSIProtocolServices.h>

// For debugging, set SCSI_PROTOCOL_SERVICES_DEBUGGING_LEVEL to one
// of the following values:
//		0	No debugging 	(GM release level)
// 		1 	PANIC_NOW only
//		2	PANIC_NOW and ERROR_LOG
//		3	PANIC_NOW, ERROR_LOG and STATUS_LOG


#if ( SCSI_PROTOCOL_SERVICES_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_PROTOCOL_SERVICES_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_PROTOCOL_SERVICES_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif


#define super IOSCSIProtocolInterface
OSDefineMetaClass ( IOSCSIProtocolServices, IOSCSIProtocolInterface );
OSDefineAbstractStructors ( IOSCSIProtocolServices, IOSCSIProtocolInterface );

// Space reserved for future expansion.
OSMetaClassDefineReservedUnused( IOSCSIProtocolServices, 1 );
OSMetaClassDefineReservedUnused( IOSCSIProtocolServices, 2 );
OSMetaClassDefineReservedUnused( IOSCSIProtocolServices, 3 );
OSMetaClassDefineReservedUnused( IOSCSIProtocolServices, 4 );
OSMetaClassDefineReservedUnused( IOSCSIProtocolServices, 5 );
OSMetaClassDefineReservedUnused( IOSCSIProtocolServices, 6 );
OSMetaClassDefineReservedUnused( IOSCSIProtocolServices, 7 );
OSMetaClassDefineReservedUnused( IOSCSIProtocolServices, 8 );
OSMetaClassDefineReservedUnused( IOSCSIProtocolServices, 9 );
OSMetaClassDefineReservedUnused( IOSCSIProtocolServices, 10 );
OSMetaClassDefineReservedUnused( IOSCSIProtocolServices, 11 );
OSMetaClassDefineReservedUnused( IOSCSIProtocolServices, 12 );
OSMetaClassDefineReservedUnused( IOSCSIProtocolServices, 13 );
OSMetaClassDefineReservedUnused( IOSCSIProtocolServices, 14 );
OSMetaClassDefineReservedUnused( IOSCSIProtocolServices, 15 );
OSMetaClassDefineReservedUnused( IOSCSIProtocolServices, 16 );

/*
struct IOPMPowerState
{
	UInt32			version;				// version number of this struct
	IOPMPowerFlags	capabilityFlags;		// bits that describe (to interested drivers) the capability of the device in this state 
	IOPMPowerFlags	outputPowerCharacter;	// description (to power domain children) of the power provided in this state 
	IOPMPowerFlags	inputPowerRequirement;	// description (to power domain parent) of input power required in this state
	UInt32			staticPower;			// average consumption in milliwatts
	UInt32			unbudgetedPower;		// additional consumption from separate power supply (mw)
	UInt32			powerToAttain;			// additional power to attain this state from next lower state (in mw)
	UInt32			timeToAttain;			// time required to enter this state from next lower state (in microseconds)
	UInt32			settleUpTime;			// settle time required after entering this state from next lower state (microseconds)
	UInt32			timeToLower;			// time required to enter next lower state from this one (in microseconds)
	UInt32			settleDownTime;			// settle time required after entering next lower state from this state (microseconds)
	UInt32			powerDomainBudget;		// power in mw a domain in this state can deliver to its children
};
*/

// Used by power manager to figure out what states we support
// The default implementation supports two basic states: ON and OFF
// ON state means the device can be used on this transport layer
// OFF means the device cannot receive any I/O on this transport layer
static IOPMPowerState sPowerStates[kSCSIProtocolLayerNumDefaultStates] =
{
	{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ 1, (IOPMDeviceUsable | IOPMMaxPerformance), IOPMPowerOn, IOPMPowerOn, 0, 0, 0, 0, 0, 0, 0, 0 }
};


UInt32
IOSCSIProtocolServices::GetInitialPowerState ( void )
{
	
	STATUS_LOG ( ( "%s%s::%s called%s\n", "\033[33m", getName ( ), __FUNCTION__, "\033[0m" ) );
	return fCurrentPowerState;
	
}


#pragma mark -
#pragma mark Public Methods


bool
IOSCSIProtocolServices::init ( OSDictionary * propTable )
{
	
	if ( super::init ( propTable ) == false )
	{
		return false;
	}
	
	return true;
	
}


bool
IOSCSIProtocolServices::start ( IOService * provider )
{
	
	if ( !super::start ( provider ) )
	{
		return false;
	}
	
	// Setup to allow service requests
	fAllowServiceRequests = true;
	
	// Initialize the head pointer for the SCSI Task Queue.
	fSCSITaskQueueHead = NULL;
	
	// Allocate the mutex for accessing the SCSI Task Queue.
	fQueueLock = IOSimpleLockAlloc ( );
	if ( fQueueLock == NULL )
	{
		PANIC_NOW ( ( "IOSCSIProtocolServices::start Allocate fQueueLock failed." ) );
	}
	
	return true;
	
}


void
IOSCSIProtocolServices::stop ( IOService * provider )
{
	
	if ( fQueueLock != NULL )
	{
		
		// Free the SCSI Task queue mutex.
		IOSimpleLockFree ( fQueueLock );
		fQueueLock = NULL;
		
	}
	
	super::stop ( provider );
	
}


#pragma mark -
#pragma mark Power Management Methods


void
IOSCSIProtocolServices::InitializePowerManagement ( IOService * provider )
{
	
	STATUS_LOG ( ( "%s%s::%s called%s\n", "\033[33m", getName ( ), __FUNCTION__, "\033[0m" ) );
	
	fCurrentPowerState = kSCSIProtocolLayerPowerStateOn;
	
	// Call our super to initialize PM vars and to join the power
	// management tree
	super::InitializePowerManagement ( provider );
	
	// Register this piece with power management as the "policy maker"
	// i.e. the thing that controls power management for the protocol layer
	registerPowerDriver ( this, sPowerStates, kSCSIProtocolLayerNumDefaultStates );
	
	// make sure we default to on state
	changePowerStateTo ( kSCSIProtocolLayerPowerStateOn );
	
}


void
IOSCSIProtocolServices::HandlePowerChange ( void )
{
	
	IOReturn	status;
	
	STATUS_LOG ( ( "%s%s::%s called%s\n", "\033[33m",
						getName ( ), __FUNCTION__, "\033[0m" ) );
	
	STATUS_LOG ( ( "fProposedPowerState = %ld, fCurrentPowerState = %ld\n",
						fProposedPowerState, fCurrentPowerState ) );
	
	
	while ( fProposedPowerState != fCurrentPowerState )
	{
		
		STATUS_LOG ( ( "Looping because power states differ\n" ) );
		
		switch ( fProposedPowerState )
		{
		
			case kSCSIProtocolLayerPowerStateOff:
				status = HandlePowerOff ( );
				STATUS_LOG ( ( "HandlePowerOff returned status = %d\n", status ) );
				if ( status == kIOReturnSuccess )
				{
					fCurrentPowerState = kSCSIProtocolLayerPowerStateOff;
				}
				break;
			
			case kSCSIProtocolLayerPowerStateOn:
				status = HandlePowerOn ( );
				STATUS_LOG ( ( "HandlePowerOn returned status = %d\n", status ) );
				if ( status == kIOReturnSuccess )
				{
					fCurrentPowerState = kSCSIProtocolLayerPowerStateOn;
				}
				break;
				
			default:
				PANIC_NOW ( ( "HandlePowerChange: bad proposed power state\n" ) );
				break;
			
		}
		
	}
	
}


void
IOSCSIProtocolServices::HandleCheckPowerState ( void )
{
	
	super::HandleCheckPowerState ( kSCSIProtocolLayerPowerStateOn );
	
}


void
IOSCSIProtocolServices::TicklePowerManager ( void )
{
	
	super::TicklePowerManager ( kSCSIProtocolLayerPowerStateOn );
	
}


IOReturn
IOSCSIProtocolServices::HandlePowerOff ( void )
{
	
	return kIOReturnSuccess;
	
}


IOReturn
IOSCSIProtocolServices::HandlePowerOn ( void )
{
	
	return kIOReturnSuccess;
	
}


#pragma mark -
#pragma mark Status Notification Senders


void
IOSCSIProtocolServices::SendNotification_DeviceRemoved ( void )
{
	
	STATUS_LOG ( ( "%s: SendNotification_DeviceRemoved called\n", getName ( ) ) );
	
	// Set the flag to prevent execution of any other service requests
	fAllowServiceRequests = false;
	
	STATUS_LOG ( ( "%s: SendNotification_DeviceRemoved Reject queued tasks\n", getName ( ) ) );
	
	// Remove all tasks from the queue.
	RejectSCSITasksCurrentlyQueued ( );
	
}


void
IOSCSIProtocolServices::SendNotification_VerifyDeviceState( void )
{
	
	STATUS_LOG ( ("%s: SendNotification_VerifyDeviceState called\n", getName ( ) ) );
	
	// Send message up to SCSI Application Layer.
	messageClients ( kSCSIProtocolNotification_VerifyDeviceState );
	
}


#pragma mark -
#pragma mark SCSI Task Field Accessors


// ---- Utility methods for accessing SCSITask attributes ----
SCSITaskAttribute
IOSCSIProtocolServices::GetTaskAttribute ( SCSITaskIdentifier request )
{
	
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->GetTaskAttribute ( );
	
}


bool
IOSCSIProtocolServices::SetTaskState ( 	SCSITaskIdentifier request,
										SCSITaskState newTaskState )
{
	
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->SetTaskState ( newTaskState );
	
}


SCSITaskState
IOSCSIProtocolServices::GetTaskState ( SCSITaskIdentifier request )
{
	
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->GetTaskState ( );
	
}


UInt8
IOSCSIProtocolServices::GetLogicalUnitNumber( SCSITaskIdentifier request )
{
	SCSITask *	scsiRequest;
	
    scsiRequest = OSDynamicCast( SCSITask, request );
	return scsiRequest->GetLogicalUnitNumber();
}


UInt8
IOSCSIProtocolServices::GetCommandDescriptorBlockSize ( SCSITaskIdentifier request )
{
	
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	
	// Check to see what the current execution mode is  
	if ( scsiRequest->GetTaskExecutionMode() == kSCSITaskMode_CommandExecution )
	{
		return scsiRequest->GetCommandDescriptorBlockSize ( );
	}
	
	else
	{
		return scsiRequest->GetAutosenseCommandDescriptorBlockSize ( );
	}
	
}


// This will always return a 16 Byte CDB.  If the Protocol Layer driver does not
// support 16 Byte CDBs, it will have to create a local SCSICommandDescriptorBlock
// variable to get the CDB data and then transfer the needed bytes from there.
bool
IOSCSIProtocolServices::GetCommandDescriptorBlock (
												SCSITaskIdentifier request,
												SCSICommandDescriptorBlock * cdbData )
{
	
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	
	// Check to see what the current execution mode is  
	if ( scsiRequest->GetTaskExecutionMode ( ) == kSCSITaskMode_CommandExecution )
	{
   	 	return scsiRequest->GetCommandDescriptorBlock ( cdbData );
	}
	
	else
	{
		return scsiRequest->GetAutosenseCommandDescriptorBlock ( cdbData );
	}
	
}


// Get the control information for the transfer, including
// the transfer direction and the number of bytes to transfer.
UInt8
IOSCSIProtocolServices::GetDataTransferDirection ( SCSITaskIdentifier request )
{
	
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	
	// Check to see what the current execution mode is  
	if ( scsiRequest->GetTaskExecutionMode ( ) == kSCSITaskMode_CommandExecution )
	{
		return scsiRequest->GetDataTransferDirection( );
	}
	
	else
	{
		return scsiRequest->GetAutosenseDataTransferDirection ( );
	}
	
}


UInt64
IOSCSIProtocolServices::GetRequestedDataTransferCount ( SCSITaskIdentifier request )
{
	
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	
	// Check to see what the current execution mode is  
	if ( scsiRequest->GetTaskExecutionMode ( ) == kSCSITaskMode_CommandExecution )
	{
		return scsiRequest->GetRequestedDataTransferCount ( );
	}
	
	else
	{
		return scsiRequest->GetAutosenseRequestedDataTransferCount ( );
	}
	
}


bool
IOSCSIProtocolServices::SetRealizedDataTransferCount ( 	SCSITaskIdentifier request,
														UInt64 newRealizedDataCount )
{
	
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );

	// Check to see what the current execution mode is  
	if ( scsiRequest->GetTaskExecutionMode ( ) == kSCSITaskMode_CommandExecution )
	{
		return scsiRequest->SetRealizedDataTransferCount ( newRealizedDataCount );
	}
	
	else
	{
		return scsiRequest->SetAutosenseRealizedDataCount ( newRealizedDataCount );
	}
	
}


UInt64
IOSCSIProtocolServices::GetRealizedDataTransferCount ( SCSITaskIdentifier request )
{
	
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	
	// Check to see what the current execution mode is  
	if ( scsiRequest->GetTaskExecutionMode ( ) == kSCSITaskMode_CommandExecution )
	{
		return scsiRequest->GetRealizedDataTransferCount ( );
	}
	
	else
	{
		return scsiRequest->GetAutosenseRealizedDataCount ( );
	}
	
}


IOMemoryDescriptor *
IOSCSIProtocolServices::GetDataBuffer ( SCSITaskIdentifier request )
{
	
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	if ( scsiRequest->GetTaskExecutionMode ( ) == kSCSITaskMode_CommandExecution )
	{
		return scsiRequest->GetDataBuffer ( );
	}
	
	else
	{
		return scsiRequest->GetAutosenseDataBuffer ( );
	}
	
}


UInt64
IOSCSIProtocolServices::GetDataBufferOffset ( SCSITaskIdentifier request )
{
	
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->GetDataBufferOffset ( );
	
}


UInt32
IOSCSIProtocolServices::GetTimeoutDuration ( SCSITaskIdentifier request )
{
	
	SCSITask *	scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->GetTimeoutDuration ( );

}


// Set the auto sense data that was returned for the SCSI Task.
// A return value if true indicates that the data copied to the member 
// sense data structure, false indicates that the data could not be saved.
bool
IOSCSIProtocolServices::SetAutoSenseData ( 	SCSITaskIdentifier request,
											SCSI_Sense_Data * senseData )
{
	
	SCSITask *		scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->SetAutoSenseData ( senseData );
	
}


bool
IOSCSIProtocolServices::SetProtocolLayerReference ( SCSITaskIdentifier request,
													void * newReferenceValue )
{
	
	SCSITask *		scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->SetProtocolLayerReference ( newReferenceValue );
	
}


void *
IOSCSIProtocolServices::GetProtocolLayerReference ( SCSITaskIdentifier request )
{
	
	SCSITask *		scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->GetProtocolLayerReference ( );
	
}


bool
IOSCSIProtocolServices::SetTaskExecutionMode ( SCSITaskIdentifier request, SCSITaskMode newTaskMode )
{
	
	SCSITask *		scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->SetTaskExecutionMode ( newTaskMode );
	
}


SCSITaskMode
IOSCSIProtocolServices::GetTaskExecutionMode ( SCSITaskIdentifier request )
{
	
	SCSITask *		scsiRequest;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	return scsiRequest->GetTaskExecutionMode ( );
	
}


#pragma mark - 
#pragma mark SCSI Task Queue Management
// Following are the commands used to manipulate the queue of pending SCSI Tasks.
// Currently the queuing is strictly first in, first out.  This needs to be changed
// to support the SCSI queueing model in the SCSI Architecture Model-2 specification.


// Add the SCSI Task to the queue.  The Task's Attribute determines where in
// the queue the Task is placed.
void
IOSCSIProtocolServices::AddSCSITaskToQueue ( SCSITaskIdentifier request )
{
	
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: AddSCSITaskToQueue called.\n", getName ( ) ) );
	
	IOSimpleLockLock ( fQueueLock );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	
	// Make sure that the new request does not have a following task.
	scsiRequest->EnqueueFollowingSCSITask ( NULL );
	
	// Check to see if there are any tasks currently queued.
	if ( fSCSITaskQueueHead == NULL )
	{
		
		// There are no other tasks currently queued, so
		// save this one as the head.
		fSCSITaskQueueHead = scsiRequest;
		
	}
	
	else
	{
		
		// There is at least one task currently in the queue,
		// Add the current one to the end.
		SCSITask *	currentElement;
		
		currentElement = fSCSITaskQueueHead;
		while ( currentElement->GetFollowingSCSITask ( ) != NULL )
		{
			currentElement = currentElement->GetFollowingSCSITask ( );
		}
		
		currentElement->EnqueueFollowingSCSITask ( scsiRequest );
		
	}
	
	IOSimpleLockUnlock ( fQueueLock );
	
}


// Add the SCSI Task to the head of the queue.  This is used when the task
// has been removed from the head of the queue, but the subclass indicates
// that it can not yet process this task.
void
IOSCSIProtocolServices::AddSCSITaskToHeadOfQueue ( SCSITask * request )
{
	
	IOSimpleLockLock ( fQueueLock );

	// Make sure that the new request does not have a following task.
	request->EnqueueFollowingSCSITask ( fSCSITaskQueueHead );
	fSCSITaskQueueHead = request;

	IOSimpleLockUnlock ( fQueueLock );
	
}


// Remove the next SCSI Task for the queue and return it.
SCSITask *
IOSCSIProtocolServices::RetrieveNextSCSITaskFromQueue ( void )
{
	
	SCSITask *		selectedTask;
	
	IOSimpleLockLock ( fQueueLock );
	
	// Check to see if there are any tasks currently queued.
	if ( fSCSITaskQueueHead == NULL )
	{
		
		// There are currently no tasks queued, return NULL.
		selectedTask = NULL;
		
	}
	
	else
	{
		
		// There is at least one task currently in the queue,
		
		// Grab the head task
		selectedTask = fSCSITaskQueueHead;
		
		// Set the head pointer to the next task in the queue.  If there
		// is no more tasks, this head pointer will be set to NULL.
		fSCSITaskQueueHead = selectedTask->GetFollowingSCSITask ( );
		
		// Make sure that the new request does not have a following task.
		selectedTask->EnqueueFollowingSCSITask ( NULL );
		
	}
	
	IOSimpleLockUnlock ( fQueueLock );
	
	return selectedTask;
	
}


// Check to see if the SCSI Task resides and abort it if it does.
// This currently does nothing since the ABORT TASK and ABORT TASK SET
// managment functions are not supported.
bool
IOSCSIProtocolServices::AbortSCSITaskFromQueue ( SCSITask *request )
{
	
	// If the indicated SCSI Task currently resides in the Queue, the SCSI Task
	// will be removed and no further processing shall occur on that Task.  This
	// method will then return true.
	
	// If the SCSI Task does not currently reside in the queue, this method will
	// return false.
	return false;
	
}


// 
void
IOSCSIProtocolServices::SendSCSITasksFromQueue ( void )
{
	
	bool			cmdAccepted = false;
	SCSITask *		nextVictim;
	
	do 
	{
		
		SCSIServiceResponse 	serviceResponse;
		SCSITaskStatus			taskStatus;
		
		// get the next command from the request queue
		nextVictim = RetrieveNextSCSITaskFromQueue ( );
		if ( nextVictim != NULL )
		{
			
			cmdAccepted = SendSCSICommand ( nextVictim, &serviceResponse, &taskStatus );
			if ( cmdAccepted == false )
			{
				
				// The subclass can not process the command at this time,
				// add it to the queue and try again at CommandComplete time.
				AddSCSITaskToHeadOfQueue ( nextVictim );
				
			}
			
			else if ( serviceResponse != kSCSIServiceResponse_Request_In_Process )
			{
				
				// The command was sent and completed, send next Task based on its Attribute.
				nextVictim->SetServiceResponse ( serviceResponse );
				nextVictim->SetTaskStatus ( taskStatus );
				nextVictim->SetTaskState ( kSCSITaskState_ENDED );
				
			}
			
		}
		
	} while ( ( cmdAccepted == true ) && ( nextVictim != NULL ) );

}


// 
void
IOSCSIProtocolServices::RejectSCSITasksCurrentlyQueued ( void )
{
	
	SCSITask *		nextVictim;
	
	STATUS_LOG ( ( "%s: RejectSCSITasksCurrentlyQueued called.\n", getName ( ) ) );
	
	do 
	{
		
		// get the next command from the request queue
		nextVictim = RetrieveNextSCSITaskFromQueue ( );
		if ( nextVictim != NULL )
		{
			RejectTask ( nextVictim );
		}
		
	} while ( nextVictim != NULL );
	
}


void
IOSCSIProtocolServices::ProcessCompletedTask ( 	SCSITaskIdentifier 	request, 
												SCSIServiceResponse serviceResponse,
												SCSITaskStatus		taskStatus )
{
	
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: ProcessCompletedTask called.\n", getName ( ) ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	
	if ( scsiRequest->GetTaskExecutionMode ( ) == kSCSITaskMode_CommandExecution )
	{
		
		// The task is currently in Command Execution mode, update the service
		// response and  
		scsiRequest->SetServiceResponse ( serviceResponse );
		
		// Check to see if this task was successfully transported to the
		// device server.
		if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
		{
			
			// Save that status into the Task object.
			scsiRequest->SetTaskStatus ( taskStatus );
			
			// See if the conditions for autosenses has been met: the task completed
			// with a CHECK_CONDITION.
			if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
				 ( taskStatus == kSCSITaskStatus_CHECK_CONDITION ) )
			{
				
				// Check if the client wants Autosense data
				if ( scsiRequest->IsAutosenseRequested ( ) == true )
				{
					
					// Check if the Protocol has already provided this data
					if ( scsiRequest->GetAutoSenseData ( NULL ) == false )
					{
						
						// Put the task into Autosense mode and
						// add to the head of the queue.
						scsiRequest->SetTaskExecutionMode ( kSCSITaskMode_Autosense );
						AddSCSITaskToHeadOfQueue ( scsiRequest );
						
						// The task can not be completed until the 
						// autosense is done, so exit and wait for that
						// completion.
						return;
						
					}
					
				}
				
			}
			
		}
		
	}
	
	else
	{
		
		// the task is in Autosense mode, check to see if the autosense
		// command completed successfully.
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( taskStatus == kSCSITaskStatus_GOOD ) )
		{
			
			scsiRequest->SetAutosenseIsValid ( true );
			
		}
		
	}
	
	scsiRequest->SetTaskState ( kSCSITaskState_ENDED );
	
	// The command is complete, release the retain for this command.
	release ( );	
	
	// The task has completed, execute the callback.
	scsiRequest->TaskCompletedNotification ( );
	
}


void
IOSCSIProtocolServices::RejectTask ( SCSITaskIdentifier 	request )
{
	
	SCSITask *	scsiRequest;
	
	STATUS_LOG ( ( "%s: RejectTask called.\n", getName ( ) ) );
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	
	scsiRequest->SetTaskState ( kSCSITaskState_ENDED );
	scsiRequest->SetServiceResponse ( kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE );
	
	// Save that status into the Task object.
	scsiRequest->SetTaskStatus ( kSCSITaskStatus_No_Status );
	
	// The command is complete, release the retain for this command.
	release ( );
	scsiRequest->TaskCompletedNotification ( );
	
}


#pragma mark - 
#pragma mark Provided Services to the SCSI Protocol Layer Subclasses


void
IOSCSIProtocolServices::CommandCompleted ( 	SCSITaskIdentifier 	request, 
											SCSIServiceResponse serviceResponse,
											SCSITaskStatus		taskStatus )
{
	
	STATUS_LOG ( ( "%s: CommandCompleted called.\n", getName ( ) ) );
	
	// Check to see if service requests are allowed
	if ( fAllowServiceRequests == false )
	{
		
		// Service requests are not allowed, return the task back
		// with an error.
		RejectTask ( request );
		return;
		
	}
	
	ProcessCompletedTask ( request, serviceResponse, taskStatus );
	
	SendSCSITasksFromQueue ( );
	
}


#pragma mark - 
#pragma mark Provided Services to the SCSI Application Layer 


// ------------ The Export functions to the SCSI Application Layer --------------
// The ExecuteCommand function will take a SCSI Task and transport
// it across the physical wire(s) to the device
void   
IOSCSIProtocolServices::ExecuteCommand ( SCSITaskIdentifier request )
{
	
	STATUS_LOG ( ( "%s::%s called.\n", getName ( ), __FUNCTION__ ) );
	
	// Make sure that the protocol driver does not go away 
	// if there are outstanding commands.
	retain ( );
	
	// Check to see if service requests are allowed
	if ( fAllowServiceRequests == false )
	{
		
		// Service requests are not allowed, return the task back
		// immediately with an error
		RejectTask ( request );
		return;
		
	}
	
	// Set the task state to ENABLED
	SetTaskState ( request, kSCSITaskState_ENABLED );
	
	// Set the execution mode to indicate standard command execution.
	SetTaskExecutionMode ( request, kSCSITaskMode_CommandExecution );
	
	// Add the new request to the queue
	AddSCSITaskToQueue ( request );
	
	SendSCSITasksFromQueue ( );
	
}


// The AbortCommand function will abort the indicated SCSI Task,
// if it is possible and the Task has not already completed.
SCSIServiceResponse
IOSCSIProtocolServices::AbortCommand ( SCSITaskIdentifier request )
{
	return kSCSIServiceResponse_FUNCTION_REJECTED;
