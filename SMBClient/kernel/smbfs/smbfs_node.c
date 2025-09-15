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
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/time.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/queue.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <libkern/OSAtomic.h>

#include <libkern/crypto/md5.h>

#include <sys/kauth.h>
#include <sys/paths.h>

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

#include <smbfs/smbfs.h>
#include <smbfs/smbfs_lockf.h>
#include <smbfs/smbfs_node.h>
#include <smbfs/smbfs_subr.h>
#include <smbfs/smbfs_subr_2.h>
#include <smbfs/smbfs_security.h>
#include <Triggers/triggers.h>
#include <smbclient/smbclient_internal.h>

#define	SMBFS_NOHASH(smp, hval)	(&(smp)->sm_hash[(hval) & (smp)->sm_hashlen])
#define	smbfs_hash_lock(smp)	(lck_mtx_lock((smp)->sm_hashlock))
#define	smbfs_hash_unlock(smp)	(lck_mtx_unlock((smp)->sm_hashlock))

extern vnop_t **smbfs_vnodeop_p;


#define	FNV_32_PRIME ((uint32_t) 0x01000193UL)
#define	FNV1_32_INIT ((uint32_t) 33554467UL)

#define isdigit(d) ((d) >= '0' && (d) <= '9')

/*
 * Lease Hash Table
 */

lck_mtx_t global_Lease_hash_lock;
LIST_HEAD(g_lease_hash_head, smb_lease) *g_lease_hash;
u_long g_lease_hash_len = 0;
#define	SMBFS_LEASE_HASH(hval)	(&g_lease_hash[(hval) & g_lease_hash_len])


/*
 * smbfs_build_path
 *
 * Build a path that starts from the root node and includes this node. May
 * want to remove the SMBFS_MAXPATHCOMP limit in the future. That would require
 * two passes through the loop.
 */
static int 
smbfs_build_path(char *path, struct smbnode *np, size_t maxlen)
{
	struct smbnode  **npstack = NULL;
	struct smbnode  **npp;
	int i, error = 0;
    int lock_count = 0;
	struct smbnode **lock_stack = NULL;
	struct smbnode **locked_npp;
    SMB_MALLOC_TYPE_COUNT(npstack, struct smbnode *, SMBFS_MAXPATHCOMP, Z_WAITOK);
    npp = &npstack[0];
    SMB_MALLOC_TYPE_COUNT(lock_stack, struct smbnode *, SMBFS_MAXPATHCOMP, Z_WAITOK);
    locked_npp = &lock_stack[0];
    vnode_t par_vp = NULL;
	
    lck_rw_lock_shared(&np->n_parent_rwlock);   /* do our own locking */
    *locked_npp++ = np;     /* Save node to be unlocked later */
    lock_count += 1;

    i = 0;
    par_vp = smbfs_smb_get_parent(np, 0);   /* do our own locking */
    if ((par_vp == NULL) &&
        (np->n_parent_vid != 0)) {
        /* Parent got recycled already? */
        SMBWARNING_LOCK(np, "Missing parent for <%s> \n", np->n_name);
        error = ENOENT;
        goto done;
    }
    
    while (par_vp != NULL) {
		if (i++ == SMBFS_MAXPATHCOMP) {
			error = ENAMETOOLONG;
            vnode_put(par_vp);
            goto done;
        }
        
		*npp++ = np;
        np = VTOSMB(par_vp);
       
        lck_rw_lock_shared(&np->n_parent_rwlock);   /* do our own locking */
        *locked_npp++ = np;     /* Save node to be unlocked later */
        lock_count += 1;
        
        par_vp = smbfs_smb_get_parent(np, 0);   /* do our own locking */
        if ((par_vp == NULL) &&
            (np->n_parent_vid != 0)) {
            /* Parent got recycled already? */
            SMBWARNING_LOCK(np, "Missing parent for <%s> \n", np->n_name);
            error = ENOENT;
            goto done;
        }
	}

	while (i-- && !error) {
		np = *--npp;
		if (strlcat(path, "/", MAXPATHLEN) >= maxlen) {
			error = ENAMETOOLONG;
		}
        else {
            lck_rw_lock_shared(&np->n_name_rwlock);
			if (strlcat(path, (char *)np->n_name, maxlen) >= maxlen) {
				error = ENAMETOOLONG;
			}
            lck_rw_unlock_shared(&np->n_name_rwlock);
		}
	}
    
done:
    /* Unlock all the vnodes */
    for (i = 0; i < lock_count; i++) {
        lck_rw_unlock_shared(&lock_stack[i]->n_parent_rwlock);
        
        if ((i != 0) &&
            (lock_stack[i]->n_vnode != NULL)) {
            /* First vnode was not fetched with smbfs_smb_get_parent */
            vnode_put(lock_stack[i]->n_vnode);
        }
    }
    if (npstack) {
        SMB_FREE_TYPE_COUNT(struct smbnode *, SMBFS_MAXPATHCOMP, npstack);
    }
    if (lock_stack) {
        SMB_FREE_TYPE_COUNT(struct smbnode *, SMBFS_MAXPATHCOMP, lock_stack);
    }

	return error;
}

static void *
smbfs_trigger_get_mount_args(vnode_t vp, __unused vfs_context_t ctx, 
							 int *errp)
{
	struct mount_url_callargs *argsp;
	int	error = 0;
	int	length;
	char	*url, *mountOnPath;
	struct smbmount *smp = VTOSMB(vp)->n_mount;

	/*
	 * Allocate the args structure
	 */
    SMB_MALLOC_TYPE(argsp, struct mount_url_callargs, Z_WAITOK);

	/*
	 * Get the UID for which the mount should be done; it's the
	 * UID for which the mount containing the trigger was done,
	 * which might not be the UID for the process that triggered
	 * the mount.
	 */
	argsp->muc_uid = smp->sm_args.uid;

	/* 
	 * Create the URL
	 * 1. smb:
	 * 2. vnode's mount point from name
	 * 3. path from the root to this vnode.
	 * 4. URL must be less than MAXPATHLEN
	 *
	 * What should be the max length, for URL should it be the MAXPATHLEN
	 * plus the scheme.
	 */
    SMB_MALLOC_DATA(url, MAXPATHLEN, Z_WAITOK_ZERO);
	strlcpy(url, "smb:", MAXPATHLEN);
	if (strlcat(url, vfs_statfs(vnode_mount(vp))->f_mntfromname, MAXPATHLEN) >= MAXPATHLEN) {
		error = ENAMETOOLONG;
	} else {
		error = smbfs_build_path(url, VTOSMB(vp), MAXPATHLEN);
	}
	if (error) {
		SMBERROR_LOCK(VTOSMB(vp), "%s: URL FAILED url = %s\n", VTOSMB(vp)->n_name, url);

        SMB_FREE_DATA(url, MAXPATHLEN);
        SMB_FREE_TYPE(struct mount_url_callargs, argsp);
		*errp = error;
		return (NULL);
	}
	
	/* Create the mount on path */
    SMB_MALLOC_DATA(mountOnPath, MAXPATHLEN, Z_WAITOK_ZERO);
	length = MAXPATHLEN;
	/* This can fail sometimes, should we even bother with it? */
	error = vn_getpath(vp, mountOnPath, &length);
	if (error) {
		SMBERROR_LOCK(VTOSMB(vp), "%s: vn_getpath FAILED, using smbfs_build_path!\n", VTOSMB(vp)->n_name);
        
		if (strlcpy(mountOnPath, vfs_statfs(vnode_mount(vp))->f_mntonname, MAXPATHLEN) >= MAXPATHLEN) {
			error = ENAMETOOLONG;
		} else {
			error = smbfs_build_path(mountOnPath, VTOSMB(vp), MAXPATHLEN);
		}
	}
	if (error) {
		SMBERROR_LOCK(VTOSMB(vp), "%s: Mount on name FAILED url = %s\n", VTOSMB(vp)->n_name, url);
        
        SMB_FREE_DATA(mountOnPath, MAXPATHLEN);
        SMB_FREE_DATA(url, MAXPATHLEN);
        SMB_FREE_TYPE(struct mount_url_callargs, argsp);
		*errp = error;
		return (NULL);
	}
    
    SMBWARNING_LOCK(VTOSMB(vp), "%s: Triggering with URL = %s mountOnPath = %s\n",
                    VTOSMB(vp)->n_name, url, mountOnPath);

	argsp->muc_url = url;
	argsp->muc_mountpoint = mountOnPath;
	argsp->muc_opts = (smp->sm_args.altflags & SMBFS_MNT_SOFT) ? (char *)"soft" : (char *)"";
	*errp = 0;
	return (argsp);
}
	
static void
smbfs_trigger_rel_mount_args(void *data)
{
	struct mount_url_callargs *argsp = data;

    SMB_FREE_DATA(argsp->muc_url, MAXPATHLEN);
    SMB_FREE_DATA(argsp->muc_mountpoint, MAXPATHLEN);
    SMB_FREE_TYPE(struct mount_url_callargs, argsp);
}

/*
 * See if this is one of those faked up symbolic link. This is Conrad and Steve
 * French method for storing and reading symlinks on Window Servers.
 *
 * The calling routine must hold a reference on the share
 *
 */
static int 
smb_check_for_windows_symlink(struct smb_share *share, struct smbnode *np, 
							  int *symlen, vfs_context_t context)
{
    uio_t uio = NULL;
    MD5_CTX md5;
    char m5b[SMB_SYMMD5LEN];
    uint32_t state[4];
    int len = 0;
    unsigned char *sb = NULL;
    unsigned char *cp;
    SMBFID fid = 0;
    int error, cerror;
    size_t read_size = 0; /* unused */
    size_t sb_allocsize = np->n_size;
    char *target = NULL;

    SMB_MALLOC_DATA(sb, sb_allocsize, Z_WAITOK);
    if (sb == NULL) {
        error = ENOMEM;
        goto exit;
    }
    
    uio = uio_create(1, 0, UIO_SYSSPACE, UIO_READ);
    if (uio == NULL) {
        error = ENOMEM;
        goto exit;
    }

    uio_addiov(uio, CAST_USER_ADDR_T(sb), np->n_size);
    
    if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
        /* SMB 2/3 */
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
            goto exit;
        }

        error = smb_smb_read(share, fid, uio, 0, context);
        
        cerror = smbfs_tmpclose(share, np, fid, context);
        if (cerror) {
            SMBWARNING_LOCK(np, "error %d closing fid %llx file %s\n", cerror, fid, np->n_name);
        }
    }

    if (!error && !bcmp(sb, smb_symmagic, SMB_SYMMAGICLEN)) {
        for (cp = &sb[SMB_SYMMAGICLEN]; cp < &sb[SMB_SYMMAGICLEN+SMB_SYMLENLEN-1]; cp++) {
            if (!isdigit(*cp))
                break;
            len *= 10;
            len += *cp - '0';
        }
        cp++; /* skip newline */
        
        if ((cp != &sb[SMB_SYMMAGICLEN+SMB_SYMLENLEN]) ||
            (len > (int)(np->n_size - SMB_SYMHDRLEN))) {
            SMBWARNING("bad symlink length\n");
            error = ENOENT; /* Not a faked up symbolic link */
        } else {
            MD5Init(&md5);
            MD5Update(&md5, &sb[SMB_SYMHDRLEN], len);
            MD5Final((u_char *)state, &md5);
            (void)snprintf(m5b, sizeof(m5b), "%08x%08x%08x%08x",
                           htobel(state[0]), htobel(state[1]), htobel(state[2]),
                           htobel(state[3]));
            if (bcmp(cp, m5b, SMB_SYMMD5LEN-1)) {
                SMBWARNING("bad symlink md5\n");
                error = ENOENT; /* Not a faked up symbolic link */
            } else {
                *symlen = len;
                
                error = 0;
                
                /*
                 * Might as well update symlink cache since we just
                 * validated it. That way we wont do a check for a
                 * French/Minshall symlink, then do the exact same
                 * Create/Read/Close to fill it in.
                 */
                target = (char *)(sb + SMB_SYMHDRLEN);
                
                SMBSYMDEBUG_LOCK(np, "%s --> %s\n", np->n_name, target);
                
                smbfs_update_symlink_cache(np, target, len);
            }
        }
    }
    else {
		error = ENOENT; /* Not a faked up symbolic link */
    }
    
exit:
    if (uio != NULL) {
        uio_free(uio);
    }
    
    if (sb != NULL) {
        SMB_FREE_DATA(sb, sb_allocsize);
    }
    
    return error;
}

/*
 * Lock a node
 */
int 
smbnode_lock(struct smbnode *np, enum smbfslocktype locktype)
{
	if (locktype == SMBFS_SHARED_LOCK)
		lck_rw_lock_shared(&np->n_rwlock);
	else
		lck_rw_lock_exclusive(&np->n_rwlock);

	np->n_lockState = locktype;
	
#if 1	
	/* For Debugging... */
	if (locktype != SMBFS_SHARED_LOCK) {
		np->n_activation = (void *) current_thread();
	}
#endif
	return (0);
}


/*
 * Try to lock a node
 */
int
smbnode_trylock(struct smbnode *np, enum smbfslocktype locktype)
{
    int error = EBUSY;

    if (locktype == SMBFS_SHARED_LOCK) {
        if (lck_rw_try_lock(&np->n_rwlock, LCK_RW_TYPE_SHARED)) {
            /* Got the lock */
            error = 0;
        }
    }
    else {
        if (lck_rw_try_lock(&np->n_rwlock, LCK_RW_TYPE_EXCLUSIVE)) {
            /* Got the lock */
            error = 0;
        }
    }

    if (error == 0) {
        np->n_lockState = locktype;

#if 1
        /* For Debugging... */
        if (locktype != SMBFS_SHARED_LOCK) {
            np->n_activation = (void *) current_thread();
        }
#endif
    }

    return (error);
}


/*
 * Lock a pair of smbnodes
 *
 * If the two nodes are not the same then lock in the order they came in. The calling routine
 * should always put them in parent/child order.
 */
int 
smbnode_lockpair(struct smbnode *np1, struct smbnode *np2, enum smbfslocktype locktype)
{
	int error;

	/*
	 * If smbnodes match then just lock one.
	 */
	if (np1 == np2) {
		return smbnode_lock(np1, locktype);
	}
	if ((error = smbnode_lock(np1, locktype)))
		return (error);
	if ((error = smbnode_lock(np2, locktype))) {
		smbnode_unlock(np1);
		return (error);
	}
	return (0);
}

/*
 * Unlock a cnode
 */
void 
smbnode_unlock(struct smbnode *np)
{
	/* The old code called lck_rw_done which is a non supported kpi */
	if (np->n_lockState == SMBFS_SHARED_LOCK) {
		/* 
		 * Should we keep a counter and set n_lockState to zero when the 
		 * counter goes to zero? We would need to lock the counter in that
		 * case.
		 */
		lck_rw_unlock_shared(&np->n_rwlock);
	} else {
		/* Note: SMBFS_RECLAIM_LOCK is really SMBFS_EXCLUSIVE_LOCK */ 
		np->n_lockState = 0;
		lck_rw_unlock_exclusive(&np->n_rwlock);
	}
}

/*
 * Unlock a pair of cnodes.
 */
void 
smbnode_unlockpair(struct smbnode *np1, struct smbnode *np2)
{
	smbnode_unlock(np1);
	if (np2 != np1)
		smbnode_unlock(np2);
}

static int
tolower(unsigned char ch)
{
    if (ch >= 'A' && ch <= 'Z')
        ch = 'a' + (ch - 'A');
	
    return ch;
}

/*
 * SMB 2/3 - if the server supports File IDs, return ino as hashval
 * If no File IDs, create hashval from the name.  Currently we use strncasecmp 
 * to find a match, since it uses tolower, we should do the same when creating 
 * our hashval from the name.
 */
uint64_t
smbfs_hash(struct smb_share *share, uint64_t ino,
            const char *name, size_t nmlen)
{
	uint64_t v;
	
    /* if no share, just want hash from name */
    
    if ((share) && (SS_TO_SESSION(share)->session_misc_flags & SMBV_HAS_FILEIDS)) {
        /* Server supports File IDs, use the inode number as hash value */
        if (ino == 0) {
            /* This should not happen */
            SMBERROR("node id of 0 for %s\n", name);
        }
        
        v = ino;
    }
    else {
        /* Server does not support File IDs, hash the name instead */
        for (v = FNV1_32_INIT; nmlen; name++, nmlen--) {
            v *= FNV_32_PRIME;
            v ^= (uint64_t)tolower((unsigned char)*name);
        }
    }
    
	return v;
}

void
smb_vhashrem(struct smbnode *np)
{
	smbfs_hash_lock(np->n_mount);
	if (np->n_hash.le_prev) {
		LIST_REMOVE(np, n_hash);
		np->n_hash.le_prev = NULL;
	}
	smbfs_hash_unlock(np->n_mount);
	return;
}

void 
smb_vhashadd(struct smbnode *np, uint64_t hashval)
{
	struct smbnode_hashhead	*nhpp;
	
	smbfs_hash_lock(np->n_mount);
	nhpp = SMBFS_NOHASH(np->n_mount, hashval);
	LIST_INSERT_HEAD(nhpp, np, n_hash);
	smbfs_hash_unlock(np->n_mount);
	return;
	
}

/* Returns 0 if the names match, non zero if they do not match */
static int
smbfs_check_name(struct smb_share *share,
                 const char *name1,
                 const char *name2,
                 size_t name_len)
{
    int ret_val = 0;
    
    if (SS_TO_SESSION(share)->session_misc_flags & SMBV_OSX_SERVER) {
        /* Its OS X Server so we know for sure */
        if (SS_TO_SESSION(share)->session_volume_caps & kAAPL_CASE_SENSITIVE) {
            /* Case Sensitive */
            ret_val = bcmp(name1, name2, name_len);
            return (ret_val);
        }
    }
    
    /* Not case sensitive */
    ret_val = strncasecmp(name1, name2, name_len);

    return (ret_val);
}

static vnode_t
smb_hashget(struct smbmount *smp, struct smbnode *dnp, uint64_t hashval,
			const char *name, size_t nmlen, size_t maxfilenamelen, 
			uint32_t node_flag, const char *sname)
{
	vnode_t	vp;
	struct smbnode_hashhead	*nhpp;
	struct smbnode *np;
	uint32_t vid;
	size_t snmlen = (sname) ? strnlen(sname, maxfilenamelen+1) : 0;
    struct smb_session *sessionp = NULL;
    vnode_t par_vp = NULL;
    struct timespec ts;

    if (smp->sm_share == NULL) {
        SMBERROR("smp->sm_share is NULL? \n");
        return (NULL);
    }
    
    sessionp = SS_TO_SESSION(smp->sm_share);
    
loop:
	smbfs_hash_lock(smp);
	nhpp = SMBFS_NOHASH(smp, hashval);
	LIST_FOREACH(np, nhpp, n_hash) {
		/* 
		 * If we are only looking for a stream node then skip any other nodes. 
		 * If we are look for a directory or data node then skip any stream nodes.
		 */
		if ((np->n_flag & N_ISSTREAM) != node_flag)
			continue;
        
        if (sessionp->session_misc_flags & SMBV_HAS_FILEIDS) {
            /* 
             * Server supports File IDs - ID uniquely identifies the item
             */
            if (np->n_ino != hashval) {
                continue;
            }
        }
        else {
            /*
             * Server does not support File IDs
             * We currently assume the remote file system is case insensitive, since
             * we have no way of telling using the protocol. Someday I would like to
             * detect and if the server is case sensitive. If the server is case
             * sensitive then we should use bcmp, if case insensitive use strncasecmp.
             * NOTE: The strncasecmp routine really only does a tolower, not what we
             * really want but the best we can do at this time.
             */
            par_vp = smbfs_smb_get_parent(np, kShareLock);
            if (par_vp != NULL) {
                if (VTOSMB(par_vp) != dnp) {
                    /* Parents do not match, so not the correct smb node */
                    vnode_put(par_vp);
                    continue;
                }
                
                vnode_put(par_vp);
            }
            else {
                if (np->n_parent_vid != 0) {
                    /* Parent got recycled already? */
                    SMBWARNING_LOCK(np, "Missing parent for <%s> \n",
                                    np->n_name);
                }

                if (dnp != NULL) {
                    /* Parents do not match, so not the correct smb node */
                    continue;
                }
            }
            
            lck_rw_lock_shared(&np->n_name_rwlock);
            if ((np->n_nmlen != nmlen) ||
                (smbfs_check_name(smp->sm_share, name, np->n_name, nmlen) != 0)) {
                lck_rw_unlock_shared(&np->n_name_rwlock);
                continue;
            }
            lck_rw_unlock_shared(&np->n_name_rwlock);
        }
        
        if ((np->n_flag & NDELETEONCLOSE) ||
            (np->n_flag & NMARKEDFORDLETE)) {
            /* Skip nodes that are not in the name space anymore */
            continue;
        }

		/* If this is a stream make sure its the correct stream */
		if (np->n_flag & N_ISSTREAM) {
			DBG_ASSERT(sname);	/* Better be looking for a stream at this point */

            if ((np->n_snmlen != snmlen) ||
				(bcmp(sname, np->n_sname, snmlen) != 0)) {
				SMBERROR_LOCK(np, "We only support one stream and we found found %s looking for %s\n",
                              np->n_sname, sname);
				continue;
			}
		}
        
        /*
         * <82732371> Use a sleeptime to handle possible race where the thread that
         * set NALLOC/NTRANSIT did not see the NWALLOC bit so we will sleep forever.
         * Using sleeptime is enough here as we will just go over the table again
         * anyway, expecting the node to change state.
         */

		if (ISSET(np->n_flag_alloc, NALLOC)) {
            lck_mtx_lock(&np->n_flag_alloc_lock);
            /* was it already cleared? */
            if (ISSET(np->n_flag_alloc, NALLOC)) {
                SET(np->n_flag_alloc, NWALLOC);
                lck_mtx_unlock(&np->n_flag_alloc_lock);
                ts.tv_sec = 1;
                ts.tv_nsec = 0;
                (void)msleep((caddr_t)np, smp->sm_hashlock, PINOD|PDROP, "smb_ngetalloc", &ts);
                goto loop;
            } else {
                lck_mtx_unlock(&np->n_flag_alloc_lock);
            }
		}

		if (ISSET(np->n_flag_alloc, NTRANSIT)) {
            lck_mtx_lock(&np->n_flag_alloc_lock);
			SET(np->n_flag_alloc, NWTRANSIT);
            lck_mtx_unlock(&np->n_flag_alloc_lock);
            ts.tv_sec = 1;
            ts.tv_nsec = 0;
			(void)msleep((caddr_t)np, smp->sm_hashlock, PINOD|PDROP, "smb_ngettransit", &ts);
			goto loop;
		}

		vp = SMBTOV(np);
		vid = vnode_vid(vp);
        
		smbfs_hash_unlock(smp);
        
		if (vnode_getwithvid(vp, vid)) {
			return (NULL);
        }

		/* Always return the node locked */
        if ((smbnode_lock(np, SMBFS_EXCLUSIVE_LOCK)) != 0) {
            vnode_put(vp);
            return (NULL);
        }

		np->n_lastvop = smb_hashget;
		return (vp);
	}
    
	smbfs_hash_unlock(smp);
	return (NULL);
}

/*
 * We need to test to see if the vtype changed on the node. We currently only support
 * three types of vnodes (VDIR, VLNK, and VREG). If the network transacition came
 * from Unix extensions, Darwin or a create then we can just test to make sure the vtype
 * is the same. Otherwise we cannot tell the difference between a symbolic link and
 * a regular file at this point. So we just make sure it didn't change from a file
 * to a directory or vise versa. Also make sure it didn't change from a reparse point
 * to a non reparse point or vise versa.
 */
static int 
node_vtype_changed(struct smb_share *share, vnode_t vp, enum vtype node_vtype, struct smbfattr *fap)
{
	int rt_value = FALSE;	/* Always default to it no changing */
    struct smbnode *np = VTOSMB(vp);
	
	/* Root node can never change, bad things will happen */
	if (vnode_isvroot(vp))
		return FALSE;

    /* 
     * <18475915> See if the node ID changed. Creates do not return the 
     * node id. Sigh.
     */
    if ((share) && (SS_TO_SESSION(share)->session_misc_flags & SMBV_HAS_FILEIDS) &&
        (fap->fa_ino != 0)) {
        if (np->n_ino != fap->fa_ino) {
            rt_value = TRUE;
            goto done;
        }
    }
    
	if (vnode_isnamedstream(vp))	/* Streams have no type so ignore them */
		return FALSE;
	
	
	/* 
	 * The vtype is valid, use it to make the decision, Unix extensions, Darwin
	 * or a create.
	 */
	if (fap->fa_valid_mask & FA_VTYPE_VALID) {
		if ((np->n_flag & NWINDOWSYMLNK) && (fap->fa_vtype == VREG)) {
			/* 
			 * This is a windows fake symlink, so the node type will come is as
			 * a regular file. Never let it change unless the node type comes
			 * in as something other than a regular file.
			 */
			rt_value = FALSE;
		} else {
			rt_value = (fap->fa_vtype != node_vtype);
		}
		goto done;
	}
	
	/* Once a directory, always a directory */
	if (((node_vtype == VDIR) && !(np->n_dosattr & SMB_EFA_DIRECTORY)) ||
		((node_vtype != VDIR) && (np->n_dosattr & SMB_EFA_DIRECTORY))) {
		rt_value = TRUE;
		goto done;
	}

	/*
     * The only reparse points that we care about are DFS and symbolic links.
     * If it changes to being a reparse point or stopped being a reparse point
     * then we need to update the vnode
     */
	if ((np->n_dosattr & SMB_EFA_REPARSE_POINT) != (fap->fa_attr & SMB_EFA_REPARSE_POINT)) {
        if ((np->n_reparse_tag == IO_REPARSE_TAG_DFS) ||
            (np->n_reparse_tag == IO_REPARSE_TAG_SYMLINK) ||
            (fap->fa_reparse_tag == IO_REPARSE_TAG_DFS) ||
            (fap->fa_reparse_tag == IO_REPARSE_TAG_SYMLINK)) {
            rt_value = TRUE;
            goto done;
        }
	}

done:
	if (rt_value) {
		SMBWARNING_LOCK(np, "%s had type/attr/id of %d/0x%x/0x%llx now its %d/0x%x/0x%llx\n",
                        np->n_name,
                        node_vtype, np->n_dosattr, np->n_ino,
                        fap->fa_vtype, fap->fa_attr, fap->fa_ino);
	}

	return rt_value;
}

int
smbfs_is_ancestor(vnode_t potential_ancestor, struct smbnode *np)
{
    vnode_t parent_vp = NULLVP;
    int is_iocount_held = 0;

    while (np != NULL) {
        lck_rw_lock_shared(&np->n_parent_rwlock);
        if (np->n_parent_vnode == potential_ancestor) {
            lck_rw_unlock_shared(&np->n_parent_rwlock);
            if (is_iocount_held) {
                vnode_put(np->n_vnode);
            }
            return TRUE;
        }
        /*
         * Get an iocount on the parent so we can move np to it
         */
        parent_vp = smbfs_smb_get_parent(np, 0);
        lck_rw_unlock_shared(&np->n_parent_rwlock);
        if (is_iocount_held) {
            /*
             * decrease the iocount on the current np
             */
            vnode_put(np->n_vnode);
        }
        if (parent_vp == NULL) {
            break;
        }
        /*
         * Move np to the parent
         */
        np = VTOSMB(parent_vp);
        is_iocount_held = 1;
    }
    return FALSE;
}

/*
 * smbfs_nget
 *
 * When calling this routine remember if you get a vpp back and no error then
 * the smbnode is locked and you will need to unlock it.
 *
 * The calling routine must hold a reference on the share
 *
 */
