/*
 * Copyright (c) 2000-2007 Apple Inc. All rights reserved.
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

#include <libkern/OSAtomic.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOCommandGate.h>
#include <IOKit/IOWorkLoop.h>
#include <IOKit/ata/IOATATypes.h>
#include <IOKit/ata/IOATADevConfig.h>
#include <IOKit/ata/IOATABusInfo.h>
#include <IOKit/ata/IOATACommand.h>
#include <IOKit/IOSyncer.h>
#include <IOKit/scsi/SCSICommandOperationCodes.h>
#include <IOKit/scsi/SCSITask.h>
#include <IOKit/scsi/SCSITaskDefinition.h>			// Remove me when API is available for IsAutosenseRequested()

#include "IOATAPIProtocolTransport.h"

#include <IOKit/storage/ata/IOATAFamilyPriv.h>


#define ATAPI_PROTOCOL_TRANSPORT_DEBUGGING_LEVEL 0

#if ( ATAPI_PROTOCOL_TRANSPORT_DEBUGGING_LEVEL >= 1 )
#define	PANIC_NOW(x)			IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( ATAPI_PROTOCOL_TRANSPORT_DEBUGGING_LEVEL >= 2 )
#define	ERROR_LOG(x)			IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( ATAPI_PROTOCOL_TRANSPORT_DEBUGGING_LEVEL >= 3 )
#define	STATUS_LOG(x)			IOLog x
#else
#define STATUS_LOG(x)
#endif


// Timeout values used by the ATAPI Driver
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
	kMaxATAPIPacketSize					= 16,					// Max ATAPI packet size
	kATAPICommandLength 				= 12,					// ATAPI command length
	kATAPIIdentifyPacketDeviceDataSize	= 512					// 512 byte identify data
};

enum
{
	kATAPICheckConditionBit				= 0,
	kATAPIDeviceBusyBit					= 8
};

enum
{
	kATAPICheckConditionMask			= ( 1 << kATAPICheckConditionBit ),
	kATAPIDeviceBusyMask				= ( 1 << kATAPIDeviceBusyBit )
};

// Configuration state machine
enum
{
	kPIOTransferModeSetup	= 1,
	kPIOTransferModeDone	= 2,
	kDMATransferModeDone	= 3
};

struct ATAPIConfigData
{
	IOATAPIProtocolTransport *	self;
	UInt32						state;
	IOSyncer *					syncer;
};
typedef struct ATAPIConfigData ATAPIConfigData;

#define kIOATAPICommandPoolSize			1

enum
{
	kATAPICommandBusyBit			= 0,
	kATAPIRequestSenseNeededBit		= 1
};

enum
{
	kATAPICommandBusyMask			= ( 1 << kATAPICommandBusyBit ),
	kATAPIRequestSenseNeededMask	= ( 1 << kATAPIRequestSenseNeededBit )
};

enum
{
	kODDMediaNotifyValue0	= 0,
	kODDMediaNotifyValue1	= 1
};

#define fSemaphore			reserved->fSemaphore
#define fMediaNotifyValue	reserved->fMediaNotifyValue

#define super IOSCSIProtocolServices
OSDefineMetaClassAndStructors ( IOATAPIProtocolTransport, IOSCSIProtocolServices );

#pragma mark е Public Methods


//--------------------------------------------------------------------------------------
//	е init	-	Initialization
//--------------------------------------------------------------------------------------

bool
IOATAPIProtocolTransport::init ( OSDictionary * propTable )
{

	STATUS_LOG ( ( "IOATAPIProtocolTransport::init entering\n" ) );
		
	// Run this by our superclass
	if ( super::init ( propTable ) == false )
	{

		STATUS_LOG ( ( "IOATAPIProtocolTransport::init superclass init returned false\n" ) );
		return false;
	
	}
		
	STATUS_LOG ( ( "IOATAPIProtocolTransport::init returning true\n" ) );
	
	return true;
	
}


//--------------------------------------------------------------------------------------
//	е start	-	Start our services
//--------------------------------------------------------------------------------------

bool
IOATAPIProtocolTransport::start ( IOService * provider )
{
	
	IOReturn		theErr				= kIOReturnSuccess;
	IOWorkLoop *	workLoop			= NULL;
	OSDictionary *	dict				= NULL;
	IOService *		powerProvider		= NULL;
	OSNumber *		mediaNotifyValue	= NULL;
	
	STATUS_LOG ( ( "IOATAPIProtocolTransport::start called\n" ) );
	
	fATADevice 				= NULL;
	fATAUnitID				= kATAInvalidDeviceID;
	fATADeviceType			= kUnknownATADeviceType;
	fPhysicallyConnected 	= true;
	fPollingThread			= NULL;
	fResetInProgress		= false;
	
	reserved = IONew ( ExpansionData, 1 );
	if ( reserved == NULL )
	{
		return false;
	}
	bzero ( reserved, sizeof ( ExpansionData ) );
	
	dict = OSDynamicCast ( OSDictionary, getProperty ( kIOPropertyATAPIMassStorageCharacteristics ) );
	if ( dict != NULL )
	{
		
		OSString *	string = NULL;
		
		string = OSDynamicCast ( OSString, dict->getObject ( kATAVendorPropertyKey ) );
		if ( string != NULL )
		{
			
			const char * cString1 = NULL;
			const char * cString2 = NULL;
			
			cString1 = ( ( OSString * ) provider->getProperty ( kATAVendorPropertyKey ) )->getCStringNoCopy ( );
			cString2 = string->getCStringNoCopy ( );
			
			STATUS_LOG ( ( "device model = %s\n", cString1 ) );
			STATUS_LOG ( ( "ATAPI Device Characteristics device model = %s\n", cString2 ) );
			
			if ( strncmp ( cString1, cString2, string->getLength ( ) ) )
			{
				
				// Not a match to what is in the dictionary, so this workaround driver
				// should not be loaded. Short circuit out and let another driver attempt
				// to load.
				return false;
				
			}
			
		}
		
		else
		{
			STATUS_LOG ( ( "ATAPI Mass Storage dictionary has no device model string\n" ) );
		}
		
	}
	
	else
	{
		STATUS_LOG ( ( "No ATAPI Mass Storage dictionary\n" ) );
	}
	
	mediaNotifyValue = OSDynamicCast ( OSNumber, getProperty ( "media-notify", gIOServicePlane ) );
	
	if ( mediaNotifyValue != NULL )
	{
		fMediaNotifyValue = mediaNotifyValue->unsigned32BitValue ( );
	}
	else
	{
		fMediaNotifyValue = kODDMediaNotifyValue0;
	}
	
	// First call start() in our superclass
	if ( super::start ( provider ) == false )
		return false;
	
	// Cache our provider
	fATADevice = OSDynamicCast ( IOATADevice, provider );
	if ( fATADevice == NULL )
	{
		
		ERROR_LOG ( ( "Error in dynamic cast\n" ) );
		// Error in the dynamic cast, so get out
		return false;
		
	}
	
	// Find out if the device type is ATAPI
	if ( fATADevice->getDeviceType ( ) != ReportATAPIDeviceType ( ) )
	{
		
		ERROR_LOG ( ( "exiting, not an ATAPI device.\n" ) );
		return false;
		
	}
	
	// Open the thing below us
	if ( fATADevice->open ( this ) == false )
	{
		
		ERROR_LOG ( ( "device wouldn't open\n" ) );
		// It wouldn't open, so bail
		return false;
		
	}
	
	fATAUnitID 		= fATADevice->getUnitID ( );
	fATADeviceType 	= fATADevice->getDeviceType ( );
	
	STATUS_LOG ( ( "unit ID is %d\n", ( UInt8 ) fATAUnitID ) );
	STATUS_LOG ( ( "deviceType is %d\n", ( UInt8 ) fATADeviceType ) );
	
	bzero ( fDeviceIdentifyData, kATAPIIdentifyPacketDeviceDataSize );
	
	fDeviceIdentifyBuffer = IOMemoryDescriptor::withAddress ( ( void * ) fDeviceIdentifyData,
																kATAPIIdentifyPacketDeviceDataSize,
																kIODirectionIn );
	
	if ( fDeviceIdentifyBuffer == NULL )
	{
		
		ERROR_LOG ( ( "fDeviceIdentifyBuffer == NULL.\n" ) );
		goto CLOSE_DEVICE_ERROR;
		
	}
	
	// Create an IOCommandGate (for power management support) and attach
	// this event source to the provider's workloop
	fCommandGate = IOCommandGate::commandGate ( this );
	if ( fCommandGate == NULL )
	{
		
		ERROR_LOG ( ( "fCommandGate == NULL.\n" ) );
		goto RELEASE_IDENTIFY_DEVICE_BUFFER_ERROR;
		
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
	
	fCommandPool = IOCommandPool::commandPool ( this, workLoop, kIOATAPICommandPoolSize );
	if ( fCommandPool == NULL )
	{
		
		ERROR_LOG ( ( "fCommandPool == NULL.\n" ) );
		goto REMOVE_EVENT_SOURCE_ERROR;
		
	}
	
	fPollingThread = thread_call_allocate (
						( thread_call_func_t ) IOATAPIProtocolTransport::sPollStatusRegister,
						( thread_call_param_t ) this );
		
	if ( fPollingThread == NULL )
	{
		
		ERROR_LOG ( ( "fPollingThread allocation failed.\n" ) );
		goto RELEASE_COMMAND_POOL_ERROR;
		
	}
	
	// Pre-allocate some command objects	
	AllocateATACommandObjects ( );
	
	// Inspect the provider
	if ( InspectDevice ( fATADevice ) == false )
	{
		
		ERROR_LOG ( ( "InspectDevice returned false.\n" ) );
		goto DEALLOCATE_COMMANDS_ERROR;
		
	}
	
	// Initialize the power provider to default
	powerProvider = provider;
	powerProvider->retain ( );
	
	// Look to see if we are the slave device and there is a master
	// device on the bus.
	if ( fATAUnitID == kATADevice1DeviceID )
	{
		
		IOService *		obj;
		OSIterator *	iter;
		OSNumber *		deviceNumber;
		
		STATUS_LOG ( ( "We are the slave, find a master.\n" ) );
		
		// We are the slave. Find a master.
		obj = provider->getProvider ( );
		
		iter = obj->getChildIterator ( gIOServicePlane );
		if ( iter != NULL )
		{
			
			STATUS_LOG ( ( "Got an iterator.\n" ) );

			while ( ( obj = ( IOService * ) iter->getNextObject ( ) ) != NULL )
			{
				
				STATUS_LOG ( ( "Looping over objects.\n" ) );

				if ( obj == provider )
					continue;

				STATUS_LOG ( ( "Check the IOUnit property.\n" ) );
				
				deviceNumber = OSDynamicCast ( OSNumber, obj->getProperty ( "IOUnit" ) );
				if ( deviceNumber != NULL )
				{
					
					STATUS_LOG ( ( "Found the IOUnit property.\n" ) );

					if ( deviceNumber->unsigned8BitValue ( ) == kATADevice0DeviceID )
					{
						
						IOService *			possibleProvider 	= NULL;
						IOReturn			status 				= kIOReturnSuccess;
						mach_timespec_t		timeout				= { 5, 0 };
						
						// Wait upto 5 seconds for matching to finish on master device.
						status = obj->waitQuiet ( &timeout );
						if ( status == kIOReturnTimeout )
						{
							break;
						}
						
						// Find this object's child to get the item which is the master.
						possibleProvider = ( IOService * ) obj->getChildEntry ( gIOServicePlane );
						if ( possibleProvider != NULL )
						{
							
							STATUS_LOG ( ( "Found the master.\n" ) );
							
							// This is our new power provider.
							powerProvider->release ( );
							powerProvider = possibleProvider;
							powerProvider->retain ( );
							break;
							
						}
						
					}
					
				}
				
			}
			
			iter->release ( );
			
		}
		
	}
	
	InitializePowerManagement ( powerProvider );
	
	powerProvider->release ( );
	
	STATUS_LOG ( ( "IOATAPIProtocolTransport::start complete\n" ) );
	
	dict = OSDictionary::withCapacity ( 1 );
	if ( dict != NULL )
	{
	
		// Copy some properties into the dictionary.
		dict->setObject ( kATAUnitNumberKey, fATADevice->getProperty ( kATAUnitNumberKey ) );
		setProperty ( kIOPropertyProtocolCharacteristicsKey, dict );
		dict->release ( );
		
	}
	
	
	registerService ( );
	
	return true;

DEALLOCATE_COMMANDS_ERROR:
	
	DeallocateATACommandObjects ( );
	
RELEASE_COMMAND_POOL_ERROR:
	
	if ( fCommandPool != NULL )
		fCommandPool->release ( );
	
REMOVE_EVENT_SOURCE_ERROR:
	
	if ( workLoop != NULL )
		workLoop->removeEventSource ( fCommandGate );
	
RELEASE_COMMAND_GATE_ERROR:
	
	if ( fCommandGate != NULL )
		fCommandGate->release ( );
	
RELEASE_IDENTIFY_DEVICE_BUFFER_ERROR:
	
	if ( fDeviceIdentifyBuffer != NULL )
		fDeviceIdentifyBuffer->release ( );
	
CLOSE_DEVICE_ERROR:
	
	fATADevice->close ( this );
	return false;
	
}


//--------------------------------------------------------------------------------------
//	е stop	-	Stop our services
//--------------------------------------------------------------------------------------

void
IOATAPIProtocolTransport::stop ( IOService * provider )
{

	STATUS_LOG ( ( "IOATAPIProtocolTransport::stop called\n" ) );
	
	// Call super's stop
	super::stop ( provider );
	
}


//--------------------------------------------------------------------------------------
//	е free	-	Called to free any resources we allocated.
//--------------------------------------------------------------------------------------

void
IOATAPIProtocolTransport::free ( void )
{
	
	if ( fCommandPool != NULL )
	{
		
		fCommandPool->release ( );
		fCommandPool = NULL;
		
	}
	
	if ( reserved != NULL )
	{
		IODelete ( reserved, ExpansionData, 1 );
		reserved = NULL;
	}
	
	if ( fPollingThread != NULL )
	{
		
		thread_call_cancel ( fPollingThread );
		thread_call_free ( fPollingThread );
		fPollingThread = NULL;
		
	}
	
	super::free ( );
	
}


#pragma mark е Protected Methods


//--------------------------------------------------------------------------------------
//	е ReportATAPIDeviceType - Report the type of the device (ATA vs. ATAPI).
//--------------------------------------------------------------------------------------

ataDeviceType
IOATAPIProtocolTransport::ReportATAPIDeviceType ( void ) const
{
	
	return kATAPIDeviceType;
	
}


//--------------------------------------------------------------------------------------
//	е SendSCSICommand	-	Sends a SCSI Command to the provider bus
//--------------------------------------------------------------------------------------

bool
IOATAPIProtocolTransport::SendSCSICommand ( SCSITaskIdentifier request,
											SCSIServiceResponse * serviceResponse,
											SCSITaskStatus * taskStatus )
{
	
	SCSICommandDescriptorBlock 		cdb;
	UInt16 							commandLength		= 0;
	IOATACommand *					cmd					= NULL;
	ATAPIClientData *				clientData			= NULL;
	UInt16							atapiCommandLength	= kATAPICommandLength;
	UInt32							flags				= 0;
	UInt64							requestCount 		= 0;
	UInt32							timeoutDuration		= 0;
	bool							shouldUseDMA		= true;
	
	STATUS_LOG ( ( "IOATAPIProtocolTransport::SendSCSICommand called\n" ) );
    
	if ( OSBitOrAtomic ( kATAPICommandBusyMask, &fSemaphore ) & kATAPICommandBusyMask )
	{
		STATUS_LOG ( ( "Command in use, returning false\n" ) );
		return false;
	}
	
	if ( fSemaphore & kATAPIRequestSenseNeededMask )
	{
		
		STATUS_LOG ( ( "kATAPIRequestSenseNeededMask set\n" ) );
		
		if ( GetTaskExecutionMode ( request ) != kSCSITaskMode_Autosense )
		{
			
			STATUS_LOG ( ( "Not an autosense command, returning false.\n" ) );
			OSBitAndAtomic ( ~kATAPICommandBusyMask, &fSemaphore );
			return false;
			
		}
		
		STATUS_LOG ( ( "Clearing kATAPIRequestSenseNeededMask.\n" ) );
		OSBitAndAtomic ( ~kATAPIRequestSenseNeededMask, &fSemaphore );
		
	}
	
	// get command and context objects
	cmd = GetATACommandObject ( );
	
	clientData 			= ( ATAPIClientData * ) cmd->refCon;	
	*serviceResponse 	= kSCSIServiceResponse_Request_In_Process;
	*taskStatus			= kSCSITaskStatus_No_Status;
	
	if ( fPhysicallyConnected == false )
	{
		
		// device is disconnected - we can not service command request
		*serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
		ReturnATACommandObject ( cmd );
		OSBitAndAtomic ( ~kATAPICommandBusyMask, &fSemaphore );
		return false;
		
	}
	
	GetCommandDescriptorBlock ( request, &cdb );
	commandLength = GetCommandDescriptorBlockSize ( request );
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
	
	cmd->zeroCommand ( );
	
	// Start filling in the command
	cmd->setUnit ( fATAUnitID );
	cmd->setBuffer ( GetDataBuffer ( request ) );
	cmd->setPosition ( GetDataBufferOffset ( request ) );
	cmd->setByteCount ( GetRequestedDataTransferCount ( request ) );
	cmd->setCommand ( kPACKET );
	
	timeoutDuration = GetTimeoutDuration ( request );
	if ( timeoutDuration == 0 )
	{
		
		// Find out what the timeout duration is. Since timeouts of zero requested from
		// the layer above us mean the maximum timeout possible and ATA has no concept of
		// infinite timeouts on commands, set it to the max possible.
		timeoutDuration = 0xFFFFFFFF;
		
	}
	
	cmd->setTimeoutMS ( timeoutDuration );
	
	// Configure the flags for this command
	flags = mATAFlagProtocolATAPI | mATAFlagUseConfigSpeed /* | mATAFlagLEDEnable */;
	flags = flags | ( GetDataTransferDirection ( request ) == kSCSIDataTransfer_FromTargetToInitiator ? mATAFlagIORead : 0 );
	flags = flags | ( GetDataTransferDirection ( request ) == kSCSIDataTransfer_FromInitiatorToTarget ? mATAFlagIOWrite : 0 );
	
	requestCount = GetRequestedDataTransferCount ( request );
	
	// Check if this is an operation we should even use DMA on. This is really ugly, but it gains us some
	// performance on reads and writes.
	if ( ( cdb[0] == kSCSICmd_READ_6 )  || ( cdb[0] == kSCSICmd_READ_10 ) 	  || ( cdb[0] == kSCSICmd_READ_12 ) || 
		 ( cdb[0] == kSCSICmd_READ_CD ) || ( cdb[0] == kSCSICmd_READ_CD_MSF ) || ( cdb[0] == kSCSICmd_WRITE_AND_VERIFY_10 ) ||
		 ( cdb[0] == kSCSICmd_WRITE_6 ) || ( cdb[0] == kSCSICmd_WRITE_10 ) 	  || ( cdb[0] == kSCSICmd_WRITE_12 ) )
	{
		shouldUseDMA = true;
	}
	
	else
	{
		shouldUseDMA = false;
	}
	
	if ( ( GetDataTransferDirection ( request ) != kSCSIDataTransfer_NoDataTransfer ) &&
		 ( ( fUltraDMAMode | fDMAMode ) != 0 ) && shouldUseDMA )
	{
		
		UInt8	features = mATAPIuseDMA;
		
		flags = flags | mATAFlagUseDMA;
		
		// Set the features register
		cmd->setFeatures ( features );
		
	}
	
	cmd->setFlags ( flags );
	
	// Set the cylinder registers
	if ( GetRequestedDataTransferCount ( request ) != 0 )
	{
		
		UInt64		requestCount 	= GetRequestedDataTransferCount ( request );
		
		if ( requestCount >= 0x10000 )
		{
			
			// Cap the amount of PIO data that can be transferred in one interrupt
			// so we don't try to hog the cpu while doing PIO transfers.
			
			// Look and see if the caller is asking for 2352 byte transfers (CDDA)
			// if so, then use a multiple of 2352 bytes for the chunk size
			if ( ( requestCount % 2352 ) == 0 )
			{
				
				requestCount = ( 0xFFFF / 2352 ) * 2352;
				
			}
			
			// Caller is asking for non-CDDA data reads, so we use 62k to get the largest
			// size transfer less than 64k we possibly can which uses even block multiples
			else
			{
				
				requestCount = 0xF800;
			
			}
			
		}
		
		UInt8		requestHi		= ( requestCount & 0xFF00 ) >> 8;
		UInt8		requestLo		= requestCount & 0x00FF;
		
		cmd->setCylHi ( requestHi );
		cmd->setCylLo ( requestLo );
		
	}
	
	cmd->setOpcode ( kATAPIFnExecIO );
	// set the device head to the correct unit
	cmd->setDevice_Head ( fATAUnitID << 4 );
	cmd->setRegMask ( ( ataRegMask ) ( mATAErrFeaturesValid | mATAStatusCmdValid ) );
	
	IOReturn theErr = cmd->setPacketCommand ( atapiCommandLength, ( UInt8 * ) cdb );
	if ( theErr != kATANoErr )
	{
		
		STATUS_LOG ( ( "IOATAPIProtocolTransport::SendSCSICommand setPacketCommand returned error = %ld\n", theErr ) );
		
	}
		
	// Setup our context
	clientData->self 		= this;
	clientData->scsiTask 	= request;
	
	cmd->setCallbackPtr ( &sSCSITaskCallbackProc );
		
	fATADevice->executeCommand ( cmd );
	
	return true;
	
}


