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
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */

/*
 * Portions Copyright 2007-2011 Apple Inc.
 */

#ifndef	_AUTOMOUNT_H
#define	_AUTOMOUNT_H

#pragma ident	"@(#)automount.h	1.69	05/09/30 SMI"

#include <assert.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/mount.h>		/* for fsid_t */
#include <oncrpc/rpc.h>
#include <netinet/in.h>		/* needed for sockaddr_in declaration */

#include <mach/mach.h>

#ifdef MALLOC_DEBUG
#include <debug_alloc.h>
#endif

#include "autofs.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef _REENTRANT
#define	fork1			vfork
#endif


/*
 * OS X autofs configuration file location
 */
#define	AUTOFSADMIN	"/etc/autofs.conf"

#define	MXHOSTNAMELEN	64
#define	MAXNETNAMELEN   255
/* We can't supply names that don't fit in a "struct dirent" */
#define	MAXFILENAMELEN	(sizeof (((struct dirent *)0)->d_name) - 1)
#define	LINESZ		4096
#define	MAXOPTSLEN	AUTOFS_MAXOPTSLEN

#define	AUTOFS_MOUNT_TIMEOUT	600	/* default min time mount will */
					/* remain mounted (in seconds) */
#define	AUTOFS_RPC_TIMEOUT	60	/* secs autofs will wait for */
					/* automountd's reply before */
					/* retransmitting */
/* stack ops */
#define	ERASE		0
#define	PUSH		1
#define	POP		2
#define	INIT		3
#define	STACKSIZ	30

#define	DIST_SELF	1
#define	DIST_MYSUB	2
#define	DIST_MYNET	3
#define	DIST_OTHER	4

#define	MAXIFS		32

/*
 * Retry operation related definitions.
 */
#define	RET_OK		0
#define	RET_RETRY	32
#define	RET_ERR		33
#define	INITDELAY	5
#define	DELAY_BACKOFF	2
#define	MAXDELAY	120
#define	DO_DELAY(delay) { \
	(void) sleep(delay); \
	delay *= DELAY_BACKOFF; \
	if (delay > MAXDELAY) \
		delay = MAXDELAY; \
}

struct mapline {
	char linebuf[LINESZ];
	char lineqbuf[LINESZ];
};

/*
 * Typedefs present in Solaris but not in OS X.
 */
typedef unsigned char uchar_t;
typedef unsigned short ushort_t;
typedef unsigned int uint_t;
typedef uint32_t rpcprog_t;
typedef uint32_t rpcvers_t;

/*
 * XXX - kill me if possible.
 */
struct mnttab {
	char	*mnt_special;
	char	*mnt_mountp;
	char	*mnt_fstype;
	char	*mnt_mntopts;
	char	*mnt_time;
};

/*
 * Structure describing a host/filesystem/dir tuple in a NIS map entry
 */
struct mapfs {
	struct mapfs *mfs_next;	/* next in entry */
	int	mfs_ignore;	/* ignore this entry */
	char	*mfs_host;	/* host name */
	char	*mfs_dir;	/* dir to mount */
	int	mfs_penalty;	/* mount penalty for this host */
	int	mfs_distance;	/* distance hint */
	struct nfs_args *mfs_args;	/* nfs_args */
	rpcvers_t	mfs_version;	/* NFS version */

#define	MFS_ALLOC_DIR		0x1	/* mfs_dir now points to different */
					/* buffer */

#define	MFS_URL			0x2	/* is NFS url listed in this tuple. */
#define	MFS_FH_VIA_WEBNFS	0x4	/* got file handle during ping phase */

	uint_t	mfs_flags;
	uint_t	mfs_port;	/* port# in NFS url */
};

/*
 * NIS entry - lookup of name in DIR gets us this
 */
