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

/*
 * DiskVolume.m
 * - objects for DiskVolume and DiskVolumes
 * - a convenient way to store information about filesystems (volumes)
 *   and perform actions on them
 */

/*
 * Modification History:
 *
 * Dieter Siegmund (dieter@apple.com) Thu Aug 20 18:31:29 PDT 1998
 * - initial revision
 * Dieter Siegmund (dieter@apple.com) Thu Oct  1 13:42:34 PDT 1998
 * - added support for hfs and cd9660 filesystems
 * Brent Knight (bknight@apple.com) Thu Apr  1 15:54:48 PST 1999
 * - adapted for Beaker, with caveats described in Radar 2320396
 * Brent Knight (bknight@apple.com) Fri Apr  9 11:16:04 PDT 1999
 * - [2320396] added support for ioWritable/ioRemovable
 * Brent Knight (bknight@apple.com) Thu Sep  9 11:36:01 PDT 1999
 * - added support for fsck_hfs, disk insertion and ejection
 * Dieter Siegmund (dieter@apple.com) Wed Nov 10 10:58:43 PST 1999
 * - added support for named UFS volumes
 * - added logic to use the NeXT label name if a "new" ufs label is
 *   not present
 * - homogenized the filesystem probing logic for ufs, hfs, and cd9660
 * - homogenized the fsck logic for ufs and hfs
 */


#include <libc.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <bsd/dev/disk.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <grp.h>
#include <ctype.h>
#include <dirent.h>
#include <pthread.h>

#include <mach/boolean.h>
#include <sys/loadable_fs.h>
#include <bsd/fsproperties.h>

#include <CoreFoundation/CoreFoundation.h>

#include "DiskVolume.h"
#include "GetRegistry.h"
#include "DiskArbitrationServerMain.h"
#include "Configuration.h"

#include <errno.h>
#include <stdio.h>

#include <sys/attr.h>

#define kIsInvisible 0x4000

#define DEVICE_SUID	"suid"
#define DEVICE_NOSUID	"nosuid"
#define DEVICE_DEV	"dev"
#define DEVICE_NODEV	"nodev"

/*
 * Generic Finder file/dir data
 */
struct FinderInfo {
        u_int32_t 	opaque_1[2];
        u_int16_t 	fdFlags;	/* Finder flags */
        int16_t 	opaque_2[11];
};
typedef struct FinderInfo FinderInfo;

/* getattrlist buffers start with an extra length field */
struct FinderAttrBuf {
        unsigned long	infoLength;
        FinderInfo	finderInfo;
};
typedef struct FinderAttrBuf FinderAttrBuf;


#define MAXNAMELEN	256

extern CFMutableArrayRef matchingArray;

int DiskArbIsHandlingUnrecognizedDisks(void)
{
        ClientPtr clientPtr;
        int i;
        for (clientPtr = g.Clients, i = 0; clientPtr != NULL; clientPtr = clientPtr->next, i++)
        {
                if (clientPtr->flags & kDiskArbClientHandlesUninitializedDisks) {
                        return FALSE;
                }
        }
        return TRUE;
}

extern mach_port_t gNotifyPort;
extern int requestorUID;

void * UnrecognizedDiskDiscovered(void * args)
{
        DiskPtr diskPtr = args;
        // display UI to inform the user that an unrecognized disk was inserted.  this must be done asynch.
        // we also need to pass the diskPtr so that it can be ejected
        SInt32			retval = ERR_SUCCESS;
        CFOptionFlags 		responseFlags;
        CFURLRef		daFrameworkURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, CFSTR("/System/Library/PrivateFrameworks/DiskArbitration.framework"), kCFURLPOSIXPathStyle, TRUE);

        // use the url of the DiskArbitration Framework and put a Localizable.strings file in there.

        if (currentConsoleUser >= 0 ) {  //someone's logged in
                retval = CFUserNotificationDisplayAlert(60.0, kCFUserNotificationStopAlertLevel, NULL, NULL, daFrameworkURL, unrecognizedHeader, unrecognizedMessage, ejectString, ignoreString, initString, &responseFlags);
        } else { // someone's at the login window
                retval = CFUserNotificationDisplayAlert(60.0, kCFUserNotificationStopAlertLevel, NULL, NULL, daFrameworkURL, unrecognizedHeaderNoInitialize, unrecognizedMessage, ejectString, ignoreString, NULL, &responseFlags);
        }
        if (responseFlags == kCFUserNotificationDefaultResponse) {
                // Eject the disk
                DiskPtr localPtr = LookupWholeDiskForThisPartition(diskPtr);

                if (localPtr && localPtr->state != kDiskStateNewlyEjected) {
                        mach_msg_empty_send_t	msg;

                        if (diskPtr->ioBSDName) {
                                requestorUID = 0;
                        	DiskArbUnmountAndEjectRequest_async_rpc( 0, 0, diskPtr->ioBSDName, FALSE);
                        }

                        msg.header.msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_MAKE_SEND, 0);
                        msg.header.msgh_size = sizeof(msg);
                        msg.header.msgh_remote_port = gNotifyPort;
                        msg.header.msgh_local_port =  NULL;
                        msg.header.msgh_id = INTERNAL_MSG;
                        //send a mach_msg to the main thread to kick it in to action ...
                        mach_msg(&msg.header,		/* msg */
                                MACH_SEND_MSG,			/* option */
                                msg.header.msgh_size,	/* send_size */
                                0,			/* rcv_size */
                                MACH_PORT_NULL,		/* rcv_name */
                                MACH_MSG_TIMEOUT_NONE,	/* timeout */
                                MACH_PORT_NULL);		/* notify */
                }                
        } else if (responseFlags == kCFUserNotificationAlternateResponse) {
                // Ignore and continue
        } else if (responseFlags == kCFUserNotificationOtherResponse) {
                // Launch Disk Utility
                // switch to the console user uid and ...
                if (currentConsoleUser >= 0) {
                        seteuid(currentConsoleUser);
                        system("/usr/bin/open \"/Applications/Utilities/Disk Utility.app\"");
                        // then switch back ...
                        seteuid(0);
                }
        }

        CFRelease(daFrameworkURL);

        return NULL;
}



