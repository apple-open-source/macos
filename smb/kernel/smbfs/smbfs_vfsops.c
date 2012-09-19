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
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <libkern/OSAtomic.h>

#include <sys/kauth.h>

#include <sys/syslog.h>
#include <sys/smb_apple.h>
#include <sys/mchain.h>

#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_dev.h>
#include <netsmb/smb_sleephandler.h>

#include <smbfs/smbfs.h>
#include <smbfs/smbfs_node.h>
#include <smbfs/smbfs_subr.h>
#include <smbclient/smbclient_internal.h>
#include "smbfs_security.h"
#include <Triggers/triggers.h>

#include <sys/buf.h>

int smbfs_module_start(kmod_info_t *ki, void *data);
int smbfs_module_stop(kmod_info_t *ki, void *data);
static int smbfs_root(struct mount *, vnode_t *, vfs_context_t);

#ifdef SMB_DEBUG
__private_extern__ int smbfs_loglevel = SMB_LOW_LOG_LEVEL;
#else // SMB_DEBUG
__private_extern__ int smbfs_loglevel = SMB_NO_LOG_LEVEL;
#endif // SMB_DEBUG

__private_extern__ uint32_t smbfs_deadtimer = DEAD_TIMEOUT;
__private_extern__ uint32_t smbfs_hard_deadtimer = HARD_DEAD_TIMER;
__private_extern__ uint32_t smbfs_trigger_deadtimer = TRIGGER_DEAD_TIMEOUT;

static int smbfs_version = SMBFS_VERSION;
static int mount_cnt = 0;
int dev_open_cnt = 0;
int unloadInProgress = FALSE;

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
lck_grp_attr_t   * dev_lck_grp_attr;
lck_grp_t  * dev_lck_grp;
lck_attr_t * dev_lck_attr;
lck_rw_t * dev_rw_lck;
lck_grp_attr_t   * hash_lck_grp_attr;
lck_grp_t  * hash_lck_grp;
lck_attr_t * hash_lck_attr;

struct smbmnt_carg {
	vfs_context_t context;
	struct mount *mp;
	int found;
};

SYSCTL_DECL(_net_smb);
SYSCTL_NODE(_net_smb, OID_AUTO, fs, CTLFLAG_RW, 0, "SMB/CIFS file system");
SYSCTL_INT(_net_smb_fs, OID_AUTO, version, CTLFLAG_RD, &smbfs_version, 0, "");
SYSCTL_INT(_net_smb_fs, OID_AUTO, loglevel, CTLFLAG_RW,  &smbfs_loglevel, 0, "");
SYSCTL_INT(_net_smb_fs, OID_AUTO, kern_deadtimer, CTLFLAG_RW, &smbfs_deadtimer, DEAD_TIMEOUT, "");
SYSCTL_INT(_net_smb_fs, OID_AUTO, kern_hard_deadtimer, CTLFLAG_RW, &smbfs_hard_deadtimer, HARD_DEAD_TIMER, "");
SYSCTL_INT(_net_smb_fs, OID_AUTO, kern_soft_deadtimer, CTLFLAG_RW, &smbfs_trigger_deadtimer, TRIGGER_DEAD_TIMEOUT, "");

extern struct sysctl_oid sysctl__net_smb;
extern struct sysctl_oid sysctl__net_smb_fs_version;
extern struct sysctl_oid sysctl__net_smb_fs_loglevel;
extern struct sysctl_oid sysctl__net_smb_fs_kern_deadtimer;
extern struct sysctl_oid sysctl__net_smb_fs_kern_hard_deadtimer;
extern struct sysctl_oid sysctl__net_smb_fs_kern_soft_deadtimer;
extern struct sysctl_oid sysctl__net_smb_fs_tcpsndbuf;
extern struct sysctl_oid sysctl__net_smb_fs_tcprcvbuf;

MALLOC_DEFINE(M_SMBFSHASH, "SMBFS hash", "SMBFS hash table");

#ifndef VFS_CTL_DISC
#define VFS_CTL_DISC	0x00010008	/* Server is disconnected */
#endif

static void 
smbfs_lock_init()
{
	hash_lck_attr = lck_attr_alloc_init();
	hash_lck_grp_attr = lck_grp_attr_alloc_init();
	hash_lck_grp = lck_grp_alloc_init("smb-hash", hash_lck_grp_attr);
	
	smbfs_lock_attr    = lck_attr_alloc_init();
	smbfs_group_attr   = lck_grp_attr_alloc_init();
	smbfs_mutex_group  = lck_grp_alloc_init("smb-mutex", smbfs_group_attr);
	smbfs_rwlock_group = lck_grp_alloc_init("smbfs-rwlock", smbfs_group_attr);
}

static void 
smbfs_lock_uninit()
{
	lck_grp_free(smbfs_mutex_group);
	lck_grp_free(smbfs_rwlock_group);
	lck_grp_attr_free(smbfs_group_attr);
	lck_attr_free(smbfs_lock_attr);

	lck_grp_free(hash_lck_grp);
	lck_grp_attr_free(hash_lck_grp_attr);
	lck_attr_free(hash_lck_attr);
}

static void 
smbnet_lock_init()
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
	
	dev_lck_attr = lck_attr_alloc_init();
	dev_lck_grp_attr = lck_grp_attr_alloc_init();
	dev_lck_grp = lck_grp_alloc_init("smb-dev", dev_lck_grp_attr);
	dev_rw_lck = lck_rw_alloc_init(dev_lck_grp, dev_lck_attr);
}

