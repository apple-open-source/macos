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
#include <mach/mach_error.h>
#include <unistd.h>

#include <DiskArbitration.h>

#include "ClientToServer.h"
#include "ServerToClient_server.h"


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

static DiskArbCallback_DiskAppearedWithMountpoint_t gCallback_DiskAppearedWithMountpoint = NULL;
static DiskArbCallback_DiskAppearedWithoutMountpoint_t gCallback_DiskAppearedWithoutMountpoint = NULL;
static DiskArbCallback_DiskAppeared_t gCallback_DiskAppeared = NULL;
static DiskArbCallback_DiskAppeared2_t gCallback_DiskAppeared2 = NULL;

static DiskArbCallback_UnmountNotification_t gCallback_UnmountNotification = NULL;

static DiskArbCallback_UnmountPreNotification_t gCallback_UnmountPreNotification = NULL;
static DiskArbCallback_UnmountPostNotification_t gCallback_UnmountPostNotification = NULL;
static DiskArbCallback_EjectPreNotification_t gCallback_EjectPreNotification = NULL;
static DiskArbCallback_EjectPostNotification_t gCallback_EjectPostNotification = NULL;

static DiskArbCallback_ClientDisconnectedNotification_t gCallback_ClientDisconnectedNotification = NULL;

static DiskArbCallback_BlueBoxBootVolumeUpdated_t gCallback_BlueBoxBootVolumeUpdated = NULL;

static DiskArbCallback_UnknownFileSystemNotification_t gCallback_UnknownFileSystemNotification = NULL;

static DiskArbCallback_DiskChangedNotification_t gCallback_DiskChangedNotification = NULL;

static DiskArbCallback_NotificationComplete_t gCallback_NotificationComplete = NULL;

static DiskArbCallback_Printer_FinalRequest_t gCallback_Printer_FinalRequest = NULL;
static DiskArbCallback_Printer_FinalResponse_t gCallback_Printer_FinalResponse = NULL;
static DiskArbCallback_Printer_FinalRelease_t gCallback_Printer_FinalRelease = NULL;

/*
-- Callback registration
*/


void DiskArbRegisterCallback_DiskAppearedWithMountpoint( DiskArbCallback_DiskAppearedWithMountpoint_t callback )
{
	gCallback_DiskAppearedWithMountpoint = callback;
}

void DiskArbRegisterCallback_DiskAppearedWithoutMountpoint( DiskArbCallback_DiskAppearedWithoutMountpoint_t callback )
{
	gCallback_DiskAppearedWithoutMountpoint = callback;
}

void DiskArbRegisterCallback_DiskAppeared( DiskArbCallback_DiskAppeared_t callback )
{
	gCallback_DiskAppeared = callback;
}

void DiskArbRegisterCallback_DiskAppeared2( DiskArbCallback_DiskAppeared2_t callback )
{
	gCallback_DiskAppeared2 = callback;
}

void DiskArbRegisterCallback_UnmountNotification( DiskArbCallback_UnmountNotification_t callback )
{
	gCallback_UnmountNotification = callback;
}

void DiskArbRegisterCallback_UnmountPreNotification( DiskArbCallback_UnmountPreNotification_t callback )
{
	gCallback_UnmountPreNotification = callback;
}

void DiskArbRegisterCallback_UnmountPostNotification( DiskArbCallback_UnmountPostNotification_t callback )
{
	gCallback_UnmountPostNotification = callback;
}

void DiskArbRegisterCallback_EjectPreNotification( DiskArbCallback_EjectPreNotification_t callback )
{
	gCallback_EjectPreNotification = callback;
}

void DiskArbRegisterCallback_EjectPostNotification( DiskArbCallback_EjectPostNotification_t callback )
{
	gCallback_EjectPostNotification = callback;
}

void DiskArbRegisterCallback_ClientDisconnectedNotification( DiskArbCallback_ClientDisconnectedNotification_t callback )
{
        gCallback_ClientDisconnectedNotification = callback;
}

void DiskArbRegisterCallback_BlueBoxBootVolumeUpdated( DiskArbCallback_BlueBoxBootVolumeUpdated_t callback )
{
	gCallback_BlueBoxBootVolumeUpdated = callback;
}

void DiskArbRegisterCallback_UnknownFileSystemNotification( DiskArbCallback_UnknownFileSystemNotification_t callback )
{
        gCallback_UnknownFileSystemNotification = callback;
}

void DiskArbRegisterCallback_DiskChangedNotification( DiskArbCallback_DiskChangedNotification_t callback )
{
        gCallback_DiskChangedNotification = callback;
}

