/*
 * Copyright (c) 2000-2001, Boris Popov
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
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <libkern/OSAtomic.h>
#include <sys/vnode_if.h>
#include <string.h>

#include <sys/kauth.h>

#include <sys/syslog.h>
#include <sys/smb_apple.h>
#include <sys/mchain.h>

#include <netsmb/smb.h>
#include <netsmb/smb_2.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_dev.h>
#include <netsmb/smb_sleephandler.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_rq_2.h>
#include <netsmb/smb_read_write.h>
#include <netsmb/smb_conn_2.h>
#include <netsmb/smb2_mc_support.h>

#include <smbfs/smbfs.h>
#include <smbfs/smbfs_lockf.h>
#include <smbfs/smbfs_node.h>
#include <smbfs/smbfs_subr.h>
#include <smbfs/smbfs_subr_2.h>
#include <smbclient/smbclient_internal.h>
#include "smbfs_security.h"
#include <Triggers/triggers.h>

#include <sys/buf.h>

int smbfs_module_start(kmod_info_t *ki, void *data);
int smbfs_module_stop(kmod_info_t *ki, void *data);
static int smbfs_root(struct mount *, vnode_t *, vfs_context_t);

#define	SMB_FSYNC_TIMO 30

#ifdef SMB_DEBUG
__attribute__((visibility("hidden"))) int smbfs_loglevel = SMB_LOW_LOG_LEVEL;
#else // SMB_DEBUG
__attribute__((visibility("hidden"))) int smbfs_loglevel = SMB_NO_LOG_LEVEL;
#endif // SMB_DEBUG

__attribute__((visibility("hidden"))) uint32_t smbfs_deadtimer = DEAD_TIMEOUT;
__attribute__((visibility("hidden"))) uint32_t smbfs_hard_deadtimer = HARD_DEAD_TIMER;
__attribute__((visibility("hidden"))) uint32_t smbfs_trigger_deadtimer = TRIGGER_DEAD_TIMEOUT;

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

lck_grp_attr_t *session_credits_grp_attr;
lck_grp_t *session_credits_lck_group;
lck_attr_t *session_credits_lck_attr;

lck_grp_attr_t *session_st_grp_attr;
lck_grp_t *session_st_lck_group;
lck_attr_t *session_st_lck_attr;

lck_grp_attr_t *ssst_grp_attr;
lck_grp_t *ssst_lck_group;
lck_attr_t *ssst_lck_attr;

lck_grp_attr_t *fid_lck_grp_attr;
lck_grp_t *fid_lck_grp;
lck_attr_t *fid_lck_attr;

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

lck_grp_attr_t *iodtdata_grp_attr;
lck_grp_t *iodtdata_lck_group;
lck_attr_t *iodtdata_lck_attr;

lck_grp_attr_t *dev_lck_grp_attr;
lck_grp_t *dev_lck_grp;
lck_attr_t *dev_lck_attr;
lck_rw_t *dev_rw_lck;

lck_grp_attr_t *hash_lck_grp_attr;
lck_grp_t *hash_lck_grp;
lck_attr_t *hash_lck_attr;

lck_grp_attr_t *mc_notifier_lck_grp_attr;
lck_grp_t *mc_notifier_lck_grp;
lck_attr_t *mc_notifier_lck_attr;
lck_mtx_t mc_notifier_lck;

struct smbmnt_carg {
	vfs_context_t context;
	struct mount *mp;
	int found;
};

/*
 * smb_maxsegreadsize/smb_maxsegwritesize is max size that UBC will call
 * our vnop_strategy with.
 */
uint32_t smb_maxsegreadsize = 1024 * 1024 * 16;
uint32_t smb_maxsegwritesize = 1024 * 1024 * 16;

SYSCTL_DECL(_net_smb);
SYSCTL_NODE(_net_smb, OID_AUTO, fs, CTLFLAG_RW, 0, "SMB/CIFS file system");
SYSCTL_INT(_net_smb_fs, OID_AUTO, version, CTLFLAG_RD, &smbfs_version, 0, "");
SYSCTL_INT(_net_smb_fs, OID_AUTO, loglevel, CTLFLAG_RW,  &smbfs_loglevel, 0, "");
SYSCTL_INT(_net_smb_fs, OID_AUTO, kern_deadtimer, CTLFLAG_RW, &smbfs_deadtimer, DEAD_TIMEOUT, "");
SYSCTL_INT(_net_smb_fs, OID_AUTO, kern_hard_deadtimer, CTLFLAG_RW, &smbfs_hard_deadtimer, HARD_DEAD_TIMER, "");
SYSCTL_INT(_net_smb_fs, OID_AUTO, kern_soft_deadtimer, CTLFLAG_RW, &smbfs_trigger_deadtimer, TRIGGER_DEAD_TIMEOUT, "");
SYSCTL_INT(_net_smb_fs, OID_AUTO, maxsegreadsize, CTLFLAG_RW, &smb_maxsegreadsize, 0, "");
SYSCTL_INT(_net_smb_fs, OID_AUTO, maxsegwritesize, CTLFLAG_RW, &smb_maxsegwritesize, 0, "");


extern struct sysctl_oid sysctl__net_smb;
extern struct sysctl_oid sysctl__net_smb_fs_version;
extern struct sysctl_oid sysctl__net_smb_fs_loglevel;
extern struct sysctl_oid sysctl__net_smb_fs_kern_deadtimer;
extern struct sysctl_oid sysctl__net_smb_fs_kern_hard_deadtimer;
extern struct sysctl_oid sysctl__net_smb_fs_kern_soft_deadtimer;
extern struct sysctl_oid sysctl__net_smb_fs_tcpsndbuf;
extern struct sysctl_oid sysctl__net_smb_fs_tcprcvbuf;
extern struct sysctl_oid sysctl__net_smb_fs_maxwrite;
extern struct sysctl_oid sysctl__net_smb_fs_maxread;
extern struct sysctl_oid sysctl__net_smb_fs_maxsegreadsize;
extern struct sysctl_oid sysctl__net_smb_fs_maxsegwritesize;


MALLOC_DEFINE(M_SMBFSHASH, "SMBFS hash", "SMBFS hash table");

#ifndef VFS_CTL_DISC
#define VFS_CTL_DISC	0x00010008	/* Server is disconnected */
#endif

/* Global dir enumeration caching */
extern lck_mtx_t global_dir_cache_lock; /* global_dir_cache_entry lock */
extern struct global_dir_cache_entry *global_dir_cache_head;
extern uint64_t g_hardware_memory_size;
extern uint32_t g_max_dirs_cached;
extern uint32_t g_max_dir_entries_cached;

int g_registered_for_low_memory = 0;

extern lck_mtx_t global_Lease_hash_lock;
extern u_long g_lease_hash_len;
extern struct g_lease_hash_head *g_lease_hash;

extern pid_t mc_notifier_pid;

static void
smbfs_lock_init(void)
{
	int error = 0;
	
	hash_lck_attr = lck_attr_alloc_init();
	hash_lck_grp_attr = lck_grp_attr_alloc_init();
	hash_lck_grp = lck_grp_alloc_init("smb-hash", hash_lck_grp_attr);
	
	smbfs_lock_attr    = lck_attr_alloc_init();
	smbfs_group_attr   = lck_grp_attr_alloc_init();
	smbfs_mutex_group  = lck_grp_alloc_init("smb-mutex", smbfs_group_attr);
	smbfs_rwlock_group = lck_grp_alloc_init("smbfs-rwlock", smbfs_group_attr);
	
	/* Init global dir enum cache mutex */
	lck_mtx_init(&global_dir_cache_lock, smbfs_mutex_group, smbfs_lock_attr);
	global_dir_cache_head = NULL;

	/* Register for low memory callback */
	error = fs_buffer_cache_gc_register(smb_global_dir_cache_low_memory, NULL);
	if (!error) {
		g_registered_for_low_memory = 1;
	}
	else {
		SMBERROR("fs_buffer_cache_gc_register failed <%d> \n", error);
	}
	
	/* Set up global lease table */
	lck_mtx_init(&global_Lease_hash_lock, smbfs_mutex_group, smbfs_lock_attr);
	g_lease_hash = hashinit(desiredvnodes, M_SMBFSHASH, &g_lease_hash_len);
	if (g_lease_hash == NULL) {
		/* Should never fail */
		SMBERROR("lease table hashinit failed \n");
	}
    
    /* Set up mutexes for buf_map */
    smbfs_init_buf_map();
}

static void 
smbfs_lock_uninit(void)
{
    /* Free mutexes for buf_map */
    smbfs_teardown_buf_map();
    
	if (g_registered_for_low_memory == 1) {
		/* Unregister for low memory callback */
		fs_buffer_cache_gc_unregister(smb_global_dir_cache_low_memory, NULL);
	}

    /* Remove all dirs from the cache */
	smb_global_dir_cache_remove(0, 1);

	/* Free global dir enum cache mutex */
	lck_mtx_destroy(&global_dir_cache_lock, smbfs_mutex_group);
	global_dir_cache_head = NULL;

	/* Free global lease hash table */
	if (g_lease_hash) {
        hashdestroy(g_lease_hash, M_SMBFSHASH, g_lease_hash_len);
		g_lease_hash = (void *)0xDEAD5AB0;
	}
	lck_mtx_destroy(&global_Lease_hash_lock, smbfs_mutex_group);

	lck_grp_free(smbfs_mutex_group);
	lck_grp_free(smbfs_rwlock_group);
	lck_grp_attr_free(smbfs_group_attr);
	lck_attr_free(smbfs_lock_attr);

	lck_grp_free(hash_lck_grp);
	lck_grp_attr_free(hash_lck_grp_attr);
	lck_attr_free(hash_lck_attr);
}

static void 
smbnet_lock_init(void)
{
	co_lck_attr = lck_attr_alloc_init();
	co_grp_attr = lck_grp_attr_alloc_init();
	co_lck_group = lck_grp_alloc_init("smb-co", co_grp_attr);
	
	session_credits_lck_attr = lck_attr_alloc_init();
	session_credits_grp_attr = lck_grp_attr_alloc_init();
	session_credits_lck_group = lck_grp_alloc_init("smb-session_credits", session_credits_grp_attr);

	session_st_lck_attr = lck_attr_alloc_init();
	session_st_grp_attr = lck_grp_attr_alloc_init();
	session_st_lck_group = lck_grp_alloc_init("smb-sessionst", session_st_grp_attr);
	
	ssst_lck_attr = lck_attr_alloc_init();
	ssst_grp_attr = lck_grp_attr_alloc_init();
	ssst_lck_group = lck_grp_alloc_init("smb-ssst", ssst_grp_attr);
	
	fid_lck_attr = lck_attr_alloc_init();
	fid_lck_grp_attr = lck_grp_attr_alloc_init();
	fid_lck_grp = lck_grp_alloc_init("smb-fid", fid_lck_grp_attr);
    
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
	
    iodtdata_lck_attr = lck_attr_alloc_init();
    iodtdata_grp_attr = lck_grp_attr_alloc_init();
    iodtdata_lck_group = lck_grp_alloc_init("smb-iodtdata", iodtdata_grp_attr);
	
	dev_lck_attr = lck_attr_alloc_init();
	dev_lck_grp_attr = lck_grp_attr_alloc_init();
	dev_lck_grp = lck_grp_alloc_init("smb-dev", dev_lck_grp_attr);

	dev_rw_lck = lck_rw_alloc_init(dev_lck_grp, dev_lck_attr);

    mc_notifier_lck_attr = lck_attr_alloc_init();
    mc_notifier_lck_grp_attr = lck_grp_attr_alloc_init();
    mc_notifier_lck_grp = lck_grp_alloc_init("smb-mc-notifier", mc_notifier_lck_grp_attr);
    lck_mtx_init(&mc_notifier_lck, mc_notifier_lck_grp, mc_notifier_lck_attr);
}

static void 
smbnet_lock_uninit(void)
{
    lck_rw_free(dev_rw_lck, dev_lck_grp);

    lck_grp_free(dev_lck_grp);
	lck_grp_attr_free(dev_lck_grp_attr);
	lck_attr_free(dev_lck_attr);
	
	lck_grp_free(iodtdata_lck_group);
	lck_grp_attr_free(iodtdata_grp_attr);
	lck_attr_free(iodtdata_lck_attr);
	
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
	
	lck_grp_free(fid_lck_grp);
	lck_grp_attr_free(fid_lck_grp_attr);
	lck_attr_free(fid_lck_attr);

	lck_grp_free(session_credits_lck_group);
	lck_grp_attr_free(session_credits_grp_attr);
	lck_attr_free(session_credits_lck_attr);

	lck_grp_free(session_st_lck_group);
	lck_grp_attr_free(session_st_grp_attr);
	lck_attr_free(session_st_lck_attr);
    
	lck_grp_free(co_lck_group);
	lck_grp_attr_free(co_grp_attr);
	lck_attr_free(co_lck_attr);	
}

/*
 * Need to check and make sure the server is in the same domain, if not
 * then we need to turn off ACL support.
 */