//--------------------------------------------------------------------------------------
//	е SCSITaskCallbackFunction	-	virtual callback routine which calls CompleteSCSITask
//--------------------------------------------------------------------------------------

void
IOATAPIProtocolTransport::SCSITaskCallbackFunction ( IOATACommand * cmd,
													 SCSITaskIdentifier scsiTask )
{
	
	ATAPIClientData *	clientData	= NULL;
	IOReturn			result;
	UInt64				bytesTransferred;
	
	STATUS_LOG ( ( "IOATAPIProtocolTransport::SCSITaskCallbackFunction entering\n" ) );	
	
	clientData = ( ATAPIClientData * ) cmd->refCon;
	
	result 				= cmd->getResult ( );
	bytesTransferred	= cmd->getActualTransfer ( );
	
	ReturnATACommandObject ( cmd );
	
	switch ( result )
	{
		
		case kATANoErr:
			{
				
				STATUS_LOG ( ( "IOATAPIProtocolTransport::SCSITaskCallbackFunction result = noErr\n" ) );	
				SetRealizedDataTransferCount ( scsiTask, bytesTransferred );
				CompleteSCSITask ( 	scsiTask,
									kSCSIServiceResponse_TASK_COMPLETE,
									kSCSITaskStatus_GOOD );
				
			}
			break;
		
		case kATAErrDevBusy:
		case kATATimeoutErr:
			{
				
				SCSITaskStatus		taskStatus = kSCSITaskStatus_No_Status;
				
				if ( result == kATATimeoutErr )
					taskStatus = kSCSITaskStatus_TaskTimeoutOccurred;
				
				else if ( result == kATAErrDevBusy )
					taskStatus = kSCSITaskStatus_DeviceNotResponding;
				
				// Reset the device because the device is hung
				clientData->self->ResetATAPIDevice ( );
				SetRealizedDataTransferCount ( scsiTask, bytesTransferred );
				CompleteSCSITask ( 	scsiTask,
									kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE,
									taskStatus );
				
				// Since we reset the device, message the upper layer to check its configuration
				// and do anything it needs to do
				SendNotification_VerifyDeviceState ( );
				
				STATUS_LOG ( ( "IOATAPIProtocolTransport::SCSITaskCallbackFunction result = %ld.\n", result ) );
				
			}
			break;
			
		case kATADeviceError:
			{
				
				// CHK bit is set, so the device indicates CheckCondition
				SetRealizedDataTransferCount ( scsiTask, bytesTransferred );
				
				SCSITask *	request = OSDynamicCast ( SCSITask, scsiTask );
				
				if ( request->IsAutosenseRequested ( ) == true )
				{
					OSBitOrAtomic ( kATAPIRequestSenseNeededMask, &fSemaphore );
				}
				
				CompleteSCSITask ( 	scsiTask,
									kSCSIServiceResponse_TASK_COMPLETE,
									kSCSITaskStatus_CHECK_CONDITION );
				
				STATUS_LOG ( ( "IOATAPIProtocolTransport::SCSITaskCallbackFunction result = %ld.\n", result ) );
				
			}
			break;
			
		case kATAModeNotSupported:
		case kATADevIntNoCmd:
		case kATADMAErr:
		default:
			{
				
				STATUS_LOG ( ( "IOATAPIProtocolTransport::SCSITaskCallbackFunction result = %ld.\n", result ) );
				SetRealizedDataTransferCount ( scsiTask, bytesTransferred );
				CompleteSCSITask ( 	scsiTask,
									kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE,
									kSCSITaskStatus_DeliveryFailure );
								
			}
			break;
			
	}
	
}


