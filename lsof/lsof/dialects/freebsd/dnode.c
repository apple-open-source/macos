/*
 * dnode.c - FreeBSD node functions for lsof
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
static char *rcsid = "$Id: dnode.c,v 1.21 2001/06/08 15:47:42 abe Exp $";
#endif


#include "lsof.h"


#if	defined(HASFDESCFS) && HASFDESCFS==1
_PROTOTYPE(static int lkup_dev_tty,(dev_t *dr, unsigned long *ir));
#endif	/* defined(HASFDESCFS) && HASFDESCFS==1 */


#if	FREEBSDV>=200
# if	defined(HASPROCFS)
_PROTOTYPE(static void getmemsz,(pid_t pid));


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
# endif	/* defined(HASPROCFS) */
#endif	/* FREEBSDV>=200 */


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
	unsigned char devs = 0;
	unsigned char lt;
	unsigned char rdevs = 0;
	char dev_ch[32], *ep;
	struct inode *i = (struct inode *)NULL;
	struct lockf lf, *lff, *lfp;
	struct nfsnode *n = (struct nfsnode *)NULL;
	char *ty;
	enum vtype type;
	struct vnode *v, vb;
	struct l_vfs *vfs;

#if	FREEBSDV>=200
	struct inode ib;
	struct nfsnode nb;
	size_t sz;
# if	FREEBSDV>=400
	struct specinfo si;
# endif	/* FREEBSDV>=400 */
#endif	/* FREEBSDV>=200 */

#if	FREEBSDV<500
	struct mfsnode *m = (struct mfsnode *)NULL;
# if	FREEBSDV>=200
	struct mfsnode mb;
# endif	/* FREEBSDV>=200 */
#endif	/* FREEBSDV<500 */

#if	defined(HAS9660FS)
	dev_t iso_dev;
	int iso_dev_def = 0;
	unsigned long iso_ino, iso_sz;
	long iso_links;
	int iso_stat = 0;
#endif	/* defined(HAS9660FS) */

#if	defined(HASFDESCFS)
	struct fdescnode *f = (struct fdescnode *)NULL;

# if	HASFDESCFS==1
	static dev_t f_tty_dev;
	static unsigned long f_tty_ino;
	static int f_tty_s = 0;
# endif	/* HASFDESCFS==1 */

# if	FREEBSDV>=200
	struct fdescnode fb;
# endif	/* FREEBSDV>=200 */

#endif	/* defined(HASFDESCFS) */

#if	FREEBSDV>=500
	struct devfs_dirent de;
	struct devfs_dirent *d = (struct devfs_dirent *)NULL;
#endif	/* FREEBSDV>=500 */

#if	defined(HASPROCFS)
	struct pfsnode *p = (struct pfsnode *)NULL;
	struct procfsid *pfi;
	static int pgsz = -1;
	struct vmspace vm;

# if	FREEBSDV>=200
	struct pfsnode pb;
# endif	/* FREEBSDV>=200 */
#endif	/* defined(HASPROCFS) */

