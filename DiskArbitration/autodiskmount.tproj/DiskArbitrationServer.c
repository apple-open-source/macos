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
    unsigned char volumenamestorage[32];
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
	kern_return_t err = 0;

	dwarning(("%s(diskIdentifier = '%s', flags = $%08x, mountpoint = '%s')\n", __FUNCTION__, diskIdentifier, flags, mountpoint));
	
	if ( ! NewDisk(	diskIdentifier,		/*ioBSDName*/
					0,					/*ioBSDUnit*/
					NULL,				/*ioContentOrNull*/
					kDiskFamily_AFP,	/*family*/
					mountpoint,			/*mountpoint*/
					NULL,				/*ioMediaNameOrNull*/
                                        NULL,				/*ioDeviceTreePathOrNull*/
                                        NULL,				/*service*/
					flags ) )			/*flags*/
	{
		dwarning(("%s: NewDisk() failed!\n", __FUNCTION__));
	}
	
//	PrintDisks();

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
                diskPtr->mountpoint = mntbuf[index].f_mntonname;
                dwarning(("%s: Disk updated in mount table (did someone mount one)?\n", __FUNCTION__));
                dwarning(("%s %s %s\n", mntbuf[index].f_fstypename, mntbuf[index].f_mntonname, mntbuf[index].f_mntfromname));
                diskPtr->state = kDiskStateNew;
                SendDiskAppearedMsgs();

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
                    free( diskPtr->mountpoint );
                    diskPtr->mountpoint = strdup( "" ); // We need to send this in messages via MIG, so it cannot be NULL

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

    GetDisksFromRegistry( ioIterator );

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
	DiskArbDiskIdentifier diskIdentifier,
	unsigned flags)
{
	kern_return_t err = 0;
	DiskPtr diskPtr;

	dwarning(("%s(diskIdentifier = '%s', flags = $%08x)\n", __FUNCTION__, diskIdentifier, flags));

	diskPtr = LookupDiskByIOBSDName( diskIdentifier );
	if ( NULL == diskPtr )
	{
		pwarning(("%s(diskIdentifier = '%s', flags = $%08x): LookupDiskByIOBDSName failed\n", __FUNCTION__, diskIdentifier, flags));
		err = -1;
		goto Return;		
	}
	
	if ( AreWeBusy() )
	{
		pwarning(("%s(diskIdentifier = '%s', flags = $%08x): already busy\n", __FUNCTION__, diskIdentifier, flags));
		err = -1;
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
	DiskArbDiskIdentifier diskIdentifier,
	unsigned flags)
{
	kern_return_t err = 0;
	DiskPtr diskPtr;

	dwarning(("%s(diskIdentifier = '%s', flags = $%08x)\n", __FUNCTION__, diskIdentifier, flags));

	diskPtr = LookupDiskByIOBSDName( diskIdentifier );
	if ( NULL == diskPtr )
	{
		pwarning(("%s(diskIdentifier = '%s', flags = $%08x): LookupDiskByIOBDSName failed\n", __FUNCTION__, diskIdentifier, flags));
		err = -1;
		goto Return;		
	}
	
	if ( AreWeBusy() )
	{
		pwarning(("%s(diskIdentifier = '%s', flags = $%08x): already busy\n", __FUNCTION__, diskIdentifier, flags));
		err = -1;
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
	DiskArbDiskIdentifier diskIdentifier,
	unsigned flags)
{
	kern_return_t err = 0;
	DiskPtr diskPtr;

	dwarning(("%s(diskIdentifier = '%s', flags = $%08x)\n", __FUNCTION__, diskIdentifier, flags));

	diskPtr = LookupDiskByIOBSDName( diskIdentifier );
	if ( NULL == diskPtr )
	{
		dwarning(("%s(diskIdentifier = '%s', flags = $%08x): LookupDiskByIOBDSName failed\n", __FUNCTION__, diskIdentifier, flags));
		err = -1;
		goto Return;		
	}
	
	if ( AreWeBusy() )
	{
		pwarning(("%s(diskIdentifier = '%s', flags = $%08x): already busy\n", __FUNCTION__, diskIdentifier, flags));
		err = -1;
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
		/* Should probably double-check that no other client record already has this flag set. */
		clientPtr->flags |= kDiskArbIAmBlueBox;
	}

	SetBlueBoxBootVolume( seqno );

//Return:
	return err;

} // DiskArbSetBlueBoxBootVolume_async_rpc

/******************************************* DiskArbRequestDiskChange_rpc *******************************************/

kern_return_t DiskArbRequestDiskChange_rpc (
        mach_port_t server,
        DiskArbDiskIdentifier diskIdentifier,
        DiskArbMountpoint mountPoint,
        int flags)
{
        kern_return_t err = 0;
        DiskPtr diskPtr;
        char deviceName[MAXPATHLEN];
        char newMountName[MAXPATHLEN];
        char newVolumeName[MAXPATHLEN];
        int success = 0;
        char		cookieFile[MAXPATHLEN];
        struct stat 	sb;
        int i = 1;

        dwarning(("%s: renaming volume %s to %s\n", __FUNCTION__, diskIdentifier, mountPoint));


        sprintf(deviceName, "/dev/r%s", (char *)diskIdentifier);
        sprintf(newMountName, "%s/%s", (char *)mountPath(), (char *)mountPoint);
        sprintf(newVolumeName, "%s", (char *)mountPoint);
        sprintf(cookieFile, "/%s/%s", newMountName, ADM_COOKIE_FILE);

        diskPtr = LookupDiskByIOBSDName( diskIdentifier );

        // we may have to modify the new mountpoint name until it matches up with an empty directory, otherwise we could end up with 2 /tmp's for example ...

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
        
        if ( ! diskPtr )
        {
                pwarning(("%s(id=%s): no known disk with this diskIdentifier.\n", __FUNCTION__, diskIdentifier));
                err = -1;
        }
        else
        {

#warning - renaming devices is FS specific.

                boolean_t is_hfs = (strcmp(diskPtr->mountedFilesystemName, "hfs") == 0);
                boolean_t is_ufs = (strcmp(diskPtr->mountedFilesystemName, "ufs") == 0);
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
                                                strcpy(diskPtr->mountpoint, newMountName);
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
                                                strcpy(diskPtr->mountpoint, newMountName);
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

//Return:
        return err;

} // DiskArbRequestDiskChange_rpc

kern_return_t DiskArbSetCurrentUser_rpc (
        mach_port_t server,
        int user)
{
    currentConsoleUser = user;

    if (user == -1) {
        //someone logged out .. eject the disks they mounted
        DiskPtr diskPtr;

        for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = diskPtr->next)
        {
            if (diskPtr->mountedUser != -1) {
                // request an ejection on the disk
                // unmount the disk and it's partitions ...
                UnmountAllPartitions(diskPtr, FALSE);
                
                // remove the disk from my tables
                // FreeDisk(diskPtr);
            }
        }

    } else {
        io_iterator_t ioIterator;
        mach_port_t masterPort;
        kern_return_t err;

        err = IOMasterPort(bootstrap_port, &masterPort);

        // get an iterator from the registry for IOMedia and get the disks out of the registry ...
        err = IOServiceGetMatchingServices(masterPort, IOServiceMatching("IOMedia"), &ioIterator);

        GetDisksFromRegistry( ioIterator );

        autodiskmount(FALSE);
    }
    
    return 0;
}

kern_return_t DiskArbSetVolumeEncoding_rpc (mach_port_t server,
                                            DiskArbDiskIdentifier diskIdentifier,
                                            int volumeEncoding)
{
        // what's the encoding?
        char *encodingName;
        DiskPtr diskPtr;
        int isWritable;
        char bsdPath[MAXPATHLEN];
        char encodingString[MAXPATHLEN];
        boolean_t is_hfs;

        diskPtr = LookupDiskByIOBSDName( diskIdentifier );

        if (!diskPtr) {
                return 0;
        }

        sprintf(bsdPath,"/dev/%s", diskIdentifier);

        isWritable = ( diskPtr->flags & kDiskArbDiskAppearedLockedMask ) == 0;
        is_hfs = (strcmp(diskPtr->mountedFilesystemName, "hfs") == 0);

        if (!is_hfs) {
                return 0;
        }

        switch (volumeEncoding) {
                case 0:
                        encodingName = "Roman";
                        break;
                case 1:
                        encodingName = "Japanese";
                        break;
                case 3:
                        encodingName = "Korean";
                        break;
                case 4:
                        encodingName = "Arabic";
                        break;
                case 5:
                        encodingName = "Hebrew";
                        break;
                case 6:
                        encodingName = "Greek";
                        break;
                case 7:
                        encodingName = "Cyrillic";
                        break;
                default:
                        encodingName = "Roman";
                        break;
        }

        // build the string
        sprintf(encodingString, "-e=%s", encodingName);

        // mount_hfs , the mount command for an HFS filesystem
        // -u to signal an update
        // or -ur to signal a read only update
        // -o to signal something or another
        // -e to signal encoding
        // encodingName
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
                int success;
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
                        if (strcmp(diskPtr->mountpoint, "/")) {
                                // report that the name is different
                                int ret = rename(diskPtr->mountpoint, newMountName);
                                if (ret == 0) {
                                        strcpy(diskPtr->mountpoint, newMountName);
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

        if (!diskPtr) {
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

/* -- Printer Arbitration -- */

static ClientPtr LookupBlueBox( void );
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
		err = DiskArbPrinter_FinalResponse_rpc( clientPtr->port, locationID, 0x00000001 /* answer */ );
		goto Return;
	}

	/* Forward the question to Blue Box. */
	
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
		dwarning(("%s: Blue Box is not registered\n"));
		err = 0;
		goto Return;
	}

	/* Forward the question to Blue Box. */
	
	err = DiskArbPrinter_FinalRelease_rpc( bbClientPtr->port, locationID );

	goto Return;
	
Return:
	return err;

} // DiskArbPrinter_Release_rpc