int 
smbfs_nget(struct smb_share *share, struct mount *mp,
           vnode_t dvp, const char *name, size_t nmlen,
           struct smbfattr *fap, vnode_t *vpp,
           uint32_t cnflags, uint32_t flags,
           vfs_context_t context)
{
	struct smbmount *smp = VFSTOSMBFS(mp);
	struct smbnode *np, *dnp;
	vnode_t vp = NULLVP, parent_vp = NULLVP;
	int error = 0, i;
	uint64_t hashval;
	struct vnode_fsparam vfsp;
	int locked = 0;
	struct componentname cnp;
	trigger_info_t *ti;
	
    /*
     * Be careful as 
     * (1) dvp can be NULL
     * (2) name can be NULL
     * (3) fap can be NULL
     */
    
	*vpp = NULL;
    
	if (vfs_isforce(mp)) {
		return ENXIO;
    }
    
	if (!(flags & SMBFS_NGET_LOOKUP_ONLY)) {
        /* dvp is only required if we are possibly creating the vnode */
        if (smp->sm_rvp != NULL && dvp == NULL) {
            return EINVAL;
        }
    }
    
	if (nmlen == 2 && bcmp(name, "..", 2) == 0) {
		SMBDEBUG("do not call me with dotdot!\n");
		return EINVAL;
	} else if (nmlen == 1 && name[0] == '.') {
		SMBDEBUG("do not call me with dot!\n");
		return (EINVAL);
	}
    
	dnp = dvp ? VTOSMB(dvp) : NULL;
	if (dnp == NULL && dvp != NULL) {
		SMBDEBUG("dead parent vnode\n");
		return (EINVAL);
	}

	/* 
	 * If we are going to add it to the name cache, then make sure its the name 
	 * on the server that gets used 
	 */
	bzero(&cnp, sizeof(cnp));
	cnp.cn_nameptr = (char *)name;
	cnp.cn_namelen = (int)nmlen;
	cnp.cn_flags = cnflags;
	
    SMB_MALLOC_TYPE(np, struct smbnode, Z_WAITOK_ZERO);
    
    hashval = smbfs_hash(share, (fap ? fap->fa_ino: 0), name, nmlen);
    if ((SS_TO_SESSION(smp->sm_share)->session_misc_flags & SMBV_HAS_FILEIDS) &&
        (dnp) &&
        (dnp->n_ino == hashval)) {
        /*
         * rdar://110992237 Protect ourselves from panic when a parent
         * and a child have the same File ID due to server bug
         * File ID should be unique
         * Parent node is already locked at this point
         * and child will be locked when found in smb_hashget()
         */
        SMBERROR("Server returned the same ID for parent <%s> and child <%s> of <0x%llx>",
                 dnp->n_name, name, hashval);
        return EIO;
    }

	if ((*vpp = smb_hashget(smp, dnp, hashval, name, nmlen,
							share->ss_maxfilenamelen, 0, NULL)) != NULL) {
        /* Found a pre existing vnode */
		DBG_ASSERT(!vnode_isnamedstream(*vpp));
        
        /* 
         * Must be v_get and we have a blank fap except for the fa_ino so dont
         * try to update the meta data cache for this vnode
         */
        if (flags & SMBFS_NGET_NO_CACHE_UPDATE) {
            /* not going to create a vnode so dont need np */
            SMB_FREE_TYPE(struct smbnode, np);
			return (0);
        }

        if ((share) && (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) &&
            (SS_TO_SESSION(share)->session_misc_flags & SMBV_HAS_FILEIDS) &&
            vnode_isdir(*vpp)) {
            /*
             * rdar://113593179
             * Some servers during an enumeration will auto resolve a symlink
             * and return the target item instead of the symlink. This confuses
             * the path building code when a child item is a resolved symlink
             * to a dir that is an ancestor and will cause an infinite loop.
             * Ignore the node and return an error
             * Do as many checks before smbfs_smb_get_parent() and
             * do the ancestor check last to reduce performance overhead
             */
            lck_rw_lock_shared(&VTOSMB(*vpp)->n_parent_rwlock);
            parent_vp = VTOSMB(*vpp)->n_parent_vnode;
            lck_rw_unlock_shared(&VTOSMB(*vpp)->n_parent_rwlock);
            if (parent_vp && dnp && (parent_vp != dnp->n_vnode)) {
                /*
                 * The saved parent is not dnp, this directory moved to dnp?
                 */
                if (smbfs_is_ancestor(*vpp, dnp)) {
                    /*
                     * a directory cannot move to its decendant
                     */
                    SMBERROR("found <%s>/<%s> for name <%s>, <%s> is ancestor of <%s> ", dnp->n_name,
                             VTOSMB(*vpp)->n_name, name, VTOSMB(*vpp)->n_name, dnp->n_name);
                    smbnode_unlock(VTOSMB(*vpp));
                    vnode_put(*vpp);
                    *vpp = NULL;
                    return EIO;
                }
            }
        }

		if (fap && node_vtype_changed(share, *vpp, vnode_vtype(*vpp), fap)) {
			/* 
			 * The node we found has the wrong vtype. We need to remove this one
			 * and create the new entry. Purge the old node from the name cache, 
			 * remove it from our hash table, and clear its cache timer. 
			 */
			cache_purge(*vpp);
			smb_vhashrem(VTOSMB(*vpp));
			VTOSMB(*vpp)->attribute_cache_timer = 0;
			VTOSMB(*vpp)->n_symlink_cache_timer = 0;
			smbnode_unlock(VTOSMB(*vpp));	/* Release the smbnode lock */
			vnode_put(*vpp);
			/* Now fall through and create the node with the correct vtype */
			*vpp = NULL;
		}
        else {
            /* not going to create a vnode so dont need np */
            SMB_FREE_TYPE(struct smbnode, np);

			/* update the attr_cache info, this is never a stream node */
			if (fap) {
				smbfs_attr_cacheenter(share, *vpp, fap, FALSE, context);
            }

			if (dvp && (cnp.cn_flags & MAKEENTRY)) {
				cache_enter(dvp, *vpp, &cnp);
            }

			return (0);
		}
	}

	/*
	 * If SMBFS_NGET_LOOKUP_ONLY set, then it is an explicit lookup
	 * for an existing vnode. Return if the vnode does not already exist.
	 */
	if (flags & SMBFS_NGET_LOOKUP_ONLY) {
        SMB_FREE_TYPE(struct smbnode, np);
		return (ENOENT);
	}

    if (fap == NULL) {
        /* This should never happen */
        SMBERROR("fap is NULL! \n");
        SMB_FREE_TYPE(struct smbnode, np);
		return (ENOENT);
    }
	
    lck_rw_init(&np->n_rwlock, smbfs_rwlock_group, smbfs_lock_attr);
	lck_rw_init(&np->n_name_rwlock, smbfs_rwlock_group, smbfs_lock_attr);
	lck_rw_init(&np->n_parent_rwlock, smbfs_rwlock_group, smbfs_lock_attr);
    lck_mtx_init(&np->n_flag_alloc_lock, smbfs_mutex_group, smbfs_lock_attr);
    lck_mtx_init(&np->n_strategy_mtx, smbfs_mutex_group, smbfs_lock_attr);

	(void) smbnode_lock(np, SMBFS_EXCLUSIVE_LOCK);
	/* if we error out, don't forget to unlock this */
	locked = 1;
	np->n_lastvop = smbfs_nget;

	/* 
	 * The node_vtype_changed routine looks at the attributes field to 
	 * detemine if the node has changed from being a reparse point. So before 
	 * entering the smbfs_attr_cacheenter we need to make sure that the attributes
	 * field has been set when the node is created.
	 * 
	 * We only set the ReparseTag here, once a tag is set its always set. We 
	 * use node_vtype_changed to test if a reparse point has been removed. 
	 */
	np->n_reparse_tag = fap->fa_reparse_tag;
	np->n_dosattr = fap->fa_attr;
	np->n_vnode = NULL;	/* redundant, but emphatic! */
	np->n_mount = smp;
	np->n_size = fap->fa_size;
	np->n_data_alloc = fap->fa_data_alloc;
	np->n_ino = fap->fa_ino;
    
    lck_rw_lock_exclusive(&np->n_name_rwlock);
	np->n_name = smb_strndup(name, nmlen, &np->n_name_allocsize);
    lck_rw_unlock_exclusive(&np->n_name_rwlock);
    
	np->n_nmlen = nmlen;
	/* Default to what we can do and Windows support */
	np->n_flags_mask = EXT_REQUIRED_BY_MAC;
    
    /*
     * n_uid and n_gid are set to KAUTH_UID_NONE/KAUTH_GID_NONE as the
     * default.
     *
     * If ACLs are retrieved for this node, then we will replace n_uid/n_gid
     * with a uid/gid that was mapped from the SID.
     *
     * When asked for the uid/gid, if they are default values, we return
     * uid/gid of the mounting user. If they are not set to default values,
     * then ACLs must have been retrieved and the uid/gid set, so we return
     * what ever value is set in n_uid/n_gid.
     */
	np->n_uid = KAUTH_UID_NONE;
	np->n_gid = KAUTH_GID_NONE;
    
    /*
     * n_nfs_uid/n_nfs_gid are the uid/gid from ACLs and from the NFS ACE.
     * We dont really do much with it because OS X <-> Windows, we cant really
     * trust its value. OS X <-> OS X we could trust its value.
     */
	np->n_nfs_uid = KAUTH_UID_NONE;
	np->n_nfs_gid = KAUTH_GID_NONE;
	SET(np->n_flag_alloc, NALLOC);
	smb_vhashadd(np, hashval);
	if (dvp) {
        lck_rw_lock_exclusive(&np->n_parent_rwlock);
        np->n_parent_vnode = dvp;
        np->n_parent_vid = vnode_vid(dvp);
        lck_rw_unlock_exclusive(&np->n_parent_rwlock);
        
		if (!vnode_isvroot(dvp)) {
			/* Make sure we can get the vnode, we could have an unmount about to happen */
			if (vnode_get(dvp) == 0) {
				if (vnode_ref(dvp) == 0) {
                    /* If we can get a refcnt then mark the child */
					np->n_flag |= NREFPARENT;
                    vnode_put(dvp);
                    
                    /* Increment parent node's child refcnt */
                    OSIncrementAtomic(&dnp->n_child_refcnt);
                } else {
                    vnode_put(dvp);
                    error = EINVAL;
                    goto errout;
                }
			} else {
                error = EINVAL;
                goto errout;
            }
		}
	}

	vfsp.vnfs_mp = mp;
	vfsp.vnfs_vtype = fap->fa_vtype;
	vfsp.vnfs_str = "smbfs";
	vfsp.vnfs_dvp = dvp;
	vfsp.vnfs_fsnode = np;
	/* This will make sure we always have a vp->v_name */
	vfsp.vnfs_cnp = &cnp;
	vfsp.vnfs_vops = smbfs_vnodeop_p;
	vfsp.vnfs_rdev = 0;	/* no VBLK or VCHR support */
	vfsp.vnfs_flags = (dvp && (cnp.cn_flags & MAKEENTRY)) ? 0 : VNFS_NOCACHE;
	vfsp.vnfs_markroot = (np->n_ino == smp->sm_root_ino);
	vfsp.vnfs_marksystem = 0;
	
	/*
	 * We are now safe to do lookups with the node. We need to be careful with 
	 * the n_vnode field and we should always check to make sure its not null 
	 * before access that field. The current code always makes that check.
	 * 
	 * So if this is the root vnode then we need to make sure we can access it 
	 * across network without any errors. We keep a reference on the root vnode 
	 * so this only happens once at mount time.
	 *
	 * If this is a regular file then we need to see if its one of our special 
	 * Windows symlink files.
	 */
	if ((vfsp.vnfs_vtype == VDIR) && (dvp == NULL) && (smp->sm_rvp == NULL) && 
		(np->n_ino == smp->sm_root_ino)) {
        /* Lookup the root vnode */
		error = smbfs_lookup(share, np, NULL, NULL, NULL, fap, context);
		if (error) {
			goto errout;
        }
		
        /* Update the root vnode hash value */
        smb_vhashrem(np);
        
        if (!(SS_TO_SESSION(share)->session_misc_flags & SMBV_HAS_FILEIDS)) {
            /* 
             * Server does not support File IDs, so set root vnode File ID to 
             * be SMBFS_ROOT_INO */
            fap->fa_ino = SMBFS_ROOT_INO;
        }

        hashval = smbfs_hash(share, fap->fa_ino, name, nmlen);
        
        /* Update the root vnode File ID */
        smp->sm_root_ino = np->n_ino = fap->fa_ino;

        smb_vhashadd(np, hashval);
	} else if ((vfsp.vnfs_vtype == VREG) && (np->n_size == SMB_SYMLEN)) {
		int symlen = 0;
		DBG_ASSERT(dvp);
		if (smb_check_for_windows_symlink(share, np, &symlen, context) == 0) {
			vfsp.vnfs_vtype = VLNK;
			fap->fa_valid_mask |= FA_VTYPE_VALID;
			fap->fa_vtype = VLNK;
			np->n_size = symlen;
			np->n_flag |= NWINDOWSYMLNK;			
		}
	}
	vfsp.vnfs_filesize = np->n_size;

	if ((np->n_dosattr & SMB_EFA_REPARSE_POINT) && 
		(np->n_reparse_tag != IO_REPARSE_TAG_DFS) && 
        (np->n_reparse_tag != IO_REPARSE_TAG_SYMLINK) &&
        (np->n_reparse_tag != IO_REPARSE_TAG_DEDUP) &&
        (np->n_reparse_tag != IO_REPARSE_TAG_STORAGE_SYNC) &&
        (np->n_reparse_tag != IO_REPARSE_TAG_MOUNT_POINT))  {
        SMBWARNING_LOCK(np, "%s - unknown reparse point tag 0x%x\n", np->n_name, np->n_reparse_tag);
	}
	
	if ((np->n_dosattr & SMB_EFA_REPARSE_POINT) && 
        ((np->n_reparse_tag == IO_REPARSE_TAG_DFS) ||
        (np->n_reparse_tag == IO_REPARSE_TAG_MOUNT_POINT))) {
		struct vnode_trigger_param vtp;

		bcopy(&vfsp, &vtp.vnt_params, sizeof(vfsp));
		ti = trigger_new(&vtp, smbfs_trigger_get_mount_args, smbfs_trigger_rel_mount_args);
		error = vnode_create(VNCREATE_TRIGGER, (uint32_t)VNCREATE_TRIGGER_SIZE, &vtp, &vp);
		if (error)
			trigger_free(ti);
	} else {
		error = vnode_create(VNCREATE_FLAVOR, (uint32_t)VCREATESIZE, &vfsp, &vp);
	}

	if (error)
		goto errout;
    
	vnode_settag(vp, VT_CIFS);
	np->n_vnode = vp;
    
	/*
	 * We now know what type of node we have so set the mode bit here. We never
	 * want to change this for the life of this node. If the type changes on
	 * the server then we will blow away this node and create a new one.
	 */
	switch (vnode_vtype(vp)) {
	    case VREG:
			np->n_mode |= S_IFREG;
			break;
	    case VLNK:
			np->n_mode |= S_IFLNK;
			break;
	    case VDIR:
			np->n_mode |= S_IFDIR;
			break;
	    default:
			SMBERROR("vnode_vtype %d\n", vnode_vtype(vp));
			np->n_mode |= S_IFREG;	/* Can't happen, but just to be safe */
	}
	
	/*
	 * Lease key hi/low is unique uuid
	 *
	 * The lease key in the vnode is used for dir leases or for the
	 * shared FID
	 */
	uuid_generate((uint8_t *) &np->n_lease_key_hi); /* also fills in n_lease_key_low */

    np->n_epoch = 0;	/* always starts at 0 */

	/* Initialize the lock used for the open state, open deny list and resource size/timer */
	if (!vnode_isdir(vp)) {
		/* Mutexes in both regular and stream nodes */
		lck_mtx_init(&np->f_openStateLock, smbfs_mutex_group, smbfs_lock_attr);
		lck_mtx_init(&np->f_clusterWriteLock, smbfs_mutex_group, smbfs_lock_attr);
        lck_mtx_init(&np->f_sharedFID_BRL_lock, smbfs_mutex_group, smbfs_lock_attr);
        lck_mtx_init(&np->f_lockFID_BRL_lock, smbfs_mutex_group, smbfs_lock_attr);

        /* Init a lease for all files */
        smb2_lease_init(share, dnp, np, 0, &np->n_lease, 1);
        
        /* Init both durable handle for all files */
        smb2_dur_handle_init(share, 0, &np->f_sharedFID_dur_handle, 1);
        for (i = 0; i < 3; i++) {
            smb2_dur_handle_init(share, 0, &np->f_sharedFID_lockEntries[i].dur_handle, 1);
        }
        
        smb2_dur_handle_init(share, 0, &np->f_lockFID_dur_handle, 1);
        
        /* Based on name, is compression allowed? */
        if (smb_compression_allowed(mp, vp) == 0) {
            /* Compression not allowed on this file based on extension */
            np->n_flag |= N_DONT_COMPRESS;
        }
        else {
            /* Compression is allowed on this file based on extension */
            np->n_flag &= ~N_DONT_COMPRESS;
        }

    }
    else {
        lck_mtx_init(&np->d_enum_cache_list_lock, smbfs_mutex_group, smbfs_lock_attr);
        lck_mtx_init(&np->d_cookie_lock, smbfs_mutex_group, smbfs_lock_attr);

		np->d_main_cache.offset = 0;
        np->d_overflow_cache.offset = 0;
        
        /* Init just a lease for a dir */
        smb2_lease_init(share, dnp, np, 0, &np->n_lease, 1);
    }

    lck_mtx_init(&np->rfrkMetaLock, smbfs_mutex_group, smbfs_lock_attr);
	lck_mtx_init(&np->f_ACLCacheLock, smbfs_mutex_group, smbfs_lock_attr);

	/* update the attr_cache info, this is never a stream node */
	smbfs_attr_cacheenter(share, vp, fap, FALSE, context);

	*vpp = vp;

    lck_mtx_lock(&np->n_flag_alloc_lock);
	CLR(np->n_flag_alloc, NALLOC);
    if (ISSET(np->n_flag_alloc, NWALLOC)) {
        CLR(np->n_flag_alloc, NWALLOC);
        lck_mtx_unlock(&np->n_flag_alloc_lock);
        wakeup(np);
    } else {
        lck_mtx_unlock(&np->n_flag_alloc_lock);
    }
	return 0;
    
errout:
	if (np->n_flag & NREFPARENT) {
		if (vnode_get(dvp) == 0) {
			vnode_rele(dvp);
			vnode_put(dvp);			
		}
		np->n_flag &= ~NREFPARENT;
        
        /* Remove the child refcnt from the parent we just added above */
        OSDecrementAtomic(&dnp->n_child_refcnt);
	}
	
	smb_vhashrem(np);
	
	if (locked == 1)
		smbnode_unlock(np);	/* Release the smbnode lock */

	if (ISSET(np->n_flag_alloc, NWALLOC))
		wakeup(np);
		
    lck_rw_lock_exclusive(&np->n_name_rwlock);
    if (np->n_name != NULL) {
        SMB_FREE_DATA(np->n_name, np->n_name_allocsize);
    }
    lck_rw_unlock_exclusive(&np->n_name_rwlock);
    
	lck_rw_destroy(&np->n_rwlock, smbfs_rwlock_group);
	lck_rw_destroy(&np->n_name_rwlock, smbfs_rwlock_group);
	lck_rw_destroy(&np->n_parent_rwlock, smbfs_rwlock_group);
    lck_mtx_destroy(&np->n_flag_alloc_lock, smbfs_mutex_group);
    lck_mtx_destroy(&np->n_strategy_mtx, smbfs_mutex_group);

    SMB_FREE_TYPE(struct smbnode, np);
    
	return error;
}

/* 
* smbfs_find_vgetstrm
 *
 * When calling this routine remember if you get a vpp back and no error then
 * the smbnode is locked and you will need to unlock it.
 */
vnode_t 
smbfs_find_vgetstrm(struct smbmount *smp, struct smbnode *np, const char *sname,
					size_t maxfilenamelen)
{
	uint64_t hashval;
    vnode_t ret_vnode = NULL;
    
    lck_rw_lock_shared(&np->n_name_rwlock);
    
    hashval = smbfs_hash(smp->sm_share, np->n_ino, np->n_name, np->n_nmlen);
	ret_vnode = smb_hashget(smp, np, hashval, np->n_name, np->n_nmlen, maxfilenamelen,
					   N_ISSTREAM, sname);
    
    lck_rw_unlock_shared(&np->n_name_rwlock);
    
	return(ret_vnode);
}

/* 
* smbfs_vgetstrm
 *
 * When calling this routine remember if you get a vpp back and no error then
 * the smbnode is locked and you will need to unlock it.
 *
 * The calling routine must hold a reference on the share
 *
 */
int 
smbfs_vgetstrm(struct smb_share *share, struct smbmount *smp, vnode_t vp, 
			   vnode_t *svpp, struct smbfattr *fap, const char *sname)
{
	struct smbnode *np, *snp;
	int error = 0, i;
	uint64_t hashval;
	struct vnode_fsparam vfsp;
	int locked = 0;
	struct componentname cnp;
	size_t maxfilenamelen = share->ss_maxfilenamelen;
    char *tmp_namep = NULL;
    size_t tmp_name_allocsize = 0;
	
	/* Better have a root vnode at this point */
	DBG_ASSERT(smp->sm_rvp);
	/* Better have a parent vnode at this point */
	DBG_ASSERT(vp);
	/* Parent vnode better not be a directory */
	DBG_ASSERT((!vnode_isdir(vp)));
	/* Parent vnode better not be a stream */
	DBG_ASSERT(!vnode_isnamedstream(vp));
	np = VTOSMB(vp);
	*svpp = NULL;
	
	if (vfs_isforce(smp->sm_mp))
		return ENXIO;	
	/* Make sure the we have the correct name, always return the xattr name */
	bzero(&cnp, sizeof(cnp));
	cnp.cn_nameiop = LOOKUP;
	cnp.cn_flags = ISLASTCN;
	cnp.cn_pnlen = MAXPATHLEN;
    SMB_MALLOC_DATA(cnp.cn_pnbuf, MAXPATHLEN, Z_WAITOK);
	if (bcmp(sname, SFM_RESOURCEFORK_NAME, sizeof(SFM_RESOURCEFORK_NAME)) == 0) {
		cnp.cn_nameptr = cnp.cn_pnbuf;
        lck_rw_lock_shared(&np->n_name_rwlock);
		cnp.cn_namelen = snprintf(cnp.cn_nameptr, MAXPATHLEN, "%s%s", np->n_name,
								  _PATH_RSRCFORKSPEC);
        lck_rw_unlock_shared(&np->n_name_rwlock);
	}
    else {
		cnp.cn_nameptr = cnp.cn_pnbuf;
        lck_rw_lock_shared(&np->n_name_rwlock);
		cnp.cn_namelen = snprintf(cnp.cn_nameptr, MAXPATHLEN, "%s%s%s", np->n_name,
								  _PATH_FORKSPECIFIER, sname);
        lck_rw_unlock_shared(&np->n_name_rwlock);
		SMBWARNING("Creating non resource fork named stream: %s\n", cnp.cn_nameptr);
	}

    SMB_MALLOC_TYPE(snp, struct smbnode, Z_WAITOK_ZERO);

    lck_rw_lock_shared(&np->n_name_rwlock);
    hashval = smbfs_hash(share, fap->fa_ino, np->n_name, np->n_nmlen);
	if ((*svpp = smb_hashget(smp, np, hashval, np->n_name, np->n_nmlen,
							 maxfilenamelen, N_ISSTREAM, sname)) != NULL) {
        lck_rw_unlock_shared(&np->n_name_rwlock);
        SMB_FREE_TYPE(struct smbnode, snp);
		/* 
		 * If this is the resource stream then the parents resource fork size 
		 * has already been update. The calling routine aleady updated it. 
		 * Remember that the parent is currently locked. smbfs_attr_cacheenter 
		 * can lock the parent if we tell it to update the parent, so never tell 
		 * it to update the parent in this routine. 
		 */
		smbfs_attr_cacheenter(share, *svpp, fap, FALSE, NULL);
		goto done;
	}
    lck_rw_unlock_shared(&np->n_name_rwlock);

	bzero(snp, sizeof(*snp));
	lck_rw_init(&snp->n_rwlock, smbfs_rwlock_group, smbfs_lock_attr);
	lck_rw_init(&snp->n_name_rwlock, smbfs_rwlock_group, smbfs_lock_attr);
	lck_rw_init(&snp->n_parent_rwlock, smbfs_rwlock_group, smbfs_lock_attr);
    lck_mtx_init(&snp->n_flag_alloc_lock, smbfs_mutex_group, smbfs_lock_attr);
    lck_mtx_init(&snp->n_strategy_mtx, smbfs_mutex_group, smbfs_lock_attr);

	(void) smbnode_lock(snp, SMBFS_EXCLUSIVE_LOCK);
	locked = 1;
	snp->n_lastvop = smbfs_vgetstrm;
	
	snp->n_mount = smp;
	snp->n_size =  fap->fa_size;
	snp->n_data_alloc = fap->fa_data_alloc;
	snp->n_ino = np->n_ino;
    
    lck_rw_lock_shared(&np->n_name_rwlock);
    tmp_namep = smb_strndup(np->n_name, np->n_nmlen, &tmp_name_allocsize);
    lck_rw_unlock_shared(&np->n_name_rwlock);
    
    lck_rw_lock_exclusive(&snp->n_name_rwlock);
	snp->n_name = tmp_namep;
    lck_rw_unlock_exclusive(&snp->n_name_rwlock);
    
	snp->n_nmlen = np->n_nmlen;
    snp->n_name_allocsize = tmp_name_allocsize;
	snp->n_flags_mask = np->n_flags_mask;
	snp->n_uid = np->n_uid;
	snp->n_gid = np->n_gid;
	snp->n_nfs_uid = np->n_nfs_uid;
	snp->n_nfs_gid = np->n_nfs_uid;
    
    lck_rw_lock_exclusive(&snp->n_parent_rwlock);
    snp->n_parent_vnode = vp;
    snp->n_parent_vid = vnode_vid(vp);
    lck_rw_unlock_exclusive(&snp->n_parent_rwlock);
    
	/* Only a stream node can have a stream name */
	snp->n_snmlen = strnlen(sname, maxfilenamelen+1);
    lck_rw_lock_exclusive(&snp->n_name_rwlock);
	snp->n_sname = smb_strndup(sname, snp->n_snmlen, &snp->n_sname_allocsize);
    lck_rw_unlock_exclusive(&snp->n_name_rwlock);
	
	SET(snp->n_flag, N_ISSTREAM);
	/* Special case that I would like to remove some day */
	if (bcmp(sname, SFM_RESOURCEFORK_NAME, sizeof(SFM_RESOURCEFORK_NAME)) == 0)
		SET(snp->n_flag, N_ISRSRCFRK);
	SET(snp->n_flag_alloc, NALLOC);
	smb_vhashadd(snp, hashval);
    
#ifdef _NOT_YET_
    /* Note: Temporarily commenting this out, see <rdar://problem/10695860> */
    
    /* Make sure we can get the parent vnode, we could have an unmount about to happen */
    if (!vnode_isvroot(vp)) {
        if (vnode_get(vp) == 0) {
            if (vnode_ref(vp) == 0) {
                /* If we can get a refcnt then mark the child */
                snp->n_flag |= NREFPARENT;
                vnode_put(vp);
                
                /* Increment parent node's child refcnt */
                OSIncrementAtomic(&np->n_child_refcnt);
            } else {
                vnode_put(vp);
                error = EINVAL;
                goto errout;
            }
        } else {
            error = EINVAL;
            goto errout;
        }
    }
#endif    
	vfsp.vnfs_mp = smp->sm_mp;
	vfsp.vnfs_vtype = VREG;
	vfsp.vnfs_str = "smbfs";
	vfsp.vnfs_dvp = NULL;
	vfsp.vnfs_fsnode = snp;
	/* This will make sure we always have  a vp->v_name */
	vfsp.vnfs_cnp = &cnp;
	vfsp.vnfs_vops = smbfs_vnodeop_p;
	vfsp.vnfs_rdev = 0;	/* no VBLK or VCHR support */
	vfsp.vnfs_flags = VNFS_NOCACHE;
	vfsp.vnfs_markroot = 0;
	vfsp.vnfs_marksystem = 0;
	vfsp.vnfs_filesize = fap->fa_size;
	
	error = vnode_create(VNCREATE_FLAVOR, (uint32_t)VCREATESIZE, &vfsp, svpp);
	if (error)
		goto errout;
	vnode_settag(*svpp, VT_CIFS);
	snp->n_vnode = *svpp;
	/*
	 * We now know what type of node we have so set the mode bit here. We never
	 * what to change this for the life of this node. If the type changes on
	 * the server then we will blow away this node and create a new one.
	 *
	 * Streams are aways regular files and have the parent node's access.
	 *
	 */
	snp->n_mode = S_IFREG | (np->n_mode & ACCESSPERMS);

	/* Mutexes in both regular and stream nodes */
	lck_mtx_init(&snp->f_openStateLock, smbfs_mutex_group, smbfs_lock_attr);
	lck_mtx_init(&snp->f_clusterWriteLock, smbfs_mutex_group, smbfs_lock_attr);
    lck_mtx_init(&snp->f_sharedFID_BRL_lock, smbfs_mutex_group, smbfs_lock_attr);
    lck_mtx_init(&snp->f_lockFID_BRL_lock, smbfs_mutex_group, smbfs_lock_attr);
    
    /*
     * <89437556> Resource Fork is a special case where it can be opened
     * directly as a file (foo/..namedfork/rsrc) or accessed as an xattr.
     * When its accessed as a file, then it will get a durable handle and
     * lease like any other file, so have to set up that support here.
     *
     * Lease key hi/low is unique uuid
     *
     * The lease key in the vnode is used for dir leases or for the
     * shared FID
     */
    uuid_generate((uint8_t *) &snp->n_lease_key_hi); /* also fills in n_lease_key_low */

    snp->n_epoch = 0;    /* always starts at 0 */

    /*
     * Init a lease for the resource fork.
     * NOTE: the parent of a named stream is the file (ie np), but
     * smb2_lease_init() should detect its a named stream and handle it
     */
    smb2_lease_init(share, NULL, snp, 0, &snp->n_lease, 1);
    
    /* Init both durable handle for all files */
    smb2_dur_handle_init(share, 0, &snp->f_sharedFID_dur_handle, 1);
    for (i = 0; i < 3; i++) {
        smb2_dur_handle_init(share, 0, &snp->f_sharedFID_lockEntries[i].dur_handle, 1);
    }
    
    smb2_dur_handle_init(share, 0, &snp->f_lockFID_dur_handle, 1);

	/*
	 * If this is the resource stream then the parents resource fork size has 
	 * already been update. The calling routine aleady updated it. Remember that 
	 * the parent is currently locked. smbfs_attr_cacheenter can lock the parent 
	 * if we tell it to update the parent, so never tell it to update the parent 
	 * in this routine. 
	 */
	smbfs_attr_cacheenter(share, *svpp, fap, FALSE, NULL);

    lck_mtx_lock(&snp->n_flag_alloc_lock);
	CLR(snp->n_flag_alloc, NALLOC);
    if (ISSET(snp->n_flag_alloc, NWALLOC)) {
        CLR(snp->n_flag_alloc, NWALLOC);
        lck_mtx_unlock(&snp->n_flag_alloc_lock);
        wakeup(snp);
    } else {
        lck_mtx_unlock(&snp->n_flag_alloc_lock);
    }
	goto done;
	
errout:
#ifdef _NOT_YET_
    /* Note: Temporarily commenting this out, see <rdar://problem/10695860> */
	if (snp->n_flag & NREFPARENT) {
		if (vnode_get(vp) == 0) {
			vnode_rele(vp);
			vnode_put(vp);			
		}
		snp->n_flag &= ~NREFPARENT;
        
        /* Remove the child refcnt from the parent we just added above */
        OSDecrementAtomic(&np->n_child_refcnt);
	}
#endif
    
	smb_vhashrem(snp);
	
	if (locked == 1)
		smbnode_unlock(snp);	/* Release the smbnode lock */
	
	if (ISSET(snp->n_flag_alloc, NWALLOC))
		wakeup(snp);
	
    lck_rw_lock_exclusive(&snp->n_name_rwlock);
    if (snp->n_name != NULL) {
        SMB_FREE_DATA(snp->n_name, snp->n_name_allocsize);
    }

    if (snp->n_sname != NULL) {
        SMB_FREE_DATA(snp->n_sname, snp->n_sname_allocsize);
    }
    lck_rw_unlock_exclusive(&snp->n_name_rwlock);

    lck_rw_destroy(&snp->n_rwlock, smbfs_rwlock_group);
	lck_rw_destroy(&snp->n_name_rwlock, smbfs_rwlock_group);
	lck_rw_destroy(&snp->n_parent_rwlock, smbfs_rwlock_group);
    lck_mtx_destroy(&np->n_flag_alloc_lock, smbfs_mutex_group);
    lck_mtx_destroy(&np->n_strategy_mtx, smbfs_mutex_group);

    SMB_FREE_TYPE(struct smbnode, snp);

done:	
    SMB_FREE_DATA(cnp.cn_pnbuf, cnp.cn_pnlen);
	return error;
}

/* 
 * Update the nodes resource fork size if needed. 
 * NOTE: Remember the parent can lock the child while hold its lock, but the 
 * child cannot lock the parent unless the child is not holding its lock. So 
 * this routine is safe, because the parent is locking the child.
 *
 * The calling routine must hold a reference on the share
 *
 */
int 
smb_get_rsrcfrk_size(struct smb_share *share, vnode_t vp, vfs_context_t context)
{
	struct smbnode *np = VTOSMB(vp);
	uint64_t strmsize = 0;
	uint64_t strmsize_alloc = 0;
	time_t attrtimeo;
	struct timespec ts;
	int error = 0;
	time_t rfrk_cache_timer;
	struct timespec reqtime;
    uint32_t stream_flags = 0;
	int use_cached_data = 0;
	
    /* If we are in reconnect, use cached data if we have it */
    if (np->rfrk_cache_timer != 0) {
        use_cached_data = (share->ss_flags & SMBS_RECONNECTING);
    }

	nanouptime(&reqtime);

    /* Check to see if the cache has timed out */
    SMB_CACHE_TIME(ts, np, attrtimeo);
    
	lck_mtx_lock(&np->rfrkMetaLock);
	rfrk_cache_timer = ts.tv_sec - np->rfrk_cache_timer;
	lck_mtx_unlock(&np->rfrkMetaLock);
    
	if ((rfrk_cache_timer > attrtimeo) && !use_cached_data) {
        /* Cache has expired go get the resource fork size. */
		error = smbfs_smb_qstreaminfo(share, np, VREG,
                                      NULL, 0,
                                      SFM_RESOURCEFORK_NAME,
                                      NULL, NULL,
                                      &strmsize, &strmsize_alloc,
                                      &stream_flags, NULL,
                                      context);
        
        if ((error == ETIMEDOUT) && (np->rfrk_cache_timer != 0)) {
            /* Just return the cached data */
            error = 0;
            goto done;
        }
        
		/* 
		 * We got the resource stream size from the server, now update the resource
		 * stream if we have one. Search our hash table and see if we have a stream,
		 * if we find one then smbfs_find_vgetstrm will return it with a vnode_get
		 * and a smb node lock on it.
		 */
		if (error == 0) {
			struct smbmount *smp = VTOSMBFS(vp);
			vnode_t svpp = smbfs_find_vgetstrm(smp, np, SFM_RESOURCEFORK_NAME, 
											   share->ss_maxfilenamelen);

			if (svpp) {
				if (smbfs_update_size(VTOSMB(svpp), &reqtime, strmsize, NULL) == TRUE) {
					/* Remember the only attribute for a stream is its size */
					nanouptime(&ts);
					VTOSMB(svpp)->attribute_cache_timer = ts.tv_sec;
				}
				smbnode_unlock(VTOSMB(svpp));
				vnode_put(svpp);
			}
		}
        else {
			/* 
			 * Remember that smbfs_smb_qstreaminfo will update the resource forks 
			 * cache and size if it finds  the resource fork. We are handling the 
			 * negative cache timer here. If we get an error then there is no 
			 * resource fork so update the cache.
			 */
			lck_mtx_lock(&np->rfrkMetaLock);
			np->rfrk_size = 0;
			np->rfrk_alloc_size = 0;
			nanouptime(&ts);
			np->rfrk_cache_timer = ts.tv_sec;
			lck_mtx_unlock(&np->rfrkMetaLock);
		}
	}

done:
	return(error);
}

