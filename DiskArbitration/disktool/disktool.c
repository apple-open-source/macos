/*
 * Copyright (c) 1998-2009 Apple Inc. All Rights Reserved.
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

#include <stdio.h>
#include <stddef.h>
#include <libc.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/disk.h>
#include <errno.h>
#include <sys/param.h>
#include <unistd.h>

#include <fts.h>


#include <sys/syslimits.h>
#include <mach/mach_error.h>

#include "DiskArbitrationPrivate.h"
#include <CoreFoundation/CoreFoundation.h>

// refresh
int             refresh = 0;
//disk unmount
int             do_unmount = 0;
//partition unmount
int             punmount = 0;
//disk eject
int             eject = 0;
//afp mount requested
int             afpmount = 0;
//afp dismount requested
int             afpdismount = 0;
//mounts or remounts a volume
int             do_mount = 0;
//mounts or remounts a volume
int             newname = 0;
//refuse ejects ?
int             refuse = 0;
//refuse Mounts ?
int             refuseMounts = 0;
//just list the current disks and quit
int             list = 0;

char           *device;
char           *mountpoint;

mach_port_t     diskArbitrationPort;
mach_msg_header_t *    msgSendBufPtr;


void
MyHandleMachMessage ( CFMachPortRef port, void * msg, CFIndex size, void * info )
{
      unsigned long                   result = 0xFFFFFFFF;

       mach_msg_header_t *             receivedMessage;

       receivedMessage = ( mach_msg_header_t * ) msg;

       // Call DiskArbHandleMsg to change mig-generated message into a
       // genuine mach message. It will then cause the callback to get called.
       result = DiskArbHandleMsg ( receivedMessage, msgSendBufPtr );
       ( void ) mach_msg_send ( msgSendBufPtr );
}

void
DisplayHelp()
{
	printf("disktool:  Disk Arbitration Command Tool\n");
	printf("disktool:  disktool -[r][a][u][e][m][p][n][d][s][g][A][S][D][l]\n");
	printf("                deviceName [options]\n");
	printf("You can use disktool to rename, eject, mount or unmount disks and volumes\n");
	printf("The acceptable command line parameters are:\n");
	printf("\nInformation about disks:\n");
	printf("\tUsage:  -l -- List Disks.  Lists the disks currently known and available\n");
	printf("\t              on the system.\n");
	printf("\nControlling arbitration:\n");
	//printf("\tUsage:  -x -- Run and disallow ejects and unmounts .  Runs the disktool\n");
	//printf("\t               and refuses to allow volumes to unmount or eject.\n");
	//printf("\tUsage:  -y -- Run and allow ejects and unmounts .  Runs the disktool and\n");
	//printf("\t               allows volumes to unmount or eject.\n");
	printf("\tUsage:  -r -- Refresh Disk Arbitration.  Causes arbitration to refresh\n");
	printf("\t               its internal tables and look for new mounts/unmounts.\n");
	printf("\nManaging disks:\n");
	printf("\tUsage:  -u -- Unmount a disk (ex. disktool -u disk2)\n");
	printf("\tUsage:  -p -- Unmount a partition (ex. disktool -p disk1s2)\n");
	printf("\tUsage:  -e -- Eject a disk (ex. disktool -e disk2)\n");
	printf("\tUsage:  -m -- Mount a disk (ex. disktool -m disk2).  Useful when a disk\n");
	printf("\t               has been unmounted by hand (using -p or -u parameters\n");
	printf("\tUsage:  -a -- Notify of mount.  Adds the disk to the Disk Arbitrations\n");
	printf("\t               internal tables.\n");
	printf("\t               Useful when you have already forced the mount and want \n");
	printf("\t               to let applications know it.\n");
	printf("\t               (ex. disktool -a disk1 AFPVolName AFPFlags)\n");
	printf("\tUsage:  -d -- Notify of dismount.  Removes the disk from Disk\n");
	printf("\t               Arbitration's internal tables.\n");
	printf("\t               Useful when you have already forced the unmount and \n");
	printf("\t               want to let applications know it.\n");
	printf("\t              (ex. disktool -d disk1)\n");
	printf("\nControlling disk parameters:\n");
	printf("\tUsage:  -n -- Rename volume.  Renames the volume specified as the first\n");
	printf("\t               argument. (ex. disktool -n disk1s2 newName)\n");
	printf("\tUsage:  -g -- Get the hfs encoding on a volume.\n");
	printf("\t              (ex. disktool -g disk1s2)\n");
	printf("\tUsage:  -s -- Set the hfs encoding on a volume.\n");
	printf("\t              (ex. disktool -s disk1s2 4)\n");
	printf("\tUsage:  -A -- Adopts the given device into the volinfo database.\n");
	printf("\tUsage:  -D -- Disowns the given device from the volinfo database.\n");
	printf("\tUsage:  -S -- Displays the status of the device in the volinfo database.\n");

	fflush(stdout);

	return;
}

void
DiskApproval_callback(DiskArbDiskIdentifier diskIdentifier,
		      DiskArbMountpoint possibleMountpoint,
		      DiskArbGenericString ioContent,
		      DiskArbGenericString deviceTreePath,
		      int diskType,
		      int isWritable,
		      int isRemovable,
		      int isWhole,
              DiskArbGenericString fsType)
{
	printf("Disallowing disk %s to mount with media name %s.  Params:  %s, %s, %s\n", diskIdentifier, possibleMountpoint, (isWritable ? "Writable" : "Read Only"), (isRemovable ? "Removable" : "Fixed"), (isWhole ? "Whole Disk" : "Partition"));
    DiskArbDiskApprovedAck_auto(diskIdentifier, kDiskArbDisallowMounting );
}



void
unrecognizedCallback(DiskArbDiskIdentifier diskIdentifier, int diskType, char *fsType, char *deviceType, int isWritable, int isRemovable, int isWhole)
{
	printf("Unrecognized disk %s appeared with type = %d\n", diskIdentifier, diskType);
	sleep(1);
	DiskArbClientWillHandleUnrecognizedDisk(diskIdentifier, 1);
	//sleep(10);
	//DiskArbClientHandlesUnrecognizedDisks(0, 0);
	//DiskArbUpdateClientFlags();
	//DiskArbClientWillHandleUnrecognizedDisk(diskIdentifier, 0);

}

void
CallFailed_callback(DiskArbDiskIdentifier diskIdentifier,
		    int type,
		    int error)
{
	printf("*** Call Failed  %s, %d, %d ***\n", diskIdentifier, type, error);
	exit(0);
}

void
reserveCallback(
		DiskArbDiskIdentifier diskIdentifier,
		unsigned status,
		unsigned pid)
{
	printf("Woohoo %d\n", status);
	fflush(stdout);

	if (status == kDiskArbDeviceReservationRefused) {
		printf("Darn it %d refused me\n", pid);
		fflush(stdout);
		exit(0);
	}
	if (status == kDiskArbDeviceIsReserved) {
		printf("Darn it %s is reserved by %d\n", diskIdentifier, pid);
		fflush(stdout);
		exit(0);
	}
	if (status == kDiskArbDeviceIsNotReserved) {
		printf("it's free\n");
		fflush(stdout);
		exit(0);
	}
	if (status == kDiskArbDeviceReservationObtained) {
		printf("Yeah - I've got it - now I hold it\n");
		fflush(stdout);
	}
}

void
willRelease(DiskArbDiskIdentifier diskIdentifier, int releaseToClientPid)
{
	printf("Someone (%d) has asked me to give up my reserve on %s\n", releaseToClientPid, diskIdentifier);
	fflush(stdout);
	DiskArbClientRelinquishesReservation(diskIdentifier, releaseToClientPid, 1);
}


void
DiskAppearedComplete_callback(
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
    printf("***Disk Appeared ('%s',Mountpoint = '%s', fsType = '%s', volName = '%s')\n", (char *) diskIdentifier, (char *) mountpoint, (char *)fsType, (char *)fsName);
	fflush(stdout);

	if (do_mount && !strcmp(device, diskIdentifier)) {
		printf("Disk Mounting Completed\n");
		fflush(stdout);
		exit(0);
	}
	return;
}

void
UnmountPreNotification_callback(DiskArbDiskIdentifier diskIdentifier, unsigned flags)
{
	kern_return_t   err;

    err = DiskArbUnmountPreNotifyAck_async_auto(diskIdentifier, refuse);
    //err = DiskArbUnmountPreNotifyAck_async_auto(diskIdentifier, kDiskArbRequireAuthentication);
    printf("***Responding %s to unmount - %s\n", (refuse ? "no" : "yes"), diskIdentifier);
    fflush(stdout);

    return;
}

void
DiskApprovedAck_callback(DiskArbDiskIdentifier diskIdentifier, unsigned flags)
{
	kern_return_t   err;

	err = DiskArbDiskApprovedAck_auto(diskIdentifier, 1);

	return;
}

void
UnmountPostNotification_callback(
				 DiskArbDiskIdentifier diskIdentifier,
				 int errorCode,
				 pid_t pid)
{
	if (!errorCode) {
		printf("***Disk Unmounted('%s')\n", (char *) diskIdentifier);
		fflush(stdout);
	} else {
		printf("***Disk NOT Unmounted('%s'), errorCode(%d), dissenter(%d)\n", (char *) diskIdentifier, errorCode, (int) pid);
		fflush(stdout);
	}

	if ((do_unmount || punmount) && !strcmp(device, diskIdentifier)) {
		exit(0);
	}
}

/*
--
*/

