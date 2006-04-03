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
 * $Id: smbfs_vfsops.c,v 1.73.64.3 2005/09/13 05:08:05 lindak Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>

#include <sys/kauth.h>

#include <sys/syslog.h>
#include <sys/smb_apple.h>
#include <sys/smb_iconv.h>
#include <sys/mchain.h>

#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_dev.h>
#include <netsmb/smb_sleephandler.h>

#include <fs/smbfs/smbfs.h>
#include <fs/smbfs/smbfs_node.h>
#include <fs/smbfs/smbfs_subr.h>

#include <sys/buf.h>

int smbfs_debuglevel = 0;

static int smbfs_version = SMBFS_VERSION;

#ifdef SMBFS_USEZONE
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_zone.h>

vm_zone_t smbfsmount_zone;
#endif

#ifdef SYSCTL_DECL
SYSCTL_DECL(_net_smb);
#endif
SYSCTL_NODE(_net_smb, OID_AUTO, fs, CTLFLAG_RW, 0, "SMB/CIFS file system");
SYSCTL_INT(_net_smb_fs, OID_AUTO, version, CTLFLAG_RD, &smbfs_version, 0, "");
SYSCTL_INT(_net_smb_fs, OID_AUTO, debuglevel, CTLFLAG_RW,
	   &smbfs_debuglevel, 0, "");
extern struct sysctl_oid sysctl__net_smb;
extern struct sysctl_oid sysctl__net_smb_fs_iconv;
extern struct sysctl_oid sysctl__net_smb_fs_iconv_add;
extern struct sysctl_oid sysctl__net_smb_fs_iconv_cslist;
extern struct sysctl_oid sysctl__net_smb_fs_iconv_drvlist;

MALLOC_DEFINE(M_SMBFSHASH, "SMBFS hash", "SMBFS hash table");


static int smbfs_mount(struct mount *, vnode_t, user_addr_t, vfs_context_t);
static int smbfs_root(struct mount *, vnode_t *, vfs_context_t);
static int smbfs_start(struct mount *, int, vfs_context_t);
static int smbfs_vfs_getattr(struct mount *, struct vfs_attr *, vfs_context_t);
static int smbfs_sync(struct mount *, int, vfs_context_t);
static int smbfs_unmount(struct mount *, int, vfs_context_t);
static int smbfs_init(struct vfsconf *vfsp);
static int smbfs_sysctl(int *, u_int, user_addr_t, size_t *, user_addr_t, size_t, vfs_context_t);

static int smbfs_vget(struct mount *, ino64_t , vnode_t *, vfs_context_t);
static int smbfs_fhtovp(struct mount *, int, unsigned char *, vnode_t *, vfs_context_t);
static int smbfs_vptofh(vnode_t, int *, unsigned char *, vfs_context_t);

static struct vfsops smbfs_vfsops = {
	smbfs_mount,
	smbfs_start,
	smbfs_unmount,
	smbfs_root,
	NULL,			/* quotactl */
	smbfs_vfs_getattr,
	smbfs_sync,
	smbfs_vget,
	smbfs_fhtovp,
	smbfs_vptofh,
	smbfs_init,
	smbfs_sysctl
};


VFS_SET(smbfs_vfsops, smbfs, VFCF_NETWORK);

#ifdef MODULE_DEPEND
MODULE_DEPEND(smbfs, netsmb, NSMB_VERSION, NSMB_VERSION, NSMB_VERSION);
MODULE_DEPEND(smbfs, libiconv, 1, 1, 1);
#endif

int smbfs_pbuf_freecnt = -1;	/* start out unlimited */

lck_grp_attr_t *co_grp_attr;
lck_grp_t *co_lck_group;
lck_attr_t *co_lck_attr;
lck_grp_attr_t *vcst_grp_attr;
lck_grp_t *vcst_lck_group;
lck_attr_t *vcst_lck_attr;
lck_grp_attr_t *ssst_grp_attr;
lck_grp_t *ssst_lck_group;
lck_attr_t *ssst_lck_attr;
lck_grp_attr_t *iodflags_grp_attr;
lck_grp_t *iodflags_lck_group;
lck_attr_t *iodflags_lck_attr;
lck_grp_attr_t *iodrq_grp_attr;
lck_grp_t *iodrq_lck_group;
lck_attr_t *iodrq_lck_attr;
lck_grp_attr_t *iodev_grp_attr;
lck_grp_t *iodev_lck_group;
lck_attr_t *iodev_lck_attr;
lck_grp_attr_t *srs_grp_attr;
lck_grp_t *srs_lck_group;
lck_attr_t *srs_lck_attr;
lck_grp_attr_t *nbp_grp_attr;
lck_grp_t *nbp_lck_group;
lck_attr_t *nbp_lck_attr;
lck_grp_attr_t   * dev_lck_grp_attr;
lck_grp_t  * dev_lck_grp;
lck_attr_t * dev_lck_attr;
lck_mtx_t * dev_lck;
lck_grp_attr_t   * hash_lck_grp_attr;
lck_grp_t  * hash_lck_grp;
lck_attr_t * hash_lck_attr;

struct smbmnt_carg {
	vfs_context_t vfsctx;
	struct mount *mp;
	int found;
};