/*
 * Anytime the stream is updated we need to update the parent's meta data. In 
 * the resource fork case this means updating the resource size and the resource 
 * size cache timer. For other streams it just means clearing the meta data cache
 * timer. We can update the parent's resource stream size and resource cache timer
 * here because we don't need the parent locked in this case. We use a different 
 * lock when updating the parents resource size and resource cache timer. Since we 
 * cannot lock the parent node here just return the parent vnode so the calling
 * process can handle clearing the meta data cache timer.
 *
 * NOTE:	smbfs_vnop_pageout calls this routine wihout the node locked. It is
 *			not setting the size so this should be safe. If anyone edits this  
 *			routine they need to keep in mind that it can be entered without a lock.
 */
vnode_t 
smb_update_rsrc_and_getparent(vnode_t vp, int setsize)
{
	struct smbnode *np = VTOSMB(vp);
	vnode_t parent_vp = vnode_getparent(vp);
	struct timespec ts;

	/* If this is a resource then update the parents resource size and cache timer */
	if ((parent_vp) && (np->n_flag & N_ISRSRCFRK)) {
		lck_mtx_lock(&VTOSMB(parent_vp)->rfrkMetaLock);
		
		/* They want us to update the size */
		if (setsize) {
			VTOSMB(parent_vp)->rfrk_size = np->n_size;
            /* assume alloc size same as new size */
			VTOSMB(parent_vp)->rfrk_alloc_size = np->n_size;
			nanouptime(&ts);
			VTOSMB(parent_vp)->rfrk_cache_timer = ts.tv_sec;					
		} else if (VTOSMB(parent_vp)->rfrk_size != np->n_size) {
			/* Something changed just reset the cache timer */
			VTOSMB(parent_vp)->rfrk_cache_timer = 0;
		}
		lck_mtx_unlock(&VTOSMB(parent_vp)->rfrkMetaLock);
	}
	return(parent_vp);	
}

static int 
smb_gid_match(struct smbmount *smp, u_int64_t node_gid)
{
    u_int32_t ii;
	
	if (node_gid == smp->ntwrk_gid)
		return TRUE;
	
	for (ii=0; ii < smp->ntwrk_cnt_gid; ii++)
		if (node_gid == smp->ntwrk_gids[ii])
			return TRUE;
	return FALSE;
}

/*
 * Check to see if the user has the request access privileges on the node. 
 * Someday we may have a call to check the access across the network, but for 
 * now all we can do is check the posix mode bits. 
 *
 * NOTE: rq_mode should be one of the S_IRWXO modes.
 */
int 
smb_check_posix_access(vfs_context_t context, struct smbnode *np, 
					   mode_t rq_mode)
{
	kauth_cred_t cred = vfs_context_ucred(context);
	uid_t	user = kauth_cred_getuid (cred);
	int		inGroup = 0;
	
	kauth_cred_ismember_gid(cred, np->n_gid, &inGroup);
	if (user == np->n_uid) {
		if (np->n_mode & (rq_mode << 6))
			return TRUE;
	} else if (inGroup) {
		if (np->n_mode & (rq_mode << 3))
			return TRUE;
	} else {
		if (np->n_mode & rq_mode)
			return TRUE;
	}
	return FALSE;
}

void smb_get_uid_gid_mode(struct smb_share *share, struct smbmount *smp,
                          struct smbfattr *fap, uint32_t flags,
                          uid_t *uid, gid_t *gid, mode_t *mode,
                          uint32_t max_access)
{
    uint16_t temp_mode = 0;
    
	if (fap->fa_unix) {
        /* Only SMB 1 supports Unix Extensions */
		if ((fap->fa_valid_mask & FA_UNIX_MODES_VALID) != FA_UNIX_MODES_VALID) {
			/*
			 * The call made to get this information did not contain the uid,
			 * gid or posix modes. So just keep using the ones we have, unless
			 * we have uninitialize values, then use the default values.
			 */
			if (*uid == KAUTH_UID_NONE) {
				*uid = smp->sm_args.uid;
				if (flags & SMBFS_GET_UGM_IS_DIR) {
					*mode |= smp->sm_args.dir_mode;
				} else {
					*mode |= smp->sm_args.file_mode;
				}
			}
            
			if (*gid == KAUTH_GID_NONE) {
				*gid = smp->sm_args.gid;
			} 			
		} else if ((smp->sm_args.altflags & SMBFS_MNT_TIME_MACHINE) ||
                   (SS_TO_SESSION(share)->session_misc_flags & SMBV_MNT_HIGH_FIDELITY)) {
			/* Remove any existing modes. */
			*mode &= ~ACCESSPERMS;
            
			/* Just return what was passed into us */
			*uid = smp->sm_args.uid;
			*gid = smp->sm_args.gid;
			*mode |= (mode_t)(fap->fa_permissions & ACCESSPERMS);
		} else if (share->ss_attributes & FILE_PERSISTENT_ACLS) {
			/* Remove any existing modes. */
			*mode &= ~ACCESSPERMS;
            
			/* 
			 * The server supports the uid and gid and posix modes, so use the
			 * ones returned in the lookup call. If mapping then used the mounted
			 * users.
			 */
			if ((smp->sm_flags & MNT_MAPS_NETWORK_LOCAL_USER) && 
				(smp->ntwrk_uid == fap->fa_uid)) {
				*uid = smp->sm_args.uid;
				*gid = smp->sm_args.gid;
			}
            else {
				*uid = (uid_t)fap->fa_uid;
				*gid = (gid_t)fap->fa_gid;
			}
			*mode |= (mode_t)(fap->fa_permissions & ACCESSPERMS);
		} else if ((fap->fa_permissions & ACCESSPERMS) &&
				   (smp->sm_args.uid == (uid_t)smp->ntwrk_uid) &&
				   (smp->sm_args.gid == (gid_t)smp->ntwrk_gid)) {
			/* Remove any existing modes. */
			*mode &= ~ACCESSPERMS;
            
			/* 
			 * The server gave us POSIX modes and the local user matches the network 
			 * user, so assume they are in the same directory name space. 
			 */
			*uid = (uid_t)fap->fa_uid;
			*gid = (gid_t)fap->fa_gid;
			*mode |= (mode_t)(fap->fa_permissions & ACCESSPERMS);
		}
        else {
			int uid_match = (fap->fa_uid == smp->ntwrk_uid);
			int gid_match = smb_gid_match(smp, fap->fa_gid);
			
			/* Remove any existing modes. */
			*mode &= ~ACCESSPERMS;
            
			*uid = smp->sm_args.uid;
			*gid = smp->sm_args.gid;
            
			/* 
			 * We have no idea let the server handle any access issues. This 
			 * is safe because we only allow root and the user that mount the
			 * volume to have access to this mount point
			 */
			if ((fap->fa_permissions & ACCESSPERMS) == 0)
				fap->fa_permissions = ACCESSPERMS;
			if (!uid_match && !gid_match) {
				/* Use other perms */
				*mode |= (mode_t)(fap->fa_permissions & S_IRWXO);
                
				/* use other for group */
				*mode |= (mode_t)((fap->fa_permissions & S_IRWXO) << 3);
                
				/* use other for owner */
				*mode |= (mode_t)((fap->fa_permissions & S_IRWXO) << 6);
			} else if (!uid_match && gid_match) {
				/* Use group and other perms  */
				*mode |= (mode_t)(fap->fa_permissions & (S_IRWXG | S_IRWXO));
                
				/* use group for owner */
				*mode |= (mode_t)((fap->fa_permissions & S_IRWXG) <<  3);
			} else if (uid_match && !gid_match) {
				/* Use owner and other perms */
				*mode |= (mode_t)(fap->fa_permissions & (S_IRWXU | S_IRWXO));
                
				/* use other for group */
				*mode |= (mode_t)((fap->fa_permissions & S_IRWXO) << 3);
			} else {
				/* Use owner, group and other perms */
				*mode |= (mode_t)(fap->fa_permissions & ACCESSPERMS);
			}
            
            /*
             * Check with max access to see if the owner privileges should 
             * have more access.
             */
            if (max_access & SMB2_FILE_READ_DATA) {
                *mode |= S_IRUSR;
            }
            
            if (max_access & SMB2_FILE_WRITE_DATA) {
                *mode |= S_IWUSR;
            }
            
            if (max_access & SMB2_FILE_EXECUTE) {
                *mode |= S_IXUSR;
            }
		}		
	}
    else {
        /*
         * See comments in smbfs_nget about n_uid and n_gid and 
         * KAUTH_UID_NONE/KAUTH_GID_NONE default values.
         */
        if ((*uid == KAUTH_UID_NONE) || (*gid == KAUTH_GID_NONE)) {
            /*
             * Either ACLs are off or no ACL retrieved for this item.
             * Return the mounting user uid/gid
             */
            *uid = smp->sm_args.uid;
            *gid = smp->sm_args.gid;
        }
        else {
            /*
             * uid/gid must have been set by a previous Get ACL, so just return
             * their current value.
             */
        }
        
        /* Figure out the mode */
        if (fap->fa_valid_mask & FA_UNIX_MODES_VALID) {
            /* 
             * Server gave us Posix modes via AAPL ReadDirAttr extension
             */
            
            /* Remove any existing modes. */
            *mode &= ~ACCESSPERMS;
            
            temp_mode = fap->fa_permissions;
            *mode |= (temp_mode & ACCESSPERMS); /* only take access perms */
        }
        else {
            if (flags & SMBFS_GET_UGM_REMOVE_POSIX_MODES) {
                /* Remove any existing modes. */
                *mode &= ~ACCESSPERMS;
                /*
                 * The system just can't handle posix modes of zero. We now support
                 * maximal access, so just dummy up the posix modes so copies work
                 * when all you have is inherited ACLs.
                 */
                if (flags & SMBFS_GET_UGM_IS_DIR) {
                    *mode |= smp->sm_args.dir_mode;
                }
                else {
                    /* symlink or regular file */
                    *mode |= smp->sm_args.file_mode;
                }
            }
        }
	}
}

/*
 * Check to see if the immutable bit should be set on this node. 
 *
 * SMB_EFA_RDONLY ~ UF_IMMUTABLE
 *
 * We treat the SMB_EFA_RDONLY as the immutable flag. This allows
 * us to support the finder lock bit and makes us follow the 
 * MSDOS code model. See msdosfs project.
 *
 * NOTE: The ready-only flags does not exactly follow the lock/immutable bit.
 *
 * See Radar 5582956 for more details.
 *
 * When dealing with Window Servers the read-only bit for folder does not  
 * mean the samething as it does for files. Doing this translation was
 * confusing customers and really didn't work the way Mac users would expect.
 */
Boolean
node_isimmutable(struct smb_share *share, vnode_t vp, struct smbfattr *fap)
{
	Boolean unix_info2 = ((UNIX_CAPS(share) & UNIX_QFILEINFO_UNIX_INFO2_CAP)) ? TRUE : FALSE;
	Boolean darwin = (SS_TO_SESSION(share)->session_flags & SMBV_DARWIN) ? TRUE : FALSE;
    uint32_t is_dir = 0;
    uint32_t is_read_only = 0;
    
    if (vp != NULL) {
        if (vnode_isdir(vp)) {
            is_dir = 1;
        }
        
        if (VTOSMB(vp)->n_dosattr & SMB_EFA_RDONLY) {
            is_read_only = 1;
        }
    }
    else {
        if (fap != NULL) {
            /* smbfs_vnop_readdirattr or smbfs_vnop_getattrlistbulk */
            if (fap->fa_vtype == VDIR) {
                is_dir = 1;
            }
            
            if (fap->fa_attr & SMB_EFA_RDONLY) {
                is_read_only = 1;
            }
        }
        else {
            /* this should be impossible */
            SMBERROR("vp and fap are NULL \n");
        }
    }
	
    if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
        if ((UNIX_SERVER(SS_TO_SESSION(share)) || !is_dir) && is_read_only) {
            return TRUE;
        }
    }
	else {
        if ((unix_info2 || darwin || !is_dir) && is_read_only) {
            return TRUE;
        }
    }
	return FALSE;
}

/*
 * routines to maintain vnode attributes cache
 *
 * The calling routine must hold a reference on the share
 *
 */
void 
smbfs_attr_cacheenter(struct smb_share *share, vnode_t vp, struct smbfattr *fap, 
					  int UpdateResourceParent, vfs_context_t context)
{
	struct smbmount *smp = VTOSMBFS(vp);
	struct smbnode *np = VTOSMB(vp);
	enum vtype node_vtype;
	struct timespec ts;
	uint32_t monitorHint = 0;
    uint32_t flags = 0;
    int error;
    SMBFID fid = 0;
    
    if ((fap->fa_reqtime.tv_sec == 0) && ((fap->fa_reqtime.tv_nsec == 0))) {
		SMBWARNING_LOCK(np, "fa_reqtime is 0 for <%s>?\n", np->n_name);
    }
    
	if (timespeccmp(&fap->fa_reqtime, &np->n_last_meta_set_time, <=)) {
		/* 
         * The meta data was fetched before a set attr so its possible stale 
         * and ok to ignore 
         */
		//SMBERROR_LOCK(np, "Old meta data, dont update <%s>\n", np->n_name);
		return;
	}
	
    node_vtype = vnode_vtype(vp);

	if ((node_vtype == VDIR) && np->d_needsUpdate) {
		monitorHint |= VNODE_EVENT_ATTRIB | VNODE_EVENT_WRITE;
		np->d_needsUpdate = FALSE;			
	}
	
	/* 
	 * The vtype of node has changed at this path location. Someone must have
     * either deleted or moved the original and then put a new item in this
     * same place with the same name. Remove this vnode from the name cache and
	 * our hash table. We set the cache timer to zero so this will cause
     * smbfs_attr_cachelookup() routine to return ENOENT.
     *
     * For now, we will be using the old vnode so we should update its meta
     * data to return, but the next access should end up creating a new vnode
     * to use.
	 */
	if (node_vtype_changed(share, vp, node_vtype, fap)) {
		np->attribute_cache_timer = 0;
		np->n_symlink_cache_timer = 0;
		cache_purge(vp);
		smb_vhashrem(np);

        if (smbfsIsCacheable(vp)) {
            /*
             * Previous file has been replaced, so no need to try to push out
             * any dirty data, just invalidate the UBC.
             */
            if (vnode_hasdirtyblks(vp)) {
                /*
                 * This should be impossible as we pushed out any dirty
                 * pages before the last close.
                 */
                SMBERROR_LOCK(np, "File <%s> changed while it was closed but still has dirty pages. Dropping the dirty pages. \n", np->n_name);
            }

            SMB_LOG_UBC_LOCK(np, "UBC_INVALIDATE on <%s> due to vnode vtype changed \n",
                             np->n_name);
            
            np->n_flag &= ~NNEEDS_UBC_INVALIDATE;
            ubc_msync (vp, 0, ubc_getsize(vp), NULL, UBC_INVALIDATE);
        }

		monitorHint |= VNODE_EVENT_RENAME | VNODE_EVENT_ATTRIB;

		/* go ahead and update the meta data and return it for this vnode */
	}

    /* Need to check to see if dataless file status has changed */
    if ((np->n_dosattr & SMB_EFA_REPARSE_POINT) != (fap->fa_attr & SMB_EFA_REPARSE_POINT)) {
        if ((np->n_reparse_tag == IO_REPARSE_TAG_STORAGE_SYNC) ||
            (fap->fa_reparse_tag == IO_REPARSE_TAG_STORAGE_SYNC)) {
            /*
             * If its a cacheable file, push any dirty data and then invalidate
             * the UBC
             */
            SMBWARNING_LOCK(np, "%s had dataless dosattr/reparse_tag of 0x%x/0x%x now its 0x%x/0x%x \n",
                            np->n_name,
                            np->n_dosattr, np->n_reparse_tag,
                            fap->fa_attr, fap->fa_reparse_tag);

            if (smbfsIsCacheable(vp)) {
                if ((np->f_lockFID_refcnt > 0) || (np->f_sharedFID_refcnt > 0)) {
                    /*
                     * File has to be open for the push dirty data to work.
                     * Although this will probably cause the file on the server
                     * to get materialized if its not already materialized.
                     */
                    SMB_LOG_UBC_LOCK(np, "UBC_PUSHDIRTY, UBC_INVALIDATE on <%s> due to dataless file changed \n",
                                     np->n_name);
                    
                    np->n_flag &= ~(NNEEDS_UBC_INVALIDATE | NNEEDS_UBC_PUSHDIRTY);
                    ubc_msync (vp, 0, ubc_getsize(vp), NULL,
                               UBC_PUSHDIRTY | UBC_SYNC | UBC_INVALIDATE);
                }
                else {
                    SMB_LOG_UBC_LOCK(np, "UBC_INVALIDATE on <%s> due to dataless file (not open) changed \n",
                                     np->n_name);
                    
                    np->n_flag &= ~NNEEDS_UBC_INVALIDATE;
                    ubc_msync (vp, 0, ubc_getsize(vp), NULL,
                               UBC_INVALIDATE);
                }
            }

            monitorHint |= VNODE_EVENT_RENAME | VNODE_EVENT_ATTRIB;
            /* Save the new reparse tag */
            np->n_reparse_tag = fap->fa_reparse_tag;
        }
    }

	/* No need to update the cache after close, we just got updated */
	np->n_flag &= ~NATTRCHANGED;
	if (node_vtype == VREG) {
		if (smbfs_update_size(np, &fap->fa_reqtime, fap->fa_size, fap) == FALSE) {
			/* We lost the race, assume we have the correct size */
			fap->fa_size = np->n_size;
			/* Force a lookup on close, make sure we have the correct size on close */
			np->n_flag |= NATTRCHANGED;
 		} else if (np->n_size != fap->fa_size) {
			/* We one the race and the size change, notify them about the change */
			monitorHint |= VNODE_EVENT_EXTEND | VNODE_EVENT_ATTRIB;
		}
	} else if (node_vtype == VDIR) {
		np->n_size = 16384; 	/* should be a better way ... */
		/* See if we need to clear the negative name cache */
		if ((np->n_flag & NNEGNCENTRIES) && 
			((share->ss_fstype == SMB_FS_FAT) || 
			 (timespeccmp(&fap->fa_mtime, &np->n_mtime, >)))) {
			np->n_flag &= ~NNEGNCENTRIES;
			cache_purge_negatives(vp);			
				
			/* Dont skip local change notify */
            np->d_changecnt++;
		}
		
		/*
		 * Don't allow mtime to go backwards.
		 * Yes this has its flaws.  Better ideas are welcome!
		 */
		if (timespeccmp(&fap->fa_mtime, &np->n_mtime, <))
			fap->fa_mtime = np->n_mtime;
	} else if (node_vtype != VLNK) {
		return;
	}
	/* The server told us the allocation size return what they told us */
	np->n_data_alloc = fap->fa_data_alloc;
	
	if (fap->fa_unix) {
		np->n_flags_mask = fap->fa_flags_mask;
		np->n_nlinks = fap->fa_nlinks;
    }
    
    /* Calculate the uid, gid, and mode */
    if (vnode_isdir(np->n_vnode)) {
        flags |= SMBFS_GET_UGM_IS_DIR;
    }
    
    /* 
     * Unix mode can come from an ACL ACE (which sets NHAS_POSIXMODES)
     * or from SMB 2 when the FA_UNIX_MODES_VALID is set. Only dummy up 
     * fake modes if we dont have the unix modes already.
     */
    if (!(np->n_flag & NHAS_POSIXMODES) &&
        !(fap->fa_valid_mask & FA_UNIX_MODES_VALID)) {
        flags |= SMBFS_GET_UGM_REMOVE_POSIX_MODES;
    }
    
    /* <17946353> See if max access grants more unix privs for SMB 1 */
    if (!(SS_TO_SESSION(share)->session_flags & SMBV_SMB2) &&
        !(fap->fa_valid_mask & FA_MAX_ACCESS_VALID)) {
        if (timespeccmp(&np->maxAccessRightChTime, &np->n_chtime, ==) &&
            (np->maxAccessRightChTime.tv_sec != 0)) {
            /* Max access is still valid so use the cached value */
            fap->fa_max_access = np->maxAccessRights;
        }
        else {
            /* No max access or cached expired, refetch it from server */
            error = smb1fs_smb_open_maxaccess(share, np,
                                              NULL, 0,
                                              &fid, &fap->fa_max_access,
                                              context);
            if (error) {
                /* Assume full access */
                fap->fa_max_access = SA_RIGHT_FILE_ALL_ACCESS | STD_RIGHT_ALL_ACCESS;
            }
            
            if (fid) {
                (void)smbfs_smb_close(share, fid, context);
            }
            
            if (vnode_isvroot(vp)) {
                /*
                 * Its the root folder, if no Execute, but share grants
                 * Execute then grant Execute to root folder
                 */
                if ((!(fap->fa_max_access & SMB2_FILE_EXECUTE)) &&
                    (share->maxAccessRights & SMB2_FILE_EXECUTE)) {
                    fap->fa_max_access |= SMB2_FILE_EXECUTE;
                }
                
                /*
                 * Its the root, if no ReadAttr, but share grants
                 * ReadAttr then grant ReadAttr to root
                 */
                if ((!(fap->fa_max_access & SMB2_FILE_READ_ATTRIBUTES)) &&
                    (share->maxAccessRights & SMB2_FILE_READ_ATTRIBUTES)) {
                    fap->fa_max_access |= SMB2_FILE_READ_ATTRIBUTES;
                }
            }
        }
        
        fap->fa_valid_mask |= FA_MAX_ACCESS_VALID;
    }
    
    smb_get_uid_gid_mode(share, smp,
                         fap, flags,
                         &np->n_uid, &np->n_gid, &np->n_mode,
                         fap->fa_max_access);
    
    if (fap->fa_valid_mask & FA_UNIX_MODES_VALID) {
        np->n_flag |= NHAS_POSIXMODES;
    }
    
    if ((monitorHint & VNODE_EVENT_ATTRIB) == 0) {
		if (!(timespeccmp(&np->n_crtime, &fap->fa_crtime, ==) ||
			 !(timespeccmp(&np->n_mtime, &fap->fa_mtime, ==))))
			monitorHint |= VNODE_EVENT_ATTRIB;
	}
	
	/* 
	 * We always set the fstatus time if its valid
	 * Never reset the fstatus if the following are true:
	 * 1. The modify time on the item hasn't changed.
	 * 2. We have already discovered that this item has no streams.
	 * 3. The fap information didn't come from an open call.
	 *
	 * NOTE: This needs to be done before we update the modify time.
	 */
	if (fap->fa_valid_mask & FA_FSTATUS_VALID) {
		/* This is a valid field use it */
		np->n_fstatus = fap->fa_fstatus;
	} else if (timespeccmp(&np->n_chtime, &fap->fa_chtime, !=)) {
		/* 
		 * Something change clear the fstatus field sine we can't trust it 
		 * NOTE: The above check needs to be done before we update the change time.
		 */
		np->n_fstatus = 0;
	}

    /* Update max access if its valid */
	if (fap->fa_valid_mask & FA_MAX_ACCESS_VALID) {
        np->maxAccessRights = fap->fa_max_access;
        np->maxAccessRightChTime = fap->fa_chtime;
    }
    
    /* Update Finder Info if its valid */
    if (fap->fa_valid_mask & FA_FINDERINFO_VALID) {
        bcopy((void *)fap->fa_finder_info, np->finfo,
              sizeof(np->finfo));

        /* Cache the finder info as long as its not a copy in progress */
        if (vnode_isreg(vp) && (bcmp(np->finfo, "brokMACS", 8) == 0)) {
            np->finfo_cache_timer = 0;
            SMBDEBUG("Don't cache finder info, we have a finder copy in progress\n");
        }
        else {
            nanouptime(&ts);
            np->finfo_cache_timer = ts.tv_sec;
        }
    }

    /*
	 * Not sure if this is still a problem. In the old days the finder did
	 * not like it when the create time of the root or directory was after
	 * the modify time. This can and will happen on FAT file systems. For
	 * now lets leave it alone and see what happens.
	 */
	np->n_crtime = fap->fa_crtime;
	np->n_chtime = fap->fa_chtime;
	np->n_atime = fap->fa_atime;
	np->n_mtime = fap->fa_mtime;

	/*
	 * This routine can be called by a Query Info, FindFirst or the NTCreateAndX
	 * routine. If the server doesn't support the UNIX extensions then the fa_unix
	 * field won't be set and fa_attr will contain the dos attributes. We map the
	 * hidden, read only and the archive bits to the hidden, immutable and 
	 * "not backed up" bits in the BSD flags.
	 *
	 * In the NTCreateAndX routine we check to see if the server supports the 
	 * UNIX extensions and we convert the fa_attr and fa_flags_mask to the correct
	 * values depending on the bits supported by the server. This allows us to
	 * always trust the values in the NTCreateAndX fap.
	 *
	 * Linux server do not support the UNIX Info2 BSD flags. This is a problem, 
	 * we still want to use the UNIX extensions, but we can't trust all the bits
	 * in the fa_attr field when they come from the Query Info or FindFirst 
	 * routine. So in this case ignore the hidden, read only and the archive bits
	 * in the fa_attr and just keep using the ones we have on the node. This means
	 * for Linux servers we only trust the bits that come from the NTCreateAndX or
	 * the bits we have set ourself. Remember we we lookup access with the NTCreateAndX
	 * so we have the latest info in that case.
	 */
	if (fap->fa_unix && ((fap->fa_flags_mask & EXT_REQUIRED_BY_MAC) != EXT_REQUIRED_BY_MAC)) {
		fap->fa_attr &= ~(SMB_EFA_RDONLY | SMB_EFA_HIDDEN | SMB_EFA_ARCHIVE);
		np->n_dosattr &= (SMB_EFA_RDONLY | SMB_EFA_HIDDEN | SMB_EFA_ARCHIVE);
		np->n_dosattr |= fap->fa_attr;
	}
    else {
		np->n_dosattr = fap->fa_attr;
	}

    if (!(SS_TO_SESSION(share)->session_misc_flags & SMBV_MNT_HIGH_FIDELITY)) {
        /* Only if not hifi, update the meta data cache timer */
        nanouptime(&ts);
        np->attribute_cache_timer = ts.tv_sec;
    }
	/*
	 * UpdateResourceParent says it is ok to update the parent if this is a 
	 * resource stream. So if this is a stream and its the resource stream then 
	 * update the parents resource fork size and cache timer. If we can't get the 
	 * parent then just get out, when the timer goes off the parent will just have 
	 * to make the wire call.
	 */
	if (UpdateResourceParent && (vnode_isnamedstream(vp)) && 
		(np->n_flag & N_ISRSRCFRK)) {
		vnode_t parent_vp = smb_update_rsrc_and_getparent(vp, (fap->fa_size) ? TRUE : FALSE);
		/* We no longer need the parent so release it. */
		if (parent_vp)  
			vnode_put(parent_vp);
	}
	
	if ((monitorHint != 0) && (vnode_ismonitored(vp)) && context) {
		struct vnode_attr vattr;
		
		vfs_get_notify_attributes(&vattr);
		smbfs_attr_cachelookup(share, vp, &vattr, context, TRUE);
		vnode_notify(vp, monitorHint, &vattr);
	}
}

/*
 * The calling routine must hold a reference on the share
 */
