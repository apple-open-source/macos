/*
 * Copyright (c) 2002-2007 Apple Inc. All rights reserved.
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

// SCSI Parallel Family includes
#include "SCSIParallelTask.h"

// General IOKit includes
#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IODMACommand.h>


//-----------------------------------------------------------------------------
//	Macros
//-----------------------------------------------------------------------------

#define DEBUG 												0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING					"SCSIParallelTask"

#if DEBUG
#define SCSI_PARALLEL_TASK_DEBUGGING_LEVEL					0
#endif


// This module needs SPI_MODULE defined in order to pick up the
// static debugging function.
#define SPI_MODULE	1

#include "IOSCSIParallelFamilyDebugging.h"


#if ( SCSI_PARALLEL_TASK_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		panic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_PARALLEL_TASK_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		kprintf x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_PARALLEL_TASK_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		kprintf x
#else
#define STATUS_LOG(x)
#endif


#define super IODMACommand
OSDefineMetaClassAndStructors ( SCSIParallelTask, IODMACommand );


#if 0
#pragma mark -
#pragma mark Public Methods
#pragma mark -
#endif


//-----------------------------------------------------------------------------
//	Create - Creates a SCSIParallelTask						   [STATIC][PUBLIC]
//-----------------------------------------------------------------------------

SCSIParallelTask *
SCSIParallelTask::Create ( UInt32 sizeOfHBAData, UInt64 alignmentMask )
{
	
	SCSIParallelTask *	newTask = NULL;
	bool				result	= false;
	
	newTask = OSTypeAlloc ( SCSIParallelTask );
	require_nonzero ( newTask, ErrorExit );
	
	result = newTask->InitWithSize ( sizeOfHBAData, alignmentMask );
	require ( result, ReleaseTask );
	
	return newTask;
	
	
ReleaseTask:
	
	
	newTask->release ( );
	newTask = NULL;
	
	
ErrorExit:
	
	
	return NULL;
	
}


//-----------------------------------------------------------------------------
//	InitWithSize - Initializes the object with requested HBA data size.
//																	   [PUBLIC]
//-----------------------------------------------------------------------------

bool
SCSIParallelTask::InitWithSize (
	UInt32		sizeOfHBAData,
	UInt64 		alignmentMask )
{
	
	IOBufferMemoryDescriptor *	buffer			= NULL;
	IOReturn					status			= kIOReturnSuccess;
	
	fCommandChain.next = NULL;
	fCommandChain.prev = NULL;
	
	fResendTaskChain.next = NULL;
	fResendTaskChain.prev = NULL;
	
	fTimeoutChain.next 	= NULL;
	fTimeoutChain.prev	= NULL;
	
	fHBADataSize = sizeOfHBAData;
	
	buffer = IOBufferMemoryDescriptor::inTaskWithPhysicalMask (
		kernel_task,
		kIODirectionOutIn | kIOMemoryPhysicallyContiguous,
		fHBADataSize,
		alignmentMask );
	require_nonzero ( buffer, ErrorExit );
	
	status = buffer->prepare ( kIODirectionOutIn );
	require_success ( status, FreeHBAData );
	
	fHBAData = buffer->getBytesNoCopy ( );
	require_nonzero ( fHBAData, CompleteHBAData );
	
	bzero ( fHBAData, fHBADataSize );
	
	fHBADataDescriptor = buffer;
	
	return true;
	

CompleteHBAData:
	
	
	buffer->complete ( );
	
	
FreeHBAData:
	
	
	buffer->release ( );
	buffer = NULL;
	
	
ErrorExit:
	
	
	return false;
	
}


//-----------------------------------------------------------------------------
//	free - Frees any resources allocated.							   [PUBLIC]
//-----------------------------------------------------------------------------

void
SCSIParallelTask::free ( void )
{
	
	if ( fHBADataDescriptor != NULL )
	{
		
		fHBADataDescriptor->complete ( );
		fHBADataDescriptor->release ( );
		fHBADataDescriptor = NULL;
		
	}
	
	super::free ( );
	
}


//-----------------------------------------------------------------------------
//	ResetForNewTask - Resets the task for execution.				   [PUBLIC]
//-----------------------------------------------------------------------------

void
SCSIParallelTask::ResetForNewTask ( void )
{
	
	fTargetID					= 0;
	fDevice						= NULL;
	fSCSITask					= NULL;
	fRealizedTransferCount		= 0;
	fControllerTaskIdentifier	= 0;
	fTaskRetryCount				= 0;
	
	fSCSIParallelFeatureRequestCount		= 0;
	fSCSIParallelFeatureRequestResultCount	= 0;
	
	// Set the feature arrays to their default values
	for ( int loop = 0; loop < kSCSIParallelFeature_TotalFeatureCount; loop++ )
	{
		
		fSCSIParallelFeatureRequest[loop]	= kSCSIParallelFeature_NoNegotiation;
		fSCSIParallelFeatureResult[loop]	= kSCSIParallelFeature_NegotitiationUnchanged;
		
	}
	
}


//-----------------------------------------------------------------------------
//	SetSCSITaskIdentifier - Sets SCSITaskIdentifier for this task.	   [PUBLIC]
//-----------------------------------------------------------------------------

bool
SCSIParallelTask::SetSCSITaskIdentifier ( SCSITaskIdentifier scsiRequest )
{
	
	fSCSITask = scsiRequest;
	return true;
	
}


//-----------------------------------------------------------------------------
//	GetSCSITaskIdentifier - Gets SCSITaskIdentifier for this task.	   [PUBLIC]
//-----------------------------------------------------------------------------

SCSITaskIdentifier
SCSIParallelTask::GetSCSITaskIdentifier ( void )
{
	return fSCSITask;
}


//-----------------------------------------------------------------------------
//	SetTargetIdentifier - Sets SCSITargetIdentifier for this task.	   [PUBLIC]
//-----------------------------------------------------------------------------

bool
SCSIParallelTask::SetTargetIdentifier ( SCSITargetIdentifier theTargetID )
{
	fTargetID = theTargetID;
	return true;
}


//-----------------------------------------------------------------------------
//	GetTargetIdentifier - Gets SCSITargetIdentifier for this task.	   [PUBLIC]
//-----------------------------------------------------------------------------

SCSITargetIdentifier
SCSIParallelTask::GetTargetIdentifier ( void )
{
	return fTargetID;
}


//-----------------------------------------------------------------------------
//	SetDevice - Sets device for this task.							   [PUBLIC]
//-----------------------------------------------------------------------------

bool
SCSIParallelTask::SetDevice ( IOSCSIParallelInterfaceDevice * device )
{
	fDevice = device;
	return true;
}


//-----------------------------------------------------------------------------
//	GetDevice - Gets device for this task.							   [PUBLIC]
//-----------------------------------------------------------------------------

IOSCSIParallelInterfaceDevice *
SCSIParallelTask::GetDevice ( void )
{
	return fDevice;
}


//-----------------------------------------------------------------------------
//	GetLogicalUnitNumber - Gets SCSILogicalUnitNumber for this task.   [PUBLIC]
//-----------------------------------------------------------------------------

SCSILogicalUnitNumber
SCSIParallelTask::GetLogicalUnitNumber ( void )
{
	return ( ( SCSITask * ) fSCSITask )->GetLogicalUnitNumber ( );
}

//-----------------------------------------------------------------------------
//	GetLogicalUnitBytes - Gets SCSILogicalUnitBytes for this task.   [PUBLIC]
//-----------------------------------------------------------------------------

void
SCSIParallelTask::GetLogicalUnitBytes ( SCSILogicalUnitBytes * logicalUnitBytes )
{
	return ( ( SCSITask * ) fSCSITask )->GetLogicalUnitBytes ( logicalUnitBytes );
}

//-----------------------------------------------------------------------------
//	GetTaskAttribute - Gets SCSITaskAttribute for this task. 		   [PUBLIC]
//-----------------------------------------------------------------------------

SCSITaskAttribute
SCSIParallelTask::GetTaskAttribute ( void )
{
	return ( ( SCSITask * ) fSCSITask )->GetTaskAttribute ( );	
}


//-----------------------------------------------------------------------------
//	GetTaggedTaskIdentifier - Gets SCSITaggedTaskIdentifier for this task.
//															 		   [PUBLIC]
//-----------------------------------------------------------------------------

SCSITaggedTaskIdentifier
SCSIParallelTask::GetTaggedTaskIdentifier( void )
{
	return ( ( SCSITask * ) fSCSITask )->GetTaggedTaskIdentifier ( );
}


//-----------------------------------------------------------------------------
//	GetCommandDescriptorBlockSize - Gets cdb size for this task. 	   [PUBLIC]
//-----------------------------------------------------------------------------

UInt8
SCSIParallelTask::GetCommandDescriptorBlockSize ( void )
{
	return ( ( SCSITask * ) fSCSITask )->GetCommandDescriptorBlockSize ( );
}


//-----------------------------------------------------------------------------
//	GetCommandDescriptorBlock - Gets cdb for this task. 			   [PUBLIC]
//-----------------------------------------------------------------------------

bool
SCSIParallelTask::GetCommandDescriptorBlock ( 
					SCSICommandDescriptorBlock *	cdbData )
{
	return ( ( SCSITask * ) fSCSITask )->GetCommandDescriptorBlock ( cdbData );
}


//-----------------------------------------------------------------------------
//	GetDataTransferDirection - Gets transfer direction for this task.  [PUBLIC]
//-----------------------------------------------------------------------------

UInt8	
SCSIParallelTask::GetDataTransferDirection ( void )
{
	return ( ( SCSITask * ) fSCSITask )->GetDataTransferDirection ( );
}


//-----------------------------------------------------------------------------
//	GetRequestedDataTransferCount - Gets transfer count for this task. [PUBLIC]
//-----------------------------------------------------------------------------

UInt64
SCSIParallelTask::GetRequestedDataTransferCount ( void )
{
	return ( ( SCSITask * ) fSCSITask )->GetRequestedDataTransferCount ( );
}


//-----------------------------------------------------------------------------
//	IncrementRealizedDataTransferCount - Adds value to the realized transfer
//										 count for this task. 		   [PUBLIC]
//-----------------------------------------------------------------------------

void
SCSIParallelTask::IncrementRealizedDataTransferCount (
					UInt64 							realizedTransferCountInBytes )
{
	fRealizedTransferCount += realizedTransferCountInBytes;
}


//-----------------------------------------------------------------------------
//	SetRealizedDataTransferCount - 	Sets the realized transfer count for
//									this task. 		 				   [PUBLIC]
//-----------------------------------------------------------------------------

bool
SCSIParallelTask::SetRealizedDataTransferCount (
					UInt64 							realizedTransferCountInBytes )
{
	
	fRealizedTransferCount = realizedTransferCountInBytes;
	return true;
	
}


//-----------------------------------------------------------------------------
//	GetRealizedDataTransferCount - 	Gets the realized transfer count for
//									this task. 		 				   [PUBLIC]
//-----------------------------------------------------------------------------

UInt64
SCSIParallelTask::GetRealizedDataTransferCount ( void )
{
	return fRealizedTransferCount;
}


//-----------------------------------------------------------------------------
//	GetDataBuffer - Gets the data buffer associated with this task.    [PUBLIC]
//-----------------------------------------------------------------------------

IOMemoryDescriptor *
SCSIParallelTask::GetDataBuffer ( void )
{
	return ( ( SCSITask * ) fSCSITask )->GetDataBuffer ( );
}


//-----------------------------------------------------------------------------
//	GetDataBufferOffset - 	Gets the data buffer offset associated with this
//							task.									   [PUBLIC]
//-----------------------------------------------------------------------------

UInt64
SCSIParallelTask::GetDataBufferOffset ( void )
{
	return ( ( SCSITask * ) fSCSITask )->GetDataBufferOffset ( );
}


//-----------------------------------------------------------------------------
//	GetTimeoutDuration - 	Gets the timeout duration associated with this
//							task.									   [PUBLIC]
//-----------------------------------------------------------------------------

UInt32
SCSIParallelTask::GetTimeoutDuration ( void )
{
	return ( ( SCSITask * ) fSCSITask )->GetTimeoutDuration ( );
}


//-----------------------------------------------------------------------------
//	SetAutoSenseData - 	Sets the auto-sense data.					   [PUBLIC]
//-----------------------------------------------------------------------------

bool
SCSIParallelTask::SetAutoSenseData (
					SCSI_Sense_Data * 	senseData,
					UInt8				senseDataSize )
{
	return ( ( SCSITask * ) fSCSITask )->SetAutoSenseData ( senseData, senseDataSize );
}


//-----------------------------------------------------------------------------
//	GetAutoSenseData - 	Gets the auto-sense data.					   [PUBLIC]
//-----------------------------------------------------------------------------

bool	
SCSIParallelTask::GetAutoSenseData ( 
					SCSI_Sense_Data * 	receivingBuffer,
					UInt8				senseDataSize )
{
	return ( ( SCSITask * ) fSCSITask )->GetAutoSenseData ( receivingBuffer, senseDataSize );	
}


//-----------------------------------------------------------------------------
//	GetAutoSenseDataSize - 	Gets the auto-sense data size.			   [PUBLIC]
//-----------------------------------------------------------------------------

UInt8
SCSIParallelTask::GetAutoSenseDataSize ( void )
{
	return ( ( SCSITask * ) fSCSITask )->GetAutoSenseDataSize ( );	
}


//-----------------------------------------------------------------------------
//	SetSCSIParallelFeatureNegotiation - Sets a feature negotiation request.
//																	   [PUBLIC]
//-----------------------------------------------------------------------------

void		
SCSIParallelTask::SetSCSIParallelFeatureNegotiation ( 
					SCSIParallelFeature 			requestedFeature,
					SCSIParallelFeatureRequest 		newRequest )
{
	
	// Check to see if this is a known feature.  Since the feature definitions
	// are zero based if the request is greater or equal than the total number
	// of features it is undefined.
	if ( requestedFeature >= kSCSIParallelFeature_TotalFeatureCount )
	{
		
		ERROR_LOG ( ( "Unknown feature request: %ld\n", ( int ) requestedFeature ) );
		
		// The object does not know of this feature, so it will
		// ignore this request.
		return;
		
	}
	
	// If this request is either to negotiate or clear a negotiation,
	// increment the requested feature count.
	if ( newRequest != kSCSIParallelFeature_NoNegotiation )
	{
		fSCSIParallelFeatureRequestCount++;
	}
	
	fSCSIParallelFeatureRequest[requestedFeature] = newRequest;
	
}


//-----------------------------------------------------------------------------
//	GetSCSIParallelFeatureNegotiation - Gets a feature negotiation request.
//																	   [PUBLIC]
//-----------------------------------------------------------------------------

SCSIParallelFeatureRequest
SCSIParallelTask::GetSCSIParallelFeatureNegotiation ( 
					SCSIParallelFeature 			requestedFeature )
{
	
	// Check to see if this is a known feature.  Since the feature definitions
	// are zero based if the request is greater or equal than the total number
	// of features it is undefined.
	if ( requestedFeature >= kSCSIParallelFeature_TotalFeatureCount )
	{
		
		ERROR_LOG ( ( "Unknown feature request: %ld\n", ( int ) requestedFeature ) );
		
		// The object does not know of this feature, so it will
		// return that negotation is not requested.
		return kSCSIParallelFeature_NoNegotiation;
		
	}
	
	return fSCSIParallelFeatureRequest[requestedFeature];
	
}


//-----------------------------------------------------------------------------
//	GetSCSIParallelFeatureNegotiationCount - Gets feature negotiation request count.
//																	   [PUBLIC]
//-----------------------------------------------------------------------------

UInt64
SCSIParallelTask::GetSCSIParallelFeatureNegotiationCount ( void )
{
	return fSCSIParallelFeatureRequestCount;
}


//-----------------------------------------------------------------------------
//	SetSCSIParallelFeatureNegotiationResult - Sets feature negotiation result.
//																	   [PUBLIC]
//-----------------------------------------------------------------------------

void
SCSIParallelTask::SetSCSIParallelFeatureNegotiationResult ( 
					SCSIParallelFeature 			requestedFeature,
					SCSIParallelFeatureResult 		newResult )
{
	
	// Check to see if this is a known feature.  Since the feature definitions
	// are zero based if the request is greater or equal than the total number
	// of features it is undefined.
	if ( requestedFeature >= kSCSIParallelFeature_TotalFeatureCount )
	{
		
		ERROR_LOG ( ( "Unknown feature request: %ld\n", ( int ) requestedFeature ) );
		
		// The object does not know of this feature, so it will
		// ignore this request.
		return;
		
	}
	
	if ( newResult != kSCSIParallelFeature_NegotitiationUnchanged )
	{
		fSCSIParallelFeatureRequestResultCount++;
	}
	
	fSCSIParallelFeatureResult[requestedFeature] = newResult;
	
}


//-----------------------------------------------------------------------------
//	GetSCSIParallelFeatureNegotiationResult - Gets feature negotiation result.
//																	   [PUBLIC]
//-----------------------------------------------------------------------------

SCSIParallelFeatureResult
SCSIParallelTask::GetSCSIParallelFeatureNegotiationResult (
 					SCSIParallelFeature 			requestedFeature )
{
	
	// Check to see if this is a known feature.  Since the feature definitions
	// are zero based if the request is greater or equal than the total number
	// of features it is undefined.
	if ( requestedFeature >= kSCSIParallelFeature_TotalFeatureCount )
	{
		
		ERROR_LOG ( ( "Unknown feature request: %ld\n", ( int ) requestedFeature ) );
		
		// The object does not know of this feature, so it will
		// return that negotation is unchanged.
		return kSCSIParallelFeature_NegotitiationUnchanged;
		
	}
	
	return fSCSIParallelFeatureResult[requestedFeature];
	
}


//-----------------------------------------------------------------------------
//	GetSCSIParallelFeatureNegotiationResultCount -  Gets feature negotiation
//													result count.	   [PUBLIC]
//-----------------------------------------------------------------------------

UInt64	
SCSIParallelTask::GetSCSIParallelFeatureNegotiationResultCount ( void )
{
	return fSCSIParallelFeatureRequestResultCount;
}


//-----------------------------------------------------------------------------
//	SetControllerTaskIdentifier -  Sets controller unique identifier.  [PUBLIC]
//-----------------------------------------------------------------------------

void	
SCSIParallelTask::SetControllerTaskIdentifier ( UInt64 newIdentifier )
{
	fControllerTaskIdentifier = newIdentifier;
}


//-----------------------------------------------------------------------------
//	GetControllerTaskIdentifier -  Gets controller unique identifier.  [PUBLIC]
//-----------------------------------------------------------------------------

UInt64
SCSIParallelTask::GetControllerTaskIdentifier ( void )
{
	return fControllerTaskIdentifier;
}


//-----------------------------------------------------------------------------
//	GetHBADataSize -  Gets data size of HBA specific data.			   [PUBLIC]
//-----------------------------------------------------------------------------

UInt32
SCSIParallelTask::GetHBADataSize ( void )
{
	return fHBADataSize;
}


//-----------------------------------------------------------------------------
//	GetHBADataPointer -  Gets virtual address of HBA specific data.	   [PUBLIC]
//-----------------------------------------------------------------------------

void *
SCSIParallelTask::GetHBADataPointer ( void )
{
	return fHBAData;
}


//-----------------------------------------------------------------------------
//	GetHBADataDescriptor -  Gets IOMemoryDescriptor of HBA specific data.
//																	   [PUBLIC]
//-----------------------------------------------------------------------------

IOMemoryDescriptor *
SCSIParallelTask::GetHBADataDescriptor ( void )
{
	return fHBADataDescriptor;
}


//-----------------------------------------------------------------------------
//	GetTimeoutDeadline -  Gets the timeout deadline in AbsoluteTime.   [PUBLIC]
//-----------------------------------------------------------------------------

AbsoluteTime
SCSIParallelTask::GetTimeoutDeadline ( void )
{
	return fTimeoutDeadline;
}


//-----------------------------------------------------------------------------
//	SetTimeoutDeadline -  Sets the timeout deadline in AbsoluteTime.   [PUBLIC]
//-----------------------------------------------------------------------------

void
SCSIParallelTask::SetTimeoutDeadline ( AbsoluteTime time )
{
	fTimeoutDeadline = time;
}


#if 0
#pragma mark -
#pragma mark Static Debugging Assertion Method
#pragma mark -
#endif


//-----------------------------------------------------------------------------
//	IOSCSIParallelFamilyDebugAssert -  Assertion routine.
//-----------------------------------------------------------------------------

void
IOSCSIParallelFamilyDebugAssert (	const char * componentNameString,
									const char * assertionString, 
									const char * exceptionLabelString,
									const char * errorString,
									const char * fileName,
									long		 lineNumber,
									int 		 errorCode )
{
	
	kprintf ( "%s Assert failed: %s ", componentNameString, assertionString );
	
	if ( exceptionLabelString != NULL )
		kprintf ( "%s ", exceptionLabelString );
	
	if ( errorString != NULL )
		kprintf ( "%s ", errorString );
	
	if ( fileName != NULL )
		kprintf ( "file: %s ", fileName );
	
	if ( lineNumber != 0 )
		kprintf ( "line: %ld ", lineNumber );
	
	if ( ( long ) errorCode != 0 )
		kprintf ( "error: %ld ", ( long ) errorCode );
	
	kprintf ( "\n" );
	
}
