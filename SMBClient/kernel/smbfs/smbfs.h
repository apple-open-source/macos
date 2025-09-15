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

#ifndef _SMBFS_SMBFS_H_
#define _SMBFS_SMBFS_H_

#include <sys/kdebug.h>
#include <netinet/in.h>
#include <netsmb/netbios.h>
#include <netsmb/smb_2.h>
#include <netsmb/smb_conn.h>

#define SMBFS_VERMAJ	6
#define SMBFS_VERMIN	0000
#define SMBFS_VERSION	(SMBFS_VERMAJ*100000 + SMBFS_VERMIN)
#define	SMBFS_VFSNAME	"smbfs"
#define SMBFS_LANMAN	"SMBFS 6.0"	/* Needs to match SMBFS_VERSION */
#define SMBFS_NATIVEOS	"Mac OS X 15"	/* Needs to match current OS version major number only */
#define SMBFS_SLASH_TONAME "/Volumes/0x2f"

#define	SMBFS_MAXPATHCOMP	1024	/* maximum number of path components */

#include <sys/mount.h>

#define SMB_STREAMS_ON	".com.apple.smb.streams.on"
#define SMB_STREAMS_OFF	".com.apple.smb.streams.off"

#define SMB_MAX_UNIQUE_ID	128 + MAXPATHLEN + (int32_t)sizeof(struct sockaddr_storage)  /* Share name, path, sockaddr */


#define kGuestAccountName	"GUEST"
#define kGuestPassword		""


struct ByteRangeLockPB {
	int64_t offset;		/* offset to first byte to lock */
	int64_t length;		/* nbr of bytes to lock */
	uint64_t retRangeStart;	/* nbr of first byte locked if successful */
	uint8_t unLockFlag;	/* 1 = unlock, 0 = lock */
	uint8_t startEndFlag;	/* 1 = rel to end of fork, 0 = rel to start */
};

struct ByteRangeLockPB2 {
	int64_t offset;		/* offset to first byte to lock */
	int64_t length;		/* nbr of bytes to lock */
	uint64_t retRangeStart;	/* nbr of first byte locked if successful */
	uint8_t unLockFlag;	/* 1 = unlock, 0 = lock */
	uint8_t startEndFlag;	/* 1 = rel to end of fork, 0 = rel to start */
	int32_t fd;
};

