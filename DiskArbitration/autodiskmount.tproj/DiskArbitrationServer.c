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

#include <mach/port.h>
#include <mach/kern_return.h>
#include <mach/mach_error.h>

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <sys/ucred.h>
#include <sys/mount.h>
#include <sys/loadable_fs.h>
#include <sys/time.h>
#include <sys/attr.h>

#include <libc.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <bsd/dev/disk.h>
#include <errno.h>
#include <sys/wait.h>
#include <grp.h>

#include <ctype.h>
#include <dirent.h>

#include <mach/boolean.h>
#include <sys/loadable_fs.h>

#include <Security/Authorization.h>
#include <CoreFoundation/CoreFoundation.h>

#include <IOKit/IOKitLib.h>

#include "ClientToServer.h"
#include "ServerToClient.h"
#include "DiskArbitrationTypes.h"
#include "DiskArbitrationServerMain.h"
#include "GetRegistry.h"
#include "FSParticular.h"

#define FILESYSTEM_ERROR	 		0
#define FILESYSTEM_MOUNTED 			1
#define FILESYSTEM_MOUNTED_ALREADY	2
#define FILESYSTEM_NEEDS_REPAIR 	3

#define ATTRREFDATA(ref) (((char *)(&(ref))) + (ref).attr_dataoffset)

struct volattr {
    attrreference_t volnameref;
    unsigned char volumenamestorage[140];
};

struct volattrbuf {
    unsigned long length;
    struct volattr va;
};

struct cominfo {
        text_encoding_t encoding;
};

struct cominfobuf {
        unsigned long length;
        struct cominfo ci;
};

int currentConsoleUser = -1;

extern void autodiskmount(int ownership);

/********************************************************************************************************/
/******************************************* Private Prototypes *****************************************/
static ClientPtr LookupBlueBox( void );
/********************************************************************************************************/

/********************************************************************************************************/
/******************************************* Private Functions ******************************************/
/********************************************************************************************************/

/** ripped off from workspace - start **/
void cleanUpAfterFork(void)
{
    int fd, maxfd = getdtablesize();

        /* Close all inherited file descriptors */

    for (fd = 0; fd < maxfd; fd++)
    {
                close(fd);
    }

        /* Disassociate ourselves from any controlling tty */

    if ((fd = open("/dev/tty", O_NDELAY)) >= 0)
    {
                ioctl(fd, TIOCNOTTY, 0);
                close(fd);
    }

    /* Reset the user and group id's to their real values */

    setgid(getgid());
    setuid(getuid());

        /* stdin = /dev/null */
        /* stdout = /dev/console */
        /* stderr = stdout = /dev/console */

    fd = open("/dev/null", O_RDONLY);
    fd = open("/dev/console", O_WRONLY);
    dup2(1, 2);

}

/********************************************************************************************************/
/******************************************* Public Functions *******************************************/
/********************************************************************************************************/

/******************************************* ClientDeath *******************************************/


void ClientDeath(mach_port_t client);

void ClientDeath(mach_port_t client)
{
	ClientPtr thisPtr, previousPtr;

	/* Deletes the first client record whose port matches. */

	dwarning(("%s(client = $%08x)\n", __FUNCTION__, client));
	
	/* Is the list empty? */

	if ( g.Clients == NULL )
	{
		goto NotFound;
	}
	
	/* Is it the first element of the list? */
	
	if ( g.Clients->port == client )
	{
		/* Save the pointer */
		thisPtr = g.Clients;
		/* Unlink the record from the list */
		g.Clients = g.Clients->next;
		/* Assert: thisPtr->port == client */
		goto FoundIt;
	}
	
	/* Is it somewhere in the body of the list? */

	previousPtr = g.Clients;
	/* Assert: previousPtr != NULL since g.Clients != NULL */
	thisPtr = previousPtr->next;
	while ( thisPtr != NULL )
	{
		/* Invariant: previousPtr != NULL and thisPtr != NULL and previousPtr->next == thisPtr */
		if ( thisPtr->port == client )
		{
			/* Unlink the record from the list */
			previousPtr->next = thisPtr->next;
			/* Assert: thisPtr->port == client */
			goto FoundIt;
		}
		/* Advance to the next element, reestablishing the invariant */
		previousPtr = thisPtr;
		thisPtr = previousPtr->next;
	}
	
	/* If we get here, it means we never found a matching client record */
	
	goto NotFound;

FoundIt:

	/* Precondition: thisPtr->port == client */
	
	dwarning(("Found client record to delete:\n"));
	PrintClient( thisPtr );

	/* In case this client is mentioned on any ack lists, forge a no-error ack value. */

	MakeDeadClientAgreeable( thisPtr );

	/* Deallocate the port once, free the memory, decrement the count, and return */

	{
		kern_return_t r;

		r = mach_port_deallocate( mach_task_self(), thisPtr->port );
		if ( r ) dwarning(("%s(client = $%08x): mach_port_deallocate(...,$%08x) => $%08x: %s\n", __FUNCTION__, client, client, r, mach_error_string(r)));
	}

	if ( thisPtr->flags & kDiskArbIAmBlueBox )
	{
		dwarning(("%s: Blue Box died, resetting gBlueBoxBootVolume = -1\n",__FUNCTION__));
		SetBlueBoxBootVolume( -1 );
	}
        {
                DiskPtr diskPtr;

                for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = diskPtr->next)
                {
                        if (diskPtr->retainingClient == thisPtr->pid) {
                                if (diskPtr->flags & kDiskArbDiskAppearedUnrecognizableFormat) {
                                        diskPtr->state = kDiskStateNew;
                                        // mark the disk new so that someone else claims it
                                }
                                
                                diskPtr->retainingClient = 0;
                        }
                }

        }

        if (thisPtr->ackOnUnrecognizedDisk) {
                DiskPtr ackDisk = (DiskPtr)thisPtr->ackOnUnrecognizedDisk;
                ackDisk->retainingClient = 0;
                ackDisk->state = kDiskStateNew;
                ackDisk->lastClientAttemptedForUnrecognizedMessages = nil;
                thisPtr->ackOnUnrecognizedDisk = nil;
        }

        if (thisPtr->clientAuthRef) {
                AuthorizationFree(thisPtr->clientAuthRef, NULL);
        }


	free( thisPtr );

	g.NumClients--;


	goto Return;

NotFound:

	dwarning(("%s(client = $%08x): no matching client found!\n", __FUNCTION__, client));
	
	goto Return;

Return:

	dwarning(("%s($%08x), After:\n", __FUNCTION__, client));
	PrintClients();
	
	return;

} // ClientDeath


/******************************************* DiskArbRegister_rpc *******************************************/


kern_return_t DiskArbRegister_rpc (
	mach_port_t server,
	mach_port_t client,
	unsigned flags)
{
	kern_return_t err = 0;
	ClientPtr newClient = NULL;

	dwarning(("%s(client = $%08x, flags = $%08x)\n", __FUNCTION__, client, flags));

	/*
		First, attempt to register for death notifications.
		If it fails, then deallocate the client-supplied port
		and exit with an error without creating a client record.
	*/
	
	if ( EnableDeathNotifications( client ) )
	{
		dwarning(("%s(client = $%08x, flags = $%08x): EnableDeathNotifications() failed!\n", __FUNCTION__, (int)client, (int)flags));
		{
			kern_return_t r;
	
			r = mach_port_deallocate( mach_task_self(), client );
			dwarning(("%s(client = $%08x): mach_port_deallocate(...,$%08x) => $%08x: %s\n", __FUNCTION__, client, client, r, mach_error_string(r)));
		}
		err = -1;
		goto Return;
	}
	
	/*
		If we got this far, we can safely create a client record knowing that if the client
		dies we will be sent a MACH_NOTIFY_DEAD_NAME message.
	*/

	/* Allocate a new client record. */

	newClient = NewClient(client, 0, flags);
	if ( newClient == NULL )
	{
		dwarning(("%s(client = $%08x, flags = $%08x): NewClient failed!\n", __FUNCTION__, client, flags));
		err = -1;
		goto Return;
	} else {
            StartDiskRegistrationCompleteThread(newClient);
        }

	PrintClients();

Return:
	return err;

} // DiskArbRegister_rpc

/******************************************* DiskArbDiskAppearedWithMountpointPing_rpc *******************************************/


