/*
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
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
 *
 * SCSIParallelTarget.h
 *
 */

#ifndef _SCSIPARALLELTARGET_H
#define _SCSIPARALLELTARGET_H

#include <IOKit/IOTypes.h>
#include <libkern/c++/OSNumber.h>
#include <IOKit/IOLocks.h>

typedef struct SCSINegotiationResults
{
    IOReturn		returnCode;

    UInt32		transferPeriodpS;
    UInt32		transferOffset;
    UInt32		transferWidth;
    UInt32		transferOptions;
    
} SCSINegotiationResults;


typedef struct SCSITarget
{
    queue_head_t		deviceList;
    
    UInt32			commandCount;
    UInt32			commandLimit;
    UInt32			commandLimitSave;

    IORWLock *			clientSem;
    IORWLock *			targetSem;

    UInt32			*tagArray;
    
    UInt32			negotiateState;
    SCSINegotiationResults	negotiationResult;

    UInt32			state;

    SCSITargetParms		targetParmsCurrent;
    SCSITargetParms		targetParmsNew;

    OSNumber			*regObjTransferPeriod;
    OSNumber			*regObjTransferOffset;
    OSNumber			*regObjTransferWidth;
    OSNumber			*regObjTransferOptions;
    OSNumber			*regObjCmdQueue;

    UInt32			reqSenseCount;
    UInt32			reqSenseState;

    void			*targetPrivateData;

    bool			targetAllocated;

} SCSITarget;    

enum 
{
    kStateIdle,
    kStateIssue,
    kStatePending,
    kStateActive
};

enum _cdbFlagsInternal
{
    kCDBFlagsEnableTagQueuing   = 0x80000000
};


enum SCSICommandType
{
    kSCSICommandNone		= 0,
    kSCSICommandExecute,
    kSCSICommandReqSense,
    kSCSICommandAbort,
    kSCSICommandAbortAll,
    kSCSICommandDeviceReset,
    kSCSICommandBusReset,
    kSCSICommandCancel
};
    

#endif
