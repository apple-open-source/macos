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

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <fstab.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/disk.h>

#define E_OPENDEV -1
#define E_READ -5

/*
 * We don't have a (non-C++) standard header for UDF (yet?), so
 * let's define the basic structures and constants we'll be using.
 */

typedef struct UDFVolumeSequenceDescriptor {
	unsigned char	type;
	unsigned char	id[5];
	unsigned char	version;
	unsigned char	data[2041];
} udfVSD;

#define UDFSTART	(32*1024)	/* First 32k is unused */

void usage(void);
char *rawname(char *name);
char *unrawname(char *name);
int CheckUDF(int, int);
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
			} else {
				int bsize;
				if (ioctl(fd, DKIOCGETBLOCKSIZE, (char*)&bsize) == -1) {
#ifdef DEBUG
					warn("DKIOCGETBLOCKSIZE ioctl failed for %s", devname);
#endif
					bsize = 2048;	/* A standard default size */
				}
				retval = CheckUDF(fd, bsize) == 1;
				if (retval == 0 && bsize != 2048) {
					retval = CheckUDF(fd, 2048) == 1;
				}
			}
		}
	}

	return retval;
}

static int
IsVSD(udfVSD *vsd) {
	int retval = memcmp(vsd->id, "BEA01", 5)==0 ||
		memcmp(vsd->id, "BOOT2", 5)==0 ||
		memcmp(vsd->id, "NSR02", 5)==0 ||
		memcmp(vsd->id, "NSR03", 5)==0 ||
		memcmp(vsd->id, "TEA01", 5)==0 ||
		memcmp(vsd->id, "CDW02", 5)==0 ||
		memcmp(vsd->id, "CD001", 5)==0;
#ifdef DEBUG
	fprintf(stderr, "IsVSD:  Returning %d\n", retval);
#endif
	return retval;
}

/*
 * This is inspired by the udf25 kext code.
 * It concludes that a device has a UDF filesystem
 * if:
 * 1)  It has a Volume Sequence Descriptor;
 * 2)  That VSD has a "BEA01" in it;
 * 3)  That VSD has an "NSR02" or "NSR03" in it before the terminating one.
 *
 * It may be necessary to check the AVDP(s), as well.
 */

int
CheckUDF(int fd, int blockSize) {
	ssize_t err;
	char buf[blockSize];
	off_t curr, max;
	char found = 0;

	curr = UDFSTART;
	max = curr + (512 * blockSize);
	if (lseek(fd, curr, SEEK_SET) == -1) {
		warn("Cannot seek to %llu", curr);
		return -1;
	}

	while (curr < max) {
		udfVSD *vsd;
		err = read(fd, buf, sizeof(buf));
		if (err != sizeof(buf)) {
			if (err == -1) {
				warn("Cannot read %zu bytes", sizeof(buf));
			} else {
				warn("Cannot read %zd bytes, only read %zd", sizeof(buf), err);
			}
			return -1;
		}
		vsd = (udfVSD*)buf;
		if (!IsVSD(vsd)) {
			break;
		}
		if (vsd->type == 0 &&
			memcmp(vsd->id, "BEA01", 5) == 0 &&
			vsd->version == 1) {
			found = 1;
			break;
		}
		curr += blockSize;
	}
	if (found == 0)
		return 0;

	found = 0;

	while (curr < max) {
		udfVSD *vsd;
		err = read(fd, buf, sizeof(buf));
		if (err != sizeof(buf)) {
			if (err == -1) {
				warn("Cannot read %zu bytes", sizeof(buf));
			} else {
				warn("Cannot read %zu bytes, only read %zd", sizeof(buf), err);
			}
			return -1;
		}
		vsd = (udfVSD*)buf;
		if (!IsVSD(vsd)) {
			break;
		}
		if (vsd->type == 0 &&
			memcmp(vsd->id, "TEA01", 5) == 0 &&
			vsd->version == 1) {
			/* we're at the end */
			break;
		} else if (memcmp(vsd->id, "NSR02", 5) == 0 ||
				memcmp(vsd->id, "NSR03", 5) == 0) {
			found = 1;
			break;
		}
		curr += blockSize;
	}

	return found;
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
#ifdef DEBUG
	else if ((stblock.st_mode & S_IFMT) == S_IFREG) {
		return strdup(origname);
	}
#endif
	/* not a block or character device */
	return NULL;
}