void DiskArbRegisterCallback_NotificationComplete( DiskArbCallback_NotificationComplete_t callback )
{
        gCallback_NotificationComplete = callback;
}

void DiskArbRegisterCallback_Printer_FinalRequest( DiskArbCallback_Printer_FinalRequest_t callback )
{
        gCallback_Printer_FinalRequest = callback;
}

void DiskArbRegisterCallback_Printer_FinalResponse( DiskArbCallback_Printer_FinalResponse_t callback )
{
        gCallback_Printer_FinalResponse = callback;
}

void DiskArbRegisterCallback_Printer_FinalRelease( DiskArbCallback_Printer_FinalRelease_t callback )
{
        gCallback_Printer_FinalRelease = callback;
}


/*
-- Public Message handler
*/


boolean_t DiskArbHandleMsg(
		mach_msg_header_t *InHeadP,
		mach_msg_header_t *OutHeadP)
{
	return ServerToClient_server( InHeadP, OutHeadP );
}


/*
-- Client -> Server
*/

kern_return_t DiskArbRegister(
				mach_port_t server,
				mach_port_t client,
				unsigned flags)
{
	gDiskArbRcvPort = client;
	return DiskArbRegister_rpc( server, client, flags );
}

kern_return_t DiskArbDeregister(
                                mach_port_t server,
                                mach_port_t client)
{
        return DiskArbDeregister_rpc( server, client );
}

kern_return_t DiskArbDiskAppearedWithMountpointPing(
				mach_port_t server,
				DiskArbDiskIdentifier diskIdentifier,
				unsigned flags,
				DiskArbMountpoint mountpoint)
{
	return DiskArbDiskAppearedWithMountpointPing_rpc( server, diskIdentifier, flags, mountpoint );
}

kern_return_t DiskArbDiskDisappearedPing(
                                mach_port_t server,
                                DiskArbDiskIdentifier diskIdentifier,
                                unsigned flags)
{
        return DiskArbDiskDisappearedPing_rpc( server, diskIdentifier, flags );
}

kern_return_t DiskArbRequestMount(
                                mach_port_t server,
                                DiskArbDiskIdentifier diskIdentifier)
{
        return DiskArbRequestMount_rpc( server, diskIdentifier, FALSE );
}

kern_return_t DiskArbRequestMountAndOwn(
                                mach_port_t server,
                                DiskArbDiskIdentifier diskIdentifier)
{
        return DiskArbRequestMount_rpc( server, diskIdentifier, TRUE );
}


kern_return_t DiskArbRefresh(
				mach_port_t server)
{
	return DiskArbRefresh_rpc( server );
}

kern_return_t DiskArbRegisterWithPID(
				mach_port_t server,
				mach_port_t client,
				unsigned flags)
{
	gDiskArbRcvPort = client;
	return DiskArbRegisterWithPID_rpc( server, client, getpid(), flags );
}


/*
-- Server -> Client
*/


/******************************************* DiskArbDiskAppearedWithoutMountpoint_rpc *******************************************/

kern_return_t DiskArbDiskAppearedWithoutMountpoint_rpc (
	mach_port_t server,
	DiskArbDiskIdentifier diskIdentifier,
	unsigned flags)
{
	kern_return_t err = 0;
	int callbackErrorCode = 0;
	
	dwarning(("%s(diskIdentifier='%s')\n", __FUNCTION__, diskIdentifier));
	
	if ( NULL == gCallback_DiskAppearedWithoutMountpoint )
	{
		goto Return;
	}

	callbackErrorCode = ( * gCallback_DiskAppearedWithoutMountpoint )( diskIdentifier, flags );

Return:
	//* errorCode = callbackErrorCode;
	return err;

} // DiskArbDiskAppearedWithoutMountpoint_rpc


/******************************************* DiskArbDiskAppearedWithMountpoint_rpc *****************************/

kern_return_t DiskArbDiskAppearedWithMountpoint_rpc (
	mach_port_t server,
	DiskArbDiskIdentifier diskIdentifier,
	unsigned flags,
	DiskArbMountpoint mountpoint)
{
	kern_return_t err = 0;
	int callbackErrorCode = 0;
	
	dwarning(("%s(diskIdentifier='%s', flags=0x%08x, mountpoint='%s')\n", __FUNCTION__, diskIdentifier, flags, mountpoint));
	
	if ( NULL == gCallback_DiskAppearedWithMountpoint )
	{
		goto Return;
	}

	callbackErrorCode = ( * gCallback_DiskAppearedWithMountpoint )( diskIdentifier, flags, mountpoint );
	
Return:
	//* errorCode = callbackErrorCode;
	return err;

} // DiskArbDiskAppearedWithMountpoint_rpc

