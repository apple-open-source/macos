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
#include <stddef.h>
#include <libc.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <bsd/dev/disk.h>
#include <errno.h>
#include <sys/param.h>
#include <unistd.h>

#include <fts.h>


#include <sys/syslimits.h>
#include <mach/mach_error.h>

#include <DiskArbitration.h>
#include <DiskArbitrationPublicAPI.h>

int			refresh = 0;	// refresh
int			do_unmount = 0;	// disk unmount
int			punmount = 0;	// partition unmount
int			eject = 0;	// disk eject
int			afpmount = 0;	// afp mount requested
int			afpdismount = 0;	// afp dismount requested
int			do_mount = 0;	// mounts or remounts a volume
int			newname = 0;	// mounts or remounts a volume
int			refuse = 0; // refuse ejects?

char			*device;
char			*mountpoint;

int 			changeUser = 0;
int 			newUser = -1;


mach_port_t		diskArbitrationPort;

void unrecognizedCallback( DiskArbDiskIdentifier diskIdentifier, int diskType,                                             char * fsType,char * deviceType,int isWritable,int isRemovable,int isWhole)
{
        printf("Unrecognized disk %s appeared with type = %d\n", diskIdentifier, diskType);
        sleep(1);
        DiskArbClientWillHandleUnrecognizedDisk(diskIdentifier, 1);
        //sleep(10);
        //DiskArbClientHandlesUnrecognizedDisks(0,0);
        //DiskArbUpdateClientFlags();
        //DiskArbClientWillHandleUnrecognizedDisk(diskIdentifier, 0);

}

void UnknownFS_callback(					DiskArbDiskIdentifier diskIdentifier,
                        char *fsType,
                        char *deviceType,
                        int isWritable,
                        int isRemovable,
                        int isWhole)
{
        printf("*** Unrecognized disk appeared on %s ***\n", diskIdentifier);
}

void DiskWillbeChecked_callback(					DiskArbDiskIdentifier diskIdentifier,
                        int flags,
                        char *ioContent)
{
        printf("*** Disk Will Be Checked  %s, %d, %s ***\n", diskIdentifier, flags, ioContent);
}

void CallFailed_callback(					DiskArbDiskIdentifier diskIdentifier,
                        int type,
                        int error)
{
        printf("*** Call Failed  %s, %d, %d ***\n", diskIdentifier, type, error);
}

void reserveCallback(
                     DiskArbDiskIdentifier diskIdentifier,
                     unsigned status,
                     unsigned pid)
{
        printf("Woohoo %d\n", status); fflush(stdout);

        if (status == kDiskArbDeviceReservationRefused) {
                printf("Darn it %d refused me\n", pid); fflush(stdout);
                exit(0);
        }
        if (status == kDiskArbDeviceIsReserved) {
                printf("Darn it %s is reserved by %d\n", diskIdentifier, pid); fflush(stdout);
                exit(0);
        }

        if (status == kDiskArbDeviceIsNotReserved) {
                printf("it's free\n"); fflush(stdout);
                exit(0);
        }

        if (status == kDiskArbDeviceReservationObtained) {
                printf("Yeah - I've got it - now I hold it\n"); fflush(stdout);
        }

}

void willRelease(DiskArbDiskIdentifier diskIdentifier, int releaseToClientPid)
{
        printf("Someone (%d) has asked me to give up my reserve on %s\n", releaseToClientPid, diskIdentifier); fflush(stdout);
        DiskArbClientRelinquishesReservation(diskIdentifier, releaseToClientPid, 1);
}


void DiskArbDiskAppeared2_callback (
	DiskArbDiskIdentifier diskIdentifier,
	unsigned flags,
	DiskArbMountpoint mountpoint,
	DiskArbIOContent ioContent,
	DiskArbDeviceTreePath deviceTreePath,
	unsigned sequenceNumber )
{
        printf("***DiskAppeared2('%s',0x%08x,'%s')\n", (char *)diskIdentifier, flags, (char *)mountpoint ); fflush(stdout);
	return;
}