int 
smbfs_attr_cachelookup(struct smb_share *share, vnode_t vp, struct vnode_attr *va, 
					   vfs_context_t context, int useCacheDataOnly)
{
    struct smbnode *np = VTOSMB(vp);
    struct smbmount *smp = VTOSMBFS(vp);
    time_t attrtimeo;
    struct timespec ts;
    vnode_t par_vp = NULL;

    SMB_CACHE_TIME(ts, np, attrtimeo);

    if (useCacheDataOnly) {
        /* Use the current cache data only */
    } else if (np->n_flag & NMARKEDFORDLETE) {
        /*
         * The file is marked for deletion on close. We can no longer 
         * gain access using the path. All access must be done using
         * the fid. So just pretend that the cache will never expire 
         * for this item. 
         *
         * NOTE: Since it is marked for deletion no one else can access
         *       it, so the cache data should stay good through the life
         *       of the open file.
         */
    }
    else if ((ts.tv_sec - np->attribute_cache_timer) > attrtimeo) {
        return (ENOENT);
    }

    if (!va) {
        return (0);
    }
	
	/* 
	 * Let vfs attr packing code know that we support 64 bit for va_linkid, 
	 * va_fileid, and va_parentid
	 */
	va->va_vaflags |= VA_64BITOBJIDS;

	if (VATTR_IS_ACTIVE(va, va_rdev)) {
        VATTR_RETURN(va, va_rdev, 0);
    }
    
    if (VATTR_IS_ACTIVE(va, va_nlink)) {
        if ((UNIX_CAPS(share) & UNIX_QFILEINFO_UNIX_INFO2_CAP))
            VATTR_RETURN(va, va_nlink, np->n_nlinks);
        else
            VATTR_RETURN(va, va_nlink, 1);
    }
	
    /*
     * Looks like we need to handle total size in the streams case. The VFS layer 
     * always fill this in with the data fork size. Still not sure of this, but 
     * for now lets go ahead and handle if ask.
     */
    if ((share->ss_attributes & FILE_NAMED_STREAMS) &&
        (VATTR_IS_ACTIVE(va, va_total_size))) {
        if (vnode_isdir(vp)) {
            VATTR_RETURN(va, va_total_size, np->n_size);
            lck_mtx_lock(&smp->sm_statfslock);
            if (smp->sm_statfsbuf.f_bsize) {
                /* Just to be safe */
                VATTR_RETURN(va, va_total_alloc, roundup(va->va_total_size,
                                                         smp->sm_statfsbuf.f_bsize));
            }
            lck_mtx_unlock(&smp->sm_statfslock);
        }
        else if (!vnode_isnamedstream(vp)) {
            if (!useCacheDataOnly) {
                (void)smb_get_rsrcfrk_size(share, vp, context);
            }
            lck_mtx_lock(&np->rfrkMetaLock);

            if (smp->sm_statfsbuf.f_bsize) {
                /* Just to be safe */
                VATTR_RETURN(va, va_rsrc_alloc,
                             roundup(np->rfrk_alloc_size, smp->sm_statfsbuf.f_bsize));
            }

            VATTR_RETURN(va, va_total_size, np->n_size + np->rfrk_size);
            lck_mtx_unlock(&np->rfrkMetaLock);
            lck_mtx_lock(&smp->sm_statfslock);
            if (smp->sm_statfsbuf.f_bsize) {
                /* Just to be safe */
                VATTR_RETURN(va, va_total_alloc, roundup(va->va_total_size,
                                                         smp->sm_statfsbuf.f_bsize));
            }
            lck_mtx_unlock(&smp->sm_statfslock);
        }
    }
	
    if (VATTR_IS_ACTIVE(va, va_data_size)) {
        VATTR_RETURN(va, va_data_size, np->n_size);
    }
    
    if (VATTR_IS_ACTIVE(va, va_data_alloc)) {
        VATTR_RETURN(va, va_data_alloc, np->n_data_alloc);
    }
    
    if (VATTR_IS_ACTIVE(va, va_iosize)) {
        lck_mtx_lock(&smp->sm_statfslock);
        VATTR_RETURN(va, va_iosize, smp->sm_statfsbuf.f_bsize);
        lck_mtx_unlock(&smp->sm_statfslock);
    }
	
    if (VATTR_IS_ACTIVE(va, va_mode)) {
        VATTR_RETURN(va, va_mode, np->n_mode);
    }
		
    if (VATTR_IS_ACTIVE(va, va_uid) || VATTR_IS_ACTIVE(va, va_gid)) {
        /*
         * The volume was mounted as guest, so we already set the mount point to
         * ignore ownership. Now always return an owner of 99 and group of 99.
         */
        if (SMBV_HAS_GUEST_ACCESS(SS_TO_SESSION(share)) &&
			!(SS_TO_SESSION(share)->session_misc_flags & SMBV_MNT_TIME_MACHINE) &&
            !(SS_TO_SESSION(share)->session_misc_flags & SMBV_MNT_HIGH_FIDELITY)) {
			/*
			 * <28555880> If its Guest mounted and NOT a TM mount, and NOT a
             * HiFi mount, then return unknown uid/gid
			 */
            VATTR_RETURN(va, va_uid, UNKNOWNUID);
            VATTR_RETURN(va, va_gid, UNKNOWNGID);
        } else {
            /*
             * For servers that support the UNIX extensions we know the uid/gid.
             * For server that don't support ACLs then the node uid/gid will be
             * set to the mounted user's uid/gid. For all other servers we need
             * to get the ACL and translate the SID to a uid or gid. The uid/gid
             * really is for display purpose only and means nothing to us. We will
             * set the nodes ids if we get a request for the ACL, but otherwise
             * we leave them unset for performance reasons.
             */
            if (np->n_uid == KAUTH_UID_NONE)
                VATTR_RETURN(va, va_uid, smp->sm_args.uid);
            else
                VATTR_RETURN(va, va_uid, np->n_uid);
            if (np->n_gid == KAUTH_GID_NONE)
                VATTR_RETURN(va, va_gid, smp->sm_args.gid);			
            else
                VATTR_RETURN(va, va_gid, np->n_gid);			
        }
    }
    
    if (VATTR_IS_ACTIVE(va, va_flags)) {
        va->va_flags = 0;
        /*
         * Remember that SMB_EFA_ARCHIVE means the items needs to be
         * archive and SF_ARCHIVED means the item has been archive.
         *
         * NOTE: Windows does not set ATTR_ARCHIVE bit for directories.
         */
        if (!vnode_isdir(vp) && !(np->n_dosattr & SMB_EFA_ARCHIVE)) {
            va->va_flags |= SF_ARCHIVED;
        }
            
        /* The server has it marked as read-only set the immutable bit. */
        if (node_isimmutable(share, vp, NULL)) {
            va->va_flags |= UF_IMMUTABLE;
        }
        
        /*
         * The server has it marked as hidden, set the new UF_HIDDEN bit. Never
         * mark the root volume as hidden.
         */
        if ((np->n_dosattr & SMB_EFA_HIDDEN) && !vnode_isvroot(vp)) {
            va->va_flags |= UF_HIDDEN;
        }
        
        /*
         * Is the file currently dataless?
         */
        if ((np->n_dosattr & SMB_EFA_REPARSE_POINT) &&
            (np->n_reparse_tag == IO_REPARSE_TAG_STORAGE_SYNC)) {
            if (np->n_dosattr & SMB_EFA_RECALL_ON_DATA_ACCESS) {
                /*
                 * If M bit is set, then its a newer server and we know for
                 * sure that reading the file will recall it. Opening the file
                 * is fine.
                 */
                va->va_flags |= SF_DATALESS;
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
                if (np->n_dosattr & (SMB_EFA_OFFLINE | SMB_EFA_SPARSE)) {
                    va->va_flags |= SF_DATALESS;
                }
            }
        }
        
        VATTR_SET_SUPPORTED(va, va_flags);
    }
    
    /* va_acl are done in smbfs_getattr */
    
    if (VATTR_IS_ACTIVE(va, va_create_time)) {
        VATTR_RETURN(va, va_create_time, np->n_crtime);
    }
    
    if (VATTR_IS_ACTIVE(va, va_modify_time)) {
        VATTR_RETURN(va, va_modify_time, np->n_mtime);
    }
    
    if (VATTR_IS_ACTIVE(va, va_access_time)) {
        /* FAT only supports the date not the time! */
        VATTR_RETURN(va, va_access_time, np->n_atime);
    }
    
    if (VATTR_IS_ACTIVE(va, va_change_time)) {
        /*
         * FAT does not support change time, so just return the modify time. 
         * Copied from the msdos code. SMB has no backup time so skip the
         * va_backup_time.
         */
        if (share->ss_fstype == SMB_FS_FAT)
            np->n_chtime.tv_sec = np->n_mtime.tv_sec;
        
        VATTR_RETURN(va, va_change_time, np->n_chtime);
    }
    
    /*
     * Exporting file IDs from HFS Plus:
     *
     * For "normal" files the c_fileid is the same value as the
     * c_cnid.  But for hard link files, they are different - the
     * c_cnid belongs to the active directory entry (ie the link)
     * and the c_fileid is for the actual inode (ie the data file).
     *
     * The stat call (getattr) uses va_fileid and the Carbon APIs,
     * which are hardlink-ignorant, will ask for va_linkid.
     */
    if (VATTR_IS_ACTIVE(va, va_fileid) || VATTR_IS_ACTIVE(va, va_linkid)) {
        lck_rw_lock_shared(&np->n_name_rwlock);
        VATTR_RETURN(va, va_fileid, smb2fs_smb_file_id_get(smp, np->n_ino,
                                                           np->n_name));
        VATTR_RETURN(va, va_linkid, smb2fs_smb_file_id_get(smp, np->n_ino,
                                                           np->n_name));
        lck_rw_unlock_shared(&np->n_name_rwlock);
    }
    
    if (VATTR_IS_ACTIVE(va, va_parentid)) {
        par_vp = smbfs_smb_get_parent(np, kShareLock);
        if (par_vp != NULL) {
            lck_rw_lock_shared(&VTOSMB(par_vp)->n_name_rwlock);
            VATTR_RETURN(va, va_parentid, smb2fs_smb_file_id_get(smp,
                                                                 VTOSMB(par_vp)->n_ino,
                                                                 VTOSMB(par_vp)->n_name));
            lck_rw_unlock_shared(&VTOSMB(par_vp)->n_name_rwlock);
            
            vnode_put(par_vp);
        }
        else {
            /*
             * This would require a lot more work so let the VFS layer handle it.
             * VATTR_RETURN(va, va_parentid, np->n_parentid);
             */
            
            if (np->n_parent_vid != 0) {
                /* Parent got recycled already? */
                SMBWARNING_LOCK(np, "Missing parent for <%s> \n",
                                np->n_name);
            }
        }
    }
    
    if (VATTR_IS_ACTIVE(va, va_fsid)) {
        VATTR_RETURN(va, va_fsid, vfs_statfs(vnode_mount(vp))->f_fsid.val[0]);
    }
    
    if (VATTR_IS_ACTIVE(va, va_filerev)) {
        VATTR_RETURN(va, va_filerev, 0);
    }
    
    if (VATTR_IS_ACTIVE(va, va_gen)) {
        VATTR_RETURN(va, va_gen, 0);
    }
	
    /*
     * We currently have no way to know the va_encoding. The VFS layer fills it
     * in with kTextEncodingMacUnicode = 0x7E, so use the same value;
     */
    if (VATTR_IS_ACTIVE(va, va_encoding)) {
        VATTR_RETURN(va, va_encoding, 0x7E);
    }
	
    /*
     * If this is the root, let VFS find out the mount name, which may be 
     * different from the real name 
     */
    if (VATTR_IS_ACTIVE(va, va_name) && !vnode_isvroot(vp)) {
        lck_rw_lock_shared(&np->n_name_rwlock);
		strlcpy ((char*) va->va_name, (char*)np->n_name, MAXPATHLEN);
        lck_rw_unlock_shared(&np->n_name_rwlock);
        VATTR_SET_SUPPORTED(va, va_name);
    }
    /* va_uuuid is done in smbfs_getattr */
    /* va_guuid is done in smbfs_getattr */
    /* We have no  way to get va_nchildren. Let VFS layer handle it. */
    return (0);
}

/*
 * FAT file systems don't exhibit POSIX behaviour with regard to 
 * updating the directory mtime when the directory's contents 
 * change.
 *
 * We force the issue here by updating our cached copy of the mtime
 * whenever we perform such an action ourselves, and then mark the
 * cache invalid.  Subsequently when the invalidated cache entry is
 * updated, we disallow an update that would move the mtime backwards.
 *
 * This preserves correct or near-correct behaviour with a
 * compliant server, and gives near-correct behaviour with
 * a non-compliant server in the most common case (we are the
 * only client changing the directory).  
 *
 * There are also complications if a server's time is ahead
 * of our own.  We must 'touch' a directory when it is first
 * created, to ensure that the timestamp starts out sane,
 * however it may have a timestamp well ahead of the 'touch'
 * point which will be returned and cached the first time the
 * directory's attributes are fetched.  Subsequently, the
 * directory's mtime will not appear to us to change at all
 * until our local time catches up to the server.
 *
 * Thus, any time a directory is 'touched', the saved timestamp
 * must advance at least far enough forwards to be visible to
 * the stat(2) interface.
 *
 * We only do this for FAT file system, all others should be handling
 * the modify time correctly.
 */
void
smbfs_attr_touchdir(struct smbnode *dnp, int fatShare)
{
	if (fatShare) {
		struct timespec ts, ta;
		
		/*
		 * Creep the saved time forwards far enough that
		 * layers above the kernel will notice.
		 */
		ta.tv_sec = 1;
		ta.tv_nsec = 0;
		timespecadd(&dnp->n_mtime, &ta);
		/*
		 * If the current time is later than the updated
		 * saved time, apply it instead.
		 */
		nanotime(&ts);	/* Need current date/time, so use nanotime */
		if (timespeccmp(&dnp->n_mtime, &ts, <))
			dnp->n_mtime = ts;		
	}
	/*
	 * Invalidate the cache, so that we go to the wire
	 * to check that the server doesn't have a better
	 * timestamp next time we care.
	 */
	dnp->attribute_cache_timer = 0;
}

int 
smbfsIsCacheable(vnode_t vp)
{
	/* Has to be a file, so dirs and symlinks are not cacheable */
	if (!vnode_isreg(vp)) {
		return FALSE;
	}
    
    if (VTOSMB(vp)->f_hasBRLs == 1) {
        /*
         * If fsctl(brls)/vnop_ioctl() were done on this file, then we assume
         * that other processes or clients will also be doing byte range
         * locks on this file. Have to disable UBC caching so we dont trip
         * over trying to access bytes that are locked by someone else.
         *
         * flock()/vnop_adv_lock() is allowed to be cacheable because its
         * an exclusive lock on the entire file.
         */
        return FALSE;
    }
    
    /* UBC caching is switched on/off by file leasing code */
	if (vnode_isnocache(vp)) {
		return FALSE;
	}
    else {
		return TRUE;
	}
}

void
smbfs_setsize(vnode_t vp, off_t size)
{
	struct smbnode *np = VTOSMB(vp);

	/*
	 * n_size is used by smbfs_vnop_pageout so it must be
	 * changed before we call setsize
	 */
	np->n_size = size;
	ubc_setsize(vp, size);
	/*
	 * this lets us avoid a race with readdir which resulted in
	 * a stale n_size, which in the worst case yielded data corruption.
	 */
	nanouptime(&np->n_sizetime);
	/* Resetting the size, blow away statfs cache */
	VTOSMBFS(vp)->sm_statfstime = 0;
}

/*
 * If the file size hasn't change then really nothing to do here, get out but
 * let the calling routine know that they can update their cache timer. If we have
 * updated the size internally, while waiting on the response from the server,
 * then don't update the size and tell the calling routine not to update its
 * cache timers. Otherwise update our internal size and the ubc size. Also tell
 * the calling routine to update any cache timers.
 */
int 
smbfs_update_size(struct smbnode *np, struct timespec *reqtime,
                  u_quad_t new_size, struct smbfattr *fap)
{
    if (np->n_size == new_size) {
        /*
         * Check to see if the mod date has changed on the server indicating
         * that someone (maybe even us) has written to this file.
         *
         * If fap or np seconds are zero, then must be not set yet
         */
        if ((fap != NULL) &&
            (fap->fa_mtime.tv_sec != 0) &&
            (np->n_mtime.tv_sec != 0) &&
            (timespeccmp(&fap->fa_mtime, &np->n_mtime, !=))) {
            /*
             * The modification date on the server is different from our copy. This
             * can happen because of two reasons:
             * 1) We flushed the UBC (and updated the file-size), and now the server
             *    has a new modification date (fap->fa_mtime).
             * 2) Another SMB client wrote data to the same file on the server,
             *    and leases or deny modes are not in use.
             * So can we trust any of our data?
             *
             * A problem here is that if two computers have the file currently
             * open and one of them writes to the file, I dont know which
             * computer did the write which resulted in the modification date
             * changing. The computer that didn't do the write will correctly
             * invalidate their UBC. The computer that did the write will also
             * get their UBC invalidated which will lose all their current
             * cached data which will harm performance. We are choosing data in
             * sync with the server versus performance. Hopefully since UBC
             * is supposed to reduce the actual number of reads/writes, this
             * wont hurt performance too much.
             *
             * If we have the file open for O_EXLOCK or O_SHLOCK, then
             * we are the only ones that can write to the file so the mod
             * date change must be due to our write and thus we can skip
             * the msync.
             *
             * Note: File leases could work, except any attempt on the server
             * to open that file would break the lease which would then turn
             * off UBC caching for that file until its closed/reopened. This
             * seems like a worse solution.
             *
             */
            if ((np->f_lockFID_refcnt > 0) || (np->f_sharedFID_refcnt > 0)) {
                /* File has to be open for the msync to make sense */
                if (np->n_write_unsafe == false) {
                    /* Did the vnop call that got us here allows us to write data? */
                    SMB_LOG_UBC_LOCK(np, "UBC_PUSHDIRTY, UBC_INVALIDATE on <%s> due to vnode mod time changed (%ld:%ld vs %ld:%ld) \n",
                                     np->n_name,
                                     fap->fa_mtime.tv_sec,
                                     fap->fa_mtime.tv_nsec,
                                     np->n_mtime.tv_sec,
                                     np->n_mtime.tv_nsec);

                    np->n_flag &= ~(NNEEDS_UBC_INVALIDATE | NNEEDS_UBC_PUSHDIRTY);
                    ubc_msync (np->n_vnode, 0, ubc_getsize(np->n_vnode), NULL,
                               UBC_PUSHDIRTY | UBC_SYNC | UBC_INVALIDATE);
                }
                else {
                   /*
                    * We should not push out our dirty data if we are in
                    * a read operation (i.e. getattr()) (rdar://80278506).
                    */
                    SMB_LOG_UBC_LOCK(np, "Postpone UBC_INVALIDATE on <%s> due to vnode mod time changed (%ld:%ld vs %ld:%ld) \n",
                                     np->n_name,
                                     fap->fa_mtime.tv_sec,
                                     fap->fa_mtime.tv_nsec,
                                     np->n_mtime.tv_sec,
                                     np->n_mtime.tv_nsec);
                    
                    np->n_flag |= NNEEDS_UBC_INVALIDATE | NNEEDS_UBC_PUSHDIRTY;
                }
            }
        }

		return TRUE; /* Nothing to update here */
    }
	
	/* Only update the size if we don't have a set eof pending */
	if (np->n_flag & NNEEDS_EOF_SET) {
		return FALSE;
	}
	
	if (np->waitOnClusterWrite) {
		return FALSE;
	}
	
	if (timespeccmp(reqtime, &np->n_sizetime, <=)) {
		return FALSE; /* we lost the race, tell the calling routine */
	}
	
	/* 
	 * The file size on the server is different from our copy. So can we trust
	 * any of our data? We should push out dirty data and invalidate because
     * since the file size changed, its probably some other client that wrote
     * to the file. Last writer wins model.
	 */
    if ((np->f_lockFID_refcnt > 0) || (np->f_sharedFID_refcnt > 0)) {
        /* File has to be open for the msync to make sense */
        if (np->n_write_unsafe == false) {
            /* Did the vnop call that got us here allows us to write data? */
            SMB_LOG_UBC_LOCK(np, "UBC_PUSHDIRTY, UBC_INVALIDATE on <%s> due to file size changed \n",
                             np->n_name);

            np->n_flag &= ~(NNEEDS_UBC_INVALIDATE | NNEEDS_UBC_PUSHDIRTY);
            ubc_msync (np->n_vnode, 0, ubc_getsize(np->n_vnode), NULL,
                       UBC_PUSHDIRTY | UBC_SYNC | UBC_INVALIDATE);
        }
        else {
           /*
            * We better not push out our dirty data if we're in
            * a read operation (ie getattr()) (rdar://80278506). Our
            * data will get pushed later on.
            */
            SMB_LOG_UBC_LOCK(np, "Postpone UBC_PUSHDIRTY, UBC_INVALIDATE on <%s> due to file size changed \n",
                             np->n_name);
            
            np->n_flag |= NNEEDS_UBC_INVALIDATE | NNEEDS_UBC_PUSHDIRTY;
        }
    }

	smbfs_setsize(np->n_vnode, new_size);
	return TRUE;
}

int
smbfs_update_name_par(struct smb_share *share, vnode_t dvp, vnode_t vp,
                      struct timespec *reqtime,
                      const char *new_name, size_t name_len)
{
    char *new_name2, *old_name;
    size_t new_name2_allocsize = 0, old_name_allocsize = 0;
    struct componentname cnp;
    struct smbnode *np, *fdnp = NULL, *tdnp = NULL;
    vnode_t fdvp = NULL;
    uint32_t orig_flag = 0;
    int update_flags = 0;
    int exclusive_lock = 0;
    vnode_t par_vp = NULL;

    if ((vp == NULL) ||
        (dvp == NULL) ||
        (share == NULL) ||
        (reqtime == NULL) ||
        (new_name == NULL)) {
        /* Nothing to update */
        //SMBDEBUG("missing info \n");
        return TRUE;
    }
    
    np = VTOSMB(vp);
    
    /* 
     * Did the parent change? 
     *
     * fdnp = np's parent
     * fdvp = np's parent->n_vnode (not locked)
     *
     * tdnp = VTOSMB(dvp)
     * tdvp = dvp (locked)
     *
     * fnp = np (vp is locked)
     */
    lck_rw_lock_shared(&np->n_parent_rwlock);   /* do our own locking */

    par_vp = smbfs_smb_get_parent(np, 0); /* do our own locking */
    if (par_vp != NULL) {
        fdnp = VTOSMB(par_vp);
        fdvp = par_vp;
    }
    else {
        /* Assume that the parent did not change */
        
        if (np->n_parent_vid != 0) {
            /* Parent got recycled already? */
            SMBWARNING_LOCK(np, "Missing parent for <%s> \n",
                            np->n_name);
        }
    }

    /* Already checked earlier for dvp == null */
    tdnp = VTOSMB(dvp);
    
    if ((fdnp != NULL) &&
        (fdvp != NULL) &&
        (tdnp != NULL) &&
        (fdnp != tdnp)) {
        /*
         * Parent changed, so need exclusive lock. Try to upgrade lock.
         * If exclusive lock upgrade fails we lose the lock and
         * have to take the exclusive lock on our own.
         */
        if (lck_rw_lock_shared_to_exclusive(&np->n_parent_rwlock) == FALSE) {
            lck_rw_lock_exclusive(&np->n_parent_rwlock);
            
            /*
             * Its remotely possible from parent changed as we were getting the
             * exclusive lock, so reset fdnp and fdvp
             */
            if (par_vp != NULL) {
                vnode_put(par_vp);
                par_vp = NULL;
            }
            fdnp = NULL;
            fdvp = NULL;
            
            par_vp = smbfs_smb_get_parent(np, 0); /* do our own locking */
            if (par_vp != NULL) {
                fdnp = VTOSMB(par_vp);
                fdvp = par_vp;
            }
            else {
                if (np->n_parent_vid != 0) {
                    /* Parent got recycled already? */
                    SMBWARNING_LOCK(np, "Missing parent for <%s> \n",
                                    np->n_name);
                }
            }

            /* Make sure fdnp and fdvp are still ok */
            if ((fdnp == NULL) || (fdvp == NULL)) {
                /* 
                 * The parent disappeared. This should not happen. 
                 * Just leave the vnode unchanged.
                 */
                SMBERROR_LOCK(np, "Parent lost during update for <%s> \n",
                              np->n_name);
                exclusive_lock = 1;
                goto error;
            }
        }
        exclusive_lock = 1;
        
        orig_flag = np->n_flag;

        /* Take a ref count on the new parent */
        if (!vnode_isvroot(dvp)) {
            if (vnode_ref(dvp) == 0) {
                np->n_flag |= NREFPARENT;
                
                /* Increment new parent node's child refcnt */
                OSIncrementAtomic(&tdnp->n_child_refcnt);
            }
            else {
                /* Failed to take ref, so clear flag */
                np->n_flag &= ~NREFPARENT;
            }
        }
        else {
            /* Do not need to ref cnt if parent is root vnode */
            np->n_flag &= ~NREFPARENT;
        }
        
        /* 
         * Remove the ref count off the old parent if there was one and
         * if the old parent was not root vnode
         */
        if ((!vnode_isvroot(fdvp)) && (orig_flag & NREFPARENT)) {
            if (vnode_get(fdvp) == 0) {
                vnode_rele(fdvp);
                vnode_put(fdvp);
                
                /* Remove the child refcnt from old parent */
                OSDecrementAtomic(&fdnp->n_child_refcnt);
            }
        }
        
        /* Set the new parent */
        np->n_parent_vnode = dvp;
        np->n_parent_vid = vnode_vid(dvp);
        
        /* Mark that we need to update the vnodes parent */
        update_flags |= VNODE_UPDATE_PARENT;
    }

error:
    if (exclusive_lock == 0) {
        /* Most of the time we should end up with just a shared lock */
        lck_rw_unlock_shared(&np->n_parent_rwlock);
    }
    else {
        /* Parent must have changed */
        lck_rw_unlock_exclusive(&np->n_parent_rwlock);
    }

    if (par_vp != NULL) {
        vnode_put(par_vp);
        par_vp = NULL;
    }

    /*
     * Did the name change?
     */
    lck_rw_lock_shared(&np->n_name_rwlock);
    if ((np->n_nmlen == name_len) &&
        (bcmp(np->n_name, new_name, np->n_nmlen) == 0)) {
        /* Name did not change, so nothing to update */
        
        /* Update parent if needed */
        if (update_flags != 0) {
            vnode_update_identity(vp, dvp, np->n_name, (int) np->n_nmlen, 0,
                                  update_flags);
        }
        
        lck_rw_unlock_shared(&np->n_name_rwlock);
        return TRUE;
    }
    lck_rw_unlock_shared(&np->n_name_rwlock);
    
    /*
     * n_rename_time is used to handle the case where an Enumerate req is sent,
     * then a Rename request/reply happens, then the Enumerate reply is 
     * processed which has the previous name. We dont want to update the name 
     * with a stale name from an Enumerate that happened before the Rename.
     */
	if (timespeccmp(reqtime, &np->n_rename_time, <=)) {
        /* we lost the race, tell the calling routine */

        /* Update parent if needed */
        if (update_flags != 0) {
            lck_rw_lock_shared(&np->n_name_rwlock);
            vnode_update_identity(vp, dvp, np->n_name, (int) np->n_nmlen, 0,
                                  update_flags);
            lck_rw_unlock_shared(&np->n_name_rwlock);
        }
        
        return FALSE;
	}

    /* Set the new name */
    new_name2 = smb_strndup(new_name, name_len, &new_name2_allocsize);
    if (new_name2) {
        /* save the old name */
        lck_rw_lock_exclusive(&np->n_name_rwlock);
        old_name = np->n_name;
        old_name_allocsize = np->n_name_allocsize;
        
        /* put in the new name */
        np->n_name = new_name2;
        np->n_nmlen = name_len;
        np->n_name_allocsize = new_name2_allocsize;
        
        /* Now its safe to free the old name */
        SMB_FREE_DATA(old_name, old_name_allocsize);

        /* Update the VFS name cache */
        bzero(&cnp, sizeof(cnp));
        cnp.cn_nameptr = (char *)np->n_name;
        cnp.cn_namelen = (int) np->n_nmlen;
        cnp.cn_flags = MAKEENTRY;
        
        /* Remove old entry, wrong case */
        cache_purge(vp);
        
        /* Add new entry, correct case */
        cache_enter(dvp, vp, &cnp);
        lck_rw_unlock_exclusive(&np->n_name_rwlock);
       
        update_flags |= VNODE_UPDATE_NAME;
    }

    /* Update parent and/or name if needed */
    if (update_flags != 0) {
        lck_rw_lock_shared(&np->n_name_rwlock);
        vnode_update_identity(vp, dvp, np->n_name, (int) np->n_nmlen, 0,
                              update_flags);
        lck_rw_unlock_shared(&np->n_name_rwlock);
    }

	return TRUE;
}

/*
 * Recursive function to find all child dirs under a starting parent and
 * close them to cause the Change Notify to be cancelled. If any child files
 * have a pending deferred close, do the close them now.
 * This allows the parent folder to be renamed successfully.
 */
void
smbfs_CloseChildren(struct smb_share *share,
                    struct smbnode *parent,
                    u_int32_t need_lock,
                    struct rename_lock_list **rename_child_listp,
                    vfs_context_t context)
{
    struct smbnode *np;
    uint32_t ii;
    struct smbmount *smp = NULL;
    vnode_t par_vp = NULL;
    struct rename_lock_list *rename_childp = NULL;
    int is_dir = 0;

    if ((share == NULL) ||
        (parent == NULL) ||
        (context == NULL)) {
        SMBERROR("share/parent/context is null \n");
        return;
    }
    
    smp = share->ss_mount;
    if (smp == NULL) {
        SMBERROR("smp is null \n");
        return;
    }
    
    if (need_lock == 1) {
        /* lock hash table before we walk it */
        smbfs_hash_lock(smp);
    }
    
    /* We have a hash table for each mount point */
    for (ii = 0; ii < (smp->sm_hashlen + 1); ii++) {
        if ((&smp->sm_hash[ii])->lh_first == NULL)
            continue;
        
        for (np = (&smp->sm_hash[ii])->lh_first; np; np = np->n_hash.le_next) {
            /*
             * Technically we should exclusive lock this as I am changing
             * something in the smb node, but then I would not be able to use
             * recursion. Shared lock *should* be fine to use.
             */
            lck_rw_lock_shared(&np->n_parent_rwlock); /* do our own locking */
            
            par_vp = smbfs_smb_get_parent(np, 0);   /* do our own locking */
            if ((par_vp == NULL) &&
                (np->n_parent_vid != 0)) {
                /* Parent got recycled already? */
                SMBWARNING_LOCK(np, "Missing parent for <%s> \n", np->n_name);
            }

            if ((par_vp != NULL) &&
                (VTOSMB(par_vp) == parent)) {
                /*
                 * If being alloc'd or in transition, then just
                 * skip this smb node.
                 */
                if ((ISSET(np->n_flag_alloc, NALLOC)) ||
                    (ISSET(np->n_flag_alloc, NTRANSIT))) {
                    lck_rw_unlock_shared(&np->n_parent_rwlock);
                    vnode_put(par_vp);
                    continue;
                }
                
                /* Is it a directory? */
                is_dir = smbfs_is_dir(np);

                if (!is_dir) {
                    /*
                     * Its not a dir, check to see if its a file that might
                     * have a pending deferred close. If so, then we need to
                     * close it now.
                     */
                    if (vnode_vtype(np->n_vnode) == VREG) {
                        CloseDeferredFileRefs(np->n_vnode, "smbfs_CloseChildren",
                                              0, context);
                    }
                    
                    /* Since its not a dir, we are done with it */
                    lck_rw_unlock_shared(&np->n_parent_rwlock);
                    vnode_put(par_vp);
                    continue;
                }

                /* SHOULD ONLY BE DIRS AT THIS POINT! */
                
                /*
                 * Someone is monitoring this item
                 */
                if (np->n_vnode && (vnode_ismonitored(np->n_vnode))) {
                    SMBDEBUG_LOCK(np, "%s is monitored and will be closed %llu\n",
                                  np->n_name, np->d_fid);
                    (void)smbfs_tmpclose(share, np, np->d_fid, context);
                    /* Mark it to be reopen */
                    np->d_needReopen = TRUE;
                    np->d_fid = 0;
                }
                
                /* Recurse and search for children of this dir now */
                smbfs_CloseChildren(share, np, 0, rename_child_listp, context);
                
                /* Add this item to list to be locked */
                SMB_MALLOC_TYPE(rename_childp, struct rename_lock_list, Z_WAITOK_ZERO);
                if (rename_childp == NULL) {
                    /* Is this even possible? */
                    SMBERROR("SMB_MALLOC_TYPE failed\n");
                }
                else {
                    rename_childp->np = np;
                    rename_childp->next = *rename_child_listp;
                    *rename_child_listp = rename_childp;
                }
            }
            
            if (par_vp != NULL) {
                vnode_put(par_vp);
            }

            lck_rw_unlock_shared(&np->n_parent_rwlock);
        }
    }
    
    if (need_lock == 1) {
        smbfs_hash_unlock(smp);
    }
}

/*
 * The share needs to be locked before calling this routine!
 *
 * Search the hash table looking for any open for write files or any files that
 * have dirty bits in UBC. If any are found, return EBUSY, else return 0.
 */
int32_t
smbfs_IObusy(struct smbmount *smp)
{
    struct smbnode *np;
    uint32_t ii;
    int is_dir = 0;

    /* lock hash table before we walk it */
    smbfs_hash_lock(smp);
    
    /* We have a hash table for each mount point */
    for (ii = 0; ii < (smp->sm_hashlen + 1); ii++) {
        if ((&smp->sm_hash[ii])->lh_first == NULL)
            continue;
        
        for (np = (&smp->sm_hash[ii])->lh_first; np; np = np->n_hash.le_next) {
            if (ISSET(np->n_flag_alloc, NALLOC))
                continue;
            
            if (ISSET(np->n_flag_alloc, NTRANSIT))
                continue;

            /* Is it a directory? */
            is_dir = smbfs_is_dir(np);

            /* Nothing else to with directories at this point */
            if (is_dir) {
                continue;
            }
            
            /* We only care about open files */
            if ((np->f_lockFID_refcnt == 0) && (np->f_sharedFID_refcnt == 0)) {
                continue;
            }
            
            if ((np->f_openTotalWCnt > 0) || (vnode_hasdirtyblks(SMBTOV(np)))) {
                /* Found one busy file so return EBUSY */
                smbfs_hash_unlock(smp);
                return EBUSY;
            }
        }
    }
    
    smbfs_hash_unlock(smp);
    
    /* No files open for write and no files with dirty UBC data */
    return 0;
}


#pragma mark - Reconnect

static void
smb1fs_reconnect(struct smbmount *smp)
{
    struct smbnode *np;
    uint32_t ii;
    int is_dir = 0;

    /* Get the hash lock */
    smbfs_hash_lock(smp);
    
    /* We have a hash table for each mount point */
    for (ii = 0; ii < (smp->sm_hashlen + 1); ii++) {
        if ((&smp->sm_hash[ii])->lh_first == NULL)
            continue;
        
        for (np = (&smp->sm_hash[ii])->lh_first; np; np = np->n_hash.le_next) {
            if (ISSET(np->n_flag_alloc, NALLOC))
                continue;
            
            if (ISSET(np->n_flag_alloc, NTRANSIT))
                continue;
			
            /* Is it a directory? */
            is_dir = smbfs_is_dir(np);

            /*
             * Someone is monitoring this item and we reconnected. Force a
             * notify update.
             */
            if (np->n_vnode && (vnode_ismonitored(np->n_vnode))) {
                SMBDEBUG_LOCK(np, "%s needs to be updated.\n", np->n_name);

                /* Do we need to reopen this item */
                if ((is_dir) && (np->d_fid != 0)) {
                    np->d_needReopen = TRUE;
                }
                
                /* Force a network lookup */
                np->attribute_cache_timer = 0;
                np->n_symlink_cache_timer = 0;
                np->d_needsUpdate = TRUE;
            }
            
            /* Nothing else to do with directories at this point */
            if (is_dir) {
                continue;
            }
            
            /* We only care about open files */
            if ((np->f_sharedFID_refcnt == 0) && (np->f_lockFID_refcnt == 0)) {
                continue;
            }
            
            /*
             * We have an open file mark it to be reopen.
             *
             * 1. Plain old POSIX open with no locks. Only revoke if reopen fails.
             * 2. POSIX open with a flock. Revoke if reopen fails. Otherwise
             *	  reestablish the lock. If the lock fails then mark it to be revoked.
             * 3. POSIX open with POSIX locks. (We do not support posix locks)
             * 4. Shared or Exclusive OpenDeny. We now revoke always.
             * 5. Mandatory Byte Range Locks. We now revoke always.
             */
            lck_mtx_lock(&np->f_openStateLock);
            
            /* Once it has been revoked it stays revoked */
            if (!(np->f_openState & kNeedRevoke)) {
                lck_mtx_lock(&np->f_sharedFID_BRL_lock);

                if ((np->f_lockFID_refcnt > 0) ||
                    (np->f_smbflock != NULL) ||
                    (np->f_sharedFID_lockEntries[0].lockList != NULL) ||
                    (np->f_sharedFID_lockEntries[1].lockList != NULL) ||
                    (np->f_sharedFID_lockEntries[2].lockList != NULL)) {
                    /*
                     * We always revoke opens that have mandatory
                     * locks, deny modes, or flock
                     */
                    np->f_openState |= kNeedRevoke;
                }
                else {
                    /* Reopen lazily later */
                    np->f_openState |= kNeedReopen;
                }
                
                lck_mtx_unlock(&np->f_sharedFID_BRL_lock);
            }
            
            lck_mtx_unlock(&np->f_openStateLock);
        }
    }
    
    smbfs_hash_unlock(smp);
}

