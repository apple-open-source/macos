/*
 * Copyright (c) 2000-2001, Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2007 Apple Inc. All rights reserved.
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

int smbfs_module_start(kmod_info_t *ki, void *data);
int smbfs_module_stop(kmod_info_t *ki, void *data);
static int smbfs_root(struct mount *, vnode_t *, vfs_context_t);
static int smbfs_init(struct vfsconf *vfsp);

__private_extern__ int smbfs_loglevel = 0;

static int smbfs_version = SMBFS_VERSION;
static int mount_cnt = 0;

lck_grp_attr_t *smbfs_group_attr;
lck_attr_t *smbfs_lock_attr;
lck_grp_t *smbfs_mutex_group;
lck_grp_t *smbfs_rwlock_group;

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

lck_grp_attr_t   * iconv_lck_grp_attr;
lck_grp_t  * iconv_lck_grp;
lck_attr_t * iconv_lck_attr;
lck_mtx_t * iconv_lck;

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

SYSCTL_DECL(_net_smb);
SYSCTL_NODE(_net_smb, OID_AUTO, fs, CTLFLAG_RW, 0, "SMB/CIFS file system");
SYSCTL_INT(_net_smb_fs, OID_AUTO, version, CTLFLAG_RD, &smbfs_version, 0, "");
SYSCTL_INT(_net_smb_fs, OID_AUTO, loglevel, CTLFLAG_RW,  &smbfs_loglevel, 0, "");

extern struct sysctl_oid sysctl__net_smb;
extern struct sysctl_oid sysctl__net_smb_fs_version;
extern struct sysctl_oid sysctl__net_smb_fs_loglevel;
extern struct sysctl_oid sysctl__net_smb_fs_iconv;
extern struct sysctl_oid sysctl__net_smb_fs_iconv_add;
extern struct sysctl_oid sysctl__net_smb_fs_iconv_cslist;
extern struct sysctl_oid sysctl__net_smb_fs_iconv_drvlist;
extern struct sysctl_oid sysctl__net_smb_fs_tcpsndbuf;
extern struct sysctl_oid sysctl__net_smb_fs_tcprcvbuf;

MALLOC_DEFINE(M_SMBFSHASH, "SMBFS hash", "SMBFS hash table");

static void smbfs_lock_init()
{
	hash_lck_attr = lck_attr_alloc_init();
	hash_lck_grp_attr = lck_grp_attr_alloc_init();
	hash_lck_grp = lck_grp_alloc_init("smb-hash", hash_lck_grp_attr);
	
	smbfs_lock_attr    = lck_attr_alloc_init();
	smbfs_group_attr   = lck_grp_attr_alloc_init();
	smbfs_mutex_group  = lck_grp_alloc_init("smb-mutex", smbfs_group_attr);
	smbfs_rwlock_group = lck_grp_alloc_init("smbfs-rwlock", smbfs_group_attr);
}

static void smbfs_lock_uninit()
{
	lck_grp_free(smbfs_mutex_group);
	lck_grp_free(smbfs_rwlock_group);
	lck_grp_attr_free(smbfs_group_attr);
	lck_attr_free(smbfs_lock_attr);

	lck_grp_free(hash_lck_grp);
	lck_grp_attr_free(hash_lck_grp_attr);
	lck_attr_free(hash_lck_attr);
}

static void smbnet_lock_init()
{
	co_lck_attr = lck_attr_alloc_init();
	co_grp_attr = lck_grp_attr_alloc_init();
	co_lck_group = lck_grp_alloc_init("smb-co", co_grp_attr);
	
	vcst_lck_attr = lck_attr_alloc_init();
	vcst_grp_attr = lck_grp_attr_alloc_init();
	vcst_lck_group = lck_grp_alloc_init("smb-vcst", vcst_grp_attr);
	
	ssst_lck_attr = lck_attr_alloc_init();
	ssst_grp_attr = lck_grp_attr_alloc_init();
	ssst_lck_group = lck_grp_alloc_init("smb-ssst", ssst_grp_attr);
	
	iodflags_lck_attr = lck_attr_alloc_init();
	iodflags_grp_attr = lck_grp_attr_alloc_init();
	iodflags_lck_group = lck_grp_alloc_init("smb-iodflags", iodflags_grp_attr);
	
	iodrq_lck_attr = lck_attr_alloc_init();
	iodrq_grp_attr = lck_grp_attr_alloc_init();
	iodrq_lck_group = lck_grp_alloc_init("smb-iodrq", iodrq_grp_attr);
	
	iodev_lck_attr = lck_attr_alloc_init();
	iodev_grp_attr = lck_grp_attr_alloc_init();
	iodev_lck_group = lck_grp_alloc_init("smb-iodev", iodev_grp_attr);
	
	srs_lck_attr = lck_attr_alloc_init();
	srs_grp_attr = lck_grp_attr_alloc_init();
	srs_lck_group = lck_grp_alloc_init("smb-srs", srs_grp_attr);
	
	nbp_lck_attr = lck_attr_alloc_init();
	nbp_grp_attr = lck_grp_attr_alloc_init();
	nbp_lck_group = lck_grp_alloc_init("smb-nbp", nbp_grp_attr);

	
	iconv_lck_attr = lck_attr_alloc_init();
	iconv_lck_grp_attr = lck_grp_attr_alloc_init();
	iconv_lck_grp = lck_grp_alloc_init("smb-dev", iconv_lck_grp_attr);
	iconv_lck = lck_mtx_alloc_init(iconv_lck_grp, iconv_lck_attr);
	
	dev_lck_attr = lck_attr_alloc_init();
	dev_lck_grp_attr = lck_grp_attr_alloc_init();
	dev_lck_grp = lck_grp_alloc_init("smb-dev", dev_lck_grp_attr);
	dev_lck = lck_mtx_alloc_init(dev_lck_grp, dev_lck_attr);
}

static void smbnet_lock_uninit()
{
	lck_grp_free(dev_lck_grp);
	lck_grp_attr_free(dev_lck_grp_attr);
	lck_attr_free(dev_lck_attr);
	
	lck_grp_free(iconv_lck_grp);
	lck_grp_attr_free(iconv_lck_grp_attr);
	lck_attr_free(iconv_lck_attr);
	
	lck_grp_free(nbp_lck_group);
	lck_grp_attr_free(nbp_grp_attr);
	lck_attr_free(nbp_lck_attr);
	
	lck_grp_free(srs_lck_group);
	lck_grp_attr_free(srs_grp_attr);
	lck_attr_free(srs_lck_attr);
	
	lck_grp_free(iodev_lck_group);
	lck_grp_attr_free(iodev_grp_attr);
	lck_attr_free(iodev_lck_attr);
	
	lck_grp_free(iodrq_lck_group);
	lck_grp_attr_free(iodrq_grp_attr);
	lck_attr_free(iodrq_lck_attr);
	
	lck_grp_free(iodflags_lck_group);
	lck_grp_attr_free(iodflags_grp_attr);
	lck_attr_free(iodflags_lck_attr);
	
	lck_grp_free(ssst_lck_group);
	lck_grp_attr_free(ssst_grp_attr);
	lck_attr_free(ssst_lck_attr);
	
	lck_grp_free(vcst_lck_group);
	lck_grp_attr_free(vcst_grp_attr);
	lck_attr_free(vcst_lck_attr);
	
	lck_grp_free(co_lck_group);
	lck_grp_attr_free(co_grp_attr);
	lck_attr_free(co_lck_attr);	
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
	struct smbnode *np;
	struct smb_share *ssp = smp->sm_share;
	int	error = 0;
	struct ntsecdesc	*w_secp = NULL;	/* Wire sec descriptor */
	struct ntsid *usidp, *gsidp;
	ntsid_t	kntsid;
	guid_t	guid;
	u_int8_t	ntauth[SIDAUTHSIZE] = { 0, 0, 0, 0, 0, 5 };
	uid_t	uid;
	u_int16_t	fid = 0;
	int seclen = 0;

	/*
	 * Does the server claim ACL support for this volume?
	 */
	if (!(ssp->ss_attributes & FILE_PERSISTENT_ACLS))
		return (-1);
		
	/* 
	 * We do not have a lock on the smbnode so get it here. This code needs
	 * to be re-written once we have a better way of testing for ACL support.
	 *
	 * Get a security descriptor from the share's root. 
	 */
	error = smbnode_lock(VTOSMB(smp->sm_rvp), SMBFS_EXCLUSIVE_LOCK);
	if (error)
		return (-1);
		
	np = VTOSMB(smp->sm_rvp);
	np->n_lastvop = smbfs_aclsflunksniff;
	error = smbfs_smb_tmpopen(np, STD_RIGHT_READ_CONTROL_ACCESS, scrp, &fid);
	if (error == 0) {
		error = smbfs_smb_getsec(ssp, fid, scrp, OWNER_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION, &w_secp, &seclen);
		/* If we get an error on close, not sure what we would do with it at this point */
		(void)smbfs_smb_tmpclose(np, fid, scrp);
	}
	/* If we cannot get the root vnodes security descriptor, then just get out. */
	if (error || (w_secp == NULL)) {
		SMBDEBUG("Failed getting the root vnode security descriptor! error = %d\n", error);
		goto err;
	}
	usidp = sdowner(w_secp);
	if (!usidp) 
		goto err;

	smb_sid_endianize(usidp);
	smb_sid2sid16(usidp, &kntsid);
	error = kauth_cred_ntsid2guid(&kntsid, &guid);
	if (error) {
		SMBDEBUG("kauth_cred_ntsid2guid error %d (owner)\n", error);
		goto err;
	}
	if (IS_MEMBERD_TEMPUUID(&guid)) {
		SMBWARNING("(fyi) user sid %s didnt map\n", smb_sid2str(usidp));
		goto err;
	}
	gsidp = sdgroup(w_secp);
	if (!gsidp)
		goto err;

	smb_sid_endianize(gsidp);
	smb_sid2sid16(gsidp, &kntsid);
	error = kauth_cred_ntsid2guid(&kntsid, &guid);
	if (error) {
		SMBDEBUG("kauth_cred_ntsid2guid error %d (group)\n", error);
		goto err;
	}
	if (IS_MEMBERD_TEMPUUID(&guid)) {
		SMBWARNING("group sid %s didnt map\n", smb_sid2str(gsidp));
		goto err;
	}

	/*
	 * Can our DS subsystem give us a SID for the mounting user?
	 */
	error = kauth_cred_uid2ntsid(smp->sm_args.uid, &kntsid);
	if (error) {
		/* kauth_cred_uid2ntsid will return ENOENT if the lookup failed, report any other errors */
		if (error != ENOENT)
			SMBDEBUG("sm_args.uid %d, error %d\n", smp->sm_args.uid, error);		
		goto err;
	}
	/*
	 * We accept SIDS of the form S-1-5-x-*, where x is not 32
	 * ("builtin" groups) and x > 20 (lower ones are "well-known")
	 * Arguably a check could/should be done on the final subauthority
	 * as those in the 500s are domain-specific well-knowns, while
	 * those around 1000 are real users.
	 */
	if ((kntsid.sid_kind != 1) || (bcmp(kntsid.sid_authority, ntauth, sizeof(ntauth))) ||
	    (kntsid.sid_authcount < 1) || (kntsid.sid_authorities[0] <= 20) || (kntsid.sid_authorities[0] == 32)) {
		SMBWARNING("sm_args.uid %d, sid %s\n", smp->sm_args.uid, smb_sid2str((struct ntsid *)&kntsid));
		goto err;
	}
	error = kauth_cred_ntsid2uid(&kntsid, &uid);
	if (error) {
		/* kauth_cred_ntsid2uid will return ENOENT if the lookup failed, report any other errors */
		if (error != ENOENT)
			SMBDEBUG("sid %s, error %d\n", smb_sid2str((struct ntsid *)&kntsid), error);
		goto err;
	}
	if (uid == smp->sm_args.uid)
		goto out;
	
	SMBWARNING("sm_args.uid %d, uid %d\n", smp->sm_args.uid, uid);
	