static void 
smbnet_lock_uninit()
{
	lck_grp_free(dev_lck_grp);
	lck_grp_attr_free(dev_lck_grp_attr);
	lck_attr_free(dev_lck_attr);
	
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
 * Need to check and make sure the server is in the same domain, if not
 * then we need to turn off ACL support.
 */
static void 
isServerInSameDomian(struct smb_share *share, struct smbmount *smp)
{
	/* Just to be safe */
	SMB_FREE(smp->ntwrk_sids, M_TEMP);
	smp->ntwrk_sids_cnt = 0;
	if (SSTOVC(share)->vc_flags & SMBV_NETWORK_SID) {
		/* See if the VC network SID is known by Directory Service */
		if ((smp->sm_args.altflags & SMBFS_MNT_DEBUG_ACL_ON) ||
			(smp->sm_args.altflags & SMBFS_MNT_TIME_MACHINE) ||
			(smbfs_is_sid_known(&SSTOVC(share)->vc_ntwrk_sid))) {
			SMB_MALLOC(smp->ntwrk_sids, ntsid_t *, sizeof(ntsid_t), M_TEMP, M_WAITOK);
			memcpy(smp->ntwrk_sids, &SSTOVC(share)->vc_ntwrk_sid, sizeof(ntsid_t));
			smp->ntwrk_sids_cnt = 1;
			return;
		}
	}
	SMBWARNING("%s: can't determine if server is in the same domain, turning off ACLs support.\n",
			   vfs_statfs(smp->sm_mp)->f_mntfromname);
	share->ss_attributes &= ~FILE_PERSISTENT_ACLS;
}

/*
 * The share needs to be locked before calling this rouitne!
 *
 * smbfs_down is called when we have a message that timeout or we are
 * starting a reconnect. It uses vfs_event_signal() to tell interested parties 
 * the connection with the server is "down".
 */
static int
smbfs_down(struct smb_share *share, int timeToNotify)
{
	struct smbmount *smp;
	int treenct = 1;
	
	smp = share->ss_mount;
	/* We have already unmounted or we are being force unmount, we are done */
	if ((smp == NULL) || (vfs_isforce(smp->sm_mp))) {
		return 0;
	}

	/* 
	 * They are attempted to unmount it so don't count this one. 
	 * Still notify them they may want to force unmount it.
	 */
	if (vfs_isunmount(smp->sm_mp)) {
		treenct = 0;
	}
	
	/* Attempt to remount the Dfs volume */
	if (treenct && (smp->sm_args.altflags & SMBFS_MNT_DFS_SHARE) &&
		!(smp->sm_status & SM_STATUS_REMOUNT)) {
		smp->sm_status |= SM_STATUS_REMOUNT;
		/* Never do Dfs failover if the share is FAT or doing Unix Extensions */
		if ((IPC_PORT_VALID(SSTOVC(share)->vc_gss.gss_mp)) && 
			(share->ss_fstype != SMB_FS_FAT) && !(UNIX_CAPS(share))) {
			
			/* Call autofs with the fsid to start the remount */
			SMBERROR("Remounting %s/%s\n", SSTOVC(share)->vc_srvname, 
					 share->ss_name);
			if (SMBRemountServer(&(vfs_statfs(smp->sm_mp))->f_fsid, 
								 sizeof(vfs_statfs(smp->sm_mp)->f_fsid),
								 SSTOVC(share)->vc_gss.gss_asid)) {
				/* Something went wrong try again next time */
				SMBERROR("Something went wrong with remounting %s/%s\n", 
						 SSTOVC(share)->vc_srvname, share->ss_name);
				smp->sm_status &= ~SM_STATUS_REMOUNT;
			}
		} else {
			SMBWARNING("Skipping remounting %s/%s, file system type mismatch\n", 
					   SSTOVC(share)->vc_srvname, share->ss_name);
		}
	}
	
	/* We need to notify and we haven't notified before then notify */
	if (timeToNotify && !(smp->sm_status & SM_STATUS_DOWN)) {
		int dontNotify = ((smp->sm_args.altflags & SMBFS_MNT_SOFT) && 
						  (vfs_flags(smp->sm_mp) & MNT_DONTBROWSE));

		SMBWARNING("Share %s not responding\n", share->ss_name);
		/* Never notify on soft-mounted nobrowse volumes */
		if (!dontNotify) {
			vfs_event_signal(&(vfs_statfs(smp->sm_mp))->f_fsid, VQ_NOTRESP, 0);
		}
		smp->sm_status |= SM_STATUS_DOWN;
	}
	return treenct;
}

/*
 * The share needs to be locked before calling this rouitne!
 *
 * smbfs_up is called when we receive a successful response to a message or we have 
 * successfully reconnect. It uses vfs_event_signal() to tell interested parties 
 * the connection is OK again  if the connection was having problems.
 */
static void
smbfs_up(struct smb_share *share, int reconnect)
{
	struct smbmount *smp;
	
	smp = share->ss_mount;
	/* We have already unmounted or we are being force unmount, we are done */
	if ((smp == NULL) || (vfs_isforce(smp->sm_mp))) {
		return;
	}
	if (reconnect) {
		smbfs_reconnect(smp);
	}
	/* We are done remounting, either we reconnect or the remount worked */
	smp->sm_status &= ~SM_STATUS_REMOUNT;
	
	if (smp->sm_status & SM_STATUS_DOWN) {
		int dontNotify = ((smp->sm_args.altflags & SMBFS_MNT_SOFT) && 
						  (vfs_flags(smp->sm_mp) & MNT_DONTBROWSE));

		smp->sm_status &= ~SM_STATUS_DOWN;
		SMBWARNING("Share %s responding\n", share->ss_name);
		/* Never notify on soft-mounted nobrowse volumes */
		if (!dontNotify) {
			vfs_event_signal(&(vfs_statfs(smp->sm_mp))->f_fsid, VQ_NOTRESP, 1);	
		}
	}
}

/*
 * The share needs to be locked before calling this rouitne!
 *
 * smbfs_dead is called when the share is no longer reachable and the dead timer
 * has gone off. It uses vfs_event_signal() to tell interested parties 
 * the connection is gone. This should cause the mount to get forced unmounted.
 */
static void
smbfs_dead(struct smb_share *share)
{
	struct smbmount *smp;
	
	smp = share->ss_mount;
	if (smp && !(smp->sm_status & SM_STATUS_DEAD)) {
		/* If we have a ss_mount then we have a sm_mp */
		SMBWARNING("Share %s has gone away, unmounting the volume\n", 
				   share->ss_name);
		vfs_event_signal(&(vfs_statfs(smp->sm_mp))->f_fsid, VQ_DEAD, 0);
		smp->sm_status |= SM_STATUS_DEAD;
	}
}

/*
 * The share needs to be locked before calling this rouitne!
 *
 * See if the volume is being forced unmounted. In the future we will also 
 * check for the share getting changed out because of Dfs trigger remounts.
 */
static int 
smbfs_is_going_away(struct smb_share* share)
{	
	struct smbmount *smp;
	
	smp = share->ss_mount;
	/* If we have a ss_mount then we have a sm_mp */
	if (smp && (vfs_isforce(smp->sm_mp))) {
		 return TRUE;			
	}
	return FALSE;
}

/*
 * Fill in the smb_remount_info structure with all the information that user
 * land needs to remount the volume.
 */
static int
smbfs_remountInfo(struct mount *mp, struct smb_share *share, 
				  struct smb_remount_info *info)
{	
#ifdef SMBDEBUG_REMOUNT
	/* Used for testing only, pretend we are in reconnect. */
	share->ss_flags |= SMBS_RECONNECTING;
#endif // SMBDEBUG_REMOUNT
	SMB_ASSERT(SSTOVC(share)->vc_gss.gss_cpn_len < sizeof(info->mntClientPrincipalName));
	bzero(info, sizeof(*info));
	info->version = REMOUNT_INFO_VERSION;
	info->mntAuthFlags = SSTOVC(share)->vc_flags & SMBV_USER_LAND_MASK;
	info->mntOwner = VFSTOSMBFS(mp)->sm_args.uid;
	info->mntGroup = VFSTOSMBFS(mp)->sm_args.gid;
	/* 
	 * The default is 30 seconds, but this is setable with a sysctl. The timer 
	 * has already started, but normally it takes only a couple of seconds to get
	 * to this point.
	 */
	info->mntDeadTimer = share->ss_dead_timer;
	if (!info->mntDeadTimer) {
		/* Never wait less than one second */
		info->mntDeadTimer = 1;
	}

	strlcpy(info->mntURL, vfs_statfs(mp)->f_mntfromname, sizeof(info->mntURL));
	strlcpy(info->mntClientPrincipalName, (char *)SSTOVC(share)->vc_gss.gss_cpn, 
			SSTOVC(share)->vc_gss.gss_cpn_len);
	info->mntClientPrincipalNameType  = SSTOVC(share)->vc_gss.gss_client_nt;
	return 0;
}

/*
 * Remount the volume by replacing the old share with the new share that was 
 * obtained using the device id.
 */

static int
smbfs_remount(int32_t dev, struct mount *mp, struct smbmount *smp, 
			  vfs_context_t context)
{
	int error = 0;
	struct smb_share *share = NULL;
	struct smb_share *new_share = NULL;
	
	error = smb_dev2share(dev, &new_share);
	if (error || !new_share) {
		return (error) ? error : ENOMEM;
	}
	/* 
	 * Can't completely protect from throwing away a perfectly good connection, 
	 * but we can make the window pretty short. So if the old share is not in
	 * reconnect mode, we should just get out, nothing for us to do here. If
	 * for some strange reason the  new share is the same as the old share 
	 * just get out. We could have reconnected, found the same share and then
	 * the connection went down again.
	 */
	share = smb_get_share_with_reference(smp);
	if (!(share->ss_flags & SMBS_RECONNECTING) || (new_share == share)) {
		/* Done with the new share release the reference */
		smb_share_rele(new_share, context);
		/* Mark that the remount completed, so it can be used again. */
		smp->sm_status &= ~SM_STATUS_REMOUNT;
		smb_share_rele(share, context);				
		return EEXIST;				
	}
	smb_share_rele(share, context);				
	
	/*
	 * We now lock the VFS from accessing either share until we are done here. 
	 * All new VFS operations will be blocked, any old operation will continue 
	 * until they need to access the smp or share again.
	 */
	lck_rw_lock_exclusive(&smp->sm_rw_sharelock);
	/*
	 * Now block any calls to the new share, since nothing is really happening 
	 * on the share this should be safe. But it could block other access on the 
	 * new vc until we are done, but we never sleep here so that should be ok.
	 */
	lck_mtx_lock(&new_share->ss_shlock);
	/*
	 * Finally lock the old share, This could block the vc reconnect code, but 
	 * not much we can do about that here. Remember the mount holds a reference
	 * on the old share and we are under the lock now so we can access it 
	 * directly without any issues.
	 */
	share = smp->sm_share;
	lck_mtx_lock(&share->ss_shlock);
	if (SSTOVC(new_share)->throttle_info) {
		/* Taking a reference here will release the old reference */ 
		throttle_info_mount_ref(mp, SSTOVC(new_share)->throttle_info);
	} else if (SSTOVC(share)->throttle_info) {
		/* 
		 * The new vc doesn't have any throttle info, but we have one on the 
		 * old VC, release the reference. 
		 */
		throttle_info_mount_rel(mp);
	}
	/* Take a volume count, since this share has a mount now */
	(void)OSAddAtomic(1, &SSTOVC(new_share)->vc_volume_cnt);
	/* 
	 * We support allowing information to changes on the VC and the share, except 
	 * for information obtained from the smbfs_qfsattr, smbfs_unix_qfsattr, 
	 * isServerInSameDomian, and smbfs_unix_whoami routines. Once ACLs are turned
	 * on they stay on, Better be in the same domain, nothing we can do about
	 * this one. So we have really only two issues here:
	 * 1. The file system types of the two shares don't match, FAT vs NTFS, need
	 *    to prevent this from happening. We just can't support switching between
	 *    dot underbar files and named streams, so we should never allow FAT 
	 *    file system to failover and we should never remount a FAT Share.
	 * 2. One of the shares supports UNIX Extensions and the other doesn't. If 
	 *    the old share is doing UNIX Extensions then never failover. If the new 
	 *    share does UNIX Extensions, ignore those and treat the server like any
	 *    other Windows system.  
	 */
	new_share->ss_fstype = share->ss_fstype; /* Always the same file system type */
	new_share->ss_attributes = share->ss_attributes;
	new_share->ss_maxfilenamelen = share->ss_maxfilenamelen;
	new_share->ss_unix_caps = share->ss_unix_caps;
	new_share->ss_going_away = smbfs_is_going_away;
	new_share->ss_down = smbfs_down;
	new_share->ss_up = smbfs_up;
	new_share->ss_dead = smbfs_dead;
	new_share->ss_dead_timer = share->ss_dead_timer;
	/* Now remove the mount point from the old share */
	share->ss_mount = NULL;
	/* Now add the mount point to the new share */
	new_share->ss_mount = smp;
	/* Now add the new share to the mount point */			
	smp->sm_share = new_share;
	smp->sm_status |= SM_STATUS_UPDATED;
	SMBERROR("replacing %s/%s with %s/%s\n",  SSTOVC(share)->vc_srvname, 
			 share->ss_name, SSTOVC(new_share)->vc_srvname, new_share->ss_name);
	/* Now unlock in reverse order */
	lck_mtx_unlock(&share->ss_shlock);
	lck_mtx_unlock(&new_share->ss_shlock);
	lck_rw_unlock_exclusive(&smp->sm_rw_sharelock);
	smb_iod_errorout_share_request(share, ETIMEDOUT);
	/* Remove the old share's volume count, since it no longer has a mount */
	(void)OSAddAtomic(-1, &SSTOVC(share)->vc_volume_cnt);
	/* Release the old share */
	smb_share_rele(share, context);
	/* Now get the new share and notify everyone we are up */
	share = smb_get_share_with_reference(smp);
	smbfs_up(share, TRUE);
	smb_share_rele(share, context);		
	return 0;
}

static int
smbfs_mount(struct mount *mp, vnode_t devvp, user_addr_t data, vfs_context_t context)
{
#pragma unused (devvp)
	struct smb_mount_args *args = NULL;		/* will hold data from mount request */
	struct smbmount *smp = NULL;
	struct smb_share *share = NULL;
	struct vfsioattr smbIOAttr;
	vnode_t vp;
	int error;

	if (data == USER_ADDR_NULL) {
		SMBDEBUG("missing data argument\n");
		error = EINVAL;
		goto bad;
	}
	
	SMB_MALLOC(args, struct smb_mount_args *, sizeof(*args), M_SMBFSDATA, 
		   M_WAITOK | M_ZERO);
	if (!args) {
		SMBDEBUG("Couldn't malloc the mount arguments!");
		error = ENOMEM;
		goto bad;
	}
	error = copyin(data, (caddr_t)args, sizeof(*args));
	if (error) {
		SMBDEBUG("Couldn't copyin the mount arguments!");
		goto bad;
	}
	
	if (args->version != SMB_IOC_STRUCT_VERSION) {
		SMBERROR("Mount structure version mismatch: kernel=%d, mount=%d\n", 
				 SMB_IOC_STRUCT_VERSION, args->version);
		error = EINVAL;
		goto bad;
	}
	
	/* Set the debug level, if set to us. */
	if (args->KernelLogLevel) {
		smbfs_loglevel =  args->KernelLogLevel;
	}
	/*
	 * Get the share and retain a reference count until we unmount or complete
	 * a mount update. The smb vfs policy requires that the share can only be
	 * passed into a routine if an extra reference has been taken on the share.
	 * Any routine require accessing the share from the mount point must call
	 * smb_get_share_with_reference to get a pointer to the share. No routine
	 * except mount and unmount should every access the mount points share 
	 * directly.
	 */
	error = smb_dev2share(args->dev, &share);
	if (error) {
		SMBDEBUG("invalid device handle %d (%d)\n", args->dev, error);
		goto bad;
	}
	
	/* Need to deal with the mount update here */
	if (vfs_isupdate(mp)) {
		SMBERROR("MNT_UPDATE not supported!");
		error = ENOTSUP;
		goto bad;
	}
	
	SMB_MALLOC(smp, struct smbmount*, sizeof(*smp), M_SMBFSDATA, M_WAITOK | M_ZERO);
	if (smp == NULL) {
		SMBDEBUG("Couldn't malloc the smb mount structure!");
		error = ENOMEM;
		goto bad;
	}

	smp->sm_mp = mp;
	vfs_setfsprivate(mp, (void *)smp);	
	smp->sm_hash = hashinit(desiredvnodes, M_SMBFSHASH, &smp->sm_hashlen);
	if (smp->sm_hash == NULL)
		goto bad;
	smp->sm_hashlock = lck_mtx_alloc_init(hash_lck_grp, hash_lck_attr);
	lck_rw_init(&smp->sm_rw_sharelock, smbfs_rwlock_group, smbfs_lock_attr);
	lck_mtx_init(&smp->sm_statfslock, smbfs_mutex_group, smbfs_lock_attr);		
	lck_mtx_init(&smp->sm_reclaim_renamelock, smbfs_mutex_group, smbfs_lock_attr);

	lck_rw_lock_exclusive(&smp->sm_rw_sharelock);
	smp->sm_share = share;
	lck_rw_unlock_exclusive(&smp->sm_rw_sharelock);
	smp->sm_rvp = NULL;
	/* Save any passed in arguments that we may need */
	smp->sm_args.altflags = args->altflags;
	smp->sm_args.uid = args->uid;
	smp->sm_args.gid = args->gid;
	error = kauth_cred_uid2guid(smp->sm_args.uid, &smp->sm_args.uuid);
	if (error) {
		SMBERROR("Couldn't get the mounted users UUID, uid = %d error = %d\n", 
				 smp->sm_args.uid, error);
		goto bad;
	}
	smp->sm_args.file_mode = args->file_mode & ACCESSPERMS;
	smp->sm_args.dir_mode  = args->dir_mode & ACCESSPERMS;
	if (args->volume_name[0]) {
		smp->sm_args.volume_name = smb_strndup(args->volume_name, 
											   sizeof(args->volume_name));
	} else {
		smp->sm_args.volume_name = NULL;
	}
	/*
	 * This call should be done from mount() in vfs layer. Not sure why each 
	 * file system has to do it here, but go ahead and make an internal call to 
	 * fill in the default values.
	 */
	error = smbfs_statfs(share, vfs_statfs(mp), context);
	if (error) {
		SMBDEBUG("The smbfs_statfs failed %d\n", error);
		goto bad;
	}
	/* Copy in the from name, used for reconnects and other things  */
	strlcpy(vfs_statfs(mp)->f_mntfromname, args->url_fromname, MAXPATHLEN);
	/* See if they sent use a starting path to use */
	if (args->path_len) {
		smbfs_create_start_path(smp, args, SMB_UNICODE_STRINGS(SSTOVC(share)));
	}
	/* Now get the mounted volumes unique id */
	smp->sm_args.unique_id_len = args->unique_id_len;
	SMB_MALLOC(smp->sm_args.unique_id, unsigned char *, smp->sm_args.unique_id_len, 
		   M_SMBFSDATA, M_WAITOK);
	if (smp->sm_args.unique_id) {
		bcopy(args->unique_id, smp->sm_args.unique_id, smp->sm_args.unique_id_len);
	} else {
		smp->sm_args.unique_id_len = 0;
	}
	SMB_FREE(args, M_SMBFSDATA);	/* Done with the args free them */

    if (smp->sm_args.altflags & SMBFS_MNT_TIME_MACHINE) {
        SMBWARNING("%s mounted using tm flag\n",  vfs_statfs(mp)->f_mntfromname);
    }

	vfs_getnewfsid(mp);
	
	/*
	 * Need to get the remote server's file system information
	 * here before we do anything else. Make sure we have the servers or
	 * the default value for ss_maxfilenamelen. NOTE: We use it in strnlen.
	 */
	smbfs_qfsattr(share, context);
	
	/* Its a unix server see if it supports any of the UNIX extensions */
	if (UNIX_SERVER(SSTOVC(share))) {
		smbfs_unix_qfsattr(share, context);
	}

	/* 
	 * This volume was mounted as guest, turn off ACLs and set the mount point to 
	 * ignore ownership. We will always return an owner of 99, and group of 99.
	 */
	if (SMBV_HAS_GUEST_ACCESS(SSTOVC(share))) {
		if (share->ss_attributes & FILE_PERSISTENT_ACLS) {
			SMB_LOG_ACCESS("%s was mounted as guest turning off ACLs support.\n", 
						   vfs_statfs(mp)->f_mntfromname);
		}
		share->ss_attributes &= ~FILE_PERSISTENT_ACLS;
		vfs_setflags(mp, MNT_IGNORE_OWNERSHIP);		
	}
	
	/* Make sure the server is in the same domain, if not turn off acls */
	if (share->ss_attributes & FILE_PERSISTENT_ACLS) {
		isServerInSameDomian(share, smp);
	}
	
	/* See if the server supports the who am I operation */ 
	if (UNIX_CAPS(share) & CIFS_UNIX_POSIX_PATH_OPERATIONS_CAP) {
		smbfs_unix_whoami(share, smp, context);
	}
	
	error = smbfs_root(mp, &vp, context);
	if (error) {
		SMBDEBUG("The smbfs_root failed %d\n", error);
		goto bad;
	}
	/*
	 * This UNIX Server says it supports the UNIX Extensions, but it doesn't
	 * support all the options we require. Turn off the UNIX Extensions that
	 * they don't support.
	 */
	if ((UNIX_CAPS(share) & UNIX_QFILEINFO_UNIX_INFO2_CAP) &&
		((VTOSMB(vp)->n_flags_mask & EXT_REQUIRED_BY_MAC) != EXT_REQUIRED_BY_MAC)) {
		/* Turn off the UNIX Info2  */
		UNIX_CAPS(share) &= ~(UNIX_SFILEINFO_UNIX_INFO2_CAP);
		/* Must be Linux, turn on the unlink call so we can delete symlink files */
		UNIX_CAPS(share) |= UNIX_SFILEINFO_POSIX_UNLINK_CAP;
		/* Force the root vnode to forget any UNIX Extensions Info */
		VTOSMB(vp)->attribute_cache_timer = 0;
		VTOSMB(vp)->n_uid = KAUTH_UID_NONE;
		VTOSMB(vp)->n_gid = KAUTH_GID_NONE;
	}
	vfs_setauthopaque (mp);
	/* we can always answer access questions better than local VFS */
	vfs_setauthopaqueaccess (mp);

	if (share->ss_attributes & FILE_PERSISTENT_ACLS) {
		guid_t ntwrk_uuid = kauth_null_guid;
		
		DBG_ASSERT(smp->ntwrk_sids); /* We should always have a network SID */
		SMB_LOG_ACCESS("%s support ACLs\n", vfs_statfs(mp)->f_mntfromname);
		vfs_setextendedsecurity(mp);
		/* Now test to see if we should be mapping the local user to the nework user */
		if (kauth_cred_ntsid2guid(smp->ntwrk_sids, &ntwrk_uuid)) {
			smp->sm_flags |= MNT_MAPS_NETWORK_LOCAL_USER;
		} else if (!kauth_guid_equal(&smp->sm_args.uuid, &ntwrk_uuid)) {
			smp->sm_flags |= MNT_MAPS_NETWORK_LOCAL_USER;
		}
	} else {
		SMB_LOG_ACCESS("%s doesn't support ACLs\n", vfs_statfs(mp)->f_mntfromname);
		vfs_clearextendedsecurity (mp);
	}
	if ((share->ss_attributes & FILE_SUPPORTS_REPARSE_POINTS) && 
		(((!UNIX_CAPS(share)) || (SSTOVC(share)->vc_flags & SMBV_DARWIN)))) {
		smp->sm_flags |= MNT_SUPPORTS_REPARSE_SYMLINKS;
	}
	/* 
	 * This is a read-only volume, so change the mount flags so
	 * the finder will show it as a read-only volume. 
	 */ 
	if (share->ss_attributes & FILE_READ_ONLY_VOLUME) {
		vfs_setflags(mp, MNT_RDONLY);
	} else if ((share->maxAccessRights & FILE_FULL_WRITE_ACCESS) == 0) {
		SMB_LOG_ACCESS("Share ACL doesn't allow write access, maxAccessRights are 0x%x\n", 
					   share->maxAccessRights);
		vfs_setflags(mp, MNT_RDONLY);
	}
	/*
	 * We now default to have named streams on if the server supports named 
	 * streams. The user can turn off named streams by setting the correct 
	 * option in the nsmb.conf file or by creating a file on the top level of 
	 * the share called ".com.apple.smb.streams.off". The "nsmb.conf" allows the 
	 * user to turn off named streams per share. So now we only check for turning
	 * off named streams since the default is to have them on.
	 *
	 * .com.apple.smb.streams.on - If exist on top level of share means turn on streams.
	 * .com.apple.smb.streams.off - If exist on top level of share means turn off streams.
	 *
	 */
	if (share->ss_attributes & FILE_NAMED_STREAMS) {		
		if (!(smp->sm_args.altflags & SMBFS_MNT_STREAMS_ON) || 
			(smbfs_smb_query_info(share, VTOSMB(vp), SMB_STREAMS_OFF, 
								  sizeof(SMB_STREAMS_OFF) - 1, NULL, context) == 0)) {
			share->ss_attributes &= ~FILE_NAMED_STREAMS;
		} else if (! UNIX_SERVER(SSTOVC(share)) && 
				   (smbfs_smb_qstreaminfo(share, VTOSMB(vp), NULL, NULL,
                                          SFM_DESKTOP_NAME, NULL, context) == 0)) {
			/* 
			 * We would like to know if this is a SFM Volume, we skip this 
			 * check for unix servers. 
			 */
			smp->sm_flags |= MNT_IS_SFM_VOLUME;
		}
	}
	
	/*
	 * The AFP code sets io_devblocksize to one, which is used by the Cluster IO
	 * code to decide what to do when writing pass the eof.  ClusterIO code uses 
	 * the io_devblocksize to decided what size block to use when writing pass 
	 * the eof. So a io_devblocksize of one means only write to the eof. Seems
	 * like a hack, but not sure what else to do at this point. Talk this over
	 * with Joe and he wants to get back to it later.
	 */
	vfs_ioattr(mp, &smbIOAttr);	/* get the current settings */
	smbIOAttr.io_devblocksize = 1;
	/*
	 * Should we just leave these set to the default, should we base them on the 
	 * server buffer size or should we try to calculate WAN/LAN speed. For now 
	 * lets base them on the server buffer size.
	 */
	smbIOAttr.io_maxsegreadsize = (uint32_t)vfs_statfs(mp)->f_iosize;
	smbIOAttr.io_maxsegwritesize = (uint32_t)vfs_statfs(mp)->f_iosize;
	smbIOAttr.io_segreadcnt = smbIOAttr.io_maxsegreadsize / PAGE_SIZE;
	smbIOAttr.io_segwritecnt = smbIOAttr.io_maxsegwritesize / PAGE_SIZE;
	vfs_setioattr(mp, &smbIOAttr);
	
	/* smbfs_root did a vnode_get and a vnode_ref, so keep the ref but release the get */
	vnode_put(vp);
	/* We now have everyting we need to setup the dead/up/down routines */
	lck_mtx_lock(&share->ss_shlock);
	/* Use to tell the VC that the share is going away, so just timeout messages */
	share->ss_going_away = smbfs_is_going_away;
	/* Routines to call when the mount is having problems */
	share->ss_down = smbfs_down;
	share->ss_up = smbfs_up;
	share->ss_dead = smbfs_dead;
	if (smp->sm_args.altflags & SMBFS_MNT_SOFT) {
		if (smp->sm_args.altflags & SMBFS_MNT_DFS_SHARE) {
			/* 
			 * Dfs Trigger node, set the dead timer to something smaller and
			 * never soft mount time out the operations
			 */
			share->ss_dead_timer = smbfs_trigger_deadtimer;
		} else {
			/* Timeout the operations */
			share->ss_soft_timer = SOFTMOUNT_TIMEOUT;
			/* Soft mounts use the default timer value */
			share->ss_dead_timer = smbfs_deadtimer;
		}
	} else {
		share->ss_dead_timer = smbfs_hard_deadtimer;
	}	
	/* All done add the mount point to the share so we can access these routines */
	share->ss_mount = smp;
	lck_mtx_unlock(&share->ss_shlock);
	SMBDEBUG("%s dead timer = %d\n", share->ss_name, share->ss_dead_timer);

	OSAddAtomic(1, &SSTOVC(share)->vc_volume_cnt);
	if (SSTOVC(share)->throttle_info) {
		throttle_info_mount_ref(mp, SSTOVC(share)->throttle_info);
	}
	
    smbfs_notify_change_create_thread(smp);
    if (smp->sm_args.altflags & SMBFS_MNT_COMPOUND_ON) {
        vfs_setcompoundopen(mp);
    }
    else {
        SMBWARNING("compound off in preferences\n");
    }

	mount_cnt++;
	return (0);
bad:
	if (share) {
		lck_mtx_lock(&share->ss_shlock);
		share->ss_mount = NULL;	/* share->ss_mount is smp which we free below  */ 
		lck_mtx_unlock(&share->ss_shlock);
		smb_share_rele(share, context);
	}
	if (smp) {
		vfs_setfsprivate(mp, (void *)0);
		/* Was malloced by hashinit */
		if (smp->sm_hash)
			SMB_FREE(smp->sm_hash, M_SMBFSHASH);
		lck_mtx_free(smp->sm_hashlock, hash_lck_grp);
		lck_mtx_destroy(&smp->sm_statfslock, smbfs_mutex_group);
		lck_mtx_destroy(&smp->sm_reclaim_renamelock, smbfs_mutex_group);
		lck_rw_destroy(&smp->sm_rw_sharelock, smbfs_rwlock_group);
		SMB_FREE(smp->sm_args.volume_name, M_SMBSTR);	
		SMB_FREE(smp->sm_args.path, M_SMBFSDATA);
		SMB_FREE(smp->sm_args.unique_id, M_SMBFSDATA);
		SMB_FREE(smp->ntwrk_gids, M_TEMP);
		SMB_FREE(smp->ntwrk_sids, M_TEMP);
		SMB_FREE(smp, M_SMBFSDATA);
	}
	SMB_FREE(args, M_SMBFSDATA); /* Done with the args free them */		
	return (error);
}

/* Unmount the filesystem described by mp. */
static int
smbfs_unmount(struct mount *mp, int mntflags, vfs_context_t context)
{
	struct smbmount *smp = VFSTOSMBFS(mp);
	vnode_t vp;
	int error;

	SMBVDEBUG("smbfs_unmount: flags=%04x\n", mntflags);

	/* Force unmount shutdown all outstanding I/O requests on this share. */
	if (mntflags & MNT_FORCE) {
		smb_iod_errorout_share_request(smp->sm_share, ENXIO);
	}

	error = smbfs_root(mp, &vp, context);
	if (error)
		return (error);

	error = vflush(mp, vp, (mntflags & MNT_FORCE) ? FORCECLOSE : 0);
	if (error) {
		vnode_put(vp);
		return (error);
	}
	if (vnode_isinuse(vp, 1)  && !(mntflags & MNT_FORCE)) {
		SMBDEBUG("smbfs_unmount: usecnt\n");
		vnode_put(vp);
		return (EBUSY);
	}
	smp->sm_rvp = NULL;	/* We no longer have a reference so clear it out */
	vnode_rele(vp);	/* to drop ref taken by smbfs_mount */
	vnode_put(vp);	/* to drop ref taken by VFS_ROOT above */

	(void)vflush(mp, NULLVP, FORCECLOSE);
	
	/* We are done with this share shutdown all outstanding I/O requests. */
	smb_iod_errorout_share_request(smp->sm_share, ENXIO);
	
	OSAddAtomic(-1, &SSTOVC(smp->sm_share)->vc_volume_cnt);
	smbfs_notify_change_destroy_thread(smp);

	if (SSTOVC(smp->sm_share)->throttle_info)
		throttle_info_mount_rel(mp);
	
	/* Remove the smb mount pointer from the share before freeing it */
	lck_mtx_lock(&smp->sm_share->ss_shlock);
	smp->sm_share->ss_mount = NULL;
	smp->sm_share->ss_dead = NULL;
	smp->sm_share->ss_up = NULL;
	smp->sm_share->ss_down = NULL;
	lck_mtx_unlock(&smp->sm_share->ss_shlock);
	 
	smb_share_rele(smp->sm_share, context);
	vfs_setfsprivate(mp, (void *)0);

	if (smp->sm_hash) {
		SMB_FREE(smp->sm_hash, M_SMBFSHASH);
		smp->sm_hash = (void *)0xDEAD5AB0;
	}
	lck_mtx_free(smp->sm_hashlock, hash_lck_grp);
	lck_mtx_destroy(&smp->sm_statfslock, smbfs_mutex_group);
	lck_mtx_destroy(&smp->sm_reclaim_renamelock, smbfs_mutex_group);
	lck_rw_destroy(&smp->sm_rw_sharelock, smbfs_rwlock_group);
	SMB_FREE(smp->sm_args.volume_name, M_SMBSTR);	
	SMB_FREE(smp->sm_args.path, M_SMBFSDATA);
	SMB_FREE(smp->sm_args.unique_id, M_SMBFSDATA);
	SMB_FREE(smp->ntwrk_gids, M_TEMP);
	SMB_FREE(smp->ntwrk_sids, M_TEMP);
	SMB_FREE(smp, M_SMBFSDATA);
	vfs_clearflags(mp, MNT_LOCAL);
	mount_cnt--;
	return (0);
}

/* 
 * Return locked root vnode of a filesystem
 */
static int 
smbfs_root(struct mount *mp, vnode_t *vpp, vfs_context_t context)
{
	struct smbmount *smp = VFSTOSMBFS(mp);
	struct smb_share *share = NULL;
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
	nanouptime(&fattr.fa_reqtime);
	fattr.fa_valid_mask |= FA_VTYPE_VALID;
	fattr.fa_attr = SMB_EFA_DIRECTORY;
	fattr.fa_vtype = VDIR;
	fattr.fa_ino = 2;
	/*
	 * First time to get the root vnode, smbfs_nget will create it and check 
	 * with the network to make sure all is well with the root node. Could get 
	 * an error if the device is not ready are we have no access.
	 */
	share = smb_get_share_with_reference(smp);
	error = smbfs_nget(share, mp, NULL, "TheRooT", 7, &fattr, &vp, 0, context);
	smb_share_rele(share, context);
	if (error)
		return (error);
	/* 
	 * Since root vnode has an exclusive lock, I know only one process can be 
	 * here at this time.  Check once more while I still have the lock that 
	 * sm_rvp is still NULL before taking a ref and saving it. 
	 */
	if (smp->sm_rvp == NULL) {
		smp->sm_rvp = vp;	/* this will be released in the unmount code */
		smbnode_unlock(VTOSMB(vp));	/* Release the smbnode lock */
		/* 
		 * Now save a ref to this vnode so that we can quickly retrieve in 
		 * subsequent calls and make sure it doesn't go away until we unmount.
		 */
		error = vnode_ref(vp);
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
smbfs_start(struct mount *mp, int flags, vfs_context_t context)
{
#pragma unused(mp, flags, context)
	return 0;
}

/*ARGSUSED*/
static int
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
 * smbfs_vfs_getattr call
 */
static int 
smbfs_vfs_getattr(struct mount *mp, struct vfs_attr *fsap, vfs_context_t context)
{
	struct smbmount *smp = VFSTOSMBFS(mp);
	struct smb_share *share = NULL;
	struct vfsstatfs cachedstatfs;
	struct timespec ts;
	int error = 0;

	if ((smp->sm_rvp == NULL) || (VTOSMB(smp->sm_rvp) == NULL))
		return (EINVAL);
	
	share = smb_get_share_with_reference(smp);	
	lck_mtx_lock(&smp->sm_statfslock);
	cachedstatfs = smp->sm_statfsbuf;
	if (smp->sm_status & SM_STATUS_STATFS)
		lck_mtx_unlock(&smp->sm_statfslock);
	else {
		smp->sm_status |= SM_STATUS_STATFS;
		lck_mtx_unlock(&smp->sm_statfslock);
		nanouptime(&ts);
		/* We always check the first time otherwise only if the cache is stale. */
		if ((smp->sm_statfstime == 0) ||
			(((ts.tv_sec - smp->sm_statfstime) > SM_MAX_STATFSTIME) &&
			(VFSATTR_IS_ACTIVE(fsap, f_bsize) || VFSATTR_IS_ACTIVE(fsap, f_blocks) ||
			 VFSATTR_IS_ACTIVE(fsap, f_bfree) || VFSATTR_IS_ACTIVE(fsap, f_bavail) ||
			 VFSATTR_IS_ACTIVE(fsap, f_files) || VFSATTR_IS_ACTIVE(fsap, f_ffree)))) {
			/* update cached from-the-server data */
			error = smbfs_statfs(share, &cachedstatfs, context);
			if (error == 0) {
				nanouptime(&ts);
				smp->sm_statfstime = ts.tv_sec;
				lck_mtx_lock(&smp->sm_statfslock);
				smp->sm_statfsbuf = cachedstatfs;
				lck_mtx_unlock(&smp->sm_statfslock);
			} else {
				error = 0;
			}
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
	 * NOTE:  the valid field indicates whether your VFS knows whether a 
	 * capability is supported or not. So, if you know FOR SURE that a capability 
	 * is support or not, then set that bit in the valid part.  Then, in the 
	 * capabilities field, you either set it if supported or leave it clear if 
	 * not supported 
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
		if (VC_CAPS(SSTOVC(share)) & SMB_CAP_LARGE_FILES)
			cap->capabilities[VOL_CAPABILITIES_FORMAT] |= VOL_CAP_FMT_2TB_FILESIZE;
			
		/* Must be FAT so don't trust the modify times */
		if (share->ss_fstype == SMB_FS_FAT)
			cap->capabilities[VOL_CAPABILITIES_FORMAT] |= VOL_CAP_FMT_NO_ROOT_TIMES;
			
		if (share->ss_attributes & FILE_CASE_PRESERVED_NAMES)
			cap->capabilities[VOL_CAPABILITIES_FORMAT] |= VOL_CAP_FMT_CASE_PRESERVING;
		
		if (share->ss_attributes & FILE_SUPPORTS_SPARSE_FILES)
			cap->capabilities[VOL_CAPABILITIES_FORMAT] |= VOL_CAP_FMT_SPARSE_FILES;

		cap->capabilities[VOL_CAPABILITIES_INTERFACES] = 
			VOL_CAP_INT_ATTRLIST | 
			VOL_CAP_INT_FLOCK |
			VOL_CAP_INT_MANLOCK |
			0;
		
		if (UNIX_CAPS(share) & CIFS_UNIX_FCNTL_LOCKS_CAP)
			cap->capabilities[VOL_CAPABILITIES_INTERFACES] |= VOL_CAP_INT_ADVLOCK;
			
		if (share->ss_attributes & FILE_NAMED_STREAMS)
			cap->capabilities[VOL_CAPABILITIES_INTERFACES] |= VOL_CAP_INT_NAMEDSTREAMS | VOL_CAP_INT_EXTENDED_ATTR;			
		
		if ((SSTOVC(share)->vc_maxmux < SMB_NOTIFY_MIN_MUX)) {
			SMBWARNING("Notifications are not support on %s volume\n",
					   (smp->sm_args.volume_name) ? smp->sm_args.volume_name : "");		
		} else if ((smp->sm_args.altflags & SMBFS_MNT_NOTIFY_OFF) == SMBFS_MNT_NOTIFY_OFF) {
			SMBWARNING("Notifications have been turned off for %s volume\n", 
					   (smp->sm_args.volume_name) ? smp->sm_args.volume_name : "");		
		} else 
			cap->capabilities[VOL_CAPABILITIES_INTERFACES] |= VOL_CAP_INT_REMOTE_EVENT;			
		
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
			VOL_CAP_INT_REMOTE_EVENT |
			0;
		
		cap->valid[VOL_CAPABILITIES_RESERVED1] = 0;
		cap->valid[VOL_CAPABILITIES_RESERVED2] = 0;
		VFSATTR_SET_SUPPORTED(fsap, f_capabilities);
	}
	
	/* 
	 * NOTE:  the valid field indicates whether your VFS knows whether a 
	 * attribute is supported or not. So, if you know FOR SURE that a capability 
	 * is support or not, then set that bit in the valid part.  
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
		if (share->ss_fstype != SMB_FS_FAT) {
			fsap->f_attributes.nativeattr.commonattr |= ATTR_CMN_CHGTIME;
		}
		/* Named Streams knows about Finder Info */
		if (share->ss_attributes & FILE_NAMED_STREAMS) {
			fsap->f_attributes.nativeattr.commonattr |= ATTR_CMN_FNDRINFO;
		}

		if (share->ss_attributes & FILE_PERSISTENT_ACLS) {
			fsap->f_attributes.nativeattr.commonattr |= ATTR_CMN_EXTENDED_SECURITY | ATTR_CMN_UUID | ATTR_CMN_GRPUUID;
		}
		
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
		if (share->ss_attributes & FILE_NAMED_STREAMS)
			fsap->f_attributes.nativeattr.fileattr |= ATTR_FILE_TOTALSIZE | 
													ATTR_FILE_ALLOCSIZE |
													ATTR_FILE_RSRCLENGTH | 
													ATTR_FILE_RSRCALLOCSIZE;

		fsap->f_attributes.nativeattr.forkattr = 0;
		VFSATTR_SET_SUPPORTED(fsap, f_attributes);
	}
	/* 
	 * Our filesystem doesn't support volume dates. Let the VFS layer handle
	 * these if requested. 
	 */
	 
	 /* 
	  * Could be one of the following:
	  *		SMB_FS_FAT, SMB_FS_CDFS, SMB_FS_UDF, 
	  *		SMB_FS_NTFS_UNKNOWN, SMB_FS_NTFS, SMB_FS_NTFS_UNIX,
	  *		SMB_FS_MAC_OS_X 
	  */
	VFSATTR_RETURN(fsap, f_fssubtype, share->ss_fstype);
		
	if (VFSATTR_IS_ACTIVE(fsap, f_vol_name) && fsap->f_vol_name) {
		if (smp->sm_args.volume_name) {
			strlcpy(fsap->f_vol_name, smp->sm_args.volume_name, MAXPATHLEN);
		} else {
			/*
			 * ref 3984574.  Returning null here keeps vfs from returning
			 * f_mntonname, and causes CarbonCore (File Mgr) to use the
			 * f_mntfromname, as it did (& still does) when an error is returned.
			 */			
			*fsap->f_vol_name = '\0';			
		}
		VFSATTR_SET_SUPPORTED(fsap, f_vol_name);
	}
	
	/* Let the vfs layer handle f_signature */
	/* We never set f_carbon_fsid, see <rdar://problem/4470282> depricated */

	smb_share_rele(share, context);
	return (error);
}

struct smbfs_sync_cargs {
	vfs_context_t	context;
	int	waitfor;
	int	error;
};


static int
smbfs_sync_callback(vnode_t vp, void *args)
{
	int error;
	struct smbfs_sync_cargs *cargs;
	struct smbnode *np = NULL;
	struct smb_share *share = NULL;

	cargs = (struct smbfs_sync_cargs *)args;

	if (smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK) != 0) {
		return (VNODE_RETURNED);
	}

	np = VTOSMB(vp);
	np->n_lastvop = smbfs_sync_callback;

	share = smb_get_share_with_reference(VTOSMBFS(vp));	
	/* 
	 * Must have gone into reconnect mode while interating the vnodes. Nothing for
	 * us to do until reconnect is done. Just get out and wait for the next time.
	 */
	if (share->ss_flags & SMBS_RECONNECTING) {
		goto done;
	}

	/* 
	 * We need to clear the ACL cache. We only want to hold on to it for a very 
	 * short period, so any chance we get remove the cache. ACL cache data can 
	 * get very large so only hold on to it for a short period of time. Don't 
	 * clear negative acl cache it doesn't cost much, so its ok to hold on to 
	 * for longer periods of time.
	 */
	if ((np->acl_error == 0) && (!vnode_isnamedstream(vp)))
		smbfs_clear_acl_cache(np);

	if (vnode_isreg(vp)) {
		/* 
		 * See if the file needs to be reopened. Ignore the error if being 
		 * revoke it will get caught below 
		 */
		(void)smbfs_smb_reopen_file(share, np, cargs->context);

		lck_mtx_lock(&np->f_openStateLock);
		if (np->f_openState == kNeedRevoke) {
			lck_mtx_unlock(&np->f_openStateLock);
			SMBWARNING("revoking %s\n", np->n_name);
			smbnode_unlock(np);
			np = NULL; /* Already unlocked */
			vn_revoke(vp, REVOKEALL, cargs->context);
			goto done;
		}
		lck_mtx_unlock(&np->f_openStateLock);
	}
	/*
	 * We have dirty data or we have a set eof pending in either case
	 * deal with it in smbfs_fsync.
	 */
	if (vnode_hasdirtyblks(vp) || 
		(vnode_isreg(vp) && (np->n_flag & (NNEEDS_EOF_SET | NNEEDS_FLUSH)))) {
		error = smbfs_fsync(share, vp, cargs->waitfor, 0, cargs->context);
		if (error)
			cargs->error = error;
	}
	
	/* Someone is monitoring this node see if we have any work */
	if (vnode_ismonitored(vp)) {
		int updateNotifyNode = FALSE;
		
		if (vnode_isdir(vp) && !(np->n_flag & N_POLLNOTIFY)) {
			/* 
			 * The smbfs_restart_change_notify will now handle not only reopening 
			 * of notifcation, but also the closing of notifications. This is 
			 * done to force items into polling when we have too many items.
			 */
			smbfs_restart_change_notify(share, np, cargs->context);
			updateNotifyNode = np->d_needsUpdate;			
		} else
			updateNotifyNode = TRUE;
		/* Looks like something change udate the notify routines and our cache */ 
		if (updateNotifyNode)
			(void)smbfs_update_cache(share, vp, NULL, cargs->context);
	}
done:
	if (np) {
		smbnode_unlock(np);
	}
	/* We only have a share if we took a reference, release it */
	if (share) {
		smb_share_rele(share, cargs->context);
	}
	return (VNODE_RETURNED);
}

/*
 * Flush out the buffer cache
 */
static int
smbfs_sync(struct mount *mp, int waitfor, vfs_context_t context)
{
	struct smbfs_sync_cargs args;
	
	args.context = context;
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
static int
smbfs_vget(struct mount *mp, ino64_t ino, vnode_t *vpp,
	   vfs_context_t context)
{
#pragma unused(mp, ino, vpp, context)
	return (ENOTSUP);
}

static int
smbfs_fhtovp(struct mount *mp, int fhlen, unsigned char *fhp, vnode_t *vpp,
	     vfs_context_t context)
{
#pragma unused(mp, fhlen, fhp, vpp, context)
	return (EINVAL);
}

/*
 * Vnode pointer to File handle, should never happen either
 */
static int
smbfs_vptofh(vnode_t vp, int *fhlen, unsigned char *fhp, vfs_context_t context)
{
#pragma unused(vp, fhlen, fhp, context)
	return (EINVAL);
}

/*
 * smbfs_sysctl handles the VFS_CTL_QUERY request which tells interested
 * parties if the connection with the remote server is up or down.
 */
static int
smbfs_sysctl(int * name, unsigned namelen, user_addr_t oldp, size_t * oldlenp,
	     user_addr_t newp, size_t newlen, vfs_context_t context)
{
#pragma unused(oldlenp, newp, newlen)
	int error;
	struct sysctl_req *req;
	struct mount *mp = NULL;
	struct smbmount *smp = NULL;
	struct vfsquery vq;
	int32_t dev = 0;
	struct smb_remount_info info;
	struct smb_share *share;
	
	/*
	 * All names at this level are terminal.
	 */
	if (namelen > 1)
		return (ENOTDIR);	/* overloaded */

	switch (name[0]) {
		case VFS_CTL_STATFS:
		case VFS_CTL_UMOUNT:
		case VFS_CTL_NEWADDR:
		case VFS_CTL_TIMEO:
		case VFS_CTL_NOLOCKS:
			/* Force the VFS layer to handle these */
			return ENOTSUP;
			break;
		case SMBFS_SYSCTL_GET_SERVER_SHARE:
		case SMBFS_SYSCTL_REMOUNT_INFO:
		case SMBFS_SYSCTL_REMOUNT:
		case VFS_CTL_QUERY:
		case VFS_CTL_SADDR:
        case VFS_CTL_DISC:
		{
			boolean_t is_64_bit = vfs_context_is64bit(context);
			union union_vfsidctl vc;
			
			req = CAST_DOWN(struct sysctl_req *, oldp);
			error = SYSCTL_IN(req, &vc, is_64_bit ? sizeof(vc.vc64) : sizeof(vc.vc32));
			if (error) {
				break;
			}
			mp = vfs_getvfs(&vc.vc32.vc_fsid); /* works for 32 and 64 */
			/*
			 * The sysctl_vfs_ctlbyfsid grabs a reference on the mount before 
			 * calling us, so we know the mount point can't go away while we 
			 * are working on it here. Just to be safe we make sure it can be
			 * found by vfs_getvfs. Also if its being forced unmounted there
			 * is nothing for us to do here just get out.
			 */
			if (mp && !(vfs_isforce(mp))) {
				smp = VFSTOSMBFS(mp);
			}
			if (!smp) {
				error = ENOENT;
				break;
			}
			req->newidx = 0;
			if (is_64_bit) {
				req->newptr = vc.vc64.vc_ptr;
				req->newlen = (size_t)vc.vc64.vc_len;
			} else {
				req->newptr = CAST_USER_ADDR_T(vc.vc32.vc_ptr);
				req->newlen = vc.vc32.vc_len;
			}			
			break;
		}
		default:
			error = ENOTSUP;
			break;
	}
	if (error) {
		goto done;
	}
	
	/* We only support new style vfs sysctl. */
	switch (name[0]) {
		case SMBFS_SYSCTL_GET_SERVER_SHARE:
		{
			size_t len;
			char *serverShareStr;
			
			share = smb_get_share_with_reference(smp);
			len = strnlen(SSTOVC(share)->vc_srvname, SMB_MAX_DNS_SRVNAMELEN);
			len += 1; /* Slash */
			len += strnlen(share->ss_name, SMB_MAXSHARENAMELEN);
			len += 1; /* null byte */
			SMB_MALLOC(serverShareStr, char *, len, M_TEMP, M_WAITOK | M_ZERO);
			strlcpy(serverShareStr, SSTOVC(share)->vc_srvname, len);
			strlcat(serverShareStr, "/", len);
			strlcat(serverShareStr, share->ss_name, len);
			smb_share_rele(share, context);
			error = SYSCTL_OUT(req, serverShareStr, len);
			SMB_FREE(serverShareStr, M_TEMP);
			break;
		}
		case SMBFS_SYSCTL_REMOUNT_INFO:
			share = smb_get_share_with_reference(smp);
			smbfs_remountInfo(mp, share, &info);
			smb_share_rele(share, context);
			error = SYSCTL_OUT(req, &info, sizeof(info));
			break;
		case SMBFS_SYSCTL_REMOUNT:
			error = SYSCTL_IN(req, &dev, sizeof(dev));
			if (!error) {
				error = smbfs_remount(dev, mp, smp, context);
			}
			break;
		case VFS_CTL_QUERY:
			bzero(&vq, sizeof(vq));
			if (smp && (smp->sm_status & SM_STATUS_DEAD)) {
				vq.vq_flags |= VQ_DEAD;
			} else if (smp) {
				int dontNotify = ((smp->sm_args.altflags & SMBFS_MNT_SOFT) && 
								(vfs_flags(smp->sm_mp) & MNT_DONTBROWSE));
				
				if ((smp->sm_status & SM_STATUS_DOWN) && !dontNotify) {
						vq.vq_flags |= VQ_NOTRESP;
				}
				if (smp->sm_status & SM_STATUS_REMOUNT) {
					vq.vq_flags |= VQ_ASSIST;
				} else if (smp->sm_status & SM_STATUS_UPDATED) {
					vq.vq_flags |= VQ_UPDATE;
					/* report back only once */
					smp->sm_status &= ~SM_STATUS_UPDATED;
				}
			}
			SMBDEBUG("vq.vq_flags = 0x%x\n", vq.vq_flags);
			error = SYSCTL_OUT(req, &vq, sizeof(vq));
			break;
            
		case VFS_CTL_SADDR:
			if (smp->sm_args.altflags & SMBFS_MNT_DFS_SHARE) {
				/* Never let them unmount a dfs share */
				error = ENOTSUP;
			} 
            else {
				struct sockaddr_storage storage;
				struct sockaddr *saddr;
				size_t len;
				
				memset(&storage, 0, sizeof(storage));

				/* Get a reference on the share */
				share = smb_get_share_with_reference(smp);
			
				if (SSTOVC(share)->vc_saddr->sa_family == AF_NETBIOS) {
					/* NetBIOS sockaddr get the real IPv4 sockaddr */
					saddr = (struct sockaddr *) 
                        &((struct sockaddr_nb *) SSTOVC(share)->vc_saddr)->snb_addrin;
				} 
                else {
					/* IPv4 or IPv6 sockaddr */
					saddr = SSTOVC(share)->vc_saddr;
				}
                
				/* Just to be safe, make sure we have a safe length */
				len = (saddr->sa_len > sizeof(storage)) ? sizeof(storage) : saddr->sa_len;
				memcpy(&storage, saddr, len);
				smb_share_rele(share, context);
				error = SYSCTL_OUT(req, &storage, len);
			}
			break;
            
        case VFS_CTL_DISC:
			if (smp->sm_args.altflags & SMBFS_MNT_DFS_SHARE) {
				/* Never let them unmount a dfs share */
				error = ENOTSUP;
			} 
            else {
                /* 
                 * Server is not responding. KEA will now request an unmount.
                 * If there are no files opened for write AND there are no files
                 * sitting in UBC with dirty data, then return 0 and signal KEA 
                 * to unmount us right now.  KEA will not display this share in
                 * the dialog since we should be unmounting immediately.
                 * Otherwise, return EBUSY and let the dialog be displayed so 
                 * the user can decide what to do
                 */
                error = 0;  /* assume can be immediately unmounted */

				/* Get a reference on the share */
				share = smb_get_share_with_reference(smp);
                
                if (!vfs_isrdonly(mp)) {
                    /* only check for "busy" files if not read only */
                    lck_mtx_lock(&share->ss_shlock);
                    
                    error = smbfs_IObusy(smp);
                    SMBDEBUG("VFS_CTL_DISC - smbfs_IObusy returned %d\n", error);
                   
                    lck_mtx_unlock(&share->ss_shlock);	
                }
                if (error != EBUSY) {
                    SMBDEBUG("VFS_CTL_DISC unmounting\n");
                    /* ok to immediately be unmounted */
					share->ss_dead(share);
                }
                
				smb_share_rele(share, context);
            }
            
            break;
	    default:
			error = ENOTSUP;
			break;
	}
done:
	if (error) {
		SMBWARNING("name[0] = %d error = %d\n", name[0], error);
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
	smbfs_sysctl,
	NULL,
	{0}
};

int smbfs_module_start(kmod_info_t *ki, void *data)
{
#pragma unused(data)
	struct vfs_fsentry vfe;
	int	error;

	smbfs_kmod_infop = ki;

	vfe.vfe_vfsops = &smbfs_vfsops;
	/* We just have vnode operations for regular files and directories */
	vfe.vfe_vopcnt = 1; 
	vfe.vfe_opvdescs = smbfs_vnodeop_opv_desc_list;
	strlcpy(vfe.vfe_fsname, smbfs_name, sizeof(vfe.vfe_fsname));
	vfe.vfe_flags = VFS_TBLTHREADSAFE | VFS_TBLFSNODELOCK | VFS_TBLNOTYPENUM | 
					VFS_TBL64BITREADY | VFS_TBLREADDIR_EXTENDED | 
					VFS_TBLUNMOUNT_PREFLIGHT;

	vfe.vfe_reserv[0] = 0;
	vfe.vfe_reserv[1] = 0;

	error = vfs_fsadd(&vfe, &smbfs_vfsconf);
	if (error)
		goto out;

	smbnet_lock_init();	/* Initialize the network locks */
	
	/* This just calls nsmb_dev_load */
	SEND_EVENT(dev_netsmb, MOD_LOAD);

	sysctl_register_oid(&sysctl__net_smb);
	sysctl_register_oid(&sysctl__net_smb_fs);
	
	sysctl_register_oid(&sysctl__net_smb_fs_version);
	sysctl_register_oid(&sysctl__net_smb_fs_loglevel);

	sysctl_register_oid(&sysctl__net_smb_fs_kern_deadtimer);
	sysctl_register_oid(&sysctl__net_smb_fs_kern_hard_deadtimer);
	sysctl_register_oid(&sysctl__net_smb_fs_kern_soft_deadtimer);

	sysctl_register_oid(&sysctl__net_smb_fs_tcpsndbuf);
	sysctl_register_oid(&sysctl__net_smb_fs_tcprcvbuf);
	
	smbfs_install_sleep_wake_notifier();

out:
	return (error ? KERN_FAILURE : KERN_SUCCESS);
}


int smbfs_module_stop(kmod_info_t *ki, void *data)
{
#pragma unused(ki)
#pragma unused(data)
	int error;

	/* 
	 * The dev_rw_lck lock is global value and protects the dev_open_cnt, 
	 * unloadInProgress flag, the device opens, closes, loads and unloads. All device
	 * opens and close are serialize, so we only have one happening at any time.
	 */
	lck_rw_lock_exclusive(dev_rw_lck);
	unloadInProgress = TRUE;
	/* We are still in use, don't unload. */
	if (mount_cnt || dev_open_cnt) {
		SMBWARNING("Still in use, we have %d volumes mounted and %d devices opened\n", 
				   mount_cnt, dev_open_cnt);
		unloadInProgress = FALSE;
		lck_rw_unlock_exclusive(dev_rw_lck);
		return KERN_NO_ACCESS;
	}
	lck_rw_unlock_exclusive(dev_rw_lck);
	error = vfs_fsremove(smbfs_vfsconf);
	if (error) {
		/* Should never happen */
		SMBERROR("vfs_fsremove failed with %d, may want to reboot!\n", error);
		goto out;
	}

	sysctl_unregister_oid(&sysctl__net_smb_fs_tcpsndbuf);
	sysctl_unregister_oid(&sysctl__net_smb_fs_tcprcvbuf);
	
	sysctl_unregister_oid(&sysctl__net_smb_fs_kern_deadtimer);
	sysctl_unregister_oid(&sysctl__net_smb_fs_kern_hard_deadtimer);
	sysctl_unregister_oid(&sysctl__net_smb_fs_kern_soft_deadtimer);
		
	sysctl_unregister_oid(&sysctl__net_smb_fs_version);
	sysctl_unregister_oid(&sysctl__net_smb_fs_loglevel);
	
	sysctl_unregister_oid(&sysctl__net_smb_fs);
	sysctl_unregister_oid(&sysctl__net_smb);

	/* This just calls nsmb_dev_load */
	SEND_EVENT(dev_netsmb, MOD_UNLOAD);

	smbfs_remove_sleep_wake_notifier();

	lck_rw_free(dev_rw_lck, dev_lck_grp);
	smbfs_lock_uninit();	/* Free up the file system locks */
	smbnet_lock_uninit();	/* Free up the network locks */
	
out:	
	return (error ? KERN_FAILURE : KERN_SUCCESS);
}