int
smb2fs_reconnect_dur_handle(struct smb_share *share, vnode_t vp,
                            struct fileRefEntry *fileEntry, struct fileRefEntryBRL *fileBRLEntry)
{
    int error = 0, warning = 0;
    struct smbnode *np = NULL;
    struct smb2_durable_handle *dur_handlep = NULL;
    struct smb2_lease temp_lease = {0};
    int lease_need_free = 0;
    struct smb2_dur_hndl_and_lease dur_hndl_lease = {0};
    struct smbfattr *fap = NULL;
    struct smb_session *sessionp = NULL;
    uint32_t rights = 0;
    uint32_t shareMode = 0;
    uint16_t accessMode = 0;
    
    /* share MUST be locked on entry! */
    
    if ((fileEntry == NULL) && (fileBRLEntry == NULL)){
        SMBERROR("fileEntry and fileBRLEntry are both NULL \n");
        return(EINVAL);
    }

    if (vp == NULL) {
        SMBERROR("vp is null \n");
        return(EINVAL);
    }
    
    np = VTOSMB(vp);
    sessionp = SS_TO_SESSION(share);

    SMB_MALLOC_TYPE(fap, struct smbfattr, Z_WAITOK_ZERO);
    if (fap == NULL) {
        SMBERROR("SMB_MALLOC_TYPE failed\n");
        return(ENOMEM);
    }

    SMB_LOG_LEASING_LOCK(np, "Reconnect durable handle attempt on <%s> \n",
                         np->n_name);

    /*
     * For reconnect, we want to set up the Create with the same values that
     * were used in the original Create.
     */
    
    /* Get original Create's dur handle, rights and access mode */
    if (fileEntry != NULL) {
        dur_handlep = &fileEntry->dur_handle;
        rights = fileEntry->rights;
        accessMode = fileEntry->accessMode;
    }
    else {
        dur_handlep = &fileBRLEntry->dur_handle;
        rights = fileBRLEntry->rights;
        accessMode = fileBRLEntry->accessMode;
    }
    
    /*
     * Set up share modes from original create's access mode
     */
    shareMode = NTCREATEX_SHARE_ACCESS_ALL;     /* assume share all */
    if ((accessMode & kDenyRead) && (accessMode & kDenyWrite)) {
        /*
         * If denyRead and denyWrite, must be Exclusive (O_EXLOCK) mode, so
         * set to share none.
         */
        shareMode &= ~NTCREATEX_SHARE_ACCESS_ALL;
    }
    else {
        /*
         * If denyWrite is set, then must be Shared (O_SHLOCK) mode, so
         * remove the write share access
         */
        if (accessMode & kDenyWrite) {
            shareMode &= ~NTCREATEX_SHARE_ACCESS_WRITE;
        }
    }
    
    smbnode_dur_handle_lock(dur_handlep, smb2fs_reconnect_dur_handle);

    if (dur_handlep->flags & (SMB2_DURABLE_HANDLE_GRANTED | SMB2_PERSISTENT_HANDLE_GRANTED)) {
        if (dur_handlep->flags & SMB2_DURABLE_HANDLE_GRANTED) {
            dur_handlep->flags |= SMB2_DURABLE_HANDLE_RECONNECT;
            dur_handlep->flags &= ~(SMB2_DURABLE_HANDLE_GRANTED | SMB2_LEASE_GRANTED);
        }
        else {
            dur_handlep->flags |= SMB2_PERSISTENT_HANDLE_RECONNECT;
            dur_handlep->flags &= ~(SMB2_PERSISTENT_HANDLE_GRANTED | SMB2_LEASE_GRANTED);
        }
        
        /* Fill in lease state */
        smb2_lease_init(share, NULL, np, 0, &temp_lease, 1);
        temp_lease.req_lease_state = np->n_lease.req_lease_state;
        temp_lease.lease_state = np->n_lease.lease_state;
        dur_hndl_lease.leasep = &temp_lease;
        lease_need_free = 1;

        /* Use the passed in durable handle directly */
        dur_hndl_lease.dur_handlep = dur_handlep;

        /* lease should already be in the tables */

        smbnode_dur_handle_unlock(dur_handlep);

        /* Dont hold share lock over reconnect */
        lck_mtx_unlock(&share->ss_shlock);
        
        error = smbfs_smb_ntcreatex(share, np,
                                    rights, shareMode, VREG,
                                    &dur_handlep->fid, NULL, 0,
                                    FILE_OPEN, FALSE, fap,
                                    FALSE, &dur_hndl_lease,
                                    NULL, NULL,
                                    sessionp->session_iod->iod_context);
        
        /* Relock the share now */
        lck_mtx_lock(&share->ss_shlock);
        
        if (!error) {
            /*
             * If reconnect worked, update the lease
             */
            if (dur_hndl_lease.leasep != NULL) {
                smbnode_lease_lock(&temp_lease, smb2fs_reconnect_dur_handle);
                
                if (temp_lease.flags & SMB2_LEASE_GRANTED) {
                    /* Update the vnode lease from the temp lease */
                    warning = smbfs_add_update_lease(share, vp, &temp_lease,
                                                     SMBFS_LEASE_UPDATE | SMBFS_IN_RECONNECT, 1,
                                                     "ReconnectWorked");
                    if (warning) {
                        /* Shouldnt ever happen */
                        SMBERROR("smbfs_add_update_lease update failed %d\n", warning);
                    }
                }
                
                smbnode_lease_unlock(&temp_lease);
            }
            
            /* Update the fid */
            if (fileEntry != NULL) {
                fileEntry->fid = dur_handlep->fid;
            }
            else {
                fileBRLEntry->fid = dur_handlep->fid;
            }
        }
        else {
            SMBERROR_LOCK(np, "Warning: Could not reopen durable handle for %s \n",
                          np->n_name);
            SMB_LOG_LEASING_LOCK(np, "Lost file lease on <%s> due to reconnect \n",
                                   np->n_name);

            /* Try to remove the vnode lease */
            smbnode_lease_lock(&np->n_lease, smb2fs_reconnect_dur_handle);

            if (np->n_lease.flags != 0) {
                warning = smbfs_add_update_lease(share, vp, &np->n_lease,
                                                 SMBFS_LEASE_REMOVE | SMBFS_IN_RECONNECT, 1,
                                                 "ReconnectFailed");
                if (warning) {
                    /* Shouldnt ever happen */
                    SMBERROR_LOCK(np, "smbfs_add_update_lease remove failed <%d> on <%s>\n",
                                  warning, np->n_name);
                }
            }
            
            smbnode_lease_unlock(&np->n_lease);
        }
    }
    else {
        /*
         * No granted durable handle
         */
        smbnode_dur_handle_unlock(dur_handlep);

        SMBERROR_LOCK(np, "Missing durable handle on <%s> \n", np->n_name);
        error = ENOTSUP;
    }
    
    /* Free just the temp lease */
    if (lease_need_free) {
        smb2_lease_free(&temp_lease);
        //lease_need_free = 0;
    }

    if (fap != NULL) {
        SMB_FREE_TYPE(struct smbfattr, fap);
    }

    return(error);
}



int
smb2fs_reconnect(struct smbmount *smp)
{
    struct smbnode *np = NULL;
    uint32_t ii = 0;
    struct smbfattr *fap = NULL;
    struct smb_session *sessionp = NULL;
    int error = 0, reconnect_error = 0, i = 0, warning = 0;
    SMB2FID temp_fid = {0};
    uint32_t need_reopen = 0, done = 0;
    int32_t curr_credits = 0;
    int32_t verify_OSX = 0;
    int32_t verify_hifi = 0;
    int is_dir = 0;

    sessionp = SS_TO_SESSION(smp->sm_share);
    
    /*
     * Make sure we have at least 5 credits since we send out compound
     * requests below for checking AAPL and that requires at least 2 credits.
     * Later we need credits for reopening files and reestablishing locks.
     * Since we are in reconnect, the normal credit checking is disabled until
     * after reconnect finshes.
     *
     * Get the number of credits given to us by the Session Setup Response
     * which is the amount of credits we will start with after a reconnect
     * succeeds.
     */
    struct smbiod *iod = NULL;
    if (smb_iod_get_main_iod(sessionp, &iod, __FUNCTION__)) { // TBD: Do we need a for loop on all iods?
        SMBERROR("Invalid iod\n");
        return 0;
    }

    SMBC_CREDIT_LOCK(iod);
    curr_credits = OSAddAtomic(0, &iod->iod_credits_ss_granted);
    SMBC_CREDIT_UNLOCK(iod);
    if (curr_credits < 5) {
        SMBERROR("Insufficient credits <%d> after reconnect \n",
                 curr_credits);
        reconnect_error = ENOTCONN;
        goto exit;
    }
    
    /*
     * Validate the negotiate if SMB 2/3. smb2fs_smb_validate_neg_info will
     * check for whether the session needs to be validated or not.
     */
    if (!(smp->sm_args.altflags & SMBFS_MNT_VALIDATE_NEG_OFF)) {
        if ((sessionp->session_flags & SMBV_SMB2) &&
            !(sessionp->session_flags & SMBV_SMB311)) {
            /*
             * Use iod_context so we can tell this is from reconnect
             * Share was locked from smb_iod_reconnect, so have to unlock it
             * otherwise we can deadlock in iod code when the share lock is
             * attempted to be locked again.
             */
            lck_mtx_unlock(&smp->sm_share->ss_shlock);
            
            error = smb2fs_smb_validate_neg_info(smp->sm_share, sessionp->session_iod->iod_context);
            if (error) {
                SMBERROR("smb2fs_smb_validate_neg_info failed %d \n", error);
                lck_mtx_lock(&smp->sm_share->ss_shlock);
                reconnect_error = ENOTCONN;
                goto exit;
            }
            
            lck_mtx_lock(&smp->sm_share->ss_shlock);
        }
    }
    else {
        SMBWARNING("Validate Negotiate is off in preferences\n");
    }
    
    SMB_MALLOC_TYPE(fap, struct smbfattr, Z_WAITOK_ZERO);

    /* Clear OSX Server flag, but check that new server support is still OSX */
    if (sessionp->session_misc_flags & SMBV_OSX_SERVER) {
        verify_OSX = 1;
        SMBDEBUG("Clearing OS X server flags\n");
        sessionp->session_misc_flags &= ~(SMBV_OSX_SERVER | SMBV_OTHER_SERVER);
    }

    /*
     * Previous session was using high fidelity, check that new server also
     * supports high fidelity
     */
    if (sessionp->session_misc_flags & SMBV_MNT_HIGH_FIDELITY) {
        verify_hifi = 1;
    }

    /* Attempt to resend AAPL create context */
    if ((smp->sm_rvp != NULL) &&
        ((verify_OSX == 1) || (verify_hifi == 1))) {
        /*
         * Use iod_context so we can tell this is from reconnect
         * Share was locked from smb_iod_reconnect, so have to unlock it
         * otherwise we can deadlock in iod code when the share lock is
         * attempted to be locked again.
         */
        lck_mtx_unlock(&smp->sm_share->ss_shlock);

        /* Send a Create/Close */
        smb2fs_smb_cmpd_create(smp->sm_share, VTOSMB(smp->sm_rvp),
                               NULL, 0,
                               NULL, 0,
                               SMB2_FILE_READ_ATTRIBUTES | SMB2_SYNCHRONIZE, VDIR,
                               NTCREATEX_SHARE_ACCESS_ALL, FILE_OPEN,
                               SMB2_CREATE_AAPL_QUERY, NULL,
                               NULL, fap,
                               NULL, NULL,
                               NULL, sessionp->session_iod->iod_context);

        lck_mtx_lock(&smp->sm_share->ss_shlock);
    }

    /* Verify that SMBV_OSX_SERVER is again set else fail reconnect */
    if ((verify_OSX == 1) &&
        !(sessionp->session_misc_flags & SMBV_OSX_SERVER)) {
        SMBERROR("Prevously was OS X server, but not OS X server after reconnecting for %s volume\n",
                 (smp->sm_args.volume_name) ? smp->sm_args.volume_name : "");
        reconnect_error = ENOTCONN;
        goto exit;
    }

    /*
     * Verify that high fidelity is again set else fail reconnect
     * Make sure to match the checks in smbfs_mount()
     */
    if (verify_hifi == 1) {
        if (!(sessionp->session_flags & SMBV_SMB2) || !(sessionp->session_misc_flags & SMBV_OSX_SERVER) ||
            !(sessionp->session_server_caps & kAAPL_SUPPORTS_HIFI) ||
            !(smp->sm_share->ss_attributes & FILE_NAMED_STREAMS)) {
            SMBERROR("%s failed high fidelity check \n",
                     (smp->sm_args.volume_name) ? smp->sm_args.volume_name : "");
            reconnect_error = ENOTCONN;
            goto exit;
        }
    }

    /*
     * <13934847> We can not hold the hash lock while we reopen files as
     * we end up dead locked. First go through the entire list with the 
     * hash lock and just mark the vnodes that need to be reopened with the
     * kNeedReopen flag.
     */
    
    /* Get the hash lock */
    smbfs_hash_lock(smp);
    
    /* We have a hash table for each mount point */
    for (ii = 0; ii < (smp->sm_hashlen + 1); ii++) {
        if ((&smp->sm_hash[ii])->lh_first == NULL)
            continue;
        
        for (np = (&smp->sm_hash[ii])->lh_first; np; np = np->n_hash.le_next) {
            if (ISSET(np->n_flag_alloc, NALLOC))
                continue;
            
            if (ISSET(np->n_flag_alloc, NTRANSIT))
                continue;
			
            /* Is it a directory? */
            is_dir = smbfs_is_dir(np);

            /*
             * Someone is monitoring this item and we reconnected. Force a
             * notify update.
             */
            if (np->n_vnode && (vnode_ismonitored(np->n_vnode))) {
                SMBDEBUG_LOCK(np, "<%s> needs to be updated.\n", np->n_name);
                
                /* Do we need to reopen this item */
                if ((is_dir) && (np->d_fid != 0)) {
                    np->d_needReopen = TRUE;
                    
                    /* Remove the open fid from the fid table */
                    smb_fid_get_kernel_fid(smp->sm_share, np->d_fid,
                                           1, &temp_fid);
                }
                
                /* Force a network lookup */
                np->attribute_cache_timer = 0;
                np->n_symlink_cache_timer = 0;
                np->d_needsUpdate = TRUE;
            }

            if (is_dir) {
                if (np->d_fctx != NULL) {
                    /* Enumeration open dir is now closed, lazily reopen it */
                    np->d_fctx->f_need_close = FALSE;
                    
					/* Dir lease is also gone */
					smbnode_lease_lock(&np->n_lease, smb2fs_reconnect);
					
					if (np->n_lease.flags & SMB2_LEASE_GRANTED) {
						np->n_lease.flags |= SMB2_LEASE_BROKEN;
						np->n_lease.flags &= ~SMB2_LEASE_GRANTED;
                        /* Try to remove the dir vnode lease */
                        warning = smbfs_add_update_lease(smp->sm_share, np->n_vnode, &np->n_lease,
                                                         SMBFS_LEASE_REMOVE | SMBFS_IN_RECONNECT, 1,
                                                         "ReconnectDir");
                        if (warning) {
                            /* Shouldnt ever happen */
                            SMBERROR_LOCK(np, "smbfs_add_update_lease remove failed <%d> on <%s>\n",
                                          warning, np->n_name);
                        }

                        SMB_LOG_LEASING_LOCK(np, "Lost dir lease on <%s> due to reconnect \n",
											   np->n_name);
					}

                    smbnode_lease_unlock(&np->n_lease);

					/* Remove the open fid from the fid table */
                    smb_fid_get_kernel_fid(smp->sm_share,
                                           np->d_fctx->f_create_fid,
                                           1, &temp_fid);
                }
				
                if (np->d_rdir_fctx != NULL) {
                    /*
                     * readdir enumeration open dir is now closed,
                     * lazily reopen it. No dir lease to handle.
                     */
                    np->d_rdir_fctx->f_need_close = FALSE;

                    /* Remove the open fid from the fid table */
                    smb_fid_get_kernel_fid(smp->sm_share,
                                           np->d_rdir_fctx->f_create_fid,
                                           1, &temp_fid);
                }

                /*
				 * Since we do not use Index field in Query Dir, when we 
				 * reopen the dir for enumeration, its going to start at the
				 * first entry again. So, we need to skip over the first X
				 * number of entries until we get back to where we were.
				 *
				 * Do NOT bump d_changecnt as this messes up filling the meta
				 * data dir cache that happens to be in progress when the
				 * reconnect occurs.
				 */
                np->d_offset = 0;
                np->d_rdir_offset = 0;

				/* Nothing else to do with directories at this point */
                continue;
            }
            
            /*
             * Only files from here on
             */

            /* Is file currently open? */
            if ((np->f_lockFID_refcnt == 0) && (np->f_sharedFID_refcnt == 0)) {
                /* File not currently open, but is there a deferred close? */
                smbnode_lease_lock(&np->n_lease, CloseDeferredFileRefs);
                
                /* Do we have a deferred close? */
                if ((np->n_lease.flags & SMB2_LEASE_GRANTED) &&
                    (np->n_lease.lease_state & SMB2_LEASE_HANDLE_CACHING) &&
                    (np->n_lease.flags & SMB2_DEFERRED_CLOSE)) {
                    /*
                     * Deferred close found, may need to reopen this file,
                     * keep doing more checks to see if we can reopen.
                     */
                    SMB_LOG_LEASING_LOCK(np, "Deferred close that needs reopen on <%s> for reconnect \n",
                                           np->n_name);
                }
                else {
                    /* No deferred close, so done with this file */
                    smbnode_lease_unlock(&np->n_lease);

                    continue;
                }
             
                smbnode_lease_unlock(&np->n_lease);
            }

            /* Once it has been revoked it stays revoked */
            lck_mtx_lock(&np->f_openStateLock);
            if (np->f_openState & kNeedRevoke)	{
                lck_mtx_unlock(&np->f_openStateLock);
                continue;
            }
            else {
                /* Will try to reopen the files */
                np->f_openState |= kNeedReopen;
                
                /* Mark that at least one file needs to be reopened */
                need_reopen = 1;
            }
            lck_mtx_unlock(&np->f_openStateLock);
        } /* for np loop */
    } /* for ii loop */
    
    /* Free the hash lock */
    smbfs_hash_unlock(smp);
        
    if (need_reopen == 0) {
        /* No files need to be reopened, so leave */
        goto exit;
    }

    /*
     * <13934847> We can not hold the hash lock while we reopen files as
     * we end up dead locked. Now go through the list again holding the hash
     * lock and if a vnode needs to be reopened, drop the hash lock, clear the
     * kNeedReopen, attempt to reopen the vnode, then start at begining of
     * loop again until there are no more vnodes that need to be reopened.
     */
    done = 0;
    error = 0;
    while (done == 0) {
        /* Assume there are no files to be reopened */
        done = 1;
        
        /* Get the hash lock */
        smbfs_hash_lock(smp);

        /* We have a hash table for each mount point */
        for (ii = 0; ii < (smp->sm_hashlen + 1); ii++) {
            if ((&smp->sm_hash[ii])->lh_first == NULL)
                continue;
            
            for (np = (&smp->sm_hash[ii])->lh_first; np; np = np->n_hash.le_next) {
                if (ISSET(np->n_flag_alloc, NALLOC))
                    continue;
                
                if (ISSET(np->n_flag_alloc, NTRANSIT))
                    continue;
                
                /* Is it a directory? */
                is_dir = smbfs_is_dir(np);

                if (is_dir) {
                    continue;
                }
                
                /* Once it has been revoked it stays revoked */
                lck_mtx_lock(&np->f_openStateLock);
                
                if (np->f_openState & kNeedReopen) {
                    /*
                     * Need to reopen this file. Clear kNeedReopen state, this 
                     * way we know if a reconnect happened during reopen.  Set 
                     * kInReopen so smbfs_attr_cacheenter() will not be called.
                     */
                    np->f_openState &= ~kNeedReopen;
                    np->f_openState |= kInReopen;
                    lck_mtx_unlock(&np->f_openStateLock);
                }
                else {
                    /* This file does not need to be reopened */
                    lck_mtx_unlock(&np->f_openStateLock);
                    continue;
                }
                
                /* 
                 * Free the hash lock - this is why we have to redo the entire 
                 * while loop as the hash table may now change.
                 */
                done = 0;
                smbfs_hash_unlock(smp);

                /*
                 * For all network calls, use iod_context so we can tell this is
                 * from reconnect and thus it wont get blocked waiting for credits.
                 *
                 * Share was locked from smb_iod_reconnect, so have to
                 * unlock it otherwise we can deadlock in iod code when
                 * the share lock is attempted to be locked again.
                 */
                
                /*
                 * <74202808> If its a TM/SMB mount and we failed to
                 * reconnect a file, then no need to try to reconnect any more
                 * files, just clean up the remaining files and fail the
                 * overall reconnect by setting reconnect_error to be non zero.
                 *
                 * If its NOT a TM/SMB mount, if a file fails to reconnect,
                 * then revoke that file, but keep trying to reconnect as many
                 * files as we can. Failing to reconnect a file will NOT cause
                 * the overall reconnect to be failed, so reconnect_error will
                 * be set to zero.
                 */
                if (!(sessionp->session_misc_flags & SMBV_MNT_TIME_MACHINE)) {
                    /* It is NOT TM/SMB, try to reconnect next file */
                    error = 0;
                }
                
                /*
                 * Any previous error will cause us to skip attempting to
                 * reopen rest of the fids and just close the fids instead.
                 */
                if (error == 0) {
                    /*
                     * Check the lockFID_fid, cant use refcnt since a deferred
                     * close will have a refcnt of 0.
                     */
                    if (np->f_lockFID_fid != 0) {
                        error = smb2fs_reconnect_dur_handle(smp->sm_share, np->n_vnode,
                                                            &np->f_lockFID, NULL);
                      if (error) {
                          SMBERROR_LOCK(np, "smb2fs_reconnect_dur_handle failed <%d> for lockFID on <%s> \n",
                                          error, np->n_name);
                          
                          /*
                           * If ENOTSUP error due to dur handle not found,
                           * remap it to a more expected error to return
                           */
                          if (error == ENOTSUP) {
                              error = EBADF;
                          }
                          
                          if (sessionp->session_misc_flags & SMBV_MNT_TIME_MACHINE) {
                              /* It is TM/SMB, fail overall reconnect */
                              reconnect_error = EBADF;
                          }
                       }
                    }
                    
                    /*
                     * If no error, then check the sharedFID_fid next
                     */
                    if ((error == 0) && (np->f_sharedFID_fid != 0)) {
                        error = smb2fs_reconnect_dur_handle(smp->sm_share, np->n_vnode,
                                                            &np->f_sharedFID, NULL);
                        /*
                         * ONLY for the sharedFID and ONLY if there
                         * are no byte range locks, try a simple
                         * reopen.
                         */
                        if ((error == ENOTSUP) &&
                            (np->f_sharedFID_lockEntries[0].refcnt == 0) &&
                            (np->f_sharedFID_lockEntries[1].refcnt == 0) &&
                            (np->f_sharedFID_lockEntries[2].refcnt == 0)) {
                            /* Set flag indicating reopen sharedFID */
                            smbnode_dur_handle_lock(&np->f_sharedFID_dur_handle, smbfs_smb_ntcreatex);
                            np->f_sharedFID_dur_handle.flags |= SMB2_NONDURABLE_HANDLE_RECONNECT;
                            np->f_sharedFID_dur_handle.fid = np->f_sharedFID_fid;
                            smbnode_dur_handle_unlock(&np->f_sharedFID_dur_handle);
                            
                            lck_mtx_unlock(&smp->sm_share->ss_shlock);
                            SMB_LOG_LEASING_LOCK(np, "Reconnect reopen sharedFID attempt on <%s> \n",
                                                 np->n_name);
                            error = smbfs_smb_reopen_file(smp->sm_share, np,
                                                          sessionp->session_iod->iod_context);
                            /*
                             * smbfs_smb_reopen_file() sets the correct f_openState
                             * for us
                             */
                            lck_mtx_lock(&smp->sm_share->ss_shlock);
                            
                            /* Clear flag indicating reopen sharedFID */
                            smbnode_dur_handle_lock(&np->f_sharedFID_dur_handle, smbfs_smb_ntcreatex);
                            np->f_sharedFID_dur_handle.flags &= ~SMB2_NONDURABLE_HANDLE_RECONNECT;
                            smbnode_dur_handle_unlock(&np->f_sharedFID_dur_handle);
                        }

                        if (error) {
                            SMBERROR_LOCK(np, "smb2fs_reconnect_dur_handle failed <%d> for sharedFID on <%s> \n",
                                          error, np->n_name);
                            
                            /*
                             * If ENOTSUP error due to dur handle not found,
                             * remap it to a more expected error to return
                             */
                            if (error == ENOTSUP) {
                                error = EBADF;
                            }

                            if (sessionp->session_misc_flags & SMBV_MNT_TIME_MACHINE) {
                                /* It is TM/SMB, fail overall reconnect */
                                reconnect_error = EBADF;
                            }
                        }
                        
                        /*
                         * Check the sharedFID BRL FIDs. Ok to use refcnt here
                         */
                        for (i = 0; i < 3; i++) {
                            if ((error == 0) && (np->f_sharedFID_lockEntries[i].refcnt > 0)) {
                                error = smb2fs_reconnect_dur_handle(smp->sm_share, np->n_vnode,
                                                                    NULL, &np->f_sharedFID_lockEntries[i]);
                                if (error) {
                                    SMBERROR_LOCK(np, "smb2fs_reconnect_dur_handle failed <%d> for sharedFID BRL <%d> on <%s> \n",
                                                  error, i, np->n_name);
                                    
                                    if (sessionp->session_misc_flags & SMBV_MNT_TIME_MACHINE) {
                                        /* It is TM/SMB, fail overall reconnect */
                                        reconnect_error = EBADF;
                                    }
                                }
                            }
                        }
                    }
                } /* if (error == 0) */
                
                if (error) {
                    /* Try to remove the vnode lease */
                    smbnode_lease_lock(&np->n_lease, smb2fs_reconnect);

                    if (np->n_lease.flags != 0) {
                        warning = smbfs_add_update_lease(smp->sm_share, np->n_vnode, &np->n_lease,
                                                         SMBFS_LEASE_REMOVE | SMBFS_IN_RECONNECT, 1,
                                                         "ReconnectFailed2");
                        if (warning) {
                            /* Shouldnt ever happen */
                            SMBERROR_LOCK(np, "smbfs_add_update_lease remove failed <%d> on <%s>\n",
                                          warning, np->n_name);
                        }
                    }
                    
                    smbnode_lease_unlock(&np->n_lease);

                    /* Remove all the FIDs */
                    if (np->f_lockFID_refcnt > 0) {
                        smb_fid_get_kernel_fid(smp->sm_share, np->f_lockFID_fid,
                                               1, &temp_fid);
                        np->f_lockFID_accessMode = 0;
                        np->f_lockFID_fid = 0;
                        
                        /* clear the durable handle */
                        smbnode_dur_handle_lock(&np->f_lockFID_dur_handle, smb2fs_reconnect);
                        smb2_dur_handle_init(smp->sm_share, 0, &np->f_lockFID_dur_handle, 0);
                        smbnode_dur_handle_unlock(&np->f_lockFID_dur_handle);
                    }

                    if (np->f_sharedFID_refcnt > 0) {
                        smb_fid_get_kernel_fid(smp->sm_share, np->f_sharedFID_fid,
                                               1, &temp_fid);
                        SMB_LOG_LEASING_LOCK(np, "clearing f_sharedFID_accessMode on <%s> \n",
                                             np->n_name);

                        np->f_sharedFID_accessMode = 0;
                        np->f_sharedFID_fid = 0;

                        /* clear the durable handle */
                        smbnode_dur_handle_lock(&np->f_sharedFID_dur_handle, smb2fs_reconnect);
                        smb2_dur_handle_init(smp->sm_share, 0, &np->f_sharedFID_dur_handle, 0);
                        smbnode_dur_handle_unlock(&np->f_sharedFID_dur_handle);

                        /* Check the sharedFID BRL FIDs */
                        for (i = 0; i < 3; i++) {
                            if (np->f_sharedFID_lockEntries[i].refcnt > 0) {
                                smb_fid_get_kernel_fid(smp->sm_share, np->f_sharedFID_lockEntries[i].fid,
                                                       1, &temp_fid);
                                np->f_sharedFID_lockEntries[i].fid = 0;
                                
                                smbnode_dur_handle_lock(&np->f_sharedFID_lockEntries[i].dur_handle, smb2fs_reconnect);
                                smb2_dur_handle_init(smp->sm_share, 0, &np->f_sharedFID_lockEntries[i].dur_handle, 0);
                                smbnode_dur_handle_unlock(&np->f_sharedFID_lockEntries[i].dur_handle);
                            }
                        }
                    }

                    /* Mark this file as revoked */
                    lck_mtx_lock(&np->f_openStateLock);
                    np->f_openState &= ~kInReopen;
                    np->f_openState |= kNeedRevoke;
                    lck_mtx_unlock(&np->f_openStateLock);
                }
                else {
                    /* No error, so we can clear kInReopen */
                    lck_mtx_lock(&np->f_openStateLock);
                    np->f_openState &= ~kInReopen;
                    lck_mtx_unlock(&np->f_openStateLock);
                }
                
                
                /*
                 * All open files now have a durable handle, so either we
                 * restored all files, or we fail reconnect.
                 *
                 * No more lazy "reopen" of a file for SMB v2/3
                 */

                /* 
                 * Paranoid check - its possible that we get reconnected while
                 * we are trying to reopen and that would reset the kInReopen
                 * which could keep us looping forever. For now, we will only
                 * try once to reopen a file and thats it. May have to rethink
                 * this if it becomes a problem.
                 *
                 * I'm unsure if this can even happen any more...
                 */
                lck_mtx_lock(&np->f_openStateLock);

                if (np->f_openState & kNeedReopen) {
                    SMBERROR_LOCK(np, "Only one attempt to reopen %s \n", np->n_name);
                    np->f_openState &= ~kNeedReopen;
                    
                    /* Mark file to be revoked in smbfs_sync_callback() */
                    np->f_openState |= kNeedRevoke;
                }
                
                lck_mtx_unlock(&np->f_openStateLock);
                
                /* 
                 * Since we dropped the hash lock, have to start the while 
                 * loop again to search entire hash table from beginning.
                 */
                goto loop_again; /* skip out of np and ii loops */
                
            } /* for np loop */
        } /* for ii loop */
        
loop_again:
        if (done == 1) {
            /* if we get here, then must not have found any files to reopen */
            smbfs_hash_unlock(smp);
        }
    }
    
exit:
    if (fap) {
        SMB_FREE_TYPE(struct smbfattr, fap);
    }
    
    smb_iod_rel(iod, NULL, __FUNCTION__);
    
	return (reconnect_error);
}

/*
 * The share needs to be locked before calling this routine!
 *
 * Search the hash table looking for any open files. Remember we have a hash
 * table for every mount point. Not sure why but it makes this part easier.
 * Currently we do not support reopens, we just mark the file to be revoked.
 */
