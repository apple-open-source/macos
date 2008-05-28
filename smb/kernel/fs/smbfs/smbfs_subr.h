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
#ifndef _FS_SMBFS_SMBFS_SUBR_H_
#define _FS_SMBFS_SMBFS_SUBR_H_

#ifdef MALLOC_DECLARE
MALLOC_DECLARE(M_SMBFSDATA);
#endif

#define SMBFSERR(format, args...) printf("%s: "format, __FUNCTION__ ,## args)

#ifdef SMB_VNODE_DEBUG
#define SMBVDEBUG(format, args...) printf("%s: "format, __FUNCTION__ ,## args)
#else
#define SMBVDEBUG(format, args...)
#endif

/*
 * Possible lock commands
 */
#define SMB_LOCK_EXCL		0
#define	SMB_LOCK_SHARED		1
#define	SMB_LOCK_RELEASE	2

struct smbfattr {
	int				fa_attr;
	u_quad_t		fa_size;	/* data stream size */
	u_quad_t		fa_data_alloc;	/* data stream allocation size */
	struct timespec	fa_atime;	/* Access Time */
	struct timespec	fa_chtime;	/* Change Time */
	struct timespec	fa_mtime;	/* Modify Time */
	struct timespec	fa_crtime;	/* Create Time */
	long			fa_ino;
	struct timespec fa_reqtime;
	enum vtype		fa_vtype;	/* vnode type, once we add the UNIX extensions this will contain any of the vtype */
	u_int64_t		fa_uid;
	u_int64_t		fa_gid;
	u_int64_t		fa_permissions;
	u_int64_t		fa_nlinks;
	u_int32_t		fa_flags_mask;
	u_int32_t		fa_unix;		/* Currently TRUE OR FALSE if we got the UNIX Info 2 data */
};

/*
 * Context to perform findfirst/findnext/findclose operations
 */
#define	SMBFS_RDD_FINDFIRST	0x01
#define	SMBFS_RDD_EOF		0x02
#define	SMBFS_RDD_FINDSINGLE	0x04
#define	SMBFS_RDD_USESEARCH	0x08
#define	SMBFS_RDD_NOCLOSE	0x10
#define	SMBFS_RDD_GOTRNAME	0x1000

/*
 * Search context supplied by server
 */
#define	SMB_SKEYLEN		21			/* search context */
#define SMB_DENTRYLEN		(SMB_SKEYLEN + 22)	/* entire entry */

struct smbfs_fctx {
	/*
	 * Setable values
	 */
	int		f_flags;	/* SMBFS_RDD_ */
	/*
	 * Return values
	 */
	struct smbfattr	f_attr;		/* current attributes */
	char *		f_name;		/* current file name */
	int		f_nmlen;	/* name len */
	/*
	 * Internal variables
	 */
	int		f_limit;	/* maximum number of entries */
	int		f_attrmask;	/* SMB_FA_ */
	int		f_sfm_conversion;	/* SFM Conversion Flag */
	int		f_wclen;
	const char *	f_wildcard;
	struct smbnode*	f_dnp;
	struct smb_cred*f_scred;
	struct smb_share *f_ssp;
	union {
		struct smb_rq *	uf_rq;
		struct smb_t2rq * uf_t2;
	} f_urq;
	int		f_left;		/* entries left */
	int		f_ecnt;		/* entries left in current response */
	int		f_eofs;		/* entry offset in data block */
	u_char 		f_skey[SMB_SKEYLEN]; /* server side search context */
	u_char		f_fname[8 + 1 + 3 + 1]; /* for 8.3 filenames */
	u_int16_t	f_Sid;
	u_int16_t	f_infolevel;
	int		f_rnamelen;
	char *		f_rname;	/* resume name */
	int		f_rnameofs;
	int		f_otws;		/* # over-the-wire ops so far */
	int		f_rkey;		/* resume key */
};

