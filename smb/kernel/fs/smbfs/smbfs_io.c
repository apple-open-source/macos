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
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/vnode.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/dirent.h>
#include <sys/sysctl.h>
#include <sys/kauth.h>

#include <sys/smb_apple.h>
#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_subr.h>

#include <fs/smbfs/smbfs.h>
#include <fs/smbfs/smbfs_node.h>
#include <fs/smbfs/smbfs_subr.h>
#include <netsmb/smb_converter.h>

static int smbfs_fastlookup = 1;

SYSCTL_DECL(_net_smb_fs);
SYSCTL_INT(_net_smb_fs, OID_AUTO, fastlookup, CTLFLAG_RW, &smbfs_fastlookup, 0, "");

/*
 * In the future I would like to move all the read directory code into
 * its own file, but for now lets leave it here.
 */
#define SMB_DIRENTRY_LEN(namlen) \
		((sizeof(struct direntry) + (namlen) - (MAXPATHLEN-1) + 7) & ~7)
#define SMB_DIRENT_LEN(namlen) \
		((sizeof(struct dirent) - (NAME_MAX+1)) + (((namlen) + 1 + 3) &~ 3))

/*
 * This routine will fill in the correct values for the correct structure. The
 * de value points t0 either a direntry or dirent structure.
 */
static uint32_t smbfs_fill_direntry(void *de, const char *name, size_t nmlen, u_int8_t dtype, u_int64_t ino, int flags)
{
	uint32_t delen = 0;
	
	if (flags & VNODE_READDIR_EXTENDED) {
		struct direntry *de64 = (struct direntry *)de;

		/* Never truncate the name, if it won't fit just drop it */
		if (nmlen >= sizeof(de64->d_name))
			return 0;
		bzero(de64, sizeof(*de64));
		de64->d_fileno = ino;
		de64->d_type = dtype;
		de64->d_namlen = nmlen;
		bcopy(name, de64->d_name, de64->d_namlen);
		delen = de64->d_reclen = SMB_DIRENTRY_LEN(de64->d_namlen);
		SMBVDEBUG("de64.d_name = %s de64.d_namlen = %d\n", de64->d_name, de64->d_namlen);
	} else {
		struct dirent *de32 = (struct dirent *)de;
		
		/* Never truncate the name, if it won't fit just drop it */
		if (nmlen >= sizeof(de32->d_name))
			return 0;
		bzero(de32, sizeof(*de32));
		de32->d_fileno = (ino_t)ino;
		de32->d_type = dtype;
		/* Should never happen, but just in case never overwrite the buffer */
		de32->d_namlen = nmlen;
		bcopy(name, de32->d_name, de32->d_namlen);
		delen = de32->d_reclen = SMB_DIRENT_LEN(de32->d_namlen);
		SMBVDEBUG("de32.d_name = %s de32.d_namlen = %d\n", de32->d_name, de32->d_namlen);
	}
	return delen;
}

/* We have an entry left over from before we need to put it into the users
 * buffer before doing any more searches. At this point we always expect 
 * them to have enough room for one entry. If not enough room then uiomove 
 * will return an error. We need to check and make sure they are using the
 * same size structure as the last lookup. If not reset the entry before
 * doing the uiomove.
 */
static int smb_add_next_entry(struct smbnode *np, uio_t uio, int flags, int32_t *numdirent)
{
	union {
		struct dirent de32;
		struct direntry de64;		
	}hold_de;
	void *de  = &hold_de;
	uint32_t delen;
	int	error = 0;

	SMBVDEBUG("%s, Using Next Entry %s\n",  np->n_name,   (flags & VNODE_READDIR_EXTENDED) ? 
				  ((struct direntry *)(np->d_nextEntry))->d_name : ((struct dirent *)(np->d_nextEntry))->d_name);
		
	if (np->d_nextEntryFlags != (flags & VNODE_READDIR_EXTENDED)) {
		SMBVDEBUG("Next Entry flags don't match was 0x%x now 0x%x\n",
				  np->d_nextEntryFlags, (flags & VNODE_READDIR_EXTENDED));
		if (np->d_nextEntryFlags & VNODE_READDIR_EXTENDED) {
			/* Have a direntry need a dirent */
			struct direntry *de64p = np->d_nextEntry;
			delen = smbfs_fill_direntry(de, de64p->d_name, de64p->d_namlen, de64p->d_type, de64p->d_fileno, flags);
		} else {
			/* Have a dirent need a direntry */
			struct dirent *de32p = np->d_nextEntry;
			delen = smbfs_fill_direntry(de, de32p->d_name, de32p->d_namlen, de32p->d_type, de32p->d_fileno, flags);				
		}
	} else {
		de = np->d_nextEntry;
		delen = np->d_nextEntryLen;
	}
	/* Name wouldn't fit in the directory entry just drop it nothing else we can do */
	if (delen == 0)
		goto done;
	error = uiomove(de, delen, uio);
	if (error)
		goto done;
	(*numdirent)++;
	np->d_offset++;
	
done:	
	FREE(np->d_nextEntry, M_TEMP);
	np->d_nextEntry = NULL;
	np->d_nextEntryLen = 0;
	return error;
}