struct gssdProxyPB
{
	uint32_t	mechtype;		
	uint32_t	alignPad1;
	uint8_t		*intoken;
	uint32_t	intokenLen;		
	uint32_t	uid;		
	uint8_t		*svc_namestr;
	uint64_t	verifier;		// inout
	uint32_t	context;		// inout
	uint32_t	cred_handle;		// inout
	uint8_t		*outtoken;		// out
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

/* DebugTestPB commands */
enum {
    kForceDisconnect = 0x01
};

/* DebugTestPB bitmap <Not Implemented> */
enum {
    kCleanDisconnect = 0x1,		/* if you want TCP FIN sent to server versus just dropping the TCP session */
    kSecondaryReconnect = 0x2,		/* if you want to force Primary Reconnect to fail so that Secondary must be used */
    kTriggeredDisconnect = 0x04,	/* disconnect only after a certain packet has been sent */
    kLoseReply = 0x08			/* lose the reply for certain packet.  Only used with kTriggeredDisconnect */
};

/*
 *  kForcePrimaryReconnectAfterCmd <Not Implemented>
 *      parameter[0]    AFP Command like kFPOpenFork (26)
 *      parameter[1]    optional parent DirID like the root dir (2)
 *      name            c string name of file/dir like "testfile"
 */

struct smbDebugTestPB {
    u_int32_t command;
    u_int32_t bitmap;			/* <Not Implemented> */
    u_int32_t trigger_afpCommand;	/* <Not Implemented> */
    u_int8_t triggerName[256];		/* <Not Implemented> */
    u_int32_t parameters[10];		/* <Not Implemented> */
};

struct smb_reconnect_stats {
	uint32_t version;
	uint32_t pad;
	uint64_t success_cnt;
	uint64_t fail_cnt;
	uint64_t disabled_cnt;
	uint64_t cancelled_cnt;
	uint64_t error_cnt;
	uint64_t tree_conn_fail_cnt;
	uint64_t reopen_fail_cnt;
	uint64_t no_vol_cnt;
	uint64_t fail_non_idempotent_cnt;
	uint64_t fail_create_cnt;
	uint64_t fail_close_cnt;
	uint64_t fail_lock_cnt;
	uint64_t fail_set_reparse_cnt;
	uint64_t fail_set_info_cnt;
	uint64_t fail_cmpd_create_cnt;
};

struct smbSockAddrPB {
	uint8_t session_uuid[16];
	struct sockaddr_storage addr;
};

struct smb_byte_range_locks {
	uint64_t offset;
	uint64_t length;
	uint32_t lock_pid;
	pid_t p_pid;
};

/* smb_file flags */
typedef enum _SMB_FILE_FLAGS
{
	SMB_FILE_UBC_CACHING = 		0x0001,
	SMB_FILE_MMAPPED = 		0x0002,
	SMB_FILE_DONT_COMPRESS = 	0x0004
} _SMB_FILE_FLAGS;

#define SMB_MAX_LOCKS_RETURNED 25

/* struct smb_lease flags */
typedef enum _SMB_LEASE_FLAGS
{
    SMB2_NEW_LEASE_KEY =                0x0001,
    SMB2_LEASE_PARENT_LEASE_KEY_SET =   0x0002,
    SMB2_LEASE_REQUESTED =              0x0004,
    SMB2_LEASE_GRANTED =                0x0008,
    SMB2_LEASE_V2 =                     0x0010,
    SMB2_LEASE_BROKEN =                 0x0020,
    SMB2_DEFERRED_CLOSE =               0x0040,
    SMB2_LEASE_FAIL =                   0x0080
} _SMB_LEASE_FLAGS;

/* struct smb_durable_handle flags */
typedef enum _SMB_DURABLE_HANDLE_FLAGS
{
    SMB2_DURABLE_HANDLE_REQUEST =       0x0001,
    SMB2_DURABLE_HANDLE_RECONNECT =     0x0002,
    SMB2_DURABLE_HANDLE_GRANTED =       0x0004,
    SMB2_PERSISTENT_HANDLE_REQUEST =    0x0008,
    SMB2_PERSISTENT_HANDLE_RECONNECT =  0x0010,
    SMB2_PERSISTENT_HANDLE_GRANTED =    0x0020,
    SMB2_DURABLE_HANDLE_V2 =            0x0040,
    SMB2_DURABLE_HANDLE_V2_CHECK =      0x0080,
    SMB2_DURABLE_HANDLE_FAIL =          0x0100,
    SMB2_NONDURABLE_HANDLE_RECONNECT =  0x0200
} _SMB_DURABLE_HANDLE_FLAGS;

/* smbStatPB durable handle flags are in _SMB2_DURABLE_HANDLE_FLAGS */
struct smb_dur_handle {
	uint64_t flags;
	SMB2FID fid;
	uint32_t timeout;
	uint32_t pad;
	uint8_t create_guid[16];
};

struct smb_fileRefEntryBRL {
	int32_t refcnt;     /* set via vnop_ioctl(BRL) and vnop_advlock() */
	SMB2FID fid;        /* file handle */
	uint16_t accessMode; /* access mode for this open */
	uint32_t rights;     /* nt granted rights */
	struct smb_byte_range_locks brl_locks[SMB_MAX_LOCKS_RETURNED];
	struct smb_dur_handle dur_handle;
};

struct smb_file {
	uint64_t flags;

	uint32_t sharedFID_refcnt;
	uint32_t sharedFID_mmapped;
	SMB2FID sharedFID_fid;
	uint32_t sharedFID_access_mode;
	uint32_t sharedFID_mmap_mode;
	uint32_t sharedFID_rights;
	uint32_t sharedFID_rw_refcnt;
	uint32_t sharedFID_r_refcnt;
	uint32_t sharedFID_w_refcnt;
	uint32_t sharedFID_is_EXLOCK;
	uint32_t sharedFID_is_SHLOCK;
	struct smb_dur_handle sharedFID_dur_handle;
	/*
	 * Used ONLY by sharedFID for byte range locks
	 * [0] - Used for Read/Write FIDs
	 * [1] - Used for Read FIDs
	 * [2] - Used for Write FIDs
	 */
	struct smb_fileRefEntryBRL sharedFID_lockEntries[3];

