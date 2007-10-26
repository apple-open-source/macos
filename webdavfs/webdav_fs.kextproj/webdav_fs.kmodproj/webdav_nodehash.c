/*
 * Copyright (c) 1999-2004 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
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
/*
 * Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved
 */
/*
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * The NEXTSTEP Software License Agreement specifies the terms
 * and conditions for redistribution.
 *
 *	@(#)ufs_ihash.c 8.7 (Berkeley) 5/17/95
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mount.h>
#include <sys/ucred.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/proc.h>

#include "webdav.h"
#include "webdav_utils.h"

/*****************************************************************************/

/*
 * Structures associated with webdav noade cacheing.
 */
LIST_HEAD(webdav_hashhead, webdavnode) *webdav_hashtbl = NULL;
u_long webdavhash;  /* size of hash table - 1 */

lck_mtx_t *webdav_node_hash_mutex;	/* protects the hash table */
static lck_grp_t *webdav_node_hash_lck_grp;

/*
 * The keys are the mount address and the fileid. The mount address will prevent
 * collisions between mounts and the fileid is unique on a mount.
 */
#define WEBDAVNODEHASH(mp, fileid) (&webdav_hashtbl[((u_long)(mp) + (u_long)(fileid)) & webdavhash])

/*****************************************************************************/

/*
 * Initialize webdav hash table.
 */
__private_extern__
void webdav_hashinit(void)
{
	webdav_hashtbl = hashinit(desiredvnodes, M_TEMP, &webdavhash);
	webdav_node_hash_lck_grp = lck_grp_alloc_init("webdav_node_hash", LCK_GRP_ATTR_NULL);
	webdav_node_hash_mutex = lck_mtx_alloc_init(webdav_node_hash_lck_grp, LCK_ATTR_NULL);
}

/*****************************************************************************/

/*
 * Free webdav hash table.
 */
__private_extern__
void webdav_hashdestroy(void)
{
	if (webdav_hashtbl != NULL)
	{
		FREE(webdav_hashtbl, M_TEMP);
	}

	lck_mtx_free(webdav_node_hash_mutex, webdav_node_hash_lck_grp);
	lck_grp_free(webdav_node_hash_lck_grp);
}

/*****************************************************************************/

/*
 * Use the mp/fileid pair to find the webdavnode.
 * If found but busy, then wait for it.
 * 
 * 
 */
 __private_extern__
struct webdavnode *webdav_hashget(struct mount *mp, ino_t fileid, struct webdavnode *pt_new, uint32_t *inserted)
{
	struct webdavnode *pt, *pt_found;
	struct webdav_hashhead *nhp;
	vnode_t vp;
	uint32_t vid;

	vp = NULLVP;
	pt_found = NULL;
	nhp = WEBDAVNODEHASH(mp, fileid);

lockAndLoop:
	lck_mtx_lock(webdav_node_hash_mutex);
loop:
	for (pt = nhp->lh_first; pt != NULL; pt = pt->pt_hash.le_next)
	{
		if ( (mp != pt->pt_mountp) || (pt->pt_fileid != fileid) )
			continue;
			
		/* found a match */
		if (ISSET(pt->pt_status, WEBDAV_INIT))
		{
			/*
			 * The webdavnode is being initialized.
			 * Wait for initialization to complete and then restart the search.
			 */
			SET(pt->pt_status, WEBDAV_WAITINIT);
			msleep(pt, webdav_node_hash_mutex, PINOD, "webdav_hashget", NULL);
			goto loop;
		}

		vp = WEBDAVTOV(pt);
		vid = vnode_vid(vp);
		lck_mtx_unlock(webdav_node_hash_mutex);
			
		if (vnode_getwithvid(vp, vid))
		{
			/* vnode is being reclaimed, or has changed identity, so try again */
			goto lockAndLoop;
		}
		
		/* found what we needed */
		webdav_lock(pt, WEBDAV_EXCLUSIVE_LOCK);
		pt->pt_lastvop = webdav_hashget;
		pt_found = pt;
		*inserted = 0;
		return (pt_found);
	}
	
	/* Not found in the hash */
	if (pt_new != NULL)
	{
		/* insert the new node */
		pt_new->pt_status |= WEBDAV_ONHASHLIST;
		LIST_INSERT_HEAD(nhp, pt_new, pt_hash);
		webdav_lock(pt_new, WEBDAV_EXCLUSIVE_LOCK);
		pt_new->pt_lastvop = webdav_hashget;
		*inserted = 1;		
	}
	else
		*inserted = 0;
	lck_mtx_unlock(webdav_node_hash_mutex);
	
	return (pt_new);
}
 

/*****************************************************************************/

/*
 * Insert the inode into the hash table
 */
__private_extern__
void webdav_hashins(pt)
	struct webdavnode *pt;
{
	lck_mtx_lock(webdav_node_hash_mutex);
	/*	put it on the appropriate hash list */
	LIST_INSERT_HEAD(WEBDAVNODEHASH(pt->pt_mountp, pt->pt_fileid), pt, pt_hash);
	pt->pt_status |= WEBDAV_ONHASHLIST;
	lck_mtx_unlock(webdav_node_hash_mutex);
}

/*****************************************************************************/

/*
 * Remove the inode from the hash table.
 */
__private_extern__
void webdav_hashrem(pt)
	struct webdavnode *pt;
{
	lck_mtx_lock(webdav_node_hash_mutex);
	if (pt->pt_status & WEBDAV_ONHASHLIST)
	{
		LIST_REMOVE(pt, pt_hash);
		pt->pt_status &= ~WEBDAV_ONHASHLIST;
	}
	lck_mtx_unlock(webdav_node_hash_mutex);
}

/*****************************************************************************/