void * DiskFsckOrMountFailed(void * args)
{
        DiskPtr diskPtr = args;
        // display UI to inform the user that an unrecognized disk was inserted.  this must be done asynch.
        // we also need to pass the diskPtr so that it can be ejected
        SInt32			retval = ERR_SUCCESS;
        CFOptionFlags 		responseFlags;
        CFURLRef		daFrameworkURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, CFSTR("/System/Library/PrivateFrameworks/DiskArbitration.framework"), kCFURLPOSIXPathStyle, TRUE);

        // use the url of the DiskArbitration Framework and put a Localizable.strings file in there.

        if (currentConsoleUser >= 0 ) {  //someone's logged in
                CFMutableStringRef localString = CFStringCreateMutable(kCFAllocatorDefault, 1024);  // build a string with the device name in it ...
                CFStringAppend(localString, someDisk);
                if (diskPtr->mountpoint && strlen(diskPtr->mountpoint)) {
                        CFStringAppendCString(localString, diskPtr->mountpoint, kCFStringEncodingUTF8);
                } else {
                        CFStringAppend(localString, unknownString);
                }
                CFStringAppend(localString, mountOrFsckFailedWithDiskUtility);

                retval = CFUserNotificationDisplayAlert(60.0, kCFUserNotificationStopAlertLevel, NULL, NULL, daFrameworkURL, localString, unrecognizedMessage, NULL, NULL, launchString, &responseFlags);
                CFRelease(localString);
        } else { // someone's at the login window
                CFMutableStringRef localString = CFStringCreateMutable(kCFAllocatorDefault, 1024);  // build a string with the device name in it ...
                CFStringAppend(localString, someDisk);
                if (diskPtr->mountpoint && strlen(diskPtr->mountpoint)) {
                        CFStringAppendCString(localString, diskPtr->mountpoint, kCFStringEncodingUTF8);
                } else {
                        CFStringAppend(localString, unknownString);
                }
                CFStringAppend(localString, mountOrFsckFailed);

                retval = CFUserNotificationDisplayAlert(60.0, kCFUserNotificationStopAlertLevel, NULL, NULL, daFrameworkURL, localString, unrecognizedMessage, NULL, NULL, NULL, &responseFlags);
                CFRelease(localString);
        }

        if (responseFlags == kCFUserNotificationOtherResponse) {
                // Launch Disk Utility
                // switch to the console user uid and ...
                if (currentConsoleUser >= 0) {
                        seteuid(currentConsoleUser);
                        system("/usr/bin/open \"/Applications/Utilities/Disk Utility.app\"");
                        // then switch back ...
                        seteuid(0);
                }
        }

        CFRelease(daFrameworkURL);

        return NULL;
}

// Disconnected arbitration messages
void StartUnrecognizedDiskDialogThread(DiskPtr disk)
{
    pthread_attr_t attr;
    pthread_t tid;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, UnrecognizedDiskDiscovered, disk);
    pthread_attr_destroy(&attr);
}

// A fsck or mount failed
void StartUnmountableDiskThread(DiskPtr disk)
{
    pthread_attr_t attr;
    pthread_t tid;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, DiskFsckOrMountFailed, disk);
    pthread_attr_destroy(&attr);
}

static struct statfs *
get_fsstat_list(int * number)
{
    int n;
    struct statfs * stat_p;

    n = getfsstat(NULL, 0, MNT_NOWAIT);
    if (n <= 0)
    {
		return (NULL);
    }

    stat_p = (struct statfs *)malloc(n * sizeof(*stat_p));
    if (stat_p == NULL)
    {
		return (NULL);
    }

    if (getfsstat(stat_p, n * sizeof(*stat_p), MNT_NOWAIT) <= 0)
    {
		free(stat_p);
		return (NULL);
    }

    *number = n;

    return (stat_p);
}

static struct statfs *
fsstat_lookup_spec(struct statfs * list_p, int n, char * spec, char * fstype)
{
    char 				alt_spec[MAXNAMELEN];
    int 				i;
    struct statfs * 	scan;

    sprintf(alt_spec, "/private%s", spec);
    for (i = 0, scan = list_p; i < n; i++, scan++)
    {
		if (strcmp(scan->f_fstypename, fstype) == 0
		    && (strcmp(scan->f_mntfromname, spec) == 0
			|| strcmp(scan->f_mntfromname, alt_spec) == 0))
		{
		    return (scan);
		}
    }
    return (NULL);
}

boolean_t
fsck_needed(char * devname, char * fstype)
{
    char command[1024];
    FILE * f;
    int ret;
    struct stat 	sb;
    char		fsck_command_line[1024];
    char *fsckCmd 	= repairPathForFileSystem(fstype);

    sprintf(command, "%s -q /dev/r%s", fsckCmd, devname);
    sprintf(fsck_command_line, fsckCmd);

    {
        /* determine if the fsck command line exists
         if it doesn't, set the volume clean */
        if (stat(fsck_command_line, &sb) != 0) {
            // the file doesn't exist
            dwarning(("%s: '%s' not found...\n", __FUNCTION__, fsck_command_line));
                free(fsckCmd);
            return (FALSE);
        }
    }

    
    dwarning(("%s('%s'): '%s'...\n", __FUNCTION__, devname, command));
    
    f = popen(command, "w");
    if (f == NULL)
    {
		dwarning(("%s('%s'): popen('%s') failed", __FUNCTION__, devname, command));
        // Couldn't find an appropriate fsck_* - assume one isn't needed
            free(fsckCmd);
		return (FALSE);
    }
    fflush(f);
    ret = pclose(f);
    free(fsckCmd);

    dwarning(("%s('%s'): '%s' => %d\n", __FUNCTION__, devname, command, ret));

    if (ret <= 0)
    {
		return (FALSE);
    }
    return (TRUE);
}