/******************************************* DiskArbDiskAppeared_rpc *****************************/

kern_return_t DiskArbDiskAppeared_rpc (
	mach_port_t server,
	DiskArbDiskIdentifier diskIdentifier,
	unsigned flags,
	DiskArbMountpoint mountpoint,
	DiskArbIOContent ioContent)
{
	kern_return_t err = 0;
	int callbackErrorCode = 0;
	
	dwarning(("%s(diskIdentifier='%s', flags=0x%08x, mountpoint='%s', ioContent='%s')\n", __FUNCTION__, diskIdentifier, flags, mountpoint, ioContent));

	if ( NULL == gCallback_DiskAppeared )
	{
		goto Return;
	}

	callbackErrorCode = ( * gCallback_DiskAppeared )( diskIdentifier, flags, mountpoint, ioContent );
	
Return:
	//* errorCode = callbackErrorCode;
	return err;

} // DiskArbDiskAppeared_rpc

/******************************************* DiskArbDiskAppeared2_rpc *****************************/

kern_return_t DiskArbDiskAppeared2_rpc (
	mach_port_t server,
	DiskArbDiskIdentifier diskIdentifier,
	unsigned flags,
	DiskArbMountpoint mountpoint,
	DiskArbIOContent ioContent,
	DiskArbDeviceTreePath deviceTreePath,
	unsigned sequenceNumber)
{
	kern_return_t err = 0;
	int callbackErrorCode = 0;
	
	dwarning(("%s(diskIdentifier='%s', flags=0x%08x, mountpoint='%s', ioContent='%s', deviceTreePath='%s', sequenceNumber=%d)\n", __FUNCTION__, diskIdentifier, flags, mountpoint, ioContent, deviceTreePath, sequenceNumber));

	if ( NULL == gCallback_DiskAppeared2 )
	{
		goto Return;
	}

	callbackErrorCode = ( * gCallback_DiskAppeared2 )( diskIdentifier, flags, mountpoint, ioContent, deviceTreePath, sequenceNumber );
	
Return:
	//* errorCode = callbackErrorCode;
	return err;

} // DiskArbDiskAppeared2_rpc

/******************************************* DiskArbUnmountNotification_rpc *****************************/

kern_return_t DiskArbUnmountNotification_rpc(
	mach_port_t server,
	DiskArbDiskIdentifier diskIdentifier,
	int pastOrFuture,
	int willEject)
{
	kern_return_t err = 0;
	int callbackErrorCode = 0;
	
	dwarning(("%s(diskIdentifier='%s')\n", __FUNCTION__, diskIdentifier));

	if ( NULL == gCallback_UnmountNotification )
	{
		goto Return;
	}

	callbackErrorCode = ( * gCallback_UnmountNotification )( diskIdentifier, pastOrFuture, willEject );
	
Return:
	//* errorCode = callbackErrorCode;
	return err;

} // DiskArbUnmountNotification_rpc


/*
-- Server -> Client (Async)
*/


/******************************************* DiskArbUnmountPreNotify_async_rpc *****************************/

kern_return_t DiskArbUnmountPreNotify_async_rpc(
	mach_port_t server,
	DiskArbDiskIdentifier diskIdentifier,
	unsigned flags)
{
	kern_return_t err = 0;

	dwarning(("%s(diskIdentifier='%s', flags = 0x%08x)\n", __FUNCTION__, diskIdentifier, flags));

	if ( NULL == gCallback_UnmountPreNotification )
	{
		goto Return;
	}

	( * gCallback_UnmountPreNotification )( diskIdentifier, flags );
	
Return:
	return err;

} // DiskArbUnmountPreNotify_async_rpc

/******************************************* DiskArbUnmountPostNotify_async_rpc *****************************/

kern_return_t DiskArbUnmountPostNotify_async_rpc(
	mach_port_t server,
	DiskArbDiskIdentifier diskIdentifier,
	int errorCode,
	int dissenter)
{
	kern_return_t err = 0;

	dwarning(("%s(diskIdentifier='%s', errorCode = 0x%08x, dissenter = %d)\n", __FUNCTION__, diskIdentifier, errorCode, dissenter));

	if ( NULL == gCallback_UnmountPostNotification )
	{
		goto Return;
	}

	( * gCallback_UnmountPostNotification )( diskIdentifier, errorCode, dissenter );
	
Return:
	return err;

} // DiskArbUnmountPostNotify_async_rpc

