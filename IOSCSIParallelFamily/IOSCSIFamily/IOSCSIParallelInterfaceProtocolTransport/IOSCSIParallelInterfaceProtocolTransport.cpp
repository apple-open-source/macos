/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All rights reserved.
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

/*
 * Copyright (c) 2000-2001 Apple Computer, Inc.  All rights reserved.
 *
 * HISTORY
 *
 *		12.21.2000	CJS  Started SCSI Parallel Interface Transport Layer
 *
 */

#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/IOSyncer.h>
#include <IOKit/IORegistryEntry.h>
#include <IOKit/scsi/SCSICmds_INQUIRY_Definitions.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>


#include "IOSCSIDevice.h"
#include "IOSCSIParallelInterfaceProtocolTransport.h"


#if ( SCSI_PARALLEL_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_PARALLEL_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_PARALLEL_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif


// Timeout values used by the SCSI Driver
enum 
{
	kNoTimeout			= 0,					// Constant to indicate no timeout
	k1SecondTimeout		= 1000,					// 1000 mS = 1 Sec
	k10SecondTimeout	= 10 * k1SecondTimeout,	
	k30SecondTimeout	= 3 * k10SecondTimeout,
	k45SecondTimeout	= 45000,
	k100Milliseconds	= 100
};

enum
{
	kSCSICommandPoolSize	= 1
};

enum
{
	kMaxSenseDataSize		= 255
};


#define super IOSCSIProtocolServices
OSDefineMetaClassAndStructors ( IOSCSIParallelInterfaceProtocolTransport, IOSCSIProtocolServices );


#pragma mark -
#pragma mark Public Methods

//--------------------------------------------------------------------------------------
//	е start	-	Start our services
//--------------------------------------------------------------------------------------

bool
IOSCSIParallelInterfaceProtocolTransport::start ( IOService * provider )
{
	
	IOReturn		theErr		= kIOReturnSuccess;
	IOWorkLoop *	workLoop	= NULL;
	
	STATUS_LOG ( ( "IOSCSIParallelInterfaceProtocolTransport::start called\n" ) );
	
	fSCSIDevice = NULL;
	
	// First call start() in our superclass
	if ( super::start ( provider ) == false )
		return false;

	// Cache our provider
	fSCSIDevice = OSDynamicCast ( IOSCSIDevice, provider );
	if ( fSCSIDevice == NULL )
	{
		
		ERROR_LOG ( ( "Error in dynamic cast\n" ) );
		// Error in the dynamic cast, so get out
		return false;
	
	}
		
	// Open the thing below us
	if ( fSCSIDevice->open ( this ) == false )
	{

		ERROR_LOG ( ( "device wouldn't open\n" ) );
		// It wouldn't open, so bail
		return false;
		
	}
	
	// Create an IOCommandGate and attach
	// this event source to the provider's workloop
	fCommandGate = IOCommandGate::commandGate ( this );
	if ( fCommandGate == NULL )
	{
		
		ERROR_LOG ( ( "fCommandGate == NULL.\n" ) );
		goto CLOSE_DEVICE_ERROR;
		
	}
	
	workLoop = getWorkLoop ( );
	if ( workLoop == NULL )
	{
		
		ERROR_LOG ( ( "workLoop == NULL.\n" ) );
		goto RELEASE_COMMAND_GATE_ERROR;
		
	}
	
	theErr = workLoop->addEventSource ( fCommandGate );
	if ( theErr != kIOReturnSuccess )
	{
		
		ERROR_LOG ( ( "Error = %d adding event source.\n", theErr ) );
		goto RELEASE_COMMAND_GATE_ERROR;
		
	}
	
	fCommandPool = IOCommandPool::withWorkLoop ( workLoop );
	if ( fCommandPool == NULL )
	{
		
		ERROR_LOG ( ( "fCommandPool == NULL.\n" ) );
		goto REMOVE_EVENT_SOURCE_ERROR;
		
	}
	
	// Pre-allocate some command objects	
	AllocateSCSICommandObjects ( );

	// Inspect the provider
	if ( InspectDevice ( fSCSIDevice ) == false )
	{
		
		ERROR_LOG ( ( "InspectDevice returned false.\n" ) );
		goto DEALLOCATE_COMMANDS_ERROR;
		
	}
	
	InitializePowerManagement ( provider );
	
	STATUS_LOG ( ( "IOSCSIParallelInterfaceProtocolTransport::start complete\n" ) );
	
	registerService ( );
	
	return true;
	
	
DEALLOCATE_COMMANDS_ERROR:
	
	DeallocateSCSICommandObjects ( );
		
	if ( fCommandPool != NULL )
		fCommandPool->release ( );
	
REMOVE_EVENT_SOURCE_ERROR:
	
	if ( workLoop != NULL )
		workLoop->removeEventSource ( fCommandGate );
	
RELEASE_COMMAND_GATE_ERROR:
	
	if ( fCommandGate != NULL )
		fCommandGate->release ( );
	
CLOSE_DEVICE_ERROR:
	
	fSCSIDevice->close ( this );
	return false;
	
}


