/*
 * dlsof.h - FreeBSD header file for lsof
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
 * $Id: dlsof.h,v 1.24 2001/10/17 19:21:51 abe Exp $
 */


#if	!defined(FREEBSD_LSOF_H)
#define	FREEBSD_LSOF_H	1

#include <stdlib.h>
#include <dirent.h>
#include <nlist.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

# if	FREEBSDV>=400
#  if	FREEBSDV>=500
#define	_KERNEL	1
#  endif	/* FREEBSDV>=500 */
#include <sys/conf.h>
#  if	FREEBSDV>=500
#undef	_KERNEL	1
#  endif	/* FREEBSDV>=500 */
# endif	/* FREEBSDV>=400 */

#include <sys/filedesc.h>
#include <sys/mbuf.h>
#define	NFS
#define m_stat	mnt_stat

# if	FREEBSDV>=320
#define	_KERNEL
# endif	/* FREEBSDV>=320 */

#include <sys/mount.h>

# if	FREEBSDV>=320
#undef	_KERNEL
# endif	/* FREEBSDV>=320 */

#include <rpc/types.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/un.h>
#include <sys/unpcb.h>

# if	FREEBSDV>=300
#undef	INADDR_LOOPBACK
# endif	/* FREEBSDV>=300 */

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
#include <sys/ucred.h>
#include <sys/uio.h>
#include <sys/vnode.h>
#include <net/raw_cb.h>
#include <sys/domain.h>
#define	pmap	RPC_pmap
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#undef	pmap

# if	FREEBSDV<200
#include <ufs/quota.h>
#include <ufs/inode.h>
#include <ufs/ufsmount.h>
#include <ufs/mfsnode.h>
# else	/* FREEBSDV>=200 */
#include <paths.h>
#include <ufs/ufs/quota.h>

#  if	FREEBSDV>=400 && defined(__alpha__)
#define	dev_t	void *
#  endif	/* FREEBSDV>=400 && defined(__alpha__) */

#include <ufs/ufs/inode.h>

#  if	FREEBSDV>=400 && defined(__alpha__)
#undef	dev_t
#  endif	/* FREEBSDV>=400 && defined(__alpha__) */

#  if   FREEBSDV<220
#include <ufs/mfs/mfsnode.h>
#  endif        /* FREEBSDV<220 */

# endif	/* FREEBSDV<200 */

# if	FREEBSDV<500
#include <nfs/nfsv2.h>
# else	/* FREEBSDV>=500 */
#include <nfs/nfsproto.h>
# endif	/* FREEBSDV<500 */

# if	defined(HASRPCV2H) || FREEBSDV>=400
#include <nfs/rpcv2.h>
# endif	/* defined(HASRPCV2H) || FREEBSDV>=400 */

# if	FREEBSDV>=500
#include <nfsclient/nfs.h>
#include <nfsclient/nfsnode.h>
# else	/* FREEBSDV<500 */
#include <nfs/nfs.h>
#include <nfs/nfsnode.h>
# endif	/* FREEBSDV>=500 */

#include <sys/proc.h>
#include <kvm.h>
#undef	TRUE
#undef	FALSE

# if	FREEBSDV<200
#include <sys/kinfo.h>
# else	/* FREEBSDV>=200 */
#include <sys/sysctl.h>
# endif	/* FREEBSDV<200 */

# if	defined(HASFDESCFS)
#define	_KERNEL
#define	KERNEL
#  if	FREEBSDV>=500
#include <fs/fdescfs/fdesc.h>
#  else	/* FREEBSDV<500 */
#include <miscfs/fdesc/fdesc.h>
#  endif	/* FREEBSDV>=500 */
#undef	_KERNEL
#undef	KERNEL
# endif	/* defined(HASFDESCFS) */

