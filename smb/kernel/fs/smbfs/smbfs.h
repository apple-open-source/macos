/*
 * Copyright (c) 2000-2001, Boris Popov
 * All rights reserved.
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
 * $Id: smbfs.h,v 1.30.100.6 2006/03/06 23:29:41 lindak Exp $
 */

#ifndef _SMBFS_SMBFS_H_
#define _SMBFS_SMBFS_H_

#define VT_SMBFS	VT_OTHER

#define SMBFS_VERMAJ	1
#define SMBFS_VERMIN	3600
#define SMBFS_VERSION	(SMBFS_VERMAJ*100000 + SMBFS_VERMIN)
#define	SMBFS_VFSNAME	"smbfs"

/* Values for flags */
#define SMBFS_MOUNT_SOFT	0x0001
#define SMBFS_MOUNT_INTR	0x0002
#define SMBFS_MOUNT_STRONG	0x0004
#define	SMBFS_MOUNT_HAVE_NLS	0x0008
#define	SMBFS_MOUNT_NO_LONG	0x0010

#define	SMBFS_MAXPATHCOMP	256	/* maximum number of path components */

#include <sys/mount.h>

/* Layout of the mount control block for an smb file system. */
struct smbfs_args {
	int		version;
	int		dev;
	u_int		flags;
	char		mount_point[MAXPATHLEN];
	u_char		root_path[512+1];
	uid_t		uid;
	gid_t 		gid;
	mode_t 		file_mode;
	mode_t 		dir_mode;
	int		caseopt;
	/* utf8_servname has to fit in f_mntfromname */
	char            utf8_servname[MNAMELEN];
};

#ifdef KERNEL

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_SMBFSMNT);
#endif

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

void smbfs_down(struct smbmount *smp);
void smbfs_up(struct smbmount *smp);
void smbfs_dead(struct smbmount *smp);

struct smbmount {
	struct smbfs_args	sm_args;
	struct mount * 		sm_mp;
	struct smbnode *	sm_root;
	struct ucred *		sm_owner;
	u_int32_t		sm_flags;
	long			sm_nextino;
	struct smb_share * 	sm_share;
	int			sm_caseopt;
	lck_mtx_t		*sm_hashlock;
	LIST_HEAD(smbnode_hashhead, smbnode) *sm_hash;
	u_long			sm_hashlen;
	u_int32_t		sm_status; /* status bits for this mount */
	time_t			sm_statfstime; /* sm_statfsbuf cache time */
	struct statfs		sm_statfsbuf; /* cached statfs data */
};

#define VFSTOSMBFS(mp)		((struct smbmount *)(vfs_fsprivate(mp)))
#define SMBFSTOVFS(smp)		((mount_t)((smp)->sm_mp))
#define VTOVFS(vp)		(vnode_mount(vp))
#define	VTOSMBFS(vp)		(VFSTOSMBFS(VTOVFS(vp)))

int smbfs_ioctl(struct vnop_ioctl_args *ap);
int smbfs_vinvalbuf(vnode_t vp, int flags, vfs_context_t context, int intrflg);

#define SMB_SYMMAGICLEN (4+1) /* includes a '\n' seperator */
extern char smb_symmagic[];
#define SMB_SYMLENLEN (4+1) /* includes a '\n' seperator */
#define SMB_SYMMD5LEN (32+1) /* includes a '\n' seperator */
#define SMB_SYMHDRLEN (SMB_SYMMAGICLEN + SMB_SYMLENLEN + SMB_SYMMD5LEN)
#define SMB_SYMLEN (SMB_SYMHDRLEN + MAXPATHLEN)

int smbfs_getids(struct smbnode *np, struct smb_cred *scred);
char *smb_sid2str(struct ntsid *sidp);
void smb_sid2sid16(struct ntsid *sidp, ntsid_t *sid16p);
void smb_sid_endianize(struct ntsid *sidp);

/*
 * internal versions of VOPs
 */
int smbi_getattr(vnode_t vp, struct vnode_attr *vap, vfs_context_t vfsctx);
int smbi_setattr(vnode_t vp, struct vnode_attr *vap, vfs_context_t vfsctx);
int smbi_open(vnode_t vp, int mode, vfs_context_t vfsctx);
int smbi_close(vnode_t vp, int fflag, vfs_context_t vfsctx);
int smbi_fsync(vnode_t vp, int waitfor, vfs_context_t vfsctx);

#define SMB_IOMAX ((size_t)MAX_UPL_TRANSFER * PAGE_SIZE)

#endif	/* KERNEL */

#endif /* _SMBFS_SMBFS_H_ */
