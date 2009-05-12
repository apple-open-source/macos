/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */
/*
 * Copyright 2004 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Portions Copyright 2007-2009 Apple Inc.
 */

/*
 * Definitions and declarations shared between the autofs kext and
 * user-mode code.
 */

#ifndef __AUTOFS_H__
#define __AUTOFS_H__

#include <stdint.h>

#include <sys/dirent.h>
#include <sys/mount.h>
#include <sys/ioccom.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifndef DT_AUTO
#define	DT_AUTO 15
#endif
#include <sys/stat.h>
#ifndef SF_AUTOMOUNT
#define	SF_AUTOMOUNT  0x80000000
#endif

#include "autofs_types.h"

#define MNTTYPE_AUTOFS	"autofs"

#define AUTOFS_ARGSVERSION	1	/* change when autofs_args changes */

/*
 * autofs mount args, as seen by userland code.
 * This structure is different in ILP32 and LP64.
 * Inside the kernel, it's the ILP32 version, as the kernel
 * is ILP32.
 */
struct autofs_args {
	int		version;	/* args structure version number */
	char		*path;		/* autofs mountpoint */
	char		*opts;		/* default mount options */
	char		*map;		/* name of map */
	char		*subdir;	/* subdir within map */
	char		*key;		/* used in direct mounts */
	uint32_t	mntflags;	/* Boolean default mount options */
	int32_t		mount_to;	/* time in secs the fs is to remain */
					/* mounted after last reference */
	int32_t		mach_to;	/* timeout for Mach calls XXX */
	int32_t		direct;		/* 1 = direct mount */
	int32_t		trigger;	/* 1 = trigger mount */
};

#ifdef KERNEL
/*
 * LP64 version of autofs_args, for use in the kernel when fetching args
 * from userland in a 64-bit process.
 * WARNING - keep in sync with autofs_args
 */
struct autofs_args_64 {
	int		version;	/* args structure version number */
	user_addr_t	path;		/* autofs mountpoint */
	user_addr_t	opts;		/* default mount options */
	user_addr_t	map;		/* name of map */
	user_addr_t	subdir;		/* subdir within map */
	user_addr_t	key;		/* used in direct mounts */
	uint32_t	mntflags;	/* Boolean default mount options */
	int32_t		mount_to;	/* time in secs the fs is to remain */
					/* mounted after last reference */
	int32_t		mach_to;	/* timeout for Mach calls XXX */
	int32_t		direct;		/* 1 = direct mount */
	int32_t		trigger;	/* 1 = trigger mount */
};
#endif

/*
 * Autofs-specific mount options.
 */
#define MNTOPT_RESTRICT		"restrict"
#define	AUTOFS_MNT_RESTRICT	0x00000001
#define MNTOPT_RDDIR		"rddir"		/* called "browse" in Solaris */
#define	AUTOFS_MNT_NORDDIR	0x00000002	/* but that collides with ours */

#ifdef __APPLE_API_PRIVATE
/* arg is int */
#define AUTOFS_CTL_DEBUG	0x0001	/* toggle debug. */
#endif /* __APPLE_API_PRIVATE */

/*
 * autofs update args, as seen by userland code.
 * This structure is different in ILP32 and LP64.
 * Inside the kernel, it's the ILP32 version, as the kernel
 * is ILP32.
 */
struct autofs_update_args {
	fsid_t		fsid;
	char		*opts;		/* default mount options */
	char		*map;		/* name of map */
	uint32_t	mntflags;	/* Boolean default mount options */
	int32_t		mount_to;	/* time in secs the fs is to remain */
					/* mounted after last reference */
	int32_t		mach_to;	/* timeout for Mach calls XXX */
	int32_t		direct;		/* 1 = direct mount */
};

#ifdef KERNEL
/*
 * LP64 version of autofs_update_args, for use in the kernel when fetching args
 * from userland in a 64-bit process.
 * WARNING - keep in sync with autofs_update_args
 */
struct autofs_update_args_64 {
	fsid_t		fsid;
	user_addr_t	opts;		/* default mount options */
	user_addr_t	map;		/* name of map */
	user_addr_t	key;		/* used in direct mounts */
	uint32_t	mntflags;	/* Boolean default mount options */
	int32_t		mount_to;	/* time in secs the fs is to remain */
					/* mounted after last reference */
	int32_t		mach_to;	/* timeout for Mach calls XXX */
	int32_t		direct;		/* 1 = direct mount */
};
#endif

