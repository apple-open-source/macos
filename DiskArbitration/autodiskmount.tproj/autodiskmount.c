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
#include <bsd/dev/disk.h>
#include <errno.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <unistd.h>

#include <fts.h>

#include <mach/boolean.h>

#include "DiskArbitrationTypes.h"
#include "DiskVolume.h"
#include "DiskArbitrationServerMain.h"
#include "FSParticular.h"

extern void DiskArbRefresh_rpc(mach_port_t server);

static void
handleSIGHUP(int signal)
{
    dwarning(("Recieved Sighup - Refreshing autodiskmount\n"));
    DiskArbRefresh_rpc(0);
}

void CleanupDirectory( char *dir )
{
        // spin throught trhe root directory and find all directories that contain the file .autodiskmount.
        // You should then remove that file and attempt remove the directory

        FTS *ftsp;
        FTSENT *chp;
        const char *rootDir[] = {dir,0};
        char cookieFile[MAXPATHLEN];
        char path[MAXPATHLEN];
        struct stat 	sb;



        ftsp = fts_open(rootDir, FTS_PHYSICAL, NULL);
        fts_read(ftsp);

        chp = fts_children(ftsp, 0);
        if (chp) {
            do {
                    sprintf(cookieFile, "%s/%s/%s", dir, chp->fts_accpath, ADM_COOKIE_FILE);
                    sprintf(path, "%s/%s", dir, chp->fts_accpath);

                    if (chp->fts_info & FTS_D) {
                            if (stat(cookieFile, &sb) == 0) {
                                    // remove the cookieFile and the directory
                                    if (remove(cookieFile) == 0) {
                                           if (rmdir(path) == 0) {
                                                    dwarning(("*** The directory %s was cleaned up and removed by autodiskmount prior to mounting ***\n", chp->fts_name));
                                                    continue;
                                           }
                                    }
                            }
                    }
            } while((chp = chp->fts_link));
        }
        
        fts_close(ftsp);
}


boolean_t
fsck_vols(DiskVolumesPtr vols) 
{
    boolean_t		result = TRUE; /* mandatory initialization */
    char 		command[1024];
    char		fsck_command_line[1024];
    int 		i;
    struct stat 	sb;


    for (i = 0; i < DiskVolumes_count(vols); i++)
    {

	DiskVolumePtr vol;

	vol = (DiskVolumePtr)DiskVolumes_objectAtIndex(vols,i);

	if (vol->writable && vol->dirty && vol->mount_point == NULL)
	{
	    /* Construct a command line to fsck/fsck_hfs this dirty volume. */
            char *fsckCmd = repairPathForFileSystem(vol->fs_type);
            char *rprCmd = repairArgsForFileSystem(vol->fs_type);

            sprintf(command, "%s %s /dev/r%s", fsckCmd, rprCmd,vol->disk_dev_name);
            sprintf(fsck_command_line, fsckCmd);



            {
                /* determine if the fsck command line exists
                 if it doesn't, set the volume clean */
                if (stat(fsck_command_line, &sb) != 0) {
                    // the file doesn't exist
                    vol->dirty = FALSE;
                    dwarning(("%s: '%s' not found...\n", __FUNCTION__, fsck_command_line));
                    continue;
                }
            }

	    /* Execute the command-line we constructed */
	    
	    {
		FILE *		f;
		int 		ret;
	
		dwarning(("%s: '%s'...\n", __FUNCTION__, command));

		f = popen(command, "w");
		if (f == NULL)
		{
			dwarning(("popen('%s') failed", command));
                        /* we assume that if no fsck_<fs type> exists, then
                        the volume must be clean so it will mount */
                        vol->dirty = FALSE;

			continue;
		}
		fflush(f);
		ret = pclose(f);

		dwarning(("%s: '%s' => %d\n", __FUNCTION__, command, ret));

		if ( ret <= 0 )
		{
			/* Mark the volume as clean so that it will be mounted */
			vol->dirty = FALSE;
		}
		else
		{
			dwarning(("'%s' failed: %d\n", command, ret));
		}

		/* Result will be TRUE iff each fsck command is successful */

		result = result && (ret == 0);
	    }
            free(fsckCmd);
            free(rprCmd);

	} // if dirty

    } // for each volume

    return result;

}

