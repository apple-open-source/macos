//
//  ntfs_xpl.c
//  NTFSUVFSPlugIn
//
//  Created by Erik Larsson on 2020-01-01.
//  Copyright © 2020 Tuxera Inc. All rights reserved.
//

#include "ntfs_xpl.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/buf.h>
#include <sys/disk.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

struct mount {
	uint64_t flags;
	int64_t mount_time;
	int isrdonly;
	struct vnode devvn;
	uint32_t blocksize;
	uint32_t blocksize_mask;
	void *fsprivate;
	void *hostprivate;
	struct vfsstatfs stfs;
	int typenum;
	struct vfsioattr ioattr;
};

#ifdef XPL_MEMORY_DEBUG
OSMallocTag xpl_malloc_tag = ((void*)2);
size_t xpl_allocated = 0;
size_t xpl_allocations = 0;

void* _OSMalloc(const char *file, int line, size_t size, OSMallocTag tag)
{
	void *res;

	(void) tag;

	res = malloc(size);
	if(res) {
		size_t pre;
		pre = __sync_fetch_and_add(&xpl_allocated, size);
		xpl_info("[%s:%d] allocated size +%zu: %zu -> %zu",
			file, line, size, pre, pre + size);
		pre = __sync_fetch_and_add(&xpl_allocations, 1);
		xpl_info("[%s:%d] allocations +1: %zu -> %zu",
			file, line, pre, pre + 1);
	}

	return res;
}

void _OSFree(const char *file, int line, void *ptr, size_t size,
	     OSMallocTag tag)
{
	size_t pre;

	(void) tag;

	free(ptr);
	pre = __sync_fetch_and_sub(&xpl_allocated, size);
	xpl_info("[%s:%d] allocated size -%zu: %zu -> %zu",
		file, line, size, pre, pre - size);
	pre = __sync_fetch_and_sub(&xpl_allocations, 1);
	xpl_info("[%s:%d] allocations -1: %zu -> %zu",
		file, line, pre, pre - 1);
}
#endif

extern struct vnodeopv_entry_desc ntfs_vnodeop_entries[];

struct buf {
	daddr64_t lblkno;
	vnode_t vnode;
	void (*filter)(buf_t, void *);
	void *transaction;
	size_t allocated_size;
	uint32_t size;  /* Allocated buffer size. */
	uint32_t count; /* Valid bytes in buffer. */
	int32_t flags;
	errno_t err;
	char buf[0];
};

struct uio {
	off_t offset;
	user_ssize_t resid;
	user_addr_t iov_baseaddr;
	user_size_t iov_length;
};

struct upl_page_info {
	int valid : 1;
	int dumped : 1;
};

struct xpl_upl {
	buf_t buf;
	//char *buffer;
	size_t pages;
	struct {
		upl_page_info_t pl;
	} pagelist[0];
};

static xpl_vnop_table ops;
xpl_vnop_table *xpl_vnops = NULL;

int fsadded = 0;
struct vfs_fsentry fstable;
vfstable_t vfshandles;

int xpl_vfs_mount_alloc_init(mount_t *out_mp, int fd, uint32_t blocksize,
		vnode_t *out_devvp)
{
	int err = 0;
	mount_t mp = NULL;

	xpl_trace_enter("mp=%p", mp);

	mp = malloc(sizeof(*mp));
	if (!mp) {
		err = (err = errno) ? err : ENOMEM;
		goto out;
	}

	memset(mp, 0, sizeof(*mp));

	mp->flags = MNT_RDONLY;
	mp->mount_time = time(NULL);
	mp->isrdonly = 1;

	mp->devvn.vnode_external = 1;
	mp->devvn.fsnode = (void*)(uintptr_t)fd;

	/* Unorthodox back-reference to mp from its device node so we can access
	 * the 'blocksize' field from VNOP_IOCTL/buf_strategy/buf_meta_bread/
	 * cluster_read_ext/cluster_pagein_ext. */
	mp->devvn.mp = mp;

	mp->blocksize = blocksize;
	mp->blocksize_mask = blocksize - 1;

	if (fcntl(fd, F_GETPATH, mp->stfs.f_mntfromname)) {
		xpl_perror(errno, "Error getting device name for file "
			"descriptor");
		memset(mp->stfs.f_mntfromname, 0,
			sizeof(mp->stfs.f_mntfromname));
	}

	*out_mp = mp;
	*out_devvp = &mp->devvn;
out:
	if (err && mp) {
		free(mp);
	}
	return err;
}

int64_t xpl_vfs_mount_get_mount_time(mount_t mp)
{
	return mp->mount_time;
}

void xpl_vfs_mount_teardown(mount_t mp)
{
	free(mp);
}

static ssize_t xpl_pread(vnode_t devvp, void *buf, size_t nbyte, off_t offset)
{
	ssize_t res;

	xpl_debug("pread(%d, %p, %zd, %lld)", (int)(uintptr_t)devvp->fsnode,
			buf, nbyte, offset);
	res = pread((int)(uintptr_t)devvp->fsnode, buf, nbyte, offset);

	return res;
}

int vfs_fsadd(struct vfs_fsentry *vfe, vfstable_t *handle)
{
	struct vnodeopv_desc *cur_desc;
	size_t i = 0;

	xpl_trace_enter("vfe=%p handle=%p", vfe, handle);

	if (fsadded) {
		fprintf(stderr, "Attempted to add a second filesystem in the "
			"same instance. This is not supported by the UserVFS "
			"bridge.\n");
		return ENOMEM;
	}

	fstable = *vfe;
	memset(&ops, 0, sizeof(ops));
	while ((cur_desc = fstable.vfe_opvdescs[i++])) {
		size_t j = 0;
		struct vnodeopv_entry_desc *cur_entry_desc;

		if (!cur_desc->opv_desc_ops ||
				!cur_desc->opv_desc_vector_p)
		{
			xpl_debug("NULL-terminator at vfe_opvdescs[%zu]",
				i - 1);
			break;
		}

		xpl_debug("desc %zu: %p", i - 1, cur_desc);

		while ((cur_entry_desc = &cur_desc->opv_desc_ops[j++])) {
			xpl_debug("op %zu: %p", j - 1, cur_entry_desc);
			if (!cur_entry_desc->opve_impl ||
				!cur_desc->opv_desc_vector_p)
			{
				xpl_debug("NULL-terminator at "
					"opv_desc_ops[%zu]",
					j - 1);
				break;
			}

			if (i > 1) {
				panic("Multiple sets of vnode operations are "
					"not yet supported (found %zu).", j);
				return ENOTSUP;
			}
			else if (cur_entry_desc->opve_op == &vnop_access_desc) {
				ops.vnop_access = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_advlock_desc)
			{
				ops.vnop_advlock = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_allocate_desc)
			{
				ops.vnop_allocate = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_blktooff_desc)
			{
				ops.vnop_blktooff = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_blockmap_desc)
			{
				ops.vnop_blockmap = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_bwrite_desc) {
				ops.vnop_bwrite = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_close_desc) {
				ops.vnop_close = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_copyfile_desc)
			{
				ops.vnop_copyfile = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_create_desc) {
				ops.vnop_create = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_default_desc)
			{
				ops.vnop_default = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_exchange_desc)
			{
				ops.vnop_exchange = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_fsync_desc) {
				ops.vnop_fsync = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_getattr_desc)
			{
				ops.vnop_getattr = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op ==
				&vnop_getnamedstream_desc)
			{
				ops.vnop_getnamedstream =
					cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_getxattr_desc)
			{
				ops.vnop_getxattr = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_inactive_desc)
			{
				ops.vnop_inactive = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_ioctl_desc) {
				ops.vnop_ioctl = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_link_desc) {
				ops.vnop_link = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op ==
				&vnop_listxattr_desc)
			{
				ops.vnop_listxattr = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_lookup_desc) {
				ops.vnop_lookup = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op ==
				&vnop_makenamedstream_desc)
			{
				ops.vnop_makenamedstream =
				cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_mkdir_desc) {
				ops.vnop_mkdir = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_mknod_desc) {
				ops.vnop_mknod = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_mmap_desc) {
				ops.vnop_mmap = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_mnomap_desc) {
				ops.vnop_mnomap = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_offtoblk_desc)
			{
				ops.vnop_offtoblk = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_open_desc) {
				ops.vnop_open = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_pagein_desc) {
				ops.vnop_pagein = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_pageout_desc)
			{
				ops.vnop_pageout = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_pathconf_desc)
			{
				ops.vnop_pathconf = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_read_desc) {
				ops.vnop_read = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_readdir_desc)
			{
				ops.vnop_readdir = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op ==
				&vnop_readdirattr_desc)
			{
				ops.vnop_readdirattr =
				cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_readlink_desc)
			{
				ops.vnop_readlink = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_reclaim_desc)
			{
				ops.vnop_reclaim = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_remove_desc)
			{
				ops.vnop_remove = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op ==
				&vnop_removenamedstream_desc)
			{
				ops.vnop_removenamedstream =
					cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op ==
				&vnop_removexattr_desc)
			{
				ops.vnop_removexattr =
					cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_rename_desc) {
				ops.vnop_rename = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_revoke_desc) {
				ops.vnop_revoke = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_rmdir_desc) {
				ops.vnop_rmdir = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_searchfs_desc)
			{
				ops.vnop_searchfs = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_select_desc) {
				ops.vnop_select = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_setattr_desc)
			{
				ops.vnop_setattr = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_setxattr_desc)
			{
				ops.vnop_setxattr = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_strategy_desc)
			{
				ops.vnop_strategy = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_symlink_desc)
			{
				ops.vnop_symlink = cur_entry_desc->opve_impl;
			}
			else if (cur_entry_desc->opve_op == &vnop_write_desc) {
				ops.vnop_write = cur_entry_desc->opve_impl;
			}
			else {
				panic("Unrecognized vnop: %p",
					cur_entry_desc->opve_op);
				return ENOTSUP;
			}
		}

		break;
	}

	xpl_vnops = &ops;
	vfshandles = *handle;
	fsadded = 1;

	return 0;
}

int vfs_fsremove(vfstable_t handle)
{
	xpl_trace_enter("handle=%p", handle);

	if (!fsadded) {
		fprintf(stderr, "Attempted to remove filesystem that was never "
			"added to the UserVFS bridge.\n");
		return EIO;
	}

	fsadded = 0;
	xpl_vnops = NULL;

	return 0;
}

uint64_t vfs_flags(mount_t mp)
{
	xpl_trace_enter("mp=%p", mp);
	return mp->flags;
}

void    vfs_setflags(mount_t mp, uint64_t flags)
{
	xpl_trace_enter("mp=%p flags=%" PRIu64, mp, flags);
	mp->flags = flags;
}

int     vfs_iswriteupgrade(mount_t mp)
{
	xpl_trace_enter("mp=%p", mp);
	return 0; // Not supported in UserVFS.
}

int     vfs_isupdate(mount_t mp)
{
	xpl_trace_enter("mp=%p", mp);
	return 0; // Not supported in UserVFS.
}

