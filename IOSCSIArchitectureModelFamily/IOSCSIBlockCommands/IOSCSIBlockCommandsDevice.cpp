/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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

// Libkern includes
#include <libkern/OSByteOrder.h>

// Generic IOKit related headers
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOKitKeys.h>

// Generic IOKit storage related headers
#include <IOKit/storage/IOBlockStorageDriver.h>

// SCSI Architecture Model Family includes
#include <IOKit/scsi-commands/SCSICommandDefinitions.h>
#include <IOKit/scsi-commands/IOBlockStorageServices.h>
#include <IOKit/scsi-commands/IOSCSIBlockCommandsDevice.h>
#include "SCSIBlockCommands.h"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define DEBUG 									0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING		"SBC"

#if DEBUG
#define SCSI_SBC_DEVICE_DEBUGGING_LEVEL			0
#endif


#include "IOSCSIArchitectureModelFamilyDebugging.h"


#if ( SCSI_SBC_DEVICE_DEBUGGING_LEVEL >= 1 )
#define PANIC_NOW(x)		IOPanic x
#else
#define PANIC_NOW(x)
#endif

#if ( SCSI_SBC_DEVICE_DEBUGGING_LEVEL >= 2 )
#define ERROR_LOG(x)		IOLog x
#else
#define ERROR_LOG(x)
#endif

#if ( SCSI_SBC_DEVICE_DEBUGGING_LEVEL >= 3 )
#define STATUS_LOG(x)		IOLog x
#else
#define STATUS_LOG(x)
#endif


#define super IOSCSIPrimaryCommandsDevice
OSDefineMetaClass ( IOSCSIBlockCommandsDevice, IOSCSIPrimaryCommandsDevice );
OSDefineAbstractStructors ( IOSCSIBlockCommandsDevice, IOSCSIPrimaryCommandsDevice );


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Constants
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define kMaxRetryCount						8
#define kCapacityDataBufferSize				8
#define kWriteProtectMask					0x80
#define kAppleKeySwitchProperty				"AppleKeyswitch"
#define kFibreChannelHDIconKey				"FibreChannelHD.icns"
#define kFireWireHDIconKey					"FireWireHD.icns"
#define kUSBHDIconKey						"USBHD.icns"
#define kModeSense6ParameterHeaderSize		4
#define kCachingModePageMinSize				18
#define	kDefaultMaxBlocksPerIO				65535