/******************************************* DiskArbEjectPreNotify_async_rpc *****************************/

kern_return_t DiskArbEjectPreNotify_async_rpc(
	mach_port_t server,
	DiskArbDiskIdentifier diskIdentifier,
	unsigned flags)
{
	kern_return_t err = 0;

	dwarning(("%s(diskIdentifier='%s', flags = 0x%08x)\n", __FUNCTION__, diskIdentifier, flags));

	if ( NULL == gCallback_EjectPreNotification )
	{
		goto Return;
	}

	( * gCallback_EjectPreNotification )( diskIdentifier, flags );
	
Return:
	return err;

} // DiskArbEjectPreNotify_async_rpc

/******************************************* DiskArbEjectPostNotify_async_rpc *****************************/

kern_return_t DiskArbEjectPostNotify_async_rpc(
	mach_port_t server,
	DiskArbDiskIdentifier diskIdentifier,
	int errorCode,
	int dissenter)
{
	kern_return_t err = 0;

	dwarning(("%s(diskIdentifier='%s', errorCode = 0x%08x, dissenter = %d)\n", __FUNCTION__, diskIdentifier, errorCode, dissenter));

	if ( NULL == gCallback_EjectPostNotification )
	{
		goto Return;
	}

	( * gCallback_EjectPostNotification )( diskIdentifier, errorCode, dissenter );
	
Return:
	return err;

} // DiskArbEjectPostNotify_async_rpc

/******************************************* DiskArbClientDisconnected_rpc *****************************/
kern_return_t DiskArbClientDisconnected_rpc(
        mach_port_t server)
{
    kern_return_t err = 0;

    dwarning(("%s\n", __FUNCTION__));

    if ( NULL == gCallback_ClientDisconnectedNotification )
    {
            goto Return;
    }

    ( * gCallback_ClientDisconnectedNotification )();

Return:
    return err;
} // DiskArbClientDisconnected_rpc



/******************************************* DiskArbBlueBoxBootVolumeUpdated_async_rpc *****************************/

kern_return_t DiskArbBlueBoxBootVolumeUpdated_async_rpc(
	mach_port_t server,
	int seqno)
{
	kern_return_t err = 0;

	dwarning(("%s(seqno = %d)\n", __FUNCTION__, seqno));

	if ( NULL == gCallback_BlueBoxBootVolumeUpdated )
	{
		goto Return;
	}

	( * gCallback_BlueBoxBootVolumeUpdated )( seqno );
	
Return:
	return err;

} // DiskArbBlueBoxBootVolumeUpdated_async_rpc

/******************************************* DiskArbUnknownFileSystemInserted_rpc *****************************/


kern_return_t DiskArbUnknownFileSystemInserted_rpc(
                        mach_port_t server,
                        char *	diskIdentifier,
                        char * fsType,
                        char * deviceType,
                        int isWritable,
                        int isRemovable,
                        int isWhole)
{
        kern_return_t err = 0;

        dwarning(("%s\n", __FUNCTION__));

        if ( NULL == gCallback_UnknownFileSystemNotification )
        {
                goto Return;
        }

        ( * gCallback_UnknownFileSystemNotification )(diskIdentifier, fsType, deviceType, isWritable, isRemovable, isWhole);

    Return:
        return err;
} // DiskArbUnknownFileSystemInserted_rpc

/******************************************* DiskArbDiskChanged_rpc *****************************/


kern_return_t DiskArbDiskChanged_rpc(
                        mach_port_t server,
                        DiskArbDiskIdentifier	diskIdentifier,
                        DiskArbMountpoint newMountPoint,
                        DiskArbMountpoint newVolumeName,
                        unsigned flags,
                        int success)
{
        kern_return_t err = 0;

        dwarning(("%s\n", __FUNCTION__));

        if ( NULL == gCallback_DiskChangedNotification )
        {
                goto Return;
        }

        ( * gCallback_DiskChangedNotification )(diskIdentifier, newMountPoint, newVolumeName, flags, success);

    Return:
        return err;
} // DiskArbDiskChanged_rpc


/******************************************* DiskArbNotificationComplete_rpc *****************************/


kern_return_t DiskArbNotificationComplete_rpc(
                        mach_port_t server,
                        int messageType)
{
        kern_return_t err = 0;

        dwarning(("%s\n", __FUNCTION__));

        if ( NULL == gCallback_NotificationComplete )
        {
                goto Return;
        }

        ( * gCallback_NotificationComplete )(messageType);

    Return:
        return err;
} // DiskArbNotificationComplete_rpc

/******************************************* DiskArbNotificationComplete_rpc *****************************/


