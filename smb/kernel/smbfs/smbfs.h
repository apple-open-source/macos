/*
 * Copyright (c) 2000-2001, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2014 Apple Inc. All rights reserved.
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

#ifndef _SMBFS_SMBFS_H_
#define _SMBFS_SMBFS_H_

#include <sys/kdebug.h>
#include <netinet/in.h>
#include <netsmb/netbios.h>

#define SMBFS_VERMAJ	3
#define SMBFS_VERMIN	0100
#define SMBFS_VERSION	(SMBFS_VERMAJ*100000 + SMBFS_VERMIN)
#define	SMBFS_VFSNAME	"smbfs"
#define SMBFS_LANMAN	"SMBFS 3.0.1"	/* Needs to match SMBFS_VERSION */
#define SMBFS_NATIVEOS	"Mac OS X 10.10"	/* Needs to match current OS version major number only */
#define SMBFS_SLASH_TONAME "/Volumes/0x2f"

#define	SMBFS_MAXPATHCOMP	256	/* maximum number of path components */

#include <sys/mount.h>

#define SMB_STREAMS_ON	".com.apple.smb.streams.on"
#define SMB_STREAMS_OFF	".com.apple.smb.streams.off"

#define SMB_MAX_UNIQUE_ID	128 + MAXPATHLEN + (int32_t)sizeof(struct sockaddr_storage)  /* Share name, path, sockaddr */


#define kGuestAccountName	"GUEST"
#define kGuestPassword		""


struct ByteRangeLockPB {
	int64_t	offset;		/* offset to first byte to lock */
	int64_t	length;		/* nbr of bytes to lock */
	uint64_t	retRangeStart;	/* nbr of first byte locked if successful */
	uint8_t	unLockFlag;	/* 1 = unlock, 0 = lock */
	uint8_t	startEndFlag;	/* 1 = rel to end of fork, 0 = rel to start */
};

struct ByteRangeLockPB2 {
	int64_t	offset;		/* offset to first byte to lock */
	int64_t	length;		/* nbr of bytes to lock */
	uint64_t	retRangeStart;	/* nbr of first byte locked if successful */
	uint8_t	unLockFlag;	/* 1 = unlock, 0 = lock */
	uint8_t	startEndFlag;	/* 1 = rel to end of fork, 0 = rel to start */
	int32_t		fd;
};

struct gssdProxyPB
{
	uint32_t	mechtype;		
	uint32_t	alignPad1;
	uint8_t	*intoken;
	uint32_t	intokenLen;		
	uint32_t	uid;		
	uint8_t	*svc_namestr;
	uint64_t	verifier;		// inout
	uint32_t	context;		// inout
	uint32_t	cred_handle;		// inout
	uint8_t	*outtoken;		// out
	uint32_t	outtokenLen;		// inout
	uint32_t	major_stat;		// out
	uint32_t	minor_stat;		// out
};

#ifdef KERNEL

struct user_gssdProxyPB
{
	uint32_t	mechtype;		
	user_addr_t	intoken;
	uint32_t	intokenLen;		
	uint32_t	uid;		
	user_addr_t	svc_namestr;
	uint64_t	verifier;		// inout
	uint32_t	context;		// inout
	uint32_t	cred_handle;		// inout
	user_addr_t	outtoken;		// out
	uint32_t	outtokenLen;		// inout
	uint32_t	major_stat;		// out
	uint32_t	minor_stat;		// out
};
#endif

enum  {
	kConnectedByUser = 0,
	kConnectedByGuest = 1,
	kConnectedByAnonymous = 2,
	kConnectedByKerberos = 4
};

/* 
 * We just want to know the type of access used to mount the volume and the 
 * user name used. If not set then the unique_id needs to be set.
 */
#define SMBFS_GET_ACCESS_INFO	1

struct UniqueSMBShareID {
	int32_t		connection_type;
	int32_t		flags;
	int32_t		error;
	int32_t		unique_id_len;
	unsigned char	unique_id[SMB_MAX_UNIQUE_ID] __attribute((aligned(8)));	/* A set of bytes that uniquely identifies this volume */
	char		user[SMB_MAXUSERNAMELEN + 1] __attribute((aligned(8)));
};

#define smbfsByteRangeLock2FSCTL		_IOWR('z', 23, struct ByteRangeLockPB2)
#define smbfsByteRangeLock2FSCTL_BASECMD	IOCBASECMD(smbfsByteRangeLock2FSCTL)

