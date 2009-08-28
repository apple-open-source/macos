/*
  File: AppleSCSIEmulatorAdapter.cpp

  Contains:

  Version: 1.0.0
  
  Copyright: Copyright (c) 2007 by Apple Inc., All Rights Reserved.

Disclaimer:IMPORTANT:  This Apple software is supplied to you by Apple Inc. 
("Apple") in consideration of your agreement to the following terms, and your use, 
installation, modification or redistribution of this Apple software constitutes acceptance 
of these terms.  If you do not agree with these terms, please do not use, install, modify or 
redistribute this Apple software.

In consideration of your agreement to abide by the following terms, and subject
to these terms, Apple grants you a personal, non-exclusive license, under Apple's
copyrights in this original Apple software (the "Apple Software"), to use, reproduce, 
modify and redistribute the Apple Software, with or without modifications, in source and/or
binary forms; provided that if you redistribute the Apple Software in its entirety
and without modifications, you must retain this notice and the following text
and disclaimers in all such redistributions of the Apple Software.  Neither the
name, trademarks, service marks or logos of Apple Inc. may be used to
endorse or promote products derived from the Apple Software without specific prior
written permission from Apple.  Except as expressly stated in this notice, no
other rights or licenses, express or implied, are granted by Apple herein,
including but not limited to any patent rights that may be infringed by your derivative
works or by other works in which the Apple Software may be incorporated.

The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO WARRANTIES, 
EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT,
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE
OR ITS USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS. IN NO EVENT SHALL APPLE 
BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA,
OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE,
REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED
AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT
LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/


//-----------------------------------------------------------------------------
//	Includes
//-----------------------------------------------------------------------------

#include <libkern/c++/OSArray.h>
#include <libkern/c++/OSData.h>
#include <libkern/c++/OSString.h>
#include <IOKit/IOTypes.h>
#include <IOKit/IOMessage.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#include <IOKit/storage/IOStorageProtocolCharacteristics.h>
#include <IOKit/scsi/spi/IOSCSIParallelInterfaceController.h>

#include "AppleSCSITargetEmulator.h"
#include "AppleSCSIEmulatorAdapter.h"
#include "AppleSCSIEmulatorEventSource.h"

//-----------------------------------------------------------------------------
//	Structures
//-----------------------------------------------------------------------------

typedef struct AdapterTargetStruct
{
	AppleSCSITargetEmulator *	emulator;
} AdapterTargetStruct;


//-----------------------------------------------------------------------------
//	Macros
//-----------------------------------------------------------------------------

#define DEBUG 												1
#define DEBUG_ASSERT_COMPONENT_NAME_STRING					"Adapter"

#if DEBUG
#define EMULATOR_ADAPTER_DEBUGGING_LEVEL					2
#endif

#include "DebugSupport.h"

#if ( EMULATOR_ADAPTER_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		panic x
#else
#define PANIC_NOW(x)		
#endif

#if ( EMULATOR_ADAPTER_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x; IOSleep(1)
#else
#define ERROR_LOG(x)		
#endif

#if ( EMULATOR_ADAPTER_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x; IOSleep(1)
#else
#define STATUS_LOG(x)		
#endif


// Define superclass
#define super IOSCSIParallelInterfaceController
OSDefineMetaClassAndStructors ( AppleSCSIEmulatorAdapter, IOSCSIParallelInterfaceController );

#define kMaxTargetID	256


//-----------------------------------------------------------------------------
//	ReportHBAConstraints
//-----------------------------------------------------------------------------

void
AppleSCSIEmulatorAdapter::ReportHBAConstraints (
	OSDictionary *		constraints )
{
	
	super::ReportHBAConstraints ( constraints );
#if USE_LUN_BYTES
	constraints->setObject ( kIOHierarchicalLogicalUnitSupportKey, kOSBooleanTrue );
#endif
	
}


//-----------------------------------------------------------------------------
//	ReportHBAHighestLogicalUnitNumber
//-----------------------------------------------------------------------------

SCSILogicalUnitNumber
AppleSCSIEmulatorAdapter::ReportHBAHighestLogicalUnitNumber ( void )
{
	
	// Report the highest LUN number devices on this HBA are allowed to have.
	// 0 is a valid response for HBAs that only allow a single LUN per device
	
	SCSILogicalUnitNumber maxLUN = 0x3FFF;
	
	STATUS_LOG ( ( "AppleSCSIEmulatorAdapter::ReportHBAHighestLogicalUnitNumber, maxLUN = %qd\n", maxLUN ) );
	
	return maxLUN;
	
}


//-----------------------------------------------------------------------------
//	DoesHBASupportSCSIParallelFeature
//-----------------------------------------------------------------------------

bool
AppleSCSIEmulatorAdapter::DoesHBASupportSCSIParallelFeature ( SCSIParallelFeature theFeature )
{
	STATUS_LOG ( ( "AppleSCSIEmulatorAdapter::DoesHBASupportSCSIParallelFeature\n" ) );
	return false;
}


//-----------------------------------------------------------------------------
//	InitializeTargetForID
//-----------------------------------------------------------------------------

bool
AppleSCSIEmulatorAdapter::InitializeTargetForID ( SCSITargetIdentifier targetID )
{	
	
	UInt32						index			= 0;
	UInt32						count			= 0;
	AppleSCSITargetEmulator *	emulator		= NULL;
	AdapterTargetStruct *		targetStruct	= NULL;
	bool						found			= false;
	
	// Link the emulator to the target.
	count = fTargetEmulators->getCount ( );
	for ( index = 0; index < count; index++ )
	{
		
		emulator = OSDynamicCast ( AppleSCSITargetEmulator, fTargetEmulators->getObject ( index ) );
		
		if ( emulator->GetTargetID ( ) == targetID )
		{
			
			targetStruct = ( AdapterTargetStruct * ) GetHBATargetDataPointer ( targetID );
			targetStruct->emulator = emulator;
			found = true;
			
		}
		
	}
	
	return found;
	
}


//-----------------------------------------------------------------------------
//	AbortTaskRequest
//-----------------------------------------------------------------------------

SCSIServiceResponse
AppleSCSIEmulatorAdapter::AbortTaskRequest ( 	
							SCSITargetIdentifier 		theT,
							SCSILogicalUnitNumber		theL,
							SCSITaggedTaskIdentifier	theQ )
{
	// Returning general failure for AbortTaskRequest as this isn't yet supported by our HBA

	return kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
}


//-----------------------------------------------------------------------------
//	AbortTaskSetRequest
//-----------------------------------------------------------------------------

SCSIServiceResponse
AppleSCSIEmulatorAdapter::AbortTaskSetRequest (
							SCSITargetIdentifier 		theT,
							SCSILogicalUnitNumber		theL )
{
	// Returning general failure for AbortTaskSetRequest as this isn't yet supported by our HBA

	return kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
}


//-----------------------------------------------------------------------------
//	ClearACARequest
//-----------------------------------------------------------------------------

SCSIServiceResponse
AppleSCSIEmulatorAdapter::ClearACARequest (
							SCSITargetIdentifier 		theT,
							SCSILogicalUnitNumber		theL )
{
	// Returning general failure for ClearACARequest as this isn't yet supported by our HBA

	return kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
}


//-----------------------------------------------------------------------------
//	ClearTaskSetRequest
//-----------------------------------------------------------------------------

SCSIServiceResponse
AppleSCSIEmulatorAdapter::ClearTaskSetRequest (
							SCSITargetIdentifier 		theT,
							SCSILogicalUnitNumber		theL )
{
	// Returning general failure for ClearTaskSetRequest as this isn't yet supported by our HBA

	return kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
}


//-----------------------------------------------------------------------------
//	LogicalUnitResetRequest
//-----------------------------------------------------------------------------

SCSIServiceResponse
AppleSCSIEmulatorAdapter::LogicalUnitResetRequest (
							SCSITargetIdentifier 		theT,
							SCSILogicalUnitNumber		theL )
{
	// Returning general failure for LogicalUnitResetRequest as this isn't yet supported by our HBA

	return kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
}


//-----------------------------------------------------------------------------
//	TargetResetRequest
//-----------------------------------------------------------------------------

SCSIServiceResponse
AppleSCSIEmulatorAdapter::TargetResetRequest (
							SCSITargetIdentifier		theT )
{
	// Returning general failure for TargetResetRequest as this isn't yet supported by our HBA

	return kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
}


//-----------------------------------------------------------------------------
//	ReportInitiatorIdentifier
//-----------------------------------------------------------------------------

SCSIInitiatorIdentifier
AppleSCSIEmulatorAdapter::ReportInitiatorIdentifier ( void )
{
	
	// What device ID does our HBA occupy on the bus?
	SCSIInitiatorIdentifier ourIdentity = kInitiatorID;
	
	STATUS_LOG ( ( "AppleSCSIEmulatorAdapter::ReportInitiatorIdentifier, ourIdentity = %qd\n", ourIdentity ) );
	
	return ourIdentity;
	
}


//-----------------------------------------------------------------------------
//	ReportHighestSupportedDeviceID
//-----------------------------------------------------------------------------

SCSIDeviceIdentifier
AppleSCSIEmulatorAdapter::ReportHighestSupportedDeviceID ( void )
{
	
	// This HBA can handle how many attached devices?  The actual number can be lower
	STATUS_LOG ( ( "AppleSCSIEmulatorAdapter::ReportHighestSupportedDeviceID, kMaxTargetID = %d\n", ( int ) kMaxTargetID ) );
	
	return kMaxTargetID;
	
}


//-----------------------------------------------------------------------------
//	ReportMaximumTaskCount
//-----------------------------------------------------------------------------

UInt32
AppleSCSIEmulatorAdapter::ReportMaximumTaskCount ( void )
{
	
	// How many concurrent tasks does our HBA support?
	// Given that this is the parallel tasking SCSI family, you'd
	// expect this to be greater than 1, but single task HBAs are
	// supported as well
	UInt32 		maxTasks = 512;
	
	STATUS_LOG ( ( "AppleSCSIEmulatorAdapter::ReportHighestSupportedDeviceID, maxTasks = %ld\n", maxTasks ) );
	
	return maxTasks;
	
}


//-----------------------------------------------------------------------------
//	ReportHBASpecificTaskDataSize
//-----------------------------------------------------------------------------

UInt32
AppleSCSIEmulatorAdapter::ReportHBASpecificTaskDataSize ( void )
{
	
	UInt32	taskDataSize = 0;
	
	// How much space do we need allocated internally for each task?
	taskDataSize = sizeof ( SCSIEmulatorRequestBlock );
	STATUS_LOG ( ( "AppleSCSIEmulatorAdapter::ReportHBASpecificTaskDataSize, taskDataSize = %ld\n", taskDataSize ) );

	return taskDataSize;
	
}


//-----------------------------------------------------------------------------
//	ReportHBASpecificDeviceDataSize
//-----------------------------------------------------------------------------

UInt32
AppleSCSIEmulatorAdapter::ReportHBASpecificDeviceDataSize ( void )
{
	
	// How much space do we need allocated internally for each attached device?
	UInt32 hbaDataSize = 0;
	
	hbaDataSize = sizeof ( AdapterTargetStruct );
	STATUS_LOG ( ( "AppleSCSIEmulatorAdapter::ReportHBASpecificDeviceDataSize, hbaDataSize = %ld\n", hbaDataSize ) );
	
	return hbaDataSize;
	
}


//-----------------------------------------------------------------------------
//	DoesHBAPerformDeviceManagement
//-----------------------------------------------------------------------------

bool
AppleSCSIEmulatorAdapter::DoesHBAPerformDeviceManagement ( void )
{
	
	STATUS_LOG ( ( "AppleSCSIEmulatorAdapter::DoesHBAPerformDeviceManagement\n" ) );
	return true;
	
}


//-----------------------------------------------------------------------------
//	SetControllerProperties
//-----------------------------------------------------------------------------

void
AppleSCSIEmulatorAdapter::SetControllerProperties ( void )
{

	OSString *	string	= NULL;
	OSData *	data	= NULL;
	UInt8		wwn[8];
	UInt8		addressID[3];
	
	string = OSString::withCString ( "Apple" );
	if ( string != NULL )
	{
		
		SetHBAProperty ( kIOPropertyVendorNameKey, string );
		string->release ( );
		string = NULL;
		
	}
	
	string = OSString::withCString ( "SCSI HBA Emulator" );
	if ( string != NULL )
	{
		
		SetHBAProperty ( kIOPropertyProductNameKey, string );
		string->release ( );
		string = NULL;
		
	}
	
	string = OSString::withCString ( "1.0" );
	if ( string != NULL )
	{
		
		SetHBAProperty ( kIOPropertyProductRevisionLevelKey, string );
		string->release ( );
		string = NULL;
		
	}
	
	string = OSString::withCString ( "Port 0" );
	if ( string != NULL )
	{
		
		SetHBAProperty ( kIOPropertyPortDescriptionKey, string );
		string->release ( );
		string = NULL;
		
	}
	
	string = OSString::withCString ( kIOPropertyPortSpeedAutomatic4GigabitKey );
	if ( string != NULL )
	{
		
		SetHBAProperty ( kIOPropertyPortSpeedKey, string );
		string->release ( );
		string = NULL;
		
	}
	
	string = OSString::withCString ( kIOPropertyPortTopologyAutomaticNPortKey );
	if ( string != NULL )
	{
		
		SetHBAProperty ( kIOPropertyPortTopologyKey, string );
		string->release ( );
		string = NULL;
		
	}
	
	wwn[0] = 0x10;
	wwn[1] = 0x00;
	wwn[2] = 0x00;
	wwn[3] = 0x40;
	wwn[4] = 0x50;
	wwn[5] = 0x60;
	wwn[6] = 0xBB;
	wwn[7] = 0xA0;
	
	data = OSData::withBytes ( wwn, sizeof ( wwn ) );
	if ( data != NULL )
	{
		
		SetHBAProperty ( kIOPropertyFibreChannelPortWorldWideNameKey, data );
		data->release ( );
		data = NULL;
		
	}
	
	wwn[0] = 0x20;
	wwn[1] = 0x00;
	wwn[2] = 0x00;
	wwn[3] = 0x40;
	wwn[4] = 0x50;
	wwn[5] = 0x60;
	wwn[6] = 0xBB;
	wwn[7] = 0xA0;
	
	data = OSData::withBytes ( wwn, sizeof ( wwn ) );
	if ( data != NULL )
	{
		
		SetHBAProperty ( kIOPropertyFibreChannelNodeWorldWideNameKey, data );
		data->release ( );
		data = NULL;
		
	}
	
	addressID[0] = 0;
	addressID[1] = 0;
	addressID[2] = 1;
	
	data = OSData::withBytes ( addressID, sizeof ( addressID ) );
	if ( data != NULL )
	{
		
		SetHBAProperty ( kIOPropertyFibreChannelAddressIdentifierKey, data );
		data->release ( );
		data = NULL;
		
	}
	
}


//-----------------------------------------------------------------------------
//	InitializeController
//-----------------------------------------------------------------------------

bool
AppleSCSIEmulatorAdapter::InitializeController ( void )
{
	
	IOReturn	status 	= kIOReturnSuccess;
	
	STATUS_LOG ( ( "+AppleSCSIEmulatorAdapter::InitializeController\n" ) );
	
	SetControllerProperties ( );
	
	// We don't have any real hardware to initialize in this example code since
	// we are a virtual HBA. So, we allocate only one thing:
	// An event source that we will use in lieu of an interrupt for our command
	// completions.
	fEventSource = AppleSCSIEmulatorEventSource::Create (
		this,
		OSMemberFunctionCast (
			AppleSCSIEmulatorEventSource::Action,
			this,
			&AppleSCSIEmulatorAdapter::TaskComplete ) );
	
	require_nonzero ( fEventSource, ErrorExit );
	
	status = GetWorkLoop ( )->addEventSource ( fEventSource );
	require_success ( status, ReleaseEventSource );
	
	fTargetEmulators = OSArray::withCapacity ( 1 );
	require_nonzero ( fTargetEmulators, RemoveEventSource );
	
	STATUS_LOG ( ( "-AppleSCSIEmulatorAdapter::InitializeController\n" ) );
	
	return true;
	

RemoveEventSource:
		
	
	GetWorkLoop ( )->removeEventSource ( fEventSource );
	
	
ReleaseEventSource:
	
	
	fEventSource->release ( );
	fEventSource = NULL;
	
	
ErrorExit:
	
	
	STATUS_LOG ( ( "-AppleSCSIEmulatorAdapter::InitializeController failed\n" ) );
	return false;
	
}


//-----------------------------------------------------------------------------
//	TerminateController
//-----------------------------------------------------------------------------

void
AppleSCSIEmulatorAdapter::TerminateController ( void )
{
	
	STATUS_LOG ( ( "+AppleSCSIEmulatorAdapter::TerminateController\n" ) );
	
	if ( fEventSource != NULL )
	{
		
		if ( GetWorkLoop ( )->removeEventSource ( fEventSource ) != kIOReturnSuccess )
		{
			ERROR_LOG ( ( "TerminateController: failed to de-register eventsource?\n" ) );
		}
		
		fEventSource->release ( );
		fEventSource = NULL;
		
	}
	
	if ( fTargetEmulators != NULL )
	{
		
		fTargetEmulators->release ( );
		fTargetEmulators = NULL;
		
	}
	
	STATUS_LOG ( ( "-AppleSCSIEmulatorAdapter::TerminateController\n" ) );
	
}


//-----------------------------------------------------------------------------
//	StartController
//-----------------------------------------------------------------------------

bool
AppleSCSIEmulatorAdapter::StartController ( void )
{
	
	// Start providing HBA services.  Re-init anything needed and go
	STATUS_LOG ( ( "AppleSCSIEmulatorAdapter::StartController\n" ) );

	return true;
	
}


//-----------------------------------------------------------------------------
//	StopController
//-----------------------------------------------------------------------------

void
AppleSCSIEmulatorAdapter::StopController ( void )
{
	
	// We've been requested to stop providing HBA services.  Cleanup and shut down
	STATUS_LOG ( ( "AppleSCSIEmulatorAdapter::StopController\n" ) );
	
}


//-----------------------------------------------------------------------------
//	HandleInterruptRequest
//-----------------------------------------------------------------------------

void
AppleSCSIEmulatorAdapter::HandleInterruptRequest ( void )
{
	
	// OK, odds are, your driver will want to do something here to get info from the HBA.
	// Usually, this will be pulling task completion info, etc. and then calling TaskCompleted().
	//
	// this->TaskCompleted(task, transportSucceeded, scsiStatus, transferredDataLength, senseData, senseDataLength);
	//
	
	// Since this example operates without using a real primary interrupt, this will never get called
	ERROR_LOG ( ( "HandleInterruptRequest: captured interrupt?" ) );
	
}


//-----------------------------------------------------------------------------
//	CreateDeviceInterrupt
//-----------------------------------------------------------------------------

IOInterruptEventSource *
AppleSCSIEmulatorAdapter::CreateDeviceInterrupt ( 
											IOInterruptEventSource::Action			action,
											IOFilterInterruptEventSource::Filter	filter,
											IOService *								provider )
{
	
	STATUS_LOG ( ( "AppleSCSIEmulatorAdapter::CreateDeviceInterrupt\n" ) );
	return NULL;
	
}


//-----------------------------------------------------------------------------
//	ProcessParallelTask
//-----------------------------------------------------------------------------

SCSIServiceResponse
AppleSCSIEmulatorAdapter::ProcessParallelTask ( SCSIParallelTaskIdentifier parallelRequest )
{
	
	SCSIServiceResponse 			ret 				= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	UInt8							transferDir			= GetDataTransferDirection ( parallelRequest );
	IOMemoryDescriptor *			transferMemDesc		= GetDataBuffer ( parallelRequest );
	UInt8							cdbLength			= GetCommandDescriptorBlockSize ( parallelRequest );
	SCSICommandDescriptorBlock  	cdbData				= { 0 };
	SCSITaskStatus					scsiStatus			= kSCSITaskStatus_No_Status;
	UInt64							dataLen				= 0;
	SCSI_Sense_Data					senseDataBuffer		= { 0 };
	UInt8							senseLength			= sizeof ( SCSI_Sense_Data );
	AdapterTargetStruct *			targetStruct		= NULL;
	SCSITargetIdentifier			targetID			= 0;
	SCSILogicalUnitNumber			logicalUnitNumber	= 0;
	
#if USE_LUN_BYTES
	SCSILogicalUnitBytes			logicalUnitBytes	= {  0 };
	GetLogicalUnitBytes ( parallelRequest, &logicalUnitBytes );
#endif /* USE_LUN_BYTES */
	
	targetID = GetTargetIdentifier ( parallelRequest );
	targetStruct = ( AdapterTargetStruct * ) GetHBATargetDataPointer ( targetID );
	
	GetCommandDescriptorBlock ( parallelRequest, &cdbData );
	
	if ( transferMemDesc && ( transferDir != kSCSIDataTransfer_NoDataTransfer ) )
	{
		dataLen = GetRequestedDataTransferCount ( parallelRequest );
	}

