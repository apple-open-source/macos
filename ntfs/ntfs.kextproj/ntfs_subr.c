/*
 * Copyright (c) 2003-2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2004 Apple Computer, Inc.  All Rights Reserved.
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*	$NetBSD: ntfs_subr.c,v 1.23 1999/10/31 19:45:26 jdolecek Exp $	*/

/*-
 * Copyright (c) 1998, 1999 Semen Ustimenko (semenu@FreeBSD.org)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/fs/ntfs/ntfs_subr.c,v 1.25 2002/04/04 21:03:19 jhb Exp $
 */

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/kernel.h>
#include <sys/vnode.h>
#include <sys/mount.h>
#include <sys/ubc.h>
#include <sys/buf.h>
#include <sys/file.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/utfconv.h>
#include <libkern/OSMalloc.h>
#include <string.h>

/* #define NTFS_DEBUG 1 */
#include "ntfs.h"
#include "ntfsmount.h"
#include "ntfs_inode.h"
#include "ntfs_vfsops.h"
#include "ntfs_subr.h"
#include "ntfs_compr.h"
#include "ntfs_ihash.h"

MALLOC_DEFINE(M_NTFSNTVATTR, "NTFS vattr", "NTFS file attribute information");
MALLOC_DEFINE(M_NTFSRDATA, "NTFS res data", "NTFS resident data");
MALLOC_DEFINE(M_NTFSRUN, "NTFS vrun", "NTFS vrun storage");

static int ntfs_ntlookupattr(struct ntfsmount *, const char *, int, int *, char **);
static int ntfs_findvattr(struct ntfsmount *, struct ntnode *, struct ntvattr **, struct ntvattr **, u_int32_t, const char *, size_t, cn_t, proc_t);
static int ntfs_attrtontvattr( struct ntfsmount *, struct ntvattr **, struct attr * );
static void ntfs_freentvattr( struct ntvattr * );
static void ntfs_ntref(struct ntnode *);
static void ntfs_ntrele(struct ntnode *);
static int ntfs_procfixups( struct ntfsmount *, u_int32_t, caddr_t, size_t );
static int ntfs_readattr_plain( struct ntfsmount *, struct ntnode *, u_int32_t, char *, off_t, size_t, void *,size_t *, struct uio *, proc_t);
static int ntfs_readntvattr_plain( struct ntfsmount *, struct ntnode *, struct ntvattr *, off_t, size_t, void *,size_t *, struct uio *);
static int ntfs_runtovrun( cn_t **, cn_t **, u_long *, u_int8_t *);
static int ntfs_unistrcasecmp(		/* Case-insensitive compare */
	size_t s_len,
	const u_int16_t *s,		/* Note: little endian */
	size_t t_len,
	const u_int16_t *t);	/* Note: native endian */
static int ntfs_unistrcmp(			/* Case-sensitive compare */
	size_t s_len,
	const u_int16_t *s,		/* Note: little endian */
	size_t t_len,
	const u_int16_t *t);	/* Note: native endian */

/* table for mapping Unicode chars into uppercase; it's filled upon first
 * ntfs mount, freed upon last ntfs umount */
static u_int16_t *ntfs_toupper_tab;
#define NTFS_TOUPPER(ch)	(ntfs_toupper_tab[(ch)])
static signed int ntfs_toupper_usecount;

/* support macro for ntfs_ntvattrget() */
#define NTFS_AALPCMP(aalp,type,namelen,name) (				\
  (le32toh(aalp->al_type) == type) && (aalp->al_namelen == namelen) &&		\
  !ntfs_unistrcmp(aalp->al_namelen,aalp->al_name,namelen,name) )

/*
 * 
 */
__private_extern__
int
ntfs_ntvattrrele(vap)
	struct ntvattr * vap;
{
	dprintf(("ntfs_ntvattrrele: ino: %d, type: 0x%x\n",
		 vap->va_ip->i_number, vap->va_type));

	ntfs_ntrele(vap->va_ip);

	return (0);
}

/*
 * find the attribute in the ntnode
 *
 * If the desired attribute is found, *vapp points to it, and it will
 * be referenced (use count incremented).
 *
 * If the desired attribute is not found, but an attribute list (i.e.
 * non-resident attributes) is found, a pointer to the attribute list
 * is returned in *lvapp.  The attribute list is not referenced.
 *
 * NOTE: if the desired attribute was found, you cannot rely on the
 * return value of *lvapp (it may be NULL, or may point to an
 * attribute list).
 *
 * Function result:
 *	 0 => desired attribute was found
 *	-1 => desired attribute was not found
 */
static int
ntfs_findvattr(ntmp, ip, lvapp, vapp, type, name, namelen, vcn, p)
	struct ntfsmount *ntmp;
	struct ntnode *ip;
	struct ntvattr **lvapp, **vapp;
	u_int32_t type;
	const char *name;
	size_t namelen;
	cn_t vcn;
	proc_t p;
{
	int error;
	struct ntvattr *vap;

	if((ip->i_flag & IN_LOADED) == 0) {
		dprintf(("ntfs_findvattr: node not loaded, ino: %d\n",
		       ip->i_number));
		error = ntfs_loadntnode(ntmp,ip,p);
		if (error) {
			printf("ntfs_findvattr: FAILED TO LOAD INO: %d\n",
			       ip->i_number);
			return (error);
		}
	}

	*lvapp = NULL;
	*vapp = NULL;
	LIST_FOREACH(vap, &ip->i_valist, va_list) {
		ddprintf(("ntfs_findvattr: type: 0x%x, vcn: %d - %d\n", \
			  vap->va_type, (u_int32_t) vap->va_vcnstart, \
			  (u_int32_t) vap->va_vcnend));
		if ((vap->va_type == type) &&
		    (vap->va_vcnstart <= vcn) && (vap->va_vcnend >= vcn) &&
		    (vap->va_namelen == namelen) &&
		    (strncmp(name, vap->va_name, namelen) == 0)) {
			*vapp = vap;
			ntfs_ntref(vap->va_ip);
			return (0);
		}
		if (vap->va_type == NTFS_A_ATTRLIST)
			*lvapp = vap;
	}

	return (-1);
}

/*
 * Search attribute specifed in ntnode (load ntnode if nessecary).
 * If not found but ATTR_A_ATTRLIST present, read it in and search throught.
 * VOP_VGET node needed, and lookup througth it's ntnode (load if nessesary).
 *
 * ntnode should be locked
 */
__private_extern__
int
ntfs_ntvattrget(
		struct ntfsmount * ntmp,
		struct ntnode * ip,
		u_int32_t type,
		const char *name,
		cn_t vcn,
		proc_t p,
		struct ntvattr ** vapp)
{
	struct ntvattr *lvap = NULL;
	struct attr_attrlist *aalp;
	struct attr_attrlist *nextaalp;
	vnode_t			newvp;
	struct ntnode  *newip;
	caddr_t         alpool;
	size_t			namelen, len;
	int             error;
	u_int16_t		reclen;
	u_int32_t       al_inumber;
	size_t			uninamelen;
	u_int16_t		uniname[NTFS_MAXATTRNAME];

	*vapp = NULL;

	if (name) {
		dprintf(("ntfs_ntvattrget: " \
			 "ino: %d, type: 0x%x, name: %s, vcn: %d\n", \
			 ip->i_number, type, name, (u_int32_t) vcn));
		namelen = strlen(name);
	} else {
		dprintf(("ntfs_ntvattrget: " \
			 "ino: %d, type: 0x%x, vcn: %d\n", \
			 ip->i_number, type, (u_int32_t) vcn));
		name = "";
		namelen = 0;
	}

	error = ntfs_findvattr(ntmp, ip, &lvap, vapp, type, name, namelen, vcn, p);
	if (error >= 0)
		return (error);

	/* If we get here, the attribute was not found.  Try attribute list. */
	if (!lvap) {
		dprintf(("ntfs_ntvattrget: UNEXISTED ATTRIBUTE: " \
		       "ino: %d, type: 0x%x, name: %s, vcn: %d\n", \
		       ip->i_number, type, name, (u_int32_t) vcn));
		return (ENOENT);
	}
        
	/* Read the $ATTRIBUTE_LIST into memory (little endian) */
	len = lvap->va_datalen;
	MALLOC(alpool, caddr_t, len, M_TEMP, M_WAITOK);
	error = ntfs_readntvattr_plain(ntmp, ip, lvap, 0, len, alpool, &len,
			NULL);  /* Assumes non-compressed */
	if (error)
		goto out;

	/* Convert the attribute name to UTF-16 */
	error = utf8_decodestr(name, namelen, uniname, &uninamelen, sizeof(uniname), 0, UTF_PRECOMPOSED);
	if (error)
		goto out;
	uninamelen /= sizeof(u_int16_t);

	/* Scan $ATTRIBUTE_LIST for requested attribute */
	aalp = (struct attr_attrlist *) alpool;
	nextaalp = NULL;

