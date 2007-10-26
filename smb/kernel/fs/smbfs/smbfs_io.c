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
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/resourcevar.h>	/* defines plimit structure in proc struct */
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/dirent.h>
#include <sys/signalvar.h>
#include <sys/sysctl.h>

#include <sys/kauth.h>

#include <sys/syslog.h>
#ifdef thread_sleep_simple_lock
#undef thread_sleep_simple_lock
#endif
#include <sys/smb_apple.h>
#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_subr.h>

#include <fs/smbfs/smbfs.h>
#include <fs/smbfs/smbfs_node.h>
#include <fs/smbfs/smbfs_subr.h>
#include <sys/smb_iconv.h>

#include <sys/buf.h>

/* keep build working with older headers */
#ifndef round_page_32
	#define round_page_32 round_page
#endif

static int smbfs_fastlookup = 1;

extern struct linker_set sysctl_net_smb_fs;

SYSCTL_DECL(_net_smb_fs);
SYSCTL_INT(_net_smb_fs, OID_AUTO, fastlookup, CTLFLAG_RW, &smbfs_fastlookup, 0, "");


#define DE_SIZE	(int)(sizeof(struct dirent))

PRIVSYM int smbfs_readvdir(vnode_t vp, uio_t uio, vfs_context_t vfsctx)
{
	struct dirent de;
	struct smb_cred scred;
	struct smbfs_fctx *ctx;
	vnode_t newvp;
	struct smbnode *np = VTOSMB(vp);
	int error;
	off_t offset;
	user_ssize_t limit;

	if (uio_resid(uio) < DE_SIZE || uio_offset(uio) < 0)
		return (EINVAL);

	np = VTOSMB(vp);
	/*
	 * This cache_purge ensures that name cache for this dir will be
	 * current - it'll only have the items for which the smbfs_nget
	 * MAKEENTRY happened.
	 *
	 * We no longer make this call. Since we are calling this everytime, none of our
	 * items  every get put in the name cache. What would be nice is to have
	 * a way to cache purge the children and only do that if uio offset is zero. Not
	 * sure that is worth it. Leaving the code in for now in case we run into any problems.
	 *
	 * if (smbfs_fastlookup)
	 *	cache_purge(vp);
	 */
	SMBVDEBUG("dirname='%s'\n", np->n_name);
	smb_scred_init(&scred, vfsctx);
	// LP64todo - should change to handle 64-bit values
	offset = uio_offset(uio) / DE_SIZE; 	/* offset in the directory */
	limit = uio_resid(uio) / DE_SIZE;
	while (limit && offset < 2) {
		limit--;
		bzero((caddr_t)&de, DE_SIZE);
		de.d_reclen = DE_SIZE;
		de.d_fileno = (offset == 0) ? np->n_ino :
		    (np->n_parent ? np->n_parent->n_ino : 2);
		if (de.d_fileno == 0)
			de.d_fileno = 0x7ffffffd + offset;
		de.d_namlen = offset + 1;
		de.d_name[0] = '.';
		de.d_name[1] = '.';
		de.d_name[offset + 1] = '\0';
		de.d_type = DT_DIR;
		error = uiomove((caddr_t)&de, DE_SIZE, uio);
		if (error)
			return error;
		offset++;
	}
	if (limit == 0)
		return (0);
	if (offset != np->n_dirofs || np->n_dirseq == NULL) {
		SMBVDEBUG("Reopening search %ld:%ld\n", offset, np->n_dirofs);
		if (np->n_dirseq) {
			smbfs_smb_findclose(np->n_dirseq, &scred);
			np->n_dirseq = NULL;
		}
		np->n_dirofs = 2;
		error = smbfs_smb_findopen(np, "*", 1,
		    SMB_FA_SYSTEM | SMB_FA_HIDDEN | SMB_FA_DIR,
		    &scred, &ctx, NO_SFM_CONVERSIONS);
		if (error) {
			SMBVDEBUG("can not open search, error = %d", error);
			return error;
		}
		np->n_dirseq = ctx;
	} else
		ctx = np->n_dirseq;
	while (np->n_dirofs < offset) {
		error = smbfs_smb_findnext(ctx, offset - np->n_dirofs++, &scred);
		if (error) {
			smbfs_smb_findclose(np->n_dirseq, &scred);
			np->n_dirseq = NULL;
			return (error == ENOENT ? 0 : error);
		}
	}
	error = 0;
	for (; limit; limit--, offset++) {
		error = smbfs_smb_findnext(ctx, limit, &scred);
		if (error)
			break;
		np->n_dirofs++;
		bzero((caddr_t)&de, DE_SIZE);
		de.d_reclen = DE_SIZE;
		de.d_fileno = ctx->f_attr.fa_ino;
		de.d_type = (ctx->f_attr.fa_attr & SMB_FA_DIR) ? DT_DIR : DT_REG;
		de.d_namlen = ctx->f_nmlen;
		bcopy(ctx->f_name, de.d_name, de.d_namlen);
		de.d_name[de.d_namlen] = '\0';
		if (smbfs_fastlookup) {
			error = smbfs_nget(vnode_mount(vp), vp, ctx->f_name, ctx->f_nmlen, &ctx->f_attr, &newvp, MAKEENTRY, vfsctx);
			if (!error) {
				/* 
				 * Some applications use the inode as a marker and expect it to be presistent. Currently our
				 * inode numbers are create by hashing the name and adding the parent inode number. Once a node
				 * is create we should try to keep the same inode number though out its life. The smbfs_nget will either
				 * create the node or return one found in the hash table. The one that gets created will use 
				 * ctx->f_attr.fa_ino, but if its in our hash table it will have its original number. So in either
				 * case set the file number to the inode number that was used when the node was created.
				 */
				de.d_fileno = VTOSMB(newvp)->n_ino;
				smbnode_unlock(VTOSMB(newvp));	/* Release the smbnode lock */
				vnode_put(newvp);
			}
		}
		error = uiomove((caddr_t)&de, DE_SIZE, uio);
		if (error)
			break;
	}
	if (error == ENOENT)
		error = 0;
	return error;
}

