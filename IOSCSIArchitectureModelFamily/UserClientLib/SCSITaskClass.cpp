/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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

#include "SCSITaskClass.h"
#include "SCSITaskDeviceClass.h"
#include "MMCDeviceUserClientClass.h"

__BEGIN_DECLS
#include <mach/mach_interface.h>
#include <IOKit/iokitmig.h>
__END_DECLS

#define SCSI_TASK_CLASS_DEBUGGING_LEVEL 0

#if ( SCSI_TASK_CLASS_DEBUGGING_LEVEL > 0 )
#define PRINT(x)	printf x
#else
#define PRINT(x)
#endif

//
// static interface table for SCSITaskInterface
//

SCSITaskInterface
SCSITaskClass::sSCSITaskInterface =
{
    0,
	&SCSITaskClass::staticQueryInterface,
	&SCSITaskClass::staticAddRef,
	&SCSITaskClass::staticRelease,
	1, 0, // version/revision
	&SCSITaskClass::staticIsTaskActive,
	&SCSITaskClass::staticSetTaskAttribute,
	&SCSITaskClass::staticGetTaskAttribute,
	&SCSITaskClass::staticSetCommandDescriptorBlock,
	&SCSITaskClass::staticGetCommandDescriptorBlockSize,
	&SCSITaskClass::staticGetCommandDescriptorBlock,
	&SCSITaskClass::staticSetScatterGatherEntries,
	&SCSITaskClass::staticSetTimeoutDuration,
	&SCSITaskClass::staticGetTimeoutDuration,
	&SCSITaskClass::staticSetTaskCompletionCallback,
	&SCSITaskClass::staticExecuteTaskAsync,
	&SCSITaskClass::staticExecuteTaskSync,
	&SCSITaskClass::staticAbortTask,
	&SCSITaskClass::staticGetServiceResponse,
	&SCSITaskClass::staticGetTaskState,
	&SCSITaskClass::staticGetTaskStatus,
	&SCSITaskClass::staticGetRealizedDataTransferCount,
	&SCSITaskClass::staticGetAutoSenseData
};


void *
SCSITaskUserClientLibFactory ( CFAllocatorRef allocator, CFUUIDRef typeID )
{
	
	PRINT ( ( "SCSITaskUserClientLibFactory called\n" ) );
	
	if ( CFEqual ( typeID, kIOSCSITaskDeviceUserClientTypeID ) )
		return ( void * ) SCSITaskDeviceClass::alloc ( );
	
	else if ( CFEqual ( typeID, kIOMMCDeviceUserClientTypeID ) )
		return ( void * ) MMCDeviceUserClientClass::alloc ( );
	
	else
		return NULL;
	
}


SCSITaskInterface **
SCSITaskClass::alloc ( SCSITaskDeviceClass * scsiTaskDevice,
					   io_connect_t connection,
					   mach_port_t asyncPort )
{
	
	IOReturn				status 		= kIOReturnSuccess;
	SCSITaskClass *			task		= NULL;
	SCSITaskInterface **	interface 	= NULL;
	
	task = new SCSITaskClass ( );
	if ( task == NULL )
		goto Error_Exit;
		
	status = task->Init ( scsiTaskDevice, connection, asyncPort );
	
	if ( status != kIOReturnSuccess )
	{
		
		delete task;
		goto Error_Exit;
	
	}
	
	// We return an interface here. queryInterface will not be called.
	// Call AddRef here to bump the refcount
	task->AddRef ( );
	interface = ( SCSITaskInterface ** ) &task->fSCSITaskInterfaceMap.pseudoVTable;
	
Error_Exit :
	
	return interface;
	
}


void
SCSITaskClass::sSetConnectionAndPort ( const void * task, void * context )
{
	
	SCSITaskClass *					myTask;
	MyConnectionAndPortContext *	myContext;

	PRINT ( ( "sSetConnectionAndPort called, task = %p, context = %p\n", task, context ) );
	
	myTask 		= getThis ( ( void * ) task );
	myContext 	= ( MyConnectionAndPortContext * ) context;
	
	(void) myTask->SetConnectionAndPort ( myContext->connection, myContext->asyncPort );
	
}


