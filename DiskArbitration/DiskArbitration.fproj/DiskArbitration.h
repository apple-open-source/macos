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

#ifndef __DISKARBITRATION_H
#define __DISKARBITRATION_H


#include <DiskArbitration/DiskArbitrationPublicAPI.h>
#include <DiskArbitration/DiskArbitrationTypes.h>

#include <mach/mach.h>

#if __cplusplus
extern "C" {
#endif



/*
-- Callback registration
*/

// PRIVATE CALLBACKS - NOT PUBLISHED
// All of these callbacks eventually call DiskArbAddCallbackHandler with overwrite = 1

/* This callback is only invoked if the disk is mounted */

typedef int ( * DiskArbCallback_DiskAppearedWithMountpoint_t )(
					DiskArbDiskIdentifier diskIdentifier,
					unsigned flags,
					DiskArbMountpoint mountpoint );

void DiskArbRegisterCallback_DiskAppearedWithMountpoint( DiskArbCallback_DiskAppearedWithMountpoint_t callback );

/* This callback is only invoked if the disk is not mounted */

typedef int ( * DiskArbCallback_DiskAppearedWithoutMountpoint_t )(
					DiskArbDiskIdentifier diskIdentifier,
					unsigned flags );

void DiskArbRegisterCallback_DiskAppearedWithoutMountpoint( DiskArbCallback_DiskAppearedWithoutMountpoint_t callback );


/* Obsoleted by: DiskArbCallback_DiskAppeared2_t */

typedef int ( * DiskArbCallback_DiskAppeared_t )(
					DiskArbDiskIdentifier diskIdentifier,
					unsigned flags,
					DiskArbMountpoint mountpoint,
					DiskArbIOContent ioContent );

/* Obsoleted by: DiskArbRegisterCallback_DiskAppeared2 */

void DiskArbRegisterCallback_DiskAppeared( DiskArbCallback_DiskAppeared_t callback );

void DiskArbRegisterCallback_DiskAppeared2( DiskArbCallback_DiskAppeared2_t callback );

/* Obsoleted by: DiskArbCallback_UnmountPreNotification_t + DiskArbCallback_UnmountPostNotification_t */

typedef int ( * DiskArbCallback_UnmountNotification_t )(
					DiskArbDiskIdentifier diskIdentifier,
					int pastOrFuture,
					int willEject );

void DiskArbRegisterCallback_UnmountNotification( DiskArbCallback_UnmountNotification_t callback );


/*
-- Async unmount + eject
*/
void DiskArbRegisterCallback_UnmountPreNotification( DiskArbCallback_UnmountPreNotification_t callback );
void DiskArbRegisterCallback_UnmountPostNotification( DiskArbCallback_UnmountPostNotification_t callback );
void DiskArbRegisterCallback_EjectPreNotification( DiskArbCallback_EjectPreNotification_t callback );
void DiskArbRegisterCallback_EjectPostNotification( DiskArbCallback_EjectPostNotification_t callback );


void DiskArbRegisterCallback_ClientDisconnectedNotification( DiskArbCallback_ClientDisconnectedNotification_t callback );

/*
-- Blue Box boot volume
*/


typedef void ( * DiskArbCallback_BlueBoxBootVolumeUpdated_t )(
					int seqno );

void DiskArbRegisterCallback_BlueBoxBootVolumeUpdated( DiskArbCallback_BlueBoxBootVolumeUpdated_t callback );

void DiskArbRegisterCallback_DiskChangedNotification( DiskArbCallback_DiskChangedNotification_t callback );
void DiskArbRegisterCallback_DiskWillBeCheckedNotification( DiskArbCallback_DiskWillBeCheckedNotification_t callback );
void DiskArbRegisterCallback_CallFailedNotification( DiskArbCallback_CallFailedNotification_t callback );
void DiskArbRegisterCallback_NotificationComplete( DiskArbCallback_NotificationComplete_t callback );

/*
-- Unknown File System Insertion
*/

typedef void ( * DiskArbCallback_UnknownFileSystemNotification_t )(					DiskArbDiskIdentifier diskIdentifier,
                        char *fsType,
                        char *deviceType,
                        int isWritable,
                        int isRemovable,
                        int isWhole);

void DiskArbRegisterCallback_UnknownFileSystemNotification( DiskArbCallback_UnknownFileSystemNotification_t callback );

/*
-- Printer Arbitration
*/

typedef void ( * DiskArbCallback_Printer_FinalRequest_t )(int pid, int locationID);
void DiskArbRegisterCallback_Printer_FinalRequest( DiskArbCallback_Printer_FinalRequest_t callback );

typedef void ( * DiskArbCallback_Printer_FinalResponse_t )(int locationID, int answer);
void DiskArbRegisterCallback_Printer_FinalResponse( DiskArbCallback_Printer_FinalResponse_t callback );

typedef void ( * DiskArbCallback_Printer_FinalRelease_t )(int locationID);
void DiskArbRegisterCallback_Printer_FinalRelease( DiskArbCallback_Printer_FinalRelease_t callback );

/*
-- Server -> Client
*/


boolean_t DiskArbHandleMsg(
			mach_msg_header_t *InHeadP,
			mach_msg_header_t *OutHeadP );


/*
-- Client -> Server
*/


kern_return_t DiskArbRegister(
				mach_port_t server,
				mach_port_t client,
				unsigned flags);

kern_return_t DiskArbRegisterWithPID(
                                mach_port_t server,
                                mach_port_t client,
                                unsigned flags);

kern_return_t DiskArbDeregister(
                                mach_port_t server,
                                mach_port_t client);

kern_return_t DiskArbDiskAppearedWithMountpointPing(
				mach_port_t server,
				DiskArbDiskIdentifier diskIdentifier,
				unsigned flags,
				DiskArbMountpoint mountpoint);

kern_return_t DiskArbDiskDisappearedPing(
                                mach_port_t server,
                                DiskArbDiskIdentifier diskIdentifier,
                                unsigned flags);

kern_return_t DiskArbRequestMount(
                                mach_port_t server,
                                DiskArbDiskIdentifier diskIdentifier);

kern_return_t DiskArbRequestMountAndOwn(
                                mach_port_t server,
                                DiskArbDiskIdentifier diskIdentifier);

kern_return_t DiskArbRefresh(
				mach_port_t server);

/*
-- "automagic" versions of Client -> Server
*/


kern_return_t DiskArbRegister_auto(
				mach_port_t client,
				unsigned flags);

kern_return_t DiskArbDeregister_auto(
                                     mach_port_t client);

kern_return_t DiskArbRegisterWithPID_auto(
                                mach_port_t client,
                                unsigned flags);

kern_return_t DiskArbUpdateClientWithPID_auto(
                                unsigned flags);


kern_return_t DiskArbDeregisterWithPID_auto(
                                mach_port_t client);


kern_return_t DiskArbMarkPIDNew_auto(
                                     mach_port_t client,
                                     unsigned flags);

kern_return_t DiskArbDiskAppearedWithMountpointPing_auto(
				DiskArbDiskIdentifier diskIdentifier,
				unsigned flags,
				DiskArbMountpoint mountpoint);

kern_return_t DiskArbDiskDisappearedPing_auto(
                                DiskArbDiskIdentifier diskIdentifier,
                                unsigned flags);

kern_return_t DiskArbRequestMount_auto(
                                DiskArbDiskIdentifier diskIdentifier);

kern_return_t DiskArbRequestMountAndOwn_auto(
                                DiskArbDiskIdentifier diskIdentifier);


kern_return_t DiskArbRefresh_auto();



/*      
-- DiskArbStart()               
-- Before calling this, register any desired callbacks.
-- Output is a receive right for a port.  A send right for that port has been passed via
-- a message to the server.  Messages from the server to this client will be sent on that
-- port and should be handled via the public routine DiskArbHandleMsg().
-- Also returns an error code.
*/              
        

kern_return_t DiskArbStart(mach_port_t * portPtr);

/*      
-- DiskArbInit()               
-- Clients that don't register any callbacks should call DiskArbInit() instead of DiskArbStart().
-- Returns an error code.
*/              
        

kern_return_t DiskArbInit(void);

/*
-- Async unmount + eject
*/


kern_return_t DiskArbUnmountRequest_async_auto(
				DiskArbDiskIdentifier diskIdentifier,
				unsigned flags);
				
kern_return_t DiskArbUnmountPreNotifyAck_async_auto(
				DiskArbDiskIdentifier diskIdentifier,
				int errorCode);

kern_return_t DiskArbEjectRequest_async_auto(
				DiskArbDiskIdentifier diskIdentifier,
				unsigned flags);
				
kern_return_t DiskArbEjectPreNotifyAck_async_auto(
				DiskArbDiskIdentifier diskIdentifier,
				int errorCode);

kern_return_t DiskArbUnmountAndEjectRequest_async_auto(
				DiskArbDiskIdentifier diskIdentifier,
				unsigned flags);


/*
-- Async Helper
*/


/* DiskArbMsgLoop() == DiskArbMsgLoopWithTimeout( MACH_MSG_TIMEOUT_NONE ) */

kern_return_t DiskArbMsgLoop( void );

/* Caller should be ready to handle a MACH_RCV_TIMED_OUT return value. */

kern_return_t DiskArbMsgLoopWithTimeout( mach_msg_timeout_t timeout );


/*
-- Blue Box boot volume
*/


/* Sets the kDiskArbIAmBlueBox flag on the corresponding client record. */

kern_return_t DiskArbSetBlueBoxBootVolume_async_auto(
				int pid,
				int seqno);

/*
-- Request Disk Update
*/

kern_return_t DiskArbRequestDiskChange_auto(
                                 DiskArbDiskIdentifier diskIdentifier,
                                 DiskArbGenericString mountPoint,
                                 int flags);

/*
-- Change the currently logged in user, pass kDiskArbNoUser (-1) to signify no user logged in to console
*/

kern_return_t DiskArbSetCurrentUser_auto(int user);

/*
-- VSDB functionality
*/

kern_return_t DiskArbVSDBAdoptVolume_auto(DiskArbDiskIdentifier diskIdentifier);
kern_return_t DiskArbVSDBDisownVolume_auto(DiskArbDiskIdentifier diskIdentifier);

// blocking routine, use sparingly
int DiskArbVSDBGetVolumeStatus_auto(DiskArbDiskIdentifier diskIdentifier);


/* 
-- HFS Volume Encoding
*/

kern_return_t DiskArbSetVolumeEncoding_auto(DiskArbDiskIdentifier diskIdentifier, int VolumeEncoding);

// blocking routine, use sparingly
int DiskArbGetVolumeEncoding_auto(DiskArbDiskIdentifier diskIdentifier);

/*
 -- No Op - specifically allows the Carbon framework load disk arb earlier in process
 */

void DiskArbNoOp();

/*
-- Uninitialized Disk Handler
*/


/* Sets/unsets the kDiskArbClientHandlesUninitializedDisks flag on the corresponding client record. Pass 1 or 0 as the flag. */

kern_return_t DiskArbClientHandlesUninitializedDisks_auto(
                                int flag);


/*
-- Printer Arbitration
*/


kern_return_t DiskArbPrinter_Request_auto (
					int locationID);

kern_return_t DiskArbPrinter_Response_auto (
					int pid,
					int locationID,
					int answer);

kern_return_t DiskArbPrinter_Release_auto (
					int locationID);

void SetSecure(void);

kern_return_t DiskArb_EjectKeyPressed();

#if __cplusplus
}
#endif

#endif
