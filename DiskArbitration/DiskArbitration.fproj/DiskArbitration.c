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

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>

#include <servers/bootstrap.h>
#include <mach/mach.h>
#include <unistd.h>

#include <Security/Authorization.h>

#include <DiskArbitration.h>
#include <CoreFoundation/CoreFoundation.h>

#include "ClientToServer.h"
//#include "ServerToClientServer.h"

/*
-- DWARNING
*/


#ifndef DEBUG
#define DEBUG 0
#endif

#if DEBUG
#define dwarning(args) printf args
#else
#define dwarning(args)
#endif


/*
-- Private Globals
*/


static mach_port_t gDiskArbSndPort = MACH_PORT_NULL;
static mach_port_t gDiskArbRcvPort = MACH_PORT_NULL;

CFMutableDictionaryRef gDiskArbitration_CallbackHandlers = NULL;

int             gDiskArbitration_ClientFlags = kDiskArbNotifyNone;

CFArrayRef
DiskArbCallbackHandlersForCallback(int callbackType)
{
	CFNumberRef     numberForType = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &callbackType);
	CFArrayRef      array = nil;

	if (!gDiskArbitration_CallbackHandlers) {
		CFRelease(numberForType);
		return array;
	}
	if (CFDictionaryContainsKey(gDiskArbitration_CallbackHandlers, numberForType)) {
		array = CFDictionaryGetValue(gDiskArbitration_CallbackHandlers, numberForType);
	}
	CFRelease(numberForType);

	return array;
}

/* flags generation */

boolean_t
updateClientFlags(void)
{
	int             flags = kDiskArbNotifyNone;

	if (DiskArbCallbackHandlersForCallback(kDA_DISK_APPEARED_WITHOUT_MT) != NULL)
		flags |= kDiskArbNotifyDiskAppearedWithoutMountpoint;
	if (DiskArbCallbackHandlersForCallback(kDA_DISK_APPEARED_WITH_MT) != NULL)
		flags |= kDiskArbNotifyDiskAppearedWithMountpoint;
	if (DiskArbCallbackHandlersForCallback(kDA_DISK_APPEARED1) != NULL)
		flags |= kDiskArbNotifyDiskAppeared;
	if (DiskArbCallbackHandlersForCallback(kDA_DISK_APPEARED) != NULL)
		flags |= kDiskArbNotifyDiskAppeared2;
	if (DiskArbCallbackHandlersForCallback(kDA_DISK_APPEARED_COMPLETE) != NULL)
		flags |= kDiskArbNotifyDiskAppearedComplete;
	if (DiskArbCallbackHandlersForCallback(kDA_DISK_UNMOUNT) != NULL)
		flags |= kDiskArbNotifyUnmount;
	if (DiskArbCallbackHandlersForCallback(kDA_DISK_APPROVAL_NOTIFY) != NULL)
		flags |= kDiskArbNotifyDiskApproval;
	if (DiskArbCallbackHandlersForCallback(kDA_DISK_UNMOUNT_PRE_NOTIFY) != NULL || DiskArbCallbackHandlersForCallback(kDA_DISK_UNMOUNT_POST_NOTIFY) != NULL || DiskArbCallbackHandlersForCallback(kDA_DISK_EJECT_PRE_NOTIFY) != NULL || DiskArbCallbackHandlersForCallback(kDA_DISK_EJECT_POST_NOTIFY) != NULL) {
		if (DiskArbCallbackHandlersForCallback(kDA_DISK_UNMOUNT_PRE_NOTIFY) != NULL && DiskArbCallbackHandlersForCallback(kDA_DISK_UNMOUNT_POST_NOTIFY) != NULL && DiskArbCallbackHandlersForCallback(kDA_DISK_EJECT_PRE_NOTIFY) != NULL && DiskArbCallbackHandlersForCallback(kDA_DISK_EJECT_POST_NOTIFY) != NULL) {
			flags |= kDiskArbNotifyAsync;
		} else {
			//printf("DiskArbStart: Disk Arbitration: All Async Clients Must Register For All Async messages!\n");
			return FALSE;
		}
	}
	if (DiskArbCallbackHandlersForCallback(kDA_BLUE_BOX_UPDATED) != NULL)
		flags |= kDiskArbNotifyBlueBoxBootVolumeUpdated;
	if (DiskArbCallbackHandlersForCallback(kDA_NOTIFICATIONS_COMPLETE) != NULL)
		flags |= kDiskArbNotifyCompleted;
	if (DiskArbCallbackHandlersForCallback(kDA_DISK_CHANGED) != NULL)
		flags |= kDiskArbNotifyChangedDisks;
	if (DiskArbCallbackHandlersForCallback(kDA_DISK_WILL_BE_CHECKED) != NULL)
		flags |= kDiskArbNotifyDiskWillBeChecked;
	if (DiskArbCallbackHandlersForCallback(kDA_CALL_FAILED) != NULL)
		flags |= kDiskArbNotifyCallFailed;
	if (DiskArbCallbackHandlersForCallback(kDA_CALL_SUCCEEDED) != NULL)
		flags |= kDiskArbNotifyCallSucceeded;
	if (DiskArbCallbackHandlersForCallback(kDA_CLIENT_WILL_HANDLE_UNRECOGNIZED_DISK) != NULL)
		flags |= kDiskArbArbitrateUnrecognizedVolumes;
	if (DiskArbCallbackHandlersForCallback(kDA_UNKNOWN_DISK_APPEARED) != NULL)
		flags |= kDiskArbNotifyUnrecognizedVolumes;

	if (flags != gDiskArbitration_ClientFlags) {
		gDiskArbitration_ClientFlags = flags;
		return TRUE;
	}
	return FALSE;
}

/*
-- Callback registration
*/


void
DiskArbAddCallbackHandler(int callbackType, void *callback, int overwrite)
{
	CFNumberRef     numberForType = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &callbackType);

	if (!gDiskArbitration_CallbackHandlers) {
		gDiskArbitration_CallbackHandlers = CFDictionaryCreateMutable(NULL, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	}
	if (!callback) {
		CFRelease(numberForType);
		return;
	}
	if (CFDictionaryContainsKey(gDiskArbitration_CallbackHandlers, numberForType)) {

		if (overwrite) {
			CFMutableArrayRef callbackArray = CFArrayCreateMutable(NULL, 0, NULL);
			CFArrayAppendValue(callbackArray, callback);
			CFDictionaryReplaceValue(gDiskArbitration_CallbackHandlers, numberForType, callbackArray);
		} else {
			CFMutableArrayRef callbackArray = CFDictionaryGetValue(gDiskArbitration_CallbackHandlers, numberForType);
			DiskArbRemoveCallbackHandler(callbackType, callback);
			//remove any old callbacks with the exact same signature so we dont end up with duplicates

				CFArrayAppendValue(callbackArray, callback);
		}

	} else {
		CFMutableArrayRef callbackArray = CFArrayCreateMutable(NULL, 0, NULL);
		CFArrayAppendValue(callbackArray, callback);
		CFDictionaryAddValue(gDiskArbitration_CallbackHandlers, numberForType, callbackArray);
	}

	if (updateClientFlags()) {
		/* send an rpc updating the client flags */
	}
	CFRelease(numberForType);

	return;
}

