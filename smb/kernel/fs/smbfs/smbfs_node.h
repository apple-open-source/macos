/*
 * Copyright (c) 2000-2001, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2007 Apple Inc. All rights reserved.
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
#define	NGOTIDS			0x00004
#define	N_ISSTREAM		0x00008	/* Node is a stream */
#define	N_ISRSRCFRK		0x00010	/* Special stream node! */
#define	NISMAPPED		0x00020
#define	NFLUSHWIRE		0x00040
#define	NATTRCHANGED	0x00080	/* use SMBFS_ATTR_CACHE_REMOVE at close */
#define	NALLOC			0x00100	/* being created */
#define	NWALLOC			0x00200	/* awaiting creation */
#define	NTRANSIT		0x00400	/* being reclaimed */
#define	NWTRANSIT		0x00800	/* awaiting reclaim */
#define	NDELETEONCLOSE	0x01000	/* We need to delete this item on close */
#define	NMARKEDFORDLETE	0x02000	/* This item will has been marked for deletion */
#define	NNEGNCENTRIES	0x04000	/* Directory has negative name cache entries */
#define NWINDOWSYMLNK	0x08000 /* This is a Conrad/Steve Window's symbolic links */

struct smbfs_fctx;

enum smbfslocktype {SMBFS_SHARED_LOCK = 1, SMBFS_EXCLUSIVE_LOCK = 2, SMBFS_RECLAIM_LOCK = 3};

/* Used in reconnect of open fiels. Look at openState.  */
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
	int 			refcnt;
	struct smbfs_fctx *	dirseq;		/* ff context */
	long			dirofs;	/* last ff offset */
};

struct smb_open_file {
	u_int32_t		refcnt;		/* open file reference count */
	u_int16_t		fid;		/* file handle */
	u_int32_t		rights;		/* nt granted rights */
	u_int16_t		accessMode;	/* access mode used when opening  */
	int				mmapMode;	/* The mode we used when opening from mmap */
	int				needClose;	/* we opened it in the read call */
	int				openRWCnt;	/* number of rw opens */
	int				openRCnt;	/* number of r opens */
	int				openWCnt;	/* number of w opens */
	int				openState;	/* Do we need to revoke or reopen the file */
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
	long				n_ino;
	u_int64_t			n_nlinks;		/* Currently only supported when using the new UNIX Extensions */
	union {
		struct smb_open_dir	dir;
		struct smb_open_file	file;
	}open_type;
	void				*acl_cache_data;
	struct timespec		acl_cache_timer;
	int					acl_error;
	u_char				*n_name;	/* node's file or directory name */
	u_char				n_nmlen;	/* node's name length */
	u_char				n_snmlen;	/* if a stream then the legnth of the stream name */
	u_char				*n_sname;	/* if a stream then the the name of the stream */
	LIST_ENTRY(smbnode)	n_hash;
};

/* Directory items */
#define n_dirrefs open_type.dir.refcnt
#define n_dirseq open_type.dir.dirseq
#define n_dirofs open_type.dir.dirofs
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
	nanotime(&np->acl_cache_timer);	\
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

int smbnode_lock(struct smbnode *np, enum smbfslocktype locktype);
int smbnode_lockpair(struct smbnode *np1, struct smbnode *np2, enum smbfslocktype locktype);
void smbnode_unlock(struct smbnode *np);
void smbnode_unlockpair(struct smbnode *np1, struct smbnode *np2);
void smbnode_unlockfour(struct smbnode *np1, struct smbnode *np2, struct smbnode *np3, struct smbnode *np4);

void smb_vhashrem (struct smbnode *np);
void smb_vhashadd(struct smbnode *np, u_long hashval);
int smbfs_nget(struct mount *mp, vnode_t dvp, const char *name, int nmlen, struct smbfattr *fap, vnode_t *vpp,
			   u_long cnflags, vfs_context_t vfsctx);
vnode_t smbfs_find_vgetstrm(struct smbmount *smp, struct smbnode *np, const char *sname);
int smbfs_vgetstrm(struct smbmount *smp, vnode_t vp, vnode_t *svpp, struct smbfattr *fap, const char *sname);
u_int32_t smbfs_hash(const u_char *name, int nmlen);

int smbfs_readvdir(vnode_t vp, uio_t uio, vfs_context_t vfsctx);
int smbfs_doread(vnode_t vp, uio_t uiop, struct smb_cred *scred, u_int16_t fid);
int  smbfs_dowrite(vnode_t vp, uio_t uiop, u_int16_t fid, vfs_context_t vfsctx, int ioflag, int timo);
int smb_get_rsrcfrk_size(vnode_t vp, struct smb_cred *scredp);
vnode_t smb_update_rsrc_and_getparent(vnode_t vp, int setsize);
void smb_clear_parent_cache_timer(vnode_t parent_vp);
void smbfs_attr_cacheenter(vnode_t vp, struct smbfattr *fap, int UpdateResourceParent);
int  smbfs_attr_cachelookup(vnode_t, struct vnode_attr *, struct smb_cred *);
void smbfs_attr_touchdir(struct smbnode *dnp);
u_char	*smbfs_name_alloc(const u_char *name, int nmlen);
void	smbfs_name_free(const u_char *name);
void	smbfs_setsize(vnode_t , off_t);
int smbfs_fsync(vnode_t vp, int waitfor, int ubc_flags, vfs_context_t vfsctx);

void smbfs_addFileRef(vnode_t vp, struct proc *p, u_int16_t accessMode, u_int32_t rights, 
			u_int16_t fid, struct fileRefEntry **fndEntry);
int32_t smbfs_findFileRef(vnode_t vp, pid_t pid, u_int16_t accessMode, int32_t flags,  int64_t offset, 
			int64_t length,  struct fileRefEntry **fndEntry, u_int16_t *fid);
int32_t smbfs_findMappedFileRef(vnode_t vp, struct fileRefEntry **fndEntry, u_int16_t *fid);
int32_t smbfs_findFileEntryByFID(vnode_t vp, u_int16_t fid, struct fileRefEntry **fndEntry);
void smbfs_removeFileRef(vnode_t vp, struct fileRefEntry *inEntry);
void smbfs_AddRemoveLockEntry (struct fileRefEntry *fndEntry, int64_t offset, int64_t length, int8_t unLock, u_int32_t lck_pid);


#define smb_ubc_getsize(v) (vnode_vtype(v) == VREG ? ubc_getsize(v) : (off_t)0)

#endif /* _FS_SMBFS_NODE_H_ */
