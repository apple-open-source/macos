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

#ifndef __SCSI_TASK_LIB_PRIV_H__
#define __SCSI_TASK_LIB_PRIV_H__


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Includes
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

// IOSCSIArchitectureModelFamily includes
#include <IOKit/scsi/SCSICommandDefinitions.h>


#ifdef __cplusplus
extern "C" {
#endif


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Constants
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

#define	kSCSITaskUserClientIniterKey	"SCSITaskUserClientIniter"

enum
{
	kIOSCSITaskUserClientAccessBit		= 16,
	kIOSCSITaskUserClientAccessMask		= (1 << kIOSCSITaskUserClientAccessBit)
};

enum
{
	kSCSITaskLibConnection = 12
};

enum
{
	
	kSCSITaskUserClientIsExclusiveAccessAvailable	= 0,	// kIOUCScalarIScalarO, 0, 0
	kSCSITaskUserClientObtainExclusiveAccess		= 1,	// kIOUCScalarIScalarO, 0, 0
	kSCSITaskUserClientReleaseExclusiveAccess		= 2,	// kIOUCScalarIScalarO, 0, 0
	kSCSITaskUserClientCreateTask					= 3,	// kIOUCScalarIScalarO, 0, 1
	kSCSITaskUserClientReleaseTask					= 4,	// kIOUCScalarIScalarO, 1, 0
	kSCSITaskUserClientAbortTask					= 5,	// kIOUCScalarIScalarO, 1, 0
	kSCSITaskUserClientExecuteTask					= 6,	// kIOUCScalarIStructI, 0, 0xFFFFFFFF
	kSCSITaskUserClientSetBuffers					= 7,	// kIOUCScalarIScalarO, 4, 0
	// MMC-2 device
	kMMCDeviceInquiry								= 8,	// kIOUCStructIStructO, sizeof ( AppleInquiryStruct ), sizeof ( SCSITaskStatus )
	kMMCDeviceTestUnitReady							= 9,	// kIOUCScalarIStructO, 1, sizeof ( SCSITaskStatus )
	kMMCDeviceGetPerformance						= 10,	// kIOUCStructIStructO, sizeof ( AppleGetPerformanceStruct ), sizeof ( SCSITaskStatus )
	kMMCDeviceGetConfiguration						= 11,	// kIOUCStructIStructO, sizeof ( AppleGetConfigurationStruct ), sizeof ( SCSITaskStatus )
	kMMCDeviceModeSense10							= 12,	// kIOUCStructIStructO, sizeof ( AppleModeSense10Struct ), sizeof ( SCSITaskStatus )
	kMMCDeviceSetWriteParametersModePage			= 13,	// kIOUCStructIStructO, sizeof ( AppleWriteParametersModePageStruct ), sizeof ( SCSITaskStatus )
	kMMCDeviceGetTrayState							= 14,	// kIOUCScalarIScalarO, 0, 1
	kMMCDeviceSetTrayState							= 15,	// kIOUCScalarIScalarO, 1, 0
	kMMCDeviceReadTableOfContents					= 16,	// kIOUCStructIStructO, sizeof ( AppleReadTableOfContentsStruct ), sizeof ( SCSITaskStatus )
	kMMCDeviceReadDiscInformation					= 17,	// kIOUCStructIStructO, sizeof ( AppleReadDiscInfoStruct ), sizeof ( SCSITaskStatus )
	kMMCDeviceReadTrackInformation					= 18,	// kIOUCStructIStructO, sizeof ( AppleReadTrackInfoStruct ), sizeof ( SCSITaskStatus )
	kMMCDeviceReadDVDStructure						= 19,	// kIOUCStructIStructO, sizeof ( AppleReadDVDStructureStruct ), sizeof ( SCSITaskStatus )
	kMMCDeviceSetCDSpeed							= 20,	// kIOUCStructIStructO, sizeof ( AppleSetCDSpeedStruct ), sizeof ( SCSITaskStatus )
	kMMCDeviceReadFormatCapacities					= 21,	// kIOUCStructIStructO, sizeof ( AppleReadFormatCapacitiesStruct ), sizeof ( SCSITaskStatus )
	
	kSCSITaskUserClientMethodCount
};

enum
{
	kSCSITaskUserClientSetAsyncCallback				= 0,	// kIOUCScalarIScalarO, 2, 0
	kSCSITaskUserClientAsyncMethodCount
};


#pragma mark -
#pragma mark Exclusive Command Structures
#pragma mark -


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Exclusive Command Structures
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

struct SCSITaskData
{
	UInt32							taskReference;
	bool							isSync;
	SCSITaskAttribute				taskAttribute;
	SCSICommandDescriptorBlock		cdbData;
	UInt8							cdbSize;
	UInt64							requestedTransferCount;
	UInt8							transferDirection;
	UInt32							timeoutDuration;
	UInt32							scatterGatherEntries;
	IOVirtualRange					scatterGatherList[1];
};
typedef struct SCSITaskData SCSITaskData;


struct SCSITaskResults
{
	SCSIServiceResponse				serviceResponse;
	SCSITaskStatus					taskStatus;
	UInt64							realizedTransferCount;
};
typedef struct SCSITaskResults SCSITaskResults;


#pragma mark -
#pragma mark Non-Exclusive Command Structures
#pragma mark -


//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
//	Non-Exclusive Command Structures (MMC)
//ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ


struct AppleInquiryStruct
{
	void *				buffer;
	SCSICmdField1Byte 	bufferSize;
	void *				senseDataBuffer;
};
typedef struct AppleInquiryStruct AppleInquiryStruct;


struct AppleGetPerformanceStruct
{
	SCSICmdField5Bit	DATA_TYPE;
	SCSICmdField4Byte 	STARTING_LBA;
	SCSICmdField2Byte	MAXIMUM_NUMBER_OF_DESCRIPTORS;
	SCSICmdField1Byte	TYPE;
	void *				buffer;
	SCSICmdField2Byte	bufferSize;
	void *				senseDataBuffer;
};
typedef struct AppleGetPerformanceStruct AppleGetPerformanceStruct;


struct AppleGetConfigurationStruct
{
	SCSICmdField1Byte 	RT;
	SCSICmdField2Byte 	STARTING_FEATURE_NUMBER;
	void *				buffer;
	SCSICmdField2Byte	bufferSize;
	void *				senseDataBuffer;
};
typedef struct AppleGetConfigurationStruct AppleGetConfigurationStruct;


struct AppleModeSense10Struct
{
	SCSICmdField1Bit	LLBAA;
	SCSICmdField1Bit	DBD;
	SCSICmdField2Bit	PC;
	SCSICmdField6Bit	PAGE_CODE;
	void *				buffer;
	SCSICmdField2Byte	bufferSize;
	void *				senseDataBuffer;
};
typedef struct AppleModeSense10Struct AppleModeSense10Struct;


struct AppleWriteParametersModePageStruct
{
	void *				buffer;
	SCSICmdField2Byte 	bufferSize;
	void *				senseDataBuffer;
};
typedef struct AppleWriteParametersModePageStruct AppleWriteParametersModePageStruct;


struct AppleReadTableOfContentsStruct
{
	SCSICmdField1Bit 	MSF;
	SCSICmdField4Bit 	FORMAT;
	SCSICmdField1Byte	TRACK_SESSION_NUMBER;
	void *				buffer;
	SCSICmdField2Byte	bufferSize;
	void *				senseDataBuffer;
};
typedef struct AppleReadTableOfContentsStruct AppleReadTableOfContentsStruct;


struct AppleReadDiscInfoStruct
{
	void *				buffer;
	SCSICmdField2Byte 	bufferSize;
	void *				senseDataBuffer;
};
typedef struct AppleReadDiscInfoStruct AppleReadDiscInfoStruct;

struct AppleReadTrackInfoStruct
{
	SCSICmdField1Byte	ADDRESS_NUMBER_TYPE;
	SCSICmdField4Byte	LOGICAL_BLOCK_ADDRESS_TRACK_SESSION_NUMBER;
	void *				buffer;
	SCSICmdField2Byte 	bufferSize;
	void *				senseDataBuffer;
};
typedef struct AppleReadTrackInfoStruct AppleReadTrackInfoStruct;

struct AppleReadDVDStructureStruct
{
	SCSICmdField4Byte 			ADDRESS;
	SCSICmdField1Byte 			LAYER_NUMBER;
	SCSICmdField1Byte 			FORMAT;
	void * 						buffer;
	SCSICmdField2Byte 			bufferSize;
	SCSICmdField2Bit 			AGID;
	void *						senseDataBuffer;
};
typedef struct AppleReadDVDStructureStruct AppleReadDVDStructureStruct;

struct AppleSetCDSpeedStruct
{
	SCSICmdField2Byte 			LOGICAL_UNIT_READ_SPEED;
	SCSICmdField2Byte 			LOGICAL_UNIT_WRITE_SPEED;
	void *						senseDataBuffer;
};
typedef struct AppleSetCDSpeedStruct AppleSetCDSpeedStruct;

struct AppleReadFormatCapacitiesStruct
{
	void * 						buffer;
	SCSICmdField2Byte 			bufferSize;
	void *						senseDataBuffer;
};
typedef struct AppleReadFormatCapacitiesStruct AppleReadFormatCapacitiesStruct;


#ifdef __cplusplus
}
#endif


#endif /* __SCSI_TASK_LIB_PRIV_H__ */