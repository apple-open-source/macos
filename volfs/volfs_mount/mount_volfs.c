/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
/*
 * Copyright (c) 1997 Apple Computer, Inc. All Rights Reserved
 *
 */

#include <sys/param.h>
#include <sys/mount.h>

#include <err.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "mount_volfs.h"

#define PROCESS_OPTIONS 0
#if PROCESS_OPTIONS
#include "mntopts.h"

struct mntopt mopts[] = {
	MOPT_STDOPTS,
	MOPT_UPDATE,
	{ NULL }
};
#endif

#define DEFAULT_ROOTUID	-2
#define DEFAULT_ANON_UID -2

#if DEBUG
#define DEBUG_MSGS 1
#else
#define DEBUG_MSGS 0
#endif

void	usage __P((void));

int
main(argc, argv)
	int argc;
	char **argv;
{
    struct volfs_args args;
    int ch, mntflags, opts;
    char *dir;
    int mountStatus, i;

    mntflags = opts = 0;
    mntflags |= MNT_RDONLY; /* *PWD* For now, just assume all disks are read-only... */

#if DEBUG_MSGS
    printf("mount_volfs: calling mount with  %d auguments\n", argc);
    for (i=0;i<argc; i++) {
        printf ("mount_volfs: augumnt %d is : %s\n", i, argv[i]);
	};
#endif

    while ((ch = getopt(argc, argv, "o")) != EOF)
        switch (ch) {
#if PROCESS_OPTIONS
            case 'o':
                getmntopts(optarg, mopts, &mntflags);
                break;
#endif
            case '?':
            default:
                usage();
                }
            argc -= optind;
    argv += optind;

    if (argc != 1)
        usage();

    dir = argv[0];

    bzero(&args, sizeof(args));	
    args.export.ex_root = DEFAULT_ROOTUID;
    args.export.ex_anon.cr_uid = DEFAULT_ANON_UID;		/* mapping for anonymous users */
    if (mntflags & MNT_RDONLY)
        args.export.ex_flags = MNT_EXRDONLY;
    else
        args.export.ex_flags = 0;
    args.flags = opts;

#if DEBUG_MSGS
    printf("mount_volfs: calling mount with  mount point = %s...\n", dir);
#endif
    if ((mountStatus = mount("volfs", dir, mntflags, &args)) < 0) {
#if DEBUG_MSGS
        printf("mount_volfs: error on mount(): error = %d.\n", mountStatus);
#endif
        err(1, NULL);
    };

    exit(0);
}

void
usage()
{
	(void)fprintf(stderr,
               "usage: mount_volfs [-o options] mount-point\n");
	exit(1);
}
