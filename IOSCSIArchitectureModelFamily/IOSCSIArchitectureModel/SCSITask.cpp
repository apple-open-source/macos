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

#include "SCSITask.h"

#define super IOCommand
OSDefineMetaClassAndStructors ( SCSITask, IOCommand );


bool
SCSITask::init ( void )
{
	
	if ( super::init ( ) == false )
	{
		return false;
	}
 
 	// Clear the owner here since it should be set when the object
 	// is instantiated and never reset.
 	fOwner					= NULL;

	fAutosenseDescriptor 	= NULL;

 	// Set this task to the default task state.  
	fTaskState = kSCSITaskState_NEW_TASK;

	// Reset all the task's fields to their defaults.
	return ResetForNewTask ( );
	
}


void
SCSITask::free ( void )
{
	
	if ( fOwner != NULL )
	{
		fOwner->release ( );
	}
	
	if ( fAutosenseDescriptor != NULL )
	{
		
		fAutosenseDescriptor->release ( );
		fAutosenseDescriptor = NULL;
		
	}
	
	super::free ( );
	
}


// Utility method to reset the object so that it may be used for a new
// Task.  This method will return true if the reset was successful
// and false if it failed because it represents an active task.
bool	
SCSITask::ResetForNewTask ( void )
{
	
	// If this is a pending task, do not allow it to be reset until
	// it has completed.
	if ( IsTaskActive ( ) == true )
	{
		return false;
	}
	
	fTaskAttribute 					= kSCSITask_SIMPLE;
   	fTaskState 						= kSCSITaskState_NEW_TASK;
	fTaskStatus						= kSCSITaskStatus_GOOD;
	fLogicalUnitNumber				= 0;	
	
	bzero ( &fCommandDescriptorBlock, kSCSICDBSize_Maximum );
	
	fCommandSize 					= 0;
	fTransferDirection 				= 0;
	fDataBuffer						= NULL;
  	fDataBufferOffset 				= 0;
	fRequestedByteCountOfTransfer	= 0;
   	fRealizedByteCountOfTransfer	= 0;

	fTimeoutDuration				= 0;
	fCompletionCallback				= NULL;
	fServiceResponse				= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	
	fNextTaskInQueue				= NULL;
	
	fProtocolLayerReference			= NULL;
	fApplicationLayerReference		= NULL;
	
	// Autosense member variables
   	fAutosenseDataRequested			= false;
	fAutosenseCDBSize				= 0;
	fAutoSenseDataIsValid			= false;
	
	bzero ( &fAutosenseCDB, kSCSICDBSize_Maximum );
	bzero ( &fAutoSenseData, sizeof ( SCSI_Sense_Data ) );
	
	if ( fAutosenseDescriptor == NULL )
	{
		
		fAutosenseDescriptor = IOMemoryDescriptor::withAddress ( 
						( void * ) &fAutoSenseData, sizeof ( SCSI_Sense_Data ), kIODirectionIn );
		
	}
 		
	fAutoSenseRealizedByteCountOfTransfer = 0;
	
	return true;
	
}


// Utility methods for setting and retreiving the Object that owns the
// instantiation of the SCSI Task
bool
SCSITask::SetTaskOwner ( OSObject * taskOwner )
{
	
	if ( fOwner != NULL )
	{
		// If this already has an owner, release
		// the retain on that one.
		fOwner->release ( );
	}
	
	fOwner = taskOwner;
	fOwner->retain ( );
	return true;
	
}


OSObject *
SCSITask::GetTaskOwner ( void )
{
	return fOwner;
}


// Utility method to check if this task represents an active.
bool	
SCSITask::IsTaskActive ( void )
{
	
	// If the state of this task is either new or it is an ended task,
	// return false since this does not qualify as active.
	
	if ( ( fTaskState == kSCSITaskState_NEW_TASK ) ||
		 ( fTaskState == kSCSITaskState_ENDED ) )
	{
		return false;
	}

	// If the task is in any other state, it is considered active.	
	return true;
	
}

// Utility Methods for managing the Logical Unit Number for which this Task 
// is intended.
bool
SCSITask::SetLogicalUnitNumber( UInt8 newLUN )
{
	fLogicalUnitNumber = newLUN;
	return true;
}

UInt8
SCSITask::GetLogicalUnitNumber( void )
{
	return fLogicalUnitNumber;
}

