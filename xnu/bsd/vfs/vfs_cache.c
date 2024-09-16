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
 * Copyright (c) 1989, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Poul-Henning Kamp of the FreeBSD Project.
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
 *
 *	@(#)vfs_cache.c	8.5 (Berkeley) 3/22/95
 */
/*
 * NOTICE: This file was modified by SPARTA, Inc. in 2005 to introduce
 * support for mandatory and extensible security protections.  This notice
 * is included in support of clause 2.2 (b) of the Apple Public License,
 * Version 2.0.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/mount_internal.h>
#include <sys/vnode_internal.h>
#include <miscfs/specfs/specdev.h>
#include <sys/namei.h>
#include <sys/errno.h>
#include <kern/kalloc.h>
#include <sys/kauth.h>
#include <sys/user.h>
#include <sys/paths.h>
#include <os/overflow.h>

#if CONFIG_MACF
#include <security/mac_framework.h>
#endif

/*
 * Name caching works as follows:
 *
 * Names found by directory scans are retained in a cache
 * for future reference.  It is managed LRU, so frequently
 * used names will hang around.  Cache is indexed by hash value
 * obtained from (vp, name) where vp refers to the directory
 * containing name.
 *
 * If it is a "negative" entry, (i.e. for a name that is known NOT to
 * exist) the vnode pointer will be NULL.
 *
 * Upon reaching the last segment of a path, if the reference
 * is for DELETE, or NOCACHE is set (rewrite), and the
 * name is located in the cache, it will be dropped.
 */

/*
 * Structures associated with name cacheing.
 */

ZONE_DEFINE_TYPE(namecache_zone, "namecache", struct namecache, ZC_NONE);

struct smrq_list_head *nchashtbl;       /* Hash Table */
u_long  nchashmask;
u_long  nchash;                         /* size of hash table - 1 */
long    numcache;                       /* number of cache entries allocated */
int     desiredNodes;
int     desiredNegNodes;
int     ncs_negtotal;
TUNABLE_WRITEABLE(int, nc_disabled, "-novfscache", 0);
__options_decl(nc_smr_level_t, uint32_t, {
	NC_SMR_DISABLED = 0,
	NC_SMR_LOOKUP = 1
});
TUNABLE(nc_smr_level_t, nc_smr_enabled, "ncsmr", NC_SMR_LOOKUP);
TAILQ_HEAD(, namecache) nchead;         /* chain of all name cache entries */
TAILQ_HEAD(, namecache) neghead;        /* chain of only negative cache entries */


#if COLLECT_STATS

struct  nchstats nchstats;              /* cache effectiveness statistics */

#define NCHSTAT(v) {            \
	nchstats.v++;           \
}
#define NAME_CACHE_LOCK_SHARED()        name_cache_lock()
#define NAME_CACHE_LOCK_SHARED_TO_EXCLUSIVE() TRUE

#else

#define NCHSTAT(v)
#define NAME_CACHE_LOCK_SHARED()        name_cache_lock_shared()
#define NAME_CACHE_LOCK_SHARED_TO_EXCLUSIVE()             name_cache_lock_shared_to_exclusive()

#endif

#define NAME_CACHE_LOCK()               name_cache_lock()
#define NAME_CACHE_UNLOCK()             name_cache_unlock()

/* vars for name cache list lock */
static LCK_GRP_DECLARE(namecache_lck_grp, "Name Cache");
static LCK_RW_DECLARE(namecache_rw_lock, &namecache_lck_grp);

typedef struct string_t {
	LIST_ENTRY(string_t)  hash_chain;
	char                  *str;
	uint32_t              strbuflen;
	uint32_t              refcount;
} string_t;

ZONE_DEFINE_TYPE(stringcache_zone, "vfsstringcache", string_t, ZC_NONE);

static LCK_GRP_DECLARE(strcache_lck_grp, "String Cache");
static LCK_ATTR_DECLARE(strcache_lck_attr, 0, 0);
LCK_RW_DECLARE_ATTR(strtable_rw_lock, &strcache_lck_grp, &strcache_lck_attr);

static LCK_GRP_DECLARE(rootvnode_lck_grp, "rootvnode");
LCK_RW_DECLARE(rootvnode_rw_lock, &rootvnode_lck_grp);

#define NUM_STRCACHE_LOCKS 1024

lck_mtx_t strcache_mtx_locks[NUM_STRCACHE_LOCKS];

SYSCTL_NODE(_vfs, OID_AUTO, ncstats, CTLFLAG_RD | CTLFLAG_LOCKED, NULL, "vfs name cache stats");

SYSCTL_COMPAT_INT(_vfs_ncstats, OID_AUTO, nc_smr_enabled,
    CTLFLAG_RD | CTLFLAG_LOCKED,
    &nc_smr_enabled, 0, "");

#if COLLECT_NC_SMR_STATS
struct ncstats {
	uint64_t cl_smr_hits;
	uint64_t cl_smr_miss;
	uint64_t cl_smr_negative_hits;
	uint64_t cl_smr_fallback;
	uint64_t cl_lock_hits;
	uint64_t clp_next;
	uint64_t clp_next_fail;
	uint64_t clp_smr_next;
	uint64_t clp_smr_next_fail;
	uint64_t clp_smr_fallback;
	uint64_t nc_lock_shared;
	uint64_t nc_lock;
} ncstats = {0};

SYSCTL_LONG(_vfs_ncstats, OID_AUTO, cl_smr_hits,
    CTLFLAG_RD | CTLFLAG_LOCKED,
    &ncstats.cl_smr_hits, "");
SYSCTL_LONG(_vfs_ncstats, OID_AUTO, cl_smr_misses,
    CTLFLAG_RD | CTLFLAG_LOCKED,
    &ncstats.cl_smr_miss, "");
SYSCTL_LONG(_vfs_ncstats, OID_AUTO, cl_smr_negative_hits,
    CTLFLAG_RD | CTLFLAG_LOCKED,
    &ncstats.cl_smr_negative_hits, "");
SYSCTL_LONG(_vfs_ncstats, OID_AUTO, cl_smr_fallback,
    CTLFLAG_RD | CTLFLAG_LOCKED,
    &ncstats.cl_smr_fallback, "");
SYSCTL_LONG(_vfs_ncstats, OID_AUTO, cl_lock_hits,
    CTLFLAG_RD | CTLFLAG_LOCKED,
    &ncstats.cl_lock_hits, "");
SYSCTL_LONG(_vfs_ncstats, OID_AUTO, clp_next,
    CTLFLAG_RD | CTLFLAG_LOCKED,
    &ncstats.clp_next, "");
SYSCTL_LONG(_vfs_ncstats, OID_AUTO, clp_next_fail,
    CTLFLAG_RD | CTLFLAG_LOCKED,
    &ncstats.clp_next_fail, "");
SYSCTL_LONG(_vfs_ncstats, OID_AUTO, clp_smr_next,
    CTLFLAG_RD | CTLFLAG_LOCKED,
    &ncstats.clp_smr_next, "");
SYSCTL_LONG(_vfs_ncstats, OID_AUTO, clp_smr_next_fail,
    CTLFLAG_RD | CTLFLAG_LOCKED,
    &ncstats.clp_smr_next_fail, "");
SYSCTL_LONG(_vfs_ncstats, OID_AUTO, nc_lock_shared,
    CTLFLAG_RD | CTLFLAG_LOCKED,
    &ncstats.nc_lock_shared, "");
SYSCTL_LONG(_vfs_ncstats, OID_AUTO, nc_lock,
    CTLFLAG_RD | CTLFLAG_LOCKED,
    &ncstats.nc_lock, "");

#define NC_SMR_STATS(v)  os_atomic_inc(&ncstats.v, relaxed)
#else
#define NC_SMR_STATS(v)
#endif /* COLLECT_NC_SMR_STATS */

static vnode_t cache_lookup_locked(vnode_t dvp, struct componentname *cnp, uint32_t *vidp);
static vnode_t cache_lookup_smr(vnode_t dvp, struct componentname *cnp, uint32_t *vidp);
static const char *add_name_internal(const char *, uint32_t, u_int, boolean_t, u_int);
static void init_string_table(void);
static void cache_delete(struct namecache *, int);
static void cache_enter_locked(vnode_t dvp, vnode_t vp, struct componentname *cnp, const char *strname);
static void cache_purge_locked(vnode_t vp, kauth_cred_t *credp);
static void namecache_smr_free(void *, size_t);
static void string_smr_free(void *, size_t);


#ifdef DUMP_STRING_TABLE
/*
 * Internal dump function used for debugging
 */
void dump_string_table(void);
#endif  /* DUMP_STRING_TABLE */

static void init_crc32(void);
static unsigned int crc32tab[256];


#define NCHHASH(dvp, hash_val) \
	(&nchashtbl[(dvp->v_id ^ (hash_val)) & nchashmask])

/*
 * This function tries to check if a directory vp is a subdirectory of dvp
 * only from valid v_parent pointers. It is called with the name cache lock
 * held and does not drop the lock anytime inside the function.
 *
 * It returns a boolean that indicates whether or not it was able to
 * successfully infer the parent/descendent relationship via the v_parent
 * pointers, or if it could not infer such relationship and that the decision
 * must be delegated to the owning filesystem.
 *
 * If it does not defer the decision, i.e. it was successfuly able to determine
 * the parent/descendent relationship,  *is_subdir tells the caller if vp is a
 * subdirectory of dvp.
 *
 * If the decision is deferred, *next_vp is where it stopped i.e. *next_vp
 * is the vnode whose parent is to be determined from the filesystem.
 * *is_subdir, in this case, is not indicative of anything and should be
 * ignored.
 *
 * The return value and output args should be used as follows :
 *
 * defer = cache_check_vnode_issubdir(vp, dvp, is_subdir, next_vp);
 * if (!defer) {
 *      if (*is_subdir)
 *              vp is subdirectory;
 *      else
 *              vp is not a subdirectory;
 * } else {
 *      if (*next_vp)
 *              check this vnode's parent from the filesystem
 *      else
 *              error (likely because of forced unmount).
 * }
 *
 */
static boolean_t
cache_check_vnode_issubdir(vnode_t vp, vnode_t dvp, boolean_t *is_subdir,
    vnode_t *next_vp)
{
	vnode_t tvp = vp;
	int defer = FALSE;

	*is_subdir = FALSE;
	*next_vp = NULLVP;
	while (1) {
		mount_t tmp;

		if (tvp == dvp) {
			*is_subdir = TRUE;
			break;
		} else if (tvp == rootvnode) {
			/* *is_subdir = FALSE */
			break;
		}

		tmp = tvp->v_mount;
		while ((tvp->v_flag & VROOT) && tmp && tmp->mnt_vnodecovered &&
		    tvp != dvp && tvp != rootvnode) {
			tvp = tmp->mnt_vnodecovered;
			tmp = tvp->v_mount;
		}

		/*
		 * If dvp is not at the top of a mount "stack" then
		 * vp is not a subdirectory of dvp either.
		 */
		if (tvp == dvp || tvp == rootvnode) {
			/* *is_subdir = FALSE */
			break;
		}

		if (!tmp) {
			defer = TRUE;
			*next_vp = NULLVP;
			break;
		}

		if ((tvp->v_flag & VISHARDLINK) || !(tvp->v_parent)) {
			defer = TRUE;
			*next_vp = tvp;
			break;
		}

		tvp = tvp->v_parent;
	}

	return defer;
}

/* maximum times retry from potentially transient errors in vnode_issubdir */
#define MAX_ERROR_RETRY 3

/*
 * This function checks if a given directory (vp) is a subdirectory of dvp.
 * It walks backwards from vp and if it hits dvp in its parent chain,
 * it is a subdirectory. If it encounters the root directory, it is not
 * a subdirectory.
 *
 * This function returns an error if it is unsuccessful and 0 on success.
 *
 * On entry (and exit) vp has an iocount and if this function has to take
 * any iocounts on other vnodes in the parent chain traversal, it releases them.
 */
int
vnode_issubdir(vnode_t vp, vnode_t dvp, int *is_subdir, vfs_context_t ctx)
{
	vnode_t start_vp, tvp;
	vnode_t vp_with_iocount;
	int error = 0;
	char dotdotbuf[] = "..";
	int error_retry_count = 0; /* retry count for potentially transient
	                            *  errors */

	*is_subdir = FALSE;
	tvp = start_vp = vp;
	/*
	 * Anytime we acquire an iocount in this function, we save the vnode
	 * in this variable and release it before exiting.
	 */
	vp_with_iocount = NULLVP;

	while (1) {
		boolean_t defer;
		vnode_t pvp;
		uint32_t vid = 0;
		struct componentname cn;
		boolean_t is_subdir_locked = FALSE;

		if (tvp == dvp) {
			*is_subdir = TRUE;
			break;
		} else if (tvp == rootvnode) {
			/* *is_subdir = FALSE */
			break;
		}

		NAME_CACHE_LOCK_SHARED();

		defer = cache_check_vnode_issubdir(tvp, dvp, &is_subdir_locked,
		    &tvp);

		if (defer && tvp) {
			vid = vnode_vid(tvp);
			vnode_hold(tvp);
		}

		NAME_CACHE_UNLOCK();

		if (!defer) {
			*is_subdir = is_subdir_locked;
			break;
		}

		if (!tvp) {
			if (error_retry_count++ < MAX_ERROR_RETRY) {
				tvp = vp;
				continue;
			}
			error = ENOENT;
			break;
		}

		if (tvp != start_vp) {
			if (vp_with_iocount) {
				vnode_put(vp_with_iocount);
				vp_with_iocount = NULLVP;
			}

			error = vnode_getwithvid(tvp, vid);
			vnode_drop(tvp);
			if (error) {
				if (error_retry_count++ < MAX_ERROR_RETRY) {
					tvp = vp;
					error = 0;
					continue;
				}
				break;
			}
			vp_with_iocount = tvp;
		} else {
			tvp = vnode_drop(tvp);
		}

		bzero(&cn, sizeof(cn));
		cn.cn_nameiop = LOOKUP;
		cn.cn_flags = ISLASTCN | ISDOTDOT;
		cn.cn_context = ctx;
		cn.cn_pnbuf = &dotdotbuf[0];
		cn.cn_pnlen = sizeof(dotdotbuf);
		cn.cn_nameptr = cn.cn_pnbuf;
		cn.cn_namelen = 2;

		pvp = NULLVP;
		if ((error = VNOP_LOOKUP(tvp, &pvp, &cn, ctx))) {
			break;
		}

		if (!(tvp->v_flag & VISHARDLINK) && tvp->v_parent != pvp) {
			(void)vnode_update_identity(tvp, pvp, NULL, 0, 0,
			    VNODE_UPDATE_PARENT);
		}

		if (vp_with_iocount) {
			vnode_put(vp_with_iocount);
		}

		vp_with_iocount = tvp = pvp;
	}

	if (vp_with_iocount) {
		vnode_put(vp_with_iocount);
	}

	return error;
}

