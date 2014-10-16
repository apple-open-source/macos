/*
 * Copyright (c) 2000-2001 Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2012 Apple Inc. All rights reserved.
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

/*
 * Userland code loops through minor #s 0 to 1023, looking for one which opens.
 * Intially we create minor 0 and leave it for anyone.  Minor zero will never
 * actually get used - opening triggers creation of another (but private) minor,
 * which userland code will get to and mark busy.
 */
#define SMBMINORS 1024
struct smb_dev * smb_dtab[SMBMINORS];
int smb_minor_hiwat = -1;
#define SMB_GETDEV(dev)         (smb_dtab[minor(dev)])

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
        SMB_MALLOC(sdp, struct smb_dev *, sizeof(*sdp), M_NSMBDEV, M_WAITOK);
		bzero(sdp, sizeof(*sdp));
		dev = makedev(smb_major, avail_minor);
		sdp->sd_devfs = devfs_make_node(dev, DEVFS_CHAR,
						kauth_cred_getuid(cred),
						kauth_cred_getgid(cred),
						0700, "nsmb%x", avail_minor);
		if (!sdp->sd_devfs) {
			SMBERROR("devfs_make_node failed %d\n", avail_minor);
			SMB_FREE(sdp, M_NSMBDEV);
			return (ENOMEM);
		}
		if (avail_minor > smb_minor_hiwat)
			smb_minor_hiwat = avail_minor;
		SMB_GETDEV(dev) = sdp;
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
	struct smb_vc *vcp;
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

	vcp = sdp->sd_vc;
	sdp->sd_vc = NULL; /* Just to be extra careful */
	if (vcp != NULL) 
		smb_vc_rele(vcp, context);
    
	lck_rw_unlock_exclusive(&sdp->sd_rwlock);

	devfs_remove(sdp->sd_devfs); /* first disallow opens */

	vfs_context_rele(context);

	SMB_GETDEV(dev) = NULL;
	lck_rw_destroy(&sdp->sd_rwlock, dev_lck_grp);
	SMB_FREE(sdp, M_NSMBDEV);
	dev_open_cnt--;

	lck_rw_unlock_exclusive(dev_rw_lck);
	return (0);
}