kern_return_t DiskArbRegistrationComplete_rpc(
                        mach_port_t server)
{
        kern_return_t err = 0;
        dwarning(("%s\n", __FUNCTION__));
        return err;
} // DiskArbRegistrationComplete_rpc

/*
-- Printer Arbitration
*/

kern_return_t DiskArbPrinter_FinalRequest_rpc (
                        mach_port_t server,
                        int pid,
                        int locationID)
{
	kern_return_t err = 0;
	
	dwarning(("%s(pid=%d,locationID=0x%08x)\n", __FUNCTION__, pid, locationID));

	if ( NULL == gCallback_Printer_FinalRequest )
	{
		goto Return;
	}

	( * gCallback_Printer_FinalRequest )(pid, locationID);

Return:
	return err;
}

kern_return_t DiskArbPrinter_FinalResponse_rpc (
                        mach_port_t server,
                        int locationID,
                        int answer)
{
	kern_return_t err = 0;
	
	dwarning(("%s(locationID=0x%08x,answer=0x%08x)\n", __FUNCTION__, locationID, answer));

	if ( NULL == gCallback_Printer_FinalResponse )
	{
		goto Return;
	}

	( * gCallback_Printer_FinalResponse )(locationID, answer);

Return:
	return err;
}

kern_return_t DiskArbPrinter_FinalRelease_rpc (
                        mach_port_t server,
                        int locationID)
{
	kern_return_t err = 0;
	
	dwarning(("%s(locationID=0x%08x)\n", __FUNCTION__, locationID));

	if ( NULL == gCallback_Printer_FinalRelease )
	{
		goto Return;
	}

	( * gCallback_Printer_FinalRelease )(locationID);

Return:
	return err;
}



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
	kern_return_t	result;
	//int				errorCode;
	unsigned		flags;
        static mach_port_t	pre_registered_port = 0x0;

	// Obtain a send right for the DiskArbitration server's public port via the bootstrap server		

	result = bootstrap_look_up( bootstrap_port, DISKARB_SERVER_NAME, & gDiskArbSndPort );
	if ( result ) 
	{
		dwarning(("%s(): {%s:%d} bootstrap_look_up() failed: $%x\n", __FUNCTION__, __FILE__, __LINE__, (int)result));
		goto Return;
	}
	
	dwarning(("%s(): gDiskArbSndPort = %d\n", __FUNCTION__, (int)gDiskArbSndPort));

	// Allocate gDiskArbRcvPort with a receive right.  DiskArbRegister() will create a send right.
        if (!pre_registered_port) {
            result = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, portPtr);
            if ( result != KERN_SUCCESS)
            {
                    dwarning(("%s(): (%s:%d) mach_port_allocate failed: $%x: %s\n", __FUNCTION__, __FILE__, __LINE__, result, mach_error_string(result)));
                    goto Return;
            }
        } else {
            * portPtr = pre_registered_port;
        }
		
	dwarning(("%s(): port = $%x\n", __FUNCTION__, (int)*portPtr));
	
	
	// Create the flags word based on the registered callbacks.
	
	flags = kDiskArbNotifyNone;

	if ( gCallback_DiskAppearedWithoutMountpoint != NULL )
		flags |= kDiskArbNotifyDiskAppearedWithoutMountpoint;
	if ( gCallback_DiskAppearedWithMountpoint != NULL )
		flags |= kDiskArbNotifyDiskAppearedWithMountpoint;
	if ( gCallback_DiskAppeared != NULL )
		flags |= kDiskArbNotifyDiskAppeared;
	if ( gCallback_DiskAppeared2 != NULL )
		flags |= kDiskArbNotifyDiskAppeared2;
	if ( gCallback_UnmountNotification != NULL )
		flags |= kDiskArbNotifyUnmount;
        if ( gCallback_UnmountPreNotification != NULL || gCallback_UnmountPostNotification != NULL || gCallback_EjectPreNotification != NULL || gCallback_EjectPostNotification != NULL ) {
            if ( gCallback_UnmountPreNotification != NULL && gCallback_UnmountPostNotification != NULL && gCallback_EjectPreNotification != NULL && gCallback_EjectPostNotification != NULL ) {
                flags |= kDiskArbNotifyAsync;
            } else {
                result = -1;
                printf("DiskArbStart: Disk Arbitration: All Async Clients Must Register For All Async messages!\n");
                goto Return;
            }
        }
	if ( gCallback_BlueBoxBootVolumeUpdated != NULL )
		flags |= kDiskArbNotifyBlueBoxBootVolumeUpdated;
        if ( gCallback_NotificationComplete != NULL )
                flags |= kDiskArbNotifyCompleted;
        if ( gCallback_DiskChangedNotification != NULL )
                flags |= kDiskArbNotifyChangedDisks;
        if ( gCallback_UnknownFileSystemNotification != NULL )
                flags |= kDiskArbNotifyUnrecognizedVolumes;



	dwarning(("%s(): flags = %08x\n", __FUNCTION__, flags));
	
	// Register with the DiskArbitration server

        if (!pre_registered_port) {
            result = DiskArbRegisterWithPID_auto( *portPtr, flags);
            DiskArbMsgLoopWithTimeout(5000);
            if ( result )
            {
                    dwarning(("%s(): {%s:%d} DiskArbRegister(sendPort=$%08x, *portPtr=$%08x, flags=$%08x) failed: $%x\n", __FUNCTION__, __FILE__, __LINE__, gDiskArbSndPort, *portPtr, flags, (int)result));
                    goto Return;
            }
        } else {
            int val = 0;
            // tell autodiskmount that this client is new?

            val = DiskArbMarkPIDNew_auto(*portPtr, flags);
            
            gDiskArbRcvPort = *portPtr;
        }

        pre_registered_port = *portPtr;

