/*
 * dnode.c - OpenUNIX node functions for lsof
 */


/*
 * Copyright 2001 Purdue Research Foundation, West Lafayette, Indiana
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
"@(#) Copyright 2001 Purdue Research Foundation.\nAll rights reserved.\n";
static char *rcsid = "$Id: dnode.c,v 1.1 2001/10/04 12:40:44 abe Exp $";
#endif


#include "lsof.h"
#include <sys/fs/namenode.h>

#include <sys/fs/ufs_inode.h>


_PROTOTYPE(static void ent_fa,(KA_T *a1, KA_T *a2, char *d));
_PROTOTYPE(static int examine_stream,(KA_T vs, struct queue *q, char **mch,
	   char **mn, char *sn, KA_T *sqp));
_PROTOTYPE(static struct l_dev * findspdev,(dev_t *dev, dev_t *rdev));
_PROTOTYPE(static struct l_dev * findstrdev,(dev_t *dev, dev_t *rdev));
_PROTOTYPE(static void getspdev,(void));
_PROTOTYPE(static int get_vty,(struct vnode *v, KA_T va, struct vfs *kv,
	   int *fx));
_PROTOTYPE(static struct l_dev * ismouse,(struct vnode *va, struct l_ino *i,
	   int fx, struct vfs *kv));
_PROTOTYPE(static char isvlocked,(struct vnode *va));
_PROTOTYPE(static int readlino,(int fx, struct vnode *v, struct l_ino *i));


/*
 * Local variables and definitions
 */

static struct protos {
	char *module;			/* stream module name */
	char *proto;			/* TCP/IP protocol name */
} Protos[] = {
	{ "tcpu",	"TCP"	},
	{ "udpu",	"UDP"	},
	{ "tcpl",	"TCP"	},
	{ "tcp",	"TCP"	},
	{ "udpl",	"UDP"	},
	{ "udp",	"UDP"	},
};
#define	NPROTOS	(sizeof(Protos)/sizeof(struct protos))

static struct specdev {
    char *name;
    struct l_dev *dp;
} SpDev[] = {
    { "/dev/log",	(struct l_dev *)NULL },
    { "/dev/mouse",	(struct l_dev *)NULL },
};
#define	SPDEV_CT	(sizeof(SpDev) / sizeof(struct specdev))
static int SpDevX = -1;			/* SpDev[] maximum index */



/*
 * ent_fa() - enter fattach addresses in NAME column addition
 */

static void
ent_fa(a1, a2, d)
	KA_T *a1;			/* first fattach address (NULL OK) */
	KA_T *a2;			/* second fattach address */
	char *d;			/* direction ("->" or "<-") */
{
	char buf[64], *cp, tbuf[32];
	MALLOC_S len;

	if (Lf->nma)
	    return;
	if (!a1)
	    (void) snpf(buf, sizeof(buf), "(FA:%s%s)", d,
		print_kptr(*a2, (char *)NULL, 0));
	else
	    (void) snpf(buf, sizeof(buf), " (FA:%s%s%s)",
		print_kptr(*a1, tbuf, sizeof(tbuf)), d,
		print_kptr(*a2, (char *)NULL, 0));
	len = strlen(buf) + 1;
	if ((cp = (char *)malloc(len)) == NULL) {
	    (void) fprintf(stderr,
		"%s: no space for fattach addresses at PID %d, FD %s\n",
		Pn, Lp->pid, Lf->fd);
	    Exit(1);
	}
	(void) snpf(cp, len, "%s", buf);
	Lf->nma = cp;
}


/*
 * examine_stream() - examine stream
 */

