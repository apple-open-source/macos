/*
 * Copyright (c) 2000-2001 Boris Popov
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
#include <sys/msfscc.h>

#include <netsmb/smb.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_conn.h>

#include <fs/smbfs/smbfs.h>
#include <fs/smbfs/smbfs_node.h>
#include <fs/smbfs/smbfs_subr.h>
#include <fs/smbfs/smbfs_lockf.h>
#include <netsmb/smb_converter.h>
#include <libkern/crypto/md5.h>
#include <libkern/OSTypes.h>

/*
 * Lack of inode numbers leads us to the problem of generating them.
 * Partially this problem can be solved by having a dir/file cache
 * with inode numbers generated from the incremented by one counter.
 * However this way will require too much kernel memory, gives all
 * sorts of locking and consistency problems, not to mentinon counter overflows.
 * So, I'm decided to use a hash function to generate pseudo random (and unique)
 * inode numbers.
 */
static u_int64_t
smbfs_getino(struct smbnode *dnp, const char *name, size_t nmlen)
{
	u_int64_t ino;

	ino = dnp->n_ino + smbfs_hash((u_char *)name, nmlen);
	if (ino <= 2)
		ino += 3;
	return ino;
}

int
smbfs_smb_lock(struct smbnode *np, int op, u_int16_t fid, u_int32_t pid,
	off_t start, u_int64_t len, vfs_context_t context, u_int32_t timo)
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
	error = smb_rq_init(rqp, SSTOCP(ssp), SMB_COM_LOCKING_ANDX, context);
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
	mb_put_uint32le(mbp, timo);	/* 0 nowait, -1 infinite wait */
	mb_put_uint16le(mbp, op == SMB_LOCK_RELEASE ? 1 : 0);
	mb_put_uint16le(mbp, op == SMB_LOCK_RELEASE ? 0 : 1);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint16le(mbp, pid);
	if (ltype & SMB_LOCKING_ANDX_LARGE_FILES) {
		mb_put_uint16le(mbp, 0); /* pad */
		mb_put_uint32le(mbp, (u_int32_t)(start >> 32)); /* OffsetHigh */
		mb_put_uint32le(mbp, (u_int32_t)(start & 0xffffffff)); /* OffsetLow */
		mb_put_uint32le(mbp, (u_int32_t)(len >> 32)); /* LengthHigh */
		mb_put_uint32le(mbp, (u_int32_t)(len & 0xffffffff)); /* LengthLow */
	}
	else {
		mb_put_uint32le(mbp, (u_int32_t)(start & 0xffffffff));
		mb_put_uint32le(mbp, (u_int32_t)(len & 0xffffffff));
	}
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	/*
	 * This may seem strange, but both Windows and Samba do the following:
	 * 
	 * Lock a region, try to lock it again you get NT_STATUS_LOCK_NOT_GRANTED,
	 * try to lock it a third time you get NT_STATUS_FILE_LOCK_CONFLICT. Seems
	 * the first lock error is always NT_STATUS_LOCK_NOT_GRANTED and the second
	 * time you get a NT_STATUS_FILE_LOCK_CONFLICT.
	 *
	 * For IO they always return NT_STATUS_FILE_LOCK_CONFLICT, which we convert 
	 * to EIO, becasue there are multiple IO routines and only one lock routine. 
	 * So we want this to be EACCES in the lock cases. So we need to convert 
	 * that error here.
	 *
	 */
	if ((error == EIO) && (rqp->sr_rpflags2 & SMB_FLAGS2_ERR_STATUS)) {
		u_int32_t nterr = rqp->sr_error & ~(0xe0000000);
		if (nterr == NT_STATUS_FILE_LOCK_CONFLICT) 
			error = EACCES;
	}
	smb_rq_done(rqp);
	return error;
}

static int
smbfs_smb_qpathinfo(struct smbnode *np, struct smbfattr *fap, 
					vfs_context_t context, short infolevel,
					const char **namep, size_t *nmlenp)
{
	struct smb_share *ssp = np->n_mount->sm_share;
	struct smb_vc *vcp = SSTOVC(ssp);
	struct smb_t2rq *t2p;
	int error, svtz;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int64_t llint;
	u_int32_t size, dattr, eaSize;
	const char *name = (namep ? *namep : NULL);
	size_t nmlen = (nmlenp ? *nmlenp : 0);
	char *ntwrkname = NULL;
	char *filename;
	u_int8_t sep = '\\';

	error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_QUERY_PATH_INFORMATION, context, &t2p);
	if (error)
		return error;
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
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
		return error;
	}
	mdp = &t2p->t2_rdata;
	/*
	 * At this point md_cur and md_top have the same value. Now all the md_get 
	 * routines will will check for null, but just to be safe we check here.
	 */
	if (mdp->md_cur == NULL) {
		SMBWARNING("Parsing error reading the message\n");
		smb_t2_done(t2p);
		return EBADRPC;
	}
	svtz = vcp->vc_sopt.sv_tz;
	switch (infolevel) {
	case SMB_QFILEINFO_BASIC_INFO:
		md_get_uint64le(mdp, &llint);	/* creation time */
		if (llint) {
			smb_time_NT2local(llint, &fap->fa_crtime);
		}
		md_get_uint64le(mdp, &llint);
		if (llint) {
			smb_time_NT2local(llint, &fap->fa_atime);
		}
		md_get_uint64le(mdp, &llint);
		if (llint) {
			smb_time_NT2local(llint, &fap->fa_mtime);
		}
		md_get_uint64le(mdp, &llint);
		if (llint) {
			smb_time_NT2local(llint, &fap->fa_chtime);
		}
		error = md_get_uint32le(mdp, &dattr);
		fap->fa_attr = dattr;
		fap->fa_vtype = (fap->fa_attr & SMB_FA_DIR) ? VDIR : VREG;
		break;
	case SMB_QFILEINFO_ALL_INFO:								
		md_get_uint64le(mdp, &llint);	/* creation time */
		if (llint) {
			smb_time_NT2local(llint, &fap->fa_crtime);
		}
		md_get_uint64le(mdp, &llint);
		if (llint) {			/* access time */
			smb_time_NT2local(llint, &fap->fa_atime);
		}
		md_get_uint64le(mdp, &llint);
		if (llint) {			/* write time */
			smb_time_NT2local(llint, &fap->fa_mtime);
		}
		md_get_uint64le(mdp, &llint);
		if (llint) {			/* change time */
			smb_time_NT2local(llint, &fap->fa_chtime);
		}
		/* 
		 * SNIA CIFS Technical Reference is wrong, this should be 
		 * a ULONG 
		 */ 
		md_get_uint32le(mdp, &dattr);	/* Attributes */
		fap->fa_attr = dattr;
		/*
		 * Because of the Steve/Conrad Symlinks we can never be completely
		 * sure that we have the corret vnode type if its a file. For 
		 * directories we always know the correct information.
		 */
		if (fap->fa_attr & SMB_EFA_DIRECTORY) {
			fap->fa_valid_mask |= FA_VTYPE_VALID;
		}			
		fap->fa_vtype = (fap->fa_attr & SMB_EFA_DIRECTORY) ? VDIR : VREG;

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
		error = md_get_uint8(mdp, NULL);	/* Directory or File */
		if (error)
			goto bad;			
		fap->fa_ino = np->n_ino;
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
		/*
		 * Confirmed from MS:
		 * When the attribute has the Reparse Point bit set then the EASize
		 * contains the reparse tag info. This behavior is consistent for 
		 * Full, Both, FullId, or BothId query dir calls.  It will pack the 
		 * reparse tag into the EaSize value if ATTRIBUTE_REPARSE_POINT is set.  
		 * I verified with local MS Engineers, and they also checking to make 
		 * sure the behavior is covered in MS-FSA. 
		 *
		 * EAs and reparse points cannot both be in a file at the same
		 * time. We return different information for each case.
		 *
		 * NOTE: This is not true for this call (SMB_QFILEINFO_ALL_INFO), they
		 * return the reparse bit but the eaSize size is always zero?
		 */
		md_get_uint32le(mdp, &eaSize);	/* extended attributes size */
		if (fap->fa_attr & SMB_EFA_REPARSE_POINT) {
			SMBDEBUG("Reparse point but tag equals %-4x, not supported by this call\n", eaSize);
		}
		/* We don't care about the name, so we can skip getting it */
		if (namep == NULL)
			break;	/* We are done */
			
		error = md_get_uint32le(mdp, &size);	/* Path name lengh */
		if (error)
			goto bad;			
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
				filename++;
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
			SMBERROR("Filename  %s nmlen = %ld\n", ntwrkname, nmlen);
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
			smb_time_NT2local(llint, &fap->fa_chtime);
		
		md_get_uint64le(mdp, &llint);	/* access time */
		if (llint)
			smb_time_NT2local(llint, &fap->fa_atime);

		md_get_uint64le(mdp, &llint);	/* write time */
		if (llint)
			smb_time_NT2local(llint, &fap->fa_mtime);

		md_get_uint64le(mdp, &llint);	/* Numeric user id for the owner */
		fap->fa_uid = llint;
		
		md_get_uint64le(mdp, &llint);	/* Numeric group id for the owner */
		fap->fa_gid = llint;

		md_get_uint32le(mdp, &dattr);	/* Enumeration specifying the file type, st_mode */
		fap->fa_valid_mask |= FA_VTYPE_VALID;
		/* Make sure the dos attributes are correct */ 
		if (dattr & EXT_UNIX_DIR) {
			fap->fa_attr |= SMB_EFA_DIRECTORY;
			fap->fa_vtype = VDIR;
		} else if (dattr & EXT_UNIX_SYMLINK) {
			fap->fa_vtype = VLNK;
		} else {
			fap->fa_vtype = VREG;
		}

		md_get_uint64le(mdp, &llint);	/* Major device number if type is device */
		md_get_uint64le(mdp, &llint);	/* Minor device number if type is device */
		md_get_uint64le(mdp, &llint);	/* This is a server-assigned unique id */
		md_get_uint64le(mdp, &llint);	/* Standard UNIX permissions */
		fap->fa_permissions = llint;
		fap->fa_valid_mask |= FA_UNIX_MODES_VALID;
		md_get_uint64le(mdp, &llint);	/* Number of hard link */
		fap->fa_nlinks = llint;

		md_get_uint64le(mdp, &llint);	/* creation time */
		if (llint)
			smb_time_NT2local(llint, &fap->fa_crtime);

		md_get_uint32le(mdp, &dattr);	/* File flags enumeration */
		error = md_get_uint32le(mdp, &fap->fa_flags_mask);	/* Mask of valid flags */
		if (error)
			break;
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
	size_t len = sizeof(SMB_DATASTREAM) - 1;

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
	SMBERROR("file \"%s\" has bad stream \"%s\"\n", np->n_name, ctx->f_name);
	return (0); /* skip it */
}

/*
 * smbfs_smb_markfordelete
 *
 * We have an open file that they want to delete. This call will tell the 
 * server to delete the file when the last close happens. Currenly we know that 
 * XP, Windows 2000 and Windows 2003 support this call. SAMBA does support the
 * call, but currently has a bug that prevents it from working.
 *
 */
static int
smbfs_smb_markfordelete(struct smb_share *ssp, struct smbnode *np,
						vfs_context_t context, u_int16_t *infid)
{
	struct smb_t2rq *t2p;
	struct smb_vc *vcp = SSTOVC(ssp);
	struct mbchain *mbp;
	int error, cerror;
	u_int16_t fid = (infid) ? *infid : 0;
	
	if (!(vcp->vc_sopt.sv_caps & SMB_CAP_NT_SMBS))
		return (ENOTSUP);
	
	/* Must pass in either a fid pointer or a node pointer */
	if (!np && !infid) {
		return (EINVAL);
	}	
	
	error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_SET_FILE_INFORMATION, context, &t2p);
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
		error = smbfs_smb_tmpopen(np, STD_RIGHT_DELETE_ACCESS, context, &fid);
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
		cerror = smbfs_smb_tmpclose(np, fid, context);
		if (cerror) {
			SMBWARNING("error %d closing fid %d\n", cerror, fid);
		}
	}
	smb_t2_done(t2p);
	return error;
}

/*
 * Create the data required for a faked up symbolic link. This is Conrad and Steve
 * French method for storing and reading symlinks on Window Servers.
 */
static void * 
smbfs_create_windows_symlink_data(const char *target, size_t targetlen, 
								  uint32_t *rtlen)
{
	MD5_CTX md5;
	uint32_t state[4];
	uint32_t datalen, filelen;
	char *wbuf, *wp;
	int	 maxwplen;
	uint32_t targlen = (uint32_t)targetlen;
	
	datalen = SMB_SYMHDRLEN + targlen;
	filelen = SMB_SYMLEN;
	maxwplen = filelen;
	
	MALLOC(wbuf, void *, filelen, M_TEMP, M_WAITOK);
	
	wp = wbuf;
	bcopy(smb_symmagic, wp, SMB_SYMMAGICLEN);
	wp += SMB_SYMMAGICLEN;
	maxwplen -= SMB_SYMMAGICLEN;
	(void)snprintf(wp, maxwplen, "%04d\n", targlen);
	wp += SMB_SYMLENLEN;
	maxwplen -= SMB_SYMLENLEN;
	MD5Init(&md5);
	MD5Update(&md5, (unsigned char *)target, targlen);
	MD5Final((u_char *)state, &md5);
	(void)snprintf(wp, maxwplen, "%08x%08x%08x%08x\n", htobel(state[0]),
				   htobel(state[1]), htobel(state[2]), htobel(state[3]));
	wp += SMB_SYMMD5LEN;
	bcopy(target, wp, targlen);
	wp += targlen;
	if (datalen < filelen) {
		*wp++ = '\n';
		datalen++;
		if (datalen < filelen)
			memset((caddr_t)wp, ' ', filelen - datalen);
	}
	*rtlen = filelen;
	return wbuf;
}

/*
 * Create a UNIX style symlink using the UNIX extension. This uses the smb trans2
 * set path info call with a unix link info level. The server will create the symlink
 * using the path, the data portion of the trans2 message will contain the target.
 * The target will be a UNIX style target including using forward slashes as the
 * delemiter.
 */
static int
smb_setfile_unix_symlink(struct smb_share *share, struct smbnode *dnp, 
						 const char *name, size_t nmlen, char *target, 
						 size_t targetlen, vfs_context_t context)
{
	struct smb_t2rq *t2p = NULL;
	struct mbchain *mbp;
	int error;
	char *ntwrkpath = NULL;
	size_t ntwrkpathlen = targetlen * 2; /* UTF8 to UTF16 can be twice as big */
	
	MALLOC(ntwrkpath, char *, ntwrkpathlen, M_SMBFSDATA, M_WAITOK | M_ZERO);
	/* smb_convert_path_to_network sets the precomosed flag */
	error = smb_convert_path_to_network(target, targetlen, ntwrkpath, 
										&ntwrkpathlen, '/', NO_SFM_CONVERSIONS, 
										SMB_UNICODE_STRINGS(SSTOVC(share)));
	if (! error) {
		error = smb_t2_alloc(SSTOCP(share), SMB_TRANS2_SET_PATH_INFORMATION, context, &t2p);
	}
	if (error) {
		goto done;
	}
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	mb_put_uint16le(mbp, SMB_SFILEINFO_UNIX_LINK);
	mb_put_uint32le(mbp, 0);		/* MBZ */
	error = smbfs_fullpath(mbp, SSTOVC(share), dnp, name, &nmlen, UTF_SFM_CONVERSIONS, '\\');
	if (error) {
		goto done;
	}
	mbp = &t2p->t2_tdata;
	mb_init(mbp);
	mb_put_mem(mbp, (caddr_t)(ntwrkpath), ntwrkpathlen, MB_MSYSTEM);
	t2p->t2_maxpcount = 2;
	t2p->t2_maxdcount = SSTOVC(share)->vc_txmax;
	error = smb_t2_request(t2p);
	
done:
	SMB_FREE(ntwrkpath, M_SMBFSDATA);
	if (t2p) {
		smb_t2_done(t2p);
	}
	return error;
	
}