static void 
isServerInSameDomain(struct smb_share *share, struct smbmount *smp)
{
	/* Just to be safe */
    if (smp->ntwrk_sids) {
        SMB_FREE_DATA(smp->ntwrk_sids, smp->ntwrk_sids_allocsize);
    }

	smp->ntwrk_sids_cnt = 0;
	
	if (smp->sm_args.altflags & SMBFS_MNT_TIME_MACHINE) {
		SMBWARNING("TM mount so disabling ACLs on <%s> \n",
				   vfs_statfs(smp->sm_mp)->f_mntfromname);
		share->ss_attributes &= ~FILE_PERSISTENT_ACLS;
		return;
	}
	
	if (SS_TO_SESSION(share)->session_flags & SMBV_NETWORK_SID) {
		/* See if the session network SID is known by Directory Service */
		if ((smp->sm_args.altflags & SMBFS_MNT_DEBUG_ACL_ON) ||
			(smbfs_is_sid_known(&SS_TO_SESSION(share)->session_ntwrk_sid))) {
            SMB_MALLOC_DATA(smp->ntwrk_sids, sizeof(ntsid_t), Z_WAITOK);
			memcpy(smp->ntwrk_sids, &SS_TO_SESSION(share)->session_ntwrk_sid, sizeof(ntsid_t));
			smp->ntwrk_sids_cnt = 1;
            smp->ntwrk_sids_allocsize = sizeof(ntsid_t);
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
    struct smbiod *iod = NULL;

	smp = share->ss_mount;
	/* We have already unmounted or we are being force unmount, we are done */
	if ((smp == NULL) || (vfs_isforce(smp->sm_mp))) {
		return 0;
	}

    if (smb_iod_get_main_iod(SS_TO_SESSION(share), &iod, __FUNCTION__)) { // TBD: Do we need a for loop on all iods?
        SMBERROR("Invalid iod\n");
        return EINVAL;
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
		if ((IPC_PORT_VALID(iod->iod_session->gss_mp)) &&
			(share->ss_fstype != SMB_FS_FAT) && !(UNIX_CAPS(share))) {
			
			/* Call autofs with the fsid to start the remount */
			SMBERROR("Remounting %s/%s\n", SS_TO_SESSION(share)->session_srvname, 
					 share->ss_name);
			if (SMBRemountServer(&(vfs_statfs(smp->sm_mp))->f_fsid, 
								 sizeof(vfs_statfs(smp->sm_mp)->f_fsid),
								 iod->iod_gss.gss_asid)) {
				/* Something went wrong try again next time */
				SMBERROR("Something went wrong with remounting %s/%s\n", 
						 SS_TO_SESSION(share)->session_srvname, share->ss_name);
				smp->sm_status &= ~SM_STATUS_REMOUNT;
			}
		} else {
			SMBWARNING("Skipping remounting %s/%s, file system type mismatch\n", 
					   SS_TO_SESSION(share)->session_srvname, share->ss_name);
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
    smb_iod_rel(iod, NULL, __FUNCTION__);
	return treenct;
}

/*
 * The share needs to be locked before calling this rouitne!
 *
 * smbfs_up is called when we receive a successful response to a message or we have 
 * successfully reconnect. It uses vfs_event_signal() to tell interested parties 
 * the connection is OK again  if the connection was having problems.
 */
static int
smbfs_up(struct smb_share *share, int reconnect)
{
	struct smbmount *smp;
	int error = 0;
	
	smp = share->ss_mount;
    
	/* We have already unmounted or we are being force unmount, we are done */
	if ((smp == NULL) || (vfs_isforce(smp->sm_mp))) {
		return (error);
	}
    
	if (reconnect) {
		error = smbfs_reconnect(smp);
        if (error) {
            return (error);
        }
	}
    
	/*
     * We are done remounting, either we reconnected successfully or the
     * remount worked. Mark that the share is up again.
     */
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
	
	return (error);
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

    if (share->ss_flags & SMBS_GOING_AWAY) {
        /* Once marked as going away, always going away */
        return TRUE;
    }
    
    smp = share->ss_mount;
    
    /* If we have a ss_mount then we have a sm_mp */
    if (smp && (vfs_isforce(smp->sm_mp))) {
        share->ss_flags |= SMBS_GOING_AWAY;
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
    size_t len;

    struct smbiod *iod;
    if (smb_iod_get_main_iod(SS_TO_SESSION(share), &iod, __FUNCTION__)) { // TBD: Do we need a for loop on all iods?
        SMBERROR("Invalid iod\n");
        return EINVAL;
    }

#ifdef SMBDEBUG_REMOUNT
	/* Used for testing only, pretend we are in reconnect. */
	share->ss_flags |= SMBS_RECONNECTING;
#endif // SMBDEBUG_REMOUNT
    
	bzero(info, sizeof(*info));
	info->version = REMOUNT_INFO_VERSION;
	info->mntAuthFlags = SS_TO_SESSION(share)->session_flags & SMBV_USER_LAND_MASK;
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

    len = iod->iod_gss.gss_cpn_len;
    if (len >= sizeof(info->mntClientPrincipalName)) {
        SMBERROR("session_gss.gss_cpn_len too big %zu >= %lu \n",
                 len,
                 sizeof(info->mntClientPrincipalName));
        len = sizeof(info->mntClientPrincipalName) - 1;
    }
    memcpy(info->mntClientPrincipalName,
           (char *)iod->iod_gss.gss_cpn,
           len);
    
	info->mntClientPrincipalNameType  = iod->iod_gss.gss_client_nt;
    smb_iod_rel(iod, NULL, __FUNCTION__);
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
	 * new session until we are done, but we never sleep here so that should be ok.
	 */
	lck_mtx_lock(&new_share->ss_shlock);
	/*
	 * Finally lock the old share, This could block the session reconnect code, but
	 * not much we can do about that here. Remember the mount holds a reference
	 * on the old share and we are under the lock now so we can access it 
	 * directly without any issues.
	 */
	share = smp->sm_share;
	lck_mtx_lock(&share->ss_shlock);
	if (SS_TO_SESSION(new_share)->throttle_info) {
		/* Taking a reference here will release the old reference */ 
		throttle_info_mount_ref(mp, SS_TO_SESSION(new_share)->throttle_info);
	} else if (SS_TO_SESSION(share)->throttle_info) {
		/* 
		 * The new session doesn't have any throttle info, but we have one on the 
		 * old session, release the reference.
		 */
		throttle_info_mount_rel(mp);
	}
	/* Take a volume count, since this share has a mount now */
	(void)OSAddAtomic(1, &SS_TO_SESSION(new_share)->session_volume_cnt);
	/* 
	 * We support allowing information to changes on the session and the share, except
	 * for information obtained from the smbfs_qfsattr, smbfs_unix_qfsattr, 
	 * isServerInSameDomain, and smbfs_unix_whoami routines. Once ACLs are turned
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
	SMBERROR("replacing %s/%s with %s/%s\n",  SS_TO_SESSION(share)->session_srvname, 
			 share->ss_name, SS_TO_SESSION(new_share)->session_srvname, new_share->ss_name);
	/* Now unlock in reverse order */
	lck_mtx_unlock(&share->ss_shlock);
	lck_mtx_unlock(&new_share->ss_shlock);
	lck_rw_unlock_exclusive(&smp->sm_rw_sharelock);
	smb_iod_errorout_share_request(share, ETIMEDOUT);
	/* Remove the old share's volume count, since it no longer has a mount */
	(void)OSAddAtomic(-1, &SS_TO_SESSION(share)->session_volume_cnt);
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
	uint32_t stream_flags = 0;
	uint8_t *uu = NULL;
	id_t uid;
    u_int32_t option = 0, i = 0;
	static const uuid_t _user_compat_prefix = {0xff, 0xff, 0xee, 0xee, 0xdd, 0xdd, 0xcc, 0xcc, 0xbb, 0xbb, 0xaa, 0xaa, 0x00, 0x00, 0x00, 0x00};
    struct vnode_attr vap;
#define COMPAT_PREFIX_LEN       (sizeof(uuid_t) - sizeof(id_t))

    SMB_LOG_KTRACE(SMB_DBG_MOUNT | DBG_FUNC_START, 0, 0, 0, 0, 0);

	if (data == USER_ADDR_NULL) {
		SMBDEBUG("missing data argument\n");
		error = EINVAL;
		goto bad;
	}

    SMB_MALLOC_TYPE(args, struct smb_mount_args, Z_WAITOK_ZERO);
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
	
    SMB_MALLOC_TYPE(smp, struct smbmount, Z_WAITOK_ZERO);
	if (smp == NULL) {
		SMBDEBUG("Couldn't malloc the smb mount structure!");
		error = ENOMEM;
		goto bad;
	}

	smp->sm_mp = mp;
	vfs_setfsprivate(mp, (void *)smp);	
    
    /* alloc hash stuff */
	smp->sm_hash = hashinit(desiredvnodes, M_SMBFSHASH, &smp->sm_hashlen);
	if (smp->sm_hash == NULL)
		goto bad;
	smp->sm_hashlock = lck_mtx_alloc_init(hash_lck_grp, hash_lck_attr);

	lck_rw_init(&smp->sm_rw_sharelock, smbfs_rwlock_group, smbfs_lock_attr);
	lck_mtx_init(&smp->sm_statfslock, smbfs_mutex_group, smbfs_lock_attr);		
    lck_mtx_init(&smp->sm_svrmsg_lock, smbfs_mutex_group, smbfs_lock_attr);

	lck_rw_lock_exclusive(&smp->sm_rw_sharelock);
	smp->sm_share = share;
	lck_rw_unlock_exclusive(&smp->sm_rw_sharelock);
	smp->sm_rvp = NULL;

	/* Save any passed in arguments that we may need */
	smp->sm_args.altflags = args->altflags;
	smp->sm_args.uid = args->uid;
    smp->sm_args.gid = args->gid;
    smp->sm_args.ip_QoS = args->ip_QoS;
    if (smp->sm_args.altflags & SMBFS_MNT_SNAPSHOT) {
        strlcpy(SS_TO_SESSION(share)->snapshot_time, args->snapshot_time,
                sizeof(SS_TO_SESSION(share)->snapshot_time));
        SS_TO_SESSION(share)->snapshot_local_time = args->snapshot_local_time;
    }

	/* Get the user's uuid */
	error = kauth_cred_uid2guid(smp->sm_args.uid, &smp->sm_args.uuid);
	if (error) {
		SMBWARNING("Couldn't get the mounted users UUID, uid = %d error = %d\n",
				   smp->sm_args.uid, error);
		
		/* 
		 * This must be the Recovery Boot partition where there is no user
		 * space resolver running (or running in install mode) so map the uid
		 * ourselves.
		 *
		 * Copied from kauth_cred_cache_lookup()
		 */
		if (sizeof(smp->sm_args.uuid) < sizeof(_user_compat_prefix)) {
			SMBERROR("smp->sm_args.uuid too small %lu < %lu\n",
					 sizeof(smp->sm_args.uuid),
					 sizeof(_user_compat_prefix));
			goto bad;
		}
		
		uu = (uint8_t *) &smp->sm_args.uuid;
		uid = smp->sm_args.uid;

		memcpy(uu, _user_compat_prefix, sizeof(_user_compat_prefix));
		memcpy(&uu[COMPAT_PREFIX_LEN], &uid, sizeof(uid));
	}
	
	smp->sm_args.file_mode = args->file_mode & ACCESSPERMS;
	smp->sm_args.dir_mode  = args->dir_mode & ACCESSPERMS;
	if (args->volume_name[0]) {
		smp->sm_args.volume_name = smb_strndup(args->volume_name, 
											   sizeof(args->volume_name), &smp->sm_args.volume_name_allocsize);
	} else {
		smp->sm_args.volume_name = NULL;
	}
	
	/* Copy in dir caching values */
    smp->sm_args.dir_cache_async_cnt = args->dir_cache_async_cnt;
    smp->sm_args.dir_cache_max = args->dir_cache_max;
    smp->sm_args.dir_cache_min = args->dir_cache_min;
    smp->sm_args.max_dirs_cached = args->max_dirs_cached;
    smp->sm_args.max_dir_entries_cached = args->max_dir_entries_cached;

    SMB_LOG_DIR_CACHE("async_cnt %d, cache_max %d, cache_min %d \n",
                      smp->sm_args.dir_cache_async_cnt,
                      smp->sm_args.dir_cache_max,
                      smp->sm_args.dir_cache_min);

    if ((smp->sm_args.max_dirs_cached != 0) &&
        (smp->sm_args.max_dirs_cached < 5000)) {
        /* Keep max in sync with preferences.c value */
        SMBWARNING("%s using custom max dirs cached of %d \n",
                   vfs_statfs(mp)->f_mntfromname, smp->sm_args.max_dirs_cached);
        g_max_dirs_cached = smp->sm_args.max_dirs_cached;
    }

    if ((smp->sm_args.max_dir_entries_cached != 0) &&
        (smp->sm_args.max_dir_entries_cached < 500000)) {
        /* Keep max in sync with preferences.c value */
        SMBWARNING("%s using custom max entries cached per dir of %d \n",
                   vfs_statfs(mp)->f_mntfromname, smp->sm_args.max_dir_entries_cached);
        g_max_dir_entries_cached = smp->sm_args.max_dir_entries_cached;
    }

    SMB_LOG_DIR_CACHE("max_dirs_cached %d, max_dir_entries_cached %d \n",
                      g_max_dirs_cached,
                      g_max_dir_entries_cached);

    
    /*
     * Set user defined read quantum sizes and counts?
     */
    smp->sm_args.read_size[0] = args->read_size[0];
    if (smp->sm_args.read_size[0] != 0) {
        SMBWARNING("%s using custom min read size of %d \n",
                   vfs_statfs(mp)->f_mntfromname, smp->sm_args.read_size[0]);
        SS_TO_SESSION(share)->iod_readSizes[0] = smp->sm_args.read_size[0];

        /* Recheck max values */
        SS_TO_SESSION(share)->session_rxmax = smb2_session_maxread(SS_TO_SESSION(share),
                                                                   kDefaultMaxIOSize);
    }

    smp->sm_args.read_size[1] = args->read_size[1];
    if (smp->sm_args.read_size[1] != 0) {
        SMBWARNING("%s using custom med read size of %d \n",
                   vfs_statfs(mp)->f_mntfromname, smp->sm_args.read_size[1]);
        SS_TO_SESSION(share)->iod_readSizes[1] = smp->sm_args.read_size[1];

        /* Recheck max values */
        SS_TO_SESSION(share)->session_rxmax = smb2_session_maxread(SS_TO_SESSION(share),
                                                                   kDefaultMaxIOSize);
    }
    
    smp->sm_args.read_size[2] = args->read_size[2];
    if (smp->sm_args.read_size[2] != 0) {
        SMBWARNING("%s using custom max read size of %d \n",
                   vfs_statfs(mp)->f_mntfromname, smp->sm_args.read_size[2]);
        SS_TO_SESSION(share)->iod_readSizes[2] = smp->sm_args.read_size[2];

        /* Recheck max values */
        SS_TO_SESSION(share)->session_rxmax = smb2_session_maxread(SS_TO_SESSION(share),
                                                                   kDefaultMaxIOSize);
    }

    smp->sm_args.read_count[0] = args->read_count[0];
    if (smp->sm_args.read_count[0] != 0) {
        SMBWARNING("%s using custom min read count of %d \n",
                   vfs_statfs(mp)->f_mntfromname, smp->sm_args.read_count[0]);
        SS_TO_SESSION(share)->iod_readCounts[0] = smp->sm_args.read_count[0];
    }

    smp->sm_args.read_count[1] = args->read_count[1];
    if (smp->sm_args.read_count[1] != 0) {
        SMBWARNING("%s using custom med read count of %d \n",
                   vfs_statfs(mp)->f_mntfromname, smp->sm_args.read_count[1]);
        SS_TO_SESSION(share)->iod_readCounts[1] = smp->sm_args.read_count[1];
    }
    
    smp->sm_args.read_count[2] = args->read_count[2];
    if (smp->sm_args.read_count[2] != 0) {
        SMBWARNING("%s using custom max read count of %d \n",
                   vfs_statfs(mp)->f_mntfromname, smp->sm_args.read_count[2]);
        SS_TO_SESSION(share)->iod_readCounts[2] = smp->sm_args.read_count[2];
    }

    /*
     * Set user defined write quantum sizes and counts?
     */
    smp->sm_args.write_size[0] = args->write_size[0];
    if (smp->sm_args.write_size[0] != 0) {
        SMBWARNING("%s using custom min write size of %d \n",
                   vfs_statfs(mp)->f_mntfromname, smp->sm_args.write_size[0]);
        SS_TO_SESSION(share)->iod_writeSizes[0] = smp->sm_args.write_size[0];

        /* Recheck max values */
        SS_TO_SESSION(share)->session_rxmax = smb2_session_maxwrite(SS_TO_SESSION(share),
                                                                    kDefaultMaxIOSize);
    }

    smp->sm_args.write_size[1] = args->write_size[1];
    if (smp->sm_args.write_size[1] != 0) {
        SMBWARNING("%s using custom med write size of %d \n",
                   vfs_statfs(mp)->f_mntfromname, smp->sm_args.write_size[1]);
        SS_TO_SESSION(share)->iod_writeSizes[1] = smp->sm_args.write_size[1];

        /* Recheck max values */
        SS_TO_SESSION(share)->session_rxmax = smb2_session_maxwrite(SS_TO_SESSION(share),
                                                                    kDefaultMaxIOSize);
    }
    
    smp->sm_args.write_size[2] = args->write_size[2];
    if (smp->sm_args.write_size[2] != 0) {
        SMBWARNING("%s using custom max write size of %d \n",
                   vfs_statfs(mp)->f_mntfromname, smp->sm_args.write_size[2]);
        SS_TO_SESSION(share)->iod_writeSizes[2] = smp->sm_args.write_size[2];

        /* Recheck max values */
        SS_TO_SESSION(share)->session_rxmax = smb2_session_maxwrite(SS_TO_SESSION(share),
                                                                    kDefaultMaxIOSize);
    }

    smp->sm_args.write_count[0] = args->write_count[0];
    if (smp->sm_args.write_count[0] != 0) {
        SMBWARNING("%s using custom min write count of %d \n",
                   vfs_statfs(mp)->f_mntfromname, smp->sm_args.write_count[0]);
        SS_TO_SESSION(share)->iod_writeCounts[0] = smp->sm_args.write_count[0];
    }

    smp->sm_args.write_count[1] = args->write_count[1];
    if (smp->sm_args.write_count[1] != 0) {
        SMBWARNING("%s using custom med write count of %d \n",
                   vfs_statfs(mp)->f_mntfromname, smp->sm_args.write_count[1]);
        SS_TO_SESSION(share)->iod_writeCounts[1] = smp->sm_args.write_count[1];
    }
    
    smp->sm_args.write_count[2] = args->write_count[2];
    if (smp->sm_args.write_count[2] != 0) {
        SMBWARNING("%s using custom max write count of %d \n",
                   vfs_statfs(mp)->f_mntfromname, smp->sm_args.write_count[2]);
        SS_TO_SESSION(share)->iod_writeCounts[2] = smp->sm_args.write_count[2];
    }

    /* Changing rw_max_check_time? */
    smp->sm_args.rw_max_check_time = args->rw_max_check_time;
    if (smp->sm_args.rw_max_check_time != 0) {
        SMBWARNING("%s using custom read/write max check timeout of %d microSecs \n",
                   vfs_statfs(mp)->f_mntfromname, smp->sm_args.rw_max_check_time);
        SS_TO_SESSION(share)->rw_max_check_time = smp->sm_args.rw_max_check_time;
    }

     /* Changing rw_gb_threshold? */
    smp->sm_args.rw_gb_threshold = args->rw_gb_threshold;
    if (smp->sm_args.rw_gb_threshold != 0) {
        SMBWARNING("%s using custom read/write Gb threshold of %d\n",
                   vfs_statfs(mp)->f_mntfromname, smp->sm_args.rw_gb_threshold);
        SS_TO_SESSION(share)->rw_gb_threshold = smp->sm_args.rw_gb_threshold;
    }

    /*
     * Compression
     */
    
    /* Client compression algorithm map passed into session via ioctl */
    smp->sm_args.compression_io_threshold = args->compression_io_threshold;
    SS_TO_SESSION(share)->compression_io_threshold = args->compression_io_threshold;
    if (smp->sm_args.compression_io_threshold != 4096) {
        SMBWARNING("%s using custom compression IO threshold of %d \n",
                   vfs_statfs(mp)->f_mntfromname, smp->sm_args.compression_io_threshold);
    }

    smp->sm_args.compression_chunk_len = args->compression_chunk_len;
    SS_TO_SESSION(share)->compression_chunk_len = args->compression_chunk_len;
    if (smp->sm_args.compression_chunk_len != 262144) {
        SMBWARNING("%s using custom compression chunk len of %d \n",
                   vfs_statfs(mp)->f_mntfromname, smp->sm_args.compression_chunk_len);
    }

    smp->sm_args.compression_max_fail_cnt = args->compression_max_fail_cnt;
    SS_TO_SESSION(share)->compression_max_fail_cnt = args->compression_max_fail_cnt;
    if (smp->sm_args.compression_max_fail_cnt != 5) {
        SMBWARNING("%s using custom compression max fail cnt of %d \n",
                   vfs_statfs(mp)->f_mntfromname, smp->sm_args.compression_max_fail_cnt);
    }

    /* Copy user exclusion list if any */
    for (i = 0; i < args->compression_exclude_cnt; i++) {
        smp->sm_args.compression_exclude[i] = smb_strndup(args->compression_exclude[i],
                                                          kClientCompressMaxExtLen,
                                                          &smp->sm_args.compression_exclude_allocsize[i]);
    }
    smp->sm_args.compression_exclude_cnt = args->compression_exclude_cnt;

    /* Copy user inclusion list if any */
    for (i = 0; i < args->compression_include_cnt; i++) {
        smp->sm_args.compression_include[i] = smb_strndup(args->compression_include[i],
                                                          kClientCompressMaxExtLen,
                                                          &smp->sm_args.compression_include_allocsize[i]);
    }
    smp->sm_args.compression_include_cnt = args->compression_include_cnt;

    /*
     * See if they sent use a submount path to use.
     * This function also checks/cleans up the args->path and args->path_len
     */
	if (args->path_len) {
		smbfs_create_start_path(smp, args, SMB_UNICODE_STRINGS(SS_TO_SESSION(share)));
	}
    
    /*
     * Are we forcing share encryption?
     * This is probably from mount_smbfs mount option being set meaning that
     * only one share is being encrypted.
     */
    if (smp->sm_args.altflags & SMBFS_MNT_SHARE_ENCRYPT) {
        /* Force share level encryption */
        SMBWARNING("Share level encryption forced on for %s volume \n",
                   vfs_statfs(mp)->f_mntfromname);
        share->ss_share_flags |= SMB2_SHAREFLAG_ENCRYPT_DATA;
        
        /*
         * If one share is being encrypted, then should also encrypt the
         * IPC$ traffic as that may also relate to that share. For example,
         * Spotlight/SMB uses IPC$ and may contain search info for the
         * encrypted share.
         */
        SS_TO_SESSION(share)->session_misc_flags |= SMBV_FORCE_IPC_ENCRYPT;
    }

    /*
     * Validate the negotiate if SMB 2/3. smb2fs_smb_validate_neg_info will
     * check for whether the session needs to be validated or not.
     */
    if (!(smp->sm_args.altflags & SMBFS_MNT_VALIDATE_NEG_OFF)) {
        if ((SS_TO_SESSION(share)->session_flags & SMBV_SMB2) &&
            !(SS_TO_SESSION(share)->session_flags & SMBV_SMB311)) {
            error = smb2fs_smb_validate_neg_info(share, context);
            if (error) {
                SMBERROR("smb2fs_smb_validate_neg_info failed %d \n", error);
                goto bad;
            }
        }
    }
    else {
        SMBWARNING("Validate Negotiate is off in preferences\n");
    }
    
    /*
     * [MS-SMB2] 4.8 The diagram shows that the query server network interfaces
     * comes after the Validate Negotiate.
     * 
     * For multichannel, see if its time to query the server interfaces
     */
    if (SS_TO_SESSION(share)->session_flags & SMBV_MULTICHANNEL_ON) {
        error = smb_session_query_net_if(SS_TO_SESSION(share));
        if (error) {
            SMBERROR("smb_session_query_net_if: error = %d\n", error);
        }
    }

    /*
	 * This call should be done from mount() in vfs layer. Not sure why each 
	 * file system has to do it here, but go ahead and make an internal call to 
	 * fill in the default values.
	 */
	error = smbfs_smb_statfs(smp, vfs_statfs(mp), context);
	if (error) {
		SMBDEBUG("smbfs_smb_statfs failed %d\n", error);
		goto bad;
	}
    
	/* Copy in the from name, used for reconnects and other things  */
	strlcpy(vfs_statfs(mp)->f_mntfromname, args->url_fromname, MAXPATHLEN);

    /*
     * Now get the mounted volumes unique id <79665965> while
     * preventing OOB read when copying from unique_id buffer
     */
    if (args->unique_id_len > SMB_MAX_UNIQUE_ID) {
        SMBERROR("Invalid unique id buffer length\n");
        error = EINVAL;
        goto bad;
    }

    smp->sm_args.unique_id_len = args->unique_id_len;
    SMB_MALLOC_DATA(smp->sm_args.unique_id, smp->sm_args.unique_id_len, Z_WAITOK);
	if (smp->sm_args.unique_id) {
		bcopy(args->unique_id, smp->sm_args.unique_id, smp->sm_args.unique_id_len);
	} else {
		smp->sm_args.unique_id_len = 0;
	}

    SMB_FREE_TYPE(struct smb_mount_args, args);
    args = NULL;

    if (smp->sm_args.altflags & SMBFS_MNT_TIME_MACHINE) {
		/* Time Machine mounts are a special hard mount with a shorter timeout */
        SMBWARNING("%s mounted using Time Machine flag\n",  vfs_statfs(mp)->f_mntfromname);
		smp->sm_args.altflags &= ~SMBFS_MNT_SOFT; /* clear soft mount flag */
		SS_TO_SESSION(share)->session_misc_flags |= SMBV_MNT_TIME_MACHINE;
    }

    vfs_getnewfsid(mp);
	
    if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
        /* Only SMB 2/3 uses File IDs or AAPL create context */
        if (smp->sm_args.altflags & SMBFS_MNT_FILE_IDS_OFF) {
            /* Turn off File IDs */
            SMBWARNING("File IDs has been turned off for %s volume\n",
                       (smp->sm_args.volume_name) ? smp->sm_args.volume_name : "");
            SS_TO_SESSION(share)->session_misc_flags &= ~SMBV_HAS_FILEIDS;
        }

        if (smp->sm_args.altflags & SMBFS_MNT_AAPL_OFF) {
            /* Turn off AAPL */
            SMBWARNING("AAPL has been turned off for %s volume\n",
                       (smp->sm_args.volume_name) ? smp->sm_args.volume_name : "");
            SS_TO_SESSION(share)->session_misc_flags |= SMBV_OTHER_SERVER;
        }

		share->ss_max_def_close_cnt = k_def_close_hi_water;
		if (smp->sm_args.altflags & SMBFS_MNT_FILE_DEF_CLOSE_OFF) {
			/* 
			 * Turn off File Handle Leasing. We will still ask for Read/Handle
			 * leases, but will no longer do the deferred closes
			 */
			SMBWARNING("File Deferred Close has been turned off for %s volume\n",
					   (smp->sm_args.volume_name) ? smp->sm_args.volume_name : "");
			share->ss_max_def_close_cnt = 0;
		}
    }

	if ((SMBV_SMB3_OR_LATER(SS_TO_SESSION(share))) &&
		(SS_TO_SESSION(share)->session_sopt.sv_active_capabilities & SMB2_GLOBAL_CAP_DIRECTORY_LEASING)) {
		if (smp->sm_args.altflags & SMBFS_MNT_DIR_LEASE_OFF) {
			/* Turn off Dir Leasing */
			SMBWARNING("Dir Leasing has been turned off for %s volume\n",
					   (smp->sm_args.volume_name) ? smp->sm_args.volume_name : "");
		}
	}

	/*
	 * Need to get the remote server's file system information
	 * here before we do anything else. Make sure we have the servers or
	 * the default value for ss_maxfilenamelen. NOTE: We use it in strnlen.
	 */
	smbfs_smb_qfsattr(smp, context); /* This checks for AAPL Create Context */

    /*
     * Do they want to force HiFi mode to be used on the client side?
     */
    if ((smp->sm_args.altflags & SMBFS_MNT_HIGH_FIDELITY) &&
        (!(SS_TO_SESSION(share)->session_misc_flags & SMBV_MNT_HIGH_FIDELITY))) {
        SMBWARNING("HiFi forced on for %s volume\n",
                   (smp->sm_args.volume_name) ? smp->sm_args.volume_name : "");
        SS_TO_SESSION(share)->session_misc_flags |= SMBV_MNT_HIGH_FIDELITY;
    }

    if (SS_TO_SESSION(share)->session_misc_flags & SMBV_MNT_HIGH_FIDELITY) {
        /* Is connecting to share disk allowed? */
        if (smp->sm_args.altflags & SMBFS_MNT_HIFI_DISABLED) {
            SMBERROR("Connecting to Share Disk is disabled on this client\n");
            error = EPERM;
            goto bad;
        }

        /*
         * High Fidelity mounts return vnode attributes directly from server
         *
         * Its a high fidelity mount, so force some settings here
         * 1. ACLs are on so that we can retrieve them and return them.
         *    This is done later in this function.
         * 2. Directory enumeration caching is off which also forces off
         *    smbfs_vnop_getattrlistbulk(). This forces vfs layer to do
         *    vnop_readdir() followed by vnop_getattr() for each item returned.
         * 3. UBC data caching is off for reads/writes as that requires the
         *    normal meta data caches. Specific case is a write that extends
         *    the EOF but is currently cached in UBC and returning the correct
         *    data length for a getattr
         * 4. Make it a special hard mount with a shorter timeout so we dont
         *    return ETIMEDOUT errors.
         */
        SMBWARNING("Mounted using High Fidelity for %s volume \n",
                   vfs_statfs(mp)->f_mntfromname);

        smp->sm_args.altflags |= (SMBFS_MNT_DIR_CACHE_OFF | SMBFS_MNT_DATACACHE_OFF);

        /*
         * High fidelity mounts are a special hard mount with a shorter timeout.
         * No ETIMEDOUT errors will ever be returned when reconnect is in
         * progress. This is how the TM/SMB mounts work.
         */
        smp->sm_args.altflags &= ~SMBFS_MNT_SOFT; /* clear soft mount flag */
    }

    if (smp->sm_args.altflags & SMBFS_MNT_DATACACHE_OFF) {
        /* Turn off data caching meaning dont use UBC for reads/writes */
        SMBWARNING("Data caching has been turned off for %s volume \n",
                   vfs_statfs(mp)->f_mntfromname);
        SS_TO_SESSION(share)->session_misc_flags |= SMBV_MNT_DATACACHE_OFF;
    }

    if (smp->sm_args.altflags & SMBFS_MNT_MDATACACHE_OFF) {
        /*
         * Turn off meta data caching which includes vnop_getattrlistbulk
         * This makes vnop_getattr() always ask the server for latest info
         */
        SMBWARNING("Meta data caching has been turned off for %s volume \n",
                   vfs_statfs(mp)->f_mntfromname);
        SS_TO_SESSION(share)->session_misc_flags |= SMBV_MNT_MDATACACHE_OFF;

        /* Turn off dir enumeration caching too as that also caches meta data */
        smp->sm_args.altflags |= SMBFS_MNT_DIR_CACHE_OFF;
    }

    /*
     * Its a unix server see if it supports any of the UNIX extensions
     * But only for SMB v1 servers.
     */
    if (!(SS_TO_SESSION(share)->session_flags & SMBV_SMB2) &&
        UNIX_SERVER(SS_TO_SESSION(share))) {
        smbfs_unix_qfsattr(share, context);
    }

	/* 
	 * This volume was mounted as guest and NOT a HiFi mount, turn off ACLs.
	 * <28555880> If its NOT a TM mount, then set the mount point to ignore
	 * ownership and we will always return an owner of 99, and group of 99.
	 */
	if (SMBV_HAS_GUEST_ACCESS(SS_TO_SESSION(share)) &&
        !(SS_TO_SESSION(share)->session_misc_flags & SMBV_MNT_HIGH_FIDELITY)) {
		if (share->ss_attributes & FILE_PERSISTENT_ACLS) {
			SMB_LOG_ACCESS("%s was mounted as guest turning off ACLs support.\n", 
						   vfs_statfs(mp)->f_mntfromname);
		}
		share->ss_attributes &= ~FILE_PERSISTENT_ACLS;

		if (!(SS_TO_SESSION(share)->session_misc_flags & SMBV_MNT_TIME_MACHINE)) {
			/* Normal Guest mounts get ignore ownership */
			vfs_setflags(mp, MNT_IGNORE_OWNERSHIP);
		}
	}
	
	/* Make sure the server is in the same domain, if not turn off acls */
	if (share->ss_attributes & FILE_PERSISTENT_ACLS) {
		isServerInSameDomain(share, smp);
	}
	
	/* See if the server supports the who am I operation */ 
	if (UNIX_CAPS(share) & CIFS_UNIX_POSIX_PATH_OPERATIONS_CAP) {
		smbfs_unix_whoami(share, smp, context);
	}

    /*
     * Only check HiFi if we are NOT forcing HiFi mode. There is a later
     * check to verify if HiFi requests work or not.
     */
    if (!(smp->sm_args.altflags & SMBFS_MNT_HIGH_FIDELITY) &&
        (SS_TO_SESSION(share)->session_misc_flags & SMBV_MNT_HIGH_FIDELITY)) {
        /*
         * Verify that the server supports high fidelity
         * 1. Has to be SMB 2/3
         * 2. Has to be OSX server (ie supports AAPL Create Context)
         * 3. Server capabilites has to have kAAPL_SUPPORTS_HIFI set
         * 4. Named streams must be supported
         *
         * Make sure to match the checks in smbf2fs_reconnect()
         */
        if (!(SS_TO_SESSION(share)->session_flags & SMBV_SMB2) || !(SS_TO_SESSION(share)->session_misc_flags & SMBV_OSX_SERVER) ||
            !(SS_TO_SESSION(share)->session_server_caps & kAAPL_SUPPORTS_HIFI) ||
            !(share->ss_attributes & FILE_NAMED_STREAMS)) {
            SMBERROR("%s failed high fidelity check \n", vfs_statfs(mp)->f_mntfromname);
            error = EINVAL;
            goto bad;
        }
    }

    if (smp->sm_args.altflags & SMBFS_MNT_SNAPSHOT) {
        /*
         * Snapshot mounts are read only and all creates have the time warp
         * token set
         */
        SMBWARNING("%s mounted using Snapshot of <%s> \n",
                   vfs_statfs(mp)->f_mntfromname,
                   SS_TO_SESSION(share)->snapshot_time);

        SS_TO_SESSION(share)->session_misc_flags |= SMBV_MNT_SNAPSHOT;
    }

    /*
     * NOTE: after this point, its really hard to fail the mount because its
     * hard to free the root vnode at this early time.
     */

    error = smbfs_root(mp, &vp, context);
	if (error) {
		SMBDEBUG("The smbfs_root failed %d\n", error);
		goto bad;
	}

    if (SS_TO_SESSION(share)->session_misc_flags & SMBV_MNT_HIGH_FIDELITY) {
        /*
         * Verify that the client and server are using the same size and
         * version of the HiFi structure.
         */
        if ((vp == NULL) ||
            (smp->sm_rvp == NULL) ||
            (vp != smp->sm_rvp)) {
            /* Paranoid check which should never happen */
            SMBERROR("vp or sm_rvp is NULL or vp != sm_rvp??? \n");
        }
        else {
            VATTR_INIT(&vap);    /* Really don't care about the vap */
            error = smbfs_getattr(share, vp, &vap, context);
            if (error) {
                /* Fail the mount */
                SMBERROR("smbfs_getattr for HiFi check failed %d \n", error);

                smp->sm_rvp = NULL;     /* We no longer have a reference so clear it out */
                vnode_rele(vp);         /* to drop ref taken by smbfs_root */
                vnode_put(vp);          /* to drop ref taken by smbfs_root above */

                /* Remove all vnodes for this mount */
                (void)vflush(mp, NULLVP, FORCECLOSE);
                goto bad;
            }
        }
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

    /*
     * Check for ACL support
     */
    if (!(SS_TO_SESSION(share)->session_misc_flags & SMBV_MNT_HIGH_FIDELITY)) {
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
        }
        else {
            SMB_LOG_ACCESS("%s doesn't support ACLs\n", vfs_statfs(mp)->f_mntfromname);
            vfs_clearextendedsecurity (mp);
        }
    }
    else {
        /* High Fidelity mount forces ACLs to be on */
        vfs_setextendedsecurity(mp);
        share->ss_attributes |= FILE_PERSISTENT_ACLS;
    }

    if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
        if (share->ss_attributes & FILE_SUPPORTS_REPARSE_POINTS) {
            smp->sm_flags |= MNT_SUPPORTS_REPARSE_SYMLINKS;
        }
        
        if (!(SS_TO_SESSION(share)->session_misc_flags & SMBV_HAS_COPYCHUNK)) {
            /* Check for CopyChunk support */
            if (!smb2fs_smb_cmpd_check_copyfile(share, VTOSMB(vp), context)) {
                SS_TO_SESSION(share)->session_misc_flags |= SMBV_HAS_COPYCHUNK;
            }
        }

        /*
         * If its a macOS 10.12.x server or older, do not use file leases
         * for deferred file closes as that leasing implementation is not
         * fully supported. Many times a lease break is not sent.
         *
         * We think its a macOS 10.12.x or older server by
         *  1) Has File Leasing, but does not have Dir Leasing
         *  2) Supports AAPL Create Context and has
         *     a) Server capabilites of macOS copyfile, readdirattr, unix based
         */
        if ((SS_TO_SESSION(share)->session_sopt.sv_active_capabilities & SMB2_GLOBAL_CAP_LEASING) &&
            !(SS_TO_SESSION(share)->session_sopt.sv_active_capabilities & SMB2_GLOBAL_CAP_DIRECTORY_LEASING) &&
            (SS_TO_SESSION(share)->session_misc_flags & SMBV_OSX_SERVER) &&
            (SS_TO_SESSION(share)->session_server_caps & kAAPL_SUPPORTS_READ_DIR_ATTR) &&
            (SS_TO_SESSION(share)->session_server_caps & kAAPL_SUPPORTS_OSX_COPYFILE) &&
            (SS_TO_SESSION(share)->session_server_caps & kAAPL_UNIX_BASED)) {
            SMBWARNING("Found older macOS server, disabling File Deferred Close for %s volume\n",
                       (smp->sm_args.volume_name) ? smp->sm_args.volume_name : "");
            share->ss_max_def_close_cnt = 0;
        }
    }
    else {
        if ((share->ss_attributes & FILE_SUPPORTS_REPARSE_POINTS) &&
            (((!UNIX_CAPS(share)) || (SS_TO_SESSION(share)->session_flags & SMBV_DARWIN)))) {
            smp->sm_flags |= MNT_SUPPORTS_REPARSE_SYMLINKS;
        }
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
     * option in the nsmb.conf file. The "nsmb.conf" allows the user to turn 
     * off named streams per share. So now we only check for turning off named 
     * streams since the default is to have them on.
     */
    if (share->ss_attributes & FILE_NAMED_STREAMS) {
        if (!(smp->sm_args.altflags & SMBFS_MNT_STREAMS_ON)) {
            share->ss_attributes &= ~FILE_NAMED_STREAMS;
        }
    }
    
    if (!(SS_TO_SESSION(share)->session_flags & SMBV_SMB2)) {
        /*
         * SMB 1 Only
         *
         * We now default to have named streams on if the server supports named
         * streams. The user can turn off named streams by creating a file on 
         * the top level of the share called ".com.apple.smb.streams.off". 
         *
         * .com.apple.smb.streams.off - If exist on top level of share means
         * turn off streams.
         */
        if (share->ss_attributes & FILE_NAMED_STREAMS) {
            if (smbfs_smb_query_info(share, VTOSMB(vp), VREG,
                                     SMB_STREAMS_OFF, sizeof(SMB_STREAMS_OFF) - 1,
                                     NULL, context) == 0) {
                share->ss_attributes &= ~FILE_NAMED_STREAMS;
            } else if (! UNIX_SERVER(SS_TO_SESSION(share)) &&
                       (smbfs_smb_qstreaminfo(share, VTOSMB(vp), VDIR,
                                              NULL, 0,
                                              SFM_DESKTOP_NAME,
                                              NULL, NULL,
                                              NULL, NULL,
                                              &stream_flags, NULL,
                                              context) == 0)) {
                /*
                 * We would like to know if this is a really old Windows server
                 * with Services For Mac (SFM Volume), we skip this check for
                 * unix servers.
                 */
                smp->sm_flags |= MNT_IS_SFM_VOLUME;
            }
        }
    }

	/*
	 * The AFP code sets io_devblocksize to one, which is used by the Cluster IO
	 * code to decide what to do when writing past the eof.  ClusterIO code uses 
	 * the io_devblocksize to decided what size block to use when writing pass 
	 * the eof. So a io_devblocksize of one means only write to the eof. Seems
	 * like a hack, but not sure what else to do at this point. Talk this over
	 * with Joe and he wants to get back to it later.
	 */
	vfs_ioattr(mp, &smbIOAttr);	/* get the current settings */
	smbIOAttr.io_devblocksize = 1;
    
	/*
     * io_maxreadcnt/io_maxwritecnt is the IO size that we want passed to us
     * from UBC. 
     *
     * io_maxsegreadsize/io_maxsegwritesize is the hardware limited max size,
     * but we are not limited so set to be same as io_maxreadcnt/io_maxwritecnt.
     *
     * io_segreadcnt/io_segwritecnt is just the segment size / page size. Again,
     * no real meaning to us.  VM requires they must be evenly divisible by 4.
     * See <rdar://problem/14266574>.
     *
	 */
    
	if (SS_TO_SESSION(share)->session_flags & SMBV_SMB2) {
		smbIOAttr.io_segwritecnt = smb_maxsegwritesize / PAGE_SIZE;
		smbIOAttr.io_segreadcnt = smb_maxsegreadsize / PAGE_SIZE;
		
	} else {
        size_t f_iosize = vfs_statfs(mp)->f_iosize;
        if (f_iosize < (PAGE_SIZE * 4)) {
            f_iosize = PAGE_SIZE * 4; /* Bad Server */
        }
        smbIOAttr.io_segreadcnt =  (uint32_t)(f_iosize / (PAGE_SIZE * 4)) * 4;
        smbIOAttr.io_segwritecnt = smbIOAttr.io_segreadcnt;
    }
	
	smbIOAttr.io_maxsegreadsize = smbIOAttr.io_segreadcnt * PAGE_SIZE;
    smbIOAttr.io_maxsegwritesize = smbIOAttr.io_segwritecnt * PAGE_SIZE;

    smbIOAttr.io_maxreadcnt = smbIOAttr.io_maxsegreadsize;
    smbIOAttr.io_maxwritecnt = smbIOAttr.io_maxsegwritesize;

	SMBWARNING("io_maxsegreadsize = %d io_maxsegwritesize = %d f_iosize = %ld session_rxmax = %d session_wxmax = %d\n",
			 smbIOAttr.io_maxsegreadsize, smbIOAttr.io_maxsegwritesize,
			 vfs_statfs(mp)->f_iosize,
			 SS_TO_SESSION(share)->session_rxmax, SS_TO_SESSION(share)->session_wxmax);
	
	vfs_setioattr(mp, &smbIOAttr);
    
	
	/* smbfs_root did a vnode_get and a vnode_ref, so keep the ref but release the get */
	vnode_put(vp);

	/* We now have everyting we need to setup the dead/up/down routines */
	lck_mtx_lock(&share->ss_shlock);
	/* Use to tell the session that the share is going away, so just timeout messages */
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
	}
	else {
		if (!(smp->sm_args.altflags & SMBFS_MNT_TIME_MACHINE) &&
            !(SS_TO_SESSION(share)->session_misc_flags & SMBV_MNT_HIGH_FIDELITY)) {
			/* Regular hard mount */
			share->ss_dead_timer = smbfs_hard_deadtimer;
		}
		else {
			/* Time Machine or HiFi mount */
			share->ss_dead_timer = TIME_MACHINE_DEAD_TIMER;
		}
	}	
	
	/* All done add the mount point to the share so we can access these routines */
	share->ss_mount = smp;
	lck_mtx_unlock(&share->ss_shlock);
	SMBDEBUG("%s dead timer = %d\n", share->ss_name, share->ss_dead_timer);

	OSAddAtomic(1, &SS_TO_SESSION(share)->session_volume_cnt);
	if (SS_TO_SESSION(share)->throttle_info) {
		throttle_info_mount_ref(mp, SS_TO_SESSION(share)->throttle_info);
	}
	
    smbfs_notify_change_create_thread(smp);
    if (smp->sm_args.altflags & SMBFS_MNT_COMPOUND_ON) {
        vfs_setcompoundopen(mp);
    }
    else {
        SMBWARNING("compound off in preferences\n");
    }
    
    if ((SS_TO_SESSION(share)->session_flags & SMBV_SMB2) &&
        (SS_TO_SESSION(share)->session_misc_flags & SMBV_OSX_SERVER)) {
        
        /* Mac OS X SMB 2/3 server */
        share->ss_fstype = SMB_FS_MAC_OS_X;
        
        /* Enable server message notification */
        smbfs_start_svrmsg_notify(smp);
    }
	
    /*
     * Are we assuming SMB 3.x always has durable handle V2 and lease V2?
     * [MS-SMB2] 3.2.4.3.5 says that SMB 3.x must use durable handle V2
     * [MS-SMB2] 3.2.4.3.8 says that SMB 3.x must use lease V2
     *
     * nsmb.conf provides a way to override this assumption.
     */
    if (!(smp->sm_args.altflags & SMBFS_MNT_ASSUME_DUR_LEASE_V2_OFF)) {
        /* If SMB version 3.x, then durable handle and leases must be V2 */
        if (SMBV_SMB3_OR_LATER(SS_TO_SESSION(share))) {
            SS_TO_SESSION(share)->session_misc_flags |= SMBV_HAS_DUR_HNDL_V2;
        }
    }
    else {
        SMBWARNING("Do not assume durable handle V2 for %s volume \n",
                   vfs_statfs(mp)->f_mntfromname);
    }

    if (smp->sm_args.altflags & SMBFS_MNT_DUR_HANDLE_LOCKFID_ONLY) {
        SMBWARNING("Durable handle/leases only for O_EXLOCK/O_SHLOCK files for %s volume \n",
                   vfs_statfs(mp)->f_mntfromname);
    }

    if (SS_TO_SESSION(share)->session_sopt.sv_active_capabilities & SMB2_GLOBAL_CAP_PERSISTENT_HANDLES) {
        /*
         * If the server supports persistent handles, then it must
         * support durable handle V2
         */
        SS_TO_SESSION(share)->session_misc_flags |= SMBV_HAS_DUR_HNDL_V2;
    }

    /* For TM/SMB, leave in the check for durable handle V2 to be paranoid */
    if (smp->sm_args.altflags & SMBFS_MNT_TIME_MACHINE) {
		/*
		 * Check if Durable Handle V2 supported for this Time Machine mount
		 */
		if (!(SS_TO_SESSION(share)->session_misc_flags & SMBV_HAS_DUR_HNDL_V2) &&
			!(SS_TO_SESSION(share)->session_misc_flags & SMBV_NO_DUR_HNDL_V2)) {
			/* Check for Durable Handle V2 support */
			SMBERROR("Check for durable handles \n");
			if (!smb2fs_smb_check_dur_handle_v2(share, VTOSMB(vp),
												&SS_TO_SESSION(share)->session_dur_hndl_v2_default_timeout,
												context)) {
				SS_TO_SESSION(share)->session_misc_flags |= SMBV_HAS_DUR_HNDL_V2;
			}
			else {
				SS_TO_SESSION(share)->session_misc_flags |= SMBV_NO_DUR_HNDL_V2;
			}
		}
	}
    
    /* Set the IP QoS if provided */
    if (smp->sm_args.ip_QoS != 0) {
        SMBWARNING("Setting custom IP Qos to <%d> for %s volume\n",
                   smp->sm_args.ip_QoS, (smp->sm_args.volume_name) ? smp->sm_args.volume_name : "");
        option = smp->sm_args.ip_QoS;
        error = smb_iod_set_qos(SS_TO_SESSION(share)->session_iod, &option);
        if (error) {
            SMBERROR("smb_iod_set_qos failed %d \n", error);
        }
    }

	mount_cnt++;

#if 0
	SMBERROR("Checking crypto performance \n");
	smb_test_crypt_performance(SS_TO_SESSION(share), 1024 * 1024, 1024);
	smb_test_crypt_performance(SS_TO_SESSION(share), 1024 * 1024, 512);
	smb_test_crypt_performance(SS_TO_SESSION(share), 1024 * 1024, 96);
	smb_test_crypt_performance(SS_TO_SESSION(share), 1024 * 1024, 64);
#endif	

	SMB_LOG_KTRACE(SMB_DBG_MOUNT | DBG_FUNC_END, 0, 0, 0, 0, 0);
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
		if (smp->sm_hash) {
            hashdestroy(smp->sm_hash, M_SMBFSHASH, smp->sm_hashlen);
		}
		
		lck_mtx_free(smp->sm_hashlock, hash_lck_grp);
		lck_mtx_destroy(&smp->sm_statfslock, smbfs_mutex_group);
		lck_rw_destroy(&smp->sm_rw_sharelock, smbfs_rwlock_group);
        lck_mtx_destroy(&smp->sm_svrmsg_lock, smbfs_mutex_group);
		
		if (smp->sm_args.volume_name) {
            SMB_FREE_DATA(smp->sm_args.volume_name, smp->sm_args.volume_name_allocsize);
		}
		if (smp->sm_args.local_path != NULL) {
            SMB_FREE_DATA(smp->sm_args.local_path, smp->sm_args.local_path_len + 1);
		}
		if (smp->sm_args.network_path != NULL) {
            SMB_FREE_DATA(smp->sm_args.network_path, smp->sm_args.network_path_len);
		}
		if (smp->sm_args.unique_id) {
            SMB_FREE_DATA(smp->sm_args.unique_id, smp->sm_args.unique_id_len);
		}
		if (smp->ntwrk_gids) {
            SMB_FREE_DATA(smp->ntwrk_gids, smp->ntwrk_gids_allocsize);
		}
		if (smp->ntwrk_sids) {
            SMB_FREE_DATA(smp->ntwrk_sids, smp->ntwrk_sids_allocsize);
		}

        SMB_FREE_TYPE(struct smbmount, smp);
	}

    if (args != NULL) {
        SMB_FREE_TYPE(struct smb_mount_args, args);
    }

    SMB_LOG_KTRACE(SMB_DBG_MOUNT | DBG_FUNC_END, error, 0, 0, 0, 0);
	return (error);
}

/* Unmount the filesystem described by mp. */
static int
smbfs_unmount(struct mount *mp, int mntflags, vfs_context_t context)
{
	struct smbmount *smp = VFSTOSMBFS(mp);
    struct smb_share *share = smp->sm_share;
	vnode_t vp = NULL;
	int error = 0;
    struct smb_session *sessionp = NULL;
    struct timespec unmount_sleeptimespec = {0, 0};
    uint32_t i = 0;

    SMB_LOG_KTRACE(SMB_DBG_UNMOUNT | DBG_FUNC_START, mntflags, 0, 0, 0, 0);

	SMBVDEBUG("smbfs_unmount: flags=%04x\n", mntflags);

    /* Paranoid checks */
    if (share == NULL) {
        SMBERROR("share is null? \n");
        error = EBUSY;
        goto done;
    }

    sessionp = SS_TO_SESSION(share);
    if (sessionp == NULL) {
        SMBERROR("sessionp is null? \n");
        error = EBUSY;
        goto done;
    }

    /* Force unmount shutdown all outstanding I/O requests on this share. */
	if (mntflags & MNT_FORCE) {
		smb_iod_errorout_share_request(share, ENXIO);
	}

	error = smbfs_root(mp, &vp, context);
	if (error) {
		goto done;
    }

    /*
     * Reconnect can not take any vnode locks because a vnop/vop request may
     * be holding the vnode lock while waiting for reconnect to finish. If
     * reconnect is past the soft mount timer, then pending requests will
     * start getting ETIMEDOUT errors and an unmount could proceed even though
     * reconnect has not finished. There is a small timing window where
     * vflush() is called to reclaim the vnodes for this mount, but reconnect
     * has succeeded and is trying to reopen those same vnodes and we panic.
     * Need to wait until reconnect is finished before continuing the unmount.
     */
    unmount_sleeptimespec.tv_sec = 5;
    unmount_sleeptimespec.tv_nsec = 0;
    //SMBERROR("Wait for reconnect to finish \n");
    while (sessionp->session_iod->iod_flags & SMBIOD_RECONNECT) {
        msleep(&sessionp->session_iod->iod_flags, 0, PWAIT, "unmount reconnect wait", &unmount_sleeptimespec);
    }

    /*
     * <74384840> needs to wait/cancel the alt channel establishment before
     * proceeding with the unmount. o.w we might still have uncompleted requests
     * when unmount code is done.
     */
    if (sessionp->session_flags & SMBV_MULTICHANNEL_ON) {
        SMB_LOG_MC("pause trials before proceeding with the unmount\n");
        smb2_mc_pause_trials(&sessionp->session_interface_table);
        while (smb2_mc_check_for_active_trials(&sessionp->session_interface_table))
        {
            SMB_LOG_MC("still have ongoing trials\n");
            smb2_mc_abort_trials(&sessionp->session_interface_table);
            struct timespec sleeptime;
            sleeptime.tv_sec = 1;
            sleeptime.tv_nsec = 0;
            msleep(&sessionp->session_iod->iod_flags, 0, PWAIT,
                   "unmount wait for ongiong trials", &sleeptime);
        }
    }

    SMB_LOG_MC("no more ongoing trials \n");

    /*
     * The vflush will cause all the vnodes associated with this
     * mount to get reclaimed.
     */
	error = vflush(mp, vp, (mntflags & MNT_FORCE) ? FORCECLOSE : 0);
	if (error) {
		vnode_put(vp);
		goto done;
	}
    
	if (vnode_isinuse(vp, 1)  && !(mntflags & MNT_FORCE)) {
		SMBDEBUG("smbfs_unmount: usecnt\n");
		vnode_put(vp);
        error = EBUSY;
		goto done;
	}

	SMB_LOG_LEASING("total def close count <%lld> on <%s> \n",
                    share->ss_total_def_close_cnt, share->ss_name);

	smp->sm_rvp = NULL;	/* We no longer have a reference so clear it out */
	vnode_rele(vp);	/* to drop ref taken by smbfs_mount */
	vnode_put(vp);	/* to drop ref taken by VFS_ROOT above */

	(void)vflush(mp, NULLVP, FORCECLOSE);
    
    /* Cancel outstanding svrmsg notify request */
    if ((sessionp->session_flags & SMBV_SMB2) &&
        (sessionp->session_misc_flags & SMBV_OSX_SERVER)) {
        smbfs_stop_svrmsg_notify(smp);
    }
	
	/* We are done with this share shutdown all outstanding I/O requests. */
	smb_iod_errorout_share_request(share, ENXIO);

    OSAddAtomic(-1, &SS_TO_SESSION(share)->session_volume_cnt);
	smbfs_notify_change_destroy_thread(smp);

	if (sessionp->throttle_info)
		throttle_info_mount_rel(mp);
	
    /* Make sure SMB 2/3 fid table is empty */
    if (sessionp->session_flags & SMBV_SMB2) {
        smb_fid_delete_all(share);
    }

	/* Remove the smb mount pointer from the share before freeing it */
	lck_mtx_lock(&share->ss_shlock);
	share->ss_mount = NULL;
	share->ss_dead = NULL;
	share->ss_up = NULL;
	share->ss_down = NULL;
	lck_mtx_unlock(&share->ss_shlock);
	 
	smb_share_rele(share, context);
	vfs_setfsprivate(mp, (void *)0);

	if (smp->sm_hash) {
        hashdestroy(smp->sm_hash, M_SMBFSHASH, smp->sm_hashlen);
		smp->sm_hash = (void *)0xDEAD5AB0;
	}
	lck_mtx_free(smp->sm_hashlock, hash_lck_grp);

	lck_mtx_destroy(&smp->sm_statfslock, smbfs_mutex_group);
    lck_mtx_destroy(&smp->sm_svrmsg_lock, smbfs_mutex_group);
	lck_rw_destroy(&smp->sm_rw_sharelock, smbfs_rwlock_group);
    
    if (smp->sm_args.volume_name) {
        SMB_FREE_DATA(smp->sm_args.volume_name, smp->sm_args.volume_name_allocsize);
    }
    if (smp->sm_args.local_path != NULL) {
        SMB_FREE_DATA(smp->sm_args.local_path, smp->sm_args.local_path_len + 1);
    }
    if (smp->sm_args.network_path != NULL) {
        SMB_FREE_DATA(smp->sm_args.network_path, smp->sm_args.network_path_len);
    }
    if (smp->sm_args.unique_id) {
        SMB_FREE_DATA(smp->sm_args.unique_id, smp->sm_args.unique_id_len);
    }
    if (smp->ntwrk_gids) {
        SMB_FREE_DATA(smp->ntwrk_gids, smp->ntwrk_gids_allocsize);
    }
    if (smp->ntwrk_sids) {
        SMB_FREE_DATA(smp->ntwrk_sids, smp->ntwrk_sids_allocsize);
    }

    for (i = 0; i < smp->sm_args.compression_exclude_cnt; i++) {
        if (smp->sm_args.compression_exclude[i] != NULL) {
            SMB_FREE_DATA(smp->sm_args.compression_exclude[i],
                          smp->sm_args.compression_exclude_allocsize[i]);
        }
    }
    smp->sm_args.compression_exclude_cnt = 0;
    
    for (i = 0; i < smp->sm_args.compression_include_cnt; i++) {
        if (smp->sm_args.compression_include[i] != NULL) {
            SMB_FREE_DATA(smp->sm_args.compression_include[i],
                          smp->sm_args.compression_include_allocsize[i]);
        }
    }
    smp->sm_args.compression_include_cnt = 0;

    if (smp) {
        SMB_FREE_TYPE(struct smbmount, smp);
    }
    
	vfs_clearflags(mp, MNT_LOCAL);
	mount_cnt--;
    
    lck_mtx_lock(&mc_notifier_lck);
    if (!mount_cnt) {

        /* Signal the mc_notifier to kill itself*/
        if (mc_notifier_pid != -1) {
            proc_signal(mc_notifier_pid, SIGTERM);
            mc_notifier_pid = -1;
        }
    }
    lck_mtx_unlock(&mc_notifier_lck);

    error = 0;
    
done:
    SMB_LOG_KTRACE(SMB_DBG_UNMOUNT | DBG_FUNC_END, error, 0, 0, 0, 0);
	return (error);
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
	int error = 0;

    SMB_LOG_KTRACE(SMB_DBG_ROOT | DBG_FUNC_START, 0, 0, 0, 0, 0);

	if (smp == NULL) {
		SMBERROR("smp == NULL (bug in umount)\n");
        error = EINVAL;
        goto done;
	}
	
	if (smp->sm_rvp) {
		/* just get the saved root vnode as its much faster */
		*vpp = smp->sm_rvp;
		error = vnode_get(*vpp);
        goto done;
	}
	
	/* Fill in the default values that we already know about the root vnode */
	bzero(&fattr, sizeof(fattr));
	nanouptime(&fattr.fa_reqtime);
	fattr.fa_valid_mask |= FA_VTYPE_VALID;
	fattr.fa_attr = SMB_EFA_DIRECTORY;
	fattr.fa_vtype = VDIR;
    
    if (smp->sm_root_ino == 0) {
        /* 
         * Must be at mount time and we dont know what the root File ID is.
         * Assume its 2 to start with
         */
        smp->sm_root_ino = SMBFS_ROOT_INO;
        fattr.fa_ino = SMBFS_ROOT_INO;
   }
    else {
        /* Recreating root vnode and we know what its ID was */
        fattr.fa_ino = smp->sm_root_ino;
    }
    
	/*
	 * First time to get the root vnode, smbfs_nget will create it and check 
	 * with the network to make sure all is well with the root node. Could get 
	 * an error if the device is not ready are we have no access.
	 */
	share = smb_get_share_with_reference(smp);
	error = smbfs_nget(share, mp,
                       NULL, "TheRooT", 7,
                       &fattr, &vp,
                       0, SMBFS_NGET_CREATE_VNODE,
                       context);
	smb_share_rele(share, context);
	if (error) {
        goto done;
    }

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
            goto done;
		}
	} else {
		/* 
		 * Must have had two or more processes running at same time, other process 
		 * saved the root vnode, so just unlock this one and return 
		 */
		smbnode_unlock(VTOSMB(vp));		/* Release the smbnode lock */		
	}

	*vpp = vp;
    error = 0;

done:
    SMB_LOG_KTRACE(SMB_DBG_ROOT | DBG_FUNC_END, error, 0, 0, 0, 0);
    return (error);
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

	if (done == 1) {
		return (0);
	}
	
	done = 1;

	size_t sz = sizeof(g_hardware_memory_size);
	
	if (sysctlbyname("hw.memsize", &g_hardware_memory_size,  &sz, NULL, 0) != 0) {
		/* Lets assume 2 GB */
		SMBERROR("Failed to get hardware memory size \n");
		g_hardware_memory_size = ((uint64_t) 2) * 1024 * 1024 * 1024;
	}
	
	if (g_hardware_memory_size <= ((uint64_t) 2) * 1024 * 1024 * 1024) {
		g_max_dirs_cached = k_2GB_max_dirs_cached;
		g_max_dir_entries_cached = k_2GB_max_dir_entries_cached;
	}
	else {
		if (g_hardware_memory_size <= ((uint64_t) 4) * 1024 * 1024 * 1024) {
			g_max_dirs_cached = k_4GB_max_dirs_cached;
			g_max_dir_entries_cached = k_4GB_max_dir_entries_cached;
		}
		else {
			if (g_hardware_memory_size <= ((uint64_t) 8) * 1024 * 1024 * 1024) {
				g_max_dirs_cached = k_8GB_max_dirs_cached;
				g_max_dir_entries_cached = k_8GB_max_dir_entries_cached;
			}
			else {
				if (g_hardware_memory_size <= ((uint64_t) 16) * 1024 * 1024 * 1024) {
					g_max_dirs_cached = k_16GB_max_dirs_cached;
					g_max_dir_entries_cached = k_16GB_max_dir_entries_cached;
				}
				else {
					g_max_dirs_cached = k_LotsOfGB_max_dirs_cached;
					g_max_dir_entries_cached = k_LotsOfGB_max_dir_entries_cached;
				}
			}
		}
	}

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
	struct vfsstatfs *cachedstatfs = NULL;
	struct timespec ts;
	int error = 0;
    struct smb_session *sessionp = NULL;
    char *tmp_str = NULL;

    SMB_LOG_KTRACE(SMB_DBG_VFS_GETATTR | DBG_FUNC_START, 0, 0, 0, 0, 0);

	if ((smp->sm_rvp == NULL) || (VTOSMB(smp->sm_rvp) == NULL)) {
		error = EINVAL;
        goto done;
    }

    SMB_MALLOC_TYPE(cachedstatfs, struct vfsstatfs, Z_WAITOK_ZERO);

    if (cachedstatfs == NULL) {
        SMBERROR("cachedstatfs failed malloc\n");
        error = ENOMEM;
        goto done;
    }

	share = smb_get_share_with_reference(smp);
    sessionp = SS_TO_SESSION(share);

	lck_mtx_lock(&smp->sm_statfslock);
    memcpy(cachedstatfs, &smp->sm_statfsbuf, sizeof(struct vfsstatfs));
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
			error = smbfs_smb_statfs(smp, cachedstatfs, context);
			if (error == 0) {
				nanouptime(&ts);
				smp->sm_statfstime = ts.tv_sec;
				lck_mtx_lock(&smp->sm_statfslock);
                memcpy(&smp->sm_statfsbuf, cachedstatfs, sizeof(struct vfsstatfs));
				lck_mtx_unlock(&smp->sm_statfslock);
			}
            else {
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
	VFSATTR_RETURN(fsap, f_bsize, cachedstatfs->f_bsize);
	VFSATTR_RETURN(fsap, f_iosize, cachedstatfs->f_iosize);
	VFSATTR_RETURN(fsap, f_blocks, cachedstatfs->f_blocks);
	VFSATTR_RETURN(fsap, f_bfree, cachedstatfs->f_bfree);
	VFSATTR_RETURN(fsap, f_bavail, cachedstatfs->f_bavail);
	VFSATTR_RETURN (fsap, f_bused, cachedstatfs->f_blocks - cachedstatfs->f_bavail);
	VFSATTR_RETURN(fsap, f_files, cachedstatfs->f_files);
	VFSATTR_RETURN(fsap, f_ffree, cachedstatfs->f_ffree);
	
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
            VOL_CAP_FMT_64BIT_OBJECT_IDS | 
			0;
		
        if (sessionp->session_flags & SMBV_SMB2) {
            /* Assume that SMB 2/3 can always handle large files */
            cap->capabilities[VOL_CAPABILITIES_FORMAT] |= VOL_CAP_FMT_2TB_FILESIZE;
        }
        else {
            /*
             * SMB 1. Only say we support large files if the server supports it.
             * FAT shares can not handle large files either.
             */
            if ((SESSION_CAPS(sessionp) & SMB_CAP_LARGE_FILES) &&
                (share->ss_fstype != SMB_FS_FAT)) {
                cap->capabilities[VOL_CAPABILITIES_FORMAT] |= VOL_CAP_FMT_2TB_FILESIZE;
            }
        }

        /* Must be FAT so don't trust the modify times */
		if (share->ss_fstype == SMB_FS_FAT)
			cap->capabilities[VOL_CAPABILITIES_FORMAT] |= VOL_CAP_FMT_NO_ROOT_TIMES;
			
		if (share->ss_attributes & FILE_CASE_PRESERVED_NAMES)
			cap->capabilities[VOL_CAPABILITIES_FORMAT] |= VOL_CAP_FMT_CASE_PRESERVING;

        if (sessionp->session_misc_flags & SMBV_OSX_SERVER) {
            /* Its OS X Server so we know for sure */
            if (sessionp->session_volume_caps & kAAPL_CASE_SENSITIVE) {
                cap->capabilities[VOL_CAPABILITIES_FORMAT] |= VOL_CAP_FMT_CASE_SENSITIVE;
            }

            if ((sessionp->session_volume_caps & kAAPL_SUPPORT_RESOLVE_ID) &&
                (sessionp->session_misc_flags & SMBV_HAS_FILEIDS)) {
                /* Supports Resolve ID */
                cap->capabilities[VOL_CAPABILITIES_FORMAT] |= VOL_CAP_FMT_PATH_FROM_ID |
                                                              VOL_CAP_FMT_PERSISTENTOBJECTIDS;
            }
        }
        else {
            /*
             * Not a OS X Server, so we have to guess.
             *
             * This SMB file system is case INSENSITIVE and case preserving,
             * but servers vary, depending on the underlying volume.  In
             * pathconf we have to give a yes or no answer. We need to return a
             * consistent answer in both cases. We do not know the real answer
             * for case sensitive, but lets default to what 90% of the servers
             * have set. Also remember this fixes Radar 4057391 and 3530751.
             */
        }

        if (share->ss_attributes & FILE_SUPPORTS_SPARSE_FILES) {
			cap->capabilities[VOL_CAPABILITIES_FORMAT] |= VOL_CAP_FMT_SPARSE_FILES;
        }

		cap->capabilities[VOL_CAPABILITIES_INTERFACES] = 
			VOL_CAP_INT_ATTRLIST | 
			VOL_CAP_INT_FLOCK |
			VOL_CAP_INT_MANLOCK |
			/*
			 * Not all servers (e.g. macOS) have this requirement, but for the
			 * sake of simplicity, we set this always.
			 */
			VOL_CAP_INT_RENAME_OPENFAIL |
			0;
		
        /*
         * If its a FAT filesystem, then named streams are not supported.
         * If named streams are not supported, then the attribute enumeration
         * calls wont work as we can not Max Access or Resource Fork info.
         * Thus named streams and non FAT is required for the attribute 
         * enumeration calls.
         */
        if ((share->ss_fstype != SMB_FS_FAT) &&
            (share->ss_attributes & FILE_NAMED_STREAMS) &&
            !(smp->sm_args.altflags & SMBFS_MNT_READDIRATTR_OFF)) {
            /* vnop_readdirattr allowed */
            cap->capabilities[VOL_CAPABILITIES_INTERFACES] |= VOL_CAP_INT_READDIRATTR;
            SMBWARNING("smbfs_vnop_readdirattr has been turned on for %s volume\n",
                       (smp->sm_args.volume_name) ? smp->sm_args.volume_name : "");
        }
        else {
            if (share->ss_fstype == SMB_FS_FAT) {
                SMBWARNING("FAT filesystem so smbfs_vnop_readdirattr is disabled for %s volume\n",
                           (smp->sm_args.volume_name) ? smp->sm_args.volume_name : "");
            }
            
            if (!(share->ss_attributes & FILE_NAMED_STREAMS)) {
                SMBWARNING("No named streams so smbfs_vnop_readdirattr is disabled for %s volume\n",
                           (smp->sm_args.volume_name) ? smp->sm_args.volume_name : "");
            }
        }
        
        if (UNIX_CAPS(share) & CIFS_UNIX_FCNTL_LOCKS_CAP)
			cap->capabilities[VOL_CAPABILITIES_INTERFACES] |= VOL_CAP_INT_ADVLOCK;
			
		if (share->ss_attributes & FILE_NAMED_STREAMS)
			cap->capabilities[VOL_CAPABILITIES_INTERFACES] |= VOL_CAP_INT_NAMEDSTREAMS | VOL_CAP_INT_EXTENDED_ATTR;			
		
        if ((smp->sm_args.altflags & SMBFS_MNT_NOTIFY_OFF) == SMBFS_MNT_NOTIFY_OFF) {
            SMBWARNING("Notifications have been turned off for %s volume\n",
                       (smp->sm_args.volume_name) ? smp->sm_args.volume_name : "");
        }
        else {
            if (!(sessionp->session_flags & SMBV_SMB2) &&
                (sessionp->session_maxmux < SMB_NOTIFY_MIN_MUX)) {
                /* SMB 1 */
                SMBWARNING("Notifications are not support on %s volume\n",
                           (smp->sm_args.volume_name) ? smp->sm_args.volume_name : "");
            }
            else {
                cap->capabilities[VOL_CAPABILITIES_INTERFACES] |= VOL_CAP_INT_REMOTE_EVENT;
            }
        }
        
        /*
         * We only turn on VOL_CAP_INT_COPYFILE if it's an SMB 2/3 connection
         * AND we know it supports FSCTL_SRV_COPY_CHUNK IOCTL.
         */
		if ((sessionp->session_flags & SMBV_SMB2) &&
            (sessionp->session_misc_flags & SMBV_HAS_COPYCHUNK)) {
            cap->capabilities[VOL_CAPABILITIES_INTERFACES] |= VOL_CAP_INT_COPYFILE;
        }
        
		cap->capabilities[VOL_CAPABILITIES_RESERVED1] = 0;
		cap->capabilities[VOL_CAPABILITIES_RESERVED2] = 0;

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
            VOL_CAP_FMT_64BIT_OBJECT_IDS |
			0;
					
        if (sessionp->session_misc_flags & SMBV_OSX_SERVER) {
            /* Its OS X Server so we know for sure */
            if ((sessionp->session_volume_caps & kAAPL_SUPPORT_RESOLVE_ID) &&
                (sessionp->session_misc_flags & SMBV_HAS_FILEIDS)) {
                /* Supports Resolve ID */
                cap->valid[VOL_CAPABILITIES_FORMAT] |= VOL_CAP_FMT_PATH_FROM_ID;
            }
        }

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
			VOL_CAP_INT_RENAME_OPENFAIL |
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
                                                /* ATTR_CMN_GEN_COUNT | */
                                                /* ATTR_CMN_DOCUMENT_ID | */
												ATTR_CMN_USERACCESS |
												ATTR_CMN_EXTENDED_SECURITY |
												ATTR_CMN_UUID |
												ATTR_CMN_GRPUUID |
                                                /* ATTR_CMN_FILEID | */
                                                /* ATTR_CMN_PARENTID | */
                                                /* ATTR_CMN_FULLPATH | */
                                                /* ATTR_CMN_ADDEDTIME | */
                                                /* ATTR_CMN_ERROR | */
                                                /* ATTR_CMN_DATA_PROTECT_FLAGS | */
												0;

        if (sessionp->session_misc_flags & SMBV_MNT_HIGH_FIDELITY) {
            /* Copied from apfs */
            fsap->f_attributes.validattr.commonattr |=
                                                ATTR_CMN_OBJPERMANENTID |
                                                ATTR_CMN_BKUPTIME |
                                                ATTR_CMN_FILEID |
                                                ATTR_CMN_PARENTID;
        }

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
                                                /* ATTR_DIR_ENTRYCOUNT | */
												ATTR_DIR_MOUNTSTATUS |
												0;

        if (sessionp->session_misc_flags & SMBV_MNT_HIGH_FIDELITY) {
            /* Copied from apfs */
            fsap->f_attributes.validattr.dirattr |= ATTR_DIR_ENTRYCOUNT;
        }

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

        if (sessionp->session_misc_flags & SMBV_MNT_HIGH_FIDELITY) {
            /* Copied from apfs */
            fsap->f_attributes.validattr.fileattr |= ATTR_FILE_IOBLOCKSIZE;
        }

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

        if (sessionp->session_misc_flags & SMBV_MNT_HIGH_FIDELITY) {
            /* Copied from apfs */
            fsap->f_attributes.nativeattr.commonattr |=
                                                ATTR_CMN_OBJPERMANENTID |
                                                ATTR_CMN_CHGTIME |
                                                ATTR_CMN_BKUPTIME |
                                                ATTR_CMN_FNDRINFO |
                                                ATTR_CMN_OWNERID |
                                                ATTR_CMN_GRPID |
                                                ATTR_CMN_ACCESSMASK |
                                                ATTR_CMN_USERACCESS |
                                                ATTR_CMN_EXTENDED_SECURITY |
                                                ATTR_CMN_UUID |
                                                ATTR_CMN_FILEID |
                                                ATTR_CMN_PARENTID;
        }

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

        if (sessionp->session_misc_flags & SMBV_MNT_HIGH_FIDELITY) {
            /* Copied from apfs */
            fsap->f_attributes.nativeattr.dirattr |=
                                                ATTR_DIR_ENTRYCOUNT |
                                                ATTR_DIR_MOUNTSTATUS;
        }

        fsap->f_attributes.nativeattr.fileattr =
												/* ATTR_FILE_LINKCOUNT | */	/* Supported but not native */
												/* ATTR_FILE_IOBLOCKSIZE */
												ATTR_FILE_DEVTYPE |
												/* ATTR_FILE_FORKCOUNT */
												/* ATTR_FILE_FORKLIST */
												ATTR_FILE_DATALENGTH |
												ATTR_FILE_DATAALLOCSIZE |
												0;

        if (sessionp->session_misc_flags & SMBV_MNT_HIGH_FIDELITY) {
            /* Copied from apfs */
            fsap->f_attributes.nativeattr.fileattr |=
                                                ATTR_FILE_LINKCOUNT |
                                                ATTR_FILE_IOBLOCKSIZE |
                                                ATTR_FILE_ALLOCSIZE;
        }

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
            if ((sessionp->session_misc_flags & SMBV_MNT_SNAPSHOT) &&
                (strnlen(sessionp->snapshot_time, sizeof(sessionp->snapshot_time)) > 0)) {
                SMB_MALLOC_DATA(tmp_str, MAXPATHLEN, Z_WAITOK_ZERO);

                if (tmp_str == NULL) {
                    SMBERROR("tmp_str failed malloc\n");
                    error = ENOMEM;
                    goto done;
                }
                /* If snapshot mount, return the name with the time stamp */
                strlcpy(tmp_str, smp->sm_args.volume_name, MAXPATHLEN);
                strlcat(tmp_str, sessionp->snapshot_time, MAXPATHLEN);

                strlcpy(fsap->f_vol_name, tmp_str, MAXPATHLEN);
            }
            else {
                strlcpy(fsap->f_vol_name, smp->sm_args.volume_name, MAXPATHLEN);
            }
		}
        else {
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

done:
    if (cachedstatfs) {
        SMB_FREE_TYPE(struct vfsstatfs, cachedstatfs);
    }

    if (tmp_str) {
        SMB_FREE_DATA(tmp_str, MAXPATHLEN);
    }

    SMB_LOG_KTRACE(SMB_DBG_VFS_GETATTR | DBG_FUNC_END, error, 0, 0, 0, 0);
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
	struct timespec	ts;
    struct timespec waittime;
    int dirty = 0;
    struct smbmount *smp = NULL;
    
	cargs = (struct smbfs_sync_cargs *)args;

	if (smbnode_lock(VTOSMB(vp), SMBFS_EXCLUSIVE_LOCK) != 0) {
		return (VNODE_RETURNED);
	}

	np = VTOSMB(vp);
	np->n_lastvop = smbfs_sync_callback;

	share = smb_get_share_with_reference(VTOSMBFS(vp));
    if (share == NULL) {
        goto done;
    }
    
	/* 
	 * Must have gone into reconnect mode while interating the vnodes. Nothing for
	 * us to do until reconnect is done. Just get out and wait for the next time.
	 */
	if (share->ss_flags & SMBS_RECONNECTING) {
		goto done;
	}

	if (vnode_isreg(vp)) {
		/* 
		 * See if the file needs to be reopened or revoked. Ignore the error if
		 * being revoked it will get caught below
		 */
		(void)smbfs_smb_reopen_file(share, np, cargs->context);

		lck_mtx_lock(&np->f_openStateLock);
		if (np->f_openState & kNeedRevoke) {
			lck_mtx_unlock(&np->f_openStateLock);
            
			SMBWARNING_LOCK(np, "revoking %s\n", np->n_name);
            
			smbnode_unlock(np);
			np = NULL; /* Already unlocked */
			vn_revoke(vp, REVOKEALL, cargs->context);
			goto done;
		}
		lck_mtx_unlock(&np->f_openStateLock);
		
		/* 
		 * See if this file can close any deferred closes now.
         * Especially since all files now have leases and potentially
         * have pending deferred closes and we dont want to keep too many
         * open files on the server.
		 */
        CloseDeferredFileRefs(vp, "smbfs_sync_callback", 1, cargs->context);
        
        /* Can we upgrade the lease? */
        smb2fs_smb_lease_upgrade(share, vp, "SyncLeaseUpgrade", cargs->context);
	}
    else if (vnode_isdir(vp)) {
        /* See if dir enumeration cache has expired and can be freed */
        smb_dir_cache_check(vp, &np->d_main_cache, 0, cargs->context);
    }
    
	/*
	 * We have dirty data or we have a set eof pending in either case
	 * deal with it in smbfs_fsync.
	 */
    dirty = vnode_hasdirtyblks(vp);
	if (dirty ||
        (vnode_isreg(vp) && (np->n_flag & (NNEEDS_EOF_SET | NNEEDS_FLUSH)))) {
        /*
         * Only send flush to server if its been longer than 30 secs since when
         * the last write was done. A flush can take a while and thus it can
         * hammer performance if you are doing a flush in the middle of a long
         * set of writes.
         *
         * Note this is to flush the data out of the server caches and down
         * to the disk.
         */
        nanouptime(&ts);
        waittime.tv_sec = SMB_FSYNC_TIMO;
        waittime.tv_nsec = 0;
        timespecsub(&ts, &waittime);
        
        if (timespeccmp(&ts, &np->n_last_write_time, >)) {
            /* Make sure a flush actually gets done and sent */
            np->n_flag |= NNEEDS_FLUSH;
            
            error = smbfs_fsync(share, vp, cargs->waitfor, 0, cargs->context);
            if (error)
                cargs->error = error;
        }
	}
	
	/* Someone is monitoring this node see if we have any work */
	if (vnode_ismonitored(vp)) {
		int updateNotifyNode = FALSE;
		
		if (vnode_isdir(vp) && !(np->n_flag & N_POLLNOTIFY)) {
            smp = share->ss_mount;
            
            /* Make sure we are not in the middle of a rename */
            if ((smp != NULL) &&
                !(smp->sm_flags & MNT_CHANGE_NOTIFY_PAUSE)) {
                /*
                 * The smbfs_restart_change_notify will now handle not only 
                 * reopening of notification, but also the closing of 
                 * notifications. This is done to force items into polling when 
                 * we have too many items.
                 */
                smbfs_restart_change_notify(share, vp, cargs->context);
                updateNotifyNode = np->d_needsUpdate;
            }
        }
        else {
			updateNotifyNode = TRUE;
        }
        
		/* 
         * Looks like something changed, update the notify routines and our 
         * cache 
         */
        if (updateNotifyNode) {
			(void)smbfs_update_cache(share, vp, NULL, cargs->context);
        }
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
    struct smbfs_sync_cargs args = {0};
    int error;
    struct smbmount *smp = VFSTOSMBFS(mp);
    struct smb_share *share = NULL;
    struct smb_session *sessionp = NULL;

    SMB_LOG_KTRACE(SMB_DBG_SYNC | DBG_FUNC_START, 0, 0, 0, 0, 0);

    /* Check for reclaimed dirs in the cache */
    smb_global_dir_cache_remove(0, 0);

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

    /* See if its time to requery the server interfaces */
    if (smp == NULL) {
        SMBERROR("smp == NULL? \n");
        args.error = EINVAL;
        goto done;
    }

    share = smb_get_share_with_reference(smp);
    if (share == NULL) {
        SMBERROR("share == NULL? \n");
        args.error = EINVAL;
        goto done;
    }

    sessionp = SS_TO_SESSION(share);
    if (sessionp == NULL) {
        SMBERROR("sessionp == NULL? \n");
        args.error = EINVAL;
        goto done;
    }

    /* For multichannel, see if its time to query the server interfaces */
    if (sessionp->session_flags & SMBV_MULTICHANNEL_ON) {
        error = smb_session_query_net_if(sessionp);
        if (error) {
            SMBERROR("smb_session_query_net_if failed %d\n", error);
        }
    }

done:
    /* We only have a share if we took a reference, release it */
    if (share) {
        smb_share_rele(share, context);
    }

    SMB_LOG_KTRACE(SMB_DBG_SYNC | DBG_FUNC_END, args.error, 0, 0, 0, 0);
	return (args.error);
}

/*
 * smbfs_vget - Equivalent of AFP Resolve ID
 * Returns an unlocked vnode
 */
static int
smbfs_vget(struct mount *mp, ino64_t ino, vnode_t *vpp, vfs_context_t context)
{
    int error;
    struct smbmount *smp = VFSTOSMBFS(mp);
    struct smb_share *share = NULL;
    struct smb_session *sessionp = NULL;
    char *path = NULL;
    size_t path_max = MAXPATHLEN;
    struct smbfattr *fap = NULL;
    uint32_t resolve_error = 0;
    char *server_path = NULL;
    size_t server_path_allocsize = 0;
    vnode_t root_vp = NULL;
    struct smbnode *root_np = NULL;

    SMB_LOG_KTRACE(SMB_DBG_VGET | DBG_FUNC_START, ino, 0, 0, 0, 0);

    if (smp == NULL) {
        SMBERROR("smp == NULL\n");
        error = EINVAL;
        goto done;
    }

    SMB_MALLOC_DATA(path, MAXPATHLEN, Z_WAITOK_ZERO);
    if (path == NULL) {
        SMBERROR("SMB_MALLOC_DATA failed\n");
        error = ENOMEM;
        goto done;
    }

    SMB_MALLOC_TYPE(fap, struct smbfattr, Z_WAITOK_ZERO);
    if (fap == NULL) {
        SMBERROR("SMB_MALLOC_TYPE failed\n");
        error = ENOMEM;
        goto done;
    }
    
    share = smb_get_share_with_reference(smp);
    sessionp = SS_TO_SESSION(share);

    if ((sessionp->session_misc_flags & SMBV_OSX_SERVER) &&
        (sessionp->session_volume_caps & kAAPL_SUPPORT_RESOLVE_ID) &&
        (sessionp->session_misc_flags & SMBV_HAS_FILEIDS)) {
        /*
         * Supports Resolve ID.
         * First check to see if we already have the vnode
         */
        if ((ino == SMBFS_ROOT_INO) || (ino == SMBFS_ROOT_PAR_INO)) {
            /* Get the root vnode */
            error = smbfs_root(mp, vpp, context);
            goto done;
        }
        else {
            /* 
             * Some other vnode
             */
            fap->fa_ino = ino;
            
            /*
             * fa_reqtime is not used as we are just checking to see if we
             * already have this vnode in the hash table.
             */
            nanouptime(&fap->fa_reqtime);
            
            /* 
             * Since we only have the ino in the fap, if we do find an existing
             * vnode, dont update its meta data
             */
            if (smbfs_nget(share, mp,
                           NULL, NULL, 0,
                           fap, vpp,
                           0, (SMBFS_NGET_LOOKUP_ONLY | SMBFS_NGET_NO_CACHE_UPDATE),
                           context) == 0) {
                /*
                 * Found one in our hash table. Unlock it and return it 
                 */
                error = 0;
                smbnode_unlock(VTOSMB(*vpp));
                goto done;
            }
        }
           
        /*
         * Not already in our hash table.
         * Do Resolve ID to server to see if server can find the item.  If so
         * it will return the path from the share to the item.
         *
         * Need root vnode to do the Resolve ID call on
         */
        error = smbfs_root(mp, &root_vp, context);
        if (error) {
            SMBDEBUG("smbfs_root failed %d\n", error);
            goto done;
        }
        root_np = VTOSMB(root_vp);

        error = smb2fs_smb_cmpd_resolve_id(share, root_np,
                                           ino, &resolve_error, &server_path, &server_path_allocsize,
                                           context);
        if (error) {
            goto done;
        }
        
        if (resolve_error) {
            error = resolve_error;
            goto done;
        }
       
        if (server_path == NULL) {
            error = ENOENT;
            goto done;
        }
		
		/*
         * Build the local path to the item starting with mount point 
         */
        if (strlcpy(path, vfs_statfs(mp)->f_mntonname, path_max) >= path_max) {
            /* Should not happen */
            SMBDEBUG("path too long <%s>\n", vfs_statfs(mp)->f_mntonname);
            error = ENAMETOOLONG;
            goto done;
        }
        path_max = MAXPATHLEN - strlen(path);

		/* Handle Submounts */
		if (smp->sm_args.local_path_len != 0) {
			/* 
			 * Submount path should be something like "subfolder1/subfolder2"
			 * Server path should be something like "subfolder1/subfolder2/item1"
			 * After subtracting the submount, server path is now "/item1"
			 */
			if (strlen(server_path) < smp->sm_args.local_path_len) {
				/* Should be impossible */
				SMBERROR("Invalid server path len %zu < %zu submount path len \n",
						 strlen(server_path), smp->sm_args.local_path_len);
				error = ENOENT;
				goto done;
			}
			
			if (strlcat(path, server_path + smp->sm_args.local_path_len, path_max) >= path_max) {
				SMBDEBUG("path too long <%s> + <%s>\n", path, server_path);
				error = ENAMETOOLONG;
				goto done;
			}
		}
		else {
			if (strlcat(path, "/", path_max) >= path_max) {
				/* Should not happen */
				SMBDEBUG("path too long <%s> + <%s>\n", path, "/");
				error = ENAMETOOLONG;
				goto done;
			}
			path_max = MAXPATHLEN - strlen(path);
			
			if (strlcat(path, server_path, path_max) >= path_max) {
				SMBDEBUG("path too long <%s> + <%s>\n", path, server_path);
				error = ENAMETOOLONG;
				goto done;
			}
		}
		
        /*
         * Follow that path to find the vnode
         */
        error = vnode_lookup (path, VNODE_LOOKUP_CROSSMOUNTNOWAIT, vpp, context);
        if (error) {
            SMBDEBUG("vnode_lookup failed %d\n", error);
            goto done;
        }
        
        /* make sure it is one of my vnodes */
        if (vnode_tag(*vpp) != VT_CIFS) {
            SMBDEBUG("vnode_lookup found non SMB vnode???\n");
            /*
             * <80744043> vnode_lookup was successful and it returns with an
             * iocount held on the resulting vnode which must be dropped with
             * vnode_put(). When we return an error, the caller will not drop
             * the iocount so we must do it to avoid iocount leak.
             */
            vnode_put(*vpp);
            error = ENOENT;
            goto done;
        }
    }
    else {
        error = ENOTSUP;
    }
    
done:    
    if (root_vp) {
        vnode_put(root_vp);
    }

    if (server_path) {
        SMB_FREE_DATA(server_path, server_path_allocsize);
    }

    if (fap) {
        SMB_FREE_TYPE(struct smbfattr, fap);
    }
    
    if (path) {
        SMB_FREE_DATA(path, MAXPATHLEN);
    }

    /* We only have a share if we took a reference, release it */
    if (share) {
        smb_share_rele(share, context);
    }
    
    SMB_LOG_KTRACE(SMB_DBG_VGET | DBG_FUNC_END, error, 0, 0, 0, 0);
    return (error);
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
	int error = 0;
	struct sysctl_req *req = NULL;
	struct mount *mp = NULL;
	struct smbmount *smp = NULL;
	struct vfsquery vq;
	int32_t dev = 0;
	struct smb_remount_info info;
	struct smb_share *share;
	struct vfs_server v_server;
	uint count;
	uint totlen;
	uint numThreads;
	struct netfs_status *nsp = NULL;
	struct smb_session *sessionp = NULL;
	struct smb_rq *rqp, *trqp;
	
    SMB_LOG_KTRACE(SMB_DBG_SYSCTL | DBG_FUNC_START, name[0], 0, 0, 0, 0);

	/*
	 * All names at this level are terminal.
	 */
	if (namelen > 1) {
		error = ENOTDIR;	/* overloaded */
        goto done;
    }

	switch (name[0]) {
		case SMBFS_SYSCTL_GET_SERVER_SHARE:
		case SMBFS_SYSCTL_REMOUNT_INFO:
		case SMBFS_SYSCTL_REMOUNT:
		case VFS_CTL_QUERY:
		case VFS_CTL_SADDR:
		case VFS_CTL_DISC:
		case VFS_CTL_SERVERINFO:
		case VFS_CTL_NSTATUS:
		{
			boolean_t is_64_bit = vfs_context_is64bit(context);
			union union_vfsidctl vc;
			extern struct vfsops smbfs_vfsops;


			req = CAST_DOWN(struct sysctl_req *, oldp);
			error = SYSCTL_IN(req, &vc, is_64_bit ? sizeof(vc.vc64) : sizeof(vc.vc32));
			if (error) {
				break;
			}
			mp = vfs_getvfs_with_vfsops(&vc.vc32.vc_fsid, &smbfs_vfsops); /* works for 32 and 64 */
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
			/* VFS handles the rest. */
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
			len = strnlen(SS_TO_SESSION(share)->session_srvname, SMB_MAX_DNS_SRVNAMELEN);
			len += 1; /* Slash */
			len += strnlen(share->ss_name, SMB_MAXSHARENAMELEN);
			len += 1; /* null byte */
            SMB_MALLOC_DATA(serverShareStr, len, Z_WAITOK_ZERO);
			strlcpy(serverShareStr, SS_TO_SESSION(share)->session_srvname, len);
			strlcat(serverShareStr, "/", len);
			strlcat(serverShareStr, share->ss_name, len);
			smb_share_rele(share, context);
			error = SYSCTL_OUT(req, serverShareStr, len);
            SMB_FREE_DATA(serverShareStr, len);
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
                
                /* Check if we have any pending svrmsg replies */
                if (smp->sm_svrmsg_pending) {
                    vq.vq_flags |= VQ_SERVEREVENT;
                }
			}
            
            SMB_LOG_KTRACE(SMB_DBG_SYSCTL | DBG_FUNC_NONE,
                           0xabc001, vq.vq_flags, 0, 0, 0);
            
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

                /* <72239144> Return original server IP address that was used */
                if (SS_TO_SESSION(share)->session_saddr->sa_family == AF_NETBIOS) {
                    /* NetBIOS sockaddr get the real IPv4 sockaddr */
                    saddr = (struct sockaddr *)
                        &((struct sockaddr_nb *) SS_TO_SESSION(share)->session_saddr)->snb_addrin;
                }
                else {
                    /* IPv4 or IPv6 sockaddr */
                    saddr = SS_TO_SESSION(share)->session_saddr;
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
                
                SMB_LOG_KTRACE(SMB_DBG_SYSCTL | DBG_FUNC_NONE,
                               0xabc002, error, 0, 0, 0);
                
                if (error != EBUSY) {
                    SMBDEBUG("VFS_CTL_DISC unmounting\n");
                    /* ok to immediately be unmounted */
					share->ss_dead(share);
                }
                
				smb_share_rele(share, context);
            }
            
            break;
            
        case VFS_CTL_SERVERINFO:
        {
            /* Fill in the server name */
            size_t len;
            share = smb_get_share_with_reference(smp);
            len = strnlen(SS_TO_SESSION(share)->session_srvname, SMB_MAX_DNS_SRVNAMELEN);
            strlcpy((char *)v_server.vs_server_name, SS_TO_SESSION(share)->session_srvname, len + 1);
                
            if (smp->sm_svrmsg_pending) {
                /* Fill in shutdown delay */
                if (smp->sm_svrmsg_pending & SVRMSG_RCVD_GOING_DOWN) {
                    /* Set the delay (in minutes) we received from the server */
                    v_server.vs_minutes = smp->sm_svrmsg_shutdown_delay / 60;
                    
                    smp->sm_svrmsg_pending &= ~SVRMSG_RCVD_GOING_DOWN;
                } else if (smp->sm_svrmsg_pending & SVRMSG_RCVD_SHUTDOWN_CANCEL) {
                    /* Delay = 0xfff means server is staying up */
                    v_server.vs_minutes = 0xfff;
                    smp->sm_svrmsg_pending &= ~SVRMSG_RCVD_SHUTDOWN_CANCEL;
                }
            } else {
                /*
                 * No server events to report.
                 * Don't return an error, otherwise nothing gets passed back.
                 * Use -1 for v_minutes to indicate an error.
                 */
                v_server.vs_minutes = -1;
            }
            smb_share_rele(share, context);
            
            error = SYSCTL_OUT(req, &v_server, sizeof(v_server));
            
            break;
        }
            
        case VFS_CTL_NSTATUS:
            /* grab a reference until we're done with the share */
            share  = smb_get_share_with_reference(smp);
            
            /* Get access to the session underneath */
            sessionp = SS_TO_SESSION(share);
            
            /* Round up the threads */
            numThreads = 0;
            SMB_IOD_RQLOCK(sessionp->session_iod);
            TAILQ_FOREACH_SAFE(rqp, &(sessionp->session_iod)->iod_rqlist, sr_link, trqp) {
                /* Only count threads for this share and
                 * they're not internal or async threads */
                if  ((rqp->sr_share == share) &&
                     !(rqp->sr_flags & (SMBR_ASYNC | SMBR_INTERNAL))) {
                    numThreads++;
                }
            }
            SMB_IOD_RQUNLOCK(sessionp->session_iod);
            
            /* Calculate total size of result buffer */
            totlen = sizeof(struct netfs_status) + (numThreads * sizeof(uint64_t));
            
            if (req->oldptr == USER_ADDR_NULL) { // Caller is querying buffer size
                smb_share_rele(share, context);
                return SYSCTL_OUT(req, NULL, totlen);
            }
            
            SMB_MALLOC_DATA(nsp, totlen, Z_WAITOK_ZERO);
            if (nsp == NULL) {
                smb_share_rele(share, context);
                return ENOMEM;
            }
            
            if (smp->sm_status & SM_STATUS_DOWN) {
                nsp->ns_status |= VQ_NOTRESP;
            }
            if (smp->sm_status & SM_STATUS_DEAD) {
                nsp->ns_status |= VQ_DEAD;
            }
            
            /* Return some significant mount options as a string
             * e.g. "rw,soft,vers=0x0302,sec=kerberos,timeout=20 */
            snprintf(nsp->ns_mountopts, sizeof(nsp->ns_mountopts),
                     "%s,%s,vers=0x%X,sec=%s,timeout=%d",
                     (vfs_flags(mp) & MNT_RDONLY) ? "ro" : "rw",
                     (smp->sm_args.altflags & SMBFS_MNT_SOFT) ? "soft" : "hard",
                     sessionp->session_sopt.sv_dialect ? sessionp->session_sopt.sv_dialect : 0x01,
                     (sessionp->session_flags & SMBV_GUEST_ACCESS)        ? "guest" :
                     (sessionp->session_flags & SMBV_PRIV_GUEST_ACCESS)   ? "privguest" :
                     (sessionp->session_flags & SMBV_ANONYMOUS_ACCESS)    ? "anonymous" :
                     (sessionp->session_flags & SMBV_KERBEROS_ACCESS)     ? "kerberos" :
                     (sessionp->session_sopt.sv_caps & SMB_CAP_EXT_SECURITY)       ? "gss" : "unknown",
                     (smp->sm_args.altflags & SMBFS_MNT_SOFT) ? share->ss_soft_timer : (share->ss_dead_timer + sessionp->session_resp_wait_timeout));
            
            nsp->ns_threadcount = numThreads;
            
            /* 
             * Get the thread ids of threads waiting for a reply
             * and find the longest wait time.
             */
            if (numThreads > 0) {
                struct timespec now;
                time_t sendtime;
                
                nanouptime(&now);
                count = 0;
                sendtime = now.tv_sec;
                SMB_IOD_RQLOCK(sessionp->session_iod);
                TAILQ_FOREACH_SAFE(rqp, &(sessionp->session_iod)->iod_rqlist, sr_link, trqp) {
                    /* Only count threads for this share if they're not internal threads */
                    if  ((rqp->sr_share == share) &&
                         !(rqp->sr_flags & (SMBR_ASYNC | SMBR_INTERNAL))) {
                        if ((rqp->sr_state & ~SMBRQ_NOTSENT) && rqp->sr_timesent.tv_sec < sendtime) {
                            sendtime = rqp->sr_timesent.tv_sec;
                        }
                        nsp->ns_threadids[count] = rqp->sr_threadId;
                        if (++count >= numThreads)
                            break;
                    }
                }
                nsp->ns_waittime = (uint32_t)(now.tv_sec - sendtime);
                SMB_IOD_RQUNLOCK(sessionp->session_iod);
            }
            
            smb_share_rele(share, context);
            
            error = SYSCTL_OUT(req, nsp, totlen);
            break;
            
	    default:
			error = ENOTSUP;
			break;
	}
    
done:
	if (error) {
		SMBWARNING("name[0] = %d error = %d\n", name[0], error);
	}
    
    SMB_LOG_KTRACE(SMB_DBG_SYSCTL | DBG_FUNC_END, error, 0, 0, 0, 0);
	return (error);
}

static char smbfs_name[MFSNAMELEN] = "smbfs";

kmod_info_t *smbfs_kmod_infop;

typedef int (*PFI)(void);

extern struct vnodeopv_desc smbfs_vnodeop_opv_desc;
static struct vnodeopv_desc *smbfs_vnodeop_opv_desc_list[1] =
{
	&smbfs_vnodeop_opv_desc
};


extern int version_major;
extern int version_minor;

static vfstable_t  smbfs_vfsconf;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-field-initializers"

struct vfsops smbfs_vfsops = {
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
	// Remaining ops unused
};

#pragma clang diagnostic pop

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
    if (error) {
        /* Should never happen */
        SMBERROR("vfs_fsadd failed with %d \n", error);
		goto out;
    }

    /* Initialize all the mutexes */
	smbnet_lock_init();	/* Initialize the network locks */
    smbfs_lock_init();  /* Initialize file system locks */

    /* Initialize and start the rw threads */
    smb_rw_init();

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

	sysctl_register_oid(&sysctl__net_smb_fs_maxwrite);
	sysctl_register_oid(&sysctl__net_smb_fs_maxread);

	sysctl_register_oid(&sysctl__net_smb_fs_maxsegreadsize);
	sysctl_register_oid(&sysctl__net_smb_fs_maxsegwritesize);

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

    smbfs_remove_sleep_wake_notifier();

    sysctl_unregister_oid(&sysctl__net_smb_fs_maxsegreadsize);
	sysctl_unregister_oid(&sysctl__net_smb_fs_maxsegwritesize);

	sysctl_unregister_oid(&sysctl__net_smb_fs_maxwrite);
	sysctl_unregister_oid(&sysctl__net_smb_fs_maxread);

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

    /* Halt and free the read/write threads/queue */
    smb_rw_cleanup();

	lck_mtx_destroy(&mc_notifier_lck, mc_notifier_lck_grp);
    /* Free all the mutexes */
	smbfs_lock_uninit();	/* Free up the file system locks */
	smbnet_lock_uninit();	/* Free up the network locks */
	
out:	
	return (error ? KERN_FAILURE : KERN_SUCCESS);
}