#if USE_LUN_BYTES	
	targetStruct->emulator->SendCommand ( cdbData, cdbLength, transferMemDesc, &dataLen, logicalUnitBytes, &scsiStatus, &senseDataBuffer, &senseLength );
#else
	logicalUnitNumber = GetLogicalUnitNumber ( parallelRequest );
	targetStruct->emulator->SendCommand ( cdbData, cdbLength, transferMemDesc, &dataLen, logicalUnitNumber, &scsiStatus, &senseDataBuffer, &senseLength );
#endif
	
	CompleteTaskOnWorkloopThread ( parallelRequest, true, scsiStatus, dataLen, &senseDataBuffer, senseLength );
	
	ret = kSCSIServiceResponse_Request_In_Process;
	return ret;
	
}


//-----------------------------------------------------------------------------
//	CompleteTaskOnWorkloopThread
//-----------------------------------------------------------------------------

void
AppleSCSIEmulatorAdapter::CompleteTaskOnWorkloopThread (
	SCSIParallelTaskIdentifier		parallelRequest,
	bool							transportSuccessful,
	SCSITaskStatus					scsiStatus,
	UInt64							actuallyTransferred,
	SCSI_Sense_Data *				senseBuffer,
	UInt8							senseLength )
{
	
	UInt8						transferDir				= GetDataTransferDirection ( parallelRequest );
	UInt64						transferSizeMax			= GetRequestedDataTransferCount ( parallelRequest );
	SCSIEmulatorRequestBlock *	srb						= ( SCSIEmulatorRequestBlock * ) GetHBADataPointer ( parallelRequest );
	
	if ( transportSuccessful && ( scsiStatus != kSCSITaskStatus_TASK_SET_FULL ) )
	{
		
		// set the realized transfer counts
		switch ( transferDir )
		{
			
			case kSCSIDataTransfer_FromTargetToInitiator:
			{
				
				if ( actuallyTransferred > transferSizeMax )
				{
					actuallyTransferred = transferSizeMax;
				}
				
				if ( !SetRealizedDataTransferCount ( parallelRequest, actuallyTransferred ) )
				{
					ERROR_LOG ( ( "CompleteTaskOnWorkloopThread: SetRealizedDataTransferCount (%d bytes) returned FAIL", actuallyTransferred ) );
				}
				break;
				
			}
			
			case kSCSIDataTransfer_FromInitiatorToTarget:
			{
				
				if ( actuallyTransferred > transferSizeMax )
				{
					actuallyTransferred = transferSizeMax;
				}
				
				if ( !SetRealizedDataTransferCount ( parallelRequest, actuallyTransferred ) )
				{
					ERROR_LOG ( ( "CompleteTaskOnWorkloopThread: SetRealizedDataTransferCount (%d bytes) returned FAIL", actuallyTransferred ) );
				}
				break;
				
			}
			
			case kSCSIDataTransfer_NoDataTransfer:
			default:
			{
				break;
			}
			
		}
		
	}
	
	if ( !transportSuccessful )
	{
		
		ERROR_LOG ( ( "CompleteTaskOnWorkloopThread: Failed transport - task = %p, transferDir = %d, transferSize = %lld, scsiStatus = 0x%X", parallelRequest, transferDir, transferSizeMax, scsiStatus ) );
		srb->fServiceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
		
	}
	
	else
	{
		
		srb->fServiceResponse = kSCSIServiceResponse_TASK_COMPLETE;
		
		// handle sense data in common fashion and complete the task
		if ( senseLength > 0 )
		{
			
			if ( !SetAutoSenseData ( parallelRequest, ( SCSI_Sense_Data * ) senseBuffer, senseLength ) )
			{
				ERROR_LOG ( ( "CompleteTaskOnWorkloopThread: Could not set sense data in parallel task" ) );
			}
			
		}
		
	}
	
	queue_init ( &srb->fQueueChain );
	srb->fParallelRequest = parallelRequest;
	srb->fTaskStatus = scsiStatus;
	fEventSource->AddItemToQueue ( srb );
	
}