kern_return_t DiskArbDiskAppearedWithMountpointPing_rpc (
	mach_port_t server,
	DiskArbDiskIdentifier diskIdentifier,
	unsigned flags,
	DiskArbMountpoint mountpoint)
{
        DiskPtr newDisk;
	kern_return_t err = 0;

	dwarning(("%s(diskIdentifier = '%s', flags = $%08x, mountpoint = '%s')\n", __FUNCTION__, diskIdentifier, flags, mountpoint));

        newDisk = NewDisk(	diskIdentifier,		/*ioBSDName*/
                0,					/*ioBSDUnit*/
                NULL,				/*ioContentOrNull*/
                kDiskFamily_AFP,	/*family*/
                mountpoint,			/*mountpoint*/
                NULL,				/*ioMediaNameOrNull*/
                NULL,				/*ioDeviceTreePathOrNull*/
NULL,				/*service*/
-1,				/*uid*/
                flags );
	
	if ( !newDisk  )			/*flags*/
	{
		dwarning(("%s: NewDisk() failed!\n", __FUNCTION__));
        } else {
                newDisk->admCreatedMountPoint = 1;  // this is so we clean up mountpoints created by other apps when we close down the remote connections
        }

	goto Return;

Return:
	return err;

} // DiskArbDiskAppearedWithMountpointPing_rpc

/******************************************* DiskArbDiskAppearedWithMountpointPing_rpc *******************************************/


kern_return_t DiskArbDiskDisappearedPing_rpc (
        mach_port_t server,
        DiskArbDiskIdentifier diskIdentifier,
        unsigned flags)
{
        kern_return_t err = 0;
        DiskPtr diskPtr;

        dwarning(("%s(diskIdentifier = '%s', flags = $%08x)\n", __FUNCTION__, diskIdentifier, flags));

        diskPtr = LookupDiskByIOBSDName( diskIdentifier );
        if ( NULL == diskPtr )
        {
                dwarning(("%s(diskIdentifier = '%s'): LookupDiskByIOBDSName failed\n", __FUNCTION__, diskIdentifier));
                err = -1;
                goto Return;
        }

        err = UnmountAllPartitions( diskPtr, FALSE );
        SendUnmountPostNotifyMsgsForOnePartition( diskPtr->ioBSDName, 0, 0 );
        SendEjectPostNotifyMsgsForOnePartition( diskPtr->ioBSDName, 0, 0 );

        FreeDisk( diskPtr );
        if ( err ) goto Return;

//        PrintDisks();

        goto Return;

Return:
        return err;

} // DiskArbDiskDisappearedPing_rpc

/*** Local function for refresh ***/
static int DiskExistsInMountTable(DiskPtr diskPtr)
{
        struct statfs *mntbuf;
        int numMounts;
        int index = 1;


        if ((numMounts = getmntinfo(&mntbuf, MNT_NOWAIT)) == 0) {
            pwarning(("getmntinfo() call failed!\n"));
        }

        for (index=0; index < numMounts; index++) {
                char devdName[MAXPATHLEN];

                if (strcmp(diskPtr->ioBSDName, mntbuf[index].f_mntfromname) == 0) {
                        dwarning(("%s: Disk discovered in mount table as %s.\n", __FUNCTION__, mntbuf[index].f_mntfromname));

                        return TRUE;
                }
                sprintf(devdName, "/dev/%s", diskPtr->ioBSDName);
                if (strcmp(mntbuf[index].f_mntfromname, devdName) == 0) {
                        dwarning(("%s: Disk discovered in mount table as %s.\n", __FUNCTION__, devdName));

                        return TRUE;
                }
        }

        dwarning(("%s: Disk NOT discovered in mount table at %s(did someone umount it)?\n", __FUNCTION__, diskPtr->ioBSDName));
        return FALSE;
}


/******************************************* DiskArbRefresh_rpc *******************************************/
                                                        
kern_return_t DiskArbRefresh_rpc (
	mach_port_t server)
{
    kern_return_t err = 0;
    int index = 1;
    struct statfs *mntbuf;
    int numMounts;
    DiskPtr diskPtr;

    // look for things newly mounted

    if ((numMounts = getmntinfo(&mntbuf, MNT_NOWAIT)) == 0) {
        dwarning(("getmntinfo() call failed!\n"));
    }
    for (index=0; index < numMounts; index++) {
        char *dev_removed_name = NULL;

        if (strstr(mntbuf[index].f_mntfromname, "/dev/")) {
                dev_removed_name = mntbuf[index].f_mntfromname + 5;  /* 5 = /dev/ */
        }

        /* If the mountfrom name is 'fdesc', <volfs>, or devfs, skip it */

        if (strcmp(mntbuf[index].f_mntfromname, "fdesc") == 0) {
                continue;
        }
        else if (strcmp(mntbuf[index].f_mntfromname, "devfs") == 0) {
                continue;
        }
        else if (strcmp(mntbuf[index].f_mntfromname, "<volfs>") == 0) {
                continue;
        }
        else if (strstr(mntbuf[index].f_mntfromname, "automount -fstab") != 0) {
                continue;
        }

        if (!dev_removed_name) {
                diskPtr = LookupDiskByIOBSDName( mntbuf[index].f_mntfromname );
        } else {
                diskPtr = LookupDiskByIOBSDName( dev_removed_name );
        }

        if (diskPtr && (diskPtr->mountpoint == NULL || (!strcmp(diskPtr->mountpoint, "")))) {
                int newDisks;
                DiskSetMountpoint(diskPtr, mntbuf[index].f_mntonname);
                dwarning(("%s: Disk updated in mount table (did someone mount one)?\n", __FUNCTION__));
                dwarning(("%s %s %s\n", mntbuf[index].f_fstypename, mntbuf[index].f_mntonname, mntbuf[index].f_mntfromname));
                diskPtr->state = kDiskStateNew;
                newDisks = SendDiskAppearedMsgs();
                SendCompletedMsgs(kDiskArbCompletedDiskAppeared, newDisks);

        }

        else if (!diskPtr) {
                if ( !  NewDisk( dev_removed_name?dev_removed_name:mntbuf[index].f_mntfromname,
                                0,
                                NULL,
                                kDiskFamily_SCSI,
                                mntbuf[index].f_mntonname,
                                NULL,
                                 NULL,
                                 NULL,
                                 -1,
                             kDiskArbDiskAppearedNoFlags ) )
            {
                    dwarning(("%s: NewDisk() failed!\n", __FUNCTION__));
            } else {
                    dwarning(("%s: NewDisk() discovered (did someone mount one)?\n", __FUNCTION__));
                    dwarning(("%s %s %s\n", mntbuf[index].f_fstypename, mntbuf[index].f_mntonname, mntbuf[index].f_mntfromname));

            }
        }

    }


    for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = diskPtr->next)
    {
            int exists = 0;
            
            if (diskPtr->mountpoint == NULL || (strcmp(diskPtr->mountpoint, "") == 0)) {
                    continue;
            }
            
            // verify that each disk exists in the mount table
            exists = DiskExistsInMountTable(diskPtr);
            
            // otherwise - remove the disk
            if (!exists) {
                    DiskSetMountpoint(diskPtr, "");

                    SetStateForOnePartition( diskPtr, kDiskStateNewlyUnmounted );
                    SendUnmountPostNotifyMsgsForOnePartition( diskPtr->ioBSDName, 0, 0 );
            }
            
    }

    

    return err;

} // DiskArbRefresh_rpc

/******************************************* DiskArbRequestMount_rpc *******************************************/
kern_return_t DiskArbRequestMount_rpc (
        mach_port_t server,
        DiskArbDiskIdentifier diskIdentifier,
        int takeUserOwnership)
{
    kern_return_t err = 0;

    io_iterator_t ioIterator;
    mach_port_t masterPort;
    DiskPtr diskPtr;

    err = IOMasterPort(bootstrap_port, &masterPort);

    dwarning(("%s(diskIdentifier = '%s')\n", __FUNCTION__, diskIdentifier));
    // get an iterator from the registry for IOMedia and get the disks out of the registry ...
    err = IOServiceGetMatchingServices(masterPort, IOServiceMatching("IOMedia"), &ioIterator);

    diskPtr = LookupDiskByIOBSDName( diskIdentifier );

    if (diskPtr)
    {
            // unmount the disk and it's partitions ...
            UnmountAllPartitions(diskPtr, FALSE);

            // remove the disk from my tables
            FreeDisk(diskPtr);
    }

    GetDisksFromRegistry( ioIterator, 0 );

    autodiskmount(takeUserOwnership);  // is this sufficient?

    return err;

} // DiskArbRequestMount_rpc


/******************************************* DiskArbRegisterWithPID_rpc *******************************************/