# if	defined(HASPROCFS)
#  if	FREEBSDV<200
#include <procfs/pfsnode.h>
# else	/* FREEBSDV>=200 */
#  if	FREEBSDV<500
#include <miscfs/procfs/procfs.h>
#  else	/* FREEBSDV>=500 */
#include <fs/procfs/procfs.h>
#  endif	/* FREEBSDV<500 */
#include <machine/reg.h>
# endif	/* FREEBSDV<200 */

#define	PNSIZ		5
# endif	/* defined(HASPROCFS) */

# if	FREEBSDV<200
#define	P_COMM		p_comm
#define	P_FD		p_fd
#define	P_PID		p_pid
#define	P_PGID		p_pgrp
#define	P_STAT		p_stat
#define	P_VMSPACE	p_vmspace
# else	/* FREEBSDV>=200 */
#  if	FREEBSDV<500
#define	P_ADDR		kp_eproc.e_paddr
#define	P_COMM		kp_proc.p_comm
#define	P_FD		kp_proc.p_fd
#define	P_PID		kp_proc.p_pid
#define	P_PGID		kp_eproc.e_pgid
#define	P_PPID		kp_eproc.e_ppid
#define	P_STAT		kp_proc.p_stat
#define	P_VMSPACE	kp_proc.p_vmspace
#  else	/* FREEBSDV>=500 */
#define	P_ADDR		ki_paddr
#define	P_COMM		ki_comm
#define	P_FD		ki_fd
#define	P_PID		ki_pid
#define	P_PGID		ki_pgid
#define	P_PPID		ki_ppid
#define	P_STAT		ki_stat
#define	P_VMSPACE	ki_vmspace
#  endif	/* FREEBSDV<500 */
# endif	/* FREEBSDV<200 */

#define	_KERNEL
#define	KERNEL
#include <sys/fcntl.h>
#include <sys/file.h>
#undef	_KERNEL
#undef	KERNEL

# if	FREEBSDV<200
#include <ufs/lockf.h>
# else	/* FREEBSDV>=200 */
struct vop_advlock_args { int dummy; };	/* to pacify lf_advlock() prototype */
#  if	FREEBSDV>=500
#undef	MALLOC_DECLARE
#define	MALLOC_DECLARE(type)	extern struct malloc_type type[1]
					/* to pacify <sys/lockf.h> */
#define	_KERNEL
#include <fs/devfs/devfs.h>
#undef	_KERNEL
#  endif	/* FREEBSDV>=500 */
#include <sys/lockf.h>
# endif	/* FREEBSDV<200 */

#include <vm/vm.h>

#  if   FREEBSDV>=220
#include <sys/pipe.h>
#   if	defined(HASVMLOCKH)
#include <vm/lock.h>
#   endif	/* defined(HASVMLOCKH) */
#include <vm/pmap.h>
#  endif        /* FREEBSDV>=220 */

#include <vm/vm_map.h>

/*
 * Compensate for removal of MAP_ENTRY_IS_A_MAP from <vm/vm_map.h>,
 *  This work-around was supplied by John Polstra <jdp@polstra.com>.
 */

#if	defined(MAP_ENTRY_IS_SUB_MAP) && !defined(MAP_ENTRY_IS_A_MAP)
#define MAP_ENTRY_IS_A_MAP	0
#endif	/* defined(MAP_ENTRY_IS_SUB_MAP) && !defined(MAP_ENTRY_IS_A_MAP) */

#include <vm/vm_object.h>
#include <vm/vm_pager.h>

#  if   FREEBSDV>=220
#undef	B_NEEDCOMMIT

#   if	FREEBSDV>=500
#include <sys/bio.h>
#   endif	/* FREEBSDV>=500 */

#include <sys/buf.h>
#include <sys/user.h>

#   if	FREEBSDV<500
#include <ufs/mfs/mfsnode.h>
#   endif	/* FREEBSDV<500 */
#  endif        /* FREEBSDV>=220 */


#define	COMP_P		const void
#define DEVINCR		1024	/* device table malloc() increment */

