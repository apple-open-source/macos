/*
 * Copyright (c) 2000-2015 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */
/* Copyright (c) 1995 NeXT Computer, Inc. All Rights Reserved */
/*
 * Copyright (c) 1982, 1986, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)vfs_lookup.c	8.10 (Berkeley) 5/27/95
 */
/*
 * NOTICE: This file was modified by SPARTA, Inc. in 2005 to introduce
 * support for mandatory and extensible security protections.  This notice
 * is included in support of clause 2.2 (b) of the Apple Public License,
 * Version 2.0.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/syslimits.h>
#include <sys/time.h>
#include <sys/namei.h>
#include <sys/vm.h>
#include <sys/vnode_internal.h>
#include <sys/mount_internal.h>
#include <sys/errno.h>
#include <kern/kalloc.h>
#include <sys/filedesc.h>
#include <sys/proc_internal.h>
#include <sys/kdebug.h>
#include <sys/unistd.h>         /* For _PC_NAME_MAX */
#include <sys/uio_internal.h>
#include <sys/kauth.h>
#include <kern/zalloc.h>
#include <security/audit/audit.h>
#if CONFIG_MACF
#include <security/mac_framework.h>
#endif
#include <os/atomic_private.h>

#include <sys/paths.h>

#if NAMEDRSRCFORK
#include <sys/xattr.h>
#endif
/*
 * The minimum volfs-style pathname is 9.
 * Example:  "/.vol/1/2"
 */
#define VOLFS_MIN_PATH_LEN  9


#if CONFIG_VOLFS
static int vfs_getrealpath(const char * path, char * realpath, size_t bufsize, vfs_context_t ctx);
#define MAX_VOLFS_RESTARTS 5
#endif

static int              lookup_traverse_mountpoints(struct nameidata *ndp, struct componentname *cnp, vnode_t dp, int vbusyflags, vfs_context_t ctx);
static int              lookup_handle_symlink(struct nameidata *ndp, vnode_t *new_dp, bool* dp_has_iocount, vfs_context_t ctx);
static int              lookup_authorize_search(vnode_t dp, struct componentname *cnp, int dp_authorized_in_cache, vfs_context_t ctx);
static void             lookup_consider_update_cache(vnode_t dvp, vnode_t vp, struct componentname *cnp, int nc_generation);
static int              lookup_handle_found_vnode(struct nameidata *ndp, struct componentname *cnp, int rdonly,
    int vbusyflags, int *keep_going, int nc_generation,
    int wantparent, int atroot, vfs_context_t ctx);
static int              lookup_handle_emptyname(struct nameidata *ndp, struct componentname *cnp, int wantparent);

#if NAMEDRSRCFORK
static int              lookup_handle_rsrc_fork(vnode_t dp, struct nameidata *ndp, struct componentname *cnp, int wantparent, vfs_context_t ctx);
#endif

extern lck_rw_t rootvnode_rw_lock;

#define RESOLVE_NOFOLLOW_ANY  0x00000001
#define RESOLVE_CHECKED       0x80000000
static int              lookup_check_for_resolve_prefix(char *path, size_t pathbuflen, size_t len, uint32_t *resolve_flags, size_t *prefix_len);

/*
 * Convert a pathname into a pointer to a locked inode.
 *
 * The FOLLOW flag is set when symbolic links are to be followed
 * when they occur at the end of the name translation process.
 * Symbolic links are always followed for all other pathname
 * components other than the last.
 *
 * The segflg defines whether the name is to be copied from user
 * space or kernel space.
 *
 * Overall outline of namei:
 *
 *	copy in name
 *	get starting directory
 *	while (!done && !error) {
 *		call lookup to search path.
 *		if symbolic link, massage name in buffer and continue
 *	}
 *
 * Returns:	0			Success
 *		ENOENT			No such file or directory
 *		ELOOP			Too many levels of symbolic links
 *		ENAMETOOLONG		Filename too long
 *		copyinstr:EFAULT	Bad address
 *		copyinstr:ENAMETOOLONG	Filename too long
 *		lookup:EBADF		Bad file descriptor
 *		lookup:EROFS
 *		lookup:EACCES
 *		lookup:EPERM
 *		lookup:ERECYCLE	 vnode was recycled from underneath us in lookup.
 *						 This means we should re-drive lookup from this point.
 *		lookup: ???
 *		VNOP_READLINK:???
 */
