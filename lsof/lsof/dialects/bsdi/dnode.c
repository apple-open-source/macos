/*
 * dnode.c - BSDI node functions for lsof
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
static char *rcsid = "$Id: dnode.c,v 1.16 2001/02/13 13:49:44 abe Exp $";
#endif


#include "lsof.h"


#if	defined(HASFDESCFS) && HASFDESCFS==1
_PROTOTYPE(static int lkup_dev_tty,(dev_t *dr, unsigned long *ir));
#endif	/* defined(HASFDESCFS) && HASFDESCFS==1 */

_PROTOTYPE(static void process_lock,(KA_T lp));

#if	defined(HASPROCFS)
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
		Lf->sz = (SZOFFTYPE)ctob(vm.vm_tsize+vm.vm_dsize+vm.vm_ssize);
		Lf->sz_def = 1;
		return;
	    }
	}
}
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
 * process_lock() - process node lock information
 */

static void
process_lock(lp)
	KA_T lp;			/* kernel lockf structure */
{
	struct lockf lf;
	KA_T lfp;
	int lt;

	if (!lp)
	    return;
/*
 * Determine the lock state.
 */
	lfp = lp;
	do {
	    if (kread(lfp, (char *)&lf, sizeof(lf)))
		break;
	    lt = 0;
	    switch (lf.lf_flags & (F_FLOCK|F_POSIX)) {
	    case F_FLOCK:
		if (Cfp && (KA_T)lf.lf_id == Cfp)
		    lt = 1;
		break;
	    case F_POSIX:
		if ((KA_T)lf.lf_id == Kpa)
		    lt = 1;
		break;
	    }
	    if (!lt)
		continue;
	    if (lf.lf_start == (off_t)0 && lf.lf_end == 0xffffffffffffffffLL)
		lt = 1;
	    else
		lt = 0;
	    if (lf.lf_type == F_RDLCK)
		Lf->lock = lt ? 'R' : 'r';
	    else if (lf.lf_type == F_WRLCK)
		Lf->lock = lt ? 'W' : 'w';
	    else if (lf.lf_type == (F_RDLCK | F_WRLCK))
		Lf->lock = 'u';
	    return;
	} while ((lfp = (KA_T)lf.lf_next) && lfp != lp);
}


/*
 * process_node() - process vnode
 */