PRIVSYM int smbfs_doread(vnode_t vp, uio_t uiop, struct smb_cred *scred, u_int16_t fid)
{
	struct smbmount *smp = VFSTOSMBFS(vnode_mount(vp));
	struct smbnode *np = VTOSMB(vp);
	int error;
	int requestsize;
	user_ssize_t remainder;
	
	
	if (uio_offset(uiop) >= (off_t)np->n_size) {
		/* if offset is beyond EOF, read nothing */
		error = 0;
		goto exit;
	}
	
	/* pin requestsize to EOF */
	requestsize = MIN(uio_resid(uiop), (off_t)(np->n_size - uio_offset(uiop)));
	
	/* subtract requestSize from uio_resid and save remainder */
	remainder = uio_resid(uiop) - requestsize;
	
	/* adjust size of read */
	uio_setresid(uiop, requestsize);
	
	error = smb_read(smp->sm_share, fid, uiop, scred);
	
	/* set remaining uio_resid */
	uio_setresid(uiop, (uio_resid(uiop) + remainder));

exit:
	
	return error;
}

/* 
 * This routine will zero fill the data between from and to. We may want to allocate
 * smbzeroes in the future.
 */
char smbzeroes[4096] = { 0 };

static int
smbfs_zero_fill(vnode_t vp, u_int16_t fid, u_quad_t from, u_quad_t to, struct smb_cred *scredp, int timo)
{
	struct smbmount *smp = VTOSMBFS(vp);
	struct smbnode *np = VTOSMB(vp);
	int len, error = 0;
	uio_t uio;

	/*
	 * XXX for coherence callers must prevent VM from seeing the file size
	 * grow until this loop is complete.
	 */
	uio = uio_create(1, from, UIO_SYSSPACE, UIO_WRITE);
	while (from < to) {
		len = min(to - from, sizeof smbzeroes);
		uio_reset(uio, from, UIO_SYSSPACE, UIO_WRITE );
		uio_addiov(uio, CAST_USER_ADDR_T(&smbzeroes[0]), len);
		error = smb_write(smp->sm_share, fid, uio, scredp, timo);
		timo = 0;
		np->n_flag |= (NFLUSHWIRE | NATTRCHANGED);
		if (error)
			break;
			/* nothing written */
		if (uio_resid(uio) == len) {
			SMBDEBUG(" short from=%llu to=%llu\n", from, to);
			break;
		}
		from += len - uio_resid(uio);
	}

	uio_free(uio);
	return (error);
}

