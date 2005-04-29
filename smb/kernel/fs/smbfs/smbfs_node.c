/*
 * Copyright (c) 2000-2001 Boris Popov
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
 * $Id: smbfs_node.c,v 1.54 2005/03/09 16:51:59 lindak Exp $
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

#include <sys/md5.h>

#include <sys/kauth.h>

#include <sys/smb_apple.h>
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

int smbfs_hashprint(struct mount *mp);

#if 0
extern struct linker_set sysctl_vfs_smbfs;
#ifdef SYSCTL_DECL
SYSCTL_DECL(_vfs_smbfs);
#endif
SYSCTL_PROC(_vfs_smbfs, OID_AUTO, vnprint, CTLFLAG_WR|CTLTYPE_OPAQUE,
	    NULL, 0, smbfs_hashprint, "S,vnlist", "vnode hash");
#endif

#define	FNV_32_PRIME ((u_int32_t) 0x01000193UL)
#define	FNV1_32_INIT ((u_int32_t) 33554467UL)

#define isdigit(d) ((d) >= '0' && (d) <= '9')

u_int32_t
smbfs_hash(const u_char *name, int nmlen)
{
	u_int32_t v;

	for (v = FNV1_32_INIT; nmlen; name++, nmlen--) {
		v *= FNV_32_PRIME;
		v ^= (u_int32_t)*name;
	}
	return v;
}

char *
smbfs_name_alloc(const u_char *name, int nmlen)
{
	u_char *cp;

	nmlen++;
#ifdef SMBFS_NAME_DEBUG
	cp = malloc(nmlen + 2 + sizeof(int), M_SMBNODENAME, M_WAITOK);
	*(int*)cp = nmlen;
	cp += sizeof(int);
	cp[0] = 0xfc;
	cp++;
	bcopy(name, cp, nmlen - 1);
	cp[nmlen] = 0xfe;
#else
	cp = malloc(nmlen, M_SMBNODENAME, M_WAITOK);
	bcopy(name, cp, nmlen - 1);
#endif
	cp[nmlen - 1] = 0;
	return cp;
}

void
smbfs_name_free(const u_char *name)
{
#ifdef SMBFS_NAME_DEBUG
	int nmlen, slen;
	u_char *cp;

	cp = name;
	cp--;
	if (*cp != 0xfc) {
		printf("First byte of name entry '%s' corrupted\n", name);
		Debugger("ditto");
	}
	cp -= sizeof(int);
	nmlen = *(int*)cp;
	slen = strlen(name) + 1;
	if (nmlen != slen) {
		printf("Name length mismatch: was %d, now %d name '%s'\n",
		    nmlen, slen, name);
		Debugger("ditto");
	}
	if (name[nmlen] != 0xfe) {
		printf("Last byte of name entry '%s' corrupted\n", name);
		Debugger("ditto");
	}
	free(cp, M_SMBNODENAME);
#else
	free(name, M_SMBNODENAME);
#endif
}

vnode_t
smb_hashget(struct smbmount *smp, struct smbnode *dnp, u_long hashval,
	    const u_char *name, int nmlen)
{
	vnode_t	vp;
	struct smbnode_hashhead	*nhpp;
	struct smbnode *np;
	uint32_t vid;
loop:
	smbfs_hash_lock(smp);
	nhpp = SMBFS_NOHASH(smp, hashval);
	LIST_FOREACH(np, nhpp, n_hash) {
		if (np->n_parent != dnp || np->n_nmlen != nmlen ||
		    bcmp(name, np->n_name, nmlen) != 0)
			continue;
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
		return (vp);
	}
	smbfs_hash_unlock(smp);
	return (NULL);
}

int
smbfs_nget(struct mount *mp, vnode_t dvp, const char *name, int nmlen,
	struct smbfattr *fap, vnode_t *vpp, u_long makeentry, enum vtype vt)
{
	struct smbmount *smp = VFSTOSMBFS(mp);
	struct smbnode_hashhead *nhpp;
	struct smbnode *np, *dnp;
	vnode_t vp;
	int error = 0, cerror;
	char *namep = NULL;
	u_long	hashval;
	struct vnode_fsparam vfsp;
	struct componentname cn;

	*vpp = NULL;
	if ((vfs_isforce(smp->sm_mp)))
		return ENXIO;
	if (smp->sm_root != NULL && dvp == NULL) {
		SMBERROR("do not allocate root vnode twice!\n");
		return EINVAL;
	}
	if (nmlen == 2 && bcmp(name, "..", 2) == 0) {
		if (dvp == NULL)
			return EINVAL;
		vp = VTOSMB(dvp)->n_parent->n_vnode;
		error = vnode_get(vp);
		if (error == 0)
			*vpp = vp;
		return error;
	} else if (nmlen == 1 && name[0] == '.') {
		SMBERROR("do not call me with dot!\n");
		return EINVAL;
	}
	dnp = dvp ? VTOSMB(dvp) : NULL;
	if (dnp == NULL && dvp != NULL) {
		SMBERROR("dead parent vnode\n");
		return EINVAL;
	}
	namep = smbfs_name_alloc(name, nmlen);
	MALLOC(np, struct smbnode *, sizeof *np, M_SMBNODE, M_WAITOK);
	hashval = smbfs_hash(name, nmlen);
	if ((*vpp = smb_hashget(smp, dnp, hashval, name, nmlen)) != NULL) {
		smbfs_name_free(namep);
		FREE(np, M_SMBNODE);
		vp = *vpp;
		/* update the attr_cache info if the file is clean */
		if (fap && !(VTOSMB(vp)->n_flag & NFLUSHWIRE))
			smbfs_attr_cacheenter(vp, fap);
		/* XXX nfs (only) does cache_enter here if dnp && makeentry */
		return (0);
	}
	/*
	 * If we don't have node attributes, then it is an explicit lookup
	 * for an existing vnode.
	 */
	if (fap == NULL) {
		smbfs_name_free(namep);
		FREE(np, M_SMBNODE);
		return (ENOENT);
	}
	bzero(np, sizeof(*np));
	np->n_vnode = NULL;	/* redundant, but emphatic! */
	np->n_mount = smp;
	np->n_size = fap->fa_size;
	np->n_ino = fap->fa_ino;
	np->n_name = namep;
	np->n_nmlen = nmlen;
	np->n_uid = KAUTH_UID_NONE;
	np->n_gid = KAUTH_GID_NONE;
	SET(np->n_flag, NALLOC);
	smbfs_hash_lock(smp);
	nhpp = SMBFS_NOHASH(smp, hashval);
	LIST_INSERT_HEAD(nhpp, np, n_hash);
	smbfs_hash_unlock(smp);
	if (dvp) {
		np->n_parent = dnp;
		if (!vnode_isvroot(dvp)) {
			vnode_get(dvp);
			vnode_ref(dvp);
			vnode_put(dvp);
			np->n_flag |= NREFPARENT;
		}
	}
	if (makeentry) {
		cn.cn_flags = 0;
		cn.cn_nameptr = namep;
		cn.cn_namelen = nmlen;
		cn.cn_hash = 0;
	}

	vfsp.vnfs_mp = mp;
	vfsp.vnfs_vtype = vt ? vt : ((fap->fa_attr & SMB_FA_DIR) ? VDIR : VREG);
	vfsp.vnfs_str = "smbfs";
	vfsp.vnfs_dvp = dvp;
	vfsp.vnfs_fsnode = np;
	vfsp.vnfs_cnp = makeentry ? &cn : NULL;
	vfsp.vnfs_vops = smbfs_vnodeop_p;
	vfsp.vnfs_rdev = 0;	/* no VBLK or VCHR support */
	vfsp.vnfs_flags = makeentry ? 0 : VNFS_NOCACHE;
        vfsp.vnfs_markroot = (np->n_ino == 2);
        vfsp.vnfs_marksystem = 0;

	if (vfsp.vnfs_vtype == VREG && np->n_size == SMB_SYMLEN) {
		uio_t uio;
		struct smb_cred scred;
		struct smb_share *ssp = smp->sm_share;
		MD5_CTX md5;
		char m5b[SMB_SYMMD5LEN-1];
		u_int32_t state[4];
		int len = 0;
		unsigned char *sb, *cp;
		vfs_context_t vfsctx;
		u_int16_t	fid = 0;

		vfsctx = vfs_context_create((vfs_context_t)0);

		smb_scred_init(&scred, vfsctx);
		error = smbfs_smb_tmpopen(np, SA_RIGHT_FILE_READ_DATA, &scred,
					  &fid);
		if (!error) {
			MALLOC(sb, void *, np->n_size, M_TEMP, M_WAITOK);
			uio = uio_create(1, 0, UIO_SYSSPACE, UIO_READ);
			uio_addiov(uio, CAST_USER_ADDR_T(sb), np->n_size);
			error = smb_read(ssp, fid, uio, &scred);
			uio_free(uio);
			cerror = smbfs_smb_tmpclose(np, fid, &scred);
			if (cerror)
				SMBERROR("error %d closing fid %d file %.*s\n",
					 cerror, fid, np->n_nmlen, np->n_name);
			if (!error &&
			    !bcmp(sb, smb_symmagic, SMB_SYMMAGICLEN)) {
				for (cp = &sb[SMB_SYMMAGICLEN];
				     cp < &sb[SMB_SYMMAGICLEN+SMB_SYMLENLEN-1];
				     cp++) {
					if (!isdigit(*cp))
						break;
					len *= 10;
					len += *cp - '0';
				}
				cp++; /* skip newline */
				if (cp != &sb[SMB_SYMMAGICLEN+SMB_SYMLENLEN] ||
				    len > np->n_size - SMB_SYMHDRLEN) {
					SMBERROR("bad symlink length\n");
				} else {
					MD5Init(&md5);
					MD5Update(&md5, &sb[SMB_SYMHDRLEN],
						  len);
					MD5Final((u_char *)state, &md5);
					(void)sprintf(m5b, "%08x%08x%08x%08x",
						      state[0], state[1],
						      state[2], state[3]);
					if (bcmp(cp, m5b, SMB_SYMMD5LEN-1)) {
						SMBERROR("bad symlink md5\n");
					} else {
						vfsp.vnfs_vtype = VLNK;
						np->n_size = len;
					}
				}
			}
			FREE(sb, M_TEMP);
		}
		vfs_context_rele(vfsctx);
	}
	vfsp.vnfs_filesize = np->n_size;
	error = vnode_create(VNCREATE_FLAVOR, VCREATESIZE, &vfsp, &vp);
	if (error)
		goto errout;
	np->n_vnode = vp;

	smbfs_attr_cacheenter(vp, fap);	/* update the attr_cache info */

	*vpp = vp;
	CLR(np->n_flag, NALLOC);
        if (ISSET(np->n_flag, NWALLOC))
                wakeup(np);
	return 0;
