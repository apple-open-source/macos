/*
 * dnode2.c - Solaris node functions for lsof
 *
 * This module must be separate to keep separate the multiple kernel inode
 * structure definitions.
 */


/*
 * Copyright 1997 Purdue Research Foundation, West Lafayette, Indiana
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
static char *rcsid = "$Id: dnode2.c,v 1.17 2001/07/05 12:27:57 abe Exp $";
#endif


#include "lsof.h"

#if	defined(HASVXFS)

# if	defined(HASVXFSUTIL)
#include <vxfsutil.h>
#define	EMSGPFX	"vx_inode: "
_PROTOTYPE(static char *add2em,(char * em, char *fmt, char *arg));
_PROTOTYPE(static char *ckptr,(char * em, char *ptr, int len, int xlen,
				  char *nm));
_PROTOTYPE(static char *getioffs,(char **vx,  int *vxl,
				  char **dev, int *devl,
				  char **ino, int *inol,
				  char **nl,  int *nll,
				  char **sz,  int *szl));
# else	/* !defined(HASVXFSUTIL) */
#  if	defined(HASVXFS_FS_H) && !defined(HASVXFS_VX_INODE)
#undef	fs_bsize
#include <sys/fs/vx_fs.h>
#  endif	/* defined(HASVXFS_FS_H) && !defined(HASVXFS_VX_INODE) */

#  if	HASVXFS_SOL_H
#include <sys/fs/vx_sol.h>
#  endif	/* defined(HSVXFS_SOL_H) */

#  if	defined(HASVXFS_SOLARIS_H) && defined(HASVXFS_U64_T)
#include <sys/fs/vx_solaris.h>
#  endif	/* defined(HASVXFS_SOLARIS_H) && defined(HASVXFS_U64_T) */

#  if	defined(HASVXFS_MACHDEP_H)
#   if	defined(HASVXFS_OFF32_T) && solaris>=70000
#define	off32_t	VXFS_off32_t
#   endif	/* defined(HASVXFS_OFF32_T) && solaris>=70000 */
#include <sys/fs/vx_machdep.h>
#  endif	/* defined(HASVXFS_MACHDEP_H) */

#  if	defined(HASVXFS_SOLARIS_H)
struct kdm_vnode {			/* dummy for <sys/fs/vx_inode.h> */
    int d1;
};
#undef	fs_bsize
#define	uint16_t	VXFS_uint16_t

#   if	defined(HASVXFS_OFF64_T)
#define	off64_t		VXFS_off64_t
#   endif	/* defined(HASVXFS_OFF64_T) */

#  if	defined(HASVXFS_SOLARIS_H) && !defined(HASVXFS_U64_T)
#include <sys/fs/vx_solaris.h>
#  endif	/* defined(HASVXFS_SOLARIS_H) && !defined(HASVXFS_U64_T) */

#include <sys/fs/vx_layout.h>
#include <sys/fs/vx_const.h>
#include <sys/fs/vx_mlink.h>
#  endif	/* defined(HASVXFS_SOLARIS_H) */

#include <sys/fs/vx_inode.h>
# endif	/* defined(HASVXFSUTIL) */


# if	defined(HASVXFSUTIL)
/*
 * add2em() - add to error message
 */

static char *
add2em(em, fmt, arg)
	char *em;			/* current error message */
	char *fmt;			/* message format */
	char *arg;			/* format's single string argument */
{
	MALLOC_S al, eml, nl;
	char msg[1024];
	MALLOC_S msgl = (MALLOC_S)sizeof(msg);

	(void) snpf(msg, msgl, fmt, arg);
	msg[msgl - 1] = '\0';
	nl = (MALLOC_S)strlen(msg);
	if (!em) {
	    al = (MALLOC_S)strlen(EMSGPFX) + nl + 1;
	    em = (char *)malloc((MALLOC_S)al);
	    eml = (MALLOC_S)0;
	} else {
	    if (!(eml = (MALLOC_S)strlen(em))) {
		(void) fprintf(stderr, "%s: add2em: previous message empty\n",
		    Pn);
		Exit(1);
	    }
	    al = eml + nl + 3;
	    em = (char *)realloc((MALLOC_P *)em, al);
	}
	if (!em) {
	    (void) fprintf(stderr, "%s: no VxFS error message space\n", Pn);
	    Exit(1);
	}
	(void) snpf(em + eml, al - eml, "%s%s%s",
	    eml ? "" : EMSGPFX,
	    eml ? "; " : "",
	    msg);
	return(em);
}


