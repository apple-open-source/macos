/*
 * Copyright (c) 2000-2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */
/* Copyright (c) 1998, Apple Computer, Inc. All rights reserved. */
/*
 * Change History:
 *
 *	01-Jen-2004	Alfred Perlstein	Autofs.
 *	17-Aug-1999	Pat Dirks		New today.
 *
 * $Id: autofs_util.c,v 1.16 2005/03/12 03:18:54 lindak Exp $
 */

#include <mach/mach_types.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/mount.h>
#include <sys/vnode.h>
#include <sys/malloc.h>
#include <sys/dirent.h>
#include <sys/namei.h>
#include <sys/attr.h>
#include <sys/time.h>
#include <sys/uio.h>
#include <sys/vm.h>
#include <sys/errno.h>
#include <vfs/vfs_support.h>
#include <kern/locks.h>

#include "autofs.h"

lck_grp_t *autofs_lck_grp;

struct autofs_direntry_head {
	u_int32_t d_fileno;		/* file number of entry */
	u_int16_t d_reclen;		/* length of this record */
	u_int8_t  d_type; 		/* file type, see below */
	u_int8_t  d_namlen;		/* length of string in d_name */
};

static int autofs_newnode(mount_t mp, vnode_t dp, const char *name,
    unsigned long nodeid, enum autofsnodetype type, mode_t mode,
    vnode_t *vpp, vfs_context_t context);

/*
 * Make newnode a child of parent.  Parent must be a directory.
 */
static int
autofs_insertnode(
	struct autofsnode *an,
	struct autofsnode *parent_an
) {
	struct timeval now;

	DBG_ASSERT(parent_an->s_type == AUTOFS_DIRECTORY);

	TAILQ_INSERT_TAIL(&parent_an->s_u.d.d_subnodes, an, s_sibling);
	parent_an->s_linkcount++;
	parent_an->s_u.d.d_entrycount++;
	parent_an->s_nodeflags |= IN_CHANGE | IN_MODIFIED;

	an->s_parent = parent_an;
	microtime(&now);
	autofs_update(ATOV(parent_an), &now, &now, 0);
	return (0);
}

int
autofs_getnewvnode(mount_t mp, struct autofsnode *sp, enum vtype vtype, unsigned long nodeid, vnode_t *vpp)
{
	errno_t result;
	struct vnode_fsparam vfsp;

	bzero(&vfsp, sizeof(struct vnode_fsparam));
	vfsp.vnfs_mp = mp;
	vfsp.vnfs_vtype = vtype;
	vfsp.vnfs_str = "autofs";
	vfsp.vnfs_dvp = 0;
	vfsp.vnfs_fsnode = sp;
	vfsp.vnfs_vops = autofs_vnodeop_p;
	vfsp.vnfs_markroot = (nodeid == ROOT_DIRID);
	vfsp.vnfs_marksystem = 0;
	vfsp.vnfs_rdev = 0;
	vfsp.vnfs_filesize = 0;
	vfsp.vnfs_cnp = NULL;
	vfsp.vnfs_flags = VNFS_NOCACHE | VNFS_CANTCACHE;

	result = vnode_create(VNCREATE_FLAVOR, VCREATESIZE, &vfsp, vpp); 
    if (result != 0) {
		DBG_VOP(("getnewvnode failed with error code %d\n", result));
		return result;
	}
	
	sp->s_vp = *vpp;
	
	return (0);
}

int
autofs_clonenode(mount_t mp, struct autofsnode *an, struct autofsnode **anp, uid_t uid, vfs_context_t context) {
	vnode_t vp;
	vnode_t newvp;
	struct autofsnode *an2;
	int error;
	
	*anp = NULL;
	
	error = autofs_new_directory(mp, NULL,
		an->s_name, an->s_nodeid,
		an->s_mode, &newvp, context);
	if (error) return error;
	
	/* we got it, setup/clone the relevant fields. */
	/* NOTE: s_parent is deliberately not cloned; PARENTNODE derives it dynamically from the original node */
	an2 = VTOA(newvp);
	an2->s_parent = NULL;
	an2->s_mounterpid = an->s_mounterpid;
	an2->s_cloneuid = uid;
	an2->s_nodeflags |= an->s_nodeflags | IN_CLONE;
	an2->s_nodeflags &= ~IN_UID;
	an2->s_uid = uid;
	an2->s_gid = an->s_gid;
	an2->s_createtime = an->s_createtime;
	an2->s_accesstime = an->s_accesstime;
	an2->s_modificationtime = an->s_modificationtime;
	an2->s_changetime = an->s_changetime;

	/*
	 * We could have blocked so check for a race.
	 * It's ok to clobber 'an2' as 'newvp' will have our
	 * new vnode.
	 */
	TAILQ_FOREACH(an2, &an->s_clonehd, s_clonelst) {
		if ((uid == an2->s_cloneuid) &&
			((error = autofs_to_vnode(mp, an2, &vp)) == 0)) break;
	}

	if (an2) {
		lck_rw_unlock_exclusive(VTOA(newvp)->s_lock);
		VNODE_PUT(newvp);
		vnode_recycle(newvp);
	} else {
		DBG_VOP(("autofs_clonenode: adding new UID child."));
		an2 = VTOA(newvp);
		an2->s_clonedfrom = an;
		TAILQ_INSERT_TAIL(&an->s_clonehd, an2, s_clonelst);
		lck_rw_unlock_exclusive(VTOA(newvp)->s_lock);
	}
	
	*anp = an2;
	return 0;
}