errout:
	if (np->n_flag & NREFPARENT) {
		vnode_get(dvp);
		vnode_rele(dvp);
		vnode_put(dvp);
	}
	smb_vhashrem(np);
        if (ISSET(np->n_flag, NWALLOC))
                wakeup(np);
	if (namep)
		smbfs_name_free(namep);
	FREE(np, M_SMBNODE);
	return error;
}

/* freebsd bug: smb_vhashrem is so we cant nget unlinked nodes */
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


/*
 * Free smbnode, and give vnode back to system
 */
int
smbfs_reclaim(ap)
	struct vnop_reclaim_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_vp;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t vp = ap->a_vp;
	vnode_t dvp;
	struct smbnode *np = VTOSMB(vp);
	struct smbmount *smp = VTOSMBFS(vp);
	
	SET(np->n_flag, NTRANSIT);

	dvp = (np->n_parent && (np->n_flag & NREFPARENT)) ?
	    np->n_parent->n_vnode : NULL;

	smb_vhashrem(np);

	cache_purge(vp);
	if (smp->sm_root == np) {
		SMBVDEBUG("root vnode\n");
		smp->sm_root = NULL;
	}
	vnode_clearfsnode(vp);
	if (np->n_name)
		smbfs_name_free(np->n_name);
        CLR(np->n_flag, (NALLOC|NTRANSIT));
        if (ISSET(np->n_flag, NWALLOC) || ISSET(np->n_flag, NWTRANSIT)) {
        	CLR(np->n_flag, (NWALLOC|NWTRANSIT));
                wakeup(np);
	}
	FREE(np, M_SMBNODE);
	if (dvp) {
		vnode_get(dvp);
		vnode_rele(dvp);
		vnode_put(dvp);
	}
	return 0;
}