static int
smb_mnt_callback(struct mount *mp2, void *args)
{
	struct smbmnt_carg *cargp;

	cargp = (struct smbmnt_carg *)args;

	if (vfs_statfs(mp2)->f_owner !=
	    kauth_cred_getuid(vfs_context_ucred(cargp->vfsctx)))
		return (VFS_RETURNED);
	if (strncmp(vfs_statfs(mp2)->f_mntfromname,
		    vfs_statfs(cargp->mp)->f_mntfromname, MAXPATHLEN))
		return (VFS_RETURNED);
	cargp->found = 1;
	return (VFS_RETURNED_DONE);
}

/*
 * These memberd constants should be in an memberd include file, obviously.
 *
 * memberd generates these UUIDs for well-known SIDs, and for translations
 * which Directory Services doesn't resolve.
 *
 * These temporary ids should probably go away altogether.  A better approach
 * would be for the requests and replies between kernel and memberd to include
 * a scope or other indication of the server used to translate the identity.
 * So we might pass our file server's IP up and memberd/DS would use the
 * appropriate Active Directory or LDAP server, falling back to the file
 * server itself in the "ad hoc" scenario.
 */
#define IS_MEMBERD_TEMPUUID(uuidp) \
	((*(u_int64_t *)(uuidp) == 0xFFFFEEEEDDDDCCCCULL &&	\
	  *((u_int32_t *)(uuidp)+2) == 0xBBBBAAAA) ||		\
	 (*(u_int64_t *)(uuidp) == 0xAAAABBBBCCCCDDDDULL &&	\
	 *((u_int32_t *)(uuidp)+2) == 0xEEEEFFFF))

static int
smbfs_aclsflunksniff(struct smbmount *smp, struct smb_cred *scrp)
{
	struct smbnode *np = smp->sm_root;
	struct smb_share *ssp = smp->sm_share;
	int	error, cerror;
	struct ntsecdesc	*w_secp = NULL;	/* Wire sec descriptor */
	struct ntsid *usidp, *gsidp;
	ntsid_t	kntsid;
	guid_t	guid;
	u_int8_t	ntauth[SIDAUTHSIZE] = { 0, 0, 0, 0, 0, 5 };
	uid_t	uid;
	u_int16_t	fid = 0;

	/*
	 * Does the server claim ACL support for this volume?
	 */
	if (!(smp->sm_flags & FILE_PERSISTENT_ACLS))
		goto err;

	/*
	 * Get a security descriptor from the share's root.
	 */
	error = smbfs_smb_tmpopen(np, STD_RIGHT_READ_CONTROL_ACCESS, scrp,
				  &fid);
	if (error) {
		SMBERROR("smbfs_smb_tmpopen error %d\n", error);
		goto out;
	}
	error = smbfs_smb_getsec(ssp, fid, scrp,
				 OWNER_SECURITY_INFORMATION |
				 GROUP_SECURITY_INFORMATION, &w_secp);
	cerror = smbfs_smb_tmpclose(np, fid, scrp);
	if (cerror)
		SMBERROR("error %d closing root fid %d\n", cerror, fid);
	if (error) {
		SMBERROR("smbfs_smb_getsec error %d\n", error);
		goto out;
	}
	/*
	 * A null w_secp commonly means a FAT filesystem, but in that
	 * case the FILE_PERSISTENT_ACL bit should not have been set.
	 */
	if (w_secp == NULL) {
		SMBERROR("null w_secp\n");
		goto err;
	}
	usidp = sdowner(w_secp);
	if (!usidp) {
		SMBERROR("null owner\n");
		goto err;
	}
	smb_sid_endianize(usidp);
	smb_sid2sid16(usidp, &kntsid);
	error = kauth_cred_ntsid2guid(&kntsid, &guid);
	if (error) {
		SMBERROR("kauth_cred_ntsid2guid error %d (owner)\n", error);
		goto out;
	}
	if (IS_MEMBERD_TEMPUUID(&guid)) {
		SMBERROR("(fyi) user sid %s didnt map\n", smb_sid2str(usidp));
		goto err;
	}
	gsidp = sdgroup(w_secp);
	if (!gsidp) {
		SMBERROR("null group\n");
		goto err;
	}
	smb_sid_endianize(gsidp);
	smb_sid2sid16(gsidp, &kntsid);
	error = kauth_cred_ntsid2guid(&kntsid, &guid);
	if (error) {
		SMBERROR("kauth_cred_ntsid2guid error %d (group)\n", error);
		goto out;
	}
	if (IS_MEMBERD_TEMPUUID(&guid)) {
		SMBERROR("group sid %s didnt map\n", smb_sid2str(gsidp));
		goto err;
	}

	/*
	 * Can our DS subsystem give us a SID for the mounting user?
	 */
	error = kauth_cred_uid2ntsid(smp->sm_args.uid, &kntsid);
	if (error) {
		SMBERROR("sm_args.uid %d, error %d\n", smp->sm_args.uid, error);
		goto out;
	}
	/*
	 * We accept SIDS of the form S-1-5-x-*, where x is not 32
	 * ("builtin" groups) and x > 20 (lower ones are "well-known")
	 * Arguably a check could/should be done on the final subauthority
	 * as those in the 500s are domain-specific well-knowns, while
	 * those around 1000 are real users.
	 */
	if (kntsid.sid_kind != 1 ||
	    bcmp(kntsid.sid_authority, ntauth, sizeof(ntauth)) ||
	    kntsid.sid_authcount < 1 ||
	    kntsid.sid_authorities[0] <= 20 ||
	    kntsid.sid_authorities[0] == 32) {
		SMBERROR("sm_args.uid %d, sid %s\n", smp->sm_args.uid,
			 smb_sid2str((struct ntsid *)&kntsid));
		goto err;
	}
	error = kauth_cred_ntsid2uid(&kntsid, &uid);
	if (error) {
		SMBERROR("sid %s, error %d\n",
			 smb_sid2str((struct ntsid *)&kntsid), error);
		goto out;
	}
	if (uid == smp->sm_args.uid)
		goto out;
	SMBERROR("sm_args.uid %d, uid %d\n", smp->sm_args.uid, uid);
err:
	error = -1;
out:
	if (w_secp)
		FREE(w_secp, M_TEMP);
	return (error);
}