#if 0
#pragma mark -
#pragma mark ¥ Public Methods - API Exported to layers above
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SyncReadWrite - 	Translates a synchronous I/O request into a
//						read or a write.							   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIBlockCommandsDevice::SyncReadWrite (
						IOMemoryDescriptor *	buffer,
						UInt64					startBlock,
						UInt64					blockCount,
						UInt64					blockSize )
{
	
	IODirection		direction;
	IOReturn		status = kIOReturnBadArgument;
	
	require_action ( IsProtocolAccessEnabled ( ),
					 ErrorExit,
					 status = kIOReturnNotAttached );
					
	require_action ( IsDeviceAccessEnabled ( ),
					 ErrorExit,
					 status = kIOReturnOffline );
	
	direction = buffer->getDirection ( );
	
	if ( direction == kIODirectionIn )
	{
		
		status = IssueRead ( buffer, startBlock, blockCount );
		
	}
	
	else if ( direction == kIODirectionOut )
	{
		
		status = IssueWrite ( buffer, startBlock, blockCount );
		
	}
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AsyncReadWrite - 	Translates an asynchronous I/O request into a
//						read or a write.							   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIBlockCommandsDevice::AsyncReadWrite (	IOMemoryDescriptor *	buffer,
											UInt64					startBlock,
											UInt64					blockCount,
						 					UInt64					blockSize,
											void *					clientData )
{
	
	IODirection		direction;
	IOReturn		status = kIOReturnBadArgument;
	
	require_action ( IsProtocolAccessEnabled ( ),
					 ErrorExit,
					 status = kIOReturnNotAttached );
					
	require_action ( IsDeviceAccessEnabled ( ),
					 ErrorExit,
					 status = kIOReturnOffline );
	
	direction = buffer->getDirection ( );
	if ( direction == kIODirectionIn )
	{
		
		status = IssueRead ( buffer, startBlock, blockCount, clientData );
		
	}
	
	else if ( direction == kIODirectionOut )
	{
		
		status = IssueWrite ( buffer, startBlock, blockCount, clientData );
		
	}
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ EjectTheMedium - 	Unlocks and ejects the medium if it is removable. If it
//						is not removable, it synchronizes the write cache.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIBlockCommandsDevice::EjectTheMedium ( void )
{
	
	IOReturn				status				= kIOReturnNoResources;
	SCSIServiceResponse		serviceResponse 	= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier		request				= NULL;
	bool					doPollForRemoval 	= false;
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	require_action ( IsProtocolAccessEnabled ( ),
					 ErrorExit,
					 status = kIOReturnNotAttached );
	
	require_action ( IsDeviceAccessEnabled ( ),
					 ErrorExit,
					 status = kIOReturnOffline );
	
	// Is the media removable?
	if ( fMediaIsRemovable == false )
	{
		
		// Not a removable disk. Synchronize the drive's write cache.
		status = SynchronizeCache ( );
		
		// Tell power management to ask us to spin the drive down.
		changePowerStateToPriv ( kSBCPowerStateSleep );
		
	}

	else
	{
		
		// We have removable media. First, if it is not a manual eject device,
		// we know we can unlock it and then eject it. Otherwise, a dialog will
		// be brought up which tells the user it is safe to eject the medium
		// manually.
		
		// Is the device a known manual eject device?
		if ( fKnownManualEject == false )
		{
			
			request = GetSCSITask ( );
			require_nonzero ( request, ErrorExit );
			
			// Unlock it.
			if ( PREVENT_ALLOW_MEDIUM_REMOVAL ( request, kMediaStateUnlocked, 0 ) == true )
			{
				
				// The command was successfully built, now send it
				( void ) SendCommand ( request, kTenSecondTimeoutInMS );
				
			}
			
			// Eject it.
			if ( START_STOP_UNIT ( request, 0, 0, 1, 0, 0 ) == true )
			{
				
				// The command was successfully built, now send it
				serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
				
				if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
					 ( GetTaskStatus ( request ) != kSCSITaskStatus_GOOD ) )
				{
					
					// The eject command failed.  This is most likely a manually ejectable
					// device, start the polling to determine when the media has been removed.
					doPollForRemoval = true;
					
				}
				
			}
			
			ReleaseSCSITask ( request );
			request = NULL;
			
		}
		
		else
		{
			
			// It is a known manual eject device. Must poll here as well.
			doPollForRemoval = true;
			
		}
		
		ResetMediumCharacteristics ( );
		fMediumIsWriteProtected = true;
		
		if ( ( doPollForRemoval == true ) || ( fMediumRemovalPrevented == false ) )
		{
			
			// ¥¥¥ Add code here to put up the dialog "It is now safe to remove
			// this drive from the machine"
			
			// Set the polling to determine when media has been removed
 			fPollingMode = kPollingMode_MediaRemoval;
 			
   		}
   		
		else
		{
			
			// Set the polling to determine when new media has been inserted
 			fPollingMode = kPollingMode_NewMedia;
 			
		}
		
		EnablePolling ( );
		
	}
	
	status = kIOReturnSuccess;
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ FormatMedium - Unsupported.									   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIBlockCommandsDevice::FormatMedium ( UInt64 blockCount, UInt64 blockSize )
{
	
	IOReturn	status = kIOReturnUnsupported;
	
	require_action ( IsProtocolAccessEnabled ( ),
					 ErrorExit,
					 status = kIOReturnNotAttached );
	
	require_action ( IsDeviceAccessEnabled ( ),
					 ErrorExit,
					 status = kIOReturnOffline );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetFormatCapacities - Unsupported.							   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt32
IOSCSIBlockCommandsDevice::GetFormatCapacities (
								UInt64 * capacities,
								UInt32   capacitiesMaxCount ) const
{
	
	return 0;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ LockUnlockMedium - Unsupported.								   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIBlockCommandsDevice::LockUnlockMedium ( bool doLock )
{
	
	IOReturn	status = kIOReturnSuccess;
	
	require_action ( IsProtocolAccessEnabled ( ), ErrorExit, status = kIOReturnNotAttached );
	require_action ( IsProtocolAccessEnabled ( ), ErrorExit, status = kIOReturnOffline );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SynchronizeCache - Synchronizes the write cache.				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIBlockCommandsDevice::SynchronizeCache ( void )
{
	
	IOReturn				status			= kIOReturnSuccess;
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier		request			= NULL;
	
	STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::SynchronizeCache called\n" ) );
	
	require_action ( IsProtocolAccessEnabled ( ),
					 ErrorExit,
					 status = kIOReturnNotAttached );
	
	require_action ( IsDeviceAccessEnabled ( ),
					 ErrorExit,
					 status = kIOReturnOffline );
	
	require ( fWriteCacheEnabled, ErrorExit );
	
	request = GetSCSITask ( );
	require_nonzero_action ( request,
							 ErrorExit,
							 status = kIOReturnNoResources );
	
	if ( SYNCHRONIZE_CACHE ( request, 0, 0, 0, 0, 0 ) == true )
	{
		
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
		
	}
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		status = kIOReturnSuccess;
	}
	
	else
	{
		status = kIOReturnError;
	}
	
	ReleaseSCSITask ( request );
	request = NULL;
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportBlockSize - Reports the medium block size.				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt64
IOSCSIBlockCommandsDevice::ReportMediumBlockSize ( void )
{
	
	return fMediumBlockSize;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportMediumTotalBlockCount - Reports total number of blocks on medium.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt64
IOSCSIBlockCommandsDevice::ReportMediumTotalBlockCount ( void )
{
	
	return fMediumBlockCount;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportMediumWriteProtection - Reports write protection characteristic
//									of medium						   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::ReportMediumWriteProtection ( void )
{
	
	return fMediumIsWriteProtected;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportDeviceMaxBlocksReadTransfer - Reports maximum read transfer blocks.
//																	   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt64
IOSCSIBlockCommandsDevice::ReportDeviceMaxBlocksReadTransfer ( void )
{

	UInt32	maxBlockCount 	= kDefaultMaxBlocksPerIO;
	UInt64	maxByteCount	= 0;
	bool	supported		= false;
	
	STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::ReportDeviceMaxBlocksWriteTransfer.\n" ) );
	
	// See if the transport driver wants us to limit the block transfer count
	supported = GetProtocolDriver ( )->IsProtocolServiceSupported (
						kSCSIProtocolFeature_MaximumReadBlockTransferCount,
						&maxBlockCount );
	
	if ( supported == false )
		maxBlockCount = kDefaultMaxBlocksPerIO;
	
	// See if the transport driver wants us to limit the transfer byte count
	supported = GetProtocolDriver ( )->IsProtocolServiceSupported (
						kSCSIProtocolFeature_MaximumReadTransferByteCount,
						&maxByteCount );	
	
	if ( ( supported == true ) && ( maxByteCount > 0 ) && ( fMediumBlockSize > 0 ) )
	{
		
		maxBlockCount = min ( maxBlockCount, ( maxByteCount / fMediumBlockSize ) );
		
	}
	
	return maxBlockCount;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportDeviceMaxBlocksWriteTransfer -	Reports maximum write transfer
//											in blocks.				   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

UInt64
IOSCSIBlockCommandsDevice::ReportDeviceMaxBlocksWriteTransfer ( void )
{

	UInt32	maxBlockCount 	= kDefaultMaxBlocksPerIO;
	UInt64	maxByteCount	= 0;
	bool	supported		= false;
	
	STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::ReportDeviceMaxBlocksWriteTransfer.\n" ) );
	
	// See if the transport driver wants us to limit the block transfer count
	supported = GetProtocolDriver ( )->IsProtocolServiceSupported (
						kSCSIProtocolFeature_MaximumWriteBlockTransferCount,
						&maxBlockCount );
	
	if ( supported == false )
		maxBlockCount = kDefaultMaxBlocksPerIO;
	
	// See if the transport driver wants us to limit the transfer byte count
	supported = GetProtocolDriver ( )->IsProtocolServiceSupported (
						kSCSIProtocolFeature_MaximumWriteTransferByteCount,
						&maxByteCount );	
	
	if ( ( supported == true ) && ( maxByteCount > 0 ) && ( fMediumBlockSize > 0 ) )
	{
		
		maxBlockCount = min ( maxBlockCount, ( maxByteCount / fMediumBlockSize ) );
		
	}
	
	return maxBlockCount;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ReportDeviceMediaRemovability -	Reports removability characteristic
//										of media					   [PUBLIC]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::ReportDeviceMediaRemovability ( void )
{
	
	STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::ReportMediaRemovability fMediaIsRemovable = %d\n",
					( int ) fMediaIsRemovable ) );
	
	return fMediaIsRemovable;
	
}


#if 0
#pragma mark -
#pragma mark ¥ Protected Methods - Methods used by this class and subclasses
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ InitializeDeviceSupport - Initializes device support			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::InitializeDeviceSupport ( void )
{
	
	bool	setupSuccessful = false;
	
	// Initialize the device characteristics flags
	fMediaIsRemovable 		= false;
	
	// Initialize the medium characteristics
	fMediumPresent			= false;
	fMediumIsWriteProtected	= true;
	fMediumRemovalPrevented	= false;
	fKnownManualEject		= false;
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	// Allocate space for our reserved data.
	fIOSCSIBlockCommandsDeviceReserved = IONew ( IOSCSIBlockCommandsDeviceExpansionData, 1 );
	require_nonzero ( fIOSCSIBlockCommandsDeviceReserved, ErrorExit );
	
	bzero ( fIOSCSIBlockCommandsDeviceReserved,
			sizeof ( IOSCSIBlockCommandsDeviceExpansionData ) );
	
	// Grab any device information from the IORegistry
	if ( getProperty ( kIOPropertySCSIDeviceCharacteristicsKey ) != NULL )
	{
		
		// There is a characteristics property for this device, check for known entries.
		OSDictionary * characterDict = NULL;
		
		characterDict = OSDynamicCast (
					OSDictionary,
					getProperty ( kIOPropertySCSIDeviceCharacteristicsKey ) );
		
		// Check if the personality for this device specifies that this is known to be manual ejectable.
		if ( characterDict->getObject ( kIOPropertySCSIManualEjectKey ) != NULL )
		{
			
			STATUS_LOG ( ( "%s: found a Manual Eject property.\n", getName ( ) ) );
			fKnownManualEject = true;
			
		}
		
	}
	
	if ( GetProtocolDriver ( )->getProperty ( kIOPropertyProtocolCharacteristicsKey ) != NULL )
	{
		
		// There is a characteristics property for this device, check for known entries.
		OSDictionary * characterDict = NULL;
		
		characterDict = OSDynamicCast (
					OSDictionary,
					GetProtocolDriver ( )->getProperty ( kIOPropertyProtocolCharacteristicsKey ) );
		
		// Check if the personality for this device specifies that this is known to be manual ejectable.
		if ( characterDict->getObject ( kIOPropertySCSIProtocolMultiInitKey ) != NULL )
		{
			
			fDeviceIsShared = true;		
			
		}
		
	}
	
	// Make sure the drive is ready for us!
	require ( ClearNotReadyStatus ( ), ReleaseReservedMemory );
	
	setupSuccessful = DetermineDeviceCharacteristics ( );
	
	if ( setupSuccessful == true )
	{		
		
		fPollingMode = kPollingMode_NewMedia;
		fPollingThread = thread_call_allocate (
						( thread_call_func_t ) IOSCSIBlockCommandsDevice::sProcessPoll,
						( thread_call_param_t ) this );
		
		require_nonzero_action_string ( fPollingThread,
										ErrorExit,
										setupSuccessful = false,
										"fPollingThread allocation failed.\n" );
		
		InitializePowerManagement ( GetProtocolDriver ( ) );
		
	}
	
	STATUS_LOG ( ( "%s::%s setupSuccessful = %d\n", getName ( ),
					__FUNCTION__, setupSuccessful ) );
	
	setProperty ( kIOMaximumBlockCountReadKey,  kDefaultMaxBlocksPerIO, 64 );
	setProperty ( kIOMaximumBlockCountWriteKey, kDefaultMaxBlocksPerIO, 64 );
	
	return setupSuccessful;
	
	
ReleaseReservedMemory:
	
	
	require_nonzero_quiet ( fIOSCSIBlockCommandsDeviceReserved, ErrorExit );
	IODelete ( fIOSCSIBlockCommandsDeviceReserved, IOSCSIBlockCommandsDeviceExpansionData, 1 );
	fIOSCSIBlockCommandsDeviceReserved = NULL;	
	
	
ErrorExit:
	
	
	return setupSuccessful;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ StartDeviceSupport - Starts device support					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::StartDeviceSupport ( void )
{
	
	OSBoolean *		shouldNotPoll = NULL;
		
	shouldNotPoll = OSDynamicCast (
							OSBoolean,
							getProperty ( kAppleKeySwitchProperty ) );
	
	if ( shouldNotPoll != NULL )
	{
		
		// See if we should not poll.
		require ( shouldNotPoll->isFalse ( ), Exit );
		
	}
	
	// Start polling
	EnablePolling ( );
	
	
Exit:
	
	
	CreateStorageServiceNub ( );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SuspendDeviceSupport - Suspends device support				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::SuspendDeviceSupport ( void )
{
	
	if ( fPollingMode != kPollingMode_Suspended )
	{
		DisablePolling ( );
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ResumeDeviceSupport - Resumes device support					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::ResumeDeviceSupport ( void )
{
	
	if ( fMediumPresent == false )
	{
		
		// The driver has not found media in the device, restart
		// the polling for new media.
		fPollingMode = kPollingMode_NewMedia;
		EnablePolling ( );
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ StopDeviceSupport - Stops device support						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::StopDeviceSupport ( void )
{
	DisablePolling ( );
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ TerminateDeviceSupport - Terminates device support			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::TerminateDeviceSupport ( void )
{
	
	if ( fPollingThread != NULL )
	{
		
		thread_call_free ( fPollingThread );
		fPollingThread = NULL;
		
	}
	
	// Release all memory/objects associated with the reserved fields.
	if ( fPowerDownNotifier != NULL )
	{
		
		// remove() will also call release() on this object (IONotifier).
		// See IONotifier.h for more info.
		fPowerDownNotifier->remove ( );
		fPowerDownNotifier = NULL;
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ CreateCommandSetObjects - Creates command set objects			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::CreateCommandSetObjects ( void )
{
	
	bool	result = false;
	
	fSCSIBlockCommandObject =
		SCSIBlockCommands::CreateSCSIBlockCommandObject ( );
	require_nonzero ( fSCSIBlockCommandObject, ErrorExit );
	
	result = true;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ FreeCommandSetObjects - Releases command set objects			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::FreeCommandSetObjects ( void )
{
	
	if ( fSCSIBlockCommandObject != NULL )
	{
		
		fSCSIBlockCommandObject->release ( );
		fSCSIBlockCommandObject = NULL;
  		
	}
	
	// Release the reserved structure.
	if ( fIOSCSIBlockCommandsDeviceReserved != NULL )
	{
		
		IODelete ( fIOSCSIBlockCommandsDeviceReserved, IOSCSIBlockCommandsDeviceExpansionData, 1 );
		fIOSCSIBlockCommandsDeviceReserved = NULL;
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetSCSIReducedBlockCommandObject - Accessor method			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIBlockCommands *
IOSCSIBlockCommandsDevice::GetSCSIBlockCommandObject ( void )
{
	
	check ( fSCSIBlockCommandObject );
	return fSCSIBlockCommandObject;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GetSCSIPrimaryCommandObject - Accessor method					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

SCSIPrimaryCommands	*
IOSCSIBlockCommandsDevice::GetSCSIPrimaryCommandObject ( void )
{
	
	check ( fSCSIBlockCommandObject );
	return OSDynamicCast ( SCSIPrimaryCommands,
						   GetSCSIBlockCommandObject ( ) );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ClearNotReadyStatus - Clears any NOT_READY status on device	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::ClearNotReadyStatus ( void )
{
	
	SCSI_Sense_Data				senseBuffer		= { 0 };
	IOMemoryDescriptor *		bufferDesc		= NULL;
	SCSIServiceResponse 		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier			request			= NULL;
	bool						driveReady 		= false;
	bool						result 			= true;
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) &senseBuffer,
													kSenseDefaultSize,
													kIODirectionIn );
	
	check ( bufferDesc );
	
	request = GetSCSITask ( );
	
	check ( request );
	
	do
	{
		
		if ( TEST_UNIT_READY ( request, 0 ) == true )
		{
			
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
			
		}
		
		if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
		{
			
			bool validSense = false;
			
			if ( GetTaskStatus ( request ) == kSCSITaskStatus_CHECK_CONDITION )
			{
				
				validSense = GetAutoSenseData ( request, &senseBuffer );
				if ( validSense == false )
				{
					
					if ( REQUEST_SENSE ( request, bufferDesc, kSenseDefaultSize, 0  ) == true )
					{
						
						// The command was successfully built, now send it
						serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
						
					}
					
					if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
					{
						
						validSense = true;
						
					}
					
				}
				
				if ( validSense == true )
				{
					
					if ( ( ( senseBuffer.SENSE_KEY  & kSENSE_KEY_Mask ) == kSENSE_KEY_NOT_READY  ) &&
							( senseBuffer.ADDITIONAL_SENSE_CODE == 0x04 ) &&
							( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x01 ) )
					{
						
						STATUS_LOG ( ( "%s::drive not ready\n", getName ( ) ) );
						driveReady = false;
						IOSleep ( 200 );
						
					}
					
					else if ( ( ( senseBuffer.SENSE_KEY & kSENSE_KEY_Mask ) == kSENSE_KEY_NOT_READY  ) &&
							( senseBuffer.ADDITIONAL_SENSE_CODE == 0x04 ) &&
							( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x02 ) )
					{
						
						// The drive needs to be spun up. Issue a START_STOP_UNIT to it.
						if ( START_STOP_UNIT ( request, 0x00, 0x00, 0x00, 0x01, 0x00 ) == true )
						{
							
							serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
							
						}
						
					}
					
					else
					{
						
						driveReady = true;
						STATUS_LOG ( ( "%s::drive READY\n", getName ( ) ) );
						
					}
					
					STATUS_LOG ( ( "sense data: %01x, %02x, %02x\n",
								 ( senseBuffer.SENSE_KEY  & kSENSE_KEY_Mask ),
								   senseBuffer.ADDITIONAL_SENSE_CODE,
								   senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER ) );
					
				}
				
			}
			
			else
			{
				
				driveReady = true;
				
			}
			
		}
		
		else
		{
			
			// the command failed - perhaps the device was hot unplugged
			// give other threads some time to run.
			IOSleep ( 200 );
			
		}
	
	// check isInactive in case device was hot unplugged during sleep
	// and we are in an infinite loop here
	} while ( ( driveReady == false ) && ( isInactive ( ) == false ) );
	
	bufferDesc->release ( );
	bufferDesc = NULL;
	
	ReleaseSCSITask ( request );
	
	result = isInactive ( ) ? false : true;
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ EnablePolling - Schedules the polling thread to run			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::EnablePolling ( void )
{		
	
	AbsoluteTime	time;
	
	// No reason to start a thread if we've been terminatated
	require ( ( isInactive ( ) == false ), Exit );
	require ( fPollingThread, Exit );
	require ( ( fPollingMode != kPollingMode_Suspended ), Exit );
	
	// Retain ourselves so that this object doesn't go away
	// while we are polling
	
	retain ( );
	
	clock_interval_to_deadline ( 1000, kMillisecondScale, &time );
	thread_call_enter_delayed ( fPollingThread, time );
	
	
Exit:
	
	
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ DisablePolling - Unschedules the polling thread if it hasn't run yet
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::DisablePolling ( void )
{		
	
	fPollingMode = kPollingMode_Suspended;
	
	// Cancel the thread if it is scheduled to run
	require ( thread_call_cancel ( fPollingThread ), Exit );
	
	// It was running, so we balance out the retain()
	// with a release()
	release ( );
	
	
Exit:
	
	
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ DetermineDeviceCharacteristics - Determines device characteristics
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::DetermineDeviceCharacteristics ( void )
{
	
	SCSIServiceResponse 			serviceResponse 	= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier				request 			= NULL;
	IOBufferMemoryDescriptor *		buffer	 			= NULL;
	SCSICmd_INQUIRY_StandardData * 	inquiryBuffer 		= NULL;
	UInt8							inquiryBufferSize	= 0;
	UInt8							loop				= 0;
	bool							succeeded			= false;
	
	STATUS_LOG ( ( "%s::%s called\n", getName ( ), __FUNCTION__ ) );
	
	if ( fDefaultInquiryCount == 0 )
	{
		
		// There is no default Inquiry count for this device, use the standard
		// structure size.
		STATUS_LOG ( ( "%s: use sizeof(SCSICmd_INQUIRY_StandardData) for Inquiry.\n", getName ( ) ) );
		inquiryBufferSize = sizeof ( SCSICmd_INQUIRY_StandardData );
		
	}
	
	else
	{
		
		// This device has a default inquiry count, use it.
		STATUS_LOG ( ( "%s: use fDefaultInquiryCount for Inquiry.\n", getName ( ) ) );
		inquiryBufferSize = fDefaultInquiryCount;
		
	}
	
	buffer = IOBufferMemoryDescriptor::withCapacity ( inquiryBufferSize, kIODirectionIn );
	require_nonzero_string ( buffer, ErrorExit,
							 "Couldn't allocate INQUIRY memory descriptor" );
	
	inquiryBuffer = ( SCSICmd_INQUIRY_StandardData * ) buffer->getBytesNoCopy ( );
	require_nonzero_string ( inquiryBuffer, ReleaseDescriptor,
							 "Couldn't allocate INQUIRY buffer" );
	
	request = GetSCSITask ( );
	require_nonzero ( request, ReleaseDescriptor );
	
	for ( loop = 0; ( ( loop < kMaxRetryCount ) && ( isInactive ( ) == false ) ); loop++ )
	{
		
		if ( INQUIRY ( 	request,
						buffer,
						0,
						0,
						0x00,
						inquiryBufferSize,
						0 ) == true )
		{
			
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
			
		}
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			
			succeeded = true;
			break;
			
		}
		
	}
	
	require ( succeeded, ReleaseTask );
	
	// Save ANSI version of the device
	SetANSIVersion ( inquiryBuffer->VERSION & kINQUIRY_ANSI_VERSION_Mask );
	
	// Deprecated, but left here for sake of compatibility
	fANSIVersion = GetANSIVersion ( );
	
	if ( ( inquiryBuffer->RMB & kINQUIRY_PERIPHERAL_RMB_BitMask ) ==
		   kINQUIRY_PERIPHERAL_RMB_MediumRemovable )
	{
		
		STATUS_LOG ( ( "Media is removable\n" ) );
		fMediaIsRemovable = true;
		
	}
	
	else
	{
		
		STATUS_LOG ( ( "Media is NOT removable\n" ) );
		fMediaIsRemovable = false;
		
	}
	
	buffer->release ( );
	buffer = NULL;
	
	// There are a whole class of devices which do not like to be sent
	// any MODE_SENSE or MODE_SELECT commands which they don't implement.
	// Since removable devices don't usually have a cache, we restrict
	// the device set to fixed hard disks for the following calls.
	require ( ( fMediaIsRemovable == false ), ReleaseTask );
	
	// Check for any caching support
	buffer = IOBufferMemoryDescriptor::withCapacity ( kCachingModePageSize,
													  kIODirectionOutIn );
	require_nonzero ( buffer, ReleaseTask );
	
	if ( MODE_SENSE_6 ( request,
						buffer,
						0x00,
						kModePageControlSavedValues,
						kCachingModePageCode,
						kCachingModePageSize,
						0x00 ) == true )
	{
		
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
		
	}
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		UInt8 *		cachePage				= ( UInt8 * ) buffer->getBytesNoCopy ( );
		UInt8		blockDescriptorLength	= 0;
		UInt8		minSize					= 0;
		bool		WCEBit					= false;
				
		// Sanity check on buffer size
		minSize = kModeSense6ParameterHeaderSize + kCachingModePageMinSize + 2;
		require ( ( cachePage[0] + sizeof ( UInt8 ) ) >= minSize, ReleaseTask );
		
		// Get the block descriptor length
		blockDescriptorLength = cachePage[3];
		
		// Sanity check on returned data from drive
		require ( ( cachePage[blockDescriptorLength + kModeSense6ParameterHeaderSize] & 0x3F ) == kCachingModePageCode, ReleaseTask );
		require ( ( cachePage[blockDescriptorLength + kModeSense6ParameterHeaderSize + 1] ) >= kCachingModePageMinSize, ReleaseTask );
		
		// Set the page code in the buffer to the cache page code
		cachePage[blockDescriptorLength + kModeSense6ParameterHeaderSize] = kCachingModePageCode;
		
		// Find out if the WCE bit is set.
		WCEBit = cachePage[blockDescriptorLength + 6] & kWriteCacheEnabledMask;
		
		// Write back out the saved bits to enable the saved behavior
		if ( MODE_SELECT_6 ( request,
							 buffer,
							 0x01,	// PageFormat		= 1
							 0x00,	// SaveParameters	= 0
							 cachePage[0] + sizeof ( UInt8 ),
							 0x00 ) == true )
		{
			
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
			
			if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
				 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
			{
				
				// Check to see if the actual WriteCacheEnable (WCE) bit was set
				// in the saved mode page values. If so, then the write cache is
				// now enabled.
				if ( WCEBit == true )
				{
					
					fWriteCacheEnabled = true;
					
				}
				
			}
			
		}
		
	}
	
	
ReleaseTask:
	
	
	require_nonzero_quiet ( request, ReleaseDescriptor );
	ReleaseSCSITask ( request );
	request = NULL;
	
	
ReleaseDescriptor:
	
	
	require_nonzero_quiet ( buffer, ErrorExit );
	buffer->release ( );
	buffer = NULL;
	
	
ErrorExit:
	
	
	STATUS_LOG ( ( "%s::%s succeeded = %d\n", getName ( ), __FUNCTION__, succeeded ) );
	
	return succeeded;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetMediumCharacteristics - Sets medium characteristics		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::SetMediumCharacteristics (
							UInt32	blockSize,
							UInt32	blockCount )
{
	
	UInt64		maxBlocksRead	= 0;
	UInt64		maxBlocksWrite	= 0;
	
	STATUS_LOG ( ( "mediumBlockSize = %ld, blockCount = %ld\n",
					blockSize, blockCount ) );
	
	fMediumBlockSize	= blockSize;
	fMediumBlockCount	= blockCount;
	
	maxBlocksRead	= ReportDeviceMaxBlocksReadTransfer ( );
	maxBlocksWrite	= ReportDeviceMaxBlocksWriteTransfer ( );
	
	setProperty ( kIOMaximumBlockCountReadKey, maxBlocksRead, 64 );
	setProperty ( kIOMaximumBlockCountWriteKey, maxBlocksWrite, 64 );
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ResetMediumCharacteristics -	Resets medium characteristics to known
//									values							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::ResetMediumCharacteristics ( void )
{
	
	fMediumBlockSize	= 0;
	fMediumBlockCount	= 0;
	fMediumPresent		= false;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ CreateStorageServiceNub - Creates the linkage object for IOStorageFamily
//								to use.								[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::CreateStorageServiceNub ( void )
{
	
	IOService * 	nub = NULL;
	
	nub = OSTypeAlloc ( IOBlockStorageServices );
	require_nonzero ( nub, ErrorExit );
	
	nub->init ( );
	require ( nub->attach ( this ), ErrorExit );
	
	nub->registerService ( );
	nub->release ( );
	
	return;
	
	
ErrorExit:
	
	
	PANIC_NOW ( ( "IOSCSIBlockCommandsDevice::CreateStorageServiceNub failed" ) );
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ ProcessPoll - Processes a poll for media.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::ProcessPoll ( void )
{
	
	switch ( fPollingMode )
	{
		
		case kPollingMode_NewMedia:
		{
			PollForNewMedia ( );
		}
		break;
		
		case kPollingMode_MediaRemoval:
		{
			PollForMediaRemoval ( );
		}
		break;
		
		default:
		{
			
			// This is an unknown polling mode -- do nothing.
			ERROR_LOG ( ( "%s:ProcessPoll Unknown polling mode.\n", getName ( ) ) );
			
		}
		break;
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ PollForNewMedia - Polls for new media insertion.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::PollForNewMedia ( void )
{
	
	bool			mediaFound 	= false;
	UInt64			blockCount	= 0;
	UInt64			blockSize	= 0;
	
	// Since this is a poll for new media, 	
	fMediumPresent = false;
	
	mediaFound = DetermineMediaPresence ( );
	require_quiet ( mediaFound, Exit );
	
	// If we got here, then we have found media
	if ( fMediaIsRemovable == true )
	{
		fMediumRemovalPrevented = PreventMediumRemoval ( );
	}
	
	else
	{
		fMediumRemovalPrevented = true;
	}
	
	require ( DetermineMediumCapacity ( &blockSize, &blockCount ), Exit );
	
	// What happens if the medium is unformatted?
	// A check should be added to handle this case.
	
	SetMediumCharacteristics ( blockSize, blockCount );
	
	fMediumIsWriteProtected = DetermineMediumWriteProtectState ( );
	
	fMediumPresent	= true;
	fPollingMode	= kPollingMode_Suspended;
	
	SetMediumIcon ( );
	
	// Message up the chain that we have media
	messageClients ( kIOMessageMediaStateHasChanged,
					 ( void * ) kIOMediaStateOnline,
					 0 );
	
	if ( fMediumRemovalPrevented == false )
	{
		
		// Media is not locked into the drive, so this is most likely
		// a manually ejectable device, start polling for media removal.
		fPollingMode = kPollingMode_MediaRemoval;
		
	}
	
	
Exit:
	
	
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ DetermineMediaPresence - Checks if media has been inserted.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::DetermineMediaPresence ( void )
{
	
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier		request			= NULL;
	bool					mediaFound 		= false;
	
	request = GetSCSITask ( );
	require_nonzero ( request, ErrorExit );
	
	// Do a TEST_UNIT_READY to generate sense data
	if ( TEST_UNIT_READY ( request, 0 ) == true )
	{
		
		// The command was successfully built, now send it, set timeout to 10 seconds.
		serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
		
	}
	
	if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
		
		if ( GetTaskStatus ( request ) == kSCSITaskStatus_CHECK_CONDITION )
		{
			
			bool					validSense 	= false;
			SCSI_Sense_Data			senseBuffer	= { 0 };
			IOMemoryDescriptor *	bufferDesc	= NULL;
			
			validSense = GetAutoSenseData ( request, &senseBuffer );
			if( validSense == false )
			{
				
				bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) &senseBuffer,
																kSenseDefaultSize,
																kIODirectionIn );
				require_nonzero ( bufferDesc, ReleaseTask );
				
				// Get the sense data to determine if media is present.
				if ( REQUEST_SENSE ( request, bufferDesc, kSenseDefaultSize, 0 ) == true )
				{
					
					// The command was successfully built, now send it
					serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
					
				}
				
				bufferDesc->release ( );
				bufferDesc = NULL;
				
				if ( ( serviceResponse != kSCSIServiceResponse_TASK_COMPLETE ) ||
		 			 ( GetTaskStatus ( request ) != kSCSITaskStatus_GOOD ) )
		 		{
					
					ERROR_LOG ( ( "%s: REQUEST_SENSE failed\n", getName ( ) ) );
					goto ReleaseTask;
					
		 		}
				
			}
			
			if ( ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x00 ) &&
				 ( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x00 ) )
			{
				
				STATUS_LOG ( ( "Media found\n" ) );
				mediaFound = true;
				
			}
			
			else
			{
				
				ERROR_LOG ( ( "ASC = 0x%02x, ASCQ = 0x%02x\n",
								senseBuffer.ADDITIONAL_SENSE_CODE,
								senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER ) );
				
			}
			
		}
		
		else
		{
			
			STATUS_LOG ( ( "Media found\n" ) );
			mediaFound = true;
			
		}
		
	}
	
	
ReleaseTask:
	
	
	ReleaseSCSITask ( request );
	
	
ErrorExit:
	
	
	return mediaFound;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ PreventMediumRemoval - Prevents Medium Removal.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::PreventMediumRemoval ( void )
{
	
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier		request			= NULL;
	bool  					mediumLocked	= false;
	
	// Before forcing work to be done, verify that it is necessary by checking if this is a known
	// manual eject device.
	require ( ( fKnownManualEject == false ), Exit );
	
	request = GetSCSITask ( );
	require_nonzero ( request, Exit );
	
	if ( PREVENT_ALLOW_MEDIUM_REMOVAL ( request, kMediaStateLocked, 0 ) == true )
	{
		
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
		
	}
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
	 	 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		mediumLocked = true;
		
	}
	
	ReleaseSCSITask ( request );
	
	
Exit:
	
	
	return mediumLocked;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ DetermineMediumCapacity - Determines capacity of the medium.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::DetermineMediumCapacity (
					UInt64 * blockSize,
					UInt64 * blockCount )
{
	
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	UInt32					capacityData[2] = { 0 };
	IOMemoryDescriptor *	bufferDesc		= NULL;
	SCSITaskIdentifier		request			= NULL;
	bool					result			= false;
	
	*blockSize 	= 0;
	*blockCount = 0;
	
	request = GetSCSITask ( );
	require_nonzero ( request, ErrorExit );
	
	bufferDesc = IOMemoryDescriptor::withAddress ( capacityData,
												   kCapacityDataBufferSize,
												   kIODirectionIn );
	require_nonzero ( bufferDesc, ReleaseTask );
	
	// We found media, get its capacity
	if ( READ_CAPACITY ( request, bufferDesc, 0, 0x00, 0, 0 ) == true )
	{
		
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
		
	}
	
	if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
	{
		
		*blockSize 	= ( UInt64 ) OSSwapBigToHostInt32 ( capacityData[1] );
		*blockCount = ( UInt64 ) ( OSSwapBigToHostInt32 ( capacityData[0] ) + 1 );
		STATUS_LOG ( ( "%s: Media capacity: %x and block size: %x\n",
						getName ( ), ( UInt32 ) *blockCount, ( UInt32 ) *blockSize ) );
		result = true;
		
	}
	
	bufferDesc->release ( );
	bufferDesc = NULL;
	
	
ReleaseTask:
	
	
	require_nonzero_quiet ( request, ErrorExit );
	ReleaseSCSITask ( request );
	request = NULL;
	
	
ErrorExit:
	
	
	return result;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ DetermineMediumWriteProtectState - Determines medium write protect state.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIBlockCommandsDevice::DetermineMediumWriteProtectState ( void )
{
	
	SCSIServiceResponse 	serviceResponse 		= kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	IOMemoryDescriptor *	bufferDesc				= NULL;
	SCSITaskIdentifier		request					= NULL;
	bool					writeProtectDetermined 	= false;
	bool					mediumIsProtected 		= true;
	SCSI_Sense_Data			senseBuffer				= { 0 };
	UInt8					modeBuffer[16]			= { 0 };
	
	// For now, report all fixed disks as writable since most have
	// no way of changing this.
	require_action_quiet ( fMediaIsRemovable, Exit, mediumIsProtected = false );
	
	request = GetSCSITask ( );
	require_nonzero ( request, ErrorExit );
	
	// Send a Request Sense to the device, this seems to make some happy again.
	bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) &senseBuffer,
													kSenseDefaultSize,
													kIODirectionIn );
	check ( bufferDesc );
	
	if ( bufferDesc != NULL )
	{
		
		// Issue a Request Sense
		// Whether the command completes successfully or not is irrelevent.
		if ( REQUEST_SENSE ( request, bufferDesc, kSenseDefaultSize, 0 ) == true )
		{
			
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
			
		}
		
		// release the sense data buffer;
		bufferDesc->release ( );
		bufferDesc = NULL;
		
	}
	
	// Now back to normal programming.
	bufferDesc = IOMemoryDescriptor::withAddress ( 	modeBuffer,
													8,
													kIODirectionIn );
	require_nonzero ( bufferDesc, ReleaseTask );
	
	if ( GetANSIVersion ( ) == kINQUIRY_ANSI_VERSION_NoClaimedConformance )
	{
		
		// The device does not claim compliance with any ANSI version, so it
		// is most likely an ATAPI device, try the 10 byte command first.
		
		if ( MODE_SENSE_10 ( 	request,
								bufferDesc,
								0x00,
								0x00,
								0x00,	// Normally, we set DBD=1, but since we're only
								0x3F,	// interested in modeBuffer[3], we set DBD=0 since
								8,		// it makes legacy devices happier
								0 ) == true )
		{
			
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
			
		}
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			
			STATUS_LOG ( ( "%s: Returned Mode sense data: ", getName ( ) ) );
			
#if SCSI_SBC_DEVICE_DEBUGGING_LEVEL >= 3
				
				for ( UInt32 i = 0; i < 8; i++ )
				{
					STATUS_LOG ( ( "%x: ", modeBuffer[i] ) );
				}
				
				STATUS_LOG ( ( "\n" ) );
				
#endif // SCSI_SBC_DEVICE_DEBUGGING_LEVEL >= 3
			
			if ( ( modeBuffer[3] & kWriteProtectMask ) == 0 )
			{
			 	mediumIsProtected = false;
			}
			
			writeProtectDetermined = true;
			
		}
		
	}
	
	// Check if the write protect status has been successfully determined.
	if ( writeProtectDetermined == false )
	{
		
		// Either this device reports an ANSI version, or the 10 Byte command failed.
		// Try the six byte mode sense.	
		if ( MODE_SENSE_6 ( request,
							bufferDesc,
							0x00,
							0x00,	// Normally, we set DBD=1, but since we're only
							0x3F,	// interested in modeBuffer[3], we set DBD=0 since
							8,		// it makes legacy devices happier
							0 ) == true )
		{
			
			// The command was successfully built, now send it
			serviceResponse = SendCommand ( request, kThirtySecondTimeoutInMS );
			
		}
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			
			STATUS_LOG ( ( "%s: Returned Mode sense data: ", getName ( ) ) );
			
		#if SCSI_SBC_DEVICE_DEBUGGING_LEVEL
			for ( UInt32 i = 0;i < 8; i++ )
			{
				STATUS_LOG ( ( "%x: ", modeBuffer[i] ) );
			}
			
			STATUS_LOG ( ( "\n" ) );
		#endif // SCSI_SBC_DEVICE_DEBUGGING_LEVEL
			
			if ( ( modeBuffer[2] & kWriteProtectMask ) == 0 )
			{
				mediumIsProtected = false;
			}
			
		}
		
		else
		{
			
			ERROR_LOG ( ( "%s: Mode Sense failed\n", getName ( ) ) );
			
			// The mode sense failed, mark as write protected to be safe.
			mediumIsProtected = true;
			
		}
		
	}
	
	bufferDesc->release ( );
	bufferDesc = NULL;
	
	
ReleaseTask:
	
	
	require_nonzero_quiet ( request, ErrorExit );
	ReleaseSCSITask ( request );
	request = NULL;
	
	
ErrorExit:
Exit:
	
	
	return mediumIsProtected;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SetMediumIcon - Sets an icon key in the registry if desired.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::SetMediumIcon ( void )
{
	
	STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::SetMediumIcon called\n" ) );
	
	// Methodology for setting icons
	// 1. Find out if an icon key is already present. If there is one, do
	// nothing since it might be from a plist entry. This makes it easy for
	// subclassers to set a media property for their device (if it is known
	// and can only be one media type - e.g. Zip) in their PB project, rather
	// than adding a method to their subclass.
	// 
	// 2. Find out if the device is on an internal bus or external bus and
	// what type of bus it is. If it is a bus for which we have a special
	// icon at this point in time, then we set that icon key. The icons are
	// located in the IOSCSIArchitectureModelFamily.kext/Contents/Resources
	// directory.
	//
	// We currently have icons for the following:
	//	¥ Firewire HD
	//	¥ Fibre Channel HD
	//	¥ USB HD
	//	¥ SuperDisk
	//	¥ MagnetoOptical
	//	¥ SmartMedia
	//	¥ MemoryStick
	//
	// We plan to have the following icons supported once we get artwork/licensing:
	//	¥ CompactFlash
	//	¥ Clik!
	//	¥ Zip
	//	¥ Jaz
	
	// Step 1. Find out if a media icon is present
	if ( getProperty ( kIOMediaIconKey, gIOServicePlane ) == NULL )
	{
		
		OSDictionary *	dict = NULL;
		
		STATUS_LOG ( ( "No current icon key\n" ) );
		
		// Step 2. No icon is present, see if we can provide one.
		dict = GetProtocolCharacteristicsDictionary ( );
		if ( dict != NULL )
		{
			
			OSString *	protocolString 	= NULL;
			
			STATUS_LOG ( ( "Got Protocol Characteristics Dictionary\n" ) );

			protocolString = OSDynamicCast ( OSString, dict->getObject ( kIOPropertyPhysicalInterconnectTypeKey ) );
			if ( protocolString != NULL )
			{
				
				const char *	protocol = NULL;
				
				STATUS_LOG ( ( "Got Protocol string\n" ) );

				protocol = protocolString->getCStringNoCopy ( );
				if ( protocol != NULL )
				{
					
					OSString *	identifier		= NULL;
					OSString *	resourceFile	= NULL;
					
					STATUS_LOG ( ( "Protocol = %s\n", protocol ) );
					
					identifier 	= OSString::withCString ( kIOSCSIArchitectureBundleIdentifierKey );
					dict		= OSDictionary::withCapacity ( 2 );
					
					if ( fMediaIsRemovable == false )
					{
						
						// If the protocol is FireWire, it needs an icon.
						if ( strcmp ( protocol, "FireWire" ) == 0 )
						{
							
							resourceFile = OSString::withCString ( kFireWireHDIconKey );
							
						}
						
						// If the protocol is USB and media is not removable, it needs an icon.
						if ( strcmp ( protocol, "USB" ) == 0 )
						{
							
							resourceFile = OSString::withCString ( kUSBHDIconKey );
							
						}
						
						// If the protocol is FibreChannel, it needs an icon.
						if ( strcmp ( protocol, "Fibre Channel Interface" ) == 0 )
						{
							
							resourceFile = OSString::withCString ( kFibreChannelHDIconKey );
							
						}
						
					}
					
					// Do we have an icon to set?
					if ( resourceFile != NULL )
					{
						
						STATUS_LOG ( ( "Resource file is non-NULL\n" ) );

						// Make sure other resources are allocated
						if ( ( dict != NULL ) && ( identifier != NULL ) )
						{
							
							STATUS_LOG ( ( "Setting keys\n" ) );
							
							dict->setObject ( kCFBundleIdentifierKey, identifier );
							dict->setObject ( kIOBundleResourceFileKey, resourceFile );
							
							setProperty ( kIOMediaIconKey, dict );
							
						}
						
						resourceFile->release ( );
						
					}
					
					if ( dict != NULL )
					{
						
						dict->release ( );
						dict = NULL;
						
					}
					
					if ( identifier != NULL )
					{
						
						identifier->release ( );
						identifier = NULL;
						
					}
					
				}
				
			}
			
		}
		
	}
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ PollForMediaRemoval - Polls for media removal.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::PollForMediaRemoval ( void )
{
	
	SCSIServiceResponse		serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier		request			= NULL;
	
	request = GetSCSITask ( );
	require_nonzero ( request, ErrorExit );
	
	// Do a TEST_UNIT_READY to generate sense data
	if ( TEST_UNIT_READY ( request, 0 ) == true )
	{
		
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
		
	}
	
	if ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE )
	{
		
		if ( GetTaskStatus ( request ) == kSCSITaskStatus_CHECK_CONDITION )
		{
			
			bool						validSense 	= false;
			SCSI_Sense_Data				senseBuffer	= { 0 };
			IOMemoryDescriptor *		bufferDesc	= NULL;
			
			validSense = GetAutoSenseData ( request, &senseBuffer );
			if ( validSense == false )
			{
				
				bufferDesc = IOMemoryDescriptor::withAddress ( ( void * ) &senseBuffer,
																kSenseDefaultSize,
																kIODirectionIn );
				require_nonzero ( bufferDesc, ReleaseTask );
				
				if ( REQUEST_SENSE ( request, bufferDesc, kSenseDefaultSize, 0 ) == true )
				{
					
					// The command was successfully built, now send it
					serviceResponse = SendCommand ( request, kTenSecondTimeoutInMS );
					
				}
				
				bufferDesc->release ( );
				bufferDesc = NULL;
				
				require ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ), ReleaseTask );
				require ( ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ), ReleaseTask );
				
			}
			
			// Check the sense data to see if media is no longer present ( ASC == 0x3A )
			// or if media has changed ( ASC==0x28, ASCQ==0x00 )
			if ( ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x3A ) ||
			   ( ( senseBuffer.ADDITIONAL_SENSE_CODE == 0x28 ) &&
				 ( senseBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER == 0x00 ) ) )
			{
				
				ERROR_LOG ( ( "Media was removed. Tearing down the media object." ) );
				
				// Media was removed, set the polling to determine when new media has been inserted
				fPollingMode = kPollingMode_NewMedia;
				
				// Message up the chain that we do not have media
				messageClients ( kIOMessageMediaStateHasChanged,
								 ( void * ) kIOMediaStateOffline );
				
				ResetMediumCharacteristics ( );
				EnablePolling ( );
				
			}
			
		}
		
	}
	
	
ReleaseTask:
	
	
	require_nonzero_quiet ( request, ErrorExit );
	ReleaseSCSITask ( request );
	request = NULL;
	
	
ErrorExit:
	
	
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ IssueRead - Issues a synchronous read command.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIBlockCommandsDevice::IssueRead (
							IOMemoryDescriptor *	buffer,
							UInt64					startBlock,
							UInt64					blockCount )
{
	
	SCSIServiceResponse 	serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier		request			= NULL;
	IOReturn				status			= kIOReturnNoResources;
	
	request = GetSCSITask ( );
	require_nonzero ( request, ErrorExit );
	
	if ( READ_10 ( 	request,
					buffer,
  					fMediumBlockSize,
					0,
					0,
					0,
					( SCSICmdField4Byte ) startBlock,
					( SCSICmdField2Byte ) blockCount,
					0 ) == true )
	{
		
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, fReadTimeoutDuration );
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			
			status = kIOReturnSuccess;
			
		}
		
		else
		{
			
			status = kIOReturnIOError;
			
		}
		
	}
	
	else
	{
		
		status = kIOReturnBadArgument;
		
	}
	
	ReleaseSCSITask ( request );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ IssueRead - Issues an asynchronous read command.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIBlockCommandsDevice::IssueRead (
							IOMemoryDescriptor *	buffer,
							UInt64					startBlock,
							UInt64					blockCount,
							void *					clientData )
{
	
	IOReturn 				status 	= kIOReturnNoResources;
	SCSITaskIdentifier		request	= NULL;
	
	request = GetSCSITask ( );
	require_nonzero ( request, ErrorExit );
	
	if ( READ_10 (	request,
					buffer,
	  				fMediumBlockSize,
					0,
					0,
					0,
					( SCSICmdField4Byte ) startBlock,
					( SCSICmdField2Byte ) blockCount,
					0 ) == true )
	{
		
		// The command was successfully built, now send it
		SetApplicationLayerReference ( request, clientData );
		STATUS_LOG ( ( "IOSCSIBlockCommandsDevice::IssueRead send command.\n" ) );
		
		SendCommand ( request,
					  fReadTimeoutDuration,
					  &IOSCSIBlockCommandsDevice::AsyncReadWriteComplete );
		
		status = kIOReturnSuccess;
		
	}
	
	else
	{
		
		ReleaseSCSITask ( request );
		request	= NULL;
		status	= kIOReturnBadArgument;
		
	}
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ IssueRead - Issues a synchronous write command.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIBlockCommandsDevice::IssueWrite ( IOMemoryDescriptor *	buffer,
										UInt64					startBlock,
										UInt64					blockCount )
{
	
	SCSIServiceResponse 	serviceResponse = kSCSIServiceResponse_SERVICE_DELIVERY_OR_TARGET_FAILURE;
	SCSITaskIdentifier		request			= NULL;
	IOReturn				status 			= kIOReturnNoResources;
	
	request = GetSCSITask ( );
	require_nonzero ( request, ErrorExit );
	
	if ( WRITE_10 ( request,
					buffer,
					fMediumBlockSize,
					0,
					0,
					0,
					0,
					( SCSICmdField4Byte ) startBlock,
					( SCSICmdField2Byte ) blockCount,
					0 ) == true )
	{
		
		// The command was successfully built, now send it
		serviceResponse = SendCommand ( request, fWriteTimeoutDuration );
		
		if ( ( serviceResponse == kSCSIServiceResponse_TASK_COMPLETE ) &&
			 ( GetTaskStatus ( request ) == kSCSITaskStatus_GOOD ) )
		{
			
			status = kIOReturnSuccess;
			
		}
		
		else
		{
			
			status = kIOReturnIOError;
			
		}
		
	}
	
	else
	{
		
		status = kIOReturnBadArgument;
		
	}
	
	ReleaseSCSITask ( request );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ IssueRead - Issues an asynchronous write command.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

IOReturn
IOSCSIBlockCommandsDevice::IssueWrite (
						IOMemoryDescriptor *	buffer,
						UInt64					startBlock,
						UInt64					blockCount,
						void *					clientData )
{
	IOReturn				status	= kIOReturnNoResources;
	SCSITaskIdentifier		request	= NULL;
	
	request = GetSCSITask ( );
	require_nonzero ( request, ErrorExit );
	
	if ( WRITE_10 ( request,
					buffer,
   					fMediumBlockSize,
					0,
					0,
					0,
					0,
					( SCSICmdField4Byte ) startBlock,
					( SCSICmdField2Byte ) blockCount,
					0 ) == true )
	{
		
		// The command was successfully built, now send it
		SetApplicationLayerReference ( request, clientData );
		SendCommand ( request,
					  fWriteTimeoutDuration,
					  &IOSCSIBlockCommandsDevice::AsyncReadWriteComplete );
		
		status = kIOReturnSuccess;
		
	}
	
	else
	{
		
		ReleaseSCSITask ( request );
		request	= NULL;
		status	= kIOReturnBadArgument;
		
	}	
	
	
ErrorExit:
	
	
	return status;
	
}


#if 0
#pragma mark -
#pragma mark ¥ Static Methods
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ AsyncReadWriteComplete - 	Static completion routine for
//								read/write requests.		  [STATIC][PRIVATE]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::AsyncReadWriteComplete (
								SCSITaskIdentifier request )
{
	
	void *								clientData	= NULL;
	IOSCSIBlockCommandsDevice	*		taskOwner	= NULL;
	SCSITask *							task		= NULL;
	IOReturn							status		= kIOReturnIOError;
	UInt64								actCount	= 0;
	
	task = OSDynamicCast ( SCSITask, request );
	require_nonzero ( task, FatalError );
	
	taskOwner = OSDynamicCast ( IOSCSIBlockCommandsDevice,
								task->GetTaskOwner ( ) );
	require_nonzero ( taskOwner, FatalError );
	
	// Extract the client data from the SCSITask	
	clientData = task->GetApplicationLayerReference ( );
	require_nonzero ( clientData, FatalError );
	
	if ( ( task->GetServiceResponse ( ) == kSCSIServiceResponse_TASK_COMPLETE ) &&
		 ( task->GetTaskStatus ( ) == kSCSITaskStatus_GOOD ) )
	{
		
		status = kIOReturnSuccess;
		actCount = task->GetRealizedDataTransferCount ( );
		
	}
	
	else
	{
		
		// Set a generic IO error for starters
		status = kIOReturnIOError;
		
		// Either the task never completed or we have a status other than GOOD,
		// return an error.		
		if ( task->GetTaskStatus ( ) == kSCSITaskStatus_CHECK_CONDITION )
		{
			
			SCSI_Sense_Data		senseDataBuffer;
			bool				senseIsValid;
			
			senseIsValid = task->GetAutoSenseData ( &senseDataBuffer, sizeof ( senseDataBuffer ) );
			if ( senseIsValid )
			{
				
				ERROR_LOG ( ( "READ or WRITE failed, ASC = 0x%02x, ASCQ = 0x%02x\n",
				senseDataBuffer.ADDITIONAL_SENSE_CODE,
				senseDataBuffer.ADDITIONAL_SENSE_CODE_QUALIFIER ) );
				
			}
			
		}
		
	}
	
	IOBlockStorageServices::AsyncReadWriteComplete ( clientData, status, actCount );
	
	taskOwner->ReleaseSCSITask ( request );
	
	return;
	
	
FatalError:
	
	
	IOPanic ( "SAM SBC: error completing I/O due to bad completion data" );
	
	return;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ sProcessPoll - Static method called to poll for media.
//															[STATIC][PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

void
IOSCSIBlockCommandsDevice::sProcessPoll ( void * pdtDriver, void * refCon )
{
	
	IOSCSIBlockCommandsDevice *	driver = NULL;
	
	driver = ( IOSCSIBlockCommandsDevice * ) pdtDriver;
	require_nonzero ( driver, ErrorExit );
	
	driver->ProcessPoll ( );
	
	if ( driver->fPollingMode != kPollingMode_Suspended )
	{
		
		// schedule the poller again
		driver->EnablePolling ( );
		
	}
	
	// drop the retain associated with this poll
	driver->release ( );
	
	
ErrorExit:
	
	
	return;
	
}


#if 0
#pragma mark -
#pragma mark ¥ VTable Padding
#pragma mark -
#endif


OSMetaClassDefineReservedUsed ( IOSCSIBlockCommandsDevice, 1 );	/* PowerDownHandler */
OSMetaClassDefineReservedUsed ( IOSCSIBlockCommandsDevice, 2 );	/* SetMediumIcon 	*/

// Space reserved for future expansion.
OSMetaClassDefineReservedUnused ( IOSCSIBlockCommandsDevice,  3 );
OSMetaClassDefineReservedUnused ( IOSCSIBlockCommandsDevice,  4 );
OSMetaClassDefineReservedUnused ( IOSCSIBlockCommandsDevice,  5 );
OSMetaClassDefineReservedUnused ( IOSCSIBlockCommandsDevice,  6 );
OSMetaClassDefineReservedUnused ( IOSCSIBlockCommandsDevice,  7 );
OSMetaClassDefineReservedUnused ( IOSCSIBlockCommandsDevice,  8 );
OSMetaClassDefineReservedUnused ( IOSCSIBlockCommandsDevice,  9 );
OSMetaClassDefineReservedUnused ( IOSCSIBlockCommandsDevice, 10 );
OSMetaClassDefineReservedUnused ( IOSCSIBlockCommandsDevice, 11 );
OSMetaClassDefineReservedUnused ( IOSCSIBlockCommandsDevice, 12 );
OSMetaClassDefineReservedUnused ( IOSCSIBlockCommandsDevice, 13 );
OSMetaClassDefineReservedUnused ( IOSCSIBlockCommandsDevice, 14 );
OSMetaClassDefineReservedUnused ( IOSCSIBlockCommandsDevice, 15 );
OSMetaClassDefineReservedUnused ( IOSCSIBlockCommandsDevice, 16 );