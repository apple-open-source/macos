/*
 * Copyright (c) 2001 Apple Computer, Inc. All rights reserved.
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

#include "SCSITaskUserClient.h"
#include <IOKit/scsi-commands/IOSCSIPrimaryCommandsDevice.h>
#include <IOKit/scsi-commands/IOSCSIMultimediaCommandsDevice.h>
#include <IOKit/scsi-commands/SCSICmds_REQUEST_SENSE_Defs.h>
#include <IOKit/scsi-commands/SCSICmds_INQUIRY_Definitions.h>
#include <IOKit/scsi-commands/SCSICommandOperationCodes.h>
#include <IOKit/IOSyncer.h>
#include <IOKit/storage/IOBlockStorageDriver.h>

// For debugging, set SCSI_TASK_USER_CLIENT_DEBUGGING_LEVEL to one
// of the following values:
//		0	No debugging 	(GM release level)
// 		1 	PANIC_NOW only
//		2	PANIC_NOW and ERROR_LOG
//		3	PANIC_NOW, ERROR_LOG and STATUS_LOG
#define SCSI_TASK_USER_CLIENT_DEBUGGING_LEVEL 0

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
		// Method #6 ExecuteTaskAsync
		0,
		( IOMethod ) &SCSITaskUserClient::ExecuteTaskAsync,
		kIOUCScalarIScalarO,
		1,
		0
	},
	{
		// Method #7 ExecuteTaskSync
		0,
		( IOMethod ) &SCSITaskUserClient::ExecuteTaskSync,
		kIOUCScalarIScalarO,
		2,
		3
	},
	{
		// Method #8 IsTaskActive
		0,
		( IOMethod ) &SCSITaskUserClient::IsTaskActive,
		kIOUCScalarIScalarO,
		1,
		1
	},
	{
		// Method #9 SetTransferDirection
		0,
		( IOMethod ) &SCSITaskUserClient::SetTransferDirection,
		kIOUCScalarIScalarO,
		2,
		0
	},
	{
		// Method #10 SetTaskAttribute
		0,
		( IOMethod ) &SCSITaskUserClient::SetTaskAttribute,
		kIOUCScalarIScalarO,
		2,
		0
	},
	{
		// Method #11 GetTaskAttribute
		0,
		( IOMethod ) &SCSITaskUserClient::GetTaskAttribute,
		kIOUCScalarIScalarO,
		1,
		1
	},
	{
		// Method #12 SetCommandDescriptorBlock
		0,
		( IOMethod ) &SCSITaskUserClient::SetCommandDescriptorBlock,
		kIOUCScalarIStructI,
		2,
		sizeof ( SCSICommandDescriptorBlock )
	},
	{
		// Method #13 SetScatterGatherList
		0,
		( IOMethod ) &SCSITaskUserClient::SetScatterGatherList,
		kIOUCScalarIStructI,
		5,
		0xFFFFFFFF
	},
	{
		// Method #14 SetSenseDataBuffer
		0,
		( IOMethod ) &SCSITaskUserClient::SetSenseDataBuffer,
		kIOUCScalarIScalarO,
		3,
		0
	},
	{
		// Method #15 SetTimeoutDuration
		0,
		( IOMethod ) &SCSITaskUserClient::SetTimeoutDuration,
		kIOUCScalarIScalarO,
		2,
		0
	},
	{
		// Method #16 GetTimeoutDuration
		0,
		( IOMethod ) &SCSITaskUserClient::GetTimeoutDuration,
		kIOUCScalarIScalarO,
		1,
		1
	},
	{
		// Method #17 GetTaskStatus
		0,
		( IOMethod ) &SCSITaskUserClient::GetTaskStatus,
		kIOUCScalarIScalarO,
		1,
		1
	},
	{
		// Method #18 GetSCSIServiceResponse
		0,
		( IOMethod ) &SCSITaskUserClient::GetSCSIServiceResponse,
		kIOUCScalarIScalarO,
		1,
		1
	},
	{
		// Method #19 GetTaskState
		0,
		( IOMethod ) &SCSITaskUserClient::GetTaskState,
		kIOUCScalarIScalarO,
		1,
		1
	},
	{
		// Method #20 GetAutoSenseData
		0,
		( IOMethod ) &SCSITaskUserClient::GetAutoSenseData,
		kIOUCScalarIStructO,
		1,
		sizeof ( SCSI_Sense_Data )
	},
	{
		// Method #21 Inquiry
		0,
		( IOMethod ) &SCSITaskUserClient::Inquiry,
		kIOUCScalarIScalarO,
		3,
		1
	},
	{
		// Method #22 TestUnitReady
		0,
		( IOMethod ) &SCSITaskUserClient::TestUnitReady,
		kIOUCScalarIScalarO,
		1,
		1
	},
	{
		// Method #23 GetPerformance
		0,
		( IOMethod ) &SCSITaskUserClient::GetPerformance,
		kIOUCScalarIScalarO,
		5,
		1
	},
	{
		// Method #24 GetConfiguration
		0,
		( IOMethod ) &SCSITaskUserClient::GetConfiguration,
		kIOUCScalarIScalarO,
		5,
		1
	},
	{
		// Method #25 ModeSense10
		0,
		( IOMethod ) &SCSITaskUserClient::ModeSense10,
		kIOUCScalarIScalarO,
		5,
		1
	},
	{
		// Method #26 SetWriteParametersModePage
		0,
		( IOMethod ) &SCSITaskUserClient::SetWriteParametersModePage,
		kIOUCScalarIScalarO,
		3,
		1
	},
	{
		// Method #27 GetTrayState
		0,
		( IOMethod ) &SCSITaskUserClient::GetTrayState,
		kIOUCScalarIScalarO,
		0,
		1
	},
	{
		// Method #28 SetTrayState
		0,
		( IOMethod ) &SCSITaskUserClient::SetTrayState,
		kIOUCScalarIScalarO,
		1,
		0
	},
	{
		// Method #29 ReadTableOfContents
		0,
		( IOMethod ) &SCSITaskUserClient::ReadTableOfContents,
		kIOUCScalarIScalarO,
		5,
		1
	},
	{
		// Method #30 ReadDiscInformation
		0,
		( IOMethod ) &SCSITaskUserClient::ReadDiscInformation,
		kIOUCScalarIScalarO,
		3,
		1
	},
	{
		// Method #31 ReadTrackInformation
		0,
		( IOMethod ) &SCSITaskUserClient::ReadTrackInformation,
		kIOUCScalarIScalarO,
		5,
		1
	},
	{
		// Method #32 ReadDVDStructure
		0,
		( IOMethod ) &SCSITaskUserClient::ReadDVDStructure,
		kIOUCScalarIScalarO,
		5,
		1
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


bool
SCSITaskUserClient::init ( OSDictionary * dictionary )
{
	
	STATUS_LOG ( ( "SCSITaskUserClient::init\n" ) );
	
	if ( !IOService::init ( dictionary ) )
		return false;
	
	fTask 				= NULL;
	fProvider 			= NULL;
	fProtocolInterface 	= NULL;
	fSetOfSCSITasks		= NULL;
	
	return true;
	
}


bool
SCSITaskUserClient::start ( IOService * provider )
{
	
	STATUS_LOG ( ( "SCSITaskUserClient::start\n" ) );
	
	OSIterator *	iterator;
	OSObject *		object;
	
	if ( fProvider != NULL )
		return false;

	if ( !IOUserClient::start ( provider ) )
         return false;

	STATUS_LOG ( ( "assigning fProvider\n" ) );
	
	fProvider = provider;
	
	fProtocolInterface = OSDynamicCast ( IOSCSIProtocolInterface, provider );
	if ( fProtocolInterface == NULL )
	{

		STATUS_LOG ( ( "provider not IOSCSIProtocolInterface\n" ) );
		
		// This provider doesn't have the interface we need, so walk the
		// parents until we get one which does (usually only one object)
		
		iterator = provider->getParentIterator ( gIOServicePlane );
		if ( iterator != NULL )
		{
			
			STATUS_LOG ( ( "got parent iterator\n" ) );
			
			while ( object = iterator->getNextObject ( ) )
			{
				
				// Is it the interface we want?
				fProtocolInterface = OSDynamicCast ( IOSCSIProtocolInterface,
													 ( IOService * ) object );
				
				STATUS_LOG ( ( "%s candidate IOSCSIProtocolInterface\n", ( ( IOService * ) object )->getName ( ) ) );
				
				if ( fProtocolInterface != NULL )
				{
					
					STATUS_LOG ( ( "found IOSCSIProtocolInterface\n" ) );
					break;
				
				}
				
			}
			
			// release the iterator
			iterator->release ( );
			
			// Did we find one?
			if ( fProtocolInterface == NULL )
			{

				STATUS_LOG ( ( "didn't find IOSCSIProtocolInterface\n" ) );
				
				// Nope, make sure we null the provider and
				// return false
				fProvider = NULL;
				return false;
				
			}
			
		}
		
	}
	
	STATUS_LOG ( ( "Opening provider\n" ) );
	
	if ( !fProvider->open ( this, kIOSCSITaskUserClientAccessMask, 0 ) )
	{
		
		STATUS_LOG ( ( "Open failed\n" ) );
		return false;
		
	}

	STATUS_LOG ( ( "start done\n" ) );
	
	fSetOfSCSITasks = OSSet::withCapacity ( 1 );
	
	// Yes, we found an object to use as our interface
	return true;
	
}


bool
SCSITaskUserClient::initWithTask ( task_t owningTask,
								   void * securityToken,
								   UInt32 type )
{
	
	STATUS_LOG ( ( "SCSITaskUserClient::initWithTask called\n" ) );
	
	if ( type != kSCSITaskLibConnection )
		return false;
	
	fTask = owningTask;
	return true;
	
}


IOReturn
SCSITaskUserClient::clientClose ( void )
{
	
	STATUS_LOG ( ( "clientClose called\n" ) );
	
	HandleTermination ( fProvider );
		
	STATUS_LOG ( ( "Done\n" ) );
	
	return kIOReturnSuccess;
	
}


IOReturn
SCSITaskUserClient::clientDied ( void )
{
	
	STATUS_LOG ( ( "SCSITaskUserClient::clientDied called\n" ) );
	
	if ( fProvider != NULL )
	{
		return clientClose ( );
	}
	
	return kIOReturnSuccess;
	
}


IOExternalMethod *
SCSITaskUserClient::getTargetAndMethodForIndex ( IOService ** target, UInt32 index )
{
	
	if ( index >= kSCSITaskUserClientMethodCount )
		return NULL;
	
	if ( fProtocolInterface == NULL )
		return NULL;
	
	if ( fProtocolInterface->IsPowerManagementIntialized ( ) )
	{
		fProtocolInterface->CheckPowerState ( );
	}
	
	*target = this;
	
	return &sMethods[index];
	
}



IOExternalAsyncMethod *
SCSITaskUserClient::getAsyncTargetAndMethodForIndex ( IOService ** target, UInt32 index )
{
	
	if ( index >= kSCSITaskUserClientAsyncMethodCount )
		return NULL;

	if ( fProtocolInterface == NULL )
		return NULL;
	
	if ( fProtocolInterface->IsPowerManagementIntialized ( ) )
	{
		fProtocolInterface->CheckPowerState ( );
	}
	
	*target = this;
	
	return &sAsyncMethods[index];
	
}


IOReturn
SCSITaskUserClient::IsExclusiveAccessAvailable ( void )
{
	
	IOReturn	status = kIOReturnExclusiveAccess;
	bool		state = true;

	if ( fProtocolInterface == NULL )
		return kIOReturnNoDevice;
	
	STATUS_LOG ( ( "IsExclusiveAccessAvailable called\n" ) );
	
	// First get the state. If there is no exclusive client, then
	// we attempt to set the state (which may fail but shouldn't
	// under normal circumstances).
	state = fProtocolInterface->GetUserClientExclusivityState ( );
	if ( state == false )
	{
		
		STATUS_LOG ( ( "No current exclusive client\n" ) );
		
		// Ok. There is no exclusive client.
		status = kIOReturnSuccess;
		
	}
	
	STATUS_LOG ( ( "IsExclusiveAccessAvailable: status = %d\n", status ) );
	
	return status;
	
}


IOReturn
SCSITaskUserClient::ObtainExclusiveAccess ( void )
{
	
	IOReturn	status = kIOReturnExclusiveAccess;
	bool		state = true;

	if ( fProtocolInterface == NULL )
		return kIOReturnNoDevice;
	
	STATUS_LOG ( ( "ObtainExclusiveAccess called\n" ) );
	
	// First get the state. If there is no exclusive client, then
	// we attempt to set the state (which may fail but shouldn't
	// under normal circumstances).
	state = fProtocolInterface->GetUserClientExclusivityState ( );
	if ( state == false )
	{
		
		STATUS_LOG ( ( "No current exclusive client\n" ) );
		
		// Ok. There is no exclusive client. Try to become the
		// exclusive client. This would only fail if two clients
		// are competing for the same exclusive access at the exact
		// same time.
		status = fProtocolInterface->SetUserClientExclusivityState ( this, true );
		
	}
	
	STATUS_LOG ( ( "ObtainExclusiveAccess: status = %d\n", status ) );

	return status;
	
}


IOReturn
SCSITaskUserClient::ReleaseExclusiveAccess ( void )
{
	
	IOReturn	status = kIOReturnExclusiveAccess;
	bool		state = true;
	
	STATUS_LOG ( ( "ReleaseExclusiveAccess called\n" ) );
	
	// Get the user client state. We don't need to release exclusive
	// access if we don't have exclusive access (just to prevent
	// faulty user-land code from doing something bad).
	state = fProtocolInterface->GetUserClientExclusivityState ( );
	if ( state == true )
	{
		
		STATUS_LOG ( ( "There is a current exclusive client\n" ) );
		
		// Release our exclusive connection.
		status = fProtocolInterface->SetUserClientExclusivityState ( this, false );
		
	}
	
	STATUS_LOG ( ( "ReleaseExclusiveAccess: status = %d\n", status ) );
	
	return status;
	
}

IOReturn
SCSITaskUserClient::CreateTask ( SCSITask ** outSCSITask, void *, void *, void *, void *, void * )
{
	
	IOReturn	status = kIOReturnSuccess;
    SCSITask *	task;
	UInt8 *		internalRefCon;

	if ( fProtocolInterface == NULL )
		return kIOReturnNoDevice;

	STATUS_LOG ( ( "CreateTask called\n" ) );
	
	// Creat the new task.
	task = new SCSITask;
	if ( task == NULL )
	{
		
		status = kIOReturnNoMemory;
		goto TASK_ALLOCATION_FAILURE;
		
	}
	
	STATUS_LOG ( ( "Initializing task\n" ) );
	
	// Initialize the task.
	if ( task->ResetForNewTask ( ) == false )
	{
		
		status = kIOReturnNoMemory;
		goto INIT_FAILURE;
		
	}

	STATUS_LOG ( ( "Allocating refcon storage\n" ) );
	
	// Create the async refcon and set it in the task.
	internalRefCon = ( UInt8 * ) IOMalloc ( sizeof ( OSAsyncReference ) );
	if ( internalRefCon == NULL )
	{
		
		status = kIOReturnNoMemory;
		goto INIT_FAILURE;
		
	}
	
	task->SetApplicationLayerReference ( internalRefCon );
	*outSCSITask = task;
	
	// Add the task to the OSSet.
	fSetOfSCSITasks->setObject ( task );
	
	return status;
	
	
INIT_FAILURE:
	
	if ( task != NULL )
	{
		task->release ( );
	}
	
	
TASK_ALLOCATION_FAILURE:
	
	return status;
	
}


IOReturn
SCSITaskUserClient::ReleaseTask ( SCSITask * inSCSITask, void *, void *, void *, void *, void * )
{
	
	IOMemoryDescriptor *	buffer;
	SCSITask * 				task = OSDynamicCast ( SCSITask, inSCSITask );
	
	STATUS_LOG ( ( "SCSITaskUserClient::ReleaseTask\n" ) );
	
	if ( task != NULL )
	{
		
		if ( task->IsTaskActive ( ) )
		{
			
			// The task is still active. It cannot be released while
			// it is still active.
			return kIOReturnNotPermitted;
			
		}
		
		fSetOfSCSITasks->removeObject ( task );
		STATUS_LOG ( ( "Removed object from OSSet\n" ) );
		
		buffer = task->GetDataBuffer ( );
		if ( buffer != NULL )
		{	
			
			buffer->release ( );
			STATUS_LOG ( ( "Released buffer\n" ) );	
		
		}
		
		task->release ( );
		STATUS_LOG ( ( "Released task\n" ) );	
		
	}
	
	return kIOReturnSuccess;
	
}
	

IOReturn
SCSITaskUserClient::ExecuteTaskAsync ( SCSITask * inSCSITask, void *, void *, void *, void *, void * )
{
	
	IOMemoryDescriptor *	buffer;
	SCSITask * 				task = OSDynamicCast ( SCSITask, inSCSITask );

	if ( fProtocolInterface == NULL )
		return kIOReturnNoDevice;

	if ( task == NULL )
		return kIOReturnBadArgument;
	
	if ( task->IsTaskActive ( ) )
	{
		return kIOReturnNotPermitted;
	}
	
	task->SetAutosenseIsValid ( false );
	
	#if ( SCSI_TASK_USER_CLIENT_DEBUGGING_LEVEL >= 3 )
	{
		SCSICommandDescriptorBlock	cdb;
		UInt8						commandLength;
		
		task->GetCommandDescriptorBlock ( &cdb );
		commandLength = task->GetCommandDescriptorBlockSize ( );
		
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
	
	buffer = task->GetDataBuffer ( );
	if ( buffer != NULL )
		buffer->prepare ( );
	
	task->SetTaskCompletionCallback ( sAsyncTaskCallback );
	task->SetAutosenseCommand ( kSCSICmd_REQUEST_SENSE, 0x00, 0x00, 0x00, sizeof ( SCSI_Sense_Data ), 0x00 );
	fProtocolInterface->ExecuteCommand ( task );
	
	return kIOReturnSuccess;
	
}


IOReturn
SCSITaskUserClient::ExecuteTaskSync ( SCSITask * inSCSITask, vm_address_t senseDataBuffer,
									  SCSITaskStatus * taskStatus, UInt32 * tranferCountHi,
									  UInt32 * tranferCountLo, void * )
{
	
	SCSI_Sense_Data			senseDataBytes;
	SCSIServiceResponse		serviceResponse	= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	IOMemoryDescriptor *	senseBuffer 	= NULL;
	IOMemoryDescriptor *	dataBuffer 		= NULL;
	IOReturn				status 			= kIOReturnSuccess;
	SCSITask * 				task			= NULL;

	if ( fProtocolInterface == NULL )
		return kIOReturnNoDevice;
	
	task = OSDynamicCast ( SCSITask, inSCSITask );
	if ( task == NULL )
		return kIOReturnBadArgument;
	
	if ( task->IsTaskActive ( ) )
	{
		return kIOReturnNotPermitted;
	}
	
	task->SetAutosenseIsValid ( false );
	
	senseBuffer = IOMemoryDescriptor::withAddress ( senseDataBuffer,
											   sizeof ( SCSI_Sense_Data ),
											   kIODirectionIn,
											   fTask );
	
	if ( senseBuffer == NULL )
	{
		return kIOReturnNoMemory;
	}

	dataBuffer = task->GetDataBuffer ( );
	if ( dataBuffer != NULL )
		dataBuffer->prepare ( );
	
	serviceResponse = SendCommand ( task );
	
	*taskStatus = task->GetTaskStatus ( );
	
	if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
		
		UInt64	transferCount = 0;
		
		transferCount = task->GetRealizedDataTransferCount ( );
			
		*tranferCountHi = ( transferCount >> 32 ) & 0xFFFFFFFF;
		*tranferCountLo = transferCount & 0xFFFFFFFF;
		
		if ( *taskStatus == kSCSITaskStatus_CHECK_CONDITION )
		{
			
			if ( task->GetAutoSenseData ( &senseDataBytes ) )
			{
				
				senseBuffer->prepare ( );
				senseBuffer->writeBytes ( 0, &senseDataBytes, sizeof ( SCSI_Sense_Data ) );
				senseBuffer->complete ( );
				
			}
			
		}
		
	}
	
	senseBuffer->release ( );

	if ( dataBuffer != NULL )
		dataBuffer->complete ( );
	
	return status;
	
}
	

IOReturn
SCSITaskUserClient::AbortTask ( SCSITask * inSCSITask, void *, void *, void *, void *, void * )
{
	
	SCSITask * task = OSDynamicCast ( SCSITask, inSCSITask );
	
	if ( fProtocolInterface == NULL )
		return kIOReturnNoDevice;
	
	if ( task == NULL )
		return kIOReturnBadArgument;
	
	// Can't abort an inactive task
	if ( task->IsTaskActive ( ) == false )
	{
		return kIOReturnNotPermitted;
	}
	
	fProtocolInterface->AbortCommand ( inSCSITask );
	
	return kIOReturnSuccess;
	
}


IOReturn
SCSITaskUserClient::SetAsyncCallback ( OSAsyncReference asyncRef,
									   SCSITask * inTask,
									   void * callback, void * refCon,
									   void *, void * )
{
	
	SCSITask *		task;
	UInt8 *			internalRefCon;
	mach_port_t		wakePort;

	if ( fProtocolInterface == NULL )
		return kIOReturnNoDevice;
	
	task = OSDynamicCast ( SCSITask, inTask );
	if ( task == NULL )
		return kIOReturnBadArgument;
	
	if ( task->IsTaskActive ( ) )
	{
		return kIOReturnNotPermitted;
	}
	
	STATUS_LOG ( ( "asyncRef[0] = %d\n", asyncRef[0] ) );
	
	wakePort = ( mach_port_t ) asyncRef[0];
	IOUserClient::setAsyncReference ( asyncRef, wakePort, callback, refCon );
	internalRefCon = ( UInt8 * ) task->GetApplicationLayerReference ( );
	bcopy ( asyncRef, internalRefCon, sizeof ( OSAsyncReference ) );
	
	STATUS_LOG ( ( "internalRefCon[0] = %d\n", internalRefCon[0] ) );
	
	return kIOReturnSuccess;
	
}


IOReturn
SCSITaskUserClient::IsTaskActive ( SCSITask * inSCSITask, UInt32 * active, void *, void *, void *, void * )
{
	
	SCSITask * 	task = OSDynamicCast ( SCSITask, inSCSITask );
	
	if ( task == NULL )
		return kIOReturnBadArgument;
	
	*active = task->IsTaskActive ( );
	
	return kIOReturnSuccess;
	
}


IOReturn
SCSITaskUserClient::SetTransferDirection ( SCSITask * inSCSITask, UInt32 transferDirection, void *, void *, void *, void * )
{
	
	UInt8			direction;
	SCSITask * 		task = OSDynamicCast ( SCSITask, inSCSITask );
	
	if ( task == NULL )
		return kIOReturnBadArgument;
	
	if ( task->IsTaskActive ( ) )
	{
		return kIOReturnNotPermitted;
	}
	
	switch ( transferDirection )
	{
		
		case kSCSIDataTransfer_NoDataTransfer:
		case kSCSIDataTransfer_FromInitiatorToTarget:
		case kSCSIDataTransfer_FromTargetToInitiator:
			break;
		default:
			return kIOReturnBadArgument;
			break;
		
	}
	
	direction = transferDirection & 0xFF;
	
	if ( task->SetDataTransferDirection ( direction ) == false )
		return kIOReturnError;
	
	return kIOReturnSuccess;
	
}


IOReturn
SCSITaskUserClient::SetTaskAttribute ( SCSITask * inSCSITask, UInt32 attribute, void *, void *, void *, void * )
{
	
	SCSITask * 			task = OSDynamicCast ( SCSITask, inSCSITask );
	SCSITaskAttribute	taskAttribute = ( SCSITaskAttribute ) attribute;
	
	if ( task == NULL )
		return kIOReturnBadArgument;
	
	if ( task->IsTaskActive ( ) )
	{
		return kIOReturnNotPermitted;
	}
	
	if ( task->SetTaskAttribute ( taskAttribute ) == false )
		return kIOReturnError;
	
	return kIOReturnSuccess;
	
}


IOReturn
SCSITaskUserClient::GetTaskAttribute ( SCSITask * inSCSITask, UInt32 * attribute, void *, void *, void *, void * )
{
	
	SCSITask * 		task = OSDynamicCast ( SCSITask, inSCSITask );
	
	if ( task == NULL )
		return kIOReturnBadArgument;
	
	*attribute = ( UInt32 ) task->GetTaskAttribute ( );
	
	return kIOReturnSuccess;
	
}


IOReturn
SCSITaskUserClient::SetCommandDescriptorBlock ( SCSITask * inSCSITask, UInt32 size, UInt8 * cdb, UInt32 inDataSize, void *, void * )
{
	
	SCSITask * 		task = OSDynamicCast ( SCSITask, inSCSITask );
	bool			result;
	
	if ( task == NULL )
		return kIOReturnBadArgument;
	
	if ( task->IsTaskActive ( ) )
	{
		return kIOReturnNotPermitted;
	}
	
	switch ( size )
	{
		
		case 6:
			result = task->SetCommandDescriptorBlock ( cdb[0], cdb[1], cdb[2], cdb[3], cdb[4], cdb[5] );
			break;
		
		case 10:
			result = task->SetCommandDescriptorBlock ( cdb[0], cdb[1], cdb[2], cdb[3], cdb[4], cdb[5],
													   cdb[6], cdb[7], cdb[8], cdb[9] );
			break;
		
		case 12:
			result = task->SetCommandDescriptorBlock ( cdb[0], cdb[1], cdb[2], cdb[3], cdb[4], cdb[5],
													   cdb[6], cdb[7], cdb[8], cdb[9], cdb[10], cdb[11] );
			break;
		
		case 16:
			result = task->SetCommandDescriptorBlock ( cdb[0], cdb[1], cdb[2], cdb[3], cdb[4], cdb[5],
													   cdb[6], cdb[7], cdb[8], cdb[9], cdb[10], cdb[11],
													   cdb[12], cdb[13], cdb[14], cdb[15] );
			break;
		
		default:
			return kIOReturnBadArgument;
			break;
		
	}
	
	if ( result == false )
		return kIOReturnError;
	
	return kIOReturnSuccess;
	
}


IOReturn
SCSITaskUserClient::SetScatterGatherList ( SCSITask * inSCSITask, UInt32 inScatterGatherEntries,
										   UInt32 inTransferCountHi, UInt32 inTransferCountLo,
										   UInt32 inTransferDirection,
										   IOVirtualRange * inScatterGatherList )
{
	
	IOReturn				status = kIOReturnSuccess;
 	IOMemoryDescriptor * 	buffer = NULL;
	SCSITask * 				task = NULL;
	UInt64					transferCount;
	UInt8					direction;
	
	STATUS_LOG ( ( "SCSITaskUserClient::SetScatterGatherList\n" ) );

	STATUS_LOG ( ( "inScatterGatherEntries = %ld\n", inScatterGatherEntries ) );
	STATUS_LOG ( ( "inTransferCountHi = %ld\n", inTransferCountHi ) );
	STATUS_LOG ( ( "inTransferCountLo = %ld\n", inTransferCountLo ) );
	STATUS_LOG ( ( "inTransferDirection = %ld\n", inTransferDirection ) );
		
	task = OSDynamicCast ( SCSITask, inSCSITask );
	if ( task == NULL )
		return kIOReturnBadArgument;
	
	if ( task->IsTaskActive ( ) )
	{
		return kIOReturnNotPermitted;
	}
	
	transferCount = inTransferCountHi;
	transferCount = ( transferCount << 32 ) | inTransferCountLo;
	
	if ( inTransferDirection == kSCSIDataTransfer_NoDataTransfer )
		direction = kIODirectionNone;
	else if ( inTransferDirection == kSCSIDataTransfer_FromTargetToInitiator )
		direction = kIODirectionIn;
	else if ( inTransferDirection == kSCSIDataTransfer_FromInitiatorToTarget )
		direction = kIODirectionOut;
	else
		return kIOReturnBadArgument;
	
	buffer = task->GetDataBuffer ( );
	if ( buffer != NULL )
		buffer->release ( );
	
	buffer = IOMemoryDescriptor::withRanges ( inScatterGatherList,
											  inScatterGatherEntries,
											  ( IODirection ) direction,
											  fTask,
											  false /* asReference */ );  
	
	if ( buffer == NULL )
		return kIOReturnNoMemory;
	
	task->SetDataTransferDirection ( inTransferDirection );
	task->SetDataBuffer ( buffer );
	task->SetRequestedDataTransferCount ( transferCount );
	
	return status;
	
}


