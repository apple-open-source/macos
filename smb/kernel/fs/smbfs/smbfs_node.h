/*
 * Copyright (c) 2000-2001, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by Boris Popov.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#ifndef _FS_SMBFS_NODE_H_
#define _FS_SMBFS_NODE_H_

#define	SMBFS_ROOT_INO		2	/* just like in UFS */

/* Bits for smbnode.n_flag */
#define	N_NOEXISTS		0x00001 /* File has been deleted */
#define	NREFPARENT		0x00002	/* node holds parent from recycling */
#define	N_ISSTREAM		0x00004	/* Node is a stream */
#define	N_ISRSRCFRK		0x00008	/* Special stream node! */
#define	NISMAPPED		0x00010 /* This file has been mmapped */
#define	NFLUSHWIRE		0x00020 /* Need to send a flush across the wire */
#define	NATTRCHANGED	0x00040	/* use SMBFS_ATTR_CACHE_REMOVE at close */
#define	NALLOC			0x00080	/* being created */
#define	NWALLOC			0x00100	/* awaiting creation */
#define	NTRANSIT		0x00200	/* being reclaimed */
#define	NWTRANSIT		0x00400	/* awaiting reclaim */
#define	NDELETEONCLOSE	0x00800	/* We need to delete this item on close */
#define	NMARKEDFORDLETE	0x01000	/* This item will has been marked for deletion */
#define	NNEGNCENTRIES	0x02000	/* Directory has negative name cache entries */
#define NWINDOWSYMLNK	0x04000 /* This is a Conrad/Steve Window's symbolic links */
#define N_POLLNOTIFY	0x08000 /* Change notify is not support, poll */

struct smbfs_fctx;

enum smbfslocktype {SMBFS_SHARED_LOCK = 1, SMBFS_EXCLUSIVE_LOCK = 2, SMBFS_RECLAIM_LOCK = 3};

/* Used in reconnect for open files. Look at openState. */
#define kNeedRevoke	0x01
#define kNeedReopen	0x02

enum {
    kAnyMatch = 1,
    kCheckDenyOrLocks = 2,
    kExactMatch = 3
};

/* Carbon Read/Write and Deny bits */
enum {
	kAccessRead = 0x01,
	kAccessWrite = 0x02,
	kDenyRead = 0x10,
	kDenyWrite = 0x20,
	kOpenMask = 0x03,
	kDenyMask = 0x30,
	kAccessMask = 0x33
};

struct ByteRangeLockEntry {
	int64_t		offset;
	int64_t		length;
	u_int32_t	lck_pid;
	struct ByteRangeLockEntry *next;
};

/* Used for Open Deny */
struct fileRefEntry {
	u_int32_t		refcnt;		/* open file reference count */
	u_int32_t		mmapped;	/* This entry has been mmaped */
	pid_t			p_pid;		/* proc that did the open */
	u_int16_t		fid;		/* file handle */
	u_int16_t		accessMode;	/* access mode for this open */
	u_int32_t		rights;		/* nt granted rights */
	struct proc		*proc;		/* used in cluster IO strategy function */
	struct ByteRangeLockEntry *lockList;
	struct fileRefEntry	*next;
};

struct smb_open_dir {
	u_int32_t		refcnt;
	u_int32_t		kq_refcnt;
	u_int16_t		fid;			/* directory handle */
	struct smbfs_fctx *fctx;		/* ff context */
	void			*nextEntry;		/* directory entry that didn't fit */
	int32_t			nextEntryLen;	/* size of directory entry that didn't fit */
	int32_t			nextEntryFlags; /* flags that the next entry was create with */
	off_t			offset;			/* last ff offset */
	u_int32_t		needReopen;		/* Need to reopen the notification */
	u_int32_t		needsUpdate;
};

struct smb_open_file {
	u_int32_t		refcnt;		/* open file reference count */
	u_int16_t		fid;		/* file handle */
	u_int32_t		rights;		/* nt granted rights */
	u_int16_t		accessMode;	/* access mode used when opening  */
	u_int32_t		mmapMode;	/* The mode we used when opening from mmap */
	int32_t			needClose;	/* we opened it in the read call */
	int32_t			openRWCnt;	/* number of rw opens */
	int32_t			openRCnt;	/* number of r opens */
	int32_t			openWCnt;	/* number of w opens */
	u_int32_t		openState;	/* Do we need to revoke or reopen the file */
	lck_mtx_t		openStateLock;	/* Locks the openState */
	lck_mtx_t		openDenyListLock;	/* Locks the open deny list */
	struct fileRefEntry	*openDenyList;
	struct smbfs_flock	*smbflock;	/*  Our flock structure */
};

