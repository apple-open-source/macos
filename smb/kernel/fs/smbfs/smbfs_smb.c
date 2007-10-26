/*
 * Copyright (c) 2000-2001 Boris Popov
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
#include <stdint.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/lock.h>
#include <sys/vnode.h>
#include <sys/xattr.h>
#include <sys/kpi_mbuf.h>
#include <sys/mount.h>

#include <sys/kauth.h>

#include <sys/smb_apple.h>
#include <sys/smb_iconv.h>
#include <sys/utfconv.h>

#include <netsmb/smb.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_conn.h>

#include <fs/smbfs/smbfs.h>
#include <fs/smbfs/smbfs_node.h>
#include <fs/smbfs/smbfs_subr.h>
#include <fs/smbfs/smbfs_lockf.h>

/*
 * Lack of inode numbers leads us to the problem of generating them.
 * Partially this problem can be solved by having a dir/file cache
 * with inode numbers generated from the incremented by one counter.
 * However this way will require too much kernel memory, gives all
 * sorts of locking and consistency problems, not to mentinon counter overflows.
 * So, I'm decided to use a hash function to generate pseudo random (and unique)
 * inode numbers.
 */
static long
smbfs_getino(struct smbnode *dnp, const char *name, int nmlen)
{
	u_int32_t ino;

	ino = dnp->n_ino + smbfs_hash((u_char *)name, nmlen);
	if (ino <= 2)
		ino += 3;
	return ino;
}

PRIVSYM int
smbfs_smb_lock(struct smbnode *np, int op, u_int16_t fid, u_int32_t pid,
	off_t start, u_int64_t len, struct smb_cred *scrp, u_int32_t timeout)
{
	struct smb_share *ssp = np->n_mount->sm_share;
	struct smb_rq rq, *rqp = &rq;
	struct mbchain *mbp;
	u_char ltype = 0;
	int error;

	if (op == SMB_LOCK_SHARED)
		ltype |= SMB_LOCKING_ANDX_SHARED_LOCK;
		/* Do they support large offsets */
	if ((SSTOVC(ssp)->vc_sopt.sv_caps & SMB_CAP_LARGE_FILES))
		ltype |= SMB_LOCKING_ANDX_LARGE_FILES;
	error = smb_rq_init(rqp, SSTOCP(ssp), SMB_COM_LOCKING_ANDX, scrp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint8(mbp, 0xff);	/* secondary command */
	mb_put_uint8(mbp, 0);		/* MBZ */
	mb_put_uint16le(mbp, 0);
	mb_put_mem(mbp, (caddr_t)&fid, 2, MB_MSYSTEM);
	mb_put_uint8(mbp, ltype);	/* locktype */
	mb_put_uint8(mbp, 0);		/* oplocklevel - 0 seems is NO_OPLOCK */
	mb_put_uint32le(mbp, timeout);	/* 0 nowait, -1 infinite wait */
	mb_put_uint16le(mbp, op == SMB_LOCK_RELEASE ? 1 : 0);
	mb_put_uint16le(mbp, op == SMB_LOCK_RELEASE ? 0 : 1);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint16le(mbp, pid);
	if (ltype & SMB_LOCKING_ANDX_LARGE_FILES) {
		mb_put_uint16le(mbp, 0); /* pad */
		mb_put_uint32le(mbp, start >> 32); /* OffsetHigh */
		mb_put_uint32le(mbp, start & 0xffffffff); /* OffsetLow */
		mb_put_uint32le(mbp, len >> 32); /* LengthHigh */
		mb_put_uint32le(mbp, len & 0xffffffff); /* LengthLow */
	}
	else {
		mb_put_uint32le(mbp, start);
		mb_put_uint32le(mbp, len);
	}
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	smb_rq_done(rqp);
	return error;
}

static int
smbfs_smb_qpathinfo(struct smbnode *np, struct smbfattr *fap, 
					struct smb_cred *scrp, short infolevel,
					const char **namep, int *nmlenp)
{
	struct smb_share *ssp = np->n_mount->sm_share;
	struct smb_vc *vcp = SSTOVC(ssp);
	struct smb_t2rq *t2p;
	int error, svtz;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int16_t date, time, wattr;
	u_int64_t llint;
	u_int32_t size, dattr;
	const char *name = (namep ? *namep : NULL);
	int nmlen = (nmlenp ? *nmlenp : 0);
	char *ntwrkname = NULL;
	char *filename;
	u_int8_t sep = '\\';

	error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_QUERY_PATH_INFORMATION,
			     scrp, &t2p);
	if (error)
		return error;
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	if (!infolevel) {
		if (SMB_DIALECT(vcp) < SMB_DIALECT_NTLM0_12)
			infolevel = SMB_QFILEINFO_STANDARD;
		else
			infolevel = SMB_QFILEINFO_BASIC_INFO;
	}
	mb_put_uint16le(mbp, infolevel);
	mb_put_uint32le(mbp, 0);
	/* mb_put_uint8(mbp, SMB_DT_ASCII); specs are wrong */
	if ((np->n_vnode) && vnode_isnamedstream(np->n_vnode)) {
		DBG_ASSERT((namep == NULL));
		name = (const char *)np->n_sname;
		nmlen = np->n_snmlen;
		sep = ':';
	}
	error = smbfs_fullpath(mbp, vcp, np, name, &nmlen, UTF_SFM_CONVERSIONS, sep);
	
	if (error) {
		smb_t2_done(t2p);
		return error;
	}
	t2p->t2_maxpcount = 2;
	t2p->t2_maxdcount = vcp->vc_txmax;
	error = smb_t2_request(t2p);
	if (error) {
		smb_t2_done(t2p);
		if (infolevel == SMB_QFILEINFO_BASIC_INFO && error == EINVAL)
			return smbfs_smb_qpathinfo(np, fap, scrp,
						   SMB_QFILEINFO_STANDARD, NULL, NULL);
		return error;
	}
	mdp = &t2p->t2_rdata;
	svtz = vcp->vc_sopt.sv_tz;
	switch (infolevel) {
	case SMB_QFILEINFO_STANDARD:
		md_get_uint16le(mdp, &date);
		md_get_uint16le(mdp, &time);	/* creation time */
		if (date || time) {
			smb_dos2unixtime(date, time, 0, svtz, &fap->fa_crtime);
		}
		md_get_uint16le(mdp, &date);
		md_get_uint16le(mdp, &time);	/* access time */
		if (date || time) {
			smb_dos2unixtime(date, time, 0, svtz, &fap->fa_atime);
		}
		md_get_uint16le(mdp, &date);
		md_get_uint16le(mdp, &time);	/* modify time */
		if (date || time) {
			smb_dos2unixtime(date, time, 0, svtz, &fap->fa_mtime);
		}
		md_get_uint32le(mdp, &size);
		fap->fa_size = size;
		md_get_uint32le(mdp, &size);	/* allocation size */
		fap->fa_data_alloc = size;
		md_get_uint16le(mdp, &wattr);
		fap->fa_attr = wattr;
		fap->fa_vtype = (fap->fa_attr & SMB_FA_DIR) ? VDIR : VREG;
		break;
	case SMB_QFILEINFO_BASIC_INFO:
		md_get_uint64le(mdp, &llint);	/* creation time */
		if (llint) {
			smb_time_NT2local(llint, svtz, &fap->fa_crtime);
		}
		md_get_uint64le(mdp, &llint);
		if (llint) {
			smb_time_NT2local(llint, svtz, &fap->fa_atime);
		}
		md_get_uint64le(mdp, &llint);
		if (llint) {
			smb_time_NT2local(llint, svtz, &fap->fa_mtime);
		}
		md_get_uint64le(mdp, &llint);
		if (llint) {
			smb_time_NT2local(llint, svtz, &fap->fa_chtime);
		}
		md_get_uint32le(mdp, &dattr);
		fap->fa_attr = dattr;
		fap->fa_vtype = (fap->fa_attr & SMB_FA_DIR) ? VDIR : VREG;
		break;
	case SMB_QFILEINFO_ALL_INFO:								
		md_get_uint64le(mdp, &llint);	/* creation time */
		if (llint) {
			smb_time_NT2local(llint, svtz, &fap->fa_crtime);
		}
		md_get_uint64le(mdp, &llint);
		if (llint) {			/* access time */
			smb_time_NT2local(llint, svtz, &fap->fa_atime);
		}
		md_get_uint64le(mdp, &llint);
		if (llint) {			/* write time */
			smb_time_NT2local(llint, svtz, &fap->fa_mtime);
		}
		md_get_uint64le(mdp, &llint);
		if (llint) {			/* change time */
			smb_time_NT2local(llint, svtz, &fap->fa_chtime);
		}
		/* 
		 * SNIA CIFS Technical Reference is wrong, this should be 
		 * a ULONG 
		 */ 
		md_get_uint32le(mdp, &dattr);	/* Attributes */
		fap->fa_attr = dattr;
		fap->fa_vtype = (fap->fa_attr & SMB_FA_DIR) ? VDIR : VREG;

		/* 
		 * SNIA CIFS Technical Reference is wrong, this should be 
		 * a ULONG PAD 
		 */ 
		md_get_uint32le(mdp, NULL);
		
		md_get_uint64le(mdp, &llint); /* allocation size */
		fap->fa_data_alloc = llint;
		
		md_get_uint64le(mdp, &llint);	/* file size */
		fap->fa_size = llint;
		
		md_get_uint32le(mdp, NULL);	/* Number of hard links */
		md_get_uint8(mdp, NULL);	/* Delete Pending */
		md_get_uint8(mdp, NULL);	/* Directory or File */
		fap->fa_ino = np->n_ino;
		if (namep == NULL)
			break;	/* We are done */
		/* 
		 * At this point the SNIA CIFS Technical Reference is wrong. 
		 * It should have the following: 
		 *			USHORT		Unknown;
		 *			ULONG		EASize;
		 *			ULONG		PathNameLength;
		 *			STRING		FullPath;
		 * We need to be carefull just in case someone followed the 
		 * Technical Reference.
		 */
		md_get_uint16(mdp, NULL);	/* Unknown */
		md_get_uint32(mdp, NULL);	/* EASize */
		md_get_uint32le(mdp, &size);	/* Path name lengh */
		/* 
		 * Make sure it is something in reason. Don't allocate it, 
		 * if it doesn't make sense. 
		 */
		if (size <= 0 || size >= t2p->t2_maxdcount) {
			error = EINVAL;
			goto bad;
		}
		/* 
		 * Since the SNIA CIFS Technical Reference is wrong, we would 
		 * like to do as much checking as possible. The whole message 
		 * should be the file name length + 72 bytes. If we get 
		 * something bigger print it out. In the worst case situation 
		 * we would end up with a bogus name. We have found that the 
		 * Snap servers follows the Technical Reference. So in their 
		 * case the file name length is bogus. This is not a problem 
		 * because they always return zero in that location. So this 
		 * would error out above. NT returns the correct information
		 * but does put 4 bytes of zero padd at the end. So now add 
		 * 4 bytes to our check.
		 */
		if (mbuf_pkthdr_len(mdp->md_top) > (size+72+4)) {
			SMBERROR("SMB_QFILEINFO_ALL_INFO: wrong size %ld\n", 
					mbuf_pkthdr_len((mbuf_t)mdp->md_top));
		}
	
		nmlen = size;

		/* Since this is a full path only check SMB_MAXFNAMELEN length 
		 * until we get the component. We just allocate what we need
		 * need here. 
		 */
		ntwrkname = malloc(nmlen, M_SMBFSDATA, M_WAITOK);
		if (ntwrkname == NULL)
			error = ENOMEM;
		else 
			error = md_get_mem(mdp, (void *)ntwrkname, nmlen,
					MB_MSYSTEM);	/* Full path name */
		
		if (error)
			goto bad;
	
		/*
		 * Here is the problem. They return the full path when we only 
		 * need the last component. So we need to find the last back 
		 * slash. So first remove any trailing nulls from the path.
		 * Now start at the end of the path name and work our way back 
		 * stopping when we find the first back slash. For UTF-16 make 
		 * sure there is a null byte after the back slash.
		 */
		if (SMB_UNICODE_STRINGS(vcp)) {
			/* Don't count any trailing nulls in the name. */
			if (nmlen > 1 && ntwrkname[nmlen - 1] == 0 && 
				ntwrkname[nmlen - 2] == 0)
				nmlen -= 2;
			/* 
			 * Now get the file name. We need to start at the end
			 * and work our way backwards.
			 */
			if (nmlen > 1)
				filename = &ntwrkname[nmlen-2];
			else filename = ntwrkname;
			 /* Work backwards until we reach the begining */
			while (filename > ntwrkname) {
				if ((*filename == 0x5c) && (*(filename+1) == 0x00))
					break;
				filename -= 2;
			}
			/* 
			 * Found a back slash move passed it and now we have 
			 * the real file name. 
			 */
			if ((*filename == 0x5c) && (*(filename+1) == 0x00))
				filename += 2;
		} else {
			/* Don't count any trailing null in the name. */
			if (nmlen && ntwrkname[nmlen - 1] == 0)
				nmlen--;
			/* 
			 * Now get the file name. We need to start at the end
			 * and work our way bacckwards.
			 */
			if (nmlen)
				filename = &ntwrkname[nmlen-1];
			else filename = ntwrkname;
			/* Work backwards until we reach the begining */
			while ((filename > ntwrkname) && (*filename != 0x5c))
				filename--;
			/* 
			 * Found a back slash move passed it and now we have 
			 * the real file name. 
			 */
			if (*filename == 0x5c)
				*filename++;
		}
		/* Reset the name length */
		nmlen = &ntwrkname[nmlen] - filename;
		/* Convert the name to a UTF-8 string  */
		filename = smbfs_ntwrkname_tolocal(vcp, (const char *)filename, 
							&nmlen);
		/* Done with the network buffer so free it */
		free(ntwrkname, M_SMBFSDATA);
		/* Now reasign it so we free the correct buffer */	
		ntwrkname = filename;

		if (ntwrkname == NULL) {
			error = EINVAL;
			SMBERROR("smbfs_ntwrkname_tolocal return NULL\n");
			goto bad;
		}
		if (nmlen > SMB_MAXFNAMELEN) {
			error = EINVAL;
			SMBERROR("Filename  %s nmlen = %d\n", ntwrkname, nmlen);
			goto bad;
		}
		*namep = (char *)smbfs_name_alloc((u_char *)(filename), nmlen);			
		if (nmlenp)	/* Return the name length */
			*nmlenp = nmlen;
		if (*namep)	/* Create the inode numer*/
			fap->fa_ino = smbfs_getino(np, *namep, *nmlenp);
bad:
		/* Free the buffer that holds the name from the network */
		if (ntwrkname) 
			free(ntwrkname, M_SMBFSDATA);
		break;
	case SMB_QFILEINFO_UNIX_INFO2:
		if (namep != NULL) {
			SMBERROR("SMB_QFILEINFO_UNIX_INFO2: Looking for the name, not supported! \n");
			error = EINVAL;			
		}
		
		md_get_uint64le(mdp, &llint);	/* file size */
		fap->fa_size = llint;
		
		md_get_uint64le(mdp, &llint); /* allocation size */
		fap->fa_data_alloc = llint;
	
		md_get_uint64le(mdp, &llint);	/* change time */
		if (llint)
			smb_time_NT2local(llint, svtz, &fap->fa_chtime);
		
		md_get_uint64le(mdp, &llint);	/* access time */
		if (llint)
			smb_time_NT2local(llint, svtz, &fap->fa_atime);

		md_get_uint64le(mdp, &llint);	/* write time */
		if (llint)
			smb_time_NT2local(llint, svtz, &fap->fa_mtime);

		md_get_uint64le(mdp, &llint);	/* Numeric user id for the owner */
		fap->fa_uid = llint;
		
		md_get_uint64le(mdp, &llint);	/* Numeric group id for the owner */
		fap->fa_gid = llint;

		md_get_uint32le(mdp, &dattr);	/* Enumeration specifying the file type, st_mode */
		/* Make sure the dos attributes are correct */ 
		if (dattr & EXT_UNIX_DIR) {
			fap->fa_attr |= SMB_FA_DIR;
			fap->fa_vtype = VDIR;
		} else {
			fap->fa_attr &= ~SMB_FA_DIR;
			if (dattr & EXT_UNIX_SYMLINK)
				fap->fa_vtype = VLNK;
			else
				fap->fa_vtype = VREG;
		}

		md_get_uint64le(mdp, &llint);	/* Major device number if type is device */
		md_get_uint64le(mdp, &llint);	/* Minor device number if type is device */
		md_get_uint64le(mdp, &llint);	/* This is a server-assigned unique id */
		md_get_uint64le(mdp, &llint);	/* Standard UNIX permissions */
		fap->fa_permissions = llint;
		md_get_uint64le(mdp, &llint);	/* Number of hard link */
		fap->fa_nlinks = llint;

		md_get_uint64le(mdp, &llint);	/* creation time */
		if (llint)
			smb_time_NT2local(llint, svtz, &fap->fa_crtime);

		md_get_uint32le(mdp, &dattr);	/* File flags enumeration */
		md_get_uint32le(mdp, &fap->fa_flags_mask);	/* Mask of valid flags */
		/* Make sure the dos attributes are correct */
		if (fap->fa_flags_mask & EXT_HIDDEN) {
			if (dattr & EXT_HIDDEN)
				fap->fa_attr |= SMB_FA_HIDDEN;
			else
				fap->fa_attr &= ~SMB_FA_HIDDEN;
		}
		if (fap->fa_flags_mask & EXT_IMMUTABLE) {
			if (dattr & EXT_IMMUTABLE)
				fap->fa_attr |= SMB_FA_RDONLY;
			else
				fap->fa_attr &= ~SMB_FA_RDONLY;
		}
		if (fap->fa_flags_mask & SMB_FA_ARCHIVE) {
			if (dattr & EXT_DO_NOT_BACKUP)
				fap->fa_attr &= ~SMB_FA_ARCHIVE;
			else
				fap->fa_attr |= SMB_FA_ARCHIVE;
		}
		fap->fa_unix = TRUE;
		if (namep == NULL)
			fap->fa_ino = np->n_ino;
		break;
	default:
		SMBERROR("unexpected info level %d\n", infolevel);
		error = EINVAL;
		break;
	}
	smb_t2_done(t2p);
	return error;
}


static int
smbfs_smb_undollardata(struct smbnode *np, struct smbfs_fctx *ctx)
{
	char *cp;
	int len = strlen(SMB_DATASTREAM);

	if (!ctx->f_name)	/* sanity check */
		goto bad;
	if (ctx->f_nmlen < len + 1)	/* "::$DATA" at a minimum */
		goto bad;
	if (*ctx->f_name != ':')	/* leading colon - "always" */
		goto bad;
	cp =  &ctx->f_name[ctx->f_nmlen - len]; /* point to 2nd colon */
	if (bcmp(cp, SMB_DATASTREAM, len))
		goto bad;
	if (ctx->f_nmlen == len + 1)	/* merely the data fork? */
		return (0);		/* skip it */
	if (ctx->f_nmlen - len > XATTR_MAXNAMELEN + 1)
		goto bad;	/* mustnt return more than 128 bytes */
	/*
	 * Un-count a colon and the $DATA, then the
	 * 2nd colon is replaced by a terminating null.
	 */
	ctx->f_nmlen -= len;
	/* Skip protected system attrs */
	if ((ctx->f_nmlen >= 17) && (xattr_protected(ctx->f_name)))
		return (0);
	
	*cp = '\0';
	return (1);
bad:
	SMBERROR("file \"%.*s\" has bad stream \"%.*s\"\n", np->n_nmlen,
		 np->n_name, ctx->f_nmlen, ctx->f_name);
	return (0); /* skip it */
}

/*
 * Support for creating a symbolic link that resides on a UNIX server. This allows
 * us to access and share symbolic links with AFP and NFS.
 */
PRIVSYM int smbfs_smb_create_symlink(struct smbnode *dnp, const char *name, int nmlen, 
									 char *target, u_int32_t targetlen, struct smb_cred *scrp)
{
	struct smb_t2rq *t2p;
	struct smb_share *ssp = dnp->n_mount->sm_share;
	struct smb_vc *vcp = SSTOVC(ssp);
	struct mbchain *mbp;
	int error;
	char *ntwrkpath = NULL;
	int ntwrkpathlen = targetlen * 2;	/* UTF8 to UTF16 most it can be is twice as big */
	int flags = UTF_PRECOMPOSED|UTF_NO_NULL_TERM;
	
	MALLOC(ntwrkpath, char *, ntwrkpathlen, M_SMBFSDATA, M_WAITOK);
	if (ntwrkpath == NULL)
		error = ENOMEM;
	else
		error = smbfs_fullpath_to_network(vcp, (char *)target, ntwrkpath, &ntwrkpathlen, '/', flags);
	if (!error)
		error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_SET_PATH_INFORMATION, scrp, &t2p);
	if (error) {
		if (ntwrkpath)
			free(ntwrkpath, M_SMBFSDATA);
		SMBWARNING("Creating symlink for %s failed! error = %d\n", name, error);
		return error;		
	}
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	mb_put_uint16le(mbp, SMB_SFILEINFO_UNIX_LINK);
	mb_put_uint32le(mbp, 0);		/* MBZ */
	error = smbfs_fullpath(mbp, vcp, dnp, name, &nmlen, UTF_SFM_CONVERSIONS, '\\');
	if (error)
		goto out;
	mbp = &t2p->t2_tdata;
	mb_init(mbp);
	mb_put_mem(mbp, (caddr_t)(ntwrkpath), ntwrkpathlen, MB_MSYSTEM);
	t2p->t2_maxpcount = 2;
	t2p->t2_maxdcount = vcp->vc_txmax;
	error = smb_t2_request(t2p);
	
out:
	free(ntwrkpath, M_SMBFSDATA);
	smb_t2_done(t2p);
	if (error)
		SMBWARNING("Creating symlink for %s failed! error = %d\n", name, error);
	return error;
	
}

/*
 * Support for reading a symbolic link that resides on a UNIX server. This allows
 * us to access and share symbolic links with AFP and NFS.
 */