int smbfs_readvdir(vnode_t vp, uio_t uio, vfs_context_t context, int flags, int32_t *numdirent)
{
	struct smbnode *np = VTOSMB(vp);
	union {
		struct dirent de32;
		struct direntry de64;		
	}de;
	struct smbfs_fctx *ctx;
	vnode_t newvp;
	off_t offset;
	u_int8_t dtype;
	uint32_t delen;
	int error = 0;
		
	/* Do we need to start or restarting the directory listing */
	offset = uio_offset(uio);
	if ((offset == 0) || (offset != np->d_offset) || (np->d_fctx == NULL)) {
		SMBVDEBUG("Reopening search for %s %lld:%lld\n", np->n_name, offset, np->d_offset);
		smbfs_closedirlookup(np, context);
		error = smbfs_smb_findopen(np, "*", 1, context, &np->d_fctx, TRUE);
		if (error) {
			SMBWARNING("Can't open search for %s, error = %d", np->n_name, error);
			goto done;			
		}
	}
	ctx = np->d_fctx;
	
	SMBVDEBUG("dirname='%s' offset %lld \n", np->n_name, offset);
	/* 
	 * SMB servers will return the dot and dotdot in most cases. If the share is a 
	 * FAT Filesystem then the information return could be bogus, also if its a
	 * FAT drive then they won't even return the dot or the dotdot. Since we already
	 * know everything about dot and dotdot just fill them in here and then skip
	 * them during the lookup.
	 */
	if (offset == 0) {
		int ii;
		
		for (ii=0; ii < 2; ii++) {
			u_int64_t node_ino = (ii == 0) ? np->n_ino : (np->n_parent ? np->n_parent->n_ino : 2);
			/* 
			 * XXX
			 * I don't believe the the two lines below are needed. Wrote up  <rdar://problem/6693209> 
			 * to cover that work. Would like to investage some more before removing it.
			 */
			if (node_ino == 0)
				node_ino = 0x7ffffffd + ii;
			delen = smbfs_fill_direntry(&de, "..", ii + 1, DT_DIR, node_ino, flags);
			/* 
			 * At this point we always expect them to have enough room for dot
			 * and dotdot. If not enough room then uiomove will return an error.
			 */
			error = uiomove((void *)&de, delen, uio);
			if (error)
				goto done;
			(*numdirent)++;
			np->d_offset++;
			offset++;
		}
	}

	/* 
	 * They are continuing from some point ahead of us in the buffer. Skip all
	 * entries until we reach their point in the buffer.
	 */
	while (np->d_offset < offset) {
		error = smbfs_smb_findnext(ctx, context);
		if (error) {
			smbfs_closedirlookup(np, context);
			goto done;			
		}
		np->d_offset++;
	}
	/* We have an entry left over from before we need to put it into the users
	 * buffer before doing any more searches. 
	 */
	if (np->d_nextEntry) {
		error = smb_add_next_entry(np, uio, flags, numdirent);	
		if (error)
			goto done;
	}
	
	/* Loop until we end the search or we don't have enough room for the max element */
	while (uio_resid(uio)) {
		error = smbfs_smb_findnext(ctx, context);
		if (error)
			break;
		dtype = (ctx->f_attr.fa_attr & SMB_FA_DIR) ? DT_DIR : DT_REG;
		delen = smbfs_fill_direntry(&de, ctx->f_name, ctx->f_nmlen, dtype, ctx->f_attr.fa_ino, flags);
		if (smbfs_fastlookup) {
			error =  smbfs_nget(vnode_mount(vp), vp, ctx->f_name, ctx->f_nmlen, &ctx->f_attr, &newvp, MAKEENTRY, context);
			if (error == 0) {
				/* 
				 * Some applications use the inode as a marker and expect it to be presistent. Currently our
				 * inode numbers are create by hashing the name and adding the parent inode number. Once a node
				 * is create we should try to keep the same inode number though out its life. The smbfs_nget will either
				 * create the node or return one found in the hash table. The one that gets created will use 
				 * ctx->f_attr.fa_ino, but if its in our hash table it will have its original number. So in either
				 * case set the file number to the inode number that was used when the node was created.
				 */
				if (flags & VNODE_READDIR_EXTENDED)
					de.de64.d_fileno = VTOSMB(newvp)->n_ino;
				else
					de.de32.d_fileno = (ino_t)VTOSMB(newvp)->n_ino;
				smbnode_unlock(VTOSMB(newvp));	/* Release the smbnode lock */
				vnode_put(newvp);
			} else  /* ignore errors from smbfs_nget, shouldn't stop the directory listing */
				error = 0;
		}
		/* Name wouldn't fit in the directory entry just drop it nothing else we can do */
		if (delen == 0)
			continue;
		if (uio_resid(uio) >= delen) {
			error = uiomove((void *)&de, delen, uio);
			if (error)
				break;
			(*numdirent)++;
			np->d_offset++;				
		} else {
			SMBVDEBUG("%s, Saving Next Entry %s,  resid == %lld\n", np->n_name, 
					  ctx->f_name, uio_resid(uio));
			MALLOC(np->d_nextEntry, void *, delen, M_TEMP, M_WAITOK);
			if (np->d_nextEntry) {
				bcopy(&de, np->d_nextEntry, delen);
				np->d_nextEntryLen = delen;
				np->d_nextEntryFlags = (flags & VNODE_READDIR_EXTENDED);
			}
			break;
		}
	}
done: 
	/*
	 * We use the uio offset to store the last directory index count. Since 
	 * the uio offset is really never used, we can set it without causing any 
	 * issues. Got this idea from the NFS code and it makes things a 
	 * lot simplier. 
	 */
	uio_setoffset(uio, np->d_offset);

	if (error && (error != ENOENT)) {
		SMBWARNING("%s directory lookup failed with error of %d\n", np->n_name, error);		
	}
	return error;
}