void
EjectPreNotification_callback(
			      DiskArbDiskIdentifier diskIdentifier,
			      unsigned flags)
{
	kern_return_t   err;
	printf("***Responding %s to eject - %s\n", (refuse ? "no" : "yes"), diskIdentifier);
	fflush(stdout);
	err = DiskArbEjectPreNotifyAck_async_auto(diskIdentifier, refuse);
}

/*
--
*/

void
EjectPostNotification_callback(
			       DiskArbDiskIdentifier diskIdentifier,
			       int errorCode,
			       pid_t pid)
{
	if (!errorCode) {
		printf("***Disk Ejected('%s')\n", (char *) diskIdentifier);
		fflush(stdout);
	} else {
		printf("***Disk NOT Ejected('%s'), errorCode(%d), dissenter(%d)\n", (char *) diskIdentifier, errorCode, (int) pid);
		fflush(stdout);
	}

	if (eject && !strcmp(device, diskIdentifier)) {
		exit(0);
	}
}

/*
--
*/

void
DiskChanged_callback(DiskArbDiskIdentifier diskIdentifier,
		     char *mountpoint,
		     char *newVolumeName,
		     int flags,
		     int success)
{
	printf("***DiskChanged('%s', '%s', '%s', %d, Success = %d)\n", diskIdentifier, mountpoint, newVolumeName, flags, success);
	if (newname && !strcmp(device, diskIdentifier)) {
		printf("Got end signal\n");
		fflush(stdout);
		exit(0);
	}
}

