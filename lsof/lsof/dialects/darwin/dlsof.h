/*
 * dlsof.h - Darwin header file for lsof
 */


/*
 * Copyright 1994 Purdue Research Foundation, West Lafayette, Indiana
 * 47907.  All rights reserved.
 *
 * Written by Victor A. Abell
 *
 * This software is not subject to any license of the American Telephone
 * and Telegraph Company or the Regents of the University of California.
 *
 * Permission is granted to anyone to use this software for any purpose on
 * any computer system, and to alter it and redistribute it freely, subject
 * to the following restrictions:
 *
 * 1. Neither the authors nor Purdue University are responsible for any
 *    consequences of the use of this software.
 *
 * 2. The origin of this software must not be misrepresented, either by
 *    explicit claim or by omission.  Credit to the authors and Purdue
 *    University must appear in documentation and sources.
 *
 * 3. Altered versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 * 4. This notice may not be removed or altered.
 */


/*
 * $Id: dlsof.h,v 1.4 2001/02/13 13:50:58 abe Exp $
 */


#if	!defined(DARWIN_LSOF_H)
#define	DARWIN_LSOF_H	1

#include <stdlib.h>
#include <dirent.h>
#include <nlist.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <sys/conf.h>
#include <sys/filedesc.h>
#include <sys/mbuf.h>
#include <sys/ucred.h>
#define m_stat	mnt_stat
#define	KERNEL
#include <sys/mount.h>
#undef	KERNEL
#include <rpc/types.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/un.h>
#include <sys/unpcb.h>
#ifdef	AF_NDRV
#include <net/if_var.h>
#define KERNEL
#include <sys/kern_event.h>
#undef  KERNEL
#include <net/ndrv.h>
#if	DARWINV>=530
#define KERNEL	1
#include <net/ndrv_var.h>
#undef  KERNEL
#endif	/* DARWINV>=530 */
#endif
#ifdef	AF_SYSTEM
#include <sys/queue.h>
#define	KERNEL
#include <sys/kern_event.h>
#undef	KERNEL
#endif
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <net/route.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_timer.h>
#include <netinet/tcp_var.h>
#include <sys/uio.h>
#include <sys/vnode.h>
#include <net/raw_cb.h>
#include <sys/domain.h>
#define	pmap	RPC_pmap
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#undef	pmap

#include <paths.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <nfs/rpcv2.h>
#include <nfs/nfs.h>
#include <nfs/nfsproto.h>
#include <nfs/nfsnode.h>

#if	DARWINV<600
#include <hfs/hfs.h>
#undef	offsetof
#else
#define	KERNEL
#include <hfs/hfs_cnode.h>
#undef	KERNEL
#endif	/* DARWINV<600 */

#include <sys/namei.h>

#define	time	t1		/* hack to make dn_times() happy */
#include <miscfs/devfs/devfsdefs.h>
#undef	time

#define	KERNEL
#include <miscfs/fdesc/fdesc.h>
#undef	KERNEL
#include <sys/proc.h>
#include <kvm.h>
#undef	TRUE
#undef	FALSE

#include <sys/sysctl.h>

/* kinfo_proc.kp_proc */
#define	P_COMM		kp_proc.p_comm
#define	P_PID		kp_proc.p_pid
#define	P_STAT		kp_proc.p_stat
#define	P_VMSPACE	kp_proc.p_vmspace

/* kinfo_proc.kp_eproc */
#define	P_ADDR		kp_eproc.e_paddr
#define	P_PGID		kp_eproc.e_pgid
#define	P_PPID		kp_eproc.e_ppid

/* kinfo_proc.kp_eproc->e_paddr */
#define	P_FD		p_fd

#define	_KERNEL
#define	KERNEL
#include <sys/fcntl.h>
#include <sys/file.h>
#undef	_KERNEL
#undef	KERNEL

struct vop_advlock_args { int dummy; };	/* to pacify lf_advlock() prototype */
#include <sys/lockf.h>

#include <sys/lock.h>

