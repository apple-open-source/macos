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
#include <libc.h>
#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/message.h>
#include <servers/bootstrap.h>
#include <mach/mach_error.h>
#include <pthread.h>
#include <assert.h>


#include <sys/types.h>
#include <errno.h>
#include <stdio.h>
#include <dev/disk.h>
#include <grp.h>
#include <pwd.h>
#include <paths.h>

#include <IOKit/OSMessageNotification.h>
#include <IOKit/IOKitLib.h>

#include <SystemConfiguration/SystemConfiguration.h>


#include "ServerToClient.h"
#include "ClientToServer.h"
#include "DiskArbitrationTypes.h"
#include "GetRegistry.h"
#include "FSParticular.h"

#include <mach/mach_interface.h>
#include <IOKit/IOBSD.h>
#include <IOKit/storage/IOMedia.h>

#include <syslog.h> /* bek - 12/18/98 */

#include "DiskArbitrationServerMain.h"
#include "DiskVolume.h"

mach_port_t ioMasterPort;

void autodiskmount(int ownership);

uid_t 		requestorUID;
gid_t 		requestorGID;
mach_port_t 	requestingClientPort;
int 		DA_postponedDisksExist;
int 		DA_stateChangedSpinRunLoop;

	
/*
	External globals
*/

#ifdef DEBUG
int gDebug = 1;
#else
int gDebug = 0;
#endif

int gDaemon;

/*
	External routines
*/

extern void ClientDeath(mach_port_t clientPort, int deadNotification);

extern boolean_t ClientToServer_server(mach_msg_header_t * msg, mach_msg_header_t * reply);

/*
	Public globals
*/

/*
	Static constants
*/

enum {
	kMsgSize = 2048, // bek - 7/15/99 - Messages can contain path names as long as PATH_MAX = 1024
};

/*
	Static globals
*/

static char * programName = NULL;

mach_port_t gNotifyPort = MACH_PORT_NULL;

static int gBlueBoxBootVolume = -1;

/*
	Static function prototypes
*/

static kern_return_t InitNotifyPort(void);
static mach_port_t GetNotifyPort(void);
static boolean_t MessageIsNotificationDeath(const mach_msg_header_t *msg, mach_port_t *deadPort);

static int NextSequenceNumber( void );

struct IONotificationMsg {
	mach_msg_header_t		msgHdr;
	OSNotificationHeader	notifyHeader;
	mach_msg_trailer_t		trailer;
};

CFStringRef yankedHeader;
CFStringRef yankedMessage;
CFStringRef unrecognizedHeader;
CFStringRef unrecognizedHeaderNoInitialize;
CFStringRef unrecognizedMessage;
CFStringRef ejectString;
CFStringRef ignoreString;
CFStringRef initString;
CFStringRef launchString;
CFStringRef someDisk;
CFStringRef mountOrFsckFailed;
CFStringRef unknownString;
CFStringRef mountOrFsckFailedWithDiskUtility;

typedef struct IONotificationMsg IONotificationMsg, * IONotificationMsgPtr;

static void HandleIONotificationMsg( IONotificationMsgPtr ioNotificationMsgPtr );

#define PIPEFULL	(4 * 1024)
static char *
read_output(int fd)
{
	char *		buf = NULL;
	ssize_t 	count;
	ssize_t		where = 0;

	buf = malloc(PIPEFULL);
	if (buf == NULL) {
		return (NULL);
	}

	/* this handles up to PIPEFULL - 1 bytes */
	while (where < (PIPEFULL - 1)
	       && (count = read(fd, buf + where, PIPEFULL - 1 - where))) {
	    if (count == -1) {
		free(buf);
		return (NULL);
	    }
	    where += count;
	}
	buf[where] = '\0';
	return (buf);
}

boolean_t
do_exec(const char * dir, const char * argv[], int * result,
	char * * output)
{
	int 		fdp[2];
	boolean_t	got_result = FALSE;
	int 		pid;

	if (g.debug) {
		const char * * scan;
		printf("do_exec(");
		for (scan = argv; *scan; scan++) {
			printf("%s%s", (scan != argv) ? " " : "", *scan);
		}
		printf(")\n");
	}
	if (output != NULL) {
		*output = NULL;
		if (pipe(fdp) == -1) {
			LogErrorMessage("do_exec(): pipe() failed, %s", 
					strerror(errno));
			return (FALSE);
		}
	}
	if (access(argv[0], F_OK) == 0) {
		pid = fork();
		if (pid == 0) {
			/* CHILD PROCESS */
			if (output == NULL)
				cleanUpAfterFork(NULL);
			else
				cleanUpAfterFork(fdp);

			if (dir) {
				chdir(dir);
			}
			execve(argv[0], (char * const *)argv, 0);
			exit(-127);
		}
		else if (pid > 0) {  /* PARENT PROCESS */
			int statusp;
			int waitResult;
			
			if (output != NULL) {
				close(fdp[1]);
				*output = read_output(fdp[0]);
                                close(fdp[0]);
			}
			dwarning(("wait4(pid=%d,&statusp,0,NULL)...\n", pid));
			waitResult = wait4(pid,&statusp,0,NULL);
			dwarning(("wait4(pid=%d,&statusp,0,NULL) => %d\n", 
				  pid, waitResult));
			if (waitResult > 0
			    && WIFEXITED(statusp)) {
				got_result = TRUE;
				*result = (int)(char)(WEXITSTATUS(statusp));
			}
		}
		else {
			LogErrorMessage("do_exec: fork() failed, %s",
					strerror(errno));
		}
	}
	return (got_result);
}

void 
cleanUpAfterFork(int fdp[])
{
    int fd, maxfd = getdtablesize();

        /* Close all inherited file descriptors */

    for (fd = 0; fd < maxfd; fd++)
    {
	    if (fdp == NULL || (fdp[0] != fd && fdp[1] != fd)) {
		    close(fd);
	    }
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

    (void)setsid();
    
    (void)chdir("/");

    if ((fd = open(_PATH_DEVNULL, O_RDWR, 0)) != -1) {
	    /* point stdin -> /dev/null */
	    (void)dup2(fd, STDIN_FILENO);
	    if (fdp != NULL) {
		    /* point stdout to one end of pipe fdp[1] */
		    (void)dup2(fdp[1], STDOUT_FILENO);

		    /* close the other end */
		    close(fdp[0]);
	    }
	    else {
		    (void)dup2(fd, STDOUT_FILENO);
	    }
	    (void)dup2(fd, STDERR_FILENO);
	    if (fd > 2)
		    (void)close (fd);
    }
    return;
}

int
findDiskInit()
{
	kern_return_t r;
        io_iterator_t ioIterator;  // first match

	r = IOMasterPort(bootstrap_port, &ioMasterPort);
	if (r != KERN_SUCCESS)
	{
		LogErrorMessage("(%s:%d) IOMasterPort failed: {0x%x} %s\n", __FILE__, __LINE__, r, mach_error_string(r));
		return -1;
	}

	r = IOServiceGetMatchingServices(ioMasterPort,
					 IOServiceMatching( "IOMedia" ),
					 &ioIterator);
        if (r != KERN_SUCCESS)
        {
                LogErrorMessage("(%s:%d) IOServiceGetMatching Services: {0x%x} %s\n", __FILE__, __LINE__, r, mach_error_string(r));
                return -1;
        }
        GetDisksFromRegistry(ioIterator, 1, 0);
        IOObjectRelease(ioIterator);
        cacheFileSystemDictionaries();
        cacheFileSystemMatchingArray();
	return (0);
}

void * YankedDiskThread(void * args)
{
        // display UI to inform the user that an disk was yanked badly.
        SInt32			retval = ERR_SUCCESS;
        CFURLRef		daFrameworkURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, CFSTR("/System/Library/PrivateFrameworks/DiskArbitration.framework"), kCFURLPOSIXPathStyle, TRUE);

        // use the url of the DiskArbitration Framework and put a Localizable.strings file in there.

        retval = CFUserNotificationDisplayNotice(0.0, kCFUserNotificationStopAlertLevel, NULL, NULL, daFrameworkURL, yankedHeader, yankedMessage, NULL);

        CFRelease(daFrameworkURL);
        //CFRelease(daBundle);

        return NULL;
}

// Disconnected arbitration messages
void StartYankedDiskMessage()
{
    pthread_attr_t attr;
    pthread_t tid;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, YankedDiskThread, nil);
    pthread_attr_destroy(&attr);
}


/*
	Static functions
*/

//------------------------------------------------------------------------

static kern_return_t InitNotifyPort(void)
{
	kern_return_t r;

	r = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &gNotifyPort);
	if (r != KERN_SUCCESS)
	{
		LogErrorMessage("(%s:%d) mach_port_allocate failed: {0x%x} %s\n", __FILE__, __LINE__, r, mach_error_string(r));
		return MACH_PORT_NULL;
	}

	dwarning(("%s: gNotifyPort = $%08x\n", programName, (int)gNotifyPort));

	return r;

} // InitNotifyPort
	
static mach_port_t GetNotifyPort(void)
{

	return gNotifyPort;
	
} // GetNotifyPort

//------------------------------------------------------------------------

static boolean_t MessageIsNotificationDeath(const mach_msg_header_t *msg, mach_port_t *deadPort)
{
	boolean_t result;
	
	if ( GetNotifyPort() == msg->msgh_local_port )
	{
		if ( MACH_NOTIFY_DEAD_NAME == msg->msgh_id )
		{
			// If the caller supplied a pointer, fill it with the dead port's name
			if ( deadPort != NULL )
			{
				const mach_dead_name_notification_t *deathMessage = (const mach_dead_name_notification_t *)msg;
				*deadPort = deathMessage->not_port;
			}
			result = TRUE;
			goto Return;
		}
                else if ( INTERNAL_MSG == msg->msgh_id) 
                {
                        // do nothing
                        dwarning(("Internal message received"));
						result = FALSE;
						goto Return;
                }
                else
                {
			/* A message for the notification port other than dead name notification */
			// This should never happen.  Log it.
			LogErrorMessage("(%s:%d) received unrecognized message (id=0x%x) on notify port\n", __FILE__, __LINE__, (int)msg->msgh_id);
			result = FALSE;
			goto Return;
		}
	}
	else
	{
		/* Not a message for the notification port */
		result = FALSE;
		goto Return;
	}

Return:
	return result;

} // MessageIsNotificationDeath

//------------------------------------------------------------------------

void LogErrorMessage(const char *format, ...)
{
        va_list ap;
        
        va_start(ap, format);

        // syslog first....
        openlog(programName, LOG_PID | LOG_CONS, LOG_DAEMON);
        // This should be LOG_WARNING instead of LOG_ERR according to "man 3 syslog", but experiments show that would prevent it from appearing in the log.
        vsyslog(LOG_ERR, format, ap);
        closelog();

        // ...printf second
//        fprintf(stderr, "%s [%d]", programName, getpid());
        vfprintf(stderr, format, ap);

        va_end(ap);

} // LogErrorMessage

//------------------------------------------------------------------------

static void HandleIONotificationMsg( IONotificationMsgPtr msg )
{
	kern_return_t r;
	unsigned long int  	notifyType;
	unsigned long int  	ref;
	
	r = OSGetNotificationFromMessage(
		&msg->msgHdr,		//	mach_msg_header_t     * msg,
		0,			//	unsigned int            index,
		&notifyType,		//	unsigned long int     * type,
		&ref,			//	unsigned long int     * reference,
		0,			//	void                 ** content,
		0 );			//	vm_size_t             * size
	if ( r )
	{
		dwarning(("%s(%d): OSGetNotificationFromMessage returned error %d\n", __FUNCTION__, __LINE__, (int)r));
		goto Return;
	}

	// we passed a string for the refcon
	dwarning(("got notification, type=%d(%s), local=%d, remote=%d\n",
			(int)notifyType, (char*)ref,
			(int)msg->msgHdr.msgh_local_port,
			(int)msg->msgHdr.msgh_remote_port ));
			
	// remote port is the notification (an iterator_t) that fired

        if (notifyType != 102) {  // anything except terminate
            GetDisksFromRegistry( (io_iterator_t) msg->msgHdr.msgh_remote_port, 0, 0 );

            autodiskmount(FALSE);
        } else {
            io_iterator_t iter = (io_iterator_t) msg->msgHdr.msgh_remote_port;

            io_registry_entry_t	entry;

            while ( (entry = IOIteratorNext( iter )) )
            {
                kern_return_t		kr;
                CFDictionaryRef properties = 0; // (needs release)
                CFStringRef     string     = 0; // (don't release)
                io_name_t		ioMediaName;
                char *          ioBSDName  = NULL; // (needs release)

                DiskPtr dp;


                kr = IORegistryEntryGetName(entry, ioMediaName);
                if ( KERN_SUCCESS != kr )
                {
                        dwarning(("can't obtain name for media object\n"));
                        continue;
                }


                kr = IORegistryEntryCreateCFProperties(entry, (CFMutableDictionaryRef *)&properties, kCFAllocatorDefault, kNilOptions);
                if ( KERN_SUCCESS != kr )
                {
                        dwarning(("can't obtain properties for '%s'\n", ioMediaName));
                        continue;
                }

                assert(CFGetTypeID(properties) == CFDictionaryGetTypeID());

                // BSDName

                string = (CFStringRef) CFDictionaryGetValue(properties, CFSTR(kIOBSDNameKey));

                if (!string) {
                        // whoa no kIOBSDName.  Ouch.  get out now.
                        dwarning(("can't obtain properties for ioBSDName\n"));
                        continue;
                }
                
                ioBSDName = daCreateCStringFromCFString(string);
                // figure out the disk and see if it exists, and if so, remove it ...

                /* Is there an existing disk on our list with this IOBSDName? */

                dp = LookupDiskByIOBSDName( ioBSDName );
                
                if (dp != NULL) {  // disk has been forcefully removed in some strange manner
                        DiskPtr wholeDiskPtr = LookupWholeDiskForThisPartition( dp );

                        if (dp->mountpoint && strlen(dp->mountpoint)) {
                                if (wholeDiskPtr && !wholeDiskPtr->wholeDiskHasBeenYanked) {
                                        wholeDiskPtr->wholeDiskHasBeenYanked++;
                                        StartYankedDiskMessage();
                               }
                        }
                        
                        kr = UnmountDisk( dp, TRUE );
                    	SendUnmountPostNotifyMsgsForOnePartition( dp->ioBSDName, 0, 0 );
                    	SendEjectPostNotifyMsgsForOnePartition( dp->ioBSDName, 0, 0 );

                        // Radar 2647679
                        // don't free the whole disk associated with a yanked disk just yet, free it later in the run loop
                        // that way we know it's always here for a disk that gets in just a we bit later.
                        if (!dp->wholeDiskHasBeenYanked) {
                                FreeDisk( dp );
                        }

                }
                IOObjectRelease(entry);
                if ( properties )	CFRelease( properties );
                if ( ioBSDName )	free( ioBSDName );
            }
        }


Return:
	return;

} // HandleIONotificationMsg

//------------------------------------------------------------------------

/*
	Public functions
*/

//------------------------------------------------------------------------

/*
-- Allocates memory for new client record.
-- Links it into the existing list.
-- Fills in the <port> and <flags> fields and sets <state>.
-- Increments the global count of client records.
*/

ClientPtr NewClient( mach_port_t port, unsigned pid, unsigned flags )
{
	ClientPtr result;

	/* Allocate memory */

	result = (ClientPtr) malloc( sizeof( * result ) );
	if ( result == NULL )
	{
		dwarning(("%s(port = $%08x, pid = %d, flags = $%08x): malloc failed!\n", __FUNCTION__, port, pid, flags));
		/* result = NULL; */
		goto Return;
	}

	/* Link it onto the front of the list */

	result->next = g.Clients;
	g.Clients = result;

	/* Fill in the fields */

	result->port = port;
	result->pid = pid;
	result->flags = flags;
	result->state = kClientStateNew;
	result->numAcksRequired = 0;
        result->notifyOnDiskTypes = 0;
        result->unrecognizedPriority = 0;
        result->ackOnUnrecognizedDisk = nil;
        result->clientAuthRef = nil;
	
	/* Increment count */

	g.NumClients ++ ;
	
Return:
	return result;

} // NewClient

//------------------------------------------------------------------------

void PrintClient(ClientPtr clientPtr)
{
	if ( ! clientPtr ) return;

	dwarning(("port = $%08x, pid = %5d, flags = $%08x, numAcksRequired = %d\n", clientPtr->port, clientPtr->pid, clientPtr->flags, clientPtr->numAcksRequired));

} // PrintClients

//------------------------------------------------------------------------

void PrintClients(void)
{
	ClientPtr clientPtr;
	int i;
	
	if ( ! gDebug ) goto Return;

	dwarning(("g.NumClients = %d\n", g.NumClients));
	for (clientPtr = g.Clients, i = 0; clientPtr != NULL; clientPtr = clientPtr->next, i++)
	{
		PrintClient( clientPtr );
	}

Return:
	return;

} // PrintClients

//------------------------------------------------------------------------

ClientPtr LookupClientByPID( pid_t pid )
{
	ClientPtr clientPtr = nil;
	
	for (clientPtr = g.Clients; clientPtr != NULL; clientPtr = clientPtr->next)
	{
		if ( clientPtr->pid == pid )
		{
			goto Return;
		}
	}

Return:
	return clientPtr;

} // LookupClientByPID

ClientPtr LookupClientByMachPort( mach_port_t port )
{
        ClientPtr clientPtr;

        for (clientPtr = g.Clients; clientPtr != NULL; clientPtr = clientPtr->next)
        {
                if ( clientPtr->port == port )
                {
                        goto Return;
                }
        }

Return:
        return clientPtr;

} // LookupClientByPID


//------------------------------------------------------------------------

/*
-- Allocates memory for new disk record.
-- Links it into the existing list.
-- Fills in the <ioBSDName>, <ioContent>, <mountpoint>, <ioDeviceTreePath>, and <flags> fields and sets <state> = kDiskStateNew.
-- Makes its own copies of input strings - doesn't retain pointers to its arguments.
-- Increments the global count of disk records.
*/

