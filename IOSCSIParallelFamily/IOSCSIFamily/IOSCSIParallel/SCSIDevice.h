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
 *
 * SCSIDevice.h
 *
 */

#ifndef _SCSIDEVICE_H
#define _SCSIDEVICE_H

#include <IOKit/IOTypes.h>

#define kDefaultInquirySize	255

typedef struct SCSITargetLun
{
    UInt8	target;
    UInt8	lun;
    UInt8	reserved[2];
} SCSITargetLun;

typedef struct SCSILunParms
{
    Boolean	disableDisconnect;

    UInt32	reserved[16];
} SCSILunParms;

typedef struct SCSITargetParms
{
    UInt32		transferPeriodpS;
    UInt32		transferOffset;
    UInt32		transferWidth;
    UInt32		transferOptions;

    Boolean		enableTagQueuing;
    Boolean             disableParity;

    UInt32		reserved[16];
} SCSITargetParms;
    
enum SCSITransferOptions
{
    kSCSITransferOptionClockDT 		= 0x00000001,
    kSCSITransferOptionQAS     		= 0x00000100,
    kSCSITransferOptionIUS     		= 0x00000200,
    kSCSITransferOptionPPR     		= 0x00000400
};

#define kSCSITransferOptionsSCSI3	(kSCSITransferOptionClockDT | kSCSITransferOptionQAS | kSCSITransferOptionIUS | kSCSITransferOptionPPR)


enum SCSIDeviceTimeouts
{
    kSCSITimerIntervalmS 	= 500,
    kSCSIProbeTimeoutmS	 	= 5000,
    kSCSIResetIntervalmS 	= 3000,
    kSCSIAbortTimeoutmS  	= 5000,
    kSCSIReqSenseTimeoutmS	= 5000,
    kSCSIDisableTimeoutmS	= 5000
};

enum SCSIClientMessage
{
    kSCSIClientMsgNone			=  0x00005000,
    kSCSIClientMsgDeviceAbort,
    kSCSIClientMsgDeviceReset,
    kSCSIClientMsgBusReset,		

    kSCSIClientMsgDone			= 0x80000000
};

enum SCSIQueueType
{
    kQTypeNormalQ		= 0,
    kQTypeBypassQ		= 1
};

enum SCSIQueuePosition
{
    kQPositionTail		= 0,
    kQPositionHead		= 1
};


#define kSCSIMaxProperties		12

#define kSCSIPropertyTarget		"SCSI Target"		/* OSNumber	*/
#define kSCSIPropertyLun		"SCSI Lun"		/* OSNumber	*/
#define kSCSIPropertyIOUnit		"IOUnit"		/* OSNumber	*/
#define kSCSIPropertyDeviceTypeID	"SCSI Device Type"	/* OSNumber	*/
#define kSCSIPropertyRemovableMedia	"SCSI Removable Media"	/* OSBoolean	*/
#define kSCSIPropertyVendorName		"SCSI Vendor Name"	/* OSString	*/
#define kSCSIPropertyProductName	"SCSI Product Name"	/* OSString	*/
#define kSCSIPropertyProductRevision	"SCSI Product Revision"	/* OSString	*/
#define kSCSIPropertyTransferPeriod	"SCSI Transfer Period"	/* OSNumber	*/
#define kSCSIPropertyTransferOffset	"SCSI Transfer Offset" 	/* OSNumber	*/
#define kSCSIPropertyTransferWidth	"SCSI Transfer Width" 	/* OSNumber	*/
#define kSCSIPropertyTransferOptions	"SCSI Transfer Options" /* OSNumber	*/
#define kSCSIPropertyCmdQueue		"SCSI CmdQueue Enabled" /* OSNumber     */

#endif