#define FILESYSTEM_ERROR	 		0
#define FILESYSTEM_MOUNTED 			1
#define FILESYSTEM_MOUNTED_ALREADY	2
#define FILESYSTEM_NEEDS_REPAIR 	     3

/* foreignLabel: return the label written to the file by the -p(robe) option of the fs.util program */

char * 
foreignLabel(char * fsName)
{
    int fd;
    char buf[MAXPATHLEN];

    char *theLabel;

    theLabel = malloc(MAXPATHLEN);
    
    sprintf(buf, "%s/%s%s/%s%s", FS_DIR_LOCATION, fsName, FS_DIR_SUFFIX, fsName, FS_LABEL_SUFFIX);
    fd = open(buf, O_RDONLY, 0);
    if (fd >= 0)
    {
		int i = read(fd, theLabel, 255);
		close(fd);
		if (i > 0)
		{
			theLabel[i] = '\0';
			return (theLabel);
		}
    }

    return (NULL);
}

/* foreignProbe: run the -p(robe) option of the given <fsName>.util program in a child process */

static int
foreignProbe(const char *fsName, const char *execPath, const char *probeCmd, const char *devName, int removable, int writable)
{
    int result;
    const char *childArgv[] = {	execPath,
                                probeCmd,
                                devName,
                                removable ? DEVICE_REMOVABLE : DEVICE_FIXED,
                                writable? DEVICE_WRITABLE : DEVICE_READONLY,
                                0 };
    char *fsDir = fsDirForFS((char *)fsName);
    int pid;

    dwarning(("%s('%s', '%s', removable=%d, writable=%d):\n'%s %s %s %s %s'\n",
		__FUNCTION__, fsName, devName, removable, writable, execPath, childArgv[1], childArgv[2], childArgv[3], childArgv[4]));

    if (access(execPath,F_OK) == 0)
    {
        if ((pid = fork()) == 0)
        {
			/* CHILD PROCESS */

            cleanUpAfterFork();
            chdir(fsDir);
            execve(execPath, childArgv, 0);
            exit(-127);
        }
        else if (pid > 0)
        {
			int statusp;
			int waitResult;
			
			/* PARENT PROCESS */
			dwarning(("wait4(pid=%d,&statusp,0,NULL)...\n", pid));
			waitResult = wait4(pid,&statusp,0,NULL);
			dwarning(("wait4(pid=%d,&statusp,0,NULL) => %d\n", pid, waitResult));
            if (waitResult > 0)
            {
                if (WIFEXITED(statusp))
                {
                	result = (int)(char)(WEXITSTATUS(statusp));
	                goto Return;
                }
            }
        }
    }

    result = FSUR_IO_FAIL;

Return:
    free(fsDir);
    dwarning(("%s(...) => %d\n", __FUNCTION__, result));
    return result;
}

/* foreignMountDevice: run the -(mount) option of the given <fsName>.util program in a child process */

static int foreignMountDevice(const char *fsName, const char *execPath, const char *devName, int removable, int writable, int suid, int dev, const char *mountPoint)
{
    int result;
    char cmd[] = {'-', FSUC_MOUNT, 0};
    // char *execPath = utilPathForFS((char *)fsName);
    const char *childArgv[] = {	execPath,
                                                        cmd,
                                                        devName,
                                                        mountPoint,
                                                        removable ? DEVICE_REMOVABLE : DEVICE_FIXED,
                                                        writable? DEVICE_WRITABLE : DEVICE_READONLY,
                                                        suid? DEVICE_SUID : DEVICE_NOSUID,
                                                        dev? DEVICE_DEV : DEVICE_NODEV,
                                                        0 };
    char *fsDir = fsDirForFS((char *)fsName);
    int pid;

    dwarning(("%s('%s', '%s', removable=%d, writable=%d, '%s'):\n'%s %s %s %s %s %s %s %s'\n",
                        __FUNCTION__, fsName, devName, removable, writable, mountPoint, execPath, childArgv[1], childArgv[2], childArgv[3], childArgv[4], childArgv[5], childArgv[6], childArgv[7]));

    if ((pid = fork()) == 0)
    {
        /* CHILD PROCESS */

        cleanUpAfterFork();
        chdir(fsDir);
        execve(execPath, childArgv, 0);
        exit(-127);
    }
    else if (pid > 0)
    {
        int statusp;
        int waitResult;

        /* PARENT PROCESS */

        dwarning(("wait4(pid=%d,&statusp,0,NULL)...\n", pid));
        waitResult = wait4(pid,&statusp,0,NULL);
        dwarning(("wait4(pid=%d,&statusp,0,NULL) => %d\n", pid, waitResult));
        if (waitResult > 0)
        {
            if (WIFEXITED(statusp))
            {
                int i = (int)(char)(WEXITSTATUS(statusp));
                if (i == FSUR_IO_SUCCESS)
                {
                        result = FILESYSTEM_MOUNTED;
                        goto Return;
                }
                else if (i == FSUR_IO_UNCLEAN)
                {
                        result = FILESYSTEM_NEEDS_REPAIR;
                        goto Return;
                }
            }
        }
    }

    result = FILESYSTEM_ERROR;

Return:
    free((char *)execPath);
    free(fsDir);
    dwarning(("%s(...) => %d\n", __FUNCTION__, result));
    return result;
}
/** ripped off from workspace - end **/