IOReturn
SCSITaskUserClient::SetSenseDataBuffer ( SCSITask * inSCSITask, vm_address_t buffer, UInt32 bufferSize, void *, void *, void * )
{

#if 0
	
	IOMemoryDescriptor * 	senseBuffer = NULL;
	SCSITask * 				task 		= NULL;
	
	task = OSDynamicCast ( SCSITask, inSCSITask );
	if ( task == NULL )
		return kIOReturnBadArgument;
	
	senseBuffer = IOMemoryDescriptor::withAddress ( buffer, 
													bufferSize,
													kIODirectionOut,
													fTask );
													
	senseBuffer->prepare ( );
	
#endif
	
	// Setting the autosense data buffer in the SCSITask is not yet supported,
	// but is planned for later. This method is a stub for when that
	// functionality is added.
	return kIOReturnUnsupported;
	
}
	

IOReturn
SCSITaskUserClient::SetTimeoutDuration ( SCSITask * inSCSITask, UInt32 timeoutMS, void *, void *, void *, void * )
{
	
	SCSITask * 		task 		= NULL;
	
	task = OSDynamicCast ( SCSITask, inSCSITask );
	if ( task == NULL )
		return kIOReturnBadArgument;
	
	if ( task->IsTaskActive ( ) )
	{
		return kIOReturnNotPermitted;
	}
	
	if ( task->SetTimeoutDuration ( timeoutMS ) == false )
		return kIOReturnError;
	
	return kIOReturnSuccess;
	
}