void
DiskArbRemoveCallbackHandler(int callbackType, void *callback)
{
	CFNumberRef     numberForType = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &callbackType);

	if (!gDiskArbitration_CallbackHandlers) {
		CFRelease(numberForType);
		return;
	}
	if (CFDictionaryContainsKey(gDiskArbitration_CallbackHandlers, numberForType)) {
		CFMutableArrayRef callbackArray = CFDictionaryGetValue(gDiskArbitration_CallbackHandlers, numberForType);
		CFIndex         index = CFArrayGetFirstIndexOfValue(callbackArray, CFRangeMake(0, CFArrayGetCount(callbackArray)), callback);
		while (index >= 0) {
			CFArrayRemoveValueAtIndex(callbackArray, index);
			index = CFArrayGetFirstIndexOfValue(callbackArray, CFRangeMake(0, CFArrayGetCount(callbackArray)), callback);
		}
	}
	CFRelease(numberForType);
	return;
}



void
DiskArbRegisterCallback_DiskAppearedWithMountpoint(DiskArbCallback_DiskAppearedWithMountpoint_t callback)
{
	DiskArbAddCallbackHandler(kDA_DISK_APPEARED_WITH_MT, callback, 0);
}

void
DiskArbRegisterCallback_DiskAppearedWithoutMountpoint(DiskArbCallback_DiskAppearedWithoutMountpoint_t callback)
{
	DiskArbAddCallbackHandler(kDA_DISK_APPEARED_WITHOUT_MT, callback, 0);
}

void
DiskArbRegisterCallback_DiskAppeared(DiskArbCallback_DiskAppeared_t callback)
{
	DiskArbAddCallbackHandler(kDA_DISK_APPEARED1, callback, 0);
}

void
DiskArbRegisterCallback_DiskAppeared2(DiskArbCallback_DiskAppeared2_t callback)
{
	DiskArbAddCallbackHandler(kDA_DISK_APPEARED, callback, 0);
}

void
DiskArbRegisterCallback_UnmountNotification(DiskArbCallback_UnmountNotification_t callback)
{
	DiskArbAddCallbackHandler(kDA_DISK_UNMOUNT, callback, 0);
}

void
DiskArbRegisterCallback_UnmountPreNotification(DiskArbCallback_UnmountPreNotification_t callback)
{
	DiskArbAddCallbackHandler(kDA_DISK_UNMOUNT_PRE_NOTIFY, callback, 1);
}

void
DiskArbRegisterCallback_UnmountPostNotification(DiskArbCallback_UnmountPostNotification_t callback)
{
	DiskArbAddCallbackHandler(kDA_DISK_UNMOUNT_POST_NOTIFY, callback, 0);
}

void
DiskArbRegisterCallback_EjectPreNotification(DiskArbCallback_EjectPreNotification_t callback)
{
	DiskArbAddCallbackHandler(kDA_DISK_EJECT_PRE_NOTIFY, callback, 1);
}

void
DiskArbRegisterCallback_EjectPostNotification(DiskArbCallback_EjectPostNotification_t callback)
{
	DiskArbAddCallbackHandler(kDA_DISK_EJECT_POST_NOTIFY, callback, 0);
}

void
DiskArbRegisterCallback_ClientDisconnectedNotification(DiskArbCallback_ClientDisconnectedNotification_t callback)
{
	DiskArbAddCallbackHandler(kDA_CLIENT_DISCONNECTED, callback, 0);
}

void
DiskArbRegisterCallback_BlueBoxBootVolumeUpdated(DiskArbCallback_BlueBoxBootVolumeUpdated_t callback)
{
	DiskArbAddCallbackHandler(kDA_BLUE_BOX_UPDATED, callback, 0);
}

void
DiskArbRegisterCallback_DiskChangedNotification(DiskArbCallback_DiskChangedNotification_t callback)
{
	DiskArbAddCallbackHandler(kDA_DISK_CHANGED, callback, 0);
}

void
DiskArbRegisterCallback_DiskWillBeCheckedNotification(DiskArbCallback_DiskWillBeCheckedNotification_t callback)
{
	DiskArbAddCallbackHandler(kDA_DISK_WILL_BE_CHECKED, callback, 0);
}

void
DiskArbRegisterCallback_CallFailedNotification(DiskArbCallback_CallFailedNotification_t callback)
{
	DiskArbAddCallbackHandler(kDA_CALL_FAILED, callback, 0);
}

void
DiskArbRegisterCallback_CallSucceededNotification(DiskArbCallback_CallSucceededNotification_t callback)
{
	DiskArbAddCallbackHandler(kDA_CALL_SUCCEEDED, callback, 0);
}

void
DiskArbRegisterCallback_NotificationComplete(DiskArbCallback_NotificationComplete_t callback)
{
	DiskArbAddCallbackHandler(kDA_NOTIFICATIONS_COMPLETE, callback, 0);
}

void
DiskArbRegisterCallback_Will_Client_Release(DiskArbCallback_Will_Client_Release_t callback)
{
	DiskArbAddCallbackHandler(kDA_WILL_CLIENT_RELEASE_DEVICE, callback, 0);
}

void
DiskArbRegisterCallback_Device_Reservation_Status(DiskArbCallback_Device_Reservation_Status_t callback)
{
	DiskArbAddCallbackHandler(kDA_DEVICE_RESERVATION_STATUS, callback, 0);
}

void
DiskArbRegisterCallback_UnknownFileSystemNotification(DiskArbCallback_UnknownFileSystemNotification_t callback)
{
	DiskArbAddCallbackHandler(kDA_UNKNOWN_DISK_APPEARED, callback, 0);
}



/*
-- Public Message handler
*/


boolean_t
DiskArbHandleMsg(
		 mach_msg_header_t * InHeadP,
		 mach_msg_header_t * OutHeadP)
{
	return ServerToClient_server(InHeadP, OutHeadP);
}


/*
-- Client -> Server
*/

kern_return_t
DiskArbRegister(mach_port_t server, mach_port_t client, unsigned flags)
{
	gDiskArbRcvPort = client;
	return DiskArbRegister_rpc(server, client, flags);
}
kern_return_t
DiskArbRegister_auto(mach_port_t client, unsigned flags)
{
	return DiskArbRegister(gDiskArbSndPort, client, flags);
}