struct smbnode {
	lck_rw_t			n_rwlock;	
	void *				n_lastvop;	/* tracks last operation that locked the smbnode */
	void *				n_activation;
	u_int32_t			n_lockState;	/* current lock state */
	u_int32_t			n_flag;
	struct smbnode		*n_parent;
	vnode_t				n_vnode;
	struct smbmount		*n_mount;
	time_t				attribute_cache_timer;	/* attributes cache time */
	struct timespec		n_crtime;	/* create time */
	struct timespec		n_mtime;	/* modify time */
	struct timespec		n_atime;	/* last access time */
	struct timespec		n_chtime;	/* change time */
	struct timespec		n_sizetime;
	u_quad_t			n_size;		/*  stream size */
	u_quad_t			n_data_alloc;	/* stream allocation size */
	int					n_dosattr;
	u_int32_t			n_flags_mask;	/* When doing unix extensions the va_flags mask */
	uid_t				n_uid;
	gid_t				n_gid;
	mode_t				n_mode;
	mode_t				create_va_mode;
	int					set_create_va_mode;
	time_t				finfo_cache;	/* finder info cache timer, only used by the data node */
	u_int8_t			finfo[FINDERINFOSIZE];	/* finder info , only used by the data node */
	time_t				rfrk_cache_timer;		/* resource stream size cache timer, only used by the data node */
	u_quad_t			rfrk_size;		/* resource stream size, only used by the data node */
	lck_mtx_t			rfrkMetaLock;	/* Locks the resource size and resource cache timer */
	u_int64_t			n_ino;
	u_int64_t			n_nlinks;		/* Currently only supported when using the new UNIX Extensions */
	union {
		struct smb_open_dir	dir;
		struct smb_open_file	file;
	}open_type;
	void				*acl_cache_data;
	struct timespec		acl_cache_timer;
	int					acl_error;
	size_t				acl_cache_len;
	lck_mtx_t			f_ACLCacheLock;	/* Locks the ACL Cache */
	lck_rw_t			n_name_rwlock;	/* ReadWrite lock for name changes */ 
	u_char				*n_name;	/* node's file or directory name */
	size_t				n_nmlen;	/* node's name length */
	size_t				n_snmlen;	/* if a stream then the legnth of the stream name */
	u_char				*n_sname;	/* if a stream then the the name of the stream */
	LIST_ENTRY(smbnode)	n_hash;
};

/* Directory items */
#define d_refcnt open_type.dir.refcnt
#define d_kqrefcnt open_type.dir.kq_refcnt
#define d_fctx open_type.dir.fctx
#define d_nextEntry open_type.dir.nextEntry
#define d_nextEntryFlags open_type.dir.nextEntryFlags
#define d_nextEntryLen open_type.dir.nextEntryLen
#define d_offset open_type.dir.offset
#define d_needReopen open_type.dir.needReopen
#define d_fid open_type.dir.fid
#define d_needsUpdate open_type.dir.needsUpdate

/* File items */
#define f_refcnt open_type.file.refcnt
#define f_fid open_type.file.fid
#define f_rights open_type.file.rights
#define f_accessMode open_type.file.accessMode
#define f_mmapMode open_type.file.mmapMode
#define f_needClose open_type.file.needClose
#define f_openRWCnt open_type.file.openRWCnt
#define f_openRCnt open_type.file.openRCnt
#define f_openWCnt open_type.file.openWCnt
#define f_openDenyList open_type.file.openDenyList
#define f_smbflock open_type.file.smbflock
#define f_openState open_type.file.openState
#define f_openStateLock open_type.file.openStateLock
#define f_openDenyListLock open_type.file.openDenyListLock

/* Attribute cache timeouts in seconds */
#define	SMB_MINATTRTIMO 2
#define	SMB_MAXATTRTIMO 30

/*
 * Determine attrtimeo. It will be something between SMB_MINATTRTIMO and
 * SMB_MAXATTRTIMO where recently modified files have a short timeout
 * and files that haven't been modified in a long time have a long
 * timeout. This is the same algorithm used by NFS.
 */
#define SMB_CACHE_TIME(ts, np, attrtimeo) { \
	nanotime(&ts);	\
	attrtimeo = (ts.tv_sec - np->n_mtime.tv_sec) / 10; \
	if (attrtimeo < SMB_MINATTRTIMO)	\
		attrtimeo = SMB_MINATTRTIMO;	\
	else if (attrtimeo > SMB_MAXATTRTIMO) \
	attrtimeo = SMB_MAXATTRTIMO; \
	nanouptime(&ts);	\
}

/* ACL cache timeouts in seconds */
#define	SMB_ACL_MINTIMO 1
#define	SMB_ACL_MAXTIMO 30

/*
 * If we are negative caching ( got an error back) then set our cache time
 * to a longer value than normal caching. Remember that the vfs will help 
 * us with cache, but not in the negative case.
 */
