/*
 * Copyright (c) 2000-2001 Boris Popov
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
#include <netsmb/smb_2.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_rq_2.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_conn_2.h>

#include <smbfs/smbfs.h>
#include <smbfs/smbfs_node.h>
#include <smbfs/smbfs_subr.h>
#include <smbfs/smbfs_subr_2.h>
#include <smbfs/smbfs_lockf.h>
#include <netsmb/smb_converter.h>
#include <smbfs/smbfs_security.h>
#include <smbclient/ntstatus.h>
#include <libkern/crypto/md5.h>

#define SMBFS_DELETE_PREFIX	".smbdeleteAAA"
#define MAKE_DELETED_NAME(NAME, SIZE, FID) \
(void) snprintf((NAME), SIZE, "%s%llx", SMBFS_DELETE_PREFIX, (FID))

/*
 * Lack of inode numbers leads us to the problem of generating them.
 * Partially this problem can be solved by having a dir/file cache
 * with inode numbers generated from the incremented by one counter.
 * However this way will require too much kernel memory, gives all
 * sorts of locking and consistency problems, not to mention counter overflows.
 * So, I'm decided to use a hash function to generate pseudo random (and unique)
 * inode numbers.
 *
 * SMB 2/3 - if server supports File IDs, then use inode number from the server
 * and never call this function.
 * SMB 1 and SMB 2/3 servers that do not support File IDs still hashes the
 * name and thus still calls this function.
 */
uint64_t
smbfs_getino(struct smbnode *dnp, const char *name, size_t nmlen)
{
	uint64_t ino;

	ino = dnp->n_ino + smbfs_hash(NULL, 0, name, nmlen);
	if (ino <= SMBFS_ROOT_INO)
		ino += 3;
	return ino;
}

/*
 * The calling routine must hold a reference on the share
 *
 * [MS-CIFS]
 * This command is used to explicitly lock and/or unlock a contiguous range of 
 * bytes in a regular file. More than one non-overlapping byte range MAY be 
 * locked and/or unlocked on an open file. Locks prevent attempts to lock, read, 
 * or write the locked portion of the file by other processes using a separate 
 * file handle (FID). Any process using the same FID specified in the request 
 * that obtained the lock has access to the locked bytes.
 *
 * So we handle the pid issue at our level, curently that code is broken and 
 * needs to be fixed. We should fix that as part of <rdar://problem/7946972>.
 * We need to hold on to the local users pid and not the network user pid in 
 * the byterange locking list.
 *
 * Since we always use the fid that open the file to take the lock the network
 * code should always work, even if we change the network pid.
 */
int
smb1fs_smb_lock(struct smb_share *share, int op, SMBFID fid, uint32_t pid,
			   off_t start, uint64_t len, uint32_t timo, vfs_context_t context)
{
#pragma unused(pid)
	struct smb_rq rq, *rqp = &rq;
	struct mbchain *mbp;
	u_char ltype = 0;
	int error;
    uint16_t smb1_fid = (uint16_t) fid; /* cast to SMB 1 fid */

	if (op == SMB_LOCK_SHARED)
		ltype |= SMB_LOCKING_ANDX_SHARED_LOCK;
		/* Do they support large offsets */
	if (SESSION_CAPS(SS_TO_SESSION(share)) & SMB_CAP_LARGE_FILES)
		ltype |= SMB_LOCKING_ANDX_LARGE_FILES;
	error = smb_rq_init(rqp, SSTOCP(share), SMB_COM_LOCKING_ANDX, 0, context);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint8(mbp, 0xff);	/* secondary command */
	mb_put_uint8(mbp, 0);		/* MBZ */
	mb_put_uint16le(mbp, 0);
	mb_put_mem(mbp, (caddr_t)&smb1_fid, sizeof(smb1_fid), MB_MSYSTEM);
	mb_put_uint8(mbp, ltype);	/* locktype */
	mb_put_uint8(mbp, 0);		/* oplocklevel - 0 seems is NO_OPLOCK */
	mb_put_uint32le(mbp, timo);	/* 0 nowait, -1 infinite wait */
	mb_put_uint16le(mbp, op == SMB_LOCK_RELEASE ? 1 : 0);
	mb_put_uint16le(mbp, op == SMB_LOCK_RELEASE ? 0 : 1);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	/* Always keep it in sync with the pid in the smb header */
	mb_put_uint16le(mbp, rqp->sr_pidLow);
	if (ltype & SMB_LOCKING_ANDX_LARGE_FILES) {
		mb_put_uint16le(mbp, 0); /* pad */
		mb_put_uint32le(mbp, (uint32_t)(start >> 32)); /* OffsetHigh */
		mb_put_uint32le(mbp, (uint32_t)(start & 0xffffffff)); /* OffsetLow */
		mb_put_uint32le(mbp, (uint32_t)(len >> 32)); /* LengthHigh */
		mb_put_uint32le(mbp, (uint32_t)(len & 0xffffffff)); /* LengthLow */
	} else {
		mb_put_uint32le(mbp, (uint32_t)(start & 0xffffffff));
		mb_put_uint32le(mbp, (uint32_t)(len & 0xffffffff));
	}
	smb_rq_bend(rqp);
	error = smb_rq_simple(rqp);
	/*
	 * This may seem strange, but both Windows and Samba do the following:
	 * 
	 * Lock a region, try to lock it again you get STATUS_LOCK_NOT_GRANTED,
	 * try to lock it a third time you get STATUS_FILE_LOCK_CONFLICT. Seems
	 * the first lock error is always STATUS_LOCK_NOT_GRANTED and the second
	 * time you get a STATUS_FILE_LOCK_CONFLICT.
	 *
	 * For IO they always return STATUS_FILE_LOCK_CONFLICT, which we convert 
	 * to EIO, becasue there are multiple IO routines and only one lock routine. 
	 * So we want this to be EACCES in the lock cases. So we need to convert 
	 * that error here.
	 *
	 */
	if ((error == EIO) && (rqp->sr_rpflags2 & SMB_FLAGS2_ERR_STATUS)) {
		if (rqp->sr_ntstatus == STATUS_FILE_LOCK_CONFLICT) 
			error = EACCES;
	}
	smb_rq_done(rqp);
	return error;
}

/*
 * The calling routine must hold a reference on the share
 */
int
smb1fs_smb_qpathinfo(struct smb_share *share, 
                     struct smbnode *np,
                     struct smbfattr *fap, 
                     short infolevel,
                     const char **namep, 
                     size_t *nmlenp, 
                     vfs_context_t context)
{
	struct smb_t2rq *t2p;
	int error;
	struct mbchain *mbp;
	struct mdchain *mdp;
	uint64_t llint;
	uint32_t size, dattr, eaSize;
	const char *name = (namep ? *namep : NULL);
	size_t nmlen = (nmlenp ? *nmlenp : 0);
    size_t ntwrkname_allocsize = 0, filename_allocsize = 0, sname_allocsize = 0, name_allocsize = 0;
	char *ntwrkname = NULL;
	char *filename;
	uint8_t sep = '\\';
    char *snamep = NULL;

	error = smb_t2_alloc(SSTOCP(share), SMB_TRANS2_QUERY_PATH_INFORMATION, context, &t2p);
	if (error)
		return error;
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	mb_put_uint16le(mbp, infolevel);
	mb_put_uint32le(mbp, 0);
	/* mb_put_uint8(mbp, SMB_DT_ASCII); specs are wrong */
	if ((np->n_vnode) && vnode_isnamedstream(np->n_vnode)) {
		DBG_ASSERT((namep == NULL));
        
        lck_rw_lock_shared(&np->n_name_rwlock);
        snamep = smb_strndup(np->n_sname, np->n_snmlen, &sname_allocsize);
        name = snamep;
        lck_rw_unlock_shared(&np->n_name_rwlock);
        
		nmlen = np->n_snmlen;
		sep = ':';
	}
	error = smbfs_fullpath(mbp, np, name, &nmlen, UTF_SFM_CONVERSIONS, 
						   SMB_UNICODE_STRINGS(SS_TO_SESSION(share)), sep);
	
	if (error) {
		smb_t2_done(t2p);
		goto done;
	}
	t2p->t2_maxpcount = 2;
	t2p->t2_maxdcount = SS_TO_SESSION(share)->session_txmax;
	error = smb_t2_request(t2p);
	if (error) {
		smb_t2_done(t2p);
		goto done;
	}
	mdp = &t2p->t2_rdata;
	/*
	 * At this point md_cur and md_top have the same value. Now all the md_get 
	 * routines will will check for null, but just to be safe we check here.
	 */
	if (mdp->md_cur == NULL) {
		SMBWARNING("Parsing error reading the message\n");
		smb_t2_done(t2p);
		error = EBADRPC;
        goto done;
	}
	switch (infolevel) {
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
		 * sure that we have the correct vnode type if its a file. For
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

        /*
		 * We don't care about the name, so we can skip getting it.
		 *
		 * NOTE: When accessing the root node the name may not be what you would
		 * expect. Windows will return a back slash if the item being shared is
		 * a drive and in all other cases the name of the directory being shared.
		 * We never ask for the name in the root node case so this should never
		 * be an issue.
		 */
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
        m_fixhdr(mdp->md_top);
		if (mbuf_get_chain_len(mdp->md_top) > (size+72+4)) {
			SMBERROR("SMB_QFILEINFO_ALL_INFO: wrong size %ld\n", 
					mbuf_get_chain_len((mbuf_t)mdp->md_top));
		}
	
		nmlen = size;

		/* Since this is a full path only check SMB_MAXFNAMELEN length 
		 * until we get the component. We just allocate what we need
		 * need here. 
		 */
        ntwrkname_allocsize = nmlen;
        SMB_MALLOC_DATA(ntwrkname, ntwrkname_allocsize, Z_WAITOK);
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
		if (SMB_UNICODE_STRINGS(SS_TO_SESSION(share))) {
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
		filename = smbfs_ntwrkname_tolocal((const char *)filename, &nmlen, &filename_allocsize,
										   SMB_UNICODE_STRINGS(SS_TO_SESSION(share)));
		/* Done with the network buffer so free it */
        SMB_FREE_DATA(ntwrkname, ntwrkname_allocsize);
		/* Now reasign it so we free the correct buffer */	
		ntwrkname = filename;
        ntwrkname_allocsize = filename_allocsize;

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
		*namep = smb_strndup(filename, nmlen, &name_allocsize);
		if (nmlenp)	/* Return the name length */
			*nmlenp = nmlen;
		if (*namep && nmlenp)	/* SMB 1 only. Create the inode number */
			fap->fa_ino = smbfs_getino(np, *namep, *nmlenp);
bad:
		/* Free the buffer that holds the name from the network */
        SMB_FREE_DATA(ntwrkname, ntwrkname_allocsize);
		break;
	case SMB_QFILEINFO_UNIX_INFO2:
			
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
				fap->fa_attr |= SMB_EFA_HIDDEN;
			else
				fap->fa_attr &= ~SMB_EFA_HIDDEN;
		}
		if (fap->fa_flags_mask & EXT_IMMUTABLE) {
			if (dattr & EXT_IMMUTABLE)
				fap->fa_attr |= SMB_EFA_RDONLY;
			else
				fap->fa_attr &= ~SMB_EFA_RDONLY;
		}
		if (fap->fa_flags_mask & SMB_EFA_ARCHIVE) {
			if (dattr & EXT_DO_NOT_BACKUP)
				fap->fa_attr &= ~SMB_EFA_ARCHIVE;
			else
				fap->fa_attr |= SMB_EFA_ARCHIVE;
		}
		fap->fa_unix = TRUE;
        if (namep == NULL) {
            fap->fa_ino = np->n_ino;
        }
        else {
            if ((*namep) && nmlenp)  {
                /* np is actually the parent dnp */
                fap->fa_ino = smbfs_getino(np, *namep, *nmlenp);
            }
            else {
                /* Should not ever happen */
                SMBERROR("Missing namep or nmlenp, so inode left at 0\n");
            }
        }
		break;
	default:
		SMBERROR("unexpected info level %d\n", infolevel);
		error = EINVAL;
		break;
	}
	smb_t2_done(t2p);

done:
    if (snamep) {
        SMB_FREE_DATA(snamep, sname_allocsize);
    }
    
    return error;
}

int
smbfs_smb_undollardata(const char *fname, char *name, size_t *nmlen,
                       uint32_t *is_data)
{
    char *cp;
    size_t len = sizeof(SMB_DATASTREAM) - 1;

    /*
     * Example stream names
     * ::$DATA
     * :AFP_AfpInfo:$DATA
     * :AFP_Resource:$DATA
     */
    
    *is_data = 0;

    /* Sanity check for a stream name */
    if (!name) {
        goto bad;
    }

    /* Has to be "::$DATA" at a minimum length */
    if (*nmlen < len + 1) {
        goto bad;
    }
    
    /* Has to have a leading colon */
    if (*name != ':') {
        goto bad;
    }
    
    /* Point to 2nd colon */
    cp =  &name[*nmlen - len];

    /* Has to end with $DATA */
    if (bcmp(cp, SMB_DATASTREAM, len)) {
        goto bad;
    }
    
    /* Merely the data fork? */
    if (*nmlen == (len + 1)) {
        /* Skip returning it. */
        *is_data = 1;
        return (0);
    }

	/* We can not return more than 128 bytes */
    if ((*nmlen - len) > (XATTR_MAXNAMELEN + 1)) {
        goto bad;
    }
    
    /*
     * Un-count a colon and the $DATA, then the
     * 2nd colon is replaced by a terminating null.
     */
    *nmlen -= len;
    *cp = '\0';

    /* Skip protected system attrs */
    if ((*nmlen >= 17) && (xattr_protected(name + 1))) {
        return (0);
    }

    /* 
     * Return the stream name. Examples:
     * :AFP_AfpInfo
     * :AFP_Resource
     */
    return (1);
    
bad:
    /*
     * If the name exist then we malloc it so we can print it
     * out. So just make sure it exist before printing.
     */
    SMBWARNING("file \"%s\" has bad stream \"%s\"\n", fname, (name) ? name : "");
    return (0);
}

/*
 * smbfs_smb_markfordelete
 *
 * We have an open file that they want to delete. This call will tell the 
 * server to delete the file when the last close happens. Currenly we know that 
 * XP, Windows 2000 and Windows 2003 support this call. SAMBA does support the
 * call, but currently has a bug that prevents it from working.
 *
 * The calling routine must hold a reference on the share
 *
 */
int
smb1fs_smb_markfordelete(struct smb_share *share, SMBFID fid, 
                         vfs_context_t context)
{
	struct smb_t2rq *t2p;
	struct mbchain *mbp;
	int error;
    uint16_t smb1_fid = (uint16_t) fid; /* cast to SMB 1 fid */
		
	error = smb_t2_alloc(SSTOCP(share), SMB_TRANS2_SET_FILE_INFORMATION,
                         context, &t2p);
	if (error) {
		return error;
	}
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
    /* SMB 1 fid in volatile */
	mb_put_mem(mbp, (caddr_t)&smb1_fid, sizeof(smb1_fid), MB_MSYSTEM);
	if (SESSION_CAPS(SS_TO_SESSION(share)) & SMB_CAP_INFOLEVEL_PASSTHRU)
		mb_put_uint16le(mbp, SMB_SFILEINFO_DISPOSITION_INFORMATION);
	else
		mb_put_uint16le(mbp, SMB_SFILEINFO_DISPOSITION_INFO);
	mb_put_uint16le(mbp, 0);
	mbp = &t2p->t2_tdata;
	mb_init(mbp);
	mb_put_uint8(mbp, 1);
	t2p->t2_maxpcount = 2;
	t2p->t2_maxdcount = 0;
	error = smb_t2_request(t2p);
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
	
    SMB_MALLOC_DATA(wbuf, filelen, Z_WAITOK);
    
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
 *
 * The calling routine must hold a reference on the share
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
    size_t ntwrk_allocsize = 0;

    ntwrk_allocsize = ntwrkpathlen;
    SMB_MALLOC_DATA(ntwrkpath, ntwrk_allocsize, Z_WAITOK_ZERO);

	/* smb_convert_path_to_network sets the precomosed flag */
	error = smb_convert_path_to_network(target, targetlen, ntwrkpath, 
										&ntwrkpathlen, '/', NO_SFM_CONVERSIONS, 
										SMB_UNICODE_STRINGS(SS_TO_SESSION(share)));
	if (! error) {
		error = smb_t2_alloc(SSTOCP(share), SMB_TRANS2_SET_PATH_INFORMATION,
							 context, &t2p);
	}
	if (error) {
		goto done;
	}
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	mb_put_uint16le(mbp, SMB_SFILEINFO_UNIX_LINK);
	mb_put_uint32le(mbp, 0);		/* MBZ */
	error = smbfs_fullpath(mbp, dnp, name, &nmlen, UTF_SFM_CONVERSIONS, 
						   SMB_UNICODE_STRINGS(SS_TO_SESSION(share)), '\\');
	if (error) {
		goto done;
	}
	mbp = &t2p->t2_tdata;
	mb_init(mbp);
	mb_put_mem(mbp, (caddr_t)(ntwrkpath), ntwrkpathlen, MB_MSYSTEM);
	t2p->t2_maxpcount = 2;
	t2p->t2_maxdcount = SS_TO_SESSION(share)->session_txmax;
	error = smb_t2_request(t2p);
	
done:
    SMB_FREE_DATA(ntwrkpath, ntwrk_allocsize);
	if (t2p) {
		smb_t2_done(t2p);
	}
	return error;
	
}

static int
smbfs_smb_fsctl(struct smb_share *share,  uint32_t fsctl, SMBFID fid, 
				uint32_t datacnt, struct mbchain *mbp, struct mdchain *mdp,
				Boolean * moreDataRequired, vfs_context_t context)
{
	struct smb_ntrq *ntp = NULL;
    int error;
    uint16_t smb1_fid = (uint16_t) fid; /* cast to SMB 1 fid */
	
	error = smb_nt_alloc(SSTOCP(share), NT_TRANSACT_IOCTL, context, &ntp);
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
	mb_put_uint16le(&ntp->nt_tsetup, smb1_fid);
	
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
	
	if (np == NULL) {
		SMBERROR("np is NULL \n");
		return;
	}
	
	/* If previous cached target, then free it */
	if (np->n_symlink_target != NULL) {
        SMB_FREE_DATA(np->n_symlink_target, np->n_symlink_target_allocsize);
    }
	
	/* Cache the new target string */
	np->n_symlink_target_len = 0;
	np->n_symlink_target = smb_strndup(target, targetlen, &np->n_symlink_target_allocsize);
	if (np->n_symlink_target) {
		np->n_symlink_target_len = targetlen;
		nanouptime(&ts);
		np->n_symlink_cache_timer = ts.tv_sec;
	}
	
	/* 
	 * If its a reparse point, the data size reported from the server
	 * is zero, but OS X would rather have the size of the target string
	 */
	if ((np->n_dosattr & SMB_EFA_REPARSE_POINT) &&
		(np->n_reparse_tag == IO_REPARSE_TAG_SYMLINK))  {
		np->n_size = targetlen;
		np->n_data_alloc = roundup(targetlen,
								   np->n_mount->sm_statfsbuf.f_bsize);
	}
}

/*
 * Create a symbolic link since the server supports the UNIX extensions. This 
 * allows us to access and share symbolic links with AFP and NFS.
 *
 * The calling routine must hold a reference on the share
 */