/*
 * This function builds the path in "buff" from the supplied vnode.
 * The length of the buffer *INCLUDING* the trailing zero byte is
 * returned in outlen.  NOTE: the length includes the trailing zero
 * byte and thus the length is one greater than what strlen would
 * return.  This is important and lots of code elsewhere in the kernel
 * assumes this behavior.
 *
 * This function can call vnop in file system if the parent vnode
 * does not exist or when called for hardlinks via volfs path.
 * If BUILDPATH_NO_FS_ENTER is set in flags, it only uses values present
 * in the name cache and does not enter the file system.
 *
 * If BUILDPATH_CHECK_MOVED is set in flags, we return EAGAIN when
 * we encounter ENOENT during path reconstruction.  ENOENT means that
 * one of the parents moved while we were building the path.  The
 * caller can special handle this case by calling build_path again.
 *
 * If BUILDPATH_VOLUME_RELATIVE is set in flags, we return path
 * that is relative to the nearest mount point, i.e. do not
 * cross over mount points during building the path.
 *
 * passed in vp must have a valid io_count reference
 *
 * If parent vnode is non-NULL it also must have an io count.  This
 * allows build_path_with_parent to be safely called for operations
 * unlink, rmdir and rename that already have io counts on the target
 * and the directory. In this way build_path_with_parent does not have
 * to try and obtain an additional io count on the parent.  Taking an
 * io count ont the parent can lead to dead lock if a forced unmount
 * occures at the right moment. For a fuller explaination on how this
 * can occur see the comment for vn_getpath_with_parent.
 *
 */
int
build_path_with_parent(vnode_t first_vp, vnode_t parent_vp, char *buff, int buflen,
    int *outlen, size_t *mntpt_outlen, int flags, vfs_context_t ctx)
{
	vnode_t vp, tvp;
	vnode_t vp_with_iocount;
	vnode_t proc_root_dir_vp;
	char *end;
	char *mntpt_end;
	const char *str;
	unsigned int  len;
	int  ret = 0;
	int  fixhardlink;

	if (first_vp == NULLVP) {
		return EINVAL;
	}

	if (buflen <= 1) {
		return ENOSPC;
	}

	/*
	 * Grab the process fd so we can evaluate fd_rdir.
	 */
	if (!(flags & BUILDPATH_NO_PROCROOT)) {
		proc_root_dir_vp = vfs_context_proc(ctx)->p_fd.fd_rdir;
	} else {
		proc_root_dir_vp = NULL;
	}

	vp_with_iocount = NULLVP;
again:
	vp = first_vp;

	end = &buff[buflen - 1];
	*end = '\0';
	mntpt_end = NULL;

	/*
	 * Catch a special corner case here: chroot to /full/path/to/dir, chdir to
	 * it, then open it. Without this check, the path to it will be
	 * /full/path/to/dir instead of "/".
	 */
	if (proc_root_dir_vp == first_vp) {
		*--end = '/';
		goto out;
	}

	/*
	 * holding the NAME_CACHE_LOCK in shared mode is
	 * sufficient to stabilize both the vp->v_parent chain
	 * and the 'vp->v_mount->mnt_vnodecovered' chain
	 *
	 * if we need to drop this lock, we must first grab the v_id
	 * from the vnode we're currently working with... if that
	 * vnode doesn't already have an io_count reference (the vp
	 * passed in comes with one), we must grab a reference
	 * after we drop the NAME_CACHE_LOCK via vnode_getwithvid...
	 * deadlocks may result if you call vnode_get while holding
	 * the NAME_CACHE_LOCK... we lazily release the reference
	 * we pick up the next time we encounter a need to drop
	 * the NAME_CACHE_LOCK or before we return from this routine
	 */
	NAME_CACHE_LOCK_SHARED();

#if CONFIG_FIRMLINKS
	if (!(flags & BUILDPATH_NO_FIRMLINK) &&
	    (vp->v_flag & VFMLINKTARGET) && vp->v_fmlink && (vp->v_fmlink->v_type == VDIR)) {
		vp = vp->v_fmlink;
	}
#endif

	/*
	 * Check if this is the root of a file system.
	 */
	while (vp && vp->v_flag & VROOT) {
		if (vp->v_mount == NULL) {
			ret = EINVAL;
			goto out_unlock;
		}
		if ((vp->v_mount->mnt_flag & MNT_ROOTFS) || (vp == proc_root_dir_vp)) {
			/*
			 * It's the root of the root file system, so it's
			 * just "/".
			 */
			*--end = '/';

			goto out_unlock;
		} else {
			/*
			 * This the root of the volume and the caller does not
			 * want to cross mount points.  Therefore just return
			 * '/' as the relative path.
			 */
#if CONFIG_FIRMLINKS
			if (!(flags & BUILDPATH_NO_FIRMLINK) &&
			    (vp->v_flag & VFMLINKTARGET) && vp->v_fmlink && (vp->v_fmlink->v_type == VDIR)) {
				vp = vp->v_fmlink;
			} else
#endif
			if (flags & BUILDPATH_VOLUME_RELATIVE) {
				*--end = '/';
				goto out_unlock;
			} else {
				vp = vp->v_mount->mnt_vnodecovered;
				if (!mntpt_end && vp) {
					mntpt_end = end;
				}
			}
		}
	}

	while ((vp != NULLVP) && (vp->v_parent != vp)) {
		int  vid;

		/*
		 * For hardlinks the v_name may be stale, so if its OK
		 * to enter a file system, ask the file system for the
		 * name and parent (below).
		 */
		fixhardlink = (vp->v_flag & VISHARDLINK) &&
		    (vp->v_mount->mnt_kern_flag & MNTK_PATH_FROM_ID) &&
		    !(flags & BUILDPATH_NO_FS_ENTER);

		if (!fixhardlink) {
			str = vp->v_name;

			if (str == NULL || *str == '\0') {
				if (vp->v_parent != NULL) {
					ret = EINVAL;
				} else {
					ret = ENOENT;
				}
				goto out_unlock;
			}
			len = (unsigned int)strlen(str);
			/*
			 * Check that there's enough space (including space for the '/')
			 */
			if ((unsigned int)(end - buff) < (len + 1)) {
				ret = ENOSPC;
				goto out_unlock;
			}
			/*
			 * Copy the name backwards.
			 */
			str += len;

			for (; len > 0; len--) {
				*--end = *--str;
			}
			/*
			 * Add a path separator.
			 */
			*--end = '/';
		}

		/*
		 * Walk up the parent chain.
		 */
		if (((vp->v_parent != NULLVP) && !fixhardlink) ||
		    (flags & BUILDPATH_NO_FS_ENTER)) {
			/*
			 * In this if () block we are not allowed to enter the filesystem
			 * to conclusively get the most accurate parent identifier.
			 * As a result, if 'vp' does not identify '/' and it
			 * does not have a valid v_parent, then error out
			 * and disallow further path construction
			 */
			if ((vp->v_parent == NULLVP) && (rootvnode != vp)) {
				/*
				 * Only '/' is allowed to have a NULL parent
				 * pointer. Upper level callers should ideally
				 * re-drive name lookup on receiving a ENOENT.
				 */
				ret = ENOENT;

				/* The code below will exit early if 'tvp = vp' == NULL */
			}
			vp = vp->v_parent;

			/*
			 * if the vnode we have in hand isn't a directory and it
			 * has a v_parent, then we started with the resource fork
			 * so skip up to avoid getting a duplicate copy of the
			 * file name in the path.
			 */
			if (vp && !vnode_isdir(vp) && vp->v_parent) {
				vp = vp->v_parent;
			}
		} else {
			/*
			 * No parent, go get it if supported.
			 */
			struct vnode_attr  va;
			vnode_t  dvp;

			/*
			 * Make sure file system supports obtaining a path from id.
			 */
			if (!(vp->v_mount->mnt_kern_flag & MNTK_PATH_FROM_ID)) {
				ret = ENOENT;
				goto out_unlock;
			}
			vid = vp->v_id;

			vnode_hold(vp);
			NAME_CACHE_UNLOCK();

			if (vp != first_vp && vp != parent_vp && vp != vp_with_iocount) {
				if (vp_with_iocount) {
					vnode_put(vp_with_iocount);
					vp_with_iocount = NULLVP;
				}
				if (vnode_getwithvid(vp, vid)) {
					vnode_drop(vp);
					goto again;
				}
				vp_with_iocount = vp;
			}

			vnode_drop(vp);

			VATTR_INIT(&va);
			VATTR_WANTED(&va, va_parentid);

			if (fixhardlink) {
				VATTR_WANTED(&va, va_name);
				va.va_name = zalloc(ZV_NAMEI);
			} else {
				va.va_name = NULL;
			}
			/*
			 * Ask the file system for its parent id and for its name (optional).
			 */
			ret = vnode_getattr(vp, &va, ctx);

			if (ret || !VATTR_IS_SUPPORTED(&va, va_parentid)) {
				ret = ENOENT;
				goto out;
			}

			/*
			 * Ask the file system for the parent vnode.
			 */
			if ((ret = VFS_VGET(vp->v_mount, (ino64_t)va.va_parentid, &dvp, ctx))) {
				goto out;
			}

			/* No exit from here before switching vp_with_iocount to dvp */

			if (fixhardlink) {
				if (VATTR_IS_SUPPORTED(&va, va_name)) {
					str = va.va_name;
				} else {
					ret = ENOENT;
					goto bad_news;
				}
				len = (unsigned int)strlen(str);

				vnode_update_identity(vp, dvp, str, len, 0, VNODE_UPDATE_NAME | VNODE_UPDATE_PARENT);

				/*
				 * Check that there's enough space.
				 */
				if ((unsigned int)(end - buff) < (len + 1)) {
					ret = ENOSPC;
				} else {
					/* Copy the name backwards. */
					str += len;

					for (; len > 0; len--) {
						*--end = *--str;
					}
					/*
					 * Add a path separator.
					 */
					*--end = '/';
				}
bad_news:
				zfree(ZV_NAMEI, va.va_name);
			} else if (vp->v_parent != dvp) {
				vnode_update_identity(vp, dvp, NULL, 0, 0, VNODE_UPDATE_PARENT);
			}

			if (vp_with_iocount) {
				vnode_put(vp_with_iocount);
			}
			vp = dvp;
			vp_with_iocount = vp;

			NAME_CACHE_LOCK_SHARED();

			/*
			 * if the vnode we have in hand isn't a directory and it
			 * has a v_parent, then we started with the resource fork
			 * so skip up to avoid getting a duplicate copy of the
			 * file name in the path.
			 */
			if (vp && !vnode_isdir(vp) && vp->v_parent) {
				vp = vp->v_parent;
			}
		}

		if (vp && (flags & BUILDPATH_CHECKACCESS)) {
			vid = vp->v_id;

			vnode_hold(vp);
			NAME_CACHE_UNLOCK();

			if (vp != first_vp && vp != parent_vp && vp != vp_with_iocount) {
				if (vp_with_iocount) {
					vnode_put(vp_with_iocount);
					vp_with_iocount = NULLVP;
				}
				if (vnode_getwithvid(vp, vid)) {
					vnode_drop(vp);
					goto again;
				}
				vp_with_iocount = vp;
			}
			vnode_drop(vp);

			if ((ret = vnode_authorize(vp, NULL, KAUTH_VNODE_SEARCH, ctx))) {
				goto out;       /* no peeking */
			}
			NAME_CACHE_LOCK_SHARED();
		}

		/*
		 * When a mount point is crossed switch the vp.
		 * Continue until we find the root or we find
		 * a vnode that's not the root of a mounted
		 * file system.
		 */
		tvp = vp;

		while (tvp) {
			if (tvp == proc_root_dir_vp) {
				goto out_unlock;        /* encountered the root */
			}

#if CONFIG_FIRMLINKS
			if (!(flags & BUILDPATH_NO_FIRMLINK) &&
			    (tvp->v_flag & VFMLINKTARGET) && tvp->v_fmlink && (tvp->v_fmlink->v_type == VDIR)) {
				tvp = tvp->v_fmlink;
				break;
			}
#endif

			if (!(tvp->v_flag & VROOT) || !tvp->v_mount) {
				break;                  /* not the root of a mounted FS */
			}
			if (flags & BUILDPATH_VOLUME_RELATIVE) {
				/* Do not cross over mount points */
				tvp = NULL;
			} else {
				tvp = tvp->v_mount->mnt_vnodecovered;
				if (!mntpt_end && tvp) {
					mntpt_end = end;
				}
			}
		}
		if (tvp == NULLVP) {
			goto out_unlock;
		}
		vp = tvp;
	}
out_unlock:
	NAME_CACHE_UNLOCK();
out:
	if (vp_with_iocount) {
		vnode_put(vp_with_iocount);
	}
	/*
	 * Slide the name down to the beginning of the buffer.
	 */
	memmove(buff, end, &buff[buflen] - end);

	/*
	 * length includes the trailing zero byte
	 */
	*outlen = (int)(&buff[buflen] - end);
	if (mntpt_outlen && mntpt_end) {
		*mntpt_outlen = (size_t)*outlen - (size_t)(&buff[buflen] - mntpt_end);
	}

	/* One of the parents was moved during path reconstruction.
	 * The caller is interested in knowing whether any of the
	 * parents moved via BUILDPATH_CHECK_MOVED, so return EAGAIN.
	 */
	if ((ret == ENOENT) && (flags & BUILDPATH_CHECK_MOVED)) {
		ret = EAGAIN;
	}

	return ret;
}