static int mountVolumeSuid(DiskVolumePtr diskVolume)
{
        if (diskVolume->removable) {
                return FALSE;
        }
        return TRUE;
}

static int mountVolumeDev(DiskVolumePtr diskVolume)
{
        if (diskVolume->removable) {
                return FALSE;
        }
        return TRUE;
}

boolean_t DiskVolume_mount_foreign(DiskVolumePtr diskVolume)
{
    int ret;

        ret = foreignMountDevice(diskVolume->fs_type,diskVolume->util_path, diskVolume->disk_dev_name,diskVolume->removable,diskVolume->writable, mountVolumeSuid(diskVolume), mountVolumeDev(diskVolume), diskVolume->mount_point);
    if (ret == FILESYSTEM_MOUNTED)
    {
        	dwarning(("Mounted %s /dev/%s on %s\n", diskVolume->fs_type, diskVolume->disk_dev_name, diskVolume->mount_point));
                DiskVolume_setMounted(diskVolume,TRUE);
		return (TRUE);
    }
	else if (ret == FILESYSTEM_NEEDS_REPAIR)
	{
		/* We should never get here, thanks to the "fsck -q" calls. */

            char m_command[1024];
            char *fsckCmd = repairPathForFileSystem(diskVolume->fs_type);
            char *rprCmd = repairArgsForFileSystem(diskVolume->fs_type);

            sprintf(m_command, "%s %s /dev/r%s", fsckCmd, rprCmd, diskVolume->disk_dev_name);

            free(fsckCmd);
            free(rprCmd);

            {
                    FILE *		f;
                    int 		ret;

                    dwarning(("%s: command to execute is '%s'\n", __FUNCTION__, m_command));
                    f = popen(m_command, "w");
                    if (f == NULL)
                    {
                            dwarning(("%s: popen('%s') failed", __FUNCTION__, m_command));
                        // Couldn't find an appropriate fsck_* - assume one isn't needed
                        ret = 0;
                    } else {
                        fflush(f);
                        ret = pclose(f);
                        if ( ret >= 0 )
                        {
                                pwarning(("%s: pclose('%s') failed\n", __FUNCTION__, m_command));
                                return (FALSE);
                        }
                    }
            }

            ret = foreignMountDevice(diskVolume->fs_type, diskVolume->util_path, diskVolume->disk_dev_name, diskVolume->removable, diskVolume->writable, mountVolumeSuid(diskVolume), mountVolumeDev(diskVolume), diskVolume->mount_point);
            if (ret == FILESYSTEM_MOUNTED)
            {
                dwarning(("Mounted %s /dev/%s on %s\n", diskVolume->fs_type, diskVolume->disk_dev_name, diskVolume->mount_point));
                    return (TRUE);
            }
            else
            {
                    return (FALSE);
            }

    }
    else if (ret == FILESYSTEM_ERROR)
    {
	    pwarning(("%s: There is a filesystem error with the device %s which was attempting to mount at %s\n", __FUNCTION__, diskVolume->disk_dev_name, diskVolume->mount_point));
    }
    else
    {
            pwarning(("%s: unrecognized return code from foreignMountDevice: %d\n", __FUNCTION__, ret));
    }

    return (FALSE);
}



boolean_t DiskVolume_mount(DiskVolumePtr diskVolume)
{

        // if the device was mounted and the kDiskArbDiskAppearedNoMountMask is set in the flags of the diskPtr
        // then never mount this device ...

        DiskPtr dp 		= LookupDiskByIOBSDName( diskVolume->disk_dev_name );

        if (dp) {
                int isRemovable = ( dp->flags & kDiskArbDiskAppearedEjectableMask ) != 0;

                if ((dp->flags & kDiskArbDiskAppearedNoMountMask) != 0) {
                        return (FALSE);  // say it mounted, but it didn't
                };
                
                if (isRemovable && (currentConsoleUser < 0) && !mountWithoutConsoleUser()) {
                        return (FALSE);  // say it mounted, but it didn't
                } else {
                        // mark the whole disk ptr as being attempted in at least one case
                        DiskPtr wdp = LookupWholeDiskForThisPartition(dp);
                        wdp->mountAttempted = 1;
                }
        }

        

#warning Using mount command to mount UFS here - this needs to change to using ufs.util through pluggable

        if (strcmp(diskVolume->fs_type, FS_TYPE_UFS) == 0)
                return DiskVolume_mount_ufs(diskVolume);
        else
                return DiskVolume_mount_foreign(diskVolume);

        return (FALSE);
}

#define FORMAT_STRING		"%-10s %-6s %-8s %-5s %-5s %-16s %-16s\n"
void DiskVolume_print(DiskVolumePtr diskVolume){
        pwarning((FORMAT_STRING,
               diskVolume->disk_dev_name,
               diskVolume->dev_type,
               diskVolume->fs_type,
               !diskVolume->removable ? "yes" : "no",
               diskVolume->writable ? "yes" : "no",
               diskVolume->disk_name,
               diskVolume->mounted ? diskVolume->mount_point : "[not mounted]" ));
}