kern_return_t DiskArbRegisterWithPID_rpc (
	mach_port_t server,
	mach_port_t client,
	int pid,
	unsigned flags)
{
	kern_return_t err = 0;
    ClientPtr clientPtr = NULL;

	dwarning(("%s(client = $%08x, pid = %d, flags = $%08x)\n", __FUNCTION__, client, pid, flags));

	/*
		First, attempt to register for death notifications.
		If it fails, then deallocate the client-supplied port
		and exit with an error without creating a client record.
	*/
	
	if ( EnableDeathNotifications( client ) )
	{
		dwarning(("%s(client = $%08x, pid = %d, flags = $%08x): EnableDeathNotifications() failed!\n", __FUNCTION__, (int)client, pid, (int)flags));
		{
			kern_return_t r;
	
			r = mach_port_deallocate( mach_task_self(), client );
			if ( r ) dwarning(("%s(client = $%08x): mach_port_deallocate(...,$%08x) => $%08x: %s\n", __FUNCTION__, client, client, r, mach_error_string(r)));
		}
		err = -1;
		goto Return;
	}
	
	/*
		If we got this far, we can safely create a client record knowing that if the client
		dies we will be sent a MACH_NOTIFY_DEAD_NAME message.
	*/

	/* Allocate a new client record. */

        clientPtr = LookupClientByPID( pid );
        if ( clientPtr )
        {
                /* Update existing client record. */
                dwarning(("%s(client = $%08x, pid = %d, flags = $%08x): updating existing client record:\n", __FUNCTION__, client, pid, flags));
                PrintClient( clientPtr );
                mach_port_deallocate( mach_task_self(), client );

                clientPtr->port = client; // Should we mach_port_deallocate() whatever was in there before if it differs from the new value?
                clientPtr->flags = flags;
                clientPtr->state = kDiskStateNew; // Force a re-send of all the msgs.
        }
        else
        {
                /* Allocate a new client record. */

                clientPtr = NewClient(client, pid, flags);
                if ( clientPtr == NULL )
                {
                        dwarning(("%s(client = $%08x, pid = %d, flags = $%08x): NewClient failed!\n", __FUNCTION__, client, pid, flags));
                        err = -1;
                } else {
                    dwarning(("%s(client = $%08x, pid = %d, flags = $%08x): Starting Complete Registration Thread!\n", __FUNCTION__, client, pid, flags));

                    StartDiskRegistrationCompleteThread(clientPtr);
                }
        }

	PrintClients();

Return:
	//*errorCodePtr = err;
	return err;

} // DiskArbRegisterWithPID_rpc

/******************************************* DiskArbmarkPIDNew_rpc *******************************************/


kern_return_t DiskArbMarkPIDNew_rpc (
        mach_port_t server,
        mach_port_t client,
        int pid,
        unsigned flags)
{

    dwarning(("%s(client = $%08x, pid = %d, flags = $%08x)\n", __FUNCTION__, client, pid, flags));
    ClientDeath(client);
    EnableDeathNotifications( client );
    NewClient(client, pid, flags);

    return 0;

} // DiskArbMarkPIDNew_rpc

/******************************************* DiskArbUpdateClientWithPID_rpc *******************************************/

kern_return_t DiskArbUpdateClientWithPID_rpc (
        mach_port_t server,
        int pid,
        unsigned flags)
{
        ClientPtr clientPtr = LookupClientByPID( pid );
        if ( clientPtr )
        {
                /* Update existing client record. */
                dwarning(("%s(pid = %d, flags = $%08x): updating existing client record:\n", __FUNCTION__, pid, flags));
                PrintClient( clientPtr );

                clientPtr->flags = flags;
                clientPtr->state = kDiskStateNew; // Force a re-send of all the msgs.
        }

    return 0;

} // DiskArbUpdateClientWithPID_rpc

/******************************************* DiskArbDeregister_rpc *******************************************/


kern_return_t DiskArbDeregister_rpc (
        mach_port_t server,
        mach_port_t client)
{

    dwarning(("%s(client = $%08x)\n", __FUNCTION__, client));
    ClientDeath(client);

    return 0;

} // DiskArbDeregister_rpc


kern_return_t DiskArbDeregisterWithPID_rpc (
        mach_port_t server,
        mach_port_t client,
        int pid)
{

    dwarning(("%s(client = $%08x)\n", __FUNCTION__, client));
    ClientDeath(client);

    return 0;

} // DiskArbDeregisterWithPID_rpc

/******************************************* DiskArbUnmountRequest_async_rpc *******************************************/


kern_return_t DiskArbUnmountRequest_async_rpc (
	mach_port_t server,
        pid_t clientPid,                                       
	DiskArbDiskIdentifier diskIdentifier,
	unsigned flags)
{
	kern_return_t err = 0;
	DiskPtr diskPtr;
        ClientPtr clientPtr = LookupClientByPID(clientPid);

	dwarning(("%s(diskIdentifier = '%s', flags = $%08x)\n", __FUNCTION__, diskIdentifier, flags));

	diskPtr = LookupDiskByIOBSDName( diskIdentifier );
	if ( NULL == diskPtr )
	{
		pwarning(("%s(diskIdentifier = '%s', flags = $%08x): LookupDiskByIOBDSName failed\n", __FUNCTION__, diskIdentifier, flags));
		err = -1;
                if (clientPtr) {
                        SendCallFailedMessage(clientPtr, NULL, kDiskArbUnmountRequestFailed, kDiskArbVolumeDoesNotExist);
                }

		goto Return;		
	}

        if (!requestingClientHasPermissionToModifyDisk(clientPid, diskPtr, "system.volume.unmount")) {
                err = -1;
                if (clientPtr) {
                        SendCallFailedMessage(clientPtr, diskPtr, kDiskArbUnmountRequestFailed, kDiskArbInsecureRequest);
                }
                goto Return;
        }
	
	if ( AreWeBusy() )
	{
		pwarning(("%s(diskIdentifier = '%s', flags = $%08x): already busy\n", __FUNCTION__, diskIdentifier, flags));
                err = -1;
                if (clientPtr) {
                        SendCallFailedMessage(clientPtr, diskPtr, kDiskArbUnmountRequestFailed, kDiskArbIsBusy);
                }
		goto Return;		
	}

        if (flags & kDiskArbForceUnmountFlag) {
            if (flags & kDiskArbUnmountOneFlag) {
                UnmountDisk( diskPtr, TRUE );
            } else {
                UnmountAllPartitions( diskPtr, TRUE );
            }
        }

        if (flags & kDiskArbUnmountOneFlag) {
            /* Mark just one of the partitions of this disk for unmounting (allocates ack-value tables, too). */

            SetStateForOnePartition( diskPtr, kDiskStateToBeUnmounted );
        } else {
            /* Mark all the partitions of this disk for unmounting (allocates ack-value tables, too). */

            SetStateForAllPartitions( diskPtr, kDiskStateToBeUnmounted );
        }


	/* Prepare a list of pre-unmount notifications to be sent. */
	
	PrepareToSendPreUnmountMsgs();
	
	/* Return to the main msg loop to process sending of pre-unmount msgs and receiving of acks. */

Return:
	return err;

} // DiskArbUnmountRequest_async_rpc


/******************************************* DiskArbUnmountPreNotifyAck_async_rpc *******************************************/


kern_return_t DiskArbUnmountPreNotifyAck_async_rpc(
	mach_port_t server,
	pid_t pid,
	DiskArbDiskIdentifier diskIdentifier,
	int errorCode)
{
	kern_return_t err = 0;
	DiskPtr diskPtr;
	ClientPtr clientPtr;

	dwarning(("%s(pid = %d, diskIdentifier = '%s', errorCode = %d)\n", __FUNCTION__, pid, diskIdentifier, errorCode));

	clientPtr = LookupClientByPID( pid );
	if ( ! clientPtr )
	{
		pwarning(("%s(pid = %d, diskIdentifier = '%s', errorCode = %d): no known client with this pid.\n", __FUNCTION__, pid, diskIdentifier, errorCode));
		err = -1;
		goto Return;		
	}

	clientPtr->numAcksRequired--;

	diskPtr = LookupDiskByIOBSDName( diskIdentifier );
	if ( NULL == diskPtr )
	{
		dwarning(("%s(pid = %d, diskIdentifier = '%s', errorCode = %d): LookupDiskByIOBDSName failed\n", __FUNCTION__, pid, diskIdentifier, errorCode));
		err = -1;
		goto Return;		
	}
	
	/* assert: diskPtr->state == kDiskStateToBeUnmounted/kDiskStateToBeUnmountedAndEjected  */

	UpdateAckValue( diskPtr->ackValues, pid, errorCode );
	
	dwarning(("%s: ack values for '%s'\n", __FUNCTION__, diskPtr->ioBSDName));
	PrintAckValues( diskPtr->ackValues );

	/* Return to the main msg loop.  It will check if enough acks were received. */

Return:
	return err;

} // DiskArbUnmountPreNotifyAck_async_rpc


