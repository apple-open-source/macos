/*
 * Copyright (c) 1999 The Santa Cruz Operation, Inc.. All Rights Reserved. 
 *                                                                         
 *        THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF THE               
 *                   SANTA CRUZ OPERATION INC.                             
 *                                                                         
 *   The copyright notice above does not evidence any actual or intended   
 *   publication of such source code.                                      
 */

#ifndef _MEMFS_NODE_H	/* wrapper symbol for kernel use */
#define _MEMFS_NODE_H	/* subject to change without notice */

#ident	"@(#)unixsrc:usr/src/common/uts/fs/memfs/memfs_mnode.h /main/uw7_nj/2"
#ident	"$Header: $"

#if defined(__cplusplus)
extern "C" {
#endif

/*
 * MP: memfs - Memory Based File System using swap backing store
 */

#ifdef _KERNEL_HEADERS

#include <fs/fski.h> /* REQUIRED */
#include <fs/vfs.h> /* REQUIRED */
#include <fs/vnode.h> /* REQUIRED */
#include <mem/swap.h> /* REQUIRED */
#include <util/ksynch.h> /* REQUIRED */
#include <util/param.h> /* REQUIRED */
#include <util/types.h> /* REQUIRED */

#elif defined(_KERNEL) || defined(_KMEMUSER)

#include <sys/fski.h> /* REQUIRED */
#include <sys/ksynch.h> /* REQUIRED */
#include <sys/param.h> /* REQUIRED */
#include <sys/types.h> /* REQUIRED */
#include <sys/vfs.h> /* REQUIRED */
#include <sys/vnode.h> /* REQUIRED */
#include <sys/swap.h> /* REQUIRED */	/* modified by VAA */

#endif /* _KERNEL_HEADERS */

/*
 * mnode: the vnode carrier of the memfs file system
 *
 * Since memfs is a memory based file system, a memfs vnode (mnode) remains
 * ACTIVE (memory resident) as long as its corresponding file exists.
 *
 * Excepting the case of fixed size files, each mnode has a backing store
 * in the form of an N-ary tree, where N = MEMFS_BNODE_SIZE. Each node
 * consists of an array of MEMFS_BNODE_SIZE of memfs_bs_t(s). Non-leaf nodes
 * (level > 0) contain pointers to lower level nodes. Leaf nodes (level == 0)
 * contain swaploc_t structures, directly describing swap backing store. All
 * of the nodes are kmem_alloc(ed) and in memory. A single node is
 * pre-allocated at mnode creation time. This node is kmem_alloc(ed) together
 * with the mnode itself. All other backing store tree nodes are allocated
 * independently.
 *
 * For a fixed size memfs file (indicated by flag MEMFS_FIXEDSIZE),
 * the backing store tree consists of a single node containing
 * btopr(mno_size) swaploc_t(s).
 * 
 * All backing store tree nodes have the property that once allocated, they
 * never move, and are never deleted until the mnodes are inactivated (i.e.
 * for memfs, this means that the file is removed). Thus, the tree can grow
 * horizontally and vertically, but the pieces of the tree do not move once
 * allocated. As a consequence, once the root of the tree is found, the tree
 * can be traversed without holding any locks.
 *
 * Some mnodes are un-named (indicated by flag MEMFS_UNNAMED). These mnodes
 * contain a smaller vnode structure (the vnode_unnamed_t), plus have a number
 * of other fields deleted.
 *
 * A swaploc_t(s) structure in the backing store tree is mutexed by a pseudo
 * lock that we call the MEM_T_LOCK. This lock is held if any of
 * the folowing conditions are in force:
 *
 *	(1) The corresponding page of the file is held PAGE_IS_WRLOCKED, or
 *
 *	(2) The corresponding page of the file is held pageout locked
 *	    (i.e. the p_pageout bit is set), or
 *
 *	(3) The corresponding page of the file is held PAGE_VPGLOCKED and
 *	    p_invalid == 0 && p_pageout == 0, or
 *
 *	(4) The corresponding page of the file does not exist, and memfs
 *	    is enforcing this fact.
 *
 * Note that these conditions are indeed exclusive of each other.
 */

/*
 * memfs backing store descriptor
 *
 *	Each backing store tree node is composed of an array of
 *	MEMFS_BNODE_SIZE memfs_bs_t(s).
 */
typedef union memfs_bs {
	union memfs_bs		*mbs_ptr;	/* ptr to next lower level */
	swaploc_t		mbs_swaploc;	/* backing store location */
} memfs_bs_t;

/*
 * Basic unit of memfs disk klustering:
 *
 *	memfs tries to kluster together MEMFS_KLUSTER_NUM pages for I/O.
 *	Since memfs allocates swap storage when it is cleaning pages, and
 *	since it never allocates backing store for uninstantiated pages,
 *	the actual klusters of backing storage allocated on the swap device
 *	may be less than this.
 *
 *	Note that in-memory backing store trees are also allocated in units
 *	of MEMFS_BNODE_SIZE memfs_bs_t(s) and that MEMFS_KLUSTER_NUM is
 *	equal to MEMFS_BNODE_SIZE. This equality is used by the
 *	getpage/putpage code in order to efficiently determine actual
 *	klustering.
 */

#define MEMFS_LVL_SHIFT		4
#define MEMFS_BNODE_SIZE	(1 << MEMFS_LVL_SHIFT)
#define MEMFS_LVL_MASK		(MEMFS_BNODE_SIZE - 1)
#define MEMFS_SHIFT(level)	((level) * MEMFS_LVL_SHIFT)
#define MEMFS_BNODE_BYTES	(MEMFS_BNODE_SIZE * sizeof(memfs_bs_t))
#define MEMFS_LEVELS		((NBITPOFF - PAGESHIFT + MEMFS_LVL_SHIFT - 1) \
				 / MEMFS_LVL_SHIFT)