int smbfs_doread(vnode_t vp, uio_t uiop, vfs_context_t context, u_int16_t fid)
{
	struct smbmount *smp = VFSTOSMBFS(vnode_mount(vp));
	struct smbnode *np = VTOSMB(vp);
	int error;
	user_ssize_t requestsize;
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
	
	error = smb_read(smp->sm_share, fid, uiop, context);
	
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
smbfs_zero_fill(vnode_t vp, u_int16_t fid, u_quad_t from, u_quad_t to, 
				vfs_context_t context, int timo)
{
	struct smbmount *smp = VTOSMBFS(vp);
	user_size_t len;
	int error = 0;
	uio_t uio;

	/*
	 * XXX for coherence callers must prevent VM from seeing the file size
	 * grow until this loop is complete.
	 */
	uio = uio_create(1, from, UIO_SYSSPACE, UIO_WRITE);
	while (from < to) {
		len = MIN((to - from), sizeof(smbzeroes));
		uio_reset(uio, from, UIO_SYSSPACE, UIO_WRITE );
		uio_addiov(uio, CAST_USER_ADDR_T(&smbzeroes[0]), len);
		error = smb_write(smp->sm_share, fid, uio, context, timo);
		timo = 0;
		if (error)
			break;
			/* nothing written */
		if (uio_resid(uio) == (user_ssize_t)len) {
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
int smbfs_0extend(vnode_t vp, u_int16_t fid, u_quad_t from, u_quad_t to, 
				  vfs_context_t context, int timo)
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
	if ((smp->sm_share->ss_fstype != SMB_FS_NTFS) || ((vcp->vc_flags & SMBV_NT4)) || ((vcp->vc_flags & SMBV_WIN2K_XP)))
		error = smbfs_zero_fill(vp, fid, from, to, context, timo);
	else {
		char onezero = 0;
		int len = 1;
		uio_t uio;
		
			/* Writing one byte of zero before the eof will force NTFS to zero fill. */
		uio = uio_create(1, (to - 1) , UIO_SYSSPACE, UIO_WRITE);
		uio_addiov(uio, CAST_USER_ADDR_T(&onezero), len);
		error = smb_write(smp->sm_share, fid, uio, context, timo);
		uio_free(uio);
	}
	return(error);
}

int smbfs_dowrite(vnode_t vp, uio_t uiop, u_int16_t fid, vfs_context_t context, 
				  int ioflag, int timo)
{
	struct smbmount *smp = VTOSMBFS(vp);
	struct smbnode *np = VTOSMB(vp);
	int error = 0;

	SMBVDEBUG("ofs=%lld,resid=%lld\n",uio_offset(uiop), uio_resid(uiop));
	/*
	 * Changed this code while working on removing extra over the wire flush calls. Didn't
	 * change the excution of the code here. May want to look at that later, but for now
	 * lets only correct the extra smb flush calls.
	 */
	if (ioflag & IO_APPEND)
		uio_setoffset(uiop, np->n_size);

	if (uio_resid(uiop) == 0)
		return (0);

	/* We have a hole in the file make sure it gets zero filled */
	if (uio_offset(uiop) > (off_t)np->n_size)
		error = smbfs_0extend(vp, fid, np->n_size, uio_offset(uiop), context, timo);

	if (!error)
		error = smb_write(smp->sm_share, fid, uiop, context, timo);
	np->n_flag |= (NFLUSHWIRE | NATTRCHANGED);

	if ((!error) && (uio_offset(uiop) > (off_t)np->n_size))
		smbfs_setsize(vp, uio_offset(uiop));
	
	return error;
}