//--------------------------------------------------------------------------------------
//	е CompleteSCSITask	-	Called to complete a SCSI Command
//--------------------------------------------------------------------------------------

void
IOATAPIProtocolTransport::CompleteSCSITask ( 	SCSITaskIdentifier	request, 
												SCSIServiceResponse	serviceResponse,
												SCSITaskStatus		taskStatus )
{
	
	STATUS_LOG ( ( "IOATAPIProtocolTransport::CompleteSCSITask called\n" ) );
	
	OSBitAndAtomic ( ~kATAPICommandBusyMask, &fSemaphore );
	CommandCompleted ( request, serviceResponse, taskStatus );
	
}


//--------------------------------------------------------------------------------------
//	е AbortSCSICommand	-	Aborts a SCSI Command
//--------------------------------------------------------------------------------------

SCSIServiceResponse
IOATAPIProtocolTransport::AbortSCSICommand ( SCSITaskIdentifier request )
{
	return kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
}


//--------------------------------------------------------------------------------------
//	е IsProtocolServiceSupported	-	Returns true if feature is supported
//--------------------------------------------------------------------------------------

bool
IOATAPIProtocolTransport::IsProtocolServiceSupported ( SCSIProtocolFeature feature,
													   void * serviceValue )
{
	
	bool	isSupported = false;
	
	STATUS_LOG ( ( "IOATAPIProtocolTransport::IsProtocolServiceSupported called\n" ) );
	
	switch ( feature )
	{
				
		case kSCSIProtocolFeature_ProtocolSpecificPolling:			
			// ATAPI supports low-power polling.
			isSupported = true;
			break;
		
		case kSCSIProtocolFeature_ProtocolSpecificSleepCommand:			
			// ATAPI supports ATA SLEEP command.
			isSupported = true;
			break;
			
		case kSCSIProtocolFeature_ProtocolSpecificPowerOff:
			// does platform support power off?
			isSupported = ( fMediaNotifyValue != kODDMediaNotifyValue0 );
			break;
		
		case kSCSIProtocolFeature_ACA:
			// ATAPI does not support Auto Contingent Allegiance
		case kSCSIProtocolFeature_CPUInDiskMode:
			// ATAPI does not support cpu in disk mode
		case kSCSIProtocolFeature_GetMaximumLogicalUnitNumber:
			// ATAPI does not support more than one logical unit.
		default:
			// Some other feature ATAPI doesn't know about, probably means
			// it isn't supported...
			break;
		
	}
	
	return isSupported;
	
}