/*
 * Autofs ioctls.
 */
#define AUTOFS_UPDATE_OPTIONS		_IOW('a', 0, struct autofs_update_args)
#ifdef KERNEL
#define AUTOFS_UPDATE_OPTIONS_64	_IOW('a', 1, struct autofs_update_args_64)
#endif
#define AUTOFS_NOTIFYCHANGE		_IO('a', 2)
#define AUTOFS_WAITFORFLUSH		_IO('a', 3)
#define AUTOFS_UNMOUNT			_IOW('a', 4, fsid_t)

/*
 * Action Status
 * Automountd replies to autofs indicating whether the operation is done,
 * or further action needs to be taken by autofs.
 */
enum autofs_stat {
	AUTOFS_ACTION=0,	/* list of actions included */
	AUTOFS_DONE=1		/* no further action required by kernel */
};

/*
 * Used by autofs to either create a link, or mount a new filesystem.
 */
enum autofs_action {
	AUTOFS_MOUNT_RQ=0,	/* mount request */
	AUTOFS_NONE=1		/* no action */
};

struct mounta {
	char		*dir;
	char		*opts;
	char		*path;
	char		*map;
	char		*subdir;
	int		flags;
	int		mntflags;
	uint32_t	isdirect;
	char		*key;
};

typedef struct action_list {
	struct mounta mounta;
	struct action_list *next;
} action_list;	
     
/*
 * Macros for readdir.
 */
#define	DIRENT_NAMELEN(reclen)	\
	((reclen) - (offsetof(struct dirent, d_name[0])))
#define DIRENT_RECLEN(namlen)	\
	((sizeof (struct dirent) - (MAXNAMLEN+1)) + (((namlen)+1 + 3) &~ 3))
#define RECLEN(dp)	DIRENT_RECLEN((dp)->d_namlen)
#define nextdp(dp)	((struct dirent *)((char *)(dp) + RECLEN(dp)))

/*
 * Autofs device; opened by processes that need to avoid triggering
 * mounts and to bypass locks on in-progress mounts, so they can do
 * mounts, to let autofs know what their PIDs are.
 * This name is relative to /dev.
 */
#define AUTOFS_DEVICE	"autofs"

/*
 * Autofs nowait device; opened by processes that want to trigger mounts
 * but don't want to wait for the mount to finish (for which read
 * "launchd"), to let autofs know what their PIDs are.
 * This name is relative to /dev.
 */
#define AUTOFS_NOWAIT_DEVICE	"autofs_nowait"

/*
 * Autofs control device; opened by the automount command to perform
 * various operations.  This name is relative to /dev.
 */
#define AUTOFS_CONTROL_DEVICE	"autofs_control"

#ifdef KERNEL
/*
 * Entry in a table of mount options; mirrors userland struct mntopt.
 */
struct mntopt {
	const char *m_option;	/* option name */
	int m_inverse;		/* if a negative option, eg "dev" */
	int m_flag;		/* bit to set, eg. MNT_RDONLY */
	int m_altloc;		/* 1 => set bit in altflags */
};

/*
 * Definitions for options that appear in RESTRICTED_MNTOPTS; mirrors
 * userland definitions.
 */
#define MOPT_NODEV		{ "dev",	1, MNT_NODEV, 0 }
#define MOPT_NOEXEC		{ "exec",	1, MNT_NOEXEC, 0 }
#define MOPT_NOSUID		{ "suid",	1, MNT_NOSUID, 0 }
#define MOPT_RDONLY		{ "rdonly",	0, MNT_RDONLY, 0 }
#endif

/*
 * List of mntoptions which are inherited when the "restrict" option 
 * is present.  The RESTRICT option must be first!
 * This define is shared between the kernel and the automount daemon.
 */
#define	RESTRICTED_MNTOPTS	\
	{ MNTOPT_RESTRICT,	0, AUTOFS_MNT_RESTRICT, 1 },	/* restricted autofs mount */	\
	MOPT_NODEV,										\
	MOPT_NOEXEC,										\
	MOPT_NOSUID,										\
	MOPT_RDONLY,										\
	{ NULL,		0, 0, 0 }	/* list terminator */

#ifdef	__cplusplus
}
#endif

#endif	/* __AUTOFS_H__ */
