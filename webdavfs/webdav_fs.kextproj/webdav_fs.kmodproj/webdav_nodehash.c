/* Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved */
/*
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * The NEXTSTEP Software License Agreement specifies the terms
 * and conditions for redistribution.
 *
 *	@(#)ufs_ihash.c 8.7 (Berkeley) 5/17/95
 */


/* A note about webdav node cacheing:
 * Because webdav nodes represent remote files and because webdav/http
 * is stateless, we do not want to rely on any information regarding a
 * old unopened vnode.	Thus webdav nodes stay in the hash list only while
 * they are referenced.	 The webdav inactive routine will see to it that they
 * are removed as soon as the count goes to zero. Likewise, our lookup
 * routine will only return the vnode if it's ref count is positive.
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/proc.h>

#include "webdav.h"
#include "vnops.h"

/*****************************************************************************/

/*
 * Structures associated with webdav noade cacheing.
 */
LIST_HEAD(webdav_hashhead, webdavnode) *webdav_hashtbl = 0;
u_long webdavhash;								/* size of hash table - 1 */
#define WEBDAVNODEHASH(length,depth,charac) (&webdav_hashtbl[(((length) << 16) + ((charac) << 12) + (depth)) & webdavhash])
#define WEBDAVUNIQUECHAR 5
#define WEBDAVGETCHARAC(url,length) (url)[(0>((int)(length))-WEBDAVUNIQUECHAR ? 0:((int)(length)) - WEBDAVUNIQUECHAR )]
struct slock webdav_hash_slock;

/*****************************************************************************/

/*
 * Initialize webdav hash table.
 */
void webdav_hashinit(void)
{
	webdav_hashtbl = hashinit(desiredvnodes, M_TEMP, &webdavhash);
	simple_lock_init(&webdav_hash_slock);
}

/*****************************************************************************/

/* Free webdav hash table. */
void webdav_hashdestroy(void)
{
	if (webdav_hashtbl)
	{
		FREE(webdav_hashtbl, M_TEMP);
	}
}

/*****************************************************************************/

/*
 * Use the fsid (in the mount struct), the depth the final char
 * and the url to find the vnode, and return a pointer
 * to it. If it is in core, return it, even if it is locked.
 */

struct vnode *webdav_hashlookup(depth, length, fsid, url)
	int depth, length;
	long fsid;
	char *url;
{
	struct webdavnode *pt;

	simple_lock(&webdav_hash_slock);
	for (pt = WEBDAVNODEHASH(length, depth, WEBDAVGETCHARAC(url, length))->lh_first;
		pt;
		pt = pt->pt_hash.le_next)
	{
		if (length == pt->pt_size &&
			(WEBDAVTOV(pt)->v_mount->mnt_stat.f_fsid.val[0] == fsid) &&
			!bcmp(url, pt->pt_arg, length))
		{
			break;
		}
	}
	simple_unlock(&webdav_hash_slock);

	if (pt && pt->pt_vnode->v_usecount > 0)
	{
		vref(pt->pt_vnode);
		return (WEBDAVTOV(pt));
	}

	return (NULLVP);
}

/*****************************************************************************/

/*
 * Use the the depth our guess at the most unique char
 * and the url to find the vnode, and return a pointer
 * to it. If it is in core, but locked, wait for it.
 */

struct vnode *webdav_hashget(depth, length, fsid, url)
	int depth, length;
	long fsid;
	char *url;
{
	struct proc *p = current_proc();			/* XXX */
	struct webdavnode *pt;
	struct vnode *vp;

loop:

	simple_lock(&webdav_hash_slock);
	for (pt = WEBDAVNODEHASH(length, depth, WEBDAVGETCHARAC(url, length))->lh_first;
		pt; 
		pt = pt->pt_hash.le_next)
	{
		if (length == pt->pt_size && 
			(WEBDAVTOV(pt)->v_mount->mnt_stat.f_fsid.val[0] == fsid) && 
			!bcmp(url, pt->pt_arg, length))
		{
			vp = WEBDAVTOV(pt);
			simple_lock(&vp->v_interlock);
			simple_unlock(&webdav_hash_slock);
			if (vget(vp, LK_EXCLUSIVE | LK_INTERLOCK, p))
			{
				goto loop;
			}
			return (vp);
		}
	}
	simple_unlock(&webdav_hash_slock);
	return (NULLVP);
}

/*****************************************************************************/

/*
 * Insert the inode into the hash table
 */
void webdav_hashins(pt)
	struct webdavnode *pt;
{
	struct webdav_hashhead *ptp;

	/*	put it on the appropriate hash list */

	simple_lock(&webdav_hash_slock);
	ptp = WEBDAVNODEHASH(pt->pt_size, pt->pt_depth, WEBDAVGETCHARAC(pt->pt_arg, pt->pt_size));
	LIST_INSERT_HEAD(ptp, pt, pt_hash);
	pt->pt_status |= WEBDAV_ONHASHLIST;
	simple_unlock(&webdav_hash_slock);
}

/*****************************************************************************/

/*
 * Remove the inode from the hash table.
 */
void webdav_hashrem(pt)
	struct webdavnode *pt;
{
	simple_lock(&webdav_hash_slock);
	if (pt->pt_status & WEBDAV_ONHASHLIST)
	{

		LIST_REMOVE(pt, pt_hash);
		pt->pt_status &= ~WEBDAV_ONHASHLIST;
#if DIAGNOSTIC
		pt->pt_hash.le_next = NULL;
		pt->pt_hash.le_prev = NULL;
#endif

	}
	simple_unlock(&webdav_hash_slock);
}

/*****************************************************************************/
