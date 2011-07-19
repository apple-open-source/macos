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
 * Portions Copyright 2007-2011 Apple Inc.
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

#include "autofs_types.h"

#define MNTTYPE_AUTOFS	"autofs"

#define AUTOFS_ARGSVERSION	2	/* change when autofs_args changes */

/*
 * Mount types.
 */
#define	MOUNT_TYPE_MAP			0	/* top-level map mount */
#define MOUNT_TYPE_TRIGGERED_MAP	1	/* map mount on a trigger */
#define MOUNT_TYPE_SUBTRIGGER		2	/* subtrigger */

/*
 * autofs mount args, as seen by userland code.
 * This structure is different in ILP32 and LP64.
 */
struct autofs_args {
	uint32_t	version;	/* args structure version number */
	char		*path;		/* autofs mountpoint */
	char		*opts;		/* default mount options */
	char		*map;		/* name of map */
	char		*subdir;	/* subdir within map */
	char		*key;		/* used in direct mounts */
	uint32_t	mntflags;	/* Boolean default mount options */
	int32_t		direct;		/* 1 = direct mount */
	int32_t		mount_type;	/* mount type - see above */
	int32_t		node_type;	/* node_type value for root node */
};

#ifdef KERNEL
/*
 * ILP32 version of autofs_args, for use in the kernel when fetching args
 * from userland in a 32-bit process.
 * WARNING - keep in sync with autofs_args
 */
struct autofs_args_32 {
	uint32_t	version;	/* args structure version number */
	uint32_t	path;		/* autofs mountpoint */
	uint32_t	opts;		/* default mount options */
	uint32_t	map;		/* name of map */
	uint32_t	subdir;		/* subdir within map */
	uint32_t	key;		/* used in direct mounts */
	uint32_t	mntflags;	/* Boolean default mount options */
	int32_t		direct;		/* 1 = direct mount */
	int32_t		mount_type;	/* mount type - see above */
	int32_t		node_type;	/* node_type value for root node */
};

/*
 * LP64 version of autofs_args, for use in the kernel when fetching args
 * from userland in a 64-bit process.
 * WARNING - keep in sync with autofs_args
 */
struct autofs_args_64 {
	uint32_t	version;	/* args structure version number */
	user_addr_t	path __attribute((aligned(8)));	/* autofs mountpoint */
	user_addr_t	opts __attribute((aligned(8)));	/* default mount options */
	user_addr_t	map __attribute((aligned(8)));	/* name of map */
	user_addr_t	subdir __attribute((aligned(8)));/* subdir within map */
	user_addr_t	key __attribute((aligned(8)));	/* used in direct mounts */
	uint32_t	mntflags;	/* Boolean default mount options */
	int32_t		direct;		/* 1 = direct mount */
	int32_t		mount_type;	/* mount type - see above */
	int32_t		node_type;	/* node_type value for root node */
};
#endif

/*
 * Autofs-specific mount options.
 */
#define MNTOPT_RESTRICT		"restrict"
#define	AUTOFS_MNT_RESTRICT	0x00000001
#define	AUTOFS_MNT_NOBROWSE	0x00000002	/* autofs's notion of "nobrowse", not OS X's */
#define MNTOPT_HIDEFROMFINDER	"hidefromfinder"
#define	AUTOFS_MNT_HIDEFROMFINDER	0x00000004

#ifdef __APPLE_API_PRIVATE
/* arg is int */
#define AUTOFS_CTL_DEBUG	0x0001	/* toggle debug. */
#endif /* __APPLE_API_PRIVATE */

/*
 * autofs update args, as seen by userland code.
 * This structure is different in ILP32 and LP64.
 */
struct autofs_update_args {
	fsid_t		fsid;
	char		*opts;		/* default mount options */
	char		*map;		/* name of map */
	uint32_t	mntflags;	/* Boolean default mount options */
	int32_t		direct;		/* 1 = direct mount */
	int32_t		node_type;	/* node_type value for root node */
};

#ifdef KERNEL
/*
 * ILP32 version of autofs_update_args, for use in the kernel when fetching args
 * from userland in a 32-bit process.
 * WARNING - keep in sync with autofs_update_args
 */
struct autofs_update_args_32 {
	fsid_t		fsid;
	uint32_t	opts;		/* default mount options */
	uint32_t	map;		/* name of map */
	uint32_t	mntflags;	/* Boolean default mount options */
	int32_t		direct;		/* 1 = direct mount */
	int32_t		node_type;	/* node_type value for root node */
};

/*
 * LP64 version of autofs_update_args, for use in the kernel when fetching args
 * from userland in a 64-bit process.
 * WARNING - keep in sync with autofs_update_args
 */
