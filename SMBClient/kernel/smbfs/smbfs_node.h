/*
 * Copyright (c) 2000-2001, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2023 Apple Inc. All rights reserved.
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

/*
 * OS X semantics expect that node id of 2 is the root of the share.
 * If File IDs are supported by the SMB 2/3 server, then we need to return 2
 * for the root vnode File ID . If some other item on the server has the 
 * File ID of 2, then we return the actual root File ID instead.
 * When vget is supported, then if they ask for File ID of 2, we return the 
 * root vnode.  If they ask for File ID that matches the actual root File ID,
 * then we return the vnode that has the File ID of 2 on the server.
 */
#define	SMBFS_ROOT_INO		2	/* just like in UFS */
#define	SMBFS_ROOT_PAR_INO	1	/* Maps to root vnode just like SMBFS_ROOT_INO */

/* Bits for smbnode.n_flag */
#define	NREFPARENT		0x00001	/* node holds parent from recycling */
#define	N_ISSTREAM		0x00002	/* Node is a stream */
#define	N_ISRSRCFRK		0x00004	/* Special stream node! */
#define	NISMAPPED		0x00008 /* This file has been mmapped */
#define NNEEDS_FLUSH	0x00010 /* Need to flush the file */
#define	NNEEDS_EOF_SET	0x00020	/* Need to set the eof, ignore the eof from the server */
#define	NATTRCHANGED	0x00040	/* Did we change the size of the file, clear cache on close */
#define	NDELETEONCLOSE	0x00080	/* We need to delete this item on close */
#define	NMARKEDFORDLETE	0x00100	/* This item will has been marked for deletion */
#define	NNEGNCENTRIES	0x00200	/* Directory has negative name cache entries */
#define NWINDOWSYMLNK	0x00400 /* This is a Conrad/Steve Window's symbolic links */
#define N_POLLNOTIFY	0x00800 /* Change notify is not support, poll */
#define NO_EXTENDEDOPEN 0x01000 /* The server doesn't support the extended open reply */
#define NHAS_POSIXMODES 0x02000 /* This node has a Windows NFS ACE that contains posix modes */
#define NNEEDS_UBC_INVALIDATE 0x04000 /* Need to do a UBC_INVALIDATE on this file */
#define NNEEDS_UBC_PUSHDIRTY  0x08000 /* Need to do a UBC_PUSH_DIRTY | UBC_INVALIDATE on this file */
#define N_DONT_COMPRESS      0x010000 /* This file is not allowed to be compressed */
#define N_WRITE_IMMEDIATELY  0x020000 /* Writes must also be sent to server immediately */

/* Bits for smbnode.n_flag_alloc */
/* protected by smbnode.n_flag_alloc_lock */
#define    NALLOC       0x00001    /* being created */
#define    NWALLOC      0x00002    /* awaiting creation */
#define    NTRANSIT     0x00004    /* being reclaimed */
#define    NWTRANSIT    0x00008    /* awaiting reclaim */

/*
 * Bits for smbnode.n_strategy_info.flags
 * protected by smbnode.n_strategy_info.mtx
 */
#define N_WAITING_FOR_IO         0x0001 /* Close is waiting for IO to finish */


#define UNKNOWNUID ((uid_t)99)
#define UNKNOWNGID ((gid_t)99)

struct smbfs_fctx;

enum smbfslocktype {SMBFS_SHARED_LOCK = 1, SMBFS_EXCLUSIVE_LOCK = 2, SMBFS_RECLAIM_LOCK = 3};

/* Used in reconnect for open files. Look at openState. */
#define kNeedRevoke	0x01
#define kNeedReopen	0x02
#define kInReopen   0x04  // Reopen in progress, don't update metadata

enum {
    kAnyMatch = 1,
    kCheckDenyOrLocks = 2,
    kExactMatch = 3,
    kPreflightOpen = 4
};

/* Flags for smbfs_smb_get_parent() */
enum {
    kShareLock = 1
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
	uint32_t    lck_pid;
    pid_t       p_pid;      /* pid that did this BRL */
	struct ByteRangeLockEntry *next;
};