	for(; len > 0; aalp = nextaalp) {
		dprintf(("ntfs_ntvattrget: "
			 "attrlist: ino: %d, attr: 0x%x, vcn: %d\n",
			 aalp->al_inumber, le32toh(aalp->al_type),
			 (u_int32_t) le64toh(aalp->al_vcnstart)));

                reclen = le16toh(aalp->reclen);
		if (len > reclen) {
			nextaalp = (struct attr_attrlist *) ((caddr_t) aalp + reclen);
		} else {
			nextaalp = NULL;
		}
		len -= reclen;

		if (!NTFS_AALPCMP(aalp, type, uninamelen, uniname) ||
		    (nextaalp && (le64toh(nextaalp->al_vcnstart) <= vcn) &&
		     NTFS_AALPCMP(nextaalp, type, uninamelen, uniname)))
			continue;

		dprintf(("ntfs_ntvattrget: attribute in ino: %d\n",
				 aalp->al_inumber));

		/* this is not a main record, so we can't use just plain
		   vget() */
                al_inumber = le32toh(aalp->al_inumber);
		error = ntfs_vgetex(ntmp->ntm_mountp, al_inumber,
				NULL, NULL, VNON,	/* No parent, name, or vtype */
				NTFS_A_DATA, NULL,
				VG_EXT, p, &newvp);
		if (error) {
			printf("ntfs_ntvattrget: CAN'T VGET INO: %d\n",
			       al_inumber);
			goto out;
		}
		newip = VTONT(newvp);
		/* XXX have to lock ntnode */
		error = ntfs_findvattr(ntmp, newip, &lvap, vapp,
				type, name, namelen, vcn, p);
		vnode_put(newvp);
		if (error == 0)
			goto out;
		printf("ntfs_ntvattrget: ATTRLIST ERROR.\n");
		break;
	}
	error = ENOENT;

	dprintf(("ntfs_ntvattrget: UNEXISTED ATTRIBUTE: " \
	       "ino: %d, type: 0x%x, name: %.*s, vcn: %d\n", \
	       ip->i_number, type, (int) namelen, name, (u_int32_t) vcn));
out:
	FREE(alpool, M_TEMP);
	return (error);
}

/*
 * Read ntnode from disk, make ntvattr list.
 *
 * ntnode should be locked
 */
__private_extern__
int
ntfs_loadntnode(
	      struct ntfsmount * ntmp,
	      struct ntnode * ip,
	      proc_t p)
{
	struct filerec  *mfrp;
	size_t		bufsize;
	daddr_t         bn;
	int		error,off;
	struct attr    *ap;
	struct ntvattr *nvap;
	uint32_t	reclen;

	dprintf(("ntfs_loadntnode: loading ino: %d\n",ip->i_number));

        /* Allocate space for a single MFT record */
        bufsize = ntfs_bntob(ntmp->ntm_bpmftrec);
	mfrp = OSMalloc(bufsize, ntfs_malloc_tag);
	if (mfrp == NULL)
		return ENOMEM;
	
	if (ip->i_number < NTFS_SYSNODESNUM) {
		struct buf     *bp;

		dprintf(("ntfs_loadntnode: read system node\n"));

		/* Calculate sector where ip's MFT record starts */
		bn = ntfs_cntobn(ntmp->ntm_mftcn) +
			ntmp->ntm_bpmftrec * ip->i_number;

		/* Read in ip's MFT record */
		error = (int)buf_meta_bread(ntmp->ntm_devvp,
				       bn, bufsize,
				       NOCRED, &bp);	/*¥ NOCRED */
		if (error) {
			printf("ntfs_loadntnode: BREAD FAILED\n");
			buf_brelse(bp);
			goto out;
		}
		/* Copy the MFT record (little endian) */
		memcpy(mfrp, (char *)buf_dataptr(bp), bufsize);
		buf_brelse(bp);
	} else {
		vnode_t vp;

		vp = ntmp->ntm_sysvn[NTFS_MFTINO];
		
		/* Read the MFT record (little endian) */
		error = ntfs_readattr(ntmp, VTONT(vp), NTFS_A_DATA, NULL,
			       ip->i_number * bufsize,
			       bufsize, mfrp, NULL, p);
		if (error) {
			printf("ntfs_loadntnode: ntfs_readattr failed\n");
			goto out;
		}
	}

	/* Check if magic and fixups are correct */
	error = ntfs_procfixups(ntmp, NTFS_FILEMAGIC, (caddr_t)mfrp, bufsize);
	if (error) {
		printf("ntfs_loadntnode: BAD MFT RECORD %d\n",
		       (u_int32_t) ip->i_number);
		goto out;
	}

	dprintf(("ntfs_loadntnode: load attrs for ino: %d\n",ip->i_number));
	off = le16toh(mfrp->fr_attroff);
	if (off > bufsize) {
		printf("ntfs_loadntnode: offset of first attribute too big "
			"(MFT record #%d, offset %d)\n", ip->i_number, off);
		error = EIO;
		goto out;
	}
	ap = (struct attr *) ((caddr_t)mfrp + off);

	LIST_INIT(&ip->i_valist);
	
	while (ap->a_hdr.a_type != -1) {	/* -1 is same for big/little endian */
		error = ntfs_attrtontvattr(ntmp, &nvap, ap);
		if (error)
			break;
		nvap->va_ip = ip;

		LIST_INSERT_HEAD(&ip->i_valist, nvap, va_list);

		reclen = le32toh(ap->a_hdr.reclen);
		if (reclen > bufsize || off+reclen > bufsize) {
			printf("ntfs_loadntnode: attribute too big "
				"(MFT record #%d, offset %d, size %u)\n",
				ip->i_number, off, reclen);
			error = EIO;
			goto out;
		}
		off += reclen;
		ap = (struct attr *) ((caddr_t)mfrp + off);
	}
	if (error) {
		printf("ntfs_loadntnode: failed to load attr ino: %d\n",
		       ip->i_number);
		goto out;
	}

	ip->i_mainrec = le64toh(mfrp->fr_mainrec);
	ip->i_nlink = le16toh(mfrp->fr_nlink);
	ip->i_frflag = le16toh(mfrp->fr_flags);

	ip->i_flag |= IN_LOADED;

out:
	OSFree(mfrp, bufsize, ntfs_malloc_tag);
	return (error);
}
		
/*
 * Routine locks ntnode and increase usecount, just opposite of
 * ntfs_ntput().
 */
__private_extern__
int
ntfs_ntget(ip)
	struct ntnode *ip;
{
	dprintf(("ntfs_ntget: get ntnode %d: %p, usecount: %d\n",
		ip->i_number, ip, ip->i_usecount));

	ip->i_usecount++;
	lck_mtx_lock(ip->i_lock);

	return 0;
}

/*
 * Routine search ntnode in hash, if found: lock, inc usecount and return.
 * If not in hash allocate structure for ntnode, prefill it, lock,
 * inc count and return.
 *
 * ntnode returned locked
 */
__private_extern__
int
ntfs_ntlookup(
	   struct ntfsmount * ntmp,
	   ino_t ino,
	   struct ntnode ** ipp)
{
	struct ntnode *ip;
	struct ntnode *new_node;

	dprintf(("ntfs_ntlookup: looking for ntnode %d\n", ino));

	/*
	 * Allocate a new node first, in case we need one.  This way, we won't
	 * block if we need to insert a new ntnode in the hash.  That way, we
	 * can take advantage of the VFS-provided funnel instead of needing
	 * a mutex to protect the hash.
	 */
	new_node = OSMalloc(sizeof(struct ntnode), ntfs_malloc_tag);

loop:
	if ((ip = ntfs_nthashlookup(ntmp->ntm_dev, ino)) != NULL) {
		if (ISSET(ip->i_flag, IN_ALLOC)) {
			/* inode is being initialized; wait for it to complete */
			SET(ip->i_flag, IN_WALLOC);
			msleep(ip, NULL, PINOD, "ntfs_ntlookup", 0);
			goto loop;
		}
		ntfs_ntget(ip);
		dprintf(("ntfs_ntlookup: ntnode %d: %p, usecount: %d\n",
			ino, ip, ip->i_usecount));
		*ipp = ip;
		OSFree(new_node, sizeof(struct ntnode), ntfs_malloc_tag);
		return (0);
	}

	bzero(new_node, sizeof(struct ntnode));
	ddprintf(("ntfs_ntlookup: allocating ntnode: %d: %p\n", ino, ip));

	/* Generic initialization */
	new_node->i_devvp = ntmp->ntm_devvp;
	new_node->i_dev = ntmp->ntm_dev;
	new_node->i_number = ino;
	new_node->i_mp = ntmp;
	SET(new_node->i_flag, IN_ALLOC);
	ntfs_nthashins(new_node);
	
	LIST_INIT(&new_node->i_fnlist);
	vnode_ref(new_node->i_devvp);

	new_node->i_lock_attr = lck_attr_alloc_init();
	new_node->i_lock = lck_mtx_alloc_init(ntfs_lck_grp, new_node->i_lock_attr);
	ntfs_ntget(new_node);

	*ipp = new_node;

	/*
	 * We're done initializing.  If we blocked, and someone was
	 * waiting for this inode, then wake them up.
	 */
	CLR(new_node->i_flag, IN_ALLOC);
	if (ISSET(new_node->i_flag, IN_WALLOC))
		wakeup(new_node);

	dprintf(("ntfs_ntlookup: ntnode %d: %p, usecount: %d\n",
		ino, new_node, new_node->i_usecount));

	return (0);
}

/*
 * Decrement usecount of ntnode and unlock it, if usecount reach zero,
 * deallocate ntnode.
 *
 * ntnode should be locked on entry, and unlocked on return.
 */
__private_extern__
void
ntfs_ntput(ip)
	struct ntnode *ip;
{
	struct ntvattr *vap;

	dprintf(("ntfs_ntput: rele ntnode %d: %p, usecount: %d\n",
		ip->i_number, ip, ip->i_usecount));

	ip->i_usecount--;

#ifdef DIAGNOSTIC
	if (ip->i_usecount < 0) {
		panic("ntfs_ntput: ino: %d usecount: %d \n",
		      ip->i_number,ip->i_usecount);
	}
#endif

	if (ip->i_usecount > 0) {
		lck_mtx_unlock(ip->i_lock);
		return;
	}