void setVar(char **var,char *val)
{
    if (*var)
    {
		free(*var);
    }
    if (val == NULL)
    {
		*var = NULL;
    }
    else 
    {
		*var = strdup(val);
    }

}
void DiskVolume_setFSType(DiskVolumePtr diskVolume,char *t)
{
    setVar(&(diskVolume->fs_type),t);
}
void DiskVolume_setDeviceType(DiskVolumePtr diskVolume,char *t)
{
    setVar(&(diskVolume->dev_type),t);
}
void DiskVolume_setDiskName(DiskVolumePtr diskVolume,char *t)
{
    setVar(&(diskVolume->disk_name),t);
}
void DiskVolume_setUtilPath(DiskVolumePtr diskVolume,char *t)
{
    setVar(&(diskVolume->util_path),t);
}
void DiskVolume_setDiskDevName(DiskVolumePtr diskVolume,char *t)
{
    setVar(&(diskVolume->disk_dev_name),t);
}
void DiskVolume_setMountPoint(DiskVolumePtr diskVolume,char *t)
{
    setVar(&(diskVolume->mount_point),t);
}
void DiskVolume_setMounted(DiskVolumePtr diskVolume,boolean_t val)
{
        if(val) {
                DiskPtr dp 		= LookupDiskByIOBSDName( diskVolume->disk_dev_name );

                if (dp) {
                        dp->mountedFilesystemName = strdup(diskVolume->fs_type);
                }
        }
        diskVolume->mounted = val;
}

void DiskVolume_setWritable(DiskVolumePtr diskVolume,boolean_t val)
{
    diskVolume->writable = val;
}
void DiskVolume_setRemovable(DiskVolumePtr diskVolume,boolean_t val)
{
    diskVolume->removable = val;
}
void DiskVolume_setDirtyFS(DiskVolumePtr diskVolume,boolean_t val)
{
    diskVolume->dirty = val;
}
void DiskVolume_new(DiskVolumePtr *diskVolume)
{
    *diskVolume = malloc(sizeof(DiskVolume));
    (*diskVolume)->fs_type = nil;
    (*diskVolume)->disk_dev_name = nil;
    (*diskVolume)->dev_type = nil;
    (*diskVolume)->disk_name = nil;
    (*diskVolume)->mount_point = nil;
    (*diskVolume)->util_path = nil;
}
void DiskVolume_delete(DiskVolumePtr diskVolume)
{
    int                 i;
    char * *    l[] = { &(diskVolume->fs_type),
                        &(diskVolume->disk_dev_name),
                        &(diskVolume->dev_type),
                        &(diskVolume->disk_name),
                        &(diskVolume->mount_point),
                        //&(diskVolume->util_path),
                        NULL };


    if(!diskVolume)
        free(diskVolume);
        
    for (i = 0; l[i] != NULL; i++)
    {
                if (*(l[i]))
                {
                    free(*(l[i]));
                }
                *(l[i]) = NULL;
    }

    free(diskVolume);
}

extern mach_port_t ioMasterPort;

DiskVolumePtr DiskVolumes_newVolume(DiskVolumesPtr diskList,DiskPtr media,boolean_t isRemovable,boolean_t isWritable,char *type,struct statfs* stat_p,int stat_number)
{
    char *			devname = media->ioBSDName;
    struct statfs *		fs_p;
    char * 			fsname;
    int 			ret;
    char			specName[MAXNAMELEN];
    DiskVolumePtr 		volume = 0x0;
    int 			matchingPointer = 0;

    for (matchingPointer = 0;matchingPointer < CFArrayGetCount(matchingArray);matchingPointer++) {
        
        // see if the diskPtr->service matches any of the filesystem types
        // if it does test that first
        // otherwise, start at the top of the list and test them alls
        int matches;
        CFDictionaryRef dictPointer = CFArrayGetValueAtIndex(matchingArray, matchingPointer);
        CFDictionaryRef mediaProps = CFDictionaryGetValue(dictPointer, CFSTR(kFSMediaPropertiesKey));
        kern_return_t error;
        
        error = IOServiceMatchPropertyTable(media->service, mediaProps, &matches);

        if (error) {
            dwarning(("some kind of error while matching service to array... %d\n", error));
        }
        
        if (matches) {
            CFStringRef utilArgsFromDict;
            CFStringRef fsNameFromDict;
            CFArrayRef fsNameArray;
            CFStringRef utilPathFromDict;

            char *utilPathFromDict2;
            char *utilArgsFromDict2;
            char *fsNameFromDict2;
            char *fstype;
            char *resourcePath;

            char utilPath[MAXPATHLEN];

            dwarning(("********We have a match for devname = %s!!!**********\n", devname));
            
            utilArgsFromDict = CFDictionaryGetValue(dictPointer, CFSTR(kFSProbeArgumentsKey));
            fsNameFromDict = CFDictionaryGetValue(dictPointer, CFSTR("FSName"));
            fsNameArray = CFStringCreateArrayBySeparatingStrings(NULL, fsNameFromDict, CFSTR("."));
            utilPathFromDict = CFDictionaryGetValue(dictPointer, CFSTR(kFSProbeExecutableKey));

            utilPathFromDict2 = daCreateCStringFromCFString(utilPathFromDict);  
            utilArgsFromDict2 = daCreateCStringFromCFString(utilArgsFromDict);
            fsNameFromDict2 = daCreateCStringFromCFString(fsNameFromDict);
            fstype = daCreateCStringFromCFString(CFArrayGetValueAtIndex(fsNameArray, 0));
            resourcePath = resourcePathForFSName(fsNameFromDict2);

            sprintf(utilPath, "%s%s", resourcePath, utilPathFromDict2);

            // clean up
            CFRelease(fsNameArray);
            free(utilPathFromDict2);
            free(fsNameFromDict2);
            free(resourcePath);
            
            sprintf(specName,"/dev/%s",devname);
            ret = foreignProbe(fstype, utilPath, utilArgsFromDict2, devname, isRemovable, isWritable);

            free(utilArgsFromDict2);

            if (ret == FSUR_RECOGNIZED || ret == -9)
            {
                fsname = foreignLabel(fstype);
                if (fsname == NULL) {
                    printf("**************** Label not used ...\n");
                    fsname = fsNameForFSWithMediaName(fstype, media->ioMediaNameOrNull);
                }

                DiskVolume_new(&volume);
                DiskVolume_setDiskDevName(volume,devname);
                DiskVolume_setFSType(volume,fstype);
                DiskVolume_setDiskName(volume,fsname);
                DiskVolume_setDeviceType(volume,type);
                DiskVolume_setWritable(volume,isWritable);
                DiskVolume_setRemovable(volume,isRemovable);
                DiskVolume_setMounted(volume,FALSE);
                DiskVolume_setDirtyFS(volume,FALSE);
                DiskVolume_setUtilPath(volume, utilPath);

                fs_p = fsstat_lookup_spec(stat_p, stat_number, specName, fstype);
                if (fs_p)
                {
                        /* already mounted */
                        DiskVolume_setMounted(volume,TRUE);
                        DiskVolume_setMountPoint(volume,fs_p->f_mntonname);
                }
                else if (isWritable || shouldFsckReadOnlyMedia())
                {
                        DiskVolume_setDirtyFS(volume,fsck_needed(devname,fstype));
                }
                free(fstype);
                free(fsname);
                break;
            } else {
                    free(fstype);
                    dwarning(("Volume is bad\n"));
                    volume = 0x0;
            }
            
        }
    }

    return volume;
}
void DiskVolumes_new(DiskVolumesPtr *diskList)
{
    *diskList = malloc(sizeof(DiskVolumes));
    (*diskList)->list = CFArrayCreateMutable(NULL,0,NULL);
    CFRetain((*diskList)->list);
}
void DiskVolumes_delete(DiskVolumesPtr diskList)
{
    int i;
    int count = CFArrayGetCount(diskList->list);
    if(!diskList)
        return;
    
    for (i = 0; i < count; i++)
    {
            DiskVolume_delete((DiskVolumePtr)CFArrayGetValueAtIndex(diskList->list,i));
    }
    
    CFArrayRemoveAllValues(diskList->list);

    CFRelease(diskList->list);

    free(diskList);
}