#define f_rq	f_urq.uf_rq
#define f_t2	f_urq.uf_t2


/*
 * smb level
 */
int smbfs_fullpath_to_network(struct smb_vc *vcp, char *utf8str, char *network, int32_t *ntwrk_len, 
							  char ntwrk_delimiter, int flags);
int smbfs_smb_read_symlink(struct smb_share *ssp, struct smbnode *np, struct uio *uio, vfs_context_t context);
int smbfs_smb_create_symlink(struct smbnode *dnp, const char *name, int nmlen, char *target, 
							 u_int32_t targetlen, struct smb_cred *scrp);

int  smbfs_smb_statfs(struct smb_share *ssp, struct vfsstatfs *sbp, struct smb_cred *scrp);
void  smbfs_smb_qfsattr(struct smb_share *ssp, struct smb_cred *scrp);
int  smbfs_smb_setfsize(struct smbnode *np, u_int16_t fid, u_int64_t newsize, vfs_context_t vfsctx);
int smbfs_smb_seteof(struct smb_share *ssp, u_int16_t fid, u_int64_t newsize, struct smb_cred *scrp);
int  smbfs_smb_query_info(struct smbnode *np, const char *name, int len,
	struct smbfattr *fap, struct smb_cred *scrp);
int  smbfs_smb_setpattr(struct smbnode *np, const char *name, int len,
	u_int16_t attr, struct timespec *mtime, struct smb_cred *scrp);
int smbfs_smb_setpattrNT(struct smbnode *np, u_int32_t attr, 
			struct timespec *crtime, struct timespec *mtime,
			struct timespec *atime, struct timespec *chtime, 
			struct smb_cred *scrp);
int  
smbfs_smb_setftime(struct smbnode *np, u_int16_t fid, struct timespec *crtime, 
	struct timespec *mtime, struct timespec *atime, struct smb_cred *scrp);
int smbfs_smb_setfattrNT(struct smbnode *np, u_int32_t attr, u_int16_t fid,
			struct timespec *crtime, struct timespec *mtime,
			struct timespec *atime, struct timespec *chtime, 
			struct smb_cred *scrp);
int smbfs_smb_open(struct smbnode *np, u_int32_t rights, u_int32_t shareMode,  
					struct smb_cred *scrp, u_int16_t *fidp);
int smbfs_smb_open_xattr(struct smbnode *np, u_int32_t rights, u_int32_t shareMode, struct smb_cred *scrp, 
						   u_int16_t *fidp, const char *name, size_t *sizep);
int smbfs_smb_reopen_file(struct smbnode *np, vfs_context_t context);
int smbfs_smb_tmpopen(struct smbnode *np, u_int32_t rights, struct smb_cred *scrp, u_int16_t *fidp);
int  smbfs_smb_close(struct smb_share *ssp, u_int16_t fid, struct smb_cred *scrp);
int  smbfs_smb_tmpclose(struct smbnode *ssp, u_int16_t fid, struct smb_cred *scrp);
int smbfs_smb_openread(struct smbnode *np, u_int16_t *fid, u_int32_t rights, uio_t uio, 
					   size_t *sizep,  const char *name, struct timespec *mtime, struct smb_cred *scrp);
int  smbfs_smb_create(struct smbnode *dnp, const char *name, int len, u_int32_t rights,
	struct smb_cred *scrp, u_int16_t *fidp, u_int32_t disp, int xattr, struct smbfattr *fap);
int  smbfs_smb_delete(struct smbnode *np, struct smb_cred *scrp,
	const char *name, int len, int xattr);
int  smbfs_smb_rename(struct smbnode *src, struct smbnode *tdnp,
	const char *tname, int tnmlen, struct smb_cred *scrp);
int  smbfs_smb_t2rename(struct smbnode *np, const char *tname, int tnmlen, 
	struct smb_cred *scrp, int overwrite, u_int16_t *infid);