//--------------------------------------------------------------------------------------
//	е HandleProtocolServiceFeature	-	Returns true if feature is handled successfully
//--------------------------------------------------------------------------------------

bool
IOATAPIProtocolTransport::HandleProtocolServiceFeature ( SCSIProtocolFeature feature,
														 void * serviceValue )
{
	
	bool	isSupported = false;
	
	STATUS_LOG ( ( "IOATAPIProtocolTransport::HandleProtocolServiceFeature called\n" ) );
	
	switch ( feature )
	{
		
		case kSCSIProtocolFeature_ProtocolSpecificPolling:
			// We╒re being told to do protocol specific polling
			if ( serviceValue != NULL )
			{
				
				UInt32	value = *( UInt32 * ) serviceValue;
				
				if ( value != 0 )
				{
					
					// start low power polling
					
					bool	resetOccurred = false;
					
					STATUS_LOG ( ( "Enabling polling of ATA Status register\n" ) );					
					
					fCommandGate->runAction ( ( IOCommandGate::Action )
											  &IOATAPIProtocolTransport::sSetWakeupResetOccurred,
											  ( void * ) resetOccurred );
					
					EnablePollingOfStatusRegister ( );
					isSupported = true;
					
				}
				
				if ( value == 0 )
				{
				
					// stop low power polling
					
					STATUS_LOG ( ( "Disabling polling of ATA Status register\n" ) );					
									
					DisablePollingOfStatusRegister ( );
					isSupported = true;
					
				}
				
			}
			
			break;
		
		case kSCSIProtocolFeature_ProtocolSpecificSleepCommand:
			
			// We╒re being told to do protocol specific sleep
			
			if ( serviceValue != NULL )
			{
				
				UInt32	value = *( UInt32 * ) serviceValue;
				
				if ( value != 0 )
				{
					
					STATUS_LOG ( ( "Sending ATA sleep command\n" ) );
					
					( void ) SendATASleepCommand ( );
					isSupported = true;
					
				}
				
			}
			
			break;
			
		case kSCSIProtocolFeature_ProtocolSpecificPowerOff:
			
			// We╒re being told to cut power to the drive OFF
			
			( void ) TurnDrivePowerOff ( );
			
			break;
			
		default:
			
			break;
		
	}
	
	return isSupported;
	
}


//--------------------------------------------------------------------------------------
//	е HandlePowerOn	- Power management routine to handle power state transition
//--------------------------------------------------------------------------------------


IOReturn
IOATAPIProtocolTransport::HandlePowerOn ( void )
{
	
	IOReturn	status		= kIOReturnSuccess;
	bool		resetOccurred = false;
	
	
	STATUS_LOG ( ( "%s%s::%s called%s\n", "\033[36m",
				   getName ( ), __FUNCTION__, "\033[0m" ) );	
	
	fCommandGate->runAction ( ( IOCommandGate::Action )
							  &IOATAPIProtocolTransport::sCheckWakeupResetOccurred,
							  ( void * ) &resetOccurred );
	
	if ( !resetOccurred )
	{
		
		STATUS_LOG ( ( "%s fWakeUpResetOccurred is false, resetting device %s\n",
							"\033[36m", getName ( ), __FUNCTION__, "\033[0m" ) );	
		
		// We aren't on a shared bus, so we need to reset the device
		status = ResetATAPIDevice ( );
		
	}
	
	else
	{
		
		STATUS_LOG ( ( "%s fWakeUpResetOccurred is true, NO reset needed %s\n",
				"\033[36m", getName ( ), __FUNCTION__, "\033[0m" ) );	
		
	}
	
	return status;
	
}


//--------------------------------------------------------------------------------------
//	е HandlePowerOff - Power managment routine to handle power state transition
//--------------------------------------------------------------------------------------


IOReturn
IOATAPIProtocolTransport::HandlePowerOff ( void )
{
	
	IOReturn		status = kIOReturnSuccess;
	bool			resetOccurred = false;
	
	fCommandGate->runAction ( ( IOCommandGate::Action )
							  &IOATAPIProtocolTransport::sSetWakeupResetOccurred,
							  ( void * ) resetOccurred );
	
	return status;
	
}


//--------------------------------------------------------------------------------------
//	е sSCSITaskCallbackProc	-	static callback routine which calls through to the virtual
//								routine
//--------------------------------------------------------------------------------------

void
IOATAPIProtocolTransport::sSCSITaskCallbackProc ( IOATACommand * cmd )
{

	SCSITaskIdentifier			scsiTask;	
	ATAPIClientData				clientData;
	IOATAPIProtocolTransport *	self = NULL;

	STATUS_LOG ( ( "IOATAPIProtocolTransport::ATACallbackProc entering.\n" ) );	
	
	// Pull the clientData out of the command
	bcopy ( cmd->refCon, &clientData, sizeof ( clientData ) );
	
	// Get the scsiTask and a pointer to self from the clientData
	scsiTask 	= clientData.scsiTask;
	self 		= clientData.self;
	
	STATUS_LOG ( ( "IOATAPIProtocolTransport::ATACallbackProc calling virtual callback...\n" ) );	
	
	// Call through to virtual callback
	self->SCSITaskCallbackFunction ( cmd, scsiTask );
	
}


//---------------------------------------------------------------------------
// е InspectDevice - Fetch information about the ATAPI device nub.
//---------------------------------------------------------------------------

bool
IOATAPIProtocolTransport::InspectDevice ( IOATADevice * ataDevice )
{
	
	OSString *		string			= NULL;
	IOReturn		theErr			= kIOReturnSuccess;
	
	// Fetch ATA device information from the nub.
	string = OSDynamicCast ( 	OSString,
								ataDevice->getProperty ( kATAVendorPropertyKey ) );
	
	if ( string != NULL )
	{
		
		strncpy ( fModel, string->getCStringNoCopy ( ), kSizeOfATAModelString );
		fModel[kSizeOfATAModelString] = '\0';
				
	}
	
	string = OSDynamicCast ( 	OSString,
								ataDevice->getProperty ( kATARevisionPropertyKey ) );
	
	if ( string != NULL )
	{
		
		strncpy ( fRevision, string->getCStringNoCopy ( ), kSizeOfATARevisionString );
		fRevision[kSizeOfATARevisionString] = '\0';
		
	}
	
	theErr = IdentifyAndConfigureATAPIDevice ( );
	
	if ( theErr != kIOReturnSuccess )
	{
		
		STATUS_LOG ( ( "IOATAPIProtocolTransport::IdentifyAndConfigureATAPIDevice theErr = %ld\n", ( UInt32 ) theErr ) );
		return false;
		
	}
	
	return true;
	
}


//--------------------------------------------------------------------------------------
//	е sATACallbackSync	-	static synchronous callback routine
//--------------------------------------------------------------------------------------

void
IOATAPIProtocolTransport::sATACallbackSync ( IOATACommand * cmd )
{
	
	IOSyncer *		syncer	= NULL;
	
	STATUS_LOG ( ( "IOATAPIProtocolTransport::sATACallbackSync entering\n" ) );	
	
	if ( cmd->getResult ( ) != kATANoErr )
	{
		
		STATUS_LOG ( ( "Command result error = %ld\n", cmd->getResult ( ) ) );
		
	}
	
	syncer = ( IOSyncer * ) cmd->refCon;
	assert ( syncer );
	
	STATUS_LOG ( ( "IOATAPIProtocolTransport::sATACallbackSync signalling syncer\n" ) );	
	
	syncer->signal ( );
	
}


//--------------------------------------------------------------------------------------
//	е sATAPIConfigStateMachine	-	state machine for configuration commands
//--------------------------------------------------------------------------------------

void
IOATAPIProtocolTransport::sATAPIConfigStateMachine ( IOATACommand * cmd )
{
	
	ATAPIConfigData *				configData;
	IOATAPIProtocolTransport *		driver;
	IOReturn						status;
		
	STATUS_LOG ( ( "IOATAPIProtocolTransport::sATAPIConfigStateMachine entering\n" ) );	
	
	configData 	= ( ATAPIConfigData * ) cmd->refCon;
	status		= cmd->getResult ( );
	driver		= configData->self;
	
	switch ( configData->state )
	{
		
		case kPIOTransferModeSetup:
			configData->state = kPIOTransferModeDone;
			driver->SetPIOTransferMode ( cmd, false );
			break;
		
		case kPIOTransferModeDone:
			if ( ( driver->fUltraDMAMode != 0 ) || ( driver->fDMAMode != 0 ) )
			{
				configData->state = kDMATransferModeDone;
				driver->SetDMATransferMode ( cmd, false );
				break;
			}

		// Intentional fall through	in case device doesn't support DMA
		case kDMATransferModeDone:
			configData->syncer->signal ( kIOReturnSuccess, false );
			break;
			
		default:
			PANIC_NOW ( ( "sATAPIConfigStateMachine unexpected state\n" ) );
			break;
		
	}
		
}


//--------------------------------------------------------------------------------------
//	е sATAPIResetCallback	-	static asynchronous callback routine for resets
//--------------------------------------------------------------------------------------

void
IOATAPIProtocolTransport::sATAPIResetCallback ( IOATACommand * cmd )
{
	
	IOATAPIProtocolTransport *	xptDriver;
	
	STATUS_LOG ( ( "IOATAPIProtocolTransport::sATAPIResetCallback entering\n" ) );	
	
	xptDriver = ( IOATAPIProtocolTransport * ) cmd->refCon;
	xptDriver->fWakeUpResetOccurred = true;
	xptDriver->fResetInProgress 	= false;
	
}


