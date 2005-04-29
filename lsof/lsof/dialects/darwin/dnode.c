/*
 * dnode.c - Darwin node functions for lsof
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
static char *rcsid = "$Id: dnode.c,v 1.8 2004/03/10 23:50:16 abe Exp $";
#endif


#include "lsof.h"


/*
 * Local function prototypes
 */

#if	DARWINV<600
_PROTOTYPE(static int lkup_dev_tty,(dev_t *dr, dev_t *rdr, unsigned long *ir));
#endif	/* DARWINV<600 */


#if	DARWINV<600
/*
 * lkup_dev_tty() - look up /dev/tty
 */

static int
lkup_dev_tty(dr, rdr, ir)
	dev_t *dr;			/* place to return device number */
	dev_t *rdr;			/* place to return raw device number */
	unsigned long *ir;		/* place to return inode number */
{
	int i;

	readdev(0);
	for (i = 0; i < Ndev; i++) {
	    if (strcmp(Devtp[i].name, "/dev/tty") == 0) {
		*dr = DevDev;
		*rdr = Devtp[i].rdev;
		*ir = (unsigned long)Devtp[i].inode;
		return(1);
	    }
	}
	return(-1);
}
#endif	/* DARWINV<600 */


#if	DARWINV>=800
static int
readname(KA_T addr, char *buf, int buflen)
{
	int n = 0;

	/*
	 * Read the name, 32 characters at a time, until a NUL character
	 * has been read or the buffer has been filled.
	 */
	while (n < buflen) {
	    int	rl;

	    rl = buflen - n;
	    if (rl > 32) {
		rl = 32;
		buf[n + rl] = '\0';
	    }

	    if (kread(addr, &buf[n], rl)) {
		return 1;
	    }

	    rl = (int)strlen(&buf[n]);
	    if (rl < 32)
		return 0;

	    addr += rl;
	    n += rl;
	}

	return 0;
}

/*
 * vnode_getpath() - get file path from vnode
 *	adapted from build_path() (.../bsd/vfs/vfs_subr.c)
 */