PRIVSYM int smbfs_smb_read_symlink(struct smb_share *ssp, struct smbnode *np, struct uio *uio, vfs_context_t context)
{
	struct smb_vc *vcp = SSTOVC(ssp);
	struct smb_t2rq *t2p;
	int error;
	struct mbchain *mbp;
	struct mdchain *mdp;
	char *ntwrkname = NULL;
	char *filename = NULL;
	struct smb_cred scred;
	int nmlen;
	char *endpath, *startpath, *nextpath;
	
	smb_scred_init(&scred, context);
	error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_QUERY_PATH_INFORMATION, &scred, &t2p);
	if (error)
		return error;
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	mb_put_uint16le(mbp, SMB_QFILEINFO_UNIX_LINK);
	mb_put_uint32le(mbp, 0);
	error = smbfs_fullpath(mbp, vcp, np, NULL, NULL, UTF_SFM_CONVERSIONS, '\\');
	if (error) 
	    goto out;
	t2p->t2_maxpcount = 2;
	t2p->t2_maxdcount = vcp->vc_txmax;
	error = smb_t2_request(t2p);
	if (error) 
	    goto out;
	mdp = &t2p->t2_rdata;
	/* Get the size of the data that contains the symbolic link */
	nmlen = (int)mbuf_pkthdr_len(mdp->md_cur);
	SMBDEBUG("network len of the symbolic link = %d\n", nmlen);
	MALLOC(ntwrkname, char *, nmlen, M_SMBFSDATA, M_WAITOK);
	if (ntwrkname == NULL)
		error = ENOMEM;
	else /* Read in the data that contains the symbolic link */
		error = md_get_mem(mdp, (void *)ntwrkname, nmlen, MB_MSYSTEM);
	if (error) 
	    goto out;
#ifdef DEBUG_SYMBOLIC_LINKS
	smb_hexdump(__FUNCTION__, "Symlink: ", (u_char *)ntwrkname, (int)nmlen);
#endif // DEBUG_SYMBOLIC_LINKS
	/*
	 * The Symbolic link data is a UNIX style symbolic link, except it is in UTF16
	 * format. The unix slash is the delemter used. We have no routine that can convert
	 * the whole path, so we need to break it up and convert it section by section.
	 *
	 * The path should end with two null bytes as a terminator. The smbfs_ntwrkname_tolocal 
	 * does not expect nmlen to include those bytes, so we can just back those bytes out.
	 * We do an extra check just in case someone decided not to include the two null bytes.
	 * Nothiing in the reference states that the null bytes are require, but this would be
	 * the normal way to handle this type of string in SMB. We know that Samba 3.0.25 puts
	 * the null bytes at the end, but there could be a server that doesn't so lets protect
	 * ourself here.
	 */
	nmlen -= 2;	/* make endpath point at the null bytes */
	endpath = ntwrkname + nmlen;
	/* Make sure endpath is pointing at the two null bytes, if we have any */
	if ((*(int16_t *)endpath) != 0)
		endpath += 2;
	startpath = ntwrkname;
	nextpath = ntwrkname;
	while ((error == 0) && (nextpath < endpath)) {
		if ((*nextpath == 0x2f) && (*(nextpath+1) == 0x00)) {
			/* If startpath equals nextpath we have just a slash nothing to do here */
			if (startpath != nextpath) {
				nmlen = nextpath - startpath;	/* Get the length of this component of the path */
				filename = smbfs_ntwrkname_tolocal(vcp, (const char *)startpath, &nmlen);
				DBG_ASSERT(filename);
				if (filename == NULL)
					error = EINVAL;	/* Something bad just happen, just bail out! */
				else {
					filename[nmlen] = 0;	/* Just to be safe, smbfs_ntwrkname_tolocal doesn't null terminate */
					SMBDEBUG("filename = %s nmlen = %d strlen = %d\n", filename, (int)nmlen, (int)strlen(filename));
					/* Copy the UTF8 string into uio */
					error = uiomove(filename, nmlen, uio);
					free(filename, M_SMBFSDATA);
				}
			}
			/* If we found an entry then we need a slash, just used the one that nextpath points to */
			if (!error)
				error = uiomove(nextpath, 1, uio);
			nextpath += 2;
			startpath = nextpath;
		} else
			nextpath += 2;
	}
	if (!error && (startpath < endpath)) {
		nmlen = endpath - startpath;		/* Get the length of the last component of the path */
		filename = smbfs_ntwrkname_tolocal(vcp, (const char *)startpath, &nmlen);
		DBG_ASSERT(filename);
		if (filename == NULL)
			error = EINVAL;	/* Something bad just happen, just bail out! */
		else {
			filename[nmlen] = 0;	/* Just to be safe, smbfs_ntwrkname_tolocal doesn't null terminate */
			SMBDEBUG("Last part of path filename = %s nmlen = %d strlen = %d\n", filename, (int)nmlen, (int)strlen(filename));
			/* Copy the UTF8 string into uio */
			error = uiomove(filename, nmlen, uio);
			free(filename, M_SMBFSDATA);					
		}
	}
	
out:
	if (ntwrkname)
		free(ntwrkname, M_SMBFSDATA);
	smb_t2_done(t2p);
	if (error)
		SMBWARNING("Reading symlink for %s failed! error = %d\n", np->n_name, error);
	return error;
}

/*
 * When calling this routine be very carefull when passing the arguments. Depending on the arguments 
 * different actions will be taken with this routine. 
 *
 * %%% - We should make into three different routines.
 */
PRIVSYM int smbfs_smb_qstreaminfo(struct smbnode *np, struct smb_cred *scrp, uio_t uio, size_t *sizep,
								  const char *strmname, u_int64_t *strmsize)
{
	struct smb_share *ssp = np->n_mount->sm_share;
	struct smb_vc *vcp = SSTOVC(ssp);
	struct smb_t2rq *t2p;
	int error;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int32_t next, nlen, used;
	struct smbfs_fctx ctx;
	u_int64_t stream_size;
	enum stream_types stype = kNoStream;
	struct timespec ts;
	int foundStream = (strmname) ? FALSE : TRUE; /* Are they looking for a specific stream */
	
	if (sizep)
		*sizep = 0;
	ctx.f_ssp = ssp;
	ctx.f_name = NULL;

	error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_QUERY_PATH_INFORMATION, scrp, &t2p);
	if (error)
		return (error);
		
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	/*
	 * SAMBA only supports the SMB_QFILEINFO_STREAM_INFORMATION level. Samba declined to 
	 * support the older info level with a comment claiming doing so caused a BSOD. Not sure
	 * what that is all about. I have never seen that happen, but there is no differences
	 * between the two levels. So if the server supports the new levels then use it.
	 */
	if (vcp->vc_sopt.sv_caps & SMB_CAP_INFOLEVEL_PASSTHRU)
		mb_put_uint16le(mbp, SMB_QFILEINFO_STREAM_INFORMATION);
	else
		mb_put_uint16le(mbp, SMB_QFILEINFO_STREAM_INFO);
	mb_put_uint32le(mbp, 0);

	error = smbfs_fullpath(mbp, vcp, np, NULL, NULL, UTF_SFM_CONVERSIONS, '\\');
	if (error)
		goto out;
	t2p->t2_maxpcount = 2;
	t2p->t2_maxdcount = vcp->vc_txmax;
	error = smb_t2_request(t2p);
	if (error)
		goto out;

	mdp = &t2p->t2_rdata;
	/*
	 * There could be no streams info associated with the item. You will find this with directory
	 * or items copied from FAT file systems. Nothing to process just get out.
	 */
	if (mdp->md_cur == NULL)
		goto out;
	
	do {
		stream_size = 0;
		if ((error = md_get_uint32le(mdp, &next)))
			goto out;
		used = 4;
		if ((error = md_get_uint32le(mdp, &nlen))) /* name length */
			goto out;
		used += 4;
		/* If this is the resource fork should we save this information */
		if ((error = md_get_uint64le(mdp, &stream_size))) /* stream size */
			goto out;
		used += 8;
		if ((error = md_get_uint64le(mdp, NULL))) /* allocated size */
			goto out;
		used += 8;
		/*
		 * Sanity check to limit DoS or buffer overrun attempts.
		 * Make sure the length is not bigger than our max buffers size.
		 */
		if (nlen > vcp->vc_txmax) {
			error = EBADRPC;
			goto out;
		}
		ctx.f_name = malloc(nlen, M_SMBFSDATA, M_WAITOK);
		if ((error = md_get_mem(mdp, ctx.f_name, nlen, MB_MSYSTEM)))
			goto out;
		used += nlen;

		/* ignore a trailing null, not that we expect them */
		if (SMB_UNICODE_STRINGS(vcp)) {
			if (nlen > 1 && !ctx.f_name[nlen - 1]
				     && !ctx.f_name[nlen - 2])
				nlen -= 2;
		} else {
			if (nlen && !ctx.f_name[nlen - 1])
				nlen -= 1;
		}
		ctx.f_nmlen = nlen;
		smbfs_fname_tolocal(&ctx); /* converts from UCS2LE */
		/*
		 * We should now have a name in the form : <foo> :$DATA Where <foo> is 
		 * UTF-8 w/o null termination. If it isn't in that form we want to LOG it 
		 * and skip it. Note we want to skip w/o logging the "data fork" entry,
		 * which is simply ::$DATA Otherwise we want to uiomove out <foo> with a null added.
		 */
		if (smbfs_smb_undollardata(np, &ctx)) {
			char *s;

			/* the "+ 1" skips over the leading colon */
			s = ctx.f_name + 1;
			
			/* Check for special case streams (resource fork and finder info */
			if ((ctx.f_nmlen >= sizeof(SFM_RESOURCEFORK_NAME)) && 
				(!strncasecmp(s, SFM_RESOURCEFORK_NAME, sizeof(SFM_RESOURCEFORK_NAME)))) {
				stype |= kResourceFrk;
				/* We always get the resource fork size and cache it here. */
				lck_mtx_lock(&np->rfrkMetaLock);
				np->rfrk_size = stream_size;
				nanotime(&ts);
				np->rfrk_cache_timer = ts.tv_sec;
				lck_mtx_unlock(&np->rfrkMetaLock);
				/* 
				 * The Resource fork and Finder info names are special and get translated between stream names and 
				 * extended attribute names. In this case we need to make sure the correct name gets used. So we are
				 * looking for a specfic stream use its stream name otherwise use its extended attribute name.
				 */
				if ((uio == NULL) && strmname && (sizep == NULL))
					s = SFM_RESOURCEFORK_NAME;
				else 
					s = XATTR_RESOURCEFORK_NAME;
					
				ctx.f_nmlen = strlen(s) + 1;
				
			} else if ((ctx.f_nmlen >= sizeof(SFM_FINDERINFO_NAME)) && 
					(!strncasecmp(s, SFM_FINDERINFO_NAME, sizeof(SFM_FINDERINFO_NAME)))) {
				stype |= kFinderInfo;
				/* 
				 * The Resource fork and Finder info names are special and get translated between stream names and 
				 * extended attribute names. In this case we need to make sure the correct name gets used. So we are
				 * looking for a specfic stream use its stream name otherwise use its extended attribute name.
				 */
				if ((uio == NULL) && strmname && (sizep == NULL))
					s = SFM_FINDERINFO_NAME;
				else 
					s = XATTR_FINDERINFO_NAME;
				
				ctx.f_nmlen = strlen(s) + 1;
			 }

			/*
			 * Depending on what is passed in we handle the data in two different ways.
			 *	1. If they have a uio then just put all the stream names into the uio buffer.
			 *	2. If they pass in a stream name then they just want the size of that stream.
			 *
			 * NOTE: If there is nothing in the stream we will not return it in the list. This
			 *       allows us to hide empty streams from copy engines. 
			 *
			 * We never return SFM_DESKTOP_NAME or SFM_IDINDEX_NAME streams.
			 */
			if (( (ctx.f_nmlen >= sizeof(SFM_DESKTOP_NAME)) && 
				(!strncasecmp(s, SFM_DESKTOP_NAME, sizeof(SFM_DESKTOP_NAME)))) ||
				((ctx.f_nmlen >= sizeof(SFM_IDINDEX_NAME)) && 
				(!strncasecmp(s, SFM_IDINDEX_NAME, sizeof(SFM_IDINDEX_NAME))))) {
				/*  Is this a SFM Volume */
				if (strmname && (!strncasecmp(SFM_DESKTOP_NAME, strmname, sizeof(SFM_DESKTOP_NAME)))) {
					foundStream = TRUE;
					goto out;
				}
				goto skipentry;
			} else if (uio && stream_size)
				uiomove(s, ctx.f_nmlen, uio);
			else if (strmname && strmsize) {
				/* They are looking for a specific stream name */ 
				nlen = strlen(strmname);
				if ((ctx.f_nmlen >= nlen) && (!strncasecmp(s, strmname, nlen))) {
					*strmsize = stream_size;
					foundStream = TRUE;
				}
			}
			/* 
			 * They could just want to know the buffer size they will need when requesting a list.
			 * This has several problem, but we cannot solve them all here. First someone can
			 * create a stream/EA between this call and the one they make to get the data. Second
			 * this will cause an extra round of traffic. We could cache all of this, but how long would 
			 * we keep this information around. Could require a large buffer.
			 */
			if (sizep && stream_size)
				*sizep += ctx.f_nmlen;
		}
		
skipentry:		
		free(ctx.f_name, M_SMBFSDATA);
		ctx.f_name = NULL;
		/* 
		 * Next should be the offset to the next entry. We have already move into 
		 * the buffer used bytes. So now need to move pass any remaining pad bytes. 
		 * So if the value next is larger than the value used, then we need to move that many 
		 * more bytes into the buffer. If that value is larger than our buffer get out.
		 */
		if (next > used) {
			next -= used;
			if (next > vcp->vc_txmax) {
				error = EBADRPC;
				goto out;
			}
			md_get_mem(mdp, NULL, next, MB_MSYSTEM);
		}
	} while (next && !error);

	/* If we searched the entire list and did not find a finder info stream, then reset the cache timer. */
	if ((stype & kFinderInfo) != kFinderInfo) {
		bzero(np->finfo, sizeof(np->finfo));	/* Negative cache the Finder Info */
		nanotime(&ts);
		np->finfo_cache = ts.tv_sec;
	}
	/* If we searched the entire list and did not find a resource stream, then reset the cache timer. */
	if ((stype & kResourceFrk) != kResourceFrk) {
		lck_mtx_lock(&np->rfrkMetaLock);
		nanotime(&ts);
		np->rfrk_size = 0;	/* Negative cache the ressource fork */
		np->rfrk_cache_timer = ts.tv_sec;
		lck_mtx_unlock(&np->rfrkMetaLock);
	}

out:;

	if (ctx.f_name)
		free(ctx.f_name, M_SMBFSDATA);
	smb_t2_done(t2p);
	
	if (foundStream == FALSE)	/* We did not find the stream we were looking for */
		error = ENOATTR;
	return (error);
}

/*
 * The SMB_QFS_POSIX_WHOAMI allows us to find out who the server thinks we are
 * and what groups we are in. It can also return the list of SIDs, but currently
 * we ignore those. Maybe in the future.
 */
static int smbfs_unix_whoami(struct smb_share *ssp, struct smb_cred *scrp)
{
	struct smbmount *smp = ssp->ss_mount;
	struct smb_t2rq *t2p;
	struct mbchain *mbp;
	struct mdchain *mdp;
	int error;
	int ii;
	u_int64_t other_gid;

	
	error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_QUERY_FS_INFORMATION, scrp, &t2p);
	if (error)
		return error;
	
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	mb_put_uint16le(mbp, SMB_QFS_POSIX_WHOAMI);
	t2p->t2_maxpcount = 4;
	t2p->t2_maxdcount = SSTOVC(ssp)->vc_txmax;
	error = smb_t2_request(t2p);
	if (error) {
		smb_t2_done(t2p);
		return error;
	}
	mdp = &t2p->t2_rdata;
	/* Currently only tells us if we logged in as guest, we should already know this by now */
	md_get_uint32le(mdp,  NULL); 	/* Mapping flags, currently only used for guest */
	md_get_uint32le(mdp,  NULL); /* Mask of valid mapping flags */
	md_get_uint64le(mdp,  &smp->ntwrk_uid);	/* Primary user ID */
	md_get_uint64le(mdp,  &smp->ntwrk_gid);	/* Primary group ID */
	md_get_uint32le(mdp,  &smp->ntwrk_cnt_gid); /* number of supplementary GIDs */
	md_get_uint32le(mdp,  NULL); /* number of SIDs */
	md_get_uint32le(mdp,  NULL); /* SID list byte count */
	md_get_uint32le(mdp,  NULL); /* Reserved (should be zero) */
	SMBDEBUG("uid = %llx gid = %llx cnt  = %d\n", smp->ntwrk_uid, smp->ntwrk_gid, smp->ntwrk_cnt_gid);
	MALLOC(smp->ntwrk_gids, u_int64_t *, sizeof(u_int64_t) * smp->ntwrk_cnt_gid, M_TEMP, M_WAITOK);

	for (ii = 0; ii < smp->ntwrk_cnt_gid; ii++) {
		md_get_uint64le(mdp,  &other_gid);
		smp->ntwrk_gids[ii] = other_gid;
		SMBDEBUG("other_gid[%d] = %llx\n", ii, smp->ntwrk_gids[ii]);
	}
	
	smb_t2_done(t2p);
	return 0;
}

/*
 * If this is a UNIX server then get its capiblities
 */
static void smbfs_unix_qfsattr(struct smb_share *ssp, struct smb_cred *scrp)
{
	struct smb_t2rq *t2p;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int16_t majorv;
	u_int16_t minorv;
	u_int64_t cap;
	int error;
	
	error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_QUERY_FS_INFORMATION, scrp, &t2p);
	if (error)
		return;
	
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	mb_put_uint16le(mbp, SMB_QFS_UNIX_INFO);
	t2p->t2_maxpcount = 4;
	t2p->t2_maxdcount = 12;
	error = smb_t2_request(t2p);
	if (error) {
		smb_t2_done(t2p);
		return;
	}
	mdp = &t2p->t2_rdata;
	md_get_uint16le(mdp,  &majorv);
	md_get_uint16le(mdp, &minorv);
	md_get_uint64le(mdp, &cap);
	SMBWARNING("version %x.%x cap = %llx\n", majorv, minorv, cap);
	
#ifdef SUPPORT_POSIX_LOCKS
#else // SUPPORT_POSIX_LOCKS
	/* Currently we do not support POSIX LOCKS, so turn them off */
	cap &= ~CIFS_UNIX_FCNTL_LOCKS_CAP;
#endif // SUPPORT_POSIX_LOCKS

	smb_t2_done(t2p);
	/* See if the server supports the who am I operation */ 
	if (cap & CIFS_UNIX_POSIX_PATH_OPERATIONS_CAP) {
		error = smbfs_unix_whoami(ssp, scrp);
		if (error)
			cap &= ~CIFS_UNIX_POSIX_PATH_OPERATIONS_CAP;
	}
	/* We now have the servers unix capiblities store them away */
	UNIX_CAPS(SSTOVC(ssp)) = cap;
}

/* 
 * Since the first thing we do is set the default values there is no longer 
 * any reason to return an error for this routine. Some servers may not support
 * this call. We should not fail the mount just because they do not support this
 * call.
 */
PRIVSYM void
smbfs_smb_qfsattr(struct smb_share *ssp, struct smb_cred *scrp)
{
	struct smb_t2rq *t2p;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int32_t nlen;
	int error;
	u_int8_t *fs_name;	/* will malloc whatever the size is */
	int fs_nmlen;		/* The sized malloced for fs_name */
	struct smbfs_fctx	ctx;

	/* Start with the default values */
	ssp->ss_fstype = SMB_FS_FAT;	/* default to FAT File System */
	ssp->ss_attributes = 0;
	/* Once we drop Windows for Workgroup support we can just default to 255 */
	ssp->ss_maxfilenamelen = (SMB_DIALECT(SSTOVC(ssp)) >= SMB_DIALECT_LANMAN2_0) ?  255 : 12;
	
	error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_QUERY_FS_INFORMATION,
			     scrp, &t2p);
	if (error)
		return;

	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	mb_put_uint16le(mbp, SMB_QFS_ATTRIBUTE_INFO);
	t2p->t2_maxpcount = 4;
	t2p->t2_maxdcount = 4 * 3 + 512;
	error = smb_t2_request(t2p);
	if (error) {
		smb_t2_done(t2p);
		SMBWARNING("smb_t2_request error %d\n", error);
		return;
	}
	mdp = &t2p->t2_rdata;
	md_get_uint32le(mdp,  &ssp->ss_attributes);
	md_get_uint32le(mdp, &ssp->ss_maxfilenamelen);
	md_get_uint32le(mdp, &nlen);	/* fs name length */
	if (ssp->ss_fsname == NULL && nlen) {
		ctx.f_ssp = ssp;
		ctx.f_name = malloc(nlen, M_SMBFSDATA, M_WAITOK);
		md_get_mem(mdp, ctx.f_name, nlen, MB_MSYSTEM);
		/*
		 * Just going from memory, I believe this is really just a
		 * WCHAR not a STRING value. I know that both Windows 98
		 * and SNAP return it as WCHAR and neither supports 
		 * UNICODE. So if they do not support UNICODE then lets 
		 * do some test and see if we can get the file system name.
		 */
		if (!SMB_UNICODE_STRINGS(SSTOVC(ssp)) && 
			((nlen > 1) && (ctx.f_name[1] == 0))) {
			int ii;
			char *instr = ctx.f_name;
			char *outstr = ctx.f_name;
			
			for (ii=0; ii < nlen; ii += 2) {
				*outstr++ = *instr++;
				instr++;
			}
			ctx.f_nmlen = nlen / 2;
		}
		else {
			ctx.f_nmlen = nlen;
			smbfs_fname_tolocal(&ctx);
		}
		fs_nmlen = ctx.f_nmlen+1;
		fs_name = malloc(fs_nmlen, M_SMBSTR, M_WAITOK);
		bcopy(ctx.f_name, fs_name, ctx.f_nmlen);
		fs_name[ctx.f_nmlen] = '\0';
		ssp->ss_fsname = (char *)fs_name;
		free(ctx.f_name, M_SMBFSDATA);
		
		/*
		 * Let's start keeping track of the file system type. Most
		 * things we need to do differently really depend on the
		 * file system type. As an example we know that FAT file systems
		 * do not update the modify time on drectories.
		 */
		if (strncmp((char *)fs_name, "FAT", fs_nmlen) == 0)
			ssp->ss_fstype = SMB_FS_FAT;
		else if (strncmp((char *)fs_name, "FAT12", fs_nmlen) == 0)
			ssp->ss_fstype = SMB_FS_FAT;
		else if (strncmp((char *)fs_name, "FAT16", fs_nmlen) == 0)
			ssp->ss_fstype = SMB_FS_FAT;
		else if (strncmp((char *)fs_name, "FAT32", fs_nmlen) == 0)
			ssp->ss_fstype = SMB_FS_FAT;
		else if (strncmp((char *)fs_name, "CDFS", fs_nmlen) == 0)
			ssp->ss_fstype = SMB_FS_CDFS;
		else if (strncmp((char *)fs_name, "UDF", fs_nmlen) == 0)
			ssp->ss_fstype = SMB_FS_UDF;
		else if (strncmp((char *)fs_name, "NTFS", fs_nmlen) == 0)
			ssp->ss_fstype = SMB_FS_NTFS_UNKNOWN;	/* Could be lying */

		SMBWARNING("(fyi) share '%s', attr 0x%x, maxfilename %d\n",
			 ssp->ss_fsname, ssp->ss_attributes, ssp->ss_maxfilenamelen);
		/*
		 * NT4 will not return the FILE_NAMED_STREAMS bit in the ss_attributes
		 * even though they support streams. So if its a NT4 server and a
		 * NTFS file format then turn on the streams flag.
		 */
		 if ((SSTOVC(ssp)->vc_flags & SMBV_NT4) && (ssp->ss_fstype & SMB_FS_NTFS_UNKNOWN))
			ssp->ss_attributes |= FILE_NAMED_STREAMS;
		 /* 
		  * The server says they support streams and they say they are NTFS. So mark
		  * the subtype as NTFS. Remember a lot of non Windows servers pretend
		  * their NTFS so they can support ACLs, but they aren't really because they have
		  * no stream support. This allows us to tell the difference.
		  */
		 if ((ssp->ss_fstype == SMB_FS_NTFS_UNKNOWN) && (ssp->ss_attributes & FILE_NAMED_STREAMS))
			 ssp->ss_fstype = SMB_FS_NTFS;	/* Real NTFS Volume */
		 else if ((ssp->ss_fstype == SMB_FS_NTFS_UNKNOWN) && (UNIX_SERVER(SSTOVC(ssp))))
			 ssp->ss_fstype = SMB_FS_NTFS_UNIX;	/* UNIX system lying about being NTFS */
		 /* Some day mark it as being Mac OS X */
	}
	smb_t2_done(t2p);
	/* Its a unix server see if it supports any of the extensions */
	if (UNIX_SERVER(SSTOVC(ssp)))
		smbfs_unix_qfsattr(ssp, scrp);
}