//--------------------------------------------------------------------------------------
//	е sATAPIVoidCallback	-	callback that does nothing
//--------------------------------------------------------------------------------------

void
IOATAPIProtocolTransport::sATAPIVoidCallback ( IOATACommand * cmd )
{
	return;	
}


//--------------------------------------------------------------------------------------
//	е sPollStatusRegister	-	Callout method for thread_call_enter_delayed.
//--------------------------------------------------------------------------------------

void
IOATAPIProtocolTransport::sPollStatusRegister ( void * driver, void * refCon )
{
	
	IOATAPIProtocolTransport *	xptDriver;
	
	xptDriver = ( IOATAPIProtocolTransport * ) driver;
	xptDriver->PollStatusRegister ( refCon );
	
}


//--------------------------------------------------------------------------------------
//	е sPollStatusRegisterCallback	-	Callback method for PollStatusRegister().
//--------------------------------------------------------------------------------------

void
IOATAPIProtocolTransport::sPollStatusRegisterCallback ( IOATACommand * cmd )
{
	
	IOATAPIProtocolTransport *		xptDriver		= NULL;
	ATAPIClientData *				clientData		= NULL;
	
	clientData = ( ATAPIClientData * ) cmd->refCon;
	xptDriver = ( IOATAPIProtocolTransport * ) clientData->self;
	
	xptDriver->PollStatusRegisterCallback ( cmd );
	
}


//--------------------------------------------------------------------------------------
//	е AllocateATACommandObjects	-	allocates ATA command objects
//--------------------------------------------------------------------------------------

void
IOATAPIProtocolTransport::AllocateATACommandObjects ( void )
{
	
	STATUS_LOG ( ( "IOATAPIProtocolTransport::AllocateATACommandObjects entering\n" ) );	
	
	IOATACommand *			cmd 		= NULL;
	ATAPIClientData *		clientData 	= NULL;
	ATAPIConfigData *		configData	= NULL;
	
	// First allocate our reserve command
	fResetCommand = fATADevice->allocCommand ( );
	assert ( fResetCommand != NULL );
	
	fConfigCommand = fATADevice->allocCommand ( );
	assert ( fConfigCommand != NULL );
	configData = ( ATAPIConfigData * ) IOMalloc ( sizeof ( ATAPIConfigData ) );
	assert ( configData != NULL );
	bzero ( configData, sizeof ( ATAPIConfigData ) );
	configData->syncer = IOSyncer::create ( );
	assert ( configData->syncer != NULL );
	fConfigCommand->refCon = ( void * ) configData;
		
	fIdentifyCommand = fATADevice->allocCommand ( );
	assert ( fIdentifyCommand != NULL );
	
	clientData = ( ATAPIClientData * ) IOMalloc ( sizeof ( ATAPIClientData ) );
	bzero ( clientData, sizeof ( ATAPIClientData ) );
	fIdentifyCommand->refCon = ( void * ) clientData;
	
	for ( UInt32 index = 0; index < kIOATAPICommandPoolSize; index++ )
	{
		
		// Allocate the command
		cmd = fATADevice->allocCommand ( );
		assert ( cmd != NULL );
		
		// Allocate the command clientData
		clientData = ( ATAPIClientData * ) IOMalloc ( sizeof ( ATAPIClientData ) );
		assert ( clientData != NULL );
		bzero ( clientData, sizeof ( ATAPIClientData ) );
		
		// set the back pointers to each other
		cmd->refCon 	= ( void * ) clientData;
		clientData->cmd	= cmd;
		
		STATUS_LOG ( ( "adding command to pool\n" ) );
		
		// Enqueue the command in the free list
		fCommandPool->returnCommand ( cmd );
		
	}
	
	STATUS_LOG ( ( "IOATAPIProtocolTransport::AllocateATACommandObjects exiting\n" ) );	
	
}


//--------------------------------------------------------------------------------------
//	е DeallocateATACommandObjects	-	deallocates ATA command objects
//--------------------------------------------------------------------------------------

void
IOATAPIProtocolTransport::DeallocateATACommandObjects ( void )
{
	
	STATUS_LOG ( ( "IOATAPIProtocolTransport::DellocateATACommandObjects entering\n" ) );	
	
	IOATACommand *		cmd 		= NULL;
	ATAPIClientData *	clientData 	= NULL;
	ATAPIConfigData *	configData	= NULL;
	
	cmd = ( IOATACommand * ) fCommandPool->getCommand ( false );
	assert ( cmd != NULL );
	
	//еее Walk the in-use queue and abort the commands (potential memory leak right now)
	
	
	// This handles walking the free command queue
	while ( cmd != NULL )
	{
		
		clientData = ( ATAPIClientData * ) cmd->refCon;
		assert ( clientData != NULL );
		
		IOFree ( clientData, sizeof ( ATAPIClientData ) );
		clientData = NULL;
		
		fATADevice->freeCommand ( cmd );
		cmd = NULL;
		
		cmd = ( IOATACommand * ) fCommandPool->getCommand ( false );
		
	}
	
	configData = ( ATAPIConfigData * ) fConfigCommand->refCon;
	assert ( configData != NULL );
	configData->syncer->release ( );
	IOFree ( configData, sizeof ( ATAPIConfigData ) );
	configData = NULL;
	
	clientData = ( ATAPIClientData * ) fIdentifyCommand->refCon;
	assert ( clientData != NULL );
	IOFree ( clientData, sizeof ( ATAPIClientData ) );
	clientData = NULL;
	
	// release "special" comands
	fATADevice->freeCommand ( fConfigCommand );
	fATADevice->freeCommand ( fResetCommand );
	fATADevice->freeCommand ( fIdentifyCommand );

	fConfigCommand 			= NULL;
	fResetCommand 			= NULL;
	fIdentifyCommand		= NULL;
	
	STATUS_LOG ( ( "IOATAPIProtocolTransport::DellocateATACommandObjects exiting\n" ) );	
	
}


//--------------------------------------------------------------------------------------
//	е GetATACommandObject	-	Gets an ata command object from the pool.
//--------------------------------------------------------------------------------------

IOATACommand *
IOATAPIProtocolTransport::GetATACommandObject ( bool blockForCommand )
{
	
	IOATACommand *		cmd	= NULL;
	
	STATUS_LOG ( ( "IOATAPIProtocolTransport::GetATACommandObject entering.\n" ) );
	
	cmd = ( IOATACommand * ) fCommandPool->getCommand ( blockForCommand );
	
	return cmd;
	
}


//--------------------------------------------------------------------------------------
//	е ReturnATACommandObject	-	Returns the command to the command pool
//--------------------------------------------------------------------------------------

void
IOATAPIProtocolTransport::ReturnATACommandObject ( IOATACommand * cmd )
{
	
	STATUS_LOG ( ( "IOATAPIProtocolTransport::ReturnATACommandObject entering.\n" ) );
	
	assert ( cmd != NULL );
	fCommandPool->returnCommand ( cmd );
	
}


//--------------------------------------------------------------------------------------
//	е IdentifyAndConfigureATAPIDevice	-	Sends a device identify request to the device
//											and uses it to configure the drive speeds
//--------------------------------------------------------------------------------------