static int
vnode_getpath(struct vnode *first_vp, char *buff, int buflen)
{
    struct vnode *vp = first_vp;
    struct vnode vb, *VP=&vb;
    struct mount mb, *MP=&mb;
    char *end;
    char *path = buff;
    int pathlen = buflen;

    end = &path[pathlen-1];
    *end = '\0';

    if (!vp
    ||  readvnode((KA_T)vp, VP)) {
//	fprintf(stderr, "vnode_getpath: no vnode address\n");
	return 0;
    }

    // if this is the root dir of a file system and there is no parent
    if (vp && (VP->v_flag & VROOT) && VP->v_mount) {
	// then if it's the root fs, just put in a '/' and get out of here
	if (kread((KA_T)VP->v_mount, (char *)&mb, sizeof(mb))) {
//	    fprintf(stderr, "vnode_getpath: no vnode mount\n");
	    return 0;
	}
	if (MP->mnt_flag & MNT_ROOTFS) {
	    *--end = '/';
	    goto out;
	} else {
	    // else just use the covered vnode to get the mount path
	    vp = MP->mnt_vnodecovered;
	    if (vp) {
		if (readvnode((KA_T)vp, VP)) {
//		    fprintf(stderr, "vnode_getpath: bad vnode address\n");
		    return 0;
		}
	    }
	}
    }

    while(vp && VP->v_parent != vp) {
	char v_name[MAXPATHLEN+1];
	char *str;
	int len;

	if (VP->v_name == NULL) {
	    if (VP->v_parent != NULL) {
		goto err;
	    }
	    break;
	}
	
	if (readname((KA_T)VP->v_name, v_name, MAXPATHLEN)) {
//	    fprintf(stderr, "vnode_getpath: bad v_name address\n");
	    goto err;
	}

	v_name[MAXPATHLEN] = '\0';
	str = &v_name[0];

	// count how long the string is
	for(len=0; *str; str++, len++)
	    /* nothing */;

	// check that there's enough space
	if ((end - path - 1) < len) {
	    if (path == buff) {
		char *oend = end;

		/*
		 * expand the size of the path buffer to see if we can
		 * get a path relative to the current working directory.
		 */
		pathlen += MAXPATHLEN;
		path = malloc(pathlen);
		end = &path[oend - buff + MAXPATHLEN];
		memmove(end, oend, &buff[buflen] - oend);
	    } else {
		// if the path won't fit in the buffer
		goto err;
	    }
	}

	// copy it backwards
	for(; len > 0; len--) {
	    *--end = *--str;
	}

	// put in the path separator
	*--end = '/';

	// walk up the chain (as long as we're not the root)  
	if (vp == first_vp && (VP->v_flag & VROOT)) {
	    if (VP->v_mount) {
		if (kread((KA_T)VP->v_mount, (char *)&mb, sizeof(mb))) {
//		    fprintf(stderr, "vnode_getpath: no vnode mount\n");
		    goto err;
		}
		if (MP->mnt_vnodecovered) {
		    vp = MP->mnt_vnodecovered;
		    if (vp) {
			if (readvnode((KA_T)vp, VP)) {
//			    fprintf(stderr, "vnode_getpath: bad vnode address\n");
			    goto err;
			}
			vp = VP->v_parent;
		    }
		} else {
		    vp = NULL;
		}
	    } else {
		vp = NULL;
	    }
	} else {
	    vp = VP->v_parent;
	}
	if (vp && readvnode((KA_T)vp, VP)) {
//	    fprintf(stderr, "vnode_getpath: bad vnode address\n");
	    goto err;
	}

	// check if we're crossing a mount point and
	// switch the vp if we are.
	if (vp && (VP->v_flag & VROOT) && VP->v_mount) {
	    if (kread((KA_T)VP->v_mount, (char *)&mb, sizeof(mb))) {
//		fprintf(stderr, "vnode_getpath: no vnode mount\n");
		goto err;
	    }
	    vp = MP->mnt_vnodecovered;
	    if (vp) {
		if (readvnode((KA_T)vp, VP)) {
//		    fprintf(stderr, "vnode_getpath: bad vnode address\n");
		    goto err;
		}
	    }
	}
    }

  out:
    /*
     * attempt to reduce long paths which are relative to the current
     * working directory.
     */
    if (path != buff) {
	static char cwd[MAXPATHLEN];
	static int cwdlen = 0;

	if (cwdlen == 0) {
	    if (!getcwd(cwd, sizeof(cwd))) {
		// if we could not get current working directory
//		fprintf(stderr, "vnode_getpath: getcwd failed\n");
		goto err;
	    }
	    cwdlen = strlen(cwd);
	    if ((cwd[cwdlen-1] != '/') && cwdlen < sizeof(cwd)) {
		cwd[cwdlen++] = '/';
	    }
	}

	if (strncmp(end, cwd, cwdlen) != 0) {
		// if the path is not relative to the current working directory
		goto err;
	}

	end += cwdlen;		// skip past cwd
	if ((&path[pathlen] - end) > buflen) {
		// if the relative path won't fit in the provided buffer
		goto err;
	}
    }

    // slide it down to the beginning of the buffer
    memmove(buff, end, &path[pathlen] - end);
    return 1;

  err:
    if (path != buff) {
	free(path);
    }
    buff[0] = '\0';
    return 0;
}
#endif	/* DARWINV>=800 */


/*
 * process_node() - process vnode
 */