err:;
	/* We failed, but without an error return an error */
	if (!error)
		error = -1;
out:;
	if (w_secp)
		FREE(w_secp, M_TEMP);
	smbnode_unlock(VTOSMB(smp->sm_rvp));
	return (error);
}

static int
smbfs_mount(struct mount *mp, vnode_t devvp, user_addr_t data,
	    vfs_context_t vfsctx)
{
	struct smb_mount_args *args = NULL;		/* will hold data from mount request */
	struct smbmount *smp = NULL;
	struct smb_vc *vcp;
	struct smb_share *ssp = NULL;
	vnode_t vp;
	struct smb_cred scred;
	int error;

	if (data == USER_ADDR_NULL) {
		SMBDEBUG("missing data argument\n");
		return (EINVAL);
	}
	if (vfs_isupdate(mp)) {
		SMBERROR("MNT_UPDATE not supported!");
		return (ENOTSUP);
	}
	
	MALLOC(args, struct smb_mount_args *, sizeof(*args), M_SMBFSDATA, M_WAITOK);
	if (!args) {
		SMBDEBUG("Couldn't malloc the mount arguments!");
		return (ENOMEM);		
	}
	error = copyin(data, (caddr_t)args, sizeof(*args));
	if (error) {
		free(args, M_SMBFSDATA);		
		return (error);		
	}
	
#ifdef RADAR_5618109
	if (args->version != SMBFS_VERSION) {
		SMBERROR("mount version mismatch: kernel=%d, mount=%d\n", SMBFS_VERSION, args->version);
		free(args, M_SMBFSDATA);		
		return (EINVAL);
	}
#endif // RADAR_5618109
	
	/* Get the debug level as soon as possible. */
	smbfs_loglevel =  args->debug_level;
	error = smb_dev2share(args->dev, &ssp);
	if (error) {
		SMBDEBUG("invalid device handle %d (%d)\n", args->dev, error);
		free(args, M_SMBFSDATA);		
		return (error);
	}
	smb_scred_init(&scred, vfsctx);
	vcp = SSTOVC(ssp);
	smb_share_unlock(ssp);

	MALLOC(smp, struct smbmount*, sizeof(*smp), M_SMBFSDATA, M_WAITOK);
	if (smp == NULL) {
		error = ENOMEM;
		goto bad;
	}
	bzero(smp, sizeof(*smp));
	smp->sm_flags |= kInMountSMBFS;
	smp->sm_mp = mp;
	vfs_setfsprivate(mp, (void *)smp);
	ssp->ss_mount = smp;
	smp->sm_hash = hashinit(desiredvnodes, M_SMBFSHASH, &smp->sm_hashlen);
	if (smp->sm_hash == NULL)
		goto bad;
	smp->sm_hashlock = lck_mtx_alloc_init(hash_lck_grp, hash_lck_attr);
	lck_mtx_init(&smp->sm_statfslock, smbfs_mutex_group, smbfs_lock_attr);		
	lck_mtx_init(&smp->sm_renamelock, smbfs_mutex_group, smbfs_lock_attr);		

	smp->sm_share = ssp;
	smp->sm_rvp = NULL;
	/* Save any passed in arguments that we may need */
	smp->sm_args.altflags = args->altflags;
	smp->sm_args.uid = args->uid;
	smp->sm_args.gid = args->gid;
	smp->sm_args.file_mode = args->file_mode & ACCESSPERMS;
	smp->sm_args.dir_mode  = args->dir_mode & ACCESSPERMS;
	
	/*
	 * Ensure cached info is set to reasonable values.
	 *
	 * This call should be done from mount() in vfs layer.
	 * Not sure why each file system has to do it here, but
	 * we do so make an internal call to fill in the default
	 * values.
	 */
	(void)smbfs_smb_statfs(ssp, vfs_statfs(mp), &scred);
	
	/* Copy in the from name, used for reconnects and other things  */
	strlcpy(vfs_statfs(mp)->f_mntfromname, args->url_fromname, MAXPATHLEN);
	/* See if they sent use a starting path to use */
	if (args->path_len)
		smbfs_create_start_path(vcp, smp, args);
	
	/* Now get the mounted volumes unique id */
	smp->sm_args.unique_id_len = args->unique_id_len;
	MALLOC(smp->sm_args.unique_id, unsigned char *, smp->sm_args.unique_id_len, M_SMBFSDATA, M_WAITOK);
	if (smp->sm_args.unique_id)
		bcopy(args->unique_id, smp->sm_args.unique_id, smp->sm_args.unique_id_len);
	else 
		smp->sm_args.unique_id_len = 0;
	free(args, M_SMBFSDATA);	/* Done with the args free them */
	args = NULL;
	
	vfs_setauthopaque(mp);
	vfs_clearauthopaqueaccess(mp);
	vfs_getnewfsid(mp);
	error = smbfs_root(mp, &vp, vfsctx);
	if (error)
		goto bad;
	vfs_clearextendedsecurity(mp);
	smbfs_smb_qfsattr(ssp, &scred);
	if (smbfs_aclsflunksniff(smp, &scred)) {
		ssp->ss_attributes &= ~FILE_PERSISTENT_ACLS;
	} else {
		vfs_setextendedsecurity(mp);
		/* Tell the upper level its ok to cache access information */
		vfs_setauthcache_ttl(mp, SMB_ACL_MAXTIMO);
		SMBWARNING("vfs_authcache_ttl = %d\n", vfs_authcache_ttl(mp));
	}

	/* 
	 * This is a read-only volume, so change the mount flags so
	 * the finder will show it as a read-only volume. 
	 */ 
	if (ssp->ss_attributes & FILE_READ_ONLY_VOLUME)
		vfs_setflags(mp, MNT_RDONLY);

	/*
	 * Some day we would like to always support streams if the server supports
	 * it. Until then this is how we decide if streams is turn on or off.
	 * 
	 * 1. The server must supports streams or streams is always off.
	 * 2. The user or MCX setting says to turn on streams support.
	 *	a. If no file exist that says turn it off then leave streams on.
	 * 3. The Server supports streams and its a UNIX server (Leopard SAMBA).
	 *	a. If no file exist that says turn it off then leave streams on.
	 * 4. There is no user or MCX setting saying streams support is on. 
	 *	a. If file exist that says turn it on then leave streams on.
	 *
	 * .com.apple.smb.streams.on - If exist on top level of share means turn on streams.
	 * .com.apple.smb.streams.off - If exist on top level of share means turn off streams.
	 *
	 */
	if (ssp->ss_attributes & FILE_NAMED_STREAMS) {
		struct smbfattr fap;
		
		if ((smp->sm_args.altflags & SMBFS_MNT_STREAMS_ON) || (UNIX_SERVER(SSTOVC(ssp)))) {
			if (smbfs_smb_query_info(VTOSMB(vp), SMB_STREAMS_OFF, strlen(SMB_STREAMS_OFF), &fap, &scred) == 0)
				ssp->ss_attributes &= ~FILE_NAMED_STREAMS;
		}
		else if (smbfs_smb_query_info(VTOSMB(vp), SMB_STREAMS_ON, strlen(SMB_STREAMS_ON), &fap, &scred) != 0)
			ssp->ss_attributes &= ~FILE_NAMED_STREAMS;
		/* We would like to know if this is a SFM Volume, skip this check for unix servers. */
		if ((! UNIX_SERVER(SSTOVC(ssp))) && (ssp->ss_attributes & FILE_NAMED_STREAMS)) {
			if (smbfs_smb_qstreaminfo(VTOSMB(vp), &scred, NULL, NULL, SFM_DESKTOP_NAME, NULL) == 0)
				ssp->ss_flags |= SMBS_SFM_VOLUME;
		}
	}

	/* smbfs_root did a vnode_get and a vnode_ref, so keep the ref but release the get */
	vnode_put(vp);
	/* 
	 * The only way we can get an error at this point is if smbfs_smb_qfsattr
	 * returns an error. Radar 3413772 makes smbfs_smb_qfsattr a void 
	 * routine. So we will no longer have a error case at this point.
	 */
	mount_cnt++;
	smp->sm_flags &= ~kInMountSMBFS;
	return (0);
bad:
	if (ssp) {
		ssp->ss_mount = NULL;	/* ssp->ss_mount is smp and we will free it below  */ 
		smb_share_put(ssp, &scred);
	}
	if (smp) {
		vfs_setfsprivate(mp, (void *)0);
		if (smp->sm_hash)
			free(smp->sm_hash, M_SMBFSHASH);
		lck_mtx_free(smp->sm_hashlock, hash_lck_grp);
		lck_mtx_destroy(&smp->sm_statfslock, smbfs_mutex_group);
		lck_mtx_destroy(&smp->sm_renamelock, smbfs_mutex_group);
		if (smp->sm_args.path)
			free(smp->sm_args.path, M_SMBFSDATA);
		if (smp->sm_args.unique_id)
			free(smp->sm_args.unique_id, M_SMBFSDATA);
		if (smp->ntwrk_gids)
		    free(smp->ntwrk_gids, M_TEMP);
		free(smp, M_SMBFSDATA);
	}
	if (args)	/* Done with the args free them */
		free(args, M_SMBFSDATA);
		
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
		SMBDEBUG("smbfs_unmount: usecnt\n");
		vnode_put(vp);
		return (EBUSY);
	}
	vnode_rele(vp);	/* to drop ref taken by smbfs_mount */
	vnode_put(vp);	/* to drop ref taken by VFS_ROOT above */
	smp->sm_rvp = NULL;	/* We no longer have a reference so clear it out */

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
	lck_mtx_destroy(&smp->sm_statfslock, smbfs_mutex_group);
	lck_mtx_destroy(&smp->sm_renamelock, smbfs_mutex_group);
	if (smp->sm_args.path)
		free(smp->sm_args.path, M_SMBFSDATA);
	if (smp->sm_args.unique_id)
		free(smp->sm_args.unique_id, M_SMBFSDATA);
	if (smp->ntwrk_gids)
	    free(smp->ntwrk_gids, M_TEMP);
	free(smp, M_SMBFSDATA);
	vfs_clearflags(mp, MNT_LOCAL);
	/* We should never have an error here, but just in case lets check for it */
	if (!error) 
		mount_cnt--;
	return (error);
}