	uint32_t lockFID_refcnt;
	uint32_t lockFID_mmapped;
	SMB2FID lockFID_fid;
	uint32_t lockFID_access_mode;
	uint32_t lockFID_mmap_mode;
	uint32_t lockFID_rights;
	uint32_t lockFID_rw_refcnt;
	uint32_t lockFID_r_refcnt;
	uint32_t lockFID_w_refcnt;
	uint32_t lockFID_is_EXLOCK;
	uint32_t lockFID_is_SHLOCK;
	struct smb_dur_handle lockFID_dur_handle;
	struct smb_byte_range_locks lockFID_brl_locks[SMB_MAX_LOCKS_RETURNED];

	time_t rsrc_fork_timer;
	time_t symlink_timer;
};

/* smb_file flags */
typedef enum _SMB_DIR_FLAGS
{
    SMB_DIR_ENUM_CACHING = 0x0001
} _SMB_DIR_FLAGS;

/* smb_dir enum_flags */
typedef enum _SMB_DIR_ENUM_FLAGS
{
	SMB_DIR_ENUM_CACHE_COMPLETE = 0x0001,
	SMB_DIR_ENUM_CACHE_PARTIAL = 0x0002,
	SMB_DIR_ENUM_CACHE_DIRTY = 0x0004
} _SMB_DIR_ENUM_FLAGS;

struct smb_dir {
	uint64_t flags;
	uint32_t refcnt;
	uint32_t pad;

	SMB2FID fid;
	
	uint64_t enum_flags;
	uint64_t enum_count;
	time_t enum_timer;
};

struct smbStatPB {
	uint32_t vnode_type;
	uint32_t pad;

	union {
		struct smb_dir dir;
		struct smb_file file;
	};
	
	time_t curr_time;
	time_t meta_data_timer;
	time_t finfo_timer;
	time_t acl_cache_timer;
	
	/* smbStatPB lease flags are in _SMB_LEASE_FLAGS */
	uint64_t lease_flags;
	uint64_t lease_key_hi;
	uint64_t lease_key_low;
	uint32_t lease_req_state;
	uint32_t lease_curr_state;
	uint64_t lease_par_key_hi;
	uint64_t lease_par_key_low;
	uint32_t lease_epoch;
	uint32_t lease_def_close_reuse_cnt;
	time_t lease_def_close_timer;
};

struct smb_update_lease {
	uint64_t flags; /* Unused at this time */
};

/* Match AFP Client definition */
#define smbfsByteRangeLockFSCTL			_IOWR('z', 17, struct ByteRangeLockPB)
#define smbfsByteRangeLock2FSCTL		_IOWR('z', 23, struct ByteRangeLockPB2)
#define smbfsGetStatsFSCTL			_IOWR('z', 25, struct smb_reconnect_stats)
#define smbfsDebugTestFSCTL			_IOWR('z', 28, struct smbDebugTestPB)
#define smbfsTimeMachineFSCTL			_IOWR('z', 29, struct TimeMachinePB)


#define smbfsUpdateLeaseFSCTL			_IOWR('z', 18, struct smb_update_lease)
#define smbfsUniqueShareIDFSCTL			_IOWR('z', 19, struct UniqueSMBShareID)
#define smbfsGetSessionSockaddrFSCTL		_IOR('z', 20, struct sockaddr_storage)
#define smbfsGetSessionSockaddrFSCTL2		_IOR('z', 30, struct smbSockAddrPB)
#define smbfsStatFSCTL				_IOR('z', 31, struct smbStatPB)

/* Layout of the mount control block for an smb file system. */
struct smb_mount_args {
	int32_t		version;
	int32_t		dev;
	int64_t		altflags;
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
	int32_t         ip_QoS;
	int32_t		max_resp_timeout;
	int32_t		dir_cache_async_cnt;
	int32_t		dir_cache_max;
	int32_t		dir_cache_min;
	int32_t		max_dirs_cached;
	int32_t		max_dir_entries_cached;
	int32_t		read_size[3];
	int32_t		read_count[3];
	int32_t		write_size[3];
	int32_t		write_count[3];
	int32_t		rw_max_check_time;
	int32_t		rw_gb_threshold;
	/* Snapshot time support */
	char		snapshot_time[32] __attribute((aligned(8)));
	time_t 		snapshot_local_time;
	
	/*
	 * Compression
	 */
	
	/* Client compression algorithm map passed into session via ioctl */
	uint32_t	compression_io_threshold; /* Min IO size to try to compress */
	uint32_t        compression_chunk_len;
	uint32_t        compression_max_fail_cnt;
	