	dprintf(("ntfs_ntput: deallocating ntnode: %d\n", ip->i_number));

	if (LIST_FIRST(&ip->i_fnlist))
		panic("ntfs_ntput: ntnode has fnodes\n");

	ntfs_nthashrem(ip);

	while ((vap = LIST_FIRST(&ip->i_valist)) != NULL) {
		LIST_REMOVE(vap,va_list);
		ntfs_freentvattr(vap);
	}
	vnode_rele(ip->i_devvp);
	lck_mtx_free(ip->i_lock, ntfs_lck_grp);
	lck_attr_free(ip->i_lock_attr);
	OSFree(ip, sizeof(struct ntnode), ntfs_malloc_tag);
}

/*
 * increment usecount of ntnode 
 */
static void
ntfs_ntref(ip)
	struct ntnode *ip;
{
	ip->i_usecount++;

	dprintf(("ntfs_ntref: ino %d, usecount: %d\n",
		ip->i_number, ip->i_usecount));
			
}

/*
 * Decrement usecount of ntnode.
 */
static void
ntfs_ntrele(ip)
	struct ntnode *ip;
{
	dprintf(("ntfs_ntrele: rele ntnode %d: %p, usecount: %d\n",
		ip->i_number, ip, ip->i_usecount));

	ip->i_usecount--;

	if (ip->i_usecount < 0)
		panic("ntfs_ntrele: ino: %d usecount: %d \n",
		      ip->i_number,ip->i_usecount);
}

/*
 * Deallocate all memory allocated for ntvattr
 */
static void
ntfs_freentvattr(vap)
	struct ntvattr * vap;
{
	if (vap->va_flag & NTFS_AF_INRUN) {
		if (vap->va_vruncn)
			FREE(vap->va_vruncn, M_NTFSRUN);
		if (vap->va_vruncl)
			FREE(vap->va_vruncl, M_NTFSRUN);
	} else {
		if (vap->va_datap)
			FREE(vap->va_datap, M_NTFSRDATA);
	}
	OSFree(vap, sizeof(struct ntvattr), ntfs_malloc_tag);
}

/*
 * Convert disk image of attribute into ntvattr structure,
 * runs are expanded also.
 *
 * The ntvattr fields themselves are converted to host's endianness.
 * The content (data) of each of the attributes is in its raw on-disk
 * form, and must be converted when used.
 */
static int
ntfs_attrtontvattr(
		   struct ntfsmount * ntmp,
		   struct ntvattr ** rvapp,
		   struct attr * rap)
{
	int             error, i;
	struct ntvattr *vap;

	error = 0;
	*rvapp = NULL;

	vap = OSMalloc(sizeof(struct ntvattr), ntfs_malloc_tag);
	bzero(vap, sizeof(struct ntvattr));
	vap->va_ip = NULL;
	vap->va_flag = rap->a_hdr.a_flag;	/* u_int_8: no endian conversion */
	vap->va_type = le32toh(rap->a_hdr.a_type);
	vap->va_compression = rap->a_hdr.a_compression;	/* u_int_8: no endian conversion */
	vap->va_index = le16toh(rap->a_hdr.a_index);

	ddprintf(("type: 0x%x, index: %d", vap->va_type, vap->va_index));

	vap->va_namelen = rap->a_hdr.a_namelen;	/* u_int_8: no endian conversion */
	if (rap->a_hdr.a_namelen) {
		wchar *unp = (wchar *) ((caddr_t) rap + rap->a_hdr.a_nameoff);
		ddprintf((", name:["));
		for (i = 0; i < vap->va_namelen; i++) {
			vap->va_name[i] = le16toh(unp[i]);
			ddprintf(("%c", vap->va_name[i]));
		}
		ddprintf(("]"));
	}
	if (vap->va_flag & NTFS_AF_INRUN) {
		ddprintf((", nonres."));
		vap->va_datalen = le64toh(rap->a_nr.a_datalen);
		vap->va_allocated = le64toh(rap->a_nr.a_allocated);
		vap->va_vcnstart = le64toh(rap->a_nr.a_vcnstart);
		vap->va_vcnend = le64toh(rap->a_nr.a_vcnend);
		vap->va_compressalg = le16toh(rap->a_nr.a_compressalg);
		error = ntfs_runtovrun(&(vap->va_vruncn), &(vap->va_vruncl),
				       &(vap->va_vruncnt),
				       (caddr_t) rap + le16toh(rap->a_nr.a_dataoff));
	} else {
		vap->va_compressalg = 0;
		ddprintf((", res."));
		vap->va_datalen = le16toh(rap->a_r.a_datalen);
		vap->va_allocated = vap->va_datalen;
		vap->va_vcnstart = 0;
		vap->va_vcnend = ntfs_btocn(vap->va_allocated);
		MALLOC(vap->va_datap, caddr_t, vap->va_datalen,
		       M_NTFSRDATA, M_WAITOK);
		memcpy(vap->va_datap, (caddr_t) rap + le16toh(rap->a_r.a_dataoff),
		       vap->va_datalen);
	}
	ddprintf((", len: %d", vap->va_datalen));

	if (error)
		OSFree(vap, sizeof(struct ntvattr), ntfs_malloc_tag);
	else
		*rvapp = vap;

	ddprintf(("\n"));

	return (error);
}

/*
 * Expand run into more utilizable and more memory eating format.
 */
__private_extern__
int
ntfs_runtovrun(
	       cn_t ** rcnp,
	       cn_t ** rclp,
	       u_long * rcntp,
	       u_int8_t * run)	/* Raw info from disk filerec */
{
	u_int32_t	off;
	u_int32_t	sz, i;
	cn_t		*cn;	/* Starting cluster numbers of each run */
	cn_t		*cl;	/* Length (in clusters) of each run */
	u_long		cnt;	/* Count or runs, index into cn[] and cl[] */
	cn_t		prev;
	cn_t		tmp;

	/* Count the number of runs and allocate space for storing them in memory */
	off = 0;
	cnt = 0;
	i = 0;
	while (run[off]) {
		off += (run[off] & 0xF) + ((run[off] >> 4) & 0xF) + 1;
		cnt++;
	}
	MALLOC(cn, cn_t *, cnt * sizeof(cn_t), M_NTFSRUN, M_WAITOK);
	MALLOC(cl, cn_t *, cnt * sizeof(cn_t), M_NTFSRUN, M_WAITOK);

	/* Go back over the compacted run list and expand the offsets/lengths */
	off = 0;	/* First run is always absolute (relative to cluster #0) */
	cnt = 0;
	prev = 0;
	while (run[off]) {

		/*
		 * Each run is a variable-length structure.  The first
		 * byte is broken into two nibbles.  The lower nibble
		 * contains the number of bytes used to hold the run
		 * length.  The upper nibble contains the number of
		 * bytes used to hold the run offset.
		 *
		 * After that first length byte (two nibbles) comes
		 * the length of the run as an unsigned value.	Then
		 * comes the run start, which is a signed value interpretted
		 * relative to the start of the previous run.
		 */
		sz = run[off++];	/* Get the sizes */

		/* Gather the bytes of the run length */
		cl[cnt] = 0;

		for (i = 0; i < (sz & 0xF); i++)
			cl[cnt] += (u_int32_t) run[off++] << (i << 3);

		/* Gather the bytes of the run start (offset) */
		sz >>= 4;	/* May become zero here. */
		if (sz && run[off + sz - 1] & 0x80) {
			/* Offset is negative; sign extend. */
			tmp = ((u_int64_t) - 1) << (sz << 3);
			for (i = 0; i < sz; i++)
				tmp |= (u_int64_t) run[off++] << (i << 3);
		} else {
			tmp = 0;
			for (i = 0; i < sz; i++)
				tmp |= (u_int64_t) run[off++] << (i << 3);
		}
		/* Might it be better to test sz instead of tmp? */
		if (tmp) {
			/* Non-sparse, so add offset to previous run's start */
			prev = cn[cnt] = prev + tmp;
		} else {
			/* Zero offset means sparse.  Store zero as starting block. */
			cn[cnt] = 0;
		}
		
		cnt++;
	}
	
	/* Return the start and length arrays, and their size */
	*rcnp = cn;
	*rclp = cl;
	*rcntp = cnt;
	return (0);
}

/* 
 * Search fnode in ntnode, if not found allocate and preinitialize.
 *
 * ntnode should be locked on entry.
 */
__private_extern__
int
ntfs_fget(
	struct ntfsmount *ntmp,
	struct ntnode *ip,
	int attrtype,
	char *attrname,
	struct fnode **fpp)
{
	struct fnode *fp;

	dprintf(("ntfs_fget: ino: %d, attrtype: 0x%x, attrname: %s\n",
		ip->i_number,attrtype, attrname?attrname:""));
	*fpp = NULL;
	LIST_FOREACH(fp, &ip->i_fnlist, f_fnlist){
		dprintf(("ntfs_fget: fnode: attrtype: %d, attrname: %s\n",
			fp->f_attrtype, fp->f_attrname?fp->f_attrname:""));

		if ((attrtype == fp->f_attrtype) && 
		    ((!attrname && !fp->f_attrname) ||
		     (attrname && fp->f_attrname &&
		      !strcmp(attrname,fp->f_attrname)))){
			dprintf(("ntfs_fget: found existed: %p\n",fp));
			*fpp = fp;
		}
	}

	if (*fpp)
		return (0);

	fp = OSMalloc(sizeof(struct fnode), ntfs_malloc_tag);
	bzero(fp, sizeof(struct fnode));
	dprintf(("ntfs_fget: allocating fnode: %p\n",fp));

