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
 * $Id: smbfs_node.c,v 1.23 2003/09/21 20:53:11 lindak Exp $
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
#ifndef APPLE
#include <vm/vm.h>
#include <vm/vm_extern.h>
/*#include <vm/vm_page.h>
#include <vm/vm_object.h>*/
#endif /* APPLE */
#include <sys/queue.h>

#ifndef APPLE
#include <sys/sysctlconf.h>
#endif

#ifdef APPLE
#include <sys/smb_apple.h>
#endif
#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_subr.h>

#include <fs/smbfs/smbfs.h>
#include <fs/smbfs/smbfs_node.h>
#include <fs/smbfs/smbfs_subr.h>

#define	SMBFS_NOHASH(smp, hval)	(&(smp)->sm_hash[(hval) & (smp)->sm_hashlen])
#define	smbfs_hash_lock(smp, p)		lockmgr(&smp->sm_hashlock, LK_EXCLUSIVE, NULL, p)
#define	smbfs_hash_unlock(smp, p)	lockmgr(&smp->sm_hashlock, LK_RELEASE, NULL, p)


extern vop_t **smbfs_vnodeop_p;

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

int
smbfs_hashprint(struct mount *mp)
{
	struct smbmount *smp = VFSTOSMBFS(mp);
	struct smbnode_hashhead *nhpp;
	struct smbnode *np;
	u_long i;

	for(i = 0; i <= smp->sm_hashlen; i++) {
		nhpp = &smp->sm_hash[i];
		LIST_FOREACH(np, nhpp, n_hash)
			vprint(NULL, SMBTOV(np));
	}
	return 0;
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

int
smbfs_nget(struct mount *mp, struct vnode *dvp, const char *name, int nmlen,
	struct smbfattr *fap, struct vnode **vpp)
{
	struct proc *p = curproc;	/* XXX */
	struct smbmount *smp = VFSTOSMBFS(mp);
	struct smbnode_hashhead *nhpp;
	struct smbnode *np, *np2, *dnp;
	struct vnode *vp;
	u_long hashval;
	int error;

	*vpp = NULL;
	if (smp->sm_status & SM_STATUS_FORCE)
		return ENXIO;
	if (smp->sm_root != NULL && dvp == NULL) {
		SMBERROR("do not allocate root vnode twice!\n");
		return EINVAL;
	}
	if (nmlen == 2 && bcmp(name, "..", 2) == 0) {
		if (dvp == NULL)
			return EINVAL;
		vp = VTOSMB(dvp)->n_parent->n_vnode;
		error = vget(vp, LK_EXCLUSIVE, p);
		if (error == 0)
			*vpp = vp;
		return error;
	} else if (nmlen == 1 && name[0] == '.') {
		SMBERROR("do not call me with dot!\n");
		return EINVAL;
	}
	dnp = dvp ? VTOSMB(dvp) : NULL;
	if (dnp == NULL && dvp != NULL) {
		vprint("smbfs_nget: dead parent vnode", dvp);
		return EINVAL;
	}
	hashval = smbfs_hash(name, nmlen);
retry:
	smbfs_hash_lock(smp, p);
loop:
	nhpp = SMBFS_NOHASH(smp, hashval);
	LIST_FOREACH(np, nhpp, n_hash) {
		vp = SMBTOV(np);
		if (np->n_parent != dnp ||
		    np->n_nmlen != nmlen || bcmp(name, np->n_name, nmlen) != 0)
			continue;
		VI_LOCK(vp);
		smbfs_hash_unlock(smp, p);
		if (vget(vp, LK_EXCLUSIVE | LK_INTERLOCK, p) != 0)
			goto retry;
		*vpp = vp;
		/* update the attr_cache info if the file is clean */
		if (fap && !(np->n_flag & NFLUSHWIRE))
			smbfs_attr_cacheenter(vp, fap);
		return 0;
	}
	smbfs_hash_unlock(smp, p);
	/*
	 * If we don't have node attributes, then it is an explicit lookup
	 * for an existing vnode.
	 */
	if (fap == NULL)
		return ENOENT;

	MALLOC(np, struct smbnode *, sizeof *np, M_SMBNODE, M_WAITOK);
#ifdef APPLE
	/* lock upfront to avoid races with unmount */
	bzero(np, sizeof(*np));
	lockinit(&np->n_lock, PINOD, "smbnode", 0, LK_CANRECURSE);
	lockmgr(&np->n_lock, LK_EXCLUSIVE, 0, p);
#endif /* APPLE */
	error = getnewvnode(VT_SMBFS, mp, smbfs_vnodeop_p, &vp);
	if (error) {
		FREE(np, M_SMBNODE);
		return error;
	}
	vp->v_type = fap->fa_attr & SMB_FA_DIR ? VDIR : VREG;
#ifndef APPLE
	bzero(np, sizeof(*np));
#endif
	vp->v_data = np;
	np->n_vnode = vp;
	np->n_mount = VFSTOSMBFS(mp);
	np->n_nmlen = nmlen;
	np->n_name = smbfs_name_alloc(name, nmlen);
	np->n_ino = fap->fa_ino;

	if (dvp) {
		np->n_parent = dnp;
		if (/*vp->v_type == VDIR &&*/ (dvp->v_flag & VROOT) == 0) {
			vref(dvp);
			np->n_flag |= NREFPARENT;
		}
	} else if (vp->v_type == VREG)
		SMBERROR("new vnode '%s' born without parent ?\n", np->n_name);

	/* update the attr_cache info */
	if (fap)
		smbfs_attr_cacheenter(vp, fap);
#ifdef APPLE
	/* Call ubc_info_init() after adding to the attr_cache so that
	 * ubc_info_init's getattr request won't cause network activity.
	 */
	if (vp->v_type == VREG)
		(void) ubc_info_init(vp);
#else
	lockinit(&np->n_lock, PINOD, "smbnode", 0, LK_CANRECURSE);
	vn_lock(vp, LK_EXCLUSIVE | LK_RETRY, p);
#endif /* APPLE */
	smbfs_hash_lock(smp, p);
	LIST_FOREACH(np2, nhpp, n_hash) {
		if (np2->n_parent != dnp ||
		    np2->n_nmlen != nmlen || bcmp(name, np2->n_name, nmlen) != 0)
			continue;
		vput(vp);
/*		smb_name_free(np->n_name);
		FREE(np, M_SMBNODE);*/
		goto loop;
	}
	LIST_INSERT_HEAD(nhpp, np, n_hash);
	smbfs_hash_unlock(smp, p);
	*vpp = vp;
	return 0;
}

/* freebsd bug: smb_vhashrem is so we cant nget unlinked nodes */
void
smb_vhashrem(struct smbnode *np, struct proc *p)
{
	struct smbmount *smp = np->n_mount;
	
	smbfs_hash_lock(smp, p);
	if (np->n_hash.le_prev) {
		LIST_REMOVE(np, n_hash);
		np->n_hash.le_prev = NULL;
	}
	smbfs_hash_unlock(smp, p);
	return;
}


/*
 * Free smbnode, and give vnode back to system
 */
int
smbfs_reclaim(ap)
	struct vop_reclaim_args /* {
		struct vnode *a_vp;
		struct proc *a_p;
	} */ *ap;
{
	struct vnode *vp = ap->a_vp;
	struct proc *p = ap->a_p;
	struct vnode *dvp;
	struct smbnode *np = VTOSMB(vp);
	struct smbmount *smp = VTOSMBFS(vp);
	
	SMBVDEBUG("%s,%d\n", np->n_name, vp->v_usecount);

	smbfs_hash_lock(smp, p);

	dvp = (np->n_parent && (np->n_flag & NREFPARENT)) ?
	    np->n_parent->n_vnode : NULL;

	if (np->n_hash.le_prev)
#ifdef APPLE
	{
#endif
		LIST_REMOVE(np, n_hash);
#ifdef APPLE
		np->n_hash.le_prev = NULL;
	}
#endif
	cache_purge(vp);
	if (smp->sm_root == np) {
		SMBVDEBUG("root vnode\n");
		smp->sm_root = NULL;
	}
	vp->v_data = NULL;
	smbfs_hash_unlock(smp, p);
	if (np->n_name)
		smbfs_name_free(np->n_name);
	FREE(np, M_SMBNODE);
	if (dvp) {
		VI_LOCK(dvp);
		if (dvp->v_usecount >= 1) {
			VI_UNLOCK(dvp);
			vrele(dvp);
		} else {
			VI_UNLOCK(dvp);
			SMBERROR("BUG: negative use count for parent!\n");
		}
	}
	return 0;
}


int
smbfs_inactive(ap)
	struct vop_inactive_args /* {
		struct vnode *a_vp;
		struct proc *a_p;
	} */ *ap;
{
	struct proc *p = ap->a_p;
	struct ucred *cred = p->p_ucred;
	struct vnode *vp = ap->a_vp;
	struct smbnode *np = VTOSMB(vp);
	struct smb_cred scred;
	int error;

	SMBVDEBUG("%s: %d\n", VTOSMB(vp)->n_name, vp->v_usecount);
	if (np->n_opencount) {
		error = smbfs_vinvalbuf(vp, V_SAVE, cred, p, 1);
		smb_makescred(&scred, p, cred);
		error = smbfs_smb_close(np->n_mount->sm_share, np->n_fid, 
		   &np->n_mtime, &scred);
		np->n_opencount = 0;
	}
	VOP_UNLOCK(vp, 0, p);
	return (0);
}
/*
 * routines to maintain vnode attributes cache
 * smbfs_attr_cacheenter: unpack np.i to vattr structure
 *
 * Note that some SMB servers do not exhibit POSIX behaviour
 * with regard to the mtime on directories.  To work around
 * this, we never allow the mtime on a directory to go backwards,
 * and bump it forwards elsewhere to simulate the correct
 * behaviour.
 */
void
smbfs_attr_cacheenter(struct vnode *vp, struct smbfattr *fap)
{
	struct smbnode *np = VTOSMB(vp);

	if (vp->v_type == VREG) {
		if (np->n_size != fap->fa_size) {
#ifdef APPLE
			off_t old, newround;
#endif
			if (timespeccmp(&fap->fa_reqtime, &np->n_sizetime, <))
				return; /* we lost the race */
#ifdef APPLE
			old = (off_t)np->n_size;
			newround = round_page_64((off_t)fap->fa_size);
			if (old > newround) {
				if (!ubc_invalidate(vp, newround,
						    (size_t)(old - newround)))
					panic("smbfs_attr_cacheenter: ubc_invalidate");
			}
#endif /* APPLE */
			smbfs_setsize(vp, fap->fa_size);
		}
	} else if (vp->v_type == VDIR) {
		np->n_size = 16384; 	/* should be a better way ... */
		/*
		 * Don't allow mtime to go backwards.
		 * Yes this has its flaws.  Better ideas are welcome!
		 */
		if (timespeccmp(&fap->fa_mtime, &np->n_mtime, <))
			fap->fa_mtime = np->n_mtime;
	} else
		return;
	np->n_mtime = fap->fa_mtime;
	np->n_dosattr = fap->fa_attr;
	np->n_flag &= ~NATTRCHANGED;
	np->n_attrage = time_second;
	return;
}

int
smbfs_attr_cachelookup(struct vnode *vp, struct vattr *va)
{
	struct smbnode *np = VTOSMB(vp);
	struct smbmount *smp = VTOSMBFS(vp);
	time_t timediff, attrtimeo;

	/* Determine attrtimeo. This will be something between SMB_MINATTRTIMO and
	 * SMB_MAXATTRTIMO where recently modified files have a short timeout and
	 * files that haven't been modified in a long time have a long timeout.
	 * This is the same algorithm used by NFS.
	 */
	timediff = (time_second - np->n_mtime.tv_sec) / 10;
	if (timediff < SMB_MINATTRTIMO) {
		attrtimeo = SMB_MINATTRTIMO;
	} else if (timediff > SMB_MAXATTRTIMO) {
		attrtimeo = SMB_MAXATTRTIMO;
	} else {
		attrtimeo = timediff;
	}
	/* has too much time passed? */
	if ((time_second - np->n_attrage) > attrtimeo) {
		return ENOENT;
	}

	va->va_type = vp->v_type;		/* vnode type (for create) */
	if (vp->v_type == VREG) {
		va->va_mode = smp->sm_args.file_mode;	/* files access mode and type */
	} else if (vp->v_type == VDIR) {
		va->va_mode = smp->sm_args.dir_mode;	/* files access mode and type */
	} else
		return EINVAL;
	va->va_size = np->n_size;
	va->va_nlink = 1;		/* number of references to file */
	va->va_uid = smp->sm_args.uid;	/* owner user id */
	va->va_gid = smp->sm_args.gid;	/* owner group id */
	va->va_fsid = vp->v_mount->mnt_stat.f_fsid.val[0];
	va->va_fileid = np->n_ino;	/* file id */
	if (va->va_fileid == 0)
		va->va_fileid = 2;
	va->va_blocksize = SSTOVC(smp->sm_share)->vc_txmax;
	va->va_mtime = np->n_mtime;
	va->va_atime = va->va_ctime = va->va_mtime;	/* time file changed */
	va->va_gen = VNOVAL;		/* generation number of file */
	va->va_flags = 0;		/* flags defined for file */
	va->va_rdev = VNOVAL;		/* device the special file represents */
	va->va_bytes = va->va_size;	/* bytes of disk space held by file */
	va->va_filerev = 0;		/* file modification number */
	va->va_vaflags = 0;		/* operations flags */
	return 0;
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
	getnanotime(&ts);
	if (timespeccmp(&dnp->n_mtime, &ts, <))
		dnp->n_mtime = ts;
	/*
	 * Invalidate the cache, so that we go to the wire
	 * to check that the server doesn't have a better
	 * timestamp next time we care.
	 */
	smbfs_attr_cacheremove(dnp);
}


/*
 * XXX TODO
 * FreeBSD (ifndef APPLE) needs this n_sizetime stuff too.
 */
void
smbfs_setsize(struct vnode *vp, off_t size)
{
	struct smbnode *np = VTOSMB(vp);

	/*
	 * n_size is used by smbfs_pageout so it must be
	 * changed before we call setsize
	 */
	np->n_size = size;
#ifdef APPLE
	vnode_pager_setsize(vp, size);
#else
	vnode_pager_setsize(vp, (u_long)size);
#endif
	/*
	 * this lets us avoid a race with readdir which resulted in
	 * a stale n_size, which in the worst case yielded data corruption.
	 */
	getnanotime(&np->n_sizetime);
}