	char		compression_exclude[kClientCompressMaxEntries][kClientCompressMaxExtLen];
	uint32_t	compression_exclude_cnt;
	char		compression_include[kClientCompressMaxEntries][kClientCompressMaxExtLen];
	uint32_t	compression_include_cnt;
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
	int64_t		altflags;
	uid_t		uid;
	gid_t 		gid;
	guid_t		uuid;		/* The UUID of the user that mounted the volume */
	mode_t 		file_mode;
	mode_t 		dir_mode;
	size_t		local_path_len;	  /* Must be less than MAXPATHLEN and does not count the null byte */
	char		*local_path;	  /* Submount path they want to use with this mounted volume */
	size_t		network_path_len; /* Len of submount path in network format */
	char		*network_path;	  /* Submount path in network format */
	int32_t		unique_id_len;
	unsigned char	*unique_id;	/* A set of bytes that uniquely identifies this volume */
	char		*volume_name;
	size_t		volume_name_allocsize; /* volume_name alloc size, required when freeing volume_name */
	int32_t         ip_QoS;
	int32_t		dir_cache_async_cnt; /* Max nbr of async calls to fill dir cache */
	int32_t		dir_cache_max; /* Max time to cache entries */
	int32_t		dir_cache_min; /* Min time to cache entries */
	int32_t		max_dirs_cached;
	int32_t		max_dir_entries_cached;
	int32_t		read_size[3];
	int32_t		read_count[3];
	int32_t		write_size[3];
	int32_t		write_count[3];
	
	int32_t		rw_max_check_time;
	int32_t		rw_gb_threshold;

	/*
	 * Compression
	 */
	
	/* Client compression algorithm map passed into session via ioctl */
	uint32_t	compression_io_threshold; /* Min IO size to try to compress */
	uint32_t        compression_chunk_len;
	uint32_t        compression_max_fail_cnt;
	
	char *          compression_exclude[kClientCompressMaxEntries];
	size_t          compression_exclude_allocsize[kClientCompressMaxEntries];
	uint32_t        compression_exclude_cnt;
	
	char *          compression_include[kClientCompressMaxEntries];
	size_t          compression_include_allocsize[kClientCompressMaxEntries];
	uint32_t        compression_include_cnt;
};


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
 * Some servers do not support all the info levels on Trans2 calls. 
 * Most systems do not care if you use a file descriptor or a path name to do 
 * a get or set info call. Seems NetApp requires a file descriptor when
 * setting the time on a file. 
 */
#define MNT_REQUIRES_FILEID_FOR_TIME	0x0001	/* Setting time requires a file descriptor */
#define MNT_MAPS_NETWORK_LOCAL_USER	0x0002	/* Map  Network User <==> Local User */
#define MNT_IS_SFM_VOLUME		0x0004	/* We mount a Service for Macintosh Volume */
#define MNT_SUPPORTS_REPARSE_SYMLINKS	0x0008
#define MNT_CHANGE_NOTIFY_PAUSE		0x0010	/* Dont reenable Change Notifies */

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
	size_t			ntwrk_gids_allocsize;	/* ntwrk_gids alloc size, required when freeing ntwrk_gids */
	ntsid_t			*ntwrk_sids;
	uint32_t		ntwrk_sids_cnt;
	size_t			ntwrk_sids_allocsize;	/* ntwrk_sids alloc size, required when freeing ntwrk_sids */
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
int smbfs_close_fid(struct smb_share *share, vnode_t vp, int openMode, int close_both,
		    int *get_mod_date, vfs_context_t context);
void smbfs_wait_for_async_io(struct smbnode *np);
int smbfs_close(struct smb_share *share, vnode_t vp, int openMode,
		vfs_context_t context);
int smbfs_open(struct smb_share *share, vnode_t vp, int mode, 
	       vfs_context_t context);

int smbfs_update_cache(struct smb_share *share, vnode_t vp, 
		       struct vnode_attr *vap, vfs_context_t context);
int smbfs_fsync(struct smb_share *share, vnode_t vp, int waitfor, int ubc_flags, 
		vfs_context_t context);
int smbfs_readlink(struct smb_share *share, vnode_t vp, struct uio *uiop,
		   vfs_context_t context);
int smbfs_getattr(struct smb_share *share, vnode_t vp, struct vnode_attr *vap,
		  vfs_context_t context);
int smbfs_do_strategy(struct buf *bp);
void smbfs_strategy_count_dec(struct smbnode* np);

/*
 * Notify change routines
 */
void smbfs_notify_change_create_thread(struct smbmount *smp);
void smbfs_notify_change_destroy_thread(struct smbmount *smp);
int smbfs_start_change_notify(struct smb_share *share, vnode_t vp,
			      vfs_context_t context, int *releaseLock);
int smbfs_start_svrmsg_notify(struct smbmount *smp);
int smbfs_stop_change_notify(struct smb_share *share, vnode_t vp,
			     int forceClose, vfs_context_t context, int *releaseLock);
int smbfs_stop_svrmsg_notify(struct smbmount *smp);
void smbfs_restart_change_notify(struct smb_share *share, vnode_t vp,
				 vfs_context_t context);

#define SMB_IOMIN (1024 * 1024)
#define SMB_IOMAX ((size_t)MAX_UPL_SIZE * PAGE_SIZE)

/*
* KERNEL_DEBUG related definitions for SMB.
*
* NOTE: The Class DBG_FSYSTEM = 3, and Subclass DBG_SMB = 0xA, so these
* debug codes are of the form 0x030Annnn.
*/
#ifndef DBG_SMB
#define DBG_SMB     0xA     /* SMB-specific events; see the smb project */
#endif // DBG_SMB

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
	SMB_DBG_SMBFS_FETCH_NEW_ENTRIES   = SMB_DBG_CODE(66),   /* 0x030A0108 */
	SMB_DBG_SMBFS_HANDLE_LEASE_BREAK  = SMB_DBG_CODE(67),   /* 0x030A010C */

