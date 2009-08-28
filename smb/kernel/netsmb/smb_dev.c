/*
 * Copyright (c) 2000-2001 Boris Popov
 * All rights reserved.
 *
 * Portions Copyright (C) 2001 - 2008 Apple Inc. All rights reserved.
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
#include <sys/mchain.h>

#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_dev.h>
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
#define	SMB_CHECKMINOR(sdp, dev)	do { \
				    sdp = SMB_GETDEV(dev); \
				    if (sdp == NULL) \
					return (ENXIO); \
				} while(0)


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

extern lck_mtx_t  * dev_lck;

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
		if (avail_minor >= SMBMINORS)
			panic("smb: %d minor devices!", avail_minor);
		sdp = malloc(sizeof(*sdp), M_NSMBDEV, M_WAITOK);
		bzero(sdp, sizeof(*sdp));
		dev = makedev(smb_major, avail_minor);
		sdp->sd_devfs = devfs_make_node(dev, DEVFS_CHAR,
						kauth_cred_getuid(cred),
						kauth_cred_getgid(cred),
						0700, "nsmb%x", avail_minor);
		if (!sdp->sd_devfs)
			panic("smb: devfs_make_node 0700");
		if (avail_minor > smb_minor_hiwat)
			smb_minor_hiwat = avail_minor;
		SMB_GETDEV(dev) = sdp;
		return (EBUSY);
	}
	sdp->sd_flags |= NSMBFL_OPEN;
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
	lck_mtx_lock(dev_lck);
	error = nsmb_dev_open_nolock(dev, oflags, devtype, p);
	lck_mtx_unlock(dev_lck);
	return (error);
}

static int
nsmb_dev_close_nolock(dev_t dev, int flag, int fmt, struct proc *p)
{
#pragma unused(flag, fmt, p)
	struct smb_dev *sdp;
	struct smb_vc *vcp;
	struct smb_share *ssp;
	vfs_context_t context;

	SMB_CHECKMINOR(sdp, dev);
	if ((sdp->sd_flags & NSMBFL_OPEN) == 0)
		return (EBADF);

	context = vfs_context_create((vfs_context_t)0);
	ssp = sdp->sd_share;
	sdp->sd_share = NULL; /* Just to be extra carefull */
	if (ssp != NULL)
		smb_share_rele(ssp, context);
	vcp = sdp->sd_vc;
	sdp->sd_vc = NULL; /* Just to be extra carefull */
	if (vcp != NULL) 
		smb_vc_rele(vcp, context);

	devfs_remove(sdp->sd_devfs); /* first disallow opens */

	vfs_context_rele(context);

	SMB_GETDEV(dev) = NULL;
	free(sdp, M_NSMBDEV);
	return (0);
}


static int
nsmb_dev_close(dev_t dev, int flag, int fmt, struct proc *p)
{
	int error;

	lck_mtx_lock(dev_lck);
	error = nsmb_dev_close_nolock(dev, flag, fmt, p);
	lck_mtx_unlock(dev_lck);
	return (error);
}

static int nsmb_dev_ioctl(dev_t dev, u_long cmd, caddr_t data, int flag, 
						  struct proc *p)
{
#pragma unused(flag, p)
	struct smb_dev *sdp;
	struct smb_vc *vcp;
	u_int32_t error = 0;
	vfs_context_t context;

	SMB_CHECKMINOR(sdp, dev);
	if ((sdp->sd_flags & NSMBFL_OPEN) == 0)
		return (EBADF);