/*
 * ckptr() - check pointer and length
 */

static char *
ckptr(em, ptr, len, xlen, nm)
	char *em;			/* pointer to previous error message */
	char *ptr;			/* pointer to check */
	int len;			/* pointer's value length */
	int xlen;			/* expected length */
	char *nm;			/* element name */
{

#if	defined(_LP64)
#define	PTR_CAST	unsigned long long
#else	/* !defined(_LP64) */
#define	PTR_CAST	unsigned long
#endif	/* defined(_LP64) */

	PTR_CAST m;

	if (!ptr)
	    return(add2em(em, "no %s pointer", nm ? nm : "(null)"));
	if (len != xlen)
	    return(add2em(em, "wrong %s size", nm ? nm : "(null)"));
	if ((m = (PTR_CAST)(len - 1)) < (PTR_CAST)1)
	    return(em);
	if ((PTR_CAST)ptr & m)
	    return(add2em(em, "%s misaligned", nm ? nm : "(null)"));
	return(em);
}


/*
 * getioffs() - get the vx_inode offsets
 */

static char *
getioffs(vx, vxl, dev, devl, ino, inol, nl, nll, sz, szl)
	char **vx;		/* pointer to allocated vx_inode space */
	int *vxl;		/* sizeof(*vx) */
	char **dev;		/* pointer to device number element of *vx */
	int *devl;		/* sizeof(*dev) */
	char **ino;		/* pointer to node number element of *vx */
	int *inol;		/* sizeof(*ino) */
	char **nl;		/* pointer to nlink element of *vx */
	int *nll;		/* sizeof(*nl) */
	char **sz;		/* pointer to size element of *vx */
	int *szl;		/* sizeof(*sz) */
{
	struct vx_ioffsets i;
	char *tv;
	int tvl;

	(void) restoregid();
	if (vxfsu_get_ioffsets(&i, sizeof(i)))
	    return(add2em((char *)NULL, "%s error", "vxfsu_get_ioffsets"));
	tvl = (int)(i.ioff_dev + i.ioff_dev_sz);
	if ((i.ioff_nlink + i.ioff_nlink_sz) > tvl)
	    tvl = (int)(i.ioff_nlink + i.ioff_nlink_sz);
	if ((i.ioff_number + i.ioff_number_sz) > tvl)
	    tvl = (int)(i.ioff_number + i.ioff_number_sz);
	if ((i.ioff_size + i.ioff_size_sz) > tvl)
	    tvl = (int)(i.ioff_size + i.ioff_size_sz);
	if (!tvl)
	    return(add2em((char *)NULL, "zero length %s", "vx_inode"));
	if (!(tv = (char *)malloc((MALLOC_S)tvl))) {
	    (void) fprintf(stderr, "%s: no vx_inode space\n", Pn);
	    Exit(1);
	}
	*vx = tv;
	*vxl = tvl;
	*dev = tv + i.ioff_dev;
	*devl = (int)i.ioff_dev_sz;
	*ino = tv + i.ioff_number;
	*inol = (int)i.ioff_number_sz;
	*nl = tv + i.ioff_nlink;
	*nll = (int)i.ioff_nlink_sz;
	*sz = tv + i.ioff_size;
	*szl = (int)i.ioff_size_sz;
	return((char *)NULL);
}
# endif	/* defined(HASVXFSUTIL) */


/*
 * read_vxnode() - read Veritas file system inode information
 */