	/* Signing/Sealing */
	SMB_DBG_RQ_SIGN                   = SMB_DBG_CODE(68),   /* 0x030A0110 */
	SMB_DBG_RQ_VERIFY                 = SMB_DBG_CODE(69),   /* 0x030A0114 */
	SMB_DBG_RQ_ENCRYPT                = SMB_DBG_CODE(70),   /* 0x030A0118 */
	SMB_DBG_RQ_DECRYPT                = SMB_DBG_CODE(71),   /* 0x030A011C */

	/* Dir enum caching */
	SMB_DBG_DIR_CACHE_CHECK           = SMB_DBG_CODE(72),   /* 0x030A0120 */
	SMB_DBG_DIR_CACHE_REMOVE          = SMB_DBG_CODE(73),   /* 0x030A0124 */
	SMB_DBG_GLOBAL_DIR_CACHE_PRUNE    = SMB_DBG_CODE(74),   /* 0x030A0128 */
	SMB_DBG_GLOBAL_DIR_LOW_MEMORY     = SMB_DBG_CODE(75),   /* 0x030A012C */

	SMB_DBG_IOD_ENQUEUE               = SMB_DBG_CODE(76),   /* 0x030A0130 */
	SMB_DBG_IOD_RECONNECT             = SMB_DBG_CODE(77),   /* 0x030A0134 */
	SMB_DBG_IOD_SENDALL       	  = SMB_DBG_CODE(78),   /* 0x030A0138 */
	SMB_DBG_IOD_RECVALL               = SMB_DBG_CODE(79),   /* 0x030A013C */

	SMB_DBG_IOD_THREAD                = SMB_DBG_CODE(80),   /* 0x030A0140 */
	SMB_DBG_IOD_MAIN                  = SMB_DBG_CODE(81),   /* 0x030A0144 */
	SMB_DBG_IOD_WAITRQ                = SMB_DBG_CODE(82),   /* 0x030A0148 */
	SMB_DBG_IOD_RQPROCESSED           = SMB_DBG_CODE(83),   /* 0x030A014C */

	SMB_DBG_IOD_RQ_SIGN               = SMB_DBG_CODE(84),   /* 0x030A0150 */
	SMB_DBG_IOD_SENDRQ                = SMB_DBG_CODE(85),   /* 0x030A0154 */
	SMB_DBG_NBST_SEND                 = SMB_DBG_CODE(86),   /* 0x030A0158 */
	SMB_DBG_NBST_RECV                 = SMB_DBG_CODE(87),   /* 0x030A015C */
	
	SMB_DBG_MMAP_CHECK                = SMB_DBG_CODE(88),   /* 0x030A0160 */
	SMB_DBG_WRITE_COMPRESS       	  = SMB_DBG_CODE(89),   /* 0x030A0164 */
	SMB_DBG_READ_DECOMPRESS      	  = SMB_DBG_CODE(90),   /* 0x030A0168 */
	SMB_DBG_DO_STRATEGY      	  = SMB_DBG_CODE(91),   /* 0x030A016C */
	