struct mapent {
	char	*map_fstype;	/* file system type e.g. "nfs" */
	char	*map_mounter;	/* base fs e.g. "cachefs" */
	char	*map_root;	/* path to mount root */
	char	*map_mntpnt;	/* path from mount root */
	char	*map_mntopts;	/* mount options */
	char    *map_fsw;	/* mount fs information */
	char    *map_fswq;	/* quoted mountfs information */
	int	map_mntlevel;	/* mapentry hierarchy level */
	bool_t	map_modified;	/* flags modified mapentries */
	bool_t	map_faked;	/* flags faked mapentries */
	int	map_err;	/* flags any bad entries in the map */
	struct mapfs *map_fs;	/* list of replicas for nfs */
	struct mapent *map_next;
};


/*
 * Descriptor for each directory served by the automounter
 */
struct autodir {
	char	*dir_name;		/* mount point */
	char	*dir_map;		/* name of map for dir */
	char	*dir_opts;		/* default mount options */
	int	dir_direct;		/* direct mountpoint ? */
	char	*dir_realpath;		/* realpath() of mount point - not always set */
	struct autodir *dir_next;	/* next entry */
	struct autodir *dir_prev;	/* prev entry */
};

/*
 * This structure is used to build an array of
 * hostnames with associated penalties to be
 * passed to the nfs_cast procedure
 */
struct host_names {
	char *host;
	int  penalty;
};

/*
 * structure used to build list of contents for a map on
 * a readdir request
 */
struct dir_entry {
	char		*name;		/* name of entry */
	ino_t		nodeid;
	off_t		offset;
	char		*line;		/* map line for entry, or NULL */
	char		*lineq;		/* map quote line for entry, or NULLs */
	struct dir_entry *next;
	struct dir_entry *left;		/* left element in binary tree */
	struct dir_entry *right;	/* right element in binary tree */
};

/*
 * offset index table
 */
struct off_tbl {
	off_t			offset;
	struct dir_entry	*first;
	struct off_tbl		*next;
};

#ifndef NO_RDDIR_CACHE
/*
 * directory cache for 'map'
 */
struct rddir_cache {
	char			*map;
	struct off_tbl		*offtp;
	uint_t			bucket_size;
	time_t			ttl;
	struct dir_entry	*entp;
	pthread_mutex_t		lock;		/* protects 'in_use' field */
	int			in_use;		/* # threads referencing it */
	pthread_rwlock_t	rwlock;		/* protects 'full' and 'next' */
	int			full;		/* full == 1 when cache full */
	struct rddir_cache	*next;
};

#define	RDDIR_CACHE_TIME	300		/* in seconds */

#endif /* NO_RDDIR_CACHE */

/*
 * structure used to maintain address list for localhost
 */

struct myaddrs {
	struct sockaddr_in sin;
	struct myaddrs *myaddrs_next;
};

extern time_t timenow;	/* set at start of processing of each RPC call */
extern char self[];
extern int verbose;
extern int trace;
extern struct autodir *dir_head;
extern struct autodir *dir_tail;
struct autofs_args;
extern struct myaddrs *myaddrs_head;

extern pthread_rwlock_t	cache_lock;
extern pthread_rwlock_t rddir_cache_lock;

extern pthread_mutex_t cleanup_lock;
extern pthread_cond_t cleanup_start_cv;
extern pthread_cond_t cleanup_done_cv;

/*
 * "Safe" string operations; used with string buffers already
 * allocated to be large enough, so we just abort if this fails.
 */
#define CHECK_STRCPY(a, b, size) \
	assert(strlcpy(a, b, (size)) < (size))

#define CHECK_STRCAT(a, b, size) \
	assert(strlcat((a), (b), (size)) < (size))

/*
 * mnttab handling routines
 */
extern void free_mapent(struct mapent *);

#define MNTTYPE_NFS	"nfs"
#define MNTTYPE_LOFS	"lofs"

/*
 * utilities
 */
extern struct mapent *parse_entry(const char *, const char *, const char *,
				const char *, uint_t, int *, bool_t, bool_t,
				int *);