bool	
SCSITask::SetTaskAttribute ( SCSITaskAttribute newAttributeValue )
{
	fTaskAttribute = newAttributeValue;
	return true;
}


SCSITaskAttribute	
SCSITask::GetTaskAttribute ( void )
{
	return fTaskAttribute;
}


bool	
SCSITask::SetTaskState ( SCSITaskState newTaskState )
{
	fTaskState = newTaskState;
	return true;
}


SCSITaskState	
SCSITask::GetTaskState ( void )
{
	return fTaskState;
}


bool	
SCSITask::SetTaskStatus ( SCSITaskStatus newTaskStatus )
{
	fTaskStatus = newTaskStatus;
	return true;
}


SCSITaskStatus	
SCSITask::GetTaskStatus ( void )
{
	return fTaskStatus;
}


// Populate the 6 Byte Command Descriptor Block
bool 
SCSITask::SetCommandDescriptorBlock ( 
							UInt8			cdbByte0,
							UInt8			cdbByte1,
							UInt8			cdbByte2,
							UInt8			cdbByte3,
							UInt8			cdbByte4,
							UInt8			cdbByte5 )
{
	
	fCommandDescriptorBlock[0] = cdbByte0;
	fCommandDescriptorBlock[1] = cdbByte1;
	fCommandDescriptorBlock[2] = cdbByte2;
	fCommandDescriptorBlock[3] = cdbByte3;
	fCommandDescriptorBlock[4] = cdbByte4;
	fCommandDescriptorBlock[5] = cdbByte5;
	fCommandDescriptorBlock[6] = 0x00;
	fCommandDescriptorBlock[7] = 0x00;
	fCommandDescriptorBlock[8] = 0x00;
	fCommandDescriptorBlock[9] = 0x00;
	fCommandDescriptorBlock[10] = 0x00;
	fCommandDescriptorBlock[11] = 0x00;
	fCommandDescriptorBlock[12] = 0x00;
	fCommandDescriptorBlock[13] = 0x00;
	fCommandDescriptorBlock[14] = 0x00;
	fCommandDescriptorBlock[15] = 0x00;

	fCommandSize = kSCSICDBSize_6Byte;
	return true;
	
}


// Populate the 10 Byte Command Descriptor Block
bool 
SCSITask::SetCommandDescriptorBlock ( 
							UInt8			cdbByte0,
							UInt8			cdbByte1,
							UInt8			cdbByte2,
							UInt8			cdbByte3,
							UInt8			cdbByte4,
							UInt8			cdbByte5,
							UInt8			cdbByte6,
							UInt8			cdbByte7,
							UInt8			cdbByte8,
							UInt8			cdbByte9 )
{
	
	fCommandDescriptorBlock[0] = cdbByte0;
	fCommandDescriptorBlock[1] = cdbByte1;
	fCommandDescriptorBlock[2] = cdbByte2;
	fCommandDescriptorBlock[3] = cdbByte3;
	fCommandDescriptorBlock[4] = cdbByte4;
	fCommandDescriptorBlock[5] = cdbByte5;
	fCommandDescriptorBlock[6] = cdbByte6;
	fCommandDescriptorBlock[7] = cdbByte7;
	fCommandDescriptorBlock[8] = cdbByte8;
	fCommandDescriptorBlock[9] = cdbByte9;
	fCommandDescriptorBlock[10] = 0x00;
	fCommandDescriptorBlock[11] = 0x00;
	fCommandDescriptorBlock[12] = 0x00;
	fCommandDescriptorBlock[13] = 0x00;
	fCommandDescriptorBlock[14] = 0x00;
	fCommandDescriptorBlock[15] = 0x00;

	fCommandSize = kSCSICDBSize_10Byte;
	return true;
	
}