#define MEMFS_MAXLEVEL		(MEMFS_LEVELS - 1)
#define MEMFS_KLUSTER_NUM	MEMFS_BNODE_SIZE
#define MEMFS_KLUSTER_SIZE	(ptob(MEMFS_KLUSTER_NUM))
#define MEMFS_KLUSTER_OFF	(MEMFS_KLUSTER_SIZE - 1)
#define MEMFS_KLUSTER_MASK	(~MEMFS_KLUSTER_OFF)

/*
 *
 *	+---------------------------------------------------------------+
 *	|	0	|  L2	|  L1	|  L0	|	PAGESHIFT	|
 *	+---------------------------------------------------------------+
 *
 *	The diagram above shows how a file offset is decomposed into
 *	fields used for traversing the backing store tree.  In this
 *	case, the backing store tree has 3 levels (mno_maxlevel == 2).
 *	The L0, L1, and L2 fields each contain MEMFS_LVL_SHIFT bits. The L2
 *	bits form an index into the root of the tree. The resulting value
 *	(mno_bsp[L2]) points at a level 1 node. L1 is used similarly. L0
 *	indexes into the level 0 node, which contains an actual backing
 *	store location [swaploc_t(s)]. Thus, the backing store location
 *	is given by:
 *
 *		mp->mno_bsp[L2].mbs_bsp[L1].mbs_swaploc[L0]
 */

#ifdef _MEMFS_HIST

/*
 * Logging facility for enhanced memfs debugging.
 */

/*
 * History Entry in an mnode.
 */
typedef struct memfs_hist_record {
	off_t		mhr_offset;	/* offset */
	swaploc_t	mhr_swaploc;	/* swap location */
	char		*mhr_service;	/* service name */
	uint_t		mhr_count;	/* saved v_count */
	uint_t		mhr_softcnt;	/* saved v_softcnt */
	int		mhr_line;	/* line number */
	char		*mhr_file;	/* file name */
	lwp_t		*mhr_lwp;	/* calling LWP */
	ulong_t		mhr_stamp;	/* time stamp */
} memfs_hist_record_t;

