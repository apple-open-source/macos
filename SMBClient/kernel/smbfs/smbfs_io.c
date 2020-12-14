/*
 * Copyright (c) 2000-2001, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2017 Apple Inc. All rights reserved.
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
#include <sys/msfscc.h>

#include <sys/smb_apple.h>
#include <netsmb/smb.h>
#include <netsmb/smb_2.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_rq_2.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_conn_2.h>
#include <netsmb/smb_subr.h>

#include <smbfs/smbfs.h>
#include <smbfs/smbfs_node.h>
#include <smbfs/smbfs_subr.h>
#include <smbfs/smbfs_subr_2.h>
#include <netsmb/smb_converter.h>
#include <smbclient/smbclient_internal.h>

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
smb_add_next_entry(struct smbnode *np, uio_t uio, int flags)
{
	union {
		struct dirent de32;
		struct direntry de64;		
	}hold_de;
	void *de  = &hold_de;
	uint32_t delen;
	int	error = 0;

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

    if (flags & VNODE_READDIR_EXTENDED) {
        struct direntry *de64p = de;
        SMB_LOG_DIR_CACHE_LOCK(np, "Add in saved <%s> in <%s> \n",
                               de64p->d_name, np->n_name);
    }
    else {
        struct dirent *de32p = de;
       SMB_LOG_DIR_CACHE_LOCK(np, "Add in saved <%s> in <%s> \n",
                               de32p->d_name, np->n_name);
    }


	error = uiomove(de, delen, uio);
	if (error)
		goto done;
	
done:	
	SMB_FREE(np->d_nextEntry, M_TEMP);
	np->d_nextEntry = NULL;
	np->d_nextEntryLen = 0;
	return error;
}

static int smbfs_add_dir_entry(vnode_t dvp, uio_t uio, int flags,
                               const char *name, size_t name_len, struct smbfattr *fap)
{
    uint8_t dtype = 0;
    uint32_t delen;
    union {
        struct dirent de32;
        struct direntry de64;
    } de;
    int error = 0;
    struct smbnode *dnp = VTOSMB(dvp);

    /*
     * SMB currently only supports types of DT_DIR, DT_REG and DT_LNK.
     * For symbolic links, there are two types for SMB v2/3.
     * 1. Reparse Points
     * 2. Conrad and Steve French symlinks which are regular files, but with
     *    special data format that identifies it as a sym link. This type of
     *    sym link we can only report as DT_REG because we dont want to read
     *    every file we encounter just to see if its a sym link.
     */
    if (fap->fa_attr & SMB_EFA_DIRECTORY) {
        /* Its a dir */
        dtype = DT_DIR;
    }
    else {
        if ((fap->fa_valid_mask & FA_VTYPE_VALID) && (fap->fa_vtype == VLNK)) {
            /* Its a reparse point sym link */
            dtype = DT_LNK;
        }
        else {
            /* Default, it must be a file */
            dtype = DT_REG;
        }
    }

    delen = smbfs_fill_direntry(&de, name, name_len, dtype, fap->fa_ino,
                                flags);
    
    /*
     * Name wouldn't fit in the directory entry just drop it nothing
     * else we can do
     */
    if (delen == 0) {
        SMB_LOG_DIR_CACHE("smbfs_fill_direntry name would not fit for <%s> \n",
                          name);
        error = ENAMETOOLONG;
        goto done;
    }
    
    if (uio_resid(uio) >= delen) {
        error = uiomove((void *)&de, delen, uio);
        if (error) {
            goto done;
        }
    }
    else {
        /* Ran out of space, save for next enumeration */
        SMB_MALLOC(dnp->d_nextEntry, void *, delen, M_TEMP, M_WAITOK);
        if (dnp->d_nextEntry) {
            bcopy(&de, dnp->d_nextEntry, delen);
            dnp->d_nextEntryLen = delen;
            dnp->d_nextEntryFlags = (flags & VNODE_READDIR_EXTENDED);
            
            SMB_LOG_DIR_CACHE_LOCK(dnp, "Save <%s> for next enum in <%s> \n",
                                   name, dnp->n_name);
        }
        
        error = ENOBUFS;
        goto done;
    }
    
