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
 * autodiskmount.c
 * - fscks and mounts all currently unmounted ufs filesystems, and mounts any
 *   hfs or cd9660 filesystems
 * - by default, only filesystems on fixed disks are mounted
 * - option '-a' means mount removable media too
 * - option '-e' means eject removable media (overrides '-a')
 * - option '-n' means don't mount anything, can be combined
 *   with other options to eject all removable media without mounting anything
 * - option '-v' prints out what's already mounted (after doing mounts)
 * - option '-d' prints out debugging information
 */

/*
 * Modification History:
 *
 * Dieter Siegmund (dieter@apple.com) Thu Aug 20 18:31:29 PDT 1998
 * - initial revision
 * Dieter Siegmund (dieter@apple.com) Thu Oct  1 13:42:34 PDT 1998
 * - added support for hfs and cd9660 filesystems
 * Brent Knight (bknight@apple.com) Thu Apr  1 15:54:48 PST 1999
 * - adapted for Beaker
 * Brent Knight (bknight@apple.com) Thu Sep  9 11:36:01 PDT 1999
 * - added support for fsck_hfs, disk insertion and ejection
 * Dieter Siegmund (dieter@apple.com) Wed Nov 10 10:58:43 PST 1999
 * - added support for named UFS volumes
 */

#include <libc.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <dev/disk.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <unistd.h>

#include <SystemConfiguration/SystemConfiguration.h>

#include <fts.h>

#include <mach/boolean.h>

#include "DiskArbitrationTypes.h"
#include "DiskVolume.h"
#include "Configuration.h"
#include "DiskArbitrationServerMain.h"
#include "FSParticular.h"
#include "FSTableLookup.h"

extern void     DiskArbRefresh_rpc(mach_port_t server);

static void
handleSIGHUP(int signal)
{
	dwarning(("Recieved Sighup - Refreshing autodiskmount\n"));
	DiskArbRefresh_rpc(0);
}

int
GidForConsoleUser()
{
	int             gid = 20;

	int             uid = 0;
	int             realGid = 0;

	CFStringRef     name = SCDynamicStoreCopyConsoleUser(NULL, &uid, &realGid);

	if (name) {
		gid = realGid;
		CFRelease(name);
	}
	return gid;
}

int
UidForConsoleUser()
{
	int             uid = -1;
	int             gid = 0;

	CFStringRef     name = SCDynamicStoreCopyConsoleUser(NULL, &uid, &gid);

	if (name) {
		CFRelease(name);
	} else {
		uid = -1;
	}

	return uid;
}

void
CleanupDirectory(char *dir)
{
	//spin throught trhe root directory and find all directories that contain the file.autodiskmount.
	// You should then remove that file and attempt remove the directory

	FTS * ftsp;
	FTSENT         *chp;
	const char     *rootDir[] = {dir, 0};
	char            cookieFile[MAXPATHLEN];
	char            path[MAXPATHLEN];
	struct stat     sb;



	ftsp = fts_open((char * const *)rootDir, FTS_PHYSICAL, NULL);
	fts_read(ftsp);

	chp = fts_children(ftsp, 0);
	if (chp) {
		do {
			sprintf(cookieFile, "%s/%s/%s", dir, chp->fts_accpath, ADM_COOKIE_FILE);
			sprintf(path, "%s/%s", dir, chp->fts_accpath);

			if (chp->fts_info & FTS_D) {
				if (stat(cookieFile, &sb) == 0) {
					//remove the cookieFile and the directory
						if (remove(cookieFile) == 0) {
						if (rmdir(path) == 0) {
							dwarning(("*** The directory %s was cleaned up and removed by autodiskmount prior to mounting ***\n", chp->fts_name));
							continue;
						}
					}
				}
			}
		} while ((chp = chp->fts_link));
	}
	fts_close(ftsp);
}

/*
 * Function: string_to_argv
 * Purpose:
 *   The given string "str" looks like a set of command-line arguments, space-separated e.g.
 *   "-v -d -s", or, "-y", or "".  Turn that into an array of string pointers using the given
 *   "argv" array to store up to "n" of them.
 *
 *   The given string is modified, as each space is replaced by a nul.
 */