int
namei(struct nameidata *ndp)
{
	struct vnode *dp;       /* the directory we are searching */
	struct vnode *usedvp = ndp->ni_dvp;  /* store pointer to vp in case we must loop due to
	                                      *                                          heavy vnode pressure */
	uint32_t cnpflags = ndp->ni_cnd.cn_flags; /* store in case we have to restore after loop */
	int error;
	struct componentname *cnp = &ndp->ni_cnd;
	vfs_context_t ctx = cnp->cn_context;
	proc_t p = vfs_context_proc(ctx);
#if CONFIG_AUDIT
/* XXX ut should be from context */
	uthread_t ut = current_uthread();
#endif

#if CONFIG_VOLFS
	int volfs_restarts = 0;
#endif
	size_t bytes_copied = 0;
	size_t resolve_prefix_len = 0;
	vnode_t rootdir_with_usecount = NULLVP;
	vnode_t startdir_with_usecount = NULLVP;
	vnode_t usedvp_dp = NULLVP;
	int32_t old_count = 0;
	uint32_t resolve_flags = 0;
	int resolve_error = 0;
	bool dp_has_iocount = false;

#if DIAGNOSTIC
	if (!vfs_context_ucred(ctx) || !p) {
		panic("namei: bad cred/proc");
	}
	if (cnp->cn_nameiop & (~OPMASK)) {
		panic("namei: nameiop contaminated with flags");
	}
	if (cnp->cn_flags & OPMASK) {
		panic("namei: flags contaminated with nameiops");
	}
#endif

	/*
	 * A compound VNOP found something that needs further processing:
	 * either a trigger vnode, a covered directory, or a symlink.
	 */
	if (ndp->ni_flag & NAMEI_CONTLOOKUP) {
		int rdonly, vbusyflags, keep_going, wantparent;

		rdonly = cnp->cn_flags & RDONLY;
		vbusyflags = ((cnp->cn_flags & CN_NBMOUNTLOOK) != 0) ? LK_NOWAIT : 0;
		keep_going = 0;
		wantparent = cnp->cn_flags & (LOCKPARENT | WANTPARENT);

		ndp->ni_flag &= ~(NAMEI_CONTLOOKUP);

		error = lookup_handle_found_vnode(ndp, &ndp->ni_cnd, rdonly, vbusyflags,
		    &keep_going, ndp->ni_ncgeneration, wantparent, 0, ctx);
		if (error) {
			goto out_drop;
		}
		if (keep_going) {
			if ((cnp->cn_flags & ISSYMLINK) == 0) {
				panic("We need to keep going on a continued lookup, but for vp type %d (tag %d)", ndp->ni_vp->v_type, ndp->ni_vp->v_tag);
			}
			goto continue_symlink;
		}

		return 0;
	}

vnode_recycled:

	/*
	 * Get a buffer for the name to be translated, and copy the
	 * name into the buffer.
	 */
	if ((cnp->cn_flags & HASBUF) == 0) {
		cnp->cn_pnbuf = ndp->ni_pathbuf;
		cnp->cn_pnlen = PATHBUFLEN;
	}

retry_copy:
	if (UIO_SEG_IS_USER_SPACE(ndp->ni_segflg)) {
		error = copyinstr(ndp->ni_dirp, cnp->cn_pnbuf,
		    cnp->cn_pnlen, &bytes_copied);
	} else {
		error = copystr(CAST_DOWN(void *, ndp->ni_dirp), cnp->cn_pnbuf,
		    cnp->cn_pnlen, &bytes_copied);
	}
	if (error == ENAMETOOLONG && !(cnp->cn_flags & HASBUF)) {
		if (bytes_copied == PATHBUFLEN) {
			resolve_error = lookup_check_for_resolve_prefix(cnp->cn_pnbuf, PATHBUFLEN,
			    PATHBUFLEN, &resolve_flags, &resolve_prefix_len);
			/* errors from copyinstr take precedence over resolve_error */
			if (!resolve_error && resolve_prefix_len) {
				ndp->ni_dirp += resolve_prefix_len;
				resolve_prefix_len = 0;
			}
		}

		cnp->cn_pnbuf = zalloc(ZV_NAMEI);
		cnp->cn_flags |= HASBUF;
		cnp->cn_pnlen = MAXPATHLEN;
		bytes_copied = 0;

		goto retry_copy;
	}
	if (error) {
		goto error_out;
	} else if (resolve_error) {
		error = resolve_error;
		goto error_out;
	}
	assert(bytes_copied <= MAXPATHLEN);
	ndp->ni_pathlen = (u_int)bytes_copied;
	bytes_copied = 0;

	if (!(resolve_flags & RESOLVE_CHECKED)) {
		assert(!(cnp->cn_flags & HASBUF) && (cnp->cn_pnlen == PATHBUFLEN));
		error = lookup_check_for_resolve_prefix(cnp->cn_pnbuf, cnp->cn_pnlen, ndp->ni_pathlen,
		    &resolve_flags, &resolve_prefix_len);
		if (error) {
			goto error_out;
		}
		if (resolve_prefix_len) {
			/*
			 * Since this is pointing to the static path buffer instead of a zalloc'ed memorry,
			 * we're not going to attempt to free this, so it is perfectly fine to change the
			 * value of cnp->cn_pnbuf.
			 */
			cnp->cn_pnbuf += resolve_prefix_len;
			cnp->cn_pnlen -= resolve_prefix_len;
			ndp->ni_pathlen -= resolve_prefix_len;
			resolve_prefix_len = 0;
		}
	}

	/* At this point we should have stripped off the prefix from the path that has to be looked up */
	assert((resolve_flags & RESOLVE_CHECKED) && (resolve_prefix_len == 0));

	/*
	 * Since the name cache may contain positive entries of
	 * the incorrect case, force lookup() to bypass the cache
	 * and call directly into the filesystem for each path
	 * component. Note: the FS may still consult the cache,
	 * but can apply rules to validate the results.
	 */
	if (proc_is_forcing_hfs_case_sensitivity(p)) {
		cnp->cn_flags |= CN_SKIPNAMECACHE;
	}

#if CONFIG_VOLFS
	/*
	 * Check for legacy volfs style pathnames.
	 *
	 * For compatibility reasons we currently allow these paths,
	 * but future versions of the OS may not support them.
	 */
	if (ndp->ni_pathlen >= VOLFS_MIN_PATH_LEN &&
	    cnp->cn_pnbuf[0] == '/' &&
	    cnp->cn_pnbuf[1] == '.' &&
	    cnp->cn_pnbuf[2] == 'v' &&
	    cnp->cn_pnbuf[3] == 'o' &&
	    cnp->cn_pnbuf[4] == 'l' &&
	    cnp->cn_pnbuf[5] == '/') {
		char * realpath;
		int realpath_err;
		/* Attempt to resolve a legacy volfs style pathname. */
		realpath = zalloc(ZV_NAMEI);
		/*
		 * We only error out on the ENAMETOOLONG cases where we know that
		 * vfs_getrealpath translation succeeded but the path could not fit into
		 * MAXPATHLEN characters.  In other failure cases, we may be dealing with a path
		 * that legitimately looks like /.vol/1234/567 and is not meant to be translated
		 */
		if ((realpath_err = vfs_getrealpath(&cnp->cn_pnbuf[6], realpath, MAXPATHLEN, ctx))) {
			zfree(ZV_NAMEI, realpath);
			if (realpath_err == ENOSPC || realpath_err == ENAMETOOLONG) {
				error = ENAMETOOLONG;
				goto error_out;
			}
		} else {
			size_t tmp_len;
			if (cnp->cn_flags & HASBUF) {
				zfree(ZV_NAMEI, cnp->cn_pnbuf);
			}
			cnp->cn_pnbuf = realpath;
			cnp->cn_pnlen = MAXPATHLEN;
			tmp_len = strlen(realpath) + 1;
			assert(tmp_len <= UINT_MAX);
			ndp->ni_pathlen = (u_int)tmp_len;
			cnp->cn_flags |= HASBUF | CN_VOLFSPATH;
		}
	}
#endif /* CONFIG_VOLFS */

#if CONFIG_AUDIT
	/* If we are auditing the kernel pathname, save the user pathname */
	if (cnp->cn_flags & AUDITVNPATH1) {
		AUDIT_ARG(upath, ut->uu_cdir, cnp->cn_pnbuf, ARG_UPATH1);
	}
	if (cnp->cn_flags & AUDITVNPATH2) {
		AUDIT_ARG(upath, ut->uu_cdir, cnp->cn_pnbuf, ARG_UPATH2);
	}
#endif /* CONFIG_AUDIT */

	/*
	 * Do not allow empty pathnames
	 */
	if (*cnp->cn_pnbuf == '\0') {
		error = ENOENT;
		goto error_out;
	}
	if (ndp->ni_flag & NAMEI_NOFOLLOW_ANY || (resolve_flags & RESOLVE_NOFOLLOW_ANY)) {
		ndp->ni_loopcnt = MAXSYMLINKS;
	} else {
		ndp->ni_loopcnt = 0;
	}

	/*
	 * determine the starting point for the translation.
	 */
	proc_dirs_lock_shared(p);
	lck_rw_lock_shared(&rootvnode_rw_lock);

	if (!(ndp->ni_flag & NAMEI_ROOTDIR)) {
		if (fdt_flag_test(&p->p_fd, FD_CHROOT)) {
			ndp->ni_rootdir = p->p_fd.fd_rdir;
		} else {
			ndp->ni_rootdir = rootvnode;
		}
	}

	if (!ndp->ni_rootdir) {
		if (ndp->ni_flag & NAMEI_ROOTDIR) {
			panic("NAMEI_ROOTDIR is set but ni_rootdir is not\n");
		} else if (fdt_flag_test(&p->p_fd, FD_CHROOT)) {
			/* This should be a panic */
			printf("p->p_fd.fd_rdir is not set\n");
		} else {
			printf("rootvnode is not set\n");
		}
		lck_rw_unlock_shared(&rootvnode_rw_lock);
		proc_dirs_unlock_shared(p);
		error = ENOENT;
		goto error_out;
	}

	cnp->cn_nameptr = cnp->cn_pnbuf;

	ndp->ni_usedvp = NULLVP;

	if (*(cnp->cn_nameptr) == '/') {
		while (*(cnp->cn_nameptr) == '/') {
			cnp->cn_nameptr++;
			ndp->ni_pathlen--;
		}
		dp = ndp->ni_rootdir;
	} else if (cnp->cn_flags & USEDVP) {
		dp = ndp->ni_dvp;
		ndp->ni_usedvp = dp;
		usedvp_dp = dp;
	} else {
		dp = vfs_context_cwd(ctx);
	}

	if (dp == NULLVP || (dp->v_lflag & VL_DEAD)) {
		dp = NULLVP;
		lck_rw_unlock_shared(&rootvnode_rw_lock);
		proc_dirs_unlock_shared(p);
		error = ENOENT;
		goto error_out;
	}

	/*
	 * We need our own usecount on the root vnode and the starting dir across
	 * the lookup. There's two things that be done here. We can hold the locks
	 * (which protect the existing usecounts on the directories) across the
	 * lookup or take our own usecount. Holding the locks across the lookup can
	 * cause deadlock issues if we re-enter namei on the same thread so the
	 * correct thing to do is to acquire our own usecount.
	 *
	 * Ideally, the usecount should be obtained by vnode_get->vnode_ref->vnode_put.
	 * However when this vnode is the rootvnode, that sequence will produce a
	 * lot of vnode mutex locks and  unlocks on a single vnode (the rootvnode)
	 * and will be highly contended and degrade performance. Since we have
	 * an existing usecount protected by the locks we hold, we'll just use
	 * an atomic op to increment the usecount on a vnode which already has one
	 * and can't be released because we have the locks which protect against that
	 * happening.
	 */
	rootdir_with_usecount = ndp->ni_rootdir;
	old_count = os_atomic_inc_orig(&rootdir_with_usecount->v_usecount, relaxed);
	if (old_count < 1) {
		panic("(1) invalid pre-increment usecount (%d) for rootdir vnode %p",
		    old_count, rootdir_with_usecount);
	} else if (old_count == INT32_MAX) {
		panic("(1) usecount overflow for vnode %p", rootdir_with_usecount);
	}

	if ((dp != rootdir_with_usecount) && (dp != usedvp_dp)) {
		old_count = os_atomic_inc_orig(&dp->v_usecount, relaxed);
		if (old_count < 1) {
			panic("(2) invalid pre-increment usecount (%d) for vnode %p", old_count, dp);
		} else if (old_count == INT32_MAX) {
			panic("(2) usecount overflow for vnode %p", dp);
		}
		startdir_with_usecount = dp;
	}

	/* Now that we have our usecount, release the locks */
	lck_rw_unlock_shared(&rootvnode_rw_lock);
	proc_dirs_unlock_shared(p);

	ndp->ni_dvp = NULLVP;
	ndp->ni_vp  = NULLVP;

	for (;;) {
#if CONFIG_MACF
		/*
		 * Give MACF policies a chance to reject the lookup
		 * before performing any filesystem operations.
		 * This hook is called before resolving the path and
		 * again each time a symlink is encountered.
		 * NB: policies receive path information as supplied
		 *     by the caller and thus cannot be trusted.
		 */
		error = mac_vnode_check_lookup_preflight(ctx, dp, cnp->cn_nameptr, cnp->cn_namelen);
		if (error) {
			goto error_out;
		}
#endif
		ndp->ni_startdir = dp;
		dp = NULLVP;

		if ((error = lookup(ndp))) {
			goto error_out;
		}

		/*
		 * Check for symbolic link
		 */
		if ((cnp->cn_flags & ISSYMLINK) == 0) {
			if (startdir_with_usecount) {
				vnode_rele(startdir_with_usecount);
				startdir_with_usecount = NULLVP;
			}
			if (rootdir_with_usecount) {
				lck_rw_lock_shared(&rootvnode_rw_lock);
				if (rootdir_with_usecount == rootvnode) {
					old_count = os_atomic_dec_orig(&rootdir_with_usecount->v_usecount, relaxed);
					if (old_count < 2) {
						/*
						 * There needs to have been at least 1 usecount left on the rootvnode
						 */
						panic("(3) Unexpected pre-decrement value (%d) of usecount for rootvnode %p",
						    old_count, rootdir_with_usecount);
					}
					rootdir_with_usecount = NULLVP;
				}
				lck_rw_unlock_shared(&rootvnode_rw_lock);
				if (rootdir_with_usecount) {
					vnode_rele(rootdir_with_usecount);
					rootdir_with_usecount = NULLVP;
				}
			}

			return 0;
		}

continue_symlink:
		/* Gives us a new path to process, and a starting dir */
		error = lookup_handle_symlink(ndp, &dp, &dp_has_iocount, ctx);
		if (error != 0) {
			break;
		}
		if (dp_has_iocount) {
			if ((dp != rootdir_with_usecount) && (dp != startdir_with_usecount) &&
			    (dp != usedvp_dp)) {
				if (startdir_with_usecount) {
					vnode_rele(startdir_with_usecount);
				}
				vnode_ref_ext(dp, 0, VNODE_REF_FORCE);
				startdir_with_usecount = dp;
			}
			vnode_put(dp);
			dp_has_iocount = false;
		}
	}
	/*
	 * only come here if we fail to handle a SYMLINK...
	 * if either ni_dvp or ni_vp is non-NULL, then
	 * we need to drop the iocount that was picked
	 * up in the lookup routine
	 */
out_drop:
	if (ndp->ni_dvp) {
		vnode_put(ndp->ni_dvp);
	}
	if (ndp->ni_vp) {
		vnode_put(ndp->ni_vp);
	}
error_out:
	if (startdir_with_usecount) {
		vnode_rele(startdir_with_usecount);
		startdir_with_usecount = NULLVP;
	}
	if (rootdir_with_usecount) {
		lck_rw_lock_shared(&rootvnode_rw_lock);
		if (rootdir_with_usecount == rootvnode) {
			old_count = os_atomic_dec_orig(&rootdir_with_usecount->v_usecount, relaxed);
			if (old_count < 2) {
				/*
				 * There needs to have been at least 1 usecount left on the rootvnode
				 */
				panic("(4) Unexpected pre-decrement value (%d) of usecount for rootvnode %p",
				    old_count, rootdir_with_usecount);
			}
			lck_rw_unlock_shared(&rootvnode_rw_lock);
		} else {
			lck_rw_unlock_shared(&rootvnode_rw_lock);
			vnode_rele(rootdir_with_usecount);
		}
		rootdir_with_usecount = NULLVP;
	}

	if ((cnp->cn_flags & HASBUF)) {
		cnp->cn_flags &= ~HASBUF;
		zfree(ZV_NAMEI, cnp->cn_pnbuf);
	}
	cnp->cn_pnbuf = NULL;
	ndp->ni_vp = NULLVP;
	ndp->ni_dvp = NULLVP;

#if CONFIG_VOLFS
	/*
	 * Deal with volfs fallout.
	 *
	 * At this point, if we were originally given a volfs path that
	 * looks like /.vol/123/456, then we would have had to convert it into
	 * a full path.  Assuming that part worked properly, we will now attempt
	 * to conduct a lookup of the item in the namespace.  Under normal
	 * circumstances, if a user looked up /tmp/foo and it was not there, it
	 * would be permissible to return ENOENT.
	 *
	 * However, we may not want to do that here.  Specifically, the volfs path
	 * uniquely identifies a certain item in the namespace regardless of where it
	 * lives.  If the item has moved in between the time we constructed the
	 * path and now, when we're trying to do a lookup/authorization on the full
	 * path, we may have gotten an ENOENT.
	 *
	 * At this point we can no longer tell if the path no longer exists
	 * or if the item in question no longer exists. It could have been renamed
	 * away, in which case the /.vol identifier is still valid.
	 *
	 * Do this dance a maximum of MAX_VOLFS_RESTARTS times.
	 */
	if ((error == ENOENT) && (ndp->ni_cnd.cn_flags & CN_VOLFSPATH)) {
		if (volfs_restarts < MAX_VOLFS_RESTARTS) {
			volfs_restarts++;
			goto vnode_recycled;
		}
	}
#endif

	if (error == ERECYCLE) {
		/* vnode was recycled underneath us. re-drive lookup to start at
		 *  the beginning again, since recycling invalidated last lookup*/
		ndp->ni_cnd.cn_flags = cnpflags;
		ndp->ni_dvp = usedvp;
		goto vnode_recycled;
	}


	return error;
}