// Populate the 12 Byte Command Descriptor Block
bool 
SCSITask::SetCommandDescriptorBlock ( 
							UInt8			cdbByte0,
							UInt8			cdbByte1,
							UInt8			cdbByte2,
							UInt8			cdbByte3,
							UInt8			cdbByte4,
							UInt8			cdbByte5,
							UInt8			cdbByte6,
							UInt8			cdbByte7,
							UInt8			cdbByte8,
							UInt8			cdbByte9,
							UInt8			cdbByte10,
							UInt8			cdbByte11 )
{
	
	fCommandDescriptorBlock[0] = cdbByte0;
	fCommandDescriptorBlock[1] = cdbByte1;
	fCommandDescriptorBlock[2] = cdbByte2;
	fCommandDescriptorBlock[3] = cdbByte3;
	fCommandDescriptorBlock[4] = cdbByte4;
	fCommandDescriptorBlock[5] = cdbByte5;
	fCommandDescriptorBlock[6] = cdbByte6;
	fCommandDescriptorBlock[7] = cdbByte7;
	fCommandDescriptorBlock[8] = cdbByte8;
	fCommandDescriptorBlock[9] = cdbByte9;
	fCommandDescriptorBlock[10] = cdbByte10;
	fCommandDescriptorBlock[11] = cdbByte11;
	fCommandDescriptorBlock[12] = 0x00;
	fCommandDescriptorBlock[13] = 0x00;
	fCommandDescriptorBlock[14] = 0x00;
	fCommandDescriptorBlock[15] = 0x00;

	fCommandSize = kSCSICDBSize_12Byte;
	return true;
	
}


// Populate the 16 Byte Command Descriptor Block
bool 
SCSITask::SetCommandDescriptorBlock ( 
							UInt8			cdbByte0,
							UInt8			cdbByte1,
							UInt8			cdbByte2,
							UInt8			cdbByte3,
							UInt8			cdbByte4,
							UInt8			cdbByte5,
							UInt8			cdbByte6,
							UInt8			cdbByte7,
							UInt8			cdbByte8,
							UInt8			cdbByte9,
							UInt8			cdbByte10,
							UInt8			cdbByte11,
							UInt8			cdbByte12,
							UInt8			cdbByte13,
							UInt8			cdbByte14,
							UInt8			cdbByte15 )
{
	
	fCommandDescriptorBlock[0] = cdbByte0;
	fCommandDescriptorBlock[1] = cdbByte1;
	fCommandDescriptorBlock[2] = cdbByte2;
	fCommandDescriptorBlock[3] = cdbByte3;
	fCommandDescriptorBlock[4] = cdbByte4;
	fCommandDescriptorBlock[5] = cdbByte5;
	fCommandDescriptorBlock[6] = cdbByte6;
	fCommandDescriptorBlock[7] = cdbByte7;
	fCommandDescriptorBlock[8] = cdbByte8;
	fCommandDescriptorBlock[9] = cdbByte9;
	fCommandDescriptorBlock[10] = cdbByte10;
	fCommandDescriptorBlock[11] = cdbByte11;
	fCommandDescriptorBlock[12] = cdbByte12;
	fCommandDescriptorBlock[13] = cdbByte13;
	fCommandDescriptorBlock[14] = cdbByte14;
	fCommandDescriptorBlock[15] = cdbByte15;

	fCommandSize = kSCSICDBSize_16Byte;
	
	return true;
	
}


UInt8	
SCSITask::GetCommandDescriptorBlockSize ( void )
{
	return fCommandSize;
}


bool	
SCSITask::GetCommandDescriptorBlock ( SCSICommandDescriptorBlock * cdbData )
{
	bcopy ( fCommandDescriptorBlock, cdbData, sizeof ( SCSICommandDescriptorBlock ) );
	return true;
}


bool	
SCSITask::SetDataTransferDirection ( UInt8 newDataTransferDirection )
{
	fTransferDirection = newDataTransferDirection;
	return true;
}


UInt8	
SCSITask::GetDataTransferDirection ( void )
{
	return fTransferDirection;
}


bool	
SCSITask::SetRequestedDataTransferCount ( UInt64 requestedTransferCountInBytes )
{
	fRequestedByteCountOfTransfer = requestedTransferCountInBytes;
	return true;
}


UInt64	
SCSITask::GetRequestedDataTransferCount ( void )
{
	return fRequestedByteCountOfTransfer;
}


bool	
SCSITask::SetRealizedDataTransferCount ( UInt64 realizedTransferCountInBytes )
{
	fRealizedByteCountOfTransfer = realizedTransferCountInBytes;
	return true;
}


UInt64	
SCSITask::GetRealizedDataTransferCount ( void )
{
	return fRealizedByteCountOfTransfer;
}


bool	
SCSITask::SetDataBuffer ( IOMemoryDescriptor * newDataBuffer )
{
	fDataBuffer = newDataBuffer;
	return true;
}