int     vfs_isreload(mount_t mp)
{
	xpl_trace_enter("mp=%p", mp);
	return 0; // Not supported in UserVFS.
}

int     vfs_isrdonly(mount_t mp)
{
	xpl_trace_enter("mp=%p", mp);
	return mp->isrdonly;
}

int     vfs_isrdwr(mount_t mp)
{
	xpl_trace_enter("mp=%p", mp);
	return !mp->isrdonly;
}

void    vfs_setlocklocal(mount_t mp)
{
	xpl_trace_enter("mp=%p", mp);
	(void) mp;
}

void *  vfs_fsprivate(mount_t mp)
{
	xpl_trace_enter("mp=%p", mp);
	return mp->fsprivate;
}

void    vfs_setfsprivate(mount_t mp, void *mntdata)
{
	xpl_trace_enter("mp=%p mntdata=%p", mp, mntdata);
	mp->fsprivate = mntdata;
}

struct vfsstatfs *      vfs_statfs(mount_t mp)
{
	xpl_trace_enter("mp=%p", mp);
	return &mp->stfs;
}

int     vfs_typenum(mount_t mp)
{
	xpl_trace_enter("mp=%p", mp);
	return mp->typenum;
}

int     vfs_devblocksize(mount_t mp)
{
	xpl_trace_enter("mp=%p", mp);
	return mp->blocksize;
}

void    vfs_ioattr(mount_t mp, struct vfsioattr *ioattrp)
{
	xpl_trace_enter("mp=%p ioattrp=%p", mp, ioattrp);
	*ioattrp = mp->ioattr;
}

void    vfs_setioattr(mount_t mp, struct vfsioattr *ioattrp)
{
	xpl_trace_enter("mp=%p ioattrp=%p", mp, ioattrp);
	mp->ioattr = *ioattrp;
}

int vn_default_error(void)
{
	xpl_trace_enter("");
	/* vfs_init.c declares the default error to be ENOTSUP. */
	return ENOTSUP;
}

errno_t vnode_create(uint32_t flavor, uint32_t size, void  *data, vnode_t *vpp)
{
	struct vnode_fsparam *param;
	vnode_t vp = NULL;

	xpl_trace_enter("flavor=%" PRIu32 " size=%" PRIu32 " data=%p vpp=%p",
			flavor, size, data, vpp);

	if (flavor != VNCREATE_FLAVOR || size != VCREATESIZE) {
		xpl_error("Unexpected flavor (%" PRIu32 ") and/or size "
				"(%" PRIu32 ").", flavor, size);
		return ENOTSUP;
	}

	param = (struct vnode_fsparam*) data;

	vp = calloc(1, sizeof(*vp));
	if (!vp) {
		return ENOMEM;
	}

	vp->iocount = 1;
	vp->mp = param->vnfs_mp;
	vp->vtype = param->vnfs_vtype;
	vp->fsnode = param->vnfs_fsnode;
	vp->issystem = param->vnfs_marksystem;
	vp->size = size;
	vp->datasize = param->vnfs_filesize;

	lck_rw_init(&vp->vnode_lock, ntfs_lock_grp, ntfs_lock_attr);

	*vpp = vp;
	return 0;
}

static void vnode_destroy(vnode_t vp)
{
	xpl_debug("Destroying vnode %p with iocount=%" PRIu64 " "
		"usecount=%" PRIu64 "...",
		vp, vp->iocount, vp->usecount);
	++vp->iocount;
	lck_rw_unlock_exclusive(&vp->vnode_lock);
	do {
		int (*inactive_op)(struct vnop_inactive_args *a) = NULL;
		struct vnop_inactive_args inactive_args;

		int (*reclaim_op)(struct vnop_reclaim_args *a) = NULL;
		struct vnop_reclaim_args reclaim_args;

		inactive_op = 
			(int (*)(struct vnop_inactive_args *))
			ops.vnop_inactive;
		reclaim_op =
			(int (*)(struct vnop_reclaim_args *)) ops.vnop_reclaim;

		inactive_args.a_vp = vp;
		inactive_op(&inactive_args);

		reclaim_args.a_vp = vp;
		reclaim_op(&reclaim_args);
	} while(0);
	lck_rw_destroy(&vp->vnode_lock, ntfs_lock_grp);
	memset(vp, 0, sizeof(*vp));
	free(vp);
}

int     vnode_removefsref(vnode_t vp)
{
	xpl_trace_enter("vp=%p", vp);

	/* TODO: Called from reclaim vnop, which doesn't check the return value.
	 * It's unclear if we need to implement this. */
	return ENOTSUP;
}

int     vnode_hasdirtyblks(vnode_t vp)
{
	int ret;

	xpl_trace_enter("vp=%p", vp);
	lck_rw_lock_shared(&vp->vnode_lock);
	ret = vp->hasdirtyblks;
	lck_rw_unlock_shared(&vp->vnode_lock);

	return ret;
}

enum vtype      vnode_vtype(vnode_t vp)
{
	enum vtype ret;

	xpl_trace_enter("vp=%p", vp);
	lck_rw_lock_shared(&vp->vnode_lock);
	ret = vp->vtype;
	lck_rw_unlock_shared(&vp->vnode_lock);

	return ret;
}

uint32_t        vnode_vid(vnode_t vp)
{
	xpl_trace_enter("vp=%p", vp);
	return 0;
}

mount_t vnode_mount(vnode_t vp)
{
	mount_t ret;

	xpl_trace_enter("vp=%p", vp);
	lck_rw_lock_shared(&vp->vnode_lock);
	ret = vp->mp;
	lck_rw_unlock_shared(&vp->vnode_lock);

	return ret;
}

dev_t   vnode_specrdev(vnode_t vp)
{
	xpl_trace_enter("vp=%p", vp);

	/* TODO: This is supposed to return the major/minor value of the
	 * underlying device and return it from getattr. Investigate how this
	 * can be done (when testing with an image there's of course no such
	 * value but fstat on the real device's file descriptor should get us
	 * the value that we want. */
	return 0;
}

void *  vnode_fsnode(vnode_t vp)
{
	void *ret;

	xpl_trace_enter("vp=%p", vp);
	lck_rw_lock_shared(&vp->vnode_lock);
	ret = vp->fsnode;
	lck_rw_unlock_shared(&vp->vnode_lock);

	return ret;
}

void    vnode_clearfsnode(vnode_t vp)
{
	xpl_trace_enter("vp=%p", vp);
	lck_rw_lock_exclusive(&vp->vnode_lock);
	vp->fsnode = NULL;
	lck_rw_unlock_exclusive(&vp->vnode_lock);
}

int     vnode_issystem(vnode_t vp)
{
	int ret;

	xpl_trace_enter("vp=%p", vp);
	lck_rw_lock_shared(&vp->vnode_lock);
	ret = vp->issystem;
	lck_rw_unlock_shared(&vp->vnode_lock);

	return ret;
}

int     vnode_isreg(vnode_t vp)
{
	int ret;

	xpl_trace_enter("vp=%p", vp);
	lck_rw_lock_shared(&vp->vnode_lock);
	ret = vp->isreg;
	lck_rw_unlock_shared(&vp->vnode_lock);

	return ret;
}

int     vnode_isblk(vnode_t vp)
{
	int ret;

	xpl_trace_enter("vp=%p", vp);
	lck_rw_lock_shared(&vp->vnode_lock);
	ret = vp->isblk;
	lck_rw_unlock_shared(&vp->vnode_lock);

	return ret;
}

int     vnode_ischr(vnode_t vp)
{
	int ret;

	xpl_trace_enter("vp=%p", vp);
	lck_rw_lock_shared(&vp->vnode_lock);
	ret = vp->ischr;
	lck_rw_unlock_shared(&vp->vnode_lock);

	return ret;
}

int     vnode_isnocache(vnode_t vp)
{
	int ret;

	xpl_trace_enter("vp=%p", vp);
	lck_rw_lock_shared(&vp->vnode_lock);
	ret = vp->isnocache;
	lck_rw_unlock_shared(&vp->vnode_lock);

	return ret;
}

int     vnode_isnoreadahead(vnode_t vp)
{
	int ret;

	xpl_trace_enter("vp=%p", vp);
	lck_rw_lock_shared(&vp->vnode_lock);
	ret = vp->isnoreadahead;
	lck_rw_unlock_shared(&vp->vnode_lock);

	return ret;
}

void    vnode_settag(vnode_t vp, int tag)
{
	xpl_trace_enter("vp=%p", vp);

	/* Note: This does not appear to need a proper implementation as this
	 * value is only advisory information passed to the VFS and never read
	 * by the filesystem code. So no-op implementation is intentional. */
}

proc_t  vfs_context_proc(vfs_context_t ctx)
{
	xpl_trace_enter("ctx=%p", ctx);
	return ctx->proc;
}

kauth_cred_t    vfs_context_ucred(vfs_context_t ctx)
{
	xpl_trace_enter("ctx=%p", ctx);
	return ctx->ucred;
}

int     vflush(struct mount *mp, struct vnode *skipvp, int flags)
{
	xpl_trace_enter("mp=%p skipvp=%p flags=0x%X", mp, skipvp, flags);

	/* TODO: Replicate any vflush logic that is necessary for XPL. */
	return 0;
}

int     vnode_get(vnode_t vp)
{
	xpl_trace_enter("vp=%p", vp);
	lck_rw_lock_exclusive(&vp->vnode_lock);
	++vp->iocount;
	lck_rw_unlock_exclusive(&vp->vnode_lock);

	return 0;
}

int     vnode_getwithvid(vnode_t vp, uint32_t vid)
{
	xpl_trace_enter("vp=%p vid=%" PRIu32, vp, vid);
	lck_rw_lock_exclusive(&vp->vnode_lock);
	/* TODO: Checking vid (only relevant when cached/reused?). */
	++vp->iocount;
	lck_rw_unlock_exclusive(&vp->vnode_lock);

	return 0;
}

int     vnode_put(vnode_t vp)
{
	xpl_trace_enter("vp=%p", vp);
	lck_rw_lock_exclusive(&vp->vnode_lock);
	if (!vp->iocount) {
		panic("Tried to decrement iocount down from 0.");
	}
	else if (vp->usecount > 0 || vp->iocount > 1) {
		/* We still have references, so just decrement the counter. */
		--vp->iocount;		
		lck_rw_unlock_exclusive(&vp->vnode_lock);
	}
	else {
		/* Deallocate vnode. */
		vnode_destroy(vp);
	}

	return 0;
}

int     vnode_ref(vnode_t vp)
{
	xpl_trace_enter("vp=%p", vp);
	lck_rw_lock_exclusive(&vp->vnode_lock);
	++vp->usecount;
	lck_rw_unlock_exclusive(&vp->vnode_lock);

	return 0;
}

void    vnode_rele(vnode_t vp)
{
	xpl_trace_enter("vp=%p", vp);
	lck_rw_lock_exclusive(&vp->vnode_lock);
	if (!vp->usecount) {
		panic("Tried to decrement usecount down from 0.");
	}

	--vp->usecount;
	if (vp->usecount < 1 && vp->iocount < 1) {
		vnode_destroy(vp);
	}
	else {
		lck_rw_unlock_exclusive(&vp->vnode_lock);
	}
}


