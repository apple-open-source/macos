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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <sys/disk.h>

#include <hfs/hfs_mount.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "fsck_hfs.h"
#include "dfalib/CheckHFS.h"

#define CACHE_IOSIZE	32768
#define CACHE_BLOCKS	128
#define CACHE_HASHSIZE	111

/* Global Variables for front end */
const char *cdevname;		/* name of device being checked */
char	*progname;
char	nflag;			/* assume a no response */
char	yflag;			/* assume a yes response */
char	preen;			/* just fix normal inconsistencies */
char	force;			/* force fsck even if clean (preen only) */
char	quick;			/* quick check returns clean, dirty, or failure */
char	debug;			/* output debugging info */
char	hotroot;		/* checking root device */
char 	guiControl; 	/* this app should output info for gui control */
char	rebuildCatalogBtree;  /* rebuild catalog btree file */
char	modeSetting;	/* set the mode when creating "lost+found" directory */
int		upgrading;		/* upgrading format */
int		lostAndFoundMode = 0; /* octal mode used when creating "lost+found" directory */

int	fsmodified;		/* 1 => write done to file system */
int	fsreadfd;		/* file descriptor for reading file system */
int	fswritefd;		/* file descriptor for writing file system */
Cache_t fscache;



static int checkfilesys __P((char * filesys));
static int setup __P(( char *dev, int *blockDevice_fdPtr, int *canWritePtr ));
static void usage __P((void));
static void getWriteAccess __P(( char *dev, int *blockDevice_fdPtr, int *canWritePtr ));

