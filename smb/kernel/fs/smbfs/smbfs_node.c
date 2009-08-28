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

#include <libkern/crypto/md5.h>

#include <sys/kauth.h>

#include <sys/smb_apple.h>
#include <sys/mchain.h>
#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_subr.h>

#include <fs/smbfs/smbfs.h>
#include <fs/smbfs/smbfs_node.h>
#include <fs/smbfs/smbfs_subr.h>

#define	SMBFS_NOHASH(smp, hval)	(&(smp)->sm_hash[(hval) & (smp)->sm_hashlen])
#define	smbfs_hash_lock(smp)	(lck_mtx_lock((smp)->sm_hashlock))
#define	smbfs_hash_unlock(smp)	(lck_mtx_unlock((smp)->sm_hashlock))

extern vnop_t **smbfs_vnodeop_p;

MALLOC_DEFINE(M_SMBNODE, "SMBFS node", "SMBFS vnode private part");
MALLOC_DEFINE(M_SMBNODENAME, "SMBFS nname", "SMBFS node name");



#define	FNV_32_PRIME ((u_int32_t) 0x01000193UL)
#define	FNV1_32_INIT ((u_int32_t) 33554467UL)

#define isdigit(d) ((d) >= '0' && (d) <= '9')

/*
 * See if this is one of those faked up symbolic link. This is Conrad and Steve
 * French method for storing and reading symlinks on Window Servers.
 */
static int 
smb_check_for_windows_symlink(struct smb_share *ssp, struct smbnode *np, 
								  int *symlen, vfs_context_t context)
{
	uio_t uio;
	MD5_CTX md5;
	char m5b[SMB_SYMMD5LEN];
	u_int32_t state[4];
	int len = 0;
	unsigned char *sb, *cp;
	u_int16_t	fid = 0;
	int error, cerror;
	
	error = smbfs_smb_tmpopen(np, SA_RIGHT_FILE_READ_DATA, context, &fid);
	if (error)
		return error;
	
	MALLOC(sb, void *, (size_t)np->n_size, M_TEMP, M_WAITOK);
	uio = uio_create(1, 0, UIO_SYSSPACE, UIO_READ);
	if (uio) {
		uio_addiov(uio, CAST_USER_ADDR_T(sb), np->n_size);
		error = smb_read(ssp, fid, uio, context);
		uio_free(uio);		
	} else
		error = ENOMEM;
	cerror = smbfs_smb_tmpclose(np, fid, context);
	if (cerror)
		SMBWARNING("error %d closing fid %d file %s\n", cerror, fid, np->n_name);
	if (!error && !bcmp(sb, smb_symmagic, SMB_SYMMAGICLEN)) {
		for (cp = &sb[SMB_SYMMAGICLEN]; cp < &sb[SMB_SYMMAGICLEN+SMB_SYMLENLEN-1]; cp++) {
			if (!isdigit(*cp))
				break;
			len *= 10;
			len += *cp - '0';
		}
		cp++; /* skip newline */
		if ((cp != &sb[SMB_SYMMAGICLEN+SMB_SYMLENLEN]) || (len > (int)(np->n_size - SMB_SYMHDRLEN))) {
			SMBWARNING("bad symlink length\n");
			error = ENOENT; /* Not a faked up symbolic link */
		} else {
			MD5Init(&md5);
			MD5Update(&md5, &sb[SMB_SYMHDRLEN], len);
			MD5Final((u_char *)state, &md5); 
			(void)snprintf(m5b, sizeof(m5b), "%08x%08x%08x%08x",
							htobel(state[0]), htobel(state[1]), htobel(state[2]), htobel(state[3]));
			if (bcmp(cp, m5b, SMB_SYMMD5LEN-1)) {
				SMBWARNING("bad symlink md5\n");
				error = ENOENT; /* Not a faked up symbolic link */
			} else {
				*symlen = len;
				error = 0;
			}
		}
	} else
		error = ENOENT; /* Not a faked up symbolic link */
	FREE(sb, M_TEMP);
	return error;
}

/*
 * Lock a node
 */
int smbnode_lock(struct smbnode *np, enum smbfslocktype locktype)
{
	if (locktype == SMBFS_SHARED_LOCK)
		lck_rw_lock_shared(&np->n_rwlock);
	else
		lck_rw_lock_exclusive(&np->n_rwlock);

	np->n_lockState = locktype;
	
	/*
	 * Skip cnodes that no longer exist (were deleted).
	 */
	if ((locktype != SMBFS_RECLAIM_LOCK) && (np->n_flag & N_NOEXISTS)) {
		smbnode_unlock(np);
		return (ENOENT);
	}
#if 1	
	/* For Debugging... */
	if (locktype != SMBFS_SHARED_LOCK) {
		np->n_activation = (void *) current_thread();
	}
#endif
	return (0);
}


/*
 * Lock a pair of smbnodes
 *
 * If the two nodes are not the same then lock in the order they came in. The calling routine
 * should always put them in parent/child order.
 */
int smbnode_lockpair(struct smbnode *np1, struct smbnode *np2, enum smbfslocktype locktype)
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
void smbnode_unlock(struct smbnode *np)
{
	/* The old code called lck_rw_done which is a non supported kpi */
	if (np->n_lockState == SMBFS_SHARED_LOCK) {
		/* 
		 * Should we keep a counter and set n_lockState to zero when the 
		 * counter goes to zero? We would need to lock the counter in that
		 * case.
		 */
		lck_rw_unlock_shared(&np->n_rwlock);
	}
	else {
		/* Note: SMBFS_RECLAIM_LOCK is really SMBFS_EXCLUSIVE_LOCK */ 
		np->n_lockState = 0;
		lck_rw_unlock_exclusive(&np->n_rwlock);
	}
}

/*
 * Unlock a pair of cnodes.
 */
void smbnode_unlockpair(struct smbnode *np1, struct smbnode *np2)
{
	smbnode_unlock(np1);
	if (np2 != np1)
		smbnode_unlock(np2);
}

u_int32_t
smbfs_hash(const u_char *name, size_t nmlen)
{
	u_int32_t v;

	for (v = FNV1_32_INIT; nmlen; name++, nmlen--) {
		v *= FNV_32_PRIME;
		v ^= (u_int32_t)*name;
	}
	return v;
}


void smb_vhashrem(struct smbnode *np)
{
	smbfs_hash_lock(np->n_mount);
	if (np->n_hash.le_prev) {
		LIST_REMOVE(np, n_hash);
		np->n_hash.le_prev = NULL;
	}
	smbfs_hash_unlock(np->n_mount);
	return;
}

void smb_vhashadd(struct smbnode *np, u_int32_t hashval)
{
	struct smbnode_hashhead	*nhpp;
	
	smbfs_hash_lock(np->n_mount);
	nhpp = SMBFS_NOHASH(np->n_mount, hashval);
	LIST_INSERT_HEAD(nhpp, np, n_hash);
	smbfs_hash_unlock(np->n_mount);
	return;
	
}

u_char *
smbfs_name_alloc(const u_char *name, size_t nmlen)
{
	u_char *cp;

	nmlen++;
	cp = malloc(nmlen, M_SMBNODENAME, M_WAITOK);
	bcopy(name, cp, nmlen - 1);
	cp[nmlen - 1] = 0;
	return cp;
}

void
smbfs_name_free(const u_char *name)
{
	free(name, M_SMBNODENAME);
}

