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

enum {
	/*
	 * Notify me when disks appear, register with the
	 * DiskArbCallback_DiskAppeared2_t callback
	 */
	kDA_DISK_APPEARED = 1,

	/*
	 * these four callbacks MUST be registered together.  If you register
	 * for one, you must register for all!!
	 */
	/*
	 * Notify me before users can unmount.  I register with the
	 * DiskArbCallback_UnmountPreNotification_t.  I must respond with the
	 * DiskArbUnmountPreNotifyAck_async_auto()
	 */
	kDA_DISK_UNMOUNT_PRE_NOTIFY = 2,
	/*
	 * Notify me after an unmount succeeds or fails.  I register with the
	 * DiskArbCallback_UnmountPostNotification_t.
	 */
	kDA_DISK_UNMOUNT_POST_NOTIFY = 3,
	/*
	 * Notify me before users can eject.  I register with the
	 * DiskArbCallback_EjectPreNotification_t.  I must respond with the
	 * DiskArbEjectPreNotifyAck_async_auto()
	 */
	kDA_DISK_EJECT_PRE_NOTIFY = 4,
	/*
	 * Notify me after an eject succeeds or fails.  I register with the
	 * DiskArbCallback_EjectPostNotification_t.
	 */
	kDA_DISK_EJECT_POST_NOTIFY = 5,

	/*
	 * Notify me when my client has been disconnected by the server.  I
	 * can then reconnect.  Register with
	 * DiskArbCallback_ClientDisconnectedNotification_t()
	 */
	kDA_CLIENT_DISCONNECTED = 6,

	/*
	 * Notify me when a disk has been changed.  (Currently only renamed).
	 * Register with DiskArbCallback_DiskChangedNotification_t
	 */
	kDA_DISK_CHANGED = 7,

	/*
	 * Notify me when some series of actions are finished (a mount or
	 * unmount for example).  This lets me know to continue to processing
	 * for a bit.  Register with DiskArbCallback_NotificationComplete_t
	 */
	kDA_NOTIFICATIONS_COMPLETE = 8,

	/*
	 * Notify me when another application agrees/disagrees to release a
	 * device registration.  Register with
	 * DiskArbCallback_Will_Client_Release_t
	 */
	kDA_WILL_CLIENT_RELEASE_DEVICE = 9,
	/*
	 * Notify me when a device status changes.  Register with
	 * DiskArbCallback_Device_Reservation_Status_t
	 */
	kDA_DEVICE_RESERVATION_STATUS = 10,

	/*
	 * Notify me when an unrecognized disk comes online.  Register with
	 * DiskArbCallback_Will_Client_Handle_Unrecognized_Disk_t.  I must
	 * repsond with DiskArbClientWillHandleUnrecognizedDisk.  before
	 * registering for this callback I need to call
	 * DiskArbClientHandlesUnrecognizedDisks() to register my intended
	 * types and my priority
	 */
	kDA_CLIENT_WILL_HANDLE_UNRECOGNIZED_DISK = 11,

	/*
	 * Notify me when a disk is being checked (fscked).  Register with
	 * DiskArbCallback_DiskWillBeCheckedNotification_t
	 */
	kDA_DISK_WILL_BE_CHECKED = 21,

	/*
	 * Notify me when a previous call failed for some reason.  Register
	 * tith DiskArbCallback_CallFailedNotification_t.  See
	 * DiskArbitrationTypes.h for the fail notification parameters
	 */
	kDA_CALL_FAILED = 22,

	/*
	 * Notify me when a previous call succeeded for some reason.
	 * Register tith DiskArbCallback_CallSucceededNotification_t.  See
	 * DiskArbitrationTypes.h for the notification parameters
	 */
	kDA_CALL_SUCCEEDED = 23,

	/* private or obsolete */
	kDA_DISK_APPEARED1 = 12,
	kDA_DISK_APPEARED_WITH_MT = 13,
	kDA_DISK_APPEARED_WITHOUT_MT = 14,
	kDA_DISK_UNMOUNT = 15,
	kDA_BLUE_BOX_UPDATED = 16,
	kDA_UNKNOWN_DISK_APPEARED = 20,

	/*
	 * Notify me before disks are mounted at all - and I can decide if
	 * the disks are allowed to mount
	 */
	kDA_DISK_APPROVAL_NOTIFY = 24,

	/*
	 * Notify of all of the available information for a disk appearing
	 * (including appear time and such ,,,)
	 */
	kDA_DISK_APPEARED_COMPLETE = 25
};