/******************************************* DiskArbEjectRequest_async_rpc *******************************************/


kern_return_t DiskArbEjectRequest_async_rpc (
	mach_port_t server,
        pid_t clientPid,                                       
	DiskArbDiskIdentifier diskIdentifier,
	unsigned flags)
{
	kern_return_t err = 0;
	DiskPtr diskPtr;
        ClientPtr clientPtr = LookupClientByPID(clientPid);

	dwarning(("%s(diskIdentifier = '%s', flags = $%08x)\n", __FUNCTION__, diskIdentifier, flags));

	diskPtr = LookupDiskByIOBSDName( diskIdentifier );
        if ( NULL == diskPtr )
        {
                pwarning(("%s(diskIdentifier = '%s', flags = $%08x): LookupDiskByIOBDSName failed\n", __FUNCTION__, diskIdentifier, flags));
                err = -1;
                if (clientPtr) {
                        SendCallFailedMessage(clientPtr, NULL, kDiskArbEjectRequestFailed, kDiskArbVolumeDoesNotExist);
                }

                goto Return;
        }

        if (!requestingClientHasPermissionToModifyDisk(clientPid, diskPtr, "system.volume.eject")) {
                err = -1;
                if (clientPtr) {
                        SendCallFailedMessage(clientPtr, diskPtr, kDiskArbEjectRequestFailed, kDiskArbInsecureRequest);
                }
                goto Return;
        }

        if ( AreWeBusy() )
        {
                pwarning(("%s(diskIdentifier = '%s', flags = $%08x): already busy\n", __FUNCTION__, diskIdentifier, flags));
                err = -1;
                if (clientPtr) {
                SendCallFailedMessage(clientPtr, diskPtr, kDiskArbEjectRequestFailed, kDiskArbIsBusy);
                }
                goto Return;
        }
	
	/* Mark all the partitions of this disk for ejection (allocates ack-value tables, too). */

	SetStateForAllPartitions( diskPtr, kDiskStateToBeEjected );

	/* Prepare a list of pre-eject notifications to be sent. */

	PrepareToSendPreEjectMsgs();
	
	/* Return to the main msg loop to process sending of pre-eject msgs and receiving of acks. */

Return:
	return err;

} // DiskArbEjectRequest_async_rpc


/******************************************* DiskArbEjectPreNotifyAck_async_rpc *******************************************/


kern_return_t DiskArbEjectPreNotifyAck_async_rpc(
	mach_port_t server,
	pid_t pid,
	DiskArbDiskIdentifier diskIdentifier,
	int errorCode)
{
	kern_return_t err = 0;
	DiskPtr diskPtr;
	ClientPtr clientPtr;
	dwarning(("%s(pid = %d, diskIdentifier = '%s', errorCode = %d)\n", __FUNCTION__, pid, diskIdentifier, errorCode));

	clientPtr = LookupClientByPID( pid );
	if ( ! clientPtr )
	{
		pwarning(("%s(pid = %d, diskIdentifier = '%s', errorCode = %d): no known client with this pid.\n", __FUNCTION__, pid, diskIdentifier, errorCode));
		err = -1;
		goto Return;		
	}

	clientPtr->numAcksRequired--;

	diskPtr = LookupDiskByIOBSDName( diskIdentifier );
	if ( NULL == diskPtr )
	{
		dwarning(("%s(pid = %d, diskIdentifier = '%s', errorCode = %d): LookupDiskByIOBDSName failed\n", __FUNCTION__, pid, diskIdentifier, errorCode));
		err = -1;
		goto Return;		
	}
	
	/* assert: diskPtr->state == kDiskStateToBeEjected */

	UpdateAckValue( diskPtr->ackValues, pid, errorCode );
	
	dwarning(("%s: ack values for '%s'\n", __FUNCTION__, diskPtr->ioBSDName));
	PrintAckValues( diskPtr->ackValues );
	
	/* Return to the main msg loop.  It will check if enough acks were received. */

Return:
	return err;

} // DiskArbEjectPreNotifyAck_async_rpc


/******************************************* DiskArbUnmountAndEjectRequest_async_rpc *******************************************/


kern_return_t DiskArbUnmountAndEjectRequest_async_rpc (
	mach_port_t server,
        pid_t clientPid,
	DiskArbDiskIdentifier diskIdentifier,
	unsigned flags)
{
	kern_return_t err = 0;
	DiskPtr diskPtr;
        ClientPtr clientPtr = LookupClientByPID(clientPid);

	dwarning(("%s(diskIdentifier = '%s', flags = $%08x)\n", __FUNCTION__, diskIdentifier, flags));

	diskPtr = LookupDiskByIOBSDName( diskIdentifier );
        if ( NULL == diskPtr )
        {
                pwarning(("%s(diskIdentifier = '%s', flags = $%08x): LookupDiskByIOBDSName failed\n", __FUNCTION__, diskIdentifier, flags));
                err = -1;
                if (clientPtr) {
                        SendCallFailedMessage(clientPtr, NULL, kDiskArbUnmountAndEjectRequestFailed, kDiskArbVolumeDoesNotExist);
                }

                goto Return;
        }

        if (!requestingClientHasPermissionToModifyDisk(clientPid, diskPtr, "system.volume.unmount.eject")) {
                err = -1;
                if (clientPtr) {
                        SendCallFailedMessage(clientPtr, diskPtr, kDiskArbUnmountAndEjectRequestFailed, kDiskArbInsecureRequest);
                }
                goto Return;
        }

        if ( AreWeBusy() )
        {
                pwarning(("%s(diskIdentifier = '%s', flags = $%08x): already busy\n", __FUNCTION__, diskIdentifier, flags));
                err = -1;
                if (clientPtr) {
                        SendCallFailedMessage(clientPtr, diskPtr, kDiskArbUnmountAndEjectRequestFailed, kDiskArbIsBusy);
                }
                goto Return;
        }

        if (flags & kDiskArbForceUnmountFlag) {
                UnmountAllPartitions( diskPtr, TRUE );
        }
	
	/* Mark all the partitions of this disk for unmounting and ejection (allocates ack-value tables, too). */

	SetStateForAllPartitions( diskPtr, kDiskStateToBeUnmountedAndEjected );

	/* Prepare a list of pre-unmount notifications to be sent. */

	/* When the unmount happens the disk will proceed to the kDiskStateToBeEjected state. */
	
	PrepareToSendPreUnmountMsgs();
	
	/* Return to the main msg loop to await acknowledgements. */

Return:
	return err;

} // DiskArbUnmountAndEjectRequest_async_rpc


/******************************************* DiskArbSetBlueBoxBootVolume_async_rpc *******************************************/


kern_return_t DiskArbSetBlueBoxBootVolume_async_rpc (
	mach_port_t server,
	pid_t pid,
	int seqno)
{
	kern_return_t err = 0;
	ClientPtr clientPtr;
	
	dwarning(("%s(%d): old gBlueBoxBootVolume = %d\n", __FUNCTION__, seqno, GetBlueBoxBootVolume()));
	
	clientPtr = LookupClientByPID( pid );
	if ( ! clientPtr )
	{
		pwarning(("%s(pid=%d,seqno=%d): no known client with this pid.\n", __FUNCTION__, pid, seqno));
		err = -1;
		/* Do not exit early.  Set the gBlueBoxBootVolume anyhow. */
	}
	else
	{
                ClientPtr newClientPtr = LookupBlueBox();

                /* Double-check that no other client record already has this flag set. */
                if ( newClientPtr != NULL )
                {
        		dwarning(("%s(pid=%d,seqno=%d): BlueBox client already exists with pid=%d\n",
                  __FUNCTION__, pid, seqno, newClientPtr->pid));
                }
                            
		clientPtr->flags |= kDiskArbIAmBlueBox;
	}

	SetBlueBoxBootVolume( seqno );

//Return:
	return err;

} // DiskArbSetBlueBoxBootVolume_async_rpc

/******************************************* DiskArbRequestDiskChange_rpc *******************************************/