static vnode_t smb_hashget(struct smbmount *smp, struct smbnode *dnp, u_int32_t hashval, 
						   const u_char *name, size_t nmlen, u_int32_t node_flag, 
						   const char *sname)
{
	vnode_t	vp;
	struct smbnode_hashhead	*nhpp;
	struct smbnode *np;
	uint32_t vid;
	size_t snmlen = (sname) ? strnlen(sname, smp->sm_share->ss_maxfilenamelen+1) : 0;
loop:
	smbfs_hash_lock(smp);
	nhpp = SMBFS_NOHASH(smp, hashval);
	LIST_FOREACH(np, nhpp, n_hash) {

		/* 
		 * If we are only looking for a stream node then skip any other nodes. If we are look for a directory or 
		 * data node then skip any stream nodes.
		 */
		if ((np->n_flag & N_ISSTREAM) != node_flag)
			continue;		
		if ((np->n_parent != dnp) || (np->n_nmlen != nmlen) || (bcmp(name, np->n_name, nmlen) != 0))
			continue;
		/* If this is a stream make sure its the correct stream */
		if (np->n_flag & N_ISSTREAM) {
			DBG_ASSERT(sname);	/* Better be looking for a stream at this point */
			if ((np->n_snmlen != snmlen) || (bcmp(sname, np->n_sname, snmlen) != 0)) {
				SMBERROR("Currently we only support one stream and we found a second one? found %s looking for %s\n", 
						 np->n_sname, sname);
				continue;
			}
		}
		if (ISSET(np->n_flag, NALLOC)) {
			SET(np->n_flag, NWALLOC);
			(void)msleep((caddr_t)np, smp->sm_hashlock, PINOD|PDROP, "smb_ngetalloc", 0);
			goto loop;
		}
		if (ISSET(np->n_flag, NTRANSIT)) {
			SET(np->n_flag, NWTRANSIT);
			(void)msleep((caddr_t)np, smp->sm_hashlock, PINOD|PDROP, "smb_ngettransit", 0);
			goto loop;
		}
		vp = SMBTOV(np);
		vid = vnode_vid(vp);
		smbfs_hash_unlock(smp);
		if (vnode_getwithvid(vp, vid))
			return (NULL);

		/* unless the flag is set, always return the node locked */
		if ((smbnode_lock(np, SMBFS_EXCLUSIVE_LOCK)) != 0) {
			vnode_put(vp);
			return (NULL);
		}

		np->n_lastvop = smb_hashget;
#if 0
		/*
		 * %%%
		 *
		 * Would like to add support for these flags in the future.
		 *
		 * Skip cnodes that are not in the name space anymore
		 * we need to check again with the cnode lock held
		 * because we may have blocked acquiring the vnode ref
		 * or the lock on the cnode which would allow the node
		 * to be unlinked
		 */
		if (np->n_flag & (N_NOEXISTS | NMARKEDFORDLETE)) {
			smbnode_unlock(np);			
			vnode_put(vp);
			return (NULL);
		}			
#endif			
		return (vp);
	}
	smbfs_hash_unlock(smp);
	return (NULL);
}

/*
 * We need to test to see if the vtype changed on the node. We currently only support
 * three types of vnodes (VDIR, VLNK, and VREG). If the network transacition came
 * from one of the new UNIX extensions then we can just test to make sure the vtype
 * is the same. Otherwise we cannot tell the difference between a symbolic link and
 * a regular file at this point. So we just make sure it didn't change from a file
 * to a directory or vise versa. 
 */
static int node_vtype_changed(vnode_t vp, enum vtype node_vtype, struct smbfattr *fap)
{
	if (vnode_isnamedstream(vp))	/* Streams have no type so ignore them */
		return FALSE;
	/* UNIX extension call trust it */
	if (fap->fa_unix)
		return (fap->fa_vtype != node_vtype);
	else if ((node_vtype == VDIR) && (fap->fa_vtype == VDIR)) 
		return FALSE;
	else if ((node_vtype != VDIR) && (fap->fa_vtype != VDIR))
		return FALSE;
	return TRUE;
}

/* 
 * smbfs_nget
 *
 * When calling this routine remember if you get a vpp back and no error then
 * the smbnode is locked and you will need to unlock it.
 */
int smbfs_nget(struct mount *mp, vnode_t dvp, const char *name, size_t nmlen, 
			   struct smbfattr *fap, vnode_t *vpp, uint32_t cnflags, vfs_context_t context)
{
	struct smbmount *smp = VFSTOSMBFS(mp);
	struct smb_share *ssp = smp->sm_share;
	struct smbnode *np, *dnp;
	vnode_t vp;
	int error = 0;
	u_int32_t	hashval;
	struct vnode_fsparam vfsp;
	int locked = 0;
	struct componentname cnp;
	
