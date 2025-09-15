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
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/ioccom.h>
#include <sys/malloc.h>
#include <sys/uio.h>
#include <sys/conf.h>
#include <sys/kpi_mbuf.h>
#include <sys/proc.h>
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>

#include <sys/kauth.h>

#include <net/if.h>
#include <netinet/in.h>
#include <sys/smb_apple.h>
#include <sys/smb_byte_order.h>
#include <sys/mchain.h>

#include <netsmb/smb.h>
#include <netsmb/smb_2.h>
#include <netsmb/smb_rq.h>
#include <netsmb/smb_rq_2.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_conn_2.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_dev.h>
#include <netsmb/smb_dev_2.h>
#include <netsmb/smb_tran.h>
#include <netsmb/smb2_mc_support.h>

#include <iokit/smbfs_iokit.h>

/*
 * Userland code loops through minor #s 0 to 1023, looking for one which opens.
 * Intially we create minor 0 and leave it for anyone.  Minor zero will never
 * actually get used - opening triggers creation of another (but private) minor,
 * which userland code will get to and mark busy.
 */
#define SMBMINORS 1024
struct smb_dev * smb_dtab[SMBMINORS];
int smb_minor_hiwat = -1;
#define SMB_GETDEV(dev)         (minor(dev) >= SMBMINORS? NULL: (smb_dtab[minor(dev)]))
#define SMB_SETDEV(dev, sdp) \
do { \
    if (minor(dev) < SMBMINORS) \
           smb_dtab[minor(dev)] = sdp; \
} while (0)

static d_open_t	 nsmb_dev_open;
static d_close_t nsmb_dev_close;
static d_ioctl_t nsmb_dev_ioctl;

static struct cdevsw nsmb_cdevsw = {
	nsmb_dev_open,
	nsmb_dev_close,
	eno_rdwrt,	/* d_read */
	eno_rdwrt,	/* d_write */
	nsmb_dev_ioctl,
	eno_stop,
	eno_reset,
	0,		/* struct tty ** d_ttys */
	eno_select,
	eno_mmap,
	eno_strat,
	eno_getc,
	eno_putc,
	0		/* d_type */
};

int	smb_major = -1;

extern lck_rw_t  * dev_rw_lck;
extern lck_grp_t  * dev_lck_grp;
extern lck_attr_t * dev_lck_attr;

extern int dev_open_cnt;
extern int unloadInProgress;

pid_t mc_notifier_pid = -1;
extern lck_mtx_t mc_notifier_lck;

extern struct smb_connobj smb_session_list;

static int
nsmb_dev_open_nolock(dev_t dev, int oflags, int devtype, struct proc *p)
{
#pragma unused(oflags, devtype, p)
	struct smb_dev *sdp;
	kauth_cred_t cred = vfs_context_ucred(vfs_context_current());

	sdp = SMB_GETDEV(dev);
	if (sdp && (sdp->sd_flags & NSMBFL_OPEN))
		return (EBUSY);
	if (!sdp || minor(dev) == 0) {
		int	avail_minor;

		for (avail_minor = 1; avail_minor < SMBMINORS; avail_minor++)
			if (!SMB_GETDEV(avail_minor))
				break;
		if (avail_minor >= SMBMINORS) {
			SMBERROR("Too many minor devices, %d >= %d !", avail_minor, SMBMINORS);
			return (ENOMEM);
		}
        SMB_MALLOC_TYPE(sdp, struct smb_dev, Z_WAITOK);
		bzero(sdp, sizeof(*sdp));
		dev = makedev(smb_major, avail_minor);
		sdp->sd_devfs = devfs_make_node(dev, DEVFS_CHAR,
						kauth_cred_getuid(cred),
						kauth_cred_getgid(cred),
						0700, "nsmb%x", avail_minor);
		if (!sdp->sd_devfs) {
			SMBERROR("devfs_make_node failed %d\n", avail_minor);
            SMB_FREE_TYPE(struct smb_dev, sdp);
			return (ENOMEM);
		}
		if (avail_minor > smb_minor_hiwat)
			smb_minor_hiwat = avail_minor;
        SMB_SETDEV(dev, sdp);
		return (EBUSY);
	}
	lck_rw_init(&sdp->sd_rwlock, dev_lck_grp, dev_lck_attr);
	sdp->sd_flags |= NSMBFL_OPEN;
	dev_open_cnt++;
	return (0);
}

static int
nsmb_dev_open(dev_t dev, int oflags, int devtype, struct proc *p)
{
    int error;

    /* Just some sanity checks for debug purposes only */
    DBG_ASSERT(sizeof(struct smbioc_negotiate) < SMB_MAX_IOC_SIZE);
    DBG_ASSERT(sizeof(struct smbioc_setup) < SMB_MAX_IOC_SIZE);
    DBG_ASSERT(sizeof(struct smbioc_share) < SMB_MAX_IOC_SIZE);
    DBG_ASSERT(sizeof(struct smbioc_rq) < SMB_MAX_IOC_SIZE);
    DBG_ASSERT(sizeof(struct smbioc_t2rq) < SMB_MAX_IOC_SIZE);
    DBG_ASSERT(sizeof(struct smbioc_rw) < SMB_MAX_IOC_SIZE);

    lck_rw_lock_exclusive(dev_rw_lck);

    if (! unloadInProgress) {
        error = nsmb_dev_open_nolock(dev, oflags, devtype, p);
    }
    else {
        SMBERROR("We are being unloaded\n");
        error = EBUSY;
    }

    lck_rw_unlock_exclusive(dev_rw_lck);
    return (error);
}

static int
nsmb_dev_close(dev_t dev, int flag, int fmt, struct proc *p)
{
#pragma unused(flag, fmt, p)
	struct smb_dev *sdp;
	struct smb_session *sessionp;
	struct smb_share *share;
	vfs_context_t context;

 	lck_rw_lock_exclusive(dev_rw_lck);
 	sdp = SMB_GETDEV(dev);
	if ((sdp == NULL) || ((sdp->sd_flags & NSMBFL_OPEN) == 0)) {
		lck_rw_unlock_exclusive(dev_rw_lck);
		return (EBADF);
    }
    
	context = vfs_context_create((vfs_context_t)0);
    
	/* make sure any ioctls have finished before proceeding */
	lck_rw_lock_exclusive(&sdp->sd_rwlock);
    
	share = sdp->sd_share;
	sdp->sd_share = NULL; /* Just to be extra careful */
	if (share != NULL) {
		smb_share_rele(share, context);
	}

	sessionp = sdp->sd_session;
	sdp->sd_session = NULL; /* Just to be extra careful */
	if (sessionp != NULL) 
		smb_session_rele(sessionp, context);
    
	lck_rw_unlock_exclusive(&sdp->sd_rwlock);

	devfs_remove(sdp->sd_devfs); /* first disallow opens */

	vfs_context_rele(context);

    SMB_SETDEV(dev, NULL);
	lck_rw_destroy(&sdp->sd_rwlock, dev_lck_grp);
    SMB_FREE_TYPE(struct smb_dev, sdp);
	dev_open_cnt--;

	lck_rw_unlock_exclusive(dev_rw_lck);
	return (0);
}

static int nsmb_dev_ioctl(dev_t dev, u_long cmd, caddr_t data, int flag, 
						  struct proc *p)
{
#pragma unused(flag)
	struct smb_dev *sdp;
	struct smb_session *sessionp;
	struct smb_share *sharep;
	uint32_t error = 0;
	vfs_context_t context;

	/* 
	 * We allow mutiple ioctl calls, but never when opening, closing or 
	 * getting the mount device. dev_rw_lck is used to keep the dev list
     * from changing as we get the sdp from the dev. Lock dev_rw_lck first, 
     * then get the sdp and then get the lock on sd_rwlock. sd_rwlock is 
     * held when an ioctl call is still in progress and keeps us from closing 
     * the dev with the outstanding ioctl call.
	 */
	lck_rw_lock_shared(dev_rw_lck);
	sdp = SMB_GETDEV(dev);
	if ((sdp == NULL) || ((sdp->sd_flags & NSMBFL_OPEN) == 0)) {
		error = EBADF;
        lck_rw_unlock_shared(dev_rw_lck);
		goto exit;
	}
    
	context = vfs_context_create((vfs_context_t)0);
    