#define MEMFS_HIST_SIZE	128

typedef struct memfs_hist {
	int			mli_cursor;		/* next avail rec */
	memfs_hist_record_t	mli_rec[MEMFS_HIST_SIZE]; /* log of records */
} memfs_hist_t;

#define MEMFS_LOG(mp, offset, service)	\
	memfs_log(mp, offset, service, __LINE__, __FILE__)

#define MEMFS_LOG_INIT(mp)	{				\
	struct_zero(&(mp)->mno_hist, sizeof(memfs_hist_t));	\
	MEMFS_LOG(mp, -1, "MEMFS_INIT");			\
}

extern void	memfs_log(struct mnode *, off_t, char *, int, char *);

#else /* !_MEMFS_HIST */

#define MEMFS_LOG(mp, offset, service)	((void)0)
#define MEMFS_LOG_INIT(mp)		/* nothing to initialize */

#endif /* _MEMFS_HIST */


/*
 * The fields of the mnode are mutexed as follows:
 *
 *	The following are mutexed by the memfs_list_lck:
 *
 *		mno_nextp, mno_lastp, mno_flags, mno_nlink
 *
 *	The following are mutexed by both mno_rwlock and mno_realloclck
 *	(either can be held to read, mno_realloclck must be held and
 *	mno_rwlock must be held in WRITEr mode to write):
 *
 *		mno_bsnp, mno_maxlevel
 *
 *	The following is mutexed by mno_realloclck:
 *
 *		mno_bsize
 *
 *	The following is mutexed by mno_rwlock;
 *
 *		mno_size
 *
 *	The following are are mutexed by mno_mutex:
 *
 *		mno_mode, mno_uid, mno_gid, mno_atime
 *		mno_mtime, mno_ctime, mno_mapcnt, mno_rdev
 */
typedef struct mnode_header {
	struct mnode_header	*mnh_nextp;	/* next mnode list member */
	struct mnode_header	*mnh_lastp;	/* prev mnode list member */
	uint_t			mnh_flags;	/* mnode flags */
} mnode_header_t;

typedef struct mnode_top {
	mnode_header_t	mnt_hdr;		/* mnode list linkages */
	size_t		mnt_size;		/* file length */
	rwsleep_t	mnt_rwlock;		/* lock for vop_rwlock */

	/*
	 * Backing Store Tree information.
	 *
	 * The first backing store tree node at level 0 is kmem_alloc(ed)
	 * together with the mnode. The other levels are kmem_alloc(ed)
	 * independently. For a fixed size file, the first level 0 node
	 * is sized to exactly provide for the entire file.
	 */
	size_t		mnt_bsize;		/* tree coverage in bytes */
	memfs_bs_t	*mnt_bsnp;		/* ptr to level 0 */
	int		mnt_maxlevel;		/* ... tree depth */
	fspin_t		mnt_realloclck;		/* mutexes reallocation */
#ifdef _MEMFS_HIST
	memfs_hist_t	mnt_hist;		/* history information */
#endif
} mnode_top_t;

typedef struct mnode {
	mnode_top_t	mno_top;
	int d1[2];				/* dummies, added by VAA */
	vnode_t		mno_vnode;		/* the associated vnode */
	struct mfs_frgopt *mno_fgmt;
	uint_t		mno_nlink;		/* link count */
	short		mno_flag;
	fspin_t		mno_mutex;		/* mutex for fields below */
	mode_t		mno_mode;		/* file mode and type */
	uid_t		mno_uid;		/* owner */
	gid_t		mno_gid;		/* group */
	struct timeval	mno_atime;		/* last access time */
	struct timeval	mno_mtime;		/* last modification time */
	struct timeval	mno_ctime;		/* last "inode change" time */
	uint_t		mno_nodeid;
	uint_t		mno_fsid;
	uint_t		mno_gen;
	ulong		mno_vcode;
	union {
		struct memfs_dirent	*un_dir;	/* pointer to directory list */
		char 		*un_symlink;	/* pointer to symlink */
	} un_mnode;
	long   		mno_mapcnt;       	/* number of page mappings */
	dev_t		mno_rdev;		/* for block/char specials */
} mnode_t;

