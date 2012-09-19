/*
 * Copyright (c) 2000-2001, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2010 Apple Inc. All rights reserved.
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

#include <netinet/in.h>
#include <netsmb/netbios.h>

#define SMBFS_VERMAJ	1
#define SMBFS_VERMIN	7000
#define SMBFS_VERSION	(SMBFS_VERMAJ*100000 + SMBFS_VERMIN)
#define	SMBFS_VFSNAME	"smbfs"
#define SMBFS_LANMAN	"SMBFS 1.8.0"	/* Needs to match SMBFS_VERSION */
#define SMBFS_NATIVEOS	"Mac OS X 10.8"	/* Needs to match current OS version major number only */
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
	size_t		path_len;	/* Must be less than MAXPATHLEN and does not count the nuull byte */
	char		*path;		/* The starting path they want to used with this mounted volume */
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
	lck_mtx_t		sm_reclaim_renamelock; /* mount reclaim/rename lock */
	void			*notify_thread;	/* pointer to the notify thread structure */
	int32_t			tooManyNotifies;
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
int smbfs_stop_change_notify(struct smb_share *share, struct smbnode *np, 
			     int forceClose, vfs_context_t context, int *releaseLock);
void smbfs_restart_change_notify(struct smb_share *share, struct smbnode *np, 
				 vfs_context_t context);
#define SMB_IOMAX ((size_t)MAX_UPL_TRANSFER * PAGE_SIZE)

#endif	/* KERNEL */

#endif /* _SMBFS_SMBFS_H_ */