kern_return_t DiskArbRequestDiskChange_rpc (
        mach_port_t server,
        pid_t clientPid,                                       
        DiskArbDiskIdentifier diskIdentifier,
        DiskArbMountpoint mountPoint,
        int flags)
{
        kern_return_t err = 0;
        DiskPtr diskPtr;
        char deviceName[MAXPATHLEN];
        int success = 0;
        char		cookieFile[MAXPATHLEN];
        struct stat 	sb;
        int i = 1;
        ClientPtr clientPtr = LookupClientByPID(clientPid);

        char newMountName[MAXPATHLEN];
        char newVolumeName[MAXPATHLEN];

        //char *newMountName = malloc(sizeof(char)*MAXPATHLEN);
        //char *newVolumeName = malloc(sizeof(char)*MAXPATHLEN);

        sprintf(deviceName, "/dev/r%s", (char *)diskIdentifier);
        sprintf(newMountName, "%s/%s", (char *)mountPath(), (char *)mountPoint);
        sprintf(newVolumeName, "%s", (char *)mountPoint);
        sprintf(cookieFile, "/%s/%s", newMountName, ADM_COOKIE_FILE);

        dwarning(("%s: renaming volume %s to %s and attempting to mount at %s\n", __FUNCTION__, diskIdentifier, mountPoint, newMountName));

        diskPtr = LookupDiskByIOBSDName( diskIdentifier );

        // we may have to modify the new mountpoint name until it matches up with an empty directory, otherwise we could end up with 2 /tmp's for example ...

        if ( NULL == diskPtr )
        {
                pwarning(("%s(diskIdentifier = '%s', flags = $%08x): LookupDiskByIOBDSName failed\n", __FUNCTION__, diskIdentifier, flags));
                err = -1;
                if (clientPtr) {
                        SendCallFailedMessage(clientPtr, NULL, kDiskArbDiskChangeRequestFailed, kDiskArbVolumeDoesNotExist);
                }

                goto Return;
        }

        /*
         * 2758222
        if (!requestingClientHasPermissionToModifyDisk(clientPid, diskPtr, "system.volume.rename")) {
                err = -1;
                if (clientPtr) {
                        SendCallFailedMessage(clientPtr, diskPtr, kDiskArbDiskChangeRequestFailed, kDiskArbInsecureRequest);
                }
                goto Return;
        }
        */

        if (diskPtr && IsNetwork(diskPtr)) {
                err = -1;
                if (clientPtr) {
                        SendCallFailedMessage(clientPtr, diskPtr, kDiskArbDiskChangeRequestFailed, kDiskArbDiskIsNetwork);
                }
                goto Return;
        }

        if (strcmp(newMountName, diskPtr->mountpoint) == 0) { // the newName would be the same as the old name
                dwarning(("%s: attempt to rename volume %s to %s.  The name is the same.\n", __FUNCTION__, diskIdentifier, diskPtr->mountpoint));
                SendDiskChangedMsgs(diskIdentifier, newMountName, newVolumeName, flags, kDiskArbRenameSuccessful);
                return err;
        }
        
        while (1) {

                if (stat(newMountName, &sb) < 0)
                {
                    if (errno == ENOENT)
                    {
                                break;
                    }
                    else if (errno == EIO)
                    {
                                /* do nothing */
                    }
                    else
                    {
                                pwarning(("stat(%s) failed, %s\n", newMountName, strerror(errno)));
                                return (FALSE);
                    }
                }
                else if (rmdir(newMountName) == 0)
                {
                        dwarning(("The asked for directory (%s) has been removed\n", newMountName));

                        /* it was an empty directory */
                        break;
                } else if (errno == ENOTEMPTY) {
                         // some file exists, see if it's the ADM_COOKIE_FILE and if that is it remove the cookie and retry the rmdir
                        if (stat(cookieFile, &sb) == 0) {
                                    if (remove(cookieFile) == 0) {
                                        if (rmdir(newMountName) == 0) {
                                                    break;
                                            }
                                    }
                            }
                } else {
                        dwarning(("The asked for directory (%s) was not removed with errno = %d\n", newMountName, errno));
                }
                sprintf(newMountName, "%s/%s %d", mountPath(), mountPoint, i);
                i++;

        }        
        
        {
#warning - renaming devices is FS specific.
                boolean_t is_hfs;
                boolean_t is_ufs;

                if (!diskPtr->mountedFilesystemName || !strlen(diskPtr->mountedFilesystemName)) {
                        err = -1;
                        goto Return;
                } else {
                        is_hfs = (strcmp(diskPtr->mountedFilesystemName, "hfs") == 0);
                        is_ufs = (strcmp(diskPtr->mountedFilesystemName, "ufs") == 0);
                }

                // change the disk name on device diskIdentifier
                if (is_hfs) {
                        struct attrlist alist;
                        struct volattrbuf volinfobuf;
                        int result;

                        alist.bitmapcount = 5;
                        alist.commonattr = 0;
                        alist.volattr = ATTR_VOL_INFO | ATTR_VOL_NAME;
                        alist.dirattr = 0;
                        alist.fileattr = 0;
                        alist.forkattr = 0;

                        result = getattrlist(diskPtr->mountpoint, &alist, &volinfobuf, sizeof(volinfobuf), 0);
                        if (result != 0) {
                                dwarning(("Hey!  Couldn't get current volume name"));
                        };

                        strncpy(volinfobuf.va.volumenamestorage, mountPoint, sizeof(volinfobuf.va.volumenamestorage) - 1);
                        volinfobuf.va.volumenamestorage[sizeof(volinfobuf.va.volumenamestorage) - 1] = (char)0;
                        volinfobuf.va.volnameref.attr_dataoffset =
                            (char *)(&volinfobuf.va.volumenamestorage) - (char *)(&volinfobuf.va.volnameref);
                        volinfobuf.va.volnameref.attr_length = strlen(mountPoint);
                        result = setattrlist(diskPtr->mountpoint, &alist, &volinfobuf.va, sizeof(volinfobuf.va), 0);
                        if (result != 0) {
                                dwarning(("Hey!  Couldn't change volume name"));
                                success = kDiskArbRenameUnsuccessful;
                                // report that the name is the same
                                strcpy(newMountName, diskPtr->mountpoint);

                        } else {

                                if (strcmp(diskPtr->mountpoint, "/")) {
                                        // report that the name is different
                                        int ret = rename(diskPtr->mountpoint, newMountName);
                                        
                                        if (ret == 0) {
                                                DiskSetMountpoint(diskPtr, newMountName);
                                                success = kDiskArbRenameSuccessful;
                                        } else {
                                                dwarning(("Hey!  Couldn't change volume name, %d, %d return from rename\n", ret, errno));
                                                success = kDiskArbRenameSuccessful | kDiskArbRenameRequiresRemount;
                                        }
                                } else {
                                        strcpy(newMountName, "/");
                                        strcpy(diskPtr->mountpoint, "/");
                                }
                                // Let everyone know that the disk has changed it's path ...
                                success = kDiskArbRenameSuccessful;

                        }
                }
                else if (is_ufs)
                {
                        if (renameUFSDevice(diskIdentifier, mountPoint)) {
                                // successful volume rename

                                if (strcmp(diskPtr->mountpoint, "/")) {
                                        // report that the name is different
                                        int ret = rename(diskPtr->mountpoint, newMountName);

                                        if (ret == 0) {
                                                DiskSetMountpoint(diskPtr, newMountName);
                                                success = kDiskArbRenameSuccessful;
                                        } else {
                                                dwarning(("Hey!  Couldn't change volume name, %d, %d return from rename\n", ret, errno));
                                                success = kDiskArbRenameSuccessful | kDiskArbRenameRequiresRemount;
                                        }

                                } else {
                                        strcpy(diskPtr->mountpoint, "/");
                                        strcpy(newMountName, "/");
                                }

                                // DiskArbRequestMount_rpc(server, diskIdentifier, TRUE);
                                success = kDiskArbRenameSuccessful;
                        } else {
                                dwarning(("Hey!  Couldn't change volume name!"));
                                success = kDiskArbRenameUnsuccessful;
                        }
                } else {
                        dwarning(("Hey!  Couldn't change volume name since it isn't hfs or ufs!"));
                        success = kDiskArbRenameUnsuccessful;
                }
                SendDiskChangedMsgs(diskIdentifier, newMountName, newVolumeName, flags, success);
                PrintDisks();
        }

Return:
        if (err == -1) {
                SendDiskChangedMsgs(diskIdentifier, newMountName, newVolumeName, flags, kDiskArbRenameUnsuccessful);
        }

        return err;

} // DiskArbRequestDiskChange_rpc