#define mno_dir          un_mnode.un_dir
#define mno_symlink      un_mnode.un_symlink

typedef struct mnode_unnamed {
	mnode_top_t	mnn_top;
	vnode_unnamed_t	mnn_vnode;
} mnode_unnamed_t;

#define mno_nextp		mno_top.mnt_hdr.mnh_nextp
#define mno_lastp		mno_top.mnt_hdr.mnh_lastp
#define mno_flags		mno_top.mnt_hdr.mnh_flags
#define mno_size		mno_top.mnt_size
#define mno_rwlock		mno_top.mnt_rwlock
#define mno_bsnp		mno_top.mnt_bsnp
#define mno_realloclck		mno_top.mnt_realloclck
#define mno_bsize		mno_top.mnt_bsize
#define mno_maxlevel		mno_top.mnt_maxlevel
#define mno_hdr			mno_top.mnt_hdr
#define mno_hist		mno_top.mnt_hist

/*
 * mno_flag
 */
#define	TUPD		0x001		/* file modified */
#define	TACC		0x002		/* file accessed */
#define	TCHG		0x004		/* inode changed */

/* modes */
#define IFMT            0170000         /* type of file */
#define IFIFO           0010000         /* named pipe (fifo) */
#define IFCHR           0020000         /* character special */
#define IFDIR           0040000         /* directory */
#define IFNAM           0050000         /* obsolete XENIX file */
#define IFBLK           0060000         /* block special */
#define IFREG           0100000         /* regular */
#define IFLNK           0120000         /* symbolic link */
#define IFSOCK          0140000         /* socket */

#define ISUID           04000           /* set user id on execution */
#define ISGID           02000           /* set group id on execution */
#define ISVTX           01000           /* save swapped text even after use */
#define IREAD           0400            /* read, write, execute permissions */
#define IWRITE          0200
#define IEXEC           0100

/*
 * memfs directories are made up of a linked list of memfs_dirent entries
 * hanging off directory memfs nodes.  File names are not fixed length,
 * but are null terminated.
 */
struct memfs_dirent {
	mnode_t			*td_mnode;	/* mnode for this file */
	struct memfs_dirent	*td_next;	/* next directory entry */
	u_int 			td_offset;	/* "offset" of dir entry */
	ushort_t		td_namelen;	/* name length */
	char			td_name[1];	/* variable length */
						/* not null terminated */
						/* max size is MAXNAMELEN-1 */
};

/*
 * Name length does not include the null terminator.
 */
#define MEMFS_DIRENT_SIZE(namelen) \
			(offsetof(struct memfs_dirent, td_name) + (namelen))

/*
 * tfid overlays the fid structure (for VFS_VGET)
 */
struct tfid {
	u_short	tfid_len;
	ino_t	tfid_ino;
	long	tfid_gen;
};

/*
 * void
 * MNODE_INIT_COMMON(mnode_t *mp, size_t size)
 *	Common portion of mnode initialization for named and
 *	unnamed mnodes.
 *
 * Calling/Exit State:
 *	mp is privately held.
 */
#define MNODE_INIT_COMMON(mp, size) {					\
	FSPIN_INIT(&(mp)->mno_realloclck);				\
        RWSLEEP_INIT(&(mp)->mno_rwlock, (uchar_t) 0,			\
		     &memfs_rw_lkinfo, KM_SLEEP);			\
	(mp)->mno_size = (size);					\
	vop_attach(MNODE_TO_VP(mp), &memfs_vnodeops);			\
	(MNODE_TO_VP(mp))->v_data = mp;					\
	MEMFS_LOG_INIT(mp);						\
}