#define smbfsByteRangeLockFSCTL			_IOWR('z', 17, struct ByteRangeLockPB)
#define smbfsByteRangeLockFSCTL_BASECMD		IOCBASECMD(smbfsByteRangeLockFSCTL)

#define smbfsUniqueShareIDFSCTL			_IOWR('z', 19, struct UniqueSMBShareID)
#define smbfsUniqueShareIDFSCTL_BASECMD		IOCBASECMD(smbfsUniqueShareIDFSCTL)

#define smbfsGetVCSockaddrFSCTL			_IOR('z', 20, struct sockaddr_storage)
#define smbfsGetVCSockaddrFSCTL_BASECMD		IOCBASECMD(smbfsGetVCSockaddrFSCTL)

/* Layout of the mount control block for an smb file system. */
struct smb_mount_args {
	int32_t		version;
	int32_t		dev;
	int32_t		altflags;
	int32_t		KernelLogLevel;
	uid_t		uid;
	gid_t 		gid;
	mode_t 		file_mode;
	mode_t 		dir_mode;
	uint32_t	path_len;
	int32_t		unique_id_len;
	char		path[MAXPATHLEN] __attribute((aligned(8))); /* The starting path they want used for the mount */
	char		url_fromname[MAXPATHLEN] __attribute((aligned(8))); /* The from name is the url used to mount the volume. */
	unsigned char	unique_id[SMB_MAX_UNIQUE_ID] __attribute((aligned(8))); /* A set of bytes that uniquely identifies this volume */
	char		volume_name[MAXPATHLEN] __attribute((aligned(8))); /* The starting path they want used for the mount */
	uint64_t	ioc_reserved __attribute((aligned(8))); /* Force correct size always */
	int32_t		max_resp_timeout;
};

#define SMBFS_SYSCTL_REMOUNT 1
#define SMBFS_SYSCTL_REMOUNT_INFO 2
#define SMBFS_SYSCTL_GET_SERVER_SHARE 3

#define REMOUNT_INFO_VERSION	1

struct smb_remount_info {
	uint32_t	version;
	uint32_t	mntAuthFlags;	
	uid_t		mntOwner;
	uid_t		mntGroup;
	uint32_t	mntDeadTimer;
	uint32_t	mntClientPrincipalNameType;
	char		mntClientPrincipalName[MAXPATHLEN] __attribute((aligned(8)));
	char		mntURL[MAXPATHLEN] __attribute((aligned(8)));
};

#ifdef KERNEL

/* The items the mount point keeps in memory */
struct smbfs_args {
	int32_t		altflags;
	uid_t		uid;
	gid_t 		gid;
	guid_t		uuid;		/* The UUID of the user that mounted the volume */
	mode_t 		file_mode;
	mode_t 		dir_mode;
	size_t		path_len;	/* Must be less than MAXPATHLEN and does not count the null byte */
	char		*path;		/* The starting path they want to use with this mounted volume */
	int32_t		unique_id_len;
	unsigned char	*unique_id;	/* A set of bytes that uniquely identifies this volume */
	char		*volume_name;
};

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_SMBFSMNT);
#endif

struct smbnode;
struct smb_share;
struct u_cred;
struct vnop_ioctl_args;
struct buf;
struct smbfs_notify_change;

/*
 * SM_MAX_STATFSTIME is the maximum time to cache statfs data. Since this
 * should be a fast call on the server, the time the data cached is short.
 * That lets the cache handle bursts of statfs() requests without generating
 * lots of network traffic.
 */
#define SM_MAX_STATFSTIME 2

/* Mask values for smbmount structure sm_status field */
#define SM_STATUS_STATFS	0x00000001 /* statfs is in progress */
#define SM_STATUS_STATFS_WANTED	0x00000002 /* statfs wakeup is wanted */
#define SM_STATUS_DOWN		0x00000004 /* this mount is not responding */
#define SM_STATUS_DEAD		0x00000010 /* connection gone - unmount this */
#define SM_STATUS_REMOUNT	0x00000020 /* remount inprogress */
#define SM_STATUS_UPDATED	0x00000040 /* Remounted with new server/share */
/* 
 * Some servers do not support all the info levels on Trans2 calls. Most systems do not care if you
 * use a file descriptor or a path name to do a get or set info call. Seems NetApp requires an file descriptor when 
 * setting the time on a file. 
 */
