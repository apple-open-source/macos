/*
 * Copyright (c) 1999-2004 Apple Computer, Inc. All rights reserved.
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
/*
 * Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved
 */
/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * The NEXTSTEP Software License Agreement specifies the terms
 * and conditions for redistribution.
 *
 *	@(#)webdav.h	8.4 (Berkeley) 1/21/94
 */

#ifndef _WEBDAV_H_INCLUDE
#define _WEBDAV_H_INCLUDE

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ucred.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/ioccom.h>

#ifdef KERNEL
#include <libkern/locks.h>
	#define DEBUG 0
#else
	#define DEBUG 0
#endif
/* Webdav file operation constants */
#define WEBDAV_LOOKUP			1
#define WEBDAV_CREATE			2
#define WEBDAV_OPEN				3
#define WEBDAV_CLOSE			4
#define WEBDAV_GETATTR			5
#define WEBDAV_SETATTR			6
#define WEBDAV_READ				7
#define WEBDAV_WRITE			8
#define WEBDAV_FSYNC			9
#define WEBDAV_REMOVE			10
#define WEBDAV_RENAME			11
#define WEBDAV_MKDIR			12
#define WEBDAV_RMDIR			13
#define WEBDAV_READDIR			14
#define WEBDAV_STATFS			15
#define WEBDAV_UNMOUNT			16
#define WEBDAV_INVALCACHES		17
/* for the future */
#define WEBDAV_LINK				18
#define WEBDAV_SYMLINK			19
#define WEBDAV_READLINK			20
#define WEBDAV_MKNOD			21
#define WEBDAV_GETATTRLIST		22
#define WEBDAV_SETATTRLIST		23
#define WEBDAV_EXCHANGE			24
#define WEBDAV_READDIRATTR		25
#define WEBDAV_SEARCHFS			26
#define WEBDAV_COPYFILE			27
#define WEBDAV_WRITESEQ			28

/* Webdav file type constants */
#define WEBDAV_FILE_TYPE		1
#define WEBDAV_DIR_TYPE			2

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

/* Shared (kernel & processs) WebDAV structures */

typedef int webdav_filetype_t;

// XXX Dependency on __DARWIN_64_BIT_INO_T
// We cannot pass ino_t back and forth between the kext and
// webdavfs_agent, because ino_t is in flux right now.
// In user space ino_t is 64-bits, but 32 bits in kernel space.
// So we have to define our own type for now (webdav_ino_t).
//
// This was the root cause of:
// <rdar://problem/6491194> 10A245+10A246: WebDAV FS complains about file names being too long.
//
// Once ino_t is identical in length for kernel and user space, then
// we can get rid of webdav_ino_t and just use ino_t exclusively.
//
typedef uint32_t webdav_ino_t;

/* Shared (kernel & process) WebDAV defninitions */

/*
 * An opaque_id is used to find a file system object in userland.
 * Lookup returns it. The rest of the operations which act upon a file system
 * object use it. If the file system object in userland goes away, the opaque_id
 * will be invalidated and messages to userland will fail with ESTALE.
 * The opaque_id 0 (kInvalidOpaqueID) is never valid.
 */
#define kInvalidOpaqueID   0
typedef uint32_t opaque_id;

/*
 * IMPORTANT: struct user_webdav_args, struct webdav_args, and webdav_mount()
 * must all be changed if the structure changes.
 */

/*
 * kCurrentWebdavArgsVersion MUST be incremented anytime changes are made in
 * either the WebDAV file system's kernel or user-land code which require both
 * executables to be released as a set.
 */
#define kCurrentWebdavArgsVersion 5

#pragma options align=packed

struct webdav_vfsstatfs
{
	uint32_t	f_bsize;	/* fundamental file system block size */
	uint32_t	f_iosize;	/* optimal transfer block size */
	uint64_t	f_blocks;	/* total data blocks in file system */
	uint64_t	f_bfree;	/* free blocks in fs */
	uint64_t	f_bavail;	/* free blocks avail to non-superuser */
	uint64_t	f_files;	/* total file nodes in file system */
	uint64_t	f_ffree;	/* free file nodes in fs */
};

struct user_webdav_args
{
	user_addr_t pa_mntfromname;					/* mntfromname */
	int	pa_version;								/* argument struct version */
	int pa_socket_namelen;						/* Socket to server name length */
	user_addr_t pa_socket_name;					/* Socket to server name */
	user_addr_t pa_vol_name;					/* volume name */
	u_int32_t pa_flags;							/* flag bits for mount */
	u_int32_t pa_server_ident;					/* identifies some (not all) types of servers we are connected to */
	opaque_id pa_root_id;						/* root opaque_id */
	webdav_ino_t pa_root_fileid;				/* root fileid */
	uid_t		pa_uid;							/* effective uid of the mounting user */
	gid_t		pa_gid;							/* effective gid of the mounting user */
	off_t pa_dir_size;							/* size of directories */
	/* pathconf values: >=0 to return value; -1 if not supported */
	int pa_link_max;							/* maximum value of a file's link count */
	int pa_name_max;							/* The maximum number of bytes in a file name (does not include null at end) */
	int pa_path_max;							/* The maximum number of bytes in a relative pathname (does not include null at end) */
	int pa_pipe_buf;							/* The maximum number of bytes that can be written atomically to a pipe (usually PIPE_BUF if supported) */
	int pa_chown_restricted;					/* Return _POSIX_CHOWN_RESTRICTED if appropriate privileges are required for the chown(2) */
	int pa_no_trunc;							/* Return _POSIX_NO_TRUNC if file names longer than KERN_NAME_MAX are truncated */
	/* end of webdav_args version 1 */
	struct webdav_vfsstatfs pa_vfsstatfs;				/* need this to fill out the statfs struct during the mount */
};

struct webdav_args
{
	char *pa_mntfromname;						/* mntfromname */
	int	pa_version;								/* argument struct version */
	int pa_socket_namelen;						/* Socket to server name length */
	struct sockaddr *pa_socket_name;			/* Socket to server name */
	char *pa_vol_name;							/* volume name */
	u_int32_t pa_flags;							/* flag bits for mount */
	u_int32_t pa_server_ident;					/* identifies some (not all) types of servers we are connected to */
	opaque_id pa_root_id;						/* root opaque_id */
	webdav_ino_t pa_root_fileid;				/* root fileid */
	uid_t		pa_uid;							/* effective uid of the mounting user */
	gid_t		pa_gid;							/* effective gid of the mounting user */	
	off_t pa_dir_size;							/* size of directories */
	/* pathconf values: >=0 to return value; -1 if not supported */
	int pa_link_max;							/* maximum value of a file's link count */
	int pa_name_max;							/* The maximum number of bytes in a file name (does not include null at end) */
	int pa_path_max;							/* The maximum number of bytes in a relative pathname (does not include null at end) */
	int pa_pipe_buf;							/* The maximum number of bytes that can be written atomically to a pipe (usually PIPE_BUF if supported) */
	int pa_chown_restricted;					/* Return _POSIX_CHOWN_RESTRICTED if appropriate privileges are required for the chown(2) */
	int pa_no_trunc;							/* Return _POSIX_NO_TRUNC if file names longer than KERN_NAME_MAX are truncated */
	/* end of webdav_args version 1 */
	struct webdav_vfsstatfs pa_vfsstatfs;				/* need this to fill out the statfs struct during the mount */
};


/* Defines for webdav_args pa_flags field */
#define WEBDAV_SUPPRESSALLUI	0x00000001		/* SuppressAllUI flag */
#define WEBDAV_SECURECONNECTION	0x00000002		/* Secure connection flag (the connection to the server is secure) */

/* Defines for webdav_args pa_server_ident field */
#define WEBDAV_IDISK_SERVER			0x00000001
#define WEBDAV_MICROSOFT_IIS_SERVER	0x00000002

struct webdav_cred
{
	uid_t pcr_uid;								/* From ucred */
};

/* WEBDAV_LOOKUP */
struct webdav_request_lookup
{
	struct webdav_cred pcr;				/* user and groups */
	opaque_id		dir_id;				/* directory to search */
	int				force_lookup;		/* if TRUE, don't use a cached lookup */
	uint32_t		name_length;		/* length of name */
	char			name[];				/* filename to find */
};

struct webdav_timespec64
{
	uint64_t	tv_sec;
	uint64_t	tv_nsec;
};

struct webdav_reply_lookup
{
	opaque_id		obj_id;				/* opaque_id of object corresponding to name */
	webdav_ino_t	obj_fileid;			/* object's file ID number */
	webdav_filetype_t obj_type;			/* WEBDAV_FILE_TYPE or WEBDAV_DIR_TYPE */
	struct webdav_timespec64 obj_atime;		/* time of last access */
	struct webdav_timespec64 obj_mtime;		/* time of last data modification */
	struct webdav_timespec64 obj_ctime;		/* time of last file status change */
	struct webdav_timespec64 obj_createtime; /* file creation time */	
	off_t			obj_filesize;		/* filesize of object */
};	

/* WEBDAV_CREATE */
struct webdav_request_create
{
	struct webdav_cred pcr;				/* user and groups */
	opaque_id		dir_id;				/* The opaque_id for the directory in which the file is to be created */
	mode_t			mode;				/* file type and initial file access permissions for the file */
	uint32_t		name_length;		/* length of name */
	char			name[];				/* The name that is to be associated with the created file */
};

struct webdav_reply_create
{
	opaque_id		obj_id;				/* opaque_id of file corresponding to name */
	webdav_ino_t	obj_fileid;			/* file's file ID number */
};

/* WEBDAV_MKDIR */
struct webdav_request_mkdir
{
	struct webdav_cred pcr;				/* user and groups */
	opaque_id		dir_id;				/* The opaque_id for the directory in which the file is to be created */
	mode_t			mode;				/* file type and initial file access permissions for the file */
	uint32_t		name_length;		/* length of name */
	char			name[];				/* The name that is to be associated with the created directory */
};

struct webdav_reply_mkdir
{
	opaque_id		obj_id;				/* opaque_id of directory corresponding to name */
	webdav_ino_t	obj_fileid;			/* directory's file ID number */
};

/* WEBDAV_OPEN */
struct webdav_request_open
{
	struct webdav_cred pcr;				/* user and groups */
	opaque_id		obj_id;				/* opaque_id of object */
	int				flags;				/* file access flags (O_RDONLY, O_WRONLY, etc.) */
	int				ref;				/* the reference to the webdav object that the cache object should be associated with */
};

struct webdav_reply_open
{
	pid_t			pid;				/* process ID of file system daemon (for matching to ref's pid) */
};

/* WEBDAV_CLOSE */
struct webdav_request_close
{
	struct webdav_cred pcr;				/* user and groups */
	opaque_id		obj_id;				/* opaque_id of object */
};

struct webdav_reply_close
{
};

struct webdav_stat {
	dev_t	 	st_dev;		/* [XSI] ID of device containing file */
	webdav_ino_t st_ino;	/* [XSI] File serial number */
	mode_t	 	st_mode;	/* [XSI] Mode of file (see below) */
	nlink_t		st_nlink;	/* [XSI] Number of hard links */
	uid_t		st_uid;		/* [XSI] User ID of the file */
	gid_t		st_gid;		/* [XSI] Group ID of the file */
	dev_t		st_rdev;	/* [XSI] Device ID */
	struct	webdav_timespec64 st_atimespec;	/* time of last access */
	struct	webdav_timespec64 st_mtimespec;	/* time of last data modification */
	struct	webdav_timespec64 st_ctimespec;	/* time of last status change */
	struct	webdav_timespec64 st_createtimespec;	/* time file was created */
	off_t		st_size;	/* [XSI] file size, in bytes */
	blkcnt_t	st_blocks;	/* [XSI] blocks allocated for file */
	blksize_t	st_blksize;	/* [XSI] optimal blocksize for I/O */
	uint32_t	st_flags;	/* user defined flags for file */
	uint32_t	st_gen;		/* file generation number */
};

/* WEBDAV_GETATTR */
struct webdav_request_getattr
{
	struct webdav_cred pcr;				/* user and groups */
	opaque_id		obj_id;				/* opaque_id of object */
};

struct webdav_reply_getattr
{
	struct webdav_stat	obj_attr;			/* attributes for the object */
};

/* WEBDAV_SETATTR XXX not needed at this time */
struct webdav_request_setattr
{
	struct webdav_cred pcr;				/* user and groups */
	opaque_id		obj_id;				/* opaque_id of object */
	struct stat		new_obj_attr;		/* new attributes of the object */
};

struct webdav_reply_setattr
{
};

/* WEBDAV_READ */
struct webdav_request_read
{
	struct webdav_cred pcr;				/* user and groups */
	opaque_id		obj_id;				/* opaque_id of file object */
	off_t			offset;				/* position within the file object at which the read is to begin */
	uint64_t		count;				/* number of bytes of data to be read (limited to WEBDAV_MAX_IO_BUFFER_SIZE (8000-bytes)) */
};

struct webdav_reply_read
{
};

/* WEBDAV_WRITE XXX not needed at this time */
struct webdav_request_write
{
	struct webdav_cred pcr;				/* user and groups */
	opaque_id		obj_id;				/* opaque_id of file object */
	off_t			offset;				/* position within the file object at which the write is to begin */
	uint64_t		count;				/* number of bytes of data to be written (limited to WEBDAV_MAX_IO_BUFFER_SIZE (8000-bytes)) */
	char			data[];				/* data to be written to the file object */
};

struct webdav_reply_write
{
	uint64_t		count;				/* number of bytes of data written to the file */
};

/* WEBDAV_FSYNC */
struct webdav_request_fsync
{
	struct webdav_cred pcr;				/* user and groups */
	opaque_id		obj_id;				/* opaque_id of object */
};

struct webdav_reply_fsync
{
};

/* WEBDAV_REMOVE */
struct webdav_request_remove
{
	struct webdav_cred pcr;				/* user and groups */
	opaque_id		obj_id;				/* opaque_id of entry to remove */
};

struct webdav_reply_remove
{
};

/* WEBDAV_RMDIR */
struct webdav_request_rmdir
{
	struct webdav_cred pcr;				/* user and groups */
	opaque_id		obj_id;				/* opaque_id of directory object to remove */
};

struct webdav_reply_rmdir
{
};

/* WEBDAV_RENAME */
struct webdav_request_rename
{
	struct webdav_cred pcr;				/* user and groups */
	opaque_id		from_dir_id;		/* opaque_id for the directory from which the entry is to be renamed */
	opaque_id		from_obj_id;		/* opaque_id for the object to be renamed */
	opaque_id		to_dir_id;			/* opaque_id for the directory to which the object is to be renamed */
	opaque_id		to_obj_id;			/* opaque_id for the object's new location if it exists (may be NULL) */
	uint32_t		to_name_length;		/* length of to_name */
	char			to_name[];			/* new name for the object */
};

struct webdav_reply_rename
{
};

/* WEBDAV_READDIR */
struct webdav_request_readdir
{
	struct webdav_cred pcr;				/* user and groups */
	opaque_id		obj_id;				/* opaque_id of directory to read */
	int				cache;				/* if TRUE, perform additional caching */
};

struct webdav_reply_readdir
{
};

/* WEBDAV_STATFS */

struct webdav_statfs {
	uint64_t	f_bsize;		/* fundamental file system block size */
	uint64_t	f_iosize;		/* optimal transfer block size */
	uint64_t	f_blocks;		/* total data blocks in file system */
	uint64_t	f_bfree;		/* free blocks in fs */
	uint64_t	f_bavail;		/* free blocks avail to non-superuser */
	uint64_t	f_files;		/* total file nodes in file system */
	uint64_t	f_ffree;		/* free file nodes in fs */
};

struct webdav_request_statfs
{
	struct webdav_cred pcr;				/* user and groups */
	opaque_id	root_obj_id;			/* opaque_id of the root directory */
};

struct webdav_reply_statfs
{
	struct webdav_statfs   fs_attr;		/* file system information */
										/*
										 * (required: f_bsize, f_iosize, f_blocks, f_bfree,
										 * f_bavail, f_files, f_ffree. The kext will either copy
										 * the remaining info from the mount struct, or the cached
										 * statfs struct in the mount struct IS the destination.
										 */
};

/* WEBDAV_UNMOUNT */
struct webdav_request_unmount
{
	struct webdav_cred pcr;				/* user and groups */
};

struct webdav_reply_unmount
{
};

/* WEBDAV_INVALCACHES */
struct webdav_request_invalcaches
{
	struct webdav_cred pcr;				/* user and groups */
};

struct webdav_reply_invalcaches
{
};

struct webdav_request_writeseq
{
	struct webdav_cred pcr;				/* user and groups */
	opaque_id		obj_id;				/* opaque_id of file object */
	off_t			offset;				/* position within the file object at which the write is to begin */
	off_t			count;				/* number of bytes of data to be written (limited to WEBDAV_MAX_IO_BUFFER_SIZE (8000-bytes)) */
	uint64_t		file_len;			/* length of the file after all sequential writes are done */
	uint32_t		is_retry;			/* non-zero indicates this request is a retry due to an EPIPE */
};

struct webdav_reply_writeseq
{
	uint64_t		count;				/* number of bytes of data written to the file */
};

union webdav_request
{
	struct webdav_request_lookup	lookup;
	struct webdav_request_create	create;
	struct webdav_request_open		open;
	struct webdav_request_close		close;
	struct webdav_request_getattr   getattr;
	struct webdav_request_setattr	setattr;
	struct webdav_request_read		read;
	struct webdav_request_write		write;
	struct webdav_request_fsync		fsync;
	struct webdav_request_remove	remove;
	struct webdav_request_rmdir		rmdir;
	struct webdav_request_rename	rename;
	struct webdav_request_readdir	readdir;
	struct webdav_request_statfs	statfs;
	struct webdav_request_invalcaches invalcaches;
	struct webdav_request_writeseq  writeseq;
};

union webdav_reply
{
	struct webdav_reply_lookup		lookup;
	struct webdav_reply_create		create;
	struct webdav_reply_open		open;
	struct webdav_reply_close		close;
	struct webdav_reply_getattr		getattr;
	struct webdav_reply_setattr		setattr;
	struct webdav_reply_read		read;
	struct webdav_reply_write		write;
	struct webdav_reply_fsync		fsync;
	struct webdav_reply_remove		remove;
	struct webdav_reply_rmdir		rmdir;
	struct webdav_reply_rename		rename;
	struct webdav_reply_readdir		readdir;
	struct webdav_reply_statfs		statfs;
	struct webdav_reply_invalcaches	invalcaches;
	struct webdav_reply_writeseq	writeseq;
};

#define UNKNOWNUID ((uid_t)99)

/*
 * The WEBDAV_CONNECTION_DOWN_MASK bit is set by the code in send_reply() in
 * activate.c in the int result when the mount_webdav daemon determines it cannot
 * communicate with the remote WebDAV server. webdav_sendmsg() and webdav_open() in
 * webdav_vnops.c check that bit to determine if the connection is up or down.
 */
#define WEBDAV_CONNECTION_DOWN_MASK	0x80000000

/*
 * fsctl(2) values for WebDAV FS
 */
 
/*
 * WEBDAVIOC_INVALIDATECACHES commmand passed to fsctl(2) causes WebDAV FS to
 * revalidate cached files with the WebDAV server and to invalidate all
 * all cached stat data.
 * example:
 * result = fsctl(path, WEBDAVIOC_INVALIDATECACHES, NULL, 0);
 */
#define	WEBDAVIOC_INVALIDATECACHES	_IO('w', 1)
#define	WEBDAV_INVALIDATECACHES		IOCBASECMD(WEBDAVIOC_INVALIDATECACHES)

/*
 * The WEBDAVIOC_WRITE_SEQUENTIAL command passed to fsctl(2) causes WebDAV FS to
 * enable Write Sequential mode on a vnode that is opened for writing.
 * The only parameter is a struct WebdavWriteSequential, that has a single field
 * to indicate the total length in bytes that will be written.
 *
 * Example:
 *
 *	struct WebdavWriteSequential {
 *		uint64_t file_len;
 *	};
 *
 *	struct WebdavWriteSequential req;
 *	req->file_len = 8192; // Will write 8k
 *	result = fsctl(path, WEBDAVIOC_WRITE_SEQUENTIAL, &req, 0);
 *
 *	Return values:
 *  0	-	Success
 *	EBUSY	-	File is already in Write Sequential mode.
 */
struct WebdavWriteSequential {
		uint64_t file_len;
};

#pragma options align=reset

#define WEBDAVIOC_WRITE_SEQUENTIAL	_IOW('z', 19, struct WebdavWriteSequential)
#define WEBDAV_WRITE_SEQUENTIAL		IOCBASECMD(WEBDAVIOC_WRITE_SEQUENTIAL)

/*
 * Sysctl values for WebDAV FS
 */
 
/*
 * If name[0] is WEBDAV_ASSOCIATECACHEFILE_SYSCTL, then 
 *		name[1] = a pointer to a struct open_associatecachefile
 *		name[2] = fd of cache file
 */
#define WEBDAV_ASSOCIATECACHEFILE_SYSCTL   1
/*
 * If name[0] is WEBDAV_NOTIFY_RECONNECTED_SYSCTL, then 
 *		name[1] = fsid.value[0]		// fsid byte 0 of reconnected file system
 *		name[2] = fsid.value[1]		// fsid byte 1 of reconnected file system 
 */
#define WEBDAV_NOTIFY_RECONNECTED_SYSCTL   2

#define WEBDAV_MAX_KEXT_CONNECTIONS 128			/* maximum number of open connections to user-land server */

#ifdef KERNEL

struct webdavmount
{
	vnode_t pm_root;							/* Root node */
	u_int32_t pm_status;						/* status bits for this mounted structure */
	struct mount *pm_mountp;					/* vfs structure for this filesystem */
	char *pm_vol_name;							/* volume name */
	struct sockaddr *pm_socket_name;			/* Socket to server name */
	u_int32_t pm_open_connections;				/* number of connections opened to user-land server */
	u_int32_t pm_server_ident;					/* identifies some (not all) types of servers we are connected to */
	off_t pm_dir_size;							/* size of directories */
	/* pathconf values: >=0 to return value; -1 if not supported */
	int pm_link_max;							/* maximum value of a file's link count (1 for file systems that do not support link counts) */
	int pm_name_max;							/* The maximum number of bytes in a file name (does not include null at end) */
	int pm_path_max;							/* The maximum number of bytes in a relative pathname (does not include null at end) */
	int pm_pipe_buf;							/* The maximum number of bytes that can be written atomically to a pipe (usually PIPE_BUF if supported) */
	int pm_chown_restricted;					/* Return _POSIX_CHOWN_RESTRICTED if appropriate privileges are required for the chown(2); otherwise 0 */
	int pm_no_trunc;							/* Return _POSIX_NO_TRUNC if file names longer than KERN_NAME_MAX are truncated; otherwise 0 */
	size_t pm_iosize;							/* saved iosize to use */
	uid_t		pm_uid;						/* effective uid of the mounting user */
	gid_t		pm_gid;						/* effective gid of the mounting user */	
	lck_mtx_t pm_mutex;							/* Protects pm_status adn pm_open_connections fields */
};

struct webdavnode
{
	lck_rw_t pt_rwlock;							/* this webdavnode's lock */
	LIST_ENTRY(webdavnode) pt_hash;				/* Hash chain. */
	struct mount *pt_mountp;					/* vfs structure for this filesystem */
	vnode_t pt_parent;							/* Pointer to parent vnode */
	vnode_t pt_vnode;							/* Pointer to vnode */
	vnode_t pt_cache_vnode;						/* Pointer to cached file vnode */
	opaque_id pt_obj_id;						/* opaque_id from lookup */
	webdav_ino_t pt_fileid;						/* file id */
	
	/* timestamp cache */
	struct webdav_timespec64 pt_atime;					/* time of last access */
	struct webdav_timespec64 pt_mtime;					/* time of last data modification */
	struct webdav_timespec64 pt_ctime;					/* time of last file status change */
	struct webdav_timespec64 pt_createtime;				/* file creation time */	
	struct webdav_timespec64 pt_mtime_old;				/* previous pt_mtime value (directory nodes only, used for negative name cache) */
	struct webdav_timespec64 pt_timestamp_refresh;		/* time of last timestamp refresh */
	
	off_t pt_filesize;							/* what we think the filesize is */
	u_int32_t pt_status;						/* WEBDAV_DIRTY, etc */
	u_int32_t pt_opencount;						/* reference count of opens */
	
	/* for Write Sequential mode */
	u_int32_t pt_opencount_write;				/* count of opens for writing */
	u_int32_t pt_writeseq_enabled;				/* TRUE if node Write Sequential mode is enabled */
	off_t pt_writeseq_offset;				/* offset we're expecting for the next write */
	uint64_t pt_writeseq_len;				/* total length in bytes that will be written in Write Sequential mode */
		
	/* SMP debug variables */
	void *pt_lastvop;							/* tracks last operation that locked this webdavnode */
	void *pt_activation;						/* tracks last thread that locked this webdavnode */
    u_int32_t pt_lockState;						/* current lock state */
};

struct open_associatecachefile
{
	vnode_t cachevp;
	pid_t   pid;
};

/* Defines for webdavnode pt_status field */

#define WEBDAV_DIRTY			0x00000001		/* Indicates webdavnode has data which has not been flushed to cache file */
#define WEBDAV_ACCESSED			0x00000002		/* Indicates file has been accessed - used by webdav_gettr to determine dates */
#define WEBDAV_ONHASHLIST		0x00000004		/* Indicates webdavnode is on the hash chain */
#define WEBDAV_DELETED			0x00000008		/* Indicates that webdav file (which is still referenced) has been deleted */
#define WEBDAV_DIR_NOT_LOADED   0x00000010		/* Indicates that an open directory is empty and needs to be populated from the server */
#define WEBDAV_INIT				0x00000020		/* Indicates that the webdavnode is in the process of being initialized */
#define WEBDAV_WAITINIT			0x00000040		/* Indicates that someone is sleeping (on webdavnode) waiting for initialization to finish */
#define WEBDAV_ISMAPPED			0x00000080		/* Indicates that the file is mapped */
#define WEBDAV_WASMAPPED		0x00000100		/* Indicates that the file is or was mapped */
#define WEBDAV_NEGNCENTRIES		0x00000200		/* Indicates one or more negative name cache entries exist (directory nodes only) */

/* Defines for webdavmount pm_status field */

#define WEBDAV_MOUNT_SUPPORTS_STATFS 0x00000001	/* Indicates that the server supports quata and quota used properties */
#define WEBDAV_MOUNT_STATFS 0x00000002			/* statfs is in progress */
#define WEBDAV_MOUNT_STATFS_WANTED 0x00000004	/* statfs wakeup is wanted */
#define WEBDAV_MOUNT_TIMEO 0x00000008			/* connection to webdav server was lost */
#define WEBDAV_MOUNT_DEAD 0x00000010			/* file system is dead. */
#define WEBDAV_MOUNT_SUPPRESS_ALL_UI 0x00000020	/* suppress UI when connection is lost */
#define WEBDAV_MOUNT_CONNECTION_WANTED 0x000000040 /* wakeup is wanted to start another connection with user-land server */
#define WEBDAV_MOUNT_SECURECONNECTION 0x000000080 /* the connection to the server is secure */

/* Webdav sizes for statfs */

#define WEBDAV_NUM_BLOCKS -1					/* not supported */
#define WEBDAV_FREE_BLOCKS	-1					/* not supported */
#define WEBDAV_NUM_FILES 65535					/* Like HFS */
#define WEBDAV_FREE_FILES (WEBDAV_NUM_FILES - 2) /* Used a couple */

/* Webdav status macros */

#define VFSTOWEBDAV(mp) ((struct webdavmount *)(vfs_fsprivate(mp)))
#define VTOWEBDAV(vp) ((struct webdavnode *)(vnode_fsnode(vp)))
#define WEBDAVTOV(pt) ((pt)->pt_vnode)
#define WEBDAVTOMP(pt) (vnode_mount(WEBDAVTOV(pt)))

/* Other defines */



/*
 * In webdav_read and webdav_pagein, webdav_read_bytes is called if the part of
 * file we need hasn't been downloaded from the server yet. However, since we're
 * already downloading the file, there's already data in the stream so reading
 * a range is counterproductive if we'll have downloaded the part we need (in the
 * stream) by the time webdav_read_bytes returns the data out of band.
 * Apache's mod_dav buffers 32K in the stream, so that's we'll use.
 */ 
#define WEBDAV_WAIT_IF_WITHIN	32768

/*
 * There are several loops where the code waits for a cache file to be downloaded,
 * or for a specific part of the cache file to be downloaded. This constant controls
 * how often the cache vnode is polled (with VNOP_GETATTR). The less often we poll,
 * the higher the latency between getting data and using it.
 *
 * A network connection with 35mbps (maximum cable) can give us a page every millisecond.
 * A network connection with 3mbps (typical capped cable or high speed DSL) can give us a page every 10 milliseconds.
 * For typical home networks over cable or DSL, 10 ms should be OK. Thus... (10 * 1000 * 1000) nanoseconds.
 */
#define WEBDAV_WAIT_FOR_PAGE_TIME (10 * 1000 * 1000)

/* the number of seconds soreceive() should block
 * before rechecking the server process state
 */
#define WEBDAV_SO_RCVTIMEO_SECONDS 10

/* How many times webdav_sendmsg() will block in soreceive()
 * before timing out the request.  The total
 * amount of seconds is WEBDAV_SO_RCVTIMEO_SECONDS * WEBDAV_MAX_SOCK_RCV_TIMEOUTS
 */
#define WEBDAV_MAX_SOCK_RCV_TIMEOUTS 9

/*
 * Used only for negative name caching, TIMESTAMP_NEGNCACHE_TIMEOUT is used by the webdav_vnop_lookup routine
 * to determine how often to fetch attributes from the server to check if a directory has been modified (va_mod_time timestamp).
 * We purge all negative name cache entries when the modification time of a directory has changed.
 *
 */
#define TIMESTAMP_CACHE_TIMEOUT 10

#if 0
	#define START_MARKER(str) \
	{ \
		log_vnop_start(str); \
	}
	#define RET_ERR(str, error) \
	{ \
		/*if (error)*/ \
			log_vnop_error(str, error); \
		return(error); \
	}
extern void log_vnop_start(char *str);
extern void log_vnop_error(char *str, int error);
#else
	#define START_MARKER(str)
	#define RET_ERR(str, error) return(error)
#endif


extern int( **webdav_vnodeop_p)();
extern void webdav_hashinit(void);
extern void webdav_hashdestroy(void);
extern void webdav_hashrem(struct webdavnode *);
extern void webdav_hashins(struct webdavnode *);
extern struct webdavnode *webdav_hashget(struct mount *mp, webdav_ino_t fileid, struct webdavnode *pt_new, uint32_t *inserted);

extern void webdav_copy_creds(vfs_context_t context, struct webdav_cred *dest);
extern int webdav_sendmsg(int vnop, struct webdavmount *fmp,
	void *request, size_t requestsize,
	void *vardata, size_t vardatasize,
	int *result, void *reply, size_t replysize);
extern int webdav_get(
	struct mount *mp,			/* mount point */
	vnode_t dvp,				/* parent vnode */
	int markroot,				/* if 1, mark as root vnode */
	struct componentname *cnp,  /* componentname */
	opaque_id obj_id,			/* object's opaque_id */
	webdav_ino_t obj_fileid,	/* object's file ID number */
	enum vtype obj_vtype,		/* VREG or VDIR */
	struct webdav_timespec64 obj_atime,  /* time of last access */
	struct webdav_timespec64 obj_mtime,  /* time of last data modification */
	struct webdav_timespec64 obj_ctime,  /* time of last file status change */
	struct webdav_timespec64 obj_createtime,  /* file creation time */					  
	off_t obj_filesize,			/* object's filesize */
	vnode_t *vpp);				/* vnode returned here */

extern int webdav_assign_ref(struct open_associatecachefile *associatecachefile, int *ref);
extern void webdav_release_ref(int ref);
extern char webdav_name[MFSNAMELEN];
				  
#endif /* KERNEL */

#endif /*ifndef _WEBDAV_H_INCLUDE */