int
smbfs_reconnect(struct smbmount *smp)
{
   	struct smb_session *sessionp;
	int error = 0;
    
	SMB_ASSERT(smp != NULL, ("smp is null"));
    
    sessionp = SS_TO_SESSION(smp->sm_share);
	SMB_ASSERT(sessionp != NULL, ("sessionp is null"));

    if (sessionp->session_flags & SMBV_SMB2) {
        error = smb2fs_reconnect(smp);
    }
    else {
        smb1fs_reconnect(smp);
    }
	
	return (error);
}


#pragma mark - Durable Handles and Leasing

int
smbnode_dur_handle_lock(struct smb2_durable_handle *dur_handlep, void *last_op)
{
    if (dur_handlep == NULL) {
        SMBERROR("dur_handlep is null? \n");
        return(EINVAL);
    }
    
    lck_mtx_lock(&dur_handlep->lock);

    /* For Debugging... */
    dur_handlep->last_op = last_op;
    dur_handlep->activation = (void *) current_thread();

    return (0);
}

int
smbnode_dur_handle_unlock(struct smb2_durable_handle *dur_handlep)
{
    if (dur_handlep == NULL) {
        SMBERROR("leasep is null? \n");
        return(EINVAL);
    }
    
    /* For Debugging... */
    dur_handlep->last_op = NULL;
    dur_handlep->activation = NULL;

    lck_mtx_unlock(&dur_handlep->lock);
    return (0);
}

int
smbnode_lease_lock(struct smb2_lease *leasep, void *last_op)
{
    if (leasep == NULL) {
        SMBERROR("leasep is null? \n");
        return(EINVAL);
    }
    
    lck_mtx_lock(&leasep->lock);

    /* For Debugging... */
    leasep->last_op = last_op;
    leasep->activation = (void *) current_thread();

    return (0);
}

int
smbnode_lease_unlock(struct smb2_lease *leasep)
{
    if (leasep == NULL) {
        SMBERROR("leasep is null? \n");
        return(EINVAL);
    }
    
    /* For Debugging... */
    leasep->last_op = NULL;
    leasep->activation = NULL;

    lck_mtx_unlock(&leasep->lock);
    return (0);
}

void
CloseDeferredFileRefs(vnode_t vp, const char *reason, uint32_t check_time,
                      vfs_context_t context)
{
    struct smbnode *np;
    int error = 0, remove_fid = 0, warning = 0;
    struct smb_share *share = NULL;
    struct timespec ts;
    time_t def_close_timeo = k_def_close_timeout;
    struct smb2_close_rq close_parms = {0};
    SMBFID fid = 0;

    if (!vp) {
        SMBERROR("vp is null \n");
        return;
    }
    
    np = VTOSMB(vp);

    share = smb_get_share_with_reference(VTOSMBFS(vp));
    if (!share) {
        SMBERROR("share is null \n");
        return;
    }

    if (vnode_isdir(vp)) {
        SMBERROR("This is not a file <%s>??? \n", np->n_name);
        goto done;
    }

    if (!(SS_TO_SESSION(share)->session_sopt.sv_active_capabilities & SMB2_GLOBAL_CAP_LEASING)) {
        /* If server does not support file leasing, then nothing to do */
        goto done;
    }

    /*
     * For a file, close any deferred file handles (either lockFID or sharedFID)
     */
    
    smbnode_lease_lock(&np->n_lease, CloseDeferredFileRefs);
    
    /* Do we have a deferred close? */
    if ((np->n_lease.flags & SMB2_LEASE_GRANTED) &&
        (np->n_lease.lease_state & SMB2_LEASE_HANDLE_CACHING) &&
        (np->n_lease.flags & SMB2_DEFERRED_CLOSE)) {
        /*
         * Found a deferred close.
         * Do we need to check the defered close time?
         */
        if (check_time == 1) {
            /* Is it time to do this close? */
            nanouptime(&ts);
            
            if ((ts.tv_sec - np->n_lease.def_close_timer) > def_close_timeo) {
                SMB_LOG_LEASING_LOCK(np, "Expired file lease on <%s> due to <%s> \n",
                                       np->n_name, reason);
                remove_fid = 1;
            }
        }
        else {
            /* Just close it */
            SMB_LOG_LEASING_LOCK(np, "Lost file lease on <%s> due to <%s> \n",
                                   np->n_name, reason);
            
            remove_fid = 1;
        }
    }

    /*
     * Has the file been revoked and thus we cant reopen?
     * In this case, we would have a deferred close which would make
     * np->f_refcnt == 0, and dur_handlep->flags would have
     * SMB2_DEFERRED_CLOSE, SMB2_DURABLE_HANDLE_RECONNECT set.
     *
     * Note: On revoke, the FID should already have been removed from FID table
     */
    if ((np->n_lease.flags & SMB2_DEFERRED_CLOSE) &&
        (np->f_openState & kNeedRevoke)) {
        remove_fid = 1;
    }

    if (remove_fid != 0) {
        /* Update flags to indicate handle lease is gone */
        np->n_lease.flags |= SMB2_LEASE_BROKEN;
        np->n_lease.flags &= ~(SMB2_LEASE_GRANTED | SMB2_DEFERRED_CLOSE);
        OSAddAtomic(-1, &share->ss_curr_def_close_cnt);
        
        /* Clear some lease fields */
        np->n_lease.handle_reuse_cnt = 0;
        np->n_lease.def_close_timer = 0;

        /* Unlock before close attempt */
        smbnode_lease_unlock(&np->n_lease);
        
        /* Only attempt to close if its not revoked */
        if (!(np->f_openState & kNeedRevoke)) {
            /*
             * Which fid needs to be closed? Should only be one.
             * Dont need to flush file as it is a deferred close meaning the
             * flush happened on the vnop_close() earlier.
             */
            if (np->f_lockFID_fid != 0) {
                /*
                 * Must clear out f_lockFID_accessMode so that a
                 * pending deferred close on the sharedFID doesnt
                 * end up matching on the already closed lockFID.
                 */
                np->f_lockFID_accessMode = 0;
                fid = np->f_lockFID_fid;
                np->f_lockFID_fid = 0;
                
                /* clear the durable handle */
                smbnode_dur_handle_lock(&np->f_lockFID_dur_handle, CloseDeferredFileRefs);
                smb2_dur_handle_init(share, 0, &np->f_lockFID_dur_handle, 0);
                smbnode_dur_handle_unlock(&np->f_lockFID_dur_handle);
                
                /*
                 * MUST clear fields before attempting close as the
                 * close can take a while to complete and in the
                 * meanwhile, another open can occur.
                 */
                SMB_LOG_LEASING_LOCK(np, "Close lockFID on <%s> \n",
                                     np->n_name);
                error = smbfs_smb_close_attr(vp, share,
                                             fid, &close_parms,
                                             context);
                if (error) {
                    SMBWARNING("close file failed %d on lockFID %llx on <%s> \n",
                               error, np->f_lockFID_fid, np->n_name);
                }
            }
            else {
                if (np->f_sharedFID_fid != 0) {
                    /*
                     * Must clear out f_sharedFID_accessMode so that a
                     * pending deferred close on the lockFID doesnt
                     * end up matching on the already closed sharedFID.
                     */
                    np->f_sharedFID_accessMode = 0;
                    fid = np->f_sharedFID_fid;
                    np->f_sharedFID_fid = 0;
                    
                    /* clear the durable handle */
                    smbnode_dur_handle_lock(&np->f_sharedFID_dur_handle, CloseDeferredFileRefs);
                    smb2_dur_handle_init(share, 0, &np->f_sharedFID_dur_handle, 0);
                    smbnode_dur_handle_unlock(&np->f_sharedFID_dur_handle);

                    /*
                     * MUST clear fields before attempting close as the
                     * close can take a while to complete and in the
                     * meanwhile, another open can occur.
                     */
                    SMB_LOG_LEASING_LOCK(np, "Close sharedFID on <%s> \n",
                                         np->n_name);
                    error = smbfs_smb_close_attr(vp, share,
                                                 fid, &close_parms,
                                                 context);
                    if (error) {
                        SMBWARNING("close file failed %d on sharedFID fid <%llx> on <%s> \n",
                                   error, np->f_sharedFID_fid, np->n_name);
                    }
                    
                }
                else {
                    SMBERROR("No deferred close FID found for <%s>??? \n", np->n_name);
                    //error = EBADF;
                }
            }
        }
        
        /* Try to remove the vnode lease */
        smbnode_lease_lock(&np->n_lease, CloseDeferredFileRefs);

        if (np->n_lease.flags != 0) {
            warning = smbfs_add_update_lease(share, vp, &np->n_lease, SMBFS_LEASE_REMOVE, 1,
                                             reason);
            if (warning) {
                /* Shouldnt ever happen */
                SMBERROR_LOCK(np, "smbfs_add_update_lease remove failed <%d> on <%s>\n",
                              warning, np->n_name);
            }
        }
        
        smbnode_lease_unlock(&np->n_lease);
    }
    else {
        smbnode_lease_unlock(&np->n_lease);
    }

done:
    smb_share_rele(share, context);
}

int
smb2_dur_handle_init(struct smb_share *share, uint64_t flags,
                     struct smb2_durable_handle *dur_handlep,
                     int initialize)
{
    int error = ENOTSUP;
    struct smb_session *sessionp = NULL;
    
    if ((dur_handlep == NULL) || (share == NULL)) {
        /* Should never happen */
        SMBERROR("dur_handlep or share is null \n");
        return EINVAL;
    }

    sessionp = SS_TO_SESSION(share);
    if (sessionp == NULL) {
        /* Should never happen */
        SMBERROR("sessionp is null \n");
        return EINVAL;
    }

    if (initialize) {
        /* Always init lock so that we can always call dur handle free function */
        lck_mtx_init(&dur_handlep->lock, smbfs_mutex_group, smbfs_lock_attr);
    }

    dur_handlep->flags = 0;
    dur_handlep->fid = 0;
    dur_handlep->timeout = 0;
    dur_handlep->pad = 0;
    bzero(dur_handlep->create_guid, sizeof(dur_handlep->create_guid));

    /*
     * Only SMB 2/3 and servers that support leasing can do
     * reconnect.
     */
    if (SMBV_SMB21_OR_LATER(sessionp) &&
        (sessionp->session_sopt.sv_active_capabilities & SMB2_GLOBAL_CAP_LEASING) &&
        (flags & SMB2_DURABLE_HANDLE_REQUEST)) {
        /* CreateGuid */
        uuid_generate((uint8_t *) &dur_handlep->create_guid);

        dur_handlep->flags |= SMB2_DURABLE_HANDLE_REQUEST;

        /*
         * If server and share supports persistent handles, then request
         * that instead of just a plain durable V2 handle
         */
        if ((sessionp->session_sopt.sv_active_capabilities & SMB2_GLOBAL_CAP_PERSISTENT_HANDLES) &&
            (share->ss_share_caps & SMB2_SHARE_CAP_CONTINUOUS_AVAILABILITY)) {
            dur_handlep->flags |= SMB2_PERSISTENT_HANDLE_REQUEST;
        }
    }
    
    return error;
}

void
smb2_dur_handle_free(struct smb2_durable_handle *dur_handlep)
{
    if (dur_handlep == NULL) {
        /* Should never happen */
        SMBERROR("dur_handlep is null \n");
        return;
    }

    /* Zero out struct to be paranoid */
    dur_handlep->flags = 0;
    dur_handlep->fid = 0;
    dur_handlep->timeout = 0;
    dur_handlep->pad = 0;
    bzero(dur_handlep->create_guid, sizeof(dur_handlep->create_guid));

    lck_mtx_destroy(&dur_handlep->lock, smbfs_mutex_group);
}

int
smb2_lease_init(struct smb_share *share, struct smbnode *dnp, struct smbnode *np,
                uint64_t flags, struct smb2_lease *leasep,
                int initialize)
{
    int error = 0;
    struct smb_session *sessionp = NULL;
    vnode_t par_vp = NULL;
    vnode_t file_vp = NULL;
    UInt8 uuid[16] = {0};
    uint64_t *lease_keyp = NULL;
    
    /*
     * If np is null, then must be checking for durable handle support or
     * doing a vnop_create_open where we dont have a vnode yet. In these cases,
     * dnp must be non null
     */
    
    if ((leasep == NULL) || (share == NULL)) {
        /* Should never happen */
        SMBERROR("leasep or share is null \n");
        return EINVAL;
    }
    
    if ((np == NULL) && (dnp == NULL)) {
        /* Should never happen */
        SMBERROR("dnp and np are null \n");
        return EINVAL;
    }
    
    sessionp = SS_TO_SESSION(share);
    if (sessionp == NULL) {
        /* Should never happen */
        SMBERROR("sessionp is null \n");
        return EINVAL;
    }

    if (initialize) {
        /* Always init lock so that we can always call dur handle free function */
        lck_mtx_init(&leasep->lock, smbfs_mutex_group, smbfs_lock_attr);
    }

    leasep->flags = 0;
    leasep->req_lease_state = 0;
    leasep->lease_state = 0;
    leasep->lease_key_hi = 0;
    leasep->lease_key_low = 0;
    leasep->par_lease_key_hi = 0;
    leasep->par_lease_key_low = 0;
    leasep->epoch = 0;
    leasep->pad = 0;
    leasep->handle_reuse_cnt = 0;
    leasep->def_close_timer = 0;


    /*
     * Only SMB 2/3 and servers that support leasing can do
     * reconnect.
     */
    if (SMBV_SMB21_OR_LATER(sessionp) &&
        (sessionp->session_sopt.sv_active_capabilities & SMB2_GLOBAL_CAP_LEASING)) {
        /* ParentLeaseKey */
        if ((np != NULL) && (np->n_flag & N_ISSTREAM)) {
            /*
             * It is a named stream smbnode being passed in where the parent is
             * the file vnode. Need to get the parent dir of the file vnode.
             */
            
            /* Try to get the file vnode which is the parent of this smbnode */
            file_vp = smbfs_smb_get_parent(np, kShareLock);
            if (file_vp != NULL) {
                /* Now get the parent dir vnode of the file vnode */
                par_vp = smbfs_smb_get_parent(VTOSMB(file_vp), kShareLock);
                if (par_vp != NULL) {
                    leasep->par_lease_key_hi = VTOSMB(par_vp)->n_lease_key_hi;
                    leasep->par_lease_key_low = VTOSMB(par_vp)->n_lease_key_low;
                    
                    vnode_put(par_vp);
                }
                
                vnode_put(file_vp);
            }
        
            /*
             * If fail to get the file vnode or the parent dir of the file
             * vnode, then just leave ParentLeaseKey at 0
             */
        }
        else {
            /* Not a named stream */
            if (dnp == NULL) {
                /* Have to go get the parent vnode */
                par_vp = smbfs_smb_get_parent(np, kShareLock);
                if (par_vp != NULL) {
                    leasep->par_lease_key_hi = VTOSMB(par_vp)->n_lease_key_hi;
                    leasep->par_lease_key_low = VTOSMB(par_vp)->n_lease_key_low;
                    
                    vnode_put(par_vp);
                }
                else {
                    /* Could not get the parent so leave ParentLeaseKey at 0 */
                }
            }
            else {
                /* Use the passed in parent vnode */
                leasep->par_lease_key_hi = dnp->n_lease_key_hi;
                leasep->par_lease_key_low = dnp->n_lease_key_low;
            }
        }
        
        /* If have np, set the epoch from np */
        if (np != NULL) {
            leasep->epoch = np->n_epoch;
        }

        /* Do we need to generate a new lease key? */
        if (flags & SMB2_NEW_LEASE_KEY) {
            uuid_generate(uuid);
            
            lease_keyp = (uint64_t *) &uuid[15];
            leasep->lease_key_hi = *lease_keyp;
            
            lease_keyp = (uint64_t *) &uuid[7];
            leasep->lease_key_low = *lease_keyp;
        }
        else {
            /* If have np, set the lease key to value from np */
            if (np != NULL) {
                leasep->lease_key_hi = np->n_lease_key_hi;
                leasep->lease_key_low = np->n_lease_key_low;
            }
            else {
                SMBERROR("Missing lease key \n");
                error = EINVAL;
            }
        }
    }
    
    return error;
}

void
smb2_lease_free(struct smb2_lease *leasep)
{
    if (leasep == NULL) {
        /* Should never happen */
        SMBERROR("leasep is null \n");
        return;
    }

    /* Zero out struct to be paranoid */
    leasep->flags = 0;
    leasep->req_lease_state = 0;
    leasep->lease_state = 0;
    leasep->lease_key_hi = 0;
    leasep->lease_key_low = 0;
    leasep->par_lease_key_hi = 0;
    leasep->par_lease_key_low = 0;
    leasep->epoch = 0;
    leasep->pad = 0;
    leasep->handle_reuse_cnt = 0;
    leasep->def_close_timer = 0;

    lck_mtx_destroy(&leasep->lock, smbfs_mutex_group);
}

int
smbfs_add_update_lease(struct smb_share *share, vnode_t vp, struct smb2_lease *in_leasep, uint32_t flags, uint32_t need_lock,
                       const char *reason)
{
    struct smbnode *np = VTOSMB(vp);
    int upgrade = 0, purge_cache = 0, allow_caching = 0;
    struct smb_lease *leasep = NULL;
    int error = 0;
    int16_t delta_epoch = 0;
    uint32_t flag_check = 0;
    
    /*
     * Expected that in_leasep is locked on entry!
     *
     * The expected usage of this function is
     * 1. Add the lease before the Create is sent because a lease break can
     *    arrive before the Create reply is parsed. in_leasep is expected
     *    to be the vnode's lease struct
     * 2. Remove the lease for any of the following
     *    a. If an error occurs for Create
     *    b. On last close and no valid lease
     *    c. Pending def close and lease break arrives for handle caching
     *    in_leasep is expected to be the vnode's lease struct
     * 3. If no error occurs for Create, then Update the lease. dur_handlep
     *    is expected to be a temp lease and its being used to update the
     *    existing lease which should be vnode's lease data
     *    Thus in_leasep is new lease state and leasep is current lease state
     *
     * Dir vnode will try to hold a read/handle lease for the dir enumeration
     * caching. On lease break, just close the dir, try to reopen the dir and
     * refill the enumeration cache again.
     *
     * Files will try to get a lease based on their read/write access. If file
     * has write access, then a durable handle will be requested too.
     */
    if (in_leasep == NULL) {
        /* This should not happen. If it does, skip adding it in */
        SMBERROR_LOCK(np, "Missing in_leasep on <%s>? \n", np->n_name);
        return(EINVAL);
    }

    /*
     * Note if leasing is not supported by this server, then dir leasing will
     * also be disabled. That would be a very strange server in that case.
     */
    if ((share != NULL) &&
        !(SS_TO_SESSION(share)->session_sopt.sv_active_capabilities & SMB2_GLOBAL_CAP_LEASING)) {
        /* If server doesnt support file leasing, nothing to do */
        return(0);
    }
    
    /* Check for valid flags, can only have one of add/remove/update */
    flag_check = (flags & (SMBFS_LEASE_ADD | SMBFS_LEASE_REMOVE | SMBFS_LEASE_UPDATE));
    if ((flag_check != SMBFS_LEASE_ADD) &&
        (flag_check != SMBFS_LEASE_REMOVE) &&
        (flag_check != SMBFS_LEASE_UPDATE)) {
        SMBERROR_LOCK(np, "Illegal flags <0x%x> on <%s>? \n",
                      flags, np->n_name);
        return(EINVAL);
    }

    if (need_lock == 1) {
        lck_mtx_lock(&global_Lease_hash_lock);
    }

    /* Check to see if lease already exists */
    leasep = smbfs_lease_hash_get(in_leasep->lease_key_hi,
                                  in_leasep->lease_key_low);
    if (leasep != NULL) {
        /* Lease already exists in tables */
        if (flags & SMBFS_LEASE_ADD) {
            /* For adding, finding a lease is a bad thing */
            SMBERROR_LOCK(np, "Cant add lease because found existing lease on <%s> during <%s>? \n",
                          np->n_name, reason);
            error = EALREADY;
        }
        else {
            if (flags & SMBFS_LEASE_REMOVE) {
                /* For removing, finding a lease is a good thing */
                SMB_LOG_LEASING_LOCK(np, "Removed lease on <%s> due to <%s> \n",
                                     np->n_name, reason);

                /* Reset some lease fields */
                in_leasep->flags = 0;
                /*
                 * Leave req_lease_state unchanged as its need for trying to
                 * upgrade a lease in smb2fs_smb_lease_upgrade.
                 */
                //in_leasep->req_lease_state = 0;
                in_leasep->lease_state = 0;
                in_leasep->epoch = 0;

                smbfs_lease_hash_remove(vp, leasep, 0, 0);
                error = 0;
            }
            else {
                if (flags & SMBFS_LEASE_UPDATE) {
                    /* For updating, finding a lease is a good thing */
                    error = 0;
                }
                else {
                    SMBERROR("Lease found. Unknown flag value <%d> during <%s>? \n", flags, reason);
                    error = EINVAL;
                }
            }
        }
    }
    else {
        /* Lease not found in tables */
        if (flags & SMBFS_LEASE_ADD) {
            /* For adding, not finding a lease is a good thing */
            SMB_LOG_LEASING_LOCK(np, "Adding lease on <%s> due to <%s> \n",
                                 np->n_name, reason);
            smbfs_lease_hash_add(vp,
                                 in_leasep->lease_key_hi,
                                 in_leasep->lease_key_low,
                                 0);
            error = 0;
        }
        else {
            if (flags & SMBFS_LEASE_REMOVE) {
                /* For removing, not finding a lease is a bad thing */
                SMBERROR_LOCK(np, "Lease entry not found for removing on <%s> due to <%s>? \n",
                              np->n_name, reason);
                error = ENOENT;
            }
            else {
                if (flags & SMBFS_LEASE_UPDATE) {
                    /* For updating, not finding a lease is a bad thing */
                    SMBERROR_LOCK(np, "Lease entry not found for updating on <%s> during <%s>? \n",
                                  np->n_name, reason);
                    error = ENOENT;
                }
                else {
                    SMBERROR("No lease. Unknown flag value <%d> during <%s>? \n", flags, reason);
                    error = EINVAL;
                }
            }
        }
    }

    /* Should be done with global hash table*/
    if (need_lock == 1) {
        lck_mtx_unlock(&global_Lease_hash_lock);
    }

    /* Are we updating the current lease? */
    if ((flags & SMBFS_LEASE_UPDATE) && (error == 0) &&
        (in_leasep->flags & SMB2_LEASE_GRANTED)) {
        /* Safety check */
        if (in_leasep == &np->n_lease) {
            SMBERROR_LOCK(np, "in_leasep should NOT be same as vnode lease for updating on <%s> during <%s>? \n",
                          np->n_name, reason);
            return(EINVAL);
        }

        /* Lock vnode current lease state */
        smbnode_lease_lock(&np->n_lease, smbfs_add_update_lease);
        
        /*
         * Is it a Lease V2 and thus we have an epoch?
         * The epoch is used to track lease state changes and helps the client
         * deteremine if this is an older lease state change that can be
         * ignored.
         */
        if (in_leasep->flags & SMB2_LEASE_V2) {
            /* Yea we have an epoch to use, go use it */
            delta_epoch = smbfs_get_epoch_delta(in_leasep->epoch,
                                                np->n_lease.epoch);
        }
        else {
            /*
             * We have no epoch to check, so have to assume new lease
             * state is always valid
             */
            delta_epoch = 1;
        }

        SMB_LOG_LEASING_LOCK(np, "delta_epoch <%d> curr lease state <0x%x> new lease state <0x%x> on <%s> during <%s> \n",
                             delta_epoch, np->n_lease.lease_state,
                             in_leasep->lease_state, np->n_name, reason);

        if (delta_epoch > 0) {
            /*
             * [MS-SMB2] 3.2.5.7.5
             * Evaluate the new lease state compared to existing
             * dur_handlep is new lease
             * vnode lease is current lease
             * [MS-SMB2] 3.3.1.4
             * Add handing for a (Rd|Wr) lease.
             */
            switch (np->n_lease.lease_state) {
                case SMB2_LEASE_NONE:
                    switch (in_leasep->lease_state) {
                        case SMB2_LEASE_READ_CACHING:
                        case (SMB2_LEASE_READ_CACHING | SMB2_LEASE_WRITE_CACHING): /* Assuming this is correct */
                        case (SMB2_LEASE_READ_CACHING | SMB2_LEASE_HANDLE_CACHING):
                        case (SMB2_LEASE_READ_CACHING | SMB2_LEASE_WRITE_CACHING | SMB2_LEASE_HANDLE_CACHING):
                            upgrade = 1;
                            break;
                        case SMB2_LEASE_NONE:
                            break;
                       default:
                            SMBERROR_LOCK(np, "Unknown new lease state <0x%x> on <%s> during <%s> \n",
                                          in_leasep->lease_state, np->n_name, reason);
                            error = EINVAL;
                            break;
                    }
                    break;
                case SMB2_LEASE_READ_CACHING:
                    switch (in_leasep->lease_state) {
                        case SMB2_LEASE_READ_CACHING:
                            purge_cache = 1;
                            break;
                        case (SMB2_LEASE_READ_CACHING | SMB2_LEASE_WRITE_CACHING): /* Assuming this is correct */
                        case (SMB2_LEASE_READ_CACHING | SMB2_LEASE_HANDLE_CACHING):
                        case (SMB2_LEASE_READ_CACHING | SMB2_LEASE_WRITE_CACHING | SMB2_LEASE_HANDLE_CACHING):
                            if (delta_epoch == 1) {
                                upgrade = 1;
                            }
                            else {
                                upgrade = 1;
                                purge_cache = 1;
                            }
                            break;
                        case SMB2_LEASE_NONE:
                            purge_cache = 1;
                            break;
                       default:
                            SMBERROR_LOCK(np, "Unknown new lease state <0x%x> on <%s> during <%s> \n",
                                          in_leasep->lease_state, np->n_name, reason);
                            error = EINVAL;
                            break;
                    }
                    break;
                case (SMB2_LEASE_READ_CACHING | SMB2_LEASE_WRITE_CACHING):
                    switch (in_leasep->lease_state) {
                        case SMB2_LEASE_READ_CACHING:
                        case (SMB2_LEASE_READ_CACHING | SMB2_LEASE_HANDLE_CACHING):
                            SMBERROR_LOCK(np, "Invalid new lease state <0x%x> on <%s> during <%s> \n",
                                          in_leasep->lease_state, np->n_name, reason);
                            error = EINVAL;
                            break;
                        case (SMB2_LEASE_READ_CACHING | SMB2_LEASE_WRITE_CACHING | SMB2_LEASE_HANDLE_CACHING):
                            upgrade = 1;
                            break;
                        case (SMB2_LEASE_READ_CACHING | SMB2_LEASE_WRITE_CACHING):
                            /* Assume we only need to update the epoch */
                            upgrade = 1;
                            break;
                        case SMB2_LEASE_NONE:
                            /* Assume this should be purge */
                            purge_cache = 1;
                            break;
                        default:
                            SMBERROR_LOCK(np, "Unknown new lease state <0x%x> on <%s> during <%s> \n",
                                          in_leasep->lease_state, np->n_name, reason);
                            error = EINVAL;
                            break;
                    }
                     break;
                case (SMB2_LEASE_READ_CACHING | SMB2_LEASE_HANDLE_CACHING):
                    switch (in_leasep->lease_state) {
                        case SMB2_LEASE_READ_CACHING:
                        case (SMB2_LEASE_READ_CACHING | SMB2_LEASE_WRITE_CACHING): /* Assuming this is correct */
                            SMBERROR_LOCK(np, "Invalid new lease state <0x%x> on <%s> during <%s> \n",
                                          in_leasep->lease_state, np->n_name, reason);
                            error = EINVAL;
                            break;
                        case (SMB2_LEASE_READ_CACHING | SMB2_LEASE_HANDLE_CACHING):
                            purge_cache = 1;
                            break;
                        case (SMB2_LEASE_READ_CACHING | SMB2_LEASE_WRITE_CACHING | SMB2_LEASE_HANDLE_CACHING):
                            if (delta_epoch == 1) {
                                upgrade = 1;
                            }
                            else {
                                upgrade = 1;
                                purge_cache = 1;
                            }
                            break;
                        case SMB2_LEASE_NONE:
                            /* Assume this should be purge */
                            purge_cache = 1;
                            break;
                        default:
                            SMBERROR_LOCK(np, "Unknown new lease state <0x%x> on <%s> during <%s> \n",
                                          in_leasep->lease_state, np->n_name, reason);
                            error = EINVAL;
                            break;
                    }
                    break;
                case (SMB2_LEASE_READ_CACHING | SMB2_LEASE_WRITE_CACHING | SMB2_LEASE_HANDLE_CACHING):
                    switch (in_leasep->lease_state) {
                        case SMB2_LEASE_READ_CACHING:
                        case (SMB2_LEASE_READ_CACHING | SMB2_LEASE_WRITE_CACHING): /* Assuming this is correct */
                        case (SMB2_LEASE_READ_CACHING | SMB2_LEASE_HANDLE_CACHING):
                            SMBERROR_LOCK(np, "Invalid new lease state <0x%x> on <%s> during <%s> \n",
                                          in_leasep->lease_state, np->n_name, reason);
                            error = EINVAL;
                            break;
                        case (SMB2_LEASE_READ_CACHING | SMB2_LEASE_WRITE_CACHING | SMB2_LEASE_HANDLE_CACHING):
                            /* Assume this should be ignored */
                            break;
                        case SMB2_LEASE_NONE:
                            /* Assume this should be purge */
                            purge_cache = 1;
                            break;
                        default:
                            SMBERROR_LOCK(np, "Unknown new lease state <0x%x> on <%s> during <%s> \n",
                                          in_leasep->lease_state, np->n_name, reason);
                            error = EINVAL;
                            break;
                    }
                     break;
                default:
                    SMBERROR_LOCK(np, "Unknown current lease state <0x%x> on <%s> during <%s> \n",
                                  np->n_lease.lease_state, np->n_name, reason);
                    error = EINVAL;
                    break;
            }
        }
            
        if (upgrade == 1) {
            SMB_LOG_LEASING_LOCK(np, "Updating lease from <0x%x> to <0x%x> on <%s> due to <%s> \n",
                                 np->n_lease.lease_state, in_leasep->lease_state,
                                 np->n_name, reason);

            np->n_lease.lease_state = in_leasep->lease_state;
            np->n_lease.epoch = in_leasep->epoch;
            
            np->n_lease.def_close_timer = 0;
            np->n_lease.req_lease_state = in_leasep->req_lease_state;
            np->n_lease.flags = in_leasep->flags;
            
            if (np->n_lease.lease_key_hi != in_leasep->lease_key_hi) {
                np->n_lease.lease_key_hi = in_leasep->lease_key_hi;
            }
            if (np->n_lease.lease_key_low != in_leasep->lease_key_low) {
                np->n_lease.lease_key_low = in_leasep->lease_key_low;
            }
        }
        
        /*
         * [MS-SMB2] 3.2.1.5
         * If Write Caching, client has exclusive access so can UBC cache
         * If Read only caching, client can still use UBC cache, but MUST send
         * all writes to server immediately
         */
        if (np->n_lease.lease_state & (SMB2_LEASE_READ_CACHING | SMB2_LEASE_WRITE_CACHING)) {
            if (np->n_lease.lease_state & SMB2_LEASE_WRITE_CACHING) {
                np->n_flag &= ~N_WRITE_IMMEDIATELY;
            }
            else {
                /* All cached writes must also be immediately sent to server */
                np->n_flag |= N_WRITE_IMMEDIATELY;
            }

            allow_caching = 1;
        }
        else {
            allow_caching = 0;
        }

        smbnode_lease_unlock(&np->n_lease);
        
        /* Deal with file UBC now */
        if (!vnode_isdir(vp)) {
            /* Its a file, see if its currently cacheable or not */
            if (vnode_isnocache(vp)) {
                /* Currently not cacheable, thus nothing to purge */
                if (allow_caching == 1) {
                    if (VTOSMB(vp)->f_hasBRLs == 0) {
                        /* Re-enable caching */
                        SMB_LOG_LEASING_LOCK(np, "UBC caching enabled on <%s> \n",
                                             np->n_name);
                        vnode_clearnocache(vp);
                    }
                    else {
                        /* File is not allowed to be cached due to brls */
                        SMB_LOG_LEASING_LOCK(np, "UBC caching not allowed to be enabled due to brls on <%s> \n",
                                             np->n_name);
                    }
                }
            }
            else {
                /* Currently cacheable */
                if (purge_cache) {
                    if (flags & SMBFS_IN_RECONNECT) {
                        /*
                         * In reconnect, so it is not safe to do UBC invalidate,
                         * do it later
                         */
                        SMB_LOG_LEASING_LOCK(np, "Delay purge UBC cache on <%s> \n",
                                               np->n_name);
                        
                        SMB_LOG_UBC_LOCK(np, "Postpone UBC_INVALIDATE on <%s> because currently in reconnect \n",
                                         np->n_name);

                        np->n_flag |= NNEEDS_UBC_INVALIDATE;
                    }
                    else {
                        SMB_LOG_LEASING_LOCK(np, "Purge UBC cache on <%s> \n",
                                               np->n_name);

                        SMB_LOG_UBC_LOCK(np, "UBC_INVALIDATE on <%s> due to lease update \n",
                                         np->n_name);
                        
                        np->n_flag &= ~NNEEDS_UBC_INVALIDATE;
                        ubc_msync(vp, 0, ubc_getsize(vp), NULL, UBC_INVALIDATE);
                    }
                }

                if (allow_caching == 0) {
                    SMB_LOG_LEASING_LOCK(np, "UBC caching disabled on <%s> \n",
                                           np->n_name);
                    vnode_setnocache(vp);
                }
            }
        }
    }

    return (error);
}