static int
examine_stream(vs, q, mch, mn, sn, sqp)
	KA_T vs;			/* stream head's stdata kernel
					 * address */
	struct queue *q;		/* queue structure buffer */
	char **mch;			/* important stream module name chain,
					 * module names separated by "->" */
	char **mn;			/* stream's last module name in *mch */
	char *sn;			/* special module name */
	KA_T *sqp;			/* special module's q_ptr */
{
	static char *ab = (char *)NULL;
	static MALLOC_S aba = (size_t)0;
	MALLOC_S al, len, naba, tlen;
	char *ap;
	struct module_info mi;
	KA_T qp;
	struct qinit qi;
	struct stdata sd;
	char tbuf[32];
	char tmnb[STRNML+1];
/*
 * Read stream's head.
 */
	if (!vs || readstdata(vs, &sd)) {
	    (void) snpf(Namech, Namechl, "can't read stream head from %s",
		print_kptr(vs, (char *)NULL, 0));
	    enter_nm(Namech);
	    return(1);
	}
	if (!sd.sd_wrq) {
	    enter_nm("no stream write queue");
	    return(1);
	}
/*
 * Examine the write queue.
 */
	for (qp = (KA_T)sd.sd_wrq, al = (MALLOC_S)0, ap = ab,
		  *mn = (char *)NULL, tmnb[sizeof(tmnb) - 1] = '\0';
	     qp;
	     qp = (KA_T)q->q_next)
	{

	/*
	 * Read stream queue entry.
	 */
	    if (kread(qp, (char *)q, sizeof(struct queue))) {
		(void) snpf(Namech, Namechl, "can't read stream queue from %s",
		    print_kptr(qp, (char *)NULL, 0));
		enter_nm(Namech);
		return(1);
	    }
	/*
	 * Read queue's information structure.
	 */
	    if (!q->q_qinfo || readstqinit((KA_T)q->q_qinfo, &qi)) {
		(void) snpf(Namech, Namechl, "can't read qinit for %s from %s",
		    print_kptr(qp, tbuf, sizeof(tbuf)),
		    print_kptr((KA_T)q->q_qinfo, (char *)NULL, 0));
		enter_nm(Namech);
		return(1);
	    }
	/*
	 * Read module information structure.
	 */
	    if (!qi.qi_minfo || readstmin((KA_T)qi.qi_minfo, &mi)) {
		(void) snpf(Namech, Namechl,
		    "can't read module info for %s from %s",
		    print_kptr((KA_T)q->q_qinfo, tbuf, sizeof(tbuf)),
		    print_kptr((KA_T)qi.qi_minfo, (char *)NULL, 0));
		enter_nm(Namech);
		return(1);
	    }
	/*
	 * Read module name.
	 */
	    if (!mi.mi_idname || kread((KA_T)mi.mi_idname, tmnb, STRNML)) {
		(void) snpf(Namech, Namechl,
		    "can't read module name for %s from %s",
		    print_kptr((KA_T)qi.qi_minfo, tbuf, sizeof(tbuf)),
		    print_kptr((KA_T)mi.mi_idname, (char *)NULL, 0));
		enter_nm(Namech);
		return(1);
	    }
	/*
	 * Save the q_ptr of the first special module.
	 */
	    if (sn && !*sqp && q->q_ptr) {
		if (strcmp(tmnb, sn) == 0)
		    *sqp = (KA_T)q->q_ptr;
	    }
	/*
	 * Assemble the module name chain.  Allocate space as required.
	 * Skip null module names and some "uninteresting" ones.
	 */
	    len = strlen(tmnb);
	    if (len
	    &&  strcmp(tmnb, "strrhead")
	    &&  strcmp(tmnb, "strwhead")
	    ) {
		tlen = len + 1 + (al ? 2 : 0);
		if ((tlen + al) > aba) {
		    aba = tlen + al + 64;	/* allocate some extra */
		    if (!ab) {
			ab = ap = (char *)malloc(aba);
		    } else {
			ab = (char *)realloc((MALLOC_P *)ab, aba);
			if (al)
			    ap = ab + (al - 1);
			else
			    ap = ab;
		    }
		    if (!ab) {
			(void) fprintf(stderr,
			    "%s: no space for stream chain", Pn); 
			Exit(1);
		    }
		}
		(void) snpf(ap, aba - (al - 1), "%s%s",
		    (ap == ab) ? "" : "->", tmnb);
		*mn = ap + ((ap == ab) ? 0 : 2);
		al += tlen;
		ap += (tlen - 1);
	    }
	}
	*mch = ab;
	if (!*mn)
	    *mn = "";
	return(0);
}


/*
 * findspdev() - find special device by raw major device number
 */

static struct l_dev *
findspdev(dev, rdev)
	dev_t *dev;			/* containing device */
	dev_t *rdev;			/* raw device */
{
	int i;
	struct l_dev *dp;

	if (*dev != DevDev)
	    return((struct l_dev *)NULL);
	if (SpDevX < 0)
	    (void) getspdev();
	for (i = 0; i < SpDevX; i++) {
	    if (!(dp = SpDev[i].dp))
		continue;
	    if (GET_MAJ_DEV(*rdev) == GET_MAJ_DEV(dp->rdev))
		return(dp);
	}
	return((struct l_dev *)NULL);
}


/*
 * findstrdev() - look up stream device by device number
 */

static struct l_dev *
findstrdev(dev, rdev)
	dev_t *dev;			/* device */
	dev_t *rdev;			/* raw device */
{
	struct clone *c;
	struct l_dev *dp;
	int i;
/*
 * Search device table for match.
 */

#if	HASDCACHE

findstrdev_again:

#endif	/* HASDCACHE */

	if ((dp = lkupdev(dev, rdev, 0, 0)))
	    return(dp);
/*
 * Search for clone.
 */
	if (Clone) {
	    for (c = Clone; c; c = c->next) {
		if (GET_MAJ_DEV(*rdev) == GET_MIN_DEV(Devtp[c->dx].rdev)) {

#if	HASDCACHE
		    if (DCunsafe && !Devtp[c->dx].v && !vfy_dev(&Devtp[c->dx]))
			goto findstrdev_again;
#endif	/* HASDCACHE */

		    return(&Devtp[c->dx]);
		}
	    }
	}
/*
 * Search for non-clone clone.
 */
	return(findspdev(dev, rdev));
}


/*
 * getspecdev() -- get Devtp[] pointers for "special" devices
 */