# if	FREEBSDV<200
typedef	off_t		KA_T;
# else	/* FREEBSDV>=200 */
typedef	u_long		KA_T;
# endif	/* FREEBSDV<200 */

#define	KMEM		"/dev/kmem"
#define MALLOC_P	void
#define FREE_P		MALLOC_P
#define MALLOC_S	size_t

# if	defined(N_UNIXV)
#define	N_UNIX_TMP(x)	#x
#define	N_UNIX_STR(x)	N_UNIX_TMP(x)
#define	N_UNIX		N_UNIX_STR(N_UNIXV)
# endif	/* defined(N_UNIXV) */

#define QSORT_P		void
#define	READLEN_T	int
#define STRNCPY_L	size_t
#define SWAP		"/dev/drum"
#define	SZOFFTYPE	long long	/* size and offset internal storage
					 * type */
#define	SZOFFPSPEC	"ll"		/* SZOFFTYPE print specification
					 * modifier */


/*
 * Global storage definitions (including their structure definitions)
 */

struct file * Cfp;

# if	FREEBSDV>=200
extern kvm_t *Kd;
# endif	/* FREEBSDV>=200 */

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

# if	FREEBSDV>=200
extern struct kinfo_proc *P;		/* local process table copy */
# endif	/* FREEBSDV>=200 */

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
 * Definitions for rdev.c
 */

#define	DIRTYPE	dirent
#define HASDNAMLEN	1	/* struct DIRTYPE has d_namlen element */


/*
 * Definitions for rnam.c
 */

# if     defined(HASNCACHE)
#include <sys/uio.h>
#  if	FREEBSDV<400 || (FREEBSDV>=400 && defined(HASNAMECACHE))
#include <sys/namei.h>
#  else	/* FREEBSDV>=400 && !defined(HASNAMECACHE) */
/*
 * The namecache struct definition should come from a header file that
 * can be #include'd, but it has been moved to a kernel source file in
 * 4.0-current for some reason unclear to me.
 *
 * So we must take the risk of defining it here. !!!! DANGER !!!!
 */

struct	namecache {
	LIST_ENTRY(namecache) nc_hash;	/* hash chain */
	LIST_ENTRY(namecache) nc_src;	/* source vnode list */
	TAILQ_ENTRY(namecache) nc_dst;	/* destination vnode list */
	struct	vnode *nc_dvp;		/* vnode of parent of name */
	struct	vnode *nc_vp;		/* vnode the name refers to */
	u_char	nc_flag;		/* flag bits */
	u_char	nc_nlen;		/* length of name */
	char	nc_name[0];		/* segment name */
};
#  endif	/* FREEBSDV<400 || (FREEBSDV>=400 && defined(HASNAMECACHE)) */

#define	NCACHE		namecache	/* kernel's structure name */
#define	NCACHE_NM	nc_name		/* name in NCACHE */
#define	NCACHE_NMLEN	nc_nlen		/* name length in NCACHE */

#  if	FREEBSDV<205
#define	NCACHE_NXT	nc_nxt		/* link in NCACHE */
#  else	/* FREEBSDV>=205 */
#   if	FREEBSDV<210
#define	NCACHE_NXT	nc_lru.tqe_next	/* link in NCACHE */
#   else	/* FREEBSDV>=210 */
#include <stddef.h>
#define	NCACHE_NXT	nc_hash.le_next	/* link in NCACHE */
#   endif	/* FREEBSDV<210 */
#  endif	/* FREEBSDV<205 */

#define	NCACHE_NODEADDR	nc_vp		/* node address in NCACHE */
#define	NCACHE_PARADDR	nc_dvp		/* parent node address in NCACHE */

#  if	defined(HASNCVPID)
#define	NCACHE_NODEID	nc_vpid		/* node ID in NCACHE */
#define	NCACHE_PARID	nc_dvpid	/* parent node ID in NCACHE */
#  endif	/* DEFINED(HASNCVPID) */
# endif  /* defined(HASNCACHE) */

#endif	/* FREEBSD_LSOF_H */
