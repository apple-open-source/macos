/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
/*
 *	SCSIParallelController.h
 *
 */
 
#ifndef _SCSIPARALLELCONTROLLER_H
#define _SCSIPARALLELCONTROLLER_H

class IOSyncer;

typedef struct SCSIControllerInfo
{
    UInt32      initiatorId;

    UInt32	maxTargetsSupported;
    UInt32	maxLunsSupported;

    UInt32	minTransferPeriodpS;
    UInt32	maxTransferOffset;
    UInt32	maxTransferWidth; 
 
    UInt32	maxCommandsPerController;
    UInt32	maxCommandsPerTarget;
    UInt32	maxCommandsPerLun;

    UInt32	tagAllocationMethod;
    UInt32	maxTags;

    UInt32	targetPrivateDataSize;
    UInt32	lunPrivateDataSize;
    UInt32	commandPrivateDataSize;

    bool	disableCancelCommands;

    UInt32	reserved[64];

} SCSIControllerInfo;

enum SCSITagAllocation
{
    kTagAllocationNone			= 0,
    kTagAllocationPerLun,
    kTagAllocationPerTarget,    
    kTagAllocationPerController
};

/*
 * Private for IOSCSIClass
 */
enum WorkLoopReqType
{
    kWorkLoopInitTarget		= 1,
    kWorkLoopReleaseTarget,
    kWorkLoopInitDevice,
    kWorkLoopReleaseDevice
};

enum DispatchAction
{
    kDispatchNextCommand	= 1,
    kDispatchNextLun,
    kDispatchNextTarget,
    kDispatchStop
};

typedef struct WorkLoopRequest
{
    WorkLoopReqType     type;
    IOSyncer *		sync;
    bool		rc;
} WorkLoopRequest;

#endif