DiskPtr NewDisk(	char * ioBSDName,
					int ioBSDUnit,
					char * ioContentOrNull,
					DiskFamily family,
					char * mountpoint,
					char * ioMediaNameOrNull,
					char * ioDeviceTreePathOrNull,
                                        io_object_t	service,
                                        int	mountingUserFromDevice,
					unsigned flags,
					UInt64 ioSize)
{
	DiskPtr result;
	
	dwarning(("%s(ioBSDName = '%s', ioContentOrNull = '%s', family = %d, mountpoint = '%s', ioMediaNameOrNull = '%s', ioDeviceTreePathOrNull = '%s', flags = $%08x, owner = %d)\n",
				__FUNCTION__,
				ioBSDName,
				ioContentOrNull ? ioContentOrNull : "(NULL)",
				family,
				mountpoint,
				ioMediaNameOrNull ? ioMediaNameOrNull : "(NULL)",
				ioDeviceTreePathOrNull ? ioDeviceTreePathOrNull : "(NULL)",
           flags, mountingUserFromDevice ));

	/* Allocate memory */

	result = (DiskPtr) malloc( sizeof( * result ) );
	if ( result == NULL )
	{
		dwarning(("%s(...): malloc failed!\n", __FUNCTION__));
		/* result = NULL; */
		goto Return;
	}

	bzero( result, sizeof( * result ) );

	/* Link it onto the front of the list */

	result->next = g.Disks;
	g.Disks = result;

	/* Fill in the fields */

        result->mountAttempted = 0;
	result->ioBSDName = strdup( ioBSDName ? ioBSDName : "" );
	result->ioBSDUnit = ioBSDUnit;
	result->ioContent = strdup( ioContentOrNull ? ioContentOrNull : "" );
	result->family = family;
	result->mountpoint = strdup( mountpoint ? mountpoint : "" );
        result->service = service;
        result->retainingClient = 0;
        result->lastClientAttemptedForUnrecognizedMessages = nil;
        result->wholeDiskContainsMountedChild = 0;
        result->wholeDiskHasBeenYanked = 0;
        result->admCreatedMountPoint = 0;
        result->ejectOnLogout = 0;
	   result->ioSize = ioSize;
        result->approvedForMounting = 0;
        result->forceTakeOwnershipOnDisk = 0;
        result->volName = strdup( "" );
        result->timeAppeared = CFAbsoluteTimeGetCurrent();

	if ( ioMediaNameOrNull )
	{
	    result->ioMediaNameOrNull = strdup( ioMediaNameOrNull );
	}
	else
	{
	    result->ioMediaNameOrNull = NULL;
	}
	
	result->ioDeviceTreePath = strdup( ioDeviceTreePathOrNull ? ioDeviceTreePathOrNull : "" );
	
	result->flags = flags;

	/* Assign a sequence number if mounted, otherwise -1.   See DiskSetMountpoint(). */

	result->sequenceNumber = ( 0 == strcmp( result->mountpoint, "" ) ) ? -1 : NextSequenceNumber();

	result->state = kDiskStatePostponed;
	result->ackValues = NULL;

        if ( mountingUserFromDevice != -1) {
                result->mountedUser = mountingUserFromDevice;
        } else {
                result->mountedUser = currentConsoleUser;  // -1 if noone logged in on the console
        }
        result->wholeDiskContainsMountedChild = 0;
	
	/* Increment count */

	g.NumDisks ++ ;
	g.NumDisksAddedOrDeleted ++;

	/* Retain service */

	if ( service )
	{
		IOObjectRetain( service );
	}

Return:
	return result;

} // NewDisk


//------------------------------------------------------------------------


boolean_t IsWhole( DiskPtr diskPtr )
{
	boolean_t result;
	
	result = ( 0 != ( diskPtr->flags & kDiskArbDiskAppearedWholeDiskMask ) );
	
	return result;

} // IsWhole


//------------------------------------------------------------------------


boolean_t IsNetwork( DiskPtr diskPtr )
{
	boolean_t result;
	
	result = ( 0 != ( diskPtr->flags & kDiskArbDiskAppearedNetworkDiskMask ) );
	
	return result;

} // IsNetwork


//------------------------------------------------------------------------

/*
-- Allocate a sequence number for use by Carbon and BlueBox to synchronize
-- their vRefNums.
-- Use approximately the same algorithm as MacOS uses for assigning drive
-- numbers: find the largest so far, and then add one.
-- This will leave gaps in the allocation, but will eventually fill in
-- those gaps.
-- It will also tend not to reuse numbers too quickly, thus avoiding the
-- possibility of having an application access the wrong volume by accident.
-- An unmounted disk has a sequence number of -1.
-- The first sequence number is 0.
*/

static int NextSequenceNumber( void )
{
	int maxSequenceNumber;
	int result;
	DiskPtr diskPtr;

	dwarning(("%s...\n", __FUNCTION__));

	maxSequenceNumber = -1; /* Thus the first sequence number will be zero. */

	for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = diskPtr->next)
	{
		if ( diskPtr->sequenceNumber > maxSequenceNumber )
		{
			maxSequenceNumber = diskPtr->sequenceNumber;
		}	
	}

	/* No other disk has this sequence number. */

	result = maxSequenceNumber + 1;

//Return:

	dwarning(("%s => %d\n", __FUNCTION__, result ));
	return result;

} // NextSequenceNumber

//------------------------------------------------------------------------

/*
-- This is called from autodiskmount.m when it either (a) discovers that
-- the disk is already mounted, or (b) explicitly mounts the disk.
-- It also triggers the assignment of a sequence number, in an attempt
-- to conserve sequence numbers by not assigning them to every disk.
-- (Most partitions are never mounted.)
*/

void DiskSetMountpoint( DiskPtr diskPtr, const char * mountpoint )
{
	if ( diskPtr->mountpoint )
	{
		free( diskPtr->mountpoint );
	}

	diskPtr->mountpoint = strdup( mountpoint ? mountpoint : "" );

	diskPtr->sequenceNumber = NextSequenceNumber();

} // DiskSetMountpoint

void DiskSetVolumeName( DiskPtr diskPtr, const char * volName )
{
    if ( diskPtr->volName )
    {
        free( diskPtr->volName );
    }

    diskPtr->volName = strdup( volName ? volName : "" );

}

//------------------------------------------------------------------------

DiskPtr LookupDiskByIOBSDName( char * ioBSDName )
{
	DiskPtr diskPtr;
	
//	dwarning(("%s('%s')\n", __FUNCTION__,  ioBSDName));

        if (!ioBSDName) {
        	pwarning(("%s(null ioBSDName passed! abort!)\n", __FUNCTION__));
        	return nil;
        }


	for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = diskPtr->next)
	{
                if (!diskPtr->ioBSDName) {
                // something went very wrong, skip this disk
                	continue;
                }
		if ( 0 == strcmp( ioBSDName, diskPtr->ioBSDName ) )
		{
			goto Return;
		}
	}

	/* Assert: diskPtr == NULL */

Return:
	return diskPtr;

} // LookupDiskByIOBSDName

void FreeAllDisks(  )
{
    DiskPtr oldDiskPtr= g.Disks;
    DiskPtr newDiskPtr= g.Disks;

    while (oldDiskPtr) {
        newDiskPtr = oldDiskPtr->next;
        FreeDisk(oldDiskPtr);
        oldDiskPtr = newDiskPtr;
    }

} // FreeAllDisks

//------------------------------------------------------------------------

/*
-- Unmount the disk and delete the mountpoint that was created for it
*/

int UnmountDisk( DiskPtr diskPtr, int forceUnmount )
{
	int result = 0;
        struct stat 	sb;
        struct statfs 	mntbuf;
        char		cookieFile[MAXPATHLEN];

        sprintf(cookieFile, "/%s/%s", diskPtr->mountpoint, ADM_COOKIE_FILE);
       
	dwarning(("%s('%s')\n", __FUNCTION__, diskPtr->ioBSDName));


	if ( 0 == strcmp( diskPtr->mountpoint, "" ) )
	{
		dwarning(("%s('%s'): disk not mounted\n", __FUNCTION__, diskPtr->ioBSDName));
		SetStateForOnePartition( diskPtr, kDiskStateIdle );
		/* result == 0 */
		goto Return;
	}

        /* need to stat and get the mount flags for this mountpoint and make sure it wasn't automounted
                If it was automounted (MNT_AUTOMOUNTED), just skip out of here */
        {
                if (statfs(diskPtr->mountpoint, &mntbuf) == 0) {
                        if (mntbuf.f_flags & MNT_AUTOMOUNTED) {
                                // do not unmount this mount
                                goto Return;
                        }
                }
        }


        result = unmount( diskPtr->mountpoint, (forceUnmount ? MNT_FORCE : 0) );
	if ( -1 == result )
	{
		result = errno;
		LogErrorMessage("%s('%s') unmount('%s') failed: %d (%s)\n", __FUNCTION__, diskPtr->ioBSDName, diskPtr->mountpoint, result, strerror(result));
		SetStateForOnePartition( diskPtr, kDiskStateIdle );
		goto Return;
	}

        if (diskPtr->admCreatedMountPoint) {
                result = rmdir( diskPtr->mountpoint );
                if ( -1 == result )
                {
                        result = errno;

                        if (result == ENOTEMPTY) {
                                if (stat(cookieFile, &sb) == 0) {
                                        if (remove(cookieFile) == 0) {
                                                if (rmdir(diskPtr->mountpoint) != 0) {
                                                        LogErrorMessage("%s('%s') rmdir('%s') failed: %d (%s)\n", __FUNCTION__, diskPtr->ioBSDName, diskPtr->mountpoint, result, strerror(result));
                                                        /* Continue despite this error. */
                                                        result = errno;
                                                } else {
                                                    result = 0;
                                                }
                                        }
                                }
                        }
                }
        }

        DiskSetMountpoint(diskPtr, ""); // We need to send this in messages via MIG, so it cannot be NULL

	SetStateForOnePartition( diskPtr, kDiskStateNewlyUnmounted );

Return:
	if ( result ) dwarning(("%s('%s') error: %d (%s)\n", __FUNCTION__, diskPtr->ioBSDName, result, strerror(result)));
	return result;

} // UnmountDisk

//------------------------------------------------------------------------

/*
-- For each disk, <d>, that comes from the same device (i.e., whose "family"
-- and "device number" match the given disk), call Unmount(<d>).
-- Ignore errors - attempt to unmount each partition.  This should help with
-- debugging problems, since only the "problem" partition(s) will remain mounted.
--
-- Handle network volumes (kDiskArbDiskAppearedNetworkDiskMask) specially:
-- do not try to parse their names, and only unmount the one disk.
--
*/

int UnmountAllPartitions( DiskPtr diskPtr, int forceUnmount )
{
	int result = 0; /* mandatory initialization */
	DiskPtr dp;
	int	family1, family2;
	int	deviceNum1, deviceNum2;

        if (diskPtr->ioBSDName && strlen(diskPtr->ioBSDName)) {
                dwarning(("%s('%s')\n", __FUNCTION__, diskPtr->ioBSDName));
        }
	

	if ( IsNetwork( diskPtr ) )
	{
            result = UnmountDisk( diskPtr, forceUnmount ); // Ignore errors - attempt to unmount each partition
		goto Return;
	}

	family1 = diskPtr->family;
	deviceNum1 = diskPtr->ioBSDUnit;

	for (dp = g.Disks; dp != NULL; dp = dp->next)
	{
		family2 = dp->family;
		deviceNum2 = dp->ioBSDUnit;

		if ( family1 == family2 && deviceNum1 == deviceNum2 )
		{
			int err;

                    err = UnmountDisk( dp , forceUnmount); // Ignore errors - attempt to unmount each partition
			if ( err && ! result )
			{
				/* Only return the first error we encounter */
				result = err;
			}
		}
	}
	
Return:
	return result;

} // UnmountAllPartitions

//------------------------------------------------------------------------

/*
-- Eject the disk.
-- Precondition: the disk must not be in use (e.g., by a mounted filesystem)
--
-- Handle network volumes (kDiskArbDiskAppearedNetworkDiskMask) specially:
-- do not try to parse their names, and just mark them as ejected (this routine
-- only gets called if the unmount succeeded).
--
*/

int EjectDisk( DiskPtr diskPtr )
{
	int		result = 0; /* mandatory initialization */
    int		fd = -1; /* mandatory initialization */
    char	livePartitionPathname[PATH_MAX];
	DiskPtr	wholeDiskPtr;

	dwarning(("%s('%s')\n", __FUNCTION__, diskPtr->ioBSDName));

	if ( IsNetwork( diskPtr ) )
	{
		/* If the unmount succeeded, then mark the disk as ejected. */
		if ( 0 == strcmp( "", diskPtr->mountpoint ) )
		{
			SetStateForOnePartition( diskPtr, kDiskStateNewlyEjected );
		}
		goto Return; /* result == 0 */
	}

	/* Find the correct LIVE partition (whole media) for this device */

	wholeDiskPtr = LookupWholeDiskForThisPartition( diskPtr );
	if ( NULL == wholeDiskPtr )
	{
		result = EINVAL;
		LogErrorMessage("%s('%s') can't find whole media\n", __FUNCTION__, diskPtr->ioBSDName);
		SetStateForAllPartitions( diskPtr, kDiskStateIdle );
		goto Return;
	}
	
	sprintf(livePartitionPathname, "/dev/r%s", wholeDiskPtr->ioBSDName);

	dwarning(("%s: livePartitionPathname = '%s'\n", __FUNCTION__, livePartitionPathname));

	/* IOKit has requested that we open with O_RDONLY and not use O_NDELAY */

    fd = open(livePartitionPathname, O_RDONLY);
    if ( -1 == fd )
    {
    	result = errno;
		LogErrorMessage("%s('%s') open('%s') failed: %d (%s)\n", __FUNCTION__, diskPtr->ioBSDName, livePartitionPathname, result, strerror(result));
		SetStateForAllPartitions( diskPtr, kDiskStateIdle );
    	goto Return;
    }

    if ( 0 != ( diskPtr->flags & kDiskArbDiskAppearedEjectableMask ) )
    {
        /* Mark each partition on this disk with <state> = kDiskStateNewlyEjected. */
        SetStateForAllPartitions( diskPtr, kDiskStateNewlyEjected );
    }
    else
    {
        /* Mark each partition on this disk with <state> = kDiskStateIdle. */
        SetStateForAllPartitions( diskPtr, kDiskStateIdle );
    }

	result = ioctl(fd, DKIOCEJECT, 0);
	if ( -1 == result )
	{
                
                result = errno;

                LogErrorMessage("%s('%s') ioctl(DKIOCEJECT) failed: %d (%s)\n", __FUNCTION__, diskPtr->ioBSDName, result, strerror(result));
                SetStateForAllPartitions( diskPtr, kDiskStateIdle );
                goto Return;
	}


Return:

	if ( fd >= 0 )
	{
		close( fd );
	}

//	if ( result ) dwarning(("%s('%s') error: %d (%s)\n", __FUNCTION__, diskPtr->ioBSDName, result, strerror(result)));
	return result;

} // EjectDisk

//------------------------------------------------------------------------

/*
-- Unlink the given disk record from the global list and free the strings
-- it points to as well as the record itself.
*/

void FreeDisk( DiskPtr diskPtr )
{
	DiskPtr dp;
	DiskPtr * dpp;

	dwarning(("%s('%s')\n", __FUNCTION__, diskPtr->ioBSDName));
	
	for (	dpp = & g.Disks, dp = * dpp;
			dp != NULL;
			dpp = & (dp->next), dp = * dpp )
	{
		if ( dp == diskPtr )
		{
			*dpp = dp->next;

			free( diskPtr->ioBSDName );
			diskPtr->ioBSDName = NULL; // Attempt to catch accidental re-use.
			free( diskPtr->ioContent );
			diskPtr->ioContent = NULL; // Attempt to catch accidental re-use.
                        free( diskPtr->mountpoint );
                        diskPtr->mountpoint = NULL; // Attempt to catch accidental re-use.
                        free( diskPtr->volName );
                        diskPtr->volName = NULL; // Attempt to catch accidental re-use.
			if ( diskPtr->ioMediaNameOrNull)
			{
			    free( diskPtr->ioMediaNameOrNull);
				diskPtr->ioMediaNameOrNull = NULL; // Attempt to catch accidental re-use.
			}
			free( diskPtr->ioDeviceTreePath );
			diskPtr->ioDeviceTreePath = NULL; // Attempt to catch accidental re-use.

                        if ( diskPtr->mountedFilesystemName)
                        {
                                free( diskPtr->mountedFilesystemName);
                                diskPtr->mountedFilesystemName = NULL; // Attempt to catch accidental re-use.
                        }

			if ( diskPtr->ackValues )
			{
				FreeAckValues( diskPtr->ackValues );
			}
			diskPtr->ackValues = NULL; // Attempt to catch accidental re-use.

			if ( diskPtr->service )
			{
				IOObjectRelease( diskPtr->service );
			}

			free( diskPtr );
                        
			g.NumDisks --;
			g.NumDisksAddedOrDeleted ++;
			
			goto Return;

		}
	}

	/* If we get here, it means we couldn't find <diskPtr> in the list */

Return:
	return;

} // FreeDisk

//------------------------------------------------------------------------