static int CanSkipContentType(char * contentType) {

    if (strcmp(contentType, "Apple_partition_map") == 0) {
        return 1;
    } else if (strcmp(contentType, "Apple_Partition_Map") == 0) {
        return 1;
    } else if (strcmp(contentType, "Apple_Driver") == 0) {
        return 1;
    } else if (strcmp(contentType, "Apple_Driver43") == 0) {
        return 1;
    } else if (strcmp(contentType, "Apple_Driver43_CD") == 0) {
        return 1;
    } else if (strcmp(contentType, "Apple_Driver_ATA") == 0) {
        return 1;
    } else if (strcmp(contentType, "Apple_Driver_ATAPI") == 0) {
        return 1;
    } else if (strcmp(contentType, "Apple_Driver_IOKit") == 0) {
        return 1;
    } else if (strcmp(contentType, "Apple_FWDriver") == 0) {
        return 1;
    } else if (strcmp(contentType, "Apple_Patches") == 0) {
        return 1;
    } else if (strcmp(contentType, "Apple_Boot") == 0) {
        return 1;
    } else if (strcmp(contentType, "Apple_Bootstrap") == 0) {
        return 1;
    } else if (strcmp(contentType, "Apple_Loader") == 0) {
        return 1;
    } else if (strcmp(contentType, "Apple_Rhapsody_Loader") == 0) {
        return 1;
    } else if (strcmp(contentType, "Apple_Free") == 0) {
        return 1;
    } else if (strcmp(contentType, "Apple_Void") == 0) {
        return 1;
    } else if (strcmp(contentType, "CD_DA") == 0) {
        return 1;
    } else if (strcmp(contentType, "CD_ROM_Mode_2") == 0) {
        return 1;
    } else if (strcmp(contentType, "CD_ROM_Mode_2_XA_Form_2") == 0) {
        return 1;
    }


    return 0;
}