	fp->f_ip = ip;
	if (attrname) {
		fp->f_flag |= FN_AATTRNAME;
		MALLOC(fp->f_attrname, char *, strlen(attrname)+1, M_TEMP, M_WAITOK);
		strcpy(fp->f_attrname, attrname);
	} else
		fp->f_attrname = NULL;
	fp->f_attrtype = attrtype;

	ntfs_ntref(ip);

	LIST_INSERT_HEAD(&ip->i_fnlist, fp, f_fnlist);

	*fpp = fp;

	return (0);
}

/*
 * Deallocate fnode, remove it from ntnode's fnode list.
 *
 * ntnode should be locked.
 */
__private_extern__
void
ntfs_frele(
	struct fnode *fp)
{
	struct ntnode *ip = FTONT(fp);

	dprintf(("ntfs_frele: fnode: %p for %d: %p\n", fp, ip->i_number, ip));

	dprintf(("ntfs_frele: deallocating fnode\n"));
	LIST_REMOVE(fp,f_fnlist);
	if (fp->f_flag & FN_AATTRNAME)
		FREE(fp->f_attrname, M_TEMP);
	OSFree(fp, sizeof(struct fnode), ntfs_malloc_tag);
	ntfs_ntrele(ip);
}

/*
 * Lookup attribute name in format: [[:$ATTR_TYPE]:$ATTR_NAME], 
 * $ATTR_TYPE is searched in attrdefs read from $AttrDefs.
 * If $ATTR_TYPE nott specifed, ATTR_A_DATA assumed.
 */
static int
ntfs_ntlookupattr(
		struct ntfsmount * ntmp,
		const char * name,
		int namelen,
		int *attrtype,
		char **attrname)
{
	const char *sys;
	size_t syslen, i;
	struct ntvattrdef *adp;

	if (namelen == 0)
		return (0);

	if (name[0] == '$') {
		sys = name;
		for (syslen = 0; syslen < namelen; syslen++) {
			if(sys[syslen] == ':') {
				name++;
				namelen--;
				break;
			}
		}
		name += syslen;
		namelen -= syslen;

		adp = ntmp->ntm_ad;
		for (i = 0; i < ntmp->ntm_adnum; i++, adp++){
			if (syslen != adp->ad_namelen || 
			   strncmp(sys, adp->ad_name, syslen) != 0)
				continue;

			*attrtype = adp->ad_type;
			goto out;
		}
		return (ENOENT);
	} else
		*attrtype = NTFS_A_DATA;

    out:
	if (namelen) {
		MALLOC((*attrname), char *, namelen, M_TEMP, M_WAITOK);
		memcpy((*attrname), name, namelen);
		(*attrname)[namelen] = '\0';
	}

	return (0);
}

/*
 * Lookup specifed node for filename, matching cnp,
 * return fnode filled.
 */
__private_extern__
int
ntfs_ntlookupfile(
	      struct ntfsmount * ntmp,
	      vnode_t vp,
	      struct componentname * cnp,
	      proc_t p,
	      vnode_t * vpp)
{
	struct fnode   *fp = VTOF(vp);
	struct ntnode  *ip = FTONT(fp);
	struct ntvattr *vap;	/* Root attribute */
	cn_t            cn;	/* VCN in current attribute */
	caddr_t         rdbuf;	/* Buffer to read directory's blocks  */
	u_int32_t       blsize;
	u_int32_t       rdsize;	/* Length of data to read from current block */
	struct attr_indexentry *iep;
	int             error, res, anamelen, fnamelen;
	const char     *fname,*aname;
	u_int32_t       aoff;
	int attrtype = NTFS_A_DATA;
	char *attrname;
	struct fnode   *nfp;
	vnode_t			nvp;
	enum vtype	f_type;
	size_t		uninamelen;
	u_int16_t	uniname[NTFS_MAXFILENAME];
	
	attrname = NULL;
	vap = NULL;
	rdbuf = NULL;
	
	error = ntfs_ntget(ip);
	if (error)
		return error;

	error = ntfs_ntvattrget(ntmp, ip, NTFS_A_INDXROOT, "$I30", 0, p, &vap);
	if (error || (vap->va_flag & NTFS_AF_INRUN)) {
		error = ENOTDIR;
		goto fail;
	}

	blsize = le32toh(vap->va_a_iroot->ir_size);
	rdsize = vap->va_datalen;
        if (rdsize > blsize)
            panic("ntfs_ntlookupfile: rdsize > blsize");

	/*
	 * Divide file name into: foofilefoofilefoofile[:attrspec]
	 * Store like this:       fname:fnamelen       [aname:anamelen]
	 */
	fname = cnp->cn_nameptr;
	aname = NULL;
	anamelen = 0;
	for (fnamelen = 0; fnamelen < cnp->cn_namelen; fnamelen++)
		if(fname[fnamelen] == ':') {
			aname = fname + fnamelen + 1;
			anamelen = cnp->cn_namelen - fnamelen - 1;
			dprintf(("ntfs_ntlookupfile: %s (%d), attr: %s (%d)\n",
				fname, fnamelen, aname, anamelen));
			break;
		}

	error = utf8_decodestr(fname, fnamelen, uniname, &uninamelen, sizeof(uniname), 0, UTF_PRECOMPOSED);
	if (error)
		goto fail;
	uninamelen /= sizeof(u_int16_t);

	dprintf(("ntfs_ntlookupfile: blksz: %d, rdsz: %d\n", blsize, rdsize));

	rdbuf = OSMalloc(blsize, ntfs_malloc_tag);

	error = ntfs_readattr(ntmp, ip, NTFS_A_INDXROOT, "$I30",
			       0, rdsize, rdbuf, NULL, p);
	if (error)
		goto fail;

	aoff = sizeof(struct attr_indexroot);

	do {
		iep = (struct attr_indexentry *) (rdbuf + aoff);

		for (; !(le32toh(iep->ie_flag) & NTFS_IEFLAG_LAST) && (rdsize > aoff);
			aoff += le16toh(iep->reclen),
			iep = (struct attr_indexentry *) (rdbuf + aoff))
		{
			ddprintf(("scan: %d, %d\n",
				  (u_int32_t) le32toh(iep->ie_number),
				  (u_int32_t) iep->ie_fnametype));

			/* check the name - the case-insensitive check
			 * has to come first, to break from this for loop
			 * if needed, so we can dive correctly */
			res = ntfs_unistrcasecmp(iep->ie_fnamelen, iep->ie_fname, uninamelen, uniname);
			if (res > 0) break;	/* Found larger key; search corresponding child */
			if (res < 0) continue;	/* Smaller key; keep looking in this buffer */

			/*
			 * If we found a case sensitive name, or mounted case sensitive,
			 * then we need to do the case sensitive compare.
			 *
			 * ¥ Are case duplicates stored in any particular order?
			 * ¥ We end up checking all of them.  But what if we had
			 * ¥ a sub-node in the middle of case duplicates (if that
			 * ¥ is even possible)?
			 */
			if (iep->ie_fnametype == 0 ||
			    (ntmp->ntm_flag & NTFS_MFLAG_CASE_SENSITIVE))
			{
				res = ntfs_unistrcmp(iep->ie_fnamelen, iep->ie_fname, uninamelen, uniname);
				if (res != 0) continue;
			}

			/* If they asked for a named attribute, get its type/name */
			if (aname) {
				error = ntfs_ntlookupattr(ntmp,
					aname, anamelen,
					&attrtype, &attrname);
				if (error)
					goto fail;
			}

			/* Check if we've found ourself (i.e. ".")*/
			if ((le32toh(iep->ie_number) == ip->i_number) &&
			    (attrtype == fp->f_attrtype) &&
			    ((!attrname && !fp->f_attrname) ||
			     (attrname && fp->f_attrname &&
			      !strcmp(attrname, fp->f_attrname))))
			{
				vnode_get(vp);
				*vpp = vp;
				error = 0;
				goto fail;
			}

			/*
			 * Vget the node.  Since we already have the information about
			 * the node, don't bother trying to load or validate it from
			 * the MFT.
			 */
			if ((le32toh(iep->ie_fflag) & NTFS_FFLAG_DIR) &&
				(attrtype == NTFS_A_DATA) &&
				(attrname == NULL))
			{
				f_type = VDIR;
			} else {
				f_type = VREG;
			}
			error = ntfs_vgetex(ntmp->ntm_mountp, le32toh(iep->ie_number),
				vp, cnp, f_type,
				attrtype, attrname,
				VG_DONTLOADIN | VG_DONTVALIDFN,
				p, &nvp);

			/* free the buffer returned by ntfs_ntlookupattr() */
			if (attrname) {
				FREE(attrname, M_TEMP);
				attrname = NULL;
			}

			if (error)
				goto fail;

			nfp = VTOF(nvp);

			if (nfp->f_flag & FN_VALID) {
				*vpp = nvp;
				goto fail;
			}

			nfp->f_fflag = le32toh(iep->ie_fflag);
			nfp->f_pnumber = le32toh(iep->ie_fpnumber);
			nfp->f_times.t_create	= le64toh(iep->ie_ftimes.t_create);
			nfp->f_times.t_write	= le64toh(iep->ie_ftimes.t_write);
			nfp->f_times.t_mftwrite	= le64toh(iep->ie_ftimes.t_mftwrite);
			nfp->f_times.t_access	= le64toh(iep->ie_ftimes.t_access);

			if ((nfp->f_attrtype == NTFS_A_DATA) &&
			    (nfp->f_attrname == NULL))
			{
				/* Opening default attribute */
				nfp->f_size = le64toh(iep->ie_fsize);
				nfp->f_allocated = le64toh(iep->ie_fallocated);
				nfp->f_flag |= FN_PRELOADED;
			} else {
				error = ntfs_filesize(ntmp, nfp, p,
					    &nfp->f_size, &nfp->f_allocated);
				if (error) {
					vnode_put(nvp);
					goto fail;
				}
			}

			nfp->f_flag &= ~FN_VALID;
			*vpp = nvp;
			goto fail;
		}

		/* Dive if possible */
		if (le32toh(iep->ie_flag) & NTFS_IEFLAG_SUBNODE) {
			dprintf(("ntfs_ntlookupfile: diving\n"));

			cn = le64toh(*(cn_t *) (rdbuf + aoff +
					le16toh(iep->reclen) - sizeof(cn_t)));
			rdsize = blsize;

			error = ntfs_readattr(ntmp, ip, NTFS_A_INDX, "$I30",
					ntfs_cntob(cn), rdsize, rdbuf, NULL, p);
			if (error)
				goto fail;

			error = ntfs_procfixups(ntmp, NTFS_INDXMAGIC,
						rdbuf, rdsize);
			if (error)
				goto fail;

			aoff = (le16toh(((struct attr_indexalloc *) rdbuf)->ia_hdrsize) +
				0x18);
		} else {
			dprintf(("ntfs_ntlookupfile: nowhere to dive :-(\n"));
			error = ENOENT;
			break;
		}
	} while (1);

	dprintf(("finish\n"));

fail:
	if (attrname) FREE(attrname, M_TEMP);
	if (vap) ntfs_ntvattrrele(vap);
	if (rdbuf) OSFree(rdbuf, blsize, ntfs_malloc_tag);
	ntfs_ntput(ip);
	return (error);
}

