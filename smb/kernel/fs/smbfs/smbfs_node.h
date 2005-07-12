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
 * $Id: smbfs_node.h,v 1.31.52.1 2005/05/27 02:35:28 lindak Exp $
 */
#ifndef _FS_SMBFS_NODE_H_
#define _FS_SMBFS_NODE_H_

#define	SMBFS_ROOT_INO		2	/* just like in UFS */

/* Bits for smbnode.n_flag */
#define	NFLUSHINPROG	0x00001
#define	NFLUSHWANT	0x00002	/* they should gone ... */
#define	NMODIFIED	0x00004	/* bogus, until async IO implemented */
/*efine	NNEW		0x00008*//* smb/vnode has been allocated */
#define	NREFPARENT	0x00010	/* node holds parent from recycling */
#define	NGOTIDS		0x00020
#define	NISMAPPED	0x00800
#define	NFLUSHWIRE	0x01000
#define	NATTRCHANGED	0x02000	/* use smbfs_attr_cacheremove at close */
#define	NALLOC		0x04000	/* being created */
#define	NWALLOC		0x08000	/* awaiting creation */
#define	NTRANSIT	0x10000	/* being reclaimed */
#define	NWTRANSIT	0x20000	/* awaiting reclaim */

struct smbfs_fctx;

struct smbnode {
#if 0
	lck_mtx_t 		*n_lock;	
#endif
	u_int32_t		n_flag;
	struct smbnode *	n_parent;
	vnode_t 		n_vnode;
	struct smbmount *	n_mount;
	time_t			n_attrage;	/* attributes cache time */
/*	time_t			n_ctime;*/
	struct timespec		n_mtime;	/* modify time */
	struct timespec		n_atime;	/* last access time */
	u_quad_t		n_size;
	long			n_ino;
	int			n_dosattr;
	int 			n_dirrefs;
	int 			n_fidrefs;
	u_int16_t		n_fid;		/* file handle */
	u_int32_t		n_rights;	/* granted rights */
	u_char			n_nmlen;
	u_char *		n_name;
	struct smbfs_fctx *	n_dirseq;	/* ff context */
	long			n_dirofs;	/* last ff offset */
	struct lockf *		n_lockf;	/* Locking records of file */
	LIST_ENTRY(smbnode)	n_hash;
	struct timespec		n_sizetime;
	struct smbfs_lockf *smb_lockf; /* Head of byte-level lock list. */
	uid_t			n_uid;
	gid_t			n_gid;
	mode_t			n_mode;
};

/* Attribute cache timeouts in seconds */
#define	SMB_MINATTRTIMO 2
#define	SMB_MAXATTRTIMO 30

#define VTOSMB(vp)	((struct smbnode *)vnode_fsnode(vp))
#define SMBTOV(np)	((vnode_t )(np)->n_vnode)

struct vnop_getpages_args;
struct vnop_inactive_args;
struct vnop_putpages_args;
struct vnop_reclaim_args;
struct ucred;
struct smbfattr;

int  smbfs_inactive(struct vnop_inactive_args *);
int  smbfs_reclaim(struct vnop_reclaim_args *);
int smbfs_nget(struct mount *mp, vnode_t dvp, const char *name, int nmlen,
	struct smbfattr *fap, vnode_t *vpp, u_long makeentry, enum vtype vt);
u_int32_t smbfs_hash(const u_char *name, int nmlen);

int  smbfs_getpages(struct vnop_getpages_args *);
int  smbfs_putpages(struct vnop_putpages_args *);
int  smbfs_readvnode(vnode_t vp, uio_t uiop, vfs_context_t vfsctx,
	struct vnode_attr *vap);
int  smbfs_writevnode(vnode_t vp, uio_t uiop, vfs_context_t vfsctx, int ioflag, int timo);
void smbfs_attr_cacheenter(vnode_t vp, struct smbfattr *fap);
int  smbfs_attr_cachelookup(vnode_t, struct vnode_attr *, struct smb_cred *);
void smbfs_attr_touchdir(struct smbnode *dnp);
char	*smbfs_name_alloc(const u_char *name, int nmlen);
void	smbfs_name_free(const u_char *name);
void	smbfs_setsize(vnode_t , off_t);

#define smbfs_attr_cacheremove(np)	(np)->n_attrage = 0

#define smb_ubc_getsize(v) (vnode_vtype(v) == VREG ? ubc_getsize(v) : (off_t)0)

#endif /* _FS_SMBFS_NODE_H_ */