int
string_to_argv(char * str, char * * argv, int n)
{
	int 	count;
	char *	scan;
    
	if (str == NULL)
		return (0);

	for (count = 0, scan = str; count < n; ) {
		char * 	space;

		argv[count++] = scan;
		space = strchr(scan, ' ');
		if (space == NULL)
			break;
		*space = '\0';
		scan = space + 1;
	}
	return (count);
}

/*
 * We only want to trigger the quotacheck command
 * on a volume when we fsck it and mark it clean.
 * The quotacheck must be done post mount.
 */
boolean_t
fsck_vols(DiskVolumesPtr vols)
{
	boolean_t       result = TRUE;	/* mandatory initialization */
	int             i;

	for (i = 0; i < DiskVolumes_count(vols); i++) {

		DiskVolumePtr   vol = (DiskVolumePtr) DiskVolumes_objectAtIndex(vols, i);
		if (!vol) {
			return FALSE;
		}

		if ((vol->writable || shouldFsckReadOnlyMedia()) && vol->dirty && vol->mount_point == NULL) {
#define NUM_ARGV	6
			const char * 	argv[NUM_ARGV] = {
				NULL, /* fsck */
				NULL, /* -y */
				NULL, /* /dev/rdisk0s8 */
				NULL, /* termination */
				NULL, /* 2 extra args in case someone wants to pass */
				NULL  /* extra args beyond -y */
			};
			int 		argc;
			DiskPtr         dp = LookupDiskByIOBSDName(vol->disk_dev_name);
			char           *fsckCmd = repairPathForFileSystem(vol->fs_type);
			char           *rprCmd = repairArgsForFileSystem(vol->fs_type);
			char 		devpath[64];
			int 		ret;

			snprintf(devpath, sizeof(devpath), "/dev/r%s", vol->disk_dev_name);
			argv[0] = fsckCmd;
			argc = string_to_argv(rprCmd, (char * *)argv + 1, NUM_ARGV - 3);
			argv[1 + argc] = devpath;

			dwarning(("sending checking messages\n"));
			SendDiskWillBeCheckedMessages(dp);

			if (do_exec(NULL, argv, &ret, NULL) == FALSE) {
				/* failed to get a result, assume the volume is clean */
				dwarning(("*** vol dirty? ***\n"));
				vol->dirty = FALSE;
				vol->run_quotacheck = TRUE;
				ret = 0;
			}
			else if (ret == 0) {
				/* Mark the volume as clean so that it will be mounted */
				vol->dirty = FALSE;
				vol->run_quotacheck = TRUE;
			}
			else {
				dwarning(("'%s' failed: %d\n", fsckCmd, ret));
				StartUnmountableDiskThread(dp);
				dp->flags |= kDiskArbDiskAppearedCheckFailed;
			}

			/*
			 * Result will be TRUE iff each fsck command
			 * is successful
			 */
			dwarning(("*** result? ***\n"));
			result = result && (ret == 0);

			dwarning(("*** freeing? ***\n"));

			free(fsckCmd);
			free(rprCmd);

		} //if dirty
    } //for each
    return result;

}

boolean_t
quotacheck_vols(DiskVolumesPtr vols)
{
	boolean_t       result = TRUE;	/* mandatory initialization */
	int             i;

	for (i = 0; i < DiskVolumes_count(vols); i++) {

		DiskVolumePtr   vol = (DiskVolumePtr) DiskVolumes_objectAtIndex(vols, i);
		if (!vol) {
			return FALSE;
		}
		
		if (vol->run_quotacheck == FALSE)
		        continue;

		/* always turn off the flag */
		vol->run_quotacheck = FALSE;

		if (vol->mounted && vol->mount_point && vol->writable) {
			const char * 	argv[] = {
				NULL, /* quotacheck */
				NULL, /* -g */
				NULL, /* -u */
				NULL, /* filesystem mount point */
				NULL  /* termination */
			};
			int 		ret;

			argv[0] = "/sbin/quotacheck";
			argv[1] = "-g";
			argv[2] = "-u";
			argv[3] = vol->mount_point;

			dwarning(("check quotas on %s\n", vol->mount_point));

			if (do_exec(NULL, argv, &ret, NULL) == FALSE) {
				/* failed to get a result, assume the volume is checked*/
				dwarning(("*** vol quotas checked? ***\n"));
				ret = 0;
			}

			/*
			 * Result will be TRUE iff each quotacheck command
			 * is successful
			 */
			dwarning(("*** quota check result? ***\n"));
			result = result && (ret == 0);

		} //if checking quotas
    } //for each
    return result;

}