int
namei_compound_available(vnode_t dp, struct nameidata *ndp)
{
	if ((ndp->ni_flag & NAMEI_COMPOUNDOPEN) != 0) {
		return vnode_compound_open_available(dp);
	}

	return 0;
}

static int
lookup_check_for_resolve_prefix(char *path, size_t pathbuflen, size_t len, uint32_t *resolve_flags, size_t *prefix_len)
{
	int error = 0;
	*resolve_flags = (uint32_t)RESOLVE_CHECKED;
	*prefix_len = 0;

	if (len < (sizeof("/.nofollow/") - 1) || path[0] != '/' || path[1] != '.') {
		return 0;
	}

	if ((strncmp(&path[2], "nofollow/", (sizeof("nofollow/") - 1)) == 0)) {
		*resolve_flags |= RESOLVE_NOFOLLOW_ANY;
		*prefix_len = sizeof("/.nofollow") - 1;
	} else if ((len >= sizeof("/.resolve/1/") - 1) &&
	    strncmp(&path[2], "resolve/", (sizeof("resolve/") - 1)) == 0) {
		char * flag = path + (sizeof("/.resolve/") - 1);
		char *next = flag;
		char last_char = path[pathbuflen - 1];

		/* no leading zeroes or non digits */
		if ((flag[0] == '0' && flag[1] != '/') ||
		    flag[0] < '0' || flag[0] > '9') {
			error = EINVAL;
			goto out;
		}

		path[pathbuflen - 1] = '\0';
		unsigned long flag_val = strtoul(flag, &next, 10);
		path[pathbuflen - 1] = last_char;
		if (next[0] != '/' || (flag_val & ~(RESOLVE_NOFOLLOW_ANY))) {
			error = EINVAL;
			goto out;
		}
		assert(next >= flag);
		*resolve_flags |= (uint32_t)flag_val;
		*prefix_len = (size_t)(next - path);
	}
out:
	assert(*prefix_len <= sizeof("/.resolve/2147483647"));
	return error;
}

static int
lookup_authorize_search(vnode_t dp, struct componentname *cnp, int dp_authorized_in_cache, vfs_context_t ctx)
{
#if !CONFIG_MACF
#pragma unused(cnp)
#endif

	int error;

	if (!dp_authorized_in_cache) {
		error = vnode_authorize(dp, NULL, KAUTH_VNODE_SEARCH, ctx);
		if (error) {
			return error;
		}
	}
#if CONFIG_MACF
	error = mac_vnode_check_lookup(ctx, dp, cnp);
	if (error) {
		return error;
	}
#endif /* CONFIG_MACF */

	return 0;
}

