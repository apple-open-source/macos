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
#include <stdlib.h>
#include <bsd/unistd.h>

#include <sys/syslimits.h>
#include <mach/mach_error.h>

#include <DiskArbitration.h>

int			refresh = 0;	// refresh
int			unmount = 0;	// disk unmount
int			punmount = 0;	// partition unmount
int			eject = 0;	// disk eject
int			afpmount = 0;	// afp mount requested
int			afpdismount = 0;	// afp dismount requested
int			mount = 0;	// mounts or remounts a volume
int			newname = 0;	// mounts or remounts a volume
int			refuse = 0; // refuse ejects?

char			*device;
char			*mountpoint;

int 			changeUser = 0;
int 			newUser = -1;


mach_port_t		diskArbitrationPort;




void DiskArbDiskAppeared2_callback (
	DiskArbDiskIdentifier diskIdentifier,
	unsigned flags,
	DiskArbMountpoint mountpoint,
	DiskArbIOContent ioContent,
	DiskArbDeviceTreePath deviceTreePath,
	unsigned sequenceNumber )
{
	printf("***DiskAppeared2('%s',0x%08x,'%s')\n", (char *)diskIdentifier, flags, (char *)mountpoint );
	return;
}

void DiskArbUnmountPreNotification_callback (DiskArbDiskIdentifier diskIdentifier,unsigned flags)
{
	kern_return_t err;

       	err = DiskArbUnmountPreNotifyAck_async_auto( diskIdentifier, refuse );
        //**DANGER  SHOULD NOT AUTORESPOND...THIS WILL CHANGE...
        printf("***Autoresponding yes to unmount - %s\n",diskIdentifier);
        
        return;
}