IOReturn
SCSITaskUserClient::GetTimeoutDuration ( SCSITask * inSCSITask, UInt32 * timeoutMS, void *, void *, void *, void * )
{
	
	SCSITask * 		task 		= NULL;
	
	task = OSDynamicCast ( SCSITask, inSCSITask );
	if ( task == NULL )
		return kIOReturnBadArgument;
	
	*timeoutMS = task->GetTimeoutDuration ( );
	
	return kIOReturnSuccess;
	
}
	

IOReturn
SCSITaskUserClient::GetTaskStatus ( SCSITask * inSCSITask, SCSITaskStatus * taskStatus, void *, void *, void *, void * )
{
	
	SCSITask * 		task 		= NULL;
	
	task = OSDynamicCast ( SCSITask, inSCSITask );
	if ( task == NULL )
		return kIOReturnBadArgument;
	
	*taskStatus = task->GetTaskStatus ( );
	
	return kIOReturnSuccess;
	
}


IOReturn
SCSITaskUserClient::GetSCSIServiceResponse ( SCSITask * inSCSITask, SCSIServiceResponse * serviceResponse, void *, void *, void *, void * )
{
	
	SCSITask * 		task 		= NULL;
	
	task = OSDynamicCast ( SCSITask, inSCSITask );
	if ( task == NULL )
		return kIOReturnBadArgument;
	
	*serviceResponse = task->GetServiceResponse ( );
	
	return kIOReturnSuccess;
	
}


