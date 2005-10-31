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
 * $Id: smbfs_io.c,v 1.41.38.3 2005/07/20 05:26:59 lindak Exp $
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

#include <sys/buf.h>

/* keep build working with older headers */
#ifndef round_page_32
	#define round_page_32 round_page
#endif

/*#define SMBFS_RWGENERIC*/

extern int smbfs_pbuf_freecnt;

static int smbfs_fastlookup = 1;

extern struct linker_set sysctl_net_smb_fs;

#ifdef SYSCTL_DECL
SYSCTL_DECL(_net_smb_fs);
#endif
SYSCTL_INT(_net_smb_fs, OID_AUTO, fastlookup, CTLFLAG_RW, &smbfs_fastlookup, 0, "");


#define DE_SIZE	(int)(sizeof(struct dirent))

static int
smbfs_readvdir(vnode_t vp, uio_t uio, vfs_context_t vfsctx)
{
	struct dirent de;
	struct smb_cred scred;
	struct smbfs_fctx *ctx;
	vnode_t newvp;
	struct smbnode *np = VTOSMB(vp);
	int error/*, *eofflag = ap->a_eofflag*/;
	long offset, limit;

	if (uio_resid(uio) < DE_SIZE || uio_offset(uio) < 0)
		return (EINVAL);

	np = VTOSMB(vp);
	/*
	 * This cache_purge ensures that name cache for this dir will be
	 * current - it'll only have the items for which the smbfs_nget
	 * MAKEENTRY happened.
	 */
	if (smbfs_fastlookup)
		cache_purge(vp);
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
		    &scred, &ctx);
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
			error = smbfs_nget(vnode_mount(vp), vp, ctx->f_name,
					   ctx->f_nmlen, &ctx->f_attr, &newvp,
					   MAKEENTRY, 0);
			if (!error)
				vnode_put(newvp);
		}
		error = uiomove((caddr_t)&de, DE_SIZE, uio);
		if (error)
			break;
	}
	if (error == ENOENT)
		error = 0;
	return error;
}

int
smbfs_readvnode(vnode_t vp, uio_t uiop, vfs_context_t vfsctx,
		struct vnode_attr *vap)
{
	struct smbmount *smp = VFSTOSMBFS(vnode_mount(vp));
	struct smbnode *np = VTOSMB(vp);
	struct smb_cred scred;
	int error;
	int requestsize;
	user_ssize_t remainder;
	int vtype;

	vtype = vnode_vtype(vp);

	if (vtype != VREG && vtype != VDIR && vtype != VLNK) {
		SMBFSERR("smbfs_readvnode only supports VREG/VDIR/VLNK!\n");
		return (EIO);
	}
	if (uio_resid(uiop) == 0)
		return (0);
	if (uio_offset(uiop) < 0)
		return (EINVAL);
/*	if (uio_offset(uiop) + uio_resid(uiop) > smp->nm_maxfilesize)
		return EFBIG;*/
	if (vtype == VDIR)
		return (smbfs_readvdir(vp, uiop, vfsctx));

/*	biosize = SSTOCN(smp->sm_share)->sc_txmax;*/
	smb_scred_init(&scred, vfsctx);
	(void) smbfs_smb_flush(np, &scred);
	
	if (uio_offset(uiop) >= (off_t)vap->va_data_size) {
		/* if offset is beyond EOF, read nothing */
		error = 0;
		goto exit;
	}
	
	/* pin requestsize to EOF */
	requestsize = MIN(uio_resid(uiop),
			  (off_t)(vap->va_data_size - uio_offset(uiop)));
	
	/* subtract requestSize from uio_resid and save remainder */
	remainder = uio_resid(uiop) - requestsize;
	
	/* adjust size of read */
	uio_setresid(uiop, requestsize);
	
	error = smb_read(smp->sm_share, np->n_fid, uiop, &scred);
	
	/* set remaining uio_resid */
	uio_setresid(uiop, (uio_resid(uiop) + remainder));

exit:
	
	return error;
}

char smbzeroes[4096] = { 0 };

int
smbfs_0extend(vnode_t vp, u_int16_t fid, u_quad_t from, u_quad_t to,
	      struct smb_cred *scredp, int timo)
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
		if (uio_resid(uio) == len) { /* nothing written?!? */
			log(LOG_WARNING,
			    "smbfs_0extend: short from=%d to=%d\n", from, to);
			break;
		}
		from += len - uio_resid(uio);
	}

	uio_free(uio);
	return (error);
}