Return:

	return result;
	
} // DiskArbStart


/*
-- DiskArbInit()
-- Clients that don't register any callbacks should use DiskArbInit() instead of DiskArbStart().
-- Returns an error code.
*/

kern_return_t DiskArbInit(void)
{
	kern_return_t	result;

	if ( gDiskArbSndPort != MACH_PORT_NULL )
	{
		result = 0;
		goto Return;
	}

	// Obtain a send right for the DiskArbitration server's public port via the bootstrap server		

	result = bootstrap_look_up( bootstrap_port, DISKARB_SERVER_NAME, & gDiskArbSndPort );
	if ( result ) 
	{
		dwarning(("%s(): {%s:%d} bootstrap_look_up() failed: $%x\n", __FUNCTION__, __FILE__, __LINE__, (int)result));
		goto Return;
	}
	
	dwarning(("%s(): gDiskArbSndPort = %d\n", __FUNCTION__, (int)gDiskArbSndPort));

Return:

	return result;
	
} // DiskArbInit


/*
-- "automagic" versions of Client -> Server
*/

kern_return_t DiskArbRegister_auto(
				mach_port_t client,
				unsigned flags)
{
	gDiskArbRcvPort = client;
	return DiskArbRegister_rpc( gDiskArbSndPort, client, flags );
}

kern_return_t DiskArbDeregister_auto(
                                mach_port_t client)
{
        return DiskArbDeregister_rpc( gDiskArbSndPort, client );
}

kern_return_t DiskArbDiskAppearedWithMountpointPing_auto(
				DiskArbDiskIdentifier diskIdentifier,
				unsigned flags,
				DiskArbMountpoint mountpoint)
{
	return DiskArbDiskAppearedWithMountpointPing_rpc( gDiskArbSndPort, diskIdentifier, flags, mountpoint );
}

kern_return_t DiskArbDiskDisappearedPing_auto(
                                DiskArbDiskIdentifier diskIdentifier,
                                unsigned flags)
{
        return DiskArbDiskDisappearedPing_rpc( gDiskArbSndPort, diskIdentifier, flags );
}

kern_return_t DiskArbRequestMount_auto(
                                DiskArbDiskIdentifier diskIdentifier)
{
        return DiskArbRequestMount_rpc( gDiskArbSndPort, diskIdentifier, FALSE );
}

kern_return_t DiskArbRequestMountAndOwn_auto(
                                DiskArbDiskIdentifier diskIdentifier)
{
        return DiskArbRequestMount_rpc( gDiskArbSndPort, diskIdentifier, TRUE );
}


kern_return_t DiskArbRefresh_auto()
{
	return DiskArbRefresh_rpc( gDiskArbSndPort );
}

kern_return_t DiskArbRegisterWithPID_auto(
				mach_port_t client,
				unsigned flags)
{
	gDiskArbRcvPort = client;
	return DiskArbRegisterWithPID_rpc( gDiskArbSndPort, client, getpid(), flags );
}

kern_return_t DiskArbDeregisterWithPID_auto(
                                mach_port_t client)
{
        gDiskArbRcvPort = client;
        return DiskArbDeregisterWithPID_rpc( gDiskArbSndPort, client, getpid());
}

kern_return_t DiskArbMarkPIDNew_auto(
                                     mach_port_t client,
                                     unsigned flags)
{
        gDiskArbRcvPort = client;
          
        return DiskArbMarkPIDNew_rpc( gDiskArbSndPort, client, getpid(), flags);
}