kern_return_t DiskArbSetCurrentUser_rpc (
        mach_port_t server,
        pid_t clientPid,                                       
        int user)
{

    if (user == -1) {
        //someone logged out .. unmount the disks they mounted
        DiskPtr diskPtr;

        for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = diskPtr->next)
        {
            if (diskPtr->mountedUser == currentConsoleUser && ( diskPtr->flags & kDiskArbDiskAppearedEjectableMask ) ) {
                // request an unmount on the disk

                    if (diskPtr->ejectOnLogout) {
                            DiskArbUnmountAndEjectRequest_async_rpc( 0, 0, diskPtr->ioBSDName, FALSE);
                    } else {
                            // unmount the disk and it's partitions ...
                            DiskArbUnmountRequest_async_rpc( 0, 0, diskPtr->ioBSDName, FALSE);
                            // arg! no wonder - have to reset the mountedUser to -1
                            diskPtr->mountedUser = user;
                    }
                    
            }
        }
        currentConsoleUser = user;
        


    } else {
        io_iterator_t ioIterator;
        mach_port_t masterPort;
        kern_return_t err;

        currentConsoleUser = user;

        err = IOMasterPort(bootstrap_port, &masterPort);

        // get an iterator from the registry for IOMedia and get the disks out of the registry ...
        err = IOServiceGetMatchingServices(masterPort, IOServiceMatching("IOMedia"), &ioIterator);

        GetDisksFromRegistry( ioIterator, 0 );

        dwarning(("autodiskmount: Setting user to %d\n", user));

        autodiskmount(TRUE);
    }


    
    return 0;
}

kern_return_t DiskArbSetVolumeEncoding_rpc (mach_port_t server,
                                            pid_t clientPid,                                       
                                            DiskArbDiskIdentifier diskIdentifier,
                                            int volumeEncoding)
{
        // what's the encoding?
        DiskPtr diskPtr;
        int isWritable;
        char bsdPath[MAXPATHLEN];
        char encodingString[MAXPATHLEN];
        boolean_t is_hfs;
        ClientPtr clientPtr = LookupClientByPID(clientPid);

        diskPtr = LookupDiskByIOBSDName( diskIdentifier );

        dwarning(("%s: change volume %s encoding to %d\n", __FUNCTION__, diskIdentifier, volumeEncoding));

        if ( NULL == diskPtr )
        {
                pwarning(("%s(diskIdentifier = '%s'): LookupDiskByIOBDSName failed\n", __FUNCTION__, diskIdentifier));
                if (clientPtr) {
                        SendCallFailedMessage(clientPtr, NULL, kDiskArbSetEncodingRequestFailed, kDiskArbVolumeDoesNotExist);
                }

                return -1;
        }

        if (!requestingClientHasPermissionToModifyDisk(clientPid, diskPtr, "system.volume.setencoding")) {
                if (clientPtr) {
                        SendCallFailedMessage(clientPtr, diskPtr, kDiskArbSetEncodingRequestFailed, kDiskArbInsecureRequest);
                }
                return -1;
        }

        sprintf(bsdPath,"/dev/%s", diskIdentifier);

        isWritable = ( diskPtr->flags & kDiskArbDiskAppearedLockedMask ) == 0;
        is_hfs = (strcmp(diskPtr->mountedFilesystemName, "hfs") == 0);

        if (!is_hfs) {
                if (clientPtr) {
                        SendCallFailedMessage(clientPtr, diskPtr, kDiskArbSetEncodingRequestFailed, kDiskArbInvalidVolumeFormat);
                }
                return -1;
        }

        // build the string
        sprintf(encodingString, "-e=%d", volumeEncoding);

        // mount_hfs , the mount command for an HFS filesystem
        // -u to signal an update
        // or -ur to signal a read only update
        // -o to signal something or another
        // -e to signal encoding
        // encodingnumber
        // /dev/disk* , diskPtr->bsdName
        // /Volumes/VolName, diskPtr->mountPoint

        {
                const char *childArgv[] = {
                        "/sbin/mount",
                        isWritable?"-u":"-ur",
                        "-o",
                        encodingString,
                        "-t",
                        "hfs",
                        bsdPath,
                        diskPtr->mountpoint,
                        0 };
                int pid;

                    if ((pid = fork()) == 0)
                    {
                                    /* CHILD PROCESS */

                        cleanUpAfterFork();
                        execve("/sbin/mount", childArgv, 0);
                        exit(-127);
                    }
                    else if (pid > 0)
                    {
                        int statusp;
                        int waitResult;
                        int result;

                        /* PARENT PROCESS */
                        dwarning(("wait4(pid=%d,&statusp,0,NULL)...\n", pid));
                        waitResult = wait4(pid,&statusp,0,NULL);
                        dwarning(("wait4(pid=%d,&statusp,0,NULL) => %d\n", pid, waitResult));
                        if (waitResult > 0)
                        {
                            if (WIFEXITED(statusp))
                            {
                                    result = (int)(char)(WEXITSTATUS(statusp));
                            }
                        }
                    }
        }

        // now check and see if the encoding changed the name, and if it did so, trigger a volume rename on that volume
        {
                struct attrlist alist;
                struct volattrbuf volinfobuf;
                int result;
                int success = kDiskArbRenameSuccessful;
                struct stat 	sb;
                char newMountName[MAXPATHLEN];
                char cookieFile[MAXPATHLEN];
                int i = 1;

                alist.bitmapcount = 5;
                alist.commonattr = 0;
                alist.volattr = ATTR_VOL_INFO | ATTR_VOL_NAME;
                alist.dirattr = 0;
                alist.fileattr = 0;
                alist.forkattr = 0;

                result = getattrlist(diskPtr->mountpoint, &alist, &volinfobuf, sizeof(volinfobuf), 0);
                if (result != 0) {
                        dwarning(("Hey!  Couldn't get current volume name"));
                };

                sprintf(newMountName, "%s/%s", (char *)mountPath(), volinfobuf.va.volumenamestorage);
                sprintf(cookieFile, "/%s/%s", newMountName, ADM_COOKIE_FILE);

                if (strcmp(newMountName, diskPtr->mountpoint) != 0) {
                        // the volume name is no longer the same, rename the mount point ...

                        while (1) {

                                if (stat(newMountName, &sb) < 0)
                                {
                                if (errno == ENOENT)
                                {
                                                break;
                                }
                                else if (errno == EIO)
                                {
                                                /* do nothing */
                                }
                                else
                                {
                                        pwarning(("stat(%s) failed, %s\n", newMountName, strerror(errno)));
                                                return (FALSE);
                                }
                                }
                                else if (rmdir(newMountName) == 0)
                                {
                                        dwarning(("The asked for directory (%s) has been removed\n", newMountName));

                                        /* it was an empty directory */
                                        break;
                                } else if (errno == ENOTEMPTY) {
                                        // some file exists, see if it's the ADM_COOKIE_FILE and if that is it remove the cookie and retry the rmdir
                                        if (stat(cookieFile, &sb) == 0) {
                                                if (remove(cookieFile) == 0) {
                                                        if (rmdir(newMountName) == 0) {
                                                                break;
                                                        }
                                                }
                                        }
                                } else {
                                        dwarning(("The asked for directory (%s) was not removed with errno = %d\n", newMountName, errno));
                                }
                                sprintf(newMountName, "%s/%s %d", mountPath(), volinfobuf.va.volumenamestorage, i);
                                i++;
                        }
                        dwarning(("Encoding changed which is forcing a rename on %s to %s\n", diskPtr->mountpoint, newMountName));


                        if (strcmp(newMountName, diskPtr->mountpoint) != 0) {
                                // the volume name is no longer the same, rename the mount point ...
                                dwarning(("Encoding changed which is forcing a rename on %s to %s\n", diskPtr->mountpoint, newMountName));


                                if (strcmp(diskPtr->mountpoint, "/")) {
                                        // report that the name is different
                                        int ret = rename(diskPtr->mountpoint, newMountName);

                                        if (ret == 0) {
                                                DiskSetMountpoint(diskPtr, newMountName);
                                                success = kDiskArbRenameSuccessful;
                                                dwarning(("Changed volume name\n"));
                                        } else {
                                                dwarning(("Hey!  Couldn't change volume name, %d, %d return from rename\n", ret, errno));
                                                success = kDiskArbRenameUnsuccessful;
                                        }
                                } else {
                                        strcpy(diskPtr->mountpoint, "/");
                                        success = kDiskArbRenameUnsuccessful;
                                }
                        }
                }
                SendDiskChangedMsgs(diskIdentifier, newMountName, volinfobuf.va.volumenamestorage, 0, success);
        }
        
    return 0;
}