/*
 * The sharedFID open/closes its current FID due to upgrades/downgrades which
 * is a problem for Byte Range Locks because a close releases any current BRLs.
 * Have to open and keep open yet another FID for the BRLs.
 *
 * Example
 * 1. Proc A opens foo with Read, takes out BRL1
 * 2. Proc B opens foo with Read/Write. sharedFID open foo for read/write and
 *    closes previous open that had Read. Have to keep BRL1 intact.
 */
struct fileRefEntryBRL {
    int32_t         refcnt;     /* set via vnop_ioctl(BRL) and vnop_advlock() */
    SMBFID          fid;        /* file handle */
    uint16_t        accessMode; /* access mode for this open */
    uint32_t        rights;     /* nt granted rights */
    struct ByteRangeLockEntry *lockList;
    struct smb2_durable_handle dur_handle;
};

/* Used for Open Deny */
struct fileRefEntry {
    int32_t         refcnt;     /* open file reference count (signed) */
    uint32_t        mmapped;    /* This entry has been mmaped */
    SMBFID          fid;        /* file handle */
    uint16_t        accessMode; /* original access mode requested for this open */
    uint32_t        mmapMode;   /* The mode we used when opening from mmap */
    uint32_t        rights;     /* nt granted rights */
    int32_t         openRWCnt;  /* number of rw opens */
    int32_t         openRCnt;   /* number of r opens */
    int32_t         openWCnt;   /* number of w opens */
    int32_t         isEXLOCK;   /* set if its an O_EXLOCK open */
    int32_t         isSHLOCK;   /* set if its an O_SHLOCK open */
    lck_mtx_t       BRL_lock;   /* byte range list lock */
    struct ByteRangeLockEntry *lockList;    /* Used ONLY by lockFID for BRLs */
    struct smb2_durable_handle dur_handle;
    
    /*
     * Used ONLY by sharedFID for byte range locks
     * [0] - Used for Read/Write FIDs
     * [1] - Used for Read FIDs
     * [2] - Used for Write FIDs
     * Never do a deferred close on these
     */
    struct fileRefEntryBRL lockEntries[3];
};

/* enum cache flags */
enum {
    kDirCacheComplete = 0x01,   /* Entire dir is cached */
    kDirCachePartial = 0x02,    /* Max allowable number of entries cached */
    kDirCacheDirty = 0x04       /* Needs Meta Data and/or Finder Info */
};

struct smb_enum_cache {
    u_int64_t       flags;
    u_int32_t       chg_cnt;         /* copy of dirchangecnt when dir cached */
    off_t           count;           /* dir enum cache number of entries */
    off_t           offset;          /* last cache offset */
    off_t           start_offset;    /* first entry cache offset */
    time_t			timer;           /* dir enum cache timer */
    struct cached_dir_entry	*list;   /* dir enum cache */
};

struct smb_dir_cookie {
    uint64_t        key;
    struct timespec last_used;
    off_t           resume_offset;
    uint64_t        resume_node_id;
    char            *resume_name_p;
};

#define kSMBDirCookieMaxCnt 25

struct smb_open_dir {
	uint32_t		refcnt;
	uint32_t		kq_refcnt;
	SMBFID          fid;			/* directory handle, SMB 1 fid in volatile */
    struct smbfs_fctx *fctx;        /* ff context */
    struct smbfs_fctx *rdir_fctx;   /* ff context for readdir */
	void			*nextEntry;		/* directory entry that didn't fit */
	int32_t			nextEntryLen;	/* size of directory entry that didn't fit */
	int32_t			nextEntryFlags; /* flags that the next entry was create with */
    off_t           offset;         /* last ff offset */
    off_t           rdir_offset;    /* last ff offset for readdir */
	uint32_t		needReopen;		/* Need to reopen the notification */
	uint32_t		needsUpdate;
    struct smb_enum_cache main_cache;           /* main dir cache */
    struct smb_enum_cache overflow_cache;       /* overflow dir cache */
	lck_mtx_t		enum_cache_list_lock;       /* dir enum list lock */
	u_int32_t       dirchangecnt;	/* changes each insert/delete. used by readdirattr */
    uint64_t        cookie_curr_key; /* Can never be zero */
    lck_mtx_t       cookie_lock;    /* smb_dir_cookie array lock */
    struct smb_dir_cookie cookies[kSMBDirCookieMaxCnt];
};