kern_return_t
DiskArbDeregister(mach_port_t server, mach_port_t client)
{
	return DiskArbDeregister_rpc(server, client);
}
kern_return_t
DiskArbDeregister_auto(mach_port_t client)
{
	return DiskArbDeregister(gDiskArbSndPort, client);
}

kern_return_t
DiskArbDiskAppearedWithMountpointPing(
				      mach_port_t server,
				      DiskArbDiskIdentifier diskIdentifier,
				      unsigned flags,
				      DiskArbMountpoint mountpoint)
{
	SetSecure();
	return DiskArbDiskAppearedWithMountpointPing_rpc(server, diskIdentifier, flags, mountpoint);
}
kern_return_t
DiskArbDiskAppearedWithMountpointPing_auto(DiskArbDiskIdentifier diskIdentifier, unsigned flags, DiskArbMountpoint mountpoint)
{
	return DiskArbDiskAppearedWithMountpointPing(gDiskArbSndPort, diskIdentifier, flags, mountpoint);
}

kern_return_t
DiskArbDiskDisappearedPing(mach_port_t server, DiskArbDiskIdentifier diskIdentifier, unsigned flags)
{
	SetSecure();
	return DiskArbDiskDisappearedPing_rpc(server, diskIdentifier, flags);
}
kern_return_t
DiskArbDiskDisappearedPing_auto(DiskArbDiskIdentifier diskIdentifier, unsigned flags)
{
	return DiskArbDiskDisappearedPing(gDiskArbSndPort, diskIdentifier, flags);
}

kern_return_t
DiskArbRequestMount(mach_port_t server, DiskArbDiskIdentifier diskIdentifier)
{
	SetSecure();
    //printf("Request mount %s\n", diskIdentifier);
	return DiskArbRequestMount_rpc(server, diskIdentifier, FALSE);
}
kern_return_t
DiskArbRequestMount_auto(DiskArbDiskIdentifier diskIdentifier)
{
	return DiskArbRequestMount(gDiskArbSndPort, diskIdentifier);
}

kern_return_t
DiskArbRequestMountAndOwn(mach_port_t server, DiskArbDiskIdentifier diskIdentifier)
{
	SetSecure();
    //printf("Request mount and own%s\n", diskIdentifier);
	return DiskArbRequestMount_rpc(server, diskIdentifier, TRUE);
}
kern_return_t
DiskArbRequestMountAndOwn_auto(DiskArbDiskIdentifier diskIdentifier)
{
	return DiskArbRequestMountAndOwn(gDiskArbSndPort, diskIdentifier);
}

kern_return_t
DiskArbRefresh(
	       mach_port_t server)
{
	return DiskArbRefresh_rpc(server);
}

kern_return_t
DiskArbRegisterWithPID(
		       mach_port_t server,
		       mach_port_t client,
		       unsigned flags)
{
	gDiskArbRcvPort = client;
	return DiskArbRegisterWithPID_rpc(server, client, getpid(), flags);
}

/*
-- Server -> Client
*/


/******************************************* DiskArbDiskAppearedWithoutMountpoint_rpc *******************************************/

kern_return_t
DiskArbDiskAppearedWithoutMountpoint_rpc(
					 mach_port_t server,
				       DiskArbDiskIdentifier diskIdentifier,
					 unsigned flags)
{
	kern_return_t   err = 0;
	CFArrayRef      callbacks = DiskArbCallbackHandlersForCallback(kDA_DISK_APPEARED_WITHOUT_MT);

	if (NULL == callbacks) {
		goto Return;
	} else {
		int             i;
		for (i = 0; i < CFArrayGetCount(callbacks); i++) {
			((DiskArbCallback_DiskAppearedWithoutMountpoint_t) CFArrayGetValueAtIndex(callbacks, i)) (diskIdentifier, flags);
		}
	}

Return:
	return err;

} //DiskArbDiskAppearedWithoutMountpoint_rpc


/******************************************* DiskArbDiskAppearedWithMountpoint_rpc *****************************/

kern_return_t DiskArbDiskAppearedWithMountpoint_rpc(
						    mach_port_t server,
				       DiskArbDiskIdentifier diskIdentifier,
						    unsigned flags,
					       DiskArbMountpoint mountpoint)
{
	kern_return_t   err = 0;
	CFArrayRef      callbacks = DiskArbCallbackHandlersForCallback(kDA_DISK_APPEARED_WITH_MT);

	if (NULL == callbacks) {
		goto Return;
	} else {
		int             i;
		for (i = 0; i < CFArrayGetCount(callbacks); i++) {
			((DiskArbCallback_DiskAppearedWithMountpoint_t) CFArrayGetValueAtIndex(callbacks, i)) (diskIdentifier, flags, mountpoint);
		}
	}

Return:
	return err;

} //DiskArbDiskAppearedWithMountpoint_rpc

/******************************************* DiskArbDiskAppeared_rpc *****************************/

kern_return_t DiskArbDiskAppeared_rpc(
				      mach_port_t server,
				      DiskArbDiskIdentifier diskIdentifier,
				      unsigned flags,
				      DiskArbMountpoint mountpoint,
				      DiskArbIOContent ioContent)
{
	kern_return_t   err = 0;
	CFArrayRef      callbacks = DiskArbCallbackHandlersForCallback(kDA_DISK_APPEARED1);

	if (NULL == callbacks) {
		goto Return;
	} else {
		int             i;
		for (i = 0; i < CFArrayGetCount(callbacks); i++) {
			((DiskArbCallback_DiskAppeared_t) CFArrayGetValueAtIndex(callbacks, i)) (diskIdentifier, flags, mountpoint, ioContent);
		}
	}

Return:
	return err;

} //DiskArbDiskAppeared_rpc

/******************************************* DiskArbDiskAppeared2_rpc *****************************/

kern_return_t DiskArbDiskAppeared2_rpc(
				       mach_port_t server,
				       DiskArbDiskIdentifier diskIdentifier,
				       unsigned flags,
				       DiskArbMountpoint mountpoint,
				       DiskArbIOContent ioContent,
				       DiskArbDeviceTreePath deviceTreePath,
				       unsigned sequenceNumber)
{
	kern_return_t   err = 0;
	CFArrayRef      callbacks = DiskArbCallbackHandlersForCallback(kDA_DISK_APPEARED);

	if (NULL == callbacks) {
		goto Return;
	} else {
		int             i;
		for (i = 0; i < CFArrayGetCount(callbacks); i++) {
			((DiskArbCallback_DiskAppeared2_t) CFArrayGetValueAtIndex(callbacks, i)) (diskIdentifier, flags, mountpoint, ioContent, deviceTreePath, sequenceNumber);
		}
	}

Return:
	return err;

} //DiskArbDiskAppeared2_rpc