	/* 
	  *%%% K64 
	 * Need to keep checking to see if this gets corrected. The problem here
	 * is ioctl_cmd_t is uint32_t on K64 builds. The _IO defines use sizeof
	 * which returns a size_t. Hopefully either cmd will be changed to u_long
	 * or the _IO defines will have sizeof typed cast to uint32_t.
	 */
	switch (cmd) {
		case SMBIOC_FIND_SESSION:
		case SMBIOC_NEGOTIATE:
		{
			int searchOnly = (cmd == SMBIOC_FIND_SESSION) ? TRUE : FALSE;
			struct smbioc_negotiate * vspec = (struct smbioc_negotiate *)data;
            struct sockaddr *saddr = NULL, *laddr = NULL;
            int32_t saddr_allocsize = 0, laddr_allocsize = 0;

			/* protect against anyone else playing with the smb dev structure */
			lck_rw_lock_exclusive(&sdp->sd_rwlock);

            /* free global lock now since we now have sd_rwlock */
            lck_rw_unlock_shared(dev_rw_lck);

            /* Make sure the version matches */
			if (vspec->ioc_version != SMB_IOC_STRUCT_VERSION) {
				error = EINVAL;
			} else if (sdp->sd_session || sdp->sd_share) {
				error = EISCONN;
			} else {
                /* Convert any pointers over to using user_addr_t */
                if (! vfs_context_is64bit (context)) {
                    vspec->ioc_kern_saddr = CAST_USER_ADDR_T(vspec->ioc_saddr);
                    vspec->ioc_kern_laddr = CAST_USER_ADDR_T(vspec->ioc_laddr);
                }

                if (vspec->ioc_saddr_len < 3) {
                    /* Paranoid check - Have to have at least sa_len and sa_family */
                    SMBERROR("invalid ioc_saddr_len (%d) \n", vspec->ioc_saddr_len);
                    error = EINVAL;
                    goto ioc_negotiate_error;
                }

                saddr = smb_memdupin(vspec->ioc_kern_saddr, vspec->ioc_saddr_len);
                saddr_allocsize = vspec->ioc_saddr_len;
                if (saddr == NULL) {
                    error = ENOMEM;
                    goto ioc_negotiate_error;
                }

                if (saddr->sa_len != vspec->ioc_saddr_len) {
                    SMBERROR("invalid parameter ioc_saddr->sa_len (%u) != sizeof(ioc_saddr_len) (%lu)",
                             saddr->sa_len, sizeof(*saddr));
                    error = EINVAL;
                    goto ioc_negotiate_error;
                }

                if (vspec->ioc_laddr_len) {
                    if (vspec->ioc_laddr_len < 3) {
                        /* Paranoid check - Have to have at least sa_len and sa_family */
                        SMBERROR("invalid ioc_laddr_len (%d) \n", vspec->ioc_laddr_len);
                        error = EINVAL;
                        goto ioc_negotiate_error;
                    }

                    laddr = smb_memdupin(vspec->ioc_kern_laddr, vspec->ioc_laddr_len);
                    laddr_allocsize = vspec->ioc_laddr_len;
                    if (laddr == NULL) {
                        error = ENOMEM;
                        goto ioc_negotiate_error;
                    }

                    if (laddr->sa_len != vspec->ioc_laddr_len) {
                        SMBERROR("invalid parameter ioc_laddr->sa_len (%u) != sizeof(ioc_laddr_len) (%lu)",
                                 saddr->sa_len, sizeof(*saddr));
                        error = EINVAL;
                        goto ioc_negotiate_error;
                    }
                }

                /*
                 * <84091372> All user mode memory should get copyin before we call the
                 * smb_usr_negotiate. To make sure usrptrs will not be used by it in
                 * the future, we temporary set this pointers to NULL.
                 */
                struct sockaddr * tmp_ioc_saddr = vspec->ioc_saddr;
                struct sockaddr * tmp_ioc_laddr = vspec->ioc_laddr;

                vspec->ioc_saddr = NULL;
                vspec->ioc_laddr = NULL;

                error = smb_usr_negotiate(vspec, context, sdp, searchOnly, saddr, laddr);

                vspec->ioc_saddr = tmp_ioc_saddr;
                vspec->ioc_laddr = tmp_ioc_laddr;
			}
ioc_negotiate_error:
            if (saddr) {
                SMB_FREE_DATA(saddr, saddr_allocsize);
            }
            if (laddr) {
                SMB_FREE_DATA(laddr, laddr_allocsize);
            }
            lck_rw_unlock_exclusive(&sdp->sd_rwlock);
            break;
		}
        case SMBIOC_UPDATE_CLIENT_INTERFACES:
        {
            uint32_t flags = 0;
            SMBDEBUG("SMBIOC_UPDATE_CLIENT_INTERFACES received.\n");
            lck_rw_lock_shared(&sdp->sd_rwlock);
            
            /* free global lock now since we now have sd_rwlock */
            lck_rw_unlock_shared(dev_rw_lck);
            
            struct smbioc_client_interface* client_info = (struct smbioc_client_interface*)data;
            sessionp = sdp->sd_session;
            if (sessionp) {
                if (sessionp->session_misc_flags & SMBV_MC_CLIENT_RSS_FORCE_ON) {
                    flags |= CLIENT_RSS_FORCE_ON;
                }
                error =  smb2_mc_parse_client_interface_array(&sessionp->session_interface_table,
                                                              client_info, flags);
            } else {
                SMBERROR("Invalid sessionp.\n");
                error = EINVAL;
            }
            lck_rw_unlock_shared(&sdp->sd_rwlock);
            break;
        }
        case SMBIOC_UPDATE_NOTIFIER_PID:
        {
            SMBDEBUG("SMBIOC_UPDATE_NOTIFIER_PID received.\n");
            if (!smbfs_is_task_entitled_to(proc_task(p), SMBIOC_UPDATE_NOTIFITER_PID_ENTITLEMENT)) {
                lck_rw_unlock_shared(dev_rw_lck);
                SMBERROR("SMBIOC_UPDATE_NOTIFIER_PID needs entitlement");
                error = EPERM;
                break;
            }

            lck_rw_lock_shared(&sdp->sd_rwlock);

            /* free global lock now since we now have sd_rwlock */
            lck_rw_unlock_shared(dev_rw_lck);
            lck_mtx_lock(&mc_notifier_lck);

            struct smbioc_notifier_pid* notifier_info = (struct smbioc_notifier_pid*)data;
            /*
             * Check if we already have an open notifier
             * In that case prevent from opening more then 1
             */
            if (mc_notifier_pid != -1) {
                char name[1024];
                proc_name(mc_notifier_pid, (char*) &name, 1024);
                if (!strncmp((char*) &name, "mc_notifier", 11)) {
                    error = EEXIST;
                }
            } else {
                mc_notifier_pid = notifier_info->pid;
            }

            lck_mtx_unlock(&mc_notifier_lck);
            lck_rw_unlock_shared(&sdp->sd_rwlock);
            break;
        }
        case SMBIOC_NOTIFIER_UPDATE_INTERFACES:
        {
            uint32_t flags = 0;
            SMB_LOG_MC("SMBIOC_NOTIFIER_UPDATE_INTERFACES received.\n");
            lck_rw_lock_shared(&sdp->sd_rwlock);

            /* free global lock now since we now have sd_rwlock */
            lck_rw_unlock_shared(dev_rw_lck);

            struct smbioc_client_interface* client_info = (struct smbioc_client_interface*)data;

            smb_sm_lock_session_list();

            /* For every open session we want to update the client interface list */
            struct smb_session *sessionp, *tsessionp;
            SMBCO_FOREACH_SAFE(sessionp, &smb_session_list, tsessionp) {

                error = smb_session_lock(sessionp);
                if (error != 0) {
                    /* Can happen with bad servers */
                    client_info->ioc_errno = error;
                    SMBDEBUG("smb_session_lock returned error %d\n", error);
                    break;
                }

                if (sessionp->session_misc_flags & SMBV_MC_CLIENT_RSS_FORCE_ON) {
                    flags |= CLIENT_RSS_FORCE_ON;
                }

                error = smb2_mc_notifier_event(&sessionp->session_interface_table, client_info,
                                               sessionp->max_channels,
                                               sessionp->srvr_rss_channels,
                                               sessionp->clnt_rss_channels,
                                               flags);

                if (!error) {
                    struct smbiod *iod = NULL;
                    
                    /*
                     * echo inactive channel if there is such one.
                     * we do the echo to detect client interface, used by
                     * inactive channel, going down.
                     * active channel interfaces going down should be detected
                     * by the usage of the channel so no need to echo.
                     * assume there is only one inactive channel here instead
                     * of interating.
                     */
                    error = smb_iod_get_non_main_iod(sessionp, &iod, __FUNCTION__, 1);

                    if ((error == 0) && iod) {
                        (void)smb_iod_request(iod, SMBIOD_EV_ECHO | SMBIOD_EV_SYNC, NULL);
                        smb_iod_rel(iod, NULL, __FUNCTION__);
                    }

                    error = smb_iod_get_main_iod(sessionp, &iod, __FUNCTION__);
                    if (error) {
                        SMBERROR("smb_iod_get_main_iod failed %d \n", error);
                    }
                    else {
                        error = smb_iod_establish_alt_ch(iod);
                        smb_iod_rel(iod, NULL, __FUNCTION__);
                    }
                }

                smb_session_unlock(sessionp);
                if (error != 0) {
                    SMBDEBUG("smb2_mc_update_client_interface_array returned error %d\n", error);
                    client_info->ioc_errno = error;
                    break;
                }
            }

            smb_sm_unlock_session_list();

            lck_rw_unlock_shared(&sdp->sd_rwlock);
            break;
        }
        case SMBIOC_GET_NOTIFIER_PID:
        {
            lck_rw_lock_shared(&sdp->sd_rwlock);
            
            /* free global lock now since we now have sd_rwlock */
            lck_rw_unlock_shared(dev_rw_lck);
            lck_mtx_lock(&mc_notifier_lck);

            char name[1024];
            proc_name(mc_notifier_pid, (char*) &name, 1024);
            if (strncmp((char*) &name, "mc_notifier", 11)) {
                mc_notifier_pid = -1;
            }
            
            struct smbioc_notifier_pid* notifier_info = (struct smbioc_notifier_pid*)data;
            notifier_info->pid = mc_notifier_pid;

            lck_mtx_unlock(&mc_notifier_lck);
            lck_rw_unlock_shared(&sdp->sd_rwlock);
            break;
        }
		case SMBIOC_NTWRK_IDENTITY:
		{
			struct smbioc_ntwrk_identity * ntwrkID = (struct smbioc_ntwrk_identity *)data;

			lck_rw_lock_shared(&sdp->sd_rwlock);
            
            /* free global lock now since we now have sd_rwlock */
            lck_rw_unlock_shared(dev_rw_lck);

			/* Make sure the version matches */
			if (ntwrkID->ioc_version != SMB_IOC_STRUCT_VERSION) {
				error = EINVAL;
			} else if (!sdp->sd_session) {
				error = ENOTCONN;
			} else {
				error = smb_usr_set_network_identity(sdp->sd_session, ntwrkID);
			}
            
			lck_rw_unlock_shared(&sdp->sd_rwlock);
			break;
		}
		case SMBIOC_SSNSETUP: 
		{
			struct smbioc_setup * sspec = (struct smbioc_setup *)data;
			
			lck_rw_lock_shared(&sdp->sd_rwlock);
            
            /* free global lock now since we now have sd_rwlock */
            lck_rw_unlock_shared(dev_rw_lck);

			/* Make sure the version matches */
			if (sspec->ioc_version != SMB_IOC_STRUCT_VERSION) {
				error = EINVAL;
			} else if (sdp->sd_share) {
				error = EISCONN;
			} else if (!sdp->sd_session) {
				error = ENOTCONN;
			} else {
				error = smb_sm_ssnsetup(sdp->sd_session, sspec, context);
			}
            
			lck_rw_unlock_shared(&sdp->sd_rwlock);
			break;
		}
		case SMBIOC_CONVERT_PATH:
		{
			struct smbioc_path_convert * dp = (struct smbioc_path_convert *)data;
			
			lck_rw_lock_shared(&sdp->sd_rwlock);
            
            /* free global lock now since we now have sd_rwlock */
            lck_rw_unlock_shared(dev_rw_lck);

			/* Make sure the version matches */
			if (dp->ioc_version != SMB_IOC_STRUCT_VERSION) {
				error = EINVAL;
			} else if (!sdp->sd_session) {
				error = ENOTCONN;
			} else {
				/* Take the 32 bit world pointers and convert them to user_addr_t. */
				if (! vfs_context_is64bit (context)) {
					dp->ioc_kern_src = CAST_USER_ADDR_T(dp->ioc_src);
					dp->ioc_kern_dest = CAST_USER_ADDR_T(dp->ioc_dest);
				}
				if (!dp->ioc_kern_src || !dp->ioc_kern_dest) {
					error = EINVAL;
				} else if (((dp->ioc_direction & (LOCAL_TO_NETWORK | NETWORK_TO_LOCAL)) == 0) ||
					((dp->ioc_direction & LOCAL_TO_NETWORK) && (dp->ioc_direction & NETWORK_TO_LOCAL))) {
					/* Need to have one set and you can't have both set */
					error = EINVAL;
                } else {
                    char *srcp = NULL;
                    char *dstp = NULL;
                    uint64_t dst_len;
                    uint64_t dst_alloclen = 0, src_alloclen = dp->ioc_src_len;
                    srcp = smb_memdupin(dp->ioc_kern_src, (uint32_t)src_alloclen);
                    if (srcp == NULL) {
                        SMBERROR("failed to dupin ioc_kern_src\n");
                        error = ENOMEM;
                        goto ioc_convert_path_error;
                    }

                    dst_alloclen = dp->ioc_dest_len;
                    SMB_MALLOC_DATA(dstp, dst_alloclen, Z_WAITOK_ZERO);

                    /*
                     * <82309348> All user mode memory should get copyin before we call
                     * smb_usr_convert_path_to_network or smb_usr_convert_network_to_path.
                     * To make sure usrptrs will not be used by it in the future, we
                     * temporary set this pointers to NULL.
                     */
                    const char* tmp_ioc_src = dp->ioc_src;
                    const char* tmp_ioc_dst = dp->ioc_dest;

                    dp->ioc_src = NULL;
                    dp->ioc_dest = NULL;
                    dst_len = dp->ioc_dest_len;
                    if (dp->ioc_direction & LOCAL_TO_NETWORK) {
                        error = smb_usr_convert_path_to_network(sdp->sd_session, srcp, dp->ioc_src_len, dstp,
                                                                &dst_len, dp->ioc_flags);
                    } else {
                        error = smb_usr_convert_network_to_path(sdp->sd_session, srcp, dp->ioc_src_len, dstp,
                                                                &dst_len, dp->ioc_flags);
                    }

                    dp->ioc_src = tmp_ioc_src;
                    dp->ioc_dest = tmp_ioc_dst;

                    if (!error) {
                        error = copyout(dstp, dp->ioc_kern_dest, dst_len);
                        if (error) {
                            SMBERROR("copyout failed : %d\n", error);
                            smb_hexdump(__FUNCTION__, "dest buffer: ", (u_char *)dstp, dp->ioc_dest_len);
                        } else {
                            dp->ioc_dest_len = dst_len;
                        }
                    }
                    if (srcp) {
                        SMB_FREE_DATA(srcp, src_alloclen);
                    }
                    if (dstp) {
                        SMB_FREE_DATA(dstp, dst_alloclen);
                    }
                }
			}
ioc_convert_path_error:
			lck_rw_unlock_shared(&sdp->sd_rwlock);
			break;
		}
		case SMBIOC_TCON:
		{
			struct smbioc_share * shspec = (struct smbioc_share *)data;
			
			/* protect against anyone else playing with the smb dev structure */
			lck_rw_lock_exclusive(&sdp->sd_rwlock);
            
            /* free global lock now since we now have sd_rwlock */
            lck_rw_unlock_shared(dev_rw_lck);

			/* Make sure the version matches */
			if (shspec->ioc_version != SMB_IOC_STRUCT_VERSION) {
				error = EINVAL;
			} else if (sdp->sd_share) {
				error = EISCONN;
			} else  if (!sdp->sd_session) {
				error = ENOTCONN;
			} else  {
				error = smb_sm_tcon(sdp->sd_session, shspec, &sdp->sd_share, context);
			}
            
			lck_rw_unlock_exclusive(&sdp->sd_rwlock);
			break;
		}
		case SMBIOC_TDIS: 
		{
			struct smbioc_share * shspec = (struct smbioc_share *)data;
			
			/* protect against anyone else playing with the smb dev structure */
			lck_rw_lock_exclusive(&sdp->sd_rwlock);
            
            /* free global lock now since we now have sd_rwlock */
            lck_rw_unlock_shared(dev_rw_lck);

			/* Make sure the version match */
			if (shspec->ioc_version != SMB_IOC_STRUCT_VERSION) {
				error = EINVAL;
			} else  if (sdp->sd_share == NULL) {
				error = ENOTCONN;
			} else {
				smb_share_rele(sdp->sd_share, context);
				sdp->sd_share = NULL;
				error = 0;
			}
            
			lck_rw_unlock_exclusive(&sdp->sd_rwlock);
			break;			
		}
		case SMBIOC_AUTH_INFO:
		{
			struct smbioc_auth_info * auth_info = (struct smbioc_auth_info *)data;

            lck_rw_lock_shared(&sdp->sd_rwlock);
            
            /* free global lock now since we now have sd_rwlock */
            lck_rw_unlock_shared(dev_rw_lck);

			if (auth_info->ioc_version != SMB_IOC_STRUCT_VERSION) {
				error = EINVAL;
			} else if (!sdp->sd_session) {
				error = ENOTCONN;
			} else {
                sessionp = sdp->sd_session;

                struct smbiod *iod = NULL;
                if (smb_iod_get_main_iod(sessionp, &iod, __FUNCTION__)) {
                    SMBERROR("NULL iod.\n");
                    error = EINVAL;
                    lck_rw_unlock_shared(&sdp->sd_rwlock);
                    break;
                }
                
				auth_info->ioc_client_nt = iod->iod_gss.gss_client_nt;
				auth_info->ioc_target_nt = iod->iod_gss.gss_target_nt;
				/* 
				 * On input the client_size and target_size must be the max size
				 * of the buffer. On output we set them to the correct size or
				 * zero if the buffer is not big enough.
				 */
				if (iod->iod_gss.gss_cpn_len >= auth_info->ioc_client_size) {
					auth_info->ioc_client_size = iod->iod_gss.gss_cpn_len;
				} else {
					auth_info->ioc_client_size = 0;
				}
				if (iod->iod_gss.gss_spn_len >= auth_info->ioc_target_size) {
					auth_info->ioc_target_size = iod->iod_gss.gss_spn_len;
				} else {
					auth_info->ioc_target_size = 0;
				}
				if (iod->iod_gss.gss_cpn && auth_info->ioc_client_size) {
					error = copyout(iod->iod_gss.gss_cpn, auth_info->ioc_client_name,
									(size_t)auth_info->ioc_client_size);
					if (error) {
                        smb_iod_rel(iod, NULL, __FUNCTION__);
						lck_rw_unlock_shared(&sdp->sd_rwlock);
						break;
					}
				}
				if (iod->iod_gss.gss_spn && auth_info->ioc_target_size) {
					error = copyout(iod->iod_gss.gss_spn, auth_info->ioc_target_name, 
									(size_t)auth_info->ioc_target_size);
				}
                
                smb_iod_rel(iod, NULL, __FUNCTION__);
			}
            
			lck_rw_unlock_shared(&sdp->sd_rwlock);
			break;
		}

        case SMBIOC_MULTICHANNEL_PROPERTIES:
        {
            struct smbioc_multichannel_properties *mc_prop = (struct smbioc_multichannel_properties *)data;

            lck_rw_lock_shared(&sdp->sd_rwlock);

            /* free global lock now since we now have sd_rwlock */
            lck_rw_unlock_shared(dev_rw_lck);

            if (mc_prop->ioc_version != SMB_IOC_STRUCT_VERSION) {
                error = EINVAL;
            } else if (!sdp->sd_session) {
                error = ENOTCONN;
            } else {
                sessionp = sdp->sd_session;

                memset(&mc_prop->iod_properties[0], 0, sizeof(mc_prop->iod_properties));

                lck_mtx_lock(&sessionp->iod_tailq_lock);

                struct smbiod *iod = TAILQ_FIRST(&sessionp->iod_tailq_head);
                uint32_t u;
                for(u = 0; u < MAX_NUM_OF_IODS_IN_QUERY; u++) {

                    if (!iod) {
                        break;
                    }

                    lck_mtx_lock(&iod->iod_session->session_interface_table.interface_table_lck);

                    SMB_IOD_FLAGSLOCK(iod);

                    if ((iod->iod_flags & SMBIOD_RUNNING) &&
                        (!(iod->iod_flags & SMBIOD_SHUTDOWN))) {
                        struct smbioc_iod_prop *p = &mc_prop->iod_properties[u];
                        p->iod_prop_id    = iod->iod_id;
                        p->iod_flags      = iod->iod_flags;
                        p->iod_prop_state = iod->iod_state;

                        p->iod_prop_rx_bytes = iod->iod_total_rx_bytes;
                        p->iod_prop_rx_packets = iod->iod_total_rx_packets;
                        p->iod_prop_tx_bytes = iod->iod_total_tx_bytes;
                        p->iod_prop_tx_packets = iod->iod_total_tx_packets;
                        p->iod_prop_setup_time.tv_sec = iod->iod_session_setup_time.tv_sec;

                        p->iod_prop_c_if  = iod->iod_conn_entry.client_if_idx;
                        p->iod_prop_s_if  = (-1);
                        if (iod->iod_conn_entry.con_entry) {
                            p->iod_prop_con_speed = iod->iod_conn_entry.con_entry->con_speed;
                            if (iod->iod_conn_entry.con_entry->con_client_nic) {
                                p->iod_prop_c_if_type = iod->iod_conn_entry.con_entry->con_client_nic->nic_type;
                                p->iod_prop_c_if_caps = iod->iod_conn_entry.con_entry->con_client_nic->nic_caps;
                            }
                            if (iod->iod_conn_entry.con_entry->con_server_nic) {
                                p->iod_prop_s_if = iod->iod_conn_entry.con_entry->con_server_nic->nic_index;
                                p->iod_prop_s_if_caps = iod->iod_conn_entry.con_entry->con_server_nic->nic_caps;
                            }
                        }

                        if (iod->iod_saddr) {

                            p->iod_prop_s_addr.addr_family = iod->iod_saddr->sa_family;

                            switch (p->iod_prop_s_addr.addr_family) {
                                case AF_INET:
                                {
                                    struct sockaddr_in *sa = (void*)iod->iod_saddr;
                                    *((uint32_t*)p->iod_prop_s_addr.addr_ipv4) = sa->sin_addr.s_addr;
                                    p->iod_prop_con_port = ntohs(sa->sin_port);
                                }
                                break;
                                case AF_INET6:
                                {
                                    struct sockaddr_in6 *sa6 = (void*)iod->iod_saddr;
                                    memcpy(p->iod_prop_s_addr.addr_ipv6, &sa6->sin6_addr, sizeof(p->iod_prop_s_addr.addr_ipv6));
                                    p->iod_prop_con_port = ntohs(sa6->sin6_port);
                                }
                                break;
                                default:
                                    SMBERROR("Unknown family.\n");
                            }
                        }
                    }
                    SMB_IOD_FLAGSUNLOCK(iod);
                    lck_mtx_unlock(&iod->iod_session->session_interface_table.interface_table_lck);
                    iod = TAILQ_NEXT(iod, tailq);
                }

                lck_mtx_unlock(&sessionp->iod_tailq_lock);

                mc_prop->num_of_iod_properties = u;
            }

            lck_rw_unlock_shared(&sdp->sd_rwlock);
            break;
        }

        case SMBIOC_NIC_INFO:
        {
            struct smbioc_nic_info *nic_info = (struct smbioc_nic_info *)data;

            lck_rw_lock_shared(&sdp->sd_rwlock);

            /* free global lock now since we now have sd_rwlock */
            lck_rw_unlock_shared(dev_rw_lck);

            if (nic_info->ioc_version != SMB_IOC_STRUCT_VERSION) {
                error = EINVAL;
            } else if (!sdp->sd_session) {
                error = ENOTCONN;
            } else if ((sdp->sd_session->session_flags & SMBV_MULTICHANNEL_ON) == 0) {
                /* nic info when MC is off is irrelevant */
                nic_info->num_of_nics = 0;
            } else {
                sessionp = sdp->sd_session;

                lck_mtx_lock(&sessionp->session_interface_table.interface_table_lck);

                struct session_network_interface_info nic_info_table = sessionp->session_interface_table;

                uint32_t nic_count = 0;
                struct complete_nic_info_entry *nic = NULL;
                switch (nic_info->flags) {
                    case SERVER_NICS:
                        nic_count = nic_info_table.server_nic_count;
                        nic = TAILQ_FIRST(&nic_info_table.server_nic_info_list);
                        break;
                    case CLIENT_NICS:
                        nic_count = nic_info_table.client_nic_count;
                        nic = TAILQ_FIRST(&nic_info_table.client_nic_info_list);
                        break;

                    default:
                        error = EINVAL;
                        break;
                }

                nic_info->num_of_nics = 0;

                uint32_t i;
                uint32_t size = 0;
                void * nic_info_buffer;
                SMB_MALLOC_DATA(nic_info_buffer, nic_info->nic_props_buffer_len, Z_WAITOK_ZERO);
                if (nic_info_buffer == NULL) {
                    SMBERROR("malloc failed.\n");
                    error = ENOMEM;
                    nic_count = 0;
                }
                void * npp = nic_info_buffer;
                for (i = 0; i < nic_count; )
                {
                    struct nic_properties *p = (struct nic_properties *)npp;

                    if (nic == NULL) {
                        break;
                    }
                    if (nic->nic_index & SMB2_IF_RSS_INDEX_MASK) { // don't send duplicate RSS channels
                        nic = TAILQ_NEXT(nic,next);
                        continue;
                    }
                    size += sizeof(struct nic_properties);
                    if (size > nic_info->nic_props_buffer_len) {
                        error = EMSGSIZE;
                        break;
                    }

                    p->if_index = nic->nic_index;
                    p->capabilities = nic->nic_caps;
                    p->speed = nic->nic_link_speed;
                    p->nic_type = nic->nic_type;
                    p->ip_types = nic->nic_ip_types;
                    p->state = nic->nic_state;

                    struct sock_addr_entry *sock_addr = TAILQ_FIRST(&nic->addr_list);
                    uint32_t j = 0;
                    while (sock_addr)
                    {
                        if (sock_addr->addr) {
                            size += sizeof(struct address_properties);
                            if (size > nic_info->nic_props_buffer_len) {
                                error = EMSGSIZE;
                                break;
                            }
                            p->addr_list[j].addr_family = sock_addr->addr->sa_family;

                            switch (p->addr_list[j].addr_family) {
                                case AF_INET:
                                {
                                    struct sockaddr_in *sa = (void*)sock_addr->addr;
                                    *((uint32_t*)p->addr_list[j].addr_ipv4) = sa->sin_addr.s_addr;
                                }
                                break;
                                case AF_INET6:
                                {
                                    struct sockaddr_in6 *sa6 = (void*)sock_addr->addr;
                                    memcpy(p->addr_list[j].addr_ipv6, &sa6->sin6_addr, sizeof(p->addr_list[j].addr_ipv6));
                                }
                                break;
                                default:
                                    SMBERROR("Unknown address family.\n");
                            }

                            j++;

                        }

                        sock_addr = TAILQ_NEXT(sock_addr,next);
                    }

                    if (error) {
                        break;
                    }

                    p->num_of_addrs = j;
                    p->size = sizeof(struct nic_properties) + (j * sizeof(struct address_properties));


                    nic = TAILQ_NEXT(nic,next);
                    npp = (uint8_t *)npp + p->size;
                    i++;
                }
                if (!error) {
                    error = copyout(nic_info_buffer, nic_info->ioc_kern_nic_props_buffer, size);
                }
                if (nic_info_buffer) {
                    SMB_FREE_DATA(nic_info_buffer, nic_info->nic_props_buffer_len);
                }
                if (!error) {
                    nic_info->num_of_nics = i;
                    nic_info->nic_props_buffer_len = size;
                }

                lck_mtx_unlock(&sessionp->session_interface_table.interface_table_lck);
            }
            lck_rw_unlock_shared(&sdp->sd_rwlock);
            break;

        }

        case SMBIOC_SESSION_PROPERTIES:
		{
			struct smbioc_session_properties * properties = (struct smbioc_session_properties *)data;
            size_t str_len = 0;
            struct smb_share *mnt_share, *tshare;

			
			lck_rw_lock_shared(&sdp->sd_rwlock);
            
            /* free global lock now since we now have sd_rwlock */
            lck_rw_unlock_shared(dev_rw_lck);

			if (properties->ioc_version != SMB_IOC_STRUCT_VERSION) {
				error = EINVAL;
			} else if (!sdp->sd_session) {
				error = ENOTCONN;
			} else {
				sessionp = sdp->sd_session;
                properties->uid = sessionp->session_uid;
				properties->smb1_caps = sessionp->session_sopt.sv_caps;
                properties->smb2_caps = sessionp->session_sopt.sv_active_capabilities;
				properties->flags = sessionp->session_flags;
                properties->misc_flags = sessionp->session_misc_flags;
				properties->hflags = sessionp->session_hflags;
				properties->hflags2 = sessionp->session_hflags2;				
				properties->txmax = sessionp->session_txmax;				
				properties->rxmax = sessionp->session_rxmax;
                properties->wxmax = sessionp->session_wxmax;
                
                /* Compression */
                properties->client_compression_algorithms_map = sessionp->client_compression_algorithms_map;
                properties->server_compression_algorithms_map = sessionp->server_compression_algorithms_map;
                properties->compression_io_threshold = sessionp->compression_io_threshold;
                properties->compression_chunk_len = sessionp->compression_chunk_len;
                properties->compression_max_fail_cnt = sessionp->compression_max_fail_cnt;

                properties->write_compress_cnt = sessionp->write_compress_cnt;
                properties->write_cnt_LZ77Huff = sessionp->write_cnt_LZ77Huff;
                properties->write_cnt_LZ77 = sessionp->write_cnt_LZ77;
                properties->write_cnt_LZNT1 = sessionp->write_cnt_LZNT1;
                properties->write_cnt_fwd_pattern = sessionp->write_cnt_fwd_pattern;
                properties->write_cnt_bwd_pattern = sessionp->write_cnt_bwd_pattern;

                properties->read_compress_cnt = sessionp->read_compress_cnt;
                properties->read_cnt_LZ77Huff = sessionp->read_cnt_LZ77Huff;
                properties->read_cnt_LZ77 = sessionp->read_cnt_LZ77;
                properties->read_cnt_LZNT1 = sessionp->read_cnt_LZNT1;
                properties->read_cnt_fwd_pattern = sessionp->read_cnt_fwd_pattern;
                properties->read_cnt_bwd_pattern = sessionp->read_cnt_bwd_pattern;

                /*
                 * If we are currently using encryption, then return the
                 * cipher being used, else return 0.
                 */
                properties->encrypt_cipher = 0;

                if (sessionp->session_sopt.sv_sessflags & SMB2_SESSION_FLAG_ENCRYPT_DATA) {
                    /* Session encryption being used */
                    properties->encrypt_cipher = sessionp->session_smb3_encrypt_ciper;
                }

                /* If we have a share, then is share encryption on? */
                if (sdp->sd_share) {
                    sharep = sdp->sd_share;
                    
                    /* Easy case first of all shares being encrypted */
                    if (sharep->ss_share_flags & SMB2_SHAREFLAG_ENCRYPT_DATA) {
                        /* Share encryption being used */
                        properties->encrypt_cipher = sessionp->session_smb3_encrypt_ciper;
                    }
                    else {
                        /*
                         * Harder case where mount_smbfs was used to only
                         * encrypt one specific share.
                         *
                         * sdp->sd_share is a new Tree Connect just for this
                         * ioctl and is also in the list of shares for this
                         * session. Need to find the actual mounted share info
                         * in this session to check it to see if encryption is
                         * on or off
                         */
                        if (sharep->ss_name != NULL) {
                            smb_session_lock(sessionp);
                            
                            SMBCO_FOREACH_SAFE(mnt_share, SESSION_TO_CP(sessionp), tshare) {
                                lck_mtx_lock(&mnt_share->ss_stlock);

                                /*
                                 * The actual mounted share will have
                                 * (ss_mount != NULL)
                                 */
                                if (!(mnt_share->ss_flags & SMBO_GONE) &&
                                    (mnt_share->ss_name != NULL) &&
                                    (mnt_share->ss_mount != NULL)) {
                                    /*
                                     * Is it the same share we are looking for?
                                     */
                                   if (strcmp(sharep->ss_name, mnt_share->ss_name) == 0) {
                                        /* Found it, now check its encrypt status */
                                        if (mnt_share->ss_share_flags & SMB2_SHAREFLAG_ENCRYPT_DATA) {
                                            /* Share encryption being used */
                                            properties->encrypt_cipher = sessionp->session_smb3_encrypt_ciper;
                                        }
                                       
                                       lck_mtx_unlock(&(mnt_share)->ss_stlock);
                                       break;
                                    }
                                }

                                lck_mtx_unlock(&(mnt_share)->ss_stlock);
                            }

                            smb_session_unlock(sessionp);
                        } /* if (sharep->ss_name != NULL) */
                    } /* else */
                } /* if (sdp->sd_share) */
                
                properties->signing_algorithm = sessionp->session_smb3_signing_algorithm;

                memset(properties->model_info, 0, (SMB_MAXFNAMELEN * 2));
                /* only when we are mac to mac */
                if (sessionp->session_misc_flags & SMBV_OSX_SERVER) {
                    lck_mtx_lock(&sessionp->session_model_info_lock);
                    
                    str_len = strnlen(sessionp->session_model_info,
                                      sizeof(sessionp->session_model_info));
                    if (str_len > 0) {
                        /* Only copy if there is a string to copy */
                       if (str_len < sizeof(properties->model_info)) {
                            /* Make sure to not read past end of session_model_info */
                            strlcpy(properties->model_info,
                                    sessionp->session_model_info,
                                    sizeof(sessionp->session_model_info));
                        }
                        else {
                            /* Make sure to not write past end of model_info */
                            strlcpy(properties->model_info,
                                    sessionp->session_model_info,
                                    sizeof(properties->model_info));
                        }
                    }

                    lck_mtx_unlock(&sessionp->session_model_info_lock);
                }

                /* mc additions */
                properties->ioc_session_reconnect_count = sessionp->session_reconnect_count;
                properties->ioc_session_reconnect_time = sessionp->session_reconnect_time;
                properties->ioc_session_setup_time = sessionp->session_setup_time;

                /*
                 * the total number of bytes transmitted received by
                 * this session on all channels is the sum of all iod rx/tx bytes
                 * and the number of tx/rx bytes of gone iod's
                 */
                properties->ioc_total_rx_bytes = sessionp->session_gone_iod_total_rx_bytes;
                properties->ioc_total_tx_bytes = sessionp->session_gone_iod_total_tx_bytes;
                properties->ioc_total_rx_packets = sessionp->session_gone_iod_total_rx_packets;
                properties->ioc_total_tx_packets = sessionp->session_gone_iod_total_tx_packets;

                lck_mtx_lock(&sessionp->iod_tailq_lock);

                struct smbiod *iod = TAILQ_FIRST(&sessionp->iod_tailq_head);

                while(iod) {
                    SMB_IOD_FLAGSLOCK(iod);

                    if ((iod->iod_flags & SMBIOD_RUNNING) &&
                        (!(iod->iod_flags & SMBIOD_SHUTDOWN))) {
                        properties->ioc_total_rx_bytes += iod->iod_total_rx_bytes;
                        properties->ioc_total_tx_bytes += iod->iod_total_tx_bytes;
                        properties->ioc_total_rx_packets += iod->iod_total_rx_packets;
                        properties->ioc_total_tx_packets += iod->iod_total_tx_packets;
                    }

                    SMB_IOD_FLAGSUNLOCK(iod);
                    iod = TAILQ_NEXT(iod, tailq);
                }

                lck_mtx_unlock(&sessionp->iod_tailq_lock);

                /* Copy snapshot string out if present */
                if (sessionp->session_misc_flags & SMBV_MNT_SNAPSHOT) {
                    str_len = strnlen(sessionp->snapshot_time,
                                      sizeof(sessionp->snapshot_time));
                     if (str_len > 0) {
                         /* Only copy if there is a string to copy */
                        if (str_len < sizeof(properties->snapshot_time)) {
                             /* Make sure to not read past end of snapshot_time */
                             strlcpy(properties->snapshot_time,
                                     sessionp->snapshot_time,
                                     sizeof(sessionp->snapshot_time));
                         }
                         else {
                             /* Make sure to not write past end of snapshot_time */
                             strlcpy(properties->snapshot_time,
                                     sessionp->snapshot_time,
                                     sizeof(properties->snapshot_time));
                         }
                     }
                }
			}

			lck_rw_unlock_shared(&sdp->sd_rwlock);
			break;
		}
		case SMBIOC_SHARE_PROPERTIES:
		{
			struct smbioc_share_properties * properties = (struct smbioc_share_properties *)data;
			
			lck_rw_lock_shared(&sdp->sd_rwlock);
            
            /* free global lock now since we now have sd_rwlock */
            lck_rw_unlock_shared(dev_rw_lck);

			if (properties->ioc_version != SMB_IOC_STRUCT_VERSION) {
				error = EINVAL;
			} else if (!sdp->sd_session || !sdp->sd_share) {
				error = ENOTCONN;
			} else {
				sharep = sdp->sd_share;
                properties->share_caps  = sharep->ss_share_caps;
                properties->share_flags = sharep->ss_share_flags;
				properties->share_type  = sharep->ss_share_type;
				properties->attributes  = sharep->ss_attributes;
			}

			lck_rw_unlock_shared(&sdp->sd_rwlock);
			break;
		}
            
        case SMBIOC_GET_OS_LANMAN:
		{
			lck_rw_lock_shared(&sdp->sd_rwlock);
            
            /* free global lock now since we now have sd_rwlock */
            lck_rw_unlock_shared(dev_rw_lck);

			if (!sdp->sd_session) {
				error = ENOTCONN;
			} else {
				struct smbioc_os_lanman * OSLanman = (struct smbioc_os_lanman *)data;
				sessionp = sdp->sd_session;
				if (sessionp->NativeOS)
					strlcpy(OSLanman->NativeOS, sessionp->NativeOS, sizeof(OSLanman->NativeOS));
				if (sessionp->NativeLANManager)
					strlcpy(OSLanman->NativeLANManager, sessionp->NativeLANManager, sizeof(OSLanman->NativeLANManager));
			}

			lck_rw_unlock_shared(&sdp->sd_rwlock);
			break;
		}
		case SMBIOC_SESSSTATE:
		{
			lck_rw_lock_shared(&sdp->sd_rwlock);
            
            /* free global lock now since we now have sd_rwlock */
            lck_rw_unlock_shared(dev_rw_lck);

			/* Check to see if the session is still up and running */
			if (sdp->sd_session &&
                sdp->sd_session->session_iod &&
                (SMB_TRAN_FATAL(sdp->sd_session->session_iod, 0) == 0)) {
				*(uint16_t *)data = EISCONN;
			} else {
				*(uint16_t *)data = ENOTCONN;
			}

			lck_rw_unlock_shared(&sdp->sd_rwlock);
			break;			
		}
		case SMBIOC_CANCEL_SESSION:
		{
			/* The global device lock protects us here */
			sdp->sd_flags |= NSMBFL_CANCEL;
            
            lck_rw_unlock_shared(dev_rw_lck);
			break;
		}
		case SMBIOC_REQUEST: 
		{
			struct smbioc_rq * dp = (struct smbioc_rq *)data;
			
			lck_rw_lock_shared(&sdp->sd_rwlock);
            
            /* free global lock now since we now have sd_rwlock */
            lck_rw_unlock_shared(dev_rw_lck);

			/* Make sure the version match */
			if (dp->ioc_version != SMB_IOC_STRUCT_VERSION) {
				error = EINVAL;
			}
			else if (sdp->sd_share == NULL) {
				error = ENOTCONN;
			} else {
                char *twordsp = NULL;
                char *tbytesp = NULL;
                char *responsep = NULL;
                size_t twordsp_allocsize = 0, tbytesp_allocsize = 0, responsep_allocsize = 0;

                /* Take the 32 bit world pointers and convert them to user_addr_t. */
                if (! vfs_context_is64bit (context)) {
                    dp->ioc_kern_twords = CAST_USER_ADDR_T(dp->ioc_twords);
                    dp->ioc_kern_tbytes = CAST_USER_ADDR_T(dp->ioc_tbytes);
                }

                if ((dp->ioc_twc) && (dp->ioc_kern_twords)) {
                    twordsp = smb_memdupin(dp->ioc_kern_twords, dp->ioc_twc * 2);
                    twordsp_allocsize = dp->ioc_twc * 2;
                    if (twordsp == NULL) {
                        SMBERROR("failed to dupin ioc_kern_twords\n");
                        error = ENOMEM;
                        goto ioc_rq_error;
                    }
                }

                if ((dp->ioc_tbc) && (dp->ioc_kern_tbytes)) {
                    tbytesp = smb_memdupin(dp->ioc_kern_tbytes, dp->ioc_tbc);
                    tbytesp_allocsize = dp->ioc_tbc;
                    if (tbytesp == NULL) {
                        SMBERROR("failed to dupin ioc_kern_tbytes\n");
                        error = ENOMEM;
                        SMB_FREE_DATA(twordsp, twordsp_allocsize);
                        goto ioc_rq_error;
                    }
                }

                /*
                 * <84091372> All user mode memory should get copyin before we call the
                 * smb_usr_simplerequest. To make sure usrptrs will not be used by it in
                 * the future, we temporary set this pointers to NULL.
                 */
                void *tmp_ioc_twords = dp->ioc_twords;
                void *tmp_ioc_tbytes = dp->ioc_tbytes;
                char *tmp_ioc_rpbuf  = dp->ioc_rpbuf;

                dp->ioc_twords = NULL;
                dp->ioc_tbytes = NULL;
                dp->ioc_rpbuf = NULL;

                responsep_allocsize = dp->ioc_rpbufsz;
                SMB_MALLOC_DATA(responsep, responsep_allocsize, Z_WAITOK_ZERO);
				error = smb_usr_simplerequest(sdp->sd_share, dp, context, twordsp, tbytesp, responsep);
                dp->ioc_twords = tmp_ioc_twords;
                dp->ioc_tbytes = tmp_ioc_tbytes;
                dp->ioc_rpbuf = tmp_ioc_rpbuf;

                if (error == 0) {
                    if (! vfs_context_is64bit (context)) {
                        dp->ioc_kern_rpbuf = CAST_USER_ADDR_T(dp->ioc_rpbuf);
                    }
                    dp->ioc_errno = copyout(responsep, dp->ioc_kern_rpbuf, dp->ioc_rpbufsz);
                }
                if (twordsp) {
                    SMB_FREE_DATA(twordsp, twordsp_allocsize);
                }
                if (tbytesp) {
                    SMB_FREE_DATA(tbytesp, tbytesp_allocsize);
                }
                SMB_FREE_DATA(responsep, responsep_allocsize);
			}
ioc_rq_error:
			lck_rw_unlock_shared(&sdp->sd_rwlock);
			break;
		}
		case SMBIOC_T2RQ: 
		{
			struct smbioc_t2rq * dp2 = (struct smbioc_t2rq *)data;
			
			lck_rw_lock_shared(&sdp->sd_rwlock);
            
            /* free global lock now since we now have sd_rwlock */
            lck_rw_unlock_shared(dev_rw_lck);

			/* Make sure the version match */
			if (dp2->ioc_version != SMB_IOC_STRUCT_VERSION) {
				error = EINVAL;
			} else if (sdp->sd_share == NULL) {
				error = ENOTCONN;
			} else {
				error = smb_usr_t2request(sdp->sd_share, dp2, context);				
			}
            
			lck_rw_unlock_shared(&sdp->sd_rwlock);
			break;		
		}
		case SMBIOC_READ: 
		case SMBIOC_WRITE: 
		{
			struct smbioc_rw *rwrq = (struct smbioc_rw *)data;
            uint32_t allow_compression = 0; /* No compression from user space */

			lck_rw_lock_shared(&sdp->sd_rwlock);
            
            /* free global lock now since we now have sd_rwlock */
            lck_rw_unlock_shared(dev_rw_lck);

			/* Make sure the version match */
			if (rwrq->ioc_version != SMB_IOC_STRUCT_VERSION) {
				error = EINVAL;
			} else if (sdp->sd_share == NULL) {
				error = ENOTCONN;
			} else {
				uio_t auio = NULL;
                char *basep = NULL;
                smbfh fh;
                SMBFID fid = 0;
                auio =  uio_create(1, rwrq->ioc_offset, UIO_SYSSPACE,
                                   (cmd == SMBIOC_READ) ? UIO_READ : UIO_WRITE);

                if (!auio) {
                    error = ENOMEM;
                    goto ioc_rw_error;
                }

                if (cmd == SMBIOC_READ) {
                    SMB_MALLOC_DATA(basep, rwrq->ioc_cnt, Z_WAITOK_ZERO);
                } else {
                    /* Take the 32 bit world pointers and convert them to user_addr_t. */
                    if (!vfs_context_is64bit(context)) {
                        rwrq->ioc_kern_base = CAST_USER_ADDR_T(rwrq->ioc_base);
                    }
                    basep = smb_memdupin(rwrq->ioc_kern_base, rwrq->ioc_cnt);
                    if (basep == NULL) {
                        SMBERROR("failed to dupin ioc_kern_base\n");
                        error = ENOMEM;
                        uio_free(auio);
                        goto ioc_rw_error;
                    }
                }

                uio_addiov(auio, CAST_USER_ADDR_T(basep), rwrq->ioc_cnt);
                fh = htoles(rwrq->ioc_fh);
                fid = fh;

                /* All calls from user maintain a reference on the share */
                if (cmd == SMBIOC_READ) {
                    error = smb_smb_read(sdp->sd_share, fid, auio,
                                         allow_compression, context);
                }
                else {
                    int ioFlags = (rwrq->ioc_writeMode & WritethroughMode) ? IO_SYNC : 0;
                    error = smb_smb_write(sdp->sd_share, fid, auio, ioFlags,
                                          &allow_compression, context);
                }

                if ((error == 0) && (cmd == SMBIOC_READ)) {
                    /* Take the 32 bit world pointers and convert them to user_addr_t. */
                    if (!vfs_context_is64bit(context)) {
                        rwrq->ioc_kern_base = CAST_USER_ADDR_T(rwrq->ioc_base);
                    }
                    error = copyout(basep, rwrq->ioc_kern_base, rwrq->ioc_cnt);
                }

                if (error == 0) {
                    rwrq->ioc_cnt -= (int32_t)uio_resid(auio);
                }

                uio_free(auio);
                SMB_FREE_DATA(basep, rwrq->ioc_cnt);
			}
ioc_rw_error:
			lck_rw_unlock_shared(&sdp->sd_rwlock);
			break;
		}
		case SMBIOC_FSCTL:
		{
			struct smbioc_fsctl * fsctl = (struct smbioc_fsctl *)data;

			lck_rw_lock_shared(&sdp->sd_rwlock);
            
            /* free global lock now since we now have sd_rwlock */
            lck_rw_unlock_shared(dev_rw_lck);

			/* Make sure the version match */
			if (fsctl->ioc_version != SMB_IOC_STRUCT_VERSION) {
				error = EINVAL;
			} else if (sdp->sd_share == NULL) {
				error = ENOTCONN;
			} else {
                
                char *tdatap = NULL;
                char *rdatap = NULL;
                size_t tdatap_allocsize = 0, rdatap_allocsize = 0;

                /* Take the 32 bit world pointers and convert them to user_addr_t. */
                if (! vfs_context_is64bit (context)) {
                    fsctl->ioc_kern_tdata = CAST_USER_ADDR_T(fsctl->ioc_tdata);
                }

                if ((fsctl->ioc_tdatacnt) && (fsctl->ioc_kern_tdata)) {
                    tdatap = smb_memdupin(fsctl->ioc_kern_tdata, fsctl->ioc_tdatacnt);
                    tdatap_allocsize = fsctl->ioc_tdatacnt;
                    if (tdatap == NULL) {
                        SMBERROR("failed to dupin ioc_kern_tdata\n");
                        error = ENOMEM;
                        goto ioc_fsctl_error;
                    }
                }

                /*
                 * <84091372> All user mode memory should get copyin before we call the
                 * smb_usr_fsctl. To make sure usrptrs will not be used by it in
                 * the future, we temporary set this pointers to NULL.
                 */
                void *tmp_ioc_tdata = fsctl->ioc_tdata;
                void *tmp_ioc_rdata = fsctl->ioc_rdata;

                fsctl->ioc_tdata = NULL;
                fsctl->ioc_rdata = NULL;

                rdatap_allocsize = fsctl->ioc_rdatacnt;
                SMB_MALLOC_DATA(rdatap, rdatap_allocsize, Z_WAITOK_ZERO);
                error = smb_usr_fsctl(sdp->sd_share, fsctl, context, tdatap, rdatap);
                
                fsctl->ioc_tdata = tmp_ioc_tdata;
                fsctl->ioc_rdata = tmp_ioc_rdata;

                if ((error == 0) && (fsctl->ioc_errno == 0) && (fsctl->ioc_rdatacnt)) {
                    if (! vfs_context_is64bit (context)) {
                        fsctl->ioc_kern_rdata = CAST_USER_ADDR_T(fsctl->ioc_rdata);
                    }
                    fsctl->ioc_errno = copyout(rdatap, fsctl->ioc_kern_rdata, fsctl->ioc_rdatacnt);
                }

                if (tdatap) {
                    SMB_FREE_DATA(tdatap, tdatap_allocsize);
                }
                SMB_FREE_DATA(rdatap, rdatap_allocsize);
			}
ioc_fsctl_error:
			lck_rw_unlock_shared(&sdp->sd_rwlock);
			break;
		}

		case SMB2IOC_CHECK_DIR: 
		{
			struct smb2ioc_check_dir * check_dir_ioc = (struct smb2ioc_check_dir *) data;
			
			lck_rw_lock_shared(&sdp->sd_rwlock);
            
            /* free global lock now since we now have sd_rwlock */
            lck_rw_unlock_shared(dev_rw_lck);

			/* Make sure the version match */
			if (check_dir_ioc->ioc_version != SMB_IOC_STRUCT_VERSION) {
				error = EINVAL;
			} else if (sdp->sd_share == NULL) {
				error = ENOTCONN;
			} else {
				error = smb_usr_check_dir(sdp->sd_share, sdp->sd_session,
                                          check_dir_ioc, context);
                if (error) {
                    /* 
                     * Note: On error, the ioctl code will NOT copy out the data
                     * structure back to user space.
                     *
                     * If ioc_ret_ntstatus is filled in, change the error to 0 
                     * so that we can return the real NT error in user space.
                     * User space code is responsible for checking both error 
                     * and ioc_ret_ntstatus for errors.
                     */
                    check_dir_ioc->ioc_ret_errno = error;
                    if (check_dir_ioc->ioc_ret_ntstatus & 0xC0000000) {
                        error = 0;
                    }
                }
			}
            
			lck_rw_unlock_shared(&sdp->sd_rwlock);
			break;		
		}

		case SMB2IOC_CLOSE:
		{
			struct smb2ioc_close * close_ioc = (struct smb2ioc_close *) data;
			
			lck_rw_lock_shared(&sdp->sd_rwlock);
            
            /* free global lock now since we now have sd_rwlock */
            lck_rw_unlock_shared(dev_rw_lck);

			/* Make sure the version match */
			if (close_ioc->ioc_version != SMB_IOC_STRUCT_VERSION) {
				error = EINVAL;
			} else if (sdp->sd_share == NULL) {
				error = ENOTCONN;
			} else {
				error = smb_usr_close(sdp->sd_share, close_ioc, context);	
                if (error) {
                    /* 
                     * Note: On error, the ioctl code will NOT copy out the data
                     * structure back to user space.
                     *
                     * If ioc_ret_ntstatus is filled in, change the error to 0 
                     * so that we can return the real NT error in user space.
                     * User space code is responsible for checking both error 
                     * and ioc_ret_ntstatus for errors.
                     */
                    if (close_ioc->ioc_ret_ntstatus & 0xC0000000) {
                        error = 0;
                    }
                }
			}
            
			lck_rw_unlock_shared(&sdp->sd_rwlock);
			break;		
		}

		case SMB2IOC_CREATE: 
		{
			struct smb2ioc_create * create_ioc = (struct smb2ioc_create *) data;
			
			lck_rw_lock_shared(&sdp->sd_rwlock);
            
            /* free global lock now since we now have sd_rwlock */
            lck_rw_unlock_shared(dev_rw_lck);

			/* Make sure the version match */
			if (create_ioc->ioc_version != SMB_IOC_STRUCT_VERSION) {
				error = EINVAL;
			} else if (sdp->sd_share == NULL) {
				error = ENOTCONN;
			} else {
				error = smb_usr_create(sdp->sd_share, create_ioc, context);
                if (error) {
                    /* 
                     * Note: On error, the ioctl code will NOT copy out the data
                     * structure back to user space.
                     *
                     * If ioc_ret_ntstatus is filled in, change the error to 0 
                     * so that we can return the real NT error in user space.
                     * User space code is responsible for checking both error 
                     * and ioc_ret_ntstatus for errors.
                     */
                    if (create_ioc->ioc_ret_ntstatus & 0xC0000000) {
                        error = 0;
                    }
                }
			}
            
			lck_rw_unlock_shared(&sdp->sd_rwlock);
			break;		
		}

		case SMB2IOC_GET_DFS_REFERRAL:
        {
			struct smb2ioc_get_dfs_referral * get_dfs_refer_ioc = (struct smb2ioc_get_dfs_referral *) data;
			
			lck_rw_lock_shared(&sdp->sd_rwlock);
            
            /* free global lock now since we now have sd_rwlock */
            lck_rw_unlock_shared(dev_rw_lck);
            
			/* Make sure the version match */
			if (get_dfs_refer_ioc->ioc_version != SMB_IOC_STRUCT_VERSION) {
				error = EINVAL;
			} else if (sdp->sd_share == NULL) {
				error = ENOTCONN;
			} else {
				error = smb_usr_get_dfs_referral(sdp->sd_share, sdp->sd_session,
                                                 get_dfs_refer_ioc, context);

                /* Convert returned status into errno */
                get_dfs_refer_ioc->ioc_ret_ntstatus_error = smb_ntstatus_to_errno(get_dfs_refer_ioc->ioc_ret_ntstatus);

                if (error) {
                    /* 
                     * Note: On error, the ioctl code will NOT copy out the data
                     * structure back to user space.
                     *
                     * If ioc_ret_ntstatus is filled in, change the error to 0 
                     * so that we can return the real NT error in user space.
                     * User space code is responsible for checking both error 
                     * and ioc_ret_ntstatus for errors.
                     */
                    if (get_dfs_refer_ioc->ioc_ret_ntstatus & 0xC0000000) {
                        error = 0;
                    }
                }
			}
            
			lck_rw_unlock_shared(&sdp->sd_rwlock);
			break;		
        }
            
		case SMB2IOC_IOCTL:
		{
			struct smb2ioc_ioctl * ioctl_ioc = (struct smb2ioc_ioctl *) data;
			
			lck_rw_lock_shared(&sdp->sd_rwlock);
            
            /* free global lock now since we now have sd_rwlock */
            lck_rw_unlock_shared(dev_rw_lck);

			/* Make sure the version match */
			if (ioctl_ioc->ioc_version != SMB_IOC_STRUCT_VERSION) {
				error = EINVAL;
			} else if (sdp->sd_share == NULL) {
				error = ENOTCONN;
			} else {
				error = smb_usr_ioctl(sdp->sd_share, sdp->sd_session,
                                      ioctl_ioc, context);
                if (error) {
                    /* 
                     * Note: On error, the ioctl code will NOT copy out the data
                     * structure back to user space.
                     *
                     * If ioc_ret_ntstatus is filled in, change the error to 0 
                     * so that we can return the real NT error in user space.
                     * User space code is responsible for checking both error 
                     * and ioc_ret_ntstatus for errors.
                     */
                    if (ioctl_ioc->ioc_ret_ntstatus & 0xC0000000) {
                        error = 0;
                    }
                }
			}
            
			lck_rw_unlock_shared(&sdp->sd_rwlock);
			break;		
		}

		case SMB2IOC_QUERY_DIR:
        {
			struct smb2ioc_query_dir *query_dir_ioc = (struct smb2ioc_query_dir *) data;

			lck_rw_lock_shared(&sdp->sd_rwlock);
            
            /* free global lock now since we now have sd_rwlock */
            lck_rw_unlock_shared(dev_rw_lck);
            
			/* Make sure the version match */
			if (query_dir_ioc->ioc_version != SMB_IOC_STRUCT_VERSION) {
				error = EINVAL;
			} else if (sdp->sd_share == NULL) {
				error = ENOTCONN;
			} else {
				error = smb_usr_query_dir(sdp->sd_share, query_dir_ioc,
                                          context);
                if (error) {
                    /*
                     * Note: On error, the ioctl code will NOT copy out the data
                     * structure back to user space.
                     *
                     * If ioc_ret_ntstatus is filled in, change the error to 0
                     * so that we can return the real NT error in user space.
                     * User space code is responsible for checking both error
                     * and ioc_ret_ntstatus for errors.
                     */
                    if (query_dir_ioc->ioc_ret_ntstatus & 0xC0000000) {
                        error = 0;
                    }
                }
			}
            
			lck_rw_unlock_shared(&sdp->sd_rwlock);
			break;
        }

		case SMB2IOC_READ:
		case SMB2IOC_WRITE: 
		{
			struct smb2ioc_rw *rw_ioc = (struct smb2ioc_rw *) data;
			
			lck_rw_lock_shared(&sdp->sd_rwlock);
            
            /* free global lock now since we now have sd_rwlock */
            lck_rw_unlock_shared(dev_rw_lck);

			/* Make sure the version match */
			if (rw_ioc->ioc_version != SMB_IOC_STRUCT_VERSION) {
				error = EINVAL;
			} else if (sdp->sd_share == NULL) {
				error = ENOTCONN;
			} else {
				error = smb_usr_read_write(sdp->sd_share, cmd, rw_ioc, context);
                if (error) {
                    /* 
                     * Note: On error, the ioctl code will NOT copy out the data
                     * structure back to user space.
                     *
                     * If ioc_ret_ntstatus is filled in, change the error to 0 
                     * so that we can return the real NT error in user space.
                     * User space code is responsible for checking both error 
                     * and ioc_ret_ntstatus for errors.
                     */
                    if (rw_ioc->ioc_ret_ntstatus & 0xC0000000) {
                        error = 0;
                    }
                }
			}
            
			lck_rw_unlock_shared(&sdp->sd_rwlock);
			break;		
		}

		default:
		{
			error = ENODEV;
            lck_rw_unlock_shared(dev_rw_lck);
			break;
		}
	}
    
