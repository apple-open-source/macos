/*
 * Copyright (c) 1999 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/termios.h>

#include <fcntl.h>
#include <dirent.h>
#include <db.h>
#include <string.h>
#include <paths.h>
#include <stdlib.h>

static char *buf = NULL;
static char *oldttyname __P((int, struct stat *));

char *
ttyname(fd)
	int fd;
{
	struct stat sb;
	struct termios term;
	DB *db;
	DBT data, key;
	struct {
		mode_t type;
		dev_t dev;
	} bkey;

	if( buf == NULL ) {
		buf = malloc(sizeof(_PATH_DEV) + MAXNAMLEN);
		if( buf == NULL )
			return NULL;
		strcpy(buf, _PATH_DEV);
	}

	/* Must be a terminal. */
	if (ioctl(fd, TIOCGETA, &term) < 0)
		return (NULL);
	/* Must be a character device. */
	if (fstat(fd, &sb) || !S_ISCHR(sb.st_mode))
		return (NULL);

	db = dbopen(_PATH_DEVDB, O_RDONLY, 0, DB_HASH, NULL);
	if (db) {
		memset(&bkey, 0, sizeof(bkey));
		bkey.type = S_IFCHR;
		bkey.dev = sb.st_rdev;
		key.data = &bkey;
		key.size = sizeof(bkey);
		if (!(db->get)(db, &key, &data, 0)) {
			bcopy(data.data,
			    buf + sizeof(_PATH_DEV) - 1, data.size);
			(void)(db->close)(db);
			return (buf);
		}
		(void)(db->close)(db);
	}
	return (oldttyname(fd, &sb));
}

static char *
oldttyname(fd, sb)
	int fd;
	struct stat *sb;
{
	register struct dirent *dirp;
	register DIR *dp;
	struct stat dsb;

	if( buf == NULL ) {
		buf = malloc(sizeof(_PATH_DEV) + MAXNAMLEN);
		if( buf == NULL )
			return NULL;
		strcpy(buf, _PATH_DEV);
	}
	if ((dp = opendir(_PATH_DEV)) == NULL)
		return (NULL);

	while ( (dirp = readdir(dp)) ) {
		if (dirp->d_fileno != sb->st_ino)
			continue;
		bcopy(dirp->d_name, buf + sizeof(_PATH_DEV) - 1,
		    dirp->d_namlen + 1);
		if (stat(buf, &dsb) || sb->st_dev != dsb.st_dev ||
		    sb->st_ino != dsb.st_ino)
			continue;
		(void)closedir(dp);
		return (buf);
	}
	(void)closedir(dp);
	return (NULL);
}