void DiskArbUnmountPreNotification_callback (DiskArbDiskIdentifier diskIdentifier,unsigned flags)
{
	kern_return_t err;

       	err = DiskArbUnmountPreNotifyAck_async_auto( diskIdentifier, refuse );
        //**DANGER  SHOULD NOT AUTORESPOND... THIS WILL CHANGE...
        printf("***Autoresponding yes to unmount - %s\n",diskIdentifier); fflush(stdout);
        
        return;
}


void DiskArbUnmountPostNotification_callback (
	DiskArbDiskIdentifier diskIdentifier,
	int errorCode,
	pid_t pid)
{
    if (!errorCode) {
            printf("***Disk Unmounted('%s')\n", (char *)diskIdentifier); fflush(stdout);
    } else {
            printf("***Disk NOT Unmounted('%s'), errorCode(%d), dissenter(%d)\n", (char *)diskIdentifier, errorCode, (int)pid); fflush(stdout);
    }
    
    if ((do_unmount || punmount) && !strcmp(device, diskIdentifier)) {
            printf( "Got end signal\n"); fflush(stdout);
        exit(0);
    }

}

/*
--
*/

void DiskArbEjectPreNotification_callback (
	DiskArbDiskIdentifier diskIdentifier,
	unsigned flags)
{
	kern_return_t err;
        printf("***Autoresponding yes to eject - %s\n",diskIdentifier); fflush(stdout);
        err = DiskArbEjectPreNotifyAck_async_auto( diskIdentifier, refuse);    
}

/* 
-- Client Death
*/

void DiskArbClientDisconnected_callback() 
{
        printf("***Your client was disconnected due to your inability to keep your end of the bargain\n");
        printf("***Restarting diskarbitration ....\n"); fflush(stdout);
        DiskArbStart( & diskArbitrationPort );

        return;
}        
/*
--
*/

void DiskArbEjectPostNotification_callback (
	DiskArbDiskIdentifier diskIdentifier,
	int errorCode,
	pid_t pid)
{
    if (!errorCode) {
            printf("***Disk Ejected('%s')\n", (char *)diskIdentifier); fflush(stdout);
    } else {
            printf("***Disk NOT Ejected('%s'), errorCode(%d), dissenter(%d)\n", (char *)diskIdentifier, errorCode, (int)pid); fflush(stdout);
    }
    
    if (eject && !strcmp(device, diskIdentifier)) {
            printf( "Got end signal\n"); fflush(stdout);
        exit(0);
    }


}

/*
--
*/

void DiskArbBlueBoxBootVolumeUpdated_callback (
	int seqno)
{
}

void DiskAppearedWithMountpoint_callback( DiskArbDiskIdentifier diskIdentifier, unsigned flags, DiskArbMountpoint mountpoint )
{
}

void DiskAppeared_callback(DiskArbDiskIdentifier diskIdentifier,
                           unsigned flags,
                           DiskArbMountpoint mountpoint,
                           DiskArbIOContent ioContent )
{
        printf( "***DiskAppeared('%s',0x%08x,'%s', '%s')\n", (char *)diskIdentifier, flags, (char *)mountpoint, (char *)ioContent); fflush(stdout);
}


void DiskUnmountNotification_callback(DiskArbDiskIdentifier diskIdentifier,
                                      int pastOrFuture,
                                      int willEject )
{
        printf( "***DiskUnmounted('%s', %d, %d)\n", (char *)diskIdentifier, pastOrFuture, willEject ); fflush(stdout);
}

void DiskCompleteNotification_callback()
{
        printf( "***DiskComplete!!\n"); fflush(stdout);
}

void DiskChanged_callback(DiskArbDiskIdentifier diskIdentifier,
                                      char * mountpoint,
                                      char * newVolumeName,
                                      int flags, 
                                      int success )
{
        printf( "***DiskChanged('%s', '%s', '%s', %d, Success = %d)\n", diskIdentifier, mountpoint, newVolumeName, flags, success);
        if (newname && !strcmp(device, diskIdentifier)) {
                printf( "Got end signal\n"); fflush(stdout);
            exit(0);
        }

}