	*vpp = NULL;
	if ((vfs_isforce(smp->sm_mp)))
		return ENXIO;
	if (smp->sm_rvp != NULL && dvp == NULL) {
		return EINVAL;
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
	/* If we are going to add it to the name cache, then make sure its the name on the server that gets used */
	bzero(&cnp, sizeof(cnp));
	cnp.cn_nameptr = (char *)name;
	cnp.cn_namelen = (int)nmlen;
	cnp.cn_flags = cnflags;
	
	MALLOC(np, struct smbnode *, sizeof *np, M_SMBNODE, M_WAITOK);
	hashval = smbfs_hash((u_char *)name, nmlen);
	if ((*vpp = smb_hashget(smp, dnp, hashval, (u_char *)name, nmlen, 0, NULL)) != NULL) {
		DBG_ASSERT(!vnode_isnamedstream(*vpp));
		if (fap && node_vtype_changed(*vpp, vnode_vtype(*vpp), fap)) {
			/* 
			 * The node we found has the wrong vtype. We need to remove this one and create the
			 * new entry. Purge the old node from the name cache, remove it from our hash table, 
			 * and clear its cache timer. 
			 */
			SMBWARNING("%s was %d now its %d\n", VTOSMB(*vpp)->n_name, vnode_vtype(*vpp), fap->fa_vtype);
			cache_purge(*vpp);
			smb_vhashrem(VTOSMB(*vpp));
			VTOSMB(*vpp)->attribute_cache_timer = 0;
			smbnode_unlock(VTOSMB(*vpp));	/* Release the smbnode lock */
			vnode_put(*vpp);
			/* Now fall through and create the node with the correct vtype */
			*vpp = NULL;
		} else {
			FREE(np, M_SMBNODE);
			/* update the attr_cache info, this is never a stream node */
			if (fap)
				smbfs_attr_cacheenter(*vpp, fap, FALSE, context);
			if (dvp && (cnp.cn_flags & MAKEENTRY))
				cache_enter(dvp, *vpp, &cnp);
			return (0);
			
		}
	}
	/*
	 * If we don't have node attributes, then it is an explicit lookup
	 * for an existing vnode.
	 */
	if (fap == NULL) {
		FREE(np, M_SMBNODE);
		return (ENOENT);
	}
	bzero(np, sizeof(*np));
	lck_rw_init(&np->n_rwlock, smbfs_rwlock_group, smbfs_lock_attr);
	lck_rw_init(&np->n_name_rwlock, smbfs_rwlock_group, smbfs_lock_attr);
	(void) smbnode_lock(np, SMBFS_EXCLUSIVE_LOCK);
	/* if we error out, don't forget to unlock this */
	locked = 1;
	np->n_lastvop = smbfs_nget;

	np->n_vnode = NULL;	/* redundant, but emphatic! */
	np->n_mount = smp;
	np->n_size = fap->fa_size;
	np->n_data_alloc = fap->fa_data_alloc;
	np->n_ino = fap->fa_ino;
	np->n_name = smbfs_name_alloc((u_char *)name, nmlen);
	np->n_nmlen = nmlen;
	/* Default to what we can do and Windows support */
	np->n_flags_mask = EXT_IMMUTABLE | EXT_HIDDEN | EXT_DO_NOT_BACKUP;
	np->n_uid = KAUTH_UID_NONE;
	np->n_gid = KAUTH_GID_NONE;
	SET(np->n_flag, NALLOC);
	smb_vhashadd(np, hashval);
	if (dvp) {
		np->n_parent = dnp;
		if (!vnode_isvroot(dvp)) {
			/* Make sure we can get the vnode, we could have an unmount about to happen */
			if (vnode_get(dvp) == 0) {
				if (vnode_ref(dvp) == 0)	/* If we can get a refcnt then mark the child */
					np->n_flag |= NREFPARENT;
				vnode_put(dvp);				
			}
		}
	}

	vfsp.vnfs_mp = mp;
	/*
	 * We now keep the type of node in fa_vtype. We fill this in ourself depending on information we obtain from
	 * the network or we determine from one of the create routines. In the future the UNIX extensions will 
	 * return this information and we can support all vtype if we want to, but for now we only support VDIR, VREG, 
	 * and VLNK.
	 */
	vfsp.vnfs_vtype = fap->fa_vtype;
	vfsp.vnfs_str = "smbfs";
	vfsp.vnfs_dvp = dvp;
	vfsp.vnfs_fsnode = np;
	vfsp.vnfs_cnp = (dvp && (cnp.cn_flags & MAKEENTRY)) ? &cnp : NULL;
	vfsp.vnfs_vops = smbfs_vnodeop_p;
	vfsp.vnfs_rdev = 0;	/* no VBLK or VCHR support */
	vfsp.vnfs_flags = (dvp && (cnp.cn_flags & MAKEENTRY)) ? 0 : VNFS_NOCACHE;
	vfsp.vnfs_markroot = (np->n_ino == 2);
	vfsp.vnfs_marksystem = 0;
	
	/*
	 * We are now safe to do lookups with the node. We need to be careful with the n_vnode field and we 
	 * should always check to make sure its not null before access that field. The current code always
	 * makes that check.
	 * 
	 * So if this is the root vnode then we need to make sure we can access it across network without any errors.
	 * We keep a reference on the root vnode so this only happens once at mount time.
	 *
	 * If this is a regular file then we need to see if its one of our special symlink files.
	 */
	if ((vfsp.vnfs_vtype == VDIR) && (dvp == NULL) && (smp->sm_rvp == NULL) && (np->n_ino == 2)) {
		error = smbfs_smb_lookup(np, NULL, NULL, fap, context);
		if (error)
			goto errout;			
	} else  if ((vfsp.vnfs_vtype == VREG) && (np->n_size == SMB_SYMLEN)) {
		int symlen = 0;
		
		DBG_ASSERT(dvp);
		if (smb_check_for_windows_symlink(ssp, np, &symlen, context) == 0) {
			vfsp.vnfs_vtype = VLNK;
			fap->fa_vtype = VLNK;
			np->n_size = symlen;
			np->n_flag |= NWINDOWSYMLNK;			
		}
	}
	vfsp.vnfs_filesize = np->n_size;
	error = vnode_create(VNCREATE_FLAVOR, (uint32_t)VCREATESIZE, &vfsp, &vp);
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
	
	/* Initialize the lock used for the open state, open deny list and resource size/timer */
	if (!vnode_isdir(vp)) {
		lck_mtx_init(&np->f_openStateLock, smbfs_mutex_group, smbfs_lock_attr);
		lck_mtx_init(&np->rfrkMetaLock, smbfs_mutex_group, smbfs_lock_attr);
		lck_mtx_init(&np->f_openDenyListLock, smbfs_mutex_group, smbfs_lock_attr);
	}

	lck_mtx_init(&np->f_ACLCacheLock, smbfs_mutex_group, smbfs_lock_attr);

	smbfs_attr_cacheenter(vp, fap, FALSE, context);	/* update the attr_cache info, this is never a stream node */

	*vpp = vp;
	CLR(np->n_flag, NALLOC);
        if (ISSET(np->n_flag, NWALLOC))
                wakeup(np);
	return 0;
errout:
	if (np->n_flag & NREFPARENT) {
		if (vnode_get(dvp) == 0) {
			vnode_rele(dvp);
			vnode_put(dvp);			
		}
		np->n_flag &= ~NREFPARENT;
	}
	
	smb_vhashrem(np);
	
	if (locked == 1)
		smbnode_unlock(np);	/* Release the smbnode lock */

	if (ISSET(np->n_flag, NWALLOC))
		wakeup(np);
		
	if (np->n_name)
		smbfs_name_free(np->n_name);

	FREE(np, M_SMBNODE);
	return error;
}

/* 
* smbfs_find_vgetstrm
 *
 * When calling this routine remember if you get a vpp back and no error then
 * the smbnode is locked and you will need to unlock it.
 */
vnode_t smbfs_find_vgetstrm(struct smbmount *smp, struct smbnode *np, const char *sname)
{
	u_int32_t	hashval;
	
	hashval = smbfs_hash(np->n_name, np->n_nmlen);
	return(smb_hashget(smp, np, hashval, np->n_name, np->n_nmlen, N_ISSTREAM, sname));
}

/* 
* smbfs_vgetstrm
 *
 * When calling this routine remember if you get a vpp back and no error then
 * the smbnode is locked and you will need to unlock it.
 */
int smbfs_vgetstrm(struct smbmount *smp, vnode_t vp, vnode_t *svpp, 
				   struct smbfattr *fap, const char *sname)
{
	struct smbnode *np, *snp;
	int error = 0;
	u_int32_t	hashval;
	struct vnode_fsparam vfsp;
	int locked = 0;
	
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
	
	MALLOC(snp, struct smbnode *, sizeof *snp, M_SMBNODE, M_WAITOK);
	hashval = smbfs_hash(np->n_name, np->n_nmlen);
	if ((*svpp = smb_hashget(smp, np, hashval, np->n_name, np->n_nmlen, N_ISSTREAM, sname)) != NULL) {
		FREE(snp, M_SMBNODE);
		/* 
		 * If this is the resource stream then the parents resource fork size has already been update. The calling routine
		 * aleady updated it. Remember that the parent is currently locked. smbfs_attr_cacheenter can lock the parent if we
		 * tell it to update the parent, so never tell it to update the parent in this routine. 
		 */
		smbfs_attr_cacheenter(*svpp, fap, FALSE, NULL);
		return (0);
	}
	bzero(snp, sizeof(*snp));
	lck_rw_init(&snp->n_rwlock, smbfs_rwlock_group, smbfs_lock_attr);
	lck_rw_init(&snp->n_name_rwlock, smbfs_rwlock_group, smbfs_lock_attr);
	(void) smbnode_lock(snp, SMBFS_EXCLUSIVE_LOCK);	
	locked = 1;
	snp->n_lastvop = smbfs_vgetstrm;
	
	snp->n_mount = smp;
	snp->n_size =  fap->fa_size;
	snp->n_data_alloc = fap->fa_data_alloc;
	snp->n_ino = np->n_ino;
	snp->n_name = smbfs_name_alloc(np->n_name, np->n_nmlen);
	snp->n_nmlen = np->n_nmlen;
	snp->n_flags_mask = np->n_flags_mask;
	snp->n_uid = np->n_uid;
	snp->n_gid = np->n_gid;
	snp->n_parent = np;
	/* Only a stream node can have a stream name */
	snp->n_snmlen = strnlen(sname, smp->sm_share->ss_maxfilenamelen+1);
	snp->n_sname = smbfs_name_alloc((u_char *)sname, snp->n_snmlen);
	
	SET(snp->n_flag, N_ISSTREAM);
	/* Special case that I would like to remove some day */
	if (bcmp(sname, SFM_RESOURCEFORK_NAME, sizeof(SFM_RESOURCEFORK_NAME)) == 0)
		SET(snp->n_flag, N_ISRSRCFRK);
	SET(snp->n_flag, NALLOC);
	smb_vhashadd(snp, hashval);
	vfsp.vnfs_mp = smp->sm_mp;
	vfsp.vnfs_vtype = VREG;
	vfsp.vnfs_str = "smbfs";
	vfsp.vnfs_dvp = NULL;
	vfsp.vnfs_fsnode = snp;
	vfsp.vnfs_cnp = NULL;
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

	lck_mtx_init(&snp->f_openStateLock, smbfs_mutex_group, smbfs_lock_attr);
	lck_mtx_init(&snp->f_openDenyListLock, smbfs_mutex_group, smbfs_lock_attr);
	/* 
	 * If this is the resource stream then the parents resource fork size has already been update. The calling routine
	 * aleady updated it. Remember that the parent is currently locked. smbfs_attr_cacheenter can lock the parent if we
	 * tell it to update the parent, so never tell it to update the parent in this routine. 
	 */
	smbfs_attr_cacheenter(*svpp, fap, FALSE, NULL);
	