#define MNT_REQUIRES_FILEID_FOR_TIME		0x0001	/* Setting time requires a file descriptor */
#define MNT_MAPS_NETWORK_LOCAL_USER		0x0002 /* Map  Network User <==> Local User */
#define MNT_IS_SFM_VOLUME				0x0004	/* We mount a Service for Macintosh Volume */
#define MNT_SUPPORTS_REPARSE_SYMLINKS		0x0008

/*
 * Bit definitions for the svrmsg_pending field in the smbmount structure
 */
#define SVRMSG_RCVD_GOING_DOWN	0x0000000000000001
#define SVRMSG_RCVD_SHUTDOWN_CANCEL	0x0000000000000002

struct smbmount {
	uint64_t		ntwrk_uid;
	uint64_t		ntwrk_gid;
	uint32_t		ntwrk_cnt_gid;
	uint64_t		*ntwrk_gids;
	ntsid_t			*ntwrk_sids;
	uint32_t		ntwrk_sids_cnt;
	struct smbfs_args	sm_args;
	struct mount * 		sm_mp;
	vnode_t			sm_rvp;
	uint64_t		sm_root_ino;
	struct smb_share * 	sm_share;
	lck_rw_t		sm_rw_sharelock;
	int			sm_flags;
	lck_mtx_t		*sm_hashlock;
	LIST_HEAD(smbnode_hashhead, smbnode) *sm_hash;
	u_long			sm_hashlen;
	uint32_t		sm_status; /* status bits for this mount */
	time_t			sm_statfstime; /* sm_statfsbuf cache time */
	lck_mtx_t		sm_statfslock; /* sm_statsbuf lock */
	struct vfsstatfs	sm_statfsbuf; /* cached statfs data */
	lck_mtx_t		sm_reclaim_lock; /* mount reclaim lock */
	void			*notify_thread;	/* pointer to the notify thread structure */
	int32_t			tooManyNotifies;
	lck_mtx_t		sm_svrmsg_lock;		/* protects svrmsg fields */
	uint64_t		sm_svrmsg_pending;	/* svrmsg replies pending (bits defined above) */
	uint32_t		sm_svrmsg_shutdown_delay;  /* valid when SVRMSG_GOING_DOWN is set */
};

#define VFSTOSMBFS(mp)		((struct smbmount *)(vfs_fsprivate(mp)))
#define	VTOSMBFS(vp)		(VFSTOSMBFS(vnode_mount(vp)))

#define SMB_SYMMAGICLEN (4+1) /* includes a '\n' seperator */
extern char smb_symmagic[];
#define SMB_SYMLENLEN (4+1) /* includes a '\n' seperator */
#define SMB_SYMMD5LEN (32+1) /* includes a '\n' seperator */
#define SMB_SYMHDRLEN (SMB_SYMMAGICLEN + SMB_SYMLENLEN + SMB_SYMMD5LEN)
#define SMB_SYMLEN (SMB_SYMHDRLEN + MAXPATHLEN)

#define CON_FILENAME(nn, nl) \
	(((nl) >= 3) && \
	(((*(nn) == 'c') || (*(nn) == 'C')) &&	\
	((*((nn)+1) == 'o') || (*((nn)+1) == 'O')) && \
	((*((nn)+2) == 'n') || (*((nn)+2) == 'N'))))

/*
 * internal versions of VOPs
 */
int smbfs_close(struct smb_share *share, vnode_t vp, int openMode, 
		vfs_context_t context);
int smbfs_open(struct smb_share *share, vnode_t vp, int mode, 
	       vfs_context_t context);

int smbfs_update_cache(struct smb_share *share, vnode_t vp, 
		       struct vnode_attr *vap, vfs_context_t context);
int smbfs_fsync(struct smb_share *share, vnode_t vp, int waitfor, int ubc_flags, 
		vfs_context_t context);

/*
 * Notify change routines
 */
void smbfs_notify_change_create_thread(struct smbmount *smp);
void smbfs_notify_change_destroy_thread(struct smbmount *smp);
int smbfs_start_change_notify(struct smb_share *share, struct smbnode *np, 
			      vfs_context_t context, int *releaseLock);
int smbfs_start_svrmsg_notify(struct smbmount *smp);
int smbfs_stop_change_notify(struct smb_share *share, struct smbnode *np,
			     int forceClose, vfs_context_t context, int *releaseLock);
int smbfs_stop_svrmsg_notify(struct smbmount *smp);
void smbfs_restart_change_notify(struct smb_share *share, struct smbnode *np, 
				 vfs_context_t context);

#define SMB_IOMIN (1024 * 1024)
#define SMB_IOMAXCACHE (SMB_IOMIN * 4)
#define SMB_IOMAX ((size_t)MAX_UPL_SIZE * PAGE_SIZE)