	vfs_context_rele(context);
exit:		
	return (error);
}


static int nsmb_dev_load(module_t mod, int cmd, void *arg)
{
#pragma unused(mod, arg)
	int error = 0;

	lck_rw_lock_exclusive(dev_rw_lck);
	switch (cmd) {
	    case MOD_LOAD:
			error = smb_sm_init();
			if (error)
				break;
			error = smb_iod_init();
			if (error) {
				(void)smb_sm_done();
				break;
			}
			if (smb_major == -1) {
				dev_t dev;
				struct smb_dev *sdp;

				smb_major = cdevsw_add(-1, &nsmb_cdevsw);
				if (smb_major == -1) {
					error = EBUSY;
					SMBERROR("smb: cdevsw_add");
					(void)smb_iod_done();
					(void)smb_sm_done();
				}
                SMB_MALLOC_TYPE(sdp, struct smb_dev, Z_WAITOK);
				bzero(sdp, sizeof(*sdp));
				dev = makedev(smb_major, 0);
				sdp->sd_devfs = devfs_make_node(dev, DEVFS_CHAR, UID_ROOT, GID_WHEEL, 0666, "nsmb0");
				if (!sdp->sd_devfs) {
					error = ENOMEM;
					SMBERROR("smb: devfs_make_node 0666");
					(void)cdevsw_remove(smb_major, &nsmb_cdevsw);
                    SMB_FREE_TYPE(struct smb_dev, sdp);
					(void)smb_iod_done();
					(void)smb_sm_done();	
				}
				smb_minor_hiwat = 0;
                SMB_SETDEV(dev, sdp);
			}
			SMBDEBUG("netsmb_dev: loaded\n");
			break;
	    case MOD_UNLOAD:
			smb_iod_done();
			error = smb_sm_done();
			if (error)
				break;
			if (smb_major != -1) {
				int m;
				struct smb_dev *sdp;

				for (m = 0; m <= smb_minor_hiwat; m++)
					if ((sdp = SMB_GETDEV(m))) {
                        SMB_SETDEV(m, NULL);
						if (sdp->sd_devfs)
							devfs_remove(sdp->sd_devfs);
					}
				smb_minor_hiwat = -1;
				smb_major = cdevsw_remove(smb_major, &nsmb_cdevsw);
				if (smb_major == -1)
					SMBERROR("smb: cdevsw_remove failed");
				smb_major = -1;
			}
			SMBDEBUG("netsmb_dev: unloaded\n");
			break;
	    default:
			error = EINVAL;
			break;
	}
	lck_rw_unlock_exclusive(dev_rw_lck);
	return (error);
}