	context = vfs_context_create((vfs_context_t)0);
	/* 
	  *%%% K64 
	 * Need to keep checking to see if this gets corrected. The problem here
	 * is ioctl_cmd_t is u_int32_t on K64 builds. The _IO defines use sizeof
	 * which returns a size_t. Hopefully either cmd will be changed to u_long
	 * or the _IO defines will have sizeof typed cast to u_int32_t.
	 */
	switch (cmd) {
		case SMBIOC_NEGOTIATE:
		{
			struct smbioc_negotiate * vspec = (struct smbioc_negotiate *)data;
			
			/* Make sure the version matches */
			if (vspec->ioc_version != SMB_IOC_STRUCT_VERSION) {
				error = EINVAL;
			} else if (sdp->sd_vc || sdp->sd_share) {
				error = EISCONN;
			} else {
				error = smb_usr_negotiate(vspec, context, sdp);				
			}			
			break;
		}
		case SMBIOC_SSNSETUP: 
		{
			struct smbioc_setup * sspec = (struct smbioc_setup *)data;
			
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
			break;
		}
		case SMBIOC_TCON:
		{
			struct smbioc_share * shspec = (struct smbioc_share *)data;
			
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
			break;
		}
		case SMBIOC_TDIS: 
		{
			struct smbioc_share * shspec = (struct smbioc_share *)data;
			
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
			break;			
		}
		case SMBIOC_GET_VC_FLAGS:
		{
			/* Get them the current settings of the vc flags */
			if (!sdp->sd_vc) {
				error = ENOTCONN;
			} else {
				vcp = sdp->sd_vc;
				*(u_int32_t *)data = vcp->vc_flags;				
			}
			break;
		}
		case SMBIOC_GET_OS_LANMAN:
		{
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
			break;
		}
		case SMBIOC_GET_VC_FLAGS2:
		{
			if (!sdp->sd_vc) {
				error = ENOTCONN;
			} else {
				vcp = sdp->sd_vc;
				*(u_int16_t *)data = vcp->vc_hflags2;
			}
			break;			
		}
		case SMBIOC_SESSSTATE:
		{
			/* Check to see if the VC is still up and running */
			if (sdp->sd_vc && (SMB_TRAN_FATAL(sdp->sd_vc, 0) == 0))
				*(u_int16_t *)data = EISCONN;
			else 
				*(u_int16_t *)data = ENOTCONN;
			break;			
		}
		case SMBIOC_CANCEL_SESSION:
		{
			sdp->sd_flags |= NSMBFL_CANCEL;
			break;			
		}
		case SMBIOC_REQUEST: 
		{
			struct smbioc_rq * dp = (struct smbioc_rq *)data;
			
			/* Make sure the version match */
			if (dp->ioc_version != SMB_IOC_STRUCT_VERSION) {
				error = EINVAL;
			}
			else if (sdp->sd_share == NULL) {
				error = ENOTCONN;
			} else {
				error = smb_usr_simplerequest(sdp->sd_share, dp, context);
			}
			break;
		}
		case SMBIOC_T2RQ: 
		{
			struct smbioc_t2rq * dp2 = (struct smbioc_t2rq *)data;
			
			/* Make sure the version match */
			if (dp2->ioc_version != SMB_IOC_STRUCT_VERSION) {
				error = EINVAL;
			} else if (sdp->sd_share == NULL) {
				error = ENOTCONN;
			} else {
				error = smb_usr_t2request(sdp->sd_share, dp2, context);				
			}
			break;		
		}
		case SMBIOC_READ: 
		case SMBIOC_WRITE: 
		{
			struct smbioc_rw *rwrq = (struct smbioc_rw *)data;
			
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

					uio_addiov(auio, rwrq->ioc_kern_base, rwrq->ioc_cnt);
					fh = htoles(rwrq->ioc_fh);
					if (cmd == SMBIOC_READ)
						error = smb_read(sdp->sd_share, fh, auio, context);
					else
						error = smb_write(sdp->sd_share, fh, auio, context, SMBWRTTIMO);
					rwrq->ioc_cnt -= (int32_t)uio_resid(auio);
					uio_free(auio);
					
				} else
					error = ENOMEM;
			}
			break;
		}
		default:
		{
			error = ENODEV;
			break;
		}
	}

	vfs_context_rele(context);
	return (error);
}


static int nsmb_dev_load(module_t mod, int cmd, void *arg)
{
#pragma unused(mod, arg)
	int error = 0;

	lck_mtx_lock(dev_lck);
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
				sdp = malloc(sizeof(*sdp), M_NSMBDEV, M_WAITOK);
				bzero(sdp, sizeof(*sdp));
				dev = makedev(smb_major, 0);
				sdp->sd_devfs = devfs_make_node(dev, DEVFS_CHAR, UID_ROOT, GID_WHEEL, 0666, "nsmb0");
				if (!sdp->sd_devfs) {
					error = ENOMEM;
					SMBERROR("smb: devfs_make_node 0666");
					(void)cdevsw_remove(smb_major, &nsmb_cdevsw);
					free(sdp, M_NSMBDEV);
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
						free(sdp, M_NSMBDEV);
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
	lck_mtx_unlock(dev_lck);
	return (error);
}

DEV_MODULE(dev_netsmb, nsmb_dev_load, 0);

int
smb_dev2share(int fd, struct smb_share **sspp)
{
	vnode_t vp;
	struct smb_dev *sdp;
	struct smb_share *ssp;
	dev_t dev = NODEV;
	int error;

	error = file_vnode_withvid(fd, &vp, NULL);
	if (error)
		return (error);
	if (vp)
		dev = vn_todev(vp);
	if (dev == NODEV) {
		file_drop(fd);
		return (EBADF);
	}
	SMB_CHECKMINOR(sdp, dev);
	ssp = sdp->sd_share;
	if (ssp == NULL) {
		file_drop(fd);
		return (ENOTCONN);		
	}
	/*
	 * The share is already referenced by the TCON ioctl
	 * We NULL to hand off share to caller (mount)
	 * This allows further ioctls against connection, for instance
	 * another tree connect and mount, in the automounter case
	 */
	sdp->sd_share = NULL;
	*sspp = ssp;
	file_drop(fd);
	return (0);
}