IOReturn
SCSITaskUserClient::GetTaskState ( SCSITask * inSCSITask, SCSITaskState * taskState, void *, void *, void *, void * )
{
	
	SCSITask * 		task 		= NULL;
	
	task = OSDynamicCast ( SCSITask, inSCSITask );
	if ( task == NULL )
		return kIOReturnBadArgument;
	
	*taskState = task->GetTaskState ( );
	
	return kIOReturnSuccess;
	
}


IOReturn
SCSITaskUserClient::GetRealizedDataTransferCount ( SCSITask * inSCSITask, UInt64 * transferCount, void *, void *, void *, void * )
{
	
	SCSITask * 		task 		= NULL;
	
	task = OSDynamicCast ( SCSITask, inSCSITask );
	if ( task == NULL )
		return kIOReturnBadArgument;
	
	*transferCount = task->GetRealizedDataTransferCount ( );
	
	return kIOReturnSuccess;
	
}


IOReturn
SCSITaskUserClient::GetAutoSenseData ( SCSITask * inSCSITask, SCSI_Sense_Data * senseDataBuffer, void *, void *, void *, void * )
{
	
	SCSITask * 		task 		= NULL;
	
	task = OSDynamicCast ( SCSITask, inSCSITask );
	if ( task == NULL )
		return kIOReturnBadArgument;
	
	if ( task->GetAutoSenseData ( senseDataBuffer ) == false )
		return kIOReturnError;
	
	return kIOReturnSuccess;
	
}


IOReturn
SCSITaskUserClient::Inquiry ( UInt32 inqBufferSize,
							  vm_address_t inqBuffer,
							  vm_address_t senseBuffer,
							  UInt32 * outTaskStatus,
							  void *, void * )
{
	
	SCSITask * 				task = NULL;
	IOMemoryDescriptor *	buffer;
	IOMemoryDescriptor *	reqSenseBuffer;
	bool					state = true;
	SCSIServiceResponse		serviceResponse;
	SCSITaskStatus			taskStatus;
	IOReturn				status = kIOReturnSuccess;
	SCSI_Sense_Data			senseData;

	if ( fProtocolInterface == NULL )
		return kIOReturnNoDevice;
	
	state = fProtocolInterface->GetUserClientExclusivityState ( );
	if ( state == true )
	{
		return kIOReturnExclusiveAccess;
	}
	
	if ( inqBufferSize > 0xFF )
		return kIOReturnBadArgument;
	
	task = new SCSITask;
	if ( task == NULL )
		return kIOReturnNoMemory;
	
	if ( task->ResetForNewTask ( ) == false )
	{
		task->release ( );
		task = NULL;
		return kIOReturnNoMemory;
	}
		
	buffer = IOMemoryDescriptor::withAddress ( inqBuffer,
											   inqBufferSize,
											   kIODirectionIn,
											   fTask );
	
	if ( buffer == NULL )
	{
		task->release ( );
		return kIOReturnNoMemory;
	}
		
	task->SetCommandDescriptorBlock ( kSCSICmd_INQUIRY,
									  0x00,
									  0x00,
									  0x00,
									  inqBufferSize & 0xFF,
									  0x00 );
	
	task->SetTimeoutDuration ( 10 * 1000 );
	task->SetDataTransferDirection ( kSCSIDataTransfer_FromTargetToInitiator );
	task->SetDataBuffer ( buffer );
	task->SetRequestedDataTransferCount ( inqBufferSize );
	
	buffer->prepare ( );
	
	serviceResponse = SendCommand ( task );
	
	taskStatus = task->GetTaskStatus ( );
	
	if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
		
		if ( taskStatus != kSCSITaskStatus_GOOD )
		{
			

			if ( task->GetAutoSenseData ( &senseData ) )
			{
				
				reqSenseBuffer = IOMemoryDescriptor::withAddress ( senseBuffer,
														   sizeof ( SCSI_Sense_Data ),
														   kIODirectionIn,
														   fTask );
				
				if ( reqSenseBuffer != NULL )
				{
					reqSenseBuffer->prepare ( );
					reqSenseBuffer->writeBytes ( 0, &senseData, sizeof ( SCSI_Sense_Data ) );
					reqSenseBuffer->complete ( );
					reqSenseBuffer->release ( );
				}
				
			}
						
		}
		
	}
	
	*outTaskStatus = ( UInt32 ) taskStatus;

	buffer->complete ( );

	buffer->release ( );
	task->release ( );
	
	return status;
	
}


IOReturn
SCSITaskUserClient::TestUnitReady ( vm_address_t senseDataBuffer,
									UInt32 * outTaskStatus,
									void *, void *, void *, void * )
{
	
	SCSITask * 				task = NULL;
	SCSITaskStatus			taskStatus;
	bool					state = true;
	SCSIServiceResponse		serviceResponse;
	SCSI_Sense_Data			senseData;
	IOMemoryDescriptor *	buffer;

	if ( fProtocolInterface == NULL )
		return kIOReturnNoDevice;
	
	state = fProtocolInterface->GetUserClientExclusivityState ( );
	if ( state == true )
	{
		return kIOReturnExclusiveAccess;
	}

	task = new SCSITask;
	if ( task == NULL )
		return kIOReturnNoMemory;

	if ( task->ResetForNewTask ( ) == false )
	{
		task->release ( );
		task = NULL;
		return kIOReturnNoMemory;
	}
		
	task->SetCommandDescriptorBlock ( kSCSICmd_TEST_UNIT_READY,
									  0x00,
									  0x00,
									  0x00,
									  0x00,
									  0x00 );

	task->SetTimeoutDuration ( 10 * 1000 );
	task->SetDataTransferDirection ( kSCSIDataTransfer_NoDataTransfer );
	
	serviceResponse = SendCommand ( task );
	
	taskStatus = task->GetTaskStatus ( );
	
	if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
		
		if ( taskStatus == kSCSITaskStatus_CHECK_CONDITION )
		{
			
			if ( task->GetAutoSenseData ( &senseData ) )
			{
				
				buffer = IOMemoryDescriptor::withAddress ( senseDataBuffer,
														   sizeof ( SCSI_Sense_Data ),
														   kIODirectionIn,
														   fTask );
				
				if ( buffer != NULL )
				{
					buffer->prepare ( );
					buffer->writeBytes ( 0, &senseData, sizeof ( SCSI_Sense_Data ) );
					buffer->complete ( );
					buffer->release ( );
				}
				
			}
			
		}
		
	}
	
	*outTaskStatus = ( UInt32 ) taskStatus;
	task->release ( );
	
	return kIOReturnSuccess;
	
}