void PrintDisks(void)
{
	DiskPtr diskPtr;
	int i;

	if ( ! gDebug ) goto Return;

	dwarning(("g.NumDisks = %d\n", g.NumDisks));
	dwarning(("g.NumDisksAddedOrDeleted = %d\n", g.NumDisksAddedOrDeleted));
        dwarning(("%2s %4s %10s %9s %5s %8s %25s %35s %8s %40s %35s %3s\n", "#", "seq#", "BSDName", "flags", "state", "BSDUnit", "Content", "FS Type", "MediaName", "DeviceTreePath", "mountpoint", "uid"));
	for (diskPtr = g.Disks, i = 0; diskPtr != NULL; diskPtr = diskPtr->next, i++)
	{
		dwarning(("%2d %4d %10s $%08x %5d %8d %25s %35s %8s %40s %35s %3d\n",
					i,
					diskPtr->sequenceNumber,
					diskPtr->ioBSDName,
					diskPtr->flags,
					diskPtr->state,
					diskPtr->ioBSDUnit,
                                        diskPtr->ioContent,
                                        diskPtr->mountedFilesystemName,
					diskPtr->ioMediaNameOrNull ? diskPtr->ioMediaNameOrNull : "<null>",
					diskPtr->ioDeviceTreePath,
					diskPtr->mountpoint ? diskPtr->mountpoint : "<not mounted>",
                                        diskPtr->mountedUser
		));
		PrintAckValues( diskPtr->ackValues );
	}

Return:
	return;

} // PrintDisks


//------------------------------------------------------------------------


int IsStateNeedingAckValueTable( DiskState diskState );
int IsStateNeedingAckValueTable( DiskState diskState )
{
	switch ( diskState )
	{
		case kDiskStateToBeEjected:
		case kDiskStateToBeUnmounted:
        case kDiskStateToBeUnmountedAndEjected:
        case kDiskStateWaitingForMountApproval:
			return 1;
		break;

		default:
			return 0;
		break;
	}

} // IsStateNeedingAckValueTable


//------------------------------------------------------------------------


void SetStateForOnePartition( DiskPtr diskPtr, DiskState newState )
{

	dwarning(("%s('%s',state=%s (%d))\n", __FUNCTION__, diskPtr->ioBSDName, DISKSTATE(newState),newState));

	/* Free an existing ack value table. */

	if ( diskPtr->ackValues )
	{
		FreeAckValues( diskPtr->ackValues );
	}

	diskPtr->state = newState;

	/* Allocate a table to record the values returned by the interested clients. */

	if ( IsStateNeedingAckValueTable( newState ) )
	{
		diskPtr->ackValues = NewAckValues( NumClientsDesiringAsyncNotification() );
	}
	else
	{
		diskPtr->ackValues = NULL;
	}

} // SetStateForOnePartition


//------------------------------------------------------------------------


/*
-- For each disk, <d>, that comes from the same device (i.e., whose "family"
-- and "device number" match the given disk), set its state.
--
-- Handle network volumes (kDiskArbDiskAppearedNetworkDiskMask) specially.
--
*/

void SetStateForAllPartitions( DiskPtr diskPtr, DiskState newState )
{
	DiskPtr diskPtr2;
	int	family1, family2;
	int	deviceNum1, deviceNum2;
	
	dwarning(("%s('%s',newState=%s (%d))\n", __FUNCTION__, diskPtr->ioBSDName, DISKSTATE(newState),newState));

	if ( IsNetwork( diskPtr ) )
	{
		SetStateForOnePartition( diskPtr, newState );
		goto Return;
	}

	family1 = diskPtr->family;
	deviceNum1 = diskPtr->ioBSDUnit;

	for (diskPtr2 = g.Disks; diskPtr2 != NULL; diskPtr2 = diskPtr2->next)
	{
		family2 = diskPtr2->family;
		deviceNum2 = diskPtr2->ioBSDUnit;

		if ( family1 == family2 && deviceNum1 == deviceNum2 )
		{
			SetStateForOnePartition( diskPtr2, newState );
		}
	}

Return:
	return;
	
} // SetStateForAllPartitions


//------------------------------------------------------------------------


/* inclusive */

int NumPartitionsMountedFromThisDisk( DiskPtr diskPtr )
{
	int result = 0; // mandatory initialization
	DiskPtr diskPtr2;
	int	family1, family2;
	int	deviceNum1, deviceNum2;
	
	if ( IsNetwork( diskPtr ) )
	{
		result = ( 0 != strcmp("", diskPtr->mountpoint) );
		goto Return;
	}

	family1 = diskPtr->family;
	deviceNum1 = diskPtr->ioBSDUnit;

	for (diskPtr2 = g.Disks; diskPtr2 != NULL; diskPtr2 = diskPtr2->next)
	{
		family2 = diskPtr2->family;
		deviceNum2 = diskPtr2->ioBSDUnit;

		if ( family1 == family2 && deviceNum1 == deviceNum2 && 0 != strcmp( "", diskPtr2->mountpoint ) )
		{
			result ++ ;
		}
	}

Return:
	dwarning(("%s('%s') => %d\n", __FUNCTION__, diskPtr->ioBSDName, result));
	return result;
	
} // NumPartitionsMountedFromThisDisk


//------------------------------------------------------------------------


/* inclusive */

int NumPartitionsToBeUnmountedAndEjectedFromThisDisk( DiskPtr diskPtr )
{
	int result = 0; // mandatory initialization
	DiskPtr diskPtr2;
	int	family1, family2;
	int	deviceNum1, deviceNum2;

	if ( IsNetwork( diskPtr ) )
	{
		result = ( 0 != strcmp("", diskPtr->mountpoint) );
		goto Return;
	}

	family1 = diskPtr->family;
	deviceNum1 = diskPtr->ioBSDUnit;

	for (diskPtr2 = g.Disks; diskPtr2 != NULL; diskPtr2 = diskPtr2->next)
	{
		family2 = diskPtr2->family;
		deviceNum2 = diskPtr2->ioBSDUnit;

		if ( family1 == family2 && deviceNum1 == deviceNum2 && diskPtr2->state == kDiskStateToBeUnmountedAndEjected )
		{
			result ++ ;
		}
	}

Return:
	dwarning(("%s('%s') => %d\n", __FUNCTION__, diskPtr->ioBSDName, result));
	return result;
	
} // NumPartitionsToBeUnmountedAndEjectedFromThisDisk


//------------------------------------------------------------------------


DiskPtr LookupWholeDiskForThisPartition( DiskPtr diskPtr )
{
	DiskPtr diskPtr2;
	int	family1, family2;
	int	deviceNum1, deviceNum2;

	if ( IsNetwork( diskPtr ) )
	{
		diskPtr2 = diskPtr;
		goto Return;
	}

	family1 = diskPtr->family;
	deviceNum1 = diskPtr->ioBSDUnit;

	for (diskPtr2 = g.Disks; diskPtr2 != NULL; diskPtr2 = diskPtr2->next)
	{
		family2 = diskPtr2->family;
		deviceNum2 = diskPtr2->ioBSDUnit;

		if ( family1 == family2 && deviceNum1 == deviceNum2 && IsWhole( diskPtr2 ) )
		{
			goto Return;
		}
	}

	/* diskPtr2 == NULL */

Return:
	if ( diskPtr2 )
	{
		dwarning(("%s('%s') => '%s'\n", __FUNCTION__, diskPtr->ioBSDName, STR(diskPtr2->ioBSDName)));
	}
	else
	{
		dwarning(("%s('%s') => NULL\n", __FUNCTION__, diskPtr->ioBSDName));
	}
	return diskPtr2;
	
} // LookupWholeDiskForThisPartition


//------------------------------------------------------------------------


void LookupWholeDisksForThisPartition(io_registry_entry_t service, LookupWholeDisksForThisPartitionApplierFunction applier)
{
    io_iterator_t parents;
    kern_return_t status;

    if ( IOObjectConformsTo(service, kIOMediaClass) )
    {
        CFBooleanRef whole = IORegistryEntryCreateCFProperty(service, CFSTR(kIOMediaWholeKey), kCFAllocatorDefault, 0);

        if ( whole )
        {
            if ( whole == kCFBooleanTrue )
            {
                DiskPtr diskPtr;

                for ( diskPtr = g.Disks; diskPtr != NULL; diskPtr = diskPtr->next )
                {
                    if ( IOObjectIsEqualTo(diskPtr->service, service) )
                    {
                        (*applier)(diskPtr);
                        break;
                    }
                }
            }

            CFRelease(whole);
        }
    }
    else if ( IOObjectConformsTo(service, "IOBlockStorageDevice") )
    {
        return;
    }

    status = IORegistryEntryGetParentIterator(service, kIOServicePlane, &parents);
    
    if ( status == KERN_SUCCESS )
    {
        while ( (service = IOIteratorNext(parents)) )
        {
            LookupWholeDisksForThisPartition(service, applier);
            IOObjectRelease(service);
        }

        IOObjectRelease(parents);
    }
} // LookupWholeDiskForThisPartition


//------------------------------------------------------------------------


DiskPtr LookupWholeDiskToBeEjected( void )
{
	DiskPtr diskPtr;
	
	for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = diskPtr->next)
	{
		/* Skip it if it is not a whole disk. */

		if ( ! ( IsWhole( diskPtr) || IsNetwork( diskPtr ) ) )
		{
			continue;
		}
		
		/* Skip it if it is not scheduled to be ejected. */

		if ( ! ( ( diskPtr->state == kDiskStateToBeUnmountedAndEjected ) || ( diskPtr->state == kDiskStateToBeEjected ) ) )
		{
			continue;
		}

		/* If we have not weeded it out, then it is a whole disk that is scheduled to be ejected. */

		goto Return;
	}

	/* diskPtr == NULL */

Return:
	if ( diskPtr )
	{
		dwarning(("%s() => '%s' (state=%s (%d))\n", __FUNCTION__, STR(diskPtr->ioBSDName), DISKSTATE(diskPtr->state), diskPtr->state));
	}
	else
	{
		dwarning(("%s() => NULL\n", __FUNCTION__));
	}
	return diskPtr;
	
} // LookupWholeDiskToBeEjected


//------------------------------------------------------------------------


/* Enable dead name notifications for the given port (send them to our global notification port) */

kern_return_t EnableDeathNotifications(mach_port_t port)
{
	kern_return_t r;
	mach_port_t oldPort;
	
	dwarning(("%s($%08x)\n", __FUNCTION__, port));
	
	r = mach_port_request_notification(mach_task_self(), port, MACH_NOTIFY_DEAD_NAME, 0, GetNotifyPort(), MACH_MSG_TYPE_MAKE_SEND_ONCE, &oldPort);
	if ( r != KERN_SUCCESS)
	{
		/* 5/20/99 - This can happen if the client died after sending the message but before we received it */
		LogErrorMessage("(%s:%d) failed to set up death notifications for port %d: {0x%x} %s\n", __FILE__, __LINE__, (int)port, r, mach_error_string(r));
	}
	
	return r;

} // EnableDeathNotifications


//------------------------------------------------------------------------


AckValues * NewAckValues( int size )
{
	AckValue	*	v;
	AckValues	*	result;
	
	dwarning(("%s(%d)\n", __FUNCTION__, size));

	v = (AckValue *)malloc( sizeof( AckValue ) * size );
	if ( v == NULL )
	{
		result = NULL;
		goto Return;
	}
	
	bzero( v, sizeof( AckValue ) * size );
	
	result = (AckValues *)malloc( sizeof( AckValues ) );
	if ( result == NULL )
	{
		free( v );
		goto Return;
	}
	
	bzero ( result, sizeof( AckValues ) );
	
	result->physicalLength = size;
	result->logicalLength = 0;
	result->ackValues = v;
	
Return:
	return result;

} // NewAckValues


void FreeAckValues( AckValues * ackValues )
{
	dwarning(("%s(0x%08x)\n", __FUNCTION__, (unsigned)ackValues));

	if ( ! ackValues ) goto Return;
	if ( ! ackValues->ackValues ) goto Return;
	
	if ( ackValues->ackValues )
	{
		free( ackValues->ackValues );
		ackValues->ackValues = NULL;
	}
	free( ackValues );
	
Return:
	return;

} // FreeAckValues

void ClearAckValuesForAllDisks()
{
        DiskPtr diskPtr;
        AckValues * p;
        int i;

        dwarning(("%s\n", __FUNCTION__));
        
        for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = diskPtr->next)
        {
                if ((p = diskPtr->ackValues) )
                {
                        for (i = 0; i < p->logicalLength; i++)
                        {
                                p->ackValues[ i ].state = kAckReceived;
                                p->ackValues[ i ].errorCode = 0;
                        }
                }
        } // FOREACH disk

        return;

} // ClearAckValuesForAllDisks

void InitAckValue( AckValues * p, pid_t pid )
{
	dwarning(("%s(0x%08x, pid=%d)\n", __FUNCTION__, (unsigned)p, (unsigned)pid));

	if ( p->logicalLength < p->physicalLength )
	{
		p->ackValues[ p->logicalLength ].pid = pid;
		p->ackValues[ p->logicalLength ].state = kSendMsg;
		p->ackValues[ p->logicalLength ].errorCode = 0;
	
		p->logicalLength ++ ;
	}
	else
	{
		LogErrorMessage("(%s:%d) %s(0x%08x, pid=%d): error: ! ( p->logicalLength < p->physicalLength )\n", __FILE__, __LINE__, __FUNCTION__, (unsigned)p, (unsigned)pid);
	}
	
} // InitAckValue


void UpdateAckValue( AckValues * p, pid_t pid, int errorCode )
{
	int i;
	
	dwarning(("%s(0x%08x, pid=%d, errorCode=%d)\n", __FUNCTION__, (unsigned)p, (unsigned)pid, errorCode));

	if ( ! p )
	{
		LogErrorMessage("(%s:%d) %s(0x%08x, pid=%d, errorCode=%d): null pointer\n", __FILE__, __LINE__, __FUNCTION__, (unsigned)p, (unsigned)pid, errorCode);
		goto Return;
	}

	for (i = 0; i < p->logicalLength; i++)
	{
		if ( p->ackValues[ i ].pid == pid )
		{
			if ( p->ackValues[ i ].state == kWaitingForAck )
			{
				p->ackValues[ i ].state = kAckReceived;
			}
			else
			{
				LogErrorMessage("(%s:%d) %s(0x%08x, pid=%d, errorCode=%d): error: state != kWaitingForAck\n", __FILE__, __LINE__, __FUNCTION__, (unsigned)p, (unsigned)pid, errorCode);
			}
			p->ackValues[ i ].errorCode = errorCode;
			goto Return;
		}
	}
	
	dwarning(("%s: ERROR, pid=%d not found!\n", __FUNCTION__, pid));
	LogErrorMessage("(%s:%d) %s(0x%08x, pid=%d, errorCode=%d): error: pid not found\n", __FILE__, __LINE__, __FUNCTION__, (unsigned)p, (unsigned)pid, errorCode);

Return:
	return;
	
} // UpdateAckValue


int NumUnsetAckValues( AckValues * p )
{
	int result;
	int i;

	result = 0;

        if (!p) {
            dwarning(("%s, No Ack Values passed in??\n", __FUNCTION__));
            return 0;
        }
	
	for (i = 0; i < p->logicalLength; i++)
	{
		if ( p->ackValues[ i ].state != kAckReceived )
		{
			result ++ ;
		}
	}

//	dwarning(("%s => %d\n", __FUNCTION__, result));
	
	return result;

} // NumUnsetAckValues


int NumUnsetAckValuesForAllDisks( void )
{
	DiskPtr diskPtr;
	int result;

	result = 0;
	
	for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = diskPtr->next)
	{
		if ( diskPtr->ackValues )
		{
			result += NumUnsetAckValues( diskPtr->ackValues );
		}
	} // FOREACH disk

	dwarning(("%s => %d\n", __FUNCTION__, result));
	
	return result;

} // NumUnsetAckValuesForAllDisks


int NumUnsetAckValuesForAllPartitions( DiskPtr diskPtr )
{
	DiskPtr diskPtr2;
	int	family1, family2;
	int	deviceNum1, deviceNum2;
	int result;

	result = 0;
	
	if ( IsNetwork( diskPtr ) )
	{
		result = NumUnsetAckValues( diskPtr->ackValues );
		goto Return;
	}

	family1 = diskPtr->family;
	deviceNum1 = diskPtr->ioBSDUnit;

	for (diskPtr2 = g.Disks; diskPtr2 != NULL; diskPtr2 = diskPtr2->next)
	{
		family2 = diskPtr2->family;
		deviceNum2 = diskPtr2->ioBSDUnit;

		if ( family1 == family2 && deviceNum1 == deviceNum2 )
		{
			if ( diskPtr2->ackValues )
			{
				result += NumUnsetAckValues( diskPtr2->ackValues );
			}
		}
	}

Return:
	dwarning(("%s => %d\n", __FUNCTION__, result));
	
	return result;

} // NumUnsetAckValuesForAllPartitions


void PrintAckValues( AckValues * p )
{
	int i;
	
	if ( ! gDebug ) goto Return;
	
	if ( ! p ) goto Return;
	
//	dwarning(("%s: p->physicalLength = %d, p->logicalLength = %d\n", __FUNCTION__, p->physicalLength, p->logicalLength));
	for (i = 0; i < p->logicalLength; i++)
	{
		dwarning(("%2d: pid = %d, state = %d, errorCode = %d\n", i, p->ackValues[ i ].pid, p->ackValues[ i ].state, p->ackValues[ i ].errorCode));
	}
	
Return:
	return;
	
} // PrintAckValues


AckValue * GetDissenterFromAckValues( AckValues * ackValuesPtr )
{
	AckValue * result;
	int i;
	
	for (i = 0; i < ackValuesPtr->logicalLength; i++)
	{
		result = & ackValuesPtr->ackValues[ i ];

		if ( result->state == kAckReceived && result->errorCode != 0 )
		{
			goto Return;
		}
	}
	
	result = NULL;

Return:
	return result;

} // GetDissenterFromAckValues


AckValue * GetDissenterFromAckValuesForAllDisks( void )
{
	AckValue * result;
	DiskPtr diskPtr;

	for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = diskPtr->next)
	{
		if ( diskPtr->ackValues )
		{
			result = GetDissenterFromAckValues( diskPtr->ackValues );
			if ( result )
			{
				goto Return;
			}
		}
	} // FOREACH disk
	
	result = NULL;

Return:
	return result;

} // GetDissenterFromAckValuesForAllDisks