struct smb_open_file {
    int32_t         needClose;          /* we opened it in the read call */
    int32_t         openTotalWriteCnt;  /* nbr of w opens (shared and nonshared) */
    int32_t         clusterCloseError;  /* Error to be return on user close */
    uint32_t        openState;          /* Do we need to revoke or reopen the file */
    lck_mtx_t       openStateLock;      /* Locks the openState */
    lck_mtx_t       clusterWriteLock;   /* Used for cluster writes */
    struct fileRefEntry    sharedFID;
    struct fileRefEntry    lockFID;
    struct smbfs_flock     *smbflock;   /* Our flock structure, only on sharedFID */
    pid_t                  smbflock_pid; /* pid used to obtain the flock on sharedFID */
    int32_t         hasBRLs;            /* fsctl(Byte range locks) were used, thus non cacheable */
};

struct smbnode {
	lck_rw_t			n_rwlock;	
	void *				n_lastvop;	/* tracks last operation that locked the smbnode */
    bool                n_write_unsafe; /* vop is a read operation, ie we cannot pushdirty the UBC */
	void *				n_activation;
	uint32_t			n_lockState;	/* current lock state */
    uint32_t            n_flag;
    uint32_t            n_flag_alloc;
    lck_mtx_t           n_flag_alloc_lock;
    vnode_t             n_parent_vnode; /* could be invalid, only use with smbfs_smb_get_parent() */
    uint32_t            n_parent_vid;
	vnode_t				n_vnode;
	struct smbmount		*n_mount;
	time_t				attribute_cache_timer;	/* attributes (MetaData) cache time */
	struct timespec		n_last_meta_set_time; /* last time we set attributes (MetaData) */
	struct timespec		n_crtime;	/* create time */
	struct timespec		n_mtime;	/* modify time */
	struct timespec		n_atime;	/* last access time */
	struct timespec		n_chtime;	/* change time */
	struct timespec		n_sizetime;     /* last time we set size */
	struct timespec		n_rename_time;  /* last rename time */
	u_quad_t			n_size;         /* stream size */
	uint8_t				waitOnClusterWrite;
	u_quad_t			n_data_alloc;	/* stream allocation size */
	int					n_dosattr;
	uint32_t			n_flags_mask;	/* When doing unix extensions the va_flags mask */
	uid_t				n_uid;
	gid_t				n_gid;
	mode_t				n_mode;
	mode_t				create_va_mode;
	uid_t				n_nfs_uid;
	gid_t				n_nfs_gid;
	int					set_create_va_mode;
	time_t				finfo_cache_timer;	/* finder info cache timer, only used by the data node */
	uint8_t				finfo[FINDERINFOSIZE];	/* finder info , only used by the data node */
	time_t				rfrk_cache_timer;		/* resource stream size cache timer, only used by the data node */
	u_quad_t			rfrk_size;		/* resource stream size, only used by the data node */
	u_quad_t			rfrk_alloc_size;/* resource stream alloc size */
	lck_mtx_t			rfrkMetaLock;	/* Locks the resource size and resource cache timer */
	uint64_t			n_ino;
	uint64_t			n_nlinks;		/* Currently only supported when using the new UNIX Extensions */
    SInt32              n_child_refcnt; /* Each child node holds a refcnt */
	union {
		struct smb_open_dir	dir;
		struct smb_open_file file;
	} open_type;
	void				*acl_cache_data;
	time_t				acl_cache_timer;
	int					acl_error;
	size_t				acl_cache_len;
	lck_mtx_t			f_ACLCacheLock;     /* Locks the ACL Cache */
	lck_rw_t			n_name_rwlock;      /* Read/Write lock for n_name */
	lck_rw_t			n_parent_rwlock;    /* Read/Write lock for n_parent_vid */
	char				*n_name;	        /* node's file or directory name */
	size_t				n_nmlen;	        /* node's name length */
    size_t              n_name_allocsize;   /* n_name alloc size, required when freeing n_name */
	size_t				n_snmlen;	        /* if a stream then the legnth of the stream name */
	char				*n_sname;	        /* if a stream then the the name of the stream */
    size_t              n_sname_allocsize;  /* n_sname alloc size, required when freeing n_sname */
	LIST_ENTRY(smbnode)	n_hash;
	uint32_t			maxAccessRights;
	struct timespec		maxAccessRightChTime;	/* change time */
	uint32_t			n_reparse_tag;
	uint16_t			n_fstatus;				/* Does the node have any named streams */
	char				*n_symlink_target;
	size_t				n_symlink_target_len;
    size_t              n_symlink_target_allocsize; /* n_symlink_target alloc size, required when freeing n_symlink_target */
	time_t				n_symlink_cache_timer;
	struct timespec		n_last_write_time;
	uint64_t			n_lease_key_hi;			/* Used for Dir Lease or shared FID */
	uint64_t			n_lease_key_low;
	uint16_t			n_epoch;				/* lease epoch */