boolean_t
mount_vols(DiskVolumesPtr vols, int takeOwnership) {
    boolean_t       fstabIsFresh = FALSE;
    int             i;

    dwarning(("*** mount_vols ***\n"));

    for (i = 0; i < DiskVolumes_count(vols); i++) {
        DiskVolumePtr   d = (DiskVolumePtr) DiskVolumes_objectAtIndex(vols, i);
        DiskPtr         dp = LookupDiskByIOBSDName(d->disk_dev_name);

        /* If already mounted then skip this volume */

        if (d->mounted) {
            if (dp) {
                DiskSetMountpoint(dp, d->mount_point);

                if (takeOwnership || dp->forceTakeOwnershipOnDisk) {
                    if ((dp->forceTakeOwnershipOnDisk || dp->mountedUser == -1) && currentConsoleUser != -1) {
                        int             gid = GidForConsoleUser();

                        dp->mountedUser = currentConsoleUser;
                        //some is actually logged in, chown the disk to that user
                            dwarning(("*** Someone should own the disks now %d, %s ***\n", dp->mountedUser, d->mount_point));
                        chown(d->mount_point, dp->mountedUser, gid);
                        dp->forceTakeOwnershipOnDisk = 0;
                    }
                }
                DiskVolume_SetTrashes(d);


            }
            continue;
        }
        /* If still dirty then skip this volume */

        if (d->dirty) {
            continue;
        }
        /*
         * if the volume has not been approved, let s mark it
         * for approval and move on... we will have to spin
         * back through all the disks and try again once
         * everyone has responded(or not)
         */
        if (dp->state == kDiskStateNew && !dp->approvedForMounting) {
            SetStateForOnePartition(dp, kDiskStateWaitingForMountApproval);
            PrepareToSendPreMountMsgsForDisk(dp);
            continue;
        }
        /* Refresh the fstab */

        if (fstabIsFresh == FALSE) {
            FSTableLookup_readFstab();
            fstabIsFresh = TRUE;
        }
        /* Determine and create the mount point */

        if (DiskVolumes_setVolumeMountPoint(vols, d) == FALSE) {
            continue;
        }
        if (d->mount_point == NULL || strlen(d->mount_point) == 0) {
            continue;
        }
        
        if (DiskVolume_mount(d)) {
            if (dp) {
                DiskSetMountpoint(dp, d->mount_point);
                if (takeOwnership || dp->forceTakeOwnershipOnDisk) {
                    if ((dp->forceTakeOwnershipOnDisk || dp->mountedUser == -1) && currentConsoleUser != -1) {
                        int             gid = GidForConsoleUser();
                        dp->mountedUser = currentConsoleUser;
                        //some is actually logged in, chown the disk to that user
                            dwarning(("*** Someone should own the disks now %d, %s ***\n", dp->mountedUser, d->mount_point));
                        chown(d->mount_point, dp->mountedUser, gid);
                        dp->forceTakeOwnershipOnDisk = 0;
                    }
                }
                DiskVolume_SetTrashes(d);
            }
        }
    }
    return (TRUE);
}

void DiskArbDiskAppearedRecognizableSectionMountedApplier(DiskPtr wholePtr) {
    wholePtr->flags = wholePtr->flags | kDiskArbDiskAppearedRecognizableSectionMounted;
}