int16_t
smbfs_get_epoch_delta(uint16_t server_epoch, uint16_t file_epoch)
{
    int16_t delta_epoch = 0;

    /*
     * Server always increments epoch by +1 {MS-SMB2] 3.3.4.7
     */
    do {
        if (server_epoch == file_epoch) {
            /* Simple case first */
            delta_epoch = 0;
            break;
        }
        
        if (server_epoch > file_epoch) {
            /* This is a newer lease state than our current */
            delta_epoch = server_epoch - file_epoch;
            break;
        }
        
        if (file_epoch > server_epoch) {
            if ((file_epoch - server_epoch) > 32767) {
                /* Server must have wrapped around and thus is newer */
                delta_epoch = file_epoch - server_epoch;
                break;
            }
            else {
                /* Server must be resending old reply, ignore it */
                delta_epoch = -1;
                break;
            }
        }
    } while (0);
    
    return(delta_epoch);
}

uint32_t
smbfs_get_req_lease_state(uint32_t access_rights)
{
    uint32_t req_lease_state = 0;

    /* [MS-SMB2] <136> Code to determine lease based on file access */
    req_lease_state = SMB2_LEASE_HANDLE_CACHING;
    
    if (access_rights & SMB2_FILE_READ_DATA) {
        req_lease_state |= SMB2_LEASE_READ_CACHING;
    }

    if (access_rights & SMB2_FILE_WRITE_DATA) {
        req_lease_state |= SMB2_LEASE_WRITE_CACHING;
    }
    
    return(req_lease_state);
}

int
smbfs_handle_dir_lease_break(struct lease_rq *lease_rqp)
{
    vnode_t vp = NULL;
    uint32_t vid = 0;
    struct smb_lease *leasep = NULL;
    int error = 0;
    struct smbnode *np = NULL;
    int is_locked = 0;

    /*
     * if its a dir lease break, then we are invalidating the dir enumeration
     * cache immediately instead of waiting for the lease thread to run at
     * some later time. Closing the dir, removing the lease entry, and sending
     * the lease break ack is done later by the lease thread.
     */

    /*
     * Use the lease key to find the leasep
     */
    lck_mtx_lock(&global_Lease_hash_lock);
    is_locked = 1;
    /*
     * Be careful here holding on to the global_Lease_hash_lock for too long.
     * Do NOT hold it over any SMB Requests/Reply as reconnect could occur
     * and the reconnect code will need to grab the global_Lease_hash_lock.
     * That can lead to a deadlock where we are holding global_Lease_hash_lock
     * and waiting for a reply, but the iod thread is in reconnect and not
     * processing replies while it waits for the global_Lease_hash_lock
     */
    leasep = smbfs_lease_hash_get(lease_rqp->lease_key_hi,
                                  lease_rqp->lease_key_low);

    if (leasep == NULL) {
        SMBERROR("Failed to find lease \n");
        error = ENOENT;
        goto bad;
    }

    /* Try to retrieve the vnode */
    vp = leasep->vnode;
    vid = leasep->vid;

    /*
     * vnode_getwithvid() will wait for smbfs_vnop_reclaim() if it's in process
     * smbfs_vnop_reclaim() might try to lock global_Lease_hash_lock
     * this will cause a deadlcok
     * Unlock global_Lease_hash_lock while we acquire iocount on the vnode
     */
    vnode_hold(vp);
    lck_mtx_unlock(&global_Lease_hash_lock);
    is_locked = 0;
    if (vnode_getwithvid(vp, vid)) {
        SMBERROR("Failed to get the vnode \n");
        vnode_drop(vp);
        error = ENOENT;
        vp = NULL;
        goto bad;
    }
    vnode_drop(vp);
    lck_mtx_lock(&global_Lease_hash_lock);
    is_locked = 1;

    leasep = smbfs_lease_hash_get(lease_rqp->lease_key_hi,
                                  lease_rqp->lease_key_low);

    /*
     * Make sure the lease is still there
     */
    if (leasep == NULL) {
        SMBERROR("Failed to find lease after acquirung iocount\n");
        error = ENOENT;
        goto bad;
    }

    if (leasep->vnode != vp) {
        /*
         * This should never happen, being extra paranoid
         */
        SMBERROR("leasep vnode doesn't match vp\n");
        error = ENOENT;
        goto bad;
    }

    /* Make sure it is one of my vnodes */
    if (vnode_tag(vp) != VT_CIFS) {
        /* Should be impossible */
        SMBERROR("vnode_getwithvid found non SMB vnode???\n");
        error = ENOENT;
        goto bad;
    }

    np = VTOSMB(vp);

    if (vnode_vtype(vp) == VDIR) {
        /* Check for a Dir Lease; can only be one per dir */
        smbnode_lease_lock(&np->n_lease, smbfs_handle_dir_lease_break);

        if ((np->n_lease.lease_key_hi == lease_rqp->lease_key_hi) &&
            (np->n_lease.lease_key_low == lease_rqp->lease_key_low)) {
            /* This will invalidate the dir enum cache */
            np->d_changecnt++;

            /* Assume a Dir Lease break essentially means the lease is gone */
            if (lease_rqp->new_lease_state != SMB2_LEASE_NONE) {
                SMBERROR_LOCK(np, "Dir Lease break not SMB2_LEASE_NONE <0x%x> on <%s>? \n",
                              lease_rqp->new_lease_state, np->n_name);
            }
            np->n_lease.flags |= SMB2_LEASE_BROKEN;
            np->n_lease.flags &= ~SMB2_LEASE_GRANTED;

            SMB_LOG_LEASING_LOCK(np, "lease changing state <0x%x> to <0x%x> on <%s> \n",
                                 np->n_lease.lease_state,
                                 lease_rqp->new_lease_state,
                                 np->n_name);

            np->n_lease.lease_state = lease_rqp->new_lease_state;
            np->n_lease.epoch = lease_rqp->server_epoch;

            /*
             * Can not remove dir lease yet as smbfs_handle_lease_break()
             * will need to find it
             */

            smbnode_lease_unlock(&np->n_lease);

            cache_purge(vp);

            /* Set need_close_dir flag so lease thread will attemp to close the dir */
            lease_rqp->need_close_dir = 1;
        }
        else {
            smbnode_lease_unlock(&np->n_lease);
            SMBERROR_LOCK(np, "No dir lease found for lease break on <%s>\n",
                          np->n_name);
        }
    }

bad:
    if (is_locked) {
        lck_mtx_unlock(&global_Lease_hash_lock);
    }

    if (vp != NULL) {
        vnode_put(vp);
    }

    return (error);
}

static void clear_pending_break(struct smbnode *np) {
    
    smbnode_lease_lock(&np->n_lease, clear_pending_break);
    np->n_lease.pending_break = 0;
    wakeup(&np->n_lease.pending_break);
    smbnode_lease_unlock(&np->n_lease);
}

int
smbfs_handle_lease_break(struct lease_rq *lease_rqp, vfs_context_t context)
{
	struct smb_share *share = NULL;
	vnode_t	vp = NULL;
	uint32_t vid = 0;
	struct smb_lease *leasep = NULL;
	int error = 0, tmp_error = 0, warning = 0;
	int16_t delta_epoch = 0;
	struct smb_session *sessionp = NULL;
	struct smbnode *np = NULL;
    int flush_cache = 0, purge_cache = 0, allow_caching = 0;
	int need_close_files = 0, skip_lease_break = 0;
    int is_locked = 0;
    int break_pending = 0;
    struct smb2_close_rq close_parms = {0};
    SMBFID fid = 0; 

    SMB_LOG_KTRACE(SMB_DBG_SMBFS_HANDLE_LEASE_BREAK | DBG_FUNC_START,
                   0, 0, 0, 0, 0);

    /*
	 * Use the lease key to find the leasep
	 */
	lck_mtx_lock(&global_Lease_hash_lock);
	is_locked = 1;

    /*
     * Be careful here holding on to the global_Lease_hash_lock for too long.
     * Do NOT hold it over any SMB Requests/Reply as reconnect could occur
     * and the reconnect code will need to grab the global_Lease_hash_lock.
     * That can lead to a deadlock where we are holding global_Lease_hash_lock
     * and waiting for a reply, but the iod thread is in reconnect and not
     * processing replies while it waits for the global_Lease_hash_lock
     */

	leasep = smbfs_lease_hash_get(lease_rqp->lease_key_hi,
                                  lease_rqp->lease_key_low);
	if (leasep == NULL) {
        SMB_LOG_KTRACE(SMB_DBG_SMBFS_HANDLE_LEASE_BREAK | DBG_FUNC_NONE,
                       0xabc001, 0, 0, 0, 0);

		SMBERROR("Failed to find lease \n");
		error = ENOENT;
		goto bad;
	}
	
	/* Try to retrieve the vnode */
	vp = leasep->vnode;
	vid = leasep->vid;
	
    /*
     * vnode_getwithvid() will wait for smbfs_vnop_reclaim() if it's in process
     * smbfs_vnop_reclaim() might try to lock global_Lease_hash_lock
     * this will cause a deadlcok
     * Unlock global_Lease_hash_lock while we acquire iocount on the vnode
     */
    vnode_hold(vp);
    lck_mtx_unlock(&global_Lease_hash_lock);
    is_locked = 0;
    if (vnode_getwithvid(vp, vid)) {
        SMBERROR("Failed to get the vnode \n");
        vnode_drop(vp);
        error = ENOENT;
        vp = NULL;
        goto bad;
    }
    vnode_drop(vp);
    lck_mtx_lock(&global_Lease_hash_lock);
    is_locked = 1;

    leasep = smbfs_lease_hash_get(lease_rqp->lease_key_hi,
                                  lease_rqp->lease_key_low);

    /*
     * Make sure the lease is still there
     */
    if (leasep == NULL) {
        SMBERROR("Failed to find lease after acquirung iocount\n");
        error = ENOENT;
        goto bad;
    }

    if (leasep->vnode != vp) {
        /*
         * This should never happen, being extra paranoid
         */
        SMBERROR("leasep vnode doesn't match vp\n");
        error = ENOENT;
        goto bad;
    }

    /* Make sure it is one of my vnodes */
    if (vnode_tag(vp) != VT_CIFS) {
        /* Should be impossible */
        SMBERROR("vnode_getwithvid found non SMB vnode???\n");
        error = ENOENT;
        goto bad;
    }

	np = VTOSMB(vp);

	/* Get the share and hold a reference on it */
	share = smb_get_share_with_reference(VTOSMBFS(vp));
	if (share == NULL) {
		SMBERROR("Failed to get the share \n");
		error = ENOENT;
		goto bad;
	}

	sessionp = SS_TO_SESSION(share);
	if (sessionp == NULL) {
		SMBERROR("Failed to get session \n");
		error = ENOENT;
		goto bad;
	}

	/* Process the lease */
	if (vnode_vtype(vp) != VDIR) {
        /* Check for a file Lease; can only be one per file */
        smbnode_lease_lock(&np->n_lease, smbfs_handle_lease_break);
        
        /*
         * Handle File Lease Break [MS-SMB2] 3.2.5.19.2
         */
        if ((np->n_lease.lease_state & SMB2_LEASE_WRITE_CACHING) &&
            !(lease_rqp->new_lease_state & SMB2_LEASE_WRITE_CACHING)) {
            /*
             * Lost write caching
             * Need to flush UBC to push out any cached data
             */
            flush_cache = 1;
        }
        
        if ((np->n_lease.lease_state & SMB2_LEASE_READ_CACHING) &&
            !(lease_rqp->new_lease_state & SMB2_LEASE_READ_CACHING)) {
            /*
             * Lost read caching
             * Need to flush UBC to push out any cached data
             */
            purge_cache = 1;
        }
        
        if ((np->n_lease.lease_state & SMB2_LEASE_HANDLE_CACHING) &&
            !(lease_rqp->new_lease_state & SMB2_LEASE_HANDLE_CACHING) &&
            (np->n_lease.flags & SMB2_DEFERRED_CLOSE)) {
            /*
             * Lost handle caching, close the deferred close now.
             * If file ref still in use (ie no deferred close), then we
             * only lose the handle lease meaning that we can not defer
             * any close on this file ref.
             *
             * The new lease state is set later in this code.
             */
            np->n_lease.flags &= ~(SMB2_LEASE_GRANTED | SMB2_DEFERRED_CLOSE);
            OSAddAtomic(-1, &share->ss_curr_def_close_cnt);
            
            /* Clear deferred close time */
            np->n_lease.def_close_timer = 0;

            /* Set flag to close the fid later in this code */
            need_close_files = 1;
            
            /* Set flag to indicate that a close request is pending */
            np->n_lease.pending_break = 1;
            break_pending = 1;
        }
        
        if (SMBV_SMB3_OR_LATER(sessionp)) {
            /*
             * Check the epoch for SMB 3.x
             */
            delta_epoch = smbfs_get_epoch_delta(lease_rqp->server_epoch,
                                                np->n_lease.epoch);

            if (delta_epoch > 0) {
                if (np->n_lease.lease_state != lease_rqp->curr_lease_state) {
                   /*  
                    * This should only happen if we sent a request to upgrade
                    * a lease and then a lease break happens before the server
                    * can reply to the lease upgrade.
                    */
                    SMBERROR_LOCK(np, "Client lease != Server Lease: delta = <%d> Client Lease = <0x%x>, Server Curr Lease = <0x%x>, Server New Lease = <0x%x> on <%s> \n",
                                  delta_epoch,
                                  np->n_lease.lease_state,
                                  lease_rqp->curr_lease_state,
                                  lease_rqp->new_lease_state,
                                  np->n_name);
                }

                /* Equal lease states and delta epoch > 1, must flush & purge UBC */
                if ((delta_epoch > 1) &&
                    (np->n_lease.lease_state == lease_rqp->new_lease_state)) {

                    if (np->n_lease.lease_state != lease_rqp->curr_lease_state) {
                        /*
                         * <rdar://95958478>:
                         * The server might provide a lease break before it responds to a lease-upgrade.
                         * In that case, flush the cache before we purge it
                         */
                        flush_cache = 1;
                    }
                    purge_cache = 1;
                }
                
                /*
                 * If delta_epoch > 0, then must copy new lease state and
                 * epoch value.
                 */
                if (np->n_lease.lease_state != lease_rqp->new_lease_state) {
                    SMB_LOG_LEASING_LOCK(np, "lease changing state <0x%x> to <0x%x> on <%s> \n",
                                         np->n_lease.lease_state,
                                         lease_rqp->new_lease_state,
                                         np->n_name);
                }

                np->n_lease.lease_state = lease_rqp->new_lease_state;
                np->n_lease.epoch = lease_rqp->server_epoch;
            }
        }
        else {
            /* SMB v2.x, must copy new lease state */
            if (np->n_lease.lease_state != lease_rqp->new_lease_state) {
                SMB_LOG_LEASING_LOCK(np, "File lease break changing state from <0x%x> to <0x%x> on <%s> \n",
                                     np->n_lease.lease_state,
                                     lease_rqp->new_lease_state,
                                     np->n_name);
            }
            
            np->n_lease.lease_state = lease_rqp->new_lease_state;
        }
        
        /*
         * If lease none, clear some lease flags
         */
        if (np->n_lease.lease_state == SMB2_LEASE_NONE) {
            SMB_LOG_LEASING_LOCK(np, "Lost file lease on <%s> due to lease break \n",
                                 np->n_name);
            
            np->n_lease.flags |= SMB2_LEASE_BROKEN;
            np->n_lease.flags &= ~SMB2_LEASE_GRANTED;
        }
        
        /*
         * [MS-SMB2] 3.2.1.5
         * If Write Caching, client has exclusive access so can UBC cache
         * If Read only caching, client can still use UBC cache, but MUST send
         * all writes to server immediately
         */
        if (np->n_lease.lease_state & (SMB2_LEASE_READ_CACHING | SMB2_LEASE_WRITE_CACHING)) {
            if (np->n_lease.lease_state & SMB2_LEASE_WRITE_CACHING) {
                np->n_flag &= ~N_WRITE_IMMEDIATELY;
            }
            else {
                /* All cached writes must also be immediately sent to server */
                np->n_flag |= N_WRITE_IMMEDIATELY;
            }

            allow_caching = 1;
        }
        else {
            allow_caching = 0;
        }

        if ((np->n_lease.lease_state == SMB2_LEASE_NONE) || (need_close_files)) {
            /* Remove the lease */
            /* rdar://108758378 unlock the hash table to prevent a deadlock
            * with smbfs_add_update_lease() called from smbfs_close() which
            * tries to lock the locks in the reverse order
            * let smbfs_add_update_lease() handle the hash table lock
            */
            lck_mtx_unlock(&global_Lease_hash_lock);
            warning = smbfs_add_update_lease(share, vp, &np->n_lease, SMBFS_LEASE_REMOVE, 1,
                                             "LeaseBreakCloseFile");
            /* smbfs_add_update_lease() unlocks the hash table if it locks it */
            is_locked = 0;
            if (warning) {
                /* Shouldnt ever happen */
                SMBERROR_LOCK(np, "smbfs_add_update_lease remove failed <%d> on <%s>\n",
                              warning, np->n_name);
            }
        }

        smbnode_lease_unlock(&np->n_lease);
	}
	
    SMB_LOG_KTRACE(SMB_DBG_SMBFS_HANDLE_LEASE_BREAK | DBG_FUNC_NONE,
                   0xabc002, need_close_files, lease_rqp->need_close_dir, 0, 0);

	/*
     * Free the hash lock as the file or dir closes and lease break ack
     * could take awhile and a reconnect could occur during that time.
     */
    if (is_locked) {
        lck_mtx_unlock(&global_Lease_hash_lock);
        is_locked = 0;
    }

    /*
     * Handle file UBC changes now
     */
    if (vnode_vtype(vp) != VDIR) {
        if (vnode_isnocache(vp)) {
            /* Currently not cacheable, thus nothing to flush or purge */
            if (allow_caching == 1) {
                if (VTOSMB(vp)->f_hasBRLs == 0) {
                    /* Re-enable caching */
                    SMB_LOG_LEASING_LOCK(np, "UBC caching enabled on <%s> \n",
                                         np->n_name);
                    vnode_clearnocache(vp);
                }
                else {
                    /* File is not allowed to be cached due to brls */
                    SMB_LOG_LEASING_LOCK(np, "UBC caching not allowed to be enabled due to brls on <%s> \n",
                                         np->n_name);
                }
            }
        }
        else {
            /* Currently cacheable */
            if (flush_cache) {
                SMB_LOG_LEASING_LOCK(np, "Flush UBC cache on <%s> \n",
                                     np->n_name);
                SMB_LOG_UBC_LOCK(np, "UBC_PUSHDIRTY on <%s> due to lease break \n",
                                 np->n_name);
                
                np->n_flag &= ~NNEEDS_UBC_PUSHDIRTY;
                ubc_msync(vp, 0, ubc_getsize(vp), NULL, UBC_PUSHDIRTY | UBC_SYNC);
                
                /* Invalidate the meta data cache too */
                np->attribute_cache_timer = 0;
            }
            
            if (purge_cache) {
                SMB_LOG_LEASING_LOCK(np, "Purge UBC cache on <%s> \n",
                                     np->n_name);
                SMB_LOG_UBC_LOCK(np, "UBC_INVALIDATE on <%s> due to lease break \n",
                                 np->n_name);
                
                np->n_flag &= ~NNEEDS_UBC_INVALIDATE;
                ubc_msync (vp, 0, ubc_getsize(vp), NULL, UBC_INVALIDATE);
                
                /* Invalidate the meta data cache too */
                np->attribute_cache_timer = 0;
            }

            if ((allow_caching == 0) && (need_close_files == 0)) {
                /* If closing the deferred close, then no need to disable UBC */
                SMB_LOG_LEASING_LOCK(np, "UBC caching disabled on <%s> \n",
                                     np->n_name);
                vnode_setnocache(vp);
            }
        }

        /* Attempt to close the file fid now if needed */
        if (need_close_files) {
            /*
             * Which fid needs to be closed? Should only be one.
             * Dont need to flush file as it is a deferred close meaning the flush
             * happened on the vnop_close() earlier.
             */
            if (np->f_lockFID_fid != 0) {
                /*
                 * Must clear out f_lockFID_accessMode so that a
                 * pending deferred close on the sharedFID doesnt
                 * end up matching on the already closed lockFID.
                 */
                np->f_lockFID_accessMode = 0;
                fid = np->f_lockFID_fid;
                np->f_lockFID_fid = 0;
                 
                 /* clear the durable handle */
                 smbnode_dur_handle_lock(&np->f_lockFID_dur_handle, CloseDeferredFileRefs);
                 smb2_dur_handle_init(share, 0, &np->f_lockFID_dur_handle, 0);
                 smbnode_dur_handle_unlock(&np->f_lockFID_dur_handle);

                /*
                 * MUST clear fields before attempting close as the
                 * close can take a while to complete and in the
                 * meanwhile, another open can occur.
                 */
                SMB_LOG_LEASING_LOCK(np, "Close lockFID on <%s> \n",
                                     np->n_name);
                error = smbfs_smb_close_attr(vp, share,
                                             fid, &close_parms,
                                             context);
                if (error) {
                    SMBWARNING("close file failed %d on lockFID %llx\n",
                               error, np->f_lockFID_fid);
                }
                else {
                    /* [MS-SMB2] 3.2.5.19.2 Close is an implicit lease break ack */
                    skip_lease_break = 1;
                }
            }
            else {
                if (np->f_sharedFID_fid != 0) {
                    /*
                     * Must clear out f_sharedFID_accessMode so that a
                     * pending deferred close on the lockFID doesnt
                     * end up matching on the already closed sharedFID.
                     */
                    np->f_sharedFID_accessMode = 0;
                    fid = np->f_sharedFID_fid;
                    np->f_sharedFID_fid = 0;
                    
                    /* clear the durable handle */
                    smbnode_dur_handle_lock(&np->f_sharedFID_dur_handle, CloseDeferredFileRefs);
                    smb2_dur_handle_init(share, 0, &np->f_sharedFID_dur_handle, 0);
                    smbnode_dur_handle_unlock(&np->f_sharedFID_dur_handle);

                    /*
                     * MUST clear fields before attempting close as the
                     * close can take a while to complete and in the
                     * meanwhile, another open can occur.
                     */
                    SMB_LOG_LEASING_LOCK(np, "Close sharedFID on <%s> \n",
                                         np->n_name);
                    error = smbfs_smb_close_attr(vp, share,
                                                 fid, &close_parms,
                                                 context);
                    if (error) {
                        SMBWARNING("close file failed %d on sharedFID %llx\n",
                                   error, np->f_sharedFID_fid);
                    }
                    else {
                        /* [MS-SMB2] 3.2.5.19.2 Close is an implicit lease break ack */
                        skip_lease_break = 1;
                    }
               }
                else {
                    SMBERROR("No deferred close FID found for <%s>??? \n", np->n_name);
                    error = EBADF;
                }
            }
        } /* need_close_files */
    } /* (vnode_vtype(vp) != VDIR) */
    
    /* Attempt to close the dir now if needed */
    if ((lease_rqp->need_close_dir) &&
        (np != NULL) &&
        (vp != NULL)) {
        /*
         * Try to get the lock and if fail, then it must be busy
         * so skip this dir. It will get closed and lease removed later in
         * smbfs_closedirlookup()
         */
        tmp_error = smbnode_trylock(np, SMBFS_EXCLUSIVE_LOCK);
        if (tmp_error == 0) {
            /*
             * SMBFS_EXCLUSIVE_LOCK should be enough to
             * keep others from accessing the dir while we close
             * it.
             */
            
            /*
             * Remove the dir lease before closing dir as that might take
             * a while to complete
             */
            smbnode_lease_lock(&np->n_lease, smbfs_handle_lease_break);

            warning = smbfs_add_update_lease(share, vp, &np->n_lease, SMBFS_LEASE_REMOVE, 1,
                                             "LeaseBreakCloseDir");
            if (warning) {
                /* Shouldnt ever happen */
                SMBERROR_LOCK(np, "smbfs_add_update_lease remove failed <%d> on <%s>\n",
                              warning, np->n_name);
            }

            smbnode_lease_unlock(&np->n_lease);

            /* Close the dir now */
            smbfs_closedirlookup(np, 0, "dir lease break", context);

            smbnode_unlock(np);
            
            /* [MS-SMB2] 3.2.5.19.2 Close is an implicit lease break ack */
            skip_lease_break = 1;
        }

        /*
         * If dir has a kQueue, then notify that some change occured.
         * If a child in the dir has been renamed, deleted or added, then
         * we get a Change Notify response too.
         * But, if a child file is written to, then we only get a lease
         * break and no Change Notify, so do the extra notify here
         */
        if ((vnode_ismonitored(vp)) && context) {
            struct vnode_attr vattr;

            vfs_get_notify_attributes(&vattr);
            smbfs_attr_cachelookup(share, vp, &vattr, context, TRUE);
            vnode_notify(vp, VNODE_EVENT_ATTRIB | VNODE_EVENT_WRITE, &vattr);
        }
    }

	if ((skip_lease_break == 0) &&
        (lease_rqp->flags & SMB2_NOTIFY_BREAK_LEASE_FLAG_ACK_REQUIRED)) {
		//lck_mtx_lock(&share->ss_shlock); /* this causes a deadlock... */
		/*
		 * Notes
         * 1. Lease Break Ack request does need to be signed if signing is
         *    being used.
         * 2. clear_pending_break() is only used if the file is being closed
         *    and we need to wait for the close request/reply to finish. A
         *    close acts as an implicit lease break ack so we should never be
         *    calling this code. Also means we should never need to call
         *    clear_pending_break() in smb2_smb_lease_break_ack()
         * 3. <111804707> Do the lease break ack request/reply using the rw
         *    helper threads so we can continue processing other lease breaks
		 */
		error = smb2_smb_lease_break_ack_queue(share, lease_rqp->received_iod,
                                               lease_rqp->lease_key_hi,
                                               lease_rqp->lease_key_low,
                                               lease_rqp->new_lease_state,
                                               context);
		//lck_mtx_unlock(&share->ss_shlock);
	}

bad:
    if (break_pending) {
        clear_pending_break(np);
    }

    if (is_locked) {
        lck_mtx_unlock(&global_Lease_hash_lock);
    }

	if (share != NULL) {
		smb_share_rele(share, context);
	}

	if (vp != NULL) {
		vnode_put(vp);
	}

    SMB_LOG_KTRACE(SMB_DBG_SMBFS_HANDLE_LEASE_BREAK | DBG_FUNC_END,
                   error, 0, 0, 0, 0);

    return (error);
}

void
smbfs_lease_hash_add(vnode_t vp, uint64_t lease_key_hi, uint64_t lease_key_low,
                     uint32_t need_lock)
{
	//struct smbnode *np = NULL;
	struct g_lease_hash_head *lhpp;
	struct smb_lease *leasep = NULL;
	uint64_t hashval = 0;
	
	if (vp == NULL) {
		SMBERROR("vp is null \n");
		return;
	}
	//np = VTOSMB(vp);
	
	/* Create a new lease entry */
    SMB_MALLOC_TYPE(leasep, struct smb_lease, Z_WAITOK_ZERO);
	
    leasep->lease_key_hi = lease_key_hi;
    leasep->lease_key_low = lease_key_low;
    leasep->vnode = vp;
	leasep->vid = vnode_vid(vp);
	
	hashval = lease_key_hi ^ lease_key_low;
	
	/* Add it into the hash table */
    if (need_lock) {
        lck_mtx_lock(&global_Lease_hash_lock);
    }
	
	lhpp = SMBFS_LEASE_HASH(hashval);
	
	LIST_INSERT_HEAD(lhpp, leasep, lease_hash);
	
    if (need_lock) {
        lck_mtx_unlock(&global_Lease_hash_lock);
    }
    
	return;
}

struct smb_lease *
smbfs_lease_hash_get(uint64_t lease_key_hi, uint64_t lease_key_low)
{
	struct g_lease_hash_head *lhpp;
	struct smb_lease *leasep = NULL;
	uint64_t hashval = 0;
	
	/* global_Lease_hash_lock MUST already be locked on entry */

	hashval = lease_key_hi ^ lease_key_low;
	
	lhpp = SMBFS_LEASE_HASH(hashval);

	LIST_FOREACH(leasep, lhpp, lease_hash) {
        if ((leasep->lease_key_hi != lease_key_hi) ||
            (leasep->lease_key_low != lease_key_low)) {
            /* Not the right entry */
            continue;
        }
        else {
            /* Found the correct lease */
            break;
        }
	}

	return (leasep);
}

void
smbfs_lease_hash_remove(vnode_t vp, struct smb_lease *in_leasep,
                        uint64_t lease_key_hi, uint64_t lease_key_low)
{
#pragma unused(vp)
	struct smb_lease *leasep = NULL;
	int is_locked = 0;

	if (in_leasep == NULL) {
		/* Need to look up leasep using lease keys */
		lck_mtx_lock(&global_Lease_hash_lock);
		is_locked = 1;

		leasep = smbfs_lease_hash_get(lease_key_hi, lease_key_low);
	}
	else {
		/* global_Lease_hash_lock MUST already be locked for this case */
		leasep = in_leasep;
	}
	
	if (leasep == NULL ) {
		SMBWARNING("leasep is null \n");

		if (is_locked) {
			lck_mtx_unlock(&global_Lease_hash_lock);
		}
		return;
	}
	
	/* Remove the lease entry from hash table */
	if (leasep->lease_hash.le_prev) {
		LIST_REMOVE(leasep, lease_hash);
		leasep->lease_hash.le_prev = NULL;
	}
	
    /* Clear it out */
    leasep->lease_key_hi = 0;
    leasep->lease_key_low = 0;
    leasep->vnode = NULL;
    leasep->vid = 0;
    
	/* If we took the lock, unlock now */
	if (is_locked) {
		lck_mtx_unlock(&global_Lease_hash_lock);
	}
	
	/* Free the lease */
    SMB_FREE_TYPE(struct smb_lease, leasep);
	
	return;
}


#pragma mark - lockFID, sharedFID, BRL LockEntries

/*
 * AddRemoveByteRangeLockEntry
 *
 * Add .
 *
 * Return Values
 *    none
 */