/*
 * This routine now supports two different info levels. The
 * SMB_QFS_SIZE_INFO info level is used if the NT_CAPS is 
 * set. This allows us to return large volume sizes.
 *
 *	SMB_QFS_SIZE_INFO
 *		Total number of allocated units as a 64 bit value
 *		Number of free units as a 64 bit value
 *		Number of sectors in each unit as a 32 bit value
 *		Number of bytes in each sector as a 32 bit value
 *
 *	SMB_QFS_ALLOCATION
 *		File system id not used
 *		Number of sectors in each unit as a 32 bit value
 *		Total number of allocated units as a 32 bit value
 *		Number of free units as a 32 bit value
 *		Number of bytes in each sector as a 16 bit value
 */
int
smbfs_smb_statfs(struct smb_share *ssp, struct vfsstatfs *sbp, struct smb_cred *scrp)
{
	struct smb_vc *vcp = SSTOVC(ssp);
	struct smb_t2rq *t2p;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int16_t bsize;
	u_int32_t bpu, bsize32;
	u_int32_t units, funits;
	u_int64_t s, t, f;
	int error;
	int xmax;

	error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_QUERY_FS_INFORMATION,
	    scrp, &t2p);
	if (error)
		return error;
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	if (SSTOVC(ssp)->vc_sopt.sv_caps & SMB_CAP_NT_SMBS) {
		mb_put_uint16le(mbp, SMB_QFS_SIZE_INFO);
		t2p->t2_maxpcount = 4;
		/* The call returns two 64 bit values and two 32 bit value */
		t2p->t2_maxdcount = (8 * 2) + (4 * 2);
	}
	else {
		mb_put_uint16le(mbp, SMB_QFS_ALLOCATION);
		t2p->t2_maxpcount = 4;
		/* The call returns four 32 bit values and one 16 bit value */
		t2p->t2_maxdcount = 4 * 4 + 2;
	}
	error = smb_t2_request(t2p);
	if (error) {
		smb_t2_done(t2p);
		return error;
	}
	mdp = &t2p->t2_rdata;
	/* Now depending on the call retrieve the correct inforamtion. */
	if (SSTOVC(ssp)->vc_sopt.sv_caps & SMB_CAP_NT_SMBS) {
		md_get_uint64le(mdp, &t); /* Total number of allocated units */
		md_get_uint64le(mdp, &f); /* Number of free units */
		md_get_uint32le(mdp, &bpu); /* Number of sectors in each unit */
		md_get_uint32le(mdp, &bsize32);	/* Number of bytes in a sector */
		s = bsize32;
		s *= bpu;
	}
	else {
		md_get_uint32(mdp, NULL); /* fs id */
		md_get_uint32le(mdp, &bpu); /* Number of sectors in each unit */
		md_get_uint32le(mdp, &units); /* Total number of allocated units */
		md_get_uint32le(mdp, &funits); /* Number of free units */
		md_get_uint16le(mdp, &bsize); /* Number of bytes in a sector */
		s = bsize;
		s *= bpu;
		t = units;
		f = funits;
	}
	/*
	 * Don't allow over-large blocksizes as they determine
	 * Finder List-view size granularities.  On the other
	 * hand, we mustn't let the block count overflow the
	 * 31 bits available.
	 */
	while (s > 16 * 1024) {
		if (t > LONG_MAX)
			break;
		s /= 2;
		t *= 2;
		f *= 2;
	}
	while (t > LONG_MAX) {
		t /= 2;
		f /= 2;
		s *= 2;
	}
	sbp->f_bsize = s;	/* fundamental file system block size */
	sbp->f_blocks= t;	/* total data blocks in file system */
	sbp->f_bfree = f;	/* free blocks in fs */
	sbp->f_bavail= f;	/* free blocks avail to non-superuser */
	sbp->f_files = (-1);	/* total file nodes in file system */
	sbp->f_ffree = (-1);	/* free file nodes in fs */
	smb_t2_done(t2p);
	
	/* Done with the network stuff now get the iosize, this code was moved from smbfs_vfs_getattr to here */
	
	/*
	 *  Now get the iosize, this code was down in smbfs_vfs_getattr, but now we do it here
	 *
	 * The Finder will in general use the f_iosize as its i/o buffer size.  We want to give it the 
	 * largest size which is less than the UBC/UPL limit (SMB_IOMAX) but is also a multiple of our
	 * maximum xfer size in a single smb. If possible we would like for every offset to be on a PAGE_SIZE 
	 * boundary. So the way to force that is make sure the f_iosize always ends up on a PAGE_SIZE boundary. 
	 * Our second goal is to use as many smb xfer as possible, but also have the f_iosize end on a xfer 
	 * boundary. We can do this in all cases, but they smb xfer must be on a 1K boundary.
	 * 
	 * NOTE: Remember that if the server sets the SMB_CAP_LARGE bit then we have complete control of the 
	 * xmax size. So our goal here is to work with those systems and do the best we can for others. So currently 
	 * we we have two different numbers for SMB_CAP_LARGE servers, 60K for Windows and 126K for the others. Since
	 * this will affect most systems we are dealing with make sure are numbers always work in those two cases. For
	 * all other cases just do the best we can do. 
	 *
	 * NOTE: We always make sure that vc_rxmax and vc_wxmax are on a 1k boundary!
	 */
	
	xmax = max(vcp->vc_rxmax, vcp->vc_wxmax);
	/* 
	 * Now we want to make sure it will land on both a PAGE_SIZE boundary and a smb xfer size boundary. So 
	 * first mod the xfer size by the page size, then subtract that from page size. This will give us the extra 
	 * amount that will be needed to get it on a page boundary. Now divide the page size by this amount.
	 * This will give us the number of xmax it will take to make f_iosize land on a  page size and xfer boundary. 
	 */
	xmax = (PAGE_SIZE / (PAGE_SIZE - (xmax % PAGE_SIZE))) * xmax;
	if (xmax > SMB_IOMAX)
		sbp->f_iosize = SMB_IOMAX;
	else
		sbp->f_iosize = (SMB_IOMAX/xmax) * xmax;
	/* 
	 * Examples:
	 * Windows (xfer is 60K) - f_iosize
	 * (4 / (4 - (60 % 4))) * 60 = 60 
	 * (1024 / 60) * 60 = 1020
	 * (1020 / PAGE_SIZE) = exactly 255 pages 
	 * (1020 / 60) = exactly 17 xfer 
	 * 
	 * SAMBA (xfer is 126K) - f_iosize
	 * (4 / (4 - (126 % 4))) * 126 = 252 
	 * (1024 / 252) * 252 = 1008
	 * (1008 / PAGE_SIZE) = exactly 252 pages 
	 * (1020 / 126) = exactly 8 xfer 
	 * SAMBA - f_iosize broken uses 63K on reads
	 * (1020 / 63) = exactly 16 xfer 
	 */
	
	return 0;
}

PRIVSYM int smbfs_smb_seteof(struct smb_share *ssp, u_int16_t fid, u_int64_t newsize, struct smb_cred *scrp)
{
	struct smb_t2rq *t2p;
	struct smb_vc *vcp = SSTOVC(ssp);
	struct mbchain *mbp;
	int error;

	error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_SET_FILE_INFORMATION,
	    scrp, &t2p);
	if (error)
		return error;
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	mb_put_mem(mbp, (caddr_t)&fid, 2, MB_MSYSTEM);
	if (vcp->vc_sopt.sv_caps & SMB_CAP_INFOLEVEL_PASSTHRU)
		mb_put_uint16le(mbp, SMB_SFILEINFO_END_OF_FILE_INFORMATION);
	else
		mb_put_uint16le(mbp, SMB_SFILEINFO_END_OF_FILE_INFO);
	mb_put_uint32le(mbp, 0); /* XXX should be 16 not 32(?) */
	mbp = &t2p->t2_tdata;
	mb_init(mbp);
	mb_put_uint64le(mbp, newsize);
	mb_put_uint32le(mbp, 0);			/* padding */
	mb_put_uint16le(mbp, 0);
	t2p->t2_maxpcount = 2;
	t2p->t2_maxdcount = 0;
	error = smb_t2_request(t2p);
	smb_t2_done(t2p);
	return error;
}

/*
 * smbfs_smb_markfordelete
 *
 * We have an open file that they want to delete. This call will tell the 
 * server to delete the file when the last close happens. Currenly we know that 
 * XP, Windows 2000 and Windows 2003 support this call. SAMBA does support the
 * call, but currently has a bug that prevents it from working.
 */
static int
smbfs_smb_markfordelete(struct smbnode *np, struct smb_cred *scrp, 
			u_int16_t *infid)
{
	struct smb_t2rq *t2p;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct smb_vc *vcp = SSTOVC(ssp);
	struct mbchain *mbp;
	int error, cerror;
	u_int16_t fid = (infid) ? *infid : 0;

	if (!(vcp->vc_sopt.sv_caps & SMB_CAP_NT_SMBS))
		return (ENOTSUP);
		
	error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_SET_FILE_INFORMATION,
			     scrp, &t2p);
	if (error)
		return error;
	if (!infid) {
		/* 
		 * See if we can open the item with delete access. Requesting 
		 * delete access can mean more then just requesting to delete
		 * the file. It is used to mark the item for deletion on close
		 * and for renaming an open file. If I find any other uses for 
		 * it I will add them to this comment.  
		 */
		error = smbfs_smb_tmpopen(np, STD_RIGHT_DELETE_ACCESS, scrp, &fid);
		if (error)
			goto exit;
	}
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	mb_put_mem(mbp, (caddr_t)&fid, 2, MB_MSYSTEM);
	if (vcp->vc_sopt.sv_caps & SMB_CAP_INFOLEVEL_PASSTHRU)
		mb_put_uint16le(mbp, SMB_SFILEINFO_DISPOSITION_INFORMATION);
	else
		mb_put_uint16le(mbp, SMB_SFILEINFO_DISPOSITION_INFO);
	mb_put_uint32le(mbp, 0);
	mbp = &t2p->t2_tdata;
	mb_init(mbp);
	mb_put_uint8(mbp, 1);
	t2p->t2_maxpcount = 2;
	t2p->t2_maxdcount = 0;
	error = smb_t2_request(t2p);
exit:
	if (!infid && fid) {
		cerror = smbfs_smb_tmpclose(np, fid, scrp);
		if (cerror) {
			SMBWARNING("error %d closing fid %d\n", cerror, fid);
		}
	}
	smb_t2_done(t2p);
	return error;
}

int
smbfs_smb_t2rename(struct smbnode *np, const char *tname, int tnmlen, 
	struct smb_cred *scrp, int overwrite, u_int16_t *infid)
{
	struct smb_t2rq *t2p;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct smb_vc *vcp = SSTOVC(ssp);
	struct mbchain *mbp;
	int32_t len, *ucslenp;
	int error, cerror;
	u_int16_t fid = (infid) ? *infid : 0;
	char convbuf[1024];

	if (!(vcp->vc_sopt.sv_caps & SMB_CAP_INFOLEVEL_PASSTHRU))
		return (ENOTSUP);
	/*
	 * Rember that smb_t2_alloc allocates t2p. We need to call
	 * smb_t2_done to free it.
	 */
	error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_SET_FILE_INFORMATION,
			     scrp, &t2p);
	if (error)
		return error;
		
	if (!infid) {
		/* 
		 * See if we can open the item with delete access. Requesting 
		 * delete access can mean more then just requesting to delete
		 * the file. It is used to mark the item for deletion on close
		 * and for renaming an open file. If I find any other uses for 
		 * it I will add them to this comment.  
		 */
		error = smbfs_smb_tmpopen(np, STD_RIGHT_DELETE_ACCESS, scrp, &fid);
		if (error)
			goto exit;
	}
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	mb_put_mem(mbp, (caddr_t)&fid, 2, MB_MSYSTEM);
	mb_put_uint16le(mbp, SMB_SFILEINFO_RENAME_INFORMATION);
	mb_put_uint16le(mbp, 0); /* reserved, nowadays */
	mbp = &t2p->t2_tdata;
	mb_init(mbp);
	mb_put_uint32le(mbp, overwrite);
	mb_put_uint32le(mbp, 0); /* Root fid, not used */
	ucslenp = (int32_t *)mb_reserve(mbp, sizeof(int32_t));
	len = smb_strtouni((u_int16_t *)convbuf, tname, tnmlen,
			   UTF_PRECOMPOSED|UTF_NO_NULL_TERM);
	*ucslenp = htolel(len); 
	error = mb_put_mem(mbp, convbuf, len, MB_MSYSTEM);
	if (error)
		goto exit;
	error = mb_put_uint16le(mbp, 0);
	if (error)
		goto exit;
	t2p->t2_maxpcount = 2;
	t2p->t2_maxdcount = 0;
	error = smb_t2_request(t2p);
exit:;  
	if (!infid && fid) {
		cerror = smbfs_smb_tmpclose(np, fid, scrp);
		if (cerror) {
			SMBWARNING("error %d closing fid %d\n", cerror, fid);
		}
	}
	smb_t2_done(t2p);
	return (error);
}

/* format of "random" names and next name to try */
/* (note: shouldn't exceed size of s_name) that includes the null byte. */
static char sillyrename_name[] = ".smbdeleteAAA%04x4.4";

/*
 * smbfs_delete_openfile
 *
 * We have an open file that they want to delete. Use the NFS silly rename
 * trick, but try to do better than NFS. The picking of the name came from the
 * NFS code. So we first open the file for deletion. Now come up with a new 
 * name and rename the file. Make the file hiddenif we cab=n. Now lets mark
 * it for deletion and close the file. If the rename fails then the whole call 
 * should fail. If the mark for deletion call fails just set a flag on the 
 * vnode and delete it when we close. 
 */
int
smbfs_delete_openfile(struct smbnode *dnp, struct smbnode *np, struct smb_cred *scrp)
{
	struct proc	*p = vfs_context_proc(scrp->scr_vfsctx);
	u_int16_t       fid = 0;
	int		error, cerror;
	char		s_name[32];	/* make sure that sillyrename_name will fit */
	long		s_namlen;
	int i, j, k;
	
	error = smbfs_smb_tmpopen(np, STD_RIGHT_DELETE_ACCESS, scrp, &fid);
	if (error)
		return(error);
	/* Get the first silly name */
	s_namlen = snprintf(s_name, sizeof(s_name), sillyrename_name, proc_pid(p));
	if (s_namlen >=  sizeof(s_name)) {
	    error = ENOENT;
	    goto out;
	 }
	/* Try rename until we get one that isn't there */
	i = j = k = 0;

	do {
		error = smbfs_smb_t2rename(np, s_name, s_namlen, scrp, 0, &fid);
		/* 
		 * SAMBA Bug: The above call will not work on a top level share.
		 * 
		 * The SAMBA code gets confused and tries to rename the file to
		 * oldfile/newfile? We fixed this for our Leopard version. Not 
		 * sure when that code will make it back into the SAMBA stream. 
		 * We still want to make this code work with old SAMBA servers, 
		 * so try again with the old rename call. SAMBA allows us to 
		 * rename a open file with this call, but not with delete 
		 * access. So close it, rename it and hide it.
		 */
		if ((error == ENOENT) && SMBTOV(dnp) && vnode_isvroot(SMBTOV(dnp))) {
			if (fid) {
				(void)smbfs_smb_tmpclose(np, fid, scrp);
				fid = 0;
			}
			error = smbfs_smb_rename(np, dnp, s_name, s_namlen, 
						scrp);
			if (! error) {
				/* ignore any errors return from hiding the item */
				(void)smbfs_smb_hideit(dnp, s_name, s_namlen, scrp);
				np->n_flag |= NDELETEONCLOSE;
			}
			if (!error)
				goto out;
		}
		else if (error && (error != EEXIST)) {	/* They return an error we were not expecting */
			struct smbfattr fap;
			
			/* 
			 * They return an error we did not expect. If the silly name file exist then
			 * we want to keep trying. So do a look up, if they say it exist keep trying
			 * otherwise just get out nothing else to do.
			 */
			if (smbfs_smb_query_info(dnp, s_name, s_namlen, &fap, scrp) == 0)
				error = EEXIST;	/* Keep Trying */
			else break;
			
		}
		/* 
		 * The file name already exist try another one. This code was taken from NFS. NFS 
		 * tested by doing a lookup, we use the rename call, see above for how we handle 
		 * strange errors. If the rename call fails keep trying till we run out of names.
		 */
		if (error) {
			if (s_name[10]++ >= 'z')
				s_name[10] = 'A';
			if (++i > ('z' - 'A' + 1)) {
				i = 0;
				if (s_name[11]++ >= 'z')
					s_name[11] = 'A';
				if (++j > ('z' - 'A' + 1)) {
					j = 0;
					if (s_name[12]++ >= 'z')
						s_name[12] = 'A';
					if (++k > ('z' - 'A' + 1)) {
						error = EINVAL;
					}
				}
			}
		}
	}while (error == EEXIST);
	
	if (error)
		goto out;
		/* ignore any errors return from hiding the item */
	(void)smbfs_smb_hideit(dnp, s_name, s_namlen, scrp);
	
	cerror = smbfs_smb_markfordelete(np, scrp, &fid);
	if (cerror) {	/* We will have to do the delete ourself! Could be SAMBA */
		np->n_flag |= NDELETEONCLOSE;
	}
	
out:;  
	if (fid) {
		cerror = smbfs_smb_tmpclose(np, fid, scrp);
		if (cerror) {
			SMBWARNING("error %d closing fid %d\n", cerror, fid);
		}
	}
	if (!error) {
		smb_vhashrem(np);
		if (np->n_name)
			smbfs_name_free(np->n_name);
		np->n_name = smbfs_name_alloc((u_char *)s_name, s_namlen);
		np->n_nmlen = s_namlen;
		np->n_flag |= NMARKEDFORDLETE;
	}
	else error = EBUSY;
	return(error);
}

/*
 * This routine will send a flush across the wire to the server. This is an expensive
 * operation that should only be done when the user request it. Windows 95/98/Me servers
 * do not handle the set eof call very well. So we will send a flush after a Set EOF
 * call to a Windows 95/98/Me server.
 */ 
PRIVSYM int smbfs_smb_flush(struct smbnode *np, struct smb_cred *scrp)
{
	struct smb_share *ssp = np->n_mount->sm_share;
	struct smb_rq rq, *rqp = &rq;
	struct mbchain *mbp;
	int error;
	u_int16_t fid = 0;

	/* Flush if we have written to the file or done a set eof. */
	if (!(np->n_flag & NFLUSHWIRE))
		return (0);
	if ((np->f_refcnt <= 0) || (!SMBTOV(np)) || (!vnode_isreg(SMBTOV(np))))
		return 0; /* not a regular open file */

	/* Before trying the flush see if the file needs to be reopened */
	error = smbfs_smb_reopen_file(np, scrp->scr_vfsctx);
	if (error) {
		SMBDEBUG(" %s waiting to be revoked\n", np->n_name);
	    return(error);
	}
	
	/* See if the file is opened for write access */
	if (smbfs_findFileRef(SMBTOV(np), scrp->pid, kAccessWrite, kCheckDenyOrLocks, 0, 0, NULL, &fid)) {
		fid = np->f_fid;	/* Nope use the shared fid */
		if ((fid == 0) || ((np->f_accessMode & kAccessWrite) != kAccessWrite))
			return(0);	/* Nothing to do here get out */
	}
	error = smb_rq_init(rqp, SSTOCP(ssp), SMB_COM_FLUSH, scrp);
	if (error)
		return (error);
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_mem(mbp, (caddr_t)&fid, 2, MB_MSYSTEM);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	/* 
	 * Flushed failed on a reconnect. The server will flush the file when it
	 * closes the file after the connection goes down. Ignore the error in this case.
	 */
	if ((error == EBADF) && (rqp->sr_flags & SMBR_REXMIT))
		error = 0;
	smb_rq_done(rqp);
	if (!error)
		np->n_flag &= ~NFLUSHWIRE;
	return (error);
}