int
build_path(vnode_t first_vp, char *buff, int buflen, int *outlen, int flags, vfs_context_t ctx)
{
	return build_path_with_parent(first_vp, NULL, buff, buflen, outlen, NULL, flags, ctx);
}

/*
 * Combined version of vnode_getparent() and vnode_getname() to acquire both vnode name and parent
 * without releasing the name cache lock in interim.
 */
void
vnode_getparent_and_name(vnode_t vp, vnode_t *out_pvp, const char **out_name)
{
	vnode_t pvp = NULLVP;
	int     locked = 0;
	int     pvid;

	NAME_CACHE_LOCK_SHARED();
	locked = 1;

	if (out_name) {
		const char *name = NULL;
		if (vp->v_name) {
			name = vfs_addname(vp->v_name, (unsigned int)strlen(vp->v_name), 0, 0);
		}
		*out_name = name;
	}

	if (!out_pvp) {
		goto out;
	}

	pvp = vp->v_parent;

	/*
	 * v_parent is stable behind the name_cache lock
	 * however, the only thing we can really guarantee
	 * is that we've grabbed a valid iocount on the
	 * parent of 'vp' at the time we took the name_cache lock...
	 * once we drop the lock, vp could get re-parented
	 */
	if (pvp != NULLVP) {
		pvid = pvp->v_id;

		vnode_hold(pvp);
		NAME_CACHE_UNLOCK();
		locked = 0;

		if (vnode_getwithvid(pvp, pvid) != 0) {
			vnode_drop(pvp);
			pvp = NULL;
		} else {
			vnode_drop(pvp);
		}
	}
	*out_pvp = pvp;

out:
	if (locked) {
		NAME_CACHE_UNLOCK();
	}
}

/*
 * return NULLVP if vp's parent doesn't
 * exist, or we can't get a valid iocount
 * else return the parent of vp
 */
vnode_t
vnode_getparent(vnode_t vp)
{
	vnode_t pvp = NULLVP;
	vnode_getparent_and_name(vp, &pvp, NULL);

	return pvp;
}

/*
 * Similar to vnode_getparent() but only returned parent vnode (with iocount
 * held) if the actual parent vnode is different than the given 'pvp'.
 */
__private_extern__ vnode_t
vnode_getparent_if_different(vnode_t vp, vnode_t pvp)
{
	vnode_t real_pvp = NULLVP;
	int     pvid;

	if (vp->v_parent == pvp) {
		goto out;
	}

	NAME_CACHE_LOCK_SHARED();

	real_pvp = vp->v_parent;
	if (real_pvp == NULLVP) {
		NAME_CACHE_UNLOCK();
		goto out;
	}

	/*
	 * Do the check again after namecache lock is acquired as the parent vnode
	 * could have changed.
	 */
	if (real_pvp != pvp) {
		pvid = real_pvp->v_id;

		vnode_hold(real_pvp);
		NAME_CACHE_UNLOCK();

		if (vnode_getwithvid(real_pvp, pvid) != 0) {
			vnode_drop(real_pvp);
			real_pvp = NULLVP;
		} else {
			vnode_drop(real_pvp);
		}
	} else {
		real_pvp = NULLVP;
		NAME_CACHE_UNLOCK();
	}

out:
	return real_pvp;
}

const char *
vnode_getname(vnode_t vp)
{
	const char *name = NULL;
	vnode_getparent_and_name(vp, NULL, &name);

	return name;
}

void
vnode_putname(const char *name)
{
	if (name) {
		vfs_removename(name);
	}
}

static const char unknown_vnodename[] = "(unknown vnode name)";

const char *
vnode_getname_printable(vnode_t vp)
{
	const char *name = vnode_getname(vp);
	if (name != NULL) {
		return name;
	}

	switch (vp->v_type) {
	case VCHR:
	case VBLK:
	{
		/*
		 * Create an artificial dev name from
		 * major and minor device number
		 */
		char dev_name[64];
		(void) snprintf(dev_name, sizeof(dev_name),
		    "%c(%u, %u)", VCHR == vp->v_type ? 'c':'b',
		    major(vp->v_rdev), minor(vp->v_rdev));
		/*
		 * Add the newly created dev name to the name
		 * cache to allow easier cleanup. Also,
		 * vfs_addname allocates memory for the new name
		 * and returns it.
		 */
		NAME_CACHE_LOCK_SHARED();
		name = vfs_addname(dev_name, (unsigned int)strlen(dev_name), 0, 0);
		NAME_CACHE_UNLOCK();
		return name;
	}
	default:
		return unknown_vnodename;
	}
}

void
vnode_putname_printable(const char *name)
{
	if (name == unknown_vnodename) {
		return;
	}
	vnode_putname(name);
}


/*
 * if VNODE_UPDATE_PARENT, and we can take
 * a reference on dvp, then update vp with
 * it's new parent... if vp already has a parent,
 * then drop the reference vp held on it
 *
 * if VNODE_UPDATE_NAME,
 * then drop string ref on v_name if it exists, and if name is non-NULL
 * then pick up a string reference on name and record it in v_name...
 * optionally pass in the length and hashval of name if known
 *
 * if VNODE_UPDATE_CACHE, flush the name cache entries associated with vp
 */
void
vnode_update_identity(vnode_t vp, vnode_t dvp, const char *name, int name_len, uint32_t name_hashval, int flags)
{
	struct  namecache *ncp;
	vnode_t old_parentvp = NULLVP;
	int isstream = (vp->v_flag & VISNAMEDSTREAM);
	int kusecountbumped = 0;
	kauth_cred_t tcred = NULL;
	const char *vname = NULL;
	const char *tname = NULL;

	if (name_len < 0) {
		return;
	}

	if (flags & VNODE_UPDATE_PARENT) {
		if (dvp && vnode_ref(dvp) != 0) {
			dvp = NULLVP;
		}
		/* Don't count a stream's parent ref during unmounts */
		if (isstream && dvp && (dvp != vp) && (dvp != vp->v_parent) && (dvp->v_type == VREG)) {
			vnode_lock_spin(dvp);
			++dvp->v_kusecount;
			kusecountbumped = 1;
			vnode_unlock(dvp);
		}
	} else {
		dvp = NULLVP;
	}
	if ((flags & VNODE_UPDATE_NAME)) {
		if (name != vp->v_name) {
			if (name && *name) {
				if (name_len == 0) {
					name_len = (int)strlen(name);
				}
				tname = vfs_addname(name, name_len, name_hashval, 0);
			}
		} else {
			flags &= ~VNODE_UPDATE_NAME;
		}
	}
	if ((flags & (VNODE_UPDATE_PURGE | VNODE_UPDATE_PARENT | VNODE_UPDATE_CACHE | VNODE_UPDATE_NAME | VNODE_UPDATE_PURGEFIRMLINK))) {
		NAME_CACHE_LOCK();

#if CONFIG_FIRMLINKS
		if (flags & VNODE_UPDATE_PURGEFIRMLINK) {
			vnode_t old_fvp = vp->v_fmlink;
			if (old_fvp) {
				vnode_lock_spin(vp);
				vp->v_flag &= ~VFMLINKTARGET;
				vp->v_fmlink = NULLVP;
				vnode_unlock(vp);
				NAME_CACHE_UNLOCK();

				/*
				 * vnode_rele can result in cascading series of
				 * usecount releases. The combination of calling
				 * vnode_recycle and dont_reenter (3rd arg to
				 * vnode_rele_internal) ensures we don't have
				 * that issue.
				 */
				vnode_recycle(old_fvp);
				vnode_rele_internal(old_fvp, O_EVTONLY, 1, 0);

				NAME_CACHE_LOCK();
			}
		}
#endif

		if ((flags & VNODE_UPDATE_PURGE)) {
			if (vp->v_parent) {
				vp->v_parent->v_nc_generation++;
			}

			while ((ncp = LIST_FIRST(&vp->v_nclinks))) {
				cache_delete(ncp, 1);
			}

			while ((ncp = TAILQ_FIRST(&vp->v_ncchildren))) {
				cache_delete(ncp, 1);
			}

			/*
			 * Use a temp variable to avoid kauth_cred_drop() while NAME_CACHE_LOCK is held
			 */
			tcred = vnode_cred(vp);
			vp->v_cred = NOCRED;
			vp->v_authorized_actions = 0;
			vp->v_cred_timestamp = 0;
		}
		if ((flags & VNODE_UPDATE_NAME)) {
			vname = vp->v_name;
			vp->v_name = tname;
		}
		if (flags & VNODE_UPDATE_PARENT) {
			if (dvp != vp && dvp != vp->v_parent) {
				old_parentvp = vp->v_parent;
				vp->v_parent = dvp;
				dvp = NULLVP;

				if (old_parentvp) {
					flags |= VNODE_UPDATE_CACHE;
				}
			}
		}
		if (flags & VNODE_UPDATE_CACHE) {
			while ((ncp = LIST_FIRST(&vp->v_nclinks))) {
				cache_delete(ncp, 1);
			}
		}
		NAME_CACHE_UNLOCK();

		if (vname != NULL) {
			vfs_removename(vname);
		}

		if (IS_VALID_CRED(tcred)) {
			kauth_cred_unref(&tcred);
		}
	}
	if (dvp != NULLVP) {
		/* Back-out the ref we took if we lost a race for vp->v_parent. */
		if (kusecountbumped) {
			vnode_lock_spin(dvp);
			if (dvp->v_kusecount > 0) {
				--dvp->v_kusecount;
			}
			vnode_unlock(dvp);
		}
		vnode_rele(dvp);
	}
	if (old_parentvp) {
		struct  uthread *ut;
		vnode_t vreclaims = NULLVP;

		if (isstream) {
			vnode_lock_spin(old_parentvp);
			if ((old_parentvp->v_type != VDIR) && (old_parentvp->v_kusecount > 0)) {
				--old_parentvp->v_kusecount;
			}
			vnode_unlock(old_parentvp);
		}
		ut = current_uthread();

		/*
		 * indicated to vnode_rele that it shouldn't do a
		 * vnode_reclaim at this time... instead it will
		 * chain the vnode to the uu_vreclaims list...
		 * we'll be responsible for calling vnode_reclaim
		 * on each of the vnodes in this list...
		 */
		ut->uu_defer_reclaims = 1;
		ut->uu_vreclaims = NULLVP;

		while ((vp = old_parentvp) != NULLVP) {
			vnode_hold(vp);
			vnode_lock_spin(vp);
			vnode_rele_internal(vp, 0, 0, 1);

			/*
			 * check to see if the vnode is now in the state
			 * that would have triggered a vnode_reclaim in vnode_rele
			 * if it is, we save it's parent pointer and then NULL
			 * out the v_parent field... we'll drop the reference
			 * that was held on the next iteration of this loop...
			 * this short circuits a potential deep recursion if we
			 * have a long chain of parents in this state...
			 * we'll sit in this loop until we run into
			 * a parent in this chain that is not in this state
			 *
			 * make our check and the vnode_rele atomic
			 * with respect to the current vnode we're working on
			 * by holding the vnode lock
			 * if vnode_rele deferred the vnode_reclaim and has put
			 * this vnode on the list to be reaped by us, than
			 * it has left this vnode with an iocount == 1
			 */
			if (ut->uu_vreclaims == vp) {
				/*
				 * This vnode is on the head of the uu_vreclaims chain
				 * which means vnode_rele wanted to do a vnode_reclaim
				 * on this vnode. Pull the parent pointer now so that when we do the
				 * vnode_reclaim for each of the vnodes in the uu_vreclaims
				 * list, we won't recurse back through here
				 *
				 * need to do a convert here in case vnode_rele_internal
				 * returns with the lock held in the spin mode... it
				 * can drop and retake the lock under certain circumstances
				 */
				vnode_lock_convert(vp);

				NAME_CACHE_LOCK();
				old_parentvp = vp->v_parent;
				vp->v_parent = NULLVP;
				NAME_CACHE_UNLOCK();
			} else {
				/*
				 * we're done... we ran into a vnode that isn't
				 * being terminated
				 */
				old_parentvp = NULLVP;
			}
			vnode_drop_and_unlock(vp);
		}
		vreclaims = ut->uu_vreclaims;
		ut->uu_vreclaims = NULLVP;
		ut->uu_defer_reclaims = 0;

		while ((vp = vreclaims) != NULLVP) {
			vreclaims = vp->v_defer_reclaimlist;

			/*
			 * vnode_put will drive the vnode_reclaim if
			 * we are still the only reference on this vnode
			 */
			vnode_put(vp);
		}
	}
}