kern_return_t DiskArbGetVolumeEncoding_rpc (mach_port_t server,
                                            DiskArbDiskIdentifier diskIdentifier,
                                            int *volumeEncoding)
{
        DiskPtr diskPtr;
        boolean_t is_hfs;

        *volumeEncoding = -1;

        diskPtr = LookupDiskByIOBSDName( diskIdentifier );

        if (!diskPtr || !diskPtr->mountedFilesystemName) {
                return 0;
        }

        is_hfs = (strcmp(diskPtr->mountedFilesystemName, "hfs") == 0);
        if (is_hfs) {
                struct attrlist alist;
                struct cominfobuf cibuf;
                int result;

                alist.bitmapcount = 5;
                alist.commonattr = ATTR_CMN_SCRIPT;
                alist.volattr = 0;
                alist.dirattr = 0;
                alist.fileattr = 0;
                alist.forkattr = 0;

                result = getattrlist(diskPtr->mountpoint, &alist, &cibuf, sizeof(cibuf), 0);
                if (result != 0) {
                       //error
                        *volumeEncoding = -1;
                } else {
                        *volumeEncoding = cibuf.ci.encoding;
                }
        }

    return 0;
}

/* -- Printer Arbitration -- */

static ClientPtr LookupBlueBox( void )
{
	ClientPtr result = NULL;
	ClientPtr clientPtr;

	dwarning(("=> %s()\n", __FUNCTION__));

	for (clientPtr = g.Clients; clientPtr != NULL; clientPtr = clientPtr->next)
	{
		if ( clientPtr->flags & kDiskArbIAmBlueBox )
		{
			result = clientPtr;
			goto Return;
		}
	}
	
	result = NULL;
	goto Return;
	
Return:
	dwarning(("<= %s(): 0x%08x\n", __FUNCTION__, (int)result));
	return result;

} // LookupBlueBox

kern_return_t DiskArbPrinter_Request_rpc (
	mach_port_t server,
	pid_t pid,
	int locationID)
{
	kern_return_t err = 0;
	ClientPtr clientPtr;
	ClientPtr bbClientPtr;
	
	dwarning(("=> %s(pid=%d,locationID=0x%08x)\n", __FUNCTION__, pid, locationID));

	bbClientPtr = LookupBlueBox();
	if ( ! bbClientPtr )
	{
		clientPtr = LookupClientByPID( pid );
		if ( ! clientPtr )
		{
			// This could happen if the requester died before Blue Box could answer.
			dwarning(("%s(pid=%d,locationID=0x%08x): no known client with this pid.\n", __FUNCTION__, pid, locationID));
			err = -1;
			goto Return;
		}

		dwarning(("%s: Blue Box is not registered\n", __FUNCTION__));
#warning  Should this be in a thread
		err = DiskArbPrinter_FinalResponse_rpc( clientPtr->port, locationID, 0x00000001 /* answer */ );
		goto Return;
	}

	/* Forward the question to Blue Box. */

#warning  Should this be in a thread
	err = DiskArbPrinter_FinalRequest_rpc( bbClientPtr->port, pid, locationID );
	goto Return;
	
Return:
	return err;

} // DiskArbPrinter_Request_rpc

kern_return_t DiskArbPrinter_Response_rpc (
	mach_port_t server,
	pid_t pid,
	int locationID,
	int answer)
{
	kern_return_t err = 0;
	ClientPtr clientPtr;

	dwarning(("%s(pid=%d,locationID=0x%08x,answer=0x%08x)\n", __FUNCTION__, pid, locationID, answer));

	/* Blue Box wants to let the requester with pid <pid> know the answer. */

	clientPtr = LookupClientByPID( pid );
	if ( ! clientPtr )
	{
		// This could happen if the requester died before Blue Box could answer.
		dwarning(("%s(pid=%d,locationID=0x%08x,answer=0x%08x): no known client with this pid.\n", __FUNCTION__, pid, locationID, answer));
		err = -1;
		goto Return;
	}

	/* Forward the message to the Blue Box */

#warning  Should this be in a thread
	err = DiskArbPrinter_FinalResponse_rpc( clientPtr->port, locationID, answer );
	
	goto Return;

Return:
	return err;

} // DiskArbPrinter_Response_rpc

kern_return_t DiskArbPrinter_Release_rpc (
	mach_port_t server,
	int locationID)
{
	kern_return_t err = 0;
	ClientPtr bbClientPtr;
	
	dwarning(("=> %s(locationID=0x%08x)\n", __FUNCTION__, locationID));

	bbClientPtr = LookupBlueBox();
	if ( ! bbClientPtr )
	{
		dwarning(("%s: Blue Box is not registered\n", __FUNCTION__));
		err = 0;
		goto Return;
	}

	/* Forward the question to Blue Box. */

#warning  Should this be in a thread
	err = DiskArbPrinter_FinalRelease_rpc( bbClientPtr->port, locationID );

	goto Return;
	
Return:
	return err;

} // DiskArbPrinter_Release_rpc

/*

-- DEVICE RESERVATIONS --

 */

kern_return_t DiskArbIsDeviceReservedForClient_rpc ( mach_port_t server, DiskArbDiskIdentifier diskIdentifier, pid_t clientPid)
{
        DiskPtr diskPtr;
        DiskPtr wholePtr;
        ClientPtr clientPtr;

        int status = kDiskArbDeviceIsNotReserved;
        int pid = 0;

        clientPtr = LookupClientByPID(clientPid);
        if ( ! clientPtr )
        {
                dwarning(("%s: client ptr not found %d", __FUNCTION__, clientPid));
                return 0;
        }


        dwarning(("%s(diskIdentifier = '%s', from pid = '%d')\n", __FUNCTION__, diskIdentifier, clientPid));

        diskPtr = LookupDiskByIOBSDName( diskIdentifier );
        if (diskPtr) {
                wholePtr = LookupWholeDiskForThisPartition( diskPtr );
        } else {
                dwarning(("%s: diskPtr not found %s", __FUNCTION__, diskIdentifier));
                return 0;
        }

        if (!wholePtr) {
                pwarning(("%s: wholePtr not found, cannot retain reservation on %s", __FUNCTION__, diskIdentifier));
        }

        if (wholePtr->retainingClient) {

                status = kDiskArbDeviceIsReserved;
                pid = wholePtr->retainingClient;
        }

        // call the client back
#warning  Should this be in a thread
        DiskArbDeviceReservationStatus_rpc(clientPtr->port, diskIdentifier, status, pid);

        return 0;

}

kern_return_t DiskArbRetainClientReservationForDevice_rpc ( mach_port_t server, DiskArbDiskIdentifier diskIdentifier, pid_t clientPid)
{
        DiskPtr diskPtr;
        DiskPtr wholePtr;
        ClientPtr clientPtr = LookupClientByPID(clientPid);

        int pid = 0;

        if ( ! clientPtr )
        {
                dwarning(("%s: client ptr not found %d", __FUNCTION__, clientPid));
                return 0;
        }

        dwarning(("%s(diskIdentifier = '%s', pid = '%d')\n", __FUNCTION__, diskIdentifier, clientPid));

        diskPtr = LookupDiskByIOBSDName( diskIdentifier );
        if (diskPtr) {
                wholePtr = LookupWholeDiskForThisPartition( diskPtr );
        } else {
                dwarning(("%s: diskPtr not found %s", __FUNCTION__, diskIdentifier));
                return 0;
        }

        if (!wholePtr) {
                pwarning(("%s: wholePtr not found, cannot retain reservation on %s", __FUNCTION__, diskIdentifier));
        }

        if (wholePtr->retainingClient) {
                ClientPtr remoteClientPtr = LookupClientByPID(wholePtr->retainingClient);
                if ( ! remoteClientPtr )
                {
                        dwarning(("%s: client ptr not found %d", __FUNCTION__, wholePtr->retainingClient));
                        return 0;
                }

		// here is where we ask the retaining client if they will give it up ...
#warning  Should this be in a thread
               DiskArbWillClientRelinquish_rpc(remoteClientPtr->port, diskIdentifier, clientPid);
               // if the application doesn't have a handler for this, the application will immediately callback and say "nope - I won't give it up"

        } else {
                wholePtr->retainingClient = clientPid;
                pid = wholePtr->retainingClient;

                // call the client back right away
#warning  Should this be in a thread
                DiskArbDeviceReservationStatus_rpc(clientPtr->port, diskIdentifier, kDiskArbDeviceReservationObtained, pid);
        }
        
        return 0;

}