int
main(argc, argv)
 int	argc;
 char	*argv[];
{
	int ch;
	int ret;
	extern int optind;
	extern char *optarg;

	if (progname = strrchr(*argv, '/'))
		++progname;
	else
		progname = *argv;

	while ((ch = getopt(argc, argv, "dfgm:npqruy")) != EOF) {
		switch (ch) {
		case 'd':
			debug++;
			break;

		case 'f':
			force++;
			break;

		case 'g':
			guiControl++;
			break;
			
		case 'm':
			modeSetting++;
			lostAndFoundMode = strtol( optarg, NULL, 8 );
			if ( lostAndFoundMode == 0 )
			{
				(void) fprintf(stderr, "%s: invalid mode argument \n", progname);
				usage();
			}
			break;
			
		case 'n':
			nflag++;
			yflag = 0;
			break;

		case 'p':
			preen++;
			break;

		case 'q':
		 	quick++;
			break;

		case 'r':
			rebuildCatalogBtree++;  // this will force a rebuild of catalog file
			break;

		case 'y':
			yflag++;
			nflag = 0;
			break;

		case 'u':
		case '?':
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;
	
	if (guiControl)
		debug = 0; /* debugging is for command line only */

	if (signal(SIGINT, SIG_IGN) != SIG_IGN)
		(void)signal(SIGINT, catch);

	if (argc < 1) {
		(void) fprintf(stderr, "%s: missing special-device\n", progname);
		usage();
	}

	ret = 0;
	while (argc-- > 0)
		ret |= checkfilesys(blockcheck(*argv++));

	exit(ret);
}

static int
checkfilesys(char * filesys)
{
	int flags;
	int result;
	int chkLev, repLev, logLev;
	int	blockDevice_fd, canWrite;

	flags = 0;
	cdevname = filesys;
	blockDevice_fd = -1;
	canWrite = 0;

	if (debug && preen)
		pwarn("starting\n");
	
	if (setup( filesys, &blockDevice_fd, &canWrite ) == 0) {
		if (preen)
			pfatal("CAN'T CHECK FILE SYSTEM.");
		result = 0;
		goto ExitThisRoutine;
	}

	if (preen == 0) {
		if (hotroot && !guiControl)
			printf("** Root file system\n");
	}

	/* start with defaults for dfa back-end */
	chkLev = kAlwaysCheck;
	repLev = kMajorRepairs;
	logLev = kVerboseLog;

	if (yflag)	
		repLev = kMajorRepairs;

	if (quick) {
		chkLev = kNeverCheck;
		repLev = kNeverRepair;
		logLev = kFatalLog;
	} else if (force) {
		chkLev = kForceCheck;
	}
	if (preen) {
		repLev = kMinorRepairs;
		chkLev = force ? kAlwaysCheck : kDirtyCheck;
		logLev = kFatalLog;
	}
	if (debug)
		logLev = kDebugLog;

	if (nflag)
		repLev = kNeverRepair;
		
	if ( rebuildCatalogBtree ) {
		chkLev = kPartialCheck;
		repLev = kForceRepairs;  // this will force rebuild of catalog B-Tree file
	}
		
	/*
	 * go check HFS volume...
	 */
	result = CheckHFS(	fsreadfd, fswritefd, chkLev, repLev, logLev, 
						guiControl, lostAndFoundMode, canWrite, &fsmodified );
	if (!hotroot) {
		ckfini(1);
		if (quick) {
			if (result == 0) {
				pwarn("QUICKCHECK ONLY; FILESYSTEM CLEAN\n");
				result = 0;
				goto ExitThisRoutine;
			} else if (result == R_Dirty) {
				pwarn("QUICKCHECK ONLY; FILESYSTEM DIRTY\n");
				result = DIRTYEXIT;
				goto ExitThisRoutine;
			} else if (result == R_BadSig) {
				pwarn("QUICKCHECK ONLY; NO HFS SIGNATURE FOUND\n");
				result = DIRTYEXIT;
				goto ExitThisRoutine;
			} else {
				result = EEXIT;
				goto ExitThisRoutine;
			}
		}
	} else {
		struct statfs stfs_buf;
		/*
		 * Check to see if root is mounted read-write.
		 */
		if (statfs("/", &stfs_buf) == 0)
			flags = stfs_buf.f_flags;
		else
			flags = 0;
		ckfini(flags & MNT_RDONLY);
	}

	/* XXX free any allocated memory here */

	if (!fsmodified) {
		result = (result == 0) ? 0 : EEXIT;
		goto ExitThisRoutine;
	}
	if (hotroot) {
		struct hfs_mount_args args;
		/*
		 * We modified the root.  Do a mount update on
		 * it, unless it is read-write, so we can continue.
		 */
		if (!preen)
			printf("\n***** FILE SYSTEM WAS MODIFIED *****\n");
		if (flags & MNT_RDONLY) {		
			bzero(&args, sizeof(args));
			flags |= MNT_UPDATE | MNT_RELOAD;
			if (mount("hfs", "/", flags, &args) == 0) {
				result = 0;
				goto ExitThisRoutine;
			}
		}
		if (!preen)
			printf("\n***** REBOOT NOW *****\n");
		sync();
		result = FIXEDROOTEXIT;
		goto ExitThisRoutine;
	}

	result = 0;
	
ExitThisRoutine:
	if ( blockDevice_fd > 0 ) {
		close( blockDevice_fd );
		blockDevice_fd = -1;
	}
	return (result);
}


/*
 * Setup for I/O to device
 * Return 1 if successful, 0 if unsuccessful.
 * blockDevice_fd - returns fd for block device or -1 if we can't get write access.
 * canWrite - 1 if we can safely write to the raw device or 0 if not.
 */
static int
setup( char *dev, int *blockDevice_fdPtr, int *canWritePtr )
{
	struct stat statb;
	int devBlockSize;

	fswritefd = -1;
	*blockDevice_fdPtr = -1;
	*canWritePtr = 0;
	
	if (stat(dev, &statb) < 0) {
		printf("Can't stat %s: %s\n", dev, strerror(errno));
		return (0);
	}
	if ((statb.st_mode & S_IFMT) != S_IFCHR) {
		pfatal("%s is not a character device", dev);
		if (reply("CONTINUE") == 0)
			return (0);
	}
	if ((fsreadfd = open(dev, O_RDONLY)) < 0) {
		printf("Can't open %s: %s\n", dev, strerror(errno));
		return (0);
	}

	/* attempt to get write access to the block device and if not check if volume is */
	/* mounted read-only.  */
	getWriteAccess( dev, blockDevice_fdPtr, canWritePtr );
	
	if (preen == 0 && !guiControl)
		printf("** %s", dev);
	if (nflag || (fswritefd = open(dev, O_WRONLY)) < 0) {
		fswritefd = -1;
		if (preen)
			pfatal("NO WRITE ACCESS");
		if (!guiControl)
			printf(" (NO WRITE)");
	}
	if (preen == 0 && !guiControl)
		printf("\n");

	/* Initialize the cache */
	ioctl(fsreadfd, DKIOCGETBLOCKSIZE, &devBlockSize);
	if (CacheInit (&fscache, fsreadfd, fswritefd, devBlockSize,
			CACHE_IOSIZE, CACHE_BLOCKS, CACHE_HASHSIZE) != EOK) {
		pfatal("Can't initialize disk cache\n");
		return (0);
	}	

	return (1);
}


// This routine will attempt to open the block device with write access for the target 
// volume in order to block others from mounting the volume with write access while we
// check / repair it.  If we cannot get write access then we check to see if the volume
// has been mounted read-only.  If it is read-only then we should be OK to write to 
// the raw device.  Note that this does not protect use from someone upgrading the mount
// from read-only to read-write.

static void getWriteAccess( char *dev, int *blockDevice_fdPtr, int *canWritePtr )
{
	int					i;
	int					myMountsCount;
	void *				myPtr;
	char *				myCharPtr;
	struct statfs *		myBufPtr;
	void *				myNamePtr;

	myPtr = NULL;
	myNamePtr = malloc( strlen(dev) + 2 );
	if ( myNamePtr == NULL ) 
		return;
		
	strcpy( (char *)myNamePtr, dev );
	if ( (myCharPtr = strrchr( (char *)myNamePtr, '/' )) != 0 ) {
		if ( myCharPtr[1] == 'r' ) {
			strcpy( &myCharPtr[1], &myCharPtr[2] );
			*blockDevice_fdPtr = open( (char *)myNamePtr, O_WRONLY );
		}
	}
	
	if ( *blockDevice_fdPtr > 0 ) {
		// we got write access to the block device so we can safely write to raw device
		*canWritePtr = 1;
		goto ExitThisRoutine;
	}
	
	// get count of mounts then get the info for each 
	myMountsCount = getfsstat( NULL, 0, MNT_NOWAIT );
	if ( myMountsCount < 0 )
		goto ExitThisRoutine;

	myPtr = (void *) malloc( sizeof(struct statfs) * myMountsCount );
	if ( myPtr == NULL ) 
		goto ExitThisRoutine;
	myMountsCount = getfsstat( 	myPtr, 
								(sizeof(struct statfs) * myMountsCount), 
								MNT_NOWAIT );
	if ( myMountsCount < 0 )
		goto ExitThisRoutine;

	myBufPtr = (struct statfs *) myPtr;
	for ( i = 0; i < myMountsCount; i++ )
	{
		if ( strcmp( myBufPtr->f_mntfromname, myNamePtr ) == 0 ) {
			if ( myBufPtr->f_flags & MNT_RDONLY )
				*canWritePtr = 1;
			goto ExitThisRoutine;
		}
		myBufPtr++;
	}
	*canWritePtr = 1;  // single user will get us here, f_mntfromname is not /dev/diskXXXX 
	
ExitThisRoutine:
	if ( myPtr != NULL )
		free( myPtr );
		
	if ( myNamePtr != NULL )
		free( myNamePtr );
	
	return;
	
} /* getWriteAccess */


static void
usage()
{
	(void) fprintf(stderr, "usage: %s [-df m [mode] npqruy] special-device\n", progname);
	(void) fprintf(stderr, "  d = output debugging info\n");
	(void) fprintf(stderr, "  f = force fsck even if clean (preen only) \n");
	(void) fprintf(stderr, "  m arg = octal mode used when creating lost+found directory \n");
	(void) fprintf(stderr, "  n = assume a no response \n");
	(void) fprintf(stderr, "  p = just fix normal inconsistencies \n");
	(void) fprintf(stderr, "  q = quick check returns clean, dirty, or failure \n");
	(void) fprintf(stderr, "  r = rebuild catalog btree \n");
	(void) fprintf(stderr, "  u = usage \n");
	(void) fprintf(stderr, "  y = assume a yes response \n");
	
	exit(1);
}