AckValue * GetDissenterFromAckValuesForAllPartitions( DiskPtr diskPtr )
{
	AckValue * result;
	DiskPtr diskPtr2;
	int	family1, family2;
	int	deviceNum1, deviceNum2;

	if ( IsNetwork( diskPtr ) )
	{
		result = GetDissenterFromAckValues( diskPtr->ackValues );
		goto Return;
	}

	family1 = diskPtr->family;
	deviceNum1 = diskPtr->ioBSDUnit;

	for (diskPtr2 = g.Disks; diskPtr2 != NULL; diskPtr2 = diskPtr2->next)
	{
		family2 = diskPtr2->family;
		deviceNum2 = diskPtr2->ioBSDUnit;

		if ( family1 == family2 && deviceNum1 == deviceNum2 )
		{
			if ( diskPtr2->ackValues )
			{
				result = GetDissenterFromAckValues( diskPtr2->ackValues );
				if ( result )
				{
					goto Return;
				}
			}
		}
	}

	result = NULL;

Return:
	return result;

} // GetDissenterFromAckValuesForAllPartitions

AckValue * GetUnresponderFromAckValues( AckValues * ackValuesPtr )
{
        AckValue * result;
        int i;

        for (i = 0; i < ackValuesPtr->logicalLength; i++)
        {
                result = & ackValuesPtr->ackValues[ i ];

                if ( result->state == kWaitingForAck )
                {
                        goto Return;
                }
        }

        result = NULL;

Return:
        return result;

} // GetUnresponderFromAckValues


AckValue * GetUnresponderFromAckValuesForAllDisks( void )
{
        AckValue * result;
        DiskPtr diskPtr;

        for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = diskPtr->next)
        {
                if ( diskPtr->ackValues )
                {
                        result = GetUnresponderFromAckValues( diskPtr->ackValues );
                        if ( result )
                        {
                                goto Return;
                        }
                }
        } // FOREACH disk

        result = NULL;

Return:
        return result;

} // GetUnresponderFromAckValuesForAllDisks


void MakeDeadClientAgreeable( ClientPtr clientPtr )
{
	DiskPtr diskPtr;
	int i;

	for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = diskPtr->next)
	{
		if ( diskPtr->ackValues )
		{
			for (i = 0; i < diskPtr->ackValues->logicalLength; i++)
			{
				if ( clientPtr->pid == diskPtr->ackValues->ackValues[ i ].pid )
				{
					diskPtr->ackValues->ackValues[ i ].state = kAckReceived;
					diskPtr->ackValues->ackValues[ i ].errorCode = 0;
				}
			}
		}

	} // FOREACH disk

} // MakeDeadClientAgreeable


//------------------------------------------------------------------------

unsigned NumClientsDesiringAsyncNotification( void )
{
    return NumClientsDesiringAsyncNotificationOfType(kDiskArbNotifyAsync) +
           NumClientsDesiringAsyncNotificationOfType(kDiskArbNotifyDiskApproval);
} // NumClientsDesiringAsyncNotification


unsigned NumClientsDesiringAsyncNotificationOfType( int type )
{
	unsigned result = 0; /* mandatory initialization */
	ClientPtr clientPtr;
	
	for (clientPtr = g.Clients; clientPtr != NULL; clientPtr = clientPtr->next)
	{
		if ( clientPtr->flags & type )
		{
			result ++ ;
		}
	}

	return result;

} // NumClientsDesiringAsyncNotification


//------------------------------------------------------------------------

DiskThreadRecord * NewDiskThreadRecord()
{
    DiskThreadRecord * record = malloc(sizeof(DiskThreadRecord));

    record->diskAppearedType = 0;
    record->port = 0;
    record->ioBSDName = NULL;
    record->flags = 0;
    record->mountpoint = NULL;
    record->pid = 0;
    record->ioDeviceTreePath = NULL;
    record->ioContent = NULL;
    record->sequenceNumber = 0;
    record->diskType = 0;
    record->fsName = NULL;
    record->fsType = NULL;
    record->timeAppeared = 0;
    
    return record;
    
}

void DeleteThreadRecord(DiskThreadRecord *record)
{
    if (record->ioBSDName) {
        free(record->ioBSDName);
        record->ioBSDName = NULL; // catch accidental double free
    }
    if (record->ioDeviceTreePath) {
        free(record->ioDeviceTreePath);
        record->ioDeviceTreePath = NULL;
    }
    if (record->ioContent) {
        free(record->ioContent);
        record->ioContent = NULL;
    }
    if (record->mountpoint) {
        free(record->mountpoint);
        record->mountpoint = NULL;
    }
    if (record->fsType) {
        free(record->fsType);
        record->fsType = NULL;
    }
    if (record->fsName) {
        free(record->fsName);
        record->fsName = NULL;
    }
    free(record);
    record = NULL;
}


static void * NotifyDiskAppeared(void * args)
{
	DiskThreadRecord *record = args;
        kern_return_t r = nil;

    if ( record->diskAppearedType == kDiskArbNotifyDiskAppearedWithoutMountpoint )
	{
		r = DiskArbDiskAppearedWithoutMountpoint_rpc(record->port, record->ioBSDName, record->flags);
	}
    else if ( record->diskAppearedType == kDiskArbNotifyDiskAppearedWithMountpoint )
        {
                r = DiskArbDiskAppearedWithMountpoint_rpc(record->port, record->ioBSDName, record->flags, record->mountpoint);
        }
    else if ( record->diskAppearedType == kDiskArbNotifyDiskAppeared )
        {
                r = DiskArbDiskAppeared_rpc(record->port, record->ioBSDName, record->flags, record->mountpoint, record->ioContent);
        }
    else if ( record->diskAppearedType == kDiskArbNotifyDiskAppeared2 )
        {
                r = DiskArbDiskAppeared2_rpc(record->port, record->ioBSDName, record->flags, record->mountpoint, record->ioContent, record->ioDeviceTreePath, record->sequenceNumber);
        }
    else if ( record->diskAppearedType == kDiskArbNotifyDiskAppearedComplete )
        {
                r = DiskArbDiskAppearedComplete_rpc(record->port, record->ioBSDName, record->flags, record->mountpoint, record->ioContent, record->ioDeviceTreePath, record->sequenceNumber, record->timeAppeared, record->fsType, record->fsName);
        }
	if ( r ) dwarning(("... $%08x: %s\n", r, mach_error_string(r)));
	if ( r == MACH_SEND_INVALID_DEST )
	{
		/* The client has died */
		/* Don't do anything here ... wait for the notification */
		dwarning(("Dead client! Port = $%08x (pid=%d)\n", record->port, record->pid));
	}
    DeleteThreadRecord(record);
	return NULL;
}

static void * NotifyDiskRegistrationComplete(void * args)
{
        ClientPtr client = args;
        kern_return_t r;

        r = DiskArbRegistrationComplete_rpc(client->port);
        if ( r ) dwarning(("... $%08x: %s\n", r, mach_error_string(r)));
        if ( r == MACH_SEND_INVALID_DEST )
        {
                /* The client has died */
                /* Don't do anything here ... wait for the notification */
                dwarning(("Dead client! Port = $%08x\n", client->port));
        }
        return NULL;
}


static void * NotifyDiskMessagesCompleted(void * args)
{
        DiskThreadRecord *record = args;
        kern_return_t r;
        r = DiskArbNotificationComplete_rpc(record->port, record->sequenceNumber);
        if ( r ) dwarning(("... $%08x: %s\n", r, mach_error_string(r)));
        if ( r == MACH_SEND_INVALID_DEST )
        {
                /* The client has died */
                /* Don't do anything here ... wait for the notification */
                dwarning(("Dead client! Port = $%08x\n", record->port));
        }
        DeleteThreadRecord(record);
        return NULL;
}

static void * NotifyClientDisconnected(void * args)
{
        ClientPtr client = args;
        kern_return_t r;
        r = DiskArbClientDisconnected_rpc(client->port);
        if ( r ) dwarning(("... $%08x: %s\n", r, mach_error_string(r)));
        if ( r == MACH_SEND_INVALID_DEST )
        {
                /* The client has died */
                /* Don't do anything here ... wait for the notification */
                dwarning(("Dead client! Port = $%08x\n", client->port));
        }
        return NULL;
}

static void * NotifyBlueBoxMessages(void * args)
{
        DiskThreadRecord *record = args;
        kern_return_t r;
        r = DiskArbBlueBoxBootVolumeUpdated_async_rpc(record->port, record->sequenceNumber);
        if ( r ) dwarning(("... $%08x: %s\n", r, mach_error_string(r)));
        if ( r == MACH_SEND_INVALID_DEST )
        {
                /* The client has died */
                /* Don't do anything here ... wait for the notification */
                dwarning(("Dead client! Port = $%08x\n", record->port));
        }
        DeleteThreadRecord(record);
        return NULL;
}

static void * NotifyDiskChanged(void * args)
{
        DiskThreadRecord *record = args;
        kern_return_t r;

        r = DiskArbDiskChanged_rpc(record->port, record->ioBSDName, record->mountpoint, record->ioContent, record->flags, record->sequenceNumber);
        if ( r ) dwarning(("... $%08x: %s\n", r, mach_error_string(r)));
        if ( r == MACH_SEND_INVALID_DEST )
        {
                /* The client has died */
                /* Don't do anything here ... wait for the notification */
                dwarning(("Dead client! Port = $%08x\n", record->port));
        }
        DeleteThreadRecord(record);
        return NULL;
}

static void * NotifyDiskWillBeChecked(void * args)
{
        DiskThreadRecord *record = args;
        kern_return_t r;

        r = DiskArbDiskWillBeChecked_rpc(record->port, record->ioBSDName, record->flags, record->ioContent);
        if ( r ) dwarning(("... $%08x: %s\n", r, mach_error_string(r)));
        if ( r == MACH_SEND_INVALID_DEST )
        {
                /* The client has died */
                /* Don't do anything here ... wait for the notification */
                dwarning(("Dead client! Port = $%08x\n", record->port));
        }
        DeleteThreadRecord(record);

        return NULL;
}

static void * NotifyCallFailed(void * args)
{
        DiskThreadRecord *record = args;
        kern_return_t r;

        r = DiskArbPreviousCallFailed_rpc(record->port, record->ioBSDName, record->flags, record->sequenceNumber);
        if ( r ) dwarning(("... $%08x: %s\n", r, mach_error_string(r)));
        if ( r == MACH_SEND_INVALID_DEST )
        {
                /* The client has died */
                /* Don't do anything here ... wait for the notification */
                dwarning(("Dead client! Port = $%08x\n", record->port));
        }
        DeleteThreadRecord(record);
        return NULL;
}

static void * NotifyCallSucceeded(void * args)
{
        DiskThreadRecord *record = args;
        kern_return_t r;

        r = DiskArbPreviousCallSucceeded_rpc(record->port, record->ioBSDName, record->flags);
        if ( r ) dwarning(("... $%08x: %s\n", r, mach_error_string(r)));
        if ( r == MACH_SEND_INVALID_DEST )
        {
                /* The client has died */
                /* Don't do anything here ... wait for the notification */
                dwarning(("Dead client! Port = $%08x\n", record->port));
        }
        DeleteThreadRecord(record);
        return NULL;
}



static void * NotifyUnrecognizedDiskInserted(void * args)
{
        DiskThreadRecord *record = args;
        kern_return_t r;

        // DO NOT PAY ATTENTION TO THE PARAM NAMES HERE - THEY ARE ALL WRONG (WELL MOSTLY)

        r = DiskArbUnknownFileSystemInserted_rpc(record->port, record->ioBSDName, record->mountpoint, record->ioContent, record->flags, record->sequenceNumber, record->diskAppearedType);
        if ( r ) dwarning(("... $%08x: %s\n", r, mach_error_string(r)));
        if ( r == MACH_SEND_INVALID_DEST )
        {
                /* The client has died */
                /* Don't do anything here ... wait for the notification */
                dwarning(("Dead client! Port = $%08x\n", record->port));
        }
        DeleteThreadRecord(record);
        return NULL;
}

static void * NotifyUnrecognizedDiskArbitration(void * args)
{
        DiskThreadRecord *record = args;
        kern_return_t r;

        // DO NOT PAY ATTENTION TO THE PARAM NAMES HERE - THEY ARE ALL WRONG (WELL MOSTLY)

        r = DiskArbWillClientHandleUnrecognizedDisk_rpc(record->port, record->ioBSDName, record->diskType, record->mountpoint, record->ioContent, record->flags, record->sequenceNumber, record->diskAppearedType);
        if ( r ) dwarning(("... $%08x: %s\n", r, mach_error_string(r)));
        if ( r == MACH_SEND_INVALID_DEST )
        {
                /* The client has died */
                /* Don't do anything here ... wait for the notification */
                dwarning(("Dead client! Port = $%08x\n", record->port));
        }
        DeleteThreadRecord(record);
        return NULL;
}

static void * NotifyPreUnmount(void * args)
{
        DiskThreadRecord *record = args;

        kern_return_t r;

        r = DiskArbUnmountPreNotify_async_rpc(record->port, record->ioBSDName, 0);
        if ( r ) dwarning(("... $%08x: %s\n", r, mach_error_string(r)));
        if ( r == MACH_SEND_INVALID_DEST )
        {
                /* The client has died */
                /* Don't do anything here ... wait for the notification */
                dwarning(("Dead client! Port = $%08x (pid=%d)\n", record->port, record->pid));
        }
        DeleteThreadRecord(record);
        return NULL;
}

static void * NotifyPreEjection(void * args)
{
        DiskThreadRecord *record = args;

        kern_return_t r;

        r = DiskArbEjectPreNotify_async_rpc(record->port, record->ioBSDName, 0);
        if ( r ) dwarning(("... $%08x: %s\n", r, mach_error_string(r)));
        if ( r == MACH_SEND_INVALID_DEST )
        {
                /* The client has died */
                /* Don't do anything here ... wait for the notification */
                dwarning(("Dead client! Port = $%08x (pid=%d)\n", record->port, record->pid));
        }
        DeleteThreadRecord(record);
        return NULL;
}

static void * NotifyPreMount(void * args)
{
        DiskThreadRecord *record = args;

        kern_return_t r;

        r = DiskArbDiskApprovalPre_rpc(record->port, record->ioBSDName, record->mountpoint, record->ioContent, record->ioDeviceTreePath, record->flags, record->isWritable, record->isRemovable, record->isWhole, record->fsType);
        if ( r ) dwarning(("... $%08x: %s\n", r, mach_error_string(r)));
        if ( r == MACH_SEND_INVALID_DEST )
        {
                /* The client has died */
                /* Don't do anything here ... wait for the notification */
                dwarning(("Dead client! Port = $%08x (pid=%d)\n", record->port, record->pid));
        }
        DeleteThreadRecord(record);
        return NULL;
}


static void * NotifyPostUnmount(void * args)
{
        DiskThreadRecord *record = args;

        kern_return_t r;

        r = DiskArbUnmountPostNotify_async_rpc(record->port, record->ioBSDName, record->sequenceNumber, record->pid);
        if ( r ) dwarning(("... $%08x: %s\n", r, mach_error_string(r)));
        if ( r == MACH_SEND_INVALID_DEST )
        {
                /* The client has died */
                /* Don't do anything here ... wait for the notification */
                dwarning(("Dead client! Port = $%08x (pid=%d)\n", record->port, record->pid));
        }
        DeleteThreadRecord(record);
        return NULL;
}

static void * NotifyPostEjection(void * args)
{
        DiskThreadRecord *record = args;

        kern_return_t r;

        r = DiskArbEjectPostNotify_async_rpc(record->port, record->ioBSDName, record->sequenceNumber, record->pid);
        if ( r ) dwarning(("... $%08x: %s\n", r, mach_error_string(r)));
        if ( r == MACH_SEND_INVALID_DEST )
        {
                /* The client has died */
                /* Don't do anything here ... wait for the notification */
                dwarning(("Dead client! Port = $%08x (pid=%d)\n", record->port, record->pid));
        }
        DeleteThreadRecord(record);
        return NULL;
}


static void StartDiskAppearedThread(DiskThreadRecord * record)
{
    pthread_attr_t attr;
    pthread_t tid;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, NotifyDiskAppeared, record);
    pthread_attr_destroy(&attr);
}


void StartDiskMessagesCompleted(DiskThreadRecord * record)
{
    pthread_attr_t attr;
    pthread_t tid;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, NotifyDiskMessagesCompleted, record);
    pthread_attr_destroy(&attr);
}

static void StartBlueBoxNotificationThread(DiskThreadRecord * record)
{
    pthread_attr_t attr;
    pthread_t tid;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, NotifyBlueBoxMessages, record);
    pthread_attr_destroy(&attr);
}


void StartDiskRegistrationCompleteThread(ClientPtr client)
{
    pthread_attr_t attr;
    pthread_t tid;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, NotifyDiskRegistrationComplete, client);
    pthread_attr_destroy(&attr);
}

void StartClientDisconnectedThread(ClientPtr client)
{
    pthread_attr_t attr;
    pthread_t tid;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, NotifyClientDisconnected, client);
    pthread_attr_destroy(&attr);
}

void StartDiskChangedThread(DiskThreadRecord * record)
{
    pthread_attr_t attr;
    pthread_t tid;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, NotifyDiskChanged, record);
    pthread_attr_destroy(&attr);
}

void StartDiskWillBeCheckedMessages(DiskThreadRecord * record)
{
    pthread_attr_t attr;
    pthread_t tid;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, NotifyDiskWillBeChecked, record);
    pthread_attr_destroy(&attr);
}

void StartCallFailedThread(DiskThreadRecord * record)
{
    pthread_attr_t attr;
    pthread_t tid;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, NotifyCallFailed, record);
    pthread_attr_destroy(&attr);
}

void StartCallSucceededThread(DiskThreadRecord * record)
{
    pthread_attr_t attr;
    pthread_t tid;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, NotifyCallSucceeded, record);
    pthread_attr_destroy(&attr);
}



void StartUnrecognizedDiskThread(DiskThreadRecord * record)
{
    pthread_attr_t attr;
    pthread_t tid;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, NotifyUnrecognizedDiskInserted, record);
    pthread_attr_destroy(&attr);
}