    struct timespec     n_last_close_mtime;     /* modify time on last close */
    u_quad_t            n_last_close_size;      /* data size on last close */

    struct smb_vnode_attr *n_hifi_attrs;        /* Cached hifi attributes from server */
    struct smb2_lease   n_lease;                /* lease for both dirs/files */

    struct {
        lck_mtx_t           mtx;
        uint16_t            count; /* how many strategy jobs are queued/being processed */
        uint16_t            flags;
    } n_strategy_info;
};

/* Directory items */
#define d_refcnt open_type.dir.refcnt
#define d_kqrefcnt open_type.dir.kq_refcnt
#define d_fctx open_type.dir.fctx
#define d_rdir_fctx open_type.dir.rdir_fctx
#define d_nextEntry open_type.dir.nextEntry
#define d_nextEntryFlags open_type.dir.nextEntryFlags
#define d_nextEntryLen open_type.dir.nextEntryLen
#define d_offset open_type.dir.offset
#define d_rdir_offset open_type.dir.rdir_offset
#define d_needReopen open_type.dir.needReopen
#define d_fid open_type.dir.fid
#define d_needsUpdate open_type.dir.needsUpdate
#define d_changecnt open_type.dir.dirchangecnt

#define d_main_cache open_type.dir.main_cache
#define d_overflow_cache open_type.dir.overflow_cache
#define d_enum_cache_list_lock open_type.dir.enum_cache_list_lock
#define d_cookie_lock open_type.dir.cookie_lock
#define d_cookies open_type.dir.cookies
#define d_cookie_cur_key open_type.dir.cookie_curr_key

/* File items */
#define f_sharedFID open_type.file.sharedFID
#define f_lockFID open_type.file.lockFID
#define f_sharedFID_refcnt open_type.file.sharedFID.refcnt
#define f_lockFID_refcnt open_type.file.lockFID.refcnt
#define f_sharedFID_mmapped open_type.file.sharedFID.mmapped
#define f_lockFID_mmapped open_type.file.lockFID.mmapped
#define f_sharedFID_p_pid open_type.file.sharedFID.p_pid
#define f_lockFID_p_pid open_type.file.lockFID.p_pid
#define f_sharedFID_fid open_type.file.sharedFID.fid
#define f_lockFID_fid open_type.file.lockFID.fid
#define f_sharedFID_accessMode open_type.file.sharedFID.accessMode
#define f_lockFID_accessMode open_type.file.lockFID.accessMode
#define f_sharedFID_mmapMode open_type.file.sharedFID.mmapMode
#define f_lockFID_mmapMode open_type.file.lockFID.mmapMode
#define f_sharedFID_rights open_type.file.sharedFID.rights
#define f_lockFID_rights open_type.file.lockFID.rights
#define f_sharedFID_openRWCnt open_type.file.sharedFID.openRWCnt
#define f_lockFID_openRWCnt open_type.file.lockFID.openRWCnt
#define f_sharedFID_openRCnt open_type.file.sharedFID.openRCnt
#define f_lockFID_openRCnt open_type.file.lockFID.openRCnt
#define f_sharedFID_openWCnt open_type.file.sharedFID.openWCnt
#define f_lockFID_openWCnt open_type.file.lockFID.openWCnt
#define f_sharedFID_isEXLOCK open_type.file.sharedFID.isEXLOCK
#define f_lockFID_isEXLOCK open_type.file.lockFID.isEXLOCK
#define f_sharedFID_isSHLOCK open_type.file.sharedFID.isSHLOCK
#define f_lockFID_isSHLOCK open_type.file.lockFID.isSHLOCK
//#define f_sharedFID_lockList open_type.file.sharedFID.lockList /* do not use */
#define f_lockFID_lockList open_type.file.lockFID.lockList
#define f_sharedFID_BRL_lock open_type.file.sharedFID.BRL_lock
#define f_lockFID_BRL_lock open_type.file.lockFID.BRL_lock
#define f_sharedFID_dur_handle open_type.file.sharedFID.dur_handle
#define f_lockFID_dur_handle open_type.file.lockFID.dur_handle
#define f_sharedFID_lockEntries open_type.file.sharedFID.lockEntries
//#define f_lockFID_lockEntries open_type.file.lockFID.lockEntries  /* do not use */