/* 
 * Return locked root vnode of a filesystem
 */
static int smbfs_root(struct mount *mp, vnode_t *vpp, vfs_context_t vfsctx)
{
	struct smbmount *smp = VFSTOSMBFS(mp);
	vnode_t vp;
	struct smbfattr fattr;
	int error;

	if (smp == NULL) {
		SMBERROR("smp == NULL (bug in umount)\n");
		return (EINVAL);
	}
	
	if (smp->sm_rvp) {
		/* just get the saved root vnode as its much faster */
		*vpp = smp->sm_rvp;
		return (vnode_get(*vpp));
	}
	
	/* Fill in the deafult values that we already know about the root vnode */
	bzero(&fattr, sizeof(fattr));
	nanotime(&fattr.fa_reqtime);
	fattr.fa_attr = SMB_FA_DIR;
	fattr.fa_vtype = VDIR;
	fattr.fa_ino = 2;
	/*
	 * First time to get the root vnode, smbfs_nget will create it and check with the network
	 * to make sure all is well with the root node. Could get an error if the device is not 
	 * ready are we have no access.
	 */
	error = smbfs_nget(mp, NULL, "TheRooT", 7, &fattr, &vp, 0, vfsctx);
	if (error)
		return (error);
	/* 
	 * Since root vnode has an exclusive lock, I know only one process can be 
	 * here at this time.  Check once more while I still have the lock that 
	 * sm_rvp is still NULL before taking a ref and saving it. 
	 */
	if (smp->sm_rvp == NULL) {
		smp->sm_rvp = vp;			/* this will be released in the unmount code */
		smbnode_unlock(VTOSMB(vp));		/* Release the smbnode lock */
		/* now save a ref to this vnode so that we can quickly retrieve in subsequent calls */
		error = vnode_ref(vp);	/* take a ref so it wont go away */
		/* It would be very rare for vnode_ref to fail, but be paranoid anyways */
		if (error) {
			SMBERROR("vnode_ref on rootvp failed error %d\n", error);
			smp->sm_rvp = NULL;
			vnode_put(vp);
			return(error);
		}
	} else {
		/* 
		 * Must have had two or more processes running at same time, other process 
		 * saved the root vnode, so just unlock this one and return 
		 */
		smbnode_unlock(VTOSMB(vp));		/* Release the smbnode lock */		
	}

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
	static int32_t done = 0;

	if (done == 1)
		return (0);
	done = 1;
	smbfs_lock_init();

	return 0;
}