#define SET_ACL_CACHE_TIME(np) { \
	struct timespec waittime;	\
								\
	nanouptime(&np->acl_cache_timer);	\
	if (np->acl_error) { \
		waittime.tv_sec = SMB_ACL_MAXTIMO;	\
		waittime.tv_nsec = 0;	\
	}	\
	else {	\
		waittime.tv_sec = SMB_ACL_MINTIMO;	\
		waittime.tv_nsec = 0;	\
	}\
	timespecadd(&np->acl_cache_timer, &waittime);	\
}


#define VTOSMB(vp)	((struct smbnode *)vnode_fsnode(vp))
#define SMBTOV(np)	((vnode_t )(np)->n_vnode)

extern lck_attr_t *smbfs_lock_attr;
extern lck_grp_t *smbfs_mutex_group;
extern lck_grp_t *smbfs_rwlock_group;

struct smbfattr;

int smbnode_lock(struct smbnode *, enum smbfslocktype);
int smbnode_lockpair(struct smbnode *, struct smbnode *, enum smbfslocktype);
void smbnode_unlock(struct smbnode *);
void smbnode_unlockpair(struct smbnode *, struct smbnode *);
void smbnode_unlockfour(struct smbnode *, struct smbnode *, struct smbnode *, struct smbnode *);
void smb_vhashrem (struct smbnode *);
void smb_vhashadd(struct smbnode *, u_int32_t /*hashval*/);
int smbfs_nget(struct mount *, vnode_t, const char */*name*/, size_t /*nmlen*/, struct smbfattr *, vnode_t *, 
			   uint32_t /*cnflags*/, vfs_context_t);
vnode_t smbfs_find_vgetstrm(struct smbmount *, struct smbnode *, const char *);
int smbfs_vgetstrm(struct smbmount *, vnode_t /*vp*/, vnode_t */*svpp*/, struct smbfattr *, const char */*sname*/);
u_int32_t smbfs_hash(const u_char */*name*/, size_t /*nmlen*/);
int smbfs_readvdir(vnode_t, uio_t, vfs_context_t, int /*flags*/, int32_t */*numdirent*/);
int smbfs_doread(vnode_t, uio_t, vfs_context_t, u_int16_t /*fid*/);
int smbfs_dowrite(vnode_t, uio_t, u_int16_t /*fid*/, vfs_context_t, int /*ioflag*/, int /*timo*/);
int smb_get_rsrcfrk_size(vnode_t, vfs_context_t);
vnode_t smb_update_rsrc_and_getparent(vnode_t, int /*setsize*/);
void smb_clear_parent_cache_timer(vnode_t);
int smb_check_posix_access(vfs_context_t, struct smbnode *, mode_t);
void smbfs_attr_cacheenter(vnode_t, struct smbfattr *, int /*UpdateResourceParent*/, vfs_context_t );
int  smbfs_attr_cachelookup(vnode_t, struct vnode_attr *, vfs_context_t, int /* useCacheDataOnly */);
void smbfs_attr_touchdir(struct smb_share *, struct smbnode *);
u_char	*smbfs_name_alloc(const u_char */*name*/, size_t /*nmlen*/);
void smbfs_name_free(const u_char */*name*/);
void smbfs_setsize(vnode_t, off_t);
int smbfs_update_size(struct smbnode *, struct timespec *, u_quad_t );
int smbfs_fsync(vnode_t, int /*waitfor*/, int /*ubc_flags*/, vfs_context_t);
void smbfs_addFileRef(vnode_t, struct proc *, u_int16_t /*accessMode*/, u_int32_t /*rights*/, u_int16_t /*fid*/, 
					  struct fileRefEntry **/*fndEntry*/);
int32_t smbfs_findFileRef(vnode_t, pid_t, u_int16_t  /*accessMode*/, int32_t /*flags*/,  int64_t /*offset*/,  
						  int64_t /*length*/,  struct fileRefEntry **, u_int16_t */*fid*/);
int32_t smbfs_findMappedFileRef(vnode_t, struct fileRefEntry **, u_int16_t */*fid*/);
int32_t smbfs_findFileEntryByFID(vnode_t, u_int16_t /*fid*/, struct fileRefEntry **);
void smbfs_removeFileRef(vnode_t, struct fileRefEntry *);
int smbfs_FindLockEntry(struct fileRefEntry *, int64_t /*offset*/, int64_t /*length*/, u_int32_t /*lck_pid*/);
void smbfs_AddRemoveLockEntry (struct fileRefEntry *, int64_t /*offset*/, int64_t /*length*/, int8_t /*unLock*/, 
							   u_int32_t /*lck_pid*/);


#define smb_ubc_getsize(v) (vnode_vtype(v) == VREG ? ubc_getsize(v) : (off_t)0)

#endif /* _FS_SMBFS_NODE_H_ */