IOReturn
IOATAPIProtocolTransport::IdentifyAndConfigureATAPIDevice ( void )
{
	
	IOReturn						theErr				= kIOReturnSuccess;
	IOATABusInfo *					busInfoPtr			= NULL;
	IOATADevConfig *				deviceConfigPtr		= NULL;
	OSDictionary *					dict				= NULL;
	
	// Get some info about the ATA bus
	busInfoPtr = IOATABusInfo::atabusinfo ( );
	assert ( busInfoPtr != NULL );
	
	busInfoPtr->zeroData ( );
	theErr = fATADevice->provideBusInfo ( busInfoPtr );
	if ( theErr != kIOReturnSuccess )
	{
		
		STATUS_LOG ( ( "IOATAPIProtocolTransport::IdentifyAndConfigureATAPIDevice provide bus info failed thErr = %ld.\n", theErr ) );
		goto ReleaseBusInfoAndBail;
		
	}
	
	fATASocketType = busInfoPtr->getSocketType ( );
	STATUS_LOG ( ( "IOATAPIProtocolTransport::IdentifyAndConfigureATAPIDevice socket type = %d.\n", ( UInt8 ) fATASocketType ) );
			
	theErr = IdentifyATAPIDevice ( );
	if ( theErr != kIOReturnSuccess )
	{
		
		STATUS_LOG ( ( "IOATAPIProtocolTransport::IdentifyAndConfigureATAPIDevice IdentifyATAPIDevice error = %ld, resetting device.\n", theErr ) );
		
		theErr = ResetATAPIDevice ( );
		if ( theErr != kIOReturnSuccess )
		{
			
			STATUS_LOG ( ( "IOATAPIProtocolTransport::IdentifyAndConfigureATAPIDevice ResetATAPIDevice error = %ld.\n", theErr ) );
			
			// Not even a reset worked, bail
			goto ReleaseBusInfoAndBail;
			
		}
		
		theErr = IdentifyATAPIDevice ( );
		if ( theErr != kIOReturnSuccess )
		{
			
			STATUS_LOG ( ( "IOATAPIProtocolTransport::IdentifyAndConfigureATAPIDevice 2nd IdentifyATAPIDevice error = %ld.\n", theErr ) );

			// Not even a reset worked, bail
			goto ReleaseBusInfoAndBail;
		
		}
		
	}

	deviceConfigPtr = IOATADevConfig::atadevconfig ( );
	assert ( deviceConfigPtr != NULL );
	
	theErr = fATADevice->provideConfig ( deviceConfigPtr );
	if ( theErr != kIOReturnSuccess )
	{
		
		STATUS_LOG ( ( "IOATAPIProtocolTransport::IdentifyAndConfigureATAPIDevice provideConfig returned an error = %ld.\n", theErr ) );
		goto ReleaseBusInfoAndBail;
		
	}
	
	theErr = deviceConfigPtr->initWithBestSelection ( ( UInt16 * ) fDeviceIdentifyData, busInfoPtr );
	if ( theErr != kIOReturnSuccess )
	{
		
		STATUS_LOG ( ( "IOATAPIProtocolTransport::IdentifyAndConfigureATAPIDevice Autoconfigure didn't work error = %ld.\n", theErr ) );
		PANIC_NOW ( ( "Autoconfigure didn't work. Initialize drive speed manually.\n" ) );
		return theErr;
		
	}
		
	theErr = fATADevice->selectConfig ( deviceConfigPtr );
	if ( theErr != kIOReturnSuccess )
	{
		
		STATUS_LOG ( ( "IOATAPIProtocolTransport::IdentifyAndConfigureATAPIDevice selectConfig returned error = %ld.\n", theErr ) );
		return theErr;
		
	}

	fPIOMode 			= deviceConfigPtr->getPIOMode ( );
	fDMAMode			= deviceConfigPtr->getDMAMode ( );
	fUltraDMAMode 		= deviceConfigPtr->getUltraMode ( );
	fATAPIPacketConfig 	= deviceConfigPtr->getPacketConfig ( );
	
	// Adjust any of the Multiword DMA or Ultra DMA values if there is a subclass with an
	// ATAPI Mass Storage Characteristics dictionary.
	dict = OSDynamicCast ( OSDictionary, getProperty ( kIOPropertyATAPIMassStorageCharacteristics ) );
	if ( dict != NULL )
	{
		
		OSNumber *	modeNumber;
		
		STATUS_LOG ( ( "ATAPI Mass Storage dictionary exists.\n" ) );
		
		modeNumber = OSDynamicCast ( OSNumber, dict->getObject ( "DMA Mode" ) );
		if ( modeNumber != NULL )
		{
			
			STATUS_LOG ( ( "Changing default Multiword DMA Mode value from %d to %d\n",
							fDMAMode, modeNumber->unsigned8BitValue ( ) ) );
			fDMAMode = modeNumber->unsigned8BitValue ( );
			
		}

		modeNumber = OSDynamicCast ( OSNumber, dict->getObject ( "UDMA Mode" ) );
		if ( modeNumber != NULL )
		{
			
			STATUS_LOG ( ( "Changing default Ultra DMA Mode value from %d to %d\n",
							fUltraDMAMode, modeNumber->unsigned8BitValue ( ) ) );
			fUltraDMAMode = modeNumber->unsigned8BitValue ( );
			
		}
		
	}
	
	else
	{
		STATUS_LOG ( ( "ATAPI Mass Storage dictionary does not exist.\n" ) );
	}
	
	STATUS_LOG ( ( "atapiConfig = %d.\n", (int) fATAPIPacketConfig ) );	
	
	theErr = ConfigureATAPIDevice ( );
		
ReleaseBusInfoAndBail:


	if ( busInfoPtr != NULL )
	{

		STATUS_LOG ( ( "IOATAPIProtocolTransport::IdentifyAndConfigureATAPIDevice releasing bus info.\n" ) );
		busInfoPtr->release ( );
		busInfoPtr = NULL;
	
	}
	
	STATUS_LOG ( ( "IOATAPIProtocolTransport::IdentifyAndConfigureATAPIDevice returning theErr = %ld.\n", theErr ) );
	
	return theErr;
	
}


//--------------------------------------------------------------------------------------
//	е IdentifyATAPIDevice	-	Sends a device identify request to the device
//								and uses it to configure the drive speeds
//--------------------------------------------------------------------------------------

IOReturn
IOATAPIProtocolTransport::IdentifyATAPIDevice ( void )
{

	IOReturn		theErr 			= kIOReturnSuccess;
	IOSyncer *		syncer			= NULL;
	void *			previousRefCon	= NULL;
	
	previousRefCon = fIdentifyCommand->refCon;
	
	syncer = IOSyncer::create ( );
	assert ( syncer != NULL );
	
	STATUS_LOG ( ( "IOATAPIProtocolTransport::IdentifyATAPIDevice entering.\n" ) );
		
	// Zero the command object
	fIdentifyCommand->zeroCommand ( );
	
	// Start filling in the command
	fIdentifyCommand->setUnit ( fATAUnitID );
	fIdentifyCommand->setBuffer ( fDeviceIdentifyBuffer );
	fIdentifyCommand->setPosition ( 0 );
	fIdentifyCommand->setByteCount ( kATAPIIdentifyPacketDeviceDataSize );
	fIdentifyCommand->setTransferChunkSize ( kATADefaultSectorSize );
	
	fIdentifyCommand->setCommand ( kID_DRIVE );
	fIdentifyCommand->setTimeoutMS ( k10SecondTimeout );
	fIdentifyCommand->setFlags ( mATAFlagIORead );

	fIdentifyCommand->setOpcode ( kATAFnExecIO );
	// set the device head to the correct unit
	fIdentifyCommand->setDevice_Head ( fATAUnitID << 4 );
	fIdentifyCommand->setRegMask ( ( ataRegMask ) ( mATAErrFeaturesValid | mATAStatusCmdValid ) );
	fIdentifyCommand->refCon = ( void * ) syncer;
	fIdentifyCommand->setCallbackPtr ( &sATACallbackSync );
	
	STATUS_LOG ( ( "IOATAPIProtocolTransport::IdentifyATAPIDevice executing identify command.\n" ) );
	
	theErr = fATADevice->executeCommand ( fIdentifyCommand );
	
	syncer->wait ( );
	
	#if defined(__BIG_ENDIAN__)
		// The identify device info needs to be byte-swapped on big-endian (ppc) 
		// systems because it is data that is produced by the drive, read across a 
		// 16-bit little-endian PCI interface, directly into a big-endian system.
		// Regular data doesn't need to be byte-swapped because it is written and 
		// read from the host and is intrinsically byte-order correct.	
		sSwapBytes16 ( ( UInt8 * ) fDeviceIdentifyData, kATAPIIdentifyPacketDeviceDataSize );
	#endif
	
	STATUS_LOG ( ( "IOATAPIProtocolTransport::IdentifyATAPIDevice exiting with theErr = %ld.\n", theErr ) );
	
	fIdentifyCommand->refCon = previousRefCon;
	
	return theErr;
	
}


//--------------------------------------------------------------------------------------
//	е ConfigureATAPIDevice	-	Configures the ATAPI Device
//--------------------------------------------------------------------------------------

IOReturn
IOATAPIProtocolTransport::ConfigureATAPIDevice ( void )
{
	
	ATAPIConfigData *		configData;
	
	STATUS_LOG ( ( "IOATAPIProtocolTransport::ConfigureATAPIDevice entering.\n" ) );
	
	configData = ( ATAPIConfigData * ) fConfigCommand->refCon;
	
	configData->self 	= this;
	configData->state 	= kPIOTransferModeSetup;
	sATAPIConfigStateMachine ( fConfigCommand );	
	
	configData->syncer->wait ( );
	
	return kIOReturnSuccess;
		
}


//--------------------------------------------------------------------------------------
//	е ReconfigureATAPIDevice	-	Reconfigures the ATAPI Device
//--------------------------------------------------------------------------------------

IOReturn
IOATAPIProtocolTransport::ReconfigureATAPIDevice ( void )
{
	
	if ( fConfigCommand != NULL )
	{
		
		SetPIOTransferMode ( fConfigCommand, true );
		
		if ( ( fUltraDMAMode != 0 ) || ( fDMAMode != 0 ) )
		{
			SetDMATransferMode ( fConfigCommand, true );
		}
		
	}
	
	return kIOReturnSuccess;
		
}


//--------------------------------------------------------------------------------------
//	е SetPIOTransferMode	-	Configures the ATAPI Device's PIO Transfer mode
//--------------------------------------------------------------------------------------