static int
smbfs_mount(struct mount *mp, vnode_t devvp, user_addr_t data,
	    vfs_context_t vfsctx)
{
	struct smbfs_args args;		/* will hold data from mount request */
	struct smbmount *smp = NULL;
	struct smb_vc *vcp;
	struct smb_share *ssp = NULL;
	vnode_t vp;
	struct smb_cred scred;
	int error;
	char *pc, *pe;
	struct smbmnt_carg carg;
	u_int redo_mntfromname = 0;
	u_int namelen = 0;

	if (data == USER_ADDR_NULL) {
		printf("missing data argument\n");
		return (EINVAL);
	}
	if (vfs_isupdate(mp)) {
		printf("MNT_UPDATE not implemented");
		return (ENOTSUP);
	}
	error = copyin(data, (caddr_t)&args, sizeof(args));
	if (error)
		return (error);
	if (args.version != SMBFS_VERSION) {
		printf("mount version mismatch: kernel=%d, mount=%d\n",
		    SMBFS_VERSION, args.version);
		return (EINVAL);
	}
	error = smb_dev2share(args.dev, &ssp);
	if (error) {
		printf("invalid device handle %d (%d)\n", args.dev, error);
		return (error);
	}
	smb_scred_init(&scred, vfsctx);
	vcp = SSTOVC(ssp);
	smb_share_unlock(ssp, vfs_context_proc(vfsctx));

#ifdef SMBFS_USEZONE
	smp = zalloc(smbfsmount_zone);
#else
	MALLOC(smp, struct smbmount*, sizeof(*smp), M_SMBFSDATA, M_USE_RESERVE);
#endif
	if (smp == NULL) {
		printf("could not alloc smbmount\n");
		error = ENOMEM;
		goto bad;
	}
	bzero(smp, sizeof(*smp));
	smp->sm_mp = mp;
	vfs_setfsprivate(mp, (void *)smp);
	ssp->ss_mount = smp;
	smp->sm_hash = hashinit(desiredvnodes, M_SMBFSHASH, &smp->sm_hashlen);
	if (smp->sm_hash == NULL)
		goto bad;
	smp->sm_hashlock = lck_mtx_alloc_init(hash_lck_grp, hash_lck_attr);
	smp->sm_share = ssp;
	smp->sm_root = NULL;
	smp->sm_args = args;
	smp->sm_caseopt = args.caseopt;
	smp->sm_args.file_mode = smp->sm_args.file_mode & ACCESSPERMS;
	smp->sm_args.dir_mode  = smp->sm_args.dir_mode & ACCESSPERMS;

smbfs_make_mntfromname:

	pc = vfs_statfs(mp)->f_mntfromname;
	/* 
	 * XXX In the future we could use the whole buffer
         * if they change the design to allow a bigger string
	 */
	pe = pc + MNAMELEN;

	bzero(pc, MNAMELEN);
	*pc++ = '/';
	*pc++ = '/';
	if (vcp->vc_domain) {
		namelen = strlen(vcp->vc_domain);
		if (pc + namelen > pe- 2) {
			/* Log warning but don't return error */ 
			SMBERROR("smbfs_mount: domain won't fit in buffer\n");
			goto smbfs_quit_mntfromname;
		}
		strncpy(pc, vcp->vc_domain, namelen);
		pc += namelen;
		*(pc++) = ';';
	}
	if (vcp->vc_username) {
		namelen = strlen(vcp->vc_username);
		if (pc + namelen > pe - 2) {
			/* Log warning but don't return error */ 
			SMBERROR("smbfs_mount: username won't fit in buffer\n");
			goto smbfs_quit_mntfromname;
		}
		strncpy(pc, vcp->vc_username, namelen);
		pc += namelen;
		*(pc++) = '@';
	}

 	/* If possible store original server component for reconnect */
    	if ((args.utf8_servname[0]) && !(redo_mntfromname)) {
        	namelen = strlen(args.utf8_servname);
        	/* Leave space for "/", share, null */
        	if (pc + namelen + strlen(ssp->ss_name) + 2 > pe) { /* Can retry with NB name */
            		redo_mntfromname = 1;
            		SMBERROR("smbfs_mount: warning: server/share name too big\n");
            		goto smbfs_make_mntfromname; /* Try with NB name */
        	}
		/* Since server, "/", and share fit, we know the server does */
        	strncpy(pc, args.utf8_servname, namelen);
        	pc += namelen;
    	} else { /* Use NB name */
        	namelen = strlen(vcp->vc_srvname);
        	if (pc + namelen + 2 > pe) {
            	/* Log warning but don't return error */
            	SMBERROR("smbfs_mount: server won't fit in buffer\n");
            	goto smbfs_quit_mntfromname;
        	}
        	strncpy(pc, vcp->vc_srvname, namelen);
        	pc += namelen;
        	if (pc + strlen(ssp->ss_name) + 2 > pe) {
            		/* Log warning but don't return error */
            		SMBERROR("smbfs_mount: server/share won't fit in buffer\n");
            		goto smbfs_quit_mntfromname;
        	}
    	}
    	*(pc++) = '/';
    	strcpy(pc, ssp->ss_name);

smbfs_quit_mntfromname:

	/*
	 * XXX
	 * This circumvents the "unique disk id" design flaw by disallowing
	 * multiple mounts of the same filesystem as the same user.  The flaw
	 * is that URLMount, DiskArb, FileManager, Finder added new semantics
	 * to f_mntfromname, requiring that field to be a unique id for a
	 * given mount.  (Another flaw added is to require that that field
	 * have sufficient information to remount.  That is not solved here.)
	 * This is XXX because it cripples multiple mounting, a traditional
         * unix feature useful in multiuser and chroot-ed environments.  This
	 * limitation can often be (manually) avoided by altering the remote
	 * login name - even a difference in case is sufficient, and
	 * should authenticate as if there were no difference.
	 *
	 * Details:
	 * "funnel" keeps us from having to lock mountlist during scan.
	 * The scan could be more cpu-efficient, but mounts are not a hot spot.
	 * Scanning mountlist is more robust than using smb_vclist.
	 *
	 * Changed for multisession: we now allow the multple mounts if being
	 * done by a different user OR if not on the desktop.
	 */
	if (!(vfs_flags(mp) & MNT_DONTBROWSE)) {
		carg.vfsctx = vfsctx;
		carg.mp = mp;
		carg.found = 0;
		(void)vfs_iterate(0, smb_mnt_callback, &carg);
		if (carg.found) {
			error = EBUSY;
			goto bad;
		}
	}
	vfs_setauthopaque(mp);
	vfs_clearauthopaqueaccess(mp);
	/* protect against invalid mount points */
	smp->sm_args.mount_point[sizeof(smp->sm_args.mount_point) - 1] = '\0';
	vfs_getnewfsid(mp);
	error = smbfs_root(mp, &vp, vfsctx);
	if (error)
		goto bad;
	vfs_clearextendedsecurity(mp);
	error = smbfs_smb_qfsattr(ssp, &smp->sm_flags, &scred);
	if (error) {
		SMBERROR("smbfs_smb_qfsattr error %d\n", error);
	} else if (smbfs_aclsflunksniff(smp, &scred)) {
		smp->sm_flags &= ~FILE_PERSISTENT_ACLS;
	} else
		vfs_setextendedsecurity(mp);

	/*
	 * Ensure cached info is set to reasonable values.
	 *
	 * XXX this call should be done from mount() in vfs layer.
	 */
	(void)vfs_update_vfsstat(mp, vfsctx);

	vnode_ref(vp);
	vnode_put(vp);
	return (error);
bad:
	if (smp) {
		vfs_setfsprivate(mp, (void *)0);
		if (smp->sm_hash)
			free(smp->sm_hash, M_SMBFSHASH);
		lck_mtx_free(smp->sm_hashlock, hash_lck_grp);
#ifdef SMBFS_USEZONE
		zfree(smbfsmount_zone, smp);
#else
		free(smp, M_SMBFSDATA);
#endif
	}
	if (ssp)
		smb_share_put(ssp, &scred);
	return (error);
}

