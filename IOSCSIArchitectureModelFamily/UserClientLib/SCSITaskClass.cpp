/*
 * Copyright (c) 2001-2002 Apple Computer, Inc. All rights reserved.
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

//—————————————————————————————————————————————————————————————————————————————
//	Includes
//—————————————————————————————————————————————————————————————————————————————

// Private includes
#include "SCSITaskIUnknown.h"
#include "SCSITaskClass.h"
#include "SCSITaskDeviceClass.h"
#include "MMCDeviceUserClientClass.h"

// Since mach headers don’t have C++ wrappers we have to
// declare extern “C” before including them.
#ifdef __cplusplus
extern "C" {
#endif

#include <mach/mach_interface.h>
#include <IOKit/iokitmig.h>

#ifdef __cplusplus
}
#endif


//—————————————————————————————————————————————————————————————————————————————
//	Macros
//—————————————————————————————————————————————————————————————————————————————

#define DEBUG									0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING		"SCSITaskClass"

#if DEBUG
#define PRINT(x)								printf x
#else
#define PRINT(x)
#endif


#define kSCSITaskNULLReference					( UInt32 ) -1


#include "IOSCSIArchitectureModelFamilyDebugging.h"


//—————————————————————————————————————————————————————————————————————————————
//	Static variable initialization
//—————————————————————————————————————————————————————————————————————————————

SCSITaskInterface
SCSITaskClass::sSCSITaskInterface =
{
    0,
	&SCSITaskClass::sQueryInterface,
	&SCSITaskClass::sAddRef,
	&SCSITaskClass::sRelease,
	1, 0, // version/revision
	&SCSITaskClass::sIsTaskActive,
	&SCSITaskClass::sSetTaskAttribute,
	&SCSITaskClass::sGetTaskAttribute,
	&SCSITaskClass::sSetCommandDescriptorBlock,
	&SCSITaskClass::sGetCommandDescriptorBlockSize,
	&SCSITaskClass::sGetCommandDescriptorBlock,
	&SCSITaskClass::sSetScatterGatherEntries,
	&SCSITaskClass::sSetTimeoutDuration,
	&SCSITaskClass::sGetTimeoutDuration,
	&SCSITaskClass::sSetTaskCompletionCallback,
	&SCSITaskClass::sExecuteTaskAsync,
	&SCSITaskClass::sExecuteTaskSync,
	&SCSITaskClass::sAbortTask,
	&SCSITaskClass::sGetServiceResponse,
	&SCSITaskClass::sGetTaskState,
	&SCSITaskClass::sGetTaskStatus,
	&SCSITaskClass::sGetRealizedDataTransferCount,
	&SCSITaskClass::sGetAutoSenseData,
	&SCSITaskClass::sSetSenseDataBuffer
};


#if 0
#pragma mark -
#pragma mark Public Methods
#pragma mark -
#endif


//—————————————————————————————————————————————————————————————————————————————
//	• alloc - Called to allocate an instance of the class			[PUBLIC]
//—————————————————————————————————————————————————————————————————————————————

SCSITaskInterface **
SCSITaskClass::alloc ( SCSITaskDeviceClass * scsiTaskDevice,
					   io_connect_t connection,
					   mach_port_t asyncPort )
{
	
	IOReturn				status 		= kIOReturnSuccess;
	SCSITaskClass *			task		= NULL;
	SCSITaskInterface **	interface 	= NULL;
	
	PRINT ( ( "SCSITaskClass::alloc called\n" ) );
	
	// Use new to create a new instance of the class.
	task = new SCSITaskClass ( );
	require_nonzero ( task, Error_Exit );
	
	// Allocation succeeded in user space. Now Init the class. Init
	// will do everything necessary to create our copy in the kernel
	// and take care of setting up our buffers. If any of that fails,
	// Init fails, and we return NULL.
	status = task->Init ( scsiTaskDevice, connection, asyncPort );
	require_success_action ( status, Error_Exit, delete task );
	
	// Everything went ok if we got here. Set the interface up.
	interface = ( SCSITaskInterface ** ) &task->fInterfaceMap.pseudoVTable;
	
	
Error_Exit:
	
	
	return interface;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sSetConnectionAndPort - C->C++ glue code.						[PUBLIC]
//—————————————————————————————————————————————————————————————————————————————

void
SCSITaskClass::sSetConnectionAndPort ( const void * task, void * context )
{
	
	SCSITaskClass *					myTask;
	MyConnectionAndPortContext *	myContext;
	
	myTask 		= getThis ( ( void * ) task );
	myContext 	= ( MyConnectionAndPortContext * ) context;
	
	PRINT ( ( "sSetConnectionAndPort called, task = %p, context = %p\n", myTask, myContext ) );
	
	// Glue through.
	( void ) myTask->SetConnectionAndPort ( myContext->connection, myContext->asyncPort );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sAbortAndReleaseTasks - Static function for C->C++ glue.		[PUBLIC]
//—————————————————————————————————————————————————————————————————————————————

void
SCSITaskClass::sAbortAndReleaseTasks ( const void * value, void * context )
{
	
	SCSITaskClass *		task = NULL;
	
	require_nonzero ( value, Error_Exit );
	
	task = getThis ( ( void * ) value );
	
	PRINT ( ( "SCSITaskClass::sAbortAndReleaseTasks\n" ) );
	
	// Sanity checks. Make sure it is non-NULL and that it is active.
	require_nonzero ( task, Error_Exit );
	nrequire ( task->IsTaskActive ( ), Error_Exit );
	
	// Abort it.
	( void ) task->AbortTask ( );
	
	
Error_Exit:
	
	
	return;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• SetConnectionAndPort - 	Called to set the connection and async
//								notification port for a task.		[PUBLIC]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskClass::SetConnectionAndPort ( 	io_connect_t 	connection,
										mach_port_t 	asyncPort )
{
	
	IOReturn				status 		= kIOReturnSuccess;
	io_async_ref_t 			asyncRef	= { 0 };
	io_scalar_inband_t		params		= { 0 };
	mach_msg_type_number_t	size		= 0;
		
	PRINT ( ( "SCSITaskClass::SetConnectionAndPort called\n" ) );
	
	check ( connection != 0 );
	
	// Set the connection
	fConnection = connection;
	
	// Make sure the async port is non-NULL
	require_action_quiet ( asyncPort != MACH_PORT_NULL,
						   Error_Exit,
						   fAsyncPort = MACH_PORT_NULL );
	
	// We got a port to use, so call the async method to set the callback
	// and pass the async port to it.
	
	asyncRef[0] = 0;
	params[0]	= ( UInt32 ) fTaskArguments.taskReference;
	params[1]	= ( UInt32 ) ( IOAsyncCallback ) &SCSITaskClass::sTaskCompletion;
	params[2]	= ( UInt32 ) this;
	
	status = io_async_method_scalarI_scalarO ( 	fConnection,
												asyncPort,
												asyncRef,
												1,
												kSCSITaskUserClientSetAsyncCallback,
												params,
												3,
												NULL,
												&size );	
	
	PRINT ( ( "SetAsyncCallback : status = 0x%08x\n", status ) );
	require_success ( status, Error_Exit );
	
	// Success. Good, save the port for further reference.
	fAsyncPort = asyncPort;	
	
	
Error_Exit:
	
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• Init - Called to initialize a task.							[PUBLIC]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskClass::Init ( SCSITaskDeviceClass * scsiTaskDevice,
					  io_connect_t connection,
					  mach_port_t asyncPort )
{
	
	IOReturn	status = kIOReturnSuccess;
	
	PRINT ( ( "SCSITaskClass::Init called\n" ) );
	
	// Sanity check
	require_nonzero_action ( scsiTaskDevice, Error_Exit, status = kIOReturnBadArgument );
	require_nonzero_action ( connection, Error_Exit, status = kIOReturnBadArgument );
	
	// Save the device for further reference.
	fSCSITaskDevice = scsiTaskDevice;
	
	// Create a task. Call the method to create a task and save the
	// reference to it in our slot for it in the fTaskArguments structure.
	status = IOConnectMethodScalarIScalarO ( connection,
											 kSCSITaskUserClientCreateTask,
											 0,
											 1,
											 &fTaskArguments.taskReference );
	require_success ( status, Error_Exit );
	
	PRINT ( ( "fTaskArguments.taskReference = %ld\n", fTaskArguments.taskReference ) );
	
	// Ok. Good, now we can set the connection and port passed in.
	status = SetConnectionAndPort ( connection, asyncPort );
	require_success ( status, Error_Exit );
	
	// Ok. That part worked. Now, we need to set the buffers for our results
	// and our sense data. Do that now, or we can’t be guaranteed it won’t fail.
	status = SetSenseDataBuffer ( &fSenseData, sizeof ( fSenseData ) );
	require_success ( status, Error_Exit );
	
	PRINT ( ( "SetBuffers succeeded\n" ) );
	
	PRINT ( ( "SCSITaskClass : fConnection %d, fAsyncPort %d, fSCSITaskDevice = %p\n", 
				fConnection, fAsyncPort, fSCSITaskDevice ) );
	
	
Error_Exit:
	
	
	PRINT ( ( "SCSITaskClass :  status = 0x%08x\n", status ) );
	
	return status;
	
}


#if 0
#pragma mark -
#pragma mark Protected Methods
#pragma mark -
#endif


//—————————————————————————————————————————————————————————————————————————————
//	• Default Constructor - Called on allocation				[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————


SCSITaskClass::SCSITaskClass ( void ) :
			SCSITaskIUnknown ( &sSCSITaskInterface )
{
	
	// Set some fields to zero.
	fConnection 	= 0;
	fTaskState		= kSCSITaskState_NEW_TASK;
	
	PRINT ( ( "SCSITaskClass constructor called\n" ) );
	
	// set the args and results to known values (zero).
	memset ( &fTaskArguments, 0, sizeof ( fTaskArguments ) );
	memset ( &fTaskResults, 0, sizeof ( fTaskResults ) );
	
	// NULL out these members.
	fCallbackFunction	= NULL;
	fCallbackRefCon 	= NULL;
	fAsyncPort			= NULL;
	fExternalSenseData	= NULL;
	
	// Set the task reference to an invalid reference
	fTaskArguments.taskReference = kSCSITaskNULLReference;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• Default Destructor - Called on deallocation				[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

SCSITaskClass::~SCSITaskClass ( void )
{

	IOReturn 	status = kIOReturnSuccess;
	
	PRINT ( ( "SCSITaskClass destructor called\n" ) );
	
	if ( fSCSITaskDevice != NULL )
	{
		
		// Remove the task from the working task set.
		PRINT ( ( "Removing task from set\n" ) );
		fSCSITaskDevice->RemoveTaskFromTaskSet (
				( SCSITaskInterface ** ) &fInterfaceMap.pseudoVTable );
		
	}
	
	if ( fTaskArguments.taskReference != kSCSITaskNULLReference )
	{
		
		// Delete the task in the kernel.
		PRINT ( ( "Releasing task\n" ) );
		status = IOConnectMethodScalarIScalarO ( fConnection, 	
												 kSCSITaskUserClientReleaseTask,
												 1,
												 0,
												 fTaskArguments.taskReference );
		
		PRINT ( ( "SCSITaskClass : release task status = 0x%08x\n", status ) );
		
	}
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• QueryInterface - Called to obtain the presence of an interface
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

HRESULT
SCSITaskClass::QueryInterface ( REFIID iid, void ** ppv )
{
	
	CFUUIDRef 	uuid 	= CFUUIDCreateFromUUIDBytes ( NULL, iid );
	HRESULT 	result 	= S_OK;
	
	PRINT ( ( "SCSITaskClass::QueryInterface\n" ) );
	
	// Is it the unknown or the one we know? If so, pass back the map
	// and add a refcount.
	if ( CFEqual ( uuid, IUnknownUUID ) || CFEqual ( uuid, kIOSCSITaskInterfaceID ) )
	{
        
		*ppv = &fInterfaceMap;
        AddRef ( );
		
    }
	
    else
    {
		
		// Not something we expected, so pass back zero.
		*ppv = 0;
		result = E_NOINTERFACE;
		
	}
	
	// Be good and cleanup what we allocated above.
	CFRelease ( uuid );
	
	return result;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• IsTaskActive - Called to find out if a task is active or not.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

Boolean
SCSITaskClass::IsTaskActive ( void )
{
	
	Boolean		isActive;
	
	// Find out if the task is active. A task is active if it is not in the
	// kSCSITaskState_NEW_TASK state and is not in the kSCSITaskState_ENDED
	// state.
	isActive = ( ( fTaskState != kSCSITaskState_NEW_TASK ) &&
				 ( fTaskState != kSCSITaskState_ENDED ) ) ? true : false;
	
	PRINT ( ( "SCSITaskClass : IsTaskActive isActive = %d\n", isActive ) );
	
	return isActive;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• SetTaskAttribute - Called to set the SCSITaskAttribute value.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

void
SCSITaskClass::SetTaskAttribute ( SCSITaskAttribute inAttributeValue )
{
	
	PRINT ( ( "SCSITaskClass : SetTaskAttribute\n" ) );
	PRINT ( ( "taskAttribute = %d\n", inAttributeValue ) );
	fTaskArguments.taskAttribute = inAttributeValue;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• GetTaskAttribute - Called to get the SCSITaskAttribute value.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

SCSITaskAttribute
SCSITaskClass::GetTaskAttribute ( void )
{
	
	PRINT ( ( "SCSITaskClass : GetTaskAttribute\n" ) );
	PRINT ( ( "taskAttribute = %d\n", fTaskArguments.taskAttribute ) );
	return fTaskArguments.taskAttribute;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• SetCommandDescriptorBlock - Called to set the CDB.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskClass::SetCommandDescriptorBlock ( UInt8 * inCDB, UInt8 inSize )
{
	
	IOReturn	status = kIOReturnBadArgument;
	
	PRINT ( ( "SCSITaskClass : SetCommandDescriptorBlock\n" ) );
	
	require_nonzero ( inCDB, Error_Exit );
	
	// Sanity check
	switch ( inSize )
	{
		
		case kSCSICDBSize_6Byte:
		case kSCSICDBSize_10Byte:
		case kSCSICDBSize_12Byte:
		case kSCSICDBSize_16Byte:
			break;
		
		default:
			goto Error_Exit;
			break;
		
	}
	
	// Copy the CDB. Make sure to clean out any stale data.
	memset ( fTaskArguments.cdbData, 0, sizeof ( SCSICommandDescriptorBlock ) );
	memcpy ( fTaskArguments.cdbData, inCDB, inSize );
	
	// Be sure to set the size as well.
	fTaskArguments.cdbSize = inSize;
	status = kIOReturnSuccess;
	
	
Error_Exit:
	
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• GetCommandDescriptorBlockSize - Called to obtain the CDB size.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

UInt8
SCSITaskClass::GetCommandDescriptorBlockSize ( void )
{
	
	check ( fTaskArguments.cdbSize != 0 );
	return fTaskArguments.cdbSize;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• GetCommandDescriptorBlock - Called to obtain the CDB.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

void
SCSITaskClass::GetCommandDescriptorBlock ( UInt8 * outCDB )
{
	
	// Copy the data to the supplied buffer. This assumes a correct-
	// sized destination buffer as documented in the SCSITaskLib
	// header file.
	check ( outCDB != NULL );
	memcpy ( outCDB, fTaskArguments.cdbData, fTaskArguments.cdbSize );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• SetScatterGatherEntries - Called to set the scatter-gather entries.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskClass::SetScatterGatherEntries ( IOVirtualRange * inScatterGatherList,
										 UInt8 inScatterGatherEntries,
										 UInt64 transferCount,
										 UInt8 transferDirection )
{
	
	IOReturn 		status 	= kIOReturnSuccess;
	
	PRINT ( ( "SCSITaskClass : SetScatterGatherEntries\n" ) );
	
	check ( inScatterGatherList != NULL );
	
	fSGList 								= inScatterGatherList;
	fTaskArguments.scatterGatherEntries 	= inScatterGatherEntries;
	fTaskArguments.requestedTransferCount 	= transferCount;
	fTaskArguments.transferDirection		= transferDirection;
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• SetTimeoutDuration - Called to set the timeout duration in milliseconds.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

void
SCSITaskClass::SetTimeoutDuration ( UInt32 timeoutDurationMS )
{
	
	PRINT ( ( "SCSITaskClass : SetTimeoutDuration\n" ) );
	PRINT ( ( "duration = %ldms\n", timeoutDurationMS ) );
	fTaskArguments.timeoutDuration = timeoutDurationMS;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• GetTimeoutDuration - Called to get the timeout duration in milliseconds.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

UInt32
SCSITaskClass::GetTimeoutDuration ( void )
{
	
	PRINT ( ( "SCSITaskClass : GetTimeoutDuration\n" ) );
	return fTaskArguments.timeoutDuration;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• SetTaskCompletionCallback - Called to set the callback routine.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

void
SCSITaskClass::SetTaskCompletionCallback ( SCSITaskCallbackFunction callback,
										   void * refCon )
{
	
	PRINT ( ( "SCSITaskClass : SetTaskCompletionCallback\n" ) );
	
	fCallbackFunction 	= callback;
	fCallbackRefCon 	= refCon;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• ExecuteTaskAsync - Called to execute the task asynchronously.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskClass::ExecuteTaskAsync ( void )
{
	
	IOReturn	status = kIOReturnNotPermitted;
	
	PRINT ( ( "SCSITaskClass : ExecuteTaskAsync\n" ) );
	
	// Sanity checks. Check for a valid async notification port.
	require ( fAsyncPort != MACH_PORT_NULL, Error_Exit );
	
	// Check for a valid callback function.
	require_nonzero ( fCallbackFunction, Error_Exit );
	
	// Not synchronous.
	fTaskArguments.isSync = false;
	
	// Call through to the helper function which does the real work.
	status = ExecuteTask ( );
	
	
Error_Exit:
	
	
	PRINT ( ( "SCSITaskClass : ExecuteTaskAsync status = 0x%08x\n", status ) );
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• ExecuteTaskSync - Called to execute the task synchronously.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskClass::ExecuteTaskSync ( SCSI_Sense_Data * senseDataBuffer,
								 SCSITaskStatus * taskStatus,
								 UInt64 * realizedTransferCount )
{
	
	IOReturn 	status 	= kIOReturnSuccess;
	
	PRINT ( ( "SCSITaskClass : ExecuteTaskSync\n" ) );
	
	// Is synchronous.
	fTaskArguments.isSync = true;
	
	// Call through to the helper function which does the real work.
	status = ExecuteTask ( );
	
	// Set the task state.
	fTaskState = kSCSITaskState_ENDED;
	
	PRINT ( ( "SCSITaskClass : ExecuteTaskSync status = 0x%08x\n", status ) );
	
	// Make sure to set the incoming vars to the results we just got.
	if ( taskStatus != NULL )
	{
		*taskStatus = fTaskResults.taskStatus;
	}
	
	if ( realizedTransferCount != NULL )
	{
		*realizedTransferCount = fTaskResults.realizedTransferCount;
	}
	
	if ( senseDataBuffer != NULL )
	{
		
		// Check if we need to copy the sense data or not.
		if ( ( fTaskResults.serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( fTaskResults.taskStatus == kSCSITaskStatus_CHECK_CONDITION ) )
		{
			
			if ( fExternalSenseData == NULL )
			{
				
				// Yes, copy it to the supplied buffer
				memcpy ( senseDataBuffer, &fSenseData, sizeof ( SCSI_Sense_Data ) );
				
				PRINT ( ( "SENSE_KEY_CODE: 0x%02x, ASC: 0x%02x, ASCQ: 0x%02x\n",
					   senseDataBuffer->SENSE_KEY & kSENSE_KEY_Mask,
					   senseDataBuffer->ADDITIONAL_SENSE_CODE,
					   senseDataBuffer->ADDITIONAL_SENSE_CODE_QUALIFIER ) );
				
			}
			
		}
		
	}
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• AbortTask - Called to abort the task.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskClass::AbortTask ( void )
{
	
	IOReturn 	status = kIOReturnSuccess;
	
	PRINT ( ( "SCSITaskClass : AbortTask\n" ) );
	
	// Send an abort to the kernel task.
	status = IOConnectMethodScalarIScalarO (	fConnection, 	
												kSCSITaskUserClientAbortTask, 
												1,
												0,
												fTaskArguments.taskReference );
	
	PRINT ( ( "SCSITaskClass : AbortTask status = 0x%08x\n", status ) );
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• GetTaskState - Called to obtain the SCSITaskState.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

SCSITaskState
SCSITaskClass::GetTaskState ( void )
{
	
	PRINT ( ( "SCSITaskClass : GetTaskState taskState = %d\n", fTaskState ) );
	return fTaskState;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• GetTaskStatus - Called to obtain the SCSITaskStatus.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

SCSITaskStatus
SCSITaskClass::GetTaskStatus ( void )
{
	
	PRINT ( ( "SCSITaskClass : GetTaskStatus taskStatus = %d\n", fTaskResults.taskStatus ) );
	return fTaskResults.taskStatus;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• GetRealizedDataTransferCount - 	Called to obtain the actual amount of
//										data transferred in bytes.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

UInt64
SCSITaskClass::GetRealizedDataTransferCount ( void )
{
	
	PRINT ( ( "SCSITaskClass : GetRealizedDataTransferCount\n" ) );
	PRINT ( ( "0x%lx 0x%lx\n", fTaskResults.realizedTransferCount >> 32,
			  fTaskResults.realizedTransferCount & 0xFFFFFFFF ) );
	
	return fTaskResults.realizedTransferCount;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• GetServiceResponse - 	Called to obtain the SCSIServiceResponse.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

SCSIServiceResponse
SCSITaskClass::GetServiceResponse ( void )
{
	
	PRINT ( ( "SCSITaskClass : GetServiceResponse\n" ) );
	PRINT ( ( "serviceResponse = %d\n", fTaskResults.serviceResponse ) );
	return fTaskResults.serviceResponse;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• GetAutoSenseData - Called to obtain the SCSI Sense Data.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskClass::GetAutoSenseData ( SCSI_Sense_Data * senseDataBuffer )
{
	
	IOReturn	status = kIOReturnNotPermitted;
	
	require_nonzero ( senseDataBuffer, ErrorExit );
	
	// Should we copy it?
	if ( ( fTaskResults.serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( fTaskResults.taskStatus == kSCSITaskStatus_CHECK_CONDITION ) )
	{
		
		// Yes. Copy it and set the status to success.
		if ( fExternalSenseData == NULL )
		{
			
			// Yes, copy it to the supplied buffer
			memcpy ( senseDataBuffer, &fSenseData, sizeof ( SCSI_Sense_Data ) );
			status = kIOReturnSuccess;
			
		}
		
		PRINT ( ( "SENSE_KEY_CODE: 0x%02x, ASC: 0x%02x, ASCQ: 0x%02x\n",
			   senseDataBuffer->SENSE_KEY & kSENSE_KEY_Mask,
			   senseDataBuffer->ADDITIONAL_SENSE_CODE,
			   senseDataBuffer->ADDITIONAL_SENSE_CODE_QUALIFIER ) );
		
	}
	
	
ErrorExit:
	
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• TaskCompletion - 	Internal async callback routine called by
//						IODispatchCalloutFromMessage. It calls the
//						user-supplied callback method.
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

void
SCSITaskClass::TaskCompletion ( IOReturn result, void ** args, int numArgs )
{
	
	PRINT ( ( "SCSITaskClass : TaskCompletion, numArgs = %d\n", numArgs ) );
	PRINT ( ( "SCSITaskClass : TaskCompletion, result = %d\n", result ) );
	
	// Set the state
	fTaskState = kSCSITaskState_ENDED;
	
	// Call the supplied callback if it exists. It should ALWAYS exist, but
	// just in case, we check here.
	if ( fCallbackFunction != NULL )
		( fCallbackFunction )( 	fTaskResults.serviceResponse,
								fTaskResults.taskStatus,
								fTaskResults.realizedTransferCount,
								fCallbackRefCon );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• ExecuteTask - Internal method called by ExecuteTaskSync and
//					ExecuteTaskAsync which handles the user-kernel transition.
//																	[PRIVATE]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskClass::ExecuteTask ( void )
{
	
	IOReturn				status				= kIOReturnSuccess;
	UInt32					size 				= 0;
	UInt8					commandData[4096]	= { 0 };
	SCSITaskData *			args;
	
	PRINT ( ( "SCSITaskClass::ExecuteTask\n" ) );
	
	// Init the transfer count.
	fTaskResults.realizedTransferCount 	= 0;
	fTaskState							= kSCSITaskState_ENABLED;
	
	args = ( SCSITaskData * ) commandData;
	*args = fTaskArguments;
	
	PRINT ( ( "Copying SG entries\n" ) );
	
	// Copy the scatter-gather entries.
	memcpy ( &args->scatterGatherList[0], fSGList, fTaskArguments.scatterGatherEntries * sizeof ( IOVirtualRange ) );
	
	PRINT ( ( "Determining size\n" ) );
	
	PRINT ( ( "Struct size = %ld\n", sizeof ( SCSITaskData ) ) );
	
	// Get the size. Subtract the pointer value of where the last arg
	// is to the original buffer to get the size.
	size = ( ( char * ) &args->scatterGatherList[fTaskArguments.scatterGatherEntries] ) - ( char * ) commandData;
	
	PRINT ( ( "size = %ld\n", size ) );
	
	// Call into the kernel to do the magic!
	status = IOConnectMethodScalarIStructureI ( fConnection,
												kSCSITaskUserClientExecuteTask,
												0,
												size,
												commandData );
	
	PRINT ( ( "ExecuteTask status = 0x%08x\n", status ) );
	
	return status;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• SetSenseDataBuffer - 	Called to set a sense data buffer to use instead
//							of the one provided by the class.
//																	[PRIVATE]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskClass::SetSenseDataBuffer ( void * buffer, UInt8 bufferSize )
{
	
	IOReturn	status 	= kIOReturnBadArgument;
	UInt8		size	= 0;
	
	PRINT ( ( "SCSITaskClass::SetSenseDataBuffer\n" ) );
	
	require_nonzero ( bufferSize, ErrorExit );
	
	// Sanity check
	if ( buffer == NULL )
	{
		
		PRINT ( ( "buffer == NULL\n" ) );
		
		buffer	= ( void * ) &fSenseData;
		size	= sizeof ( fSenseData );
		
		bufferSize = ( bufferSize < size ) ? bufferSize : size;
		
	}
	
	if ( buffer != &fSenseData )
	{	
		fExternalSenseData = ( SCSI_Sense_Data * ) buffer;	
	}
	
	else
	{
		fExternalSenseData = NULL;
	}
	
	PRINT ( ( "taskReference = %ld\n", fTaskArguments.taskReference ) );
	PRINT ( ( "fTaskResultsBuffer = %x\n", &fTaskResults ) );
	PRINT ( ( "senseDataBuffer = %x\n", buffer ) );
	PRINT ( ( "senseDataBufferSize = %ld\n", bufferSize ) );
	
	// Call into the kernel to set the buffers.
	status = IOConnectMethodScalarIScalarO ( fConnection,
											 kSCSITaskUserClientSetBuffers,
											 4,
											 0,
											 fTaskArguments.taskReference,
											 &fTaskResults,
											 buffer,
											 bufferSize );
	
	PRINT ( ( "SetBuffers returned status = 0x%08x\n", status ) );
	
	
ErrorExit:
	
	
	return status;
	
}


#if 0
#pragma mark -
#pragma mark Static C->C++ Glue Functions
#pragma mark -
#endif


//—————————————————————————————————————————————————————————————————————————————
//	• sIsTaskActive - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

Boolean
SCSITaskClass::sIsTaskActive ( void * task )
{
	
	check ( task != NULL );
	return getThis ( task )->IsTaskActive ( );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sSetTaskAttribute - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskClass::sSetTaskAttribute (
						void *				task,
						SCSITaskAttribute 	inAttributeValue )
{
	
	check ( task != NULL );
	getThis ( task )->SetTaskAttribute ( inAttributeValue );
	return kIOReturnSuccess;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sGetTaskAttribute - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskClass::sGetTaskAttribute (
						void * 				task,
						SCSITaskAttribute * outTaskAttributeValue )
{
	
	check ( task != NULL );
	*outTaskAttributeValue = getThis ( task )->GetTaskAttribute ( );
	return kIOReturnSuccess;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sSetCommandDescriptorBlock - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskClass::sSetCommandDescriptorBlock ( void *		task,
											UInt8 * 	inCDB,
											UInt8		inSize )
{
	
	check ( task != NULL );
	return getThis ( task )->SetCommandDescriptorBlock ( inCDB, inSize );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sGetCommandDescriptorBlockSize - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

UInt8
SCSITaskClass::sGetCommandDescriptorBlockSize ( void * task )
{
	
	check ( task != NULL );
	return getThis ( task )->GetCommandDescriptorBlockSize ( );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sGetCommandDescriptorBlock - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskClass::sGetCommandDescriptorBlock ( void * task, UInt8 * outCDB )
{
	
	check ( task != NULL );
	getThis ( task )->GetCommandDescriptorBlock ( outCDB );
	return kIOReturnSuccess;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sSetScatterGatherEntries - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskClass::sSetScatterGatherEntries (
							void * 				task,
							IOVirtualRange * 	inScatterGatherList,
							UInt8				inScatterGatherEntries,
							UInt64				transferCount,
							UInt8				transferDirection )
{
	
	check ( task != NULL );
	return getThis ( task )->SetScatterGatherEntries (
												inScatterGatherList,
												inScatterGatherEntries,
												transferCount,
												transferDirection );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sSetTimeoutDuration - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskClass::sSetTimeoutDuration ( void * task, UInt32 timeoutDurationMS )
{
	
	check ( task != NULL );
	getThis ( task )->SetTimeoutDuration ( timeoutDurationMS );
	return kIOReturnSuccess;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sGetTimeoutDuration - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

UInt32
SCSITaskClass::sGetTimeoutDuration ( void * task )
{
	
	check ( task != NULL );
	return getThis ( task )->GetTimeoutDuration ( );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sGetTimeoutDuration - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskClass::sSetTaskCompletionCallback (
					void *						task,
					SCSITaskCallbackFunction 	callback,
					void *						refCon )
{
	
	check ( task != NULL );
	getThis ( task )->SetTaskCompletionCallback ( callback, refCon );
	return kIOReturnSuccess;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sExecuteTaskAsync - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskClass::sExecuteTaskAsync ( void * task )
{
	
	check ( task != NULL );
	return getThis ( task )->ExecuteTaskAsync ( );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sExecuteTaskSync - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskClass::sExecuteTaskSync (
							void * 				task,
							SCSI_Sense_Data * 	senseDataBuffer,
							SCSITaskStatus * 	taskStatus,
							UInt64 *			realizedTransferCount )
{
	
	check ( task != NULL );
	return getThis ( task )->ExecuteTaskSync ( 	senseDataBuffer,
												taskStatus,
												realizedTransferCount );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sAbortTask - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskClass::sAbortTask ( void * task )
{
	
	check ( task != NULL );
	return getThis ( task )->AbortTask ( );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sGetTaskState - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskClass::sGetTaskState ( void * task, SCSITaskState * outTaskState )
{
	
	check ( task != NULL );
	*outTaskState = getThis ( task )->GetTaskState ( );
	return kIOReturnSuccess;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sGetTaskStatus - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskClass::sGetTaskStatus ( void * task, SCSITaskStatus * outTaskStatus )
{
	
	check ( task != NULL );
	*outTaskStatus = getThis ( task )->GetTaskStatus ( );
	return kIOReturnSuccess;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sGetRealizedDataTransferCount - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

UInt64
SCSITaskClass::sGetRealizedDataTransferCount ( void * task )
{
	
	check ( task != NULL );
	return getThis ( task )->GetRealizedDataTransferCount ( );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sGetServiceResponse - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskClass::sGetServiceResponse (
							void *					task,
							SCSIServiceResponse * 	serviceResponse )
{
	
	check ( task != NULL );
	*serviceResponse = getThis ( task )->GetServiceResponse ( );
	return kIOReturnSuccess;
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sGetAutoSenseData - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskClass::sGetAutoSenseData (
							void *				task,
							SCSI_Sense_Data *	senseDataBuffer )
{
	
	check ( task != NULL );
	return getThis ( task )->GetAutoSenseData ( senseDataBuffer );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sTaskCompletion - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

void
SCSITaskClass::sTaskCompletion ( 	void *		refcon,
									IOReturn	result,
									void **		args,
									int			numArgs )
{
	
	PRINT ( ( "SCSITaskClass : sTaskCompletion, numArgs = %d\n", numArgs ) );
	check ( refcon != NULL );
	( ( SCSITaskClass * ) refcon )->TaskCompletion ( result, args, numArgs );
	
}


//—————————————————————————————————————————————————————————————————————————————
//	• sSetSenseDataBuffer - Static function for C->C++ glue
//																	[PROTECTED]
//—————————————————————————————————————————————————————————————————————————————

IOReturn
SCSITaskClass::sSetSenseDataBuffer ( 	void * 				task,
										SCSI_Sense_Data * 	buffer,
										UInt8				bufferSize )
{
	
	check ( task != NULL );
	return getThis ( task )->SetSenseDataBuffer ( ( void * ) buffer, bufferSize );
	
}