/******************************************* DiskArbDiskAppearedComplete_rpc *****************************/

kern_return_t DiskArbDiskAppearedComplete_rpc(
					      mach_port_t server,
				       DiskArbDiskIdentifier diskIdentifier,
					      unsigned flags,
					      DiskArbMountpoint mountpoint,
					      DiskArbIOContent ioContent,
				       DiskArbDeviceTreePath deviceTreePath,
					      unsigned sequenceNumber,
					      double timeAppeared,
					      DiskArbGenericString fsType,
					      DiskArbGenericString fsName)
{
	kern_return_t   err = 0;
	CFArrayRef      callbacks = DiskArbCallbackHandlersForCallback(kDA_DISK_APPEARED_COMPLETE);

	if (NULL == callbacks) {
		goto Return;
	} else {
		int             i;
		for (i = 0; i < CFArrayGetCount(callbacks); i++) {
			((DiskArbCallback_DiskAppearedComplete_t) CFArrayGetValueAtIndex(callbacks, i)) (diskIdentifier, flags, mountpoint, ioContent, deviceTreePath, sequenceNumber, timeAppeared, fsType, fsName);
		}
	}

Return:
	return err;

} //DiskArbDiskAppearedComplete_rpc

/******************************************* DiskArbUnmountNotification_rpc *****************************/

kern_return_t DiskArbUnmountNotification_rpc(
					     mach_port_t server,
				       DiskArbDiskIdentifier diskIdentifier,
					     int pastOrFuture,
					     int willEject)
{
	kern_return_t   err = 0;
	CFArrayRef      callbacks = DiskArbCallbackHandlersForCallback(kDA_DISK_UNMOUNT);

	if (NULL == callbacks) {
		goto Return;
	} else {
		int             i;
		for (i = 0; i < CFArrayGetCount(callbacks); i++) {
			((DiskArbCallback_UnmountNotification_t) CFArrayGetValueAtIndex(callbacks, i)) (diskIdentifier, pastOrFuture, willEject);
		}
	}

Return:
	return err;

} //DiskArbUnmountNotification_rpc


/*
-- Server -> Client (Async)
*/


/******************************************* DiskArbUnmountPreNotify_async_rpc *****************************/

kern_return_t DiskArbUnmountPreNotify_async_rpc(
						mach_port_t server,
				       DiskArbDiskIdentifier diskIdentifier,
						unsigned flags)
{
	kern_return_t   err = 0;
	CFArrayRef      callbacks = DiskArbCallbackHandlersForCallback(kDA_DISK_UNMOUNT_PRE_NOTIFY);

	if (NULL == callbacks) {
		goto Return;
	} else {
		int             i;
		for (i = 0; i < CFArrayGetCount(callbacks); i++) {
			((DiskArbCallback_UnmountPreNotification_t) CFArrayGetValueAtIndex(callbacks, i)) (diskIdentifier, flags);
		}
	}
Return:
	return err;

} //DiskArbUnmountPreNotify_async_rpc

/******************************************* DiskArbUnmountPostNotify_async_rpc *****************************/

kern_return_t DiskArbUnmountPostNotify_async_rpc(
						 mach_port_t server,
				       DiskArbDiskIdentifier diskIdentifier,
						 int errorCode,
						 int dissenter)
{
	kern_return_t   err = 0;
	CFArrayRef      callbacks = DiskArbCallbackHandlersForCallback(kDA_DISK_UNMOUNT_POST_NOTIFY);

        if (NULL == callbacks) {
		goto Return;
	} else {
		int             i;
		for (i = 0; i < CFArrayGetCount(callbacks); i++) {
			((DiskArbCallback_UnmountPostNotification_t) CFArrayGetValueAtIndex(callbacks, i)) (diskIdentifier, errorCode, dissenter);
		}
	}
Return:
	return err;

} //DiskArbUnmountPostNotify_async_rpc

/******************************************* DiskArbEjectPreNotify_async_rpc *****************************/

kern_return_t DiskArbEjectPreNotify_async_rpc(
					      mach_port_t server,
				       DiskArbDiskIdentifier diskIdentifier,
					      unsigned flags)
{
	kern_return_t   err = 0;
	CFArrayRef      callbacks = DiskArbCallbackHandlersForCallback(kDA_DISK_EJECT_PRE_NOTIFY);

        if (NULL == callbacks) {
		goto Return;
	} else {
		int             i;
		for (i = 0; i < CFArrayGetCount(callbacks); i++) {
			((DiskArbCallback_EjectPreNotification_t) CFArrayGetValueAtIndex(callbacks, i)) (diskIdentifier, flags);
		}
	}
Return:
	return err;

} //DiskArbEjectPreNotify_async_rpc

/******************************************* DiskArbEjectPostNotify_async_rpc *****************************/

kern_return_t DiskArbEjectPostNotify_async_rpc(
					       mach_port_t server,
				       DiskArbDiskIdentifier diskIdentifier,
					       int errorCode,
					       int dissenter)
{
	kern_return_t   err = 0;
	CFArrayRef      callbacks = DiskArbCallbackHandlersForCallback(kDA_DISK_EJECT_POST_NOTIFY);

        if (NULL == callbacks) {
		goto Return;
	} else {
		int             i;
		for (i = 0; i < CFArrayGetCount(callbacks); i++) {
			((DiskArbCallback_EjectPostNotification_t) CFArrayGetValueAtIndex(callbacks, i)) (diskIdentifier, errorCode, dissenter);
		}
	}
Return:
	return err;

} //DiskArbEjectPostNotify_async_rpc

/******************************************* DiskArbDiskApprovalPre_rpc *****************************/

kern_return_t DiskArbDiskApprovalPre_rpc(mach_port_t server,
				       DiskArbDiskIdentifier diskIdentifier,
				       DiskArbMountpoint volName,
					 DiskArbGenericString ioContent,
					 DiskArbGenericString deviceTreePath,
					 unsigned flags,
					 int isWritable,
					 int isRemovable,
					 int isWhole,
                     DiskArbGenericString fsType)
{
	kern_return_t   err = 0;
	CFArrayRef      callbacks = DiskArbCallbackHandlersForCallback(kDA_DISK_APPROVAL_NOTIFY);

	if (NULL == callbacks) {
		goto Return;
	} else {
		int             i;
		for (i = 0; i < CFArrayGetCount(callbacks); i++) {
			((DiskArbCallback_DiskApprovalNotification_t) CFArrayGetValueAtIndex(callbacks, i)) (diskIdentifier, volName, ioContent, deviceTreePath, flags, isWritable, isRemovable, isWhole, fsType);
		}
	}
Return:
	return err;

} //DiskArbDiskApprovalPre_rpc