/*
 * Read the vnode.
 */
	if ( ! va) {
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

#if	defined(MOUNT_NONE)
		switch (vfs->type) {
		case MOUNT_NFS:
		    Ntype = N_NFS;
		    break;

# if	defined(HASPROCFS)
		case MOUNT_PROCFS:
		    Ntype = N_PROC;
		    break;
		}
# endif	/* defined(HASPROCFS) */
#else	/* !defined(MOUNT_NONE) */
		if (strcasecmp(vfs->typnm, "nfs") == 0)
		    Ntype = N_NFS;

# if	defined(HASPROCFS)
		else if (strcasecmp(vfs->typnm, "procfs") == 0)
		    Ntype = N_PROC;
# endif	/* defined(HASPROCFS) */
#endif	/* defined(MOUNT_NONE) */

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
 * Define the specific node pointer.
 */
	switch (v->v_tag) {

#if	FREEBSDV>=500
	case VT_DEVFS:
	    if (!v->v_data
	    ||  kread((KA_T)v->v_data, (char *)&de, sizeof(de)))
	    {
		(void) snpf(Namech, Namechl, "no devfs node: %s",
		    print_kptr((KA_T)v->v_data, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	    d = &de;
	    if (v->v_type == VDIR) {
		if (!d->de_dir
		||  kread((KA_T)d->de_dir, (char *)&de, sizeof(de))) {
		    (void) snpf(Namech, Namechl, "no devfs dir node: %s",
			print_kptr((KA_T)d->de_dir, (char *)NULL, 0));
		    enter_nm(Namech);
		    return;
		}
	    }
	    break;
#endif	/* FREEBSDV>=500 */

#if	defined(HAS9660FS)
	case VT_ISOFS:
	    if (read_iso_node(v, &iso_dev, &iso_dev_def, &iso_ino, &iso_links,
			      &iso_sz))
	    {
		(void) snpf(Namech, Namechl, "no iso node: %s",
		    print_kptr((KA_T)v->v_data, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	    iso_stat = 1;
	    break;
#endif	/* defined(HAS9660FS) */

#if	FREEBSDV<500
	case VT_MFS:

# if	FREEBSDV<200
	    m = (struct mfsnode *)v->v_data;
# else	/* FREEBSDV>=200 */
	    if (!v->v_data
	    ||  kread((KA_T)v->v_data, (char *)&mb, sizeof(mb))) {
		(void) snpf(Namech, Namechl, "no mfs node: %s",
		    print_kptr((KA_T)v->v_data, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	    m = &mb;
# endif	/* FREEBSDV<200 */
#endif	/* FREEBSDV<500 */

	    break;
	case VT_NFS:

#if	FREEBSDV<200
	    n = (struct nfsnode *)v->v_data;
#else	/* FREEBSDV>=200 */
	    if (!v->v_data
	    ||  kread((KA_T)v->v_data, (char *)&nb, sizeof(nb))) {
		(void) snpf(Namech, Namechl, "no nfs node: %s",
		    print_kptr((KA_T)v->v_data, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	    n = &nb;
#endif	/* FREEBSDV<200 */

	    break;

#if	defined(HASFDESCFS)
	case VT_FDESC:

# if	FREEBSDV<200
	    f = (struct fdescnode *)v->v_data;
# else	/* FREEBSDV>=200 */
	    if (kread((KA_T)v->v_data, (char *)&fb, sizeof(fb)) != 0) {
		(void) snpf(Namech, Namechl, "can't read fdescnode at: %s",
		    print_kptr((KA_T)v->v_data, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	    f = &fb;
	    break;
# endif	/* FREEBSDV<200 */
#endif	/* defined(HASFDESCFS) */

#if	defined(HASPROCFS)
	case VT_PROCFS:

# if	FREEBSDV<200
	    p = (struct pfsnode *)v->v_data;
# else	/* FREEBSDV>=200 */
	    if (!v->v_data
	    ||  kread((KA_T)v->v_data, (char *)&pb, sizeof(pb))) {
		(void) snpf(Namech, Namechl, "no pfs node: %s",
		    print_kptr((KA_T)v->v_data, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	    p = &pb;
# endif	/* FREEBSDV<200 */

	    break;
#endif	/* defined(HASPROCFS) */

	case VT_UFS:

#if	FREEBSDV<200
	    i = (struct inode *)v->v_data;
#else	/* FREEBSDV>=200 */
	    if (!v->v_data
	    ||  kread((KA_T)v->v_data, (char *)&ib, sizeof(ib))) {
		(void) snpf(Namech, Namechl, "no ufs node: %s",
		    print_kptr((KA_T)v->v_data, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	    i = &ib;
#endif	/* FREEBSDV<200 */

	    if ((lff = i->i_lockf)) {

	    /*
	     * Determine the lock state.
	     */
		lfp = lff;
		do {
		    if (kread((KA_T)lfp, (char *)&lf, sizeof(lf)))
			break;
		    lt = 0;
		    switch (lf.lf_flags & (F_FLOCK|F_POSIX)) {
		    case F_FLOCK:
			if (Cfp && (struct file *)lf.lf_id == Cfp)
			    lt = 1;
			break;
		    case F_POSIX:

#if	defined(P_ADDR)
			if ((KA_T)lf.lf_id == Kpa)
			    lt = 1;
#endif	/* defined(P_ADDR) */

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
	    (void) snpf(Namech,Namechl,"unknown file system type: %d",v->v_tag);
	    enter_nm(Namech);
	    return;
	}
/*
 * Get device and type for printing.
 */
	type = v->v_type;
	if (n) {
	    dev = n->n_vattr.va_fsid;
	    devs = 1;
	    if ((type == VCHR) || (type == VBLK)) {
		rdev = n->n_vattr.va_rdev;
		rdevs = 1;
	    }
	} else if (i) {

#if	FREEBSDV>=400
	    if (i->i_dev
	    &&  !kread((KA_T)i->i_dev, (char *)&si, sizeof(si))) {
		dev = si.si_udev;
		devs = 1;
	    }
#else	/* FREEBSDV<400 */
	    dev = i->i_dev;
	    devs = 1;
#endif	/* FREEBSDV>=400 */

	    if ((type == VCHR) || (type == VBLK)) {
		rdev = i->i_rdev ;
		rdevs = 1;
	    }
	}

#if	defined(HASFDESCFS) && (defined(HASFDLINK) || HASFDESCFS==1)
	else if (f) {

# if	defined(HASFDLINK)
	    if (f->fd_link
	    &&  kread((KA_T)f->fd_link, Namech, Namechl - 1) == 0)
		Namech[Namechl - 1] = '\0';

#  if	HASFDESCFS==1
	    else
#  endif	/* HASFDESFS==1 */
# endif	/* defined(HASFDLINK) */

# if	HASFDESCFS==1
		if (f->fd_type == Fctty) {
		    if (f_tty_s == 0)
			f_tty_s = lkup_dev_tty(&f_tty_dev, &f_tty_ino);
		    if (f_tty_s == 1) {
			dev = f_tty_dev;
			Lf->inode = f_tty_ino;
			devs = Lf->inp_ty = 1;
		    }
		}
# endif	/* HASFDESFS==1 */

	}
#endif	defined(HASFDESCFS) && (defined(HASFDLINK) || HASFDESCFS==1)

#if	defined(HAS9660FS)
	else if (iso_stat && iso_dev_def) {
	    dev = iso_dev;
	    devs = Lf->inp_ty = 1;
	}
#endif	/* defined(HAS9660FS) */

#if	FREEBSDV>=500
	else if (d) {
	    if (vfs) {
		dev = vfs->fsid.val[0];
		devs = 1;
	    } else {
		dev = DevDev;
		devs = 1;
	    }
	    if ((type == VCHR) && v->v_rdev) {
		if (!kread((KA_T)v->v_rdev, (char *)&si, sizeof(si))) {
		    rdev = si.si_udev;
		    rdevs = 1;
		}
	    }
	}
#endif	/* FREEBSDV>=500 */

/*
 * Obtain the inode number.
 */
	if (i) {
	    if (type != VBLK) {
		Lf->inode = (unsigned long)i->i_number;
		Lf->inp_ty = 1;
	    }
	} else if (n) {
	    Lf->inode = (unsigned long)n->n_vattr.va_fileid;
	    Lf->inp_ty = 1;
	}

#if	defined(HAS9660FS)
	else if (iso_stat) {
	    Lf->inode = iso_ino;
	    Lf->inp_ty = 1;
	}
#endif	/* defined(HAS9660FS) */

#if	defined(HASPROCFS)
# if	FREEBSDV>=200
	else if (p) {
	    Lf->inode = (unsigned long)p->pfs_fileno;
	    Lf->inp_ty = 1;
	}
# endif	/* FREEBSDV>=200 */
#endif	/* defined(HASPROCFS) */

#if	FREEBSDV>=500
	else if (d) {
	    Lf->inode = (unsigned long)d->de_inode;
	    Lf->inp_ty = 1;
	}
#endif	/* FREEBSDV>=500 */


/*
 * Obtain the file size.
 */
	if (Foffset)
	    Lf->off_def = 1;
	else {
	    switch (Ntype) {
	    case N_FIFO:
		if (!Fsize)
		    Lf->off_def = 1;
		break;
	    case N_NFS:
		if (n) {
		    Lf->sz = (SZOFFTYPE)n->n_vattr.va_size;
		    Lf->sz_def = 1;
		}
		break;

#if	defined(HASPROCFS)
	    case N_PROC:

# if	FREEBSDV<200
		if (type == VDIR || !p || !p->pfs_vs
		||  kread((KA_T)p->pfs_vs, (char *)&vm, sizeof(vm)))
		    break;
		if (pgsz < 0)
		    pgsz = getpagesize();
		Lf->sz = (SZOFFTYPE)((pgsz * vm.vm_tsize)
		       +         (pgsz * vm.vm_dsize)
		       +         (pgsz * vm.vm_ssize));
		Lf->sz_def = 1;
		break;
# else	/* FREEBSDV>=200 */
		if (p) {
		    switch(p->pfs_type) {
		    case Proot:
		    case Pproc:
			Lf->sz = (SZOFFTYPE)DEV_BSIZE;
			Lf->sz_def = 1;
			break;
		    case Pmem:
			(void) getmemsz(p->pfs_pid);
			break;
		    case Pregs:
			Lf->sz = (SZOFFTYPE)sizeof(struct reg);
			Lf->sz_def = 1;
			break;
		    case Pfpregs:
			Lf->sz = (SZOFFTYPE)sizeof(struct fpreg);
			Lf->sz_def = 1;
			break;
		    }
		}
# endif	/* FREEBSDV<200 */
#endif	/* defined(HASPROCFS) */

	    case N_REGLR:
		if (type == VREG || type == VDIR) {
		    if (i) {
			Lf->sz = (SZOFFTYPE)i->i_size;
			Lf->sz_def = 1;
		    }

#if	FREEBSDV<500
		    else if (m) {
			Lf->sz = (SZOFFTYPE)m->mfs_size;
			Lf->sz_def = 1;
		    }
#endif	/* FREEBSDV<500 */

#if	defined(HAS9660FS)
		    else if (iso_stat) {
			Lf->sz = (SZOFFTYPE)iso_sz;
			Lf->sz_def = 1;
		    }
#endif	/* defined(HAS9660FS) */

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
	    case N_NFS:
		if (n) {
		    Lf->nlink = (long)n->n_vattr.va_nlink;
		    Lf->nlink_def = 1;
		}
		break;
	    case N_REGLR:
		if (i) {
		    Lf->nlink = (long)i->i_nlink;
		    Lf->nlink_def = 1;
		}

#if	defined(HAS9660FS)
		else if (iso_stat) {
		    Lf->nlink = iso_links;
		    Lf->nlink_def = 1;
		}
#endif	/* defined(HAS9660FS) */

#if	FREEBSDV>=500
		else if (d) {
		    Lf->nlink = d->de_links;
		    Lf->nlink_def = 1;
		}
#endif	/* FREEBSDV>=500 */


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
 * Save the device numbers and their states.
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
	case VDIR:
	    ty = (type == VREG) ? "VREG" : "VDIR";
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
	    ty = (char *)NULL;
	}
	if (ty)
	    (void) snpf(Lf->type, sizeof(Lf->type), "%s", ty);
	Lf->ntype = Ntype;
/*
 * Handle some special cases:
 *
 * 	ioctl(fd, TIOCNOTTY) files;
 *	memory node files;
 *	/proc files.
 */

	if (type == VBAD)
	    (void) snpf(Namech, Namechl, "(revoked)");

#if	FREEBSDV<500
	else if (m) {
	    Lf->dev_def = Lf->rdev_def = 0;
	    (void) snpf(Namech, Namechl, "%#x", m->mfs_baseoff);
	    (void) snpf(dev_ch, sizeof(dev_ch), "    memory");
	    enter_dev_ch(dev_ch);
	}
#endif	/* FREEBSDV<500 */


#if	defined(HASPROCFS)
	else if (p) {
	    Lf->dev_def = Lf->rdev_def = 0;

# if	FREEBSDV<200
	    if (type == VDIR)
		(void) snpf(Namech, Namechl, "/%s", HASPROCFS);
	    else
		(void) snpf(Namech, Namechl, "/%s/%0*d", HASPROCFS, PNSIZ,
		    p->pfs_pid);
	    enter_nm(Namech);
# else	/* FREEBSDV>=200 */
	    ty = (char *)NULL;
	    (void) snpf(Namech, Namechl, "/%s", HASPROCFS);
	    switch (p->pfs_type) {
	    case Proot:
		ty = "PDIR";
		break;
	    case Pproc:
		ep = endnm(&sz);
		(void) snpf(ep, sz, "/%d", p->pfs_pid);
		ty = "PDIR";
		break;
	    case Pfile:
		ep = endnm(&sz);
		(void) snpf(ep, sz, "/%d/file", p->pfs_pid);
		ty = "PFIL";
		break;
	    case Pmem:
		ep = endnm(&sz);
		(void) snpf(ep, sz, "/%d/mem", p->pfs_pid);
		ty = "PMEM";
		break;
	    case Pregs:
		ep = endnm(&sz);
		(void) snpf(ep, sz, "/%d/regs", p->pfs_pid);
		ty = "PREG";
		break;
	    case Pfpregs:
		ep = endnm(&sz);
		(void) snpf(ep, sz, "/%d/fpregs", p->pfs_pid);
		ty = "PFPR";
		break;
	    case Pctl:
		ep = endnm(&sz);
		(void) snpf(ep, sz, "/%d/ctl", p->pfs_pid);
		ty = "PCTL";
		break;
	    case Pstatus:
		ep = endnm(&sz);
		(void) snpf(ep, sz, "/%d/status", p->pfs_pid);
		ty = "PSTA";
		break;
	    case Pnote:
		ep = endnm(&sz);
		(void) snpf(ep, sz, "/%d/note", p->pfs_pid);
		ty = "PNTF";
		break;
	    case Pnotepg:
		ep = endnm(&sz);
		(void) snpf(ep, sz, "/%d/notepg", p->pfs_pid);
		ty = "PGID";
		break;

#  if	FREEBSDV>=300
	    case Pmap:
		ep = endnm(&sz);
		(void) snpf(ep, sz, "/%d/map", p->pfs_pid);
		ty = "PMAP";
		break;
	    case Ptype:
		ep = endnm(&sz);
		(void) snpf(ep, sz, "/%d/etype", p->pfs_pid);
		ty = "PETY";
		break;
#  endif	/* FREEBSDV>=300 */

	    }
	    if (ty)
		(void) snpf(Lf->type, sizeof(Lf->type), "%s", ty);
	    enter_nm(Namech);

# endif	/* FREEBSDV<200 */
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
	    } else {
		for (pfi = Procfsid; pfi; pfi = pfi->next) {
		    if ((pfi->pid && pfi->pid == p->pfs_pid)

# if	defined(HASPINODEN)
		    ||  (Lf->inp_ty == 1 && Lf->inode == pfi->inode)
# else	/* !defined(HASPINODEN) */
				if (pfi->pid == p->pfs_pid)
# endif	/* defined(HASPINODEN) */

		    ) {
			pfi->f = 1;
			if (!Namech[0])
			    (void) snpf(Namech, Namechl, "%s", pfi->nm);
			Lf->sf |= SELNM;
			break;
		    }
		}
	    }
	} else
#endif	/* defined(HASPROCFS) */

	{
	    if (Sfile && is_file_named((char *)NULL,
				       ((type == VCHR) || (type == VBLK)) ? 1
									  : 0))
		Lf->sf |= SELNM;
	}
/*
 * Enter name characters.
 */
	if (Namech[0])
	    enter_nm(Namech);
}


#if	FREEBSDV>=220
/*
 * process_pipe() - process a file structure whose type is DTYPE_PIPE
 */

void
process_pipe(pa)
	KA_T pa;			/* pipe structure address */
{
	char dev_ch[32], *ep;
	struct pipe p;
	size_t sz;

	if (!pa || kread(pa, (char *)&p, sizeof(p))) {
	    (void) snpf(Namech, Namechl,
		"can't read DTYPE_PIPE pipe struct: %s",
		print_kptr((KA_T)pa, (char *)NULL, 0));
	    enter_nm(Namech);
	    return;
	}
	(void) snpf(Lf->type, sizeof(Lf->type), "PIPE");
	(void) snpf(dev_ch, sizeof(dev_ch), "%#x", pa);
	enter_dev_ch(dev_ch);
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
#endif	/* FREEBSDV>=220 */