#if CONFIG_FIRMLINKS
errno_t
vnode_setasfirmlink(vnode_t vp, vnode_t target_vp)
{
	int error = 0;
	vnode_t old_target_vp = NULLVP;
	vnode_t old_target_vp_v_fmlink = NULLVP;
	kauth_cred_t target_vp_cred = NULL;
	kauth_cred_t old_target_vp_cred = NULL;

	if (!vp) {
		return EINVAL;
	}

	if (target_vp) {
		if (vp->v_fmlink == target_vp) { /* Will be checked again under the name cache lock */
			return 0;
		}

		/*
		 * Firmlink source and target will take both a usecount
		 * and kusecount on each other.
		 */
		if ((error = vnode_ref_ext(target_vp, O_EVTONLY, VNODE_REF_FORCE))) {
			return error;
		}

		if ((error = vnode_ref_ext(vp, O_EVTONLY, VNODE_REF_FORCE))) {
			vnode_rele_ext(target_vp, O_EVTONLY, 1);
			return error;
		}
	}

	NAME_CACHE_LOCK();

	old_target_vp = vp->v_fmlink;
	if (target_vp && (target_vp == old_target_vp)) {
		NAME_CACHE_UNLOCK();
		return 0;
	}
	vp->v_fmlink = target_vp;

	vnode_lock_spin(vp);
	vp->v_flag &= ~VFMLINKTARGET;
	vnode_unlock(vp);

	if (target_vp) {
		target_vp->v_fmlink = vp;
		vnode_lock_spin(target_vp);
		target_vp->v_flag |= VFMLINKTARGET;
		vnode_unlock(target_vp);
		cache_purge_locked(vp, &target_vp_cred);
	}

	if (old_target_vp) {
		old_target_vp_v_fmlink = old_target_vp->v_fmlink;
		old_target_vp->v_fmlink = NULLVP;
		vnode_lock_spin(old_target_vp);
		old_target_vp->v_flag &= ~VFMLINKTARGET;
		vnode_unlock(old_target_vp);
		cache_purge_locked(vp, &old_target_vp_cred);
	}

	NAME_CACHE_UNLOCK();

	if (IS_VALID_CRED(target_vp_cred)) {
		kauth_cred_unref(&target_vp_cred);
	}

	if (old_target_vp) {
		if (IS_VALID_CRED(old_target_vp_cred)) {
			kauth_cred_unref(&old_target_vp_cred);
		}

		vnode_rele_ext(old_target_vp, O_EVTONLY, 1);
		if (old_target_vp_v_fmlink) {
			vnode_rele_ext(old_target_vp_v_fmlink, O_EVTONLY, 1);
		}
	}

	return 0;
}

errno_t
vnode_getfirmlink(vnode_t vp, vnode_t *target_vp)
{
	int error;

	if (!vp->v_fmlink) {
		return ENODEV;
	}

	NAME_CACHE_LOCK_SHARED();
	if (vp->v_fmlink && !(vp->v_flag & VFMLINKTARGET) &&
	    (vnode_get(vp->v_fmlink) == 0)) {
		vnode_t tvp = vp->v_fmlink;

		vnode_lock_spin(tvp);
		if (tvp->v_lflag & (VL_TERMINATE | VL_DEAD)) {
			vnode_unlock(tvp);
			NAME_CACHE_UNLOCK();
			vnode_put(tvp);
			return ENOENT;
		}
		if (!(tvp->v_flag & VFMLINKTARGET)) {
			panic("firmlink target for vnode %p does not have flag set", vp);
		}
		vnode_unlock(tvp);
		*target_vp = tvp;
		error = 0;
	} else {
		*target_vp = NULLVP;
		error = ENODEV;
	}
	NAME_CACHE_UNLOCK();
	return error;
}

#else /* CONFIG_FIRMLINKS */

errno_t
vnode_setasfirmlink(__unused vnode_t vp, __unused vnode_t src_vp)
{
	return ENOTSUP;
}

errno_t
vnode_getfirmlink(__unused vnode_t vp, __unused vnode_t *target_vp)
{
	return ENOTSUP;
}

#endif

/*
 * Mark a vnode as having multiple hard links.  HFS makes use of this
 * because it keeps track of each link separately, and wants to know
 * which link was actually used.
 *
 * This will cause the name cache to force a VNOP_LOOKUP on the vnode
 * so that HFS can post-process the lookup.  Also, volfs will call
 * VNOP_GETATTR2 to determine the parent, instead of using v_parent.
 */
void
vnode_setmultipath(vnode_t vp)
{
	vnode_lock_spin(vp);

	/*
	 * In theory, we're changing the vnode's identity as far as the
	 * name cache is concerned, so we ought to grab the name cache lock
	 * here.  However, there is already a race, and grabbing the name
	 * cache lock only makes the race window slightly smaller.
	 *
	 * The race happens because the vnode already exists in the name
	 * cache, and could be found by one thread before another thread
	 * can set the hard link flag.
	 */

	vp->v_flag |= VISHARDLINK;

	vnode_unlock(vp);
}



/*
 * backwards compatibility
 */
void
vnode_uncache_credentials(vnode_t vp)
{
	vnode_uncache_authorized_action(vp, KAUTH_INVALIDATE_CACHED_RIGHTS);
}


/*
 * use the exclusive form of NAME_CACHE_LOCK to protect the update of the
 * following fields in the vnode: v_cred_timestamp, v_cred, v_authorized_actions
 * we use this lock so that we can look at the v_cred and v_authorized_actions
 * atomically while behind the NAME_CACHE_LOCK in shared mode in 'cache_lookup_path',
 * which is the super-hot path... if we are updating the authorized actions for this
 * vnode, we are already in the super-slow and far less frequented path so its not
 * that bad that we take the lock exclusive for this case... of course we strive
 * to hold it for the minimum amount of time possible
 */

void
vnode_uncache_authorized_action(vnode_t vp, kauth_action_t action)
{
	kauth_cred_t tcred = NOCRED;

	NAME_CACHE_LOCK();

	vp->v_authorized_actions &= ~action;

	if (action == KAUTH_INVALIDATE_CACHED_RIGHTS &&
	    IS_VALID_CRED(vp->v_cred)) {
		/*
		 * Use a temp variable to avoid kauth_cred_unref() while NAME_CACHE_LOCK is held
		 */
		tcred = vnode_cred(vp);
		vp->v_cred = NOCRED;
	}
	NAME_CACHE_UNLOCK();

	if (IS_VALID_CRED(tcred)) {
		kauth_cred_unref(&tcred);
	}
}


/* disable vnode_cache_is_authorized() by setting vnode_cache_defeat */
static TUNABLE(int, bootarg_vnode_cache_defeat, "-vnode_cache_defeat", 0);

boolean_t
vnode_cache_is_authorized(vnode_t vp, vfs_context_t ctx, kauth_action_t action)
{
	kauth_cred_t    ucred;
	boolean_t       retval = FALSE;

	/* Boot argument to defeat rights caching */
	if (bootarg_vnode_cache_defeat) {
		return FALSE;
	}

	if ((vp->v_mount->mnt_kern_flag & (MNTK_AUTH_OPAQUE | MNTK_AUTH_CACHE_TTL))) {
		/*
		 * a TTL is enabled on the rights cache... handle it here
		 * a TTL of 0 indicates that no rights should be cached
		 */
		if (vp->v_mount->mnt_authcache_ttl) {
			if (!(vp->v_mount->mnt_kern_flag & MNTK_AUTH_CACHE_TTL)) {
				/*
				 * For filesystems marked only MNTK_AUTH_OPAQUE (generally network ones),
				 * we will only allow a SEARCH right on a directory to be cached...
				 * that cached right always has a default TTL associated with it
				 */
				if (action != KAUTH_VNODE_SEARCH || vp->v_type != VDIR) {
					vp = NULLVP;
				}
			}
			if (vp != NULLVP && vnode_cache_is_stale(vp) == TRUE) {
				vnode_uncache_authorized_action(vp, vp->v_authorized_actions);
				vp = NULLVP;
			}
		} else {
			vp = NULLVP;
		}
	}
	if (vp != NULLVP) {
		ucred = vfs_context_ucred(ctx);

		NAME_CACHE_LOCK_SHARED();

		if (vnode_cred(vp) == ucred && (vp->v_authorized_actions & action) == action) {
			retval = TRUE;
		}

		NAME_CACHE_UNLOCK();
	}
	return retval;
}


void
vnode_cache_authorized_action(vnode_t vp, vfs_context_t ctx, kauth_action_t action)
{
	kauth_cred_t tcred = NOCRED;
	kauth_cred_t ucred;
	struct timeval tv;
	boolean_t ttl_active = FALSE;

	ucred = vfs_context_ucred(ctx);

	if (!IS_VALID_CRED(ucred) || action == 0) {
		return;
	}

	if ((vp->v_mount->mnt_kern_flag & (MNTK_AUTH_OPAQUE | MNTK_AUTH_CACHE_TTL))) {
		/*
		 * a TTL is enabled on the rights cache... handle it here
		 * a TTL of 0 indicates that no rights should be cached
		 */
		if (vp->v_mount->mnt_authcache_ttl == 0) {
			return;
		}

		if (!(vp->v_mount->mnt_kern_flag & MNTK_AUTH_CACHE_TTL)) {
			/*
			 * only cache SEARCH action for filesystems marked
			 * MNTK_AUTH_OPAQUE on VDIRs...
			 * the lookup_path code will time these out
			 */
			if ((action & ~KAUTH_VNODE_SEARCH) || vp->v_type != VDIR) {
				return;
			}
		}
		ttl_active = TRUE;

		microuptime(&tv);
	}
	NAME_CACHE_LOCK();

	tcred = vnode_cred(vp);
	if (tcred == ucred) {
		tcred = NOCRED;
	} else {
		/*
		 * Use a temp variable to avoid kauth_cred_drop() while NAME_CACHE_LOCK is held
		 */
		kauth_cred_ref(ucred);
		vp->v_cred = ucred;
		vp->v_authorized_actions = 0;
	}
	if (ttl_active == TRUE && vp->v_authorized_actions == 0) {
		/*
		 * only reset the timestamnp on the
		 * first authorization cached after the previous
		 * timer has expired or we're switching creds...
		 * 'vnode_cache_is_authorized' will clear the
		 * authorized actions if the TTL is active and
		 * it has expired
		 */
		vp->v_cred_timestamp = (int)tv.tv_sec;
	}
	vp->v_authorized_actions |= action;

	NAME_CACHE_UNLOCK();

	if (IS_VALID_CRED(tcred)) {
		kauth_cred_unref(&tcred);
	}
}


boolean_t
vnode_cache_is_stale(vnode_t vp)
{
	struct timeval  tv;
	boolean_t       retval;

	microuptime(&tv);

	if ((tv.tv_sec - vp->v_cred_timestamp) > vp->v_mount->mnt_authcache_ttl) {
		retval = TRUE;
	} else {
		retval = FALSE;
	}

	return retval;
}

VFS_SMR_DECLARE;

/*
 * Components of nameidata (or objects it can point to) which may
 * need restoring in case fast path lookup fails.
 */
struct nameidata_state {
	u_long  ni_loopcnt;
	char *ni_next;
	u_int ni_pathlen;
	int32_t ni_flag;
	char *cn_nameptr;
	int cn_namelen;
	int cn_flags;
	uint32_t cn_hash;
};

static void
save_ndp_state(struct nameidata *ndp, struct componentname *cnp, struct nameidata_state *saved_statep)
{
	saved_statep->ni_loopcnt = ndp->ni_loopcnt;
	saved_statep->ni_next = ndp->ni_next;
	saved_statep->ni_pathlen = ndp->ni_pathlen;
	saved_statep->ni_flag = ndp->ni_flag;
	saved_statep->cn_nameptr = cnp->cn_nameptr;
	saved_statep->cn_namelen = cnp->cn_namelen;
	saved_statep->cn_flags = cnp->cn_flags;
	saved_statep->cn_hash = cnp->cn_hash;
}

static void
restore_ndp_state(struct nameidata *ndp, struct componentname *cnp, struct nameidata_state *saved_statep)
{
	ndp->ni_loopcnt = saved_statep->ni_loopcnt;
	ndp->ni_next = saved_statep->ni_next;
	ndp->ni_pathlen = saved_statep->ni_pathlen;
	ndp->ni_flag = saved_statep->ni_flag;
	cnp->cn_nameptr = saved_statep->cn_nameptr;
	cnp->cn_namelen = saved_statep->cn_namelen;
	cnp->cn_flags = saved_statep->cn_flags;
	cnp->cn_hash = saved_statep->cn_hash;
}

static inline bool
vid_is_same(vnode_t vp, uint32_t vid)
{
	return !(os_atomic_load(&vp->v_lflag, relaxed) & (VL_DRAIN | VL_TERMINATE | VL_DEAD)) && (vnode_vid(vp) == vid);
}

static inline bool
can_check_v_mountedhere(vnode_t vp)
{
	return (os_atomic_load(&vp->v_usecount, relaxed) > 0) &&
	       (os_atomic_load(&vp->v_flag, relaxed) & VMOUNTEDHERE) &&
	       !(os_atomic_load(&vp->v_lflag, relaxed) & (VL_TERMINATE | VL_DEAD) &&
	       (vp->v_type == VDIR));
}

/*
 * Returns:	0			Success
 *		ERECYCLE		vnode was recycled from underneath us.  Force lookup to be re-driven from namei.
 *                                              This errno value should not be seen by anyone outside of the kernel.
 */