int     vnode_isinuse(vnode_t vp, int refcnt)
{
	int res;

	xpl_trace_enter("vp=%p", vp);

	lck_rw_lock_exclusive(&vp->vnode_lock);
	res = (vp->usecount > refcnt) ? 1 : 0;
	lck_rw_unlock_exclusive(&vp->vnode_lock);

	return res;
}

int     vnode_recycle(vnode_t vp)
{
	xpl_trace_enter("vp=%p", vp);

	/* vnode_recycle is a hint to make sure the vnode is reclaimed
	 * immediately after the last reference is dropped. In XPL we do this
	 * anyway so it has no effect. */
	return 0;
}

void    vnode_update_identity(vnode_t vp, vnode_t dvp, const char *name, int name_len, uint32_t name_hashval, int flags)
{
	xpl_trace_enter("vp=%p dvp=%p name=%p (\"%.*s\") name_len=%d "
			"name_hashval=%" PRIu32 " flags=%d",
			vp, dvp, name, name ? name_len : 0, name ? name : "",
			name_len, name_hashval, flags);

	if (flags & VNODE_UPDATE_PARENT) {
		vnode_t cur_parent;

		/* Fetch the current parent and set the parent of the vnode to
		 * NULL while we are updating identity. */
		lck_rw_lock_exclusive(&vp->vnode_lock);
		cur_parent = vp->parent;
		vp->parent = NULL;
		lck_rw_unlock_exclusive(&vp->vnode_lock);

		if (dvp) {
			lck_rw_lock_exclusive(&dvp->vnode_lock);
			++dvp->usecount;
			lck_rw_unlock_exclusive(&dvp->vnode_lock);
		}

		if (cur_parent) {
			lck_rw_lock_exclusive(&cur_parent->vnode_lock);
			if (!cur_parent->usecount) {
				panic("Tried to decrement usecount down from "
					"0.");
			}

			--cur_parent->usecount;
			if (cur_parent->usecount < 1 && cur_parent->iocount < 1)
			{
				vnode_destroy(cur_parent);
			}
			else {
				lck_rw_unlock_exclusive(&cur_parent->
					vnode_lock);
			}
		}

		lck_rw_lock_exclusive(&vp->vnode_lock);
		if (vp->parent) {
			/* Someone raced us to setting the parent and won. Thus
			 * we have to release our usecount reference to dvp. */
			lck_rw_unlock_exclusive(&vp->vnode_lock);

			vnode_rele(dvp);
		}
		else {
			vp->parent = dvp;
			lck_rw_unlock_exclusive(&vp->vnode_lock);
		}
	}
}

int     vn_bwrite(struct vnop_bwrite_args *ap)
{
	xpl_trace_enter("ap=%p", ap);

	/* TODO: This needs to be implemented for write support. */
	return ENOTSUP;
}

int     vnode_iterate(struct mount *mp, int flags, int (*callout)(struct vnode *, void *), void *arg)
{
	xpl_trace_enter("mp=%p flags=0x%X callout=%p args=%p",
		mp, flags, callout, arg);

	/* TODO: This is called from 2 contexts. When syncing an entire volume's
	 * inodes to disk (only relevant when mounted read/write) and when
	 * marking all of a volume's inodes to be recycled (currently a no-op as
	 * we always "recycle"). So this does not need to be implemented yet.
	 * (Returning ENOTSUP even though the return value is never checked in
	 * the ntfs code.) */
	return ENOTSUP;
}

int     cache_lookup(vnode_t dvp, vnode_t *vpp, struct componentname *cnp)
{
	xpl_trace_enter("dvp=%p vpp=%p cnp=%p", dvp, vpp, cnp);

	/* We don't maintain a name cache in XPL right now, so return 0 to
	 * indicate a cache miss (-1 means found, ENOENT a negative hit). */
	return 0;
}
void    cache_enter(vnode_t dvp, vnode_t vp, struct componentname *cnp)
{
	xpl_trace_enter("dvp=%p vp=%p cnp=%p", dvp, vp, cnp);

	/* We don't maintain a name cache in XPL right now, so this function is
	 * a no-op. */
}
void    cache_purge(vnode_t vp)
{
	xpl_trace_enter("vp=%p", vp);

	/* We don't maintain a name cache in XPL right now, so this function is
	 * a no-op. */
}
void    cache_purge_negatives(vnode_t vp)
{
	xpl_trace_enter("vp=%p", vp);

	/* We don't maintain a name cache in XPL right now, so this function is
	 * a no-op. */
}

const char      *vnode_getname(vnode_t vp)
{
	xpl_trace_enter("vp=%p", vp);

	/* We don't refcount the nodename, so just return it. */
	return vp->nodename;
}

void    vnode_putname(const char *name)
{
	xpl_trace_enter("name=%p (%s)", name, name);
	/* We don't refcount the nodename, so this is a no-op. */
}

vnode_t vnode_getparent(vnode_t vp)
{
	xpl_trace_enter("vp=%p", vp);
	//vnode_getwithvid(pvp, pvid);
	//return vp->parent;
	return NULL;
}

/* Stub declaration of struct vnodeop_desc. */
struct vnodeop_desc {
	uint8_t reserved;
};

/* Stub declarations of global 'desc' symbols. */
struct vnodeop_desc vnop_access_desc;
struct vnodeop_desc vnop_advlock_desc;
struct vnodeop_desc vnop_allocate_desc;
struct vnodeop_desc vnop_blktooff_desc;
struct vnodeop_desc vnop_blockmap_desc;
struct vnodeop_desc vnop_bwrite_desc;
struct vnodeop_desc vnop_close_desc;
struct vnodeop_desc vnop_copyfile_desc;
struct vnodeop_desc vnop_create_desc;
struct vnodeop_desc vnop_default_desc;
struct vnodeop_desc vnop_exchange_desc;
struct vnodeop_desc vnop_fsync_desc;
struct vnodeop_desc vnop_getattr_desc;
struct vnodeop_desc vnop_getnamedstream_desc;
struct vnodeop_desc vnop_getxattr_desc;
struct vnodeop_desc vnop_inactive_desc;
struct vnodeop_desc vnop_ioctl_desc;
struct vnodeop_desc vnop_link_desc;
struct vnodeop_desc vnop_listxattr_desc;
struct vnodeop_desc vnop_lookup_desc;
struct vnodeop_desc vnop_makenamedstream_desc;
struct vnodeop_desc vnop_mkdir_desc;
struct vnodeop_desc vnop_mknod_desc;
struct vnodeop_desc vnop_mmap_desc;
struct vnodeop_desc vnop_mnomap_desc;
struct vnodeop_desc vnop_offtoblk_desc;
struct vnodeop_desc vnop_open_desc;
struct vnodeop_desc vnop_pagein_desc;
struct vnodeop_desc vnop_pageout_desc;
struct vnodeop_desc vnop_pathconf_desc;
struct vnodeop_desc vnop_read_desc;
struct vnodeop_desc vnop_readdir_desc;
struct vnodeop_desc vnop_readdirattr_desc;
struct vnodeop_desc vnop_readlink_desc;
struct vnodeop_desc vnop_reclaim_desc;
struct vnodeop_desc vnop_remove_desc;
struct vnodeop_desc vnop_removenamedstream_desc;
struct vnodeop_desc vnop_removexattr_desc;
struct vnodeop_desc vnop_rename_desc;
struct vnodeop_desc vnop_revoke_desc;
struct vnodeop_desc vnop_rmdir_desc;
struct vnodeop_desc vnop_searchfs_desc;
struct vnodeop_desc vnop_select_desc;
struct vnodeop_desc vnop_setattr_desc;
struct vnodeop_desc vnop_setxattr_desc;
struct vnodeop_desc vnop_strategy_desc;
struct vnodeop_desc vnop_symlink_desc;
struct vnodeop_desc vnop_write_desc;

/* <Header: vfs/vfs_support.h> */

int nop_revoke(struct vnop_revoke_args *ap)
{
	xpl_trace_enter("ap=%p", ap);
	return 0;
}

int nop_readdirattr(struct vnop_readdirattr_args *ap)
{
	xpl_trace_enter("ap=%p", ap);
	return 0;
}

int nop_allocate(struct vnop_allocate_args *ap)
{
	xpl_trace_enter("ap=%p", ap);
	return 0;
}

int err_advlock(struct vnop_advlock_args *ap)
{
	xpl_trace_enter("ap=%p", ap);
	/* vfs_support.c declares the default error to be be ENOTSUP. */
	return ENOTSUP;
}

int err_searchfs(struct vnop_searchfs_args *ap)
{
	xpl_trace_enter("ap=%p", ap);
	/* vfs_support.c declares the default error to be be ENOTSUP. */
	return ENOTSUP;
}

int err_copyfile(struct vnop_copyfile_args *ap)
{
	xpl_trace_enter("ap=%p", ap);
	/* vfs_support.c declares the default error to be be ENOTSUP. */
	return ENOTSUP;
}

/* </Header: vfs/vfs_support.h> */

/* <Header: sys/buf.h> */

errno_t	buf_error(buf_t bp)
{
	xpl_trace_enter("bp=%p", bp);
	return bp->err;
}

void	buf_seterror(buf_t bp, errno_t err)
{
	xpl_trace_enter("bp=%p err=%d (%s)", bp, err, strerror(err));
	bp->err = err;
}

#define BUF_X_WRFLAGS (B_PHYS | B_RAW | B_LOCKED | B_ASYNC | B_READ | B_WRITE | B_PAGEIO |\
		       B_NOCACHE | B_FUA | B_PASSIVE | B_IOSTREAMING)

void	buf_setflags(buf_t bp, int32_t flags)
{
	xpl_trace_enter("bp=%p flags=0x%" PRIX32, bp, flags);
	bp->flags |= (flags & BUF_X_WRFLAGS);
}

int32_t	buf_flags(buf_t bp)
{
	xpl_trace_enter("bp=%p", bp);
	return bp->flags;
}

errno_t	buf_map(buf_t bp, caddr_t *io_addr)
{
	xpl_trace_enter("bp=%p io_addr=%p", bp, io_addr);
	*io_addr = bp->buf;
	return 0;
}

errno_t	buf_unmap(buf_t bp)
{
	xpl_trace_enter("bp=%p", bp);
	return 0;
}

daddr64_t buf_lblkno(buf_t bp)
{
	xpl_trace_enter("bp=%p", bp);
	return bp->lblkno;
}

uint32_t buf_count(buf_t bp)
{
	xpl_trace_enter("bp=%p", bp);
	return bp->count;
}

uint32_t buf_size(buf_t bp)
{
	xpl_trace_enter("bp=%p", bp);
	return bp->size;
}

vnode_t	buf_vnode(buf_t bp)
{
	xpl_trace_enter("bp=%p", bp);
	return bp->vnode;
}