static void
lookup_consider_update_cache(vnode_t dvp, vnode_t vp, struct componentname *cnp, int nc_generation)
{
	int isdot_or_dotdot;
	isdot_or_dotdot = (cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.') || (cnp->cn_flags & ISDOTDOT);

	if (vp->v_name == NULL || vp->v_parent == NULLVP) {
		int  update_flags = 0;

		if (isdot_or_dotdot == 0) {
			if (vp->v_name == NULL) {
				update_flags |= VNODE_UPDATE_NAME;
			}
			if (dvp != NULLVP && vp->v_parent == NULLVP) {
				update_flags |= VNODE_UPDATE_PARENT;
			}

			if (update_flags) {
				vnode_update_identity(vp, dvp, cnp->cn_nameptr, cnp->cn_namelen, cnp->cn_hash, update_flags);
			}
		}
	}
	if ((cnp->cn_flags & MAKEENTRY) && (vp->v_flag & VNCACHEABLE) && LIST_FIRST(&vp->v_nclinks) == NULL) {
		/*
		 * missing from name cache, but should
		 * be in it... this can happen if volfs
		 * causes the vnode to be created or the
		 * name cache entry got recycled but the
		 * vnode didn't...
		 * check to make sure that ni_dvp is valid
		 * cache_lookup_path may return a NULL
		 * do a quick check to see if the generation of the
		 * directory matches our snapshot... this will get
		 * rechecked behind the name cache lock, but if it
		 * already fails to match, no need to go any further
		 */
		if (dvp != NULLVP && (nc_generation == dvp->v_nc_generation) && (!isdot_or_dotdot)) {
			cache_enter_with_gen(dvp, vp, cnp, nc_generation);
		}
	}
}

#if NAMEDRSRCFORK
/*
 * Can change ni_dvp and ni_vp.  On success, returns with iocounts on stream vnode (always) and
 * data fork if requested.  On failure, returns with iocount data fork (always) and its parent directory
 * (if one was provided).
 */
static int
lookup_handle_rsrc_fork(vnode_t dp, struct nameidata *ndp, struct componentname *cnp, int wantparent, vfs_context_t ctx)
{
	vnode_t svp = NULLVP;
	enum nsoperation nsop;
	int nsflags;
	int error;

	if (dp->v_type != VREG) {
		error = ENOENT;
		goto out;
	}
	switch (cnp->cn_nameiop) {
	case DELETE:
		if (cnp->cn_flags & CN_ALLOWRSRCFORK) {
			nsop = NS_DELETE;
		} else {
			error = EPERM;
			goto out;
		}
		break;
	case CREATE:
		if (cnp->cn_flags & CN_ALLOWRSRCFORK) {
			nsop = NS_CREATE;
		} else {
			error = EPERM;
			goto out;
		}
		break;
	case LOOKUP:
		/* Make sure our lookup of "/..namedfork/rsrc" is allowed. */
		if (cnp->cn_flags & CN_ALLOWRSRCFORK) {
			nsop = NS_OPEN;
		} else {
			error = EPERM;
			goto out;
		}
		break;
	default:
		error = EPERM;
		goto out;
	}

	nsflags = 0;
	if (cnp->cn_flags & CN_RAW_ENCRYPTED) {
		nsflags |= NS_GETRAWENCRYPTED;
	}

	/* Ask the file system for the resource fork. */
	error = vnode_getnamedstream(dp, &svp, XATTR_RESOURCEFORK_NAME, nsop, nsflags, ctx);

	/* During a create, it OK for stream vnode to be missing. */
	if (error == ENOATTR || error == ENOENT) {
		error = (nsop == NS_CREATE) ? 0 : ENOENT;
	}
	if (error) {
		goto out;
	}
	/* The "parent" of the stream is the file. */
	if (wantparent) {
		if (ndp->ni_dvp) {
			vnode_put(ndp->ni_dvp);
		}
		ndp->ni_dvp = dp;
	} else {
		vnode_put(dp);
	}
	ndp->ni_vp = svp;  /* on create this may be null */

	/* Restore the truncated pathname buffer (for audits). */
	if (ndp->ni_pathlen == 1 && ndp->ni_next[0] == '\0') {
		/*
		 * While we replaced only '/' with '\0' and would ordinarily
		 * need to just switch that back, the buffer in which we did
		 * this may not be what the pathname buffer is now when symlinks
		 * are involved. If we just restore the "/" we will make the
		 * string not terminated anymore, so be safe and restore the
		 * entire suffix.
		 */
		strncpy(ndp->ni_next, _PATH_RSRCFORKSPEC, sizeof(_PATH_RSRCFORKSPEC));
		cnp->cn_nameptr = ndp->ni_next + 1;
		cnp->cn_namelen = sizeof(_PATH_RSRCFORKSPEC) - 1;
		ndp->ni_next += cnp->cn_namelen;
		if (ndp->ni_next[0] != '\0') {
			panic("Incorrect termination of path in %s", __FUNCTION__);
		}
	}
	cnp->cn_flags  &= ~MAKEENTRY;

	return 0;
out:
	return error;
}
#endif /* NAMEDRSRCFORK */

/*
 * iocounts in:
 *      --One on ni_vp.  One on ni_dvp if there is more path, or we didn't come through the
 *      cache, or we came through the cache and the caller doesn't want the parent.
 *
 * iocounts out:
 *	--Leaves us in the correct state for the next step, whatever that might be.
 *	--If we find a symlink, returns with iocounts on both ni_vp and ni_dvp.
 *	--If we are to look up another component, then we have an iocount on ni_vp and
 *	nothing else.
 *	--If we are done, returns an iocount on ni_vp, and possibly on ni_dvp depending on nameidata flags.
 *	--In the event of an error, may return with ni_dvp NULL'ed out (in which case, iocount
 *	was dropped).
 */
static int
lookup_handle_found_vnode(struct nameidata *ndp, struct componentname *cnp, int rdonly,
    int vbusyflags, int *keep_going, int nc_generation,
    int wantparent, int atroot, vfs_context_t ctx)
{
	vnode_t dp;
	int error;
	char *cp;

	dp = ndp->ni_vp;
	*keep_going = 0;

	if (ndp->ni_vp == NULLVP) {
		panic("NULL ni_vp in %s", __FUNCTION__);
	}

	if (atroot) {
		goto nextname;
	}

	/*
	 * Take into account any additional components consumed by
	 * the underlying filesystem.
	 */
	if (cnp->cn_consume > 0) {
		cnp->cn_nameptr += cnp->cn_consume;
		ndp->ni_next += cnp->cn_consume;
		ndp->ni_pathlen -= cnp->cn_consume;
		cnp->cn_consume = 0;
	} else {
		lookup_consider_update_cache(ndp->ni_dvp, dp, cnp, nc_generation);
	}

	/*
	 * Check to see if the vnode has been mounted on...
	 * if so find the root of the mounted file system.
	 * Updates ndp->ni_vp.
	 */
	error = lookup_traverse_mountpoints(ndp, cnp, dp, vbusyflags, ctx);
	dp = ndp->ni_vp;
	if (error) {
		goto out;
	}

#if CONFIG_MACF
	if (vfs_flags(vnode_mount(dp)) & MNT_MULTILABEL) {
		error = vnode_label(vnode_mount(dp), NULL, dp, NULL, 0, ctx);
		if (error) {
			goto out;
		}
	}
#endif

	/*
	 * Check for symbolic link
	 */
	if ((dp->v_type == VLNK) &&
	    ((cnp->cn_flags & FOLLOW) || (ndp->ni_flag & NAMEI_TRAILINGSLASH) || *ndp->ni_next == '/')) {
		cnp->cn_flags |= ISSYMLINK;
		*keep_going = 1;
		return 0;
	}

	/*
	 * Check for bogus trailing slashes.
	 */
	if ((ndp->ni_flag & NAMEI_TRAILINGSLASH)) {
		if (dp->v_type != VDIR) {
			error = ENOTDIR;
			goto out;
		}
		ndp->ni_flag &= ~(NAMEI_TRAILINGSLASH);
	}

#if NAMEDSTREAMS
	/*
	 * Deny namei/lookup requests to resolve paths that point to shadow files.
	 * Access to shadow files must be conducted by explicit calls to VNOP_LOOKUP
	 * directly, and not use lookup/namei
	 */
	if (vnode_isshadow(dp)) {
		error = ENOENT;
		goto out;
	}
#endif

nextname:
	/*
	 * Not a symbolic link.  If more pathname,
	 * continue at next component, else return.
	 *
	 * Definitely have a dvp if there's another slash
	 */
	if (*ndp->ni_next == '/') {
		cnp->cn_nameptr = ndp->ni_next + 1;
		ndp->ni_pathlen--;
		while (*cnp->cn_nameptr == '/') {
			cnp->cn_nameptr++;
			ndp->ni_pathlen--;
		}

		cp = cnp->cn_nameptr;
		vnode_put(ndp->ni_dvp);
		ndp->ni_dvp = NULLVP;

		if (*cp == '\0') {
			goto emptyname;
		}

		*keep_going = 1;
		return 0;
	}

	/*
	 * Disallow directory write attempts on read-only file systems.
	 */
	if (rdonly &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME)) {
		error = EROFS;
		goto out;
	}

	/* If SAVESTART is set, we should have a dvp */
	if (cnp->cn_flags & SAVESTART) {
		/*
		 * note that we already hold a reference
		 * on both dp and ni_dvp, but for some reason
		 * can't get another one... in this case we
		 * need to do vnode_put on dp in 'bad2'
		 */
		if ((vnode_get(ndp->ni_dvp))) {
			error = ENOENT;
			goto out;
		}
		ndp->ni_startdir = ndp->ni_dvp;
	}
	if (!wantparent && ndp->ni_dvp) {
		vnode_put(ndp->ni_dvp);
		ndp->ni_dvp = NULLVP;
	}

	if (cnp->cn_flags & AUDITVNPATH1) {
		AUDIT_ARG(vnpath, dp, ARG_VNODE1);
	} else if (cnp->cn_flags & AUDITVNPATH2) {
		AUDIT_ARG(vnpath, dp, ARG_VNODE2);
	}

#if NAMEDRSRCFORK
	/*
	 * Caller wants the resource fork.
	 */
	if ((cnp->cn_flags & CN_WANTSRSRCFORK) && (dp != NULLVP)) {
		error = lookup_handle_rsrc_fork(dp, ndp, cnp, wantparent, ctx);
		if (error != 0) {
			goto out;
		}

		dp = ndp->ni_vp;
	}
#endif
	if (kdebug_enable) {
		kdebug_lookup(ndp->ni_vp, cnp);
	}

	return 0;

emptyname:
	error = lookup_handle_emptyname(ndp, cnp, wantparent);
	if (error != 0) {
		goto out;
	}

	return 0;
out:
	return error;
}

/*
 * Comes in iocount on ni_vp.  May overwrite ni_dvp, but doesn't interpret incoming value.
 */