void
process_node(va)
	KA_T va;			/* vnode kernel space address */
{
	struct iso_node cd;
	u_int cn;
	dev_t dev, rdev;
	unsigned char devs = 0;
	unsigned char rdevs = 0;
	struct denode dn;
	char *ep, *ty;
	struct inode i;
	struct mfsnode m;
	struct nfsnode n;
	enum nodetype {NONODE, INODE, MFSNODE, NFSNODE, CDFSNODE, PCFSNODE,
		FDESCNODE, PFSNODE} nty = NONODE;
	size_t sz;
	enum vtype type;
	struct vnode *v, vb;
	struct l_vfs *vfs;

#if	defined(HASFDESCFS)
	struct fdescnode f;

# if	HASFDESCFS==1
	static dev_t f_tty_dev;
	static unsigned long f_tty_ino;
	static int f_tty_s = 0;
# endif	/* HASFDESCFS==1 */

#endif	/* defined(HASFDESCFS) */

#if	defined(HASPROCFS)
	struct pfsnode p;
	struct procfsid *pfi;
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

#if	BSDIV<30000
		if (vfs->type == MOUNT_UFS || vfs->type == MOUNT_MFS)
		    Ntype = N_REGLR;
		else if (vfs->type == MOUNT_NFS)
		    Ntype = N_NFS;
		else if (vfs->type == MOUNT_CD9660)
		    Ntype = N_CDFS;
		else if (vfs->type == MOUNT_MSDOS)
		    Ntype = N_PCFS;
		else if (vfs->type == MOUNT_NONE) {
		    if (v->v_type != VNON && v->v_type != VBAD) {
			enter_nm("no file system");
			return;
		    }
		}

# if	defined(HASPROCFS)
		else if (vfs->type == MOUNT_PROCFS)
		    Ntype = N_PROC;
# endif	/* defined(HASPROCFS) */

		else {
		    (void) snpf(Namech, Namechl,
			"unsupported file system type: %d", vfs->type);
		    enter_nm(Namech);
		    return;
		}
#else	/* BSDIV>=30000 */
		if (strcasecmp(vfs->typnm, "ufs") == 0
		||  strcasecmp(vfs->typnm, "mfs") == 0)
		    Ntype = N_REGLR;
		else if (strcasecmp(vfs->typnm, "nfs") == 0)
		    Ntype = N_NFS;
		else if (strcasecmp(vfs->typnm, "cd9660") == 0)
		    Ntype = N_CDFS;
		else if (strcasecmp(vfs->typnm, "msdos") == 0)
		    Ntype = N_PCFS;

# if	defined(HASPROCFS)
		else if (strcasecmp(vfs->typnm, "procfs") == 0)
		    Ntype = N_PROC;
# endif	/* defined(HASPROCFS) */

		else {
		    if (vfs->typnm[0]
		    ||  (v->v_type != VNON && v->v_type != VBAD)) {
			(void) snpf(Namech, Namechl,
			    "unsupported file system type: %s", vfs->typnm);
			enter_nm(Namech);
			return;
		    }
		}
#endif	/* BSDIV>=30000 */

	    }
	}
	if (!vfs && v->v_type != VNON && v->v_type != VBAD) {
	    enter_nm("no file system");
	    return;
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
	case VT_ISOFS:
	    if (kread((KA_T)v->v_data, (char *)&cd, sizeof(cd))) {
		(void) snpf(Namech, Namechl, "can't read cd9660_node at: %s",
		    print_kptr((KA_T)v->v_data, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	    nty = CDFSNODE;
	    (void) process_lock((KA_T)cd.i_lockf);
	    break;
	case VT_MFS:
	    if (kread((KA_T)v->v_data, (char *)&m, sizeof(m))) {
		(void) snpf(Namech, Namechl, "can't read mfsnode at: %s",
		    print_kptr((KA_T)v->v_data, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	    nty = MFSNODE;
	    break;
	case VT_MSDOSFS:
	    if (kread((KA_T)v->v_data, (char *)&dn, sizeof(dn))) {
		(void) snpf(Namech, Namechl, "can't read denode at: %s",
		    print_kptr((KA_T)v->v_data, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	    nty = PCFSNODE;
	    (void) process_lock((KA_T)dn.de_lockf);
	    break;
	case VT_NFS:
	    if (kread((KA_T)v->v_data, (char *)&n, sizeof(n))) {
		(void) snpf(Namech, Namechl, "can't read nfsnode at: %s",
		    print_kptr((KA_T)v->v_data, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	    nty = NFSNODE;
	    break;

#if	defined(HASFDESCFS)
	case VT_FDESC:
	    if (kread((KA_T)v->v_data, (char *)&f, sizeof(f))) {
		(void) snpf(Namech, Namechl, "can't read fdescnode at: %s",
		    print_kptr((KA_T)v->v_data, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	    nty = FDESCNODE;
	    break;
#endif	/* defined(HASFDESCFS) */

#if	defined(HASPROCFS)
	case VT_PROCFS:
	    if (kread((KA_T)v->v_data, (char *)&p, sizeof(p))) {
		(void) snpf(Namech, Namechl, "can't read pfsnode at: %s",
		    print_kptr((KA_T)v->v_data, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	    nty = PFSNODE;
	    break;
#endif	/* defined(HASPROCFS) */

	case VT_UFS:
	    if (kread((KA_T)v->v_data, (char *)&i, sizeof(i))) {
		(void) snpf(Namech, Namechl, "can't read inode at: %s",
		    print_kptr((KA_T)v->v_data, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	    nty = INODE;
	    (void) process_lock((KA_T)i.i_lockf);
	    break;
	}
/*
 * Get device and type for printing.
 */
	type = v->v_type;
	switch (nty) {
	case INODE:
	    dev = i.i_dev;
	    devs = 1;
	    if (type == VCHR || type == VBLK) {
		rdev = i.i_din.di_rdev;
		rdevs = 1;
	    }
	    break;
	case NFSNODE:
	    dev = n.n_vattr.va_fsid;
	    devs = 1;
	    break;
	case CDFSNODE:
	    dev = cd.i_dev;
	    devs = 1;
	    break;
	case PCFSNODE:
	    dev = dn.de_dev;
	    devs = 1;
	}
/*
 * Obtain the inode number.
 */
	switch (nty) {

#if	defined(HASFDESCFS)
	case FDESCNODE:

# if	defined(HASFDLINK)
	    if (f.fd_link
	    &&  !kread((KA_T)f.fd_link, Namech, (int)(Namechl - 1))) {
		Namech[(int)(Namechl - 1)] = '\0';
		break;
	    }
# endif	/* defined(HASFDLINK)

# if	HASFDESCFS==1
	    if (f.fd_type == Fctty) {
		if (f_tty_s == 0)
		    f_tty_s = lkup_dev_tty(&f_tty_dev, &f_tty_ino);
		if (f_tty_s == 1) {
		    dev = f_tty_dev;
		    devs = 1;
		    Lf->inode = f_tty_ino;
		    Lf->inp_ty = 1;
		}
	    }
	    break;
# endif	/* HASFDESCFS==1 */

#endif	/* defined(HASFDESCFS) */

	case INODE:
	    if (type != VBLK) {
		Lf->inode = (unsigned long)i.i_number;
		Lf->inp_ty = 1;
	    }
	    break;
	case NFSNODE:
	    Lf->inode = (unsigned long)n.n_vattr.va_fileid;
	    Lf->inp_ty = 1;
	    break;
	case CDFSNODE:
	    Lf->inode = (unsigned long)cd.i_number;
	    Lf->inp_ty = 1;
	    break;
	case PCFSNODE:
	    if (dn.de_Attributes & ATTR_DIRECTORY) {
		if ((cn = dn.de_StartCluster) == MSDOSFSROOT)
		    cn = 1;
	    } else {
		if ((cn = dn.de_dirclust) == MSDOSFSROOT)
		    cn = 1;
		cn = (cn << 16)

#if	BSDIV<30000
		   | (dn.de_diroffset & 0xffff);
#else	/* BSDIV>=30000 */
		   | ((dn.de_diroffset / sizeof(struct direntry)) & 0xffff);
#endif	/* BSDIV<30000 */

	    }
	    Lf->inode = (unsigned long)cn;
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
	    case N_CDFS:
		if (nty == CDFSNODE) {
		    Lf->sz = (SZOFFTYPE)cd.i_size;
		    Lf->sz_def = 1;
		}
		break;
	    case N_FIFO:
		if (!Fsize)
		    Lf->off_def = 1;
		break;
	    case N_PCFS:
		if (nty == PCFSNODE) {
		    Lf->sz = (SZOFFTYPE)dn.de_FileSize;
		    Lf->sz_def = 1;
		}
		break;
	    case N_NFS:
		if (nty == NFSNODE) {
		    Lf->sz = (SZOFFTYPE)n.n_vattr.va_size;
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
		    case Pmem:
			(void) getmemsz(p.pfs_pid);
			break;
		    }
		}
		break;
#endif	/* defined(HASPROCFS) */

	    case N_REGLR:
		if (type == VREG || type == VDIR) {
		    if (nty == INODE) {
			Lf->sz = (SZOFFTYPE)i.i_din.di_size;
			Lf->sz_def = 1;
		    } else if (nty == MFSNODE) {
			Lf->sz = (SZOFFTYPE)m.mfs_size;
			Lf->sz_def = 1;
		    }
		} else if ((type == VCHR || type == VBLK) && !Fsize)
		    Lf->off_def = 1;
		break;
	    }
	}
/*
 * Record link count.
 */
	if (Fnlink) {
	    switch (Ntype) {
	    case N_CDFS:
		Lf->nlink = (long)cd.inode.iso_links;
		Lf->nlink_def = 1;
		break;
	    case N_FIFO:		/* no link count? */
		break;
	    case N_PCFS:		/* no link count? */
		break;
	    case N_NFS:
		if (nty == NFSNODE) {
		    Lf->nlink = (long)n.n_vattr.va_nlink;
		    Lf->nlink_def = 1;
		}
		break;

# if	defined(HASPROCFS)
	    case N_PROC:		/* no link count? */
		break;
# endif	/* defined(HASPROCFS) */

	    case N_REGLR:
		if (nty == INODE) {
		    Lf->nlink = (long)i.i_din.di_nlink;
		    Lf->nlink_def = 1;
		}
		break;
	    }
	    if (Nlink && Lf->nlink_def && (Lf->nlink < Nlink))
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
	    (void) snpf(Lf->type, sizeof(Lf->type), ty);
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
	else if (nty == MFSNODE) {
	    devs = Lf->dev_def = Lf->rdev_def = rdevs = 0;
	    (void) snpf(Namech, Namechl, "%#x", m.mfs_baseoff);
	    enter_dev_ch("memory");
	}

#if	defined(HASPROCFS)
	else if (nty == PFSNODE) {
	    devs = Lf->dev_def = Lf->rdev_def = rdevs = 0;
	    ty = (char *)NULL;
	    (void) snpf(Namech, Namechl, "/%s", HASPROCFS);
	    switch (p.pfs_type) {
	    case Proot:
		ty = "PDIR";
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
		(void) snpf(Lf->type, sizeof(Lf->type), "%s", ty);
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
		Lf->sf |= SELNM;
		Procfind = 1;
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