int
smbfs_smb_setfsize(struct smbnode *np, u_int16_t fid, u_int64_t newsize, vfs_context_t vfsctx)
{
	struct smb_share *ssp = np->n_mount->sm_share;
	struct smb_rq rq, *rqp = &rq;
	struct mbchain *mbp;
	int error;
	struct smb_cred scred;

	smb_scred_init(&scred, vfsctx);
	/* Windows 98 does not support this call so don't even try it. */
	if (SSTOVC(ssp)->vc_sopt.sv_caps & SMB_CAP_NT_SMBS) {
		if (!smbfs_smb_seteof(ssp, fid, newsize, &scred)) {
			np->n_flag |= (NFLUSHWIRE | NATTRCHANGED);
			return (0);
		}
	}
	/*
	 * For servers that do not support the SMB_CAP_NT_SMBS we need to
	 * do a zero length write as per the SMB Core Reference. 
	 */
	if (newsize > UINT32_MAX)
		return (EFBIG);

	error = smb_rq_init(rqp, SSTOCP(ssp), SMB_COM_WRITE, &scred);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_mem(mbp, (caddr_t)&fid, 2, MB_MSYSTEM);
	mb_put_uint16le(mbp, 0);
	mb_put_uint32le(mbp, newsize);
	mb_put_uint16le(mbp, 0);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint8(mbp, SMB_DT_DATA);
	mb_put_uint16le(mbp, 0);
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	smb_rq_done(rqp);
	np->n_flag |= NATTRCHANGED | NFLUSHWIRE;
	/* Windows 98 requires a flush when setting the eof, so lets just do it here. */
	if (smbfs_smb_flush(np, &scred))
		np->n_flag |= NFLUSHWIRE;	/* Didn't work, Try again later? */
	return error;
}


int
smbfs_smb_query_info(struct smbnode *np, const char *name, int len,
		     struct smbfattr *fap, struct smb_cred *scrp)
{
	struct smb_rq rq, *rqp = &rq;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int8_t wc;
	int error;
	u_int16_t wattr;
	u_int32_t lint;

	error = smb_rq_init(rqp, SSTOCP(ssp), SMB_COM_QUERY_INFORMATION, scrp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint8(mbp, SMB_DT_ASCII);
	do {
		error = smbfs_fullpath(mbp, SSTOVC(ssp), np, name, &len, UTF_SFM_CONVERSIONS, '\\');
		if (error)
			break;
		smb_rq_bend(rqp);
		error = smb_rq_simple(rqp);
		if (error)
			break;
		smb_rq_getreply(rqp, &mdp);
		if (md_get_uint8(mdp, &wc) != 0 || wc != 10) {
			error = EBADRPC;
			break;
		}
		md_get_uint16le(mdp, &wattr);
		fap->fa_attr = wattr;
		fap->fa_vtype = (fap->fa_attr & SMB_FA_DIR) ? VDIR : VREG;
		/*
		 * Be careful using the time returned here, as
		 * with FAT on NT4SP6, at least, the time returned is low
		 * 32 bits of 100s of nanoseconds (since 1601) so it rolls
		 * over about every seven minutes!
		 */
		md_get_uint32le(mdp, &lint); /* specs: secs since 1970 */
		if (lint)	/* avoid bogus zero returns */
			smb_time_server2local(lint, SSTOVC(ssp)->vc_sopt.sv_tz,
					      &fap->fa_mtime);
		md_get_uint32le(mdp, &lint);
		fap->fa_size = lint;
		/* No allocation size here fake it, based on the data size and block size */
		lck_mtx_lock(&np->n_mount->sm_statfslock);
		if (np->n_mount->sm_statfsbuf.f_bsize)	/* Should never happen, but just to be safe */
			fap->fa_data_alloc = roundup(lint, np->n_mount->sm_statfsbuf.f_bsize);
		else
			fap->fa_data_alloc = fap->fa_size;
		lck_mtx_unlock(&np->n_mount->sm_statfslock);
	} while(0);
	smb_rq_done(rqp);
	return error;
}

/*
 * Set DOS file attributes. mtime should be NULL for dialects above lm10
 */
PRIVSYM int
smbfs_smb_setpattr(struct smbnode *np, const char *name, int len,
		   u_int16_t attr, struct timespec *mtime,
		   struct smb_cred *scrp)
{
	struct smb_rq rq, *rqp = &rq;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct mbchain *mbp;
	long time;
	int error, svtz;

	error = smb_rq_init(rqp, SSTOCP(ssp), SMB_COM_SET_INFORMATION, scrp);
	if (error)
		return error;
	svtz = SSTOVC(ssp)->vc_sopt.sv_tz;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint16le(mbp, attr);
	if (mtime) {
		smb_time_local2server(mtime, svtz, &time);
	} else
		time = 0;
	mb_put_uint32le(mbp, time);		/* mtime */
	mb_put_mem(mbp, NULL, 5 * 2, MB_MZERO);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint8(mbp, SMB_DT_ASCII);
	do {
		error = smbfs_fullpath(mbp, SSTOVC(ssp), np, name, &len, UTF_SFM_CONVERSIONS, '\\');
		if (error)
			break;
		mb_put_uint8(mbp, SMB_DT_ASCII);
		if (SMB_UNICODE_STRINGS(SSTOVC(ssp))) {
			mb_put_padbyte(mbp);
			mb_put_uint8(mbp, 0);	/* 1st byte of NULL Unicode char */
		}
		mb_put_uint8(mbp, 0);
		smb_rq_bend(rqp);
		error = smb_rq_simple(rqp);
		if (error)
			break;
	} while(0);
	smb_rq_done(rqp);
	return error;
}

PRIVSYM int
smbfs_smb_hideit(struct smbnode *np, const char *name, int len,
		 struct smb_cred *scrp)
{
	struct smbfattr fa;
	int error;
	u_int16_t attr;

	error = smbfs_smb_query_info(np, name, len, &fa, scrp);
	attr = fa.fa_attr;
	if (!error && !(attr & SMB_FA_HIDDEN)) {
		attr |= SMB_FA_HIDDEN;
		error = smbfs_smb_setpattr(np, name, len, attr, NULL, scrp);
	}
	return (error);
}


PRIVSYM int
smbfs_smb_unhideit(struct smbnode *np, const char *name, int len,
		   struct smb_cred *scrp)
{
	struct smbfattr fa;
	u_int16_t attr;
	int error;

	error = smbfs_smb_query_info(np, name, len, &fa, scrp);
	attr = fa.fa_attr;
	if (!error && (attr & SMB_FA_HIDDEN)) {
		attr &= ~SMB_FA_HIDDEN;
		error = smbfs_smb_setpattr(np, name, len, attr, NULL, scrp);
	}
	return (error);
}

PRIVSYM int
smbfs_set_unix_info2(struct smbnode *np, struct timespec *crtime, struct timespec *mtime, struct timespec *atime, 
	u_int64_t fsize,  u_int32_t perms, u_int32_t FileFlags, u_int32_t FileFlagsMask, struct smb_cred *scrp)
{
	struct smb_t2rq *t2p;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct smb_vc *vcp = SSTOVC(ssp);
	struct mbchain *mbp;
	u_int64_t tm;
	int error, tzoff;
	
	error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_SET_PATH_INFORMATION, scrp, &t2p);
	if (error)
		return error;
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	mb_put_uint16le(mbp, SMB_SFILEINFO_UNIX_INFO2);
	mb_put_uint32le(mbp, 0);		/* MBZ */
	error = smbfs_fullpath(mbp, vcp, np, NULL, NULL, UTF_SFM_CONVERSIONS, '\\');
	if (error) {
		smb_t2_done(t2p);
		return error;
	}
	tzoff = vcp->vc_sopt.sv_tz;
	
	mbp = &t2p->t2_tdata;
	mb_init(mbp);
	/* File size */
	mb_put_uint64le(mbp, fsize);
	/* Number of blocks used on disk */
	tm = SMB_SIZE_NO_CHANGE;
	mb_put_uint64le(mbp, tm);

	/* Set the change time, not allowed */		
	mb_put_uint64le(mbp, 0);

	/* set the access time */	
	if (atime)
		smb_time_local2NT(atime, tzoff, &tm, FALSE);
	else 
	     tm = 0;
	mb_put_uint64le(mbp, tm);

	/* set the write/modify time */	
	if (mtime)
		smb_time_local2NT(mtime, tzoff, &tm, FALSE);
	else 
	     tm = 0;
	mb_put_uint64le(mbp, tm);

	/* Numeric user id for the owner */
	tm = SMB_UID_NO_CHANGE;
	mb_put_uint64le(mbp, tm);
	/* Numeric group id of owner */
	tm = SMB_GID_NO_CHANGE;
	mb_put_uint64le(mbp, tm);
	/* Enumeration specifying the file type */
	tm = SMB_DEFAULT_NO_CHANGE;
	mb_put_uint32le(mbp, tm);
	/* Major device number if type is device */
	tm = SMB_DEFAULT_NO_CHANGE;
	mb_put_uint64le(mbp, tm);
	/* Minor device number if type is device */
	tm = SMB_DEFAULT_NO_CHANGE;
	mb_put_uint64le(mbp, tm);
	/* This is a server-assigned unique id */
	tm = SMB_DEFAULT_NO_CHANGE;
	mb_put_uint64le(mbp, tm);
	/* Standard UNIX permissions */
	tm = perms;
	mb_put_uint64le(mbp, tm);
	/* Number of hard link */
	tm = SMB_DEFAULT_NO_CHANGE;
	mb_put_uint64le(mbp, tm);
	/* set the creation time */
	if (crtime)
		smb_time_local2NT(crtime, tzoff, &tm, FALSE);
	else 
	    tm = 0;
	mb_put_uint64le(mbp, tm);
	/* File flags enumeration */
	mb_put_uint32le(mbp, FileFlags);
	/* Mask of valid flags */
	mb_put_uint32le(mbp, FileFlagsMask);
	
	t2p->t2_maxpcount = 24;
	t2p->t2_maxdcount = 116;
	error = smb_t2_request(t2p);
	
	smb_t2_done(t2p);
	return error;
	
}

/*
 * *BASIC_INFO works with Samba, but Win2K servers say it is an
 * invalid information level on a SET_PATH_INFO.  Note Win2K does
 * support *BASIC_INFO on a SET_FILE_INFO, and they support the
 * equivalent *BASIC_INFORMATION on SET_PATH_INFO.  Go figure.
 */
PRIVSYM int
smbfs_smb_setpattrNT(struct smbnode *np, u_int32_t attr, 
			struct timespec *crtime, struct timespec *mtime,
			struct timespec *atime, struct timespec *chtime, 
			struct smb_cred *scrp)
{
	struct smb_t2rq *t2p;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct smb_vc *vcp = SSTOVC(ssp);
	struct mbchain *mbp;
	u_int64_t tm;
	int error, tzoff;
	int fat_fstype = (ssp->ss_fstype == SMB_FS_FAT);
	/* 64 bit value for Jan 1 1980 */
	PRIVSYM u_int64_t DIFF1980TO1601 = 11960035200ULL*10000000ULL;

	error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_SET_PATH_INFORMATION,
	    scrp, &t2p);
	if (error)
		return error;
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	if (vcp->vc_sopt.sv_caps & SMB_CAP_INFOLEVEL_PASSTHRU)
		mb_put_uint16le(mbp, SMB_SFILEINFO_BASIC_INFORMATION);
	else
		mb_put_uint16le(mbp, SMB_SFILEINFO_BASIC_INFO);
	mb_put_uint32le(mbp, 0);		/* MBZ */
	/* mb_put_uint8(mbp, SMB_DT_ASCII); specs incorrect */
	error = smbfs_fullpath(mbp, vcp, np, NULL, NULL, UTF_SFM_CONVERSIONS, '\\');
	if (error) {
		smb_t2_done(t2p);
		return error;
	}
	tzoff = vcp->vc_sopt.sv_tz;
	
	mbp = &t2p->t2_tdata;
	mb_init(mbp);
		/* set the creation time */
	if (crtime) {
		smb_time_local2NT(crtime, tzoff, &tm, fat_fstype);
		/* FAT file systems don't support dates earlier than 1980. */ 
		if (fat_fstype && (tm < DIFF1980TO1601)) {
			tm = DIFF1980TO1601;
			smb_time_NT2local(tm, tzoff, crtime);
		}
	} else
		tm = 0;
	mb_put_uint64le(mbp, tm);
		/* set the access time */	
	if (atime) {
		smb_time_local2NT(atime, tzoff, &tm, fat_fstype);
		/* FAT file systems don't support dates earlier than 1980. */ 
		if (fat_fstype && (tm < DIFF1980TO1601)) {
			tm = DIFF1980TO1601;
			smb_time_NT2local(tm, tzoff, atime);
		}
	} else
		tm = 0;
	mb_put_uint64le(mbp, tm);
		/* set the write/modify time */	
	if (mtime) {
		smb_time_local2NT(mtime, tzoff, &tm, fat_fstype);
		/* FAT file systems don't support dates earlier than 1980. */ 
		if (fat_fstype && (tm < DIFF1980TO1601)) {
			tm = DIFF1980TO1601;
			smb_time_NT2local(tm, tzoff, mtime);
		}
	} else
		tm = 0;
	mb_put_uint64le(mbp, tm);
		/* set the change time */		
	if (chtime) {
		smb_time_local2NT(chtime, tzoff, &tm, fat_fstype);
		/* FAT file systems don't support dates earlier than 1980. */ 
		if (fat_fstype && (tm < DIFF1980TO1601)) {
			tm = DIFF1980TO1601;
			smb_time_NT2local(tm, tzoff, chtime);
		}
	} else
		tm = 0;
	mb_put_uint64le(mbp, tm);
	
	mb_put_uint32le(mbp, attr);		/* attr */
	mb_put_uint32le(mbp, 0);	/* undocumented padding */
	t2p->t2_maxpcount = 24;
	t2p->t2_maxdcount = 56;
	error = smb_t2_request(t2p);

	smb_t2_done(t2p);
	return error;

}

/*
 * Set file atime and mtime. Isn't supported by core dialect.
 */
PRIVSYM int
smbfs_smb_setftime(struct smbnode *np, u_int16_t fid, struct timespec *crtime, 
			struct timespec *mtime, struct timespec *atime, 
			struct smb_cred *scrp)
{
	struct smb_rq rq, *rqp = &rq;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct mbchain *mbp;
	u_int16_t date, time;
	int error, tzoff;

	error = smb_rq_init(rqp, SSTOCP(ssp), SMB_COM_SET_INFORMATION2, scrp);
	if (error)
		return error;
	tzoff = SSTOVC(ssp)->vc_sopt.sv_tz;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_mem(mbp, (caddr_t)&fid, 2, MB_MSYSTEM);

	if (crtime)
		smb_time_unix2dos(crtime, tzoff, &date, &time, NULL);
	else
		time = date = 0;
	mb_put_uint16le(mbp, date);		/* creation time */
	mb_put_uint16le(mbp, time);

	if (atime)
		smb_time_unix2dos(atime, tzoff, &date, &time, NULL);
	else
		time = date = 0;
	mb_put_uint16le(mbp, date);
	mb_put_uint16le(mbp, time);
	if (mtime)
		smb_time_unix2dos(mtime, tzoff, &date, &time, NULL);
	else
		time = date = 0;
	mb_put_uint16le(mbp, date);
	mb_put_uint16le(mbp, time);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	SMBSDEBUG("%d\n", error);
	smb_rq_done(rqp);
	return error;
}

/*
 * Set DOS file attributes.
 * Looks like this call can be used only if CAP_NT_SMBS bit is on.
 */
PRIVSYM int
smbfs_smb_setfattrNT(struct smbnode *np, u_int32_t attr, u_int16_t fid,
			struct timespec *crtime, struct timespec *mtime,
			struct timespec *atime, struct timespec *chtime, 
			struct smb_cred *scrp)
{
	struct smb_t2rq *t2p;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct smb_vc *vcp = SSTOVC(ssp);
	struct mbchain *mbp;
	u_int64_t tm;
	int error, svtz;
	int fat_fstype = (ssp->ss_fstype == SMB_FS_FAT);
	/* 64 bit value for Jan 1 1980 */
	PRIVSYM u_int64_t DIFF1980TO1601 = 11960035200ULL*10000000ULL;

	error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_SET_FILE_INFORMATION,
	    scrp, &t2p);
	if (error)
		return error;
	svtz = SSTOVC(ssp)->vc_sopt.sv_tz;
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	mb_put_mem(mbp, (caddr_t)&fid, 2, MB_MSYSTEM);
	if (vcp->vc_sopt.sv_caps & SMB_CAP_INFOLEVEL_PASSTHRU)
		mb_put_uint16le(mbp, SMB_SFILEINFO_BASIC_INFORMATION);
	else
		mb_put_uint16le(mbp, SMB_SFILEINFO_BASIC_INFO);
	mb_put_uint32le(mbp, 0); /* XXX should be 16 not 32(?) */
	mbp = &t2p->t2_tdata;
	mb_init(mbp);
	/*
	 * In this call setting a time field to zero means no time specified. 
	 * Like everything else in this protocol there are exceptions. Windows 
	 * 95/98 thinks that you want to set the time to zero time. So for them
	 * we always set it back to last time they gave us.  
	 */
	if (vcp->vc_flags & SMBV_WIN98) {
		if (! crtime)
			crtime = &np->n_crtime;
		if (! mtime)
			mtime = &np->n_mtime;
		atime = mtime;
		chtime = mtime;
	}
		/* set the creation time */	
	if (crtime) {
		smb_time_local2NT(crtime, svtz, &tm, fat_fstype);
		/* FAT file systems don't support dates earlier than 1980. */ 
		if (fat_fstype && (tm < DIFF1980TO1601)) {
			tm = DIFF1980TO1601;
			smb_time_NT2local(tm, svtz, crtime);
		}
	} else
		tm = 0;
	mb_put_uint64le(mbp, tm);
		/* set the access time */	
	if (atime) {
		smb_time_local2NT(atime, svtz, &tm, fat_fstype);
		/* FAT file systems don't support dates earlier than 1980. */ 
		if (fat_fstype && (tm < DIFF1980TO1601)) {
			tm = DIFF1980TO1601;
			smb_time_NT2local(tm, svtz, atime);
		}
	} else
		tm = 0;
	mb_put_uint64le(mbp, tm);
		/* set the write/modify time */		
	if (mtime) {
		smb_time_local2NT(mtime, svtz, &tm, fat_fstype);
		/* FAT file systems don't support dates earlier than 1980. */ 
		if (fat_fstype && (tm < DIFF1980TO1601)) {
			tm = DIFF1980TO1601;
			smb_time_NT2local(tm, svtz, mtime);
		}
	} else
		tm = 0;
	mb_put_uint64le(mbp, tm);
		/* set the change time */		
	if (chtime) {
		smb_time_local2NT(chtime, svtz, &tm, fat_fstype);
		/* FAT file systems don't support dates earlier than 1980. */ 
		if (fat_fstype && (tm < DIFF1980TO1601)) {
			tm = DIFF1980TO1601;	
			smb_time_NT2local(tm, svtz, chtime);
		}
	} else
		tm = 0;
	mb_put_uint64le(mbp, tm);
	
	mb_put_uint32le(mbp, attr);
	mb_put_uint32le(mbp, 0);			/* padding */
	t2p->t2_maxpcount = 2;
	t2p->t2_maxdcount = 0;
	error = smb_t2_request(t2p);
	smb_t2_done(t2p);
	return error;
}

/*
 * Modern create/open of file or directory.
 *
 * If disp is NTCREATEX_DISP_OPEN then this is an open attempt, and:
 *   If xattr then name is the stream to be opened at np,
 *   Else np should be opened.
 *   ...we won't touch *fidp,
 * Else this is a creation attempt, and:
 *   If xattr then name is the stream to create at np,
 *   Else name is the thing to create under directory np.
 *   ...we will return *fidp,
 */