void StartUnrecognizedDiskArbitrationThread(DiskThreadRecord * record)
{
    pthread_attr_t attr;
    pthread_t tid;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, NotifyUnrecognizedDiskArbitration, record);
    pthread_attr_destroy(&attr);
}

void StartPreUnmountNotifyThread(DiskThreadRecord * record)
{
    pthread_attr_t attr;
    pthread_t tid;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, NotifyPreUnmount, record);
    pthread_attr_destroy(&attr);
}

void StartPreMountNotifyThread(DiskThreadRecord * record)
{
    pthread_attr_t attr;
    pthread_t tid;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, NotifyPreMount, record);
    pthread_attr_destroy(&attr);
}

void StartPreEjectNotifyThread(DiskThreadRecord * record)
{
    pthread_attr_t attr;
    pthread_t tid;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, NotifyPreEjection, record);
    pthread_attr_destroy(&attr);
}

void StartPostUnmountNotifyThread(DiskThreadRecord * record)
{
    pthread_attr_t attr;
    pthread_t tid;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, NotifyPostUnmount, record);
    pthread_attr_destroy(&attr);
}

void StartPostEjectNotifyThread(DiskThreadRecord * record)
{
    pthread_attr_t attr;
    pthread_t tid;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, NotifyPostEjection, record);
    pthread_attr_destroy(&attr);
}

void SendClientWasDisconnectedMsg(ClientPtr clientPtr)
{
    dwarning(("DiskArbClientDisconnected_rpc($%08x) ...\n", clientPtr->port));

    StartClientDisconnectedThread(clientPtr);

} // SendClientWasDisconnectedMsg

//------------------------------------------------------------------------

int SendDiskAppearedMsgs( void )
{
	ClientPtr clientPtr;
	DiskPtr diskPtr;

        int newDiskAppeared = FALSE;
	
	dwarning(("%s\n", __FUNCTION__));
	
	/* For each client, and disk, send a notification if either of them is new */

	for (clientPtr = g.Clients; clientPtr != NULL; clientPtr = clientPtr->next)
	{

		/* Each new client that wants it gets an initial message giving the Blue Box boot volume. */

		if ( ( kClientStateNew == clientPtr->state ) && ( clientPtr->flags & kDiskArbNotifyBlueBoxBootVolumeUpdated ) )
		{
                        DiskThreadRecord * record = NewDiskThreadRecord();
                        dwarning(("DiskArbBlueBoxBootVolumeUpdated_async_rpc($%08x (pid=%d), gBlueBoxBootVolume=%d) ...\n", clientPtr->port, clientPtr->pid, gBlueBoxBootVolume));
                        record->sequenceNumber = gBlueBoxBootVolume;
                        record->port = clientPtr->port;
                        StartBlueBoxNotificationThread(record);
			
		}

		for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = diskPtr->next)
		{
            if ( ( kClientStateNew == clientPtr->state ) || ( kDiskStateNew == diskPtr->state ) || ( kDiskStateMountApprovalDenied == diskPtr->state ) || ( kDiskStateUnrecognized == diskPtr->state ))
            {
                if ((diskPtr->flags & kDiskArbDiskAppearedUnrecognizableFormat) && (clientPtr->flags & kDiskArbNotifyUnrecognizedVolumes) ) {
                        SendUnrecognizedDiskMsgs(clientPtr->port, diskPtr->ioBSDName, "", "", (!((diskPtr->flags & kDiskArbDiskAppearedLockedMask) == kDiskArbDiskAppearedLockedMask)), ((diskPtr->flags & kDiskArbDiskAppearedEjectableMask) == kDiskArbDiskAppearedEjectableMask), IsWhole(diskPtr));
                        continue;
                }

				/* New semantics: DiskAppearedWithoutMountpoint and DiskAppearedWithMountpoint *are* mutually exclusive */
				
				/* But: DiskAppeared gets sent for *every* disk ... */

				if ( (clientPtr->flags & kDiskArbNotifyDiskAppearedWithoutMountpoint) && 0 == strcmp("",diskPtr->mountpoint) )
				{
					DiskThreadRecord * record = NewDiskThreadRecord();

					dwarning(("DiskArbDiskAppearedWithoutMountpoint_rpc($%08x (pid=%d), '%s', $%08x) ...\n", clientPtr->port, clientPtr->pid, diskPtr->ioBSDName, diskPtr->flags));

                                        record->diskAppearedType = kDiskArbNotifyDiskAppearedWithoutMountpoint;
					record->port = clientPtr->port;
					record->ioBSDName = strdup( diskPtr->ioBSDName );
					record->flags = diskPtr->flags;
					record->mountpoint = strdup( diskPtr->mountpoint );
					record->pid = clientPtr->pid;
                                        record->ioContent = strdup(diskPtr->ioContent);
                                        record->ioDeviceTreePath = strdup(diskPtr->ioDeviceTreePath);
                                        record->sequenceNumber = diskPtr->sequenceNumber;

					StartDiskAppearedThread(record);
                    if ( kDiskStateNew == diskPtr->state ) newDiskAppeared = TRUE;
				}

				if ( (clientPtr->flags & kDiskArbNotifyDiskAppearedWithMountpoint) && 0 != strcmp("",diskPtr->mountpoint) )
				{
					DiskThreadRecord * record = NewDiskThreadRecord();

					dwarning(("DiskArbDiskAppearedWithMountpoint_rpc($%08x (pid=%d), '%s', $%08x, '%s') ...\n", clientPtr->port, clientPtr->pid, diskPtr->ioBSDName, diskPtr->flags, diskPtr->mountpoint));

                                        record->diskAppearedType = kDiskArbNotifyDiskAppearedWithMountpoint;
                                        record->port = clientPtr->port;
                                        record->ioBSDName = strdup( diskPtr->ioBSDName );
                                        record->flags = diskPtr->flags;
                                        record->mountpoint = strdup( diskPtr->mountpoint );
                                        record->pid = clientPtr->pid;
                                        record->ioContent = strdup(diskPtr->ioContent);
                                        record->ioDeviceTreePath = strdup(diskPtr->ioDeviceTreePath);
                                        record->sequenceNumber = diskPtr->sequenceNumber;
                                        
                    StartDiskAppearedThread(record);
                    if ( kDiskStateNew == diskPtr->state ) newDiskAppeared = TRUE;
				}
                            
                if ( clientPtr->flags & kDiskArbNotifyDiskAppeared ) {
                    DiskThreadRecord * record = NewDiskThreadRecord();

                    dwarning(("DiskArbDiskAppeared_rpc($%08x (pid=%d), '%s', $%08x, '%s', '%s') ...\n", clientPtr->port, clientPtr->pid, diskPtr->ioBSDName, diskPtr->flags, diskPtr->mountpoint, diskPtr->ioContent));

                    record->diskAppearedType = kDiskArbNotifyDiskAppeared;
                    record->port = clientPtr->port;
                    record->ioBSDName = strdup( diskPtr->ioBSDName );
                    record->flags = diskPtr->flags;
                    record->mountpoint = strdup( diskPtr->mountpoint );
                    record->pid = clientPtr->pid;
                    record->ioContent = strdup(diskPtr->ioContent);
                    record->ioDeviceTreePath = strdup(diskPtr->ioDeviceTreePath);
                    record->sequenceNumber = diskPtr->sequenceNumber;
                    StartDiskAppearedThread(record);
                    if ( kDiskStateNew == diskPtr->state ) newDiskAppeared = TRUE;
                }

                if ( clientPtr->flags & kDiskArbNotifyDiskAppeared2 ) {
                    DiskThreadRecord * record = NewDiskThreadRecord();

                    dwarning(("DiskArbDiskAppeared2_rpc($%08x (pid=%d), '%s', $%08x, '%s', '%s', '%s', %d) ...\n", clientPtr->port, clientPtr->pid, diskPtr->ioBSDName, diskPtr->flags, diskPtr->mountpoint, diskPtr->ioContent, diskPtr->ioDeviceTreePath, diskPtr->sequenceNumber));

                    record->diskAppearedType = kDiskArbNotifyDiskAppeared2;
                    record->port = clientPtr->port;
                    record->ioBSDName = strdup( diskPtr->ioBSDName );
                    record->flags = diskPtr->flags;
                    record->mountpoint = strdup( diskPtr->mountpoint );
                    record->pid = clientPtr->pid;
                    record->ioContent = strdup(diskPtr->ioContent);
                    record->ioDeviceTreePath = strdup(diskPtr->ioDeviceTreePath);
                    record->sequenceNumber = diskPtr->sequenceNumber;
                    StartDiskAppearedThread(record);
                    if ( kDiskStateNew == diskPtr->state ) newDiskAppeared = TRUE;
                }

                if ( clientPtr->flags & kDiskArbNotifyDiskAppearedComplete ) {
                    DiskThreadRecord * record = NewDiskThreadRecord();

                    dwarning(("DiskArbDiskAppearedComplete_rpc\n"));

                    record->diskAppearedType = kDiskArbNotifyDiskAppearedComplete;
                    record->port = clientPtr->port;
                    record->ioBSDName = strdup( diskPtr->ioBSDName );
                    record->flags = diskPtr->flags;
                    record->mountpoint = strdup( diskPtr->mountpoint );
                    record->pid = clientPtr->pid;
                    record->ioContent = strdup(diskPtr->ioContent);
                    record->ioDeviceTreePath = strdup(diskPtr->ioDeviceTreePath);
                    record->sequenceNumber = diskPtr->sequenceNumber;
                    record->fsType = (diskPtr->mountedFilesystemName?strdup( diskPtr->mountedFilesystemName):strdup(""));
                    record->timeAppeared = diskPtr->timeAppeared;
                    record->fsName = (diskPtr->volName?strdup( diskPtr->volName):strdup(""));

                    StartDiskAppearedThread(record);
                    if ( kDiskStateNew == diskPtr->state ) newDiskAppeared = TRUE;
                }


			} // IF client is new OR disk is new

		} // FOREACH disk

	} // FOREACH client

        // cleanup -- all unrecognized disks need to be marked Idle - notifications have been sent for them
        for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = diskPtr->next)
        {

            if ( kDiskStateUnrecognized == diskPtr->state || kDiskStateMountApprovalDenied == diskPtr->state) {
                SetStateForOnePartition(diskPtr, kDiskStateIdle);
            }
        }
        
	
    return newDiskAppeared;


} // SendDiskAppearedMsgs

void SendUnrecognizedDiskMsgs(mach_port_t port, char *devname, char *fstype, char *deviceType, int isWritable, int isRemovable, int isWhole)
{
        DiskThreadRecord * record = NewDiskThreadRecord();

        record->diskAppearedType = isWhole;
        record->port = port;
        record->ioBSDName = strdup( devname );
        record->flags = isWritable;
        record->mountpoint = strdup( fstype );
        record->pid = 0;
        record->ioContent = strdup(deviceType);
        record->sequenceNumber = isRemovable;

        StartUnrecognizedDiskThread(record);
        
} // SendUnrecognizedDiskMsgs

void SendUnrecognizedDiskArbitrationMsgs(mach_port_t port, char *devname, char *fstype, char *deviceType, int isWritable, int isRemovable, int isWhole, int diskType)
{
        DiskThreadRecord * record = NewDiskThreadRecord();

        record->diskAppearedType = isWhole;
        record->port = port;
        record->ioBSDName = strdup( devname );
        record->flags = isWritable;
        record->mountpoint = strdup( fstype );
        record->pid = 0;
        record->ioContent = strdup(deviceType);
        record->sequenceNumber = isRemovable;
        record->diskType = diskType;

        StartUnrecognizedDiskArbitrationThread(record);

} // SendUnrecognizedDiskArbitrationMsgs


void SendDiskChangedMsgs(char *devname, char *newMountpoint, char *newVolumeName, int flags, int success)
{
        ClientPtr clientPtr;

        dwarning(("%s\n", __FUNCTION__));

        /* For each client, and disk, send a notification if either of them is new */

        for (clientPtr = g.Clients; clientPtr != NULL; clientPtr = clientPtr->next)
        {
                if ( ( clientPtr->flags & kDiskArbNotifyChangedDisks ) ) {
                        DiskThreadRecord * record = NewDiskThreadRecord();
                        dwarning(("DiskArbDiskChanged_rpc($%08x (pid=%d)) ...\n", clientPtr->port, clientPtr->pid));

                        record->port = clientPtr->port;
                        record->ioBSDName = strdup( devname );
                        record->flags = flags;
                        record->mountpoint = strdup( newMountpoint );
                        record->ioContent = strdup(newVolumeName);
                        record->sequenceNumber = success;

                        StartDiskChangedThread(record);
                }
        }

} // SendDiskChangedMsgs

void SendDiskWillBeCheckedMessages(DiskPtr diskPtr)
{
        ClientPtr clientPtr;
        dwarning(("%s\n", __FUNCTION__));

        /* For each client, and disk, send a notification if either of them is new */

        for (clientPtr = g.Clients; clientPtr != NULL; clientPtr = clientPtr->next)
        {
                if ( ( clientPtr->flags & kDiskArbNotifyDiskWillBeChecked ) ) {
                        DiskThreadRecord * record = NewDiskThreadRecord();

                        dwarning(("SendDiskWillBeCheckedMessages_rpc($%08x (pid=%d)) ...\n", clientPtr->port, clientPtr->pid));

                        record->port = clientPtr->port;
                        record->ioBSDName = strdup( diskPtr->ioBSDName );
                        record->flags = diskPtr->flags;
                        record->pid = clientPtr->pid;
                        record->ioContent = strdup(diskPtr->ioContent);

                        StartDiskWillBeCheckedMessages(record);
                }
        }

} // SendDiskWillBeCheckedMessages

void SendCallFailedMessage(ClientPtr clientPtr, DiskPtr diskPtr, int failedType, int error)
{
        if ( ( clientPtr->flags & kDiskArbNotifyCallFailed ) ) {
                DiskThreadRecord * record = NewDiskThreadRecord();

                dwarning(("SendCallFailedMessage($%08x (pid=%d)) ...\n", clientPtr->port, clientPtr->pid));

                record->port = clientPtr->port;
                record->ioBSDName = (diskPtr ? strdup( diskPtr->ioBSDName ) : strdup(""));
                record->flags = failedType;
                record->pid = clientPtr->pid;
                record->sequenceNumber = error;

                StartCallFailedThread(record);
        }
} // SendCallFailedMessage

void SendCallSucceededMessage(ClientPtr clientPtr, DiskPtr diskPtr, int succeededType)
{
        if ( ( clientPtr->flags & kDiskArbNotifyCallSucceeded ) ) {
                DiskThreadRecord * record = NewDiskThreadRecord();

                dwarning(("SendCallSucceededMessage($%08x (pid=%d)) ...\n", clientPtr->port, clientPtr->pid));

                record->port = clientPtr->port;
                record->ioBSDName = (diskPtr ? strdup( diskPtr->ioBSDName ) : strdup(""));
                record->flags = succeededType;
                record->pid = clientPtr->pid;

                StartCallSucceededThread(record);
        }
} // SendCallSucceededMessage


//------------------------------------------------------------------------

void PrepareToSendPreUnmountMsgs( void )
{
    ClientPtr clientPtr;
    DiskPtr diskPtr;

    dwarning(("%s\n", __FUNCTION__));

    /* For each client and disk */

    for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = diskPtr->next)
    {
                if ( kDiskStateToBeUnmounted == diskPtr->state || kDiskStateToBeUnmountedAndEjected == diskPtr->state )
                {
                        // Assert: the <ackValues> field for this disk was allocated/initialized when its state was set.

                        // only ask for unmount notifications for *mounted" disks
                        // makes sense *right*?

                        if (diskPtr->mountpoint && strlen(diskPtr->mountpoint)) {

                                for (clientPtr = g.Clients; clientPtr != NULL; clientPtr = clientPtr->next)
                                {
                                    if ( clientPtr->flags & kDiskArbNotifyAsync )
                                        {
                                                InitAckValue( diskPtr->ackValues, clientPtr->pid );
                                        }

                                } // FOREACH client
                       }
                }
    } // FOREACH disk

} // PrepareToSendPreUnmountMsgs


//------------------------------------------------------------------------


void PrepareToSendPreEjectMsgs( void )
{
    ClientPtr clientPtr;
    DiskPtr diskPtr;

    dwarning(("%s\n", __FUNCTION__));

    /* For each client and disk */

    for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = diskPtr->next)
    {
        if ( kDiskStateToBeEjected == diskPtr->state )
        {
            // Assert: the <ackValues> field for this disk was allocated/initialized when its state was set.
                        // only ask for eject notifications for *whole" disks
                        // makes sense *right*?

                        //if (IsWhole(diskPtr)) {

                                for (clientPtr = g.Clients; clientPtr != NULL; clientPtr = clientPtr->next)
                                {
                                        if ( clientPtr->flags & kDiskArbNotifyAsync )
                                        {
                                                InitAckValue( diskPtr->ackValues, clientPtr->pid );
                                        }

                                } // FOREACH client
                        //}

        }

    } // FOREACH disk

} // PrepareToSendPreEjectMsgs

void PrepareToSendPreMountMsgsForDisk( DiskPtr diskPtr )
{
    ClientPtr clientPtr;

    dwarning(("%s\n", __FUNCTION__));

    /* For each client and disk */

    if ( kDiskStateWaitingForMountApproval == diskPtr->state )
    {
            // Assert: the <ackValues> field for this disk was allocated/initialized when its state was set.

            // only ask for unmount notifications for *mounted" disks
            // makes sense *right*?

            if (diskPtr->ioMediaNameOrNull && strlen(diskPtr->ioMediaNameOrNull)) {

                    for (clientPtr = g.Clients; clientPtr != NULL; clientPtr = clientPtr->next)
                    {
                            if ( clientPtr->flags & kDiskArbNotifyDiskApproval )
                            {
                                    InitAckValue( diskPtr->ackValues, clientPtr->pid );
                            }

                    } // FOREACH client
            }
    }

} // PrepareToSendPreMountMsgsForDisk


//------------------------------------------------------------------------