static int
smbfs_smb_fsctl(struct smb_share *share,  uint32_t fsctl, uint16_t fid, 
				uint32_t datacnt, struct mbchain *mbp, struct mdchain *mdp,
				Boolean * moreDataRequired, vfs_context_t context)
{
	struct smb_ntrq *ntp = NULL;
	
	int error = smb_nt_alloc(SSTOCP(share), NT_TRANSACT_IOCTL, context, &ntp);
	if (error) {
		goto done;
	}
	/*
	 * TotalParameterCount (4 bytes): 
	 * ULONG This field MUST be set to 0x0000. 
	 *
	 * MaxParameterCount (4 bytes): 
	 * ULONG This field MUST be set to 0x0000. 
	 *
	 * ParameterCount (4 bytes) : 
	 * ULONG This field MUST be set to 0x0000.
	 */
	/*
	 * MaxDataCount (4 bytes): 
	 * ULONG The max data that can be returned. 
	 */
	ntp->nt_maxdcount = datacnt;
	
	/* The NT_TRANSACT_IOCTL setup structure */
	mb_init(&ntp->nt_tsetup);
	
	/* 
	 * FunctionCode (4 bytes): 
	 * ULONG Windows NT device or file system control code.
	 */
	mb_put_uint32le(&ntp->nt_tsetup, fsctl);
	
	/* FID (2 bytes): 
	 * USHORT MUST contain a valid FID obtained from a previously successful 
	 * SMB open command. The FID MUST be for either an I/O device or for a 
	 * file system control device. The type of FID being supplied is specified 
	 * by IsFctl.
	 */
	mb_put_uint16le(&ntp->nt_tsetup, fid);
	
	/*
	 * IsFctl (1 byte): 
	 * BOOLEAN This field is TRUE if the command is a file system control 
	 * command and the FID is a file system control device. Otherwise, the 
	 * command is a device control command and FID is an I/O device.
	 *
	 * Currently always set to true 
	 */
	mb_put_uint8(&ntp->nt_tsetup, 1);
	
	/* 
	 * IsFlags (1 byte): 
	 * BOOLEAN If bit 0 is set, the command is to be applied to a share root 
	 * handle. The share MUST be a Distributed File System (DFS) type.
	 *
	 * Currently always set to false 
	 */
	mb_put_uint8(&ntp->nt_tsetup, 0);
	
	/*
	 * NT_Trans_Parameters (variable): (0 bytes): 
	 * No NT_Trans parameters are sent in this request.
	 */
	/*
	 * NT_Trans_Data (variable):
	 * The raw bytes that are passed to the fsctl or ioctl function as the 
	 * input buffer.
	 */
	if (mbp) {
		/* Use the mbchain passed in */
		ntp->nt_tdata = *mbp;
		memset(mbp, 0, sizeof(*mbp));
	}
	
	error = smb_nt_request(ntp);
	if (error) {
		SMBWARNING("smb_nt_request error = %d\n", error);
		/* The data buffer wasn't big enough. Caller will have to retry. */
		if (moreDataRequired && (ntp->nt_flags & SMBT2_MOREDATA)) {
			*moreDataRequired = TRUE;
		}
		goto done;
	}
	if (ntp->nt_rdata.md_top && mdp) {		
		SMBSYMDEBUG("Repase data size = %d\n", (int)datalen);
		/* Store the mbuf info into their mdchain */
		md_initm(mdp, ntp->nt_rdata.md_top);
		/* Clear it out so we don't free the mbuf */
		memset(&ntp->nt_rdata, 0, sizeof(ntp->nt_rdata));
	}
	
done:
	if (ntp) {
		smb_nt_done(ntp);
	}
	
	return error;
	
}

void 
smbfs_update_symlink_cache(struct smbnode *np, char *target, size_t targetlen)
{
	struct timespec ts;
	
	SMB_FREE(np->n_symlink_target, M_TEMP);
	np->n_symlink_target_len = 0;
	np->n_symlink_target = smb_strdup(target, targetlen);
	if (np->n_symlink_target) {
		np->n_symlink_target_len = targetlen;
		nanouptime(&ts);
		np->n_symlink_cache_timer = ts.tv_sec;
	}
}

/*
 * Create a symbolic link since the server supports the UNIX extensions. This 
 * allows us to access and share symbolic links with AFP and NFS.
 *
 */
int 
smbfs_smb_create_unix_symlink(struct smb_share *share, struct smbnode *dnp, 
							  const char *in_name, size_t in_nmlen, char *target, 
							  size_t targetlen, struct smbfattr *fap,
							  vfs_context_t context)
{
	const char *name = in_name;
	size_t nmlen = in_nmlen;
	int error;
	
	memset(fap, 0, sizeof(*fap));
	error = smb_setfile_unix_symlink(share, dnp, name, nmlen, target, 
									 targetlen, context);
	if (error) {
		goto done;
	}
	/* 
	 * The smb_setfile_unix_symlink call does not return any meta data
	 * info that includes the inode number of the item. We could just dummy up 
	 * these values, but that wouldn't be correct and besides we would 
	 * just need invalidate the cache, which would cause a lookup anyways.
	 * 
	 * Seems if wide links are turned off Snow Leopard servers will return
	 * access denied on any lookup of the symbolic link. So for now dummy up
	 * the file's attribute if an error is returned on the lookup. If this gets
	 * fixed in the future then we could remove this code, but we need to cleanup
	 * on failure.
	 *
	 * We should cleanup on failure, but we don't do that in any of the 
	 * other failure case. See <rdar://problem/8498673>
	 */
	error = smbfs_smb_lookup(dnp, &name, &nmlen, fap, context);
	if (error) {
		struct smbmount *smp = dnp->n_mount;
		
		fap->fa_attr = SMB_EFA_NORMAL;
		fap->fa_size = targetlen;
		fap->fa_data_alloc = roundup(fap->fa_size, smp->sm_statfsbuf.f_bsize);
		nanotime(&fap->fa_crtime);	/* Need current date/time, so use nanotime */
		fap->fa_atime = fap->fa_crtime;
		fap->fa_chtime = fap->fa_crtime;
		fap->fa_mtime = fap->fa_crtime;
		fap->fa_ino = smbfs_getino(dnp, in_name, in_nmlen);
		nanouptime(&fap->fa_reqtime);
		fap->fa_valid_mask |= FA_VTYPE_VALID;
		fap->fa_vtype = VLNK;
		fap->fa_nlinks = 1;
		fap->fa_flags_mask = EXT_REQUIRED_BY_MAC;
		fap->fa_unix = TRUE;	
		error = 0;
	}
done:
	/* If lookup returned a new name free it we never need that name */
	if (name != in_name) {
		SMB_FREE(name, M_SMBNODENAME);
	}
	if (error) {
		SMBWARNING("Creating symlink for %s failed! error = %d\n", name, error);
	}
	return error;
}

/*
 * This server doesn't support UNIX or reparse point style symbolic links, so
 * create a faked up symbolic link, using the Conrad and Steve French method
 * for storing and reading symlinks on Window Servers. 
 *
 * NOTE: We should remove creating these in the future, but first we need to see
 * if we can get reparse point style symbolic links working.
 *
 */
int 
smbfs_smb_create_windows_symlink(struct smb_share *share, struct smbnode *dnp, 
								 const char *name, size_t nmlen, char *target, 
								 size_t targetlen, struct smbfattr *fap,
								 vfs_context_t context)
{
	uint32_t wlen = 0;
	uio_t uio;
	char *wdata  = NULL;
	int error;
	uint16_t fid = 0;
	
	error = smbfs_smb_create(dnp, name, nmlen, SMB2_FILE_WRITE_DATA, 
							 context, &fid, NTCREATEX_DISP_CREATE, 0, fap);
	if (error) {
		goto done;		
	}
	wdata = smbfs_create_windows_symlink_data(target, targetlen, &wlen);
	if (!wdata) {
		error = ENOMEM;
		goto done;		
	}
	uio = uio_create(1, 0, UIO_SYSSPACE, UIO_WRITE);
	uio_addiov(uio, CAST_USER_ADDR_T(wdata), wlen);
	error = smb_write(share, fid, uio, context, SMBWRTTIMO);
	uio_free(uio);
	if (!error)	/* We just changed the size of the file */
		fap->fa_size = wlen;
done:
	FREE(wdata, M_TEMP);
	if (fid) {
		(void)smbfs_smb_close(share, fid, context);
	}
	if (error) {
		SMBWARNING("Creating symlink for %s failed! error = %d\n", name, error);
	}
	return error;
}

/*
 * This server support reparse point style symbolic links, This allows us to 
 * access and share symbolic links with AFP and NFS.
 *
 */
int 
smbfs_smb_create_reparse_symlink(struct smb_share *share, struct smbnode *dnp, 
								 const char *name, size_t nmlen, char *target, 
								 size_t targetlen, struct smbfattr *fap,
								 vfs_context_t context)
{
	struct smbmount *smp = dnp->n_mount;
	int error;
	uint16_t fid = 0;
	struct mbchain	mbp;
	size_t path_len;
	char *path;
	uint16_t reparseLen;
	uint16_t SubstituteNameOffset, SubstituteNameLength;
	uint16_t PrintNameOffset, PrintNameLength;
	
	error = smbfs_smb_ntcreatex(dnp, SMB2_FILE_WRITE_DATA | SMB2_FILE_WRITE_ATTRIBUTES | SMB2_DELETE, 
								NTCREATEX_SHARE_ACCESS_ALL, context, 
								VLNK, &fid,  name, nmlen, NTCREATEX_DISP_CREATE,  
								0, fap);
	if (error) {
		goto done;		
	}
	path_len = (targetlen * 2) + 2;	/* Start with the max possible size */
	MALLOC(path, char *, path_len, M_TEMP, M_WAITOK | M_ZERO);
	if (path == NULL) {
		error = ENOMEM;
		goto done;		
	}
	/* Convert it to a network style path */
	error = smb_convert_path_to_network(target, targetlen, path,  &path_len, 
										'\\', SMB_UTF_SFM_CONVERSIONS, SMB_UNICODE_STRINGS(SSTOVC(share)));
	if (error) {
		SMB_FREE(path, M_TEMP);
		goto done;		
	}
	SubstituteNameLength = path_len;
	SubstituteNameOffset = 0;
	PrintNameOffset = SubstituteNameLength;
	PrintNameLength = SubstituteNameLength;
	reparseLen = SubstituteNameLength + PrintNameLength + 12;
	mb_init(&mbp);
	mb_put_uint32le(&mbp, IO_REPARSE_TAG_SYMLINK);
	mb_put_uint16le(&mbp, reparseLen);
	mb_put_uint16le(&mbp, 0);	/* Reserved */
	mb_put_uint16le(&mbp, SubstituteNameOffset);
	mb_put_uint16le(&mbp, SubstituteNameLength);
	mb_put_uint16le(&mbp, PrintNameOffset);
	mb_put_uint16le(&mbp, PrintNameLength);
	/*
	 * Flags field can be either SYMLINK_FLAG_ABSOLUTE or SYMLINK_FLAG_RELATIVE.
	 * If the target starts with a slash assume its absolute otherwise it must
	 * me realative.
	 */
	if (*target == '/') {
		mb_put_uint32le(&mbp, SYMLINK_FLAG_ABSOLUTE);
	} else {
		mb_put_uint32le(&mbp, SYMLINK_FLAG_RELATIVE);
	}	
	mb_put_mem(&mbp, path, SubstituteNameLength, MB_MSYSTEM);
	mb_put_mem(&mbp, path, PrintNameLength, MB_MSYSTEM);
	SMB_FREE(path, M_TEMP);
	error = smbfs_smb_fsctl(share,  FSCTL_SET_REPARSE_POINT, fid, 0, &mbp, 
							NULL, NULL, context);
	mb_done(&mbp);
	/* 
	 * Windows systems requires special user access to create a reparse symlinks.
	 * They default to only allow administrator symlink create access. This can
	 * be changed on the server, but we are going to run into this issue. So if
	 * we get an access error on the fsctl then we assume this user doesn't have
	 * create symlink rights and we need to fallback to the old Conrad/Steve
	 * symlinks. Since the create work we know the user has access to the file
	 * system, they just don't have create symlink rights. We never fallback if 
	 * the server is running darwin.
	 */
	if ((error == EACCES) && !(SSTOVC(share)->vc_flags & SMBV_DARWIN)) {
		smp->sm_flags &= ~MNT_SUPPORTS_REPARSE_SYMLINKS;
	} 
	
	if (error) {
		/* Failed to create the symlink, remove the file on close */
		(void)smbfs_smb_markfordelete(share, NULL, context, &fid);
	} else {
		/* Reset any other fap information */
		fap->fa_size = targetlen;
		/* The FSCTL_SET_REPARSE_POINT succeeded, so mark it as a reparse point */
		fap->fa_attr |= SMB_EFA_REPARSE_POINT;
		fap->fa_valid_mask |= FA_REPARSE_TAG_VALID;
		fap->fa_reparse_tag = IO_REPARSE_TAG_SYMLINK;
		fap->fa_valid_mask |= FA_VTYPE_VALID;
		fap->fa_vtype = VLNK;
	}
	
done:
	if (fid) {
		(void)smbfs_smb_close(share, fid, context);
	}
	/* 
	 * This user doesn't have access to create a reparse symlink, create the 
	 * old style symlink. 
	 */
	if ((error == EACCES) && !(smp->sm_flags & MNT_SUPPORTS_REPARSE_SYMLINKS)) {
		error = smbfs_smb_create_windows_symlink(share, dnp, name, nmlen, target, 
												 targetlen, fap, context);
	}
	
	if (error) {
		SMBWARNING("Creating symlink for %s failed! error = %d\n", name, error);
	}
	return error;
}

/*
 * The symbolic link is stored in a reparse point, support by some windows servers
 * and Darwin servers.
 *
 */
int 
smbfs_smb_reparse_read_symlink(struct smb_share *share, struct smbnode *np, 
							   struct uio *uiop, vfs_context_t context)
{
	int error;
	Boolean moreDataRequired = FALSE;
	uint32_t rdatacnt = SSTOVC(share)->vc_txmax;
	struct mdchain mdp;
	uint32_t reparseTag = 0;
	uint16_t reparseLen = 0;
	uint16_t SubstituteNameOffset = 0;
	uint16_t SubstituteNameLength = 0;
	uint16_t PrintNameOffset = 0;
	uint16_t PrintNameLength = 0;
	uint32_t Flags = 0;
	uint16_t fid = 0;
	char *ntwrkname = NULL;
	char *target = NULL;
	size_t targetlen;
	