IOReturn
IOATAPIProtocolTransport::SetPIOTransferMode ( IOATACommand * cmd, bool forceSync )
{
	
	IOReturn		theErr 	= kIOReturnSuccess;
	UInt8			mode	= 0;

	STATUS_LOG ( ( "IOATAPIProtocolTransport::SetPIOTransferMode entering.\n" ) );
	
	// Zero the command object
	cmd->zeroCommand ( );
	
	// Start filling in the command
	cmd->setUnit ( fATAUnitID );	
	cmd->setCommand ( kATAcmdSetFeatures );
	cmd->setTimeoutMS ( k10SecondTimeout );
	cmd->setFeatures ( kATASetTransferMode );
	
	// Always set to highest transfer mode
	mode = sConvertHighestBitToNumber ( fPIOMode );
	
	// PIO transfer mode is capped at 4 for now in the ATA-5 spec. If a device supports
	// more than mode 4 it has to at least support mode 4. We might not get the best
	// performance out of the drive, but it will work until we update to latest spec.
	if ( mode > 4 )
	{
		
		STATUS_LOG ( ( "IOATAPIProtocolTransport::SetPIOTransferMode mode > 4 = %ld.\n", ( UInt32 ) mode ) );
		mode = 4;
		
	}
	
	cmd->setSectorCount ( kATAEnablePIOModeMask | mode );
	cmd->setOpcode ( kATAFnExecIO );

	// set the device head to the correct unit
	cmd->setDevice_Head ( fATAUnitID << 4 );
	cmd->setFlags ( mATAFlagImmediate );
	
	if ( forceSync )
	{
		cmd->setCallbackPtr ( &sATAPIVoidCallback );
	}
	
	else
	{
		cmd->setCallbackPtr ( &sATAPIConfigStateMachine );
	}
	
	theErr = fATADevice->executeCommand ( cmd );	

	STATUS_LOG ( ( "IOATAPIProtocolTransport::SetPIOTransferMode exiting with error = %ld.\n", theErr ) );
	
	return theErr;
	
}


//--------------------------------------------------------------------------------------
//	е SetDMATransferMode	-	Configures the ATAPI Device's DMA Transfer mode
//--------------------------------------------------------------------------------------

IOReturn
IOATAPIProtocolTransport::SetDMATransferMode ( IOATACommand * cmd, bool forceSync )
{
	
	IOReturn		theErr 			= kIOReturnSuccess;
	UInt8			mode	= 0;

	STATUS_LOG ( ( "IOATAPIProtocolTransport::SetDMATransferMode entering.\n" ) );
		
	// Zero the command object
	cmd->zeroCommand ( );
	
	// Start filling in the command
	cmd->setUnit ( fATAUnitID );	
	cmd->setCommand ( kATAcmdSetFeatures );
	cmd->setTimeoutMS ( k10SecondTimeout );
	cmd->setFeatures ( kATASetTransferMode );
	cmd->setOpcode ( kATAFnExecIO );
	cmd->setDevice_Head ( fATAUnitID << 4 );
	cmd->setFlags ( mATAFlagImmediate );

	// Always set to highest transfer mode
	if ( fUltraDMAMode )
	{
		
		STATUS_LOG ( ( "IOATAPIProtocolTransport::SetDMATransferMode choosing UltraDMA.\n" ) );
		mode = sConvertHighestBitToNumber ( fUltraDMAMode );
		// Ultra DMA is capped at 4 for now in the ATA-5 spec. If a device supports
		// more than mode 4 it MUST at least support mode 4. We might not get the best
		// performance out of the drive, but it will work until we update to latest spec.
		if ( mode > 4 )
			mode = 4;
		
		cmd->setSectorCount ( kATAEnableUltraDMAModeMask | mode );
		
	}
	
	else
	{
		
		STATUS_LOG ( ( "IOATAPIProtocolTransport::SetDMATransferMode choosing DMA.\n" ) );
		mode = sConvertHighestBitToNumber ( fDMAMode );
		// MultiWord DMA is capped at 2 for now in the ATA-5 spec. If a device supports
		// more than mode 2 it MUST at least support mode 2. We might not get the best
		// performance out of the drive, but it will work until we update to latest spec.
		if ( mode > 2 )
			mode = 2;
		
		cmd->setSectorCount ( kATAEnableMultiWordDMAModeMask | mode );
		
	}
	
	if ( forceSync )
	{
		cmd->setCallbackPtr ( &sATAPIVoidCallback );
	}
	
	else
	{
		cmd->setCallbackPtr ( &sATAPIConfigStateMachine );
	}
		
	STATUS_LOG ( ( "IOATAPIProtocolTransport::SetDMATransferMode executing DMA setup command.\n" ) );
	
	theErr = fATADevice->executeCommand ( cmd );
	
	STATUS_LOG ( ( "IOATAPIProtocolTransport::SetDMATransferMode exiting with error = %ld.\n", theErr ) );
	
	return theErr;
	
}


//--------------------------------------------------------------------------------------
//	е ResetATAPIDevice	-	Sends a device reset command to the device
//--------------------------------------------------------------------------------------

IOReturn
IOATAPIProtocolTransport::ResetATAPIDevice ( void )
{

	IOReturn	theErr = kIOReturnSuccess;
	
	STATUS_LOG ( ( "IOATAPIProtocolTransport::ResetATAPIDevice entering.\n" ) );
	
	if ( fResetInProgress )
		return kIOReturnNotPermitted;
	
	fResetInProgress = true;
	
	// Zero the command object
	fResetCommand->zeroCommand ( );
	
	// Start filling in the command
	fResetCommand->setUnit ( fATAUnitID );	
	fResetCommand->setCommand ( kSOFTRESET );
	fResetCommand->setTimeoutMS ( k45SecondTimeout );
	fResetCommand->setFlags ( mATAFlagImmediate );
	fResetCommand->setOpcode ( kATAFnBusReset );
	fResetCommand->setCallbackPtr ( &sATAPIResetCallback );
	fResetCommand->refCon = ( void * ) this;
	
	theErr = fATADevice->executeCommand ( fResetCommand );
	
	return theErr;
	
}


//--------------------------------------------------------------------------------------
//	е EnablePollingOfStatusRegister	-	Called to schedule a poll of the status register.
//--------------------------------------------------------------------------------------

void
IOATAPIProtocolTransport::EnablePollingOfStatusRegister ( void )
{
	
	AbsoluteTime	time;
	
	STATUS_LOG ( ( "EnablePollingOfStatusRegister called\n" ) );
	
	// No reason to start a thread if we've been termintated	
	if ( ( isInactive ( ) == false ) &&
		 ( fPollingThread != NULL ) &&
		 ( fWakeUpResetOccurred == false ) )
	{
		
		// Retain ourselves so that this object doesn't go away
		// while we are polling
		
		retain ( );
		
		clock_interval_to_deadline ( 1000, kMillisecondScale, &time );
		thread_call_enter_delayed ( fPollingThread, time );
		
	}
	
}


//--------------------------------------------------------------------------------------
//	е DisablePollingOfStatusRegister	-	Called to cancel a poll of the status register.
//--------------------------------------------------------------------------------------

void
IOATAPIProtocolTransport::DisablePollingOfStatusRegister ( void )
{
	
	fWakeUpResetOccurred = true;
	
	// Cancel the thread if it is scheduled.
	if ( thread_call_cancel ( fPollingThread ) )
	{
		
		// It was scheduled, so we balance out the retain ( )
		// with a release ( )
		release ( );
		
	}
	
}


//--------------------------------------------------------------------------------------
//	е PollStatusRegister	-	Called to poll the status register to see if the drive
//								bay door has been opened.
//--------------------------------------------------------------------------------------

void
IOATAPIProtocolTransport::PollStatusRegister ( void * refCon )
{
	
	IOATACommand *		cmd;
	ATAPIClientData *	clientData;
	
	STATUS_LOG ( ( "PollStatusRegister called\n" ) );
	
	if ( fWakeUpResetOccurred == true )
		return;
	
	// Get a command
	cmd = fIdentifyCommand;
	
	clientData = ( ATAPIClientData * ) cmd->refCon;
	
	clientData->self = this;
	
	// Zero the command
	cmd->zeroCommand ( );
	
	// Set the command up for reading the register
	cmd->setFlags ( mATAFlagIORead );
	cmd->setOpcode ( kATAFnRegAccess );
	cmd->setUnit ( fATAUnitID );
	cmd->setRegMask ( mATAStatusCmdValid );
	cmd->setTimeoutMS ( k10SecondTimeout );
	cmd->setCallbackPtr ( &sPollStatusRegisterCallback );
	
	fATADevice->executeCommand ( cmd );
	
}


//--------------------------------------------------------------------------------------
//	е PollStatusRegisterCallback	-	Callback handler for status register polling
//--------------------------------------------------------------------------------------

void
IOATAPIProtocolTransport::PollStatusRegisterCallback ( IOATACommand * cmd )
{
	
	IOReturn		theErr 			= kIOReturnSuccess;
	UInt8			statusRegValue	= 0;
	
	STATUS_LOG ( ( "IOATAPIProtocolTransport::PollStatusRegisterCallback called\n" ) );
	
	theErr = cmd->getResult ( );
	if ( theErr == kIOReturnSuccess )
	{
		
		// Read the status register value
		statusRegValue = cmd->getStatus ( );
		
		// If the value is 0x50, then the drive door has been opened since we last
		// checked. Let the SCSI Application Layer know so it can try to poll for
		// media.
		if ( statusRegValue == 0x50 )
		{

			STATUS_LOG ( ( "Sending message to application layer.\n" ) );
			
			// Reset the device to bring it out of sleep mode, since media
			// might have been inserted.
			ResetATAPIDevice ( );
			
		}
		
		else
		{
			
			// Do another poll
			EnablePollingOfStatusRegister ( );
			
		}
		
	}
	
	else
	{
		
		ERROR_LOG ( ( "Error = %d occurred while polling status register", theErr ) );
		// Some error occurred. For now, just issue another poll
		EnablePollingOfStatusRegister ( );
		
	}
	
	// Drop the retain for this poll
	release ( );
	
}


