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

#ifndef _SMBFS_SMBFS_H_
#define _SMBFS_SMBFS_H_


#define VT_SMBFS	VT_OTHER

#define SMBFS_VERMAJ	1
#define SMBFS_VERMIN	4400
#define SMBFS_VERSION	(SMBFS_VERMAJ*100000 + SMBFS_VERMIN)
#define	SMBFS_VFSNAME	"smbfs"
#define SMBFS_LANMAN	"SMBFS 1.4.4"	/* Needs to match SMBFS_VERSION */
#define SMBFS_NATIVEOS	"Mac OS X 10.5"	/* Needs to match current OS version major number only */
#define SMBFS_SLASH_TONAME "/Volumes/0x2f"

/* Values for flags */
#define SMBFS_MNT_SOFT	0x0001
#define	SMBFS_MNT_STREAMS_ON	0x0004
#define	SMBFS_MOUNT_NO_LONG	0x0008

#define	SMBFS_MAXPATHCOMP	256	/* maximum number of path components */

#include <sys/mount.h>

#define SMB_STREAMS_ON	".com.apple.smb.streams.on"
#define SMB_STREAMS_OFF	".com.apple.smb.streams.off"

#define SMB_MAX_UBIQUE_ID	128 + 16 /* Share name, ip address andd port number */


#define kGuestAccountName	"GUEST"
#define kGuestPassword		""


struct ByteRangeLockPB {
	int64_t	offset;		/* offset to first byte to lock */
	int64_t	length;		/* nbr of bytes to lock */
	u_int64_t	retRangeStart;	/* nbr of first byte locked if successful */
	u_int8_t	unLockFlag;	/* 1 = unlock, 0 = lock */
	u_int8_t	startEndFlag;	/* 1 = rel to end of fork, 0 = rel to start */
};

struct ByteRangeLockPB2 {
	int64_t	offset;		/* offset to first byte to lock */
	int64_t	length;		/* nbr of bytes to lock */
	u_int64_t	retRangeStart;	/* nbr of first byte locked if successful */
	u_int8_t	unLockFlag;	/* 1 = unlock, 0 = lock */
	u_int8_t	startEndFlag;	/* 1 = rel to end of fork, 0 = rel to start */
	int		fd;
};

struct SpotLightSideBandInfoPB
{
	u_int32_t	version;		// out
	u_int32_t	flags;			// unused for now
	u_int32_t	networkProtocol;	// out
	u_int32_t	srvrAddrDataLen;	// in max size, out actual size
	u_int8_t	*srvrAddrData;		// first byte is count of network addresses.  Each network address is len byte, tag byte, address
	u_int32_t	alignPad1;
	u_int32_t	authFileDataLen;	// in max size, out actual size
	u_int8_t	*authFileData;		// in address of where to return data
};

struct gssdProxyPB
{
	u_int32_t	mechtype;		
	u_int32_t	alignPad1;
	u_int8_t	*intoken;
	u_int32_t	intokenLen;		
	u_int32_t	uid;		
	u_int8_t	*svc_namestr;
	u_int64_t	verifier;		// inout
	u_int32_t	context;		// inout
	u_int32_t	cred_handle;		// inout
	u_int8_t	*outtoken;		// out
	u_int32_t	outtokenLen;		// inout
	u_int32_t	major_stat;		// out
	u_int32_t	minor_stat;		// out
};

#ifdef KERNEL
struct user_SpotLightSideBandInfoPB
{
	u_int32_t	version;		// out
	u_int32_t	flags;			// unused for now
	u_int32_t	networkProtocol;	// out
	u_int32_t	srvrAddrDataLen;	// in max size, out actual size
	user_addr_t	srvrAddrData;		// first byte is count of network addresses.  Each network address is len byte, tag byte, address
	u_int32_t	authFileDataLen;	// in max size, out actual size
	user_addr_t	authFileData;		// in address of where to return data
};

struct user_gssdProxyPB
{
	u_int32_t	mechtype;		
	user_addr_t	intoken;
	u_int32_t	intokenLen;		
	u_int32_t	uid;		
	user_addr_t	svc_namestr;
	u_int64_t	verifier;		// inout
	u_int32_t	context;		// inout
	u_int32_t	cred_handle;		// inout
	user_addr_t	outtoken;		// out
	u_int32_t	outtokenLen;		// inout
	u_int32_t	major_stat;		// out
	u_int32_t	minor_stat;		// out
};
#endif