void SendPreUnmountMsgsForDisk( DiskPtr diskPtr )
{
        int i;

        dwarning(("%s('%s')\n", __FUNCTION__, diskPtr->ioBSDName));

        /* For each client and disk */

        if ( kDiskStateToBeUnmounted == diskPtr->state || kDiskStateToBeUnmountedAndEjected == diskPtr->state )
        {
                for (i = 0; i < diskPtr->ackValues->logicalLength; i++)
                {
                        ClientPtr clientPtr;

                        if ( diskPtr->ackValues->ackValues[ i ].state != kSendMsg )
                        {
                                continue;
                        }

                        clientPtr = LookupClientByPID( diskPtr->ackValues->ackValues[ i ].pid );
                        if ( ! clientPtr )
                        {
                                pwarning(("%s: pid = %d: no known client with this pid.\n", __FUNCTION__, diskPtr->ackValues->ackValues[ i ].pid));
                                continue;
                        }

                        if ( clientPtr->numAcksRequired > 0 )
                        {
                                dwarning(("%s: skipping pid = %d for '%s' since numAcksRequired = %d\n", __FUNCTION__, clientPtr->pid, diskPtr->ioBSDName, clientPtr->numAcksRequired));
                                continue;
                        }

                        {
                                DiskThreadRecord * record = NewDiskThreadRecord();
                                record->port = clientPtr->port;
                                record->ioBSDName = strdup( diskPtr->ioBSDName );
                                StartPreUnmountNotifyThread(record);
                        }

                        clientPtr->numAcksRequired++;

                        diskPtr->ackValues->ackValues[ i ].state = kWaitingForAck;

                } // FOREACH ack value

        }

} // SendPreUnmountMsgsForDisk


void SendPreMountMsgsForDisk( DiskPtr diskPtr )
{
        int i;

        dwarning(("%s('%s')\n", __FUNCTION__, diskPtr->ioBSDName));

        /* For each client and disk */

        if ( kDiskStateWaitingForMountApproval == diskPtr->state )
        {
                for (i = 0; i < diskPtr->ackValues->logicalLength; i++)
                {
                        ClientPtr clientPtr;

                        if ( diskPtr->ackValues->ackValues[ i ].state != kSendMsg )
                        {
                                continue;
                        }

                        clientPtr = LookupClientByPID( diskPtr->ackValues->ackValues[ i ].pid );
                        if ( ! clientPtr )
                        {
                                pwarning(("%s: pid = %d: no known client with this pid.\n", __FUNCTION__, diskPtr->ackValues->ackValues[ i ].pid));
                                continue;
                        }

                        if ( clientPtr->numAcksRequired > 0)
                        {
                                dwarning(("%s: skipping pid = %d for '%s' since numAcksRequired = %d\n", __FUNCTION__, clientPtr->pid, diskPtr->ioBSDName, clientPtr->numAcksRequired));
                                continue;
                        }

                        {
                                DiskThreadRecord * record = NewDiskThreadRecord();
                                record->port = clientPtr->port;
                                record->ioBSDName = strdup( diskPtr->ioBSDName );
                                record->ioDeviceTreePath = strdup( diskPtr->ioDeviceTreePath );
                                record->mountpoint = strdup( diskPtr->volName );
                                record->ioContent = strdup( diskPtr->ioContent );

                                record->isWritable = ( diskPtr->flags & kDiskArbDiskAppearedLockedMask ) == 0;
                                record->flags = diskPtr->flags;
                                record->isRemovable = ( diskPtr->flags & kDiskArbDiskAppearedEjectableMask ) == 0;
                                record->isWhole = IsWhole(diskPtr);
                                record->fsType = strdup("");  // not yet implemented
                                
                                StartPreMountNotifyThread(record);
                        }

                        clientPtr->numAcksRequired++;

                        diskPtr->ackValues->ackValues[ i ].state = kWaitingForAck;

                } // FOREACH ack value

        }

} // SendPreMountMsgsForDisk


//------------------------------------------------------------------------


void SendPreEjectMsgsForDisk( DiskPtr diskPtr )
{
        int i;

    if (!diskPtr) {
        pwarning(("%s no diskPtr!\n", __FUNCTION__));
    }

        dwarning(("%s('%s')\n", __FUNCTION__, diskPtr->ioBSDName));

        /* For each client and disk */

        if ( kDiskStateToBeEjected == diskPtr->state )
        {
                for (i = 0; i < diskPtr->ackValues->logicalLength; i++)
                {
                        ClientPtr clientPtr;

                        if ( diskPtr->ackValues->ackValues[ i ].state != kSendMsg )
                        {
                                continue;
                        }

                        clientPtr = LookupClientByPID( diskPtr->ackValues->ackValues[ i ].pid );
                        if ( ! clientPtr )
                        {
                                pwarning(("%s: pid = %d: no known client with this pid.\n", __FUNCTION__, diskPtr->ackValues->ackValues[ i ].pid));
                                continue;
                        }

                        if ( clientPtr->numAcksRequired > 0 )
                        {
                                dwarning(("%s: skipping pid = %d for '%s' since numAcksRequired = %d\n", __FUNCTION__, clientPtr->pid, diskPtr->ioBSDName, clientPtr->numAcksRequired));
                                continue;
                        }

                        {
                                DiskThreadRecord * record = NewDiskThreadRecord();
                                record->port = clientPtr->port;
                                record->ioBSDName = strdup( diskPtr->ioBSDName );
                                StartPreEjectNotifyThread(record);
                        }

                        clientPtr->numAcksRequired++;

                        diskPtr->ackValues->ackValues[ i ].state = kWaitingForAck;

                } // FOREACH ack value

        }


} // SendPreEjectMsgsForDisk


//------------------------------------------------------------------------


void SendUnmountPostNotifyMsgsForOnePartition( char * ioBSDName, int errorCode, pid_t pid )
{
	ClientPtr clientPtr;

	dwarning(("%s('%s', errorCode = %d, pid = %d)\n", __FUNCTION__, ioBSDName, errorCode, pid));

	for (clientPtr = g.Clients; clientPtr != NULL; clientPtr = clientPtr->next)
	{
		if ( clientPtr->flags & kDiskArbNotifyAsync )
		{
                        DiskThreadRecord * record = NewDiskThreadRecord();
                        dwarning(("DiskArbUnmountPostNotify_async_rpc($%08x (pid=%d), '%s', errorCode=%d, pid=%d) ...\n", clientPtr->port, clientPtr->pid, ioBSDName, errorCode, pid));
                        record->sequenceNumber = errorCode;
                        record->port = clientPtr->port;
                        record->ioBSDName = strdup( ioBSDName );
                        record->pid = pid;
                        StartPostUnmountNotifyThread(record);
		} 

	} // FOREACH client


} // SendUnmountPostNotifyMsgsForOnePartition


//------------------------------------------------------------------------


void SendEjectPostNotifyMsgsForOnePartition( char * ioBSDName, int errorCode, pid_t pid )
{
	ClientPtr clientPtr;

	dwarning(("%s('%s', errorCode = %d, pid = %d)\n", __FUNCTION__, ioBSDName, errorCode, pid));

	for (clientPtr = g.Clients; clientPtr != NULL; clientPtr = clientPtr->next)
	{
		if ( clientPtr->flags & kDiskArbNotifyAsync )
		{
                        DiskThreadRecord * record = NewDiskThreadRecord();
                        dwarning(("DiskArbEjectPostNotify_async_rpc($%08x (pid=%d), '%s', errorCode=%d, pid=%d) ...\n", clientPtr->port, clientPtr->pid, ioBSDName, errorCode, pid));
                        record->sequenceNumber = errorCode;
                        record->port = clientPtr->port;
                        record->ioBSDName = strdup( ioBSDName );
                        record->pid = pid;
                        StartPostEjectNotifyThread(record);
                }

	} // FOREACH client

} // SendEjectPostNotifyMsgsForOnePartition


//------------------------------------------------------------------------


void SendEjectPostNotifyMsgsForAllPartitions( DiskPtr diskPtr, int errorCode, pid_t pid )
{
	DiskPtr diskPtr2;
	int	family1, family2;
	int	deviceNum1, deviceNum2;
	
	dwarning(("%s('%s', errorCode = %d, pid = %d)\n", __FUNCTION__, diskPtr->ioBSDName, errorCode, pid));
	
	if ( IsNetwork( diskPtr ) )
	{
		SendEjectPostNotifyMsgsForOnePartition( diskPtr->ioBSDName, errorCode, pid );
		goto Return;
	}

	family1 = diskPtr->family;
	deviceNum1 = diskPtr->ioBSDUnit;

	for (diskPtr2 = g.Disks; diskPtr2 != NULL; diskPtr2 = diskPtr2->next)
	{
		family2 = diskPtr2->family;
		deviceNum2 = diskPtr2->ioBSDUnit;

		if ( family1 == family2 && deviceNum1 == deviceNum2 )
		{
			SendEjectPostNotifyMsgsForOnePartition( diskPtr2->ioBSDName, errorCode, pid );
		}
	}

Return:
	return;

} // SendEjectPostNotifyMsgsForAllPartitions


//------------------------------------------------------------------------


void SendBlueBoxBootVolumeUpdatedMsgs( void )
{
	ClientPtr clientPtr;

	dwarning(("%s()\n", __FUNCTION__));

	for (clientPtr = g.Clients; clientPtr != NULL; clientPtr = clientPtr->next)
	{
		if ( clientPtr->flags & kDiskArbNotifyBlueBoxBootVolumeUpdated )
		{
                        DiskThreadRecord * record = NewDiskThreadRecord();
			dwarning(("DiskArbBlueBoxBootVolumeUpdated_async_rpc($%08x (pid=%d), gBlueBoxBootVolume=%d) ...\n", clientPtr->port, clientPtr->pid, gBlueBoxBootVolume));
                        record->sequenceNumber = gBlueBoxBootVolume;
                        record->port = clientPtr->port;
                        StartBlueBoxNotificationThread(record);
		}

	} // FOREACH client

} // SendBlueBoxBootVolumeUpdatedMsgs

//------------------------------------------------------------------------


void SendCompletedMsgs( int messageType, int newDisk )
{
        ClientPtr clientPtr;
        DiskPtr diskPtr;

        dwarning(("%s(%d)\n", __FUNCTION__, messageType));

        for (clientPtr = g.Clients; clientPtr != NULL; clientPtr = clientPtr->next)
        {

                if ( ( clientPtr->flags & kDiskArbNotifyCompleted ) ) {
                        int shouldSend = FALSE;

                        //  client is new or disk is new and it's the new disk notification
                        if ((messageType == kDiskArbCompletedDiskAppeared) && (newDisk || ( kClientStateNew == clientPtr->state ))) {

                                dwarning(("Sending completed: Disk Appeared: newDisk = %d, clientState = %d\n", newDisk, clientPtr->state)); 
                                shouldSend = TRUE;
                        }

                        // unmount post and eject post go to everyone.
                        if (((messageType == kDiskArbCompletedPostUnmount) || (messageType == kDiskArbCompletedPostEject)) && clientPtr->flags & kDiskArbNotifyAsync ) {
                                dwarning(("Sending completed: Disk Unmounted or Ejected\n")); 
                                shouldSend = TRUE;
                        }

                        if (shouldSend) {
                                DiskThreadRecord * record = NewDiskThreadRecord();
                                dwarning(("DiskArbNotificationComplete_rpc($%08x (messageType=%d)) ...\n", clientPtr->port, messageType));

                                record->port = clientPtr->port;
                                record->sequenceNumber = messageType;

                                StartDiskMessagesCompleted(record);
                        }
                }
        } // FOREACH client

        /* For each disk and client, if it was new, then it is no longer new */

        for (clientPtr = g.Clients; clientPtr != NULL; clientPtr = clientPtr->next)
        {
                if ( kClientStateNew == clientPtr->state ) clientPtr->state = kClientStateIdle;
        }
        for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = diskPtr->next)
        {
                if ( kDiskStateNew == diskPtr->state ) SetStateForOnePartition( diskPtr, kDiskStateIdle );
        }


} // SendCompletedMsgs

//------------------------------------------------------------------------


void SetBlueBoxBootVolume( int seqno )
{
	dwarning(("%s(seqno=%d)\n", __FUNCTION__, seqno));

	gBlueBoxBootVolume = seqno;
	SendBlueBoxBootVolumeUpdatedMsgs();

} // SetBlueBoxBootVolume


//------------------------------------------------------------------------


int GetBlueBoxBootVolume( void )
{
//	dwarning(("%s() => %d\n", __FUNCTION__, gBlueBoxBootVolume));

	return gBlueBoxBootVolume;

} // GetBlueBoxBootVolume


//------------------------------------------------------------------------


void CompleteUnmountForDisk( DiskPtr diskPtr )
{
    AckValue * avp;
    int errorCode_out = 0;
    pid_t pid_out = 0;
    DiskPtr wholeDiskToEject = LookupWholeDiskForThisPartition(diskPtr);
    boolean_t unmountingAllowed = FALSE;
    int flags;
    int requestor;

    dwarning(("%s\n", __FUNCTION__));

    // Unmount could be completed for each partition independently.
    // But it simplifies things to wait them to all complete before proceeding.

    if ( 0 != NumUnsetAckValues(diskPtr->ackValues) )
    {
        goto Return;
    }

    /* 0 == NumUnsetAckValuesForAllDisks() */

    /* Initialize this before we start changing their states.  It will be NULL if this is not an unmount-and-eject. */

    if ( !( ( diskPtr->state == kDiskStateToBeUnmountedAndEjected ) || ( diskPtr->state == kDiskStateToBeEjected ) ) )
    {
        wholeDiskToEject = NULL;
    }


    /* Skip disks that are not undergoing unmounting. */

    if ( diskPtr->state != kDiskStateToBeUnmounted && diskPtr->state != kDiskStateToBeUnmountedAndEjected )
    {
        goto Continue;
    }

    avp = GetDissenterFromAckValues( diskPtr->ackValues );


    if ( NULL == avp ) {
         flags = kDiskArbApproveMounting;
     } else {
         flags = avp->errorCode;
     }

    requestor = diskPtr->unmountOrEjectRequestorUID;

    if (flags == kDiskArbApproveMounting || (requestor == 0)) {
        unmountingAllowed = TRUE;
    }
    if ((flags & kDiskArbRequireAuthentication) && (requestor != 0)) {
        ClientPtr client = LookupClientByPID(avp->pid);
        
        dwarning(("%s: checking client authorization to unmount '%s'\n", __FUNCTION__, diskPtr->ioBSDName));
        
        if (client && authorizationAllowedForEvent(client, "system.volume.unmount")) {
            unmountingAllowed = TRUE;
            avp->errorCode = 0;
            flags = 0;
        } else {
            unmountingAllowed = FALSE;
        }
    }

    if ( unmountingAllowed )
    {
        int err;

        dwarning(("%s: there was no dissenter for '%s'\n", __FUNCTION__, diskPtr->ioBSDName));

        err = UnmountDisk( diskPtr, FALSE ); /* Updates diskPtr->state for this partition. */
        if ( err )
        {
            /* Failure.  The unmount failed.  diskPtr->state == kDiskStateIdle */

            dwarning(("%s: the unmount failed\n", __FUNCTION__));

            errorCode_out = err;
            pid_out = -1;
        }
        else
        {
            /* Success.  diskPtr->state == kDiskStateNewlyUnmounted */
           errorCode_out = 0;
            pid_out = -1;

        }
    }
    else
    {
        /* Failure.  There was a dissenter.  Report it. */

        dwarning(("%s: there was a dissenter for '%s'\n", __FUNCTION__, diskPtr->ioBSDName));

        errorCode_out = flags;
        pid_out = (avp?avp->pid:0);

        SetStateForOnePartition( diskPtr, kDiskStateIdle );
    }

    /* Send each waiting client a notification with the errorcode and dissenter. */

    SendUnmountPostNotifyMsgsForOnePartition( diskPtr->ioBSDName, errorCode_out, pid_out );

    /* Was this the first phase of an unmount-and-eject? */
Continue:

        if ( wholeDiskToEject && ( 0 == NumPartitionsMountedFromThisDisk( wholeDiskToEject ) ) )
        {
            /* WARNING: this might rob old-style clients of their synchronous UnmountCommit notifications if the eject fails. */
            /* Why?  Because we keep no record that an unmount happened.  So if the eject fails, we won't send any UnmountCommit notifications. */
            /* This can only happen if there is someone holding open a file descriptor on one of the /dev nodes for the disk. */

            SetStateForAllPartitions( wholeDiskToEject, kDiskStateToBeEjected );

            /* Prepare a list of pre-eject notifications to be sent. */

            PrepareToSendPreEjectMsgs();

            /* Return to the main msg loop to process sending of pre-eject msgs and receiving of acks. */

        }

Return:
        return;

} // CompleteUnmountForDisk


//------------------------------------------------------------------------


