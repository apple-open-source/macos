/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * Created by Patrick Dirks on Mon Mar 17 2003.
 *
 * $Id: vfs_sysctl.h,v 1.4 2004/10/13 17:42:28 lindak Exp $
 */

#ifndef VFS_SYSCTL_H
#define VFS_SYSCTL_H

#include <c.h>

/* arg is struct autofs_userreq */
#define AUTOFS_CTL_GETREQS	0x0001	/* Get mount requests. */
#define AUTOFS_CTL_SERVREQ	0x0002	/* Serve mount requests. */

/* arg is struct autofs_mounterreq */
#define AUTOFS_CTL_MOUNTER	0x0003	/* Set as a mounter. */
#define AUTOFS_CTL_TRIGGER	0x0004	/* Set up a trigger. */

#ifdef __APPLE_API_PRIVATE
/* arg is int */
#define AUTOFS_CTL_DEBUG	0x0005	/* toggle debug. */
#endif /* __APPLE_API_PRIVATE */

#define AUTOFS_PROTOVERS	0x02

/*
 * Structure passed back and forth from userland to
 * describe/fullfill a pending mount request.
 */
struct autofs_userreq {
	char au_name[PATH_MAX];	/* name, useful for debug. */
	ino_t au_dino;	/* inode of directory. */
	ino_t au_ino;	/* inode of entry being looked up. */
	pid_t au_pid;	/* pid of (initial) requester. */
	uid_t au_uid;	/* uid of requester. */
	gid_t au_gid;	/* gid of requester. */
	int au_flags;	/* flags (none for now) */
	int au_errno;	/* errno passed back. */
	int au_pad[16];
};

/*
 * Structure passed with AUTOFS_CTL_MOUNTER, AUTOFS_CTL_TRIGGER
 * to register as the one responsible for a mount.
 */
struct autofs_mounterreq {
	ino_t amu_ino;		/* which inode we're mounting. */
	pid_t amu_pid;		/* which pid, (to register someone else.) */
	uid_t amu_uid;		/* which uid (for MOUNTER settings) */
	int amu_flags;		/* flags, see below. */
	int amu_pad[15];
};

/* flags for autofs_mounterreq.amu_flags */
#define AUTOFS_MOUNTERREQ_UID	0x01	/* seperate vnodes per uid. */
#define AUTOFS_MOUNTERREQ_DEFER 0x0002	/* content generation deferred */

int vfsevent_init(void);
u_int vfsevent_wait(int , int);
int	sysctl_fsid(int, fsid_t *, void *, size_t *, void *, size_t);
size_t get_vfslist(fsid_t **);
int	create_vfslist(fsid_t **, int *);
void free_vfslist(fsid_t *);
size_t update_fsstat_array(struct statfs **);
int find_fsstat_by_path(const char *path, bool exact_match, struct statfs **sfsp);
void invalidate_fsstat_array(void);
int revalidate_fsstat_array(struct statfs **sfsp);
int	sysctl_statfs(fsid_t *, struct statfs *, int);
int	sysctl_queryfs(fsid_t *, struct vfsquery *);
int	sysctl_unmount(fsid_t *, int);
int	sysctl_setfstimeout(fsid_t *, int);
int	sysctl_getfstimeout(fsid_t *, int *);

#endif /* !VFS_SYSCTL_H */