errno_t	buf_strategy(vnode_t devvp, void *ap)
{
	int err = 0;
	struct vnop_strategy_args *args;
	buf_t buf;
	int (*blktooff_op)(struct vnop_blktooff_args *a) = NULL;
	int (*blockmap_op)(struct vnop_blockmap_args *a) = NULL;

	struct vnop_blktooff_args blktooff_args;
	off_t foffset = 0;

	xpl_trace_enter("devvp=%p ap=%p", devvp, ap);

	args = (struct vnop_strategy_args*) ap;
	buf = args->a_bp;
	xpl_debug("devvp->vnode_external: %d", devvp->vnode_external);
	if (!devvp->vnode_external) {
		xpl_error("buf_strategy unsupported for internal vnodes.");
		err = ENOTSUP;
		goto out;
	}

	xpl_debug("args->a_bp->vnode->mp: %p", args->a_bp->vnode->mp);
	xpl_debug("buf->size: %" PRIu32, buf->size);
	xpl_debug("buf->count: %" PRIu32, buf->count);
	xpl_debug("buf->lblkno: %" PRId64, buf->lblkno);
	xpl_debug("buf->flags: 0x%X", buf->flags);

	blktooff_op = (int (*)(struct vnop_blktooff_args *)) ops.vnop_blktooff;
	blockmap_op = (int (*)(struct vnop_blockmap_args *)) ops.vnop_blockmap;

	memset(&blktooff_args, 0, sizeof(blktooff_args));
	blktooff_args.a_vp = args->a_bp->vnode;
	blktooff_args.a_lblkno = buf->lblkno;
	blktooff_args.a_offset = &foffset;

	err = blktooff_op(&blktooff_args);
	if (err) {
		xpl_perror(err, "Error from blktooff operation");
		goto out;
	}

	xpl_debug("Mapped logical block %" PRId64 " to byte offset %" PRId64 ".",
			buf->lblkno, foffset);

	while (buf->count < buf->size) {
		struct vnop_blockmap_args blockmap_args;
		daddr64_t bpn;
		size_t run;
		ssize_t res;

		memset(&blockmap_args, 0, sizeof(blockmap_args));
		blockmap_args.a_vp = buf->vnode;
		blockmap_args.a_foffset = foffset;
		blockmap_args.a_size = buf->size - buf->count;
		blockmap_args.a_bpn = &bpn;
		blockmap_args.a_run = &run;
		blockmap_args.a_poff = NULL;
		blockmap_args.a_flags = VNODE_READ;

		err = blockmap_op(&blockmap_args);
		if (err) {
			xpl_perror(err, "Error from blockmap operation");
			goto out;
		}

		xpl_debug("bpn: %" PRId64, bpn);
		xpl_debug("run: %zu", run);

		if (run > buf->size - buf->count) {
			/* Don't overflow. */
			run = buf->size - buf->count;
		}

		/* Transfer the data from the special device vnode, which we
		 * know has a file descriptor as fsnode. */
		if (bpn != -1) {
			const size_t aligned_run =
				/* We enforce page-aligned buffers in buf_t,
				 * meaning we always have space to read a full
				 * sector (assuming sector size <= page size,
				 * which seems to be a reasonable assumption).
				 * This is both more efficient and often
				 * required (direct I/O raw devices, etc.).
				 * XXX: Note that once we have a proper page
				 * cache we can just use that instead. */
				(run + devvp->mp->blocksize_mask) &
				~devvp->mp->blocksize_mask;
#ifdef XPL_IO_DEBUG
			xpl_info("Reading %zu (->%zu) bytes from %lld...",
					run, aligned_run,
					bpn * devvp->mp->blocksize);
#endif
			res = xpl_pread(devvp, &buf->buf[buf->count],
					aligned_run,
					bpn * devvp->mp->blocksize);
			if (res < 0) {
				err = (err = errno) ? err : EIO;
				xpl_perror(errno, "Error while reading %zu "
						"bytes from device offset "
						"%" PRId64,
						run,
						bpn * devvp->mp->blocksize);
				goto out;
			}
			else if (res > run) {
				res = run;
			}
		}
		else {
			memset(&buf->buf[buf->count], 0, run);
			res = run;
		}

		xpl_debug("Advancing foffset: %lld -> %lld",
				foffset, foffset + res);
		foffset += res;
		xpl_debug("Advancing buf->count: %" PRIu32 " -> %" PRIu32,
				buf->count, (uint32_t)(buf->count + res));
		buf->count += res;
	}

	buf_biodone(buf);
out:
	return err;
}

errno_t	buf_invalblkno(vnode_t vp, daddr64_t lblkno, int flags)
{
	xpl_trace_enter("vp=%p lblkno=%" PRId64 " flags=0x%X",
			vp, lblkno, flags);
	/* TODO: This is only required for write support and its semantics have
	 * not been researched yet. If we get here we panic (we won't,
	 * hopefully). */
	panic("Unimplemented function %s.", __FUNCTION__);
	return ENOTSUP;
}

int	buf_invalidateblks(vnode_t vp, int flags, int slpflag, int slptimeo)
{
	xpl_trace_enter("vp=%p flags=0x%X slpflag=0x%X slpflag=%d",
			vp, flags, slpflag, slptimeo);

	/* Invalidate all buffers associated with the vnode. Currently there are
	 * none, but this may change with write support. */
	return 0;
}

void	buf_flushdirtyblks(vnode_t vp, int wait, int flags, const char *msg)
{
	xpl_trace_enter("vp=%p wait=%d flags=0x%X msg=%p (%s)",
			vp, wait, flags, msg, msg);
	/* TODO: This needs to be implemented for write support. */
	panic("No flushdirtyblocks");
}

void	buf_clear(buf_t bp)
{
	xpl_trace_enter("bp=%p", bp);

	memset(&bp->buf, 0, bp->count);
}
errno_t	buf_bawrite(buf_t bp)
{
	xpl_trace_enter("bp=%p", bp);

	/* TODO: This needs to be implemented for write support. */
	return ENOTSUP;
}
errno_t	buf_bdwrite(buf_t bp)
{
	xpl_trace_enter("bp=%p", bp);

	/* TODO: This needs to be implemented for write support. */
	return ENOTSUP;
}
errno_t	buf_bwrite(buf_t bp)
{
	xpl_trace_enter("bp=%p", bp);

	/* TODO: This needs to be implemented for write support. */
	return ENOTSUP;
}
void	buf_biodone(buf_t bp)
{
	xpl_trace_enter("bp=%p", bp);

	if (bp->filter) {
		/* Need to call filter when we are done. */
		bp->filter(bp, bp->transaction);

		/* Reset filter and transaction as this is a one-time thing. */
		bp->filter = NULL;
		bp->transaction = NULL;
	}
}

void	buf_brelse(buf_t bp)
{
	xpl_trace_enter("bp=%p", bp);
	if (bp) {
		free(bp);
	}
}

static buf_t _buf_alloc(vnode_t vp, int size)
{
	const size_t aligned_size =
		(((size_t) size) + vp->mp->blocksize_mask) &
		~vp->mp->blocksize_mask;

	buf_t buf;

	buf = malloc(sizeof(*buf) + aligned_size);
	if (!buf) {
		return NULL;
	}

	xpl_debug("allocated %zu (->%zu) bytes: %p",
		sizeof(*buf) + size, sizeof(*buf) + aligned_size, buf);

	memset(buf, 0, sizeof(*buf) + size);
	buf->vnode = vp;
	buf->size = size;
	buf->allocated_size = aligned_size;

	return buf;
}

errno_t	buf_meta_bread(vnode_t vp, daddr64_t blkno, int size, kauth_cred_t cred, buf_t *bpp)
{
	int err = 0;
	buf_t readbuf = NULL;
	ssize_t res = 0;

	xpl_trace_enter("vp=%p blkno=%llu size=%d cred=%p bpp=%p",
			vp, blkno, size, cred, bpp);
	if (size < 0) {
		xpl_error("Invalid negative size: %d", size);
		err = EINVAL;
		goto out;
	}

	readbuf = _buf_alloc(vp, size);
	if (!readbuf) {
		err = ENOMEM;
		goto out;
	}

	readbuf->lblkno = blkno;
	readbuf->flags = B_READ;

	if (!vp->vnode_external) {
		int (*strategy_op)(struct vnop_strategy_args *a) = NULL;
		struct vnop_strategy_args args;

		strategy_op =
			(int (*)(struct vnop_strategy_args*)) ops.vnop_strategy;

		args.a_bp = readbuf;

		err = strategy_op(&args);
		if (err) {
			xpl_perror(err, "Error while initiating strategy "
					"operation for internal vnode");
			goto out;
		}
	}
	else {
		const size_t aligned_run =
			/* We enforce page-aligned buffers in buf_t, meaning we
			 * always have space to read a full sector (assuming
			 * sector size <= page size, which seems to be a
			 * reasonable assumption).This is both more efficient
			 * and often required (direct I/O raw devices, etc.).
			 * XXX: Note that once we have a proper page cache we
			 * can just use that instead. */
			(readbuf->size + vp->mp->blocksize_mask) &
			~vp->mp->blocksize_mask;
#ifdef XPL_IO_DEBUG
		xpl_info("Reading %" PRIu32 " (->%zu) bytes from %lld...",
				readbuf->size, aligned_run,
				readbuf->lblkno * vp->mp->blocksize);
#endif
		res = xpl_pread(vp, readbuf->buf, aligned_run,
				readbuf->lblkno * vp->mp->blocksize);
		if (res < 0) {
			err = (err = errno) ? errno : EIO;
			xpl_perror(errno, "Error from pread");
			goto out;
		}
		else if (res < readbuf->size) {
			xpl_error("Incomplete pread: %zd / %" PRIu32 " bytes",
					res, readbuf->size);
			err = EIO;
			goto out;
		}
		else if (res > readbuf->size) {
			res = readbuf->size;
		}
	}

	*bpp = readbuf;
	readbuf = NULL;
out:
	if (readbuf) {
		free(readbuf);
	}

	return err;
}

buf_t	buf_getblk(vnode_t vp, daddr64_t blkno, int size, int slpflag, int slptimeo, int operation)
{
	xpl_trace_enter("vp=%p blkno=%" PRId64 " size=%d slpflag=%d "
			"slptimeo=%d operation=%d",
			vp, blkno, size, slpflag, slptimeo, operation);

	/* TODO: This needs to be implemented for write support. */
	panic("No error return path.");
	return NULL;
}

void buf_setfilter(buf_t bp, void (*filter)(buf_t, void *), void *transaction,
		void (**old_iodone)(buf_t, void *), void **old_transaction)
{
	xpl_trace_enter("bp=%p filter=%p transaction=%p old_iodone=%p "
			"old_transaction=%p",
			bp, filter, transaction, old_iodone, old_transaction);

	if (old_iodone) {
		*old_iodone = bp->filter;
	}

	if (old_transaction) {
		*old_transaction = bp->transaction;
	}

	bp->filter = filter;
	bp->transaction = transaction;
}

/* </Header: sys/buf.h> */