void CompleteEjectForDisk( DiskPtr diskPtr )
{
    AckValue * avp;
    int errorCode_out = 0;
    pid_t pid_out = 0;
    DiskPtr wholeDiskToEject = LookupWholeDiskForThisPartition(diskPtr);
    boolean_t ejectingAllowed = FALSE;
    int flags;
    int requestor;

    dwarning(("%s\n", __FUNCTION__));
    if ( ! wholeDiskToEject )
    {
            LogErrorMessage("%s(): LookupWholeDiskToBeEjected() => NULL\n", __FUNCTION__);
            goto Return;
    }

    // Ejection differs from unmounting because it requires acks for
    // all partitions before proceeding.

    if ( 0 != NumUnsetAckValuesForAllPartitions(wholeDiskToEject) )
    {
        goto Return;
    }

    /* 0 == NumUnsetAckValuesForAllDisks() */

    avp = GetDissenterFromAckValuesForAllPartitions( wholeDiskToEject );


    if ( NULL == avp ) {
         flags = kDiskArbApproveMounting;
     } else {
         flags = avp->errorCode;
     }

    requestor = wholeDiskToEject->unmountOrEjectRequestorUID;

    if (flags == kDiskArbApproveMounting || (requestor == 0)) {
        ejectingAllowed = TRUE;
    }
    if ((flags & kDiskArbRequireAuthentication) && (requestor != 0)) {
        ClientPtr client = LookupClientByPID(avp->pid);
        
        dwarning(("%s: checking client authorization to eject '%s'\n", __FUNCTION__, wholeDiskToEject->ioBSDName));
        
        if (client && authorizationAllowedForEvent(client, "system.volume.eject")) {
            ejectingAllowed = TRUE;
            avp->errorCode = 0;
            flags = 0;
        } else {
            ejectingAllowed = FALSE;
        }
    }

    if ( ejectingAllowed )
    {
        int err;

        dwarning(("%s: there was no dissenter\n", __FUNCTION__));

        /* Assume that all the partitions have already been unmounted. */
	
        err = EjectDisk( wholeDiskToEject ); /* Updates wholeDiskToEject->state for each partition on this disk. */
        if ( err )
        {
            /* Failure.  The eject failed.  wholeDiskToEject->state == kDiskStateIdle */

            dwarning(("%s: the eject failed\n", __FUNCTION__));

            errorCode_out = err;
            pid_out = -1;
        }
        else
        {
            /* Success.  wholeDiskToEject->state == kDiskStateNewlyEjected */
            errorCode_out = 0;
            pid_out = -1;

        }
    }
    else
    {
        /* Failure.  There was a dissenter.  Report it. */

        dwarning(("%s: there was a dissenter\n", __FUNCTION__));

        errorCode_out = avp->errorCode;
        pid_out = avp->pid;

        SetStateForAllPartitions( wholeDiskToEject, kDiskStateIdle );
    }

    /* Send each waiting client a notification with the errorcode and dissenter. */

    SendEjectPostNotifyMsgsForAllPartitions( wholeDiskToEject, errorCode_out, pid_out );

Return:
        return;

} // CompleteEjectForDisk


//------------------------------------------------------------------------

DiskState AreWeBusyForDisk( DiskPtr diskPtr )
{
        DiskState result;

        result = kDiskStateIdle;

        switch ( diskPtr->state )
        {
                case kDiskStateToBeUnmounted:
                case kDiskStateToBeEjected:
                case kDiskStateToBeUnmountedAndEjected:
                case kDiskStateWaitingForMountApproval:
                    result = diskPtr->state;
                    break;

                case kDiskStateIdle: /* not busy */
                case kDiskStateNew: /* not busy */
                case kDiskStateNewlyEjected: /* not busy */
                case kDiskStateNewlyUnmounted: /* not busy */
                default:
                        /* do nothing - result initialized to zero / kDiskStateIdle */
                break;
        }

//Return:
        if (result) {
            dwarning(("%s() => %s (%d)\n", __FUNCTION__, DISKSTATE(result), result));
        }
        return result;

} // AreWeBusyForDisk


//------------------------------------------------------------------------

// write out our pid


static void
writepid(void)
{
        FILE *fp;

        fp = fopen(PID_FILE, "w");
        if (fp != NULL)
        {
                fprintf(fp, "%d\n", getpid());
                fclose(fp);
        }
}


//------------------------------------------------------------------------

/* 
static int isRequestingUserAdminUser(uid_t uid)
{
    int admin = 0;
    struct passwd *p;
    struct group *g;
    char **m;
    
    p = getpwuid(uid);
    if (p == NULL) {
            pwarning(("Can't find user\n"));
            admin = 0;
            goto end;
    }

    g = getgrnam("admin");
    if (g == NULL) {
            pwarning(("Can't find the admin group\n"));
            admin = 0;
            goto end;
    }

    if (p->pw_gid == g->gr_gid) {
            admin = 1;
            goto end;
    }

    m = g->gr_mem;
    
    while (*m) {
            if (strcmp(p->pw_name, *m) == 0) {
                    admin = 1;
                    break;
            }
            m++;
    }
    
end:    
    // otherwise
    return admin;
}
*/

int authorizationAllowedForEvent(ClientPtr client, char *event)
{
        OSStatus err = 0;
        int authorized = 0;
        AuthorizationRights rights;
        AuthorizationItem items[1];
        
        if (!client->clientAuthRef) {
                return 0;
        }

        items[0].name = event;
        items[0].value = NULL;
        items[0].valueLength = 0;
        items[0].flags = 0;

        rights.count = 1;
        rights.items = items;

        err = AuthorizationCopyRights(client->clientAuthRef, &rights, kAuthorizationEmptyEnvironment, kAuthorizationFlagExtendRights | kAuthorizationFlagInteractionAllowed | kAuthorizationFlagDestroyRights, NULL); 

        authorized = (errAuthorizationSuccess == err);

        return authorized;
	        
}

// Check our security to modify the disk

int requestingClientHasPermissionToModifyDisk(pid_t pid, DiskPtr diskPtr, char *right)
{
        ClientPtr client = nil;

        if (pid) {
                client = LookupClientByPID(pid);
        }
        
	// root requests it, always say YES
        if (requestorUID == 0 || pid == 0) {
                dwarning(("autodiskmount: requestor is root\n"));

                return TRUE;
        }

	// The person who mounted it requests it, say YES
        if (requestorUID == diskPtr->mountedUser) {
                dwarning(("autodiskmount: requestor mounted disk\n"));
                return TRUE;
        }

	// The person who owns the mount point requests it, say YES
        {
                struct stat statbuf;
                if ( stat(diskPtr->mountpoint, &statbuf) >= 0 ) {
                	if (statbuf.st_uid == requestorUID) {
                                dwarning(("autodiskmount: requestor owns mount point\n"));
                                return TRUE;
                        } 
                }
        }

        if (!client && !pid){
                
        }

        if (client && authorizationAllowedForEvent(client, right)) {
                return TRUE;      
        }

        /* if (allowAdmin && isRequestingUserAdminUser(requestorUID)) {
                dwarning(("autodiskmount: requestor is admin\n"));
                return TRUE;
        } */
        
	return FALSE;
}
int DiskArbitrationServerMain(int argc, char* argv[])
{
	kern_return_t r;
	mach_port_t serverPort;

	mach_port_t ioNotificationPort;
	const char * ioNotificationType;
        io_iterator_t ioIterator;  // first match
        io_iterator_t ioIterator2; // terminated
        io_registry_entry_t	entry; // for flushing and arming



	mach_port_t portSet;
	
	mach_msg_header_t* msg;
	mach_msg_header_t* reply;

        mach_msg_timeout_t timeout;

	// Handle command-line arguments

	programName = argv[0];
	
	if ((argc > 1) && (argv[1][0] == '-') && (argv[1][1] == 'd'))
	{
		/* Do nothing.  Do not daemonize yourself.  Good for "-d"(ebugging). */
		gDebug = 1;
		gDaemon = 0;
		dwarning(("DiskArbitration is not a daemon\n"));
	}
	else
	{
            gDaemon = 1;
		dwarning(("DiskArbitration is a daemon\n"));
	}        

	// Incoming message buffer needs room for message header, body, and trailer.
	msg = (mach_msg_header_t*)malloc(kMsgSize);

	// Outgoing message buffer needs room for only message header and body
	reply = (mach_msg_header_t*)malloc(kMsgSize);

        /*
        -- Cache needed "stuff"
        */
        cacheFileSystemDictionaries();
        cacheFileSystemMatchingArray();

        if (gDebug) {
            //printArgsForFsname("ufs");
            //printArgsForFsname("hfs");
            //printArgsForFsname("cd9660");
            //printArgsForFsname("msdos");
            //printArgsForFsname("udf");
            //printArgsForFsname("cdda");
            //printArgsForFsname("foobar");
        }

        /*  We have found CFSTR to not be thread safe, so let's initialize all CFSTR resources before moving on
                it appears that the CFSTR hash lookup is thread safe
        */
        {
                yankedHeader = YANKED_HEADER;
                yankedMessage = YANKED_MESSAGE;
                unrecognizedHeader = UNINITED_HEADER;
                unrecognizedHeaderNoInitialize = UNINITED_HEADER_NO_INIT;
                unrecognizedMessage = UNINITED_MESSAGE;
                ejectString = EJECT_BUTTON;
                ignoreString = IGNORE_BUTTON;
                initString = INIT_BUTTON;
                launchString = LAUNCH_BUTTON;
                someDisk = A_DISK;
                mountOrFsckFailedWithDiskUtility = MOUNT_FAILED_WITH_DU;
                mountOrFsckFailed = MOUNT_FAILED;
                unknownString = UNKNOWN_STRING;
        }

	
	/*
	-- Phase 1 - handle fixed disks
	*/

	// Create and register a notify port for this task
	
	r = InitNotifyPort();
	if (r != KERN_SUCCESS)
	{
		return -1;
	}
	
	// Get the IO Master Port
	
	r = IOMasterPort(bootstrap_port, &ioMasterPort);
	if (r != KERN_SUCCESS)
	{
		LogErrorMessage("(%s:%d) IOMasterPort failed: {0x%x} %s\n", __FILE__, __LINE__, r, mach_error_string(r));
		return -1;
	}

	// Allocate a port for receiving IO notifications

	r = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &ioNotificationPort);
	if (r != KERN_SUCCESS)
	{
		LogErrorMessage("(%s:%d) mach_port_allocate failed: {0x%x} %s\n", __FILE__, __LINE__, r, mach_error_string(r));
		return -1;
	}

	// Register for IO notifications of "IOMedia" objects and get an IO iterator in return

        ioNotificationType = kIOFirstMatchNotification;
        r = IOServiceAddNotification(	ioMasterPort, ioNotificationType,
                                                                        IOServiceMatching( "IOMedia" ),
                                                                        ioNotificationPort, (unsigned int) ioNotificationType,
                                                                        &ioIterator );
        if (r != KERN_SUCCESS)
        {
                LogErrorMessage("(%s:%d) IOServiceAddNotification failed: {0x%x} %s\n", __FILE__, __LINE__, r, mach_error_string(r));
                return -1;
        }

        // Get the existing disks from the IO Registry and arm the notification

        GetDisksFromRegistry( ioIterator, 1, 0 );

        ioNotificationType = kIOTerminatedNotification;
        r = IOServiceAddNotification(	ioMasterPort, ioNotificationType,
                                                                        IOServiceMatching( "IOMedia" ),
                                                                        ioNotificationPort, (unsigned int) ioNotificationType,
                                                                        &ioIterator2 );
        if (r != KERN_SUCCESS)
        {
                LogErrorMessage("(%s:%d) IOServiceAddNotification failed: {0x%x} %s\n", __FILE__, __LINE__, r, mach_error_string(r));
                return -1;
        }

        while ( (entry = IOIteratorNext( ioIterator2 )) ) {  // clear the iterator and move on
            IOObjectRelease( entry );
        }

	// Perform auto-mounting (including probing and fsck) for each disk we discovered

	autodiskmount(FALSE);
	
	/*
	-- Phase 2 - optionally daemonize and re-initialize ports in the child process
	*/

	// Daemonize
	
	dwarning(("Non-debug version calls daemon() here...\n"));
	
	if ( gDaemon )
	{
                // Free all the old disks (they have bad IOService pointers)
                // we *have* to do this before daemon or we will leak io_service_t objects

                FreeAllDisks();

		if (-1 == daemon(0, 0))
		{
			return -1;
		}
            
		/*
		-- Port re-initialization for child process
		*/
	
		// Create and register a notify port for this task
		
		r = InitNotifyPort();
		if (r != KERN_SUCCESS)
		{
			return -1;
		}
		
		// Get the IO Master Port
		
		r = IOMasterPort(bootstrap_port, &ioMasterPort);
		if (r != KERN_SUCCESS)
		{
			LogErrorMessage("(%s:%d) IOMasterPort failed: {0x%x} %s\n", __FILE__, __LINE__, r, mach_error_string(r));
			return -1;
		}
		
		// Allocate a port for receiving IO notifications
	
		r = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &ioNotificationPort);
		if (r != KERN_SUCCESS)
		{
			LogErrorMessage("(%s:%d) mach_port_allocate failed: {0x%x} %s\n", __FILE__, __LINE__, r, mach_error_string(r));
			return -1;
		}
	
		// Register for IO notifications of "IOMedia" objects and get an IO iterator in return
	
		ioNotificationType = kIOFirstMatchNotification;
		r = IOServiceAddNotification(	ioMasterPort, ioNotificationType,
										IOServiceMatching( "IOMedia" ),
										ioNotificationPort, (unsigned int) ioNotificationType,
										&ioIterator );
		if (r != KERN_SUCCESS)
		{
			LogErrorMessage("(%s:%d) IOServiceAddNotification failed: {0x%x} %s\n", __FILE__, __LINE__, r, mach_error_string(r));
			return -1;
		}


                
		// Get the existing disks from the IO Registry and arm the notification

                GetDisksFromRegistry( ioIterator, 0, 0 );

                ioNotificationType = kIOTerminatedNotification;
                r = IOServiceAddNotification(	ioMasterPort, ioNotificationType,
                                                                                IOServiceMatching( "IOMedia" ),
                                                                                ioNotificationPort, (unsigned int) ioNotificationType,
                                                                                &ioIterator2 );
                if (r != KERN_SUCCESS)
                {
                        LogErrorMessage("(%s:%d) IOServiceAddNotification failed: {0x%x} %s\n", __FILE__, __LINE__, r, mach_error_string(r));
                        return -1;
                }

                while ( (entry = IOIteratorNext( ioIterator2 )) ) {  // clear the iterator and move on
                    IOObjectRelease( entry );
                }

		// Perform auto-mounting (including probing and fsck) for each disk we discovered
		// This should be a no-op here, since we probably discovered all these disks before daemonizing
	
		autodiskmount(FALSE);

    } else {
        // this is being run debug, let's go ahead and set the current user to the owner of /dev/console
        int uid = 0;
        int gid = 0;

        CFStringRef name = SCDynamicStoreCopyConsoleUser(NULL, &uid, &gid);

        if (name) {
            dwarning(("Setting currentConsoleUser to be %d, %d\n", uid, gid));
            currentConsoleUser = uid;
            CFRelease(name);
        } else {
            currentConsoleUser = 0;
        }
    }
	
	/*
	-- Phase 3 - become a server
	*/

	// Create the server port with receive and send rights
	
	r = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &serverPort);
	if (r != KERN_SUCCESS)
	{
		LogErrorMessage("(%s:%d) mach_port_allocate failed: {0x%x} %s\n", __FILE__, __LINE__, r, mach_error_string(r));
		return -1;
	}
	
	r = mach_port_insert_right(mach_task_self(), serverPort, serverPort, MACH_MSG_TYPE_MAKE_SEND);
	if (r != KERN_SUCCESS)
	{
		LogErrorMessage("(%s:%d) mach_port_insert_right failed: {0x%x} %s\n", __FILE__, __LINE__, r, mach_error_string(r));
		return -1;
	}

	dwarning(("%s: serverPort = %d\n", programName, (int)serverPort));

	// Register the server port with the bootstrap server so clients can find it
	
	r = bootstrap_register(bootstrap_port, DISKARB_SERVER_NAME, serverPort);
	if (r != KERN_SUCCESS)
	{
		return -1;
	}
	
	// Create a port set....

	r = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_PORT_SET, &portSet);
	if (r != KERN_SUCCESS)
	{
		LogErrorMessage("(%s:%d) mach_port_allocate failed: {0x%x} %s\n", __FILE__, __LINE__, r, mach_error_string(r));
		return -1;
	}

	// ... and add the notify, server, and IO notification ports to it.

	r = mach_port_move_member(mach_task_self(), GetNotifyPort(), portSet);
	if (r != KERN_SUCCESS)
	{
		LogErrorMessage("(%s:%d) mach_port_move_member (notify port) failed: {0x%x} %s\n", __FILE__, __LINE__, r, mach_error_string(r));
		return -1;
	}

	r = mach_port_move_member(mach_task_self(), serverPort, portSet);
	if (r != KERN_SUCCESS)
	{
		LogErrorMessage("(%s:%d) mach_port_move_member (server port) failed: {0x%x} %s\n", __FILE__, __LINE__, r, mach_error_string(r));
		return -1;
	}

	r = mach_port_move_member(mach_task_self(), ioNotificationPort, portSet);
	if (r != KERN_SUCCESS)
	{
		LogErrorMessage("(%s:%d) mach_port_move_member (IO notification port) failed: {0x%x} %s\n", __FILE__, __LINE__, r, mach_error_string(r));
		return -1;
	}
    if (!g.readOnlyBoot) {
        // write out our pid
        writepid();
    }

	// Server loop