done:
    return (error);
}

int
smbfs_readvdir(vnode_t dvp, uio_t uio, int flags, int32_t *numdirent,
               vfs_context_t context)
{
	struct smbnode *dnp = VTOSMB(dvp);
	union {
		struct dirent de32;
		struct direntry de64;
	} de;
	struct smbfs_fctx *ctx = NULL;
	off_t offset;
	uint32_t delen;
	int error = 0;
	struct smb_share *share = NULL;
    uint64_t node_ino;
    const char *name = NULL;
    size_t name_len = 0;
	struct smbfattr *fap = NULL;
   	struct smb_session *sessionp = NULL;
    int i;
    vnode_t par_vp = NULL;
    int first = 0;
    char *last_entry_namep = NULL;

    SMB_MALLOC(last_entry_namep,
               char *,
               PATH_MAX,
               M_SMBTEMP,
               M_WAITOK | M_ZERO);

    /*
	 * <31997944> Finder now has special mode where it will use vnop_readdir
	 * to enumerate, then they will display immediately. After displaying, then
	 * they will go back and retrieve the meta data for each item. Thus we 
	 * need to make vnop_readdir return quickly so that Finder can display the
	 * contents quickly.
	 *
	 * Note: Most programs now use getattrlistbulk.
	 */

    /*
     * offset - offset of item to start at
     * d_rdir_offset - offset of next item to fetch from server
     * dnp->d_main_cache.offset - offset of next item to be placed into dir cache
     */
    
    share = smb_get_share_with_reference(VTOSMBFS(dvp));
   	sessionp = SS_TO_SESSION(share);

	offset = uio_offset(uio);
    SMB_LOG_DIR_CACHE_LOCK(dnp, "offset %lld d_rdir_offset %lld for <%s>\n",
                           offset, dnp->d_rdir_offset, dnp->n_name);

	/*
     * SMB servers will return the dot and dotdot in most cases. If the share is a
     * FAT Filesystem then the information return could be bogus, also if its a
     * FAT drive then they won't even return the dot or the dotdot. Since we already
     * know everything about dot and dotdot just fill them in here and then skip
     * them during the lookup.
     */
    if (offset == 0) {
        dnp->d_rdir_offset = 0;
        
        for (i = 0; i < 2; i++) {
            if (i == 0) {
                node_ino = dnp->n_ino;
            }
            else {
                par_vp = smbfs_smb_get_parent(dnp, kShareLock);
                if (par_vp != NULL) {
                    node_ino = VTOSMB(par_vp)->n_ino;
                    vnode_put(par_vp);
                }
                else {
                    if (dnp->n_parent_vid == 0) {
                        /* Must be the root of share */
                        node_ino = SMBFS_ROOT_INO;
                    }
                    else {
                        /* Parent got recycled already? */
                        SMBWARNING_LOCK(dnp, "Missing parent for <%s> \n",
                                        dnp->n_name);
                        error = ENOENT;
                        goto done;
                    }
                }
            }
            
            delen = smbfs_fill_direntry(&de, "..", i + 1, DT_DIR,
                                        node_ino, flags);
            /*
             * At this point we always expect them to have enough room for dot
             * and dotdot. If not enough room then uiomove will return an error.
             */
            error = uiomove((void *)&de, delen, uio);
            if (error) {
                goto done;
            }
            
            (*numdirent)++;
            offset++;
            dnp->d_rdir_offset++;
        }
    }

	/*
     * Do we need to open or restart the enumeration?
     */
    if (!(dnp->d_rdir_fctx) ||
        (dnp->d_rdir_fctx->f_share != share) ||
        (offset == 2) ||
        (offset != dnp->d_rdir_offset)) {
        SMB_LOG_DIR_CACHE_LOCK(dnp, "Restart enum offset %lld d_rdir_offset %lld for <%s> \n",
                               offset, dnp->d_rdir_offset, dnp->n_name);
        
        smbfs_closedirlookup(dnp, 1, "readvdir dir restart", context); /* sets dnp->d_rdir_offset to 0 */
        
        /* "." and ".." were added in earlier in this code */
        if (offset == 2) {
            dnp->d_rdir_offset = 2;
        }
        
        error = smbfs_smb_findopen(share, dnp, "*", 1, &dnp->d_rdir_fctx, TRUE,
                                   1, context);

        /* If starting over, then free any saved entry from previous enum */
        if (dnp->d_nextEntry) {
            SMB_FREE(dnp->d_nextEntry, M_TEMP);
            dnp->d_nextEntry = NULL;
            dnp->d_nextEntryLen = 0;
        }
   }
    
	if (error) {
		goto done;
	}
    
	ctx = dnp->d_rdir_fctx;
    
    /*
     * They are continuing from some point ahead of us in the buffer. Skip all
     * entries until we reach their point in the buffer.
     */
    if (offset < 2) {
        /* This should never happen */
        SMBERROR_LOCK(dnp, "offset less than two for <%s>??? \n", dnp->n_name);
        error = EINVAL;
        goto done;
    }

    while (dnp->d_rdir_offset < (offset - 2)) {
        error = smbfs_findnext(ctx, context);
        if (error) {
            smbfs_closedirlookup(dnp, 1, "readvdir findnext error", context);
            goto done;
        }

        dnp->d_rdir_offset++;
    }

    /* 
     * We have an entry left over from before we need to put it into the users
     * buffer before doing any more searches.
     */
    if (dnp->d_nextEntry) {
        error = smb_add_next_entry(dnp, uio, flags);
        if (error)
            goto done;
        (*numdirent)++;
        offset++;
        dnp->d_rdir_offset++;
    }
    
    /*
     * Loop until we end the search or we don't have enough room for the
     * max element 
     */
    first = 0;
    bzero(last_entry_namep, PATH_MAX);

    while (uio_resid(uio)) {
        error = smbfs_findnext(ctx, context);
        if (error) {
            if (error == ENOENT) {
                /* Done enumerating dir */

                /*
                 * If you return any entries, then that means the
                 * enumeration is not complete. Only close the dir when
                 * we have not returned any entries. This prevents us
                 * from re enumerating the entire dir because d_rdir_fctx is
                 * null just to find out we finished the enumeration on the
                 * previous call
                 */
                if (offset == uio_offset(uio)) {
                    smbfs_closedirlookup(dnp, 1, "readvdir EOF", context);
                }
            }
            break;
        }
        
        name = ctx->f_LocalName;
        name_len = ctx->f_LocalNameLen;
        fap = &ctx->f_attr;
        
        /*
         * <14430881> If file IDs are supported by this server, skip any
         * child that has the same id as the current parent that we are
         * enumerating. Seems like snapshot dirs have the same id as the parent
         * and that will cause us to deadlock when we find the vnode with same
         * id and then try to lock it again (deadlock on parent id).
         */
        if (sessionp->session_misc_flags & SMBV_HAS_FILEIDS) {
            if (fap->fa_ino == dnp->n_ino) {
                SMBDEBUG("Skipping <%s> as it has same ID as parent\n", name);
                continue;
            }
        }

		/* Return this entry */
        error = smbfs_add_dir_entry(dvp, uio, flags, name, name_len, fap);
        switch (error) {
            case 0:
                /* Item added successfully */
                if (first == 0) {
                    SMB_LOG_DIR_CACHE2_LOCK(dnp, "First return <%s> in <%s> \n",
                                            name, dnp->n_name);
                    first = 1;
                }
                (*numdirent)++;
                offset++;
                dnp->d_rdir_offset++;
                
                /* Save last entry added for debugging */
                strncpy(last_entry_namep, name, PATH_MAX);
                break;
                
            case ENOBUFS:
                /* uio is full and no more entries will fit */
                if (strnlen(last_entry_namep, PATH_MAX)) {
                    SMB_LOG_DIR_CACHE2_LOCK(dnp, "Last return <%s> in <%s>. uio is full \n",
                                            last_entry_namep, dnp->n_name);
                }
                error = 0;
                goto done;
                
            case ENAMETOOLONG:
                /* Name did not fit, just skip it */
                error = 0;
                break;
                
            default:
                SMBERROR_LOCK(dnp, "smbfs_add_dir_entry failed %d for <%s> in <%s> \n",
                              error, name, dnp->n_name);
                goto done;
        }
    }

    if (strnlen(last_entry_namep, PATH_MAX)) {
        SMB_LOG_DIR_CACHE2_LOCK(dnp, "Last return <%s> in <%s> \n",
                                last_entry_namep, dnp->n_name);
    }

done:
    SMB_LOG_DIR_CACHE_LOCK(dnp, "actual count %d <%s> \n",
                           *numdirent, dnp->n_name);

    /*
	 * We use the uio offset to store the last directory index count. Since
	 * the uio offset is really never used, we can set it without causing any 
	 * issues. Got this idea from the NFS code and it makes things a 
	 * lot simplier. 
	 */
    uio_setoffset(uio, offset);

    if (share != NULL) {
        smb_share_rele(share, context);
    }
	
    if (last_entry_namep != NULL) {
        SMB_FREE(last_entry_namep, M_SMBTEMP);
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
smbfs_zero_fill(struct smb_share *share, SMBFID fid, u_quad_t from,
                u_quad_t to, int ioflag, vfs_context_t context)
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
		error = smb_smb_write(share, fid, uio, ioflag, context);
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
smbfs_0extend(struct smb_share *share, SMBFID fid, u_quad_t from,
              u_quad_t to, int ioflag, vfs_context_t context)
{
	int error;

	/*
	 * Make an exception here for UNIX servers. Since UNIX servers always zero 
	 * fill there is no reason to make this call in their case. So if this is a 
	 * UNIX server just return no error.
	 */
	if (UNIX_SERVER(SS_TO_SESSION(share)))
		return(0);
	/* 
	 * We always zero fill the whole amount if the share is FAT based. We always 
	 * zero fill NT4 servers and Windows 2000 servers. For all others just write 
	 * one byte of zero data at the eof of file. This will cause the NTFS windows
	 * servers to zero fill.
	 */
	if ((share->ss_fstype == SMB_FS_FAT) || 
		((SS_TO_SESSION(share)->session_flags & SMBV_NT4)) || 
		((SS_TO_SESSION(share)->session_flags & SMBV_WIN2K_XP))) {
		error = smbfs_zero_fill(share, fid, from, to, ioflag, context);
	} else {
		char onezero = 0;
		int len = 1;
		uio_t uio;
		
		/* Writing one byte of zero before the eof will force NTFS to zero fill. */
		uio = uio_create(1, (to - 1) , UIO_SYSSPACE, UIO_WRITE);
		uio_addiov(uio, CAST_USER_ADDR_T(&onezero), len);
		error = smb_smb_write(share, fid, uio, ioflag, context);
		uio_free(uio);
	}
	return(error);
}

/*
 * The calling routine must hold a reference on the share
 */
int 
smbfs_doread(struct smb_share *share, off_t endOfFile, uio_t uiop, 
             SMBFID fid, vfs_context_t context)
{
#pragma unused(endOfFile)
	int error;

    /*
     * Dont check for reads past EOF or try to pin the request size to the
     * EOF because we can never be sure what the current EOF is on the server
     * right at this exact moment. Just try the read request as is.
     */
	error = smb_smb_read(share, fid, uiop, context);

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
              SMBFID fid, int ioflag, vfs_context_t context)
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
		error = smb_smb_write(share, fid, uiop, ioflag, context);
	}

	return error;
}
