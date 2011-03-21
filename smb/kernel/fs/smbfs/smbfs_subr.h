/*
 * Copyright (c) 2000-2001, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2009 Apple Inc. All rights reserved.
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

/*
 * Currently the valid mask is only used for the reparse tag and vtype, but in
 * the future I would like to use it for all items in the fa.
 * NOTE: the vtype valid mask doesn't mean we don't have it, just that we are 
 * not sure if its correct.
 */
#define FA_VTYPE_VALID			1
#define FA_REPARSE_TAG_VALID	2
#define FA_FSTATUS_VALID		4
#define FA_UNIX_MODES_VALID		8

struct smbfattr {
	uint64_t		fa_valid_mask;
	int				fa_attr;
	u_quad_t		fa_size;	/* data stream size */
	u_quad_t		fa_data_alloc;	/* data stream allocation size */
	struct timespec	fa_atime;	/* Access Time */
	struct timespec	fa_chtime;	/* Change Time */
	struct timespec	fa_mtime;	/* Modify Time */
	struct timespec	fa_crtime;	/* Create Time */
	u_int64_t		fa_ino;
	struct timespec fa_reqtime;
	enum vtype		fa_vtype;	/* vnode type, once we add the UNIX extensions this will contain any of the vtype */
	u_int64_t		fa_uid;
	u_int64_t		fa_gid;
	u_int64_t		fa_permissions;
	u_int64_t		fa_nlinks;
	u_int32_t		fa_flags_mask;
	u_int32_t		fa_unix;		/* Currently TRUE OR FALSE if we got the UNIX Info 2 data */
	uint32_t		fa_reparse_tag;	
};

/*
 * Context to perform findfirst/findnext/findclose operations
 */
#define	SMBFS_RDD_FINDFIRST	0x01
#define	SMBFS_RDD_EOF		0x02
#define	SMBFS_RDD_FINDSINGLE	0x04
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
	size_t		f_nmlen;	/* name len */
	/*
	 * Internal variables
	 */
	u_int16_t	f_searchCount;	/* maximum number of entries to be returned */
	int		f_attrmask;	/* SMB_FA_ */
	int		f_sfm_conversion;	/* SFM Conversion Flag */
	size_t			f_lookupNameLen;
	const char *	f_lookupName;
	struct smbnode*	f_dnp;
	struct smb_share *f_ssp;
	union {
		struct smb_rq *	uf_rq;
		struct smb_t2rq * uf_t2;
	} f_urq;
	int			f_ecnt;		/* entries left in current response */
	u_int32_t	f_eofs;		/* entry offset in data block */
	u_char 		f_skey[SMB_SKEYLEN]; /* server side search context */
	u_char		f_fname[8 + 1 + 3 + 1]; /* for 8.3 filenames */
	u_int16_t	f_Sid;
	u_int16_t	f_infolevel;
	int			f_rnamelen;
	char *		f_rname;	/* resume name */
	u_int32_t	f_rnameofs;
	int			f_rkey;		/* resume key */
};

#define f_rq	f_urq.uf_rq
#define f_t2	f_urq.uf_t2


/*
 * smb level
 */
int smbfs_fullpath_to_network(struct smb_share *, char * /*utf8str*/, char * /*network*/, size_t * /*ntwrk_len*/, 
							  char /*ntwrk_delimiter*/, int /*flags*/);
int smbfs_smb_create_unix_symlink(struct smb_share *share, struct smbnode *dnp, 
								  const char *name, size_t nmlen, char *target, 
								  size_t targetlen, struct smbfattr *fap,
								  vfs_context_t context);
int smbfs_smb_create_windows_symlink(struct smb_share *share, struct smbnode *dnp, 
									 const char *name, size_t nmlen, char *target, 
									 size_t targetlen, struct smbfattr *fap,
									 vfs_context_t context);