/*
 * void
 * MNODE_INIT(mnode_t *mp, vattr_t vap)
 *	Initialize a named mnode.
 *
 * Calling/Exit State:
 *	mp is privately held.
 */
#define MNODE_INIT(mp, vap) {					\
	FSPIN_INIT(&(mp)->mno_mutex);					\
	(mp)->mno_mode = MAKEIMODE((vap)->va_type, (vap)->va_mode);	\
	(mp)->mno_flag = 0;						\
	(mp)->mno_uid = (vap)->va_uid;					\
	(mp)->mno_gid = (vap)->va_gid;					\
	MNODE_INIT_COMMON(mp, 0);					\
}

/*
 * void
 * MNODE_INIT_UNNAMED(mnode_t *mp, uint_t flags, ulong_t size)
 *	Initialize an unnamed mnode.
 *
 * Calling/Exit State:
 *	mp is privately held.
 */
#define MNODE_INIT_UNNAMED(mp, flags, size) {				\
	(mp)->mno_flags = (flags) | MEMFS_UNNAMED;			\
	VN_INIT_UNNAMED(MNODE_TO_VP(mp), NULL, VUNNAMED, 0,		\
		 VSWAPBACK | VNOSYNC, KM_SLEEP);			\
	MNODE_INIT_COMMON(mp, size);					\
}

/*
 * void
 * MNODE_DEINIT_COMMON(mnode_t *mp)
 *	Common portion of mnode de-initialization for named and
 *	unnamed mnodes.
 *
 * Calling/Exit State:
 *	mp is privately held.
 */
#define MNODE_DEINIT_COMMON(mp) {				\
	RWSLEEP_DEINIT(&(mp)->mno_rwlock);			\
}

/*
 * void
 * MNODE_DEINIT_NAMED(mnode_t *mp)
 *	Named specific portion of mnode de-initialization.
 *
 * Calling/Exit State:
 *	mp is privately held.
 */
#define MNODE_DEINIT_NAMED(mp) {				\
	VN_DEINIT(MNODE_TO_VP(mp));				\
}

/*
 * void
 * MNODE_DEINIT_UNNAMED(mnode_t *mp)
 *	Named specific portion of mnode de-initialization.
 *
 * Calling/Exit State:
 *	mp is privately held.
 */
#define MNODE_DEINIT_UNNAMED(mp) {				\
	VN_DEINIT_UNNAMED(MNODE_TO_VP(mp));			\
}

#define MNODE_RELE(mp)	{				\
	VN_RELE(MNODE_TO_VP(mp));			\
}

/*
 * Macros to convert between mnode, mode header, and vnode pointers.
 */
#define MNODE_TO_VP(mp)		(&(mp)->mno_vnode)
#define VP_TO_MNODE(vp)		((mnode_t *)((vp)->v_data))
#define MNODE_TO_MNODEH(mp)	(&(mp)->mno_hdr)
#define MNODEH_TO_MNODE(mhp)	((mnode_t *)mhp)

/*
 * void
 * MNODE_LIST_INSERT(mnode_header_t *mhp, mnode_header_t *base_mhp)
 *	Insert mnode ``mhp'' into an mnode list following ``base_mhp''.
 *
 * Calling/Exit State:
 *	None.
 */
#define MNODE_LIST_INSERT(mhp, base_mhp) {			\
	mnode_header_t *next_mhp = (base_mhp)->mnh_nextp;	\
								\
	(mhp)->mnh_nextp = next_mhp;				\
	(mhp)->mnh_lastp = (base_mhp);				\
	(base_mhp)->mnh_nextp = (mhp);				\
	next_mhp->mnh_lastp = (mhp);				\
}