/*
 * Check if name type is permitted to show.
 */
__private_extern__
int
ntfs_isnamepermitted(
		     struct ntfsmount * ntmp,
		     struct attr_indexentry * iep)
{
	if (ntmp->ntm_flag & NTFS_MFLAG_ALLNAMES)
		return 1;

	switch (iep->ie_fnametype) {
	case 2:
		ddprintf(("ntfs_isnamepermitted: skiped DOS name\n"));
		return 0;
	case 0: case 1: case 3:
		return 1;
	default:
		printf("ntfs_isnamepermitted: " \
		       "WARNING! Unknown file name type: %d\n",
		       iep->ie_fnametype);
		break;
	}
	return 0;
}

/*
 * Determine the appropriate buffer size to use when calling ntfs_ntreaddir.
 * The buffer must be big enough to hold either the index root, or one
 * index allocation entry (i.e. a B-tree node).
 *
 * If the file system were read/write, it would be up to the caller to
 * guarantee that the directory didn't change for as long as it uses the
 * buffer size returned by this routine (since the size could potentially
 * change if the index root's size changes).
 */
__private_extern__
int
ntfs_ntreaddir_bufsize(
	struct ntfsmount * ntmp,
	struct fnode * fp,
	proc_t p,
	u_int32_t *bufsize)
{
	struct ntnode  *ip = FTONT(fp);
	struct ntvattr *vap = NULL;	/* IndexRoot attribute */
	int error = 0;
	u_int32_t blsize;			/* Index allocation (B-tree node) size */

	error = ntfs_ntget(ip);
	if (error)
		return (error);

	/* Get the root of the directory's index B+ tree */
	error = ntfs_ntvattrget(ntmp, ip, NTFS_A_INDXROOT, "$I30", 0, p, &vap);
	if (error) {
		error = ENOTDIR;
	} else {
		blsize = le32toh(vap->va_a_iroot->ir_size);
		*bufsize = MAX(vap->va_datalen, blsize);
	}
	
	if (vap)
		ntfs_ntvattrrele(vap);
	ntfs_ntput(ip);
	
	return error;	
}



/*
 * Read ntfs dir like stream of attr_indexentry, not like btree of them.
 * ¥ Does that mean we get entries out of order?
 * This is done by scaning $BITMAP:$I30 for busy clusters and reading them.
 * Ofcouse $INDEX_ROOT:$I30 is read before. Last read values are stored in
 * fnode, so we can skip toward record number num almost immediatly.
 * Anyway this is rather slow routine. The problem is that we don't know
 * how many records are there in $INDEX_ALLOCATION:$I30 block.
 */
__private_extern__
int
ntfs_ntreaddir(
	       struct ntfsmount * ntmp,
	       struct fnode * fp,
	       u_int32_t num,
	       caddr_t rdbuf,
	       struct attr_indexentry ** riepp,
	       proc_t p)
{
	struct ntnode  *ip = FTONT(fp);
	struct ntvattr *vap = NULL;	/* IndexRoot attribute */
	struct ntvattr *bmvap = NULL;	/* BitMap attribute */
	struct ntvattr *iavap = NULL;	/* IndexAllocation attribute */
	u_char         *bmp = NULL;	/* Bitmap */
	u_int32_t       blsize;		/* Index allocation size (2048) */
	u_int32_t       rdsize;		/* Length of data to read */
	u_int32_t       attrnum;	/* Current attribute type */
	u_int32_t       cpbl = 1;	/* Clusters per directory block */
	u_int32_t       blnum;
	struct attr_indexentry *iep;
	int             error = ENOENT;
	u_int32_t       aoff, cnum;
	u_int32_t       fileno;

	dprintf(("ntfs_ntreaddir: read ino: %d, num: %d\n", ip->i_number, num));
	error = ntfs_ntget(ip);
	if (error)
		return (error);

        /* Get the root of the directory's index B+ tree */
	error = ntfs_ntvattrget(ntmp, ip, NTFS_A_INDXROOT, "$I30", 0, p, &vap);
	if (error) {
		error = ENOTDIR;
		goto fail;
	}

	blsize = le32toh(vap->va_a_iroot->ir_size);

        /* Get the index bitmap and external allocation, if any */
	if (vap->va_a_iroot->ir_flag & NTFS_IRFLAG_INDXALLOC) {	/* little endian byte */
		error = ntfs_ntvattrget(ntmp, ip, NTFS_A_INDXBITMAP, "$I30",
					0, p, &bmvap);
		if (error) {
			error = ENOTDIR;
			goto fail;
		}
		MALLOC(bmp, u_char *, bmvap->va_datalen, M_TEMP, M_WAITOK);
		error = ntfs_readattr(ntmp, ip, NTFS_A_INDXBITMAP, "$I30", 0,
				       bmvap->va_datalen, bmp, NULL, p);
		if (error)
			goto fail;

		error = ntfs_ntvattrget(ntmp, ip, NTFS_A_INDX, "$I30",
					0, p, &iavap);
		if (error) {
			error = ENOTDIR;
			goto fail;
		}
		cpbl = ntfs_btocn(blsize + ntfs_cntob(1) - 1);
		dprintf(("ntfs_ntreaddir: indexalloc: %d, cpbl: %d\n",
			 iavap->va_datalen, cpbl));
	} else {
		dprintf(("ntfs_ntreadidir: w/o BitMap and IndexAllocation\n"));
		iavap = bmvap = NULL;
		bmp = NULL;
	}

        /*
         * Optimize performance for the common case where calls pass
         * "num" as increasing values.  Avoid iterating through the
         * entire directory every call by remembering an earlier
         * num and its corresponding position.
         */
	if ((fp->f_lastdnum < num) && (fp->f_lastdnum != 0)) {
		attrnum = fp->f_lastdattr;
		aoff = fp->f_lastdoff;
		blnum = fp->f_lastdblnum;
		cnum = fp->f_lastdnum;
	} else {
                /*
                 * By default, start with the index root.
                 * Skip over the root's header.
                 */
		attrnum = NTFS_A_INDXROOT;
		aoff = sizeof(struct attr_indexroot);
		blnum = 0;
		cnum = 0;
	}

        /* Loop over index entries until we find the num'th one */
	do {
                /* Read the next buffer full of index entries (root or allocation) */
		dprintf(("ntfs_ntreaddir: scan: 0x%x, %d, %d, %d, %d\n",
			 attrnum, (u_int32_t) blnum, cnum, num, aoff));
		rdsize = (attrnum == NTFS_A_INDXROOT) ? vap->va_datalen : blsize;
		error = ntfs_readattr(ntmp, ip, attrnum, "$I30",
				ntfs_cntob(blnum * cpbl), rdsize, rdbuf, NULL, p);
		if (error)
			goto fail;

		if (attrnum == NTFS_A_INDX) {
			error = ntfs_procfixups(ntmp, NTFS_INDXMAGIC,
						rdbuf, rdsize);
			if (error)
				goto fail;
		}
                
                /* If we're starting a new buffer, position to first entry */
		if (aoff == 0)
			aoff = (attrnum == NTFS_A_INDX) ?
				(0x18 + le16toh(((struct attr_indexalloc *) rdbuf)->ia_hdrsize)) :
				sizeof(struct attr_indexroot);

                /* Loop over entries in this buffer */
		iep = (struct attr_indexentry *) (rdbuf + aoff);
		for (; !(le32toh(iep->ie_flag) & NTFS_IEFLAG_LAST) && (rdsize > aoff);
			aoff += le16toh(iep->reclen),
			iep = (struct attr_indexentry *) (rdbuf + aoff))
		{
			if (!ntfs_isnamepermitted(ntmp, iep)) continue;

			fileno = le32toh(iep->ie_number);
			if (fileno != NTFS_ROOTINO && fileno < 24)
					continue;	/* Skip over system files */
			if (cnum >= num) {
				fp->f_lastdnum = cnum;
				fp->f_lastdoff = aoff;
				fp->f_lastdblnum = blnum;
				fp->f_lastdattr = attrnum;

				*riepp = iep;

				error = 0;
				goto fail;
			}
			cnum++;
		}

		if (iavap) {
			if (attrnum == NTFS_A_INDXROOT)
				blnum = 0;
			else
				blnum++;

			while (ntfs_cntob(blnum * cpbl) < iavap->va_datalen) {
				if (bmp[blnum >> 3] & (1 << (blnum & 3)))
					break;
				blnum++;
			}

			attrnum = NTFS_A_INDX;
			aoff = 0;
			if (ntfs_cntob(blnum * cpbl) >= iavap->va_datalen)
				break;
			dprintf(("ntfs_ntreaddir: blnum: %d\n", (u_int32_t) blnum));
		}
	} while (iavap);

	*riepp = NULL;
	fp->f_lastdnum = 0;

fail:
	if (vap)
		ntfs_ntvattrrele(vap);
	if (bmvap)
		ntfs_ntvattrrele(bmvap);
	if (iavap)
		ntfs_ntvattrrele(iavap);
	if (bmp)
		FREE(bmp, M_TEMP);
	ntfs_ntput(ip);
	return (error);
}