IOReturn
SCSITaskClass::SetConnectionAndPort ( io_connect_t connection, mach_port_t asyncPort )
{
	
	IOReturn	status = kIOReturnSuccess;
	
	fConnection = connection;
	
	if ( asyncPort != MACH_PORT_NULL )
	{
		
		io_async_ref_t 			asyncRef;
		io_scalar_inband_t		params;
		mach_msg_type_number_t	size = 0;
		
		asyncRef[0] = 0;
		params[0]	= ( UInt32 ) fTaskReference;
		params[1]	= ( UInt32 ) ( IOAsyncCallback ) &SCSITaskClass::staticTaskCompletion;
		params[2]	= ( UInt32 ) this;
		
		status = io_async_method_scalarI_scalarO ( fConnection, asyncPort, 
												   asyncRef, 1, 
												   kSCSITaskUserClientSetAsyncCallback,
												   params, 3,
												   NULL, &size );	
		
		PRINT ( ( "SetAsyncCallback : status = 0x%08x\n", status ) );
		
		if ( status == kIOReturnSuccess )
		{ 
			fAsyncPort = asyncPort;
		}
		
	}
	
	else
		fAsyncPort = MACH_PORT_NULL;
	
	return status;
	
}


IOReturn
SCSITaskClass::Init ( SCSITaskDeviceClass * scsiTaskDevice,
					  io_connect_t connection,
					  mach_port_t asyncPort )
{
	
	IOReturn	status = kIOReturnSuccess;
	
	if ( !connection )
		return kIOReturnBadArgument;
	
	fSCSITaskDevice = scsiTaskDevice;
	
	if ( status == kIOReturnSuccess )
	{
		
		mach_msg_type_number_t len = 1;
		status = io_connect_method_scalarI_scalarO ( connection, 					 
													 kSCSITaskUserClientCreateTask, 
													 NULL, 0, ( int * ) &fTaskReference, &len );
		
		if ( status != kIOReturnSuccess )
			fTaskReference = 0; // just to make sure
		else
			status = SetConnectionAndPort ( connection, asyncPort );
		
		PRINT ( ( "SCSITaskClass : fConnection %d, fAsyncPort %d, fSCSITaskDevice = %p\n", 
					fConnection, fAsyncPort, fSCSITaskDevice ) );
		
		PRINT ( ( "SCSITaskClass :  status = 0x%08x = fTaskReference 0x%08lx\n",
					status, fTaskReference ) );
		
	}
	
	return status;
	
}


// Constructor

SCSITaskClass::SCSITaskClass ( void )
{
		
	fRefCount 			= 0;
	fConnection 		= 0;
	fTaskReference 		= 0;
	fCallbackFunction	= NULL;
	fCallbackRefCon 	= NULL;
	fAsyncPort			= NULL;
	
	// create test driver interface map
	fSCSITaskInterfaceMap.pseudoVTable 	= ( IUnknownVTbl * ) &sSCSITaskInterface;
	fSCSITaskInterfaceMap.obj 			= this;
	
}


// Destructor

SCSITaskClass::~SCSITaskClass ( void )
{

	IOReturn 	status = kIOReturnSuccess;
		
	if ( fSCSITaskDevice )
	{
		fSCSITaskDevice->RemoveTaskFromTaskSet ( ( SCSITaskInterface ** ) fSCSITaskInterfaceMap.pseudoVTable );
	}
	
	if ( fTaskReference )
	{
		
		mach_msg_type_number_t 	len = 0;
		
		status = io_connect_method_scalarI_scalarO ( fConnection, 	
													 kSCSITaskUserClientReleaseTask, 
													 ( int * ) &fTaskReference, 1, NULL, &len );
		
		PRINT ( ( "SCSITaskClass : release task status = 0x%08x\n", status ) );
		
	}
		
}


