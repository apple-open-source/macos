/*
 * dnode.c - Solaris node reading functions for lsof
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
static char *rcsid = "$Id: dnode.c,v 1.42 2001/10/17 19:19:19 abe Exp $";
#endif


#include "lsof.h"

#undef	fs_bsize
#include <sys/fs/ufs_inode.h>


_PROTOTYPE(static struct l_dev *finddev,(dev_t *dev, dev_t *rdev, int flags));


/*
 * Finddev() "look-in " flags
 */

#define	LOOKDEV_TAB	0x01		/* look in device table */
#define	LOOKDEV_CLONE	0x02		/* look in Clone table */
#define	LOOKDEV_PSEUDO	0x04		/* look in Pseudo table */
#define	LOOKDEV_ALL	(LOOKDEV_TAB | LOOKDEV_CLONE | LOOKDEV_PSEUDO)
					/* look all places */


_PROTOTYPE(static char isvlocked,(struct vnode *va));
_PROTOTYPE(static int readinode,(KA_T ia, struct inode *i));
_PROTOTYPE(static void read_mi,(KA_T s, dev_t *dev, caddr_t so, int *so_st, KA_T *so_ad, struct l_dev **sdp));

#if	solaris>=20500
# if	solaris>=20600
_PROTOTYPE(static int read_nan,(KA_T na, KA_T aa, struct fnnode *rn));
_PROTOTYPE(static int read_nson,(KA_T na, KA_T sa, struct sonode *sn));
_PROTOTYPE(static int read_nusa,(struct soaddr *so, struct sockaddr_un *ua));
# else	/* solaris<20600 */
_PROTOTYPE(static int read_nan,(KA_T na, KA_T aa, struct autonode *a));
# endif	/* solaris>=20600 */
_PROTOTYPE(static int idoorkeep,(struct door_node *d));
_PROTOTYPE(static int read_ndn,(KA_T na, KA_T da, struct door_node *d));
#endif	/* solaris>=20500 */

_PROTOTYPE(static int read_nfn,(KA_T na, KA_T fa, struct fifonode *f));
_PROTOTYPE(static int read_nhn,(KA_T na, KA_T ha, struct hsnode *h));
_PROTOTYPE(static int read_nin,(KA_T na, KA_T ia, struct inode *i));
_PROTOTYPE(static int read_nmn,(KA_T na, KA_T ia, struct mvfsnode *m));
_PROTOTYPE(static int read_npn,(KA_T na, KA_T pa, struct pcnode *p));
_PROTOTYPE(static int read_nrn,(KA_T na, KA_T ra, struct rnode *r));
_PROTOTYPE(static int read_nsn,(KA_T na, KA_T sa, struct snode *s));
_PROTOTYPE(static int read_ntn,(KA_T na, KA_T ta, struct tmpnode *t));
_PROTOTYPE(static int read_nvn,(KA_T na, KA_T va, struct vnode *v));

#if	defined(HASPROCFS)
_PROTOTYPE(static int read_npi,(KA_T na, struct vnode *v, struct pid *pids));
#endif	/* defined(HASPROCFS) */

_PROTOTYPE(static char *ent_fa,(KA_T *a1, KA_T *a2, char *d, int *len));
_PROTOTYPE(static int is_socket,(struct vnode *v));
_PROTOTYPE(static int read_cni,(struct snode *s, struct vnode *rv,
	struct vnode *v, struct snode *rs, struct dev_info *di, char *din,
	int dinl));
_PROTOTYPE(static int read_ncn,(KA_T na, KA_T ca, struct cnode *cn));
_PROTOTYPE(static int read_nln,(KA_T na, KA_T la, struct lnode *ln));
_PROTOTYPE(static int read_nnn,(KA_T na, KA_T nna, struct namenode *n));
_PROTOTYPE(static void savesockmod,(struct so_so *so, struct so_so *sop, int *so_st));


/*
 * Local static values
 */

static KA_T Auvops;		/* auto_vnodeops address */
static KA_T Avops;		/* [_]afs_vnodeops address */
static KA_T Cvops;		/* cache_vnodeops address */
static KA_T Dvops;		/* door_vnodeops address */
static KA_T Fvops;		/* [_]fifo_vnodeops address */
static KA_T Hvops;		/* [_]hsfs_vnodeops address */
static KA_T Lvops;		/* lo_vnodeops address */
static KA_T Mntops;		/* mntvnodeops address */
static KA_T Mvops;		/* [_]mvfs_vnodeops address */
static KA_T N3vops;		/* [_]nfs3_vnodeops address */
static KA_T Nmvops;		/* nm_vnodeops address */
static KA_T Nvops;		/* [_]nfs_vnodeops address */
static KA_T Pdvops;		/* [_]pcfs_dvnodeops address */
static KA_T Pfvops;		/* [_]pcfs_fvnodeops address */
static KA_T Prvops;		/* prvnodeops address */
static KA_T Sckvops;		/* [_]sock_vnodeops address */
static KA_T Spvops;		/* [_]spec_vnodeops address */
static KA_T Tvops;		/* [_]tmp_vnodeops address */
static KA_T Uvops;		/* [_]ufs_vnodeops address */
static KA_T Vvops[VXVOP_NUM];	/* addresses of:
				 *   [_]fdd_chain_vnops (VXVOP_FDDCH),
				 *   [_]fdd_vnops (VXVOP_FDD), and
				 *   [_]vx_vnodeops (VXVOP_REG) */


/*
 * ent_fa() - enter fattach addresses in NAME column addition
 */

static char *
ent_fa(a1, a2, d, len)
	KA_T *a1;			/* first fattach address (NULL OK) */
	KA_T *a2;			/* second fattach address */
	char *d;			/* direction ("->" or "<-") */
	int *len;			/* returned description length */
{
	static char buf[1024];
	size_t bufl = sizeof(buf);
	MALLOC_S l, nl;
	char tbuf[32];
/*
 * Form the fattach description.
 */
	if (!a1)

#if	solaris<20600
	    (void) snpf(buf, bufl, "(FA:%s%s)", d,
		print_kptr(*a2, (char *)NULL, 0));
#else	/* solaris>=20600 */
	    (void) snpf(buf, bufl, "(FA:%s%s)", d,
		print_kptr(*a2, (char *)NULL, 0));
#endif	/* solaris<20600 */

	else

#if	solaris<20600
	    (void) snpf(buf, bufl, "(FA:%s%s%s)",
		print_kptr(*a1, tbuf, sizeof(tbuf)), d,
		print_kptr(*a2, (char *)NULL, 0));
#else	/* solaris>=20600 */
	    (void) snpf(buf, bufl, "(FA:%s%s%s)",
		print_kptr(*a1, tbuf, sizeof(tbuf)), d,
		print_kptr(*a2, (char *)NULL, 0));
#endif	/* solaris<20600 */

	*len = (int) strlen(buf);
	return(buf);
}


/*
 * is_socket() - is the stream a socket?
 */

static int
is_socket(v)
	struct vnode *v;		/* vnode pointer */
{
	char *cp, *ep, *pf;
	struct l_dev *dp;
	int i, j, len, n, pfl;
	static struct tcpudp {
	    int ds;
	    dev_t rdev;
	    char *proto;
	} tcpudp[] = {
	    { 0, 0, "tcp" },
	    { 0, 0, "udp" },

#if	defined(HASIPv6)
	    { 0, 0, "tcp6" },
	    { 0, 0, "udp6" },
#endif	/* defined(HASIPv6) */

	};
#define	NTCPUDP	(sizeof(tcpudp) / sizeof(struct tcpudp))

	static int tcpudps = 0;

	if (!v->v_stream)
	    return(0);
/*
 * Fill in tcpudp[], as required.
 */
	if (!tcpudps) {

#if	solaris<80000
	    pf = "/devices/pseudo/clone";
#else	/* solaris>=80000 */
	    pf = "/devices/pseudo/";
#endif	/* solaris<80000 */

	    for (i = n = 0, pfl = strlen(pf); (i < Ndev) && (n < NTCPUDP); i++)
	    {
		if (strncmp(Devtp[i].name, pf, pfl)
		||  !(ep = strrchr((cp = &Devtp[i].name[pfl]), ':'))
		||  (strncmp(++ep, "tcp", 3) && strncmp(ep, "udp", 3)))
		    continue;

#if	solaris<80000
		if (*(ep + 3))
		    continue;
		for (j = 0; j < NTCPUDP; j++) {
		    if (!tcpudp[j].ds && !strcmp(ep, tcpudp[j].proto)) {
			tcpudp[j].ds = 1;
			tcpudp[j].rdev = Devtp[i].rdev;
			n++;
			break;
		    }
		}
#else	/* solaris>=80000 */
		len = (*(ep + 3) == '6') ? 4 : 3;
		if (*(ep + len) || ((cp + len) >= ep) || strncmp(cp, ep, len))
		    continue;
		for (j = 0; j < NTCPUDP; j++) {
		    if (!tcpudp[j].ds && !strcmp(ep, tcpudp[j].proto)) {
			tcpudp[j].ds = 1;
			tcpudp[j].rdev = Devtp[i].rdev;
			n++;
			break;
		    }
		}
#endif	/* solaris<80000 */

	    }
	    tcpudps = n ? 1 : -1;
	}
/*
 * Check for known IPv[46] TCP or UDP device.
 */
	for (i = 0; (i < NTCPUDP) && (tcpudps > 0); i++) {

#if	solaris<80000
	    if (!tcpudp[i].ds
	    ||  (GET_MAJ_DEV(v->v_rdev) != GET_MIN_DEV(tcpudp[i].rdev)))
		continue;
#else	/* solaris>=80000 */
	    if (!tcpudp[i].ds
	    ||  (GET_MAJ_DEV(v->v_rdev) != GET_MAJ_DEV(tcpudp[i].rdev)))
		continue;
#endif	/* solaris<80000 */

	    process_socket((KA_T)v->v_stream, tcpudp[i].proto);
	    return(1);
	}
	return(0);
}


/*
 * isvlocked() - is Solaris vnode locked?
 */

static char
isvlocked(va)
	struct vnode *va;		/* local vnode address */
{

#if	solaris<20500
	struct filock f;
	KA_T ff;
	KA_T fp;
#endif	/* solaris<20500 */

	int l;

#if	solaris>=20300
	struct lock_descriptor ld;
	KA_T lf;
	KA_T lp;
# if	solaris<20500
#define	LOCK_END	ld.info.li_sleep.sli_flock.l_len
#define	LOCK_FLAGS	ld.flags
#define	LOCK_NEXT	ld.next
#define	LOCK_OWNER	ld.owner.pid
#define	LOCK_START	ld.start
#define	LOCK_TYPE	ld.type
# else	/* solaris>=20500 */
#define	LOCK_END	ld.l_flock.l_len
#define	LOCK_FLAGS	ld.l_state
#define	LOCK_NEXT	ld.l_next
#define	LOCK_OWNER	ld.l_flock.l_pid
#define	LOCK_START	ld.l_start
#define	LOCK_TYPE	ld.l_type
# endif	/* solaris<20500 */
#endif	/* solaris>=20300 */

	if (va->v_filocks == NULL)
		return(' ');

#if	solaris<20500
# if	solaris>20300 || (solaris==20300 && defined(P101318) && P101318>=45)
	if (Ntype == N_NFS)
# endif	/* solaris>20300 || (solaris==20300 && defined(P101318) && P101318>=45) */

	{
	    ff = fp = (KA_T)va->v_filocks;
	    do {
		if (kread(fp, (char *)&f, sizeof(f)))
		    return(' ');
		if (f.set.l_pid != (pid_t)Lp->pid)
		    continue;
		if (f.set.l_whence == 0 && f.set.l_start == 0
		&&  f.set.l_len == MAXEND)
		    l = 1;
		else
		    l = 0;
		switch (f.set.l_type & (F_RDLCK | F_WRLCK)) {
		case F_RDLCK:
		    return(l ? 'R' : 'r');
		case F_WRLCK:
		    return(l ? 'W' : 'w');
		case F_RDLCK|F_WRLCK:
		    return('u');
		default:
		    return('N');
		}
	    } while ((fp = (KA_T)f.next) && fp != ff);
	}
#endif	/* solaris<20500 */

#if	solaris>=20300
	lf = lp = (KA_T)va->v_filocks;
	do {
	    if (kread(lp, (char *)&ld, sizeof(ld)))
		return(' ');
	    if (!(LOCK_FLAGS & ACTIVE_LOCK) || LOCK_OWNER != (pid_t)Lp->pid)
		continue;
	    if (LOCK_START == 0
	    &&  (LOCK_END == 0

# if	solaris<20500
	    ||   LOCK_END == MAXEND
# else	/* solaris>=20500 */
	    ||   LOCK_END == MAXEND
# endif	/* solaris<20500 */

	    ))
		l = 1;
	    else
		l = 0;
	    switch (LOCK_TYPE) {
	    case F_RDLCK:
		return(l ? 'R' : 'r');
	    case F_WRLCK:
		return(l ? 'W' : 'w');
	    case (F_RDLCK | F_WRLCK):
		return('u');
	    default:
		return('L');
	    }
	} while ((lp = (KA_T)LOCK_NEXT) && lp != lf);
	return(' ');
#endif	/* solaris>=20300 */

}


/*
 * finddev() - look up device by device number
 */