/*
 * Convert NTFS times that are in 100 ns units and begins from
 * 1601 Jan 1 into unix times.
 *
 * Assumes input parameter has already been converted to native
 * byte order.
 */
__private_extern__
struct timespec
ntfs_nttimetounix(
		  u_int64_t nt)
{
	struct timespec t;

	/* WindowNT times are in 100 ns and from 1601 Jan 1 */
	t.tv_nsec = (nt % (1000 * 1000 * 10)) * 100;
	t.tv_sec = nt / (1000 * 1000 * 10) -
		369LL * 365LL * 24LL * 60LL * 60LL -
		89LL * 1LL * 24LL * 60LL * 60LL;
	return (t);
}

#if NTFS_WRITE
/*
 * Get file times from NTFS_A_NAME attribute.
 */
int
ntfs_times(
	   struct ntfsmount * ntmp,
	   struct ntnode * ip,
	   vfs_context_t context,
	   ntfs_times_t * tm)
{
	struct ntvattr *vap;
	int             error;

	dprintf(("ntfs_times: ino: %d...\n", ip->i_number));

	error = ntfs_ntget(ip);
	if (error)
		return (error);

	error = ntfs_ntvattrget(ntmp, ip, NTFS_A_NAME, NULL, 0, context, &vap);
	if (error) {
		ntfs_ntput(ip);
		return (error);
	}
#warning needs to be big endian aware
	*tm = vap->va_a_name->n_times;
	ntfs_ntvattrrele(vap);
	ntfs_ntput(ip);

	return (0);
}
#endif

/*
 * Get file sizes from corresponding attribute. 
 * 
 * ntnode under fnode should be locked.
 */
__private_extern__
int
ntfs_filesize(
	      struct ntfsmount * ntmp,
	      struct fnode * fp,
	      proc_t p,
	      u_int64_t * size,
	      u_int64_t * bytes)
{
	struct ntvattr *vap;
	struct ntnode *ip = FTONT(fp);
	u_int64_t       sz, bn;
	int             error;

	dprintf(("ntfs_filesize: ino: %d\n", ip->i_number));

	error = ntfs_ntvattrget(ntmp, ip,
		fp->f_attrtype, fp->f_attrname, 0, p, &vap);
	if (error)
		return (error);

        /* Sizes are already in native byte order */
	bn = vap->va_allocated;
	sz = vap->va_datalen;

	dprintf(("ntfs_filesize: %d bytes (%d bytes allocated)\n",
		(u_int32_t) sz, (u_int32_t) bn));

	if (size)
		*size = sz;
	if (bytes)
		*bytes = bn;

	ntfs_ntvattrrele(vap);

	return (0);
}

#if NTFS_WRITE
/*
 * This is one of write routine.
 */
int
ntfs_writeattr_plain(
	struct ntfsmount * ntmp,
	struct ntnode * ip,
	u_int32_t attrnum,	
	char *attrname,
	off_t roff,
	size_t rsize,
	void *rdata,
	size_t * initp,
	struct uio *uio)
{
	size_t          init;
	int             error = 0;
	off_t           off = roff, left = rsize, towrite;
	caddr_t         data = rdata;
	struct ntvattr *vap;
	*initp = 0;

	while (left) {
		error = ntfs_ntvattrget(ntmp, ip, attrnum, attrname,
					ntfs_btocn(off), context, &vap);
		if (error)
			return (error);
		towrite = MIN(left, ntfs_cntob(vap->va_vcnend + 1) - off);
		ddprintf(("ntfs_writeattr_plain: o: %d, s: %d (%d - %d)\n",
			 (u_int32_t) off, (u_int32_t) towrite,
			 (u_int32_t) vap->va_vcnstart,
			 (u_int32_t) vap->va_vcnend));
		error = ntfs_writentvattr_plain(ntmp, ip, vap,
					 off - ntfs_cntob(vap->va_vcnstart),
					 towrite, data, &init, uio);
		if (error) {
			printf("ntfs_writeattr_plain: " \
			       "ntfs_writentvattr_plain failed: o: %d, s: %d\n",
			       (u_int32_t) off, (u_int32_t) towrite);
			printf("ntfs_writeattr_plain: attrib: %d - %d\n",
			       (u_int32_t) vap->va_vcnstart, 
			       (u_int32_t) vap->va_vcnend);
			ntfs_ntvattrrele(vap);
			break;
		}
		ntfs_ntvattrrele(vap);
		left -= towrite;
		off += towrite;
		data = data + towrite;
		*initp += init;
	}

	return (error);
}

/*
 * This is one of write routine.
 *
 * ntnode should be locked.
 */
#warning ntfs_writentvattr_plain needs to be big endian aware
int
ntfs_writentvattr_plain(
	struct ntfsmount * ntmp,
	struct ntnode * ip,
	struct ntvattr * vap,
	off_t roff,
	size_t rsize,
	void *rdata,
	size_t * initp,
	struct uio *uio)
{
	int             error = 0;
	off_t           off;
	int             cnt;
	cn_t            ccn, ccl, cn, left, cl;
	caddr_t         data = rdata;
	struct buf     *bp;
	size_t          tocopy;

	*initp = 0;

	if ((vap->va_flag & NTFS_AF_INRUN) == 0) {
		printf("ntfs_writevattr_plain: CAN'T WRITE RES. ATTRIBUTE\n");
		return ENOTTY;
	}

	ddprintf(("ntfs_writentvattr_plain: data in run: %ld chains\n",
		 vap->va_vruncnt));

	off = roff;
	left = rsize;
	ccl = 0;
	ccn = 0;
	cnt = 0;
	for (; left && (cnt < vap->va_vruncnt); cnt++) {
		ccn = vap->va_vruncn[cnt];
		ccl = vap->va_vruncl[cnt];

		ddprintf(("ntfs_writentvattr_plain: " \
			 "left %d, cn: 0x%x, cl: %d, off: %d\n", \
			 (u_int32_t) left, (u_int32_t) ccn, \
			 (u_int32_t) ccl, (u_int32_t) off));

		if (ntfs_cntob(ccl) < off) {
			off -= ntfs_cntob(ccl);
			cnt++;
			continue;
		}
		if (!ccn && ip->i_number != NTFS_BOOTINO)
			continue; /* XXX */

		ccl -= ntfs_btocn(off);
		cn = ccn + ntfs_btocn(off);
		off = ntfs_btocnoff(off);

		while (left && ccl) {
			tocopy = MIN(left,
				  MIN(ntfs_cntob(ccl) - off, MAXBSIZE - off));
			cl = ntfs_btocl(tocopy + off);
			ddprintf(("ntfs_writentvattr_plain: write: " \
				"cn: 0x%x cl: %d, off: %d len: %d, left: %d\n",
				(u_int32_t) cn, (u_int32_t) cl, 
				(u_int32_t) off, (u_int32_t) tocopy, 
				(u_int32_t) left));
			if ((off == 0) && (tocopy == ntfs_cntob(cl)))
			{
				bp = buf_getblk(ntmp->ntm_devvp, ntfs_cntobn(cn),
					    ntfs_cntob(cl), 0, 0, BLK_META);
				buf_clear(bp);
			} else {
				error = (int)buf_bread(ntmp->ntm_devvp, ntfs_cntobn(cn),
						       ntfs_cntob(cl), NOCRED, &bp);	/*¥ NOCRED */
				if (error) {
					buf_brelse(bp);
					return (error);
				}
			}
			if (uio)
				uiomove((char *)buf_dataptr(bp) + off, tocopy, uio);
			else
				memcpy((char *)buf_dataptr(bp) + off, data, tocopy);
			buf_bawrite(bp);
			data = data + tocopy;
			*initp += tocopy;
			off = 0;
			left -= tocopy;
			cn += cl;
			ccl -= cl;
		}
	}

	if (left) {
		printf("ntfs_writentvattr_plain: POSSIBLE RUN ERROR\n");
		error = EINVAL;
	}

	return (error);
}
#endif

/*
 * Given an ntvattr (attribute in-memory form), read a range of bytes
 * from that attribute.  Does not handle compression, but does zero
 * fill sparse runs.  Handles both internal attributes (eg., inside
 * an MFT record) and external attributes (using runs).
 *
 * Returns E2BIG if end of requested range is beyond the end of the
 * last run.
 *
 * ntnode should be locked.
 */
__private_extern__
int
ntfs_readntvattr_plain(
	struct ntfsmount * ntmp,
	struct ntnode * ip,
	struct ntvattr * vap,
	off_t roff,
	size_t rsize,
	void *rdata,
	size_t * initp,		/* output: number of initialized bytes on disk */
	struct uio *uio)
{
	int             error = 0;
	off_t           off;