void 
autodiskmount(int takeOwnership) 
{
	DiskVolumesPtr  vols;
	DiskPtr         diskPtr;
	
	DiskVolumes_new(&vols);
	
	//clean up pre - existing mountpoint directories that have a.autodiskmounted file in them in the root
	
	CleanupDirectory(mountPath());
	
	DiskVolumes_do_volumes(vols);
	
	if (fsck_vols(vols) == FALSE) {
		pwarning(("Some fsck failed!\n"));
	}
	if (mount_vols(vols, takeOwnership) == FALSE) {
		pwarning(("Some mount() failed!\n"));
	}
	if (quotacheck_vols(vols) == FALSE) {
		pwarning(("Some quotacheck failed!\n"));
	}
	for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = diskPtr->next) {
		/*
		 * if any of the diskPartitions contain a mounted
		 * volume then mark the whole disk as having at least
		 * one mount */
		
		if ((diskPtr->mountpoint != NULL &&
		    strlen(diskPtr->mountpoint)) || diskPtr->state == kDiskStateWaitingForMountApproval) {
			LookupWholeDisksForThisPartition(diskPtr->service, DiskArbDiskAppearedRecognizableSectionMountedApplier);
		}
	}
	if (g.verbose) {
		DiskVolumes_print(vols);
	}
	
	DiskVolumes_delete(vols);
}

boolean_t
autodiskmount_findDisk(boolean_t all, const char * volume_name)
{
	boolean_t	   	found = TRUE;
	DiskVolumesPtr 	 	vols;
	
	DiskVolumes_new(&vols);
	DiskVolumes_do_volumes(vols);

	if (g.verbose) {
		DiskVolumes_print(vols);
	}
	else {
		(void)fsck_vols(vols);
		found = DiskVolumes_findDisk(vols, all, volume_name);
	}
	DiskVolumes_delete(vols);
	return (found);
}

GlobalStruct g;

void setReadOnlyStatus()
{
    struct statfs *mntbuf;
    int numMounts;
    int index;
    
    if ((numMounts = getmntinfo(&mntbuf, MNT_NOWAIT)) == 0) {
        dwarning(("getmntinfo() call failed!\n"));
    }
    for (index=0; index < numMounts; index++) {
        if (strcmp(mntbuf[index].f_mntonname, "/") != 0) {
                continue;
        } else {
            g.readOnlyBoot = (mntbuf[index].f_flags & MNT_RDONLY);

            if (g.readOnlyBoot) {
                pwarning(("autodiskmount running on read only file system\n"));
            }
        }
    }

}


int
main(int argc, char * argv[])
{
	const char * 	volume_name = NULL;
	char * progname;
	char ch;
	boolean_t	find = FALSE;
	boolean_t	all = FALSE;
	
	/* Initialize globals */
	
	g.Clients = NULL;
	g.NumClients = 0;
	
	g.Disks = NULL;
	g.NumDisks = 0;
	g.NumDisksAddedOrDeleted = 1;
	
	g.verbose = FALSE;
	g.debug = FALSE;
	
	/* Initialize <progname> */
	
	progname = argv[0];
	
	/* Must run as root */
	if (getuid() != 0) {
		pwarning(("%s: must be run as root\n", progname));
		exit(1);
	}
	
	/* Parse command-line arguments */
	while ((ch = getopt(argc, argv, "avdFV:")) != -1)	{
		switch (ch) {
		case 'a':
			all = TRUE;
			break;
		case 'v':
			g.verbose = TRUE;
			break;
		case 'd':
			g.debug = TRUE;
			break;
		case 'F':
			find = TRUE;
			break;
		case 'V':
			volume_name = optarg;
			break;
		}
	}
	
	if (find == TRUE) {
		extern int findDiskInit();
		
		if (findDiskInit() < 0) {
			exit(2);
		}
		if (autodiskmount_findDisk(all, volume_name) == FALSE) {
			exit(1);
		}
		exit(0);
	}

	signal(SIGHUP, handleSIGHUP);

    // if we are booted off of a read only media set a flag

    setReadOnlyStatus();
	
	{	/* Radar 2323271 */
		char           *argv2[] = {argv[0], "-d"};
		int             argc2 = (g.debug ? 2 : 1);
		(void) DiskArbitrationServerMain(argc2, argv2);
	}
	
	/* Not reached */
	
	exit(0);
}