/*
* KERNEL_DEBUG related definitions for SMB.
*
* NOTE: The Class DBG_FSYSTEM = 3, and Subclass DBG_SMB = 0xA, so these
* debug codes are of the form 0x030Annnn.
*/
#define DBG_SMB     0xA     /* SMB-specific events; see the smb project */

#define SMB_DBG_CODE(code)    FSDBG_CODE(DBG_SMB, code)

/* example usage */
//SMB_LOG_KTRACE(SMB_DBG_MOUNT | DBG_FUNC_START, 0, 0, 0, 0, 0);
//SMB_LOG_KTRACE(SMB_DBG_MOUNT | DBG_FUNC_END, error, 0, 0, 0, 0);
//SMB_LOG_KTRACE(SMB_DBG_MOUNT | DBG_FUNC_NONE, 0xabc001, error, 0, 0, 0);

enum {
        /* VFS OPs */
        SMB_DBG_MOUNT                     = SMB_DBG_CODE(0),    /* 0x030A0000 */
        SMB_DBG_UNMOUNT                   = SMB_DBG_CODE(1),    /* 0x030A0004 */
        SMB_DBG_ROOT                      = SMB_DBG_CODE(2),    /* 0x030A0008 */
        SMB_DBG_VFS_GETATTR               = SMB_DBG_CODE(3),    /* 0x030A000C */
        SMB_DBG_SYNC                      = SMB_DBG_CODE(4),    /* 0x030A0010 */
        SMB_DBG_VGET                      = SMB_DBG_CODE(5),    /* 0x030A0014 */
        SMB_DBG_SYSCTL                    = SMB_DBG_CODE(6),    /* 0x030A0018 */
    
        /* VFS VNODE OPs */
        SMB_DBG_ADVLOCK                   = SMB_DBG_CODE(7),    /* 0x030A001C */
        SMB_DBG_CLOSE                     = SMB_DBG_CODE(8),    /* 0x030A0020 */
        SMB_DBG_CREATE                    = SMB_DBG_CODE(9),    /* 0x030A0024 */
        SMB_DBG_FSYNC                     = SMB_DBG_CODE(10),   /* 0x030A0028 */
        SMB_DBG_GET_ATTR                  = SMB_DBG_CODE(11),   /* 0x030A002C */
        SMB_DBG_PAGE_IN                   = SMB_DBG_CODE(12),   /* 0x030A0030 */
        SMB_DBG_INACTIVE                  = SMB_DBG_CODE(13),   /* 0x030A0034 */
        SMB_DBG_IOCTL                     = SMB_DBG_CODE(14),   /* 0x030A0038 */
        SMB_DBG_LINK                      = SMB_DBG_CODE(15),   /* 0x030A003C */
        SMB_DBG_LOOKUP                    = SMB_DBG_CODE(16),   /* 0x030A0040 */
        SMB_DBG_MKDIR                     = SMB_DBG_CODE(17),   /* 0x030A0044 */
        SMB_DBG_MKNODE                    = SMB_DBG_CODE(18),   /* 0x030A0048 */
        SMB_DBG_MMAP                      = SMB_DBG_CODE(19),   /* 0x030A004C */
        SMB_DBG_MNOMAP                    = SMB_DBG_CODE(20),   /* 0x030A0050 */
        SMB_DBG_OPEN                      = SMB_DBG_CODE(21),   /* 0x030A0054 */
        SMB_DBG_CMPD_OPEN                 = SMB_DBG_CODE(22),   /* 0x030A0058 */
        SMB_DBG_PATHCONF                  = SMB_DBG_CODE(23),   /* 0x030A005C */
        SMB_DBG_PAGE_OUT                  = SMB_DBG_CODE(24),   /* 0x030A0060 */
        SMB_DBG_COPYFILE                  = SMB_DBG_CODE(25),   /* 0x030A0064 */
        SMB_DBG_READ                      = SMB_DBG_CODE(26),   /* 0x030A0068 */
        SMB_DBG_READ_DIR                  = SMB_DBG_CODE(27),   /* 0x030A006C */
        SMB_DBG_READ_DIR_ATTR             = SMB_DBG_CODE(28),   /* 0x030A0070 */
        SMB_DBG_READ_LINK                 = SMB_DBG_CODE(29),   /* 0x030A0074 */
        SMB_DBG_RECLAIM                   = SMB_DBG_CODE(30),   /* 0x030A0078 */
        SMB_DBG_REMOVE                    = SMB_DBG_CODE(31),   /* 0x030A007C */
        SMB_DBG_RENAME                    = SMB_DBG_CODE(32),   /* 0x030A0080 */
        SMB_DBG_RM_DIR                    = SMB_DBG_CODE(33),   /* 0x030A0084 */
        SMB_DBG_SET_ATTR                  = SMB_DBG_CODE(34),   /* 0x030A0088 */
        SMB_DBG_SYM_LINK                  = SMB_DBG_CODE(35),   /* 0x030A008C */
        SMB_DBG_WRITE                     = SMB_DBG_CODE(36),   /* 0x030A0090 */
        SMB_DBG_STRATEGY                  = SMB_DBG_CODE(37),   /* 0x030A0094 */
        SMB_DBG_GET_XATTR                 = SMB_DBG_CODE(38),   /* 0x030A0098 */
        SMB_DBG_SET_XATTR                 = SMB_DBG_CODE(39),   /* 0x030A009C */
        SMB_DBG_RM_XATTR                  = SMB_DBG_CODE(40),   /* 0x030A00A0 */
        SMB_DBG_LIST_XATTR                = SMB_DBG_CODE(41),   /* 0x030A00A4 */
        SMB_DBG_MONITOR                   = SMB_DBG_CODE(42),   /* 0x030A00A8 */
        SMB_DBG_GET_NSTREAM               = SMB_DBG_CODE(43),   /* 0x030A00AC */
        SMB_DBG_MAKE_NSTREAM              = SMB_DBG_CODE(44),   /* 0x030A00B0 */
        SMB_DBG_RM_NSTREAM                = SMB_DBG_CODE(45),   /* 0x030A00B4 */
        SMB_DBG_ACCESS                    = SMB_DBG_CODE(46),   /* 0x030A00B8 */
        SMB_DBG_ALLOCATE                  = SMB_DBG_CODE(47),   /* 0x030A00BC */
    