enum  {
	kConnectedByUser = 0,
	kConnectedByGuest = 1,
	kConnectedByAnonymous = 2,
	kConnectedByKerberos = 4
};

struct UniqueSMBShareID {
	int32_t		connection_type;
	int32_t		reserved;
	int32_t		error;
	int32_t		unique_id_len;
	unsigned char	unique_id[SMB_MAX_UBIQUE_ID];	/* A set of bytes that uniquely identifies this volume */
	char		user[SMB_MAXUSERNAMELEN + 1];
};

#define smbfsByteRangeLock2FSCTL		_IOWR('z', 23, struct ByteRangeLockPB2)
#define smbfsByteRangeLock2FSCTL_BASECMD	IOCBASECMD(smbfsByteRangeLock2FSCTL)

#define smbfsByteRangeLockFSCTL			_IOWR('z', 17, struct ByteRangeLockPB)
#define smbfsByteRangeLockFSCTL_BASECMD		IOCBASECMD(smbfsByteRangeLockFSCTL)

#define smbfsUniqueShareIDFSCTL			_IOWR('z', 19, struct UniqueSMBShareID)
#define smbfsUniqueShareIDFSCTL_BASECMD		IOCBASECMD(smbfsUniqueShareIDFSCTL)

#ifdef USE_SIDEBAND_CHANNEL_RPC

#define smbfsSpotLightSideBandRPC		_IOWR('z', 27, struct SpotLightSideBandInfoPB)
#define smbfsSpotLightSideBandRPC_BASECMD	IOCBASECMD(smbfsSpotLightSideBandRPC)

#define smbfsGSSDProxy				_IOWR('z', 28, struct gssdProxyPB)
#define smbfsGSSDProxy_BASECMD			IOCBASECMD(smbfsGSSDProxy)

#endif // USE_SIDEBAND_CHANNEL_RPC

/* Layout of the mount control block for an smb file system. */
struct smb_mount_args {
	int32_t		version;
	int32_t		dev;
	int32_t		altflags;
	int32_t		debug_level;
	uid_t		uid;
	gid_t 		gid;
	mode_t 		file_mode;
	mode_t 		dir_mode;
	int32_t		path_len;
	int32_t		unique_id_len;
	char		path[MAXPATHLEN];		/* The starting path they want used for the mount */
	char		url_fromname[MAXPATHLEN];	/* The from name is the url used to mount the volume. */
	unsigned char	unique_id[SMB_MAX_UBIQUE_ID];	/* A set of bytes that uniquely identifies this volume */
};

#ifdef KERNEL

/* The items the mount point keeps in memory */
struct smbfs_args {
	int32_t		altflags;
	uid_t		uid;
	gid_t 		gid;
	mode_t 		file_mode;
	mode_t 		dir_mode;
	int32_t		path_len;	/* Must be less than MAXPATHLEN and does not count the nuull byte */
	char		*path;		/* The starting path they want to used with this mounted volume */
	int32_t		unique_id_len;
	unsigned char	*unique_id;	/* A set of bytes that uniquely identifies this volume */
};

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_SMBFSMNT);
#endif

#ifdef __APPLE_API_UNSTABLE
/*
 * %%% - We need vnode_getparent prototype in vnode.h, currently its only 
 * prototype in vnode_internal.h 
 *
 * I would prefer not to include the whole vnode_internal.h file when 
 * vnode_getparent is the only item I need. Remember that vnode_getparent is 
 * exported in the unsupport kext. So for now we will prototype here, until 
 * it gets added to vnode.h.  Note: We always need to do a vnode_put if 
 * vnode_getparent returns a vnode.
*/
vnode_t vnode_getparent(vnode_t);
#endif // __APPLE_API_UNSTABLE

struct smbnode;
struct smb_share;
struct u_cred;
struct vnop_ioctl_args;
struct buf;

/*
 * SM_MAX_STATFSTIME is the maximum time to cache statfs data. Since this
 * should be a fast call on the server, the time the data cached is short.
 * That lets the cache handle bursts of statfs() requests without generating
 * lots of network traffic.
 */
