/*
 * dnode.c - NetBSD and OpenBSD node functions for lsof
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

#ifndef lint
static char copyright[] =
"@(#) Copyright 1994 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: dnode.c,v 1.21 2001/02/13 14:16:14 abe Exp $";
#endif


#include "lsof.h"


#if	defined(HASFDESCFS) && HASFDESCFS==1
_PROTOTYPE(static int lkup_dev_tty,(dev_t *dr, unsigned long *ir));
#endif	/* defined(HASFDESCFS) && HASFDESCFS==1 */

#if	defined(HASPROCFS)
_PROTOTYPE(static void getmemsz,(pid_t pid));

# if	!defined(PGSHIFT)
#define	PGSHIFT	pgshift
# endif	/* !defined(PGSHIFT) */


/*
 * getmemsz() - get memory size of a /proc/<n>/mem entry
 */

static void
getmemsz(pid)
	pid_t pid;
{
	int n;
	struct kinfo_proc *p;
	struct vmspace vm;

	for (n = 0, p = P; n < Np; n++, p++) {
	    if (p->P_PID == pid) {
		if (!p->P_VMSPACE
		||  kread((KA_T)p->P_VMSPACE, (char *)&vm, sizeof(vm)))
		    return;
		Lf->sz = (SZOFFTYPE)ctob(vm.vm_tsize + vm.vm_dsize
						     + vm.vm_ssize);
		Lf->sz_def = 1;
		return;
	    }
	}
}
#undef	PGSHIFT
#endif	/* defined(HASPROCFS) */


#if	defined(HASFDESCFS) && HASFDESCFS==1
/*
 * lkup_dev_tty() - look up /dev/tty
 */

static int
lkup_dev_tty(dr, ir)
	dev_t *dr;			/* place to return device number */
	unsigned long *ir;		/* place to return inode number */
{
	int i;

	readdev(0);

# if	defined(HASDCACHE)

lkup_dev_tty_again:

# endif	/* defined(HASDCACHE) */

	for (i = 0; i < Ndev; i++) {
	    if (strcmp(Devtp[i].name, "/dev/tty") == 0) {

# if	defined(HASDCACHE)
		if (DCunsafe && !Devtp[i].v && !vfy_dev(&Devtp[i]))
		    goto lkup_dev_tty_again;
# endif	/* defined(HASDCACHE) */

		*dr = Devtp[i].rdev;
		*ir = (unsigned long)Devtp[i].inode;
		return(1);
	    }
	}

# if	defined(HASDCACHE)
	if (DCunsafe) {
	    (void) rereaddev();
	    goto lkup_dev_tty_again;
	}
# endif	/* defined(HASDCACHE) */

	return(-1);
}
#endif	/* defined(HASFDESCFS) && HASFDESCFS==1 */


/*
 * process_node() - process vnode
 */