        /* Sub Functions */
        SMB_DBG_SMBFS_CLOSE               = SMB_DBG_CODE(48),   /* 0x030A00C0 */
        SMB_DBG_SMBFS_CREATE              = SMB_DBG_CODE(49),   /* 0x030A00C4 */
        SMB_DBG_SMBFS_FSYNC               = SMB_DBG_CODE(50),   /* 0x030A00C8 */
        SMB_DBG_SMB_FSYNC                 = SMB_DBG_CODE(51),   /* 0x030A00CC */
        SMB_DBG_SMBFS_UPDATE_CACHE        = SMB_DBG_CODE(52),   /* 0x030A00D0 */
        SMB_DBG_SMBFS_OPEN                = SMB_DBG_CODE(53),   /* 0x030A00D4 */
        SMB_DBG_SMB_READ                  = SMB_DBG_CODE(54),   /* 0x030A00D8 */
        SMB_DBG_SMB_RW_ASYNC              = SMB_DBG_CODE(55),   /* 0x030A00DC */
        SMB_DBG_SMB_RW_FILL               = SMB_DBG_CODE(56),   /* 0x030A00E0 */
        SMB_DBG_PACK_ATTR_BLK             = SMB_DBG_CODE(57),   /* 0x030A00E4 */
        SMB_DBG_SMBFS_REMOVE              = SMB_DBG_CODE(58),   /* 0x030A00E8 */
        SMB_DBG_SMBFS_SETATTR             = SMB_DBG_CODE(59),   /* 0x030A00EC */
        SMB_DBG_SMBFS_GET_SEC             = SMB_DBG_CODE(60),   /* 0x030A00F0 */
        SMB_DBG_SMBFS_SET_SEC             = SMB_DBG_CODE(61),   /* 0x030A00F4 */
        SMB_DBG_SMBFS_GET_MAX_ACCESS      = SMB_DBG_CODE(62),   /* 0x030A00F8 */
        SMB_DBG_SMBFS_LOOKUP              = SMB_DBG_CODE(63),   /* 0x030A00FC */
        SMB_DBG_SMBFS_NOTIFY              = SMB_DBG_CODE(64),   /* 0x030A0100 */
	
        SMB_DBG_GET_ATTRLIST_BULK         = SMB_DBG_CODE(65),   /* 0x030A0104 */
        SMB_DBG_UPDATE_CTX                = SMB_DBG_CODE(66),   /* 0x030A0108 */
};


#endif	/* KERNEL */

#endif /* _SMBFS_SMBFS_H_ */