	CLR(snp->n_flag, NALLOC);
	if (ISSET(snp->n_flag, NWALLOC))
		wakeup(snp);
	return 0;
	
errout:;	
	smb_vhashrem(snp);
	
	if (locked == 1)
		smbnode_unlock(snp);	/* Release the smbnode lock */
	
	if (ISSET(snp->n_flag, NWALLOC))
		wakeup(snp);
	
	if (snp->n_name)
		smbfs_name_free(snp->n_name);
	if (snp->n_sname)
		smbfs_name_free(snp->n_sname);
	
	FREE(snp, M_SMBNODE);
	return error;
}

/* 
 * Update the nodes resource fork size if needed. 
 * NOTE: Remmeber the parent can lock the child while hold its lock, but the child cannot lock the parent unless
 * the child is not holding its lock. So this routine is safe, because the parent is locking the child.
 */
int smb_get_rsrcfrk_size(vnode_t vp, vfs_context_t context)
{
	struct smbnode *np = VTOSMB(vp);
	u_int64_t strmsize = 0;
	time_t attrtimeo;
	struct timespec ts;
	int error = 0;
	time_t rfrk_cache_timer;
	struct timespec reqtime;
	
	nanouptime(&reqtime);
	SMB_CACHE_TIME(ts, np, attrtimeo);
	lck_mtx_lock(&np->rfrkMetaLock);
	rfrk_cache_timer = ts.tv_sec - np->rfrk_cache_timer;
	lck_mtx_unlock(&np->rfrkMetaLock);
	/* Cache has expired go get the resource fork size. */
	if (rfrk_cache_timer > attrtimeo) {
		error = smbfs_smb_qstreaminfo(np, context, NULL, NULL, SFM_RESOURCEFORK_NAME, &strmsize);
		/* 
		 * We got the resource stream size from the server, now update the resource stream
		 * if we have one. Search our hash table and see if we have a stream, if we find one
		 * then smbfs_find_vgetstrm will return it with  a vnode_get and a smb node lock on it.
		 */
		if (error == 0) {
			struct smbmount *smp = VTOSMBFS(vp);
			vnode_t svpp = smbfs_find_vgetstrm(smp, np, SFM_RESOURCEFORK_NAME);

			if (svpp) {
				if (smbfs_update_size(VTOSMB(svpp), &reqtime, strmsize) == TRUE) {
					/* Remember the only attribute for a stream is its size */
					nanouptime(&ts);
					VTOSMB(svpp)->attribute_cache_timer = ts.tv_sec;			
				}
				smbnode_unlock(VTOSMB(svpp));
				vnode_put(svpp);
			}
		} else {
			/* 
			 * Remeber that smbfs_smb_qstreaminfo will update the resource forks cache and size if it finds 
			 * the resource fork. We are handling the negative cache timer here. If we get an error then there
			 * is no resource fork so update the cache.
			 */
			lck_mtx_lock(&np->rfrkMetaLock);
			np->rfrk_size = 0;
			nanouptime(&ts);
			np->rfrk_cache_timer = ts.tv_sec;
			lck_mtx_unlock(&np->rfrkMetaLock);
		}
	}		
	return(error);
}

/*
 * Anytime the stream is updated we need to update the parent's meta data. In the resource fork case this
 * means updating the resource size and the resource size cache timer. For other streams it just means clearing the
 * meta data cache timer. We can update the parent's resource stream size and resource cache timer here because we
 * don't need the parent locked in this case. We use a different lock when updating the parents resource size and
 * resource cache timer. Since we cannot lock the parent node here just return the parent vnode so the calling process
 * can handle clearing the meta data cache timer.
 *
 * NOTE:	smbfs_vnop_pageout calls this routine wihout the node locked. It is not setting the size so this
 *			should be safe. If anyone edits this routine they need to keep in mind that it can be entered 
 *			without a lock.
 */
vnode_t smb_update_rsrc_and_getparent(vnode_t vp, int setsize)
{
	struct smbnode *np = VTOSMB(vp);
	vnode_t parent_vp = vnode_getparent(vp);
	struct timespec ts;

	/* If this is a resource stream then update the parents resource fork size and resource cache timer */
	if ((parent_vp) && (np->n_flag & N_ISRSRCFRK)) {
		lck_mtx_lock(&VTOSMB(parent_vp)->rfrkMetaLock);
		
		/* They want us to update the size */
		if (setsize) {
			VTOSMB(parent_vp)->rfrk_size = np->n_size;
			nanouptime(&ts);
			VTOSMB(parent_vp)->rfrk_cache_timer = ts.tv_sec;					
		} else if (VTOSMB(parent_vp)->rfrk_size != np->n_size)	/* Something changed just reset the cache timer */
			VTOSMB(parent_vp)->rfrk_cache_timer = 0;
	
		lck_mtx_unlock(&VTOSMB(parent_vp)->rfrkMetaLock);
	}
	return(parent_vp);	
}

/* 
 * Given a parent vnode lock the node , clear the meta cache timer, unlock it and then release the vnode. The
 * child node must not be locked while this routine is being call. Remmeber the parent can lock the child while holds
 * its own lock, but the child cannot lock the parent unless the child is not holding its own lock. So this routine is 
 * safe, because the child has already release it lock before calling this routine.
 */
void smb_clear_parent_cache_timer(vnode_t parent_vp)
{
	if (parent_vp) {
		if (smbnode_lock(VTOSMB(parent_vp), SMBFS_EXCLUSIVE_LOCK) == 0) {
			VTOSMB(parent_vp)->attribute_cache_timer = 0;
			smbnode_unlock(VTOSMB(parent_vp));			
		}
		vnode_put(parent_vp);		
	}	
}

static int smb_gid_match(struct smbmount *smp, u_int64_t node_gid)
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
 * Check to see if the user has the request access privileges on the node. Someday
 * we may have a call to check the access across the network, but for now all we can
 * do is check the posix mode bits. 
 *
 * NOTE: rq_mode should be one of the S_IRWXO modes.
 */
int smb_check_posix_access(vfs_context_t context, struct smbnode * np, mode_t rq_mode)
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

/*
 * routines to maintain vnode attributes cache
 * smbfs_attr_cacheenter: unpack np.i to vnode_vattr structure
 *
 * Note that some SMB servers do not exhibit POSIX behaviour
 * with regard to the mtime on directories.  To work around
 * this, we never allow the mtime on a directory to go backwards,
 * and bump it forwards elsewhere to simulate the correct
 * behaviour.
 */
void
smbfs_attr_cacheenter(vnode_t vp, struct smbfattr *fap, int UpdateResourceParent, vfs_context_t context)
{
	struct smbmount *smp = VTOSMBFS(vp);
	struct smb_share *ssp = smp->sm_share;
	int unix_extensions = ((UNIX_CAPS(SSTOVC(ssp)) & CIFS_UNIX_POSIX_PATH_OPERATIONS_CAP)) ? TRUE : FALSE;
	struct smbnode *np = VTOSMB(vp);
	enum vtype node_vtype;
	struct timespec ts;
	u_int32_t monitorHint = 0;

	node_vtype = vnode_vtype(vp);

	if ((node_vtype == VDIR) && np->d_needsUpdate) {
		monitorHint |= VNODE_EVENT_ATTRIB | VNODE_EVENT_WRITE;
		np->d_needsUpdate = FALSE;			
	}
	
	/* 
	 * The vtype of node has changed, so remove it from the name cache and our hash table.
	 * We set the cache timer to zero this will cause cache lookup routine to return ENOENT  
	 */
	if (node_vtype_changed(vp, node_vtype, fap)) {
		SMBWARNING("%s was %d now its %d\n", np->n_name, node_vtype, fap->fa_vtype);
		np->attribute_cache_timer = 0;
		cache_purge(vp);
		smb_vhashrem(np);
		monitorHint |= VNODE_EVENT_RENAME | VNODE_EVENT_ATTRIB;
		goto vnode_notify_needed;
	}

	if (node_vtype == VREG) {
		if (np->n_size != fap->fa_size)
			monitorHint |= VNODE_EVENT_EXTEND | VNODE_EVENT_ATTRIB;
		if (smbfs_update_size(np, &fap->fa_reqtime, fap->fa_size) == FALSE)
			return; /* we lost the race, ignore this update */
	} else if (node_vtype == VDIR) {
		np->n_size = 16384; 	/* should be a better way ... */
		/* See if we need to clear the negative name cache */
		if ((np->n_flag & NNEGNCENTRIES) && 
			((ssp->ss_fstype == SMB_FS_FAT) || (timespeccmp(&fap->fa_mtime, &np->n_mtime, >)))) {
			np->n_flag &= ~NNEGNCENTRIES;
			cache_purge_negatives(vp);			
		}
		/*
		 * Don't allow mtime to go backwards.
		 * Yes this has its flaws.  Better ideas are welcome!
		 */
		if (timespeccmp(&fap->fa_mtime, &np->n_mtime, <))
			fap->fa_mtime = np->n_mtime;
	} else if (node_vtype != VLNK)
		return;
	
	/* The server told us the allocation size return what they told us */
	np->n_data_alloc = fap->fa_data_alloc;

	if (fap->fa_unix) {
		np->n_flags_mask = fap->fa_flags_mask;
		np->n_nlinks = fap->fa_nlinks;
		/* Clear out the access mode bits so we can fill them correctly. */
		np->n_mode &= ~ACCESSPERMS;
		
		if (ssp->ss_attributes & FILE_PERSISTENT_ACLS) {
			/* 
			 * Use the uid, gid, and posix modes sent across the network. We have
			 * ACLs and POSIX Modes. Since we have both we can support POSIX modes 
			 * being set to zero.
			 */
			np->n_uid = (uid_t)fap->fa_uid;
			np->n_gid = (gid_t)fap->fa_gid;
			np->n_mode |= (mode_t)(fap->fa_permissions & ACCESSPERMS);
		} else if ((fap->fa_permissions & ACCESSPERMS) &&
			(smp->sm_args.uid == (uid_t)smp->ntwrk_uid) &&
			(smp->sm_args.gid == (gid_t)smp->ntwrk_gid)) {
			/* 
			 * The server gave us POSIX modes and the local user matches the network 
			 * user, so assume they are in the same directory name space. 
			 */
			np->n_uid = (uid_t)fap->fa_uid;
			np->n_gid = (gid_t)fap->fa_gid;
			np->n_mode |= (mode_t)(fap->fa_permissions & ACCESSPERMS);
		} else {
			int uid_match = (fap->fa_uid == smp->ntwrk_uid);
			int gid_match = smb_gid_match(smp, fap->fa_gid);
			
			np->n_uid = smp->sm_args.uid;
			np->n_gid = smp->sm_args.gid;
			/* 
			 * We have no idea let the server handle any access issues. This 
			 * is safe because we only allow root and the user that mount the
			 * volume to have access to this mount point
			 */
			if ((fap->fa_permissions & ACCESSPERMS) == 0)
				fap->fa_permissions = ACCESSPERMS;
			if (!uid_match && !gid_match) {
				/* Use other perms */
				np->n_mode |= (mode_t)(fap->fa_permissions & S_IRWXO);
				/* use other for group */
				np->n_mode |= (mode_t)((fap->fa_permissions & S_IRWXO) << 3);			
				/* use other for owner */
				np->n_mode |= (mode_t)((fap->fa_permissions & S_IRWXO) << 6);			
			} else if (!uid_match && gid_match) {
				/* Use group and other perms  */
				np->n_mode |= (mode_t)(fap->fa_permissions & (S_IRWXG | S_IRWXO));			
				/* use group for owner */
				np->n_mode |= (mode_t)((fap->fa_permissions & S_IRWXG) <<  3);			
			} else if (uid_match && !gid_match) {
				/* Use owner and other perms */
				np->n_mode |= (mode_t)(fap->fa_permissions & (S_IRWXU | S_IRWXO));			
				/* use other for group */
				np->n_mode |= (mode_t)((fap->fa_permissions & S_IRWXO) << 3);			
			} else {
				/* Use owner, group and other perms */
				np->n_mode |= (mode_t)(fap->fa_permissions & ACCESSPERMS);
			}
		}
	} else if (unix_extensions) {
		/* 
		 * We are doing unix extensions yet this fap didn't come from the unix 
		 * info2 call. We got this info from a NtCreateAndX message ignore the 
		 * setting of the uid, gid or modes. We will catch it on the next lookup.
		 */
	} else if (ssp->ss_attributes & FILE_PERSISTENT_ACLS) {
		/* 
		 * We are using Active Directory ACLs only, skip UID/GID/POSIX Modes until we 
		 * have a request. We have nothing to do until they ask for UID/GID/POSIX Modes
		 */ 		
	} else if ((np->n_uid == KAUTH_UID_NONE) && (np->n_gid == KAUTH_GID_NONE)) {
		/* Use mapped ids and modes */
		np->n_uid = smp->sm_args.uid;
		np->n_gid = smp->sm_args.gid;
		if (vnode_vtype(vp) == VDIR)
			np->n_mode |= smp->sm_args.dir_mode;
		else	/* symlink or regular file */
			np->n_mode |= smp->sm_args.file_mode;
	}

	if ((monitorHint & VNODE_EVENT_ATTRIB) == 0) {
		if (!(timespeccmp(&np->n_crtime, &fap->fa_crtime, ==) ||
			 !(timespeccmp(&np->n_mtime, &fap->fa_mtime, ==))))
			monitorHint |= VNODE_EVENT_ATTRIB;
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
	np->n_dosattr = fap->fa_attr;
	np->n_flag &= ~NATTRCHANGED;
	nanouptime(&ts);
	np->attribute_cache_timer = ts.tv_sec;
	/*
	 * UpdateResourceParent says it is ok to update the parent if this is a resource stream. So if this is a stream
	 * and its the resource stream then update the parents resource fork size and cache timer. If we can't get the 
	 * parent then just get out, when the timer goes off the parent will just have to make the wire call.
	 */
	if (UpdateResourceParent && (vnode_isnamedstream(vp)) && (np->n_flag & N_ISRSRCFRK)) {
		vnode_t parent_vp = smb_update_rsrc_and_getparent(vp, (fap->fa_size) ? TRUE : FALSE);
		/* We no longer need the parent so release it. */
		if (parent_vp)  
			vnode_put(parent_vp);
	}
	
vnode_notify_needed:
	if ((monitorHint != 0) && (vnode_ismonitored(vp)) && context) {
		struct vnode_attr vattr;
		
		vfs_get_notify_attributes(&vattr);
		smbfs_attr_cachelookup(vp, &vattr, context, TRUE);
		vnode_notify(vp, monitorHint, &vattr);
	}
}

int
smbfs_attr_cachelookup(vnode_t vp, struct vnode_attr *va, vfs_context_t context, int useCacheDataOnly)
{
	struct smbnode *np = VTOSMB(vp);
	struct smbmount *smp = VTOSMBFS(vp);
	struct smb_share *ssp = smp->sm_share;
	int unix_extensions = ((UNIX_CAPS(SSTOVC(ssp)) & CIFS_UNIX_POSIX_PATH_OPERATIONS_CAP)) ? TRUE : FALSE;
	time_t attrtimeo;
	struct timespec ts;

	SMB_CACHE_TIME(ts, np, attrtimeo);

	if ((ssp->ss_flags & SMBS_RECONNECTING) && ((smp->sm_args.altflags & SMBFS_MNT_SOFT) != SMBFS_MNT_SOFT)) {
		/* 
		 * We are in reconnect mode and this is not a soft mount, 
		 * then use cached data for now.  See Radar 5092244 for more details
		 */
	} else if (useCacheDataOnly) {
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
	else if ((ts.tv_sec - np->attribute_cache_timer) > attrtimeo)
		return (ENOENT);

	if (!va)
		return (0);
	
	VATTR_RETURN(va, va_rdev, 0);
	if (unix_extensions)
		VATTR_RETURN(va, va_nlink, np->n_nlinks);
	else
		VATTR_RETURN(va, va_nlink, 1);
	
	/* 
	 * Looks like we need to handle total size in the streams case. The VFS layer always fill this in with the 
	 * data fork size. Still not sure of this, but for now lets go ahead and handle if ask.
	 */
	if ((ssp->ss_attributes & FILE_NAMED_STREAMS) && (VATTR_IS_ACTIVE(va, va_total_size))) {
		if (vnode_isdir(vp)) {
			VATTR_RETURN(va, va_total_size, np->n_size);
			lck_mtx_lock(&np->n_mount->sm_statfslock);
			if (np->n_mount->sm_statfsbuf.f_bsize)	/* Just to be safe */
				VATTR_RETURN(va, va_total_alloc, roundup(va->va_total_size, np->n_mount->sm_statfsbuf.f_bsize));
			lck_mtx_unlock(&np->n_mount->sm_statfslock);
		}
		else if (!vnode_isnamedstream(vp)) {
			(void)smb_get_rsrcfrk_size(vp, context);
			lck_mtx_lock(&np->rfrkMetaLock);
			VATTR_RETURN(va, va_total_size, np->n_size + np->rfrk_size);
			lck_mtx_unlock(&np->rfrkMetaLock);
			lck_mtx_lock(&np->n_mount->sm_statfslock);
			if (np->n_mount->sm_statfsbuf.f_bsize)	/* Just to be safe */
				VATTR_RETURN(va, va_total_alloc, roundup(va->va_total_size, np->n_mount->sm_statfsbuf.f_bsize));
			lck_mtx_unlock(&np->n_mount->sm_statfslock);
		}
	}
	
	VATTR_RETURN(va, va_data_size, np->n_size);
	VATTR_RETURN(va, va_data_alloc, np->n_data_alloc);

	VATTR_RETURN(va, va_iosize, SSTOVC(smp->sm_share)->vc_txmax);
	
	if (VATTR_IS_ACTIVE(va, va_uid) || VATTR_IS_ACTIVE(va, va_gid) || VATTR_IS_ACTIVE(va, va_mode)) {
		/* We are using Active Directory ACLs, only set UID/GID/POSIX Modes if they ask for them. */  
		if ((ssp->ss_attributes & FILE_PERSISTENT_ACLS) && 
			((np->n_uid == KAUTH_UID_NONE) && (np->n_gid == KAUTH_GID_NONE))) {
			struct vnode_attr vattr;
			/* 
			 * We use smbfs_getsecurity to fill in the uid, gid and mode. We ignore
			 * any errors, because they don't affect the fields we care about. 
			 */
			VATTR_INIT(&vattr);
			VATTR_WANTED(&vattr, va_uuuid);
			VATTR_WANTED(&vattr, va_guuid);
			(void)smbfs_getsecurity(np, &vattr, context);
			
		}
		VATTR_RETURN(va, va_mode, np->n_mode);
		VATTR_RETURN(va, va_uid, np->n_uid);
		VATTR_RETURN(va, va_gid, np->n_gid);
	}
	if (VATTR_IS_ACTIVE(va, va_flags)) {
		va->va_flags = 0;
		/*
		 * Remember that SMB_FA_ARCHIVE means the items needs to be 
		 * archive and SF_ARCHIVED means the item has been archive.
		 *
		 * NOTE: Windows does not set ATTR_ARCHIVE bit for directories.
		 */
		if (!vnode_isdir(vp) && !(np->n_dosattr & SMB_FA_ARCHIVE))
			va->va_flags |= SF_ARCHIVED;
		/*
		 * SMB_FA_RDONLY ~ UF_IMMUTABLE
		 *
		 * We treat the SMB_FA_RDONLY as the immutable flag. This allows
		 * us to support the finder lock bit and makes us follow the 
		 * MSDOS code model. See msdosfs project.
		 *
		 * NOTE: The ready-only flags does not exactly follow the lock/immutable bit.
		 *
		 * See Radar 5582956 for more details.
		 *
		 * When dealing with Window Servers the read-only bit for folder does not mean the samething 
		 * as it does for files. Doing this translation was confusing customers and really
		 * didn't work the way Mac users would expect.
		 */
		if ((unix_extensions || (!vnode_isdir(vp))) && (np->n_dosattr & SMB_FA_RDONLY))
			va->va_flags |= UF_IMMUTABLE;
		/* The server has it marked as hidden set the new UF_HIDDEN bit. */
 		if (np->n_dosattr & SMB_FA_HIDDEN)
			va->va_flags |= UF_HIDDEN;
		
		VATTR_SET_SUPPORTED(va, va_flags);
	}
	
	/* va_acl are done in smbfs_getattr */
	
	VATTR_RETURN(va, va_create_time, np->n_crtime);
	VATTR_RETURN(va, va_modify_time, np->n_mtime);
	/* FAT only supports the date not the time? */
	VATTR_RETURN(va, va_access_time, np->n_atime);
	/* 
	 * FAT does not support change time, so just return the modify time. 
	 * Copied from the msdos code. SMB has no backup time so skip the
	 * va_backup_time.
	 */
	if (ssp->ss_fstype == SMB_FS_FAT)
		np->n_chtime.tv_sec = np->n_mtime.tv_sec;
	VATTR_RETURN(va, va_change_time, np->n_chtime);
	
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
	VATTR_RETURN(va, va_fileid, np->n_ino ? np->n_ino : 2);
	VATTR_RETURN(va, va_linkid, np->n_ino ? np->n_ino : 2);
	/* 
	 * This would require a lot more work so let the VFS layer handle it.  
	 * VATTR_RETURN(va, va_parentid, np->n_parentid);
	 */

	VATTR_RETURN(va, va_fsid, vfs_statfs(vnode_mount(vp))->f_fsid.val[0]);
	VATTR_RETURN(va, va_filerev, 0);	
	VATTR_RETURN(va, va_gen, 0);
	
	/* 
	 * We currently have no way to know the va_encoding. The VFS layer fills it
	 * in with kTextEncodingMacUnicode = 0x7E. Lets leave it to the VFS layer
	 * to handle for now.
	 * VATTR_RETURN(va, va_encoding, 0x7E);
	 */
	
	/* 
	 * If this is the root, let VFS find out the mount name, which may be 
	 * different from the real name 
	 */
	if (VATTR_IS_ACTIVE(va, va_name) && !vnode_isvroot(vp)) {
		strlcpy ((char*) va->va_name, (char*)np->n_name, MAXPATHLEN);
		VATTR_SET_SUPPORTED(va, va_name);
	}
	/* va_uuuid is done in smbfs_getattr */
	/* va_guuid is done in smbfs_getattr */
	/* We have no  way to get va_nchildren. Let VFS layer handle it. */
	return (0);
}

/*
 * Some SMB servers (FAT) don't exhibit POSIX behaviour with regard to 
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
smbfs_attr_touchdir(struct smb_share *ssp, struct smbnode *dnp)
{
	if (ssp->ss_fstype == SMB_FS_FAT) {
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
}

/*
 * If the file size hasn't change then really nothing to do here, get out but
 * let the calling routine know that they can update their cache timer. If we have
 * updated the size internally, while waiting on the response from the server,
 * then don't update the size and tell the calling routine not to update its
 * cache timers. Otherwise update our internal size and the ubc size. Also tell
 * the calling routine to update any cache timers.
 */
int smbfs_update_size(struct smbnode *np, struct timespec *reqtime, u_quad_t new_size)
{
	off_t old, newround;
	
	if (np->n_size == new_size)
		return TRUE; /* Nothing to update here */
	
	if (timespeccmp(reqtime, &np->n_sizetime, <=))
		return FALSE; /* we lost the race, tell the calling routine */
	
	old = (off_t)np->n_size;
	newround = round_page_64((off_t)new_size);
	if (old > newround) {
		int error = ubc_msync(np->n_vnode, newround, old, NULL, UBC_INVALIDATE);
		if (error) {
			SMBERROR("%s ubc_msync failed! %d\n", np->n_name, error);			
		}
	}
	smbfs_setsize(np->n_vnode, new_size);
	return TRUE;
}

/*
 * smbfs_FindLockEntry
 *
 * Return Values
 *
 *	TRUE	- We have this ranged locked already
 *	FALSE	- We don't have this range locked
 */
int smbfs_FindLockEntry(struct fileRefEntry *fndEntry, int64_t offset, int64_t length, u_int32_t lck_pid)
{
	struct ByteRangeLockEntry *curr = fndEntry->lockList;
	
	while (curr) {
		if ((curr->offset == offset) && (curr->length == length) && (curr->lck_pid == lck_pid))
			return TRUE;
		curr = curr->next;			
	}
	return FALSE;
}
	
/*
 * smbfs_AddRemoveLockEntry
 *
 * Add .
 * 
 * Return Values
 *	none
 */
void smbfs_AddRemoveLockEntry (struct fileRefEntry *fndEntry, int64_t offset, int64_t length, int8_t unLock, u_int32_t lck_pid)
{
	struct ByteRangeLockEntry *curr = NULL;
	struct ByteRangeLockEntry *prev = NULL;
	struct ByteRangeLockEntry *new = NULL;
	int32_t foundIt = 0;

	if (unLock == 0) {	/* Locking, so add a new ByteRangeLockEntry */
		MALLOC (new, struct ByteRangeLockEntry *, sizeof (struct ByteRangeLockEntry), M_TEMP, M_WAITOK);
		new->offset = offset;
		new->length = length;
		new->lck_pid = lck_pid;
		new->next = NULL;

		curr = fndEntry->lockList;
		if (curr == NULL) {
			/* first entry is empty so use it */
			fndEntry->lockList = new;
		}
		else { /* find the last entry and add the new entry to the end of list */
			while (curr->next != NULL)
				curr = curr->next;
			curr->next = new;
		}
	}
	else {	/* Unlocking, so remove a ByteRangeLockEntry */
		curr = fndEntry->lockList;
		if (curr == NULL) {
		    SMBWARNING("smbfs_AddRemoveLockEntry:  no entries found\n");
		    return;
		}
		
		if ((curr->offset == offset) && (curr->length == length)) {
			/* first entry is it, so remove it from the head */
			fndEntry->lockList = curr->next;
			FREE(curr, M_TEMP);
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
					FREE(curr, M_TEMP);
					break;
				}
				prev = curr;
				curr = curr->next;
			}

			if (foundIt == 0)
				SMBWARNING ("offset 0x%llx/0x%llx not found in fndEntry %p\n", offset, length, (void *)fndEntry);
		}
	}
}

/*
 * smbfs_addFileRef
 *
 * Create a new open deny file list entry.
 * 
 * Return Values
 *	fndEntry is not NULL then return the entry.
 */
void 
smbfs_addFileRef(vnode_t vp, struct proc *p, u_int16_t accessMode, u_int32_t rights, 
		u_int16_t fid, struct fileRefEntry **fndEntry)
{
	struct smbnode	*np = VTOSMB(vp);
	struct fileRefEntry *entry = NULL;
	struct fileRefEntry *current = NULL;
        
	/* Create a new fileRefEntry and insert it into the hp list */
	MALLOC(entry, struct fileRefEntry *, sizeof (struct fileRefEntry), M_TEMP, M_WAITOK);
	entry->refcnt = 0;
	entry->mmapped = FALSE;
	entry->proc = p;
	entry->p_pid = proc_pid(p);
	entry->accessMode = accessMode;
	entry->rights = rights;
	entry->fid = fid;
	entry->lockList = NULL;
	entry->next = NULL;
	if (fndEntry) *fndEntry = entry;

	lck_mtx_lock(&np->f_openDenyListLock);
	if (np->f_openDenyList == NULL) /* No other entries, so we are the first */
		np->f_openDenyList = entry;
	else { /* look for last entry in the list */
		current = np->f_openDenyList;
		while (current->next != NULL) {
		current = current->next;
        }
        /* put it at the end of the list */
        current->next = entry;
    }
	lck_mtx_unlock(&np->f_openDenyListLock);
}


/*
 * smbfs_findFileEntryByFID
 *
 * Find an entry in the open deny file list entry. Use the fid to locate the
 * entry.
 * 
 * Return Values
 *	-1	No matching entry found
 *	0	Found a match 
 */
int32_t 
smbfs_findFileEntryByFID(vnode_t vp, u_int16_t fid, struct fileRefEntry **fndEntry)
{
	struct fileRefEntry *entry = NULL;
	struct smbnode	*np;
	
#ifdef SMB_DEBUG
	if (fndEntry)
		DBG_ASSERT(*fndEntry == NULL);
#endif // SMB_DEBUG
	
	/* If we have not vnode then we are done. */
	if (!vp)
		return (-1);

	np = VTOSMB(vp);
	lck_mtx_lock(&np->f_openDenyListLock);
	/* Now search the list until we find a match */
	for (entry = np->f_openDenyList; entry; entry = entry->next) {
		if (entry->fid == fid) {
			if (fndEntry) 
				*fndEntry = entry;
			lck_mtx_unlock(&np->f_openDenyListLock);
			return(0);
		}
	}
	lck_mtx_unlock(&np->f_openDenyListLock);
	return(-1);	/* No match found */
}

/*
 * smbfs_findMappedFileRef
 *
 * Search the open deny file list entry looking for a mapped entry. If they requested
 * the entry return it, if they requested the fid return it also. 
 * 
 * Return Values
 *	FALSE	No matching entry found
 *	TRUE	Found a match
 */
int32_t 
smbfs_findMappedFileRef(vnode_t vp, struct fileRefEntry **fndEntry, u_int16_t *fid)
{
	struct fileRefEntry *entry = NULL;
	int32_t foundIt = FALSE;
	struct smbnode	*np;
	
	/* If we have no vnode then we are done. */
	if (!vp)
		return (foundIt);
	
	np = VTOSMB(vp);
	lck_mtx_lock(&np->f_openDenyListLock);
	for (entry = np->f_openDenyList; entry; entry = entry->next) {
		if (entry->mmapped) {
			if (fid)
			    *fid = entry->fid;
			if (fndEntry)
			    *fndEntry = entry;
			foundIt = TRUE;
			break;
		}
	}
	lck_mtx_unlock(&np->f_openDenyListLock);
	return (foundIt);
}

/*
 * smbfs_findFileRef
 *
 * Find an entry in the open deny file list entry. Use accessMode and flags to 
 * locate the entry.
 * 
 * Return Values
 *	-1	No matching entry found
 *	0	Found a match
 *			if fndEntry is not NULL it will point to that entry.
 *			fid now holds file reference id for that entry.
 */
int32_t 
smbfs_findFileRef(vnode_t vp, pid_t pid, u_int16_t accessMode, int32_t flags,  int64_t offset, 
			int64_t length,  struct fileRefEntry **fndEntry, u_int16_t *fid)
{
	struct fileRefEntry *entry = NULL;
	struct fileRefEntry *tempEntry = NULL;
	struct ByteRangeLockEntry *currBRL = NULL;
	int32_t foundIt = 0;
	struct smbnode	*np;
	
#ifdef SMB_DEBUG
	if (fndEntry)
		DBG_ASSERT(*fndEntry == NULL);
#endif // SMB_DEBUG
	/* If we have no vnode then we are done. */
	if (!vp)
		return (-1);

	np = VTOSMB(vp);
	lck_mtx_lock(&np->f_openDenyListLock);
	for (entry = np->f_openDenyList; entry; entry = entry->next) {
		/* No match continue looking */
		if (entry->p_pid != pid)
			continue;
		
		switch (flags) {
		case kAnyMatch:
			/* 
			 * if any fork will do, make sure at least have accessMode 
			 * set. This is for the old ByteRangeLocks and other misc 
			 * functions looking for a file ref 
			 */
			if (entry->accessMode & accessMode)
				foundIt = 1;
			break;
		case kCheckDenyOrLocks:
			/* 
			 * This was originally written for Classic support, but after looking at it some we
			 * decide it could happen in Carbon.
			 *
			 * Where I have the same PID on two different file,  some BRL taken, and a read/write occuring.
			 * I have to determine which file will successfully read/write on due to any possible
			 * byte range locks already taken out.  Note that Classic keeps track of BRLs itself 
			 * and will not block any read/writes that would fail due to a BRL.  I just have to 
			 * find the correct fork so that the read/write will succeed.
			 * Example:  open1 rw/DW, open2 r, lock1 0-5, read1 0-5 should occur on fork1 and not fork2
			*/
			/* make sure we have correct access */
			if (entry->accessMode & accessMode) {
				/* 
				 * save this entry in case we find no entry with a matching BRL.
				 * saves me from having to search all over again for an OpenDeny match 
				 */
				if (tempEntry == NULL)
					tempEntry = entry;

				/* check the BRLs to see if the offset/length fall inside one of them */
				currBRL = entry->lockList;
				while (currBRL != NULL) {
					/* is start of read/write inside of the BRL? */
					if ( (offset >= currBRL->offset) && (offset <= (currBRL->offset + currBRL->length)) ) {
						foundIt = 1;
						break;
					}
					/* is end of read/write inside of the BRL? */
					if ( ((offset + length) >= currBRL->offset) &&  
						((offset + length) <= (currBRL->offset + currBRL->length)) ) {
						foundIt = 1;
						break;
					}
					currBRL = currBRL->next;
				}
			}
			break;
			
		case kExactMatch:
		default:
			/* 
			 * If we want an exact match, then check access mode too
			 * This is for ByteRangeLocks and closing files 
			 */
			if (accessMode == entry->accessMode)
				foundIt = 1;
			break;
		}
        
		if (foundIt == 1) {
			*fid = entry->fid;
			if (fndEntry) *fndEntry = entry;
			break;
		}
	}
	lck_mtx_unlock(&np->f_openDenyListLock);

	/* Will only happen after we add byte range locking support */
	if (foundIt == 0) {
		if ( (flags == kCheckDenyOrLocks) && (tempEntry != NULL) ) {
			/* Did not find any BRL that matched, see if there was a match with an OpenDeny */
			*fid = tempEntry->fid;
			if (fndEntry) *fndEntry = entry;
			return (0);
		}
		return (-1);    /* fork not found */
	}
	else
		return (0);
}

/*
 * smbfs_removeFileRef
 *
 * Remove the entry that was passed in from the list and free it. If no entry is 
 * passed in then remove all entries.
 * 
 * Return Values
 *	none
 */
void 
smbfs_removeFileRef(vnode_t vp, struct fileRefEntry *inEntry)
{
	struct smbnode	*np = VTOSMB(vp);
	struct fileRefEntry *curr = NULL;
	struct fileRefEntry *prev = NULL;
	struct fileRefEntry *entry = NULL;
	struct ByteRangeLockEntry *currBRL = NULL;
	struct ByteRangeLockEntry *nextBRL = NULL;
	int32_t foundIt = 0;

	lck_mtx_lock(&np->f_openDenyListLock);
	if (inEntry == NULL) {	/* Means remove all */
		entry = np->f_openDenyList;
		while (entry != NULL) {
			/* wipe out the ByteRangeLockEntries first */
			currBRL = entry->lockList;
			while (currBRL != NULL) {
				nextBRL = currBRL->next; /* save next in list */
				FREE (currBRL, M_TEMP);	 /* free current entry */
				currBRL = nextBRL;	 /* and on to the next */
			}
			entry->lockList = NULL;
			/* now wipe out the file refs */
			curr = entry;
			entry = entry->next;
			DBG_ASSERT(curr->refcnt == 0);
			FREE(curr, M_TEMP);
		}
		np->f_openDenyList = NULL;
		goto out;
	}
	DBG_ASSERT(inEntry->refcnt == 0);

	/* wipe out the ByteRangeLockEntries first */
	currBRL = inEntry->lockList;
	while (currBRL != NULL) {
		nextBRL = currBRL->next;	/* save next in list */
		FREE(currBRL, M_TEMP);		/* free current entry */
		currBRL = nextBRL;		/* and on to the next */
	}
	inEntry->lockList = NULL;

	/* Remove the fileRefEntry */
	curr = np->f_openDenyList;
	if (curr == NULL)
		goto out;
	/* 
	 * if its the first entry in the list, then just set the first 
	 * entry to be entry->next
	 */
	if (inEntry == curr) {
		np->f_openDenyList = inEntry->next;
		foundIt = 1;
		FREE(curr, M_TEMP);
		curr = NULL;
	}
	else {
		// its not the first, so search the rest
		prev = np->f_openDenyList;
		curr = prev->next;
		while (curr != NULL) {
			if (inEntry == curr) {
				prev->next = curr->next;
				foundIt = 1;
				FREE(curr, M_TEMP);
				curr = NULL;
				break;
			}
			prev = curr;
			curr = curr->next;
		}
	}
	if (foundIt == 0)
		SMBWARNING ("inEntry %p not found in vp %p\n", (void *)inEntry, (void *)vp);
out:
	lck_mtx_unlock(&np->f_openDenyListLock);
}

/*
 * Search the hash table looking for any open files. Remember we have a hash table
 * for every mount point. Not sure why but it makes this part easier. Currently we
 * do not support reopens, we just mark the file to be revoked.
 */
void smbfs_reconnect(struct smb_share *ssp)
{
	struct smbmount *smp;
	struct smbnode *np;
	u_int32_t ii;
	
	lck_mtx_lock(&ssp->ss_mntlock);
	if (ssp->ss_mount == NULL)
		goto WeAreDone;
	smp = ssp->ss_mount;
	smbfs_hash_lock(smp);
	
	/* We have a hash table for each mount point */
	for (ii = 0; ii < (smp->sm_hashlen + 1); ii++) {
		if ((&smp->sm_hash[ii])->lh_first == NULL)
			continue;
		
		for (np = (&smp->sm_hash[ii])->lh_first; np; np = np->n_hash.le_next) {
			if (ISSET(np->n_flag, NALLOC))
				continue;

			if (ISSET(np->n_flag, NTRANSIT))
				continue;
			
			/*
			 * Someone is monitoring this item and we reconnected. Force a
			 * notify update.
			 */
			if (np->n_vnode && (vnode_ismonitored(np->n_vnode))) {
				SMBDEBUG("%s needs to be updated.\n", np->n_name);
				/* Do we need to reopen this item */
				if ((np->n_dosattr & SMB_FA_DIR) && np->d_fid)
					np->d_needReopen = TRUE;
				/* Force a network lookup */
				np->attribute_cache_timer = 0;
				np->d_needsUpdate = TRUE;
			}
			
			/* Nothing else to with directories at this point */
			if (np->n_dosattr & SMB_FA_DIR)
				continue;
			/* We only care about open files */			
			if (np->f_refcnt == 0)
				continue;
			/*
			 * We have an open file mark it to be reopen.
			 *
			 * 1. Plain old POSIX open with no locks. Only revoke if reopen fails.
			 * 2. POSIX open with a flock. Revoke if reopen fails. Revoke if the modify time or size is different. 
			 *		Otherwise reestablish the lock. If the lock fails then mark it to be revoked.
			 * 3. POSIX open with POSIX locks. NOT SUPPORTED CURRENTY! %%% (We do not support posix locks)
			 * 4. Carbon Open (OpenDeny) with no locks. Revoke if reopen fails. Revoke if the modify time or size is different.
			 * 5. Carbon Open with Carbon Locks. Revoke if reopen fails. Revoke if the modify time or size is different. 
			 *		Otherwise reestablish the lock. If the lock fails then mark it to be revoked.
			 */
			lck_mtx_lock(&np->f_openStateLock);
			if (np->f_openState != kNeedRevoke)	/* Once it has been revoked it stays revoked */
				np->f_openState = kNeedReopen;
			lck_mtx_unlock(&np->f_openStateLock);
		}
	}
	smbfs_hash_unlock(smp);
	
WeAreDone:
	lck_mtx_unlock(&ssp->ss_mntlock);
}