/*
 * smbfs_statfs call
 */
static int smbfs_vfs_getattr(struct mount *mp, struct vfs_attr *fsap, vfs_context_t vfsctx)
{
	struct smbmount *smp = VFSTOSMBFS(mp);
	struct smb_share *ssp = smp->sm_share;
	struct smb_vc *vcp = SSTOVC(ssp);
	struct vfsstatfs cachedstatfs;
	struct smb_cred scred;
	struct timespec ts;
	int error = 0;

	if ((smp->sm_rvp == NULL) || (VTOSMB(smp->sm_rvp) == NULL))
		return (EINVAL);

	/*
	 * Some day we should use NT Notifications and a background thread for updating our statfs 
	 * cache. For now we just borrow one of the threads coming in to update our cache.
	 */
	lck_mtx_lock(&smp->sm_statfslock);
	cachedstatfs = smp->sm_statfsbuf;
	if (smp->sm_status & SM_STATUS_STATFS)
		lck_mtx_unlock(&smp->sm_statfslock);
	else {
		smp->sm_status |= SM_STATUS_STATFS;
		lck_mtx_unlock(&smp->sm_statfslock);
		/* We always check the first time otherwise only if they ask and our cache is stale. */
		if ((smp->sm_statfstime == 0) ||
			((ts.tv_sec - smp->sm_statfstime > SM_MAX_STATFSTIME) &&
			(VFSATTR_IS_ACTIVE(fsap, f_bsize) || VFSATTR_IS_ACTIVE(fsap, f_blocks) ||
			 VFSATTR_IS_ACTIVE(fsap, f_bfree) || VFSATTR_IS_ACTIVE(fsap, f_bavail) ||
			 VFSATTR_IS_ACTIVE(fsap, f_files) || VFSATTR_IS_ACTIVE(fsap, f_ffree)))) {
			/* update cached from-the-server data */
			smb_scred_init(&scred, vfsctx);
			error = smbfs_smb_statfs(ssp, &cachedstatfs, &scred);
			if (error == 0) {
				nanotime(&ts);
				smp->sm_statfstime = ts.tv_sec;
				lck_mtx_lock(&smp->sm_statfslock);
				smp->sm_statfsbuf = cachedstatfs;
				lck_mtx_unlock(&smp->sm_statfslock);
			} else 
				error = 0;
		}
		
		lck_mtx_lock(&smp->sm_statfslock);
		smp->sm_status &= ~SM_STATUS_STATFS;
		lck_mtx_unlock(&smp->sm_statfslock);
	}

	/*
	 * Not sure what to do about these items, seems we get call for them and
	 * if they are not filled in an error gets return. We tell them in the 
	 * capibilities that we do not support these items. Notice AFP fills them
	 * in and the values they using are the same as below. The AFP code does have
	 * these items but they never get updated and always end with these same
	 * values.
	 */
	VFSATTR_RETURN (fsap, f_objcount, (uint64_t) 0 + (uint64_t)0);
	/* We do not support setting filecount. */
	VFSATTR_RETURN (fsap, f_filecount, (uint64_t) 0);
	/* We do not support setting dircount. */
	VFSATTR_RETURN (fsap, f_dircount, (uint64_t) 0);
	/* We do not support setting maxobjcount. */
	VFSATTR_RETURN (fsap, f_maxobjcount, (uint64_t) 0xFFFFFFFF);
	
	/* copy results from cached statfs */
	VFSATTR_RETURN(fsap, f_bsize, cachedstatfs.f_bsize);
	VFSATTR_RETURN(fsap, f_iosize, cachedstatfs.f_iosize);
	VFSATTR_RETURN(fsap, f_blocks, cachedstatfs.f_blocks);
	VFSATTR_RETURN(fsap, f_bfree, cachedstatfs.f_bfree);
	VFSATTR_RETURN(fsap, f_bavail, cachedstatfs.f_bavail);
	VFSATTR_RETURN (fsap, f_bused, cachedstatfs.f_blocks - cachedstatfs.f_bavail);
	VFSATTR_RETURN(fsap, f_files, cachedstatfs.f_files);
	VFSATTR_RETURN(fsap, f_ffree, cachedstatfs.f_ffree);
	
	fsap->f_fsid.val[0] = vfs_statfs(mp)->f_fsid.val[0];
	fsap->f_fsid.val[1] = vfs_typenum(mp);
	VFSATTR_SET_SUPPORTED(fsap, f_fsid);
	
	/* The VFS layer handles f_owner. */

	/* 
	 * NOTE:  the valid field indicates whether your VFS knows whether a capability is supported or not.
	 * So, if you know FOR SURE that a capability is support or not, then set that bit in the valid part.  
	 * Then, in the capabilities field, you either set it if supported or leave it clear if not supported 
	 */
	if (VFSATTR_IS_ACTIVE(fsap, f_capabilities)) {
		vol_capabilities_attr_t *cap = &fsap->f_capabilities;
		
		cap->capabilities[VOL_CAPABILITIES_FORMAT] = 
			VOL_CAP_FMT_SYMBOLICLINKS | 
			VOL_CAP_FMT_FAST_STATFS |
			VOL_CAP_FMT_OPENDENYMODES |
			VOL_CAP_FMT_HIDDEN_FILES |
			0;
		
		/* Only say we support large files if the server supports it */
		if (vcp->vc_sopt.sv_caps & SMB_CAP_LARGE_FILES)
			cap->capabilities[VOL_CAPABILITIES_FORMAT] |= VOL_CAP_FMT_2TB_FILESIZE;
			
		/* Must be FAT so don't trust the modify times */
		if (ssp->ss_fstype == SMB_FS_FAT)
			cap->capabilities[VOL_CAPABILITIES_FORMAT] |= VOL_CAP_FMT_NO_ROOT_TIMES;
			
		if (ssp->ss_attributes & FILE_CASE_PRESERVED_NAMES)
			cap->capabilities[VOL_CAPABILITIES_FORMAT] |= VOL_CAP_FMT_CASE_PRESERVING;
		
		if (ssp->ss_attributes & FILE_SUPPORTS_SPARSE_FILES)
			cap->capabilities[VOL_CAPABILITIES_FORMAT] |= VOL_CAP_FMT_SPARSE_FILES;

		cap->capabilities[VOL_CAPABILITIES_INTERFACES] = 
			VOL_CAP_INT_ATTRLIST | 
			VOL_CAP_INT_FLOCK |
			VOL_CAP_INT_MANLOCK |
			0;
		
		if (vcp->vc_sopt.sv_unix_caps & CIFS_UNIX_FCNTL_LOCKS_CAP)
			cap->capabilities[VOL_CAPABILITIES_INTERFACES] |= VOL_CAP_INT_ADVLOCK;
			
		if (ssp->ss_attributes & FILE_NAMED_STREAMS)
			cap->capabilities[VOL_CAPABILITIES_INTERFACES] |= VOL_CAP_INT_NAMEDSTREAMS | VOL_CAP_INT_EXTENDED_ATTR;			
		
		cap->capabilities[VOL_CAPABILITIES_RESERVED1] = 0;
		cap->capabilities[VOL_CAPABILITIES_RESERVED2] = 0;
		/*
		 * This SMB file system is case insensitive and case preserving, but
		 * servers vary, depending on the underlying volume.  In pathconf
		 * we have to give a yes or no answer. We need to return a consistent
		 * answer in both cases. We do not know the real answer for case 
		 * sensitive, but lets default to what 90% of the servers have set. 
		 * Also remeber this fixes Radar 4057391 and 3530751. 
		 */
		cap->valid[VOL_CAPABILITIES_FORMAT] =
			VOL_CAP_FMT_PERSISTENTOBJECTIDS |
			VOL_CAP_FMT_NO_ROOT_TIMES |
			VOL_CAP_FMT_SYMBOLICLINKS |
			VOL_CAP_FMT_HARDLINKS |
			VOL_CAP_FMT_JOURNAL |
			VOL_CAP_FMT_JOURNAL_ACTIVE |
			VOL_CAP_FMT_SPARSE_FILES |
			VOL_CAP_FMT_ZERO_RUNS |
			VOL_CAP_FMT_2TB_FILESIZE |
			VOL_CAP_FMT_CASE_PRESERVING |
			VOL_CAP_FMT_CASE_SENSITIVE |
			VOL_CAP_FMT_FAST_STATFS |
			VOL_CAP_FMT_OPENDENYMODES |
			VOL_CAP_FMT_HIDDEN_FILES |
			0;
					

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
			VOL_CAP_INT_FLOCK |
			VOL_CAP_INT_MANLOCK |
			VOL_CAP_INT_NAMEDSTREAMS |
			VOL_CAP_INT_EXTENDED_ATTR |
			0;
		
		cap->valid[VOL_CAPABILITIES_RESERVED1] = 0;
		cap->valid[VOL_CAPABILITIES_RESERVED2] = 0;
		VFSATTR_SET_SUPPORTED(fsap, f_capabilities);
	}
	
	/* 
	 * NOTE:  the valid field indicates whether your VFS knows whether a attribute is supported or not.
	 * So, if you know FOR SURE that a capability is support or not, then set that bit in the valid part.  
	 */
	if (VFSATTR_IS_ACTIVE(fsap, f_attributes)) {
		fsap->f_attributes.validattr.commonattr =
												ATTR_CMN_NAME	|
												ATTR_CMN_DEVID	|
												ATTR_CMN_FSID	|
												ATTR_CMN_OBJTYPE |
												ATTR_CMN_OBJTAG	|
												ATTR_CMN_OBJID	|
												/* ATTR_CMN_OBJPERMANENTID | */
												ATTR_CMN_PAROBJID |
												/* ATTR_CMN_SCRIPT | */
												ATTR_CMN_CRTIME |
												ATTR_CMN_MODTIME |
												ATTR_CMN_CHGTIME |
												ATTR_CMN_ACCTIME |
												/* ATTR_CMN_BKUPTIME | */
												/* Just not sure about the following: */
												ATTR_CMN_FNDRINFO |
												ATTR_CMN_OWNERID |
												ATTR_CMN_GRPID	|
												ATTR_CMN_ACCESSMASK |
												ATTR_CMN_FLAGS	|
												ATTR_CMN_USERACCESS |
												ATTR_CMN_EXTENDED_SECURITY |
												ATTR_CMN_UUID |
												ATTR_CMN_GRPUUID |
												0;
		fsap->f_attributes.validattr.volattr =
												ATTR_VOL_FSTYPE	|
												/* ATTR_VOL_SIGNATURE */
												ATTR_VOL_SIZE	|
												ATTR_VOL_SPACEFREE |
												ATTR_VOL_SPACEAVAIL |
												ATTR_VOL_MINALLOCATION |
												ATTR_VOL_ALLOCATIONCLUMP |
												ATTR_VOL_IOBLOCKSIZE |
												/* ATTR_VOL_OBJCOUNT */
												/* ATTR_VOL_FILECOUNT */
												/* ATTR_VOL_DIRCOUNT */
												/* ATTR_VOL_MAXOBJCOUNT */
												ATTR_VOL_MOUNTPOINT |
												ATTR_VOL_NAME	|
												ATTR_VOL_MOUNTFLAGS |
												ATTR_VOL_MOUNTEDDEVICE |
												/* ATTR_VOL_ENCODINGSUSED */
												ATTR_VOL_CAPABILITIES |
												ATTR_VOL_ATTRIBUTES |
												0;
		fsap->f_attributes.validattr.dirattr =
												ATTR_DIR_LINKCOUNT |
												ATTR_DIR_MOUNTSTATUS |
												0;
		fsap->f_attributes.validattr.fileattr =
												ATTR_FILE_LINKCOUNT |
												ATTR_FILE_TOTALSIZE |
												ATTR_FILE_ALLOCSIZE |
												/* ATTR_FILE_IOBLOCKSIZE */
												ATTR_FILE_DEVTYPE |
												/* ATTR_FILE_FORKCOUNT */
												/* ATTR_FILE_FORKLIST */
												ATTR_FILE_DATALENGTH |
												ATTR_FILE_DATAALLOCSIZE |
												ATTR_FILE_RSRCLENGTH |
												ATTR_FILE_RSRCALLOCSIZE |
												0;
		fsap->f_attributes.validattr.forkattr = 0;
		
		fsap->f_attributes.nativeattr.commonattr =
												ATTR_CMN_NAME	|
												ATTR_CMN_DEVID	|
												ATTR_CMN_FSID	|
												ATTR_CMN_OBJTYPE |
												ATTR_CMN_OBJTAG	|
												ATTR_CMN_OBJID	|
												/* ATTR_CMN_OBJPERMANENTID | */
												ATTR_CMN_PAROBJID |
												/* ATTR_CMN_SCRIPT | */
												ATTR_CMN_CRTIME |
												ATTR_CMN_MODTIME |
												ATTR_CMN_ACCTIME |
												/* ATTR_CMN_BKUPTIME | */
												/* ATTR_CMN_OWNERID | */	/* Supported but not native */
												/* ATTR_CMN_GRPID	| */	/* Supported but not native */
												/* ATTR_CMN_ACCESSMASK | */	/* Supported but not native */
												ATTR_CMN_FLAGS	|
												/* ATTR_CMN_USERACCESS | */	/* Supported but not native */
												0;
			/* FAT does not support change time */
		if (ssp->ss_fstype != SMB_FS_FAT)
			fsap->f_attributes.nativeattr.commonattr |= ATTR_CMN_CHGTIME;
			/* Streams knows about Finder Info */
		/*
		 * Once we added streams support we should add this code. Radar 4004371
		 */
		 if (ssp->ss_attributes & FILE_NAMED_STREAMS)
			fsap->f_attributes.nativeattr.commonattr |= ATTR_CMN_FNDRINFO;

		if (ssp->ss_attributes & FILE_PERSISTENT_ACLS)
			fsap->f_attributes.nativeattr.commonattr |= ATTR_CMN_EXTENDED_SECURITY | ATTR_CMN_UUID | ATTR_CMN_GRPUUID;
			
			
		fsap->f_attributes.nativeattr.volattr =
												ATTR_VOL_FSTYPE	|
												/* ATTR_VOL_SIGNATURE */
												ATTR_VOL_SIZE	|
												ATTR_VOL_SPACEFREE |
												ATTR_VOL_SPACEAVAIL |
												ATTR_VOL_MINALLOCATION |
												ATTR_VOL_ALLOCATIONCLUMP |
												ATTR_VOL_IOBLOCKSIZE |
												/* ATTR_VOL_OBJCOUNT */
												/* ATTR_VOL_FILECOUNT */
												/* ATTR_VOL_DIRCOUNT */
												/* ATTR_VOL_MAXOBJCOUNT */
												ATTR_VOL_MOUNTPOINT |
												ATTR_VOL_NAME	|
												ATTR_VOL_MOUNTFLAGS |
												ATTR_VOL_MOUNTEDDEVICE |
												/* ATTR_VOL_ENCODINGSUSED */
												ATTR_VOL_CAPABILITIES |
												ATTR_VOL_ATTRIBUTES |
												0;
		fsap->f_attributes.nativeattr.dirattr = 0;
		fsap->f_attributes.nativeattr.fileattr =
												/* ATTR_FILE_LINKCOUNT | */	/* Supported but not native */
												/* ATTR_FILE_IOBLOCKSIZE */
												ATTR_FILE_DEVTYPE |
												/* ATTR_FILE_FORKCOUNT */
												/* ATTR_FILE_FORKLIST */
												ATTR_FILE_DATALENGTH |
												ATTR_FILE_DATAALLOCSIZE |
												0;
		/*
		 * Once we added streams support we should add this code. Radar 2899967
		 */
		if (ssp->ss_attributes & FILE_NAMED_STREAMS)
			fsap->f_attributes.nativeattr.fileattr |= ATTR_FILE_TOTALSIZE | 
													ATTR_FILE_ALLOCSIZE |
													ATTR_FILE_RSRCLENGTH | 
													ATTR_FILE_RSRCALLOCSIZE;

		fsap->f_attributes.nativeattr.forkattr = 0;
		VFSATTR_SET_SUPPORTED(fsap, f_attributes);
	}
	/* 
	 * Our filesystem doesn't support volume dates. We could do this Mac to
	 * Mac if we implemented the CIFS Macintosh Extensions. Does Carbon just
	 * use the root vnode times? From my testing we never get ask for the
	 * root volume times. 
	 *
	if (VFSATTR_IS_ACTIVE(fsap, f_create_time))
		SMBWARNING("Request for Volume Create Time!\n");
	if (VFSATTR_IS_ACTIVE(fsap, f_modify_time))
		SMBWARNING("Request for Volume Modify Time!\n");
	if (VFSATTR_IS_ACTIVE(fsap, f_access_time))
		SMBWARNING("Request for Volume ACCESS Time!\n");
	if (VFSATTR_IS_ACTIVE(fsap, f_backup_time))
		SMBWARNING("Request for Volume Backup Time!\n");
	 */
	 
	 /* 
	  * Could be one of the following:
	  *		SMB_FS_FAT, SMB_FS_CDFS, SMB_FS_UDF, 
	  *		SMB_FS_NTFS_UNKNOWN, SMB_FS_NTFS, SMB_FS_NTFS_UNIX,
	  *		SMB_FS_MAC_OS_X 
	  */
	VFSATTR_RETURN(fsap, f_fssubtype, ssp->ss_fstype);
		
	/*
	 * ref 3984574.  Returning null here keeps vfs from returning
	 * f_mntonname, and causes CarbonCore (File Mgr) to use the
	 * f_mntfromname, as it did (& still does) when an error is returned.
	 */
	if (VFSATTR_IS_ACTIVE(fsap, f_vol_name) && fsap->f_vol_name) {
		/*
		 * They could be trying to mount a share call "/" in that case our from name
		 * will have it escaped out. So look at the to name and see if it has the 
		 * percent escape name, if so return to the finder that our name is "/".
		 */
		if (strncmp(vfs_statfs(mp)->f_mntonname, SMBFS_SLASH_TONAME, strlen(vfs_statfs(mp)->f_mntonname)+1) == 0) {
			fsap->f_vol_name[0] = '/';
			fsap->f_vol_name[1] = '\0';			
		} else
			*fsap->f_vol_name = '\0';
		VFSATTR_SET_SUPPORTED(fsap, f_vol_name);
	}
	
	/* Let the vfs layer handle f_signature */
	
	/* 
	 * What about f_carbon_fsid, not sure HFS sets it to 0. Need to check with the 
	 * File Manager group and see if we need to add a value to this item.
	 *
	 * This is the carbon filesystemID?
	 *
	 * VFSATTR_RETURN (fsap, f_carbon_fsid, 0);
	 */
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
	struct smbnode *np = NULL;
	struct smb_share *ssp;

	cargs = (struct smbfs_sync_cargs *)args;

	if (smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK) != 0) {
		return (VNODE_RETURNED);
	}

	np = VTOSMB(vp);
	np->n_lastvop = smbfs_sync_callback;

	ssp = np->n_mount->sm_share;
	/* 
	 * Must have gone into reconnect mode while interating the vnodes. Nothing for
	 * us to do until reconnect is done. Just get out and wait for the next time.
	 */
	if (ssp->ss_flags & SMBS_RECONNECTING) {
		smbnode_unlock(np);
		return (VNODE_RETURNED);		
	}

	/* 
	 * We need to clear the ACL cache. We only want to hold on to it for a very short period, so
	 * any chance we get remove the cache. ACL cache data can get very large so only hold on to
	 * it for a short period of time. Don't clear negative acl cache it doesn't cost much, so its ok to
	 * hold on to for longer periods of time.
	 */
	if ((np->acl_error == 0) && (!vnode_isnamedstream(vp)))
		smbfs_clear_acl_cache(np);

	if (!vnode_isdir(vp)) {
		/* See if the file needs to be reopened. Ignore the error if being revoke it will get caught below */
		(void)smbfs_smb_reopen_file(np, cargs->vfsctx);

		lck_mtx_lock(&np->f_openStateLock);
		if (np->f_openState == kNeedRevoke) {
			lck_mtx_unlock(&np->f_openStateLock);
			SMBWARNING("revoking %s\n", np->n_name);
			smbnode_unlock(np);
			vn_revoke(vp, REVOKEALL, cargs->vfsctx);
			return (VNODE_RETURNED);
		}
		lck_mtx_unlock(&np->f_openStateLock);
		
	}
	
	if (vnode_hasdirtyblks(vp)) {
		error = smbfs_fsync(vp, cargs->waitfor, 0, cargs->vfsctx);
		if (error)
			cargs->error = error;
	}
		
	smbnode_unlock(np);
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
	struct smb_share *ssp = VFSTOSMBFS(mp)->sm_share;

	/* 
	 * No reason to interate the vnodes if we are in reconnect mode since there is nothing
	 * we can do with them. Just get out and wait for the next time.
	 *
	 * XXX If we add data caching support in the future we will need to change this code,
	 * but for now we are safe just skipping this call during reconnect.
	 */
	if (ssp->ss_flags & SMBS_RECONNECTING)
		return 0;
	
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

