/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Portions Copyright (c) 1999 Apple Computer, Inc.  All Rights
 * Reserved.  This file contains Original Code and/or Modifications of
 * Original Code as defined in and that are subject to the Apple Public
 * Source License Version 1.1 (the "License").  You may not use this file
 * except in compliance with the License.  Please obtain a copy of the
 * License at http://www.apple.com/publicsource and read it before using
 * this file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON- INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved
 *
 * Copyright (c) 1980, 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * The NEXTSTEP Software License Agreement specifies the terms
 * and conditions for redistribution.
 *
 *	@(#)fstab.c	8.1 (Berkeley) 6/4/93
 */


#include <errno.h>
#include <fstab.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/mount.h>

static FILE *_fs_fp = NULL;
static struct fstab _fs_fstab;
static struct fstab _root_fstab;
static char _root_fstype[MFSNAMELEN];
static int firstTime = 1;
static int returnedRoot = 0;
static void error __P((int));
static int fstabscan __P((void));
static const char *slash = "/";
static const char *remountedroot = "/root";

/* We don't want to depend on fstab for the root filesystem entry,
** since that can change when disks are added or removed from the system.
** So, we stat "/" and find the _PATH_DEV entry that matches. The devname()
** function can be used as a shortcut if _PATH_DEVDB exists. This returns a
** string like "sd1a", so we need to prepend _PATH_DEV to it.
*/

static char *getDevPath(dev_t target_dev) {
    static char *dev = NULL;
    char *name;

    if (dev == NULL) {
	dev = malloc(MAXPATHLEN);
	if (dev == NULL)
	    return NULL;
    }

    strcpy(dev, _PATH_DEV);

    /* The root device in fstab should always be a block special device */
    name = devname(target_dev, S_IFBLK);
    if (name == NULL) {
        /* No _PATH_DEVDB. We have to search for it the slow way */
        DIR *dirp;
        struct dirent *ent;
        dirp = opendir(_PATH_DEV);
        if (dirp == NULL) {
            perror("opendir");
            return NULL;
        }
        while ((ent = readdir(dirp)) != NULL) {
            /* Look for a block special device */
            if (ent->d_type == DT_BLK) {
                struct stat devst;
                strcat(dev, ent->d_name);
                if (stat(dev, &devst) >= 0) {
                    if (devst.st_rdev == target_dev) {
                        return dev;
                    }
                }
            }
            /* set dev to _PATH_DEV and try again */
            dev[sizeof(_PATH_DEV) - 1] = '\0';
        }
    } else {
        /* We found the _PATH_DEVDB entry */
        strcat(dev, name);
        return dev;
    }
    return NULL;
}

static int initrootentry(struct fstab *rootentry)
{
     char *rootpath = (char *)slash;
     struct stat rootstat;
     struct statfs rootfsinfo;
     
     if (stat(rootpath, &rootstat) < 0) {
        perror("stat");
        return -1;
     };
     if (statfs(rootpath, &rootfsinfo) < 0) {
     	perror("statfs");
     	return -1;
     };
     
     /* Check to make sure we're not looking at a synthetic root: */
     if (strcmp(rootfsinfo.f_fstypename, "synthfs") == 0) {
     	rootpath = (char *)remountedroot;
        if (stat(rootpath, &rootstat) < 0) {
           perror("stat");
           return -1;
        };
        if (statfs(rootpath, &rootfsinfo) < 0) {
           perror("statfs");
           return -1;
        };
     };
     
     /* Copy the type name before returning a pointer a pointer to it */
     strncpy(_root_fstype, rootfsinfo.f_fstypename, MFSNAMELEN);
     
     rootentry->fs_spec = getDevPath(rootstat.st_dev);
     rootentry->fs_file = rootpath;
     rootentry->fs_vfstype = _root_fstype;
     rootentry->fs_mntops = FSTAB_RW;
     rootentry->fs_type = FSTAB_RW;
     rootentry->fs_freq = 0;
     rootentry->fs_passno = 1;
     
     return 0;
}

static int fstabscan()
{
	register char *cp;
#define	MAXLINELENGTH	1024
	static char *line = NULL;
	char subline[MAXLINELENGTH];
	int typexx;

        if (!returnedRoot) {
            returnedRoot = 1;
            if (firstTime) {
                firstTime = 0;
                if (initrootentry(&_root_fstab) != 0) {
                	return 0;
                };
            }
            _fs_fstab = _root_fstab;
            return 1;
        }
	if (!_fs_fp) {
		return(0);
	}

	if (line == NULL) {
		line = malloc(MAXLINELENGTH);
		if (line == NULL)
			return 0;
	}

        for (;;) {
		if (!(cp = fgets(line, MAXLINELENGTH, _fs_fp)))
			return(0);
/* OLD_STYLE_FSTAB */
		if (!strpbrk(cp, " \t")) {
			_fs_fstab.fs_spec = strtok(cp, ":\n");
#if defined(__APPLE__)
			if (!_fs_fstab.fs_spec || *_fs_fstab.fs_spec == '#')
				continue;
#endif
			_fs_fstab.fs_file = strtok((char *)NULL, ":\n");
			/* Only list the root filesystem once */
			if (!_fs_fstab.fs_file || !(strcmp(_fs_fstab.fs_file, "/"))) {
				continue;
			}
			_fs_fstab.fs_type = strtok((char *)NULL, ":\n");
			if (_fs_fstab.fs_type) {
				if (!strcmp(_fs_fstab.fs_type, FSTAB_XX))
					continue;
				_fs_fstab.fs_mntops = _fs_fstab.fs_type;
				_fs_fstab.fs_vfstype =
				    strcmp(_fs_fstab.fs_type, FSTAB_SW) ?
				    "ufs" : "swap";
				if ((cp = strtok((char *)NULL, ":\n"))) {
					_fs_fstab.fs_freq = atoi(cp);
					if ((cp = strtok((char *)NULL, ":\n"))) {
						_fs_fstab.fs_passno = atoi(cp);
						return(1);
					}
				}
			}
			goto bad;
		}
/* OLD_STYLE_FSTAB */
		_fs_fstab.fs_spec = strtok(cp, " \t\n");
		if (!_fs_fstab.fs_spec || *_fs_fstab.fs_spec == '#')
			continue;
		_fs_fstab.fs_file = strtok((char *)NULL, " \t\n");
                /* Only list the root filesystem once */
                if (!(strcmp(_fs_fstab.fs_file, "/"))) {
                     continue;
                }
		_fs_fstab.fs_vfstype = strtok((char *)NULL, " \t\n");
		_fs_fstab.fs_mntops = strtok((char *)NULL, " \t\n");
		if (_fs_fstab.fs_mntops == NULL)
			goto bad;
		_fs_fstab.fs_freq = 0;
		_fs_fstab.fs_passno = 0;
		if ((cp = strtok((char *)NULL, " \t\n")) != NULL) {
			_fs_fstab.fs_freq = atoi(cp);
			if ((cp = strtok((char *)NULL, " \t\n")) != NULL)
				_fs_fstab.fs_passno = atoi(cp);
		}
		strcpy(subline, _fs_fstab.fs_mntops);
		for (typexx = 0, cp = strtok(subline, ","); cp;
		     cp = strtok((char *)NULL, ",")) {
			if (strlen(cp) != 2)
				continue;
			if (!strcmp(cp, FSTAB_RW)) {
				_fs_fstab.fs_type = FSTAB_RW;
				break;
			}
			if (!strcmp(cp, FSTAB_RQ)) {
				_fs_fstab.fs_type = FSTAB_RQ;
				break;
			}
			if (!strcmp(cp, FSTAB_RO)) {
				_fs_fstab.fs_type = FSTAB_RO;
				break;
			}
			if (!strcmp(cp, FSTAB_SW)) {
				_fs_fstab.fs_type = FSTAB_SW;
				break;
			}
			if (!strcmp(cp, FSTAB_XX)) {
				_fs_fstab.fs_type = FSTAB_XX;
				typexx++;
				break;
			}
		}
		if (typexx)
			continue;
		if (cp != NULL)
			return(1);

bad:		/* no way to distinguish between EOF and syntax error */
		error(EFTYPE);
	}
	/* NOTREACHED */
}

struct fstab *
getfsent()
{
	if (!returnedRoot) {
		setfsent();
	}
	if (!fstabscan()) {
		return((struct fstab *)NULL);
	}
	return(&_fs_fstab);
}

struct fstab *
getfsspec(name)
	register const char *name;
{
	if (setfsent())
		while (fstabscan())
			if (!strcmp(_fs_fstab.fs_spec, name))
				return(&_fs_fstab);
	return((struct fstab *)NULL);
}

struct fstab *
getfsfile(name)
	register const char *name;
{
	if (setfsent())
		while (fstabscan())
			if (!strcmp(_fs_fstab.fs_file, name))
				return(&_fs_fstab);
	return((struct fstab *)NULL);
}

int
setfsent()
{
        returnedRoot = 0;
	if (_fs_fp) {
		rewind(_fs_fp);
	} else {
		_fs_fp = fopen(_PATH_FSTAB, "r");
	}
	return(1);
}

void
endfsent()
{
	returnedRoot = 0;
	if (_fs_fp) {
		(void)fclose(_fs_fp);
		_fs_fp = NULL;
	}
}

static void error(err)
	int err;
{
	char *p;

	(void)write(STDERR_FILENO, "fstab: ", 7);
	(void)write(STDERR_FILENO, _PATH_FSTAB, sizeof(_PATH_FSTAB) - 1);
	(void)write(STDERR_FILENO, ": ", 1);
	p = strerror(err);
	(void)write(STDERR_FILENO, p, strlen(p));
	(void)write(STDERR_FILENO, "\n", 1);
}