static int
lookup_handle_emptyname(struct nameidata *ndp, struct componentname *cnp, int wantparent)
{
	vnode_t dp;
	int error = 0;

	dp = ndp->ni_vp;
	cnp->cn_namelen = 0;
	/*
	 * A degenerate name (e.g. / or "") which is a way of
	 * talking about a directory, e.g. like "/." or ".".
	 */
	if (dp->v_type != VDIR) {
		error = ENOTDIR;
		goto out;
	}
	if (cnp->cn_nameiop == CREATE && dp == rootvnode) {
		error = EEXIST;
		goto out;
	}
	if (cnp->cn_nameiop != LOOKUP) {
		error = EISDIR;
		goto out;
	}
	if (wantparent) {
		/*
		 * note that we already hold a reference
		 * on dp, but for some reason can't
		 * get another one... in this case we
		 * need to do vnode_put on dp in 'bad'
		 */
		if ((vnode_get(dp))) {
			error = ENOENT;
			goto out;
		}
		ndp->ni_dvp = dp;
	}
	cnp->cn_flags &= ~ISDOTDOT;
	cnp->cn_flags |= ISLASTCN;
	ndp->ni_next = cnp->cn_nameptr;
	ndp->ni_vp = dp;

	if (cnp->cn_flags & AUDITVNPATH1) {
		AUDIT_ARG(vnpath, dp, ARG_VNODE1);
	} else if (cnp->cn_flags & AUDITVNPATH2) {
		AUDIT_ARG(vnpath, dp, ARG_VNODE2);
	}
	if (cnp->cn_flags & SAVESTART) {
		panic("lookup: SAVESTART");
	}

	return 0;
out:
	return error;
}
/*
 * Search a pathname.
 * This is a very central and rather complicated routine.
 *
 * The pathname is pointed to by ni_ptr and is of length ni_pathlen.
 * The starting directory is taken from ni_startdir. The pathname is
 * descended until done, or a symbolic link is encountered. The variable
 * ni_more is clear if the path is completed; it is set to one if a
 * symbolic link needing interpretation is encountered.
 *
 * The flag argument is LOOKUP, CREATE, RENAME, or DELETE depending on
 * whether the name is to be looked up, created, renamed, or deleted.
 * When CREATE, RENAME, or DELETE is specified, information usable in
 * creating, renaming, or deleting a directory entry may be calculated.
 * If flag has LOCKPARENT or'ed into it, the parent directory is returned
 * locked. If flag has WANTPARENT or'ed into it, the parent directory is
 * returned unlocked. Otherwise the parent directory is not returned. If
 * the target of the pathname exists and LOCKLEAF is or'ed into the flag
 * the target is returned locked, otherwise it is returned unlocked.
 * When creating or renaming and LOCKPARENT is specified, the target may not
 * be ".".  When deleting and LOCKPARENT is specified, the target may be ".".
 *
 * Overall outline of lookup:
 *
 * dirloop:
 *	identify next component of name at ndp->ni_ptr
 *	handle degenerate case where name is null string
 *	if .. and crossing mount points and on mounted filesys, find parent
 *	call VNOP_LOOKUP routine for next component name
 *	    directory vnode returned in ni_dvp, unlocked unless LOCKPARENT set
 *	    component vnode returned in ni_vp (if it exists), locked.
 *	if result vnode is mounted on and crossing mount points,
 *	    find mounted on vnode
 *	if more components of name, do next level at dirloop
 *	return the answer in ni_vp, locked if LOCKLEAF set
 *	    if LOCKPARENT set, return locked parent in ni_dvp
 *	    if WANTPARENT set, return unlocked parent in ni_dvp
 *
 * Returns:	0			Success
 *		ENOENT			No such file or directory
 *		EBADF			Bad file descriptor
 *		ENOTDIR			Not a directory
 *		EROFS			Read-only file system [CREATE]
 *		EISDIR			Is a directory [CREATE]
 *		cache_lookup_path:ERECYCLE  (vnode was recycled from underneath us, redrive lookup again)
 *		vnode_authorize:EROFS
 *		vnode_authorize:EACCES
 *		vnode_authorize:EPERM
 *		vnode_authorize:???
 *		VNOP_LOOKUP:ENOENT	No such file or directory
 *		VNOP_LOOKUP:EJUSTRETURN	Restart system call (INTERNAL)
 *		VNOP_LOOKUP:???
 *		VFS_ROOT:ENOTSUP
 *		VFS_ROOT:ENOENT
 *		VFS_ROOT:???
 */