/*
 * Allocate a new autofs node, returns the vnode back locked
 * if successful.  
 * This should only be used internally by new_directory/new_symlink.
 */
static int
autofs_newnode(
	mount_t mp,
	vnode_t dp,
	const char *name,
	unsigned long nodeid,
	enum autofsnodetype type,
	mode_t mode,
	vnode_t *vpp,
	vfs_context_t context
) {
	int result;
	struct autofsnode *sp;
	vnode_t vp;
	struct timeval now;
	char *nodename;
	struct autofs_mntdata *amp = VFSTOAFS(mp);
	enum vtype vtype = VNON;

	switch (type) {
	  case AUTOFS_DIRECTORY:
		vtype = VDIR;
		break;
	  case AUTOFS_SYMLINK:
		vtype = VLNK;
		break;
	  default:
		panic("autofs_newnode: unknown node type %d", type);
		break;
	}

	MALLOC(sp, struct autofsnode *, sizeof(struct autofsnode), M_AUTOFS, M_WAITOK);
	bzero(sp, sizeof(*sp));
	sp->s_type = type;

	if (name == NULL) {
		MALLOC(nodename, char *, 1, M_TEMP, M_WAITOK);
		nodename[0] = 0;
	} else {
		MALLOC(nodename, char *, strlen(name) + 1, M_TEMP, M_WAITOK);
		strcpy(nodename, name);
	}

	/* Initialize the relevant autofsnode fields: */
	sp->s_lock = lck_rw_alloc_init(autofs_lck_grp, NULL);
	lck_rw_lock_exclusive(sp->s_lock);
	
	sp->s_nodeid = nodeid;

	/* Initialize all times from a consistent snapshot of the clock: */
	microtime(&now);
	sp->s_createtime = now;
	sp->s_accesstime = now;
	sp->s_modificationtime = now;
	sp->s_changetime = now;
	sp->s_name = nodename;
	sp->s_mode = mode;

	result = autofs_getnewvnode(mp, sp, vtype, nodeid, &vp);
	if (result != 0) {
		if (vp) {
			vnode_put(vp);
			vnode_recycle(vp);
		};
		DBG_VOP(("autofs_getnewvnode failed with error code %d\n", result));
		FREE(nodename, M_TEMP);
		FREE(sp, M_TEMP);
		return (result);
	}
	if (vp == NULL) {
		DBG_VOP(("autofs_getnewvnode returned NULL without an error!\n"));
		FREE(nodename, M_TEMP);
		FREE(sp, M_TEMP);
		return (EINVAL);
	}

	TAILQ_INSERT_TAIL(&amp->autofs_nodes, sp, s_mntlst);
	TAILQ_INIT(&sp->s_clonehd);
	amp->autofs_nodecnt++;

	/*
	 * If there's a parent directory, update its subnode structures
	 * to insert this new node:
	 */
	if (dp) {
		result = autofs_insertnode(sp, VTOA(dp));
	}

	*vpp = vp;

	return (result);
}

void
autofs_destroynode(struct autofsnode *an)
{
	struct autofs_mntdata *amp;
	char *name;

	amp = ATOAFS(an);
	name = an->s_name;
	if (an->s_clonedfrom != NULL) {
		DBG_VOP(("autofs_destroynode: removing from clonelst."));
		TAILQ_REMOVE(&an->s_clonedfrom->s_clonehd, an, s_clonelst);
	}
	TAILQ_REMOVE(&amp->autofs_nodes, an, s_mntlst);
	amp->autofs_nodecnt--;
	if (amp->autofs_nodecnt < 0) printf("autofs_destroy: amp->autofs_nodecnt < 0\n");
	FREE(name, M_TEMP);
	FREE(an, M_AUTOFS);
	an->s_name = NULL;
}


