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
 * $Id: smb_dev.c,v 1.13 2003/08/19 01:34:17 lindak Exp $
 */
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/ioccom.h>
#include <sys/malloc.h>
#include <sys/uio.h>
#include <sys/conf.h>
#include <sys/mbuf.h>
#include <sys/proc.h>
#ifdef APPLE
	#include <sys/filedesc.h>
	/* XXX xnu needs to install poll.h into kernel framework! */
#endif
#include <sys/fcntl.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/poll.h>
#include <sys/socketvar.h>
#include <sys/sysctl.h>
#include <sys/vnode.h>

#include <net/if.h>

#ifdef APPLE
#include <sys/smb_apple.h>
#endif

#include <netsmb/smb.h>
#include <netsmb/smb_conn.h>
#include <netsmb/smb_subr.h>
#include <netsmb/smb_dev.h>

#ifdef APPLE
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
#else
#define SMB_GETDEV(dev)		((struct smb_dev*)(dev)->si_drv1)
#endif /* APPLE */
#define	SMB_CHECKMINOR(dev)	do { \
				    sdp = SMB_GETDEV(dev); \
				    if (sdp == NULL) return ENXIO; \
				} while(0)

static d_open_t	 nsmb_dev_open;
static d_close_t nsmb_dev_close;
static d_ioctl_t nsmb_dev_ioctl;

#ifdef MODULE_DEPEND
MODULE_DEPEND(netsmb, libiconv, 1, 1, 1);
#endif

#ifdef MODULE_VERSION
MODULE_VERSION(netsmb, NSMB_VERSION);
#endif


#ifndef SI_NAMED
#define	SI_NAMED	0
#endif

static int smb_version = NSMB_VERSION;


#ifdef SYSCTL_DECL
SYSCTL_DECL(_net_smb);
#endif
SYSCTL_INT(_net_smb, OID_AUTO, version, CTLFLAG_RD, &smb_version, 0, "");

MALLOC_DEFINE(M_NSMBDEV, "NETSMBDEV", "NET/SMB device");


/*
int smb_dev_queue(struct smb_dev *ndp, struct smb_rq *rqp, int prio);
*/

static struct cdevsw nsmb_cdevsw = {
	nsmb_dev_open,
	nsmb_dev_close,
#ifdef APPLE
	/* read */	eno_rdwrt,
	/* write */	eno_rdwrt,
	nsmb_dev_ioctl,
	eno_stop,
	eno_reset,
	0,
	eno_select,
	eno_mmap,
	eno_strat,
	eno_getc,
	eno_putc,
	0
#else
	nsmb_dev_ioctl,
	NSMB_NAME,
	NSMB_MAJOR,
#endif
};


#ifdef APPLE
int	smb_major = -1;

int	smb_dev_lock_init = 0;
struct lock__bsd__ smb_dev_lock;

static __inline__ int
SMB_DEV_LOCK()
{
    return (lockmgr(&smb_dev_lock, LK_EXCLUSIVE, NULL, current_proc()));
}

static __inline__ int
SMB_DEV_UNLOCK()
{
    return (lockmgr(&smb_dev_lock, LK_RELEASE, NULL, current_proc()));
}
#endif


static int
#ifdef APPLE
nsmb_dev_open_nolock(dev_t dev, int oflags, int devtype, struct proc *p)
#else
nsmb_dev_open(dev_t dev, int oflags, int devtype, struct proc *p)
#endif /* APPLE */
{
	#pragma unused(oflags, devtype)
	struct smb_dev *sdp;
	struct ucred *cred = p->p_ucred;
	int s;

	sdp = SMB_GETDEV(dev);
	if (sdp && (sdp->sd_flags & NSMBFL_OPEN))
		return EBUSY;
#ifdef APPLE
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
		sdp->sd_devfs = devfs_make_node(dev, DEVFS_CHAR, cred->cr_uid,
						cred->cr_gid, 0700, "nsmb%x",
						avail_minor);
		if (!sdp->sd_devfs)
			panic("smb: devfs_make_node 0700");
		if (avail_minor > smb_minor_hiwat)
			smb_minor_hiwat = avail_minor;
		SMB_GETDEV(dev) = sdp;
		return EBUSY;
	}
