/*
 * dproto.h - Solaris function prototypes for lsof
 *
 * The _PROTOTYPE macro is defined in the common proto.h.
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
 * $Id: dproto.h,v 1.13 2001/07/05 12:26:21 abe Exp $
 */


_PROTOTYPE(extern void completevfs,(struct l_vfs *vfs, dev_t *dev));
_PROTOTYPE(extern int is_file_named,(char *p, int nt, enum vtype vt, int ps));
_PROTOTYPE(extern struct l_vfs *readvfs,(KA_T ka, struct vfs *la, struct vnode *lv));
_PROTOTYPE(extern int vop2ty,(struct vnode *vp));

#if	defined(HAS_AFS)
_PROTOTYPE(extern struct vnode *alloc_vcache,(void));
_PROTOTYPE(extern void ckAFSsym,(struct nlist *nl));
_PROTOTYPE(extern int hasAFS,(struct vnode *vp));
_PROTOTYPE(extern int readafsnode,(KA_T va, struct vnode *v, struct afsnode *an));
#endif	/* defined(HAS_AFS) */

#if	defined(HASDCACHE)
_PROTOTYPE(extern int rw_clone_sect,(int m));
_PROTOTYPE(extern void clr_sect,(void));
_PROTOTYPE(extern int rw_pseudo_sect,(int m));
#endif	/* defined(HASDCACHE) */

#if	defined(HASVXFS)
_PROTOTYPE(extern int read_vxnode,(KA_T va, struct vnode *v, struct l_vfs *vfs,
				   struct l_ino *li, KA_T *vnops));
#endif	/* defined(HASVXFS) */

_PROTOTYPE(extern void close_kvm,(void));
_PROTOTYPE(extern void open_kvm,(void));
_PROTOTYPE(extern void process_socket,(KA_T sa, char *ty));
_PROTOTYPE(extern void read_clone,(void));

#if	solaris<20500
_PROTOTYPE(extern int get_max_fd,(void));
#endif	/* solaris<20500 */

#if	defined(WILLDROPGID)
_PROTOTYPE(extern void restoregid,(void));
#endif	/* defined(WILLDROPGID) */