IOReturn
SCSITaskUserClient::GetPerformance ( UInt32 TOLERANCE_WRITE_EXCEPT, UInt32 STARTING_LBA,
									 UInt32 MAXIMUM_NUMBER_OF_DESCRIPTORS_AND_BUFFER_SIZE,
									 vm_address_t performanceBuffer, vm_address_t senseDataBuffer,
									 UInt32 * outTaskStatus )
{
	
	SCSITask * 				task = NULL;
	IOReturn				status = kIOReturnSuccess;
	bool					state = true;
	SCSIServiceResponse		serviceResponse;
	SCSITaskStatus			taskStatus;
	IOMemoryDescriptor *	buffer;
	IOMemoryDescriptor *	reqSenseBuffer;
	SCSI_Sense_Data			senseData;
	SCSICmdField2Bit 		TOLERANCE;
	SCSICmdField1Bit 		WRITE;
	SCSICmdField2Bit 		EXCEPT;
	SCSICmdField2Byte 		MAXIMUM_NUMBER_OF_DESCRIPTORS;
	UInt16					bufferSize;	

	if ( fProtocolInterface == NULL )
		return kIOReturnNoDevice;
	
	state = fProtocolInterface->GetUserClientExclusivityState ( );
	if ( state == true )
	{
		return kIOReturnExclusiveAccess;
	}

	task = new SCSITask;
	if ( task == NULL )
		return kIOReturnNoMemory;

	if ( task->ResetForNewTask ( ) == false )
	{
		task->release ( );
		task = NULL;
		return kIOReturnNoMemory;
	}
	
	TOLERANCE 	= ( TOLERANCE_WRITE_EXCEPT >> 16 ) & 0x03;
	WRITE 		= ( TOLERANCE_WRITE_EXCEPT >> 8 ) & 0x01;
	EXCEPT 		= ( TOLERANCE_WRITE_EXCEPT & 0x03 );
	
	MAXIMUM_NUMBER_OF_DESCRIPTORS = ( MAXIMUM_NUMBER_OF_DESCRIPTORS_AND_BUFFER_SIZE >> 16 ) & 0xFFFF;
	bufferSize = MAXIMUM_NUMBER_OF_DESCRIPTORS_AND_BUFFER_SIZE & 0xFFFF;
	
	buffer = IOMemoryDescriptor::withAddress ( performanceBuffer,
											   bufferSize,
											   kIODirectionIn,
											   fTask );
	
	if ( buffer == NULL )
	{
		task->release ( );
		return kIOReturnNoMemory;
	}
	
	task->SetCommandDescriptorBlock ( kSCSICmd_GET_PERFORMANCE,
									( TOLERANCE << 3 ) | ( WRITE << 2 ) | EXCEPT,
									( STARTING_LBA >> 24 ) 	& 0xFF,
									( STARTING_LBA >> 16 ) 	& 0xFF,
									( STARTING_LBA >>  8 ) 	& 0xFF,
									  STARTING_LBA         	& 0xFF,
									0x00,
									0x00,
									( MAXIMUM_NUMBER_OF_DESCRIPTORS >> 8 )	& 0xFF,
									MAXIMUM_NUMBER_OF_DESCRIPTORS			& 0xFF,
									0x00,
									0x00 );
	
	task->SetTimeoutDuration ( 30 * 1000 );
	task->SetDataTransferDirection ( kSCSIDataTransfer_FromTargetToInitiator );
	task->SetDataBuffer ( buffer );
	task->SetRequestedDataTransferCount ( bufferSize );
	
	IOReturn bufErr = buffer->prepare ( );
	if ( bufErr != kIOReturnSuccess )
	{
		STATUS_LOG ( ( "Error preparing buffer" ) );
	}
	
	serviceResponse = SendCommand ( task );
	
	taskStatus = task->GetTaskStatus ( );
	
	if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
		
		if ( taskStatus != kSCSITaskStatus_GOOD )
		{
			
			if ( task->GetAutoSenseData ( &senseData ) )
			{
				
				reqSenseBuffer = IOMemoryDescriptor::withAddress ( senseDataBuffer,
														   sizeof ( SCSI_Sense_Data ),
														   kIODirectionIn,
														   fTask );
				
				if ( reqSenseBuffer != NULL )
				{
					reqSenseBuffer->prepare ( );
					reqSenseBuffer->writeBytes ( 0, &senseData, sizeof ( SCSI_Sense_Data ) );
					reqSenseBuffer->complete ( );
					reqSenseBuffer->release ( );
				}
				
			}
						
		}
		
	}

	*outTaskStatus = ( UInt32 ) taskStatus;
	
	buffer->complete ( );
	buffer->release ( );
	task->release ( );
		
	return status;
	
}

IOReturn
SCSITaskUserClient::GetConfiguration ( UInt32 RT, UInt32 STARTING_FEATURE_NUMBER, vm_address_t configBuffer,
									   UInt32 bufferSize, vm_address_t senseDataBuffer, UInt32 * outTaskStatus )
{
	
	SCSITask * 				task = NULL;
	IOReturn				status = kIOReturnSuccess;
	bool					state = true;
	SCSIServiceResponse		serviceResponse;
	SCSITaskStatus			taskStatus;
	IOMemoryDescriptor *	buffer;
	IOMemoryDescriptor *	reqSenseBuffer;
	SCSI_Sense_Data			senseData;

	if ( fProtocolInterface == NULL )
		return kIOReturnNoDevice;
	
	state = fProtocolInterface->GetUserClientExclusivityState ( );
	if ( state == true )
	{
		return kIOReturnExclusiveAccess;
	}

	task = new SCSITask;
	if ( task == NULL )
		return kIOReturnNoMemory;

	if ( task->ResetForNewTask ( ) == false )
	{
		task->release ( );
		task = NULL;
		return kIOReturnNoMemory;
	}
	
	buffer = IOMemoryDescriptor::withAddress ( configBuffer,
											   bufferSize,
											   kIODirectionIn,
											   fTask );
	
	if ( buffer == NULL )
	{
		task->release ( );
		return kIOReturnNoMemory;
	}
	
	task->SetCommandDescriptorBlock ( 	kSCSICmd_GET_CONFIGURATION,
										RT,
										( STARTING_FEATURE_NUMBER >> 8 ) & 0xFF,
										  STARTING_FEATURE_NUMBER        & 0xFF,
										0x00,
										0x00,
										0x00,
										( bufferSize >> 8 ) 	& 0xFF,
										  bufferSize        	& 0xFF,
										0x00 );
	
	task->SetTimeoutDuration ( 30 * 1000 );
	task->SetDataTransferDirection ( kSCSIDataTransfer_FromTargetToInitiator );
	task->SetDataBuffer ( buffer );
	task->SetRequestedDataTransferCount ( bufferSize );
	
	IOReturn bufErr = buffer->prepare ( );
	if ( bufErr != kIOReturnSuccess )
	{
		STATUS_LOG ( ( "Error preparing buffer" ) );
	}
	
	serviceResponse = SendCommand ( task );
	
	taskStatus = task->GetTaskStatus ( );
	
	if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
		
		if ( taskStatus != kSCSITaskStatus_GOOD )
		{
			
			if ( task->GetAutoSenseData ( &senseData ) )
			{
				
				reqSenseBuffer = IOMemoryDescriptor::withAddress ( senseDataBuffer,
														   sizeof ( SCSI_Sense_Data ),
														   kIODirectionIn,
														   fTask );
				
				if ( reqSenseBuffer != NULL )
				{
					reqSenseBuffer->prepare ( );
					reqSenseBuffer->writeBytes ( 0, &senseData, sizeof ( SCSI_Sense_Data ) );
					reqSenseBuffer->complete ( );
					reqSenseBuffer->release ( );
				}
				
			}
						
		}
		
	}

	*outTaskStatus = ( UInt32 ) taskStatus;
	
	buffer->complete ( );
	buffer->release ( );
	task->release ( );
		
	return status;
	
}

