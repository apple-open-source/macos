/*
 * dproc.c - Solaris lsof functions for accessing process infomation
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
static char *rcsid = "$Id: dproc.c,v 1.23 2001/07/05 12:26:57 abe Exp $";
#endif

#include "lsof.h"

#if	solaris<20500
#include "kernelbase.h"
#endif	/* solaris<20500 */


/*
 * Local definitions
 */

#if	solaris>=20501
#define	KVMHASHBN	8192		/* KVM hash bucket count -- MUST BE
					 * A POWER OF 2!!! */
#define	HASHKVM(va)	((int)((va * 31415) >> 3) & (KVMHASHBN-1))
					/* virtual address hash function */

# if	solaris<70000
#define	KAERR	(u_longlong_t)-1	/* kvm_physaddr() error return */
#define	KBUFT	char			/* kernel read buffer type */
#define	KPHYS	u_longlong_t		/* kernel physical address type */
#define	KVIRT	u_int			/* kernel virtual address type */
# else	/* solaris>=70000 */
#define	KAERR	(uint64_t)-1		/* kvm_physaddr() error return */
#define	KBUFT	void			/* kernel read buffer type */
#define	KPHYS	uint64_t		/* kernel physical address type */
#define	KVIRT	uintptr_t		/* kernel virtual address type */
# endif	/* solaris<70000 */
#endif	/* solaris>=20501 */


/*
 * Local structures
 */

#if	solaris>=20501
typedef struct kvmhash {
	KVIRT vpa;			/* virtual page address */
	KPHYS pa;			/* physical address */
	struct kvmhash *nxt;		/* next virtual address */
} kvmhash_t;
#endif	/* solaris>=20501 */


/*
 * Local variables
 */

#if	solaris>=20501
static struct as *Kas = (struct as *)NULL;
					/* pointer to kernel's address space
					 * map in kernel virtual memory */
static kvmhash_t **KVMhb = (kvmhash_t **)NULL;
					/* KVM hash buckets */
static int PageSz = 0;			/* page size */
static int PSMask = 0;			/* page size mask */
static int PSShft = 0;			/* page size shift */

# if	solaris<70000
static struct as Kam;			/* kernel's address space map */
static int Kmd = -1;			/* memory device file descriptor */
# endif	/* solaris<70000 */
#endif	/* solaris>=20501 */

#if	solaris>=20500
static KA_T Kb = (KA_T)NULL;		/* KERNELBASE for Solaris 2.5 */
#endif	/* solaris>=20500 */

static int Knp = PROCDFLT;		/* numer of proc structures known
					 * to the kernel */
static int Np;				/* number of P[], Pgid[], and Pid[]
					 * entries  */
static struct proc *P = NULL;		/* local proc structure table */
static KA_T Sgvops;			/* [_]segvn_ops address */
static int *Pgid = NULL;		/* process group IDs for P[] entries */
static int *Pid = NULL;			/* PIDs for P[] entries */
static gid_t Savedgid;			/* saved (effective) GID */
static int Switchgid = 0;		/* must switch GIDs for kvm_open() */


/*
 * Local function prototypes
 */

_PROTOTYPE(static void get_kernel_access,(void));
_PROTOTYPE(static void process_text,(KA_T pa));
_PROTOTYPE(static void read_proc,(void));
_PROTOTYPE(static void readfsinfo,(void));

#if	solaris>=20501
_PROTOTYPE(static void readkam,(KA_T addr));
#endif	/* solaris>=20501 */

#if	solaris>=20501 && solaris<70000
_PROTOTYPE(extern u_longlong_t kvm_physaddr,(kvm_t *, struct as *, u_int));
#endif	/* solaris>=20501 && solaris<70000 */



/*
 * close_kvm() - close kernel virtual memory access
 */

void
close_kvm()
{
	if (!Kd)
	    return;
	if (Kd) {
	    if (kvm_close(Kd) != 0) {
		(void) fprintf(stderr, "%s: kvm_close failed\n", Pn);
		Exit(1);
	    }
	    Kd = (kvm_t *)NULL;
	}

#if	solaris>=20501 && solaris<70000
	if (Kmd >= 0) {
	    (void) close(Kmd);
	    Kmd = -1;
	}
#endif	/* solaris>=20501 && solaris<70000 */

}


/*
 * gather_proc_info() - gather process information
 */