typedef enum {
	MEXPAND_OK,			/* expansion worked */
	MEXPAND_LINE_TOO_LONG,		/* line too long */
	MEXPAND_VARNAME_TOO_LONG	/* variable name too long */
} macro_expand_status;
extern macro_expand_status macro_expand(const char *, char *, char *, int);
extern void unquote(char *, char *);
extern void trim(char *);
extern char *get_line(FILE *, char *, char *, int);
extern int getword(char *, char *, char **, char **, char, int);
extern int get_retry(const char *);
extern int str_opt(struct mnttab *, char *, char **);
extern void dirinit(char *, char *, char *, int, char **, char ***);
extern void pr_msg(const char *, ...) __printflike(1, 2);
extern void trace_prt(int, char *, ...) __printflike(2, 3);
extern void free_action_list_fields(action_list *);

extern int nopt(struct mnttab *, char *, int *);
extern int set_versrange(rpcvers_t, rpcvers_t *, rpcvers_t *);
extern enum clnt_stat pingnfs(const char *, rpcvers_t *, rpcvers_t,
	ushort_t, const char *, const char *);

extern int self_check(char *);
extern int host_is_us(const char *, size_t);
extern void flush_host_name_cache(void);
extern int we_are_a_server(void);
extern int do_mount1(const autofs_pathname, const char *,
	const autofs_pathname, const autofs_opts, const autofs_pathname,
	boolean_t, boolean_t, fsid_t, uid_t, au_asid_t, fsid_t *,
	uint32_t *, byte_buffer *, mach_msg_type_number_t *);
extern int do_lookup1(const autofs_pathname, const char *,
	const autofs_pathname, const autofs_opts, boolean_t, uid_t, int *);
extern int do_unmount1(fsid_t, autofs_pathname, autofs_pathname,
	autofs_component, autofs_opts);
extern int do_readdir(autofs_pathname, off_t, uint32_t, off_t *,
	boolean_t *, byte_buffer *, mach_msg_type_number_t *);
extern int do_readsubdir(autofs_pathname, char *, autofs_pathname,
	autofs_opts, uint32_t, off_t, uint32_t, off_t *, boolean_t *,
	byte_buffer *, mach_msg_type_number_t *);
extern int nfsunmount(fsid_t *, struct mnttab *);
extern int loopbackmount(char *, char *, char *);
extern int mount_nfs(struct mapent *, char *, char *, boolean_t,
	fsid_t, au_asid_t, fsid_t *, uint32_t *);
extern int mount_autofs(const char *, struct mapent *, const char *, fsid_t,
	action_list **, const char *, const char *, const char *, fsid_t *,
	uint32_t *);
extern int mount_generic(char *, char *, char *, int, char *, boolean_t,
	boolean_t, fsid_t, uid_t, au_asid_t, fsid_t *, uint32_t *);
extern int get_triggered_mount_info(const char *, fsid_t, fsid_t *,
	uint32_t *);
extern enum clnt_stat nfs_cast(struct mapfs *, struct mapfs **, int);

extern bool_t hasrestrictopt(const char *);

extern void flush_caches(void);

#ifndef NO_RDDIR_CACHE
/*
 * readdir handling routines
 */
extern char *auto_rddir_malloc(unsigned);
extern char *auto_rddir_strdup(const char *);
extern struct dir_entry *btree_lookup(struct dir_entry *, const char *);
extern void btree_enter(struct dir_entry **, struct dir_entry *);
extern int add_dir_entry(const char *, const char *, const char *,
	struct dir_entry **, struct dir_entry **);
extern void *cache_cleanup(void *);
extern struct dir_entry *rddir_entry_lookup(const char *, const char *);
#endif /* NO_RDDIR_CACHE */

/*
 * generic interface to specific name service functions
 */
extern void ns_setup(char **, char ***);
extern int getmapent(const char *, const char *, struct mapline *, char **,
			char ***, bool_t *, bool_t);
extern int getmapkeys(char *, struct dir_entry **, int *, int *, char **,
			char ***);