//--------------------------------------------------------------------------------------
//	е stop	-	Stop our services
//--------------------------------------------------------------------------------------

void
IOSCSIParallelInterfaceProtocolTransport::stop ( IOService * provider )
{
	
	STATUS_LOG ( ( "IOSCSIParallelInterfaceProtocolTransport::stop called\n" ) );
	
	DeallocateSCSICommandObjects ( );
	
	// Call super's stop
	super::stop ( provider );
	
}


#pragma mark -
#pragma mark Protected Methods


//--------------------------------------------------------------------------------------
//	е SendSCSICommand	-	Sends a SCSI Command to the provider bus
//--------------------------------------------------------------------------------------

bool
IOSCSIParallelInterfaceProtocolTransport::SendSCSICommand (
											SCSITaskIdentifier		request,
											SCSIServiceResponse * 	serviceResponse,
											SCSITaskStatus *		taskStatus )
{
	
	SCSICommandDescriptorBlock 		cdb;
	UInt16 							commandLength		= 0;
	IOSCSICommand *					cmd					= NULL;
	SCSIClientData *				clientData			= NULL;
	SCSICDBInfo						cdbInfo;
	UInt8							requestType;
	
	cmd = GetSCSICommandObject ( false );
	if ( cmd == NULL )
	{
		return false;
	}
	
	bzero ( &cdbInfo, sizeof ( cdbInfo ) );
	
	clientData			= ( SCSIClientData * ) cmd->getClientData ( );
	*serviceResponse 	= kSCSIServiceResponse_Request_In_Process;
	
	STATUS_LOG ( ( "IOSCSIParallelInterfaceProtocolTransport::SendSCSICommand called\n" ) );
	
	GetCommandDescriptorBlock ( request, &cdb );
	commandLength = GetCommandDescriptorBlockSize ( request );
	if ( commandLength == kSCSICDBSize_6Byte )
	{
		
		STATUS_LOG( ( "cdb = %02x:%02x:%02x:%02x:%02x:%02x\n", cdb[0], cdb[1],
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
	
	for ( UInt32 index = 0; index < commandLength; index++ )
	{
		
		STATUS_LOG ( ( "copying the cdb\n" ) );
		cdbInfo.cdb[index] = cdb[index];
		
	}
	
	STATUS_LOG ( ( "setting the cdb length\n" ) );
	cdbInfo.cdbLength = commandLength;
	
	STATUS_LOG ( ( "zeroing the scsi command\n" ) );
	cmd->zeroCommand ( );
	
	STATUS_LOG ( ( "Setting the data pointers\n" ) );
	// Start filling in the command
	requestType = GetDataTransferDirection ( request );
	if ( requestType == kSCSIDataTransfer_NoDataTransfer )
	{
		
		STATUS_LOG ( ( "Setting for no data transfer\n" ) );
		cmd->setPointers ( 0, 0, false, false );
		
	}
	
	else if ( requestType == kSCSIDataTransfer_FromTargetToInitiator )
	{
		
		STATUS_LOG ( ( "Setting for DataTransferFromTargetToInitiator\n" ) );
		cmd->setPointers ( 	GetDataBuffer ( request ),
							GetRequestedDataTransferCount ( request ),
							false,
							false );
		
	}
	
	else
	{
		
		STATUS_LOG ( ( "Setting for DataTransferFromInitiatorToTarget\n" ) );
		cmd->setPointers ( 	GetDataBuffer ( request ),
							GetRequestedDataTransferCount ( request ),
							true,
							false );
		
	}
	
	STATUS_LOG ( ( "Setting the auto-sense pointers\n" ) );
	
	// Set the auto request-sense buffer for the command
	cmd->setPointers ( 	clientData->senseBuffer,
						min ( clientData->senseBuffer->getLength ( ), GetAutosenseRequestedDataTransferCount ( request ) ),
						false,
						true );
	
	STATUS_LOG ( ( "Setting the command timeout\n" ) );
	cmd->setTimeout ( GetTimeoutDuration ( request ) );
	
	STATUS_LOG ( ( "Setting the cdb\n" ) );
	cmd->setCDB ( &cdbInfo );
	
	// Setup our context
	STATUS_LOG ( ( "Setting up the context\n" ) );
	clientData->scsiTask 	= request;
	clientData->cmd			= cmd;
	
	STATUS_LOG ( ( "Setting the callback proc\n" ) );
	cmd->setCallback ( this, &sSCSICallbackProc, ( void * ) clientData );
	
	STATUS_LOG ( ( "Executing the command\n" ) );
	cmd->execute ( );
	
	STATUS_LOG ( ( "Exiting...\n" ) );
	
	return true;
	
}


//--------------------------------------------------------------------------------------
//	е SCSICallbackFunction	-	virtual callback routine which calls CompleteSCSITask
//--------------------------------------------------------------------------------------

void
IOSCSIParallelInterfaceProtocolTransport::SCSICallbackFunction (
												IOSCSICommand *		cmd,
												SCSIClientData *	clientData )
{
	
	SCSIResults				results;
	SCSIServiceResponse		serviceResponse;
	SCSITaskStatus			taskStatus;
	
	STATUS_LOG ( ( "%s::%s entering\n", getName ( ), __FUNCTION__ ) );	
	
	cmd->getResults ( &results );
	
	STATUS_LOG ( ( "Returning the scsi command to pool\n" ) );
	ReturnSCSICommandObject ( cmd );
	
	// Some controllers flag underrun errors. We explicitly check the
	// realized data transfer count and complete the I/O with what the controller
	// said was transferred.
	if ( results.returnCode == kIOReturnUnderrun )
		results.returnCode = kIOReturnSuccess;
	
	ERROR_LOG ( ( "Error code = 0x%08x\n", results.returnCode ) );
	if ( results.returnCode != kIOReturnSuccess )
	{
		
		// Some controllers return kIOReturnIOError on check conditions. We should
		// not treat these as SERVICE_DELIVERY_OR_TARGET_FAILUREs but as TASK_COMPLETEs
		// with CHECK_CONDITION set.
		if ( results.scsiStatus == kSCSIStatusCheckCondition )
		{
			
			ERROR_LOG ( ( "%s::%s result = %d\n", getName ( ), __FUNCTION__, results.scsiStatus ) );
			
			if ( results.requestSenseDone )
			{
				
				// Pull out the sense bytes here
				ERROR_LOG ( ( "Pulling out sense data\n" ) );
				SetAutoSenseData ( clientData->scsiTask,
								   ( SCSI_Sense_Data * ) clientData->senseBuffer->getBytesNoCopy ( ),
								   clientData->senseBuffer->getLength ( ) );
				ERROR_LOG ( ( "Finished setting sense data\n" ) );
				
			}
			
			SetRealizedDataTransferCount ( clientData->scsiTask, results.bytesTransferred );
			
			taskStatus 		= kSCSITaskStatus_CHECK_CONDITION;
			serviceResponse = kSCSIServiceResponse_TASK_COMPLETE;
			
		}
		
		else
		{
			
			// Some error occurred
			serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
			taskStatus 		= kSCSITaskStatus_No_Status;
			
		}
		
	}
	
	else
	{
		
		switch ( results.scsiStatus )
		{
			
			case kSCSIStatusGood:
				STATUS_LOG ( ( "%s::%s result = noErr\n", getName ( ), __FUNCTION__ ) );	
				SetRealizedDataTransferCount ( clientData->scsiTask, results.bytesTransferred );
				taskStatus 		= kSCSITaskStatus_GOOD;
				serviceResponse = kSCSIServiceResponse_TASK_COMPLETE;
				break;
				
			case kSCSIStatusCheckCondition:
				
				ERROR_LOG ( ( "%s::%s result = %d\n", getName ( ), __FUNCTION__, results.scsiStatus ) );
				
				// Some error occurred
				if ( results.requestSenseDone )
				{
					
					// Pull out the sense bytes here
					ERROR_LOG ( ( "Pulling out sense data\n" ) );
					SetAutoSenseData ( clientData->scsiTask,
									   ( SCSI_Sense_Data * ) clientData->senseBuffer->getBytesNoCopy ( ),
									   clientData->senseBuffer->getLength ( ) );
					ERROR_LOG ( ( "Finished setting sense data\n" ) );
					
				}
				
				SetRealizedDataTransferCount ( clientData->scsiTask, results.bytesTransferred );
				
				taskStatus 		= kSCSITaskStatus_CHECK_CONDITION;
				serviceResponse = kSCSIServiceResponse_TASK_COMPLETE;
				break;
				
			case kSCSIStatusConditionMet:
	
				taskStatus 		= kSCSITaskStatus_CONDITION_MET;
				serviceResponse = kSCSIServiceResponse_TASK_COMPLETE;
				break;
				
			case kSCSIStatusBusy:
				
				taskStatus 		= kSCSITaskStatus_BUSY;
				serviceResponse = kSCSIServiceResponse_TASK_COMPLETE;				
				break;
				
			case kSCSIStatusIntermediate:
				
				taskStatus 		= kSCSITaskStatus_INTERMEDIATE;
				serviceResponse = kSCSIServiceResponse_TASK_COMPLETE;				
				break;
				
			case kSCSIStatusIntermediateMet:
				
				taskStatus 		= kSCSITaskStatus_INTERMEDIATE_CONDITION_MET;
				serviceResponse = kSCSIServiceResponse_TASK_COMPLETE;				
				break;
				
			case kSCSIStatusReservationConfict:
				
				taskStatus 		= kSCSITaskStatus_RESERVATION_CONFLICT;
				serviceResponse = kSCSIServiceResponse_TASK_COMPLETE;				
				break;
				
			case kSCSIStatusQueueFull:
				
				taskStatus 		= kSCSITaskStatus_TASK_SET_FULL;
				serviceResponse = kSCSIServiceResponse_TASK_COMPLETE;				
				break;
			
			default:
				
				taskStatus 		= kSCSITaskStatus_No_Status;
				serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
				break;
			
		}
		
	}
	
	CompleteSCSITask ( 	clientData->scsiTask,
						serviceResponse,
						taskStatus );
	
	STATUS_LOG ( ( "Exiting virtual callback\n" ) );
	
}


//--------------------------------------------------------------------------------------
//	е CompleteSCSITask	-	Called to complete a SCSI Command
//--------------------------------------------------------------------------------------

void
IOSCSIParallelInterfaceProtocolTransport::CompleteSCSITask (
											SCSITaskIdentifier	scsiTask,
											SCSIServiceResponse	serviceResponse,
											SCSITaskStatus		taskStatus )
{
		
	STATUS_LOG ( ( "IOSCSIParallelInterfaceProtocolTransport::CompleteSCSITask called\n" ) );

	CommandCompleted ( scsiTask, serviceResponse, taskStatus );
	
}


//--------------------------------------------------------------------------------------
//	е AbortSCSICommand	-	Aborts a SCSI Command
//--------------------------------------------------------------------------------------

SCSIServiceResponse
IOSCSIParallelInterfaceProtocolTransport::AbortSCSICommand ( SCSITaskIdentifier request )
{

	SCSIServiceResponse	serviceResponse = kSCSIServiceResponse_FUNCTION_REJECTED;
	
	ERROR_LOG ( ( "IOSCSIParallelInterfaceProtocolTransport::AbortSCSICommand called\n" ) );
		
	return serviceResponse;
	
}


//--------------------------------------------------------------------------------------
//	е IsProtocolServiceSupported
//--------------------------------------------------------------------------------------


bool
IOSCSIParallelInterfaceProtocolTransport::IsProtocolServiceSupported (
											SCSIProtocolFeature feature,
											void * value )
{
	
	bool	isSupported = false;
	
	switch ( feature )
	{
		
		case kSCSIProtocolFeature_GetMaximumLogicalUnitNumber:
			isSupported = true;
			*( UInt32 * ) value = 0;
			break;
		
		case kSCSIProtocolFeature_MaximumReadTransferByteCount:
			isSupported = true;			
			*( UInt64 * ) value = 131072;	// Cap to 128k for old family
			break;
		
		case kSCSIProtocolFeature_MaximumWriteTransferByteCount:
			isSupported = true;			
			*( UInt64 * ) value = 131072;	// Cap to 128k for old family
			break;
		
		default:
			break;
		
	}
	
	return isSupported;
	
}


//--------------------------------------------------------------------------------------
//	е HandleProtocolServiceFeature - doesn't handle any features, so return false
//--------------------------------------------------------------------------------------


bool
IOSCSIParallelInterfaceProtocolTransport::HandleProtocolServiceFeature (
											SCSIProtocolFeature feature, 
											void * serviceValue )
{
	return false;
}


#pragma mark -
#pragma mark Private Methods


//--------------------------------------------------------------------------------------
//	е sSCSICallbackProc	-	static callback routine which calls through to the virtual
//							routine
//--------------------------------------------------------------------------------------

void
IOSCSIParallelInterfaceProtocolTransport::sSCSICallbackProc ( void * target, void * refCon )
{
	
	SCSIClientData								clientData;
	SCSIClientData *							clientDataPtr	= NULL;
	IOSCSIParallelInterfaceProtocolTransport *	xpt				= NULL;
	IOSCSICommand *								cmd				= NULL;
	
	STATUS_LOG ( ( "IOSCSIParallelInterfaceProtocolTransport::sSCSICallbackProc entering.\n" ) );	
	
	xpt = ( IOSCSIParallelInterfaceProtocolTransport * ) target;
	clientDataPtr = ( SCSIClientData * ) refCon;
	assert ( clientDataPtr != NULL );
	
	cmd = clientDataPtr->cmd;
	bcopy ( clientDataPtr, &clientData, sizeof ( SCSIClientData ) );
		
	STATUS_LOG ( ( "IOSCSIParallelInterfaceProtocolTransport::sSCSICallbackProc calling virtual callback...\n" ) );	
	
	xpt->SCSICallbackFunction ( cmd, &clientData );
	
}


//---------------------------------------------------------------------------
// Fetch information about the SCSI device nub.
//---------------------------------------------------------------------------

bool
IOSCSIParallelInterfaceProtocolTransport::InspectDevice ( IOSCSIDevice * scsiDevice )
{
	
	OSObject *		obj		= NULL;
	OSDictionary *	dict	= NULL;
	bool			created	= false;
	
	obj = getProperty ( kIOPropertyProtocolCharacteristicsKey, gIOServicePlane );
	if ( obj != NULL )
	{
		
		dict = OSDynamicCast ( OSDictionary, obj );
		if ( dict != NULL )
		{
			
			dict = OSDictionary::withDictionary ( dict );
			if ( dict != NULL )
			{
				
				setProperty ( kIOPropertyProtocolCharacteristicsKey, dict );
				dict->release ( );
				dict = NULL;
				
			}
			
		}
		
	}
	
	obj = getProperty ( kIOPropertyProtocolCharacteristicsKey );
	if ( obj == NULL )
	{
		
		dict = OSDictionary::withCapacity ( 3 );
		created = true;
		
	}
	
	else
	{
		dict = OSDynamicCast ( OSDictionary, obj );
	}
	
	if ( dict != NULL )
	{
		
		OSNumber *	domainID	= NULL;
		OSNumber *	targetID 	= NULL;
		OSNumber *	LUN		 	= NULL;
		
		domainID = OSDynamicCast ( OSNumber, scsiDevice->getProperty ( kIOPropertySCSIDomainIdentifierKey, gIOServicePlane ) );
		if ( domainID != NULL )
		{
			dict->setObject ( kIOPropertySCSIDomainIdentifierKey, domainID );
		}
		
		targetID = OSDynamicCast ( OSNumber, scsiDevice->getProperty ( kSCSIPropertyTarget ) );
		if ( targetID != NULL )
		{
			dict->setObject ( kIOPropertySCSITargetIdentifierKey, targetID );
		}
		
		LUN = OSDynamicCast ( OSNumber, scsiDevice->getProperty ( kSCSIPropertyLun ) );
		if ( LUN != NULL )
		{
			dict->setObject ( kIOPropertySCSILogicalUnitNumberKey, LUN );
		}
		
	}
	
	if ( created == true )
	{
		
		setProperty ( kIOPropertyProtocolCharacteristicsKey, dict );
		dict->release ( );
		created = false;
		
	}
	
	return true;
	
}


//--------------------------------------------------------------------------------------
//	е AllocateSCSICommandObjects	-	allocates SCSI command objects
//--------------------------------------------------------------------------------------

void
IOSCSIParallelInterfaceProtocolTransport::AllocateSCSICommandObjects ( void )
{

	STATUS_LOG ( ( "IOSCSIParallelInterfaceProtocolTransport::AllocateSCSICommandObjects entering\n" ) );	
	
	IOSCSICommand *			cmd 		= NULL;
	SCSIClientData *		clientData	= NULL;
		
	for ( UInt32 index = 0; index < kSCSICommandPoolSize; index++ )
	{
		
		// Allocate the command
		cmd = fSCSIDevice->allocCommand ( fSCSIDevice, sizeof ( SCSIClientData ) );
		assert ( cmd != NULL );
		
		clientData = ( SCSIClientData * ) cmd->getClientData ( );
		assert ( clientData != NULL );
		
		clientData->senseBuffer = IOBufferMemoryDescriptor::withCapacity ( kMaxSenseDataSize,
																		   kIODirectionIn,
																		   true );
		
		STATUS_LOG ( ( "adding command to pool\n" ) );
		
		// Enqueue the command in the free list
		fCommandPool->returnCommand ( cmd );
		
	}
	
	STATUS_LOG ( ( "IOSCSIParallelInterfaceProtocolTransport::AllocateSCSICommandObjects exiting\n" ) );	
	
}


//--------------------------------------------------------------------------------------
//	е DellocateSCSICommandObjects	-	deallocates SCSI command objects
//--------------------------------------------------------------------------------------

void
IOSCSIParallelInterfaceProtocolTransport::DeallocateSCSICommandObjects ( void )
{
	
	STATUS_LOG ( ( "IOSCSIParallelInterfaceProtocolTransport::DeallocateSCSICommandObjects entering\n" ) );	
	
	IOSCSICommand *		cmd 		= NULL;
	SCSIClientData *	clientData	= NULL;
	
	cmd = ( IOSCSICommand * ) fCommandPool->getCommand ( false );
	assert ( cmd != NULL );
	
	//еее Walk the in-use queue and abort the commands (potential memory leak right now)
	
	
	// This handles walking the free command queue
	while ( cmd != NULL )
	{
		
		clientData = ( SCSIClientData * ) cmd->getClientData ( );
		assert ( clientData != NULL );
		
		// Release the memory descriptor attached to the request sense buffer
		clientData->senseBuffer->release ( );
		
		cmd->release ( );		
		cmd = ( IOSCSICommand * ) fCommandPool->getCommand ( false );
		
	}
	
	STATUS_LOG ( ( "IOSCSIParallelInterfaceProtocolTransport::DeallocateSCSICommandObjects exiting\n" ) );	
	
}


//--------------------------------------------------------------------------------------
//	е GetSCSICommandObject	-	Gets the first non-busy scsi command object
//								If they are all busy, it allocates more
//
//	NOTE: 	This routine MUST guarantee that an IOSCSICommand is returned to the caller
//			if blockForCommand is true.
//--------------------------------------------------------------------------------------

IOSCSICommand *
IOSCSIParallelInterfaceProtocolTransport::GetSCSICommandObject ( bool blockForCommand )
{
	
	IOSCSICommand *		cmd	= NULL;
	
	STATUS_LOG ( ( "IOSCSIParallelInterfaceProtocolTransport::GetSCSICommandObject entering.\n" ) );
	
	cmd = ( IOSCSICommand * ) fCommandPool->getCommand ( blockForCommand );
	
	return cmd;
	
}


//--------------------------------------------------------------------------------------
//	е ReturnSCSICommandObject	-	Returns the command to the command pool
//--------------------------------------------------------------------------------------

void
IOSCSIParallelInterfaceProtocolTransport::ReturnSCSICommandObject ( IOSCSICommand * cmd )
{
	
	STATUS_LOG ( ( "IOSCSIParallelInterfaceProtocolTransport::ReturnSCSICommandObject entering.\n" ) );
	
	assert ( cmd != NULL );
	fCommandPool->returnCommand ( cmd );
	
}


//---------------------------------------------------------------------------
// Handles messages from our provider.
//---------------------------------------------------------------------------

IOReturn
IOSCSIParallelInterfaceProtocolTransport::message ( UInt32 type, IOService * provider, void * argument )
{
	
	IOReturn ret = kIOReturnSuccess;
	
	ERROR_LOG ( ( "IOSCSIParallelInterfaceProtocolTransport::message %p %lx\n", this, type ) );
	
	switch ( type )
	{
		
		case kSCSIClientMsgBusReset:
			if ( !fBusResetInProgress )
			{
				
				ERROR_LOG ( ( "kSCSIClientMsgBusReset\n" ) );

				fBusResetInProgress = true;
				fSCSIDevice->holdQueue ( kQTypeNormalQ );
				
			}
			break;
		
		case ( kSCSIClientMsgBusReset | kSCSIClientMsgDone ):
			ERROR_LOG ( ( "kSCSIClientMsgBusResetDone\n" ) );
			fSCSIDevice->releaseQueue ( kQTypeNormalQ );
			fBusResetInProgress = false;
			break;
		
		default:
			ret = super::message ( type, provider, argument );
			break;
		
	}
	
	return ret;
	
}


//--------------------------------------------------------------------------------------
//							End				Of				File
//--------------------------------------------------------------------------------------
