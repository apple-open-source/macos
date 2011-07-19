/*
 * Copyright (c) 2000-2001, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2010 Apple Inc. All rights reserved.
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

#include <smbfs/smbfs.h>
#include <smbfs/smbfs_node.h>
#include <smbfs/smbfs_subr.h>
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
static uint32_t 
smbfs_fill_direntry(void *de, const char *name, size_t nmlen, uint8_t dtype, 
					uint64_t ino, int flags)
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
static int 
smb_add_next_entry(struct smbnode *np, uio_t uio, int flags, int32_t *numdirent)
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
			delen = smbfs_fill_direntry(de, de64p->d_name, de64p->d_namlen, 
										de64p->d_type, de64p->d_fileno, flags);
		} else {
			/* Have a dirent need a direntry */
			struct dirent *de32p = np->d_nextEntry;
			delen = smbfs_fill_direntry(de, de32p->d_name, de32p->d_namlen, 
										de32p->d_type, de32p->d_fileno, flags);				
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

int 
smbfs_readvdir(vnode_t dvp, uio_t uio, vfs_context_t context, int flags, 
			   int32_t *numdirent)
{
	struct smbnode *dnp = VTOSMB(dvp);
	union {
		struct dirent de32;
		struct direntry de64;		
	}de;
	struct smbfs_fctx *ctx;
	off_t offset;
	uint8_t dtype;
	uint32_t delen;
	int error = 0;
	struct smb_share * share = NULL;
		
	/* Do we need to start or restarting the directory listing */
	offset = uio_offset(uio);
	
	share = smb_get_share_with_reference(VTOSMBFS(dvp));
	if (!dnp->d_fctx || (dnp->d_fctx->f_share != share) || (offset == 0) || 
		(offset != dnp->d_offset)) {
		SMBVDEBUG("Reopening search for %s %lld:%lld\n", dnp->n_name, offset, dnp->d_offset);
		smbfs_closedirlookup(dnp, context);
		error = smbfs_smb_findopen(share, dnp, "*", 1, context, &dnp->d_fctx, TRUE);
	}
	/* 
	 * The directory fctx keeps a reference on the share so we can release our 
	 * reference on the share now.
	 */ 
	smb_share_rele(share, context);
	
	if (error) {
		SMBWARNING("Can't open search for %s, error = %d", dnp->n_name, error);
		goto done;			
	}
	ctx = dnp->d_fctx;
	
	SMBVDEBUG("dirname='%s' offset %lld \n", dnp->n_name, offset);
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
			uint64_t node_ino = (ii == 0) ? dnp->n_ino : (dnp->n_parent ? dnp->n_parent->n_ino : 2);

			delen = smbfs_fill_direntry(&de, "..", ii + 1, DT_DIR, node_ino, flags);
			/* 
			 * At this point we always expect them to have enough room for dot
			 * and dotdot. If not enough room then uiomove will return an error.
			 */
			error = uiomove((void *)&de, delen, uio);
			if (error)
				goto done;
			(*numdirent)++;
			dnp->d_offset++;
			offset++;
		}
	}

	/* 
	 * They are continuing from some point ahead of us in the buffer. Skip all
	 * entries until we reach their point in the buffer.
	 */
	while (dnp->d_offset < offset) {
		error = smbfs_findnext(ctx, context);
		if (error) {
			smbfs_closedirlookup(dnp, context);
			goto done;			
		}
		dnp->d_offset++;
	}
	/* We have an entry left over from before we need to put it into the users
	 * buffer before doing any more searches. 
	 */
	if (dnp->d_nextEntry) {
		error = smb_add_next_entry(dnp, uio, flags, numdirent);	
		if (error)
			goto done;
	}
	
	/* Loop until we end the search or we don't have enough room for the max element */
	while (uio_resid(uio)) {
		error = smbfs_findnext(ctx, context);
		if (error)
			break;
		dtype = (ctx->f_attr.fa_attr & SMB_EFA_DIRECTORY) ? DT_DIR : DT_REG;
		delen = smbfs_fill_direntry(&de, ctx->f_LocalName, ctx->f_LocalNameLen, 
									dtype, ctx->f_attr.fa_ino, flags);
		if (smbfs_fastlookup) {
			vnode_t vp = NULL;
			
			error = smbfs_nget(ctx->f_share, vnode_mount(dvp), dvp, ctx->f_LocalName, 
								ctx->f_LocalNameLen, &ctx->f_attr, &vp, 
								MAKEENTRY, context);
			if (error == 0) {
				struct smbnode *np = VTOSMB(vp);

				/* 
				 * Some applications use the inode as a marker and expect it to 
				 * be presistent. Currently our inode numbers are create by 
				 * hashing the name and adding the parent inode number. Once a node
				 * is create we should try to keep the same inode number though 
				 * out its life. The smbfs_nget will either create the node or 
				 * return one found in the hash table. The one that gets created 
				 * will use ctx->f_attr.fa_ino, but if its in our hash table it 
				 * will have its original number. So in either case set the file 
				 * number to the inode number that was used when the node was created.
				 */
				if (flags & VNODE_READDIR_EXTENDED)
					de.de64.d_fileno = np->n_ino;
				else
					de.de32.d_fileno = (ino_t)np->n_ino;
				/*
				 * The server should always return the correct case of the name
				 * in the directory listing. We currently assume the remote file 
				 * system is case insensitive, since we have no way of telling 
				 * using the protocol. Someday I would like to detect and if the 
				 * server is case sensitive. So if the node name doesn't match 
				 * the direcotry listing name, then update the name to match.
				 * The smbfs_nget routine has already made sure its a case
				 * insensitive match, this includes the length field.
				 */
				if (bcmp(ctx->f_LocalName, np->n_name, ctx->f_LocalNameLen) != 0) {
					struct componentname cnp;
					char *new_name;
					
					SMBWARNING("Case changed was %s now %s\n", np->n_name, ctx->f_LocalName);
					/* Different case, update the node */
					new_name = smb_strndup(ctx->f_LocalName, ctx->f_LocalNameLen);
					if (new_name) {
						char *old_name = np->n_name;

						lck_rw_lock_exclusive(&np->n_name_rwlock);
						np->n_name = new_name;
						np->n_nmlen = ctx->f_LocalNameLen;
						lck_rw_unlock_exclusive(&np->n_name_rwlock);
						/* Reset the cache node */
						bzero(&cnp, sizeof(cnp));
						cnp.cn_nameptr = (char *)np->n_name;
						cnp.cn_namelen = (int)np->n_nmlen;
						cnp.cn_flags = MAKEENTRY;
						 /* Remove old entry, wrong case */
						cache_purge(vp);
						/* Added new entry, correct case */
						cache_enter(dvp, vp, &cnp);
						SMB_FREE(old_name, M_SMBNODENAME);
					}
				}
				smbnode_unlock(np);	/* Release the smbnode lock */
				vnode_put(vp);
			} else  {
				/* ignore errors from smbfs_nget, shouldn't stop the directory listing */
				error = 0;
			}
		}
		/* Name wouldn't fit in the directory entry just drop it nothing else we can do */
		if (delen == 0)
			continue;
		if (uio_resid(uio) >= delen) {
			error = uiomove((void *)&de, delen, uio);
			if (error)
				break;
			(*numdirent)++;
			dnp->d_offset++;				
		} else {
			SMBVDEBUG("%s, Saving Next Entry %s,  resid == %lld\n", dnp->n_name, 
					  ctx->f_LocalName, uio_resid(uio));
			MALLOC(dnp->d_nextEntry, void *, delen, M_TEMP, M_WAITOK);
			if (dnp->d_nextEntry) {
				bcopy(&de, dnp->d_nextEntry, delen);
				dnp->d_nextEntryLen = delen;
				dnp->d_nextEntryFlags = (flags & VNODE_READDIR_EXTENDED);
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
	uio_setoffset(uio, dnp->d_offset);

	if (error && (error != ENOENT)) {
		SMBWARNING("%s directory lookup failed with error of %d\n", dnp->n_name, error);		
	}
	return error;
}

/* 
 * This routine will zero fill the data between from and to. We may want to allocate
 * smbzeroes in the future.
 *
 * The calling routine must hold a reference on the share
 */
static char smbzeroes[4096] = { 0 };

static int
smbfs_zero_fill(struct smb_share *share, uint16_t fid, u_quad_t from, u_quad_t to, 
				int ioflag, vfs_context_t context)
{
	user_size_t len;
	int error = 0;
	uio_t uio;

	/*
	 * Coherence callers must prevent VM from seeing the file size
	 * grow until this loop is complete.
	 */
	uio = uio_create(1, from, UIO_SYSSPACE, UIO_WRITE);
	while (from < to) {
		len = MIN((to - from), sizeof(smbzeroes));
		uio_reset(uio, from, UIO_SYSSPACE, UIO_WRITE );
		uio_addiov(uio, CAST_USER_ADDR_T(&smbzeroes[0]), len);
		error = smb_write(share, fid, uio, ioflag, context);
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
 * One of two things has happen. The file is growing or the file has holes in it. 
 * Either case we would like to make sure the data return is zero filled. For 
 * UNIX servers we get this for free. So if the server is UNIX just return and 
 * let the server handle this issue.
 *
 * The calling routine must hold a reference on the share
 *
 */
int 
smbfs_0extend(struct smb_share *share, uint16_t fid, u_quad_t from, 
				  u_quad_t to, int ioflag, vfs_context_t context)
{
	int error;

	/*
	 * Make an exception here for UNIX servers. Since UNIX servers always zero 
	 * fill there is no reason to make this call in their case. So if this is a 
	 * UNIX server just return no error.
	 */
	if (UNIX_SERVER(SSTOVC(share)))
		return(0);
	/* 
	 * We always zero fill the whole amount if the share is not NTFS. We always 
	 * zero fill NT4 servers and Windows 2000 servers. For all others just write 
	 * one byte of zero data at the eof of file. This will cause the NTFS windows
	 * servers to zero fill.
	 */
	if ((share->ss_fstype != SMB_FS_NTFS) || 
		((SSTOVC(share)->vc_flags & SMBV_NT4)) || 
		((SSTOVC(share)->vc_flags & SMBV_WIN2K_XP))) {
		error = smbfs_zero_fill(share, fid, from, to, ioflag, context);
	} else {
		char onezero = 0;
		int len = 1;
		uio_t uio;
		
		/* Writing one byte of zero before the eof will force NTFS to zero fill. */
		uio = uio_create(1, (to - 1) , UIO_SYSSPACE, UIO_WRITE);
		uio_addiov(uio, CAST_USER_ADDR_T(&onezero), len);
		error = smb_write(share, fid, uio, ioflag, context);
		uio_free(uio);
	}
	return(error);
}

/*
 * The calling routine must hold a reference on the share
 */
int 
smbfs_doread(struct smb_share *share, off_t endOfFile, uio_t uiop, 
			 vfs_context_t context, uint16_t fid)
{
	int error;
	user_ssize_t requestsize;
	user_ssize_t remainder;
	
	/* if offset is beyond EOF, read nothing */
	if (uio_offset(uiop) >= endOfFile) {
		error = 0;
		goto exit;
	}
	
	/* pin requestsize to EOF */
	requestsize = MIN(uio_resid(uiop), endOfFile - uio_offset(uiop));
	
	/* subtract requestSize from uio_resid and save remainder */
	remainder = uio_resid(uiop) - requestsize;
	
	/* adjust size of read */
	uio_setresid(uiop, requestsize);
	
	error = smb_read(share, fid, uiop, context);
	
	/* set remaining uio_resid */
	uio_setresid(uiop, (uio_resid(uiop) + remainder));
	
exit:
	
	return error;
}

/*
 * %%%  Radar 4573627 We should resend the write if we failed because of a 
 * reconnect. We need to dup the uio before the write and if it fails reset 
 * it back to the dup verison.
 *
 * The calling routine must hold a reference on the share
 *
 */
int 
smbfs_dowrite(struct smb_share *share, off_t endOfFile, uio_t uiop, 
				  uint16_t fid, int ioflag, vfs_context_t context)
{
	int error = 0;

	SMBVDEBUG("ofs=%lld,resid=%lld\n",uio_offset(uiop), uio_resid(uiop));

	if (uio_resid(uiop) == 0)
		return (0);

	/* 
	 * When the pageout and strategy routines call this function n_size should
	 * always be bigger than the offset. We count on this so if we every change
	 * that behavior this code will need to be changed.
	 * We have a hole in the file make sure it gets zero filled 
	 */
	if (uio_offset(uiop) > endOfFile) {
		error = smbfs_0extend(share, fid, endOfFile, uio_offset(uiop), ioflag, context);
	}

	if (!error) {
		error = smb_write(share, fid, uiop, ioflag, context);
	}

	return error;
}