/* Unmount the filesystem described by mp. */
static int
smbfs_unmount(struct mount *mp, int mntflags, vfs_context_t vfsctx)
{
	struct smbmount *smp = VFSTOSMBFS(mp);
	vnode_t vp;
	struct smb_cred scred;
	int error, flags;

	SMBVDEBUG("smbfs_unmount: flags=%04x\n", mntflags);
	flags = 0;
	if (mntflags & MNT_FORCE) {
		flags |= FORCECLOSE;
		wakeup(&smp->sm_status); /* sleeps are down in smb_rq.c */
	}
	error = smbfs_root(mp, &vp, vfsctx);
	if (error)
		return (error);
	error = vflush(mp, vp, flags);
	if (error) {
		vnode_put(vp);
		return (error);
	}
	if (vnode_isinuse(vp, 1)  && !(flags & FORCECLOSE)) {
		printf("smbfs_unmount: usecnt\n");
		vnode_put(vp);
		return (EBUSY);
	}
	vnode_rele(vp);	/* to drop ref taken by smbfs_mount */
	vnode_put(vp);	/* to drop ref taken by VFS_ROOT above */

	(void)vflush(mp, NULLVP, FORCECLOSE);

	if (flags & FORCECLOSE) {
		/*
		 * Shutdown all outstanding I/O requests on this share.
		 */
		smb_iod_shutdown_share(smp->sm_share);
	}
	smb_scred_init(&scred, vfsctx);
	smp->sm_share->ss_mount = NULL;
	smb_share_put(smp->sm_share, &scred);
	vfs_setfsprivate(mp, (void *)0);

	if (smp->sm_hash) {
		free(smp->sm_hash, M_SMBFSHASH);
		smp->sm_hash = (void *)0xDEAD5AB0;
	}
	lck_mtx_free(smp->sm_hashlock, hash_lck_grp);
#ifdef SMBFS_USEZONE
	zfree(smbfsmount_zone, smp);
#else
	free(smp, M_SMBFSDATA);
#endif
	vfs_clearflags(mp, MNT_LOCAL);
	return (error);
}

