/*
 * Copyright (c) 2003, 2004 Apple Computer, Inc. All rights reserved.
 *
 * Created by Patrick Dirks on Mon Mar 17 2003.
 *
 * $Id: vfs_sysctl.c,v 1.3 2004/10/07 20:25:27 lindak Exp $
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "vfs_sysctl.h"

#define FSARRAY_SIZE_MARGIN 10

static pthread_mutex_t fsid_list_mutex = PTHREAD_MUTEX_INITIALIZER;
static size_t fsid_array_size = 0;
static fsid_t *fsid_array = NULL;

// static pthread_mutex_t statfs_list_mutex = PTHREAD_MUTEX_INITIALIZER;
static size_t statfs_array_size = 0;
static struct statfs *statfs_array = NULL;
static size_t statfs_array_entries;
static bool statfs_array_valid = false;

int
vfsevent_init(void)
{
	int error, kq;
	struct kevent ki;
	struct timespec to;

	bzero(&to, sizeof(to));
	kq = kqueue();
	if (kq == -1)
		err(1, "kqueue");
	EV_SET(&ki, 0, EVFILT_FS, EV_ADD, 0, 0, 0);
again:
	error = kevent(kq, &ki, 1, NULL, 0, &to);
	if (error == -1) {
		if (errno == EINTR)
			goto again;
		err(1, "kevent(EVFILT_FS)");
	}
	return (kq);
}

u_int
vfsevent_wait(int kq, int timeout)
{
	int error;
	struct timespec to, *tp;
	struct kevent ko;

	if (timeout) {
		tp = &to;
		to.tv_sec = timeout;
		to.tv_nsec = 0;
		//dbg("waiting (up to %d seconds) for fs event...\n", timeout);
	} else {
		tp = NULL;
		//dbg("waiting (forever) for fs event...\n");
	}
again:
	error = kevent(kq, NULL, 0, &ko, 1, tp);
	if (error == -1) {
		if (errno == EINTR)
			goto again;
		err(1, "kevent(EVFILT_FS)");
	}
	return (error == 0 ? 0 : ko.fflags);
}

int
sysctl_fsid(op, fsid, oldp, oldlenp, newp, newlen)
	int op;
	fsid_t *fsid;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
{
	int ctlname[CTL_MAXNAME+2];
	size_t ctllen;
	const char *sysstr = "vfs.generic.ctlbyfsid";
	struct vfsidctl vc;

	ctllen = CTL_MAXNAME+2;
	if (sysctlnametomib(sysstr, ctlname, &ctllen) == -1) return -1;
	ctlname[ctllen] = op;

	bzero(&vc, sizeof(vc));
	vc.vc_vers = VFS_CTL_VERS1;
	vc.vc_fsid = *fsid;
	vc.vc_ptr = newp;
	vc.vc_len = newlen;
	return (sysctl(ctlname, ctllen + 1, oldp, oldlenp, &vc, sizeof(vc)));
}


static int
resize_fsid_array(size_t newentrycount)
{
	if (fsid_array) {
		fsid_array = realloc(fsid_array, sizeof(fsid_t) * newentrycount);
	} else {
		fsid_array = malloc(sizeof(fsid_t) * newentrycount);
	};
	if (fsid_array == NULL) return ENOMEM;
	fsid_array_size = newentrycount;
	
	return 0;
}

size_t
get_vfslist(fsid_t **fsidp)
{
	int result;
	size_t fs_count, array_size;
	const char *sysstr = "vfs.generic.vfsidlist";
	
sysctl_loop:
	if (fsid_array_size == 0) {
		result = -1;				/* Fall into error handling loop, below */
	} else {
		array_size = fsid_array_size * sizeof(fsid_t);
		result = sysctlbyname(sysstr, fsid_array, &array_size, NULL, 0);
		fs_count = array_size / sizeof(fsid_t);
	};
	if ((result == -1) || (fs_count >= fsid_array_size)) {
		/* The only way to be sure ALL mounted filesystems have been included is
		 * to see the system return a value less than the available buffer space:
		 */
		result = sysctlbyname(sysstr, NULL, &array_size, NULL, 0);
		fs_count = array_size / sizeof(fsid_t);
		if (resize_fsid_array(fs_count + FSARRAY_SIZE_MARGIN) != 0) return 0;
		
		/* fsid_array_size SHOULD always be >0 now, but would be infinite loop otherwise... */
		if (fsid_array_size == 0) return 0;
		
		goto sysctl_loop;
	};
	
	*fsidp = fsid_array;
	return fs_count;
}