void
process_node(va)
	KA_T va;			/* vnode kernel space address */
{
	dev_t dev, rdev;
	unsigned char devs = 0;
	unsigned char rdevs = 0;

#if	DARWINV<800
	struct devnode *d = (struct devnode *)NULL;
	struct devnode db;
	unsigned char lt;
	char dev_ch[32];
# if	defined(HASFDESCFS)
	struct fdescnode *f = (struct fdescnode *)NULL;
	struct fdescnode fb;
# endif	/* defined(HASFDESCFS) */
	static unsigned long fi;
	static dev_t fdev, frdev;
	static int fs = 0;
	struct inode *i = (struct inode *)NULL;
	struct inode ib;
	struct lockf lf, *lff, *lfp;
	struct nfsnode *n = (struct nfsnode *)NULL;
	struct nfsnode nb;
#endif	/* DARWINV<800 */

	char *ty;
	enum vtype type;
	struct vnode *v, vb;
	struct l_vfs *vfs;

#if	DARWINV<600
	struct hfsnode *h = (struct hfsnode *)NULL;
	struct hfsnode hb;
	struct hfsfilemeta *hm = (struct hfsfilemeta *)NULL;
	struct hfsfilemeta hmb;
#elif	DARWINV<800
	struct cnode *h = (struct cnode *)NULL;
	struct cnode hb;
	struct filefork *hf = (struct filefork *)NULL;
	struct filefork hfb;
#endif	/* DARWINV<800 */

#if	defined(HAS9660FS)
	dev_t iso_dev;
	int iso_dev_def = 0;
	unsigned long iso_ino, iso_sz;
	long iso_links;
	int iso_stat = 0;
#endif	/* defined(HAS9660FS) */

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
	type = v->v_type;

#if	defined(HASNCACHE)
	Lf->na = va;
# if	defined(HASNCVPID)
	Lf->id = v->v_id;
# endif	/* defined(HASNCVPID) */
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
		if (strcasecmp(vfs->typnm, "nfs") == 0)
		    Ntype = N_NFS;

#if	DARWINV<130
		else if (strcasecmp(vfs->typnm, "afpfs") == 0)
		    Ntype = N_AFPFS;
#endif	/* DARWINV<130 */

	    }
	}
	if (Ntype == N_REGLR) {
	    switch (v->v_type) {
	    case VFIFO:
		Ntype = N_FIFO;
		break;
	    default:
		break;
	    }
	}