kern_return_t DiskArbRequestDiskChange_auto(
                                 DiskArbDiskIdentifier diskIdentifier,
                                 DiskArbGenericString mountPoint,
                                 int flags)
{
        return DiskArbRequestDiskChange_rpc( gDiskArbSndPort, diskIdentifier, mountPoint, flags);
}

kern_return_t DiskArbSetCurrentUser_auto( int user)
{
    return DiskArbSetCurrentUser_rpc(gDiskArbSndPort, user);
}

kern_return_t DiskArbVSDBAdoptVolume_auto(DiskArbDiskIdentifier diskIdentifier)
{
    return DiskArbVSDBAdoptVolume_rpc(gDiskArbSndPort, diskIdentifier);
}

kern_return_t DiskArbVSDBDisownVolume_auto(DiskArbDiskIdentifier diskIdentifier)
{
    return DiskArbVSDBDisownVolume_rpc(gDiskArbSndPort, diskIdentifier);
}

int DiskArbVSDBGetVolumeStatus_auto(DiskArbDiskIdentifier diskIdentifier)
{
    int status;

    DiskArbVSDBGetVolumeStatus_rpc(gDiskArbSndPort, diskIdentifier, &status);

    return status;
}

kern_return_t DiskArbSetVolumeEncoding_auto(DiskArbDiskIdentifier diskIdentifier, int volumeEncoding)
{
    return DiskArbSetVolumeEncoding_rpc(gDiskArbSndPort, diskIdentifier, volumeEncoding);
}

int DiskArbGetVolumeEncoding_auto(DiskArbDiskIdentifier diskIdentifier)
{
    int status;

    DiskArbGetVolumeEncoding_rpc(gDiskArbSndPort, diskIdentifier, &status);

    return status;
}

/* Sets the kDiskArbHandlesUninitializedDisks flag on the corresponding client record. */

kern_return_t DiskArbClientHandlesUninitializedDisks_auto(
                                int flag)
{
        printf("FOOOO\n");
        return DiskArbClientHandlesUninitializedDisks_rpc( gDiskArbSndPort, getpid(), flag );
}



/*
-- Async
*/

kern_return_t DiskArbUnmountRequest_async_auto(
				DiskArbDiskIdentifier diskIdentifier,
				unsigned flags)
{
	return DiskArbUnmountRequest_async_rpc( gDiskArbSndPort, diskIdentifier, flags );
}

kern_return_t DiskArbUnmountPreNotifyAck_async_auto(
				DiskArbDiskIdentifier diskIdentifier,
				int errorCode)
{
	return DiskArbUnmountPreNotifyAck_async_rpc( gDiskArbSndPort, getpid(), diskIdentifier, errorCode );
}

kern_return_t DiskArbEjectRequest_async_auto(
				DiskArbDiskIdentifier diskIdentifier,
				unsigned flags)
{
	return DiskArbEjectRequest_async_rpc( gDiskArbSndPort, diskIdentifier, flags );
}

kern_return_t DiskArbEjectPreNotifyAck_async_auto(
				DiskArbDiskIdentifier diskIdentifier,
				int errorCode)
{
	return DiskArbEjectPreNotifyAck_async_rpc( gDiskArbSndPort, getpid(), diskIdentifier, errorCode );
}

kern_return_t DiskArbUnmountAndEjectRequest_async_auto(
				DiskArbDiskIdentifier diskIdentifier,
				unsigned flags)
{
	return DiskArbUnmountAndEjectRequest_async_rpc( gDiskArbSndPort, diskIdentifier, flags );
}

/* Sets the kDiskArbIAmBlueBox flag on the corresponding client record. */

kern_return_t DiskArbSetBlueBoxBootVolume_async_auto(
				int pid,
				int seqno)
{
	return DiskArbSetBlueBoxBootVolume_async_rpc( gDiskArbSndPort, pid, seqno );
}


/*
-- Async Helper
*/


kern_return_t DiskArbMsgLoop( void )
{
	return DiskArbMsgLoopWithTimeout ( MACH_MSG_TIMEOUT_NONE );
}

void DiskArbNoOp( void)
{
    return;
}