	*initp = 0;
	if (vap->va_flag & NTFS_AF_INRUN) {
                /* Reading from a run list (as opposed to an MFT record) */
		int             cnt;
		cn_t            ccn, ccl, cn, left, cl;
		caddr_t         data = rdata;
		struct buf     *bp;
		size_t          tocopy;

		ddprintf(("ntfs_readntvattr_plain: data in run: %ld chains\n",
			 vap->va_vruncnt));

                /*
                 * We loop over all runs for this attribute, ignoring runs
                 * containing data before the start of the requested range.
                 */
		off = roff;	/* Desired offset from start of next run */
		left = rsize;	/* Bytes remaining to read */
		ccl = 0;
		ccn = 0;
		cnt = 0;
		while (left && (cnt < vap->va_vruncnt)) {
			ccn = vap->va_vruncn[cnt];	/* Start of this run */
			ccl = vap->va_vruncl[cnt];	/* Length of this run */

			ddprintf(("ntfs_readntvattr_plain: " \
				 "left %d, cn: 0x%x, cl: %d, off: %d\n", \
				 (u_int32_t) left, (u_int32_t) ccn, \
				 (u_int32_t) ccl, (u_int32_t) off));

			if (ntfs_cntob(ccl) <= off) {
				/* current run is entirely before start of range; skip it */
				off -= ntfs_cntob(ccl);
				cnt++;
				continue;
			}
			if (ccn || ip->i_number == NTFS_BOOTINO) {
				/* Current run is not sparse */
				ccl -= ntfs_btocn(off);		/* Skip over unused part of run (if any) */
				cn = ccn + ntfs_btocn(off);	/* Cluster containing start of range */
				off = ntfs_btocnoff(off);	/* Offset of range within cluster #cn */

				/*¥ Could this be improved with cluster I/O? */
				while (left && ccl) {
					tocopy = MIN(left,
						  MIN(ntfs_cntob(ccl) - off,
						      MAXBSIZE - off));
					cl = ntfs_btocl(tocopy + off);	/* Number of clusters to read */
					ddprintf(("ntfs_readntvattr_plain: " \
						"read: cn: 0x%x cl: %d, " \
						"off: %d len: %d, left: %d\n",
						(u_int32_t) cn, 
						(u_int32_t) cl, 
						(u_int32_t) off, 
						(u_int32_t) tocopy, 
						(u_int32_t) left));
					error = (int)buf_bread(ntmp->ntm_devvp,
			                              ntfs_cntobn(cn),
						      ntfs_cntob(cl),
						      NOCRED, &bp);	/*¥ NOCRED */
					if (error) {
						buf_brelse(bp);
						return (error);
					}
                                        
                                        /* Copy from cache block to caller's buffer */
					if (uio) {
						uiomove((char *)buf_dataptr(bp) + off,
							tocopy, uio);
					} else {
						memcpy(data, (char *)buf_dataptr(bp) + off,
							tocopy);
					}
					buf_brelse(bp);
					data = data + tocopy;
					*initp += tocopy;
					off = 0;
					left -= tocopy;
					cn += cl;
					ccl -= cl;
				}
			} else {
                                /* Current run is sparse; zero fill buffer */
				tocopy = MIN(left, ntfs_cntob(ccl) - off);
				ddprintf(("ntfs_readntvattr_plain: "
					"hole: ccn: 0x%x ccl: %d, off: %d, " \
					" len: %d, left: %d\n", 
					(u_int32_t) ccn, (u_int32_t) ccl, 
					(u_int32_t) off, (u_int32_t) tocopy, 
					(u_int32_t) left));
				left -= tocopy;
				off = 0;
				if (uio) {
					/*¥ There must be a better way to zero fill uio buffer */
					printf("ntfs_readntvattr_plain: doing slow uiomoves to zero buffer");
					size_t remains = tocopy;
					for(; remains; remains++)
						uiomove("", 1, uio);
				} else 
					bzero(data, tocopy);
				data = data + tocopy;
			}
			cnt++;
		}
		if (left) {
			printf("ntfs_readntvattr_plain: POSSIBLE RUN ERROR\n");
			error = E2BIG;
		}
	} else {
		/*¥ Should range be bounds checked against attribute size? */
		ddprintf(("ntfs_readnvattr_plain: data is in mft record\n"));
		if (uio) 
			uiomove(vap->va_datap + roff, rsize, uio);
		else
			memcpy(rdata, vap->va_datap + roff, rsize);
		*initp += rsize;
	}

	return (error);
}

/*
 * Read some raw on-disk data for an attribute.
 * Does not handle decompression of data.
 * Does zero fill sparse runs.
 */
__private_extern__
int
ntfs_readattr_plain(
	struct ntfsmount * ntmp,
	struct ntnode * ip,	/* the file/dir to read from */
	u_int32_t attrnum,	/* the attribute's type */
	char *attrname,		/* attribute name; NULL means empty string */
	off_t roff,		/* offset with attribute to start reading */
	size_t rsize,		/* maximum amount to read */
	void *rdata,		/* internal buffer to store results (if uio==NULL) */
	size_t * initp,		/* output: number of initialized bytes on disk */
	struct uio *uio,	/* user buffer to store data */
	proc_t p)
{
	size_t          init;
	int             error = 0;
	off_t           off = roff, left = rsize, toread;
	caddr_t         data = rdata;
	struct ntvattr *vap;
	*initp = 0;

	while (left) {
		error = ntfs_ntvattrget(ntmp, ip, attrnum, attrname,
					ntfs_btocn(off), p, &vap);
		if (error)
			return (error);

		toread = MIN(left, ntfs_cntob(vap->va_vcnend + 1) - off);
		ddprintf(("ntfs_readattr_plain: o: %d, s: %d (%d - %d)\n",
			 (u_int32_t) off, (u_int32_t) toread,
			 (u_int32_t) vap->va_vcnstart,
			 (u_int32_t) vap->va_vcnend));
		error = ntfs_readntvattr_plain(ntmp, ip, vap,
					 off - ntfs_cntob(vap->va_vcnstart),
					 toread, data, &init, uio);
		if (error) {
			printf("ntfs_readattr_plain: " \
			       "ntfs_readntvattr_plain failed: o: %d, s: %d\n",
			       (u_int32_t) off, (u_int32_t) toread);
			printf("ntfs_readattr_plain: attrib: %d - %d\n",
			       (u_int32_t) vap->va_vcnstart, 
			       (u_int32_t) vap->va_vcnend);
			ntfs_ntvattrrele(vap);
			break;
		}
		ntfs_ntvattrrele(vap);
		left -= toread;
		off += toread;
		data = data + toread;
		*initp += init;
	}

	return (error);
}

/*
 * This is one of read routines.
 */
__private_extern__
int
ntfs_readattr(
	struct ntfsmount * ntmp,
	struct ntnode * ip,	/* file/dir containing desired attribute */
	u_int32_t attrnum,	/* attribute type */
	char *attrname,		/* attribute name (NULL means empty string) */
	off_t roff,		/* offset within attribute to read from */
	size_t rsize,		/* number of bytes of attribute to read */
	void *rdata,		/* internal buffer to store the data, OR: */
	struct uio *uio,	/* user buffer to store the data */
	proc_t p)
{
	int             error = 0;
	struct ntvattr *vap;	/* internal representation of attribute */
	size_t          init;	/* initialized (valid) data size */

	ddprintf(("ntfs_readattr: reading %d: 0x%x, from %d size %d bytes\n",
	       ip->i_number, attrnum, (u_int32_t) roff, (u_int32_t) rsize));

        /* Find the attribute information for given ntnode */
	error = ntfs_ntvattrget(ntmp, ip, attrnum, attrname, 0, p, &vap);
	if (error)
		return (error);

	if ((roff > vap->va_datalen) ||
	    (roff + rsize > vap->va_datalen)) {
		ddprintf(("ntfs_readattr: offset too big\n"));
		ntfs_ntvattrrele(vap);
		return (E2BIG);
	}
	if (vap->va_compression && vap->va_compressalg) {
		u_int8_t       *cup;	/* buffer for compressed data */
		u_int8_t       *uup;	/* buffer for uncompressed data */
		off_t           off = roff, left = rsize, tocopy;
		caddr_t         data = rdata;
		cn_t            cn;		/* Offset in terms of compression units */
		size_t			comp_unit_size;		/* Bytes per compression unit */

		comp_unit_size = (ntmp->ntm_bps * ntmp->ntm_spc) << vap->va_compressalg;
		
		ddprintf(("ntfs_ntreadattr: compression: %d\n",
			 vap->va_compressalg));

		cup = OSMalloc(comp_unit_size, ntfs_malloc_tag);
		if (rdata && rsize == comp_unit_size && (roff % comp_unit_size) == 0)
			uup = rdata;		/* Decompress straight into caller's buffer */
		else
			uup = OSMalloc(comp_unit_size, ntfs_malloc_tag);

		/*
		 * Determine cluster number of start of compression unit,
		 * and byte offset within that compression unit.
		 */
		cn = ntfs_btocn(roff) & ~((1 << vap->va_compressalg) - 1);
		off = roff - ntfs_cntob(cn);

		while (left) {
                        /* Read the raw data into our buffer */
			error = ntfs_readattr_plain(ntmp, ip, attrnum,
						  attrname, ntfs_cntob(cn),
					      comp_unit_size, cup, &init, NULL, p);
			if (error)
				break;

			tocopy = MIN(left, comp_unit_size - off);

			if (init == comp_unit_size) {
                                /*
                                 * The size on disk is an entire compression unit,
                                 * which means this chunk isn't actually compressed.
                                 * So, just copy it as-is.
                                 */
				if (uio)
					uiomove(cup + off, tocopy, uio);
				else
					memcpy(data, cup + off, tocopy);
			} else if (init == 0) {
                                /*
                                 * Nothing stored on disk, which means this chunk is
                                 * sparse (all zeroes).
                                 */
				if (uio) {
					/* Zero out tocopy bytes and uimove them. */
					/* I wish there was a uizero() call. */
					bzero(uup, tocopy);
					uiomove(uup, tocopy, uio);
				}
				else
					bzero(data, tocopy);
			} else {
                                /* Uncompress one chunk and move the uncompressed data */
				error = ntfs_uncompunit(ntmp, uup, cup, comp_unit_size);
				if (error)
					break;
				if (uio)
					uiomove(uup + off, tocopy, uio);
				else if (data != (caddr_t)uup)
					memcpy(data, uup + off, tocopy);
			}

			left -= tocopy;
			data = data + tocopy;
			off += tocopy - comp_unit_size;
			cn += 1 << vap->va_compressalg;
		}

		if (rdata != uup)
			OSFree(uup, comp_unit_size, ntfs_malloc_tag);
		OSFree(cup, comp_unit_size, ntfs_malloc_tag);
	} else
		error = ntfs_readattr_plain(ntmp, ip, attrnum, attrname,
					     roff, rsize, rdata, &init, uio, p);
	ntfs_ntvattrrele(vap);
	return (error);
}