/* <Header: sys/uio.h> */

uio_t uio_create( int a_iovcount,               /* max number of iovecs */
    off_t a_offset,                                             /* current offset */
    int a_spacetype,                                            /* type of address space */
    int a_iodirection )                                 /* read or write flag */
{
	struct uio *res;

	xpl_trace_enter("a_iovcountr=%d a_offset=%lld a_spacetype=%d "
			"a_iodirection=%d",
			a_iovcount, a_offset, a_spacetype, a_iodirection);

	res = calloc(1, sizeof(struct uio));
	if (res) {
		res->offset = a_offset;
	}

	return res;
}

void uio_reset( uio_t a_uio,
    off_t a_offset,                                             /* current offset */
    int a_spacetype,                                            /* type of address space */
    int a_iodirection )                                 /* read or write flag */
{
	xpl_trace_enter("a_uio=%p a_offset=%" PRId64 " a_spacetype=%d "
			"a_iodirection=%d",
			a_uio, a_offset, a_spacetype, a_iodirection);
	memset(a_uio, 0, sizeof(*a_uio));
	a_uio->offset = a_offset;
}

void uio_free( uio_t a_uio )
{
	xpl_trace_enter("a_uio=%p", a_uio);
	free(a_uio);
}

int uio_addiov( uio_t a_uio, user_addr_t a_baseaddr, user_size_t a_length )
{
	int err = 0;

	xpl_trace_enter("a_uio=%p a_baseaddr=0x%" PRIX64 " a_length=%" PRIu64,
			a_uio, a_baseaddr, a_length);
	if (a_uio->iov_baseaddr) {
		xpl_error("Only one iov supported.");
		err = EINVAL;
		goto out;
	}

	a_uio->iov_baseaddr = a_baseaddr;
	a_uio->iov_length = a_length;
	a_uio->resid += a_length;
out:
	return err;
}

user_ssize_t uio_resid( uio_t a_uio )
{
	xpl_trace_enter("a_uio=%p", a_uio);

	return a_uio->resid;
}
void uio_setresid( uio_t a_uio, user_ssize_t a_value )
{
	xpl_trace_enter("a_uio=%p a_value=%" PRId64, a_uio, a_value);

	a_uio->resid = a_value;
}

off_t uio_offset( uio_t a_uio )
{
	xpl_trace_enter("a_uio=%p", a_uio);

	return a_uio->offset;
}
void uio_setoffset( uio_t a_uio, off_t a_offset )
{
	xpl_trace_enter("a_uio=%p a_offset=%" PRId64, a_uio, a_offset);

	a_uio->offset = a_offset;
}

int uiomove(const char * cp, int n, struct uio *uio)
{
	xpl_trace_enter("cp=%p n=%d uio=%p", cp, n, uio);

	if (n < 0) {
		xpl_debug("Invalid parameter 'n': %d", n);
		return EINVAL;
	}

	if ((user_ssize_t)n > uio->resid) {
		xpl_debug("Truncating uiomove amount: %d -> %d",
			n, (int)uio->resid);
		n = (int)uio->resid;
	}

	memcpy((void*)(uintptr_t)uio->iov_baseaddr, cp, n);
	uio->iov_baseaddr += n;
	uio->resid -=n;
	return 0;
}

/* </Header: sys/uio.h> */

/* <Header: sys/utfconv.h> */

static const uint16_t xpl_sfm_wrap_table[] = {
	0x0000, 0xF001,
	0xF002, 0xF003,
	0xF004, 0xF005,
	0xF006, 0xF007,
	0xF008, 0xF009,
	0xF00A, 0xF00B,
	0xF00C, 0xF00D,
	0xF00E, 0xF00F,
	0xF010, 0xF011,
	0xF012, 0xF013,
	0xF014, 0xF015,
	0xF016, 0xF017,
	0xF018, 0xF019,
	0xF01A, 0xF01B,
	0xF01C, 0xF01D,
	0xF01E, 0xF01F,
	0x0020, 0x0021,
	0xF020, 0x0023,
	0x0024, 0x0025,
	0x0026, 0x0027,
	0x0028, 0x0029,
	0xF021, 0x002B,
	0x002C, 0x002D,
	0x002E, 0x002F,
	0x0030, 0x0031,
	0x0032, 0x0033,
	0x0034, 0x0035,
	0x0036, 0x0037,
	0x0038, 0x0039,
	0xF022, 0x003B,
	0xF023, 0x003D,
	0xF024, 0xF025,
	0x0040, 0x0041,
	0x0042, 0x0043,
	0x0044, 0x0045,
	0x0046, 0x0047,
	0x0048, 0x0049,
	0x004A, 0x004B,
	0x004C, 0x004D,
	0x004E, 0x004F,
	0x0050, 0x0051,
	0x0052, 0x0053,
	0x0054, 0x0055,
	0x0056, 0x0057,
	0x0058, 0x0059,
	0x005A, 0x005B,
	0xF026, 0x005D,
	0x005E, 0x005F,
	0x0060, 0x0061,
	0x0062, 0x0063,
	0x0064, 0x0065,
	0x0066, 0x0067,
	0x0068, 0x0069,
	0x006A, 0x006B,
	0x006C, 0x006D,
	0x006E, 0x006F,
	0x0070, 0x0071,
	0x0072, 0x0073,
	0x0074, 0x0075,
	0x0076, 0x0077,
	0x0078, 0x0079,
	0x007A, 0x007B,
	0xF027,
};

static const uint16_t xpl_sfm_unwrap_table[] = {
	0x0001, 0x0002,
	0x0003, 0x0004,
	0x0005, 0x0006,
	0x0007, 0x0008,
	0x0009, 0x000A,
	0x000B, 0x000C,
	0x000D, 0x000E,
	0x000F, 0x0010,
	0x0011, 0x0012,
	0x0013, 0x0014,
	0x0015, 0x0016,
	0x0017, 0x0018,
	0x0019, 0x001A,
	0x001B, 0x001C,
	0x001D, 0x001E,
	0x001F, 0x0022,
	0x002A, 0x003A,
	0x003C, 0x003E,
	0x003F, 0x005C,
	0x007C,
};

static uint16_t xpl_sfm_remap_char(const uint16_t cur, const int unwrap,
		const int is_final)
{
	uint16_t res;

	if (cur <= 0x7C) {
		res = xpl_sfm_wrap_table[cur];
	}
	else if (cur >= 0xF001 && cur <= 0xF027) {
		res = xpl_sfm_unwrap_table[cur - 0xF001];
	}
	else if (cur == 0xF02A) {
		/* SFM substitution -> Apple logo */
		res = 0xF8FF;
	}
	else if (cur == 0xF8FF) {
		/* Apple logo -> SFM substitution */
		res = 0xF02A;
	}
	else {
		res = cur;
	}

	if (is_final) {
		/* If the last character is a space or period, use SFM
		 * substitutions. */
		if (unwrap) {
			if (cur == 0xF028) {
				res = 0x20;
			}
			else if (cur == 0xF029) {
				res = 0x2E;
			}
		}
		else {
			if (cur == 0x20) {
				res = 0xF028;
			}
			else if (cur == 0x2E) {
				res = 0xF029;
			}
		}
	}

	return res;
}

#ifdef __APPLE__
/* For now we use CoreFoundation to implement the encoding/decoding and
 * normalization functionality present in the kernel's utf8 library. This
 * however does allocations and deallocations and might be a performance problem
 * as many operations can usually be performed inline. */

static int utf8_transcode(int encode, const void *inp, size_t insize,
		void *outp, size_t *outlen, size_t outbufsize, int flags)
{
	int err = 0;

	CFStringRef cf_source_string;
	CFRange range_to_process;
	CFIndex required_buffer_size = 0;

	/* Convert the UTF-8 string to a CFString. */
	cf_source_string = CFStringCreateWithBytes(
		/* CFAllocatorRef alloc */
		kCFAllocatorDefault,
		/* const UInt8 *bytes */
		(const UInt8*) inp,
		/* CFIndex numBytes */
		insize,
		/* CFStringEncoding encoding */
		encode ? kCFStringEncodingUTF16LE : kCFStringEncodingUTF8,
		/* Boolean isExternalRepresentation */
		false);
	if (cf_source_string == NULL) {
		xpl_error("Error while creating CFString.");
		err = ENOMEM;
		goto out;
	}

	if (flags & (UTF_DECOMPOSED | UTF_PRECOMPOSED | UTF_SFM_CONVERSIONS)) {
		/* Create a mutable string from cfSourceString that we are free
		 * to modify. */
		CFMutableStringRef cf_mutable_string;

		cf_mutable_string = CFStringCreateMutableCopy(
			/* CFAllocatorRef alloc */
			kCFAllocatorDefault,
			/* CFIndex maxLength */
			0,
			/* CFStringRef theString */
			cf_source_string);
		if (cf_mutable_string == NULL) {
			xpl_error("Error while creating mutable copy of "
				"CFString.");
			err = ENOMEM;
			goto out;
		}

		if (flags & UTF_SFM_CONVERSIONS) {
			const CFIndex length =
				CFStringGetLength(
					/* CFStringRef theString */
					cf_mutable_string);
			CFIndex i;

			for (i = 0; i < length; ++i) {
				const UniChar cur_char =
					CFStringGetCharacterAtIndex(
						/* CFStringRef theString */
						cf_mutable_string,
						/* CFIndex idx */
						i);
				const UniChar remapped_char =
					xpl_sfm_remap_char(
						/* uint16_t cur */
						cur_char,
						/* int unwrap */
						encode ? 1 : 0,
						/* int is_final */
						(i == length - 1) ? 1 : 0);
				CFStringRef char_str;

				if (cur_char == remapped_char) {
					/* No mapping for this character. */
					continue;
				}

				/* There's apparently no support for setting
				 * individual characters in a
				 * CFMutableString? */
				char_str =
					CFStringCreateWithCharactersNoCopy(
						/* CFAllocatorRef alloc */
						kCFAllocatorDefault,
						/* const UniChar *chars */
						&remapped_char,
						/* CFIndex numChars */
						1,
						/* CFAllocatorRef
						 * contentsDeallocator */
						kCFAllocatorNull);

				CFStringReplace(
					/* CFMutableStringRef theString */
					cf_mutable_string,
					/* CFRange range*/
					CFRangeMake(i, 1),
					/* CFStringRef replacement */
					char_str);
				CFRelease(char_str);
			}
		}

		if (flags & (UTF_DECOMPOSED | UTF_PRECOMPOSED)) {
			/* Normalize the mutable string to the desired
			 * normalization form. */
			CFStringNormalize(
				/* CFMutableStringRef theString */
				cf_mutable_string,
				/* CFStringNormalizationForm theForm */
				(flags & UTF_DECOMPOSED) ?
				kCFStringNormalizationFormD :
				kCFStringNormalizationFormC);
		}

		/* Replace source string with modified copy. */
		CFRelease(cf_source_string);
		cf_source_string = cf_mutable_string;
		cf_mutable_string = NULL;
	}

	/* Store the resulting string in a '\0'-terminated UTF-8 encoded char*
	 * buffer. */
	range_to_process = CFRangeMake(0, CFStringGetLength(cf_source_string));
	if (CFStringGetBytes(
		/* CFStringRef theString */
		cf_source_string,
		/* CFRange range */
		range_to_process,
		/* CFStringEncoding encoding */
		encode ? kCFStringEncodingUTF8 : kCFStringEncodingUTF16LE,
		/* UInt8 lossByte */
		0,
		/* Boolean isExternalRepresentation */
		false,
		/* UInt8 *buffer */
		NULL,
		/* CFIndex maxBufLen */
		0,
		/* CFIndex *usedBufLen */
		&required_buffer_size) <= 0)
	{
		xpl_error("Could not perform check for required length of "
			"UTF-%d conversion of CFMutableString.",
			encode ? 8 : 16);
		err = ENOMEM;
		goto out;
	}

	if (outp) {
		if (outbufsize < required_buffer_size + 1) {
			xpl_error("Buffer too small.");
			err = ERANGE;
			goto out;
		}

		if (CFStringGetBytes(
			/* CFStringRef theString */
			cf_source_string,
			/* CFRange range */
			range_to_process,
			/* CFStringEncoding encoding */
			encode ? kCFStringEncodingUTF8 :
			kCFStringEncodingUTF16LE,
			/* UInt8 lossByte */
			0,
			/* Boolean isExternalRepresentation */
			false,
			/* UInt8 *buffer */
			(UInt8*)outp,
			/* CFIndex maxBufLen */
			required_buffer_size,
			/* CFIndex *usedBufLen */
			&required_buffer_size) <= 0)
		{
			xpl_error("Could not perform UTF-8 conversion of "
				"CFMutableString.");
			err = EILSEQ;
			goto out;
		}

		((UInt8*)outp)[required_buffer_size] = '\0';
	}

	*outlen = required_buffer_size;
out:
	if (cf_source_string) {
		CFRelease(cf_source_string);
	}

	return err;
}