#define f_needClose open_type.file.needClose
#define f_openTotalWCnt open_type.file.openTotalWriteCnt
#define f_smbflock open_type.file.smbflock
#define f_smbflock_pid open_type.file.smbflock_pid
#define f_openState open_type.file.openState
#define f_openStateLock open_type.file.openStateLock
#define f_clusterWriteLock open_type.file.clusterWriteLock
#define f_openDenyListLock open_type.file.openDenyListLock
#define f_clusterCloseError open_type.file.clusterCloseError
#define f_hasBRLs open_type.file.hasBRLs

/* Strategy information items */
#define n_strategy_mtx n_strategy_info.mtx
#define n_strategy_count n_strategy_info.count
#define n_strategy_flags n_strategy_info.flags

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

#define SMB_DIR_CACHE_TIME(ts, np, attrtimeo, max_to, min_to) { \
    nanotime(&ts);	\
    attrtimeo = (ts.tv_sec - np->n_mtime.tv_sec) / 10; \
    if (attrtimeo < min_to)	\
        attrtimeo = min_to;	\
    else if (attrtimeo > max_to) \
        attrtimeo = max_to; \
    nanouptime(&ts);	\
}

#define VTOSMB(vp)	((struct smbnode *)vnode_fsnode(vp))
#define SMBTOV(np)	((vnode_t )(np)->n_vnode)

/* smbfs_nget flags */
typedef enum _SMBFS_NGET_FLAGS
{
    SMBFS_NGET_CREATE_VNODE = 0x0001,
    SMBFS_NGET_LOOKUP_ONLY = 0x0002,
    SMBFS_NGET_NO_CACHE_UPDATE = 0x0004
} _SMBFS_NGET_FLAGS;

/* smb_get_uid_gid_mode flags */
typedef enum _SMBFS_GET_UGM_FLAGS
{
    SMBFS_GET_UGM_IS_DIR = 0x0001,
    SMBFS_GET_UGM_REMOVE_POSIX_MODES = 0x0002
} _SMBFS_GET_UGM_FLAGS;

extern lck_attr_t *smbfs_lock_attr;
extern lck_grp_t *smbfs_mutex_group;
extern lck_grp_t *smbfs_rwlock_group;

struct rename_lock_list {
	struct smbnode *np;
	UInt32 is_locked;
	struct rename_lock_list *next;
};

struct smbfattr;

/* Global Lease Hash functions */
struct smb_lease {
	LIST_ENTRY(smb_lease) lease_hash;
    uint64_t lease_key_hi;
    uint64_t lease_key_low;
	vnode_t vnode;
	uint32_t vid;
};