/*
 * void
 * MNODE_LIST_INSERT_PREV(mnode_header_t *mhp, mnode_header_t *base_mhp)
 *	Insert mnode ``mhp'' into an mnode list following ``base_mhp''.
 *
 * Calling/Exit State:
 *	None.
 */
#define MNODE_LIST_INSERT_PREV(mhp, base_mhp) {			\
	mnode_header_t *last_mhp = (base_mhp)->mnh_lastp;	\
								\
	(mhp)->mnh_nextp = (base_mhp);				\
	(mhp)->mnh_lastp = last_mhp;				\
	last_mhp->mnh_nextp = (mhp);				\
	(base_mhp)->mnh_lastp = (mhp);				\
}

/*
 * void
 * MNODE_LIST_DELETE(mnode_header_t *mhp, mnode_header_t *base_mhp)
 *	Delete mnode ``mhp'' from a list.
 *
 * Calling/Exit State:
 *	None.
 */
#define MNODE_LIST_DELETE(mhp) {				\
	mnode_header_t *next_mhp = (mhp)->mnh_nextp;		\
	mnode_header_t *last_mhp = (mhp)->mnh_lastp;		\
								\
	last_mhp->mnh_nextp = next_mhp;				\
	next_mhp->mnh_lastp = last_mhp;				\
}

#define MNODE_ACCESSED(mp) {					\
	timestruc_t timenow;					\
	GET_HRESTIME(&timenow);					\
	FSPIN_LOCK(&(mp)->mno_mutex);				\
	(mp)->mno_atime.tv_sec = timenow.tv_sec;		\
	(mp)->mno_atime.tv_usec = timenow.tv_nsec/1000;		\
	FSPIN_UNLOCK(&(mp)->mno_mutex);				\
}

#define MNODE_CHANGED(mp) {					\
	timestruc_t timenow;					\
	GET_HRESTIME(&timenow);					\
	FSPIN_LOCK(&(mp)->mno_mutex);				\
	(mp)->mno_ctime.tv_sec = timenow.tv_sec;		\
	(mp)->mno_ctime.tv_usec = timenow.tv_nsec/1000;		\
	FSPIN_UNLOCK(&(mp)->mno_mutex);				\
}

#define MNODE_MODIFIED(mp) {					\
	timestruc_t timenow;					\
	GET_HRESTIME(&timenow);					\
	FSPIN_LOCK(&(mp)->mno_mutex);				\
	(mp)->mno_mtime.tv_sec = timenow.tv_sec;		\
	(mp)->mno_mtime.tv_usec = timenow.tv_nsec/1000;		\
	FSPIN_UNLOCK(&(mp)->mno_mutex);				\
}

#define MNODE_MODIFIED_CHANGED(mp) {					\
	timestruc_t timenow;					\
	GET_HRESTIME(&timenow);					\
	FSPIN_LOCK(&(mp)->mno_mutex);				\
	(mp)->mno_mtime.tv_sec = timenow.tv_sec;		\
	(mp)->mno_mtime.tv_usec = timenow.tv_nsec/1000;		\
	(mp)->mno_ctime.tv_sec = timenow.tv_sec;		\
	(mp)->mno_ctime.tv_usec = timenow.tv_nsec/1000;		\
	FSPIN_UNLOCK(&(mp)->mno_mutex);				\
}

extern size_t memfs_maxbsize[];
extern memfs_bs_t       memfs_empty_node[];
extern mnode_header_t	mnode_anchor;
extern mnode_header_t	mnode_ianchor;
extern int		memfs_ndelabort;
extern sv_t		memfs_sv;
lock_t		memfs_sb_lck;
extern lock_t memfs_list_lck;
extern sleep_t memfs_renamelock;
extern struct seg       memfs_seg;


#if defined(__cplusplus)
	}
#endif

#endif /* _MEMFS_NODE_H */