IOReturn
SCSITaskUserClient::ModeSense10 ( UInt32 LLBAAandDBD, UInt32 PCandPageCode, vm_address_t pageBuffer,
								  UInt32 bufferSize, vm_address_t senseDataBuffer, UInt32 * outTaskStatus )
{
	
	SCSITask * 				task = NULL;
	IOReturn				status = kIOReturnSuccess;
	bool					state = true;
	SCSIServiceResponse		serviceResponse;
	SCSITaskStatus			taskStatus;
	IOMemoryDescriptor *	buffer;
	IOMemoryDescriptor *	reqSenseBuffer;
	UInt8					byte1, byte2;
	UInt16					ALLOCATION_LENGTH;
	SCSI_Sense_Data			senseData;

	if ( fProtocolInterface == NULL )
		return kIOReturnNoDevice;
	
	state = fProtocolInterface->GetUserClientExclusivityState ( );
	if ( state == true )
	{
		return kIOReturnExclusiveAccess;
	}

	task = new SCSITask;
	if ( task == NULL )
		return kIOReturnNoMemory;

	if ( task->ResetForNewTask ( ) == false )
	{
		task->release ( );
		task = NULL;
		return kIOReturnNoMemory;
	}
	
	ALLOCATION_LENGTH = ( bufferSize & 0xFFFF );
	byte1 = LLBAAandDBD & 0xFF;
	byte2 = PCandPageCode & 0xFF;
	
	buffer = IOMemoryDescriptor::withAddress ( pageBuffer,
											   ALLOCATION_LENGTH,
											   kIODirectionIn,
											   fTask );
	
	if ( buffer == NULL )
	{
		task->release ( );
		return kIOReturnNoMemory;
	}
	
	task->SetCommandDescriptorBlock ( kSCSICmd_MODE_SENSE_10,
									  byte1,
									  byte2,
									  0x00,
									  0x00,
									  0x00,
									  0x00,
									  ( ALLOCATION_LENGTH >> 8 & 0xFF ),
									  ( ALLOCATION_LENGTH & 0xFF ),
									  0x00 );
	
	task->SetTimeoutDuration ( 30 * 1000 );
	task->SetDataTransferDirection ( kSCSIDataTransfer_FromTargetToInitiator );
	task->SetDataBuffer ( buffer );
	task->SetRequestedDataTransferCount ( ALLOCATION_LENGTH );
	
	IOReturn bufErr = buffer->prepare ( );
	if ( bufErr != kIOReturnSuccess )
	{
		STATUS_LOG ( ( "Error preparing buffer" ) );
	}
	
	serviceResponse = SendCommand ( task );
	
	taskStatus = task->GetTaskStatus ( );
	
	if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
		
		if ( taskStatus != kSCSITaskStatus_GOOD )
		{
			
			if ( task->GetAutoSenseData ( &senseData ) )
			{
				
				reqSenseBuffer = IOMemoryDescriptor::withAddress ( senseDataBuffer,
														   sizeof ( SCSI_Sense_Data ),
														   kIODirectionIn,
														   fTask );
				
				if ( reqSenseBuffer != NULL )
				{
					reqSenseBuffer->prepare ( );
					reqSenseBuffer->writeBytes ( 0, &senseData, sizeof ( SCSI_Sense_Data ) );
					reqSenseBuffer->complete ( );
					reqSenseBuffer->release ( );
				}
				
			}
						
		}
		
	}

	*outTaskStatus = ( UInt32 ) taskStatus;
	
	buffer->complete ( );
	buffer->release ( );
	task->release ( );
		
	return status;
	
}


IOReturn
SCSITaskUserClient::SetWriteParametersModePage ( vm_address_t paramBuffer,
												 UInt32 bufferSize,
												 vm_address_t senseDataBuffer,
												 UInt32 * outTaskStatus )
{
	
	SCSITask * 				task = NULL;
	IOReturn				status = kIOReturnSuccess;
	bool					state = true;
	SCSIServiceResponse		serviceResponse;
	SCSITaskStatus			taskStatus;
	IOMemoryDescriptor *	buffer;
	IOMemoryDescriptor *	reqSenseBuffer;
	SCSI_Sense_Data			senseData;
	UInt16					pageSize = ( bufferSize & 0xFF );
	SCSICmdField1Bit		PF = 1;

	if ( fProtocolInterface == NULL )
		return kIOReturnNoDevice;
	
	state = fProtocolInterface->GetUserClientExclusivityState ( );
	if ( state == true )
	{
		return kIOReturnExclusiveAccess;
	}

	task = new SCSITask;
	if ( task == NULL )
		return kIOReturnNoMemory;

	if ( task->ResetForNewTask ( ) == false )
	{
		task->release ( );
		task = NULL;
		return kIOReturnNoMemory;
	}
		
	buffer = IOMemoryDescriptor::withAddress ( paramBuffer,
											   pageSize,
											   kIODirectionOut,
											   fTask );
	
	if ( buffer == NULL )
	{
		task->release ( );
		return kIOReturnNoMemory;
	}
	
	task->SetCommandDescriptorBlock (	kSCSICmd_MODE_SELECT_10,
										( PF << 4),
										0x00,
										0x00,
										0x00,
										0x00,
										0x00,
										( pageSize >> 8 ) & 0xFF,
										  pageSize 		  & 0xFF,
										0x00 );
	
	task->SetTimeoutDuration ( 30 * 1000 );
	task->SetDataTransferDirection ( kSCSIDataTransfer_FromInitiatorToTarget );
	task->SetDataBuffer ( buffer );
	task->SetRequestedDataTransferCount ( pageSize );
	
	IOReturn bufErr = buffer->prepare ( );
	if ( bufErr != kIOReturnSuccess )
	{
		STATUS_LOG ( ( "Error preparing buffer" ) );
	}
	
	serviceResponse = SendCommand ( task );
	
	taskStatus = task->GetTaskStatus ( );
	
	if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
		
		if ( taskStatus != kSCSITaskStatus_GOOD )
		{
			
			if ( task->GetAutoSenseData ( &senseData ) )
			{
				
				reqSenseBuffer = IOMemoryDescriptor::withAddress ( senseDataBuffer,
														   sizeof ( SCSI_Sense_Data ),
														   kIODirectionIn,
														   fTask );
				
				if ( reqSenseBuffer != NULL )
				{
					reqSenseBuffer->prepare ( );
					reqSenseBuffer->writeBytes ( 0, &senseData, sizeof ( SCSI_Sense_Data ) );
					reqSenseBuffer->complete ( );
					reqSenseBuffer->release ( );
				}
				
			}
						
		}
		
	}

	*outTaskStatus = ( UInt32 ) taskStatus;
	
	buffer->complete ( );
	buffer->release ( );
	task->release ( );
		
	return status;
	
}


IOReturn
SCSITaskUserClient::GetTrayState ( UInt32 * trayState )
{
	
	IOReturn		status 			= kIOReturnSuccess;
	UInt8			actualTrayState	= 0;
	bool			state			= false;

	if ( fProtocolInterface == NULL )
		return kIOReturnNoDevice;
	
	state = fProtocolInterface->GetUserClientExclusivityState ( );
	if ( state == true )
	{
		return kIOReturnExclusiveAccess;
	}
	
	status = ( ( IOSCSIMultimediaCommandsDevice * ) fProtocolInterface )->GetTrayState ( &actualTrayState );
	if ( status == kIOReturnSuccess )
	{
		*trayState = actualTrayState;
	}
	
	return status;
	
}


IOReturn
SCSITaskUserClient::SetTrayState ( UInt32 trayState )
{
	
	IOReturn		status 				= kIOReturnSuccess;
	UInt8			desiredTrayState	= 0;
	bool			state				= false;

	if ( fProtocolInterface == NULL )
		return kIOReturnNoDevice;

	state = fProtocolInterface->GetUserClientExclusivityState ( );
	if ( state == true )
	{
		return kIOReturnExclusiveAccess;
	}
	
	desiredTrayState = trayState & 0x01;
	status = ( ( IOSCSIMultimediaCommandsDevice * ) fProtocolInterface )->SetTrayState ( desiredTrayState );
	
	return status;
	
}


IOReturn
SCSITaskUserClient::ReadTableOfContents ( UInt32 MSF_FORMAT,
										  UInt32 TRACK_SESSION_NUMBER,
										  vm_address_t tocBuffer,
										  UInt32 bufferSize,
										  vm_address_t senseDataBuffer,
										  UInt32 * outTaskStatus )
{
	
	IOReturn				status = kIOReturnSuccess;
	SCSITask * 				task = NULL;
	SCSIServiceResponse		serviceResponse;
	IOMemoryDescriptor *	buffer;
	IOMemoryDescriptor *	reqSenseBuffer;
	SCSI_Sense_Data			senseData;
	bool					state = true;
	UInt8					MSF = ( ( MSF_FORMAT >> 8 ) & 0x01 );
	UInt8					FORMAT = ( MSF_FORMAT & 0xFF );

	if ( fProtocolInterface == NULL )
		return kIOReturnNoDevice;
	
	state = fProtocolInterface->GetUserClientExclusivityState ( );
	if ( state == true )
	{
		return kIOReturnExclusiveAccess;
	}
	
	// Validate the parameters
	if ( bufferSize > 0xFFFF ) 
	{
		return kIOReturnBadArgument;
	}

	task = new SCSITask;
	if ( task == NULL )
		return kIOReturnNoMemory;
	
	if ( task->ResetForNewTask ( ) == false )
	{
		task->release ( );
		task = NULL;
		return kIOReturnNoMemory;
	}
	
	buffer = IOMemoryDescriptor::withAddress ( tocBuffer,
											   bufferSize,
											   kIODirectionIn,
											   fTask );
	
	if ( buffer == NULL )
	{
		task->release ( );
		return kIOReturnNoMemory;
	}
	
	if ( FORMAT & 0x04 )
	{
		
		// Use new style from MMC-2
		task->SetCommandDescriptorBlock ( kSCSICmd_READ_TOC_PMA_ATIP,
										  MSF << 1,
										  FORMAT,
										  0x00,
										  0x00,
										  0x00,
										  TRACK_SESSION_NUMBER,
										  ( bufferSize >> 8 ) & 0xFF,
										    bufferSize        & 0xFF,
										  0x00 );

	
	}
	
	else
	{

		// Use old style from SFF-8020i
		task->SetCommandDescriptorBlock ( kSCSICmd_READ_TOC_PMA_ATIP,
										  MSF << 1,
										  0x00,
										  0x00,
										  0x00,
										  0x00,
										  TRACK_SESSION_NUMBER,
										  ( bufferSize >> 8 ) & 0xFF,
										    bufferSize        & 0xFF,
										  ( FORMAT & 0x03 ) << 6 );
		
	}
	
	// set timeout to 30 seconds
	task->SetTimeoutDuration ( 30 * 1000 );
	task->SetDataTransferDirection ( kSCSIDataTransfer_FromTargetToInitiator );
	task->SetDataBuffer ( buffer );
	task->SetRequestedDataTransferCount ( bufferSize );
	
	IOReturn bufErr = buffer->prepare ( );
	if ( bufErr != kIOReturnSuccess )
	{
		STATUS_LOG ( ( "Error preparing buffer" ) );
	}
	
	serviceResponse = SendCommand ( task );
	
	if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
				
		if ( task->GetTaskStatus ( ) != kSCSITaskStatus_GOOD )
		{
			
			if ( task->GetAutoSenseData ( &senseData ) )
			{
				
				reqSenseBuffer = IOMemoryDescriptor::withAddress ( senseDataBuffer,
														   sizeof ( SCSI_Sense_Data ),
														   kIODirectionIn,
														   fTask );
				
				if ( reqSenseBuffer != NULL )
				{
					reqSenseBuffer->prepare ( );
					reqSenseBuffer->writeBytes ( 0, &senseData, sizeof ( SCSI_Sense_Data ) );
					reqSenseBuffer->complete ( );
					reqSenseBuffer->release ( );
				}
				
			}
						
		}

		
	}
	
	*outTaskStatus = ( UInt32 ) task->GetTaskStatus ( );
	
	buffer->complete ( );
	
	buffer->release ( );
	task->release ( );
	
	return status;
	
}