int
lookup(struct nameidata *ndp)
{
	char    *cp;            /* pointer into pathname argument */
	vnode_t         tdp;            /* saved dp */
	vnode_t         dp;             /* the directory we are searching */
	int docache = 1;                /* == 0 do not cache last component */
	int wantparent;                 /* 1 => wantparent or lockparent flag */
	int rdonly;                     /* lookup read-only flag bit */
	int dp_authorized = 0;
	int error = 0;
	struct componentname *cnp = &ndp->ni_cnd;
	vfs_context_t ctx = cnp->cn_context;
	int vbusyflags = 0;
	int nc_generation = 0;
	vnode_t last_dp = NULLVP;
	int keep_going;
	int atroot;

	/*
	 * Setup: break out flag bits into variables.
	 */
	if (cnp->cn_flags & NOCACHE) {
		docache = 0;
	}
	wantparent = cnp->cn_flags & (LOCKPARENT | WANTPARENT);
	rdonly = cnp->cn_flags & RDONLY;
	cnp->cn_flags &= ~ISSYMLINK;
	cnp->cn_consume = 0;

	dp = ndp->ni_startdir;
	ndp->ni_startdir = NULLVP;

	if ((cnp->cn_flags & CN_NBMOUNTLOOK) != 0) {
		vbusyflags = LK_NOWAIT;
	}
	cp = cnp->cn_nameptr;

	if (*cp == '\0') {
		if ((vnode_getwithref(dp))) {
			dp = NULLVP;
			error = ENOENT;
			goto bad;
		}
		ndp->ni_vp = dp;
		error = lookup_handle_emptyname(ndp, cnp, wantparent);
		if (error) {
			goto bad;
		}

		return 0;
	}
dirloop:
	atroot = 0;
	ndp->ni_vp = NULLVP;

	if ((error = cache_lookup_path(ndp, cnp, dp, ctx, &dp_authorized, last_dp))) {
		dp = NULLVP;
		goto bad;
	}
	if ((cnp->cn_flags & ISLASTCN)) {
		if (docache) {
			cnp->cn_flags |= MAKEENTRY;
		}
	} else {
		cnp->cn_flags |= MAKEENTRY;
	}

	dp = ndp->ni_dvp;

	if (ndp->ni_vp != NULLVP) {
		/*
		 * cache_lookup_path returned a non-NULL ni_vp then,
		 * we're guaranteed that the dp is a VDIR, it's
		 * been authorized, and vp is not ".."
		 *
		 * make sure we don't try to enter the name back into
		 * the cache if this vp is purged before we get to that
		 * check since we won't have serialized behind whatever
		 * activity is occurring in the FS that caused the purge
		 */
		if (dp != NULLVP) {
			nc_generation = dp->v_nc_generation - 1;
		}

		goto returned_from_lookup_path;
	}

	/*
	 * Handle "..": two special cases.
	 * 1. If at root directory (e.g. after chroot)
	 *    or at absolute root directory
	 *    then ignore it so can't get out.
	 * 2. If this vnode is the root of a mounted
	 *    filesystem, then replace it with the
	 *    vnode which was mounted on so we take the
	 *    .. in the other file system.
	 */
	if ((cnp->cn_flags & ISDOTDOT)) {
		/*
		 * if this is a chroot'ed process, check if the current
		 * directory is still a subdirectory of the process's
		 * root directory.
		 */
		if (ndp->ni_rootdir && (ndp->ni_rootdir != rootvnode) &&
		    dp != ndp->ni_rootdir) {
			int sdir_error;
			int is_subdir = FALSE;

			sdir_error = vnode_issubdir(dp, ndp->ni_rootdir,
			    &is_subdir, vfs_context_kernel());

			/*
			 * If we couldn't determine if dp is a subdirectory of
			 * ndp->ni_rootdir (sdir_error != 0), we let the request
			 * proceed.
			 */
			if (!sdir_error && !is_subdir) {
				vnode_put(dp);
				dp = ndp->ni_rootdir;
				/*
				 * There's a ref on the process's root directory
				 * but we can't use vnode_getwithref here as
				 * there is nothing preventing that ref being
				 * released by another thread.
				 */
				if (vnode_get(dp)) {
					error = ENOENT;
					goto bad;
				}
			}
		}

		for (;;) {
			if (dp == ndp->ni_rootdir || dp == rootvnode) {
				ndp->ni_dvp = dp;
				ndp->ni_vp = dp;
				/*
				 * we're pinned at the root
				 * we've already got one reference on 'dp'
				 * courtesy of cache_lookup_path... take
				 * another one for the ".."
				 * if we fail to get the new reference, we'll
				 * drop our original down in 'bad'
				 */
				if ((vnode_get(dp))) {
					error = ENOENT;
					goto bad;
				}
				atroot = 1;
				goto returned_from_lookup_path;
			}
			if ((dp->v_flag & VROOT) == 0 ||
			    (cnp->cn_flags & NOCROSSMOUNT)) {
				break;
			}
			if (dp->v_mount == NULL) {      /* forced umount */
				error = EBADF;
				goto bad;
			}
			tdp = dp;
			dp = tdp->v_mount->mnt_vnodecovered;

			if ((vnode_getwithref(dp))) {
				vnode_put(tdp);
				dp = NULLVP;
				error = ENOENT;
				goto bad;
			}

			vnode_put(tdp);

			ndp->ni_dvp = dp;
			dp_authorized = 0;
		}
	}

	/*
	 * We now have a segment name to search for, and a directory to search.
	 */
#if CONFIG_UNION_MOUNTS
unionlookup:
#endif /* CONFIG_UNION_MOUNTS */
	ndp->ni_vp = NULLVP;

	if (dp->v_type != VDIR) {
		error = ENOTDIR;
		goto lookup_error;
	}
	if ((cnp->cn_flags & DONOTAUTH) != DONOTAUTH) {
		error = lookup_authorize_search(dp, cnp, dp_authorized, ctx);
		if (error) {
			goto lookup_error;
		}
	}

	/*
	 * Now that we've authorized a lookup, can bail out if the filesystem
	 * will be doing a batched operation.  Return an iocount on dvp.
	 */
#if NAMEDRSRCFORK
	if ((cnp->cn_flags & ISLASTCN) && namei_compound_available(dp, ndp) && !(cnp->cn_flags & CN_WANTSRSRCFORK)) {
#else
	if ((cnp->cn_flags & ISLASTCN) && namei_compound_available(dp, ndp)) {
#endif /* NAMEDRSRCFORK */
		ndp->ni_flag |= NAMEI_UNFINISHED;
		ndp->ni_ncgeneration = dp->v_nc_generation;
		return 0;
	}

	nc_generation = dp->v_nc_generation;

	/*
	 * Note:
	 * Filesystems that support hardlinks may want to call vnode_update_identity
	 * if the lookup operation below will modify the in-core vnode to belong to a new point
	 * in the namespace.  VFS cannot infer whether or not the look up operation makes the vnode
	 * name change or change parents.  Without this, the lookup may make update
	 * filesystem-specific in-core metadata but fail to update the v_parent or v_name
	 * fields in the vnode.  If VFS were to do this, it would be necessary to call
	 * vnode_update_identity on every lookup operation -- expensive!
	 *
	 * However, even with this in place, multiple lookups may occur in between this lookup
	 * and the subsequent vnop, so, at best, we could only guarantee that you would get a
	 * valid path back, and not necessarily the one that you wanted.
	 *
	 * Example:
	 * /tmp/a == /foo/b
	 *
	 * If you are now looking up /foo/b and the vnode for this link represents /tmp/a,
	 * vnode_update_identity will fix the parentage so that you can get /foo/b back
	 * through the v_parent chain (preventing you from getting /tmp/b back). It would
	 * not fix whether or not you should or should not get /tmp/a vs. /foo/b.
	 */

	error = VNOP_LOOKUP(dp, &ndp->ni_vp, cnp, ctx);

	if (error) {
lookup_error:
#if CONFIG_UNION_MOUNTS
		if ((error == ENOENT) &&
		    (dp->v_mount != NULL) &&
		    (dp->v_mount->mnt_flag & MNT_UNION)) {
			tdp = dp;
			error = lookup_traverse_union(tdp, &dp, ctx);
			vnode_put(tdp);
			if (error) {
				dp = NULLVP;
				goto bad;
			}

			ndp->ni_dvp = dp;
			dp_authorized = 0;
			goto unionlookup;
		}
#endif /* CONFIG_UNION_MOUNTS */

		if (error != EJUSTRETURN) {
			goto bad;
		}

		if (ndp->ni_vp != NULLVP) {
			panic("leaf should be empty");
		}

#if NAMEDRSRCFORK
		/*
		 * At this point, error should be EJUSTRETURN.
		 *
		 * If CN_WANTSRSRCFORK is set, that implies that the
		 * underlying filesystem could not find the "parent" of the
		 * resource fork (the data fork), and we are doing a lookup
		 * for a CREATE event.
		 *
		 * However, this should be converted to an error, as the
		 * failure to find this parent should disallow further
		 * progress to try and acquire a resource fork vnode.
		 */
		if (cnp->cn_flags & CN_WANTSRSRCFORK) {
			error = ENOENT;
			goto bad;
		}
#endif

		error = lookup_validate_creation_path(ndp);
		if (error) {
			goto bad;
		}
		/*
		 * We return with ni_vp NULL to indicate that the entry
		 * doesn't currently exist, leaving a pointer to the
		 * referenced directory vnode in ndp->ni_dvp.
		 */
		if (cnp->cn_flags & SAVESTART) {
			if ((vnode_get(ndp->ni_dvp))) {
				error = ENOENT;
				goto bad;
			}
			ndp->ni_startdir = ndp->ni_dvp;
		}
		if (!wantparent) {
			vnode_put(ndp->ni_dvp);
		}

		if (kdebug_enable) {
			kdebug_lookup(ndp->ni_dvp, cnp);
		}
		return 0;
	}
returned_from_lookup_path:
	/* We'll always have an iocount on ni_vp when this finishes. */
	error = lookup_handle_found_vnode(ndp, cnp, rdonly, vbusyflags, &keep_going, nc_generation, wantparent, atroot, ctx);
	if (error != 0) {
		goto bad2;
	}

	if (keep_going) {
		dp = ndp->ni_vp;

		/* namei() will handle symlinks */
		if ((dp->v_type == VLNK) &&
		    ((cnp->cn_flags & FOLLOW) || (ndp->ni_flag & NAMEI_TRAILINGSLASH) || *ndp->ni_next == '/')) {
			return 0;
		}

		/*
		 * Otherwise, there's more path to process.
		 * cache_lookup_path is now responsible for dropping io ref on dp
		 * when it is called again in the dirloop.  This ensures we hold
		 * a ref on dp until we complete the next round of lookup.
		 */
		last_dp = dp;

		goto dirloop;
	}

	return 0;
bad2:
	if (ndp->ni_dvp) {
		vnode_put(ndp->ni_dvp);
	}

	vnode_put(ndp->ni_vp);
	ndp->ni_vp = NULLVP;

	if (kdebug_enable) {
		kdebug_lookup(dp, cnp);
	}
	return error;

bad:
	if (dp) {
		vnode_put(dp);
	}
	ndp->ni_vp = NULLVP;

	if (kdebug_enable) {
		kdebug_lookup(dp, cnp);
	}
	return error;
}

#if CONFIG_UNION_MOUNTS
/*
 * Given a vnode in a union mount, traverse to the equivalent
 * vnode in the underlying mount.
 */
int
lookup_traverse_union(vnode_t dvp, vnode_t *new_dvp, vfs_context_t ctx)
{
	char *path = NULL, *pp;
	const char *name, *np;
	size_t len;
	int error = 0;
	struct nameidata nd;
	vnode_t vp = dvp;

	*new_dvp = NULL;

	if (vp && vp->v_flag & VROOT) {
		*new_dvp = vp->v_mount->mnt_vnodecovered;
		if (vnode_getwithref(*new_dvp)) {
			return ENOENT;
		}
		return 0;
	}

	path = zalloc_flags(ZV_NAMEI, Z_WAITOK | Z_NOFAIL);

	/*
	 * Walk back up to the mountpoint following the
	 * v_parent chain and build a slash-separated path.
	 * Then lookup that path starting with the covered vnode.
	 */
	pp = path + (MAXPATHLEN - 1);
	*pp = '\0';

	while (1) {
		name = vnode_getname(vp);
		if (name == NULL) {
			printf("lookup_traverse_union: null parent name: .%s\n", pp);
			error = ENOENT;
			goto done;
		}
		len = strlen(name);
		if ((len + 1) > (size_t)(pp - path)) {          // Enough space for this name ?
			error = ENAMETOOLONG;
			vnode_putname(name);
			goto done;
		}
		for (np = name + len; len > 0; len--) { // Copy name backwards
			*--pp = *--np;
		}
		vnode_putname(name);
		vp = vp->v_parent;
		if (vp == NULLVP || vp->v_flag & VROOT) {
			break;
		}
		*--pp = '/';
	}

	/* Evaluate the path in the underlying mount */
	NDINIT(&nd, LOOKUP, OP_LOOKUP, USEDVP, UIO_SYSSPACE, CAST_USER_ADDR_T(pp), ctx);
	nd.ni_dvp = dvp->v_mount->mnt_vnodecovered;
	error = namei(&nd);
	if (error == 0) {
		*new_dvp = nd.ni_vp;
	}
	nameidone(&nd);
done:
	if (path) {
		zfree(ZV_NAMEI, path);
	}
	return error;
}
#endif /* CONFIG_UNION_MOUNTS */

int
lookup_validate_creation_path(struct nameidata *ndp)
{
	struct componentname *cnp = &ndp->ni_cnd;

	/*
	 * If creating and at end of pathname, then can consider
	 * allowing file to be created.
	 */
	if (cnp->cn_flags & RDONLY) {
		return EROFS;
	}
	if ((cnp->cn_flags & ISLASTCN) && (ndp->ni_flag & NAMEI_TRAILINGSLASH) && !(cnp->cn_flags & WILLBEDIR)) {
		return ENOENT;
	}

	return 0;
}

/*
 * Modifies only ni_vp.  Always returns with ni_vp still valid (iocount held).
 */
static int
lookup_traverse_mountpoints(struct nameidata *ndp, struct componentname *cnp, vnode_t dp,
    int vbusyflags, vfs_context_t ctx)
{
	mount_t mp;
	vnode_t tdp;
	int error = 0;
	uint32_t depth = 0;
	vnode_t mounted_on_dp;
	int current_mount_generation = 0;
#if CONFIG_TRIGGERS
	vnode_t triggered_dp = NULLVP;
	int retry_cnt = 0;
#define MAX_TRIGGER_RETRIES 1
#endif

	if (dp->v_type != VDIR || cnp->cn_flags & NOCROSSMOUNT) {
		return 0;
	}

	mounted_on_dp = dp;
#if CONFIG_TRIGGERS
restart:
#endif
	current_mount_generation = mount_generation;

	while (dp->v_mountedhere) {
		vnode_lock_spin(dp);
		if ((mp = dp->v_mountedhere)) {
			mp->mnt_crossref++;
			vnode_unlock(dp);
		} else {
			vnode_unlock(dp);
			break;
		}

		if (ISSET(mp->mnt_lflag, MNT_LFORCE)) {
			mount_dropcrossref(mp, dp, 0);
			break;  // don't traverse into a forced unmount
		}


		if (vfs_busy(mp, vbusyflags)) {
			mount_dropcrossref(mp, dp, 0);
			if (vbusyflags == LK_NOWAIT) {
				error = ENOENT;
				goto out;
			}

			continue;
		}

		error = VFS_ROOT(mp, &tdp, ctx);

		mount_dropcrossref(mp, dp, 0);
		vfs_unbusy(mp);

		if (error) {
			goto out;
		}

		vnode_put(dp);
		ndp->ni_vp = dp = tdp;
		if (dp->v_type != VDIR) {
#if DEVELOPMENT || DEBUG
			panic("%s : Root of filesystem not a directory",
			    __FUNCTION__);
#else
			break;
#endif
		}
		depth++;
	}

#if CONFIG_TRIGGERS
	/*
	 * The triggered_dp check here is required but is susceptible to a
	 * (unlikely) race in which trigger mount is done from here and is
	 * unmounted before we get past vfs_busy above. We retry to deal with
	 * that case but it has the side effect of unwanted retries for
	 * "special" processes which don't want to trigger mounts.
	 */
	if (dp->v_resolve && retry_cnt < MAX_TRIGGER_RETRIES) {
		error = vnode_trigger_resolve(dp, ndp, ctx);
		if (error) {
			goto out;
		}
		if (dp == triggered_dp) {
			retry_cnt += 1;
		} else {
			retry_cnt = 0;
		}
		triggered_dp = dp;
		goto restart;
	}
#endif /* CONFIG_TRIGGERS */

	if (depth) {
		mp = mounted_on_dp->v_mountedhere;

		if (mp) {
			mount_lock_spin(mp);
			mp->mnt_realrootvp_vid = dp->v_id;
			mp->mnt_realrootvp = dp;
			mp->mnt_generation = current_mount_generation;
			mount_unlock(mp);
		}
	}

	return 0;

out:
	return error;
}

/*
 * Takes ni_vp and ni_dvp non-NULL.  Returns with *new_dp set to the location
 * at which to start a lookup with a resolved path, and all other iocounts dropped.
 */
static int
lookup_handle_symlink(struct nameidata *ndp, vnode_t *new_dp, bool *new_dp_has_iocount, vfs_context_t ctx)
{
	int error;
	char *cp;               /* pointer into pathname argument */
	uio_t auio;
	UIO_STACKBUF(uio_buf, 1);
	int need_newpathbuf;
	u_int linklen = 0;
	struct componentname *cnp = &ndp->ni_cnd;
	vnode_t dp;
	char *tmppn;
	u_int rsrclen = (cnp->cn_flags & CN_WANTSRSRCFORK) ? sizeof(_PATH_RSRCFORKSPEC) : 0;
	bool dp_has_iocount = false;

	if (ndp->ni_loopcnt++ >= MAXSYMLINKS) {
		return ELOOP;
	}
#if CONFIG_MACF
	if ((error = mac_vnode_check_readlink(ctx, ndp->ni_vp)) != 0) {
		return error;
	}
#endif /* MAC */
	if (ndp->ni_pathlen > 1 || !(cnp->cn_flags & HASBUF)) {
		need_newpathbuf = 1;
	} else {
		need_newpathbuf = 0;
	}

	if (need_newpathbuf) {
		cp = zalloc(ZV_NAMEI);
	} else {
		cp = cnp->cn_pnbuf;
	}
	auio = uio_createwithbuffer(1, 0, UIO_SYSSPACE, UIO_READ, &uio_buf[0], sizeof(uio_buf));

	uio_addiov(auio, CAST_USER_ADDR_T(cp), MAXPATHLEN);

	error = VNOP_READLINK(ndp->ni_vp, auio, ctx);

	if (!error) {
		user_ssize_t resid = uio_resid(auio);

		assert(resid <= MAXPATHLEN);

		if (resid == MAXPATHLEN) {
			linklen = 0;
		} else {
			/*
			 * Safe to set unsigned with a [larger] signed type here
			 * because 0 <= uio_resid <= MAXPATHLEN and MAXPATHLEN
			 * is only 1024.
			 */
			linklen = (u_int)strnlen(cp, MAXPATHLEN - (u_int)resid);
		}

		if (linklen == 0) {
			error = ENOENT;
		} else if (linklen + ndp->ni_pathlen + rsrclen > MAXPATHLEN) {
			error = ENAMETOOLONG;
		}
	}

	if (error) {
		if (need_newpathbuf) {
			zfree(ZV_NAMEI, cp);
		}
		return error;
	}

	if (need_newpathbuf) {
		tmppn = cnp->cn_pnbuf;
		bcopy(ndp->ni_next, cp + linklen, ndp->ni_pathlen);
		cnp->cn_pnbuf = cp;
		cnp->cn_pnlen = MAXPATHLEN;

		if ((cnp->cn_flags & HASBUF)) {
			zfree(ZV_NAMEI, tmppn);
		} else {
			cnp->cn_flags |= HASBUF;
		}
	} else {
		cnp->cn_pnbuf[linklen] = '\0';
	}

	ndp->ni_pathlen += linklen;
	cnp->cn_nameptr = cnp->cn_pnbuf;

	/*
	 * starting point for 'relative'
	 * symbolic link path
	 */
	dp = ndp->ni_dvp;

	/*
	 * get rid of reference returned via 'lookup'
	 * ni_dvp is released only if we restart at /.
	 */
	vnode_put(ndp->ni_vp);
	ndp->ni_vp = NULLVP;
	ndp->ni_dvp = NULLVP;

	dp_has_iocount = true;

	/*
	 * Check if symbolic link restarts us at the root
	 */
	if (*(cnp->cn_nameptr) == '/') {
		while (*(cnp->cn_nameptr) == '/') {
			cnp->cn_nameptr++;
			ndp->ni_pathlen--;
		}
		if (linklen != 0) {
			vnode_put(dp); /* ALWAYS have a dvp for a symlink */
			dp_has_iocount = false;
			if ((dp = ndp->ni_rootdir) == NULLVP) {
				return ENOENT;
			}
		}
	}

	*new_dp = dp;
	*new_dp_has_iocount = dp_has_iocount;

	return 0;
}

/*
 * relookup - lookup a path name component
 *    Used by lookup to re-aquire things.
 */
int
relookup(struct vnode *dvp, struct vnode **vpp, struct componentname *cnp)
{
	struct vnode *dp = NULL;                /* the directory we are searching */
	int wantparent;                 /* 1 => wantparent or lockparent flag */
	int rdonly;                     /* lookup read-only flag bit */
	int error = 0;
#ifdef NAMEI_DIAGNOSTIC
	int i, newhash;                 /* DEBUG: check name hash */
	char *cp;                       /* DEBUG: check name ptr/len */
#endif
	vfs_context_t ctx = cnp->cn_context;

	/*
	 * Setup: break out flag bits into variables.
	 */
	wantparent = cnp->cn_flags & (LOCKPARENT | WANTPARENT);
	rdonly = cnp->cn_flags & RDONLY;
	cnp->cn_flags &= ~ISSYMLINK;

	if (cnp->cn_flags & NOCACHE) {
		cnp->cn_flags &= ~MAKEENTRY;
	} else {
		cnp->cn_flags |= MAKEENTRY;
	}

	dp = dvp;

	/*
	 * Check for degenerate name (e.g. / or "")
	 * which is a way of talking about a directory,
	 * e.g. like "/." or ".".
	 */
	if (cnp->cn_nameptr[0] == '\0') {
		if (cnp->cn_nameiop != LOOKUP || wantparent) {
			error = EISDIR;
			goto bad;
		}
		if (dp->v_type != VDIR) {
			error = ENOTDIR;
			goto bad;
		}
		if ((vnode_get(dp))) {
			error = ENOENT;
			goto bad;
		}
		*vpp = dp;

		if (cnp->cn_flags & SAVESTART) {
			panic("lookup: SAVESTART");
		}
		return 0;
	}
	/*
	 * We now have a segment name to search for, and a directory to search.
	 */
	if ((error = VNOP_LOOKUP(dp, vpp, cnp, ctx))) {
		if (error != EJUSTRETURN) {
			goto bad;
		}
#if DIAGNOSTIC
		if (*vpp != NULL) {
			panic("leaf should be empty");
		}
#endif
		/*
		 * If creating and at end of pathname, then can consider
		 * allowing file to be created.
		 */
		if (rdonly) {
			error = EROFS;
			goto bad;
		}
		/*
		 * We return with ni_vp NULL to indicate that the entry
		 * doesn't currently exist, leaving a pointer to the
		 * (possibly locked) directory inode in ndp->ni_dvp.
		 */
		return 0;
	}
	dp = *vpp;

#if DIAGNOSTIC
	/*
	 * Check for symbolic link
	 */
	if (dp->v_type == VLNK && (cnp->cn_flags & FOLLOW)) {
		panic("relookup: symlink found.");
	}
#endif

	/*
	 * Disallow directory write attempts on read-only file systems.
	 */
	if (rdonly &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME)) {
		error = EROFS;
		goto bad2;
	}
	/* ASSERT(dvp == ndp->ni_startdir) */

	return 0;

bad2:
	vnode_put(dp);
bad:
	*vpp = NULL;

	return error;
}