kern_return_t DiskArbReleaseClientReservationForDevice_rpc ( mach_port_t server, DiskArbDiskIdentifier diskIdentifier, pid_t pid)
{
        DiskPtr diskPtr;
        DiskPtr wholePtr;

        dwarning(("%s(diskIdentifier = '%s', pid = '%d')\n", __FUNCTION__, diskIdentifier, pid));

        diskPtr = LookupDiskByIOBSDName( diskIdentifier );
        if (diskPtr) {
                wholePtr = LookupWholeDiskForThisPartition( diskPtr );
        } else {
                dwarning(("%s: diskPtr not found %s", __FUNCTION__, diskIdentifier));
                return 0;
        }

        if (!wholePtr) {
                pwarning(("%s: wholePtr not found, cannot release reservation on %s", __FUNCTION__, diskIdentifier));
        } else if (wholePtr->retainingClient == pid) {
                wholePtr->retainingClient = 0;
	}
        
        return 0;

}

kern_return_t DiskArbClientRelinquishesReservation_rpc ( mach_port_t server, DiskArbDiskIdentifier diskIdentifier, pid_t pid, pid_t releaseToClientPid, int status)
{
        DiskPtr diskPtr;
        DiskPtr wholePtr;
        ClientPtr clientPtr = LookupClientByPID(releaseToClientPid);

        if ( ! clientPtr )
        {
                dwarning(("%s: client ptr not found %d", __FUNCTION__, releaseToClientPid));
                return 0;
        }

        dwarning(("%s(diskIdentifier = '%s', my pid = '%d', release to pid = '%d', status = '%d')\n", __FUNCTION__, diskIdentifier, pid, releaseToClientPid, status));

        diskPtr = LookupDiskByIOBSDName( diskIdentifier );
        if (diskPtr) {
                wholePtr = LookupWholeDiskForThisPartition( diskPtr );
        } else {
                dwarning(("%s: diskPtr not found %s", __FUNCTION__, diskIdentifier));
                return 0;
        }

        if (!wholePtr) {
                pwarning(("%s: wholePtr not found, cannot release reservation on %s", __FUNCTION__, diskIdentifier));
                return 0;
        }

        if (status) {
                // now notify the person who asked that they now have or don't have the reservation
                wholePtr->retainingClient = releaseToClientPid;
#warning  Should this be in a thread
                DiskArbDeviceReservationStatus_rpc(clientPtr->port, diskIdentifier, kDiskArbDeviceReservationObtained, releaseToClientPid);
        } else {
                wholePtr->retainingClient = pid;
#warning  Should this be in a thread
                DiskArbDeviceReservationStatus_rpc(clientPtr->port, diskIdentifier, kDiskArbDeviceReservationRefused, pid);
        }
        
        return 0;

}

/* Unintialized Disk notifications */

kern_return_t DiskArbClientHandlesUnrecognizedDisks_rpc ( mach_port_t server,  pid_t pid, int types, int priority)
{
        ClientPtr clientPtr = LookupClientByPID(pid);

        if (!clientPtr) {
                dwarning(("%s : No client ptr for client pid %d\n", __FUNCTION__, pid));
        } else {
                clientPtr->unrecognizedPriority = priority;
                clientPtr->notifyOnDiskTypes = types;
        }
        return 0;
}

kern_return_t DiskArbClientHandlesUninitializedDisks_rpc (
        mach_port_t server,
        int clientPid,
        int flags)
{
        kern_return_t err = 0;
        ClientPtr clientPtr;

        dwarning(("%s(%d:%d)\n", __FUNCTION__, clientPid, flags));

        clientPtr = LookupClientByPID( clientPid );
        if ( ! clientPtr )
        {
                dwarning(("%s(pid=%d,flags=%d): no known client with this pid.\n", __FUNCTION__, clientPid, flags));
                err = -1;
        }
        else
        {
                if (flags) {
                        // or it in
                        clientPtr->flags |= kDiskArbClientHandlesUninitializedDisks;
                } else {
                        // and it out
                        clientPtr->flags &= ~kDiskArbClientHandlesUninitializedDisks;
                }
        }

        return err;
}


kern_return_t DiskArbClientWillHandleUnrecognizedDisk_rpc ( mach_port_t server,  DiskArbDiskIdentifier diskIdentifier, pid_t pid, int yesNo)
{
        DiskPtr diskPtr = LookupDiskByIOBSDName( diskIdentifier );
        ClientPtr clientPtr = LookupClientByPID( pid );
        clientPtr->ackOnUnrecognizedDisk = nil;

        if (!clientPtr || !diskPtr) {
                return 0;
        }
        
        if (yesNo == FALSE) {

		/* Mark the disk new again, this will get the disk to get
                recognized as unrecognizable again in the next loop */
                diskPtr->state = kDiskStateNew;
        } else {
                DiskPtr wholePtr = LookupWholeDiskForThisPartition( diskPtr );
                diskPtr->retainingClient = pid;
                /* give a claim on the disk to the client */
                if (wholePtr) {
                        wholePtr->retainingClient = pid;
                }
        }
       
        return 0;
}

kern_return_t DiskArbSetSecuritySettingsForClient_rpc ( mach_port_t server,  pid_t pid, DiskArbSecurityToken token)
{
        kern_return_t err = 0;
        ClientPtr clientPtr = LookupClientByPID( pid );

        if (!clientPtr) {
                dwarning(("%s(pid=%d): no known client with this pid.\n", __FUNCTION__, pid));
                err = -1;
        } else {
                AuthorizationRef ref;
                int ok = AuthorizationCreateFromExternalForm((const AuthorizationExternalForm *)token, &ref);

                if (0 == ok && ref) {
                        dwarning(("%s Setting token for (pid=%d)\n", __FUNCTION__, pid));
                        // good
                        clientPtr->clientAuthRef = ref;

                } else {
                        // bad
                }


        }

        return err;

}

static void EjectAllCDAndDVDMedia()
{
        // spin through all disks, find whole media ones only, if it's a CD or DVD, eject it
        DiskPtr diskPtr;

        for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = diskPtr->next)
        {
                DiskPtr wholePtr = LookupWholeDiskForThisPartition(diskPtr);

                if (wholePtr == diskPtr) {
                        if ((diskPtr->flags & kDiskArbDiskAppearedCDROMMask) || (diskPtr->flags & kDiskArbDiskAppearedDVDROMMask)) {
                                DiskArbUnmountAndEjectRequest_async_rpc( 0, 0, diskPtr->ioBSDName, FALSE);
                        }
                }
        }
}

static void OpenVacantDriveDoor(io_registry_entry_t device)
{
    io_registry_entry_t driver = 0;
    io_registry_entry_t media  = 0;
    kern_return_t       status = KERN_SUCCESS;

    status = IORegistryEntryGetChildEntry(device, kIOServicePlane, &driver);
    if (status != KERN_SUCCESS)  goto OpenVacantDriveDoorErr;

    status = IORegistryEntryGetChildEntry(driver, kIOServicePlane, &media);
    if (status == KERN_SUCCESS)  goto OpenVacantDriveDoorErr;

    IORegistryEntrySetCFProperty(device, CFSTR("TrayState"), kCFBooleanTrue);

OpenVacantDriveDoorErr:

    if (driver)  IOObjectRelease(driver);
    if (media)  IOObjectRelease(media);
}

static void OpenAllVacantCDAndDVDDriveDoors()
{
    CFMutableDictionaryRef description = 0;
    mach_port_t            masterPort  = 0;
    io_registry_entry_t    device      = 0;
    io_iterator_t          devices     = 0;
    kern_return_t          status      = KERN_SUCCESS;

    status = IOMasterPort(bootstrap_port, &masterPort);
    if (status != KERN_SUCCESS)  goto OpenAllVacantCDAndDVDDriveDoorsErr;

    description = IOServiceMatching("IOCDBlockStorageDevice");
    if (description == 0)  goto OpenAllVacantCDAndDVDDriveDoorsErr;

    status = IOServiceGetMatchingServices(masterPort, description, &devices);
    if (status != KERN_SUCCESS)  goto OpenAllVacantCDAndDVDDriveDoorsErr;

    description = 0; // (retain consumed in above call)

    while ( (device = IOIteratorNext(devices)) )
    {
        OpenVacantDriveDoor(device);
        IOObjectRelease(device);
    }

OpenAllVacantCDAndDVDDriveDoorsErr:

    if (description)  CFRelease(description);
    if (devices)  IOObjectRelease(devices);
}


kern_return_t DiskArbOpenVacantDriveDoors_rpc ( mach_port_t server )
{
        OpenAllVacantCDAndDVDDriveDoors();
        return 0;

}

kern_return_t DiskArbEjectKeyPressed_rpc ( mach_port_t server )
{
        OpenAllVacantCDAndDVDDriveDoors();
        EjectAllCDAndDVDMedia();
        return 0;

}