/*
 * Compensate for removal of MAP_ENTRY_IS_A_MAP from <vm/vm_map.h>,
 *  This work-around was supplied by John Polstra <jdp@polstra.com>.
 */

#if	defined(MAP_ENTRY_IS_SUB_MAP) && !defined(MAP_ENTRY_IS_A_MAP)
#define MAP_ENTRY_IS_A_MAP	0
#endif	/* defined(MAP_ENTRY_IS_SUB_MAP) && !defined(MAP_ENTRY_IS_A_MAP) */

#undef	B_NEEDCOMMIT
#include <sys/buf.h>
#include <sys/user.h>

#define	COMP_P		const void
#define DEVINCR		1024	/* device table malloc() increment */
#define	DIRTYPE		dirent	/* directory entry type */

typedef	u_long		KA_T;

#define	KMEM		"/dev/kmem"
#define MALLOC_P	void
#define FREE_P		MALLOC_P
#define MALLOC_S	size_t

#define N_UNIX	"/mach_kernel"

#define QSORT_P		void
#define	READLEN_T	int
#define STRNCPY_L	size_t
#define SWAP		"/dev/drum"


/*
 * Global storage definitions (including their structure definitions)
 */

struct file * Cfp;

extern kvm_t *Kd;

# if	defined(P_ADDR)
extern KA_T Kpa;
# endif	/* defined(P_ADDR) */

struct l_vfs {
	KA_T addr;			/* kernel address */
	fsid_t	fsid;			/* file system ID */

# if	defined(MOUNT_NONE)
	short type;			/* type of file system */
# else	/* !defined(MOUNT_NONE) */
	char *typnm;			/* file system type name */
# endif	/* defined(MOUNT_NONE) */

	char *dir;			/* mounted directory */
	char *fsname;			/* file system name */
	struct l_vfs *next;		/* forward link */
};
extern struct l_vfs *Lvfs;

struct mounts {
        char *dir;              	/* directory (mounted on) */
	char *fsname;           	/* file system
					 * (symbolic links unresolved) */
	char *fsnmres;           	/* file system
					 * (symbolic links resolved) */
        dev_t dev;              	/* directory st_dev */
	dev_t rdev;			/* directory st_rdev */
	ino_t inode;			/* directory st_ino */
	mode_t mode;			/* directory st_mode */
	mode_t fs_mode;			/* file system st_mode */
        struct mounts *next;    	/* forward link */
};

#define	X_NCACHE	"ncache"
#define	X_NCSIZE	"ncsize"
#define	NL_NAME		n_name

extern int Np;				/* number of kernel processes */

extern struct kinfo_proc *P;		/* local process table copy */

struct sfile {
	char *aname;			/* argument file name */
	char *name;			/* file name (after readlink()) */
	char *devnm;			/* device name (optional) */
	dev_t dev;			/* device */
	dev_t rdev;			/* raw device */
	u_short mode;			/* S_IFMT mode bits from stat() */
	int type;			/* file type: 0 = file system
				 	 *	      1 = regular file */
	ino_t i;			/* inode number */
	int f;				/* file found flag */
	struct sfile *next;		/* forward link */

};


/*
 * Definitions for rnmh.c
 */

# if     defined(HASNCACHE)
#include <sys/uio.h>
#include <sys/namei.h>

#define	NCACHE		namecache	/* kernel's structure name */

#define	NCACHE_NM	nc_name		/* name in NCACHE */
#define	NCACHE_NMLEN	nc_nlen		/* name length in NCACHE */
#define	NCACHE_NXT	nc_hash.le_next	/* link in NCACHE */
#define	NCACHE_NODEADDR	nc_vp		/* node address in NCACHE */
#define	NCACHE_PARADDR	nc_dvp		/* parent node address in NCACHE */

#define	NCACHE_NODEID	nc_vpid		/* node ID in NCACHE */
#define	NCACHE_PARID	nc_dvpid	/* parent node ID in NCACHE */
# endif  /* defined(HASNCACHE) */

#endif	/* DARWIN_LSOF_H */