/******************************************* DiskArbClientDisconnected_rpc *****************************/
kern_return_t DiskArbClientDisconnected_rpc(
					    mach_port_t server)
{
	kern_return_t   err = 0;
	CFArrayRef      callbacks = DiskArbCallbackHandlersForCallback(kDA_CLIENT_DISCONNECTED);

	if (NULL == callbacks) {
		goto Return;
	} else {
		int             i;
		for (i = 0; i < CFArrayGetCount(callbacks); i++) {
			((DiskArbCallback_ClientDisconnectedNotification_t) CFArrayGetValueAtIndex(callbacks, i)) ();
		}
	}
Return:
	return err;
} //DiskArbClientDisconnected_rpc



/******************************************* DiskArbBlueBoxBootVolumeUpdated_async_rpc *****************************/

kern_return_t DiskArbBlueBoxBootVolumeUpdated_async_rpc(
							mach_port_t server,
							int seqno)
{
	kern_return_t   err = 0;
	CFArrayRef      callbacks = DiskArbCallbackHandlersForCallback(kDA_BLUE_BOX_UPDATED);

	if (NULL == callbacks) {
		goto Return;
	} else {
		int             i;
		for (i = 0; i < CFArrayGetCount(callbacks); i++) {
			((DiskArbCallback_BlueBoxBootVolumeUpdated_t) CFArrayGetValueAtIndex(callbacks, i)) (seqno);
		}
	}
Return:
	return err;

} //DiskArbBlueBoxBootVolumeUpdated_async_rpc

/******************************************* DiskArbDiskChanged_rpc *****************************/


kern_return_t DiskArbDiskChanged_rpc(
				     mach_port_t server,
				     DiskArbDiskIdentifier diskIdentifier,
				     DiskArbMountpoint newMountPoint,
				     DiskArbMountpoint newVolumeName,
				     unsigned flags,
				     int success)
{
	kern_return_t   err = 0;
	CFArrayRef      callbacks = DiskArbCallbackHandlersForCallback(kDA_DISK_CHANGED);

	if (NULL == callbacks) {
		goto Return;
	} else {
		int             i;
		for (i = 0; i < CFArrayGetCount(callbacks); i++) {
			((DiskArbCallback_DiskChangedNotification_t) CFArrayGetValueAtIndex(callbacks, i)) (diskIdentifier, newMountPoint, newVolumeName, flags, success);
		}
	}
Return:
	return err;
} //DiskArbDiskChanged_rpc

/******************************************* DiskArbDiskWillBeChecked_rpc *****************************/


kern_return_t DiskArbDiskWillBeChecked_rpc(
					   mach_port_t server,
				       DiskArbDiskIdentifier diskIdentifier,
					   unsigned flags,
					   DiskArbIOContent content)
{
	kern_return_t   err = 0;
	CFArrayRef      callbacks = DiskArbCallbackHandlersForCallback(kDA_DISK_WILL_BE_CHECKED);

	if (NULL == callbacks) {
		goto Return;
	} else {
		int             i;
		for (i = 0; i < CFArrayGetCount(callbacks); i++) {
			((DiskArbCallback_DiskWillBeCheckedNotification_t) CFArrayGetValueAtIndex(callbacks, i)) (diskIdentifier, flags, content);
		}
	}
Return:
	return err;
} //DiskArbDiskWillBeChecked_rpc

/******************************************* DiskArbPreviousCallFailed_rpc *****************************/


kern_return_t DiskArbPreviousCallFailed_rpc(
					    mach_port_t server,
				       DiskArbDiskIdentifier diskIdentifier,
					    int failedCallType,
					    int error)
{
	kern_return_t   err = 0;
	CFArrayRef      callbacks = DiskArbCallbackHandlersForCallback(kDA_CALL_FAILED);

	if (NULL == callbacks) {
		goto Return;
	} else {
		int             i;
		for (i = 0; i < CFArrayGetCount(callbacks); i++) {
			((DiskArbCallback_CallFailedNotification_t) CFArrayGetValueAtIndex(callbacks, i)) (diskIdentifier, failedCallType, error);
		}
	}
Return:
	return err;
} //DiskArbPreviousCallFailed_rpc

/******************************************* DiskArbPreviousCallFailed_rpc *****************************/


kern_return_t DiskArbPreviousCallSucceeded_rpc(
					       mach_port_t server,
				       DiskArbDiskIdentifier diskIdentifier,
					       int succeededCallType)
{
	kern_return_t   err = 0;
	CFArrayRef      callbacks = DiskArbCallbackHandlersForCallback(kDA_CALL_SUCCEEDED);

	if (NULL == callbacks) {
		goto Return;
	} else {
		int             i;
		for (i = 0; i < CFArrayGetCount(callbacks); i++) {
			((DiskArbCallback_CallSucceededNotification_t) CFArrayGetValueAtIndex(callbacks, i)) (diskIdentifier, succeededCallType);
		}
	}
Return:
	return err;
} //DiskArbPreviousCallSucceeded_rpc


/******************************************* DiskArbNotificationComplete_rpc *****************************/


kern_return_t DiskArbNotificationComplete_rpc(
					      mach_port_t server,
					      int messageType)
{
	kern_return_t   err = 0;
	CFArrayRef      callbacks = DiskArbCallbackHandlersForCallback(kDA_NOTIFICATIONS_COMPLETE);

	if (NULL == callbacks) {
		goto Return;
	} else {
		int             i;
		for (i = 0; i < CFArrayGetCount(callbacks); i++) {
			((DiskArbCallback_NotificationComplete_t) CFArrayGetValueAtIndex(callbacks, i)) (messageType);
		}
	}

Return:
	return err;
} //DiskArbNotificationComplete_rpc

kern_return_t DiskArbRegistrationComplete_rpc(
					      mach_port_t server)
{
	kern_return_t   err = 0;
#warning NOT CURRENTLY DOING A THING!!!!

	dwarning(("%s\n", __FUNCTION__));
	return err;
} //DiskArbRegistrationComplete_rpc