int 
smbfs_smb_create_unix_symlink(struct smb_share *share, struct smbnode *dnp, 
							  const char *in_name, size_t in_nmlen, char *target, 
							  size_t targetlen, struct smbfattr *fap,
							  vfs_context_t context)
{
	const char *name = in_name;
	size_t nmlen = in_nmlen;
    size_t name_allocsize = 0;
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
	error = smbfs_lookup(share, dnp, &name, &nmlen, &name_allocsize, fap, context);
	if (error) {
		struct smbmount *smp = dnp->n_mount;

		fap->fa_attr = SMB_EFA_NORMAL;
		fap->fa_size = targetlen;

        lck_mtx_lock(&smp->sm_statfslock);
        fap->fa_data_alloc = roundup(fap->fa_size, smp->sm_statfsbuf.f_bsize);
        lck_mtx_unlock(&smp->sm_statfslock);

        nanotime(&fap->fa_crtime);	/* Need current date/time, so use nanotime */
		fap->fa_atime = fap->fa_crtime;
		fap->fa_chtime = fap->fa_crtime;
		fap->fa_mtime = fap->fa_crtime;
		fap->fa_ino = smbfs_getino(dnp, in_name, in_nmlen); /* SMB 1 only */
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
        SMB_FREE_DATA(name, name_allocsize);
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
 * The calling routine must hold a reference on the share
 */
int 
smbfs_smb_create_windows_symlink(struct smb_share *share, struct smbnode *dnp, 
                                 const char *name, size_t nmlen,
                                 char *target, size_t targetlen,
                                 struct smbfattr *fap, vfs_context_t context)
{
    uint32_t wlen = 0;
    uio_t uio = NULL;
    char *wdata = NULL;
    int error;
    SMBFID fid = 0;
    uint64_t create_flags = SMB2_CREATE_DO_CREATE | SMB2_CREATE_GET_MAX_ACCESS;
    uint32_t allow_compression = 0; /* Dont allow compression for symlinks */

    wdata = smbfs_create_windows_symlink_data(target, targetlen, &wlen);
    if (!wdata) {
        error = ENOMEM;
        goto done;
    }
    
    uio = uio_create(1, 0, UIO_SYSSPACE, UIO_WRITE);
    if (uio == NULL) {
        error = ENOMEM;
        goto done;
    }

    uio_addiov(uio, CAST_USER_ADDR_T(wdata), wlen);
    
    if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
        /* SMB 2/3  - compound Create/Write/Close */
        error = smb2fs_smb_cmpd_create_write(share, dnp,
                                             name, nmlen,
                                             NULL, 0,
                                             SMB2_FILE_WRITE_DATA, fap,
                                             create_flags, uio,
                                             NULL, 0,
                                             context);
    }
    else {
        /* SMB 1 */
        error = smbfs_smb_create(share, dnp, name, nmlen, SMB2_FILE_WRITE_DATA,
                                 &fid, FILE_CREATE, 0, fap, context);
        if (error) {
            goto done;
        }
        
        error = smb_smb_write(share, fid, uio, 0,
                              &allow_compression, context);
        
        (void) smbfs_smb_close(share, fid, context);
    }
    
    if (!error)	{
        /* We just changed the size of the file */
        fap->fa_size = wlen;
    }

done:
    if (uio != NULL) {
        uio_free(uio);
    }

    if (wdata) {
        SMB_FREE_DATA(wdata, wlen);
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
 * The calling routine must hold a reference on the share
 */
int 
smb1fs_smb_create_reparse_symlink(struct smb_share *share, struct smbnode *dnp,
                                  const char *name, size_t nmlen, char *target, 
                                  size_t targetlen, struct smbfattr *fap,
                                  vfs_context_t context)
{
	struct smbmount *smp = dnp->n_mount;
	int error;
    SMBFID fid = 0;
	struct mbchain	mbp;
	size_t path_len, path_allocsize = 0;
	char *path;
	uint16_t reparseLen;
	uint16_t SubstituteNameOffset, SubstituteNameLength;
	uint16_t PrintNameOffset, PrintNameLength;
    int need_close = 0;
    uint32_t rights = SMB2_FILE_WRITE_DATA |  SMB2_FILE_WRITE_ATTRIBUTES | SMB2_DELETE;
	
	error = smbfs_smb_ntcreatex(share, dnp, 
                                rights, NTCREATEX_SHARE_ACCESS_ALL, VLNK,
                                &fid, name, nmlen,
                                FILE_CREATE, FALSE, fap,
                                TRUE, NULL,
                                NULL, NULL,
                                context);
	if (error) {
		goto done;		
	}
    need_close = 1;
    
	path_len = (targetlen * 2) + 2;	/* Start with the max possible size */
    path_allocsize = path_len;
    SMB_MALLOC_DATA(path, path_allocsize, Z_WAITOK_ZERO);
	if (path == NULL) {
		error = ENOMEM;
		goto done;		
	}
	/* Convert it to a network style path */
	error = smb_convert_path_to_network(target, targetlen, path, &path_len, 
										'\\', SMB_UTF_SFM_CONVERSIONS, 
										SMB_UNICODE_STRINGS(SS_TO_SESSION(share)));
	if (error) {
        SMB_FREE_DATA(path, path_allocsize);
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
    SMB_FREE_DATA(path, path_allocsize);
	error = smbfs_smb_fsctl(share, FSCTL_SET_REPARSE_POINT, fid, 0, &mbp, 
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
	if ((error == EACCES) && !(SS_TO_SESSION(share)->session_flags & SMBV_DARWIN)) {
		smp->sm_flags &= ~MNT_SUPPORTS_REPARSE_SYMLINKS;
	} 
	
	if (error) {
		/* Failed to create the symlink, remove the file on close */
		(void)smbfs_smb_markfordelete(share, fid, context);
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
	if (need_close) {
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
 * Get the reparse tag and if its a symlink get the target and target size.
 *
 * The calling routine must hold a reference on the share
 *
 */
int 
smbfs_smb_get_reparse_tag(struct smb_share *share, SMBFID fid, 
                          uint32_t *reparseTag, char **outTarget,
                          size_t *outTargetlen, size_t *outTargetAllocSize, vfs_context_t context)
{
	int error;
	Boolean moreDataRequired = FALSE;
	uint32_t rdatacnt = SS_TO_SESSION(share)->session_txmax;
	struct mdchain mdp;
	uint16_t reparseLen = 0;
	uint16_t SubstituteNameOffset = 0;
	uint16_t SubstituteNameLength = 0;
	uint16_t PrintNameOffset = 0;
	uint16_t PrintNameLength = 0;
	uint32_t Flags = 0;
	char *ntwrkname = NULL;
	char *target = NULL;
	size_t targetlen = 0;
    size_t target_allocsize = 0;
	
	memset(&mdp, 0, sizeof(mdp));
	
	error = smbfs_smb_fsctl(share, FSCTL_GET_REPARSE_POINT, fid, rdatacnt, NULL, 
							&mdp, &moreDataRequired, context);
	if (!error && !mdp.md_top) {
		error = ENOENT;
	}
	if (error) {
		goto done;
	}
	
	md_get_uint32le(&mdp, reparseTag);
	/* Should have checked to make sure the node reparse tag matches */
	if (*reparseTag != IO_REPARSE_TAG_SYMLINK) {
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
	if ((SubstituteNameLength == 0) || (SubstituteNameLength > SS_TO_SESSION(share)->session_txmax)) {
		error = ENOMEM;
		SMBSYMDEBUG("SubstituteNameLength too large or zero %d \n", SubstituteNameLength);
		md_done(&mdp);
		goto done;
	}
	
	if (SubstituteNameOffset) {
		md_get_mem(&mdp, NULL, SubstituteNameOffset, MB_MSYSTEM);
	}
	
    SMB_MALLOC_DATA(ntwrkname, SubstituteNameLength, Z_WAITOK_ZERO);
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
    target_allocsize = targetlen;
    SMB_MALLOC_DATA(target, target_allocsize, Z_WAITOK_ZERO);
	if (target == NULL) {
		error = ENOMEM;
	} else {
		error = smb_convert_network_to_path(ntwrkname, SubstituteNameLength, target, 
											&targetlen, '\\', UTF_SFM_CONVERSIONS, 
											SMB_UNICODE_STRINGS(SS_TO_SESSION(share)));
	}
	if (!error) {
		*outTarget = target;
		*outTargetlen = targetlen;
        *outTargetAllocSize = target_allocsize;
		target = NULL;
	}
	
done:
    if (ntwrkname) {
        SMB_FREE_DATA(ntwrkname, SubstituteNameLength);
    }
    if (target) {
        SMB_FREE_DATA(target, target_allocsize);
    }
	return error;
	
}

/*
 * The symbolic link is stored in a reparse point, support by some windows servers
 * and Darwin servers.
 *
 * The calling routine must hold a reference on the share
 *
 */
int 
smb1fs_smb_reparse_read_symlink(struct smb_share *share, struct smbnode *np,
                                struct uio *uiop, vfs_context_t context)
{
	int error;
	uint32_t reparseTag = 0;
    SMBFID fid = 0;
	char *target = NULL;
	size_t targetlen = 0, target_allocsize = 0;
    int need_close = 0;
	
	error = smbfs_tmpopen(share, np, 
                          SMB2_FILE_READ_DATA | SMB2_FILE_READ_ATTRIBUTES, 
                          &fid, context);
	if (error) {
		goto done;
	}
    need_close = 1;
    
	error = smbfs_smb_get_reparse_tag(share, fid, &reparseTag, &target, 
                                      &targetlen, &target_allocsize, context);
	if (!error && (reparseTag != IO_REPARSE_TAG_SYMLINK)) {
		error = ENOENT;
	} else if (!error && (target == NULL)) {
		error = EINVAL;
	}
	if (error) {
		goto done;
	}
    
	/* smbfs_update_symlink_cache will deal with the null pointer */
    SMBSYMDEBUG_LOCK(np, "%s --> %s\n", np->n_name, target);
    
	smbfs_update_symlink_cache(np, target, targetlen);
	if (uiop != NULL) {
		error = uiomove(target, (int)targetlen, uiop);
	}
	
done:
    SMB_FREE_DATA(target, target_allocsize);
    
	if (need_close == 1) {
		(void)smbfs_tmpclose(share, np, fid, context);
	}
    
	if (error) {
		SMBWARNING_LOCK(np, "%s failed %d\n", np->n_name, error);
	}
    
	return error;
}

/*
 * Supports reading a faked up symbolic link. This is Conrad and Steve
 * French method for storing and reading symlinks on Window Servers.
 *
 * The calling routine must hold a reference on the share
 *
 */
int 
smbfs_smb_windows_read_symlink(struct smb_share *share, struct smbnode *np, 
							   struct uio *uiop, vfs_context_t context)
{
	unsigned char *wbuf = NULL, *cp = NULL;
	unsigned len = 0, flen = 0;
	uio_t uio = NULL;
	int error = 0;
    SMBFID fid = 0;
	char *target = NULL;
    size_t read_size = 0; /* unused */

	flen = SMB_SYMLEN;
    SMB_MALLOC_DATA(wbuf, flen, Z_WAITOK);
    
    uio = uio_create(1, 0, UIO_SYSSPACE, UIO_READ);
    uio_addiov(uio, CAST_USER_ADDR_T(wbuf), flen);

    if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
        /* Do a Create/Read/Close */
        error = smbfs_smb_cmpd_create_read_close(share, np,
                                                 NULL, 0,
                                                 NULL, 0,
                                                 uio, &read_size,
                                                 NULL,
                                                 context);
    }
    else {
        /* SMB 1 */
        error = smbfs_tmpopen(share, np, SMB2_FILE_READ_DATA, &fid, context);
        if (error) {
            goto out;
        }
        
        error = smb_smb_read(share, fid, uio, 0, context);
        
        (void)smbfs_tmpclose(share, np, fid, context);
    }
    
    if (error) {
        goto out;
    }

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
    /* %%% is this function valid for SMB 2/3, note the SMBHDRLEN below */
	target = (char *)(wbuf + SMB_SYMHDRLEN);

    SMBSYMDEBUG_LOCK(np, "%s --> %s\n", np->n_name, target);
    
	smbfs_update_symlink_cache(np, target, len);
	if (uiop != NULL) {
		error = uiomove(target, (int)len, uiop);
	}
out:
    if (uio) {
        uio_free(uio);
    }
    
    SMB_FREE_DATA(wbuf, flen);
    
	if (error) {
		SMBWARNING_LOCK(np, "%s failed %d\n", np->n_name, error);
	}
	return (error);
}

/*
 * Support for reading a symbolic link that resides on a UNIX server. This allows
 * us to access and share symbolic links with AFP and NFS.
 *
 * The calling routine must hold a reference on the share
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
	size_t targetlen, target_allocsize = 0;
	size_t nmlen, ntwrkname_allocsize = 0;
	
    /* %%% To Do - implement for SMB 2/3 */
    if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
        SMBERROR("smbfs_smb_unix_read_symlink not supported by SMB 2/3\n");
        error = ENOTSUP;
        return error;
    }

	error = smb_t2_alloc(SSTOCP(share), SMB_TRANS2_QUERY_PATH_INFORMATION, context, &t2p);
	if (error)
		return error;
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	mb_put_uint16le(mbp, SMB_QFILEINFO_UNIX_LINK);
	mb_put_uint32le(mbp, 0);
	error = smbfs_fullpath(mbp, np, NULL, NULL, UTF_SFM_CONVERSIONS, 
						   SMB_UNICODE_STRINGS(SS_TO_SESSION(share)), '\\');
	if (error) 
	    goto out;
	t2p->t2_maxpcount = 2;
	t2p->t2_maxdcount = SS_TO_SESSION(share)->session_txmax;
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
    m_fixhdr(mdp->md_top);
    nmlen = mbuf_get_chain_len(mdp->md_top);
	SMBSYMDEBUG("network len of the symbolic link = %ld\n", nmlen);
    ntwrkname_allocsize = nmlen;
    SMB_MALLOC_DATA(ntwrkname, ntwrkname_allocsize, Z_WAITOK);
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
    target_allocsize = targetlen;
    SMB_MALLOC_DATA(target, target_allocsize, Z_WAITOK_ZERO);
	error = smb_convert_network_to_path(ntwrkname, nmlen, target, 
											&targetlen, '/', UTF_SFM_CONVERSIONS, 
											SMB_UNICODE_STRINGS(SS_TO_SESSION(share)));
	if (!error) {
		SMBSYMDEBUG_LOCK(np, "%s --> %s\n", np->n_name, target);
		smbfs_update_symlink_cache(np, target, targetlen);
		if (uiop != NULL) {
			error = uiomove(target, (int)targetlen, uiop);
		}
	}
	
    SMB_FREE_DATA(target, target_allocsize);
	
out:
    SMB_FREE_DATA(ntwrkname, ntwrkname_allocsize);
	smb_t2_done(t2p);
	if (error) {
		SMBWARNING_LOCK(np, "%s failed %d\n", np->n_name, error);
	}
	return error;
}

/*
 * When calling this routine be very careful when passing the arguments. 
 * Depending on the arguments different actions will be taken with this routine. 
 *
 * The calling routine must hold a reference on the share
 *
 */
int 
smb1fs_smb_qstreaminfo(struct smb_share *share, struct smbnode *np,
                       const char *namep, size_t name_len,
                       const char *strmname,
                       uio_t uio, size_t *sizep,
                       uint64_t *strmsize, uint64_t *strmsize_alloc,
                       uint32_t *stream_flagsp,
                       vfs_context_t context)
{
	struct smb_t2rq *t2p;
	int error;
	struct mbchain *mbp;
	struct mdchain *mdp;
	uint32_t next, nlen, used, ntwrkname_allocsize = 0;
	uint64_t stream_size, stream_alloc_size;
	enum stream_types stype = kNoStream;
	struct timespec ts;
	int foundStream = (strmname) ? FALSE : TRUE; /* Are they looking for a specific stream */
	char *streamName = NULL;		/* current file name */
	size_t streamNameLen = 0;	/* name len */
    size_t in_name_len = name_len;
    size_t streamNameMallocLen = 0;
    const char *fname;
    uint32_t is_data_stream = 0;
    int n_name_locked = 0;
	
    /* 
     * Two usage cases:
     * 1) Given np and namep == NULL. Query the path of np
     * 2) Given np and namep (must be readdirattr). Query PARENT np and 
     * child namep. In this case, do not update vnode np as that is the parent.
     * 
     * In both cases, Query for a list of streams and if streams are found, 
     * see if they match stream_namep that was passed in.
     */

    if (namep == NULL) {
        /* Not readdirattr case */
        if ((np->n_fstatus & kNO_SUBSTREAMS) ||
            (np->n_dosattr & SMB_EFA_REPARSE_POINT)) {
            foundStream = FALSE;
            error = ENOATTR;
            goto done;
        }
    }
	
	if (sizep)
		*sizep = 0;

	error = smb_t2_alloc(SSTOCP(share), SMB_TRANS2_QUERY_PATH_INFORMATION, context, &t2p);
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
	if (SESSION_CAPS(SS_TO_SESSION(share)) & SMB_CAP_INFOLEVEL_PASSTHRU)
		mb_put_uint16le(mbp, SMB_QFILEINFO_STREAM_INFORMATION);
	else
		mb_put_uint16le(mbp, SMB_QFILEINFO_STREAM_INFO);
	mb_put_uint32le(mbp, 0);

	error = smbfs_fullpath(mbp, np, namep, &in_name_len, UTF_SFM_CONVERSIONS,
						   SMB_UNICODE_STRINGS(SS_TO_SESSION(share)), '\\');
	if (error)
		goto out;
	t2p->t2_maxpcount = 2;
	t2p->t2_maxdcount = SS_TO_SESSION(share)->session_txmax;
	error = smb_t2_request(t2p);
	if (error)
		goto out;

	/*
	 * Should never happen since we test for UNICODE CAPS when mounting, but
	 * just to be safe never let them send us a Query Stream call that doesn't
	 * have the UNICODE bit set in the flags field. See <rdar://problem/7653132>
	 */
	if (!SMB_UNICODE_STRINGS(SS_TO_SESSION(share))) {
		error = ENOTSUP;
		goto out;		
	}
	mdp = &t2p->t2_rdata;
	/*
	 * There could be no streams info associated with the item. You will find this with directory
	 * or items copied from FAT file systems. Nothing to process just get out.
	 */
	if (mdp->md_cur == NULL)
		goto out;
	
	do {
		char *ntwrk_name = NULL;
		
		stream_size = 0;
        stream_alloc_size = 0;
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
		if ((error = md_get_uint64le(mdp, &stream_alloc_size))) /* alloc size */
			goto out;
		used += 8;
		/*
		 * Sanity check to limit DoS or buffer overrun attempts.
		 * Make sure the length is not bigger than our max buffers size.
		 */
		if (nlen > SS_TO_SESSION(share)->session_txmax) {
			error = EBADRPC;
			goto out;
		}
        ntwrkname_allocsize = nlen;
        SMB_MALLOC_DATA(ntwrk_name, ntwrkname_allocsize, Z_WAITOK_ZERO);
		if ((error = md_get_mem(mdp, ntwrk_name, nlen, MB_MSYSTEM))) {
            SMB_FREE_DATA(ntwrk_name, ntwrkname_allocsize);
			goto out;
		}
		used += nlen;

		/* 
		 * Ignore any trailing null, not that we expect them 
		 * NOTE: MS-CIFS states that the stream name is alway in UNICODE. We
		 * only support streams if the server supports UNICODE.
		 */
		if ((nlen > 1) && !ntwrk_name[nlen - 1] && !ntwrk_name[nlen - 2])
			nlen -= 2;
		streamNameLen = nlen;
		streamName = smbfs_ntwrkname_tolocal(ntwrk_name, &streamNameLen, &streamNameMallocLen,
											 SMB_UNICODE_STRINGS(SS_TO_SESSION(share)));
        SMB_FREE_DATA(ntwrk_name, ntwrkname_allocsize);
		/*
		 * We should now have a name in the form : <foo> :$DATA Where <foo> is 
		 * UTF-8 w/o null termination. If it isn't in that form we want to LOG it 
		 * and skip it. Note we want to skip w/o logging the "data fork" entry,
		 * which is simply ::$DATA Otherwise we want to uiomove out <foo> with a null added.
		 */
        if (namep == NULL) {
            lck_rw_lock_shared(&np->n_name_rwlock);
            n_name_locked = 1;
            fname = np->n_name;
        }
        else {
            /* readdirattr case - parent np and child namep */
            fname = namep;
        }

		if (smbfs_smb_undollardata(fname, streamName, &streamNameLen,
                                   &is_data_stream)) {
			const char *s;

            if (n_name_locked) {
                lck_rw_unlock_shared(&np->n_name_rwlock);
                n_name_locked = 0;
            }
            
			/* the "+ 1" skips over the leading colon */
			s = streamName + 1;
			
			/* Check for special case streams (resource fork and finder info */
			if ((streamNameLen >= sizeof(SFM_RESOURCEFORK_NAME)) && 
				(!strncasecmp(s, SFM_RESOURCEFORK_NAME, sizeof(SFM_RESOURCEFORK_NAME)))) {
				stype |= kResourceFrk;

                if (namep == NULL) {
                    /*
                     * We always get the resource fork size and cache it here.
                     * Dirs never have resource forks.
                     */
                    if (!(vnode_isdir(np->n_vnode))) {
                        lck_mtx_lock(&np->rfrkMetaLock);
                        np->rfrk_size = stream_size;
                        np->rfrk_alloc_size = stream_alloc_size;
                        nanouptime(&ts);
                        np->rfrk_cache_timer = ts.tv_sec;
                        lck_mtx_unlock(&np->rfrkMetaLock);
                    }
                    else {
                        SMBERROR_LOCK(np, "Why is the server returning a resource fork on a dir <%s>??? \n",
                                      np->n_name);
                    }
                }
                
				/* 
				 * The Resource fork and Finder info names are special and get 
				 * translated between stream names and extended attribute names. 
				 * In this case we need to make sure the correct name gets used. 
				 * So we are looking for a specfic stream use its stream name 
				 * otherwise use its extended attribute name.
				 */
				if ((uio == NULL) && strmname && (sizep == NULL)) {
					s = SFM_RESOURCEFORK_NAME;
					streamNameLen = sizeof(SFM_RESOURCEFORK_NAME);
				} else {
					s = XATTR_RESOURCEFORK_NAME;
					streamNameLen = sizeof(XATTR_RESOURCEFORK_NAME);
				}
				/* 
				 * The uio means we are gettting this from a listxattr call, never 
				 * display zero length resource forks. Resource forks should 
				 * always contain a resource map. Seems CoreService never deleted
				 * the resource fork, they just set the eof to zero. We need to
				 * handle these resource forks here.
				 */
				if (uio && (stream_size == 0))
					goto skipentry;				    

			} else if ((streamNameLen >= sizeof(SFM_FINDERINFO_NAME)) && 
					(!strncasecmp(s, SFM_FINDERINFO_NAME, sizeof(SFM_FINDERINFO_NAME)))) {
				/*
				 * They have an AFP_Info stream and it has no size must be a Samba 
				 * server. We treat this the same as if the file has no Finder Info
				 */
				if (stream_size == 0)
					goto skipentry;

				stype |= kFinderInfo;
				/* 
				 * The Resource fork and Finder info names are special and get 
				 * translated between stream names and extended attribute names. 
				 * In this case we need to make sure the correct name gets used. 
				 * So we are looking for a specfic stream use its stream name 
				 * otherwise use its extended attribute name.
				 */
				if ((uio == NULL) && strmname && (sizep == NULL)) {
					s = SFM_FINDERINFO_NAME;
					streamNameLen = sizeof(SFM_FINDERINFO_NAME);
				}
				else {
					s = XATTR_FINDERINFO_NAME;
					streamNameLen = sizeof(XATTR_FINDERINFO_NAME);
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
			if (( (streamNameLen >= sizeof(SFM_DESKTOP_NAME)) && 
				(!strncasecmp(s, SFM_DESKTOP_NAME, sizeof(SFM_DESKTOP_NAME)))) ||
				((streamNameLen >= sizeof(SFM_IDINDEX_NAME)) && 
				(!strncasecmp(s, SFM_IDINDEX_NAME, sizeof(SFM_IDINDEX_NAME))))) {
				/*  Is this a SFM Volume */
				if (strmname && (!strncasecmp(SFM_DESKTOP_NAME, strmname, sizeof(SFM_DESKTOP_NAME)))) {
					foundStream = TRUE;
				}
				goto skipentry;
			} else if (uio)
				uiomove(s, (int)streamNameLen, uio);
			else if (!foundStream && strmname && strmsize) {
				/* They are looking for a specific stream name and we havn't found it yet. */ 
				nlen = (uint32_t)strnlen(strmname, share->ss_maxfilenamelen+1);
				if ((streamNameLen >= nlen) && (!strncasecmp(s, strmname, nlen))) {
					*strmsize = stream_size;
                    *strmsize_alloc = stream_alloc_size;
					foundStream = TRUE;
				}
			}
			/* 
			 * They could just want to know the buffer size they will need when 
			 * requesting a list. This has several problem, but we cannot solve 
			 * them all here. First someone can create a stream/EA between this 
			 * call and the one they make to get the data. Second this will cause 
			 * an extra round of traffic. We could cache all of this, but how 
			 * long would we keep this information around. Could require a large buffer.
			 */
			if (sizep)
				*sizep += streamNameLen;
		}

        if (n_name_locked) {
            lck_rw_unlock_shared(&np->n_name_rwlock);
            n_name_locked = 0;
        }

skipentry:
        SMB_FREE_DATA(streamName, streamNameMallocLen);
		/* 
		 * Next should be the offset to the next entry. We have already move into 
		 * the buffer used bytes. So now need to move pass any remaining pad bytes. 
		 * So if the value next is larger than the value used, then we need to move
		 * that many more bytes into the buffer. If that value is larger than 
		 * our buffer get out.
		 */
		if (next > used) {
			next -= used;
			if (next > SS_TO_SESSION(share)->session_txmax) {
				error = EBADRPC;
				goto out;
			}
			md_get_mem(mdp, NULL, next, MB_MSYSTEM);
		}
	} while (next && !error);

out:
    if (streamName != NULL) {
        SMB_FREE_DATA(streamName, streamNameMallocLen);
    }
	smb_t2_done(t2p);

done:
	/* 
     * If we searched the entire list and did not find a finder info stream, 
     * then reset the cache timer. 
     */
    if ((stype & kFinderInfo) != kFinderInfo) {
        /* set stream_flag indicating no Finder Info */
        *stream_flagsp |= SMB_NO_FINDER_INFO;
        
        if (namep == NULL) {
            /* Negative cache the Finder Info in the vnode */
            bzero(np->finfo, sizeof(np->finfo));
            nanouptime(&ts);
            np->finfo_cache_timer = ts.tv_sec;
        }
    }

    /* 
     * If we searched the entire list and did not find a resource stream, 
     * then reset the cache timer. 
     */
    if ((stype & kResourceFrk) != kResourceFrk) {
        /* set stream_flag indicating no Resource Fork */
        *stream_flagsp |= SMB_NO_RESOURCE_FORK;
        
        if ((namep == NULL) && !(vnode_isdir(np->n_vnode))) {
            /*
             * Negative cache the resource fork in the vnode
             * Dirs never have resource forks.
             */
            lck_mtx_lock(&np->rfrkMetaLock);
            
            nanouptime(&ts);
            np->rfrk_size = 0;
            np->rfrk_alloc_size = 0;
            np->rfrk_cache_timer = ts.tv_sec;
            
            lck_mtx_unlock(&np->rfrkMetaLock);
        }
    }
	
	if ((foundStream == FALSE) || (error == ENOENT))	/* We did not find the stream we were looking for */
		error = ENOATTR;
	return (error);
}

/*
 * The SMB_QFS_POSIX_WHOAMI allows us to find out who the server thinks we are
 * and what groups we are in. 
 *
 * The calling routine must hold a reference on the share
 * 
 */
void 
smbfs_unix_whoami(struct smb_share *share, struct smbmount *smp, vfs_context_t context)
{
	struct smb_t2rq *t2p;
	struct mbchain *mbp;
	struct mdchain *mdp;
	int error;
	uint32_t ii;
	uint32_t reserved;
	size_t total_bytes = 0;
	uint32_t ntwrk_sids_cnt;
	uint32_t ntwrk_sid_size;	

	error = smb_t2_alloc(SSTOCP(share), SMB_TRANS2_QUERY_FS_INFORMATION, context, &t2p);
	if (error)
		return;
	
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	mb_put_uint16le(mbp, SMB_QFS_POSIX_WHOAMI);
	t2p->t2_maxpcount = 4;
	t2p->t2_maxdcount = SS_TO_SESSION(share)->session_txmax;
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
	total_bytes = smp->ntwrk_cnt_gid * sizeof(uint64_t);
	if ((total_bytes / sizeof(uint64_t)) != smp->ntwrk_cnt_gid) {
		error = EBADRPC;
		goto done;
	}
	
	/* Make sure we are not allocating more than we said we could handle */
	if (total_bytes > SS_TO_SESSION(share)->session_txmax) {
		error = EBADRPC;
		goto done;
	}
    
    smp->ntwrk_gids_allocsize = total_bytes;
    SMB_MALLOC_DATA(smp->ntwrk_gids, smp->ntwrk_gids_allocsize, Z_WAITOK);
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
	UNIX_CAPS(share) |= UNIX_QFS_POSIX_WHOAMI_CAP;
	/* 
	 * At this point we have everything we really need. So any errors from this
	 * point on should be ignored. If we error out below we should just pretend 
	 * that we didn't get any network sids.
	 */
sid_groups:
	if (share->ss_attributes & FILE_PERSISTENT_ACLS) {
		smb_get_sid_list(share, smp, mdp,ntwrk_sids_cnt, ntwrk_sid_size);
	}

done:	
	smb_t2_done(t2p);
	if (error == EBADRPC)
		SMBERROR("Parsing error reading the message\n");
	
	if (error && smp->ntwrk_gids) {
        SMB_FREE_DATA(smp->ntwrk_gids, smp->ntwrk_gids_allocsize);
		smp->ntwrk_cnt_gid = 0;
	}
}

/*
 * This is a  UNIX server get its UNIX capiblities
 *
 * The calling routine must hold a reference on the share
 * 
 */
void 
smbfs_unix_qfsattr(struct smb_share *share, vfs_context_t context)
{
	struct smb_t2rq *t2p;
	struct mbchain *mbp;
	struct mdchain *mdp;
	uint16_t majorv;
	uint16_t minorv;
	uint64_t cap;
	int error;
	
	error = smb_t2_alloc(SSTOCP(share), SMB_TRANS2_QUERY_FS_INFORMATION, context, &t2p);
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
		goto done;
	}
	md_get_uint16le(mdp,  &majorv);
	md_get_uint16le(mdp, &minorv);
	md_get_uint64le(mdp, &cap);
	SMBWARNING("version %x.%x cap = %llx\n", majorv, minorv, cap);
	UNIX_CAPS(share) = UNIX_QFS_UNIX_INFO_CAP | (cap & CIFS_UNIX_POSIX_PATH_OPERATIONS_CAP);
	
	if (UNIX_CAPS(share) & CIFS_UNIX_POSIX_PATH_OPERATIONS_CAP) {
		UNIX_CAPS(share) |= UNIX_QFILEINFO_UNIX_LINK_CAP | UNIX_SFILEINFO_UNIX_LINK_CAP | 
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
		 * UNIX_CAPS(share) |= UNIX_SFILEINFO_POSIX_UNLINK_CAP;
		 */
	}
done:
	smb_t2_done(t2p);
}

/* 
 * Since the first thing we do is set the default values there is no longer 
 * any reason to return an error for this routine. Some servers may not support
 * this call. We should not fail the mount just because they do not support this
 * call. 
 *
 * The calling routine must hold a reference on the share
 * 
 */
void 
smb1fs_qfsattr(struct smb_share *share, vfs_context_t context)
{
	struct smb_t2rq *t2p;
	struct mbchain *mbp;
	struct mdchain *mdp;
	uint32_t nlen = 0, ntwrkname_allocsize = 0;
	int error;
	size_t fs_nmlen;	/* The sized malloced for fs_name */
    size_t fs_allocsize = 0;
	char *fsname = NULL;

	/* Start with the default values */
	share->ss_fstype = SMB_FS_FAT;	/* default to FAT File System */
	share->ss_attributes = 0;
	share->ss_maxfilenamelen = 255;

	error = smb_t2_alloc(SSTOCP(share), SMB_TRANS2_QUERY_FS_INFORMATION, context, &t2p);
	if (error) {
		return;
    }
    
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	mb_put_uint16le(mbp, SMB_QFS_ATTRIBUTE_INFO);
	t2p->t2_maxpcount = 4;
	t2p->t2_maxdcount = 4 * 3 + 512;
	error = smb_t2_request(t2p);
	if (error) {
		goto done;
    }
	
	mdp = &t2p->t2_rdata;
	/*
	 * At this point md_cur and md_top have the same value. Now all the md_get 
	 * routines will will check for null, but just to be safe we check here
	 */
	if (mdp->md_cur == NULL) {
		error = EBADRPC;
    }
	else {
		md_get_uint32le(mdp,  &share->ss_attributes);
		
        /*
         * Make sure Max Name Length is a reasonable value.
         * See <rdar://problem/12171424>.
         */
        md_get_uint32le(mdp, &share->ss_maxfilenamelen);   /* max name length */
        if (share->ss_maxfilenamelen > (SMB_MAXFNAMELEN * 2)) {
            SMBERROR("Illegal file name len %u\n", share->ss_maxfilenamelen);
            share->ss_maxfilenamelen = 255;
        }
        
		error = md_get_uint32le(mdp, &nlen);	/* fs name length */
	}
	if (error) {
		/* This is a very bad server */
		SMBWARNING("Server returned a bad SMB_QFS_ATTRIBUTE_INFO message\n");
		/* Don't believe them when they say they are unix */
		SS_TO_SESSION(share)->session_sopt.sv_caps &= ~SMB_CAP_UNIX;
		goto done;
	}
	if ((nlen > 0) && (nlen < PATH_MAX)) {
		char *ntwrkName;

        ntwrkname_allocsize = nlen;
        SMB_MALLOC_DATA(ntwrkName, ntwrkname_allocsize, Z_WAITOK_ZERO);
		md_get_mem(mdp, ntwrkName, nlen, MB_MSYSTEM);
		/*
		 * Just going from memory, I believe this is really just a
		 * WCHAR not a STRING value. I know that both Windows 98
		 * and SNAP return it as WCHAR and neither supports 
		 * UNICODE. So if they do not support UNICODE then lets 
		 * do some test and see if we can get the file system name.
		 */
		
        fs_nmlen = nlen;
        fsname = smbfs_ntwrkname_tolocal(ntwrkName, &fs_nmlen, &fs_allocsize,
                                        SMB_UNICODE_STRINGS(SS_TO_SESSION(share)));
        SMB_FREE_DATA(ntwrkName, ntwrkname_allocsize);

		if (fsname == NULL) {
			goto done;	/* Should never happen, but just to be safe */
		}
		
		fs_nmlen += 1; /* Include the null byte for the compare */
		/*
		 * Let's start keeping track of the file system type. Most
		 * things we need to do differently really depend on the
		 * file system type. As an example we know that FAT file systems
		 * do not update the modify time on drectories.
		 */
		if (strncmp(fsname, "FAT", fs_nmlen) == 0)
			share->ss_fstype = SMB_FS_FAT;
		else if (strncmp(fsname, "FAT12", fs_nmlen) == 0)
			share->ss_fstype = SMB_FS_FAT;
		else if (strncmp(fsname, "FAT16", fs_nmlen) == 0)
			share->ss_fstype = SMB_FS_FAT;
		else if (strncmp(fsname, "FAT32", fs_nmlen) == 0)
			share->ss_fstype = SMB_FS_FAT;
		else if (strncmp(fsname, "CDFS", fs_nmlen) == 0)
			share->ss_fstype = SMB_FS_CDFS;
		else if (strncmp(fsname, "UDF", fs_nmlen) == 0)
			share->ss_fstype = SMB_FS_UDF;
		else if (strncmp(fsname, "NTFS", fs_nmlen) == 0)
			share->ss_fstype = SMB_FS_NTFS_UNKNOWN;	/* Could be lying */

		SMBWARNING("%s/%s type '%s', attr 0x%x, maxfilename %d\n",
				   SS_TO_SESSION(share)->session_srvname, share->ss_name, fsname, 
				   share->ss_attributes, share->ss_maxfilenamelen);
		/*
		 * NT4 will not return the FILE_NAMED_STREAMS bit in the ss_attributes
		 * even though they support streams. So if its a NT4 server and a
		 * NTFS file format then turn on the streams flag.
		 */
		 if ((SS_TO_SESSION(share)->session_flags & SMBV_NT4) && (share->ss_fstype & SMB_FS_NTFS_UNKNOWN))
			share->ss_attributes |= FILE_NAMED_STREAMS;
		 /* 
		  * The server says they support streams and they say they are NTFS. So mark
		  * the subtype as NTFS. Remember a lot of non Windows servers pretend
		  * their NTFS so they can support ACLs, but they aren't really because they have
		  * no stream support. This allows us to tell the difference.
		  */
		 if ((share->ss_fstype == SMB_FS_NTFS_UNKNOWN) && (share->ss_attributes & FILE_NAMED_STREAMS))
			 share->ss_fstype = SMB_FS_NTFS;	/* Real NTFS Volume */
		 else if ((share->ss_fstype == SMB_FS_NTFS_UNKNOWN) && (UNIX_SERVER(SS_TO_SESSION(share))))
			 share->ss_fstype = SMB_FS_NTFS_UNIX;	/* UNIX system lying about being NTFS */
		 /* Some day mark it as being Mac OS X */
	}
done:
    SMB_FREE_DATA(fsname, fs_allocsize);
	smb_t2_done(t2p);
}

/*
 * The calling routine must hold a reference on the share
 */
int
smb1fs_statfs(struct smb_share *share, struct vfsstatfs *sbp, vfs_context_t context)
{
	struct smb_t2rq *t2p;
	struct mbchain *mbp;
	struct mdchain *mdp;
	uint32_t bpu, bsize32;
	uint64_t s, t, f;
	int error;
	size_t xmax;
    
	error = smb_t2_alloc(SSTOCP(share), SMB_TRANS2_QUERY_FS_INFORMATION, context, &t2p);
	if (error)
		return error;
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
    
    /* SMB_QFS_SIZE_INFO info level allows us to return large volume sizes.
     *
     *		Total number of allocated units as a 64 bit value
     *		Number of free units as a 64 bit value
     *		Number of sectors in each unit as a 32 bit value
     *		Number of bytes in each sector as a 32 bit value
     */
    mb_put_uint16le(mbp, SMB_QFS_SIZE_INFO);
    t2p->t2_maxpcount = 4;
    /* The call returns two 64 bit values and two 32 bit value */
    t2p->t2_maxdcount = (8 * 2) + (4 * 2);

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
	/* Now retrieve the correct inforamtion. */
    md_get_uint64le(mdp, &t); /* Total number of allocated units */
    md_get_uint64le(mdp, &f); /* Number of free units */
    md_get_uint32le(mdp, &bpu); /* Number of sectors in each unit */
    md_get_uint32le(mdp, &bsize32);	/* Number of bytes in a sector */
    s = bsize32;
    s *= bpu;
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
	sbp->f_files = sbp->f_blocks - 2;  /* max files is constrained by total blocks */
    sbp->f_ffree = sbp->f_bavail;
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
	 * NOTE: We always make sure that session_rxmax and session_wxmax are on a 1k boundary!
	 */
	
	xmax = max(SS_TO_SESSION(share)->session_rxmax, SS_TO_SESSION(share)->session_wxmax);
    
    /*
     * Now we want to make sure it will land on both a PAGE_SIZE boundary and a
     * smb xfer size boundary. So first mod the xfer size by the page size, then
     * subtract that from page size. This will give us the extra amount that
     * will be needed to get it on a page boundary. Now divide the page size by
     * this amount. This will give us the number of xmax it will take to make
     * f_iosize land on a page size and xfer boundary.
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
	 * Snow (xfer is 126K) - f_iosize
	 * (4 / (4 - (126 % 4))) * 126 = 252
	 * (1024 / 252) * 252 = 1008
	 * (1008 / PAGE_SIZE) = exactly 252 pages
	 * (1008 / 126) = exactly 8 xfer
	 *
	 * Lion (xfer is 128K) - f_iosize
	 * (4 / (4 - (128 % 4))) * 128 = 128
	 * (1024 / 128) * 128 = 1024
	 * (1024 / PAGE_SIZE) = exactly 256 pages
	 * (1024 / 128) = exactly 8 xfer
	 */

	return 0;
}

/*
 * The calling routine must hold a reference on the share
 */
int
smbfs_smb_t2rename(struct smb_share *share, struct smbnode *np, 
                   const char *tname, size_t tnmlen, int overwrite, 
                   SMBFID *infid, vfs_context_t context)
{
	struct smb_t2rq *t2p;
	struct mbchain *mbp;
	int32_t *ucslenp;
	size_t	outLen = 0;
	int error, cerror;
    SMBFID fid = 0;
    int need_close = 0;
    uint16_t smb1_fid = 0;

    if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
        SMBERROR("smbfs_smb_t2rename not supported by SMB 2/3\n");
        error = ENOTSUP;
        return error;
    }

    if (infid) {
        fid = *infid;
        smb1_fid = (uint16_t) fid;
    }
    else {
        fid = 0;
    }
    
	/*
	 * We will continue to return not supported here. If the calling routine 
	 * needs a different error then it needs to make this check before calling 
	 * this routine.
	 */
	if (!(SESSION_CAPS(SS_TO_SESSION(share)) & SMB_CAP_INFOLEVEL_PASSTHRU))
		return (ENOTSUP);
	/*
	 * Rember that smb_t2_alloc allocates t2p. We need to call
	 * smb_t2_done to free it.
	 */
	error = smb_t2_alloc(SSTOCP(share), SMB_TRANS2_SET_FILE_INFORMATION, context, &t2p);
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
		error = smbfs_tmpopen(share, np, SMB2_DELETE, &fid, context);
		if (error)
			goto exit;
        need_close = 1;
        smb1_fid = fid;
	}
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	mb_put_mem(mbp, (caddr_t)&smb1_fid, sizeof(smb1_fid), MB_MSYSTEM);
	mb_put_uint16le(mbp, SMB_SFILEINFO_RENAME_INFORMATION);
	mb_put_uint16le(mbp, 0); /* reserved, nowadays */
	mbp = &t2p->t2_tdata;
	mb_init(mbp);
	mb_put_uint32le(mbp, overwrite);
	mb_put_uint32le(mbp, 0); /* Root fid, not used */
	/* Reserve file name location */
	ucslenp = (int32_t *)mb_reserve(mbp, sizeof(int32_t));
	error = smb_put_dmem(mbp, tname, tnmlen, UTF_SFM_CONVERSIONS, 
						 SMB_UNICODE_STRINGS(SS_TO_SESSION(share)), &outLen);
	if (!error)
		error = mb_put_uint16le(mbp, 0);
	if (error)
		goto exit;
	/* Now we can put the file name length into the buffer */
	*ucslenp = htolel((int32_t)outLen); 
	t2p->t2_maxpcount = 2;
	t2p->t2_maxdcount = 0;
	error = smb_t2_request(t2p);
exit:;  
	if (need_close == 1) {
		cerror = smbfs_tmpclose(share, np, fid, context);
		if (cerror) {
			SMBWARNING("error %d closing fid %llx\n", cerror, fid);
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
 * name and rename the file. Make the file hidden if we can. Now lets mark
 * it for deletion and close the file. If the rename fails then the whole call 
 * should fail. If the mark for deletion call fails just set a flag on the 
 * vnode and delete it when we close.
 *
 * The calling routine must hold a reference on the share
 *
 */
int
smbfs_delete_openfile(struct smb_share *share, struct smbnode *dnp, 
					  struct smbnode *np, vfs_context_t context)
{
    struct proc	*p;
    SMBFID fid = 0;
    int	error, cerror;
    char s_name[32];	/* make sure that sillyrename_name will fit */
    size_t s_namlen;
    int	i, j, k;
    int need_close = 0;
    int need_hide = 0;
    int samba_bug = 0;
	struct smbmount *smp = np->n_mount;
    uint64_t ino = 0;
    uint64_t hashval = 0;
    char *new_name = NULL;
    char *old_name = NULL;
    size_t new_name_allocsize = 0, old_name_allocsize = 0;
	
    /* Should never happen, but just to be safe */
    if (context == NULL) {
        return ENOTSUP;
    }
    p = vfs_context_proc(context);
    
    if (!(SS_TO_SESSION(share)->session_flags & SMBV_SMB2)) {
        /*
         * SMB 1
         *
         * smbfs_smb_t2rename requires passthru, so just return EBUSY since we 
         * are attempting to delete an open file.
         */
        if (!(SESSION_CAPS(SS_TO_SESSION(share)) & SMB_CAP_INFOLEVEL_PASSTHRU)) {
            return EBUSY;
        }

        error = smbfs_tmpopen(share, np, SMB2_DELETE, &fid, context);
        if (error) {
            return (error);
        }
        need_close = 1;
        
        /* SMB 2/3 will not hide the file as I deem it unnecessary work */
        need_hide = 1;
    }
    
    if (SS_TO_SESSION(share)->session_misc_flags & SMBV_HAS_FILEIDS) {
        /* 
         * Use the unique File ID to create a unique name 
         */
        lck_rw_lock_shared(&np->n_name_rwlock);
        ino = smb2fs_smb_file_id_get(smp, np->n_ino, np->n_name);
        lck_rw_unlock_shared(&np->n_name_rwlock);
        
        /* Just try once to rename in the same dir */
        MAKE_DELETED_NAME((char*) s_name, sizeof(s_name), ino);
        s_namlen = strlen((char *) s_name);
        
        error = smbfs_smb_rename(share, np, dnp, s_name, s_namlen, context);
    }
    else {
        /* 
         * No File IDs, so have to use NFS naming convention to find unique
         * name. Get the first silly name 
         */
        s_namlen = snprintf(s_name, sizeof(s_name), ".smbdeleteAAA%04x4.4",
                            proc_pid(p));
        if (s_namlen >= sizeof(s_name)) {
            error = ENOENT;
            goto out;
        }

        /* Try rename until we get one that isn't there */
        i = j = k = 0;

        do {
            if ((SS_TO_SESSION(share)->session_flags & SMBV_SMB2) ||
                (samba_bug == 1)) {
                /* SMB 2/3 or dealing with SAMBA buggy server */
                error = smbfs_smb_rename(share, np, dnp, s_name, s_namlen,
                                         context);
            }
            else {
                /* SMB 1 */
                error = smbfs_smb_t2rename(share, np, s_name, s_namlen, 0, &fid,
                                           context);
                
                if (((error == ENOENT) || (error == ENOTSUP)) &&
                    SMBTOV(dnp) &&
                    (samba_bug == 0)) {
                    /*
                     * SAMBA Bug:
                     *
                     * Some times the SAMBA code gets confused and fails the 
                     * above rename with an ENOENT error. We need to make sure 
                     * this code works with all SAMBA servers, so try again 
                     * with the old rename call. SAMBA allows us to rename an 
                     * open file with this call, but not with delete access. 
                     * So close it, rename it and hide it. Once the SAMBA 
                     * server is in this state, it will always return an error, 
                     * so just use the old rename from now on.
                     */
                    if (need_close == 1) {
                        (void)smbfs_tmpclose(share, np, fid, context);
                        need_close = 0;
                    }
                    
                    /* loop around and try with old rename */
                    samba_bug = 1;
                    error = EEXIST; /* so we loop around */
                    continue;
                }
            }
                
            if (error && (error != EEXIST)) {
                /* 
                 * They returned an error we did not expect. If the silly name 
                 * file exists then we want to keep trying. So do a look up and 
                 * if the file exists, keep trying otherwise just get out since 
                 * there is nothing else for us to do.
                 */
                if (smbfs_smb_query_info(share, dnp, VREG,
                                         s_name, s_namlen, NULL, context) == 0) {
                    /* That name exists on server, keep trying for a new name */
                    error = EEXIST;
                }
                else {
                    break;
                }
            }
            
            /* 
             * The file name already exists, try another one. This code was 
             * taken from NFS. NFS tested by doing a lookup, we use the rename 
             * call, see above for how we handle strange errors. If the rename 
             * call fails, keep trying till we run out of names.
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
        } while (error == EEXIST);
    }

    if (error) {
        goto out;
    }
    
    /*
     * At this point, the file has been successfully renamed on the server.
     */
    
    if (need_hide) {
        /* ignore any errors return from hiding the item */
        (void)smbfs_set_hidden_bit(share, dnp, VREG,
                                   s_name, s_namlen,
                                   TRUE, context);
    }

    if ((SS_TO_SESSION(share)->session_flags & SMBV_SMB2) || (need_close == 0)) {
        /* 
         * SMB 2/3 will always just wait until last close to do the delete.
         * This matches AFP behavior.
         * If we hit the SAMBA bug, then the file is closed so cant set the
         * Delete on Close because we have no fid.
         */
        np->n_flag |= NDELETEONCLOSE;
    }
    else {
        /* SMB 1 - Set file for Delete on Close */
        cerror = smbfs_smb_markfordelete(share, fid, context);
        if (cerror) {
            /* 
             * Failed to delete it now, so just wait until last close to do 
             * the delete. Could be SAMBA misbehaving.
             */
            np->n_flag |= NDELETEONCLOSE;
        }
    }
	
out:
    if (need_close == 1) {
        cerror = smbfs_tmpclose(share, np, fid, context);
        if (cerror) {
            SMBWARNING("error %d closing fid %llx\n", cerror, fid);
        }
    }
    
    if (!error) {
        lck_rw_lock_exclusive(&np->n_name_rwlock);

        new_name = smb_strndup(s_name, s_namlen, &new_name_allocsize);
        old_name = np->n_name;
        old_name_allocsize = np->n_name_allocsize;

        if (!(SS_TO_SESSION(share)->session_misc_flags & SMBV_HAS_FILEIDS) ||
            !(np->n_flag & NDELETEONCLOSE)) {
            /* 
             * If no File IDs OR if we deleted it now, then remove the node 
             * from hash table
             */
            smb_vhashrem(np);
        }
        
        /* Now reset the name, so other path lookups can use it. */
        if (new_name) {
            np->n_name = new_name;
            np->n_nmlen = s_namlen;
            np->n_name_allocsize = new_name_allocsize;
            
            /* Mark smb node so Meta data cache never expires */
            np->n_flag |= NMARKEDFORDLETE;
            
            if (!(SS_TO_SESSION(share)->session_misc_flags & SMBV_HAS_FILEIDS) &&
                !(np->n_flag & NDELETEONCLOSE)) {
                /* 
                 * If no File IDs AND the file has not been deleted yet, then 
                 * add it back into hash table.
                 */
                hashval = smbfs_hash(NULL, 0, np->n_name, np->n_nmlen);
                smb_vhashadd(np, hashval);
            }

            /* Now its safe to free the old name */
            SMB_FREE_DATA(old_name, old_name_allocsize);
        }

        lck_rw_unlock_exclusive(&np->n_name_rwlock);
    }
    else {
        /* All errors are mapped to EBUSY */
        error = EBUSY;
    }
    
    return(error);
}

/*
 * This routine will send a flush across the wire to the server. This is an expensive
 * operation that should only be done when the user request it. 
 *
 * The calling routine must hold a reference on the share
 *
 */ 
int 
smb1fs_smb_flush(struct smb_share *share, SMBFID fid, vfs_context_t context)
{
	struct smb_rq rq, *rqp = &rq;
	struct mbchain *mbp;
	int error;
    uint16_t smb1_fid = (uint16_t) fid;
	
	error = smb_rq_init(rqp, SSTOCP(share), SMB_COM_FLUSH, 0, context);
	if (error) {
		goto done;
	}
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_mem(mbp, (caddr_t)&smb1_fid, sizeof(smb1_fid), MB_MSYSTEM); 
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
done:
	if (error) {
		SMBWARNING("smbfs_smb_flush failed error = %d\n", error);
	}
	return (error);
}

/*
 * This routine will send a seteof across the wire to the server. 
 *
 * The calling routine must hold a reference on the share
 *
 */ 
int 
smb1fs_seteof(struct smb_share *share, SMBFID fid, uint64_t newsize,
              vfs_context_t context)
{
	struct mbchain *mbp;
	int error;
    uint16_t smb1_fid = (uint16_t) fid;
    struct smb_t2rq *t2p;

    error = smb_t2_alloc(SSTOCP(share), SMB_TRANS2_SET_FILE_INFORMATION, context, &t2p);
    if (error)
        return error;
    mbp = &t2p->t2_tparam;
    mb_init(mbp);
    mb_put_mem(mbp, (caddr_t)&smb1_fid, sizeof(smb1_fid), MB_MSYSTEM);
    if (SESSION_CAPS(SS_TO_SESSION(share)) & SMB_CAP_INFOLEVEL_PASSTHRU)
        mb_put_uint16le(mbp, SMB_SFILEINFO_END_OF_FILE_INFORMATION);
    else
        mb_put_uint16le(mbp, SMB_SFILEINFO_END_OF_FILE_INFO);
    mb_put_uint16le(mbp, 0);
    mbp = &t2p->t2_tdata;
    mb_init(mbp);
    mb_put_uint64le(mbp, newsize);
    t2p->t2_maxpcount = 2;
    t2p->t2_maxdcount = 0;
    error = smb_t2_request(t2p);
    smb_t2_done(t2p);

	return error;
}


/*
 * This routine will send an allocation across the wire to the server. 
 *
 * The calling routine must hold a reference on the share
 *
 */ 
int 
smb1fs_set_allocation(struct smb_share *share, SMBFID fid, uint64_t newsize,
                     vfs_context_t context)
{
	struct smb_t2rq *t2p;
	struct mbchain *mbp;
	int error;
    uint16_t smb1_fid = (uint16_t) fid;
		
	error = smb_t2_alloc(SSTOCP(share), SMB_TRANS2_SET_FILE_INFORMATION, context, &t2p);
	if (error)
		return error;
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	mb_put_mem(mbp, (caddr_t)&smb1_fid, sizeof(smb1_fid), MB_MSYSTEM);
	if (SESSION_CAPS(SS_TO_SESSION(share)) & SMB_CAP_INFOLEVEL_PASSTHRU)
		mb_put_uint16le(mbp, SMB_SFILEINFO_ALLOCATION_INFORMATION);
	else
		mb_put_uint16le(mbp, SMB_SFILEINFO_ALLOCATION_INFO);
	mb_put_uint16le(mbp, 0);
	mbp = &t2p->t2_tdata;
	mb_init(mbp);
	mb_put_uint64le(mbp, newsize);
	t2p->t2_maxpcount = 2;
	t2p->t2_maxdcount = 0;
	error = smb_t2_request(t2p);
	smb_t2_done(t2p);
	return error;
}

/*
 * Set the eof and clear and set the node flags required,
 *
 * The calling routine must hold a reference on the share
 *
 */
int 
smbfs_seteof(struct smb_share *share, struct smbnode *np, SMBFID fid, 
             uint64_t newsize, vfs_context_t context)
{
	int error;
	
	error = smbfs_smb_seteof(share, fid, newsize, context);
	if (error && (error != EBADF)) {
		/* Not a reconnect error then report it */
		SMBWARNING("smbfs_node_seteof failed error = %d\n", error);
	} else if (!error) {
		np->n_flag &= ~NNEEDS_EOF_SET;
		np->n_flag |= NNEEDS_FLUSH;
	}
	return error;
}

/*
 * See if we have a pending seteof or need to flush the file. 
 *
 * The calling routine must hold a reference on the share
 *
 * smbfs_smb_fsync flags
     enum {
         kFullSync = 0x01,
         kCloseFile = 0x02
     };
 */
int 
smbfs_smb_fsync(struct smb_share *share, struct smbnode *np, uint32_t flags,
				vfs_context_t context)
{
	int error = 0, i;
    
    SMB_LOG_KTRACE(SMB_DBG_SMB_FSYNC | DBG_FUNC_START, np->n_flag, flags, 0, 0, 0);

    if ((!SMBTOV(np)) || (!vnode_isreg(SMBTOV(np)))) {
        /* Paranoid check */
        SMBERROR_LOCK(np, "No vnode or not a file on %s \n",
                      np->n_name);
        np->n_flag &= ~(NNEEDS_FLUSH | NNEEDS_EOF_SET);
        error = EINVAL; /* No vnode or not a regular file */
        goto done;
    }
    
    if ((np->f_lockFID_refcnt == 0) &&
        (np->f_sharedFID_refcnt == 0)) {
        /*
         * There is one case where the file may not be currently open.
         * 1. vput->vnode_reclaim_internal->vgone->vclean can call our
         * vnop_fsync directly. but it does not check on the error return.
         *
         * If file is not open, no way to push out data or set eof, so just
         * return an error.
         */
        SMB_LOG_FILE_OPS_LOCK(np, "f_refcnt %d/%d == 0 on %s \n",
                              np->f_lockFID_refcnt, np->f_sharedFID_refcnt,
                              np->n_name);
        np->n_flag &= ~(NNEEDS_FLUSH | NNEEDS_EOF_SET);
        error = EINVAL; /* Why is the file not open? */
        goto done;
    }
	
	if (!(flags & kFullSync)) {
		/* Its just a normal fsync */
		if ((np->n_flag & (NNEEDS_EOF_SET | NNEEDS_FLUSH)) == 0) {
			/* Nothing to do here just return */
			error = 0;
			goto done;
		}
		
		/* Before trying the flush see if the file needs to be reopened or revoked */
		error = smbfs_smb_reopen_file(share, np, context);
		if (error) {
			SMBDEBUG_LOCK(np, " %s waiting to be revoked\n", np->n_name);
			goto done;
		}
	}
	
    /* For F_FULLSYNC, find any open FID to do send it on */
    if (flags & kFullSync) {
        if (np->f_lockFID_refcnt > 0) {
            error = smbfs_smb_flush(share, np->f_lockFID_fid, 1, context);
        }
        else {
            if (np->f_sharedFID_refcnt > 0) {
                error = smbfs_smb_flush(share, np->f_sharedFID_fid, 1, context);
            }
            else {
                SMBERROR_LOCK(np, "No open FIDs %d/%d found for F_FULLSYNC for <%s> \n",
                              np->f_lockFID_refcnt, np->f_sharedFID_refcnt,
                              np->n_name);
                error = EBADF;
            }
        }
        
        if (error) {
            SMBERROR_LOCK(np, "F_FULLSYNC failed <%d> on <%s> \n",
                          error, np->n_name);
        }
        
        SMB_LOG_KTRACE(SMB_DBG_SMB_FSYNC | DBG_FUNC_NONE, 0xabc001, error, 0, 0, 0);
        goto done;
    }
        
    /* Normal fsync, flush data on all FIDs that have write access */

    /* Check lockFID */
    if (np->f_lockFID_refcnt > 0) {
        if (np->f_lockFID_accessMode & kAccessWrite) {
            /*
              * If we have a set eof pending do it here.
              * smbfs_seteof() will clear the NNEEDS_EOF_SET flag if no error occurs
              */
             SMB_LOG_KTRACE(SMB_DBG_SMB_FSYNC | DBG_FUNC_NONE, 0xabc002, 0, 0, 0, 0);

             if (np->n_flag & NNEEDS_EOF_SET) {
                 error = smbfs_seteof(share, np, np->f_lockFID_fid, np->n_size, context);
                 if (error) {
                     /* If cant set EOF, skip trying to do flush */
                     goto done;
                 }
             }
#if 0
            /*
             * <99885765> Dont do Flush/Close as it can crash third party
             * servers.
             *
             * If its the last close and SMB2 or later, then defer the flush
             * to compound with the close request. Assuming the server is ok with a
             * compound Flush/Close requests.
             */
            if ((flags & kCloseFile) &&
                (np->f_lockFID_refcnt == 1) &&
                (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) &&
                !(SS_TO_SESSION(share)->session_misc_flags & SMBV_NO_CMPD_FLUSH_CLOSE)) {
                /* Assume no error with the later flush/close */
                error = 0;
            }
            else {
                /* Send the flush */
                error = smbfs_smb_flush(share, np->f_lockFID_fid, 0, context);
                if (error == 0) {
                    /* Normal flush worked, so clear flag */
                    np->n_flag &= ~NNEEDS_FLUSH;
                }
            }
#else
            /* Send the flush */
            error = smbfs_smb_flush(share, np->f_lockFID_fid, 0, context);
            if (error == 0) {
                /* Normal flush worked, so clear flag */
                np->n_flag &= ~NNEEDS_FLUSH;
            }
#endif
        }
        else {
            /*
             * If its a fsync and lockFID is open for read only, then
             * nothing to do here so just return with no error.
             *
             * Dont need flush because we could not write any data.
             * Dont need set EOF because NNEEDS_EOF_SET is only set in the
             * Write path.
             */
            np->n_flag &= ~(NNEEDS_FLUSH | NNEEDS_EOF_SET);
            error = 0;
        }
    }
    
    /* Check sharedFID */
    if (np->f_sharedFID_refcnt > 0) {
        if (np->f_sharedFID_accessMode & kAccessWrite) {
            /*
             * If we have a set eof pending do it here.
             * smbfs_seteof() will clear the NNEEDS_EOF_SET flag if no error occurs
             */
            SMB_LOG_KTRACE(SMB_DBG_SMB_FSYNC | DBG_FUNC_NONE, 0xabc003, 0, 0, 0, 0);

            if (np->n_flag & NNEEDS_EOF_SET) {
                error = smbfs_seteof(share, np, np->f_sharedFID_fid, np->n_size, context);
                if (error) {
                    /* If cant set EOF, skip trying to do flush */
                    goto done;
                }
            }
#if 0
            /*
             * <99885765> Dont do Flush/Close as it can crash third party
             * servers.
             *
             * If its the last close and SMB2 or later, then defer the flush
             * to compound with the close request. Assuming the server is ok with a
             * compound Flush/Close requests.
             */
            if ((flags & kCloseFile) &&
                (np->f_sharedFID_refcnt == 1) &&
                (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) &&
                !(SS_TO_SESSION(share)->session_misc_flags & SMBV_NO_CMPD_FLUSH_CLOSE)) {
                /* Assume no error with the later flush/close */
                error = 0;
            }
            else {
                /* Send the flush */
                error = smbfs_smb_flush(share, np->f_sharedFID_fid, 0, context);
                if (error == 0) {
                    /* Normal flush worked, so clear flag */
                    np->n_flag &= ~NNEEDS_FLUSH;
                }
            }
#else
            /* Send the flush */
            error = smbfs_smb_flush(share, np->f_sharedFID_fid, 0, context);
            if (error == 0) {
                /* Normal flush worked, so clear flag */
                np->n_flag &= ~NNEEDS_FLUSH;
            }
#endif

            /* Check the sharedFID BRL FIDs */
            for (i = 0; i < 3; i++) {
                if ((np->f_sharedFID_lockEntries[i].refcnt > 0) &&
                    (np->f_sharedFID_lockEntries[i].accessMode & kAccessWrite)) {
                    /* Set EOF was done earlier, just need Flush */
                    SMB_LOG_KTRACE(SMB_DBG_SMB_FSYNC | DBG_FUNC_NONE, 0xabc004, error, 0, 0, 0);

                    error = smbfs_smb_flush(share, np->f_sharedFID_lockEntries[i].fid, 0, context);
                    if (error == 0) {
                        /* On no error, clear NNEEDS_FLUSH flag */
                        np->n_flag &= ~NNEEDS_FLUSH;
                    }
                }
            }
        }
        else {
            /*
             * If its a fsync and sharedFID is open for read only, then
             * nothing to do here so just return with no error.
             *
             * Dont need flush because we could not write any data.
             * Dont need set EOF because NNEEDS_EOF_SET is only set in the
             * Write path.
             */
            np->n_flag &= ~(NNEEDS_FLUSH | NNEEDS_EOF_SET);
            error = 0;
        }
    }

done:
	SMB_LOG_KTRACE(SMB_DBG_SMB_FSYNC | DBG_FUNC_END, error, 0, 0, 0, 0);
    return error;
}

/*
 * We should replace it with something more modern. See <rdar://problem/7595213>. 
 * This routine is only used to test the existence of an item or to get its 
 * DOS attributes when changing the status of the HIDDEN bit.
 *
 * The calling routine must hold a reference on the share
 *
 * Eventually, smb1fs_smb_query_info will just go away and we wont have to worry
 * about it using an old call any more.
 */
int 
smb1fs_smb_query_info(struct smb_share *share, struct smbnode *np, 
                      const char *name, size_t len, uint32_t *in_attr, 
                      vfs_context_t context)
{
	struct smb_rq rq, *rqp = &rq;
	struct mbchain *mbp;
	struct mdchain *mdp;
	uint8_t wc = 0;
	int error;
    uint16_t attr;
	
	error = smb_rq_init(rqp, SSTOCP(share), SMB_COM_QUERY_INFORMATION, 0, context);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint8(mbp, SMB_DT_ASCII);
	error = smbfs_fullpath(mbp, np, name, &len, UTF_SFM_CONVERSIONS, 
						   SMB_UNICODE_STRINGS(SS_TO_SESSION(share)), '\\');
	if (!error) {
		smb_rq_bend(rqp);
		error = smb_rq_simple(rqp);
	}
	if (!error && in_attr) {
		smb_rq_getreply(rqp, &mdp);
		md_get_uint8(mdp, &wc);
		error = md_get_uint16le(mdp, &attr);
        *in_attr = attr;
	}
	smb_rq_done(rqp);
	return error;
}

/*
 * Set DOS file attributes, may want to replace with a more modern call
 *
 * The calling routine must hold a reference on the share
 *
 */
int 
smb1fs_smb_setpattr(struct smb_share *share, struct smbnode *np,
                    const char *name, size_t len,
                    uint16_t attr, vfs_context_t context)
{
	struct smb_rq rq, *rqp = &rq;
	struct mbchain *mbp;
	uint32_t time;
	int error;

	error = smb_rq_init(rqp, SSTOCP(share), SMB_COM_SET_INFORMATION, 0, context);
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
		error = smbfs_fullpath(mbp, np, name, &len, UTF_SFM_CONVERSIONS, 
							   SMB_UNICODE_STRINGS(SS_TO_SESSION(share)), '\\');
		if (error)
			break;
		if (SMB_UNICODE_STRINGS(SS_TO_SESSION(share))) {
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

/*
 * The calling routine must hold a reference on the share
 */
int 
smbfs_set_hidden_bit(struct smb_share *share, struct smbnode *dnp, enum vtype vnode_type,
                     const char *name, size_t len,
                     Boolean hideit, vfs_context_t context)
{
	int error;
	uint32_t attr;
	
	/* Look it up and get the dos attributes */
	error = smbfs_smb_query_info(share, dnp, vnode_type,
                                 name, len,
                                 &attr, context);
	if (error) {
		return error;
	}
    
	if (hideit && !(attr & SMB_EFA_HIDDEN)) {
		attr |= SMB_EFA_HIDDEN;
	} else if (!hideit && (attr & SMB_EFA_HIDDEN)) {
		attr &= ~SMB_EFA_HIDDEN;
	} else {
		return 0; /* Nothing to do here */
	}
    
	return smbfs_smb_setpattr(share, dnp, vnode_type,
                              name, len,
                              attr, context);
}

/*
 * The calling routine must hold a reference on the share
 */
int
smbfs_set_unix_info2(struct smb_share *share, struct smbnode *np, 
					 struct timespec *crtime, struct timespec *mtime, 
					 struct timespec *atime, uint64_t fsize,  uint64_t perms, 
					 uint32_t FileFlags, uint32_t FileFlagsMask, vfs_context_t context)
{
	struct smb_t2rq *t2p;
	struct mbchain *mbp;
	uint64_t tm;
	uint32_t ftype;
	int error;
	
	error = smb_t2_alloc(SSTOCP(share), SMB_TRANS2_SET_PATH_INFORMATION, context, &t2p);
	if (error)
		return error;
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	mb_put_uint16le(mbp, SMB_SFILEINFO_UNIX_INFO2);
	mb_put_uint32le(mbp, 0);		/* MBZ */
	error = smbfs_fullpath(mbp, np, NULL, NULL, UTF_SFM_CONVERSIONS, 
						   SMB_UNICODE_STRINGS(SS_TO_SESSION(share)), '\\');
	if (error) {
		smb_t2_done(t2p);
		return error;
	}
	
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
 * BASIC_INFO works with Samba, but Win2K servers say it is an invalid information
 * level on a SET_PATH_INFO.  Note Win2K does support *BASIC_INFO on a SET_FILE_INFO, 
 * and they support the equivalent *BASIC_INFORMATION on SET_PATH_INFO. Go figure.
 *
 * The calling routine must hold a reference on the share
 *
 */
int
smb1fs_smb_setpattrNT(struct smb_share *share, struct smbnode *np, 
                      uint32_t attr, struct timespec *crtime, 
                      struct timespec *mtime, struct timespec *atime, 
                      vfs_context_t context)
{
	struct smb_t2rq *t2p;
	struct mbchain *mbp;
	uint64_t tm;
	int error;

	error = smb_t2_alloc(SSTOCP(share), SMB_TRANS2_SET_PATH_INFORMATION, context, &t2p);
	if (error)
		return error;
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	if (SESSION_CAPS(SS_TO_SESSION(share)) & SMB_CAP_INFOLEVEL_PASSTHRU)
		mb_put_uint16le(mbp, SMB_SFILEINFO_BASIC_INFORMATION);
	else
		mb_put_uint16le(mbp, SMB_SFILEINFO_BASIC_INFO);
	mb_put_uint32le(mbp, 0);		/* MBZ */
	/* mb_put_uint8(mbp, SMB_DT_ASCII); specs incorrect */
	error = smbfs_fullpath(mbp, np, NULL, NULL, UTF_SFM_CONVERSIONS, 
						   SMB_UNICODE_STRINGS(SS_TO_SESSION(share)), '\\');
	if (error) {
		smb_t2_done(t2p);
		return error;
	}
	
	mbp = &t2p->t2_tdata;
	mb_init(mbp);

	/* set the creation time */
	tm = 0;
	if (crtime) {
		smb_time_local2NT(crtime, &tm, (share->ss_fstype == SMB_FS_FAT));
	}
	mb_put_uint64le(mbp, tm);
	
	/* set the access time */	
	tm = 0;
	if (atime) {
		smb_time_local2NT(atime, &tm, (share->ss_fstype == SMB_FS_FAT));
	}
	mb_put_uint64le(mbp, tm);

	/* set the write/modify time */	
	tm = 0;
	if (mtime) {
		smb_time_local2NT(mtime, &tm, (share->ss_fstype == SMB_FS_FAT));
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
 * Same as above except with a file hanlde. Note once we remove Windows 98 
 * support we can remove passing the node into this routine.
 *
 * The calling routine must hold a reference on the share
 *
 */
int
smb1fs_smb_setfattrNT(struct smb_share *share, uint32_t attr, SMBFID fid,
                      struct timespec *crtime, struct timespec *mtime,
                      struct timespec *atime, vfs_context_t context)
{
	struct smb_t2rq *t2p;
	struct mbchain *mbp;
	uint64_t tm;
	int error;
    uint16_t smb1_fid = (uint16_t) fid;

	error = smb_t2_alloc(SSTOCP(share), SMB_TRANS2_SET_FILE_INFORMATION, context, &t2p);
	if (error)
		return error;
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	mb_put_mem(mbp, (caddr_t)&smb1_fid, sizeof(smb1_fid), MB_MSYSTEM);
	if (SESSION_CAPS(SS_TO_SESSION(share)) & SMB_CAP_INFOLEVEL_PASSTHRU)
		mb_put_uint16le(mbp, SMB_SFILEINFO_BASIC_INFORMATION);
	else
		mb_put_uint16le(mbp, SMB_SFILEINFO_BASIC_INFO);
	mb_put_uint16le(mbp, 0);
	mbp = &t2p->t2_tdata;
	mb_init(mbp);
	
	/* set the creation time */	
	tm = 0;
	if (crtime) {
		smb_time_local2NT(crtime, &tm, (share->ss_fstype == SMB_FS_FAT));
	}
	mb_put_uint64le(mbp, tm);
	
	/* set the access time */	
	tm = 0;
	if (atime) {
		smb_time_local2NT(atime, &tm, (share->ss_fstype == SMB_FS_FAT));
	}
	mb_put_uint64le(mbp, tm);
	
	/* set the write/modify time */		
	tm = 0;
	if (mtime) {
		smb_time_local2NT(mtime, &tm, (share->ss_fstype == SMB_FS_FAT));
	}
	mb_put_uint64le(mbp, tm);
	
	/* We never allow anyone to set the change time */		
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
 * If disp is FILE_OPEN then this is an open attempt, and:
 *   If xattr then name is the stream to be opened at np,
 *   Else np should be opened.
 *   ...we won't touch *fidp,
 * Else this is a creation attempt, and:
 *   If xattr then name is the stream to create at np,
 *   Else name is the thing to create under directory np.
 *   ...we will return *fidp,
 *
 * The calling routine must hold a reference on the share
 *
 * Either pass in np which is the file/dir to open OR
 * pass in dnp and a name 
 *
 */
int 
smb1fs_smb_ntcreatex(struct smb_share *share, struct smbnode *np, 
                     uint32_t rights, uint32_t shareMode, enum vtype vt, 
                     SMBFID *fidp, const char *name, size_t in_nmlen, 
                     uint32_t disp, int xattr, struct smbfattr *fap, 
                     int do_create, vfs_context_t context)
{
	struct smb_rq rq, *rqp = &rq;
	int unix_info2 = ((UNIX_CAPS(share) & UNIX_QFILEINFO_UNIX_INFO2_CAP)) ? TRUE : FALSE;
	struct mbchain *mbp;
	struct mdchain *mdp;
	uint8_t wc;
	uint32_t lint, createopt, efa;
	uint64_t llint;
	int error;
	uint16_t fid = 0, *namelenp;
	size_t nmlen = in_nmlen;	/* Don't change the input name length, we need it for making the ino number */
    int need_close = 0;
    SMBFID temp_fid = 0;
    char *snamep = NULL;
    size_t sname_allocsize = 0;
    vnode_t par_vp = NULL;

    /*
     * NOTE: vt is the vnode type of the item being opened/created and it may
     * be different from the vnode type of np.
     * For example, np can be the directory to create the file in with the name
     * of "name" and a vt of VREG.
     */

	DBG_ASSERT(fap); /* Should never happen */
    
    if (np == NULL) {
        /* Should never happen */
        SMBERROR("np is null? \n");
        return(EINVAL);
    }
    
	bzero(fap, sizeof(*fap));
	nanouptime(&fap->fa_reqtime);
	error = smb_rq_init(rqp, SSTOCP(share), SMB_COM_NT_CREATE_ANDX, 0, context);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint8(mbp, 0xff);	/* secondary command */
	mb_put_uint8(mbp, 0);		/* MBZ */
	mb_put_uint16le(mbp, 0);	/* offset to next command (none) */
	mb_put_uint8(mbp, 0);		/* MBZ */
	namelenp = (uint16_t *)mb_reserve(mbp, sizeof(uint16_t));
	/*
	 * XP to W2K Server never sets the NTCREATEX_FLAGS_OPEN_DIRECTORY
	 * for creating nor for opening a directory. Samba ignores the bit.
	 *
	 * Request the extended reply to get maximal access
	 */
	mb_put_uint32le(mbp, NTCREATEX_FLAGS_EXTENDED);	/* NTCREATEX_FLAGS_* */
	mb_put_uint32le(mbp, 0);	/* FID - basis for path if not root */
	mb_put_uint32le(mbp, rights);
	mb_put_uint64le(mbp, 0);	/* "initial allocation size" */
	efa = (vt == VDIR) ? SMB_EFA_DIRECTORY : SMB_EFA_NORMAL;
	if (disp != FILE_OPEN && !xattr) {
		if (efa == SMB_EFA_NORMAL)
			efa |= SMB_EFA_ARCHIVE;
		if (name && (*name == '.'))
			efa |= SMB_EFA_HIDDEN;
	}
	mb_put_uint32le(mbp, efa);
	/*
	 * To rename an open file we need delete shared access. We currently always
	 * allow delete access.
	 */
	mb_put_uint32le(mbp, shareMode);
	mb_put_uint32le(mbp, disp);
	createopt = 0;
	if (disp != FILE_OPEN) {
		if (vt == VDIR)
			createopt |= NTCREATEX_OPTIONS_DIRECTORY;
		/* (other create options currently not useful) */
	}
	/*
	 * The server supports reparse points so open the item with a reparse point 
	 * and bypass normal reparse point processing for the file.
	 */
	if (share->ss_attributes & FILE_SUPPORTS_REPARSE_POINTS) {
		createopt |= NTCREATEX_OPTIONS_OPEN_REPARSE_POINT;
        
		if (np && (np->n_dosattr & SMB_EFA_OFFLINE)) {
		/*
		 * File has been moved to offline storage, do not open with a
		 * reparse point in this case.  See <rdar://problem/10836961>.
		 */
		createopt &= ~NTCREATEX_OPTIONS_OPEN_REPARSE_POINT;
		}
	}
    
	mb_put_uint32le(mbp, createopt);
	mb_put_uint32le(mbp, NTCREATEX_IMPERSONATION_IMPERSONATION); /* (?) */
	mb_put_uint8(mbp, 0);   /* security flags (?) */
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	do {
		uint16_t resourceType = 0;
		uint8_t sep = xattr ? ':' : '\\';
		/* Do they want to open the resource fork? */
		if ((np->n_vnode) && (vnode_isnamedstream(np->n_vnode)) && (!name) && (!xattr)) {
            lck_rw_lock_shared(&np->n_name_rwlock);
            snamep = smb_strndup(np->n_sname, np->n_snmlen, &sname_allocsize);
            name = snamep;
            lck_rw_unlock_shared(&np->n_name_rwlock);

			nmlen = np->n_snmlen;
			sep = ':';
		}
		if (name == NULL)
			nmlen = 0;
		error = smbfs_fullpath(mbp, np, name, &nmlen, UTF_SFM_CONVERSIONS, 
							   SMB_UNICODE_STRINGS(SS_TO_SESSION(share)), sep);
		if (error)
			break;
		*namelenp = htoles(nmlen); /* doesn't includes null */
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
        error = md_get_uint8(mdp, &wc);
        if (!error) {
            md_get_uint8(mdp, NULL);	/* secondary cmd */
            md_get_uint8(mdp, NULL);	/* mbz */
            md_get_uint16le(mdp, NULL);     /* andxoffset */
            md_get_uint8(mdp, NULL);	/* oplock lvl granted */
            error = md_get_uint16(mdp, &fid);       /* yes, leaving it LE */
            need_close = 1;
        }
        
        if (error) {
            error = EBADRPC;
            break;
        }
        
		if ( (wc != NTCREATEX_NORMAL_WDCNT) && (wc != NTCREATEX_EXTENDED_WDCNT) &&
                (wc != NTCREATEX_BRKEN_SPEC_26_WDCNT) ) {
            if (need_close == 1) {
                /* not much we can do if the close fails */
                temp_fid = fid;    /* cast SMB 1 fid to full smb fid */
                smbfs_smb_close(share, temp_fid, context);
            }
            
			error = EBADRPC;
			break;
		}
        
		md_get_uint32le(mdp, &fap->fa_created_disp);     /* create disposition */
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
		 * sure that we have the correct vnode type if its a file. For
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
		md_get_uint16le(mdp, &resourceType);     /* Resource Type */
		/*
		 * Never trust UNIX Servers when it comes to the FileStatus flags, they
		 * lie and always return a hard coded 7. We make an exception for Darwin
		 * servers, since they treat this field correctly.
		 */
		if ((resourceType == kFileTypeDisk) && 
			((!UNIX_SERVER(SS_TO_SESSION(share))) || (SS_TO_SESSION(share)->session_flags & SMBV_DARWIN))) {
			/* 
			 * If device type has NO_SUBSTREAMS then spec says: The file or directory 
			 * has no data streams other than the main data stream.
			 *
			 * If NO_EAS, then spec says: The file or directory has no extended 
			 * attributes.
			 */
			md_get_uint16le(mdp, &fap->fa_fstatus);	/* FileStatus Flags */
			fap->fa_valid_mask |= FA_FSTATUS_VALID;	/* Mark that this field is valid */
		} else {
			md_get_uint16le(mdp, NULL);     /* NMPipeStatus */
		}
		
		md_get_uint8(mdp, NULL);	/* directory (boolean) */
		/* 
		 * We want maximal access if we are opening up the node, if we have a 
		 * name then ignore. This means we will never update the stream node, 
		 * but currently we always use the main node for checking access.
		 */
		if (name)
			break; 
		
		/* Supports the maximal access rights, so lets get them */
		if (wc == NTCREATEX_EXTENDED_WDCNT) {
			int maxAccessRightsError;
			uint8_t VolumeGID[16];
			uint64_t fileID = 0;
			uint32_t guestMaxAccessRights = 0;
			
			md_get_mem(mdp, (caddr_t)VolumeGID, sizeof(VolumeGID), MB_MSYSTEM);
			md_get_uint64le(mdp, &fileID);   /* File ID */
			/* We only care about maximal access rights currently, so check for any errors */ 
			maxAccessRightsError = md_get_uint32le(mdp, &np->maxAccessRights);
			if (!maxAccessRightsError)
				maxAccessRightsError = md_get_uint32le(mdp, &guestMaxAccessRights);
			if (maxAccessRightsError) {
				np->n_flag |= NO_EXTENDEDOPEN;
				SMB_LOG_AUTH_LOCK(np, "Error %d getting extended reply for %s\n", maxAccessRightsError, np->n_name);
			} else {
				SMB_LOG_AUTH_LOCK(np, "%s fileID = %llx maxAccessRights = 0x%x guestMaxAccessRights = 0x%x\n",
                                  np->n_name, fileID, np->maxAccessRights, guestMaxAccessRights);
				np->n_flag &= ~NO_EXTENDEDOPEN;
                
				/*
				 * We weren't granted delete access, but the parent allows 
                 * delete child so say we have delete access on the item. If no 
                 * parent then just say we have delete access (let the server 
                 * control it).
				 */
                if ((np->maxAccessRights & SMB2_DELETE) != SMB2_DELETE) {
                    par_vp = smbfs_smb_get_parent(np, kShareLock);
                    if (par_vp != NULL) {
                        if (VTOSMB(par_vp)->maxAccessRights & SMB2_FILE_DELETE_CHILD) {
                            np->maxAccessRights |= SMB2_DELETE;
                        }
                        
                        vnode_put(par_vp);
                    }
                    else {
                        if (np->n_parent_vid != 0) {
                            /* Parent got recycled already? */
                            SMBWARNING_LOCK(np, "Missing parent for <%s> \n",
                                            np->n_name);
                        }
                        
                        /* Could not get parent, so assume Delete access */
                        np->maxAccessRights |= SMB2_DELETE;
                    }
                }
			}
		} else {
			np->n_flag |= NO_EXTENDEDOPEN;
		}
		/* 
		 * They don't support maximal access rights, so set it to all access rights
		 * and let the server handle any deny issues.
		 */
		if (np->n_flag & NO_EXTENDEDOPEN) {
			np->maxAccessRights = SA_RIGHT_FILE_ALL_ACCESS | STD_RIGHT_ALL_ACCESS;
			SMBDEBUG_LOCK(np, "Extended reply not supported: %s setting maxAccessRights to 0x%x rights = 0x%x\n",
                          np->n_name, np->maxAccessRights, rights);
		}
        np->maxAccessRightChTime = fap->fa_chtime;
	} while(0);
	smb_rq_done(rqp);
	if (error) {
        if (snamep) {
            SMB_FREE_DATA(snamep, sname_allocsize);
        }
        
		return error;
    }
	
	if (fidp) {
        /* note fid is in native form */
        *fidp = fid;       /* save SMB 1 fid */
    }
    
    /*
     * If not a directory, check if node needs to be reopened,
     * if so, then don't update anything at this point.
     * See <rdar://problem/11366143>.
     */
    if ((np) && (np->n_vnode) && !(vnode_isdir(np->n_vnode))) {
        lck_mtx_lock(&np->f_openStateLock);
        if (np->f_openState & kInReopen) {
            lck_mtx_unlock(&np->f_openStateLock);
            goto WeAreDone;
        }
        lck_mtx_unlock(&np->f_openStateLock);
    }
	
	if (xattr)	{
		/* If an EA or Stream then we are done */
		goto WeAreDone;
	}
	
	/* We are creating the item so create the ino number */
	if (do_create == TRUE) {
		DBG_ASSERT(name != NULL);	/* This is a create so better have a name */
		fap->fa_ino = smbfs_getino(np, name, in_nmlen); /* SMB 1 only */
		goto WeAreDone;			
	}

	/* If this is a SYMLINK, then n_vnode could be set to NULL */
	if (np->n_vnode == NULL) {
		goto WeAreDone;
	}
	/*
	 * We only get to this point if the n_vnode exist and we are doing a normal 
	 * open. If we are using UNIX extensions then we can't trust some of the 
	 * values returned from this open response. We need to reset some of the 
	 * value back to what we found in in the UNIX Info2 lookup.
	 */
	if (unix_info2) {
		/* Reset it to look like a UNIX Info2 lookup */
		fap->fa_unix = TRUE;
		fap->fa_flags_mask = EXT_REQUIRED_BY_MAC;
		fap->fa_nlinks = np->n_nlinks;
		/* 
		 * Samba servers will return the read only bit when the posix modes
		 * are set to read only. This is not the same as the immutable bit,
		 * so don't believe what they say here about the read only bit. Keep 
		 * the value we have store in the node we can  update from the unix
		 * info level.if
		 */
		fap->fa_attr &= ~SMB_EFA_RDONLY;
		fap->fa_attr |= (np->n_dosattr & SMB_EFA_RDONLY);
		fap->fa_valid_mask |= FA_VTYPE_VALID;
		fap->fa_vtype = vnode_vtype(np->n_vnode);
		/* Make sure we have the correct fa_attr setting */
		if (vnode_isdir(np->n_vnode))
			fap->fa_attr |= SMB_EFA_DIRECTORY;
		else
			fap->fa_attr &= ~SMB_EFA_DIRECTORY;
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
	smbfs_attr_cacheenter(share, np->n_vnode, fap, TRUE, context);

WeAreDone:
    if (snamep) {
        SMB_FREE_DATA(snamep, sname_allocsize);
    }
    
	return (0);
}

/*
 * smbfs_tmpopen() - this function is called when we need to send a request
 * to the server using a FID. This function will check to see if the lockFID
 * or sharedFID can be used to send the request and if so, then a refcnt is
 * taken on that FID. If the vnode is a dir or the lockFID or sharedFID can
 * not be used, then a new FID is opened to the server and used.
 *
 * The calling routine must hold a reference on the share
 */
int
smbfs_tmpopen(struct smb_share *share, struct smbnode *np, uint32_t rights,
              SMBFID *fidp, vfs_context_t context)
{
	int searchOpenFiles;
	int error = 0;
	struct smbfattr fattr;
	struct fileRefEntryBRL *fileBRLEntry;

	/* If no vnode or the vnode is a directory then don't use already open items */
    if (!(np->n_vnode) || (vnode_isdir(np->n_vnode))) {
        searchOpenFiles = FALSE;
    }
	else {
		/* Check to see if the file needs to be reopened or revoked */
		error = smbfs_smb_reopen_file(share, np, context);
		if (error) {
			SMBDEBUG_LOCK(np, " %s waiting to be revoked\n", np->n_name);
			return(error);
		}
        
		/*
		 * A normal open can have the following rights 
		 *	SMB2_READ_CONTROL - always set
		 *	SMB2_FILE_READ_DATA
		 *	SMB2_FILE_APPEND_DATA
		 *	SMB2_FILE_WRITE_DATA
		 *
		 * A normal open will never have the following rights 
		 *	SMB2_DELETE
		 *	SMB2_WRITE_DAC
		 *	SMB2_WRITE_OWNER
		 *	SMB2_FILE_WRITE_ATTRIBUTES
		 *	
		 */
        if (rights & (SMB2_DELETE | SMB2_WRITE_DAC | SMB2_WRITE_OWNER)) {
            searchOpenFiles = FALSE;
        }
        else {
            if (rights & SMB2_FILE_WRITE_ATTRIBUTES) {
                searchOpenFiles = FALSE;
            }
            else {
                searchOpenFiles = TRUE;
            }
        }
	}
		
	/* 
	 * Remember we could have been called before the vnode is create. Conrads
	 * crazy symlink code. So if we have no vnode then we cannot borrow the
	 * fid. Only borrow a fid if the requested access modes could have been
	 * made on an open call. 
	 */
	if (searchOpenFiles && SMBTOV(np)) {
		uint16_t accessMode = 0;
		
		if (rights & (SMB2_READ_CONTROL | SMB2_FILE_READ_DATA))
			accessMode |= kAccessRead;
		if (rights & (SMB2_FILE_APPEND_DATA | SMB2_FILE_WRITE_DATA))
			accessMode |= kAccessWrite;
			/* Must be a Windows 98 system */
		if (rights & SMB2_FILE_WRITE_ATTRIBUTES)
			accessMode |= kAccessWrite;
        
        /* Check the fileBRLEntry with the requested access, if we have one up the refcnt */
        fileBRLEntry = smbfs_find_lockEntry(np->n_vnode, accessMode);
        if (fileBRLEntry && (fileBRLEntry->refcnt > 0)
            && ((accessMode & fileBRLEntry->accessMode) == accessMode)) {
            fileBRLEntry->refcnt += 1;
            SMB_LOG_FILE_OPS_LOCK(np, "Incr f_fileBRLEntry_refcnt to <%d> on <%s> f_fileBRLEntry_fid 0x%llx\n",
                                  fileBRLEntry->refcnt, np->n_name, fileBRLEntry->fid);
            *fidp = fileBRLEntry->fid;
            return (0);
        }

        /* Check the fileBRLEntry with read/write access, if we have one up the refcnt */
        if (accessMode != (kAccessRead | kAccessWrite)) {
            fileBRLEntry = smbfs_find_lockEntry(np->n_vnode, kAccessRead | kAccessWrite);
            if (fileBRLEntry && (fileBRLEntry->refcnt > 0)
                && ((accessMode & fileBRLEntry->accessMode) == accessMode)) {
                fileBRLEntry->refcnt += 1;
                SMB_LOG_FILE_OPS_LOCK(np, "Incr f_fileBRLEntryRW_refcnt to <%d> on <%s> f_fileBRLEntryRW_fid 0x%llx\n",
                                      fileBRLEntry->refcnt, np->n_name, fileBRLEntry->fid);
                *fidp = fileBRLEntry->fid;
                return (0);
            }
        }

		/* Check the non deny mode opens, if we have one up the refcnt */
		if ((np->f_sharedFID_refcnt > 0)
            && ((accessMode & np->f_sharedFID_accessMode) == accessMode)) {
			np->f_sharedFID_refcnt += 1;
            SMB_LOG_FILE_OPS_LOCK(np, "Incr f_sharedFID_refcnt to <%d> on <%s> f_sharedFID_fid 0x%llx \n",
                                  np->f_sharedFID_refcnt, np->n_name, np->f_sharedFID_fid);
			*fidp = np->f_sharedFID_fid;
			return (0);
		}
        
		/* Now check the deny mode opens, if we find one up the refcnt */
		if ((np->f_lockFID_refcnt > 0)
            && ((accessMode & np->f_lockFID_accessMode) == accessMode)) {
            np->f_lockFID_refcnt += 1;
            SMB_LOG_FILE_OPS_LOCK(np, "Incr f_lockFID_refcnt to <%d> on <%s> f_lockFID_fid 0x%llx \n",
                                  np->f_lockFID_refcnt, np->n_name, np->f_lockFID_fid);
            *fidp = np->f_lockFID_fid;
			return (0);
		}
	}
    
    /*
     * Do we need to close any pending deferred closes?
     * Only need to check if a file, SMB2 and leases are supported.
     */
    if ((np->n_vnode) &&
        !(vnode_isdir(np->n_vnode)) &&
        (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) &&
        (SS_TO_SESSION(share)->session_sopt.sv_active_capabilities & SMB2_GLOBAL_CAP_LEASING)) {
        /* Close any pending deferred closes */
        CloseDeferredFileRefs(np->n_vnode, "smbfs_tmpopen", 0, context);
    }

	/*
	 * For temp opens we give unixy semantics of permitting everything not forbidden 
	 * by permissions.  Ie denial is up to server with clients/openers needing to use
	 * advisory locks for further control.
	 */
    uint32_t shareMode = NTCREATEX_SHARE_ACCESS_ALL;

    error = smbfs_smb_ntcreatex(share, np,
                                rights, shareMode, (np->n_vnode && vnode_isdir(np->n_vnode)) ? VDIR : VREG,
                                fidp, NULL, 0,
                                FILE_OPEN, FALSE, &fattr,
                                FALSE, NULL,
                                NULL, NULL,
                                context);
	if (error) {
		SMBWARNING_LOCK(np, "%s failed to open: error = %d\n", np->n_name, error);
    }
    SMB_LOG_FILE_OPS_LOCK(np, "New tmp open on <%s> fid 0x%llx \n",
                          np->n_name, *fidp);

	return (error);
}

static int
smbfs_tmpclose_fid(struct smb_share *share, struct smbnode *np, vfs_context_t context, int accessMode, int *refcnt)
{
    int openMode = 0;
    if (*refcnt == 1) {
        /*
         * Set Open Mode to do last close for the SMBFID
         */
        openMode = accessMode;

        return(smbfs_close(share, SMBTOV(np), openMode, context));
    }

    /*
     * We borrowed the SMBFID, just decrement the ref count
     * If its 0 (which would be odd), do nothing as its completely closed
     * Must be > 1 because we checked for == 1 above, thus decrement
     */
    if (*refcnt > 0) {
        *refcnt -= 1;
    }

    return 0;
}

/*
 * smbfs_tmpclose() - This function closes the FID that was opened using
 * smbfs_tmpopen(). If its dir, then just close the FID. If the FID does not
 * match the current lockFID or sharedFID, then just close it. If the FID
 * does match, then check the refcnts and either close the FID or decrement
 * the refcnt.
 *
 * NOTE: Is it possible for smbfs_tmpopen() to use the sharedFID and increment
 * its refcnt, then the sharedFID get upgraded/downgraded so that the tmp
 * FID no longer matches the sharedFID? This could mess up the refcnt for the
 * sharedFID. Not sure this could happen as the tmpopen/tmpclose should
 * all happen close together and under some type of vnode lock.
 *
 * The calling routine must hold a reference on the share
 */
int
smbfs_tmpclose(struct smb_share *share, struct smbnode *np, SMBFID fid, 
			   vfs_context_t context)
{
	vnode_t vp = SMBTOV(np);

    SMB_LOG_FILE_OPS_LOCK(np, "Enter on <%s> fid 0x%llx \n",
                          np->n_name, fid);

    /*
	 * Remember we could have been called before the vnode is created. Conrads 
	 * crazy symlink code. So if we have no vnode then we did not borrow the
	 * fid. If we did not borrow the fid then just close the fid and get out.
	 *
	 * If no vnode or the vnode is a directory then just close it, we are not
	 * sharing the open.
	 */
    if (!vp || vnode_isdir(vp)) {
        SMB_LOG_FILE_OPS_LOCK(np, "Calling smbfs_smb_close on <%s> fid 0x%llx \n",
                              np->n_name, fid);
        return(smbfs_smb_close(share, fid, context));
    }

    SMB_LOG_FILE_OPS_LOCK(np, "<%s> fid 0x%llx lockFID 0x%llx shareFID 0x%llx BRLReadWriteFID 0x%llx BRLReadFID 0x%llx BRLWriteFID 0x%llx\n",
                          np->n_name, fid, np->f_lockFID_fid, np->f_sharedFID_fid, np->f_sharedFID.lockEntries[0].fid, np->f_sharedFID.lockEntries[1].fid, np->f_sharedFID.lockEntries[2].fid);

    /*
	 * OK we borrowed the fid, do we have the last reference count on it? If
	 * yes, then we need to close up every thing. smbfs_close can handle this
	 * for us.
	 */
    if (fid == np->f_lockFID_fid) {
        return smbfs_tmpclose_fid(share, np, context, np->f_lockFID_accessMode, &np->f_lockFID_refcnt);
    } else if (fid == np->f_sharedFID_fid) {
        return smbfs_tmpclose_fid(share, np, context, np->f_sharedFID_accessMode, &np->f_sharedFID_refcnt);
    } else if (fid == np->f_sharedFID.lockEntries[0].fid) {
        return smbfs_tmpclose_fid(share, np, context, np->f_sharedFID.lockEntries[0].accessMode, &np->f_sharedFID.lockEntries[0].refcnt);
    } else if (fid == np->f_sharedFID.lockEntries[1].fid) {
        return smbfs_tmpclose_fid(share, np, context, np->f_sharedFID.lockEntries[1].accessMode, &np->f_sharedFID.lockEntries[1].refcnt);
    } else if (fid == np->f_sharedFID.lockEntries[2].fid) {
        return smbfs_tmpclose_fid(share, np, context, np->f_sharedFID.lockEntries[2].accessMode, &np->f_sharedFID.lockEntries[2].refcnt);
    } else {
        /* FID does not match lockFID, sharedFID or ByteRangeLockFIDs, so just close it */
        SMB_LOG_FILE_OPS_LOCK(np, "Calling smbfs_smb_close 2 on <%s> fid 0x%llx \n",
                              np->n_name, fid);
        return(smbfs_smb_close(share, fid, context));
    }
}

/*
 * This routine chains the open and read into one message. This routine is used only
 * for reading data out of a stream. If we decided to use it for something else then
 * we will need to make some changes.
 *
 * The calling routine must hold a reference on the share
 *
 */
int 
smb1fs_smb_openread(struct smb_share *share, struct smbnode *np, SMBFID *fid, 
				   uint32_t rights, uio_t uio, size_t *sizep, const char *name, 
				   struct timespec *mtime, vfs_context_t context)
{
	struct smb_rq rq, *rqp = &rq;
	struct mbchain *mbp;
	struct mdchain *mdp;
	uint8_t wc, cmd;
	int error = 0;
	uint16_t *namelenp, *nextWdCntOffset, nextOffset;
	uint64_t eof;
	uint16_t residhi, residlo, off, doff;
	uint32_t resid;
	uint32_t len = (uint32_t)uio_resid(uio);
	size_t nmlen = strnlen(name, share->ss_maxfilenamelen+1);
    uint16_t smb1_fid;

	/* 
	 * Make sure the whole response message will fit in our max buffer size. Since 
	 * we use the CreateAndX open call make sure the server supports that call. 
	 * The calling routine must handle this routine returning ENOTSUP.
	 */	
	if ((SS_TO_SESSION(share)->session_txmax - SMB_MAX_CHAIN_READ) < len)
		return(ENOTSUP);
	
	/* encode the CreateAndX request */
	error = smb_rq_init(rqp, SSTOCP(share), SMB_COM_NT_CREATE_ANDX, 0, context);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint8(mbp, SMB_COM_READ_ANDX);	/* next chain command will be a read */
	mb_put_uint8(mbp, 0);					/* MBZ */
	/* 
	 * The next command offset is the numbers of bytes from the smb header to 
	 * the location ofthe next commands word count field. Save that location so 
	 * we can fill it in later.
	 */
	nextWdCntOffset = (uint16_t *)mb_reserve(mbp, sizeof(uint16_t)); /* offset to next command */
	mb_put_uint8(mbp, 0);		/* MBZ */
	/* Save off the name length field so we can fill it in later */
	namelenp = (uint16_t *)mb_reserve(mbp, sizeof(uint16_t));

	mb_put_uint32le(mbp, 0);	/* Oplock?  NTCREATEX_FLAGS_REQUEST_OPLOCK */
	mb_put_uint32le(mbp, 0);	/* Root fid not used */
	mb_put_uint32le(mbp, rights);
	mb_put_uint64le(mbp, 0);	/* "initial allocation size" */
	mb_put_uint32le(mbp, SMB_EFA_NORMAL);
	/* Deny write access if they want write access */
	if (rights & SMB2_FILE_WRITE_DATA) {
		mb_put_uint32le(mbp, (NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_DELETE));
		mb_put_uint32le(mbp, FILE_OPEN_IF);		
	} else {
		mb_put_uint32le(mbp, NTCREATEX_SHARE_ACCESS_ALL);
		mb_put_uint32le(mbp, FILE_OPEN);
	}
	mb_put_uint32le(mbp, 0);
	mb_put_uint32le(mbp, NTCREATEX_IMPERSONATION_IMPERSONATION); 
	mb_put_uint8(mbp, 0);   /* security flags */
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	/* Put in the path name here */
	error = smbfs_fullpath(mbp, np, name, &nmlen, UTF_SFM_CONVERSIONS, 
						   SMB_UNICODE_STRINGS(SS_TO_SESSION(share)), ':');
	if (error)
		goto exit;

	*namelenp = htoles(nmlen); /* doesn't include the null bytes */
	smb_rq_bend(rqp);

	mb_put_padbyte(mbp); /* make sure the next message is on an even boundry */
	*nextWdCntOffset = htoles(mb_fixhdr(mbp));	
	
	/* now add the read request */
	smb_rq_wstart(rqp);	
	mb_put_uint8(mbp, 0xff); /* no secondary command */
	mb_put_uint8(mbp, 0);
	mb_put_uint16le(mbp, 0); /* offset to secondary, no more chain items */
	mb_put_uint16le(mbp, 0); /* set fid field to zero the server fills this in */

	mb_put_uint32le(mbp, (uint32_t)uio_offset(uio)); /* Lower offset */
	mb_put_uint16le(mbp, (uint16_t)len); /* MaxCount */
	mb_put_uint16le(mbp, (uint16_t)len); /* MinCount (only indicates blocking) */
	mb_put_uint32le(mbp, len >> 16); /* MaxCountHigh */
	mb_put_uint16le(mbp, (uint16_t)len); /* Remaining ("obsolete") */
	mb_put_uint32le(mbp, (uint32_t)(uio_offset(uio) >> 32)); /* high offset */
	
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
	 * remember exactly why (Not Windows Systems). Shouldn't matter to us because 
	 * the offset to the read message will eat any extra bytes.
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
	md_get_uint16(mdp, &smb1_fid);       /* Get the fid */
    *fid = smb1_fid;       /* save SMB 1 fid */
	md_get_uint32le(mdp, NULL);     /* create_action */
	md_get_uint64le(mdp, NULL);   /* creation time */
	md_get_uint64le(mdp, NULL);   /* access time */
	if (mtime) {
		uint64_t llint;
		
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
		if ((rights & SMB2_FILE_WRITE_DATA) && fid)	/* We created the file */
			error = 0;
		else 
			error = ENOENT;
		goto exit;
	}
	
	off = nextOffset;
	/* Is the offset pass the end of our buffer? */
    m_fixhdr(mdp->md_top);
	if (nextOffset > mbuf_get_chain_len(mdp->md_top)) {
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
	
exit:
	smb_rq_done(rqp);
	return (error);
}

int 
smb1fs_smb_open_maxaccess(struct smb_share *share, struct smbnode *dnp,
                          const char *namep, size_t name_len,
                          SMBFID *fidp, uint32_t *max_accessp,
                          vfs_context_t context)
{
#pragma unused(name_len)
	struct smb_rq rq, *rqp = &rq;
	struct mbchain *mbp;
	struct mdchain *mdp;
	uint8_t wc;
	int error = 0;
	uint16_t *namelenp, nextOffset;
	size_t nmlen = 0;
    uint16_t smb1_fid;
    
    if (namep != NULL) {
        nmlen = strnlen(namep, share->ss_maxfilenamelen+1);
    }
    
    /* Non zero fid means the open worked and a close is needed */
    *fidp = 0;
    
	/* encode the CreateAndX request */
	error = smb_rq_init(rqp, SSTOCP(share), SMB_COM_NT_CREATE_ANDX, 0, context);
	if (error) {
		return error;
    }
    
	smb_rq_getrequest(rqp, &mbp);
    
	smb_rq_wstart(rqp);
	mb_put_uint8(mbp, 0xff);                /* secondary command */
	mb_put_uint8(mbp, 0);                   /* MBZ */
	mb_put_uint16le(mbp, 0);                /* offset to next command (none) */
	mb_put_uint8(mbp, 0);                   /* MBZ */
	/* Save off the name length field so we can fill it in later */
	namelenp = (uint16_t *)mb_reserve(mbp, sizeof(uint16_t));
	mb_put_uint32le(mbp, NTCREATEX_FLAGS_EXTENDED);	/* NTCREATEX_FLAGS_* */
	mb_put_uint32le(mbp, 0);                /* Root fid not used */
	mb_put_uint32le(mbp, SMB2_FILE_READ_DATA);
	mb_put_uint64le(mbp, 0);                /* "initial allocation size" */
	mb_put_uint32le(mbp, SMB_EFA_NORMAL);
    mb_put_uint32le(mbp, NTCREATEX_SHARE_ACCESS_ALL);
    mb_put_uint32le(mbp, FILE_OPEN);
	mb_put_uint32le(mbp, 0);
	mb_put_uint32le(mbp, NTCREATEX_IMPERSONATION_IMPERSONATION); 
	mb_put_uint8(mbp, 0);                   /* security flags */
	smb_rq_wend(rqp);
    
	smb_rq_bstart(rqp);
    	/* Put in the path name with stream name here */
	error = smbfs_fullpath_stream(mbp, dnp,
                                  namep, NULL,
                                  &nmlen, 0, UTF_SFM_CONVERSIONS,
                                  SMB_UNICODE_STRINGS(SS_TO_SESSION(share)), '\\');
	if (error)
		goto exit;
    
	*namelenp = htoles(nmlen);              /* doesn't include the null bytes */
	smb_rq_bend(rqp);
    
	/* Send the message */
	error = smb_rq_simple(rqp);
	if (error)
		goto exit;
    
	smb_rq_getreply(rqp, &mdp);
	
	/*
	 * I know there are some servers that return word counts of 42, but I don't
	 * remember exactly why (Not Windows Systems). Shouldn't matter to us because 
	 * the offset to the read message will eat any extra bytes.
     */
	if (md_get_uint8(mdp, &wc) != 0 || 
        (wc != NTCREATEX_NORMAL_WDCNT && wc != NTCREATEX_EXTENDED_WDCNT)) {
		error = EINVAL;
		goto exit;
	}
    
	md_get_uint8(mdp, NULL);            /* secondary cmd */
    
	md_get_uint8(mdp, NULL);            /* mbz */
	/* Contains the offset from the start of the message to the read message. */
	md_get_uint16le(mdp, &nextOffset);  /* andxoffset */
	md_get_uint8(mdp, NULL);            /* oplock lvl granted */
	md_get_uint16(mdp, &smb1_fid);      /* Get the fid */
    *fidp = smb1_fid;                   /* save SMB 1 fid */
	md_get_uint32le(mdp, NULL);         /* create_action */
	md_get_uint64le(mdp, NULL);         /* creation time */
	md_get_uint64le(mdp, NULL);         /* access time */
    md_get_uint64le(mdp, NULL);         /* write time */
    
	md_get_uint64le(mdp, NULL);         /* change time */
	md_get_uint32le(mdp, NULL);         /* attributes */
	md_get_uint64le(mdp, NULL);         /* allocation size */
	md_get_uint64le(mdp, NULL);         /* EOF */
	md_get_uint16le(mdp, NULL);         /* file type */
	md_get_uint16le(mdp, NULL);         /* device state */
	md_get_uint8(mdp, NULL);            /* directory (boolean) */
    
    /* Supports the maximal access rights, so lets get them */
    if (wc == NTCREATEX_EXTENDED_WDCNT) {
        int maxAccessRightsError;
        uint8_t VolumeGID[16];
        uint64_t fileID = 0;
        uint32_t guestMaxAccessRights = 0;
        uint32_t max_access_rights = 0;
        
        md_get_mem(mdp, (caddr_t)VolumeGID, sizeof(VolumeGID), MB_MSYSTEM);
        md_get_uint64le(mdp, &fileID);   /* File ID */
        
        /* 
         * We only care about maximal access rights currently, 
         * so check for any errors 
         */ 
        maxAccessRightsError = md_get_uint32le(mdp, &max_access_rights);
        if (!maxAccessRightsError) {
            maxAccessRightsError = md_get_uint32le(mdp, &guestMaxAccessRights);
        }
        
        if (maxAccessRightsError) {
            *max_accessp = SA_RIGHT_FILE_ALL_ACCESS | STD_RIGHT_ALL_ACCESS;
        } 
        else {
            /*
             * We weren't granted delete access, but the parent allows delete child
             * so say we have delete access on the item. If no parent then just
             * say we have delete access (let the server control it).
             */
            if (((max_access_rights & SMB2_DELETE) != SMB2_DELETE) &&
                (!dnp || (dnp->maxAccessRights & SMB2_FILE_DELETE_CHILD))) {
                max_access_rights |= SMB2_DELETE;
            }
            
            *max_accessp = max_access_rights;
        }
    }
    else {
        /* Must not support max access rights, so grant full access */
        SMBDEBUG("Server does not support max access, returning full access\n");
        *max_accessp = SA_RIGHT_FILE_ALL_ACCESS | STD_RIGHT_ALL_ACCESS;
    }

exit:
	smb_rq_done(rqp);
	return (error);
}

int 
smb1fs_smb_open_read(struct smb_share *share, struct smbnode *dnp,
                     const char *namep, size_t name_len,
                     const char *strm_namep, size_t strm_name_len,
                     SMBFID *fidp, uio_t uio, size_t *sizep,
                     uint32_t *max_accessp,
                     vfs_context_t context)
{
#pragma unused(name_len)
#pragma unused(strm_name_len)
	struct smb_rq rq, *rqp = &rq;
	struct mbchain *mbp;
	struct mdchain *mdp;
	uint8_t wc, cmd;
	int error = 0;
    int error2 = 0;
	uint16_t *namelenp, *nextWdCntOffset, nextOffset;
	uint64_t eof;
	uint16_t residhi, residlo, off, doff;
	uint32_t resid;
	uint32_t len = (uint32_t)uio_resid(uio);
	size_t nmlen = strnlen(namep, share->ss_maxfilenamelen+1);
	size_t snmlen = strnlen(strm_namep, share->ss_maxfilenamelen+1);
    uint16_t smb1_fid;
    uint32_t rights = SMB2_FILE_READ_DATA;

	/* 
	 * Make sure the whole response message will fit in our max buffer size. Since 
	 * we use the CreateAndX open call make sure the server supports that call. 
	 * The calling routine must handle this routine returning ENOTSUP.
	 */	
	if ((SS_TO_SESSION(share)->session_txmax - SMB_MAX_CHAIN_READ) < len)
		return(ENOTSUP);
	
	/* encode the CreateAndX request */
	error = smb_rq_init(rqp, SSTOCP(share), SMB_COM_NT_CREATE_ANDX, 0, context);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint8(mbp, SMB_COM_READ_ANDX);	/* next chain command will be a read */
	mb_put_uint8(mbp, 0);					/* MBZ */
	/* 
	 * The next command offset is the numbers of bytes from the smb header to 
	 * the location ofthe next commands word count field. Save that location so 
	 * we can fill it in later.
	 */
	nextWdCntOffset = (uint16_t *)mb_reserve(mbp, sizeof(uint16_t)); /* offset to next command */
	mb_put_uint8(mbp, 0);		/* MBZ */
	/* Save off the name length field so we can fill it in later */
	namelenp = (uint16_t *)mb_reserve(mbp, sizeof(uint16_t));
    
    if (max_accessp) {
        /* Ask for max access */
        mb_put_uint32le(mbp, NTCREATEX_FLAGS_EXTENDED);	/* NTCREATEX_FLAGS_* */
    }
    else {
        mb_put_uint32le(mbp, 0);                        /* NTCREATEX_FLAGS_* */
    }

	mb_put_uint32le(mbp, 0);	/* Root fid not used */
	mb_put_uint32le(mbp, rights);
	mb_put_uint64le(mbp, 0);	/* "initial allocation size" */
	mb_put_uint32le(mbp, SMB_EFA_NORMAL);
	/* Deny write access if they want write access */
	if (rights & SMB2_FILE_WRITE_DATA) {
		mb_put_uint32le(mbp, (NTCREATEX_SHARE_ACCESS_READ | NTCREATEX_SHARE_ACCESS_DELETE));
		mb_put_uint32le(mbp, FILE_OPEN_IF);		
	} else {
		mb_put_uint32le(mbp, NTCREATEX_SHARE_ACCESS_ALL);
		mb_put_uint32le(mbp, FILE_OPEN);
	}
	mb_put_uint32le(mbp, 0);
	mb_put_uint32le(mbp, NTCREATEX_IMPERSONATION_IMPERSONATION); 
	mb_put_uint8(mbp, 0);   /* security flags */
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	/* Put in the path name with stream name here */
	error = smbfs_fullpath_stream(mbp, dnp,
                                  namep, strm_namep,
                                  &nmlen, snmlen, UTF_SFM_CONVERSIONS,
                                  SMB_UNICODE_STRINGS(SS_TO_SESSION(share)), '\\');
	if (error)
		goto exit;
    
	*namelenp = htoles(nmlen); /* doesn't include the null bytes */
	smb_rq_bend(rqp);
    
	mb_put_padbyte(mbp); /* make sure the next message is on an even boundry */
	*nextWdCntOffset = htoles(mb_fixhdr(mbp));	
	
	/* now add the read request */
	smb_rq_wstart(rqp);	
	mb_put_uint8(mbp, 0xff); /* no secondary command */
	mb_put_uint8(mbp, 0);
	mb_put_uint16le(mbp, 0); /* offset to secondary, no more chain items */
	mb_put_uint16le(mbp, 0); /* set fid field to zero the server fills this in */
    
	mb_put_uint32le(mbp, (uint32_t)uio_offset(uio)); /* Lower offset */
	mb_put_uint16le(mbp, (uint16_t)len); /* MaxCount */
	mb_put_uint16le(mbp, (uint16_t)len); /* MinCount (only indicates blocking) */
	mb_put_uint32le(mbp, len >> 16); /* MaxCountHigh */
	mb_put_uint16le(mbp, (uint16_t)len); /* Remaining ("obsolete") */
	mb_put_uint32le(mbp, (uint32_t)(uio_offset(uio) >> 32)); /* high offset */
	
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	smb_rq_bend(rqp);
	/* Send the message */
	error = smb_rq_simple(rqp);
	if (error) {
		/* 
         * Dont exit yet, may still be able to parse the Create part out and
         * get the fid so we can do a close
         */
    }
	smb_rq_getreply(rqp, &mdp);
	
	/*
	 * I know there are some servers that return word counts of 42, but I don't
	 * remember exactly why (Not Windows Systems). Shouldn't matter to us because 
	 * the offset to the read message will eat any extra bytes.
     */
	if (md_get_uint8(mdp, &wc) != 0 || 
        (wc != NTCREATEX_NORMAL_WDCNT && wc != NTCREATEX_EXTENDED_WDCNT)) {
        /* Dont overwrite earlier error */
        if (error == 0) {
            error = EINVAL;
        }
		goto exit;
	}
    
	error2 = md_get_uint8(mdp, &cmd);       /* AndX Command */
    if (error2) {
        /* Dont overwrite earlier error */
        if (error == 0) {
            error = error2;
        }
		goto exit;
    }
    
	error2 = md_get_uint8(mdp, NULL);       /* AndX Reserved */
    if (error2) {
        /* Dont overwrite earlier error */
        if (error == 0) {
            error = error2;
        }
		goto exit;
    }

	error2 = md_get_uint16le(mdp, &nextOffset); /* AndX Offset */
    if (error2) {
        /* Dont overwrite earlier error */
        if (error == 0) {
            error = error2;
        }
		goto exit;
    }
    
	error2 = md_get_uint8(mdp, NULL);       /* OpLock Level */
    if (error2) {
        /* Dont overwrite earlier error */
        if (error == 0) {
            error = error2;
        }
		goto exit;
    }

	error2 = md_get_uint16(mdp, &smb1_fid); /* FID */
    if (error2) {
        /* Dont overwrite earlier error */
        if (error == 0) {
            error = error2;
        }
		goto exit;
    }
    *fidp = smb1_fid;       /* save SMB 1 fid */

	if (error) {
        /* Now have FID so can do a close, now we can return with error */
		goto exit;
    }
    
    md_get_uint32le(mdp, NULL);     /* create_action */
	md_get_uint64le(mdp, NULL);     /* creation time */
	md_get_uint64le(mdp, NULL);     /* access time */
    md_get_uint64le(mdp, NULL);     /* write time */
    
	md_get_uint64le(mdp, NULL);     /* change time */
	md_get_uint32le(mdp, NULL);     /* attributes */
	md_get_uint64le(mdp, NULL);     /* allocation size */
	md_get_uint64le(mdp, &eof);     /* EOF */
	if (sizep) 
		*sizep = (size_t)eof;		/* The calling routines can only handle size_t */
	md_get_uint16le(mdp, NULL);     /* file type */
	md_get_uint16le(mdp, NULL);     /* device state */
	md_get_uint8(mdp, NULL);        /* directory (boolean) */
    
    /* Supports the maximal access rights, so lets get them */
    if (wc == NTCREATEX_EXTENDED_WDCNT) {
        int maxAccessRightsError;
        uint8_t VolumeGID[16];
        uint64_t fileID = 0;
        uint32_t guestMaxAccessRights = 0;
        uint32_t max_access_rights = 0;
        
        md_get_mem(mdp, (caddr_t)VolumeGID, sizeof(VolumeGID), MB_MSYSTEM);
        md_get_uint64le(mdp, &fileID);   /* File ID */
        
        /* We only care about maximal access rights currently, so check for any errors */ 
        maxAccessRightsError = md_get_uint32le(mdp, &max_access_rights);
        if (!maxAccessRightsError) {
            maxAccessRightsError = md_get_uint32le(mdp, &guestMaxAccessRights);
        }
        
        if (maxAccessRightsError) {
            if (max_accessp) {
                *max_accessp = SA_RIGHT_FILE_ALL_ACCESS | STD_RIGHT_ALL_ACCESS;
            }
        } 
        else {
            /*
             * We weren't granted delete access, but the parent allows delete child
             * so say we have delete access on the item. If no parent then just
             * say we have delete access (let the server control it).
             */
            if (((max_access_rights & SMB2_DELETE) != SMB2_DELETE) &&
                (!dnp || (dnp->maxAccessRights & SMB2_FILE_DELETE_CHILD))) {
                max_access_rights |= SMB2_DELETE;
            }
            
            if (max_accessp) {
                *max_accessp = max_access_rights;
            }
        }
    }
    else {
        /* Must not support max access rights, so grant full access */
        if (max_accessp) {
            SMBDEBUG("Server does not support max access, returning full access\n");
            *max_accessp = SA_RIGHT_FILE_ALL_ACCESS | STD_RIGHT_ALL_ACCESS;
        }
    }
    
    if (cmd != SMB_COM_READ_ANDX) {
		if ((rights & SMB2_FILE_WRITE_DATA) && fidp)	/* We created the file */
			error = 0;
		else 
			error = ENOENT;
		goto exit;
	}
	
	off = nextOffset;
	/* Is the offset pass the end of our buffer? */
    m_fixhdr(mdp->md_top);
	if (nextOffset > mbuf_get_chain_len(mdp->md_top)) {
		error = EINVAL;
		goto exit;
	}
    
	/* Take off what we have already consumed. */
    if (wc == NTCREATEX_EXTENDED_WDCNT) {
        /* Extended Create response */
        nextOffset -= SMB_HDRLEN;
        nextOffset -= 69;       /* Ext Create Response length */
        nextOffset -= 32;       /* Max Access Rights len */
    }
    else {
        /* Non Extended Create response */
        md_get_uint16(mdp, NULL);        /* Byte Count */

        nextOffset -= (SMB_HDRLEN + SMB_CREATEXRLEN + SMB_BCOUNT_LEN);
    }
    
	if (nextOffset != 0) {
        /* Anything left dump it */
		md_get_mem(mdp, NULL, nextOffset, MB_MSYSTEM);
    }
    
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
	
exit:
	smb_rq_done(rqp);
	return (error);
}

/*
 * The calling routine must hold a reference on the share
 */
int 
smbfs_smb_open_file(struct smb_share *share, struct smbnode *np, 
                    uint32_t rights, uint32_t shareMode, SMBFID *fidp, 
                    const char *name, size_t nmlen, int xattr, 
                    struct smbfattr *fap, struct smb2_dur_hndl_and_lease *dur_hndl_leasep,
                    vfs_context_t context)
{
	int error;
	int do_create;
    uint32_t disp;
		
    /* 
     * We are opening the resource fork from a normal open, so it should 
     * always exist. Tell the server to create it if it doesn't exist otherwise
     * just open it. We never create it if coming from an xattr call.
     */
    if ((np->n_flag & N_ISRSRCFRK) && !xattr) {
        disp = FILE_OPEN_IF;
        do_create = TRUE;
    } else {
        disp = FILE_OPEN;
        do_create = FALSE;
    }
    error = smbfs_smb_ntcreatex(share, np,
                                rights, shareMode, VREG,
                                fidp, name, nmlen,
                                disp, xattr, fap,
                                do_create, dur_hndl_leasep,
                                NULL, NULL,
                                context);
	return (error);
}

/* 
 * The calling routine must hold a reference on the share
 */
int
smbfs_smb_open_xattr(struct smb_share *share, struct smbnode *np, uint32_t rights,
                     uint32_t shareMode, SMBFID *fidp, const char *name, 
                     size_t *sizep, vfs_context_t context)
{
	size_t nmlen = strnlen(name, share->ss_maxfilenamelen+1);
	int error;
	struct smbfattr fattr;

	error = smbfs_smb_open_file(share, np, rights, shareMode, fidp, 
								name, nmlen, TRUE,
                                &fattr, NULL,
                                context);
	if (!error && sizep)
		*sizep = (size_t)fattr.fa_size;
	return(error);
}

/* 
 * The calling routine must hold a reference on the share
 * Attempts to reopen the shared fid only.
 */
int 
smbfs_smb_reopen_file(struct smb_share *share, struct smbnode *np, 
                      vfs_context_t context)
{
    int error = 0;
    struct smbfattr *fap = NULL;

    /* 
    * We are in the middle of a reconnect, wait for it to complete 
    * Although if its the iod_context, allow this reopen to proceed
    */
    if (context != SS_TO_SESSION(share)->session_iod->iod_context) {
        while (share->ss_flags & SMBS_RECONNECTING) {
            SMBDEBUG("SMBS_RECONNECTING Going to sleep! \n");
            msleep(&share->ss_flags, 0, PWAIT, "smbfs_smb_reopen_file", NULL);
        }
    }

    lck_mtx_lock(&np->f_openStateLock);
    
    /* File was already revoked, just return the correct error */
    if (np->f_openState & kNeedRevoke) {
        lck_mtx_unlock(&np->f_openStateLock);
        return EIO;
    } else if (!(np->f_openState & kNeedReopen) &&
               !(np->f_openState & kInReopen)) {
        /*
         * Note smb2fs_reconnect() resets kNeedReopen before making this call
         * so it can detect if another reconnect happened while trying to reopen
         * the file. But this inadvertently prevents the file from being opened,
         * since we would simply return here.
         *
         * Luckily smb2fs_reconnect() sets kInReopen at the same time,
         * so now we check both kNeedReopen and kInReopen to decide
         * whether to just return here or try to open the file below.
         * Fixes <rdar://problem/17510490>.
         */
        lck_mtx_unlock(&np->f_openStateLock);
        return 0;	/* Nothing wrong so just return */
    }

	/*
     * Clear the kNeedReopen flag, this way we know if a
     * reconnect happens during reopen
     */
	np->f_openState &= ~kNeedReopen;
    np->f_openState |= kInReopen;
    lck_mtx_unlock(&np->f_openStateLock);

    SMB_MALLOC_TYPE(fap, struct smbfattr, Z_WAITOK_ZERO);
    if (fap == NULL) {
        SMBERROR("SMB_MALLOC_TYPE failed\n");
        return ENOMEM;
    }

    /*
     * For SMB 1, if O_EXLOCK/O_SHLOCK was used, byte range locks were present,
     * or flock was used, then the open file got revoked and thus no need for
     * reopen. This means that only files that used the sharedFID can be
     * attempted to be reopened.
     *
     * For SMB 2/3, all open files should have durable handles so either the
     * file got restored or it got revoked. If its sharedFID and no durable
     * handle is found and no byte range locks are present, then we will fall
     * back to trying to reopen it in reconnect.
     *
     * Thus, we ONLY reopen SMB v1 sharedFID files! No need for leases or
     * durable handles
     */
    
    /* POSIX Open: Reopen with the same modes we had it open with before the reconnect */
    
    if (context != SS_TO_SESSION(share)->session_iod->iod_context) {
        error = smbfs_smb_open_file(share, np,
                                    np->f_sharedFID_rights, NTCREATEX_SHARE_ACCESS_ALL, &np->f_sharedFID_fid,
                                    NULL, 0, FALSE,
                                    fap, NULL,
                                    context);
    }
    else {
        /* In reconnect reopening the shared fork */
        error = smbfs_smb_ntcreatex(share, np,
                                    np->f_sharedFID_rights, NTCREATEX_SHARE_ACCESS_ALL, VREG,
                                    &np->f_sharedFID_fid, NULL, 0,
                                    FILE_OPEN, FALSE, fap,
                                    FALSE, NULL,
                                    NULL, NULL,
                                    context);
    }
    
    if (error) {
        /* If an error then we are done, nothing else to do */
        SMBERROR_LOCK(np, "Reopen %s failed because the open call failed <%d>!\n",
                      np->n_name, error);
    }

    /*
     * There used to be code here to put a flock() back into place by just
     * doing the lock operation again if the file mod time/size has not
     * changed. That doesnt make sense, since the flock() was put in place to
     * keep others from changing the file, but while we were disconnected,
     * some one else could have gotten in and changed the file without
     * changing the mod time (second granularity or size). Just putting
     * the flock() back in place does not mean the file is still unchanged.
     * smb1fs_reconnect() now just revokes the file if there was a flock() just
     * like if there was a O_EXLOCK/O_SHLOCK or byte range lock was present.
     */

    lck_mtx_lock(&np->f_openStateLock);
    
    np->f_openState &= ~kInReopen;

    /* Error or we reconnected after the open, not much we can do here */
    if (error || (np->f_openState & kNeedReopen)) {
        /* Mark the file to be revoked */
        SMBERROR_LOCK(np, "Warning: reopening of %s failed with error %d\n",
                      np->n_name, error);

        np->f_openState |= kNeedRevoke;
        error = EIO;
    }
    
    lck_mtx_unlock(&np->f_openStateLock);

    if (fap != NULL) {
        SMB_FREE_TYPE(struct smbfattr, fap);
    }

    return(error);
}

/* 
 * The calling routine must hold a reference on the share
 */
int
smb1fs_smb_close(struct smb_share *share, SMBFID fid, vfs_context_t context)
{
	struct smb_rq rq, *rqp = &rq;
	struct mbchain *mbp;
	uint32_t time;
	int error;
    uint16_t smb1_fid = (uint16_t) fid;
    
	DBG_ASSERT(smb1_fid);
	error = smb_rq_init(rqp, SSTOCP(share), SMB_COM_CLOSE, 0, context);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_mem(mbp, (caddr_t)&smb1_fid, sizeof(smb1_fid), MB_MSYSTEM);
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

/* 
 * The calling routine must hold a reference on the share
 */
int
smbfs_smb_create(struct smb_share *share, struct smbnode *dnp, 
                 const char *in_name, size_t in_nmlen, uint32_t rights, 
                 SMBFID *fidp, uint32_t disp, int xattr, struct smbfattr *fap, 
                 vfs_context_t context)
{
	const char *name = in_name;
	size_t nmlen = in_nmlen;
    SMBFID fid = 0;
	int error;
    uint32_t desired_access = rights;
    uint32_t share_access = NTCREATEX_SHARE_ACCESS_ALL;
    uint64_t create_flags = SMB2_CREATE_DO_CREATE | SMB2_CREATE_GET_MAX_ACCESS;
    uint32_t ntstatus = 0;
    char *file_namep = NULL, *stream_namep = NULL;
    size_t file_name_len = 0, stream_name_len = 0;
	
    if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
        /* 
         * SMB 2/3 - do compound create/close, or create
         * If fidp == NULL, then it will do a create/close.
         */
        if (!xattr) {
            file_namep = (char *) in_name;
            file_name_len = in_nmlen;
        }
        else {
            /*
             * name is actually the stream name
             * Since its an xattr, dont need the QueryInfo or Max Access
             */
            create_flags = SMB2_CREATE_IS_NAMED_STREAM;

            stream_namep = (char *) in_name;
            stream_name_len = in_nmlen;
        }

        error = smb2fs_smb_cmpd_create(share, dnp,
                                       file_namep, file_name_len,
                                       stream_namep, stream_name_len,
                                       desired_access, VREG,
                                       share_access, disp,
                                       create_flags, &ntstatus,
                                       fidp, fap,
                                       NULL, NULL,
                                       NULL, context);
    }
    else {
        /* SMB 1 */
        error = smbfs_smb_ntcreatex(share, dnp,
                                    desired_access, share_access, VREG,
                                    &fid, name, nmlen,
                                    disp, xattr, fap,
                                    TRUE, NULL,
                                    NULL, NULL,
                                    context);
        if (fidp) {
            /* Caller wants the FID, return it to them */
            *fidp = fid;
        } 
        else {
            if (!error) {
                /* Caller doesn't want the FID, close it if we have it opened */
                (void)smbfs_smb_close(share, fid, context);
            }
        }
    }
	
	return error;
}

/*
 * This is the only way to remove symlinks with a samba server.
 *
 * The calling routine must hold a reference on the share
 *
 */
static int 
smbfs_posix_unlink(struct smb_share *share, struct smbnode *np, 
				   vfs_context_t context, const char *name, size_t nmlen)
{
	struct smb_t2rq *t2p;
	struct mbchain *mbp;
	int error;
	uint32_t isDir = (vnode_isdir(np->n_vnode)) ? 1 : 0;
	
	error = smb_t2_alloc(SSTOCP(share), SMB_TRANS2_SET_PATH_INFORMATION, context, &t2p);
	if (error)
		return error;
	mbp = &t2p->t2_tparam;
	mb_init(mbp);
	mb_put_uint16le(mbp, SMB_SFILEINFO_POSIX_UNLINK);
	mb_put_uint32le(mbp, 0);	
	error = smbfs_fullpath(mbp, np, name, &nmlen, UTF_SFM_CONVERSIONS, 
						   SMB_UNICODE_STRINGS(SS_TO_SESSION(share)), '\\');
	if (error) {
		smb_t2_done(t2p);
		return error;
	}
	
	mbp = &t2p->t2_tdata;
	mb_init(mbp);
	mb_put_uint32le(mbp, isDir);

	t2p->t2_maxpcount = 2;
	t2p->t2_maxdcount = SS_TO_SESSION(share)->session_txmax;
	error = smb_t2_request(t2p);
	smb_t2_done(t2p);
	return error;
}

/*
 * The calling routine must hold a reference on the share
 */
int
smb1fs_smb_delete(struct smb_share *share, struct smbnode *np, const char *name,
                  size_t nmlen, int xattr, vfs_context_t context)
{
	struct smb_rq rq, *rqp = &rq;
	struct mbchain *mbp;
	int error;

	/* Not doing extended attribute and they support the posix unlink call */
	if (!xattr && (UNIX_CAPS(share) & UNIX_SFILEINFO_POSIX_UNLINK_CAP)) {
		error = smbfs_posix_unlink(share, np, context, name, nmlen);
		/* 
		 * If the file doesn't have write posix modes then Samba returns 
		 * STATUS_CANNOT_DELETE, which we convert to EPERM. This seems
		 * wrong we are expecting posix symantics from the call. So for now
		 * try to change the mode and attempt the delete again.
		 */
		if (error == EPERM) {
			int chmod_error;
			uint64_t vamode = np->n_mode | S_IWUSR;
			
			/* See if we can chmod on the file */
			chmod_error = smbfs_set_unix_info2(share, np, NULL, NULL, NULL, SMB_SIZE_NO_CHANGE, 
										 vamode, SMB_FLAGS_NO_CHANGE, SMB_FLAGS_NO_CHANGE, context);
			if (chmod_error == 0) {
				error = smbfs_posix_unlink(share, np, context, name, nmlen);
			}
		}
		if (error != ENOTSUP) {
			return error;
		} else {
			/* They don't really support this call, don't call them again */
			UNIX_CAPS(share) &= ~UNIX_SFILEINFO_POSIX_UNLINK_CAP;
		}
	}
	
	error = smb_rq_init(rqp, SSTOCP(share), SMB_COM_DELETE, 0, context);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	mb_put_uint16le(mbp, SMB_EFA_SYSTEM | SMB_EFA_HIDDEN);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint8(mbp, SMB_DT_ASCII);
	error = smbfs_fullpath(mbp, np, name, &nmlen, UTF_SFM_CONVERSIONS, 
						   SMB_UNICODE_STRINGS(SS_TO_SESSION(share)), xattr ? ':' : '\\');
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
 * The calling routine must hold a reference on the share
 */
int 
smb1fs_smb_rename(struct smb_share *share, struct smbnode *src,
                  struct smbnode *tdnp, const char *tname, size_t tnmlen,
                  vfs_context_t context)
{
	struct smb_rq rq, *rqp = &rq;
	struct mbchain *mbp;
	int error, retest = 0;

	error = smb_rq_init(rqp, SSTOCP(share), SMB_COM_RENAME, 0, context);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	/* freebsd bug: Let directories be renamed - Win98 requires DIR bit */
	mb_put_uint16le(mbp, (vnode_isdir(SMBTOV(src)) ? SMB_EFA_DIRECTORY : 0) |
			     SMB_EFA_SYSTEM | SMB_EFA_HIDDEN);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint8(mbp, SMB_DT_ASCII);
	do {
		error = smbfs_fullpath(mbp, src, NULL, NULL, UTF_SFM_CONVERSIONS, 
							   SMB_UNICODE_STRINGS(SS_TO_SESSION(share)), '\\');
		if (error)
			break;
		mb_put_uint8(mbp, SMB_DT_ASCII);
		error = smbfs_fullpath(mbp, tdnp, tname, &tnmlen, UTF_SFM_CONVERSIONS, 
							   SMB_UNICODE_STRINGS(SS_TO_SESSION(share)), '\\');
		if (error)
			break;
		smb_rq_bend(rqp);
		error = smb_rq_simple(rqp);
	} while(0);
	if ((error == ENOENT) && (rqp->sr_flags & SMBR_REXMIT))
		retest = 1;
	smb_rq_done(rqp);
	/*
	 * We could have sent the rename before the connection went down, but we lost
	 * the response. We resent the rename message, but since it already succeeded 
	 * we got an ENOENTerror. So lets test to see if the rename worked or not.
	 *
	 *	1. Check to make sure the source doesn't exist.
	 *	2. Check to make sure the dest does exist.
	 *
	 * If either fails then we leave the error alone, we could still be wrong here.
	 * Someone could have played with the file between the time we lost the
	 * connection and the time we do our test. Not trying to be prefect here just 
	 * to the best we can.
	 */
	if (error && retest) {
		if ((smbfs_smb_query_info(share, src, VREG, NULL, 0, NULL, context) == ENOENT) &&
			(smbfs_smb_query_info(share, tdnp, VREG, tname, tnmlen, NULL, context) == 0))
			error = 0;
	}
	return error;
}

/*
 * The calling routine must hold a reference on the share
 */
int 
smbfs_smb_mkdir(struct smb_share *share, struct smbnode *dnp, const char *name, 
				size_t len, struct smbfattr *fap, vfs_context_t context)
{
    SMBFID fid = 0;
	int error = 0;
    uint32_t desired_access = SMB2_FILE_READ_DATA;
    uint32_t share_access = NTCREATEX_SHARE_ACCESS_ALL;
    uint64_t create_flags = SMB2_CREATE_DO_CREATE | SMB2_CREATE_GET_MAX_ACCESS;
    uint32_t ntstatus = 0;
    
	/*
	 * We ask for SMB2_FILE_READ_DATA not because we need it, but
	 * just to be asking for something.  The rights == 0 case could
	 * easily be broken on some old or unusual servers.
	 */
    if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
        /* SMB 2 - do compound create/close */
        error = smb2fs_smb_cmpd_create(share, dnp,
                                       name, len,
                                       NULL, 0,
                                       desired_access, VDIR,
                                       share_access, FILE_CREATE,
                                       create_flags, &ntstatus,
                                       NULL, fap,
                                       NULL, NULL,
                                       NULL, context);
    }
    else {
        /* SMB 1 */
        error = smbfs_smb_ntcreatex(share, dnp,
                                    desired_access, share_access, VDIR, 
                                    &fid, name, len,
                                    FILE_CREATE, 0, fap,
                                    TRUE, NULL,
                                    NULL, NULL,
                                    context);
        if (!error) {
            (void)smbfs_smb_close(share, fid, context);
        }
    }

	return error;
}

/*
 * The calling routine must hold a reference on the share
 */
int
smb1fs_smb_rmdir(struct smb_share *share, struct smbnode *np, vfs_context_t context)
{
	struct smb_rq rq, *rqp = &rq;
	struct mbchain *mbp;
	int error;

	error = smb_rq_init(rqp, SSTOCP(share), SMB_COM_DELETE_DIRECTORY, 0, context);
	if (error)
		return error;
	smb_rq_getrequest(rqp, &mbp);
	smb_rq_wstart(rqp);
	smb_rq_wend(rqp);
	smb_rq_bstart(rqp);
	mb_put_uint8(mbp, SMB_DT_ASCII);
	error = smbfs_fullpath(mbp, np, NULL, NULL, UTF_SFM_CONVERSIONS, 
						   SMB_UNICODE_STRINGS(SS_TO_SESSION(share)), '\\');
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
	struct mbchain *mbp;
	struct mdchain *mdp;
	uint16_t tw, flags;
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
		error = smb_t2_alloc(SSTOCP(ctx->f_share), SMB_TRANS2_FIND_FIRST2, context, &t2p);
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
		error = smbfs_fullpath(mbp, ctx->f_dnp, ctx->f_lookupName, &len, 
							   ctx->f_sfm_conversion, 
							   SMB_UNICODE_STRINGS(SS_TO_SESSION(ctx->f_share)), '\\');
		if (error)
			return error;
	} else	{
		error = smb_t2_alloc(SSTOCP(ctx->f_share), SMB_TRANS2_FIND_NEXT2, context, &t2p);
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
		if (SMB_UNICODE_STRINGS(SS_TO_SESSION(ctx->f_share)))
			mb_put_uint8(mbp, 0);	/* 1st byte of NULL Unicode char */
		mb_put_uint8(mbp, 0);
	}
	t2p->t2_maxpcount = 5 * 2;
	t2p->t2_maxdcount = SS_TO_SESSION(ctx->f_share)->session_txmax;
	error = smb_t2_request(t2p);
	if (error) {
		/*
		 * We told them to close search when end of search is reached. So if we
		 * get an ENOENT, no reason to send a close. From testing this
		 * is correct. Not sure about other errors, so for now only do it on 
		 * ENOENT.
		 */
		if (error == ENOENT) {
			ctx->f_flags |= SMBFS_RDD_EOF | SMBFS_RDD_NOCLOSE;
		}
		return error;
	}
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
     m_fixhdr(mdp->md_top);
	 if (mbuf_get_chain_len(mdp->md_top) == 0) {
         SMBERROR("bug: ecnt = %d, but m_len = 0 and m_next = 0x%lx (please report)\n",
                  ctx->f_ecnt, smb_hideaddr(mbuf_next(mbp->mb_top)));
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
smbfs_ntwrk_findclose(struct smbfs_fctx *ctx, vfs_context_t context)
{
	struct smb_rq rq, *rqp = &rq;
	struct mbchain *mbp;
	int error;

	error = smb_rq_init(rqp, SSTOCP(ctx->f_share), SMB_COM_FIND_CLOSE2, 0, context);
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

int
smb1fs_smb_findclose(struct smbfs_fctx *ctx, vfs_context_t context)
{	
	if (ctx->f_t2)
		smb_t2_done(ctx->f_t2);
	if ((ctx->f_flags & SMBFS_RDD_NOCLOSE) == 0)
		smbfs_ntwrk_findclose(ctx, context);
	/* We are done with the share release our reference */
    smb_share_rele(ctx->f_share, context);

    if (ctx->f_LocalName) {
        SMB_FREE_DATA(ctx->f_LocalName, ctx->f_LocalNameAllocSize);
    }

    if (ctx->f_NetworkNameBuffer) {
        SMB_FREE_DATA(ctx->f_NetworkNameBuffer, ctx->f_MaxNetworkNameBufferSize);
    }

    if (ctx->f_rname) {
        SMB_FREE_DATA(ctx->f_rname, ctx->f_rname_allocsize);
    }

    SMB_FREE_TYPE(struct smbfs_fctx, ctx);
	return 0;
}

int
smb1fs_smb_findnext(struct smbfs_fctx *ctx, vfs_context_t context)
{
	struct mdchain *mdp;
	struct smb_t2rq *t2p;
	uint32_t next, dattr, resumekey = 0;
	uint64_t llint;
	int error, cnt;
	uint32_t fxsz, recsz;
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
    
	ctx->f_NetworkNameLen = 0;
    
    /* Set default uid/gid values */
    ctx->f_attr.fa_uid = KAUTH_UID_NONE;
    ctx->f_attr.fa_gid = KAUTH_GID_NONE;
    
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
		 * sure that we have the correct vnode type if its a file. Since we 
		 * don't support Steve/Conrad Symlinks with Darwin we can always count 
		 * on the vtype being correct. For directories we always know the 
		 * correct information.
		 */
		if ((SS_TO_SESSION(ctx->f_share)->session_flags & SMBV_DARWIN) ||
			(ctx->f_attr.fa_attr & SMB_EFA_DIRECTORY)) {
			ctx->f_attr.fa_valid_mask |= FA_VTYPE_VALID;
		}
		ctx->f_attr.fa_vtype = (ctx->f_attr.fa_attr & SMB_EFA_DIRECTORY) ? VDIR : VREG;
		md_get_uint32le(mdp, &ctx->f_NetworkNameLen);
		fxsz = 64; /* size of info up to filename */
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
		if (ctx->f_attr.fa_attr & SMB_EFA_REPARSE_POINT) {
			ctx->f_attr.fa_reparse_tag = eaSize;
			if (ctx->f_attr.fa_reparse_tag == IO_REPARSE_TAG_SYMLINK) {
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
		recsz = next ? next : fxsz + ctx->f_NetworkNameLen;
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
			ctx->f_attr.fa_attr |= SMB_EFA_DIRECTORY;
			ctx->f_attr.fa_vtype = VDIR;
		} else {
			ctx->f_attr.fa_attr &= ~SMB_EFA_DIRECTORY;
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
				ctx->f_attr.fa_attr |= SMB_EFA_HIDDEN;
			else
				ctx->f_attr.fa_attr &= ~SMB_EFA_HIDDEN;
		}
		if (ctx->f_attr.fa_flags_mask & EXT_IMMUTABLE) {
			if (dattr & EXT_IMMUTABLE)
				ctx->f_attr.fa_attr |= SMB_EFA_RDONLY;
			else
				ctx->f_attr.fa_attr &= ~SMB_EFA_RDONLY;
		}
		if (ctx->f_attr.fa_flags_mask & EXT_DO_NOT_BACKUP) {
			if (dattr & EXT_DO_NOT_BACKUP)
				ctx->f_attr.fa_attr &= ~SMB_EFA_ARCHIVE;
			else
				ctx->f_attr.fa_attr |= SMB_EFA_ARCHIVE;
		}
		
		ctx->f_attr.fa_unix = TRUE;

		md_get_uint32le(mdp, &ctx->f_NetworkNameLen);	
		fxsz = 128; /* size ofinfo up to filename */
		recsz = next ? next : fxsz + ctx->f_NetworkNameLen;
		break;
	default:
		SMBERROR("unexpected info level %d\n", ctx->f_infolevel);
		return EINVAL;
	}
	if ((size_t)ctx->f_NetworkNameLen > ctx->f_MaxNetworkNameBufferSize)
		ctx->f_NetworkNameLen = (uint32_t)ctx->f_MaxNetworkNameBufferSize;
	error = md_get_mem(mdp, ctx->f_NetworkNameBuffer, ctx->f_NetworkNameLen, MB_MSYSTEM);
	if (error)
		return error;
	if (next) {
		cnt = next - ctx->f_NetworkNameLen - fxsz;
		if (cnt > 0)
			md_get_mem(mdp, NULL, cnt, MB_MSYSTEM);
		else if (cnt < 0) {
			SMBERROR("out of sync\n");
			return EBADRPC;
		}
	}
	/* Don't count any trailing null in the name. */
	if (SMB_UNICODE_STRINGS(SS_TO_SESSION(ctx->f_share))) {
		if ((ctx->f_NetworkNameLen > 1) && (ctx->f_NetworkNameBuffer[ctx->f_NetworkNameLen - 1] == 0) && 
			(ctx->f_NetworkNameBuffer[ctx->f_NetworkNameLen - 2] == 0))
			ctx->f_NetworkNameLen -= 2;
	} else {
		if (ctx->f_NetworkNameLen && ctx->f_NetworkNameBuffer[ctx->f_NetworkNameLen - 1] == 0)
			ctx->f_NetworkNameLen--;
	}
	if (ctx->f_NetworkNameLen == 0)
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
	if (ctx->f_rnameofs && ((ctx->f_flags & SMBFS_RDD_GOTRNAME) == 0) &&
	    ((ctx->f_rnameofs >= ctx->f_eofs) && (ctx->f_rnameofs < next))) {
		/* Server needs a resume filename. */
		if (ctx->f_rnamelen < ctx->f_NetworkNameLen) {
            if (ctx->f_rname) {
                SMB_FREE_DATA(ctx->f_rname, ctx->f_rname_allocsize);
            }
            ctx->f_rname_allocsize = ctx->f_NetworkNameLen;
            SMB_MALLOC_DATA(ctx->f_rname, ctx->f_rname_allocsize, Z_WAITOK);
		}
		ctx->f_rnamelen = ctx->f_NetworkNameLen;
		bcopy(ctx->f_NetworkNameBuffer, ctx->f_rname, ctx->f_NetworkNameLen);
		ctx->f_flags |= SMBFS_RDD_GOTRNAME;
	}
	ctx->f_eofs = next;
	ctx->f_ecnt--;
	return 0;
}

/*
 * The calling routine must hold a reference on the share
 */
int 
smbfs_smb_findopen(struct smb_share *share, struct smbnode *dnp,
                   const char *lookupName, size_t lookupNameLen,
                   struct smbfs_fctx **ctxpp, int wildCardLookup,
                   uint32_t is_readdir, vfs_context_t context)
{
	struct smbfs_fctx *ctx;
	int error = 0;

    SMB_MALLOC_TYPE(ctx, struct smbfs_fctx, Z_WAITOK_ZERO);
	if (ctx == NULL) {
		return ENOMEM;
    }
	
	/* We need to hold a reference on the share for the life of the directory listing. */
	ctx->f_share = share;
	smb_share_ref(ctx->f_share);

    ctx->f_is_readdir = is_readdir;
	ctx->f_dnp = dnp;
	ctx->f_flags |= SMBFS_RDD_FINDFIRST;
	/*
	 * If this is a wildcard lookup then make sure we are not setting the 
	 * UTF_SFM_CONVERSIONS flag. We are either doing a lookup by name or we are 
	 * doing a wildcard lookup using the asterisk. When doing a wildcard lookup 
	 * the asterisk is legit, so we don't have to convert it. Now once we send
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
	
	if (UNIX_CAPS(share) & UNIX_FIND_FILE_UNIX_INFO2_CAP) {
		/* 
		 * Search count is the clients request for the max number of items to 
		 * be return. We could always request some large number, but lets just
		 * request the max number of entries that will fit in a buffer.
		 *
		 * NOTE: We always make sure session_txmax is 1k or larger.
		 */
		if (wildCardLookup) {
			ctx->f_searchCount = SS_TO_SESSION(share)->session_txmax / SMB_FIND_FILE_UNIX_INFO2_MIN_LEN;
		}
		ctx->f_infolevel = SMB_FIND_FILE_UNIX_INFO2;
	} else {
		/* 
		 * Search count is the clients request for the max number of items to 
		 * be return. We could always request some large number, but lets just
		 * request the max number of entries that will fit in a buffer.
		 *
		 * NOTE: We always make sure session_txmax is 1k or larger.
		 */
		if (wildCardLookup) {
			ctx->f_searchCount = SS_TO_SESSION(share)->session_txmax / SMB_FIND_BOTH_DIRECTORY_INFO_MIN_LEN;
		}
        
        if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
            /* Short Name info may contain special info if its a Mac OS Server */
            ctx->f_infolevel = SMB_FIND_BOTH_DIRECTORY_INFO;
        }
        else {
            ctx->f_infolevel = SMB_FIND_BOTH_DIRECTORY_INFO;
        }
	}
	/* We always default to using the same attribute mask */
	ctx->f_attrmask = SMB_EFA_SYSTEM | SMB_EFA_HIDDEN | SMB_EFA_DIRECTORY;
	ctx->f_lookupName = lookupName;
	ctx->f_lookupNameLen = lookupNameLen;
	/*
	 * Unicode requires 4 * max file name len, codepage requires 3 * max file 
	 * name, so lets just always use the unicode size.
	 */
	ctx->f_MaxNetworkNameBufferSize = share->ss_maxfilenamelen * 4;
    SMB_MALLOC_DATA(ctx->f_NetworkNameBuffer, ctx->f_MaxNetworkNameBufferSize, Z_WAITOK);
	if (ctx->f_NetworkNameBuffer == NULL) {
        SMBERROR("f_NetworkNameBuffer failed\n");
		error = ENOMEM;
    }

	if (error) {
		smbfs_smb_findclose(ctx, context);
    }
	else {
		*ctxpp = ctx;
    }
    
	return error;
}

int
smbfs_findnext(struct smbfs_fctx *ctx, vfs_context_t context)
{
	int error;
	struct timespec save_reqtime;

	for (;;) {
        /* save time that enumerate was done at */
        save_reqtime = ctx->f_attr.fa_reqtime;
        bzero(&ctx->f_attr, sizeof(ctx->f_attr));
        ctx->f_attr.fa_reqtime = save_reqtime;

		error = smbfs_smb_findnext(ctx, context);
        if (error == EAGAIN) {
            /* 
             * Try one more time, server may have a tempsmbfs_smb_qpathinfo 
             * resource issue 
             */
            /* save time that enumerate was done at */
            save_reqtime = ctx->f_attr.fa_reqtime;
            bzero(&ctx->f_attr, sizeof(ctx->f_attr));
            ctx->f_attr.fa_reqtime = save_reqtime;

            error = smbfs_smb_findnext(ctx, context);
        }
		if (error) {
			return error;
        }
        
		if (SMB_UNICODE_STRINGS(SS_TO_SESSION(ctx->f_share))) {
            /* ignore the Unicode '.' and '..' dirs */
			if ((ctx->f_NetworkNameLen == 2 &&
			     letohs(*(uint16_t *)ctx->f_NetworkNameBuffer) == 0x002e) ||
			    (ctx->f_NetworkNameLen == 4 &&
			     letohl(*(uint32_t *)ctx->f_NetworkNameBuffer) == 0x002e002e))
				continue;
		} 
        else {
            /* ignore the '.' and '..' dirs */
			if ((ctx->f_NetworkNameLen == 1 && ctx->f_NetworkNameBuffer[0] == '.') ||
			    (ctx->f_NetworkNameLen == 2 && ctx->f_NetworkNameBuffer[0] == '.' &&
			     ctx->f_NetworkNameBuffer[1] == '.'))
				continue;
		}
		break;
	}
    
    /* 
     * Successfully parsed out one entry from the search buffer
     * so return that one entry.
     */
    ctx->f_parsed_cnt += 1;

    if (ctx->f_LocalName) {
        SMB_FREE_DATA(ctx->f_LocalName, ctx->f_LocalNameAllocSize); /* Free any old name we may have */
    }
	ctx->f_LocalNameLen = ctx->f_NetworkNameLen;
	ctx->f_LocalName = smbfs_ntwrkname_tolocal(ctx->f_NetworkNameBuffer,
                                               &ctx->f_LocalNameLen, &ctx->f_LocalNameAllocSize,
                                               SMB_UNICODE_STRINGS(SS_TO_SESSION(ctx->f_share)));
    
    if (!(SS_TO_SESSION(ctx->f_share)->session_misc_flags & SMBV_HAS_FILEIDS)) {
        /* Server does not support File IDs */
        ctx->f_attr.fa_ino = smbfs_getino(ctx->f_dnp,
                                          ctx->f_LocalName, 
                                          ctx->f_LocalNameLen);
    }

	return 0;
}

/*
 * The calling routine must hold a reference on the share
 */
int 
smbfs_lookup(struct smb_share *share, struct smbnode *dnp, const char **namep, 
				 size_t *nmlenp, size_t *name_allocsize, struct smbfattr *fap, vfs_context_t context)
{
	struct smbfs_fctx *ctx;
	int error = EINVAL;
	const char *name = (namep ? *namep : NULL);
	size_t nmlen = (nmlenp ? *nmlenp : 0);
	struct smbmount *smp;
    struct smb_session *sessionp = SS_TO_SESSION(share);
    enum vtype vnode_type = VREG;
    short infolevel = SMB_QFILEINFO_ALL_INFO;
    vnode_t par_vp = NULL;

	DBG_ASSERT(dnp);
    
    SMB_LOG_KTRACE(SMB_DBG_SMBFS_LOOKUP | DBG_FUNC_START, 0, 0, 0, 0, 0);

	/* This should no longer happen, but just in case (should remove someday). */
	if (dnp == NULL) {
		SMBERROR("The parent node is NULL, shouldn't happen\n");
		error = EINVAL;
        goto done;
	}
    
    if (UNIX_CAPS(share) & UNIX_QFILEINFO_UNIX_INFO2_CAP) {
        infolevel = SMB_QFILEINFO_UNIX_INFO2;
    }

	bzero(fap, sizeof(*fap));
	/* We keep track of the time the lookup call was requested */
	nanouptime(&fap->fa_reqtime);
	
    smp = dnp->n_mount;
	if ((dnp->n_ino == smp->sm_root_ino) && (name == NULL)) {
		uint64_t DIFF1980TO1601 = 11960035200ULL*10000000ULL;

		fap->fa_attr = SMB_EFA_DIRECTORY;
		fap->fa_vtype = VDIR;
        fap->fa_valid_mask |= FA_VTYPE_VALID;

        if (smp->sm_root_ino == 0) {
            /*
             * Must be at mount time and we dont know what the root File ID is.
             * Assume its 2 to start with
             */
            smp->sm_root_ino = SMBFS_ROOT_INO;
            fap->fa_ino = SMBFS_ROOT_INO;
        }
        else {
            /* Recreating root vnode and we know what its ID was */
            fap->fa_ino = smp->sm_root_ino;
        }

        /* Its the root,thus its a dir */
        vnode_type = VDIR;

        if (sessionp->session_flags & SMBV_SMB2) {
            if (!(sessionp->session_misc_flags & SMBV_NO_QUERYINFO)) {
                /* Try the normal Query Info */
                error = smbfs_smb_qpathinfo(share, dnp, vnode_type,
                                            fap, infolevel,
                                            NULL, NULL, NULL,
                                            context);

                SMB_LOG_KTRACE(SMB_DBG_SMBFS_LOOKUP | DBG_FUNC_NONE,
                               0xabc001, error, 0, 0, 0);
           
                if (error) {
                    SMBERROR("Server failed Query Info %d\n", error);
                        /* This server does not support Query Info? */
                        sessionp->session_misc_flags |= SMBV_NO_QUERYINFO;
                }

            }
            
            if (sessionp->session_misc_flags & SMBV_NO_QUERYINFO) {
                /* Use Query Dir instead of Query Info */
                error = smb2fs_smb_cmpd_query_dir_one(share, dnp,
                                                      NULL, 0,
                                                      fap, NULL, NULL, NULL,
                                                      context);
            
                SMB_LOG_KTRACE(SMB_DBG_SMBFS_LOOKUP | DBG_FUNC_NONE,
                               0xabc002, error, 0, 0, 0);
            }
        } else {
            /* SMB 1 case */
            error = smbfs_smb_qpathinfo(share, dnp, vnode_type,
                                        fap, infolevel,
                                        NULL, NULL, NULL,
                                        context);
            
            SMB_LOG_KTRACE(SMB_DBG_SMBFS_LOOKUP | DBG_FUNC_NONE,
                           0xabc003, error, 0, 0, 0);
        }

		/*
		 * NTFS handles dates correctly, but FAT file systems have some
		 * problems. If an entire FAT drive is shared, then the root of the
         * share may have either no dates (ie 0) or bad dates. This seems to
         * only happen with the root of the share, so we have to check for bad
         * dates and fix them up. Windows XP/2000/2003 will return Jan. 1, 1980
         * for the create, modif and access dates. NT4 will return no dates
         * at all.
		 *
		 * So we follow the Windows 2000 model. If any of the time
		 * fields are zero we fill them in with Jan 1, 1980.
         *
         * Note: in setattr, we also do not allow any dates before 1980 for a 
         * FAT file system.
		 */
		if (fap->fa_mtime.tv_sec == 0)
			smb_time_NT2local(DIFF1980TO1601, &fap->fa_mtime);
		if (fap->fa_crtime.tv_sec == 0)
			smb_time_NT2local(DIFF1980TO1601, &fap->fa_crtime);
		if (fap->fa_atime.tv_sec == 0)
			fap->fa_atime = fap->fa_mtime;
		if (fap->fa_chtime.tv_sec == 0)
			fap->fa_chtime = fap->fa_mtime;
        
		goto done;
	}

	if (nmlen == 1 && name && name[0] == '.') {
		error = smbfs_lookup(share, dnp, NULL, NULL, NULL, fap, context);
        SMB_LOG_KTRACE(SMB_DBG_SMBFS_LOOKUP | DBG_FUNC_NONE,
                       0xabc004, error, 0, 0, 0);
		goto done;
	} else if (nmlen == 2 && name && name[0] == '.' && name[1] == '.') {
		/* 
		 * Remember that error is set to EINVAL. This should never happen, but
		 * just in case make sure we have a parent, if not return EINVAL 
		 */
        par_vp = smbfs_smb_get_parent(dnp, kShareLock);
        if (par_vp != NULL) {
            error = smbfs_lookup(share, VTOSMB(par_vp), NULL, NULL, NULL, fap, context);
            SMB_LOG_KTRACE(SMB_DBG_SMBFS_LOOKUP | DBG_FUNC_NONE,
                           0xabc004, error, 0, 0, 0);
            
            vnode_put(par_vp);
        }
        else {
            if (dnp->n_parent_vid != 0) {
                /* Parent got recycled already? */
                SMBWARNING_LOCK(dnp, "Missing parent for <%s> \n",
                                dnp->n_name);
            }

            error = EINVAL;
        }
        
		goto done;
	}
    
	/*
	 * So we now default to using FindFirst, because of Dfs. Only FindFirst 
	 * support getting the reparse point tag. If this is a stream node then
	 * use the query info call to do the lookup. The FindFirst code doesn't
	 * handle stream nodes yet, may want to change that in the future.
	 */
	if ((dnp->n_vnode) && vnode_isnamedstream(dnp->n_vnode)) {
		goto doQueryInfo;
	}

    if (sessionp->session_flags & SMBV_SMB2) {
        /* SMB 2/3 can do this in in one compound req */
        error = smb2fs_smb_cmpd_query_dir_one(share, dnp,
                                              name, nmlen,
                                              fap, (char **) namep, nmlenp, name_allocsize,
                                              context);
        SMB_LOG_KTRACE(SMB_DBG_SMBFS_LOOKUP | DBG_FUNC_NONE,
                       0xabc006, error, 0, 0, 0);
    }
    else {
		Boolean unix_symlink = ((UNIX_CAPS(share) & UNIX_SFILEINFO_UNIX_LINK_CAP)) ? TRUE : FALSE;
		Boolean darwin = (SS_TO_SESSION(share)->session_flags & SMBV_DARWIN) ? TRUE : FALSE;
		
		/* 
		 * We need to workaround a Samba Server bug. In some lookup paths, Samba
		 * will follow the symlink to see if it exists before returning the info
		 * requested. In the case were the symlink is broken Samba will return
		 * that the symlink doesn't exist when its what the symlink points to that
		 * doesn't exist.  This can cause symlinks to appear not to exist in some case
		 * and not others. Samba servers will follow the symlink if we make a
		 * non wildcard FindFirst/Next call, but they will not follow it if we do a
		 * Trans2Query. The problem we have here is the Trans2Query call does not
		 * return the name of the file, which means we could have the wrong case.
		 *
		 * If they support unix symlinks and its not darwin then we know its one
		 * of these Samba servers, this includes Snow Leopard Servers.
		 * 
		 * So if we already have the vnode then we know its a symlink and have its
		 * name so just make the Trans2Query call and we are done.
		 * 
		 * In the case were we don't have the vnode we need to do the FindFirst/Next
		 * call first, if that call returns ENOENT then we need to retry with a
		 * Trans2Query call. This can cause a performance issue, but only when
		 * the item doesn't exist and we should have this in our negative name
		 * cache after this double lookup.
		*/
		if (unix_symlink && !darwin && (namep == NULL) && dnp->n_vnode &&
			(vnode_vtype(dnp->n_vnode) == VLNK)) {
            error = smbfs_smb_qpathinfo(share, dnp, VREG,
                                       fap, SMB_QFILEINFO_UNIX_INFO2,
                                       namep, nmlenp, name_allocsize,
                                       context);
            goto done;
		}

		/* SMB 1 using old, slower way */
		error = smbfs_smb_findopen(share, dnp, name, nmlen, &ctx, FALSE,
                                   0, context);
		if (error) {
			/* Can't happen we do wait ok */
			goto done;
		}
		error = smbfs_findnext(ctx, context);
		if (error == 0) {
			*fap = ctx->f_attr;
			if (name == NULL) {
				fap->fa_ino = dnp->n_ino;
			}
			if (namep) {
				*namep = ctx->f_LocalName;
				ctx->f_LocalName = NULL;
			}
			if (nmlenp) {
				*nmlenp = ctx->f_LocalNameLen;
			}
            if (name_allocsize) {
                *name_allocsize = ctx->f_LocalNameAllocSize;
            }
		}
		
		/* save vnode type before we free the ctx */
		vnode_type = (ctx->f_attr.fa_attr & SMB_EFA_DIRECTORY) ? VDIR : VREG;
		
		/* Free ctx */
		smbfs_smb_findclose(ctx, context);
		
		/*
		 * We need to workaround a Samba Server bug. See the comment above for
		 * complete details.
		 */
		if ((error == ENOENT) && unix_symlink && !darwin && (namep)) {
			SMBWARNING("Working around Samba bug doing second lookup\n");
            error = smbfs_smb_qpathinfo(share, dnp, vnode_type,
                                        fap, SMB_QFILEINFO_UNIX_INFO2,
                                        namep, nmlenp, name_allocsize,
                                        context);
            goto done;
		}
    }

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
     *
     * NOTE: If we failed because of a low resources (EAGAIN) then try again.
	 */
	if ((error != EACCES) && (error != EPERM) && (error != EAGAIN)) {
		goto done;
	}
    
doQueryInfo:
	/*
     * Either its a named stream vnode, or the Query Dir failed so try
     * doing a Query Info instead
     */
    
     /* 
      * Since we dont know what the item is, try VREG as that is least likely
      * to return an error
      */
    vnode_type = VREG;
    
    if ((infolevel == SMB_QFILEINFO_UNIX_INFO2) && (namep != NULL)) {
        /* If they are requesting the name, return error nothing else we can do here. */
        error = EACCES;

        goto done;
    }
    
    error = smbfs_smb_qpathinfo(share, dnp, vnode_type,
                                fap, infolevel,
                                namep, nmlenp, name_allocsize,
                                context);

    SMB_LOG_KTRACE(SMB_DBG_SMBFS_LOOKUP | DBG_FUNC_NONE,
                   0xabc007, error, 0, 0, 0);
    
done:
    SMB_LOG_KTRACE(SMB_DBG_SMBFS_LOOKUP | DBG_FUNC_END, error, 0, 0, 0, 0);
	return error;
}

void smbfs_remove_dir_lease(struct smbnode *np, const char *reason) {
    int warning = 0;

    smbnode_lease_lock(&np->n_lease, smbfs_closedirlookup);

    if (np->n_lease.flags != 0) {
        np->n_lease.flags |= SMB2_LEASE_BROKEN;
        np->n_lease.flags &= ~SMB2_LEASE_GRANTED;

        /*
         * Remove the lease from the lease hash table
         *
         * Note: Don't have the share here so just pass in NULL here
         * as we only call this when we know we had a dir lease, so
         * dont need the check for dir leases being supported.
         */
        warning = smbfs_add_update_lease(NULL, np->n_vnode, &np->n_lease, SMBFS_LEASE_REMOVE, 1,
                                          "CloseDir");
        if (warning) {
            /* Shouldnt ever happen */
            SMBERROR_LOCK(np, "smbfs_add_update_lease remove failed <%d> on <%s>\n",
                          warning, np->n_name);
        }

        SMB_LOG_LEASING_LOCK(np, "Lost dir lease on <%s> due to <%s> \n",
                             np->n_name, reason);
    }

    smbnode_lease_unlock(&np->n_lease);
}

void smbfs_closedirlookup(struct smbnode *np, int32_t is_readdir,
                          const char *reason, vfs_context_t context)
{
    if (is_readdir == 0) {
        /* Not vnop_readdir */
        if (np->d_fctx) {
            smbfs_smb_findclose(np->d_fctx, context);

            /* If we closed the dir, then the dir lease is gone */
            smbfs_remove_dir_lease(np, reason);
        }

        np->d_fctx = NULL;
        np->d_offset = 0;
    }
    else {
        /* It is vnop_readdir */
       if (np->d_rdir_fctx) {
            smbfs_smb_findclose(np->d_rdir_fctx, context);
        }

        np->d_rdir_fctx = NULL;
        np->d_rdir_offset = 0;

        if (np->d_nextEntry) {
            SMB_FREE_DATA(np->d_nextEntry, np->d_nextEntryLen);
        }
        np->d_nextEntry = NULL;
        np->d_nextEntryLen = 0;
    }
}

/*
 * The calling routine must hold a reference on the share
 */
static int
smbfs_smb_getsec_int(struct smb_share *share, SMBFID fid, uint32_t selector, 
                     struct ntsecdesc **res, uint32_t *reslen, 
                     vfs_context_t context)
{
	struct smb_ntrq *ntp;
	struct mbchain *mbp;
	struct mdchain *mdp;
	int error;
	size_t len;
    uint16_t smb1_fid = (uint16_t) fid;

	error = smb_nt_alloc(SSTOCP(share), NT_TRANSACT_QUERY_SECURITY_DESC, context, &ntp);
	if (error)
		return error;
	mbp = &ntp->nt_tparam;
	mb_init(mbp);
	mb_put_mem(mbp, (caddr_t)&smb1_fid, sizeof(smb1_fid), MB_MSYSTEM);
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
	} else {
		md_get_uint32le(mdp, reslen);
	}
	mdp = &ntp->nt_rdata;
	if (mdp->md_top) {
		len = m_fixhdr(mdp->md_top);
		if (len != (size_t)*reslen)
			SMBWARNING("Sent us %ld but said they sent us %d for fid = 0x%x\n", 
					   len, *reslen, letohs(smb1_fid));
		/*
		 * The following "if (len < *reslen)" handles a Windows bug
		 * observed when the underlying filesystem is FAT32.  In that
		 * case a 32 byte security descriptor comes back (S-1-1-0, ie
		 * "Everyone") but the Parameter Block claims 44 is the length
		 * of the security descriptor.  (The Data Block length
		 * claimed is 32.  This server bug was reported against NT
		 * first and I've personally observed it with W2K.
		 */
		
		if (len < (size_t)*reslen) {
			 /* Believe what we got instead of what they told us */
			*reslen = (uint32_t)len;
		} else if (len > (size_t)*reslen) {
			len = *reslen;	 /* ignore any extra data */
		}
		/* 
		 * All the calling routines expect to have sizeof(struct ntsecdesc). The
		 * len is the amount of data we have received and *reslen is what they
		 * claim they sent. Up above we make sure that *reslen and len are the
		 * same. So all we need to do here is make sure that len is not less than
		 * the size of our ntsecdesc structure.
		 */
		if (len >= sizeof(struct ntsecdesc)) {
            SMB_MALLOC_DATA(*res, len, Z_WAITOK_ZERO);
			error = md_get_mem(mdp, (caddr_t)*res, len, MB_MSYSTEM);
            if (error) {
                SMB_FREE_DATA(*res, len);
            }
		} else {
			SMBERROR("len %ld < ntsecdesc %ld fid 0x%x\n", len, 
					 sizeof(struct ntsecdesc), letohs(smb1_fid));
			error = EBADRPC; 
		}	
	} else {
		SMBERROR("null md_top? fid 0x%x\n", letohs(smb1_fid));
		error = EBADRPC; 		
	}
done:
	smb_nt_done(ntp);
	return (error);
}

/*
 * The calling routine must hold a reference on the share
 */
int
smbfs_smb_getsec(struct smb_share *share, struct smbnode *np,
                 uint32_t desired_access, SMBFID fid, uint32_t selector,
                 struct ntsecdesc **res, size_t *rt_len, vfs_context_t context)
{
	int error;
	uint32_t seclen;
	
    seclen = SS_TO_SESSION(share)->session_txmax;

    if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
        error = smb2fs_smb_security_get(share, np,
                                        desired_access, selector,
                                        res, &seclen,
                                        context);
    }
    else {
        /*
         * If the buffer size is to small we could end up making two request. Using 
         * the max transmit buffer should limit this from happening to often.
         */
        error = smbfs_smb_getsec_int(share, fid, selector, res, &seclen, context);
        if (error && (seclen > SS_TO_SESSION(share)->session_txmax)) {
            error = smbfs_smb_getsec_int(share, fid, selector, res, &seclen, 
                                         context);
        }
    }
        
    /* Return the the size we ended up getting */
    if (error == 0)
        *rt_len = seclen;
    
	return (error);
}

/*
 * The calling routine must hold a reference on the share
 */
int
smb1fs_setsec(struct smb_share *share, SMBFID fid, uint32_t selector,
              uint16_t ControlFlags, struct ntsid *owner, struct ntsid *group, 
              struct ntacl *sacl, struct ntacl *dacl, vfs_context_t context)
{
	struct smb_ntrq *ntp;
	struct mbchain *mbp;
	int error;
	uint32_t off;
	struct ntsecdesc ntsd;
    uint16_t smb1_fid = (uint16_t) fid;
	
	error = smb_nt_alloc(SSTOCP(share), NT_TRANSACT_SET_SECURITY_DESC, 
						 context, &ntp);
	if (error)
		return error;
	mbp = &ntp->nt_tparam;
	mb_init(mbp);
	mb_put_mem(mbp, (caddr_t)&smb1_fid, sizeof(smb1_fid), MB_MSYSTEM);
	mb_put_uint16le(mbp, 0); /* reserved */
	mb_put_uint32le(mbp, selector);
	mbp = &ntp->nt_tdata;
	mb_init(mbp);
	bzero(&ntsd, sizeof ntsd);
	ntsd.Revision = 0x01;	/* Should we make this a define? */
	/*
	 * A note about flags ("SECURITY_DESCRIPTOR_CONTROL" in MSDN)
	 * We set here only those bits we can be sure must be set.  The rest
	 * are up to the caller.  In particular, the caller may intentionally
	 * set an acl PRESENT bit while giving us a null pointer for the
	 * acl - that sets a null acl, denying access to everyone.
	 */
	ControlFlags |= SE_SELF_RELATIVE;
	off = (uint32_t)sizeof(ntsd);
	if (owner) {
		ntsd.OffsetOwner = htolel(off);
		off += (uint32_t)sidlen(owner);
	}
	if (group) {
		ntsd.OffsetGroup = htolel(off);
		off += (uint32_t)sidlen(group);
	}
	if (sacl) {
		ControlFlags |= SE_SACL_PRESENT | SE_SACL_AUTO_INHERITED | SE_SACL_AUTO_INHERIT_REQ;
		ntsd.OffsetSacl = htolel(off);
		off += acllen(sacl);
	}
	if (dacl) {
		ControlFlags |= SE_DACL_PRESENT | SE_DACL_AUTO_INHERITED | SE_DACL_AUTO_INHERIT_REQ;
		ntsd.OffsetDacl = htolel(off);
	}
	
	ntsd.ControlFlags = htoles(ControlFlags);
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
    
	if ((error != 0) && (ntp->nt_status == STATUS_INVALID_SID)) {
		/*
		 * If the server returns STATUS_INVALID_SID, then just pretend that
		 * we set the security info even though it "failed".
		 * See <rdar://problem/10852453>.
		 */
		error = 0;
    }

	smb_nt_done(ntp);
	return (error);
}