#if	DARWINV<800
/*
 * Define the specific node pointer.
 */
	switch (v->v_tag) {

# if	DARWINV>120
	case VT_AFP:
 	    break;
# endif	/* DARWINV>120 */

# if	DARWINV>120
	case VT_CDDA:
	    break;
# endif	/* DARWINV>120 */

# if	DARWINV>120
	case VT_CIFS:
	    break;
# endif	/* DARWINV>120 */

	case VT_DEVFS:
	    if (!v->v_data
	    ||  kread((KA_T)v->v_data, (char *)&db, sizeof(db))) {
		(void) snpf(Namech, Namechl, "no devfs node: %#x", v->v_data);
		enter_nm(Namech);
		return;
	    }
	    d = &db;
	    break;

# if	defined(HASFDESCFS)
	case VT_FDESC:
	    if (!v->v_data
	    ||  kread((KA_T)v->v_data, (char *)&fb, sizeof(fb))) {
		(void) snpf(Namech, Namechl, "no fdesc node: %s",
			print_kptr((KA_T)v->v_data, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	    f = &fb;
	    break;
# endif	/* defined(HASFDESCFS) */

	case VT_HFS:

# if	DARWINV<130
	    if (Ntype != N_AFPFS) {
# endif	/* DARWINV<130 */

		if (!v->v_data
		||  kread((KA_T)v->v_data, (char *)&hb, sizeof(hb))) {
		    (void) snpf(Namech, Namechl, "no hfs node: %s",
			print_kptr((KA_T)v->v_data, (char *)NULL, 0));
		    enter_nm(Namech);
		    return;
		}
		h = &hb;

# if	DARWINV<600
		if (!h->h_meta
		||  kread((KA_T)h->h_meta, (char *)&hmb, sizeof(hmb))) {
		    (void) snpf(Namech, Namechl, "no hfs node metadata: %s",
			print_kptr((KA_T)v->v_data, (char *)NULL, 0));
		    enter_nm(Namech);
		    return;
		}
		hm = &hmb;
# else	/* DARWINV>=600 */
		if (v->v_type == VDIR)
		    break;
		if (h->c_rsrc_vp == v)
		    hf = h->c_rsrcfork;
		else
		    hf = h->c_datafork;
		if (!hf
		||  kread((KA_T)hf, (char *)&hfb, sizeof(hfb))) {
		    (void) snpf(Namech, Namechl, "no hfs node fork: %s",
			print_kptr((KA_T)v->v_data, (char *)NULL, 0));
		    enter_nm(Namech);
		    return;
		}
		hf = &hfb;
# endif	/* DARWINV<600 */

# if	DARWINV<130
	    }
# endif	/* DARWINV<130 */

	    break;

# if	defined(HAS9660FS)
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
# endif	/* defined(HAS9660FS) */

	case VT_NFS:
	    if (!v->v_data
	    ||  kread((KA_T)v->v_data, (char *)&nb, sizeof(nb))) {
		(void) snpf(Namech, Namechl, "no nfs node: %s",
			print_kptr((KA_T)v->v_data, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	    n = &nb;
	    break;

# if	DARWINV>120
	case VT_UDF:
	    break;
# endif	/* DARWINV>120 */

	case VT_UFS:
	    if (!v->v_data
	    ||  kread((KA_T)v->v_data, (char *)&ib, sizeof(ib))) {
		(void) snpf(Namech, Namechl, "no ufs node: %s",
			print_kptr((KA_T)v->v_data, (char *)NULL, 0));
		enter_nm(Namech);
		return;
	    }
	    i = &ib;
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

# if	DARWINV>120
	case VT_WEBDAV:
   	    break;
# endif	/* DARWINV>120 */

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
	if (n) {
	    dev = n->n_vattr.va_fsid;
	    devs = 1;
	} else if (i) {
	    dev = i->i_dev;
	    devs = 1;
	    if ((type == VCHR) || (type == VBLK)) {
		rdev = i->i_rdev ;
		rdevs = 1;
	    }

# if	defined(HASFDESCFS)
	} else if (f) {
	    if (f->fd_link
	    &&  !kread((KA_T)f->fd_link, Namech, Namechl -1))
		Namech[Namechl - 1] = '\0';

#  if	DARWINV<600
	    else if (f->fd_type == Fctty) {
		if (fs == 0)
		    fs = lkup_dev_tty(&fdev, &frdev, &fi);
		if (fs == 1) {
		    dev = fdev;
		    rdev = frdev;
		    devs = Lf->inp_ty = rdevs = 1;
		    Lf->inode = fi;
		}
	    }
#  endif	/* DARWINV<600 */
# endif	defined(HASFDESCFS)

	} else if (h) {

# if	DARWINV<600
	    dev = hm->h_dev;
# else	/* DARWINV>=600 */
	    dev = h->c_dev;
# endif	/* DARWINV<600 */

	    devs = 1;
	    if ((type == VCHR) || (type == VBLK)) {

# if	DARWINV<600
		rdev = hm->h_rdev;
# else	/* DARWINV>=600 */
		rdev = h->c_rdev;
# endif	/* DARWINV<600 */

		rdevs = 1;
	    }
	} else if (d) {
	    dev = DevDev;
	    devs = 1;
	    rdev = d->dn_typeinfo.dev;
	    rdevs = 1;
	}

# if	defined(HAS9660FS)
	else if (iso_stat && iso_dev_def) {
	    dev = iso_dev;
	    devs = Lf->inp_ty = 1;
	}
# endif	/* defined(HAS9660FS) */


/*
 * Obtain the inode number.
 */
	if (i) {
	    Lf->inode = (unsigned long)i->i_number;
	    Lf->inp_ty = 1;
	} else if (n) {
	    Lf->inode = (unsigned long)n->n_vattr.va_fileid;
	    Lf->inp_ty = 1;
	} else if (h) {

# if	DARWINV<600
	    Lf->inode = (unsigned long)hm->h_nodeID;
# else	/* DARWINV>=600 */
	    Lf->inode = (unsigned long)h->c_fileid;
# endif	/* DARWINV<600 */

	    Lf->inp_ty = 1;
	}

# if	defined(HAS9660FS)
	else if (iso_stat) {
	    Lf->inode = iso_ino;
	    Lf->inp_ty = 1;
	}
# endif	/* defined(HAS9660FS) */

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

# if	DARWINV<130
	    case N_AFPFS:
		break;
# endif	/* DARWINV<130 */

	    case N_REGLR:
		if (type == VREG || type == VDIR) {
		    if (i) {
			Lf->sz = (SZOFFTYPE)i->i_size;
			Lf->sz_def = 1;
		    } else if (h) {

# if	DARWINV<600
			Lf->sz = (type == VDIR) ? (SZOFFTYPE)hm->h_size
						: (SZOFFTYPE)h->fcbEOF;
# else	/* DARWINV>=600 */
			if (type == VDIR)
			    Lf->sz = (SZOFFTYPE)h->c_nlink * 128;
			else
			    Lf->sz = (SZOFFTYPE)hf->ff_size;
# endif	/* DARWINV<600 */

			Lf->sz_def = 1;
		    }

# if	defined(HAS9660FS)
		    else if (iso_stat) {
			Lf->sz = (SZOFFTYPE)iso_sz;
			Lf->sz_def = 1;
		    }
# endif	/* defined(HAS9660FS) */

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

# if	DARWINV<130
	    case N_AFPFS:
		break;
# endif	/* DARWINV<130 */

	    case N_REGLR:
		if (i) {
		    Lf->nlink = (long)i->i_nlink;
		    Lf->nlink_def = 1;
		} else if (h) {

# if	DARWINV<600
		    Lf->nlink = (long)hm->h_nlink;
# else	/* DARWINV>=600 */
		    Lf->nlink = (long)h->c_nlink;
# endif	/* DARWINV>=600 */

		    Lf->nlink_def = 1;
		}

# if	defined(HAS9660FS)
		else if (iso_stat) {
		    Lf->nlink = iso_links;
		    Lf->nlink_def = 1;
		}
# endif	/* defined(HAS9660FS) */

		break;
	    }
	    if (Lf->nlink_def && Nlink && (Lf->nlink < Nlink))
		Lf->sf |= SELNLINK;
	}

#else	/* DARWINV>=800 */

	if (vnode_getpath((struct vnode *)va, Lf->path, MAXPATHLEN)) {
	    struct stat	sb;

	    if (stat(Lf->path, &sb) == 0) {
		if (Foffset) {
		    Lf->off_def = 1;	/* show file offset */
		} else {
		    switch (Ntype) {
		    case N_FIFO:
			if (!Fsize)
			    /* show file offset */
			    Lf->off_def = 1;
			break;
		    case N_NFS:
		    case N_REGLR:
			if (type == VREG || type == VDIR) {
			    /* file size, in bytes */
			    Lf->sz = sb.st_size;
			    Lf->sz_def = 1;
			}
			else if ((type == VCHR || type == VBLK) && !Fsize) {
			    /* show file offset */
			    Lf->off_def = 1;
			}
			break;
		    }
		}

		/* inode's number */
		Lf->inode = sb.st_ino;
		Lf->inp_ty = 1; 

		if (Fnlink) {
		    /* number or hard links to the file */
		    Lf->nlink = sb.st_nlink;
		    Lf->nlink_def = 1;
		}

		/* device inode resides on */
		switch (v->v_tag) {
		case VT_DEVFS:
		    Lf->hasPath = 0;
		    dev = DevDev;
		    devs = 1;
		    break;
		default :
		    Lf->hasPath = 1;
		    dev = sb.st_dev;
		    devs = 1;
		    break;
		}

		/* device type, for special file inode */
		if ((type == VCHR) || (type == VBLK)) {
		    rdev = sb.st_rdev;
		    rdevs = 1;
		}
	    }
	}

#endif	/* DARWINV>=800 */

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
	if (Sfile && is_file_named((char *)NULL,
				   ((type == VCHR) || (type == VBLK) ? 1
								     : 0)))
	    Lf->sf |= SELNM;
/*
 * Enter name characters.
 */
	if (Namech[0])
	    enter_nm(Namech);
}


#if	DARWINV>=800

/*      
 * print_vnode_path() - print a vnode file path
 *
 * return: 1 if path printed
 *
 * This function is called by the name HASPRIVNMCACHE from printname().
 */

int
print_vnode_path(lf)
        struct lfile *lf;               /* file whose name is to be printed */
{
	if (lf->hasPath) {
	    safestrprt(lf->path, stdout, 0);
	    return(1);
	}
	return (0);

}

/*
 * process_pipe() - process a file structure whose type is DTYPE_PIPE
 */

void
process_pipe(pa)
	KA_T pa;			/* pipe structure address */
{
	char dev_ch[32];

	(void) snpf(Lf->type, sizeof(Lf->type), "PIPE");
	(void) snpf(dev_ch, sizeof(dev_ch), "%#x", pa);
	enter_dev_ch(dev_ch);
	Namech[0] = '\0';
}

#endif	/* DARWINV>=800 */