static void
getspdev()
{
	struct l_dev *dp;
	int i, j, n;

	if (SpDevX >= 0)
	    return;
/*
 * Scan Devtp[] for the devices named in SpDev[].
 */
	for (i = n = 0; (i < Ndev) && (n < SPDEV_CT); i++) {
	    dp = Sdev[i];
	    for (j = 0; j < SPDEV_CT; j++) {
		if (SpDev[j].dp)
		    continue;
		if (strcmp(SpDev[j].name, dp->name) == 0) {
		    SpDev[j].dp = dp;
		    n++;
		    SpDevX = j + 1;
		    break;
		}
	    }
	}
	if (SpDevX < 0)
	    SpDevX = 0;
}


/*
 * get_vty() - get vnode type
 *
 * return: vnode type as an N_* symbol value
 *	   N_REGLR if no special file system type applies
 *	   -1 if the vnode type is VUNNAMED
 *	   -2 if the vfs structure has an illegal type index
 *	   -3 if the vfs structure can't be read
 */

static int
get_vty(v, va, kv, fx)
	struct vnode *v;		/* vnode to test */
	KA_T va;			/* vnode's kernel address */
	struct vfs *kv;			/* copy of vnode's kernel vfs struct */
	int *fx;			/* file system type index */
{
	int fxt;
	int nty = N_REGLR;
	char tbuf[32];

	if (v->v_type == VUNNAMED) {
	    *fx = 0;
	    return(-1);
	}
	if (!v->v_vfsp) {
	    *fx = 0;
	    if ((v->v_type == VFIFO) || v->v_stream)
		return(N_STREAM);
	    return(N_REGLR);
	}
	if (!kread((KA_T)v->v_vfsp, (char *)kv, sizeof(struct vfs))) {

	/*
	 * Check the file system type.
	 */
	    fxt = kv->vfs_fstype;
	    if (fxt > 0 && fxt <= Fsinfomax) {
		if (!strcmp(Fsinfo[fxt-1], "fifofs"))
		    nty = N_FIFO;
		else if (!strcmp(Fsinfo[fxt-1], "nfs"))
		    nty = N_NFS;
		else if (!strcmp(Fsinfo[fxt-1], "namefs"))
		    nty = N_NM;


#if	defined(HASPROCFS)
		else if (!strcmp(Fsinfo[fxt-1], "proc"))
		    nty = N_PROC;
#endif	/* defined(HASPROCFS) */

	    } else {
		(void) snpf(Namech, Namechl,
		    "vnode@%s: bad file system index (%d)",
		    print_kptr(va, (char *)NULL, 0), fxt);
		enter_nm(Namech);
		return(-2);
	    }
	} else {
	    (void) snpf(Namech, Namechl, "vnode@%s: bad vfs pointer (%s)",
		print_kptr(va, tbuf, sizeof(tbuf)),
		print_kptr((KA_T)v->v_vfsp, (char *)NULL, 0));
	    enter_nm(Namech);
	    return(-3);
	}
	if (nty == N_REGLR) {
	    if (v->v_type == VFIFO)
		nty = N_FIFO;
	    else if (v->v_stream)
		nty = N_STREAM;
	}
	*fx = fxt;
	return(nty);
}


/*
 * ismouse() - is vnode attached to /dev/mouse
 */

static struct l_dev *
ismouse(va, i, fx, kv)
	struct vnode *va;		/* local vnode address */
	struct l_ino *i;		/* local inode structure */
	int fx;				/* file system index */
	struct vfs *kv;			/* copy of kernel VFS structure */
{
	struct l_dev *dp;
	int j;

	if ((fx < 1) || (fx > Fsinfomax))
	    return((struct l_dev *)NULL);
	if ((dp = findspdev(&kv->vfs_dev, &va->v_rdev))) {
	    i->dev = kv->vfs_dev;
	    i->dev_def = 1;
	    i->nlink = (long)0;
	    i->nlink_def = 0;
	    i->nm = (char *)NULL;
	    i->number = dp->inode;
	    i->number_def = 1;
	    i->rdev = va->v_rdev;
	    i->rdev_def = 0;
	    i->size = (SZOFFTYPE)0;
	    i->size_def = 0;
	    Ntype = N_REGLR;
	}
	return(dp);
}


/*
 * isvlocked() - is a vnode locked
 */

static char
isvlocked(va)
	struct vnode *va;		/* local vnode address */
{
	struct filock f;
	KA_T flf, flp;
	int i, l;

	if (!(flf = (KA_T)va->v_filocks))
	    return(' ');
	flp = flf;
	i = 0;
	do {
	    if (i++ > 1000)
		break;
	    if (kread(flp, (char *)&f, sizeof(f)))
		break;
	    if (f.set.l_sysid || f.set.l_pid != (pid_t)Lp->pid)
		continue;
	    if (f.set.l_whence == 0 && f.set.l_start == 0
	    &&  (f.set.l_len == 0 || f.set.l_len == 0x7fffffff))
		l = 1;
	    else
		l = 0;
	    switch (f.set.l_type & (F_RDLCK | F_WRLCK)) {
	    case F_RDLCK:
		return((l) ? 'R' : 'r');
	    case F_WRLCK:
		return((l) ? 'W' : 'w');
	    case (F_RDLCK + F_WRLCK):
		return('u');
	    default:
		return(' ');
	    }
	} while (flp != (KA_T)f.next && (flp = (KA_T)f.next) && flp != flf);
	return(' ');
}


