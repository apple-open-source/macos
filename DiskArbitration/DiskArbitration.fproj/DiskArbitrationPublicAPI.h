/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * "Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.0 (the 'License').  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License."
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/* DiskArbitration.h */

#ifndef __DISKARBITRATION_PUBLIC_H
#define __DISKARBITRATION_PUBLIC_H

#include <CoreFoundation/CFString.h>
#include <DiskArbitration/DiskArbitrationTypes.h>

#include <sys/types.h>

#include <mach/kern_return.h>

enum
{
       		kDA_DISK_APPEARED 				= 1,
        	kDA_DISK_UNMOUNT_PRE_NOTIFY 			= 2,
                kDA_DISK_UNMOUNT_POST_NOTIFY 			= 3,
                kDA_DISK_EJECT_PRE_NOTIFY 			= 4,
                kDA_DISK_EJECT_POST_NOTIFY 			= 5,
                kDA_CLIENT_DISCONNECTED 			= 6,
                kDA_DISK_CHANGED 				= 7,
                kDA_NOTIFICATIONS_COMPLETE 			= 8,
                kDA_WILL_CLIENT_RELEASE_DEVICE 			= 9,
                kDA_DEVICE_RESERVATION_STATUS 			= 10,
                kDA_CLIENT_WILL_HANDLE_UNRECOGNIZED_DISK 	= 11,
                kDA_DISK_WILL_BE_CHECKED 			= 21,
                kDA_CALL_FAILED 				= 22,

        /* private or obsolete */
                kDA_DISK_APPEARED1 				= 12,
                kDA_DISK_APPEARED_WITH_MT 			= 13,
                kDA_DISK_APPEARED_WITHOUT_MT 			= 14,
                kDA_DISK_UNMOUNT 				= 15,
                kDA_BLUE_BOX_UPDATED 				= 16,
                kDA_PRINTER_FINAL_REQUEST 			= 17,
                kDA_PRINTER_FINAL_RESPONSE 			= 18,
                kDA_PRINTER_FINAL_RELEASE 			= 19,
        	kDA_UNKNOWN_DISK_APPEARED			= 20
};

#if __cplusplus
extern "C" {
#endif


/* Note: The ioContent field can be the empty string, e.g., for AFP volumes. */
/* This callback is invoked regardless of whether or not the disk is mounted. */
typedef void ( * DiskArbCallback_DiskAppeared2_t )( DiskArbDiskIdentifier diskIdentifier,
                                                   unsigned flags,
                                                   DiskArbMountpoint mountpoint,
                                                   DiskArbIOContent ioContent,
                                                   DiskArbDeviceTreePath deviceTreePath,
                                                   unsigned sequenceNumber );

typedef void ( * DiskArbCallback_UnmountPreNotification_t )( DiskArbDiskIdentifier diskIdentifier,
                                        unsigned flags );

typedef void ( * DiskArbCallback_UnmountPostNotification_t )( DiskArbDiskIdentifier diskIdentifier,
                                        int errorCode,
                                        pid_t dissenter );

typedef void ( * DiskArbCallback_EjectPreNotification_t )( DiskArbDiskIdentifier diskIdentifier,
                                        unsigned flags );

typedef void ( * DiskArbCallback_EjectPostNotification_t )( DiskArbDiskIdentifier diskIdentifier,
                                        int errorCode,
                                        pid_t dissenter );

typedef void ( * DiskArbCallback_ClientDisconnectedNotification_t )();

/*
-- Disk Changed
The volume name and mountpoint can differ (esp. in the case of the mountpoint being / and the vol name being 'Mac OS X', etc., or in the case of duplicate vol or directory names)
*/
typedef void ( * DiskArbCallback_DiskChangedNotification_t )( DiskArbDiskIdentifier diskIdentifier,
                        char *newMountPoint,
                        char *newVolumeName,
                        int flags, // currently ignored
                        int successFlags);

typedef void ( * DiskArbCallback_DiskWillBeCheckedNotification_t )( DiskArbDiskIdentifier diskIdentifier,
                        int flags, // currently ignored
                        DiskArbIOContent content);

typedef void ( * DiskArbCallback_CallFailedNotification_t )( DiskArbDiskIdentifier diskIdentifier,
                        int failedCallType, // currently ignored
                        int error);



typedef void ( * DiskArbCallback_NotificationComplete_t )(int messageType);

typedef void ( * DiskArbCallback_Will_Client_Release_t )(DiskArbDiskIdentifier diskIdentifier, int releaseToClientPid);

typedef void ( * DiskArbCallback_Device_Reservation_Status_t )( DiskArbDiskIdentifier diskIdentifier, int status, int pid);

typedef void ( * DiskArbCallback_Will_Client_Handle_Unrecognized_Disk_t )( DiskArbDiskIdentifier diskIdentifier, int diskType,                                             char * fsType,char * deviceType,int isWritable,int isRemovable,int isWhole);

/* Register for a callback.  Most callbacks can be multiplexed, however, the async unmounts can't be.  I suggest sending overwrite on them. */

void DiskArbAddCallbackHandler(int callbackType, void * callback, int overwrite);
void DiskArbRemoveCallbackHandler(int callbackType, void * callback);

void DiskArbUpdateClientFlags();  // needs to be called after any add callback handler calls, call once after a collection of them

int DiskArbIsActive();  // returns true if disk arb start has been called, false otherwise

/* Device reservations */

kern_return_t DiskArbIsDeviceReservedForClient(	 DiskArbDiskIdentifier diskIdentifier);

kern_return_t DiskArbRetainClientReservationForDevice (	DiskArbDiskIdentifier diskIdentifier);

kern_return_t DiskArbReleaseClientReservationForDevice ( DiskArbDiskIdentifier diskIdentifier);

kern_return_t DiskArbClientRelinquishesReservation ( DiskArbDiskIdentifier diskIdentifier, int releaseToClientPid, int status);

/* Unintialized Disk notifications */

kern_return_t DiskArbClientHandlesUnrecognizedDisks ( int diskTypes, int priority);

kern_return_t DiskArbClientWillHandleUnrecognizedDisk ( DiskArbDiskIdentifier diskIdentifier, int yesNo);

#if __cplusplus
}
#endif

#endif