int smbfs_delete_openfile(struct smbnode *dnp, struct smbnode *np, 
			struct smb_cred *scrp);
int  smbfs_smb_mkdir(struct smbnode *dnp, const char *name, int len, struct smb_cred *scrp, struct smbfattr *fap);
int  smbfs_smb_rmdir(struct smbnode *np, struct smb_cred *scrp);
int  smbfs_smb_findopen(struct smbnode *dnp, const char *wildcard, int wclen,
	int attr, struct smb_cred *scrp, struct smbfs_fctx **ctxpp, int conversion_flag);
int  smbfs_smb_findnext(struct smbfs_fctx *ctx, int limit,
	struct smb_cred *scrp);
int  smbfs_smb_findclose(struct smbfs_fctx *ctx, struct smb_cred *scrp);
int  smbfs_fullpath(struct mbchain *mbp, struct smb_vc *vcp, struct smbnode *dnp, 
	const char *name, int *nmlenp, int name_flags, u_int8_t sep);
int  smbfs_smb_lookup(struct smbnode *dnp, const char **namep, int *nmlenp,
	struct smbfattr *fap, struct smb_cred *scrp);
int  smbfs_smb_hideit(struct smbnode *np, const char *name, int len,
		      struct smb_cred *scrp);
int  smbfs_smb_unhideit(struct smbnode *np, const char *name, int len,
			struct smb_cred *scrp);
int smbfs_set_unix_info2(struct smbnode *np, struct timespec *crtime, struct timespec *mtime, struct timespec *atime, u_int64_t fsize,  u_int32_t perms, u_int32_t FileFlags, u_int32_t FileFlagsMask, struct smb_cred *scrp);
int  smbfs_smb_getsec(struct smb_share *ssp, u_int16_t fid,
	struct smb_cred *scrp, u_int32_t selector, struct ntsecdesc **res, int *seclen);
int  smbfs_smb_setsec(struct smb_share *ssp, u_int16_t fid,
	struct smb_cred *scrp, u_int32_t selector, u_int16_t flags,
	struct ntsid *owner, struct ntsid *group, struct ntacl *sacl,
	struct ntacl *dacl);
int smbfs_smb_qstreaminfo(struct smbnode *np, struct smb_cred *scrp, uio_t uio, size_t *sizep, 
						  const char *strmname, u_int64_t *strmsize);
int smbfs_smb_flush(struct smbnode *np, struct smb_cred *scrp);
void smbfs_fname_tolocal(struct smbfs_fctx *ctx);
char * smbfs_ntwrkname_tolocal(struct smb_vc *vcp, const char *ntwrk_name, int *nmlen);

void  smb_time_local2server(struct timespec *tsp, int tzoff, long *seconds);
void  smb_time_server2local(u_long seconds, int tzoff, struct timespec *tsp);
void  smb_time_NT2local(u_int64_t nsec, int tzoff, struct timespec *tsp);
void  smb_time_local2NT(struct timespec *tsp, int tzoff, u_int64_t *nsec, int fat_fstype);
void  smb_time_unix2dos(struct timespec *tsp, int tzoff, u_int16_t *ddp, 
	     u_int16_t *dtp, u_int8_t *dhp);
void smb_dos2unixtime (u_int dd, u_int dt, u_int dh, int tzoff, struct timespec *tsp);
int smbfs_smb_lock(struct smbnode *np, int op, u_int16_t fid, u_int32_t pid, off_t start, 
			u_int64_t len, struct smb_cred *scrp, u_int32_t timeout);
#ifdef USE_SIDEBAND_CHANNEL_RPC
int smbfs_spotlight(struct smbnode *np, struct smb_cred *scrp, void *idata, void *odata, size_t isize, size_t *osize);
#endif // USE_SIDEBAND_CHANNEL_RPC

#endif /* !_FS_SMBFS_SMBFS_SUBR_H_ */