static struct l_dev *
finddev(dev, rdev, flags)
	dev_t *dev;			/* device */
	dev_t *rdev;			/* raw device */
	int flags;			/* look flags -- see LOOKDEV_* symbol
					 * definitions */
{
	struct clone *c;
	struct l_dev *dp;
	struct pseudo *p;

	if (!Sdev)
	    readdev(0);
/*
 * Search device table for match.
 */

#if	defined(HASDCACHE)

finddev_again:

#endif	/* defined(HASDCACHE) */

	if (flags & LOOKDEV_TAB) {
	    if ((dp = lkupdev(dev, rdev, 0, 0)))
		return(dp);
	}
/*
 * Search for clone.
 */
	if ((flags & LOOKDEV_CLONE) && Clone) {
	    for (c = Clone; c; c = c->next) {
		if (GET_MAJ_DEV(*rdev) == GET_MIN_DEV(c->cd.rdev)) {

#if	defined(HASDCACHE)
		    if (DCunsafe && !c->cd.v && !vfy_dev(&c->cd))
			goto finddev_again;
#endif	/* defined(HASDCACHE) */

		    return(&c->cd);
		}
	    }
	}
/*
 * Search for pseudo device match on major device only.
 */
	if ((flags & LOOKDEV_PSEUDO) && Pseudo) {
	    for (p = Pseudo; p; p = p->next) {
		if (GET_MAJ_DEV(*rdev) == GET_MAJ_DEV(p->pd.rdev)) {

#if	defined(HASDCACHE)
		    if (DCunsafe && !p->pd.v && !vfy_dev(&p->pd))
			goto finddev_again;
#endif	/* defined(HASDCACHE) */

		    return(&p->pd);
		}
	    }
	}
	return((struct l_dev *)NULL);
}


#if	solaris>=20500
/*
 * idoorkeep() -- identify door keeper process
 */

static int
idoorkeep(d)
	struct door_node *d;		/* door's node */
{
	char buf[1024], *cp;
	size_t bufl = sizeof(buf);
	MALLOC_S l, nl;
	struct proc dp;
	struct pid dpid;
/*
 * Get the proc structure and its pid structure for the door target.
 */
	if (!d->door_target
	||  kread((KA_T)d->door_target, (char *)&dp, sizeof(dp)))
	    return(0);
	if (!dp.p_pidp
	||  kread((KA_T)dp.p_pidp, (char *)&dpid, sizeof(dpid)))
	    return(0);
/*
 * Form a description of the door.
 *
 * Put the description in the NAME column addition field.  If there's already
 * something there, allocate more space and add the door description to it.
 */
	if (Lp->pid == (int)dpid.pid_id)
	    (void) snpf(buf, bufl, "(this PID's door)");
	else {
	    (void) snpf(buf, bufl, "(door to %.64s[%ld])", dp.p_user.u_comm,
		(long)dpid.pid_id);
	}
	(void) add_nma(buf, (int)strlen(buf));
	return(1);
}
#endif	/* solaris>=20500 */


/*
 * process_node() - process vnode
 */