int
smbfs_writevnode(vnode_t vp, uio_t uiop,
	vfs_context_t vfsctx, int ioflag, int timo)
{
	struct smbmount *smp = VTOSMBFS(vp);
	struct smbnode *np = VTOSMB(vp);
	struct smb_cred scred;
	int error = 0;

	if (!vnode_isreg(vp)) {
		SMBERROR("vn types other than VREG unsupported !\n");
		return (EIO);
	}
	SMBVDEBUG("ofs=%d,resid=%d\n",(int)uio_offset(uiop), uio_resid(uiop));
	if (uio_offset(uiop) < 0)
		return (EINVAL);
/*	if (uio_offset(uiop) + uio_resid(uiop) > smp->nm_maxfilesize)
		return (EFBIG);*/
	if (ioflag & (IO_APPEND | IO_SYNC)) {
		if (np->n_flag & NMODIFIED) {
			smbfs_attr_cacheremove(np);
			error = smbfs_vinvalbuf(vp, V_SAVE, vfsctx, 1);
			if (error)
				return error;
		}
		if (ioflag & IO_APPEND) {
#if notyet
			struct vnode_attr vattr;
			/*
			 * File size can be changed by another client
			 */
			smbfs_attr_cacheremove(np);
			VATTR_INIT(&vattr);
			VATTR_WANTED(&vattr, va_data_size);
			error = smbi_getattr(vp, &vattr, vfsctx);
			if (error)
				return (error);
#endif
			uio_setoffset(uiop, np->n_size);
		}
	}
	if (uio_resid(uiop) == 0)
		return (0);

	smb_scred_init(&scred, vfsctx);
	if (uio_offset(uiop) > (off_t)np->n_size) {
		// LP64todo - need to handle 64-bit offset value 
		error = smbfs_0extend(vp, np->n_fid, np->n_size,
				      uio_offset(uiop), &scred, timo);
		timo = 0;
		if (!error)
			smbfs_setsize(vp, uio_offset(uiop));
	}
	if (!error)
		error = smb_write(smp->sm_share, np->n_fid, uiop, &scred, timo);
	np->n_flag |= (NFLUSHWIRE | NATTRCHANGED);
	SMBVDEBUG("after: ofs=%d,resid=%d\n",(int)uio_offset(uiop), uio_resid(uiop));
	if (!error) {
		if (uio_offset(uiop) > (off_t)np->n_size)
			smbfs_setsize(vp, uio_offset(uiop));
	}
	return error;
}

static int
smbfs_vinvalbuf_internal(vnode_t vp, int flags, vfs_context_t vfsctx, int slpflg)
{
	int error = 0;

	if (flags & BUF_WRITE_DATA)
		error = smbi_fsync(vp, MNT_WAIT, vfsctx);
	if (!error)
		error = buf_invalidateblks(vp, flags, slpflg, 0);
	return (error);
}

/*
 * Flush and invalidate all dirty buffers. If another process is already
 * doing the flush, just wait for completion.
 */
PRIVSYM int
smbfs_vinvalbuf(vp, flags, vfsctx, intrflg)
	vnode_t vp;
	int flags;
	vfs_context_t vfsctx;
	int intrflg;
{
	struct smbnode *np = VTOSMB(vp);
	int error = 0, slpflag, slptimeo;
	int lasterror = ENXIO;
	struct timespec ts;
	off_t size;

	if (intrflg) {
		slpflag = PCATCH;
		ts.tv_sec = 2;
		ts.tv_nsec = 0;
		slptimeo = 1;
	} else {
		slpflag = 0;
		slptimeo = 0;
	}
	while (np->n_flag & NFLUSHINPROG) {
		np->n_flag |= NFLUSHWANT;
		error = msleep((caddr_t)&np->n_flag, 0, PRIBIO + 2, "smfsvinv", slptimeo? &ts:0);
		error = smb_sigintr(vfsctx);
		if (error == EINTR && intrflg)
			return (EINTR);
	}
	np->n_flag |= NFLUSHINPROG;
	error = smbfs_vinvalbuf_internal(vp, flags, vfsctx, slpflag);
	while (error && error != lasterror) {
		if (intrflg && (error == ERESTART || error == EINTR)) {
			np->n_flag &= ~NFLUSHINPROG;
			if (np->n_flag & NFLUSHWANT) {
				np->n_flag &= ~NFLUSHWANT;
				wakeup((caddr_t)&np->n_flag);
			}
			return (EINTR);
		}
		lasterror = error;
		/* Avoid potential CPU loop by yielding for at least 0.1 sec */
		ts.tv_sec= 0;
		ts.tv_nsec = 100 *1000 *1000;
		(void)msleep(NULL, NULL, PWAIT, "smbfs_vinvalbuf", &ts);
		error = smbfs_vinvalbuf_internal(vp, flags, vfsctx, slpflag);
	}
	np->n_flag &= ~(NMODIFIED | NFLUSHINPROG);
	if (np->n_flag & NFLUSHWANT) {
		np->n_flag &= ~NFLUSHWANT;
		wakeup((caddr_t)&np->n_flag);
	}
	/* get the pages out of vm also */
	size = smb_ubc_getsize(vp);
	if (size && !ubc_sync_range(vp, (off_t)0, size,
				    UBC_PUSHALL | UBC_INVALIDATE))
		SMBERROR("ubc_sync_range failure");
	return (error);
}