int
autofs_remove_entry(struct autofsnode *sp)
{
	/* Unlike PARENTNODE(sp) this is properly NULL for clones, non-null for originals */
	struct autofsnode *psp = sp->s_parent;
	struct timeval now;

	if (sp->s_nodeflags & IN_UID && !TAILQ_EMPTY(&sp->s_clonehd)) {
		DBG_VOP(("autofs_remove_entry: IN_UID with children denied."));
		return (ENOTEMPTY);
	}

	switch (sp->s_type) {
	case AUTOFS_DIRECTORY:
		if (sp->s_linkcount < 2) {
			DBG_VOP(("autofs_remove_entry: bad link count "
				"%d < 2!\n", sp->s_linkcount));
		}

		if (sp->s_linkcount > 2) {
			DBG_VOP(("autofs_remove_entry: "
				"ENOTEMPTY (link cnt %d > 2)!\n",
				sp->s_linkcount));
			return (ENOTEMPTY);
		}

		if (psp != NULL && (sp->s_type == AUTOFS_DIRECTORY) &&
		    (psp != sp)) {
			/*
			 * account for the [fictitious] ".."
			 * link now removed
			 */
			--psp->s_linkcount;
		}

		/* one less for the "." entry. */
		sp->s_linkcount--;
		break;

	case AUTOFS_SYMLINK:
		FREE(sp->s_u.s.s_symlinktarget, M_TEMP);
		break;

	default:
		panic("autofs_remove_entry: unknown type %d", sp->s_type);
		break;
	}

	if (psp) {
		TAILQ_REMOVE(&psp->s_u.d.d_subnodes, sp, s_sibling);
		psp->s_u.d.d_entrycount--;
		psp->s_linkcount--;
		psp->s_nodeflags |= IN_CHANGE | IN_MODIFIED;
		microtime(&now);
		if (ATOV(psp) != NULL) autofs_update(ATOV(psp), &now, &now, 0);
		DBG_VOP(("autofs_remove_entry: parent s_linkcount = %d\n",
			    psp->s_linkcount));
	}

	if (sp->s_clonedfrom != NULL) {
		DBG_VOP(("autofs_remove_entry: removing from clonelst."));
		TAILQ_REMOVE(&sp->s_clonedfrom->s_clonehd, sp, s_clonelst);
	}
	sp->s_linkcount--;

	DBG_VOP(("autofs_remove_entry: s_linkcount = %d\n", sp->s_linkcount));
	return (0);
}

int
autofs_new_directory(
	mount_t mp,
	vnode_t dp,
	const char *name,
	unsigned long nodeid,
	mode_t mode,
	vnode_t *vpp,
	vfs_context_t context
) {
	int error;
	struct autofsnode *sp;

	error = autofs_newnode(mp, dp, name, nodeid, AUTOFS_DIRECTORY, mode, vpp, context);
	if (error) return (error);
	sp = VTOA(*vpp);
	sp->s_linkcount = 2;

	if (dp) {
		/* Account for the [fictitious] ".." link */
		++VTOA(dp)->s_linkcount;
	}

	/* No entries in this directory yet */
	sp->s_u.d.d_entrycount = 0;
	/* No subnodes of this directory yet */
	TAILQ_INIT(&sp->s_u.d.d_subnodes);

	return (0);
}

int
autofs_new_symlink(
	mount_t mp,
	vnode_t dp,
	const char *name,
	unsigned long nodeid,
	char *targetstring,
	vnode_t *vpp,
	vfs_context_t context
) {
	int error;
	vnode_t vp;
	struct autofsnode *sp;

	error = autofs_newnode(mp, dp, name, nodeid, AUTOFS_SYMLINK, 0, &vp, context);
	if (error) return (error);
	sp = VTOA(vp);
	sp->s_linkcount = 1;

	/* Set up the symlink-specific fields: */
	sp->s_type = AUTOFS_SYMLINK;
	sp->s_u.s.s_length = strlen(targetstring);
	MALLOC(sp->s_u.s.s_symlinktarget, char *, sp->s_u.s.s_length + 1, M_TEMP, M_WAITOK);
	strcpy(sp->s_u.s.s_symlinktarget, targetstring);

	*vpp = vp;

	return (0);
}

long
autofs_adddirentry(
	u_int32_t fileno,
	u_int8_t type,
	const char *name,
	struct autofsnode *ap,
	struct uio *uio)
{
	struct autofs_direntry_head direntry;
	long namelength;
	int padding;
	long padtext = 0;
	unsigned short direntrylength;

	namelength = ((name == NULL) ? 0 : strlen(name) + 1);
	padding = (4 - (namelength & 3)) & 3;
	direntrylength =
	    sizeof(struct autofs_direntry_head) + namelength + padding;

	direntry.d_fileno = fileno;
	direntry.d_reclen = direntrylength;
	switch (type) {
	  case AUTOFS_DIRECTORY:
		direntry.d_type = ((ap->s_nodeflags & IN_TRIGGER) && (ap->s_clonedfrom == NULL)) ? DT_AUTO : DT_DIR;
		break;
	  case AUTOFS_SYMLINK:
		direntry.d_type = DT_LNK;
		break;
	  default:
		panic("autofs_adddirentry: unknown type %d", type);
		break;
	}
	direntry.d_namlen = namelength == 0 ? 0 : namelength - 1;

	if (uio_resid(uio) < direntry.d_reclen) {
		direntrylength = 0;
	} else {
		uiomove((caddr_t)(&direntry), sizeof(direntry), uio);
		if (name != NULL) {
			uiomove((caddr_t)name, namelength, uio);
		}
		if (padding > 0) {
			uiomove((caddr_t)&padtext, padding, uio);
		}
	}

	return (direntrylength);
}