int
cache_lookup_path(struct nameidata *ndp, struct componentname *cnp, vnode_t dp,
    vfs_context_t ctx, int *dp_authorized, vnode_t last_dp)
{
	struct nameidata_state saved_state;
	char            *cp;            /* pointer into pathname argument */
	uint32_t        vid;
	uint32_t        vvid = 0;       /* protected by vp != NULLVP */
	vnode_t         vp = NULLVP;
	vnode_t         tdp = NULLVP;
	vnode_t         start_dp = dp;
	kauth_cred_t    ucred;
	boolean_t       ttl_enabled = FALSE;
	struct timeval  tv;
	mount_t         mp;
	mount_t         dmp;
	unsigned int    hash;
	int             error = 0;
	boolean_t       dotdotchecked = FALSE;
	bool            locked = false;
	bool            needs_lock = false;
	bool            dp_iocount_taken = false;

#if CONFIG_TRIGGERS
	vnode_t         trigger_vp;
#endif /* CONFIG_TRIGGERS */

	ucred = vfs_context_ucred(ctx);
retry:
	if (nc_smr_enabled && !needs_lock) {
		save_ndp_state(ndp, cnp, &saved_state);
		vfs_smr_enter();
	} else {
		NAME_CACHE_LOCK_SHARED();
		locked = true;
	}
	ndp->ni_flag &= ~(NAMEI_TRAILINGSLASH);

	dmp = dp->v_mount;
	vid = dp->v_id;
	if (dmp && (dmp->mnt_kern_flag & (MNTK_AUTH_OPAQUE | MNTK_AUTH_CACHE_TTL))) {
		ttl_enabled = TRUE;
		microuptime(&tv);
	}
	for (;;) {
		/*
		 * Search a directory.
		 *
		 * The cn_hash value is for use by cache_lookup
		 * The last component of the filename is left accessible via
		 * cnp->cn_nameptr for callers that need the name.
		 */
		hash = 0;
		cp = cnp->cn_nameptr;

		while (*cp && (*cp != '/')) {
			hash = crc32tab[((hash >> 24) ^ (unsigned char)*cp++)] ^ hash << 8;
		}
		/*
		 * the crc generator can legitimately generate
		 * a 0... however, 0 for us means that we
		 * haven't computed a hash, so use 1 instead
		 */
		if (hash == 0) {
			hash = 1;
		}
		cnp->cn_hash = hash;
		cnp->cn_namelen = (int)(cp - cnp->cn_nameptr);

		ndp->ni_pathlen -= cnp->cn_namelen;
		ndp->ni_next = cp;

		/*
		 * Replace multiple slashes by a single slash and trailing slashes
		 * by a null.  This must be done before VNOP_LOOKUP() because some
		 * fs's don't know about trailing slashes.  Remember if there were
		 * trailing slashes to handle symlinks, existing non-directories
		 * and non-existing files that won't be directories specially later.
		 */
		while (*cp == '/' && (cp[1] == '/' || cp[1] == '\0')) {
			cp++;
			ndp->ni_pathlen--;

			if (*cp == '\0') {
				ndp->ni_flag |= NAMEI_TRAILINGSLASH;
				*ndp->ni_next = '\0';
			}
		}
		ndp->ni_next = cp;

		cnp->cn_flags &= ~(MAKEENTRY | ISLASTCN | ISDOTDOT);

		if (*cp == '\0') {
			cnp->cn_flags |= ISLASTCN;
		}

		if (cnp->cn_namelen == 2 && cnp->cn_nameptr[1] == '.' && cnp->cn_nameptr[0] == '.') {
			cnp->cn_flags |= ISDOTDOT;
		}

#if NAMEDRSRCFORK
		/*
		 * Process a request for a file's resource fork.
		 *
		 * Consume the _PATH_RSRCFORKSPEC suffix and tag the path.
		 */
		if ((ndp->ni_pathlen == sizeof(_PATH_RSRCFORKSPEC)) &&
		    (cp[1] == '.' && cp[2] == '.') &&
		    bcmp(cp, _PATH_RSRCFORKSPEC, sizeof(_PATH_RSRCFORKSPEC)) == 0) {
			/* Skip volfs file systems that don't support native streams. */
			if ((dmp != NULL) &&
			    (dmp->mnt_flag & MNT_DOVOLFS) &&
			    (dmp->mnt_kern_flag & MNTK_NAMED_STREAMS) == 0) {
				goto skiprsrcfork;
			}
			cnp->cn_flags |= CN_WANTSRSRCFORK;
			cnp->cn_flags |= ISLASTCN;
			ndp->ni_next[0] = '\0';
			ndp->ni_pathlen = 1;
		}
skiprsrcfork:
#endif

		*dp_authorized = 0;

#if CONFIG_FIRMLINKS
		if ((cnp->cn_flags & ISDOTDOT) && (dp->v_flag & VFMLINKTARGET) && dp->v_fmlink) {
			/*
			 * If this is a firmlink target then dp has to be switched to the
			 * firmlink "source" before exiting this loop.
			 *
			 * For a firmlink "target", the policy is to pick the parent of the
			 * firmlink "source" as the parent. This means that you can never
			 * get to the "real" parent of firmlink target via a dotdot lookup.
			 */
			vnode_t v_fmlink = dp->v_fmlink;
			uint32_t old_vid = vid;
			mp = dmp;
			if (v_fmlink) {
				vid = v_fmlink->v_id;
				dmp = v_fmlink->v_mount;
				if ((dp->v_fmlink == v_fmlink) && dmp) {
					dp = v_fmlink;
				} else {
					vid = old_vid;
					dmp = mp;
				}
			}
		}
#endif


		if (ttl_enabled &&
		    (dmp->mnt_authcache_ttl == 0 ||
		    ((tv.tv_sec - dp->v_cred_timestamp) > dmp->mnt_authcache_ttl))) {
			break;
		}

		/*
		 * NAME_CACHE_LOCK holds these fields stable
		 *
		 * We can't cache KAUTH_VNODE_SEARCHBYANYONE for root correctly
		 * so we make an ugly check for root here. root is always
		 * allowed and breaking out of here only to find out that is
		 * authorized by virtue of being root is very very expensive.
		 * However, the check for not root is valid only for filesystems
		 * which use local authorization.
		 *
		 * XXX: Remove the check for root when we can reliably set
		 * KAUTH_VNODE_SEARCHBYANYONE as root.
		 */
		int v_authorized_actions = os_atomic_load(&dp->v_authorized_actions, relaxed);
		if ((vnode_cred(dp) != ucred || !(v_authorized_actions & KAUTH_VNODE_SEARCH)) &&
		    !(v_authorized_actions & KAUTH_VNODE_SEARCHBYANYONE) &&
		    (ttl_enabled || !vfs_context_issuser(ctx))) {
			break;
		}

		/*
		 * indicate that we're allowed to traverse this directory...
		 * even if we fail the cache lookup or decide to bail for
		 * some other reason, this information is valid and is used
		 * to avoid doing a vnode_authorize before the call to VNOP_LOOKUP
		 */
		*dp_authorized = 1;

		if ((cnp->cn_flags & (ISLASTCN | ISDOTDOT))) {
			if (cnp->cn_nameiop != LOOKUP) {
				break;
			}
			if (cnp->cn_flags & LOCKPARENT) {
				break;
			}
			if (cnp->cn_flags & NOCACHE) {
				break;
			}

			if (cnp->cn_flags & ISDOTDOT) {
				/*
				 * Force directory hardlinks to go to
				 * file system for ".." requests.
				 */
				if ((dp->v_flag & VISHARDLINK)) {
					break;
				}
				/*
				 * Quit here only if we can't use
				 * the parent directory pointer or
				 * don't have one.  Otherwise, we'll
				 * use it below.
				 */
				if ((dp->v_flag & VROOT) ||
				    dp == ndp->ni_rootdir ||
				    dp->v_parent == NULLVP) {
					break;
				}
			}
		}

		if ((cnp->cn_flags & CN_SKIPNAMECACHE)) {
			/*
			 * Force lookup to go to the filesystem with
			 * all cnp fields set up.
			 */
			break;
		}

		/*
		 * "." and ".." aren't supposed to be cached, so check
		 * for them before checking the cache.
		 */
		if (cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.') {
			vp = dp;
			vvid = vid;
		} else if ((cnp->cn_flags & ISDOTDOT)) {
			/*
			 * If this is a chrooted process, we need to check if
			 * the process is trying to break out of its chrooted
			 * jail. We do that by trying to determine if dp is
			 * a subdirectory of ndp->ni_rootdir. If we aren't
			 * able to determine that by the v_parent pointers, we
			 * will leave the fast path.
			 *
			 * Since this function may see dotdot components
			 * many times and it has the name cache lock held for
			 * the entire duration, we optimise this by doing this
			 * check only once per cache_lookup_path call.
			 * If dotdotchecked is set, it means we've done this
			 * check once already and don't need to do it again.
			 */
			if (!locked && (ndp->ni_rootdir != rootvnode)) {
				vfs_smr_leave();
				needs_lock = true;
				goto prep_lock_retry;
			} else if (locked && !dotdotchecked && (ndp->ni_rootdir != rootvnode)) {
				vnode_t tvp = dp;
				boolean_t defer = FALSE;
				boolean_t is_subdir = FALSE;

				defer = cache_check_vnode_issubdir(tvp,
				    ndp->ni_rootdir, &is_subdir, &tvp);

				if (defer) {
					/* defer to Filesystem */
					break;
				} else if (!is_subdir) {
					/*
					 * This process is trying to break  out
					 * of its chrooted jail, so all its
					 * dotdot accesses will be translated to
					 * its root directory.
					 */
					vp = ndp->ni_rootdir;
				} else {
					/*
					 * All good, let this dotdot access
					 * proceed normally
					 */
					vp = dp->v_parent;
				}
				dotdotchecked = TRUE;
			} else {
				vp = dp->v_parent;
			}
			if (!vp) {
				break;
			}
			vvid = vp->v_id;
		} else {
			if (!locked) {
				vp = cache_lookup_smr(dp, cnp, &vvid);
				if (!vid_is_same(dp, vid)) {
					vp = NULLVP;
					needs_lock = true;
					vfs_smr_leave();
					goto prep_lock_retry;
				}
			} else {
				vp = cache_lookup_locked(dp, cnp, &vvid);
			}


			if (!vp) {
				break;
			}

			if ((vp->v_flag & VISHARDLINK)) {
				/*
				 * The file system wants a VNOP_LOOKUP on this vnode
				 */
				vp = NULL;
				break;
			}

#if CONFIG_FIRMLINKS
			vnode_t v_fmlink = vp->v_fmlink;
			if (v_fmlink && !(vp->v_flag & VFMLINKTARGET)) {
				if (cnp->cn_flags & CN_FIRMLINK_NOFOLLOW ||
				    ((vp->v_type != VDIR) && (vp->v_type != VLNK))) {
					/* Leave it to the filesystem */
					vp = NULLVP;
					break;
				}

				/*
				 * Always switch to the target unless it is a VLNK
				 * and it is the last component and we have NOFOLLOW
				 * semantics
				 */
				if (vp->v_type == VDIR) {
					vp = v_fmlink;
					vvid = vnode_vid(vp);
				} else if ((cnp->cn_flags & FOLLOW) ||
				    (ndp->ni_flag & NAMEI_TRAILINGSLASH) || *ndp->ni_next == '/') {
					if (ndp->ni_loopcnt >= MAXSYMLINKS - 1) {
						vp = NULLVP;
						break;
					}
					ndp->ni_loopcnt++;
					vp = v_fmlink;
					vvid = vnode_vid(vp);
				}
			}
#endif
		}
		if ((cnp->cn_flags & ISLASTCN)) {
			break;
		}

		if (vp->v_type != VDIR) {
			if (vp->v_type != VLNK) {
				vp = NULL;
			}
			break;
		}

		/*
		 * v_mountedhere is PAC protected which means vp has to be a VDIR
		 * to access that pointer as v_mountedhere. However, if we don't
		 * have the name cache lock or an iocount (which we won't in the
		 * !locked case) we can't guarantee that. So we try to detect it
		 * via other fields to avoid having to dereference v_mountedhere
		 * when we don't need to. Note that in theory if entire reclaim
		 * happens between the time we check can_check_v_mountedhere()
		 * and the subsequent access this will still fail but the fields
		 * we check make that exceedingly unlikely and will result in
		 * the chances of that happening being practically zero (but not
		 * zero).
		 */
		if ((locked || can_check_v_mountedhere(vp)) &&
		    (mp = vp->v_mountedhere) && ((cnp->cn_flags & NOCROSSMOUNT) == 0)) {
			vnode_t tmp_vp;
			int tmp_vid;

			if (!(locked || vid_is_same(vp, vvid))) {
				vp = NULL;
				break;
			}
			tmp_vp = mp->mnt_realrootvp;
			tmp_vid = mp->mnt_realrootvp_vid;
			if (tmp_vp == NULLVP || mp->mnt_generation != mount_generation ||
			    tmp_vid != tmp_vp->v_id) {
				break;
			}

			if ((mp = tmp_vp->v_mount) == NULL) {
				break;
			}

			vp = tmp_vp;
			vvid = tmp_vid;
			dmp = mp;
			if (dmp->mnt_kern_flag & (MNTK_AUTH_OPAQUE | MNTK_AUTH_CACHE_TTL)) {
				ttl_enabled = TRUE;
				microuptime(&tv);
			} else {
				ttl_enabled = FALSE;
			}
		}

#if CONFIG_TRIGGERS
		/*
		 * After traversing all mountpoints stacked here, if we have a
		 * trigger in hand, resolve it.  Note that we don't need to
		 * leave the fast path if the mount has already happened.
		 */
		if (vp->v_resolve) {
			break;
		}
#endif /* CONFIG_TRIGGERS */

		if (!(locked || vid_is_same(vp, vvid))) {
			vp = NULL;
			break;
		}

		dp = vp;
		vid = vvid;
		vp = NULLVP;
		vvid = 0;

		cnp->cn_nameptr = ndp->ni_next + 1;
		ndp->ni_pathlen--;
		while (*cnp->cn_nameptr == '/') {
			cnp->cn_nameptr++;
			ndp->ni_pathlen--;
		}
	}
	if (!locked) {
		if (vp && !vnode_hold_smr(vp)) {
			vp = NULLVP;
			vvid = 0;
		}
		if (!vnode_hold_smr(dp)) {
			vfs_smr_leave();
			if (vp) {
				vnode_drop(vp);
				vp = NULLVP;
				vvid = 0;
			}
			goto prep_lock_retry;
		}
		vfs_smr_leave();
	} else {
		if (vp != NULLVP) {
			vvid = vp->v_id;
			vnode_hold(vp);
		}
		vid = dp->v_id;

		vnode_hold(dp);
		NAME_CACHE_UNLOCK();
	}

	tdp = NULLVP;
	if (!(cnp->cn_flags & DONOTAUTH) &&
	    (vp != NULLVP) && (vp->v_type != VLNK) &&
	    ((cnp->cn_flags & (ISLASTCN | LOCKPARENT | WANTPARENT | SAVESTART)) == ISLASTCN)) {
		/*
		 * if we've got a child and it's the last component, and
		 * the lookup doesn't need to return the parent then we
		 * can skip grabbing an iocount on the parent, since all
		 * we're going to do with it is a vnode_put just before
		 * we return from 'lookup'.  If it's a symbolic link,
		 * we need the parent in case the link happens to be
		 * a relative pathname.
		 *
		 * However, we can't make this optimisation if we have to call
		 * a MAC hook.
		 */
		tdp = dp;
		dp = NULLVP;
	} else {
need_dp:
		/*
		 * return the last directory we looked at
		 * with an io reference held. If it was the one passed
		 * in as a result of the last iteration of VNOP_LOOKUP,
		 * it should already hold an io ref. No need to increase ref.
		 */
		if (last_dp != dp) {
			if (dp == ndp->ni_usedvp) {
				/*
				 * if this vnode matches the one passed in via USEDVP
				 * than this context already holds an io_count... just
				 * use vnode_get to get an extra ref for lookup to play
				 * with... can't use the getwithvid variant here because
				 * it will block behind a vnode_drain which would result
				 * in a deadlock (since we already own an io_count that the
				 * vnode_drain is waiting on)... vnode_get grabs the io_count
				 * immediately w/o waiting... it always succeeds
				 */
				vnode_get(dp);
			} else if ((error = vnode_getwithvid_drainok(dp, vid))) {
				/*
				 * failure indicates the vnode
				 * changed identity or is being
				 * TERMINATED... in either case
				 * punt this lookup.
				 *
				 * don't necessarily return ENOENT, though, because
				 * we really want to go back to disk and make sure it's
				 * there or not if someone else is changing this
				 * vnode. That being said, the one case where we do want
				 * to return ENOENT is when the vnode's mount point is
				 * in the process of unmounting and we might cause a deadlock
				 * in our attempt to take an iocount. An ENODEV error return
				 * is from vnode_get* is an indication this but we change that
				 * ENOENT for upper layers.
				 */
				if (error == ENODEV) {
					error = ENOENT;
				} else {
					error = ERECYCLE;
				}
				vnode_drop(dp);
				if (vp) {
					vnode_drop(vp);
				}
				goto errorout;
			}
			dp_iocount_taken = true;
		}
		vnode_drop(dp);
	}

#if CONFIG_MACF
	/*
	 * Name cache provides authorization caching (see below)
	 * that will short circuit MAC checks in lookup().
	 * We must perform MAC check here.  On denial
	 * dp_authorized will remain 0 and second check will
	 * be perfomed in lookup().
	 */
	if (!(cnp->cn_flags & DONOTAUTH)) {
		error = mac_vnode_check_lookup(ctx, dp, cnp);
		if (error) {
			*dp_authorized = 0;
			if (dp_iocount_taken) {
				vnode_put(dp);
			}
			if (vp) {
				vnode_drop(vp);
				vp = NULLVP;
			}
			goto errorout;
		}
	}
#endif /* MAC */

	if (vp != NULLVP) {
		if ((vnode_getwithvid_drainok(vp, vvid))) {
			vnode_drop(vp);
			vp = NULLVP;

			/*
			 * can't get reference on the vp we'd like
			 * to return... if we didn't grab a reference
			 * on the directory (due to fast path bypass),
			 * then we need to do it now... we can't return
			 * with both ni_dvp and ni_vp NULL, and no
			 * error condition
			 */
			if (dp == NULLVP) {
				dp = tdp;
				tdp = NULLVP;
				goto need_dp;
			}
		} else {
			vnode_drop(vp);
		}
		if (dp_iocount_taken && vp && (vp->v_type != VLNK) &&
		    ((cnp->cn_flags & (ISLASTCN | LOCKPARENT | WANTPARENT | SAVESTART)) == ISLASTCN)) {
			vnode_put(dp);
			dp = NULLVP;
		}
	}

	if (tdp) {
		vnode_drop(tdp);
		tdp = NULLVP;
	}

	ndp->ni_dvp = dp;
	ndp->ni_vp  = vp;

#if CONFIG_TRIGGERS
	trigger_vp = vp ? vp : dp;
	if ((error == 0) && (trigger_vp != NULLVP) && vnode_isdir(trigger_vp)) {
		error = vnode_trigger_resolve(trigger_vp, ndp, ctx);
		if (error) {
			if (vp) {
				vnode_put(vp);
			}
			if (dp) {
				vnode_put(dp);
			}
			goto errorout;
		}
	}
#endif /* CONFIG_TRIGGERS */

errorout:
	/*
	 * If we came into cache_lookup_path after an iteration of the lookup loop that
	 * resulted in a call to VNOP_LOOKUP, then VNOP_LOOKUP returned a vnode with a io ref
	 * on it.  It is now the job of cache_lookup_path to drop the ref on this vnode
	 * when it is no longer needed.  If we get to this point, and last_dp is not NULL
	 * and it is ALSO not the dvp we want to return to caller of this function, it MUST be
	 * the case that we got to a subsequent path component and this previous vnode is
	 * no longer needed.  We can then drop the io ref on it.
	 */
	if ((last_dp != NULLVP) && (last_dp != ndp->ni_dvp)) {
		vnode_put(last_dp);
	}

	//initialized to 0, should be the same if no error cases occurred.
	return error;

prep_lock_retry:
	restore_ndp_state(ndp, cnp, &saved_state);
	dp = start_dp;
	goto retry;
}