IOReturn
SCSITaskUserClient::ReadDiscInformation ( vm_address_t discInfoBuffer, UInt32 bufferSize,
										  vm_address_t senseDataBuffer, UInt32 * outTaskStatus,
										  void *, void * )
{
	
	IOReturn				status = kIOReturnSuccess;
	SCSITask * 				task = NULL;
	SCSIServiceResponse		serviceResponse;
	IOMemoryDescriptor *	buffer;
	IOMemoryDescriptor *	reqSenseBuffer;
	SCSI_Sense_Data			senseData;
	bool					state = true;

	if ( fProtocolInterface == NULL )
		return kIOReturnNoDevice;
	
	state = fProtocolInterface->GetUserClientExclusivityState ( );
	if ( state == true )
	{
		return kIOReturnExclusiveAccess;
	}
	
	// Validate the parameters
	if ( bufferSize > 0xFFFF ) 
	{
		return kIOReturnBadArgument;
	}

	task = new SCSITask;
	if ( task == NULL )
		return kIOReturnNoMemory;
	
	if ( task->ResetForNewTask ( ) == false )
	{
		task->release ( );
		task = NULL;
		return kIOReturnNoMemory;
	}
	
	buffer = IOMemoryDescriptor::withAddress ( discInfoBuffer,
											   bufferSize,
											   kIODirectionIn,
											   fTask );
	
	if ( buffer == NULL )
	{
		task->release ( );
		return kIOReturnNoMemory;
	}
	
	// Use new style from MMC-2
	task->SetCommandDescriptorBlock ( kSCSICmd_READ_DISC_INFORMATION,
									  0x00,
									  0x00,
									  0x00,
									  0x00,
									  0x00,
									  0x00,
									  ( bufferSize >> 8 ) & 0xFF,
									    bufferSize        & 0xFF,
									  0x00 );
	
	// set timeout to 30 seconds
	task->SetTimeoutDuration ( 30 * 1000 );
	task->SetDataTransferDirection ( kSCSIDataTransfer_FromTargetToInitiator );
	task->SetDataBuffer ( buffer );
	task->SetRequestedDataTransferCount ( bufferSize );
	
	IOReturn bufErr = buffer->prepare ( );
	if ( bufErr != kIOReturnSuccess )
	{
		STATUS_LOG ( ( "Error preparing buffer" ) );
	}
	
	serviceResponse = SendCommand ( task );
	
	if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
				
		if ( task->GetTaskStatus ( ) != kSCSITaskStatus_GOOD )
		{
			
			if ( task->GetAutoSenseData ( &senseData ) )
			{
				
				reqSenseBuffer = IOMemoryDescriptor::withAddress ( senseDataBuffer,
														   sizeof ( SCSI_Sense_Data ),
														   kIODirectionIn,
														   fTask );
				
				if ( reqSenseBuffer != NULL )
				{
					reqSenseBuffer->prepare ( );
					reqSenseBuffer->writeBytes ( 0, &senseData, sizeof ( SCSI_Sense_Data ) );
					reqSenseBuffer->complete ( );
					reqSenseBuffer->release ( );
				}
				
			}
			
		}
				
	}

	*outTaskStatus = ( UInt32 ) task->GetTaskStatus ( );
	
	buffer->complete ( );
	
	buffer->release ( );
	task->release ( );
	
	return status;
	
}


IOReturn
SCSITaskUserClient::ReadTrackInformation ( UInt32 ADDRESS_NUMBER_TYPE,
										   UInt32 LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER,
										   vm_address_t trackInfoBuffer,
										   UInt32 bufferSize, vm_address_t senseDataBuffer,
										   UInt32 * outTaskStatus )
{
	
	IOReturn				status = kIOReturnSuccess;
	SCSITask * 				task = NULL;
	SCSIServiceResponse		serviceResponse;
	IOMemoryDescriptor *	buffer;
	IOMemoryDescriptor *	reqSenseBuffer;
	SCSI_Sense_Data			senseData;
	bool					state = true;

	if ( fProtocolInterface == NULL )
		return kIOReturnNoDevice;
	
	state = fProtocolInterface->GetUserClientExclusivityState ( );
	if ( state == true )
	{
		return kIOReturnExclusiveAccess;
	}
	
	// Validate the parameters
	if ( bufferSize > 0xFFFF ) 
	{
		return kIOReturnBadArgument;
	}

	task = new SCSITask;
	if ( task == NULL )
		return kIOReturnNoMemory;
	
	if ( task->ResetForNewTask ( ) == false )
	{
		task->release ( );
		task = NULL;
		return kIOReturnNoMemory;
	}
	
	buffer = IOMemoryDescriptor::withAddress ( trackInfoBuffer,
											   bufferSize,
											   kIODirectionIn,
											   fTask );
	
	if ( buffer == NULL )
	{
		task->release ( );
		return kIOReturnNoMemory;
	}
	
	// Use new style from MMC-2
	task->SetCommandDescriptorBlock ( kSCSICmd_READ_TRACK_INFORMATION,
									  ADDRESS_NUMBER_TYPE & 0x03,
									  ( LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER >> 24 ) & 0xFF,
									  ( LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER >> 16 ) & 0xFF,
									  ( LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER >>  8 ) & 0xFF,
								 	    LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER         & 0xFF,
									  0x00,
									  ( bufferSize >>  8 ) & 0xFF,
								  	    bufferSize         & 0xFF,
									  0x00 );
	
	// set timeout to 30 seconds
	task->SetTimeoutDuration ( 30 * 1000 );
	task->SetDataTransferDirection ( kSCSIDataTransfer_FromTargetToInitiator );
	task->SetDataBuffer ( buffer );
	task->SetRequestedDataTransferCount ( bufferSize );
	
	IOReturn bufErr = buffer->prepare ( );
	if ( bufErr != kIOReturnSuccess )
	{
		STATUS_LOG ( ( "Error preparing buffer" ) );
	}
	
	serviceResponse = SendCommand ( task );
	
	if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
				
		if ( task->GetTaskStatus ( ) != kSCSITaskStatus_GOOD )
		{
			
			if ( task->GetAutoSenseData ( &senseData ) )
			{
				
				reqSenseBuffer = IOMemoryDescriptor::withAddress ( senseDataBuffer,
														   sizeof ( SCSI_Sense_Data ),
														   kIODirectionIn,
														   fTask );
				
				if ( reqSenseBuffer != NULL )
				{
					reqSenseBuffer->prepare ( );
					reqSenseBuffer->writeBytes ( 0, &senseData, sizeof ( SCSI_Sense_Data ) );
					reqSenseBuffer->complete ( );
					reqSenseBuffer->release ( );
				}
				
			}
						
		}
				
	}

	*outTaskStatus = ( UInt32 ) task->GetTaskStatus ( );
	
	buffer->complete ( );
	
	buffer->release ( );
	task->release ( );
	
	return status;
	
}