void UnknownFileSystemInserted_callback( DiskArbDiskIdentifier diskIdentifier, int diskType, char *fsType, char *deviceType, int isWritable, int isRemovable, int isWhole)
{
        printf( "***Unknown Filesystem Inserted ('%s', '%s', '%s', %d, %d, %d)\n", (char *)diskIdentifier, fsType, deviceType, isWritable, isRemovable, isWhole); fflush(stdout);
}

void NotificationComplete_callback(int messageType)
{
        printf( "***Notifications Complete for type %d\n", messageType); fflush(stdout);
}

void DisplayHelp()
{
    printf( "disktool:  Disk Arbitration Command Tool\n");
    printf( "disktool:  disktool -rauempd deviceName [options]\n");
    printf( "Usage:  You can use disktool to refresh, eject, mount or unmount disks and volumes\n");
    printf( "Usage:  The acceptable command line parameters are\n");
    printf( "Usage:  \t-r -- Refresh Disk Arbitration (ex. disktool -r)\n");
    printf( "Usage:  \t-a -- Notify of mount.  Adds the disk to the Disk Arbitrations internal tables.  Useful when you have already forced the mount and want to let applications know it. (ex. disktool -a disk1 AFPVolName AFPFlags)\n");
    printf( "Usage:  \t-u -- Unmount a disk, the flags parameter is rarely used (ex. disktool -u disk2 0)\n");
    printf( "Usage:  \t-e -- Eject a disk, the flags parameter is rarely used (ex. disktool -e disk2 0)\n");
    printf( "Usage:  \t-m -- Mount a disk (ex. disktool -m disk2).  Useful when a disk has been unmounted using -p or -u above\n");
    printf( "Usage:  \t-o -- Open Vacant Drive Doors\n");
    printf( "Usage:  \t-p -- Unmount a partition, the flags parameter is rarely used (ex. disktool -p disk1s2 0)\n");
    printf( "Usage:  \t-d -- Notify of dismount.  Removes the disk from Disk Arbitrations internal tables.  Useful when you have already forced the unmount and want to let applications know it.  (ex. disktool -d disk1)\n");
    printf( "Usage:  \t-n -- Rename volume.  Renames the volume specified as the first argument..  (ex. disktool -n disk1s2 newName)\n");
    printf( "Usage:  \t-x -- Run and disallow ejects and unmounts .  Runs the disktool and refuses to allow volumes to unmount or eject. (ex. disktool -x)\n");
    printf( "Usage:  \t-y -- Run and allow ejects and unmounts .  Runs the disktool allows volumes to unmount or eject. (ex. disktool -y)\n");
    printf( "Usage:  \t-g -- Get the hfs encoding on a volume. (ex. disktool -g disk1s2)\n");
    printf( "Usage:  \t-s -- Set the hfs encoding on a volume. (ex. disktool -s disk1s2 4)\n");
    printf( "Usage:  \t-va -- Adopts the given device into the volinfo database.\n");
    printf( "Usage:  \t-vd -- Disowns the given device from the volinfo database.\n");
    printf( "Usage:  \t-vs -- Displays the status of the device in the volinfo database.\n");

    fflush(stdout);
    
    return;
}

int testDevice(char * devname)
{
        char deviceName[255];
        struct stat 	sb;

        sprintf(deviceName, "/dev/%s", devname);

        if (stat(deviceName, &sb) == 0) {
                return 1;
        } else {
                printf("Cannot find device %s\n", deviceName);
                return 0;
        }
}