static vnode_t
cache_lookup_locked(vnode_t dvp, struct componentname *cnp, uint32_t *vidp)
{
	struct namecache *ncp;
	long namelen = cnp->cn_namelen;
	unsigned int hashval = cnp->cn_hash;

	if (nc_disabled) {
		return NULL;
	}

	smrq_serialized_foreach(ncp, NCHHASH(dvp, cnp->cn_hash), nc_hash) {
		if ((ncp->nc_dvp == dvp) && (ncp->nc_hashval == hashval)) {
			if (strncmp(ncp->nc_name, cnp->cn_nameptr, namelen) == 0 && ncp->nc_name[namelen] == 0) {
				break;
			}
		}
	}
	if (ncp == 0) {
		/*
		 * We failed to find an entry
		 */
		NCHSTAT(ncs_miss);
		NC_SMR_STATS(clp_next_fail);
		return NULL;
	}
	NCHSTAT(ncs_goodhits);

	if (!ncp->nc_vp) {
		return NULL;
	}

	*vidp = ncp->nc_vid;
	NC_SMR_STATS(clp_next);

	return ncp->nc_vp;
}

static vnode_t
cache_lookup_smr(vnode_t dvp, struct componentname *cnp, uint32_t *vidp)
{
	struct namecache *ncp;
	long namelen = cnp->cn_namelen;
	unsigned int hashval = cnp->cn_hash;
	vnode_t vp = NULLVP;
	uint32_t vid = 0;
	uint32_t counter = 1;

	if (nc_disabled) {
		return NULL;
	}

	smrq_entered_foreach(ncp, NCHHASH(dvp, cnp->cn_hash), nc_hash) {
		counter = os_atomic_load(&ncp->nc_counter, acquire);
		if (!(counter & NC_VALID)) {
			ncp = NULL;
			goto out;
		}
		if ((ncp->nc_dvp == dvp) && (ncp->nc_hashval == hashval)) {
			const char *nc_name =
			    os_atomic_load(&ncp->nc_name, relaxed);
			if (nc_name &&
			    strncmp(nc_name, cnp->cn_nameptr, namelen) == 0 &&
			    nc_name[namelen] == 0) {
				break;
			} else if (!nc_name) {
				ncp = NULL;
				goto out;
			}
		}
	}

	/* We failed to find an entry */
	if (ncp == 0) {
		goto out;
	}

	vp = ncp->nc_vp;
	vid = ncp->nc_vid;

	/*
	 * The validity of vp and vid depends on the value of the counter being
	 * the same when we read it first in the loop and now. Anything else
	 * and we can't use this vp & vid.
	 * Hopefully this ncp wasn't reused 2 billion times between the time
	 * we read it first and when we the counter value again.
	 */
	if (os_atomic_load(&ncp->nc_counter, acquire) != counter) {
		vp = NULLVP;
		goto out;
	}

	*vidp = vid;
	NC_SMR_STATS(clp_smr_next);

	return vp;

out:
	NC_SMR_STATS(clp_smr_next_fail);
	return NULL;
}


unsigned int hash_string(const char *cp, int len);
//
// Have to take a len argument because we may only need to
// hash part of a componentname.
//
unsigned int
hash_string(const char *cp, int len)
{
	unsigned hash = 0;

	if (len) {
		while (len--) {
			hash = crc32tab[((hash >> 24) ^ (unsigned char)*cp++)] ^ hash << 8;
		}
	} else {
		while (*cp != '\0') {
			hash = crc32tab[((hash >> 24) ^ (unsigned char)*cp++)] ^ hash << 8;
		}
	}
	/*
	 * the crc generator can legitimately generate
	 * a 0... however, 0 for us means that we
	 * haven't computed a hash, so use 1 instead
	 */
	if (hash == 0) {
		hash = 1;
	}
	return hash;
}


/*
 * Lookup an entry in the cache
 *
 * We don't do this if the segment name is long, simply so the cache
 * can avoid holding long names (which would either waste space, or
 * add greatly to the complexity).
 *
 * Lookup is called with dvp pointing to the directory to search,
 * cnp pointing to the name of the entry being sought. If the lookup
 * succeeds, the vnode is returned in *vpp, and a status of -1 is
 * returned. If the lookup determines that the name does not exist
 * (negative cacheing), a status of ENOENT is returned. If the lookup
 * fails, a status of zero is returned.
 */

static int
cache_lookup_fallback(struct vnode *dvp, struct vnode **vpp,
    struct componentname *cnp, int flags)
{
	struct namecache *ncp;
	long namelen = cnp->cn_namelen;
	unsigned int hashval = cnp->cn_hash;
	boolean_t       have_exclusive = FALSE;
	uint32_t vid;
	vnode_t  vp;

	NAME_CACHE_LOCK_SHARED();

relook:
	smrq_serialized_foreach(ncp, NCHHASH(dvp, cnp->cn_hash), nc_hash) {
		if ((ncp->nc_dvp == dvp) && (ncp->nc_hashval == hashval)) {
			if (strncmp(ncp->nc_name, cnp->cn_nameptr, namelen) == 0 && ncp->nc_name[namelen] == 0) {
				break;
			}
		}
	}
	/* We failed to find an entry */
	if (ncp == 0) {
		NCHSTAT(ncs_miss);
		NAME_CACHE_UNLOCK();
		return 0;
	}

	/* We don't want to have an entry, so dump it */
	if ((cnp->cn_flags & MAKEENTRY) == 0) {
		if (have_exclusive == TRUE) {
			NCHSTAT(ncs_badhits);
			cache_delete(ncp, 1);
			NAME_CACHE_UNLOCK();
			return 0;
		}
		if (!NAME_CACHE_LOCK_SHARED_TO_EXCLUSIVE()) {
			NAME_CACHE_LOCK();
		}
		have_exclusive = TRUE;
		goto relook;
	}
	vp = ncp->nc_vp;

	/* We found a "positive" match, return the vnode */
	if (vp) {
		NCHSTAT(ncs_goodhits);

		vid = ncp->nc_vid;
		vnode_hold(vp);
		NAME_CACHE_UNLOCK();

		if (vnode_getwithvid(vp, vid)) {
			vnode_drop(vp);
#if COLLECT_STATS
			NAME_CACHE_LOCK();
			NCHSTAT(ncs_badvid);
			NAME_CACHE_UNLOCK();
#endif
			return 0;
		}
		vnode_drop(vp);
		*vpp = vp;
		NC_SMR_STATS(cl_lock_hits);
		return -1;
	}

	/* We found a negative match, and want to create it, so purge */
	if (cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME) {
		if (have_exclusive == TRUE) {
			NCHSTAT(ncs_badhits);
			cache_delete(ncp, 1);
			NAME_CACHE_UNLOCK();
			/*
			 * Even though we're purging the entry, it
			 * may be useful to the caller to know that
			 * we got a neg hit (to, for example, avoid
			 * an expensive IPC/RPC).
			 */
			return (flags & CACHE_LOOKUP_ALLHITS) ? ENOENT : 0;
		}
		if (!NAME_CACHE_LOCK_SHARED_TO_EXCLUSIVE()) {
			NAME_CACHE_LOCK();
		}
		have_exclusive = TRUE;
		goto relook;
	}

	/*
	 * We found a "negative" match, ENOENT notifies client of this match.
	 */
	NCHSTAT(ncs_neghits);

	NAME_CACHE_UNLOCK();
	return ENOENT;
}



/*
 * Lookup an entry in the cache
 *
 * Lookup is called with dvp pointing to the directory to search,
 * cnp pointing to the name of the entry being sought. If the lookup
 * succeeds, the vnode is returned in *vpp, and a status of -1 is
 * returned. If the lookup determines that the name does not exist
 * (negative cacheing), a status of ENOENT is returned. If the lookup
 * fails, a status of zero is returned.
 */
