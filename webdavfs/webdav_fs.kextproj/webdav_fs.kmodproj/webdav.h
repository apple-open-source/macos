#ifndef _WEBDAV_H_INCLUDE
#define _WEBDAV_H_INCLUDE


/* Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved */
/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * The NEXTSTEP Software License Agreement specifies the terms
 * and conditions for redistribution.
 *
 *	@(#)webdav.h	8.4 (Berkeley) 1/21/94
 */

#include "vnops.h"
#include <sys/vnode.h>
#include <sys/param.h>
#include <sys/attr.h>

#ifdef KERNEL
#include <sys/uio.h>
#include <sys/ubc.h>
#endif /* KERNEL */

struct webdav_args
{
	char *pa_config;							/* Config file */
	int pa_socket;								/* Socket to server */
	char *pa_uri;								/* url of mounted entity */
};

struct webdav_cred
{
	int pcr_flag;								/* File open mode */
	uid_t pcr_uid;								/* From ucred */
	short pcr_ngroups;							/* From ucred */
	gid_t pcr_groups[NGROUPS];					/* From ucred */
};

typedef struct
{
	int wd_first_uri_size;
	int wd_second_uri_size;
} webdav_rename_header_t;

typedef struct
{
	off_t wd_byte_start;
	off_t wd_num_bytes;
	int wd_uri_size;
} webdav_byte_read_header_t;

/*
 * The WEBDAV_CONNECTION_DOWN_MASK bit is set by the code in send_reply() and send_data()
 * activate.c in the int result when the mount_webdav daemon determines it cannot
 * communicate with the remote WebDAV server. webdav_sendmsg() and webdav_open() in
 * webdav_vnops.c check that bit to determine if the connection is up or down.
 */
#define WEBDAV_CONNECTION_DOWN_MASK	0x80000000

/*
 * WEBDAVINVALIDATECACHES commmand passed to fsctl(2) causes WebDAV FS to
 * revalidate cached files with the WebDAV server and to invalidate all
 * all cached stat data.
 * example:
 * result = fsctl(path, WEBDAVINVALIDATECACHES, NULL, 0);
 */
#define	WEBDAVINVALIDATECACHES	_IO('w', 1)


#ifdef KERNEL
struct webdavmount
{
	struct vnode *pm_root;						/* Root node */
	struct file *pm_server;						/* Held reference to server socket */
	u_int32_t status;							/* status bits for this mounted structure */
	struct mount *pm_mountp;					/* vfs structure for this filesystem */
};

struct webdavnode
{
	LIST_ENTRY(webdavnode) pt_hash;				/* Hash chain. */
	int pt_size;								/* Length of Arg */
	struct lock__bsd__ pt_lock;					/* node lock */
	char *pt_arg;								/* Arg to send to server */
	int pt_depth;								/* distance of this node from mount point */
	struct vnode *pt_cache_vnode;				/* Pointer to cached file vnode */
	struct vnode *pt_vnode;						/* Pointer to parent vnode */
	int pt_fileid;								/* cookie */
	u_int32_t pt_status;						/* Dirty Bit etc */
	webdav_filehandle_t pt_file_handle;			/* server process file handle */
	struct timespec pt_atime;					/* access time */
	struct timespec pt_mtime;					/* time last modified */
	struct timespec pt_ctime;					/* last metadata change time */
};

/* Defines for webdav status field */

#define WEBDAV_DIRTY 0x00000001					/* Indicates webdav node has data which has not been flushed to cache file */
#define WEBDAV_ACCESSED 0x00000002				/* Indicates file has been accessed - used by webdav_gettr to determine dates */
#define WEBDAV_ONHASHLIST 0x00000004			/* Indicates webdav node is on the has chaing */
#define WEBDAV_DELETED 0x00000008				/* Indicates that webdv file (which is still referenced ) has been deleted */
#define WEBDAV_DIR_NOT_LOADED 0x00000010		/* Indicates that an open directory is empty and needs to be populated from the server */

/* Defines for webdav mount structure status field */

#define WEBDAV_MOUNT_SUPPORTS_STATFS 0x00000001	/* Indicates that the server supports quata and quota used properties */
#define WEBDAV_MOUNT_STATFS 0x00000002			/* statfs is in progress */
#define WEBDAV_MOUNT_STATFS_WANTED 0x00000004	/* statfs wakeup is wanted */
#define WEBDAV_MOUNT_TIMEO 0x00000008			/* connection to webdav server was lost */
#define WEBDAV_MOUNT_FORCE 0x00000010			/* doing a forced unmount. */

/* Webdav sizes for statfs */

#define WEBDAV_NUM_BLOCKS -1					/* not supported */
#define WEBDAV_FREE_BLOCKS	-1					/* not supported */
#define WEBDAV_NUM_FILES 65535					/* Like HFS */
#define WEBDAV_FREE_FILES (WEBDAV_NUM_FILES - 2) /* Used a couple */
#define WEBDAV_IOSIZE (4*1024)					/* should be < WEBDAV_MAX_IO_BUFFER_SIZE */

/* Webdav status macros */

#define VFSTOWEBDAV(mp) ((struct webdavmount *)((mp)->mnt_data))
#define VTOWEBDAV(vp) ((struct webdavnode *)(vp)->v_data)
#define WEBDAVTOV(pt) ((pt)->pt_vnode)
#define WEBDAVISMAPPED(vp) (UBCINFOEXISTS(vp) && ubc_issetflags(vp, UI_WASMAPPED))


/* Other defines */
#define UNKNOWNUID ((uid_t)99)

/* The WEBDAV_MAX_IO_BUFFER_SIZE gates how many bytes we
 * will try to read with a byte range request to the server.
 * Because sockets are only so big we can't transfer 8192 bytes.
 * That many bytes won't fit in a buffer so rather than having
 * webdav_sendmsg wait for all data, it would have to loop.	 Limiting
 * at 8000 works (based on emprical study on Darwin). If you are porting
 * this code to another platform or if the default socket buffer size
 * changes you may need to change this constan to implement the looping.
 * None of this would be necessary if Darwin's soreserve actually did reserve
 * space rather than just enforcing limits
 */
 
#define WEBDAV_MAX_IO_BUFFER_SIZE 8000	/* Gates byte read optimization */

/*
 * In webdav_read and webdav_pagein, webdav_read_bytes is called if the part of
 * file we need hasn't been downloaded from the server yet. However, since we're
 * already downloading the file, there's already data in the stream so reading
 * a range is counterproductive if we'll have downloaded the part we need (in the
 * stream) by the time webdav_read_bytes returns the data out of band.
 * Apache's mod_dav buffers 32K in the stream, so that's we'll use.
 */ 
#define WEBDAV_WAIT_IF_WITHIN	32768

/* the number of seconds soreceive() should block
 * before rechecking the server process state
 */
#define WEBDAV_SO_RCVTIMEO_SECONDS 10

extern int( **webdav_vnodeop_p)();
extern struct vfsops webdav_vfsops;
extern void webdav_hashinit(void);
extern void webdav_hashdestroy(void);
extern void webdav_hashrem(struct webdavnode *);
extern void webdav_hashins(struct webdavnode *);
extern struct vnode *webdav_hashlookup(int, int, long, char *);
extern struct vnode *webdav_hashget(int, int, long, char *);
extern int webdav_sendmsg(int, int, struct webdavnode *, struct webdav_cred *, struct webdavmount *,
	struct proc *, void *, int, int *, void *, int, struct vnode *);
extern int webdav_getattrlist(struct vop_getattrlist_args *ap);
					  
/* Workaround for lack of current_proc() */
void *current_task(void);
void *get_bsdtask_info(void *);

#define current_proc()	((struct proc *) get_bsdtask_info(current_task()))

#endif /* KERNEL */
#endif /*ifndef _WEBDAV_H_INCLUDE */