int
smbfs_inactive(ap)
	struct vnop_inactive_args /* {
		struct vnodeop_desc *a_desc;
		vnode_t a_vp;
		vfs_context_t a_context;
	} */ *ap;
{
	vnode_t vp = ap->a_vp;
	struct smbnode *np = VTOSMB(vp);
	struct smb_cred scred;
	int error;

	if (np->n_fidrefs) {
		if (np->n_fidrefs != 1)
			SMBERROR("opencount %d fid %d file %.*s\n",
				 np->n_fidrefs, np->n_fid,
				 np->n_nmlen, np->n_name);
		error = smbfs_vinvalbuf(vp, V_SAVE, ap->a_context, 1);
		smb_scred_init(&scred, ap->a_context);
		np->n_fidrefs = 0;
		error = smbfs_smb_close(np->n_mount->sm_share, np->n_fid, 
					NULL, &scred);
		if (error)
			SMBERROR("error %d closing fid %d file %.*s\n", error,
				 np->n_fid, np->n_nmlen, np->n_name);
		np->n_fid = 0;
	}
	return (0);
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
smbfs_attr_cacheenter(vnode_t vp, struct smbfattr *fap)
{
	struct smbnode *np = VTOSMB(vp);
	int vtype;
	struct timespec ts;

	vtype = vnode_vtype(vp);

	if (vtype == VREG) {
		if (np->n_size != fap->fa_size) {
			off_t old, newround;
			if (timespeccmp(&fap->fa_reqtime, &np->n_sizetime, <))
				return; /* we lost the race */
			old = (off_t)np->n_size;
			newround = round_page_64((off_t)fap->fa_size);
			if (old > newround) {
				if (!ubc_sync_range(vp, newround, old,
						    UBC_INVALIDATE))
					panic("smbfs_attr_cacheenter: UBC_INVALIDATE");
			}
			smbfs_setsize(vp, fap->fa_size);
		}
	} else if (vtype == VDIR) {
		np->n_size = 16384; 	/* should be a better way ... */
		/*
		 * Don't allow mtime to go backwards.
		 * Yes this has its flaws.  Better ideas are welcome!
		 */
		if (timespeccmp(&fap->fa_mtime, &np->n_mtime, <))
			fap->fa_mtime = np->n_mtime;
	} else if (vtype != VLNK)
		return;
	np->n_mtime = fap->fa_mtime;
	np->n_dosattr = fap->fa_attr;
	np->n_flag &= ~NATTRCHANGED;
	nanotime(&ts);
	np->n_attrage = ts.tv_sec;
	return;
}

int
smbfs_attr_cachelookup(vnode_t vp, struct vnode_attr *va,
		       struct smb_cred *scredp)
{
	struct smbnode *np = VTOSMB(vp);
	struct smbmount *smp = VTOSMBFS(vp);
	time_t attrtimeo;
	struct timespec ts;
	mode_t	type;

	/*
	 * Determine attrtimeo. It will be something between SMB_MINATTRTIMO and
	 * SMB_MAXATTRTIMO where recently modified files have a short timeout
	 * and files that haven't been modified in a long time have a long
	 * timeout. This is the same algorithm used by NFS.
	 */
	nanotime(&ts);
	attrtimeo = (ts.tv_sec - np->n_mtime.tv_sec) / 10;
	if (attrtimeo < SMB_MINATTRTIMO) {
		attrtimeo = SMB_MINATTRTIMO;
	} else if (attrtimeo > SMB_MAXATTRTIMO)
		attrtimeo = SMB_MAXATTRTIMO;
	/* has too much time passed? */
	if ((ts.tv_sec - np->n_attrage) > attrtimeo)
		return (ENOENT);

	if (!va)
		return (0);

	switch (vnode_vtype(vp)) {
	    case VREG:
		type = S_IFREG;
		break;
	    case VLNK:
		type = S_IFLNK;
		break;
	    case VDIR:
		type = S_IFDIR;
		break;
	    default:
		SMBERROR("vnode_vtype %d\n", vnode_vtype(vp));
		return (EINVAL);
	}
	VATTR_RETURN(va, va_data_size, np->n_size);
	VATTR_RETURN(va, va_nlink, 1);
	if (VATTR_IS_ACTIVE(va, va_uid) || VATTR_IS_ACTIVE(va, va_gid) ||
	    VATTR_IS_ACTIVE(va, va_mode)) {
		if (!(np->n_flag & NGOTIDS)) {
			np->n_mode = type;
			if (smp->sm_flags & FILE_PERSISTENT_ACLS) {
				if (!smbfs_getids(np, scredp)) {
					np->n_flag |= NGOTIDS;
					np->n_mode |= ACCESSPERMS; /* 0777 */
				}
			}
			if (!(np->n_flag & NGOTIDS)) {
				np->n_flag |= NGOTIDS;
				np->n_uid = smp->sm_args.uid;
				np->n_gid = smp->sm_args.gid;
				if (vnode_vtype(vp) == VDIR)
					np->n_mode |= smp->sm_args.dir_mode;
				else	/* symlink and regular file */
					np->n_mode |= smp->sm_args.file_mode;
			}
		}
		VATTR_RETURN(va, va_mode, np->n_mode);
		VATTR_RETURN(va, va_uid, np->n_uid);
		VATTR_RETURN(va, va_gid, np->n_gid);
	}
	VATTR_RETURN(va, va_fsid, vfs_statfs(vnode_mount(vp))->f_fsid.val[0]);
	VATTR_RETURN(va, va_fileid, np->n_ino ? np->n_ino : 2);
	VATTR_RETURN(va, va_iosize, SSTOVC(smp->sm_share)->vc_txmax);
	VATTR_RETURN(va, va_modify_time, np->n_mtime);
	VATTR_RETURN(va, va_access_time, np->n_atime);
	/* XXX we could provide create_time here */
	return (0);
}

/*
 * Some SMB servers don't exhibit POSIX behaviour with regard to 
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
 * XXX note that better behaviour with non-compliant servers
 *     could be obtained by bumping the mtime forwards when 
 *     an update for an invalidated entry returns a nonsensical
 *     mtime.  
 */

void
smbfs_attr_touchdir(struct smbnode *dnp)
{
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
	nanotime(&ts);
	if (timespeccmp(&dnp->n_mtime, &ts, <))
		dnp->n_mtime = ts;
	/*
	 * Invalidate the cache, so that we go to the wire
	 * to check that the server doesn't have a better
	 * timestamp next time we care.
	 */
	smbfs_attr_cacheremove(dnp);
}


void
smbfs_setsize(vnode_t vp, off_t size)
{
	struct smbnode *np = VTOSMB(vp);

	/*
	 * n_size is used by smbfs_pageout so it must be
	 * changed before we call setsize
	 */
	np->n_size = size;
	vnode_pager_setsize(vp, size);
	/*
	 * this lets us avoid a race with readdir which resulted in
	 * a stale n_size, which in the worst case yielded data corruption.
	 */
	nanotime(&np->n_sizetime);
}