#else
#include <limits.h>

static int utf16_to_utf8_size(const uint16_t *ins, const int ins_len,
		int outs_len)
{
	int i, ret = -1;
	int count = 0;
	boolean_t surrog;

	surrog = FALSE;
	for (i = 0; i < ins_len && ins[i]; i++) {
		uint16_t c = OSSwapLittleToHostInt16(ins[i]);
		if (surrog) {
			if ((c >= 0xdc00) && (c < 0xe000)) {
				surrog = FALSE;
				count += 4;
			} else {
				goto fail;
			}
		} else {
			if (c < 0x80) {
				count++;
			} else if (c < 0x800) {
				count += 2;
			} else if (c < 0xd800) {
				count += 3;
			} else if (c < 0xdc00) {
				surrog = TRUE;
			} else if (c >= 0xe000)
			{
				count += 3;
			} else {
				goto fail;
			}
		}
		if (count > outs_len) {
			errno = ENAMETOOLONG;
			goto out;
		}
	}
	if (surrog) {
		goto fail;
	}

	ret = count;
out:
	return ret;
fail:
	errno = EILSEQ;
	goto out;
}

static int ntfs_utf16_to_utf8(const uint16_t *ins, const int ins_len,
		char **outs, int outs_len)
{
	int ret = -1;
	char *t;
	int i;
	int size;
	int halfpair = 0;


	size = utf16_to_utf8_size(ins, ins_len, outs_len);
	if (size < 0) {
		goto out;
	}

	t = *outs;

	for (i = 0; i < ins_len && ins[i]; i++) {
		uint16_t c = OSSwapLittleToHostInt16(ins[i]);
			/* size not double-checked */
		if (halfpair) {
			if ((c >= 0xdc00) && (c < 0xe000)) {
				*t++ = 0xf0 + (((halfpair + 64) >> 8) & 7);
				*t++ = 0x80 + (((halfpair + 64) >> 2) & 63);
				*t++ = 0x80 + ((c >> 6) & 15) + ((halfpair & 3) << 4);
				*t++ = 0x80 + (c & 63);
				halfpair = 0;
			} else {
				goto fail;
			}
		} else if (c < 0x80) {
			*t++ = c;
		} else {
			if (c < 0x800) {
				*t++ = (0xc0 | ((c >> 6) & 0x3f));
				*t++ = 0x80 | (c & 0x3f);
			} else if (c < 0xd800) {
				*t++ = 0xe0 | (c >> 12);
				*t++ = 0x80 | ((c >> 6) & 0x3f);
				*t++ = 0x80 | (c & 0x3f);
			} else if (c < 0xdc00) {
				halfpair = c;
			} else if (c >= 0xe000) {
				*t++ = 0xe0 | (c >> 12);
				*t++ = 0x80 | ((c >> 6) & 0x3f);
				*t++ = 0x80 | (c & 0x3f);
			} else {
				goto fail;
			}
		}
	}
	*t = '\0';
	ret = t - *outs;
out:
	return ret;
fail:
	errno = EILSEQ;
	goto out;
}

static int utf8_to_utf16_size(const char *s)
{
	int ret = -1;
	unsigned int byte;
	size_t count = 0;

	while ((byte = *((const unsigned char*)s++))) {
		if (++count >= PATH_MAX) {
			goto fail;
		}
		if (byte >= 0xc0) {
			if (byte >= 0xF5) {
				errno = EILSEQ;
				goto out;
			}
			if (!*s) {
				break;
			}
			if (byte >= 0xC0) {
				s++;
			}
			if (!*s) {
				break;
			}
			if (byte >= 0xE0) {
				s++;
			}
			if (!*s) {
				break;
			}
			if (byte >= 0xF0) {
				s++;
				if (++count >= PATH_MAX) {
					goto fail;
				}
			}
		}
	}
	ret = count;
out:
	return ret;
fail:
	errno = ENAMETOOLONG;
	goto out;
}

static int utf8_to_unicode(uint32_t *wc, const char *s)
{
	unsigned int byte = *((const unsigned char *)s);

					/* single byte */
	if (byte == 0) {
		*wc = (uint32_t) 0;
		return 0;
	} else if (byte < 0x80) {
		*wc = (uint32_t) byte;
		return 1;
					/* double byte */
	} else if (byte < 0xC2) {
		goto fail;
	} else if (byte < 0xE0) {
		if ((s[1] & 0xC0) == 0x80) {
			*wc = ((uint32_t)(byte & 0x1F) << 6)
			    | ((uint32_t)(s[1] & 0x3F));
			return 2;
		} else {
			goto fail;
		}
					/* three-byte */
	} else if (byte < 0xF0) {
		if (((s[1] & 0xC0) == 0x80) && ((s[2] & 0xC0) == 0x80)) {
			*wc = ((uint32_t)(byte & 0x0F) << 12)
			    | ((uint32_t)(s[1] & 0x3F) << 6)
			    | ((uint32_t)(s[2] & 0x3F));
			/* Check valid ranges */
			if (((*wc >= 0x800) && (*wc <= 0xD7FF))
			  || ((*wc >= 0xe000) && (*wc <= 0xFFFF)))
			{
				return 3;
			}
		}
		goto fail;
					/* four-byte */
	} else if (byte < 0xF5) {
		if (((s[1] & 0xC0) == 0x80) && ((s[2] & 0xC0) == 0x80)
		  && ((s[3] & 0xC0) == 0x80))
		{
			*wc = ((uint32_t)(byte & 0x07) << 18)
			    | ((uint32_t)(s[1] & 0x3F) << 12)
			    | ((uint32_t)(s[2] & 0x3F) << 6)
			    | ((uint32_t)(s[3] & 0x3F));
			/* Check valid ranges */
			if ((*wc <= 0x10ffff) && (*wc >= 0x10000)) {
				return 4;
			}
		}
		goto fail;
	}
fail:
	errno = EILSEQ;
	return -1;
}

static int ntfs_utf8_to_utf16(const char *ins, uint16_t **outs)
{
	int ret = -1;
	int shorts;
	const char *t = ins;
	uint32_t wc;
	uint16_t *outpos;

	shorts = utf8_to_utf16_size(ins);
	if (shorts < 0) {
		goto fail;
	}

	outpos = *outs;

	while(1) {
		int m  = utf8_to_unicode(&wc, t);
		if (m <= 0) {
			if (m < 0) {
				goto fail;
			}
			*outpos++ = OSSwapHostToLittleConstInt16(0);
			break;
		}
		if (wc < 0x10000) {
			*outpos++ = OSSwapHostToLittleInt16(wc);
		}
		else {
			wc -= 0x10000;
			*outpos++ =
				OSSwapHostToLittleInt16((wc >> 10) + 0xd800);
			*outpos++ =
				OSSwapHostToLittleInt16((wc & 0x3ff) + 0xdc00);
		}
		t += m;
	}

	ret = --outpos - *outs;
fail:
	return ret;
}
#endif /* defined(__APPLE__) ... */

size_t
utf8_encodelen(const u_int16_t * ucsp, size_t ucslen, u_int16_t altslash,
    int flags)
{
#ifdef __APPLE__
	int err;
#endif
	size_t utf8len = 0;

	(void)altslash; // Not used by the NTFS code.

#ifdef __APPLE__
	err = utf8_transcode(1, ucsp, ucslen, NULL, &utf8len, 0, flags);
	if (err) {
		xpl_perror(err, "Error while getting encoded string length");
	}
#else
	(void)flags; /* TODO: Currently ignored for non-macOS platforms. */
	utf8len = utf16_to_utf8_size(ucsp, ucslen / 2, INT_MAX);
	if (utf8len < 0) {
		xpl_perror(errno, "Error while getting encoded string length");
		utf8len = 0;
	}
#endif

	return utf8len;
}

int
utf8_encodestr(const u_int16_t * ucsp, size_t ucslen, u_int8_t * utf8p,
    size_t * utf8len, size_t buflen, u_int16_t altslash, int flags)
{
#ifndef __APPLE__
	int err;
	int ret;
	u_int8_t *utf8p_orig;
#endif

	(void)altslash; // Not used by the NTFS code.

#ifdef __APPLE__
	return utf8_transcode(1, ucsp, ucslen, utf8p, utf8len, buflen, flags);
#else
	(void)flags; /* TODO: Currently ignored for non-macOS platforms. */

	utf8p_orig = utf8p;
	ret = ntfs_utf16_to_utf8(ucsp, ucslen / 2, (char**)&utf8p, buflen);
	if (ret < 0) {
		size_t i;
		err = (err = errno) ? err : ENOMEM;
		xpl_perror(errno, "Error while converting %zu-byte UTF-16 "
			"string to UTF-8", ucslen);
		for(i = 0; i < ucslen; i += 2) {
			xpl_info("  [%zu]: 0x%04X ('%c')",
				i / 2,
				ucsp[i / 2],
				(ucsp[i / 2] < 0x7F) ? ucsp[i / 2] : '?');
		}
	}
	else if (utf8p != utf8p_orig) {
		xpl_error("Unexpectedly reallocated utf8p: %p != %p",
			utf8p, utf8p_orig);
		err = EINVAL;
	}
	else {
		*utf8len = (size_t)ret;
		err = 0;
	}

	return err;
#endif
}