#else
	if (sdp == NULL) {
		sdp = malloc(sizeof(*sdp), M_NSMBDEV, M_WAITOK);
		dev->si_drv1 = (void*)sdp;
	}
	/*
	 * XXX: this is just crazy - make a device for an already passed device...
	 * someone should take care of it.
	 */
	if ((dev->si_flags & SI_NAMED) == 0)
		make_dev(&nsmb_cdevsw, minor(dev), cred->cr_uid, cred->cr_gid, 0700,
		    NSMB_NAME"%d", lminor(dev));
	bzero(sdp, sizeof(*sdp));
#endif
/*
	STAILQ_INIT(&sdp->sd_rqlist);
	STAILQ_INIT(&sdp->sd_rplist);
	bzero(&sdp->sd_pollinfo, sizeof(struct selinfo));
*/
	s = splimp();
	sdp->sd_level = -1;
	sdp->sd_flags |= NSMBFL_OPEN;
	splx(s);
	return 0;
}

#ifdef APPLE
static int
nsmb_dev_open(dev_t dev, int oflags, int devtype, struct proc *p)
{
	int error;

	SMB_DEV_LOCK();
	error = nsmb_dev_open_nolock(dev, oflags, devtype, p);
	SMB_DEV_UNLOCK();
	return (error);
}
#endif /* APPLE */

static int
#ifdef APPLE
nsmb_dev_close_nolock(dev_t dev, int flag, int fmt, struct proc *p)
#else
nsmb_dev_close(dev_t dev, int flag, int fmt, struct proc *p)
#endif /* APPLE */
{
	#pragma unused(flag, fmt)
	struct smb_dev *sdp;
	struct smb_vc *vcp;
	struct smb_share *ssp;
	struct smb_cred scred;
	int s;

	SMB_CHECKMINOR(dev);
	s = splimp();
	if ((sdp->sd_flags & NSMBFL_OPEN) == 0) {
		splx(s);
		return EBADF;
	}
	smb_makescred(&scred, p, NULL);
	ssp = sdp->sd_share;
	if (ssp != NULL)
		smb_share_rele(ssp, &scred);
	vcp = sdp->sd_vc;
	if (vcp != NULL)
		smb_vc_rele(vcp, &scred);
/*
	smb_flushq(&sdp->sd_rqlist);
	smb_flushq(&sdp->sd_rplist);
*/
#ifdef APPLE
	devfs_remove(sdp->sd_devfs); /* first disallow opens */
	SMB_GETDEV(dev) = NULL;
	free(sdp, M_NSMBDEV);
#else
#if __FreeBSD_version > 400001
	dev->si_drv1 = NULL;
	free(sdp, M_NSMBDEV);
	destroy_dev(dev);
#else
	sdp->sd_flags &= ~NSMBFL_OPEN;
#endif
#endif
	splx(s);
	return 0;
}


#ifdef APPLE
static int
nsmb_dev_close(dev_t dev, int flag, int fmt, struct proc *p)
{
	int error;

	SMB_DEV_LOCK();
	error = nsmb_dev_close_nolock(dev, flag, fmt, p);
	SMB_DEV_UNLOCK();
	return (error);
}
#endif /* APPLE */