DiskVolumesPtr DiskVolumes_do_volumes(DiskVolumesPtr diskList)
{
	DiskPtr				diskPtr;
	boolean_t			success = FALSE;
	struct statfs *		stat_p;
	int					stat_number;
	char * 				type = "???"; /* Radar 2323887 */
	int	nfs = 0; /* # filesystems defined in /usr/filesystems */
	struct dirent **fsdirs = NULL;
	int	n; /* iterator for nfs/fsdirs */
    
	stat_p = get_fsstat_list(&stat_number);
	if (stat_p == NULL || stat_number == 0)
	{
		goto Return;
	}

	/* discover known filesystem types */
	nfs = scandir(FS_DIR_LOCATION, &fsdirs, suffixfs, sortfs);
	/*
	 * suffixfs ensured we have only names ending in ".fs"
	 * now we convert the periods to nulls to give us
	 * filesystem type strings.
	 */
	for (n = 0; n < nfs; n++)
	{
		*strrchr(&fsdirs[n]->d_name[0], '.') = '\0';
	}
	if ( gDebug ) {
		dwarning(("%d filesystems known:\n", nfs));
		for (n=0; n<nfs; n++)
		{
			dwarning(("%s\n", &fsdirs[n]->d_name[0]));
		}
	}

	for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = diskPtr->next )
	{
		int isWhole, isWritable, isRemovable;
		DiskVolumePtr volume = 0x0;

		/* Skip non-new disks */

                if ( kDiskStateNew != diskPtr->state )
                {
                        continue;
                }
		
		/* Initialize some convenient flags */
		
		isWhole = IsWhole(diskPtr);
		isWritable = ( diskPtr->flags & kDiskArbDiskAppearedLockedMask ) == 0;
		isRemovable = ( diskPtr->flags & kDiskArbDiskAppearedEjectableMask ) != 0;

                if (diskPtr->mountedUser < 0 && currentConsoleUser >= 0) {
                        diskPtr->mountedUser = currentConsoleUser;
                }

                /* Take appropriate action based on the <ioContent> field */

                /* There are certain IOContent fields that we *know* we can skip over,
                    so let's encode them here till after MacOS X Beta - yeah right :) */
                
                /* if we are chowning /dev nodes, now is the time to do it */

                if (consoleDevicesAreOwnedByMountingUser() && isRemovable && (diskPtr->mountedUser >= 0)) {
                        char devdName[MAXPATHLEN];
                        char devrName[MAXPATHLEN];

                        sprintf(devdName, "/dev/%s", diskPtr->ioBSDName);
                        sprintf(devrName, "/dev/r%s", diskPtr->ioBSDName);

                        dwarning(("Changing devices %s, %s to be owned by user %d\n", devdName, devrName, diskPtr->mountedUser));

                        chown(devdName, diskPtr->mountedUser, 5);	// 5 == operator
                        chown(devrName, diskPtr->mountedUser, 5);	// 5 == operator
                }

                if (CanSkipContentType(diskPtr->ioContent)) {
                    continue;
                }

                if ((diskPtr->flags & kDiskArbDiskAppearedNoSizeMask) != 0) {
                    DiskPtr wholePtr = LookupWholeDiskForThisPartition(diskPtr);
                    wholePtr->mountAttempted = 1;   // so that unrecognized disks will still
                    diskPtr->flags = diskPtr->flags | kDiskArbDiskAppearedUnrecognizableFormat;
                    continue;  // if it's zero length, skip it
                };

                if (IsNetwork(diskPtr)) {
                        continue;
                        // we don't discover network volumes - we are told about them ...
                }

                volume = DiskVolumes_newVolume(diskList,
                                                      diskPtr,
                                                      isRemovable,
                                                      isWritable,
                                                      type,
                                                      stat_p,
                                                      stat_number);

                //printf("val = %d\n", strncmp(FS_RESERVED_PREFIX, diskPtr->ioContent, strlen(FS_RESERVED_PREFIX)));
                //if (!volume && strncmp(FS_RESERVED_PREFIX, diskPtr->ioContent, strlen(FS_RESERVED_PREFIX))) {
                if (!volume) {
                        // file system unrecognized
                        DiskPtr wholePtr = LookupWholeDiskForThisPartition(diskPtr);

                        if (isRemovable && (currentConsoleUser < 0) && !mountWithoutConsoleUser()) {
                                wholePtr->mountAttempted = 0;
                        } else {
                                wholePtr->mountAttempted = 1;   // so that unrecognized disks will still
                                diskPtr->flags = diskPtr->flags | kDiskArbDiskAppearedUnrecognizableFormat;
                        }

                }
		if (volume != nil) {
                    CFArrayAppendValue(diskList->list,volume);
                }

	} /* for */

    success = TRUE;

Return:
	if (fsdirs) {
		for (n = 0; n < nfs; n++) {
			free((void *)fsdirs[n]);
		}
		free((void *)fsdirs);
	}
	if (stat_p)
	{
		free(stat_p);
	}

	if (success)
	{
		return diskList;
	}

        DiskVolumes_delete(diskList);
	return nil;

}
DiskVolumesPtr   DiskVolumes_print(DiskVolumesPtr diskList)
{
    int i;
    printf(	FORMAT_STRING,
    		"DiskDev",
    		"Type",
    		"FileSys",
    		"Fixed",
			"Write",
			"Volume Name",
			"Mounted On" );
    for (i = 0; i < CFArrayGetCount(diskList->list); i++)
    {
            DiskVolume_print((DiskVolumePtr)CFArrayGetValueAtIndex(diskList->list,i));
    }
    return diskList;
}
unsigned 	DiskVolumes_count(DiskVolumesPtr diskList)
{
    return CFArrayGetCount(diskList->list);
}
DiskVolumePtr 	DiskVolumes_objectAtIndex(DiskVolumesPtr diskList,int index)
{
    return (DiskVolumePtr)CFArrayGetValueAtIndex(diskList->list,index);
}

DiskVolumePtr 	DiskVolumes_volumeWithMount(DiskVolumesPtr diskList,char *path)
{
    int i;
    int listSize = CFArrayGetCount(diskList->list);
    
    for (i = 0; i < listSize; i++) {
	DiskVolumePtr d = (DiskVolumePtr)CFArrayGetValueAtIndex(diskList->list,i);
	if (d->mounted && d->mount_point && strcmp(d->mount_point, path) == 0){
	    return (d);
	}
    }
    return (nil);
}

char *mountPath(void)
{
    struct stat 	sb;
    char * mountPath = "/Volumes";

    if (stat(mountPath, &sb) < 0)
    {
        if (errno == ENOENT)
        {
            // create the mount path
            if (mkdir(mountPath, 01777) < 0)
            {
                pwarning(("mountDisks: mkdir(%s) failed, %s\n", mountPath, strerror(errno)));
                    return "/";
            }
        }
        else
        {
            pwarning(("stat(%s) failed, %s\n", mountPath, strerror(errno)));
            return "/";
        }
    }

    // correct permissions on "/Volumes"
    if (chmod(mountPath, 01777) < 0)
    {
            pwarning(("%s: chmod(%s) failed: %s\n", __FUNCTION__, mountPath, strerror(errno)));
    }

    return "/Volumes";
}

