/*
 * Copyright (c) 2006, 2007 Apple Inc. All rights reserved.
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

#include <mach/mach.h>
#include <mach/mach_error.h>
#include <servers/bootstrap.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <membership.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <uuid/uuid.h>
#include <string.h>
#include <libkern/OSByteOrder.h>

#include "dirhelper.h"
#include "dirhelper_priv.h"

#define BUCKETLEN	2

#define MUTEX_LOCK(x)	if(__is_threaded) pthread_mutex_lock(x)
#define MUTEX_UNLOCK(x)	if(__is_threaded) pthread_mutex_unlock(x)

#define ENCODEBITS	6
#define ENCODEDSIZE	((8 * UUID_UID_SIZE + ENCODEBITS - 1) / ENCODEBITS)
#define UUID_UID_SIZE	(sizeof(uuid_t) + sizeof(uid_t))

extern int __is_threaded;

static const mode_t modes[] = {
    0,		/* unused */
    0700,	/* temp */
    0700,	/* cache */
};

static const char *subdirs[] = {
    DIRHELPER_TOP_STR,
    DIRHELPER_TEMP_STR,
    DIRHELPER_CACHE_STR,
};

static const char encode[] = "+-0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

static void
encode_uuid_uid(uuid_t uuid, uid_t uid, char *str)
{
    unsigned char buf[UUID_UID_SIZE + 1];
    unsigned char *bp = buf;
    int i = 0;
    unsigned int n;

    memcpy(bp, uuid, sizeof(uuid_t));
    uid = OSSwapHostToBigInt32(uid);
    memcpy(bp + sizeof(uuid_t), &uid, sizeof(uid_t));
    bp[UUID_UID_SIZE] = 0; // this ensures the last encoded byte will have trailing zeros
    while(i < ENCODEDSIZE) {
	switch(i % 4) {
	case 0:
	    n = *bp++;
	    *str++ = encode[n >> 2];
	    break;
	case 1:
	    n = ((n & 0x3) << 8) | *bp++;
	    *str++ = encode[n >> 4];
	    break;
	case 2:
	    n = ((n & 0xf) << 8) | *bp++;
	    *str++ = encode[n >> 6];
	    break;
	case 3:
	    *str++ = encode[n & 0x3f];
	    break;
	}
	i++;
    }
    *str = 0;
}

char *
__user_local_dirname(uid_t uid, dirhelper_which_t which, char *path, size_t pathlen)
{
    uuid_t uuid;
    char str[ENCODEDSIZE + 1];
    int res;

    if(which < 0 || which > DIRHELPER_USER_LOCAL_LAST) {
	errno = EINVAL;
	return NULL;
    }

    res = mbr_uid_to_uuid(uid, uuid);
    if(res != 0) {
        errno = res;
        return NULL;
    }
    
    //
    // We partition the namespace so that we don't end up with too
    // many users in a single directory.  With 4096 buckets, we
    // could scale to 1,000,000 users while keeping the average
    // number of files in a single directory below 250
    //
    encode_uuid_uid(uuid, uid, str);
    res = snprintf(path, pathlen,
	"%s%.*s/%s/%s",
	VAR_FOLDERS_PATH, BUCKETLEN, str, str, subdirs[which]);
    if(res >= pathlen) {
	errno = EINVAL;
	return NULL; /* buffer too small */
    }
    return path;
}

__private_extern__ char *
_dirhelper(dirhelper_which_t which, char *path, size_t pathlen)
{
    static char userdir[PATH_MAX];
    static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
    int res;
    struct stat sb;

    if(which < 0 || which > DIRHELPER_USER_LOCAL_LAST) {
	errno = EINVAL;
	return NULL;
    }

    if(!*userdir) {
	MUTEX_LOCK(&lock);
	if(!*userdir) {
	    mach_port_t mp;

	    if(bootstrap_look_up(bootstrap_port, DIRHELPER_BOOTSTRAP_NAME, &mp) != KERN_SUCCESS) {
		errno = EPERM;
		MUTEX_UNLOCK(&lock);
		return NULL;
	    }
	    if(__user_local_dirname(geteuid(), DIRHELPER_USER_LOCAL, userdir, sizeof(userdir)) == NULL) {
server_error:
		mach_port_deallocate(mach_task_self(), mp);
		MUTEX_UNLOCK(&lock);
		return NULL;
	    }
	    /*
	     * check if userdir exists, and if not, call
	     * __dirhelper_create_user_local to create it
	     * (we have to check again afterwards).
	     */
	    if(stat(userdir, &sb) < 0) {
		if(errno != ENOENT) { /* some unknown error */
		    *userdir = 0;
		    goto server_error;
		}
		if(__dirhelper_create_user_local(mp) != KERN_SUCCESS) {
		    errno = EPERM;
		    *userdir = 0;
		    goto server_error;
		}
		/* double check that the directory really got created */
		if(stat(userdir, &sb) < 0) {
		    *userdir = 0;
		    goto server_error;
		}
	    }
	    mach_port_deallocate(mach_task_self(), mp);
	}
	MUTEX_UNLOCK(&lock);
    }
    
    if(pathlen < strlen(userdir) + strlen(subdirs[which]) + 1) {
	errno = EINVAL;
	return NULL; /* buffer too small */
    }
    strcpy(path, userdir);
    strcat(path, subdirs[which]);

    /*
     * now for subdirectories, create it with the appropriate permissions
     * if it doesn't already exist.
     */
    if(which != DIRHELPER_USER_LOCAL) {
	res = mkdir(path, modes[which]);
	if(res != 0 && errno != EEXIST)
	    return NULL;
    }

    return path;
}