//////////////////////////////////////////////////////////////////
// IUnknown methods
//

// staticQueryInterface
//

HRESULT
SCSITaskClass::staticQueryInterface ( void * self, REFIID iid, void ** ppv )
{
	return getThis ( self )->QueryInterface ( iid, ppv );
}


// QueryInterface
//

HRESULT
SCSITaskClass::QueryInterface ( REFIID iid, void ** ppv )
{
	
	CFUUIDRef 	uuid 	= CFUUIDCreateFromUUIDBytes ( NULL, iid );
	HRESULT 	result 	= S_OK;

	if ( CFEqual ( uuid, IUnknownUUID ) || CFEqual ( uuid, kIOSCSITaskInterfaceID ) ) 
	{
        
		*ppv = &fSCSITaskInterfaceMap;
        AddRef ( );
		
    }
	
    else
		*ppv = 0;
	
	if ( !*ppv )
		result = E_NOINTERFACE;
	
	CFRelease ( uuid );
	
	return result;
	
}


// staticAddRef
//

UInt32
SCSITaskClass::staticAddRef ( void * self )
{
	return getThis ( self )->AddRef ( );
}


// AddRef
//

UInt32
SCSITaskClass::AddRef ( void )
{
	
	fRefCount += 1;
	return fRefCount;
	
}

// staticRelease
//

UInt32
SCSITaskClass::staticRelease ( void * self )
{
	return getThis ( self )->Release ( );
}


// Release
//

UInt32
SCSITaskClass::Release ( void )
{
	
	UInt32		retVal = fRefCount;
	
	if ( 1 == fRefCount-- ) 
	{
		delete this;
    }
	
    else if ( fRefCount < 0 )
	{
        fRefCount = 0;
	}
	
	return retVal;
	
}


//////////////////////////////////////	
// SCSITask Interface methods

Boolean
SCSITaskClass::staticIsTaskActive ( void * task )
{
	return getThis ( task )->IsTaskActive ( );
}


Boolean
SCSITaskClass::IsTaskActive ( void )
{
	
	Boolean						isActive;
	IOReturn 					status = kIOReturnSuccess;
	mach_msg_type_number_t 		len = 1;
	UInt32						active = 0;
	
	status = io_connect_method_scalarI_scalarO ( fConnection, 	
												 kSCSITaskUserClientIsTaskActive, 
												 ( int * ) &fTaskReference, 1, ( int * ) &active, &len );
	
	if ( status != kIOReturnSuccess )
		return false;
	
	isActive = ( active != 0 );
	
	PRINT ( ( "SCSITaskClass : IsTaskActive status = 0x%08x, isActive = %d\n",
			  status, isActive ) );
	
	return isActive;
	
}


IOReturn
SCSITaskClass::staticSetTaskAttribute ( void * task, SCSITaskAttribute inAttributeValue )
{
	return getThis ( task )->SetTaskAttribute ( inAttributeValue );
}


IOReturn
SCSITaskClass::SetTaskAttribute ( SCSITaskAttribute inAttributeValue )
{
	
	IOReturn 					status = kIOReturnSuccess;
	mach_msg_type_number_t 		len = 0;
	int							params[2];
	
	params[0] = ( int ) fTaskReference;
	params[1] = ( int ) inAttributeValue;
	
	status = io_connect_method_scalarI_scalarO ( fConnection, 	
												 kSCSITaskUserClientSetTaskAttribute, 
												 ( int * ) params, 2, NULL, &len );
			
	PRINT ( ( "SCSITaskClass : SetTaskAttribute status = 0x%08x\n", status ) );
	
	return status;
	
}


IOReturn
SCSITaskClass::staticGetTaskAttribute ( void * task, SCSITaskAttribute * outAttribute )
{
	return getThis ( task )->GetTaskAttribute ( outAttribute );
}