int
cache_lookup_ext(struct vnode *dvp, struct vnode **vpp,
    struct componentname *cnp, int flags)
{
	struct namecache *ncp;
	long namelen = cnp->cn_namelen;
	vnode_t  vp;
	uint32_t vid = 0;
	uint32_t counter = 1;
	unsigned int hashval;

	*vpp = NULLVP;

	if (cnp->cn_hash == 0) {
		cnp->cn_hash = hash_string(cnp->cn_nameptr, cnp->cn_namelen);
	}
	hashval = cnp->cn_hash;

	if (nc_disabled) {
		return 0;
	}

	if (!nc_smr_enabled) {
		goto out_fallback;
	}

	/* We don't want to have an entry, so dump it */
	if ((cnp->cn_flags & MAKEENTRY) == 0) {
		goto out_fallback;
	}

	vfs_smr_enter();

	smrq_entered_foreach(ncp, NCHHASH(dvp, cnp->cn_hash), nc_hash) {
		counter = os_atomic_load(&ncp->nc_counter, acquire);
		if (!(counter & NC_VALID)) {
			vfs_smr_leave();
			goto out_fallback;
		}
		if ((ncp->nc_dvp == dvp) && (ncp->nc_hashval == hashval)) {
			const char *nc_name =
			    os_atomic_load(&ncp->nc_name, relaxed);
			if (nc_name &&
			    strncmp(nc_name, cnp->cn_nameptr, namelen) == 0 &&
			    nc_name[namelen] == 0) {
				break;
			} else if (!nc_name) {
				vfs_smr_leave();
				goto out_fallback;
			}
		}
	}

	/* We failed to find an entry */
	if (ncp == 0) {
		NCHSTAT(ncs_miss);
		vfs_smr_leave();
		NC_SMR_STATS(cl_smr_miss);
		return 0;
	}

	vp = ncp->nc_vp;
	vid = ncp->nc_vid;

	/*
	 * The validity of vp and vid depends on the value of the counter being
	 * the same when we read it first in the loop and now. Anything else
	 * and we can't use this vp & vid.
	 * Hopefully this ncp wasn't reused 2 billion times between the time
	 * we read it first and when we the counter value again.
	 */
	if (os_atomic_load(&ncp->nc_counter, acquire) != counter) {
		vfs_smr_leave();
		goto out_fallback;
	}

	if (vp) {
		bool holdcount_acquired = vnode_hold_smr(vp);

		vfs_smr_leave();

		if (!holdcount_acquired) {
			goto out_fallback;
		}

		if (vnode_getwithvid(vp, vid) != 0) {
			vnode_drop(vp);
			goto out_fallback;
		}
		vnode_drop(vp);
		NCHSTAT(ncs_goodhits);

		*vpp = vp;
		NC_SMR_STATS(cl_smr_hits);
		return -1;
	}

	vfs_smr_leave();

	/* We found a negative match, and want to create it, so purge */
	if (cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME) {
		goto out_fallback;
	}

	/*
	 * We found a "negative" match, ENOENT notifies client of this match.
	 */
	NCHSTAT(ncs_neghits);
	NC_SMR_STATS(cl_smr_negative_hits);
	return ENOENT;

out_fallback:
	NC_SMR_STATS(cl_smr_fallback);
	return cache_lookup_fallback(dvp, vpp, cnp, flags);
}

int
cache_lookup(struct vnode *dvp, struct vnode **vpp, struct componentname *cnp)
{
	return cache_lookup_ext(dvp, vpp, cnp, 0);
}

const char *
cache_enter_create(vnode_t dvp, vnode_t vp, struct componentname *cnp)
{
	const char *strname;

	if (cnp->cn_hash == 0) {
		cnp->cn_hash = hash_string(cnp->cn_nameptr, cnp->cn_namelen);
	}

	/*
	 * grab 2 references on the string entered
	 * one for the cache_enter_locked to consume
	 * and the second to be consumed by v_name (vnode_create call point)
	 */
	strname = add_name_internal(cnp->cn_nameptr, cnp->cn_namelen, cnp->cn_hash, TRUE, 0);

	NAME_CACHE_LOCK();

	cache_enter_locked(dvp, vp, cnp, strname);

	NAME_CACHE_UNLOCK();

	return strname;
}


/*
 * Add an entry to the cache...
 * but first check to see if the directory
 * that this entry is to be associated with has
 * had any cache_purges applied since we took
 * our identity snapshot... this check needs to
 * be done behind the name cache lock
 */
void
cache_enter_with_gen(struct vnode *dvp, struct vnode *vp, struct componentname *cnp, int gen)
{
	if (cnp->cn_hash == 0) {
		cnp->cn_hash = hash_string(cnp->cn_nameptr, cnp->cn_namelen);
	}

	NAME_CACHE_LOCK();

	if (dvp->v_nc_generation == gen) {
		(void)cache_enter_locked(dvp, vp, cnp, NULL);
	}

	NAME_CACHE_UNLOCK();
}


/*
 * Add an entry to the cache.
 */
void
cache_enter(struct vnode *dvp, struct vnode *vp, struct componentname *cnp)
{
	const char *strname;

	if (cnp->cn_hash == 0) {
		cnp->cn_hash = hash_string(cnp->cn_nameptr, cnp->cn_namelen);
	}

	/*
	 * grab 1 reference on the string entered
	 * for the cache_enter_locked to consume
	 */
	strname = add_name_internal(cnp->cn_nameptr, cnp->cn_namelen, cnp->cn_hash, FALSE, 0);

	NAME_CACHE_LOCK();

	cache_enter_locked(dvp, vp, cnp, strname);

	NAME_CACHE_UNLOCK();
}


static void
cache_enter_locked(struct vnode *dvp, struct vnode *vp, struct componentname *cnp, const char *strname)
{
	struct namecache *ncp, *negp;
	struct smrq_list_head  *ncpp;

	if (nc_disabled) {
		return;
	}

	/*
	 * if the entry is for -ve caching vp is null
	 */
	if ((vp != NULLVP) && (LIST_FIRST(&vp->v_nclinks))) {
		/*
		 * someone beat us to the punch..
		 * this vnode is already in the cache
		 */
		if (strname != NULL) {
			vfs_removename(strname);
		}
		return;
	}
	/*
	 * We allocate a new entry if we are less than the maximum
	 * allowed and the one at the front of the list is in use.
	 * Otherwise we use the one at the front of the list.
	 */
	if (numcache < desiredNodes &&
	    ((ncp = nchead.tqh_first) == NULL ||
	    (ncp->nc_counter & NC_VALID))) {
		/*
		 * Allocate one more entry
		 */
		if (nc_smr_enabled) {
			ncp = zalloc_smr(namecache_zone, Z_WAITOK_ZERO_NOFAIL);
		} else {
			ncp = zalloc(namecache_zone);
		}
		ncp->nc_counter = 0;
		numcache++;
	} else {
		/*
		 * reuse an old entry
		 */
		ncp = TAILQ_FIRST(&nchead);
		TAILQ_REMOVE(&nchead, ncp, nc_entry);

		if (ncp->nc_counter & NC_VALID) {
			/*
			 * still in use... we need to
			 * delete it before re-using it
			 */
			NCHSTAT(ncs_stolen);
			cache_delete(ncp, 0);
		}
	}
	NCHSTAT(ncs_enters);

	/*
	 * Fill in cache info, if vp is NULL this is a "negative" cache entry.
	 */
	if (vp) {
		ncp->nc_vid = vnode_vid(vp);
		vnode_hold(vp);
	}
	ncp->nc_vp = vp;
	ncp->nc_dvp = dvp;
	ncp->nc_hashval = cnp->cn_hash;

	if (strname == NULL) {
		ncp->nc_name = add_name_internal(cnp->cn_nameptr, cnp->cn_namelen, cnp->cn_hash, FALSE, 0);
	} else {
		ncp->nc_name = strname;
	}

	//
	// If the bytes of the name associated with the vnode differ,
	// use the name associated with the vnode since the file system
	// may have set that explicitly in the case of a lookup on a
	// case-insensitive file system where the case of the looked up
	// name differs from what is on disk.  For more details, see:
	//   <rdar://problem/8044697> FSEvents doesn't always decompose diacritical unicode chars in the paths of the changed directories
	//
	const char *vn_name = vp ? vp->v_name : NULL;
	unsigned int len = vn_name ? (unsigned int)strlen(vn_name) : 0;
	if (vn_name && ncp && ncp->nc_name && strncmp(ncp->nc_name, vn_name, len) != 0) {
		unsigned int hash = hash_string(vn_name, len);

		vfs_removename(ncp->nc_name);
		ncp->nc_name = add_name_internal(vn_name, len, hash, FALSE, 0);
		ncp->nc_hashval = hash;
	}

	/*
	 * make us the newest entry in the cache
	 * i.e. we'll be the last to be stolen
	 */
	TAILQ_INSERT_TAIL(&nchead, ncp, nc_entry);

	ncpp = NCHHASH(dvp, cnp->cn_hash);
#if DIAGNOSTIC
	{
		struct namecache *p;

		smrq_serialized_foreach(p, ncpp, nc_hash) {
			if (p == ncp) {
				panic("cache_enter: duplicate");
			}
		}
	}
#endif
	/*
	 * make us available to be found via lookup
	 */
	smrq_serialized_insert_head(ncpp, &ncp->nc_hash);

	if (vp) {
		/*
		 * add to the list of name cache entries
		 * that point at vp
		 */
		LIST_INSERT_HEAD(&vp->v_nclinks, ncp, nc_un.nc_link);
	} else {
		/*
		 * this is a negative cache entry (vp == NULL)
		 * stick it on the negative cache list.
		 */
		TAILQ_INSERT_TAIL(&neghead, ncp, nc_un.nc_negentry);

		ncs_negtotal++;

		if (ncs_negtotal > desiredNegNodes) {
			/*
			 * if we've reached our desired limit
			 * of negative cache entries, delete
			 * the oldest
			 */
			negp = TAILQ_FIRST(&neghead);
			cache_delete(negp, 1);
		}
	}

	/*
	 * add us to the list of name cache entries that
	 * are children of dvp
	 */
	if (vp) {
		TAILQ_INSERT_TAIL(&dvp->v_ncchildren, ncp, nc_child);
	} else {
		TAILQ_INSERT_HEAD(&dvp->v_ncchildren, ncp, nc_child);
	}

	/*
	 * nc_counter represents a sequence counter and 1 bit valid flag.
	 * When the counter value is odd, it represents a valid and in use
	 * namecache structure. We increment the value on every state transition
	 * (invalid to valid (here) and valid to invalid (in cache delete).
	 * Lockless readers have to read the value before reading other fields
	 * and ensure that the field is valid and remains the same after the fields
	 * have been read.
	 */
	uint32_t old_count = os_atomic_inc_orig(&ncp->nc_counter, release);
	if (old_count & NC_VALID) {
		/* This is a invalid to valid transition */
		panic("Incorrect state for old nc_counter(%d), should be even", old_count);
	}
}


/*
 * Initialize CRC-32 remainder table.
 */
static void
init_crc32(void)
{
	/*
	 * the CRC-32 generator polynomial is:
	 *   x^32 + x^26 + x^23 + x^22 + x^16 + x^12 + x^10
	 *        + x^8  + x^7  + x^5  + x^4  + x^2  + x + 1
	 */
	unsigned int crc32_polynomial = 0x04c11db7;
	unsigned int i, j;

	/*
	 * pre-calculate the CRC-32 remainder for each possible octet encoding
	 */
	for (i = 0; i < 256; i++) {
		unsigned int crc_rem = i << 24;

		for (j = 0; j < 8; j++) {
			if (crc_rem & 0x80000000) {
				crc_rem = (crc_rem << 1) ^ crc32_polynomial;
			} else {
				crc_rem = (crc_rem << 1);
			}
		}
		crc32tab[i] = crc_rem;
	}
}


/*
 * Name cache initialization, from vfs_init() when we are booting
 */
void
nchinit(void)
{
	desiredNegNodes = (desiredvnodes / 10);
	desiredNodes = desiredvnodes + desiredNegNodes;

	if (nc_smr_enabled) {
		zone_enable_smr(namecache_zone, VFS_SMR(), &namecache_smr_free);
		zone_enable_smr(stringcache_zone, VFS_SMR(), &string_smr_free);
	}
	TAILQ_INIT(&nchead);
	TAILQ_INIT(&neghead);

	init_crc32();

	nchashtbl = hashinit(MAX(CONFIG_NC_HASH, (2 * desiredNodes)), M_CACHE, &nchash);
	nchashmask = nchash;
	nchash++;

	init_string_table();

	for (int i = 0; i < NUM_STRCACHE_LOCKS; i++) {
		lck_mtx_init(&strcache_mtx_locks[i], &strcache_lck_grp, &strcache_lck_attr);
	}
}

void
name_cache_lock_shared(void)
{
	lck_rw_lock_shared(&namecache_rw_lock);
	NC_SMR_STATS(nc_lock_shared);
}

void
name_cache_lock(void)
{
	lck_rw_lock_exclusive(&namecache_rw_lock);
	NC_SMR_STATS(nc_lock);
}

boolean_t
name_cache_lock_shared_to_exclusive(void)
{
	return lck_rw_lock_shared_to_exclusive(&namecache_rw_lock);
}

void
name_cache_unlock(void)
{
	lck_rw_done(&namecache_rw_lock);
}