DEV_MODULE(dev_netsmb, nsmb_dev_load, 0);

int
smb_dev2share(int fd, struct smb_share **outShare)
{
	vnode_t vp;
	struct smb_dev *sdp = NULL;
	struct smb_share *share;
	dev_t dev = NODEV;
	int error;

	error = file_vnode_withvid(fd, &vp, NULL);
	if (error) {
		return (error);
	}
	lck_rw_lock_exclusive(dev_rw_lck);
	if (vp) {
		dev = vn_todev(vp);
	}
	if (dev != NODEV) {
		sdp = SMB_GETDEV(dev);
	}
	if (sdp == NULL) {
        SMBERROR("NULL sdp");
		error = EBADF;
		goto done;
	}
	/* over kill since we have the global device lock, but it looks cleaner */
	lck_rw_lock_exclusive(&sdp->sd_rwlock);
	share = sdp->sd_share;
	if (share == NULL) {
		lck_rw_unlock_exclusive(&sdp->sd_rwlock);
 		error = ENOTCONN;
		goto done;
	}
	/*
	 * The share is already referenced by the TCON ioctl
	 * We NULL to hand off share to caller (mount)
	 * This allows further ioctls against connection, for instance
	 * another tree connect and mount, in the automounter case
	 */
	sdp->sd_share = NULL;
	lck_rw_unlock_exclusive(&sdp->sd_rwlock);
	*outShare = share;
done:
	file_drop(fd);
	lck_rw_unlock_exclusive(dev_rw_lck);
	return (error);
}