IOReturn
SCSITaskClass::GetTaskAttribute ( SCSITaskAttribute * outAttribute )
{
	
	IOReturn 					status = kIOReturnSuccess;
	mach_msg_type_number_t 		len = 1;
	UInt32						attribute = 0;
	
	status = io_connect_method_scalarI_scalarO ( fConnection, 	
												 kSCSITaskUserClientGetTaskAttribute, 
												 ( int * ) &fTaskReference, 1, ( int * ) &attribute, &len );
	
	PRINT ( ( "SCSITaskClass : GetTaskAttribute status = 0x%08x, attribute = %ld\n",
			  status, attribute ) );
	
	*outAttribute = ( SCSITaskAttribute ) attribute;
	
	return status;
	
}


IOReturn
SCSITaskClass::staticSetCommandDescriptorBlock ( void * task, UInt8 * inCDB, UInt8 inSize )
{
	return getThis ( task )->SetCommandDescriptorBlock ( inCDB, inSize );
}


IOReturn
SCSITaskClass::SetCommandDescriptorBlock ( UInt8 * inCDB, UInt8 inSize )
{
	
	IOReturn 					status = kIOReturnSuccess;
	SCSICommandDescriptorBlock	cdb;
	int							params[2];
	
	switch ( inSize )
	{
		
		case 6:
		case 10:
		case 12:
		case 16:
			break;
		
		default:
			return kIOReturnBadArgument;
			break;
		
	}
	
	memset ( cdb, 0, sizeof ( SCSICommandDescriptorBlock ) );
	memcpy ( cdb, inCDB, inSize );
	memcpy ( fCDB, inCDB, inSize );
	
	fCDBSize = inSize;
	
	params[0] = fTaskReference;
	params[1] = inSize;
	
	status = io_connect_method_scalarI_structureI ( fConnection, 	
												    kSCSITaskUserClientSetCommandDescriptorBlock, 
												   ( int * ) params, 2, ( char * ) cdb, sizeof ( SCSICommandDescriptorBlock ) );
	
	PRINT ( ( "SCSITaskClass : SetCommandDescriptorBlock status = 0x%08x\n", status ) );
		
	return status;
	
}


UInt8
SCSITaskClass::staticGetCommandDescriptorBlockSize ( void * task )
{
	return getThis ( task )->GetCommandDescriptorBlockSize ( );
}


UInt8
SCSITaskClass::GetCommandDescriptorBlockSize ( void )
{
	return fCDBSize;	
}


IOReturn
SCSITaskClass::staticGetCommandDescriptorBlock ( void * task, UInt8 * outCDB )
{
	return getThis ( task )->GetCommandDescriptorBlock ( outCDB );
}


IOReturn
SCSITaskClass::GetCommandDescriptorBlock ( UInt8 * outCDB )
{
	
	memcpy ( outCDB, fCDB, fCDBSize );
	return kIOReturnSuccess;
	
}


IOReturn
SCSITaskClass::staticSetScatterGatherEntries ( void * task,
									   IOVirtualRange * inScatterGatherList,
									   UInt8 inScatterGatherEntries,
									   UInt64 transferCount,
									   UInt8 transferDirection )
{
	return getThis ( task )->SetScatterGatherEntries ( inScatterGatherList,
											   inScatterGatherEntries,
											   transferCount,
											   transferDirection );
}


IOReturn
SCSITaskClass::SetScatterGatherEntries ( IOVirtualRange * inScatterGatherList,
										 UInt8 inScatterGatherEntries,
										 UInt64 transferCount,
										 UInt8 transferDirection )
{
	
	IOReturn 					status = kIOReturnSuccess;
	mach_msg_type_number_t 		len = 0;
	int							params[5];
	
	params[0] = ( int ) fTaskReference;
	params[1] = ( int ) inScatterGatherEntries;
	params[2] = ( int ) ( ( transferCount >> 32 ) & 0xFFFFFFFF );
	params[3] = ( int ) ( transferCount & 0xFFFFFFFF );
	params[4] = ( int ) transferDirection;
	
	len = inScatterGatherEntries * sizeof ( IOVirtualRange );
	
	status =io_connect_method_scalarI_structureI ( fConnection, 	
												 kSCSITaskUserClientSetScatterGatherList, 
												 ( int * ) params, 5,
												 ( char * ) inScatterGatherList, len );
			
	PRINT ( ( "SCSITaskClass : SetScatterGatherEntries status = 0x%08x\n", status ) );
	
	return status;
	
}