	SMB_DBG_SMB_WRITE                 = SMB_DBG_CODE(92),   /* 0x030A0170 */
	SMB_DBG_RQ_CREDIT_DECREMENT       = SMB_DBG_CODE(93),   /* 0x030A0174 */
	SMB_DBG_RQ_CREDIT_INCREMENT       = SMB_DBG_CODE(94),   /* 0x030A0178 */
	SMB_DBG_RQ_REPLY                  = SMB_DBG_CODE(95),   /* 0x030A017C */
	
	SMB_DBG_SMB_ECHO                  = SMB_DBG_CODE(96),   /* 0x030A0180 */
	SMB_DBG_RW_THREAD                 = SMB_DBG_CODE(97),   /* 0x030A0184 */
	SMB_DBG_SMB_READ_ONE              = SMB_DBG_CODE(98),   /* 0x030A0188 */
	SMB_DBG_SMB_WRITE_ONE             = SMB_DBG_CODE(99),   /* 0x030A018C */
	
	SMB_DBG_ADJUST_QUANTUM_SIZES      = SMB_DBG_CODE(100),  /* 0x030A0190 */
	SMB_DBG_BUF_MAP           	  = SMB_DBG_CODE(101),  /* 0x030A0194 */
	SMB_DBG_BUF_UNMAP           	  = SMB_DBG_CODE(102)   /* 0x030A0198 */
};

/* 
 * Global dir enumeration caching
 *
 * Have to be careful of kern.maxfilesperproc which is at 10240. Each cached
 * dir will counts as an open file, thus keep max dirs cache upper limit at 300
 *
 * Assumptions (dir entry struct size 288 + name len = assume avg 600 bytes)
 * 1. Less than or equal to 2 GB of system RAM
 *		a) Use up to max of 0.25 GB
 *		b) Max dirs to cache - 200
 *		c) Max entries per dir - 2048 (round down to 2,000)
 *		d) 600 * 200 * 2048 = 0.23 GB
 * 2. Less than or equal to 4 GB of system RAM
 *		a) Use up to max of 0.5 GB
 *		b) Max dirs to cache - 300
 *		c) Max entries per dir - 4096 (round down to 4,000)
 *		d) 600 * 300 * 4096 = 0.69 GB
 * 3. Less than or equal to 8 GB of system RAM
 *		a) Use up to max of 1 GB
 *		b) Max dirs to cache - 300
 *		c) Max entries per dir - 7168  (round down to 6,000)
 *		d) 600 * 300 * 7168 = 1.2 GB
 * 4. Less than or equal to 16 GB of system RAM
 *		a) Use up to max of 2 GB
 *		b) Max dirs to cache - 300
 *		c) Max entries per dir - 9216  (round down to 8,000)
 *		d) 600 * 300 * 8192 = 1.37 GB
 * 5. More than or equal to 16 GB of system RAM
 *		a) Use up to max of 4 GB
 *		b) Max dirs to cache - 300
 *		c) Max entries per dir - 11000  (round down to 10,000)
 *		d) 600 * 300 * 11000 = 1.84 GB
 */
#define k_2GB_max_dirs_cached 200
#define k_2GB_max_dir_entries_cached 2000

#define k_4GB_max_dirs_cached 300
#define k_4GB_max_dir_entries_cached 4000

#define k_8GB_max_dirs_cached 300
#define k_8GB_max_dir_entries_cached 6000

#define k_16GB_max_dirs_cached 300
#define k_16GB_max_dir_entries_cached 8000

#define k_LotsOfGB_max_dirs_cached 300
#define k_LotsOfGB_max_dir_entries_cached 10000

#define k_entry_struct_size 600;

struct global_dir_cache_entry {
	const char		*name;			/* dir name */
	size_t			name_len;		/* dir name length */
	vnode_t			dvp;			/* dvp */
	uint32_t		dir_vid;		/* dir vid */
	struct timespec		last_access_time;	/* last access time */
	uint64_t		cached_cnt;		/* how many entries cached */
	struct global_dir_cache_entry *next;		/* next in list */
};

/* File Handle Leasing */
#define k_def_close_timeout 30		/* default 30 secs timeout */
#define k_def_close_lo_water 25		/* when to start closing def closes <unused> */
#define k_def_close_hi_water 100	/* dont allow any more def closes */

#endif	/* KERNEL */

#endif /* _SMBFS_SMBFS_H_ */