int
utf8_decodestr(const u_int8_t* utf8p, size_t utf8len, u_int16_t* ucsp,
    size_t *ucslen, size_t buflen, u_int16_t altslash, int flags)
{
#ifndef __APPLE__
	int err;
	int ret;
	u_int16_t *ucsp_orig;
#endif

	(void)altslash; // Not used by the NTFS code.

#ifdef __APPLE__
	return utf8_transcode(0, utf8p, utf8len, ucsp, ucslen, buflen, flags);
#else
	(void)flags; /* TODO: Currently ignored for non-macOS platforms. */

	if (buflen < utf8len * 2) {
		/* This could potentially overflow, so throw an error here to be
		 * safe. */
		xpl_error("Too small output buffer: %zu < %zu",
			buflen, utf8len * 2);
		return ERANGE;
	}

	ucsp_orig = ucsp;
	ret = ntfs_utf8_to_utf16((const char*)utf8p, &ucsp);
	if (ret < 0) {
		err = (err = errno) ? err : ENOMEM;
		xpl_perror(errno, "Error while converting %zu-byte UTF-8 "
			"string to UTF-16", utf8len);
	}
	else if (ucsp != ucsp_orig) {
		xpl_error("Unexpectedly reallocated ucsp: %p != %p",
			ucsp, ucsp_orig);
		err = EINVAL;
	}
	else {
		*ucslen = ((size_t)ret) * 2;
		err = 0;
	}

	return err;
#endif
}
/* </Header: sys/utfconv.h> */

/* <Header: mach/memory_object_types.h> */

boolean_t        upl_dirty_page(upl_page_info_t *upl, int index)
{
	xpl_trace_enter("upl=%p index=%d", upl, index);

	/* TODO: This needs to be implemented for write support. */
	return FALSE;
}

boolean_t        upl_valid_page(upl_page_info_t *upl, int index)
{
	xpl_trace_enter("upl=%p index=%d", upl, index);
	return upl->valid;
}

/* </Header: mach/memory_object_types.h> */

/* <Header: sys/ubc.h> */

off_t   ubc_getsize(struct vnode *vp)
{
	xpl_trace_enter("vp=%p", vp);
	return vp->datasize;
}

int ubc_setsize(vnode_t vp, off_t nsize)
{
	xpl_trace_enter("vp=%p nsize=%lld", vp, (long long)nsize);

	/* TODO: This needs to be implemented for write support. */
	return ENOTSUP;
}

errno_t
ubc_msync(vnode_t vp, off_t beg_off, off_t end_off, off_t *resid_off, int flags)
{
	xpl_trace_enter("vp=%p beg_off=%lld end_off=%lld resid_off=%p "
		"flags=0x%X",
		vp, (long long)beg_off, (long long)end_off, resid_off, flags);
	
	/* TODO: This needs to be implemented for write support. */
	return ENOTSUP;
}

/* cluster IO routines */

int
cluster_read(vnode_t vp, struct uio *uio, off_t filesize, int xflags)
{
	xpl_trace_enter("vp=%p uio=%p filesize=%lld xflags=0x%X",
		vp, uio, (long long)filesize, xflags);
	return cluster_read_ext(vp, uio, filesize, xflags, NULL, NULL);
}
int
cluster_read_ext(vnode_t vp, struct uio *uio, off_t filesize, int xflags, int (*callback)(buf_t, void *), void *callback_arg)
{
	int err = 0;

	int (*blockmap_op)(struct vnop_blockmap_args *a) = NULL;

	off_t offset;
	size_t size;
	size_t count = 0;

	xpl_trace_enter("vp=%p uio=%p filesize=%" PRId64 " xflags=0x%X "
			"callback=%p callback_arg=%p",
			vp, uio, filesize, xflags, callback, callback_arg);

	if (callback) {
		xpl_error("TODO: Implement for reads with callbacks.");
		err = ENOTSUP;
		goto out;
	}

	blockmap_op = (int (*)(struct vnop_blockmap_args *a)) ops.vnop_blockmap;

	offset = uio->offset;
	if (uio->offset >= filesize) {
		size = 0;
	}
	else if (uio->resid > filesize - uio->offset) {
		size = filesize - uio->offset;
	}
	else {
		size = uio->resid;
	}

	while (count < size) {
		struct vnop_blockmap_args blockmap_args;
		daddr64_t bpn;
		size_t run;
		ssize_t res;

		memset(&blockmap_args, 0, sizeof(blockmap_args));
		blockmap_args.a_vp = vp;
		blockmap_args.a_foffset = offset;
		blockmap_args.a_size = size - count;
		blockmap_args.a_bpn = &bpn;
		blockmap_args.a_run = &run;
		blockmap_args.a_poff = NULL;
		blockmap_args.a_flags = VNODE_READ;

		err = blockmap_op(&blockmap_args);
		if (err) {
			xpl_perror(err, "Error from blockmap operation");
			goto out;
		}

		xpl_debug("bpn: %" PRId64, bpn);
		xpl_debug("run: %zu", run);

		if (run > size - count) {
			/* Don't overflow. */
			run = size - count;
			xpl_debug("run -> %zu", run);
		}

		/* Transfer the data from the special device vnode, which we
		 * know has a file descriptor as fsnode. */
		xpl_debug("fd=%d", (int)(uintptr_t)vp->mp->devvn.fsnode);
		xpl_debug("ptr=%p",
			&((char*)(uintptr_t)uio->iov_baseaddr)[count]);

		if (bpn != -1) {
			const size_t aligned_run =
				(run + vp->mp->blocksize_mask) &
				~vp->mp->blocksize_mask;
			void *const uiobuf =
				&((char*)(uintptr_t)uio->iov_baseaddr)[count];

			void *tmpbuf = NULL;
			void *buf;

			/* With uio_t buffers we can't assume that there's extra
			 * space for alignment padding. So if the run is
			 * misaligned we have to deal with it by reading into a
			 * temporary buffer. */
			if(aligned_run != run) {
				xpl_debug("Allocating %zu-byte alignment "
						"buffer for %zu-byte run at "
						"bpn %" PRId64 " (byte offset "
						"%" PRId64 ")...",
						aligned_run, run, bpn,
						bpn * vp->mp->blocksize);
				tmpbuf = valloc(aligned_run);
				if (!tmpbuf) {
					err = (err = errno) ? err : EIO;
					xpl_perror(errno, "Error while "
							"allocating %zu-byte "
							"alignment buffer for "
							"%zu-byte run at bpn "
							"%" PRId64 " (byte "
							"offset %" PRId64 ")",
							aligned_run, run, bpn,
							bpn * vp->mp->
							blocksize);
					goto out;
				}

				buf = tmpbuf;
			}
			else {
				buf = uiobuf;
			}

#ifdef XPL_IO_DEBUG
			xpl_info("Reading %zu bytes from %lld...",
					aligned_run, bpn * vp->mp->blocksize);
#endif
			res = xpl_pread(&vp->mp->devvn, buf, aligned_run,
					 bpn * vp->mp->blocksize);
			if (tmpbuf && res > 0) {
				if (res > run) {
					res = run;
				}

				memcpy(uiobuf, tmpbuf, res);
				free(tmpbuf);
			}
			if (res < 0) {
				err = (err = errno) ? err : EIO;
				xpl_perror(errno, "Error while reading %zu "
						"bytes from device offset "
						"%" PRId64,
						run, bpn * vp->mp->blocksize);
				goto out;
			}
		}
		else {
			memset(&((char*)(uintptr_t)uio->iov_baseaddr)[count], 0,
					run);
			res = run;
		}

		xpl_debug("Advancing foffset: %lld -> %lld",
				offset, offset + res);
		offset += res;
		xpl_debug("Advancing count: %zu -> %zu",
				count, count + (size_t) res);
		count += res;
	}

	uio_setresid(uio, size - count);

out:
	return err;
}

int
cluster_write_ext(vnode_t vp, struct uio *uio, off_t oldEOF, off_t newEOF, off_t headOff, off_t tailOff,
		  int xflags, int (*callback)(buf_t, void *), void *callback_arg)
{
	xpl_trace_enter("vp=%p uio=%p oldEOF=%lld newEOF=%lld headOff=%lld "
		"tailOff=%lld xflags=0x%X callback=%p",
		vp, uio, (long long)oldEOF, (long long)newEOF,
		(long long)headOff, (long long)tailOff, xflags, callback);

	/* TODO: This needs to be implemented for write support. */
	return ENOTSUP;
}

int
cluster_pageout_ext(vnode_t vp, upl_t upl, upl_offset_t upl_offset, off_t f_offset,
		int size, off_t filesize, int flags, int (*callback)(buf_t, void *), void *callback_arg)
{
	xpl_trace_enter("vp=%p upl=%p upl_offset=%lu f_offset=%lld size=%d "
		"filesize=%lld flags=0x%X callback=%p",
		vp, upl, (unsigned long)upl_offset, (long long)f_offset, size,
		(long long)filesize, flags, callback);

	/* TODO: This needs to be implemented for write support. */
	return ENOTSUP;
}

int
cluster_pagein_ext(vnode_t vp, upl_t upl, upl_offset_t upl_offset, off_t f_offset,
	       int size, off_t filesize, int flags, int (*callback)(buf_t, void *), void *callback_arg)
{
	int err = 0;

	int (*blockmap_op)(struct vnop_blockmap_args *a) = NULL;

	uint32_t read_size;
	int count = 0;
	buf_t buf;

	xpl_trace_enter("vp=%p upl=%p upl_offset=%" PRIu32 " "
			"offset=%" PRId64 " size=%d filesize=%" PRId64 " "
			"flags=0x%X callback=%p callback_arg=%p",
			vp, upl, upl_offset, f_offset, size, filesize, flags,
			callback, callback_arg);

	blockmap_op = (int (*)(struct vnop_blockmap_args *a)) ops.vnop_blockmap;
	buf = upl->buf;
	read_size =
		(buf->size < filesize - f_offset) ? buf->size :
		(uint32_t) (filesize - f_offset);

	while (buf->count < read_size) {
		struct vnop_blockmap_args blockmap_args;
		daddr64_t bpn;
		size_t run;
		ssize_t res;

		memset(&blockmap_args, 0, sizeof(blockmap_args));
		blockmap_args.a_vp = vp;
		blockmap_args.a_foffset = f_offset;
		blockmap_args.a_size = size - count;
		blockmap_args.a_bpn = &bpn;
		blockmap_args.a_run = &run;
		blockmap_args.a_poff = NULL;
		blockmap_args.a_flags = VNODE_READ;

		err = blockmap_op(&blockmap_args);
		if (err) {
			xpl_perror(err, "Error from blockmap operation");
			goto out;
		}

		xpl_debug("bpn: %" PRId64, bpn);
		xpl_debug("run: %zu", run);

		if (run > read_size - buf->count) {
			/* Don't overflow. */
			run = read_size - buf->count;
		}

		/* Transfer the data from the special device vnode, which we
		 * know has a file descriptor as fsnode. */
		xpl_debug("fd=%d", (int)(uintptr_t)vp->mp->devvn.fsnode);
		if (bpn != -1) {
			const size_t aligned_run =
				/* We enforce page-aligned buffers in buf_t,
				 * meaning we always have space to read a full
				 * sector (assuming sector size <= page size,
				 * which seems to be a reasonable assumption).
				 * This is both more efficient and often
				 * required (direct I/O raw devices, etc.).
				 * XXX: Note that once we have a proper page
				 * cache we can just use that instead. */
				(run + vp->mp->blocksize_mask) &
				~vp->mp->blocksize_mask;
#ifdef XPL_IO_DEBUG
			xpl_info("Reading %zu (->%zu) bytes from %lld...",
					run, aligned_run,
					bpn * vp->mp->blocksize);
#endif
			res = xpl_pread(&vp->mp->devvn, &buf->buf[buf->count],
					aligned_run, bpn * vp->mp->blocksize);
			if (res < 0) {
				err = (err = errno) ? err : EIO;
				xpl_perror(errno, "Error while reading %zu "
						"bytes from device offset "
						"%" PRId64,
						run, bpn * vp->mp->blocksize);
				goto out;
			}
			else if(res > run) {
				res = run;
			}
		}
		else {
			memset(&buf->buf[buf->count], 0, run);
			res = run;
		}

		xpl_debug("Advancing foffset: %lld -> %lld",
				f_offset, f_offset + res);
		f_offset += res;
		xpl_debug("Advancing buf->count: %" PRIu32 " -> %" PRIu32,
				buf->count, (uint32_t)(buf->count + res));
		buf->count += res;
		count += res;
	}

	if (callback) {
		callback(buf, callback_arg);
	}
out:
	return err;
}