#if __cplusplus
extern          "C" {
#endif


	/*
	 * Note: The ioContent field can be the empty string, e.g., for AFP
	 * volumes.
	 */
	/*
	 * This callback is invoked regardless of whether or not the disk is
	 * mounted.
	 */
	typedef void    (*DiskArbCallback_DiskAppeared2_t) (DiskArbDiskIdentifier diskIdentifier,
					                     unsigned flags,
			                       DiskArbMountpoint mountpoint,
				                 DiskArbIOContent ioContent,
		                       DiskArbDeviceTreePath deviceTreePath,
				                   unsigned sequenceNumber);


	typedef void    (*DiskArbCallback_DiskAppearedComplete_t) (DiskArbDiskIdentifier diskIdentifier,
					                     unsigned flags,
			                       DiskArbMountpoint mountpoint,
				                 DiskArbIOContent ioContent,
		                       DiskArbDeviceTreePath deviceTreePath,
				                    unsigned sequenceNumber,
					                  double timeAppeared,
				                DiskArbGenericString fsType,
			                       DiskArbGenericString fsName);

	typedef void    (*DiskArbCallback_UnmountPreNotification_t) (DiskArbDiskIdentifier diskIdentifier,
					                    unsigned flags);

	typedef void    (*DiskArbCallback_DiskApprovalNotification_t) (DiskArbDiskIdentifier diskIdentifier,
		                       DiskArbMountpoint volName,
			                     DiskArbGenericString ioContent,
			                DiskArbGenericString deviceTreePath,
					                       unsigned flags,
					                     int isWritable,
					                    int isRemovable,
					                       int isWhole,
                                    DiskArbGenericString fsType);

	/*
	 * An error code of a non-zero value is a failure to unmount.  Pid
	 * represents the pid of the client who refused the unmount.  A pid
	 * of -1 represents the kernel refusing to unmount. An errorCode of
	 * 16 in this case represents the fact that iles are still open on
	 * the device
	 */
	typedef void    (*DiskArbCallback_UnmountPostNotification_t) (DiskArbDiskIdentifier diskIdentifier,
					                      int errorCode,
					                   pid_t dissenter);

	typedef void    (*DiskArbCallback_EjectPreNotification_t) (DiskArbDiskIdentifier diskIdentifier,
					                    unsigned flags);

	/*
	 * An error code of a non-zero value is a failure to eject.  Pid
	 * represents the pid of the client who refused the eject.  A pid of
	 * -1 represents the kernel refusing to eject.
	 */
	typedef void    (*DiskArbCallback_EjectPostNotification_t) (DiskArbDiskIdentifier diskIdentifier,
					                      int errorCode,
					                   pid_t dissenter);

	/* You've been bounced for non responsiveness.  Reconnect */
	typedef void    (*DiskArbCallback_ClientDisconnectedNotification_t) ();

	/*
	-- Disk Changed
	The volume name and mountpoint can differ (esp. in the case of the mountpoint being / and the vol name being 'Mac OS X', etc., or in the case of duplicate vol or directory names)
	*/
	typedef void    (*DiskArbCallback_DiskChangedNotification_t) (DiskArbDiskIdentifier diskIdentifier,
					                char *newMountPoint,
					                char *newVolumeName,
			                      int flags, //currently ignored
					                  int successFlags);

	/*
	 * Could allow a UI application to display a notification that the
	 * disk is being checked, etc.
	 */
	typedef void    (*DiskArbCallback_DiskWillBeCheckedNotification_t) (DiskArbDiskIdentifier diskIdentifier,
			                      int flags, //currently ignored
				                  DiskArbIOContent content);

	/* Currently sent for security violations, etc. */
	typedef void    (*DiskArbCallback_CallFailedNotification_t) (DiskArbDiskIdentifier diskIdentifier,
					                 int failedCallType,
						                 int error);

	/* Currently sent on success for VSDB calls, etc. */
	typedef void    (*DiskArbCallback_CallSucceededNotification_t) (DiskArbDiskIdentifier diskIdentifier,
				                     int succeededCallType);

	/* something is done */
	typedef void    (*DiskArbCallback_NotificationComplete_t) (int messageType);

	/* Will you release you reservation to the application pid sent? */
	typedef void    (*DiskArbCallback_Will_Client_Release_t) (DiskArbDiskIdentifier diskIdentifier, int releaseToClientPid);

	/* returns the status of the reservation of the device */
	typedef void    (*DiskArbCallback_Device_Reservation_Status_t) (DiskArbDiskIdentifier diskIdentifier, int status, int pid);

	/*
	 * You must respond to this callback with a
	 * DiskArbClientWillHandleUnrecognizedDisk() call
	 */
	typedef void    (*DiskArbCallback_Will_Client_Handle_Unrecognized_Disk_t) (DiskArbDiskIdentifier diskIdentifier, int diskType, char *fsType, char *deviceType, int isWritable, int isRemovable, int isWhole);

	/*
	 * Register for a callback.  Most callbacks can be multiplexed,
	 * however, the async unmounts can't be.  I suggest sending overwrite
	 * on them.
	 */
	void            DiskArbAddCallbackHandler(int callbackType, void *callback, int overwrite);
	void            DiskArbRemoveCallbackHandler(int callbackType, void *callback);

	void            DiskArbUpdateClientFlags();
	              //needs to be called after any add callback handler calls, call once after a collection of them

	int             DiskArbIsActive();
	              //returns true if disk arb start has been called, false otherwise

	/* Device reservations */
	                kern_return_t DiskArbIsDeviceReservedForClient(DiskArbDiskIdentifier diskIdentifier);
	/* Get a reservation */
	kern_return_t   DiskArbRetainClientReservationForDevice(DiskArbDiskIdentifier diskIdentifier);
	/* Release a reservation */
	kern_return_t   DiskArbReleaseClientReservationForDevice(DiskArbDiskIdentifier diskIdentifier);
	/* I do or do not give up my reservation to the requesting pid */
	kern_return_t   DiskArbClientRelinquishesReservation(DiskArbDiskIdentifier diskIdentifier, int releaseToClientPid, int status);

	/* Unintialized Disk notifications */
	/*
	 * I want notification of certain types of disks and here is my
	 * priority
	 */
	kern_return_t   DiskArbClientHandlesUnrecognizedDisks(int diskTypes, int priority);
	/* Yes/No about my ability to handle this disk */
	kern_return_t   DiskArbClientWillHandleUnrecognizedDisk(DiskArbDiskIdentifier diskIdentifier, int yesNo);

	/* Disk approval */

	kern_return_t   DiskArbDiskApprovedAck_auto(DiskArbDiskIdentifier diskIdentifier, int status);

#if __cplusplus
}
#endif

#endif