/* 
 * Return locked root vnode of a filesystem
 */
static int
smbfs_root(struct mount *mp, vnode_t *vpp, vfs_context_t vfsctx)
{
	struct smbmount *smp = VFSTOSMBFS(mp);
	vnode_t vp;
	struct smbnode *np;
	struct smbfattr fattr;
	struct smb_cred scred;
	int error;

	if (smp == NULL) {
		SMBERROR("smp == NULL (bug in umount)\n");
		return (EINVAL);
	}
	if (smp->sm_root) {
		*vpp = SMBTOV(smp->sm_root);
		return (vnode_get(*vpp));
	}
	smb_scred_init(&scred, vfsctx);
	error = smbfs_smb_lookup(NULL, NULL, NULL, &fattr, &scred);
	if (error)
		return (error);
	error = smbfs_nget(mp, NULL, "TheRooT", 7, &fattr, &vp, !MAKEENTRY, 0);
	if (error)
		return (error);
	np = VTOSMB(vp);
	smp->sm_root = np;
	*vpp = vp;
	return (0);
}

/*
 * Vfs start routine, a no-op.
 */
/* ARGSUSED */
static int
smbfs_start(struct mount *mp, int flags, vfs_context_t vfsctx)
{
	#pragma unused(mp, flags, vfsctx)
	return 0;
}

/*ARGSUSED*/
int
smbfs_init(struct vfsconf *vfsp)
{
	#pragma unused(vfsp)
	smb_checksmp();

#ifdef SMBFS_USEZONE
	smbfsmount_zone = zinit("SMBFSMOUNT", sizeof(struct smbmount), 0, 0, 1);
#endif
	SMBVDEBUG("done.\n");
	return 0;
}


/*
 * smbfs_statfs call
 */