static int
smbfs_smb_ntcreatex(struct smbnode *np, u_int32_t rights, u_int32_t shareMode,  struct smb_cred *scrp,  enum vtype vt, 
					u_int16_t *fidp, const char *name,  int in_nmlen, u_int32_t disp, int xattr, struct smbfattr *fap)
{
	struct smb_rq rq, *rqp = &rq;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct smb_vc *vcp = SSTOVC(ssp);
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int8_t wc;
	u_int32_t lint, createopt, efa;
	u_int64_t llint;
	int error;
	u_int16_t fid, *namelenp;
	int nmlen = in_nmlen;	/* Don't change the input name length, we need it for making the ino number */

	DBG_ASSERT(fap); /* Should never happen */
	bzero(fap, sizeof(*fap));
	nanotime(&fap->fa_reqtime);
	error = smb_rq_init(rqp, SSTOCP(ssp), SMB_COM_NT_CREATE_ANDX, scrp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint8(mbp, 0xff);	/* secondary command */
	mb_put_uint8(mbp, 0);		/* MBZ */
	mb_put_uint16le(mbp, 0);	/* offset to next command (none) */
	mb_put_uint8(mbp, 0);		/* MBZ */
	namelenp = (u_int16_t *)mb_reserve(mbp, sizeof(u_int16_t));
	/*
	 * XP to a W2K Server does not use NTCREATEX_FLAGS_OPEN_DIRECTORY
	 * for creating nor for opening a directory.  Samba ignores the bit.
	 */
	mb_put_uint32le(mbp, 0);	/* NTCREATEX_FLAGS_* */
	mb_put_uint32le(mbp, 0);	/* FID - basis for path if not root */
	mb_put_uint32le(mbp, rights);
	mb_put_uint64le(mbp, 0);	/* "initial allocation size" */
	efa = (vt == VDIR) ? SMB_EFA_DIRECTORY : SMB_EFA_NORMAL;
	if (disp != NTCREATEX_DISP_OPEN && !xattr) {
		if (efa == SMB_EFA_NORMAL)
			efa |= SMB_EFA_ARCHIVE;
		if (*name == '.')
			efa |= SMB_EFA_HIDDEN;
	}
	mb_put_uint32le(mbp, efa);
	/*
	 * To rename an open file we need delete shared access. We currently always
	 * allow delete access.
	 */
	mb_put_uint32le(mbp, shareMode);
	/*
	 * If this is just an open call on a stream then always request it to bee create if it doesn't exist. See the 
	 * smbfs_vnop_removenamedstream for the reasons behind this decision.
	 */ 
	if ((np->n_vnode) && (vnode_isnamedstream(np->n_vnode)) && (!name) && (!xattr))
		mb_put_uint32le(mbp, (disp | NTCREATEX_DISP_OPEN_IF));
	else
		mb_put_uint32le(mbp, disp);
	createopt = 0;
	if (disp != NTCREATEX_DISP_OPEN) {
		if (vt == VDIR)
			createopt |= NTCREATEX_OPTIONS_DIRECTORY;
		/* (other create options currently not useful) */
	}
	mb_put_uint32le(mbp, createopt);
	mb_put_uint32le(mbp, NTCREATEX_IMPERSONATION_IMPERSONATION); /* (?) */
	mb_put_uint8(mbp, 0);   /* security flags (?) */
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	do {
		u_int8_t sep = xattr ? ':' : '\\';
		/* Do they want to open the resource fork? */
		if ((np->n_vnode) && (vnode_isnamedstream(np->n_vnode)) && (!name) && (!xattr)) {
			name = (const char *)np->n_sname;
			nmlen = np->n_snmlen;
			sep = ':';
		}
		if (name == NULL)
			nmlen = 0;
		error = smbfs_fullpath(mbp, vcp, np, name, &nmlen, UTF_SFM_CONVERSIONS, sep);
		if (error)
			break;
		*namelenp = htoles(nmlen); /* includes null */
		smb_rq_bend(rqp);
		error = smb_rq_simple(rqp);
		if (error)
			break;
		smb_rq_getreply(rqp, &mdp);
		/*
		 * spec says 26 for word count, but 34 words are defined
		 * and observed from win2000
		 */
		if (md_get_uint8(mdp, &wc) != 0 ||
		    (wc != 26 && wc != 34 && wc != 42)) {
			error = EBADRPC;
			break;
		}
		md_get_uint8(mdp, NULL);	/* secondary cmd */
		md_get_uint8(mdp, NULL);	/* mbz */
		md_get_uint16le(mdp, NULL);     /* andxoffset */
		md_get_uint8(mdp, NULL);	/* oplock lvl granted */
		md_get_uint16(mdp, &fid);       /* yes, leaving it LE */
		md_get_uint32le(mdp, NULL);     /* create_action */
		md_get_uint64le(mdp, &llint);   /* creation time */
		if (llint) {
			smb_time_NT2local(llint, vcp->vc_sopt.sv_tz, &fap->fa_crtime);
		}
		md_get_uint64le(mdp, &llint);   /* access time */
		if (llint) {
			smb_time_NT2local(llint, vcp->vc_sopt.sv_tz, &fap->fa_atime);
		}
		md_get_uint64le(mdp, &llint);   /* write time */
		if (llint) {
			smb_time_NT2local(llint, vcp->vc_sopt.sv_tz, &fap->fa_mtime);
		}
		md_get_uint64le(mdp, &llint);   /* change time */
		if (llint) {
			smb_time_NT2local(llint, vcp->vc_sopt.sv_tz, &fap->fa_chtime);
		}
		md_get_uint32le(mdp, &lint);    /* attributes */
		fap->fa_attr = lint;
		fap->fa_vtype = (fap->fa_attr & SMB_FA_DIR) ? VDIR : VREG;
		md_get_uint64le(mdp, &llint);     /* allocation size */
		fap->fa_data_alloc = llint;
		md_get_uint64le(mdp, &llint);   /* EOF */
		fap->fa_size = llint;
		md_get_uint16le(mdp, NULL);     /* file type */
		md_get_uint16le(mdp, NULL);     /* device state */
		md_get_uint8(mdp, NULL);	/* directory (boolean) */
	} while(0);
	smb_rq_done(rqp);
	if (error)      
		return error;
	
	if (fidp)
		*fidp = fid;
	
	if (xattr)	/* If an EA or Stream then we are done */
		goto WeAreDone;
		/* We are creating the item, create the  ino number */
	if (disp != NTCREATEX_DISP_OPEN) {
		DBG_ASSERT(name != NULL);	/* This is a create better have a name */
		fap->fa_ino = smbfs_getino(np, name, in_nmlen);
		goto WeAreDone;			
	}
	/* If this is a SYMLINK, then n_vnode could be set to NULL */
	if (np->n_vnode == NULL)
		goto WeAreDone;

	/* 
 	 * We have all the meta data attributes so update the cache. If the 
	 * calling routine is setting an attribute it should not change the 
	 * smb node value until after the open has completed. NOTE: The old 
	 * code would only update the cache if the mtime, attributes and size 
	 * haven't changed.
 	 */
	smbfs_attr_cacheenter(np->n_vnode, fap, TRUE);
WeAreDone:
	return (0);
}

static int
smb_rights2mode(u_int32_t rights)
{
	int accmode = SMB_AM_OPENEXEC; /* our fallback */

	if (rights & (SA_RIGHT_FILE_APPEND_DATA | SA_RIGHT_FILE_DELETE_CHILD |
		      SA_RIGHT_FILE_WRITE_EA | SA_RIGHT_FILE_WRITE_ATTRIBUTES |
		      SA_RIGHT_FILE_WRITE_DATA | STD_RIGHT_WRITE_OWNER_ACCESS |
		      STD_RIGHT_DELETE_ACCESS | STD_RIGHT_WRITE_DAC_ACCESS |
		      GENERIC_RIGHT_ALL_ACCESS | GENERIC_RIGHT_WRITE_ACCESS))
		accmode = SMB_AM_OPENWRITE;
	if (rights & (SA_RIGHT_FILE_READ_DATA | SA_RIGHT_FILE_READ_ATTRIBUTES |
		      SA_RIGHT_FILE_READ_EA | STD_RIGHT_READ_CONTROL_ACCESS |
		      GENERIC_RIGHT_ALL_ACCESS | GENERIC_RIGHT_READ_ACCESS))
		accmode = (accmode == SMB_AM_OPENEXEC) ? SMB_AM_OPENREAD
						       : SMB_AM_OPENRW;
	return (accmode);
}


static int
smbfs_smb_oldopen(struct smbnode *np, int accmode, struct smb_cred *scrp, u_int16_t *fidp,
			const char *name, int nmlen, int xattr, struct smbfattr *fap)
{
	struct smb_rq rq, *rqp = &rq;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct smb_vc *vcp = SSTOVC(ssp);
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int8_t wc;
	u_int16_t fid, wattr, grantedmode;
	u_int32_t lint;
	int error;

	bzero(fap, sizeof(*fap));
	nanotime(&fap->fa_reqtime);
	error = smb_rq_init(rqp, SSTOCP(ssp), SMB_COM_OPEN, scrp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint16le(mbp, accmode);
	mb_put_uint16le(mbp, SMB_FA_SYSTEM | SMB_FA_HIDDEN | SMB_FA_RDONLY |
			     SMB_FA_DIR);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint8(mbp, SMB_DT_ASCII);
	do {
		error = smbfs_fullpath(mbp, vcp, np, name, &nmlen, UTF_SFM_CONVERSIONS,
					xattr ? ':' : '\\');
		if (error)
			break;
		smb_rq_bend(rqp);
		error = smb_rq_simple(rqp);
		if (error)
			break;
		smb_rq_getreply(rqp, &mdp);
		/*
		 * 8/2002 a DAVE server returned wc of 15 so we ignore that.
		 * (the actual packet length and data was correct)
		 */
		if (md_get_uint8(mdp, &wc) != 0 || (wc != 7 && wc != 15)) {
			error = EBADRPC;
			break;
		}
		md_get_uint16(mdp, &fid); /* yes, we leave it LE */
		md_get_uint16le(mdp, &wattr);
		fap->fa_attr = wattr;
		fap->fa_vtype = (fap->fa_attr & SMB_FA_DIR) ? VDIR : VREG;
		/*
		 * Be careful using the time returned here, as
		 * with FAT on NT4SP6, at least, the time returned is low
		 * 32 bits of 100s of nanoseconds (since 1601) so it rolls
		 * over about every seven minutes!
		 */
		md_get_uint32le(mdp, &lint); /* specs: secs since 1970 */
		if (lint)	/* avoid bogus zero returns */
			smb_time_server2local(lint, vcp->vc_sopt.sv_tz, &fap->fa_mtime);
		md_get_uint32le(mdp, &lint);
		fap->fa_size = lint;
		md_get_uint16le(mdp, &grantedmode);
	} while(0);
	smb_rq_done(rqp);
	if (error)
		return error;
	if (fidp)
		*fidp = fid;
	if (xattr)
		goto WeAreDone;
		
	/* If this is a SYMLINK, then n_vnode could be set to NULL */
	if (np->n_vnode == NULL)
		goto WeAreDone;

	/*
	 * Update the cached attributes if they are still valid
	 * in the cache and if nothing has changed.
	 * Note that this won't ever update if the file size is
	 * greater than the 32-bits returned by SMB_COM_OPEN.
	 * For 64-bit file sizes, SMB_COM_NT_CREATE_ANDX must
	 * be used instead of SMB_COM_OPEN.
	 */
	if (smbfs_attr_cachelookup(np->n_vnode, NULL, scrp) != 0)
		goto WeAreDone;	/* the cached attributes are not valid */
	if (fap->fa_size != np->n_size) {
		np->attribute_cache_timer = 0;
		goto WeAreDone;	/* the size is different */
	}
	if (fap->fa_attr != np->n_dosattr) {
		np->attribute_cache_timer = 0;
		goto WeAreDone;	/* the attrs are different */
	}
	/*
	 * fap->fa_mtime is in two second increments while np->n_mtime
	 * may be in one second increments, so comparing the times is
	 * somewhat sloppy. Windows 98 or older so assume its FAT?
	 */
	if (fap->fa_mtime.tv_sec != np->n_mtime.tv_sec &&
	    fap->fa_mtime.tv_sec != np->n_mtime.tv_sec - 1 &&
	    fap->fa_mtime.tv_sec != np->n_mtime.tv_sec + 1) {
		np->attribute_cache_timer = 0;
		goto WeAreDone;	/* the mod time is different */
	}
	/* We assume the other times stay the same. */
	fap->fa_crtime = np->n_crtime;
	fap->fa_chtime = np->n_chtime;
	fap->fa_atime = np->n_atime; 
	fap->fa_mtime = np->n_mtime; /* keep higher res time */
	smbfs_attr_cacheenter(np->n_vnode, fap, TRUE);
WeAreDone:
	return 0;
}

PRIVSYM int
smbfs_smb_tmpopen(struct smbnode *np, u_int32_t rights, struct smb_cred *scrp, u_int16_t *fidp)
{
	struct smb_vc	*vcp = SSTOVC(np->n_mount->sm_share);
	int		searchOpenFiles = TRUE;
	int		error = 0;
	struct smbfattr fattr;

	/* Check to see if the file needs to be reopened */
	error = smbfs_smb_reopen_file(np, scrp->scr_vfsctx);
	if (error) {
		SMBDEBUG(" %s waiting to be revoked\n", np->n_name);
	    return(error);
	}
	
	/*
	 * A normal open can have the following rights 
	 *	STD_RIGHT_READ_CONTROL_ACCESS - always set
	 *	SA_RIGHT_FILE_READ_DATA
	 *	SA_RIGHT_FILE_APPEND_DATA
	 *	SA_RIGHT_FILE_WRITE_DATA
	 *
	 * A normal open will never have the following rights 
	 *	STD_RIGHT_DELETE_ACCESS
	 *	STD_RIGHT_WRITE_DAC_ACCESS
	 *	STD_RIGHT_WRITE_OWNER_ACCESS
	 *	SA_RIGHT_FILE_WRITE_ATTRIBUTES
	 *	
	 */
	if (rights & (STD_RIGHT_DELETE_ACCESS | STD_RIGHT_WRITE_DAC_ACCESS | STD_RIGHT_WRITE_OWNER_ACCESS))
		searchOpenFiles = FALSE;
	if ((vcp->vc_sopt.sv_caps & SMB_CAP_NT_SMBS) && (rights & SA_RIGHT_FILE_WRITE_ATTRIBUTES))
		searchOpenFiles = FALSE;
		
	/* 
	 * Remmeber we could have been called before the vnode is create. Conrads 
	 * crazy symlink code. So if we have no vnode then we cannot borrow the
	 * fid. Only borrow a fid if the requested access modes could have been
	 * made on an open call. 
	 */
	if (searchOpenFiles && SMBTOV(np)) {
		u_int16_t accessMode = 0;
		
		if (rights & (STD_RIGHT_READ_CONTROL_ACCESS | SA_RIGHT_FILE_READ_DATA))
			accessMode |= kAccessRead;
		if (rights & (SA_RIGHT_FILE_APPEND_DATA | SA_RIGHT_FILE_WRITE_DATA))
			accessMode |= kAccessWrite;
			/* Must be a Windows 98 system */
		if (rights & SA_RIGHT_FILE_WRITE_ATTRIBUTES)
			accessMode |= kAccessWrite;
		/* First check the non deny mode opens, if we have one up the refcnt */
		if (np->f_fid && (accessMode & np->f_accessMode) == accessMode) {
			np->f_refcnt++; 
			*fidp = np->f_fid;
			return (0);
		}
		/* Now check the deny mode opens, if we find one up the refcnt */
		if (np->f_refcnt && (smbfs_findFileRef(SMBTOV(np), scrp->pid, accessMode, kAnyMatch, 0, 0, NULL, fidp) == 0)) {
			np->f_refcnt++;	
			return (0);
		}
	}
	/*
	 * For temp opens we give unixy semantics of permitting everything not forbidden 
	 * by permissions.  Ie denial is up to server with clients/openers needing to use
	 * advisory locks for further control.
	 */
	if (!(vcp->vc_sopt.sv_caps & SMB_CAP_NT_SMBS)) {
		int accmode = smb_rights2mode(rights) | SMB_SM_DENYNONE;
		
		error = smbfs_smb_oldopen(np, accmode, scrp, fidp, NULL, 0, 0, &fattr);
	}
	else {
		u_int32_t shareMode = NTCREATEX_SHARE_ACCESS_ALL;

		error = smbfs_smb_ntcreatex(np, rights, shareMode, scrp, VREG, fidp, 
					NULL, 0, NTCREATEX_DISP_OPEN,  0, &fattr);
	}
	if (error)
		SMBWARNING("%s failed to open: error = %d\n", np->n_name, error);
	return (error);
}

PRIVSYM int
smbfs_smb_tmpclose(struct smbnode *np, u_int16_t fid, struct smb_cred *scrp)
{
	struct smb_share *ssp = np->n_mount->sm_share;
	struct fileRefEntry *entry = NULL;
	vnode_t vp = SMBTOV(np);

	/* 
	 * Remeber we could have been called before the vnode is create. Conrads 
	 * crazy symlink code. So if we have no vnode then we did not borrow the
	 * fid. If we did not borrow the fid then just close the fid and get out. 
	 */
	if (!vp || ((fid != np->f_fid) && (smbfs_findFileEntryByFID(vp, fid, &entry)))) {
		return(smbfs_smb_close(ssp, fid, scrp));
	}
	/* 
	 * OK we borrowed the fid do we have the last reference count on it. If 
	 * yes, then we need to close up every thing. smbfs_close can handle this
	 * for us.
	 */
	if (np->f_refcnt == 1) {
		/* Open Mode does not matter becasue we closing everything */
		return(smbfs_close(vp, 0, scrp->scr_vfsctx));
	}
	/* We borrowed the fid decrement the ref count */
	np->f_refcnt--;
	return (0);
}

/*
 * This routine chains the open and read into one message. This routine is used only
 * for reading data out of a stream. If we decided to use it for something else then
 * we will need to make some changes.
 */
PRIVSYM 
int smbfs_smb_openread(struct smbnode *np, u_int16_t *fid, u_int32_t rights, uio_t uio, 
							   size_t *sizep,  const char *name, struct timespec *mtime, struct smb_cred *scrp)
{
	struct smb_share *ssp = np->n_mount->sm_share;
	struct smb_vc *vcp = SSTOVC(ssp);
	struct smb_rq rq, *rqp = &rq;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int8_t wc, cmd;
	int error = 0;
	u_int16_t *namelenp, *nextWdCntOffset, nextOffset;
	u_int64_t eof;
	u_int16_t residhi, residlo, off, doff;
	u_int32_t resid;
	u_int32_t len = uio_resid(uio);
	int nmlen = strlen(name);

	/* 
	 * Make sure the whole response message will fit in our max buffer size. Since we 
	 * use the CreateAndX open call make sure the server supports that call. The calling
	 * routine must handle this routine returning ENOTSUP.
	 */	
	if (((vcp->vc_txmax - SMB_MAX_CHAIN_READ) < len) || !(vcp->vc_sopt.sv_caps & SMB_CAP_NT_SMBS))
		return(ENOTSUP);
	
	/* encode the CreateAndX request */
	error = smb_rq_init(rqp, SSTOCP(ssp), SMB_COM_NT_CREATE_ANDX, scrp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint8(mbp, SMB_COM_READ_ANDX);	/* next chain command will be a read */
	mb_put_uint8(mbp, 0);					/* MBZ */
	/* 
	 * The next command offset is the numbers of bytes from the smb header to the location of
	 * the next commands word count field. Save that location so we can fill it in later.
	 */
	nextWdCntOffset = (u_short*)mb_reserve(mbp, sizeof(u_int16_t)); /* offset to next command */
	mb_put_uint8(mbp, 0);		/* MBZ */
	/* Save off the name length field so we can fill it in later */
	namelenp = (u_int16_t *)mb_reserve(mbp, sizeof(u_int16_t));
	/* 
	 * %%%
	 * I remember a problem with NT servers and deleting items after opening the stream
	 * file. May have only been a problem with SFM. I know opening the file with oplocks solve the
	 * problem, but I do not remeber all the details. It may have only been a problem when writing
	 * to the Finder Info Stream? We should test this out, but will need file manager support before
	 * we can do a real test.
	 */
	mb_put_uint32le(mbp, 0);	/* Oplock?  NTCREATEX_FLAGS_REQUEST_OPLOCK */
	mb_put_uint32le(mbp, 0);	/* Root fid not used */
	mb_put_uint32le(mbp, rights);
	mb_put_uint64le(mbp, 0);	/* "initial allocation size" */
	mb_put_uint32le(mbp, SMB_EFA_NORMAL);
	/* Deny write access if they want write access */
	if (rights & SA_RIGHT_FILE_WRITE_DATA) {
		mb_put_uint32le(mbp, (NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_DELETE));
		mb_put_uint32le(mbp, NTCREATEX_DISP_OPEN_IF);		
	}
	else {
		mb_put_uint32le(mbp, NTCREATEX_SHARE_ACCESS_ALL);
		mb_put_uint32le(mbp, NTCREATEX_DISP_OPEN);
	}
	mb_put_uint32le(mbp, 0);
	mb_put_uint32le(mbp, NTCREATEX_IMPERSONATION_IMPERSONATION); 
	mb_put_uint8(mbp, 0);   /* security flags */
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	/* Put in the path name here */
	error = smbfs_fullpath(mbp, vcp, np, name, &nmlen, UTF_SFM_CONVERSIONS, ':');
	if (error)
		goto exit;

	*namelenp = htoles(nmlen); /* include the null bytes */
	smb_rq_bend(rqp);

	mb_put_padbyte(mbp); /* make sure the next message is on an even boundry */
	*nextWdCntOffset = htoles(mb_fixhdr(mbp));	
	
	/* now add the read request */
	smb_rq_wstart(rqp);	
	mb_put_uint8(mbp, 0xff);	/* no secondary command */
	mb_put_uint8(mbp, 0);		/* MBZ */
	mb_put_uint16le(mbp, 0);	/* offset to secondary, no more chain items */
	mb_put_uint16le(mbp, 0);	/* set fid field to zero the server fills this in */

	mb_put_uint32le(mbp, (u_int32_t)uio_offset(uio));	/* Lower offset */
	mb_put_uint16le(mbp, (u_int16_t)len);	/* MaxCount */
	mb_put_uint16le(mbp, (u_int16_t)len);	/* MinCount (only indicates blocking) */
	mb_put_uint32le(mbp, (unsigned)len >> 16);	/* MaxCountHigh */
	mb_put_uint16le(mbp, (u_int16_t)len);	/* Remaining ("obsolete") */
	mb_put_uint32le(mbp, (u_int32_t)(uio_offset(uio) >> 32));	/* high offset */
	
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	smb_rq_bend(rqp);
	/* Send the message */
	error = smb_rq_simple(rqp);
	if (error)
		goto exit;
	smb_rq_getreply(rqp, &mdp);
	
	/*
	 * I know there are some servers that return word counts of 42, but I don't
	 * remember exactly why (Not Windows Systems). Shouldn't matter to us because the 
	 * offset to the read message will eat any extra bytes.
	*/
	if (md_get_uint8(mdp, &wc) != 0 || (wc != 34 && wc != 42)) {
		error = EINVAL;
		goto exit;
	}
	md_get_uint8(mdp, &cmd);	/* secondary cmd */

	md_get_uint8(mdp, NULL);	/* mbz */
	/* Contains the offset from the start of the message to the read message. */
	md_get_uint16le(mdp, &nextOffset);     /* andxoffset */
	md_get_uint8(mdp, NULL);	/* oplock lvl granted */
	md_get_uint16(mdp, fid);       /* Get the fid */
	md_get_uint32le(mdp, NULL);     /* create_action */
	md_get_uint64le(mdp, NULL);   /* creation time */
	md_get_uint64le(mdp, NULL);   /* access time */
	if (mtime) {
		u_int64_t llint;
		
		md_get_uint64le(mdp, &llint);   /* write time */
		if (llint)
			smb_time_NT2local(llint, vcp->vc_sopt.sv_tz, mtime);
	}
	else 
		md_get_uint64le(mdp, NULL);   /* write time */
		
	md_get_uint64le(mdp, NULL);   /* change time */
	md_get_uint32le(mdp, NULL);    /* attributes */
	md_get_uint64le(mdp, NULL);     /* allocation size */
	md_get_uint64le(mdp, &eof);   /* EOF */
	if (sizep) 
		*sizep = eof;
	md_get_uint16le(mdp, NULL);     /* file type */
	md_get_uint16le(mdp, NULL);     /* device state */
	md_get_uint8(mdp, NULL);	/* directory (boolean) */
	md_get_uint16(mdp, NULL);	/* byte count */

	if (cmd != SMB_COM_READ_ANDX) {
		if ((rights & SA_RIGHT_FILE_WRITE_DATA) && fid)	/* We created the file */
			error = 0;
		else 
			error = ENOENT;
		goto exit;
	}
	
	off = nextOffset;
	/* Is the offset pass the end of our buffer? */
	if (nextOffset > mbuf_pkthdr_len(mdp->md_top)) {
		error = EINVAL;
		goto exit;
	}
	/* Take off what we have already consumed. */
	nextOffset -= (SMB_HDRLEN + SMB_CREATEXRLEN + SMB_BCOUNT_LEN); 
	if (nextOffset != 0) /* Anything left dump it */
		md_get_mem(mdp, NULL, nextOffset, MB_MSYSTEM);
		
		/* We are at the read message make sure the word count matches. */
	if (md_get_uint8(mdp, &wc) != 0 || (wc != 12)) {
		error = EINVAL;
		goto exit;
	}
	/* Now handle the read response */
	off++;
	md_get_uint8(mdp, NULL);
	off++;
	md_get_uint8(mdp, NULL);
	off++;
	md_get_uint16le(mdp, NULL);
	off += 2;
	md_get_uint16le(mdp, NULL);
	off += 2;
	md_get_uint16le(mdp, NULL);	/* data compaction mode */
	off += 2;
	md_get_uint16le(mdp, NULL);
	off += 2;
	md_get_uint16le(mdp, &residlo);
	off += 2;
	md_get_uint16le(mdp, &doff);	/* data offset */
	off += 2;
	md_get_uint16le(mdp, &residhi);
	off += 2;
	resid = (residhi << 16) | residlo;
	md_get_mem(mdp, NULL, 4 * 2, MB_MSYSTEM);
	off += 4*2;
	md_get_uint16le(mdp, NULL);	/* ByteCount */
	off += 2;
	if (doff > off)	/* pad byte(s)? */
		md_get_mem(mdp, NULL, doff - off, MB_MSYSTEM);
	if (resid)
		error = md_get_uio(mdp, uio, resid);
	
exit:;
	smb_rq_done(rqp);
	return (error);
}

static int
smbfs_smb_open_file(struct smbnode *np, u_int32_t rights, u_int32_t shareMode, struct smb_cred *scrp,
					u_int16_t *fidp, const char *name, int nmlen, int xattr, struct smbfattr *fap)
{
	int error;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct smb_vc *vcp = SSTOVC(ssp);
	
	if (vcp->vc_sopt.sv_caps & SMB_CAP_NT_SMBS) {
		error = smbfs_smb_ntcreatex(np, rights, shareMode, scrp, VREG, fidp, name, 
					nmlen, NTCREATEX_DISP_OPEN, xattr, fap);
	} else {
		int accmode = smb_rights2mode(rights);
		/* Converrt the NT deny modes to the old Windows deny modes */
		switch (shareMode) {
		case NTCREATEX_SHARE_ACCESS_ALL:
			accmode |= SMB_SM_DENYNONE;
			break;
		case NTCREATEX_SHARE_ACCESS_READ:
			accmode |= SMB_SM_DENYWRITE;
			break;
		default:
			accmode |= (SMB_SM_DENYWRITE | SMB_SM_DENYREADEXEC);
			break;
		}
		
		error = smbfs_smb_oldopen(np, smb_rights2mode(rights), scrp,
					  fidp, name, nmlen, xattr, fap);
	}
	return (error);
}

PRIVSYM int
smbfs_smb_open(struct smbnode *np, u_int32_t rights, u_int32_t shareMode, struct smb_cred *scrp,
				u_int16_t *fidp)
{
	struct smbfattr fattr;
	
	return(smbfs_smb_open_file(np, rights, shareMode, scrp, fidp, NULL, 0, FALSE, &fattr));
}

PRIVSYM int
smbfs_smb_open_xattr(struct smbnode *np, u_int32_t rights, u_int32_t shareMode, struct smb_cred *scrp, 
					   u_int16_t *fidp, const char *name, size_t *sizep)
{
	int error;
	struct smbfattr fattr;
	int nmlen = strlen(name);
	
	error = smbfs_smb_open_file(np, rights, shareMode, scrp, fidp, name, nmlen, TRUE, &fattr);
	if (!error && sizep)
		*sizep = fattr.fa_size;
	return(error);
}

PRIVSYM int 
smbfs_smb_reopen_file(struct smbnode *np, vfs_context_t context)
{
	struct smb_share *ssp = np->n_mount->sm_share;
	int error = 0;
	struct timespec n_mtime = np->n_mtime;	/* Remember open can change this value save it */
	u_quad_t		n_size = np->n_size;	/* Remember open can change this value save it */
	struct smbfattr fattr;
	struct smb_cred scred;
	
	/* We are in the middle of a reconnect, wait for it to complete */
	while (ssp->ss_flags & SMBS_RECONNECTING) {
		SMBDEBUG("SMBS_RECONNECTING Going to sleep! \n");
		msleep(&ssp->ss_flags, 0, PWAIT, "smbfs_smb_reopen_file", NULL);
	}
	/* File was already revoked, just return the correct error */
	if (np->f_openState == kNeedRevoke)
		return EIO;
	else if (np->f_openState != kNeedReopen)
		return 0;	/* Nothing wrong so just return */

	DBG_ASSERT(np->f_refcnt);	/* Better have an open at this point */
	/* Clear the state flag, this way we know if a reconnect happen during reopen */
	lck_mtx_lock(&np->f_openStateLock);
	np->f_openState = 0; 
	lck_mtx_unlock(&np->f_openStateLock);
	/* Hope to remove some day */
	smb_scred_init(&scred, context);
	
	/* Was open with deny modes */
	if (np->f_openDenyList) {
		struct fileRefEntry	*curr = NULL;
		struct ByteRangeLockEntry *brl;
		u_int32_t	shareMode;

		/* Make sure no one else is accessing the list, remember pagein and pageout are not under the node lock */
		lck_mtx_lock(&np->f_openDenyListLock);
		/*
		 * We have to open all of the file reference entries. We will also have to 
		 * reestablish all the locks, any failure on the open or locks means a complete 
		 * failure. We only check the modify time and file size once.
		 */
		curr = np->f_openDenyList;
		while (curr != NULL) {
			/* Start with share all and then remove what isn't needed */
			shareMode = NTCREATEX_SHARE_ACCESS_ALL;
			if (curr->accessMode & kDenyRead)	/* O_EXLOCK */
				shareMode &= ~(NTCREATEX_SHARE_ACCESS_WRITE | NTCREATEX_SHARE_ACCESS_READ);
			else	/* O_SHLOCK */
				shareMode &= ~NTCREATEX_SHARE_ACCESS_WRITE;
			/* Now try to open the file */
			error = smbfs_smb_open_file(np, curr->rights, shareMode, &scred, &curr->fid, NULL, 0, FALSE, &fattr);
			if (error)	/* Failed then get out one failure stops the reopen */ {
				SMBERROR("Reopen %s with open deny modes failed because the open call failed!\n",  np->n_name);							
				break;
			}
			/* If this is the first open make sure the modify time and size have not change since we last looked. */
			if ((curr == np->f_openDenyList) && ((!(timespeccmp(&n_mtime, &fattr.fa_mtime, ==))) || (n_size != fattr.fa_size))) {
				if (n_size != np->n_size)
					SMBERROR("Reopen %s with open deny modes failed because the size has changed was 0x%lld now 0x%lld!\n",  np->n_name, n_size, fattr.fa_size);
				else 
					SMBERROR("Reopen %s with open deny modes failed because the modify time has changed was %lds %ldns now %lds %ldns!\n",  np->n_name, n_mtime.tv_sec, 
							 n_mtime.tv_nsec, fattr.fa_mtime.tv_sec, fattr.fa_mtime.tv_nsec);			
				error = EIO;
				break;
			}
			/* Now deal with any locks */
			brl = curr->lockList;
			while ((error == 0) && (brl != NULL)) {
				error = smbfs_smb_lock(np, SMB_LOCK_EXCL, curr->fid, brl->lck_pid, brl->offset, brl->length, &scred, 0);
				brl = brl->next;
			}
			if (error)	/* Failed then get out any failure stops the reopen process */ {
				SMBERROR("Reopen %s with open deny modes failed because we could not reestablish the locks! \n",  np->n_name);							
				break;					
			}
			curr = curr->next;	/* Go to the next one */
		}
		/* We had an error close any files we may have opened this will break any locks. */
		if (error) {
			curr = np->f_openDenyList;
			while (curr != NULL) {
				(void)smbfs_smb_close(ssp, curr->fid, &scred);
				curr->fid = -1;
				curr = curr->next;
			}
		}
		lck_mtx_unlock(&np->f_openDenyListLock);

	} else {
		/* %%% Should be able to support carbon locks on POSIX Opens. */
		/* POSIX Open: Reopen with the same modes we had it open with before the reconnect */
		error = smbfs_smb_open_file(np, np->f_rights, NTCREATEX_SHARE_ACCESS_ALL, &scred, &np->f_fid, NULL, 0, FALSE, &fattr);
		if (error)
			SMBERROR("Reopen %s failed because the open call failed!\n",  np->n_name);							
		/* If an error or no lock then we are done, nothing else to do */
		if (error || (np->f_smbflock == NULL))
			goto exit;
		if ((!(timespeccmp(&n_mtime, &fattr.fa_mtime, ==))) || (n_size != fattr.fa_size)) {
			if (n_size != np->n_size)
				SMBERROR("Reopen %s failed because the size has changed was 0x%lld now 0x%lld!\n",  np->n_name, n_size, fattr.fa_size);
			else 
				SMBERROR("Reopen %s failed because the modify time has changed was %lds %ldns now %lds %ldns!\n",  np->n_name, n_mtime.tv_sec, 
						 n_mtime.tv_nsec, fattr.fa_mtime.tv_sec, fattr.fa_mtime.tv_nsec);			
			error = EIO;
		}
		else {
			struct smbfs_flock *flk = np->f_smbflock;
			error = smbfs_smb_lock(np, SMB_LOCK_EXCL, np->f_fid, flk->lck_pid, flk->start, flk->len, &scred, 0);
			if (error)
				SMBERROR("Reopen %s failed because we could not reestablish the lock! \n",  np->n_name);							
		}
		/* Something is different or we failed on the lock request, close it */
		if (error)
			(void)smbfs_smb_close(ssp, np->f_fid, &scred);
	}

exit:;
	
	lck_mtx_lock(&np->f_openStateLock);
	if (error) {
		char errbuf[32];
		int pid = proc_pid(vfs_context_proc(context));
		
		proc_name(pid, &errbuf[0], 32);
		SMBERROR("Warning: pid %d(%.*s) reopening of %s failed with error %d\n", pid, 32, &errbuf[0], np->n_name, error);
		np->f_openState = kNeedRevoke;
		error = EIO;
	}
	lck_mtx_unlock(&np->f_openStateLock);

	return(error);
}

PRIVSYM int
smbfs_smb_close(struct smb_share *ssp, u_int16_t fid, struct smb_cred *scrp)
{
	struct smb_rq rq, *rqp = &rq;
	struct mbchain *mbp;
	long time;
	int error;

	/* 
	 * Notice a close with a zero file descriptor in a packet trace. I have no idea how
	 * that could happen. Putting in this assert to see if we can every catch it happening
	 * again.
	 */
	DBG_ASSERT(fid);
	error = smb_rq_init(rqp, SSTOCP(ssp), SMB_COM_CLOSE, scrp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_mem(mbp, (caddr_t)&fid, sizeof(fid), MB_MSYSTEM);
	/*
	 * Never set the modify time on close. Just a really bad idea!
	 *
	 * Leach and SNIA docs say to send zero here.  X/Open says
	 * 0 and -1 both are leaving timestamp up to the server.
	 * Win9x treats zero as a real time-to-be-set!  We send -1,
	 * same as observed with smbclient.
	 */
	time = -1;
	mb_put_uint32le(mbp, time);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	smb_rq_done(rqp);
	/*
	 * ENOTCONN isn't interesting - if the connection is closed,
	 * so are all our FIDs - and ENXIO is also not interesting,
	 * as it means a forced unmount was done.
	 *
	 * EBADF means the fid is no longer good. Reconnect will make this happen.
	 * Should we check to see if the open was broken on the reconnect or does it
	 * really matter?
	 *
	 * Don't clog up the system log with warnings about those failures
	 * on closes.
	 */
	if ((error == ENOTCONN) || (error == ENXIO) || (error == EBADF))
		error = 0;
	return error;
}

static int
smbfs_smb_oldcreate(struct smbnode *dnp, const char *name, int nmlen,
	struct smb_cred *scrp, u_int16_t *fidp, int xattr)
{
	struct smb_rq rq, *rqp = &rq;
	struct smb_share *ssp = dnp->n_mount->sm_share;
	struct mbchain *mbp;
	struct mdchain *mdp;
	struct timespec ctime;
	u_int8_t wc;
	long tm;
	int error;
	u_int16_t attr = SMB_FA_ARCHIVE;

	error = smb_rq_init(rqp, SSTOCP(ssp), SMB_COM_CREATE, scrp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	if (name && *name == '.')
		attr |= SMB_FA_HIDDEN;
	mb_put_uint16le(mbp, attr);		/* attributes  */
	nanotime(&ctime);
	smb_time_local2server(&ctime, SSTOVC(ssp)->vc_sopt.sv_tz, &tm);
	mb_put_uint32le(mbp, tm);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint8(mbp, SMB_DT_ASCII);
	error = smbfs_fullpath(mbp, SSTOVC(ssp), dnp, name, &nmlen, UTF_SFM_CONVERSIONS,
				xattr ? ':' : '\\');
	if (!error) {
		smb_rq_bend(rqp);
		error = smb_rq_simple(rqp);
		if (!error) {
			smb_rq_getreply(rqp, &mdp);
			md_get_uint8(mdp, &wc);
			if (wc == 1)
				md_get_uint16(mdp, fidp);
			else
				error = EBADRPC;
		}
	}
	smb_rq_done(rqp);
	return error;
}

PRIVSYM int
smbfs_smb_create(struct smbnode *dnp, const char *name, int nmlen, u_int32_t rights,
	struct smb_cred *scrp, u_int16_t *fidp, u_int32_t disp, int xattr, struct smbfattr *fap)
{
	struct smb_vc *vcp = SSTOVC(dnp->n_mount->sm_share);

	/*
	 * When the SMB_CAP_NT_SMBS mode is set we can pass access rights into the create
	 * call. Windows 95/98/Me do not support SMB_CAP_NT_SMBS so they get the same 
	 * treatment as before. We are not removing any support just adding.
	 */
	if (vcp->vc_sopt.sv_caps & SMB_CAP_NT_SMBS) {
		return (smbfs_smb_ntcreatex(dnp, rights, NTCREATEX_SHARE_ACCESS_ALL,
					scrp, VREG,  fidp, name, nmlen, disp, xattr, fap));
	} else
		return (smbfs_smb_oldcreate(dnp, name, nmlen, scrp, fidp, xattr));
}

#ifdef POSIX_UNLINK_NOT_YET
/*
 * Not sure we need this yet. It currently doesn't work, if I have time we should implement it for
 * open deletes.
 */
static int smbfs_posix_unlink(struct smbnode *np, struct smb_cred *scrp, const char *name, int nmlen)
{
	struct smb_t2rq *t2p;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct mbchain *mbp;
	int error;
	
	error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_SET_PATH_INFORMATION, scrp, &t2p);
	if (error)
		return error;
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	mb_put_uint16le(mbp, SMB_SFILEINFO_POSIX_UNLINK);
	mb_put_uint32le(mbp, 0);	
	error = smbfs_fullpath(mbp, SSTOVC(ssp), np, name, &nmlen, UTF_SFM_CONVERSIONS, '\\');
	if (error) {
		smb_t2_done(t2p);
		return error;
	}
	
	mbp = &t2p->t2_tdata;
	mb_init(mbp);
	t2p->t2_maxpcount = 2;
	t2p->t2_maxdcount = SSTOVC(ssp)->vc_txmax;
	error = smb_t2_request(t2p);
	smb_t2_done(t2p);
	return error;
}
#endif // POSIX_UNLINK_NOT_YET

int smbfs_smb_delete(struct smbnode *np, struct smb_cred *scrp, const char *name, int nmlen, int xattr)
{
	struct smb_rq rq, *rqp = &rq;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct mbchain *mbp;
	int error;

#ifdef POSIX_UNLINK_NOT_YET
	/* Not doing extended attribute and they support posix the use the posix unlink call */
	if (!xattr && (UNIX_CAPS(SSTOVC(ssp)) & CIFS_UNIX_POSIX_PATH_OPERATIONS_CAP))
		return smbfs_posix_unlink(np, scrp, name, nmlen);
#endif // POSIX_UNLINK_NOT_YET
	
	error = smb_rq_init(rqp, SSTOCP(ssp), SMB_COM_DELETE, scrp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint16le(mbp, SMB_FA_SYSTEM | SMB_FA_HIDDEN);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint8(mbp, SMB_DT_ASCII);
	error = smbfs_fullpath(mbp, SSTOVC(ssp), np, name, &nmlen, UTF_SFM_CONVERSIONS, 
				xattr ? ':' : '\\');
	if (!error) {
		smb_rq_bend(rqp);
		error = smb_rq_simple(rqp);
	}
	/*
	 * We could have sent the delete before the connection went down, but we lost the
	 * response. We resent the delete, but since it already succeeded we got an ENOENT
	 * error. We are deleting something that does not exist, since it happen during
	 * a reconnect take a guess that we succeeded.
	 */
	if ((error == ENOENT) && (rqp->sr_flags & SMBR_REXMIT))
		error = 0;
	smb_rq_done(rqp);
	return error;
}

int
smbfs_smb_rename(struct smbnode *src, struct smbnode *tdnp,
	const char *tname, int tnmlen, struct smb_cred *scrp)
{
	struct smb_rq rq, *rqp = &rq;
	struct smb_share *ssp = src->n_mount->sm_share;
	struct mbchain *mbp;
	int error, retest = 0;

	error = smb_rq_init(rqp, SSTOCP(ssp), SMB_COM_RENAME, scrp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	/* freebsd bug: Let directories be renamed - Win98 requires DIR bit */
	mb_put_uint16le(mbp, (vnode_isdir(SMBTOV(src)) ? SMB_FA_DIR : 0) |
			     SMB_FA_SYSTEM | SMB_FA_HIDDEN);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint8(mbp, SMB_DT_ASCII);
	do {
		error = smbfs_fullpath(mbp, SSTOVC(ssp), src, NULL, NULL, UTF_SFM_CONVERSIONS, '\\');
		if (error)
			break;
		mb_put_uint8(mbp, SMB_DT_ASCII);
		error = smbfs_fullpath(mbp, SSTOVC(ssp), tdnp, tname, &tnmlen, UTF_SFM_CONVERSIONS, '\\');
		if (error)
			break;
		smb_rq_bend(rqp);
		error = smb_rq_simple(rqp);
	} while(0);
	if ((error == ENOENT) && (rqp->sr_flags & SMBR_REXMIT))
		retest = 1;
	smb_rq_done(rqp);
	/*
	 * We could have sent the rename before the connection went down, but we lost the
	 * response. We resent the rename message, but since it already succeeded we got an ENOENT
	 * error. So lets test to see if the rename worked or not.
	 *
	 *	1. Check to make sure the source doesn't exist.
	 *	2. Check to make sure the dest does exist.
	 *
	 * If either fails then we leave the error alone, we could still be wrong here. Someone
	 * could have played with either file  between the time we lost the connection and the
	 * time we do our test. Not trying to be prefect here just to the best we can.
	 */
	if (error && retest) {
		struct smbfattr fattr;

		if ((smbfs_smb_query_info(src, NULL, 0, &fattr, scrp) == ENOENT) &&
			(smbfs_smb_query_info(tdnp, tname, tnmlen, &fattr, scrp) == 0))
			error = 0;
	}
	return error;
}

static int
smbfs_smb_oldmkdir(struct smbnode *dnp, const char *name, int len,
		   struct smb_cred *scrp)
{
	struct smb_rq rq, *rqp = &rq;
	struct smb_share *ssp = dnp->n_mount->sm_share;
	struct mbchain *mbp;
	int error;

	error = smb_rq_init(rqp, SSTOCP(ssp), SMB_COM_CREATE_DIRECTORY, scrp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint8(mbp, SMB_DT_ASCII);
	error = smbfs_fullpath(mbp, SSTOVC(ssp), dnp, name, &len, UTF_SFM_CONVERSIONS, '\\');
	if (!error) {
		smb_rq_bend(rqp);
		error = smb_rq_simple(rqp);
	}
	smb_rq_done(rqp);
	return error;
}

PRIVSYM int
smbfs_smb_mkdir(struct smbnode *dnp, const char *name, int len, struct smb_cred *scrp, struct smbfattr *fap)
{
	struct smb_share *ssp = dnp->n_mount->sm_share;
	u_int16_t fid;
	int error;

	/*
	 * We ask for SA_RIGHT_FILE_READ_DATA not because we need it, but
	 * just to be asking for something.  The rights==0 case could
	 * easily be broken on some old or unusual servers.
	 */
	if (SSTOVC(ssp)->vc_sopt.sv_caps & SMB_CAP_NT_SMBS) {
		error = smbfs_smb_ntcreatex(dnp, SA_RIGHT_FILE_READ_DATA, NTCREATEX_SHARE_ACCESS_ALL,
					scrp, VDIR, &fid, name, len, NTCREATEX_DISP_CREATE, 0, fap);
		if (error)
			return (error);
		error = smbfs_smb_close(ssp, fid, scrp);
		if (error)
			SMBERROR("error %d closing fid %d\n", error, fid);
		return (0);
	} else
		return (smbfs_smb_oldmkdir(dnp, name, len, scrp));
}

int
smbfs_smb_rmdir(struct smbnode *np, struct smb_cred *scrp)
{
	struct smb_rq rq, *rqp = &rq;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct mbchain *mbp;
	int error;

	error = smb_rq_init(rqp, SSTOCP(ssp), SMB_COM_DELETE_DIRECTORY, scrp);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint8(mbp, SMB_DT_ASCII);
	error = smbfs_fullpath(mbp, SSTOVC(ssp), np, NULL, NULL, UTF_SFM_CONVERSIONS, '\\');
	if (!error) {
		smb_rq_bend(rqp);
		error = smb_rq_simple(rqp);
	}
	/*
	 * We could have sent the delete before the connection went down, but we lost the
	 * response. We resent the delete, but since it already succeeded we got an ENOENT
	 * error. We are deleting something that does not exist, since it happen during
	 * a reconnect take a guess that we succeeded.
	 */
	if ((error == ENOENT) && (rqp->sr_flags & SMBR_REXMIT))
		error = 0;
	smb_rq_done(rqp);
	return error;
}

static int
smbfs_smb_search(struct smbfs_fctx *ctx)
{
	struct smb_vc *vcp = SSTOVC(ctx->f_ssp);
	struct smb_rq *rqp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int8_t wc, bt;
	u_int16_t ec, dlen, bc;
	int len, maxent,error, iseof = 0;

	maxent = min(ctx->f_left,
		     (vcp->vc_txmax - SMB_HDRLEN - 2*2) / SMB_DENTRYLEN);
	if (ctx->f_rq) {
		smb_rq_done(ctx->f_rq);
		ctx->f_rq = NULL;
	}
	error = smb_rq_alloc(SSTOCP(ctx->f_ssp), SMB_COM_SEARCH, ctx->f_scred, &rqp);
	if (error)
		return error;
	ctx->f_rq = rqp;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint16le(mbp, maxent);	/* max entries to return */
	mb_put_uint16le(mbp, ctx->f_attrmask);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint8(mbp, SMB_DT_ASCII);	/* buffer format */
	if (ctx->f_flags & SMBFS_RDD_FINDFIRST) {
		len = ctx->f_wclen;
		error = smbfs_fullpath(mbp, vcp, ctx->f_dnp, ctx->f_wildcard,
				       &len, ctx->f_sfm_conversion, '\\');
		if (error)
			return error;
		mb_put_uint8(mbp, SMB_DT_VARIABLE);
		mb_put_uint16le(mbp, 0);	/* context length */
		/* Turn on the SFM Conversion flag. Next call is a FindNext */
		ctx->f_flags &= ~SMBFS_RDD_FINDFIRST;
		ctx->f_sfm_conversion = UTF_SFM_CONVERSIONS;
	} else {
		/* XXX - use "smbfs_fullpath()" and a null string? */
		if (SMB_UNICODE_STRINGS(vcp)) {
			mb_put_padbyte(mbp);
			mb_put_uint8(mbp, 0);
		}
		mb_put_uint8(mbp, 0);
		mb_put_uint8(mbp, SMB_DT_VARIABLE);
		mb_put_uint16le(mbp, SMB_SKEYLEN);
		mb_put_mem(mbp, (caddr_t)(ctx->f_skey), SMB_SKEYLEN, MB_MSYSTEM);
	}
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	if (rqp->sr_errclass == ERRDOS && rqp->sr_serror == ERRnofiles) {
		error = 0;
		iseof = 1;
		ctx->f_flags |= SMBFS_RDD_EOF;
	} else if (error)
		return error;
	smb_rq_getreply(rqp, &mdp);
	md_get_uint8(mdp, &wc);
	if (wc != 1) 
		return iseof ? ENOENT : EBADRPC;
	md_get_uint16le(mdp, &ec);
	if (ec == 0)
		return ENOENT;
	ctx->f_ecnt = ec;
	md_get_uint16le(mdp, &bc);
	if (bc < 3)
		return EBADRPC;
	bc -= 3;
	md_get_uint8(mdp, &bt);
	if (bt != SMB_DT_VARIABLE)
		return EBADRPC;
	md_get_uint16le(mdp, &dlen);
	if (dlen != bc || dlen % SMB_DENTRYLEN != 0)
		return EBADRPC;
	return 0;
}

static int
smbfs_smb_findopenLM1(struct smbfs_fctx *ctx, struct smbnode *dnp,
	const char *wildcard, int wclen, int attr, struct smb_cred *scrp)
{
	#pragma unused(dnp, scrp)
	ctx->f_attrmask = attr;
	if (wildcard) {
		if (wclen == 1 && wildcard[0] == '*') {
			ctx->f_wildcard = "*.*";
			ctx->f_wclen = 3;
		} else {
			ctx->f_wildcard = wildcard;
			ctx->f_wclen = wclen;
		}
	} else {
		ctx->f_wildcard = NULL;
		ctx->f_wclen = 0;
	}
	ctx->f_name = (char *)(ctx->f_fname);
	return 0;
}

static int
smbfs_smb_findnextLM1(struct smbfs_fctx *ctx, int limit)
{
	struct mdchain *mdp;
	struct smb_rq *rqp;
	char *cp;
	u_int8_t battr;
	u_int16_t date, time;
	u_int32_t size;
	int error;
	struct timespec ts;

	if (ctx->f_ecnt == 0) {
		if (ctx->f_flags & SMBFS_RDD_EOF)
			return ENOENT;
		ctx->f_left = ctx->f_limit = limit;
		nanotime(&ts);
		error = smbfs_smb_search(ctx);
		if (error)
			return error;
		ctx->f_attr.fa_reqtime = ts;
	}
	rqp = ctx->f_rq;
	smb_rq_getreply(rqp, &mdp);
	md_get_mem(mdp, (caddr_t)(ctx->f_skey), SMB_SKEYLEN, MB_MSYSTEM);
	md_get_uint8(mdp, &battr);
	md_get_uint16le(mdp, &time);
	md_get_uint16le(mdp, &date);
	md_get_uint32le(mdp, &size);
	cp = ctx->f_name;
	md_get_mem(mdp, cp, sizeof(ctx->f_fname), MB_MSYSTEM);
	cp[sizeof(ctx->f_fname) - 1] = 0;
	cp += strlen(cp) - 1;
	while (*cp == ' ' && cp >= ctx->f_name)
		*cp-- = 0;
	ctx->f_attr.fa_attr = battr;
	ctx->f_attr.fa_vtype = (ctx->f_attr.fa_attr & SMB_FA_DIR) ? VDIR : VREG;
	smb_dos2unixtime(date, time, 0, rqp->sr_vc->vc_sopt.sv_tz,
	    &ctx->f_attr.fa_mtime);
	ctx->f_attr.fa_size = size;
	/* 
	 * XXX - Once we drop Windows for Workgroup support we can remove this whole
	 * routine. Until then just fake the size here and use 512 for the block size.
	 */
	ctx->f_attr.fa_data_alloc = roundup(size, 512);
	ctx->f_nmlen = strlen(ctx->f_name);
	ctx->f_ecnt--;
	ctx->f_left--;
	return 0;
}

static int
smbfs_smb_findcloseLM1(struct smbfs_fctx *ctx)
{
	if (ctx->f_rq)
		smb_rq_done(ctx->f_rq);
	return 0;
}

/*
 * TRANS2_FIND_FIRST2/NEXT2, used for NT LM12 dialect
 */
static int
smbfs_smb_trans2find2(struct smbfs_fctx *ctx)
{
	struct smb_t2rq *t2p;
	struct smb_vc *vcp = SSTOVC(ctx->f_ssp);
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int16_t tw, flags;
	int len, error;

	if (ctx->f_t2) {
		smb_t2_done(ctx->f_t2);
		ctx->f_t2 = NULL;
	}
	ctx->f_flags &= ~SMBFS_RDD_GOTRNAME;
	flags = FIND2_RETURN_RESUME_KEYS | FIND2_CLOSE_ON_EOS;
	if (ctx->f_flags & SMBFS_RDD_FINDSINGLE) {
		flags |= FIND2_CLOSE_AFTER_REQUEST;
		ctx->f_flags |= SMBFS_RDD_NOCLOSE;
	}
	if (ctx->f_flags & SMBFS_RDD_FINDFIRST) {
		error = smb_t2_alloc(SSTOCP(ctx->f_ssp), SMB_TRANS2_FIND_FIRST2,
		    ctx->f_scred, &t2p);
		if (error)
			return error;
		ctx->f_t2 = t2p;
		mbp = &t2p->t2_tparam;
		mb_init(mbp);
		mb_put_uint16le(mbp, ctx->f_attrmask);
		mb_put_uint16le(mbp, ctx->f_limit);
		mb_put_uint16le(mbp, flags);
		mb_put_uint16le(mbp, ctx->f_infolevel);
		mb_put_uint32le(mbp, 0);
		len = ctx->f_wclen;
		error = smbfs_fullpath(mbp, vcp, ctx->f_dnp, ctx->f_wildcard,
				       &len, ctx->f_sfm_conversion, '\\');
		if (error)
			return error;
	} else	{
		error = smb_t2_alloc(SSTOCP(ctx->f_ssp), SMB_TRANS2_FIND_NEXT2,
		    ctx->f_scred, &t2p);
		if (error)
			return error;
		ctx->f_t2 = t2p;
		mbp = &t2p->t2_tparam;
		mb_init(mbp);
		mb_put_mem(mbp, (caddr_t)&ctx->f_Sid, 2, MB_MSYSTEM);
		mb_put_uint16le(mbp, ctx->f_limit);
		mb_put_uint16le(mbp, ctx->f_infolevel);
		/* If they give us a resume key return it */
		mb_put_uint32le(mbp, ctx->f_rkey);
		mb_put_uint16le(mbp, flags);
		if (ctx->f_rname) {
			/* resume file name */
			mb_put_mem(mbp, ctx->f_rname, ctx->f_rnamelen, MB_MSYSTEM);
		}
		/* Add trailing null - 1 byte if ASCII, 2 if Unicode */
		if (SMB_UNICODE_STRINGS(SSTOVC(ctx->f_ssp)))
			mb_put_uint8(mbp, 0);	/* 1st byte of NULL Unicode char */
		mb_put_uint8(mbp, 0);
	}
	t2p->t2_maxpcount = 5 * 2;
	t2p->t2_maxdcount = vcp->vc_txmax;
	error = smb_t2_request(t2p);
	if (error)
		return error;
	mdp = &t2p->t2_rparam;
	if (ctx->f_flags & SMBFS_RDD_FINDFIRST) {
		if ((error = md_get_uint16(mdp, &ctx->f_Sid)) != 0)
			return error;
		/* Turn on the SFM Conversion flag. Next call is a FindNext */
		ctx->f_flags &= ~SMBFS_RDD_FINDFIRST;
		ctx->f_sfm_conversion = UTF_SFM_CONVERSIONS;
	}
	if ((error = md_get_uint16le(mdp, &tw)) != 0)
		return error;
	ctx->f_ecnt = tw; /* search count - # entries returned */
	if ((error = md_get_uint16le(mdp, &tw)) != 0)
		return error;
	/*
	 * tw now is the "end of search" flag. against an XP server tw
	 * comes back zero when the prior find_next returned exactly
	 * the number of entries requested.  in which case we'd try again
	 * but the search has in fact been closed so an EBADF results.  our
	 * circumvention is to check here for a zero search count.
	 */
	if (tw || ctx->f_ecnt == 0)
		ctx->f_flags |= SMBFS_RDD_EOF | SMBFS_RDD_NOCLOSE;
	if ((error = md_get_uint16le(mdp, &tw)) != 0)
		return error;
	if ((error = md_get_uint16le(mdp, &tw)) != 0)
		return error;
	if (ctx->f_ecnt == 0)
		return ENOENT;
	ctx->f_rnameofs = tw;
	mdp = &t2p->t2_rdata;
	if (mdp->md_top == NULL) {
		SMBERROR("bug: ecnt = %d, but data is NULL (please report)\n", ctx->f_ecnt);
		/*
		 * Something bad has happened we did not get all the data. We 
		 * need to close the directory listing, otherwise we may cause 
		 * the calling process to hang in an infinite loop. 
		 */
		ctx->f_ecnt = 0; /* Force the directory listing to close. */
		ctx->f_flags |= SMBFS_RDD_EOF;
		return ENOENT;
	}
	/*
	 * Changed the code to check to see if there is any data on the whole 
	 * mbuf chain, not just the first mbuf. Remember in a mbuf chain there 
	 * can be mbufs with nothing in them. Need to make sure we have data in 
	 * the mbuf chain, if not we received a bad message.
	 */
	 if (mbuf_pkthdr_len(mdp->md_top) == 0) {
		SMBERROR("bug: ecnt = %d, but m_len = 0 and m_next = %p (please report)\n", ctx->f_ecnt, mbuf_next(mbp->mb_top));
		/*
		 * Something bad has happened we did not get all the data. We 
		 * need to close the directory listing, otherwise we may cause 
		 * the calling process to hang in an infinite loop. 
		 */
		ctx->f_ecnt = 0; /* Force the directory listing to close. */
		ctx->f_flags |= SMBFS_RDD_EOF;
		return ENOENT;
	}
	ctx->f_eofs = 0;
	return 0;
}

static int
smbfs_smb_findclose2(struct smbfs_fctx *ctx)
{
	struct smb_rq rq, *rqp = &rq;
	struct mbchain *mbp;
	int error;

	error = smb_rq_init(rqp, SSTOCP(ctx->f_ssp), SMB_COM_FIND_CLOSE2, ctx->f_scred);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_mem(mbp, (caddr_t)&ctx->f_Sid, 2, MB_MSYSTEM);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	smb_rq_done(rqp);
	return error;
}

static int
smbfs_smb_findopenLM2(struct smbfs_fctx *ctx, struct smbnode *dnp,
	const char *wildcard, int wclen, int attr, struct smb_cred *scrp)
{
	#pragma unused(dnp, scrp)
	if (SMB_UNICODE_STRINGS(SSTOVC(ctx->f_ssp)))
		ctx->f_name = malloc(SMB_MAXFNAMELEN*2, M_SMBFSDATA, M_WAITOK);
	else
		ctx->f_name = malloc(SMB_MAXFNAMELEN, M_SMBFSDATA, M_WAITOK);
	if (ctx->f_name == NULL)
		return ENOMEM;
	if (UNIX_CAPS(SSTOVC(dnp->n_mount->sm_share)) & CIFS_UNIX_POSIX_PATH_OPERATIONS_CAP)
		ctx->f_infolevel = SMB_FIND_FILE_UNIX_INFO2;
	else
		ctx->f_infolevel = SMB_FIND_BOTH_DIRECTORY_INFO;
	ctx->f_attrmask = attr;
	ctx->f_wildcard = wildcard;
	ctx->f_wclen = wclen;
	return 0;
}

static int
smbfs_smb_findnextLM2(struct smbfs_fctx *ctx, int limit)
{
	struct mdchain *mdp;
	struct smb_t2rq *t2p;
	char *cp;
	u_int32_t size, next, dattr, resumekey = 0;
	u_int64_t llint;
	int error, svtz, cnt, fxsz, nmlen, recsz;
	struct timespec ts;

	if (ctx->f_ecnt == 0) {
		if (ctx->f_flags & SMBFS_RDD_EOF)
			return ENOENT;
		ctx->f_left = ctx->f_limit = limit;
		nanotime(&ts);
		error = smbfs_smb_trans2find2(ctx);
		if (error)
			return error;
		ctx->f_attr.fa_reqtime = ts;
		ctx->f_otws++;
	}
	t2p = ctx->f_t2;
	mdp = &t2p->t2_rdata;
	svtz = SSTOVC(ctx->f_ssp)->vc_sopt.sv_tz;
	switch (ctx->f_infolevel) {
	case SMB_FIND_DIRECTORY_INFO:
	case SMB_FIND_BOTH_DIRECTORY_INFO:
		md_get_uint32le(mdp, &next);
		md_get_uint32le(mdp, &resumekey); /* file index (resume key) */
		md_get_uint64le(mdp, &llint);	/* creation time */
		if (llint) {
			smb_time_NT2local(llint, svtz, &ctx->f_attr.fa_crtime);
		}
		md_get_uint64le(mdp, &llint);
		if (llint) {
			smb_time_NT2local(llint, svtz, &ctx->f_attr.fa_atime);
		}
		md_get_uint64le(mdp, &llint);
		if (llint) {
			smb_time_NT2local(llint, svtz, &ctx->f_attr.fa_mtime);
		}
		md_get_uint64le(mdp, &llint);
		if (llint) {
			smb_time_NT2local(llint, svtz, &ctx->f_attr.fa_chtime);
		}
		md_get_uint64le(mdp, &llint);	/* data size */
		ctx->f_attr.fa_size = llint;
		md_get_uint64le(mdp, &llint);	/* data allocation size */
		ctx->f_attr.fa_data_alloc = llint;
		/* freebsd bug: fa_attr endian bug */
		md_get_uint32le(mdp, &dattr);	/* extended file attributes */
		ctx->f_attr.fa_attr = dattr;
		ctx->f_attr.fa_vtype = (ctx->f_attr.fa_attr & SMB_FA_DIR) ? VDIR : VREG;
		md_get_uint32le(mdp, &size);	/* name len */
		fxsz = 64; /* size ofinfo up to filename */
		if (ctx->f_infolevel == SMB_FIND_BOTH_DIRECTORY_INFO) {
			/* 
			 * Skip EaSize (4 bytes), a byte of ShortNameLength,
			 * a reserved byte, and ShortName (8.3 means 24 bytes,
			 * as Leach defined it to always be Unicode)
			 */
			md_get_mem(mdp, NULL, 30, MB_MSYSTEM);
			fxsz += 30;
		}
		recsz = next ? next : fxsz + size;
		break;
	case SMB_FIND_FILE_UNIX_INFO2:
		md_get_uint32le(mdp, &next);
		md_get_uint32le(mdp, &resumekey); /* file index (resume key) */
		
		
		md_get_uint64le(mdp, &llint);	/* file size */
		ctx->f_attr.fa_size = llint;
		
		md_get_uint64le(mdp, &llint); /* allocation size */
		ctx->f_attr.fa_data_alloc = llint;
		
		md_get_uint64le(mdp, &llint);	/* change time */
		if (llint)
			smb_time_NT2local(llint, svtz, &ctx->f_attr.fa_chtime);
		
		md_get_uint64le(mdp, &llint);	/* access time */
		if (llint)
			smb_time_NT2local(llint, svtz, &ctx->f_attr.fa_atime);
		
		md_get_uint64le(mdp, &llint);	/* write time */
		if (llint)
			smb_time_NT2local(llint, svtz, &ctx->f_attr.fa_mtime);
		
		md_get_uint64le(mdp, &llint);	/* Numeric user id for the owner */
		ctx->f_attr.fa_uid = llint;
		
		md_get_uint64le(mdp, &llint);	/* Numeric group id for the owner */
		ctx->f_attr.fa_gid = llint;
		
		md_get_uint32le(mdp, &dattr);	/* Enumeration specifying the file type, st_mode */
		/* Make sure the dos attributes are correct */ 
		if (dattr & EXT_UNIX_DIR) {
			ctx->f_attr.fa_attr |= SMB_FA_DIR;
			ctx->f_attr.fa_vtype = VDIR;
		} else {
			ctx->f_attr.fa_attr &= ~SMB_FA_DIR;
			if (dattr & EXT_UNIX_SYMLINK)
				ctx->f_attr.fa_vtype = VLNK;
			else	/* Do we ever what to handle the others? */
				ctx->f_attr.fa_vtype = VREG;
		}
		
		md_get_uint64le(mdp, &llint);	/* Major device number if type is device */
		md_get_uint64le(mdp, &llint);	/* Minor device number if type is device */
		md_get_uint64le(mdp, &llint);	/* This is a server-assigned unique id */
		md_get_uint64le(mdp, &llint);	/* Standard UNIX permissions */
		ctx->f_attr.fa_permissions = llint;
		md_get_uint64le(mdp, &llint);	/* Number of hard link */
		ctx->f_attr.fa_nlinks = llint;
		
		md_get_uint64le(mdp, &llint);	/* creation time */
		if (llint)
			smb_time_NT2local(llint, svtz, &ctx->f_attr.fa_crtime);
		
		md_get_uint32le(mdp, &dattr);	/* File flags enumeration */
		md_get_uint32le(mdp, &ctx->f_attr.fa_flags_mask);	/* Mask of valid flags */
		if (ctx->f_attr.fa_flags_mask & EXT_HIDDEN) {
			if (dattr & EXT_HIDDEN)
				ctx->f_attr.fa_attr |= SMB_FA_HIDDEN;
			else
				ctx->f_attr.fa_attr &= ~SMB_FA_HIDDEN;
		}
		if (ctx->f_attr.fa_flags_mask & EXT_IMMUTABLE) {
			if (dattr & EXT_IMMUTABLE)
				ctx->f_attr.fa_attr |= SMB_FA_RDONLY;
			else
				ctx->f_attr.fa_attr &= ~SMB_FA_RDONLY;
		}
		if (ctx->f_attr.fa_flags_mask & EXT_DO_NOT_BACKUP) {
			if (dattr & EXT_DO_NOT_BACKUP)
				ctx->f_attr.fa_attr &= ~SMB_FA_ARCHIVE;
			else
				ctx->f_attr.fa_attr |= SMB_FA_ARCHIVE;
		}
		
		ctx->f_attr.fa_unix = TRUE;

		md_get_uint32le(mdp, &size);	/* name len */
		fxsz = 128; /* size ofinfo up to filename */
		recsz = next ? next : fxsz + size;
		break;
	default:
		SMBERROR("unexpected info level %d\n", ctx->f_infolevel);
		return EINVAL;
	}
	if (SMB_UNICODE_STRINGS(SSTOVC(ctx->f_ssp)))
		nmlen = min(size, SMB_MAXFNAMELEN * 2);
	else
		nmlen = min(size, SMB_MAXFNAMELEN);
	cp = ctx->f_name;
	error = md_get_mem(mdp, cp, nmlen, MB_MSYSTEM);
	if (error)
		return error;
	if (next) {
		cnt = next - nmlen - fxsz;
		if (cnt > 0)
			md_get_mem(mdp, NULL, cnt, MB_MSYSTEM);
		else if (cnt < 0) {
			SMBERROR("out of sync\n");
			return EBADRPC;
		}
	}
	/* Don't count any trailing null in the name. */
	if (SMB_UNICODE_STRINGS(SSTOVC(ctx->f_ssp))) {
		if (nmlen > 1 && cp[nmlen - 1] == 0 && cp[nmlen - 2] == 0)
			nmlen -= 2;
	} else {
		if (nmlen && cp[nmlen - 1] == 0)
			nmlen--;
	}
	if (nmlen == 0)
		return EBADRPC;

	/*
	 * Ref radar 3983209.  On a find-next we expect a server will
	 * 1) if the continue bit is set, use the server's idea of current loc,
	 * 2) else if the resume key is non-zero, use that location,
	 * 3) else if the resume name is set, use that location,
	 * 4) else use the server's idea of current location.
	 *
	 * There was some crazy code here to work around a NetApp bug, but it
	 * was wrong. We always set the resume key flag. If the server returns a
	 * resume key then we should send it back to them. This would have solve 
	 * the NetApp bug.  
	 */
	ctx->f_rkey = resumekey;

	next = ctx->f_eofs + recsz;
	if (ctx->f_rnameofs &&
		(ctx->f_flags & SMBFS_RDD_GOTRNAME) == 0 &&
	    (ctx->f_rnameofs >= ctx->f_eofs &&
		ctx->f_rnameofs < (int)next)) {
		/*
		 * Server needs a resume filename.
		 */
		if (ctx->f_rnamelen < nmlen) {
			if (ctx->f_rname)
				free(ctx->f_rname, M_SMBFSDATA);
			ctx->f_rname = malloc(nmlen, M_SMBFSDATA, M_WAITOK);
		}
		ctx->f_rnamelen = nmlen;
		bcopy(ctx->f_name, ctx->f_rname, nmlen);
		ctx->f_flags |= SMBFS_RDD_GOTRNAME;
	}
	ctx->f_nmlen = nmlen;
	ctx->f_eofs = next;
	ctx->f_ecnt--;
	ctx->f_left--;
	return 0;
}

static int
smbfs_smb_findcloseLM2(struct smbfs_fctx *ctx)
{
	if (ctx->f_name)
		free(ctx->f_name, M_SMBFSDATA);
	if (ctx->f_t2)
		smb_t2_done(ctx->f_t2);
	if ((ctx->f_flags & SMBFS_RDD_NOCLOSE) == 0)
		smbfs_smb_findclose2(ctx);
	return 0;
}

PRIVSYM int
smbfs_smb_findopen(struct smbnode *dnp, const char *wildcard, int wclen, int attr,
	struct smb_cred *scrp, struct smbfs_fctx **ctxpp, int conversion_flag)
{
	struct smbfs_fctx *ctx;
	int error;

	ctx = malloc(sizeof(*ctx), M_SMBFSDATA, M_WAITOK);
	if (ctx == NULL)
		return ENOMEM;
	bzero(ctx, sizeof(*ctx));
	if (dnp->n_mount->sm_share) {
		ctx->f_ssp = dnp->n_mount->sm_share;
		smb_share_ref(ctx->f_ssp);
	}
	ctx->f_dnp = dnp;
	/*
	 * If they are doing a wildcard lookup don't set the SFM Conversion flag.
	 * Check to see if the name is a wildcard name. If it is a wildcard name
	 * then make sure we are not setting the UTF_SFM_CONVERSIONS flag. Never
	 * set the UTF_SFM_CONVERSIONS on a wildcard lookup. Since only FindFirst
	 * message can do wildcard lookup reset f_sfm_conversion once we turn off
	 * SMBFS_RDD_FINDFIRST flag. This means we are doing a FindNext message 
	 * and we need to have UTF_SFM_CONVERSIONS flag set.
	 */
	ctx->f_flags = SMBFS_RDD_FINDFIRST;
	ctx->f_sfm_conversion = conversion_flag;	/* Is this a wildcard lookup or a name lookup */
	ctx->f_scred = scrp;
	if (SMB_DIALECT(SSTOVC(ctx->f_ssp)) < SMB_DIALECT_LANMAN2_0 ||
	    (dnp->n_mount->sm_args.altflags & SMBFS_MOUNT_NO_LONG)) {
		ctx->f_flags |= SMBFS_RDD_USESEARCH;
		error = smbfs_smb_findopenLM1(ctx, dnp, wildcard, wclen, attr, scrp);
	} else
		error = smbfs_smb_findopenLM2(ctx, dnp, wildcard, wclen, attr, scrp);
	if (error)
		smbfs_smb_findclose(ctx, scrp);
	else
		*ctxpp = ctx;
	return error;
}

PRIVSYM int
smbfs_smb_findnext(struct smbfs_fctx *ctx, int limit, struct smb_cred *scrp)
{
	int error;

	if (limit == 0)
		limit = 1000000;
	else
		limit += 3; /* ensures we ask for 1 extra, plus . and .. */
	ctx->f_scred = scrp;
	for (;;) {
		if (ctx->f_flags & SMBFS_RDD_USESEARCH) {
			error = smbfs_smb_findnextLM1(ctx, limit);
		} else
			error = smbfs_smb_findnextLM2(ctx, limit);
		if (error)
			return error;
		if (SMB_UNICODE_STRINGS(SSTOVC(ctx->f_ssp))) {
			if ((ctx->f_nmlen == 2 &&
			     letohs(*(u_int16_t *)ctx->f_name) == 0x002e) ||
			    (ctx->f_nmlen == 4 &&
			     letohl(*(u_int32_t *)ctx->f_name) == 0x002e002e))
				continue;
		} else {
			if ((ctx->f_nmlen == 1 && ctx->f_name[0] == '.') ||
			    (ctx->f_nmlen == 2 && ctx->f_name[0] == '.' &&
			     ctx->f_name[1] == '.'))
				continue;
		}
		break;
	}
	smbfs_fname_tolocal(ctx);
	ctx->f_attr.fa_ino = smbfs_getino(ctx->f_dnp, ctx->f_name,
					  ctx->f_nmlen);
	return 0;
}

PRIVSYM int
smbfs_smb_findclose(struct smbfs_fctx *ctx, struct smb_cred *scrp)
{
	ctx->f_scred = scrp;
	if (ctx->f_flags & SMBFS_RDD_USESEARCH) {
		smbfs_smb_findcloseLM1(ctx);
	} else
		smbfs_smb_findcloseLM2(ctx);
	if (ctx->f_rname)
		free(ctx->f_rname, M_SMBFSDATA);
	if (ctx->f_ssp)
		smb_share_rele(ctx->f_ssp, ctx->f_scred);
	free(ctx, M_SMBFSDATA);
	return 0;
}

PRIVSYM int
smbfs_smb_lookup(struct smbnode *dnp, const char **namep, int *nmlenp,
	struct smbfattr *fap, struct smb_cred *scrp)
{
	struct smbfs_fctx *ctx;
	int error;
	const char *name = (namep ? *namep : NULL);
	int nmlen = (nmlenp ? *nmlenp : 0);
	PRIVSYM u_int64_t DIFF1980TO1601 = 11960035200ULL*10000000ULL;

	if (dnp == NULL || (dnp->n_ino == 2 && name == NULL)) {
		bzero(fap, sizeof(*fap));
		/* We keep track of the time the lookup call was requested */
		nanotime(&fap->fa_reqtime);
		fap->fa_attr = SMB_FA_DIR;
		fap->fa_vtype = VDIR;
		fap->fa_ino = 2;
		DBG_ASSERT(dnp);
		if (dnp == NULL)	/* This should no longer happen, post Leoaprd we should clean this up */
			return 0;
		/* 
		 * Windows 95/98/Me do not support create or access times on the
		 * root share. SMB_QFILEINFO_BASIC_INFO is not support with
		 * Windows 95/98/Me. On the root share Windows 95/98/Me will 
		 * return bogus information in the SMB_QFILEINFO_STANDARD call.
		 * So for Windows 95/98/Me we use the base call. This will 
		 * sometmes get us the correct modify time. 
		 *
		 * NTFS handles dates correctly, but FAT file systems have some
		 * problems. FAT DRIVEs do not support any dates. So some root 
		 * shares may have either no dates or bad dates. Windows 
		 * XP/2000/2003 will return Jan. 1, 1980 for the create, modify 
		 * and access dates. NT4 will return no dates at all. 
		 *
		 * So we follow the Windows 2000 model. If any of the time 
		 * fields are zero we fill them in with Jan 1, 1980. 
		 */ 
		if (SSTOVC(dnp->n_mount->sm_share)->vc_flags & SMBV_WIN98) {			
			error = smbfs_smb_query_info(dnp, NULL, 0, fap, scrp);
			fap->fa_atime = fap->fa_mtime;
			fap->fa_crtime.tv_sec = 0;
		}
		else {
			error = smbfs_smb_qpathinfo(dnp, fap, scrp, 0, 
				NULL, NULL);
			if (error == EINVAL)
				error = smbfs_smb_query_info(dnp, NULL, 
					0, fap, scrp);
		}
			
		if (fap->fa_mtime.tv_sec == 0)
			smb_time_NT2local(DIFF1980TO1601, 0, &fap->fa_mtime);
		if (fap->fa_crtime.tv_sec == 0)
			smb_time_NT2local(DIFF1980TO1601, 0, &fap->fa_crtime);
		if (fap->fa_atime.tv_sec == 0)
			fap->fa_atime = fap->fa_mtime;
		if (fap->fa_chtime.tv_sec == 0)
			fap->fa_chtime = fap->fa_mtime;
		return error;
	}
	if (nmlen == 1 && name[0] == '.') {
		error = smbfs_smb_lookup(dnp, NULL, NULL, fap, scrp);
		return error;
	} else if (nmlen == 2 && name[0] == '.' && name[1] == '.') {
		error = smbfs_smb_lookup(dnp->n_parent, NULL, NULL, fap, scrp);
		return error;
	}
	/*
	 * Added a new info level call. We no longer use FindFirst to do normal 
	 * lookups. If they claim to support the NT CAPS option and they do not 
	 * return EINVAL then we will use the Trans2 Query All Info call instead 
	 * of the FindFirst call. Both calls return the same information and 
	 * this call will work with Drop boxes.
	 *
	 * Setting up a Drop Box for Windows 2000 and 2003
	 * 
	 * 1. In Windows Explorer, right-click on the folder and choose 
	 *    "Properties"
	 * 2. Switch to the Security tab in the Properties dialog
	 * 3. Confirm "Allow inheritable permissions from parent to propagate 
	 *    to this object" is not checked.
	 * 4. Confirm that CREATOR OWNER exist and has full access to 
	 *    sub-folders and files.
	 * 5. Select/Add the user, group or everyone who will only have drop box
	 *    permissions.
	 * 6. Under the "Permissions" section at the bottom of the dialog, click
	 *    "Write" under the "Allow" column.
	 * 7. Click on the Advanced Tab
	 * 8. Select the user or group who will only have drop box permissions.
	 * 9. Click "View/Edit"
	 * 10. There are several different settings that will work, but here are
	 *     the ones required.
	 * 
	 *    In the Apply onto menu select "This Folder, subfolders and files".
	 * 
	 * 	Select the following under the "Allow" column:
	 * 		- Traverse Folders / Execute File
	 *		- Create Files / Write Data
	 *		- Create Folders / Append Data
	 *		- Write Attributes
	 *		- Write Extended Attributes
	 * 
	 * 11. Click OK.
	 * 12. Click Apply to apply your changes.
	 * 13. Click OK
	 * 
	 * Setting up a Drop Box for Windows NT
	 * 
	 * 1. In Windows Explorer, right-click on the folder and choose 
	 *    "Properties"
	 * 2. Switch to the Security tab in the Properties dialog
	 * 3. Click on the "Permissions" button.
	 * 4. Confirm that CREATOR OWNER exist and has full control.
	 * 5. Select/Add the user, group or everyone who will only have drop box
	 *    permissions.
	 * 6. Double click on the user, group or everyone who will only have 
	 *    drop box permissions.
	 * 7. Check the Write checkbox.
	 * 8. Un-check all the other checkboxes.
	 * 9. Click Apply to apply your changes.
	 * 10. Click OK
	 * 
	 * Setting up a Drop Box for SAMBA on a Mac OS X system
	 * 
	 * 1. Bring up a terminal.
	 * 2. Create a folder and make the owner the administrator of the 
	 *    system.
	 * 3. Now run the chmod command give everyone write and execute 
	 *    privileges. 
	 * 	e.g. chmod 733 ./DropBox
	 *
	 * If they support CIFS_UNIX_POSIX_PATH_OPERATIONS_CAP then they must support UNIX_INFO2.
	 * Hopefully everyone does this correctly. Remember the SMB_QFILEINFO_UNIX_INFO2 call does
	 * not return the name. So if they are asking for the name fall through to the find first code
	 * which will do a SMB_FIND_FILE_UNIX_INFO2 that does return the name.
	 */
	bzero(fap, sizeof(*fap));
	/* We keep track of the time the lookup call was requested */
	nanotime(&fap->fa_reqtime);
	if (UNIX_CAPS(SSTOVC(dnp->n_mount->sm_share)) & CIFS_UNIX_POSIX_PATH_OPERATIONS_CAP) {
		if (namep == NULL)
			error = smbfs_smb_qpathinfo(dnp, fap, scrp, SMB_QFILEINFO_UNIX_INFO2, namep, nmlenp);
		else /* If they are requesting the name fall through to the FindFrist call. */
			error = EINVAL;
	} else if (SSTOVC(dnp->n_mount->sm_share)->vc_sopt.sv_caps & SMB_CAP_NT_SMBS)
		error = smbfs_smb_qpathinfo(dnp, fap, scrp, SMB_QFILEINFO_ALL_INFO, namep, nmlenp);
	else 
		error = EINVAL;

	/* All EINVAL means is to fall through to the find first code */
	if (error != EINVAL) {
		if (error && (error != ENOENT) && (error != EACCES))
			SMBERROR("smbfs_smb_qpathinfo error = %d\n", error);
		return error;
	}
	
	/*
	 * This hides a server bug observable in Win98:
	 * size changes may not show until a CLOSE or a FLUSH op
	 */
	if (SSTOVC(dnp->n_mount->sm_share)->vc_flags & SMBV_WIN98)
		error = smbfs_smb_flush(dnp, scrp);

	error = smbfs_smb_findopen(dnp, name, nmlen,
			       SMB_FA_SYSTEM | SMB_FA_HIDDEN | SMB_FA_DIR,
			       scrp, &ctx, UTF_SFM_CONVERSIONS);
	if (error)
		return error;
	ctx->f_flags |= SMBFS_RDD_FINDSINGLE;
	error = smbfs_smb_findnext(ctx, 1, scrp);
	if (error == 0) {
		*fap = ctx->f_attr;
		if (name == NULL)
			fap->fa_ino = dnp->n_ino;
		if (namep)
			*namep = (char *)smbfs_name_alloc((u_char *)(ctx->f_name), ctx->f_nmlen);
		if (nmlenp)
			*nmlenp = ctx->f_nmlen;
	}
	smbfs_smb_findclose(ctx, scrp);
	return error;
}

static int
smbfs_smb_getsec_int(struct smb_share *ssp, u_int16_t fid,
		     struct smb_cred *scrp, u_int32_t selector,
		     struct ntsecdesc **res, int *reslen)
{
	struct smb_ntrq *ntp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	int error, len;

	error = smb_nt_alloc(SSTOCP(ssp), NT_TRANSACT_QUERY_SECURITY_DESC,
	    scrp, &ntp);
	if (error)
		return error;
	mbp = &ntp->nt_tparam;
	mb_init(mbp);
	mb_put_mem(mbp, (caddr_t)&fid, 2, MB_MSYSTEM);
	mb_put_uint16le(mbp, 0); /* reserved */
	mb_put_uint32le(mbp, selector);
	ntp->nt_maxpcount = 4;
	ntp->nt_maxdcount = *reslen;
	error = smb_nt_request(ntp);
	*res = NULL;
	mdp = &ntp->nt_rparam;
	if (error) {
	    	/*
		 * The error is that the buffer wasn't big enough; get
		 * the required buffer size, so our caller can try again
		 * with a bigger buffer.
		 */
		if (ntp->nt_flags & SMBT2_MOREDATA)
		    md_get_uint32le(mdp, (u_int32_t *)reslen);
		goto done;
	 }
	else md_get_uint32le(mdp, (u_int32_t *)reslen);

	mdp = &ntp->nt_rdata;
	if (mdp->md_top) {	/* XXX md_cur safer than md_top */
		len = m_fixhdr(mdp->md_top);
		/*
		 * The following "if (len < *reslen)" handles a Windows bug
		 * observed when the underlying filesystem is FAT32.  In that
		 * case a 32 byte security descriptor comes back (S-1-1-0, ie
		 * "Everyone") but the Parameter Block claims 44 is the length
		 * of the security descriptor.  (The Data Block length
		 * claimed is 32.  This server bug was reported against NT
		 * first and I've personally observed it with W2K.
		 */
		if (len < *reslen)
			*reslen = len;
		if (len == *reslen) {
			MALLOC(*res, struct ntsecdesc *, len, M_TEMP, M_WAITOK);
			md_get_mem(mdp, (caddr_t)*res, len, MB_MSYSTEM);
		} else if (len > *reslen)
			SMBERROR("len %d *reslen %d fid 0x%x\n", len, *reslen,
				 letohs(fid));
	} else
		SMBERROR("null md_top? fid 0x%x\n", letohs(fid));
done:
	smb_nt_done(ntp);
	return (error);
}

int
smbfs_smb_getsec(struct smb_share *ssp, u_int16_t fid, struct smb_cred *scrp,
	u_int32_t selector, struct ntsecdesc **res)
{
	int error, olen, seclen;
	/*
	 * We were using a hard code 500 byte request here. We now use the
	 * max transmit buffer size. This will correct the second part of Radar
	 * 4209575 slow directory listings. When the buffer size is to small
	 * we would end up making two request. Using the max transmit buffer 
	 * size should prevent this from happening.
	 */
	olen = seclen = SSTOVC(ssp)->vc_txmax;
	error = smbfs_smb_getsec_int(ssp, fid, scrp, selector, res, &seclen);
	if (error && seclen > olen)
		error = smbfs_smb_getsec_int(ssp, fid, scrp, selector, res,
					     &seclen);
	return (error);
}

int
smbfs_smb_setsec(struct smb_share *ssp, u_int16_t fid, struct smb_cred *scrp,
	u_int32_t selector, u_int16_t flags, struct ntsid *owner,
	struct ntsid *group, struct ntacl *sacl, struct ntacl *dacl)
{
	struct smb_ntrq *ntp;
	struct mbchain *mbp;
	int error, off;
	struct ntsecdesc ntsd;

	error = smb_nt_alloc(SSTOCP(ssp), NT_TRANSACT_SET_SECURITY_DESC,
	    scrp, &ntp);
	if (error)
		return error;
	mbp = &ntp->nt_tparam;
	mb_init(mbp);
	mb_put_mem(mbp, (caddr_t)&fid, 2, MB_MSYSTEM);
	mb_put_uint16le(mbp, 0); /* reserved */
	mb_put_uint32le(mbp, selector);
	mbp = &ntp->nt_tdata;
	mb_init(mbp);
	bzero(&ntsd, sizeof ntsd);
	wset_sdrevision(&ntsd);
	/*
	 * A note about flags ("SECURITY_DESCRIPTOR_CONTROL" in MSDN)
	 * We set here only those bits we can be sure must be set.  The rest
	 * are up to the caller.  In particular, the caller may intentionally
	 * set an acl PRESENT bit while giving us a null pointer for the
	 * acl - that sets a null acl, giving access to everyone.  Note also
	 * that the AUTO_INHERITED bits should probably always be set unless
	 * the server is NT.
	 */
	flags |= SD_SELF_RELATIVE;
	off = sizeof ntsd;
	if (owner) {
		wset_sdowneroff(&ntsd, off);
		off += sidlen(owner);
	}
	if (group) {
		wset_sdgroupoff(&ntsd, off);
		off += sidlen(group);
	}
	if (sacl) {
		flags |= SD_SACL_PRESENT;
		wset_sdsacloff(&ntsd, off);
		off += acllen(sacl);
	}
	if (dacl) {
		flags |= SD_DACL_PRESENT;
		wset_sddacloff(&ntsd, off);
	}
	wset_sdflags(&ntsd, flags);
	mb_put_mem(mbp, (caddr_t)&ntsd, sizeof ntsd, MB_MSYSTEM);
	if (owner)
		mb_put_mem(mbp, (caddr_t)owner, sidlen(owner), MB_MSYSTEM);
	if (group)
		mb_put_mem(mbp, (caddr_t)group, sidlen(group), MB_MSYSTEM);
	if (sacl)
		mb_put_mem(mbp, (caddr_t)sacl, acllen(sacl), MB_MSYSTEM);
	if (dacl)
		mb_put_mem(mbp, (caddr_t)dacl, acllen(dacl), MB_MSYSTEM);
	ntp->nt_maxpcount = 0;
	ntp->nt_maxdcount = 0;
	error = smb_nt_request(ntp);
	smb_nt_done(ntp);
	return (error);
}

#ifdef USE_SIDEBAND_CHANNEL_RPC
PRIVSYM 
int smbfs_spotlight(struct smbnode *np, struct smb_cred *scrp, void *idata, void *odata, size_t isize, size_t *osize)
{
	struct smb_share *ssp = np->n_mount->sm_share;
	struct smb_vc *vcp = SSTOVC(ssp);
	struct smb_t2rq *t2p;
	int error;
	struct mbchain *mbp;
	struct mdchain *mdp;

	if (isize > vcp->vc_txmax) {
		SMBERROR("smbfs_spotlight: isize %d > allowed %d\n", (int) isize, (int) vcp->vc_txmax);
		return (EINVAL);
	}

	error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_QUERY_PATH_INFORMATION, scrp, &t2p);
	if (error)
		return error;
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	mb_put_uint16le(mbp, SMB_QFILEINFO_MAC_SPOTLIGHT);
	mb_put_uint32le(mbp, 0);
	
	error = smbfs_fullpath(mbp, vcp, np, NULL, NULL, UTF_SFM_CONVERSIONS, '\\');
	if (error)
		goto out;

	mbp = &t2p->t2_tdata;
	mb_init(mbp);
	error = mb_put_mem(mbp, idata, isize, MB_MSYSTEM);
	if (error)
		goto out;
	
	t2p->t2_maxpcount = 2;
	t2p->t2_maxdcount = vcp->vc_txmax;	/* Could we get away with a bigger value here? */
	
	error = smb_t2_request(t2p);
	if (error)
		goto out;

	mdp = &t2p->t2_rdata;
	*osize = (mbuf_pkthdr_len(mdp->md_top) > *osize) ? *osize : mbuf_pkthdr_len(mdp->md_top);
	/* How to we want to copy the data in? USER, SYSTEM, UIO? */
	error = md_get_mem(mdp, odata, *osize, MB_MSYSTEM);
out:
	smb_t2_done(t2p);
	return error;
}
#endif // USE_SIDEBAND_CHANNEL_RPC