void
NotificationComplete_callback(int messageType)
{
	if (list == 1 && messageType == 1) {
		exit(0);
	}
	printf("***Notifications Complete for type %d\n", messageType);
	fflush(stdout);
}


int
testDevice(char *devname)
{
	char            deviceName[255];
	struct stat     sb;

	if (strncmp(devname, "disk", strlen("disk"))) {
		return 1;
	}

    snprintf(deviceName, sizeof(deviceName), "/dev/%s", devname);
    
	if (stat(deviceName, &sb) == 0) {
		return 1;
	} else {
		printf("Cannot find device %s\n", deviceName);
		return 0;
	}
}

int
main(int argc, const char *argv[])
{
	kern_return_t   err;

    unsigned        msgSendBufLength;

	int             flags = 0;
	char            ch;
	int             afpflags = 0;

	int             vsdb = 0;
	int             vsdbDisplay = 0;
	int             vsdbAdopt = 0;
	int             vsdbDisown = 0;
	int             getEncoding = 0;
	int             setEncoding = 0;
	int             unrecognizedMessages = 0;
	int             hideUnrecognizedMessages = 0;

	int             retain = 0, test = 0, release = 0;
    CFMachPortRef           cfMachPort = 0;
    CFMachPortContext       context;
    CFRunLoopSourceRef      cfRunLoopSource;
    Boolean                         shouldFreeInfo;

    context.version                 = 1;
    context.info                    = 0;
    context.retain                  = NULL;
    context.release                 = NULL;
    context.copyDescription = NULL;

	if (argc < 2) {
		DisplayHelp();
		goto Exit;
	}
    //while ((ch = getopt(argc, argv, "hrauempndxysgASDlTREUVX")) != -1)
    while ((ch = getopt(argc, (char * const *)argv, "hrauempndxysgASDl")) != -1)
    {
        switch (ch) {
			case 'S':
				vsdbDisplay = 1;
				if (argc < 3) {
					(void) printf("disktool: missing device for status\n Usage: disktool -S device\n");
					fflush(stdout);
					goto Exit;
				} else {
					device = (char *) argv[2];
					if (!testDevice(device)) {
						goto Exit;
					}
				}
				break;
			case 's':
				setEncoding = 1;
				if (argc < 3) {
					(void) printf("disktool: missing device for encoding\n Usage: disktool -s device encoding\n");
					fflush(stdout);
					goto Exit;
				} else {
					device = (char *) argv[2];
					if (!testDevice(device)) {
						goto Exit;
					}
					if (argv[3]) {
						flags = atoi(argv[3]);
					} else {
						flags = 0;
					}
				}
				break;
			case 'g':
				getEncoding = 1;
				if (argc < 3) {
					(void) printf("disktool: missing device for encoding\n Usage: disktool -g device\n");
					fflush(stdout);
					goto Exit;
				} else {
					device = (char *) argv[2];
					if (!testDevice(device)) {
						goto Exit;
					}
				}
			case 'r':
				refresh = 1;
				break;
			case 'u':
				do_unmount = 1;
				if (argc < 3) {
					(void) printf("disktool: missing device for unmount\n Usage: disktool -u device flags\n");
					fflush(stdout);
					goto Exit;
				} else {
					device = (char *) argv[2];
					if (!testDevice(device)) {
						goto Exit;
					}
					if (argv[3]) {
						flags = atoi(argv[3]);
					} else {
						flags = 0;
					}

				}
				break;
			case 'p':
				punmount = 1;
				if (argc < 3) {
					(void) printf("disktool: missing device for partition unmount\n Usage: disktool -p device flags\n");
					fflush(stdout);
					goto Exit;
				} else {
					device = (char *) argv[2];
					if (!testDevice(device)) {
						goto Exit;
					}
					flags = kDiskArbUnmountOneFlag;
				}

				break;
			case 'e':
				eject = 1;
				if (argc < 3) {
					(void) printf("disktool: missing device for eject\n Usage: disktool -e device flags\n");
					fflush(stdout);
					goto Exit;
				} else {
					device = (char *) argv[2];
					if (!testDevice(device)) {
						goto Exit;
					}
					if (argv[3]) {
						flags = atoi(argv[3]);
					} else {
						flags = 0;
					}
				}
				break;

			case 'A':
				vsdbAdopt = 1;
				if (argc < 3) {
					(void) printf("disktool: missing device for adopt\n Usage: disktool -A device\n");
					fflush(stdout);
					goto Exit;
				} else {
					device = (char *) argv[2];
					if (!testDevice(device)) {
						goto Exit;
					}
				}
				break;
			case 'a':
				afpmount = 1;
				if (argc < 5) {
					(void) printf("disktool: missing device, mountpoint or flags for afp mount\n AFP Usage: disktool -a device mountpoint flags\n");
					fflush(stdout);
					goto Exit;
				} else {
					device = (char *) argv[2];
					if (!testDevice(device)) {
						goto Exit;
					}
					mountpoint = (char *) argv[3];
					afpflags = atoi(argv[4]);
				}
				break;
			case 'D':
				vsdbDisown = 1;
				if (argc < 3) {
					(void) printf("disktool: missing device for disown\n Usage: disktool -D device\n");
					fflush(stdout);
					goto Exit;
				} else {
					device = (char *) argv[2];
					if (!testDevice(device)) {
						goto Exit;
					}
				}
				break;
			case 'd':
				afpdismount = 1;
				if (argc < 3) {
					(void) printf("disktool: missing device for dismount\n Usage: disktool -d device\n");
					fflush(stdout);
					goto Exit;
				} else {
					device = (char *) argv[2];
					if (!testDevice(device)) {
						goto Exit;
					}
				}
				break;
			case 'm':
				do_mount = 1;
				if (argc < 3) {
					(void) printf("disktool: missing device for mount\n Usage: disktool -m device\n");
					fflush(stdout);
					goto Exit;
				} else {
					device = (char *) argv[2];
					if (!testDevice(device)) {
						goto Exit;
					}
				}
				break;
			case 'n':
				newname = 1;
				if (argc < 4) {
					(void) printf("disktool: missing device or name for rename\n Usage: disktool -n device newName\n");
					fflush(stdout);
					goto Exit;
				} else {
					device = (char *) argv[2];
					if (!testDevice(device)) {
						goto Exit;
					}
					mountpoint = (char *) argv[3];
				}
				break;
			case 'x':
				refuse = 1;
				break;
			case 'X':
				refuseMounts = 1;
				break;
			case 'y':
				refuse = 0;
				break;
			case 'l':
				refuse = 0;
				list = 1;
				break;
			case 'R':
				device = (char *) argv[2];
				retain = 1;
				break;
			case 'T':
				device = (char *) argv[2];
				test = 1;
				break;
			case 'E':
				device = (char *) argv[2];
				release = 1;
				break;
			case 'U':
				unrecognizedMessages = 1;
				flags = atoi(argv[2]);
				break;
			case 'V':
				hideUnrecognizedMessages = 1;
				break;
			case 'h':
				DisplayHelp();
				goto Exit;
				break;
			default:
				exit(1);
				break;
        }
    }

	/*
	 * Connect to the server.  Framework allocates a port upon which
	 * messages will arrive from the server.
	 */
	err = DiskArbStart(&diskArbitrationPort);
	if (err != KERN_SUCCESS) {
		printf("FAILURE: DiskArbStart() -> %d\n", err);
		fflush(stdout);
		goto Exit;
	}
	DiskArbRegisterCallback_CallFailedNotification(CallFailed_callback);

    if (refuseMounts) {
        DiskArbAddCallbackHandler(kDA_DISK_APPROVAL_NOTIFY, DiskApproval_callback, 0);
    }

	if (!do_unmount && !eject && !punmount && !vsdb && !getEncoding && !setEncoding && !retain && !test && !release && !unrecognizedMessages) {

		/* Register for all callbacks and just run */
		DiskArbRegisterCallback_UnmountPreNotification(UnmountPreNotification_callback);
		DiskArbRegisterCallback_UnmountPostNotification(UnmountPostNotification_callback);
		DiskArbRegisterCallback_EjectPreNotification(EjectPreNotification_callback);
		DiskArbRegisterCallback_EjectPostNotification(EjectPostNotification_callback);
		DiskArbRegisterCallback_DiskChangedNotification(DiskChanged_callback);
		DiskArbRegisterCallback_NotificationComplete(NotificationComplete_callback);
		DiskArbAddCallbackHandler(kDA_DISK_APPEARED_COMPLETE, DiskAppearedComplete_callback, 0);
	}
	if (do_unmount || punmount || eject) {
		DiskArbRegisterCallback_UnmountPreNotification(UnmountPreNotification_callback);
		DiskArbRegisterCallback_UnmountPostNotification(UnmountPostNotification_callback);
		DiskArbRegisterCallback_NotificationComplete(NotificationComplete_callback);

		DiskArbRegisterCallback_EjectPreNotification(EjectPreNotification_callback);
		DiskArbRegisterCallback_EjectPostNotification(EjectPostNotification_callback);
		DiskArbRegisterCallback_CallFailedNotification(CallFailed_callback);

	}
    
	DiskArbUpdateClientFlags();

	if (hideUnrecognizedMessages) {
		DiskArbClientHandlesUninitializedDisks_auto(1);
	}
	/* Debugging */

    msgSendBufLength = sizeof(mach_msg_empty_send_t) + 20;	/* Over-allocate */
	msgSendBufPtr = (mach_msg_header_t *) malloc(msgSendBufLength);
	if (msgSendBufPtr == NULL) {
		printf("FAILURE: msgSendBufPtr = malloc(%d)\n", msgSendBufLength);
		fflush(stdout);
		goto Exit;
	}
    cfMachPort = CFMachPortCreateWithPort ( kCFAllocatorDefault,diskArbitrationPort,( CFMachPortCallBack ) MyHandleMachMessage,&context,&shouldFreeInfo );

    if (!cfMachPort) {
        printf("FAILURE: could not create CFMachPort\n");
        goto Exit;
    }
    
    cfRunLoopSource = CFMachPortCreateRunLoopSource ( kCFAllocatorDefault, cfMachPort, 0 );

    if (!cfRunLoopSource) {
        printf("FAILURE: could not create CFRunLoopSource\n");
        goto Exit;
    }
    
    CFRunLoopAddSource ( CFRunLoopGetCurrent ( ), cfRunLoopSource, kCFRunLoopDefaultMode );

    CFRelease ( cfMachPort );
    CFRelease ( cfRunLoopSource );

	if (setEncoding) {
		DiskArbSetVolumeEncoding_auto(device, flags);
		getEncoding = 1;
	}
	if (getEncoding) {
		int             foo = DiskArbGetVolumeEncoding_auto(device);
		printf("The volume encoding is %d\n", foo);
		fflush(stdout);
		goto Exit;

	}
	if (vsdbAdopt && device) {
		printf("Adopting volinfo database device %s\n", device);
		fflush(stdout);
		DiskArbVSDBAdoptVolume_auto(device);
		vsdbDisplay = 1;
	}
	if (vsdbDisown && device) {
		printf("Disowning volinfo database device %s\n", device);
		fflush(stdout);
		DiskArbVSDBDisownVolume_auto(device);

		vsdbDisplay = 1;
	}
	if (vsdbDisplay && device) {
		printf("Displaying volinfo database device %s.  permissions = %d\n", device, DiskArbVSDBGetVolumeStatus_auto(device));
		fflush(stdout);
		goto Exit;
	}
	if (refresh) {
		printf("Refreshing Disk Arbitration ...\n");
		fflush(stdout);
		DiskArbRefresh_auto();

		goto Exit;
	}
	if (retain) {
		DiskArbAddCallbackHandler(kDA_DEVICE_RESERVATION_STATUS, reserveCallback, 0);
		DiskArbAddCallbackHandler(kDA_WILL_CLIENT_RELEASE_DEVICE, willRelease, 0);
		DiskArbRetainClientReservationForDevice(device);

	}
	if (test) {
		DiskArbAddCallbackHandler(kDA_DEVICE_RESERVATION_STATUS, reserveCallback, 0);
		DiskArbAddCallbackHandler(kDA_WILL_CLIENT_RELEASE_DEVICE, willRelease, 0);
		DiskArbIsDeviceReservedForClient(device);
	}
	if (release) {
		DiskArbReleaseClientReservationForDevice(device);
		goto Exit;
	}
	if (unrecognizedMessages) {
		printf("Waiting on unrecognized disks of all types with priority %d\n", flags);
		DiskArbClientHandlesUnrecognizedDisks(kDiskArbHandlesAllUnrecognizedOrUninitializedMedia, flags);
		DiskArbAddCallbackHandler(kDA_CLIENT_WILL_HANDLE_UNRECOGNIZED_DISK, unrecognizedCallback, 0);
	}
	if (do_unmount && device) {
		printf("%s device will be unmounted ...\n", device);
		fflush(stdout);
		DiskArbUnmountRequest_async_auto(device, flags);

		goto Loop;
	}
	if (punmount && device) {
		printf("%s partition will attempt to be unmounted ...\n", device);
		fflush(stdout);

		DiskArbUnmountRequest_async_auto(device, flags);

		goto Loop;
	}
	if (eject && device) {
		printf("%s device will attempt to be ejected ...\n", device);
		fflush(stdout);

		DiskArbUnmountAndEjectRequest_async_auto(device, flags);

		goto Loop;
	}
	if (do_mount && device) {
		printf("%s device will attempt to be mounted ...\n", device);
		fflush(stdout);

		DiskArbRequestMount_auto(device);

		goto Loop;
	}
	if (afpmount && device) {
		printf("%s afp mount will attempt to be mounted ...\n", device);
		fflush(stdout);

		DiskArbDiskAppearedWithMountpointPing_auto(device, kDiskArbDiskAppearedNetworkDiskMask | kDiskArbDiskAppearedEjectableMask, mountpoint);

		goto Exit;
	}
	if (afpdismount && device) {
		printf("%s afp mount will attempt to be unmounted ...\n", device);
		fflush(stdout);

		DiskArbDiskDisappearedPing_auto(device, afpflags);

		goto Exit;
	}
	if (newname && device && mountpoint) {
		printf("Device %s will be attempt to renamed to %s ... \n", device, mountpoint);
		fflush(stdout);

		DiskArbRequestDiskChange_auto(device, mountpoint, 0);
	}
Loop:
    CFRunLoopRun();

Exit:

	exit(0);
	//insure the process exit status is 0
		return 0;
	//...and make main fit the ANSI spec.

}