int
resize_namecache(int newsize)
{
	struct smrq_list_head   *new_table;
	struct smrq_list_head   *old_table;
	struct smrq_list_head   *old_head;
	struct namecache    *entry;
	uint32_t            i, hashval;
	int                 dNodes, dNegNodes, nelements;
	u_long              new_size, old_size;

	if (newsize < 0) {
		return EINVAL;
	}

	dNegNodes = (newsize / 10);
	dNodes = newsize + dNegNodes;
	// we don't support shrinking yet
	if (dNodes <= desiredNodes) {
		return 0;
	}

	if (os_mul_overflow(dNodes, 2, &nelements)) {
		return EINVAL;
	}

	new_table = hashinit(nelements, M_CACHE, &nchashmask);
	new_size  = nchashmask + 1;

	if (new_table == NULL) {
		return ENOMEM;
	}

	NAME_CACHE_LOCK();

	/* No need to switch if the hash table size hasn't changed. */
	if (new_size == nchash) {
		NAME_CACHE_UNLOCK();
		hashdestroy(new_table, M_CACHE, new_size - 1);
		return 0;
	}

	// do the switch!
	old_table = nchashtbl;
	nchashtbl = new_table;
	old_size  = nchash;
	nchash    = new_size;

	// walk the old table and insert all the entries into
	// the new table
	//
	for (i = 0; i < old_size; i++) {
		old_head = &old_table[i];
		smrq_serialized_foreach_safe(entry, old_head, nc_hash) {
			//
			// XXXdbg - Beware: this assumes that hash_string() does
			//                  the same thing as what happens in
			//                  lookup() over in vfs_lookup.c
			hashval = hash_string(entry->nc_name, 0);
			entry->nc_hashval = hashval;

			smrq_serialized_insert_head(NCHHASH(entry->nc_dvp, hashval), &entry->nc_hash);
		}
	}
	desiredNodes = dNodes;
	desiredNegNodes = dNegNodes;

	NAME_CACHE_UNLOCK();
	hashdestroy(old_table, M_CACHE, old_size - 1);

	return 0;
}

static void
namecache_smr_free(void *_ncp, __unused size_t _size)
{
	struct namecache *ncp = _ncp;

	bzero(ncp, sizeof(*ncp));
}

static void
cache_delete(struct namecache *ncp, int free_entry)
{
	NCHSTAT(ncs_deletes);

	/*
	 * See comment at the end of cache_enter_locked expalining the usage of
	 * nc_counter.
	 */
	uint32_t old_count = os_atomic_inc_orig(&ncp->nc_counter, release);
	if (!(old_count & NC_VALID)) {
		/* This should be a valid to invalid transition */
		panic("Incorrect state for old nc_counter(%d), should be odd", old_count);
	}

	if (ncp->nc_vp) {
		LIST_REMOVE(ncp, nc_un.nc_link);
	} else {
		TAILQ_REMOVE(&neghead, ncp, nc_un.nc_negentry);
		ncs_negtotal--;
	}
	TAILQ_REMOVE(&(ncp->nc_dvp->v_ncchildren), ncp, nc_child);

	smrq_serialized_remove((NCHHASH(ncp->nc_dvp, ncp->nc_hashval)), &ncp->nc_hash);

	const char *nc_name = ncp->nc_name;
	ncp->nc_name = NULL;
	vfs_removename(nc_name);
	if (ncp->nc_vp) {
		vnode_t vp = ncp->nc_vp;

		ncp->nc_vp = NULLVP;
		vnode_drop(vp);
	}

	if (free_entry) {
		TAILQ_REMOVE(&nchead, ncp, nc_entry);
		if (nc_smr_enabled) {
			zfree_smr(namecache_zone, ncp);
		} else {
			zfree(namecache_zone, ncp);
		}
		numcache--;
	}
}


/*
 * purge the entry associated with the
 * specified vnode from the name cache
 */
static void
cache_purge_locked(vnode_t vp, kauth_cred_t *credp)
{
	struct namecache *ncp;

	*credp = NULL;
	if ((LIST_FIRST(&vp->v_nclinks) == NULL) &&
	    (TAILQ_FIRST(&vp->v_ncchildren) == NULL) &&
	    (vnode_cred(vp) == NOCRED) &&
	    (vp->v_parent == NULLVP)) {
		return;
	}

	if (vp->v_parent) {
		vp->v_parent->v_nc_generation++;
	}

	while ((ncp = LIST_FIRST(&vp->v_nclinks))) {
		cache_delete(ncp, 1);
	}

	while ((ncp = TAILQ_FIRST(&vp->v_ncchildren))) {
		cache_delete(ncp, 1);
	}

	/*
	 * Use a temp variable to avoid kauth_cred_unref() while NAME_CACHE_LOCK is held
	 */
	*credp = vnode_cred(vp);
	vp->v_cred = NOCRED;
	vp->v_authorized_actions = 0;
}

void
cache_purge(vnode_t vp)
{
	kauth_cred_t tcred = NULL;

	if ((LIST_FIRST(&vp->v_nclinks) == NULL) &&
	    (TAILQ_FIRST(&vp->v_ncchildren) == NULL) &&
	    (vnode_cred(vp) == NOCRED) &&
	    (vp->v_parent == NULLVP)) {
		return;
	}

	NAME_CACHE_LOCK();

	cache_purge_locked(vp, &tcred);

	NAME_CACHE_UNLOCK();

	if (IS_VALID_CRED(tcred)) {
		kauth_cred_unref(&tcred);
	}
}

/*
 * Purge all negative cache entries that are children of the
 * given vnode.  A case-insensitive file system (or any file
 * system that has multiple equivalent names for the same
 * directory entry) can use this when creating or renaming
 * to remove negative entries that may no longer apply.
 */
void
cache_purge_negatives(vnode_t vp)
{
	struct namecache *ncp, *next_ncp;

	NAME_CACHE_LOCK();

	TAILQ_FOREACH_SAFE(ncp, &vp->v_ncchildren, nc_child, next_ncp) {
		if (ncp->nc_vp) {
			break;
		}

		cache_delete(ncp, 1);
	}

	NAME_CACHE_UNLOCK();
}

/*
 * Flush all entries referencing a particular filesystem.
 *
 * Since we need to check it anyway, we will flush all the invalid
 * entries at the same time.
 */
void
cache_purgevfs(struct mount *mp)
{
	struct smrq_list_head *ncpp;
	struct namecache *ncp;

	NAME_CACHE_LOCK();
	/* Scan hash tables for applicable entries */
	for (ncpp = &nchashtbl[nchash - 1]; ncpp >= nchashtbl; ncpp--) {
restart:
		smrq_serialized_foreach(ncp, ncpp, nc_hash) {
			if (ncp->nc_dvp->v_mount == mp) {
				cache_delete(ncp, 0);
				goto restart;
			}
		}
	}
	NAME_CACHE_UNLOCK();
}



//
// String ref routines
//
static LIST_HEAD(stringhead, string_t) * string_ref_table;
static u_long   string_table_mask;
static uint32_t filled_buckets = 0;




static void
resize_string_ref_table(void)
{
	struct stringhead *new_table;
	struct stringhead *old_table;
	struct stringhead *old_head, *head;
	string_t          *entry, *next;
	uint32_t           i, hashval;
	u_long             new_mask, old_mask;

	/*
	 * need to hold the table lock exclusively
	 * in order to grow the table... need to recheck
	 * the need to resize again after we've taken
	 * the lock exclusively in case some other thread
	 * beat us to the punch
	 */
	lck_rw_lock_exclusive(&strtable_rw_lock);

	if (4 * filled_buckets < ((string_table_mask + 1) * 3)) {
		lck_rw_done(&strtable_rw_lock);
		return;
	}
	assert(string_table_mask < INT32_MAX);
	new_table = hashinit((int)(string_table_mask + 1) * 2, M_CACHE, &new_mask);

	if (new_table == NULL) {
		printf("failed to resize the hash table.\n");
		lck_rw_done(&strtable_rw_lock);
		return;
	}

	// do the switch!
	old_table         = string_ref_table;
	string_ref_table  = new_table;
	old_mask          = string_table_mask;
	string_table_mask = new_mask;
	filled_buckets    = 0;

	// walk the old table and insert all the entries into
	// the new table
	//
	for (i = 0; i <= old_mask; i++) {
		old_head = &old_table[i];
		for (entry = old_head->lh_first; entry != NULL; entry = next) {
			hashval = hash_string((const char *)entry->str, 0);
			head = &string_ref_table[hashval & string_table_mask];
			if (head->lh_first == NULL) {
				filled_buckets++;
			}
			next = entry->hash_chain.le_next;
			LIST_INSERT_HEAD(head, entry, hash_chain);
		}
	}
	lck_rw_done(&strtable_rw_lock);

	hashdestroy(old_table, M_CACHE, old_mask);
}


static void
init_string_table(void)
{
	string_ref_table = hashinit(CONFIG_VFS_NAMES, M_CACHE, &string_table_mask);
}


const char *
vfs_addname(const char *name, uint32_t len, u_int hashval, u_int flags)
{
	return add_name_internal(name, len, hashval, FALSE, flags);
}


static const char *
add_name_internal(const char *name, uint32_t len, u_int hashval, boolean_t need_extra_ref, __unused u_int flags)
{
	struct stringhead *head;
	string_t          *entry;
	uint32_t          chain_len = 0;
	uint32_t          hash_index;
	uint32_t          lock_index;
	char              *ptr;

	if (len > MAXPATHLEN) {
		len = MAXPATHLEN;
	}

	/*
	 * if the length already accounts for the null-byte, then
	 * subtract one so later on we don't index past the end
	 * of the string.
	 */
	if (len > 0 && name[len - 1] == '\0') {
		len--;
	}
	if (hashval == 0) {
		hashval = hash_string(name, len);
	}

	/*
	 * take this lock 'shared' to keep the hash stable
	 * if someone else decides to grow the pool they
	 * will take this lock exclusively
	 */
	lck_rw_lock_shared(&strtable_rw_lock);

	/*
	 * If the table gets more than 3/4 full, resize it
	 */
	if (4 * filled_buckets >= ((string_table_mask + 1) * 3)) {
		lck_rw_done(&strtable_rw_lock);

		resize_string_ref_table();

		lck_rw_lock_shared(&strtable_rw_lock);
	}
	hash_index = hashval & string_table_mask;
	lock_index = hash_index % NUM_STRCACHE_LOCKS;

	head = &string_ref_table[hash_index];

	lck_mtx_lock_spin(&strcache_mtx_locks[lock_index]);

	for (entry = head->lh_first; entry != NULL; chain_len++, entry = entry->hash_chain.le_next) {
		if (strncmp(entry->str, name, len) == 0 && entry->str[len] == 0) {
			entry->refcount++;
			break;
		}
	}
	if (entry == NULL) {
		const uint32_t buflen = len + 1;

		lck_mtx_convert_spin(&strcache_mtx_locks[lock_index]);
		/*
		 * it wasn't already there so add it.
		 */
		if (nc_smr_enabled) {
			entry = zalloc_smr(stringcache_zone, Z_WAITOK_ZERO_NOFAIL);
		} else {
			entry = zalloc(stringcache_zone);
		}

		if (head->lh_first == NULL) {
			OSAddAtomic(1, &filled_buckets);
		}
		ptr = kalloc_data(buflen, Z_WAITOK);
		strncpy(ptr, name, len);
		ptr[len] = '\0';
		entry->str = ptr;
		entry->strbuflen = buflen;
		entry->refcount = 1;
		LIST_INSERT_HEAD(head, entry, hash_chain);
	}
	if (need_extra_ref == TRUE) {
		entry->refcount++;
	}

	lck_mtx_unlock(&strcache_mtx_locks[lock_index]);
	lck_rw_done(&strtable_rw_lock);

	return (const char *)entry->str;
}

static void
string_smr_free(void *_entry, __unused size_t size)
{
	string_t *entry = _entry;

	kfree_data(entry->str, entry->strbuflen);
	bzero(entry, sizeof(*entry));
}

int
vfs_removename(const char *nameref)
{
	struct stringhead *head;
	string_t          *entry;
	uint32_t           hashval;
	uint32_t           hash_index;
	uint32_t           lock_index;
	int                retval = ENOENT;

	hashval = hash_string(nameref, 0);

	/*
	 * take this lock 'shared' to keep the hash stable
	 * if someone else decides to grow the pool they
	 * will take this lock exclusively
	 */
	lck_rw_lock_shared(&strtable_rw_lock);
	/*
	 * must compute the head behind the table lock
	 * since the size and location of the table
	 * can change on the fly
	 */
	hash_index = hashval & string_table_mask;
	lock_index = hash_index % NUM_STRCACHE_LOCKS;

	head = &string_ref_table[hash_index];

	lck_mtx_lock_spin(&strcache_mtx_locks[lock_index]);

	for (entry = head->lh_first; entry != NULL; entry = entry->hash_chain.le_next) {
		if (entry->str == nameref) {
			entry->refcount--;

			if (entry->refcount == 0) {
				LIST_REMOVE(entry, hash_chain);

				if (head->lh_first == NULL) {
					OSAddAtomic(-1, &filled_buckets);
				}
			} else {
				entry = NULL;
			}
			retval = 0;
			break;
		}
	}
	lck_mtx_unlock(&strcache_mtx_locks[lock_index]);
	lck_rw_done(&strtable_rw_lock);

	if (entry) {
		assert(entry->refcount == 0);
		if (nc_smr_enabled) {
			zfree_smr(stringcache_zone, entry);
		} else {
			kfree_data(entry->str, entry->strbuflen);
			entry->str = NULL;
			entry->strbuflen = 0;
			zfree(stringcache_zone, entry);
		}
	}

	return retval;
}


#ifdef DUMP_STRING_TABLE
void
dump_string_table(void)
{
	struct stringhead *head;
	string_t          *entry;
	u_long            i;

	lck_rw_lock_shared(&strtable_rw_lock);

	for (i = 0; i <= string_table_mask; i++) {
		head = &string_ref_table[i];
		for (entry = head->lh_first; entry != NULL; entry = entry->hash_chain.le_next) {
			printf("%6d - %s\n", entry->refcount, entry->str);
		}
	}
	lck_rw_done(&strtable_rw_lock);
}
#endif  /* DUMP_STRING_TABLE */
