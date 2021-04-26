/*
 * Copyright (c) 2000-2001 Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2019 Apple Inc. All rights reserved.
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
#include <sys/xattr.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/mount.h>
#include <sys/unistd.h>
#include <sys/lockf.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <vfs/vfs_support.h>
#include <sys/namei.h>
#include <libkern/OSAtomic.h>
#include <sys/attr.h>
#include <sys/kauth.h>
#include <sys/syslog.h>
#include <sys/priv.h>

#include <sys/smb_apple.h>
#include <sys/smb_byte_order.h>
#include <sys/mchain.h>
#include <sys/msfscc.h>

#include <netsmb/smb.h>
#include <netsmb/smb_2.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_rq_2.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_conn_2.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb2_mc_support.h>

#include <smbfs/smbfs.h>
#include <smbfs/smbfs_node.h>
#include <smbfs/smbfs_subr.h>
#include <smbfs/smbfs_subr_2.h>
#include <smbfs/smbfs_lockf.h>
#include <netsmb/smb_converter.h>
#include <smbfs/smbfs_security.h>
#include <smbfs/smbfs_attrlist.h>
#include <smbclient/smbclient_internal.h>

#include <sys/buf.h>

/*
 * kAsyncFetchCount value is a balancing act between too high or too low for
 * non macOS servers or those servers that do not support
 * kAAPL_SUPPORTS_READ_DIR_ATTR.
 *
 * When the dir contains more than can be cached, then this is the number
 * of entries to store in the overflow cache before we have to go out and do
 * another enumeration to refill overflow cache. Thus a higher number here
 * results in fewer enumerations and faster performance on very large dirs.
 *
 * If the directory being enumerated is constantly changing, then this ends
 * up being the number of entries that we keep fetching and then throwing away
 * because the dir changed, thus a lower number here results in fewer
 * enumerations and faster performance in this situation.
 */
#define kAsyncFetchCount 500

char smb_symmagic[SMB_SYMMAGICLEN] = {'X', 'S', 'y', 'm', '\n'};

static int smbfs_setattr(struct smb_share *share, vnode_t vp, struct vnode_attr *vap,
                         vfs_context_t context);
static void smbfs_set_create_vap(struct smb_share *share, struct vnode_attr *vap, vnode_t vp, 
								 vfs_context_t context, int set_mode_now);
static int smbfs_vnop_compound_open(struct vnop_compound_open_args *ap);
static int smbfs_vnop_open(struct vnop_open_args *ap);
int smbfs_hifi_set_perms(struct smb_share *share, vnode_t vp, vfs_context_t context);

extern struct smb_reconnect_stats smb_reconn_stats;
extern uint32_t g_max_dir_entries_cached;
int smbfs_is_dataless_access_allowed(vnode_t vp, uint32_t open_check, vfs_context_t context);


int
smbfs_is_dataless_access_allowed(vnode_t vp, uint32_t open_check,
                                 vfs_context_t context)
{
    int error = 0;
    struct smbnode *np = NULL;
    int do_check = 0;

    if (vp == NULL) {
        SMBERROR("vp is null \n");
        return(EINVAL);
    }

    /* No support for dataless dirs at this time */
    if (vnode_isdir(vp)) {
        return(0);
    }
    
    np = VTOSMB(vp);
    if (np == NULL) {
        SMBERROR("np is null \n");
        return(EINVAL);
    }
    
    /*
     * If its a dataless file that is partially or fully offline, then it will
     * show up as a reparse point. Otherwise its either not a dataless file
     * or its a dataless file that has been completely recalled.
     *
     * Windows DeDupe services look very similar in that files are replaced
     * with reparse points and the P bit is set. We do not want to treat
     * deduped files like dataless, so check the specific reparse tag
     *
     * Thus dataless files MUST have the reparse tag of
     * IO_REPARSE_TAG_STORAGE_SYNC to get this special treatment
     */
    if (!(np->n_dosattr & SMB_EFA_REPARSE_POINT) ||
        (np->n_reparse_tag != IO_REPARSE_TAG_STORAGE_SYNC)) {
        return(0);
    }

    /*
     * If M bit is set, then its a newer server and we know for sure that
     * reading the file will recall it. Opening the file is fine.
     */
    if (np->n_dosattr & SMB_EFA_RECALL_ON_DATA_ACCESS) {
        if (open_check == 1) {
            /* Opening the file is allowed */
            return(0);
        }

        /* IO access has to be checked to see if its allowed */
        do_check = 1;
    }
    else {
        /*
         * Check for older server which might have P or O bits set
         * If P or O bits are set, then must be an older server so we assume
         * that just opening the file will recall it.
         */
        if (np->n_dosattr & (SMB_EFA_OFFLINE | SMB_EFA_SPARSE)) {
            do_check = 1;
        }
    }
    
    /*
     * See if the process is allowed to open or do any IO on this file
     */
    if (do_check == 1) {
        error = vfs_context_dataless_materialization_is_prevented(context);
        if (error) {
            SMBWARNING_LOCK(np, "Dataless access denied %d on <%s> \n",
                     error, np->n_name);
        }
    }
    else {
        /* No check required, so access is allowed */
        error = 0;
    }

    return(error);
}


static uint32_t
smbfs_get_req_lease_state(uint32_t access_rights)
{
#pragma unused(access_rights)
	/*
	 * Pre macOS 10.13.x SMB Servers only supported Read/Handle leases.
	 * Since we only ask for leases on O_EXLOCK or O_SHLOCK files, we will
	 * always have deny Write set on the opened file. Thus we do not need to
	 * ask for a Write lease.
	 *
	 * [MS-SMB2] <50> Windows servers only support R, RW, RH, RWH leases
	 * Since we always want a handle lease, that leaves us with just RH.
	 *
	 */
	uint32_t req_lease_state = SMB2_LEASE_READ_CACHING | SMB2_LEASE_HANDLE_CACHING;

#if 0
	/* [MS-SMB2] <116> Code to determine lease based on file access */
	uint32_t req_lease_state = SMB2_LEASE_HANDLE_CACHING;
	
	if (access_rights & SMB2_FILE_READ_DATA) {
		req_lease_state |= SMB2_LEASE_READ_CACHING;
	}

	if (access_rights & SMB2_FILE_WRITE_DATA) {
		req_lease_state |= SMB2_LEASE_WRITE_CACHING;
	}
#endif
	
	return(req_lease_state);
}


/*
 * We were doing an IO and received an error. Was the error caused because we were
 * reconnecting to the server. If yes then see if we can reopen the file. If everything
 * is ok and the file was reopened then get the fid we need for doing the IO.
 *
 * The calling routine must hold a reference on the share
 *
 */
static int 
smbfs_io_reopen(struct smb_share *share, vnode_t vp, uio_t uio, 
				uint16_t accessMode, SMBFID *fid, int error, 
				vfs_context_t context)
{
	struct smbnode *np = VTOSMB(vp);
	
	if (!(np->f_openState & kNeedReopen))
		return(error);
	
	error = smbfs_smb_reopen_file(share, np, context);
	if (error)
		return(error);
	
	if (FindFileRef(vp, vfs_context_proc(context), accessMode, kCheckDenyOrLocks, 
                    uio_offset(uio), uio_resid(uio), NULL, fid)) {
        *fid = np->f_fid;
    }

    /* Should always have a fid at this point */
    DBG_ASSERT(*fid != 0);	
	return(0);
}

/*
 * smbfs_update_cache
 *
 * General routine that will update the meta data cache for the vnode. If a vap
 * is passed in it will get filled in with the correct information, otherwise it
 * will be ignored.
 *
 * The calling routine must hold a reference on the share
 *
 */
int 
smbfs_update_cache(struct smb_share *share, vnode_t vp, 
                   struct vnode_attr *vap, vfs_context_t context)
 {
	 /* If we are in reconnect mode, then use cached data for now. */
	 int use_cached_data = 0;
	 struct smbfattr fattr;
	 int error = 0;
 
     /* If we are in reconnect, use cached data if we have it */
     if (VTOSMB(vp)->attribute_cache_timer != 0) {
         use_cached_data = (share->ss_flags & SMBS_RECONNECTING);
     }

     /*
      * <44386255> if its a file and its set for non cacheable, then follow
      * AFP Client behavior and also do not cache meta data.
      *
      * Developers will always need a way to get the latest info when they want
      * too.
      */
     if (!(share->ss_flags & SMBS_RECONNECTING) &&
         (vnode_isreg(vp)) && (!smbfsIsCacheable(vp))) {
         VTOSMB(vp)->attribute_cache_timer = 0;
     }

     error = smbfs_attr_cachelookup(share, vp, vap, context, use_cached_data);

     SMB_LOG_KTRACE(SMB_DBG_SMBFS_UPDATE_CACHE | DBG_FUNC_START,
                    error, 0, 0, 0, 0);

	 if (error != ENOENT) {
		 goto done;
     }

	 error = smbfs_lookup(share, VTOSMB(vp), NULL, NULL, &fattr, context);
     SMB_LOG_KTRACE(SMB_DBG_SMBFS_UPDATE_CACHE | DBG_FUNC_NONE,
                    0xabc001, error, 0, 0, 0);

     if ((error == ETIMEDOUT) && (VTOSMB(vp)->attribute_cache_timer != 0)) {
         /* Just return the cached data */
         use_cached_data = TRUE;
         goto use_cache;
     }

	 if (error) {
		 goto done;
     }
     
     /*
	  * At this point we have the data from the server, so we can
	  * just use cached data for now. See <rdar://problem/13813721>.
	  */
     use_cached_data = TRUE;
     
     /* Enter in the latest data from the server into the caches */
	 smbfs_attr_cacheenter(share, vp, &fattr, TRUE, context);

use_cache:
     error = smbfs_attr_cachelookup(share, vp, vap, context, use_cached_data);

done:
     SMB_LOG_KTRACE(SMB_DBG_SMBFS_UPDATE_CACHE | DBG_FUNC_END, error, 0, 0, 0, 0);
	 return (error);
 }
 
/*
 * smbfs_close - The internal open routine, the vnode should be locked
 * before this is called. We only handle VREG in this routine.
 *
 * The calling routine must hold a reference on the share
 *
 */
int 
smbfs_close(struct smb_share *share, vnode_t vp, int openMode, 
			vfs_context_t context)
{
	struct smbnode *np = VTOSMB(vp);
	proc_t p = vfs_context_proc(context);
	struct fileRefEntry	*fndEntry = NULL;
	struct fileRefEntry	*curr = NULL;
	int	error = 0;
	uint16_t accessMode = 0;
    SMBFID fid = 0;
	int32_t	needOpenFile;
	uint16_t openAccessMode;
	uint32_t rights;
    struct smbfattr *fap = NULL;
    vnode_t par_vp = NULL;
    int get_mod_date = 0;
	
	/* 
	 * We have more than one open, so before closing see if the file needs to 
	 * be reopened 
	 */
	if ((np->f_refcnt > 1) && (smbfs_smb_reopen_file(share, np, context) == EIO)) {
		SMBDEBUG_LOCK(np, " %s waiting to be revoked\n", np->n_name);
		np->f_refcnt--;
		return (0);
	}
		
    SMB_LOG_KTRACE(SMB_DBG_SMBFS_CLOSE | DBG_FUNC_START,
                   openMode, np->f_refcnt, 0, 0, 0);

	if (openMode & FREAD)
		accessMode |= kAccessRead;

	if (openMode & FWRITE)
		accessMode |= kAccessWrite;

	/* Check the number of times Open() was called */
	if (np->f_refcnt == 1) {
		/*
		 * This is the last Close() we will get, so close sharedForkRef 
		 * and any other forks created by ByteRangeLocks
		 */
		if (np->f_fid != 0) {
			SMBFID oldFID = np->f_fid;
						
			/* Close the shared file first. Clear out the refs to it 
			 * first so that no one else trys to use it while I'm waiting 
			 * for the close file reply to arrive.  There was a case 
			 * where cluster i/o was firing off after I sent the close 
			 * file req, but had not gotten the close file reply yet 
			 * and tyring to still use the shared file 
			 */
			np->f_fid = 0;		/* clear the ref num */
			np->f_accessMode = 0;
			np->f_rights = 0;
			np->f_openRWCnt = 0;
			np->f_openRCnt = 0;
			np->f_openWCnt = 0;
			np->f_openTotalWCnt = 0;
			np->f_needClose = 0;
			np->f_clusterCloseError = 0;
			/*
			 * They didn't unlock the file before closing. A SMB close will remove
			 * any locks so lets free the memory associated with that lock.
			 */
			if (np->f_smbflock) {
				SMB_FREE(np->f_smbflock, M_LOCKF);
				np->f_smbflock = NULL;				
			}
			error = smbfs_smb_close(share, oldFID, context);
			if (error) {
				SMBWARNING("close file failed %d on fid %llx\n", 
                           error, oldFID);
            }
		 }

		/* Remove forks that were opened due to ByteRangeLocks or DenyModes */
		lck_mtx_lock(&np->f_openDenyListLock);
		curr = np->f_openDenyList;
		while (curr != NULL) {
			/* If we have a handle lease, skip the close */
			lck_mtx_lock(&curr->dur_handlep->lock);
			
			if ((curr->dur_handlep->flags & SMB2_LEASE_GRANTED) &&
				(curr->dur_handlep->lease_state & SMB2_LEASE_HANDLE_CACHING) &&
				(curr->dur_handlep->req_lease_state == curr->dur_handlep->lease_state) &&
				(OSAddAtomic(0, &share->ss_curr_def_close_cnt) < share->ss_max_def_close_cnt)) {
				/* Mark it as a deferred close */
				curr->dur_handlep->flags |= SMB2_DEFERRED_CLOSE;
				OSAddAtomic(1, &share->ss_curr_def_close_cnt);
				
				/* Update cumulative count */
				OSAddAtomic64(1, &share->ss_total_def_close_cnt);

				/* Set deferred close time */
				nanouptime(&curr->dur_handlep->def_close_time);

				lck_mtx_unlock(&curr->dur_handlep->lock);
				
				SMB_LOG_UNIT_TEST_LOCK(np, "FileLeaseUnitTest - Deferring last close on <%s> \n",
									   np->n_name);
				
				lck_mtx_unlock(&np->f_openDenyListLock);
				
				np->f_refcnt = 0;
				error = 0;

                /* Get the last close mod date and data size */
                get_mod_date = 1;

				goto exit;
			}
			else {
				lck_mtx_unlock(&curr->dur_handlep->lock);
			}

			error = smbfs_smb_close(share, curr->fid, context);
			if (error) {
				SMBWARNING("close file failed %d on fid %llx\n", 
                           error, curr->fid);
            }
			curr = curr->next;
		}
		lck_mtx_unlock(&np->f_openDenyListLock);
		np->f_refcnt = 0;
		RemoveFileRef (vp, NULL, 0);		/* remove all file refs */

		/*
		 * We did the last close on the file. This file is 
		 * marked for deletion on close. So lets delete it
		 * here. If we get an error then try again when the node
		 * becomes inactive.
		 */
        if (np->n_flag & NDELETEONCLOSE) {
            if (smbfs_smb_delete(share, np, VREG,
                                 NULL, 0,
                                 0, context) == 0) {
                np->n_flag &= ~NDELETEONCLOSE;

                /* Assume the file is now deleted on the server */
                smb_vhashrem(np);
                
                par_vp = smbfs_smb_get_parent(np, kShareLock);
                if (par_vp != NULL) {
                    smbfs_attr_touchdir(VTOSMB(par_vp), (share->ss_fstype == SMB_FS_FAT));

					/* <33469405> if dir has active lease skip local change notify */
					lck_mtx_lock(&VTOSMB(par_vp)->d_dur_handle.lock);
					if (!(VTOSMB(par_vp)->d_dur_handle.flags & SMB2_LEASE_GRANTED) ||
						(VTOSMB(par_vp)->d_dur_handle.flags & SMB2_LEASE_BROKEN)) {
						VTOSMB(par_vp)->d_changecnt++;
					}
					lck_mtx_unlock(&VTOSMB(par_vp)->d_dur_handle.lock);
					
                    /* Remove any negative cache entries. */
                    if (VTOSMB(par_vp)->n_flag & NNEGNCENTRIES) {
                        VTOSMB(par_vp)->n_flag &= ~NNEGNCENTRIES;
                        cache_purge_negatives(VTOSMB(par_vp)->n_vnode);
                    }
                    
                    vnode_put(par_vp);
                }
                else {
                    if (np->n_parent_vid != 0) {
                        /* Parent got recycled already? Ok to ignore */
                        SMBWARNING_LOCK(np, "Missing parent for <%s> \n",
                                        np->n_name);
                    }
                }
            }
        }

        /*
         * It was not cacheable before, but now that all files are closed,
         * make it cacheable again (if its a valid cacheable file). If
         * we were caching should we remove attributes cache. Realy only
         * matters when we turn on cluster io?
         */
        if (vnode_isnocache(vp))
            vnode_clearnocache(vp);

        /* Did we change the file during the open (IE wrote to the file) */
        if (np->n_flag & NATTRCHANGED) {
            np->attribute_cache_timer = 0;
            smb_dir_cache_invalidate(vp, 0);
        }

        lck_mtx_lock(&np->f_openStateLock);
        if (np->f_openState & kNeedRevoke) {
			error = 0;
            
            /*
             * If we are going to clear out a pending revoke, then
             * clear the NNEEDS_FLUSH | NNEEDS_EOF_SET flags. The file
             * is no longer open so we can not do those operations.
             */
            np->n_flag &= ~(NNEEDS_FLUSH | NNEEDS_EOF_SET);
        }
        
		/*
		 * Clear all the f_openState flags, we are closed. If we have a pending
		 * revoke clear that out, the file is closed no need to revoke it now.
		 */
		np->f_openState = 0;
		lck_mtx_unlock(&np->f_openStateLock);

        /* Get the last close mod date and data size */
        get_mod_date = 1;

        goto exit;
	}

	/* More than one open */
    
	/* 
	 * See if we can match this Close() to a matching file that has byte range 
	 * locks or denyModes.
	 *
	 * NOTE: FHASLOCK can be set by open with O_EXCLUSIVE or O_SHARED which 
	 *	 maps to my deny modes or FHASLOCK could also have been set/cleared 
	 *	by calling flock directly.
	 *
	 * Cases that work are:
	 *	1)  Carbon using deny modes and thus FHASLOCK maps to my deny modes. 
	 *	    No flock being used.
	 *	2)  Cocoa using open with O_EXCLUSIVE or O_SHARED and not calling flock.
	 *	3)  Cocoa using open, then calling flock and later calling flock 
	 *	    to unlock before close.
	 *	4)  Cocoa open, then calling flock, but not calling flock to unlock 
	 *	    before close.  I would fall through to the shared fork code correctly.
	 *
	 * Cases that may not work are:
	 *	1)  Carbon using deny modes and then calling flock to unlock, thus 
	 *	    clearing FHASLOCK flag.I would assume it was the shared file.
	 *	2)  Cocoa open with O_EXCLUSIVE or O_SHARED and then calling flock 
	 *	    to lock and then unlock, then calling close.
	 *	3)  ??? 
	 */
	if (openMode & FHASLOCK) {
		uint16_t tempAccessMode = accessMode;

		/* Try with denyWrite, if not found, then try with denyRead/denyWrite */
		tempAccessMode |= kDenyWrite;
		error = FindFileRef(vp, p, tempAccessMode, kExactMatch, 0, 0, &fndEntry, 
							&fid);
		if (error != 0) {
			tempAccessMode |= kDenyRead;
			error = FindFileRef(vp, p, tempAccessMode, kExactMatch, 0, 0, 
								&fndEntry, &fid);
		}
		if (error == 0)
			accessMode = tempAccessMode;

        SMB_LOG_KTRACE(SMB_DBG_SMBFS_CLOSE | DBG_FUNC_NONE,
                       0xabc001, error, 0, 0, 0);
    } else {
		/* No deny modes used, so look for any forks opened for byteRangeLocks */
		error = FindFileRef(vp, p, accessMode, kExactMatch, 0, 0, &fndEntry, &fid);
        SMB_LOG_KTRACE(SMB_DBG_SMBFS_CLOSE | DBG_FUNC_NONE,
                       0xabc002, error, 0, 0, 0);
	}

	/* always decrement the count, dont care if got an error or not */
    if (np->f_refcnt > 0) {
        np->f_refcnt--;
    }
	
	if ((error == 0) && fndEntry) {
		if (fndEntry->refcnt > 0) {
			/*
			 * We have an Open Deny entry that is being used by more than 
			 * one open call just decrement it and get out.
			 */
			fndEntry->refcnt--;
			SMB_LOG_KTRACE(SMB_DBG_SMBFS_CLOSE | DBG_FUNC_NONE,
						   0xabc003, error, 0, 0, 0);
		}
		else {
			/* If we have a handle lease, skip the close */
			lck_mtx_lock(&fndEntry->dur_handlep->lock);

			if ((fndEntry->dur_handlep->flags & SMB2_LEASE_GRANTED) &&
				(fndEntry->dur_handlep->lease_state & SMB2_LEASE_HANDLE_CACHING) &&
				(fndEntry->dur_handlep->req_lease_state == fndEntry->dur_handlep->lease_state) &&
				(OSAddAtomic(0, &share->ss_curr_def_close_cnt) < share->ss_max_def_close_cnt)) {
				/* Mark it as a deferred close */
				fndEntry->dur_handlep->flags |= SMB2_DEFERRED_CLOSE;
				OSAddAtomic(1, &share->ss_curr_def_close_cnt);

				/* Set deferred close time */
				nanouptime(&fndEntry->dur_handlep->def_close_time);

				lck_mtx_unlock(&fndEntry->dur_handlep->lock);
				
				error = 0;
				SMB_LOG_UNIT_TEST_LOCK(np, "FileLeaseUnitTest - Deferring close on <%s> \n",
									   np->n_name);
			}
			else {
				lck_mtx_unlock(&fndEntry->dur_handlep->lock);
				
				/* 
				 * We have an Open Deny entry that is all done. Time to close 
				 * it. 
				 */
				error = smbfs_smb_close(share, fndEntry->fid, context);
				/* We are not going to get another close, so always remove it from the list */
				RemoveFileRef(vp, fndEntry, 0);
				SMB_LOG_KTRACE(SMB_DBG_SMBFS_CLOSE | DBG_FUNC_NONE,
							   0xabc004, error, 0, 0, 0);
			}
		}
		
		goto exit;
	}
	
	/*
     * Not an open deny mode open.
     *
     * An oddity. If first open was read/write/denyWrite and then a second
     * open of just read, then the second open will open the shared fork.
     * Now, get a close on the "read" fork, the np->f_refcnt will be 2, and we
     * will end up here. The np->f_refcnt will be decremented to 1 so the next
     * close call will close everything. In the meantime, we end up leaving 
     * the shared fork open with "read" and thus we actually have two forks
     * open, one for the read/write/denyWrite and the other is the shared fork.
     * We have has this behavior for a long time and just have not noticed.
     * Since its an edge case and no one has complained, we can leave it this
     * way for now.
     */
	needOpenFile = 0;
	openAccessMode = 0;
	fid = 0;
	rights = SMB2_READ_CONTROL;
  
	/* 
	 * Just return 0 for no err, but dont close the file since another 
	 * process is still using it 
	 */
	error = 0;
    
	/* Check to downgrade access mode if needed */
	switch (accessMode) {
	case (kAccessRead | kAccessWrite):
		np->f_openRWCnt -= 1;
		if ((np->f_openRWCnt == 0) && (np->f_openRCnt > 0) && (np->f_openWCnt == 0)) {
			/* drop from rw to read only */
			needOpenFile = 1;
			openAccessMode = kAccessRead;
			rights |= SMB2_FILE_READ_DATA;
		}
		/* Dont ever downgrade to write only since Unix expects read/write */
		break;
	case kAccessRead:
		np->f_openRCnt -= 1;
		/* Dont ever downgrade to write only since Unix expects read/write */
		break;
	case kAccessWrite:
		np->f_openWCnt -= 1;
		if ( (np->f_openRCnt > 0) && (np->f_openRWCnt == 0) && 
			(np->f_openWCnt == 0) ) {
			/* drop from rw to read only */
			needOpenFile = 1;
			openAccessMode = kAccessRead;
			rights |= SMB2_FILE_READ_DATA;
		}
		break;
	}
	/* set up for the open fork */           
	if (needOpenFile == 1) {
        SMB_MALLOC(fap,
                   struct smbfattr *,
                   sizeof(struct smbfattr),
                   M_SMBTEMP,
                   M_WAITOK | M_ZERO);
        if (fap == NULL) {
            SMBERROR("SMB_MALLOC failed\n");
            error = ENOMEM;
            goto exit;
        }
        
        error = smbfs_smb_open_file(share, np,
                                    rights, NTCREATEX_SHARE_ACCESS_ALL, &fid,
                                    NULL, 0, FALSE,
                                    fap, context);
        SMB_LOG_KTRACE(SMB_DBG_SMBFS_CLOSE | DBG_FUNC_NONE,
                       0xabc005, error, 0, 0, 0);
		if (error == 0) {
			SMBFID oldFID = np->f_fid;
			
			/* We are downgrading the open flush it out any data */
			ubc_msync(vp, 0, ubc_getsize(vp), NULL, UBC_PUSHDIRTY | UBC_SYNC | UBC_INVALIDATE);
			/* Close the shared file first and use this new one now 
			 * Switch the ref before closing the old shared file so the 
			 * old file wont get used while its being closed. 
			 */
			np->f_fid = fid;	/* reset the ref num */
			np->f_accessMode = openAccessMode;
			np->f_rights = rights;
			error = smbfs_smb_close(share, oldFID, context);
			if (error) {
				SMBWARNING("close file failed %d on fid %llx\n", error, oldFID);
            }
            SMB_LOG_KTRACE(SMB_DBG_SMBFS_CLOSE | DBG_FUNC_NONE,
                           0xabc006, error, 0, 0, 0);
		}
	}

    SMB_LOG_KTRACE(SMB_DBG_SMBFS_CLOSE | DBG_FUNC_NONE,
                   0xabc007,
                   np->f_openRCnt,
                   np->f_openWCnt,
                   np->f_openRWCnt,
                   0);

exit:
    if (smbfsIsCacheable(vp) && (get_mod_date == 1)) {
        /*
         * Since we just closed the file, the mod time probably changed.  Hit
         * the server to get the latest time stamp and data size. On the next
         * open, we will check the mod date and data size and if either changed
         * then invalidate the UBC. Last close to next open logic from NFS
         * client.
         */
        np->attribute_cache_timer = 0;
        (void) smbfs_update_cache(share, vp, NULL, context);

        np->n_last_close_mtime = np->n_mtime;
        np->n_last_close_size = np->n_size;
    }

    if ((error == 0) && (openMode & FWRITE) && (np->f_openTotalWCnt > 0)) {
        np->f_openTotalWCnt--;
    }

    if (fap != NULL) {
        SMB_FREE(fap, M_SMBTEMP);
    }

    SMB_LOG_KTRACE(SMB_DBG_SMBFS_CLOSE | DBG_FUNC_END, error, 0, 0, 0, 0);

	return (error);
}

/*
 * smbfs_vnop_close - smbfs vnodeop entry point
 *	vnode_t a_vp;
 *	int a_fflags;
 *	vfs_context_t a_context;
 */
static int 
smbfs_vnop_close(struct vnop_close_args *ap)
{
	vnode_t vp = ap->a_vp;
	int error = 0;
	struct smbnode *np;

    if (smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK) != 0)
        return (0);
    
    SMB_LOG_KTRACE(SMB_DBG_CLOSE | DBG_FUNC_START, 0, 0, 0, 0, 0);

	np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_close;
	
	if (vnode_isdir(vp)) {
		if (--np->d_refcnt) {
			error = 0;
		}
        else {
			lck_mtx_lock(&np->d_dur_handle.lock);

			if (!(np->d_dur_handle.flags & SMB2_LEASE_GRANTED)) {
				lck_mtx_unlock(&np->d_dur_handle.lock);

				/* We have a dir lease, defer last close until vnop_inactive */
				smbfs_closedirlookup(np, 0, "vnop_close", ap->a_context);
			}
			else {
				lck_mtx_unlock(&np->d_dur_handle.lock);
			}

            /* Close readdir dir which never has a dir lease */
            smbfs_closedirlookup(np, 1, "vnop_close", ap->a_context);
		}
	} else if ( vnode_isreg(vp) || vnode_islnk(vp) ) {
		int clusterCloseError = np->f_clusterCloseError;
		struct smb_share *share;
		
		/* if its readonly volume, then no sense in trying to write out dirty data */
		if (!vnode_vfsisrdonly(vp) && smbfsIsCacheable(vp)) {
			if (np->n_flag & NISMAPPED) {
				/* More expensive, but handles mmapped files */
				ubc_msync (vp, 0, ubc_getsize(vp), NULL, UBC_PUSHDIRTY | UBC_SYNC);
			} else {
				/* Less expensive, but does not handle mmapped files */
				cluster_push(vp, IO_CLOSE);
			}
		}		
		share = smb_get_share_with_reference(VTOSMBFS(vp));
        
        /* Do any pending set eof or flushes before closing the file */
        smbfs_smb_fsync(share, np, 0, ap->a_context);
        
		error = smbfs_close(share, vp, ap->a_fflag, ap->a_context);
		smb_share_rele(share, ap->a_context);
		if (!error)
			error = clusterCloseError;
	}
	smbnode_unlock(np);

    SMB_LOG_KTRACE(SMB_DBG_CLOSE | DBG_FUNC_END, error, 0, 0, 0, 0);
    return (error);
}

static void 
smbfs_get_rights_shareMode(int fmode, uint32_t *rights, uint32_t *shareMode, uint16_t *accessMode)
{
	/*
	 * We always ask for READ_CONTROL so we can always get the owner/group 
	 * IDs to satisfy a stat.
	 */
	*rights = SMB2_READ_CONTROL;
	if (fmode & FREAD) {
		*accessMode |= kAccessRead;
		*rights |= SMB2_FILE_READ_DATA;
	}
	if (fmode & FWRITE) {
		*accessMode |= kAccessWrite;
		*rights |= SMB2_FILE_APPEND_DATA | SMB2_FILE_WRITE_DATA;
	}
	
	/*
	 * O_EXLOCK -> denyRead/denyWrite is always cacheable since we have exclusive 
	 *			   access.
	 * O_SHLOCK -> denyWrite is always cacheable since we are the only one who 
	 *			   can change the file.
	 * denyNone -> is not cacheable if from Carbon (a FSCTL call from Carbon 
	 *			   will set the vnode to be non cacheable). It is always 
	 *			   cacheable from Unix since that is what home dirs mainly use.
	 */
	*shareMode = NTCREATEX_SHARE_ACCESS_ALL;
	if (fmode & O_SHLOCK) {
		*accessMode |= kDenyWrite;
		/* Remove the wr shared access */
		*shareMode &= ~NTCREATEX_SHARE_ACCESS_WRITE;
	}
	
	if (fmode & O_EXLOCK) {
		*accessMode |= kDenyWrite;
		*accessMode |= kDenyRead;
		/* Remove the Read/Write/Delete shared access */
		*shareMode &= ~NTCREATEX_SHARE_ACCESS_ALL;
	}
}

static void 
smbfs_update_RW_cnts(vnode_t vp, uint16_t accessMode)
{
	/* count nbr of opens with rw, r, w so can downgrade access in close if needed */
	struct smbnode *np = VTOSMB(vp);
	
	switch (accessMode) {
		case (kAccessWrite | kAccessRead):
			np->f_openRWCnt += 1;
			break;
		case kAccessRead:
			np->f_openRCnt += 1;
			break;
		case kAccessWrite:
			np->f_openWCnt += 1;
			break;
	}
}

static int 
smbfs_create_open(struct smb_share *share, vnode_t dvp, struct componentname *cnp,
				  struct vnode_attr *vap,  uint32_t open_disp, int fmode, 
				  SMBFID *fidp, struct smbfattr *fattrp, vnode_t *vpp,
				  vfs_context_t context)
{
	struct smbnode *dnp = VTOSMB(dvp);
	struct smbmount *smp = VTOSMBFS(dvp);
	vnode_t vp;
	const char *name = cnp->cn_nameptr;
	size_t nmlen = cnp->cn_namelen;
	int error = 0;
	uint32_t rights, addedReadRights, saved_rights;
	uint16_t accessMode = 0;
	struct smbnode *np;
	uint32_t shareMode = 0;
	char *target = NULL;
	size_t targetlen = 0;
    struct smb2_durable_handle *dur_handlep = NULL;
    int created_file = 0;

	/*
	 * We have NO vnode for the target!
	 * The target may or may not exist on the server.
	 * The target could be a dir or a file. 
	 * 
	 * If its a dir, then it can not be a creation. Use vnop_mkdir instead.
	 * It could be an open on a dir which SMB protocol allows.  Make sure
	 * to close the dir afterwards.
	 * 
	 * If its a file, it cant be a symlink, use vnop_link instead.
	 * It could be a create and open on a file.
	 * Since a vnode was not found for the file, we know its not already open
	 *	by this client. This means I know its the FIRST open call.
	 * 
	 * dnp should be locked when this function is called
	 */
	
	*fidp = 0;
	
	smbfs_get_rights_shareMode(fmode, &rights, &shareMode, &accessMode);
	
	/* 
	 * If opening with write only, try opening it with read/write. Unix 
	 * expects read/write access like open/map/close/PageIn. This also helps 
	 * the cluster code since if write only, the reads will fail in the 
	 * cluster code since it trys to page align the requests.  
	 *
	 * NOTE: While we may open it RW, the shared RW_cnts will count it as
	 * WriteOnly and the FileRef will save it as WriteOnly.
	 * np->f_accessMode will show the true RW access.
	 *
	 * NOTE: Since this call only creates items that are regular files we always
	 * set the vtype to VREG. The smbfs_smb_ntcreatex only uses that on create.
	 */	
	addedReadRights = (accessMode == kAccessWrite) ? SMB2_FILE_READ_DATA : 0;
    saved_rights = rights | addedReadRights;
    
    error = smbfs_smb_ntcreatex(share, dnp,
                                rights | addedReadRights, shareMode, VREG,
                                fidp, name, nmlen,
                                open_disp, 0, fattrp,
                                TRUE, NULL, context);
	
	if (error && addedReadRights) {
		/* Failed to add read rights, so fall back to write only */
		addedReadRights = 0;
        saved_rights = rights;

		error = smbfs_smb_ntcreatex(share, dnp,
                                    rights, shareMode, VREG,
                                    fidp, name, nmlen,
                                    open_disp, 0, fattrp,
                                    TRUE, NULL, context);
	}
    
 	if (error) {
		return error;
	}

    /* Did we actually create a file on the server? */
    if (fattrp->fa_created_disp == FILE_CREATE) {
        created_file = 1;
    }

    /* Reparse point need to get the reparse tag */
	if (fattrp->fa_attr & SMB_EFA_REPARSE_POINT) {
		error = smbfs_smb_get_reparse_tag(share, *fidp, &fattrp->fa_reparse_tag, 
										  &target, &targetlen, context);
		if (error) {
			(void) smbfs_smb_close(share, *fidp, context);
			return error;
		}
	}
	
	/* Create the vnode using fattr info and return the created vnode */
	error = smbfs_nget(share, vnode_mount(dvp),
                       dvp, name, nmlen,
                       fattrp, &vp,
                       cnp->cn_flags, SMBFS_NGET_CREATE_VNODE,
                       context);
	if (error) {
        if (target) {
            SMB_FREE(target, M_TEMP);
        }
		(void) smbfs_smb_close(share, *fidp, context);
		return error;
	}
	
	/* 
	 * If they passed in a vnode then we no longer need it, so remove the 
	 * reference. The smbfs_nget always returns us a vnode with a reference. The 
	 * vnode could be the same or a new one. 
	 */
	if (*vpp) {
		vnode_put(*vpp);
	}
	*vpp = vp;	
	np = VTOSMB(vp);

	/*
	 * We treat directories, files and symlinks different. Since currently we
	 * only handle reparse points as dfs triggers or symlinks we can ignore
	 * treating reparse points special.
	 */
	if (vnode_isdir(vp)) {
		/* We opened a directory, increment its ref count and close it */
		np->d_refcnt++;
		(void)smbfs_smb_close(share, *fidp, context);
	} else if (vnode_islnk(vp)) {
		/* We opened a symlink, close it */
		(void)smbfs_smb_close(share, *fidp, context);
		if (target && targetlen) {
			smbfs_update_symlink_cache(np, target, targetlen);
		}
	} else if (vnode_isreg(vp)) {
		/* We opened the file so bump ref count */
		np->f_refcnt++;
		
		np->f_rights = rights;
		np->f_accessMode = accessMode;
		if (addedReadRights) {
			np->f_rights |= SMB2_FILE_READ_DATA;
			np->f_accessMode |= kAccessRead;
		}

		/* if deny modes were used, save the file ref into file list */
		if ((fmode & O_EXLOCK) || (fmode & O_SHLOCK)) {
			SMB_MALLOC(dur_handlep, struct smb2_durable_handle *,
					   sizeof (struct smb2_durable_handle),
					   M_TEMP, M_WAITOK | M_ZERO);
			
            error = smb2_smb_dur_handle_init(share, np,
											 (SMB2_DURABLE_HANDLE_REQUEST | SMB2_NEW_LEASE_KEY), dur_handlep);
			dur_handlep->req_lease_state = smbfs_get_req_lease_state(saved_rights);
			
            if (error == 0) {
                /*
                 * %%% TO DO - can we improve the performance of this?
                 * Want to get a durable handle, but the lease key consists
                 * of the file ID, so have to wait until the first create 
                 * succeeds which gets us the file ID, then close it and reopen 
                 * it but this time requesting the durable handle.
                 */
                (void) smbfs_smb_close(share, *fidp, context);
                
                error = smbfs_smb_ntcreatex(share, dnp,
                                            saved_rights, shareMode, VREG,
                                            fidp, name, nmlen,
                                            FILE_OPEN, 0, fattrp,
                                            FALSE, dur_handlep, context);
            }
            else {
                /* 
                 * Durable handles must not be supported by this server, just 
                 * use the earlier open which must have worked.
                 * Clear the durable handle not supported error
                 */
				error = 0;
            }
            
            if (error == 0) {
                /* 
                 * Either durable handles not supported or reopening the file
                 * with durable handle worked so save file ref
				 *
				 * Always pass in a dur handle pointer to be used
                 */
                AddFileRef (vp, vfs_context_proc(context), accessMode, rights,
                            *fidp, dur_handlep, NULL);
				
				/* Do not free dur_handlep as its now being used by File Ref */
				dur_handlep = NULL;
            }
		}
        else {
            np->f_fid = *fidp;
			smbfs_update_RW_cnts(vp, accessMode);
		}
		
		if ((accessMode == kAccessWrite) &&
			(addedReadRights == 0)) {
			/* if opened with just write, then turn off cluster code */
			vnode_setnocache(vp);
		}
	}
    
	/* If it was allocated then free, since we are done with it now */ 
    if (target) {
        SMB_FREE(target, M_TEMP);
    }
	
	if (created_file == 1) {
		struct timespec ts;
		/* 
		 * We just created the file, so we have no finder info and the resource fork
		 * should be empty. So set our cache timers to reflect this information
		 */
		nanouptime(&ts);
		VTOSMB(vp)->finfo_cache_timer = ts.tv_sec;
		VTOSMB(vp)->rfrk_cache_timer = ts.tv_sec;
		
		/* 
		 * On create, an initial ACL can be set.
		 * If unix extensions are supported, then can set mode, owner, group.
		 */
		if (vap != NULL) {
			smbfs_set_create_vap(share, vap, vp, context, TRUE);  /* Set the REAL create attributes NOW */
		}
		
        /* If HiFi and Super Guest mode, attempt to set the uid/gid */
        smbfs_hifi_set_perms(share, vp, context);

        smbfs_attr_touchdir(dnp, (share->ss_fstype == SMB_FS_FAT));

		/* <33469405> if dir has active lease skip local change notify */
		lck_mtx_lock(&dnp->d_dur_handle.lock);
		if (!(dnp->d_dur_handle.flags & SMB2_LEASE_GRANTED) ||
			(dnp->d_dur_handle.flags & SMB2_LEASE_BROKEN)) {
			dnp->d_changecnt++;
		}
		lck_mtx_unlock(&dnp->d_dur_handle.lock);

		/* Remove any negative cache entries. */
		if (dnp->n_flag & NNEGNCENTRIES) {
			dnp->n_flag &= ~NNEGNCENTRIES;
			cache_purge_negatives(dvp);
		}
		
		/* blow away statfs cache */
		smp->sm_statfstime = 0;
	}
	/* smbfs_nget returns a locked node, unlock it */
	smbnode_unlock(VTOSMB(vp));		/* Release the smbnode lock */
	
	if (dur_handlep != NULL) {
		lck_mtx_lock(&dur_handlep->lock);
		dur_handlep->flags = 0;
		lck_mtx_unlock(&dur_handlep->lock);

		smb2_smb_dur_handle_free(dur_handlep);
		SMB_FREE(dur_handlep, M_TEMP);
	}
	
	return (error);
}

/*
 * smbfs_open -	The internal open routine, the vnode should be locked
 *		before this is called.
 *
 * The calling routine must hold a reference on the share
 *
 */
int 
smbfs_open(struct smb_share *share, vnode_t vp, int mode, 
		   vfs_context_t context)
{
	proc_t p = vfs_context_proc(context);
	struct smbnode *np = VTOSMB(vp);
	uint16_t accessMode = 0;
	uint16_t savedAccessMode = 0;
	uint32_t rights, addedReadRights = 0;
	uint32_t shareMode;
    SMBFID fid = 0;
	int error = 0;
	int	warning = 0;
    struct smbfattr *fap = NULL;
	struct smb2_durable_handle *create_dur_handlep = NULL;
	struct smb2_durable_handle *dur_handlep = NULL;
	int do_create;
    uint32_t disp;
    struct fileRefEntry *fndEntry = NULL;
	int needUpgrade = 0;

    SMB_LOG_KTRACE(SMB_DBG_SMBFS_OPEN | DBG_FUNC_START, 0, 0, 0, 0, 0);

	/* It was already open so see if the file needs to be reopened */
	if ((np->f_refcnt) && 
		((error = smbfs_smb_reopen_file(share, np, context)) != 0)) {
		SMBDEBUG_LOCK(np, " %s waiting to be revoked\n", np->n_name);
		goto exit;
	}
	
    SMB_MALLOC(fap,
               struct smbfattr *,
               sizeof(struct smbfattr),
               M_SMBTEMP,
               M_WAITOK | M_ZERO);
    if (fap == NULL) {
        SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto exit;
    }
    
	smbfs_get_rights_shareMode(mode, &rights, &shareMode, &accessMode);
	savedAccessMode = accessMode;	/* Save the original access requested */

	/*
	 * If opening with write only, try opening it with read/write. Unix
	 * expects read/write access like open/map/close/PageIn. This also helps
	 * the cluster code since if write only, the reads will fail in the
	 * cluster code since it trys to page align the requests.
	 *
	 * NOTE: While we may open it RW, the shared RW_cnts will count it as
	 * WriteOnly and the FileRef will save it as WriteOnly.
	 * np->f_accessMode will show the true RW access.
	 */
	addedReadRights = (accessMode == kAccessWrite) ? SMB2_FILE_READ_DATA : 0;
	
	if ((mode & O_EXLOCK) || (mode & O_SHLOCK)) {
		/* 
		 * if using deny modes and I had to open the file myself, then close 
		 * the file now so it does not interfere with the deny mode open.
		 * We only do this in read.
		 */
		if (np->f_needClose) {
			np->f_needClose = 0;
			warning = smbfs_close(share, vp, FREAD, context);
			if (warning) {
				SMBWARNING_LOCK(np, "error %d closing %s\n", warning, np->n_name);
           }
		}

		/* Using deny modes, see if already in open file list */
		error = FindFileRef(vp, p, accessMode, kExactMatch, 0, 0, &fndEntry, &fid);
		if (error == 0) {
			DBG_ASSERT(fndEntry);

			/* Can we reuse a file ref kept open by a handle lease? */
			lck_mtx_lock(&fndEntry->dur_handlep->lock);
			if ((fndEntry->dur_handlep->flags & SMB2_LEASE_GRANTED) &&
				(fndEntry->dur_handlep->lease_state & SMB2_LEASE_HANDLE_CACHING) &&
				(fndEntry->dur_handlep->flags & SMB2_DEFERRED_CLOSE)) {
				/* 
				 * File ref was no longer in use, mark that its back in use
				 */
				fndEntry->dur_handlep->flags &= ~SMB2_DEFERRED_CLOSE;
				OSAddAtomic(-1, &share->ss_curr_def_close_cnt);

				OSAddAtomic(1, &fndEntry->dur_handlep->handle_reuse_cnt);
				
				/* Clear deferred close time */
				fndEntry->dur_handlep->def_close_time.tv_sec = 0;
				fndEntry->dur_handlep->def_close_time.tv_nsec = 0;

				lck_mtx_unlock(&fndEntry->dur_handlep->lock);
				
				SMB_LOG_UNIT_TEST_LOCK(np, "FileLeaseUnitTest - Reusing <%s> accessMode <0x%x> cnt <%d> \n",
									   np->n_name,
									   accessMode,
									   OSAddAtomic(0, &fndEntry->dur_handlep->handle_reuse_cnt));
			}
			else {
				lck_mtx_unlock(&fndEntry->dur_handlep->lock);
				
				/*
				 * Already in list due to previous open with deny modes. Can't have
				 * two exclusive or two write/denyWrite. Multiple read/denyWrites are
				 * allowed.
				 */
				if ((accessMode & kDenyWrite) && (accessMode & kAccessWrite)) {
					error = EBUSY;
					goto exit;
				}
				
				if (mode & O_EXLOCK) {
					if ((accessMode & kAccessRead) && !(accessMode & kAccessWrite)) {
						/*
						 * Multiple r/dR/dW are allowed, see FindFileRef for
						 * more details
						 */
					}
					else {
						error = EBUSY;
						goto exit;
					}
				}
				
				/*
				 * We are going to reuse this Open Deny entry. Up the counter so
				 * we will know not to free it until the counter goes back to zero.
				 */
				fndEntry->refcnt++;
			}
		}
        else {
			/*
			 * Close any deferred file handles now to save time waiting on
			 * lease breaks.
			 */
			CloseDeferredFileRefs(vp, "smbfs_open deny", 0, context);

			/*
             * Check one more time looking for other pids that might already
             * have the file open for exclusive or shared access that will
             * cause this open to get denied. This is to reduce the number of 
             * times a handle lease break will occur with SMB 2/3. If we lose
             * the handle lease, then reopening a durable handle reconnect will 
             * fail.
             */
            error = FindFileRef(vp, NULL, accessMode, kPreflightOpen, 0, 0, &fndEntry, &fid);
            if (error == 0) {
                /* Some other process has it open already */
                error = EBUSY;
                goto exit;
            }
            
			/* not in list, so open new file */			
           
            /* Request a durable handle */
			SMB_MALLOC(dur_handlep, struct smb2_durable_handle *,
					   sizeof (struct smb2_durable_handle),
					   M_TEMP, M_WAITOK | M_ZERO);

			error = smb2_smb_dur_handle_init(share, np,
											 (SMB2_DURABLE_HANDLE_REQUEST | SMB2_NEW_LEASE_KEY), dur_handlep);
			dur_handlep->req_lease_state = smbfs_get_req_lease_state((rights | addedReadRights));

			if (error == 0) {
				/* Tell smbfs_smb_ntcreatex to try to get a durable handle */
				create_dur_handlep = dur_handlep;
			}
            
            if (np->n_flag & N_ISRSRCFRK) {
                disp = FILE_OPEN_IF;
                do_create = TRUE;
            }
            else {
                disp = FILE_OPEN;
                do_create = FALSE;
            }

            error = smbfs_smb_ntcreatex(share, np,
                                        rights | addedReadRights, shareMode, VREG,
                                        &fid, NULL, 0,
                                        disp, FALSE, fap,
                                        do_create, create_dur_handlep, context);

			if (error && addedReadRights) {
				/* Failed to add read rights, so fall back to write only */
				addedReadRights = 0;
				dur_handlep->lease_state &= ~SMB2_LEASE_READ_CACHING;
				
				error = smbfs_smb_ntcreatex(share, np,
											rights, shareMode, VREG,
											&fid, NULL, 0,
											disp, FALSE, fap,
											do_create, create_dur_handlep, context);
			}
			
			if (error == 0) {
				/* 
				 * if open worked, save the file ref into file list
				 *
				 * Always pass in a dur handle pointer to be used
				 */
				AddFileRef (vp, p, accessMode, rights, fid, dur_handlep, NULL);
				
				/* Do not free dur_handlep as its now being used by File Ref */
				dur_handlep = NULL;
			}
		}
		
		goto exit;
	}

	/*
	 * If we get here, then deny modes are NOT being used. If the open call is 
	 * coming in from Carbon, then Carbon will follow immediately with an FSCTL 
	 * to turn off caching (I am assuming that denyNone means this file will 
	 * be shared among multiple process and that ByteRangeLocking will be used).
	 *
	 * no deny modes, so use the shared file reference
	 *
	 */
	
	/* 
	 * Close any deferred file handes now to save time waiting on
	 * lease breaks.
	 */
	CloseDeferredFileRefs(vp, "smbfs_open", 0, context);
	
	/* We have open file descriptor for non deny mode opens */
	if (np->f_fid != 0) { 
		/* Already open check to make sure current access is sufficient */
		switch (np->f_accessMode) {
			case (kAccessRead | kAccessWrite):
				/* Currently RW, can't do any better than that so dont open a new fork */
				break;
			case kAccessRead:
				/* Currently only have Read access, if they want Write too, then open as RW */
				if (accessMode & kAccessWrite) {
					needUpgrade = 1;
					accessMode |= kAccessRead; /* keep orginal mode */
					rights |= SMB2_FILE_READ_DATA;
				}
				break;
			case kAccessWrite:
				/*  Currently only have Write access, if they want Read too, then open as RW */
				if (accessMode & kAccessRead) {
					needUpgrade = 1;
					accessMode |= kAccessWrite;
					rights |= SMB2_FILE_APPEND_DATA | SMB2_FILE_WRITE_DATA;
				}
				break;
			}
		
		if (!needUpgrade) {
			/* The existing open is good enough */
			goto ShareOpen;
		}
	} else if (addedReadRights) {
         /*
		  * Try to open with added Read access
		  *
		  * Preflight for open deny to preserve handle lease. More details in
		  * O_EXLOCK/O_SHLOCK code above.
		  */
        error = FindFileRef(vp, NULL, accessMode | kAccessRead, kPreflightOpen,
                            0, 0, &fndEntry, &fid);
        if (error != 0) {
            /* Not already open locally, so try to open it */
            error = smbfs_smb_open_file(share, np,
                                        rights | addedReadRights, shareMode, &fid,
                                        NULL, 0, FALSE,
                                        fap, context);
            if (error == 0) {
                np->f_fid = fid;
                np->f_rights = rights | SMB2_FILE_READ_DATA;
                np->f_accessMode = accessMode | kAccessRead;
                goto ShareOpen;
            }
			else {
				/* Failed to open with added read */
				addedReadRights = 0;
			}
        }
	}

    /*
     * Preflight for open deny to preserve handle lease. More details in
     * O_EXLOCK/O_SHLOCK code above.
     */
    error = FindFileRef(vp, NULL, accessMode, kPreflightOpen, 0, 0, &fndEntry,
                        &fid);
    if (error == 0) {
        /* Some other process has it open already */
        error = EBUSY;
        goto exit;
    }

    error = smbfs_smb_open_file(share, np,
                                rights, shareMode, &fid,
                                NULL, 0, FALSE,
                                fap, context);
	if (error) {
		goto exit;
	}
	
	/*
	 * We already had it open (presumably because it was open with insufficient 
	 * rights.) So now close the old open, if we already had it open.
	 */
	if (np->f_refcnt && 
        (np->f_fid != 0)) {
		warning = smbfs_smb_close(share, np->f_fid, context);
		if (warning) {
			SMBWARNING_LOCK(np, "error %d closing %s\n", warning, np->n_name);
        }
	}
	np->f_fid = fid;
	np->f_rights = rights;
	np->f_accessMode = accessMode;
	
ShareOpen:
	smbfs_update_RW_cnts(vp, savedAccessMode);
	
exit:
	if ((accessMode == kAccessWrite) &&
		(addedReadRights == 0)) {
		/* if opened with just write, then turn off cluster code */
		vnode_setnocache(vp);
	}

	/* If we reused a deferred close, then dont update counts */
	if (error == 0) {
        /* We opened the file or pretended too; either way bump the count */
		np->f_refcnt++;
        
        /* keep track of how many opens for this file had write access */
        if (mode & FWRITE) {
            np->f_openTotalWCnt++;
        }
	}

    if (fap != NULL) {
        SMB_FREE(fap, M_SMBTEMP);
    }

	if (dur_handlep != NULL) {
		lck_mtx_lock(&dur_handlep->lock);
		dur_handlep->flags = 0;
		lck_mtx_unlock(&dur_handlep->lock);

		smb2_smb_dur_handle_free(dur_handlep);
		SMB_FREE(dur_handlep, M_TEMP);
	}

	SMB_LOG_KTRACE(SMB_DBG_SMBFS_OPEN | DBG_FUNC_END, error, 0, 0, 0, 0);
	return (error);
}

static int 
smbfs_vnop_open_common(vnode_t vp, int mode, vfs_context_t context, void *n_lastvop)
{
	struct smbnode *np;
	int	error;
    int ubc_invalidate = 0;
    struct smb_share *share = NULL;

	/* We only open files and directories and symlinks */
	if (!vnode_isreg(vp) && !vnode_isdir(vp) && !vnode_islnk(vp)) {
		return (EACCES);
    }
		
    /*
     * Check to see if its a dataless file and allowed to be materialized
     */
    if (!vnode_isdir(vp)) {
        error = smbfs_is_dataless_access_allowed(vp, 1, context);
        if (error) {
            /* This process is not allowed to materialize this file */
            return(error);
        }
    }
    
    if ((error = smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK))) {
		return (error);
    }
	
	np = VTOSMB(vp);
	np->n_lastvop = n_lastvop;
	
	/* Just mark that the directory was opened */
	if (vnode_isdir(vp)) {
		np->d_refcnt++;
		error = 0;
	}
    else {
		share = smb_get_share_with_reference(VTOSMBFS(vp));
		
        if (mode & O_TRUNC) {
            /* If truncating the file on open, do any pending set eofs */
            smbfs_smb_fsync(share, np, 0, context);
        }

        /*
         * On the first open, check if the current mod date or data size is
         * different from the last closed mod date or data size, then call
         * ubc_msync(UBC_INVALIDATE). The assumption is that someone changed
         * the file while we had it closed so our UBC data is invalid now.
         */
        if ((np->f_refcnt == 0) && smbfsIsCacheable(vp)) {
            /*
             * Make sure we hit the server to get the latest info so we can
             * check the mod dates
             */
            np->attribute_cache_timer = 0;
            (void) smbfs_update_cache(share, vp, NULL, context);

            /* Only check if the file has been closed at least once */
            if (!((np->n_last_close_mtime.tv_sec == 0) && (np->n_last_close_mtime.tv_nsec == 0))) {
                if (timespeccmp(&np->n_last_close_mtime, &np->n_mtime, !=)) {
                    ubc_invalidate = 1;
                }
                else {
                    if (np->n_last_close_size != np->n_size) {
                        ubc_invalidate = 1;
                    }
                }
            }

            if (ubc_invalidate == 1) {
                if (vnode_hasdirtyblks(vp)) {
                    /*
                     * This should be impossible as we pushed out any dirty
                     * pages before the last close.
                     */
                    SMBERROR_LOCK(np, "File <%s> was modified while it was closed but still has dirty pages. Dropping the dirty pages. \n", np->n_name);
                }

                ubc_msync (vp, 0, ubc_getsize(vp), NULL, UBC_INVALIDATE);
            }
        }

        /* Open the file */
        error = smbfs_open(share, vp, mode, context);
		/* 
		 * We created the file with different posix modes than request in 
		 * smbfs_vnop_create. We need to correct that here, so set the posix
		 * modes to those request on create. See smbfs_set_create_vap for more 
		 * details on this issue.
		 */
		if ((error == 0) && (np->set_create_va_mode) && (mode & O_CREAT)) {
			struct vnode_attr vap;
			
			np->set_create_va_mode = FALSE;	/* Only try once */
			VATTR_INIT(&vap);
			VATTR_SET_ACTIVE(&vap, va_mode);
			vap.va_mode = np->create_va_mode;
			error = smbfs_setattr(share, vp, &vap, context);
			if (error)	/* Got an error close the file and return the error */
				(void)smbfs_close(share, vp, mode, context);
		}
        
        if (share != NULL) {
            smb_share_rele(share, context);
        }
	}
    
    if (error == EBUSY) {
		error = EAGAIN;
    }

	smbnode_unlock(np);
	return(error);
}

/*
 * smbfs_vnop_compound_open - smbfs vnodeop entry point
 *	vnode_t a_dvp;
 *	vnode_t *a_vpp; 
 *	int a_fmode;
 *	struct componentname *a_cnp;
 *	vnode_attr *a_vap;
 *	uint32_t a_flags;
 *	uint32_t *a_status;
 *	vfs_context_t context;
 *	int (*a_open_create_authorizer);
 *	int (*a_open_existing_authorizer);
 *	void *a_reserved;
 *
 * In broad strokes, our compound Open VNOP will be able to act as a 
 * VNOP_LOOKUP(), a VNOP_OPEN(), and possibly a VNOP_CREATE(), all in one trip 
 * to the filesystem.  Depending on flags, it may execute an operation similar 
 * to an open(2) with O_CREAT: lookup, create if not present, and open.  It may 
 * also act as a simple open(2) without O_CREAT: lookup and open if present.  
 * When it returns successfully, the file it has found or created will be "open" 
 * and ready to be hooked into a file descriptor.  As we will discuss later, this 
 * call may under some circumstances return to VFS with its operation unfinished, 
 * to request help from VFS, or fail but nonetheless return a vnode.  Filesystems 
 * will be responsible for calling the passed-in authorizer routines to make sure 
 * that the caller can legitimately perform the requested action. 
 *
 * Arguments in Detail
 *
 *		1.  a_dvp - The directory in which to open/create. The directory vnode is 
 *			equivalent to what would be passed to VNOP_LOOKUP() or VNOP_CREATE(): 
 *			it is the directory in which we wish to open a file, possibly doing 
 *			a create if the file is not already present.
 *		2.  a_vpp - Resulting vnode. This argument is a pointer to a "vnode_t" 
 *			in which the filesystem can place a pointer to the vnode for a file 
 *			it either looks up or creates. In the event of a hit in the name 
 *			cache, VFS will pass a file to the filesystem in *a_vpp. In that 
 *			case, the filesystem will be free to call vnode_put() on that vnode 
 *			and replace it with another of its choosing.  In the event of a cache 
 *			miss, *a_vpp will store NULLVP. On success, the pointer installed at 
 *			this address shall point to an "opened" vnode. On failure, it may 
 *			point to a vnode which is not open.  Because several facilities in 
 *			the kernel--notably auditing and kdebug support for fs_usage--rely 
 *			on examining vnodes between lookup and a subsequent VNOP, filesystems 
 *			should make every effort to return a vnode with an iocount if a lookup 
 *			would have succeeded for that file, regardless of whether the main 
 *			operation has failed.  This subtlety will apply to all other compound 
 *			VNOPs as well, including those which might fail with EEXIST (e.g. mkdir).  
 *			An unopened vnode may also be returned because more help is required 
 *			from VFS (EKEEPLOOKING).
 *		3.  a_fmode - Open mode. This field will contain open flags as normally 
 *			passed to VNOP_OPEN(), e.g. O_TRUNC.  For filesystems implementing 
 *			compound open, O_TRUNC must be handled as part of the compound open 
 *			call--a subsequent VNOP_SETATTR() call will not be sent to set the size.
 *		4.  a_cnp - Path to look up. This field will contain a componentname, as 
 *			passed to VNOP_LOOKUP() or VNOP_CREATE(), specifying the name of the 
 *			file to look up and possibly create.
 *		5.  a_vap - Attributes with which to create, if appropriate. This field 
 *			will contain a pointer to vnode attributes to be used in creating a 
 *			file.  In the event of an open without O_CREAT, this field will be NULL.  
 *			In the event of a cache hit, it will also be NULL, as we wish to take 
 *			a fast path in this case which does not involve the heavyweight operation 
 *			of initializing creation attributes.  This field can be ignored in 
 *			the event that an existing file is detected.
 *		6.  a_flags - VNOP-control flags. This field will contain control flags 
 *			specific to the compound open VNOP.  For now, there will be only one 
 *			flag: VNOP_COMPOUND_OPEN_DO_CREATE  will indicate that if no existing 
 *			file is found, a new one is to be created (authorization, etc. permitting).
 *		7.  a_status - Information about results. The filesystem will use this 
 *			field to return information about the result of the VNOP.  At the moment, 
 *			there will be only one flag: COMPOUND_OPEN_STATUS_DID_CREATE  will 
 *			indicate that a file was created by this call.  The field will be set 
 *			to zero before a pointer is passed to the filesystem, so the FS will 
 *			be free (for now) to only touch it in the event of having done a 
 *			create.  COMPOUND_OPEN_STATUS_DID_CREATE should never be set if 
 *			VNOP_COMPOUND_OPEN_DO_CREATE was not set in a_flags.
 *		8.  a_context - Authorization context. This field is a vfs_context_t, 
 *			just as is passed to most VNOPs.
 *		9.  a_open_create_authorizer - Authorizer for create case. This field 
 *			will contain a pointer to a function to be called to authorize creating 
 *			a file.  It need only be called if a file is to be created; for an 
 *			existing file, a_open_existing_authorizer should be used.  The 
 *			componentname and vnode_attr passed to this function should be exactly 
 *			those which were passed to the VNOP, because opaque data in these 
 *			structures, installed by VFS, will be interpreted in authorization 
 *			(the same will apply to all other authorizer callbacks).  Some additional 
 *			validation which is not authorization per se, but which is currently 
 *			performed in VFS on behalf of the filesystem, will also be executed 
 *			here.  As a result of that validation, a variety of errors may be 
 *			returned; these should generally be passed back to VFS.  Filesystems 
 *			which will attempt a compound RPC that may result in a file's being 
 *			created should always call this function beforehand.  In that case, 
 *			if authorization is denied, the filesystem should attempt an "open 
 *			without O_CREAT," because an open of an existing file may succeed 
 *			despite a denial of permission to create.  The reserved argument should 
 *			currently always be NULL.  
 *			NOTE:  The locking constraints with respect to this call, and all 
 *			authorizer callbacks in this document must unfortunately be left 
 *			somewhat ambiguous due to the unrestricted behavior of Kauth and 
 *			MACF plugins.  However, I believe that it should be safe to call out 
 *			with resources (iocounts, memory, references) held, and even with some 
 *			kinds of locks (a lock preventing only remove of a file, for instance).  
 *			The chief thing to avoid is holding big locks (directory lock, file lock) 
 *			which would prevent stat(2) and similar operations which authorizers 
 *			are likely to try to use.
 *		10. a_open_existing_authorizer - Authorizer for preexisting case. This 
 *			field will contain a pointer to a function to be called to authorize 
 *			opening an existing file.  It need only be called if a file has not 
 *			been created.  The componentname passed to this function should be 
 *			exactly that which was passed to the VNOP, because opaque data in 
 *			this structure, installed by VFS, will be interpreted in authorization.  
 *			Some additional validation which is not authorization per se, but which 
 *			is currently performed in VFS on behalf of the filesystem, will also 
 *			be executed here.  As a result of that validation, a variety of errors 
 *			may be returned; these should generally be passed back to VFS.  
 *			The reserved argument should currently always be NULL.  For networked 
 *			filesystems, this routine can be used after going over the wire with a 
 *			compound lookup+open RPC; if access is denied, the filesystem should 
 *			clean up (e.g. issue a close over the wire) and return to VFS.
 *		11. a_reserved - This field is currently unused and should not be interpreted.
 *
 * Symlinks, Mountpoints, Trigger Points:
 * 
 * int vnode_lookup_continue_needed(vnode_t vp, struct componentname *cnp);
 * 
 * #define EKEEPLOOKING    (-7)
 * 
 * As part of a compound operation, a file may be detected which the filesystem 
 * is not equipped to handle--for instance, a mountpoint, a symlink if O_NOFOLLOW 
 * is set (which may point to another volume), or a trigger point. VFS will 
 * provide the above routine to determine if a given vnode is one which requires 
 * additional VFS processing.  It will take a vnode (the preexisting file 
 * discovered by a compound VNOP) and a componentname; the latter should be the 
 * exact pointer which was passed to the filesystem by VFS, because opaque data 
 * will be interpreted to make the decision.  If this routine returns a nonzero 
 * value, the filesystem should return EKEEPLOOKING to VFS.  
 * vnode_lookup_continue_needed() will also update an opaque field on the 
 * componentname for interpretation by VFS--while unnecessary for VNOP_COMPOUND_OPEN(), 
 * for a rename (where two lookups take place) that update will let VFS determine 
 * which lookup needs to be continued.  This helper will be used in numerous 
 * compound VNOPs.
 *
 * "Traditional"VNOPs, Dark Corners of the Kernel, and the Root of a Filesystem
 *
 * VNOP_LOOKUP() will still need to be supported to allow us to reach intermediate 
 * components of a path.  Currently, we plan to require that filesystems continue 
 * to support the "traditional" VNOP_CREATE() and VNOP_OPEN() (and correspondingly, 
 * the traditional version of other namespace-changing operations).  The chief 
 * reason for this is that there are numerous areas in the kernel that use these 
 * old-style operations directly.  After some discussion with various filesystem 
 * owners, I think that it is best that we gradually adjust those areas to be 
 * able to use compound VNOPs, thereby limiting the instability from moving them 
 * all at once.  When all internal code has been prepared to use a given compound 
 * VNOP, filesystems will be able to remove their "traditional" implementation.  
 * VNOP_OPEN() deserves a bit of special discussion.  There are some places in 
 * the kernel where we "open" a vnode without doing a true lookup; the best example 
 * is the root of a filesystem, which is never "looked up" the way other entries 
 * in a volume are, but there are other places where we have in hand that a vnode 
 * that was obtained at some point in the past.  I therefore think that we will 
 * probably keep VNOP_OPEN() around for the long term, rather than increasing the 
 * complexity of the spreadsheet of "which arguments can be NULL, and when" for 
 * VNOP_COMPOUND_OPEN().
*/
static int 
smbfs_vnop_compound_open(struct vnop_compound_open_args *ap)
{
	vnode_t dvp = ap->a_dvp;
	vnode_t *vpp = ap->a_vpp;
	vnode_t vp = (ap->a_vpp) ? *ap->a_vpp : NULL;
	vfs_context_t context = ap->a_context;
	struct componentname *cnp = ap->a_cnp;
	struct vnode_attr *vap = ap->a_vap;
	int fmode = ap->a_fmode;
	int error, error2;
	int create_authorizer_error = 0;
	uint32_t open_disp = 0;
	struct smb_share *share = NULL;
	struct smbnode *dnp;
    SMBFID fid = 0;
	uint32_t vid;
    struct smbfattr *fap = NULL;
    const char *namep = cnp->cn_nameptr;
    size_t name_len = cnp->cn_namelen;
	uint32_t flags = cnp->cn_flags;

	if (vpp == NULL) {
		SMBWARNING("Calling us without a vpp\n");
		return ENOTSUP;
	}

    /*
     * Case 1:
     *    They passed in a vnode they found in the name cache or we found
     *    one in our hash table and its a symlink or reprase point. Not sure what
     *    to do with reprase points yet. Symlinks are easy call vnode_lookup_continue_needed
     *    and let it tell us what to do.
     *
     * Case 2:
     *    They passed in a vnode they found in the name cache or we found in our
     *    hash table. If they don't have O_CREAT set or its already open then we
     *    can just do a normal open. We need to call a_open_existing_authorizer
     *    before calling smbfs_vnop_open_common. If O_TRUNC is set then we need to
     *    set the file size to zero. Even on error we need to return the vnode
     *    with a reference.
     *
     * Case 3:
     *    We have no vnode or they have O_CREAT set and we don't have it already
     *    open. Set the correct open dispostion depending on the modes passed in
     *    and what a_open_create_authorizer returns. We only call a_open_create_authorizer
     *    if O_CREAT is set. Now we let smbfs_create_open handle all other cases.
     *    Will use <rdar://problem/8574808> to update this comment and to make
     *    reprase points and symlinks work correctly.
     *
     *
     * Notes:
     * 1) vap may be null, if null on create then thats not supported
     * 2) if a_vpp is null and we return a vnode, dont do a vnode_put on it as the
     *    vfs open code will call vnode_put on it for us.
     */

	/*
	 * We may have to create the item make sure it has a vap and they
	 * are creating a file. We only support create on files.
	 */
	if ((fmode & O_CREAT) && ((vap == NULL) || (vap->va_type != VREG))) {
		return (ENOTSUP);
	}
	
    SMB_LOG_KTRACE(SMB_DBG_CMPD_OPEN | DBG_FUNC_START, fmode, 0, 0, 0, 0);

    SMB_MALLOC(fap,
               struct smbfattr *, 
               sizeof(struct smbfattr), 
               M_SMBTEMP, 
               M_WAITOK | M_ZERO);
    if (fap == NULL) {
        SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto done;
    }

	share = smb_get_share_with_reference(VTOSMBFS(dvp));

	/* They didn't pass us a vnode, see if we have one in our hash */
	if (vp == NULLVP) {
        SMB_LOG_KTRACE(SMB_DBG_CMPD_OPEN | DBG_FUNC_NONE, 0xabc001, 0, 0, 0, 0);

		if (((cnp->cn_namelen == 1) && (cnp->cn_nameptr[0] == '.')) ||
			(flags & ISDOTDOT)) {
			if (flags & ISDOTDOT) {
				/* 
				 * Dot Dot to be opened, so return parent of dvp 
				 *
				 * Call vnode_lookup_continue_needed() sinces its ".." to be
				 * sure we can handle this (ie unlikely case that vp is 
				 * our root vnode and thus we can not get the parent)
				 */
				error = vnode_lookup_continue_needed(dvp, cnp);
				if (error) {
					error2 = vnode_get(dvp);
					if (!error2) {
						/* Dont change the original error value */
						*vpp = dvp;
					}
					else {
						/* vnode_get failed, so return that error instead */
						error = error2;
					}

					goto done;
				}
				
				/* Get the parent vp of dvp */
				dnp = VTOSMB(dvp);
				vp = smbfs_smb_get_parent(dnp, kShareLock);
				if (vp == NULL) {
					if (dnp->n_parent_vid != 0) {
						/* Parent got recycled already? */
						SMBWARNING_LOCK(dnp, "Missing parent for <%s> \n",
										dnp->n_name);
					}
					
					error = ENOENT;
					goto done;
				}
			}
			else {
				/*
				 * They didn't give us a vnode, but they want Dot opened, so
				 * get a reference on the parent node dvp and return it.
				 */
				vid = vnode_vid(dvp);
				error = vnode_getwithvid(dvp, vid);
				if (error) {
					goto done;
				}
				vp = dvp;
			}
		}
        else {
            if (SS_TO_SESSION(share)->session_misc_flags & SMBV_HAS_FILEIDS) {
                /* 
                 * Server supports File IDs.
                 */
                
                /*
                 * Before we can see if vnode already exists in our hash,
                 * need to get the inode number first. Might as well get all
                 * the meta data too so we can update the vnode if its found
				 *
				 * <29519410> Always use smb2fs_smb_cmpd_query_dir_one() instead
				 * of smbfs_smb_qpathinfo() because we dont need to know the
				 * vnode type before making the call.
                 */
				/*
				 * Should never be a named stream vnode
				 */
				if ((VTOSMB(dvp)->n_vnode) && vnode_isnamedstream(VTOSMB(dvp)->n_vnode)) {
					DBG_ASSERT(0);
				}

				error = smb2fs_smb_cmpd_query_dir_one(share, VTOSMB(dvp),
													  namep, name_len,
													  fap, (char **) &namep, &name_len,
													  context);

				SMB_LOG_KTRACE(SMB_DBG_CMPD_OPEN | DBG_FUNC_NONE,
                               0xabc002, error, 0, 0, 0);
                
                if (error == 0) {
                    /* Lock the parent */
                    dnp = VTOSMB(dvp);
                    if (smbnode_lock(dnp, SMBFS_EXCLUSIVE_LOCK) != 0) {
                        error = ENOENT;
                        goto done;
                    }
                    
                    /* 
                     * Found item on server, see if its in the hash.
                     * If it is in hash, then update its meta data and return vp
                     * If its not in hash, smbfs_nget will create it for us and
                     * return it in vp. Either way, we get back a vp.
                     */
                    if (smbfs_nget(share, vnode_mount(dvp),
                                   dvp, cnp->cn_nameptr, cnp->cn_namelen,
                                   fap, &vp,
                                   cnp->cn_flags, SMBFS_NGET_CREATE_VNODE,
                                   ap->a_context) == 0) {
                        /* 
                         * Found one in our hash table unlock it, we just need 
                         * the vnode reference at this point
                         */
                        smbnode_unlock(VTOSMB(vp));
                    }
                    
                    /* Unlock the parent */
                    dnp->n_lastvop = smbfs_vnop_compound_open;
                    smbnode_unlock(dnp);

                    /*  If smbfs_smb_qpathinfo returned a new name, free it */
                    if (namep != cnp->cn_nameptr) {
                        SMB_FREE(namep, M_SMBNODENAME);
                    }
                }
                else {
                    /* Probably not found, either way vp is left at NULL */
                    goto not_found;
                }
            }
            else {
                /* Lock the parent */
                dnp = VTOSMB(dvp);
                if (smbnode_lock(dnp, SMBFS_EXCLUSIVE_LOCK) != 0) {
                    error = ENOENT;
                    goto done;
                }
                
                /*
                 * Server does not support File IDs.
                 * Use the name for the hash value and try to find the vnode 
                 * in the hash table with just the parent vnode and name. If its
                 * does not already exist in the hash,then smbfs_nget will
                 * return ENOENT (since fap == NULL) and vp will be left as NULL
                 */
                if (smbfs_nget(share, vnode_mount(dvp),
                               dvp, cnp->cn_nameptr, cnp->cn_namelen,
                               NULL, &vp,
                               cnp->cn_flags, SMBFS_NGET_LOOKUP_ONLY,
                               ap->a_context) == 0) {
                    /* 
                     * Found one in our hash table. Unlock it as we just need
                     * the vnode reference at this point
                     */
                    smbnode_unlock(VTOSMB(vp));

                    /* Check if item still exists on the server */
                    error = smbfs_smb_query_info(share, dnp, vnode_vtype(vp),
                                                 cnp->cn_nameptr, cnp->cn_namelen,
                                                 NULL, context);
                    if (error != 0) {
                        /*
                         * Could not find or access the file, remove it from
                         * the hash table and reset vp back to NULL. Continue
                         * on as if we had never found it in the hash table.
                         */
                        smb_vhashrem(VTOSMB(vp));
                        vnode_put(vp);
                        vp = NULLVP;
                    }
                }
                
                /* Unlock the parent */
                dnp->n_lastvop = smbfs_vnop_compound_open;
                smbnode_unlock(dnp);
            }
        }
    }
	
	/*
	 * Symlink or reparse point. Call vnode_lookup_continue_needed before
	 * proceeding.
	 */
	if (vp && (vnode_islnk(vp) || (VTOSMB(vp)->n_dosattr & SMB_EFA_REPARSE_POINT))) {
		SMBDEBUG_LOCK(VTOSMB(vp), "symlink %s\n", VTOSMB(vp)->n_name);
        
		error = vnode_lookup_continue_needed(vp, cnp);
		if (error) {
			*vpp = vp;
			vp = NULL;
			goto done;
		}
		/* Let smbfs_vnop_open_common handle any open errors */
		fmode &= ~(O_CREAT | O_TRUNC); /* Can't create or truncate a symlink */
	}
	
	/*
	 * Do a normal open:
	 * 1. We have a vnode and they just want to open the file or directory. The
	 *    O_CREAT/O_TRUNC is not set. NOTE they can't have O_CREAT set on a directory.
	 * 2. We have a vnode to a file and its already opened.
	 * 
	 * NOTE: They could be opening a file with O_CREAT, we found a directory in 
	 * our hash, yet it doesn't exist on the server. So we should create the file
	 * and remove the directory node from our hash table. In this case we should
	 * falldown to the create open code.
	 */
	
	if (vp && (!(fmode & (O_CREAT | O_TRUNC)) || vnode_isreg(vp))) {
        
        if ((fmode & O_EXCL) && vnode_isreg(vp)) {
            /*
             * O_EXCL is set and we *know* the file exists (we have
             * a vp). No need to go to the server, since we know the result.
             */
            if (fmode & O_CREAT) {
                /* If O_CREAT and O_EXCL and file exists then EEXISTS */
                error = EEXIST;
				*vpp = vp;
				vp = NULL;
				goto done;
           }
        }
        
		/* Call back and see if it is ok to be opened */
		error = ap->a_open_existing_authorizer(vp, cnp, fmode, context, NULL);
		if (!error) {
			error = smbfs_vnop_open_common(vp, fmode, context, smbfs_vnop_compound_open);
            SMB_LOG_KTRACE(SMB_DBG_CMPD_OPEN | DBG_FUNC_NONE,
                           0xabc003, error, 0, 0, 0);
		}
		
		/* The file was already open, but they wanted us to truncate it. */
		if (!error && (fmode & O_TRUNC)) {
			struct vnode_attr va;
			
			memset(&va, 0, sizeof(va));
			VATTR_INIT(&va);
			VATTR_SET_ACTIVE(&va, va_data_size);
			error = smbfs_setattr(share, vp, &va, context);
			if (error)	{
				/* Got an error close the file and return the error */
				(void)smbfs_close(share, vp, fmode, context);
			}
		}
		/* Even if the truncate fails we need to return the vnode */
		*vpp = vp;
		vp = NULL;
		goto done;
	}
	
not_found:
	/* Set the default create dispostion value */
	create_authorizer_error = 0;
    
	if (fmode & O_TRUNC) {
		open_disp = FILE_OVERWRITE;
        if (vp) {
            /* If truncating the file on open, do any pending set eofs */
            smbfs_smb_fsync(share, VTOSMB(vp), 0, context);
        }
	}
    else {
		open_disp = FILE_OPEN;
	}
    
	if (fmode & O_CREAT) {
		/* Call back and see if it is ok to be created */
		create_authorizer_error = ap->a_open_create_authorizer(dvp, cnp, vap, context, NULL);
		if (!create_authorizer_error) {
			/* We can create so set the correct create dispostion value */
			if (fmode & O_EXCL) {
				open_disp = FILE_CREATE;
			} else if (fmode & O_TRUNC) {
				open_disp = FILE_OVERWRITE_IF;
			} else {
				open_disp = FILE_OPEN_IF;
			}
		}
	}	
	
	/* Lock the parent */
	if ((error = smbnode_lock(VTOSMB(dvp), SMBFS_EXCLUSIVE_LOCK))) {
		goto done;
	}
	
	dnp = VTOSMB(dvp);
	dnp->n_lastvop = smbfs_vnop_compound_open;

	/* Try to create/open the file */
	error = smbfs_create_open(share, dvp, cnp, vap, open_disp, fmode, &fid, fap, &vp, context);
	smbnode_unlock(dnp);

    SMB_LOG_KTRACE(SMB_DBG_CMPD_OPEN | DBG_FUNC_NONE, 0xabc004, error, 0, 0, 0);
	if (error) {
		if (create_authorizer_error) {
			error = create_authorizer_error;
		}
	}
    else {
		if ((fap->fa_created_disp == FILE_CREATE) && (!(fmode & O_CREAT))) {
			/* 
			 * The server says it was created, but we didn't request it to be 
			 * created. The VFS layer can't handle this so just replace the 
			 * FILE_CREATE with FILE_OPEN and log what happen. This should never
			 * happen, but it is better than the VFS panicing.
			 */
			fap->fa_created_disp = FILE_OPEN;
			SMBERROR("Server created %s when we only wanted it open, server error\n", 
					 cnp->cn_nameptr);
		}
		if (fap->fa_created_disp == FILE_CREATE) {
			*ap->a_status = COMPOUND_OPEN_STATUS_DID_CREATE;
		} else {
			error = ap->a_open_existing_authorizer(vp, cnp, fmode, context, NULL);
			/* Error so close it */
			if (error) {
				(void)smbfs_smb_close(share, fid, context);
			}
			/* Not sure how to handle reparse yet, deal with that in <rdar://problem/8574808> */
			if (vnode_islnk(vp) || (VTOSMB(vp)->n_dosattr &  SMB_EFA_REPARSE_POINT)) {
				error = vnode_lookup_continue_needed(vp, cnp);
				if (!error && (vnode_islnk(vp))) {
					/* We never let them open a symlink */
					error = EACCES;
				}
			}
		}
		/* Note: Even on error, return the vnode */
		*vpp = vp;
		vp = NULL; /* Don't release the reference */
	}
	
done:
    if (error) {
        if ((fmode & O_CREAT) && (error == ENOENT)) {
            SMBDEBUG("Creating %s returned ENOENT, resetting to EACCES\n", cnp->cn_nameptr);
            /* 
             * Some servers (Samba) support an option called veto. This prevents
             * clients from creating or access these files. The server returns
             * an ENOENT error in these cases. The VFS layer will loop forever
             * if a ENOENT error is returned on create, so we convert this error
             * to EACCES.
             */
            error = EACCES;
        } else if ((fmode & O_EXCL) &&
                   (error != EEXIST) &&
                   (error != EISDIR)) {
            /*
             * Since O_EXCL is set, we really need to return EEXIST
             * if the file exists.
             * See <rdar://problem/10074916>
             */
            if ( (smbfs_smb_query_info(share, VTOSMB(dvp), VREG,
                                       cnp->cn_nameptr, cnp->cn_namelen,
                                       NULL, context) == 0)) {
                SMBDEBUG("%s: O_EXCL but error = %d, resetting to EEXIST.\n", __FUNCTION__, error);
                if (fmode & O_CREAT) {
                    /* If O_CREAT and O_EXCL and file exists then EEXISTS */
                    error = EEXIST;
                }
                else {
                    error = EAGAIN;
                }
            }          
        } 
    }

    if (share) {
		smb_share_rele(share, context);
	}
    
	/*
	 * We have an error and they didn't pass us in a vnode, then we found the
	 * vnode in our hash table. We need to remove our reference.
	 */
	if (error && (*vpp == NULLVP) && vp) {
		vnode_put(vp);
	}

    if (fap) {
        SMB_FREE(fap, M_SMBTEMP);
    }
    
    SMB_LOG_KTRACE(SMB_DBG_CMPD_OPEN | DBG_FUNC_END, error, 0, 0, 0, 0);
    return (error);
}

/*
 * smbfs_vnop_open - smbfs vnodeop entry point
 *	vnode_t a_vp;
 *	int  a_mode;
 *	vfs_context_t a_context;
 */
static int 
smbfs_vnop_open(struct vnop_open_args *ap)
{
    int error;
    
    SMB_LOG_KTRACE(SMB_DBG_OPEN | DBG_FUNC_START, 0, 0, 0, 0, 0);
    
	error = smbfs_vnop_open_common(ap->a_vp, ap->a_mode, ap->a_context, smbfs_vnop_open);
    
    SMB_LOG_KTRACE(SMB_DBG_OPEN | DBG_FUNC_END, error, 0, 0, 0, 0);
	return (error);
}

/*
 * smbfs_vnop_mmap_check
 * struct vnodeop_desc *a_desc;
 * vnode_t a_vp;
 * int flags;
 * vfs_context_t a_context;
*/
static int
smbfs_vnop_mmap_check(struct vnop_mmap_check_args *ap)
{
    vnode_t vp = ap->a_vp;
    struct smbnode *np = NULL;
    int error = 0;
    
    SMB_LOG_KTRACE(SMB_DBG_MMAP_CHECK | DBG_FUNC_START, 0, 0, 0, 0, 0);

    if (vp == NULL) {
        SMBERROR("vp is null \n");
        return(EINVAL);
    }

    np = VTOSMB(vp);
    if (np == NULL) {
        SMBERROR("np is null \n");
        return(EINVAL);
    }

    np->n_lastvop = smbfs_vnop_mmap_check;

    /*
     * Check to see if its a dataless file and allowed to be materialized
     */
    error = smbfs_is_dataless_access_allowed(vp, 0, ap->a_context);

    SMB_LOG_KTRACE(SMB_DBG_MMAP_CHECK | DBG_FUNC_END, error, 0, 0, 0, 0);
    
    return (error);
}

/*
 * smbfs_vnop_mmap - smbfs vnodeop entry point
 *	vnode_t a_vp;
 *	int a_fflags;
 *	vfs_context_t a_context;
 *
 * The mmap routine is a hint that we need to keep the file open. We can get  
 * mutilple mmap before we get a mnomap. We only care about the first one. We 
 * need to take a reference count on the open file and hold it open until we get 
 * a mnomap call. The file should already be open when we get the mmap call and 
 * with the correct open mode access.  So we shouldn't have to worry about 
 * upgrading because the open should have  handled that for us. If the open was 
 * done using an Open Deny mode then we need to mark the open deny entry as being 
 * mmaped so the pagein, pageout, and mnomap routines can find.
 *
 * NOTE: On return all errors are ignored except EPERM. 
 */
static int 
smbfs_vnop_mmap(struct vnop_mmap_args *ap)
{
	vnode_t			vp = ap->a_vp;
	struct smbnode *np = NULL;
	int				error = 0;
	uint32_t		mode = (ap->a_fflags & PROT_WRITE) ? (FWRITE | FREAD) : FREAD;
	int				accessMode = (ap->a_fflags & PROT_WRITE) ? kAccessWrite : kAccessRead;
    SMBFID fid = 0;
	struct fileRefEntry *entry = NULL;
	
	if ((error = smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK)))
		return (EPERM);

    SMB_LOG_KTRACE(SMB_DBG_MMAP | DBG_FUNC_START, 0, 0, 0, 0, 0);

	np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_mmap;	

	/* We already have it mapped, just ignore this one */
	if (np->n_flag & NISMAPPED)
	    goto out;
	/*
	 * Since we should already be open with the correct modes then we should never
	 * need to really open the file. For now we try these three cases. 
	 *
	 * First try the simple case, we have a posix open with the correct access.
	 *
	 * Second see if we have a match in the open deny mode list. Still not 100% sure this
	 * will work ever time because they are passing the current context, which may not match
	 * the one passed to open. From taking to Joe he believe we will always be in the open 
	 * context when call. From my testing this seems to be true.
	 *
	 * Third just return EPERM.
	 */
	if ((np->f_fid != 0) && 
        (np->f_accessMode & accessMode)) {
		np->f_refcnt++;
	} else if (FindFileRef(vp, vfs_context_proc(ap->a_context), accessMode, 
							   kAnyMatch, 0, 0, &entry, &fid) == 0) {
		entry->refcnt++;
		entry->mmapped = TRUE;
		np->f_refcnt++;
	} else {
		SMBERROR_LOCK(np, "%s We could not find an open file with mode = 0x%x? \n",
                      np->n_name, mode);
		error = EPERM;
		goto out;
	}
	np->n_flag |= NISMAPPED;
	np->f_mmapMode = mode;
out:	
	smbnode_unlock(np);
    
    SMB_LOG_KTRACE(SMB_DBG_MMAP | DBG_FUNC_END, error, 0, 0, 0, 0);
	return (error);
}

/*
 * smbfs_vnop_mnomap - smbfs vnodeop entry point
 *	vnode_t a_vp;
 *	vfs_context_t a_context;
 *
 * When called this is a hint that we can now close the file. We will not get any
 * more pagein or pageout calls without another mmap call.  If our reference count
 * is down to one then all we have to do is call close and it will clean everything
 * up. Otherwise we have a little more work to do see below for more details.
 *
 * NOTE: All errors are ignored by the calling routine
 */
static int 
smbfs_vnop_mnomap(struct vnop_mnomap_args *ap)
{
	vnode_t				vp = ap->a_vp;
	struct smbnode		*np;
	struct fileRefEntry *entry;
	int					error = 0;

	if (smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK))
		return (EPERM);	/* Not sure what to do here, they ignore errors */

    SMB_LOG_KTRACE(SMB_DBG_MNOMAP | DBG_FUNC_START, 0, 0, 0, 0, 0);

    np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_mnomap;

	/* Only one open reference just call close and let it clean every thing up. */
	if (np->f_refcnt == 1) {
		struct smb_share *share;
		
		share = smb_get_share_with_reference(VTOSMBFS(vp));

        /*
         * Copied behavior from AFP client (7945205) Push any dirty pages out
         * now to reduce the possibility that a page out will occur after
         * vnop_inactive was called
         */
        if (!vnode_vfsisrdonly(vp) && smbfsIsCacheable(vp)) {
            ubc_msync(vp, 0, ubc_getsize(vp), NULL, UBC_PUSHDIRTY | UBC_SYNC | UBC_INVALIDATE);
        }

		error = smbfs_close(share, vp, np->f_mmapMode, ap->a_context);
		smb_share_rele(share, ap->a_context);
		if (error) {
			SMBWARNING_LOCK(np, "%s close failed with error = %d\n",
                            np->n_name, error);
        }
		goto out;
	} else {
        if (np->f_refcnt > 0) {
            np->f_refcnt--;
        }
	}
	/*
	 * We get passed the current context which may or may not be the the same as the one used
	 * in the open. So search the list and see if there are any mapped entries. Remember we only
	 * have one item mapped at a time.
	 */
	if (FindMappedFileRef(vp, &entry, NULL) == TRUE) {
		entry->mmapped = FALSE;
		if (entry->refcnt > 0)	/* This entry is still in use don't remove it yet. */
			entry->refcnt--;
		else /* Done with it remove it from the list */
		    RemoveFileRef(vp, entry, 0);
	}
out:
	np->f_mmapMode = 0;
	np->n_flag &= ~NISMAPPED;
	smbnode_unlock(np);

    SMB_LOG_KTRACE(SMB_DBG_MNOMAP | DBG_FUNC_END, error, 0, 0, 0, 0);
    return (error);
}

/*
 * Last reference to the cnode.
 * 
 * smbfs_vnop_inactive - smbfs vnodeop entry point
 *	vnode_t a_vp;
 *	vfs_context_t a_context;
 */
static int 
smbfs_vnop_inactive(struct vnop_inactive_args *ap)
{
	vnode_t vp = ap->a_vp;
	struct smbnode *np;
	struct smb_share *share = NULL;
	int releaseLock = TRUE;

    SMB_LOG_KTRACE(SMB_DBG_INACTIVE | DBG_FUNC_START, 0, 0, 0, 0, 0);

	(void)smbnode_lock(VTOSMB(vp), SMBFS_RECLAIM_LOCK);
	np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_inactive;
	share = smb_get_share_with_reference(VTOSMBFS(vp));
	
    /*
	 * Before we take the lock, someone could jump in and do an open and start using this vnode again.
	 * We now check for that and just skip out if it happens. We will get another inactive later, if 
	 * this volume is not being force unmount. So check here to see if the vnode is in use and the
	 * volume is not being forced unmounted. Note that Kqueue opens will not be found by vnode_isinuse.
	 */
	if ((vnode_isinuse(vp, 0)) && !(vfs_isforce(vnode_mount(vp)))) {
        goto out;
	}
	
	if (vnode_isdir(vp)) {
		np->d_refcnt = 0;
		if (np->d_kqrefcnt) {
			smbfs_stop_change_notify(share, vp, TRUE, ap->a_context, &releaseLock);
		}
	}
	
out:
	smb_share_rele(share, ap->a_context);
	if (releaseLock)
		smbnode_unlock(np);

    SMB_LOG_KTRACE(SMB_DBG_INACTIVE | DBG_FUNC_END, 0, 0, 0, 0, 0);
	return (0);
}

/*
 * Free smbnode, and give vnode back to system to be used for other purposes
 * 
 * As VNOP_INACTIVE is not necessarily called from vnodes being recycled and 
 * reclaimed, it is important that we do any post-processing necessary for the 
 * cnode in both places.
 *
 *		struct vnodeop_desc *a_desc;
 *		vnode_t a_vp;
 *		vfs_context_t a_context;
 */
static int smbfs_vnop_reclaim(struct vnop_reclaim_args *ap)
{
	vnode_t vp = ap->a_vp;
	struct smbnode *np = NULL;
	struct smbmount *smp = NULL;
    vnode_t par_vp = NULL;
	int error = 0;
	struct smb_share *share = NULL;

    SMB_LOG_KTRACE(SMB_DBG_RECLAIM | DBG_FUNC_START, 0, 0, 0, 0, 0);

	(void) smbnode_lock(VTOSMB(vp), SMBFS_RECLAIM_LOCK);
    
	np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_reclaim;
	smp = VTOSMBFS(vp);

#ifdef SMB_DEBUG
	if (vnode_isdir(vp)) {
		DBG_ASSERT((np->d_kqrefcnt == 0));	
	}
#endif // SMB_DEBUG

    if (vnode_isdir(vp)) {
		/* Close the enumeration fd and free the dir lease */
        smbfs_closedirlookup(np, 0, "vnop_reclaim", ap->a_context);
        smbfs_closedirlookup(np, 1, "vnop_reclaim", ap->a_context);

		/* Free the enumeration cache if it exists */
        smb_dir_cache_remove(vp, &np->d_main_cache, "main", "vnop_reclaim", 0);
        smb_dir_cache_remove(vp, &np->d_overflow_cache, "overflow", "vnop_reclaim", 0);
		
		/*
         * Dont call smb_global_dir_cache_remove_one() as this will lead to
         * a deadlock where we are trying to get the global_dir_cache_lock()
         * but someone holding that lock is calling vnode_getwithvid() and
         * that is waiting for reclaim to finish. Just let the reclaim finish
         * and then the next time vnode_getwithvid() is called, it will get
         * ENOENT, then we can remove that entry from the global dir list
         */
    }
	else {
		/* 
		 * For files, close any deferred file handles
		 */
		CloseDeferredFileRefs(vp, "vnop_reclaim", 0, ap->a_context);
		
		/*
		 * Force close all remaining file refs (should be none by this time)
		 */
		share = smb_get_share_with_reference(VTOSMBFS(vp));
		
		if (np->f_refcnt > 0) {
			np->f_refcnt = 1;
			error = smbfs_close(share, vp, FREAD, ap->a_context);
			if (error) {
				SMBDEBUG_LOCK(np, "error %d closing fid %llx file %s\n",
							  error, np->f_fid, np->n_name);
			}
		}

		/*
		 * Does the file need to be deleted on close. Make one more check here
		 * just in case.
		 */
		if (np->n_flag & NDELETEONCLOSE) {
			error = smbfs_smb_delete(share, np, VREG,
									 NULL, 0,
									 0, ap->a_context);
			if (error) {
				SMBWARNING_LOCK(np, "error %d deleting silly rename file %s\n",
								error, np->n_name);
			}
			else np->n_flag &= ~NDELETEONCLOSE;
		}

		smb_share_rele(share, ap->a_context);


#ifdef SMB_DEBUG
		/* If the file is not in use then it should be closed. */
		DBG_ASSERT((np->f_refcnt == 0));
		DBG_ASSERT((np->f_openDenyList == NULL));
		DBG_ASSERT((np->f_smbflock == NULL));
#endif // SMB_DEBUG
	}
	
	/*
	 * Decrement child ref count in parent
	 */
    lck_rw_lock_exclusive(&np->n_parent_rwlock);    /* do our own locking */

    SET(np->n_flag, NTRANSIT);
	
    par_vp = smbfs_smb_get_parent(np, 0);  /* do our own locking */
    if (par_vp != NULL) {
        if ((np->n_flag & NREFPARENT) &&
            ((VTOSMB(par_vp)->n_flag & NTRANSIT) != NTRANSIT)) {
            /* 
             * Parent exists and is not being reclaimed, remove child's 
             * refcount 
             */
            vnode_rele(par_vp);
            OSDecrementAtomic(&VTOSMB(par_vp)->n_child_refcnt);
            
            np->n_parent_vnode = NULL;
            np->n_parent_vid = 0;
        }
        vnode_put(par_vp);
    }
    else {
        if (np->n_parent_vid != 0) {
            /* Parent got recycled already? Ok to ignore */
            SMBWARNING_LOCK(np, "Missing parent for <%s> \n", np->n_name);
        }
    }

    lck_rw_unlock_exclusive(&np->n_parent_rwlock);

    SMB_LOG_KTRACE(SMB_DBG_RECLAIM | DBG_FUNC_NONE,
                   0xabc001,
                   np->n_child_refcnt,
                   vfs_isforce(vnode_mount(vp)), 0, 0);

    /* Child_refcnt should be zero */
	if (np->n_child_refcnt) {
        if (!(vfs_isforce(vnode_mount(vp)))) {
            /*
             * Forced unmounts are very brutal, and it's not unusual for a 
             * parent node being reclaimed to have a non-zero child refcount. 
             * So we only log an error if this is not a forced unmount, which 
             * we do care about.
             */
            if (vnode_getname(vp) != NULL) {
                SMBERROR("%s: node: %s, n_child_refcnt not zero like it should be: %ld\n",
                         __FUNCTION__, vnode_getname(vp), (long) np->n_child_refcnt);
            }
            else {
                SMBERROR("%s: n_child_refcnt not zero like it should be: %ld\n",
                         __FUNCTION__, (long) np->n_child_refcnt);
            }
        }
        
		smbfs_ClearChildren(smp, np);
       
        smb_vhashrem(np);
    }
    else {
        smb_vhashrem(np);
    }
    
	cache_purge(vp);
    
    if (smp->sm_rvp == vp) {
		SMBVDEBUG("root vnode\n");
		smp->sm_rvp = NULL;
	}
    
	/* Destroy the lock used for the open state, open deny list and resource size/timer */
	if (!vnode_isdir(vp)) {
		/* Mutexes in both regular and stream nodes */
		lck_mtx_destroy(&np->f_openStateLock, smbfs_mutex_group);
		lck_mtx_destroy(&np->f_clusterWriteLock, smbfs_mutex_group);
		lck_mtx_destroy(&np->f_openDenyListLock, smbfs_mutex_group);

		/* Mutexes ONLY in non stream nodes - rfrkMetaLock and dur handle */
		if (!vnode_isnamedstream(vp)) {
			lck_mtx_lock(&np->f_dur_handle.lock);
			np->f_dur_handle.flags = 0;
			lck_mtx_unlock(&np->f_dur_handle.lock);
			
			smb2_smb_dur_handle_free(&np->f_dur_handle);
		}
	}
    else {
        lck_mtx_destroy(&np->d_enum_cache_list_lock, smbfs_mutex_group);
        lck_mtx_destroy(&np->d_cookie_lock, smbfs_mutex_group);

		lck_mtx_lock(&np->d_dur_handle.lock);
		np->d_dur_handle.flags = 0;
		lck_mtx_unlock(&np->d_dur_handle.lock);

		smb2_smb_dur_handle_free(&np->d_dur_handle);
    }

	/* Clear any symlink cache, always safe to do even on non symlinks */
    if (np->n_symlink_target != NULL) {
        SMB_FREE(np->n_symlink_target, M_TEMP);
    }
	np->n_symlink_target_len = 0;
	np->n_symlink_cache_timer = 0;
	
	/* We are done with the node clear the acl cache and destroy the acl cache lock  */
	if (!vnode_isnamedstream(vp)) {
		smbfs_clear_acl_cache(np);
		lck_mtx_destroy(&np->f_ACLCacheLock, smbfs_mutex_group);
		lck_mtx_destroy(&np->rfrkMetaLock, smbfs_mutex_group);
	}
	
	/* Free up both names before we unlock the node */
    lck_rw_lock_exclusive(&np->n_name_rwlock);
    if (np->n_name != NULL) {
        SMB_FREE(np->n_name, M_SMBNODENAME);
        np->n_name = NULL;
    }

    if (np->n_sname != NULL) {
        SMB_FREE(np->n_sname, M_SMBNODENAME);
        np->n_sname = NULL;
    }
    lck_rw_unlock_exclusive(&np->n_name_rwlock);

    /* Clear the private data pointer *before* unlocking the node, so we don't
     * race with another thread doing a 'np = VTOSMB(vp)'.
     */
    vnode_clearfsnode(vp);
	smbnode_unlock(np);

	CLR(np->n_flag, (NALLOC|NTRANSIT));
	if (ISSET(np->n_flag, NWALLOC) || ISSET(np->n_flag, NWTRANSIT)) {
		CLR(np->n_flag, (NWALLOC|NWTRANSIT));
		wakeup(np);
	}
	lck_rw_destroy(&np->n_rwlock, smbfs_rwlock_group);
	lck_rw_destroy(&np->n_name_rwlock, smbfs_rwlock_group);
	lck_rw_destroy(&np->n_parent_rwlock, smbfs_rwlock_group);
	SMB_FREE(np, M_SMBNODE);

	SMB_LOG_KTRACE(SMB_DBG_RECLAIM | DBG_FUNC_END, 0, 0, 0, 0, 0);
    return 0;
}

/*
 * The calling routine must hold a reference on the share
 */
static int
smbfs_unpack_smb_vap(vnode_t vp, struct vnode_attr *vap,
                     struct smb_vnode_attr *smb_vattrp)
{
    int error = EINVAL;
    struct smbnode *np = VTOSMB(vp);
    struct smbmount *smp = VTOSMBFS(vp);
    int isDir = 0;

    /*
     * Let vfs attr packing code know that we support 64 bit for va_linkid,
     * va_fileid, and va_parentid
     */
    vap->va_vaflags |= VA_64BITOBJIDS;

    /* Check to see if its a dir or not */
    if (VATTR_IS_ACTIVE(vap, va_objtype)) {
        /* Did the server tell us what the object is? */
        if (vap->va_objtype == VDIR) {
            isDir = 1;
        }
    }
    else {
        /* Guess from our current vnode */
        if (vnode_vtype(vp) == VDIR) {
            isDir = 1;
        }
    }

    /*
     * If they want the attr, copy it directly from smb_vattrp
     */

    /* traditional stat(2) parameter fields */
    /* Only for files */
    if (isDir == 0) {
        if (VATTR_IS_ACTIVE(vap, va_rdev)) {
            if (VATTR_IS_SUPPORTED(smb_vattrp, va_rdev)) {
                VATTR_RETURN(vap, va_rdev, smb_vattrp->va_rdev);
            }
            else {
                SMBERROR_LOCK(np, "Missing va_rdev for <%s> \n", np->n_name);
            }
        }

        if (VATTR_IS_ACTIVE(vap, va_nlink)) {
            if (VATTR_IS_SUPPORTED(smb_vattrp, va_nlink)) {
                VATTR_RETURN(vap, va_nlink, smb_vattrp->va_nlink);
            }
            else {
                SMBERROR_LOCK(np, "Missing va_nlink for <%s> \n", np->n_name);
            }
        }
    }

    /*
     * If the filesystem does not support datalength
     * or dataallocsize, then we infer that totalsize and
     * totalalloc are substitutes.
     */
    if (VATTR_IS_ACTIVE(vap, va_total_size)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_total_size)) {
            VATTR_RETURN(vap, va_total_size, smb_vattrp->va_total_size);
        }
    }

    if (VATTR_IS_ACTIVE(vap, va_total_alloc)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_total_alloc)) {
            VATTR_RETURN(vap, va_total_alloc, smb_vattrp->va_total_alloc);
        }
    }

    if (VATTR_IS_ACTIVE(vap, va_data_size)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_data_size)) {
            VATTR_RETURN(vap, va_data_size, smb_vattrp->va_data_size);
        }
        else {
            if (!VATTR_IS_SUPPORTED(smb_vattrp, va_total_size)) {
                SMBERROR_LOCK(np, "Missing va_data_size and va_total_size for <%s> \n", np->n_name);
            }
        }
    }

    if (VATTR_IS_ACTIVE(vap, va_data_alloc)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_data_alloc)) {
            VATTR_RETURN(vap, va_data_alloc, smb_vattrp->va_data_alloc);
        }
        else {
            if (!VATTR_IS_SUPPORTED(smb_vattrp, va_total_alloc)) {
                SMBERROR_LOCK(np, "Missing va_data_alloc and va_total_alloc for <%s> \n", np->n_name);
            }
        }
    }

    if (VATTR_IS_ACTIVE(vap, va_iosize)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_iosize)) {
            VATTR_RETURN(vap, va_iosize, smb_vattrp->va_iosize);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_iosize for <%s> \n", np->n_name);
        }
    }

    /* file security information */
    if (VATTR_IS_ACTIVE(vap, va_uid)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_uid)) {
            VATTR_RETURN(vap, va_uid, smb_vattrp->va_uid);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_uid for <%s> \n", np->n_name);
        }
    }

    if (VATTR_IS_ACTIVE(vap, va_gid)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_gid)) {
            VATTR_RETURN(vap, va_gid, smb_vattrp->va_gid);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_gid for <%s> \n", np->n_name);
        }
    }

    if (VATTR_IS_ACTIVE(vap, va_mode)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_mode)) {
            VATTR_RETURN(vap, va_mode, smb_vattrp->va_mode);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_mode for <%s> \n", np->n_name);
        }
    }

    if (VATTR_IS_ACTIVE(vap, va_flags)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_flags)) {
            VATTR_RETURN(vap, va_flags, smb_vattrp->va_flags);

            /* Client checks in various spots to see if immutable */
            if (vap->va_flags & (SF_IMMUTABLE | UF_IMMUTABLE)) {
                np->n_dosattr |= SMB_EFA_RDONLY;
            }
            else {
                np->n_dosattr &= ~SMB_EFA_RDONLY;
            }
        }
        else {
            SMBERROR_LOCK(np, "Missing va_flags for <%s> \n", np->n_name);
        }
    }

    /*
     * If va_acl is wanted and the server indicates there is an ACL,
     * then dont return va_acl, va_uuid, or va_guuid. The VFS will then
     * try to read the KAUTH_FILESEC_XATTR and fill in those fields for us.
     */
    if ((VATTR_IS_ACTIVE(vap, va_acl)) &&
        (VATTR_IS_SUPPORTED(smb_vattrp, va_acl))) {
        /*
         * They want ACL and one exists on server, do not return va_acl,
         * va_uuid or va_guuid. Let vfs try to get the xattr instead and
         * return all of those values for us.
         */
    }
    else {
        /* If they only want va_uuid or va_guuid, we can do that */
        if (VATTR_IS_ACTIVE(vap, va_uuuid)) {
            if (VATTR_IS_SUPPORTED(smb_vattrp, va_uuuid)) {
                memcpy(&vap->va_uuuid, smb_vattrp->va_uuuid,
                       sizeof(vap->va_uuuid));
                VATTR_SET_SUPPORTED(vap, va_uuuid);
            }
            else {
                SMBERROR_LOCK(np, "Missing va_uuuid for <%s> \n", np->n_name);
            }
        }

        if (VATTR_IS_ACTIVE(vap, va_guuid)) {
            if (VATTR_IS_SUPPORTED(smb_vattrp, va_guuid)) {
                memcpy(&vap->va_guuid, smb_vattrp->va_guuid,
                       sizeof(vap->va_guuid));
                VATTR_SET_SUPPORTED(vap, va_guuid);
            }
            else {
                SMBERROR_LOCK(np, "Missing va_guuid for <%s> \n", np->n_name);
            }
        }
    }

    /* timestamps */
    if (VATTR_IS_ACTIVE(vap, va_create_time)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_create_time)) {
            vap->va_create_time.tv_sec = smb_vattrp->va_create_time.tv_sec;
            vap->va_create_time.tv_nsec = smb_vattrp->va_create_time.tv_nsec;
            VATTR_SET_SUPPORTED(vap, va_create_time);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_create_time for <%s> \n", np->n_name);
        }
    }

    if (VATTR_IS_ACTIVE(vap, va_access_time)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_access_time)) {
            vap->va_access_time.tv_sec = smb_vattrp->va_access_time.tv_sec;
            vap->va_access_time.tv_nsec = smb_vattrp->va_access_time.tv_nsec;
            VATTR_SET_SUPPORTED(vap, va_access_time);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_access_time for <%s> \n", np->n_name);
        }
    }

    if (VATTR_IS_ACTIVE(vap, va_modify_time)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_modify_time)) {
            vap->va_modify_time.tv_sec = smb_vattrp->va_modify_time.tv_sec;
            vap->va_modify_time.tv_nsec = smb_vattrp->va_modify_time.tv_nsec;
            VATTR_SET_SUPPORTED(vap, va_modify_time);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_modify_time for <%s> \n", np->n_name);
        }
    }

    if (VATTR_IS_ACTIVE(vap, va_change_time)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_change_time)) {
            vap->va_change_time.tv_sec = smb_vattrp->va_change_time.tv_sec;
            vap->va_change_time.tv_nsec = smb_vattrp->va_change_time.tv_nsec;
            VATTR_SET_SUPPORTED(vap, va_change_time);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_change_time for <%s> \n", np->n_name);
        }
    }

    if (VATTR_IS_ACTIVE(vap, va_backup_time)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_backup_time)) {
            vap->va_backup_time.tv_sec = smb_vattrp->va_backup_time.tv_sec;
            vap->va_backup_time.tv_nsec = smb_vattrp->va_backup_time.tv_nsec;
            VATTR_SET_SUPPORTED(vap, va_backup_time);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_backup_time for <%s> \n", np->n_name);
        }
    }

    /* file parameters but used by non files too */
    if (VATTR_IS_ACTIVE(vap, va_fileid)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_fileid)) {
            VATTR_RETURN(vap, va_fileid, smb_vattrp->va_fileid);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_fileid for <%s> \n", np->n_name);
        }
    }

    if (VATTR_IS_ACTIVE(vap, va_linkid)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_linkid)) {
            VATTR_RETURN(vap, va_linkid, smb_vattrp->va_linkid);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_linkid for <%s> \n", np->n_name);
        }
    }

    if (VATTR_IS_ACTIVE(vap, va_parentid)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_parentid)) {
            VATTR_RETURN(vap, va_parentid, smb_vattrp->va_parentid);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_parentid for <%s> \n", np->n_name);
        }
    }

    /* Return SMB Vol value */
    if (VATTR_IS_ACTIVE(vap, va_fsid)) {
        VATTR_RETURN(vap, va_fsid, vfs_statfs(vnode_mount(vp))->f_fsid.val[0]);
    }

    if (VATTR_IS_ACTIVE(vap, va_filerev)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_filerev)) {
            VATTR_RETURN(vap, va_filerev, smb_vattrp->va_filerev);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_filerev for <%s> \n", np->n_name);
        }
    }

    if (VATTR_IS_ACTIVE(vap, va_gen)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_gen)) {
            VATTR_RETURN(vap, va_gen, smb_vattrp->va_gen);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_gen for <%s> \n", np->n_name);
        }
    }

    /* misc parameters */
    if (VATTR_IS_ACTIVE(vap, va_encoding)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_encoding)) {
            VATTR_RETURN(vap, va_encoding, smb_vattrp->va_encoding);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_encoding for <%s> \n", np->n_name);
        }
    }

    if (VATTR_IS_ACTIVE(vap, va_type)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_type)) {
            VATTR_RETURN(vap, va_type, smb_vattrp->va_type);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_type for <%s> \n", np->n_name);
        }
    }

    /*
     * If this is the root, let VFS find out the mount name, which may be
     * different from the real name
     */
    if (VATTR_IS_ACTIVE(vap, va_name) && !vnode_isvroot(vp)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_name)) {
            strlcpy ((char*) vap->va_name, (char*)smb_vattrp->va_name, MAXPATHLEN);
            VATTR_SET_SUPPORTED(vap, va_name);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_name for <%s> \n", np->n_name);
        }
    }

    /* va_uuuid and va_guuid are filled in with va_acl earlier */

    /* Meaningful for directories only */
    if (isDir == 1) {
        if (VATTR_IS_ACTIVE(vap, va_nchildren)) {
            if (VATTR_IS_SUPPORTED(smb_vattrp, va_nchildren)) {
                VATTR_RETURN(vap, va_nchildren, smb_vattrp->va_nchildren);
            }
            else {
                SMBERROR_LOCK(np, "Missing va_nchildren for <%s> \n", np->n_name);
            }
        }

        if (VATTR_IS_ACTIVE(vap, va_dirlinkcount)) {
            if (VATTR_IS_SUPPORTED(smb_vattrp, va_dirlinkcount)) {
                VATTR_RETURN(vap, va_dirlinkcount, smb_vattrp->va_dirlinkcount);
            }
            else {
                SMBERROR_LOCK(np, "Missing va_dirlinkcount for <%s> \n", np->n_name);
            }
        }
    }

    if (VATTR_IS_ACTIVE(vap, va_addedtime)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_addedtime)) {
            vap->va_addedtime.tv_sec = smb_vattrp->va_addedtime.tv_sec;
            vap->va_addedtime.tv_nsec = smb_vattrp->va_addedtime.tv_nsec;
            VATTR_SET_SUPPORTED(vap, va_addedtime);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_addedtime for <%s> \n", np->n_name);
        }
    }

    /* Data Protection fields */
    if (VATTR_IS_ACTIVE(vap, va_dataprotect_class)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_dataprotect_class)) {
            VATTR_RETURN(vap, va_dataprotect_class, smb_vattrp->va_dataprotect_class);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_dataprotect_class for <%s> \n", np->n_name);
        }
    }

    if (VATTR_IS_ACTIVE(vap, va_dataprotect_flags)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_dataprotect_flags)) {
            VATTR_RETURN(vap, va_dataprotect_flags, smb_vattrp->va_dataprotect_flags);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_dataprotect_flags for <%s> \n", np->n_name);
        }
    }

    /* Document revision tracking */
    if (VATTR_IS_ACTIVE(vap, va_document_id)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_document_id)) {
            VATTR_RETURN(vap, va_document_id, smb_vattrp->va_document_id);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_document_id for <%s> \n", np->n_name);
        }
    }

    /* Fields for Bulk args */
    if (VATTR_IS_ACTIVE(vap, va_devid)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_devid)) {
            VATTR_RETURN(vap, va_devid, smb_vattrp->va_devid);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_devid for <%s> \n", np->n_name);
        }
    }

    if (VATTR_IS_ACTIVE(vap, va_objtype)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_objtype)) {
            VATTR_RETURN(vap, va_objtype, smb_vattrp->va_objtype);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_objtype for <%s> \n", np->n_name);
        }
    }

    if (VATTR_IS_ACTIVE(vap, va_objtag)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_objtag)) {
            VATTR_RETURN(vap, va_objtag, smb_vattrp->va_objtag);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_objtag for <%s> \n", np->n_name);
        }
    }

    if (VATTR_IS_ACTIVE(vap, va_user_access)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_user_access)) {
            VATTR_RETURN(vap, va_user_access, smb_vattrp->va_user_access);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_user_access for <%s> \n", np->n_name);
        }
    }

    if (VATTR_IS_ACTIVE(vap, va_finderinfo)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_finderinfo)) {
            bcopy(smb_vattrp->va_finderinfo, vap->va_finderinfo,
                  sizeof(u_int8_t) * 32);
            VATTR_SET_SUPPORTED(vap, va_finderinfo);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_finderinfo for <%s> \n", np->n_name);
        }
    }

    if (VATTR_IS_ACTIVE(vap, va_rsrc_length)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_rsrc_length)) {
            VATTR_RETURN(vap, va_rsrc_length, smb_vattrp->va_rsrc_length);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_rsrc_length for <%s> \n", np->n_name);
        }
    }

    if (VATTR_IS_ACTIVE(vap, va_rsrc_alloc)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_rsrc_alloc)) {
            VATTR_RETURN(vap, va_rsrc_alloc, smb_vattrp->va_rsrc_alloc);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_rsrc_alloc for <%s> \n", np->n_name);
        }
    }

    /* Return SMB Vol value */
    if (VATTR_IS_ACTIVE(vap, va_fsid64)) {
        /* Copy AFP Client behavior */
        memcpy(&vap->va_fsid64, &(vfs_statfs(smp->sm_mp)->f_fsid), sizeof(vap->va_fsid64));
        VATTR_SET_SUPPORTED(vap, va_fsid64);
    }

    if (VATTR_IS_ACTIVE(vap, va_write_gencount)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_write_gencount)) {
            VATTR_RETURN(vap, va_write_gencount, smb_vattrp->va_write_gencount);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_write_gencount for <%s> \n", np->n_name);
        }
    }

    if (VATTR_IS_ACTIVE(vap, va_private_size)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_private_size)) {
            VATTR_RETURN(vap, va_private_size, smb_vattrp->va_private_size);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_private_size for <%s> \n", np->n_name);
        }
    }

    if (VATTR_IS_ACTIVE(vap, va_clone_id)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_clone_id)) {
            VATTR_RETURN(vap, va_clone_id, smb_vattrp->va_clone_id);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_clone_id for <%s> \n", np->n_name);
        }
    }

    if (VATTR_IS_ACTIVE(vap, va_extflags)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_extflags)) {
            VATTR_RETURN(vap, va_extflags, smb_vattrp->va_extflags);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_extflags for <%s> \n", np->n_name);
        }
    }

    if (VATTR_IS_ACTIVE(vap, va_recursive_gencount)) {
        if (VATTR_IS_SUPPORTED(smb_vattrp, va_recursive_gencount)) {
            VATTR_RETURN(vap, va_recursive_gencount, smb_vattrp->va_recursive_gencount);
        }
        else {
            SMBERROR_LOCK(np, "Missing va_recursive_gencount for <%s> \n", np->n_name);
        }
    }

    /* Cache the attribute data from the server */
    memcpy(&np->n_hifi_attrs, smb_vattrp, sizeof(np->n_hifi_attrs));

    error = 0;

    return(error);
}

/*
 * smbfs_getattr call from vfs.
 *
 * The calling routine must hold a reference on the share
 *
 */
int
smbfs_getattr(struct smb_share *share, vnode_t vp, struct vnode_attr *vap, 
			  vfs_context_t context)
{
	struct smbnode *np = VTOSMB(vp);
	int error = EINVAL;
    uint8_t *bufferp = NULL;
    uio_t hifi_uio = NULL;
    size_t actual_read_size = 0;
    uint64_t *versionp = NULL;
    uint64_t *flagsp = NULL;
    struct smb_vnode_attr *smb_vattrp = NULL;
    time_t attrtimeo;
    struct timespec ts;

    if (SS_TO_SESSION(share)->session_misc_flags & SMBV_MNT_HIGH_FIDELITY) {
        /*
         * High Fidelity mode
         * Get vnode attributes directly from server.
         */

        /* Has the meta data cache expired? */
        SMB_CACHE_TIME(ts, np, attrtimeo);
        if ((np->n_hifi_attrs.va_supported == 0) ||
            ((ts.tv_sec - np->attribute_cache_timer) > attrtimeo)) {
            /*
             * Meta data cache has timed out. Need to read attributes from
             * server again.
             *
             * Need a buffer to read attr data into
             */
            SMB_MALLOC(bufferp, uint8_t *, XATTR_HIGH_FIDELITY_LEN,
                       M_SMBFSDATA, M_WAITOK | M_ZERO);
            if (bufferp == NULL) {
                SMBERROR("Malloc failed for hifi %d\n", XATTR_HIGH_FIDELITY_LEN);
                error = ENOMEM;
                goto bad;
            }

            /* Set up uio to point to the read buffer */
            hifi_uio = uio_create(1, 0, UIO_SYSSPACE, UIO_READ);
            if (hifi_uio == NULL) {
                SMBERROR("uio_create failed for hifi \n");
                error = ENOMEM;
                goto bad;
            }

            error = uio_addiov(hifi_uio, CAST_USER_ADDR_T(bufferp),
                               XATTR_HIGH_FIDELITY_LEN);
            if (error) {
                SMBERROR("uio_addiov failed for hifi \n");
                error = ENOMEM;
                goto bad;
            }

            uio_setoffset(hifi_uio, 0);

            /* Do compound Create/Read/Close to get the attrs from server */
            error = smbfs_smb_cmpd_create_read_close(share, np,
                                                     NULL, 0,
                                                     XATTR_HIGH_FIDELITY_NAME, strlen(XATTR_HIGH_FIDELITY_NAME),
                                                     hifi_uio, &actual_read_size,
                                                     NULL,
                                                     context);
            if (error) {
                SMBERROR_LOCK(np, "smbfs_smb_cmpd_create_read_close failed %d for hifi on %s \n",
                              error, np->n_name);
                goto bad;
            }

            /* Verify that we received enough attr data */
             if (actual_read_size < XATTR_HIGH_FIDELITY_LEN) {
                 SMBERROR("buffer too small %zu < %d \n",
                          actual_read_size, XATTR_HIGH_FIDELITY_LEN);
                 error = EINVAL;
                 goto bad;
             }

             /* Check version */
             versionp = (uint64_t *) bufferp;
             if (*versionp != XATTR_HIGH_FIDELITY_VERSION) {
                 SMBERROR("Incompatible version %llu != %d \n",
                          *versionp, XATTR_HIGH_FIDELITY_VERSION);
                 error = EINVAL;
                 goto bad;
             }

             /* Check flags */
             flagsp = (uint64_t *) bufferp;
             if (!(*flagsp & SMB_HIFI_HAS_XATTR)) {
                 np->n_fstatus |= kNO_SUBSTREAMS;
             }

            /* Update the meta data cache timer */
            nanouptime(&ts);
            np->attribute_cache_timer = ts.tv_sec;

            /* Cast buffer to smb_vnode_attr, skipping over version and flags */
            smb_vattrp = (struct smb_vnode_attr *) &bufferp[16];
        }
        else {
            /* Meta data cache is still valid */
            smb_vattrp = &np->n_hifi_attrs;
        }

        /* Parse smb_vnode_attr from bufferp into vap to be returned */
        error = smbfs_unpack_smb_vap(vp, vap, smb_vattrp);
        if (error) {
            SMBERROR_LOCK(np, "smbfs_unpack_smb_vap failed %d on %s \n",
                          error, np->n_name);
            goto bad;
        }
    }
    else {
        /*
         * If not in High Fidelity mode, then check ACLs and reparse points
         */
        if (share->ss_attributes & FILE_PERSISTENT_ACLS &&
            (VATTR_IS_ACTIVE(vap, va_acl) || VATTR_IS_ACTIVE(vap, va_guuid) ||
             VATTR_IS_ACTIVE(vap, va_uuuid))) {
                DBG_ASSERT(!vnode_isnamedstream(vp));
                (void)smbfs_getsecurity(share, VTOSMB(vp), vap, context);
        }

        error = smbfs_update_cache(share, vp, vap, context);
    }

bad:
    if (bufferp != NULL) {
        SMB_FREE(bufferp, M_TEMP);
        bufferp = NULL;
    }

    if (hifi_uio != NULL) {
        uio_free(hifi_uio);
        hifi_uio = NULL;
    }

    return(error);
}

/*
 * smbfs_vnop_getattr
 *
 * vnode_t	a_vp;
 * struct vnode_attr *a_vap;
 * vfs_context_t a_context;
 */
static int
smbfs_vnop_getattr(struct vnop_getattr_args *ap)
{
	int32_t error = 0;
	struct smb_share *share;
	struct smbnode *np;
    vnode_t vp = ap->a_vp;

	if ((error = smbnode_lock(VTOSMB(vp), SMBFS_SHARED_LOCK))) {
		return (error);
	}

	SMB_LOG_KTRACE(SMB_DBG_GET_ATTR | DBG_FUNC_START, 0, 0, 0, 0, 0);

    np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_getattr;
	share = smb_get_share_with_reference(VTOSMBFS(vp));

    if (!(SS_TO_SESSION(share)->session_misc_flags & SMBV_MNT_HIGH_FIDELITY) &&
        (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) &&
        (np->n_dosattr & SMB_EFA_REPARSE_POINT) &&
        (np->n_reparse_tag == IO_REPARSE_TAG_SYMLINK) &&
        (VATTR_IS_ACTIVE(ap->a_vap, va_total_size) ||
         VATTR_IS_ACTIVE(ap->a_vap, va_total_alloc) ||
         VATTR_IS_ACTIVE(ap->a_vap, va_data_size) ||
         VATTR_IS_ACTIVE(ap->a_vap, va_data_alloc))) {
        /*
         * If its not HiFi and its SMB v2/3 and its a symlink and they
         * want the size, then we need an exclusive lock so we can read in the
         * link and cache it to determine its size.
         */

        /* Drop the shared lock */
        smbnode_unlock(np);

        /* Get an exclusive lock */
        if ((error = smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK))) {
            return (error);
        }

        /* Best effort to get the symlink info */
        error = smbfs_readlink(share, vp, NULL, ap->a_context);
        if (error) {
            /*
             * If we got an error, then log it and ignore it as it just means
             * that we might return the wrong size for the symlink.
             */
            SMBERROR_LOCK(np, "Failed %d to get reparse point data for <%s> \n",
                          error, np->n_name);
        }

        /* Unlock the exclusive lock */
        smbnode_unlock(VTOSMB(vp));

        /* And get the shared lock again */
        if ((error = smbnode_lock(VTOSMB(vp), SMBFS_SHARED_LOCK))) {
            return (error);
        }
    }

    /* Before updating see if it needs to be reopened. */
	if ((!vnode_isdir(vp)) && (np->f_openState & kNeedReopen)) {
		/* smbfs_smb_reopen_file should check to see if the share changed? */
		(void)smbfs_smb_reopen_file(share, np, ap->a_context);
	}
    
    /* Is meta data caching disabled? */
    if (SS_TO_SESSION(share)->session_misc_flags & SMBV_MNT_MDATACACHE_OFF) {
        /* Don't use cached meta data */
        np->attribute_cache_timer = 0;
    }

	error = smbfs_getattr(share, vp, ap->a_vap, ap->a_context);
	smb_share_rele(share, ap->a_context);
	smbnode_unlock(np);
    
    SMB_LOG_KTRACE(SMB_DBG_GET_ATTR | DBG_FUNC_END, error, 0, 0, 0, 0);
	return (error);
}

/*
 * The number of seconds between Jan 1, 1970 and Jan 1, 1980. In that
 * interval there were 8 regular years and 2 leap years.
 */
#define	SECONDSTO1980	(((8 * 365) + (2 * 366)) * (24 * 60 * 60))
static struct timespec fat1980_time = {SECONDSTO1980, 0};

/*
 * smbfs_hifi_set_perms
 *
 * <69977384> When the HiFi server is running in Super Guest mode, then any
 * time an item is created on the server, its owned by root. To fix this,
 * the client will attempt to set the uid/gid to be the same as the mounting
 * user after every create.
 *
 * Assumptions:
 *      1. HiFi server is a macOS server
 *      2. The uid/gid of the mounting user is the same uid/gid on server
 *
 * The calling routine must hold a reference on the share
 */
int
smbfs_hifi_set_perms(struct smb_share *share, vnode_t vp, vfs_context_t context)
{
    struct vnode_attr vap = {0};
    struct smbmount *smp = NULL;
    int error = 0;
    struct smb_session *sessionp = NULL;


    if (vp == NULL) {
        SMBERROR("vp is NULL? \n");
        return(EINVAL);
    }
    smp = VTOSMB(vp)->n_mount;

    if (smp == NULL) {
        SMBERROR("smp is NULL? \n");
        return(EINVAL);
    }

    if (share == NULL) {
        SMBERROR("share is NULL? \n");
        return(EINVAL);
    }
    sessionp = SS_TO_SESSION(share);

    if ((sessionp->session_flags & SMBV_SFS_ACCESS) &&
        (sessionp->session_misc_flags & SMBV_MNT_HIGH_FIDELITY)) {
        VATTR_INIT(&vap);

        VATTR_SET_ACTIVE(&vap, va_uid);
        vap.va_uid = smp->sm_args.uid;

        VATTR_SET_ACTIVE(&vap, va_gid);
        vap.va_gid = smp->sm_args.gid;

        error = smbfs_setattr(share, vp, &vap, context);
        if (error) {
            SMBERROR_LOCK(VTOSMB(vp), "smbfs_setattr failed <%d> on <%s> \n",
                          error, VTOSMB(vp)->n_name);
        }
    }
    else {
        /* Nothing to do */
        error = 0;
    }

    return(error);
}

/*
 * The calling routine must hold a reference on the share
 */
static int
smbfs_pack_smb_vap(vnode_t vp, struct vnode_attr *vap,
                   uint8_t *bufferp)
{
    #pragma unused(vp)
    int error = EINVAL;
    uint64_t *versionp = NULL;
    uint64_t *flagsp = NULL;
    struct smb_vnode_attr *smb_vattrp = NULL;

    /* Initial checks */
    if ((vap == NULL) || (bufferp == NULL)) {
        SMBERROR("vap or bufferp is null \n ");
        error = EINVAL;
        goto bad;
    }

    /* Set version */
    versionp = (uint64_t *) bufferp;
    *versionp = XATTR_HIGH_FIDELITY_VERSION;

    /* Set flags */
    flagsp = (uint64_t *) &bufferp[8];
    *flagsp = 0;

    /* Cast buffer to smb_vnode_attr, skipping over version */
    smb_vattrp = (struct smb_vnode_attr *) &bufferp[16];

    /*
     * If they are setting the attr, copy it directly into smb_vattrp
     */

    /* traditional stat(2) parameter fields */
    if (VATTR_IS_ACTIVE(vap, va_rdev)) {
        /* Set value and set attr as active in smb_vattrp */
        VATTR_SET(smb_vattrp, va_rdev, vap->va_rdev);
        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_rdev);
    }

    if (VATTR_IS_ACTIVE(vap, va_nlink)) {
        /* Set value and set attr as active in smb_vattrp */
        VATTR_SET(smb_vattrp, va_nlink, vap->va_nlink);
        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_nlink);
    }

    if (VATTR_IS_ACTIVE(vap, va_total_size)) {
        /* Set value and set attr as active in smb_vattrp */
        VATTR_SET(smb_vattrp, va_total_size, vap->va_total_size);
        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_total_size);
    }

    if (VATTR_IS_ACTIVE(vap, va_total_alloc)) {
        /* Set value and set attr as active in smb_vattrp */
        VATTR_SET(smb_vattrp, va_total_alloc, vap->va_total_alloc);
        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_total_alloc);
    }

    /* va_data_size was already handled before this function */

    if (VATTR_IS_ACTIVE(vap, va_data_alloc)) {
        /* Set value and set attr as active in smb_vattrp */
        VATTR_SET(smb_vattrp, va_data_alloc, vap->va_data_alloc);
        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_data_alloc);
    }

    if (VATTR_IS_ACTIVE(vap, va_iosize)) {
        /* Set value and set attr as active in smb_vattrp */
        VATTR_SET(smb_vattrp, va_iosize, vap->va_iosize);
        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_iosize);
    }

    /* file security information */
    if (VATTR_IS_ACTIVE(vap, va_uid)) {
        /* Set value and set attr as active in smb_vattrp */
        VATTR_SET(smb_vattrp, va_uid, vap->va_uid);
        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_uid);
    }

    if (VATTR_IS_ACTIVE(vap, va_gid)) {
        /* Set value and set attr as active in smb_vattrp */
        VATTR_SET(smb_vattrp, va_gid, vap->va_gid);
        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_gid);
    }

    if (VATTR_IS_ACTIVE(vap, va_mode)) {
        /* Set value and set attr as active in smb_vattrp */
        VATTR_SET(smb_vattrp, va_mode, vap->va_mode);
        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_mode);
    }

    if (VATTR_IS_ACTIVE(vap, va_flags)) {
        /* Set value and set attr as active in smb_vattrp */
        VATTR_SET(smb_vattrp, va_flags, vap->va_flags);
        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_flags);
    }

    /*
     * If va_acl is being set then dont return that we support va_acl, va_uuid,
     * or va_guuid. The VFS will then try to write the KAUTH_FILESEC_XATTR and
     * set those fields for us.
     */
    if (VATTR_IS_ACTIVE(vap, va_acl)) {
        /*
         * They want to set an ACL, do not return that we support va_acl,
         * va_uuid or va_guuid. Let vfs try to set the xattr instead and
         * set all of those values for us.
         */
    }
    else {
        /* If they only want to set va_uuid or va_guuid, we can do that */
        if (VATTR_IS_ACTIVE(vap, va_uuuid)) {
            /* Set value and set attr as active in smb_vattrp */
            memcpy(smb_vattrp->va_uuuid, &vap->va_uuuid,
                   sizeof(smb_vattrp->va_uuuid));
            VATTR_SET_ACTIVE(smb_vattrp, va_uuuid);

            /* Return that we support setting this attr */
            VATTR_SET_SUPPORTED(vap, va_uuuid);
        }

        if (VATTR_IS_ACTIVE(vap, va_guuid)) {
            /* Set value and set attr as active in smb_vattrp */
            memcpy(smb_vattrp->va_guuid, &vap->va_guuid,
                   sizeof(smb_vattrp->va_guuid));
            VATTR_SET_ACTIVE(smb_vattrp, va_guuid);

            /* Return that we support setting this attr */
            VATTR_SET_SUPPORTED(vap, va_guuid);
        }
    }

    /* timestamps */
    if (VATTR_IS_ACTIVE(vap, va_create_time)) {
        /* Set value and set attr as active in smb_vattrp */
        smb_vattrp->va_create_time.tv_sec = vap->va_create_time.tv_sec;
        smb_vattrp->va_create_time.tv_nsec = vap->va_create_time.tv_nsec;
        VATTR_SET_ACTIVE(smb_vattrp, va_create_time);

        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_create_time);
    }

    if (VATTR_IS_ACTIVE(vap, va_access_time)) {
        /* Set value and set attr as active in smb_vattrp */
        smb_vattrp->va_access_time.tv_sec = vap->va_access_time.tv_sec;
        smb_vattrp->va_access_time.tv_nsec = vap->va_access_time.tv_nsec;
        VATTR_SET_ACTIVE(smb_vattrp, va_access_time);

        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_access_time);
    }

    if (VATTR_IS_ACTIVE(vap, va_modify_time)) {
        /* Set value and set attr as active in smb_vattrp */
        smb_vattrp->va_modify_time.tv_sec = vap->va_modify_time.tv_sec;
        smb_vattrp->va_modify_time.tv_nsec = vap->va_modify_time.tv_nsec;
        VATTR_SET_ACTIVE(smb_vattrp, va_modify_time);

        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_modify_time);
    }

    if (VATTR_IS_ACTIVE(vap, va_change_time)) {
        /* Set value and set attr as active in smb_vattrp */
        smb_vattrp->va_change_time.tv_sec = vap->va_change_time.tv_sec;
        smb_vattrp->va_change_time.tv_nsec = vap->va_change_time.tv_nsec;
        VATTR_SET_ACTIVE(smb_vattrp, va_change_time);

        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_change_time);
    }

    if (VATTR_IS_ACTIVE(vap, va_backup_time)) {
        /* Set value and set attr as active in smb_vattrp */
        smb_vattrp->va_backup_time.tv_sec = vap->va_backup_time.tv_sec;
        smb_vattrp->va_backup_time.tv_nsec = vap->va_backup_time.tv_nsec;
        VATTR_SET_ACTIVE(smb_vattrp, va_backup_time);

        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_backup_time);
    }

    /* file parameters */
    if (VATTR_IS_ACTIVE(vap, va_fileid)) {
        /* Set value and set attr as active in smb_vattrp */
        VATTR_SET(smb_vattrp, va_fileid, vap->va_fileid);
        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_fileid);
    }

    if (VATTR_IS_ACTIVE(vap, va_linkid)) {
        /* Set value and set attr as active in smb_vattrp */
        VATTR_SET(smb_vattrp, va_linkid, vap->va_linkid);
        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_linkid);
    }

    if (VATTR_IS_ACTIVE(vap, va_parentid)) {
        /* Set value and set attr as active in smb_vattrp */
        VATTR_SET(smb_vattrp, va_parentid, vap->va_parentid);
        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_parentid);
    }

    /* Setting va_fsid is not allowed, just ignore it */

    if (VATTR_IS_ACTIVE(vap, va_filerev)) {
        /* Set value and set attr as active in smb_vattrp */
        VATTR_SET(smb_vattrp, va_filerev, vap->va_filerev);
        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_filerev);
    }

    if (VATTR_IS_ACTIVE(vap, va_gen)) {
        /* Set value and set attr as active in smb_vattrp */
        VATTR_SET(smb_vattrp, va_gen, vap->va_gen);
        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_gen);
    }

    /* misc parameters */
    if (VATTR_IS_ACTIVE(vap, va_encoding)) {
        /* Set value and set attr as active in smb_vattrp */
        VATTR_SET(smb_vattrp, va_encoding, vap->va_encoding);
        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_encoding);
    }

    /* Setting va_type is not allowed, just ignore it */

    /* Setting va_name is not allowed, just ignore it */

    /* va_uuuid and va_guuid are filled in with va_acl earlier */

    /* Meaningful for directories only */
    if (VATTR_IS_ACTIVE(vap, va_nchildren)) {
        /* Set value and set attr as active in smb_vattrp */
        VATTR_SET(smb_vattrp, va_nchildren, vap->va_nchildren);
        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_nchildren);
    }

    if (VATTR_IS_ACTIVE(vap, va_dirlinkcount)) {
        /* Set value and set attr as active in smb_vattrp */
        VATTR_SET(smb_vattrp, va_dirlinkcount, vap->va_dirlinkcount);
        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_dirlinkcount);
    }

    if (VATTR_IS_ACTIVE(vap, va_addedtime)) {
        /* Set value and set attr as active in smb_vattrp */
        smb_vattrp->va_addedtime.tv_sec = vap->va_addedtime.tv_sec;
        smb_vattrp->va_addedtime.tv_nsec = vap->va_addedtime.tv_nsec;
        VATTR_SET_ACTIVE(smb_vattrp, va_addedtime);

        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_addedtime);
    }

    /* Data Protection fields */
    if (VATTR_IS_ACTIVE(vap, va_dataprotect_class)) {
        /* Set value and set attr as active in smb_vattrp */
        VATTR_SET(smb_vattrp, va_dataprotect_class, vap->va_dataprotect_class);
        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_dataprotect_class);
    }

    if (VATTR_IS_ACTIVE(vap, va_dataprotect_flags)) {
        /* Set value and set attr as active in smb_vattrp */
        VATTR_SET(smb_vattrp, va_dataprotect_flags, vap->va_dataprotect_flags);
        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_dataprotect_flags);
    }

    /* Document revision tracking */
    if (VATTR_IS_ACTIVE(vap, va_document_id)) {
        /* Set value and set attr as active in smb_vattrp */
        VATTR_SET(smb_vattrp, va_document_id, vap->va_document_id);
        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_document_id);
    }

    /* Fields for Bulk args */
    if (VATTR_IS_ACTIVE(vap, va_devid)) {
        /* Set value and set attr as active in smb_vattrp */
        VATTR_SET(smb_vattrp, va_devid, vap->va_devid);
        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_devid);
    }

    if (VATTR_IS_ACTIVE(vap, va_objtype)) {
        /* Set value and set attr as active in smb_vattrp */
        VATTR_SET(smb_vattrp, va_objtype, vap->va_objtype);
        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_objtype);
    }

    if (VATTR_IS_ACTIVE(vap, va_objtag)) {
        /* Set value and set attr as active in smb_vattrp */
        VATTR_SET(smb_vattrp, va_objtag, vap->va_objtag);
        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_objtag);
    }

    if (VATTR_IS_ACTIVE(vap, va_user_access)) {
        /* Set value and set attr as active in smb_vattrp */
        VATTR_SET(smb_vattrp, va_user_access, vap->va_user_access);
        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_user_access);
    }

    if (VATTR_IS_ACTIVE(vap, va_finderinfo)) {
        /* Set value and set attr as active in smb_vattrp */
        bcopy(vap->va_finderinfo, smb_vattrp->va_finderinfo,
              sizeof(u_int8_t) * 32);
        VATTR_SET_ACTIVE(smb_vattrp, va_finderinfo);
        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_finderinfo);
    }

    if (VATTR_IS_ACTIVE(vap, va_rsrc_length)) {
        /* Set value and set attr as active in smb_vattrp */
        VATTR_SET(smb_vattrp, va_rsrc_length, vap->va_rsrc_length);
        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_rsrc_length);
    }

    if (VATTR_IS_ACTIVE(vap, va_rsrc_alloc)) {
        /* Set value and set attr as active in smb_vattrp */
        VATTR_SET(smb_vattrp, va_rsrc_alloc, vap->va_rsrc_alloc);
        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_rsrc_alloc);
    }

    /* You cant change the va_fsid64 */

    if (VATTR_IS_ACTIVE(vap, va_write_gencount)) {
        /* Set value and set attr as active in smb_vattrp */
        VATTR_SET(smb_vattrp, va_write_gencount, vap->va_write_gencount);
        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_write_gencount);
    }

    if (VATTR_IS_ACTIVE(vap, va_private_size)) {
        /* Set value and set attr as active in smb_vattrp */
        VATTR_SET(smb_vattrp, va_private_size, vap->va_private_size);
        /* Return that we support setting this attr */
        VATTR_SET_SUPPORTED(vap, va_private_size);
    }

    /* Setting va_clone_id is not allowed, just ignore it */

    /* Setting va_extflags is not allowed, just ignore it */

    /* Setting va_recursive_gencount is not allowed, just ignore it */

    error = 0;

bad:
    return(error);
}

static int
smbfs_set_data_size(struct smb_share *share, vnode_t vp, struct vnode_attr *vap,
                    int *modified, vfs_context_t context)
{
    struct smbnode *np = VTOSMB(vp);
    uint32_t rights;
    SMBFID fid = 0;
    int error = 0;
    u_quad_t tsize = 0;
    uint32_t trycnt = 0;

    ubc_msync (vp, 0, ubc_getsize(vp), NULL, UBC_PUSHDIRTY | UBC_SYNC);

    /* The seteof call requires the file to be opened for write and append data. */
    rights = SMB2_FILE_WRITE_DATA | SMB2_FILE_APPEND_DATA;

    /*
     * The connection could go down in the middle of setting the eof, in
     * this case we will get a bad file descriptor error. Keep trying until
     * the open fails or we get something besides EBADF.
     *
     * We now have a counter that keeps us from going into an infinite loop. Once
     * we implement SMB 2/3 we should take a second look at how this should be done.
     */
    do {
        error = smbfs_tmpopen(share, np, rights, &fid, context);
        if (error) {
            SMB_LOG_IO_LOCK(np, "%s seteof open failed %d\n",
                            np->n_name, error);
            /* The open failed, fail the seteof. */
            break;
        }

        /* zero fill if needed, ignore any errors will catch them on the seteof call */
        tsize = np->n_size;
        if (tsize < vap->va_data_size) {
            error = smbfs_0extend(share, fid, tsize, vap->va_data_size, 0, context);
            SMB_LOG_KTRACE(SMB_DBG_SMBFS_SETATTR | DBG_FUNC_NONE,
                           0xabc002, error, 0, 0, 0);
        }

        /* Set the eof on the server */
        if (!error) {
            error = smbfs_seteof(share, np, fid, vap->va_data_size, context);
            SMB_LOG_KTRACE(SMB_DBG_SMBFS_SETATTR | DBG_FUNC_NONE,
                           0xabc003, error, 0, 0, 0);
        }

        if (error == EBADF) {
            trycnt++;
            SMB_LOG_IO_LOCK(np, "%s seteof failed because of reconnect, trying again.\n",
                            np->n_name);
        }

        /*
         * Windows FAT file systems require a flush, after a seteof. Until the
         * flush, they will keep returning the old file size.
         */
        if ((!error) && (share->ss_fstype == SMB_FS_FAT) &&
            (!UNIX_SERVER(SS_TO_SESSION(share)))) {
            error = smbfs_smb_flush(share, fid, 0, context);
            if (!error) {
                np->n_flag &= ~NNEEDS_FLUSH;
            }
        }

        /* We ignore errors on close, not much we can do about it here */
        (void)smbfs_tmpclose(share, np, fid, context);
    } while ((error == EBADF) && (trycnt < SMB_MAX_REOPEN_CNT));

    SMB_LOG_IO_LOCK(np, "%s: Calling smbfs_setsize, old eof = %lld  new eof = %lld\n",
                    np->n_name, tsize, vap->va_data_size);

    if (error) {
        /* If we failed, try to restore previous size in UBC and leave */
        smbfs_setsize(vp, (off_t)tsize);
        goto out;
    }
    else {
        /* Set new size in UBC */
        smbfs_setsize(vp, (off_t)vap->va_data_size);
    }
    SMB_LOG_KTRACE(SMB_DBG_SMBFS_SETATTR | DBG_FUNC_NONE,
                   0xabc004, error, 0, 0, 0);

    /* Successfully changed the data size */
    VATTR_SET_SUPPORTED(vap, va_data_size);

    /* Tell the stream's parent that something has changed */
    if (vnode_isnamedstream(vp)) {
        vnode_t parent_vp = smb_update_rsrc_and_getparent(vp, TRUE);
        /*
         * We cannot always update the parents meta cache timer, so don't
         * even try here
         */
        if (parent_vp) {
            vnode_put(parent_vp);
        }
    }

    /* Return that we did modify the data size */
    *modified = 1;

out:
    return(error);
}

/*
* The calling routine must hold a reference on the share
*/
static int 
smbfs_setattr(struct smb_share *share, vnode_t vp, struct vnode_attr *vap, 
			  vfs_context_t context)
{
	struct smbnode *np = VTOSMB(vp);
	struct smbmount *smp = VTOSMBFS(vp);
	struct timespec *crtime, *mtime, *atime;
	int error = 0, cerror, modified = 0;
    SMBFID fid = 0;
	uint32_t rights;
	Boolean useFatTimes = (share->ss_fstype == SMB_FS_FAT);
    enum vtype vnode_type = VREG;
    uint32_t forceInvalidate = 0;
    uint8_t *bufferp = NULL;
    uio_t hifi_uio = NULL;
    uint64_t create_flags = 0;
    struct smb_vnode_attr *smb_vattrp = NULL;

    SMB_LOG_KTRACE(SMB_DBG_SMBFS_SETATTR | DBG_FUNC_START, 0, 0, 0, 0, 0);

    if (np == NULL) {
        SMBERROR("Null np \n");
        error = EINVAL;
        goto out;
    }

    if ((np) && (np->n_vnode)) {
        /* Use vnode to determine type */
        vnode_type = vnode_isdir(np->n_vnode) ? VDIR : VREG;
    }
    
	/* If this is a stream then they can only set the size */
	if ((vnode_isnamedstream(vp)) && 
		(vap->va_active & ~VNODE_ATTR_BIT(va_data_size))) {
		SMBDEBUG_LOCK(np, "Using stream node %s to set something besides the size?\n",
                      np->n_name);
		error = ENOTSUP;
		goto out;
	}
	
    if (SS_TO_SESSION(share)->session_misc_flags & SMBV_MNT_HIGH_FIDELITY) {
        /*
         * High Fidelity mode
         * Set vnode attributes directly to server.
         */

        /* Assume that something will change and invalidate meta data cache */
        np->attribute_cache_timer = 0;

        /*
         * Setting the va_data_size has to be done by the client
         */
        if (VATTR_IS_ACTIVE(vap, va_data_size) && (vnode_isreg(vp))) {
            error = smbfs_set_data_size(share, vp, vap, &modified, context);
            if (error) {
                goto out;
            }
        }

        /* Need a buffer to write attr data into */
        SMB_MALLOC(bufferp, uint8_t *, XATTR_HIGH_FIDELITY_LEN,
                   M_SMBFSDATA, M_WAITOK | M_ZERO);
        if (bufferp == NULL) {
            SMBERROR("Malloc failed for hifi %d\n", XATTR_HIGH_FIDELITY_LEN);
            error = ENOMEM;
            goto out;
        }

        /* Pack vap attrs into smb_vnode_attr in bufferp to send to server */
        error = smbfs_pack_smb_vap(vp, vap, bufferp);
        if (error) {
            SMBERROR_LOCK(np, "smbfs_pack_smb_vap failed %d on %s \n",
                          error, np->n_name);
            goto out;
        }

        /*
         * Check to see if there is anything to set on the server.
         * For example, if its just an ACL, then there would be nothing to set
         * via the smb_vnode_attr.
         *
         * Cast buffer to smb_vnode_attr, skipping over version
         */
        smb_vattrp = (struct smb_vnode_attr *) &bufferp[16];
        if (smb_vattrp->va_active == 0) {
            /* Nothing to write to server */
            goto out;
        }

        /* Set up uio to point to the write buffer */
        hifi_uio = uio_create(1, 0, UIO_SYSSPACE, UIO_WRITE);
        if (hifi_uio == NULL) {
            SMBERROR("uio_create failed for hifi \n");
            error = ENOMEM;
            goto out;
        }

        error = uio_addiov(hifi_uio, CAST_USER_ADDR_T(bufferp),
                           XATTR_HIGH_FIDELITY_LEN);
        if (error) {
            SMBERROR("uio_addiov failed for hifi \n");
            error = ENOMEM;
            goto out;
        }

        uio_setoffset(hifi_uio, 0);

        /* Do compound Create/Write/Close to set the attrs on server */
        error = smb2fs_smb_cmpd_create_write(share, np,
                                             NULL, 0,
                                             XATTR_HIGH_FIDELITY_NAME, strlen(XATTR_HIGH_FIDELITY_NAME),
                                             SMB2_FILE_WRITE_DATA, NULL,
                                             create_flags, hifi_uio,
                                             NULL, 0,
                                             context);
        if (error) {
            SMBERROR_LOCK(np, "smb2fs_smb_cmpd_create_write failed %d for hifi on %s \n",
                          error, np->n_name);
            goto out;
        }

        goto out;
    }

    /*
     * Not in High Fidelity mode
     */

    /*
	 * If our caller is trying to set multiple attributes, they
	 * can make no assumption about what order they are done in.
	 * Here we try to do them in order of decreasing likelihood
	 * of failure, just to minimize the chance we'll wind up
	 * with a partially complete request.
	 */

	if (share->ss_attributes & FILE_PERSISTENT_ACLS &&
	    (VATTR_IS_ACTIVE(vap, va_acl) || VATTR_IS_ACTIVE(vap, va_guuid) ||
	     VATTR_IS_ACTIVE(vap, va_uuuid))) {
		error = smbfs_setsecurity(share, vp, vap, context);
        SMB_LOG_KTRACE(SMB_DBG_SMBFS_SETATTR | DBG_FUNC_NONE,
                       0xabc001, error, 0, 0, 0);
		if (error)
			goto out;
		/*
		 * Failing to set VATTR_SET_SUPPORTED to something which was
		 * requested causes fallback to EAs, which we never want. This
		 * is done because of HFS, so if you support it you have to
		 * always return something, even if its wrong.
		 */
		if (VATTR_IS_ACTIVE(vap, va_acl))
			VATTR_SET_SUPPORTED(vap, va_acl);
		if (VATTR_IS_ACTIVE(vap, va_guuid))
			VATTR_SET_SUPPORTED(vap, va_guuid);
		if (VATTR_IS_ACTIVE(vap, va_uuuid))
			VATTR_SET_SUPPORTED(vap, va_uuuid);
		modified = 1;

        /* Changing security does not generate a par dir lease break */
        forceInvalidate = 1;
	}
		
	/*
	 * If the server supports the new UNIX extensions, then we can support 
	 * changing the uid, gid, mode, and va_flags. Currently the uid and gid 
	 * don't make any sense, but in the future we may add this support.
	 *
	 * The old code would check the users creditials here. There is no need for 
	 * that in our case. The lower level will make sure the correct local user 
	 * is using the session and the server should protect us for any other case.
	 */

	if ((VATTR_IS_ACTIVE(vap, va_mode)) || (VATTR_IS_ACTIVE(vap, va_flags))) {
		int supportUnixBSDFlags = ((UNIX_CAPS(share) & UNIX_SFILEINFO_UNIX_INFO2_CAP)) ? TRUE : FALSE;
		int supportUnixInfo2 = ((UNIX_CAPS(share) & UNIX_QFILEINFO_UNIX_INFO2_CAP)) ? TRUE : FALSE;
		int darwin = (SS_TO_SESSION(share)->session_flags & SMBV_DARWIN) ? TRUE : FALSE;
		int dosattr = np->n_dosattr;
		uint32_t vaflags = 0;
		uint32_t vaflags_mask = SMB_FLAGS_NO_CHANGE;
		uint64_t vamode = SMB_MODE_NO_CHANGE;
		
		if (VATTR_IS_ACTIVE(vap, va_flags)) {
			/*
			 * Here we are strict, stricter than ufs in not allowing users to  
			 * attempt to set SF_SETTABLE bits or anyone to set unsupported bits.   
			 * However, we ignore attempts to set ATTR_ARCHIVE for directories 
			 * `cp -pr' from a more sensible file system attempts it a lot.
			 */
			if (vap->va_flags & ~(SF_ARCHIVED | SF_IMMUTABLE | UF_IMMUTABLE | UF_HIDDEN))
			{
				error = EINVAL;
				goto out;
			}
			/* Only set items we can change and the server supports */
			vaflags_mask = np->n_flags_mask & EXT_REQUIRED_BY_MAC;

			/*
			 * Remember that SMB_EFA_ARCHIVE means the items needs to be 
			 * archive and SF_ARCHIVED means the item has been archive.
			 */
			if (vap->va_flags & SF_ARCHIVED) {
				dosattr &= ~SMB_EFA_ARCHIVE;				
				vaflags |= EXT_DO_NOT_BACKUP;
			} else {
				dosattr |= SMB_EFA_ARCHIVE;	
			}
			/*
			 * SMB_EFA_RDONLY ~ UF_IMMUTABLE
			 *
			 * We treat the SMB_EFA_RDONLY as the immutable flag. This allows
			 * us to support the finder lock bit and makes us follow the 
			 * MSDOS code model. See msdosfs project.
			 *
			 * NOTE: The ready-only flags does not exactly follow the 
			 * lock/immutable bit also note the for directories its advisory only.
			 *
			 * We do not support the setting the read-only bit for folders if 
			 * the server does not support the new UNIX extensions.
			 * 
			 * See Radar 5582956 for more details.
			 */
            if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
                if (UNIX_SERVER(SS_TO_SESSION(share)) || (!vnode_isdir(vp))) {
                    if (vap->va_flags & (SF_IMMUTABLE | UF_IMMUTABLE)) {
                        dosattr |= SMB_EFA_RDONLY;
                        vaflags |= EXT_IMMUTABLE;
                    }
                    else {
                        dosattr &= ~SMB_EFA_RDONLY;
                    }
                }
           }
            else {
                if (supportUnixBSDFlags || darwin || (!vnode_isdir(vp))) {
                    if (vap->va_flags & (SF_IMMUTABLE | UF_IMMUTABLE)) {
                        dosattr |= SMB_EFA_RDONLY;				
                        vaflags |= EXT_IMMUTABLE;				
                    }
                    else {
                        dosattr &= ~SMB_EFA_RDONLY;
                    }
                }
            }
            
			/*
			 * NOTE: Windows does not set ATTR_ARCHIVE bit for directories. 
			 */
			if ((! supportUnixBSDFlags) && (vnode_isdir(vp)))
				dosattr &= ~SMB_EFA_ARCHIVE;
			
			/* Now deal with the new Hidden bit */
			if (vap->va_flags & UF_HIDDEN) {
				dosattr |= SMB_EFA_HIDDEN;				
				vaflags |= EXT_HIDDEN;
			} else {
				dosattr &= ~SMB_EFA_HIDDEN;
			}
		}

		/* 
		 * Currently we do not allow setting the uid, gid, or sticky bits. Also 
		 * chmod on a symbolic link doesn't really make sense. BSD allows this 
		 * with the lchmod and on create, but Samba doesn't support this because 
		 * its not POSIX. If we try to chmod here it will get set on the target 
		 * which would be bad. So ignore the fact that they made this request.
		 */
		if (VATTR_IS_ACTIVE(vap, va_mode)) {
			if (supportUnixInfo2 && (!vnode_islnk(vp))) {
				vamode = vap->va_mode & ACCESSPERMS;
			} else if ((share->ss_attributes & FILE_PERSISTENT_ACLS) &&
					   (darwin || !UNIX_CAPS(share))) {
				vamode = vap->va_mode & ACCESSPERMS;
			} else if (SS_TO_SESSION(share)->session_server_caps & kAAPL_SUPPORTS_NFS_ACE) {
                /*
                 * For OS X <-> OS X PFS (where ACLs are off by default), we 
                 * need a way to set Posix permissions. If the server supports
                 * the NFS ACE, then it will allow us to get the ACL and send
                 * it back with the desired Posix permissions in the NFS ACE.
                 */
				vamode = vap->va_mode & ACCESSPERMS;
            }

		}
		
		if (dosattr == np->n_dosattr) {
			vaflags_mask = 0; /* Nothing really changes, no need to make the call */ 
		}
		
		if (vaflags_mask || (vamode != SMB_MODE_NO_CHANGE)) {
			if (!supportUnixInfo2) {
				/* Windows style server */
				if (vaflags_mask) {
					error = smbfs_smb_setpattr(share, np, vnode_type,
                                               NULL, 0,
                                               dosattr, context);
				}
				if (vamode != SMB_MODE_NO_CHANGE) {
					/* Should we report the error? */
					error = smbfs_set_ace_modes(share, np, vamode, context);
				}
			} else if (supportUnixBSDFlags) {
				/* Samba server that does support the BSD flags (Mac OS) */
				error = smbfs_set_unix_info2(share, np, NULL, NULL, NULL, 
											 SMB_SIZE_NO_CHANGE,  vamode, vaflags, 
											 vaflags_mask, context);
			} else {				
				/* Samba server that doesn't support the BSD flags, Linux */
				if (vamode != SMB_MODE_NO_CHANGE) {
					/* Now set the posix modes, using unix info level */
					error = smbfs_set_unix_info2(share, np, NULL, NULL, NULL, 
												 SMB_SIZE_NO_CHANGE, vamode,
												 SMB_FLAGS_NO_CHANGE, vaflags_mask, 
												 context);
				}
				if (vaflags_mask) {
					/* Set the BSD flags using normal smb info level  */
					error = smbfs_smb_setpattr(share, np, vnode_type,
                                               NULL, 0,
                                               dosattr, context);
				}
			}

			/* 
			 * We attempted to set flags or mode bits, see what we ended up
			 * with on the server whether it worked or failed.
			 */
			modified = 1;

            /* Changing security does not generate a par dir lease break */
            forceInvalidate = 1;

            if (error) {
				goto out;
            }
		}
		/* Everything work update the local cache and mark that we did the work */
		if (VATTR_IS_ACTIVE(vap, va_mode)) {
	    	if (vamode != SMB_MODE_NO_CHANGE) {
				np->n_mode = vamode;
				VATTR_SET_SUPPORTED(vap, va_mode);
			}
		}
		if (VATTR_IS_ACTIVE(vap, va_flags)) {
			np->n_dosattr = dosattr;
			VATTR_SET_SUPPORTED(vap, va_flags);
		}
	}
	
	if (VATTR_IS_ACTIVE(vap, va_data_size) && (vnode_isreg(vp))) {
        error = smbfs_set_data_size(share, vp, vap, &modified, context);
        if (error) {
            goto out;
        }
  	}

	/*
	 * Note that it's up to the caller to provide (or not) a fallback for
	 * backup_time, as we don't support them.
	 *
	 */
	crtime = VATTR_IS_ACTIVE(vap, va_create_time) ? &vap->va_create_time : NULL;
	mtime = VATTR_IS_ACTIVE(vap, va_modify_time) ? &vap->va_modify_time : NULL;
	atime = VATTR_IS_ACTIVE(vap, va_access_time) ? &vap->va_access_time : NULL;

	/* 
	 * If they are just setting the time to the same value then just say we made 
	 * the call. This will not hurt anything and will protect us from badly 
	 * written applications. Here is what was happening in the case of the finder 
	 * copy. The file gets copied, and the modify time and create time have been set to
	 * the current time by the server. The application is using utimes to set the
	 * modify time to the original file's modify time. This time is before the create time.
	 * So we set both the create and modify time to the same value. See the HFS note
	 * below. Now the applications wants to set the create time to be the same as the
	 * orignal file. In this case the original file has the same modify and create time. So
	 * we end up setting the create time twice to the same value. Even with this code the
	 * copy engine needs to be fixed, looking into that now. Looks like this will get fix
	 * with Radar 4385758. We should retest once that radar is completed.
	 */
	if (crtime && (timespeccmp(crtime, &np->n_crtime, ==))) {
		VATTR_SET_SUPPORTED(vap, va_create_time);
		crtime = NULL;
	}
	if (mtime && (timespeccmp(mtime, &np->n_mtime, ==))) {
		VATTR_SET_SUPPORTED(vap, va_modify_time);		
		mtime = NULL;
	}
	if (atime && (timespeccmp(atime, &np->n_atime, ==))) {
		VATTR_SET_SUPPORTED(vap, va_access_time);
		atime = NULL;
	}
 
	/*
	 * We sometimes get sent a zero access time. Did some testing and found
	 * out the following:
	 *
	 *	MSDOS	- The date gets set to Dec 30 17:31:44 1969
	 *	SMB FAT	- The date gets set to Jan  1 00:00:00 1980
	 *	UFS	- The date gets set to Dec 31 16:00:00 1969
	 *	SMB NTFS- The date gets set to Dec 31 16:00:00 1969
	 *	HFS	- The date displayed from ls is Dec 31 16:00:00 1969
	 *	HFS	- The getattrlist date is <no value>
	 *
	 * I believe this is from a utimes call where they are setting the
	 * modify time, but leaving the access time set to zero. We seem to be 
	 * doing the same thing as everyone else so let them do it.
	 */
	/*
	 * The following comment came from the HFS code.
	 * The utimes system call can reset the modification time but it doesn't
	 * know about create times. So we need to ensure that the creation time 
	 * is always at least as old as the modification time.
	 *
	 * The HFS code also checks to make sure it was not the root vnode. Don 
	 * Brady said that the SMB code should not use that part of the check.
	 */
	if (!crtime && mtime && (timespeccmp(mtime, &np->n_crtime, <))) {
		crtime = mtime;
	}
	
	if (!crtime && !mtime && !atime) {
		/* Nothing left to do here get out */
		goto out;
	}
	
	if (smbfsIsCacheable(vp) && (mtime)) {
		/* 
		 * <29067234> If setting mod date, make sure all pending UBC writes
		 * are finished. This prevents a later UBC write from resetting the
		 * mod date.
		 */
		ubc_msync (vp, 0, ubc_getsize(vp), NULL, UBC_PUSHDIRTY | UBC_SYNC);
	}
	
retrySettingTime:
#if 0
	if (crtime && mtime) {
		SMBDEBUG_LOCK(np, "%s crtime = %ld:%ld mtime = %ld:%ld\n", np->n_name,
                      crtime->tv_sec, crtime->tv_nsec, mtime->tv_sec, mtime->tv_nsec);
	} else if (crtime) {
		SMBDEBUG_LOCK(np, "%s crtime = %ld:%ld\n", np->n_name, crtime->tv_sec, crtime->tv_nsec);
	} else if (mtime) {
		SMBDEBUG_LOCK(np, "%s mtime = %ld:%ld\n", np->n_name, mtime->tv_sec, mtime->tv_nsec);
	}
#endif // SMB_DEBUG
	/* 
	 * FAT file systems don't support dates earlier than 1980, reset the
	 * date to 1980. Now the Finder likes to set the create date to 1904 or 1946, 
	 * should we treat this special? We could hold on to the original time 
	 * and return that value until they reset it or the vnode goes away. Since
	 * we never had any reports lets not add any extra code.
	 */ 
	if (useFatTimes) {
		if (crtime && (timespeccmp(crtime, &fat1980_time, <))) {
			crtime = &fat1980_time;
			SMBDEBUG_LOCK(np, "%s FAT crtime.tv_sec = %ld crtime.tv_nsec = %ld\n",
                          np->n_name, crtime->tv_sec, crtime->tv_nsec);
		}
		if (mtime && (timespeccmp(mtime, &fat1980_time, <))) {
			mtime = &fat1980_time;
			SMBDEBUG_LOCK(np, "%s FAT mtime.tv_sec = %ld mtime.tv_nsec = %ld\n",
                          np->n_name, mtime->tv_sec, mtime->tv_nsec);
		}
		/* Never let them set the access time before 1980 */
		if (atime && (timespeccmp(atime, &fat1980_time, <))) {
			atime = NULL;
		}
	}
	
	if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
       /* SMB 2/3 always has to open/set/close the file */
        error = smb2fs_smb_setpattrNT(share, np, vnode_type,
                                      NULL, 0,
                                      np->n_dosattr, crtime,
                                      mtime, atime,
                                      context);
        SMB_LOG_KTRACE(SMB_DBG_SMBFS_SETATTR | DBG_FUNC_NONE,
                       0xabc005, error, 0, 0, 0);
    }
    else {
        rights = SMB2_FILE_WRITE_ATTRIBUTES;
        /*
         * For Windows 95/98/Me/NT4 and all old dialects we must have 
         * the item open before we can set the date and time. For all 
         * other systems; if the item is already open then make sure it 
         * has the correct open mode.
         *
         * We currently never do a NT style open with write attributes.
         * So for all systems except NT4 that spport the NTCreateAndX 
         * call we will fall through and just use the set path method.
         * In the future we may decide to add open deny support. So if
         * we decide to add write atributes access then this code will
         * work without any other changes.  
         */ 
        if ((SS_TO_SESSION(share)->session_flags & SMBV_NT4) || 
            (smp->sm_flags & MNT_REQUIRES_FILEID_FOR_TIME) ||
            ((!vnode_isdir(vp)) && np->f_refcnt && (np->f_rights & rights))) {
            
            /*
             * For NT systems we need the file open for write attributes. 
             * Either write access or all access will work. If they 
             * already have it open for all access just use that, 
             * otherwise use write. We corrected tmpopen
             * to work correctly now. So just ask for the NT access.
             */
            error = smbfs_tmpopen(share, np, rights, &fid, context);
            if (error)
                goto out;
            
            error = smbfs_smb_setfattrNT(share, np->n_dosattr, fid, crtime, 
                                         mtime, atime, context);
            cerror = smbfs_tmpclose(share, np, fid, context);
            if (cerror) {
                SMBERROR_LOCK(np, "error %d closing fid %llx file %s\n",
                              cerror, fid, np->n_name);
            }

        } else {
            error = smbfs_smb_setpattrNT(share, np, vnode_type,
                                         NULL, 0,
                                         np->n_dosattr, crtime, mtime,
                                         atime, context);
            /* They don't support this call, we need to fallback to the old method; stupid NetApp */
            if (error == ENOTSUP) {
                SMBWARNING("Server does not support setting time by path, fallback to old method\n"); 
                smp->sm_flags |= MNT_REQUIRES_FILEID_FOR_TIME;	/* Never go down this path again */
                error = smbfs_tmpopen(share, np, rights, &fid, context);
                if (!error) {
                    error = smbfs_smb_setfattrNT(share, np->n_dosattr, fid, 
                                                 crtime, mtime, atime, context);
                    (void)smbfs_tmpclose(share, np, fid, context);				
                }
            }
        }
    }
	/* 
	 * Some servers (NetApp) don't support setting time before 1970 and will
	 * return an error here. So if we get an error and we were trying to set
	 * the time to before 1970 lets try again, but this time lets treat them
	 * the same as a FAT file system.
	 */
	if (error && !useFatTimes && 
		((crtime && (crtime->tv_sec < 0)) || (mtime && (mtime->tv_sec < 0)))) {
		useFatTimes = TRUE;
		goto retrySettingTime;
	}
	 
	if (error)
		goto out;
	
	if (crtime) {
		VATTR_SET_SUPPORTED(vap, va_create_time);
		np->n_crtime = *crtime;
		np->n_crtime.tv_nsec = 0; /* we only use seconds */
	}
    
	if (mtime) {
		VATTR_SET_SUPPORTED(vap, va_modify_time);
		np->n_mtime = *mtime;
		np->n_mtime.tv_nsec = 0; /* we only use seconds */
	}
    
	if (atime) {
		VATTR_SET_SUPPORTED(vap, va_access_time);
		np->n_atime = *atime;
		np->n_atime.tv_nsec = 0; /* we only use seconds */
	}
    
	/* Update the change time */
	if (crtime || mtime || atime) {
		nanotime(&np->n_chtime); /* Need current date/time, so use nanotime */
    }
	
	/*
	 * This avoids a race with calls that were down immediately before we "set"
     * the current Meta Data. We dont want to update the Meta Data with stale
     * data from before the "set" call.
	 */
	nanouptime(&np->n_last_meta_set_time);

out:
	if (modified) {
		/*
		 * Invalidate attribute cache in case if server doesn't set
		 * required attributes.
		 */
		np->attribute_cache_timer = 0;	/* invalidate cache */
        smb_dir_cache_invalidate(vp, forceInvalidate);
    }
    
    if (bufferp != NULL) {
        SMB_FREE(bufferp, M_TEMP);
        bufferp = NULL;
    }

    if (hifi_uio != NULL) {
        uio_free(hifi_uio);
        hifi_uio = NULL;
    }

    SMB_LOG_KTRACE(SMB_DBG_SMBFS_SETATTR | DBG_FUNC_END, error, 0, 0, 0, 0);
	return (error);
}

/*
 * smbfs_vnop_getattr
 *
 * vnode_t	a_vp;
 * struct vnode_attr *a_vap;    
 * vfs_context_t a_context;
 */
static int 
smbfs_vnop_setattr(struct vnop_setattr_args *ap)
{
	int32_t error = 0;
	struct smbnode *np;
	struct smb_share *share;

    /*
     * Check to see if its a dataless file and allowed to be materialized
     * Only need to check if its a file and setting va_data_size
     */
    if (VATTR_IS_ACTIVE(ap->a_vap, va_data_size) && (!vnode_isdir(ap->a_vp))) {
        error = smbfs_is_dataless_access_allowed(ap->a_vp, 0, ap->a_context);
        if (error) {
            /* This process is not allowed to materialize this file */
            return(error);
        }
    }

    if ((error = smbnode_lock(VTOSMB(ap->a_vp), SMBFS_EXCLUSIVE_LOCK))) {
		return (error);
	}
    
    SMB_LOG_KTRACE(SMB_DBG_SET_ATTR | DBG_FUNC_START, 0, 0, 0, 0, 0);

	np = VTOSMB(ap->a_vp);
	np->n_lastvop = smbfs_vnop_setattr;
	share = smb_get_share_with_reference(VTOSMBFS(ap->a_vp));
	error = smbfs_setattr (share, ap->a_vp, ap->a_vap, ap->a_context);
	smb_share_rele(share, ap->a_context);
	smbnode_unlock(VTOSMB(ap->a_vp));
	/* If this is a stream try to update the parents meta cache timer. */
	if (vnode_isnamedstream(ap->a_vp)) {
		vnode_t parent_vp = vnode_getparent(ap->a_vp);
		if (parent_vp) {
			VTOSMB(parent_vp)->attribute_cache_timer = 0;
			vnode_put(parent_vp);		
		}	
	}

	SMB_LOG_KTRACE(SMB_DBG_SET_ATTR | DBG_FUNC_END, error, 0, 0, 0, 0);
    return (error);
}

/*
 * smbfs_vnop_blockmap
 *
 * vnode_t	a_vp;
 * off_t	a_foffset;    
 * uint32_t a_size;
 * daddr64_t *a_bpn;
 * uint32_t *a_run;
 * void *a_poff;
 * int32_t a_flags;
 */
static int 
smbfs_vnop_blockmap(struct vnop_blockmap_args *ap)
{	
    /* 
	 * Always match the VM page size.  At this time it is 4K which limits us 
	 * to 44 bit file size instead of 64...??? 
	 */
	/* make sure we don't go past EOF */
	if (ap->a_run)
		*ap->a_run = ap->a_size;
	
	/* divide it by block size, MUST match value in smbfs_strategy */
    *ap->a_bpn = (daddr64_t)(ap->a_foffset / PAGE_SIZE);
    
	if (ap->a_poff)
		*(int32_t *)ap->a_poff = 0;
	
    return (0);
}

/*
 * smbfs_vnop_strategy
 *
 *	struct buf *a_bp;
 */
static int 
smbfs_vnop_strategy(struct vnop_strategy_args *ap)
{
	struct buf *bp = ap->a_bp;
	vnode_t vp = buf_vnode(bp);
	int32_t bflags = buf_flags(bp);
	struct smbnode *np = VTOSMB(vp);
	caddr_t io_addr = 0;
	uio_t	uio = NULL;
	int32_t error;
    SMBFID fid = 0;
	struct smb_share *share;
	uint32_t trycnt = 0;
    struct smbfattr *fap = NULL;
		
    SMB_LOG_KTRACE(SMB_DBG_STRATEGY | DBG_FUNC_START, 0, 0, 0, 0, 0);

	if (np->f_openState & kNeedRevoke) {
        /* behave like the deadfs does */
        SMBERROR_LOCK(np, "%s waiting to be revoked\n", np->n_name);
        error = EIO;
		buf_seterror(bp, error);
        goto exit;
    }
	
	if (np->f_refcnt == 0) {
        /* 
         * This happens when a file is open and mmapped, then server goes down
         * and calls start soft timing out with ETIMEDOUT. The reads/writes 
         * error out, then the close gets called, it gets an error but clears
         * the f_refcnt.  When unmount comes along, it tries again to push
         * out the data and thats when we end up here.
         */
        SMBERROR_LOCK(np, "%s has no file refs\n", np->n_name);
        error = EIO;
		buf_seterror(bp, error);
        goto exit;
    }

    SMB_MALLOC(fap,
               struct smbfattr *,
               sizeof(struct smbfattr),
               M_SMBTEMP,
               M_WAITOK | M_ZERO);
    if (fap == NULL) {
        SMBERROR("SMB_MALLOC failed\n");
        error = ENOMEM;
        goto exit;
    }

    /*
	 * Can't use the physical addresses passed in the vector list, so map it
	 * into the kernel address space
	 */
    if ((error = buf_map(bp, &io_addr))) {
        panic("smbfs_vnop_strategy: buf_map() failed with (%d)", error);
	}
	
	uio = uio_create(1, ((off_t)buf_blkno(bp)) * PAGE_SIZE, UIO_SYSSPACE, 
					 (bflags & B_READ) ? UIO_READ : UIO_WRITE);
	if (!uio) {
        panic("smbfs_vnop_strategy: uio_create() failed");
	}
	
	uio_addiov(uio, CAST_USER_ADDR_T(io_addr), buf_count(bp));
	/* 
	 * Remember that buf_proc(bp) can return NULL, but in that case this is
	 * coming from the kernel and is not associated with a particular proc.  
	 * In fact it just may be the pager itself trying to free up space and there 
	 * is no proc. I need to find any proc that already has the fork open for 
	 * read or write to use for read/write to work. This is handled in the
	 * FindFileRef routine.
	 */
	if (FindFileRef(vp, buf_proc(bp), (bflags & B_READ) ? kAccessRead : kAccessWrite, 
					kCheckDenyOrLocks, uio_offset(uio), uio_resid(uio), NULL, &fid)) {
		/* No matches or no pid to match, so just use the generic shared fork */
		fid = np->f_fid;	/* We should always have something at this point */
	}
	DBG_ASSERT(fid);	 
	
	SMB_LOG_IO_LOCK(np, "%s: %s offset %lld, size %lld, bflags 0x%x\n",
                    (bflags & B_READ) ? "Read":"Write", np->n_name, uio_offset(uio),
                    uio_resid(uio), bflags);
	
	share = smb_get_share_with_reference(VTOSMBFS(vp));
	/*
	 * Since we have already authorized the user when we opened the file, just 
	 * pass a NULL context down to the authorization code.
	 */
	if (bflags & B_READ) {
		error = smbfs_doread(share, (off_t)np->n_size, uio, fid, NULL);
	} else {
		error = smbfs_dowrite(share, (off_t)np->n_size, uio, fid, 0, NULL);
        
        if (!error) {
            /* Save last time we wrote data */
            nanouptime(&np->n_last_write_time);
        }
	}
    SMB_LOG_KTRACE(SMB_DBG_STRATEGY | DBG_FUNC_NONE,
                   0xabc001, error, 0, 0, 0);

	/*
	 * We can't handle reopens in the normal fashion, because we have no lock. A 
	 * bad file descriptor error, could mean a reconnect happen. Since
	 * we revoke all opens with manatory locks or open deny mode, we can just
	 * do the open, read/write then close.
	 *
	 * We now have a counter that keeps us from going into an infinite loop. Once
	 * we implement SMB 2/3 we should take a second look at how this should be done.
	 */
	while ((error == EBADF) && (trycnt < SMB_MAX_REOPEN_CNT)) {
		lck_mtx_lock(&np->f_openStateLock);

        SMB_LOG_IO_LOCK(np, "%s failed the %s, because of reconnect, openState = 0x%x. Try again.\n",
				   np->n_name, (bflags & B_READ) ? "READ" : "WRITE", np->f_openState);

		if (np->f_openState & kNeedRevoke) {
			lck_mtx_unlock(&np->f_openStateLock);
			break;
		}

		np->f_openState |= (kNeedReopen | kInReopen);
		lck_mtx_unlock(&np->f_openStateLock);

		/* Could be a dfs share make sure we have the correct share */
		smb_share_rele(share, NULL);
		share = smb_get_share_with_reference(VTOSMBFS(vp));
		/* Recreate the uio */
		uio_free(uio);
		uio = uio_create(1, ((off_t)buf_blkno(bp)) * PAGE_SIZE, UIO_SYSSPACE, 
						 (bflags & B_READ) ? UIO_READ : UIO_WRITE);
		if (!uio) {
			error = ENOMEM;
		} else {
            uio_addiov(uio, CAST_USER_ADDR_T(io_addr), buf_count(bp));
		
            error = smbfs_smb_open_file(share, np,
                                        np->f_rights, NTCREATEX_SHARE_ACCESS_ALL, &fid,
                                        NULL, 0, FALSE,
                                        fap, NULL);
        }
        
        lck_mtx_lock(&np->f_openStateLock);
        np->f_openState &= ~kInReopen;
        lck_mtx_unlock(&np->f_openStateLock);

		if (error) {
			break;
		}
		if (bflags & B_READ) {
			error = smbfs_doread(share, (off_t)np->n_size, uio, fid, NULL);
		} else {
			error = smbfs_dowrite(share, (off_t)np->n_size, uio, fid, 0, NULL);
            
            if (!error) {
                /* Save last time we wrote data */
                nanouptime(&np->n_last_write_time);
            }
		}
        SMB_LOG_KTRACE(SMB_DBG_STRATEGY | DBG_FUNC_NONE,
                       0xabc002, error, 0, 0, 0);

		(void)smbfs_smb_close(share, fid, NULL);
		trycnt++;
	}
	
	if (bflags & B_READ) {
		/* 
		 * If we were not able to read the entire page, check to
		 * see if we are at the end of the file, and if so, zero
		 * out the remaining part of the page.
		 */
		while ((error == 0) && (uio_resid(uio))) {
			size_t bytes_to_zero = (uio_resid(uio) > PAGE_SIZE) ? PAGE_SIZE : (size_t)uio_resid(uio);
			
			bzero((caddr_t) (io_addr + buf_count(bp) - uio_resid(uio)), bytes_to_zero); 
			uio_update(uio, bytes_to_zero);
		}
    } else {
		lck_mtx_lock(&np->f_clusterWriteLock);
		if ((u_quad_t)uio_offset(uio) >= np->n_size) {
			/* We finished writing past the eof reset the flag */
			nanouptime(&np->n_sizetime);
			np->waitOnClusterWrite = FALSE;
            SMB_LOG_IO_LOCK(np, "%s: TURNING OFF waitOnClusterWrite np->n_size = %lld\n",
                            np->n_name, np->n_size);
		}
		lck_mtx_unlock(&np->f_clusterWriteLock);		
    }
	
	if (error) {
		SMBERROR_LOCK(np, "%s on %s failed with an error of %d\n",
                      (bflags & B_READ) ? "READ" : "WRITE", np->n_name, error);

		np->f_clusterCloseError = error;	/* Error to be returned on close */
		
		if ( (error == ENOTCONN) || (error == EBADF) || (error == ETIMEDOUT) ) {
			/* 
			 * VFS cluster code has now been changed to handle non transient 
			 * errors see radar 2894150.  It should be safe to return ENXIO. 
			 */
			SMB_LOG_IO("failed with %d, returning ENXIO instead\n", error);
			error = ENXIO;
		}
	}
	smb_share_rele(share, NULL);
    buf_seterror(bp, error);
    
	/* Should be zero when all done with reading/writing file */
    buf_setresid(bp, (uint32_t)uio_resid(uio));
	
    if ((error = buf_unmap(bp)))
        panic("smbfs_vnop_strategy: buf_unmap() failed with (%d)", error);
	
    
exit:
	if (error) {
		SMB_LOG_IO_LOCK(np, "%s: buf_resid(bp) %d, error = %d \n",
                        np->n_name, buf_resid(bp), error);
	}
    
	if (uio != NULL)
		uio_free(uio);
    
    buf_biodone(bp);

    if (fap != NULL) {
        SMB_FREE(fap, M_SMBTEMP);
    }

    SMB_LOG_KTRACE(SMB_DBG_STRATEGY | DBG_FUNC_END, error, 0, 0, 0, 0);
    return (error);
}

/*
 * smbfs_vnop_read
 *
 *	vnode_t a_vp;
 *	uio_t a_uio;
 *	int a_ioflag;
 *	vfs_context_t a_context;
 */
static int 
smbfs_vnop_read(struct vnop_read_args *ap)
{
	vnode_t vp = ap->a_vp;
	uio_t uio = ap->a_uio;
	int error = 0;
	struct smbnode *np = NULL;
    SMBFID fid = 0;
	struct smb_share *share;
	
	/* Preflight checks */
	if (!vnode_isreg(vp)) {
		/* can only read regular files */
		if (vnode_isdir(vp))
			return (EISDIR);
		else
			return (EPERM);
	}

	if (uio_resid(uio) == 0)
		return (0);		/* Nothing left to do */
	
	if (uio_offset(uio) < 0)
		return (EINVAL);	/* cant read from a negative offset */
	
    /*
     * Check to see if its a dataless file and allowed to be materialized
     */
    error = smbfs_is_dataless_access_allowed(vp, 0, ap->a_context);
    if (error) {
        /* This process is not allowed to materialize this file */
        return(error);
    }

    if ((error = smbnode_lock(VTOSMB(vp), SMBFS_SHARED_LOCK)))
		return (error);
	
    SMB_LOG_KTRACE(SMB_DBG_READ | DBG_FUNC_START,
                   uio_offset(uio),
                   uio_resid(uio), 0, 0, 0);

	np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_read;
	share = smb_get_share_with_reference(VTOSMBFS(vp));
	/*
	 * History: FreeBSD vs Darwin VFS difference; we can get VNOP_READ without
 	 * preceeding open via the exec path, so do it implicitly.  VNOP_INACTIVE  
	 * closes the extra network file handle, and decrements the open count.
	 *
	 * If we are in reconnect mode calling smbfs_open will handle any reconnect
	 * issues. So only if we have a f_refcnt do we call smbfs_smb_reopen_file.
 	 */
 	if (!np->f_refcnt) {
 		error = smbfs_open(share, vp, FREAD, ap->a_context);
 		if (error)
 			goto exit;
		else np->f_needClose = 1;
 	} else {
		error = smbfs_smb_reopen_file(share, np, ap->a_context);
		if (error) {
			SMBDEBUG_LOCK(np, " %s waiting to be revoked\n", np->n_name);
			goto exit;
		}
	}
    
    /* 
	 * Note: smbfs_isCacheable checks to see if the file is globally non 
	 * cacheable, but we also need to support non cacheable on a per file
	 * descriptor level for reads/writes. So also check the IO_NOCACHE flag.
	 *
	 * So IO_NOCACHE means the same thing as VNOCACHE_DATA but only for this IO.
	 * Now VNOCACHE_DATA has the following comment:
	 *		"don't keep data cached once it's been consumed"
	 * Which seems to imply we could use cluster_read, but we would need to 
	 * push out any data first and then invalidate the range after we are done 
	 * with the read? Byte range locking prevents us from using the cluster_read.
	 */
	if (smbfsIsCacheable(vp) &&
        !(ap->a_ioflag & IO_NOCACHE) &&
        !(SS_TO_SESSION(share)->session_misc_flags & SMBV_MNT_DATACACHE_OFF)) {
		error = cluster_read(vp, uio, (off_t) np->n_size, ap->a_ioflag);
        SMB_LOG_KTRACE(SMB_DBG_READ | DBG_FUNC_NONE, 0xabc001, error, 0, 0, 0);
		if (error) {
			SMB_LOG_IO_LOCK(np, "%s failed cluster_read with an error of %d\n",
                            np->n_name, error);
		}
		/* 
		 * EACCES means a denyConflict occured and must have hit some other 
		 * computer's byte range lock on that file. Mark the file as 
		 * noncacheable and retry read again
		 *
		 * Need to check the error here, could be EIO/EPERM
		 *
		 * XXX - NOTE: The strategy routine is going to mark this vnode as needing 
		 * to be revoked. So this code seems wrong. We should either correct
		 * the strategy routine or remove this code.
		 */
		if (error == EACCES) {
			if (np->n_flag & NISMAPPED) {
				/* More expensive, but handles mmapped files */
				ubc_msync (vp, 0, ubc_getsize(vp), NULL, UBC_PUSHDIRTY | UBC_SYNC);
			} else {
				/* Less expensive, but does not handle mmapped files */
				cluster_push(vp, IO_SYNC);
			}
			ubc_msync (vp, 0, ubc_getsize(vp), NULL, UBC_INVALIDATE);
			vnode_setnocache(vp);
			/* Fall through and try a non cached read */
			error = 0;
 		} else {
			goto exit;
		}
    }
    
	if (error)
		goto exit;
    
	/* 
	 * AFP COMMENTS (<rdar://problem/5977339>) Be careful of a cacheable write 
	 * that extends the eof, but leaves a hole between the old eof and the new 
	 * eof.  It that is followed by a non cacheable read that starts before the 
	 * old eof and extends into the hole (or starts in the hole), but does not 
	 * extend past the hole, then msync for the read range will not find any 
	 * dirty pages in the hole and thus no data will be pushed and thus the eof 
	 * will not get extended which will cause the non cacheable read to get 
	 * erroneous data.  cluster_push works around this  because it pushes all of 
	 * the pages in the delayed clusters. msync for the entire range also works 
	 * too. 
	 */
	if (VTOSMB(vp)->n_flag & NISMAPPED) {
		/* More expensive, but handles mmapped files */
		ubc_msync (vp, 0, ubc_getsize(vp), NULL, UBC_PUSHDIRTY | UBC_SYNC);
	} else {
		/* Less expensive, but does not handle mmapped files */
		cluster_push(vp, IO_SYNC);
	}
    
	/* 
	 * AFP doesn't invalidate the range here. That seems wrong, we should
	 * invalidate the range here. 
	 */
	ubc_msync (vp, uio_offset(uio), uio_offset(uio)+ uio_resid(uio), NULL, 
			   UBC_INVALIDATE);
	
	if (FindFileRef(vp, vfs_context_proc(ap->a_context), kAccessRead, 
					kCheckDenyOrLocks, uio_offset(uio), uio_resid(uio), 
					NULL, &fid)) {
		/* No matches or no pid to match, so just use the generic shared fork */
		fid = np->f_fid;	/* We should always have something at this point */
	}
	DBG_ASSERT(fid);	

	error = smbfs_doread(share, (off_t)np->n_size, uio, fid, ap->a_context);
    SMB_LOG_KTRACE(SMB_DBG_READ | DBG_FUNC_NONE, 0xabc002, error, 0, 0, 0);

	/*
	 * We got an error, did it happen on a reconnect, then retry. First see if
	 * we have a new share to use, then attmept to reopent the file, then try
	 * the read again.
	 */
	while (error == EBADF) {
		SMB_LOG_IO_LOCK(np, "%s failed the non cache read, because of reconnect. Try again.\n",
                        np->n_name);

		/* Could be a dfs share make sure we have the correct share */
		smb_share_rele(share, ap->a_context);
		share = smb_get_share_with_reference(VTOSMBFS(vp));
		/* The reopen code will handle the case of the node being revoked. */
		if (smbfs_io_reopen(share, vp, uio, kAccessRead, &fid, error, ap->a_context) == 0) {
			error = smbfs_doread(share, (off_t)np->n_size, uio, fid, 
                                 ap->a_context);
            SMB_LOG_KTRACE(SMB_DBG_READ | DBG_FUNC_NONE,
                           0xabc003, error, 0, 0, 0);
		}
        else {
			SMB_LOG_IO_LOCK(np, "%s : The Read reopen failed.\n", np->n_name);
			break;
		}
	}
	
	if (error) {
		SMB_LOG_IO_LOCK(np, "%s failed non cached read with an error of %d\n", np->n_name, error);
	}
	
exit:
	smb_share_rele(share, ap->a_context);
	smbnode_unlock(np);

	SMB_LOG_KTRACE(SMB_DBG_READ | DBG_FUNC_END, error, 0, 0, 0, 0);
    return (error);
}

/*
 * smbfs_vnop_write
 *
 *	vnode_t a_vp;
 *	uio_t a_uio;
 *	int a_ioflag;
 *	vfs_context_t a_context;
 */
static int 
smbfs_vnop_write(struct vnop_write_args *ap)
{
	vnode_t vp = ap->a_vp;
	vnode_t parent_vp = NULL;	/* Always null unless this is a stream node */
	struct smbnode *np = NULL;
	struct smb_share *share;
	uio_t uio = ap->a_uio;
	int error = 0;
    SMBFID fid = 0;
	u_quad_t originalEOF = 0;	
	user_size_t writeCount;
	
	/* Preflight checks */
	if (!vnode_isreg(vp)) {
		/* can only read regular files */
		if (vnode_isdir(vp))
			return (EISDIR);
		else
			return (EPERM);
	}

	if (uio_offset(uio) < 0)
		return (EINVAL);
	
	if (uio_resid(uio) == 0)
		return (0);
	
    /*
     * Check to see if its a dataless file and allowed to be materialized
     */
    error = smbfs_is_dataless_access_allowed(vp, 0, ap->a_context);
    if (error) {
        /* This process is not allowed to materialize this file */
        return(error);
    }

    if ((error = smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK)))
		return (error);
	
    SMB_LOG_KTRACE(SMB_DBG_WRITE | DBG_FUNC_START,
                   uio_offset(uio),
                   uio_resid(uio), 0, 0, 0);

	np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_write;
	share = smb_get_share_with_reference(VTOSMBFS(vp));
	
	/* Before trying the write see if the file needs to be reopened */
	error = smbfs_smb_reopen_file(share, np, ap->a_context);
	if (error) {
		SMBDEBUG_LOCK(np, " %s waiting to be revoked\n", np->n_name);
		goto exit;
	}
	
	if (ap->a_ioflag & IO_APPEND)
		uio_setoffset(uio, np->n_size);
	
	originalEOF = np->n_size;	/* Save off the orignial end of file */
	
    /* 
	 * Note: smbfs_isCacheable checks to see if the file is globally non 
	 * cacheable, but we also need to support non cacheable on a per file
	 * descriptor level for reads/writes. So also check the IO_NOCACHE flag.
	 *
	 * So IO_NOCACHE means the same thing as VNOCACHE_DATA but only for this IO.
	 * Now VNOCACHE_DATA has the following comment:
	 *		"don't keep data cached once it's been consumed"
	 * Which seems to imply we could use cluster_write, but we would need to 
	 * push out any data and then invalidate the range after we are done 
	 * with the write? Byte range locking prevents us from using the cluster_write.
	 */
	if (smbfsIsCacheable(vp) &&
        !(ap->a_ioflag & IO_NOCACHE) &&
        !(SS_TO_SESSION(share)->session_misc_flags & SMBV_MNT_DATACACHE_OFF)) {
		u_quad_t writelimit;
        u_quad_t newEOF;
        u_quad_t zero_head_off;
        u_quad_t zero_tail_off;
        int32_t   lflag;

		lflag = ap->a_ioflag & ~(IO_TAILZEROFILL | IO_HEADZEROFILL |
								 IO_NOZEROVALID | IO_NOZERODIRTY);
		zero_head_off = 0;
		zero_tail_off = 0;			
		writelimit = uio_offset(uio) + uio_resid(uio);       
		
		/* 
		 * They are writing pass the eof, force a zero fill. What should we 
		 * do for sparse files?
		 */
        if ((uint64_t)writelimit > np->n_size) {			
			/* Save off the new eof */
            newEOF = writelimit;
            if ((uint64_t) uio_offset(uio) > np->n_size) {
                zero_head_off = np->n_size;
				/* Make sure we tell the kernel to zero fill the head */ 
                lflag |= IO_HEADZEROFILL;            
            }
  			zero_tail_off = (writelimit + (PAGE_SIZE_64 - 1)) & ~PAGE_MASK_64;
			if (zero_tail_off > newEOF) { 
				zero_tail_off = newEOF;
			}
			if (zero_tail_off > writelimit) {
				/* Make sure we tell the kernel to zero fill the tail */
				lflag |= IO_TAILZEROFILL; 
			}
        } else
            newEOF = np->n_size;
		
		lck_mtx_lock(&np->f_clusterWriteLock);
		if (originalEOF < newEOF) {
			np->waitOnClusterWrite = TRUE;
			np->n_size = newEOF;
			SMB_LOG_IO_LOCK(np, "%s: TURNING ON waitOnClusterWrite old eof = %lld new eof = %lld\n",
                            np->n_name, originalEOF, newEOF);
		}
		lck_mtx_unlock(&np->f_clusterWriteLock);
		/*
         * If the write starts beyond the current EOF then we we'll zero fill 
		 * from the current EOF to where the write begins
         */
        error = cluster_write(vp, uio, originalEOF, newEOF, zero_head_off, zero_tail_off, lflag);
        SMB_LOG_KTRACE(SMB_DBG_WRITE | DBG_FUNC_NONE,
                       0xabc001, error, 0, 0, 0);
		if (error) {
			lck_mtx_lock(&np->f_clusterWriteLock);
			np->n_size = originalEOF; /* Set it back to the original eof */
			np->waitOnClusterWrite = FALSE;
			lck_mtx_unlock(&np->f_clusterWriteLock);

            SMB_LOG_IO_LOCK(np, "%s failed cluster_write with an error of %d\n", np->n_name, error);
		}
		/* 
		 * EACCES means a denyConflict occured and must have hit some other 
		 * computer's byte range lock on that file. Mark the file as 
		 * noncacheable and retry read again
		 *
		 * Need to check the error here, could be EIO/EPERM
		 *
		 * XXX - NOTE: The stragey routine is going to mark this vnode as needing 
		 * to be revokd. So this code seems wrong. We should either correct
		 * the stragey routine or remove this code.
		 */
		if (error == EACCES) {
			if (np->n_flag & NISMAPPED) {
				/* More expensive, but handles mmapped files */
				ubc_msync (vp, 0, ubc_getsize(vp), NULL, UBC_PUSHDIRTY | UBC_SYNC);
			} else {
				/* Less expensive, but does not handle mmapped files */
				cluster_push(vp, IO_SYNC);	
			}
			ubc_msync (vp, 0, ubc_getsize(vp), NULL, UBC_INVALIDATE);
			vnode_setnocache(vp);
			/* Fall through and try a non cached write */
			error = 0;
 		} else {
			goto exit;
		}
    }
    
	if (error)
		goto exit;
    
    /*
	 * If it is not cacheable, make sure to wipe out UBC since any of its
	 * data would be no be invalid.  Make sure to push out any dirty data 
	 * first due to prev cached write. 
	 */
	ubc_msync(vp, uio_offset(uio), uio_offset(uio)+ uio_resid(uio), NULL, 
			   UBC_PUSHDIRTY | UBC_SYNC);
	ubc_msync(vp, uio_offset(uio), uio_offset(uio)+ uio_resid(uio), NULL, 
			   UBC_INVALIDATE);
	
	
	if (FindFileRef(vp, vfs_context_proc(ap->a_context), kAccessWrite, 
					kCheckDenyOrLocks, uio_offset(uio), uio_resid(uio), NULL, &fid)) {
		/* No matches or no pid to match, so just use the generic shared fork */
		fid = np->f_fid;	/* We should always have something at this point */
	}
	DBG_ASSERT(fid);

	/* Total amount that we need to write */
	writeCount = uio_resid(ap->a_uio);
	do {
		if (uio && (uio != ap->a_uio)) {
			/* We allocated, need to free it */
			uio_free(uio);
		}
		/*
		 * We could get disconnected in the middle of a write, so we need to make 
		 * a copy of the uio, just in case. 
		 */
		uio = uio_duplicate(ap->a_uio);
		if (uio == NULL) {
			/* Failed, so just used the passed in uio */
			uio = ap->a_uio;
		}
		error = smbfs_dowrite(share, (off_t)np->n_size, uio, fid, ap->a_ioflag, 
							  ap->a_context);
        SMB_LOG_KTRACE(SMB_DBG_WRITE | DBG_FUNC_NONE,
                       0xabc002, error, 0, 0, 0);
        
        if (!error) {
            /* Save last time we wrote data */
            nanouptime(&np->n_last_write_time);
        }
        
		if (error == EBADF) {
			/* Could be a dfs share make sure we have the correct share */
			smb_share_rele(share, ap->a_context);
			share = smb_get_share_with_reference(VTOSMBFS(vp));
			if (smbfs_io_reopen(share, vp, uio, kAccessWrite, &fid,  error, 
								ap->a_context) != 0) {
				/* The reopen failed, just get out nothing left to do here */
				break;
			}
		}
	} while ((error == EBADF) && (uio != ap->a_uio));
	/* Set the original uio to match the one passed into the write. */
	if (uio != ap->a_uio) {
		/* Total amount we were able to write */ 
		writeCount -= uio_resid(uio);
		/* Update the user's uio with that amount */
		uio_update( ap->a_uio, writeCount);
		uio_free(uio);
		uio = ap->a_uio;
	}
	
	/* 
	 * Mark that we need to send a flush if we didn't get an error and 
	 * we didn't send a write though message. Remember if the IO_SYNC bit
	 * is set then we set the write though bit, which should do the same
	 * thing as a flush.
	 */
	if (!error && !(ap->a_ioflag & IO_SYNC)) {
		VTOSMB(vp)->n_flag |= NNEEDS_FLUSH;
	} else if (error) {
		SMB_LOG_IO_LOCK(np, "%s failed non cached write with an error of %d\n",
                        np->n_name, error);
	}
exit:
	/* Wrote pass the eof, need to set the new file size */
	if (!error && ((uint64_t) uio_offset(uio) > originalEOF)) {
		/* 
		 * Windows servers do not handle writing past the end of file the 
		 * same as Unix servers. If we do a directory lookup before the file 
		 * is closed then the server may return the old size. Setting the end of
		 * file here will prevent that from happening. Unix servers do not seem  
		 * to have this problem so there is no reason to make this call in that 
		 * case. So if the file size has changed and this is not a Unix server 
		 * then set the eof of file to the new value.
		 */
		if (!UNIX_SERVER(SS_TO_SESSION(share))) {
            /* 
             * Just set the flag to set EOF later. Otherwise for every write
             * on a cached file, a set EOF request will be sent to the server.
             * This will result in hundreds/thousands of set EOF requests being
             * sent constantly to the server.
             *
             * The file size is cached locally so sending the set EOF request
             * to the server can wait until system sync or vnop_sync time.
             */
            np->n_flag |= NNEEDS_EOF_SET;
            
            /*
             * Windows FAT file systems require a flush, after a seteof. Until the
             * the flush they will keep returning the old file size.
             */
            if (share->ss_fstype == SMB_FS_FAT) {
                /* Do the flush later too */
                np->n_flag |= NNEEDS_FLUSH;

            }
		}
        
		smbfs_setsize(vp, uio_offset(uio));
        
		SMB_LOG_IO_LOCK(np, "%s: Calling smbfs_setsize, old eof = %lld  new eof = %lld time %ld:%ld\n",
                        np->n_name, originalEOF, uio_offset(uio),
                        np->n_sizetime.tv_sec, np->n_sizetime.tv_nsec);

		/*
		 * We could have lost a cache update in the write. Since we wrote pass 
		 * the eof, mark that the close should clear the cache timer. This way
		 * the cache will be update with the server. We clear this flag in cache
		 * entry so any lookups after this write will clear the flag.
		 */
		np->n_flag |= NATTRCHANGED;

        if (SS_TO_SESSION(share)->session_misc_flags & SMBV_MNT_HIGH_FIDELITY) {
            /*
             * For HiFi, cached writes are not used so the file size is now
             * different. Need to invalidate the meta data cache.
             */
            np->attribute_cache_timer = 0;
        }
	}
	
	/* Tell the stream's parent that something has changed */
	if (vnode_isnamedstream(vp))
		parent_vp = smb_update_rsrc_and_getparent(vp, FALSE);

	smb_share_rele(share, ap->a_context);
	smbnode_unlock(VTOSMB(vp));
	/* We have the parent vnode, so reset its meta data cache timer. */
	if (parent_vp) {
		VTOSMB(parent_vp)->attribute_cache_timer = 0;
		vnode_put(parent_vp);		
	}	
	
    SMB_LOG_KTRACE(SMB_DBG_WRITE | DBG_FUNC_END, error, 0, 0, 0, 0);
	return (error);
}

/*
 * This is an internal utility function called from our smbfs_mkdir and 
 * smbfs_create to set the vnode_attr passed into those routines. The first 
 * problem comes from the vfs layer. It wants to set things that the server will 
 * set for us. So to stop taking a performance hit turn off those items that vfs 
 * turn on. See vnode_authattr_new for what is getting set. Second there is a set 
 * of items that we can't set anyways so clear them out and pretend we did it. 
 * Third if they are setting ACLs then make sure we keep any inherited ACLs. 
 *
 * Question should we ever error out in this routine? The old code never did, but 
 * what if setting the mode, owner, group, or acl fails? 
 *
 * The calling routine must hold a reference on the share
 *
 */
static void 
smbfs_set_create_vap(struct smb_share *share, struct vnode_attr *vap, vnode_t vp, 
					 vfs_context_t context, int set_mode_now)
{
	struct smbnode *np = VTOSMB(vp);	
	struct smbmount *smp = np->n_mount;
	struct vnode_attr svrva;
	kauth_acl_t savedacl = NULL;
	int error;
    int has_posix_modes = ((UNIX_CAPS(share) & UNIX_QFILEINFO_UNIX_INFO2_CAP)) ? TRUE : FALSE;;
    
    if ((!has_posix_modes) &&
        (SS_TO_SESSION(share)->session_server_caps & kAAPL_SUPPORTS_NFS_ACE)) {
        /*
         * For OS X <-> OS X PFS (where ACLs are off by default), we
         * need a way to set Posix permissions. If the server supports
         * the NFS ACE, then it will allow us to get the ACL and send
         * it back with the desired Posix permissions in the NFS ACE.
         */
        has_posix_modes = TRUE;
    }
	
	/*
	 * Initialize here so we know if it needs to be freed at the end. Also
	 * ask for everything since its the same performance hit and updates our
	 * nodes uid/gid cache. The following are not used unless the user is
	 * setting ACLs.
	 */
	VATTR_INIT(&svrva);
	VATTR_WANTED(&svrva, va_acl);
	VATTR_WANTED(&svrva, va_uuuid);
	VATTR_WANTED(&svrva, va_guuid);
	
	/* 
	 * This will be zero if vnode_authattr_new set it. Do not change 
	 * this on create, the default is fine. 
	 */
	if (VATTR_IS_ACTIVE(vap, va_flags) && (vap->va_flags == 0)) {
		VATTR_SET_SUPPORTED(vap, va_flags);
		VATTR_CLEAR_ACTIVE(vap, va_flags);
	}
	/* The server will set all times for us */
	if (VATTR_IS_ACTIVE(vap, va_create_time)) {
		VATTR_SET_SUPPORTED(vap, va_create_time);
		VATTR_CLEAR_ACTIVE(vap, va_create_time);
	}
	if (VATTR_IS_ACTIVE(vap, va_modify_time)) {
		VATTR_SET_SUPPORTED(vap, va_modify_time);
		VATTR_CLEAR_ACTIVE(vap, va_modify_time);
	}
	if (VATTR_IS_ACTIVE(vap, va_access_time)) {
		VATTR_CLEAR_ACTIVE(vap, va_access_time);
		VATTR_SET_SUPPORTED(vap, va_access_time);
	}
	if (VATTR_IS_ACTIVE(vap, va_change_time)) {
		VATTR_CLEAR_ACTIVE(vap, va_change_time);
		VATTR_SET_SUPPORTED(vap, va_change_time);
	}
	
	if (VATTR_IS_ACTIVE(vap, va_mode)) {
		if (set_mode_now == TRUE) {
			/* compound_vnop_open already has the file open so we can set the 
			 Posix file modes if server supports Posix perms and reg file */
			VATTR_SET_SUPPORTED(vap, va_mode);
			
			if ( !vnode_isreg(vp) || !has_posix_modes ) {
				VATTR_CLEAR_ACTIVE(vap, va_mode);
			}
		}
        else {
			/*
			 * This routine gets called from create. If this is a regular file they 
			 * could be setting the posix file modes to something that doesn't allow 
			 * them to open the file with the permissions they requested. 
			 *		Example: open(path, O_WRONLY | O_CREAT | O_EXCL,  0400)
			 * We only care about this if the server supports setting/getting posix 
			 * permissions. So if they are setting the owner with read/write access 
			 * then save the settings until after the open. We will just pretend to 
			 * set them here. The following radar will make this a mute point.
			 *
			 * <rdar://problem/5199099> Need a new VNOP_OPEN call that allows the 
			 * create/lookup/open/access in one call
			 */
			if (vnode_isreg(vp) && has_posix_modes &&
				((vap->va_mode & (S_IRUSR | S_IWUSR)) != (S_IRUSR | S_IWUSR))) {
				np->create_va_mode = vap->va_mode;
				np->set_create_va_mode = TRUE;
				/* The server should always give us read/write on create */
				VATTR_SET_SUPPORTED(vap, va_mode);
				VATTR_CLEAR_ACTIVE(vap, va_mode);
			} else if (!has_posix_modes) {
				/* We can only set these if the server supports the unix extensions */
				VATTR_SET_SUPPORTED(vap, va_mode);
				VATTR_CLEAR_ACTIVE(vap, va_mode);
			}
		}
	}	
	
	/*
	 * We have never supported setting the uid, this is supported by the
	 * UNIX extensions and for other servers we could convert it using
	 * uid --> UUID --> SID translation. This just seems like a waste of
	 * time since they would only be able to set it to themself if they
	 * are already the owner. So just say we did, this is what we have
	 * always done.
	 */
	if (VATTR_IS_ACTIVE(vap, va_uid)) {
		VATTR_SET_SUPPORTED(vap, va_uid);
		VATTR_CLEAR_ACTIVE(vap, va_uid);
	}
	
	/*
	 * We have never supported setting the gid, this is supported by the
	 * UNIX extensions and for other servers we could convert it using
	 * gid --> GUID --> SID translation. We may want to add the support 
	 * in the future, but we should require that they do this by setting
	 * the va_guuid.
	 */
	if (VATTR_IS_ACTIVE(vap, va_gid)) {
		VATTR_SET_SUPPORTED(vap, va_gid);
		VATTR_CLEAR_ACTIVE(vap, va_gid);
		if (vap->va_gid != smp->sm_args.gid)
			SMB_LOG_ACCESS("They want to set the gid to %d from %d\n", 
						   vap->va_gid, smp->sm_args.gid);
	}
	
	/*
	 * If they have an ACE that allows them to take ownership then 
	 * this call will work. Should we test to see before going any
	 * future.
	 */
	if (VATTR_IS_ACTIVE(vap, va_uuuid)) {
		VATTR_SET_SUPPORTED(vap, va_uuuid);
		/* We never let them set it if a  TEMP UUID */ 
		if (is_memberd_tempuuid(&vap->va_uuuid))
			VATTR_CLEAR_ACTIVE(vap, va_uuuid);
	}
	
	/*
	 * If they have an ACE that allows them to make this change then 
	 * this call will work. Should we test to see before going any
	 * future.
	 */
	if (VATTR_IS_ACTIVE(vap, va_guuid)) {
		VATTR_SET_SUPPORTED(vap, va_guuid);
		/* We never let them set it if a  TEMP UUID */ 
		if (is_memberd_tempuuid(&vap->va_guuid))
			VATTR_CLEAR_ACTIVE(vap, va_guuid);
	}
	
	/* 
	 * Make sure the VFS layer doesn't fall back to using EA, always say we set 
	 * the ACL. 
	 */
	if (VATTR_IS_ACTIVE(vap, va_acl)) {
		VATTR_SET_SUPPORTED(vap, va_acl);
		/* Nothing for us to do here */
		if ((vap->va_acl == NULL) || (vap->va_acl->acl_entrycount == 0))
			VATTR_CLEAR_ACTIVE(vap, va_acl);
	}
	
	/* All we have left to do is call our setattr */
	if (!VATTR_IS_ACTIVE(vap, va_acl))
		goto do_setattr;
	
	/* Get the inherited ACLs from the server */
	error = smbfs_getattr(share, vp, &svrva, context);
	if (error) {
		SMBWARNING("Error %d returned while gettting the inherit ACLs from the server\n", 
				   error);
		goto out;
	}
	
	/*
	 * if none created then just slam in the requested ACEs
	 */
	if (!VATTR_IS_SUPPORTED(&svrva, va_acl) || svrva.va_acl == NULL ||
	    svrva.va_acl->acl_entrycount == 0) {
		goto do_setattr;
	}
	error = smbfs_compose_create_acl(vap, &svrva, &savedacl);
	if (error)
		goto out;
	
do_setattr:
	error = smbfs_setattr(share, vp, vap, context);
	if (error)
		SMBERROR("smbfs_setattr, error %d\n", error);
	if (savedacl) { 
		kauth_acl_free(vap->va_acl);
		vap->va_acl = savedacl;
	}
out:
	if (VATTR_IS_SUPPORTED(&svrva, va_acl) && svrva.va_acl != NULL)
		kauth_acl_free(svrva.va_acl);
	return;
}

/*
 * Create a regular file or a "symlink". In the symlink case we will have a target. Depending
 * on the sytle of symlink the target may be just what we set or we may need to encode it into
 * that wacky windows data 
 *
 * The calling routine must hold a reference on the share
 *
 */
static int 
smbfs_create(struct smb_share *share, struct vnop_create_args *ap, char *target, 
			 size_t targetlen)
{
	vnode_t				dvp = ap->a_dvp;
	struct vnode_attr *vap = ap->a_vap;
	vnode_t			*vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	struct smbnode *dnp = VTOSMB(dvp);
	struct smbmount *smp = VTOSMBFS(dvp);
	vnode_t 	vp;
	struct smbfattr fattr;
	const char *name = cnp->cn_nameptr;
	size_t nmlen = cnp->cn_namelen;
	int error;
	struct timespec ts;
	int unix_symlink = ((UNIX_CAPS(share) & UNIX_SFILEINFO_UNIX_LINK_CAP)) ? TRUE : FALSE;

	*vpp = NULL;
	if (vap->va_type != VREG && vap->va_type != VLNK)
		return (ENOTSUP);
	
    SMB_LOG_KTRACE(SMB_DBG_SMBFS_CREATE | DBG_FUNC_START,
                   vap->va_type, 0, 0, 0, 0);

	if (vap->va_type == VLNK) {
        if (smp->sm_flags & MNT_SUPPORTS_REPARSE_SYMLINKS) {
			error = smbfs_smb_create_reparse_symlink(share, dnp, name, nmlen, target, 
                                                     targetlen, &fattr, ap->a_context);
		} else if (unix_symlink) {
			error = smbfs_smb_create_unix_symlink(share, dnp, name, nmlen, target, 
												  targetlen, &fattr, ap->a_context);
		} else {
			error = smbfs_smb_create_windows_symlink(share, dnp, name, nmlen, target, 
												  targetlen, &fattr, ap->a_context);				
		}
	} else {
		/* Now create the file, sending a null fid pointer will cause it to be closed */
		error = smbfs_smb_create(share, dnp, name, nmlen, SMB2_FILE_WRITE_DATA, 
								 NULL, FILE_CREATE, 0, &fattr, ap->a_context);
	}

    SMB_LOG_KTRACE(SMB_DBG_SMBFS_CREATE | DBG_FUNC_NONE,
                   0xabc001, error, 0, 0, 0);

    if (error) {
		if (error == ENOENT) {
			SMBDEBUG("Creating %s returned ENOENT, resetting to EACCES\n", name);
			/* 
			 * Some servers (Samba) support an option called veto. This prevents
			 * clients from creating or access these files. The server returns
			 * an ENOENT error in these cases. The VFS layer will loop forever
			 * if a ENOENT error is returned on create, so we convert this error
			 * to EACCES.
			 */
			error = EACCES;
		}
        goto bad;
	}
	
	smbfs_attr_touchdir(dnp, (share->ss_fstype == SMB_FS_FAT));

	/* <33469405> if dir has active lease skip local change notify */
	lck_mtx_lock(&dnp->d_dur_handle.lock);
	if (!(dnp->d_dur_handle.flags & SMB2_LEASE_GRANTED) ||
		(dnp->d_dur_handle.flags & SMB2_LEASE_BROKEN)) {
		dnp->d_changecnt++;
	}
	lck_mtx_unlock(&dnp->d_dur_handle.lock);
	
	/* 
	 * We have smbfs_nget returning a lock so we need to unlock it when we 
	 * are done with it. Would really like to clean this code up in the future. 
	 * The whole create, mkdir and smblink create code should use the same code path.
	 */
	fattr.fa_vtype = vap->va_type;
	error = smbfs_nget(share, vnode_mount(dvp),
                       dvp, name, nmlen,
                       &fattr, &vp,
                       cnp->cn_flags, SMBFS_NGET_CREATE_VNODE,
                       ap->a_context);
	if (error) {
		/* Error, try to cleanup anything we created on the server */
		smbfs_smb_delete(share, dnp, VREG,
                         name, nmlen,
                         0, ap->a_context);
		goto bad;
	}

	/* 
	 * We just created the file, so we have no finder info and the resource fork
	 * should be empty. So set our cache timers to reflect this information
	 */
	nanouptime(&ts);
	VTOSMB(vp)->finfo_cache_timer = ts.tv_sec;
	VTOSMB(vp)->rfrk_cache_timer = ts.tv_sec;
	
	smbfs_set_create_vap(share, vap, vp, ap->a_context, FALSE);

    /* If HiFi and Super Guest mode, attempt to set the uid/gid */
    smbfs_hifi_set_perms(share, vp, ap->a_context);

    if (vap->va_type == VLNK) {
		smbfs_update_symlink_cache(VTOSMB(vp), target, targetlen);
		VTOSMB(vp)->n_size = targetlen;	/* Set it to the correct size */
		if (!unix_symlink)	/* Mark it as a Windows symlink */
			VTOSMB(vp)->n_flag |= NWINDOWSYMLNK;
	}
	*vpp = vp;
	smbnode_unlock(VTOSMB(vp));	/* Done with the smbnode unlock it. */
	
	/* <33469405> if dir has active lease skip local change notify */
	lck_mtx_lock(&dnp->d_dur_handle.lock);
	if (!(dnp->d_dur_handle.flags & SMB2_LEASE_GRANTED) ||
		(dnp->d_dur_handle.flags & SMB2_LEASE_BROKEN)) {
		dnp->d_changecnt++;
	}
	lck_mtx_unlock(&dnp->d_dur_handle.lock);

	/* Remove any negative cache entries. */
	if (dnp->n_flag & NNEGNCENTRIES) {
		dnp->n_flag &= ~NNEGNCENTRIES;
		cache_purge_negatives(dvp);
	}
	
bad:
	/* if success, blow away statfs cache */
	if (!error)
		smp->sm_statfstime = 0;

    SMB_LOG_KTRACE(SMB_DBG_SMBFS_CREATE | DBG_FUNC_END, error, 0, 0, 0, 0);
    return (error);
}

/*
 * smbfs_vnop_create
 *
 * vnode_t a_dvp;
 * vnode_t *a_vpp;
 * struct componentname *a_cnp;
 * struct vnode_attr *a_vap;
 * vfs_context_t a_context;
 */
static int 
smbfs_vnop_create(struct vnop_create_args *ap)
{
	vnode_t 	dvp = ap->a_dvp;
	int			error;
	struct smbnode *dnp;
	struct smb_share *share;

    /* Make sure we lock the parent before calling smbfs_create */
    if ((error = smbnode_lock(VTOSMB(dvp), SMBFS_EXCLUSIVE_LOCK)))
        return (error);

    SMB_LOG_KTRACE(SMB_DBG_CREATE | DBG_FUNC_START, 0, 0, 0, 0, 0);

    dnp = VTOSMB(dvp);
    dnp->n_lastvop = smbfs_vnop_create;
    share = smb_get_share_with_reference(VTOSMBFS(dvp));

    error = smbfs_create(share, ap, NULL, 0);

    smb_share_rele(share, ap->a_context);
    smbnode_unlock(dnp);

    SMB_LOG_KTRACE(SMB_DBG_CREATE | DBG_FUNC_END, error, 0, 0, 0, 0);
    return (error);
}

/*
 * The calling routine must hold a reference on the share
 */
static int 
smbfs_remove(struct smb_share *share, vnode_t dvp, vnode_t vp, 
			 struct componentname *cnp, int flags, vfs_context_t context)
{
#pragma unused(cnp)
    struct smbnode *dnp = VTOSMB(dvp);
    proc_t p = vfs_context_proc(context);
    struct smbnode *np = VTOSMB(vp);
    vnode_t svpp = NULL;
    struct smbmount *smp = VTOSMBFS(vp);
    int error;
    uint64_t hashval = 0;
    
    SMB_LOG_KTRACE(SMB_DBG_SMBFS_REMOVE | DBG_FUNC_START, 0, 0, 0, 0, 0);
    
    DBG_ASSERT((!vnode_isnamedstream(vp)))
    
    cache_purge(vp);
    
	/* Close any deferred file handles */
	CloseDeferredFileRefs(vp, "vnop_remove", 0, context);

	/* Is there a named stream (we only support resource fork)? */
    svpp = smbfs_find_vgetstrm(smp, np, SFM_RESOURCEFORK_NAME,
                               share->ss_maxfilenamelen);
    
    /*
     * Carbon semantics prohibit deleting busy files.
     * (enforced when NODELETEBUSY is requested) We just return
     * EBUSY here. Do not print an error to system log in this
     * case.
     *
     * NOTE: Kqueue opens will not be found by vnode_isinuse
     */
    if (flags & VNODE_REMOVE_NODELETEBUSY) {
        if (vnode_isinuse(vp, 0)) {
            error = EBUSY; /* Do not print an error in this case */
            goto done;
        }
        else {
            /* Check if any streams associated with this vnode are opened */
            if (svpp != NULL) {
                if (vnode_isinuse(svpp, 0)) {
                    /*
                     *  This vnode has an opened stream associated with it,
                     *  we still need to return EBUSY here.
                     *  See <rdar://problem/9904683>
                     */
                    SMBDEBUG("%s: Cannot delete %s, resource fork in use\n",
                             __FUNCTION__, vnode_getname(vp));
                    error = EBUSY; /* Do not print an error in this case */
                    goto done;
                }
            }
        }
    }
    
    if (svpp != NULL) {
        // rdar://problem/34755230
        // If we deferred closing the resource fork, we must close it now.
        // Otherwise a Windows server will hold off deleting the file until
        // the resource fork is closed. Also for a deferred-close file,
        // vnode_isinuse() won't indicate it's busy.
        
        CloseDeferredFileRefs(svpp, "vnop_remove", 0, context);
    }
    
    /*
     * Did we open this in our read routine. Then we should close it.
     */
    if ((np->f_refcnt == 1) && np->f_needClose) {
        error = smbfs_close(share, vp, FREAD, context);
        if (error) {
            SMBWARNING_LOCK(np, "error %d closing %s\n", error, np->n_name);
        }
    }
    
    /*
     * The old code would check vnode_isinuse to see if the file was open,
     * but if the file was open by Kqueue then vnode_isinuse will not find it.
     * So at this point if the file is open then do the silly rename delete
     * trick if the server supports it.
     */
    if (np->f_refcnt) {
        error = smbfs_delete_openfile(share, dnp, np, context);
        SMB_LOG_KTRACE(SMB_DBG_SMBFS_REMOVE | DBG_FUNC_NONE,
                       0xabc001, error, 0, 0, 0);
        if (!error) {
            /* skip doing the vnode_recycle as file may still be on server */
            error = 0;
            goto done;
        }
        
        goto out;
    }
    
    /*
     * NetApp and Samba servers will reuse the File ID immediately after a
     * delete, so remove the smb node from the hash table before doing the
     * delete. If the delete fails, then we know the File ID is still valid
     * and it should be ok to reinsert the smbnode.
     */
    smb_vhashrem(np);
    if (svpp != NULL) {
        /* Remove the resource fork vnode too */
        smb_vhashrem(VTOSMB(svpp));
    }
    
    error = smbfs_smb_delete(share, np, VREG,
                             NULL, 0,
                             0, context);
    SMB_LOG_KTRACE(SMB_DBG_SMBFS_REMOVE | DBG_FUNC_NONE,
                   0xabc002, error, 0, 0, 0);
    if (error) {
        /*
         * Delete failed, add smbnode back into hash table.
         * Then leave as there is nothing else we can do at this point
         */
        lck_rw_lock_shared(&np->n_name_rwlock);
        hashval = smbfs_hash(share, np->n_ino, np->n_name, np->n_nmlen);
        lck_rw_unlock_shared(&np->n_name_rwlock);
        
        smb_vhashadd(np, hashval);
        if (svpp != NULL) {
            /* Add back in the resource fork vnode too */
            smb_vhashadd(VTOSMB(svpp), hashval);
        }
        
        goto out;
    }
    
    smbfs_attr_touchdir(dnp, (share->ss_fstype == SMB_FS_FAT));

	/* <33469405> if dir has active lease skip local change notify */
	lck_mtx_lock(&dnp->d_dur_handle.lock);
	if (!(dnp->d_dur_handle.flags & SMB2_LEASE_GRANTED) ||
		(dnp->d_dur_handle.flags & SMB2_LEASE_BROKEN)) {
		dnp->d_changecnt++;
	}
	lck_mtx_unlock(&dnp->d_dur_handle.lock);
	
    /* Remove any negative cache entries. */
    if (dnp->n_flag & NNEGNCENTRIES) {
        dnp->n_flag &= ~NNEGNCENTRIES;
        cache_purge_negatives(dvp);
    }
    
out:
    if (error == EBUSY) {
        char errbuf[32];
        
        (void)proc_name(proc_pid(p), &errbuf[0], 32);
        
        SMBWARNING_LOCK(np, "warning: pid %d(%.*s) unlink open file(%s)\n",
                        proc_pid(p), 32, &errbuf[0], np->n_name);
    }
    
    if (!error) {
        /* Not sure why we do this here. Leave it for now. */
        (void) vnode_recycle(vp);
        /* if success, blow away statfs cache */
        smp->sm_statfstime = 0;
    }
    
done:
    if (svpp != NULL) {
        smbnode_unlock(VTOSMB(svpp));
        vnode_put(svpp);
    }
    
    SMB_LOG_KTRACE(SMB_DBG_SMBFS_REMOVE | DBG_FUNC_END, error, 0, 0, 0, 0);
    return (error);
}

/*
 * smbfs_vnop_remove
 *
 * vnode_t a_dvp;
 * vnode_t a_vp;
 * struct componentname *a_cnp;
 * int a_flags;
 * vfs_context_t a_context;
 */
static int 
smbfs_vnop_remove(struct vnop_remove_args *ap)
{
	vnode_t dvp = ap->a_dvp;
	vnode_t vp = ap->a_vp;
	int32_t error;
	struct smb_share *share;

    if (vnode_isdir(vp)) {
        /*
         * <66220948> The unlink docs tell us that we are supposed to return
         * EPERM instead of EISDIR.
         */
        return (EPERM);
    }

    if (dvp == vp) {
		return (EINVAL);
    }

	/* Always put in the order of parent then child */
    if ((error = smbnode_lockpair(VTOSMB(dvp),
                                  VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK))) {
		return (error);
    }

    SMB_LOG_KTRACE(SMB_DBG_REMOVE | DBG_FUNC_START, 0, 0, 0, 0, 0);

	VTOSMB(dvp)->n_lastvop = smbfs_vnop_remove;
	VTOSMB(vp)->n_lastvop = smbfs_vnop_remove;
	share = smb_get_share_with_reference(VTOSMBFS(dvp));
	error = smbfs_remove(share, dvp, vp, ap->a_cnp, ap->a_flags, ap->a_context);
	smb_share_rele(share, ap->a_context);
	smbnode_unlockpair(VTOSMB(dvp), VTOSMB(vp));

	SMB_LOG_KTRACE(SMB_DBG_REMOVE | DBG_FUNC_END, error, 0, 0, 0, 0);
    return (error);
}

/*
 * smbfs remove directory call
 *
 * The calling routine must hold a reference on the share
 *
 */
static int 
smbfs_rmdir(struct smb_share *share, vnode_t dvp, vnode_t vp, 
			struct componentname *cnp, vfs_context_t context)
{
#pragma unused(cnp)
	struct smbmount *smp = VTOSMBFS(vp);
	struct smbnode *dnp = VTOSMB(dvp);
	struct smbnode *np = VTOSMB(vp);
	int error;
    SMBFID fid = 0;
    uint64_t hashval = 0;

	/* XXX other OSX fs test fs nodes here, not vnodes. Why? */
	if (dvp == vp) {
		error = EINVAL;
		goto bad;
	}

    /* Close Query Dir Create FID if we need to */
    if ((np->d_fctx != NULL) && (np->d_fctx->f_need_close == TRUE)) {
        error = smb2_smb_close_fid(np->d_fctx->f_share,
                                   np->d_fctx->f_create_fid,
                                   NULL, NULL, NULL, context);
        if (error) {
            SMBDEBUG("smb2_smb_close_fid failed %d\n", error);
        }
        np->d_fctx->f_need_close = FALSE;
    }

    if ((np->d_rdir_fctx != NULL) && (np->d_rdir_fctx->f_need_close == TRUE)) {
        error = smb2_smb_close_fid(np->d_rdir_fctx->f_share,
                                   np->d_rdir_fctx->f_create_fid,
                                   NULL, NULL, NULL, context);
        if (error) {
            SMBDEBUG("smb2_smb_close_fid failed %d\n", error);
        }
        np->d_rdir_fctx->f_need_close = FALSE;
    }

    /* Free the enumeration cache if it exists */
    smb_dir_cache_remove(vp, &np->d_main_cache, "main", "smbfs_rmdir", 0);
    smb_dir_cache_remove(vp, &np->d_overflow_cache, "overflow", "smbfs_rmdir", 0);
    
	/* Remove it from the global dir enum cache */
	smb_global_dir_cache_remove_one(vp, 0);

	/*
	 * We may still have a change notify on this node, close it so
	 * the item will get deleted on the server. Mark it not to be
	 * reopened first, then save off the fid, clear the node fid
	 * now close the file descriptor.
	 */
	np->d_needReopen = FALSE;
	fid = np->d_fid;
	np->d_fid = 0;
	if (fid != 0) {
		(void)smbfs_tmpclose(share, np, fid, context);
	}
    
    cache_purge(vp);

    /*
     * NetApp and Samba servers will reuse the File ID immediately after a
     * delete, so remove the smb node from the hash table before doing the
     * delete. If the delete fails, then we know the File ID is still valid
     * and it should be ok to reinsert the smbnode.
     */
    smb_vhashrem(np);
        
	error = smbfs_smb_rmdir(share, np, context);
	if (error) {
        /*
         * Delete failed, add smbnode back into hash table.
         * Then leave as there is nothing else we can do at this point
         */
        lck_rw_lock_shared(&np->n_name_rwlock);
        hashval = smbfs_hash(share, np->n_ino, np->n_name, np->n_nmlen);
        lck_rw_unlock_shared(&np->n_name_rwlock);

        smb_vhashadd(np, hashval);
	    goto bad;
    }

	smbfs_attr_touchdir(dnp, (share->ss_fstype == SMB_FS_FAT));

	/* <33469405> if dir has active lease skip local change notify */
	lck_mtx_lock(&dnp->d_dur_handle.lock);
	if (!(dnp->d_dur_handle.flags & SMB2_LEASE_GRANTED) ||
		(dnp->d_dur_handle.flags & SMB2_LEASE_BROKEN)) {
		dnp->d_changecnt++;
	}
	lck_mtx_unlock(&dnp->d_dur_handle.lock);
	
	/* Remove any negative cache entries. */
	if (dnp->n_flag & NNEGNCENTRIES) {
		dnp->n_flag &= ~NNEGNCENTRIES;
		cache_purge_negatives(dvp);
	}
	
bad:
	/* if success, blow away statfs cache */
	if (!error) {
		smp->sm_statfstime = 0;
		(void) vnode_recycle(vp);
	}
	return (error);
}

/*
 * smbfs_vnop_rmdir
 *
 * vnode_t a_dvp;
 * vnode_t a_vp;
 * struct componentname *a_cnp;
 * vfs_context_t a_context;
 */
static int smbfs_vnop_rmdir(struct vnop_rmdir_args *ap)
{
	vnode_t dvp = ap->a_dvp;
	vnode_t vp = ap->a_vp;
	int32_t error;
	struct smb_share *share;

	if (!vnode_isdir(vp))
		return (ENOTDIR);

	if (dvp == vp)
		return (EINVAL);

	/* Always put in the order of parent then child */
	if ((error = smbnode_lockpair(VTOSMB(dvp), VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK)))
		return (error);

    SMB_LOG_KTRACE(SMB_DBG_RM_DIR | DBG_FUNC_START, 0, 0, 0, 0, 0);

	VTOSMB(dvp)->n_lastvop = smbfs_vnop_rmdir;
	VTOSMB(vp)->n_lastvop = smbfs_vnop_rmdir;
	share = smb_get_share_with_reference(VTOSMBFS(dvp));
	error = smbfs_rmdir(share, dvp, vp, ap->a_cnp, ap->a_context);
	smb_share_rele(share, ap->a_context);	
	smbnode_unlockpair(VTOSMB(dvp), VTOSMB(vp));

    SMB_LOG_KTRACE(SMB_DBG_RM_DIR | DBG_FUNC_END, error, 0, 0, 0, 0);
	return (error);
}

/*
 * smbfs_vnop_rename
 *
 * vnode_t a_fdvp;
 * vnode_t a_fvp;
 * struct componentname *a_fcnp;
 * vnode_t a_tdvp;
 * vnode_t a_tvp;
 * struct componentname *a_tcnp;
 * vfs_context_t a_context;
 */
static int 
smbfs_vnop_rename(struct vnop_rename_args *ap)
{
	vnode_t 	fvp = ap->a_fvp;
	vnode_t 	tvp = ap->a_tvp;
	vnode_t 	fdvp = ap->a_fdvp;
	vnode_t 	tdvp = ap->a_tdvp;
	struct smbmount *smp = VFSTOSMBFS(vnode_mount(fvp));
	struct smb_share *share = NULL;
	struct componentname *tcnp = ap->a_tcnp;
	struct componentname *fcnp = ap->a_fcnp;
	proc_t p = vfs_context_proc(ap->a_context);
	int error = 0;
	struct smbnode *fnp = NULL;
	struct smbnode *tdnp = NULL;
	struct smbnode *fdnp = NULL;
	int vtype;
	struct smbnode * lock_order[4] = {NULL};
	int	lock_cnt = 0;
	int ii;
    vnode_t from_par_vp = NULL;
    vnode_t to_par_vp = NULL;
	struct rename_lock_list *rename_child_listp = NULL;
	struct rename_lock_list *tptr = NULL;
	struct rename_lock_list *free_ptr = NULL;
	
    if (smp == NULL) {
        SMBERROR("smp is null \n");
        return (EINVAL);
    }

    /* Check for cross-device rename */
	if ((vnode_mount(fvp) != vnode_mount(tdvp)) || 
		(tvp && (vnode_mount(fvp) != vnode_mount(tvp))))
		return (EXDEV);
	
	vtype = vnode_vtype(fvp);
	if ( (vtype != VDIR) && (vtype != VREG) && (vtype != VLNK) )
		return (EINVAL);

	/*
	 * First lets deal with the parents. If they are the same only lock the from
	 * vnode, otherwise see if one is the parent of the other. We always want 
	 * to lock in parent child order if we can. If they are not the parent of 
	 * each other then lock in address order.
	 */
    
    if (fdvp != tdvp) {
        lck_rw_lock_shared(&VTOSMB(fdvp)->n_parent_rwlock);     /* do our own locking */
        from_par_vp = smbfs_smb_get_parent(VTOSMB(fdvp), 0);    /* do our own locking */
        if ((from_par_vp == NULL) &&
            (VTOSMB(fdvp)->n_parent_vid != 0)) {
            /* Parent got recycled already? */
            lck_rw_unlock_shared(&VTOSMB(fdvp)->n_parent_rwlock);

            SMBWARNING_LOCK(VTOSMB(fdvp), "Missing parent for <%s> (from)\n",
                            VTOSMB(fdvp)->n_name);
            return (EINVAL);
        }
        
        lck_rw_lock_shared(&VTOSMB(tdvp)->n_parent_rwlock);     /* do our own locking */
        to_par_vp = smbfs_smb_get_parent(VTOSMB(tdvp), 0);      /* do our own locking */
        if ((to_par_vp == NULL) &&
            (VTOSMB(tdvp)->n_parent_vid != 0)) {
            /* Parent got recycled already? */
            lck_rw_unlock_shared(&VTOSMB(tdvp)->n_parent_rwlock);
            lck_rw_unlock_shared(&VTOSMB(fdvp)->n_parent_rwlock);

            SMBWARNING_LOCK(VTOSMB(tdvp), "Missing parent for <%s> (to) \n",
                            VTOSMB(tdvp)->n_name);
            if (from_par_vp != NULL) {
                vnode_put(from_par_vp);
                from_par_vp = NULL;
            }
            return (EINVAL);
        }
    }
    
    SMB_LOG_KTRACE(SMB_DBG_RENAME | DBG_FUNC_START, 0, 0, 0, 0, 0);

    if (fdvp == tdvp)
		lock_order[lock_cnt++] = VTOSMB(fdvp);
	else if (from_par_vp && (VTOSMB(from_par_vp) == VTOSMB(tdvp))) {
		lock_order[lock_cnt++] = VTOSMB(tdvp);
		lock_order[lock_cnt++] = VTOSMB(fdvp);			
	} else if (to_par_vp && (VTOSMB(to_par_vp) == VTOSMB(fdvp))) {
		lock_order[lock_cnt++] = VTOSMB(fdvp);			
		lock_order[lock_cnt++] = VTOSMB(tdvp);				
	} else if (VTOSMB(fdvp) < VTOSMB(tdvp)) {
		lock_order[lock_cnt++] = VTOSMB(fdvp);
		lock_order[lock_cnt++] = VTOSMB(tdvp);
	} else {
		lock_order[lock_cnt++] = VTOSMB(tdvp);
		lock_order[lock_cnt++] = VTOSMB(fdvp);				
	}
    
	/*
	 * Now lets deal with the children. If any of the following is true then just 
	 * lock the from vnode:
	 *		1. The to vnode doesn't exist
	 *		2. The to vnode and from vnodes are the same
	 *		3. The to vnode and the from parent vnodes are the same, I know 
	 *		   it's strange but can happen.
	 * Otherwise, lock in address order
	 */
	if ((tvp == NULL) || (tvp == fvp) || (tvp == fdvp))
		lock_order[lock_cnt++] = VTOSMB(fvp);
	else {
		if (VTOSMB(fvp) < VTOSMB(tvp)) {
			lock_order[lock_cnt++] = VTOSMB(fvp);
			lock_order[lock_cnt++] = VTOSMB(tvp);
		} else {
			lock_order[lock_cnt++] = VTOSMB(tvp);
			lock_order[lock_cnt++] = VTOSMB(fvp);				
		}
	}
	/* Make sure there are no duplicates, this would be a design flaw */
	DBG_LOCKLIST_ASSERT(lock_cnt, lock_order);

    if (fdvp != tdvp) {
        lck_rw_unlock_shared(&VTOSMB(tdvp)->n_parent_rwlock);
        if (to_par_vp != NULL) {
            vnode_put(to_par_vp);
            to_par_vp = NULL;
        }

        lck_rw_unlock_shared(&VTOSMB(fdvp)->n_parent_rwlock);
        if (from_par_vp != NULL) {
            vnode_put(from_par_vp);
            from_par_vp = NULL;
        }
    }
		
	for (ii = 0; ii < lock_cnt; ii++) {
		if (error)
			lock_order[ii] = NULL;
		else if ((error = smbnode_lock(lock_order[ii], SMBFS_EXCLUSIVE_LOCK)))
			lock_order[ii] = NULL;
	}
	if (error)
		goto out;
    
    /*
     * Make sure that we do not proceed if we get lock after parent is
     * reclaimed after forced unmount.
     */
    if (vfs_isforce(smp->sm_mp)) {
        error = ENXIO;
        goto out;
    }

	fdnp = VTOSMB(fdvp);
	fnp = VTOSMB(fvp);
	tdnp = VTOSMB(tdvp);

	fdnp->n_lastvop = smbfs_vnop_rename;
	fnp->n_lastvop = smbfs_vnop_rename;
	tdnp->n_lastvop = smbfs_vnop_rename;
    if (tvp != NULL) {
		VTOSMB(tvp)->n_lastvop = smbfs_vnop_rename;
    }
    
    lck_rw_lock_shared(&fnp->n_parent_rwlock);  /* do our own locking */
    
    from_par_vp = smbfs_smb_get_parent(fnp, 0); /* do our own locking */
    if ((from_par_vp == NULL) &&
        (fnp->n_parent_vid != 0)) {
        /* Parent got recycled already? */
        SMBWARNING_LOCK(fnp, "Missing parent for <%s> \n", fnp->n_name);
        error = ENXIO;
        goto out;
    }
    
    if ((from_par_vp != NULL) &&
        (VTOSMB(from_par_vp) != VTOSMB(fdvp))) {
        /*
         * We lost the race with another thread that just
         * changed the parent of fvp, so fdvp is stale and
         * we punt. See <rdar://problem/15313516>
         */
        lck_rw_unlock_shared(&fnp->n_parent_rwlock);
        
        vnode_put(from_par_vp);
        from_par_vp = NULL;
        
        error = ENOENT;
		goto out;
    }
    
    lck_rw_unlock_shared(&fnp->n_parent_rwlock);
    
    if (from_par_vp != NULL) {
        vnode_put(from_par_vp);
        from_par_vp = NULL;
    }

	share = smb_get_share_with_reference(smp);
	/*
	 * Check to see if the SMB_EFA_RDONLY/IMMUTABLE are set. If they are set
	 * then do not allow the rename. See HFS and AFP code.
	 */
	if (node_isimmutable(share, fvp, NULL)) {
		SMBWARNING_LOCK(fnp, "%s is a locked file : Permissions error on delete\n",
                        fnp->n_name);
		error = EPERM;
		goto out;
	}
	 
	/*
	 * Since there are no hard links (from our client point of view)
	 * fvp==tvp means the arguments are case-variants.  (If they
	 * were identical the rename syscall doesnt call us.)
	 */
	if (fvp == tvp)
		tvp = NULL;
	
	/*
	 * The problem we have here is some servers will not return the correct
	 * case of the file name in the lookup. This can cause to have two vnodes
	 * that point to the same item. Very bad but nothing we can do about that
	 * in smb. So if the target exist, the parents are the same and the name is
	 * the same except for case, then don't delete the target.
	 */
    if (SS_TO_SESSION(share)->session_misc_flags & SMBV_HAS_FILEIDS) {
        /* Server supports File IDs */
        if (tvp && (fdvp == tdvp) && (fnp->n_ino == VTOSMB(tvp)->n_ino)) {
            SMBWARNING_LOCK(fnp, "Not removing target, same file. %s \n",
                            fnp->n_name);

            smb_vhashrem(VTOSMB(tvp)); /* Remove it from our hash so it can't be found */
            (void) vnode_recycle(tvp);
            tvp = NULL;
        }
    }
    else {
        /* Server does not support File IDs */
        lck_rw_lock_shared(&fnp->n_name_rwlock);
        
        if (tvp) {
            lck_rw_lock_shared(&VTOSMB(tvp)->n_name_rwlock);
        }

        if (tvp && (fdvp == tdvp) && (fnp->n_nmlen == VTOSMB(tvp)->n_nmlen) &&
                    (strncasecmp((char *)fnp->n_name, (char *)VTOSMB(tvp)->n_name, 
                                 fnp->n_nmlen) == 0)) {
            SMBWARNING("Not removing target, same file. %s ==> %s\n",
                       fnp->n_name, VTOSMB(tvp)->n_name);
            lck_rw_unlock_shared(&VTOSMB(tvp)->n_name_rwlock);

            smb_vhashrem(VTOSMB(tvp)); /* Remove it from our hash so it can't be found */
            (void) vnode_recycle(tvp);
            tvp = NULL;
        }
        
        if (tvp) {
            /* tvp might have been unlocked and null'd out above */
            lck_rw_unlock_shared(&VTOSMB(tvp)->n_name_rwlock);
        }
        
        lck_rw_unlock_shared(&fnp->n_name_rwlock);
    }

	/*
	 * If we are not doing Named Streams, they gave us a target to delete, and
	 * the source is a dot underscore file then make sure the source exist before
	 * deleting the target.
	 *
	 * This problem happens when going Mac to Mac and the share is FAT or some
	 * share that doesn't support streams. The remote VNOP_RENAME will rename
	 * the dot underscore file underneath the client. So when the cleint tries 
	 * to rename the dot underscore it thinkis the target exist and needs to be 
	 * deleted. So never delete the target if the source doesn't exist.
	 */
    lck_rw_lock_shared(&fnp->n_name_rwlock);
	if ((tvp) && (!(share->ss_attributes & FILE_NAMED_STREAMS)) &&
		(fnp->n_nmlen > 2) && (fnp->n_name[0] == '.') && (fnp->n_name[1] == '_')) {
		const char *name = (const char *)fnp->n_name;
		size_t nmlen = fnp->n_nmlen;
		struct smbfattr fattr;
		
		error = smbfs_lookup(share, fdnp, &name, &nmlen, &fattr, ap->a_context);
		/* Should we check for any error or just ENOENT */
		if (error == ENOENT)
			tvp = NULL;
		/* smbfs_lookup could have replace the name, free it if did */
		if (name != (char *)fnp->n_name) {
			SMB_FREE(name, M_SMBNODENAME);
		}
	}
    lck_rw_unlock_shared(&fnp->n_name_rwlock);
		
	/*
	 * If the destination exists then it needs to be removed.
	 */
	if (tvp) {
		if (vnode_isdir(tvp)) {
			/* 
			 * From the man 2 rename
			 *
			 * CONFORMANCE
			 * The restriction on renaming a directory whose permissions disallow 
			 * writing is based on the fact that UFS directories contain a ".." 
			 * entry.  If renaming a directory would move it to another parent 
			 * directory, this entry needs to be changed.
			 * 
			 * This restriction has been generalized to disallow renaming of any 
			 * write-disabled directory, even when this would not require a 
			 * change to the ".." entry.  For consistency, HFS+ directories 
			 * emulate this behavior.
			 * 
			 * So if you are renaming a dir to an existing dir, you must have 
			 * write access on the existing dir. Seems if the user is a super 
			 * user then we can delete the existing directory.
			 *
			 * XXX - We need to remove the smb_check_posix_access and just do a 
			 * vnop_access.
			 */
			if (share->ss_attributes & FILE_PERSISTENT_ACLS)
				error = smbfs_rmdir(share, tdvp, tvp, tcnp, ap->a_context);
			else if ((vfs_context_suser(ap->a_context) == 0) || 
					 (smb_check_posix_access(ap->a_context, VTOSMB(tvp), S_IWOTH)))
				error = smbfs_rmdir(share, tdvp, tvp, tcnp, ap->a_context);
			else
				error = EPERM;
		} else {
			error = smbfs_remove(share, tdvp, tvp, tcnp, 0, ap->a_context);
		}
		if (error)
			goto out;
	}

	cache_purge(fvp);

	/* Did we open this in our read routine. Then we should close it. */
	if ((!vnode_isdir(fvp)) && (!vnode_isinuse(fvp, 0)) && 
		(fnp->f_refcnt == 1) && fnp->f_needClose) {
		error = smbfs_close(share, fvp, FREAD, ap->a_context);
		if (error) {
			SMBWARNING_LOCK(fnp, "error %d closing %s\n", error, fnp->n_name);
        }
	}
	
    /* Are we renaming a directory? */
    if (vnode_isdir(fvp)) {
        /*
         * Do not allow Change Notifies to be restarted until after we attempt
         * the rename of a folder so we can close all the children's Change
         * Notify dir handles.
         */
        smp->sm_flags |= MNT_CHANGE_NOTIFY_PAUSE;

        /*
         * Windows Servers will not let you move a item that contains open items. So
         * if we are moving an folder and the source folder has an open notification
         * then close the notification, before doing the move.
         */
        if (fnp->d_fid != 0) {
            (void)smbfs_tmpclose(share, fnp, fnp->d_fid, ap->a_context);
            /* Mark it to be reopen */
            fnp->d_needReopen = TRUE;
            fnp->d_fid = 0;
        }
		
		/* 
		 * Enumerations also have the dir open, so halt them too. Since we
		 * have exclusive lock on this dir, enumerations wont start up again
		 * until we finish.
		 */
        smbfs_closedirlookup(fnp, 0, "vnop_rename from", ap->a_context);
        smbfs_closedirlookup(fnp, 1, "vnop_rename from", ap->a_context);

        /*
         * Windows Servers will not let you rename a folder if any of its 
         * children dir are open due to a Change Notify. Go find all of its
         * children dirs and close them which will cancel the Change Notify.
         */
        smbfs_CloseChildren(share, fnp, 1, &rename_child_listp, ap->a_context);
		
		/* Stop enumerations */
		tptr = rename_child_listp;
		while (tptr != NULL) {
			if ((smbnode_lock(tptr->np, SMBFS_EXCLUSIVE_LOCK))) {
				SMBERROR_LOCK(tptr->np, "failed to lock <%s> \n", tptr->np->n_name);
				error = EBUSY;
				goto out;
			}
			else {
				tptr->is_locked = 1;
				
				/*
				 * Enumerations also have the dir open, so halt them too. Since we
				 * have exclusive lock on this dir, enumerations wont start up again
				 * until we finish.
				 */
                smbfs_closedirlookup(tptr->np, 0, "vnop_rename to", ap->a_context);
                smbfs_closedirlookup(tptr->np, 1, "vnop_rename to", ap->a_context);
			}

			tptr = tptr->next;
		}
    }
	else {
		/* Close any deferred file handles */
		CloseDeferredFileRefs(fvp, "vnop_rename", 0, ap->a_context);
	}
    
    /*
	 * Try to rename the file, this may fail if the file is open. Some 
	 * SAMBA systems allow us to rename an open file, so try this case
	 * first. 
	 *
	 * FYI: While working on Radar 4532498 I notice that you can move/rename
	 * an open file if it is open for read-only. Only tested this on Windows 2003.
	 */  
	error = smbfs_smb_rename(share, fnp, tdnp, tcnp->cn_nameptr, 
							 tcnp->cn_namelen, ap->a_context);
    
	/* 
	 * The file could be open so lets try again. This call does not work on
	 * Windows 95/98/Me/NT4 systems. Since this call only allows you to
	 * rename in place do not even try it if they are moving the item.
	 */
	if ( (fdvp == tdvp) && error && 
		(SESSION_CAPS(SS_TO_SESSION(share)) & SMB_CAP_INFOLEVEL_PASSTHRU))
		error = smbfs_smb_t2rename(share, fnp, tcnp->cn_nameptr, 
								   tcnp->cn_namelen, 1, NULL, ap->a_context);

    if (vnode_isdir(fvp)) {
        /* Allow Change Notify to start up again on any child dirs */
        smp->sm_flags &= ~MNT_CHANGE_NOTIFY_PAUSE;
		
		/* Allow enumerations */
		tptr = rename_child_listp;
		while (tptr != NULL) {
			/* If its locked still, then unlock it now */
			if (tptr->is_locked == 1) {
				smbnode_unlock(tptr->np);
				tptr->is_locked = 0;
			}
			
			tptr = tptr->next;
		}
    }

    /*
	 * We should really print a better description of the error to the 
	 * system log. Thing we could print in the system log.
	 * 1. This server does not support renaming of an open file.
	 * 2. This server only supports renaming of an open file in place.
	 * 3. Renaming open file failed.
	 * 
	 * Written up as Radar 4381236
	 */
	if (!error) {
		char *new_name;
		uint64_t hashval;
		uint32_t orig_flag = fnp->n_flag;
		
        if (SS_TO_SESSION(share)->session_misc_flags & SMBV_MNT_HIGH_FIDELITY) {
            /*
             * For HiFi, the name is returned in vnop_getattr() and the name is
             * now different. Need to invalidate the meta data cache.
             */
            fnp->attribute_cache_timer = 0;
        }

        /*
		 * At this point we still have both parents and the children locked if 
		 * they exist. Since the parents are locked we know that a lookup of the 
		 * children cannot happen over the network. So we are safe to play with 
		 * both names and the hash node entries. The old code would just remove 
		 * the node from the hash table and then just change the nodes name, this
		 * was very bad and could cause the following to happen:
		 *
		 * RENAME HAPPENS ON VP1: vp1->np1->n_name = file1 gets renamed to vp1->np1->n_name = file2
		 *	1. vp1 is no longer in the name cache.
		 *  2. np1 is no longer in the hash table.
		 *  3. vp1 still has a ref taken on it and can still be used.
		 * 
		 * LOOKUP HAPPNES ON file2: Which will cause a new vnode and smbnode to get created
		 *  1. vp1 is not found because its not in the name cache.
		 *  2. np1 is not found because its not in the hash table.
		 *  3. vp2 and np2 get created and vp2->np2->n_name = file2
		 *
		 * RENAME HAPPENS ON VP2: vp2->np2->n_name = file2 gets renamed to vp2->np2->n_name = file3
		 *	1. vp1 no longer has the correct name.
		 *	2. vp2 is no longer in the name cache.
		 *  3. np2 is no longer in the hash table.
		 *  4. vp2 still has a ref taken on it and can still be used.
		 *	
		 * SOME OTHER OPERATION HAPPENS ON VP1: It will fail because VP1 now has the wrong name.
		 *	1. Now the whole thing can repeat, very bad! 
		 */
        
        if (!(SS_TO_SESSION(share)->session_misc_flags & SMBV_HAS_FILEIDS)) {
            /* 
             * Server does not support File IDs.
             * Remove it from the hash, it doesn't exist under that name 
             * anymore 
             */
            smb_vhashrem(fnp);
        }
        
		/* We always need the following code. */
		if (tdvp && (fdvp != tdvp)) {
            lck_rw_lock_exclusive(&fnp->n_parent_rwlock);   /* do our own locking */

			/* Take a ref count on the new parent */
			if (!vnode_isvroot(tdvp)) {
				if (vnode_get(tdvp) == 0) {
					if (vnode_ref(tdvp) == 0) {
						fnp->n_flag |= NREFPARENT;
                        
                        /* Increment new parent node's child refcnt */
                        OSIncrementAtomic(&tdnp->n_child_refcnt);
                    }
					else {
						fnp->n_flag &= ~NREFPARENT;
                    }
					vnode_put(tdvp);
				}
			}
			else 
				fnp->n_flag &= ~NREFPARENT;

			/* Remove the ref count off the old parent */
			if ((!vnode_isvroot(fdvp)) && (orig_flag & NREFPARENT)) {
				if (vnode_get(fdvp) == 0) {
					vnode_rele(fdvp);
					vnode_put(fdvp);
                    
                    /* Remove the child refcnt from old parent */
                    OSDecrementAtomic(&fdnp->n_child_refcnt);
				}
			}
            
            fnp->n_parent_vnode = tdvp;
            fnp->n_parent_vid = vnode_vid(tdvp);

            lck_rw_unlock_exclusive(&fnp->n_parent_rwlock);
		}
		
		/* 
		 * Now reset the name so path lookups will work and add the node back 
		 * into the hash table, so other lookups can this node. 
		 */
		new_name = smb_strndup(tcnp->cn_nameptr, tcnp->cn_namelen);
		if (new_name) {
			char * old_name = NULL;
            
			lck_rw_lock_exclusive(&fnp->n_name_rwlock);
			old_name = fnp->n_name;
			fnp->n_name = new_name;
			fnp->n_nmlen = tcnp->cn_namelen;

            if (!(SS_TO_SESSION(share)->session_misc_flags & SMBV_HAS_FILEIDS)) {
                /* Server does not support File IDs */
                hashval = smbfs_hash(NULL, 0, fnp->n_name, fnp->n_nmlen);
                smb_vhashadd(fnp, hashval);
            }
            
			lck_rw_unlock_exclusive(&fnp->n_name_rwlock);

			/* Now its safe to free the old name */
			SMB_FREE(old_name, M_SMBNODENAME);
		}
        
        if (!(SS_TO_SESSION(share)->session_misc_flags & SMBV_HAS_FILEIDS)) {
            /*
             * Servers does not support File IDs
             * <10854595> Update the n_ino with the new name and/or parent 
             */
            from_par_vp = smbfs_smb_get_parent(fnp, kShareLock);
            if (from_par_vp != NULL) {
                lck_rw_lock_shared(&fnp->n_name_rwlock);
                fnp->n_ino = smbfs_getino(VTOSMB(from_par_vp), fnp->n_name, fnp->n_nmlen);
                lck_rw_unlock_shared(&fnp->n_name_rwlock);
                
                vnode_put(from_par_vp);
            }
            else {
                /* This should be impossible */
                SMBERROR_LOCK(fnp, "Missing parent for <%s> \n", fnp->n_name);
                fnp->n_ino = 0;
            }
        }
        
        /* Update time stamp of when we last did rename */
        nanouptime(&fnp->n_rename_time);
    }
	
	/*
	 *	Source			Target
	 *	Dot	Hidden		Dot	HIDE
	 *	Dot	Unhidden	Dot	HIDE! (Puma recovery)
	 *	NoDot	Hidden		Dot	HIDE (Win hid it)
	 *	NoDot	Unhidden	Dot	HIDE
	 *	Dot	Hidden		NoDot	UNHIDE
	 *	Dot	Unhidden	NoDot	UNHIDE
	 *	NoDot	Hidden		NoDot	HIDE! (Win hid it)
	 *	NoDot	Unhidden	NoDot	UNHIDE
	 */
	if (!error) {
		int hiderr;
		Boolean hideItem = (tcnp->cn_nameptr[0] == '.');
		Boolean unHideItem = ((tcnp->cn_nameptr[0] != '.') && 
							  (fcnp->cn_nameptr[0] == '.'));
		
		if (hideItem || unHideItem) {
			hiderr = smbfs_set_hidden_bit(share, tdnp, (vtype == VDIR) ? VDIR : VREG,
                                          tcnp->cn_nameptr, tcnp->cn_namelen,
                                          hideItem, ap->a_context);
			if (hiderr) {
				SMBWARNING("Error %d %s %s\n", hiderr, (hideItem) ? "hiding" : 
						 "unhiding", tcnp->cn_nameptr);
			}
		}
	}

	if (error == EBUSY) {
		char errbuf[32];

		proc_name(proc_pid(p), &errbuf[0], 32);

        SMBERROR_LOCK(fnp, "warning: pid %d(%.*s) rename open file(%s)\n", proc_pid(p),
                      32, &errbuf[0], fnp->n_name);
	}
    
	smbfs_attr_touchdir(fdnp, (share->ss_fstype == SMB_FS_FAT));

	/* <33469405> if dir has active lease skip local change notify */
	lck_mtx_lock(&fdnp->d_dur_handle.lock);
	if (!(fdnp->d_dur_handle.flags & SMB2_LEASE_GRANTED) ||
		(fdnp->d_dur_handle.flags & SMB2_LEASE_BROKEN)) {
		fdnp->d_changecnt++;
	}
	lck_mtx_unlock(&fdnp->d_dur_handle.lock);
	
    if (tdvp != fdvp) {
		smbfs_attr_touchdir(tdnp, (share->ss_fstype == SMB_FS_FAT));

		/* <33469405> if dir has active lease skip local change notify */
		lck_mtx_lock(&tdnp->d_dur_handle.lock);
		if (!(tdnp->d_dur_handle.flags & SMB2_LEASE_GRANTED) ||
			(tdnp->d_dur_handle.flags & SMB2_LEASE_BROKEN)) {
			tdnp->d_changecnt++;
		}
		lck_mtx_unlock(&tdnp->d_dur_handle.lock);
    }
    
	/* if success, blow away statfs cache */
	if (!error) {
		smp->sm_statfstime = 0;
        
        /* Invalidate negative cache entries in destination dir */
		if (tdnp->n_flag & NNEGNCENTRIES) {
			tdnp->n_flag &= ~NNEGNCENTRIES;
			cache_purge_negatives(tdvp);
		}
	}
	
out:
	/* We only have a share if we obtain a reference on it, so release it */
	if (share) {
		smb_share_rele(share, ap->a_context);
	}
	
	if (error == EBUSY) {
		char errbuf[32];

		proc_name(proc_pid(p), &errbuf[0], 32);

		SMBWARNING_LOCK(fnp, "warning: pid %d(%.*s) rename open file(%s)\n",
                        proc_pid(p), 32, &errbuf[0], fnp->n_name);
	}
	
	/* Clean up the child list */
	tptr = rename_child_listp;
	while (tptr != NULL) {
		/* If its locked still, then unlock it now */
		if (tptr->is_locked == 1) {
			smbnode_unlock(tptr->np);
			tptr->is_locked = 0;
		}
		
		/* Free list element */
		free_ptr = tptr;
		tptr = tptr->next;
		SMB_FREE(free_ptr, M_SMBTEMP);
	}

	for (ii = 0; ii < lock_cnt; ii++)
		if (lock_order[ii])
			smbnode_unlock(lock_order[ii]);
				
    SMB_LOG_KTRACE(SMB_DBG_RENAME | DBG_FUNC_END, error, 0, 0, 0, 0);
	return (error);
}

/*
 * smbfs_vnop_link
 *
 * vnode_t a_vp;
 * vnode_t a_tdvp;
 * struct componentname *a_cnp;
 * vfs_context_t a_context;
 *
 * someday this will come true...
 */
static int 
smbfs_vnop_link(struct vnop_link_args *ap)
{
	proc_t p = vfs_context_proc(ap->a_context);
	struct smbnode *np = VTOSMB(ap->a_vp);
	char errbuf[32];

    SMB_LOG_KTRACE(SMB_DBG_LINK | DBG_FUNC_START, 0, 0, 0, 0, 0);

	proc_name(proc_pid(p), &errbuf[0], 32);

    SMBERROR_LOCK(np, "warning: pid %d(%.*s) hardlink(%s)\n",
                  proc_pid(p), 32, &errbuf[0], np->n_name);

    SMB_LOG_KTRACE(SMB_DBG_LINK | DBG_FUNC_END, 0, 0, 0, 0, 0);
	return (err_link(ap));
}

/*
 * smbfs_vnop_symlink link create call.
 * 
 * vnode_t a_dvp;
 * vnode_t *a_vpp;
 * struct componentname *a_cnp;
 * struct vnode_attr *a_vap;
 *  char *a_target;
 *  vfs_context_t a_context;
 */
static int smbfs_vnop_symlink(struct vnop_symlink_args *ap)
{
	int error;
	struct vnop_create_args a;
	vnode_t 	dvp = ap->a_dvp;
	struct smbnode *dnp;
	struct smb_share *share;
	
	/* Make sure we lock the parent before calling smbfs_create */
	if ((error = smbnode_lock(VTOSMB(dvp), SMBFS_EXCLUSIVE_LOCK)))
		return error;
	
    SMB_LOG_KTRACE(SMB_DBG_SYM_LINK | DBG_FUNC_START, 0, 0, 0, 0, 0);

	dnp = VTOSMB(dvp);
	dnp->n_lastvop = smbfs_vnop_symlink;
	a.a_dvp = dvp;
	a.a_vpp = ap->a_vpp;
	a.a_cnp = ap->a_cnp;
	a.a_vap = ap->a_vap;
	a.a_context = ap->a_context;
	/* 
	 * We use PATH_MAX because we have no way currently to find out what is the 
	 * max path on the server.
	 */
	share = smb_get_share_with_reference(VTOSMBFS(dvp));
    
	error = smbfs_create(share, &a, ap->a_target, strnlen(ap->a_target, PATH_MAX+1));
    
	smb_share_rele(share, ap->a_context);
	smbnode_unlock(dnp);

	SMB_LOG_KTRACE(SMB_DBG_SYM_LINK | DBG_FUNC_END, error, 0, 0, 0, 0);
    return (error);
}

int
smbfs_readlink(struct smb_share *share, vnode_t vp, struct uio *uiop,
			   vfs_context_t context)
{
	struct smbnode *np = NULL;
	int error = 0;
	time_t attrtimeo;
	struct timespec ts;
	int use_cached_data = 0;

	np = VTOSMB(vp);
	if (np == NULL) {
		SMBERROR("np is NULL \n");
		return (EINVAL);
	}
	
	/* If we are in reconnect, use cached data if we have it */
	if (np->n_symlink_cache_timer != 0) {
		use_cached_data = (share->ss_flags & SMBS_RECONNECTING);
	}
	
	/* Check to see if the cache has timed out */
	SMB_CACHE_TIME(ts, np, attrtimeo);
	
	if ((np->n_symlink_target) &&
		(use_cached_data || ((ts.tv_sec - np->n_symlink_cache_timer) <= attrtimeo))) {
		/* Cached data is still valid */
		if (uiop != NULL) {
			error = uiomove(np->n_symlink_target,
							(int)np->n_symlink_target_len,
							uiop);
		}
	} else if ((np->n_dosattr & SMB_EFA_REPARSE_POINT) &&
			   (np->n_reparse_tag == IO_REPARSE_TAG_SYMLINK)) {
		error = smbfs_smb_reparse_read_symlink(share, np, uiop, context);
	} else if (np->n_flag & NWINDOWSYMLNK) {
		error = smbfs_smb_windows_read_symlink(share, np, uiop, context);
	} else {
		error = smbfs_smb_unix_read_symlink(share, np, uiop, context);
	}
	
	if ((error == ETIMEDOUT) && (np->n_symlink_cache_timer != 0)) {
		/* Just return the cached data */
		if (uiop != NULL) {
			error = uiomove(np->n_symlink_target,
							(int) np->n_symlink_target_len,
							uiop);
		}
	}
	
	return (error);
}

/*
 * smbfs_vnop_readlink read symlink call.
 *
 * vnode_t *a_vpp;
 * struct componentname *a_cnp;
 * uio_t a_uio; 
 *  vfs_context_t a_context;
 */
static int 
smbfs_vnop_readlink(struct vnop_readlink_args *ap)
{
	vnode_t vp = ap->a_vp;
	struct smbnode *np = NULL;
	int error;
	struct smb_share *share;

	if (vnode_vtype(vp) != VLNK) {
		return (EINVAL);
	}

	if ((error = smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK))) {
		return (error);
	}
	
    SMB_LOG_KTRACE(SMB_DBG_READ_LINK | DBG_FUNC_START, 0, 0, 0, 0, 0);

	np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_readlink;
	share = smb_get_share_with_reference(VTOSMBFS(vp));

	error = smbfs_readlink(share, vp, ap->a_uio, ap->a_context);
	
	smb_share_rele(share, ap->a_context);
	smbnode_unlock(np);
    
    SMB_LOG_KTRACE(SMB_DBG_READ_LINK | DBG_FUNC_END, error, 0, 0, 0, 0);
	return (error);
}

/*
 * smbfs_vnop_mknod
 *
 * vnode_t a_dvp;
 * vnode_t *a_vpp;
 * struct componentname *a_cnp;
 * struct vnode_attr *a_vap;
 * vfs_context_t a_context;
 */
static int 
smbfs_vnop_mknod(struct vnop_mknod_args *ap) 
{
	proc_t p = vfs_context_proc(ap->a_context);
	char errbuf[32];

    SMB_LOG_KTRACE(SMB_DBG_MKNODE | DBG_FUNC_START, 0, 0, 0, 0, 0);
    
	proc_name(proc_pid(p), &errbuf[0], 32);
	SMBERROR("warning: pid %d(%.*s) mknod(%s)\n", proc_pid(p), 32, &errbuf[0], 
			 ap->a_cnp->cn_nameptr);
    
    SMB_LOG_KTRACE(SMB_DBG_MKNODE | DBG_FUNC_END, 0, 0, 0, 0, 0);
	return (err_mknod(ap));
}

/*
 * smbfs_vnop_mkdir
 *
 * vnode_t a_dvp;
 * vnode_t *a_vpp;
 * struct componentname *a_cnp;
 * struct vnode_attr *a_vap;
 * vfs_context_t a_context;
 */
static int 
smbfs_vnop_mkdir(struct vnop_mkdir_args *ap)
{
	vnode_t 	dvp = ap->a_dvp;
	struct vnode_attr *vap = ap->a_vap;
	vnode_t 	vp;
	struct componentname *cnp = ap->a_cnp;
	struct smbnode *dnp = NULL;
	struct smbmount *smp = NULL;
	struct smbfattr fattr;
	const char *name = cnp->cn_nameptr;
	size_t len = cnp->cn_namelen;
	int error;
	struct smb_share *share;
	struct timespec ts;

	if (name[0] == '.' && (len == 1 || (len == 2 && name[1] == '.')))
		return (EEXIST);

	if ((error = smbnode_lock(VTOSMB(dvp), SMBFS_EXCLUSIVE_LOCK)))
		return (error);

	SMB_LOG_KTRACE(SMB_DBG_MKDIR | DBG_FUNC_START, 0, 0, 0, 0, 0);

    dnp = VTOSMB(dvp);
	dnp->n_lastvop = smbfs_vnop_mkdir;
	smp = dnp->n_mount;
	share = smb_get_share_with_reference(smp);
	error = smbfs_smb_mkdir(share, dnp, name, len, &fattr, ap->a_context);
	if (error)
		goto exit;

	smbfs_attr_touchdir(dnp, (share->ss_fstype == SMB_FS_FAT));
	
	/* <33469405> if dir has active lease skip local change notify */
	lck_mtx_lock(&dnp->d_dur_handle.lock);
	if (!(dnp->d_dur_handle.flags & SMB2_LEASE_GRANTED) ||
		(dnp->d_dur_handle.flags & SMB2_LEASE_BROKEN)) {
		dnp->d_changecnt++;
	}
	lck_mtx_unlock(&dnp->d_dur_handle.lock);

	/* 
	 * %%%
	 * Would really like to clean this code up in the future. The whole create, 
	 * mkdir and smblink create code should use the same code path. 
	 */
	error = smbfs_nget(share, vnode_mount(dvp),
                       dvp, name, len,
                       &fattr, &vp,
                       cnp->cn_flags, SMBFS_NGET_CREATE_VNODE,
                       ap->a_context);
	if (error)
		goto bad;
	
	/* 
	 * We just create the directory, so we have no finder info. So set our cache 
	 * timer to reflect this information
	 */
	nanouptime(&ts);
	VTOSMB(vp)->finfo_cache_timer = ts.tv_sec;

    /* Attempt to set the inital vnode_attrs */
	smbfs_set_create_vap(share, vap, vp, ap->a_context, FALSE);

    /* If HiFi and Super Guest mode, attempt to set the uid/gid */
    smbfs_hifi_set_perms(share, vp, ap->a_context);

    *ap->a_vpp = vp;
	smbnode_unlock(VTOSMB(vp));	/* Done with the smbnode unlock it. */
	error = 0;

	if (dnp->n_flag & NNEGNCENTRIES) {
		dnp->n_flag &= ~NNEGNCENTRIES;
		cache_purge_negatives(dvp);
	}
	
bad:
	if (name != cnp->cn_nameptr) {
		SMB_FREE(name, M_SMBNODENAME);
	}
	/* if success, blow away statfs cache */
	smp->sm_statfstime = 0;
exit:
	smb_share_rele(share, ap->a_context);
	smbnode_unlock(dnp);
    
    SMB_LOG_KTRACE(SMB_DBG_MKDIR | DBG_FUNC_END, error, 0, 0, 0, 0);
	return (error);
}

/*
 * smbfs_vnop_readdir
 *
 * vnode_t a_vp;
 * struct uio *a_uio;
 * int a_flags;
 * int *a_eofflag;
 * int *a_numdirent;
 * vfs_context_t a_context;
 */
static int 
smbfs_vnop_readdir(struct vnop_readdir_args *ap)
{
	vnode_t	vp = ap->a_vp;
	uio_t uio = ap->a_uio;
	int error;
	int32_t numdirent = 0;

	if (uio_offset(uio) < 0)
		return (EINVAL);

	if (!vnode_isdir(vp))
		return (EPERM);

	if (ap->a_eofflag)
		*ap->a_eofflag = 0;
	
	if (uio_resid(uio) == 0)
		return (0);
    
	if ((error = smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK)))
		return (error);
		
	SMB_LOG_KTRACE(SMB_DBG_READ_DIR | DBG_FUNC_START, VTOSMB(vp)->d_fid, 0, 0, 0, 0);

	VTOSMB(vp)->n_lastvop = smbfs_vnop_readdir;
	
	error = smbfs_readvdir(vp, uio, ap->a_flags, &numdirent, ap->a_context);
	if (error == ENOENT) {
		/* We have reached the end of the search */
		if (ap->a_eofflag)
			*ap->a_eofflag = 1;
		error = 0;
	}
    
	/* Return the number of entries from this lookup */ 
	if (ap->a_numdirent)
		*ap->a_numdirent = numdirent;
    
	smbnode_unlock(VTOSMB(vp));
    
    SMB_LOG_KTRACE(SMB_DBG_READ_DIR | DBG_FUNC_END, error, numdirent, 0, 0, 0);
	return (error);
}

int32_t 
smbfs_fsync(struct smb_share *share, vnode_t vp, int waitfor, int ubc_flags, 
			vfs_context_t context)
{
#pragma unused(waitfor, ubc_flags)
	int error;
	off_t size;
	
	if (!vnode_isreg(vp)) {
		return 0; /* Nothing to do here */
	}
    
    SMB_LOG_KTRACE(SMB_DBG_SMBFS_FSYNC | DBG_FUNC_START, 0, 0, 0, 0, 0);
    
	size = smb_ubc_getsize(vp);
	if ((size > 0) && smbfsIsCacheable(vp)) {
		if (VTOSMB(vp)->n_flag & NISMAPPED) {
			/* More expensive, but handles mmapped files */
			ubc_msync (vp, 0, ubc_getsize(vp), NULL, UBC_PUSHDIRTY | UBC_SYNC);
		}
		else {
			/* Less expensive, but does not handle mmapped files */
			cluster_push(vp, IO_SYNC);
		}
	}
	error = smbfs_smb_fsync(share, VTOSMB(vp), 0, context);
	if (!error) {
		VTOSMBFS(vp)->sm_statfstime = 0;
	}

	SMB_LOG_KTRACE(SMB_DBG_SMBFS_FSYNC | DBG_FUNC_END, error, 0, 0, 0, 0);
    return (error);
}

/*	smbfs_vnop_fsync
 * 
 * vnode_t a_vp;
 * int32_t a_waitfor;
 * vfs_context_t a_context;
 */
static int32_t 
smbfs_vnop_fsync(struct vnop_fsync_args *ap)
{
	int32_t error;
	struct smb_share *share;
    
    /* If this is a read-only mount, there is nothing to do here */
    if (vfs_isrdonly(vnode_mount(ap->a_vp))) {
        return (0);
    }

	error = smbnode_lock(VTOSMB(ap->a_vp), SMBFS_EXCLUSIVE_LOCK);
	if (error)
		return (0);

	SMB_LOG_KTRACE(SMB_DBG_FSYNC | DBG_FUNC_START, 0, 0, 0, 0, 0);

    VTOSMB(ap->a_vp)->n_lastvop = smbfs_vnop_fsync;
	share = smb_get_share_with_reference(VTOSMBFS(ap->a_vp));

	/* Make sure a flush actually gets done and sent */
	VTOSMB(ap->a_vp)->n_flag |= NNEEDS_FLUSH;
	
	error = smbfs_fsync(share, ap->a_vp, ap->a_waitfor, 0, ap->a_context);
	smb_share_rele(share, ap->a_context);
	smbnode_unlock(VTOSMB(ap->a_vp));

    SMB_LOG_KTRACE(SMB_DBG_FSYNC | DBG_FUNC_END, error, 0, 0, 0, 0);
    return (error);
}

/*
 * smbfs_vnop_pathconf
 *
 *  vnode_t a_vp;
 *  int a_name;
 *  int32_t *a_retval;
 *  vfs_context_t a_context;
 */
static int smbfs_vnop_pathconf(struct vnop_pathconf_args *ap)
{
	struct smb_share *share;
	int32_t *retval = ap->a_retval;
	int error = 0;
	
    SMB_LOG_KTRACE(SMB_DBG_PATHCONF | DBG_FUNC_START, ap->a_name, 0, 0, 0, 0);

	share = smb_get_share_with_reference(VTOSMBFS(ap->a_vp));
	switch (ap->a_name) {
		case _PC_LINK_MAX:	/* Hard Link Support */
			*retval = 0; /* May support in the future. Depends on the server */
			break;
		case _PC_NAME_MAX:
			*retval = share->ss_maxfilenamelen;
			break;
		case _PC_PATH_MAX:
				/*
				 * XXX
				 * Most Windows system have a 255 limit, but you can configure
				 * them to support 1024. It would be nice if we could figure out
				 * the real value for this field.
				 */
				*retval = PATH_MAX;
				break;
		case _PC_CHOWN_RESTRICTED:
				*retval = 1;
				break;
		case _PC_NO_TRUNC:
				*retval = 0;
				break;
		case _PC_NAME_CHARS_MAX:
				*retval = share->ss_maxfilenamelen;
				break;
		case _PC_CASE_SENSITIVE:
            *retval = 0;

            if (SS_TO_SESSION(share)->session_misc_flags & SMBV_OSX_SERVER) {
                /* Its OS X Server so we know for sure */
                if (SS_TO_SESSION(share)->session_volume_caps & kAAPL_CASE_SENSITIVE) {
                    SMBDEBUG("Volume is case sensitive\n");
                    *retval = 1;
                }
            }
            else {
                /*
                 * Thought about using FILE_CASE_SENSITIVE_SEARCH, but this
                 * really just means they will search by case. It does not mean
                 * this is a case sensitive file system. Currently we have no
                 * way of determining if this is a case sensitive or
                 * insensitive file system. We need to return a consistent
                 * answer between pathconf and vfs_getattr. We do not know the real
                 * answer for case sensitive, but lets default to what 90% of the
                 * servers have set. Also remember this fixes Radar 4057391 and 3530751.
                 */
                *retval = 0;
            }
			break;
		case _PC_CASE_PRESERVING:
			if (share->ss_attributes & FILE_CASE_PRESERVED_NAMES)
				*retval = 1;
			else *retval = 0;
			break;
		/* 
		 * Handle by vn_pathconf.
		 *
		 * case _PC_EXTENDED_SECURITY_NP:
		 *		*retval = vfs_extendedsecurity(vnode_mount(vp)) ? 1 : 0;
		 *		break;
		 * case _PC_AUTH_OPAQUE_NP:
		 *		*retval = vfs_authopaque(vnode_mount(vp));
		 *		break;
		 * case _PC_2_SYMLINKS:
		 *		*retval = 1;
		 *		break;
		 * case _PC_ALLOC_SIZE_MIN:
		 *		*retval = 1;    // XXX lie: 1 byte
		 *		break;
		 * case _PC_ASYNC_IO:     // unistd.h: _POSIX_ASYNCHRONUS_IO
		 *		*retval = 1;    // [AIO] option is supported
		 *		break;
		 */
		case _PC_FILESIZEBITS:
			if (SESSION_CAPS(SS_TO_SESSION(share)) & SMB_CAP_LARGE_FILES)
				*retval = 64;	/* The server supports 64 bit offsets */
			else *retval = 32;	/* The server supports 32 bit offsets */
			break;
	    case _PC_XATTR_SIZE_BITS:
			if (!(share->ss_attributes & FILE_NAMED_STREAMS)) {
				/* Doesn't support named streams VFS handles this cases */ 
				error = EINVAL;
			} else if (SESSION_CAPS(SS_TO_SESSION(share)) & SMB_CAP_LARGE_FILES) {
				*retval = 64;	/* The server supports 64 bit offsets */
			} else {
				*retval = 32;	/* The server supports 32 bit offsets */
			}
			break;
		/* 
		 * Handle by vn_pathconf.
		 *
		 * case _PC_PRIO_IO:       // unistd.h: _POSIX_PRIORITIZED_IO
		 *		 *retval = 0;    // [PIO] option is not supported
		 *		break;
		 * case _PC_REC_INCR_XFER_SIZE:
		 *		*retval = 4096; // XXX go from MIN to MAX 4K at a time
		 *		break;
		 * case _PC_REC_MIN_XFER_SIZE:
		 *		*retval = 4096; // XXX recommend 4K minimum reads/writes
		 *		break;
		 * case _PC_REC_MAX_XFER_SIZE: // Should we use SMB_IOMAX
		 *		*retval = 65536; // XXX recommend 64K maximum reads/writes
		 *		break;
		 * case _PC_REC_XFER_ALIGN:
		 *		*retval = 4096; // XXX recommend page aligned buffers
		 *		break;
		 * case _PC_SYMLINK_MAX:
		 *		*retval = 255;  // Minimum acceptable POSIX value
		 *		break;
		 * case _PC_SYNC_IO:       // unistd.h: _POSIX_SYNCHRONIZED_IO
		 *		*retval = 0;    // [SIO] option is not supported
		 *		break;
		 */
		default:
			error = EINVAL;
	}
	smb_share_rele(share, ap->a_context);

    SMB_LOG_KTRACE(SMB_DBG_PATHCONF | DBG_FUNC_END, error, 0, 0, 0, 0);
    return (error);
}

/*
 * smbfs_vnop_ioctl - smbfs vnodeop entry point
 *	vnode_t a_vp;
 *	int32_t  a_command;
 *	caddr_t  a_data;
 *	int32_t  a_fflag;
 *	vfs_context_t context;
 */
static int32_t smbfs_vnop_ioctl(struct vnop_ioctl_args *ap)
{
    vnode_t vp = ap->a_vp;
    struct smbnode *np;
    int32_t error = 0;
	proc_t p = vfs_context_proc(ap->a_context);
	struct smb_share *share;
	struct smbmount *smp;
    kauth_cred_t cred;
	struct smb_session *sessionp = NULL;
	uint32_t timeout = 0;
	
	error = smbnode_lock(VTOSMB(ap->a_vp), SMBFS_EXCLUSIVE_LOCK);
	if (error)
		return (error);

    SMB_LOG_KTRACE(SMB_DBG_IOCTL | DBG_FUNC_START, ap->a_command, 0, 0, 0, 0);

	np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_ioctl;
	smp = VTOSMBFS(vp);
	share = smb_get_share_with_reference(VTOSMBFS(vp));

	if ((share == NULL) || (SS_TO_SESSION(share) == NULL)) {
		SMBERROR("share or sessionp is null \n");
		error = EINVAL;
		goto exit;
	}
	
	sessionp = SS_TO_SESSION(share);
	
	/*
	 * The smbfsByteRangeLock2FSCTL call was made to support classic. We do 
	 * not support classic, but the file manager will only make the 
	 * smbfsByteRangeLock2FSCTL call. So for now support all the commands, but 
	 * treat them all the same.
	 */
	switch (ap->a_command) {
        case smbfsByteRangeLock2FSCTL:
        case smbfsByteRangeLockFSCTL: {
            struct ByteRangeLockPB *pb = (struct ByteRangeLockPB *) ap->a_data;
            struct ByteRangeLockPB2 *pb2 = (struct ByteRangeLockPB2 *) ap->a_data;
            uint32_t lck_pid;
            uint32_t timo;
            SMBFID fid = 0;
            struct fileRefEntry *fndEntry = NULL;
            uint16_t accessMode = 0;
            int8_t flags;
            struct smbfattr *fap = NULL;
			struct smb2_durable_handle *create_dur_handlep = NULL;
			struct smb2_durable_handle *dur_handlep = NULL;
            int do_create;
            uint32_t disp;
            
            /* make sure its a file */
            if (vnode_isdir(vp)) {
                error = EISDIR;
                goto exit;
            }

            /* Before trying the lock see if the file needs to be reopened */
            error = smbfs_smb_reopen_file(share, np, ap->a_context);
            if (error) {
                SMBDEBUG_LOCK(np, " %s waiting to be revoked\n", np->n_name);
                goto exit;
            }
            
            /*
             * If adding/removing a ByteRangeLock, thus this vnode should NEVER 
             * be cacheable since the page in/out may overlap a lock and get 
             * an error.
             */
            if (smbfsIsCacheable(vp)) {
                if (np->n_flag & NISMAPPED) {
                    /* More expensive, but handles mmapped files */
                    ubc_msync (vp, 0, ubc_getsize(vp), NULL, UBC_PUSHDIRTY | UBC_SYNC);
                } else {
                    /* Less expensive, but does not handle mmapped files */
                    cluster_push(vp, IO_SYNC);
                }
                ubc_msync (vp, 0, ubc_getsize(vp), NULL, UBC_INVALIDATE);
                vnode_setnocache(vp);
            }
            
            accessMode = np->f_accessMode;
            
            /* 
             * Since we never get a smbfsByteRangeLockFSCTL call we could skip this 
             * check, but for now leave it. 
             */
            if (ap->a_command == smbfsByteRangeLock2FSCTL) {
                int32_t openMode = 0;
                    
                /* 
                 * Not just for classic any more, could get this from Carbon.
                 * 
                 * They will be using an extended BRL call that also passes in the 
                 * open mode used that was used to open the file.  I will use the 
                 * open access mode and pid to find the fork ref to do the lock on.
                 * Scenarios:
                 *	Open (R/W), BRL, Close (R/W)
                 *	Open (R/W/dW), BRL, Close (R/W/dW)
                 *	Open1 (R/W), Open2 (R), BRL1, BRL2, Close1 (R/W), Close2 (R)
                 *	Open1 (R/W/dW), Open2 (R), BRL1, BRL2, Close1 (R/W/dW), Close2 (R)
                 *	Open1 (R/dW), Open2 (R/dW), BRL1, BRL2, Close1 (R/dW), Close2 (R/dW) 
                 */
                if ( (error = file_flags(pb2->fd, &openMode)) ) {
                    goto exit;
                }

                if (openMode & FREAD) {
                    accessMode |= kAccessRead;
                }
                if (openMode & FWRITE) {
                    accessMode |= kAccessWrite;
                }
                   
                /* 
                 * See if we can find a matching fork that has byte range locks or 
                 * denyModes 
                 */
                if (openMode & FHASLOCK) {
                    /* 
                     * NOTE:  FHASLOCK can be set by open with O_EXCLUSIVE or  
                     * O_SHARED which maps to my deny modes or FHASLOCK could also 
                     * have been set/cleared by calling flock directly.  I assume 
                     * that if they are using byte range locks, then they are Carbon 
                     * and unlikely to be using flock.
                     * 
                     * Assume it was opened with denyRead/denyWrite or just denyWrite.
                     * Try denyWrite first, if not found, try with denyRead and denyWrite 
                     */
                    accessMode |= kDenyWrite;
                    error = FindFileRef(vp, p, accessMode, kExactMatch, 0, 0, 
                                        &fndEntry, &fid);
                    if (error != 0) {
                        accessMode |= kDenyRead;
                        error = FindFileRef(vp, p, accessMode, kExactMatch, 0, 0, 
                                             &fndEntry, &fid);
                    }
                    if (error != 0) {
                        /* deny modes were used, but the fork ref cant be found, return error */
                        error = EBADF;
                        goto exit;
                    }
                }
                else {
                    /* no deny modes used, look for any forks opened previously for BRL */
                    error = FindFileRef(vp, p, accessMode, kExactMatch, 0, 0, 
                                        &fndEntry, &fid);
                }
            } else {
                error = FindFileRef(vp, p, kAccessRead | kAccessWrite, kAnyMatch, 
                                     0, 0, &fndEntry, &fid);
            }
            
            if ((error == 0) && (fndEntry != NULL)) {
                /* Are we reusing a file ref kept open by a handle lease? */
                lck_mtx_lock(&fndEntry->dur_handlep->lock);
                
                if ((fndEntry->dur_handlep->flags & SMB2_LEASE_GRANTED) &&
                    (fndEntry->dur_handlep->lease_state & SMB2_LEASE_HANDLE_CACHING) &&
                    (fndEntry->dur_handlep->flags & SMB2_DEFERRED_CLOSE)) {
                    /*
                     * File ref was no longer in use, mark that its back in use
                     */
                    fndEntry->dur_handlep->flags &= ~SMB2_DEFERRED_CLOSE;
                    OSAddAtomic(-1, &share->ss_curr_def_close_cnt);
                    
                    OSAddAtomic(1, &fndEntry->dur_handlep->handle_reuse_cnt);
                    
                    /* Clear deferred close time */
                    fndEntry->dur_handlep->def_close_time.tv_sec = 0;
                    fndEntry->dur_handlep->def_close_time.tv_nsec = 0;

                    SMB_LOG_UNIT_TEST_LOCK(np, "FileLeaseUnitTest - Reusing <%s> accessMode <0x%x> cnt <%d> \n",
                                           np->n_name,
                                           accessMode,
                                           OSAddAtomic(0, &fndEntry->dur_handlep->handle_reuse_cnt));
                }
                
                lck_mtx_unlock(&fndEntry->dur_handlep->lock);
            }
            
            /* 
             * The process was not found or no list found
             * Either case, we need to
             *	1)  Open a new fork
             *	2)  create a new OpenForkRefEntry entry and add it into the list 
             */
            if (error) {
				uint32_t rights = 0;
				uint32_t shareMode = NTCREATEX_SHARE_ACCESS_ALL;
				proc_t p = vfs_context_proc(ap->a_context);
				
                /* This is wrong, someone has to call open() before doing a byterange lock */
                if (np->f_refcnt <= 0) {
                    error = EBADF;
                    goto exit;
                }

                /* We must find a matching lock in order to succeed at unlock */
                if (pb->unLockFlag == 1) {
                    error = EINVAL;
                    goto exit;
                }
                
                /* Need to open the file here */
                if (accessMode & kAccessRead)
                    rights |= SMB2_FILE_READ_DATA;
                if (accessMode & kAccessWrite)
                    rights |= SMB2_FILE_APPEND_DATA | SMB2_FILE_WRITE_DATA;
                    
                if (accessMode & kDenyWrite)
                    shareMode &= ~NTCREATEX_SHARE_ACCESS_WRITE; /* Remove the wr shared access */
                if (accessMode & kDenyRead)
                    shareMode &= ~NTCREATEX_SHARE_ACCESS_READ; /* Remove the wr shared access */
            
                SMB_MALLOC(fap,
                           struct smbfattr *,
                           sizeof(struct smbfattr),
                           M_SMBTEMP,
                           M_WAITOK | M_ZERO);
                if (fap == NULL) {
                    SMBERROR("SMB_MALLOC failed\n");
                    error = ENOMEM;
                    goto exit;
                }
                
                /* Request a durable handle */
				SMB_MALLOC(dur_handlep, struct smb2_durable_handle *,
						   sizeof (struct smb2_durable_handle),
						   M_TEMP, M_WAITOK | M_ZERO);

				/*
				 * Even though we request a Read/Handle lease, UBC caching is
				 * always disabled for BRL files so that UBC read/writes dont
				 * trip over a BRL from another client.
				 */
				error = smb2_smb_dur_handle_init(share, np,
												 (SMB2_DURABLE_HANDLE_REQUEST | SMB2_NEW_LEASE_KEY), dur_handlep);
				dur_handlep->req_lease_state = smbfs_get_req_lease_state(rights);

				if (error == 0) {
					/* Tell smbfs_smb_ntcreatex to try to get a durable handle */
					create_dur_handlep = dur_handlep;
                }
                
                if (np->n_flag & N_ISRSRCFRK) {
                    disp = FILE_OPEN_IF;
                    do_create = TRUE;
                }
                else {
                    disp = FILE_OPEN;
                    do_create = FALSE;
                }
                
                error = smbfs_smb_ntcreatex(share, np,
                                            rights, shareMode, VREG,
                                            &fid, NULL, 0,
                                            disp, FALSE, fap,
                                            do_create, create_dur_handlep, ap->a_context);

				if (error != 0) {
					if (dur_handlep != NULL) {
						lck_mtx_lock(&dur_handlep->lock);
						dur_handlep->flags = 0;
						lck_mtx_unlock(&dur_handlep->lock);

						smb2_smb_dur_handle_free(dur_handlep);
						SMB_FREE(dur_handlep, M_TEMP);
					}
                    goto exit;
				}
				
				/* Always pass in a dur handle pointer to be used */
                AddFileRef(vp, p, accessMode, rights, fid, dur_handlep, &fndEntry);
				
				/* Do not free dur_handlep as its now being used by File Ref */
				dur_handlep = NULL;
            }

            /* Now I can do the ByteRangeLock */
            if (pb->startEndFlag) {
                /* 
                 * SMB only allows you to lock relative to the begining of the
                 * file.  So, I need to convert offset base on the begining
                 * of the file.  
                 */
                uint64_t fileSize = np->n_size;

                pb->offset += fileSize;
                pb->startEndFlag = 0;
            }
                
            flags = 0;
            if (pb->unLockFlag)
                flags |= SMB_LOCK_RELEASE;
            else
                flags |= SMB_LOCK_EXCL;
            
            /* The problem here is that the lock pid must match the SMB Header PID. 
             * Some day it would be nice to pass a better value here. But for now 
             * always use the same value.
             */
            lck_pid = 1;
            /*
             * -1 infinite wait
             * 0  no wait
             * any other number is the number of milliseconds to wait.
             */
            timo = 0;

            error = smbfs_smb_lock(share, flags, fid, lck_pid, pb->offset, 
                                   pb->length, timo, ap->a_context);
            if (error == 0) {
                /* Save/remove lock info for use in read/write to determine what fork to use */
                AddRemoveByteRangeLockEntry (fndEntry, pb->offset, pb->length,
                                             pb->unLockFlag, lck_pid);
                /* return the offset to the first byte of the lock */
                pb->retRangeStart = pb->offset;
            } else if ((!pb->unLockFlag) && (error == EACCES)) {
                /* 
                 * Need to see if we are locking against ourself, so we can return 
                 * the correct error.
                 */
                if (FindByteRangeLockEntry(fndEntry, pb->offset, pb->length, lck_pid))
                    error = EAGAIN;
            }
        }
            break;
            
        case smbfsDebugTestFSCTL: {
            struct smbDebugTestPB *pb = (struct smbDebugTestPB *) ap->a_data;
            cred = vfs_context_ucred(ap->a_context);
            
            /* For security, we will only allow root to make this call */
            if (!kauth_cred_issuser(cred)) {
                SMBERROR("uid 0x%x not allowed\n", kauth_cred_getuid (cred));
                error = EPERM;
                goto exit;
            }
            
            switch (pb->command) {
                case kForceDisconnect:
                    /*
                     * With multichannel, forcing the main channel into
                     * reconnect will cause it to fail over to another channel
                     * until there are no more alt channels left, then it will
                     * do a reconnect.
                     */
                    (void) smb_session_force_reconnect(sessionp->session_iod);
                    break;

                default:
                    error = EINVAL;
            }
        }
            break;

        case smbfsGetSessionSockaddrFSCTL: {
            /* <72239144> Return original server IP address that was used */
            if (sessionp->session_saddr) {
                memcpy(ap->a_data, sessionp->session_saddr,
                       sessionp->session_saddr->sa_len);
            } else {
                error = EINVAL; /* Better never happen, but just in case */
            }
            break;
        }
		
        case smbfsGetSessionSockaddrFSCTL2: {
            struct smbSockAddrPB *pb = (struct smbSockAddrPB *) ap->a_data;

            pb->sessionp = sessionp;

            /* <72239144> Return original server IP address that was used */
            if (sessionp->session_saddr) {
                memcpy(&pb->addr, sessionp->session_saddr,
                       sessionp->session_saddr->sa_len);
            }
            else {
                error = EINVAL; /* Better never happen, but just in case */
            }
            break;
        }

        case smbfsUniqueShareIDFSCTL: {
            struct UniqueSMBShareID *uniqueptr = (struct UniqueSMBShareID *)ap->a_data;
			
            uniqueptr->error = 0;
            if ((uniqueptr->flags & SMBFS_GET_ACCESS_INFO) ||
                ((uniqueptr->unique_id_len == smp->sm_args.unique_id_len) &&
                 (bcmp(smp->sm_args.unique_id, uniqueptr->unique_id, uniqueptr->unique_id_len) == 0))) {
                    /*
                     * Unique IDs are a match, now figure out what Auth method was
                     * used to mount. In the future we should return the lsa name
                     * if we have one
                     */
                    uniqueptr->user[0] = 0;	/* Just in case we have no user name */
                    
                    if (sessionp->session_flags & SMBV_GUEST_ACCESS) {
                        uniqueptr->connection_type = kConnectedByGuest;
                        strlcpy(uniqueptr->user, kGuestAccountName, SMB_MAXUSERNAMELEN + 1);
                    }
                    else if (sessionp->session_username) {
                        uniqueptr->connection_type = kConnectedByUser;
                        strlcpy(uniqueptr->user, sessionp->session_username, SMB_MAXUSERNAMELEN + 1);
                    }
                    else {
                        uniqueptr->connection_type = kConnectedByKerberos;
                    }
                    
                    SMB_LOG_AUTH("UniqueShareID match for <%s> \n",
                                 sessionp->session_srvname);
                    
                    uniqueptr->error = EEXIST;
                }
        }
            break;
            
		case smbfsTimeMachineFSCTL: {
			struct TimeMachinePB *pb = (struct TimeMachinePB *) ap->a_data;
			u_int32_t option = 0;
			
			/*
			 * Entitlement check
			 */
			cred = vfs_context_ucred(ap->a_context);
			if (priv_check_cred(cred, PRIV_SMB_TIMEMACHINE_CONTROL, 0) != 0) {
				SMBERROR("Time Machine entitlement check failed\n");
				error = EPERM;
				goto exit;
			}
			
			switch (pb->command) {
				case kReadSettings:
					/* 
					 * Check if Durable Handle V2 supported if its not
					 * already been checked. This is the case where a share has
					 * been mounted and Time Machine is just checking to see if
					 * its a valid backup destination or not.
                     *
                     * A Continuously Available (CA) server would also qualify
                     * as a valid TM backup destination. CA servers have
                     * persistent handles and servers should never crash.
                     * Persistent Handles are used to make sure all IO goes
                     * directly to stable storage and thus F_FULLFSYNC is not
                     * needed for CA servers.
					 */
					if (!(sessionp->session_misc_flags & SMBV_HAS_DUR_HNDL_V2) &&
						!(sessionp->session_misc_flags & SMBV_NO_DUR_HNDL_V2)) {
						/* Check for Durable Handle V2 support */
						SMBERROR("Check for durable handles \n");
						if (!smb2fs_smb_check_dur_handle_v2(share, VTOSMB(vp),
															&sessionp->session_dur_hndl_v2_default_timeout,
															ap->a_context)) {
							sessionp->session_misc_flags |= SMBV_HAS_DUR_HNDL_V2;
						}
						else {
							sessionp->session_misc_flags |= SMBV_NO_DUR_HNDL_V2;
						}
					}

					pb->bitmap = 0;
					
					pb->attributes = kSMBServer;
					
					if ((sessionp->session_volume_caps & kAAPL_SUPPORTS_FULL_SYNC)
                        || (share->ss_share_caps & SMB2_SHARE_CAP_CONTINUOUS_AVAILABILITY)) {
						/* Server supports Full Sync */
						pb->attributes |= kSMBFullFSyncSupported;
					}
					
					if (sessionp->session_misc_flags & SMBV_HAS_DUR_HNDL_V2) {
						/* Server supports Durable Handle V2 */
						pb->attributes |= kSMBDurableHandleV2Supported;
						
						if (sessionp->session_dur_hndl_v2_desired_timeout == 0) {
							/* Return the current default timeout value */
							pb->reconnectTimeOut = sessionp->session_dur_hndl_v2_default_timeout;
						}
						else {
							/* Return the current set timeout value */
							pb->reconnectTimeOut = sessionp->session_dur_hndl_v2_desired_timeout;
						}
					}
					else {
						pb->reconnectTimeOut = 0;
					}
					
					/* Unused */
					pb->reconnectConnectTimeOut = 0;
					
					if (sessionp->reconnect_wait_time == 0) {
						/* Reconnect is disabled */
						pb->disablePrimaryReconnect = 1;
					}
					else {
						pb->disablePrimaryReconnect = 0;
					}

					pb->disableSecondaryReconnect = 0;
					
					error = smb_iod_get_qos(sessionp->session_iod, &option);
					if (error) {
						SMBDEBUG("smb_iod_get_qos failed %d \n", error);
					}
					
					pb->IP_QoS = option;
					
					SMBERROR("TM Read - attributes 0x%x reconnTO %d QoS 0x%x\n",
							 pb->attributes, pb->reconnectTimeOut, pb->IP_QoS);
					
					break;
				case kWriteSettings:
					if ((pb->reconnectTimeOut != 0) &&
						(pb->reconnectTimeOut > (16 * 60))) {
						/* 
						 * Windows 7 and Windows 2008 R2 set the V2 durable 
						 * handle timeout to be 16 minutes. Windows 8 and 2012
						 * set the timer to be 2 minutes.
						 */
						SMBERROR ("Invalid durable handle V2 time out %d\n", pb->reconnectTimeOut);
						error = EINVAL;
						goto exit;
					}
					else {
						SMB_LOG_IO("Setting Durable Handle V2 timeout to %d seconds \n", pb->reconnectTimeOut);
						sessionp->session_dur_hndl_v2_desired_timeout = pb->reconnectTimeOut;
						
						/* Verify that server supports this timeout value */
						timeout = sessionp->session_dur_hndl_v2_desired_timeout;
						
						/* If Time Machine mount, then see if Durable Handle V2 supported */
						if (!(sessionp->session_misc_flags & SMBV_HAS_DUR_HNDL_V2)) {
							SMBERROR ("Server does not support Durable Handle V2\n");
							error = ENOTSUP;
							goto exit;
						}
						
						/* Check for Durable Handle V2 support */
						SMBERROR("Attempt setting durable handle timeout to %d \n", timeout);
						if (!smb2fs_smb_check_dur_handle_v2(share, VTOSMB(vp),
															&timeout,
															ap->a_context)) {
							/* 
							 * Setting timeout worked, but did we get the
							 * timeout that we requested?
							 */
							if (timeout != sessionp->session_dur_hndl_v2_desired_timeout) {
								SMBERROR("Server failed setting timeout to %d, got %d instead \n",
										 sessionp->session_dur_hndl_v2_desired_timeout,
										 timeout);
								error = E2BIG;
								goto exit;
							}
						}
						else {
							SMBERROR ("Server failed to set Durable Handle V2\n");
							error = ENOTSUP;
							goto exit;
						}
					}
					
					if (pb->disablePrimaryReconnect == 1) {
						SMBERROR("Disable reconnect \n");
						sessionp->reconnect_wait_time = 0;
					}
					
					/* 
					 * TCP Quality of Service option
					 * Expected to be 0x20 meaning "Low Priority Data"
					 * according to RFC4594. IE dont hog the connection. 
					 */
					option = pb->IP_QoS;
					
					error = smb_iod_set_qos(sessionp->session_iod, &option);
					if (error) {
						SMBERROR("smb_iod_set_qos failed %d \n", error);
					}
					
					break;
				default:
					error = EINVAL;
			}
		}
			break;

		case smbfsGetStatsFSCTL: {
			struct smb_reconnect_stats *pb = (struct smb_reconnect_stats *) ap->a_data;
			
			smb_reconn_stats.version = 1;
			
			memcpy (pb, &smb_reconn_stats, sizeof (smb_reconn_stats));
			error = 0;
		}
			break;

		case F_FULLFSYNC:
			if (!(sessionp->session_volume_caps & kAAPL_SUPPORTS_FULL_SYNC)
                && !(share->ss_share_caps & SMB2_SHARE_CAP_CONTINUOUS_AVAILABILITY)) {
				/* Server does not support F_FULLSYNC */
				error = ENOTSUP;
				break;
			}

			if (vp != NULL) {
				if (!vnode_isreg(vp)) {
					/* If not a regular file, then not supported */
					error = ENOTSUP;
					break;
				}

				if (np->f_openState & kNeedRevoke) {
					/* If file has been revoked, return EINVAL */
					error = EINVAL;
					break;
				}
			}
			
			/* 
			 * First do normal flush
			 * Make sure a flush actually gets done and sent 
			 */
			VTOSMB(ap->a_vp)->n_flag |= NNEEDS_FLUSH;

			error = smbfs_fsync(share, vp, 0, 0, ap->a_context);
			if (error) {
				SMBERROR_LOCK(np, "smbfs_fsync failed %d on <%s> \n", error, np->n_name);
				break;
			}
			
			/* Now do F_FULLSYNC if we are not a Continuous Availability Server */
            if (!(share->ss_share_caps & SMB2_SHARE_CAP_CONTINUOUS_AVAILABILITY)) {
                error = smbfs_smb_fsync(share, np, 1, ap->a_context);
                if (error) {
                    SMBERROR_LOCK(np, "smbfs_smb_fsync failed %d on <%s> \n", error, np->n_name);
                    break;
                }
            }
                
			break;

		default:
			error = ENOTSUP;
			goto exit;
    }
    
exit:
	smb_share_rele(share, ap->a_context);
	smbnode_unlock(np);
    
    SMB_LOG_KTRACE(SMB_DBG_IOCTL | DBG_FUNC_END, error, 0, 0, 0, 0);
	return (error);
}

/*
 * SMB locks do not map to POSIX.1 advisory locks in several ways:
 * 1 - SMB provides no way to find an existing lock on the server.
 *     So, the F_GETLK operation can only report locks created by processes
 *     on this client. 
 * 2 - SMB locks cannot overlap an existing locked region of a file. So,
 *     F_SETLK/F_SETLKW operations that establish locks cannot extend an
 *     existing lock.
 * 3 - When unlocking a SMB locked region, the region to unlock must correspond
 *     exactly to an existing locked region. So, F_SETLK F_UNLCK operations
 *     cannot split an existing lock or unlock more than was locked (this is
 *     especially important because whne files are closed, we receive a request
 *     to unlock the entire file: l_whence and l_start point to the beginning
 *     of the file, and l_len is zero).
 *
 * The result... SMB cannot support POSIX.1 advisory locks. It can however
 * support BSD flock() locks, so that's what this implementation will allow. 
 *
 * Since we now support open deny modes we will only support flocks on files that
 * have no files open w
 *
 *		vnode_t a_vp;
 *		caddr_t  a_id;
 *		int  a_op;
 *		struct flock *a_fl;
 *		int  a_flags;
 *		vfs_context_t a_context;
 */
static int32_t 
smbfs_vnop_advlock(struct vnop_advlock_args *ap)
{
	int		flags = ap->a_flags;
	vnode_t vp = ap->a_vp;
	struct smb_share *share;
	struct smbnode *np;
	int error = 0;
	uint32_t timo;
	off_t start = 0;
	uint64_t len = -1;
	uint32_t lck_pid;

	/* Preflight checks */
	if (!vnode_isreg(vp)) {
		/* can only read regular files */
		if (vnode_isdir(vp))
			return (EISDIR);
		else
			return (EPERM);
	}

	share = smb_get_share_with_reference(VTOSMBFS(vp));
	if ((flags & F_POSIX) && ((UNIX_CAPS(share) & CIFS_UNIX_FCNTL_LOCKS_CAP) == 0)) {
		/* Release the share reference before returning */			
		smb_share_rele(share, ap->a_context);
		return(err_advlock(ap));
	}
    
	if ((flags & (F_FLOCK | F_POSIX)) == 0) {
		/* Release the share reference before returning */			
		smb_share_rele(share, ap->a_context);
		SMBWARNING("Lock flag we do not understand %x\n", flags);
		return(err_advlock(ap));
	}

	if ((error = smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK))) {
		/* Release the share reference before returning */			
		smb_share_rele(share, ap->a_context);
		return (error);
	}

	SMB_LOG_KTRACE(SMB_DBG_ADVLOCK | DBG_FUNC_START, 0, 0, 0, 0, 0);

    /* If we got here it must be a flock. Remember flocks lock the whole file. */
	np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_advlock;
	
	/* 
	 * This vnode has a file open with open deny modes, so the file is really 
	 * already locked. Remember that vn_open and vn_close will also call us here 
	 * so to make them work for now, return no err. If the opened it for 
	 * Open Deny then no one else should be allowed to use it. We could check
	 * the pid here, but the open call should have handled that for us.
	 */
	if (np->f_openDenyList) {
		error = 0;
		goto exit;
	}
	
	/* Before trying the lock see if the file needs to be reopened */
	error = smbfs_smb_reopen_file(share, np, ap->a_context);
	if (error) {
		SMBDEBUG_LOCK(np, " %s waiting to be revoked\n", np->n_name);
		goto exit;
	}
	
	/* 
	 * So if we got to this point we have a normal flock happening. We can have
	 * the following flags passed to us.
	 *
	 *	LOCK_SH - shared lock
	 *	LOCK_EX - exclusive lock
	 *	LOCK_NB - don't block when locking
	 *	LOCK_UN - unlock
	 *
	 * Currently we always allow the server handle blocking. We may want to 
	 * re-look at this later. What if we have a lock that is blocked and
	 * the server goes down how long to we wait.
	 *
	 * The locking mechanism allows two types of locks: shared locks and
	 * exclusive locks.  At any time multiple shared locks may be applied to a
	 * file, but at no time are multiple exclusive, or both shared and exclusive,
	 * locks allowed simultaneously on a file.
	 *
	 * A shared lock may be upgraded to an exclusive lock, and vice versa, sim-
	 * ply by specifying the appropriate lock type; this results in the previous
	 * lock being released and the new lock applied (possibly after other processes
	 * have gained and released the lock).
	 *
	 * We currently treat LOCK_EX and LOCK_SH the same except we do not allow
	 * you to have more that one LOCK_EX.
	*/
	timo = (flags & F_WAIT) ? -1 : 0;
	/* The problem here is that the lock pid must match the SMB Header PID. Some
	 * day it would be nice to pass a better value here. But for now always
	 * use the same value.
	 */
	lck_pid = 1;
	/* Remember that we are always using the share open file at this point */
	switch(ap->a_op) {
	case F_SETLK:
		if (! np->f_smbflock) {
			error = smbfs_smb_lock(share, SMB_LOCK_EXCL, np->f_fid, lck_pid, 
								   start, len, timo, ap->a_context);
			if (error)
				goto exit;
			SMB_MALLOC(np->f_smbflock, struct smbfs_flock *, sizeof *np->f_smbflock, 
				   M_LOCKF, M_WAITOK);
			np->f_smbflock->refcnt = 1;
			np->f_smbflock->fl_type = ap->a_fl->l_type;	
			np->f_smbflock->lck_pid = lck_pid;
			np->f_smbflock->start = start;
			np->f_smbflock->len = len;
			np->f_smbflock->flck_pid = proc_pid(vfs_context_proc(ap->a_context));
		} else if (np->f_smbflock->flck_pid == (uint32_t)proc_pid(vfs_context_proc(ap->a_context))) {
			/* First see if this is a upgrade or downgrade */
			if ((np->f_smbflock->refcnt == 1) && 
				(np->f_smbflock->fl_type != ap->a_fl->l_type)) {
					np->f_smbflock->fl_type = ap->a_fl->l_type;
					goto exit;
			}
			/* Trying to mismatch two different style of locks with the same process id bad! */
			if (np->f_smbflock->fl_type != ap->a_fl->l_type) {
				error = ENOTSUP;
				goto exit;
			}
			/* 
			 * We know they have the same lock style from above, but they are 
			 * asking for two exclusive. So from Terry comments it looks like
			 * this is ok.  Here's the issue: because what they are doing is 
			 * upgrading an exclusive lock to an exclusive lock in the process 
			 * that holds the previous lock, there are _not_ multiple locks 
			 * involved; there's only the first lock and the lock that replaces 
			 * it.
			 */
			if (np->f_smbflock->fl_type != F_WRLCK) {
				/* 
				 * At no time are multiple exclusive locks allowed simultaneously
				 * on a file. So we can have only one refcnt. This is an upgrade
				 * not another lock.
				 */
			} else {
				np->f_smbflock->refcnt++;
			}
		} else {
			/*
			 * Radar 5572840
			 * F_WAIT is set we should sleep until the other flock
			 * gets free then to an upgrade or down grade. Not support
			 * with SMB yet.
			 */
			error = EWOULDBLOCK;
			goto exit;
		}
		break;
	case F_UNLCK:
		error = 0;
		if (! np->f_smbflock)	/* Got an  unlock and had no lock ignore */
			break;
		np->f_smbflock->refcnt--;
		/* remove the lock on the network and  */
		if (np->f_smbflock->refcnt <= 0) {
			error = smbfs_smb_lock(share, SMB_LOCK_RELEASE, np->f_fid, lck_pid, 
								   start, len, timo, ap->a_context);
			if (error == 0) {
				SMB_FREE(np->f_smbflock, M_LOCKF);
				np->f_smbflock = NULL;
			}
		}
		break;
	default:
		error = EINVAL;
		break;
	}
exit:
	smb_share_rele(share, ap->a_context);
	smbnode_unlock(np);

	SMB_LOG_KTRACE(SMB_DBG_ADVLOCK | DBG_FUNC_END, error, 0, 0, 0, 0);
	return (error);
}

/*
 * The calling routine must hold a reference on the share
 */
static int
smbfs_pathcheck(struct smb_share *share, const char *name, size_t nmlen, 
				uint32_t nameiop)
{
	const char *cp, *endp;
	int error;

	/*
	 * We need to check the file name length. We now use ss_maxfilenamelen 
	 * since that gives us a more accurate max file name length. If the 
	 * server supports UNICODE we should do more checking. Since UTF8 can
	 * have three bytes per character just checking the length is not enough.
	 * We should convert it to UTF16 and see if the length is twice 
	 * ss_maxfilenamelen.
	 *
	 */
	if ((uint32_t)nmlen > share->ss_maxfilenamelen) {
		if (SMB_UNICODE_STRINGS(SS_TO_SESSION(share))) {
			uint16_t *convbuf;
			size_t ntwrk_len;
			/*
			 * smb_strtouni needs an output buffer that is twice 
			 * as large as the input buffer (name).
			 */
            SMB_MALLOC(convbuf, uint16_t *, nmlen * 2, M_SMBNODENAME, M_WAITOK);
			if (! convbuf)
				return ENAMETOOLONG;
			/* 
			 * We need to get the UFT16 length, so just use smb_strtouni
			 * instead of smb_convert_to_network.
			 */
			ntwrk_len = smb_strtouni(convbuf, name,  nmlen, 
						UTF_PRECOMPOSED | UTF_SFM_CONVERSIONS);
			SMB_FREE(convbuf, M_SMBNODENAME);
			if (ntwrk_len > (share->ss_maxfilenamelen * 2))
				return ENAMETOOLONG;
		} else {
			return ENAMETOOLONG;
		}
	} else if (! nmlen) {
		return ENAMETOOLONG;
	}
	
	/* Check name only if CREATE, DELETE, or RENAME */
	if (nameiop == LOOKUP)
		return (0);

	/* 
	 * Winodws systems do not allow items that begin with "con" to be created. 
	 * If this is not a UNIX server then stop the user from trying
	 * to create this file or folder. When trying to create a "con" folder or 
	 * "con.xxx" file a windows system will report the following error:
	 * Cannot create or replace file: The filename you specified is too long.
	 * Specify a different filename.
	 *
	 * From my testing any name that matches "con" or begins with "con."
	 * should not be create.
	 *
	 * Should we be like windows and return ENAMETOOLONG or EACCES
	 */
	if ((! UNIX_SERVER(SS_TO_SESSION(share))) && CON_FILENAME(name, nmlen)) {
		if ((nmlen == 3) || ((nmlen > 3) && (*(name+3) == '.')))
			return (ENAMETOOLONG); 
	}
	
	/* If the server supports UNICODE then we are done checking the name. */
	if (SMB_UNICODE_STRINGS(SS_TO_SESSION(share)))
		return (0);

	/*
	 * Normally, we'd return EINVAL when the name is syntactically invalid,
	 * but ENAMETOOLONG makes it clear that the name is the problem (and
	 * allows Carbon to return a more meaningful error).
	 */
	error = ENAMETOOLONG;

	/*
	 * Note: This code does not prevent the smb file system client
	 * from creating filenames which are difficult to use with
	 * other clients. For example, you can create "  foo  " or
	 * "foo..." which cannot be moved, renamed, or deleted by some
	 * other clients.
	 */
	 
	/* check for illegal characters, if the server does not support UNICODE */
	for (cp = name, endp = name + nmlen; cp < endp; ++cp) {
		/*
		 * The set of illegal characters in long names is the same as
		 * 8.3 except the characters 0x20, 0x2b, 0x2c, 0x3b, 0x3d, 0x5b,
		 * and 0x5d are now legal, and the restrictions on periods was
		 * removed.
		 */
		switch (*cp) {
			case 0x20:	/* space */
			case 0x2B:	/* +     */
			case 0x2C:	/* ,     */
			case 0x3B:	/* ;     */
			case 0x3D:	/* =     */
			case 0x5B:	/* [     */
			case 0x5D:	/* ]     */
				break;
			case 0x22:	/* "     */
			case 0x2A:	/* *     */
			case 0x2F:	/* /     */
			case 0x3A:	/* :     */
			case 0x3C:	/* <     */
			case 0x3E:	/* >     */
			case 0x3F:	/* ?     */
			case 0x5C:	/* \     */
			case 0x7C:	/* |     */
				/* illegal character found */
				return (error);
				break;
			default:
				break;
		}
	}
	return (0);
}

/*
 * smbfs_vnop_lookup
 *
 * struct vnodeop_desc *a_desc;
 * vnode_t a_dvp;
 * vnode_t *a_vpp;
 * struct componentname *a_cnp;
 * vfs_context_t a_context;
 */
static int 
smbfs_vnop_lookup(struct vnop_lookup_args *ap)
{
	vfs_context_t context = ap->a_context;
	vnode_t dvp = ap->a_dvp;
	vnode_t *vpp = ap->a_vpp;
	vnode_t vp;
	struct smbnode *dnp = NULL;
	struct mount *mp = vnode_mount(dvp);
	struct smb_share *share = NULL;
	struct componentname *cnp = ap->a_cnp;
	const char *name = cnp->cn_nameptr;
	uint32_t flags = cnp->cn_flags;
	uint32_t nameiop = cnp->cn_nameiop;
	size_t nmlen = cnp->cn_namelen;
	struct smbfattr fattr, *fap = NULL;
	int wantparent, error, islastcn, isdot = FALSE;
	int parent_locked = FALSE;
    int check_name = 0;
    vnode_t par_vp = NULL;
	
	/* 
	 * We may want to move smbfs_pathcheck here, but we really should never
	 * find a bad name in the name cache lookup.
	 */	
	if (!vnode_isdir(dvp))
		return (ENOTDIR);

	if ((flags & ISDOTDOT) && vnode_isvroot(dvp)) {
		SMBERROR("invalid '..'\n");
		return (EIO);
	}
    
	islastcn = (flags & ISLASTCN) ? TRUE : FALSE;
	if (islastcn && vfs_isrdonly(mp) && nameiop != LOOKUP)
		return (EROFS);

    SMB_LOG_KTRACE(SMB_DBG_LOOKUP | DBG_FUNC_START,
                   VTOSMB(dvp)->d_fid, 0, 0, 0, 0);

	wantparent = (flags & (LOCKPARENT|WANTPARENT)) ? TRUE : FALSE;
	
	share = smb_get_share_with_reference(VTOSMBFS(dvp));
    
	/*
	 * We need to make sure the negative name cache gets updated if 
	 * needed. So if the parents cache has expired, then update the
	 * the parent's cache. This will cause the negative name cache to
	 * be flush if the parent's modify time has changed.
	 */
	if (smbnode_lock(VTOSMB(dvp), SMBFS_EXCLUSIVE_LOCK) == 0) {
		VTOSMB(dvp)->n_lastvop = smbfs_vnop_lookup;
		if (VTOSMB(dvp)->n_flag & NNEGNCENTRIES) {
			/* ignore any errors here we will catch them later */
			(void)smbfs_update_cache(share, dvp, NULL, context);
		}
		smbnode_unlock(VTOSMB(dvp));	/* Release the smbnode lock */
	}
	
	*vpp = NULLVP;
	error = cache_lookup(dvp, vpp, cnp);
    
    SMB_LOG_KTRACE(SMB_DBG_LOOKUP | DBG_FUNC_NONE,
                   0xabc001, error, 0, 0, 0);
    
	switch (error) {
		case ENOENT:	/* negative cache entry */
			goto skipLookup;
		case 0:		/* cache miss */
			break;
		case -1:	/* cache hit */
			/*
			 * On CREATE we can't trust a cache hit as if it is stale
			 * and the object doesn't exist on the server returning zero
			 * here would cause the vfs layer to, for instance, EEXIST
			 * the mkdir.
			 */
			if (nameiop != CREATE) {
				error = 0;
				/* Check to see it the node's meta cache needs to be update */
				if (smbnode_lock(VTOSMB(*vpp), SMBFS_EXCLUSIVE_LOCK) == 0) {
					VTOSMB(*vpp)->n_lastvop = smbfs_vnop_lookup;
					error = smbfs_update_cache(share, *vpp, NULL, context);
					smbnode_unlock(VTOSMB(*vpp));	/* Release the smbnode lock */
				}
				/* 
				 * At this point we only care if it exist or not so any other 
				 * error should just get ignored 
				 */
                if (error != ENOENT) {
					error =  0;
					goto done;
				}
				/* 
				 * The item we had, no longer exists so fall through and see if 
				 * it exist as a different item 
				 */
			}
			if (*vpp) {
				cache_purge(*vpp);
				vnode_put(*vpp);
				*vpp = NULLVP;
			}
			break;
		default:	/* unknown & unexpected! */
			SMBWARNING("cache_lookup error=%d\n", error);
			goto done;
	}
    
	/* 
	 * entry is not in the name cache
	 *
	 * validate syntax of name.  ENAMETOOLONG makes it clear the name
	 * is the problem
	 */
	error = smbfs_pathcheck(share, cnp->cn_nameptr, cnp->cn_namelen, nameiop);
	if (error) {
		SMBWARNING("warning: bad filename %s\n", name);
		goto done;
	}
	dnp = VTOSMB(dvp);

	/* lock the parent while we go look for the item on server */
	if (smbnode_lock(dnp, SMBFS_EXCLUSIVE_LOCK) != 0) {
		error = ENOENT;
		goto skipLookup;
	}
	parent_locked = TRUE;
	dnp->n_lastvop = smbfs_vnop_lookup;

	isdot = (nmlen == 1 && name[0] == '.');
	fap = &fattr;
	/* 
	 * This can allocate a new "name" do not return before the end of the
	 * routine from here on.
	 */
	if (flags & ISDOTDOT) {
        par_vp = smbfs_smb_get_parent(dnp, kShareLock);
        if (par_vp != NULL) {
            error = smbfs_lookup(share, VTOSMB(par_vp), NULL, NULL, fap, context);

            vnode_put(par_vp);
        }
        else {
            if (dnp->n_parent_vid != 0) {
                /* Parent got recycled already? */
                SMBWARNING_LOCK(dnp, "Missing parent for <%s> \n",
                                dnp->n_name);
            }

            error = ENOENT;
        }
    }
    else {
        if ((SS_TO_SESSION(share)->session_flags & SMBV_SMB2) &&
            (share->ss_fstype != SMB_FS_FAT) &&
            (share->ss_attributes & FILE_NAMED_STREAMS)) {
            /* Just need basic attributes */
            error = smb_dir_cache_find_entry(dvp, &dnp->d_main_cache,
                                             (char *) name, nmlen, fap, 0);
            if (error) {
                error = smbfs_lookup(share, dnp, &name, &nmlen, fap, context);
            }
        }
        else {
            error = smbfs_lookup(share, dnp, &name, &nmlen, fap, context);
        }

		if (error == 0) {
			/* Check to see if we need to update vnodes name */
			check_name = 1;
		}
	}
    SMB_LOG_KTRACE(SMB_DBG_LOOKUP | DBG_FUNC_NONE,
                   0xabc002, error, 0, 0, 0);

	/*
	 * We use to unlock the parent here, but we really need it locked
	 * until after we do the smbfs_nget calls.
	 */
	/*
	 * We didn't find it and this is not a CREATE or RENAME operation so
	 * add it to the negative name cache.
	 */
	if ((error == ENOENT) && (cnp->cn_flags & MAKEENTRY) && 
		(!(((nameiop == CREATE) || (nameiop == RENAME)) && islastcn))) {
		/* add a negative entry in the name cache */
		cache_enter(dvp, NULL, cnp);
		dnp->n_flag |= NNEGNCENTRIES;
	}
	
skipLookup:
	if (error) {
		/*
		 * note the EJUSTRETURN code in lookup()
		 */
		if (((nameiop == CREATE) || (nameiop == RENAME)) && 
			(error == ENOENT) && islastcn) {
			error = EJUSTRETURN;
		}
	} else if ((nameiop == RENAME) && islastcn && wantparent) {
		if (isdot) {
			error = EISDIR;
		} else {
			error = smbfs_nget(share, mp,
                               dvp, name, nmlen,
                               fap, &vp,
                               0, SMBFS_NGET_CREATE_VNODE,
                               context);
			if (!error) {
                if (check_name) {
                    /*
                     * If we used dir enumeration cache or a lookup, then we 
                     * found it by name. Then we used smbfs_nget and found it 
                     * by File ID. Thus the vnode name might not match the 
                     * latest name and it needs to be checked and possibly 
                     * updated.
                     */
                    smbfs_update_name_par(share, dvp, vp,
                                          &fap->fa_reqtime,
                                          name, nmlen);
                }
				smbnode_unlock(VTOSMB(vp));	/* Release the smbnode lock */
				*vpp = vp;
			}
		}
	} else if ((nameiop == DELETE) && islastcn) {
		if (isdot) {
			error = vnode_get(dvp);
			if (!error)
				*vpp = dvp;
		} else {
			error = smbfs_nget(share, mp,
                               dvp, name, nmlen,
                               fap, &vp,
                               0, SMBFS_NGET_CREATE_VNODE,
                               context);
			if (!error) {
                if (check_name) {
                    /*
                     * If we used dir enumeration cache or a lookup, then we
                     * found it by name. Then we used smbfs_nget and found it
                     * by File ID. Thus the vnode name might not match the
                     * latest name and it needs to be checked and possibly
                     * updated.
                     */
                    smbfs_update_name_par(share, dvp, vp,
                                          &fap->fa_reqtime,
                                          name, nmlen);
                }
				smbnode_unlock(VTOSMB(vp));	/* Release the smbnode lock */
				*vpp = vp;
			}
		}
	} else if (flags & ISDOTDOT) {
        if (dvp) {
            vp = smbfs_smb_get_parent(VTOSMB(dvp), kShareLock);
            if (vp != NULL) {
                /* Dont need vnode_put() as we want to do a vnode_vget() */
                *vpp = vp;
                error = 0;
            }
            else {
                if (VTOSMB(dvp)->n_parent_vid != 0) {
                    /* Parent got recycled already? */
                    SMBWARNING_LOCK(VTOSMB(dvp), "Missing parent for <%s> \n",
                                    VTOSMB(dvp)->n_name);
                }

                error = ENOENT;
            }
        }
	} else if (isdot) {
		error = vnode_get(dvp);
		if (!error)
			*vpp = dvp;
	} else {
		error = smbfs_nget(share, mp,
                           dvp, name, nmlen,
                           fap, &vp,
                           cnp->cn_flags, SMBFS_NGET_CREATE_VNODE,
                           context);
		if (!error) {
            if (check_name) {
                /*
                 * If we used dir enumeration cache or a lookup, then we
                 * found it by name. Then we used smbfs_nget and found it
                 * by File ID. Thus the vnode name might not match the
                 * latest name and it needs to be checked and possibly
                 * updated.
                 */
                smbfs_update_name_par(share, dvp, vp,
                                      &fap->fa_reqtime,
                                      name, nmlen);
            }
			smbnode_unlock(VTOSMB(vp));	/* Release the smbnode lock */
			*vpp = vp;
		}
	}
	if (name != cnp->cn_nameptr) {
		SMB_FREE(name, M_SMBNODENAME);
	}
	/* If the parent node is still lock then unlock it here. */
	if (parent_locked && dnp)
		smbnode_unlock(dnp);
done:
	smb_share_rele(share, context);
    
    SMB_LOG_KTRACE(SMB_DBG_LOOKUP | DBG_FUNC_END, error, 0, 0, 0, 0);
	return (error);
}

/* 
 * smbfs_vnop_offtoblk
 * 
 * vnode_t a_vp;
 * off_t a_offset;
 * daddr64_t *a_lblkno;
 * vfs_context_t a_context;
 *
 * ftoblk converts a file offset to a logical block number
 */
static int 
smbfs_vnop_offtoblk(struct vnop_offtoblk_args *ap)
{
	*ap->a_lblkno = ap->a_offset / PAGE_SIZE_64;
	return (0);
}

/* 
 * smbfs_vnop_blktooff
 * 
 * vnode_t a_vp;
 * off_t a_offset;
 * daddr64_t *a_lblkno;
 * off_t *a_offset;
 * vfs_context_t a_context;
 *
 * blktooff converts a logical block number to a file offset
 */
static int 
smbfs_vnop_blktooff(struct vnop_blktooff_args *ap)
{	
	*ap->a_offset = (off_t)ap->a_lblkno * PAGE_SIZE_64;
	return (0);
}

/*
 * smbfs_vnop_pagein
 *
 *  vnode_t 	a_vp,
 *  upl_t		a_pl,
 *  vm_offset_t	a_pl_offset,
 *  off_t		a_f_offset, 
 *  size_t		a_size,
 *  int			a_flags
 *  vfs_context_t a_context;
 *
 * NOTE: We no longer take a node lock in this routine.
 */
static int 
smbfs_vnop_pagein(struct vnop_pagein_args *ap)
{       
	vnode_t vp = ap->a_vp;
	struct smb_share *share;
	size_t size = ap->a_size;
	off_t f_offset = ap->a_f_offset;
	struct smbnode *np;
	int error;
	
    SMB_LOG_KTRACE(SMB_DBG_PAGE_IN | DBG_FUNC_START, 0, 0, 0, 0, 0);

	np = VTOSMB(vp);
	
	if ((size <= 0) || (f_offset < 0) || (f_offset >= (off_t)np->n_size) || 
		(f_offset & PAGE_MASK_64) || (size & PAGE_MASK)) {
		error = err_pagein(ap);	/* behave like the deadfs does */
        goto done;
	}
	share = smb_get_share_with_reference(VTOSMBFS(vp));
	/* Before trying the read see if the file needs to be reopened */
	error = smbfs_smb_reopen_file(share, np, ap->a_context);
	if (error) {
		SMBDEBUG_LOCK(np, " %s waiting to be revoked\n", np->n_name);

		/* Release the share reference before returning */
		smb_share_rele(share, ap->a_context);
		error = err_pagein(ap);	/* behave like the deadfs does */
		goto done;
	}
	/*
	 * The old code would check to see if the node smbfsIsCacheable. If not then
	 * it would try to invalidate the page to force the cluster code to get a
	 * new copy from disk/network. Talked this over with Joe and this is not 
	 * really need.
	 *
	 * The smbfs_vnop_pagein will only be called for extents of pages that do 
	 * NOT already exist in the cache. When the UPL is created, the pages are 
	 * acquired and locked down for the 'holes' that exist in the cache. Once 
	 * those pages are locked into the UPL, there can be no other path by which 
	 * the pages can be made valid. So... no reason to flush pages in the range 
	 * being passed into you and then on to cluster_pagein.
	 */ 
	error = cluster_pagein(vp, ap->a_pl, ap->a_pl_offset, ap->a_f_offset,
						   (int)ap->a_size, (off_t)np->n_size, ap->a_flags);
	if (error) {
		SMB_LOG_IO_LOCK(np, "%s failed cluster_pagein with an error of %d\n",
                        np->n_name, error);
	}
	smb_share_rele(share, ap->a_context);

done:
    SMB_LOG_KTRACE(SMB_DBG_PAGE_IN | DBG_FUNC_END, error, 0, 0, 0, 0);
    return (error);
}

/*
 * smbfs_vnop_pageout
 *
 *  vnode_t 	a_vp,
 *  upl_t		a_pl,
 *  vm_offset_t	a_pl_offset,
 *  off_t		a_f_offset, 
 *  size_t		a_size,
 *  int			a_flags
 *  vfs_context_t a_context;
 *
 * NOTE: We no longer take a node lock in this routine.
 *
 */
static int 
smbfs_vnop_pageout(struct vnop_pageout_args *ap) 
{       
	vnode_t vp = ap->a_vp;
	struct smbnode *np;
	struct smb_share *share;
	upl_t pl = ap->a_pl;
	size_t size = ap->a_size;
	off_t f_offset = ap->a_f_offset;
	int error;
	
	if (vnode_vfsisrdonly(vp))
		return(EROFS);
	
    SMB_LOG_KTRACE(SMB_DBG_PAGE_OUT | DBG_FUNC_START, 0, 0, 0, 0, 0);

	np = VTOSMB(vp);
	
	if (pl == (upl_t)NULL)
		panic("smbfs_vnop_pageout: no upl");
	
	if ((size <= 0) || (f_offset < 0) || (f_offset >= (off_t)np->n_size) ||
	    (f_offset & PAGE_MASK_64) || (size & PAGE_MASK)) {
		error = err_pageout(ap);
        goto done;
	}
	share = smb_get_share_with_reference(VTOSMBFS(vp));
	/* Before trying the write see if the file needs to be reopened */
	error = smbfs_smb_reopen_file(share, np, ap->a_context);
	if (error) {
		SMBDEBUG_LOCK(np, " %s waiting to be revoked\n", np->n_name);

		/* Release the share reference before returning */
		smb_share_rele(share, ap->a_context);
		error = err_pageout(ap);
        goto done;
	}
	
	error = cluster_pageout(vp, ap->a_pl, ap->a_pl_offset, ap->a_f_offset, 
							(int)ap->a_size, (off_t)np->n_size, ap->a_flags);
	if (error) {
		SMB_LOG_IO_LOCK(np, "%s failed cluster_pageout with an error of %d\n",
                        np->n_name, error);
	}
	
	smb_share_rele(share, ap->a_context);
	/* If we can get the parent vnode, reset its meta data cache timer. */
	if (vnode_isnamedstream(vp)) {
		vnode_t parent_vp = vnode_getparent(vp);
		if (parent_vp) {
			VTOSMB(parent_vp)->attribute_cache_timer = 0;
			vnode_put(parent_vp);		
		}	
	}

done:
    SMB_LOG_KTRACE(SMB_DBG_PAGE_OUT | DBG_FUNC_END, error, 0, 0, 0, 0);
    return (error);
}

/*
 * smbfs_vnop_copyfile
 *
 * vnode_t a_fvp;
 * vnode_t a_tdvp;
 * vnode_t a_tvp;
 * struct componentname *a_tcnp;
 * int a_flags;
 * vfs_context_t a_context;
 */
static int
smbfs_vnop_copyfile(struct vnop_copyfile_args *ap)
{
	vnode_t 	fvp = ap->a_fvp;
	vnode_t 	tvp = ap->a_tvp;
	vnode_t 	tdvp = ap->a_tdvp;
	struct smbmount *smp = VFSTOSMBFS(vnode_mount(fvp));
	struct smb_share *share = NULL;
	struct componentname *tcnp = ap->a_tcnp;
	struct smbnode *fnp = NULL;
	struct smbnode *tdnp = NULL;
	struct smbnode *tnp = NULL;
	int error = 0, vtype, need_unlock = 0;
	
    /* VFS checks the following before calling us:
     *
     * tvp exists AND (ap->flags & CPF_OVERWRITE)
     * fvp AND tvp are not directories
     * KAUTH_VNODE_ADD_FILE authorized on tdvp
     * fvp != tvp
     * fvp != tdvp
     */
    
    SMB_LOG_KTRACE(SMB_DBG_COPYFILE | DBG_FUNC_START, 0, 0, 0, 0, 0);

    /* Check if this is an SMB 2/3 server (need COPYCHUNK IOCTL) */
    share = smb_get_share_with_reference(smp);
    if (!(SS_TO_SESSION(share)->session_flags & SMBV_SMB2)) {
        SMBERROR("copyfile not supported on this server.\n");
        error = ENOTSUP;
        goto out;
    }
	
	/* Check for cross-device copyfile */
	if ((vnode_mount(fvp) != vnode_mount(tdvp)) ||
		(tvp && (vnode_mount(fvp) != vnode_mount(tvp)))) {
        SMBERROR("cross-device copyfile not supported.\n");
		error = EXDEV;
        goto out;
    }
	
    /* source file must be a directory, symbolic link, or regular file */
	vtype = vnode_vtype(fvp);
	if ( (vtype != VDIR) && (vtype != VREG) && (vtype != VLNK) ) {
        SMBERROR("copyfile not supported on vtype: %d\n", vtype);
		error = EINVAL;
        goto out;
    }

    /*
     * Check to see if source file is a dataless file and allowed to be
     * materialized
     */
    error = smbfs_is_dataless_access_allowed(fvp, 1, ap->a_context);
    if (error) {
        /* This process is not allowed to materialize this file */
        goto out;
    }

    fnp = VTOSMB(fvp);
	tdnp = VTOSMB(tdvp);
    tnp = (tvp == NULL) ? NULL : VTOSMB(tvp);
    
    /* Lock source file and target directory */
    smbnode_lockpair(fnp, tdnp, SMBFS_EXCLUSIVE_LOCK);
    need_unlock = 1;
    
	fnp->n_lastvop = smbfs_vnop_copyfile;
	tdnp->n_lastvop = smbfs_vnop_copyfile;
	if (tnp != NULL)
		tnp->n_lastvop = smbfs_vnop_copyfile;
    
	/*
	 * Do the copyfile operation.
	 */
    error = smb2fs_smb_copyfile(share, fnp, tdnp, tcnp->cn_nameptr,
                                tcnp->cn_namelen, ap->a_context);
    if (error) {
        SMBERROR("smb2fs_smb_copyfile returned: %d\n", error);
        goto out;
    }
    
	smbfs_attr_touchdir(tdnp, (share->ss_fstype == SMB_FS_FAT));

	/* <33469405> if dir has active lease skip local change notify */
	lck_mtx_lock(&tdnp->d_dur_handle.lock);
	if (!(tdnp->d_dur_handle.flags & SMB2_LEASE_GRANTED) ||
		(tdnp->d_dur_handle.flags & SMB2_LEASE_BROKEN)) {
		tdnp->d_changecnt++;
	}
	lck_mtx_unlock(&tdnp->d_dur_handle.lock);
	
	/* blow away statfs cache */
    smp->sm_statfstime = 0;
    
    /* Invalidate negative cache entries in destination dir */
    if (tdnp->n_flag & NNEGNCENTRIES) {
        tdnp->n_flag &= ~NNEGNCENTRIES;
        cache_purge_negatives(tdvp);
    }
	
out:
	/* We only have a share if we obtain a reference on it, so release it */
	if (share) {
		smb_share_rele(share, ap->a_context);
	}
    
    if (need_unlock) {
        smbnode_unlockpair(fnp, tdnp);
    }
    
    SMB_LOG_KTRACE(SMB_DBG_COPYFILE | DBG_FUNC_END, error, 0, 0, 0, 0);
	return (error);
}

static uint32_t emptyfinfo[8] = {0};
/*
 * DefaultFillAfpInfo
 *
 * Given a buffer fill in the default AfpInfo values.
 */
static void 
DefaultFillAfpInfo(uint8_t *afpinfo)
{
	int ii = 0;
	bzero(afpinfo, AFP_INFO_SIZE);
		/* Signature is a DWORD. Must be *(PDWORDD)"AFP" */
	afpinfo[ii++] = 'A';
	afpinfo[ii++] = 'F';
	afpinfo[ii++] = 'P';
	afpinfo[ii++] = 0;
		/* Version is a DWORD. Must be 0x00010000 (byte swapped) */
	afpinfo[ii++] = 0;
	afpinfo[ii++] = 0;
	afpinfo[ii++] = 0x01;
	afpinfo[ii++] = 0;
		/* Reserved1 is a DWORD */
	ii += 4;
	/* 
	 * Backup time is a DWORD. Backup time for the file/dir. Not set equals 
	 * 0x80010000 (byte swapped) 
	 */
	afpinfo[ii++] = 0;
	afpinfo[ii++] = 0;
	afpinfo[ii++] = 0;
	afpinfo[ii] = 0x80;
	/* Finder Info is 32 bytes. Calling process fills this in */
	/* ProDos Info is 6 bytes. Leave set to zero? */
	/* Reserved2 is 6 bytes */
}

/* 
 * xattr2sfm
 *
 * See if this xattr is really the resource fork or the finder info stream. If so
 * return the correct streams name otherwise just return the name passed to us.
 */
static const char *
xattr2sfm(const char *xa, enum stream_types *stype)
{
    size_t in_len;
    
    in_len = strlen(xa);
    
	/* Never let them use the SFM Stream Names */
	if ((in_len == strlen(SFM_RESOURCEFORK_NAME)) &&
        !bcmp(xa, SFM_RESOURCEFORK_NAME, sizeof(SFM_RESOURCEFORK_NAME))) {
		return(NULL);
	}
	if ((in_len == strlen(SFM_FINDERINFO_NAME)) &&
        !bcmp(xa, SFM_FINDERINFO_NAME, sizeof(SFM_FINDERINFO_NAME))) {
		return(NULL);
	}
	if ((in_len == strlen(SFM_DESKTOP_NAME)) &&
        !bcmp(xa, SFM_DESKTOP_NAME, sizeof(SFM_DESKTOP_NAME))) {
		return(NULL);
	}
	if ((in_len == strlen(SFM_IDINDEX_NAME)) &&
        !bcmp(xa, SFM_IDINDEX_NAME, sizeof(SFM_IDINDEX_NAME))) {
		return(NULL);
	}
	
	if ((in_len == strlen(XATTR_RESOURCEFORK_NAME)) &&
        !bcmp(xa, XATTR_RESOURCEFORK_NAME, sizeof(XATTR_RESOURCEFORK_NAME))) {
		*stype = kResourceFrk;
		return (SFM_RESOURCEFORK_NAME);
	}
	if ((in_len == strlen(XATTR_FINDERINFO_NAME)) &&
        !bcmp(xa, XATTR_FINDERINFO_NAME, sizeof(XATTR_FINDERINFO_NAME))) {
		*stype = kFinderInfo;
		return (SFM_FINDERINFO_NAME);
	}
	*stype = kExtendedAttr;
	return (xa);
}

/*
 * smbfs_vnop_setxattr
 *
 *	vnode_t a_vp;
 *	int8_t * a_name;
 *	uio_t a_uio;
 *	int32_t a_options;
 *	vfs_context_t a_context;
 */
static int 
smbfs_vnop_setxattr(struct vnop_setxattr_args *ap)
{
	vnode_t vp = ap->a_vp;
	const char *sfmname;
	int error = 0;
	SMBFID fid = 0;
	uint32_t rights = SMB2_FILE_WRITE_DATA;
	struct smbnode *np = NULL;
	struct smb_share *share = NULL;
	enum stream_types stype = kNoStream;
	uint32_t open_disp = 0;
	uio_t afp_uio = NULL;
	uint8_t	afpinfo[60];
	struct smbfattr fattr;
    u_int16_t newFinderFlags;
	int old_dosattr = 0;
	
	DBG_ASSERT(!vnode_isnamedstream(vp));
	
	if ((error = smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK)))
		return (error);
    
    SMB_LOG_KTRACE(SMB_DBG_SET_XATTR | DBG_FUNC_START, 0, 0, 0, 0, 0);

	np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_setxattr;
	share = smb_get_share_with_reference(VTOSMBFS(vp));

    /*
	 * FILE_NAMED_STREAMS tells us the server supports streams. 
	 * NOTE: This flag may have been overwriten by us in smbfs_mount. The default
	 * is for streams to be turn off. See the smbfs_mount for more details.
	 */ 
	if (!(share->ss_attributes & FILE_NAMED_STREAMS)) {
		error = ENOTSUP;
		goto exit;
	}

	/* You cant have both of these set at the same time. */
	if ( (ap->a_options & XATTR_CREATE) && (ap->a_options & XATTR_REPLACE) ) {
		error = EINVAL;	
		goto exit;
	}
	
	/* SMB doesn't support having a slash in the xattr name */
	if (strchr(ap->a_name, '/')) {
		error = EINVAL;
        SMBWARNING_LOCK(np, "Slash in xattr name not allowed: error %d %s:%s\n", error,
                        np->n_name, ap->a_name);
		goto exit;
	}
	sfmname = xattr2sfm(ap->a_name, &stype);
	if (!sfmname) {
		error = EINVAL;	
		goto exit;		
	}
	
	/* 
	 * Need to add write attributes if we want to reset the modify time. We never do this
	 * for the resource fork. The file manager expects the modify time to change if the 
	 * resource fork changes.
	 */
	if ((stype & kResourceFrk) != kResourceFrk) {
		rights |= SMB2_FILE_WRITE_ATTRIBUTES;
	} else {
		/* Never allow Resource Fork to be set on directories. */
		if (vnode_isdir(vp)) {
			error = EPERM;
			goto exit;
		}
	}

	/* 
	 * We treat finder info differently than any other EA/Stream. Because of 
	 * SFM we need to do things a little different. Remember the AFPInfo stream 
	 * has more information in it than just the finder info. WARNING: SFM can 
	 * get very confused if you do not handle this correctly!   
	 */
	if (stype & kFinderInfo) {
		uint8_t finfo[FINDERINFOSIZE];
		time_t attrtimeo;
		struct timespec ts;
		size_t sizep;
		int len = (int)uio_resid(ap->a_uio);

		/* Can't be larger that 32 bytes */
		if (len > FINDERINFOSIZE) {
			error = EINVAL;
			goto exit;
		}
        
        bzero(finfo, FINDERINFOSIZE);
		error = uiomove((void *)finfo, len, ap->a_uio);
		if (error) {
			goto exit;
        }
		
		/* Save current attributes to see if they change later on */
		old_dosattr = np->n_dosattr;
		
        if (len > 10) {
            /* 
             * See if Finder Flags (offset 0x08) have the Invisible bit 
             * set (0x4000) or not 
             */
            newFinderFlags = *((uint16_t *) &finfo[8]);
            newFinderFlags = ntohs(newFinderFlags);
            
            if (newFinderFlags & 0x4000) {
                np->n_dosattr |= SMB_EFA_HIDDEN;
            }
            else {
                np->n_dosattr &= ~SMB_EFA_HIDDEN;
            }
        }

        SMB_CACHE_TIME(ts, np, attrtimeo);
		/* 
		 * The Finder Info cache hasn't expired so check to see if they are 
		 * setting Finder Info to something different or not. If the Finder
		 * Info is unchanged, then skip trying to set it as this is what AFP 
		 * Client does.
		 *
		 * Also check to see if DOS attributes changed (ie Invisible bit)
		 */
		if ((ts.tv_sec - np->finfo_cache_timer) <= attrtimeo) {
			if ((bcmp(np->finfo, finfo, sizeof(finfo)) == 0) &&
				(old_dosattr == np->n_dosattr)) {
				goto exit;
			}
		}
        
		/* We want to read also in this case.  */
		rights |= SMB2_FILE_READ_DATA;
        
		/* Create a dummy uio structure */
		afp_uio = uio_create(1, 0, UIO_SYSSPACE, UIO_READ);
		if (afp_uio) {
			error = uio_addiov( afp_uio, CAST_USER_ADDR_T(afpinfo), sizeof(afpinfo));
		}
		else {
			error = ENOMEM;
		}
		if (error) {
			goto exit;
		}
		
		/* Now set the default afp info buffer */
		DefaultFillAfpInfo(afpinfo);
        
		/* Open and read the data in, if an empty file we will still get an fid */
		error = smbfs_smb_openread(share, np,
                                   &fid, rights,
                                   afp_uio, &sizep, sfmname,
                                   &ts, ap->a_context);

		SMB_LOG_KTRACE(SMB_DBG_SET_XATTR | DBG_FUNC_NONE,
                       0xabc001, error, stype, 0, 0);

        /* Replace the finder info with the data that was passed down. */
		if (!error) {
			bcopy((void *)finfo, (void *)&afpinfo[AFP_INFO_FINDER_OFFSET], len);
			uio_reset(afp_uio, 0, UIO_SYSSPACE, UIO_WRITE );
			error = uio_addiov( afp_uio, CAST_USER_ADDR_T(afpinfo), sizeof(afpinfo));			
		}
		else {
			/*
			 * <29089937/28829780> Is this the smbx server not letting us open
			 * Finder Info on a reparse point? If so, then silently fail with
			 * no error to workaround it.
			 */
			if ((vnode_vtype(SMBTOV(np)) == VLNK) &&
				(SS_TO_SESSION(share)->session_misc_flags & SMBV_OSX_SERVER)) {
				error = 0;
			}
			
			goto out;
		}
		
		/* Truncate the stream if there is anything in it, this will wake up SFM */
		if (sizep && (VTOSMBFS(vp)->sm_flags & MNT_IS_SFM_VOLUME))	{
			 /* Ignore any errors, write will catch them */
			(void)smbfs_smb_seteof(share, fid, 0, ap->a_context);
		}
        
		/* Now we can write the afp info back out with the new finder information */
		if (!error) {
			error = smb_smb_write(share, fid, afp_uio, 0, ap->a_context);
            SMB_LOG_KTRACE(SMB_DBG_SET_XATTR | DBG_FUNC_NONE,
                           0xabc002, error, stype, 0, 0);
		}
        
		/* 
		 * Try to set the modify time back to the original time, ignore any 
		 * errors. Since we are using the open stream file descriptor to change 
		 * the time remove the directory attribute bit if set.
		 */
		(void)smbfs_smb_setfattrNT(share, (np->n_dosattr & ~SMB_EFA_DIRECTORY), 
								   fid, NULL, &ts, NULL, ap->a_context);
		/* Reset our cache timer and copy the new data into our cache */
		if (!error) {
			nanouptime(&ts);
			np->finfo_cache_timer = ts.tv_sec;
			bcopy((void *)&afpinfo[AFP_INFO_FINDER_OFFSET], np->finfo, 
				  sizeof(np->finfo));

            smb_dir_cache_invalidate(vp, 0);
        }
		goto out;
	}

	switch(ap->a_options & (XATTR_CREATE | XATTR_REPLACE)) {
		case XATTR_CREATE:	/* set the value, fail if attr already exists */
            /* if exists fail else create it */
			open_disp = FILE_CREATE;
			break;
		case XATTR_REPLACE:	/* set the value, fail if attr does not exist */
            /* if exists overwrite item else fail */
			open_disp = FILE_OVERWRITE;
			break;
		default:
			if ((stype & kResourceFrk) == kResourceFrk) {
				/* if resource fork then if it exists open it else create it */
				open_disp = FILE_OPEN_IF;
			} else {
				/* if anything else then if it exists overwrite it else create it */
				open_disp = FILE_OVERWRITE_IF;
			}
			break;
	}

    if (strcmp(ap->a_name, KAUTH_FILESEC_XATTR) == 0) {
        open_disp = FILE_OPEN_IF;
    }

	/* Open/create the stream */
	error = smbfs_smb_create(share, np, sfmname, 
							 strnlen(sfmname, share->ss_maxfilenamelen+1), 
							 rights, &fid, open_disp, 1, &fattr, ap->a_context);
    SMB_LOG_KTRACE(SMB_DBG_SET_XATTR | DBG_FUNC_NONE,
                   0xabc003, error, stype, 0, 0);
	if (error) {
		goto exit;
	}
    
	/* Now write out the stream data */
	error = smb_smb_write(share, fid, ap->a_uio, 0, ap->a_context);
    SMB_LOG_KTRACE(SMB_DBG_SET_XATTR | DBG_FUNC_NONE,
                   0xabc004, error, stype, 0, 0);
    
    if (error == 0) {
        smb_dir_cache_invalidate(vp, 0);
    }
    
	/*
	 * %%% 
	 * Should we reset the modify time back to the original time? Never for the  
	 * resource fork, but what about EAs? Could be a performance issue, really 
	 * need a clearer message from the rest of the file system team.
	 *
	 * For now try to set the modify time back to the original time, ignore any
	 * errors. Since we are using the open stream file descriptor to change the 
	 * time remove the directory attribute bit if set.	 
	 */
	if ((stype & kResourceFrk) != kResourceFrk) {
		(void)smbfs_smb_setfattrNT(share, (np->n_dosattr & ~SMB_EFA_DIRECTORY), 
								   fid, NULL, &np->n_mtime, NULL, ap->a_context);
	}
	
out:
	if (fid != 0) {
		(void)smbfs_smb_close(share, fid, ap->a_context);
    }
	
exit:
	if (afp_uio)
		uio_free(afp_uio);
		
	if (error == ENOENT)
		error = ENOATTR;

	/* Check to see if its a normal error */
	if (error && (error != ENOTSUP) && (error != ENOATTR)) {
		SMBWARNING_LOCK(np, "error %d %s:%s\n", error, np->n_name, ap->a_name);

		/* Always make sure its a legit error, see man listxattr */
		if ((error != EROFS) && (error != EPERM) && (error != EINVAL) && 
			(error != ENOTDIR) && (error != EACCES) && (error != ELOOP) && 
			(error != EFAULT) && (error != EIO) && (error != ENAMETOOLONG) &&
			(error != EEXIST) && (error != ERANGE) && 
			(error != E2BIG) && (error != ENOSPC))
			error = EIO;	/* Not sure what else to do here */
	}
	if (!error) {
		/* We create a named stream, so remove the no stream flag  */
		np->n_fstatus &= ~kNO_SUBSTREAMS;
	}

    smb_share_rele(share, ap->a_context);
	smbnode_unlock(np);


    SMB_LOG_KTRACE(SMB_DBG_SET_XATTR | DBG_FUNC_END, error, 0, 0, 0, 0);
    return (error);
}

/*
 * smbfs_vnop_listxattr
 *
 *	vnode_t a_vp;
 *	uio_t a_uio;
 *	size_t *a_size;
 *	int32_t a_options;
 *	vfs_context_t a_context;
 */
static int 
smbfs_vnop_listxattr(struct vnop_listxattr_args *ap)
{
	vnode_t vp = ap->a_vp;
	uio_t uio = ap->a_uio;
	size_t *sizep = ap->a_size;
	struct smbnode *np = NULL;
	struct smb_share *share = NULL;
	int error = 0;
    uint32_t stream_flags = 0;
    enum vtype vnode_type = VREG;
	user_ssize_t reply_buf_len = 0;

	DBG_ASSERT(!vnode_isnamedstream(vp));
	
	if ((error = smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK))) {
		return (error);
	}
	
    SMB_LOG_KTRACE(SMB_DBG_LIST_XATTR | DBG_FUNC_START, 0, 0, 0, 0, 0);

	np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_listxattr;
	share = smb_get_share_with_reference(VTOSMBFS(vp));

    /*
	 * FILE_NAMED_STREAMS tells us the server supports streams. 
	 * NOTE: This flag may have been overwriten by us in smbfs_mount. The default
	 * is for streams to be turn off. See the smbfs_mount for more details.
	 */ 
	if (!(share->ss_attributes & FILE_NAMED_STREAMS)) {
		error = ENOTSUP;
		goto exit;
	}
    
	if (np->n_fstatus & kNO_SUBSTREAMS) {
		error = ENOATTR;
		goto exit;
	}
	
	/* Save buffer len for checking for ERANGE errors */
	if (uio) {
		reply_buf_len = uio_resid(uio);
	}
	
    /* For listing xattrs, create is done on the item, not the stream */
    if ((np) && (np->n_vnode)) {
        /* Use vnode to determine type */
        vnode_type = vnode_isdir(np->n_vnode) ? VDIR : VREG;
    }

	error = smbfs_smb_qstreaminfo(share, np, vnode_type,
                                  NULL, 0,
                                  NULL,
                                  uio, sizep,
                                  NULL, NULL,
                                  &stream_flags, NULL,
                                  ap->a_context);
	
exit:
	/*
	 * From the man pages: If no accessible extended attributes are associated 
	 * with the given path or fd, the function returns zero.
	*/
	if (error == ENOATTR) {
		error = 0;
	}
	
	/* Check for ERANGE error */
	if ((error == 0) && (uio) && (sizep)) {
		if (*sizep > (size_t) reply_buf_len) {
			error = ERANGE;
		}
	}
	
	/* Check to see if its a normal error */
	if (error && (error != ENOTSUP)) {
		SMBWARNING_LOCK(np, "error %d %s\n", error, np->n_name);

		/* Always make sure its a legit error, see man listxattr */
		if ((error != ERANGE) && (error != EPERM) && (error != EINVAL) && 
			(error != ENOTDIR) && (error != EACCES) && (error != ELOOP) && 
			(error != EFAULT) && (error != EIO))
			error = 0;	/* Just pretend it doesn't exist */
	}

    smb_share_rele(share, ap->a_context);
	smbnode_unlock(np);

	SMB_LOG_KTRACE(SMB_DBG_LIST_XATTR | DBG_FUNC_END, error, 0, 0, 0, 0);
    return (error);
}

/*
 * vnop_removexattr_args
 *
 *	vnode_t a_vp;
 *	int8_t * a_name;
 *	int32_t a_options;
 *	vfs_context_t a_context;
 */
static int 
smbfs_vnop_removexattr(struct vnop_removexattr_args *ap)
{
	vnode_t vp = ap->a_vp;
	const char *sfmname;
	int error = 0, saved_error = 0;
	struct smbnode *np = NULL;
	struct smb_share *share = NULL;
	enum stream_types stype = kNoStream;
	struct timespec ts;

	DBG_ASSERT(!vnode_isnamedstream(vp));
	
	if ((error = smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK)))
		return (error);

	SMB_LOG_KTRACE(SMB_DBG_RM_XATTR | DBG_FUNC_START, 0, 0, 0, 0, 0);

    np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_removexattr;
	share = smb_get_share_with_reference(VTOSMBFS(vp));

    /*
	 * FILE_NAMED_STREAMS tells us the server supports streams. 
	 * NOTE: This flag may have been overwriten by us in smbfs_mount. The default
	 * is for streams to be turn off. See the smbfs_mount for more details.
	 */ 
	if (!(share->ss_attributes & FILE_NAMED_STREAMS)) {
		error = ENOTSUP;
		goto exit;
	}
	
	/* SMB doesn't support having a slash in the xattr name */
	if (strchr(ap->a_name, '/')) {
		error = EINVAL;
        SMBWARNING_LOCK(np, "Slash in xattr name not allowed: error %d %s:%s\n", error,
                        np->n_name, ap->a_name);
		goto exit;
	}
	
	sfmname = xattr2sfm(ap->a_name, &stype);
	if (!sfmname) {
		error = EINVAL;	
		goto exit;		
	}
	
	if (stype & kFinderInfo) {
		if (VTOSMBFS(vp)->sm_flags & MNT_IS_SFM_VOLUME) {
			/* 
			 * We do not allow them to remove the finder info stream on SFM 
			 * Volume. It could hold other information used by SFM. 
			 */
			error = ENOTSUP;
		}
        else {
			/*
			 * If the volume is just a normal NTFS Volume then deleting the named
			 * stream should be ok, but some servers (EMC) don't support deleting
			 * the named stream. In this case see if we can just zero out the
			 * finder info.
             *
             * Streams are always VREG since they are files
			 */
			error = smbfs_smb_delete(share, np, VREG,
                                     sfmname, strnlen(sfmname, share->ss_maxfilenamelen+1),
                                     1, ap->a_context);
            SMB_LOG_KTRACE(SMB_DBG_RM_XATTR | DBG_FUNC_NONE,
                           0xabc001, error, stype, 0, 0);
		}
        
		/* SFM server or the server doesn't support deleting named streams */
		if (error) {
			uio_t afp_uio = NULL;
			uint8_t	afpinfo[60];
			SMBFID fid = 0;
			uint32_t rights = SMB2_FILE_WRITE_DATA | SMB2_FILE_READ_DATA | SMB2_FILE_WRITE_ATTRIBUTES;	
			
			/* 
			 * Either a SFM volume or the server doesn't support deleting named
			 * streams. Try to zero out the finder info data.
			 */
            saved_error = error; /* save original failure error */
            
			afp_uio = uio_create(1, 0, UIO_SYSSPACE, UIO_READ);
			if (!afp_uio) {
				error = saved_error;
				goto exit;
			}
			error = uio_addiov( afp_uio, CAST_USER_ADDR_T(afpinfo), sizeof(afpinfo));
			if (error) {
				uio_free(afp_uio);
				error = saved_error;
				goto exit;
			}
			
			/* open and read the data */
			error = smbfs_smb_openread(share, np,
                                       &fid, rights,
                                       afp_uio, NULL, sfmname,
                                       &ts, ap->a_context);
            SMB_LOG_KTRACE(SMB_DBG_RM_XATTR | DBG_FUNC_NONE,
                           0xabc002, error, stype, 0, 0);

			/* clear out the finder info data */
			bzero(&afpinfo[AFP_INFO_FINDER_OFFSET], FINDERINFOSIZE);
			
			if (!error)	{
                /* truncate the stream, this will wake up SFM */
				error = smbfs_smb_seteof(share, fid, 0, ap->a_context);
            }

			/* Reset our uio */
			if (!error) {
				uio_reset(afp_uio, 0, UIO_SYSSPACE, UIO_WRITE );
				error = uio_addiov( afp_uio, CAST_USER_ADDR_T(afpinfo), sizeof(afpinfo));
			}
            
			if (!error) {
				error = smb_smb_write(share, fid, afp_uio, 0, ap->a_context);
                SMB_LOG_KTRACE(SMB_DBG_RM_XATTR | DBG_FUNC_NONE,
                               0xabc003, error, stype, 0, 0);
            }

            if (error) {
				error = saved_error; /* restore original error */
            }
            
			/* Try to set the modify time back to the original time, ignore any errors */
			(void)smbfs_smb_setfattrNT(share, (np->n_dosattr & ~SMB_EFA_DIRECTORY), fid, NULL, &ts,
									   NULL, ap->a_context);
			if (fid != 0)
				(void)smbfs_smb_close(share, fid, ap->a_context);
			if (afp_uio) {
				uio_free(afp_uio);
			}
		}
	}
    else {
        /* Streams are always VREG since they are files */
		error = smbfs_smb_delete(share, np, VREG,
                                 sfmname, strnlen(sfmname, share->ss_maxfilenamelen+1),
                                 1, ap->a_context);
        SMB_LOG_KTRACE(SMB_DBG_RM_XATTR | DBG_FUNC_NONE,
                       0xabc004, error, stype, 0, 0);
	}

	/* Finder info so reset our cache timer and zero out our cache */
	if ((stype & kFinderInfo) && !error) {
		nanouptime(&ts);
		np->finfo_cache_timer = ts.tv_sec;
		bzero(np->finfo, sizeof(np->finfo));
	}
	
    smb_dir_cache_invalidate(vp, 0);

exit:
	if (error == ENOENT)
		error = ENOATTR;
	
	/* Check to see if its a normal error */
	if (error && (error != ENOTSUP) && (error != ENOATTR)) {
		SMBWARNING_LOCK(np, "error %d %s:%s\n", error, np->n_name, ap->a_name);

		/* Always make sure its a legit error, see man listxattr */
		if ((error != EROFS) && (error != EPERM) && (error != EINVAL) && 
			(error != ENOTDIR) && (error != EACCES) && (error != ELOOP) && 
			(error != EFAULT) && (error != EIO) && (error != ENAMETOOLONG))
			error = ENOATTR;	/* Not sure what else to do here */
	}

	smb_share_rele(share, ap->a_context);
	smbnode_unlock(np);

    SMB_LOG_KTRACE(SMB_DBG_RM_XATTR | DBG_FUNC_END, error, 0, 0, 0, 0);
    return (error);
}

/*
 * smbfs_vnop_getxattr
 *
 *	vnode_t a_vp;
 *	int8_t * a_name;
 *	uio_t a_uio;
 *	size_t *a_size;
 *	int32_t a_options;
 *	vfs_context_t a_context;
 */
static int 
smbfs_vnop_getxattr(struct vnop_getxattr_args *ap)
{
	vnode_t vp = ap->a_vp;
	const char *sfmname;
	uio_t uio = ap->a_uio;
	size_t *sizep = ap->a_size;
	SMBFID fid = 0;
	int error = 0;
	struct smbnode *np = NULL;
	struct smb_share *share = NULL;
	size_t rq_resid = (uio) ? (size_t)uio_resid(uio) : 0;
	uio_t afp_uio = NULL;
	enum stream_types stype = kNoStream;
	struct timespec ts;
	time_t attrtimeo;
    uint32_t stream_flags = 0;
	int use_cached_data = 0;
    uint64_t strmsize = 0;
    uint64_t strm_alloc_size = 0;
    size_t afpsize = 0;
    uint8_t	afpinfo[60];
    enum vtype vnode_type = VREG; /* Streams are always files */
    struct smbfattr *fap = NULL;
    uint32_t ntstatus = 0;

	DBG_ASSERT(!vnode_isnamedstream(vp));
	
	if ((error = smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK)))
		return (error);

	
    SMB_LOG_KTRACE(SMB_DBG_GET_XATTR | DBG_FUNC_START, 0, 0, 0, 0, 0);

    np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_getxattr;
	share = smb_get_share_with_reference(VTOSMBFS(vp));

    /*
	 * FILE_NAMED_STREAMS tells us the server supports streams. 
	 * NOTE: This flag may have been overwriten by us in smbfs_mount. The default
	 *	 is for streams to be turn off. See the smbfs_mount for more details.
	 */ 
	if (!(share->ss_attributes & FILE_NAMED_STREAMS)) {
		error = ENOTSUP;
		goto exit;
	}
	
	/* SMB doesn't support having a slash in the xattr name */
	if (strchr(ap->a_name, '/')) {
		error = ENOTSUP;
		SMBWARNING_LOCK(np, "Slash in xattr name not allowed: error %d %s:%s\n", error,
                        np->n_name, ap->a_name);
		goto exit;
	}
	
	sfmname = xattr2sfm(ap->a_name, &stype);
	if (!sfmname) {
		error = EINVAL;	
		goto exit;		
	}
		
	/*
     * <6804472> <26559530> Return no error if resource fork length is 0
     * Always allow KAUTH_FILESEC_XATTR to be checked
     */
	if (!(stype & kResourceFrk) &&
        !(strcmp(ap->a_name, KAUTH_FILESEC_XATTR) == 0) &&
        (np->n_fstatus & kNO_SUBSTREAMS)) {
		error = ENOATTR;
		goto exit;
	}
	
	/* They just want the size of the stream. */
	if ((uio == NULL) && !(stype & kFinderInfo)) {
		if (stype & kResourceFrk) {
			if (vnode_isdir(vp)) {
				/*
				 * See comment above about not returning an error
				 * in thise case.
				 */
				strmsize = 0;
			} else {
				error = smb_get_rsrcfrk_size(share, vp, ap->a_context);
				lck_mtx_lock(&np->rfrkMetaLock);
				/*
				 * <6804472> <26559530> Return no error if
				 * resource fork len is 0
				 *
				 * The node's rfork size will have the correct
				 * value at this point
				 */
				strmsize = np->rfrk_size;
				lck_mtx_unlock(&np->rfrkMetaLock);
			}
		} else {
			/*
			 * For getting xattrs, create is done on the item,
			 * not the stream
			 */
			if ((np) && (np->n_vnode)) {
				/* Use vnode to determine type */
				vnode_type = vnode_isdir(np->n_vnode) ? VDIR : VREG;
			}

            if (strcmp(ap->a_name, KAUTH_FILESEC_XATTR) == 0) {
                /*
                 * For the ACL xattr, do a create/close to get its size.
                 * QueryInfo for named streams wont find it because its a
                 * hidden xattr
                 */
                SMB_MALLOC(fap,
                           struct smbfattr *,
                           sizeof(struct smbfattr),
                           M_SMBTEMP,
                           M_WAITOK | M_ZERO);
                if (fap == NULL) {
                    SMBERROR("SMB_MALLOC failed\n");
                    error = ENOMEM;
                    goto exit;
                }

                error = smb2fs_smb_cmpd_create(share, np,
                                               NULL, 0,
                                               KAUTH_FILESEC_XATTR, strlen(KAUTH_FILESEC_XATTR),
                                               SMB2_FILE_READ_DATA, vnode_type,
                                               NTCREATEX_SHARE_ACCESS_ALL, FILE_OPEN,
                                               0, &ntstatus,
                                               NULL, fap,
                                               NULL, ap->a_context);
                strmsize = fap->fa_size;
                SMB_FREE(fap, M_SMBTEMP);
                fap = NULL;
            }
            else {
                error = smbfs_smb_qstreaminfo(share, np, vnode_type,
                                              NULL, 0,
                                              sfmname,
                                              NULL, NULL,
                                              &strmsize, &strm_alloc_size,
                                              &stream_flags, NULL,
                                              ap->a_context);
            }
            SMB_LOG_KTRACE(SMB_DBG_GET_XATTR | DBG_FUNC_NONE,
                           0xabc001, error, stype, 0, 0);
		}
		
        if (sizep) {
            *sizep = (size_t)strmsize;
        }

		if (error) {
			error = ENOATTR;
		}
		
		goto exit;
	}
	
	/*
	 * We treat finder info differently than any other EA/Stream. Because of SFM
	 * we need to do things a little different. Remember the AFPInfo stream has 
	 * more information in it than just the finder info. WARNING: SFM can get 
	 * very confused if you do not handle this correctly!   
	 */
	if (stype & kFinderInfo) {
		/* If we are in reconnect, use cached data if we have it */
		if (np->finfo_cache_timer != 0) {
			use_cached_data = (share->ss_flags & SMBS_RECONNECTING);
		}

		/* Check to see if the cache has timed out */
		SMB_CACHE_TIME(ts, np, attrtimeo);
		if (((ts.tv_sec - np->finfo_cache_timer) > attrtimeo) &&
		    !use_cached_data) {
			/* Cache has expired go get the finder information. */
			afp_uio = uio_create(1, 0, UIO_SYSSPACE, UIO_READ);
			if (afp_uio) {
				error = uio_addiov(afp_uio, CAST_USER_ADDR_T(afpinfo),
						   sizeof(afpinfo));
			} else {
				error = ENOMEM;
			}

			if (error) {
				goto exit;
			}

			uio_setoffset(afp_uio, 0);
			
			/* open and read the data */
			if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
				/* SMB 2/3 will do create/read/close */
				error = smbfs_smb_openread(share, np, NULL,
							   SMB2_FILE_READ_DATA,
							   afp_uio, &afpsize, sfmname,
							   NULL, ap->a_context);
				fid = 0;
			} else {
				/* SMB 1 will do create/read */
				error = smbfs_smb_openread(share, np, &fid,
							   SMB2_FILE_READ_DATA,
							   afp_uio, &afpsize, sfmname,
							   NULL, ap->a_context);
			}

			SMB_LOG_KTRACE(SMB_DBG_GET_XATTR | DBG_FUNC_NONE,
				       0xabc002, error, stype, 0, 0);

			if ((error == ETIMEDOUT) && (np->finfo_cache_timer != 0)) {
				/* Just return the cached data */
				error = 0;
				goto done;
			}

			/* Should never happen but just in case */
			if (afpsize != AFP_INFO_SIZE) {
				error = ENOENT;
			}

			/* If ENOENT, return zero'd Finder Info, else return the info */
			if (error == ENOENT) {
				bzero(np->finfo, sizeof(np->finfo));
			} else {
				bcopy((void *)&afpinfo[AFP_INFO_FINDER_OFFSET], np->finfo, 
				      sizeof(np->finfo));
			}
            
			/* Cache the finder info as long as its not a copy in progress */
			if (vnode_isreg(vp) && (bcmp(np->finfo, "brokMACS", 8) == 0)) {
				np->finfo_cache_timer = 0;
				SMBDEBUG("Don't cache finder info, we have a finder copy in progress\n");
			} else {
				nanouptime(&ts);
				np->finfo_cache_timer = ts.tv_sec;		
			}
		}

done:
		/* If the finder info is all zero hide it, except if its a SFM volume */ 
		if ((!(VTOSMBFS(vp)->sm_flags & MNT_IS_SFM_VOLUME)) && 
			(bcmp(np->finfo, emptyfinfo, sizeof(emptyfinfo)) == 0)) {
				error = ENOENT;
		}

		if (uio && !error) {
			error = uiomove((const char *)np->finfo, (int)sizeof(np->finfo), ap->a_uio);
		}

		if (sizep && !error) 
			*sizep = FINDERINFOSIZE; 
	} else {
		if ((stype & kResourceFrk) && vnode_isdir(vp)) {
			/* Directories never have a Resource Fork. */
			error = ENOATTR;
		} else {
			error = smbfs_smb_openread(share, np, &fid,
						   SMB2_FILE_READ_DATA,
						   uio, sizep, sfmname,
						   NULL, ap->a_context);
		}
		SMB_LOG_KTRACE(SMB_DBG_GET_XATTR | DBG_FUNC_NONE,
			       0xabc003, error, stype, 0, 0);
	}
	/*
	 * If ENOTSUP support is returned then do the open and read
	 * in two transactions.
	 */
	if (error != ENOTSUP)
		goto out;
		
	/* 
	 * May need to add an oplock to this open call, if this is a finder info open.
	 * Not sure I remember the exact details, something about deletes.
	 */
	error = smbfs_smb_open_xattr(share, np, SMB2_FILE_READ_DATA,
				     NTCREATEX_SHARE_ACCESS_ALL, &fid,
				     sfmname, sizep, ap->a_context);

    SMB_LOG_KTRACE(SMB_DBG_GET_XATTR | DBG_FUNC_NONE,
		       0xabc004, error, stype, 0, 0);
	if (error)
		goto exit;
		
	/*
	 * When reading finder-info, munge the uio so we read at offset 16 where the  
	 * actual finder info is located. Also ensure we don't read past the 32 bytes,
	 * of finder info. Since we are just reading we really don't care about the 
	 * rest of the data.
	 * This is only here in case a server does not support the chain message above.
	 * We do not cache in this case. Should never happen, but just to be safe.
	 */
	if (stype & kFinderInfo) {
		user_ssize_t r;

		if (sizep)
			*sizep = FINDERINFOSIZE;
		/* Just wanted the size get out */
		if (uio == NULL)
			goto out;

		r = uio_resid(uio);
		if (uio_offset(uio) >= FINDERINFOSIZE) {
			uio_setresid(uio, 0);
		} else if (uio_offset(uio) + r > FINDERINFOSIZE)
		uio_setresid(uio, FINDERINFOSIZE - uio_offset(uio));
		r = r - uio_resid(uio);
		uio_setoffset(uio, uio_offset(uio) + 4*4);
		
		error = smb_smb_read(share, fid, uio, ap->a_context);
		
		uio_setoffset(uio, uio_offset(uio) - 4*4);
		uio_setresid(uio, uio_resid(uio) + r);
	} else {
		error = smb_smb_read(share, fid, uio, ap->a_context);
	}

	SMB_LOG_KTRACE(SMB_DBG_GET_XATTR | DBG_FUNC_NONE,
		       0xabc005, error, stype, 0, 0);

out:;
	if (uio && sizep && (*sizep > rq_resid))
			error = ERANGE;
		
	/* Even an error can leave the file open. */
	if (fid != 0)	
		(void)smbfs_smb_close(share, fid, ap->a_context);
exit:
	/* 
	 * So ENOENT just means ENOATTR. 
	 * Note: SAMBA 4 will reutrn EISDIR for folders which is legit, but not 
	 * expected by the finder 
	 */
	if ((error == ENOENT) || ((error == EISDIR) && (stype & kFinderInfo)))
		error = ENOATTR;
	
	if (afp_uio)
		uio_free(afp_uio);

	/* Check to see if its a normal error */
	if (error && (error != ENOTSUP) && (error != ENOATTR)) {
		SMBWARNING_LOCK(np, "error %d %s:%s\n", error, np->n_name, ap->a_name);

		/* Nope make sure its a legit error, see man getxattr */
		if ((error != ERANGE) && (error != EPERM) && (error != EINVAL) && 
			(error != EISDIR) && (error != ENOTDIR) && (error != EACCES) &&
			(error != ELOOP) && (error != EFAULT) && (error != EIO))
			error = ENOATTR;		
	}

    smb_share_rele(share, ap->a_context);
	smbnode_unlock(np);

    SMB_LOG_KTRACE(SMB_DBG_GET_XATTR | DBG_FUNC_END, error, 0, 0, 0, 0);
	return (error);
}

/*
 * smbfs_vnop_getnamedstream - Obtain the vnode for a stream.
 *
 *	vnode_t a_vp;
 *	vnode_t *a_svpp;
 *	const char *a_name;
	enum nsoperation a_operation; (NS_OPEN, NS_CREATE, NS_DELETE)
 *	vfs_context_t a_context;
 *
 */
static int 
smbfs_vnop_getnamedstream(struct vnop_getnamedstream_args* ap)
{
	struct smb_share *share;
	vnode_t vp = ap->a_vp;
	vnode_t *svpp = ap->a_svpp;
	const char * streamname = ap->a_name;
	const char * sname = ap->a_name;
	struct smbnode *np = NULL;
	int error = 0;
	uint64_t strmsize = 0;
	uint64_t strm_alloc_size = 0;
	struct smbfattr fattr;
	struct vnode_attr vap;
	struct timespec ts;
	time_t attrtimeo;
	struct timespec reqtime;
    uint32_t stream_flags = 0;
	int use_cached_data = 0;
    enum vtype vnode_type = VREG;

	/* Lock the parent while we look for the stream */
	if ((error = smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK)))
		return (error);
	
    SMB_LOG_KTRACE(SMB_DBG_GET_NSTREAM | DBG_FUNC_START, 0, 0, 0, 0, 0);

	nanouptime(&reqtime);
	np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_getnamedstream;
	share = smb_get_share_with_reference(VTOSMBFS(vp));
	
	*svpp = NULL;
	/* Currently we only support the "com.apple.ResourceFork" stream. */
	if (bcmp(streamname, XATTR_RESOURCEFORK_NAME, sizeof(XATTR_RESOURCEFORK_NAME)) != 0) {
		SMBDEBUG_LOCK(np, "Wrong stream %s:$%s\n", np->n_name, streamname);
		error = ENOATTR;
		goto exit;
	} else {
	    sname = SFM_RESOURCEFORK_NAME;	/* This is the resource stream use the SFM name */
	}
	if ( !vnode_isreg(vp) ) {
		SMBDEBUG_LOCK(np, "%s not a file (EPERM)\n", np->n_name);
		error = EPERM;
		goto exit;
	}

	/*
	 * %%%
	 * Since we have the parent node update its meta cache. Remember that 
	 * smbfs_getattr will check to see if the cache has expired. May want to 
	 * look at this and see  how it affects performance.
	 */
	VATTR_INIT(&vap);	/* Really don't care about the vap */
	error = smbfs_getattr(share, vp, &vap, ap->a_context);
	if (error) {
		SMBERROR_LOCK(np, "%s lookup failed %d\n", np->n_name, error);
		goto exit;
	}

	/*
	 * If we already have the stream vnode in our hash table and its cache timer
	 * has not expired then just return we are done.
	 */
	if ((*svpp = smbfs_find_vgetstrm(VTOSMBFS(vp), np, sname, 
									 share->ss_maxfilenamelen)) != NULL) {
        /* If we are in reconnect, use cached data if we have it */
        if (np->attribute_cache_timer != 0) {
            use_cached_data = (share->ss_flags & SMBS_RECONNECTING);
        }

		VTOSMB(*svpp)->n_mtime = np->n_mtime;	/* update the modify time */

        /* Check to see if the cache has timed out */
        SMB_CACHE_TIME(ts, VTOSMB(*svpp), attrtimeo);
		if (((ts.tv_sec - VTOSMB(*svpp)->attribute_cache_timer) <= attrtimeo) ||
            use_cached_data) {
			/* The cache is up to date, we are done */
            goto exit;
        }
	}
	
    if ((np) && (np->n_vnode)) {
        /* Use vnode to determine type */
        vnode_type = vnode_isdir(np->n_vnode) ? VDIR : VREG;
    }

	/*
	 * Lookup the stream and get its size. This call will fail if the server 
	 * tells us the stream does not exist. 
	 *
	 * NOTE1: If this is the resource stream then making this call will update 
	 * the the data fork  node's resource size and its resource cache timer. 
	 *
	 * NOTE2: SAMBA will not return the resource stream if the size is zero. 
	 *
	 * NOTE3: We always try to create the stream on an open. Because of note two.
	 *
	 * If smbfs_smb_qstreaminfo returns an error and we do not have the stream  
	 * node in our hash table then it doesn't exist and they will have to create 
	 * it.
	 *
	 * If smbfs_smb_qstreaminfo returns an error and we do  have the stream node  
	 * in our hash table then it could exist so just pretend that it does for 
	 * now. If they try to open it and it doesn't exist the open will create it.
	 *
	 * If smbfs_smb_qstreaminfo returns no error and we do have the stream node 
	 * in our hash table then just update its size and cache timers.
	 *
	 * If smbfs_smb_qstreaminfo returns no error and we do not have the stream 
	 * node in our hash table then create the stream node, using the data node 
	 * to fill in all information except the size.
	 */
    error = smbfs_smb_qstreaminfo(share, np, vnode_type,
                                  NULL, 0,
                                  sname,
                                  NULL, NULL,
                                  &strmsize, &strm_alloc_size,
                                  &stream_flags, NULL,
                                  ap->a_context);
	if (error && (*svpp == NULL)) {
		error = ENOATTR;
		goto exit;		
	}
    
    if ((error == ETIMEDOUT) && (np->attribute_cache_timer != 0)) {
        /* Just return the cached data */
        error = 0;
        goto exit;
    }

	/*
	 * We already have the stream vnode. If it doesn't exist we will attempt to
	 * create it on the open. In the SMB open you can say create it if it does 
	 * not exist. Reset the size if the above called failed then set the size to 
	 * zero.
	 */
	if (*svpp) {		
		if (smbfs_update_size(VTOSMB(*svpp), &reqtime, strmsize, NULL) == TRUE) {
			/* Remember the only attribute for a stream is its size */
			nanouptime(&ts);
			VTOSMB(*svpp)->attribute_cache_timer = ts.tv_sec;			
		}
		goto exit;	/* We have everything we need, so we are done */
	}	
	
	bzero(&fattr, sizeof(fattr));
	fattr.fa_vtype = VREG;		/* Streams are always regular files */
    fattr.fa_valid_mask |= FA_VTYPE_VALID;
	fattr.fa_size = strmsize;	/* Fill in the stream size */
	fattr.fa_data_alloc = 0;	/* %%% not sure this really matters */	
	/* Now for the rest of the information we just use the data node information */
	fattr.fa_attr = np->n_dosattr;
	fattr.fa_atime = np->n_atime;	/* Access Time */
	fattr.fa_chtime = np->n_chtime;	/* Change Time */
	fattr.fa_mtime = np->n_mtime;	/* Modify Time */
	fattr.fa_crtime = np->n_crtime;	/* Create Time */
	/* Stream inode number has same inode number as data node */
    fattr.fa_ino = np->n_ino;       
	nanouptime(&fattr.fa_reqtime);
	error = smbfs_vgetstrm(share, VTOSMBFS(vp), vp, svpp, &fattr, sname);

exit:
	if (*svpp)
		smbnode_unlock(VTOSMB(*svpp));	/* We are done with the node unlock it. */

	if (error && (error != ENOATTR)) {
		SMBWARNING_LOCK(np, " %s:$%s Original Stream name %s error = %d\n",
                        np->n_name, sname, streamname, error);
	}
	smb_share_rele(share, ap->a_context);
	smbnode_unlock(np);
    
    SMB_LOG_KTRACE(SMB_DBG_GET_NSTREAM | DBG_FUNC_END, error, 0, 0, 0, 0);
	return (error);
}

/*
 * smbfs_vnop_makenamedstream - Create a stream.
 *
 *	vnode_t a_vp;
 *	vnode_t *a_svpp;
 *	const char *a_name;
 *	vfs_context_t a_context;
 */
static int 
smbfs_vnop_makenamedstream(struct vnop_makenamedstream_args* ap)
{
	struct smb_share *share;
	vnode_t vp = ap->a_vp;
	vnode_t *svpp = ap->a_svpp;
	const char * streamname = ap->a_name;
	struct smbnode *np = NULL;
	int error = 0;
	struct smbfattr fattr;
	struct timespec ts;
	int rsrcfrk = FALSE;
	size_t max_name_len;
	
	/* Lock the parent while we create the stream */
	if ((error = smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK)))
		return (error);

	SMB_LOG_KTRACE(SMB_DBG_MAKE_NSTREAM | DBG_FUNC_START, 0, 0, 0, 0, 0);
    
    np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_makenamedstream;
	share = smb_get_share_with_reference(VTOSMBFS(vp));
	
	*svpp = NULL;
    
	/* Currently we only support the "com.apple.ResourceFork" stream. */
	if (bcmp(streamname, XATTR_RESOURCEFORK_NAME, 
			 sizeof(XATTR_RESOURCEFORK_NAME)) != 0) {
		SMBDEBUG_LOCK(np, "Wrong stream %s:$%s\n", np->n_name, streamname);

		/* max_name_len = strnlen(streamname, share->ss_maxfilenamelen+1) */
		error = ENOATTR;
		goto exit;
	} else {
		max_name_len = sizeof(XATTR_RESOURCEFORK_NAME);
		/* This is the resource stream use the SFM name */	
	    streamname = SFM_RESOURCEFORK_NAME;	
	}
	
	if ( !vnode_isreg(vp) ) {
		SMBDEBUG_LOCK(np, "%s not a file (EPERM)\n", np->n_name);
		error = EPERM;
		goto exit;
	}
	
	/* Now create the stream, sending a null fid pointer will cause it to be closed */
	error = smbfs_smb_create(share, np, streamname, max_name_len, 
							 SMB2_FILE_WRITE_DATA, NULL, 
							 FILE_OPEN_IF, 1, &fattr, ap->a_context);
	if (error)
		goto exit;
    
    smb_dir_cache_invalidate(vp, 0);

    /* We create a named stream, so remove the no stream flag  */
	np->n_fstatus &= ~kNO_SUBSTREAMS;
		
	/* Stream inode number has same inode number as data node */
    fattr.fa_ino = np->n_ino;

	error = smbfs_vgetstrm(share, VTOSMBFS(vp), vp, svpp, &fattr, streamname);
	if (error == 0) {
		if (rsrcfrk) /* Update the data nodes resource size */ {
			lck_mtx_lock(&np->rfrkMetaLock);
			np->rfrk_size = fattr.fa_size;
            /* assume alloc size is the same */
			np->rfrk_alloc_size = fattr.fa_size;
			nanouptime(&ts);
			np->rfrk_cache_timer = ts.tv_sec;			
			lck_mtx_unlock(&np->rfrkMetaLock);
		}
		smbnode_unlock(VTOSMB(*svpp));	/* Done with the smbnode unlock it. */		
	}

exit:
	if (error) {
		SMBWARNING_LOCK(np, " %s:$%s error = %d\n", np->n_name, streamname, error);
    }
    
	smb_share_rele(share, ap->a_context);
	smbnode_unlock(np);

	SMB_LOG_KTRACE(SMB_DBG_MAKE_NSTREAM | DBG_FUNC_END, error, 0, 0, 0, 0);
    return (error);
}

/*
 * smbfs_vnop_removenamedstream - Remove a stream.
 *
 *	vnode_t a_vp;
 *	vnode_t a_svpp;
 *	const char *a_name;
 *	vfs_context_t a_context;
 */
static int 
smbfs_vnop_removenamedstream(struct vnop_removenamedstream_args* ap)
{
	vnode_t vp = ap->a_vp;
	vnode_t svp = ap->a_svp;
	const char * streamname = ap->a_name;
	struct smbnode *np = NULL;
	int error = 0;
	size_t max_name_len;
	struct smb_share *share = NULL;
		
	
	/* Lock the parent and stream while we delete the stream*/
	if ((error = smbnode_lockpair(VTOSMB(vp), VTOSMB(svp), SMBFS_EXCLUSIVE_LOCK)))
		return (error);

	SMB_LOG_KTRACE(SMB_DBG_RM_NSTREAM | DBG_FUNC_START, 0, 0, 0, 0, 0);

    np = VTOSMB(svp);
	np->n_lastvop = smbfs_vnop_removenamedstream;
	share = smb_get_share_with_reference(VTOSMBFS(vp));
    
	/* Currently we only support the "com.apple.ResourceFork" stream. */
	if (bcmp(streamname, XATTR_RESOURCEFORK_NAME, sizeof(XATTR_RESOURCEFORK_NAME)) != 0) {
		SMBDEBUG_LOCK(np, "Wrong stream %s:$%s\n", np->n_name, streamname);

		/* max_name_len = strnlen(streamname, share->ss_maxfilenamelen+1) */
		error = ENOATTR;
		goto exit;
	}
    else {
		max_name_len = sizeof(XATTR_RESOURCEFORK_NAME);
		/* This is the resource stream use the SFM name */
	    streamname = SFM_RESOURCEFORK_NAME;	
	}
	
	if ( !vnode_isreg(vp) ) {
		SMBDEBUG_LOCK(np, "%s not a file (EPERM)\n", np->n_name);
		error = EPERM;
		goto exit;
	}
    
    /* Streams are always VREG since they are files */
	error = smbfs_smb_delete(share, np, VREG,
                             streamname, max_name_len,
                             TRUE, ap->a_context);
    if (!error) {
		smb_vhashrem(np);
        smb_dir_cache_invalidate(vp, 0);
    }

exit:
	if (error) {
		SMBWARNING_LOCK(np, " %s:$%s error = %d\n",
                        np->n_name, streamname, error);
    }
    
	smb_share_rele(share, ap->a_context);
	smbnode_unlockpair(VTOSMB(vp), VTOSMB(svp));

	SMB_LOG_KTRACE(SMB_DBG_RM_NSTREAM | DBG_FUNC_END, error, 0, 0, 0, 0);
    return (error);
}

/*
 * smbfs_vnop_monitor - Monitor an item.
 *
 *	vnode_t a_vp;
 *  uint32_t a_unused_events;	- not used currently
 *  uint32_t a_flags;
 *				VNODE_MONITOR_BEGIN - setup notfication
 *				VNODE_MONITOR_END	- remove notfication
 *				VNODE_MONITOR_UPDATE	- change 
 *	void *a_handle;
 *				struct knote *
 *  vfs_context_t a_context;
 *
 */
static int 
smbfs_vnop_monitor(struct vnop_monitor_args *ap)
{
	struct smbnode *np;
	struct smb_share *share = NULL;
	int error = 0;
	int releaseLock = TRUE;
	
	/* Currently we only support directories */
	if (!vnode_isdir(ap->a_vp))	{
		SMBDEBUG_LOCK(VTOSMB(ap->a_vp), "%s is not a directory (ENOTSUP): node type = 0x%0x a_events = 0x%x, a_flags = 0x%x, a_handle = %p\n",
                      VTOSMB(ap->a_vp)->n_name, vnode_vtype(ap->a_vp),
                      ap->a_events, ap->a_flags, ap->a_handle);
		return ENOTSUP;
	} 
	
	if ((error = smbnode_lock(VTOSMB(ap->a_vp), SMBFS_EXCLUSIVE_LOCK)))
		return (error);
	
    SMB_LOG_KTRACE(SMB_DBG_MONITOR | DBG_FUNC_START, ap->a_flags, 0, 0, 0, 0);

	np = VTOSMB(ap->a_vp);
	np->n_lastvop = smbfs_vnop_monitor;
	share = smb_get_share_with_reference(VTOSMBFS(ap->a_vp));

    SMBDEBUG_LOCK(np, "%s a_events = 0x%x, a_flags = 0x%x, a_handle = %p\n",
                  np->n_name, ap->a_events, ap->a_flags, ap->a_handle);

	switch (ap->a_flags) {
		case VNODE_MONITOR_BEGIN:
			error = smbfs_start_change_notify(share, ap->a_vp, ap->a_context,
											  &releaseLock);
			break;
		case VNODE_MONITOR_END:
			error = smbfs_stop_change_notify(share, ap->a_vp, FALSE, ap->a_context,
											 &releaseLock);
			break;
		case VNODE_MONITOR_UPDATE: /* We no longer get called to update */
		default:
			error = ENOTSUP;
			break;
	}
	smb_share_rele(share, ap->a_context);
	if (releaseLock)
		smbnode_unlock(VTOSMB(ap->a_vp));

	SMB_LOG_KTRACE(SMB_DBG_MONITOR | DBG_FUNC_END, error, 0, 0, 0, 0);
    return error;
}

/*
 * smbfs_vnop_access - Check for access
 *
 *	vnode_t a_vp;
 *  int32_t a_action;
 *  vfs_context_t a_context;
 *
 */
static int 
smbfs_vnop_access(struct vnop_access_args *ap)
{
	vnode_t vp = ap->a_vp;
	int32_t action = ap->a_action, write_rights;
	vfs_context_t context = ap->a_context;
	kauth_cred_t cred = vfs_context_ucred(context);
	struct smbmount *smp = NULL;
	struct smb_share *share = NULL;
	uint32_t maxAccessRights;
	int error = 0;
	struct smb_session *sessionp = NULL;
	struct smbnode *np = NULL;
	
    SMB_LOG_KTRACE(SMB_DBG_ACCESS | DBG_FUNC_START, action, 0, 0, 0, 0);

	if (vp == NULL) {
		SMBERROR("vp is null \n");
		error = EACCES;
		goto done;
	}
	
	smp = VTOSMBFS(vp);
	np = VTOSMB(vp);
	
	share = smb_get_share_with_reference(smp);
	if (share == NULL) {
		SMBERROR("share is null \n");
		error = EACCES;
		goto done;
	}
	
	sessionp = SS_TO_SESSION(share);

	/*
	 * Not the root user, not the user that mounted the volume and the volume 
	 * wasn't mounted as guest then refuse all access. 
	 */
	if ((vfs_context_suser(context) != 0) &&
		(kauth_cred_getuid(cred) != smp->sm_args.uid)) {
		/*
		 * <28555880> If its not Guest OR its a TM mount, OR its a HiFi mount,
         * then the access check failed.
		 */
		if ((!SMBV_HAS_GUEST_ACCESS(sessionp)) ||
			(sessionp->session_misc_flags & SMBV_MNT_TIME_MACHINE) ||
            (sessionp->session_misc_flags & SMBV_MNT_HIGH_FIDELITY)) {
			SMB_LOG_ACCESS_LOCK(np, "%d not authorized to access %s : action = 0x%x\n",
								kauth_cred_getuid(cred), np->n_name, action);
			error = EACCES;
			goto done;
		}
	}
	
	/*
	 * If KAUTH_VNODE_ACCESS is not set then this is an authoritative request,
	 * we can't answer those correctly so always grant access. Now if they are
	 * asking about excute we should do some extra checking. If we have excute or
	 * read access then grant it otherwise fail the access request.
	 */
	if (((action & KAUTH_VNODE_ACCESS) != KAUTH_VNODE_ACCESS) && 
		(((action & KAUTH_VNODE_EXECUTE) != KAUTH_VNODE_EXECUTE) || 
		 (!vnode_isreg(vp)))) {
			goto done;
	}
	
	/* Deal with the immutable bit, never allow write, delete or security changes. */
	write_rights = KAUTH_VNODE_WRITE_RIGHTS;
	/* Allow them to change the immutable, if they own it, we always allow */
	write_rights &= ~(KAUTH_VNODE_WRITE_ATTRIBUTES | KAUTH_VNODE_WRITE_EXTATTRIBUTES);
	/* Remove this one, we allow access if its set */
	write_rights &= ~KAUTH_VNODE_CHECKIMMUTABLE;
	if (node_isimmutable(share, vp, NULL) && (action & write_rights)) {
		SMB_LOG_ACCESS_LOCK(np, "%s action = 0x%x %s denied\n",
                            np->n_name, action,
                            vnode_isdir(vp) ? "IMMUTABLE_DIR" : "IMMUTABLE_FILE");
		error = EPERM;
		goto done;
	}
	
	/*
	 * Windows FAT file systems have no access check, so always grant access and
	 * let the server make the final call. We could have some strange server that
	 * supports ACLs on a FAT file system or has some kind of access model. See
	 * FAT on a UNIX/Mac. In that case lets see if they are returning the correct
	 * maximal access.
	 */
	if ((share->ss_fstype == SMB_FS_FAT) && 
		((share->ss_attributes & FILE_PERSISTENT_ACLS) != FILE_PERSISTENT_ACLS)) {
		SMB_LOG_ACCESS("FAT: Access call not supported by server\n");
		goto done;
	}
	/* 
	 * We were mounted with guest access.
	 *
	 * 1. We turn off ACLs if mounted as guest. Without ACLs we can't determine 
	 *    maximal access with Samba that includes our version. So in the samba 
	 *	  case we always ask the server for the check.
	 * 2. Windows return us the correct maximal access, so we can return the
	 *    the correct value in that case.
	 *
	 */
#ifdef SMB_DEBUG_ACCESS
	if (SMBV_HAS_GUEST_ACCESS(sessionp)) {
		SMB_LOG_ACCESS_LOCK(np, "SMBV_GUEST_ACCESS: %s action = 0x%x\n",
                            np->n_name, action);
	}
    else {
		SMB_LOG_ACCESS_LOCK(np, "%s action = 0x%x\n",
                            np->n_name, action);
	}
#endif // SMB_DEBUG_ACCESS
	
	/*
	 * They are asking about a stream, how do we want to handle stream
	 * nodes. Windows will return this in the open, but we would need to change
	 * the open call to support getting it on stream.Streams have the same access
	 * as the parent (data stream). So lets get the parent and return whatever
	 * the parent supports.
	 */
	if (vnode_isnamedstream(vp)) {
		vnode_t parent_vp = vnode_getparent(vp);
		if (!parent_vp) {
			/* Can't get the parent, let the server make the call */
			SMBERROR("Cant get the parent of %s \n", np->n_name);
			error = 0;
			goto done;
		}
		maxAccessRights = smbfs_get_maximum_access(share, parent_vp, context);
		vnode_put(parent_vp);	
	} else {
		maxAccessRights = smbfs_get_maximum_access(share, vp, context);
	}
	
	/* KAUTH_VNODE_READ_DATA for files and KAUTH_VNODE_LIST_DIRECTORY for directories */
	if ((action & KAUTH_VNODE_READ_DATA) && 
		((maxAccessRights & SMB2_FILE_READ_DATA) != SMB2_FILE_READ_DATA)) {
		SMB_LOG_ACCESS_LOCK(np, "%s action = 0x%x %s denied\n",
                            np->n_name, action,
                            vnode_isdir(vp) ? "KAUTH_VNODE_LIST_DIRECTORY" : "KAUTH_VNODE_READ_DATA");
		error = EACCES;
		goto done;
	}
    
	/* KAUTH_VNODE_WRITE_DATA for files and KAUTH_VNODE_ADD_FILE for directories */
	if ((action & KAUTH_VNODE_WRITE_DATA) && 
		((maxAccessRights & SMB2_FILE_WRITE_DATA) != SMB2_FILE_WRITE_DATA)) {
		SMB_LOG_ACCESS_LOCK(np, "%s action = 0x%x %s denied\n",
                            np->n_name, action,
                            vnode_isdir(vp) ? "KAUTH_VNODE_ADD_FILE" : "KAUTH_VNODE_WRITE_DATA");
		error = EACCES;
		goto done;
	}

	/* KAUTH_VNODE_EXECUTE for files and KAUTH_VNODE_SEARCH for directories */
	if (action & KAUTH_VNODE_EXECUTE) {
		if ((vnode_isdir(vp)) &&
			((maxAccessRights & SMB2_FILE_TRAVERSE) != SMB2_FILE_TRAVERSE) &&
			((maxAccessRights & SMB2_FILE_LIST_DIRECTORY) != SMB2_FILE_LIST_DIRECTORY)) {
			/*
			 * See <rdar://problem/11151288> for more details, we use to require
			 * SMB2_FILE_TRAVERSE for granting directory search access, but now
			 * we also accept SMB2_FILE_LIST_DIRECTORY as well.
			 */
			SMB_LOG_ACCESS_LOCK(np, "%s action = 0x%x KAUTH_VNODE_SEARCH denied\n",
                                np->n_name, action);
			error = EACCES;
			goto done;
		} else if (!vnode_isdir(vp) && 
				   ((maxAccessRights & SMB2_FILE_EXECUTE) != SMB2_FILE_EXECUTE)) {
			/* 
			 * <31261611> If the server says its Unix based, then assume the
			 * max access execute bit is accurate
			 */
			if (!(SESSION_CAPS(sessionp) & SMB_CAP_UNIX)) {
				/*
				 * If this authoritative request and they have execute or read access
				 * then grant the access.
				 */
				if (((action & KAUTH_VNODE_ACCESS) != KAUTH_VNODE_ACCESS) &&
					((maxAccessRights & SMB2_FILE_READ_DATA) == SMB2_FILE_READ_DATA)) {
					goto done;
				}
			}
			
			/* 
			 * See <rdar://problem/7327306> for more details, but we use to say
			 * if the file had read access let them have execute access. Not sure
			 * why I did that and it broke  <rdar://problem/7327306> so removing
			 * that check.
			 */
			SMB_LOG_ACCESS_LOCK(np, "%s action = 0x%x SMB2_FILE_EXECUTE denied\n",
                                np->n_name, action);
			error = EACCES;
			goto done;
		}
	}
    
	if ((action & KAUTH_VNODE_DELETE) &&  
		((maxAccessRights & SMB2_DELETE) != SMB2_DELETE)) {
		SMB_LOG_ACCESS_LOCK(np, "%s action = 0x%x KAUTH_VNODE_DELETE denied\n",
                            np->n_name, action);
		error = EACCES;
		goto done;
	}
    
	/* KAUTH_VNODE_APPEND_DATA for files and KAUTH_VNODE_ADD_SUBDIRECTORY for directories */
	if ((action & KAUTH_VNODE_APPEND_DATA) && 
		((maxAccessRights & SMB2_FILE_APPEND_DATA) != SMB2_FILE_APPEND_DATA)) {
		SMB_LOG_ACCESS_LOCK(np, "%s action = 0x%x %s denied\n",
                            np->n_name, action,
                            vnode_isdir(vp) ? "KAUTH_VNODE_ADD_SUBDIRECTORY" : "KAUTH_VNODE_APPEND_DATA");
		error = EACCES;
		goto done;
	}
    
#ifdef SMB_DEBUG_ACCESS
	/* Need to look at this some more, seems Apple and MS don't argree on this SMB2_FILE_DELETE_CHILD */
	if ((action & KAUTH_VNODE_DELETE_CHILD) && 
		((maxAccessRights & SMB2_FILE_DELETE_CHILD) != SMB2_FILE_DELETE_CHILD)) {
		SMB_LOG_ACCESS_LOCK(np, "%s action = 0x%x 0x%x KAUTH_VNODE_DELETE_CHILD should denied\n",
                            np->n_name, action, KAUTH_VNODE_DELETE_CHILD);
	}
	/* 
	 * Need to look at this some more, seems Apple and MS don't argree on what 
	 * KAUTH_VNODE_READ_ATTRIBUTES allows and doesn't allow. Window still 
	 * allow us to get some meta data?
	 */
	if ((action & KAUTH_VNODE_READ_ATTRIBUTES) && 
		((maxAccessRights & SMB2_FILE_READ_ATTRIBUTES) != SMB2_FILE_READ_ATTRIBUTES)) {
		SMB_LOG_ACCESS_LOCK(np, "%s action = 0x%x 0x%x KAUTH_VNODE_READ_ATTRIBUTES should denied\n",
                            np->n_name, action, KAUTH_VNODE_READ_ATTRIBUTES);
	}
#endif // SMB_DEBUG_ACCESS
    
	if ((action & KAUTH_VNODE_WRITE_ATTRIBUTES) && 
		((maxAccessRights & SMB2_FILE_WRITE_ATTRIBUTES) != SMB2_FILE_WRITE_ATTRIBUTES)) {
		SMB_LOG_ACCESS_LOCK(np, "%s action = 0x%x KAUTH_VNODE_WRITE_ATTRIBUTES denied\n",
                            np->n_name, action);
		error = EACCES;
		goto done;
	}
    
	if ((action & KAUTH_VNODE_READ_EXTATTRIBUTES) && 
		((maxAccessRights & SMB2_FILE_READ_ATTRIBUTES) != SMB2_FILE_READ_ATTRIBUTES)) {
		SMB_LOG_ACCESS_LOCK(np, "%s action = 0x%x KAUTH_VNODE_READ_EXTATTRIBUTES denied\n",
                            np->n_name, action);
		error = EACCES;
		goto done;
	}
    
	if ((action & KAUTH_VNODE_WRITE_EXTATTRIBUTES) &&
		((maxAccessRights & SMB2_FILE_WRITE_ATTRIBUTES) != SMB2_FILE_WRITE_ATTRIBUTES)) {
		SMB_LOG_ACCESS_LOCK(np, "%s action = 0x%x KAUTH_VNODE_WRITE_EXTATTRIBUTES denied\n",
                            np->n_name, action);
		error = EACCES;
		goto done;
	}
    
	if ((action & KAUTH_VNODE_READ_SECURITY) && 
		((maxAccessRights & SMB2_READ_CONTROL) != SMB2_READ_CONTROL)) {
		SMB_LOG_ACCESS_LOCK(np, "%s action = 0x%x KAUTH_VNODE_READ_SECURITY denied\n",
                            np->n_name, action);
		error = EACCES;
		goto done;
	}
    
	/* Check to see if the share acls allow access */
	if ((action & KAUTH_VNODE_WRITE_SECURITY) && 
		((share->maxAccessRights & SMB2_WRITE_DAC) != SMB2_WRITE_DAC)) {
		SMB_LOG_ACCESS_LOCK(np, "%s action = 0x%x KAUTH_VNODE_WRITE_SECURITY denied by Share ACL\n",
                            np->n_name, action);
		error = EACCES;
		goto done;
	}
    
	/* Check to see if the share acls allow access */
	if ((action & KAUTH_VNODE_TAKE_OWNERSHIP) && 
		((share->maxAccessRights & SMB2_WRITE_OWNER) != SMB2_WRITE_OWNER)) {
		SMB_LOG_ACCESS_LOCK(np, "%s action = 0x%x SHARE KAUTH_VNODE_TAKE_OWNERSHIP by Share ACL\n",
                            np->n_name, action);
		error = EACCES;
		goto done;
	}
    
	if ((action & KAUTH_VNODE_WRITE_SECURITY) && 
		((maxAccessRights & SMB2_WRITE_DAC) != SMB2_WRITE_DAC)) {
		SMB_LOG_ACCESS_LOCK(np, "%s action = 0x%x KAUTH_VNODE_WRITE_SECURITY denied\n",
                            np->n_name, action);
		error = EACCES;
		goto done;
	}
    
	if ((action & KAUTH_VNODE_TAKE_OWNERSHIP) && 
		((maxAccessRights & SMB2_WRITE_OWNER) != SMB2_WRITE_OWNER)) {
		SMB_LOG_ACCESS_LOCK(np, "%s action = 0x%x KAUTH_VNODE_TAKE_OWNERSHIP denied\n",
                            np->n_name, action);
		error = EACCES;
		goto done;
	}
    
done:	
	if (error) {
        if (np == NULL) {
            SMB_LOG_ACCESS("action = 0x%x denied\n", action);
        }
        else {
            SMB_LOG_ACCESS_LOCK(np, "%s action = 0x%x denied\n",
                                np->n_name, action);
        }
	}
	smb_share_rele(share, ap->a_context);
    
    SMB_LOG_KTRACE(SMB_DBG_ACCESS | DBG_FUNC_END, error, 0, 0, 0, 0);
	return error;
}


/*
 * smbfs_vnop_allocate - 
 *
 *	vnode_t a_vp;
 *  off_t a_length;
 *	u_int32_t a_flags;
 *  off_t *a_bytesallocated;
 *  off_t a_offset;
 *  vfs_context_t a_context;
 *
 */
static int 
smbfs_vnop_allocate(struct vnop_allocate_args *ap)
{
	vnode_t vp = ap->a_vp;
	u_int64_t length = (u_int64_t)ap->a_length;
	struct smbnode *np;
	int32_t error = 0;
	SMBFID fid = 0;
	
	*(ap->a_bytesallocated) = 0;
	
	/* Preflight checks */
	if (!vnode_isreg(vp)) {
		/* can only read regular files */
		if (vnode_isdir(vp))
			return (EISDIR);
		else
			return (EPERM);
	}
	
	if ((error = smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK))) {
		return (error);
	}

	SMB_LOG_KTRACE(SMB_DBG_ALLOCATE | DBG_FUNC_START,
                   ap->a_flags, ap->a_length, 0, 0, 0);

    np = VTOSMB(vp);
	np->n_lastvop = smbfs_vnop_allocate;
	
	if ((ap->a_flags & ALLOCATEFROMVOL) && (length < np->n_size)) {
		error = EINVAL;
		goto done;
	}
    
    if (ap->a_flags & ALLOCATEFROMPEOF) {
		if (length > (UINT64_MAX - np->n_size)) {
			error = EINVAL;
			goto done;
		}
        length += np->n_size;
    }
	
	if (FindFileRef(vp, vfs_context_proc(ap->a_context), kAccessWrite, 
						  kAnyMatch, 0, 0, NULL, &fid)) {
		/* No matches or no pid to match, so just use the generic shared fork */
		fid = np->f_fid;
	}
    
	if (fid == 0) {
		error = EBADF;
		goto done;
	}
    
    /* If nothing is changing, then we're done */
    if (!length || (np->n_size == length)) {
        length = 0;
    }
    else {
		struct smb_share *share;
		
		share = smb_get_share_with_reference(VTOSMBFS(vp));

        lck_mtx_lock(&VTOSMBFS(vp)->sm_statfslock);
        length = roundup(length, VTOSMBFS(vp)->sm_statfsbuf.f_bsize);
        lck_mtx_unlock(&VTOSMBFS(vp)->sm_statfslock);

        error = smbfs_smb_set_allocation(share, fid, length, ap->a_context);
        
        SMB_LOG_KTRACE(SMB_DBG_ALLOCATE | DBG_FUNC_NONE,
                       0xabc001, error, 0, 0, 0);
		smb_share_rele(share, ap->a_context);
	}
    
	if (!error) {
		*(ap->a_bytesallocated) = length;
	}	
	
done:
	if (error) {
		SMBWARNING_LOCK(np, "%s: length = %lld, error = %d\n",
                        np->n_name, length, error);
	}
	smbnode_unlock(VTOSMB(ap->a_vp));

	SMB_LOG_KTRACE(SMB_DBG_ALLOCATE | DBG_FUNC_END, error, 0, 0, 0, 0);
    return error;
}

static char*
mountpointname(struct mount *mp)
{
    size_t namelength = strlen(vfs_statfs(mp)->f_mntonname);
    int foundchars = 0;
    char *c;

    if (namelength == 0) {
        return (NULL);
    }
	
    /*
     * Look backwards through the name string, looking for
     * the first slash encountered (which must precede the
     * last part of the pathname).
     */
    for (c = vfs_statfs(mp)->f_mntonname + namelength - 1;
         namelength > 0; --c, --namelength) {
        if (*c != '/') {
            foundchars = 1;
        } else if (foundchars) {
            return (c + 1);
        }
    }
	
    return (vfs_statfs(mp)->f_mntonname);
}

/*
 * The calling routine must hold a reference on the share
 */
static int
smbfs_pack_vap(struct smb_share *share, vnode_t dvp,
               const char *name, size_t name_len,
               struct smbfattr *fap, struct vnode_attr *vap,
               vfs_context_t context)
{
#pragma unused(name_len)
    struct smbnode *dnp = NULL;
    struct smbmount *smp = NULL;
    uint32_t is_dir;
    uint64_t data_fork_size, data_fork_alloc;
    uint64_t rsrc_fork_size, rsrc_fork_alloc;
    uint64_t file_id;
    uid_t uid;
    gid_t gid;
    mode_t mode = 0;
    uint32_t flags = 0;
    int isroot = 0;
    uid_t cuid = 1;
    uint32_t cmn_user_rights = 0;
    char *mp_name = NULL;
    size_t mp_namelen = 0;
    struct vfsstatfs *stat;
    
    if ((!share) || (!dvp) || (!vap) || (!fap)) {
        SMBERROR("Missing share, dvp, vap, or fap \n");
        return (EINVAL);
    }

	/*
	 * Let vfs attr packing code know that we support 64 bit for va_linkid,
	 * va_fileid, and va_parentid
	 */
	vap->va_vaflags |= VA_64BITOBJIDS;

	dnp = VTOSMB(dvp);
    smp = VTOSMBFS(dvp);

    is_dir = (fap->fa_attr & SMB_EFA_DIRECTORY) ? 1 : 0;

    /* Use values from fap */
    data_fork_size = fap->fa_size;
    data_fork_alloc = fap->fa_data_alloc;
    rsrc_fork_size = fap->fa_rsrc_size;
    rsrc_fork_alloc = fap->fa_rsrc_alloc;

    /*
     * If there was a vnode np, then fap->fa_uid/fa_gid was set to
     * np->n_uid/n_gid which could have been updated by a Get ACL
     */
    uid = (uid_t) fap->fa_uid;
    gid = (gid_t) fap->fa_gid;

    if (VATTR_IS_ACTIVE(vap, va_mode) ||
        VATTR_IS_ACTIVE(vap, va_uid) ||
        VATTR_IS_ACTIVE(vap, va_gid)) {
        /* This is used later in this function... */
        cuid = kauth_cred_getuid(vfs_context_ucred(context));
        isroot = cuid == 0;

        /* Calculate uid, gid and mode from Query Dir results */
        if (is_dir) {
            flags |= SMBFS_GET_UGM_IS_DIR;
        }

        if (!(fap->fa_valid_mask & FA_UNIX_MODES_VALID)) {
            /* Must not have had a vnode to get unix mode from */
            flags |= SMBFS_GET_UGM_REMOVE_POSIX_MODES;
        }

        smb_get_uid_gid_mode(share, smp,
                             fap, flags,
                             &uid, &gid, &mode,
                             fap->fa_max_access);
    }

    if (VATTR_IS_ACTIVE(vap, va_rdev)) {
        VATTR_RETURN(vap, va_rdev, 0);
    }
    
    /*
     * The DIR_LINKCOUNT is the count of real directory hard links.
     * (i.e. its not the sum of the implied "." and ".." references
     *  typically used in stat's st_nlink field)
     */
    if (VATTR_IS_ACTIVE(vap, va_nlink)) {
        /* There ARE no hard links, at least not yet... */
        VATTR_RETURN(vap, va_nlink, 1);
    }
	
    if (VATTR_IS_ACTIVE(vap, va_total_size)) {
        VATTR_RETURN(vap, va_total_size, data_fork_size + rsrc_fork_size);
    }
    
    if (VATTR_IS_ACTIVE(vap, va_total_alloc)) {
        /* Should already be rounded up */
        VATTR_RETURN(vap, va_total_alloc, data_fork_alloc + rsrc_fork_alloc);
    }
	
    if (VATTR_IS_ACTIVE(vap, va_data_size)) {
        VATTR_RETURN(vap, va_data_size, data_fork_size);
    }

    if (VATTR_IS_ACTIVE(vap, va_data_alloc)) {
        VATTR_RETURN(vap, va_data_alloc, data_fork_alloc);
    }

    if (VATTR_IS_ACTIVE(vap, va_iosize)) {
        lck_mtx_lock(&smp->sm_statfslock);
        VATTR_RETURN(vap, va_iosize, smp->sm_statfsbuf.f_bsize);
        lck_mtx_unlock(&smp->sm_statfslock);
    }
	
    if (VATTR_IS_ACTIVE(vap, va_uid)) {
        uid_t nuid;
        
		/*
		 * <28555880> If its Guest mounted and NOT a TM mount, and not a HiFi
         * mount, then return unknown uid
		 */
        if (SMBV_HAS_GUEST_ACCESS(SS_TO_SESSION(share)) &&
			!(SS_TO_SESSION(share)->session_misc_flags & SMBV_MNT_TIME_MACHINE) &&
            !(SS_TO_SESSION(share)->session_misc_flags & SMBV_MNT_HIGH_FIDELITY)) {
            nuid = UNKNOWNUID;
        }
        else {
            /*
             * For servers that support the UNIX extensions we know the uid/gid.
             * For server that don't support ACLs then the node uid/gid will be
             * set to the mounted user's uid/gid. For all other servers we need
             * to get the ACL and translate the SID to a uid or gid. The uid/gid
             * really is for display purpose only and means nothing to us. We will
             * set the nodes ids if we get a request for the ACL, but otherwise
             * we leave them unset for performance reasons.
             */
            if (fap->fa_uid == KAUTH_UID_NONE) {
                nuid = smp->sm_args.uid;
            }
            else {
                nuid = uid;
            }
        }
        
        if (!isroot) {
            if (((unsigned int)vfs_flags(smp->sm_mp)) & MNT_UNKNOWNPERMISSIONS)
                nuid = cuid;
            else if (nuid == UNKNOWNUID)
                nuid = cuid;
        }
        
        VATTR_RETURN(vap, va_uid, nuid);
    }

    if (VATTR_IS_ACTIVE(vap, va_gid)) {
        gid_t ngid;
        
		/*
		 * <28555880> If its Guest mounted and NOT a TM mount, and not a HiFi
         * mount, then return unknown uid
		 */
        if (SMBV_HAS_GUEST_ACCESS(SS_TO_SESSION(share)) &&
			!(SS_TO_SESSION(share)->session_misc_flags & SMBV_MNT_TIME_MACHINE) &&
            !(SS_TO_SESSION(share)->session_misc_flags & SMBV_MNT_HIGH_FIDELITY)) {
            ngid = UNKNOWNGID;
        }
        else {
            /*
             * For servers that support the UNIX extensions we know the uid/gid.
             * For server that don't support ACLs then the node uid/gid will be
             * set to the mounted user's uid/gid. For all other servers we need
             * to get the ACL and translate the SID to a uid or gid. The uid/gid
             * really is for display purpose only and means nothing to us. We will
             * set the nodes ids if we get a request for the ACL, but otherwise
             * we leave them unset for performance reasons.
             */
            if (fap->fa_gid == KAUTH_GID_NONE) {
                ngid = smp->sm_args.gid;
            }
            else {
                ngid = gid;
            }
        }
        
        if (!isroot) {
            gid_t cgid = kauth_cred_getgid(vfs_context_ucred(context));
            if (((unsigned int)vfs_flags(smp->sm_mp)) & MNT_UNKNOWNPERMISSIONS)
                ngid = cgid;
            else if (ngid == UNKNOWNUID)
                ngid = cgid;
        }
        
        VATTR_RETURN(vap, va_gid, ngid);
    }
	
    if (VATTR_IS_ACTIVE(vap, va_mode)) {
        if (fap->fa_vtype == VDIR) {
            VATTR_RETURN(vap, va_mode, (S_IFDIR | mode));
        }
        
        if (fap->fa_vtype == VREG) {
            VATTR_RETURN(vap, va_mode, (S_IFREG | mode));
        }
        
        if (fap->fa_vtype == VLNK) {
            VATTR_RETURN(vap, va_mode, (S_IFLNK | mode));
        }
    }
    
    if (VATTR_IS_ACTIVE(vap, va_flags)) {
        uint32_t va_flags = 0;
        
        /*
         * The server has it marked as hidden, set the new UF_HIDDEN bit. Never
         * mark the root volume as hidden.
         */
        if ((fap->fa_attr & SMB_EFA_HIDDEN) &&
            (fap->fa_ino != smp->sm_root_ino)) {
            va_flags |= UF_HIDDEN;
        }
        
        /*
         * Remember that SMB_EFA_ARCHIVE means the items needs to be
         * archived and SF_ARCHIVED means the item has been archive.
         *
         * NOTE: Windows does not set ATTR_ARCHIVE bit for directories.
         */
        if ((fap->fa_vtype != VDIR) &&
            !(fap->fa_attr & SMB_EFA_ARCHIVE)) {
            va_flags |= SF_ARCHIVED;
        }
        
		if (node_isimmutable(share, NULL, fap)) {
            va_flags |= UF_IMMUTABLE;
        }
        
        /*
         * Is the file currently dataless?
         */
        if ((fap->fa_attr & SMB_EFA_REPARSE_POINT) &&
            (fap->fa_reparse_tag == IO_REPARSE_TAG_STORAGE_SYNC)) {
            if (fap->fa_attr & SMB_EFA_RECALL_ON_DATA_ACCESS) {
                /*
                 * If M bit is set, then its a newer server and we know for
                 * sure that reading the file will recall it. Opening the file
                 * is fine.
                 */
                va_flags |= SF_DATALESS;
            }
            else {
                /*
                 * Check for older server which might have P or O bits set
                 * If P or O bits are set, then must be an older server so we
                 * assume that just opening the file will recall it.
                 *
                 * This is separated out in case we change our mind on this
                 * behavior for older servers.
                 */
                if (fap->fa_attr & (SMB_EFA_OFFLINE | SMB_EFA_SPARSE)) {
                    va_flags |= SF_DATALESS;
                }
            }
        }

        VATTR_RETURN(vap, va_flags, va_flags);
    }
		
    /* va_acl is done in smbfs_vnop_getattrlistbulk() */

    if (VATTR_IS_ACTIVE(vap, va_create_time)) {
        VATTR_RETURN(vap, va_create_time, fap->fa_crtime);
    }
    
    if (VATTR_IS_ACTIVE(vap, va_access_time)) {
        VATTR_RETURN(vap, va_access_time, fap->fa_atime);
    }
    
    if (VATTR_IS_ACTIVE(vap, va_modify_time)) {
        VATTR_RETURN(vap, va_modify_time, fap->fa_mtime);
    }
    
    if (VATTR_IS_ACTIVE(vap, va_change_time)) {
        if (share->ss_fstype == SMB_FS_FAT) {
            /*
             * FAT does not support change time, so just return the modify time.
             * Copied from the msdos code. SMB has no backup time so skip the
             * va_backup_time.
             */
            VATTR_RETURN(vap, va_change_time, fap->fa_mtime);
        }
        else {
            VATTR_RETURN(vap, va_change_time, fap->fa_chtime);
        }
    }
	
    if (VATTR_IS_ACTIVE(vap, va_backup_time)) {
        /* Backup time not supported so return 0 */
        vap->va_backup_time.tv_sec = 0;
        vap->va_backup_time.tv_nsec = 0;
        VATTR_SET_SUPPORTED(vap, va_backup_time);
    }
    
    if (VATTR_IS_ACTIVE(vap, va_fileid)) {
        file_id = smb2fs_smb_file_id_get(smp,
                                         fap->fa_ino,
                                         name);
        VATTR_RETURN(vap, va_fileid, file_id);
    }
    
    if (VATTR_IS_ACTIVE(vap, va_linkid)) {
        file_id = smb2fs_smb_file_id_get(smp,
                                         fap->fa_ino,
                                         name);
        VATTR_RETURN(vap, va_linkid, file_id);
    }

    if (VATTR_IS_ACTIVE(vap, va_parentid)) {
        lck_rw_lock_shared(&dnp->n_name_rwlock);
        file_id = smb2fs_smb_file_id_get(smp,
                                         dnp->n_ino,
                                         dnp->n_name);
        lck_rw_unlock_shared(&dnp->n_name_rwlock);
        VATTR_RETURN(vap, va_parentid, file_id);
    }

    if (VATTR_IS_ACTIVE(vap, va_fsid)) {
        /* Copy AFP Client behavior */
        stat = vfs_statfs(smp->sm_mp);
        
        if (stat != NULL) {
            VATTR_RETURN(vap, va_fsid, stat->f_fsid.val[0]);
        }
    }
    
    if (VATTR_IS_ACTIVE(vap, va_filerev)) {
        VATTR_RETURN(vap, va_filerev, 0);
    }
    
    if (VATTR_IS_ACTIVE(vap, va_gen)) {
        VATTR_RETURN(vap, va_gen, 0);
    }

    /*
     * We currently have no way to know the va_encoding. The VFS layer fills it
     * in with kTextEncodingMacUnicode = 0x7E, so use the same value;
     */
    if (VATTR_IS_ACTIVE(vap, va_encoding)) {
        VATTR_RETURN(vap, va_encoding, 0x7E);
    }
	
    /*
     * If this is the root, let VFS find out the mount name, which may be
     * different from the real name
     */
    if (VATTR_IS_ACTIVE(vap, va_name)) {
        if (fap->fa_ino != smp->sm_root_ino) {
            strlcpy ((char*) vap->va_name, (char*) name, MAXPATHLEN);
            VATTR_SET_SUPPORTED(vap, va_name);
        }
        else {
            /*
             * A cnode's name may be incorrect for the root of a mounted
             * filesystem (it can be mounted on a different directory name
             * than the name of the volume, such as "blah-1").  So for the
             * root directory, it's best to return the last element of the
             * location where the volume's mounted.
             */
            mp_name = mountpointname(vnode_mount(dnp->n_vnode));
            if (mp_name != NULL) {
                mp_namelen = strlen(mp_name);
                
                /* Trim off any trailing slashes: */
                while ((mp_namelen > 0) && (mp_name[mp_namelen - 1] == '/')) {
                    --mp_namelen;
                }
                
                /* If there's anything left, use it instead of the vol's name */
                if (mp_namelen > 0) {
                    strlcpy ((char*) vap->va_name, (char*) mp_name, MAXPATHLEN);
                    VATTR_SET_SUPPORTED(vap, va_name);
                }
                else {
                    SMBERROR("mountpointname failed to find a name\n");
                }
            }
            else {
                SMBERROR("mountpointname failed \n");
            }
        }
    }
    
    /* va_uuuid, va_guuid is done in smbfs_vnop_getattrlistbulk() */
    
    if (VATTR_IS_ACTIVE(vap, va_nchildren)) {
        /* Apparently 0 is a fine answer to return for a filesystem */
        VATTR_RETURN(vap, va_nchildren, 0);
    }

    /* There ARE no hard links, at least not yet... */
    if (VATTR_IS_ACTIVE(vap, va_dirlinkcount)) {
        VATTR_RETURN(vap, va_dirlinkcount, 1);
    }
    
    /*
     * We have no way to get:
     * va_addedtime,
     * va_dataprotect_class, va_dataprotect_flags, va_document_id
     */

    /*
     * New fields added to the struct vnode_attr for the vnop_getattrlistbulk
     */
    if (VATTR_IS_ACTIVE(vap, va_devid)) {
        /* Copy AFP Client behavior */
        VATTR_RETURN(vap, va_devid, vfs_statfs(smp->sm_mp)->f_fsid.val[0]);
    }

    if (VATTR_IS_ACTIVE(vap, va_objtype)) {
        /*
         * Because of the Steve/Conrad Symlinks we can never be completely
         * sure that we have the correct vnode type if its a file. Since we
         * don't support Steve/Conrad Symlinks with Darwin we can always count
         * on the vtype being correct. For directories we always know the
         * correct information.
         */
        VATTR_RETURN(vap, va_objtype, fap->fa_vtype);
    }

    if (VATTR_IS_ACTIVE(vap, va_objtag)) {
        VATTR_RETURN(vap, va_objtag, VT_CIFS);
    }

    if (VATTR_IS_ACTIVE(vap, va_user_access)) {
        /*
         * The effective permissions for the current user which we derive
         * from the max access
         */
		if (!(fap->fa_valid_mask & FA_MAX_ACCESS_VALID)) {
			SMBERROR("Missing max access <%s> \n", name);
		}
		
        if (fap->fa_max_access & SMB2_FILE_READ_DATA) {
            cmn_user_rights |= R_OK;
        }
        
        if (fap->fa_max_access & SMB2_FILE_WRITE_DATA) {
            cmn_user_rights |= W_OK;
        }
        
        if (fap->fa_max_access & SMB2_FILE_EXECUTE) {
            cmn_user_rights |= X_OK;
        }
        
        VATTR_RETURN(vap, va_user_access, cmn_user_rights);
    }
    
    if (VATTR_IS_ACTIVE(vap, va_finderinfo)) {
        if ((fap->fa_fstatus & kNO_SUBSTREAMS) ||
            (fap->fa_fstatus & kNO_FINDERINFO)) {
            /* No Finder Info on item so nothing to return */
        }
        else {
			if (!(fap->fa_valid_mask & FA_FINDERINFO_VALID)) {
				SMBERROR("Missing Finder Info <%s> \n", name);
			}

            bcopy(fap->fa_finder_info, vap->va_finderinfo,
                  sizeof(u_int8_t) * 32);
            VATTR_SET_SUPPORTED(vap, va_finderinfo);
        }
    }
    
    if ((fap->fa_fstatus & kNO_SUBSTREAMS) ||
        (fap->fa_fstatus & kNO_RSRCFORK)) {
        /* No Rsrc Fork on item so nothing to return */
    }
    else {
        if (VATTR_IS_ACTIVE(vap, va_rsrc_length)) {
            VATTR_RETURN(vap, va_rsrc_length, rsrc_fork_size);
        }
        
        if (VATTR_IS_ACTIVE(vap, va_rsrc_alloc)) {
            VATTR_RETURN(vap, va_rsrc_alloc, rsrc_fork_alloc);
        }
    }

    if (VATTR_IS_ACTIVE(vap, va_fsid64)) {
        /* Copy AFP Client behavior */
        memcpy(&vap->va_fsid64, &(vfs_statfs(smp->sm_mp)->f_fsid), sizeof(vap->va_fsid64));
        VATTR_SET_SUPPORTED(vap, va_fsid64);
    }

    return (0);
}

static int
smbfs_add_dir_entry_attr(struct smb_share *share, vnode_t dvp,
                         const char *name, size_t name_len,
                         struct attrlist *alist, uint64_t a_options,
                         struct vnode_attr *vap, struct smbfattr *fap,
                         uio_t uio, ssize_t *max_fixed_len,
                         vfs_context_t context)
{
    int error = 0;
    enum vtype vnode_type = VREG;
    ssize_t fixed_len = 0, variable_len = 0, acl_len = 0;
    struct vnode *vp = NULL;
    off_t orig_resid;
    char *tname = NULL;
	uint32_t flags = 0;
	uint32_t cn_flags = 0;
	
    /*
     * Get the vnode type. vfs_setup_vattr_from_attrlist() only cares if
     * its a dir or not.
     */
    vnode_type = (fap->fa_attr & SMB_EFA_DIRECTORY) ? VDIR : VREG;
    
    /* Make sure the vap is zero'd out and set up correctly */
    tname = vap->va_name;
    bzero(vap, sizeof(struct vnode_attr));
    vap->va_name = tname;
    
    vfs_setup_vattr_from_attrlist(alist, vap, vnode_type, &fixed_len,
                                  context);

    /* Calculate max_fixed_len for termination check */
    if (fixed_len > *max_fixed_len) {
        *max_fixed_len = fixed_len;
    }
    
    if (VATTR_IS_ACTIVE(vap, va_name)) {
        /* Add name length for termination check */
        variable_len += MAXPATHLEN;
    }
    
    if ((share->ss_attributes & FILE_PERSISTENT_ACLS) &&
        (VATTR_IS_ACTIVE(vap, va_acl) ||
         VATTR_IS_ACTIVE(vap, va_guuid) ||
         VATTR_IS_ACTIVE(vap, va_uuuid))) {
            /* Calculate maxacl_len for termination check */
            acl_len = KAUTH_ACL_SIZE(KAUTH_ACL_MAX_ENTRIES);
    }
    
    /*
     * Is there enough buffer space remaining in the uio to add this?
     * Need at least 4 bytes for len, and then 4 bytes for one data element
     */
    if (uio_resid(uio) <= 8) {
        if (vp != NULL) {
            smbnode_unlock(VTOSMB(vp));
            vnode_put(vp);
        }
        error = ENOBUFS;
        goto done;
    }
    
    /*
     * Termination check.
     * Make sure the uio buffer has enough space
     * for another entry.
     */
    if (uio_resid(uio) < (*max_fixed_len + variable_len + acl_len)) {
        error = ENOBUFS;
        goto done;
    }

    if ((share->ss_attributes & FILE_PERSISTENT_ACLS) &&
        (VATTR_IS_ACTIVE(vap, va_acl) ||
         VATTR_IS_ACTIVE(vap, va_guuid) ||
         VATTR_IS_ACTIVE(vap, va_uuuid))) {
            /*
             * In order to get the ACL, we are going to need a vnode
             * created. Someday, we may be able to get the ACL without
             * a vnode, but that is a lot of code to change at this time.
             */
            error = smbfs_nget(share, vnode_mount(dvp),
							   dvp, name, name_len,
							   fap, &vp,
							   MAKEENTRY, SMBFS_NGET_CREATE_VNODE,
							   context);
            if (error) {
                if (vp != NULL) {
                    smbnode_unlock(VTOSMB(vp));
                    vnode_put(vp);
                }
                goto done;
            }
            
            /*
             * Fill in the ACL info here if needed. ACL data has to have a
             * vnode to use (at least for now).
             */
            if (vp != NULL) {
                DBG_ASSERT(!vnode_isnamedstream(vp));
                (void)smbfs_getsecurity(share, VTOSMB(vp), vap, context);
                
                /*
                 * vnode must be unlocked before calling vfs_attr_pack
                 */
                smbnode_unlock(VTOSMB(vp));
            }
        }
    
    /*
     * If its a DFS trigger that has been mounted, then to fill in the 
     * ATTR_DIR_MOUNTSTATUS, we MUST have a vnode so that smbfs_pack_vap() 
     * can fill it in correctly. If there is no vnode found, then the DFS
     * trigger has not been activated and thus its ok to not have a vnode.
     * Only need to do this for directories since only directories can be DFS
     * triggers. This will also update the smb node with the latest meta data
     * if a smb node is found.
	 *
	 * For all items, see if the vnode already exists
     */
    if (vp == NULL) {
		flags = SMBFS_NGET_LOOKUP_ONLY;
		cn_flags = 0;
		
		/* 
		 * if its a DFS reparse point, then we have to create the vnode so
		 * that vfs_attr_pack() can set the correct ATTR_DIR_MOUNTSTATUS
		 */
		if ((vnode_type == VDIR) &&
			(fap->fa_attr & SMB_EFA_REPARSE_POINT) &&
			(fap->fa_reparse_tag == IO_REPARSE_TAG_DFS)) {
			cn_flags = MAKEENTRY;
			flags = SMBFS_NGET_CREATE_VNODE;
		}
		
        if (smbfs_nget(share, vnode_mount(dvp),
                       dvp, name, name_len,
                       fap, &vp,
                       cn_flags, flags,
                       context) == 0) {
            /*
             * Found one in our hash table unlock it, we just need
             * the vnode reference at this point
			 *
			 * Enumerates alway return the correct case of the name.
			 * Update the name and parent if needed.
			 */
			smbfs_update_name_par(share, dvp, vp,
								  &fap->fa_reqtime,
								  name, name_len);
			
            smbnode_unlock(VTOSMB(vp));
        }
    }
    
    /*
     * Fill in the vap using the data from the ctx
     */
    error = smbfs_pack_vap(share, dvp, name, name_len, fap, vap, context);
    if (error == 0) {
        /* Save current resid so can tell if uio is full */
        orig_resid = uio_resid(uio);
        
        /* Copy info from vap into uio */
        error = vfs_attr_pack(vp, uio, alist, a_options, vap, NULL, context);
        if (error == 0) {
            if (orig_resid == uio_resid(uio)) {
                /* No more space in uio */
                error = ENOBUFS;
            }
        }
        else {
            SMBERROR("vfs_attr_pack() failed for %s, error %d", name, error);
        }
    }
    else {
        SMBERROR("smbfs_pack_vap() failed for %s, error %d", name, error);
    }
    
    /* Done with the vp */
    if (vp != NULL) {
        vnode_put(vp);
    }
    
done:
    return (error);
}


static int
smbfs_fetch_new_entries(struct smb_share *share, vnode_t dvp,
                        struct smb_enum_cache *cachep, off_t offset,
                        uint32_t is_overflow, vfs_context_t context)
{
    struct smbnode *dnp = NULL;
    char *name = NULL;
    size_t name_len = 0;
    struct smbfattr *fap = NULL;
    int error = 0, tmp_error = 0;
    struct smbfs_fctx *ctx = NULL;
    off_t fetch_count = kAsyncFetchCount;
    struct timespec	start, stop;
	u_quad_t reparse_point_len = 0;
	int need_refill = 0;
	off_t saved_d_offset = 0;
	int32_t dir_cache_max_cnt = g_max_dir_entries_cached;
    struct timespec waittime, now;
    int old_dir = 0;
    uint32_t cache_entries_added = 0;

    dnp = VTOSMB(dvp);

	SMB_LOG_UNIT_TEST_LOCK(dnp, "LeaseUnitTest - Filling enum cache for <%s>\n", dnp->n_name);

	SMB_LOG_DIR_CACHE_LOCK(dnp, "fetch offset %lld d_offset %lld cachep->offset %lld for <%s>\n",
                           offset, dnp->d_offset, cachep->offset, dnp->n_name);
    
    SMB_LOG_KTRACE(SMB_DBG_SMBFS_FETCH_NEW_ENTRIES | DBG_FUNC_START,
                   offset, dnp->d_offset, cachep->offset, 0, 0);

    /* Is this dir older than 10 min from now? */
    waittime.tv_sec = 60 * 10;
    waittime.tv_nsec = 0;
    timespecadd(&waittime, &dnp->n_mtime);

    nanotime(&now); /* Need current date/time, so use nanotime */
    if (timespeccmp(&now, &waittime, >)) {
        old_dir = 1;
    }

    nanotime(&start);

    /*
     * Do we need to refill main dir cache?
     */
	if ((is_overflow == 0) &&
		(offset != 0) &&
		(cachep->offset == 0)) {
		/* 
		 * if main cache is empty and current offset != 0, then main cache 
		 * must have gotten invalidated in the middle of an enumeration,
		 * so need to refill the main cache.
		 */
		SMB_LOG_DIR_CACHE_LOCK(dnp, "Main cache needs to be refilled <%s> \n",
							   dnp->n_name);
		need_refill = 1;
		saved_d_offset = dnp->d_offset;
		
		if (offset > dir_cache_max_cnt) {
            /* Refill main cache with whatever is current on the server */
			fetch_count = dir_cache_max_cnt;
		}
		else {
            if ((SS_TO_SESSION(share)->session_misc_flags & SMBV_OSX_SERVER) &&
                ((SS_TO_SESSION(share)->session_server_caps & kAAPL_SUPPORTS_READ_DIR_ATTR) ||
                 (SS_TO_SESSION(share)->session_server_caps & kAAPL_SUPPORTS_READ_DIR_ATTR_V2))) {
                /*
                 * If its a macOS server with the fast enumeration, then we can
                 * easily refill the caches
                 */
                SMB_LOG_DIR_CACHE_LOCK(dnp, "Fetch max cnt for macOS style server <%s> \n",
                                       dnp->n_name);
                fetch_count = dir_cache_max_cnt;
            }
            else {
                /* Windows style server */
                if (old_dir) {
                    /*
                     * Assume dir is not going to change anytime soon,
                     * so cache more.
                     */
                    SMB_LOG_DIR_CACHE_LOCK(dnp, "Dir has not been modified recently <%s> \n",
                                           dnp->n_name);
                    fetch_count = offset + (2 * kAsyncFetchCount);
                }
                else {
                    /*
                     * Assume dir is still changing so dont cache too much
                     */
                    fetch_count = offset + kAsyncFetchCount;
                }
            }
		}
	}
    else {
        /*
         * Filling the overflow cache
         */
        if ((SS_TO_SESSION(share)->session_misc_flags & SMBV_OSX_SERVER) &&
            ((SS_TO_SESSION(share)->session_server_caps & kAAPL_SUPPORTS_READ_DIR_ATTR) ||
             (SS_TO_SESSION(share)->session_server_caps & kAAPL_SUPPORTS_READ_DIR_ATTR_V2))) {
            /*
             * If its a macOS server with the fast enumeration, then we can
             * easily refill the dir caches
             */
            SMB_LOG_DIR_CACHE_LOCK(dnp, "Fetch max cnt for macOS style server <%s> \n",
                                   dnp->n_name);
            fetch_count = dir_cache_max_cnt;
        }
        else {
            /* Windows style server */
            if (old_dir) {
                /*
                 * Assume dir is not going to change anytime soon,
                 * so cache more.
                 */
                SMB_LOG_DIR_CACHE_LOCK(dnp, "Dir has not been modified recently <%s> \n",
                                       dnp->n_name);
                fetch_count = 2 * kAsyncFetchCount;
            }
            else {
                /*
                 * Assume dir is still changing so dont cache too much
                 */
                fetch_count = kAsyncFetchCount;
            }
        }
    }

	/*
	 * Do we need to open or restart the enumeration?
	 */
	if (!(dnp->d_fctx) ||
		(dnp->d_fctx->f_share != share) ||
		(offset == 0) ||
		(offset != dnp->d_offset) ||
		(need_refill == 1)) {
        SMB_LOG_DIR_CACHE_LOCK(dnp, "Restart enum offset %lld d_offset %lld for <%s> \n",
                               offset, dnp->d_offset, dnp->n_name);
        
        smbfs_closedirlookup(dnp, 0, "fetch_new_entries restart", context); /* sets dnp->d_offset to 0 */
        
        if (offset == 0) {
            dnp->d_offset = 0;
        }
        
        error = smbfs_smb_findopen(share, dnp, "*", 1, &dnp->d_fctx, TRUE,
                                   0, context);
		
		if (is_overflow == 0) {
            /* 
             * Remove the overflow cache if it exists, but only if we are
             * filling the main cache
             */
            smb_dir_cache_remove(dvp, &dnp->d_overflow_cache, "overflow", "fetch_new_entries restart", 1);
        }
    }
    
    if (error) {
        goto done;
    }
    
    ctx = dnp->d_fctx;
    
	if (is_overflow == 1) {
        /*
         * Do we need to skip entries until we get to the starting offset for
         * the overflow cache?
         */
        while (cachep->offset < offset) {
            error = smbfs_findnext(ctx, context);
            if (error) {
                smbfs_closedirlookup(dnp, 0, "fetch_new_entries findnext error", context);
                goto done;
            }

            cachep->offset++;
        }
	}
	
    /*
     * Before we can return any entries, we need to have all the meta data
     * already retrieved. So, get kAsyncFetchCount entries at a time and then 
	 * get all the meta data for those entries and cache all that information.
     */
    SMB_LOG_KTRACE(SMB_DBG_SMBFS_FETCH_NEW_ENTRIES | DBG_FUNC_NONE,
                   fetch_count, 0, 0, 0, 0);

    while (fetch_count > 0) {
        error = smbfs_findnext(ctx, context);
        if (error) {
            if (error == ENOENT) {
                /* Done enumerating dir */
                if (!(cachep->flags & kDirCachePartial)) {
                    /* Only set if not at max entries in dir cache */
                    SMB_LOG_DIR_CACHE_LOCK(dnp, "Set cache complete in <%s> \n",
                                           dnp->n_name);
                    cachep->flags |= kDirCacheComplete;
                }
				
				lck_mtx_lock(&dnp->d_dur_handle.lock);
				if (!(dnp->d_dur_handle.flags & SMB2_LEASE_GRANTED)) {
					lck_mtx_unlock(&dnp->d_dur_handle.lock);
					
					/* We have a dir lease, so dont need to close the dir */
					SMB_LOG_DIR_CACHE_LOCK(dnp, "Done enumerating dir for <%s> \n",
										   dnp->n_name);
					smbfs_closedirlookup(dnp, 0, "fetch_new_entries EOF", context);
				}
				else {
					lck_mtx_unlock(&dnp->d_dur_handle.lock);
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
        if ((SS_TO_SESSION(share)->session_misc_flags & SMBV_HAS_FILEIDS) &&
            (fap->fa_ino == dnp->n_ino)) {
            SMBDEBUG("Skipping <%s> as it has same ID as parent\n", name);
        }
        else {
            /*SMB_LOG_DIR_CACHE2_LOCK(dnp, "Fetch return <%s> in <%s> \n",
                                    name, dnp->n_name);*/
            fetch_count -= 1;
            
			/*
			 * If its a reparse point, the data size reported from the server
			 * is zero, but OS X would rather have the size of the target string
			 */
			if ((fap->fa_attr & SMB_EFA_REPARSE_POINT) &&
				(fap->fa_reparse_tag == IO_REPARSE_TAG_SYMLINK)) {
				tmp_error = smb2fs_smb_cmpd_reparse_point_get(share, dnp,
															  name, name_len,
															  NULL, &reparse_point_len,
															  context);
				if (tmp_error == 0) {
					fap->fa_size = reparse_point_len;
					fap->fa_data_alloc = roundup(reparse_point_len,
												 dnp->n_mount->sm_statfsbuf.f_bsize);
				}
				else {
					SMBERROR("Failed to get reparse point size %d for <%s> \n",
							 tmp_error, name);
				}
			}

			smb_dir_cache_add_entry(dvp, cachep,
                                    name, name_len, fap,
                                    is_overflow, 1);

			/* Did we hit the max number of entries? */
            if (cachep->flags & kDirCachePartial) {
                break; /* yep, return what we have */
            }

            cache_entries_added += 1;
        }
    }
    
    /* Get the meta data for this set of entries */
    if (cachep->flags & kDirCacheDirty) {
        smb_dir_cache_get_attrs(share, dvp, cachep, 1, context);
        cachep->flags &= ~kDirCacheDirty;
    }
    
done:
    nanotime(&stop);
    SMB_LOG_DIR_CACHE_LOCK(dnp, "elapsed time %ld:%ld for <%s>\n",
                           stop.tv_sec - start.tv_sec,
                           stop.tv_nsec - start.tv_nsec,
                           dnp->n_name);

	if (need_refill == 1) {
		/* Restore dnp->d_offset */
		dnp->d_offset = saved_d_offset;
	}

    SMB_LOG_KTRACE(SMB_DBG_SMBFS_FETCH_NEW_ENTRIES | DBG_FUNC_END,
                   error, cache_entries_added, 0, 0, 0);

    return (error);
}

static int
smbfs_new_cookie(struct vnode *dvp, uint64_t *keyp)
{
    int error = ENOENT;
    int i;
    struct smbnode *dnp = NULL;
    struct timespec ts = {0};
    int oldest = -1;

    /* d_enum_cache_list_lock MUST be already held */

    if ((dvp == NULL) ||
        (keyp == NULL))  {
        SMBERROR("Invalid parameters \n");
        return(EINVAL);
    }
    
    dnp = VTOSMB(dvp);
    
    lck_mtx_lock(&dnp->d_cookie_lock);
    
    /* Find an entry which has 0 for the time (unused) or the oldest */
    for (i = 0; i < kSMBDirCookieMaxCnt; i++) {
        if ((dnp->d_cookies[i].last_used.tv_sec == 0) &&
            (dnp->d_cookies[i].last_used.tv_nsec == 0)) {
            oldest = i;
            break;
        }
        else {
            /* Find first entry with non zero time and set that as oldest */
            if ((ts.tv_sec == 0) && (ts.tv_nsec == 0)) {
                oldest = i;
                ts = dnp->d_cookies[i].last_used;
            }
        }
        
        /*
         * Is this entry the oldest?
         */
       if (timespeccmp(&ts, &dnp->d_cookies[i].last_used, >)) {
            ts = dnp->d_cookies[i].last_used;
            oldest = i;
       }
    }

    if (oldest != -1) {
        dnp->d_cookie_cur_key += 1;
        
        /*
         * d_cookie_cur_key can never be 0 as an offset/key of 0 means
         * to start the enumeration from the beginning
         */
        if (dnp->d_cookie_cur_key == 0) {
            /* Must have wrapped around */
            dnp->d_cookie_cur_key += 1;
        }
        
        dnp->d_cookies[oldest].key = dnp->d_cookie_cur_key;
        nanotime(&dnp->d_cookies[oldest].last_used);
        dnp->d_cookies[oldest].resume_offset = 0;
        dnp->d_cookies[oldest].resume_node_id = 0;

        SMB_LOG_DIR_CACHE_LOCK(dnp, "Using [%d] key %lld for <%s> \n",
                               oldest, dnp->d_cookie_cur_key, dnp->n_name);

        *keyp = dnp->d_cookies[oldest].key;
        error = 0;
    }
    
    lck_mtx_unlock(&dnp->d_cookie_lock);
    
    if (error == ENOENT) {
        /* Should be impossible */
        SMBERROR_LOCK(dnp, "Could not create new cookie for <%s> \n",
                      dnp->n_name);
    }
    
    return(error);
}

static int
smbfs_update_cookie(struct vnode *dvp, uint64_t key, off_t resume_offset,
                    struct cached_dir_entry *resume_cachep)
{
    int error = ENOENT;
    int i;
    struct smbnode *dnp = NULL;
    
    /* d_enum_cache_list_lock MUST be already held */

    if ((dvp == NULL) ||
        (key == 0))  {
        SMBERROR("Invalid parameters \n");
        return(EINVAL);
    }
    
    dnp = VTOSMB(dvp);
    
    lck_mtx_lock(&dnp->d_cookie_lock);
    
    for (i = 0; i < kSMBDirCookieMaxCnt; i++) {
        if (key == dnp->d_cookies[i].key) {
            dnp->d_cookies[i].resume_offset = resume_offset;

            if (resume_cachep == NULL) {
                /* At EOF for this dir */
                SMB_LOG_DIR_CACHE_LOCK(dnp, "Key %lld, offset %lld at EOF, nodeID 0x0 name <NULL> for <%s>\n",
                                       key, resume_offset, dnp->n_name);
                
                dnp->d_cookies[i].resume_node_id = 0;
                bzero(dnp->d_cookies[i].resume_name,
                      sizeof(dnp->d_cookies[i].resume_name));
            }
            else {
                SMB_LOG_DIR_CACHE_LOCK(dnp, "Key %lld, offset %lld, nodeID 0x%llx name <%s> for <%s> \n",
                                       key, resume_offset,
                                       resume_cachep->fattr.fa_ino,
                                       resume_cachep->name,
                                       dnp->n_name);

                dnp->d_cookies[i].resume_node_id = resume_cachep->fattr.fa_ino;
                strncpy(dnp->d_cookies[i].resume_name, resume_cachep->name,
                        sizeof(dnp->d_cookies[i].resume_name));
            }
            
            nanotime(&dnp->d_cookies[i].last_used);
            error = 0;
            break;
        }
    }
    
    lck_mtx_unlock(&dnp->d_cookie_lock);
    
    if (error == ENOENT) {
        SMBERROR_LOCK(dnp, "No cookie found for key %lld for <%s> \n",
                      key, dnp->n_name);
    }
    
    return(error);
}

static int
smbfs_find_cookie(struct vnode *dvp, uint64_t key,
                  struct smb_dir_cookie **resume_cookiepp)
{
    int error = ENOENT;
    int i;
    struct smbnode *dnp = NULL;
    
    /* d_enum_cache_list_lock MUST be already held */

    if ((dvp == NULL) ||
        (key == 0) ||
        (resume_cookiepp == NULL))  {
        SMBERROR("Invalid parameters \n");
        return(EINVAL);
    }
    
    dnp = VTOSMB(dvp);

    lck_mtx_lock(&dnp->d_cookie_lock);
    
    for (i = 0; i < kSMBDirCookieMaxCnt; i++) {
        if (key == dnp->d_cookies[i].key) {
            SMB_LOG_DIR_CACHE_LOCK(dnp, "Key %lld, offset %lld, nodeID 0x%llx name <%s> for <%s>\n",
                                   key, dnp->d_cookies[i].resume_offset,
                                   dnp->d_cookies[i].resume_node_id,
                                   dnp->d_cookies[i].resume_name,
                                   dnp->n_name);
            
            *resume_cookiepp = &dnp->d_cookies[i];

            nanotime(&dnp->d_cookies[i].last_used);
            error = 0;
            break;
        }
    }
    
    lck_mtx_unlock(&dnp->d_cookie_lock);

    if (error == ENOENT) {
        SMBERROR_LOCK(dnp, "No cookie found for key %lld for <%s> \n",
                      key, dnp->n_name);
    }
    
    return(error);
}

static int
smbfs_entries_match(struct smb_session *sessionp, struct vnode *dvp,
                    struct cached_dir_entry *enum_cache_currp,
                    struct smb_dir_cookie *resume_cookiep)
{
#pragma unused(dvp)
    int is_match = 0;
    size_t resume_name_len = 0;

    /* d_enum_cache_list_lock MUST be already held */
    
    if ((sessionp == NULL) ||
        (dvp == NULL) ||
        (resume_cookiep == NULL))  {
        SMBERROR("Invalid parameters \n");
        return(EINVAL);
    }

    if (sessionp->session_misc_flags & SMBV_HAS_FILEIDS) {
        /* Match using node ID */
        if (enum_cache_currp == NULL) {
            if (resume_cookiep->resume_node_id == 0) {
                /* Matched on EOF */
                is_match = 1;
            }
        }
        else {
            if (resume_cookiep->resume_node_id == enum_cache_currp->fattr.fa_ino) {
                is_match = 1;
            }
        }
    }
    else {
        /* Match using name */
        resume_name_len = strnlen(resume_cookiep->resume_name,
                                  sizeof(resume_cookiep->resume_name));
        
        if (enum_cache_currp == NULL) {
            if (resume_name_len == 0) {
                /* Matched on EOF */
                is_match = 1;
            }
        }
        else {
            if ((resume_name_len == enum_cache_currp->name_len) &&
                strncmp(resume_cookiep->resume_name, enum_cache_currp->name, resume_name_len) == 0) {
                is_match = 1;
            }
        }
    }
    
    return(is_match);
}

static int
smbfs_search_main_cache(struct smb_session *sessionp, struct vnode *dvp,
                        struct smb_dir_cookie *resume_cookiep,
                        off_t *return_offsetp)
{
    struct smbnode *dnp = NULL;
    struct cached_dir_entry *enum_cache_currp = NULL;
    int error = ENOENT, is_match;
    off_t curr_offset = 0;

    /* d_enum_cache_list_lock MUST be already held */

    if ((sessionp == NULL) ||
        (dvp == NULL) ||
        (resume_cookiep == NULL) ||
        (return_offsetp == NULL))  {
        SMBERROR("Invalid parameters \n");
        return(EINVAL);
    }

    dnp = VTOSMB(dvp);
    
    enum_cache_currp = dnp->d_main_cache.list;

    while (enum_cache_currp != NULL) {
        is_match = smbfs_entries_match(sessionp, dvp,
                                       enum_cache_currp,
                                       resume_cookiep);

        if (is_match == 1) {
            *return_offsetp = curr_offset;
            return(0);
        }
        
        curr_offset += 1;
        enum_cache_currp = enum_cache_currp->next;
    }
    
    return(error);
}

static int
smbfs_verify_resume(struct smb_session *sessionp, struct vnode *dvp,
                    struct cached_dir_entry *enum_cache_currp,
                    struct smb_dir_cookie *resume_cookiep,
                    off_t *return_offsetp)
{
    int error = 0;
    int is_match = 0;
    struct smbnode *dnp = NULL;
    off_t resume_offset = 0;

    if ((sessionp == NULL) ||
        (dvp == NULL) ||
        (resume_cookiep == NULL) ||
        (return_offsetp == NULL))  {
        SMBERROR("Invalid parameters \n");
        return(EINVAL);
    }

    dnp = VTOSMB(dvp);

    is_match = smbfs_entries_match(sessionp, dvp,
                                   enum_cache_currp, resume_cookiep);
    if (is_match == 1) {
        SMB_LOG_DIR_CACHE_LOCK(dnp, "Verified for <%s> \n", dnp->n_name);
        return(0);
    }
    
    SMB_LOG_DIR_CACHE_LOCK(dnp, "Failed nodeID 0x%llx <%s> != 0x%llx <%s> for <%s> \n",
                           enum_cache_currp != NULL ? enum_cache_currp->fattr.fa_ino : 0,
                           enum_cache_currp != NULL ? enum_cache_currp->name : "Null",
                           resume_cookiep->resume_node_id,
                           resume_cookiep->resume_name,
                           dnp->n_name);
    
    /* Search main cache for item to resume with */
    error = smbfs_search_main_cache(sessionp, dvp, resume_cookiep,
                                    &resume_offset);
    if (error) {
        /* Cant find resume offset, so reset to beginning */
        SMB_LOG_DIR_CACHE_LOCK(dnp, "No match, restart enumeration for <%s> \n",
                               dnp->n_name);
        *return_offsetp = 0;
    }
    else {
        SMB_LOG_DIR_CACHE_LOCK(dnp, "Found match, resume offset %lld for <%s> \n",
                               resume_offset, dnp->n_name);
        *return_offsetp = resume_offset;
    }

    error = ESTALE;
    
    return(error);
}


static int
smbfs_vnop_getattrlistbulk(struct vnop_getattrlistbulk_args *ap)
/* struct vnop_getattrlistbulk_args {
 struct vnodeop_desc *a_desc;
 vnode_t a_vp;
 struct attrlist *a_alist;
 struct vnode_attr *a_vap;
 struct uio *a_uio;
 void *a_private;
 uint64_t a_options;
 int32_t *a_eofflag;
 int32_t *a_actualcount;
 vfs_context_t a_context;
 } *ap; */
{
    struct vnode *dvp = ap->a_vp;
    uio_t uio = ap->a_uio;
    struct vnode_attr *vap = ap->a_vap;
    vfs_context_t context = ap->a_context;
    struct smbnode *dnp = NULL;
    ssize_t max_fixed_len = 0;
    off_t offset;
    int error = 0, overflow_error, tmp_error;
    struct smb_share *share = NULL;
   	struct smb_session *sessionp = NULL;
    int i;
    struct cached_dir_entry *enum_cache_currp = NULL;
    off_t skip_count = 0;
    int32_t add_remaining = 0;
    struct timespec	start, stop;
	struct smbmount *smp = NULL;
    int first = 0;
    char *last_entry_namep = NULL;
    uint64_t key = 0;
    struct cached_dir_entry *resume_cachep = NULL;
    struct smb_dir_cookie *resume_cookiep = NULL;
    off_t resume_offset = 0;
    int check_resume = 0;

    /*
     * For smbfs_vnop_getattrlistbulk, we need to have all the meta data 
     * fetched BEFORE we can pack and put the entry into the users buffer.
     * In order to do this, the enumerations are done first and the results
     * are stored in the dir cache. Then the meta data is fetched for each
     * entry in the dir cache. Then, we use the dir cache to start filling in
     * the user's buffer.
     *
     * Once the max number of entries allowed in the dir cache is reached,
     * from then on, we use the overflow cache. The overflow cache is smaller
     * and only kept briefly.
     *
     * The main cache holds g_max_dir_entries_cached entries and always caches
     * from the beginning (offset 0). The overflow cache will cache a smaller
     * range of entries and has a variable start_offset and offset to indicate
     * the current cached range of entries. If the current enumeration is past
     * the end of the main cache, but within the overflow cache, then the
     * overflow cache used to return the entries. If the current enumeration 
     * does not fall within the current overflow cache, then the dir is closed
     * and the enumeration is started from the beginnning. Entries are skipped
     * until the desired offset is reached, then that offset is saved as the
     * starting offset for the overflow cache and the overflow cache is filled
     * with the next kAsyncFetchCount number of entries.
     *
     * The actual offset stored in uio_offset is now a key value used to find
     * a dir cookie that contains the information needed to resume an
     * enumeration. This info is also used to validate where the dir has
     * changed while we were in the middle of enumerating it.
     */
    
    /*
     * offset - offset of item to start at
     * d_offset - offset of next item to fetch from server
     * dnp->d_main_cache.offset - offset of next item to be placed into dir cache
     */

    /* Check for invalid buffer space. */
    if ((uio_resid(uio) <= 0) || (uio_iovcnt(uio) > 1)) {
        SMBDEBUG("Invalid buf size\n");
        return (EINVAL);
    }
    
    /*
     * Lock parent dir that we are enumerating
     */
    if ((error = smbnode_lock(VTOSMB(dvp), SMBFS_EXCLUSIVE_LOCK))) {
        return (error);
    }

    SMB_MALLOC(last_entry_namep,
               char *,
               PATH_MAX,
               M_SMBTEMP,
               M_WAITOK | M_ZERO);

    nanotime(&start);

    VTOSMB(dvp)->n_lastvop = smbfs_vnop_getattrlistbulk;
    
    dnp = VTOSMB(dvp);
    
    SMB_LOG_KTRACE(SMB_DBG_GET_ATTRLIST_BULK | DBG_FUNC_START, dnp->d_fid, 0, 0, 0, 0);
    
    /* Get Share reference */
    share = smb_get_share_with_reference(VTOSMBFS(dvp));
   	sessionp = SS_TO_SESSION(share);
    
    /*
     * Non FAT Filesystem, named streams, and SMB 2/3 are required.
     * smbfs_vnop_getattrlistbulk will always use dir caching.
     */
    if (!(sessionp->session_flags & SMBV_SMB2) ||
        (share->ss_fstype == SMB_FS_FAT) ||
        !(share->ss_attributes & FILE_NAMED_STREAMS)) {
        SMB_LOG_DIR_CACHE("FAT or no named streams so smbfs_vnop_getattrlistbulk not supported \n");
        error = ENOTSUP;
        goto err_exit;
    }
    
    /* See if dir caching is disabled or not */
    smp = VFSTOSMBFS(vnode_mount(dvp));
    if (smp != NULL) {
        if (smp->sm_args.altflags & SMBFS_MNT_DIR_CACHE_OFF) {
            SMB_LOG_DIR_CACHE("Dir Enumeration cache turned off \n");
            error = ENOTSUP;
            goto err_exit;
        }
    }
    
    /* Offset is the key to find the dir cookie */
    offset = uio_offset(uio);

    if (uio_offset(uio) != 0) {
        SMB_LOG_DIR_CACHE_LOCK(dnp, "Resuming enumeration for <%s>\n", dnp->n_name);
        
        /*
         * Resuming enumeration, find existing cookie and set offset to the
         * resume offset
         */
        key = uio_offset(uio);
        error = smbfs_find_cookie(dvp, key, &resume_cookiep);
        if (error) {
            /*
             * Should be impossible. If it does happen, set uio_offset to
             * be 0 and fall through to start enumeration over from
             * beginning.
             */
            uio_setoffset(uio, 0);
        }
        else {
            offset = resume_cookiep->resume_offset;
            check_resume = 1;   /* Need to verify resume entry */
        }
    }
    
    if (uio_offset(uio) == 0) {
        SMB_LOG_DIR_CACHE_LOCK(dnp, "New enumeration for <%s>\n", dnp->n_name);

        /* Starting a new enumeration, get a new cookie to use */
        error = smbfs_new_cookie(dvp, &key);
        if (error) {
            /* Should be impossible, let vfs handle this enumerate */
            error = ENOTSUP;
            goto err_exit;
        }
    }
    
    SMB_LOG_DIR_CACHE_LOCK(dnp, "offset %lld d_offset %lld d_main_cache.offset %lld for <%s>\n",
                           offset, dnp->d_offset, dnp->d_main_cache.offset,
                           dnp->n_name);
	*(ap->a_actualcount) = 0;
    *(ap->a_eofflag) = 0;
    
    /* We should not need to allocate va_name */
    if (VATTR_IS_ACTIVE(vap, va_name)) {
        SMB_ASSERT(vap->va_name != NULL);
    }

    /* Lock dir enum cache list */
    lck_mtx_lock(&dnp->d_enum_cache_list_lock);

    /* Setup the dir cache */
    smb_dir_cache_check(dvp, &dnp->d_main_cache, 1);

    while (1) {
        /*
         * If the offset is less than dnp->d_main_cache.offset (next entry to be cached),
         * then we can start filling from the dir cache.
         */
        if (offset < dnp->d_main_cache.offset) {
            /*
             * Start filling from dir cache
             */
			SMB_LOG_UNIT_TEST_LOCK(dnp, "LeaseUnitTest - Using enum cache for <%s> \n",
								   dnp->n_name);

			SMB_LOG_DIR_CACHE_LOCK(dnp, "Using enum cache offset %lld cache_offset %lld for <%s> \n",
                                   offset, dnp->d_main_cache.offset, dnp->n_name);
            
            SMB_LOG_KTRACE(SMB_DBG_GET_ATTRLIST_BULK | DBG_FUNC_NONE,
                           0xabc001, offset, dnp->d_main_cache.offset, 0, 0);

            /*
             * Find starting cached entry
             */
            enum_cache_currp = dnp->d_main_cache.list;
            skip_count = offset;
            
            for (i = 0; i < skip_count; i++) {
                if (i == 0) {
                    SMB_LOG_DIR_CACHE2_LOCK(dnp, "First skip <%s> for <%s> \n",
                                            enum_cache_currp->name,
                                            dnp->n_name);
                }
                if (i == (skip_count - 1)) {
                    SMB_LOG_DIR_CACHE2_LOCK(dnp, "Last skip <%s> for <%s> \n",
                                            enum_cache_currp->name,
                                            dnp->n_name);
                }

                if (enum_cache_currp == NULL) {
                    /* Should never happen */
                    SMBERROR_LOCK(dnp, "Out of cached entries i = %d offset = %lld/%lld for <%s> \n",
                                  i, offset, dnp->d_main_cache.offset, dnp->n_name);
                    smb_dir_cache_remove(dvp, &dnp->d_main_cache, "main", "Out of cached entries", 1);
                    smb_dir_cache_remove(dvp, &dnp->d_overflow_cache, "overflow", "Out of cached entries", 1);
                    goto fetch_new_entries;
                }
                
                enum_cache_currp = enum_cache_currp->next;
            }
            
            /* Do we need to verify the resume entry? */
            if (check_resume) {
                check_resume = 0;   /* Only need to check once on resume */
                tmp_error = smbfs_verify_resume(sessionp, dvp,
                                                enum_cache_currp,
                                                resume_cookiep,
                                                &resume_offset);
                if (tmp_error != 0) {
                    /* Retry the enumeration with the new offset */
                    offset = resume_offset;
                    continue;
                }
            }
            
            /* Start returning cached entries */
            first = 0;
            bzero(last_entry_namep, PATH_MAX);

            while (offset < dnp->d_main_cache.offset) {
                if (enum_cache_currp == NULL) {
                    /*
                     * We are out of cached dir entries but there may be more
                     * entries in the dir. Ask the server for more.
                     */
                    SMB_LOG_DIR_CACHE_LOCK(dnp, "No more cached, fetching more for <%s> \n",
                                           dnp->n_name);
                    goto fetch_new_entries;
                }
                
                error = smbfs_add_dir_entry_attr(share, dvp,
                                                 enum_cache_currp->name, enum_cache_currp->name_len,
                                                 ap->a_alist, ap->a_options,
                                                 vap, &enum_cache_currp->fattr,
                                                 uio, &max_fixed_len,
                                                 context);
                switch (error) {
                    case 0:
                        /* Item added successfully */
                        if (first == 0) {
                            SMB_LOG_DIR_CACHE2_LOCK(dnp, "First cache return <%s> for <%s> \n",
                                                    enum_cache_currp->name, dnp->n_name);
                            first = 1;
                        }

                        *ap->a_actualcount += 1;
                        offset++;
						if (offset > dnp->d_offset) {
							/* 
							 * To handle multiple threads enumerating at the 
							 * same time, only increment d_offset if its less
							 * than offset, meaning that we must have fetched
							 * more entries from server
							 */
							dnp->d_offset++;
						}

                        /* Save last entry added for debugging */
                        strncpy(last_entry_namep, enum_cache_currp->name,
                                PATH_MAX);
                        
                        break;
                        
                    case ENOBUFS:
                        /* uio is full and no more entries will fit */
                        if (strnlen(last_entry_namep, PATH_MAX)) {
                            SMB_LOG_DIR_CACHE2_LOCK(dnp, "Last cache return <%s> uio is full for <%s> \n",
                                                    last_entry_namep, dnp->n_name);
                        }
                        
                        /* Save next entry to be returned */
                        resume_cachep = enum_cache_currp;

                        error = 0;
                        goto done;
                        
                    default:
                        SMBERROR_LOCK(dnp, "smbfs_add_dir_entry failed %d for <%s> for <%s> \n",
                                      error, enum_cache_currp->name, dnp->n_name);
                        goto done;
                }
                
                /* on to next cached dir entry */
                enum_cache_currp = enum_cache_currp->next;
            }

            if (strnlen(last_entry_namep, PATH_MAX)) {
                SMB_LOG_DIR_CACHE2_LOCK(dnp, "Last cache return <%s> for <%s> \n",
                                        last_entry_namep, dnp->n_name);
            }
        }
        
        /*
         * Entire dir is cached and we are out of entries so the
         * enumeration is complete. Or it was just an empty dir and we are done.
         */
        if ((offset >= dnp->d_main_cache.offset) &&
            (dnp->d_main_cache.flags & kDirCacheComplete)) {

            /* Do we need to verify the resume entry? */
            if (check_resume) {
                check_resume = 0;   /* Only need to check once on resume */
                tmp_error = smbfs_verify_resume(sessionp, dvp,
                                                enum_cache_currp,
                                                resume_cookiep,
                                                &resume_offset);
                if (tmp_error != 0) {
                    /* Retry the enumeration with the new offset */
                    offset = resume_offset;
                    continue;
                }
            }

            SMB_LOG_DIR_CACHE_LOCK(dnp, "Enum completely cached and done for <%s> \n",
                                   dnp->n_name);
            error = ENOENT;
            goto done;
        }

        if ((dnp->d_main_cache.flags & kDirCachePartial) &&
            (add_remaining == 1)) {
            /* 
             * This time through loop was to just add the remaining entries
             * that we got right before hitting max number of entries allowed.
             */
            SMB_LOG_DIR_CACHE_LOCK(dnp, "Max entries reached for <%s> \n",
                                   dnp->n_name);

            /*
             * Main cache must have just gotten refilled, but its hit its max
             * and the current offset is past the max. We need to check the
             * overflow cache to see if we can return any more entries.
            */
            goto fetch_new_entries;
        }
        
fetch_new_entries:
        /*
         * If the main cache is null, then we are just refilling it so skip
         * adding saved overflow entries.
         * If overflow cache exists and offset is inside the
         * dnp->d_overflow_cache, then we can start filling from the overflow
         * cache.
         */
        if ((dnp->d_main_cache.list != NULL) &&
            (dnp->d_overflow_cache.list != NULL) &&
            (offset >= dnp->d_overflow_cache.start_offset) &&
            (offset < dnp->d_overflow_cache.offset)) {
            /*
             * Start filling from overflow cache
             */
            SMB_LOG_UNIT_TEST_LOCK(dnp, "LeaseUnitTest - Using saved for <%s> \n",
                                   dnp->n_name);

            SMB_LOG_DIR_CACHE_LOCK(dnp, "Using saved offset %lld cache_offset %lld for <%s>\n",
                                   offset, dnp->d_overflow_cache.offset, dnp->n_name);

            /*
             * Find starting cached entry
             */
            enum_cache_currp = dnp->d_overflow_cache.list;
            skip_count = offset - dnp->d_overflow_cache.start_offset;
            SMB_LOG_DIR_CACHE2_LOCK(dnp, "saved skip %lld offset %lld start %lld offset %lld for <%s> \n",
                                    skip_count, offset, dnp->d_overflow_cache.start_offset,
                                    dnp->d_overflow_cache.offset, dnp->n_name);

            for (i = 0; i < skip_count; i++) {
                if (i == 0) {
                    SMB_LOG_DIR_CACHE2("First saved skip <%s> \n", enum_cache_currp->name);
                }
                if (i == (skip_count - 1)) {
                    SMB_LOG_DIR_CACHE2("Last saved skip <%s> \n", enum_cache_currp->name);
                }

                if (enum_cache_currp == NULL) {
                    /* Can happen for overflow cache */
                    SMB_LOG_DIR_CACHE2_LOCK(dnp, "Ran out of saved i %d skip %lld offset %lld start %lld offset %lld for <%s> \n",
                                            i, skip_count, offset, dnp->d_overflow_cache.start_offset,
                                            dnp->d_overflow_cache.offset, dnp->n_name);
                    smb_dir_cache_remove(dvp, &dnp->d_overflow_cache, "overflow", "Out of cached entries", 1);
                    goto done_with_saved;
                }

                enum_cache_currp = enum_cache_currp->next;
            }

            /* Do we need to verify the resume entry? */
            if (check_resume) {
                check_resume = 0;   /* Only need to check once on resume */
                tmp_error = smbfs_verify_resume(sessionp, dvp,
                                                enum_cache_currp,
                                                resume_cookiep,
                                                &resume_offset);
                if (tmp_error != 0) {
                    /* Retry the enumeration with the new offset */
                    offset = resume_offset;
                    continue;
                }
            }

            /* Start returning saved cached entries */
            first = 0;
            bzero(last_entry_namep, PATH_MAX);

            while (offset < dnp->d_overflow_cache.offset) {
                if (enum_cache_currp == NULL) {
                    /*
                     * We are out of cached dir entries but there may be more
                     * entries in the dir. Ask the server for more.
                     */
                    SMB_LOG_DIR_CACHE_LOCK(dnp, "No more saved, fetching more for <%s> \n",
                                           dnp->n_name);
                    goto done_with_saved;
                }

                error = smbfs_add_dir_entry_attr(share, dvp,
                                                 enum_cache_currp->name, enum_cache_currp->name_len,
                                                 ap->a_alist, ap->a_options,
                                                 vap, &enum_cache_currp->fattr,
                                                 uio, &max_fixed_len,
                                                 context);
                switch (error) {
                    case 0:
                        /* Item added successfully */
                        if (first == 0) {
                            SMB_LOG_DIR_CACHE2_LOCK(dnp, "First saved return <%s> for <%s> \n",
                                                    enum_cache_currp->name, dnp->n_name);
                            first = 1;
                        }

                        *ap->a_actualcount += 1;
                        offset++;
                        if (offset > dnp->d_offset) {
                            /*
                             * To handle multiple threads enumerating at the
                             * same time, only increment d_offset if its less
                             * than offset, meaning that we must have fetched
                             * more entries from server
                             */
                            dnp->d_offset++;
                        }

                        /* Save last entry added for debugging */
                        strncpy(last_entry_namep, enum_cache_currp->name,
                                PATH_MAX);

                        break;

                    case ENOBUFS:
                        /* uio is full and no more entries will fit */
                        if (strnlen(last_entry_namep, PATH_MAX)) {
                            SMB_LOG_DIR_CACHE2_LOCK(dnp, "Last saved return <%s>. uio is full for <%s> \n",
                                                    last_entry_namep, dnp->n_name);
                        }
 
                        /* Save next entry to be returned */
                        resume_cachep = enum_cache_currp;

                        error = 0;
                        goto done;

                    default:
                        SMBERROR_LOCK(dnp, "Saved smbfs_add_dir_entry failed %d on <%s> for <%s> \n",
                                      error, enum_cache_currp->name, dnp->n_name);
                        goto done;
                }

                /* on to next cached dir entry */
                enum_cache_currp = enum_cache_currp->next;
            }

            if (strnlen(last_entry_namep, PATH_MAX)) {
                SMB_LOG_DIR_CACHE2_LOCK(dnp, "Last saved return <%s> for <%s> \n",
                                        last_entry_namep, dnp->n_name);
            }
        }

done_with_saved:
        /*
         * Go get more entries to fill either main or overflow cache
         */
        if (!(dnp->d_main_cache.flags & kDirCachePartial)) {
            /* Add more entries into main cache */
            error = smbfs_fetch_new_entries(share, dvp,
                                            &dnp->d_main_cache, offset,
                                            0, context);
            if ((error) && (error != ENOENT)) {
                goto done;
            }
            
            if (dnp->d_main_cache.flags & kDirCachePartial) {
                add_remaining = 1;
            }
            
            
            /*
             * For main cache, loop around and add entries that are now
             * cached. Repeat until no more entries or the buffer is full.
             */
            continue;
        }
        else {
            /* Add more entries into overflow cache */
            SMB_LOG_DIR_CACHE_LOCK(dnp, "Using overflow for <%s> \n",
                                   dnp->n_name);

            /* Overflow cache should now be empty and can be removed */
            smb_dir_cache_remove(dvp, &dnp->d_overflow_cache, "overflow", "done with saved", 1);

            /*
             * Start the enumeration from beginning for overflow cache
             */
            smbfs_closedirlookup(dnp, 0, "switch to overflow", context);

            overflow_error = smbfs_fetch_new_entries(share, dvp,
                                                     &dnp->d_overflow_cache, offset,
                                                     1, context);
            if ((overflow_error) && (overflow_error != ENOENT)) {
				error = overflow_error;
                goto done;
            }
        }
        
        first = 0;
        bzero(last_entry_namep, PATH_MAX);

        enum_cache_currp = dnp->d_overflow_cache.list;

        /* Do we need to verify the resume entry? */
        if (check_resume) {
            check_resume = 0;   /* Only need to check once on resume */
            tmp_error = smbfs_verify_resume(sessionp, dvp,
                                            enum_cache_currp,
                                            resume_cookiep,
                                            &resume_offset);
            if (tmp_error != 0) {
                /* Retry the enumeration with the new offset */
                offset = resume_offset;
                continue;
            }
        }

        /*
         * For overflow, just add as many of the entries that we just got
         * into the buffer
         */
        while (enum_cache_currp != NULL) {
            /* Is this the last entry and not finished enumerating? */
            if ((overflow_error != ENOENT) &&
                (enum_cache_currp->next == NULL)) {
                /* If so, need to save this entry for the next resume */
                if (strnlen(last_entry_namep, PATH_MAX)) {
                    SMB_LOG_DIR_CACHE2_LOCK(dnp, "Last overflow return <%s>. Save one entry for resume for <%s> \n",
                                            last_entry_namep, dnp->n_name);
                }

                /* Save next entry to be returned */
                resume_cachep = enum_cache_currp;
                error = 0;
                goto done;
            }
            
            error = smbfs_add_dir_entry_attr(share, dvp,
                                             enum_cache_currp->name, enum_cache_currp->name_len,
                                             ap->a_alist, ap->a_options,
                                             vap, &enum_cache_currp->fattr,
                                             uio, &max_fixed_len,
                                             context);
            switch (error) {
                case 0:
                    /* Item added successfully */
                    if (first == 0) {
                        SMB_LOG_DIR_CACHE2_LOCK(dnp, "First overflow return <%s> for <%s> \n",
                                                enum_cache_currp->name, dnp->n_name);
                        first = 1;
                    }

                    /* Save last entry added for debugging */
                    strncpy(last_entry_namep, enum_cache_currp->name,
                            PATH_MAX);

                    *ap->a_actualcount += 1;
                    offset++;
                    dnp->d_offset++;
                    break;
                    
                case ENOBUFS:
                    /* uio is full and no more entries will fit */
                    if (strnlen(last_entry_namep, PATH_MAX)) {
                        SMB_LOG_DIR_CACHE2_LOCK(dnp, "Last overflow return <%s>. uio is full for <%s> \n",
                                                last_entry_namep, dnp->n_name);
                    }
 
                    /* Save next entry to be returned */
                    resume_cachep = enum_cache_currp;

                    error = 0;
                    goto done;
                    
                default:
                    SMBERROR_LOCK(dnp, "Overflow smbfs_add_dir_entry failed %d on <%s> for <%s> \n",
                                  error, enum_cache_currp->name, dnp->n_name);
                    goto done;
            }
            
            /* on to next cached dir entry */
            enum_cache_currp = enum_cache_currp->next;
        }

        if (strnlen(last_entry_namep, PATH_MAX)) {
            SMB_LOG_DIR_CACHE2_LOCK(dnp, "Last overflow return <%s> for <%s> \n",
                                    last_entry_namep, dnp->n_name);
        }

		if (overflow_error == ENOENT) {
			SMB_LOG_DIR_CACHE_LOCK(dnp, "Overflow finished enumerating for <%s> \n",
								   dnp->n_name);
			error = ENOENT;
		}
		
        /* All done adding non cached entries */
        goto done;
    }
    
done:
    lck_mtx_unlock(&dnp->d_enum_cache_list_lock);

    if (error == ENOENT) {
		SMB_LOG_DIR_CACHE_LOCK(dnp, "Setting EOF flag for <%s> \n",
                               dnp->n_name);
		
        *(ap->a_eofflag) = TRUE;
        error = 0;
        
        /* At EOF, so next resume entry is NULL */
        resume_cachep = NULL;
    }
    
    /*
     * We use the uio offset to store the key which allows us to find
     * the dir cookie which tells us where to resume enumerating from.
     */
    if (key == 0) {
        SMB_LOG_DIR_CACHE_LOCK(dnp, "Why is the key still zero for <%s>? \n ",
                               dnp->n_name);
    }
    else {
        /* Ignore any errors */
        smbfs_update_cookie(dvp, key, offset, resume_cachep);
        
        uio_setoffset(uio, key);
    }
	
	/* Update the global dir enum cache list */
	tmp_error = smb_global_dir_cache_update_entry(dvp);
	if (tmp_error == ENOENT) {
		/* 
		 * Not yet in list, so add it in. Since we added a dir to the global
		 * cache, check to see if we need to remove an old dir.  We need to
		 * do this check after we free the lock on this dir to prevent a 
		 * deadlock <34561718>
		 */
		smb_global_dir_cache_add_entry(dvp, 0);
	}
    
err_exit:
    if (share != NULL) {
        smb_share_rele(share, context);
    }
    
	SMB_LOG_DIR_CACHE_LOCK(dnp, "actual count %d for <%s> \n",
                           *ap->a_actualcount, dnp->n_name);

	nanotime(&stop);
    SMB_LOG_DIR_CACHE_LOCK(dnp, "elapsed time %ld for <%s>\n",
                           stop.tv_sec - start.tv_sec, dnp->n_name);

    smbnode_unlock(VTOSMB(dvp));
	
	/* Do we need to prune the global dir enum cache list? */
	smb_global_dir_cache_prune(NULL, 0, context);
	
    SMB_LOG_KTRACE(SMB_DBG_GET_ATTRLIST_BULK | DBG_FUNC_END,
                   error, *ap->a_actualcount, 0, 0, 0);
    
    if (last_entry_namep != NULL) {
        SMB_FREE(last_entry_namep, M_SMBTEMP);
    }

    return (error);
}

vnop_t **smbfs_vnodeop_p;
static struct vnodeopv_entry_desc smbfs_vnodeop_entries[] = {
	{ &vnop_default_desc,		(vnop_t *) vn_default_error },
	{ &vnop_advlock_desc,		(vnop_t *) smbfs_vnop_advlock },
	{ &vnop_close_desc,			(vnop_t *) smbfs_vnop_close },
	{ &vnop_create_desc,		(vnop_t *) smbfs_vnop_create },
	{ &vnop_fsync_desc,			(vnop_t *) smbfs_vnop_fsync },
	{ &vnop_getattr_desc,		(vnop_t *) smbfs_vnop_getattr },
	{ &vnop_pagein_desc,		(vnop_t *) smbfs_vnop_pagein },
	{ &vnop_inactive_desc,		(vnop_t *) smbfs_vnop_inactive },
	{ &vnop_ioctl_desc,			(vnop_t *) smbfs_vnop_ioctl },
	{ &vnop_link_desc,			(vnop_t *) smbfs_vnop_link },
	{ &vnop_lookup_desc,		(vnop_t *) smbfs_vnop_lookup },
	{ &vnop_mkdir_desc,			(vnop_t *) smbfs_vnop_mkdir },
	{ &vnop_mknod_desc,			(vnop_t *) smbfs_vnop_mknod },
    { &vnop_mmap_check_desc,    (vnop_t *) smbfs_vnop_mmap_check },
	{ &vnop_mmap_desc,			(vnop_t *) smbfs_vnop_mmap },
	{ &vnop_mnomap_desc,		(vnop_t *) smbfs_vnop_mnomap },
	{ &vnop_open_desc,			(vnop_t *) smbfs_vnop_open },
	{ &vnop_compound_open_desc,	(vnop_t *) smbfs_vnop_compound_open },
	{ &vnop_pathconf_desc,		(vnop_t *) smbfs_vnop_pathconf },
	{ &vnop_pageout_desc,		(vnop_t *) smbfs_vnop_pageout },
    { &vnop_copyfile_desc,		(vnop_t *) smbfs_vnop_copyfile },
	{ &vnop_read_desc,			(vnop_t *) smbfs_vnop_read },
	{ &vnop_readdir_desc,		(vnop_t *) smbfs_vnop_readdir },
    { &vnop_readdirattr_desc,   (vnop_t *) smbfs_vnop_readdirattr },
    { &vnop_getattrlistbulk_desc, (vnop_t *) smbfs_vnop_getattrlistbulk },
	{ &vnop_readlink_desc,		(vnop_t *) smbfs_vnop_readlink },
	{ &vnop_reclaim_desc,		(vnop_t *) smbfs_vnop_reclaim },
	{ &vnop_remove_desc,		(vnop_t *) smbfs_vnop_remove },
	{ &vnop_rename_desc,		(vnop_t *) smbfs_vnop_rename },
	{ &vnop_rmdir_desc,			(vnop_t *) smbfs_vnop_rmdir },
	{ &vnop_setattr_desc,		(vnop_t *) smbfs_vnop_setattr },
	{ &vnop_symlink_desc,		(vnop_t *) smbfs_vnop_symlink },
	{ &vnop_write_desc,			(vnop_t *) smbfs_vnop_write },
	{ &vnop_blockmap_desc,		(vnop_t *) smbfs_vnop_blockmap },
	{ &vnop_strategy_desc,		(vnop_t *) smbfs_vnop_strategy },
	{ &vnop_searchfs_desc,		(vnop_t *) err_searchfs },
	{ &vnop_offtoblk_desc,		(vnop_t *) smbfs_vnop_offtoblk },
	{ &vnop_blktooff_desc,		(vnop_t *) smbfs_vnop_blktooff },
	{ &vnop_getxattr_desc,		(vnop_t *) smbfs_vnop_getxattr },
	{ &vnop_setxattr_desc,		(vnop_t *) smbfs_vnop_setxattr },
	{ &vnop_removexattr_desc,	(vnop_t *) smbfs_vnop_removexattr },
	{ &vnop_listxattr_desc,		(vnop_t *) smbfs_vnop_listxattr },
	{ &vnop_monitor_desc,		(vnop_t *) smbfs_vnop_monitor},
	{ &vnop_getnamedstream_desc, (vnop_t *) smbfs_vnop_getnamedstream },
    { &vnop_makenamedstream_desc, (vnop_t *) smbfs_vnop_makenamedstream },
    { &vnop_removenamedstream_desc, (vnop_t *) smbfs_vnop_removenamedstream },
	{ &vnop_access_desc,		(vnop_t *) smbfs_vnop_access },
	{ &vnop_allocate_desc,		(vnop_t *) smbfs_vnop_allocate },
	{ NULL, NULL }
};

struct vnodeopv_desc smbfs_vnodeop_opv_desc =
	{ &smbfs_vnodeop_p, smbfs_vnodeop_entries };
