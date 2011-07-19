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

#include <libkern/OSTypes.h>

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
 * Currentlyt the valid mask is only used for the reparse tag and vtype, but in
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
	uint64_t		fa_ino;
	struct timespec fa_reqtime;
	enum vtype		fa_vtype;	/* vnode type, once we add the UNIX extensions this will contain any of the vtype */
	uint64_t		fa_uid;
	uint64_t		fa_gid;
	uint64_t		fa_permissions;
	uint64_t		fa_nlinks;
	uint32_t		fa_flags_mask;
	uint32_t		fa_unix;		/* Currently TRUE OR FALSE if we got the UNIX Info 2 data */
	uint32_t		fa_reparse_tag;
	uint16_t		fa_fstatus;
	uint32_t		fa_created_disp;
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
	int				f_flags;	/* SMBFS_RDD_ */
	struct smbfattr	f_attr;		/* current attributes */
	char *			f_LocalName;
	size_t			f_LocalNameLen;
	char *			f_NetworkNameBuffer;
	size_t			f_MaxNetworkNameBufferSize;
	uint32_t		f_NetworkNameLen;
	uint16_t		f_searchCount;	/* maximum number of entries to be returned */
	int				f_attrmask;	/* SMB_FA_ */
	int				f_sfm_conversion;	/* SFM Conversion Flag */
	size_t			f_lookupNameLen;
	const char *	f_lookupName;
	struct smbnode*	f_dnp;
	struct smb_share *f_share;
	union {
		struct smb_rq *	uf_rq;
		struct smb_t2rq * uf_t2;
	} f_urq;
	int			f_ecnt;		/* entries left in current response */
	uint32_t	f_eofs;		/* entry offset in data block */
	u_char 		f_skey[SMB_SKEYLEN]; /* server side search context */
	uint16_t	f_Sid;
	uint16_t	f_infolevel;
	uint32_t	f_rnamelen;
	char *		f_rname;	/* resume name, always a network name */
	uint32_t	f_rnameofs;
	int			f_rkey;		/* resume key */
};

#define f_rq	f_urq.uf_rq
#define f_t2	f_urq.uf_t2

struct smb_mount_args;

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
int smbfs_smb_get_reparse_tag(struct smb_share *share, uint16_t fid, 
						  vfs_context_t context, uint32_t *reparseTag, 
						  char **outTarget, size_t *outTargetlen);
int smbfs_smb_reparse_read_symlink(struct smb_share *share, struct smbnode *np, 
								   struct uio *uiop, vfs_context_t context);
int smbfs_smb_windows_read_symlink(struct smb_share *share, struct smbnode *np, 
								struct uio *uiop, vfs_context_t context);
int smbfs_smb_unix_read_symlink(struct smb_share *share, struct smbnode *np, 
						   struct uio *uiop, vfs_context_t context);

int smbfs_smb_qstreaminfo(struct smb_share *share, struct smbnode *np, 
						  vfs_context_t context, uio_t uio, size_t *sizep, 
						  const char *strmname, uint64_t *strmsize);

void smbfs_unix_whoami(struct smb_share *share, struct smbmount *smp, vfs_context_t context);
void smbfs_unix_qfsattr(struct smb_share *share, vfs_context_t context);
void smbfs_qfsattr(struct smb_share *share, vfs_context_t context);
int smbfs_statfs(struct smb_share *share, struct vfsstatfs *sbp, vfs_context_t context);

int smbfs_smb_t2rename(struct smb_share *share, struct smbnode *np, const char *tname, 
					   size_t tnmlen, vfs_context_t context, int overwrite,  uint16_t *infid);
int smbfs_delete_openfile(struct smb_share *share, struct smbnode *dnp, 
						  struct smbnode *np, vfs_context_t context);
int smbfs_smb_flush(struct smb_share *share, uint16_t fid, vfs_context_t context);
int smbfs_seteof(struct smb_share *share, uint16_t fid, uint64_t newsize, 
				 vfs_context_t context);
int smbfs_set_allocation(struct smb_share *share, uint16_t fid, uint64_t newsize, 
					 vfs_context_t context);
int smbfs_smb_seteof(struct smb_share *share, struct smbnode *np, uint16_t fid, 
					 uint64_t newsize, vfs_context_t context);
int smbfs_smb_fsync(struct smb_share *share, struct smbnode *np, vfs_context_t context);
int smbfs_smb_query_info(struct smb_share *share, struct smbnode *np, const char *name, 
						 size_t len, uint16_t *attr, vfs_context_t context);
int smbfs_smb_lock(struct smb_share *share, int op, uint16_t fid, uint32_t pid,
				   off_t start, uint64_t len, vfs_context_t context, uint32_t timo);
int smbfs_smb_setpattr(struct smb_share *share, struct smbnode *np, const char *name, 
					   size_t len, uint16_t attr, vfs_context_t context);
int smbfs_set_hidden_bit(struct smb_share *share, struct smbnode *np, const char *name, 
						 size_t len, vfs_context_t context, Boolean hideit);