static int
nsmb_dev_ioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	#pragma unused(flag)
	struct smb_dev *sdp;
	struct smb_vc *vcp;
	struct smb_share *ssp;
	struct smb_cred scred;
	int error = 0;

	SMB_CHECKMINOR(dev);
	if ((sdp->sd_flags & NSMBFL_OPEN) == 0)
		return EBADF;

	smb_makescred(&scred, p, NULL);
	switch (cmd) {
#ifndef APPLE
	    case SMBIOC_OPENSESSION:
		if (sdp->sd_vc)
			return EISCONN;
		error = smb_usr_opensession((struct smbioc_ossn*)data,
		    &scred, &vcp);
		if (error)
			break;
		sdp->sd_vc = vcp;
		smb_vc_unlock(vcp, 0, p);
		sdp->sd_level = SMBL_VC;
		break;
	    case SMBIOC_OPENSHARE:
		if (sdp->sd_share)
			return EISCONN;
		if (sdp->sd_vc == NULL)
			return ENOTCONN;
		error = smb_usr_openshare(sdp->sd_vc,
		    (struct smbioc_oshare*)data, &scred, &ssp);
		if (error)
			break;
		sdp->sd_share = ssp;
		smb_share_unlock(ssp, 0, p);
		sdp->sd_level = SMBL_SHARE;
		break;
#endif /* !APPLE */
	    case SMBIOC_REQUEST:
		if (sdp->sd_share == NULL)
			return ENOTCONN;
		error = smb_usr_simplerequest(sdp->sd_share,
		    (struct smbioc_rq*)data, &scred);
		break;
	    case SMBIOC_T2RQ:
		if (sdp->sd_share == NULL)
			return ENOTCONN;
		error = smb_usr_t2request(sdp->sd_share,
		    (struct smbioc_t2rq*)data, &scred);
		break;
#ifndef APPLE
	    case SMBIOC_SETFLAGS: {
		struct smbioc_flags *fl = (struct smbioc_flags*)data;
		int on;
	
		if (fl->ioc_level == SMBL_VC) {
			if (fl->ioc_mask & SMBV_PERMANENT) {
				on = fl->ioc_flags & SMBV_PERMANENT;
				if ((vcp = sdp->sd_vc) == NULL)
					return ENOTCONN;
				error = smb_vc_get(vcp, LK_EXCLUSIVE, &scred);
				if (error)
					break;
				if (on && (vcp->obj.co_flags & SMBV_PERMANENT) == 0) {
					vcp->obj.co_flags |= SMBV_PERMANENT;
					smb_vc_ref(vcp, p);
				} else if (!on && (vcp->obj.co_flags & SMBV_PERMANENT)) {
					vcp->obj.co_flags &= ~SMBV_PERMANENT;
					smb_vc_rele(vcp, &scred);
				}
				smb_vc_put(vcp, &scred);
			} else
				error = EINVAL;
		} else if (fl->ioc_level == SMBL_SHARE) {
			if (fl->ioc_mask & SMBS_PERMANENT) {
				on = fl->ioc_flags & SMBS_PERMANENT;
				if ((ssp = sdp->sd_share) == NULL)
					return ENOTCONN;
				error = smb_share_get(ssp, LK_EXCLUSIVE, &scred);
				if (error)
					break;
				if (on && (ssp->obj.co_flags & SMBS_PERMANENT) == 0) {
					ssp->obj.co_flags |= SMBS_PERMANENT;
					smb_share_ref(ssp, p);
				} else if (!on && (ssp->obj.co_flags & SMBS_PERMANENT)) {
					ssp->obj.co_flags &= ~SMBS_PERMANENT;
					smb_share_rele(ssp, &scred);
				}
				smb_share_put(ssp, &scred);
			} else
				error = EINVAL;
			break;
		} else
			error = EINVAL;
		break;
	    }
	    case SMBIOC_LOOKUP:
		if (sdp->sd_vc || sdp->sd_share)
			return EISCONN;
		vcp = NULL;
		ssp = NULL;
		error = smb_usr_lookup((struct smbioc_lookup*)data, &scred, &vcp, &ssp);
		if (error)
			break;
		if (vcp) {
			sdp->sd_vc = vcp;
			smb_vc_unlock(vcp, 0, p);
			sdp->sd_level = SMBL_VC;
		}
		if (ssp) {
			sdp->sd_share = ssp;
			smb_share_unlock(ssp, 0, p);
			sdp->sd_level = SMBL_SHARE;
		}
		break;
#endif /* !APPLE */
	    case SMBIOC_NEGOTIATE:
		if (sdp->sd_vc || sdp->sd_share)
			return EISCONN;
		vcp = NULL;
		ssp = NULL;
		error = smb_usr_negotiate((struct smbioc_lookup*)data, &scred,
					  &vcp, &ssp);
		if (error)
			break;
		if (vcp) {
			sdp->sd_vc = vcp;
			smb_vc_unlock(vcp, 0, p);
			sdp->sd_level = SMBL_VC;
		}
		if (ssp) {
			sdp->sd_share = ssp;
			smb_share_unlock(ssp, 0, p);
			sdp->sd_level = SMBL_SHARE;
		}
		break;
	    case SMBIOC_SSNSETUP:
		if (sdp->sd_share)
			return EISCONN;
		if (!sdp->sd_vc)
			return ENOTCONN;
		vcp = sdp->sd_vc;
		ssp = NULL;
		error = smb_usr_ssnsetup((struct smbioc_lookup*)data, &scred,
					 vcp, &ssp);
		if (error)
			break;
		smb_vc_put(vcp, &scred);
		if (ssp) {
			sdp->sd_share = ssp;
			smb_share_unlock(ssp, 0, p);
			sdp->sd_level = SMBL_SHARE;
		}
		break;
	    case SMBIOC_TDIS:
		if (sdp->sd_share == NULL)
			return ENOTCONN;
		smb_share_rele(sdp->sd_share, &scred);
		sdp->sd_share = NULL;
		sdp->sd_level = SMBL_VC;
		break;
	    case SMBIOC_TCON:
		if (sdp->sd_share)
			return EISCONN;
		if (!sdp->sd_vc)
			return ENOTCONN;
		vcp = sdp->sd_vc;
		ssp = NULL;
		error = smb_usr_tcon((struct smbioc_lookup*)data, &scred,
				     vcp, &ssp);
		if (error)
			break;
		smb_vc_put(vcp, &scred);
		if (ssp) {
			sdp->sd_share = ssp;
			smb_share_unlock(ssp, 0, p);
			sdp->sd_level = SMBL_SHARE;
		}
		break;
#ifndef APPLE
	    case SMBIOC_READ: case SMBIOC_WRITE: {
		struct smbioc_rw *rwrq = (struct smbioc_rw*)data;
		struct uio auio;
		struct iovec iov;
	
		if ((ssp = sdp->sd_share) == NULL)
			return ENOTCONN;
		iov.iov_base = rwrq->ioc_base;
		iov.iov_len = rwrq->ioc_cnt;
		auio.uio_iov = &iov;
		auio.uio_iovcnt = 1;
		auio.uio_offset = rwrq->ioc_offset;
		auio.uio_resid = rwrq->ioc_cnt;
		auio.uio_segflg = UIO_USERSPACE;
		auio.uio_rw = (cmd == SMBIOC_READ) ? UIO_READ : UIO_WRITE;
		auio.uio_procp = p;
		/*
		 * XXX use page cache for coherence?
		 */
		if (cmd == SMBIOC_READ)
			error = smb_read(ssp, rwrq->ioc_fh, &auio, &scred);
		else
			error = smb_write(ssp, rwrq->ioc_fh, &auio, &scred,
					  SMBWRTTIMO);
		rwrq->ioc_cnt -= auio.uio_resid;
		break;
	    }
#endif /* !APPLE */
	    default:
		error = ENODEV;
	}
	return error;
}