int main (int argc, const char *argv[])
{
	kern_return_t			err;


	unsigned				msgReceiveBufLength;
	mach_msg_header_t	*	msgReceiveBufPtr;
	
	unsigned				msgSendBufLength;
	mach_msg_header_t	*	msgSendBufPtr;
	
	mach_msg_return_t		receiveResult;

        int			errorResult = 0;
        int			flags = 0;
        char			ch;
        int 			afpflags = 0;

        int vsdb = 0;
        int vsdbDisplay = 0;
        int vsdbAdopt = 0;
        int vsdbDisown = 0;
        int getEncoding = 0;
        int setEncoding = 0;
        int unrecognizedMessages = 0;
        int hideUnrecognizedMessages = 0;
        int testEject = 0;

        int retain = 0, test = 0, release = 0;
        
        if (argc < 2) {
            DisplayHelp();
            goto Exit;
        }

        //while ((ch = getopt(argc, argv, "rauempndxycvsgTREUV")) != -1)
        while ((ch = getopt(argc, argv, "rauempndxycvsgo")) != -1)
        {
            switch (ch)
            {
                case 'v':
                    vsdb = 1;
                    break;
                case 's':
                    if (vsdb) {
                        vsdbDisplay = 1;
                        device = (char *)argv[2];
                        if (!testDevice(device)) {
                                goto Exit;
                        }

                    } else {
                        setEncoding = 1;
                        device = (char *)argv[2];
                        if (!testDevice(device)) {
                                goto Exit;
                        }

                        flags = atoi(argv[3]);
                   }

                    break;
                case 'g':
                    getEncoding = 1;
                    device = (char *)argv[2];
                        if (!testDevice(device)) {
                                goto Exit;
                        }


                case 'r':
                    refresh = 1;
                    break;
                case 'R':
                        device = (char *)argv[2];
                    retain = 1;
                    break;
                case 'T':
                        device = (char *)argv[2];
                    test = 1;
                    break;
                case 'E':
                        device = (char *)argv[2];
                    release = 1;
                    break;
                case 'U':
                    unrecognizedMessages = 1;
                    flags = atoi(argv[2]);
                    break;
                case 'V':
                    hideUnrecognizedMessages = 1;
                    break;
                case 'u':
                    do_unmount = 1;
                    if (argc < 4) {
                            (void) printf("disktool: missing device for unmount\n Usage: disktool -u device flags\n"); fflush(stdout);
                        goto Exit;
                    } else {
                        device = (char *)argv[2];
                            if (!testDevice(device)) {
                                    goto Exit;
                            }

                        flags = atoi(argv[3]);
                    }
                    break;
                case 'p':
                    punmount = 1;
                    if (argc < 4) {
                            (void) printf("disktool: missing device for partition unmount\n Usage: disktool -p device flags\n"); fflush(stdout);
                        goto Exit;
                    } else {
                        device = (char *)argv[2];
                            if (!testDevice(device)) {
                                    goto Exit;
                            }

                        flags = kDiskArbUnmountOneFlag;
                    }

                    break;
                case 'e':
                    eject = 1;
                    if (argc < 4) {
                            (void) printf("disktool: missing device for eject\n Usage: disktool -e device flags\n"); fflush(stdout);
                        goto Exit;
                    } else {
                        device = (char *)argv[2];
                            if (!testDevice(device)) {
                                    goto Exit;
                            }

                        flags = atoi(argv[3]);
                    }
                    break;
                case 'a':

                    if (vsdb) {
                        vsdbAdopt = 1;
                        device = (char *)argv[2];
                        if (!testDevice(device)) {
                                goto Exit;
                        }


                        break;
                    }
                    
                    afpmount = 1;
                    if (argc < 5) {
                            (void) printf("disktool: missing device, mountpoint or flags for afp mount\n AFP Usage: disktool -a device mountpoint flags\n"); fflush(stdout);
                        goto Exit;
                    } else {
                        device = (char *)argv[2];
                            if (!testDevice(device)) {
                                    goto Exit;
                            }

                        mountpoint = (char *)argv[3];
                        afpflags = atoi(argv[4]);
                    }
                    break;
                case 'd':

                    if (vsdb) {
                        vsdbDisown = 1;
                        device = (char *)argv[2];
                        if (!testDevice(device)) {
                                goto Exit;
                        }


                        break;
                    }

                    afpdismount = 1;
                    if (argc < 3) {
                            (void) printf("disktool: missing device for afp dismount\n AFP Usage: disktool -d device\n"); fflush(stdout);
                        goto Exit;
                    } else {
                        device = (char *)argv[2];
                            if (!testDevice(device)) {
                                    goto Exit;
                            }

                    }
                    break;
                case 'm':
                    do_mount = 1;
                    if (argc < 3) {
                            (void) printf("disktool: missing device for mount\n Usage: disktool -m device\n"); fflush(stdout);
                        goto Exit;
                    } else {
                        device = (char *)argv[2];
                            if (!testDevice(device)) {
                                    goto Exit;
                            }

                    }
                    break;
                case 'n':
                    newname = 1;
                    if (argc < 4) {
                            (void) printf("disktool: missing device or name for rename\n Usage: disktool -n device newName\n"); fflush(stdout);
                        goto Exit;
                    } else {
                        device = (char *)argv[2];
                        if (!testDevice(device)) {
                                goto Exit;
                        }
                        mountpoint = (char *)argv[3];
                    }
                    break;
                case 'o':
                    testEject = 1;
                    break;
                case 'x':
                    refuse = 1;
                    break;
                case 'y':
                    refuse = 0;
                    break;
                case 'c':
                    changeUser = 1;
                    if (argc < 3) {
                            (void) printf("disktool: missing user id\n Usage: disktool -c uid\n"); fflush(stdout);
                        goto Exit;
                    } else {
                        newUser = atoi(argv[2]);
                    }

                    break;

                default:

                    
                break;
            }
        }

        /* Connect to the server.  Framework allocates a port upon which messages will arrive from the server. */
        err = DiskArbStart( & diskArbitrationPort );
        if ( err != KERN_SUCCESS )
        {
                printf( "FAILURE: DiskArbStart() -> %d\n", err ); fflush(stdout);
                goto Exit;
        }

        
        if (!do_unmount && !eject && !punmount && !vsdb && !getEncoding && !setEncoding && !retain && !test && !release && !unrecognizedMessages && !testEject) {

            /* Register for all callbacks and just run */
                DiskArbRegisterCallback_DiskAppeared2( DiskArbDiskAppeared2_callback );
                DiskArbRegisterCallback_UnmountPreNotification( DiskArbUnmountPreNotification_callback );
                DiskArbRegisterCallback_UnmountPostNotification( DiskArbUnmountPostNotification_callback );
                DiskArbRegisterCallback_EjectPreNotification( DiskArbEjectPreNotification_callback );
                DiskArbRegisterCallback_EjectPostNotification( DiskArbEjectPostNotification_callback );
                DiskArbRegisterCallback_ClientDisconnectedNotification( DiskArbClientDisconnected_callback );
                DiskArbRegisterCallback_DiskChangedNotification( DiskChanged_callback );
                DiskArbRegisterCallback_NotificationComplete( NotificationComplete_callback );
                DiskArbRegisterCallback_UnknownFileSystemNotification( UnknownFS_callback );
                DiskArbRegisterCallback_DiskWillBeCheckedNotification( DiskWillbeChecked_callback );
                DiskArbRegisterCallback_CallFailedNotification( CallFailed_callback );
        }
        
        if (do_unmount || punmount || eject) {
                DiskArbRegisterCallback_UnmountPreNotification( DiskArbUnmountPreNotification_callback );
                DiskArbRegisterCallback_UnmountPostNotification( DiskArbUnmountPostNotification_callback );
                DiskArbRegisterCallback_ClientDisconnectedNotification( DiskArbClientDisconnected_callback );
                DiskArbRegisterCallback_NotificationComplete( NotificationComplete_callback );

        	DiskArbRegisterCallback_EjectPreNotification( DiskArbEjectPreNotification_callback );
                DiskArbRegisterCallback_EjectPostNotification( DiskArbEjectPostNotification_callback );
                DiskArbRegisterCallback_CallFailedNotification( CallFailed_callback );

        }

        DiskArbUpdateClientFlags();

        if (hideUnrecognizedMessages) {
        	DiskArbClientHandlesUninitializedDisks_auto(1);
        }

	/* Debugging */


	msgSendBufLength = sizeof( mach_msg_empty_send_t ) + 20; /* Over-allocate */
	msgSendBufPtr = (mach_msg_header_t * )malloc( msgSendBufLength );
	if ( msgSendBufPtr == NULL )
	{
                printf( "FAILURE: msgSendBufPtr = malloc(%d)\n", msgSendBufLength ); fflush(stdout);
		goto Exit;
	}
		
	msgReceiveBufLength = sizeof( mach_msg_empty_rcv_t );
	msgReceiveBufPtr = NULL;

        if (setEncoding) {
                DiskArbSetVolumeEncoding_auto(device, flags);
                getEncoding = 1;
        }

        if (getEncoding) {
                int foo = DiskArbGetVolumeEncoding_auto(device);
                printf("The volume encoding is %d\n", foo); fflush(stdout);
                goto Exit;

        }


        if (testEject) {
                DiskArb_EjectKeyPressed();
                goto Exit;
        }

        if (changeUser) {
                printf("Changing the current user to %d\n", newUser); fflush(stdout);
            DiskArbSetCurrentUser_auto(newUser);
            goto Exit;

        }

        if (vsdbDisplay && device) {
                printf("Displaying vsdb device %s.  permissions = %d\n", device, DiskArbVSDBGetVolumeStatus_auto(device)); fflush(stdout);
            goto Exit;
        }

        if (vsdbAdopt && device) {
                printf("Adopting vsdb device %s\n", device); fflush(stdout);
            DiskArbVSDBAdoptVolume_auto(device);
            goto Exit;
        }

        if (vsdbDisown && device) {
                printf("Disowning vsdb device %s\n", device); fflush(stdout);
            DiskArbVSDBDisownVolume_auto(device);
            goto Exit;
        }
        
        if (refresh) {
                printf("Refreshing autodiskmounter ...\n"); fflush(stdout);
            DiskArbRefresh_auto(&errorResult);
            printf("Returning from autodiskmounter ...\n"); fflush(stdout);

            goto Exit;
        }

        if (retain) {
                DiskArbRetainClientReservationForDevice(device);
                DiskArbAddCallbackHandler(kDA_DEVICE_RESERVATION_STATUS, reserveCallback, 0);
                DiskArbAddCallbackHandler(kDA_WILL_CLIENT_RELEASE_DEVICE, willRelease, 0);

        }

        if (test) {
                DiskArbIsDeviceReservedForClient(device);
                DiskArbAddCallbackHandler(kDA_DEVICE_RESERVATION_STATUS, reserveCallback, 0);
                DiskArbAddCallbackHandler(kDA_WILL_CLIENT_RELEASE_DEVICE, willRelease, 0);
        }

        if (release) {
                DiskArbReleaseClientReservationForDevice(device);
                goto Exit;
        }

        if (unrecognizedMessages) {
                printf("Waiting on unrecognized disks of all types with priority %d\n", flags);
                DiskArbClientHandlesUnrecognizedDisks(kDiskArbHandlesAllUnrecognizedOrUninitializedMedia , flags);
                DiskArbAddCallbackHandler(kDA_CLIENT_WILL_HANDLE_UNRECOGNIZED_DISK, unrecognizedCallback, 0);
        }


        if (do_unmount && device) {
                printf("%s device will be unmounted ...\n", device); fflush(stdout);

            DiskArbUnmountRequest_async_auto( device, flags );

            goto Loop;
        }

        if (punmount && device) {
                printf("%s partition will attempt to be unmounted ...\n", device); fflush(stdout);

            DiskArbUnmountRequest_async_auto( device, flags );

            goto Loop;
        }


        if (eject && device) {
                printf("%s device will attempt to be ejected ...\n", device); fflush(stdout);
            
            DiskArbUnmountAndEjectRequest_async_auto( device, flags );
        
            goto Loop;
        }
        
        if (do_mount && device) {
                printf("%s device will attempt to be mounted ...\n", device); fflush(stdout);
            
            DiskArbRequestMount_auto( device );
        
            goto Loop;
        }

        if (afpmount && device) {
                printf("%s afp mount will attempt to be mounted ...\n", device); fflush(stdout);

            DiskArbDiskAppearedWithMountpointPing_auto(device, kDiskArbDiskAppearedNetworkDiskMask | kDiskArbDiskAppearedEjectableMask, mountpoint);

            goto Exit;
        }
        
        if (afpdismount && device) {
                printf("%s afp mount will attempt to be unmounted ...\n", device); fflush(stdout);

            DiskArbDiskDisappearedPing_auto(device, afpflags);

            goto Exit;
        }
        
        if (newname && device && mountpoint) {
                printf("Device %s will be attempt to renamed to %s ... \n", device, mountpoint); fflush(stdout);
            
            DiskArbRequestDiskChange_auto(device, mountpoint, 0);
        }
        
Loop:        
	while ( 1 )
	{
		/* (Re)allocate a buffer for receiving msgs from the server. */

		if ( msgReceiveBufPtr == NULL )
		{
			msgReceiveBufPtr = (mach_msg_header_t * )malloc( msgReceiveBufLength );
			if ( msgReceiveBufPtr == NULL )
			{
                                printf( "FAILURE: msgReceiveBufPtr = malloc(%d)\n", msgReceiveBufLength ); fflush(stdout);
				goto Exit;
			}
		}

		/* Message loop.  CFRunLoop? */
		
		//printf("Waiting for a message ...\n");
	
		receiveResult = mach_msg(	msgReceiveBufPtr,
									MACH_RCV_MSG | MACH_RCV_LARGE,
									0,
									msgReceiveBufLength,
									diskArbitrationPort,
									MACH_MSG_TIMEOUT_NONE,
									MACH_PORT_NULL);

		/* WARNING: clients are responsible for resizing/enlarging their msg receive buffer as necessary to accomodate the incoming server msgs */
		/* I assume CFRunLoop does this automagically */

		if ( receiveResult == MACH_RCV_TOO_LARGE )
		{
			//printf( "%s(): mach_msg: $%08x: %s\n", __FUNCTION__, receiveResult, mach_error_string(receiveResult) );
			//printf( "msgReceiveBufPtr->msgh_size = %d\n", msgReceiveBufPtr->msgh_size );
			msgReceiveBufLength = msgReceiveBufPtr->msgh_size + sizeof(mach_msg_trailer_t);
			free( msgReceiveBufPtr );
			msgReceiveBufPtr = NULL;
			/* Retry: reallocate a larger buffer and retry the msg_rcv */
			continue;
		}
		else
		if ( receiveResult != MACH_MSG_SUCCESS )
		{
                        printf( "%s(): mach_msg: $%08x: %s\n", __FUNCTION__, receiveResult, mach_error_string(receiveResult) ); fflush(stdout);
			goto Exit;
		}

		/* This multiplexing should be performed by CFRunLoop, although you may need an adapter function. */

		if ( msgReceiveBufPtr->msgh_local_port == diskArbitrationPort )
		{
			(void) DiskArbHandleMsg( msgReceiveBufPtr, msgSendBufPtr );
			(void) mach_msg_send( msgSendBufPtr );
		}
		else
		{
                        printf( "FAILURE: unrecognized msgh_local_port\n" ); fflush(stdout);
			goto Exit;
		}

	}


Exit:

	exit(0);       // insure the process exit status is 0
	return 0;      // ...and make main fit the ANSI spec.

}


