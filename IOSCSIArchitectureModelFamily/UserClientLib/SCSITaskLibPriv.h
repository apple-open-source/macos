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

#ifndef __SCSI_TASK_LIB_PRIV_H__
#define __SCSI_TASK_LIB_PRIV_H__

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
	kSCSITaskUserClientIsExclusiveAccessAvailable	= 0,
	kSCSITaskUserClientObtainExclusiveAccess		= 1,	// kIOUCScalarIScalarO, 0, 0
	kSCSITaskUserClientReleaseExclusiveAccess		= 2,	// kIOUCScalarIScalarO, 0, 0
	kSCSITaskUserClientCreateTask					= 3,	// kIOUCScalarIScalarO, 0, 1
	kSCSITaskUserClientReleaseTask					= 4,	// kIOUCScalarIScalar0, 1, 0
	kSCSITaskUserClientAbortTask					= 5,	// kIOUCScalarIScalar0, 1, 0
	kSCSITaskUserClientExecuteTaskAsync				= 6,	// kIOUCScalarIScalar0, 1, 0
	kSCSITaskUserClientExecuteTaskSync				= 7,	// kIOUCScalarIScalar0, 2, 3
	kSCSITaskUserClientIsTaskActive					= 8,	// kIOUCScalarIScalar0, 1, 1
	kSCSITaskUserClientSetTransferDirection			= 9,	// kIOUCScalarIScalar0, 2, 0
	kSCSITaskUserClientSetTaskAttribute				= 10,	// kIOUCScalarIScalar0, 2, 0
	kSCSITaskUserClientGetTaskAttribute				= 11,	// kIOUCScalarIScalar0, 1, 1
	kSCSITaskUserClientSetCommandDescriptorBlock	= 12,	// kIOUCScalarIStructureI, 2, sizeof ( SCSICommandDescriptorBlock )
	kSCSITaskUserClientSetScatterGatherList			= 13,	// kIOUCScalarIStructureI, 5, ( sizeof ( IOVirtualRange ) * 0xFF )
	kSCSITaskUserClientSetSenseDataBuffer			= 14,	// kIOUCScalarIScalarO, 3, 0
	kSCSITaskUserClientSetTimeoutDuration			= 15,	// kIOUCScalarIScalarO, 2, 0
	kSCSITaskUserClientGetTimeoutDuration			= 16,	// kIOUCScalarIScalarO, 1, 1
	kSCSITaskUserClientGetTaskStatus				= 17,	// kIOUCScalarIScalarO, 1, 1
	kSCSITaskUserClientGetSCSIServiceResponse		= 18,	// kIOUCScalarIScalarO, 1, 1
	kSCSITaskUserClientGetTaskState					= 19,	// kIOUCScalarIScalarO, 1, 1
	kSCSITaskUserClientGetAutoSenseData				= 20,	// kIOUCScalarIStructureO, 1, sizeof ( SCSISenseData )
	
	// MMC-2 device
	kMMCDeviceInquiry								= 21,	// kIOUCScalarIStructure0, 0, sizeof ( SCSICmd_INQUIRY_StandardData )
	kMMCDeviceTestUnitReady							= 22,	// kIOUCScalarIStructureO, 0, sizeof ( SCSISenseData )
	kMMCDeviceGetPerformance						= 23,	// kIOUCScalarIScalarO, 5, 1
	kMMCDeviceGetConfiguration						= 24,	// kIOUCScalarIScalarO, 5, 1
	kMMCDeviceModeSense10							= 25,	// kIOUCScalarIScalarO, 4, 0
	kMMCDeviceSetWriteParametersModePage			= 26,	// kIOUCScalarIScalarO, 3, 1
	kMMCDeviceGetTrayState							= 27,	// kIOUCScalarIScalarO, 0, 1
	kMMCDeviceSetTrayState							= 28,	// kIOUCScalarIScalarO, 1, 0
	kMMCDeviceReadTableOfContents					= 29,	// kIOUCScalarIScalarO, 5, 0
	kMMCDeviceReadDiscInformation					= 30,	// kIOUCScalarIScalarO, 2, 0
	kMMCDeviceReadTrackInformation					= 31,	// kIOUCScalarIScalarO, 4, 0
	kMMCDeviceReadDVDStructure						= 32,	// kIOUCScalarIScalarO, 5, 0
	
	kSCSITaskUserClientMethodCount
};

enum
{
	kSCSITaskUserClientSetAsyncCallback			= 0,		// kIOUCScalarIScalarO, 2, 0
	kSCSITaskUserClientAsyncMethodCount
};

#endif /* __SCSI_TASK_LIB_PRIV_H__ */