kern_return_t DiskArbWillClientRelinquish_rpc(
					      mach_port_t server,
				       DiskArbDiskIdentifier diskIdentifier,
					      int releaseToClientPid)
{
	kern_return_t   err = 0;
	CFArrayRef      callbacks = DiskArbCallbackHandlersForCallback(kDA_WILL_CLIENT_RELEASE_DEVICE);

	////printf("Will I %d release %s to %d?\n", getpid(), diskIdentifier, releaseToClientPid);

	if (NULL == callbacks) {
		////printf("Nope - I won't give it up\n");
		DiskArbClientRelinquishesReservation(diskIdentifier, releaseToClientPid, 0);
		//nope - they arent responding, they still want it.
			goto Return;
	} else {
		int             i;
		for (i = 0; i < CFArrayGetCount(callbacks); i++) {
			((DiskArbCallback_Will_Client_Release_t) CFArrayGetValueAtIndex(callbacks, i)) (diskIdentifier, releaseToClientPid);
		}
	}
Return:
	return err;
}

kern_return_t
DiskArbDeviceReservationStatus_rpc(
				   mach_port_t server,
				   DiskArbDiskIdentifier diskIdentifier,
				   int status,
				   int pid)
{
	kern_return_t   err = 0;
	CFArrayRef      callbacks = DiskArbCallbackHandlersForCallback(kDA_DEVICE_RESERVATION_STATUS);
	//printf("%d, Status on %s is %d by pid %d\n", getpid(), diskIdentifier, status, pid);

	if (NULL == callbacks) {
		goto Return;
	} else {
		int             i;
		for (i = 0; i < CFArrayGetCount(callbacks); i++) {
			((DiskArbCallback_Device_Reservation_Status_t) CFArrayGetValueAtIndex(callbacks, i)) (diskIdentifier, status, pid);
		}
	}
Return:
	return err;
}

kern_return_t
DiskArbWillClientHandleUnrecognizedDisk_rpc(
					    mach_port_t server,
				       DiskArbDiskIdentifier diskIdentifier,
					    int diskType,
					    char *fsType,
					    char *deviceType,
					    int isWritable,
					    int isRemovable,
					    int isWhole)
{
	kern_return_t   err = 0;
	CFArrayRef      callbacks = DiskArbCallbackHandlersForCallback(kDA_CLIENT_WILL_HANDLE_UNRECOGNIZED_DISK);

	if (NULL == callbacks) {
		goto Return;
	} else {
		int             i;
		for (i = 0; i < CFArrayGetCount(callbacks); i++) {
			((DiskArbCallback_Will_Client_Handle_Unrecognized_Disk_t) CFArrayGetValueAtIndex(callbacks, i)) (diskIdentifier, diskType, fsType, deviceType, isWritable, isRemovable, isWhole);
		}
	}
Return:
	return err;
}

/******************************************* DiskArbUnknownFileSystemInserted_rpc *****************************/

kern_return_t
DiskArbUnknownFileSystemInserted_rpc(
				     mach_port_t server,
				     char *diskIdentifier,
				     char *fsType,
				     char *deviceType,
				     int isWritable,
				     int isRemovable,
				     int isWhole)
{
	kern_return_t   err = 0;
	CFArrayRef      callbacks = DiskArbCallbackHandlersForCallback(kDA_UNKNOWN_DISK_APPEARED);

	if (NULL == callbacks) {
		goto Return;
	} else {
		int             i;
		for (i = 0; i < CFArrayGetCount(callbacks); i++) {
			((DiskArbCallback_UnknownFileSystemNotification_t) CFArrayGetValueAtIndex(callbacks, i)) (diskIdentifier, fsType, deviceType, isWritable, isRemovable, isWhole);
		}
	}

Return:
	return err;
} //DiskArbUnknownFileSystemInserted_rpc



/*
-- Higher-level routines
*/


/*
-- DiskArbStart()
-- Before calling this, register any desired callbacks.
-- Output is a receive right for a port.  A send right for that port has been passed via
-- a message to the server.  Messages from the server to this client will be sent on that
-- port and should be handled via the public routine DiskArbHandleMsg().
-- Also returns an error code.
*/

kern_return_t DiskArbStart(mach_port_t * portPtr)
{
	kern_return_t   result;
	static mach_port_t pre_registered_port = 0x0;

	//Obtain a send right for the DiskArbitration servers public port via the bootstrap server

	result = bootstrap_look_up(bootstrap_port, DISKARB_SERVER_NAME, &gDiskArbSndPort);
	if (result) {
		dwarning(("%s(): {%s:%d} bootstrap_look_up() failed: $%x\n", __FUNCTION__, __FILE__, __LINE__, (int) result));
		goto Return;
	}
	dwarning(("%s(): gDiskArbSndPort = %d\n", __FUNCTION__, (int) gDiskArbSndPort));

	//Allocate gDiskArbRcvPort with a receive right.DiskArbRegister() will create a send right.
	if              (!pre_registered_port) {
		result = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, portPtr);
		if (result != KERN_SUCCESS) {
			dwarning(("%s(): (%s:%d) mach_port_allocate failed: $%x: %s\n", __FUNCTION__, __FILE__, __LINE__, result, mach_error_string(result)));
			goto Return;
		}
	} else {
		*portPtr = pre_registered_port;
	}

	dwarning(("%s(): port = $%x\n", __FUNCTION__, (int) *portPtr));


	//Create the flags word based on the registered callbacks.
		// its a global
    updateClientFlags();

	dwarning(("%s(): flags = %08x\n", __FUNCTION__, gDiskArbitration_ClientFlags));

	//Register with the DiskArbitration server

		if (!pre_registered_port) {
		result = DiskArbRegisterWithPID_auto(*portPtr, gDiskArbitration_ClientFlags);
		DiskArbMsgLoopWithTimeout(5000);
		if (result) {
			dwarning(("%s(): {%s:%d} DiskArbRegister(sendPort=$%08x, *portPtr=$%08x, flags=$%08x) failed: $%x\n", __FUNCTION__, __FILE__, __LINE__, gDiskArbSndPort, *portPtr, gDiskArbitration_ClientFlags, (int) result));
			goto Return;
		}
	}
	pre_registered_port = *portPtr;

Return:

	return result;

} //DiskArbStart


/*
-- DiskArbInit()
-- Clients that don't register any callbacks should use DiskArbInit() instead of DiskArbStart().
-- Returns an error code.
*/

kern_return_t DiskArbInit(void)
{
	kern_return_t   result;

	if (gDiskArbSndPort != MACH_PORT_NULL) {
		result = 0;
		goto Return;
	}
	//Obtain a send right for the DiskArbitration servers public port via the bootstrap server

	result = bootstrap_look_up(bootstrap_port, DISKARB_SERVER_NAME, &gDiskArbSndPort);
	if (result) {
		dwarning(("%s(): {%s:%d} bootstrap_look_up() failed: $%x\n", __FUNCTION__, __FILE__, __LINE__, (int) result));
		goto Return;
	}
	dwarning(("%s(): gDiskArbSndPort = %d\n", __FUNCTION__, (int) gDiskArbSndPort));

Return:

	return result;

} //DiskArbInit