int smbfs_set_unix_info2(struct smb_share *share, struct smbnode *np, 
						 struct timespec *crtime, struct timespec *mtime, 
						 struct timespec *atime, uint64_t fsize,  uint64_t perms, 
						 uint32_t FileFlags, uint32_t FileFlagsMask, vfs_context_t context);
int smbfs_smb_setpattrNT(struct smb_share *share, struct smbnode *np, uint32_t attr, 
						 struct timespec *crtime, struct timespec *mtime, 
						 struct timespec *atime, vfs_context_t context);
int smbfs_smb_setfattrNT(struct smb_share *share, struct smbnode *np, uint32_t attr, 
						 uint16_t fid, struct timespec *crtime, struct timespec *mtime, 
						 struct timespec *atime, vfs_context_t context);
int smbfs_tmpopen(struct smb_share *share, struct smbnode *np, uint32_t rights, 
					  vfs_context_t context, uint16_t *fidp);
int smbfs_tmpclose(struct smb_share *share, struct smbnode *np, uint16_t fid, 
					   vfs_context_t context);
int smbfs_smb_openread(struct smb_share *share, struct smbnode *np, uint16_t *fid, 
					   uint32_t rights, uio_t uio, size_t *sizep,  const char *name, 
					   struct timespec *mtime, vfs_context_t context);
int smbfs_smb_open(struct smb_share *share, struct smbnode *np, uint32_t rights, 
			   uint32_t shareMode, vfs_context_t context, uint16_t *fidp);
int smbfs_smb_open_xattr(struct smb_share *share, struct smbnode *np, uint32_t rights, 
						 uint32_t shareMode, vfs_context_t context,  uint16_t *fidp, 
						 const char *name, size_t *sizep);
int smbfs_smb_reopen_file(struct smb_share *share, struct smbnode *np, vfs_context_t context);
int smbfs_smb_close(struct smb_share *share, uint16_t fid, vfs_context_t context);
int smbfs_smb_create(struct smb_share *share, struct smbnode *dnp, const char *name, 
					 size_t nmlen, uint32_t rights, vfs_context_t context, 
					 uint16_t *fidp, uint32_t disp, int xattr, struct smbfattr *fap);
int smbfs_smb_delete(struct smb_share *share, struct smbnode *np, 
					 vfs_context_t context, const char *name, size_t nmlen, int xattr);
int smbfs_smb_rename(struct smb_share *share, struct smbnode *src, 
					 struct smbnode *tdnp, const char *tname, size_t tnmlen, 
					 vfs_context_t context);
int smbfs_smb_mkdir(struct smb_share *share, struct smbnode *dnp, const char *name, 
					size_t len, vfs_context_t context, struct smbfattr *fap);
int smbfs_smb_rmdir(struct smb_share *share, struct smbnode *np, vfs_context_t context);
int smbfs_smb_findopen(struct smb_share *share, struct smbnode *dnp, 
					   const char *lookupName, size_t lookupNameLen, 
					   vfs_context_t context, struct smbfs_fctx **ctxpp, 
					   int wildCardLookup);
int smbfs_findnext(struct smbfs_fctx *, vfs_context_t);
void smbfs_closedirlookup(struct smbnode *, vfs_context_t);
int smbfs_lookup(struct smb_share *share, struct smbnode *dnp, const char **namep, 
				 size_t *nmlenp, struct smbfattr *fap, vfs_context_t context);
int smbfs_getsec(struct smb_share *share, uint16_t fid, vfs_context_t context, 
				 uint32_t selector, struct ntsecdesc **res, size_t *rt_len);
int smbfs_setsec(struct smb_share *share, uint16_t fid, vfs_context_t context, 
				 uint32_t selector, uint16_t ControlFlags, struct ntsid *owner,
				 struct ntsid *group, struct ntacl *sacl, struct ntacl *dacl);
void smb_time_NT2local(uint64_t nsec, struct timespec *tsp);
void smb_time_local2NT(struct timespec *tsp, uint64_t *nsec, int fat_fstype);
char *smbfs_ntwrkname_tolocal(const char *ntwrk_name, size_t *nmlen, int usingUnicode);
void smbfs_create_start_path(struct smbmount *smp, struct smb_mount_args *args, 
							 int usingUnicode);
int smbfs_fullpath(struct mbchain *mbp, struct smbnode *dnp, const char *name, 
				   size_t *lenp, int name_flags, int usingUnicode, uint8_t sep);
struct smb_share *smb_get_share_with_reference(struct smbmount *smp);
int smbfs_smb_ntcreatex(struct smb_share *share, struct smbnode *dnp_or_np, uint32_t rights, 
						uint32_t shareMode, vfs_context_t context, enum vtype vt, 
						uint16_t *fidp, const char *name, size_t in_nmlen, 
						uint32_t disp, int xattr, struct smbfattr *fap,
						int do_create);

#endif /* !_FS_SMBFS_SMBFS_SUBR_H_ */