PRIVSYM int smbfs_module_start(kmod_info_t *ki, void *data)
{
#pragma unused(data)
	struct vfs_fsentry vfe;
	int	error;

	smbfs_kmod_infop = ki;

	vfe.vfe_vfsops = &smbfs_vfsops;
	vfe.vfe_vopcnt = 1; /* We just have vnode operations for regular files and directories */
	vfe.vfe_opvdescs = smbfs_vnodeop_opv_desc_list;
	strlcpy(vfe.vfe_fsname, smbfs_name, sizeof(vfe.vfe_fsname));
	vfe.vfe_flags = VFS_TBLTHREADSAFE | VFS_TBLFSNODELOCK | VFS_TBLNOTYPENUM | VFS_TBL64BITREADY;  /* fine grain locking & 64bit mount & sysctl & ioctl */
	vfe.vfe_reserv[0] = 0;
	vfe.vfe_reserv[1] = 0;

	error = vfs_fsadd(&vfe, &smbfs_vfsconf);
	if (error)
		goto out;

	smbnet_lock_init();	/* Initialize the network locks */

	SEND_EVENT(iconv, MOD_LOAD);
	SEND_EVENT(iconv_ces, MOD_LOAD);

	SEND_EVENT(iconv_xlat, MOD_LOAD);
	/* Bring up UTF-8 converter */
	SEND_EVENT(iconv_utf8, MOD_LOAD);
	iconv_add("utf8", "utf-8", "ucs-2");
	iconv_add("utf8", "ucs-2", "utf-8");
	/* Bring up default codepage converter */
	SEND_EVENT(iconv_codepage, MOD_LOAD);
	iconv_add("codepage", "utf-8", "cp437");
	iconv_add("codepage", "cp437", "utf-8");
	
	/* This just calls nsmb_dev_load */
	SEND_EVENT(dev_netsmb, MOD_LOAD);

	sysctl_register_oid(&sysctl__net_smb);
	sysctl_register_oid(&sysctl__net_smb_fs);
	
	sysctl_register_oid(&sysctl__net_smb_fs_version);
	sysctl_register_oid(&sysctl__net_smb_fs_loglevel);
	
	sysctl_register_oid(&sysctl__net_smb_fs_iconv);
	sysctl_register_oid(&sysctl__net_smb_fs_iconv_add);
	sysctl_register_oid(&sysctl__net_smb_fs_iconv_cslist);
	sysctl_register_oid(&sysctl__net_smb_fs_iconv_drvlist);
	
	sysctl_register_oid(&sysctl__net_smb_fs_tcpsndbuf);
	sysctl_register_oid(&sysctl__net_smb_fs_tcprcvbuf);
	
	smbfs_install_sleep_wake_notifier();

out:
	return (error ? KERN_FAILURE : KERN_SUCCESS);
}