//--------------------------------------------------------------------------------------
//	е SendATASleepCommand	-	Sends an ATA SLEEP command to the drive
//--------------------------------------------------------------------------------------

IOReturn
IOATAPIProtocolTransport::SendATASleepCommand ( void )
{
	
	IOReturn		status;
	IOSyncer *		syncer;
	IOATACommand *	cmd;
	void *			previousRefCon;
	
	STATUS_LOG ( ( "%s%s::%s called%s\n", "\033[36m", getName ( ), __FUNCTION__, "\033[0m" ) );	
			
	syncer = IOSyncer::create ( );	
	assert ( syncer != NULL );
	
	cmd = GetATACommandObject ( );
	
	// Zero the command
	cmd->zeroCommand ( );
	cmd->setUnit ( fATAUnitID );	
	cmd->setTimeoutMS ( kATATimeout10Seconds );
	cmd->setCallbackPtr ( &IOATAPIProtocolTransport::sATACallbackSync );
	cmd->setDevice_Head ( fATAUnitID << 4 );
	cmd->setOpcode ( kATAFnExecIO );
	cmd->setCommand ( kATAcmdSleep );
	
	previousRefCon = cmd->refCon;
	cmd->refCon = ( void * ) syncer;
	
	status = fATADevice->executeCommand ( cmd );
	
	status = syncer->wait ( false );
	syncer->release ( );

	cmd->refCon = previousRefCon;
		
	ReturnATACommandObject ( cmd );
	
	return status;	
	
}


//--------------------------------------------------------------------------------------
//	е TurnDrivePowerOff	-	Called to turn power to the drive OFF.
//--------------------------------------------------------------------------------------

IOReturn
IOATAPIProtocolTransport::TurnDrivePowerOff ( void )
{
	
	IOReturn		status;
	IOSyncer *		syncer;
	IOATACommand *	cmd;
	void *			previousRefCon;
	
	STATUS_LOG ( ( "IOATAPIProtocolTransport::TurnDrivePowerOff called\n" ) );
			
	syncer = IOSyncer::create ( );	
	assert ( syncer != NULL );
	
	cmd = GetATACommandObject ( );
	
	// Zero the command
	cmd->zeroCommand ( );
	
	// Set the command up for shutting the drive power off
	cmd->setUnit ( fATAUnitID );	
	cmd->setOpcode ( kATAFnRegAccess );
	cmd->setRegMask ( mATAStatusCmdValid );
	cmd->setFlags ( mATAFlagIORead | mATAFlagQuiesce );
	cmd->setTimeoutMS ( kATATimeout10Seconds );
	cmd->setCallbackPtr ( &IOATAPIProtocolTransport::sATACallbackSync );
	
	previousRefCon = cmd->refCon;
	cmd->refCon = ( void * ) syncer;
	
	status = fATADevice->executeCommand ( cmd );
	
	status = syncer->wait ( false );
	syncer->release ( );

	cmd->refCon = previousRefCon;
		
	ReturnATACommandObject ( cmd );
	
	return status;	
	
}


//--------------------------------------------------------------------------------------
//	е SetWakeupResetOccurred	-	Called on safe side of command gate to set state
//--------------------------------------------------------------------------------------

void
IOATAPIProtocolTransport::SetWakeupResetOccurred ( bool resetOccurred )
{
	
	fWakeUpResetOccurred = resetOccurred;
	
}


//--------------------------------------------------------------------------------------
//	е sSetWakeupResetOccurred	-	Called on safe side of command gate
//--------------------------------------------------------------------------------------

void
IOATAPIProtocolTransport::sSetWakeupResetOccurred ( IOATAPIProtocolTransport * driver,
												    bool resetOccurred )
{
	
	driver->SetWakeupResetOccurred ( resetOccurred );
	
}


//--------------------------------------------------------------------------------------
//	е CheckWakeupResetOccurred	-	Called on safe side of command gate to find out if
//									the driver has already received a reset message
//--------------------------------------------------------------------------------------

bool
IOATAPIProtocolTransport::CheckWakeupResetOccurred ( void )
{
	
	return fWakeUpResetOccurred;
	
}


//--------------------------------------------------------------------------------------
//	е sCheckWakeupResetOccurred	-	Called on safe side of command gate
//--------------------------------------------------------------------------------------

void
IOATAPIProtocolTransport::sCheckWakeupResetOccurred ( IOATAPIProtocolTransport * driver,
												 	  bool * resetOccurred )
{
	
	*resetOccurred = driver->CheckWakeupResetOccurred ( );
	return;
	
}


//--------------------------------------------------------------------------------------
//	е sSwapBytes16	-	swaps the buffer for device identify data
//--------------------------------------------------------------------------------------

void
IOATAPIProtocolTransport::sSwapBytes16 ( UInt8 * buffer, IOByteCount numBytesToSwap )
{
	
	IOByteCount		index;
	UInt8			temp;
	UInt8 *			firstBytePtr;
	
	for ( index = 0; index < numBytesToSwap; index += 2 )
	{
		
		firstBytePtr 	= buffer;				// save pointer
		temp 			= *buffer++;			// Save Byte0, point to Byte1
		*firstBytePtr 	= *buffer;				// Byte0 = Byte1
		*buffer++		= temp;					// Byte1 = Byte0
		
	}
	
}


//--------------------------------------------------------------------------------------
//	е sConvertHighestBitToNumber	-	Finds the higest bit in a number and returns
//--------------------------------------------------------------------------------------

UInt8
IOATAPIProtocolTransport::sConvertHighestBitToNumber ( UInt16 bitField )
{
	
	UInt16  index, integer;
	
	// Test all bits from left to right, terminating at the first non-zero bit
	for ( index = 0x0080, integer = 7; ( ( index & bitField ) == 0 && index != 0 ) ; index >>= 1, integer-- )
	{ ; }
	
	return ( integer );
	
}


//---------------------------------------------------------------------------
// Handles messages from our provider.
//---------------------------------------------------------------------------

IOReturn
IOATAPIProtocolTransport::message ( UInt32 type, IOService * provider, void * argument )
{
	
	IOReturn		status = kIOReturnSuccess;
	
	STATUS_LOG ( ( "IOATABlockStorageDevice::message %p %lx\n", this, type ) );
	

	switch ( type )
	{
		
		case kATAResetEvent:					// Someone gave a reset to the bus
			// reconfig device here
			fWakeUpResetOccurred = true;
			status = ReconfigureATAPIDevice ( );
			// Tell the layer above us that a reset occurred so it can lock media
			// and verify that the device is in a good state.
			SendNotification_VerifyDeviceState ( );
			break;
		
		case kATANewMediaEvent:
			// Tell the layer above us that new media has been added so it can lock media
			// and verify that the device is in a good state.
			SendNotification_VerifyDeviceState ( );
 			break;
		
		case kATANullEvent:						// Just kidding -- nothing happened
			break;
		
		// atapi resets are not relevent to ATA devices, but soft-resets ARE relevant to ATAPI devices.
		case kATAPIResetEvent:					// Someone gave a ATAPI reset to the drive
			// reconfig device here
			fWakeUpResetOccurred = true;
			status = ReconfigureATAPIDevice ( );
			// Tell the layer above us that a reset occurred so it can lock media
			// and verify that the device is in a good state.
			SendNotification_VerifyDeviceState ( );
			break;
		
		case kIOMessageServiceIsRequestingClose:
            fPhysicallyConnected = false;
			SendNotification_DeviceRemoved ( );
            DeallocateATACommandObjects ( );
            if ( fATADevice != NULL )
            {
				// Make sure we close provider, else the terminate won't propagate up the
				// stack.
                fATADevice->close ( this );
                fATADevice = NULL;
				
            }
			break;
			
		default:
			status = super::message ( type, provider, argument );
			break;
		
	}
	
	return status;
	
}


// binary compatibility reserved method space
OSMetaClassDefineReservedUnused ( IOATAPIProtocolTransport, 1 );
OSMetaClassDefineReservedUnused ( IOATAPIProtocolTransport, 2 );
OSMetaClassDefineReservedUnused ( IOATAPIProtocolTransport, 3 );
OSMetaClassDefineReservedUnused ( IOATAPIProtocolTransport, 4 );
OSMetaClassDefineReservedUnused ( IOATAPIProtocolTransport, 5 );
OSMetaClassDefineReservedUnused ( IOATAPIProtocolTransport, 6 );
OSMetaClassDefineReservedUnused ( IOATAPIProtocolTransport, 7 );
OSMetaClassDefineReservedUnused ( IOATAPIProtocolTransport, 8 );
OSMetaClassDefineReservedUnused ( IOATAPIProtocolTransport, 9 );
OSMetaClassDefineReservedUnused ( IOATAPIProtocolTransport, 10 );
OSMetaClassDefineReservedUnused ( IOATAPIProtocolTransport, 11 );
OSMetaClassDefineReservedUnused ( IOATAPIProtocolTransport, 12 );
OSMetaClassDefineReservedUnused ( IOATAPIProtocolTransport, 13 );
OSMetaClassDefineReservedUnused ( IOATAPIProtocolTransport, 14 );
OSMetaClassDefineReservedUnused ( IOATAPIProtocolTransport, 15 );
OSMetaClassDefineReservedUnused ( IOATAPIProtocolTransport, 16 );


//--------------------------------------------------------------------------------------
//							End				Of				File
//--------------------------------------------------------------------------------------