/* smbfs_add_update_lease flags */
typedef enum _SMBFS_ADD_UPDATE_LEASE_FLAGS
{
    SMBFS_LEASE_ADD =       0x00000001, /* Can only set ONE of add/remove/update! */
    SMBFS_LEASE_REMOVE =    0x00000002,
    SMBFS_LEASE_UPDATE =    0x00000004,
    SMBFS_IN_RECONNECT =    0x00000008
} _SMBFS_ADD_UPDATE_LEASE_FLAGS;

#define smb_ubc_getsize(v) (vnode_vtype(v) == VREG ? ubc_getsize(v) : (off_t)0)

int smbnode_lock(struct smbnode *np, enum smbfslocktype);
int smbnode_trylock(struct smbnode *np, enum smbfslocktype locktype);
int smbnode_lockpair(struct smbnode *np1, struct smbnode *np2, enum smbfslocktype);
void smbnode_unlock(struct smbnode *np);
void smbnode_unlockpair(struct smbnode *np1, struct smbnode *np2);
uint64_t smbfs_hash(struct smb_share *share, uint64_t ino,
                    const char *name, size_t nmlen);
void smb_vhashrem (struct smbnode *np);
void smb_vhashadd(struct smbnode *np, uint64_t hashval);
int smbfs_is_ancestor(vnode_t potential_ancestor, struct smbnode *np);
int smbfs_nget(struct smb_share *share, struct mount *mp,
               vnode_t dvp, const char *name, size_t nmlen,
               struct smbfattr *fap, vnode_t *vpp,
               uint32_t cnflags, uint32_t flags,
               vfs_context_t context);
vnode_t smbfs_find_vgetstrm(struct smbmount *smp, struct smbnode *np, const char *sname, 
							size_t maxfilenamelen);
int smbfs_vgetstrm(struct smb_share *share, struct smbmount *smp, vnode_t vp, 
				   vnode_t *svpp, struct smbfattr *fap, const char *sname);
int smb_get_rsrcfrk_size(struct smb_share *share, vnode_t vp, vfs_context_t context);
vnode_t smb_update_rsrc_and_getparent(vnode_t vp, int setsize);
int smb_check_posix_access(vfs_context_t context, struct smbnode * np, 
						   mode_t rq_mode);
Boolean node_isimmutable(struct smb_share *share, vnode_t vp, struct smbfattr *fap);
void smbfs_attr_cacheenter(struct smb_share *share, vnode_t vp, struct smbfattr *fap, 
						   int UpdateResourceParent, vfs_context_t context);
int smbfs_attr_cachelookup(struct smb_share *share, vnode_t vp, struct vnode_attr *va, 
						   vfs_context_t context, int useCacheDataOnly);
void smbfs_attr_touchdir(struct smbnode *dnp, int fatShare);

int smbfsIsCacheable(vnode_t vp);
void smbfs_setsize(vnode_t vp, off_t size);
int smbfs_update_size(struct smbnode *np, struct timespec * reqtime,
                      u_quad_t new_size, struct smbfattr *fap);
int smbfs_update_name_par(struct smb_share *share, vnode_t dvp, vnode_t vp,
                          struct timespec *reqtime,
                          const char *new_name, size_t name_len);

void smb_get_uid_gid_mode(struct smb_share *share, struct smbmount *smp,
                          struct smbfattr *fap, uint32_t flags,
                          uid_t *uid, gid_t *gid, mode_t *mode,
                          uint32_t max_access);

/* smbfs_io.c prototypes */
int smbfs_readvdir(vnode_t vp, uio_t uio, int flags, int32_t *numdirent,
                   vfs_context_t context);
int smbfs_0extend(struct smb_share *share, SMBFID fid, u_quad_t from,
                  u_quad_t to, int ioflag,
                  uint32_t *allow_compressionp, vfs_context_t context);
int smbfs_doread(struct smb_share *share, off_t endOfFile, uio_t uiop,
                 SMBFID fid, uint32_t allow_compression,
                 vfs_context_t context);
int smbfs_dowrite(struct smb_share *share, off_t endOfFile, uio_t uiop,
				  SMBFID fid, int ioflag,
                  uint32_t *allow_compressionp, vfs_context_t context);