int
cluster_push_ext(vnode_t vp, int flags, int (*callback)(buf_t, void *), void *callback_arg)
{
	xpl_trace_enter("vp=%p flags=0x%X callback=%p", vp, flags, callback);

	/* TODO: This needs to be implemented for write support. */
	return ENOTSUP;
}

int
cluster_copy_ubc_data(vnode_t vp, struct uio *uio, int *io_resid, int mark_dirty)
{
	xpl_trace_enter("vp=%p uio=%p io_resid=%p mark_dirty=%d",
		vp, uio, io_resid, mark_dirty);

	/* This function is supposed to copy from the UBC cache the data that is
	 * available without having to do I/O. At this time we simply return 0
	 * with unchanged io_resid to simulate a cache miss meaning the code has
	 * to explicitly trigger a pagein to get the data. */
	if (io_resid) {
		if (uio->resid > INT_MAX) {
			xpl_debug("uio resid is too large to be stored in an "
				"int: %lld", uio->resid);
			return ERANGE;
		}

		*io_resid = (int)MIN(vp->datasize - uio->offset, uio->resid);
	}

	return 0;
}

/* UPL routines */
int     ubc_create_upl(
	struct vnode	*vp,
	off_t 		f_offset,
	int		bufsize,
	upl_t		*uplp,
	upl_page_info_t	**plp,
	int		uplflags)
{
	int res = KERN_SUCCESS;
	size_t pages;
	size_t allocsize;
	struct xpl_upl *upl = NULL;
	buf_t buf = NULL;

	xpl_trace_enter("vp=%p f_offset=%lld bufsize=%d uplp=%p plp=%p "
		"uplflags=%d",
		vp, f_offset, bufsize, uplp, plp, uplflags);

	pages = (bufsize + PAGE_SIZE - 1) / PAGE_SIZE;
	allocsize = sizeof(struct xpl_upl) + pages * sizeof(upl->pagelist[0]);
	upl = malloc(allocsize);
	if (!upl) {
		xpl_perror(errno, "Error while allocating memory for upl");
		res = KERN_FAILURE;
		goto out;
	}

	memset(upl, 0, allocsize);
	upl->pages = pages;

	buf = _buf_alloc(vp, bufsize);
	if (!buf) {
		res = KERN_FAILURE;
		goto out;
	}

	buf->flags = B_READ;
	upl->buf = buf;
	{
		size_t i;

		for (i = 0; i < pages; ++i) {
			upl_page_info_t *pl = &upl->pagelist[i].pl;
			pl->valid = 0;
			pl->dumped = 0;
		}
	}

	*uplp = upl;
	*plp = &upl->pagelist[0].pl;
	upl = NULL;
out:
	if (upl) {
		if (upl->buf) {
			buf_brelse(upl->buf);
		}

		free(upl);
	}

	return res;
}

int     ubc_upl_map(
	upl_t		upl,
	vm_offset_t	*dst_addr)
{
	xpl_trace_enter("upl=%p dst_addr=%p", upl, dst_addr);

	/* "mapping" in XPL simply means returning a pointer to an allocated
	 * buffer. We have only one address space. */
	*dst_addr = (vm_offset_t) upl->buf->buf;
	return 0;
}

int     ubc_upl_unmap(
	upl_t	upl)
{
	xpl_trace_enter("upl=%p", upl);

	/* Unmapping a page is a no-op in XPL since "mapping" simply means
	 * returning a pointer to an allocated buffer. */
	return 0;
}

int     ubc_upl_commit_range(
	upl_t 			upl,
	upl_offset_t		offset,
	upl_size_t		size,
	int				flags)
{
	xpl_trace_enter("upl=%p offset=%" PRIu32 " size=%" PRIu32 " flags=%d",
			upl, offset, size, flags);
	/* TODO: Write back pages as part of implementing write support. */
	if (upl->buf) {
		buf_brelse(upl->buf);
		upl->buf = NULL;
	}

	free(upl);

	return 0;
}

int     ubc_upl_abort_range(
	upl_t			upl,
	upl_offset_t		offset,
	upl_size_t		size,
	int				abort_flags)
{
	upl_offset_t page_start;
	upl_offset_t page_end;
	size_t page;
	int empty;

	xpl_trace_enter("upl=%p offset=%" PRIu32 " size=%" PRIu32 " "
			"abort_flags=%d",
			upl, offset, size, abort_flags);

	page_start = offset / PAGE_SIZE;
	page_end = (offset + size + PAGE_SIZE - 1) / PAGE_SIZE;

	if (page_start >= upl->pages || page_end > upl->pages) {
		xpl_error("Range is beyond allocated page list: offset=%lld "
			"(page %lld) offset+size=%lld (page %lld)",
			(long long)offset, (long long)page_start,
			(long long)(offset + size), (long long)page_end);
		return EINVAL;
	}

	empty = 1;
	for (page = 0; page < upl->pages; ++page) {
		if ((abort_flags & UPL_ABORT_DUMP_PAGES) &&
			page >= page_start && page < page_end)
		{
			upl->pagelist[page].pl.dumped = 1;
		}

		if (!upl->pagelist[page].pl.dumped) {
			empty = 0;
		}
	}

	if (empty && (abort_flags & UPL_ABORT_FREE_ON_EMPTY)) {
		if (upl->buf) {
			buf_brelse(upl->buf);
			upl->buf = NULL;
		}

		free(upl);
	}

	return 0;
}

upl_size_t
ubc_upl_maxbufsize(
	void)
{
	xpl_trace_enter("");
	return(MAX_UPL_SIZE_BYTES);
}

/* </Header: sys/ubc.h> */

/* <Header: sys/time.h> */

void    microuptime(struct timeval *tv)
{
	struct timespec ts;
	xpl_trace_enter("tv=%p", tv);
	clock_gettime(CLOCK_UPTIME_RAW, &ts);
	tv->tv_sec = ts.tv_sec;
	tv->tv_usec = (int)(ts.tv_nsec / 1000);
}

void    nanotime(struct timespec *ts)
{
	xpl_trace_enter("ts=%p", ts);
	clock_gettime(CLOCK_REALTIME, ts);
}

/* </Header: sys/time.h> */

/* <Header: sys/proc.h> */

extern int proc_selfpid(void)
{
	xpl_trace_enter("");
	return getpid();
}

/* </Header: sys/proc.h> */

/* <Header: sys/xattr.h> */

/* Copied from: bsd/vfs/vfs_xattr.c */
/*
 * Determine whether an EA is a protected system attribute.
 */
int
xattr_protected(const char *attrname)
{
	xpl_trace_enter("attrname=%p (\"%s\")", attrname, attrname);
	return(!strncmp(attrname, "com.apple.system.", 17));
}

/* </Header: sys/xattr.h> */

/* <Header: sys/vnode_if.h> */

errno_t VNOP_IOCTL(vnode_t vp, u_long command, caddr_t data, int fflag,
		vfs_context_t ctx)
{
	/* We know that this will only be called for the mount, having a file
	 * descriptor as 'fsnode'. */
	int err = 0;
	int fd;
	struct stat st;

	xpl_trace_enter("vp=%p command=0x%lX data=%p fflag=0x%X ctx=%p",
			vp, command, data, fflag, ctx);

	if (!vp->vnode_external) {
		xpl_error("Invalid internal vnode %p for VNOP_IOCTL.", vp);
		err = EINVAL;
		goto out;
	}

	fd = (int)(uintptr_t)vp->fsnode;
	xpl_debug("fd=%d", fd);

	if (fstat(fd, &st)) {
		xpl_perror(errno, "Error during fstat for fd=%d", fd);
	}
	else if ((st.st_mode & S_IFMT) == S_IFREG) {
		/* For files we emulate the required ioctl calls to make them
		 * behave like a device. */
		switch (command) {
		case DKIOCGETBLOCKSIZE:
			*((uint32_t*)data) = vp->mp->blocksize;
			goto out;
		case DKIOCSETBLOCKSIZE:
			if (*(uint32_t*)data != vp->mp->blocksize) {
				err = EINVAL;
				goto out;
			}
			/* No-op. */
			goto out;
		case DKIOCGETBLOCKCOUNT:
			*((uint64_t*)data) = st.st_size / vp->mp->blocksize;
			goto out;
		default:
			panic("Unimplemented file ioctl: 0x%lX", command);
			err = ENOTSUP;
			goto out;
		}
	}
	else {
		xpl_debug("Non-file mode: 0x%X", st.st_mode & S_IFMT);
	}

	if (ioctl(fd, command, data)) {
		uint32_t blocksize = 0;

		err = errno;
		if (command == DKIOCSETBLOCKSIZE &&
			!ioctl(fd, DKIOCGETBLOCKSIZE, &blocksize) &&
			blocksize == *((uint32_t*)data))
		{
			/* Block size is actually what we were trying to set it
			 * to, so this is fine. */
			err = 0;
			goto out;
		}

		err = err ? err : EIO;
	}
out:
	return err;
}

/* </Header: sys/vnode_if.h> */

/* <Header: sys/systm.h> */

void    set_fsblocksize(struct vnode *vn)
{
	xpl_trace_enter("vn=%p", vn);

	/* TODO: Unclear if any action is needed here. */
}

/* </Header: sys/systm.h> */