mode_t mountModeForDisk(DiskVolumePtr disk)
{
        if (disk->removable && strictRemovableMediaSettings()) {
                return 0700;
        }

        return 0755;
}


boolean_t     	DiskVolumes_setVolumeMountPoint(DiskVolumesPtr diskList,DiskVolumePtr vol)
{
    DiskVolumePtr	disk 	= vol;
    int 		i 	= 1;
    mode_t		mode 	= mountModeForDisk(disk);
    char 		mount_path[MAXLBLLEN + 32];
    char 		possible_mount_path[MAXLBLLEN + 32];
    struct stat 	sb;
    FILE		*fp;
    char		cookieFile[MAXPATHLEN];
    char		*index;

    sprintf(possible_mount_path, "%s", disk->disk_name);

    /* Check and see if the mount_path contains a "/" - if it does remove the "/" and mount the volume at the same name but use a ":" instead if you can */

    index = strchr(possible_mount_path, '/');
    
    while (index != NULL) {
        *index = ':';
        index = strchr(possible_mount_path, '/');
    }

    sprintf(mount_path, "%s/%s", mountPath(), possible_mount_path);
    sprintf(cookieFile, "/%s/%s", mount_path, ADM_COOKIE_FILE);
    
    while (1)
    {
		if (stat(mount_path, &sb) < 0)
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
				pwarning(("stat(%s) failed, %s\n", mount_path, strerror(errno)));
				return (FALSE);
		    }
                }
		else if (DiskVolumes_volumeWithMount(diskList,mount_path))
                {
			/* do nothing */
                }
		else if (rmdir(mount_path) == 0)
		{
                        /* it was an empty directory */
                        break;
                }
                else if (errno == ENOTEMPTY) {
                        // some file exists, see if it's the ADM_COOKIE_FILE and if that is it remove the cookie and retry the rmdir
                        if (stat(cookieFile, &sb) == 0) {
                                if (remove(cookieFile) == 0) {
                                        if (rmdir(mount_path) == 0) {
                                                break;
                                        }
                                }
                        }
                }
                sprintf(mount_path, "%s/%s %d", mountPath() ,disk->disk_name, i);
                sprintf(cookieFile, "/%s/%s", mount_path, ADM_COOKIE_FILE);
		i++;
    }

    if (mkdir(mount_path, mode) < 0)
    {
	pwarning(("mountDisks: mkdir(%s) failed, %s\n", mount_path, strerror(errno)));
	return (FALSE);
    } else {
            /* When you make the path, mark the diskPtr so that the path can get deleted by us later.  That way, we never attempt to delete the mount path of a disk we didn't mount */

            DiskPtr diskPtr 		= LookupDiskByIOBSDName( disk->disk_dev_name );
            if (diskPtr) {
                    diskPtr->admCreatedMountPoint = 1;
            }
    }

    /* Set the mode again, just in case the umask interfered with the mkdir() */

    if (chmod(mount_path, mode) < 0)
    {
        pwarning(("mountDisks: chmod(%s) failed: %s\n", mount_path, strerror(errno)));
    }

    /* add the special cookie in to the directory stating that adm created this dir. */
    fp = fopen(cookieFile, "w");
    fclose(fp);
    
    DiskVolume_setMountPoint(disk,mount_path);
    
    return (TRUE);
}

int HideFile(const char * file)
{
        struct attrlist alist = {0};
        FinderAttrBuf finderInfoBuf = {0};
        int result;

        alist.bitmapcount = ATTR_BIT_MAP_COUNT;
        alist.commonattr = ATTR_CMN_FNDRINFO;

        result = getattrlist(file, &alist, &finderInfoBuf, sizeof(finderInfoBuf), 0);
        if (result) return (errno);

        if (finderInfoBuf.finderInfo.fdFlags & kIsInvisible) {
                dwarning(("hide: %s is alreadly invisible\n", file));
                return (0);
        }

        finderInfoBuf.finderInfo.fdFlags |= kIsInvisible;

        result = setattrlist(file, &alist, &finderInfoBuf.finderInfo, sizeof(FinderInfo), 0);

        return (result == -1 ? errno : result);
}


void DiskVolume_SetTrashes(DiskVolumePtr dptr)
{

    struct stat 	sb;
    char		trashesLocation[1024];

    if (!(dptr->writable)) {
        return;
    }

    sprintf(trashesLocation, "%s/.Trashes", dptr->mount_point);

    /* determine if the trash directory exists
         if it doesn't, set the volume clean */
    if (stat(trashesLocation, &sb) != 0) {
        // the file doesn't exist
        dwarning(("%s: '%s' not found...\n", __FUNCTION__, trashesLocation));
        if (mkdir(trashesLocation, 0333) < 0)
        {
            pwarning(("%s: mkdir(%s) failed, %s\n", __FUNCTION__, trashesLocation, strerror(errno)));
        }
        if (chmod(trashesLocation, 0333) < 0)
        {
            pwarning(("%s: chmod(%s) failed: %s\n", __FUNCTION__, trashesLocation, strerror(errno)));
        }
    } else {
        dwarning(("%s: '%s' found with incorrect permissions, fixing ...\n", __FUNCTION__, trashesLocation));
        // trash exists - are the permissions correct?
        if (chmod(trashesLocation, 0333) < 0)
        {
            pwarning(("%s: chmod(%s) failed: %s\n", __FUNCTION__, trashesLocation, strerror(errno)));
        }
    }

    // Now mark the .trashes folder invisible in the finder
    HideFile(trashesLocation);



    return;
}
