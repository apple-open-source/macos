/*
 * Copyright (c) 2003 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * Copyright (c) 1999-2003 Apple Computer, Inc.  All Rights Reserved.
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
/*	$NetBSD: ntfs_ihash.c,v 1.5 1999/09/30 16:56:40 jdolecek Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1991, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)ufs_ihash.c	8.7 (Berkeley) 5/17/95
 * $FreeBSD: src/sys/fs/ntfs/ntfs_ihash.c,v 1.18 2002/04/04 21:03:19 jhb Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <libkern/OSMalloc.h>
#include "ntfs.h"
#include "ntfs_inode.h"
#include "ntfs_ihash.h"


/*
 * Structures associated with inode cacheing.
 */
static LIST_HEAD(nthashhead, ntnode) *ntfs_nthashtbl;
static u_long	ntfs_nthash;		/* size of hash table - 1 */
#define	NTNOHASH(device, inum)	(&ntfs_nthashtbl[(minor(device) + (inum)) & ntfs_nthash])

/*
 * The lock group for ntnodes.
 */
lck_grp_t *ntfs_lck_grp;
lck_grp_attr_t *ntfs_lck_grp_attr;

/*
 * Initialize inode hash table.
 */
__private_extern__
void
ntfs_nthashinit()
{
	ntfs_nthashtbl = hashinit(desiredvnodes, M_TEMP, &ntfs_nthash);
}

/*
 * Destroy inode hash table.
 */
__private_extern__
void
ntfs_nthashdestroy(void)
{
	/* Nothing to do */
}

/*
 * Use the device/inum pair to find the incore inode, and return a pointer
 * to it. If it is in core, return it, even if it is locked.
 */
__private_extern__
struct ntnode *
ntfs_nthashlookup(dev, inum)
	dev_t dev;
	ino_t inum;
{
	struct ntnode *ip;

	LIST_FOREACH(ip, NTNOHASH(dev, inum), i_hash)
		if (inum == ip->i_number && dev == ip->i_dev)
			break;

	return (ip);
}

/*
 * Insert the ntnode into the hash table.
 */
__private_extern__
void
ntfs_nthashins(ip)
	struct ntnode *ip;
{
	struct nthashhead *ipp;

	ipp = NTNOHASH(ip->i_dev, ip->i_number);
	LIST_INSERT_HEAD(ipp, ip, i_hash);
	ip->i_flag |= IN_HASHED;
}

/*
 * Remove the inode from the hash table.
 */
__private_extern__
void
ntfs_nthashrem(ip)
	struct ntnode *ip;
{
	if (ip->i_flag & IN_HASHED) {
		ip->i_flag &= ~IN_HASHED;
		LIST_REMOVE(ip, i_hash);
	}
}