IOReturn
SCSITaskClass::staticSetTimeoutDuration ( void * task, UInt32 timeoutDurationMS )
{
	return getThis ( task )->SetTimeoutDuration ( timeoutDurationMS );
}


IOReturn
SCSITaskClass::SetTimeoutDuration ( UInt32 timeoutDurationMS )
{
	
	IOReturn 					status = kIOReturnSuccess;
	mach_msg_type_number_t 		len = 0;
	int							params[2];
	
	params[0] = ( int ) fTaskReference;
	params[1] = ( int ) timeoutDurationMS;
	
	fTimeoutDuration = timeoutDurationMS;
	
	status = io_connect_method_scalarI_scalarO ( fConnection, 	
												 kSCSITaskUserClientSetTimeoutDuration, 
												 ( int * ) params, 2, NULL, &len );
			
	PRINT ( ( "SCSITaskClass : SetTimeoutDuration status = 0x%08x\n", status ) );
	
	return status;
	
}


UInt32
SCSITaskClass::staticGetTimeoutDuration ( void * task )
{
	return getThis ( task )->GetTimeoutDuration ( );
}


UInt32
SCSITaskClass::GetTimeoutDuration ( void )
{
	return fTimeoutDuration;
}


IOReturn
SCSITaskClass::staticSetTaskCompletionCallback ( void * task,
												 SCSITaskCallbackFunction callback,
												 void * refCon )
{
	return getThis ( task )->SetTaskCompletionCallback ( callback, refCon );
}


IOReturn
SCSITaskClass::SetTaskCompletionCallback ( SCSITaskCallbackFunction callback,
										   void * refCon )
{

	fCallbackFunction 	= callback;
	fCallbackRefCon 	= refCon;
	
	return kIOReturnSuccess;
	
}


IOReturn
SCSITaskClass::staticExecuteTaskAsync ( void * task )
{
	return getThis ( task )->ExecuteTaskAsync ( );
}


IOReturn
SCSITaskClass::ExecuteTaskAsync ( void )
{
	
	IOReturn 				status = kIOReturnSuccess;
	mach_msg_type_number_t 	len = 0;
	
	if ( fAsyncPort == MACH_PORT_NULL )
		return kIOReturnNotPermitted;
	
	fIsTaskSynch = false;
	
	status = io_connect_method_scalarI_scalarO ( fConnection, 	
												 kSCSITaskUserClientExecuteTaskAsync, 
												 ( int * ) &fTaskReference, 1, NULL, &len );
	
	fRealizedTransferCount = 0;
	
	PRINT ( ( "SCSITaskClass : ExecuteTask status = 0x%08x\n", status ) );
	
	return status;
	
}


IOReturn
SCSITaskClass::staticExecuteTaskSync ( void * task, SCSI_Sense_Data * senseDataBuffer,
									   SCSITaskStatus * taskStatus, UInt64 * realizedTransferCount )
{
	return getThis ( task )->ExecuteTaskSync ( senseDataBuffer, taskStatus, realizedTransferCount );
}