struct autofs_update_args_64 {
	fsid_t		fsid;
	user_addr_t	opts __attribute((aligned(8)));	/* default mount options */
	user_addr_t	map __attribute((aligned(8)));	/* name of map */
	uint32_t	mntflags;	/* Boolean default mount options */
	int32_t		direct;		/* 1 = direct mount */
	int32_t		node_type;	/* node_type value for root node */
};
#endif

/*
 * Autofs ioctls.
 */
#define AUTOFS_SET_MOUNT_TO		_IOW('a', 0, int)
#ifdef KERNEL
#define AUTOFS_UPDATE_OPTIONS_32	_IOW('a', 1, struct autofs_update_args_32)
#define AUTOFS_UPDATE_OPTIONS_64	_IOW('a', 1, struct autofs_update_args_64)
#else
#define AUTOFS_UPDATE_OPTIONS		_IOW('a', 1, struct autofs_update_args)
#endif
#define AUTOFS_NOTIFYCHANGE		_IO('a', 2)
#define AUTOFS_WAITFORFLUSH		_IO('a', 3)
#define AUTOFS_UNMOUNT			_IOW('a', 4, fsid_t)
#define AUTOFS_UNMOUNT_TRIGGERED	_IO('a', 5)

/*
 * Autofs fsctls.
 */
#define AUTOFS_MARK_HOMEDIRMOUNT	_IO('a', 666)

/*
 * Node type flags
 */
#define NT_SYMLINK	0x00000001	/* node should be a symlink to / */
#define NT_TRIGGER	0x00000002	/* node is a trigger */
#define NT_FORCEMOUNT	0x00000004	/* wildcard lookup - must do mount to check for existence */

/*
 * Action Status
 * Automountd replies to autofs indicating whether the operation is done,
 * or further action needs to be taken by autofs.
 */
enum autofs_stat {
	AUTOFS_ACTION=0,	/* list of actions included */
	AUTOFS_DONE=1		/* no further action required by kernel */
};

struct mounta {
	char		*dir;
	char		*opts;
	char		*path;
	char		*map;
	char		*subdir;
	char		*trig_mntpnt;
	int		flags;
	int		mntflags;
	uint32_t	isdirect;
	uint32_t	needs_subtrigger;
	char		*key;
};

typedef struct action_list {
	struct mounta mounta;
	struct action_list *next;
} action_list;	

/*
 * Flags returned by autofs_mount() and autofs_mount_url().
 */
#define MOUNT_RETF_DONTUNMOUNT		0x00000001	/* don't auto-unmount or preemptively unmount this */
#define MOUNT_RETF_DONTPREUNMOUNT	0x00000002	/* don't preemptively unmount this */

#ifndef KERNEL
/*
 * XXX - "struct dirent" is different depending on whether
 * __DARWIN_64_BIT_INO_T is defined or not.  We define it,
 * so we can get the long f_mntonname field, but the autofs
 * file system expects us to return the form of the structure
 * you get when __DARWIN_64_BIT_INO_T isn't defined.
 *
 * For now, we solve this by defining "struct dirent_nonext"
 * to be the old directory entry.
 */
struct dirent_nonext {
	__uint32_t d_ino;		/* file number of entry */
	__uint16_t d_reclen;		/* length of this record */
	__uint8_t  d_type;		/* file type, see below */
	__uint8_t  d_namlen;		/* length of string in d_name */
	char d_name[__DARWIN_MAXNAMLEN + 1];	/* name must be no longer than this */
};
#else
/* In the kernel, "struct direct" is always the non-extended directory entry. */
#define dirent_nonext	dirent
#endif

/*
 * Macros for readdir.
 */
#define	DIRENT_NAMELEN(reclen)	\
	((reclen) - (offsetof(struct dirent_nonext, d_name[0])))
#define DIRENT_RECLEN(namlen)	\
	(((u_int)sizeof (struct dirent_nonext) - (MAXNAMLEN+1)) + (((namlen)+1 + 3) &~ 3))
#define RECLEN(dp)	DIRENT_RECLEN((dp)->d_namlen)
#define nextdp(dp)	((struct dirent_nonext *)((char *)(dp) + RECLEN(dp)))

/*
 * Autofs device; opened by automountd, which needs to avoid triggering
 * mounts and to bypass locks on in-progress mounts, so it can do mounts,
 * to let autofs know what its PID is.
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
 * Autofs notrigger device; opened by processes that don't want to trigger
 * mounts, to let autofs know what their PIDs are.
 * This name is relative to /dev.
 */
#define AUTOFS_NOTRIGGER_DEVICE	"autofs_notrigger"

/*
 * Autofs homedirmounter device; opened by processes that want to mount
 * home directories on an autofs trigger.
 * This name is relative to /dev.
 */
#define AUTOFS_HOMEDIRMOUNTER_DEVICE	"autofs_homedirmounter"

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