/*
 * process_node() - process node
 */

void
process_node(na)
	KA_T na;			/* vnode kernel space address */
{				   
	char *cp, *ep;
	dev_t dev, rdev;
	unsigned char devs = 0;
	unsigned char rdevs = 0;
	unsigned char ni = 0;
	struct l_dev *dp;
	struct fifonode f;
	int fx, rfx;
	struct l_ino i;
	int is = 1;
	int j, k;
	KA_T ka;
	struct vfs kv, rkv;
	char *mch, *mn;
	struct mnode mno;
	MALLOC_S msz;
	struct namenode nn;
	int px;
	struct queue q;
	struct rnode r;
	struct vnode rv, v;
	struct snode s;
	struct so_so so;
	KA_T sqp = (KA_T)NULL;
	size_t sz;
	char tbuf[32], *ty;
	enum vtype type;
	struct sockaddr_un ua;

#if	defined(HASPROCFS)
	struct as as;
	struct proc p;
	KA_T pa;
	struct procfsid *pfi;
	long pid;
	struct prnode pr;
	struct prcommon prc;
#endif	/* defined(HASPROCFS) */

/*
 * Read the vnode.
 */
	if (!na) {
	    enter_nm("no vnode address");
	    return;
	}
	if (readvnode((KA_T)na, &v)) {
            enter_nm(Namech);
            return;
        }

#if	defined(HASNCACHE)
	Lf->na = na;
#endif	/* defined(HASNCACHE) */

#if	defined(HASFSTRUCT)
	Lf->fna = na;
	Lf->fsv |= FSV_NI;
#endif	/* defined(HASFSTRUCT) */

/*
 * Determine the vnode type.
 */
	if ((Ntype = get_vty(&v, na, &kv, &fx)) < 0) {
	    if (Ntype == -1)
		Lf->sf = 0;
	    return;
	}
/*
 * Determine the lock state.
 */

get_lock_state:

	Lf->lock = isvlocked(&v);
/*
 * Read the fifonode, inode, namenode, prnode, rnode, snode, ...
 */
	switch (Ntype) {
	case N_FIFO:
	    if (!v.v_data || readfifonode((KA_T)v.v_data, &f)) {
		(void) snpf(Namech, Namechl,
		    "vnode@%s: can't read fifonode (%s)",
		    print_kptr(na, tbuf, sizeof(tbuf)),
		    print_kptr((KA_T)v.v_data, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	    if (f.fn_realvp) {
		if (readvnode((KA_T)f.fn_realvp, &rv)) {
		    (void) snpf(Namech, Namechl,
			"fifonode@%s: can't read real vnode (%s)",
			print_kptr((KA_T)v.v_data, tbuf, sizeof(tbuf)),
			print_kptr((KA_T)f.fn_realvp, (char *)NULL, 0));
		    enter_nm(Namech);
		    return;
		}

#if	defined(HASNCACHE)
		Lf->na = (KA_T)f.fn_realvp;
#endif	/* defined(HASNCACHE) */

		if (!rv.v_data || (is = readlino(fx, &rv, &i))) {
		    (void) snpf(Namech, Namechl,
			"fifonode@%s: can't read inode (%s)",
			print_kptr((KA_T)v.v_data, tbuf, sizeof(tbuf)),
			print_kptr((KA_T)rv.v_data, (char *)NULL, 0));
		    enter_nm(Namech);
		    return;
		}
	    } else
		ni = 1;
	    break;
	case N_NFS:
	    if (!v.v_data || readrnode((KA_T)v.v_data, &r)) {
		(void) snpf(Namech, Namechl, "vnode@%s: can't read rnode (%s)",
		    print_kptr(na, tbuf, sizeof(tbuf)),
		    print_kptr((KA_T)v.v_data, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	    break;
	case N_NM:
	    if (!v.v_data || kread((KA_T)v.v_data, (char *)&nn, sizeof(nn))) {
		(void) snpf(Namech, Namechl, "vnode@%s: no namenode (%s)",
		    print_kptr(na, tbuf, sizeof(tbuf)),
		    print_kptr((KA_T)v.v_data, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	    i.dev = nn.nm_vattr.va_fsid;
	    i.rdev = nn.nm_vattr.va_rdev;
	    i.number = nn.nm_vattr.va_nodeid;
	    i.size = nn.nm_vattr.va_size;
	    if (!nn.nm_mountpt)
		break;
	/*
	 * The name node is mounted over/to another vnode.  Process that node.
	 */
	    (void) ent_fa(&na, (KA_T *)&nn.nm_mountpt, "->");
	    if (kread((KA_T)nn.nm_mountpt, (char *)&rv, sizeof(rv))) {
		(void) snpf(Namech, Namechl,
		    "vnode@%s: can't read namenode's mounted vnode (%s)",
		    print_kptr(na, tbuf, sizeof(tbuf)),
		    print_kptr((KA_T)nn.nm_mountpt, (char *)NULL, 0));
		return;
	    }
	    if ((Ntype = get_vty(&rv, (KA_T)nn.nm_mountpt, &rkv, &rfx)) < 0) {
		if (Ntype == -1)
		    Lf->sf = 0;
		return;
	    }
	/*
	 * Unless the mounted-over/to node is another "namefs" node, promote
	 * it to the vnode of interest.
	 */
	    if (Ntype == N_NM)
		break;
	    fx = rfx;
	    kv = rkv;
	    v = rv;
	    goto get_lock_state;

#if	defined(HASPROCFS)
	case N_PROC:
	    ni = 1;
	    if (!v.v_data || kread((KA_T)v.v_data, (char *)&pr, sizeof(pr))) {
		(void) snpf(Namech, Namechl, "vnode@%s: can't read prnode (%s)",
		    print_kptr(na, tbuf, sizeof(tbuf)),
		    print_kptr((KA_T)v.v_data, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	    i.number = (long)pr.pr_ino;
	    if (pr.pr_common
	    &&  !kread((KA_T)pr.pr_common, (char *)&prc, sizeof(prc))) {
		pid = (long)prc.prc_pid;
		switch(pr.pr_type) {
		case PR_PIDDIR:
		    (void) snpf(Namech, Namechl, "/%s/%ld", HASPROCFS, pid);
		    break;
		case PR_AS:
		    (void) snpf(Namech, Namechl, "/%s/%ld/as", HASPROCFS, pid);
		    break;
		case PR_CTL:
		    (void) snpf(Namech, Namechl, "/%s/%ld/ctl", HASPROCFS, pid);
		    break;
		case PR_STATUS:
		    (void) snpf(Namech, Namechl, "/%s/%ld/status", HASPROCFS,
			pid);
		    break;
		case PR_MAP:
		    (void) snpf(Namech, Namechl, "/%s/%ld/map", HASPROCFS, pid);
		    break;
		case PR_CRED:
		    (void) snpf(Namech, Namechl, "/%s/%ld/cred", HASPROCFS, pid);
		    break;
		case PR_SIGACT:
		    (void) snpf(Namech, Namechl, "/%s/%ld/sigact", HASPROCFS,
			pid);
		    break;
		case PR_OBJECTDIR:
		    (void) snpf(Namech, Namechl, "/%s/%ld/object", HASPROCFS,
			pid);
		    break;
		case PR_LWPDIR:
		    (void) snpf(Namech, Namechl, "/%s/%ld/lwp", HASPROCFS, pid);
		    break;
		case PR_LWPIDDIR:
		    (void) snpf(Namech, Namechl, "/%s/%ld/lwp/%d",
			HASPROCFS, pid, prc.prc_lwpid);
		    break;
		case PR_LWPCTL:
		    (void) snpf(Namech, Namechl, "/%s/%ld/lwp/%d/lwpctl",
			HASPROCFS, pid, prc.prc_lwpid);
		    break;
		case PR_LWPSTATUS:
		    (void) snpf(Namech, Namechl, "/%s/%ld/lwp/%d/lwpstatus",
			HASPROCFS, pid, prc.prc_lwpid);
		    break;
		case PR_LWPSINFO:
		    (void) snpf(Namech, Namechl, "/%s/%ld/lwp/%d/lwpsinfo",
			HASPROCFS, pid, prc.prc_lwpid);
		    break;
		}
	    } else
		pid = 0l;
	    break;
#endif	/* defined(HASPROCFS) */

	case N_STREAM:
	    if (v.v_stream) {
		Lf->is_stream = ni = 1;
		if (process_unix_sockstr(&v, na)) {

		/*
		 * The stream is a UNIX socket stream.  No more need be done;
		 * process_unix_stream() has done it all.
		 */
		    return;
		}
	    /*
	     * Get the queue pointer and module name at the end of the stream.
	     * The module name identifies socket streams.
	     */
		if (examine_stream((KA_T)v.v_stream, &q, &mch, &mn, "sockmod",
				   &sqp))
		    return;
		for (px = 0; px < NPROTOS; px++) {
		    if (strcmp(mn, Protos[px].module) == 0) {
			process_socket(Protos[px].proto, &q);
			return;
		    }
		}
	    /*
	     * If this stream has a "sockmod" module with a non-NULL q_ptr,
	     * try to use it to read an so_so structure.
	     */
		if (sqp && kread(sqp, (char *)&so, sizeof(so)) == 0)
		    break;
		sqp = (KA_T)NULL;
		(void) snpf(Namech, Namechl, "STR");
		j = strlen(Namech);
		if (v.v_type == VCHR) {
	    /*
	     * If this is a VCHR stream, look up the device name and record it.
	     */
		    if ((dp = findstrdev(&DevDev, (dev_t *)&v.v_rdev))) {
			Lf->inode = (unsigned long)dp->inode;
			Lf->inp_ty = 1;
			Namech[j++] = ':';
			k = strlen(dp->name);
			if ((j + k) <= (Namechl - 1)) {
			    (void) snpf(&Namech[j], Namechl - j, "%s",
				dp->name);
			    j += k;
			    if ((cp = strrchr(Namech, '/'))
			    &&  *(cp + 1) == '\0')
			    {
				*cp = '\0';
				j--;
			    }
			}
		    }
		}
	    /*
	     * Follow the "STR" and possibly the device name with "->" and
	     * the stream's significant module names.
	     */
		if ((j + 2) <= (Namechl - 1)) {
		    (void) snpf(&Namech[j], Namechl - j, "->");
		    j += 2;
		}
		if (*mch) {
		    if ((j + strlen(mch)) <= (Namechl - 1))
			(void) snpf(&Namech[j], Namechl - j, mch);
		} else {
		    if ((j + strlen("none")) <= (Namechl - 1))
			(void) snpf(&Namech[j], Namechl - j, "none");
	        }
	    }
	    break;
	case N_REGLR:
	default:

	/*
	 * Follow a VCHR vnode to its snode, then to its real vnode, finally
	 * to its inode.
	 */
	    if (v.v_type == VCHR) {
		if (!v.v_data || readsnode((KA_T)v.v_data, &s)) {
		    (void) snpf(Namech, Namechl,
			"vnode@%s: can't read snode (%s)",
			print_kptr(na, tbuf, sizeof(tbuf)),
			print_kptr((KA_T)v.v_data, (char *)NULL, 0));
		    enter_nm(Namech);
		    return;
		}
		if (s.s_realvp) {
		    if (readvnode((KA_T)s.s_realvp, &rv)) {
			(void) snpf(Namech, Namechl,
			    "snode@%s: can't read real vnode (%s)",
			    print_kptr((KA_T)v.v_data, tbuf, sizeof(tbuf)),
			    print_kptr((KA_T)s.s_realvp, (char *)NULL, 0));
			enter_nm(Namech);
			return;
		    }
		    if (!rv.v_data || (is = readlino(fx, &rv, &i))) {
			(void) snpf(Namech, Namechl,
			    "snode@%s: unknown inode@%s; fx=",
			    print_kptr((KA_T)v.v_data, tbuf, sizeof(tbuf)),
			    print_kptr((KA_T)rv.v_data, (char *)NULL, 0));
			ep = endnm(&sz);
			if (fx < 1 || fx > Fsinfomax)
			    (void) snpf(ep, sz, "%d", fx);
			else
			    (void) snpf(ep, sz, "%s", Fsinfo[fx - 1]);
			enter_nm(Namech);
			return;
		    }
		}
	    /*
	     * If there's no real vnode, look for a common vnode and a
	     * common snode.
	     */
		else if ((ka = (KA_T)s.s_commonvp)) {
		    if (readvnode(ka, &rv)) {
			(void) snpf(Namech, Namechl,
			    "snode@%s: can't read common vnode (%s)",
			    print_kptr((KA_T)v.v_data, tbuf, sizeof(tbuf)),
			    print_kptr((KA_T)ka, (char *)NULL, 0));
			enter_nm(Namech);
			return;
		    }
		    if (!rv.v_vfsp) {
			if ((dp = ismouse(&rv, &i, fx, &kv))) {
			    (void) snpf(Namech, Namechl, "STR:%s", dp->name);
			    break;
			}
		    }
		    if (get_vty(&rv, ka, &rkv, &rfx) < 0)
		        Lf->is_com = ni = 1;
		    else {
			if ((is = readlino(rfx, &rv, &i))) {
			    (void) snpf(Namech, Namechl,
				"vnode@%s: unknown successor@%s; fx=",
				print_kptr((KA_T)ka, tbuf, sizeof(tbuf)),
				print_kptr((KA_T)v.v_data, (char *)NULL, 0));
			    ep = endnm(&sz);
			    if (rfx < 1 || rfx > Fsinfomax)
				(void) snpf(ep, sz, "%d", rfx);
			    else
				(void) snpf(ep, sz, "%s", Fsinfo[rfx - 1]);
			    enter_nm(Namech);
			    return;
			}
		    }
		} else
		    ni = 1;
		break;
	    } else if (v.v_type == VNON) {
		ni = 1;
		break;
	    }
	    if (v.v_data == NULL) {
		(void) snpf(Namech, Namechl, "vnode@%s: no further information",
		    print_kptr(na, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	/*
	 * Read inode information.
	 */
	    if ((is = readlino(fx, &v, &i))) {
		(void) snpf(Namech, Namechl,
		    "vnode@%s: unknown successor@%s; fx=",
		    print_kptr(na, tbuf, sizeof(tbuf)),
		    print_kptr((KA_T)v.v_data, (char *)NULL, 0));
		ep = endnm(&sz);
		if (fx < 1 || fx > Fsinfomax)
		    (void) snpf(ep, sz, "%d", fx);
		else
		    (void) snpf(ep, sz, "%s", Fsinfo[fx - 1]);
		enter_nm(Namech);
		return;
	    }
	}
/*
 * Get device and type for printing.
 */
	switch (Ntype) {
	case N_NFS:
	    dev = r.r_attr.va_fsid;
	    devs = 1;
	    break;

#if	defined(HASPROCFS)
	case N_PROC:
	    dev = kv.vfs_dev;
	    devs = 1;
	    break;
#endif	/* defined(HASPROCFS) */

	case N_STREAM:
	    if (sqp) {
		if (so.lux_dev.size >= 8) {
		    dev = DevDev;
		    rdev = so.lux_dev.addr.tu_addr.dev;
		    devs = rdevs = 1;
		} else
		    enter_dev_ch(print_kptr(sqp, (char *)NULL, 0));
		break;
	    }
	    if (v.v_type == VFIFO) {
		KA_T ta;

		if ((ta = (KA_T)(v.v_data ? v.v_data : v.v_stream)))
		    enter_dev_ch(print_kptr(ta, (char *)NULL, 0));
		break;
	    }
	    /* fall through */
	default:
	    if (!ni) {
		dev = i.dev;
		devs = i.dev_def;
	    } else if ((Ntype == N_STREAM)
		   &&  ((v.v_type == VCHR) || (v.v_type == VBLK)))
	    {
		dev = DevDev;
		devs = 1;
	    }
	    if ((v.v_type == VCHR) || (v.v_type == VBLK)) {
		rdev = v.v_rdev;
		rdevs = 1;
	    }
	}
	type = v.v_type;
/*
 * Obtain the inode number.
 */
	switch (Ntype) {

	case N_NFS:
	    Lf->inode = (unsigned long)r.r_attr.va_nodeid;
	    Lf->inp_ty = 1;
	    break;

#if	defined(HASPROCFS)
	case N_PROC:
	    Lf->inode = (unsigned long)i.number;
	    Lf->inp_ty = 1;
	    break;
#endif	/* defined(HASPROCFS) */

	case N_FIFO:
	    if (!f.fn_realvp) {
		enter_dev_ch(print_kptr((KA_T)v.v_data, (char *)NULL, 0));
		Lf->inode = (unsigned long)f.fn_ino;
		Lf->inp_ty = 1;
		if (f.fn_flag & ISPIPE)
		    (void) snpf(Namech, Namechl, "PIPE");
		if (f.fn_mate) {
		    ep = endnm(&sz);
		    (void) snpf(ep, sz, "->%s",
			print_kptr((KA_T)f.fn_mate, (char *)NULL, 0));
		}
		break;
	    }
	    /* fall through */
	case N_REGLR:
	    if (!ni) {
		Lf->inode = (unsigned long)i.number;
		Lf->inp_ty = i.number_def;
	    }
	    break;
	case N_STREAM:
	    if (sqp && so.lux_dev.size >= 8) {
		Lf->inode = (unsigned long)so.lux_dev.addr.tu_addr.ino;
		Lf->inp_ty = 1;
	    }
	}
/*
 * Obtain the file size.
 */
	if (Foffset)
	    Lf->off_def = 1;
	else {
	    switch (Ntype) {

	    case N_FIFO:
	    case N_STREAM:
		if (!Fsize)
		    Lf->off_def = 1;
		break;
	    case N_NFS:
		Lf->sz = (SZOFFTYPE)r.r_attr.va_size;
		Lf->sz_def = 1;
		break;

#if	defined(HASPROCFS)
	    case N_PROC:
		Lf->sz = (SZOFFTYPE)0;
		Lf->sz_def = 0;
		break;
#endif	/* defined(HASPROCFS) */

	    case N_REGLR:
		if (type == VREG || type == VDIR) {
		    if (!ni) {
			Lf->sz = (SZOFFTYPE)i.size;
			Lf->sz_def = i.size_def;
		    }
		} else if (type == VCHR && !Fsize)
		    Lf->off_def = 1;
		break;
	    }
	}
/*
 * Record link count.
 */
	if (Fnlink) {
	    switch(Ntype) {
	    case N_FIFO:
		Lf->nlink = (long)f.fn_open;
		Lf->nlink_def = 1;
		break;
	    case N_NFS:
		Lf->nlink = (long)r.r_attr.va_nlink;
		Lf->nlink_def = 1;
		break;

#if	defined(HASPROCFS)
	    case N_PROC:
#endif	/* defined(HASPROCFS) */

	    case N_REGLR:
		if (!ni) {
		    Lf->nlink = (long)i.nlink;
		    Lf->nlink_def = i.nlink_def;
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
 * Defer file system info lookup until printname().
 */
	Lf->lmi_srch = 1;
/*
 * Save the device numbers and their states.
 *
 * Format the vnode type, and possibly the device name.
 */
	if (type != VFIFO) {
	    Lf->dev = dev;
	    Lf->dev_def = devs;
	    Lf->rdev = rdev;
	    Lf->rdev_def = rdevs;
	}
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
	    if (Lf->is_stream == 0)
		Ntype = N_CHR;
	    break;
	case VLNK:
	    ty = "VLNK";
	    break;

#if	defined(VSOCK)
	case VSOCK:
	    ty = "SOCK";
	    break;
#endif	/* VSOCK */

	case VBAD:
	    ty = "VBAD";
	    break;
	case VFIFO:
	    if (!Lf->dev_ch || Lf->dev_ch[0] == '\0') {
		Lf->dev = dev;
		Lf->dev_def = devs;
		Lf->rdev = rdev;
		Lf->rdev_def = rdevs;
	    }
	    ty = "FIFO";
	    break;
	case VUNNAMED:
	    ty = "UNNM";
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
 * If this is a VBLK file and it's missing an inode number, try to
 * supply one.
 */
	if ((Lf->inp_ty == 0) && (Lf->ntype == N_BLK))
	    find_bl_ino();
/*
 * If this is a VCHR file and it's missing an inode number, try to
 * supply one.
 */
	if ((Lf->inp_ty == 0) && (type == VCHR))
	    find_ch_ino();
/*
 * If this is a stream with a "sockmod" module whose q_ptr leads to an
 * so_so structure, assume it's a UNIX domain socket and try to get
 * the path.  Clear the is_stream status.
 */
	if (Ntype == N_STREAM && sqp) {
	    if (Funix)
		Lf->sf |= SELUNX;
	    (void) snpf(Lf->type, sizeof(Lf->type), "unix");
	    if (!Namech[0]
	    &&  so.laddr.buf && so.laddr.len == sizeof(ua)
	    &&  !kread((KA_T)so.laddr.buf, (char *)&ua, sizeof(ua))) {
		ua.sun_path[sizeof(ua.sun_path) - 1] = '\0';
		(void) snpf(Namech, Namechl, "%s", ua.sun_path);
		if (Sfile && is_file_named(Namech, 0))
		    Lf->sf = SELNM;
		if (so.lux_dev.size >= 8) {
		    Lf->inode = (unsigned long)so.lux_dev.addr.tu_addr.ino;
		    Lf->inp_ty = 1;
		}
	    }
	    if (so.so_conn) {
		ep = endnm(&sz);
		(void) snpf(ep, sz, "->%s",
		    print_kptr((KA_T)so.so_conn, (char *)NULL, 0));
	    }
	    Lf->is_stream = 0;
	}
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

		    if ((pfi->pid && pid && pfi->pid == (pid_t)pid)

# if	defined(HASPINODEN)
		    || (Lf->inp_ty == 1 && Lf->inode == pfi->inode)
# endif	/* defined(HASPINODEN) */

		    ) {
			pfi->f = 1;
			Lf->sf |= SELNM;
			if (!Namech[0] && pfi->nm) {
			    (void) strncpy(Namech, pfi->nm, Namechl - 1);
			    Namech[Namechl-1] = '\0';
			}
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
 * Enter name characters.  If there's an l_ino structure with a file name
 * pointer, and no name column addition exists, make what the l_ino file
 * name pointer addresses a name column addition.
 */
	if (!Lf->nma && !is && i.nm) {
	    if ((msz = (MALLOC_S)strlen(i.nm))) {
		if (!(cp = (char *)malloc(msz + 1))) {
		    (void) fprintf(stderr,
			"%s: can't allocate %d bytes for l_ino name addition\n",
			msz, Pn);
		    Exit(1);
		}
		(void) snpf(cp, msz + 1, "%s", i.nm);
		Lf->nma = cp;
	    }
	}
	if (Namech[0])
	    enter_nm(Namech);
}


/*
 * readlino() - read local inode information
 */

static int
readlino(fx, v, i)
	int fx;				/* file system index */
	struct vnode *v;		/* vnode pointing to inode */
	struct l_ino *i;		/* local inode */
{
	struct vnode fa;
	struct mnode mn;
	struct inode sn;

	i->nlink_def = 0;
	if (fx < 1 || fx > Fsinfomax || !v->v_data)
	    return(1);
	if (!strcmp(Fsinfo[fx-1], "fifofs")
	||  !strcmp(Fsinfo[fx-1], "sfs")
	||  !strcmp(Fsinfo[fx-1], "ufs")) {
	    if (kread((KA_T)v->v_data, (char *)&sn, sizeof(sn)))
		return(1);
	    i->dev = sn.i_dev;
	    i->dev_def = 1;
	    i->rdev = v->v_rdev;
	    i->rdev_def = 1;
	    i->nlink = (long)sn.i_nlink;
	    i->nlink_def = 1;
	    i->nm = (char *)NULL;
	    i->number = sn.i_number;
	    i->number_def = 1;
	    i->size = (SZOFFTYPE)sn.i_size;
	    i->size_def = 1;
	    return(0);
	}
	else if (!strcmp(Fsinfo[fx-1], "s5"))
	    return(reads5lino(v, i));
 	else if (!strcmp(Fsinfo[fx-1], "vxfs"))
	    return(readvxfslino(v, i));
 	else if (!strcmp(Fsinfo[fx-1], "bfs"))
	    return(readbfslino(v, i));
	else if (!strcmp(Fsinfo[fx-1], "memfs")) {
	    if (kread((KA_T)v->v_data, (char *)&mn, sizeof(mn)))
		return(1);
	    i->dev = mn.mno_fsid;
	    i->dev_def = 1;
	    i->nlink = (long)mn.mno_nlink;
	    i->nlink_def = 1;
	    i->nm = (char *)NULL;
	    i->number = mn.mno_nodeid;
	    i->number_def = 1;
	    i->rdev = mn.mno_rdev;
	    i->rdev_def = 1;
	    i->size = (SZOFFTYPE)mn.mno_size;
	    i->size_def = 1;
	    return(0);
	}
	else if (!strcmp(Fsinfo[fx-1], "cdfs"))
	    return readcdfslino(v, i);
	else if (!strcmp(Fsinfo[fx-1], "dosfs"))
	    return readdosfslino(v, i);
	return(1);
}