PRIVSYM
#ifndef APPLE /* so we can call from smbfs_module_start in smbfs_vfsops.c */
static
#endif
int
nsmb_dev_load(module_t mod, int cmd, void *arg)
{
	#pragma unused(mod, arg)
	int error = 0;

	switch (cmd) {
	    case MOD_LOAD:
#ifdef APPLE
		if (!smb_dev_lock_init)
			lockinit(&smb_dev_lock, PINOD, "smb_dev", 0, 0);
		smb_dev_lock_init++;
		SMB_DEV_LOCK();
#endif
		error = smb_checksmp();
		if (error)
			break;
		error = smb_sm_init();
		if (error)
			break;
		error = smb_iod_init();
		if (error) {
			smb_sm_done();
			break;
		}
#if __FreeBSD_version > 400001
		cdevsw_add(&nsmb_cdevsw);
#endif
#ifdef APPLE
		if (smb_major == -1) {
			dev_t dev;
			struct smb_dev *sdp;

			smb_major = cdevsw_add(-1, &nsmb_cdevsw);
			if (smb_major == -1)
				panic("smb: cdevsw_add");
			sdp = malloc(sizeof(*sdp), M_NSMBDEV, M_WAITOK);
			bzero(sdp, sizeof(*sdp));
			dev = makedev(smb_major, 0);
			sdp->sd_devfs = devfs_make_node(dev, DEVFS_CHAR,
							UID_ROOT, GID_WHEEL,
							0666, "nsmb0");
			if (!sdp->sd_devfs)
				panic("smb: devfs_make_node 0666");
			smb_minor_hiwat = 0;
			SMB_GETDEV(dev) = sdp;
		}
		SMB_DEV_UNLOCK();
#endif
		printf("netsmb_dev: loaded\n");
		break;
	    case MOD_UNLOAD:
#ifdef APPLE
		SMB_DEV_LOCK();
#endif
		smb_iod_done();
		error = smb_sm_done();
		error = 0;
#if __FreeBSD_version > 400001
		cdevsw_remove(&nsmb_cdevsw);
#endif
#ifdef APPLE
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
				panic("smb: cdevsw_remove failed");
			smb_major = -1;
		}
		SMB_DEV_UNLOCK();
#endif
		printf("netsmb_dev: unloaded\n");
		break;
	    default:
		error = EINVAL;
		break;
	}
	return error;
}