int smbfs_smb_create_reparse_symlink(struct smb_share *share, struct smbnode *dnp, 
									 const char *name, size_t nmlen, char *target, 
									 size_t targetlen, struct smbfattr *fap,
									 vfs_context_t context);
void smbfs_update_symlink_cache(struct smbnode *np, char *target, size_t targetlen);
int smbfs_smb_reparse_read_symlink(struct smb_share *share, struct smbnode *np, 
								   struct uio *uiop, vfs_context_t context);
int smbfs_smb_windows_read_symlink(struct smb_share *share, struct smbnode *np, 
								   struct uio *uiop, vfs_context_t context);
int smbfs_smb_unix_read_symlink(struct smb_share *share, struct smbnode *np, 
								struct uio *uiop, vfs_context_t context);
int smbfs_smb_statfs(struct smb_share *, struct vfsstatfs *, vfs_context_t);
void smbfs_smb_qfsattr(struct smb_share *, struct smbmount *, vfs_context_t);
int smbfs_smb_setfsize(struct smbnode *, u_int16_t /*fid*/, u_int64_t /*newsize*/, vfs_context_t);
int smbfs_smb_seteof(struct smb_share *, u_int16_t /*fid*/, u_int64_t  /*newsize*/, vfs_context_t);
int smbfs_smb_query_info(struct smbnode *, const char * /*name*/, size_t /*nmlen*/, struct smbfattr *, vfs_context_t);
int smbfs_smb_setpattr(struct smbnode *, const char * /*name*/, size_t  /*nmlen*/, u_int16_t /*attr*/, vfs_context_t);
int smbfs_smb_setpattrNT(struct smbnode *, u_int32_t /*attr*/, struct timespec * /*crtime*/, struct timespec * /*mtime*/, 
						 struct timespec * /*atime*/, vfs_context_t);
int smbfs_smb_setfattrNT(struct smbnode *, u_int32_t /*attr*/, u_int16_t /*fid*/, struct timespec * /*crtime*/, 
						 struct timespec * /*mtime*/, struct timespec * /*atime*/, vfs_context_t);
int smbfs_smb_open(struct smbnode *, u_int32_t /*rights*/, u_int32_t /*shareMode*/, vfs_context_t, u_int16_t */*fidp*/);
int smbfs_smb_open_xattr(struct smbnode *, u_int32_t /*rights*/, u_int32_t /*shareMode*/, vfs_context_t, u_int16_t */*fidp*/, 
						 const char */*name*/, size_t */*sizep*/);
int smbfs_smb_reopen_file(struct smbnode *, vfs_context_t );
int smbfs_smb_tmpopen(struct smbnode *, u_int32_t /*rights*/, vfs_context_t, u_int16_t */*fidp*/);
int smbfs_smb_close(struct smb_share *, u_int16_t /*fid*/, vfs_context_t);
int smbfs_smb_tmpclose(struct smbnode *, u_int16_t /*fid*/, vfs_context_t);
int smbfs_smb_openread(struct smbnode *, u_int16_t */*fidp*/, u_int32_t /*rights*/, uio_t, size_t */*sizep*/, const char */*name*/, 
					   struct timespec */*mtime*/, vfs_context_t);
int smbfs_smb_create(struct smbnode *, const char */*name*/, size_t /*nmlen*/, u_int32_t /*rights*/, vfs_context_t , 
					 u_int16_t */*fidp*/, u_int32_t /*disp*/, int /*xattr*/, struct smbfattr *);