PRIVSYM int smbfs_module_stop(kmod_info_t *ki, void *data)
{
#pragma unused(ki)
#pragma unused(data)
	int error;

	if (mount_cnt)	/* We have mounted volumes. */
		return KERN_NO_ACCESS;
	
	error = vfs_fsremove(smbfs_vfsconf);
	if (error)
		goto out;

	sysctl_unregister_oid(&sysctl__net_smb_fs_tcpsndbuf);
	sysctl_unregister_oid(&sysctl__net_smb_fs_tcprcvbuf);

	sysctl_unregister_oid(&sysctl__net_smb_fs_iconv_drvlist);
	sysctl_unregister_oid(&sysctl__net_smb_fs_iconv_cslist);
	sysctl_unregister_oid(&sysctl__net_smb_fs_iconv_add);
	sysctl_unregister_oid(&sysctl__net_smb_fs_iconv);
	
	sysctl_unregister_oid(&sysctl__net_smb_fs_version);
	sysctl_unregister_oid(&sysctl__net_smb_fs_loglevel);
	
	sysctl_unregister_oid(&sysctl__net_smb_fs);
	sysctl_unregister_oid(&sysctl__net_smb);

	/* This just calls nsmb_dev_load */
	SEND_EVENT(dev_netsmb, MOD_UNLOAD);

	SEND_EVENT(iconv_xlat, MOD_UNLOAD);
	SEND_EVENT(iconv_utf8, MOD_UNLOAD);
	SEND_EVENT(iconv_ces, MOD_UNLOAD);
	SEND_EVENT(iconv, MOD_UNLOAD);

	smbfs_remove_sleep_wake_notifier();

	lck_mtx_free(iconv_lck, iconv_lck_grp);
	lck_mtx_free(dev_lck, dev_lck_grp);
	smbfs_lock_uninit();	/* Free up the file system locks */
	smbnet_lock_uninit();	/* Free up the network locks */
	
out:	
	return (error ? KERN_FAILURE : KERN_SUCCESS);
}