IOReturn
SCSITaskClass::ExecuteTaskSync ( SCSI_Sense_Data * senseDataBuffer,
								 SCSITaskStatus * taskStatus,
								 UInt64 * realizedTransferCount )
{
	
	IOReturn 				status = kIOReturnSuccess;
	mach_msg_type_number_t 	len = 3;
	int						params[2];
	int						outParams[3];
	
	fIsTaskSynch = true;
	
	params[0] = ( int ) fTaskReference;
	params[1] = ( int ) senseDataBuffer;

	fRealizedTransferCount = 0;
	
	status = io_connect_method_scalarI_scalarO ( fConnection, 	
												 kSCSITaskUserClientExecuteTaskSync, 
												 ( int * ) params, 2,
												 ( int * ) outParams, &len );
	
	PRINT ( ( "SCSITaskClass : ExecuteTaskSync status = 0x%08x\n", status ) );
	
	*taskStatus 			= ( SCSITaskStatus ) outParams[0];
	*realizedTransferCount	= ( ( UInt64 ) outParams[1] << 32 );
	*realizedTransferCount	+= ( UInt32 ) outParams[2];
	
	if ( fTaskStatus == kSCSITaskStatus_CHECK_CONDITION )
	{
		
		status = GetAutoSenseData ( senseDataBuffer );
		
	}
	
	return status;
	
}


IOReturn
SCSITaskClass::staticAbortTask ( void * task )
{
	return getThis ( task )->AbortTask ( );
}


IOReturn
SCSITaskClass::AbortTask ( void )
{
	
	IOReturn 				status = kIOReturnSuccess;
	mach_msg_type_number_t 	len = 0;
	
	status = io_connect_method_scalarI_scalarO ( fConnection, 	
												 kSCSITaskUserClientAbortTask, 
												 ( int * ) &fTaskReference, 1, NULL, &len );
	
	PRINT ( ( "SCSITaskClass : AbortTask status = 0x%08x\n", status ) );
	
	return status;

}


IOReturn
SCSITaskClass::staticGetTaskState ( void * task, SCSITaskState * outState )
{
	return getThis ( task )->GetTaskState ( outState );
}


IOReturn
SCSITaskClass::GetTaskState ( SCSITaskState * outState )
{
		
	IOReturn 					status = kIOReturnSuccess;
	mach_msg_type_number_t 		len = 1;
	UInt32						taskState = 0;
	
	status = io_connect_method_scalarI_scalarO ( fConnection, 	
												 kSCSITaskUserClientGetTaskState, 
												 ( int * ) &fTaskReference, 1,
												 ( int * ) &taskState, &len );
	
	PRINT ( ( "SCSITaskClass : GetTaskState status = 0x%08x, taskState = %ld\n",
			  status, taskState ) );
	
	*outState = ( SCSITaskState ) taskState;
	
	return status;
	

}


IOReturn
SCSITaskClass::staticGetTaskStatus ( void * task, SCSITaskStatus * outStatus )
{
	return getThis ( task )->GetTaskStatus ( outStatus );
}


IOReturn
SCSITaskClass::GetTaskStatus ( SCSITaskStatus * outStatus )
{
	
	IOReturn 					status = kIOReturnSuccess;
	mach_msg_type_number_t 		len = 1;
	UInt32						taskStatus = 0;
	
	status = io_connect_method_scalarI_scalarO ( fConnection, 	
												 kSCSITaskUserClientGetTaskStatus, 
												 ( int * ) &fTaskReference, 1,
												 ( int * ) &taskStatus, &len );
	
	PRINT ( ( "SCSITaskClass : GetTaskStatus status = 0x%08x, taskStatus = %ld\n",
			  status, taskStatus ) );
	
	*outStatus = ( SCSITaskStatus ) taskStatus;
	
	return status;
	
}


UInt64
SCSITaskClass::staticGetRealizedDataTransferCount ( void * task )
{
	return getThis ( task )->GetRealizedDataTransferCount ( );
}


UInt64
SCSITaskClass::GetRealizedDataTransferCount ( void )
{
	return fRealizedTransferCount;
}


IOReturn
SCSITaskClass::staticGetServiceResponse ( void * task, SCSIServiceResponse * outResponse )
{
	return getThis ( task )->GetServiceResponse ( outResponse );
}