int smbfs_smb_delete(struct smbnode *, vfs_context_t, const char */*name*/, size_t /*nmlen*/, int /*xattr*/);
int smbfs_smb_rename(struct smbnode */*src*/, struct smbnode */*tdnp*/, const char */*tname*/, size_t /*tnmlen*/, vfs_context_t);
int smbfs_smb_t2rename(struct smbnode *, const char */*tname*/, size_t /*tnmlen*/, vfs_context_t, int /*overwrite*/, u_int16_t */*infid*/);
int smbfs_delete_openfile(struct smbnode */*dnp*/, struct smbnode */*np*/, vfs_context_t);
int smbfs_smb_mkdir(struct smbnode *, const char */*name*/, size_t /*nmlen*/, vfs_context_t, struct smbfattr *);
int smbfs_smb_rmdir(struct smbnode *, vfs_context_t);
int smbfs_smb_findopen(struct smbnode *, const char */*lookupName*/, size_t /*lookupNameLen*/, 
					   vfs_context_t, struct smbfs_fctx **, int /*wildCardLookup*/);
int smbfs_smb_findnext(struct smbfs_fctx *, vfs_context_t);
void smbfs_closedirlookup(struct smbnode *, vfs_context_t);
int smbfs_fullpath(struct mbchain *, struct smb_vc *, struct smbnode *, const char */*name*/, size_t */*nmlenp*/, 
				   int /*name_flags*/, u_int8_t /*sep*/);
int smbfs_smb_lookup(struct smbnode *, const char **/*namep*/, size_t */*nmlenp*/, struct smbfattr *, vfs_context_t);
int smbfs_smb_hideit(struct smbnode *, const char */*name*/, size_t /*nmlen*/, vfs_context_t);
int smbfs_smb_unhideit(struct smbnode *, const char */*name*/, size_t /*nmlen*/, vfs_context_t);
int smbfs_set_unix_info2(struct smbnode *, struct timespec */*crtime*/, struct timespec */*mtime*/, struct timespec */*atime*/, 
						 u_int64_t /*fsize*/,  u_int64_t /*perms*/, u_int32_t /*FileFlags*/, u_int32_t /*FileFlagsMask*/, vfs_context_t);
int smbfs_smb_getsec(struct smb_share *, u_int16_t /*fid*/, vfs_context_t, u_int32_t /*selector*/, struct ntsecdesc **, size_t *seclen);
int smbfs_smb_setsec(struct smb_share *, u_int16_t /*fid*/, vfs_context_t, u_int32_t  /*selector*/, u_int16_t /*flags*/, 
					 struct ntsid */*owner*/, struct ntsid */*group*/, struct ntacl */*sacl*/, struct ntacl */*dacl*/);
int smbfs_smb_qstreaminfo(struct smbnode *, vfs_context_t, uio_t, size_t */*sizep*/, const char */*strmname*/, u_int64_t */*strmsize*/);
int smbfs_smb_flush(struct smbnode *, vfs_context_t);
void smbfs_fname_tolocal(struct smbfs_fctx *);
char *smbfs_ntwrkname_tolocal(struct smb_vc *, const char */*ntwrk_name*/, size_t */*nmlen*/);
void smb_time_server2local(u_long /*seconds*/, int /*tzoff*/, struct timespec *);
void smb_time_NT2local(u_int64_t /*nsec*/, struct timespec *);
void smb_time_local2NT(struct timespec *, u_int64_t */*nsec*/, int /*fat_fstype*/);
int smbfs_smb_lock(struct smbnode *, int /*op*/, u_int16_t /*fid*/, u_int32_t /*pid*/, off_t /*start*/, u_int64_t /*len*/, 
				   vfs_context_t, u_int32_t /*timo*/);
int smbfs_smb_ntcreatex(struct smbnode *np, u_int32_t rights, u_int32_t shareMode, 
						vfs_context_t context, enum vtype vt, u_int16_t *fidp, 
						const char *name, size_t in_nmlen, u_int32_t disp, 
						int xattr, struct smbfattr *fap);
#ifdef USE_SIDEBAND_CHANNEL_RPC
int smbfs_spotlight(struct smbnode *, vfs_context_t, void */*idata*/, void */*odata*/, size_t /*isize*/, size_t */*osize*/);
#endif // USE_SIDEBAND_CHANNEL_RPC

#endif /* !_FS_SMBFS_SMBFS_SUBR_H_ */