//-----------------------------------------------------------------------------
//	TaskComplete
//-----------------------------------------------------------------------------

void
AppleSCSIEmulatorAdapter::TaskComplete (
							SCSIParallelTaskIdentifier parallelRequest )
{
	
	SCSIEmulatorRequestBlock *	srb = ( SCSIEmulatorRequestBlock * ) GetHBADataPointer ( parallelRequest );
	
	if ( srb != NULL )
	{
		CompleteParallelTask ( parallelRequest, srb->fTaskStatus, srb->fServiceResponse );
	}
	
}


//-----------------------------------------------------------------------------
//	CreateLUN
//-----------------------------------------------------------------------------

IOReturn
AppleSCSIEmulatorAdapter::CreateLUN (
	EmulatorTargetParamsStruct * 	targetParameters,
	task_t							task )
{
	
	IOReturn					status					= kIOReturnBadArgument;
	bool						result					= false;
	AppleSCSITargetEmulator *	target					= NULL;
	AdapterTargetStruct *		targetStruct			= NULL;
	IOMemoryDescriptor *		inquiryBuffer			= NULL;
	IOMemoryDescriptor *		inquiryPage00Buffer		= NULL;
	IOMemoryDescriptor *		inquiryPage80Buffer		= NULL;
	IOMemoryDescriptor *		inquiryPage83Buffer		= NULL;
	
	ERROR_LOG ( ( "AppleSCSIEmulatorAdapter::CreateTargetEmulator, targetID = %qd\n", targetParameters->targetID ) );
	
	require ( ( targetParameters->targetID != kInitiatorID ), ErrorExit );
	
	targetStruct = ( AdapterTargetStruct * ) GetHBATargetDataPointer ( targetParameters->targetID );
	
	if ( targetStruct == NULL )
	{
		
		target = AppleSCSITargetEmulator::Create ( targetParameters->targetID );
		require_nonzero_action ( target, ErrorExit, status = kIOReturnNoResources );
		
	}
	
	else
	{
		
		target = targetStruct->emulator;
		
	}
	
	ERROR_LOG ( ( "logicalUnit = %qd, capacity = %qd\n", targetParameters->lun.logicalUnit, targetParameters->lun.capacity ) );

	ERROR_LOG ( ( "lun.inquiryData = %qx\n", targetParameters->lun.inquiryData ) );
	ERROR_LOG ( ( "lun.inquiryPage00Data = %qx\n", targetParameters->lun.inquiryPage00Data ) );
	ERROR_LOG ( ( "lun.inquiryPage80Data = %qx\n", targetParameters->lun.inquiryPage80Data ) );
	ERROR_LOG ( ( "lun.inquiryPage83Data = %qx\n", targetParameters->lun.inquiryPage83Data ) );

	ERROR_LOG ( ( "lun.inquiryDataLength = %d\n", targetParameters->lun.inquiryDataLength ) );
	ERROR_LOG ( ( "lun.inquiryPage00DataLength = %d\n", targetParameters->lun.inquiryPage00DataLength ) );
	ERROR_LOG ( ( "lun.inquiryPage80DataLength = %d\n", targetParameters->lun.inquiryPage80DataLength ) );
	ERROR_LOG ( ( "lun.inquiryPage83DataLength = %d\n", targetParameters->lun.inquiryPage83DataLength ) );
	
	inquiryBuffer = IOMemoryDescriptor::withAddressRange (
		targetParameters->lun.inquiryData,
		targetParameters->lun.inquiryDataLength,
		0,
		task );
			
	inquiryPage00Buffer = IOMemoryDescriptor::withAddressRange (
		targetParameters->lun.inquiryPage00Data,
		targetParameters->lun.inquiryPage00DataLength,
		0,
		task );

	inquiryPage80Buffer = IOMemoryDescriptor::withAddressRange (
		targetParameters->lun.inquiryPage80Data,
		targetParameters->lun.inquiryPage80DataLength,
		0,
		task );

	inquiryPage83Buffer = IOMemoryDescriptor::withAddressRange (
		targetParameters->lun.inquiryPage83Data,
		targetParameters->lun.inquiryPage83DataLength,
		0,
		task );
	
	if ( inquiryBuffer != NULL )
	{
		inquiryBuffer->prepare ( );
	}

	if ( inquiryPage00Buffer != NULL )
	{
		inquiryPage00Buffer->prepare ( );
	}

	if ( inquiryPage80Buffer != NULL )
	{
		inquiryPage80Buffer->prepare ( );
	}

	if ( inquiryPage83Buffer != NULL )
	{
		inquiryPage83Buffer->prepare ( );
	}
	
	result = target->AddLogicalUnit (
		targetParameters->lun.logicalUnit,
		targetParameters->lun.capacity,
		inquiryBuffer,
		inquiryPage00Buffer,
		inquiryPage80Buffer,
		inquiryPage83Buffer );
	
	if ( inquiryBuffer != NULL )
	{
		inquiryBuffer->complete ( );
		inquiryBuffer->release ( );
	}	
	
	if ( inquiryPage00Buffer != NULL )
	{
		inquiryPage00Buffer->complete ( );
		inquiryPage00Buffer->release ( );
	}	
	
	if ( inquiryPage80Buffer != NULL )
	{
		inquiryPage80Buffer->complete ( );
		inquiryPage80Buffer->release ( );
	}	
	
	if ( inquiryPage83Buffer != NULL )
	{
		inquiryPage83Buffer->complete ( );
		inquiryPage83Buffer->release ( );
	}	
	
	// Did we just allocate this target?
	if ( targetStruct == NULL )
	{
		
		OSDictionary *	dict = NULL;
		
		// Yes. Make sure to add it to the list of target emulators.
		fTargetEmulators->setObject ( target );
		
		dict = OSDictionary::withCapacity ( 3 );
		if ( dict != NULL )
		{
			
			OSData *	data		= NULL;
			UInt8		nodeWWN[]	= { 0x60, 0x00, 0x00, 0x00, 0x12, 0x34, 0x56, 0x78 };
			UInt8		portWWN[]	= { 0x50, 0x00, 0x00, 0x00, 0x12, 0x34, 0x56, 0x78 };
			
			nodeWWN[1] = ( targetParameters->targetID >> 16 ) & 0xFF;
			nodeWWN[2] = ( targetParameters->targetID >>  8 ) & 0xFF;
			nodeWWN[3] = ( targetParameters->targetID >>  0 ) & 0xFF;
			
			data = OSData::withBytes ( nodeWWN, sizeof ( nodeWWN ) );
			if ( data != NULL )
			{
				dict->setObject ( kIOPropertyFibreChannelNodeWorldWideNameKey, data );
				data->release ( );
				data = NULL;
				
			}
			
			data = OSData::withBytes ( portWWN, sizeof ( portWWN ) );
			if ( data != NULL )
			{
				dict->setObject ( kIOPropertyFibreChannelPortWorldWideNameKey, data );
				data->release ( );
				data = NULL;
				
			}
			
		}
		
		// Last thing to do is allocate the target device.
		result = CreateTargetForID ( targetParameters->targetID, dict );
		
		target->release ( );
		target = NULL;
		
		dict->release ( );
		dict = NULL;
		
	}
	
	if ( result == true )
		status = kIOReturnSuccess;
	else
		status = kIOReturnError;
	
	
ErrorExit:
	
	
	return status;
	
}