#if UNUSED_CODE
int
ntfs_parserun(
	      cn_t * cn,
	      cn_t * cl,
	      u_int8_t * run,
	      u_long len,
	      u_long *off)
{
	u_int8_t        sz;
	int             i;

	if (NULL == run) {
		printf("ntfs_parsetun: run == NULL\n");
		return (EINVAL);
	}
	sz = run[(*off)++];
	if (0 == sz) {
		printf("ntfs_parserun: trying to go out of run\n");
		return (E2BIG);
	}
	*cl = 0;
	if ((sz & 0xF) > 8 || (*off) + (sz & 0xF) > len) {
		printf("ntfs_parserun: " \
		       "bad run: length too big: sz: 0x%02x (%ld < %ld + sz)\n",
		       sz, len, *off);
		return (EINVAL);
	}
	for (i = 0; i < (sz & 0xF); i++)
		*cl += (u_int32_t) run[(*off)++] << (i << 3);

	sz >>= 4;
	if ((sz & 0xF) > 8 || (*off) + (sz & 0xF) > len) {
		printf("ntfs_parserun: " \
		       "bad run: length too big: sz: 0x%02x (%ld < %ld + sz)\n",
		       sz, len, *off);
		return (EINVAL);
	}
	for (i = 0; i < (sz & 0xF); i++)
		*cn += (u_int32_t) run[(*off)++] << (i << 3);

	return (0);
}
#endif

/*
 * Process fixup routine on given buffer.
 */
__private_extern__
int
ntfs_procfixups(
		struct ntfsmount * ntmp,
		u_int32_t magic,
		caddr_t buf,
		size_t len)
{
	struct fixuphdr *fhp = (struct fixuphdr *) buf;
	int             i;
	u_int16_t       fixup;
	u_int16_t      *fxp;
	u_int16_t      *cfxp;
        u_int32_t	fixup_magic;
        u_int16_t	fixup_count;
        u_int16_t	fixup_offset;
        
        fixup_magic = le32toh(fhp->fh_magic);
	if (fixup_magic != magic) {
		printf("ntfs_procfixups: magic doesn't match: %08x != %08x\n",
		       fixup_magic, magic);
		return (EINVAL);
	}
        fixup_count = le16toh(fhp->fh_fnum);
	if ((fixup_count - 1) * ntmp->ntm_bps != len) {
		printf("ntfs_procfixups: " \
		       "bad fixups number: %d for %ld bytes block\n", 
		       fixup_count, (long)len);	/* XXX printf kludge */
		return (EINVAL);
	}
        fixup_offset = le16toh(fhp->fh_foff);
	if (fixup_offset >= ntmp->ntm_spc * ntmp->ntm_mftrecsz * ntmp->ntm_bps) {
		printf("ntfs_procfixups: invalid offset: %x", fixup_offset);
		return (EINVAL);
	}
	fxp = (u_int16_t *) (buf + fixup_offset);
	cfxp = (u_int16_t *) (buf + ntmp->ntm_bps - 2);
	fixup = *fxp++;
	for (i = 1; i < fixup_count; i++, fxp++) {
		if (*cfxp != fixup) {
			printf("ntfs_procfixups: fixup %d doesn't match\n", i);
			return (EINVAL);
		}
		*cfxp = *fxp;
		((caddr_t) cfxp) += ntmp->ntm_bps;
	}
	return (0);
}

#if UNUSED_CODE
int
ntfs_runtocn(
	     cn_t * cn,	
	     struct ntfsmount * ntmp,
	     u_int8_t * run,
	     u_long len,
	     cn_t vcn)
{
	cn_t            ccn = 0;
	cn_t            ccl = 0;
	u_long          off = 0;
	int             error = 0;

#if NTFS_DEBUG
	int             i;
	printf("ntfs_runtocn: run: 0x%p, %ld bytes, vcn:%ld\n",
		run, len, (u_long) vcn);
	printf("ntfs_runtocn: run: ");
	for (i = 0; i < len; i++)
		printf("0x%02x ", run[i]);
	printf("\n");
#endif

	if (NULL == run) {
		printf("ntfs_runtocn: run == NULL\n");
		return (EINVAL);
	}
	do {
		if (run[off] == 0) {
			printf("ntfs_runtocn: vcn too big\n");
			return (E2BIG);
		}
		vcn -= ccl;
		error = ntfs_parserun(&ccn, &ccl, run, len, &off);
		if (error) {
			printf("ntfs_runtocn: ntfs_parserun failed\n");
			return (error);
		}
	} while (ccl <= vcn);
	*cn = ccn + vcn;
	return (0);
}
#endif

/*
 * this initializes toupper table & dependant variables to be ready for
 * later work
 */
__private_extern__
void
ntfs_toupper_init()
{
	ntfs_toupper_tab = (u_int16_t *) NULL;
	ntfs_toupper_usecount = 0;
}

static void unicode_to_host(u_int16_t *unicode, u_int length)
{
#if defined(__BIG_ENDIAN__)
    u_int i;
    
    for (i=0; i<length; ++i)
        unicode[i] = le16toh(unicode[i]);
#elif defined(__LITTLE_ENDIAN__)
    /* Already little endian.  Nothing to do. */
#else
#error Unknown endianness.
#endif
}

/*
 * if the ntfs_toupper_tab[] is filled already, just raise use count;
 * otherwise read the data from the filesystem we are currently mounting
 */
__private_extern__
int
ntfs_toupper_use(mp, ntmp, p)
	mount_t mp;
	struct ntfsmount *ntmp;
	proc_t p;
{
	int error = 0;
	vnode_t vp;
	
	/* only read the translation data from a file if it hasn't been
	 * read already */
	if (ntfs_toupper_tab)
		goto out;

	/*
	 * Read in Unicode lowercase -> uppercase translation file.
	 * XXX for now, just the first 256 entries are used anyway,
	 * so don't bother reading more
	 */
	ntfs_toupper_tab = OSMalloc(65536 * sizeof(u_int16_t), ntfs_malloc_tag);
		
	error = ntfs_vgetex(mp, NTFS_UPCASEINO, NULLVP, NULL, VNON, NTFS_A_DATA, NULL, 0, p, &vp);
	if (error)
		goto out;
	error = ntfs_readattr(ntmp, VTONT(vp), NTFS_A_DATA, NULL, 0,
			65536*sizeof(u_int16_t), (char *) ntfs_toupper_tab, NULL, p);
	if (!error)
		unicode_to_host(ntfs_toupper_tab, 65536);
	vnode_put(vp);

out:
	ntfs_toupper_usecount++;
	return (error);
}

/*
 * lower the use count and if it reaches zero, free the memory
 * tied by toupper table
 */
__private_extern__
void
ntfs_toupper_unuse()
{
	ntfs_toupper_usecount--;
	if (ntfs_toupper_usecount == 0) {
		OSFree(ntfs_toupper_tab, 65536 * sizeof(u_int16_t), ntfs_malloc_tag);
		ntfs_toupper_tab = NULL;
	}
#ifdef DIAGNOSTIC
	else if (ntfs_toupper_usecount < 0) {
		panic("ntfs_toupper_unuse(): use count negative: %d\n",
			ntfs_toupper_usecount);
	}
#endif
} 

static int ntfs_unistrcmp(
	size_t s_len,
	const u_int16_t *s,	/* Note: little endian */
	size_t t_len,
	const u_int16_t *t)	/* Note: native endian */
{
	size_t i;
	int result;
	
	for (i=0; (i<s_len) && (i<t_len); ++i)
	{
		result = le16toh(s[i]) - t[i];
		if (result)
			return result;
	}
	return s_len - t_len;
}

static int ntfs_unistrcasecmp(
	size_t s_len,
	const u_int16_t *s,	/* Note: little endian */
	size_t t_len,
	const u_int16_t *t)	/* Note: native endian */
{
	size_t i;
	int result;
	
	for (i=0; (i<s_len) && (i<t_len); ++i)
	{
		result = NTFS_TOUPPER(le16toh(s[i])) - NTFS_TOUPPER(t[i]);
		if (result)
			return result;
	}
	return s_len - t_len;
}