void
AddRemoveByteRangeLockEntry(struct fileRefEntry *fileEntry, struct fileRefEntryBRL *fileBRLEntry,
                            int64_t offset, int64_t length, int8_t unLock, uint32_t lck_pid, vfs_context_t context)
{
    struct ByteRangeLockEntry *curr = NULL;
    struct ByteRangeLockEntry *prev = NULL;
    struct ByteRangeLockEntry *new = NULL;
    int32_t foundIt = 0;
    pid_t pid = 0;

    /* The BRL_lock must be locked on entry */
    
    if ((fileEntry == NULL) && (fileBRLEntry == NULL)){
        SMBERROR("fileEntry and fileBRLEntry are both NULL \n");
        return;
    }
    
    if (context == NULL) {
        SMBERROR("context is NULL \n");
        return;
    }
    pid = vfs_context_pid(context);

    if (fileEntry != NULL) {
        curr = fileEntry->lockList;
    }
    else {
        curr = fileBRLEntry->lockList;
    }
    
    if (unLock == 0) {
        /* Locking, so add a new ByteRangeLockEntry */
        SMB_MALLOC_TYPE(new, struct ByteRangeLockEntry, Z_WAITOK_ZERO);
        new->offset = offset;
        new->length = length;
        new->lck_pid = lck_pid;
        new->p_pid = pid;
        new->next = NULL;

        if (curr == NULL) {
            /* first entry is empty so use it */
            if (fileEntry != NULL) {
                fileEntry->lockList = new;
            }
            else {
                fileBRLEntry->lockList = new;
            }
        }
        else { /* find the last entry and add the new entry to the end of list */
            while (curr->next != NULL) {
                curr = curr->next;
            }
            curr->next = new;
        }
    }
    else {
        /* Unlocking, so remove a ByteRangeLockEntry */
        if (curr == NULL) {
            SMBWARNING("no entries found\n");
            return;
        }
        
        if ((curr->offset == offset) && (curr->length == length)) {
            /* first entry is it, so remove it from the head */
            if (fileEntry != NULL) {
                fileEntry->lockList = curr->next;
            }
            else {
                fileBRLEntry->lockList = curr->next;
            }
            SMB_FREE_TYPE(struct ByteRangeLockEntry, curr);
        }
        else {
            /* Not the first entry, so search the rest of them */
            prev = curr;
            curr = curr->next;
            while (curr != NULL) {
                if ((curr->offset == offset) && (curr->length == length)) {
                    foundIt = 1;
                    /* found it so remove it */
                    prev->next = curr->next;
                    SMB_FREE_TYPE(struct ByteRangeLockEntry, curr);
                    break;
                }
                prev = curr;
                curr = curr->next;
            }

            if (foundIt == 0) {
                SMBWARNING ("offset 0x%llx/0x%llx not found \n",
                            offset, length);
            }
        }
    }
}

/*
 * CheckByteRangeLockEntry
 *
 * Return Values (exact_match == 0)
 *    TRUE    - A BRL falls inside the offset/length
 *    FALSE   - No BRLs falls inside the offset/length
 *
 * Return Values (exact_match == 1). Used for preflighting unlock requests
 *    TRUE    - A BRL matches the offset/length
 *    FALSE   - No BRLs matches the offset/length
 */
int
CheckByteRangeLockEntry(struct fileRefEntry *fileEntry, struct fileRefEntryBRL *fileBRLEntry,
                        int64_t offset, int64_t length, int exact_match, pid_t *lock_holder)
{
    struct ByteRangeLockEntry *currBRL = NULL;

    /* The BRL_lock must be locked on entry */

    if ((fileEntry == NULL) && (fileBRLEntry == NULL)){
        SMBERROR("fileEntry and fileBRLEntry are both NULL \n");
        return(FALSE);
    }
    
    SMB_LOG_FILE_OPS("offset <%lld> length <%lld> \n", offset, length);

    if (fileEntry != NULL) {
        currBRL = fileEntry->lockList;
    }
    else {
        currBRL = fileBRLEntry->lockList;
    }
    
    while (currBRL != NULL) {
        if (exact_match == 0) {
            /*
             * Have to be careful here as offset 5, len 5 means bytes 5-9 are
             * locked and not bytes 5-10.
             */
            SMB_LOG_FILE_OPS("Checking currBRL offset <%lld> length <%lld> \n",
                             currBRL->offset, currBRL->length);
            
            /* Is BRL start inside offset/length */
            if ((currBRL->offset >= offset) &&
                (currBRL->offset < (offset + length))) {
                *lock_holder = currBRL->p_pid;
                return(TRUE);
            }
            
            /* Is BRL end inside of the offset/length? */
            if (((currBRL->offset + currBRL->length) > offset) &&
                ((currBRL->offset + currBRL->length) <= (offset + length))) {
                *lock_holder = currBRL->p_pid;
                return(TRUE);
            }
        }
        else {
            /* Does BRL match offset/length */
            SMB_LOG_FILE_OPS("Checking currBRL offset <%lld> length <%lld> for exact match \n",
                             currBRL->offset, currBRL->length);

            if ((currBRL->offset = offset) && (currBRL->length = length)) {
                *lock_holder = currBRL->p_pid;
                return(TRUE);
            }
        }
        
        currBRL = currBRL->next;
    }

    return FALSE;
}

/*
 * FindByteRangeLockEntry
 *
 * Return Values
 *
 *    TRUE    - We have this ranged locked already
 *    FALSE   - We don't have this range locked
 */
int
FindByteRangeLockEntry(struct fileRefEntry *fileEntry, struct fileRefEntryBRL *fileBRLEntry,
                       int64_t offset, int64_t length, vfs_context_t context)
{
    struct ByteRangeLockEntry *curr = NULL;
    pid_t pid = 0;
    
    /* The BRL_lock must be locked on entry */

    if ((fileEntry == NULL) && (fileBRLEntry == NULL)) {
        SMBERROR("fileEntry and fileBRLEntry are both NULL \n");
        return(FALSE);
    }
    
    if (context == NULL) {
        SMBERROR("context is NULL \n");
        return(FALSE);
    }
    pid = vfs_context_pid(context);
    
    
    if (fileEntry != NULL) {
        curr = fileEntry->lockList;
    }
    else {
        curr = fileBRLEntry->lockList;
    }
    
    while (curr) {
        if ((curr->offset == offset) &&
            (curr->length == length) &&
            (curr->p_pid == pid)) {
            return TRUE;
        }
        curr = curr->next;
    }
    return FALSE;
}
    
/*
 * FindFileRef
 *
 * Byte range locks are tracked by pid. SMBClient always uses Exclusive locks
 * meaning that only the lock owner can do IO on the locked range.
 *
 * flock() is tracked by fd and NOT by pid! flock() will take out an EXCLUSIVE
 * byte range lock over the entire file.
 *
 * Open/Close are not tracked by pid. We assume the local VFS has already
 * verified that the IO is allowed on the fd used in user space.
 *
 * First check if flock() has been done on the file and if so, use it.
 * Because flock() is tracked by fd and not by pid, other fds that did not
 * do the flock() locally will get access to the file. This is a case where
 * open(O_EXLOCK) is superior to using flock() as it will prevent other local
 * access to the file.
 *
 * If proc p is non null, seach the local byte range locks to see if the
 * IO's offset/length falls inside one of the existing locks.
 * 1. If the IO falls inside a BRL and the in_pid matches the pid that did the
 *    brl, then use that FID to do the IO.
 * 2. If the IO falls insida a BRL and in_pid does not match the pid, then
 *    return EACCESS.
 *
 * If still no match, then find any FID that has correct access.
 *
 * Return Values
 *    EACCES   local byte range lock would fail this request
 *    0        IO is allowed. *fid holds SMBFID to do the IO on
 */
int32_t
FindFileRef(vnode_t vp, proc_t p, uint16_t accessMode,
            int64_t offset, int64_t length, SMBFID *fid)
{
    struct smbnode *np;
    int i = 0, ret = 0;
    pid_t in_pid = -1, found_pid = -1;
    struct fileRefEntryBRL *fileBRLEntry = NULL;

    /* If we have no vnode then we are done. */
    if (!vp) {
        SMBERROR("vp is null? \n");
        return (EBADF);
    }

    np = VTOSMB(vp);
    
    if ((np->f_sharedFID_refcnt == 0) && (np->f_lockFID_refcnt == 0)) {
        SMBERROR_LOCK(np, "No open FIDs on <%s> \n", np->n_name);
        return(EBADF);
    }

    SMB_LOG_FILE_OPS_LOCK(np, "Enter with offset <%lld> length <%lld> proc <%s> accessMode <0x%x> on <%s> \n",
                          offset, length, (p == NULL) ? "p is null" : "non null",
                          accessMode, np->n_name);

    /*
     * Is there a flock() on the file and does its access mode match?
     * flock() is always done on the sharedFID
     */
    if ((np->f_sharedFID_refcnt > 0) &&
        (np->f_smbflock) &&
        (accessMode & np->f_smbflock->access_mode)) {
        /*
         * flock() acts more like open(O_EXLOCK/O_SHLOCK) than fcntl(BRL)
         * in that its tracked by fd and NOT by pid.
         *
         * If there is a flock present, then assume the local VFS already
         * decided this process can access the file, so find the sharedFID
         * lockEntry using the flock's access_mode and let them use that fid.
         */
        fileBRLEntry = smbfs_find_lockEntry(vp, np->f_smbflock->access_mode);
        if (fileBRLEntry == NULL) {
            /* Paranoid check */
            SMBERROR_LOCK(np, "flock present, but no lockEntry found for <%s>? \n",
                          np->n_name);
            return(EBADF);
        }
        
        if (fileBRLEntry->refcnt <= 0) {
            /* Paranoid check */
            SMBERROR_LOCK(np, "flock present, lockEntry was found but not in use <%d> for <%s>? \n",
                          fileBRLEntry->refcnt, np->n_name);
            return(EBADF);
        }
        
        /*
         * Found a lock entry that is in use and matches the
         * accessMode, assume its the flocked FID.
         */
        *fid = fileBRLEntry->fid;
        return(0);
    }

    /* Only if we have a proc p will we search the BRLs */
    if (p != NULL) {
        in_pid = proc_pid(p);
        
        SMB_LOG_FILE_OPS_LOCK(np, "Check BRL with offset <%lld> length <%lld> pid <%d> on <%s> \n",
                              offset, length, in_pid, np->n_name);

        /*
         * Check sharedFID first for any BRLs that cover this offset/length
         * Harder case as there are 3 BRL FIDs to check.
         * This is the more likely case to be in use for BRLs though.
         */
        if (np->f_sharedFID_refcnt > 0) {
            for (i = 0; i < 3; i++) {
                /* Is this lockEntry in use? */
                if (np->f_sharedFID.lockEntries[i].refcnt > 0) {
                    /* In use, check its BRLs */
                    
                    lck_mtx_lock(&np->f_sharedFID_BRL_lock);

                    ret = CheckByteRangeLockEntry(NULL, &np->f_sharedFID.lockEntries[i],
                                                  offset, length, 0, &found_pid);
                    
                    lck_mtx_unlock(&np->f_sharedFID_BRL_lock);

                    if (ret == TRUE) {
                        SMB_LOG_FILE_OPS_LOCK(np, "CheckByteRangeLockEntry found match in index <%d> with in_pid <%d> found_pid <%d> for <%s> \n",
                                              i, in_pid, found_pid, np->n_name);
                       /* Offset/len is inside one of these BRLs, check pid */
                        if (in_pid != found_pid) {
                            /* Another process owns the lock, so error */
                            return(EACCES);
                        }
                        else {
                            /* This process did the lock, so use this FID */
                            *fid = np->f_sharedFID.lockEntries[i].fid;
                            return(0);
                        }
                    }
                } /* lockEntry refcnt > 0 */
            } /* for loop */
        } /* sharedFID refcnt > 0 */
        
        /*
         * Check lockFID for any BRLs that cover this offset/length
         * Easy case as there is only one FID
         * Less likely scenario as mixing open deny modes with BRLs
         */
        if (np->f_lockFID_refcnt > 0) {
            /* In use, check its BRLs */
            lck_mtx_lock(&np->f_lockFID_BRL_lock);

            ret = CheckByteRangeLockEntry(&np->f_lockFID, NULL,
                                          offset, length, 0, &found_pid);
            
            lck_mtx_unlock(&np->f_lockFID_BRL_lock);

            if (ret == TRUE) {
                /* Offset/len is inside one of these BRLs, check pid */
                if (in_pid != found_pid) {
                    /* Another process owns the lock, so error */
                    return(EACCES);
                }
                else {
                    /* This process did the lock, so use this FID */
                    *fid = np->f_lockFID_fid;
                    return(0);
                }
            }
        }
    } /* p != NULL */
    
    /*
     * At this point, either p == NULL or no BRLs were found. Just find any
     * FID that will allow this IO to work on.
     *
     * Just need to check sharedFID (always a super set of BRL lockEntries
     * or the lockFID
     */
    SMB_LOG_FILE_OPS_LOCK(np, "Check sharedFID refcnt <%d> f_sharedFID_fid <0x%llx> sharedFID_accessMode <0x%x> accessMode <0x%x> on <%s> \n",
                          np->f_sharedFID_refcnt,  np->f_sharedFID_fid,
                          np->f_sharedFID_accessMode, accessMode,
                          np->n_name);

    if ((np->f_sharedFID_refcnt > 0) &&
        (np->f_sharedFID_accessMode & accessMode)) {
        /* This FID will work */
        *fid = np->f_sharedFID_fid;
        return(0);
    }

    SMB_LOG_FILE_OPS_LOCK(np, "Check lockFID refcnt <%d> f_lockFID_fid <0x%llx> lockFID_accessMode <0x%x> accessMode <0x%x> on <%s> \n",
                          np->f_lockFID_refcnt, np->f_lockFID_fid, np->f_lockFID_accessMode, accessMode,
                          np->n_name);

    if ((np->f_lockFID_refcnt > 0) &&
        (np->f_lockFID_accessMode & accessMode)) {
        /* This FID will work */
        *fid = np->f_lockFID_fid;
        return(0);
    }

    /* Odd, we did not find any FID to use */
    SMBERROR_LOCK(np, "No useable FIDs found on <%s> \n", np->n_name);
    return(EBADF);
}

/*
 * smbfs_clear_lockEntries() is used to clear the sharedFID BRL lock entries
 */
int smbfs_clear_lockEntries(vnode_t vp, struct fileRefEntryBRL *fileBRLEntry)
{
    struct smbnode *np = NULL;
    struct ByteRangeLockEntry *currBRL = NULL, *nextBRL = NULL;
    int i = 0;

    if (vp == NULL) {
        SMBERROR("vp is null \n");
        return(EINVAL);
    }
    
    np = VTOSMB(vp);
    
    if (fileBRLEntry == NULL) {
        /* Clear all lock entries */

        for (i = 0; i < 3; i++) {
            lck_mtx_lock(&np->f_sharedFID_BRL_lock);

            currBRL = np->f_sharedFID_lockEntries[i].lockList;
            while (currBRL != NULL) {
                nextBRL = currBRL->next;        /* save next in list */
                SMB_FREE_TYPE(struct ByteRangeLockEntry, currBRL); /* free current entry */
                currBRL = nextBRL;              /* and on to the next */
            }
            np->f_sharedFID_lockEntries[i].lockList = NULL;
            
            lck_mtx_unlock(&np->f_sharedFID_BRL_lock);

            np->f_sharedFID_lockEntries[i].refcnt = 0;
            np->f_sharedFID_lockEntries[i].fid = 0;
            np->f_sharedFID_lockEntries[i].accessMode = 0;
            
            /* clear the durable handle */
            smbnode_dur_handle_lock(&np->f_sharedFID_lockEntries[i].dur_handle, smbfs_clear_lockEntries);

            np->f_sharedFID_lockEntries[i].dur_handle.flags = 0;
            np->f_sharedFID_lockEntries[i].dur_handle.fid = 0;
            np->f_sharedFID_lockEntries[i].dur_handle.timeout = 0;
            bzero(np->f_sharedFID_lockEntries[i].dur_handle.create_guid,
                  sizeof(np->f_sharedFID_lockEntries[i].dur_handle.create_guid));

            smbnode_dur_handle_unlock(&np->f_sharedFID_lockEntries[i].dur_handle);
        }
    }
    else {
        /* Clear just the passed in entry */
        lck_mtx_lock(&np->f_sharedFID_BRL_lock);

        currBRL = fileBRLEntry->lockList;
        while (currBRL != NULL) {
            nextBRL = currBRL->next;        /* save next in list */
            SMB_FREE_TYPE(struct ByteRangeLockEntry, currBRL); /* free current entry */
            currBRL = nextBRL;              /* and on to the next */
        }
        fileBRLEntry->lockList = NULL;
        
        lck_mtx_unlock(&np->f_sharedFID_BRL_lock);

        fileBRLEntry->refcnt = 0;
        fileBRLEntry->fid = 0;
        fileBRLEntry->accessMode = 0;
        
        /* clear the durable handle */
        smbnode_dur_handle_lock(&fileBRLEntry->dur_handle, smbfs_clear_lockEntries);

        fileBRLEntry->dur_handle.flags = 0;
        fileBRLEntry->dur_handle.fid = 0;
        fileBRLEntry->dur_handle.timeout = 0;
        bzero(fileBRLEntry->dur_handle.create_guid,
              sizeof(fileBRLEntry->dur_handle.create_guid));

        smbnode_dur_handle_unlock(&fileBRLEntry->dur_handle);
    }
    
    return(0);
}

struct fileRefEntryBRL * smbfs_find_lockEntry(vnode_t vp, uint16_t accessMode)
{
    struct smbnode *np = NULL;
    struct fileRefEntryBRL *fileBRLEntry = NULL;

    /*
     * This function finds which sharedFID lockEntry should be used
     * for the accessMode
     */

    if (vp == NULL) {
        SMBERROR("vp is null? \n");
        return(NULL);
    }
    
    np = VTOSMB(vp);

    /* Find the lockEntry to use for this accessMode */
    if (accessMode == (kAccessRead | kAccessWrite)) {
        fileBRLEntry = &np->f_sharedFID.lockEntries[0];
    }
    else {
        if (accessMode == kAccessRead) {
            fileBRLEntry = &np->f_sharedFID.lockEntries[1];
        }
        else {
            if (accessMode == kAccessWrite) {
                fileBRLEntry = &np->f_sharedFID.lockEntries[2];
            }
            else {
                SMBERROR_LOCK(np, "Invalid accessMode <0x%x> for <%s> \n",
                              accessMode, np->n_name);
                return(NULL);
            }
        }
    }
    
    return(fileBRLEntry);
}


int smbfs_free_locks_on_close(struct smb_share *share, vnode_t vp,
                              int openMode, vfs_context_t context)
{
    int error = 0, warning = 0;
    struct smbnode *np = NULL;
    struct fileRefEntry *fileEntry = NULL;
    struct fileRefEntryBRL *fileBRLEntry = NULL;
    struct ByteRangeLockEntry *curr = NULL;
    int32_t found_one = 0;
    pid_t pid = 0;
    int64_t offset = 0, length = 0;
    uint32_t lck_pid = 1;
    uint16_t accessMode = 0;
    SMBFID fid = 0;
    
    /*
     * A close operation will free any remaining locks, but we attempt to do
     * the unlock in this function because
     * 1. For sharedFID lockEntry, there may be other brls from other processes
     *    using that lockEntry, so the lockEntry FID wont get closed and thus
     *    the brls will remain intact.
     * 2. If the FID has a handle lease and the close gets deferred, then the
     *    brl will remain intact.
     */
    
    /* Setup accessMode */
    if (openMode & FREAD) {
        accessMode |= kAccessRead;
    }

    if (openMode & FWRITE) {
        accessMode |= kAccessWrite;
    }

    if (vp == NULL) {
        SMBERROR("vp is null? \n");
        return(EINVAL);
    }
    np = VTOSMB(vp);

    SMB_LOG_FILE_OPS_LOCK(np, "openMode <0x%x> for <%s> \n",
                          openMode, np->n_name);

    if (context == NULL) {
        SMBERROR("context is null? \n");
        return(EINVAL);
    }
    pid = vfs_context_pid(context);

    /* Does the flock() need to be unlocked? */
    if ((np->f_smbflock != NULL) &&
        (np->f_smbflock_pid == pid)) {
        /*
         * flocked file, pids match, so there must be a sharedFID lockEntry
         * that is in use for flock.
         */
        fileBRLEntry = smbfs_find_lockEntry(vp, accessMode);
        if ((fileBRLEntry == NULL) || (fileBRLEntry->refcnt <= 0)) {
            /* If did not find lockEntry and its read, try with read/write */
            if (!(accessMode & kAccessWrite)) {
                SMB_LOG_FILE_OPS_LOCK(np, "Adding write and searching for flock again for <%s> \n",
                                      np->n_name);
                fileBRLEntry = smbfs_find_lockEntry(vp, accessMode | kAccessWrite);
            }
        }
        
        if ((fileBRLEntry == NULL) || (fileBRLEntry->refcnt <= 0)) {
            SMBERROR_LOCK(np, "flock present, but lockEntry is <%s> or not in use <%d> for <%s>? \n",
                          (fileBRLEntry == NULL) ? "null" : "non null",
                          (fileBRLEntry == NULL) ? 0 : fileBRLEntry->refcnt,
                          np->n_name);
            /* Ignore this error and continue on */
        }
        else {
            /* Try to unlock the flock */
            warning = smbfs_smb_lock(share, SMB_LOCK_RELEASE, fileBRLEntry->fid,
                                     lck_pid, 0, np->f_smbflock->len, 0,
                                     context);
            if (warning) {
                SMBERROR_LOCK(np, "smbfs_smb_lock failed <%d> to unlock flock <%d:%lld> on <%s> \n",
                              warning, 0, np->f_smbflock->len, np->n_name);
            }
        }

        /* Remove the flock */
        SMB_FREE_TYPE(struct smbfs_flock, np->f_smbflock);
        np->f_smbflock = NULL;
    }

    /*
     * Now check for regular fcntl(byte range locks)
     */

    if (openMode & FWASLOCKED) {
        /* lockFID, so easy case as there is only one open */
        if (np->f_lockFID_refcnt <= 0) {
            /*
             * Hmm, must have been the flock() that set FWASLOCKED
             * Assume there is nothing to unlock.
             */
            error = 0;
            goto exit;
        }
        
        fileEntry = &np->f_lockFID;
    }
    else {
        /* sharedFID, so have to check the lockEntries */
        if (np->f_sharedFID_refcnt <= 0) {
            SMBERROR_LOCK(np, "sharedFID has no open files for <%s> <%d> \n",
                          np->n_name, np->f_sharedFID_refcnt);
            error = EBADF;
            goto exit;
        }
        
        fileBRLEntry = smbfs_find_lockEntry(vp, accessMode);
        if (fileBRLEntry == NULL) {
            SMBERROR_LOCK(np, "lockEntry not found for accessMode <0x%x> openMode <0x%x> for <%s> \n",
                          accessMode, openMode, np->n_name);
            error = EBADF;
            goto exit;
        }
    }

    /*
     * Search the lockList for any locks that match the pid and attempt to
     * unlock and remove them.
     */
    if (openMode & FWASLOCKED) {
        lck_mtx_lock(&np->f_lockFID_BRL_lock);
    }
    else {
        lck_mtx_lock(&np->f_sharedFID_BRL_lock);
    }

    do {
        found_one = 0;
        offset = 0;
        length = 0;
        lck_pid = 0;
        //fid = 0;
        
        if (openMode & FWASLOCKED) {
            curr = fileEntry->lockList;
        }
        else {
            curr = fileBRLEntry->lockList;
        }
        
        while (curr) {
            if (curr->p_pid == pid) {
                found_one = 1;
                offset = curr->offset;
                length = curr->length;
                lck_pid = curr->lck_pid;
                break;
            }
            curr = curr->next;
        }

        if (found_one) {
            if (openMode & FWASLOCKED) {
                fid = np->f_lockFID_fid;
            }
            else {
                fid = fileBRLEntry->fid;
            }
            
            SMB_LOG_FILE_OPS_LOCK(np, "Unlocking <%lld:%lld> on <%s> \n",
                                  offset, length, np->n_name);

            /* Try to unlock this brl */
            warning = smbfs_smb_lock(share, SMB_LOCK_RELEASE, fid,
                                     lck_pid, offset, length, 0,
                                     context);
            if (warning) {
                SMBERROR_LOCK(np, "smbfs_smb_lock failed <%d> to unlock <%lld:%lld> on <%s> \n",
                              warning, offset, length, np->n_name);
            }
            
            /* Remove this lock from either lockFID or sharedFID lockEntry */
            AddRemoveByteRangeLockEntry(fileEntry, fileBRLEntry, offset, length,
                                        1, lck_pid, context);
        }
    } while (found_one == 1);
    
    if (openMode & FWASLOCKED) {
        lck_mtx_unlock(&np->f_lockFID_BRL_lock);
    }
    else {
        lck_mtx_unlock(&np->f_sharedFID_BRL_lock);
    }

    /* For sharedFID lockEntry, can it be closed now? */
    if ((fileBRLEntry != NULL) &&
        (fileBRLEntry->lockList == NULL) &&
        (fileBRLEntry->fid != 0)) {
        SMB_LOG_FILE_OPS_LOCK(np, "Closing lockEntry accessMode <0x%x> on <%s> \n",
                              fileBRLEntry->accessMode, np->n_name);
        warning = smbfs_smb_close(share, fileBRLEntry->fid, context);
        if (warning) {
            SMBWARNING_LOCK(np, "Error %d on closing shared lockEntry %s during close \n",
                            warning, np->n_name);
        }
        
        /* Clear out this lockEnry */
        smbfs_clear_lockEntries(vp, fileBRLEntry);
    }
    
exit:
    return(error);
}

/*
 * smbfs_get_lockEntry() find the lockEntry entry matching the accessMode
 * [0] - Used for Read/Write FIDs
 * [1] - Used for Read FIDs
 * [2] - Used for Write FIDs
 *
 * If the entry is currently in use (ie has an open FID), then return that entry
 * If entry is not in use, then do an open to get a FID and durable handle
 *
 * Always returns a lockEntry to use for byte range locking
 */
int smbfs_get_lockEntry(struct smb_share *share, vnode_t vp,
                        uint16_t accessMode,
                        struct fileRefEntryBRL **ret_entry,
                        vfs_context_t context)
{
    int error = 0, warning = 0;
    struct smbnode *np = NULL;
    struct fileRefEntryBRL *fileBRLEntry = NULL;
    uint32_t rights = 0;
    uint32_t shareMode = NTCREATEX_SHARE_ACCESS_ALL;
    struct smbfattr *fap = NULL;
    int do_create = 0;
    uint32_t disp = 0;
    SMBFID fid = 0;
    struct smb2_lease temp_lease = {0};
    struct smb2_durable_handle temp_dur_hndl = {0};
    int lease_need_free = 0;
    int dur_need_free = 0;
    struct smb2_dur_hndl_and_lease dur_hndl_lease = {0};

    /* The BRL_lock must be locked on entry */

    if (vp == NULL) {
        SMBERROR("vp is null? \n");
        return(EINVAL);
    }
    
    np = VTOSMB(vp);

    /*
     * This is wrong, someone has to call open() before doing a
     * byte range lock
     */
    if (np->f_sharedFID_refcnt <= 0) {
        SMBERROR_LOCK(np, "sharedFID has no open files for <%s> <%d> \n",
                      np->n_name, np->f_sharedFID_refcnt);
        error = EBADF;
        goto exit;
    }

    /* Find the lockEntry to use for this accessMode */
    fileBRLEntry = smbfs_find_lockEntry(vp, accessMode);
    if (fileBRLEntry == NULL) {
        SMBERROR_LOCK(np, "lockEntry not found for accessMode <0x%x> for <%s> \n",
                      accessMode, np->n_name);
        error = EBADF;
        goto exit;
    }

    /* refcnt used by vnop_ioctl(BRL) and vnop_advlock() */

    /* Is this entry already open and in use? */
    if (fileBRLEntry->refcnt > 0) {
        /* Already in use, just return it */
        error = 0;
        *ret_entry = fileBRLEntry;
        goto exit;
    }
    
    /* Need to open a FID for this access mode */
    if (accessMode & kAccessRead) {
        rights |= SMB2_FILE_READ_DATA;
    }
    
    if (accessMode & kAccessWrite) {
        rights |= SMB2_FILE_APPEND_DATA | SMB2_FILE_WRITE_DATA;
    }

    SMB_MALLOC_TYPE(fap, struct smbfattr, Z_WAITOK_ZERO);
    if (fap == NULL) {
        SMBERROR("SMB_MALLOC_TYPE failed\n");
        error = ENOMEM;
        goto exit;
    }

    /* Always request a durable handle which includes a lease */
    smb2_lease_init(share, NULL, np, 0, &temp_lease, 1);
    temp_lease.req_lease_state = smbfs_get_req_lease_state(rights);
    dur_hndl_lease.leasep = &temp_lease;
    lease_need_free = 1;

    smb2_dur_handle_init(share, SMB2_DURABLE_HANDLE_REQUEST, &temp_dur_hndl, 1);
    dur_hndl_lease.dur_handlep = &temp_dur_hndl;
    dur_need_free = 1;

    /* lease should already be in the tables */
    
    if (np->n_flag & N_ISRSRCFRK) {
        disp = FILE_OPEN_IF;
        do_create = TRUE;
    }
    else {
        disp = FILE_OPEN;
        do_create = FALSE;
    }

    SMB_LOG_FILE_OPS_LOCK(np, "New open for lockEntry, accessMode <0x%x> for <%s> \n",
                          accessMode, np->n_name);

    error = smbfs_smb_ntcreatex(share, np,
                                rights, shareMode, VREG,
                                &fid, NULL, 0,
                                disp, FALSE, fap,
                                do_create, &dur_hndl_lease,
                                NULL, NULL,
                                context);
    if (error != 0) {
        /* Failed to open the file, so exit */
        goto exit;
    }

    /*
     * Successfuly opened a file
     * Did we ask for and get a lease?
     */
    smbnode_lease_lock(&temp_lease, smbfs_get_lockEntry);

    if (temp_lease.flags & SMB2_LEASE_GRANTED) {
        /* Update the vnode lease from the temp lease */
        warning = smbfs_add_update_lease(share, vp, &temp_lease, SMBFS_LEASE_UPDATE, 1,
                                         "NewLockEntry");
        if (warning) {
            /* Shouldnt ever happen */
            SMBERROR("smbfs_add_update_lease update failed %d\n", warning);
        }
    }
    
    smbnode_lease_unlock(&temp_lease);

    /* Update the lockEntry */
    fileBRLEntry->refcnt += 1;
    fileBRLEntry->fid = fid;
    fileBRLEntry->accessMode = accessMode;
    fileBRLEntry->rights = rights;

    /* Did we ask for and get a durable handle? */
    smbnode_dur_handle_lock(&temp_dur_hndl, smbfs_get_lockEntry);
    
    if (temp_dur_hndl.flags & (SMB2_DURABLE_HANDLE_GRANTED | SMB2_PERSISTENT_HANDLE_GRANTED)) {
        /* Copy the temp dur handle info into the entry dur handle */
        smbnode_dur_handle_lock(&fileBRLEntry->dur_handle, smbfs_get_lockEntry);

        fileBRLEntry->dur_handle.flags = temp_dur_hndl.flags;
        fileBRLEntry->dur_handle.fid = fid;
        fileBRLEntry->dur_handle.timeout = temp_dur_hndl.timeout;
        memcpy(fileBRLEntry->dur_handle.create_guid, temp_dur_hndl.create_guid,
               sizeof(fileBRLEntry->dur_handle.create_guid));
        
        smbnode_dur_handle_unlock(&fileBRLEntry->dur_handle);
    }

    smbnode_dur_handle_unlock(&temp_dur_hndl);
    
    *ret_entry = fileBRLEntry;
    
exit:
    if (fap != NULL) {
        SMB_FREE_TYPE(struct smbfattr, fap);
    }
    
    /* Free the temp lease */
    if (lease_need_free) {
        smb2_lease_free(&temp_lease);
        //lease_need_free = 0;
    }

    /* Free the temp durable handle */
    if (dur_need_free) {
        smb2_dur_handle_free(&temp_dur_hndl);
        //dur_need_free = 0;
    }

    return(error);
}
