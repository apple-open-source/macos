/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Includes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// Private includes
#include "SCSITaskUserClient.h"
#include "SCSITaskLib.h"

// Libkern includes
#include <libkern/OSAtomic.h>

// IOKit includes
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOWorkLoop.h>

// IOKit storage includes
#include <IOKit/storage/IOBlockStorageDriver.h>

// SCSI Architecture Model Family includes
#include <IOKit/scsi/IOSCSIPrimaryCommandsDevice.h>
#include <IOKit/scsi/IOSCSIMultimediaCommandsDevice.h>
#include <IOKit/scsi/SCSICmds_REQUEST_SENSE_Defs.h>
#include <IOKit/scsi/SCSICmds_INQUIRY_Definitions.h>
#include <IOKit/scsi/SCSICommandOperationCodes.h>


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define DEBUG 									0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING		"SCSITaskUserClient"

#if DEBUG
#define SCSI_TASK_USER_CLIENT_DEBUGGING_LEVEL	0
#endif

#include "IOSCSIArchitectureModelFamilyDebugging.h"


#if ( SCSI_TASK_USER_CLIENT_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_TASK_USER_CLIENT_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_TASK_USER_CLIENT_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif


#define super IOUserClient
OSDefineMetaClassAndStructors ( SCSITaskUserClient, IOUserClient );


#if 0
#pragma mark -
#pragma mark Constants
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Constants
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOExternalMethod
SCSITaskUserClient::sMethods[kSCSITaskUserClientMethodCount] =
{
	{
		// Method #0 IsExclusiveAccessAvailable
		0,
		( IOMethod ) &SCSITaskUserClient::IsExclusiveAccessAvailable,
		kIOUCScalarIScalarO,
		0,
		0
	},
	{
		// Method #1 ObtainExclusiveAccess
		0,
		( IOMethod ) &SCSITaskUserClient::ObtainExclusiveAccess,
		kIOUCScalarIScalarO,
		0,
		0
	},
	{
		// Method #2 ReleaseExclusiveAccess
		0,
		( IOMethod ) &SCSITaskUserClient::ReleaseExclusiveAccess,
		kIOUCScalarIScalarO,
		0,
		0
	},
	{
		// Method #3 CreateTask
		0,
		( IOMethod ) &SCSITaskUserClient::CreateTask,
		kIOUCScalarIScalarO,
		0,
		1
	},
	{
		// Method #4 ReleaseTask
		0,
		( IOMethod ) &SCSITaskUserClient::ReleaseTask,
		kIOUCScalarIScalarO,
		1,
		0
	},
	{
		// Method #5 AbortTask
		0,
		( IOMethod ) &SCSITaskUserClient::AbortTask,
		kIOUCScalarIScalarO,
		1,
		0
	},
	{
		// Method #6 ExecuteTask
		0,
		( IOMethod ) &SCSITaskUserClient::ExecuteTask,
		kIOUCScalarIStructI,
		0,
		0xFFFFFFFF
	},
	{
		// Method #7 SetBuffers
		0,
		( IOMethod ) &SCSITaskUserClient::SetBuffers,
		kIOUCScalarIScalarO,
		4,
		0
	},
	{
		// Method #8 Inquiry
		0,
		( IOMethod ) &SCSITaskUserClient::Inquiry,
		kIOUCStructIStructO,
		sizeof ( AppleInquiryStruct ),
		sizeof ( SCSITaskStatus )
	},
	{
		// Method #9 TestUnitReady
		0,
		( IOMethod ) &SCSITaskUserClient::TestUnitReady,
		kIOUCScalarIStructO,
		1,
		sizeof ( SCSITaskStatus )
	},
	{
		// Method #10 GetPerformance
		0,
		( IOMethod ) &SCSITaskUserClient::GetPerformance,
		kIOUCStructIStructO,
		sizeof ( AppleGetPerformanceStruct ),
		sizeof ( SCSITaskStatus )
	},
	{
		// Method #11 GetConfiguration
		0,
		( IOMethod ) &SCSITaskUserClient::GetConfiguration,
		kIOUCStructIStructO,
		sizeof ( AppleGetConfigurationStruct ),
		sizeof ( SCSITaskStatus )
	},
	{
		// Method #12 ModeSense10
		0,
		( IOMethod ) &SCSITaskUserClient::ModeSense10,
		kIOUCStructIStructO,
		sizeof ( AppleModeSense10Struct ),
		sizeof ( SCSITaskStatus )
	},
	{
		// Method #13 SetWriteParametersModePage
		0,
		( IOMethod ) &SCSITaskUserClient::SetWriteParametersModePage,
		kIOUCStructIStructO,
		sizeof ( AppleWriteParametersModePageStruct ),
		sizeof ( SCSITaskStatus )
	},
	{
		// Method #14 GetTrayState
		0,
		( IOMethod ) &SCSITaskUserClient::GetTrayState,
		kIOUCScalarIScalarO,
		0,
		1
	},
	{
		// Method #15 SetTrayState
		0,
		( IOMethod ) &SCSITaskUserClient::SetTrayState,
		kIOUCScalarIScalarO,
		1,
		0
	},
	{
		// Method #16 ReadTableOfContents
		0,
		( IOMethod ) &SCSITaskUserClient::ReadTableOfContents,
		kIOUCStructIStructO,
		sizeof ( AppleReadTableOfContentsStruct ),
		sizeof ( SCSITaskStatus )
	},
	{
		// Method #17 ReadDiscInformation
		0,
		( IOMethod ) &SCSITaskUserClient::ReadDiscInformation,
		kIOUCStructIStructO,
		sizeof ( AppleReadDiscInfoStruct ),
		sizeof ( SCSITaskStatus )
	},
	{
		// Method #18 ReadTrackInformation
		0,
		( IOMethod ) &SCSITaskUserClient::ReadTrackInformation,
		kIOUCStructIStructO,
		sizeof ( AppleReadTrackInfoStruct ),
		sizeof ( SCSITaskStatus )
	},
	{
		// Method #19 ReadDVDStructure
		0,
		( IOMethod ) &SCSITaskUserClient::ReadDVDStructure,
		kIOUCStructIStructO,
		sizeof ( AppleReadDVDStructureStruct ),
		sizeof ( SCSITaskStatus )
	},
	{
		// Method #20 SetCDSpeed
		0,
		( IOMethod ) &SCSITaskUserClient::SetCDSpeed,
		kIOUCStructIStructO,
		sizeof ( AppleSetCDSpeedStruct ),
		sizeof ( SCSITaskStatus )
	},
	{
		// Method #21 ReadFormatCapacities
		0,
		( IOMethod ) &SCSITaskUserClient::ReadFormatCapacities,
		kIOUCStructIStructO,
		sizeof ( AppleReadFormatCapacitiesStruct ),
		sizeof ( SCSITaskStatus )
	}
};


IOExternalAsyncMethod SCSITaskUserClient::sAsyncMethods[kSCSITaskUserClientAsyncMethodCount] = 
{
    {   //  Async Method #0  SetAsyncCallback
        0,
        ( IOAsyncMethod ) &SCSITaskUserClient::SetAsyncCallback,
        kIOUCScalarIScalarO,
        3,
        0
    }
};