/*
 * Free pathname buffer
 */
void
nameidone(struct nameidata *ndp)
{
	if (ndp->ni_cnd.cn_flags & HASBUF) {
		char *tmp = ndp->ni_cnd.cn_pnbuf;

		ndp->ni_cnd.cn_pnbuf = NULL;
		ndp->ni_cnd.cn_flags &= ~HASBUF;
		zfree(ZV_NAMEI, tmp);
	}
}


/*
 * Log (part of) a pathname using kdebug, as used by fs_usage.  The path up to
 * and including the current component name are logged.  Up to NUMPARMS * 4
 * bytes of pathname will be logged.  If the path to be logged is longer than
 * that, then the last NUMPARMS * 4 bytes are logged. That is, the truncation
 * removes the leading portion of the path.
 *
 * The logging is done via multiple KDBG_RELEASE calls.  The first one is marked
 * with DBG_FUNC_START.  The last one is marked with DBG_FUNC_END (in addition
 * to DBG_FUNC_START if it is also the first).  There may be intermediate ones
 * with neither DBG_FUNC_START nor DBG_FUNC_END.
 *
 * The first event passes the vnode pointer and 24 or 32 (on K32, 12 or 24)
 * bytes of pathname.  The remaining events add 32 (on K32, 16) bytes of
 * pathname each.  The minimum number of events required to pass the path are
 * used.  Any excess padding in the final event (because not all of the 24 or 32
 * (on K32, 12 or 16) bytes are needed for the remainder of the path) is set to
 * zero bytes, or '>' if there is more path beyond the current component name
 * (usually because an intermediate component was not found).
 *
 * NOTE: If the path length is greater than NUMPARMS * 4, or is not of the form
 * 24 + N * 32 (or on K32, 12 + N * 16), there will be no padding.
 */