boolean_t 
mount_vols(DiskVolumesPtr vols, int takeOwnership)
{
    int i;

    for (i = 0; i < DiskVolumes_count(vols); i++)
    {
            DiskVolumePtr d = (DiskVolumePtr)DiskVolumes_objectAtIndex(vols,i);
            DiskPtr dp = LookupDiskByIOBSDName( d->disk_dev_name );

            /* If already mounted then skip this volume */

            if (d->mounted)
            {
                    if (dp)
                    {
                            DiskSetMountpoint(dp, d->mount_point);
                            if (takeOwnership) {

                                    if (dp->mountedUser != -1) {
                                        // some is actually logged in, chown the disk to that user
                                        dwarning(("*** Someone should own the disks now %d, %s ***\n", dp->mountedUser, d->mount_point));
                                        chown(d->mount_point, dp->mountedUser, 99);
                                    }
                            }
                            DiskVolume_SetTrashes(d);


                    }
                    continue;
            }

            /* If still dirty then skip this volume */

            if (d->dirty)
            {
                    continue;
            }

            /* Determine and create the mount point */

            if ( DiskVolumes_setVolumeMountPoint(vols,d) == FALSE)
            {
                    continue;
            }

            if (DiskVolume_mount(d))
            {
                    if (dp)
                    {
                            DiskSetMountpoint( dp, d->mount_point );
                            if (takeOwnership) {

                                    if (dp->mountedUser != -1) {
                                        // some is actually logged in, chown the disk to that user
                                        dwarning(("*** Someone should own the disks now %d, %s ***\n", dp->mountedUser, d->mount_point));
                                        chown(d->mount_point, dp->mountedUser, 99);
                                    }
                             }

                            DiskVolume_SetTrashes(d);
                    }
            }
    }

    return (TRUE);
}

void autodiskmount(int takeOwnership)
{
    DiskVolumesPtr vols;
    DiskPtr diskPtr;

    DiskVolumes_new(&vols);

    // clean up pre-existing mountpoint directories that have a .autodiskmounted file in them in the root

    CleanupDirectory("/");
    CleanupDirectory(mountPath());

    DiskVolumes_do_removable(vols,g.do_removable,g.eject_removable);

    if (g.do_mount)
    {
	if (fsck_vols(vols) == FALSE)
	{
		pwarning(("Some fsck failed!\n"));
	}

	if (mount_vols(vols, takeOwnership) == FALSE)
	{
		pwarning(("Some mount() failed!\n"));
	}


            for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = diskPtr->next )
            {
                    // if any of the diskPartitions contain a mounted volume
                    // then mark the whole disk as having a mount

                    if (diskPtr->mountpoint != NULL && strlen(diskPtr->mountpoint)) {
                            DiskPtr wholePtr = LookupWholeDiskForThisPartition(diskPtr);

                            if (wholePtr) {
                                    wholePtr->flags = wholePtr->flags | kDiskArbDiskAppearedRecognizableSectionMounted;
                            }
                    }

            }
            for (diskPtr = g.Disks; diskPtr != NULL; diskPtr = diskPtr->next )
            {
                    // go through them all again ...
                    // check the new whole disks for a new disk that does not contain a recognizable section

                    int isWhole = ( diskPtr->flags & kDiskArbDiskAppearedWholeDiskMask );
                    int dialogPreviouslyDisplayed = ( diskPtr->flags & kDiskArbDiskAppearedDialogDisplayed );
                    int neverMount = ( diskPtr->flags & kDiskArbDiskAppearedNoMountMask );

                    if (isWhole && !dialogPreviouslyDisplayed && !neverMount) {
                            int recognizablePartitionsAppeared = ( diskPtr->flags & kDiskArbDiskAppearedRecognizableSectionMounted );

                            if (!recognizablePartitionsAppeared) {
                                    // display the dialog
                                    if (DiskArbIsHandlingUnrecognizedDisks()) {
                                            StartUnrecognizedDiskDialogThread(diskPtr);
                                            diskPtr->flags |= kDiskArbDiskAppearedDialogDisplayed;
                                    }
                            }
                    }
            }


            
    }

    if (g.verbose)
    {
        DiskVolumes_print(vols);
    }

    DiskVolumes_delete(vols);
}


GlobalStruct g;

int
main(int argc, char * argv[])
{
    char * progname;
    char ch;

    /* Initialize globals */

    g.Clients = NULL;
    g.NumClients = 0;
	
    g.Disks = NULL;
    g.NumDisks = 0;
    g.NumDisksAddedOrDeleted = 1;
	
    g.eject_removable = FALSE;
    g.do_removable = FALSE;
    g.verbose = FALSE;
    g.do_mount = TRUE;
    g.debug = FALSE;
	
    /* Initialize <progname> */
	
    progname = argv[0];

    /* Must run as root */

    if (getuid() != 0)
    {
	pwarning(("%s: must be run as root\n", progname));
	exit(1);
    }

    signal(SIGHUP, handleSIGHUP);

    /* Parse command-line arguments */

    while ((ch = getopt(argc, argv, "avned")) != -1)
    {
	switch (ch)
	{
	    case 'a':
		g.do_removable = TRUE;
	    break;
	    case 'v':
		g.verbose = TRUE;
	    break;
	    case 'n':
		g.do_mount = FALSE;
	    break;
	    case 'e':
		g.eject_removable = TRUE;
	    break;
	    case 'd':
		g.debug = TRUE;
	    break;
	}
    }

    if (g.eject_removable)
    {
	g.do_removable = FALSE; /* sorry, eject overrides use */
    }

    {	/* Radar 2323271 */
    	char * argv2[] = { argv[0], "-d" };
    	int argc2 = ( g.debug ? 2 : 1 );
    	(void) DiskArbitrationServerMain(argc2, argv2);
    }

    /* Not reached */

    exit(0);
}