	memset(&mdp, 0, sizeof(mdp));
	error = smbfs_smb_tmpopen(np, SMB2_FILE_READ_DATA | SMB2_FILE_READ_ATTRIBUTES, context, &fid);
	if (error) {
		goto done;
	}
	
	error = smbfs_smb_fsctl(share,  FSCTL_GET_REPARSE_POINT, fid, rdatacnt, NULL, 
							&mdp, &moreDataRequired, context);
	if (!error && !mdp.md_top) {
		error = ENOENT;
	}
	if (error) {
		goto done;
	}
	
	md_get_uint32le(&mdp, &reparseTag);
	/* Should have checked to make sure the node reparse tag matches */
	if (reparseTag != IO_REPARSE_TAG_SYMLINK) {
		md_done(&mdp);
		goto done;
	}
	md_get_uint16le(&mdp, &reparseLen);
	md_get_uint16le(&mdp, NULL);	/* Reserved */
	md_get_uint16le(&mdp, &SubstituteNameOffset);
	md_get_uint16le(&mdp, &SubstituteNameLength);
	md_get_uint16le(&mdp, &PrintNameOffset);
	md_get_uint16le(&mdp, &PrintNameLength);
	/*
	 * Flags field can be either SYMLINK_FLAG_ABSOLUTE or SYMLINK_FLAG_RELATIVE,
	 * in either case we don't care and just ignore it for now.
	 */
	md_get_uint32le(&mdp, &Flags);
	SMBSYMDEBUG("reparseLen = %d SubstituteNameOffset = %d SubstituteNameLength = %d PrintNameOffset = %d PrintNameLength = %d Flags = %d\n",
				reparseLen, 
				SubstituteNameOffset, SubstituteNameLength, 
				PrintNameOffset, PrintNameLength, Flags);
	/*
	 * Mount Point Reparse Data Buffer
	 * A mount point has a substitute name and a print name associated with it. 
	 * The substitute name is a pathname (section 2.1.5) identifying the target 
	 * of the mount point. The print name SHOULD be an informative pathname 
	 * (section 2.1.5), suitable for display to a user, that also identifies the 
	 * target of the mount point. Neither of these pathnames can contain dot 
	 * directory names.
	 * 
	 * So the above implies that we should always use the substitute name, but
	 * my guess is they are always the same in symbolic link case.
	 */
	/* Never allocate more than our transcation size buffer */
	if (SubstituteNameLength > SSTOVC(share)->vc_txmax) {
		error = ENOMEM;
		SMBSYMDEBUG("%s SubstituteNameLength too large %d \n", np->n_name, SubstituteNameLength);
		md_done(&mdp);
		goto done;
	}
	
	if (SubstituteNameOffset) {
		md_get_mem(&mdp, NULL, SubstituteNameOffset, MB_MSYSTEM);
	}
	
	MALLOC(ntwrkname, char *, (size_t)SubstituteNameLength, M_TEMP, M_WAITOK | M_ZERO);
	if (ntwrkname == NULL) {
		error = ENOMEM;
	} else {
		error = md_get_mem(&mdp, (void *)ntwrkname, (size_t)SubstituteNameLength, MB_MSYSTEM);
	}
	md_done(&mdp);
	if (error) {
		goto done;
	}
	targetlen = SubstituteNameLength * 9 + 1;
	MALLOC(target, char *, targetlen, M_TEMP, M_WAITOK | M_ZERO);
	if (target == NULL) {
		error = ENOMEM;
	} else {
		error = smb_convert_network_to_path(ntwrkname, SubstituteNameLength, target, 
											&targetlen, '\\', UTF_SFM_CONVERSIONS, 
											SMB_UNICODE_STRINGS(SSTOVC(share)));
	}
	if (!error) {
		SMBSYMDEBUG("%s --> %s\n", np->n_name, target);
		smbfs_update_symlink_cache(np, target, targetlen);
		error = uiomove(target, (int)targetlen, uiop);
	}
	
done:
	SMB_FREE(ntwrkname, M_TEMP);
	SMB_FREE(target, M_TEMP);
	if (fid) {
		(void)smbfs_smb_tmpclose(np, fid, context);
	}
	if (error) {
		SMBWARNING("%s failed %d\n", np->n_name, error);
	}
	return error;
	
}

/*
 * Supports reading a faked up symbolic link. This is Conrad and Steve
 * French method for storing and reading symlinks on Window Servers.
 *
 */
int 
smbfs_smb_windows_read_symlink(struct smb_share *share, struct smbnode *np, 
							   struct uio *uiop, vfs_context_t context)
{
	unsigned char *wbuf, *cp;
	unsigned len, flen;
	uio_t uio;
	int error;
	uint16_t fid = 0;
	char *target = NULL;
	
	flen = SMB_SYMLEN;
	MALLOC(wbuf, void *, flen, M_TEMP, M_WAITOK);
	
	error = smbfs_smb_tmpopen(np, SMB2_FILE_READ_DATA, context, &fid);
	if (error)
		goto out;
	uio = uio_create(1, 0, UIO_SYSSPACE, UIO_READ);
	uio_addiov(uio, CAST_USER_ADDR_T(wbuf), flen);
	error = smb_read(share, fid, uio, context);
	uio_free(uio);
	(void)smbfs_smb_tmpclose(np, fid, context);
	if (error)
		goto out;
	for (len = 0, cp = wbuf + SMB_SYMMAGICLEN;
	     cp < wbuf + SMB_SYMMAGICLEN + SMB_SYMLENLEN-1; cp++) {
		if (*cp < '0' || *cp > '9') {
			SMBWARNING("symlink length nonnumeric: %c (0x%x)\n", *cp, *cp);
			return (EINVAL);
		}
		len *= 10;
		len += *cp - '0';
	}
	if (len != np->n_size) {
		SMBWARNING("symlink length payload changed from %u to %u\n", 
				   (unsigned)np->n_size, len);
		np->n_size = len;
	}
	target = (char *)(wbuf + SMB_SYMHDRLEN);
	SMBSYMDEBUG("%s --> %s\n", np->n_name, target);
	smbfs_update_symlink_cache(np, target, len);
	error = uiomove(target, (int)len, uiop);
out:
	FREE(wbuf, M_TEMP);
	if (error) {
		SMBWARNING("%s failed %d\n", np->n_name, error);
	}
	return (error);
}

/*
 * Support for reading a symbolic link that resides on a UNIX server. This allows
 * us to access and share symbolic links with AFP and NFS.
 *
 *
 */
int 
smbfs_smb_unix_read_symlink(struct smb_share *share, struct smbnode *np, 
							struct uio *uiop, vfs_context_t context)
{
	struct smb_t2rq *t2p;
	int error;
	struct mbchain *mbp;
	struct mdchain *mdp;
	char *ntwrkname = NULL;
	char *target;
	size_t targetlen;
	size_t nmlen;
	
	error = smb_t2_alloc(SSTOCP(share), SMB_TRANS2_QUERY_PATH_INFORMATION, context, &t2p);
	if (error)
		return error;
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	mb_put_uint16le(mbp, SMB_QFILEINFO_UNIX_LINK);
	mb_put_uint32le(mbp, 0);
	error = smbfs_fullpath(mbp, SSTOVC(share), np, NULL, NULL, UTF_SFM_CONVERSIONS, '\\');
	if (error) 
	    goto out;
	t2p->t2_maxpcount = 2;
	t2p->t2_maxdcount = SSTOVC(share)->vc_txmax;
	error = smb_t2_request(t2p);
	if (error) 
	    goto out;
	mdp = &t2p->t2_rdata;
	/*
	 * At this point md_cur and md_top have the same value. Now all the md_get 
	 * routines will will check for null, but just to be safe we check here
	 */
	if (mdp->md_top == NULL) {
		error = EIO;
	    goto out;
	}
	/* Get the size of the data that contains the symbolic link */
	nmlen = mbuf_pkthdr_len(mdp->md_top);
	SMBSYMDEBUG("network len of the symbolic link = %ld\n", nmlen);
	MALLOC(ntwrkname, char *, nmlen, M_TEMP, M_WAITOK);
	if (ntwrkname == NULL) {
		error = ENOMEM;
	} else {
		/* Read in the data that contains the symbolic link */
		error = md_get_mem(mdp, (void *)ntwrkname, nmlen, MB_MSYSTEM);
	}
	if (error) 
	    goto out;
#ifdef DEBUG_SYMBOLIC_LINKS
	smb_hexdump(__FUNCTION__, "Symlink: ", (u_char *)ntwrkname, nmlen);
#endif // DEBUG_SYMBOLIC_LINKS
	/*
	 * The Symbolic link data is a UNIX style symbolic link, except it is in UTF16
	 * format. The unix slash is the delemter used. The path should end with two 
	 * null bytes as a terminator. The smb_convert_network_to_path  does not expect 
	 * nmlen to include those bytes, so we can just back those bytes out. Nothiing 
	 * in the reference states that the null bytes are require, but this would be
	 * the normal way to handle this type of string in SMB. 
	 */
	nmlen -= 2;
	targetlen = nmlen * 9 + 1;
	MALLOC(target, char *, targetlen, M_TEMP, M_WAITOK | M_ZERO);
	error = smb_convert_network_to_path(ntwrkname, nmlen, target, 
										&targetlen, '/', UTF_SFM_CONVERSIONS, 
										SMB_UNICODE_STRINGS(SSTOVC(share)));
	if (!error) {
		SMBSYMDEBUG("%s --> %s\n", np->n_name, target);
		smbfs_update_symlink_cache(np, target, targetlen);
		error = uiomove(target, (int)targetlen, uiop);
	}
	
	SMB_FREE(target, M_TEMP);
	
out:
	SMB_FREE(ntwrkname, M_TEMP);
	smb_t2_done(t2p);
	if (error) {
		SMBWARNING("%s failed %d\n", np->n_name, error);
	}
	return error;
}

/*
 * When calling this routine be very carefull when passing the arguments. Depending on the arguments 
 * different actions will be taken with this routine. 
 *
 * %%% - We should make into three different routines.
 */
int smbfs_smb_qstreaminfo(struct smbnode *np, vfs_context_t context, uio_t uio, size_t *sizep,
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
	
	if ((np->n_fstatus & kNO_SUBSTREAMS) || (np->n_dosattr &  SMB_EFA_REPARSE_POINT)) {
		foundStream = FALSE;
		error = ENOATTR;
		goto done;
	}	

	error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_QUERY_PATH_INFORMATION, context, &t2p);
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
			const char *s;

			/* the "+ 1" skips over the leading colon */
			s = ctx.f_name + 1;
			
			/* Check for special case streams (resource fork and finder info */
			if ((ctx.f_nmlen >= sizeof(SFM_RESOURCEFORK_NAME)) && 
				(!strncasecmp(s, SFM_RESOURCEFORK_NAME, sizeof(SFM_RESOURCEFORK_NAME)))) {
				stype |= kResourceFrk;
				/* We always get the resource fork size and cache it here. */
				lck_mtx_lock(&np->rfrkMetaLock);
				np->rfrk_size = stream_size;
				nanouptime(&ts);
				np->rfrk_cache_timer = ts.tv_sec;
				lck_mtx_unlock(&np->rfrkMetaLock);
				/* 
				 * The Resource fork and Finder info names are special and get translated between stream names and 
				 * extended attribute names. In this case we need to make sure the correct name gets used. So we are
				 * looking for a specfic stream use its stream name otherwise use its extended attribute name.
				 */
				if ((uio == NULL) && strmname && (sizep == NULL)) {
					s = SFM_RESOURCEFORK_NAME;
					ctx.f_nmlen = sizeof(SFM_RESOURCEFORK_NAME);
				}
				else {
					s = XATTR_RESOURCEFORK_NAME;
					ctx.f_nmlen = sizeof(XATTR_RESOURCEFORK_NAME);
				}
									
			} else if ((ctx.f_nmlen >= sizeof(SFM_FINDERINFO_NAME)) && 
					(!strncasecmp(s, SFM_FINDERINFO_NAME, sizeof(SFM_FINDERINFO_NAME)))) {
				/*
				 * They have an AFP_Info stream and it has no size must be a Samba 
				 * server. We treat this the same as if the file has no Finder Info
				 */
				if (stream_size == 0)
					goto skipentry;

				stype |= kFinderInfo;
				/* 
				 * The Resource fork and Finder info names are special and get translated between stream names and 
				 * extended attribute names. In this case we need to make sure the correct name gets used. So we are
				 * looking for a specfic stream use its stream name otherwise use its extended attribute name.
				 */
				if ((uio == NULL) && strmname && (sizep == NULL)) {
					s = SFM_FINDERINFO_NAME;
					ctx.f_nmlen = sizeof(SFM_FINDERINFO_NAME);
				}
				else  {
					s = XATTR_FINDERINFO_NAME;
					ctx.f_nmlen = sizeof(XATTR_FINDERINFO_NAME);
				}
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
				}
				goto skipentry;
			} else if (uio && stream_size)
				uiomove(s, (int)ctx.f_nmlen, uio);
			else if (!foundStream && strmname && strmsize) {
				/* They are looking for a specific stream name and we havn't found it yet. */ 
				nlen = (u_int32_t)strnlen(strmname, ssp->ss_maxfilenamelen+1);
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

out:
	smb_t2_done(t2p);
	
done:
	/* If we searched the entire list and did not find a finder info stream, then reset the cache timer. */
	if ((stype & kFinderInfo) != kFinderInfo) {
		bzero(np->finfo, sizeof(np->finfo));	/* Negative cache the Finder Info */
		nanouptime(&ts);
		np->finfo_cache = ts.tv_sec;
	}
	/* If we searched the entire list and did not find a resource stream, then reset the cache timer. */
	if ((stype & kResourceFrk) != kResourceFrk) {
		lck_mtx_lock(&np->rfrkMetaLock);
		nanouptime(&ts);
		np->rfrk_size = 0;	/* Negative cache the ressource fork */
		np->rfrk_cache_timer = ts.tv_sec;
		lck_mtx_unlock(&np->rfrkMetaLock);
	}	
	if (ctx.f_name)
		free(ctx.f_name, M_SMBFSDATA);
	
	if ((foundStream == FALSE) || (error == ENOENT))	/* We did not find the stream we were looking for */
		error = ENOATTR;
	return (error);
}

static const ntsid_t unix_users_domsid =
{ 1, 1, {0, 0, 0, 0, 0, 22}, {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0} };

static const ntsid_t unix_groups_domsid =
{ 1, 1, {0, 0, 0, 0, 0, 22}, {2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0} };