#if (KDEBUG_LEVEL >= KDEBUG_LEVEL_IST)

void
kdebug_vfs_lookup(unsigned long *path_words, int path_len, void *vnp,
    uint32_t flags)
{
	bool noprocfilt = flags & KDBG_VFS_LOOKUP_FLAG_NOPROCFILT;

	assert(path_len >= 0);

	int code = ((flags & KDBG_VFS_LOOKUP_FLAG_LOOKUP) ? VFS_LOOKUP :
	    VFS_LOOKUP_DONE) | DBG_FUNC_START;

	if (path_len <= (3 * (int)sizeof(long))) {
		code |= DBG_FUNC_END;
	}

	if (noprocfilt) {
		KDBG_RELEASE_NOPROCFILT(code, kdebug_vnode(vnp), path_words[0],
		    path_words[1], path_words[2]);
	} else {
		KDBG_RELEASE(code, kdebug_vnode(vnp), path_words[0], path_words[1],
		    path_words[2]);
	}

	code &= ~DBG_FUNC_START;

	for (int i = 3; i * (int)sizeof(long) < path_len; i += 4) {
		if ((i + 4) * (int)sizeof(long) >= path_len) {
			code |= DBG_FUNC_END;
		}

		if (noprocfilt) {
			KDBG_RELEASE_NOPROCFILT(code, path_words[i], path_words[i + 1],
			    path_words[i + 2], path_words[i + 3]);
		} else {
			KDBG_RELEASE(code, path_words[i], path_words[i + 1],
			    path_words[i + 2], path_words[i + 3]);
		}
	}
}

void
kdebug_lookup_gen_events(long *path_words, int path_len, void *vnp, bool lookup)
{
	assert(path_len >= 0);
	kdebug_vfs_lookup((unsigned long *)path_words, path_len, vnp,
	    lookup ? KDBG_VFS_LOOKUP_FLAG_LOOKUP : 0);
}

void
kdebug_lookup(vnode_t vnp, struct componentname *cnp)
{
	unsigned long path_words[NUMPARMS];

	/*
	 * Truncate the leading portion of the path to fit in path_words.
	 */
	char *path_end = cnp->cn_nameptr + cnp->cn_namelen;
	size_t path_len = MIN(path_end - cnp->cn_pnbuf,
	    (ssize_t)sizeof(path_words));
	assert(path_len >= 0);
	char *path_trunc = path_end - path_len;

	memcpy(path_words, path_trunc, path_len);

	/*
	 * Pad with '\0' or '>'.
	 */
	if (path_len < (ssize_t)sizeof(path_words)) {
		bool complete_str = *(cnp->cn_nameptr + cnp->cn_namelen) == '\0';
		memset((char *)path_words + path_len, complete_str ? '\0' : '>',
		    sizeof(path_words) - path_len);
	}
	kdebug_vfs_lookup(path_words, (int)path_len, vnp, KDBG_VFS_LOOKUP_FLAG_LOOKUP);
}

#else /* (KDEBUG_LEVEL >= KDEBUG_LEVEL_IST) */

void
kdebug_vfs_lookup(long *dbg_parms __unused, int dbg_namelen __unused,
    void *dp __unused, __unused uint32_t flags)
{
}

static void
kdebug_lookup(struct vnode *dp __unused, struct componentname *cnp __unused)
{
}
#endif /* (KDEBUG_LEVEL >= KDEBUG_LEVEL_IST) */

int
vfs_getbyid(fsid_t *fsid, ino64_t ino, vnode_t *vpp, vfs_context_t ctx)
{
	mount_t mp;
	int error;

	mp = mount_lookupby_volfsid(fsid->val[0], 1);
	if (mp == NULL) {
		return EINVAL;
	}

	/* Get the target vnode. */
	if (ino == 2) {
		error = VFS_ROOT(mp, vpp, ctx);
	} else {
		error = VFS_VGET(mp, ino, vpp, ctx);
	}

	vfs_unbusy(mp);
	return error;
}
/*
 * Obtain the real path from a legacy volfs style path.
 *
 * Valid formats of input path:
 *
 *	"555/@"
 *	"555/2"
 *	"555/123456"
 *	"555/123456/foobar"
 *
 * Where:
 *	555 represents the volfs file system id
 *	'@' and '2' are aliases to the root of a file system
 *	123456 represents a file id
 *	"foobar" represents a file name
 */
#if CONFIG_VOLFS
static int
vfs_getrealpath(const char * path, char * realpath, size_t bufsize, vfs_context_t ctx)
{
	vnode_t vp;
	struct mount *mp = NULL;
	char  *str;
	char ch;
	unsigned long id;
	ino64_t ino;
	int error;
	int length;

	/* Get file system id and move str to next component. */
	id = strtoul(path, &str, 10);
	if (id == 0 || str[0] != '/') {
		return EINVAL;
	}
	while (*str == '/') {
		str++;
	}
	ch = *str;

	if (id > INT_MAX) {
		return ENOENT;
	}
	mp = mount_lookupby_volfsid((int)id, 1);
	if (mp == NULL) {
		return EINVAL;  /* unexpected failure */
	}
	/* Check for an alias to a file system root. */
	if (ch == '@' && str[1] == '\0') {
		ino = 2;
		str++;
	} else {
		/* Get file id and move str to next component. */
		ino = strtouq(str, &str, 10);
	}

	/* Get the target vnode. */
	if (ino == 2) {
		struct vfs_attr vfsattr;
		int use_vfs_root = TRUE;

		VFSATTR_INIT(&vfsattr);
		VFSATTR_WANTED(&vfsattr, f_capabilities);
		if (vfs_getattr(mp, &vfsattr, vfs_context_kernel()) == 0 &&
		    VFSATTR_IS_SUPPORTED(&vfsattr, f_capabilities)) {
			if ((vfsattr.f_capabilities.capabilities[VOL_CAPABILITIES_FORMAT] & VOL_CAP_FMT_VOL_GROUPS) &&
			    (vfsattr.f_capabilities.valid[VOL_CAPABILITIES_FORMAT] & VOL_CAP_FMT_VOL_GROUPS)) {
				use_vfs_root = FALSE;
			}
		}

		if (use_vfs_root) {
			error = VFS_ROOT(mp, &vp, ctx);
		} else {
			error = VFS_VGET(mp, ino, &vp, ctx);
		}
	} else {
		error = VFS_VGET(mp, ino, &vp, ctx);
	}
	vfs_unbusy(mp);
	if (error) {
		goto out;
	}
	realpath[0] = '\0';

	/* Get the absolute path to this vnode. */
	error = build_path(vp, realpath, (int)bufsize, &length, 0, ctx);
	vnode_put(vp);

	if (error == 0 && *str != '\0') {
		size_t attempt = strlcat(realpath, str, MAXPATHLEN);
		if (attempt > MAXPATHLEN) {
			error = ENAMETOOLONG;
		}
	}
out:
	return error;
}
#endif

void
lookup_compound_vnop_post_hook(int error, vnode_t dvp, vnode_t vp, struct nameidata *ndp, int did_create)
{
	if (error == 0 && vp == NULLVP) {
		panic("NULL vp with error == 0.");
	}

	/*
	 * We don't want to do any of this if we didn't use the compound vnop
	 * to perform the lookup... i.e. if we're allowing and using the legacy pattern,
	 * where we did a full lookup.
	 */
	if ((ndp->ni_flag & NAMEI_COMPOUND_OP_MASK) == 0) {
		return;
	}

	/*
	 * If we're going to continue the lookup, we'll handle
	 * all lookup-related updates at that time.
	 */
	if (error == EKEEPLOOKING) {
		return;
	}

	/*
	 * Only audit or update cache for *found* vnodes.  For creation
	 * neither would happen in the non-compound-vnop case.
	 */
	if ((vp != NULLVP) && !did_create) {
		/*
		 * If MAKEENTRY isn't set, and we've done a successful compound VNOP,
		 * then we certainly don't want to update cache or identity.
		 */
		if ((error != 0) || (ndp->ni_cnd.cn_flags & MAKEENTRY)) {
			lookup_consider_update_cache(dvp, vp, &ndp->ni_cnd, ndp->ni_ncgeneration);
		}
		if (ndp->ni_cnd.cn_flags & AUDITVNPATH1) {
			AUDIT_ARG(vnpath, vp, ARG_VNODE1);
		} else if (ndp->ni_cnd.cn_flags & AUDITVNPATH2) {
			AUDIT_ARG(vnpath, vp, ARG_VNODE2);
		}
	}

	/*
	 * If you created (whether you opened or not), cut a lookup tracepoint
	 * for the parent dir (as would happen without a compound vnop).  Note: we may need
	 * a vnode despite failure in this case!
	 *
	 * If you did not create:
	 *      Found child (succeeded or not): cut a tracepoint for the child.
	 *      Did not find child: cut a tracepoint with the parent.
	 */
	if (kdebug_enable) {
		kdebug_lookup(vp ? vp : dvp, &ndp->ni_cnd);
	}
}