/*
 * One of two things has happen. The file is growing or the file has holes in it. Either case we would like
 * to make sure the data return is zero filled. For UNIX servers we get this for free. So if the server is UNIX
 * just return and let the server handle this issue.
 */
int smbfs_0extend(vnode_t vp, u_int16_t fid, u_quad_t from, u_quad_t to, struct smb_cred *scredp, int timo)
{
	struct smbmount *smp = VTOSMBFS(vp);
	struct smb_vc *vcp = SSTOVC(smp->sm_share);
	int error;

	/*
	 * Make an exception here for UNIX servers. Since UNIX servers always zero fill there is no reason to make this 
	 * call in their case. So if this is a UNIX server just return no error.
	 */
	if (UNIX_SERVER(vcp))
		return(0);
	/* 
	 * We always zero fill the whole amount if the share is not NTFS. We always zero fill NT4 servers and Windows 2000
	 * servers. For all others just write one byte of zero data at the eof of file. This will cause the NTFS windows
	 * servers to zero fill.
	 */
	if ((smp->sm_share->ss_fstype != SMB_FS_NTFS) || ((vcp->vc_flags & SMBV_NT4)) || ((vcp->vc_flags & SMBV_WIN2K)))
		error = smbfs_zero_fill(vp, fid, from, to, scredp, timo);
	else {
		char onezero = 0;
		int len = 1;
		uio_t uio;
		
			/* Writing one byte of zero before the eof will force NTFS to zero fill. */
		uio = uio_create(1, (to - 1) , UIO_SYSSPACE, UIO_WRITE);
		uio_addiov(uio, CAST_USER_ADDR_T(&onezero), len);
		error = smb_write(smp->sm_share, fid, uio, scredp, timo);
		uio_free(uio);
	}
	return(error);
}

PRIVSYM int smbfs_dowrite(vnode_t vp, uio_t uiop, u_int16_t fid, vfs_context_t vfsctx, int ioflag, int timo)
{
	struct smbmount *smp = VTOSMBFS(vp);
	struct smbnode *np = VTOSMB(vp);
	struct smb_cred scred;
	int error = 0;

	SMBVDEBUG("ofs=%d,resid=%d\n",(int)uio_offset(uiop), uio_resid(uiop));
	/*
	 * Changed this code while working on removing extra over the wire flush calls. Didn't
	 * change the excution of the code here. May want to look at that later, but for now
	 * lets only correct the extra smb flush calls.
	 */
	if (ioflag & IO_APPEND)
		uio_setoffset(uiop, np->n_size);

	if (uio_resid(uiop) == 0)
		return (0);

	smb_scred_init(&scred, vfsctx);
	/* We have a hole in the file make sure it gets zero filled */
	if (uio_offset(uiop) > (off_t)np->n_size)
		error = smbfs_0extend(vp, fid, np->n_size, uio_offset(uiop), &scred, timo);

	if (!error)
		error = smb_write(smp->sm_share, fid, uiop, &scred, timo);
	np->n_flag |= (NFLUSHWIRE | NATTRCHANGED);

	if ((!error) && (uio_offset(uiop) > (off_t)np->n_size))
		smbfs_setsize(vp, uio_offset(uiop));
	
	return error;
}