IOReturn
SCSITaskClass::GetServiceResponse ( SCSIServiceResponse * outResponse )
{
	
	IOReturn 					status = kIOReturnSuccess;
	mach_msg_type_number_t 		len = 1;
	UInt32						serviceResponse = 0;
	
	status = io_connect_method_scalarI_scalarO ( fConnection, 	
												 kSCSITaskUserClientGetSCSIServiceResponse, 
												 ( int * ) &fTaskReference, 1,
												 ( int * ) &serviceResponse, &len );
	
	PRINT ( ( "SCSITaskClass : GetServiceResponse status = 0x%08x, serviceResponse = %ld\n",
			  status, serviceResponse ) );
	
	*outResponse = ( SCSIServiceResponse ) serviceResponse;
	
	return status;
	
}


IOReturn
SCSITaskClass::staticGetAutoSenseData ( void * task, SCSI_Sense_Data * senseDataBuffer )
{
	return getThis ( task )->GetAutoSenseData ( senseDataBuffer );
}


IOReturn
SCSITaskClass::GetAutoSenseData ( SCSI_Sense_Data * senseDataBuffer )
{
	
	IOReturn 					status = kIOReturnSuccess;
	mach_msg_type_number_t 		len = sizeof ( SCSI_Sense_Data );
	
	status = io_connect_method_scalarI_structureO ( fConnection, 	
												    kSCSITaskUserClientGetAutoSenseData, 
												    ( int * ) &fTaskReference, 1,
													( char * ) senseDataBuffer, &len );
	
	PRINT ( ( "SCSITaskClass : GetAutoSenseData status = 0x%08x\n", status ) );
	PRINT ( ( "SENSE_KEY_CODE: 0x%08x, ASC: 0x%08x, ASCQ: 0x%08x\n",
			   senseDataBuffer->SENSE_KEY & kSENSE_KEY_Mask,
			   senseDataBuffer->ADDITIONAL_SENSE_CODE,
			   senseDataBuffer->ADDITIONAL_SENSE_CODE_QUALIFIER ) );
	
	return status;
	
}


// callback routines
void
SCSITaskClass::staticTaskCompletion ( void * refcon, IOReturn result,
									  void ** args, int numArgs )
{

	PRINT ( ( "SCSITaskClass : staticTaskCompletion, numArgs = %d\n", numArgs ) );
	( ( SCSITaskClass * ) refcon )->TaskCompletion ( result, args, numArgs );
}


void
SCSITaskClass::TaskCompletion ( IOReturn result, void ** args, int numArgs )
{
	
	PRINT ( ( "SCSITaskClass : TaskCompletion, numArgs = %d\n", numArgs ) );
	PRINT ( ( "SCSITaskClass : TaskCompletion, result = %d\n", result ) );
	
	SCSIServiceResponse		serviceResponse;
	SCSITaskStatus			taskStatus;
	UInt64					bytesTransferred;
	
	if ( numArgs == 4 )
	{
		
		serviceResponse 	= ( SCSIServiceResponse ) ( UInt32 ) args[0];
		taskStatus 			= ( SCSITaskStatus ) ( UInt32 ) ( args[1] );
		bytesTransferred	= ( ( ( UInt64 )( args[2] ) ) << 32 );
		bytesTransferred	+= ( UInt32 ) ( args[3] );
		
		fRealizedTransferCount = bytesTransferred;
		
		PRINT ( ( "serviceResponse = %d, taskStatus = %d, bytesTransferred = %ld\n",
				   serviceResponse, taskStatus, ( UInt32 ) bytesTransferred ) );
				
		if ( fCallbackFunction != NULL )
			( fCallbackFunction )( serviceResponse, taskStatus, bytesTransferred, fCallbackRefCon );
		
	}
	
}

void
SCSITaskClass::sAbortAndReleaseTasks ( const void * value, void * context )
{
	
	if ( value == NULL )
		return;
	
	SCSITaskClass *		task = getThis ( ( void * ) value );
	
	if ( task != NULL )
	{
		
		if ( task->IsTaskActive ( ) )
		{
			
			( void ) task->AbortTask ( );
			
		}
		
	}
	
}