int
smbfs_vfs_getattr(struct mount *mp, struct vfs_attr *fsap, vfs_context_t vfsctx)
{
	struct smbmount *smp = VFSTOSMBFS(mp);
	struct smbnode *np = smp->sm_root;
	struct smb_share *ssp = smp->sm_share;
	struct smb_vc *vcp = SSTOVC(ssp);
	struct statfs *cachedstatfs = &smp->sm_statfsbuf;
	struct smb_cred scred;
	struct timespec ts;
	int error = 0;
	int xmax;

	if (np == NULL)
		return (EINVAL);

	/* while there's a smbfs_smb_statfs request outstanding, sleep */
	while (smp->sm_status & SM_STATUS_STATFS) {
		smp->sm_status |= SM_STATUS_STATFS_WANTED;
		(void) msleep((caddr_t)&smp->sm_status, 0, PRIBIO,
			      "smbfs_vfs_getattr", 0);
	}
	/* we're making a request so grab the token */
	smp->sm_status |= SM_STATUS_STATFS;
	/* if from-the-server data is wanted and our cache is stale... */
	nanotime(&ts);
	if (ts.tv_sec - smp->sm_statfstime > SM_MAX_STATFSTIME &&
	    (VFSATTR_IS_ACTIVE(fsap, f_bsize) ||
	     VFSATTR_IS_ACTIVE(fsap, f_blocks) ||
	     VFSATTR_IS_ACTIVE(fsap, f_bfree) ||
	     VFSATTR_IS_ACTIVE(fsap, f_bavail) ||
	     VFSATTR_IS_ACTIVE(fsap, f_files) ||
	     VFSATTR_IS_ACTIVE(fsap, f_ffree))) {
		/* update cached from-the-server data */
		smb_scred_init(&scred, vfsctx);
		if (SMB_DIALECT(SSTOVC(ssp)) >= SMB_DIALECT_LANMAN2_0)
			error = smbfs_smb_statfs2(ssp, cachedstatfs, &scred);
		else
			error = smbfs_smb_statfs(ssp, cachedstatfs, &scred);
		if (error)
			goto releasetoken;
		nanotime(&ts);
		smp->sm_statfstime = ts.tv_sec;
	}
	/* copy results from cached statfs */
	VFSATTR_RETURN(fsap, f_bsize, cachedstatfs->f_bsize);
	VFSATTR_RETURN(fsap, f_blocks, cachedstatfs->f_blocks);
	VFSATTR_RETURN(fsap, f_bfree, cachedstatfs->f_bfree);
	VFSATTR_RETURN(fsap, f_bavail, cachedstatfs->f_bavail);
	VFSATTR_RETURN(fsap, f_files, cachedstatfs->f_files);
	VFSATTR_RETURN(fsap, f_ffree, cachedstatfs->f_ffree);
releasetoken:
	/* we're done, so release the token */
	smp->sm_status &= ~SM_STATUS_STATFS;
	/* if anyone else is waiting, wake them up */
	if (smp->sm_status & SM_STATUS_STATFS_WANTED) {
		smp->sm_status &= ~SM_STATUS_STATFS_WANTED;
		wakeup((caddr_t)&smp->sm_status);
	}
	if (error)
		return (error);

	/*
	 * The Finder will in general use the f_iosize as its i/o
	 * buffer size.  We want to give it the largest size which is less
	 * than the UBC/UPL limit (SMB_IOMAX) but is also a multiple of our
	 * maximum we can xfer in a single smb.
	 */
	xmax = max(vcp->vc_rxmax, vcp->vc_wxmax);
	if (xmax > SMB_IOMAX)
		VFSATTR_RETURN(fsap, f_iosize, SMB_IOMAX);
	else
		VFSATTR_RETURN(fsap, f_iosize, (SMB_IOMAX/xmax) * xmax);

	/*
	 * ref 3984574.  Returning null here keeps vfs from returning
	 * f_mntonname, and causes CarbonCore (File Mgr) to use the
	 * f_mntfromname, as it did (& still does) when an error is returned.
	 */
	if (VFSATTR_IS_ACTIVE(fsap, f_vol_name) && fsap->f_vol_name) {
		*fsap->f_vol_name = '\0';
		VFSATTR_SET_SUPPORTED(fsap, f_vol_name);
	}

	if (VFSATTR_IS_ACTIVE(fsap, f_capabilities)) {
		vol_capabilities_attr_t *cap = &fsap->f_capabilities;

		cap->capabilities[VOL_CAPABILITIES_FORMAT] =
						VOL_CAP_FMT_SYMBOLICLINKS |
						VOL_CAP_FMT_NO_ROOT_TIMES |
						VOL_CAP_FMT_FAST_STATFS;
		cap->capabilities[VOL_CAPABILITIES_INTERFACES] =
						VOL_CAP_INT_FLOCK;
		cap->capabilities[VOL_CAPABILITIES_RESERVED1] = 0;
		cap->capabilities[VOL_CAPABILITIES_RESERVED2] = 0;
		/*
		 * This file system is case sensitive and case preserving, but
		 * servers vary, depending on the underlying volume.  So rather
		 * than providing a wrong yes or no answer we deny knowledge of
		 * VOL_CAP_FMT_CASE_SENSITIVE and VOL_CAP_FMT_CASE_PRESERVING;
		 */
		cap->valid[VOL_CAPABILITIES_FORMAT] =
					VOL_CAP_FMT_PERSISTENTOBJECTIDS |
					VOL_CAP_FMT_SYMBOLICLINKS |
					VOL_CAP_FMT_HARDLINKS |
					VOL_CAP_FMT_JOURNAL |
					VOL_CAP_FMT_JOURNAL_ACTIVE |
					VOL_CAP_FMT_NO_ROOT_TIMES |
					VOL_CAP_FMT_SPARSE_FILES |
					VOL_CAP_FMT_ZERO_RUNS |
					VOL_CAP_FMT_FAST_STATFS;
		cap->valid[VOL_CAPABILITIES_INTERFACES] =
						VOL_CAP_INT_SEARCHFS |
						VOL_CAP_INT_ATTRLIST |
						VOL_CAP_INT_NFSEXPORT |
						VOL_CAP_INT_READDIRATTR |
						VOL_CAP_INT_EXCHANGEDATA |
						VOL_CAP_INT_COPYFILE |
						VOL_CAP_INT_ALLOCATE |
						VOL_CAP_INT_VOL_RENAME |
						VOL_CAP_INT_ADVLOCK |
						VOL_CAP_INT_FLOCK;
		cap->valid[VOL_CAPABILITIES_RESERVED1] = 0;
		cap->valid[VOL_CAPABILITIES_RESERVED2] = 0;
		VFSATTR_SET_SUPPORTED(fsap, f_capabilities);
	}
	return (error);
}

struct smbfs_sync_cargs {
	vfs_context_t	vfsctx;
	int	waitfor;
	int	error;
};


static int
smbfs_sync_callback(vnode_t vp, void *args)
{
	int error;
	struct smbfs_sync_cargs *cargs;

	cargs = (struct smbfs_sync_cargs *)args;

	if (vnode_hasdirtyblks(vp)) {
		error = VNOP_FSYNC(vp, cargs->waitfor, cargs->vfsctx);

		if (error)
			cargs->error = error;
	}
	return (VNODE_RETURNED);
}

/*
 * Flush out the buffer cache
 */
/* ARGSUSED */
static int
smbfs_sync(struct mount *mp, int waitfor, vfs_context_t vfsctx)
{
	struct smbfs_sync_cargs args;

	args.vfsctx = vfsctx;
	args.waitfor = waitfor;
	args.error = 0;
	/*
	 * Force stale buffer cache information to be flushed.
	 *
	 * sbmfs_sync_callback will be called for each vnode
	 * hung off of this mount point... the vnode will be
	 * properly referenced and unreferenced around the callback
	 */
	vnode_iterate(mp, VNODE_ITERATE_ACTIVE, smbfs_sync_callback, (void *)&args);

	return (args.error);
}

/*
 * smbfs flat namespace lookup. Unsupported.
 */
/* ARGSUSED */
static int
smbfs_vget(struct mount *mp, ino64_t ino, vnode_t *vpp,
	   vfs_context_t vfsctx)
{
	#pragma unused(mp, ino, vpp, vfsctx)
	return (ENOTSUP);
}

/* ARGSUSED */
static int
smbfs_fhtovp(struct mount *mp, int fhlen, unsigned char *fhp, vnode_t *vpp,
	     vfs_context_t vfsctx)
{
	#pragma unused(mp, fhlen, fhp, vpp, vfsctx)
	return (EINVAL);
}