IOMemoryDescriptor *
SCSITask::GetDataBuffer ( void )
{
	return fDataBuffer;
}


bool	
SCSITask::SetDataBufferOffset ( UInt64 newDataBufferOffset )
{
	fDataBufferOffset = newDataBufferOffset;
	return true;
}


UInt64	
SCSITask::GetDataBufferOffset ( void )
{
	return fDataBufferOffset;
}


bool	
SCSITask::SetTimeoutDuration ( UInt32 timeoutValue )
{
	fTimeoutDuration = timeoutValue;
 	return true;
}


UInt32	
SCSITask::GetTimeoutDuration ( void )
{
	return fTimeoutDuration;
}


bool	
SCSITask::SetTaskCompletionCallback ( SCSITaskCompletion newCallback )
{
	fCompletionCallback = newCallback;
	return true;
}


void	
SCSITask::TaskCompletedNotification ( void )
{
	fCompletionCallback( this );
}


bool	
SCSITask::SetServiceResponse ( SCSIServiceResponse serviceResponse )
{
	fServiceResponse = serviceResponse;
	
	return true;
}


SCSIServiceResponse 
SCSITask::GetServiceResponse ( void )
{
	return fServiceResponse;
}


// Set the auto sense data that was returned for the SCSI Task.
// A return value if true indicates that the data copied to the member 
// sense data structure, false indicates that the data could not be saved.
bool	
SCSITask::SetAutoSenseData ( SCSI_Sense_Data * senseData )
{
	
	bcopy ( senseData, &fAutoSenseData, kSenseDefaultSize );
	fAutoSenseDataIsValid = true;
	
	return true;
	
}


// Get the auto sense data that was returned for the SCSI Task.  A return value
// of true indicates that valid auto sense data has been returned in the receivingBuffer.
// A return value of false indicates that there is no auto sense data for this SCSI Task,
// and the receivingBuffer does not have valid data.
// If the receivingBuffer is NULL, this routine will return whether the autosense data is valid
// without tryiing to copy it to the receivingBuffer.
bool	
SCSITask::GetAutoSenseData ( SCSI_Sense_Data * receivingBuffer )
{
	
	if ( fAutoSenseDataIsValid == false )
	{
		return false;
	}
	
	if ( receivingBuffer != NULL )
	{
		bcopy ( &fAutoSenseData, receivingBuffer, kSenseDefaultSize );
	}
	
	return true;
	
}


bool	
SCSITask::SetProtocolLayerReference ( void * newReferenceValue )
{
	fProtocolLayerReference = newReferenceValue;
	return true;
}


void *
SCSITask::GetProtocolLayerReference ( void )
{
	return fProtocolLayerReference;
}


bool	
SCSITask::SetApplicationLayerReference ( void * newReferenceValue )
{
	fApplicationLayerReference = newReferenceValue;
	return true;
}


void *
SCSITask::GetApplicationLayerReference ( void )
{
	return fApplicationLayerReference;
}


#pragma mark -
#pragma mark SCSI Protocol Layer Mode methods
// These methods are only for the SCSI Protocol Layer to set the command execution
// mode of the command.  There currently are two modes, standard command execution
// for executing the command for which the task was created, and the autosense command
// execution mode for executing the Request Sense command for retrieving sense data.
bool
SCSITask::SetTaskExecutionMode ( SCSITaskMode newTaskMode )
{
	fTaskExecutionMode = newTaskMode;
	return true;
}


SCSITaskMode
SCSITask::GetTaskExecutionMode ( void )
{
	return fTaskExecutionMode;
}


bool
SCSITask::IsAutosenseRequested ( void )
{
	return fAutosenseDataRequested;
}


bool				
SCSITask::SetAutosenseIsValid ( bool newAutosenseState )
{
	fAutoSenseDataIsValid = newAutosenseState;
	return true;
}


UInt8				
SCSITask::GetAutosenseCommandDescriptorBlockSize ( void )
{
	return fAutosenseCDBSize;
}


bool				
SCSITask::GetAutosenseCommandDescriptorBlock ( SCSICommandDescriptorBlock * cdbData )
{
	bcopy ( &fAutosenseCDB, cdbData, sizeof ( SCSICommandDescriptorBlock ) );
	return true;
}