#if 0
#pragma mark -
#pragma mark Public Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ initWithTask - Save task_t and validate the connection type	[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSITaskUserClient::initWithTask ( task_t owningTask,
								   void * securityToken,
								   UInt32 type )
{
	
	bool	result	= false;
	
	STATUS_LOG ( ( "SCSITaskUserClient::initWithTask called\n" ) );
	
	result = super::initWithTask ( owningTask, securityToken, type );
	require ( result, BAD_CONNECTION_TYPE );
	require_action ( ( type == kSCSITaskLibConnection ), BAD_CONNECTION_TYPE, result = false );
	
	fTask = owningTask;
	result = true;
	
	
BAD_CONNECTION_TYPE:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ start - Start providing services								[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSITaskUserClient::start ( IOService * provider )
{
	
	bool			result		= false;
	OSIterator *	iterator	= NULL;
	OSObject *		object		= NULL;
	IOWorkLoop *	workLoop	= NULL;
	
	STATUS_LOG ( ( "SCSITaskUserClient::start\n" ) );
	
	require ( ( fProvider == 0 ), GENERAL_ERR );
	require ( super::start ( provider ), GENERAL_ERR );
	
	// Zero our array for tasks.
	bzero ( fArray, sizeof ( SCSITask * ) * kMaxSCSITaskArraySize );
	
	// Save the provider
	fProvider = provider;
	
	// See if this object exports the IOSCSIProtocolInterface
	fProtocolInterface = OSDynamicCast ( IOSCSIProtocolInterface, provider );
	if ( fProtocolInterface == NULL )
	{
		
		ERROR_LOG ( ( "Provider not IOSCSIProtocolInterface\n" ) );
		
		// This provider doesn't have the interface we need, so walk the
		// parents until we get one which does (usually only one object)
		
		iterator = provider->getParentIterator ( gIOServicePlane );
		if ( iterator != NULL )
		{
			
			STATUS_LOG ( ( "Got parent iterator\n" ) );
			
			while ( object = iterator->getNextObject ( ) )
			{
				
				// Is it the interface we want?
				fProtocolInterface = OSDynamicCast ( IOSCSIProtocolInterface,
													 ( IOService * ) object );
				
				STATUS_LOG ( ( "%s candidate IOSCSIProtocolInterface\n", ( ( IOService * ) object )->getName ( ) ) );
				
				if ( fProtocolInterface != NULL )
				{
					
					STATUS_LOG ( ( "Found IOSCSIProtocolInterface\n" ) );
					break;
				
				}
				
			}
			
			// release the iterator
			iterator->release ( );
			
			// Did we find one?
			require_nonzero ( fProtocolInterface, GENERAL_ERR );
			
		}
		
	}
	
	STATUS_LOG ( ( "Creating command gate\n" ) );
	fCommandGate = IOCommandGate::commandGate ( this );
	require_nonzero ( fCommandGate, GENERAL_ERR );
	
	workLoop = getWorkLoop ( );
	require_nonzero_action ( workLoop,
							 GENERAL_ERR,
							 fCommandGate->release ( ) );
	
	workLoop->addEventSource ( fCommandGate );
	
	result = fProvider->open ( this, kIOSCSITaskUserClientAccessMask, 0 );
	require_action ( result,
					 GENERAL_ERR,
					 workLoop->removeEventSource ( fCommandGate );
					 fCommandGate->release ( );
					 fCommandGate = NULL );
	
	fWorkLoop = workLoop;
	
	
GENERAL_ERR:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ clientClose - Close down services.							[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::clientClose ( void )
{
	
	require_nonzero ( fProvider, GENERAL_ERR );
	HandleTerminate ( fProvider );
	
	
GENERAL_ERR:
	
	
	return super::clientClose ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ free - Releases any items we need to release.					[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
SCSITaskUserClient::free ( void )
{
	
	// Remove the command gate from the workloop
	if ( fWorkLoop != NULL )
	{
		
		fWorkLoop->removeEventSource ( fCommandGate );
		fWorkLoop = NULL;
		
	}
	
	// Release the command gate
	if ( fCommandGate != NULL )
	{
		
		fCommandGate->release ( );
		fCommandGate = NULL;
		
	}
	
	super::free ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ IsExclusiveAccessAvailable - 	Determines if exclusive acces is available
//									for this client.				[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::IsExclusiveAccessAvailable ( void )
{
	
	IOReturn	status = kIOReturnNoDevice;
	bool		state = true;
	
	STATUS_LOG ( ( "IsExclusiveAccessAvailable called\n" ) );
	
	require ( isInactive ( ) == false, GENERAL_ERR );
	
	// First get the state. If there is no exclusive client, then
	// we attempt to set the state (which may fail but shouldn't
	// under normal circumstances).
	state = fProtocolInterface->GetUserClientExclusivityState ( );
	require_action ( ( state == false ), GENERAL_ERR, status = kIOReturnExclusiveAccess );
	
	STATUS_LOG ( ( "No current exclusive client\n" ) );
		
	// Ok. There is no exclusive client.
	status = kIOReturnSuccess;
	
	
GENERAL_ERR:
	
	
	STATUS_LOG ( ( "IsExclusiveAccessAvailable: status = %d\n", status ) );
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ObtainExclusiveAccess - 	Obtains exclusive access for this client.
//																	[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::ObtainExclusiveAccess ( void )
{
	
	IOReturn	status	= kIOReturnNoDevice;
	bool		state 	= true;
	
	STATUS_LOG ( ( "ObtainExclusiveAccess called\n" ) );
	
	require ( isInactive ( ) == false, GENERAL_ERR );
	
	// First get the state. If there is no exclusive client, then
	// we attempt to set the state (which may fail but shouldn't
	// under normal circumstances).
	state = fProtocolInterface->GetUserClientExclusivityState ( );
	require_action ( ( state == false ), GENERAL_ERR, status = kIOReturnExclusiveAccess );
		
	STATUS_LOG ( ( "No current exclusive client\n" ) );
	
	// Ok. There is no exclusive client. Try to become the
	// exclusive client. This would only fail if two clients
	// are competing for the same exclusive access at the exact
	// same time.
	status = fProtocolInterface->SetUserClientExclusivityState ( this, true );
	require_success ( status, GENERAL_ERR );
	
	if ( fProtocolInterface->IsPowerManagementIntialized ( ) )
	{
		
		UInt32	minutes = 0;
		
		// Make sure the idle timer for the in-kernel driver is set to
		// never change power state.
		fProtocolInterface->getAggressiveness ( kPMMinutesToSpinDown, &minutes );
		fProtocolInterface->setAggressiveness ( kPMMinutesToSpinDown, minutes );
		
	}
	
	
GENERAL_ERR:
	
	
	STATUS_LOG ( ( "ObtainExclusiveAccess: status = %d\n", status ) );
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReleaseExclusiveAccess - 	Releases exclusive access for this client.
//																	[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::ReleaseExclusiveAccess ( void )
{
	
	IOReturn	status	= kIOReturnNoDevice;
	bool		state	= true;
	
	STATUS_LOG ( ( "ReleaseExclusiveAccess called\n" ) );
	
	require ( isInactive ( ) == false, GENERAL_ERR );
	
	// Get the user client state. We don't need to release exclusive
	// access if we don't have exclusive access (just to prevent
	// faulty user-land code from doing something bad).
	state = fProtocolInterface->GetUserClientExclusivityState ( );
	require_action_quiet ( state, GENERAL_ERR, status = kIOReturnNotPermitted );
	
	STATUS_LOG ( ( "There is a current exclusive client\n" ) );
	
	// Release our exclusive connection.
	status = fProtocolInterface->SetUserClientExclusivityState ( this, false );
	require_success ( status, GENERAL_ERR );
	
	if ( fProtocolInterface->IsPowerManagementIntialized ( ) )
	{
		
		UInt32	minutes = 0;
		
		// Make sure the idle timer for the in-kernel driver is set to
		// what the setting is now.
		fProtocolInterface->getAggressiveness ( kPMMinutesToSpinDown, &minutes );
		fProtocolInterface->setAggressiveness ( kPMMinutesToSpinDown, minutes );
	
	}
	
	
GENERAL_ERR:
	
	
	STATUS_LOG ( ( "ReleaseExclusiveAccess: status = %d\n", status ) );
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ CreateTask - 	Creates a SCSITask.								   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::CreateTask ( SInt32 * taskReference )
{
	
	IOReturn			status 	= kIOReturnNoDevice;
    SInt32				taskRef	= -1;
    SCSITask *			task	= NULL;
	SCSITaskRefCon *	refCon	= NULL;
	
	STATUS_LOG ( ( "CreateTask called\n" ) );
	
	require ( isInactive ( ) == false, GENERAL_ERR );
	
	check ( taskReference );
	
	task = new SCSITask;
	require_nonzero_action_string ( task,
									ALLOCATION_FAILED_ERR,
									status = kIOReturnNoResources,
									"task == NULL, memory allocation failed\n" );
	
	// Initialize the task.
	require_string ( task->ResetForNewTask ( ), INIT_FAILED_ERR,
					 "task->ResetForNewTask ( ) == false, memory allocation failed\n" );
	
	refCon = IONew ( SCSITaskRefCon, 1 );
	require_nonzero_string ( refCon, INIT_FAILED_ERR,
							 "refCon == NULL, memory allocation failed\n" );
	
	bzero ( refCon, sizeof ( SCSITaskRefCon ) );
	task->SetApplicationLayerReference ( refCon );
	
	// Run the action to add it to the array of tasks
	status = fCommandGate->runAction ( ( IOCommandGate::Action ) &SCSITaskUserClient::sCreateTask,
									   ( void * ) task,
									   ( void * ) &taskRef );
	
	require_success ( status, ACTION_ERR );
	
	STATUS_LOG ( ( "taskRef = %ld\n", taskRef ) );
	
	*taskReference = taskRef;
		
	return status;
	
	
ACTION_ERR:	
	
	
	require_nonzero ( refCon, INIT_FAILED_ERR );
	IODelete ( refCon, SCSITaskRefCon, 1 );
	refCon = NULL;
	
	
INIT_FAILED_ERR:
	
	
	require_nonzero ( task, ALLOCATION_FAILED_ERR );
	task->release ( );
	task = NULL;
	
	
ALLOCATION_FAILED_ERR:
GENERAL_ERR:
	
		
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReleaseTask - Releases a SCSITask.							[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::ReleaseTask ( SInt32 taskReference )
{
	
	IOReturn				status 	= kIOReturnBadArgument;
	SCSITask *				task	= NULL;
	IOMemoryDescriptor *	buffer	= NULL;
	SCSITaskRefCon *		refCon	= NULL;
	
	STATUS_LOG ( ( "SCSITaskUserClient::ReleaseTask\n" ) );
	
	require ( ( taskReference >= 0 ) && ( taskReference < kMaxSCSITaskArraySize ),
			  GENERAL_ERR );
	
	status = fCommandGate->runAction ( ( IOCommandGate::Action ) &SCSITaskUserClient::sReleaseTask,
									   ( void * ) taskReference,
									   ( void * ) &task );
	
	require_success ( status, GENERAL_ERR );
	
	refCon = ( SCSITaskRefCon * ) task->GetApplicationLayerReference ( );
	if ( refCon != NULL )
	{
		
		buffer = refCon->taskResultsBuffer;
		if ( buffer != NULL )
		{
			
			buffer->release ( );
			
		}
		
		IODelete ( refCon, SCSITaskRefCon, 1 );
		task->SetApplicationLayerReference ( NULL );
		
	}
	
	task->release ( );
	
	
GENERAL_ERR:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ExecuteTask - Executes a task passed in from user space.		[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::ExecuteTask ( SCSITaskData * args, UInt32 argSize )
{
	
	SCSITask *				request				= NULL;
	SCSITaskRefCon *		refCon				= NULL;
	IOReturn				status				= kIOReturnBadArgument;
	bool					userBufPrepared 	= false;
	bool					resultsBufPrepared 	= false;
	bool					senseBufPrepared 	= false;
	IOMemoryDescriptor *	buffer				= NULL;
	
	STATUS_LOG ( ( "SCSITaskUserClient::ExecuteTask called\n" ) );
	STATUS_LOG ( ( "argSize = %ld\n", argSize ) );
	
	fOutstandingCommands++;
	
	require_action ( isInactive ( ) == false, GENERAL_ERR, status = kIOReturnNoDevice );
	
	check ( args );
	
	require ( ( args->taskReference >= 0 ) && ( args->taskReference < kMaxSCSITaskArraySize ),
			  GENERAL_ERR );
	
	request = fArray[args->taskReference];
	require_nonzero ( request, GENERAL_ERR );
	
	nrequire_action ( request->IsTaskActive ( ), GENERAL_ERR, status = kIOReturnNotPermitted );
	
	refCon = ( SCSITaskRefCon * ) request->GetApplicationLayerReference ( );
	
	refCon->commandType = args->isSync ? kCommandTypeExecuteSync : kCommandTypeExecuteAsync;
	refCon->self		= this;
	
	request->ResetForNewTask ( );
	
	request->SetApplicationLayerReference ( ( void * ) refCon );
	
	status = fCommandGate->runAction ( ( IOCommandGate::Action ) &SCSITaskUserClient::sValidateTask,
									   ( void * ) request,
									   ( void * ) args,
									   ( void * ) argSize );
	
	require_success ( status, ACTION_FAILED_ERR );
	
	STATUS_LOG ( ( "Task is valid\n" ) );
	
	request->SetTimeoutDuration ( args->timeoutDuration );
	
	if ( ( args->scatterGatherEntries > 0 ) && ( args->requestedTransferCount > 0 ) )
	{
		
		IODirection		ioDirection;
		
		STATUS_LOG ( ( "Preparing buffers\n" ) );
		
		ioDirection = ( args->transferDirection == kSCSIDataTransfer_FromTargetToInitiator ) ? kIODirectionIn : kIODirectionOut;
		
		buffer = IOMemoryDescriptor::withRanges ( args->scatterGatherList,
												  args->scatterGatherEntries,
												  ioDirection,
												  fTask );
		
		require_nonzero_action_string ( buffer,
										BUFFER_CREATE_FAILED_ERR,
										status = kIOReturnNoResources,
										"Error creating memory descriptor\n" );
		
		status = buffer->prepare ( );
		require_success_string ( status,
								 BUFFER_PREPARE_FAILED_ERR,
								 "Error preparing user memory descriptor\n" );
		
		userBufPrepared = true;
		
		request->SetDataBuffer ( buffer );
		request->SetRequestedDataTransferCount ( args->requestedTransferCount );
		
	}
	
	status = refCon->taskResultsBuffer->prepare ( );
	require_success_string ( status,
							 BUFFER_PREPARE_FAILED_ERR,
							 "Error preparing results memory descriptor\n" );
	
	resultsBufPrepared = true;
	
	status = request->GetAutosenseDataBuffer ( )->prepare ( );
	require_success_string ( status,
							 BUFFER_PREPARE_FAILED_ERR,
							 "Error preparing sense data memory descriptor\n" );
	
	senseBufPrepared = true;
	
	// Retain the task, results buffer, and sense data buffer. They will be released by sTaskCallback method.
	request->retain ( );
	refCon->taskResultsBuffer->retain ( );
	
	request->SetTaskCompletionCallback ( &SCSITaskUserClient::sTaskCallback );
	request->SetAutosenseCommand ( kSCSICmd_REQUEST_SENSE, 0x00, 0x00, 0x00, sizeof ( SCSI_Sense_Data ), 0x00 );
	fProtocolInterface->ExecuteCommand ( request );
	
	if ( refCon->commandType == kCommandTypeExecuteSync )
	{
		
		retain ( );
		
		status = fCommandGate->runAction ( 	( IOCommandGate::Action ) &SCSITaskUserClient::sWaitForTask,
									   		( void * ) request );
		
		if ( buffer != NULL )
		{
			
			// Make sure to complete any data buffers from client
			status = CompleteBuffers ( buffer );
			
		}
		
		release ( );
		
	}
	
	
	return status;
	
	
BUFFER_PREPARE_FAILED_ERR:
	
	
	if ( userBufPrepared )
		CompleteBuffers ( buffer );
		
	if ( resultsBufPrepared )
		refCon->taskResultsBuffer->complete ( );
		
	if ( senseBufPrepared )
		request->GetAutosenseDataBuffer ( )->complete ( );
	
	
BUFFER_CREATE_FAILED_ERR:	
ACTION_FAILED_ERR:
GENERAL_ERR:
	
	
	fOutstandingCommands--;
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AbortTask - Aborts a task passed in from user space (if possible).
//																	[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::AbortTask ( SInt32 taskReference )
{
	
	SCSITask * 	task	= NULL;
	IOReturn	status	= kIOReturnBadArgument;
	
	STATUS_LOG ( ( "SCSITaskUserClient::AbortTask called\n" ) );

	require_action ( isInactive ( ) == false, GENERAL_ERR, status = kIOReturnNoDevice );	
	require ( ( taskReference >= 0 ) && ( taskReference < kMaxSCSITaskArraySize ), GENERAL_ERR );
	
	task = fArray[taskReference];
	require_nonzero ( task, GENERAL_ERR );
	
	// Can't abort an inactive task
	require_action ( task->IsTaskActive ( ), GENERAL_ERR, status = kIOReturnNotPermitted );
	
	// Make sure we can send the command
	require_nonzero_action ( fProtocolInterface, GENERAL_ERR, status = kIOReturnNoDevice );
	
	fProtocolInterface->AbortCommand ( task );
	
	
GENERAL_ERR:
	
	
	return kIOReturnSuccess;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetAsyncCallback - 	Sets an async callback reference so a method is
//							called for asynchronous notifications.
//																	[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::SetAsyncCallback ( OSAsyncReference	asyncRef,
									   SInt32			taskReference,
									   void *			callback,
									   void *			userRefCon )
{
	
	SCSITask *			task		= NULL;
	mach_port_t			wakePort 	= MACH_PORT_NULL;
	SCSITaskRefCon *	refCon		= NULL;
	IOReturn			status		= kIOReturnBadArgument;
	
	check ( callback );
	check ( userRefCon );
	
	STATUS_LOG ( ( "SCSITaskUserClient::SetAsyncCallback called\n" ) );
	
	require_action ( isInactive ( ) == false, GENERAL_ERR, status = kIOReturnNoDevice );
	require ( ( taskReference >= 0 ) && ( taskReference < kMaxSCSITaskArraySize ), GENERAL_ERR );
	
	task = fArray[taskReference];
	require_nonzero ( task, GENERAL_ERR );
	
	// Can't touch an active task
	nrequire_action ( task->IsTaskActive ( ), GENERAL_ERR, status = kIOReturnNotPermitted );
	
	STATUS_LOG ( ( "asyncRef[0] = %d\n", asyncRef[0] ) );
	
	wakePort = ( mach_port_t ) asyncRef[0];
	
	super::setAsyncReference ( asyncRef, wakePort, callback, userRefCon );
	
	refCon = ( SCSITaskRefCon * ) task->GetApplicationLayerReference ( );
	require_nonzero_action ( refCon, GENERAL_ERR, status = kIOReturnError );
	
	bcopy ( asyncRef, refCon->asyncReference, kOSAsyncRefSize );
	
	STATUS_LOG ( ( "refCon->asyncReference[0] = %d\n", refCon->asyncReference[0] ) );
	status = kIOReturnSuccess;
	
	
GENERAL_ERR:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetBuffers - 	Sets the results buffer and sense data buffer for the
//					specified task.									[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::SetBuffers ( SInt32			taskReference,
								 vm_address_t	results,
								 vm_address_t	senseData,
								 UInt32			senseBufferSize )
{
	
	IOReturn				status	= kIOReturnBadArgument;
	SCSITask *				task	= NULL;
	SCSITaskRefCon *		refCon	= NULL;
	IOMemoryDescriptor *	buffer	= NULL;
	bool					result	= false;
	
	STATUS_LOG ( ( "SCSITaskUserClient::SetBuffers called\n" ) );
	
	require_action ( isInactive ( ) == false, GENERAL_ERR, status = kIOReturnNoDevice );	
	require ( ( taskReference >= 0 ), GENERAL_ERR );
	require ( ( taskReference < kMaxSCSITaskArraySize ), GENERAL_ERR );
	
	task = fArray[taskReference];
	require ( task, GENERAL_ERR );
	
	// Can't touch an active task
	require_action ( ( task->IsTaskActive ( ) == false ),
					 GENERAL_ERR,
					 status = kIOReturnNotPermitted );
	
	refCon = ( SCSITaskRefCon * ) task->GetApplicationLayerReference ( );
	require_nonzero_action ( refCon, GENERAL_ERR, status = kIOReturnError );
	
	buffer = IOMemoryDescriptor::withAddress ( results,
											   sizeof ( SCSITaskResults ),
											   kIODirectionIn,
											   fTask );
	
	require_nonzero_action ( buffer, GENERAL_ERR, status = kIOReturnNoResources );
	
	if ( refCon->taskResultsBuffer != NULL )
	{
		
		refCon->taskResultsBuffer->release ( );
		
	}
	
	refCon->taskResultsBuffer = buffer;
	
	result = task->SetAutoSenseDataBuffer ( ( SCSI_Sense_Data * ) senseData,
											senseBufferSize,
											fTask );
	
	require_action ( result, GENERAL_ERR, status = kIOReturnNoResources );
	
	status = kIOReturnSuccess;
	
	
GENERAL_ERR:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ Inquiry - Issues an INQUIRY command to the drive as defined by SPC-2.	
//																	[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::Inquiry ( AppleInquiryStruct * 	inquiryData,
							  SCSITaskStatus * 		taskStatus,
							  UInt32				inStructSize,
							  UInt32 *				outStructSize )
{
	
	IOReturn				status 	= kIOReturnExclusiveAccess;
	IOReturn				status2 = kIOReturnExclusiveAccess;
	SCSITask * 				task 	= NULL;
	IOMemoryDescriptor *	buffer	= NULL;
	bool					state 	= true;
	
	check ( inquiryData );
	check ( taskStatus );
	check ( outStructSize );
	
	*outStructSize = 0;
	
	fOutstandingCommands++;
	
	require_action ( isInactive ( ) == false, GENERAL_ERR, status = kIOReturnNoDevice );
	
	state = fProtocolInterface->GetUserClientExclusivityState ( );
	nrequire ( state, EXCLUSIVE_ACCESS_ERR );
	
	// Create and initialize a task
	status = SetupTask ( &task );
	require_success ( status, TASK_SETUP_ERR );
	
	// Prepare the buffers
	status = PrepareBuffers ( &buffer, inquiryData->buffer, inquiryData->bufferSize, kIODirectionIn );
	require_success ( status, BUFFER_PREPARATION_ERR );
	
	// Prepare the task
	task->SetCommandDescriptorBlock ( kSCSICmd_INQUIRY,
									  0x00,
									  0x00,
									  0x00,
									  inquiryData->bufferSize,
									  0x00 );
	
	task->SetTimeoutDuration ( kTenSecondTimeoutInMS );
	task->SetDataTransferDirection ( kSCSIDataTransfer_FromTargetToInitiator );
	task->SetDataBuffer ( buffer );
	task->SetRequestedDataTransferCount ( inquiryData->bufferSize );
	
	status	= SendCommand ( task, inquiryData->senseDataBuffer, taskStatus );
	status2	= CompleteBuffers ( buffer );
	
	if ( ( status == kIOReturnSuccess ) && ( status2 != kIOReturnSuccess ) )
	{
		status = status2;
	}
	
	*outStructSize = sizeof ( SCSITaskStatus );
	
	task->release ( );
	
	return status;
	
	
BUFFER_PREPARATION_ERR:
	
	
	task->release ( );
	
	
TASK_SETUP_ERR:
EXCLUSIVE_ACCESS_ERR:
GENERAL_ERR:
	
	
	fOutstandingCommands--;
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ TestUnitReady - 	Issues a TEST_UNIT_READY command to the drive as
//						defined by SPC-2.							[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::TestUnitReady ( vm_address_t 		senseDataBuffer,
									SCSITaskStatus * 	taskStatus,
									UInt32 *			outStructSize )
{
	
	IOReturn				status 	= kIOReturnExclusiveAccess;
	SCSITask * 				task 	= NULL;
	bool					state 	= true;
	
	check ( taskStatus );
	check ( outStructSize );
	
	*outStructSize = 0;
	
	fOutstandingCommands++;
	
	require_action ( isInactive ( ) == false, GENERAL_ERR, status = kIOReturnNoDevice );
	
	state = fProtocolInterface->GetUserClientExclusivityState ( );
	nrequire ( state, EXCLUSIVE_ACCESS_ERR );
	
	// Create and initialize a task
	status = SetupTask ( &task );
	require_success ( status, TASK_SETUP_ERR );
	
	task->SetCommandDescriptorBlock ( kSCSICmd_TEST_UNIT_READY,
									  0x00,
									  0x00,
									  0x00,
									  0x00,
									  0x00 );
	
	task->SetTimeoutDuration ( kTenSecondTimeoutInMS );
	task->SetDataTransferDirection ( kSCSIDataTransfer_NoDataTransfer );
	
	status = SendCommand ( task, ( void * ) senseDataBuffer, taskStatus );
	
	task->release ( );
	*outStructSize = sizeof ( SCSITaskStatus );
	
	return status;
	
	
TASK_SETUP_ERR:
EXCLUSIVE_ACCESS_ERR:
GENERAL_ERR:
	
	
	fOutstandingCommands--;
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetPerformance - 	Issues a GET_PERFORMANCE command to the drive as
//						defined by Mt. Fuji/SFF-8090i revision 5.0.	[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::GetPerformance ( AppleGetPerformanceStruct * 	performanceData,
									 SCSITaskStatus * 				taskStatus,
									 UInt32 						inStructSize,
									 UInt32 * 						outStructSize )
{
	
	IOReturn				status 	= kIOReturnExclusiveAccess;
	IOReturn				status2 = kIOReturnExclusiveAccess;
	SCSITask * 				task 	= NULL;
	IOMemoryDescriptor *	buffer	= NULL;
	bool					state 	= true;
	
	check ( performanceData );
	check ( taskStatus );
	check ( outStructSize );
	
	*outStructSize = 0;
	
	fOutstandingCommands++;
	
	require_action ( isInactive ( ) == false, GENERAL_ERR, status = kIOReturnNoDevice );
	
	state = fProtocolInterface->GetUserClientExclusivityState ( );
	nrequire ( state, EXCLUSIVE_ACCESS_ERR );
	
	// Create and initialize a task
	status = SetupTask ( &task );
	require_success ( status, TASK_SETUP_ERR );
	
	// Prepare the buffers
	status = PrepareBuffers ( &buffer, performanceData->buffer, performanceData->bufferSize, kIODirectionIn );
	require_success ( status, BUFFER_PREPARATION_ERR );	
	
	task->SetCommandDescriptorBlock ( kSCSICmd_GET_PERFORMANCE,
									  performanceData->DATA_TYPE,
									  ( performanceData->STARTING_LBA >> 24 ) 	& 0xFF,
									  ( performanceData->STARTING_LBA >> 16 ) 	& 0xFF,
									  ( performanceData->STARTING_LBA >>  8 ) 	& 0xFF,
									  performanceData->STARTING_LBA         	& 0xFF,
									  0x00,
									  0x00,
									  ( performanceData->MAXIMUM_NUMBER_OF_DESCRIPTORS >> 8 )	& 0xFF,
									    performanceData->MAXIMUM_NUMBER_OF_DESCRIPTORS			& 0xFF,
									  performanceData->TYPE,
									  0x00 );
	
	task->SetTimeoutDuration ( kThirtySecondTimeoutInMS );
	task->SetDataTransferDirection ( kSCSIDataTransfer_FromTargetToInitiator );
	task->SetDataBuffer ( buffer );
	task->SetRequestedDataTransferCount ( performanceData->bufferSize );
	
	status	= SendCommand ( task, performanceData->senseDataBuffer, taskStatus );
	status2	= CompleteBuffers ( buffer );
	
	if ( ( status == kIOReturnSuccess ) && ( status2 != kIOReturnSuccess ) )
	{
		status = status2;
	}
	
	*outStructSize = sizeof ( SCSITaskStatus );
	
	task->release ( );
	
	return status;
	
	
BUFFER_PREPARATION_ERR:
	
	
	task->release ( );
	
	
TASK_SETUP_ERR:
EXCLUSIVE_ACCESS_ERR:
GENERAL_ERR:
	
	
	fOutstandingCommands--;
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetConfiguration - 	Issues a GET_CONFIGURATION command to the drive as
//							defined by MMC-2.						[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::GetConfiguration ( AppleGetConfigurationStruct * 	configData,
									   SCSITaskStatus * 				taskStatus,
									   UInt32 							inStructSize,
									   UInt32 * 						outStructSize )
{
	
	IOReturn				status 	= kIOReturnExclusiveAccess;
	IOReturn				status2 = kIOReturnExclusiveAccess;
	SCSITask * 				task 	= NULL;
	IOMemoryDescriptor *	buffer	= NULL;
	bool					state 	= true;
	
	check ( configData );
	check ( taskStatus );
	check ( outStructSize );
	
	*outStructSize = 0;
	
	fOutstandingCommands++;
	
	require_action ( isInactive ( ) == false, GENERAL_ERR, status = kIOReturnNoDevice );
	
	state = fProtocolInterface->GetUserClientExclusivityState ( );
	nrequire ( state, EXCLUSIVE_ACCESS_ERR );
	
	// Create and initialize a task
	status = SetupTask ( &task );
	require_success ( status, TASK_SETUP_ERR );
	
	// Prepare the buffers
	status = PrepareBuffers ( &buffer, configData->buffer, configData->bufferSize, kIODirectionIn );
	require_success ( status, BUFFER_PREPARATION_ERR );
	
	task->SetCommandDescriptorBlock ( 	kSCSICmd_GET_CONFIGURATION,
										configData->RT,
										( configData->STARTING_FEATURE_NUMBER >> 8 ) & 0xFF,
										  configData->STARTING_FEATURE_NUMBER        & 0xFF,
										0x00,
										0x00,
										0x00,
										( configData->bufferSize >> 8 ) 	& 0xFF,
										  configData->bufferSize        	& 0xFF,
										0x00 );
	
	task->SetTimeoutDuration ( kThirtySecondTimeoutInMS );
	task->SetDataTransferDirection ( kSCSIDataTransfer_FromTargetToInitiator );
	task->SetDataBuffer ( buffer );
	task->SetRequestedDataTransferCount ( configData->bufferSize );
	
	status	= SendCommand ( task, configData->senseDataBuffer, taskStatus );
	status2	= CompleteBuffers ( buffer );
	
	if ( ( status == kIOReturnSuccess ) && ( status2 != kIOReturnSuccess ) )
	{
		status = status2;
	}
	
	*outStructSize = sizeof ( SCSITaskStatus );
	
	task->release ( );
	
	return status;
	
	
BUFFER_PREPARATION_ERR:
	
	
	task->release ( );
	
	
TASK_SETUP_ERR:
EXCLUSIVE_ACCESS_ERR:
GENERAL_ERR:
	
	
	fOutstandingCommands--;
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ModeSense10 - Issues a MODE_SENSE_10 command to the drive as
//					defined by SPC-2.								[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::ModeSense10 ( AppleModeSense10Struct * 	modeSenseData,
								  SCSITaskStatus * 			taskStatus,
								  UInt32 					inStructSize,
								  UInt32 * 					outStructSize )
{
	
	IOReturn				status 	= kIOReturnExclusiveAccess;
	IOReturn				status2 = kIOReturnExclusiveAccess;
	SCSITask * 				task 	= NULL;
	IOMemoryDescriptor *	buffer	= NULL;
	bool					state 	= true;
	
	check ( modeSenseData );
	check ( taskStatus );
	check ( outStructSize );
	
	*outStructSize = 0;
	
	fOutstandingCommands++;
	
	require_action ( isInactive ( ) == false, GENERAL_ERR, status = kIOReturnNoDevice );
	
	state = fProtocolInterface->GetUserClientExclusivityState ( );
	nrequire ( state, EXCLUSIVE_ACCESS_ERR );
	
	// Create and initialize a task
	status = SetupTask ( &task );
	require_success ( status, TASK_SETUP_ERR );
	
	// Prepare the buffers
	status = PrepareBuffers ( &buffer, modeSenseData->buffer, modeSenseData->bufferSize, kIODirectionIn );
	require_success ( status, BUFFER_PREPARATION_ERR );
	
	task->SetCommandDescriptorBlock ( kSCSICmd_MODE_SENSE_10,
									  ( modeSenseData->LLBAA << 4 ) | ( modeSenseData->DBD << 3 ),
									  ( modeSenseData->PC << 6 ) | modeSenseData->PAGE_CODE,
									  0x00,
									  0x00,
									  0x00,
									  0x00,
									  ( modeSenseData->bufferSize >> 8 & 0xFF ),
									  ( modeSenseData->bufferSize & 0xFF ),
									  0x00 );
	
	task->SetTimeoutDuration ( kThirtySecondTimeoutInMS );
	task->SetDataTransferDirection ( kSCSIDataTransfer_FromTargetToInitiator );
	task->SetDataBuffer ( buffer );
	task->SetRequestedDataTransferCount ( modeSenseData->bufferSize );
	
	status	= SendCommand ( task, modeSenseData->senseDataBuffer, taskStatus );	
	status2	= CompleteBuffers ( buffer );
	
	if ( ( status == kIOReturnSuccess ) && ( status2 != kIOReturnSuccess ) )
	{
		status = status2;
	}
	
	*outStructSize = sizeof ( SCSITaskStatus );
	
	task->release ( );
	
	return status;
	
	
BUFFER_PREPARATION_ERR:
	
	
	task->release ( );
	
	
TASK_SETUP_ERR:
EXCLUSIVE_ACCESS_ERR:
GENERAL_ERR:
	
	
	fOutstandingCommands--;
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetWriteParametersModePage - 	Issues a MODE_SELECT_10 command to the drive
//									with the Write Parameters Mode Page set as
//									defined by MMC-2.				[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::SetWriteParametersModePage ( AppleWriteParametersModePageStruct * paramsData,
												 SCSITaskStatus * 	taskStatus,
												 UInt32 			inStructSize,
												 UInt32 * 			outStructSize )
{
	
	IOReturn				status 	= kIOReturnExclusiveAccess;
	IOReturn				status2 = kIOReturnExclusiveAccess;
	SCSITask * 				task 	= NULL;
	IOMemoryDescriptor *	buffer	= NULL;
	bool					state 	= true;
	SCSICmdField1Bit		PF 		= 1;

	check ( paramsData );
	check ( taskStatus );
	check ( outStructSize );
	
	*outStructSize = 0;
	
	fOutstandingCommands++;
	
	require_action ( isInactive ( ) == false, GENERAL_ERR, status = kIOReturnNoDevice );
	
	state = fProtocolInterface->GetUserClientExclusivityState ( );
	nrequire ( state, EXCLUSIVE_ACCESS_ERR );
	
	// Create and initialize a task
	status = SetupTask ( &task );
	require_success ( status, TASK_SETUP_ERR );
	
	// Prepare the buffers
	status = PrepareBuffers ( &buffer, paramsData->buffer, paramsData->bufferSize, kIODirectionOut );
	require_success ( status, BUFFER_PREPARATION_ERR );
	
	task->SetCommandDescriptorBlock (	kSCSICmd_MODE_SELECT_10,
										( PF << 4),
										0x00,
										0x00,
										0x00,
										0x00,
										0x00,
										( paramsData->bufferSize >> 8 ) & 0xFF,
										  paramsData->bufferSize 		& 0xFF,
										0x00 );
	
	task->SetTimeoutDuration ( kThirtySecondTimeoutInMS );
	task->SetDataTransferDirection ( kSCSIDataTransfer_FromInitiatorToTarget );
	task->SetDataBuffer ( buffer );
	task->SetRequestedDataTransferCount ( paramsData->bufferSize );
	
	status	= SendCommand ( task, paramsData->senseDataBuffer, taskStatus );
	status2	= CompleteBuffers ( buffer );
	
	if ( ( status == kIOReturnSuccess ) && ( status2 != kIOReturnSuccess ) )
	{
		status = status2;
	}
	
	*outStructSize = sizeof ( SCSITaskStatus );
	
	task->release ( );
	
	return status;
	
	
BUFFER_PREPARATION_ERR:
	
	
	task->release ( );
	
	
TASK_SETUP_ERR:
EXCLUSIVE_ACCESS_ERR:
GENERAL_ERR:
	
	
	fOutstandingCommands--;
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetTrayState - Returns the current tray state of the drive.	[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::GetTrayState ( UInt32 * trayState )
{
	
	IOReturn		status 			= kIOReturnExclusiveAccess;
	UInt8			actualTrayState	= 0;
	bool			state			= false;
	
	fOutstandingCommands++;
	
	require_action ( isInactive ( ) == false, GENERAL_ERR, status = kIOReturnNoDevice );
	
	state = fProtocolInterface->GetUserClientExclusivityState ( );
	nrequire ( state, EXCLUSIVE_ACCESS_ERR );
	
	status = ( ( IOSCSIMultimediaCommandsDevice * ) fProtocolInterface )->GetTrayState ( &actualTrayState );
	require_success ( status, COMMAND_FAILED_ERR );
	*trayState = actualTrayState;
	
	
COMMAND_FAILED_ERR:
EXCLUSIVE_ACCESS_ERR:
GENERAL_ERR:
	
	
	fOutstandingCommands--;
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetTrayState - Sets the driveÕs tray state to the proposed state.
//																	[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::SetTrayState ( UInt32 trayState )
{
	
	IOReturn		status 				= kIOReturnExclusiveAccess;
	UInt8			desiredTrayState	= 0;
	bool			state				= false;
	
	fOutstandingCommands++;
	
	require_action ( isInactive ( ) == false, GENERAL_ERR, status = kIOReturnNoDevice );
	
	state = fProtocolInterface->GetUserClientExclusivityState ( );
	nrequire ( state, EXCLUSIVE_ACCESS_ERR );
	
	desiredTrayState = trayState & kMMCDeviceTrayMask;
	status = ( ( IOSCSIMultimediaCommandsDevice * ) fProtocolInterface )->SetTrayState ( desiredTrayState );
	
	
EXCLUSIVE_ACCESS_ERR:
GENERAL_ERR:
	
	
	fOutstandingCommands--;
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReadTableOfContents - Issues a READ_TOC_PMA_ATIP command to the drive as
//							defined in MMC-2.						[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::ReadTableOfContents ( AppleReadTableOfContentsStruct * 	readTOCData,
										  SCSITaskStatus * 					taskStatus,
										  UInt32 							inStructSize,
										  UInt32 * 							outStructSize )
{
	
	IOReturn				status 	= kIOReturnExclusiveAccess;
	IOReturn				status2	= kIOReturnExclusiveAccess;
	SCSITask * 				task 	= NULL;
	IOMemoryDescriptor *	buffer	= NULL;
	bool					state 	= true;
	
	check ( readTOCData );
	check ( taskStatus );
	check ( outStructSize );
	
	fOutstandingCommands++;
	
	require_action ( isInactive ( ) == false, GENERAL_ERR, status = kIOReturnNoDevice );
	
	state = fProtocolInterface->GetUserClientExclusivityState ( );
	nrequire ( state, EXCLUSIVE_ACCESS_ERR );
	
	// Create and initialize a task
	status = SetupTask ( &task );
	require_success ( status, TASK_SETUP_ERR );
	
	// Prepare the buffers
	status = PrepareBuffers ( &buffer, readTOCData->buffer, readTOCData->bufferSize, kIODirectionIn );
	require_success ( status, BUFFER_PREPARATION_ERR );
	
	if ( readTOCData->FORMAT & 0x04 )
	{
		
		// Use new style from MMC-2
		task->SetCommandDescriptorBlock ( kSCSICmd_READ_TOC_PMA_ATIP,
										  readTOCData->MSF << 1,
										  readTOCData->FORMAT,
										  0x00,
										  0x00,
										  0x00,
										  readTOCData->TRACK_SESSION_NUMBER,
										  ( readTOCData->bufferSize >> 8 ) & 0xFF,
										    readTOCData->bufferSize        & 0xFF,
										  0x00 );

	
	}
	
	else
	{

		// Use old style from SFF-8020i
		task->SetCommandDescriptorBlock ( kSCSICmd_READ_TOC_PMA_ATIP,
										  readTOCData->MSF << 1,
										  0x00,
										  0x00,
										  0x00,
										  0x00,
										  readTOCData->TRACK_SESSION_NUMBER,
										  ( readTOCData->bufferSize >> 8 ) & 0xFF,
										    readTOCData->bufferSize        & 0xFF,
										  ( readTOCData->FORMAT & 0x03 ) << 6 );
		
	}
	
	// set timeout to 30 seconds
	task->SetTimeoutDuration ( kThirtySecondTimeoutInMS );
	task->SetDataTransferDirection ( kSCSIDataTransfer_FromTargetToInitiator );
	task->SetDataBuffer ( buffer );
	task->SetRequestedDataTransferCount ( readTOCData->bufferSize );
	
	status	= SendCommand ( task, readTOCData->senseDataBuffer, taskStatus );
	status2 = CompleteBuffers ( buffer );
	
	if ( ( status == kIOReturnSuccess ) && ( status2 != kIOReturnSuccess ) )
	{
		status = status2;
	}
	
	*outStructSize = sizeof ( SCSITaskStatus );
	
	task->release ( );
	
	return status;
	
	
BUFFER_PREPARATION_ERR:
	
	
	task->release ( );
	
	
TASK_SETUP_ERR:
EXCLUSIVE_ACCESS_ERR:
GENERAL_ERR:
	
	
	fOutstandingCommands--;
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReadDiscInformation - Issues a READ_DISC_INFORMATION command to the
//							drive as defined in MMC-2.				[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::ReadDiscInformation ( AppleReadDiscInfoStruct * 	discInfoData,
										  SCSITaskStatus * 				taskStatus,
										  UInt32 						inStructSize,
										  UInt32 * 						outStructSize )
{
	
	IOReturn				status 	= kIOReturnExclusiveAccess;
	IOReturn				status2	= kIOReturnExclusiveAccess;
	SCSITask * 				task 	= NULL;
	IOMemoryDescriptor *	buffer	= NULL;
	bool					state 	= true;
		
	check ( discInfoData );
	check ( taskStatus );
	check ( outStructSize );
	
	fOutstandingCommands++;
	
	require_action ( isInactive ( ) == false, GENERAL_ERR, status = kIOReturnNoDevice );
	
	state = fProtocolInterface->GetUserClientExclusivityState ( );
	nrequire ( state, EXCLUSIVE_ACCESS_ERR );
	
	// Create and initialize a task
	status = SetupTask ( &task );
	require_success ( status, TASK_SETUP_ERR );
	
	// Prepare the buffers
	status = PrepareBuffers ( &buffer, discInfoData->buffer, discInfoData->bufferSize, kIODirectionIn );
	require_success ( status, BUFFER_PREPARATION_ERR );
	
	task->SetCommandDescriptorBlock ( kSCSICmd_READ_DISC_INFORMATION,
									  0x00,
									  0x00,
									  0x00,
									  0x00,
									  0x00,
									  0x00,
									  ( discInfoData->bufferSize >> 8 ) & 0xFF,
									    discInfoData->bufferSize        & 0xFF,
									  0x00 );
	
	// set timeout to 30 seconds
	task->SetTimeoutDuration ( kThirtySecondTimeoutInMS );
	task->SetDataTransferDirection ( kSCSIDataTransfer_FromTargetToInitiator );
	task->SetDataBuffer ( buffer );
	task->SetRequestedDataTransferCount ( discInfoData->bufferSize );
	
	status 	= SendCommand ( task, discInfoData->senseDataBuffer, taskStatus );
	status2 = CompleteBuffers ( buffer );
	
	if ( ( status == kIOReturnSuccess ) && ( status2 != kIOReturnSuccess ) )
	{
		status = status2;
	}
	
	*outStructSize = sizeof ( SCSITaskStatus );
	
	task->release ( );
	
	return status;
	
	
BUFFER_PREPARATION_ERR:
	
	
	task->release ( );
	
	
TASK_SETUP_ERR:
EXCLUSIVE_ACCESS_ERR:
GENERAL_ERR:
	
	
	fOutstandingCommands--;
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReadTrackInformation - 	Issues a READ_TRACK/RZONE_INFORMATION command
//								to the drive as defined in MMC-2.	[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::ReadTrackInformation ( AppleReadTrackInfoStruct * 	trackInfoData,
										   SCSITaskStatus * 			taskStatus,
										   UInt32 						inStructSize,
										   UInt32 * 					outStructSize )
{
	
	IOReturn				status 	= kIOReturnExclusiveAccess;
	IOReturn				status2 = kIOReturnExclusiveAccess;
	SCSITask * 				task 	= NULL;
	IOMemoryDescriptor *	buffer	= NULL;
	bool					state 	= true;
	
	check ( trackInfoData );
	check ( taskStatus );
	check ( outStructSize );
	
	fOutstandingCommands++;
	
	require_action ( isInactive ( ) == false, GENERAL_ERR, status = kIOReturnNoDevice );
	
	state = fProtocolInterface->GetUserClientExclusivityState ( );
	nrequire ( state, EXCLUSIVE_ACCESS_ERR );
	
	// Create and initialize a task
	status = SetupTask ( &task );
	require_success ( status, TASK_SETUP_ERR );
	
	// Prepare the buffers
	status = PrepareBuffers ( &buffer, trackInfoData->buffer, trackInfoData->bufferSize, kIODirectionIn );
	require_success ( status, BUFFER_PREPARATION_ERR );
	
	task->SetCommandDescriptorBlock ( kSCSICmd_READ_TRACK_INFORMATION,
									  trackInfoData->ADDRESS_NUMBER_TYPE & 0x03,
									  ( trackInfoData->LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER >> 24 ) & 0xFF,
									  ( trackInfoData->LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER >> 16 ) & 0xFF,
									  ( trackInfoData->LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER >>  8 ) & 0xFF,
								 	    trackInfoData->LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER         & 0xFF,
									  0x00,
									  ( trackInfoData->bufferSize >>  8 ) & 0xFF,
								  	    trackInfoData->bufferSize         & 0xFF,
									  0x00 );
	
	// set timeout to 30 seconds
	task->SetTimeoutDuration ( kThirtySecondTimeoutInMS );
	task->SetDataTransferDirection ( kSCSIDataTransfer_FromTargetToInitiator );
	task->SetDataBuffer ( buffer );
	task->SetRequestedDataTransferCount ( trackInfoData->bufferSize );
	
	status	= SendCommand ( task, trackInfoData->senseDataBuffer, taskStatus );
	status2	= CompleteBuffers ( buffer );
	
	if ( ( status == kIOReturnSuccess ) && ( status2 != kIOReturnSuccess ) )
	{
		status = status2;
	}
	
	*outStructSize = sizeof ( SCSITaskStatus );
	
	task->release ( );
	
	return status;
	
	
BUFFER_PREPARATION_ERR:
	
	
	task->release ( );
	
	
TASK_SETUP_ERR:
EXCLUSIVE_ACCESS_ERR:
GENERAL_ERR:
	
	
	fOutstandingCommands--;
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReadDVDStructure - 	Issues a READ_DVD_STRUCTURE command to the drive as
//							defined in MMC-2.						[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::ReadDVDStructure ( AppleReadDVDStructureStruct * 	dvdStructData,
									   SCSITaskStatus * 				taskStatus,
									   UInt32 							inStructSize,
									   UInt32 * 						outStructSize )
{
	
	IOReturn				status 	= kIOReturnExclusiveAccess;
	IOReturn				status2 = kIOReturnExclusiveAccess;
	SCSITask * 				task 	= NULL;
	IOMemoryDescriptor *	buffer	= NULL;
	bool					state 	= true;
	
	check ( dvdStructData );
	check ( taskStatus );
	check ( outStructSize );
	
	fOutstandingCommands++;
	
	require_action ( isInactive ( ) == false, GENERAL_ERR, status = kIOReturnNoDevice );
	
	state = fProtocolInterface->GetUserClientExclusivityState ( );
	nrequire ( state, EXCLUSIVE_ACCESS_ERR );
	
	// Create and initialize a task
	status = SetupTask ( &task );
	require_success ( status, TASK_SETUP_ERR );
	
	// Prepare the buffers
	status = PrepareBuffers ( &buffer, dvdStructData->buffer, dvdStructData->bufferSize, kIODirectionIn );
	require_success ( status, BUFFER_PREPARATION_ERR );
	
	task->SetCommandDescriptorBlock ( kSCSICmd_READ_DVD_STRUCTURE,
									  0x00,
									  ( dvdStructData->ADDRESS >> 24 ) & 0xFF,
									  ( dvdStructData->ADDRESS >> 16 ) & 0xFF,
									  ( dvdStructData->ADDRESS >> 8  ) & 0xFF,
									    dvdStructData->ADDRESS		   & 0xFF,
									    dvdStructData->LAYER_NUMBER,
									    dvdStructData->FORMAT,
									  ( dvdStructData->bufferSize >> 8 ) & 0xFF,
									    dvdStructData->bufferSize        & 0xFF,
									  ( dvdStructData->AGID << 6 ),
									  0x00 );
	
	// set timeout to 30 seconds
	task->SetTimeoutDuration ( kThirtySecondTimeoutInMS );
	task->SetDataTransferDirection ( kSCSIDataTransfer_FromTargetToInitiator );
	task->SetDataBuffer ( buffer );
	task->SetRequestedDataTransferCount ( dvdStructData->bufferSize );
	
	status	= SendCommand ( task, dvdStructData->senseDataBuffer, taskStatus );
	status2	= CompleteBuffers ( buffer );
	
	if ( ( status == kIOReturnSuccess ) && ( status2 != kIOReturnSuccess ) )
	{
		status = status2;
	}
	
	*outStructSize = sizeof ( SCSITaskStatus );
	
	task->release ( );
	
	return status;
	
	
BUFFER_PREPARATION_ERR:
	
	
	task->release ( );
	
	
TASK_SETUP_ERR:
EXCLUSIVE_ACCESS_ERR:
GENERAL_ERR:
	
	
	fOutstandingCommands--;
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetCDSpeed - Called to set the new CD read/write speeds.
//																	[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::SetCDSpeed ( AppleSetCDSpeedStruct *	setCDSpeedData,
								 SCSITaskStatus * 			taskStatus,
								 UInt32 					inStructSize,
								 UInt32 * 					outStructSize )
{
	
	IOReturn				status			= kIOReturnExclusiveAccess;
	SCSITask * 				task			= NULL;
	bool					state			= true;
	UInt16					readSpeed		= 0;
	UInt16					writeSpeed		= 0;
	
	
	check ( setCDSpeedData );
	check ( taskStatus );
	check ( outStructSize );
	
	fOutstandingCommands++;
	
	require_action ( isInactive ( ) == false, GENERAL_ERR, status = kIOReturnNoDevice );
	
	state = fProtocolInterface->GetUserClientExclusivityState ( );
	nrequire ( state, EXCLUSIVE_ACCESS_ERR );
	
	// Create and initialize a task
	status = SetupTask ( &task );
	require_success ( status, TASK_SETUP_ERR );
	
	readSpeed	= setCDSpeedData->LOGICAL_UNIT_READ_SPEED;
	writeSpeed	= setCDSpeedData->LOGICAL_UNIT_WRITE_SPEED;
	
	task->SetCommandDescriptorBlock ( kSCSICmd_SET_CD_SPEED,
									  0x00,
									  ( readSpeed >>   8 )	& 0xFF,
									  readSpeed				& 0xFF,
									  ( writeSpeed >>  8 )	& 0xFF,
									  writeSpeed			& 0xFF,
									  0x00,
									  0x00,
									  0x00,
									  0x00,
									  0x00,
									  0x00 );
	
	task->SetTimeoutDuration ( kThirtySecondTimeoutInMS );
	task->SetDataTransferDirection ( kSCSIDataTransfer_NoDataTransfer );
	
	status	= SendCommand ( task,
							setCDSpeedData->senseDataBuffer,
							taskStatus );
	
	*outStructSize = sizeof ( SCSITaskStatus );
	
	task->release ( );
	
	return status;
	
	
TASK_SETUP_ERR:
EXCLUSIVE_ACCESS_ERR:
GENERAL_ERR:
	
	
	fOutstandingCommands--;
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReadFormatCapacities - Issues a READ_FORMAT_CAPACITIES command to the
//							 drive as defined in MMC-2.				[PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::ReadFormatCapacities ( AppleReadFormatCapacitiesStruct *	readFormatCapacitiesData,
										   SCSITaskStatus * 					taskStatus,
										   UInt32 								inStructSize,
										   UInt32 * 							outStructSize )
{
	
	IOReturn				status			= kIOReturnExclusiveAccess;
	IOReturn				status2			= kIOReturnExclusiveAccess;
	bool					state			= true;
	SCSITask * 				task			= NULL;
	IOMemoryDescriptor *	buffer			= NULL;
	
	
	check ( readFormatCapacitiesData );
	check ( taskStatus );
	check ( outStructSize );
	
	fOutstandingCommands++;
	
	require_action ( isInactive ( ) == false, GENERAL_ERR, status = kIOReturnNoDevice );
	
	state = fProtocolInterface->GetUserClientExclusivityState ( );
	nrequire ( state, EXCLUSIVE_ACCESS_ERR );
	
	// Create and initialize a task
	status = SetupTask ( &task );
	require_success ( status, TASK_SETUP_ERR );
	
	// Prepare the buffers
	status = PrepareBuffers ( &buffer, readFormatCapacitiesData->buffer, readFormatCapacitiesData->bufferSize, kIODirectionIn );
	require_success ( status, BUFFER_PREPARATION_ERR );
	
	task->SetCommandDescriptorBlock ( kSCSICmd_READ_FORMAT_CAPACITIES,
									  0x00,
									  0x00,
									  0x00,
									  0x00,
									  0x00,
									  0x00,
									  ( readFormatCapacitiesData->bufferSize >> 8 ) & 0xFF,
									    readFormatCapacitiesData->bufferSize        & 0xFF,
									  0 );
	
	task->SetTimeoutDuration ( kThirtySecondTimeoutInMS );
	task->SetDataTransferDirection ( kSCSIDataTransfer_FromTargetToInitiator );
	task->SetDataBuffer ( buffer );
	task->SetRequestedDataTransferCount ( readFormatCapacitiesData->bufferSize );
	
	status	= SendCommand ( task, readFormatCapacitiesData->senseDataBuffer, taskStatus );
	
	status2	= CompleteBuffers ( buffer );
	
	if ( ( status == kIOReturnSuccess ) && ( status2 != kIOReturnSuccess ) )
	{
		status = status2;
	}
	
	*outStructSize = sizeof ( SCSITaskStatus );
	
	task->release ( );
	
	return status;
	
	
BUFFER_PREPARATION_ERR:
	
	
	task->release ( );
	
	
TASK_SETUP_ERR:
EXCLUSIVE_ACCESS_ERR:
GENERAL_ERR:
	
	fOutstandingCommands--;
	
	return status;
	
}


#if 0
#pragma mark -
#pragma mark Protected Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ getTargetAndMethodForIndex - 	Returns a pointer to the target of the
//									method call and the method vector itself
//									based on the provided index.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOExternalMethod *
SCSITaskUserClient::getTargetAndMethodForIndex (
							IOService **	target,
							UInt32			index )
{
	
	IOExternalMethod *	method = NULL;
	
	check ( target );
	require ( index < kSCSITaskUserClientMethodCount, GENERAL_ERR );
	
	fOutstandingCommands++;
	
	require ( isInactive ( ) == false, DECREMENT_COUNTER );
	
	fProtocolInterface->retain ( );
	
	if ( fProtocolInterface->IsPowerManagementIntialized ( ) )
	{
		fProtocolInterface->CheckPowerState ( );
	}
	
	fProtocolInterface->release ( );
	
	*target = this;
	method 	= &sMethods[index];
	
	
DECREMENT_COUNTER:
	
	
	fOutstandingCommands--;
	
	
GENERAL_ERR:
	
	
	return method;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ getAsyncTargetAndMethodForIndex - Returns a pointer to the target of the
//										async method call and the method
//										vector itself based on the provided
//										index.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOExternalAsyncMethod *
SCSITaskUserClient::getAsyncTargetAndMethodForIndex (
							IOService **	target,
							UInt32			index )
{
	
	
	IOExternalAsyncMethod *	method = NULL;
	
	check ( target );
	require ( index < kSCSITaskUserClientAsyncMethodCount, GENERAL_ERR );
	
	fOutstandingCommands++;
	
	require ( isInactive ( ) == false, DECREMENT_COUNTER );
	
	fProtocolInterface->retain ( );
	
	if ( fProtocolInterface->IsPowerManagementIntialized ( ) )
	{
		fProtocolInterface->CheckPowerState ( );
	}
	
	fProtocolInterface->release ( );
	
	*target = this;
	method 	= &sAsyncMethods[index];
	
	
DECREMENT_COUNTER:
	
	
	fOutstandingCommands--;
	
	
GENERAL_ERR:
	
	
	return method;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GatedCreateTask -	Safely obtains a taskReference for the newly created
//						task if one is available. It is called while holding
//						the workloop lock.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::GatedCreateTask ( SCSITask * task, SInt32 * taskReference )
{
	
	IOReturn		status 	= kIOReturnNoResources;
	UInt32			index	= 0;
	
	check ( task );
	check ( taskReference );
	
	for ( index = 0; index < kMaxSCSITaskArraySize; index++ )
	{
		
		if ( fArray[index] == NULL )
			break;
		
	}
	
	require_action ( index < kMaxSCSITaskArraySize, ARRAY_INDEX_ERR, *taskReference = -1 );
	
	fArray[index]	= task;
	*taskReference 	= index;
	status			= kIOReturnSuccess;
	
	
ARRAY_INDEX_ERR:	
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GatedReleaseTask -	Safely obtains a taskReference for the newly
//							created task if one is available. It is called
//							while holding the workloop lock.		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::GatedReleaseTask ( SInt32 taskReference, SCSITask ** task )
{
	
	IOReturn	status 	= kIOReturnSuccess;
	SCSITask *	victim	= NULL;
	
	check ( task != NULL );
	
	// Sanity check
	victim = fArray[taskReference];
	require_nonzero_action ( victim, GENERAL_ERR, status = kIOReturnBadArgument );
	
	// If the task is still active, it cannot be released.
	require_action ( ( victim->IsTaskActive ( ) == false ), GENERAL_ERR, status = kIOReturnNotPermitted );
	
	// Remove it now.
	fArray[taskReference] = 0;
	
	STATUS_LOG ( ( "Removed object from array\n" ) );
	
	// Pass back the pointer to the object so it can be safely destroyed.
	*task = victim;	
	
	
GENERAL_ERR:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GatedValidateTask -	Safely validates a task. It is called while
//							holding the workloop lock.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::GatedValidateTask ( SCSITask * 		request,
										SCSITaskData * 	args,
										UInt32 			argSize )
{
	
	IOReturn	status = kIOReturnBadArgument;
	
	STATUS_LOG ( ( "SCSITaskUserClient::GatedValidateTask called\n" ) );
	
	check ( request );
	check ( args );
	
	require ( ( args->taskAttribute >= kSCSITask_SIMPLE ) &&
			  ( args->taskAttribute <= kSCSITask_ACA ),
			  INVALID_ARGUMENT );
	
	request->SetTaskAttribute ( args->taskAttribute );
	
	#if ( SCSI_TASK_USER_CLIENT_DEBUGGING_LEVEL >= 3 )
	{
		
		SCSICommandDescriptorBlock	cdb;
		UInt8						commandLength;
		
		request->GetCommandDescriptorBlock ( &cdb );
		commandLength = request->GetCommandDescriptorBlockSize ( );
		
		if ( commandLength == kSCSICDBSize_6Byte )
		{
			
			STATUS_LOG ( ( "cdb = %02x:%02x:%02x:%02x:%02x:%02x\n", cdb[0], cdb[1],
						cdb[2], cdb[3], cdb[4], cdb[5] ) );
			
		}
		
		else if ( commandLength == kSCSICDBSize_10Byte )
		{
			
			STATUS_LOG ( ( "cdb = %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n", cdb[0],
						cdb[1], cdb[2], cdb[3], cdb[4], cdb[5], cdb[6], cdb[7], cdb[8],
						cdb[9] ) );
			
		}
		
		else if ( commandLength == kSCSICDBSize_12Byte )
		{
			
			STATUS_LOG ( ( "cdb = %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n", cdb[0],
						cdb[1], cdb[2], cdb[3], cdb[4], cdb[5], cdb[6], cdb[7], cdb[8],
						cdb[9], cdb[10], cdb[11] ) );
			
		}
		
		else if ( commandLength == kSCSICDBSize_16Byte )
		{
			
			STATUS_LOG ( ( "cdb = %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n", cdb[0],
						cdb[1], cdb[2], cdb[3], cdb[4], cdb[5], cdb[6], cdb[7], cdb[8],
						cdb[9], cdb[10], cdb[11], cdb[12], cdb[13], cdb[14], cdb[15] ) );
			
		}
		
	}
	#endif /* ( SCSI_TASK_USER_CLIENT_DEBUGGING_LEVEL >= 3 ) */
	
	switch ( args->cdbSize )
	{
		
		case kSCSICDBSize_6Byte:
			request->SetCommandDescriptorBlock ( args->cdbData[0],
												 args->cdbData[1],
												 args->cdbData[2],
												 args->cdbData[3],
												 args->cdbData[4],
												 args->cdbData[5] );
			break;
		
		case kSCSICDBSize_10Byte:
			request->SetCommandDescriptorBlock ( args->cdbData[0],
												 args->cdbData[1],
												 args->cdbData[2],
												 args->cdbData[3],
												 args->cdbData[4],
												 args->cdbData[5],
												 args->cdbData[6],
												 args->cdbData[7],
												 args->cdbData[8],
												 args->cdbData[9] );
			break;
		
		case kSCSICDBSize_12Byte:
			request->SetCommandDescriptorBlock ( args->cdbData[0],
												 args->cdbData[1],
												 args->cdbData[2],
												 args->cdbData[3],
												 args->cdbData[4],
												 args->cdbData[5],
												 args->cdbData[6],
												 args->cdbData[7],
												 args->cdbData[8],
												 args->cdbData[9],
												 args->cdbData[10],
												 args->cdbData[11] );
			break;
		
		case kSCSICDBSize_16Byte:
			request->SetCommandDescriptorBlock ( args->cdbData[0],
												 args->cdbData[1],
												 args->cdbData[2],
												 args->cdbData[3],
												 args->cdbData[4],
												 args->cdbData[5],
												 args->cdbData[6],
												 args->cdbData[7],
												 args->cdbData[8],
												 args->cdbData[9],
												 args->cdbData[10],
												 args->cdbData[11],
												 args->cdbData[12],
												 args->cdbData[13],
												 args->cdbData[14],
												 args->cdbData[15] );
			break;
		
		default:
			ERROR_LOG ( ( "Invalid command length size\n" ) );
			goto INVALID_ARGUMENT;
		
	}
	
	require ( ( args->transferDirection <= kSCSIDataTransfer_FromTargetToInitiator ),
			  INVALID_ARGUMENT );
	
	request->SetDataTransferDirection ( args->transferDirection );
	
	if ( args->scatterGatherEntries > 0 )
	{
		
		UInt32		structSizeWithoutList = sizeof ( SCSITaskData ) - sizeof ( IOVirtualRange );
		UInt32		sgListSize = 0;
		
		sgListSize = argSize - structSizeWithoutList;
		
		require_string ( ( sgListSize / sizeof ( IOVirtualRange ) ) == args->scatterGatherEntries,
						 INVALID_ARGUMENT,
						 "Invalid scatter-gather list" );
		
	}
	
	status = kIOReturnSuccess;
	
	
INVALID_ARGUMENT:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GatedWaitForTask -	Waits for signal to wake up. It must hold the
//							workloop lock in order to call commandSleep()
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::GatedWaitForTask ( SCSITask * request )
{
	
	IOReturn			status	= kIOReturnSuccess;
	SCSITaskRefCon *	refCon	= NULL;
	
	check ( request );
	refCon = ( SCSITaskRefCon * ) request->GetApplicationLayerReference ( );
	check ( refCon );
	
	while ( request->GetTaskState ( ) != kSCSITaskState_ENDED )
	{
		
		status = fCommandGate->commandSleep ( &refCon->commandType, THREAD_UNINT );
		
	}
	
	status = kIOReturnSuccess;
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ didTerminate - Checks to see if termination should be deferred.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
SCSITaskUserClient::didTerminate ( IOService * 		provider,
								   IOOptionBits		options,
								   bool *			defer )
{
	
	if ( fOutstandingCommands == 0 )
	{
		HandleTerminate ( provider );
	}
	
	return true;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ HandleTerminate -	Handles terminating our object and any resources it
//						allocated.									[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::HandleTerminate ( IOService * provider )
{
	
	IOReturn	status = kIOReturnSuccess;
	
	check ( provider );
	
	// Check if we have our provider open.
	if ( provider->isOpen ( this ) )
	{
		
		// Yes we do, so close the connection
		STATUS_LOG ( ( "Closing provider\n" ) );
		provider->close ( this, kIOSCSITaskUserClientAccessMask );
		
	}
	
	// Decouple us from the IORegistry.
	detach ( provider );
	fProvider = NULL;
	
	// Clean up work.
	
	// 1) Release exclusive access to the device
	ReleaseExclusiveAccess ( );
	
	// 2) Release any tasks not cleaned up by the userspace code.
	for ( UInt32 index = 0; index < kMaxSCSITaskArraySize; index++ )
	{
		
		SCSITask *	task = NULL;
		
		task = ( SCSITask * ) fArray[index];
		if ( task == NULL )
			continue;
		
		ReleaseTask ( index );
		
	}
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SendCommand -	Sends a command to the hardware synchronously. This is a
//					helper method for all our non-exclusive methods.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::SendCommand ( SCSITask * 		request,
								  void * 			senseBuffer,
								  SCSITaskStatus *	taskStatus )
{
	
	SCSITaskRefCon			refCon	= { 0 };
	IOReturn				status	= kIOReturnNoResources;
	bool					result  = false;
	
	check ( request );
	check ( senseBuffer );
	check ( taskStatus );
	
	*taskStatus = kSCSITaskStatus_No_Status;
	
	refCon.commandType 	= kCommandTypeNonExclusive;
	refCon.self			= this;
	
	request->SetTaskCompletionCallback ( &SCSITaskUserClient::sTaskCallback );
	request->SetApplicationLayerReference ( ( void * ) &refCon );
	request->SetAutosenseCommand ( kSCSICmd_REQUEST_SENSE,
								   0x00,
								   0x00,
								   0x00,
								   sizeof ( SCSI_Sense_Data ),
								   0x00 );
	
	result = request->SetAutoSenseDataBuffer (
								( SCSI_Sense_Data * ) senseBuffer,
								sizeof ( SCSI_Sense_Data ),
								fTask );
	
	require ( result, ErrorExit );
	
	status = request->GetAutosenseDataBuffer ( )->prepare ( );
	require_success ( status, ErrorExit );
	
	// Retain the task. It will be released by sTaskCallback method.
	request->retain ( );
	
	fProtocolInterface->ExecuteCommand ( request );
	fCommandGate->runAction ( ( IOCommandGate::Action ) &SCSITaskUserClient::sWaitForTask,
							  ( void * ) request );
	
	*taskStatus = request->GetTaskStatus ( );
	
	if ( request->GetServiceResponse ( ) == kSCSIServiceResponse_TASK_COMPLETE )
		status = kIOReturnSuccess;
	else
		status = kIOReturnIOError;
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ TaskCallback - 	This method is called by sTaskCallback as the
//						completion routine for all SCSITasks.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
SCSITaskUserClient::TaskCallback ( SCSITask * task, SCSITaskRefCon * refCon )
{
	
	IOMemoryDescriptor *	buffer = NULL;
	
	STATUS_LOG ( ( "SCSITaskUserClient::TaskCallback called.\n") );
	
	check ( task );
	check ( refCon );
	
	buffer = refCon->taskResultsBuffer;	
	if ( buffer != NULL )
	{
		
		SCSITaskResults		results;
		UInt32				numBytes = 0;
		
		results.serviceResponse			= task->GetServiceResponse ( );
		results.taskStatus				= task->GetTaskStatus ( );
		results.realizedTransferCount	= task->GetRealizedDataTransferCount ( );
		
		numBytes = sizeof ( SCSITaskResults );
		
		buffer->writeBytes ( 0, ( void * ) &results, numBytes );
		buffer->complete ( );
		
		// Make sure to release since it was retained by ExecuteTask
		buffer->release ( );
		buffer = NULL;
		
	}
	
	buffer = task->GetAutosenseDataBuffer ( );
	if ( buffer != NULL )
	{
		buffer->complete ( );
	}
	
	// Release the task as it was retained in ExecuteTask or SendCommand
	task->release ( );
	
	if ( refCon->commandType == kCommandTypeNonExclusive )
	{
		
		fOutstandingCommands--;
		fCommandGate->commandWakeup ( &refCon->commandType );
		
	}
	
	else if ( refCon->commandType == kCommandTypeExecuteSync )
	{
		
		// We've executed the task, so decrement the count now.
		fOutstandingCommands--;
		fCommandGate->commandWakeup ( &refCon->commandType );
		
	}
		
	else
	{
		
		OSAsyncReference	asyncRef;
		
		bcopy ( refCon->asyncReference, asyncRef, sizeof ( OSAsyncReference ) );
		
		STATUS_LOG ( ( "asyncRef[0] = %d\n", asyncRef[0] ) );
		
		buffer = task->GetDataBuffer ( );
		if ( buffer != NULL )
		{
			
			// Make sure to complete any data buffers from client
			CompleteBuffers ( buffer );
			
		}
		
		// Send the result
        ( void ) sendAsyncResult ( asyncRef, kIOReturnSuccess, NULL, 0 );
		
		// We've executed asynchronously, so decrement the count now.
		fOutstandingCommands--;
		
	}
	
	if ( isInactive ( ) && ( fOutstandingCommands == 0 ) )
		HandleTerminate ( fProvider );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetupTask - 	Creates and initializes a new SCSITask.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::SetupTask ( SCSITask ** task )
{
	
	SCSITask *		newTask = NULL;
	IOReturn		status	= kIOReturnNoMemory;
	
	check ( task );
	
	newTask = OSTypeAlloc ( SCSITask );
	require_nonzero ( newTask, TASK_CREATE_ERR );
	require_action ( newTask->ResetForNewTask ( ),
					 TASK_CREATE_ERR,
					 newTask->release ( ) );
	
	newTask->SetTaskOwner ( this );
	
	*task 	= newTask;
	status	= kIOReturnSuccess;
	
	
TASK_CREATE_ERR:
	
	
	return status;	
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ PrepareBuffers - 	Prepares any user space buffers.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::PrepareBuffers ( IOMemoryDescriptor **	buffer,
									 void *					userBuffer,
									 IOByteCount			bufferSize,
									 IODirection			direction )
{
	
	IOReturn	status = kIOReturnNoMemory;
	
	check ( buffer );
	check ( userBuffer );
	
	*buffer = IOMemoryDescriptor::withAddress ( ( vm_address_t ) userBuffer,
											    bufferSize,
											    direction,
											    fTask );
	
	require_nonzero ( *buffer, BUFFER_CREATE_ERR );
	
	status = ( *buffer )->prepare ( );
	if ( status != kIOReturnSuccess )
	{
		
		( *buffer )->release ( );
		( *buffer ) = NULL;
		
	}
	
	
BUFFER_CREATE_ERR:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ CompleteBuffers - Completes any user space buffers.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::CompleteBuffers ( IOMemoryDescriptor * buffer )
{
	
	IOReturn	status = kIOReturnSuccess;
	
	check ( buffer );
	
	buffer->complete ( );
	buffer->release ( );
	
	return status;
	
}


#if 0
#pragma mark -
#pragma mark Static Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ sCreateTask - Called by runAction and holds the workloop lock.
//																	[STATIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::sCreateTask ( void *		self,
								  SCSITask *	task,
								  SInt32 *		taskReference )
{
	
	check ( self );
	return ( ( SCSITaskUserClient * ) self )->GatedCreateTask ( task, taskReference );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ sReleaseTask - Called by runAction and holds the workloop lock.
//																	[STATIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::sReleaseTask ( void * self, SInt32 taskReference, void * task )
{
	
	check ( self );
	return ( ( SCSITaskUserClient * ) self )->GatedReleaseTask ( taskReference,
																 ( SCSITask ** ) task );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ sWaitForTask - Called by runAction and holds the workloop lock.
//																	[STATIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::sWaitForTask ( void *		userClient,
								   SCSITask *	request )
{
	
	check ( userClient );
	return ( ( SCSITaskUserClient * ) userClient )->GatedWaitForTask ( request );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ sValidateTask - Called by runAction and holds the workloop lock.
//																	[STATIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
SCSITaskUserClient::sValidateTask ( void *			userClient,
									SCSITask *		request,
									SCSITaskData *	args,
									UInt32			argSize )
{
	
	check ( userClient );
	return ( ( SCSITaskUserClient * ) userClient )->GatedValidateTask ( request, args, argSize );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ sTaskCallback - 	Static completion routine. Calls TaskCallback. It holds
//						the workloop lock as well since it is on the completion
//						chain from the controller.
//																	[STATIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
SCSITaskUserClient::sTaskCallback ( SCSITaskIdentifier completedTask )
{
	
	SCSITask *				task	= NULL;
	SCSITaskRefCon *		refCon	= NULL;
	SCSITaskUserClient *	uc		= NULL;
	
	STATUS_LOG ( ( "SCSITaskUserClient::sTaskCallback called.\n") );
	
	task = OSDynamicCast ( SCSITask, completedTask );
	require_nonzero ( task, GENERAL_ERR );
	
	refCon = ( SCSITaskRefCon * ) task->GetApplicationLayerReference ( );
	require_nonzero ( refCon, GENERAL_ERR );
	
	uc = refCon->self;
	require_nonzero ( uc, GENERAL_ERR );
	
	uc->TaskCallback ( task, refCon );
	
	
GENERAL_ERR:
	
	
	return;
	
}