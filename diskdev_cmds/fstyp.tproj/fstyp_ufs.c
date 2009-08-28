/*
 * Copyright (c) 2005 Apple Computer, Inc. All rights reserved.
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

#include <sys/stat.h>
#include <sys/param.h>
#include <sys/time.h>

#include <ufs/ufs/dinode.h>
#include <ufs/ffs/fs.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fstab.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define E_OPENDEV -1
#define E_READ -5

void usage(void);
char *rawname(char *name);
char *unrawname(char *name);
int checkVolHdr(const struct fs *volhdr);
char *blockcheck(char *origname);

char *progname;

/*
 * prefer to use raw device. TODO: suppose block device is valid but
 * the corresponding raw device is not valid, then we fail. this is
 * probably no the desired behavior.
 */

int
main(int argc, char **argv)
{
	union {
		struct fs       fs;
		char            pad[SBSIZE];
	} volhdr;

	char *devname = NULL;
	int fd, retval;

	retval = 0;
	fd = -1;

	if ((progname = strrchr(*argv, '/')))
		++progname;
	else
		progname = *argv;

	if (argc != 2) {
		usage();
	} else {
		devname = blockcheck(argv[1]);
		if (devname != NULL) {
			if ((fd = open(devname, O_RDONLY, 0)) < 0) {
				retval = E_OPENDEV;
			} else if (pread(fd, &(volhdr.fs), SBSIZE, (off_t) SBOFF) != SBSIZE) {
				retval = E_READ;
			} else {
				retval = checkVolHdr(&(volhdr.fs));
			}

			if (-1 != fd) {
				close(fd);
				fd = -1;
			}
		}
	}

	return retval;
}

void
usage(void)
{
	fprintf(stdout, "usage: %s device\n", progname);
	return;
}

/* copied from diskdev_cmds/fsck_hfs/utilities.c */
char *
rawname(char *name)
{
	static char     rawbuf[32];
	char           *dp;

	if ((dp = strrchr(name, '/')) == 0)
		return (0);
	*dp = 0;
	(void) strcpy(rawbuf, name);
	*dp = '/';
	(void) strcat(rawbuf, "/r");
	(void) strcat(rawbuf, &dp[1]);

	return (rawbuf);
}

/* copied from diskdev_cmds/fsck_hfs/utilities.c */
char *
unrawname(char *name)
{
	char           *dp;
	struct stat     stb;

	if ((dp = strrchr(name, '/')) == 0)
		return (name);
	if (stat(name, &stb) < 0)
		return (name);
	if ((stb.st_mode & S_IFMT) != S_IFCHR)
		return (name);
	if (dp[1] != 'r')
		return (name);
	(void) strcpy(&dp[1], &dp[2]);

	return (name);
}

/*
 * copied from diskdev_cmds/fsck_hfs/utilities.c, and modified:
 * 1) remove "hotroot"
 * 2) if error, return NULL
 * 3) if not a char device, return NULL (effectively, this is treated
 *    as error even if accessing the block device might have been OK)
 */
char *
blockcheck(char *origname)
{
	struct stat     stblock, stchar;
	char           *newname, *raw;
	int             retried = 0;

	newname = origname;
retry:
	if (stat(newname, &stblock) < 0) {
		perror(newname);
		fprintf(stderr, "Can't stat %s\n", newname);
		return NULL;
	}
	if ((stblock.st_mode & S_IFMT) == S_IFBLK) {
		raw = rawname(newname);
		if (stat(raw, &stchar) < 0) {
			perror(raw);
			fprintf(stderr, "Can't stat %s\n", raw);
			return NULL;
		}
		if ((stchar.st_mode & S_IFMT) == S_IFCHR) {
			return (raw);
		} else {
			fprintf(stderr, "%s is not a character device\n", raw);
			return NULL;
		}
	} else if ((stblock.st_mode & S_IFMT) == S_IFCHR && !retried) {
		newname = unrawname(newname);
		retried++;
		goto retry;
	}
	/* not a block or character device */
	return NULL;
}


/*
 * simply checks the magic number as dumpfs does.
 *
 * return 1 if volhdr is a UFS volhdr, 0 otherwise
 */
int
checkVolHdr(const struct fs * volhdr)
{
	int             retval = 0;

	retval = (volhdr->fs_magic == FS_MAGIC);

	return retval;
}