IOReturn
SCSITaskUserClient::ReadDVDStructure ( UInt32 LBA,
									   UInt32 LAYER_FORMAT,
									   vm_address_t dvdBuffer,
									   UInt32 bufferSize,
									   vm_address_t senseDataBuffer,
									   UInt32 * outTaskStatus )
{
	
	IOReturn				status = kIOReturnSuccess;
	SCSITask * 				task = NULL;
	SCSIServiceResponse		serviceResponse;
	IOMemoryDescriptor *	buffer;
	IOMemoryDescriptor *	reqSenseBuffer;
	SCSI_Sense_Data			senseData;
	bool					state = true;
	UInt8					LAYER = ( LAYER_FORMAT >> 8 ) & 0xFF;
	UInt8					FORMAT = LAYER_FORMAT & 0xFF;

	if ( fProtocolInterface == NULL )
		return kIOReturnNoDevice;
	
	state = fProtocolInterface->GetUserClientExclusivityState ( );
	if ( state == true )
	{
		return kIOReturnExclusiveAccess;
	}
	
	// Validate the parameters
	if ( bufferSize > 0xFFFF ) 
	{
		return kIOReturnBadArgument;
	}

	task = new SCSITask;
	if ( task == NULL )
		return kIOReturnNoMemory;
	
	if ( task->ResetForNewTask ( ) == false )
	{
		task->release ( );
		task = NULL;
		return kIOReturnNoMemory;
	}
	
	buffer = IOMemoryDescriptor::withAddress ( dvdBuffer,
											   bufferSize,
											   kIODirectionIn,
											   fTask );
	
	if ( buffer == NULL )
	{
		task->release ( );
		return kIOReturnNoMemory;
	}
	
	task->SetCommandDescriptorBlock ( kSCSICmd_READ_DVD_STRUCTURE,
									  0x00,
									  ( LBA >> 24 ) & 0xFF,
									  ( LBA >> 16 ) & 0xFF,
									  ( LBA >> 8  ) & 0xFF,
									    LBA		    & 0xFF,
									  LAYER,
									  FORMAT,
									  ( bufferSize >> 8 ) & 0xFF,
									    bufferSize        & 0xFF,
									  0x00,
									  0x00 );
	
	// set timeout to 30 seconds
	task->SetTimeoutDuration ( 30 * 1000 );
	task->SetDataTransferDirection ( kSCSIDataTransfer_FromTargetToInitiator );
	task->SetDataBuffer ( buffer );
	task->SetRequestedDataTransferCount ( bufferSize );
	
	IOReturn bufErr = buffer->prepare ( );
	if ( bufErr != kIOReturnSuccess )
	{
		STATUS_LOG ( ( "Error preparing buffer" ) );
	}
	
	serviceResponse = SendCommand ( task );
	
	if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
				
		if ( task->GetTaskStatus ( ) != kSCSITaskStatus_GOOD )
		{
			
			if ( task->GetAutoSenseData ( &senseData ) )
			{
				
				reqSenseBuffer = IOMemoryDescriptor::withAddress ( senseDataBuffer,
														   sizeof ( SCSI_Sense_Data ),
														   kIODirectionIn,
														   fTask );
				
				if ( reqSenseBuffer != NULL )
				{
					reqSenseBuffer->prepare ( );
					reqSenseBuffer->writeBytes ( 0, &senseData, sizeof ( SCSI_Sense_Data ) );
					reqSenseBuffer->complete ( );
					reqSenseBuffer->release ( );
				}
				
			}
						
		}
				
	}

	*outTaskStatus = ( UInt32 ) task->GetTaskStatus ( );
	
	buffer->complete ( );
	
	buffer->release ( );
	task->release ( );
	
	return status;
	
}


SCSIServiceResponse
SCSITaskUserClient::SendCommand ( SCSITask * request )
{
	
	SCSIServiceResponse 	serviceResponse;
    IOSyncer *				fSyncLock;
	
	fSyncLock = IOSyncer::create ( false );
	if ( fSyncLock == NULL )
    {
		PANIC_NOW ( ( "SCSITaskUserClient::SendCommand Allocate fSyncLock failed.") );
	}
	
	fSyncLock->signal ( kIOReturnSuccess, false );
	
	request->SetTaskCompletionCallback ( &SCSITaskUserClient::sTaskCallback );
	request->SetApplicationLayerReference ( ( void * ) fSyncLock );
	
	// Should use the Request Sense constant, but hard code to limit changes.
	request->SetAutosenseCommand ( 0x03, 0x00, 0x00, 0x00, sizeof ( SCSI_Sense_Data ), 0x00 );
	
    fSyncLock->reinit ( );
	fProtocolInterface->ExecuteCommand ( request );
	
	// Wait for the completion routine to get called
	serviceResponse = ( SCSIServiceResponse ) fSyncLock->wait ( false );
    fSyncLock->release ( );
		
	return serviceResponse;
	
}


void 
SCSITaskUserClient::sTaskCallback ( SCSITaskIdentifier completedTask )
{
	
	IOSyncer *				fSyncLock;
	SCSIServiceResponse 	serviceResponse;
	SCSITask *				scsiRequest;

	STATUS_LOG ( ( "SCSITaskUserClient::sTaskCallback called.\n") );
		
	scsiRequest = OSDynamicCast( SCSITask, completedTask );
	if ( scsiRequest == NULL )
	{
		PANIC_NOW(( "SCSITaskUserClient::sAsyncTaskCallback scsiRequest==NULL." ));
	}
	
	fSyncLock = ( IOSyncer * ) scsiRequest->GetApplicationLayerReference ( );
	serviceResponse = scsiRequest->GetServiceResponse ( );
	fSyncLock->signal ( serviceResponse, false );
	
}


void 
SCSITaskUserClient::sAsyncTaskCallback ( SCSITaskIdentifier completedTask )
{
	
	OSAsyncReference		asyncRef;
	IOMemoryDescriptor *	buffer;
	UInt8 *					internalRefCon;
	IOReturn				status;
	SCSITask *				scsiRequest;
		
	STATUS_LOG ( ( "SCSITaskUserClient::sAsyncTaskCallback called.\n") );
	
	scsiRequest = OSDynamicCast( SCSITask, completedTask );
	if ( scsiRequest == NULL )
	{
		PANIC_NOW(( "SCSITaskUserClient::sAsyncTaskCallback scsiRequest==NULL." ));
	}
	
	buffer = scsiRequest->GetDataBuffer ( );
	if ( buffer != NULL )
		buffer->complete ( );

	internalRefCon = ( UInt8 * ) scsiRequest->GetApplicationLayerReference ( );
	bcopy ( internalRefCon, asyncRef, sizeof ( OSAsyncReference ) );

	STATUS_LOG ( ( "asyncRef[0] = %d\n", asyncRef[0] ) );
	
	if ( asyncRef[0] != 0 )
	{
		
		void * 	args[16];
		UInt64	actualTransferCount;
		
		STATUS_LOG ( ( "serviceResponse = %d\n", scsiRequest->GetServiceResponse ( ) ) );
		STATUS_LOG ( ( "taskStatus = %d\n", scsiRequest->GetTaskStatus ( ) ) );
		
		// Get the service response and task status.
		args[0] = ( void * ) scsiRequest->GetServiceResponse ( );
		args[1] = ( void * ) scsiRequest->GetTaskStatus ( );
		
		// Get the number of bytes transferred
		actualTransferCount = scsiRequest->GetRealizedDataTransferCount ( );
		
		STATUS_LOG ( ( "actualTransferCount = %ld\n", ( UInt32 ) actualTransferCount ) );
		
		args[2] = ( void * )( ( actualTransferCount >> 32 ) & 0xFFFFFFFF );
		args[3]	= ( void * )( actualTransferCount & 0xFFFFFFFF );
		
		// Send the result
        status = sendAsyncResult ( asyncRef, kIOReturnSuccess, ( void ** ) &args, 4 );
		
		STATUS_LOG ( ( "sendAsyncResult status = 0x%08x\n", status ) );
		
	}
	
}


IOReturn
SCSITaskUserClient::message ( UInt32 type, IOService * provider, void * arg )
{
	
	IOReturn	status = kIOReturnSuccess;

	STATUS_LOG ( ( "message called\n" ) );
	
	STATUS_LOG ( ( "type = %ld, provider = %p\n", type, provider ) );
	
	switch ( type )
	{
		
		case kIOMessageServiceIsRequestingClose:
			break;
		
		case kIOMessageServiceIsTerminated:
			
			STATUS_LOG ( ( "kIOMessageServiceIsTerminated called\n" ) );
			status = HandleTermination ( provider );
			break;
			
		default:
			status = super::message ( type, provider, arg );
			break;
		
	}
	
	return status;
	
}


IOReturn
SCSITaskUserClient::HandleTermination ( IOService * provider )
{
	
	IOReturn	status = kIOReturnSuccess;
	
	if ( provider->isOpen ( this ) )
	{
		
		STATUS_LOG ( ( "Closing provider\n" ) );
		provider->close ( this );
		
		STATUS_LOG ( ( "Closed provider\n" ) );
		
	}
	
	STATUS_LOG ( ( "Detaching provider\n" ) );
	
	detach ( provider );
	fProvider = NULL;
	
	STATUS_LOG ( ( "Detached provider\n" ) );
	
	if ( fProtocolInterface != NULL )
	{
		
		STATUS_LOG ( ( "Releasing exclusive access\n" ) );
		
		( void ) ReleaseExclusiveAccess ( );
		fProtocolInterface = NULL;
		
	}
	
	if ( fSetOfSCSITasks != NULL )
	{
		
		STATUS_LOG ( ( "Releasing any leftover SCSITasks...\n" ) );
		
		for ( UInt32 index = 0; index < fSetOfSCSITasks->getCount ( ); index ++ )
		{
			
			SCSITask *	task = NULL;
			
			task = ( SCSITask * ) fSetOfSCSITasks->getAnyObject ( );
			if ( task == NULL )
				break;
			
			STATUS_LOG ( ( "Releasing task = %p\n", task ) );
			
			ReleaseTask ( task, ( void * ) NULL, ( void * ) NULL,
							( void * ) NULL, ( void * ) NULL, ( void * ) NULL );
			
		}
		
		fSetOfSCSITasks = NULL;
		
	}
	
	return status;
	
}