UInt8
SCSITask::GetAutosenseDataTransferDirection ( void )
{
	return kSCSIDataTransfer_FromTargetToInitiator;
}


UInt64
SCSITask::GetAutosenseRequestedDataTransferCount( void )
{
	return sizeof ( SCSI_Sense_Data );
}


bool
SCSITask::SetAutosenseRealizedDataCount ( UInt64 realizedTransferCountInBytes )
{
	fAutoSenseRealizedByteCountOfTransfer = realizedTransferCountInBytes;
	return true;
}


UInt64
SCSITask::GetAutosenseRealizedDataCount ( void )
{
	return fAutoSenseRealizedByteCountOfTransfer;
}


IOMemoryDescriptor	*
SCSITask::GetAutosenseDataBuffer ( void )
{
	return fAutosenseDescriptor;
}


bool
SCSITask::SetAutosenseCommand (
					UInt8			cdbByte0,
					UInt8			cdbByte1,
					UInt8			cdbByte2,
					UInt8			cdbByte3,
					UInt8			cdbByte4,
					UInt8			cdbByte5 )
{
	
	fAutosenseCDB[0] = cdbByte0;
	fAutosenseCDB[1] = cdbByte1;
	fAutosenseCDB[2] = cdbByte2;
	fAutosenseCDB[3] = cdbByte3;
	fAutosenseCDB[4] = cdbByte4;
	fAutosenseCDB[5] = cdbByte5;
	fAutosenseCDB[6] = 0x00;
	fAutosenseCDB[7] = 0x00;
	fAutosenseCDB[8] = 0x00;
	fAutosenseCDB[9] = 0x00;
	fAutosenseCDB[10] = 0x00;
	fAutosenseCDB[11] = 0x00;
	fAutosenseCDB[12] = 0x00;
	fAutosenseCDB[13] = 0x00;
	fAutosenseCDB[14] = 0x00;
	fAutosenseCDB[15] = 0x00;

	fAutosenseCDBSize = kSCSICDBSize_6Byte;
	fAutosenseDataRequested = true;
	
	return true;
	
}


#pragma mark -
#pragma mark SCSI Task Queue Management Methods
// These are the methods used for adding and removing the SCSI Task object
// to a queue.  These are mainly for use by the SCSI Protocol Layer, but can be
// used by the SCSI Application Layer if the task is currently not active (the
// Task state is kSCSITaskState_NEW_TASK or kSCSITaskState_ENDED).


// This method queues the specified Task after this one
void	
SCSITask::EnqueueFollowingSCSITask ( SCSITask * followingTask )
{
	fNextTaskInQueue = followingTask;
}


// Returns the pointer to the SCSI Task that is queued after
// this one.  Returns NULL if one is not currently queued.
SCSITask *
SCSITask::GetFollowingSCSITask ( void )
{
	return fNextTaskInQueue;
}


// Returns the pointer to the SCSI Task that is queued after
// this one and removes it from the queue.  Returns NULL if 
// one is not currently queued.
SCSITask *
SCSITask::DequeueFollowingSCSITask ( void )
{
	
	SCSITask *	returnTask;
	
	returnTask 			= fNextTaskInQueue;
	fNextTaskInQueue 	= NULL;
	
	return returnTask;
	
}


// Returns the pointer to the SCSI Task that is queued after
// this one and removes it from the queue.  Returns NULL if 
// one is not currently queued.  After dequeueing the following
// Task, the specified newFollowingTask will be enqueued after this
// task.
SCSITask *
SCSITask::ReplaceFollowingSCSITask ( SCSITask * newFollowingTask )
{
	
	SCSITask *	returnTask;
	
	returnTask 			= fNextTaskInQueue;
	fNextTaskInQueue 	= newFollowingTask;
	
	return returnTask;
	
}

// Space reserved for future expansion.
OSMetaClassDefineReservedUnused( SCSITask, 1 );
OSMetaClassDefineReservedUnused( SCSITask, 2 );
OSMetaClassDefineReservedUnused( SCSITask, 3 );
OSMetaClassDefineReservedUnused( SCSITask, 4 );
OSMetaClassDefineReservedUnused( SCSITask, 5 );
OSMetaClassDefineReservedUnused( SCSITask, 6 );
OSMetaClassDefineReservedUnused( SCSITask, 7 );
OSMetaClassDefineReservedUnused( SCSITask, 8 );