//-----------------------------------------------------------------------------
//	DestroyLUN
//-----------------------------------------------------------------------------

IOReturn
AppleSCSIEmulatorAdapter::DestroyLUN (
	SCSITargetIdentifier	targetID,
	SCSILogicalUnitNumber	logicalUnit )
{
	
	AppleSCSITargetEmulator *	emulator		= NULL;
	AdapterTargetStruct *		targetStruct	= NULL;
	
	ERROR_LOG ( ( "AppleSCSIEmulatorAdapter::DestroyLUN, targetID = %qd, logicalUnit = %qd\n", targetID, logicalUnit ) );
	
	targetStruct = ( AdapterTargetStruct * ) GetHBATargetDataPointer ( targetID );
	require_nonzero ( targetStruct, ErrorExit );
	
	emulator = targetStruct->emulator;
	emulator->RemoveLogicalUnit ( logicalUnit );
	
	return kIOReturnSuccess;
	
	
ErrorExit:
	
	
	return kIOReturnError;
	
}


//-----------------------------------------------------------------------------
//	DestroyTarget
//-----------------------------------------------------------------------------

IOReturn
AppleSCSIEmulatorAdapter::DestroyTarget (
	SCSITargetIdentifier targetID )
{
	
	int							index		= 0;
	int							count		= 0;
	AppleSCSITargetEmulator *	emulator	= NULL;
	
	ERROR_LOG ( ( "AppleSCSIEmulatorAdapter::DestroyTarget, targetID = %qd\n", targetID ) );
	
	DestroyTargetForID ( targetID );
	
	// Release the emulator.
	count = fTargetEmulators->getCount ( );
	for ( index = 0; index < count; index++ )
	{
		
		emulator = OSDynamicCast ( AppleSCSITargetEmulator, fTargetEmulators->getObject ( index ) );
		
		if ( emulator->GetTargetID ( ) == targetID )
		{
			
			fTargetEmulators->removeObject ( index );
			break;
			
		}
		
	}
	
	return kIOReturnSuccess;
	
	
ErrorExit:
	
	
	return kIOReturnError;
	
}


//-----------------------------------------------------------------------------
//	AppleSCSIEmulatorDebugAssert
//-----------------------------------------------------------------------------

void
AppleSCSIEmulatorDebugAssert (
						const char * componentNameString,
						const char * assertionString, 
						const char * exceptionLabelString,
						const char * errorString,
						const char * fileName,
						long lineNumber,
						int errorCode )
{
	
	IOLog ( "%s Assert failed: %s ", componentNameString, assertionString );
	
	if ( exceptionLabelString != NULL )
		IOLog ( "%s ", exceptionLabelString );

	if ( errorString != NULL )
		IOLog ( "%s ", errorString );
	
	if ( fileName != NULL )
		IOLog ( "file: %s ", fileName );
	
	if ( lineNumber != 0 )
		IOLog ( "line: %ld ", lineNumber );
	
	if ( ( long ) errorCode != 0 )
		IOLog ( "error: %ld ", ( long ) errorCode );
	
	IOLog ( "\n" );
	IOSleep ( 1 );
	
}