void
process_node(va)
	KA_T va;			/* vnode kernel space address */
{
	dev_t dev, rdev;
	struct denode d;
	u_long dpb;
	unsigned char devs = 0;
	unsigned char lt;
	unsigned char ns;
	unsigned char rdevs = 0;
	char *ep, *ty;
	struct lockf lf, *lff, *lfp;
	struct inode i;
	struct mfsnode m;
	struct nfsnode n;
	u_long long nn;
	enum nodetype {NONODE, CDFSNODE, DOSNODE, EXT2NODE, FDESCNODE, INODE,
		KERNFSNODE, MFSNODE, NFSNODE, PFSNODE} nty = NONODE;
	struct msdosfsmount pm;
	enum vtype type;
	struct vnode *v, vb;
	struct l_vfs *vfs;

#if	defined(HAS9660FS)
	dev_t iso_dev;
	unsigned long iso_ino, iso_sz;
	long iso_nlink;
	int iso_stat = 0;
#endif	/* defined(HAS9660FS) */

#if	defined(HASFDESCFS)
	struct fdescnode f;

# if	HASFDESCFS==1
	static dev_t f_tty_dev;
	static unsigned long f_tty_ino;
	static int f_tty_s = 0;
# endif	/* HASFDESCFS==1 */

#endif	/* defined(HASFDESCFS) */

#if	defined(HASKERNFS)
	struct kernfs_node kn;
	struct stat ksb;
	int ksbs = 0;
	struct kern_target kt;
	int ktnl;
	char ktnm[MAXPATHLEN+1];
#endif	/* defined(HASKERNFS) */

#if	defined(HASNFSVATTRP)
	struct vattr nv;
#define	NVATTR	nv
#else	/* !defined(HASNFSVATTRP) */
#define	NVATTR	n.n_vattr
#endif	/* defined(HASNFSVATTRP) */

#if	defined(HASPROCFS)
	struct pfsnode p;
	struct procfsid *pfi;
	size_t sz;
#endif	/* defined(HASPROCFS) */

/*
 * Read the vnode.
 */
	if (!va) {
	    enter_nm("no vnode address");
	    return;
	}
	v = &vb;
	if (readvnode(va, v)) {
	    enter_nm(Namech);
	    return;
	}

#if	defined(HASNCACHE)
	Lf->na = va;
# if	defined(HASNCAPID)
	Lf->id = v->v_id;
# endif	/* defined(HASNCAPID) */
#endif	/* defined(HASNCACHE) */

#if	defined(HASFSTRUCT)
	Lf->fna = va;
	Lf->fsv |= FSV_NI;
#endif	/* defined(HASFSTRUCT) */

/*
 * Get the vnode type.
 */
	if (!v->v_mount)
	    vfs = (struct l_vfs *)NULL;
	else {
	    vfs = readvfs((KA_T)v->v_mount);
	    if (vfs) {
		if (strcmp(vfs->type, MOUNT_NFS) == 0)
		    Ntype = N_NFS;

#if	defined(HASKERNFS)
		else if (strcmp(vfs->type, MOUNT_KERNFS) == 0)
		    Ntype = N_KERN;
#endif	/* defined(HASKERNFS) */

#if	defined(HASPROCFS)
		else if (strcmp(vfs->type, MOUNT_PROCFS) == 0)
		    Ntype = N_PROC;
#endif	/* defined(HASPROCFS) */

#if	defined(HAS9660FS)
		else if (strcmp(vfs->type, MOUNT_CD9660) == 0)
		    Ntype = N_CDFS;
#endif	/* defined(HAS9660FS) */

	    }
	}
	if (Ntype == N_REGLR) {
	    switch (v->v_type) {
	    case VFIFO:
		Ntype = N_FIFO;
		break;
	    }
	}
/*
 * Read the successor node.
 */
	switch (v->v_tag) {

#if	defined(HAS9660FS)
	case VT_ISOFS:
	    if (read_iso_node(v, &iso_dev, (unsigned long *)&iso_ino,
		  &iso_nlink, (SZOFFTYPE *)&iso_sz))
	    {
		(void) snpf(Namech, Namechl, "can't read iso_node at: %s",
		    print_kptr((KA_T)v->v_data, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	    iso_stat = 1;
	    nty = CDFSNODE;
	    break;
#endif	/* defined(HAS9660FS) */

#if	defined(HASKERNFS)
	case VT_KERNFS:
	
	/*
	 * Read the kernfs_node.
	 */
	    if (!v->v_data
	    ||  kread((KA_T)v->v_data, (char *)&kn, sizeof(kn))) {
		if (v->v_type != VDIR || !(v->v_flag && VROOT)) {
		    (void) snpf(Namech, Namechl,
			"can't read kernfs_node at: %s",
			print_kptr((KA_T)v->v_data, (char *)NULL, 0));
		    enter_nm(Namech);
		    return;
		} else
		    kn.kf_kt = (struct kern_target *)NULL;
	    }
	/*
	 * Generate the /kern file name by reading the kern_target to which
	 * the kernfs_node points.
	 */
	    if (kn.kf_kt
	    &&  kread((KA_T)kn.kf_kt, (char *)&kt, sizeof(kt)) == 0
	    &&  (ktnl = (int)kt.kt_namlen) > 0
	    &&  kt.kt_name)
	    {
		if (ktnl > (sizeof(ktnm) - 1))
		    ktnl = sizeof(ktnm) - 1;
		if (!kread((KA_T)kt.kt_name, ktnm, ktnl)) {
		    ktnm[ktnl] = 0;
		    ktnl = strlen(ktnm);
		    if (ktnl > (MAXPATHLEN - strlen(_PATH_KERNFS) - 2)) {
			ktnl = MAXPATHLEN - strlen(_PATH_KERNFS) - 2;
			ktnm[ktnl] = '\0';
		    }
		    (void) snpf(Namech, Namechl, "%s/%s", _PATH_KERNFS, ktnm);
		}
	    }
	/*
	 * If this is the /kern root directory, its name, inode number and
	 * size are fixed; otherwise, safely stat() the file to get the
	 * inode number and size.
	 */
	    if (v->v_type == VDIR && (v->v_flag & VROOT)) {
		(void) snpf(Namech, Namechl, "%s", _PATH_KERNFS);
		ksb.st_ino = 2;
		ksb.st_size = DEV_BSIZE;
		ksbs = 1;
	    } else if (Namech[0] && statsafely(Namech, &ksb) == 0)
		ksbs = 1;
	    nty = KERNFSNODE;
	    break;
#endif	/* defined(HASKERNFS) */

	case VT_MFS:
	    if (!v->v_data
	    ||  kread((KA_T)v->v_data, (char *)&m, sizeof(m))) {
		(void) snpf(Namech, Namechl, "can't read mfsnode at: %s",
		    print_kptr((KA_T)v->v_data, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	    nty = MFSNODE;
	    break;
	case VT_MSDOSFS:
	    if (!v->v_data
	    ||  kread((KA_T)v->v_data, (char *)&d, sizeof(d))) {
		(void) snpf(Namech, Namechl, "can't read denode at: %s",
		    print_kptr((KA_T)v->v_data, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	    nty = DOSNODE;
	    break;
	case VT_NFS:
	    if (!v->v_data
	    ||  kread((KA_T)v->v_data, (char *)&n, sizeof(n))) {
		(void) snpf(Namech, Namechl, "can't read nfsnode at: %s",
		    print_kptr((KA_T)v->v_data, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }

#if	defined(HASNFSVATTRP)
	    if (!n.n_vattr
	    ||  kread((KA_T)n.n_vattr, (char *)&nv, sizeof(nv))) {
		(void) snpf(Namech, Namechl, "can't read n_vattr at: %x",
		    print_kptr((KA_T)n.n_vattr, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
#endif	/* defined(HASNFSVATTRP) */

	    nty = NFSNODE;
	    break;

#if	defined(HASFDESCFS)
	case VT_FDESC:
	    if (!v->v_data
	    ||  kread((KA_T)v->v_data, (char *)&f, sizeof(f))) {
		(void) snpf(Namech, Namechl, "can't read fdescnode at: %x",
		    print_kptr((KA_T)v->v_data, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	    nty = FDESCNODE;
	    break;
#endif	/* defined(HASFDESCFS) */

#if	defined(HASPROCFS)
	case VT_PROCFS:
	    if (!v->v_data
	    ||  kread((KA_T)v->v_data, (char *)&p, sizeof(p))) {
		(void) snpf(Namech, Namechl, "can't read pfsnode at: %s",
		    print_kptr((KA_T)v->v_data, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	    nty = PFSNODE;
	    break;
#endif	/* defined(HASPROCFS) */

#if	defined(HASEXT2FS)
	case VT_EXT2FS:
#endif	/* defined(HASEXT2FS) */

	case VT_UFS:
	    if (!v->v_data
	    ||  kread((KA_T)v->v_data, (char *)&i, sizeof(i))) {
		(void) snpf(Namech, Namechl, "can't read inode at: %s",
		    print_kptr((KA_T)v->v_data, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }

#if	defined(HASEXT2FS)
	    if (v->v_tag == VT_EXT2FS)
		nty = EXT2NODE;
	    else
#endif	/* defined(HASEXT2FS) */

		nty = INODE;
	    if ((lff = i.i_lockf)) {

	    /*
	     * Determine the lock state.
	     */
		lfp = lff;
		do {
		    if (kread((KA_T)lfp, (char *)&lf, sizeof(lf)))
			break;
		    lt = 0;
		    switch(lf.lf_flags & (F_FLOCK|F_POSIX)) {
		    case F_FLOCK:
			if (Cfp && (struct file *)lf.lf_id == Cfp)
			    lt = 1;
			break;
		    case F_POSIX:
			if ((KA_T)lf.lf_id == Kpa)
			    lt = 1;
			break;
		    }
		    if (!lt)
			continue;
		    if (lf.lf_start == (off_t)0
		    &&  lf.lf_end == 0xffffffffffffffffLL)
			lt = 1;
		    else
			lt = 0;
		    if (lf.lf_type == F_RDLCK)
			Lf->lock = lt ? 'R' : 'r';
		    else if (lf.lf_type == F_WRLCK)
			Lf->lock = lt ? 'W' : 'w';
		    else if (lf.lf_type == (F_RDLCK | F_WRLCK))
			Lf->lock = 'u';
		    break;
		} while ((lfp = lf.lf_next) && lfp != lff);
	    }
	    break;
	default:
	    if (v->v_type == VBAD || v->v_type == VNON)
		break;
	    (void) snpf(Namech, Namechl, "unknown file system type: %d",
		v->v_tag);
	    enter_nm(Namech);
	    return;
	}
/*
 * Get device and type for printing.
 */
	type = v->v_type;
	switch (nty) {

	case DOSNODE:
	    dev = d.de_dev;
	    devs = 1;
	    break;

#if	defined(HASFDESCFS)
	case FDESCNODE:

# if	defined(HASFDLINK)
	    if (f.fd_link
	    &&  !kread((KA_T)f.fd_link, Namech, Namechl - 1)) {
		Namech[Namechl - 1] = '\0';
		break;
	    }
# endif	/* defined(HASFDLINK) */

# if	HASFDESCFS==1
	    if (f.fd_type == Fctty) {
		if (f_tty_s == 0)
		    f_tty_s = lkup_dev_tty(&f_tty_dev, &f_tty_ino);
		if (f_tty_s == 1) {
		    dev = DevDev;
		    rdev = f_tty_dev;
		    Lf->inode = f_tty_ino;
		    devs = Lf->inp_ty = rdevs = 1;
		}
	    }
	    break;
# endif	/* HASFDESCFS==1 */
#endif	/* defined(HASFDESCFS) */

#if	defined(HASEXT2FS)
	case EXT2NODE:

# if	defined(HASI_FFS)
	    dev = i.i_dev;
	    devs = 1;
# else	/* !defined(HASI_FFS) */
	    dev = i.i_dev;
	    devs = 1;
	    if ((type == VCHR) || (type == VBLK)) {
		rdev = i.i_rdev;
		rdevs = 1;
	    }
# endif	/* defined(HASI_FFS) */

	    break;
#endif	/* defined(HASEXT2FS) */

	case INODE:
	    dev = i.i_dev;
	    devs = 1;
	    if ((type == VCHR) || (type == VBLK)) {

#if	defined(HASI_FFS)
		rdev = i.i_ffs_rdev;
#else	/* !defined(HASI_FFS) */
		rdev = i.i_rdev;
#endif	/* defined(HASI_FFS) */

		rdevs = 1;
	    }
	    break;

#if	defined(HASKERNFS)
	case KERNFSNODE:
	    if (vfs) {
		dev = (dev_t)vfs->fsid.val[0];
		devs = 1;
	    }
	    break;
#endif	/* defined(HASKERNFS) */


#if	defined(HAS9660FS)
	case CDFSNODE:
	    if (iso_stat) {
		dev = iso_dev;
		devs = 1;
	    }
	    break;
#endif	/* defined(HAS9660FS) */

	case NFSNODE:
	    dev = NVATTR.va_fsid;
	    devs = 1;
	}
/*
 * Obtain the inode number.
 */
	switch (nty) {
	case DOSNODE:
	    if (d.de_pmp && !kread((KA_T)d.de_pmp, (char *)&pm, sizeof(pm))) {
		dpb = (u_long)(pm.pm_BytesPerSec / sizeof(struct direntry));
		if (d.de_Attributes & ATTR_DIRECTORY) {
		    if (d.de_StartCluster == MSDOSFSROOT)
			nn = (u_long)1;
		    else
			nn = (u_long)(cntobn(&pm, d.de_StartCluster) * dpb);
		} else {
		    if (d.de_dirclust == MSDOSFSROOT)
			nn = (u_long)(roottobn(&pm, 0) * dpb);
		    else
			nn = (u_long)(cntobn(&pm, d.de_dirclust) * dpb);
		    nn += (u_long)(d.de_diroffset / sizeof(struct direntry));
		}
		Lf->inode = (unsigned long)nn;
		Lf->inp_ty = 1;
	    }
	    break;

#if	defined(HASEXT2FS)
	case EXT2NODE:
#endif	/* defined(HASEXT2FS) */

	case INODE:
	    if (type != VBLK) {
		Lf->inode = (unsigned long)i.i_number;
		Lf->inp_ty = 1;
	    }
	    break;

#if	defined(HASKERNFS)
	case KERNFSNODE:
	    if (ksbs) {
		Lf->inode = (unsigned long)ksb.st_ino;
		Lf->inp_ty = 1;
	    }
	    break;
#endif	/* defined(HASKERNFS) */

#if	defined(HAS9660FS)
	case CDFSNODE:
	    if (iso_stat) {
		Lf->inode = iso_ino;
		Lf->inp_ty = 1;
	    }
	    break;
#endif	/* defined(HAS9660FS) */

	case NFSNODE:
	    Lf->inode = (unsigned long)NVATTR.va_fileid;
	    Lf->inp_ty = 1;
	    break;

#if	defined(HASPROCFS)
	case PFSNODE:
	    Lf->inode = (unsigned long)p.pfs_fileno;
	    Lf->inp_ty = 1;
	    break;
#endif	/* defined(HASPROCFS) */

	}

/*
 * Obtain the file size.
 */
	if (Foffset)
	    Lf->off_def = 1;
	else {
	    switch (Ntype) {

#if	defined(HAS9660FS)
	    case N_CDFS:
		if (iso_stat) {
		    Lf->sz = (SZOFFTYPE)iso_sz;
		    Lf->sz_def = 1;
		}
		break;
#endif	/* defined(HAS9660FS) */

	    case N_FIFO:
		if (!Fsize)
		    Lf->off_def = 1;
		break;

#if	defined(HASKERNFS)
	    case N_KERN:
		if (ksbs) {
		    Lf->sz = (SZOFFTYPE)ksb.st_size;
		    Lf->sz_def = 1;
		}
		break;
#endif	/* defined(HASKERNFS) */

	    case N_NFS:
		if (nty == NFSNODE) {
		    Lf->sz = (SZOFFTYPE)NVATTR.va_size;
		    Lf->sz_def = 1;
		}
		break;

#if	defined(HASPROCFS)
	    case N_PROC:
		if (nty == PFSNODE) {
		    switch (p.pfs_type) {
		    case Proot:
		    case Pproc:
			Lf->sz = (SZOFFTYPE)DEV_BSIZE;
			Lf->sz_def = 1;
			break;
		    case Pcurproc:
			Lf->sz = (SZOFFTYPE)DEV_BSIZE;
			Lf->sz_def = 1;
			break;
		    case Pmem:
			(void) getmemsz(p.pfs_pid);
			break;
		    case Pregs:
			Lf->sz = (SZOFFTYPE)sizeof(struct reg);
			Lf->sz_def = 1;
			break;

# if	defined(FP_QSIZE)
		    case Pfpregs:
			Lf->sz = (SZOFFTYPE)sizeof(struct fpreg);
			Lf->sz_def = 1;
			break;
# endif	/* defined(FP_QSIZE) */

		    }
		}
		break;
#endif	/* defined(HASPROCFS) */

	    case N_REGLR:
		if (type == VREG || type == VDIR) {
		    if (nty == INODE) {

#if	defined(HASI_FFS)
			Lf->sz = (SZOFFTYPE)i.i_ffs_size;
#else	/* !defined(HASI_FFS) */
			Lf->sz = (SZOFFTYPE)i.i_size;
#endif	/* defined(HASI_FFS) */

			Lf->sz_def = 1;
		    } else if (nty == DOSNODE) {
			Lf->sz = (SZOFFTYPE)d.de_FileSize;
			Lf->sz_def = 1;
		    } else if (nty == MFSNODE) {
			Lf->sz = (SZOFFTYPE)m.mfs_size;
			Lf->sz_def = 1;
		    }

#if	defined(HASEXT2FS)
		    else if (nty == EXT2NODE) {

# if	defined(HASI_E2FS)
			Lf->sz = (SZOFFTYPE)i.i_e2fs_size;
# else	/* !defined(HASI_E2FS) */
			Lf->sz = (SZOFFTYPE)i.i_size;
# endif	/* defined(HASI_E2FS) */

			Lf->sz_def = 1;
		    }
#endif	/* defined(HASEXT2FS) */

		}
		else if ((type == VCHR || type == VBLK) && !Fsize)
		    Lf->off_def = 1;
		break;
	    }
	}
/*
 * Record the link count.
 */
	if (Fnlink) {
	    switch(Ntype) {

#if	defined(HAS9660FS)
	    case N_CDFS:
		if (iso_stat) {
		    Lf->nlink = iso_nlink;
		    Lf->nlink_def = 1;
		}
		break;
#endif	/* defined(HAS9660FS) */

#if	defined(HASKERNFS)
	    case N_KERN:
		if (ksbs) {
		    Lf->nlink = (long)ksb.st_nlink;
		    Lf->nlink_def = 1;
		}
		break;
#endif	/* defined(HASKERNFS) */

	    case N_NFS:
		if (nty == NFSNODE) {
		    Lf->nlink = (long)NVATTR.va_nlink;
		    Lf->nlink_def = 1;
		}
		break;
	    case N_REGLR:
		if (nty == INODE) {

# if	defined(HASI_FFS)
		    Lf->nlink = (long)i.i_ffs_nlink;
# else	/* !defined(HASI_FFS) */
		    Lf->nlink = (long)i.i_nlink;
# endif	/* defined(HASI_FFS) */

		    Lf->nlink_def = 1;
		} else if (nty == DOSNODE) {
		    Lf->nlink = (long)d.de_refcnt;
		    Lf->nlink_def = 1;
		}

#if	defined(HASEXT2FS)
		else if (nty == EXT2NODE) {
		    Lf->nlink - (long)i.i_e2fs_nlink;
		    Lf->nlink_def = 1;
		}
#endif	/* defined(HASEXT2FS) */

		break;
	    }
	    if (Lf->nlink_def && Nlink && (Lf->nlink < Nlink))
		Lf->sf |= SELNLINK;
	}
/*
 * Record an NFS file selection.
 */
	if (Ntype == N_NFS && Fnfs)
	    Lf->sf |= SELNFS;
/*
 * Save the file system names.
 */
	if (vfs) {
	    Lf->fsdir = vfs->dir;
	    Lf->fsdev = vfs->fsname;
	}
/*
 * Save the deice numbers and their states.
 *
 * Format the vnode type, and possibly the device name.
 */
	Lf->dev = dev;
	Lf->dev_def = devs;
	Lf->rdev = rdev;
	Lf->rdev_def = rdevs;
	switch (type) {
	case VNON:
	    ty ="VNON";
	    break;
	case VREG:
	    ty = "VREG";
	    break;
	case VDIR:
	    ty = "VDIR";
	    break;
	case VBLK:
	    ty = "VBLK";
	    Ntype = N_BLK;
	    break;
	case VCHR:
	    ty = "VCHR";
	    Ntype = N_CHR;
	    break;
	case VLNK:
	    ty = "VLNK";
	    break;

#if	defined(VSOCK)
	case VSOCK:
	    ty = "SOCK";
	    break;
#endif	/* defined(VSOCK) */

	case VBAD:
	    ty = "VBAD";
	    break;
	case VFIFO:
	    ty = "FIFO";
	    break;
	default:
	    if (type > 9999)
		(void) snpf(Lf->type, sizeof(Lf->type), "*%03d", type % 1000);
	    else
		(void) snpf(Lf->type, sizeof(Lf->type), "%4d", type);
	    (void) snpf(Namech, Namechl, "unknown type");
	    ty = NULL;
	}
	if (ty)
	    (void) snpf(Lf->type, sizeof(Lf->type), "%s", ty);
	Lf->ntype = Ntype;
/*
 * Handle some special cases:
 *
 * 	ioctl(fd, TIOCNOTTY) files;
 *	/kern files
 *	memory node files;
 *	/proc files.
 */

	if (type == VBAD)
	    (void) snpf(Namech, Namechl, "(revoked)");
	else if (nty == MFSNODE) {
	    Lf->dev_def = Lf->rdev_def = 0;
	    (void) snpf(Namech, Namechl, "%#x", m.mfs_baseoff);
	    enter_dev_ch("memory");
	}

#if	defined(HASPROCFS)
	else if (nty == PFSNODE) {
	    Lf->dev_def= Lf->rdev_def = 0;
	    ty = NULL;
	    (void) snpf(Namech, Namechl, "/%s", HASPROCFS);
	    switch (p.pfs_type) {
	    case Proot:
		ty = "PDIR";
		break;
	    case Pcurproc:
		ep = endnm(&sz);
		(void) snpf(ep, sz, "/curproc");
		ty = "PCUR";
		break;
	    case Pproc:
		ep = endnm(&sz);
		(void) snpf(ep, sz, "/%d", p.pfs_pid);
		ty = "PDIR";
		break;
	    case Pfile:
		ep = endnm(&sz);
		(void) snpf(ep, sz, "/%d/file", p.pfs_pid);
		ty = "PFIL";
		break;
	    case Pmem:
		ep = endnm(&sz);
		(void) snpf(ep, sz, "/%d/mem", p.pfs_pid);
		ty = "PMEM";
		break;
	    case Pregs:
		ep = endnm(&sz);
		(void) snpf(ep, sz, "/%d/regs", p.pfs_pid);
		ty = "PREG";
		break;
	    case Pfpregs:
		ep = endnm(&sz);
		(void) snpf(ep, sz, "/%d/fpregs", p.pfs_pid);
		ty = "PFPR";
		break;
	    case Pctl:
		ep = endnm(&sz);
		(void) snpf(ep, sz, "/%d/ctl", p.pfs_pid);
		ty = "PCTL";
		break;
	    case Pstatus:
		ep = endnm(&sz);
		(void) snpf(ep, sz, "/%d/status", p.pfs_pid);
		ty = "PSTA";
		break;
	    case Pnote:
		ep = endnm(&sz);
		(void) snpf(ep, sz, "/%d/note", p.pfs_pid);
		ty = "PNTF";
		break;
	    case Pnotepg:
		ep = endnm(&sz);
		(void) snpf(ep, sz, "/%d/notepg", p.pfs_pid);
		ty = "PGID";
		break;
	    }
	    if (ty)
		(void) snpf(Lf->type, sizeof(Lf->type), ty);
	    if (Namech[0])
		enter_nm(Namech);
	}
#endif	/* defined(HASPROCFS) */

#if	defined(HASBLKDEV)
/*
 * If this is a VBLK file and it's missing an inode number, try to
 * supply one.
 */
	if ((Lf->inp_ty == 0) && (type == VBLK))
	    find_bl_ino();
#endif	/* defined(HASBLKDEV) */

/*
 * If this is a VCHR file and it's missing an inode number, try to
 * supply one.
 */
	if ((Lf->inp_ty == 0) && (type == VCHR))
	    find_ch_ino();
/*
 * Test for specified file.
 */

#if	defined(HASPROCFS)
	if (Ntype == N_PROC) {
	    if (Procsrch) {
		Procfind = 1;
		Lf->sf |= SELNM;
	    } else if (nty == PFSNODE) {
		for (pfi = Procfsid; pfi; pfi = pfi->next) {
		    if ((pfi->pid && pfi->pid == p.pfs_pid)
		    
# if	defined(HASPINODEN)
		    ||  (Lf->inp_ty == 1 && pfi->inode == Lf->inode)
# endif	/* defined(HASPINODEN) */

		    ) {
			pfi->f = 1;
			if (Namech[0] && pfi->nm)
			    (void) snpf(Namech, Namechl, "%s", pfi->nm);
			Lf->sf |= SELNM;
			break;
		    }
		}
	    }
	} else
#endif	/* defined(HASPROCFS) */

	{
	    if (Namech[0]) {
		enter_nm(Namech);
		ns = 1;
	    } else
		ns = 0;
	    if (Sfile && is_file_named((char *)NULL,
				       ((type == VCHR) || (type == VBLK)) ? 1
									  : 0))
		Lf->sf |= SELNM;
	    if (ns)
		Namech[0] = '\0';
	}
/*
 * Enter name characters.
 */
	if (Namech[0])
	    enter_nm(Namech);
}


#if	defined(OPENBSDV)
/*
 * process_pipe() - process a file structure whose type is DTYPE_PIPE
 */

void
process_pipe(pa)
	KA_T pa;			/* pipe structure kernel address */
{
	char *ep;
	struct pipe p;
	size_t sz;

	if (!pa || kread((KA_T)pa, (char *)&p, sizeof(p))) {
	    (void) snpf(Namech, Namechl,
		"can't read DTYPE_PIPE pipe struct: %#s",
		print_kptr(pa, (char *)NULL, 0));
	    enter_nm(Namech);
	    return;
	}
	(void) snpf(Lf->type, sizeof(Lf->type), "PIPE");
	enter_dev_ch(print_kptr(pa, (char *)NULL, 0));
	if (Foffset)
	    Lf->off_def = 1;
	else {
	    Lf->sz = (SZOFFTYPE)p.pipe_buffer.size;
	    Lf->sz_def = 1;
	}
	if (p.pipe_peer)
	    (void) snpf(Namech, Namechl, "->%s",
		print_kptr((KA_T)p.pipe_peer, (char *)NULL, 0));
	else
	    Namech[0] = '\0';
	if (p.pipe_buffer.cnt) {
	    ep = endnm(&sz);
	    (void) snpf(ep, sz, ", cnt=%d", p.pipe_buffer.cnt);
	}
	if (p.pipe_buffer.in) {
	    ep = endnm(&sz);
	    (void) snpf(ep, sz, ", in=%d", p.pipe_buffer.in);
	}
	if (p.pipe_buffer.out) {
	    ep = endnm(&sz);
	    (void) snpf(ep, sz, ", out=%d", p.pipe_buffer.out);
	}
/*
 * Enter name characters.
 */
	if (Namech[0])
	    enter_nm(Namech);
}
#endif	/* defined(OPENBSDV) */