static int nsmb_dev_ioctl(dev_t dev, u_long cmd, caddr_t data, int flag, 
						  struct proc *p)
{
#pragma unused(flag, p)
	struct smb_dev *sdp;
	struct smb_vc *vcp;
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
		case SMBIOC_FIND_VC:
		case SMBIOC_NEGOTIATE:
		{
			int searchOnly = (cmd == SMBIOC_FIND_VC) ? TRUE : FALSE;
			struct smbioc_negotiate * vspec = (struct smbioc_negotiate *)data;
			
			/* protect against anyone else playing with the smb dev structure */
			lck_rw_lock_exclusive(&sdp->sd_rwlock);

            /* free global lock now since we now have sd_rwlock */
            lck_rw_unlock_shared(dev_rw_lck);

            /* Make sure the version matches */
			if (vspec->ioc_version != SMB_IOC_STRUCT_VERSION) {
				error = EINVAL;
			} else if (sdp->sd_vc || sdp->sd_share) {
				error = EISCONN;
			} else {
				error = smb_usr_negotiate(vspec, context, sdp, searchOnly);				
			}
            
			lck_rw_unlock_exclusive(&sdp->sd_rwlock);
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
			} else if (!sdp->sd_vc) {
				error = ENOTCONN;
			} else {
				error = smb_usr_set_network_identity(sdp->sd_vc, ntwrkID);
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
			} else if (!sdp->sd_vc) {
				error = ENOTCONN;
			} else {
				error = smb_sm_ssnsetup(sdp->sd_vc, sspec, context);				
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
			} else if (!sdp->sd_vc) {
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
				} else if (dp->ioc_direction & LOCAL_TO_NETWORK) {
					error = smb_usr_convert_path_to_network(sdp->sd_vc, dp);
				} else {
					error = smb_usr_convert_network_to_path(sdp->sd_vc, dp);
				}
			}
            
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
			} else  if (!sdp->sd_vc) {
				error = ENOTCONN;
			} else  {
				error = smb_sm_tcon(sdp->sd_vc, shspec, &sdp->sd_share, context);
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
			} else if (!sdp->sd_vc) {
				error = ENOTCONN;
			} else {
				vcp = sdp->sd_vc;
				auth_info->ioc_client_nt = vcp->vc_gss.gss_client_nt;				
				auth_info->ioc_target_nt = vcp->vc_gss.gss_target_nt;
				/* 
				 * On input the client_size and target_size must be the max size
				 * of the buffer. On output we set them to the correct size or
				 * zero if the buffer is not big enough.
				 */
				if (vcp->vc_gss.gss_cpn_len >= auth_info->ioc_client_size) {
					auth_info->ioc_client_size = vcp->vc_gss.gss_cpn_len;
				} else {
					auth_info->ioc_client_size = 0;
				}
				if (vcp->vc_gss.gss_spn_len >= auth_info->ioc_target_size) {
					auth_info->ioc_target_size = vcp->vc_gss.gss_spn_len;
				} else {
					auth_info->ioc_target_size = 0;
				}
				if (vcp->vc_gss.gss_cpn && auth_info->ioc_client_size) {
					error = copyout(vcp->vc_gss.gss_cpn, auth_info->ioc_client_name, 
									(size_t)auth_info->ioc_client_size);
					if (error) {
						lck_rw_unlock_shared(&sdp->sd_rwlock);
						break;
					}
				}
				if (vcp->vc_gss.gss_spn && auth_info->ioc_target_size) {
					error = copyout(vcp->vc_gss.gss_spn, auth_info->ioc_target_name, 
									(size_t)auth_info->ioc_target_size);
				}
			}
            
			lck_rw_unlock_shared(&sdp->sd_rwlock);
			break;
		}
		case SMBIOC_VC_PROPERTIES:
		{
			struct smbioc_vc_properties * properties = (struct smbioc_vc_properties *)data;
			
			lck_rw_lock_shared(&sdp->sd_rwlock);
            
            /* free global lock now since we now have sd_rwlock */
            lck_rw_unlock_shared(dev_rw_lck);

			if (properties->ioc_version != SMB_IOC_STRUCT_VERSION) {
				error = EINVAL;
			} else if (!sdp->sd_vc) {
				error = ENOTCONN;
			} else {
				vcp = sdp->sd_vc;
                properties->uid = vcp->vc_uid;
				properties->smb1_caps = vcp->vc_sopt.sv_caps;
                properties->smb2_caps = vcp->vc_sopt.sv_capabilities;
				properties->flags = vcp->vc_flags;
                properties->misc_flags = vcp->vc_misc_flags;
				properties->hflags = vcp->vc_hflags;
				properties->hflags2 = vcp->vc_hflags2;				
				properties->txmax = vcp->vc_txmax;				
				properties->rxmax = vcp->vc_rxmax;
                properties->wxmax = vcp->vc_wxmax;
                memset(properties->model_info, 0, (SMB_MAXFNAMELEN * 2));
                /* only when we are mac to mac */
                if ((vcp->vc_misc_flags & SMBV_OSX_SERVER) && vcp->vc_model_info) {
                    memcpy(properties->model_info, vcp->vc_model_info, strlen(vcp->vc_model_info));
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
			} else if (!sdp->sd_vc || !sdp->sd_share) {
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

			if (!sdp->sd_vc) {
				error = ENOTCONN;
			} else {
				struct smbioc_os_lanman * OSLanman = (struct smbioc_os_lanman *)data;
				vcp = sdp->sd_vc;
				if (vcp->NativeOS)
					strlcpy(OSLanman->NativeOS, vcp->NativeOS, sizeof(OSLanman->NativeOS));
				if (vcp->NativeLANManager)
					strlcpy(OSLanman->NativeLANManager, vcp->NativeLANManager, sizeof(OSLanman->NativeLANManager));
			}

			lck_rw_unlock_shared(&sdp->sd_rwlock);
			break;
		}
		case SMBIOC_SESSSTATE:
		{
			lck_rw_lock_shared(&sdp->sd_rwlock);
            
            /* free global lock now since we now have sd_rwlock */
            lck_rw_unlock_shared(dev_rw_lck);

			/* Check to see if the VC is still up and running */
			if (sdp->sd_vc && (SMB_TRAN_FATAL(sdp->sd_vc, 0) == 0)) {
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
				error = smb_usr_simplerequest(sdp->sd_share, dp, context);
			}
            
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

				/* Take the 32 bit world pointers and convert them to user_addr_t. */
				if (vfs_context_is64bit(context))
					auio = uio_create(1, rwrq->ioc_offset, UIO_USERSPACE64, 
									  (cmd == SMBIOC_READ) ? UIO_READ : UIO_WRITE);
				else {
					rwrq->ioc_kern_base = CAST_USER_ADDR_T(rwrq->ioc_base);
					auio = uio_create(1, rwrq->ioc_offset, UIO_USERSPACE32, 
									  (cmd == SMBIOC_READ) ? UIO_READ : UIO_WRITE);
				}
				if (auio) {
                    smbfh fh;
                    SMBFID fid = 0;

                    uio_addiov(auio, rwrq->ioc_kern_base, rwrq->ioc_cnt);
                    fh = htoles(rwrq->ioc_fh);
                    fid = fh;
                    /* All calls from user maintain a reference on the share */
                    if (cmd == SMBIOC_READ) {
                        error = smb_smb_read(sdp->sd_share, fid, auio, context);
                    }
                    else {
                        int ioFlags = (rwrq->ioc_writeMode & WritethroughMode) ? IO_SYNC : 0;

                        error = smb_smb_write(sdp->sd_share, fid, auio, ioFlags, context);
                    }
                    rwrq->ioc_cnt -= (int32_t)uio_resid(auio);
                    uio_free(auio);
				} 
                else {
					error = ENOMEM;
                }
			}
            
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
				error = smb_usr_fsctl(sdp->sd_share, fsctl, context);
			}
            
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
				error = smb_usr_check_dir(sdp->sd_share, sdp->sd_vc,
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
				error = smb_usr_get_dfs_referral(sdp->sd_share, sdp->sd_vc,
                                                 get_dfs_refer_ioc, context);
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
				error = smb_usr_ioctl(sdp->sd_share, sdp->sd_vc,
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
                SMB_MALLOC(sdp, struct smb_dev *, sizeof(*sdp), M_NSMBDEV, M_WAITOK);
				bzero(sdp, sizeof(*sdp));
				dev = makedev(smb_major, 0);
				sdp->sd_devfs = devfs_make_node(dev, DEVFS_CHAR, UID_ROOT, GID_WHEEL, 0666, "nsmb0");
				if (!sdp->sd_devfs) {
					error = ENOMEM;
					SMBERROR("smb: devfs_make_node 0666");
					(void)cdevsw_remove(smb_major, &nsmb_cdevsw);
					SMB_FREE(sdp, M_NSMBDEV);
					(void)smb_iod_done();
					(void)smb_sm_done();	
				}
				smb_minor_hiwat = 0;
				SMB_GETDEV(dev) = sdp;
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
						SMB_GETDEV(m) = 0;
						if (sdp->sd_devfs)
							devfs_remove(sdp->sd_devfs);
						SMB_FREE(sdp, M_NSMBDEV);
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

