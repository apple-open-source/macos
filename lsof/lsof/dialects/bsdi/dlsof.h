/*
 * dlsof.h - BSDI header file for lsof
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
 * $Id: dlsof.h,v 1.15 2001/02/13 13:49:17 abe Exp $
 */


#if	!defined(BSDI_LSOF_H)
#define	BSDI_LSOF_H	1

#include <stdlib.h>
#include <dirent.h>
#include <nlist.h>
#include <setjmp.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

# if	BSDIV>=40100
#include <arpa/inet.h>
# endif	/* BSDIV>=40100 */

#include <sys/filedesc.h>
#include <sys/mbuf.h>
#define	NFS
#define m_stat	mnt_stat
#include <sys/mount.h>
#include <rpc/types.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/un.h>
#include <sys/unpcb.h>
#include <netinet/in.h>

# if	defined(HASIPv6) && !defined(IN6_ARE_ADDR_EQUAL) && defined(IN6_IS_ADDR_EQUAL)
#define	IN6_ARE_ADDR_EQUAL	IN6_IS_ADDR_EQUAL
# endif	/* defined(HASIPv6) && !defined(IN6_ARE_ADDR_EQUAL) && defined(IN6_IS_ADDR_EQUAL) */

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
#include <sys/vnode.h>
#include <net/raw_cb.h>
#include <sys/domain.h>
#include <isofs/cd9660/cd9660_node.h>
#define	pmap	RPC_pmap
#include <rpc/rpc.h>
#include <rpc/pmap_prot.h>
#undef	pmap
#undef IN_ACCESS
#undef IN_LOCKED
#undef IN_WANTED
#undef VTOI
#define KERNEL
#include <ufs/ufs/quota.h>
# if	defined(DIRBLKSIZ)
#define	DIRENT_DIRBLKSIZ	DIRBLKSIZ
#undef	DIRBLKSIZ
# endif	/* defined(DIRBLKSIZ) */

#include <ufs/ufs/inode.h>
#undef	i_rdev
#undef	i_size

# if	defined(DIRENT_BLKSIZ)
#define	DIRBLKSIZ	DIRENT_DIRBLKSIZ
#undef	DIRENT_DIRBLKSIZ
# endif	/*defined(DIRENT_BLKSIZ) */

#undef KERNEL
#include <msdosfs/direntry.h>
#include <msdosfs/fat.h>
#include <msdosfs/denode.h>
#include <ufs/mfs/mfsnode.h>

# if	BSDIV>=30000
#include <nfs/nfsproto.h>
#include <nfs/rpcv2.h>
# else	/* BSDIV<30000 */
#include <nfs/nfsv2.h>
# endif	/* BSDIV>=30000 */

#include <nfs/nfs.h>
#include <nfs/nfsnode.h>
#include <sys/proc.h>
#include <kvm.h>
#undef	TRUE
#undef	FALSE
#include <sys/sysctl.h>
#define	P_ADDR		kp_eproc.e_paddr
#define	P_COMM		kp_proc.p_comm
#define	P_FD		kp_proc.p_fd
#define	P_PID		kp_proc.p_pid
#define	P_PGID		kp_eproc.e_pgid
#define	P_PPID		kp_eproc.e_ppid
#define	P_STAT		kp_proc.p_stat
#define	P_TEXTVP	kp_proc.p_textvp
#define	P_VMSPACE	kp_proc.p_vmspace

# if	defined(HASFDESCFS)
#define	KERNEL
#include <miscfs/fdesc/fdesc.h>
#undef	KERNEL
# endif	/* defined(HASFDESCFS) */

# if	defined(HASPROCFS)
#include <miscfs/procfs/procfs.h>
#include <machine/reg.h>
# endif	defined(HASPROCFS)

#define	KERNEL
#define _KERNEL
#include <sys/file.h>
#include <sys/fcntl.h>
#include <ufs/ufs/lockf.h>
#undef	KERNEL
#undef	_KERNEL
#include <vm/vm.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_pager.h>

#define	COMP_P		const void
#define DEVINCR		1024	/* device table malloc() increment */
typedef	u_long		KA_T;
#define	KMEM		"/dev/kmem"
#define MALLOC_P	void
#define FREE_P		MALLOC_P
#define MALLOC_S	size_t
#define N_UNIX		"/bsd"
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

KA_T Cfp;
extern kvm_t *Kd;
extern KA_T Kpa;

struct l_vfs {
	KA_T addr;			/* kernel address */
	fsid_t	fsid;			/* file system ID */

#if	BSDIV<30000
	short type;			/* type of file system */
#else	/* BSDIV>=30000 */
	char *typnm;			/* file system type name */
#endif	/* BSDIV<30000 */

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
struct kinfo_proc *P;			/* local process table copy */
extern int pgshift;			/* kernel's page shift */

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

#  if	BSDIV>=20100
#include <stddef.h>
#  endif	/* BSDIV>=20100 */

#include <sys/uio.h>
#include <sys/namei.h>

#define	NCACHE		namecache	/* kernel's structure name */
#define	NCACHE_NM	nc_name		/* name in NCACHE */
#define	NCACHE_NMLEN	nc_nlen		/* name length in NCACHE */
#define	NCACHE_NODEADDR	nc_vp		/* node address in NCACHE */
#define	NCACHE_PARADDR	nc_dvp		/* parent node address in NCACHE */

#  if	defined(HASNCVPID)
#define	NCACHE_NODEID	nc_vpid		/* node ID in NCACHE */
#define	NCACHE_PARID	nc_dvpid	/* parent node ID in NCACHE */
#  endif	/* defined(HASNCVPID) */

#  if	BSDIV<20000
#define	NCACHE_NXT	nc_lru.tqe_next	/* link in NCACHE */
#  else	/* BSDIV>=20000 */
#   if	BSDIV<30000
#define	NCACHE_NXT	nc_forw		/* link in NCACHE */
#   else	/* BSDIV>=30000 */
#define	NCACHE_NXT	nc_hash.le_next	/* link in NCACHE */
#   endif	/* BSDIV<30000 */
#  endif	/* BSDIV<20000 */
# endif  /* defined(HASNCACHE) */

#endif	/* BSDI_LSOF_H */