kern_return_t DiskArbMsgLoopWithTimeout( mach_msg_timeout_t millisecondTimeout )
{
	kern_return_t			err = 0;

	unsigned				msgReceiveBufLength;
	mach_msg_header_t	*	msgReceiveBufPtr = NULL;
	
	unsigned				msgSendBufLength;
	mach_msg_header_t	*	msgSendBufPtr = NULL;
	
	mach_msg_return_t		receiveResult;

	msgSendBufLength = sizeof( mach_msg_empty_send_t ) + 20; /* Over-allocate */
	msgSendBufPtr = (mach_msg_header_t * )malloc( msgSendBufLength );
	if ( msgSendBufPtr == NULL )
	{
		dwarning(( "FAILURE: msgSendBufPtr = malloc(%d)\n", msgSendBufLength ));
		err = -1;
		goto Return;
	}
	else
	{
		dwarning(( "SUCCESS: msgSendBufPtr = malloc(%d)\n", msgSendBufLength ));
	}
		
	msgReceiveBufLength = sizeof( mach_msg_empty_rcv_t );
	msgReceiveBufPtr = NULL;
		
	while ( 1 )
	{
		/* (Re)allocate a buffer for receiving msgs from the server. */

		if ( msgReceiveBufPtr == NULL )
		{
			msgReceiveBufPtr = (mach_msg_header_t * )malloc( msgReceiveBufLength );
			if ( msgReceiveBufPtr == NULL )
			{
				dwarning(( "FAILURE: msgReceiveBufPtr = malloc(%d)\n", msgReceiveBufLength ));
				err = -2;
				goto Return;
			}
			else
			{
				dwarning(( "SUCCESS: msgReceiveBufPtr = malloc(%d)\n", msgReceiveBufLength ));
			}
		}

		dwarning(("gDiskArbRcvPort = $%x", gDiskArbRcvPort));
		dwarning(("%s: Waiting for a message (millisecondTimeount = %d)...\n", __FUNCTION__, millisecondTimeout));
	
		receiveResult = mach_msg(	msgReceiveBufPtr,
									MACH_RCV_MSG | MACH_RCV_LARGE | (MACH_MSG_TIMEOUT_NONE == millisecondTimeout ? 0 : MACH_RCV_TIMEOUT),
									0,
									msgReceiveBufLength,
									gDiskArbRcvPort,
									millisecondTimeout,
									MACH_PORT_NULL);

		if ( receiveResult == MACH_RCV_TOO_LARGE )
		{
			dwarning(( "%s(): mach_msg: $%08x: %s\n", __FUNCTION__, receiveResult, mach_error_string(receiveResult) ));
			dwarning(( "msgReceiveBufPtr->msgh_size = %d\n", msgReceiveBufPtr->msgh_size ));
			msgReceiveBufLength = msgReceiveBufPtr->msgh_size + sizeof(mach_msg_trailer_t);
			free( msgReceiveBufPtr );
			msgReceiveBufPtr = NULL;
			/* Retry: reallocate a larger buffer and retry the msg_rcv */
			continue;
		}
		else
		if ( receiveResult != MACH_MSG_SUCCESS )
		{
			dwarning(( "%s(): mach_msg: $%08x: %s\n", __FUNCTION__, receiveResult, mach_error_string(receiveResult) ));
			err = receiveResult;
			goto Return;
		}

		if ( msgReceiveBufPtr->msgh_local_port == gDiskArbRcvPort )
		{
			bzero( msgSendBufPtr, sizeof( mach_msg_header_t ) );
			dwarning(("%s: DiskArbHandleMsg...\n", __FUNCTION__));
			(void) DiskArbHandleMsg( msgReceiveBufPtr, msgSendBufPtr );
			dwarning(("%s: mach_msg_send...\n", __FUNCTION__));
			(void) mach_msg_send( msgSendBufPtr );
			goto Return;
		}
		else
		{
			dwarning(("FAILURE: unrecognized msgh_local_port = $%x\n", (int)msgReceiveBufPtr->msgh_local_port));
			err = -3;
			goto Return;
		}

	} // while ( 1 )

Return:
	if ( msgReceiveBufPtr ) free ( msgReceiveBufPtr );
	if ( msgSendBufPtr ) free ( msgSendBufPtr );
	return err;

} // DiskArbMsgLoop

/*
-- Printer Arbitration
*/

kern_return_t DiskArbPrinter_Request_auto (
					int locationID)
{
	return DiskArbPrinter_Request_rpc( gDiskArbSndPort, getpid(), locationID );
}

kern_return_t DiskArbPrinter_Response_auto (
					int pid,
					int locationID,
					int answer)
{
	return DiskArbPrinter_Response_rpc( gDiskArbSndPort, pid, locationID, answer );
}

kern_return_t DiskArbPrinter_Release_auto (
					int locationID)
{
	return DiskArbPrinter_Release_rpc( gDiskArbSndPort, locationID );
}