int
create_vfslist(fsid_t **fsidp, int *count)
{
	fsid_t *fsidlist;
	size_t fs_count;

	pthread_mutex_lock(&fsid_list_mutex);
	
	fs_count = get_vfslist(&fsidlist);
	if (fs_count > 0) {
		*fsidp = (fsid_t *)malloc(fs_count * sizeof(fsid_t));
		if (*fsidp) {
			bcopy(fsidlist, *fsidp, fs_count * sizeof(fsid_t));
		} else {
			fs_count = -1;
		};
	};
	
	pthread_mutex_unlock(&fsid_list_mutex);
	
	*count = fs_count;
	return 0;
}

void
free_vfslist(fsid_t *fsid)
{
	free(fsid);
}

static int
resize_statfs_array(size_t newentrycount)
{
	if (statfs_array) {
		statfs_array = realloc(statfs_array, sizeof(struct statfs) * newentrycount);
	} else {
		statfs_array = malloc(sizeof(struct statfs) * newentrycount);
	};
	if (statfs_array == NULL) return ENOMEM;
	statfs_array_size = newentrycount;
	
	return 0;
}

size_t
update_fsstat_array(struct statfs **sfsp)
{
	size_t fs_count;
	
fsstat_loop:
	if (statfs_array_size == 0) {
		fs_count = -1;				/* Fall into error handling loop, below */
	} else {
		fs_count = getfsstat(statfs_array, statfs_array_size * sizeof(struct statfs), MNT_NOWAIT);
		statfs_array_entries = fs_count;
	};
	if ((fs_count == -1) || (fs_count >= statfs_array_size)) {
		/* The only way to be sure ALL mounted filesystems have been included is
		 * to see the system return a value less than the available buffer space:
		 */
		fs_count = getfsstat(NULL, 0, MNT_NOWAIT);
		if (resize_statfs_array(fs_count + FSARRAY_SIZE_MARGIN) != 0) return 0;
		
		/* statfs_array_size SHOULD always be >0 now, but would be infinite loop otherwise... */
		if (statfs_array_size == 0) return 0;
		
		goto fsstat_loop;
	};
	
	if (sfsp) *sfsp = statfs_array;
	return fs_count;
}

int
find_fsstat_by_path(const char *path, bool exact_match, struct statfs **sfsp)
{
	int fs;
	size_t pathlength = strlen(path);
	
	for (fs = 0; fs < statfs_array_entries; ++fs) {
		if ((!exact_match || (exact_match && (pathlength == strlen(statfs_array[fs].f_mntonname)))) &&
			(strncmp(statfs_array[fs].f_mntonname, path, pathlength) == 0)) {
			if (sfsp) *sfsp = &statfs_array[fs];
			return 0;
		}
	}
	
	if (sfsp) *sfsp = (struct statfs *)0;
	return 1;
}

void
invalidate_fsstat_array(void)
{
	statfs_array_valid = false;
}

int
revalidate_fsstat_array(struct statfs **sfsp)
{
	int fscount = statfs_array_entries;
	
	if (!statfs_array_valid) {
		fscount = update_fsstat_array(NULL);
		if (fscount > 0) statfs_array_valid = true;
	};
	
	if (sfsp) *sfsp = statfs_array;
	return fscount;
}

int
sysctl_statfs(fsid_t *fsid, struct statfs *sfs, int flag)
{
	size_t size;

	size = sizeof(*sfs);
	return (sysctl_fsid(VFS_CTL_STATFS, fsid, sfs, &size, &flag,
		sizeof(flag)));
}

int
sysctl_queryfs(fsid_t *fsid, struct vfsquery *vq)
{
	size_t size;

	size = sizeof(*vq);
	return (sysctl_fsid(VFS_CTL_QUERY, fsid, vq, &size, NULL, 0));
}

int
sysctl_unmount(fsid_t *fsid, int flag)
{

	return (sysctl_fsid(VFS_CTL_UMOUNT, fsid, NULL, 0, &flag,
		sizeof(flag)));
}

int
sysctl_setfstimeout(fsid_t *fsid, int timeo)
{

	return (sysctl_fsid(VFS_CTL_TIMEO, fsid, NULL, 0,
		    &timeo, sizeof(timeo)));
}

int
sysctl_getfstimeout(fsid_t *fsid, int *timeop)
{
	size_t size = sizeof(timeop);

	return (sysctl_fsid(VFS_CTL_TIMEO, fsid, timeop, &size,
		    NULL, 0));
}