static void smb_get_sid_list(struct smb_share *ssp, struct smbmount *smp, struct mdchain *mdp, 
						u_int32_t ntwrk_sids_cnt, u_int32_t ntwrk_sid_size)
{
	u_int32_t ii;
	int error;
	void *sidbufptr = NULL;
	char *endsidbufptr;
	char *nextsidbufptr;
	struct ntsid *ntwrk_wire_sid;
	ntsid_t *ntwrk_sids = NULL;
	ntsid_t tmpsid;
	uid_t	ntwrk_sid_uid;

	/* Free any pre existing list */
	if (smp->ntwrk_sids)
		FREE(smp->ntwrk_sids, M_TEMP);
	smp->ntwrk_sids = NULL;
	smp->ntwrk_sids_cnt = 0;
	
	if ((ntwrk_sids_cnt == 0) || (ntwrk_sid_size == 0)) {
		SMBDEBUG("ntwrk_sids_cnt = %d ntwrk_sid_size = %d\n", ntwrk_sids_cnt, ntwrk_sid_size);
		goto done; /* Nothing to do here we are done */
	}
	
	/* Never allocate more than we could have received in this message */
	if (ntwrk_sid_size > SSTOVC(ssp)->vc_txmax) {
		SMBDEBUG("Too big ntwrk_sid_size = %d\n", ntwrk_sid_size);
		goto done;
	}
	
	/* Max number we will support, about 9K */
	if (ntwrk_sids_cnt > KAUTH_ACL_MAX_ENTRIES) 
		ntwrk_sids_cnt = KAUTH_ACL_MAX_ENTRIES;
	
	MALLOC(ntwrk_sids, void *, ntwrk_sids_cnt * sizeof(*ntwrk_sids) , M_TEMP, M_WAITOK | M_ZERO);
	if (ntwrk_sids == NULL) {
		SMBDEBUG("ntwrk_sids malloc failed!\n");
		goto done;		
	}
	MALLOC(sidbufptr, void *, ntwrk_sid_size, M_TEMP, M_WAITOK);
	if (sidbufptr == NULL) {
		SMBDEBUG("SID malloc failed!\n");
		goto done;
	}
	error = md_get_mem(mdp, sidbufptr, ntwrk_sid_size, MB_MSYSTEM);
	if (error) {
		SMBDEBUG("Could get the list of sids? error = %d\n", error);
		goto done;
	}
	
	endsidbufptr = (char *)sidbufptr + ntwrk_sid_size;
	nextsidbufptr = sidbufptr;
	for (ii = 0; ii < ntwrk_sids_cnt; ii++) {		
		ntwrk_wire_sid = (struct ntsid *)nextsidbufptr;		
		nextsidbufptr += sizeof(*ntwrk_wire_sid);
		/* Make sure we don't overrun our buffer */
		if (nextsidbufptr > endsidbufptr) {
			SMBDEBUG("Network sid[%d] buffer to small start %p current %p end %p\n", 
					 ii, sidbufptr, nextsidbufptr, endsidbufptr);
			break;
		}
		/* 
		 * We are done with nextsidbufptr for this loop, reset it to the next 
		 * entry. The smb_sid2sid16 routine will protect us from any buffer overruns,
		 * so no need to check here.
		 */
		nextsidbufptr += (ntwrk_wire_sid->sid_subauthcount * sizeof(u_int32_t));
		
		smb_sid2sid16(ntwrk_wire_sid, &tmpsid, endsidbufptr);

		/* Don't store any unix_users or unix_groups sids */
		if (!smb_sid_in_domain(&unix_users_domsid, &tmpsid) &&
			!smb_sid_in_domain(&unix_groups_domsid, &tmpsid)) {
			ntwrk_sids[smp->ntwrk_sids_cnt++] = tmpsid;
		} else {
			SMBDEBUG("Skipping ntwrk_wire_sid entry %d\n", ii);
			if (ii == 0)
				goto done;
			else
				continue;
		}

		if (smbfs_loglevel == SMB_ACL_LOG_LEVEL) {
			smb_printsid(ntwrk_wire_sid, endsidbufptr, "WHOAMI network", NULL, ii, 0);
		}		

		/* First entry is the owner sid see if we can translate it */
		if (ii == 0) {
			error = kauth_cred_ntsid2uid(&tmpsid, &ntwrk_sid_uid);
			if (error) {
				SMBWARNING("Failed to translate the owner sid, error = %d\n", error);			
				goto done;
			} else {
				if (smbfs_loglevel == SMB_ACL_LOG_LEVEL) {
					SMBWARNING("local uid %d, translated sid to uid %d network uid = %lld\n", 
							   smp->sm_args.uid, ntwrk_sid_uid, smp->ntwrk_uid);				
				}
				/* 
				 * So we can translate the sid into a uid, but it doesn't match the 
				 * mounted users uid. This means they are not in the same managed realm,
				 * so we are done get out.
				 */
				if (ntwrk_sid_uid != smp->sm_args.uid)
					goto done;
			}			
		}
	}
	/* We found some unix_users or unix_groups, resize the buffer */
	if (smp->ntwrk_sids_cnt != ntwrk_sids_cnt) {
		size_t sidarraysize = smp->ntwrk_sids_cnt * sizeof(*ntwrk_sids);
		
		MALLOC(smp->ntwrk_sids, void *, sidarraysize, M_TEMP, M_WAITOK | M_ZERO);
		if (smp->ntwrk_sids) {
			bcopy(ntwrk_sids, smp->ntwrk_sids, sidarraysize);
			FREE(ntwrk_sids, M_TEMP);
		}
	}
	if (smp->ntwrk_sids == NULL)
		smp->ntwrk_sids = ntwrk_sids;
	ntwrk_sids = NULL;
	
done:
	/* Not using ntwrk_sids clean up */
	if (ntwrk_sids) {
		smp->ntwrk_sids_cnt = 0;
		FREE(ntwrk_sids, M_TEMP);
	}
	if (sidbufptr)
		FREE(sidbufptr, M_TEMP);
}

/*
 * The SMB_QFS_POSIX_WHOAMI allows us to find out who the server thinks we are
 * and what groups we are in. It can also return the list of SIDs, but currently
 * we ignore those. Maybe in the future.
 */
static int smbfs_unix_whoami(struct smb_share *ssp, struct smbmount *smp, vfs_context_t context)
{
	struct smb_t2rq *t2p;
	struct mbchain *mbp;
	struct mdchain *mdp;
	int error;
	u_int32_t ii;
	u_int32_t reserved;
	size_t total_bytes;
	u_int32_t ntwrk_sids_cnt;
	u_int32_t ntwrk_sid_size;	

	error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_QUERY_FS_INFORMATION, context, &t2p);
	if (error)
		return error;
	
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	mb_put_uint16le(mbp, SMB_QFS_POSIX_WHOAMI);
	t2p->t2_maxpcount = 4;
	t2p->t2_maxdcount = SSTOVC(ssp)->vc_txmax;
	error = smb_t2_request(t2p);
	if (error) 
		goto done;
	
	mdp = &t2p->t2_rdata;
	/*
	 * At this point md_cur and md_top have the same value. Now all the md_get 
	 * routines will will check for null, but just to be safe we check here
	 */
	if (mdp->md_cur == NULL) {
		error = EBADRPC;		
		goto done;
	}
	/* Currently only tells us if we logged in as guest, we should already know this by now */
	md_get_uint32le(mdp,  NULL); 	/* Mapping flags, currently only used for guest */
	md_get_uint32le(mdp,  NULL); /* Mask of valid mapping flags */
	md_get_uint64le(mdp,  &smp->ntwrk_uid);	/* Primary user ID */
	md_get_uint64le(mdp,  &smp->ntwrk_gid);	/* Primary group ID */
	md_get_uint32le(mdp,  &smp->ntwrk_cnt_gid); /* number of supplementary GIDs */
	md_get_uint32le(mdp,  &ntwrk_sids_cnt); /* number of SIDs */
	md_get_uint32le(mdp,  &ntwrk_sid_size); /* size of the list of SIDs */
	error = md_get_uint32le(mdp,  &reserved); /* Reserved (should be zero) */
	if (error)
		goto done;
	SMBWARNING("network uid = %lld network gid = %lld supplementary group cnt  = %d SID cnt = %d\n", 
			 smp->ntwrk_uid, smp->ntwrk_gid, smp->ntwrk_cnt_gid, ntwrk_sids_cnt);
	/* 
	 * If we can't get the reserved field then the buffer is not big enough. Both the
	 * group count and sid count must set to zero if no groups or sids are return.
	 * Added a little safty net here, we do not allow these fields to be negative.
	 */
	if (error || (reserved != 0) || ((int32_t)smp->ntwrk_cnt_gid < 0) || ((int32_t)ntwrk_sids_cnt < 0)) {
		if (! error)
			error = EBADRPC;
		goto done;
	}
	
	/* No group list see if there is a sid list */
	if (smp->ntwrk_cnt_gid == 0)
		goto sid_groups;

	/* Now check to make sure we don't have an integer overflow */
	total_bytes = smp->ntwrk_cnt_gid * sizeof(u_int64_t);
	if ((total_bytes / sizeof(u_int64_t)) != smp->ntwrk_cnt_gid) {
		error = EBADRPC;
		goto done;
	}
	
	/* Make sure we are not allocating more than we said we could handle */
	if (total_bytes > SSTOVC(ssp)->vc_txmax) {
		error = EBADRPC;
		goto done;
	}
	
	MALLOC(smp->ntwrk_gids, u_int64_t *, total_bytes, M_TEMP, M_WAITOK);
	/* Should never happen, but just to be safe */
	if (smp->ntwrk_gids == NULL) {
		error = ENOMEM;
		goto done;
	}
	for (ii = 0; ii < smp->ntwrk_cnt_gid; ii++) {
		error = md_get_uint64le(mdp,  &smp->ntwrk_gids[ii]);
		if (error)
			goto done;			
		SMBDEBUG("smp->ntwrk_gids[%d] = %lld\n", ii, smp->ntwrk_gids[ii]);
	}
	
	/* 
	 * At this point we have everything we really need. So any errors from this
	 * point on should be ignored. If we error out below we should just pretend 
	 * that we didn't get any network sids.
	 */
sid_groups:
	smb_get_sid_list(ssp, smp, mdp,ntwrk_sids_cnt, ntwrk_sid_size);

done:	
	smb_t2_done(t2p);

	if (error == EBADRPC)
		SMBERROR("Parsing error reading the message\n");
	
	if (error && smp->ntwrk_gids) {
		free(smp->ntwrk_gids, M_TEMP);
		smp->ntwrk_gids = NULL;
		smp->ntwrk_cnt_gid = 0;
	}
	return error;
}

/*
 * If this is a UNIX server then get its capiblities
 */
static void smbfs_unix_qfsattr(struct smb_share *ssp, struct smbmount *smp, vfs_context_t context)
{
	struct smb_t2rq *t2p;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int16_t majorv;
	u_int16_t minorv;
	u_int64_t cap;
	int error;
	
	error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_QUERY_FS_INFORMATION, context, &t2p);
	if (error)
		return;
	
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	mb_put_uint16le(mbp, SMB_QFS_UNIX_INFO);
	t2p->t2_maxpcount = 4;
	t2p->t2_maxdcount = 12;
	error = smb_t2_request(t2p);
	if (error)
		goto done;

	mdp = &t2p->t2_rdata;
	/*
	 * At this point md_cur and md_top have the same value. Now all the md_get 
	 * routines will will check for null, but just to be safe we check here
	 */
	if (mdp->md_cur == NULL) {
		SMBWARNING("Parsing error reading the message\n");
		error = EBADRPC;		
		goto done;
	}
	md_get_uint16le(mdp,  &majorv);
	md_get_uint16le(mdp, &minorv);
	md_get_uint64le(mdp, &cap);
	SMBWARNING("version %x.%x cap = %llx\n", majorv, minorv, cap);
	UNIX_CAPS(SSTOVC(ssp)) = UNIX_QFS_UNIX_INFO_CAP | (cap & CIFS_UNIX_POSIX_PATH_OPERATIONS_CAP);
	
	if (UNIX_CAPS(SSTOVC(ssp)) & CIFS_UNIX_POSIX_PATH_OPERATIONS_CAP) {
		UNIX_CAPS(SSTOVC(ssp)) |= UNIX_QFILEINFO_UNIX_LINK_CAP | UNIX_SFILEINFO_UNIX_LINK_CAP | 
		UNIX_QFILEINFO_UNIX_INFO2_CAP | UNIX_SFILEINFO_UNIX_INFO2_CAP;

		/*
		 * Seems Leopard Servers don't handle the posix unlink call correctly.
		 * They support the call and say it work, but they don't really delete
		 * the item. So until we stop supporting Leopard don't set this unless
		 * the server doesn't support the BSD flags. Mac servers support the 
		 * BSD flags, but Linux servers don't. So in mount_smbfs we will turn 
		 * this back on if we determine its a linux server.
		 * NOTE: Snow Leopard seems to work correctly
		 *
		 * UNIX_CAPS(SSTOVC(ssp)) |= UNIX_SFILEINFO_POSIX_UNLINK_CAP;
		 */
		
		/* See if the server supports the who am I operation */ 
		error = smbfs_unix_whoami(ssp, smp, context);
		if (! error)
			UNIX_CAPS(SSTOVC(ssp)) |= UNIX_QFS_POSIX_WHOAMI_CAP;
	}
done:
	smb_t2_done(t2p);
}

/* 
 * Since the first thing we do is set the default values there is no longer 
 * any reason to return an error for this routine. Some servers may not support
 * this call. We should not fail the mount just because they do not support this
 * call.
 */
void smbfs_smb_qfsattr(struct smb_share *ssp, struct smbmount *smp, vfs_context_t context)
{
	struct smb_t2rq *t2p;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int32_t nlen = 0;
	int error;
	size_t fs_nmlen;	/* The sized malloced for fs_name */
	struct smbfs_fctx	ctx;

	/* Start with the default values */
	ssp->ss_fstype = SMB_FS_FAT;	/* default to FAT File System */
	ssp->ss_attributes = 0;
	ssp->ss_maxfilenamelen = 255;
	
	error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_QUERY_FS_INFORMATION, context, &t2p);
	if (error)
		return;

	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	mb_put_uint16le(mbp, SMB_QFS_ATTRIBUTE_INFO);
	t2p->t2_maxpcount = 4;
	t2p->t2_maxdcount = 4 * 3 + 512;
	error = smb_t2_request(t2p);
	if (error)
		goto done;
	mdp = &t2p->t2_rdata;
	/*
	 * At this point md_cur and md_top have the same value. Now all the md_get 
	 * routines will will check for null, but just to be safe we check here
	 */
	if (mdp->md_cur == NULL)
		error = EBADRPC;		
	else {
		md_get_uint32le(mdp,  &ssp->ss_attributes);
		md_get_uint32le(mdp, &ssp->ss_maxfilenamelen);
		error = md_get_uint32le(mdp, &nlen);	/* fs name length */
	}
	if (error) {
		/* This is a very bad server */
		SMBWARNING("Server returned a bad SMB_QFS_ATTRIBUTE_INFO message\n");
		goto done;
	}
	if (ssp->ss_fsname == NULL && (nlen > 0) && (nlen < PATH_MAX)) {
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
			u_int32_t ii;
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
		ssp->ss_fsname = malloc(fs_nmlen, M_SMBSTR, M_WAITOK);
		if (ssp->ss_fsname == NULL) {
			goto done;	/* Should never happen, but just to be safe */
		}
		bcopy(ctx.f_name, ssp->ss_fsname, ctx.f_nmlen);
		ssp->ss_fsname[ctx.f_nmlen] = '\0';
		free(ctx.f_name, M_SMBFSDATA);
		
		/*
		 * Let's start keeping track of the file system type. Most
		 * things we need to do differently really depend on the
		 * file system type. As an example we know that FAT file systems
		 * do not update the modify time on drectories.
		 */
		if (strncmp(ssp->ss_fsname, "FAT", fs_nmlen) == 0)
			ssp->ss_fstype = SMB_FS_FAT;
		else if (strncmp(ssp->ss_fsname, "FAT12", fs_nmlen) == 0)
			ssp->ss_fstype = SMB_FS_FAT;
		else if (strncmp(ssp->ss_fsname, "FAT16", fs_nmlen) == 0)
			ssp->ss_fstype = SMB_FS_FAT;
		else if (strncmp(ssp->ss_fsname, "FAT32", fs_nmlen) == 0)
			ssp->ss_fstype = SMB_FS_FAT;
		else if (strncmp(ssp->ss_fsname, "CDFS", fs_nmlen) == 0)
			ssp->ss_fstype = SMB_FS_CDFS;
		else if (strncmp(ssp->ss_fsname, "UDF", fs_nmlen) == 0)
			ssp->ss_fstype = SMB_FS_UDF;
		else if (strncmp(ssp->ss_fsname, "NTFS", fs_nmlen) == 0)
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
done:
	smb_t2_done(t2p);
	/* Its a unix server see if it supports any of the extensions */
	if ((error == 0) && (UNIX_SERVER(SSTOVC(ssp))))
		smbfs_unix_qfsattr(ssp, smp, context);
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
smbfs_smb_statfs(struct smb_share *ssp, struct vfsstatfs *sbp, vfs_context_t context)
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
	size_t xmax;

	error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_QUERY_FS_INFORMATION,
	    context, &t2p);
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
	/*
	 * At this point md_cur and md_top have the same value. Now all the md_get 
	 * routines will will check for null, but just to be safe we check here
	 */
	if (mdp->md_cur == NULL) {
		SMBWARNING("Parsing error reading the message\n");
		smb_t2_done(t2p);
		return EBADRPC;		
	}
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
	sbp->f_bsize = (uint32_t)s;	/* fundamental file system block size */
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