void
process_node(va)
	KA_T va;			/* vnode kernel space address */
{
	struct cnode cn;
	dev_t dev, rdev, trdev;
	unsigned char devs = 0;
	unsigned char fxs = 0;
	unsigned char ins = 0;
	unsigned char kvs = 0;
	unsigned char nns = 0;
	unsigned char pnl = 0;
	unsigned char rdevs = 0;
	unsigned char rvs = 0;
	unsigned char tdef;
	unsigned char trdevs = 0;
	unsigned char unix_sock = 0;
	struct dev_info di;
	char din[DINAMEL];
	char *ep;
	struct fifonode f;
	char *fa = (char *)NULL;
	int fal;
	struct vfs favfs;
	static int ft = 1;
	struct vnode fv, rv;
	int fx;
	struct hsnode h;
	struct inode i;
	struct lnode lo;
	struct vfs kv;
	int len, nl, snl, sepl;
	struct mvfsnode m;
	struct mounts *mp;
	struct namenode nn;
	struct l_vfs *nvfs, *vfs;
	struct pcnode pc;
	struct pcfs pcfs;
	struct rnode r;
	KA_T realvp = (KA_T)NULL;
	struct snode rs;
	struct snode s;
	struct l_dev *sdp = (struct l_dev *)NULL;
	KA_T so_ad[2];
	struct so_so soso;
	int so_st = 0;
	size_t sz;
	struct tmpnode t;
	char tbuf[128], *ty, ubuf[128];
	int tbufx;
	enum vtype type;
	struct sockaddr_un ua;
	static struct vnode *v = (struct vnode *)NULL;
	KA_T vs;
	int vty = 0;
	int  vty_tmp;

#if	solaris>=20500
# if	solaris>=20600
	struct fnnode fnn;
	struct pairaddr {
	    short f;
	    unsigned short p;
	} *pa;
	KA_T peer;
	struct sonode so;
	KA_T soa;
# else	/* solaris<20600 */
	struct autonode au;
# endif	/* solaris>=20600 */

	struct door_node dn;
	int dns = 0;
#endif	/* solaris >=20500 */

#if	defined(HASPROCFS)
	struct procfsid *pfi;
	struct pid pids;
#endif	/* defined(HASPROCFS) */

#if	defined(HAS_AFS)
	struct afsnode an;
#endif	/* defined(HAS_AFS) */

#if	defined(HASVXFS)
	struct l_ino vx;
#endif	/* defined(HASVXFS) */

/*
 * Do first-time only operations.
 */
	so_ad[0] = so_ad[1] = (KA_T)0;
	if (ft) {
	    if (get_Nl_value("auvops", Drive_Nl, &Auvops) < 0)
		Auvops = (KA_T)0;
	    if (get_Nl_value("avops", Drive_Nl, &Avops) < 0 || !Avops) {
		if (get_Nl_value("Avops", Drive_Nl, &Avops) < 0)
		    Avops = (KA_T)0;
	    }
	    if (get_Nl_value("cvops", Drive_Nl, &Cvops) < 0)
		Cvops = (KA_T)0;
	    if (get_Nl_value("dvops", Drive_Nl, &Dvops) < 0)
		Dvops = (KA_T)0;
	    if (get_Nl_value("fvops", Drive_Nl, &Fvops) < 0)
		Fvops = (KA_T)0;
	    if (get_Nl_value("hvops", Drive_Nl, &Hvops) < 0)
		Hvops = (KA_T)0;
	    if (get_Nl_value("lvops", Drive_Nl, &Lvops) < 0)
		Lvops = (KA_T)0;
	    if (get_Nl_value("mntops", Drive_Nl, &Mntops) < 0)
		Mntops = (KA_T)0;
	    if (get_Nl_value("mvops", Drive_Nl, &Mvops) < 0)
		Mvops = (KA_T)0;
	    if (get_Nl_value("n3vops", Drive_Nl, &N3vops) < 0)
		N3vops = (KA_T)0;
	    if (get_Nl_value("nmvops", Drive_Nl, &Nmvops) < 0)
		Nmvops = (KA_T)0;
	    if (get_Nl_value("nvops", Drive_Nl, &Nvops) < 0)
		Nvops = (KA_T)0;
	    if (get_Nl_value("pdvops", Drive_Nl, &Pdvops) < 0)
		Pdvops = (KA_T)0;
	    if (get_Nl_value("pfvops", Drive_Nl, &Pfvops) < 0)
		Pfvops = (KA_T)0;
	    if (get_Nl_value("prvops", Drive_Nl, &Prvops) < 0)
		Prvops = (KA_T)0;
	    if (get_Nl_value("sckvops", Drive_Nl, &Sckvops) < 0)
		Sckvops = (KA_T)0;
	    if (get_Nl_value("spvops", Drive_Nl, &Spvops) < 0)
		Spvops = (KA_T)0;
	    if (get_Nl_value("tvops", Drive_Nl, &Tvops) < 0)
		Tvops = (KA_T)0;
	    if (get_Nl_value("uvops", Drive_Nl, &Uvops) < 0)
		Uvops = (KA_T)0;
	    if (VXVOP_FDD < VXVOP_NUM) {
		if (get_Nl_value("vvfops", Drive_Nl, &Vvops[VXVOP_FDD]) < 0)
		    Vvops[VXVOP_FDD] = (KA_T)0;
	    }
	    if (VXVOP_FDDCH < VXVOP_NUM) {
		if (get_Nl_value("vvfcops", Drive_Nl, &Vvops[VXVOP_FDDCH]) < 0)
		    Vvops[VXVOP_FDDCH] = (KA_T)0;
	    }
	    if (VXVOP_REG < VXVOP_NUM) {
		if (get_Nl_value("vvops", Drive_Nl, &Vvops[VXVOP_REG]) < 0)
		    Vvops[VXVOP_REG] = (KA_T)0;
	    }
	    ft = 0;
	}
/*
 * Read the vnode.
 */
	if (!va) {
	    enter_nm("no vnode address");
	    return;
	}
	if (!v) {

	/*
	 * Allocate space for the vnode or AFS vcache structure.
	 */

#if	defined(HAS_AFS)
	    v = alloc_vcache();
#else	/* !defined(HAS_AFS) */
	    v = (struct vnode *) malloc(sizeof(struct vnode));
#endif	/* defined(HAS_AFS) */

	    if (!v) {
		(void) fprintf(stderr, "%s: can't allocate %s space\n", Pn,

#if	defined(HAS_AFS)
			       "vcache"
#else	/* !defined(HAS_AFS) */
			       "vnode"
#endif	/* defined(HAS_AFS) */

			      );
		Exit(1);
	    }
	}
	if (readvnode(va, v)) {
	    enter_nm(Namech);
	    return;
	}

#if	defined(HASNCACHE)
	Lf->na = va;
#endif	/* defined(HASNCACHE) */

#if	defined(HASFSTRUCT)
	Lf->fna = va;
	Lf->fsv |= FSV_NI;
#endif	/* defined(HASFSTRUCT) */

	vs = (KA_T)v->v_stream;
/*
 * Check for a Solaris socket.
 */
	if (is_socket(v))
	    return;
/*
 * Obtain the Solaris virtual file system structure.
 */
	if (v->v_vfsp) {
	    if (kread((KA_T)v->v_vfsp, (char *)&kv, sizeof(kv))) {

vfs_read_error:

		(void) snpf(Namech, Namechl, "vnode at %s: can't read vfs: %s",
		    print_kptr(va, tbuf, sizeof(tbuf)),
		    print_kptr((KA_T)v->v_vfsp, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	    kvs = 1;
	} else
	    kvs = 0;
/*
 * Derive the virtual file system structure's device number from
 * its file system ID for NFS and High Sierra file systems.
 */
	if (kvs && ((fx = kv.vfs_fstype - 1) >= 0) && (fx < Fsinfomax)) {
	    fxs = 1;
	    if (strcmp(Fsinfo[fx], "nfs") == 0
	    ||  strcmp(Fsinfo[fx], "nfs3") == 0
	    ||  strcmp(Fsinfo[fx], "hsfs") == 0)
		kv.vfs_dev = (dev_t)kv.vfs_fsid.val[0];
	}
/*
 * Determine the Solaris vnode type.
 */
	if ((Ntype = vop2ty(v)) < 0) {
	    if (v->v_type == VFIFO) {
		vty = N_REGLR;
		Ntype = N_FIFO;
	    } else if (vs) {
		Ntype = vty = N_STREAM;
		Lf->is_stream = 1;
	    }
	    if (Ntype < 0) {
		(void) snpf(Namech, Namechl,
		    "unknown file system type%s%s%s, v_op: %s",
		    fxs ? " (" : "",
		    fxs ? Fsinfo[fx] : "",
		    fxs ? ")" : "",
		    print_kptr((KA_T)v->v_op, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	} else {
	    vty = Ntype;
	    if (v->v_type == VFIFO)
		Ntype = N_FIFO;
	    else if (vs && Ntype != N_SOCK) {
		Ntype = vty = N_STREAM;
		Lf->is_stream = 1;
	    }
	}
/*
 * See if this Solaris node has been fattach'ed to another node.
 * If it has, read the namenode, and enter the node addresses in
 * the NAME column addition.
 *
 * See if it's covering a socket as well and process accordingly.
 */
	if (vty == N_NM) {
	    if (read_nnn(va, (KA_T)v->v_data, &nn))
		return;
	    nns = 1;
	    if (nn.nm_mountpt)

#if	solaris>=20500
		fa = ent_fa((KA_T *)((Ntype == N_FIFO || v->v_type == VDOOR)
			    ? NULL : &va),
			    (KA_T *)&nn.nm_mountpt, "->", &fal);
#else	/* solaris<20500 */
		fa = ent_fa((KA_T *)((Ntype == N_FIFO)
			    ? NULL : &va),
			    (KA_T *)&nn.nm_mountpt, "->", &fal);
#endif	/* solaris>=20500 */

	    if (Ntype != N_FIFO
	    &&  nn.nm_filevp
	    &&  !kread((KA_T)nn.nm_filevp, (char *)&rv, sizeof(rv))) {
		rvs = 1;

#if	defined(HASNCACHE)
		Lf->na = (KA_T)nn.nm_filevp;
#endif	/* defined(HASNCACHE) */

		if (is_socket(&rv))
		    return;
	    }
	}
	if (Selinet && Ntype != N_SOCK)
	    return;
/*
 * See if this Solaris node is served by spec_vnodeops.
 */
	if (Spvops && Spvops == (KA_T)v->v_op) 
	    Ntype = N_SPEC;

/*
 * Determine the Solaris lock state.
 */
	Lf->lock = isvlocked(v);
/*
 * Establish the Solaris local virtual file system structure.
 */
	if (!v->v_vfsp || !kvs)
	    vfs = (struct l_vfs *)NULL;
	else if (!(vfs = readvfs((KA_T)v->v_vfsp, &kv, v)))
	    goto vfs_read_error;
/*
 * Read the afsnode, autonode, cnode, door_node, fifonode, fnnode, lnode,
 * inode, pcnode, rnode, snode, tmpnode, etc.
 */
	switch (Ntype) {
	case N_SPEC:
	
	/*
	 * A N_SPEC node is a node that resides in in an underlying file system
	 * type -- e.g. NFS, HSFS.  Its vnode points to an snode.  Subsequent
	 * node structures are implied by the underlying node type.
	 */
	    if (read_nsn(va, (KA_T)v->v_data, &s))
		return;
	    realvp = (KA_T)s.s_realvp;
	    if (!realvp && s.s_commonvp) {
		if (read_cni(&s, &rv, v, &rs, &di, din, sizeof(din)) == 1)
		    return;
		if (!rv.v_stream) {
		    if (din[0]) {
			(void) snpf(Namech, Namechl, "COMMON: %s", din);
			Lf->is_com = 1;
		    }
		    break;
		}
	    }
	    if (!realvp) {

	    /*
	     * If the snode lacks a real vnode (and also lacks a common vnode),
	     * it's original type is N_STREAM or N_REGLR, and it has a stream
	     * pointer, get the module names.
	     */
		if ((vty == N_STREAM || vty == N_REGLR) && vs) {
		    Lf->is_stream = 1;
		    vty = N_STREAM;
		    read_mi(vs, (dev_t *)&s.s_dev, (caddr_t)&soso, &so_st,
			    so_ad, &sdp);
		    vs = (KA_T)NULL;
		}
	    }
	    break;

#if	defined(HAS_AFS)
	case N_AFS:
	    if (readafsnode(va, v, &an))
		return;
	    break;
#endif	/* defined(HAS_AFS) */

#if	solaris>=20500
	case N_AUTO:

# if	solaris<20600
	    if (read_nan(va, (KA_T)v->v_data, &au))
# else	/* solaris>=20600 */
	    if (read_nan(va, (KA_T)v->v_data, &fnn))
# endif	/* solaris<20600 */

		return;
	    break;
	case N_DOOR:
	    if (read_ndn(va, (KA_T)v->v_data, &dn))
		return;
	    dns = 1;
	    break;
#endif	/* solaris>=20500 */

	case N_CACHE:
	    if (read_ncn(va, (KA_T)v->v_data, &cn))
		return;
	    break;

#if	solaris>=20600
	case N_SOCK:
	    if (read_nson(va, (KA_T)v->v_data, &so))
		return;
	    break;
#endif	/* solaris>=20600 */

	case N_MNT:
	    /* Information comes from the l_vfs structure. */
	    break;
	case N_MVFS:
	    if (read_nmn(va, (KA_T)v->v_data, &m))
		return;
	    break;
	case N_NFS:
	    if (read_nrn(va, (KA_T)v->v_data, &r))
		return;
	    break;
	case N_NM:
	    if (nns)
		realvp = (KA_T)nn.nm_filevp;

#if	defined(HASNCACHE)
		Lf->na = (KA_T)nn.nm_filevp;
#endif	/* defined(HASNCACHE) */

	    break;
	case N_FIFO:

	/*
	 * Solaris FIFO vnodes are usually linked to a fifonode.  One
	 * exception is a FIFO vnode served by nm_vnodeops; it is linked
	 * to a namenode, and the namenode points to the fifonode.
	 *
	 * Non-pipe fifonodes are linked to a vnode thorough fn_realvp.
	 */
	    if (vty == N_NM && nns) {
		if (nn.nm_filevp) {
		    if (read_nfn(va, (KA_T)nn.nm_filevp, &f))
			return;
		    realvp = (KA_T)NULL;
		    vty = N_FIFO;
		} else {
		    (void) snpf(Namech, Namechl,
			"FIFO namenode at %s: no fifonode pointer",
			print_kptr((KA_T)v->v_data, (char *)NULL, 0));
		    return;
		}
	    } else {
		if (read_nfn(va, (KA_T)v->v_data, &f))
		    return;
		realvp = (KA_T)f.fn_realvp;
	    }
	    if (!realvp) {
		Lf->inode = (unsigned long)(nns ? nn.nm_vattr.va_nodeid
						: f.fn_ino);

#if	solaris>=80000	/* Solaris 8 and above hack! */
# if	defined(_LP64)
		if (Lf->inode >= (unsigned long)0xbaddcafebaddcafe)
# else	/* !defined(_LP64) */
		if (Lf->inode >= (unsigned long)0xbaddcafe)
# endif	/* defined(_LP64) */

		    Lf->inp_ty = 0;
		else
#endif	/* solaris>=80000 Solaris 8 and above hack! */

		    Lf->inp_ty = 1;
		enter_dev_ch(print_kptr((KA_T)v->v_data, (char *)NULL, 0));
		if (f.fn_flag & ISPIPE) {
		    (void) snpf(tbuf, sizeof(tbuf), "PIPE");
		    tbufx = (int) strlen(tbuf);
		} else
		    tbufx = 0;

#if	solaris<20500
		if (f.fn_mate) {
		    (void) snpf(&tbuf[tbufx], sizeof(tbuf) - tbufx, "->%s",
			print_kptr((KA_T)f.fn_mate, (char *)NULL, 0));
		    tbufx = (int) strlen(tbuf);
		}
#else	/* solaris>=20500 */
		if (f.fn_dest) {
		    (void) snpf(&tbuf[tbufx], sizeof(tbuf) - tbufx, "->%s",
			print_kptr((KA_T)f.fn_dest, (char *)NULL, 0));
		    tbufx = (int) strlen(tbuf);
		}
#endif	/* solaris<20500 */

		if (tbufx)
		    (void) add_nma(tbuf, tbufx);
		break;
	    }
	    break;

	case N_HSFS:
	    if (read_nhn(va, (KA_T)v->v_data, &h))
		return;
	    break;
	case N_LOFS:
	    do {
		if (read_nln(va, (KA_T)v->v_data, &lo))
		    return;
		if (!(realvp = (KA_T)lo.lo_vp)) {
		    (void) snpf(Namech, Namechl, "lnode at %s: no real vnode",
			print_kptr((KA_T)v->v_data, (char *)NULL, 0));
		    return;
		}
		if (read_nvn((KA_T)v->v_data, (KA_T)realvp, v))
		    return;
		vty_tmp = vop2ty(v);
	    } while (vty_tmp == N_LOFS);
	    break;
	case N_PCFS:
	    if (read_npn(va, (KA_T)v->v_data, &pc))
		return;
	    break;

#if	defined(HASPROCFS)
	case N_PROC:
	    if (read_npi(va, v, &pids))
		return;
	    break;
#endif	/* defined(HASPROCFS) */

	case N_TMP:
	    if (read_ntn(va, (KA_T)v->v_data, &t))
		return;
	    break;
	case N_STREAM:
	    if (read_nsn(va, (KA_T)v->v_data, &s))
		return;
	    if (vs) {
		Lf->is_stream = 1;
		read_mi(vs, (dev_t *)&s.s_dev, (caddr_t)&soso, &so_st, so_ad,
		    &sdp);
		vs = (KA_T)NULL;
	    }
	    break;

#if	defined(HASVXFS)
	case N_VXFS:
	    if (read_vxnode(va, v, vfs, &vx, Vvops))
		return;
	    break;
#endif	/* defined(HASVXFS) */

	case N_REGLR:
	default:
	    if (v->v_type != VBLK) {
		if (read_nin(va, (KA_T)v->v_data, &i))
		    return;
		ins = 1;
	    }
	}
/*
 * If the node has a real vnode pointer, follow it.
 */
	if (realvp) {
	    if (rvs)
		*v = rv;
	    else if (read_nvn((KA_T)v->v_data, (KA_T)realvp, v))
		return;

#if	defined(HASNCACHE)
	    else
		Lf->na = (KA_T)realvp;
#endif	/* defined(HASNCACHE) */

	/*
	 * If the original vnode type is N_STREAM, and if there is
	 * a stream pointer, get the module names.
	 */
	    if (vty == N_STREAM && vs) {
		Lf->is_stream = 1;
		read_mi(vs, (dev_t *)&s.s_dev, (caddr_t)&soso, &so_st, so_ad,
		    &sdp);
		vs = (KA_T)NULL;
	    }
	/*
	 * Get the real vnode's type.
	 */
	    if ((vty = vop2ty(v)) < 0) {
		if (Ntype != N_FIFO && vs)
		    vty = N_STREAM;
		else {
		    (void) snpf(Namech, Namechl,
			"unknown file system type, v_op: %s",
			print_kptr((KA_T)v->v_op, (char *)NULL, 0));
		    enter_nm(Namech);
		    return;
		}
	    }
	    if (Ntype == N_NM || Ntype == N_AFS)
		Ntype = vty;
	/*
	 * Base further processing on the "real" vnode.
	 */
	    switch (vty) {

#if	defined(HAS_AFS)
	    case N_AFS:
		if (readafsnode(va, v, &an))
		    return;
		break;
#endif	/* defined(HAS_AFS) */
	
#if	solaris>=20500
	    case N_AUTO:

# if	solaris<20600
		if (read_nan(va, (KA_T)v->v_data, &au))
# else	/* solaris>=20600 */
		if (read_nan(va, (KA_T)v->v_data, &fnn))
# endif	/* solaris<20600 */

		    return;
		break;
	    case N_DOOR:

# if	solaris<20600
		if (read_ndn(realvp, (KA_T)v->v_data, &dn))
# else	/* solaris>=20600 */
		if (read_ndn(va, (KA_T)v->v_data, &dn))
# endif	/* solaris<20500 */

		    return;
		dns = 1;
		break;
#endif	/* solaris>=20500 */

	    case N_CACHE:
		if (read_ncn(va, (KA_T)v->v_data, &cn))
		    return;
		break;

#if	solaris>=20600
	    case N_SOCK:
		if (read_nson(va, (KA_T)v->v_data, &so))
		    return;
		break;
#endif	/* solaris>=20600 */

	    case N_HSFS:
		if (read_nhn(va, (KA_T)v->v_data, &h))
		    return;
		break;
	    case N_MNT:
		/* Information comes from the l_vfs structure. */
		break;
	    case N_MVFS:
		if (read_nmn(va, (KA_T)v->v_data, &m))
		    return;
		break;
	    case N_NFS:
		if (read_nrn(va, (KA_T)v->v_data, &r))
		    return;
		break;
	    case N_NM:
		if (read_nnn(va, (KA_T)v->v_data, &nn))
		    return;
		nns = 1;
		break;
	    case N_PCFS:
		if (read_npn(va, (KA_T)v->v_data, &pc))
		    return;
		break;
	    case N_STREAM:
		if (vs) {
		    Lf->is_stream = 1;
		    read_mi(vs, (dev_t *)&s.s_dev, (caddr_t)&soso, &so_st,
			    so_ad, &sdp);
		    vs = (KA_T)NULL;
		}
		break;
	    case N_TMP:
		if (read_ntn(va, (KA_T)v->v_data, &t))
		    return;
		break;

#if	defined(HASVXFS)
	    case N_VXFS:
		if (read_vxnode(va, v, vfs, &vx, Vvops))
		    return;
		break;
#endif	/* defined(HASVXFS) */

	    case N_REGLR:
	    default:
		if (v->v_type != VBLK) {
		    if (read_nin(va, (KA_T)v->v_data, &i))
			return;
		    ins = 1;
		}
	    }
	/*
	 * If this is a Solaris loopback node, use the "real" node type.
	 */
	    if (Ntype == N_LOFS)
		Ntype = vty;
	}
/*
 * Get device and type for printing.
 */
	switch (Ntype == N_FIFO ? vty : Ntype) {

#if	defined(HAS_AFS)
	case N_AFS:
	    dev = an.dev;
	    devs = 1;
	    break;
#endif	/* defined(HAS_AFS) */

#if	solaris>=20500
	case N_AUTO:
	    if (kvs) {
		dev = (dev_t)kv.vfs_fsid.val[0];
		devs = 1;
	    }
	    break;
	case N_DOOR:

# if	solaris<20600
	    if (kvs) {
		dev = (dev_t)kv.vfs_fsid.val[0];
		devs = 1;
	    }
# else	/* solaris>=20600 */
	    if (nns) {
		dev = (dev_t)nn.nm_vattr.va_fsid;
		devs = 1;
	    } else if (dns) {
		dev = (dev_t)dn.door_index;
		devs = 1;
	    }
# endif	/* solaris<20600 */

	    break;
#endif	/* solaris>=20500 */

	case N_CACHE:
	case N_HSFS:
	case N_PCFS:
	    if (kvs) {
		dev = kv.vfs_dev;
		devs = 1;
	    }
	    break;
#if	solaris>=20600
	case N_SOCK:
	    if (so.so_family == AF_UNIX) {

	    /*
	     * If this is an AF_UNIX socket node:
	     *
	     *    Enter the sonode address as the device (netstat's local
	     *	  address);
	     *    Get a non-NULL local sockaddr_un and enter it in Namech;
	     *    Get a non-NULL foreign sockaddr_un and enter it in Namech;
	     *    Check for matches on sockaddr_un.sun_path names.
	     */
		if (!sdp)
		    sdp = finddev(&DevDev, &so.so_vnode.v_rdev, LOOKDEV_ALL);
		if (sdp) {
		    dev = DevDev;
		    rdev = so.so_vnode.v_rdev;
		    trdev = sdp->rdev;
		    devs = rdevs = trdevs = 1;
		    Lf->inode = (unsigned long)sdp->inode;
		    Lf->inp_ty = 1;
		    (void) snpf(Namech, Namechl, "%s", sdp->name);
		} else
		    devs = 0;
		nl = snl = strlen(Namech);
		if ((len = read_nusa(&so.so_laddr, &ua))) {
		    if (Sfile
		    &&  is_file_named(ua.sun_path, Ntype, VSOCK, 0))
			Lf->sf |= SELNM;
		    sepl = Namech[0] ? 2 : 0;
		    if (len > (Namechl - nl - sepl - 1))
			len = Namechl - nl - sepl - 1;
		    if (len > 0) {
			ua.sun_path[len] = '\0';
			(void) snpf(&Namech[nl], Namechl - nl, "%s%s",
			    sepl ? "->" : "", ua.sun_path);
			nl += (len + sepl);
		    }
		}
		if ((len = read_nusa(&so.so_faddr, &ua))) {
		    if (Sfile
		    &&  is_file_named(ua.sun_path, Ntype, VSOCK, 0))
			Lf->sf |= SELNM;
		    sepl = Namech[0] ? 2 : 0;
		    if (len > (Namechl - nl - sepl - 1))
			len = Namechl - nl - sepl - 1;
		    if (len > 0) {
			ua.sun_path[len] = 0;
			(void) snpf(&Namech[nl], Namechl - nl, "%s%s",
			    sepl ? "->" : "", ua.sun_path);
			nl += (len + sepl);
		    }
		}
		if (nl == snl
		&&  so.so_ux_laddr.sou_magic == SOU_MAGIC_IMPLICIT) {

		/*
		 * There are no addresses; this must be a socket pair.
		 * Print its identity.
		 */
		    pa = (struct pairaddr *)&ua;
		    if (!(peer = (KA_T)((int)pa->p)))
			peer = (KA_T)so.so_ux_laddr.sou_vp;
		    if (peer)
			(void) snpf(ubuf, sizeof(ubuf), "(socketpair: %s)",
			    print_kptr(peer, (char *)NULL, 0));
		    else
			(void) snpf(ubuf, sizeof(ubuf), "(socketpair)");
		    len = strlen(ubuf);
		    sepl = Namech[0] ? 2 : 0;
		    if (len > (Namechl - nl - sepl - 1))
			len = Namechl - nl - sepl - 1;
		    if (len > 0) {
			(void) snpf(&Namech[nl], Namechl - nl, "%s%s",
			    sepl ? "->" : "", ubuf);
			nl += (len + sepl);
		    }
		}
	    /*
	     * Add the local and foreign addresses, ala `netstat -f unix` to
	     * the name.
	     */
		soa = (KA_T)so.so_ux_faddr.sou_vp;
		(void) snpf(ubuf, sizeof(ubuf), "%s(%s%s%s)",
		    Namech[0] ? " " : "",
		    print_kptr((KA_T)v->v_data, (char *)NULL, 0),
		    soa ? "->" : "",
		    soa ? print_kptr(soa, tbuf, sizeof(tbuf)) : "");
		len = strlen(ubuf);
		if (len <= (Namechl - nl - 1)) {
		    (void) snpf(&Namech[nl], Namechl - nl, "%s", ubuf);
		    nl += len;
		}
	    /*
	     * If there is a bound vnode, add its address to the name.
	     */
		if (so.so_ux_bound_vp) {
		    (void) snpf(ubuf, sizeof(ubuf), "%s(Vnode=%s)",
			Namech[0] ? " " : "",
			print_kptr((KA_T)so.so_ux_bound_vp, (char *)NULL, 0));
		    len = strlen(ubuf);
		    if (len <= (Namechl - nl - 1)) {
			(void) snpf(&Namech[nl], Namechl - nl, "%s", ubuf);
			nl += len;
		    }
		}
	    }
	    break;
#endif	/* solaris>=20600 */

	case N_MNT:

#if	defined(CVFS_DEVSAVE)
	    if (vfs) {
		dev = vfs->dev;
		devs = 1;
	    }
#endif	/* defined(CVFS_DEVSAVE) */

	    break;
	case N_MVFS:

#if	defined(CVFS_DEVSAVE)
	    if (vfs) {
		dev = vfs->dev;
		devs = 1;
	    }
#endif	/* defined(CVFS_DEVSAVE) */

	    break;
	case N_NFS:
	    dev = r.r_attr.va_fsid;
	    devs = 1;
	    break;
	case N_NM:
	    if (nns) {
		dev = (dev_t)nn.nm_vattr.va_fsid;
		devs = 1;
	    } else
		enter_dev_ch("    NMFS");
	    break;

#if	defined(HASPROCFS)
	case N_PROC:
	    if (kvs) {
		dev = kv.vfs_dev;
		devs = 1;
	    }
	    break;
#endif	/* defined(HASPROCFS) */

	case N_SPEC:
	    if (((Ntype = vty) == N_STREAM) && so_st) {
		if (Funix)
		    Lf->sf |= SELUNX;
		unix_sock = 1;
		if (so_ad[0]) {
		    if (sdp) {
			if (vfs) {
			    dev = vfs->dev;
			    devs = 1;
			}
			rdev = sdp->rdev;
			rdevs = 1;
			Lf->inode = (unsigned long)sdp->inode;
			Lf->inp_ty = 1;
			(void) snpf(ubuf, sizeof(ubuf), "(%s%s%s)",
			    print_kptr(so_ad[0], (char *)NULL, 0),
			    so_ad[1] ? "->" : "",
			    so_ad[1] ? print_kptr(so_ad[1], tbuf, sizeof(tbuf))
				     : "");
		    } else {
			enter_dev_ch(print_kptr(so_ad[0], (char *)NULL, 0));
			if (so_ad[1])
			    (void) snpf(ubuf, sizeof(ubuf), "(->%s)",
				print_kptr(so_ad[1], (char *)NULL, 0));
		    }
		    if (!Lf->nma && (Lf->nma = (char *) malloc(strlen(ubuf)+1)))
			(void) snpf(Lf->nma, strlen(ubuf) + 1, "%s", ubuf);
		} else if (soso.lux_dev.addr.tu_addr.ino) {
		    if (vfs) {
			dev = vfs->dev;
			devs = 1;
		    }
		    rdev = soso.lux_dev.addr.tu_addr.dev;
		    rdevs = 1;
		} else {
		    int dc, dl, dr;

#if	solaris<20400
		    dl = (soso.lux_dev.addr.tu_addr.dev >> 16) & 0xffff;
		    dr = (soso.rux_dev.addr.tu_addr.dev >> 16) & 0xffff;
#else	/* solaris>=20400 */
		    dl = soso.lux_dev.addr.tu_addr.dev & 0xffff;
		    dr = soso.rux_dev.addr.tu_addr.dev & 0xffff;
#endif	/* solaris<20400 */

		    dc = (dl << 16) | dr;
		    enter_dev_ch(print_kptr((KA_T)dc, (char *)NULL, 0));
		    devs = 0;
		}
		if (soso.laddr.buf && soso.laddr.len == sizeof(ua)) {
		    if (kread((KA_T)soso.laddr.buf, (char *)&ua, sizeof(ua))
		    == 0) {
			ua.sun_path[sizeof(ua.sun_path) - 1] = '\0';
			if (ua.sun_path[0]) {
			    if (Sfile
			    &&  is_file_named(ua.sun_path, Ntype, type, 0))
				Lf->sf |= SELNM;
			    len = strlen(ua.sun_path);
			    nl = strlen(Namech);
			    sepl = Namech[0] ? 2 : 0;
			    if (len > (Namechl - nl - sepl - 1))
				len = Namechl - nl - sepl - 1;
			    if (len > 0) {
				ua.sun_path[len] = '\0';
				(void) snpf(&Namech[nl], Namechl - nl, "%s%s",
				    sepl ? "->" : "", ua.sun_path);
			    }
			}
		    }
		}
	    } else {
		if (vfs) {
		    dev = vfs->dev;
		    devs = 1;
		}
		rdev = s.s_dev;
		rdevs = 1;
	    }
	    break;
	case N_STREAM:
	    if (vfs) {
		dev = vfs->dev;
		devs = 1;
	    }
	    rdev = s.s_dev;
	    rdevs = 1;
	    break;
	case N_TMP:
	    dev = t.tn_attr.va_fsid;
	    devs = 1;
	    break;

#if	defined(HASVXFS)
	case N_VXFS:
	    dev = vx.dev;
	    devs = vx.dev_def;
	    if ((v->v_type == VCHR) || (v->v_type == VBLK)) {
		rdev = vx.rdev;
		rdevs = vx.rdev_def;
	    }
	    break;
#endif	/* defined(HASVXFS) */

	default:
	    if (ins) {
		dev = i.i_dev;
		devs = 1;
	    } else if (nns) {
		dev = nn.nm_vattr.va_fsid;
		devs = 1;
	    } else if (vfs) {
		dev = vfs->dev;
		devs = 1;
	    }
	    if ((v->v_type == VCHR) || (v->v_type == VBLK)) {
		rdev = v->v_rdev;
		rdevs = 1;
	    }
	}
	type = v->v_type;
	if (devs && vfs && !vfs->dir) {
	    (void) completevfs(vfs, &dev);

#if	defined(HAS_AFS)
	    if (vfs->dir && (Ntype == N_AFS || vty == N_AFS) && !AFSVfsp)
		AFSVfsp = (KA_T)v->v_vfsp;
#endif	/* defined(HAS_AFS) */

	}
/*
 * Obtain the inode number.
 */
	switch (vty) {

#if	defined(HAS_AFS)
	case N_AFS:
	    if (an.ino_st) {
		Lf->inode = an.inode;
		Lf->inp_ty = 1;
	    }
	    break;
#endif	/* defined(HAS_AFS) */

#if	solaris>=20500
	case N_AUTO:

# if	solaris<20600
	    Lf->inode = (unsigned long)au.an_nodeid;
# else	/* solaris>=20600 */
	    Lf->inode = (unsigned long)fnn.fn_nodeid;
# endif	/* solaris<20600 */

	    Lf->inp_ty = 1;
	    break;
	case N_DOOR:
	    if (nns && (Lf->inode = (unsigned long)nn.nm_vattr.va_nodeid)) {
		Lf->inp_ty = 1;
		break;
	    }
	    if (dns) {
		if ((Lf->inode = (unsigned long)dn.door_index)) 
		    Lf->inp_ty = 1;
	    }
	    break;
#endif	/* solaris>=20500 */

	case N_CACHE:
	    Lf->inode = (unsigned long)cn.c_fileno;
	    Lf->inp_ty = 1;
	    break;
	case N_HSFS:

#if	defined(HAS_HS_NODEID)
	    Lf->inode = (unsigned long)h.hs_nodeid;
#else	/* defined(HAS_HS_NODEID) */
	    Lf->inode = (unsigned long)h.hs_dirent.ext_lbn;
#endif	/* defined(HAS_HS_NODEID) */

	    Lf->inp_ty = 1;
	    break;

	case N_MNT:

#if	defined(HASFSINO)
	    if (vfs) {
		Lf->inode = vfs->fs_ino;
		Lf->inp_ty = 1;
	    }
#endif	/* defined(HASFSINO) */

	    break;
	case N_MVFS:
	    Lf->inode = (unsigned long)m.m_ino;
	    Lf->inp_ty = 1;
	    break;
	case N_NFS:
	    Lf->inode = (unsigned long)r.r_attr.va_nodeid;
	    Lf->inp_ty = 1;
	    break;
	case N_NM:
	    Lf->inode = (unsigned long)nn.nm_vattr.va_nodeid;
	    Lf->inp_ty = 1;
	    break;

#if	defined(HASPROCFS)
	case N_PROC:

	/*
	 * The proc file system inode number is defined when the
	 * prnode is read.
	 */
	    break;
#endif	/* defined(HASPROCFS) */

	case N_PCFS:
	    if (kvs && kv.vfs_data
	    && !kread((KA_T)kv.vfs_data, (char *)&pcfs, sizeof(pcfs))) {

#if	solaris>=70000
		Lf->inode = (unsigned long)pc_makenodeid(pc.pc_eblkno,
			    pc.pc_eoffset,
			    pc.pc_entry.pcd_attr,
			    IS_FAT32(&pcfs)
				? ltohs(pc.pc_entry.pcd_scluster_lo) |
				  (ltohs(pc.pc_entry.un.pcd_scluster_hi) << 16)
				: ltohs(pc.pc_entry.pcd_scluster_lo),
			    pcfs.pcfs_entps);
#else	/* solaris<70000 */
		Lf->inode = (unsigned long)pc_makenodeid(pc.pc_eblkno,
			    pc.pc_eoffset,
			    &pc.pc_entry,
			    pcfs.pcfs_entps);
#endif	/* solaris>=70000 */

		Lf->inp_ty = 1;
	    }
	    break;

	case N_REGLR:
	    if (nns) {
		if ((Lf->inode = (unsigned long)nn.nm_vattr.va_nodeid))
		    Lf->inp_ty = 1;
	    } else if (ins) {
		if ((Lf->inode = (unsigned long)i.i_number))
		    Lf->inp_ty = 1;
	    }
	    break;
	case N_STREAM:
	    if (so_st && soso.lux_dev.addr.tu_addr.ino) {
		if (Lf->inp_ty) {
		    nl = Lf->nma ? strlen(Lf->nma) : 0;
		    (void) snpf(ubuf, sizeof(ubuf),
			"%s(Inode=%lu)", nl ? " " : "",
			(unsigned long)soso.lux_dev.addr.tu_addr.ino);
		    len = nl + strlen(ubuf) + 1;
		    if (Lf->nma)
			Lf->nma = (char *) realloc(Lf->nma, len);
		    else
			Lf->nma = (char *) malloc(len);
		    if (Lf->nma)
			(void) snpf(&Lf->nma[nl], len - nl, "%s", ubuf);
		} else {
		    Lf->inode = (unsigned long)soso.lux_dev.addr.tu_addr.ino;
		    Lf->inp_ty = 1;
		}
	    }
	    break;
	case N_TMP:
	    Lf->inode = (unsigned long)t.tn_attr.va_nodeid;
	    Lf->inp_ty = 1;
	    break;

#if	defined(HASVXFS)
	case N_VXFS:
	    if (vx.ino_def) {
		Lf->inode = vx.ino;
		Lf->inp_ty = 1;
	    } else if (type == VCHR)
		pnl = 1;
	    break;
#endif	/* defined(HASVXFS) */

	}
/*
 * Obtain the file size.
 */
	if (Foffset)
	    Lf->off_def = 1;
	else {
	    switch (Ntype) {

#if	defined(HAS_AFS)
	    case N_AFS:
		Lf->sz = (SZOFFTYPE)an.size;
		Lf->sz_def = 1;
		break;
#endif	/* defined(HAS_AFS) */

#if	solaris>=20500
	    case N_AUTO:

# if	solaris<20600
		Lf->sz = (SZOFFTYPE)au.an_size;
# else	/* solaris >=20600 */
		Lf->sz = (SZOFFTYPE)fnn.fn_size;
# endif	/* solaris < 20600 */

		Lf->sz_def = 1;
		break;
#endif	/* solaris>=20500 */

	    case N_CACHE:
		Lf->sz = (SZOFFTYPE)cn.c_size;
		Lf->sz_def = 1;
		break;

#if	solaris>=20600
	    case N_SOCK:
		Lf->off_def = 1;
		break;
#endif	/* solaris>=20600 */

	    case N_HSFS:
		Lf->sz = (SZOFFTYPE)h.hs_dirent.ext_size;
		Lf->sz_def = 1;
		break;
	    case N_NM:
		Lf->sz = (SZOFFTYPE)nn.nm_vattr.va_size;
		Lf->sz_def = 1;
		break;
	    case N_DOOR:
	    case N_FIFO:
		if (!Fsize)
		    Lf->off_def = 1;
		break;
	    case N_MNT:

#if	defined(CVFS_SZSAVE)
		if (vfs) {
		    Lf->sz = (SZOFFTYPE)vfs->size;
		    Lf->sz_def = 1;
		} else
#endif	/* defined(CVFS_SZSAVE) */

		    Lf->off_def = 1;
		break;
	    case N_MVFS:
		/* The location of file size isn't known. */
		break;
	    case N_NFS:
		if ((type == VCHR || type == VBLK) && !Fsize)
		    Lf->off_def = 1;
		else {
		    Lf->sz = (SZOFFTYPE)r.r_size;
		    Lf->sz_def = 1;
		}
		break;

	    case N_PCFS:
		Lf->sz = (SZOFFTYPE)pc.pc_size;
		Lf->sz_def = 1;
		break;

#if	defined(HASPROCFS)
	    case N_PROC:

	    /*
	     * The proc file system size is defined when the
	     * prnode is read.
	     */
		break;
#endif	/* defined(HASPROCFS) */

	    case N_REGLR:
		if (type == VREG || type == VDIR) {
		    if (ins | nns) {
			Lf->sz = (SZOFFTYPE)(nns ? nn.nm_vattr.va_size
						 : i.i_size);
			Lf->sz_def = 1;
		    }
		} else if ((type == VCHR || type == VBLK) && !Fsize)
		    Lf->off_def = 1;
		break;
	    case N_STREAM:
		if (!Fsize)
		    Lf->off_def = 1;
		break;
	    case N_TMP:
		Lf->sz = (SZOFFTYPE)t.tn_attr.va_size;
		Lf->sz_def = 1;
		break;

#if	defined(HASVXFS)
	    case N_VXFS:
		if (type == VREG || type == VDIR) {
		    Lf->sz = (SZOFFTYPE)vx.sz;
		    Lf->sz_def = vx.sz_def;
		} else if ((type == VCHR || type == VBLK) && !Fsize)
		    Lf->off_def = 1;
		break;
#endif	/* defined(HASVXFS) */

	    }
	}
/*
 * Record link count.
 */
	if (Fnlink) {
	    switch (Ntype) {

#if	defined(HAS_AFS)
	    case N_AFS:
		Lf->nlink = an.nlink;
		Lf->nlink_def = an.nlink_st;
		break;
#endif	/* defined(HAS_AFS) */

#if	solaris>=20500
	    case N_AUTO:
		break;
	    case N_CACHE:
		Lf->nlink = (long)cn.c_attr.va_nlink;
		Lf->nlink_def = 1;
		break;
#endif	/* solaris>=20500 */

#if	solaris>=20600
	    case N_SOCK:			/* no link count */
		break;
#endif	/* solaris>=20600 */

	    case N_HSFS:
		Lf->nlink = (long)h.hs_dirent.nlink;
		Lf->nlink_def = 1;
		break;
	    case N_NM:
		Lf->nlink = (long)nn.nm_vattr.va_nlink;
		Lf->nlink_def = 1;
		break;
	    case N_DOOR:
		Lf->nlink = (long)v->v_count;
		Lf->nlink_def = 1;
		break;
	    case N_FIFO:
		break;
	    case N_MNT:

#if	defined(CVFS_NLKSAVE)
		if (vfs) {
		    Lf->nlink = (long)vfs->nlink;
		    Lf->nlink_def = 1;
		}
#endif	/* defined(CVFS_NLKSAVE) */

		break;
	    case N_MVFS:			/* no link count */
		break;
	    case N_NFS:
		Lf->nlink = (long)r.r_attr.va_nlink;
		Lf->nlink_def = 1;
		break;
	    case N_PCFS:
		break;

#if	defined(HASPROCFS)
	    case N_PROC:
		break;
#endif	/* defined(HASPROCFS) */

	    case N_REGLR:
		if (ins) {
		    Lf->nlink = (long)i.i_nlink;
		    Lf->nlink_def = 1;
		}
		break;
	    case N_STREAM:
		break;
	    case N_TMP:
		Lf->nlink = (long)t.tn_attr.va_nlink;
		Lf->nlink_def = 1;
		break;

#if	defined(HASVXFS)
	    case N_VXFS:
		Lf->nlink = vx.nl;
		Lf->nlink_def = vx.nl_def;
		break;
#endif	/* defined(HASVXFS) */

	    }
	    if (Nlink && Lf->nlink_def && (Lf->nlink < Nlink))
		Lf->sf |= SELNLINK;
	}
/*
 * Record an NFS selection.
 */
	if (Ntype == N_NFS && Fnfs)
	    Lf->sf |= SELNFS;

#if	solaris>=20500
/*
 * If this is a Solaris 2.5 and greater autofs entry, save the autonode name
 * (less than Solaris 2.6) or fnnode name (Solaris 2.6 and greater).
 */
	if (Ntype == N_AUTO && !Namech[0]) {

# if	solaris<20600
	    if (au.an_name[0])
		(void) snpf(Namech, Namechl, "%s", au.an_name);
# else  /* solaris>=20600 */
	    if (fnn.fn_name
	    &&  (len = fnn.fn_namelen) > 0
	    &&  len < (Namechl - 1))
	    {
		if (kread((KA_T)fnn.fn_name, Namech, len))
		    Namech[0] = '\0';
		else
		    Namech[len] = '\0';
	    }
# endif /* solaris<20600 */

	}
/*
 * If there is no local virtual file system pointer, or if its directory and
 * file system names are NULL, and if there is a namenode, and if we're using
 * the device number from it, see if its nm_mountpt vnode pointer leads to a
 * local virtual file system structure with non-NULL directory and file system
 * names.  If it does, switch to that local virtual file system pointer.
 */
	if (nns && (!vfs || (!vfs->dir && !vfs->fsname))
	&&  devs && (dev == nn.nm_vattr.va_fsid)
	&&  nn.nm_mountpt)
	{
	    if (!readvnode((KA_T)nn.nm_mountpt, &fv) && fv.v_vfsp) {
		if ((nvfs = readvfs((KA_T)fv.v_vfsp, (struct vfs *)NULL, 
				    nn.nm_filevp))
		&&  !nvfs->dir)
		{
		    (void) completevfs(nvfs, &dev);
		}

#if	defined(HASNCACHE)
		if (nvfs && nvfs->dir && nvfs->fsname) {
		    fa = (char *)NULL;
		    vfs = nvfs;
		}
#endif	/* defined(HASNCACHE) */

	    }
	}
/*
 * If there's a namenode and its device and node number match this one,
 * use the nm_mountpt's address for name cache lookups.
 */
	if (nns && devs && (dev == nn.nm_vattr.va_fsid) && (Lf->inp_ty == 1)
	&&  (Lf->inode == (unsigned long)nn.nm_vattr.va_nodeid))
	    Lf->na = (KA_T)nn.nm_mountpt;
#endif	/* solaris>=20500 */

/*
 * Save the file system names.
 */
	if (vfs) {
	    Lf->fsdir = vfs->dir;
	    Lf->fsdev = vfs->fsname;
	    if (!Lf->fsdir && !Lf->fsdev && kvs && fxs)
		Lf->fsdev = Fsinfo[fx];

#if	defined(HASFSINO)
	    Lf->fs_ino = vfs->fs_ino;
#endif	/* defined(HASFSINO) */

	}
/*
 * Save the device numbers, and their states.
 *
 * Format the vnode type, and possibly the device name.
 */
	switch (type) {

	case VNON:
	    ty ="VNON";
	    Lf->dev = dev;
	    Lf->dev_def = devs;
	    Lf->rdev = rdev;
	    Lf->rdev_def = rdevs;
	    break;
	case VREG:
	case VDIR:
	    ty = (type == VREG) ? "VREG" : "VDIR";
	    Lf->dev = dev;
	    Lf->dev_def = devs;
	    Lf->rdev = rdev;
	    Lf->rdev_def = rdevs;
	    break;
	case VBLK:
	    ty = "VBLK";
	    Lf->dev = dev;
	    Lf->dev_def = devs;
	    Lf->rdev = rdev;
	    Lf->rdev_def = rdevs;
	    Ntype = N_BLK;
	    break;
	case VCHR:
	    Lf->dev = dev;
	    Lf->dev_def = devs;
	    Lf->rdev = rdev;
	    Lf->rdev_def = rdevs;
	    if (unix_sock) {
		ty = "unix";
		break;
	    }
	    ty = "VCHR";
	    if (Lf->is_stream == 0 && Lf->is_com == 0)
		Ntype = N_CHR;
	    break;

#if	solaris>=20500
	case VDOOR:
	    Lf->dev = dev;
	    Lf->dev_def = devs;
	    Lf->rdev = rdev;
	    Lf->rdev_def = rdevs;
	    ty = "DOOR";
	    if (dns)
		(void) idoorkeep(&dn);
	    break;
#endif	/* solaris>=20500 */

	case VLNK:
	    ty = "VLNK";
	    Lf->dev = dev;
	    Lf->dev_def = devs;
	    Lf->rdev = rdev;
	    Lf->rdev_def = rdevs;
	    break;

#if	solaris>=20600
	case VPROC:

	/*
	 * The proc file system type is defined when the prnode is read.
	 */
	    Lf->dev = dev;
	    Lf->dev_def = devs;
	    Lf->rdev = rdev;
	    Lf->rdev_def = rdevs;
	    ty = (char *)NULL;
	    break;
#endif	/* solaris>=20600 */

#if	defined(HAS_VSOCK)
	case VSOCK:

# if	solaris>=20600
	    if (so.so_family == AF_UNIX) {
		ty = "unix";
		if (Funix)
		    Lf->sf |= SELUNX;
	    } else {
		if (so.so_family == AF_INET) {

#  if	defined(HASIPv6)
		    ty = "IPv4";
#  else	/* !defined(HASIPv6) */
		    ty = "inet";
#  endif	/* defined(HASIPv6) */

		    (void) snpf(Namech, Namechl, printsockty(so.so_type));
		    if (Fnet && (FnetTy != 6))
			Lf->sf |= SELNET;
		}

#  if	defined(HASIPv6)
		else if (so.so_family == AF_INET6) {
		    ty = "IPv6";
		    (void) snpf(Namech, Namechl, printsockty(so.so_type));
		    if (Fnet && (FnetTy != 4))
			Lf->sf |= SELNET;
		}
#  endif	/* defined(HASIPv6) */

		else {
		    ty = "sock";
		    (void) printunkaf(so.so_family, 0);
		    ep = endnm(&sz);
		    (void) snpf(ep, sz, ", %s", printsockty(so.so_type));
		}
	    }
# endif	/* solaris>=20600 */

	    Lf->dev = dev;
	    Lf->dev_def = devs;
	    Lf->rdev = rdev;
	    Lf->rdev_def = rdevs;
	    break;
#endif	/* defined(HAS_VSOCK) */

	case VBAD:
	    ty = "VBAD";
	    Lf->dev = dev;
	    Lf->dev_def = devs;
	    Lf->rdev = rdev;
	    Lf->rdev_def = rdevs;
	    break;
	case VFIFO:
	    ty = "FIFO";
	    if (!Lf->dev_ch || Lf->dev_ch[0] == '\0') {
		Lf->dev = dev;
		Lf->dev_def = devs;
		Lf->rdev = rdev;
		Lf->rdev_def = rdevs;
	    }
	    break;
	default:
	    Lf->dev = dev;
	    Lf->dev_def = devs;
	    Lf->rdev = rdev;
	    Lf->rdev_def = rdevs;
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
 * If this a Solaris common vnode/snode void some information.
 */
	if (Lf->is_com)
	    Lf->sz_def = Lf->inp_ty = 0;
/*
 * If a file attach description remains, put it in the NAME column addition.
 */
	if (fa)
	    (void) add_nma(fa, fal);

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
	if ((Lf->inp_ty == 0) && (type == VCHR)) {
	    find_ch_ino();
	/*
	 * If the VCHR inode number still isn't known and this is a COMMON
	 * vnode file or a stream, or if a pseudo node ID lookup has been
	 * requested, see if an inode number can be derived from a pseudo
	 * or clone device node.
	 *
	 * If it can, save the pseudo or clone device for temporary
	 * use when searching for a match with a named file argument.
	 */
	    if ((Lf->inp_ty == 0) && (Lf->is_com || Lf->is_stream || pnl)
	    &&  (Clone || Pseudo))
	    {
		if (!sdp) {
		    if (rdevs || devs) {
			if (Lf->is_stream && !pnl)
			    sdp = finddev(devs  ? &dev  : &DevDev,
					  rdevs ? &rdev : &Lf->dev,
					  LOOKDEV_CLONE);
			else
			    sdp = finddev(devs  ? &dev  : &DevDev,
					  rdevs ? &rdev : &Lf->dev,
					  LOOKDEV_PSEUDO);
			if (!sdp)
			    sdp = finddev(devs  ? &dev  : &DevDev,
					  rdevs ? &rdev : &Lf->dev,
					  LOOKDEV_ALL);
			if (sdp) {
			    if (!rdevs) {
				Lf->rdev = Lf->dev;
				Lf->rdev_def = rdevs = 1;
			    }
			    if (!devs) {
				Lf->dev = DevDev;
				devs = Lf->dev_def = 1;
			    }
			}
		    }
		} else {

		/*
		 * A local device structure has been located.  Make sure
		 * that it's accompanied by device settings.
		 */
		    if (!devs && vfs) {
			dev = Lf->dev = vfs->dev;
			devs = Lf->dev_def = 1;
		    }
		    if (!rdevs) {
			Lf->rdev = rdev = sdp->rdev;
			Lf->rdev_def = rdevs = 1;
		    }
		}
		if (sdp) {

		/*
		 * Process the local device information.
		 */
		    trdev = sdp->rdev;
		    Lf->inode = sdp->inode;
		    Lf->inp_ty = trdevs = 1;
		    if (!Namech[0] || Lf->is_com)
			(void) snpf(Namech, Namechl, "%s", sdp->name);
		    if (Lf->is_com && !Lf->nma) {
			len = strlen("(COMMON)") + 1;
			if (!(Lf->nma = (char *) malloc(len))) {
			    (void) fprintf(stderr,
				"%s: no space for (COMMON): PID %d; FD %s\n",
				Pn, Lp->pid, Lf->fd);
			    Exit(1);
			}
			(void) snpf(Lf->nma, len, "(COMMON)");
		    }
		}
	    }
	}
/*
 * Record stream status.
 */
	if (Lf->inp_ty == 0 && Lf->is_stream && strcmp(Lf->iproto, "STR") == 0)
	    Lf->inp_ty = 2;
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
		    if ((pfi->pid && pfi->pid == pids.pid_id)

# if	defined(HASPINODEN)
		    ||  (Lf->inp_ty == 1 && Lf->inode == pfi->inode)
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
	    if (Sfile) {
		if (trdevs) {
		    rdev = Lf->rdev;
		    Lf->rdev = trdev;
		    tdef = Lf->rdev_def;
		    Lf->rdev_def = 1;
		}
		if (is_file_named(NULL, Ntype, type, 1))
		    Lf->sf |= SELNM;
		if (trdevs) {
		    Lf->rdev = rdev;
		    Lf->rdev_def = tdef;
		}
	    }
	}
/*
 * Enter name characters.
 */
	if (Namech[0])
	    enter_nm(Namech);
}


/*
 * read_cni() - read common snode information
 */

static int
read_cni(s, rv, v, rs, di, din, dinl)
	struct snode *s;		/* starting snode */
	struct vnode *rv;		/* "real" vnode receiver */
	struct vnode *v;		/* starting vnode */
	struct snode *rs;		/* "real" snode receiver */
	struct dev_info *di;		/* dev_info structure receiver */
	char *din;			/* device info name receiver */
	int dinl;			/* sizeof(*din) */
{
	char tbuf[32];

	if (read_nvn((KA_T)v->v_data, (KA_T)s->s_commonvp, rv))
	    return(1);
	if (read_nsn((KA_T)s->s_commonvp, (KA_T)rv->v_data, rs))
	    return(1);
	*din = '\0';
	if (rs->s_dip) {
	    if (kread((KA_T)rs->s_dip, (char *)di, sizeof(struct dev_info))) {
		(void) snpf(Namech, Namechl,
		    "common snode at %s: no dev info: %s",
		    print_kptr((KA_T)rv->v_data, tbuf, sizeof(tbuf)),
		    print_kptr((KA_T)rs->s_dip, (char *)NULL, 0));
		enter_nm(Namech);
		return(1);
	    }
	    if (di->devi_name
	    &&  kread((KA_T)di->devi_name, din, dinl-1) == 0)
		din[dinl-1] = '\0';
	}
	return(0);
}


/*
 * readinode() - read inode
 */

static int
readinode(ia, i)
	KA_T ia;			/* inode kernel address */
	struct inode *i;		/* inode buffer */
{
	if (kread((KA_T)ia, (char *)i, sizeof(struct inode))) {
	    (void) snpf(Namech, Namechl, "can't read inode at %s",
		print_kptr((KA_T)ia, (char *)NULL, 0));
	    return(1);
	}
	return(0);
}


#if	solaris>=20500
/*
 * read_ndn() - read node's door node
 */

static int
read_ndn(na, da, dn)
	KA_T na;			/* containing vnode's address */
	KA_T da;			/* door node's address */
	struct door_node *dn;		/* door node receiver */
{
	char tbuf[32];

	if (!da || kread((KA_T)da, (char *)dn, sizeof(struct door_node))) {
	    (void) snpf(Namech, Namechl,
		"vnode at %s: can't read door_node: %s",
		print_kptr(na, tbuf, sizeof(tbuf)),
		print_kptr(da, (char *)NULL, 0));
	    enter_nm(Namech);
	    return(1);
	}
	return(0);
}
#endif	/* solaris>=20500 */



/*
 * read_mi() - read stream's module information
 */

static void
read_mi(s, rdev, so, so_st, so_ad, sdp)
	KA_T s;				/* kernel stream pointer address */
	dev_t *rdev;			/* raw device pointer */
	caddr_t so;			/* so_so return (Solaris) */
	int *so_st;			/* so_so status */
	KA_T *so_ad;			/* so_so addresses */
	struct l_dev **sdp;		/* returned device pointer */
{
	struct l_dev *dp;
	int i, j, k, nl;
	KA_T ka;
	struct module_info mi;
	char mn[STRNML];
	struct stdata sd;
	struct queue q;
	struct qinit qi;
	KA_T qp;
/*
 * If there is no stream pointer, or we can't read the stream head,
 * return.
 */
	if (!s)
	    return;
	if (kread((KA_T)s, (char *)&sd, sizeof(sd))) {
	    (void) snpf(Namech, Namechl, "can't read stream head: %s",
		print_kptr(s, (char *)NULL, 0));
	    return;
	}
/*
 * Follow the stream head to each of its queue structures, retrieving the
 * module names from each queue's q_info->qi_minfo->mi_idname chain of
 * structures.  Separate each additional name from the previous one with
 * "->".
 *
 * Ignore failures to read all but queue structure chain entries.
 *
 * Ignore module names that end in "head".
 */
	k = 0;
	Namech[0] = '\0';
	if (!(dp = finddev(&DevDev, rdev, LOOKDEV_CLONE)))
	    dp = finddev(&DevDev, rdev, LOOKDEV_ALL);
	if (dp) {
	    (void) snpf(Namech, Namechl, "%s", dp->name);
	    k = strlen(Namech);
	    *sdp = dp;
	} else
	    (void) snpf(Lf->iproto, sizeof(Lf->iproto), "STR");
	nl = sizeof(mn) - 1;
	mn[nl] = '\0';
	qp = (KA_T)sd.sd_wrq;
	for (i = 0; qp && i < 20; i++, qp = (KA_T)q.q_next) {
	    if (!qp ||  kread(qp, (char *)&q, sizeof(q)))
		break;
	    if ((ka = (KA_T)q.q_qinfo) == (KA_T)NULL
	    ||  kread(ka, (char *)&qi, sizeof(qi)))
		continue;
	    if ((ka = (KA_T)qi.qi_minfo) == (KA_T)NULL
	    ||  kread(ka, (char *)&mi, sizeof(mi)))
		continue;
	    if ((ka = (KA_T)mi.mi_idname) == (KA_T)NULL
	    ||  kread(ka, mn, nl))
		continue;
	    if ((j = strlen(mn)) < 1)
		continue;
	    if (j >= 4 && strcmp(&mn[j - 4], "head") == 0)
		continue;
	    if (strcmp(mn, "sockmod") == 0) {

	    /*
	     * Save the Solaris sockmod device and inode numbers.
	     */
		if (so) {

		    struct so_so s;

		    if (!kread((KA_T)q.q_ptr, (char *)&s, sizeof(s))) {
			if (!(*so_st))
			    so_ad[0] = (KA_T)q.q_ptr;
			else
			    so_ad[1] = (KA_T)q.q_ptr;
			(void) savesockmod(&s, (struct so_so *)so, so_st);
		    }
		}
	    }
	    if (k) {
		if ((k + 2) > (Namechl - 1))
		    break;
		(void) snpf(&Namech[k], Namechl - k, "->");
		k += 2;
	    }
	    if ((k + j) > (Namechl - 1))
		break;
	    (void) snpf(&Namech[k], Namechl - k, "%s", mn);
	    k += j;
	}
}


#if	solaris>=20500

/*
 * read_nan(na, ca, cn) - read node's autofs node
 */

static int
read_nan(na, aa, rn)
	KA_T na;			/* containing node's address */
	KA_T aa;			/* autofs node address */

# if    solaris<20600
	struct autonode *rn;		/* autofs node receiver */
# else  /* solaris>=20600 */
	struct fnnode *rn;		/* autofs node receiver */
# endif /* solaris<20600 */

{
	char tbuf[32];

# if    solaris<20600
	if (!aa || kread((KA_T)aa, (char *)rn, sizeof(struct autonode)))
# else  /* solaris>=20600 */
	if (!aa || kread((KA_T)aa, (char *)rn, sizeof(struct fnnode)))
# endif /* solaris<20600 */

	{
	    (void) snpf(Namech, Namechl,

# if    solaris<20600
		"node at %s: can't read autonode: %s",
# else  /* solaris>=20600 */
		"node at %s: can't read fnnode: %s",
# endif /* solaris<20600 */

		print_kptr(na, tbuf, sizeof(tbuf)),
		print_kptr(aa, (char *)NULL, 0));
	    enter_nm(Namech);
	    return(1);
	}
	return(0);
}
#endif	/* solaris>=20500 */


/*
 * read_ncn(na, ca, cn) - read node's cache node
 */

static int
read_ncn(na, ca, cn)
	KA_T na;			/* containing node's address */
	KA_T ca;			/* cache node address */
	struct cnode *cn;		/* cache node receiver */
{
	char tbuf[32];

	if (!ca || kread((KA_T)ca, (char *)cn, sizeof(struct cnode))) {
	    (void) snpf(Namech, Namechl, "node at %s: can't read cnode: %s",
		print_kptr(na, tbuf, sizeof(tbuf)),
		print_kptr(ca, (char *)NULL, 0));
	    enter_nm(Namech);
	    return(1);
	}
	return(0);
}


/*
 * read_nfn() - read node's fifonode
 */

static int
read_nfn(na, fa, f)
	KA_T na;			/* containing node's address */
	KA_T fa;			/* fifonode address */
	struct fifonode *f;		/* fifonode receiver */
{
	char tbuf[32];

	if (!fa || readfifonode(fa, f)) {
	    (void) snpf(Namech, Namechl, "node at %s: can't read fifonode: %s",
		print_kptr(na, tbuf, sizeof(tbuf)),
		print_kptr(fa, (char *)NULL, 0));
	    enter_nm(Namech);
	    return(1);
	}
	return(0);
}


/*
 * read_nhn() - read node's High Sierra node
 */

static int
read_nhn(na, ha, h)
	KA_T na;			/* containing node's address */
	KA_T ha;			/* hsnode address */
	struct hsnode *h;		/* hsnode receiver */
{
	char tbuf[32];

	if (!ha || readhsnode(ha, h)) {
	    (void) snpf(Namech, Namechl, "node at %s: can't read hsnode: %s",
		print_kptr(na, tbuf, sizeof(tbuf)),
		print_kptr(ha, (char *)NULL, 0));
	    enter_nm(Namech);
		return(1);
	}
	return(0);
}


/*
 * read_nin() - read node's inode
 */

static int
read_nin(na, ia, i)
	KA_T na;			/* containing node's address */
	KA_T ia;			/* kernel inode address */
	struct inode *i;		/* inode receiver */
{
	char tbuf[32];

	if (!ia || readinode(ia, i)) {
	    (void) snpf(Namech, Namechl,
		"node at %s: can't read inode: %s",
		print_kptr(na, tbuf, sizeof(tbuf)),
		print_kptr(ia, (char *)NULL, 0));
	    enter_nm(Namech);
	    return(1);
	}
	return(0);
}


/*
 * read_nln(na, la, ln) - read node's loopback node
 */

static int
read_nln(na, la, ln)
	KA_T na;			/* containing node's address */
	KA_T la;			/* loopback node address */
	struct lnode *ln;		/* loopback node receiver */
{
	char tbuf[32];

	if (!la || kread((KA_T)la, (char *)ln, sizeof(struct lnode))) {
	    (void) snpf(Namech, Namechl, "node at %s: can't read lnode: %s",
		print_kptr(na, tbuf, sizeof(tbuf)),
		print_kptr(la, (char *)NULL, 0));
	    enter_nm(Namech);
	    return(1);
	}
	return(0);
}


/*
 * read_nnn() - read node's namenode
 */

static int
read_nnn(na, nna, nn)
	KA_T na;			/* containing node's address */
	KA_T nna;			/* namenode address */
	struct namenode *nn;		/* namenode receiver */
{
	char tbuf[32];

	if (!nna || kread((KA_T)nna, (char *)nn, sizeof(struct namenode))) {
	    (void) snpf(Namech, Namechl, "node at %s: can't read namenode: %s",
		print_kptr(na, tbuf, sizeof(tbuf)),
		print_kptr(nna, (char *)NULL, 0));
	    enter_nm(Namech);
	    return(1);
	}
	return(0);
}


/*
 * read_nmn() - read node's mvfsnode
 */

static int
read_nmn(na, ma, m)
	KA_T na;			/* containing node's address */
	KA_T ma;			/* kernel mvfsnode address */
	struct mvfsnode *m;		/* mvfsnode receiver */
{
	char tbuf[32];

	if (!ma || kread((KA_T)ma, (char *)m, sizeof(struct mvfsnode))) {
	    (void) snpf(Namech, Namechl, "node at %s: can't read mvfsnode: %s",
		print_kptr(na, tbuf, sizeof(tbuf)),
		print_kptr(ma, (char *)NULL, 0));
	    enter_nm(Namech);
	    return(1);
	}
	return(0);
}


#if	defined(HASPROCFS)
/*
 * read_npi() - read node's /proc file system information
 */

static int
read_npi(na, v, pids)
	KA_T na;			/* containing node's address */
	struct vnode *v;		/* containing vnode */
	struct pid *pids;		/* pid structure receiver */
{
	struct as as;
	struct proc p;
	struct prnode pr;
	char tbuf[32];

#if	solaris>=20600
	prcommon_t pc, ppc;
	int pcs, ppcs, prpcs, prppcs;
	struct proc pp;
	pid_t prpid;
	id_t prtid;
	char *ty = (char *)NULL;
#endif	/* solaris>=20600 */

	if (!v->v_data || kread((KA_T)v->v_data, (char *)&pr, sizeof(pr))) {
	    (void) snpf(Namech, Namechl, "node at %s: can't read prnode: %s",
		print_kptr(na, tbuf, sizeof(tbuf)),
		print_kptr((KA_T)v->v_data, (char *)NULL, 0));
	    enter_nm(Namech);
	    return(1);
	}

#if	solaris<20600
/*
 * For Solaris < 2.6:
 *	* Read the proc structure, get the process size and PID;
 *	* Return the PID;
 *	* Enter a name, constructed from the file system and PID;
 *	* Enter an inode number, constructed from the PID.
 */
	if (!pr.pr_proc) {
	    if (v->v_type == VDIR) {
		(void) snpf(Namech, Namechl, "/%s", HASPROCFS);
		enter_nm(Namech);
		Lf->inode = PR_ROOTINO;
		Lf->inp_ty = 1;
	    } else {
		(void) snpf(Namech, Namechl, "/%s/", HASPROCFS);
		enter_nm(Namech);
		Lf->inp_ty = 0;
	    }
	    return(0);
	}
	if (kread((KA_T)pr.pr_proc, (char *)&p, sizeof(p))) {
	    (void) snpf(Namech, Namechl, "prnode at %s: can't read proc: %s",
		print_kptr((KA_T)v->v_data, tbuf, sizeof(tbuf)),
		print_kptr((KA_T)pr.pr_proc, (char *)NULL, 0));
	    enter_nm(Namech);
	    return(1);
	}
	if (p.p_as && !kread((KA_T)p.p_as, (char *)&as, sizeof(as))) {
	    Lf->sz = (SZOFFTYPE)as.a_size;
	    Lf->sz_def = 1;
	}
	if (!p.p_pidp
	||  kread((KA_T)p.p_pidp, (char *)pids, sizeof(struct pid))) {
	    (void) snpf(Namech, Namechl,
		"proc struct at %s: can't read pid: %s",
		print_kptr((KA_T)pr.pr_proc, tbuf, sizeof(tbuf)),
		print_kptr((KA_T)p.p_pidp, (char *)NULL, 0));
	    enter_nm(Namech);
	    return(1);
	}
	(void) snpf(Namech, Namechl, "/%s/%d", HASPROCFS, (int)pids->pid_id);
	Lf->inode = (unsigned long)ptoi(pids->pid_id);
	Lf->inp_ty = 1;
#else	/* solaris>=20600 */
/*
 * Enter the >= Solaris 2.6 inode number.
 */
	Lf->inode = (unsigned long)pr.pr_ino;
	Lf->inp_ty = 1;
/*
 * Read the >= Solaris 2.6 prnode common structures.
 *
 * Return the PID number.
 *
 * Identify the lwp PID (the thread ID).
 */
	if (pr.pr_common
	&&  kread((KA_T)pr.pr_common, (char *)&pc, sizeof(pc)) == 0) {
	    pcs = 1;
	    if (pc.prc_proc
	    &&  kread((KA_T)pc.prc_proc, (char *)&p, sizeof(p)) == 0)
		prpcs = 1;
	    else
		prpcs = 0;
	} else
	   pcs = prpcs = 0;
	if (pr.pr_pcommon
	&&  kread((KA_T)pr.pr_pcommon, (char *)&ppc, sizeof(ppc)) == 0) {
	    ppcs = 1;
	    if (ppc.prc_proc
	    &&  kread((KA_T)ppc.prc_proc, (char *)&pp, sizeof(pp)) == 0)
		prppcs = 1;
	    else
		prppcs = 0;
	} else
	    ppcs = prppcs = 0;
	if (pcs && pc.prc_pid)
	    pids->pid_id = prpid = pc.prc_pid;
	else if (ppcs && ppc.prc_pid)
	    pids->pid_id = prpid = ppc.prc_pid;
	else
	    pids->pid_id = prpid = (pid_t)0;
	if (pcs && pc.prc_tid)
	    prtid = pc.prc_tid;
	else if (ppcs && ppc.prc_tid)
	    prtid = ppc.prc_tid;
	else
	    prtid = (id_t)0;
/*
 * Identify the Solaris 2.6 /proc file system name, file size, and file type.
 */
	switch (pr.pr_type) {
	case PR_PROCDIR:
	    (void) snpf(Namech, Namechl,  "/%s", HASPROCFS);
	    ty = "PDIR";
	    break;
	case PR_PIDDIR:
	    (void) snpf(Namech, Namechl,  "/%s/%d", HASPROCFS, (int)prpid);
	    ty = "PDIR";
	    break;
	case PR_AS:
	    (void) snpf(Namech, Namechl,  "/%s/%d/as", HASPROCFS, (int)prpid);
	    ty = "PAS";
	    if (prpcs
	    &&  kread((KA_T)pc.prc_proc, (char *)&p, sizeof(p)) == 0
	    &&  p.p_as
	    &&  kread((KA_T)p.p_as, (char *)&as, sizeof(as)) == 0) {
		Lf->sz = (SZOFFTYPE)as.a_size;
		Lf->sz_def = 1;
	    }
	    break;
	case PR_CTL:
	    (void) snpf(Namech, Namechl,  "/%s/%d/ctl", HASPROCFS, (int)prpid);
	    ty = "PCTL";
	    break;
	case PR_STATUS:
	    (void) snpf(Namech, Namechl,
		"/%s/%d/status", HASPROCFS, (int)prpid);
	    ty = "PSTA";
	    break;
	case PR_LSTATUS:
	    (void) snpf(Namech, Namechl,
		"/%s/%d/lstatus", HASPROCFS, (int)prpid);
	    ty = "PLST";
	    break;
	case PR_PSINFO:
	    (void) snpf(Namech, Namechl,
		"/%s/%d/psinfo", HASPROCFS, (int)prpid);
	    ty = "PSIN";
	    break;
	case PR_LPSINFO:
	    (void) snpf(Namech, Namechl,
		"/%s/%d/lpsinfo", HASPROCFS, (int)prpid);
	    ty = "PLPI";
	    break;
	case PR_MAP:
	    (void) snpf(Namech, Namechl,
		"/%s/%d/map", HASPROCFS, (int)prpid);
	    ty = "PMAP";
	    break;
	case PR_RMAP:
	    (void) snpf(Namech, Namechl,
		"/%s/%d/rmap", HASPROCFS, (int)prpid);
	    ty = "PRMP";
	    break;
	case PR_XMAP:
	    (void) snpf(Namech, Namechl,
		"/%s/%d/xmap", HASPROCFS, (int)prpid);
	    ty = "PXMP";
	    break;
	case PR_CRED:
	    (void) snpf(Namech, Namechl,
		"/%s/%d/cred", HASPROCFS, (int)prpid);
	    ty = "PCRE";
	    break;
	case PR_SIGACT:
	    (void) snpf(Namech, Namechl,
		"/%s/%d/sigact", HASPROCFS, (int)prpid);
	    ty = "PSGA";
	    break;
	case PR_AUXV:
	    (void) snpf(Namech, Namechl,
		"/%s/%d/auxv", HASPROCFS, (int)prpid);
	    ty = "PAXV";
	    break;

# if	defined(HASPR_LDT)
	case PR_LDT:
	    (void) snpf(Namech, Namechl,
		"/%s/%d/ldt", HASPROCFS, (int)prpid);
	    ty = "PLDT";
	    break;
# endif	/* defined(HASPR_LDT) */

	case PR_USAGE:
	    (void) snpf(Namech, Namechl,
		"/%s/%d/usage", HASPROCFS, (int)prpid);
	    ty = "PUSG";
	    break;
	case PR_LUSAGE:
	    (void) snpf(Namech, Namechl,
		"/%s/%d/lusage", HASPROCFS, (int)prpid);
	    ty = "PLU";
	    break;
	case PR_PAGEDATA:
	    (void) snpf(Namech, Namechl,
		"/%s/%d/pagedata", HASPROCFS, (int)prpid);
	    ty = "PGD";
	    break;
	case PR_WATCH:
	    (void) snpf(Namech, Namechl,
		"/%s/%d/watch", HASPROCFS, (int)prpid);
	    ty = "PW";
	    break;
	case PR_CURDIR:
	    (void) snpf(Namech, Namechl,
		"/%s/%d/cwd", HASPROCFS, (int)prpid);
	    ty = "PCWD";
	    break;
	case PR_ROOTDIR:
	    (void) snpf(Namech, Namechl,
		"/%s/%d/root", HASPROCFS, (int)prpid);
	    ty = "PRTD";
	    break;
	case PR_FDDIR:
	    (void) snpf(Namech, Namechl,
		"/%s/%d/fd", HASPROCFS, (int)prpid);
	    ty = "PFDR";
	    break;
	case PR_FD:
	    (void) snpf(Namech, Namechl,
		"/%s/%d/fd/%d", HASPROCFS, (int)prpid,
		pr.pr_index);
	    ty = "PFD";
	    break;
	case PR_OBJECTDIR:
	    (void) snpf(Namech, Namechl,
		"/%s/%d/object", HASPROCFS, (int)prpid);
	    ty = "PODR";
	    break;
	case PR_OBJECT:
	    (void) snpf(Namech, Namechl,
		"/%s/%d/object/", HASPROCFS, (int)prpid);
	    ty = "POBJ";
	    break;
	case PR_LWPDIR:
	    (void) snpf(Namech, Namechl, "/%s/%d/lpw", HASPROCFS, (int)prpid);
	    ty = "PLDR";
	    break;
	case PR_LWPIDDIR:
	    (void) sprintf(Namech, "/%s/%d/lwp/%d", HASPROCFS, (int)prpid,
		(int)prtid);
	    ty = "PLDR";
	    break;
	case PR_LWPCTL:
	    (void) snpf(Namech, Namechl, "/%s/%d/lwp/%d/lwpctl", HASPROCFS,
		(int)prpid, (int)prtid);
	    ty = "PLC";
	    break;
	case PR_LWPSTATUS:
	    (void) snpf(Namech, Namechl, "/%s/%d/lwp/%d/lwpstatus", HASPROCFS,
		(int)prpid, (int)prtid);
	    ty = "PLWS";
	    break;
	case PR_LWPSINFO:
	    (void) snpf(Namech, Namechl, "/%s/%d/lwp/%d/lwpsinfo", HASPROCFS,
		(int)prpid, (int)prtid);
	    ty = "PLWI";
	    break;
	case PR_LWPUSAGE:
	    (void) snpf(Namech, Namechl, "/%s/%d/lwp/%d/lwpusage", HASPROCFS,
		(int)prpid, (int)prtid);
	    ty = "PLWU";
	    break;
	case PR_XREGS:
	    (void) snpf(Namech, Namechl, "/%s/%d/lwp/%d/xregs", HASPROCFS,
		(int)prpid, (int)prtid);
	    ty = "PLWX";
	    break;

# if	defined(HASPR_GWINDOWS)
	case PR_GWINDOWS:
	    (void) snpf(Namech, Namechl, "/%s/%d/lwp/%d/gwindows", HASPROCFS,
		(int)prpid, (int)prtid);
	    ty = "PLWG";
	    break;
# endif	/* defined(HASPR_GWINDOWS) */

	case PR_PIDFILE:
	    (void) snpf(Namech, Namechl, "/%s/%d", HASPROCFS, (int)prpid);
	    ty = "POPF";
	    break;
	case PR_LWPIDFILE:
	    (void) snpf(Namech, Namechl, "/%s/%d", HASPROCFS, (int)prpid);
	    ty = "POLP";
	    break;
	case PR_OPAGEDATA:
	    (void) snpf(Namech, Namechl, "/%s/%d", HASPROCFS, (int)prpid);
	    ty = "POPG";
	    break;
	default:
	    ty = (char *)NULL;
	}
	if (!ty) {
	    if (pr.pr_type > 9999)
		(void) snpf(Lf->type, sizeof(Lf->type), "*%03d", pr.pr_type);
	    else
		(void) snpf(Lf->type, sizeof(Lf->type), "%4d", pr.pr_type);
	    (void) snpf(Namech, Namechl,
		"unknown %s node type", HASPROCFS);
	} else
	    (void) snpf(Lf->type, sizeof(Lf->type), "%s", ty);
/*
 * Record the Solaris 2.6 /proc file system inode number.
 */
	Lf->inode = (unsigned long)pr.pr_ino;
	Lf->inp_ty = 1;
# endif	/* solaris<20600 */

	enter_nm(Namech);
	return(0);
}
#endif	/* defined(HASPROCFS) */


/*
 * read_npn() - read node's pcnode
 */

static int
read_npn(na, pa, p)
	KA_T na;			/* containing node's address */
	KA_T pa;			/* pcnode address */
	struct pcnode *p;		/* pcnode receiver */
{
	char tbuf[32];

	if (!pa || kread(pa, (char *)p, sizeof(struct pcnode))) {
	    (void) snpf(Namech, Namechl, "node at %s: can't read pcnode: %s",
		print_kptr(na, tbuf, sizeof(tbuf)),
		print_kptr(pa, (char *)NULL, 0));
	    enter_nm(Namech);
	    return(1);
	}
	return(0);
}


/*
 * read_nrn() - read node's rnode
 */

static int
read_nrn(na, ra, r)
	KA_T na;			/* containing node's address */
	KA_T ra;			/* rnode address */
	struct rnode *r;		/* rnode receiver */
{
	char tbuf[32];

	if (!ra || readrnode(ra, r)) {
	    (void) snpf(Namech, Namechl, "node at %s: can't read rnode: %s",
		print_kptr(na, tbuf, sizeof(tbuf)),
		print_kptr(ra, (char *)NULL, 0));
	    enter_nm(Namech);
	    return(1);
	}
	return(0);
}


#if	solaris>=20600
/*
 * read_nson() - read node's sonode
 */

static int
read_nson(na, sa, sn)
	KA_T na;			/* containing node's address */
	KA_T sa;			/* sonode address */
	struct sonode *sn;		/* sonode receiver */

{
	char tbuf[32];

	if (!sa || kread((KA_T)sa, (char *)sn, sizeof(struct sonode))) {
	    (void) snpf(Namech, Namechl, "node at %s: can't read sonode: %s",
		print_kptr(na, tbuf, sizeof(tbuf)),
		print_kptr(sa, (char *)NULL, 0));
	    enter_nm(Namech);
	    return(1);
	}
	return(0);
}
#endif	/* solaris>=20600 */


/*
 * read_nsn() - read node's snode
 */

static int
read_nsn(na, sa, s)
	KA_T na;			/* containing node's address */
	KA_T sa;			/* snode address */
	struct snode *s;		/* snode receiver */
{
	char tbuf[32];

	if (!sa || readsnode(sa, s)) {
	    (void) snpf(Namech, Namechl, "node at %s: can't read snode: %s",
		print_kptr(na, tbuf, sizeof(tbuf)),
		print_kptr(sa, (char *)NULL, 0));
	    enter_nm(Namech);
	    return(1);
	}
	return(0);
}


/*
 * read_ntn() - read node's tmpnode
 */

static int
read_ntn(na, ta, t)
	KA_T na;			/* containing node's address */
	KA_T ta;			/* tmpnode address */
	struct tmpnode *t;		/* tmpnode receiver */
{
	char tbuf[32];

	if (!ta || readtnode(ta, t)) {
	    (void) snpf(Namech, Namechl, "node at %s: can't read tnode: %s",
		print_kptr(na, tbuf, sizeof(tbuf)),
		print_kptr(ta, (char *)NULL, 0));
	    enter_nm(Namech);
	    return(1);
	}
	return(0);
}


#if	solaris>=20600
/*
 * read_nusa() - read sondode's UNIX socket address
 */

static int
read_nusa(so, ua)
	struct soaddr *so;		/* kernel soaddr structure */
	struct sockaddr_un *ua;		/* local sockaddr_un address */
{
	KA_T a;
	int len;
	int min = offsetof(struct sockaddr_un, sun_path);

	ua->sun_path[0] = '\0';
	if (!(a = (KA_T)so->soa_sa)
	||  (len = so->soa_len) < (min + 2)
	||  len > (int)sizeof(struct sockaddr_un)
	||  kread(a, (char *)ua, len)
	||  ua->sun_family != AF_UNIX)
	    return(0);
	len -= min;
	if (len >= sizeof(ua->sun_path))
	    len = sizeof(ua->sun_path) - 1;
	ua->sun_path[len] = '\0';
	return(strlen(ua->sun_path));
}
#endif	/* solaris>=20600 */


/*
 * read_nvn() - read node's vnode
 */

static int
read_nvn(na, va, v)
	KA_T na;			/* node's address */
	KA_T va;			/* vnode address */
	struct vnode *v;		/* vnode receiver */
{
	char tbuf[32];

	if (readvnode(va, v)) {
	    (void) snpf(Namech, Namechl,
		"node at %s: can't read real vnode: %s",
		print_kptr(na, tbuf, sizeof(tbuf)),
		print_kptr(va, (char *)NULL, 0));
	    enter_nm(Namech);
	    return(1);
	}
	return(0);
}


/*
 * savesockmod() - save addresses from sockmod so_so structure
 */

static void
savesockmod(so, sop, so_st)
	struct so_so *so;		/* new so_so structure pointer */
	struct so_so *sop;		/* previous so_so structure pointer */
	int *so_st;			/* status of *sop (0 if not loaded) */
{
	dev_t d1, d2, d3;

#define	luxadr	lux_dev.addr.tu_addr
#define	luxdev	lux_dev.addr.tu_addr.dev
#define	luxino	lux_dev.addr.tu_addr.ino
#define	ruxadr	rux_dev.addr.tu_addr
#define	ruxdev	rux_dev.addr.tu_addr.dev
#define	ruxino	rux_dev.addr.tu_addr.ino

#if	solaris<20500
/*
 * If either address in the new structure is missing a device number, clear
 * its corresponding inode number.  Then sort the inode-less device numbers.
 */
	if (!so->luxdev)
	    so->luxino = (ino_t)0;
	if (!so->ruxdev)
	    so->ruxino = (ino_t)0;
	if (!so->luxino && !so->ruxino) {
	    if (so->luxdev > so->ruxdev) {
		d2 = so->luxdev;
		d1 = so->luxdev = so->ruxdev;
		so->ruxdev = d2;
	    } else {
		d1 = so->luxdev;
		d2 = so->ruxdev;
	    }
	} else
	    d1 = d2 = (dev_t)0;
/*
 * If the previous structure hasn't been loaded, save the new one in it with
 * adjusted or sorted addresses.
 */
	if (!*so_st) {
	    if (so->luxdev && so->luxino) {
		*sop = *so;
		sop->ruxdev = (dev_t)0;
		sop->ruxino = (ino_t)0;
		*so_st = 1;
		return;
	    }
	    if (so->ruxdev && so->ruxino) {
		*sop = *so;
		sop->luxadr = sop->ruxadr;
		sop->ruxdev = (dev_t)0;
		sop->ruxino = (ino_t)0;
		*so_st = 1;
		return;
	    }
	    *sop = *so;
	    *so_st = 1;
	    return;
	}
/*
 * See if the new sockmod addresses need to be merged with the previous
 * ones:
 *
 *	*  Don't merge if the previous so_so structure's lux_dev has a non-
 *	   zero device and a non-zero inode number.
 *
 *	*  If either of the device/inode pairs in the new structure is non-
 *	   zero, propagate them to the previous so_so structure.
 *
 *	*  Don't merge if the both device numbers in the new structure are
 *	   zero.
 */
	if (sop->luxdev && sop->luxino)
	    return;
	if (so->luxdev && so->luxino) {
	    sop->luxadr = so->luxadr;
	    sop->ruxdev = (dev_t)0;
	    sop->ruxino = (ino_t)0;
	    return;
	}
	if (so->ruxdev && so->ruxino) {
	    sop->luxadr = so->ruxadr;
	    sop->ruxdev = (dev_t)0;
	    sop->ruxino = (ino_t)0;
	    return;
	}
	if (!so->luxdev && !so->ruxdev)
	    return;
/*
 * Check the previous structure's device numbers:
 *
 *	*  If both are zero, replace the previous structure with the new one.
 *
 *	*  Choose the minimum and maximum non-zero device numbers contained in
 *	   either structure.
 */
	if (!sop->luxdev && !sop->ruxdev) {
	    *sop = *so;
	    return;
	}
	if (!sop->luxdev && (d1 || d2)) {
	    if (d1) {
		sop->luxdev = d1;
		d1 = (dev_t)0;
	    } else {
		sop->luxdev = d2;
		d2 = (dev_t)0;
	    }
	    if (sop->luxdev > sop->ruxdev) {
		d3 = sop->luxdev;
		sop->luxdev = sop->ruxdev;
		sop->ruxdev = d3;
	    }
	}
	if (!sop->ruxdev && (d1 || d2)) {
	    if (d1) {
		sop->ruxdev = d1;
		d1 = (dev_t)0;
	    } else {
		sop->ruxdev = d2;
		d2 = (dev_t)0;
	    }
	    if (sop->luxdev > sop->ruxdev) {
		d3 = sop->luxdev;
		sop->luxdev = sop->ruxdev;
		sop->ruxdev = d3;
	    }
	}
	if (sop->luxdev && sop->ruxdev) {
	    if (d1) {
		if (d1 < sop->luxdev)
		    sop->luxdev = d1;
		else if (d1 > sop->ruxdev)
		    sop->ruxdev = d1;
	    }
	    if (d2) {
		if (d2 < sop->luxdev)
		    sop->luxdev = d2;
		else if (d2 > sop->ruxdev)
		    sop->ruxdev = d2;
	    }
	}
#else	/* solaris>=20500 */
/*
 * Save the first sockmod structure.
 */
	if (!*so_st) {
	    *so_st = 1;
	    *sop = *so;
	}
#endif	/* solaris<20500 */

}


/*
 * vop2ty() - convert vnode operation switch address to internal type
 */

int
vop2ty(vp)
	struct vnode *vp;		/* local vnode pointer */
{
	register int i;

#if	defined(HAS_AFS)
	int afs = 0;			/* afs test status: -1 = no AFS
					 *		     0 = not tested
					 *		     1 = AFS */
#endif	/* defined(HAS_AFS) */

	if (!vp->v_op)
		return(-1);
	if ((Uvops && Uvops == (KA_T)vp->v_op)
	||  (Spvops && Spvops == (KA_T)vp->v_op))
	    return(N_REGLR);
	if (Nvops && Nvops == (KA_T)vp->v_op)
	    return(N_NFS);
	else if (N3vops && N3vops == (KA_T)vp->v_op)
	    return(N_NFS);

# if	defined(HASVXFS)
	for (i = 0; i < VXVOP_NUM; i++) {
	    if (Vvops[i] && Vvops[i] == (KA_T)vp->v_op)
		return(N_VXFS);
	}
# endif	/* defined(HASVXFS) */

	if (Tvops && Tvops == (KA_T)vp->v_op)
	    return(N_TMP);
	else if (Auvops && Auvops == (KA_T)vp->v_op)
	    return(N_AUTO);
	else if (Hvops && Hvops == (KA_T)vp->v_op)
	    return(N_HSFS);
	else if ((Pdvops && Pdvops == (KA_T)vp->v_op)
	     ||  (Pfvops && Pfvops == (KA_T)vp->v_op))
	{
	    return(N_PCFS);
	}
	else if (Mntops && Mntops == (KA_T)vp->v_op)
	    return(N_MNT);
	else if (Mvops && Mvops == (KA_T)vp->v_op)
	    return(N_MVFS);
	else if (Cvops && Cvops == (KA_T)vp->v_op)
	    return(N_CACHE);
	else if (Dvops && Dvops == (KA_T)vp->v_op)
	    return(N_DOOR);
	else if (Fvops && Fvops == (KA_T)vp->v_op)
	    return(N_FIFO);
	else if (Lvops && Lvops == (KA_T)vp->v_op)
	    return(N_LOFS);
	else if (Nmvops && Nmvops == (KA_T)vp->v_op)
	    return(N_NM);
	else if (Prvops && Prvops == (KA_T)vp->v_op)
	    return(N_PROC);
	else if (Sckvops && Sckvops == (KA_T)vp->v_op)
	    return(N_SOCK);

#if	defined(HAS_AFS)
/*
 * Caution: this should be the last test in vop2ty().
 */
	else if (Avops) {
	    if (Avops == (KA_T)vp->v_op)
		return(N_AFS);
	    else
		return(-1);
	}
	if (vp->v_data || !vp->v_vfsp)
	    return(-1);
	switch (afs) {
	case -1:
	    return(-1);
	case 0:
	    if (!hasAFS(vp)) {
		afs = -1;
		return(-1);
	    }
	    afs = 1;
	    return(N_AFS);
	case 1:
	    if ((KA_T)vp->v_vfsp == AFSVfsp)
		return(N_AFS);
	}
	return(-1);

#else	/* !defined(HAS_AFS) */
	return(-1);
#endif	/* defined(HAS_AFS) */

}
