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
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/proc.h>

#include "webdav.h"

/*****************************************************************************/

/*
 * Structures associated with webdav noade cacheing.
 */
LIST_HEAD(webdav_hashhead, webdavnode) *webdav_hashtbl = NULL;
u_long webdavhash;  /* size of hash table - 1 */

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
}

/*****************************************************************************/

/*
 * Use the mp/fileid pair to find the vnode, and return a pointer
 * to it. If found but locked, wait for it.
 */
__private_extern__
vnode_t webdav_hashget(struct mount *mp, ino_t fileid)
{
	struct webdavnode *pt;
	vnode_t vp;
	int error;
	uint32_t vid;

	vp = NULLVP;
	pt = WEBDAVNODEHASH(mp, fileid)->lh_first;
	while (pt != NULL)
	{
		if ( (vnode_mount(WEBDAVTOV(pt)) == mp) &&
			 (pt->pt_fileid == fileid) )
		{
			/* found a matching webdavnode */
			if (ISSET(pt->pt_status, WEBDAV_INIT))
			{
				/*
				 * The webdavnode is being initialized.
				 * Wait for initialization to complete and then restart the search.
				 */
				SET(pt->pt_status, WEBDAV_WAITINIT);
				(void) msleep(pt, NULL, PINOD, "webdav_hashget", NULL);
			}
			else
			{
				vp = WEBDAVTOV(pt);
				vid = vnode_vid(vp);
				
				error = vnode_getwithvid(vp, vid);
				if ( error == 0 )
				{
					/* got it */
					break;
				}
			}
			/* restart the search */
			vp = NULLVP;
			pt = WEBDAVNODEHASH(mp, fileid)->lh_first;
		}
		else
		{
			/* next webdavnode */
			pt = pt->pt_hash.le_next;
		}
	}
	return ( vp );
}

/*****************************************************************************/

/*
 * Insert the inode into the hash table
 */
__private_extern__
void webdav_hashins(pt)
	struct webdavnode *pt;
{
	/*	put it on the appropriate hash list */
	LIST_INSERT_HEAD(WEBDAVNODEHASH(pt->pt_mountp, pt->pt_fileid), pt, pt_hash);
	pt->pt_status |= WEBDAV_ONHASHLIST;
}

/*****************************************************************************/

/*
 * Remove the inode from the hash table.
 */
__private_extern__
void webdav_hashrem(pt)
	struct webdavnode *pt;
{
	if (pt->pt_status & WEBDAV_ONHASHLIST)
	{
		LIST_REMOVE(pt, pt_hash);
		pt->pt_status &= ~WEBDAV_ONHASHLIST;
	}
}

/*****************************************************************************/