/*
 * Vnode pointer to File handle, should never happen either
 */
/* ARGSUSED */
static int
smbfs_vptofh(vnode_t vp, int *fhlen, unsigned char *fhp, vfs_context_t vfsctx)
{
	#pragma unused(vp, fhlen, fhp, vfsctx)
	return (EINVAL);
}

/*
 * smbfs_sysctl handles the VFS_CTL_QUERY request which tells interested
 * parties if the connection with the remote server is up or down.
 */
static int
smbfs_sysctl(int * name, u_int namelen, user_addr_t oldp, size_t * oldlenp,
	     user_addr_t newp, size_t newlen, vfs_context_t vfsctx)
{
	#pragma unused(oldlenp, newp, newlen)

	int error;
	struct sysctl_req *req;
	struct vfsidctl vc;
	struct user_vfsidctl user_vc;
	struct mount *mp;
	struct smbmount *smp;
	struct vfsquery vq;

	/*
	 * All names at this level are terminal.
	 */
	if (namelen > 1)
		return (ENOTDIR);	/* overloaded */
	switch (name[0]) {
	    case VFS_CTL_QUERY:
		req = CAST_DOWN(struct sysctl_req *, oldp);	/* we're new style vfs sysctl. */
		if (vfs_context_is64bit(vfsctx)) {
			error = SYSCTL_IN(req, &user_vc, sizeof(user_vc));
			if (error) break;
			mp = vfs_getvfs(&user_vc.vc_fsid);
		} else {
			error = SYSCTL_IN(req, &vc, sizeof(vc));
			if (error) break;
			mp = vfs_getvfs(&vc.vc_fsid);
		}
		if (!mp) {
			error = ENOENT;
		} else {
			smp = VFSTOSMBFS(mp);
			bzero(&vq, sizeof(vq));
			if (smp && (smp->sm_status & SM_STATUS_DEAD))
				vq.vq_flags |= VQ_DEAD;
			else if (smp && (smp->sm_status & SM_STATUS_TIMEO))
				vq.vq_flags |= VQ_NOTRESP;
			error = SYSCTL_OUT(req, &vq, sizeof(vq));
		}
		break;
	    default:
		error = ENOTSUP;
		break;
	}
	return (error);
}

static char smbfs_name[MFSNAMELEN] = "smbfs";

kmod_info_t *smbfs_kmod_infop;

typedef int (*PFI)();

extern struct vnodeopv_desc smbfs_vnodeop_opv_desc;
static struct vnodeopv_desc *smbfs_vnodeop_opv_desc_list[1] =
{
	&smbfs_vnodeop_opv_desc
};


extern int version_major;
extern int version_minor;

static vfstable_t  smbfs_vfsconf;