void
gather_proc_info()
{
	MALLOC_S bc;
	static int ft = 1;
	int i, j;
	struct proc *p;
	int pgid, pid, px;
	long pofv;
	short pss, sf;
	struct user *u;
	uid_t uid;

#if	solaris>=20400
	int k;

# if	!defined(NFPCHUNK)
#define	uf_ofile	uf_file
#define	uf_pofile	uf_flag
#define	u_flist		u_finfo.fi_list
#define	u_nofiles	u_finfo.fi_nfiles
#define	NFPREAD		64
# else	/* defined(NFPCHUNK) */
#define	NFPREAD		NFPCHUNK
# endif	/* !defined(NFPCHUNK) */
	uf_entry_t uf[NFPREAD];
#endif	/* solaris>=20400 */
#if	solaris>=20500
	struct cred pc;
#endif	/* solaris>=20500 */

#if	defined(HASFSTRUCT)
	static int npofb = 0;
	char *pofp;
	static char *pofb = (char *)NULL;
#endif	/* defined(HASFSTRUCT) */

/*
 * Do first-time only operations.
 */
	if (ft) {
	    if (get_Nl_value("sgvops", Drive_Nl, &Sgvops) < 0)
		Sgvops = (KA_T)NULL;
	    ft = 0;
	}

#if	solaris>=20501
	else

	/*
	 * If not the first time, re-read the kernel's address space map.
	 */
	    readkam((KA_T)NULL);
#endif	/* solaris>=20501 */

/*
 * Read the process table.
 *
 * The Solaris process table is a linked list whose head pointer is acquired
 * by open_kvm()'s call to kvm_open().  For Solaris close_kvm() is called so
 * the process table head pointer can be re-acquired by a call to open_kvm()
 * immediately before the actual reading of the process table.
 *
 * Since the SunOS 4.1.x process table is sequential, there's no worry about
 * a stale or incorrect list head pointer.  Thus no close_kvm() is needed.
 * A kvm_setproc() call is sufficient.
 */
	close_kvm();
	read_proc();
/*
 * Loop through processes.
 */
	for (p = P, px = 0; px < Np; p++, px++) {

	/*
	 * Get the process ID.
	 */

	    if (Fpgid)
		pgid = Pgid[px];
	    else
		pgid = 0;
	    pid = Pid[px];

#if solaris<20500
	    uid = p->p_uid;
#else	/* solaris >=20500 */
	/*
	 * Read credentials for Solaris 2.5 and above process.
	 */
	    if (kread((KA_T)p->p_cred, (char *)&pc, sizeof(pc)))
		continue;
	    uid = pc.cr_uid;
#endif	/* solaris<20500 */

	/*
	 * See if the process is excluded.
	 */
	    if  (is_proc_excl(pid, pgid, (UID_ARG)uid, &pss, &sf))
		continue;
	/*
	 * Get the user area associated with the process.
	 */
	    u = &p->p_user;
	/*
	 * Allocate a local process structure and start filling it.
	 */
	    if (is_cmd_excl(u->u_comm, &pss, &sf))
		continue;
	    alloc_lproc(pid, pgid, (int)p->p_ppid, (UID_ARG)uid, u->u_comm,
		(int)pss, (int)sf);
	    Plf = (struct lfile *)NULL;
	/*
	 * Save current working directory information.
	 */
	    if (u->u_cdir) {
		alloc_lfile(CWD, -1);

#if	defined(FILEPTR)
		FILEPTR = (struct file *)NULL;
#endif	/* defined(FILEPTR) */

		process_node((KA_T)u->u_cdir);
		if (Lf->sf)
		    link_lfile();
	    }
	/*
	 * Save root directory information.
	 */
	    if (u->u_rdir) {
		alloc_lfile(RTD, -1);

#if	defined(FILEPTR)
		FILEPTR = (struct file *)NULL;
#endif	/* defined(FILEPTR) */

		process_node((KA_T)u->u_rdir);
		if (Lf->sf)
		    link_lfile();
	    }
	/*
	 * Save information on text files.
	 */
	    if (p->p_as && Sgvops) {

#if	defined(FILEPTR)
		FILEPTR = (struct file *)NULL;
#endif	/* defined(FILEPTR) */

		process_text((KA_T)p->p_as);
	    }
	/*
	 * Save information on file descriptors.
	 *
	 * Under Solaris the file pointers are stored in dynamically-linked
	 * ufchunk structures, each containing NFPREAD file pointers.  The
	 * first ufchunk structure is in the user area.
	 *
	 * Under Solaris 2.4 the file pointers are in a dynamically allocated,
	 * contiguous memory block.
	 */

#if	solaris<20400
	    for (i = 0, j = 0; i < u->u_nofiles; i++) {
		if (++j > NFPCHUNK) {
		    if (!u->u_flist.uf_next)
			break;
		    if (kread((KA_T)u->u_flist.uf_next,
			(char *)&u->u_flist, sizeof(struct ufchunk)))
			    break;
		    j = 1;
		}
		if (!u->u_flist.uf_ofile[j-1])
#else	/* solaris>=20400 */
	    for (i = 0, j = NFPREAD; i < u->u_nofiles; i++) {
		if (++j > NFPREAD) {
		    k = u->u_nofiles - i;
		    if (k > NFPREAD)
			k = NFPREAD;
		    if (kread((KA_T)((unsigned long)u->u_flist +
				     i * sizeof(uf_entry_t)),
				     (char*)&uf, k * sizeof(uf_entry_t)))
		    {
			break;
		    }
		    j = 1;
		}
		if (!uf[j-1].uf_ofile)
#endif	/* solaris<20400 */

		    continue;
		alloc_lfile((char *)NULL, i);

#if	solaris<20400
		pofv = (long)u->u_flist.uf_pofile[j-1];
		process_file((KA_T)u->u_flist.uf_ofile[j-1]);
#else	/* solaris>=20400 */
		pofv = uf[j-1].uf_pofile;
		process_file((KA_T)uf[j-1].uf_ofile);
#endif	/* solaris <20400 */

		if (Lf->sf) {

#if	defined(HASFSTRUCT)
		    if (Fsv & FSV_FG)
			Lf->pof = pofv;
#endif	/* defined(HASFSTRUCT) */

		    link_lfile();
		}
	    }
	/*
	 * Examine results.
	 */
	    if (examine_lproc())
		return;
	}
}


/*
 * get_kernel_access() - access the required information in the kernel
 */

static void
get_kernel_access()
{
	char er[64];
	int i;
	KA_T v;

#if	defined(HAS_AFS)
	struct nlist *nl = (struct nlist *)NULL;
#endif	/* defined(HAS_AFS) */

/*
 * Check the Solaris or SunOS version number; check the SunOS architecture.
 */
	(void) ckkv("Solaris", LSOF_VSTR, (char *)NULL, (char *)NULL);

#if	solaris>=70000
/*
 * Compare the Solaris 7 lsof compilation bit size with the kernel bit size.
 * Quit on a mismatch.
 */
	{
	    char *cp, isa[1024];
	    short kbits = 32;

# if	defined(_LP64)
	    short xkbits = 64;
# else	/* !defined(_LP64) */
	    short xkbits = 32;
# endif	/* defined(_LP64) */

	    if (sysinfo(SI_ISALIST, isa, (long)sizeof(isa)) < 0) {
		(void) fprintf(stderr, "%s: can't get ISA list: %s\n",
		    Pn, strerror(errno));
		Exit(1);
	    }
	    for (cp = isa; *cp;) {
		if (strncmp(cp, "sparcv9", strlen("sparcv9")) == 0) {
		    kbits = 64;
		    break;
		}
		if (!(cp = strchr(cp, ' ')))
		    break;
		cp++;
	    }
	    if (kbits != xkbits) {
		(void) fprintf(stderr,
		    "%s: FATAL: lsof was compiled for a %d bit kernel,\n",
		    Pn, (int)xkbits);
		(void) fprintf(stderr,
		    "      but this machine has booted a %d bit kernel.\n",
		    (int)kbits);
		Exit(1);
	    }
	}
#endif	/* solaris>=70000 */

/*
 * Get kernel symbols.
 */
	if (Nmlst && !is_readable(Nmlst, 1))
	    Exit(1);
	(void) build_Nl(Drive_Nl);

#if	defined(HAS_AFS)
	if (!Nmlst) {

	/*
	 * If AFS is defined and we're getting kernel symbol values from
	 * from N_UNIX, make a copy of Nl[] for possible use with the AFS
	 * modload file.
	 */
	    if (!(nl = (struct nlist *)malloc(Nll))) {
		(void) fprintf(stderr, "%s: no space (%d) for Nl[] copy\n",
		    Pn, Nll);
		Exit(1);
	    }
	    (void) memcpy((void *)nl, (void *)Nl, (size_t)Nll);
	}
#endif	/* defined(HAS_AFS) */

	if (nlist(Nmlst ? Nmlst : N_UNIX, Nl) < 0) {
	    (void) fprintf(stderr, "%s: can't read namelist from %s\n",
		Pn, Nmlst ? Nmlst : N_UNIX);
	    Exit(1);
	}

#if	defined(HAS_AFS)
	if (nl) {

	/*
	 * If AFS is defined and we're getting kernel symbol values from
	 * N_UNIX, and if any X_AFS_* symbols isn't there, see if it is in the
	 * the AFS modload file.  Make sure that other symbols that appear in
	 * both name list files have the same values.
	 */
	    if ((get_Nl_value("arFID", Drive_Nl, &v) >= 0 && !v)
	    ||  (get_Nl_value("avops", Drive_Nl, &v) >= 0 && !v)
	    ||  (get_Nl_value("avol",  Drive_Nl, &v) >= 0 && !v))
		(void) ckAFSsym(nl);
	    (void) free((MALLOC_P *)nl);
	}
#endif	/* defined(HAS_AFS) */

#if	defined(WILLDROPGID)
/*
 * If Solaris kernel memory is coming from KMEM, and the process is willing
 * to surrender GID permission, set up for GID switching after the first
 * call to open_kvm().
 */
	if (!Memory) {
	    Savedgid = getegid();
	    if (Setgid)
		Switchgid = 1;
	}
/*
 * If kernel memory isn't coming from KMEM, drop setgid permission
 * before attempting to open the (Memory) file.
 */
	if (Memory)
	    (void) dropgid();
#else	/* !defined(WILLDROPGID) */
/*
 * See if the non-KMEM memory file is readable.
 */
	if (Memory && !is_readable(Memory, 1))
	    Exit(1);
#endif	/* defined(WILLDROPGID) */

/*
 * Open access to kernel memory.
 */
	open_kvm();
/*
 * Get a proc structure count estimate.
 */
	if (get_Nl_value("nproc", Drive_Nl, &v) < 0 || !v
	||  kread((KA_T)v, (char *)&Knp, sizeof(Knp))
	||  Knp < 1)
	    Knp = PROCDFLT;

#if	solaris>=20500
/*
 * Get the kernel's KERNELBASE value for Solaris 2.5 and above.
 */
	v = (KA_T)0;
	if (get_Nl_value("kbase", Drive_Nl, &v) < 0 || !v
	||  kread((KA_T)v, (char *)&Kb, sizeof(Kb))) {
	    (void) fprintf(stderr,
		"%s: can't read kernel base address from %s\n",
		Pn, print_kptr(v, (char *)NULL, 0));
	    Exit(1);
	}
#endif	/* solaris>=20500 */

/*
 * Get the Solaris clone major device number, if possible.
 */
	v = (KA_T)0;
	if (get_Nl_value("clmaj", Drive_Nl, &v) >= 0 && v
	&&  kread((KA_T)v, (char *)&CloneMaj, sizeof(CloneMaj)) == 0)
	    HaveCloneMaj = 1;

#if	solaris>=20501
/*
 * Get the kernel's virtual to physical map structure for Solaris 2.5.1 and
 * above.
 */
	if (get_Nl_value("kasp", Drive_Nl, &v) >= 0 && v) {
	    PageSz = getpagesize();
	    PSMask = PageSz - 1;
	    for (i = 1, PSShft = 0; i < PageSz; i <<= 1, PSShft++)
		;
	    (void) readkam(v);
	}
#endif	/* solaris>=20501 */

}


/*
 * initialize() - perform all initialization
 */

void
initialize()
{
	get_kernel_access();
/*
 * Read Solaris file system information and construct the clone table.
 *
 * The clone table is needed to identify sockets.
 */
	readfsinfo();

#if	defined(HASDCACHE)
	readdev(0);
#else	/* !defined(HASDCACHE) */
	read_clone();
#endif	/*defined(HASDCACHE) */

}


/*
 * kread() - read from kernel memory
 */

int
kread(addr, buf, len)
	KA_T addr;			/* kernel memory address */
	char *buf;			/* buffer to receive data */
	READLEN_T len;			/* length to read */
{
	register int br;
/*
 * Because lsof reads kernel data and follows pointers found there at a
 * rate considerably slower than the kernel, lsof sometimes acquires
 * invalid pointers.  If the invalid pointers are fed to kvm_[k]read(),
 * a segmentation violation may result, so legal kernel addresses are
 * limited by the value of the KERNELBASE symbol (Kb value from the
 * kernel's _kernelbase variable for Solaris 2.5 and above).
 */

#if	solaris>=20500
#define	KVMREAD	kvm_kread
	if (addr < Kb)
#else	/* solaris<20500 */
#define	KVMREAD kvm_read
	if (addr < (KA_T)KERNELBASE)
#endif	/* solaris>=20500 */

	    return(1);

#if	solaris>=20501

/*
 * Make sure the virtual address represents real physical memory by testing
 * it with kvm_physaddr().
 *
 * For Solaris below 7 read the kernel data with llseek() and read().  For
 * Solaris 7 and above use kvm_pread().
 */
	if (Kas) {

# if	solaris>20501
	    register int b2r;
	    register char *bp;
# endif	/* solaris>20501 */

	    register int h, ip, tb;
	    register kvmhash_t *kp;
	    KPHYS pa;
	    register KVIRT va, vpa;

# if	solaris<20600
	    for (tb = 0, va = (KVIRT)addr;
		 tb < len;
		 tb += br, va += (KVIRT)br)
# else	/* solaris>=20600 */
	    for (bp = buf, tb = 0, va = (KVIRT)addr;
		 tb < len;
		 bp += br, tb += br, va += (KVIRT)br)
# endif	/* solaris<20600 */

	    {
		vpa = (va & (KVIRT)~PSMask) >> PSShft;
		ip = (int)(va & (KVIRT)PSMask);
		h = HASHKVM(vpa);
		for (kp = KVMhb[h]; kp; kp = kp->nxt) {
		    if (kp->vpa == vpa) {
			pa = kp->pa;
			break;
		    }
		}
		if (!kp) {
		    if ((pa = kvm_physaddr(Kd, Kas, va)) == KAERR)
			return(1);
		    if (!(kp = (kvmhash_t *)malloc(sizeof(kvmhash_t)))) {
			(void) fprintf(stderr, "%s: no kvmhash_t space\n", Pn);
			Exit(1);
		    }
		    kp->nxt = KVMhb[h];
		    pa = kp->pa = (pa & ~(KPHYS)PSMask);
		    kp->vpa = vpa;
		    KVMhb[h] = kp;
		}

# if	solaris<20600
		br = (int)(len - tb);
		if ((ip + br) > PageSz)
		    br = PageSz - ip;
# else	/* solaris>=20600 */
		b2r = (int)(len - tb);
		if ((ip + b2r) > PageSz)
		    b2r = PageSz - ip;
		pa |= (KPHYS)ip;

#  if	solaris<70000
		if (llseek(Kmd, (offset_t)pa, SEEK_SET) == (offset_t)-1)
		    return(1);
		if ((br = (int)read(Kmd, (void *)bp, (size_t)b2r)) <= 0)
		    return(1);
#  else	/* solaris>=70000 */
		if ((br = kvm_pread(Kd, pa, (void *)bp, (size_t)b2r)) <= 0)
		    return(1);
#  endif	/* solaris<70000 */
# endif	/* solaris<20600 */

	    }

# if	solaris>=20600
	    return(0);
# endif	/* solaris>=20600 */

	}
#endif	/* solaris>=20501 */

/*
 * Use kvm_read for Solaris < 2.5; use kvm_kread() Solaris >= 2.5.
 */
	br = KVMREAD(Kd, (u_long)addr, buf, len);
	return(((READLEN_T)br == len) ? 0 : 1);
}


/*
 * open_kvm() - open kernel virtual memory access
 */

void
open_kvm()
{
	if (Kd)
	    return;

#if	defined(WILLDROPGID)
/*
 * If this Solaris process began with setgid permission and its been
 * surrendered, regain it.
 */
	(void) restoregid();
#endif	/* defined(WILLDROPGID) */

	if (!(Kd = kvm_open(Nmlst, Memory, NULL, O_RDONLY, NULL))) {
	    (void) fprintf(stderr, "%s: kvm_open (namelist=%s, core=%s): %s\n",
		Pn,
		Nmlst ? Nmlst : "default",
		Memory  ? Memory  : "default",
		strerror(errno));
	    Exit(1);
	}

#if	solaris>=20501 && solaris<70000
	if ((Kmd = open((Memory ? Memory : KMEM), O_RDONLY)) < 0) {
	    (void) fprintf(stderr, "%s: open(\"/dev/mem\"): %s\n", Pn, 
		strerror(errno));
	    Exit(1);
	}
#endif	/* solaris>=20501 && solaris<70000 */

#if	defined(WILLDROPGID)
/*
 * If this process has setgid permission, and is willing to surrender it,
 * do so.
 */
	(void) dropgid();
/*
 * If this Solaris process must switch GIDs, enable switching after the
 * first call to this function.
 */
	if (Switchgid == 1)
	    Switchgid = 2;
#endif	/* define(WILLDROPGID) */

}


/*
 * process_text() - process text access information
 */

#if	solaris>=90000
#include <sys/avl.h>

/*
 * Avl trees are implemented as follows: types in AVL trees contain an
 * avl_node_t.  These avl_nodes connect to other avl nodes embedded in
 * objects of the same type.  The avl_tree contains knowledge about the
 * size of the structure and the offset of the AVL node in the object
 * so we can convert between AVL nodes and (in this case) struct seg. 
 *
 * This code was provided by Casper Dik <Casper.Dik@holland.sun.com>.
 */

#define READ_AVL_NODE(n,o,s) \
	if (kread((KA_T)AVL_NODE2DATA(n, o), (char*) s, sizeof(*s))) \
		return -1

static int
get_first_seg(avl_tree_t *av, struct seg *s)
{
	avl_node_t *node = av->avl_root;
	size_t off = av->avl_offset;
	int count = 0;

	while (node != NULL && ++count < MAXSEGS * 2) {
	    READ_AVL_NODE(node, off, s);
	    node = s->s_tree.avl_child[0];
	    if (node == NULL)
		return 0;
	}
	return -1;
}

static int
get_next_seg(avl_tree_t *av, struct seg *s)
{
	avl_node_t *node = &s->s_tree;
	size_t off = av->avl_offset;
	int count = 0;

	if (node->avl_child[1]) {
	    /*
	     * Has right child, go all the way to the leftmost child of
	     * the right child.
	     */
	    READ_AVL_NODE(node->avl_child[1], off, s);
	    while (node->avl_child[0] != NULL && ++count < 2 * MAXSEGS)
		 READ_AVL_NODE(node->avl_child[0],off,s);
	    if (count < 2 * MAXSEGS)
		return 0;
	} else {
	    /*
	     * No right child, go up until we find a node we're not a right
	     * child of.
	     */
	    for (;count < 2 * MAXSEGS; count++) {
		int index = AVL_XCHILD(node);
		avl_node_t *parent = AVL_XPARENT(node);

		if (parent == NULL)
		    return -1;

		READ_AVL_NODE(parent, off, s);

		if (index == 0)
		    return 0;
	    }
	}
	return -1;
}

static void
process_text(pa)
	KA_T pa;			/* address space description pointer */
{
	struct as as;
	int i, j, k, l;
	struct seg s;
	struct segvn_data vn;
	avl_tree_t *avtp;
	KA_T v[MAXSEGS];
/*
 * Get address space description.
 */
	if (kread((KA_T)pa, (char *)&as, sizeof(as))) {
	    alloc_lfile(" txt", -1);
	    (void) snpf(Namech, Namechl, "can't read text segment list (%s)",
		print_kptr(pa, (char *)NULL, 0));
	    enter_nm(Namech);
	    if (Lf->sf)
		link_lfile();
	    return;
	}
/*
 * Loop through the segments.  The loop should stop when the segment
 * pointer returns to its starting point, but just in case, it's stopped
 * when MAXSEGS have been recorded or 2*MAXSEGS have been examined.
 */
	avtp = &as.a_segtree;

	for (i = j = k = 0; i < MAXSEGS && j < 2*MAXSEGS; j++) {
	    if (j == 0 ? get_first_seg(avtp, &s) : get_next_seg(avtp, &s))
		break;
		
	    if ((KA_T)s.s_ops == Sgvops && s.s_data) {
		if (kread((KA_T)s.s_data, (char *)&vn, sizeof(vn)))
		    break;
		if (vn.vp) {
			
		/*
		 * This is a virtual node segment.
		 *
		 * If its vnode pointer has not been seen already,
		 * print its information.
		 */
		    for (l = 0; l < k; l++) {
			if (v[l] == (KA_T)vn.vp)
			    break;
		    }
		    if (l >= k) {
			alloc_lfile(" txt", -1);

# if	defined(FILEPTR)
			FILEPTR = (struct file *)NULL;
# endif	/* defined(FILEPTR) */

			process_node((KA_T)vn.vp);
			if (Lf->sf) {
			    link_lfile();
			    i++;
			}
			v[k++] = (KA_T)vn.vp;
		    }
		}
	    }
	}
}

#else	/* solaris<90000 */

# if	solaris>=20400
#define S_NEXT s_next.list
# else	/* solaris<20400 */
#define S_NEXT s_next
# endif	/* solaris>=20400 */

static void
process_text(pa)
	KA_T pa;			/* address space description pointer */
{
	struct as as;
	int i, j, k, l;
	struct seg s;
	struct segvn_data vn;
	KA_T v[MAXSEGS];
/*
 * Get address space description.
 */
	if (kread((KA_T)pa, (char *)&as, sizeof(as))) {
	    alloc_lfile(" txt", -1);
	    (void) snpf(Namech, Namechl, "can't read text segment list (%s)",
		print_kptr(pa, (char *)NULL, 0));
	    enter_nm(Namech);
	    if (Lf->sf)
		link_lfile();
	    return;
	}
/*
 * Loop through the segments.  The loop should stop when the segment
 * pointer returns to its starting point, but just in case, it's stopped
 * when MAXSEGS have been recorded or 2*MAXSEGS have been examined.
 */
	s.s_next = as.a_segs;
	for (i = j = k = 0; i < MAXSEGS && j < 2*MAXSEGS; j++) {
	    if (!s.S_NEXT
	    ||  kread((KA_T)s.S_NEXT, (char *)&s, sizeof(s)))
		break;
	    if ((KA_T)s.s_ops == Sgvops && s.s_data) {
		if (kread((KA_T)s.s_data, (char *)&vn, sizeof(vn)))
		    break;
		if (vn.vp) {
			
		/*
		 * This is a virtual node segment.
		 *
		 * If its vnode pointer has not been seen already,
		 * print its information.
		 */
		    for (l = 0; l < k; l++) {
			if (v[l] == (KA_T)vn.vp)
			    break;
		    }
		    if (l >= k) {
			alloc_lfile(" txt", -1);

# if	defined(FILEPTR)
			FILEPTR = (struct file *)NULL;
# endif	/* defined(FILEPTR) */

			process_node((KA_T)vn.vp);
			if (Lf->sf) {
			    link_lfile();
			    i++;
			}
			v[k++] = (KA_T)vn.vp;
		    }
		}
	    }
	/*
	 * Follow the segment link to the starting point in the address
	 * space description.  (The i and j counters place an absolute
	 * limit on the loop.)
	 */

# if	solaris<20400
	    if (s.s_next == as.a_segs)
# else	/* solaris>=20400 */
	    if (s.s_next.list == as.a_segs.list)
# endif	/* solaris<20400 */

		break;
	}
}
#endif  /* solaris>=90000 */


/*
 * readfsinfo() - read file system information
 */

static void
readfsinfo()
{
	char buf[FSTYPSZ+1];
	int i, len;

	if ((Fsinfomax = sysfs(GETNFSTYP)) == -1) {
	    (void) fprintf(stderr, "%s: sysfs(GETNFSTYP) error: %s\n",
		Pn, strerror(errno));
	    Exit(1);
	} 
	if (Fsinfomax == 0)
		return;
	if (!(Fsinfo = (char **)malloc((MALLOC_S)(Fsinfomax * sizeof(char *)))))
	{
	    (void) fprintf(stderr, "%s: no space for sysfs info\n", Pn);
	    Exit(1);
	}
	for (i = 1; i <= Fsinfomax; i++) {
	    if (sysfs(GETFSTYP, i, buf) == -1) {
		(void) fprintf(stderr, "%s: sysfs(GETFSTYP) error: %s\n",
		    Pn, strerror(errno));
		Exit(1);
	    }
	    if (buf[0] == '\0') {
		Fsinfo[i-1] = "";
		continue;
	    }
	    buf[FSTYPSZ] = '\0';
	    len = strlen(buf) + 1;
	    if (!(Fsinfo[i-1] = (char *)malloc((MALLOC_S)len))) {
		(void) fprintf(stderr,
		    "%s: no space for file system entry %s\n", Pn, buf);
		Exit(1);
	    }
	    (void) snpf(Fsinfo[i-1], len, "%s", buf);

# if	defined(HAS_AFS)
	    if (strcasecmp(buf, "afs") == 0)
		AFSfstype = i;
# endif	/* defined(HAS_AFS) */

	}
}


#if	solaris>=20501
/*
 * readkam() - read kernel's address map structure
 */

static void
readkam(addr)
	KA_T addr;			/* kernel virtual address */
{
	register int i;
	register kvmhash_t *kp, *kpp;
	static KA_T kas = (KA_T)NULL;
	size_t sz;

	if (addr)
	    kas = addr;
	Kas = (struct as *)NULL;

#if	solaris<70000
	if (kas && !kread(kas, (char *)&Kam, sizeof(Kam)))
	    Kas = (KA_T)&Kam;
#else	/* solaris>=70000 */
	Kas = (struct as *)kas;
#endif	/* solaris<70000 */

	if (Kas) {
	    if (!KVMhb) {
		if (!(KVMhb = (kvmhash_t **)calloc(KVMHASHBN,
						   sizeof(kvmhash_t *))))
		{
		     (void) fprintf(stderr,
			"%s: no space (%d) for KVM hash buckets\n",
			Pn, (KVMHASHBN * sizeof(kvmhash_t *)));
		    Exit(1);
		}
	    } else if (!addr) {
		for (i = 0; i < KVMHASHBN; i++) {
		    if ((kp = KVMhb[i])) {
			while (kp) {
			    kpp = kp->nxt;
			    (void) free((void *)kp);
			    kp = kpp;
			}
			KVMhb[i] = (kvmhash_t *)NULL;
		    }
		}
	    }
	}
}
#endif	/* solaris>=20501 */


/*
 * read_proc() - read proc structures
 *
 * As a side-effect, Kd is set by a call to kvm_open().
 */

static void
read_proc()
{
	MALLOC_S len;
	static int sz = 0;
	int try;
	struct proc *p;
	struct pid pg, pids;
/*
 * Try PROCTRYLM times to read a valid proc table.
 */
	for (try = 0; try < PROCTRYLM; try++) {

	/*
	 * Pre-allocate proc structure space.
	 */
	    if (sz == 0) {
		sz = Knp + PROCDFLT/4;
		len = (sz * sizeof(struct proc));
		if (!(P = (struct proc *)malloc(len))) {
		    (void) fprintf(stderr, "%s: no proc table space\n", Pn);
		    Exit(1);
		}
	/*
	 * Pre-allocate space for Solaris PGID and PID numbers.
	 */
		len = (MALLOC_S)(sz * sizeof(int));
		if (Fpgid) {
		    if (!(Pgid = (int *)malloc(len))) {
			(void) fprintf(stderr, "%s: no PGID table space\n", Pn);
			Exit(1);
		    }
		}
		if (!(Pid = (int *)malloc(len))) {
		    (void) fprintf(stderr, "%s: no PID table space\n", Pn);
		    Exit(1);
		}
	    }
	/*
	 * Prepare for a proc table scan.
	 */
	    open_kvm();
	    if (kvm_setproc(Kd) != 0) {
		(void) fprintf(stderr, "%s: kvm_setproc: %s\n", Pn,
		    strerror(errno));
		Exit(1);
	    }
	/*
	 * Accumulate proc structures.
	 */
	    Np = 0;
	    while ((p = kvm_nextproc(Kd))) {
		if (p->p_stat == 0 || p->p_stat == SZOMB)
		    continue;

#if	solaris >=20500
		/*
		 * Check Solaris 2.5 and above p_cred pointer.
		 */
	    	    if (!p->p_cred)
			continue;
#endif	/* solaris >=20500 */

		/*
		 * Read Solaris PGID and PID numbers.
		 */
		if (Fpgid) {
		    if (!p->p_pgidp
		    ||  kread((KA_T)p->p_pgidp, (char *)&pg, sizeof(pg)))
			continue;
		}
		if (!p->p_pidp
		||  kread((KA_T)p->p_pidp, (char *)&pids, sizeof(pids)))
		    continue;
		if (Np >= sz) {

		/*
		 * Expand the local proc table.
		 */
		    sz += PROCDFLT/2;
		    len = (MALLOC_S)(sz * sizeof(struct proc));
		    if (!(P = (struct proc *)realloc((MALLOC_P *)P, len))) {
			(void) fprintf(stderr,
			    "%s: no more (%d) proc space\n", Pn, sz);
			Exit(1);
		    }
		/*
		 * Expand the Solaris PGID and PID tables.
		 */
		    len = (MALLOC_S)(sz * sizeof(int));
		    if (Fpgid) {
			if (!(Pgid = (int *)realloc((MALLOC_P *)Pgid, len))) {
			    (void) fprintf(stderr,
				"%s: no more (%d) PGID space\n", Pn, sz);
			    Exit(1);
			}
		    }
		    if (!(Pid = (int *)realloc((MALLOC_P *)Pid, len))) {
			(void) fprintf(stderr,
			    "%s: no more (%d) PID space\n", Pn, sz);
			Exit(1);
		    }
		}
	    /*
	     * Save the Solaris PGID and PID numbers in
	     * local tables.
	     */
		if (Fpgid)
		    Pgid[Np] = (int)pg.pid_id;
		Pid[Np] = (int)pids.pid_id;
	    /*
	     * Save the proc structure in a local table.
	     */
		P[Np++] = *p;
	    }
	/*
	 * If not enough processes were saved in the local table, try again.
	 */
	    if (Np >= PROCMIN)
		break;
	    close_kvm();
	}
/*
 * Quit if no proc structures were stored in the local table.
 */
	if (try >= PROCTRYLM) {
	    (void) fprintf(stderr, "%s: can't read proc table\n", Pn);
	    Exit(1);
	}
	if (Np < sz && !RptTm) {

	/*
	 * Reduce the local proc structure table size to its minimum if
	 * not in repeat mode.
	 */
	    len = (MALLOC_S)(Np * sizeof(struct proc));
	    if (!(P = (struct proc *)realloc((MALLOC_P *)P, len))) {
		(void) fprintf(stderr, "%s: can't reduce proc table to %d\n",
		    Pn, Np);
		Exit(1);
	    }
	/*
	 * Reduce the Solaris PGID and PID tables to their minimum if
	 * not in repeat mode.
	 */
	    len = (MALLOC_S)(Np * sizeof(int));
	    if (Fpgid) {
		if (!(Pgid = (int *)realloc((MALLOC_P *)Pgid, len))) {
		    (void) fprintf(stderr,
			"%s: can't reduce PGID table to %d\n", Pn, Np);
		    Exit(1);
		}
	    }
	    if (!(Pid = (int *)realloc((MALLOC_P *)Pid, len))) {
		(void) fprintf(stderr,
		    "%s: can't reduce PID table to %d\n", Pn, Np);
		Exit(1);
	    }
	}
}


#if	defined(WILLDROPGID)
/*
 * restoregid() -- restore setgid permission, as required
 */

void
restoregid()
{
	if (Switchgid == 2 && !Setgid) {
	    if (setgid(Savedgid) != 0) {
		(void) fprintf(stderr,
		    "%s: can't set effective GID to %d: %s\n",
		    Pn, (int)Savedgid, strerror(errno));
		Exit(1);
	    }
	    Setgid = 1;
	}
}
#endif	/* defined(WILLDROPGID) */