kern_return_t DiskArbRefresh_auto()
{
	return DiskArbRefresh_rpc(gDiskArbSndPort);
}

kern_return_t
DiskArbRegisterWithPID_auto(
			    mach_port_t client,
			    unsigned flags)
{
	gDiskArbRcvPort = client;
	return DiskArbRegisterWithPID_rpc(gDiskArbSndPort, client, getpid(), flags);
}

kern_return_t
DiskArbUpdateClientWithPID_auto(unsigned flags)
{
	return DiskArbUpdateClientWithPID_rpc(gDiskArbSndPort, getpid(), flags);
}

kern_return_t
DiskArbDeregisterWithPID_auto(mach_port_t client)
{
	gDiskArbRcvPort = client;
	return DiskArbDeregisterWithPID_rpc(gDiskArbSndPort, client, getpid());
}

kern_return_t
DiskArbMarkPIDNew_auto(mach_port_t client, unsigned flags)
{
	gDiskArbRcvPort = client;
	SetSecure();

	return DiskArbMarkPIDNew_rpc(gDiskArbSndPort, client, getpid(), flags);
}

kern_return_t
DiskArbRequestDiskChange_auto(DiskArbDiskIdentifier diskIdentifier, DiskArbGenericString mountPoint, int flags)
{
	SetSecure();
    //printf("Request disk change %s\n", diskIdentifier);
	return DiskArbRequestDiskChange_rpc(gDiskArbSndPort, getpid(), diskIdentifier, mountPoint, flags);
}

kern_return_t
DiskArbSetCurrentUser_auto(int user)
{
	SetSecure();
	return DiskArbSetCurrentUser_rpc(gDiskArbSndPort, getpid(), user);
}

kern_return_t
DiskArbVSDBAdoptVolume_auto(DiskArbDiskIdentifier diskIdentifier)
{
	SetSecure();
	return DiskArbVSDBAdoptVolume_rpc(gDiskArbSndPort, getpid(), diskIdentifier);
}

kern_return_t
DiskArbVSDBDisownVolume_auto(DiskArbDiskIdentifier diskIdentifier)
{
	SetSecure();
	return DiskArbVSDBDisownVolume_rpc(gDiskArbSndPort, getpid(), diskIdentifier);
}

int
DiskArbVSDBGetVolumeStatus_auto(DiskArbDiskIdentifier diskIdentifier)
{
	int             status;
	SetSecure();

	DiskArbVSDBGetVolumeStatus_rpc(gDiskArbSndPort, diskIdentifier, &status);

	return status;
}

kern_return_t
DiskArbSetVolumeEncoding_auto(DiskArbDiskIdentifier diskIdentifier, int volumeEncoding)
{
	SetSecure();
	return DiskArbSetVolumeEncoding_rpc(gDiskArbSndPort, getpid(), diskIdentifier, volumeEncoding);
}

int
DiskArbGetVolumeEncoding_auto(DiskArbDiskIdentifier diskIdentifier)
{
	int             status;

	SetSecure();
	DiskArbGetVolumeEncoding_rpc(gDiskArbSndPort, diskIdentifier, &status);
        
	return status;
}

/*
 * Sets the kDiskArbHandlesUninitializedDisks flag on the corresponding
 * client record.
 */

kern_return_t
DiskArbClientHandlesUninitializedDisks_auto(int flag)
{
	SetSecure();
	return DiskArbClientHandlesUninitializedDisks_rpc(gDiskArbSndPort, getpid(), flag);
}

/*
-- Async
*/

kern_return_t
DiskArbUnmountRequest_async_auto(
				 DiskArbDiskIdentifier diskIdentifier,
				 unsigned flags)
{
	SetSecure();
    //printf("Request unmount %s\n", diskIdentifier);
	return DiskArbUnmountRequest_async_rpc(gDiskArbSndPort, getpid(), diskIdentifier, flags);
}

kern_return_t
DiskArbUnmountPreNotifyAck_async_auto(
				      DiskArbDiskIdentifier diskIdentifier,
				      int errorCode)
{
    if (errorCode & kDiskArbRequireAuthentication) {
        SetSecure();        
    }
	return DiskArbUnmountPreNotifyAck_async_rpc(gDiskArbSndPort, getpid(), diskIdentifier, errorCode);
}

kern_return_t
DiskArbDiskApprovedAck_auto(
			    DiskArbDiskIdentifier diskIdentifier,
			    int status)
{
	SetSecure();
	return DiskArbDiskApprovedAck_rpc(gDiskArbSndPort, diskIdentifier, getpid(), status);
}

kern_return_t
DiskArbEjectRequest_async_auto(
			       DiskArbDiskIdentifier diskIdentifier,
			       unsigned flags)
{
	SetSecure();
    //printf("Request eject %s\n", diskIdentifier);
	return DiskArbEjectRequest_async_rpc(gDiskArbSndPort, getpid(), diskIdentifier, flags);
}

kern_return_t
DiskArbEjectPreNotifyAck_async_auto(
				    DiskArbDiskIdentifier diskIdentifier,
				    int errorCode)
{
	return DiskArbEjectPreNotifyAck_async_rpc(gDiskArbSndPort, getpid(), diskIdentifier, errorCode);
}

kern_return_t
DiskArbUnmountAndEjectRequest_async_auto(
				     DiskArbDiskIdentifier diskIdentifier,
					 unsigned flags)
{
	SetSecure();
    //printf("Request unmount and eject %s\n", diskIdentifier);
	return DiskArbUnmountAndEjectRequest_async_rpc(gDiskArbSndPort, getpid(), diskIdentifier, flags);
}

/* Sets the kDiskArbIAmBlueBox flag on the corresponding client record. */

kern_return_t
DiskArbSetBlueBoxBootVolume_async_auto(
				       int pid,
				       int seqno)
{
	SetSecure();
	return DiskArbSetBlueBoxBootVolume_async_rpc(gDiskArbSndPort, pid, seqno);
}

void
DiskArbNoOp(void)
{
	return;
}

kern_return_t
DiskArbMsgLoop(void)
{
	return DiskArbMsgLoopWithTimeout(MACH_MSG_TIMEOUT_NONE);
}