__private_extern__ int
smbfs_module_start(kmod_info_t *ki, void *data)
{
#pragma unused(data)
	struct vfs_fsentry vfe;
	int	error;
	boolean_t	funnel_state;

#if 0
	/* instead of this just set breakpoint on kmod_start_or_stop */
	Debugger("smb module start");
#endif
	funnel_state = thread_funnel_set(kernel_flock, TRUE);

	smbfs_kmod_infop = ki;

	vfe.vfe_vfsops = &smbfs_vfsops;
	vfe.vfe_vopcnt = 1; /* We just have vnode operations for regular files and directories */
	vfe.vfe_opvdescs = smbfs_vnodeop_opv_desc_list;
	strncpy(vfe.vfe_fsname, smbfs_name, MFSNAMELEN);
	vfe.vfe_flags = VFS_TBLNOTYPENUM |
			VFS_TBL64BITREADY; /* 64bit mount & sysctl & ioctl */
	vfe.vfe_reserv[0] = 0;
	vfe.vfe_reserv[1] = 0;

	error = vfs_fsadd(&vfe, &smbfs_vfsconf);

	if (!error) {
		co_grp_attr = lck_grp_attr_alloc_init();
		lck_grp_attr_setstat(co_grp_attr);
		co_lck_group = lck_grp_alloc_init("smb-co", co_grp_attr);
		co_lck_attr = lck_attr_alloc_init();
		lck_attr_setdebug(co_lck_attr);

		vcst_grp_attr = lck_grp_attr_alloc_init();
		lck_grp_attr_setstat(vcst_grp_attr);
		vcst_lck_group = lck_grp_alloc_init("smb-vcst", vcst_grp_attr);
		vcst_lck_attr = lck_attr_alloc_init();
		lck_attr_setdebug(vcst_lck_attr);

		ssst_grp_attr = lck_grp_attr_alloc_init();
		lck_grp_attr_setstat(ssst_grp_attr);
		ssst_lck_group = lck_grp_alloc_init("smb-ssst", ssst_grp_attr);
		ssst_lck_attr = lck_attr_alloc_init();
		lck_attr_setdebug(ssst_lck_attr);

		iodflags_grp_attr = lck_grp_attr_alloc_init();
		lck_grp_attr_setstat(iodflags_grp_attr);
		iodflags_lck_group = lck_grp_alloc_init("smb-iodflags", iodflags_grp_attr);
		iodflags_lck_attr = lck_attr_alloc_init();
		lck_attr_setdebug(iodflags_lck_attr);

		iodrq_grp_attr = lck_grp_attr_alloc_init();
		lck_grp_attr_setstat(iodrq_grp_attr);
		iodrq_lck_group = lck_grp_alloc_init("smb-iodrq", iodrq_grp_attr);
		iodrq_lck_attr = lck_attr_alloc_init();
		lck_attr_setdebug(iodrq_lck_attr);

		iodev_grp_attr = lck_grp_attr_alloc_init();
		lck_grp_attr_setstat(iodev_grp_attr);
		iodev_lck_group = lck_grp_alloc_init("smb-iodev", iodev_grp_attr);
		iodev_lck_attr = lck_attr_alloc_init();
		lck_attr_setdebug(iodev_lck_attr);

		srs_grp_attr = lck_grp_attr_alloc_init();
		lck_grp_attr_setstat(srs_grp_attr);
		srs_lck_group = lck_grp_alloc_init("smb-srs", srs_grp_attr);
		srs_lck_attr = lck_attr_alloc_init();
		lck_attr_setdebug(srs_lck_attr);

		nbp_grp_attr = lck_grp_attr_alloc_init();
		lck_grp_attr_setstat(nbp_grp_attr);
		nbp_lck_group = lck_grp_alloc_init("smb-nbp", nbp_grp_attr);
		nbp_lck_attr = lck_attr_alloc_init();
		lck_attr_setdebug(nbp_lck_attr);

		dev_lck_grp_attr = lck_grp_attr_alloc_init();
		lck_grp_attr_setstat(dev_lck_grp_attr);
		dev_lck_grp = lck_grp_alloc_init("smb-dev", dev_lck_grp_attr);
		dev_lck_attr = lck_attr_alloc_init();
		lck_attr_setdebug(dev_lck_attr);
		dev_lck = lck_mtx_alloc_init(dev_lck_grp, dev_lck_attr);

		hash_lck_grp_attr = lck_grp_attr_alloc_init();
		lck_grp_attr_setstat(hash_lck_grp_attr);
		hash_lck_grp = lck_grp_alloc_init("smb-hash", hash_lck_grp_attr);
		hash_lck_attr = lck_attr_alloc_init();
		lck_attr_setdebug(hash_lck_attr);

		SEND_EVENT(iconv, MOD_LOAD);
		SEND_EVENT(iconv_ces, MOD_LOAD);
#ifdef XXX /* apparently unused */
		SEND_EVENT(iconv_xlat_?, MOD_LOAD); /* iconv_xlatmod_handler */
		SEND_EVENT(iconv_ces_table, MOD_LOAD);
#endif
		SEND_EVENT(iconv_xlat, MOD_LOAD);
		/* Bring up UTF-8 converter */
		SEND_EVENT(iconv_utf8, MOD_LOAD);
		iconv_add("utf8", "utf-8", "ucs-2");
		iconv_add("utf8", "ucs-2", "utf-8");
		/* Bring up default codepage converter */
		SEND_EVENT(iconv_codepage, MOD_LOAD);
		iconv_add("codepage", "utf-8", "cp437");
		iconv_add("codepage", "cp437", "utf-8");
		SEND_EVENT(dev_netsmb, MOD_LOAD);

		sysctl_register_oid(&sysctl__net_smb);
		sysctl_register_oid(&sysctl__net_smb_fs);
		sysctl_register_oid(&sysctl__net_smb_fs_iconv);
		sysctl_register_oid(&sysctl__net_smb_fs_iconv_add);
		sysctl_register_oid(&sysctl__net_smb_fs_iconv_cslist);
		sysctl_register_oid(&sysctl__net_smb_fs_iconv_drvlist);

		smbfs_install_sleep_wake_notifier();
	}
	(void) thread_funnel_set(kernel_flock, funnel_state);
	return (error ? KERN_FAILURE : KERN_SUCCESS);
}


__private_extern__ int
smbfs_module_stop(kmod_info_t *ki, void *data)
{
#pragma unused(ki)
#pragma unused(data)
	int error;
	boolean_t	funnel_state;

	funnel_state = thread_funnel_set(kernel_flock, TRUE);

	sysctl_unregister_oid(&sysctl__net_smb_fs_iconv_drvlist);
	sysctl_unregister_oid(&sysctl__net_smb_fs_iconv_cslist);
	sysctl_unregister_oid(&sysctl__net_smb_fs_iconv_add);
	sysctl_unregister_oid(&sysctl__net_smb_fs_iconv);
	sysctl_unregister_oid(&sysctl__net_smb_fs);
	sysctl_unregister_oid(&sysctl__net_smb);

	error = vfs_fsremove(smbfs_vfsconf);

	SEND_EVENT(dev_netsmb, MOD_UNLOAD);
	SEND_EVENT(iconv_xlat, MOD_UNLOAD);
	SEND_EVENT(iconv_utf8, MOD_UNLOAD);
#ifdef XXX /* apparently unused */
	SEND_EVENT(iconv_ces_table, MOD_UNLOAD);
	SEND_EVENT(iconv_xlat_?, MOD_UNLOAD); /* iconv_xlatmod_handler */
#endif
	SEND_EVENT(iconv_ces, MOD_UNLOAD);
	SEND_EVENT(iconv, MOD_UNLOAD);

	smbfs_remove_sleep_wake_notifier();

	lck_mtx_free(dev_lck, dev_lck_grp);

	(void) thread_funnel_set(kernel_flock, funnel_state);

	return (error ? KERN_FAILURE : KERN_SUCCESS);
}