#if __FreeBSD_version > 400000
DEV_MODULE (dev_netsmb, nsmb_dev_load, 0);
#else
CDEV_MODULE(dev_netsmb, NSMB_MAJOR, nsmb_cdevsw, nsmb_dev_load, 0);
#endif


/*
 * Convert a file descriptor to appropriate smb_share pointer
 */
static struct file*
nsmb_getfp(struct filedesc* fdp, int fd, int flag)
{
	struct file* fp;

	if (fd >= fdp->fd_nfiles ||
	    (fp = fdp->fd_ofiles[fd]) == NULL ||
	    (fp->f_flag & flag) == 0)
		return (NULL);
	return (fp);
}

int
smb_dev2share(int fd, int mode, struct smb_cred *scred,
	struct smb_share **sspp)
{
	#pragma unused(mode)
	struct file *fp;
	struct vnode *vp;
	struct smb_dev *sdp;
	struct smb_share *ssp;
	dev_t dev;

	if ((fp = nsmb_getfp(scred->scr_p->p_fd, fd, FREAD | FWRITE)) == NULL)
		return EBADF;
	vp = (struct vnode*)fp->f_data;
	if (vp == NULL)
		return EBADF;
	dev = vn_todev(vp);
	if (dev == NODEV)
		return EBADF;
	SMB_CHECKMINOR(dev);
	ssp = sdp->sd_share;
	if (ssp == NULL)
		return ENOTCONN;
	/*
	 * The share is already locked and referenced by the TCON ioctl
	 * We NULL to hand off share to caller (mount)
	 * This allows further ioctls against connection, for instance
	 * another tree connect and mount, in the automounter case
	 */
	sdp->sd_share = NULL;
	*sspp = ssp;
	return 0;
}