void smbfs_uio_update(uio_t uio, user_size_t length);
int32_t smbfs_IObusy(struct smbmount *smp);
void smbfs_CloseChildren(struct smb_share *share,
                         struct smbnode *parent,
                         u_int32_t need_lock,
						 struct rename_lock_list **rename_child_listp,
                         vfs_context_t context);


#pragma mark - Reconnect Prototypes
int smb2fs_reconnect_dur_handle(struct smb_share *share, vnode_t vp,
                                struct fileRefEntry *fileEntry, struct fileRefEntryBRL *fileBRLEntry);
int smb2fs_reconnect(struct smbmount *smp);
int smbfs_reconnect(struct smbmount *smp);


#pragma mark - Durable Handles and Leasing Prototypes
int smbnode_dur_handle_lock(struct smb2_durable_handle *dur_handlep, void *last_op);
int smbnode_dur_handle_unlock(struct smb2_durable_handle *dur_handlep);
int smbnode_lease_lock(struct smb2_lease *leasep, void *last_op);
int smbnode_lease_unlock(struct smb2_lease *leasep);

void CloseDeferredFileRefs(vnode_t vp, const char *reason, uint32_t check_time, vfs_context_t context);

int smb2_dur_handle_init(struct smb_share *share, uint64_t flags,
                         struct smb2_durable_handle *dur_handlep,
                         int initialize);
void smb2_dur_handle_free(struct smb2_durable_handle *dur_handlep);
int smb2_lease_init(struct smb_share *share, struct smbnode *dnp, struct smbnode *np,
                    uint64_t flags, struct smb2_lease *leasep,
                    int initialize);
void smb2_lease_free(struct smb2_lease *leasep);

int smbfs_add_update_lease(struct smb_share *share, vnode_t vp, struct smb2_lease *in_leasep,
                           uint32_t flags, uint32_t need_lock,
                           const char *reason);
int16_t smbfs_get_epoch_delta(uint16_t server_epoch, uint16_t file_epoch);
uint32_t smbfs_get_req_lease_state(uint32_t access_rights);
int smbfs_handle_lease_break(struct lease_rq *lease_rqp, vfs_context_t context);
int smbfs_handle_dir_lease_break(struct lease_rq *lease_rqp);
void smbfs_lease_hash_add(vnode_t vp, uint64_t lease_key_hi, uint64_t lease_key_low,
                          uint32_t need_lock);
struct smb_lease *smbfs_lease_hash_get(uint64_t lease_key_hi, uint64_t lease_key_low);
void smbfs_lease_hash_remove(vnode_t vp, struct smb_lease *in_leasep,
                            uint64_t lease_key_hi, uint64_t lease_key_low);


#pragma mark - lockFID, sharedFID, BRL LockEntries Prototypes
void AddRemoveByteRangeLockEntry(struct fileRefEntry *fileEntry, struct fileRefEntryBRL *fileBRLEntry,
                                 int64_t offset, int64_t length, int8_t unLock, uint32_t lck_pid, vfs_context_t context);
int CheckByteRangeLockEntry(struct fileRefEntry *fileEntry, struct fileRefEntryBRL *fileBRLEntry,
                            int64_t offset, int64_t length, int exact_match, pid_t *lock_holder);

int FindByteRangeLockEntry(struct fileRefEntry *fileEntry, struct fileRefEntryBRL *fileBRLEntry,
                           int64_t offset, int64_t length, vfs_context_t context);
int32_t FindFileRef(vnode_t vp, proc_t p, uint16_t accessMode,
                    int64_t offset, int64_t length, SMBFID *fid);
int smbfs_clear_lockEntries(vnode_t vp, struct fileRefEntryBRL *fileBRLEntry);
struct fileRefEntryBRL * smbfs_find_lockEntry(vnode_t vp, uint16_t accessMode);
int smbfs_free_locks_on_close(struct smb_share *share, vnode_t vp,
                              int openMode, vfs_context_t context);
int smbfs_get_lockEntry(struct smb_share *share, vnode_t vp,
                        uint16_t accessMode,
                        struct fileRefEntryBRL **ret_entry,
                        vfs_context_t context);


#endif /* _FS_SMBFS_NODE_H_ */
