/*
 * Copyright (c) 1998-2002 Apple Computer, Inc. All rights reserved.
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


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Includes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// SCSI Architecture Model Family includes
#include <IOKit/scsi-commands/SCSICommandDefinitions.h>
#include "IOSCSIMultimediaCommandsDevice.h"


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Macros
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// Flag to turn on compiling of APIs marked as obsolete
#define INCLUDE_OBSOLETE_APIS					1

#define DEBUG 									0
#define DEBUG_ASSERT_COMPONENT_NAME_STRING		"MMC"

#include "IOSCSIArchitectureModelFamilyDebugging.h"


#if 0
#pragma mark -
#pragma mark ¥ Multimedia Commands Builders
#pragma mark -
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ BLANK - 	Builds a BLANK command.								[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::BLANK (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			IMMED, 
						SCSICmdField3Bit 			BLANKING_TYPE, 
						SCSICmdField4Byte 			START_ADDRESS_TRACK_NUMBER, 
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );

	status = GetSCSIMultimediaCommandObject ( )->BLANK (
													scsiRequest,
													IMMED,
													BLANKING_TYPE,
													START_ADDRESS_TRACK_NUMBER,
													CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ CLOSE_TRACK_SESSION - Builds a CLOSE_TRACK_SESSION command.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::CLOSE_TRACK_SESSION (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			IMMED,
						SCSICmdField1Bit 			SESSION,
						SCSICmdField1Bit 			TRACK,
						SCSICmdField2Byte 			TRACK_NUMBER,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->CLOSE_TRACK_SESSION (
								scsiRequest,
								IMMED,
								SESSION,
								TRACK,
								TRACK_NUMBER,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ FORMAT_UNIT - Builds a FORMAT_UNIT command.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::FORMAT_UNIT (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						IOByteCount					parameterListSize,
						SCSICmdField1Bit 			FMT_DATA,
						SCSICmdField1Bit 			CMP_LIST,
						SCSICmdField3Bit 			FORMAT_CODE,
						SCSICmdField2Byte 			INTERLEAVE_VALUE,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->FORMAT_UNIT (
								scsiRequest,
								dataBuffer,
								parameterListSize,
								FMT_DATA,
								CMP_LIST,
								FORMAT_CODE,
								INTERLEAVE_VALUE,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GET_CONFIGURATION - Builds a GET_CONFIGURATION command.		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::GET_CONFIGURATION (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Bit 			RT,
						SCSICmdField2Byte 			STARTING_FEATURE_NUMBER,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->GET_CONFIGURATION (
								scsiRequest,
								dataBuffer,
								RT,
								STARTING_FEATURE_NUMBER,
								ALLOCATION_LENGTH,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GET_EVENT_STATUS_NOTIFICATION - 	Builds a GET_EVENT_STATUS_NOTIFICATION
//										command.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::GET_EVENT_STATUS_NOTIFICATION (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			IMMED,
						SCSICmdField1Byte 			NOTIFICATION_CLASS_REQUEST,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->GET_EVENT_STATUS_NOTIFICATION (
								scsiRequest,
								dataBuffer,
								IMMED,
								NOTIFICATION_CLASS_REQUEST,
								ALLOCATION_LENGTH,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ GET_PERFORMANCE - Builds a GET_PERFORMANCE command.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::GET_PERFORMANCE (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Bit 			TOLERANCE,
						SCSICmdField1Bit 			WRITE,
						SCSICmdField2Bit 			EXCEPT,
						SCSICmdField4Byte 			STARTING_LBA,
						SCSICmdField2Byte 			MAXIMUM_NUMBER_OF_DESCRIPTORS,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->GET_PERFORMANCE (
								scsiRequest,
								dataBuffer,
								TOLERANCE,
								WRITE,
								EXCEPT,
								STARTING_LBA,
								MAXIMUM_NUMBER_OF_DESCRIPTORS,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ LOAD_UNLOAD_MEDIUM - Builds a LOAD_UNLOAD_MEDIUM command.		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::LOAD_UNLOAD_MEDIUM (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			IMMED,
						SCSICmdField1Bit 			LO_UNLO,
						SCSICmdField1Bit 			START,
						SCSICmdField1Byte 			SLOT,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->LOAD_UNLOAD_MEDIUM (
								scsiRequest,
								IMMED,
								LO_UNLO,
								START,
								SLOT,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	

}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ MECHANISM_STATUS - Builds a MECHANISM_STATUS command.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::MECHANISM_STATUS (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->MECHANISM_STATUS (
								scsiRequest,
								dataBuffer,
								ALLOCATION_LENGTH,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ PAUSE_RESUME - Builds a PAUSE_RESUME command.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::PAUSE_RESUME (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			RESUME,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->PAUSE_RESUME (
								scsiRequest,
								RESUME,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ PLAY_AUDIO_10 - Builds a PLAY_AUDIO_10 command.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::PLAY_AUDIO_10 (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			STARTING_LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			PLAY_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->PLAY_AUDIO_10 (
								scsiRequest,
								RELADR,
								STARTING_LOGICAL_BLOCK_ADDRESS,
								PLAY_LENGTH,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ PLAY_AUDIO_12 - Builds a PLAY_AUDIO_12 command.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::PLAY_AUDIO_12 (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			STARTING_LOGICAL_BLOCK_ADDRESS,
						SCSICmdField4Byte 			PLAY_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->PLAY_AUDIO_12 (
								scsiRequest,
								RELADR,
								STARTING_LOGICAL_BLOCK_ADDRESS,
								PLAY_LENGTH,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ PLAY_AUDIO_MSF - Builds a PLAY_AUDIO_MSF command.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::PLAY_AUDIO_MSF (
						SCSITaskIdentifier			request,
						SCSICmdField3Byte 			STARTING_MSF,
						SCSICmdField3Byte 			ENDING_MSF,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->PLAY_AUDIO_MSF (
								scsiRequest,
								STARTING_MSF,
								ENDING_MSF,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ PLAY_CD - Builds a PLAY_CD command.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::PLAY_CD (
						SCSITaskIdentifier			request,
						SCSICmdField3Bit 			EXPECTED_SECTOR_TYPE,
						SCSICmdField1Bit 			CMSF,
						SCSICmdField4Byte 			STARTING_LOGICAL_BLOCK_ADDRESS,
						SCSICmdField4Byte 			PLAY_LENGTH_IN_BLOCKS,
						SCSICmdField1Bit 			SPEED,
						SCSICmdField1Bit 			PORT2,
						SCSICmdField1Bit 			PORT1,
						SCSICmdField1Bit 			COMPOSITE,
						SCSICmdField1Bit 			AUDIO,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->PLAY_CD (
								scsiRequest,
								EXPECTED_SECTOR_TYPE,
								CMSF,
								STARTING_LOGICAL_BLOCK_ADDRESS,
								PLAY_LENGTH_IN_BLOCKS,
								SPEED,
								PORT2,
								PORT1,
								COMPOSITE,
								AUDIO,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_BUFFER_CAPACITY - Builds a READ_BUFFER_CAPACITY command.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::READ_BUFFER_CAPACITY (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->READ_BUFFER_CAPACITY (
								scsiRequest,
								dataBuffer,
								ALLOCATION_LENGTH,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_10 - Builds a READ_10 command.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::READ_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						UInt32						blockSize,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit 			FUA,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			TRANSFER_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	bool		status 				= false;
	SCSITask *	scsiRequest			= NULL;
	UInt64		requestedByteCount	= 0;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	// Check the validity of the media
	require_nonzero ( blockSize, ErrorExit );
	
	requestedByteCount = TRANSFER_LENGTH * blockSize;
	
	status = GetSCSIBlockCommandObject ( )->READ_10 (
					scsiRequest,
					dataBuffer,
					requestedByteCount,
					DPO,
					FUA,
					RELADR,
					LOGICAL_BLOCK_ADDRESS,
					TRANSFER_LENGTH,
					CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_CD - Builds a READ_CD command.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::READ_CD (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField3Bit 			EXPECTED_SECTOR_TYPE,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			STARTING_LOGICAL_BLOCK_ADDRESS,
						SCSICmdField3Byte 			TRANSFER_LENGTH,
						SCSICmdField1Bit 			SYNC,
						SCSICmdField2Bit 			HEADER_CODES,
						SCSICmdField1Bit 			USER_DATA,
						SCSICmdField1Bit 			EDC_ECC,
						SCSICmdField2Bit 			ERROR_FIELD,
						SCSICmdField3Bit 			SUBCHANNEL_SELECTION_BITS,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->READ_CD (
								scsiRequest,
								dataBuffer,
								EXPECTED_SECTOR_TYPE,
								RELADR,
								STARTING_LOGICAL_BLOCK_ADDRESS,
								TRANSFER_LENGTH,
								SYNC,
								HEADER_CODES,
								USER_DATA,
								EDC_ECC,
								ERROR_FIELD,
								SUBCHANNEL_SELECTION_BITS,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_CD_MSF - Builds a READ_CD_MSF command.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::READ_CD_MSF (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField3Bit 			EXPECTED_SECTOR_TYPE,
						SCSICmdField3Byte 			STARTING_MSF,
						SCSICmdField3Byte 			ENDING_MSF,
						SCSICmdField1Bit 			SYNC,
						SCSICmdField2Bit 			HEADER_CODES,
						SCSICmdField1Bit 			USER_DATA,
						SCSICmdField1Bit 			EDC_ECC,
						SCSICmdField2Bit 			ERROR_FIELD,
						SCSICmdField3Bit 			SUBCHANNEL_SELECTION_BITS,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->READ_CD_MSF (
								scsiRequest,
								dataBuffer,
								EXPECTED_SECTOR_TYPE,
								STARTING_MSF,
								ENDING_MSF,
								SYNC,
								HEADER_CODES,
								USER_DATA,
								EDC_ECC,
								ERROR_FIELD,
								SUBCHANNEL_SELECTION_BITS,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_CAPACITY - Builds a READ_CAPACITY command.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::READ_CAPACITY (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField1Bit 			PMI,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->READ_CAPACITY (
								scsiRequest,
								dataBuffer,
								RELADR,
								LOGICAL_BLOCK_ADDRESS,
								PMI,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_DISC_INFORMATION - Builds a READ_DISC_INFORMATION command.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::READ_DISC_INFORMATION (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->READ_DISC_INFORMATION (
								scsiRequest,
								dataBuffer,
								ALLOCATION_LENGTH,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_DVD_STRUCTURE - Builds a READ_DVD_STRUCTURE command.		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::READ_DVD_STRUCTURE (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField4Byte 			ADDRESS,
						SCSICmdField1Byte 			LAYER_NUMBER,
						SCSICmdField1Byte 			FORMAT,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField2Bit 			AGID,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->READ_DVD_STRUCTURE (
								scsiRequest,
								dataBuffer,
								ADDRESS,
								LAYER_NUMBER,
								FORMAT,
								ALLOCATION_LENGTH,
								AGID,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_FORMAT_CAPACITIES - Builds a READ_FORMAT_CAPACITIES command.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::READ_FORMAT_CAPACITIES (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->READ_FORMAT_CAPACITIES (
								scsiRequest,
								dataBuffer,
								ALLOCATION_LENGTH,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_HEADER - Builds a READ_HEADER command.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::READ_HEADER (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			MSF,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->READ_HEADER (
								scsiRequest,
								dataBuffer,
								MSF,
								LOGICAL_BLOCK_ADDRESS,
								ALLOCATION_LENGTH,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}

#ifdef INCLUDE_OBSOLETE_APIS

//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_MASTER_CUE - Builds a READ_MASTER_CUE command.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::READ_MASTER_CUE (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Byte 			SHEET_NUMBER,
						SCSICmdField3Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->READ_MASTER_CUE (
								scsiRequest,
								dataBuffer,
								SHEET_NUMBER,
								ALLOCATION_LENGTH,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}

#endif	/* INCLUDE_OBSOLETE_APIS */


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_SUB_CHANNEL - Builds a READ_SUB_CHANNEL command.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::READ_SUB_CHANNEL (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			MSF,
						SCSICmdField1Bit 			SUBQ,
						SCSICmdField1Byte 			SUB_CHANNEL_PARAMETER_LIST,
						SCSICmdField1Byte 			TRACK_NUMBER,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->READ_SUB_CHANNEL (
								scsiRequest,
								dataBuffer,
								MSF,
								SUBQ,
								SUB_CHANNEL_PARAMETER_LIST,
								TRACK_NUMBER,
								ALLOCATION_LENGTH,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_TOC_PMA_ATIP - Builds a READ_TOC_PMA_ATIP command.		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::READ_TOC_PMA_ATIP (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			MSF,
						SCSICmdField4Bit 			FORMAT,
						SCSICmdField1Byte			TRACK_SESSION_NUMBER,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->READ_TOC_PMA_ATIP (
								scsiRequest,
								dataBuffer,
								MSF,
								FORMAT,
								TRACK_SESSION_NUMBER,
								ALLOCATION_LENGTH,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ READ_TRACK_INFORMATION - Builds a READ_TRACK_INFORMATION command.
//																	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::READ_TRACK_INFORMATION (
		SCSITaskIdentifier			request,
		IOMemoryDescriptor *		dataBuffer,
		SCSICmdField2Bit 			ADDRESS_NUMBER_TYPE,
		SCSICmdField4Byte			LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER,
		SCSICmdField2Byte 			ALLOCATION_LENGTH,
		SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->READ_TRACK_INFORMATION (
								scsiRequest,
								dataBuffer,
								ADDRESS_NUMBER_TYPE,
								LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER,
								ALLOCATION_LENGTH,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ REPAIR_TRACK - Builds a REPAIR_TRACK command.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::REPAIR_TRACK (
						SCSITaskIdentifier			request,
						SCSICmdField2Byte 			TRACK_NUMBER,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->REPAIR_TRACK (
								scsiRequest,
								TRACK_NUMBER,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ REPORT_KEY - Builds a REPORT_KEY command.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::REPORT_KEY (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField4Byte			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			ALLOCATION_LENGTH,
						SCSICmdField2Bit 			AGID,
						SCSICmdField6Bit 			KEY_FORMAT,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->REPORT_KEY (
								scsiRequest,
								dataBuffer,
								LOGICAL_BLOCK_ADDRESS,
								ALLOCATION_LENGTH,
								AGID,
								KEY_FORMAT,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ RESERVE_TRACK - Builds a RESERVE_TRACK command.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::RESERVE_TRACK (
						SCSITaskIdentifier			request,
						SCSICmdField4Byte			RESERVATION_SIZE,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->RESERVE_TRACK (
								scsiRequest,
								RESERVATION_SIZE,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SCAN - Builds a SCAN command.									[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::SCAN (
					SCSITaskIdentifier			request,
					SCSICmdField1Bit 			DIRECT,
					SCSICmdField1Bit 			RELADR,
					SCSICmdField4Byte			SCAN_STARTING_ADDRESS_FIELD,
					SCSICmdField2Bit 			TYPE,
					SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->SCAN (
								scsiRequest,
								DIRECT,
								RELADR,
								SCAN_STARTING_ADDRESS_FIELD,
								TYPE,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SEND_CUE_SHEET - Builds a SEND_CUE_SHEET command.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::SEND_CUE_SHEET (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField3Byte			CUE_SHEET_SIZE,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->SEND_CUE_SHEET (
								scsiRequest,
								dataBuffer,
								CUE_SHEET_SIZE,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SEND_DVD_STRUCTURE - Builds a SEND_DVD_STRUCTURE command.		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::SEND_DVD_STRUCTURE (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Byte			FORMAT,
						SCSICmdField2Byte 			STRUCTURE_DATA_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->SEND_DVD_STRUCTURE (
								scsiRequest,
								dataBuffer,
								FORMAT,
								STRUCTURE_DATA_LENGTH,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SEND_EVENT - Builds a SEND_EVENT command.						[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::SEND_EVENT (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			IMMED,
						SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->SEND_EVENT (
								scsiRequest,
								dataBuffer,
								IMMED,
								PARAMETER_LIST_LENGTH,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SEND_KEY - Builds a SEND_KEY command.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::SEND_KEY (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
						SCSICmdField2Bit 			AGID,
						SCSICmdField6Bit 			KEY_FORMAT,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->SEND_KEY (
								scsiRequest,
								dataBuffer,
								PARAMETER_LIST_LENGTH,
								AGID,
								KEY_FORMAT,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SEND_OPC_INFORMATION - Builds a SEND_OPC_INFORMATION command.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::SEND_OPC_INFORMATION (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField1Bit 			DO_OPC,
						SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->SEND_OPC_INFORMATION (
								scsiRequest,
								dataBuffer,
								DO_OPC,
								PARAMETER_LIST_LENGTH,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SET_CD_SPEED - Builds a SET_CD_SPEED command.					[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::SET_CD_SPEED (
						SCSITaskIdentifier			request,
						SCSICmdField2Byte 			LOGICAL_UNIT_READ_SPEED,
						SCSICmdField2Byte 			LOGICAL_UNIT_WRITE_SPEED,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->SET_CD_SPEED (
								scsiRequest,
								LOGICAL_UNIT_READ_SPEED,
								LOGICAL_UNIT_WRITE_SPEED,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SET_READ_AHEAD - Builds a SET_READ_AHEAD command.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::SET_READ_AHEAD (
				SCSITaskIdentifier			request,
				SCSICmdField4Byte 			TRIGGER_LOGICAL_BLOCK_ADDRESS,
				SCSICmdField4Byte 			READ_AHEAD_LOGICAL_BLOCK_ADDRESS,
				SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->SET_READ_AHEAD (
								scsiRequest,
								TRIGGER_LOGICAL_BLOCK_ADDRESS,
								READ_AHEAD_LOGICAL_BLOCK_ADDRESS,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SET_STREAMING - Builds a SET_STREAMING command.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::SET_STREAMING (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						SCSICmdField2Byte 			PARAMETER_LIST_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->SET_STREAMING (
								scsiRequest,
								dataBuffer,
								PARAMETER_LIST_LENGTH,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ START_STOP_UNIT - Builds a START_STOP_UNIT command.			[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::START_STOP_UNIT ( 
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			IMMED,
						SCSICmdField4Bit 			POWER_CONDITIONS,
						SCSICmdField1Bit 			LOEJ,
						SCSICmdField1Bit 			START,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIBlockCommandObject ( )->START_STOP_UNIT (
											scsiRequest,
											IMMED,
											POWER_CONDITIONS,
											LOEJ,
											START,
											CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ STOP_PLAY_SCAN - Builds a STOP_PLAY_SCAN command.				[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::STOP_PLAY_SCAN (
						SCSITaskIdentifier			request,
						SCSICmdField1Byte 		CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->STOP_PLAY_SCAN (
								scsiRequest,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ SYNCHRONIZE_CACHE - Builds a SYNCHRONIZE_CACHE command.		[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::SYNCHRONIZE_CACHE (
						SCSITaskIdentifier			request,
						SCSICmdField1Bit 			IMMED,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			NUMBER_OF_BLOCKS,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->SYNCHRONIZE_CACHE (
													scsiRequest,
													IMMED,
													RELADR,
													LOGICAL_BLOCK_ADDRESS,
													NUMBER_OF_BLOCKS,
													CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ WRITE_10 - Builds a WRITE_10 command.							[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::WRITE_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						UInt32						blockSize,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit 			FUA,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField2Byte 			TRANSFER_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->WRITE_10 (
								scsiRequest,
								dataBuffer,
								blockSize,
								DPO,
								FUA,
								RELADR,
								LOGICAL_BLOCK_ADDRESS,
								TRANSFER_LENGTH,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	¥ WRITE_AND_VERIFY_10 - Builds a WRITE_AND_VERIFY_10 command.	[PROTECTED]
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

bool
IOSCSIMultimediaCommandsDevice::WRITE_AND_VERIFY_10 (
						SCSITaskIdentifier			request,
						IOMemoryDescriptor *		dataBuffer,
						UInt32						blockSize,
						SCSICmdField1Bit 			DPO,
						SCSICmdField1Bit 			BYT_CHK,
						SCSICmdField1Bit 			RELADR,
						SCSICmdField4Byte 			LOGICAL_BLOCK_ADDRESS,
						SCSICmdField4Byte 			TRANSFER_LENGTH,
						SCSICmdField1Byte 			CONTROL )
{
	
	SCSITask *	scsiRequest	= NULL;
	bool		status 		= false;
	
	scsiRequest = OSDynamicCast ( SCSITask, request );
	require_nonzero ( scsiRequest, ErrorExit );
	require ( scsiRequest->ResetForNewTask ( ), ErrorExit );
	
	status = GetSCSIMultimediaCommandObject ( )->WRITE_AND_VERIFY_10 (
								scsiRequest,
								dataBuffer,
								blockSize,
								DPO,
								BYT_CHK,
								RELADR,
								LOGICAL_BLOCK_ADDRESS,
								TRANSFER_LENGTH,
								CONTROL );
	
	
ErrorExit:
	
	
	return status;
	
}