int
read_vxnode(va, v, vfs, li, vnops)
	KA_T va;			/* containing vnode's address */
	struct vnode *v;		/* containing vnode */
	struct l_vfs *vfs;		/* local vfs structure */
	struct l_ino *li;		/* local inode value receiver */
	KA_T *vnops;			/* table of VxFS v_op values */
{
	struct vnode cv;
	char tbuf[32];

# if	defined(HASVXFS_VX_INODE)
	struct vx_inode vx;
	int vxl = (int)sizeof(vx);
	dev_t *vxn_dev = (dev_t *)&vx.i_dev;
	int *vxn_nlink = (int *)&vx.i_nlink;
	unsigned int *vxn_ino = (unsigned int *)&vx.i_number;
	SZOFFTYPE *vxn_sz = (SZOFFTYPE *)&vx.i_size;
	char *vxp = (char *)&vx;
# else	/* !defined(HASVXFS_VX_INODE) */
#  if	defined(HASVXFSUTIL)
	static char *em = (char *)NULL;
	int devl, inol, nll, szl;
	static char *vxp = (char *)NULL;
	static int vxl = 0;
	static dev_t *vxn_dev = (dev_t *)NULL;
	static int *vxn_nlink = (int *)NULL;
	static unsigned int *vxn_ino = (unsigned int *)NULL;
	static SZOFFTYPE *vxn_sz = (SZOFFTYPE *)NULL;
#  else	/* !defined(HASVXFSUTIL) */
	struct inode vx;
	int vxl - sizeof(vx);
	dev_t *vxn_dev = (dev_t *)&vx.i_dev;
	int *vxn_nlink = (int *)&vx.i_nlink;
	long *vxn_ino = (long *)&vx.i_number;
	SZOFFTYPE *vxn_sz = (SZOFFTYPE *)&vx.i_size;
	char *vxp = (char &)&vx;
#  endif	/* defined(HASVXFSUTIL) */
# endif	/* defined(HASVXFS_VX_INODE) */

	li->dev_def = li->ino_def = li->nl_def = li->rdev_def = li->sz_def = 0;
/*
 * See if this is vnode is served by fdd_chain_vnops.  If it is, its
 * v_data pointer leads to the "real" vnode.
 */
	if (v->v_data && v->v_op && (VXVOP_FDDCH < VXVOP_NUM)
	&&  vnops[VXVOP_FDDCH] && ((KA_T)v->v_op == vnops[VXVOP_FDDCH]))
	{
	    if (kread((KA_T)v->v_data, (char *)&cv, sizeof(cv))) {
		(void) snpf(Namech, Namechl,
		    "node at %s: can't read real vx vnode: %s",
		    print_kptr(va, tbuf, sizeof(tbuf)),
		    print_kptr((KA_T)v->v_data, (char *)NULL, 0));
		enter_nm(Namech);
		return(1);
	    }

# if	defined(HASNCACHE)
	    Lf->na = (KA_T)v->v_data;
# endif	/* defined(HASNCACHE) */

	    *v = cv;
	    Ntype = vop2ty(v);
	}

#  if	defined(HASVXFSUTIL)
/*
 * If libvxfsutil[64].a is in use, establish the vx_inode size and the
 * locations and sizes of its device, link count, node number, and size
 * elements.
 *
 * If an error was detected while determining the vx_inode values, repeat
 * the error explanation in the NAME column.
 */
	if (!vxp && !em) {
	    em = getioffs(&vxp, &vxl,
			  (char **)&vxn_dev, &devl,
			  (char **)&vxn_ino, &inol,
			  (char **)&vxn_nlink, &nll,
			  (char **)&vxn_sz, &szl);
	    if (!em) {

	    /*
	     * Check the returned pointers and their sizes.
	     */
		em = ckptr(em, (char *)vxn_dev, devl, sizeof(dev_t), "dev");
		em = ckptr(em, (char *)vxn_ino, inol, sizeof(int), "ino");
		em = ckptr(em, (char *)vxn_nlink, nll, sizeof(int), "nlink");
		em = ckptr(em, (char *)vxn_sz, szl, sizeof(SZOFFTYPE), "sz");
	    }
	}
	if (em) {
	    (void) snpf(Namech, Namechl, "%s", em);
	    (void) enter_nm(Namech);
	    return(1);
	}
#  endif	/* !defined(HASVXFSUTIL) */

/*
 * Read vnode's vx_inode.
 */
	if (!v->v_data || kread((KA_T)v->v_data, vxp, vxl)) {
	    (void) snpf(Namech, Namechl, "node at %s: can't read vx_inode: %s",
		print_kptr(va, tbuf, sizeof(tbuf)),
		print_kptr((KA_T)v->v_data, (char *)NULL, 0));
	    (void) enter_nm(Namech);
	    return(1);
	}
/*
 * Return device number, inode number, link count, raw device number, and size.
 */
	if (vfs && vfs->fsname) {
	    li->dev = (dev_t)vfs->dev;
	    li->dev_def = 1;
	} else if (vxn_dev) {
	    li->dev = (dev_t)*vxn_dev;
	    li->dev_def = 1;
	}
	if (vxn_ino) {
	    li->ino = (unsigned long)*vxn_ino;
	    li->ino_def = 1;
	}
	if (vxn_nlink) {
	    li->nl = (long)*vxn_nlink;
	    li->nl_def = 1;
	}
	li->rdev = v->v_rdev;
	li->rdev_def = 1;
	if (vxn_sz) {
	    li->sz = (SZOFFTYPE)*vxn_sz;
	    li->sz_def = 1;
	}
	return(0);
}

#endif	/* defined(HASVXFS) */