extern int loadmaster_map(char *, char *, char **, char ***);
extern int loaddirect_map(char *, char *, char *, char **, char ***);

/*
 * Name service return statuses.
 */
#define	__NSW_SUCCESS	0	/* found the required data */
#define	__NSW_NOTFOUND	1	/* the naming service returned lookup failure */
#define	__NSW_UNAVAIL	2	/* could not call the naming service */

/*
 * these name service specific functions should not be
 * accessed directly, use the generic functions.
 */
extern void init_files(char **, char ***);
extern int getmapent_files(const char *, const char *, struct mapline *,
				char **, char ***, bool_t *, bool_t);
extern int loadmaster_files(char *, char *, char **, char ***);
extern int loaddirect_files(char *, char *, char *, char **, char ***);
extern int getmapkeys_files(char *, struct dir_entry **, int *, int *,
	char **, char ***);
extern int stack_op(int, char *, char **, char ***);

extern void init_od(char **, char ***);
extern int getmapent_od(const char *, const char *, struct mapline *, char **,
				char ***, bool_t *, bool_t);
extern int loadmaster_od(char *, char *, char **, char ***);
extern int loaddirect_od(char *, char *, char *, char **, char ***);
extern int getmapkeys_od(char *, struct dir_entry **, int *, int *,
	char **, char ***);
/*
 * Node for fstab entry.
 */
struct fstabnode {
	char	*fst_dir;	/* directory part of fs_spec */
	char	*fst_vfstype;	/* fs_vfstype */
	char	*fst_mntops;	/* fs_mntops plus fs_type */
	char	*fst_url;	/* URL from fs_mntops, if fs_vfstype is "url" */
	struct fstabnode *fst_next;
};

/*
 * Structure for a static map entry (non-"net") in fstab.
 */
struct staticmap {
	char	*dir;			/* name of the directory on which to mount */
	char	*vfstype;		/* fs_vfstype */
	char	*mntops;		/* fs_mntops plus fs_type */
	char	*host;			/* host from which to mount */
	char	*localpath;		/* full path, on the server, for that item */
	char	*spec;			/* item to mount */
	struct staticmap *next;		/* next entry in hash table bucket */
};

/*
 * Look up a particular host in the fstab map hash table and, if we find it,
 * run the callback routine on each entry in its fstab entry list.
 */
extern int fstab_process_host(const char *host,
    int (*callback)(struct fstabnode *, void *), void *callback_arg);

/*
 * Enumerate all the entries in the -fstab map.
 * This is used by a readdir on the -fstab map; those are likely to
 * be followed by stats on one or more of the entries in that map, so
 * we populate the fstab map cache and return values from that.
 */
extern int getfstabkeys(struct dir_entry **list, int *error, int *cache_time);

/*
 * Check whether we have any entries in the -fstab map.
 * This is used by automount to decide whether to mount that map
 * or not.
 */
extern int havefstabkeys(void);

/*
 * Load the -static direct map.
 */
extern int loaddirect_static(char *local_map, char *opts, char **stack,
    char ***stkptr);

/*
 * Find the -static map entry corresponding to a given mount point.
 */
extern struct staticmap *get_staticmap_entry(const char *dir);

/*
 * Indicate that we're done with a -static map entry returned by
 * get_staticmap_entry().
 */
extern void release_staticmap_entry(struct staticmap *static_ent);

/*
 * Purge the fstab cache; if scheduled is true, do so only if it's
 * stale, otherwise do it unconditionally.
 */
extern void clean_fstab_cache(int scheduled);

/*
 * end of name service specific functions
 */

/*
 * not defined in any header file
 */
extern int getnetmaskbynet(const struct in_addr, struct in_addr *);

/*
 * Hidden rpc functions
 */
extern int __nis_reset_state();
extern int __rpc_negotiate_uid(int);
extern int __rpc_get_local_uid(SVCXPRT *, uid_t *);

#ifdef __cplusplus
}
#endif

#endif	/* _AUTOMOUNT_H */