ServerLoop:    

	while( 1 )
	{
                DiskPtr diskPtr;
                DiskPtr nextDiskPtr;
		mach_port_t deadPort;
		DiskState busyState = NULL;
                mach_msg_format_0_trailer_t	*trailer;

                nextDiskPtr = g.Disks;

                for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = nextDiskPtr)
                {
                        /* Are we (still) busy, i.e., in the midst of processing an async unmount transaction? */

                        nextDiskPtr = diskPtr->next;
                        busyState = AreWeBusyForDisk(diskPtr);

                        if ( kDiskStateToBeUnmounted == busyState || kDiskStateToBeUnmountedAndEjected == busyState )
                        {
                                CompleteUnmountForDisk(diskPtr);
                                // Note: Recalculate <busyState> since we may have completed an unmount w/ zero clients.
                                if (NumUnsetAckValuesForAllDisks() == 0) {
                                        SendCompletedMsgs(kDiskArbCompletedPostUnmount, 0);
                                }
                        }
                }

                nextDiskPtr = g.Disks;

                for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = nextDiskPtr)
                {
                        /* Are we (still) busy, i.e., in the midst of processing an async eject transaction? */

                        nextDiskPtr = diskPtr->next;
                        busyState = AreWeBusyForDisk(diskPtr);

                        if ( kDiskStateToBeEjected == busyState )
                        {
                                CompleteEjectForDisk(diskPtr);
                                // Note: Recalculate <busyState> since we may have completed an unmount w/ zero clients.
                                if (NumUnsetAckValuesForAllDisks() == 0) {
                                        SendCompletedMsgs(kDiskArbCompletedPostEject, 0);
                                }
                                busyState = AreWeBusyForDisk(diskPtr);
                        }

                        /* Send out async notifications about to any to-be-unmounted/ejected disks */

                        if ( kDiskStateToBeUnmounted == busyState || kDiskStateToBeUnmountedAndEjected == busyState )
                        {
                                SendPreUnmountMsgsForDisk(diskPtr);
                        }

                        /* Send out async notifications about to any to-be-ejected disks */

                        if ( kDiskStateToBeEjected == busyState )
                        {
                                SendPreEjectMsgsForDisk(diskPtr);
                        }
                        
                        if ( kDiskStateWaitingForMountApproval == busyState )
                        {
                                SendPreMountMsgsForDisk(diskPtr);
                        }
                }


                nextDiskPtr = g.Disks;

                for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = nextDiskPtr)
                {

                        nextDiskPtr = diskPtr->next;
                        if ((diskPtr->flags & kDiskArbDiskAppearedUnrecognizableFormat) && (diskPtr->state == kDiskStateNew)) {

                                ClientPtr nextClientPtr = GetNextClientForDisk(diskPtr);

                                if (nextClientPtr) {
                                     SendUnrecognizedDiskArbitrationMsgs(nextClientPtr->port, diskPtr->ioBSDName, "", "", (!((diskPtr->flags & kDiskArbDiskAppearedLockedMask) == kDiskArbDiskAppearedLockedMask)), ((diskPtr->flags & kDiskArbDiskAppearedEjectableMask) == kDiskArbDiskAppearedEjectableMask), IsWhole(diskPtr), DiskTypeForDisk(diskPtr));
                                    SetStateForOnePartition(diskPtr, kDiskStateUnrecognized);
                                } else {
                                        // check the new whole disks for a new disk that does not contain a recognizable section
                                        DiskPtr wholePtr = LookupWholeDiskForThisPartition(diskPtr);

                                        int dialogPreviouslyDisplayed = ( wholePtr->flags & kDiskArbDiskAppearedDialogDisplayed );
                                        int neverMount = ( wholePtr->flags & kDiskArbDiskAppearedNoMountMask );

                                        if (!dialogPreviouslyDisplayed && !neverMount) {
                                                int recognizablePartitionsAppeared = ( wholePtr->flags & kDiskArbDiskAppearedRecognizableSectionMounted );
                                                if (!recognizablePartitionsAppeared && wholePtr->mountAttempted && !IsNetwork(wholePtr)) {

                                                        // display the dialog
                                                        if (DiskArbIsHandlingUnrecognizedDisks()) {
                                                                StartUnrecognizedDiskDialogThread(wholePtr);
                                                                wholePtr->flags |= kDiskArbDiskAppearedDialogDisplayed;
                                                        }
                                                }
                                        }
                                }
                        }
                }

		/* Send out synchronous notifications out to any new clients and about any new disks */

		//if ( kDiskStateIdle == busyState )
		{
                        int newDisks = SendDiskAppearedMsgs();
                        SendCompletedMsgs(kDiskArbCompletedDiskAppeared, newDisks);

		}

		/* For each disk, if it was Ejected, then delete it, and if it was Unmounted, then mark it Idle again */

                nextDiskPtr = g.Disks;
                DA_postponedDisksExist = 0;
                DA_stateChangedSpinRunLoop = 0;

                for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = nextDiskPtr)
                {
                       nextDiskPtr = diskPtr->next;
                       if (diskPtr->wholeDiskHasBeenYanked) {
                                FreeDisk(diskPtr);
                        }
               }

                nextDiskPtr = g.Disks;
		
		for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = nextDiskPtr)
		{
                        nextDiskPtr = diskPtr->next;
			switch ( diskPtr->state )
			{
                case kDiskStateNewlyUnmounted:
                    SetStateForOnePartition( diskPtr, kDiskStateIdle );
                break;
                case kDiskStateWaitingForMountApproval:
                    if (NumUnsetAckValues(diskPtr->ackValues) == 0) {
                        AckValue *avp = GetDissenterFromAckValues( diskPtr->ackValues );
                        boolean_t mountingAllowed = FALSE;
                        boolean_t eject = FALSE;
                        boolean_t readOnly = FALSE;

                       int flags;

                       if ( NULL == avp ) {
                            flags = kDiskArbApproveMounting;
                        } else {
                            flags = avp->errorCode;
                        }

                        if (flags == kDiskArbApproveMounting) {
                            mountingAllowed = TRUE;
                        }
                        
                        if (flags & kDiskArbEjectDevice) {
                            eject = TRUE;
                        }
                        
                        if (flags & kDiskArbMountReadOnly) {
                            mountingAllowed = TRUE;
                            readOnly = TRUE;
                        }

                        if (flags & kDiskArbRequireAuthentication) {
                            ClientPtr client = LookupClientByPID(avp->pid);

                            if (client && authorizationAllowedForEvent(client, "system.volume.mount")) {
                                mountingAllowed = TRUE;
                            } else {
                                mountingAllowed = FALSE;
                            }
                        }
                        
                        if (flags & kDiskArbDisallowMounting) {
                            mountingAllowed = FALSE;
                        }

                        if (mountingAllowed) {                            
                            dwarning(("Making a disk new - no one is objecting ...\n"));
                            SetStateForOnePartition( diskPtr, kDiskStateNew );
                            diskPtr->approvedForMounting++;
                            DA_stateChangedSpinRunLoop++;

                            if (readOnly) {
                                dwarning(("Making a disk new - and it should be mounted read only ...\n"));
                                // mark the disk for read only-ness
                                diskPtr->flags |= kDiskArbDiskAppearedLockedMask;
                            }
                            
                        } else {
                            dwarning(("There is a dissenter and you can't mount the disk\n"));
                            SetStateForOnePartition(diskPtr, kDiskStateMountApprovalDenied);
                            diskPtr->approvedForMounting = 0;
                            DA_stateChangedSpinRunLoop++;

                            if (eject) {
                                dwarning(("The disk should be ejecting now\n"));
                                DiskArbEjectRequest_async_rpc( 0, 0, diskPtr->ioBSDName, FALSE);
                                goto ServerLoop;
                            }
                        }
                    }
                    break;

				case kDiskStateNewlyEjected:
					FreeDisk( diskPtr );
				break;

                                /* go through all the disks, looking for postponed disks, if those
                                disks have a whole disk and the whole disks service isn't busy, mark it as
                                new, otherwise, schedule the system to try again
                                */
                                case kDiskStatePostponed:
                                        {
                                                DiskPtr wholeDiskPtr = LookupWholeDiskForThisPartition(diskPtr);
                                                if (wholeDiskPtr) {
                                                        int busy;

                                                        IOServiceGetBusyState(wholeDiskPtr->service, &busy);

                                                        if (!busy) {
                                                                dwarning(("Setting disk state to new, whole is there and not busy\n"));
                                                                SetStateForOnePartition(diskPtr, kDiskStateNew);
                                                                DA_stateChangedSpinRunLoop++;
                                                        } else {
                                                                dwarning(("Some whole disk is busy\n"));
                                                                DA_postponedDisksExist++;
                                                        }
                                                } else {
                                                       dwarning(("Some whole disk is missing\n"));
                                                       DA_postponedDisksExist++;
                                                }
                                        }
                                        break;


				case kDiskStateIdle: /* do nothing */
				case kDiskStateNew: /* do nothing */
				case kDiskStateToBeUnmounted: /* do nothing - waiting for acknowledgements */
				case kDiskStateToBeEjected: /* do nothing - waiting for acknowledgements */
				case kDiskStateToBeUnmountedAndEjected: /* do nothing - waiting for acknowledgements */
				default:
					/* do nothing */
				break;
			}
		}



		if ( g.NumDisksAddedOrDeleted )
		{
			PrintDisks();
			g.NumDisksAddedOrDeleted = 0;
		}

                if (DA_postponedDisksExist) {
                        io_iterator_t ioIterator;
                        mach_port_t masterPort;
                        kern_return_t err;
                        dwarning(("Some disk is postponed - waiting one second and re-running autodiskmount loop\n"));

                        err = IOMasterPort(bootstrap_port, &masterPort);

                        // get an iterator from the registry for IOMedia and get the disks out of the registry ...
                        err = IOServiceGetMatchingServices(masterPort, IOServiceMatching("IOMedia"), &ioIterator);

                        GetDisksFromRegistry( ioIterator, 0, 0 );
                        IOObjectRelease( ioIterator );
                        continue;
                }

                if (DA_stateChangedSpinRunLoop) {
                        dwarning(("Some disk is now new or denied - sending ourselves a new disk message\n"));
                        autodiskmount(FALSE);
                        continue;
                }

		/* Wait for: client requests, disk insertions, and death notifications */

                /* if there are clients who we are waiting on acknowledgements from we need to
                    set a timeout on our recieve, if the timeout passes, then the clients never
                    responded and we should allow everything to continue without them (or post
                    a warning to the user in some format ... CFUserNotification?) */

                if (NumUnsetAckValuesForAllDisks() != 0) {
                    timeout = (mach_msg_timeout_t)(15000);
                } else {
                    timeout = MACH_MSG_TIMEOUT_NONE;
                }

		dwarning(("%s: mach_msg\n", programName));
                r = mach_msg(msg, MACH_RCV_MSG | (MACH_MSG_TIMEOUT_NONE == timeout ? 0 : MACH_RCV_TIMEOUT) | MACH_RCV_TRAILER_TYPE(MACH_RCV_TRAILER_SENDER) | MACH_RCV_TRAILER_ELEMENTS(2), 0, kMsgSize, portSet, timeout, MACH_PORT_NULL);
                if (r == MACH_RCV_TIMED_OUT) {
                    AckValue *unresponder = GetUnresponderFromAckValuesForAllDisks();

                    if (unresponder && unresponder->pid) {
                            Client *unrespondingClient = LookupClientByPID(unresponder->pid);
                            mach_port_t unrespondingPort = unrespondingClient->port;
                            pwarning(("Someone (pid = (%d)) isn't responding to a sent disk acknowledgement request. That client has been disconnected.\n", unresponder->pid));
                            SendClientWasDisconnectedMsg(unrespondingClient);
                            ClientDeath(unrespondingPort, FALSE);
                    } else {
                            dwarning(("Someone timed out, but we don't know who ..."));
                    }
                }
                
		if (r != KERN_SUCCESS) {
                        LogErrorMessage("(%s:%d) mach_msg failed: {0x%x} %s\n", __FILE__, __LINE__, r, mach_error_string(r));
			continue;
		}

                /* check the security of the mach message
                *        set the requestor id to be the uid of the sender of the mach message
                */
                {
                        mig_reply_error_t		*request = (mig_reply_error_t *)msg;

                        trailer = (mach_msg_security_trailer_t *)((vm_offset_t)request +
                                                                  round_msg(request->Head.msgh_size));

                        requestingClientPort = msg->msgh_local_port;

                        if ((trailer->msgh_trailer_type == MACH_MSG_TRAILER_FORMAT_0) &&
                                (trailer->msgh_trailer_size >= MACH_MSG_TRAILER_FORMAT_0_SIZE)) {
                                dwarning(("set REQUESTOR ID for this mach message to be = %d\n", trailer->msgh_sender.val[0]));

                                requestorUID = trailer->msgh_sender.val[0];
                                requestorGID = trailer->msgh_sender.val[1];
                                
                        } else {
                                static boolean_t warned = FALSE;

                                if (!warned) {
                                        pwarning(("Caller credentials not available."));
                                        warned = TRUE;
                                }
                        }
                }
                
		/* Demultiplex based on the port: notification port or server port */

		if ( msg->msgh_local_port == GetNotifyPort() )
		{
			dwarning(("%s: Message received on notification port\n", programName));

			/* This test is partially redundant, and that's alright */
			
			if ( MessageIsNotificationDeath( msg, & deadPort ) == TRUE )
			{
				dwarning(("Death of port %d\n", (int)deadPort));
				
				/* Deallocate the send right that came in the dead name notification */
	
				{
					kern_return_t r;
			
					r = mach_port_deallocate( mach_task_self(), deadPort );
					if ( r ) dwarning(("%s(client = $%08x): mach_port_deallocate(...,$%08x) => $%08x: %s\n", __FUNCTION__, (int)deadPort, (int)deadPort, r, mach_error_string(r)));
				}
				
				/* Clean up the resources consumed by the client record */
				
				ClientDeath( deadPort, TRUE );
			}
			else
			{
				/* Do nothing - MessageIsNotificationDeath() will log an error in this case */
			}
		}
		else if ( msg->msgh_local_port == serverPort )
		{
                        dwarning(("%s: Message received on server port from port $%08x\n", programName, (int)(msg->msgh_remote_port)));
			bzero( reply, sizeof( mach_msg_header_t ) );
			(void) ClientToServer_server(msg, reply);
			(void) mach_msg_send(reply);
		}
		else if ( msg->msgh_local_port == ioNotificationPort )
		{
                        dwarning(("%s: Message received on IO notification port from port $%08x\n", programName, (int)(msg->msgh_remote_port)));
			HandleIONotificationMsg( (IONotificationMsgPtr) msg );
		}
		else
		{
			// This should never happen.  Log it.
			LogErrorMessage("Unrecognized receive port %d.\n", (int)(msg->msgh_local_port));
		}

                requestorUID = -1;
                requestingClientPort = -1;

	} /* while (1) */
	
	// We only exit the server loop if something goes wrong

	dwarning(("%s: exit\n", programName));

	return -1;

}

int DiskTypeForDisk(DiskPtr diskPtr)
{
        int value = 0;

        if (diskPtr->flags & kDiskArbDiskAppearedCDROMMask) {
                if (diskPtr->flags & kDiskArbDiskAppearedNoSizeMask) {
                        value = kDiskArbHandlesUninitializedCDMedia;
                } else {
                        value = kDiskArbHandlesUnrecognizedCDMedia;
                }

        } else if (diskPtr->flags & kDiskArbDiskAppearedDVDROMMask) {
                if (diskPtr->flags & kDiskArbDiskAppearedNoSizeMask) {
                        value = kDiskArbHandlesUninitializedDVDMedia;
                } else {
                        value = kDiskArbHandlesUnrecognizedDVDMedia;
                }
        } else if (diskPtr->flags & kDiskArbDiskAppearedEjectableMask) {
                if (diskPtr->flags & kDiskArbDiskAppearedNoSizeMask) {
                        value = kDiskArbHandlesUninitializedOtherRemovableMedia;
                } else {
                        value = kDiskArbHandlesUnrecognizedOtherRemovableMedia;
                }
        } else {
                if (diskPtr->flags & kDiskArbDiskAppearedNoSizeMask) {
                        value = kDiskArbHandlesUninitializedFixedMedia;
                } else {
                        value = kDiskArbHandlesUnrecognizedFixedMedia;
                }
        }
        
        return value;
}

CFComparisonResult compareClientPriorities(const void *val1, const void *val2, void *context)
{
    int val1Priority = ((ClientPtr)val1)->unrecognizedPriority;
       int val2Priority = ((ClientPtr)val2)->unrecognizedPriority;

   if (val1Priority > val2Priority) {
           return kCFCompareLessThan;
    } else if (val1Priority < val2Priority) {
            return kCFCompareGreaterThan;
    }

    return kCFCompareEqualTo;
}

ClientPtr GetNextClientForDisk(DiskPtr diskPtr)
{
	// create a CFArray
        CFMutableArrayRef array = CFArrayCreateMutable(NULL,0,NULL);
        
	// make sure to only return clients that have the unrecognized callback set up ...
        ClientPtr lastClient = diskPtr->lastClientAttemptedForUnrecognizedMessages;
        ClientPtr clientPtr;
        ClientPtr nextClientPtr;
        int i = 0;
        int count = 0;
        int lastClientIndex = 0;
        int nextClientIndex = 0;

        if (lastClient && (lastClient->pid == 0 || lastClient->port == 0)) {
                lastClient = 0x0;
        }
        
        // add all the clients to the array.
        // only add the clients that have a priority non zero and do match the disk type
        for (clientPtr = g.Clients, i = 0; clientPtr != NULL; clientPtr = clientPtr->next, i++)
        {
                if ((clientPtr->unrecognizedPriority > 0) && (DiskTypeForDisk(diskPtr) & clientPtr->notifyOnDiskTypes)) {
                        CFArrayAppendValue(array, clientPtr);
                }
        }

        count = CFArrayGetCount(array);

        // return nil if the count == 0
        if (0 == count) {
                nextClientPtr = nil;
                goto Return;
        }
        
        // if the first element != lastClient, return it, other wise return nil

        // if for some reason the last client deregisters, then we need to make sure that the client in the list is the one we want
        if (lastClient && 1 == count && lastClient ==(ClientPtr)CFArrayGetValueAtIndex(array, 0)) {
                nextClientPtr = nil;
                goto Return;
        }

        if (!lastClient && 1 == count) {
                nextClientPtr = (ClientPtr)CFArrayGetValueAtIndex(array, 0);
                goto Return;
        }
        
        // sort the final array by priority
        CFArraySortValues(array, CFRangeMake(0, CFArrayGetCount(array)), compareClientPriorities, NULL);


	// find the index of the lastClient (if it exists).
        if (lastClient) {
                lastClientIndex = CFArrayGetFirstIndexOfValue(array, CFRangeMake(0, CFArrayGetCount(array)), lastClient);
                dwarning(("client was set, index found, %d\n", lastClientIndex));
                nextClientIndex = lastClientIndex + 1;
        }

        
        // if the last client is the last element, return nil
        if (nextClientIndex == count ) {
                nextClientPtr = nil;
                goto Return;
        } else {
                nextClientPtr = (ClientPtr)CFArrayGetValueAtIndex(array, nextClientIndex);
                // return the element *after* the last client element
                goto Return;
        }

Return:
        CFRelease(array);
        diskPtr->lastClientAttemptedForUnrecognizedMessages = nextClientPtr;
        if (nextClientPtr) {
                nextClientPtr->ackOnUnrecognizedDisk = diskPtr;
        }
        return nextClientPtr;
}