int smbfs_smb_seteof(struct smb_share *ssp, u_int16_t fid, u_int64_t newsize, vfs_context_t context)
{
	struct smb_t2rq *t2p;
	struct smb_vc *vcp = SSTOVC(ssp);
	struct mbchain *mbp;
	int error;

	error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_SET_FILE_INFORMATION, context, &t2p);
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

int
smbfs_smb_t2rename(struct smbnode *np, const char *tname, size_t tnmlen, 
				   vfs_context_t context, int overwrite, u_int16_t *infid)
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
			     context, &t2p);
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
		error = smbfs_smb_tmpopen(np, STD_RIGHT_DELETE_ACCESS, context, &fid);
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
	len = (int32_t)smb_strtouni((u_int16_t *)convbuf, tname, tnmlen, UTF_PRECOMPOSED|UTF_NO_NULL_TERM);
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
		cerror = smbfs_smb_tmpclose(np, fid, context);
		if (cerror) {
			SMBWARNING("error %d closing fid %d\n", cerror, fid);
		}
	}
	smb_t2_done(t2p);
	return (error);
}

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
smbfs_delete_openfile(struct smbnode *dnp, struct smbnode *np, vfs_context_t context)
{
	struct proc	*p = vfs_context_proc(context);
	u_int16_t	fid = 0;
	int			error, cerror;
	char		s_name[32];	/* make sure that sillyrename_name will fit */
	int32_t		s_namlen;
	int			i, j, k;
	
	error = smbfs_smb_tmpopen(np, STD_RIGHT_DELETE_ACCESS, context, &fid);
	if (error)
		return(error);
	/* Get the first silly name */
	s_namlen = snprintf(s_name, sizeof(s_name), ".smbdeleteAAA%04x4.4", proc_pid(p));
	if (s_namlen >=  (int32_t)sizeof(s_name)) {
	    error = ENOENT;
	    goto out;
	 }
	/* Try rename until we get one that isn't there */
	i = j = k = 0;

	do {
		error = smbfs_smb_t2rename(np, s_name, s_namlen, context, 0, &fid);
		/* 
		 * SAMBA Bug:
		 * 
		 * Some times the SAMBA code gets confused and fails the above rename
		 * with an ENOENT error. We need to make sure this code work with all 
		 * SAMBA servers, so try again with the old rename call. SAMBA allows us 
		 * to rename an open file with this call, but not with delete 
		 * access. So close it, rename it and hide it.
		 */
		if ((error == ENOENT) && SMBTOV(dnp)) {
			if (fid) {
				(void)smbfs_smb_tmpclose(np, fid, context);
				fid = 0;
			}
			error = smbfs_smb_rename(np, dnp, s_name, s_namlen, context);
			if (! error) {
				/* ignore any errors return from hiding the item */
				(void)smbfs_smb_hideit(dnp, s_name, s_namlen, context);
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
			if (smbfs_smb_query_info(dnp, s_name, s_namlen, &fap, context) == 0)
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
	(void)smbfs_smb_hideit(dnp, s_name, s_namlen, context);
	
	cerror = smbfs_smb_markfordelete(np->n_mount->sm_share, np, context, &fid);
	if (cerror) {	/* We will have to do the delete ourself! Could be SAMBA */
		np->n_flag |= NDELETEONCLOSE;
	}
	
out:;  
	if (fid) {
		cerror = smbfs_smb_tmpclose(np, fid, context);
		if (cerror) {
			SMBWARNING("error %d closing fid %d\n", cerror, fid);
		}
	}
	if (!error) {
		u_char *new_name = smbfs_name_alloc((u_char *)s_name, s_namlen);
		u_char *old_name = np->n_name;

		smb_vhashrem(np);
		/* Now reset the name, so other path lookups can use it. */
		lck_rw_lock_exclusive(&np->n_name_rwlock);
		np->n_name = new_name;
		np->n_nmlen = s_namlen;
		lck_rw_unlock_exclusive(&np->n_name_rwlock);
		np->n_flag |= NMARKEDFORDLETE;
		/* Now its safe to free the old name */
		if (old_name)
			smbfs_name_free(old_name);
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
int smbfs_smb_flush(struct smbnode *np, vfs_context_t context)
{
	struct smb_share *ssp = np->n_mount->sm_share;
	struct smb_rq rq, *rqp = &rq;
	struct mbchain *mbp;
	int error;
	u_int16_t fid = 0;
	pid_t	pid = proc_pid(vfs_context_proc(context));

	/* Flush if we have written to the file or done a set eof. */
	if (!(np->n_flag & NFLUSHWIRE))
		return (0);
	if ((np->f_refcnt <= 0) || (!SMBTOV(np)) || (!vnode_isreg(SMBTOV(np))))
		return 0; /* not a regular open file */

	/* Before trying the flush see if the file needs to be reopened */
	error = smbfs_smb_reopen_file(np, context);
	if (error) {
		SMBDEBUG(" %s waiting to be revoked\n", np->n_name);
	    return(error);
	}
	
	/* See if the file is opened for write access */
	if (smbfs_findFileRef(SMBTOV(np), pid, kAccessWrite, kCheckDenyOrLocks, 0, 0, NULL, &fid)) {
		fid = np->f_fid;	/* Nope use the shared fid */
		if ((fid == 0) || ((np->f_accessMode & kAccessWrite) != kAccessWrite))
			return(0);	/* Nothing to do here get out */
	}
	error = smb_rq_init(rqp, SSTOCP(ssp), SMB_COM_FLUSH, context);
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
smbfs_smb_setfsize(struct smbnode *np, u_int16_t fid, u_int64_t newsize, vfs_context_t context)
{
	struct smb_share *ssp = np->n_mount->sm_share;
	struct smb_rq rq, *rqp = &rq;
	struct mbchain *mbp;
	int error;

	/* Windows 98 does not support this call so don't even try it. */
	if (SSTOVC(ssp)->vc_sopt.sv_caps & SMB_CAP_NT_SMBS) {
		if (!smbfs_smb_seteof(ssp, fid, newsize, context)) {
			np->n_flag |= (NFLUSHWIRE | NATTRCHANGED);
			if (ssp->ss_fstype == SMB_FS_FAT) {
				if (smbfs_smb_flush(np, context) == 0)
					np->n_flag &= ~NFLUSHWIRE;
			}
			return (0);
		}
	}
	/*
	 * For servers that do not support the SMB_CAP_NT_SMBS we need to
	 * do a zero length write as per the SMB Core Reference. 
	 */
	if (newsize > UINT32_MAX)
		return (EFBIG);

	error = smb_rq_init(rqp, SSTOCP(ssp), SMB_COM_WRITE, context);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_mem(mbp, (caddr_t)&fid, 2, MB_MSYSTEM);
	mb_put_uint16le(mbp, 0);
	mb_put_uint32le(mbp, (u_int32_t)newsize);
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
	if (smbfs_smb_flush(np, context))
		np->n_flag |= NFLUSHWIRE;	/* Didn't work, Try again later? */
	return error;
}

/*
 * XXX - We need to remove this routine and replace it with something more 
 * modern. See <rdar://problem/7595213>
 */
int smbfs_smb_query_info(struct smbnode *np, const char *name, size_t len, 
						 struct smbfattr *fap, vfs_context_t context)
{
	struct smb_rq rq, *rqp = &rq;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int8_t wc;
	int error;
	u_int16_t wattr;
	u_int32_t lint;

	error = smb_rq_init(rqp, SSTOCP(ssp), SMB_COM_QUERY_INFORMATION, context);
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
 * Set DOS file attributes, may want to replace with a more modern call
 */
int smbfs_smb_setpattr(struct smbnode *np, const char *name, size_t len, 
					   u_int16_t attr, vfs_context_t context)
{
	struct smb_rq rq, *rqp = &rq;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct mbchain *mbp;
	u_int32_t time;
	int error;

	error = smb_rq_init(rqp, SSTOCP(ssp), SMB_COM_SET_INFORMATION, context);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint16le(mbp, attr);
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

int smbfs_smb_hideit(struct smbnode *np, const char *name, size_t len,
		 vfs_context_t context)
{
	struct smbfattr fa;
	int error;
	u_int16_t attr;

	error = smbfs_smb_query_info(np, name, len, &fa, context);
	attr = fa.fa_attr;
	if (!error && !(attr & SMB_FA_HIDDEN)) {
		attr |= SMB_FA_HIDDEN;
		error = smbfs_smb_setpattr(np, name, len, attr, context);
	}
	return (error);
}


int smbfs_smb_unhideit(struct smbnode *np, const char *name, size_t len, 
					   vfs_context_t context)
{
	struct smbfattr fa;
	u_int16_t attr;
	int error;

	error = smbfs_smb_query_info(np, name, len, &fa, context);
	attr = fa.fa_attr;
	if (!error && (attr & SMB_FA_HIDDEN)) {
		attr &= ~SMB_FA_HIDDEN;
		error = smbfs_smb_setpattr(np, name, len, attr, context);
	}
	return (error);
}

int
smbfs_set_unix_info2(struct smbnode *np, struct timespec *crtime, struct timespec *mtime, struct timespec *atime, 
	u_int64_t fsize,  u_int64_t perms, u_int32_t FileFlags, u_int32_t FileFlagsMask, vfs_context_t context)
{
	struct smb_t2rq *t2p;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct smb_vc *vcp = SSTOVC(ssp);
	struct mbchain *mbp;
	u_int64_t tm;
	u_int32_t ftype;
	int error, tzoff;
	
	error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_SET_PATH_INFORMATION, context, &t2p);
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
		smb_time_local2NT(atime, &tm, FALSE);
	else 
	     tm = 0;
	mb_put_uint64le(mbp, tm);

	/* set the write/modify time */	
	if (mtime)
		smb_time_local2NT(mtime, &tm, FALSE);
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
	ftype = SMB_DEFAULT_NO_CHANGE;
	mb_put_uint32le(mbp, ftype);
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
	mb_put_uint64le(mbp, perms);
	/* Number of hard link */
	tm = SMB_DEFAULT_NO_CHANGE;
	mb_put_uint64le(mbp, tm);
	/* set the creation time */
	if (crtime)
		smb_time_local2NT(crtime, &tm, FALSE);
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
int
smbfs_smb_setpattrNT(struct smbnode *np, u_int32_t attr, 
			struct timespec *crtime, struct timespec *mtime,
			struct timespec *atime, vfs_context_t context)
{
	struct smb_t2rq *t2p;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct smb_vc *vcp = SSTOVC(ssp);
	struct mbchain *mbp;
	u_int64_t tm;
	int error, tzoff;

	error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_SET_PATH_INFORMATION,
	    context, &t2p);
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
	tm = 0;
	if (crtime) {
		smb_time_local2NT(crtime, &tm, (ssp->ss_fstype == SMB_FS_FAT));
	}
	mb_put_uint64le(mbp, tm);
	
	/* set the access time */
	tm = 0;
	if (atime) {
		smb_time_local2NT(atime, &tm, (ssp->ss_fstype == SMB_FS_FAT));
	}
	mb_put_uint64le(mbp, tm);
	
	/* set the write/modify time */	
	tm = 0;
	if (mtime) {
		smb_time_local2NT(mtime, &tm, (ssp->ss_fstype == SMB_FS_FAT));
	}
	mb_put_uint64le(mbp, tm);
	
	/* Never let them set the change time */		
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
 * Set DOS file attributes.
 * Looks like this call can be used only if CAP_NT_SMBS bit is on.
 */
int
smbfs_smb_setfattrNT(struct smbnode *np, u_int32_t attr, u_int16_t fid,
			struct timespec *crtime, struct timespec *mtime,
			struct timespec *atime, vfs_context_t context)
{
	struct smb_t2rq *t2p;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct smb_vc *vcp = SSTOVC(ssp);
	struct mbchain *mbp;
	u_int64_t tm;
	int error;

	error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_SET_FILE_INFORMATION,
	    context, &t2p);
	if (error)
		return error;
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
	}
		/* set the creation time */	
	tm = 0;
	if (crtime) {
		smb_time_local2NT(crtime, &tm, (ssp->ss_fstype == SMB_FS_FAT));
	}
	mb_put_uint64le(mbp, tm);
	
	/* set the access time */
	tm = 0;
	if (atime) {
		smb_time_local2NT(atime, &tm, (ssp->ss_fstype == SMB_FS_FAT));
	}
	mb_put_uint64le(mbp, tm);
	
	/* set the write/modify time */
	tm = 0;
	if (mtime) {
		smb_time_local2NT(mtime, &tm, (ssp->ss_fstype == SMB_FS_FAT));
	}
	mb_put_uint64le(mbp, tm);
	
	/* We never allow anyone to set the change time, but see note above about Windows 98 */		
	tm = 0;
	if (vcp->vc_flags & SMBV_WIN98) {
		smb_time_local2NT(&np->n_chtime, &tm, (ssp->ss_fstype == SMB_FS_FAT));
	}
	
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
int smbfs_smb_ntcreatex(struct smbnode *np, u_int32_t rights, u_int32_t shareMode, 
						vfs_context_t context, enum vtype vt, u_int16_t *fidp, 
						const char *name, size_t in_nmlen, u_int32_t disp, 
						int xattr, struct smbfattr *fap)
{
	struct smb_rq rq, *rqp = &rq;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct smb_vc *vcp = SSTOVC(ssp);
	int unix_info2 = ((UNIX_CAPS(vcp) & UNIX_QFILEINFO_UNIX_INFO2_CAP)) ? TRUE : FALSE;	
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int8_t wc;
	u_int32_t lint, createopt, efa;
	u_int64_t llint;
	int error;
	u_int16_t fid, *namelenp;
	size_t nmlen = in_nmlen;	/* Don't change the input name length, we need it for making the ino number */

	DBG_ASSERT(fap); /* Should never happen */
	bzero(fap, sizeof(*fap));
	nanouptime(&fap->fa_reqtime);
	error = smb_rq_init(rqp, SSTOCP(ssp), SMB_COM_NT_CREATE_ANDX, context);
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
	 * If this is just an open call on a stream then always request it to be created if it doesn't exist. See the 
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
	/*
	 * The server supports reparse points so open the item with a reparse point 
	 * and bypass normal reparse point processing for the file.
	 */
	if (ssp->ss_attributes & FILE_SUPPORTS_REPARSE_POINTS) {
		createopt |= NTCREATEX_OPTIONS_OPEN_REPARSE_POINT;
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
		 * Spec say 26 for word count, but 34 words are defined and observed from 
		 * all servers.  
		 *
		 * The spec is wrong and word count should always be 34 unless we request 
		 * the extended reply. Now some server will always return 42 even it the 
		 * NTCREATEX_FLAGS_EXTENDED flag is not set.
		 * 
		 * From the MS-SMB document concern the extend response:
		 *
		 * The word count for this response MUST be 0x2A (42). WordCount in this 
		 * case is not used as the count of parameter words but is just a number.
		 */
		
		if (md_get_uint8(mdp, &wc) != 0 ||
		    (wc != 26 && wc != NTCREATEX_NORMAL_WDCNT && wc != NTCREATEX_EXTENDED_WDCNT)) {
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
			smb_time_NT2local(llint, &fap->fa_crtime);
		}
		md_get_uint64le(mdp, &llint);   /* access time */
		if (llint) {
			smb_time_NT2local(llint, &fap->fa_atime);
		}
		md_get_uint64le(mdp, &llint);   /* write time */
		if (llint) {
			smb_time_NT2local(llint, &fap->fa_mtime);
		}
		md_get_uint64le(mdp, &llint);   /* change time */
		if (llint) {
			smb_time_NT2local(llint, &fap->fa_chtime);
		}
		md_get_uint32le(mdp, &lint);    /* attributes */
		fap->fa_attr = lint;
		/*
		 * Because of the Steve/Conrad Symlinks we can never be completely
		 * sure that we have the corret vnode type if its a file. For 
		 * directories we always know the correct information.
		 */
		if (fap->fa_attr & SMB_EFA_DIRECTORY) {
			fap->fa_valid_mask |= FA_VTYPE_VALID;
		}		
		fap->fa_vtype = (fap->fa_attr & SMB_EFA_DIRECTORY) ? VDIR : VREG;
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
	 * We only get to this point if the n_vnode exist and we are doing a normal 
	 * open. If we are using UNIX extensions then we can't trust some of the 
	 * values returned from this open response. We need to reset some of the 
	 * value back to what we found in in the UNIX Info2 lookup.
	 */
	if (unix_info2) {
		/* 
		 * We are doing unix extensions yet this fap didn't come from a unix 
		 * info2 call. The fa_attr and fa_vtype could be wrong. We cannot trust 
		 * the read only bit in this case so leave it set to the value we got 
		 * from the last unix info2 call. We cannot trust the fa_vtype either so
		 * leave it set to the value we got from the last unix info2 call. 
		 */
		/* Reset it to look like a UNIX Info2 lookup */
		fap->fa_unix = TRUE;
		fap->fa_flags_mask = np->n_flags_mask;
		fap->fa_nlinks = np->n_nlinks;
		
		fap->fa_attr &= ~SMB_FA_RDONLY;
		fap->fa_attr |= (np->n_dosattr & SMB_FA_RDONLY);
		fap->fa_valid_mask |= FA_VTYPE_VALID;
		fap->fa_vtype = vnode_vtype(np->n_vnode);
		/* Make sure we have the correct fa_attr setting */
		if (vnode_isdir(np->n_vnode))
			fap->fa_attr |= SMB_FA_DIR;
		else
			fap->fa_attr &= ~SMB_FA_DIR;
		/* 
		 * Samba will return the modify time for the change time in this 
		 * call. So if we are doing unix extensions never trust the change 
		 * time retrieved from this call.
		 */		
		fap->fa_chtime = np->n_chtime;		
	}  
		
	/* 
 	 * We have all the meta data attributes so update the cache. If the 
	 * calling routine is setting an attribute it should not change the 
	 * smb node value until after the open has completed. NOTE: The old 
	 * code would only update the cache if the mtime, attributes and size 
	 * haven't changed.
 	 */
	smbfs_attr_cacheenter(np->n_vnode, fap, TRUE, context);
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


static int smbfs_smb_oldopen(struct smbnode *np, int accmode, vfs_context_t context, 
							 u_int16_t *fidp, const char *name, size_t nmlen, 
							 int xattr, struct smbfattr *fap)
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
	nanouptime(&fap->fa_reqtime);
	error = smb_rq_init(rqp, SSTOCP(ssp), SMB_COM_OPEN, context);
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
	if (smbfs_attr_cachelookup(np->n_vnode, NULL, context, FALSE) != 0)
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
	smbfs_attr_cacheenter(np->n_vnode, fap, TRUE, context);
WeAreDone:
	return 0;
}

int
smbfs_smb_tmpopen(struct smbnode *np, u_int32_t rights, vfs_context_t context, u_int16_t *fidp)
{
	struct smb_vc	*vcp = SSTOVC(np->n_mount->sm_share);
	int		searchOpenFiles;
	int		error = 0;
	struct smbfattr fattr;

	/* If no vnode or the vnode is a directory then don't use already open items */
	if (!np->n_vnode || vnode_isdir(np->n_vnode))
		searchOpenFiles = FALSE;
	else {
		/* Check to see if the file needs to be reopened */
		error = smbfs_smb_reopen_file(np, context);
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
		else if ((vcp->vc_sopt.sv_caps & SMB_CAP_NT_SMBS) && (rights & SA_RIGHT_FILE_WRITE_ATTRIBUTES))
			searchOpenFiles = FALSE;
		else
			searchOpenFiles = TRUE;
	}
		
	/* 
	 * Remmeber we could have been called before the vnode is create. Conrads 
	 * crazy symlink code. So if we have no vnode then we cannot borrow the
	 * fid. Only borrow a fid if the requested access modes could have been
	 * made on an open call. 
	 */
	if (searchOpenFiles && SMBTOV(np)) {
		u_int16_t accessMode = 0;
		pid_t pid = proc_pid(vfs_context_proc(context));
		
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
		if (np->f_refcnt && (smbfs_findFileRef(SMBTOV(np), pid, accessMode, kAnyMatch, 0, 0, NULL, fidp) == 0)) {
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
		
		error = smbfs_smb_oldopen(np, accmode, context, fidp, NULL, 0, 0, &fattr);
	}
	else {
		u_int32_t shareMode = NTCREATEX_SHARE_ACCESS_ALL;

		error = smbfs_smb_ntcreatex(np, rights, shareMode, context, 
					(np->n_vnode  && vnode_isdir(np->n_vnode)) ? VDIR : VREG, fidp, 
					NULL, 0, NTCREATEX_DISP_OPEN,  0, &fattr);
	}
	if (error)
		SMBWARNING("%s failed to open: error = %d\n", np->n_name, error);
	return (error);
}

int
smbfs_smb_tmpclose(struct smbnode *np, u_int16_t fid, vfs_context_t context)
{
	struct smb_share *ssp = np->n_mount->sm_share;
	struct fileRefEntry *entry = NULL;
	vnode_t vp = SMBTOV(np);

	/* 
	 * Remeber we could have been called before the vnode is create. Conrads 
	 * crazy symlink code. So if we have no vnode then we did not borrow the
	 * fid. If we did not borrow the fid then just close the fid and get out.
	 *
	 * If no vnode or the vnode is a directory then just close it, we are not
	 * sharing the open.
	 */
	if (!vp || vnode_isdir(vp) || ((fid != np->f_fid) && (smbfs_findFileEntryByFID(vp, fid, &entry)))) {
		return(smbfs_smb_close(ssp, fid, context));
	}
	/* 
	 * OK we borrowed the fid do we have the last reference count on it. If 
	 * yes, then we need to close up every thing. smbfs_close can handle this
	 * for us.
	 */
	if (np->f_refcnt == 1) {
		/* Open Mode does not matter becasue we closing everything */
		return(smbfs_close(vp, 0, context));
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

int smbfs_smb_openread(struct smbnode *np, u_int16_t *fid, u_int32_t rights, uio_t uio, 
					   size_t *sizep,  const char *name, struct timespec *mtime, 
					   vfs_context_t context)
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
	u_int32_t len = (u_int32_t)uio_resid(uio);
	size_t nmlen = strnlen(name, ssp->ss_maxfilenamelen+1);

	/* 
	 * Make sure the whole response message will fit in our max buffer size. Since we 
	 * use the CreateAndX open call make sure the server supports that call. The calling
	 * routine must handle this routine returning ENOTSUP.
	 */	
	if (((vcp->vc_txmax - SMB_MAX_CHAIN_READ) < len) || !(vcp->vc_sopt.sv_caps & SMB_CAP_NT_SMBS))
		return(ENOTSUP);
	
	/* encode the CreateAndX request */
	error = smb_rq_init(rqp, SSTOCP(ssp), SMB_COM_NT_CREATE_ANDX, context);
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
	mb_put_uint32le(mbp, len >> 16);		/* MaxCountHigh */
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
			smb_time_NT2local(llint, mtime);
	}
	else 
		md_get_uint64le(mdp, NULL);   /* write time */
		
	md_get_uint64le(mdp, NULL);   /* change time */
	md_get_uint32le(mdp, NULL);    /* attributes */
	md_get_uint64le(mdp, NULL);     /* allocation size */
	md_get_uint64le(mdp, &eof);   /* EOF */
	if (sizep) 
		*sizep = (size_t)eof;		/* The calling routines can only handle size_t */
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

static int smbfs_smb_open_file(struct smbnode *np, u_int32_t rights, u_int32_t shareMode, 
							   vfs_context_t context, u_int16_t *fidp, const char *name, 
							   size_t nmlen, int xattr, struct smbfattr *fap)
{
	int error;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct smb_vc *vcp = SSTOVC(ssp);
	
	if (vcp->vc_sopt.sv_caps & SMB_CAP_NT_SMBS) {
		error = smbfs_smb_ntcreatex(np, rights, shareMode, context, VREG, fidp, name, 
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
		
		error = smbfs_smb_oldopen(np, smb_rights2mode(rights), context,
					  fidp, name, nmlen, xattr, fap);
	}
	return (error);
}

int
smbfs_smb_open(struct smbnode *np, u_int32_t rights, u_int32_t shareMode, 
			   vfs_context_t context, u_int16_t *fidp)
{
	struct smbfattr fattr;
	
	return(smbfs_smb_open_file(np, rights, shareMode, context, fidp, NULL, 0, FALSE, &fattr));
}

int
smbfs_smb_open_xattr(struct smbnode *np, u_int32_t rights, u_int32_t shareMode, 
					 vfs_context_t context,  u_int16_t *fidp, const char *name, size_t *sizep)
{
	struct smb_share *ssp = np->n_mount->sm_share;
	size_t nmlen = strnlen(name, ssp->ss_maxfilenamelen+1);
	int error;
	struct smbfattr fattr;
	
	error = smbfs_smb_open_file(np, rights, shareMode, context, fidp, name, nmlen, TRUE, &fattr);
	if (!error && sizep)
		*sizep = (size_t)fattr.fa_size;
	return(error);
}

int 
smbfs_smb_reopen_file(struct smbnode *np, vfs_context_t context)
{
	struct smb_share *ssp = np->n_mount->sm_share;
	int error = 0;
	struct timespec n_mtime = np->n_mtime;	/* Remember open can change this value save it */
	u_quad_t		n_size = np->n_size;	/* Remember open can change this value save it */
	struct smbfattr fattr;
	
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
			error = smbfs_smb_open_file(np, curr->rights, shareMode, context, &curr->fid, NULL, 0, FALSE, &fattr);
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
				error = smbfs_smb_lock(np, SMB_LOCK_EXCL, curr->fid, brl->lck_pid, brl->offset, brl->length, context, 0);
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
				(void)smbfs_smb_close(ssp, curr->fid, context);
				curr->fid = -1;
				curr = curr->next;
			}
		}
		lck_mtx_unlock(&np->f_openDenyListLock);

	} else {
		/* %%% Should be able to support carbon locks on POSIX Opens. */
		/* POSIX Open: Reopen with the same modes we had it open with before the reconnect */
		error = smbfs_smb_open_file(np, np->f_rights, NTCREATEX_SHARE_ACCESS_ALL, context, &np->f_fid, NULL, 0, FALSE, &fattr);
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
			error = smbfs_smb_lock(np, SMB_LOCK_EXCL, np->f_fid, flk->lck_pid, flk->start, flk->len, context, 0);
			if (error)
				SMBERROR("Reopen %s failed because we could not reestablish the lock! \n",  np->n_name);							
		}
		/* Something is different or we failed on the lock request, close it */
		if (error)
			(void)smbfs_smb_close(ssp, np->f_fid, context);
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

int
smbfs_smb_close(struct smb_share *ssp, u_int16_t fid, vfs_context_t context)
{
	struct smb_rq rq, *rqp = &rq;
	struct mbchain *mbp;
	u_int32_t time;
	int error;

	/* 
	 * Notice a close with a zero file descriptor in a packet trace. I have no idea how
	 * that could happen. Putting in this assert to see if we can every catch it happening
	 * again.
	 */
	DBG_ASSERT(fid);
	error = smb_rq_init(rqp, SSTOCP(ssp), SMB_COM_CLOSE, context);
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

static int smbfs_smb_oldcreate(struct smbnode *dnp, const char *name, size_t nmlen,
							   vfs_context_t context, u_int16_t *fidp, int xattr)
{
	struct smb_rq rq, *rqp = &rq;
	struct smb_share *ssp = dnp->n_mount->sm_share;
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int8_t wc;
	u_int32_t tm;
	int error;
	u_int16_t attr = SMB_FA_ARCHIVE;

	error = smb_rq_init(rqp, SSTOCP(ssp), SMB_COM_CREATE, context);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	if (name && *name == '.')
		attr |= SMB_FA_HIDDEN;
	mb_put_uint16le(mbp, attr);		/* attributes  */
	/* 
	 * This is a depricated call, we assume no one looks at the create time
	 * field. We assume any server that does looks at this value will take zero 
	 * to mean don't set the create time.
	 */
	tm = 0;
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

int
smbfs_smb_create(struct smbnode *dnp, const char *in_name, size_t in_nmlen, u_int32_t rights,
				 vfs_context_t context, u_int16_t *fidp, u_int32_t disp, int xattr, 
				 struct smbfattr *fap)
{
	const char *name = in_name;
	size_t nmlen = in_nmlen;
	uint16_t fid = 0;
	int error;
	struct smb_share *share = dnp->n_mount->sm_share;
	struct smb_vc *vcp = SSTOVC(share);

	/*
	 * When the SMB_CAP_NT_SMBS mode is set we can pass access rights into the create
	 * call. Windows 95/98/Me do not support SMB_CAP_NT_SMBS so they get the same 
	 * treatment as before. We are not removing any support just adding.
	 */
	if (vcp->vc_sopt.sv_caps & SMB_CAP_NT_SMBS) {
		error = smbfs_smb_ntcreatex(dnp, rights, NTCREATEX_SHARE_ACCESS_ALL,
					context, VREG,  &fid, name, nmlen, disp, xattr, fap);
	} else {
		/* Should be remove as part of <rdar://problem/8209715> */
		bzero(fap, sizeof(*fap));		
		error = smbfs_smb_oldcreate(dnp, name, nmlen, context, &fid, xattr);
		if (!error) {
			/* No attributes returned on the create, do a lookup */
			error = smbfs_smb_lookup(dnp, &name, &nmlen, fap, context);
		}
		/* If lookup returned a new name free it we never need that name */
		if (name != in_name) {
			SMB_FREE(name, M_SMBNODENAME);
		}		
	}
	if (fidp) {
		/* Caller wants the FID, return it to them */
		*fidp = fid;
	} else if (fid) {
		/* Caller doesn't want the FID, close it if we have it opened */
		(void)smbfs_smb_close(share, fid, context);
	}
	
	return error;
}

/*
 * This is the only way to remove symlinks with a samba server.
 */
static int smbfs_posix_unlink(struct smbnode *np, vfs_context_t context, 
							  const char *name, size_t nmlen)
{
	struct smb_t2rq *t2p;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct mbchain *mbp;
	int error;
	u_int32_t isDir = (vnode_isdir(np->n_vnode)) ? 1 : 0;
	
	error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_SET_PATH_INFORMATION, context, &t2p);
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
	mb_put_uint32le(mbp, isDir);
	
	t2p->t2_maxpcount = 2;
	t2p->t2_maxdcount = SSTOVC(ssp)->vc_txmax;
	error = smb_t2_request(t2p);
	smb_t2_done(t2p);
	return error;
}

int smbfs_smb_delete(struct smbnode *np, vfs_context_t context, const char *name, 
					 size_t nmlen, int xattr)
{
	struct smb_rq rq, *rqp = &rq;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct mbchain *mbp;
	int error;

	/* Not doing extended attribute and they support the posix unlink call */
	if (!xattr && (UNIX_CAPS(SSTOVC(ssp)) & UNIX_SFILEINFO_POSIX_UNLINK_CAP)) {
		error = smbfs_posix_unlink(np, context, name, nmlen);
		
		/* 
		 * If the file doesn't have write posix modes then Samba returns 
		 * NT_STATUS_CANNOT_DELETE, which we convert to EPERM. This seems
		 * wrong we are expecting posix symantics from the call. So for now
		 * try to change the mode and attempt the delete again.
		 */
		if (error == EPERM) {
			int chmod_error;
			uint64_t vamode = np->n_mode | S_IWUSR;
			
			/* See if we can chmod on the file */
			chmod_error = smbfs_set_unix_info2(np, NULL, NULL, NULL, SMB_SIZE_NO_CHANGE, 
											   vamode, SMB_FLAGS_NO_CHANGE, SMB_FLAGS_NO_CHANGE, context);
			if (chmod_error == 0) {
				error = smbfs_posix_unlink(np, context, name, nmlen);
			}
		}		
		if (error != ENOTSUP) {
			return error;
		} else {
			/* They don't really support this call, don't call them again */
			UNIX_CAPS(SSTOVC(ssp)) &= ~UNIX_SFILEINFO_POSIX_UNLINK_CAP;
		}
	}

	error = smb_rq_init(rqp, SSTOCP(ssp), SMB_COM_DELETE, context);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint16le(mbp, SMB_FA_SYSTEM | SMB_FA_HIDDEN);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint8(mbp, SMB_DT_ASCII);
	error = smbfs_fullpath(mbp, SSTOVC(ssp), np, name, &nmlen, 
						   UTF_SFM_CONVERSIONS, xattr ? ':' : '\\');
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

int smbfs_smb_rename(struct smbnode *src, struct smbnode *tdnp, const char *tname, 
					 size_t tnmlen, vfs_context_t context)
{
	struct smb_rq rq, *rqp = &rq;
	struct smb_share *ssp = src->n_mount->sm_share;
	struct mbchain *mbp;
	int error, retest = 0;

	error = smb_rq_init(rqp, SSTOCP(ssp), SMB_COM_RENAME, context);
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

		if ((smbfs_smb_query_info(src, NULL, 0, &fattr, context) == ENOENT) &&
			(smbfs_smb_query_info(tdnp, tname, tnmlen, &fattr, context) == 0))
			error = 0;
	}
	return error;
}

static int smbfs_smb_oldmkdir(struct smbnode *dnp, const char *name, size_t len, 
							  vfs_context_t context)
{
	struct smb_rq rq, *rqp = &rq;
	struct smb_share *ssp = dnp->n_mount->sm_share;
	struct mbchain *mbp;
	int error;

	error = smb_rq_init(rqp, SSTOCP(ssp), SMB_COM_CREATE_DIRECTORY, context);
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

int smbfs_smb_mkdir(struct smbnode *dnp, const char *name, size_t len, 
					vfs_context_t context, struct smbfattr *fap)
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
					context, VDIR, &fid, name, len, NTCREATEX_DISP_CREATE, 0, fap);
		if (error)
			return (error);
		error = smbfs_smb_close(ssp, fid, context);
		if (error)
			SMBERROR("error %d closing fid %d\n", error, fid);
		return (0);
	} else
		return (smbfs_smb_oldmkdir(dnp, name, len, context));
}

int
smbfs_smb_rmdir(struct smbnode *np, vfs_context_t context)
{
	struct smb_rq rq, *rqp = &rq;
	struct smb_share *ssp = np->n_mount->sm_share;
	struct mbchain *mbp;
	int error;

	error = smb_rq_init(rqp, SSTOCP(ssp), SMB_COM_DELETE_DIRECTORY, context);
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

/*
 * TRANS2_FIND_FIRST2/NEXT2, used for NT LM12 dialect
 */
static int
smbfs_smb_trans2find2(struct smbfs_fctx *ctx, vfs_context_t context)
{
	struct smb_t2rq *t2p;
	struct smb_vc *vcp = SSTOVC(ctx->f_ssp);
	struct mbchain *mbp;
	struct mdchain *mdp;
	u_int16_t tw, flags;
	size_t len;
	int error;

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
		error = smb_t2_alloc(SSTOCP(ctx->f_ssp), SMB_TRANS2_FIND_FIRST2, context, &t2p);
		if (error)
			return error;
		ctx->f_t2 = t2p;
		mbp = &t2p->t2_tparam;
		mb_init(mbp);
		mb_put_uint16le(mbp, ctx->f_attrmask);
		mb_put_uint16le(mbp, ctx->f_searchCount);
		mb_put_uint16le(mbp, flags);
		mb_put_uint16le(mbp, ctx->f_infolevel);
		mb_put_uint32le(mbp, 0);
		len = ctx->f_lookupNameLen;
		error = smbfs_fullpath(mbp, vcp, ctx->f_dnp, ctx->f_lookupName,
				       &len, ctx->f_sfm_conversion, '\\');
		if (error)
			return error;
	} else	{
		error = smb_t2_alloc(SSTOCP(ctx->f_ssp), SMB_TRANS2_FIND_NEXT2, context, &t2p);
		if (error)
			return error;
		ctx->f_t2 = t2p;
		mbp = &t2p->t2_tparam;
		mb_init(mbp);
		mb_put_mem(mbp, (caddr_t)&ctx->f_Sid, 2, MB_MSYSTEM);
		mb_put_uint16le(mbp, ctx->f_searchCount);
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
	/*
	 * At this point md_cur and md_top have the same value. Now all the md_get 
	 * routines will will check for null, but just to be safe we check here
	 */
	if (mdp->md_cur == NULL) {
		SMBWARNING("Parsing error reading the message\n");
		return EBADRPC;
	}
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
smbfs_smb_findclose2(struct smbfs_fctx *ctx, vfs_context_t context)
{
	struct smb_rq rq, *rqp = &rq;
	struct mbchain *mbp;
	int error;

	error = smb_rq_init(rqp, SSTOCP(ctx->f_ssp), SMB_COM_FIND_CLOSE2, context);
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
smbfs_smb_findclose(struct smbfs_fctx *ctx, vfs_context_t context)
{	
	if (ctx->f_name)
		free(ctx->f_name, M_SMBFSDATA);
	if (ctx->f_t2)
		smb_t2_done(ctx->f_t2);
	if ((ctx->f_flags & SMBFS_RDD_NOCLOSE) == 0)
		smbfs_smb_findclose2(ctx, context);
	if (ctx->f_rname)
		free(ctx->f_rname, M_SMBFSDATA);
	free(ctx, M_SMBFSDATA);
	return 0;
}

static int
smbfs_smb_findnextLM2(struct smbfs_fctx *ctx, vfs_context_t context)
{
	struct mdchain *mdp;
	struct smb_t2rq *t2p;
	char *cp;
	u_int32_t size, next, dattr, resumekey = 0;
	u_int64_t llint;
	int error, cnt, nmlen;
	u_int32_t fxsz, recsz;
	struct timespec ts;
	uint32_t eaSize;

	if (ctx->f_ecnt == 0) {
		if (ctx->f_flags & SMBFS_RDD_EOF)
			return ENOENT;
		nanouptime(&ts);
		error = smbfs_smb_trans2find2(ctx, context);
		if (error)
			return error;
		ctx->f_attr.fa_reqtime = ts;
	}
	t2p = ctx->f_t2;
	mdp = &t2p->t2_rdata;
	switch (ctx->f_infolevel) {
	case SMB_FIND_BOTH_DIRECTORY_INFO:
		md_get_uint32le(mdp, &next);
		md_get_uint32le(mdp, &resumekey); /* file index (resume key) */
		md_get_uint64le(mdp, &llint);	/* creation time */
		if (llint) {
			smb_time_NT2local(llint, &ctx->f_attr.fa_crtime);
		}
		md_get_uint64le(mdp, &llint);
		if (llint) {
			smb_time_NT2local(llint, &ctx->f_attr.fa_atime);
		}
		md_get_uint64le(mdp, &llint);
		if (llint) {
			smb_time_NT2local(llint, &ctx->f_attr.fa_mtime);
		}
		md_get_uint64le(mdp, &llint);
		if (llint) {
			smb_time_NT2local(llint, &ctx->f_attr.fa_chtime);
		}
		md_get_uint64le(mdp, &llint);	/* data size */
		ctx->f_attr.fa_size = llint;
		md_get_uint64le(mdp, &llint);	/* data allocation size */
		ctx->f_attr.fa_data_alloc = llint;
		/* freebsd bug: fa_attr endian bug */
		md_get_uint32le(mdp, &dattr);	/* extended file attributes */
		ctx->f_attr.fa_attr = dattr;
		/*
		* Because of the Steve/Conrad Symlinks we can never be completely
		* sure that we have the corret vnode type if its a file. Since we 
		* don't support Steve/Conrad Symlinks with Darwin we can always count 
		* on the vtype being correct. For directories we always know the 
		* correct information.
		*/
		if ((SSTOVC(ctx->f_ssp)->vc_flags & SMBV_DARWIN) ||
			(ctx->f_attr.fa_attr & SMB_EFA_DIRECTORY)) {
			ctx->f_attr.fa_valid_mask |= FA_VTYPE_VALID;
		}
		ctx->f_attr.fa_vtype = (ctx->f_attr.fa_attr & SMB_FA_DIR) ? VDIR : VREG;
		md_get_uint32le(mdp, &size);	/* name len */
		fxsz = 64; /* size ofinfo up to filename */
		/*
		* Confirmed from MS:
		* When the attribute has the Reparse Point bit set then the EASize
		* contains the reparse tag info. This behavior is consistent for 
		* Full, Both, FullId, or BothId query dir calls.  It will pack the 
		* reparse tag into the EaSize value if ATTRIBUTE_REPARSE_POINT is set.  
		* I verified with local MS Engineers, and they also checking to make 
		* sure the behavior is covered in MS-FSA. 
		*
		* EAs and reparse points cannot both be in a file at the same
		* time. We return different information for each case.
		*/
		ctx->f_attr.fa_valid_mask |= FA_REPARSE_TAG_VALID;
		md_get_uint32le(mdp, &eaSize);	/* extended attributes size */			
		if (ctx->f_attr.fa_attr &  SMB_EFA_REPARSE_POINT) {
			ctx->f_attr.fa_reparse_tag = eaSize;
			if (ctx->f_attr.fa_reparse_tag == IO_REPARSE_TAG_SYMLINK) {
				SMBDEBUG("IO_REPARSE_TAG_SYMLINK\n");
				ctx->f_attr.fa_valid_mask |= FA_VTYPE_VALID;
				ctx->f_attr.fa_vtype = VLNK;
			}
		} else {
			ctx->f_attr.fa_reparse_tag = IO_REPARSE_TAG_RESERVED_ZERO;
		}
		md_get_uint8(mdp, NULL);		/* Skip short name Length */
		md_get_uint8(mdp, NULL);		/* Skip reserved byte */
		/* Skip 8.3 short name, defined to be 12 WCHAR or 24 bytes. */
		md_get_mem(mdp, NULL, 24, MB_MSYSTEM);
		fxsz += 30;
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
			smb_time_NT2local(llint, &ctx->f_attr.fa_chtime);
		
		md_get_uint64le(mdp, &llint);	/* access time */
		if (llint)
			smb_time_NT2local(llint, &ctx->f_attr.fa_atime);
		
		md_get_uint64le(mdp, &llint);	/* write time */
		if (llint)
			smb_time_NT2local(llint, &ctx->f_attr.fa_mtime);
		
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
		ctx->f_attr.fa_valid_mask |= FA_UNIX_MODES_VALID;
		md_get_uint64le(mdp, &llint);	/* Number of hard link */
		ctx->f_attr.fa_nlinks = llint;
		
		md_get_uint64le(mdp, &llint);	/* creation time */
		if (llint)
			smb_time_NT2local(llint, &ctx->f_attr.fa_crtime);
		
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
		nmlen = MIN(size, SMB_MAXFNAMELEN * 2);
	else
		nmlen = MIN(size, SMB_MAXFNAMELEN);
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
		ctx->f_rnameofs < next)) {
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
	return 0;
}

int smbfs_smb_findopen(struct smbnode *dnp, const char *lookupName, size_t lookupNameLen, 
					   vfs_context_t context, struct smbfs_fctx **ctxpp, int wildCardLookup)
{
	struct smb_vc *vcp = SSTOVC(dnp->n_mount->sm_share);
	struct smbfs_fctx *ctx;
	int error = 0;

	ctx = malloc(sizeof(*ctx), M_SMBFSDATA, M_WAITOK);
	if (ctx == NULL)
		return ENOMEM;
	bzero(ctx, sizeof(*ctx));
	if (dnp->n_mount->sm_share) {
		ctx->f_ssp = dnp->n_mount->sm_share;
	}
	ctx->f_dnp = dnp;
	ctx->f_flags |= SMBFS_RDD_FINDFIRST;
	/*
	 * If this is a wildcard lookup then make sure we are not setting the 
	 * UTF_SFM_CONVERSIONS flag. We are either doing a lookup by name or we are 
	 * doing a wildcard lookup using the asterisk. When doing a wildcard lookup 
	 * the  asterisk is legit, so we don't have to convert it. Now once we send
	 * the FindFirst we need to turn the UTF_SFM_CONVERSIONS flag back on, this
	 * is done in smbfs_smb_trans2find2. Also by definition non wildcard lookups
	 * need to be single lookups, so if we are not doing a wildcard lookup then
	 * set the SMBFS_RDD_FINDSINGLE flag.
	 */
	if (!wildCardLookup) {
		ctx->f_sfm_conversion = UTF_SFM_CONVERSIONS;
		ctx->f_flags |= SMBFS_RDD_FINDSINGLE;
		ctx->f_searchCount = 1;
	}
	
	if (UNIX_CAPS(SSTOVC(dnp->n_mount->sm_share)) & UNIX_FIND_FILE_UNIX_INFO2_CAP) {
		/* 
		 * Search count is the clients request for the max number of items to 
		 * be return. We could always request some large number, but lets just
		 * request the max number of entries that will fit in a buffer.
		 *
		 * NOTE: We always make sure vc_txmax is 1k or larger.
		 */
		if (wildCardLookup) {
			ctx->f_searchCount = vcp->vc_txmax / SMB_FIND_FILE_UNIX_INFO2_MIN_LEN;
		}
		ctx->f_infolevel = SMB_FIND_FILE_UNIX_INFO2;
	}
	else {
		/* 
		 * Search count is the clients request for the max number of items to 
		 * be return. We could always request some large number, but lets just
		 * request the max number of entries that will fit in a buffer.
		 *
		 * NOTE: We always make sure vc_txmax is 1k or larger.
		 */
		if (wildCardLookup) {
			ctx->f_searchCount = vcp->vc_txmax / SMB_FIND_BOTH_DIRECTORY_INFO_MIN_LEN;
		}
		ctx->f_infolevel = SMB_FIND_BOTH_DIRECTORY_INFO;
	}
	/* We always default to using the same attribute mask */
	ctx->f_attrmask = SMB_FA_SYSTEM | SMB_FA_HIDDEN | SMB_FA_DIR;
	ctx->f_lookupName = lookupName;
	ctx->f_lookupNameLen = lookupNameLen;
	
	if (SMB_UNICODE_STRINGS(SSTOVC(ctx->f_ssp)))
		ctx->f_name = malloc(SMB_MAXFNAMELEN*2, M_SMBFSDATA, M_WAITOK);
	else
		ctx->f_name = malloc(SMB_MAXFNAMELEN, M_SMBFSDATA, M_WAITOK);
	if (ctx->f_name == NULL)
		error = ENOMEM;
	
	if (error)
		smbfs_smb_findclose(ctx, context);
	else
		*ctxpp = ctx;
	return error;
}

int
smbfs_smb_findnext(struct smbfs_fctx *ctx, vfs_context_t context)
{
	int error;

	if ((ctx->f_flags & SMBFS_RDD_FINDSINGLE) == SMBFS_RDD_FINDSINGLE)
		ctx->f_searchCount = 1; /* Single lookup only ask for one item */

	for (;;) {
		error = smbfs_smb_findnextLM2(ctx, context);
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
	ctx->f_attr.fa_ino = smbfs_getino(ctx->f_dnp, ctx->f_name, ctx->f_nmlen);
	return 0;
}

int smbfs_smb_lookup(struct smbnode *dnp, const char **namep, size_t *nmlenp,
					 struct smbfattr *fap, vfs_context_t context)
{
	struct smb_vc *vcp;
	struct smbfs_fctx *ctx;
	int error = EINVAL;
	const char *name = (namep ? *namep : NULL);
	size_t nmlen = (nmlenp ? *nmlenp : 0);
	u_int64_t DIFF1980TO1601 = 11960035200ULL*10000000ULL;

	DBG_ASSERT(dnp);
	/* This should no longer happen, but just in case (should remove someday). */
	if (dnp == NULL) {
		SMBERROR("The parent node is NULL, shouldn't happen\n");
		return EINVAL;
	}
	vcp = SSTOVC(dnp->n_mount->sm_share);
	if ((dnp->n_ino == 2) && (name == NULL)) {
		bzero(fap, sizeof(*fap));
		/* We keep track of the time the lookup call was requested */
		nanouptime(&fap->fa_reqtime);
		fap->fa_attr = SMB_FA_DIR;
		fap->fa_vtype = VDIR;
		fap->fa_ino = 2;
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
			error = smbfs_smb_query_info(dnp, NULL, 0, fap, context);
			fap->fa_atime = fap->fa_mtime;
			fap->fa_crtime.tv_sec = 0;
		}
		else {
			error = EINVAL;
			
			if (UNIX_CAPS(SSTOVC(dnp->n_mount->sm_share)) & UNIX_QFILEINFO_UNIX_INFO2_CAP)
				error = smbfs_smb_qpathinfo(dnp, fap, context, SMB_QFILEINFO_UNIX_INFO2, NULL, NULL);
			else if (vcp->vc_sopt.sv_caps & SMB_CAP_NT_SMBS)
				error = smbfs_smb_qpathinfo(dnp, fap, context, SMB_QFILEINFO_BASIC_INFO, NULL, NULL);
			/* They don't support SMB_QFILEINFO_BASIC_INFO try the very old stuff */
			if ((error == EINVAL) || (error == ENOTSUP)) {
				error = smbfs_smb_query_info(dnp, NULL, 0, fap, context);
			}
		}
			
		if (fap->fa_mtime.tv_sec == 0)
			smb_time_NT2local(DIFF1980TO1601, &fap->fa_mtime);
		if (fap->fa_crtime.tv_sec == 0)
			smb_time_NT2local(DIFF1980TO1601, &fap->fa_crtime);
		if (fap->fa_atime.tv_sec == 0)
			fap->fa_atime = fap->fa_mtime;
		if (fap->fa_chtime.tv_sec == 0)
			fap->fa_chtime = fap->fa_mtime;
		return error;
	}
	if (nmlen == 1 && name[0] == '.') {
		error = smbfs_smb_lookup(dnp, NULL, NULL, fap, context);
		return error;
	} else if (nmlen == 2 && name[0] == '.' && name[1] == '.') {
		/* 
		 * Remember that error is set to EINVAL. This should never happen, but just in case 
		 * make sure we have a parent, if not return EINVAL 
		 */
		if (dnp->n_parent)
			error = smbfs_smb_lookup(dnp->n_parent, NULL, NULL, fap, context);
		return error;
	}
	bzero(fap, sizeof(*fap));
	/* We keep track of the time the lookup call was requested */
	nanouptime(&fap->fa_reqtime);

	/*
	 * So we now default to using FindFirst, becasue of Dfs. Only FindFirst 
	 * support getting the reparse point tag. If this is a stream node then
	 * use the query info call to do the lookup. The FindFrist code doesn't 
	 * handle stream nodes yet, may want to change that in the future.
	 */
	if ((dnp->n_vnode) && vnode_isnamedstream(dnp->n_vnode)) {
		goto doQueryInfo;
	}
	
	error = smbfs_smb_findopen(dnp, name, nmlen, context, &ctx, FALSE);
	if (error) {
		/* Can't happen we do wait ok */
		return error;
	}
	error = smbfs_smb_findnext(ctx, context);
	if (error == 0) {
		*fap = ctx->f_attr;
		if (name == NULL)
			fap->fa_ino = dnp->n_ino;
		if (namep)
			*namep = (char *)smbfs_name_alloc((u_char *)(ctx->f_name), ctx->f_nmlen);
		if (nmlenp)
			*nmlenp = ctx->f_nmlen;
	}
	smbfs_smb_findclose(ctx, context);
	/*
	 * So if the FindFirst fails with an access error of some kind. If they support
	 * the UNIX extensions we can try SMB_QFILEINFO_UNIX_INFO2. If they support 
	 * the NT CAPS option we can try SMB_QFILEINFO_ALL_INFO. Both Trans2 Query
	 * Info levels return the same information as FindFirst and will work 
	 * with Drop boxes.
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
	 * If they support UNIX_QFILEINFO_UNIX_INFO2_CAP then they must support UNIX_INFO2.
	 * Hopefully everyone does this correctly. Remember the SMB_QFILEINFO_UNIX_INFO2 call does
	 * not return the name. So if they are asking for the name then just fail.
	 */
	if ((error != EACCES) || (error != EPERM))
		return error;
	
doQueryInfo:
	if (UNIX_CAPS(SSTOVC(dnp->n_mount->sm_share)) & UNIX_QFILEINFO_UNIX_INFO2_CAP) {
		if (namep == NULL)
			error = smbfs_smb_qpathinfo(dnp, fap, context, SMB_QFILEINFO_UNIX_INFO2, namep, nmlenp);
		else {
			/* If they are requesting the name, return error nothing else we can do here. */
			error = EACCES;
		}
	} else if (vcp->vc_sopt.sv_caps & SMB_CAP_NT_SMBS) {
		error = smbfs_smb_qpathinfo(dnp, fap, context, SMB_QFILEINFO_ALL_INFO, namep, nmlenp);
	}
	return error;
}

/*
 * Close the network search, reset the offset count and if there is 
 * a next entry remove it.
 */
void smbfs_closedirlookup(struct smbnode *np, vfs_context_t context)
{
	if (np->d_fctx)
		smbfs_smb_findclose(np->d_fctx, context);
	np->d_fctx = NULL;
	np->d_offset = 0;
	if (np->d_nextEntry)
		FREE(np->d_nextEntry, M_TEMP);
	np->d_nextEntry = NULL;
	np->d_nextEntryLen = 0;
}

static int
smbfs_smb_getsec_int(struct smb_share *ssp, u_int16_t fid,
		     vfs_context_t context, u_int32_t selector,
		     struct ntsecdesc **res, u_int32_t *reslen)
{
	struct smb_ntrq *ntp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	int error;
	size_t len;

	error = smb_nt_alloc(SSTOCP(ssp), NT_TRANSACT_QUERY_SECURITY_DESC, context, &ntp);
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
		    md_get_uint32le(mdp, reslen);
		goto done;
	 }
	else md_get_uint32le(mdp, reslen);

	mdp = &ntp->nt_rdata;
	if (mdp->md_top) {	/* XXX md_cur safer than md_top */
		len = m_fixhdr(mdp->md_top);
		if (len != (size_t)*reslen)
			SMBWARNING("Sent us %ld but said they sent us %d for fid = %d\n", 
					   len, *reslen, letohs(fid));
		/*
		 * The following "if (len < *reslen)" handles a Windows bug
		 * observed when the underlying filesystem is FAT32.  In that
		 * case a 32 byte security descriptor comes back (S-1-1-0, ie
		 * "Everyone") but the Parameter Block claims 44 is the length
		 * of the security descriptor.  (The Data Block length
		 * claimed is 32.  This server bug was reported against NT
		 * first and I've personally observed it with W2K.
		 */
		
		if (len < (size_t)*reslen)
			*reslen = (u_int32_t)len;	/* Believe what we got instead of what they told us */
		else if (len > (size_t)*reslen)
			len = *reslen;	 /* ignore any extra data */
		/* 
		 * All the calling routines expect to have sizeof(struct ntsecdesc). The
		 * len is the amount of data we have received and *reslen is what they
		 * claim they sent. Up above we make sure that *reslen and len are the
		 * same. So all we need to do here is make sure that len is not less than
		 * the size of our ntsecdesc structure.
		 */
		if (len >= sizeof(struct ntsecdesc)) {
			MALLOC(*res, struct ntsecdesc *, len, M_TEMP, M_WAITOK);
			md_get_mem(mdp, (caddr_t)*res, len, MB_MSYSTEM);
		} else {
			SMBERROR("len %ld < ntsecdesc %ld fid 0x%x\n", len, 
					 sizeof(struct ntsecdesc), letohs(fid));
			error = EBADRPC; 
		}	
	} else {
		SMBERROR("null md_top? fid 0x%x\n", letohs(fid));
		error = EBADRPC; 		
	}
done:
	smb_nt_done(ntp);
	return (error);
}

int
smbfs_smb_getsec(struct smb_share *ssp, u_int16_t fid, vfs_context_t context, 
				 u_int32_t selector, struct ntsecdesc **res, size_t *rt_len)
{
	int error;
	u_int32_t seclen;
	
	/*
	 * If the buffer size is to small we could end up making two request. Using 
	 * the max transmit buffer should limit this from happening to often.
	 */
	seclen = SSTOVC(ssp)->vc_txmax;
	error = smbfs_smb_getsec_int(ssp, fid, context, selector, res, &seclen);
	if (error && (seclen > SSTOVC(ssp)->vc_txmax))
		error = smbfs_smb_getsec_int(ssp, fid, context, selector, res, &seclen);
	/* Return the the size we ended up getting */
	if (error == 0)
		*rt_len = seclen;
	return (error);
}

int
smbfs_smb_setsec(struct smb_share *ssp, u_int16_t fid, vfs_context_t context,
	u_int32_t selector, u_int16_t flags, struct ntsid *owner,
	struct ntsid *group, struct ntacl *sacl, struct ntacl *dacl)
{
	struct smb_ntrq *ntp;
	struct mbchain *mbp;
	int error;
	u_int32_t off;
	struct ntsecdesc ntsd;

	error = smb_nt_alloc(SSTOCP(ssp), NT_TRANSACT_SET_SECURITY_DESC, 
						 context, &ntp);
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
	ntsd.sd_revision = 0x01;	/* Should we make this a define? */
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
	off = (u_int32_t)sizeof(ntsd);
	if (owner) {
		ntsd.sd_owneroff = htolel(off);
		off += (u_int32_t)sidlen(owner);
	}
	if (group) {
		ntsd.sd_groupoff = htolel(off);
		off += (u_int32_t)sidlen(group);
	}
	if (sacl) {
		flags |= SD_SACL_PRESENT;
		ntsd.sd_sacloff = htolel(off);
		off += acllen(sacl);
	}
	if (dacl) {
		flags |= SD_DACL_PRESENT;
		ntsd.sd_dacloff = htolel(off);
	}
	
	ntsd.sd_flags = letohs(flags);
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

int smbfs_spotlight(struct smbnode *np, vfs_context_t context, void *idata, void *odata, size_t isize, size_t *osize)
{
	struct smb_share *ssp = np->n_mount->sm_share;
	struct smb_vc *vcp = SSTOVC(ssp);
	struct smb_t2rq *t2p;
	int error;
	struct mbchain *mbp;
	struct mdchain *mdp;

	if (isize > vcp->vc_txmax) {
		SMBERROR("smbfs_spotlight: isize %d > allowed %d\n", isize, vcp->vc_txmax);
		return (EINVAL);
	}

	error = smb_t2_alloc(SSTOCP(ssp), SMB_TRANS2_QUERY_PATH_INFORMATION, context, &t2p);
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
	/*
	 * At this point md_cur and md_top have the same value. Now all the md_get 
	 * routines will will check for null, but just to be safe we check here
	 */
	if (mdp->md_top == NULL) {
		SMBWARNING("Parsing error reading the message\n");
		error = EBADRPC;
		goto out;
	}
	*osize = (mbuf_pkthdr_len(mdp->md_top) > *osize) ? *osize : mbuf_pkthdr_len(mdp->md_top);
	/* How to we want to copy the data in? USER, SYSTEM, UIO? */
	error = md_get_mem(mdp, odata, *osize, MB_MSYSTEM);		

out:
	smb_t2_done(t2p);
	return error;
}
#endif // USE_SIDEBAND_CHANNEL_RPC