void DiskArbUnmountPostNotification_callback (
	DiskArbDiskIdentifier diskIdentifier,
	int errorCode,
	pid_t pid)
{
    if (!errorCode) {
	printf("***Disk Unmounted('%s')\n", (char *)diskIdentifier);
    } else {
	printf("***Disk NOT Unmounted('%s'), errorCode(%d), dissenter(%d)\n", (char *)diskIdentifier, errorCode, (int)pid);
    }
    
    if ((unmount || punmount) && !strcmp(device, diskIdentifier)) {
        printf( "Got end signal\n");
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
        printf("***Autoresponding yes to eject - %s\n",diskIdentifier);
        err = DiskArbEjectPreNotifyAck_async_auto( diskIdentifier, refuse);    
}

/* 
-- Client Death
*/

void DiskArbClientDisconnected_callback() 
{
        printf("***Your client was disconnected due to your inability to keep your end of the bargain\n");
        printf("***Restarting diskarbitration ....\n");
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
	printf("***Disk Ejected('%s')\n", (char *)diskIdentifier);
    } else {
	printf("***Disk NOT Ejected('%s'), errorCode(%d), dissenter(%d)\n", (char *)diskIdentifier, errorCode, (int)pid);
    }
    
    if (eject && !strcmp(device, diskIdentifier)) {
        printf( "Got end signal\n");
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
	printf( "***DiskAppeared('%s',0x%08x,'%s', '%s')\n", (char *)diskIdentifier, flags, (char *)mountpoint, (char *)ioContent);
}


void DiskUnmountNotification_callback(DiskArbDiskIdentifier diskIdentifier,
                                      int pastOrFuture,
                                      int willEject )
{
        printf( "***DiskUnmounted('%s', %d, %d)\n", (char *)diskIdentifier, pastOrFuture, willEject );
}

void DiskCompleteNotification_callback()
{
        printf( "***DiskComplete!!\n");
}

void DiskChanged_callback(DiskArbDiskIdentifier diskIdentifier,
                                      char * mountpoint,
                                      char * newVolumeName,
                                      int flags, 
                                      int success )
{
        printf( "***DiskChanged('%s', '%s', '%s', %d, Success = %d)\n", diskIdentifier, mountpoint, newVolumeName, flags, success);
        if (newname && !strcmp(device, diskIdentifier)) {
            printf( "Got end signal\n");
            exit(0);
        }

}

void UnknownFileSystemInserted_callback( DiskArbDiskIdentifier diskIdentifier, char *fsType, char *deviceType, int isWritable, int isRemovable, int isWhole)
{
    printf( "***Unknown Filesystem Inserted ('%s', '%s', '%s', %d, %d, %d)\n", (char *)diskIdentifier, fsType, deviceType, isWritable, isRemovable, isWhole);
}

void NotificationComplete_callback(int messageType)
{
    printf( "***Notifcations Complete for type %d\n", messageType);
}

void DisplayHelp()
{
    printf( "disktool:  Disk Arbitration Command Tool\n");
    printf( "disktool:  disktool -rauempd deviceName [options]\n");
    printf( "Usage:  You can use disktool to refresh, eject, mount or unmount disks and volumes\n");
    printf( "Usage:  The acceptable command line parameters are\n");
    printf( "Usage:  \t-r -- Refresh Disk Arbitration (ex. disktool -r)\n");
    printf( "Usage:  \t-a -- AFP mount.  Adds the disk to the Disk Arbitrations internal tables.  Useful when you have already forced the mount and want to let applications know it. (ex. disktool -a disk1 AFPVolName AFPFlags)\n");
    printf( "Usage:  \t-u -- Unmount a disk, the flags parameter is rarely used (ex. disktool -u disk2 0)\n");
    printf( "Usage:  \t-e -- Eject a disk, the flags parameter is rarely used (ex. disktool -e disk2 0)\n");
    printf( "Usage:  \t-m -- Mount a disk (ex. disktool -m disk2).  Useful when a disk has been unmounted using -p or -u above\n");
    printf( "Usage:  \t-p -- Unmount a partition, the flags parameter is rarely used (ex. disktool -p disk1s2 0)\n");
    printf( "Usage:  \t-d -- AFP dismount.  Removes the disk from Disk Arbitrations internal tables.  Useful when you have already forced the unmount and want to let applications know it.  (ex. disktool -d disk1)\n");
    printf( "Usage:  \t-n -- rename volume.  Renames the volume specified as the first argument..  (ex. disktool -n disk1s2 newName)\n");
    printf( "Usage:  \t-x -- Run and disallow ejects and unmounts .  Runs the disktool and refuses to allow volumes to unmount or eject. (ex. disktool -x)\n");
    printf( "Usage:  \t-y -- Run and allow ejects and unmounts .  Runs the disktool allows volumes to unmount or eject. (ex. disktool -x)\n");
    printf( "Usage:  \t-c -- Change the uid of the currently logged in user.\n");
    printf( "Usage:  \t-g -- Get the hfs encoding on a volume. (ex. disktool -g disk1s2)\n");
    printf( "Usage:  \t-s -- Set the hfs encoding on a volume. (ex. disktool -s disk1s2 4)\n");
    printf( "Usage:  \t-va -- Adopts the given device into the vsdb database.\n");
    printf( "Usage:  \t-vd -- Disowns the given device from the vsdb database.\n");
    printf( "Usage:  \t-vs -- Displays the status of the device in the vsdb database.\n");

    
    return;
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
        
        if (argc < 2) {
            DisplayHelp();
            goto Exit;
        }

        while ((ch = getopt(argc, argv, "rauempndxycvsg")) != -1)
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
                    } else {
                        setEncoding = 1;
                        device = (char *)argv[2];
                        flags = atoi(argv[3]);
                   }

                    break;
                case 'g':
                    getEncoding = 1;
                    device = (char *)argv[2];
                    
                case 'r':
                    refresh = 1;
                    break;
                case 'u':
                    unmount = 1;
                    if (argc < 4) {
                        (void) printf("disktool: missing device for unmount\n Usage: disktool -u device flags\n");
                        goto Exit;
                    } else {
                        device = (char *)argv[2];
                        flags = atoi(argv[3]);
                    }
                    break;
                case 'p':
                    punmount = 1;
                    if (argc < 4) {
                        (void) printf("disktool: missing device for partition unmount\n Usage: disktool -p device flags\n");
                        goto Exit;
                    } else {
                        device = (char *)argv[2];
                        flags = kDiskArbUnmountOneFlag;
                    }

                    break;
                case 'e':
                    eject = 1;
                    if (argc < 4) {
                        (void) printf("disktool: missing device for eject\n Usage: disktool -e device flags\n");
                        goto Exit;
                    } else {
                        device = (char *)argv[2];
                        flags = atoi(argv[3]);
                    }
                    break;
                case 'a':

                    if (vsdb) {
                        vsdbAdopt = 1;
                        device = (char *)argv[2];

                        break;
                    }
                    
                    afpmount = 1;
                    if (argc < 5) {
                        (void) printf("disktool: missing device, mountpoint or flags for afp mount\n AFP Usage: disktool -a device mountpoint flags\n");
                        goto Exit;
                    } else {
                        device = (char *)argv[2];
                        mountpoint = (char *)argv[3];
                        afpflags = atoi(argv[4]);
                    }
                    break;
                case 'd':

                    if (vsdb) {
                        vsdbDisown = 1;
                        device = (char *)argv[2];
                        break;
                    }

                    afpdismount = 1;
                    if (argc < 3) {
                        (void) printf("disktool: missing device for afp dismount\n AFP Usage: disktool -d device\n");
                        goto Exit;
                    } else {
                        device = (char *)argv[2];
                    }
                    break;
                case 'm':
                    mount = 1;
                    if (argc < 3) {
                        (void) printf("disktool: missing device for mount\n Usage: disktool -m device\n");
                        goto Exit;
                    } else {
                        device = (char *)argv[2];
                    }
                    break;
                case 'n':
                    newname = 1;
                    if (argc < 4) {
                        (void) printf("disktool: missing device or name for rename\n Usage: disktool -n device newName\n");
                        goto Exit;
                    } else {
                        device = (char *)argv[2];
                        mountpoint = (char *)argv[3];
                    }
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
                        (void) printf("disktool: missing user id\n Usage: disktool -c uid\n");
                        goto Exit;
                    } else {
                        newUser = atoi(argv[2]);
                    }

                    break;

                default:

                    
                break;
            }
        }

        if (!unmount && !eject && !punmount && !vsdb && !getEncoding && !setEncoding) {

            /* Register for all callbacks and just run */
                DiskArbRegisterCallback_DiskAppeared2( DiskArbDiskAppeared2_callback );
                DiskArbRegisterCallback_UnmountPreNotification( DiskArbUnmountPreNotification_callback );
                DiskArbRegisterCallback_UnmountPostNotification( DiskArbUnmountPostNotification_callback );
                DiskArbRegisterCallback_EjectPreNotification( DiskArbEjectPreNotification_callback );
                DiskArbRegisterCallback_EjectPostNotification( DiskArbEjectPostNotification_callback );
                DiskArbRegisterCallback_ClientDisconnectedNotification( DiskArbClientDisconnected_callback );
                DiskArbRegisterCallback_UnknownFileSystemNotification( UnknownFileSystemInserted_callback );
                DiskArbRegisterCallback_DiskChangedNotification( DiskChanged_callback );
                DiskArbRegisterCallback_NotificationComplete( NotificationComplete_callback );
                //DiskArbRegisterCallback_DiskAppearedWithMountpoint(NULL);
                //DiskArbRegisterCallback_DiskAppearedWithoutMountpoint(NULL);
                //DiskArbRegisterCallback_DiskAppeared(NULL);
                //DiskArbRegisterCallback_UnmountNotification(NULL);
                //DiskArbRegisterCallback_BlueBoxBootVolumeUpdated(NULL);

            //DiskArbRegisterCallback_CompleteNotification( (DiskArbCallback_CompleteNotification_t) & DiskCompleteNotification_callback );
        }
        
        if (unmount || punmount || eject) {
                DiskArbRegisterCallback_UnmountPreNotification( DiskArbUnmountPreNotification_callback );
                DiskArbRegisterCallback_UnmountPostNotification( DiskArbUnmountPostNotification_callback );
                DiskArbRegisterCallback_ClientDisconnectedNotification( DiskArbClientDisconnected_callback );
                DiskArbRegisterCallback_NotificationComplete( NotificationComplete_callback );

        	DiskArbRegisterCallback_EjectPreNotification( DiskArbEjectPreNotification_callback );
                DiskArbRegisterCallback_EjectPostNotification( DiskArbEjectPostNotification_callback );

        }

	/* Connect to the server.  Framework allocates a port upon which messages will arrive from the server. */

	err = DiskArbStart( & diskArbitrationPort );
	if ( err != KERN_SUCCESS )
	{
		printf( "FAILURE: DiskArbStart() -> %d\n", err );
		goto Exit;
	}
	else
	{
		printf( "SUCCESS: DiskArbStart()\n" );
	}

	/* Debugging */

	printf( "sizeof( mach_msg_header_t ) = %ld\n", sizeof( mach_msg_header_t ) );
	printf( "sizeof( mach_msg_trailer_t ) = %ld\n", sizeof( mach_msg_trailer_t ) );
	printf( "sizeof( mach_msg_empty_rcv_t ) = %ld\n", sizeof( mach_msg_empty_rcv_t ) );

	msgSendBufLength = sizeof( mach_msg_empty_send_t ) + 20; /* Over-allocate */
	msgSendBufPtr = (mach_msg_header_t * )malloc( msgSendBufLength );
	if ( msgSendBufPtr == NULL )
	{
		printf( "FAILURE: msgSendBufPtr = malloc(%d)\n", msgSendBufLength );
		goto Exit;
	}
	else
	{
		printf( "SUCCESS: msgSendBufPtr = malloc(%d)\n", msgSendBufLength );
	}
		
	msgReceiveBufLength = sizeof( mach_msg_empty_rcv_t );
	msgReceiveBufPtr = NULL;

        if (setEncoding) {
                DiskArbSetVolumeEncoding_auto(device, flags);
                getEncoding = 1;
        }

        if (getEncoding) {
                int foo = DiskArbGetVolumeEncoding_auto(device);
                printf("The volume encoding is %d\n", foo);
                goto Exit;

        }

        if (changeUser) {
            printf("Changing the current user to %d\n", newUser);
            DiskArbSetCurrentUser_auto(newUser);
            goto Exit;

        }

        if (vsdbDisplay && device) {
            printf("Displaying vsdb device %s.  permissions = %d\n", device, DiskArbVSDBGetVolumeStatus_auto(device));
            goto Exit;
        }

        if (vsdbAdopt && device) {
            printf("Adopting vsdb device %s\n", device);
            DiskArbVSDBAdoptVolume_auto(device);
            goto Exit;
        }

        if (vsdbDisown && device) {
            printf("Disowning vsdb device %s\n", device);
            DiskArbVSDBDisownVolume_auto(device);
            goto Exit;
        }
        
        if (refresh) {
            printf("Refreshing autodiskmounter ...\n");
            DiskArbRefresh_auto(&errorResult);
            printf("Returning from autodiskmounter ...\n");

            goto Exit;
        }

        if (unmount && device) {
            printf("%s device will be unmounted ...\n", device);

            DiskArbUnmountRequest_async_auto( device, flags );

            goto Loop;
        }

        if (punmount && device) {
            printf("%s partition will be unmounted ...\n", device);

            DiskArbUnmountRequest_async_auto( device, flags );

            goto Loop;
        }


        if (eject && device) {
            printf("%s device will be ejected ...\n", device);
            
            DiskArbUnmountAndEjectRequest_async_auto( device, flags );
        
            goto Loop;
        }
        
        if (mount && device) {
            printf("%s device will be mounted ...\n", device);
            
            DiskArbRequestMount_auto( device );
        
            goto Loop;
        }

        if (afpmount && device) {
            printf("%s afp mount will be mounted ...\n", device);

            DiskArbDiskAppearedWithMountpointPing_auto(device, kDiskArbDiskAppearedNetworkDiskMask | kDiskArbDiskAppearedEjectableMask, mountpoint);

            goto Exit;
        }
        
        if (afpdismount && device) {
            printf("%s afp mount will be unmounted ...\n", device);

            DiskArbDiskDisappearedPing_auto(device, afpflags);

            goto Exit;
        }
        
        if (newname && device && mountpoint) {
            printf("Device %s will be renamed to %s ... \n", device, mountpoint);
            
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
				printf( "FAILURE: msgReceiveBufPtr = malloc(%d)\n", msgReceiveBufLength );
				goto Exit;
			}
			else
			{
				printf( "SUCCESS: msgReceiveBufPtr = malloc(%d)\n", msgReceiveBufLength );
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
			printf( "%s(): mach_msg: $%08x: %s\n", __FUNCTION__, receiveResult, mach_error_string(receiveResult) );
			printf( "msgReceiveBufPtr->msgh_size = %d\n", msgReceiveBufPtr->msgh_size );
			msgReceiveBufLength = msgReceiveBufPtr->msgh_size + sizeof(mach_msg_trailer_t);
			free( msgReceiveBufPtr );
			msgReceiveBufPtr = NULL;
			/* Retry: reallocate a larger buffer and retry the msg_rcv */
			continue;
		}
		else
		if ( receiveResult != MACH_MSG_SUCCESS )
		{
			printf( "%s(): mach_msg: $%08x: %s\n", __FUNCTION__, receiveResult, mach_error_string(receiveResult) );
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
			printf( "FAILURE: unrecognized msgh_local_port\n" );
			goto Exit;
		}

	}


Exit:

	exit(0);       // insure the process exit status is 0
	return 0;      // ...and make main fit the ANSI spec.

}