#define SM_MAX_STATFSTIME 2

/* Mask values for smbmount structure sm_status field */
#define SM_STATUS_STATFS 0x00000001 /* statfs is in progress */
#define SM_STATUS_STATFS_WANTED 0x00000002 /* statfs wakeup is wanted */
#define SM_STATUS_TIMEO 0x00000004 /* this mount is not responding */
#define SM_STATUS_DEAD	0x00000010 /* connection gone - unmount this */

void smbfs_reconnect(struct smbmount *smp);
void smbfs_down(struct smbmount *smp);
void smbfs_up(struct smbmount *smp);
void smbfs_dead(struct smbmount *smp);
void smbfs_clear_acl_cache(struct smbnode *np);

/* 
 * Some servers do not support all the info levels on Trans2 calls. Most systems do not care if you
 * use a file descriptor or a path name to do a get or set info call. Seems NetApp requires an file descriptor when 
 * setting the time on a file. 
 */
#define kRequiresFileInfoTime	0x0001	/* Setting time requires a file descriptor */
#define kInMountSMBFS		0x0002	/* In the mount call still don't allow reconnects */


struct smbmount {
    	u_int64_t		ntwrk_uid;
    	u_int64_t		ntwrk_gid;
    	u_int32_t		ntwrk_cnt_gid;
    	u_int64_t		*ntwrk_gids;
	struct smbfs_args	sm_args;
	struct mount * 		sm_mp;
	vnode_t			sm_rvp;
	kauth_cred_t		sm_owner;
	long			sm_nextino;
	struct smb_share * 	sm_share;
	int			sm_flags;	/* Info Levels that are not support on this mount point */
	lck_mtx_t		*sm_hashlock;
	LIST_HEAD(smbnode_hashhead, smbnode) *sm_hash;
	u_long			sm_hashlen;
	u_int32_t		sm_status; /* status bits for this mount */
	time_t			sm_statfstime; /* sm_statfsbuf cache time */
	lck_mtx_t		sm_statfslock; /* sm_statsbuf lock */
	struct vfsstatfs	sm_statfsbuf; /* cached statfs data */
	lck_mtx_t		sm_renamelock; /* mount rename lock */
};

#define VFSTOSMBFS(mp)		((struct smbmount *)(vfs_fsprivate(mp)))
#define SMBFSTOVFS(smp)		((mount_t)((smp)->sm_mp))
#define VTOVFS(vp)		(vnode_mount(vp))
#define	VTOSMBFS(vp)		(VFSTOSMBFS(VTOVFS(vp)))

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

#define ALLOFTHEM (KAUTH_VNODE_READ_DATA | KAUTH_VNODE_WRITE_DATA | \
		KAUTH_VNODE_APPEND_DATA | KAUTH_VNODE_READ_EXTATTRIBUTES | \
		KAUTH_VNODE_WRITE_EXTATTRIBUTES | KAUTH_VNODE_EXECUTE | \
		KAUTH_VNODE_DELETE_CHILD | KAUTH_VNODE_READ_ATTRIBUTES | \
		KAUTH_VNODE_WRITE_ATTRIBUTES | KAUTH_VNODE_DELETE | \
		KAUTH_VNODE_READ_SECURITY | KAUTH_VNODE_WRITE_SECURITY | \
		KAUTH_VNODE_TAKE_OWNERSHIP | KAUTH_VNODE_SYNCHRONIZE)

int smbfs_getids(struct smbnode *np, struct smb_cred *scred);
char *smb_sid2str(struct ntsid *sidp);
void smb_sid2sid16(struct ntsid *sidp, ntsid_t *sid16p);
void smb_sid_endianize(struct ntsid *sidp);

/*
 * internal versions of VOPs
 */
int smbfs_open(vnode_t vp, int mode, vfs_context_t vfsctx);
int smbfs_close(vnode_t vp, int fflag, vfs_context_t vfsctx);
void smbfs_create_start_path(struct smb_vc *vcp, struct smbmount *smp, struct smb_mount_args *args);

#define SMB_IOMAX ((size_t)MAX_UPL_TRANSFER * PAGE_SIZE)

#endif	/* KERNEL */

#endif /* _SMBFS_SMBFS_H_ */
