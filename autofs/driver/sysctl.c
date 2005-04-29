/*
 * Copyright (c) 2003, 2004 Apple Computer, Inc. All rights reserved.
 *
 * Created by Patrick Dirks on Mon Mar 17 2003.
 *
 * $Id: sysctl.c,v 1.2 2004/05/27 08:07:47 pwd Exp $
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/mount.h>
#include <sys/sysctl.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#include "sysctl.h"

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
	if (sysctlnametomib(sysstr, ctlname, &ctllen) == -1) {
		warn("sysctlnametomib(%s)", sysstr);
		return -1;
	};
	ctlname[ctllen] = op;

	bzero(&vc, sizeof(vc));
	vc.vc_vers = VFS_CTL_VERS1;
	vc.vc_fsid = *fsid;
	vc.vc_ptr = newp;
	vc.vc_len = newlen;
	return (sysctl(ctlname, ctllen + 1, oldp, oldlenp, &vc, sizeof(vc)));
}

int
vfslist_get(fsid_t **fsidp, int *count)
{
	int x;
	size_t olen;
	fsid_t *fsidlst;
	const char *sysstr = "vfs.generic.vfsidlist";

rescan:
	x = sysctlbyname(sysstr, NULL, &olen, NULL, 0);
	if (x == -1) return -1;

	fsidlst = malloc(olen);
	if (fsidlst == NULL) return -1;

	x = sysctlbyname(sysstr, fsidlst, &olen, NULL, 0);
	if (x == -1) {
		if (errno == ENOMEM) {
			free(fsidlst);
			goto rescan;
		}
		return -1;
	}
	*fsidp = fsidlst;
	*count = (int)(olen / sizeof(fsid_t));
	return 0;
}

void
vfslist_free(fsid_t *fsid)
{
	free(fsid);
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
	size_t size = sizeof(*timeop);

	return (sysctl_fsid(VFS_CTL_TIMEO, fsid, timeop, &size,
		    NULL, 0));
}