kern_return_t
DiskArbMsgLoopWithTimeout(mach_msg_timeout_t millisecondTimeout)
{
	kern_return_t   err = 0;

	unsigned        msgReceiveBufLength;
	mach_msg_header_t *msgReceiveBufPtr = NULL;

	unsigned        msgSendBufLength;
	mach_msg_header_t *msgSendBufPtr = NULL;

	mach_msg_return_t receiveResult;

	msgSendBufLength = sizeof(mach_msg_empty_send_t) + 20;	/* Over-allocate */
	msgSendBufPtr = (mach_msg_header_t *) malloc(msgSendBufLength);
	if (msgSendBufPtr == NULL) {
		dwarning(("FAILURE: msgSendBufPtr = malloc(%d)\n", msgSendBufLength));
		err = -1;
		goto Return;
	} else {
		dwarning(("SUCCESS: msgSendBufPtr = malloc(%d)\n", msgSendBufLength));
	}

	msgReceiveBufLength = sizeof(mach_msg_empty_rcv_t);
	msgReceiveBufPtr = NULL;

	while (1) {
		/* (Re)allocate a buffer for receiving msgs from the server. */

		if (msgReceiveBufPtr == NULL) {
			msgReceiveBufPtr = (mach_msg_header_t *) malloc(msgReceiveBufLength);
			if (msgReceiveBufPtr == NULL) {
				dwarning(("FAILURE: msgReceiveBufPtr = malloc(%d)\n", msgReceiveBufLength));
				err = -2;
				goto Return;
			} else {
				dwarning(("SUCCESS: msgReceiveBufPtr = malloc(%d)\n", msgReceiveBufLength));
			}
		}
		dwarning(("gDiskArbRcvPort = $%x", gDiskArbRcvPort));
		dwarning(("%s: Waiting for a message (millisecondTimeount = %d)...\n", __FUNCTION__, millisecondTimeout));

		receiveResult = mach_msg(msgReceiveBufPtr,
					 MACH_RCV_MSG | MACH_RCV_LARGE | (MACH_MSG_TIMEOUT_NONE == millisecondTimeout ? 0 : MACH_RCV_TIMEOUT),
					 0,
					 msgReceiveBufLength,
					 gDiskArbRcvPort,
					 millisecondTimeout,
					 MACH_PORT_NULL);

		if (receiveResult == MACH_RCV_TOO_LARGE) {
			dwarning(("%s(): mach_msg: $%08x: %s\n", __FUNCTION__, receiveResult, mach_error_string(receiveResult)));
			dwarning(("msgReceiveBufPtr->msgh_size = %d\n", msgReceiveBufPtr->msgh_size));
			msgReceiveBufLength = msgReceiveBufPtr->msgh_size + sizeof(mach_msg_trailer_t);
			free(msgReceiveBufPtr);
			msgReceiveBufPtr = NULL;
			/*
			 * Retry: reallocate a larger buffer and retry the
			 * msg_rcv
			 */
			continue;
		} else if (receiveResult != MACH_MSG_SUCCESS) {
			dwarning(("%s(): mach_msg: $%08x: %s\n", __FUNCTION__, receiveResult, mach_error_string(receiveResult)));
			err = receiveResult;
			goto Return;
		}
		if (msgReceiveBufPtr->msgh_local_port == gDiskArbRcvPort) {
			bzero(msgSendBufPtr, sizeof(mach_msg_header_t));
			dwarning(("%s: DiskArbHandleMsg...\n", __FUNCTION__));
			(void) DiskArbHandleMsg(msgReceiveBufPtr, msgSendBufPtr);
			dwarning(("%s: mach_msg_send...\n", __FUNCTION__));
			(void) mach_msg_send(msgSendBufPtr);
			goto Return;
		} else {
			dwarning(("FAILURE: unrecognized msgh_local_port = $%x\n", (int) msgReceiveBufPtr->msgh_local_port));
			err = -3;
			goto Return;
		}

	} //while (1)
Return:
		if (msgReceiveBufPtr)
			free(msgReceiveBufPtr);
	if (msgSendBufPtr)
		free(msgSendBufPtr);
	return err;

} //DiskArbMsgLoop

/* Device Arbitration/Backing Store Arb */

kern_return_t DiskArbIsDeviceReservedForClient(DiskArbDiskIdentifier diskIdentifier)
{
	SetSecure();
	return DiskArbIsDeviceReservedForClient_rpc(gDiskArbSndPort, diskIdentifier, getpid());

}

kern_return_t
DiskArbRetainClientReservationForDevice(DiskArbDiskIdentifier diskIdentifier)
{
	SetSecure();
	return DiskArbRetainClientReservationForDevice_rpc(gDiskArbSndPort, diskIdentifier, getpid());

}

kern_return_t
DiskArbReleaseClientReservationForDevice(DiskArbDiskIdentifier diskIdentifier)
{
	SetSecure();
	return DiskArbReleaseClientReservationForDevice_rpc(gDiskArbSndPort, diskIdentifier, getpid());

}

kern_return_t
DiskArbClientRelinquishesReservation(DiskArbDiskIdentifier diskIdentifier, int releaseToClientPid, int status)
{
	SetSecure();
	return DiskArbClientRelinquishesReservation_rpc(gDiskArbSndPort, diskIdentifier, getpid(), releaseToClientPid, status);

}

/* register yourself */
kern_return_t
DiskArbClientHandlesUnrecognizedDisks(int diskTypes, int priority)
{
	SetSecure();
	return DiskArbClientHandlesUnrecognizedDisks_rpc(gDiskArbSndPort, getpid(), diskTypes, priority);

}

kern_return_t
DiskArbClientWillHandleUnrecognizedDisk(DiskArbDiskIdentifier diskIdentifier, int yesNo)
{
	SetSecure();
	return DiskArbClientWillHandleUnrecognizedDisk_rpc(gDiskArbSndPort, diskIdentifier, getpid(), yesNo);

}



/* Other stuff */

void
DiskArbUpdateClientFlags()
{
	DiskArbUpdateClientWithPID_auto(gDiskArbitration_ClientFlags);
	return;
}

int
DiskArbIsActive()
{
	if (gDiskArbSndPort != NULL && gDiskArbRcvPort != NULL) {
		return 1;
	}
	return 0;
}

/* Security/Authorization */

void
SetSecure(void)
{
	static int      securityTokenPassed = 0;

	if (!securityTokenPassed) {
		static AuthorizationRef ref;
		int             error;

		int             flags = kAuthorizationFlagPreAuthorize | kAuthorizationFlagPartialRights | kAuthorizationFlagExtendRights;

		error = AuthorizationCreate(NULL, NULL, flags, &ref);

		//printf("Passing security token err = %d\n", error);

		if (!error) {
			AuthorizationExternalForm externalForm;
			error = AuthorizationMakeExternalForm(ref, &externalForm);

			//printf("Making security pass err = %d\n", error);

			if (!error) {
				DiskArbSetSecuritySettingsForClient_rpc(gDiskArbSndPort, getpid(), externalForm.bytes);
			}
		}
		securityTokenPassed++;
	}
	return;
}


void DiskArb_EjectKeyPressed()
{
    return;
}
