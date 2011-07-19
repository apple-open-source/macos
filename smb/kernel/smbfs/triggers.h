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
 * Information kept for each trigger vnode.
 */
typedef struct trigger_info trigger_info_t;

extern trigger_info_t *trigger_new(struct vnode_trigger_param *vnt,
    void *(*get_mount_args)(vnode_t, vfs_context_t, int *),
    void (*rel_mount_args)(void *));
extern void trigger_free(trigger_info_t *ti);

/*
 * The arguments passed to the thread routine that calls up to automountd
 * to resolve the trigger are assumed to be in a structure the first
 * element of which is one of these structures.
 */
struct trigger_callargs {
	vnode_t		tc_vp;		/* trigger vnode */
	fsid_t		tc_this_fsid;	/* fsid of file system with trigger vnode */
	trigger_info_t	*tc_ti;		/* trigger information */
	thread_t	tc_origin;	/* thread that fired up this thread */
					/* used for debugging purposes */
	mach_port_t	tc_gssd_port;	/* mach port for gssd */
	fsid_t		tc_mounted_fsid;/* fsid of newly-mounted file system */
	boolean_t	tc_is_volfs;	/* TRUE if newly-mounted fs is volfs */
};

/*
 * Arguments passed to thread routine to call up to automountd to mount
 * an arbitrary URL on an arbitrary path.
 */
struct mount_url_callargs {
	struct trigger_callargs muc_t;	/* common args */
	uid_t		muc_uid;	/* UID for which to do the mount */
	char		*muc_url;	/* URL to mount */
	char		*muc_mountpoint; /* where to mount it */
	char		*muc_opts;	/* mount options to use; null string if none */
};

#define muc_this_fsid		muc_t.tc_this_fsid
#define muc_origin		muc_t.tc_origin
#define muc_gssd_port		muc_t.tc_gssd_port
#define muc_mounted_fsid	muc_t.tc_mounted_fsid
#define muc_is_volfs		muc_t.tc_is_volfs

/*
 * Make an upcall to automountd to call SMBRemountServer() from the
 * SMB client framework.
 */
extern int SMBRemountServer(const void *ptr, size_t len, mach_port_t gssd_port);
