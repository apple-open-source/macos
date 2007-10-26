/*
 * ntfs_inode.c - NTFS kernel inode operations.
 *
 * Copyright (c) 2006, 2007 Anton Altaparmakov.  All Rights Reserved.
 * Portions Copyright (c) 2006, 2007 Apple Inc.  All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer. 
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution. 
 * 3. Neither the name of Apple Inc. ("Apple") nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * ALTERNATIVELY, provided that this notice and licensing terms are retained in
 * full, this file may be redistributed and/or modified under the terms of the
 * GNU General Public License (GPL) Version 2, in which case the provisions of
 * that version of the GPL will apply to you instead of the license terms
 * above.  You can obtain a copy of the GPL Version 2 at
 * http://developer.apple.com/opensource/licenses/gpl-2.txt.
 */

#include <sys/cdefs.h>

#include <sys/errno.h>
#include <sys/kernel_types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/ucred.h>
#include <sys/ubc.h>
#include <sys/vnode.h>
/*
 * struct nameidata is defined in <sys/namei.h>, but it is private so put in a
 * forward declaration for now so that vnode_internal.h can compile.  All we
 * need vnode_internal.h for is the declaration of vnode_getparent(),
 * vnode_getname(), and vnode_putname(),  which are exported in
 * Unsupported.exports.
 */
// #include <sys/namei.h>
struct nameidata;
#include <sys/vnode_internal.h>

#include <string.h>

#include <libkern/libkern.h>
#include <libkern/OSAtomic.h>
#include <libkern/OSMalloc.h>

#include <kern/debug.h>
#include <kern/locks.h>

#include <mach/machine/vm_param.h>

#include "ntfs.h"
#include "ntfs_attr.h"
#include "ntfs_debug.h"
#include "ntfs_dir.h"
#include "ntfs_hash.h"
#include "ntfs_inode.h"
#include "ntfs_mft.h"
#include "ntfs_runlist.h"
#include "ntfs_time.h"
#include "ntfs_types.h"
#include "ntfs_volume.h"
#include "ntfs_vnops.h"

/**
 * ntfs_inode_test - compare two (possibly fake) ntfs inodes for equality
 * @ni:		ntfs inode which to test
 * @na:		ntfs attribute which is being tested with
 *
 * Compare the ntfs attribute embedded in the ntfs inode @ni for equality with
 * the ntfs attribute @na.
 *
 * If searching for the normal file/directory inode, set @na->type to AT_UNUSED.
 * @na->name and @na->name_len are then ignored.
 *
 * Return TRUE if the attributes match and FALSE if not.
 *
 * Locking: Caller must hold the @ntfs_inode_hash_lock.
 */
BOOL ntfs_inode_test(ntfs_inode *ni, const ntfs_attr *na)
{
	if (ni->mft_no != na->mft_no)
		return FALSE;
	/* If !NInoAttr(ni), @ni is a normal file or directory inode. */
	if (!NInoAttr(ni)) {
		/* If not looking for a normal inode this is a mismatch. */
		if (na->type != AT_UNUSED)
			return FALSE;
	} else {
		/* A fake inode describing an attribute. */
		if (ni->type != na->type)
			return FALSE;
		if (ni->name_len != na->name_len)
			return FALSE;
		if (na->name_len && bcmp(ni->name, na->name,
				na->name_len * sizeof(ntfschar)))
			return FALSE;
	}
	/*
	 * If looking for raw inode but found non-raw one or looking for
	 * non-raw inode and found raw one this is a mismatch.
	 */
	if ((BOOL)NInoRaw(ni) != na->raw)
		return FALSE;
	/* Match! */
	return TRUE;
}

/**
 * __ntfs_inode_init - initialize an ntfs inode
 * @vol:	ntfs volume to which @ni belongs.
 * @ni:		ntfs inode to initialize
 *
 * Initialize the ntfs inode @ni to defaults.
 *
 * NOTE: ni->mft_no, ni->flags, ni->type, ni->name, and ni->name_len are left
 * untouched.  Make sure to initialize them elsewhere.
 */
static inline void __ntfs_inode_init(ntfs_volume *vol, ntfs_inode *ni)
{
	ni->vol = vol;
	ni->vn = NULL;
	ni->nr_refs = 0;
	lck_rw_init(&ni->lock, ntfs_lock_grp, ntfs_lock_attr);
	/*
	 * By default do i/o in sectors.  This for example gets overridden for
	 * mst protected attributes for which the size is set to the ntfs
	 * record size being protected by the mst fixups.
	 */
	ni->block_size = vol->sector_size;
	ni->block_size_shift = vol->sector_size_shift;
	lck_spin_init(&ni->size_lock, ntfs_lock_grp, ntfs_lock_attr);
	ni->allocated_size = ni->data_size = ni->initialized_size = 0;
	ni->seq_no = 0;
	ni->link_count = 0;
	ni->uid = vol->uid;
	ni->gid = vol->gid;
	ni->mode = 0;
	ni->file_attributes = 0;
	ni->creation_time = ni->last_data_change_time =
			ni->last_mft_change_time =
			ni->last_access_time = (struct timespec) {
		.tv_sec = 0,
		.tv_nsec = 0,
	};
	ntfs_rl_init(&ni->rl);
	lck_mtx_init(&ni->mrec_lock, ntfs_lock_grp, ntfs_lock_attr);
	ni->mrec_upl = NULL;
	ni->mrec_pl = NULL;
	ni->mrec_kaddr = NULL;
	ni->attr_list_size = 0;
	ni->attr_list = NULL;
	ntfs_rl_init(&ni->attr_list_rl);
	ni->compressed_size = 0;
	ni->compression_block_size = 0;
	ni->compression_block_size_shift = 0;
	ni->compression_block_clusters = 0;
	lck_mtx_init(&ni->extent_lock, ntfs_lock_grp, ntfs_lock_attr);
	ni->nr_extents = 0;
	ni->base_ni = NULL;
}

/**
 * ntfs_inode_init - initialize an ntfs inode
 * @vol:	ntfs volume to which @ni belongs.
 * @ni:		ntfs inode to initialize
 * @na:		ntfs attribute which to initialize @ni to
 *
 * Initialize the ntfs inode @ni with the values from the ntfs attribute @na in
 * order to enable ntfs_inode_test() to do its work.
 *
 * If initializing the normal file/directory inode, set @na->type to AT_UNUSED.
 * In that case, @na->name and @na->name_len should be set to NULL and 0,
 * respectively.  Although that is not strictly necessary as ntfs_inode_read()
 * will fill them in later.
 *
 * Return 0 on success and errno on error.
 *
 * The only defined error code is ENOMEM.
 */
errno_t ntfs_inode_init(ntfs_volume *vol, ntfs_inode *ni, const ntfs_attr *na)
{
	ni->flags = (1 << NI_Locked) | (1 << NI_Alloc);
	ni->mft_no = na->mft_no;
	ni->type = na->type;
	if (na->type == AT_INDEX_ALLOCATION)
		NInoSetMstProtected(ni);
	ni->name = na->name;
	ni->name_len = na->name_len;
	if (na->raw)
		NInoSetRaw(ni);
	__ntfs_inode_init(vol, ni);
	/* If initializing a normal inode, we are done. */
	if (na->type == AT_UNUSED)
		return 0;
	/* It is a fake inode. */
	NInoSetAttr(ni);
	/*
	 * We have I30 global constant as an optimization as it is the name
	 * in >99.9% of named attributes!  The other <0.1% incur an allocation
	 * but that is ok.  And most attributes are unnamed anyway, thus the
	 * fraction of named attributes with name != I30 is actually absolutely
	 * tiny.
	 */
	if (na->name_len && na->name != I30) {
		unsigned i = na->name_len * sizeof(ntfschar);
		ni->name = OSMalloc(i + sizeof(ntfschar), ntfs_malloc_tag);
		if (!ni->name)
			return ENOMEM;
		memcpy(ni->name, na->name, i);
		ni->name[na->name_len] = 0;
	}
	return 0;
}

static errno_t ntfs_inode_read(ntfs_inode *ni);
static errno_t ntfs_attr_inode_read(ntfs_inode *base_ni, ntfs_inode *ni);
static errno_t ntfs_index_inode_read(ntfs_inode *base_ni, ntfs_inode *ni);

/**
 * ntfs_inode_get_vtype - return the vtype of an ntfs inode
 * @ni:		ntfs inode whose vtype to return
 *
 * Figure out the vtype of the ntfs inode @ni and return it.
 *
 * Valid vtypes are:
 *	VNON = No type.
 *	VREG = Regular file.
 *	VDIR = Directory.
 *	VBLK = Block device.
 *	VCHR = Character device.
 *	VLNK = Symbolic link.
 *	VSOCK = Socket.
 *	VFIFO = Named pipe / fifo.
 *	VBAD = Dead vnode.
 *	VSTR = Not used in current OSX kernel.
 *	VCPLX = Not used in current OSX kernel.
 */
static inline enum vtype ntfs_inode_get_vtype(ntfs_inode *ni)
{
	/*
	 * Attribute inodes do not really have a type.
	 *
	 * However, the current OSX kernel does not allow use of ubc with
	 * anything other than regular files (i.e. VREG vtype), thus we need to
	 * return VREG for named $DATA attributes, i.e. named streams, so that
	 * they can be accessed via mmap like regular files.  And the same goes
	 * for index inodes which we need to be able to read via the ubc.
	 *
	 * And a further however is that ntfs_unmount() uses vnode_iterate() to
	 * flush all inodes of the mounted of volume and vnode_iterate() skips
	 * over all VNON vnodes, thus we cannot have any vnodes marked VNON or
	 * unmounting would fail.  (Note we cannote use vflush() instead of
	 * vnode_iterate() because vflush() calls vnode_umount_preflight()
	 * which in turn aborts the vflush() if any vnodes are busy and in our
	 * case we want to evict the non-system vnodes only thus the system
	 * vnodes are busy thus vflush() is aborted from the preflight call.)
	 */
	if (NInoAttr(ni))
		return VREG;
	/*
	 * Not an attribute inode, thus the mode will be a proper POSIX mode,
	 * which we just need to convert to V*** type.
	 */
	return IFTOVT(ni->mode);
}

/**
 * ntfs_inode_add_vnode - create and attach a vnode to an ntfs inode
 * @ni:		ntfs inode to which to attach a new vnode
 * @is_system:	true if @ni is a system inode and false otherwise
 * @parent_vn:	vnode of directory containing @ni or NULL
 * @cn:		componentname containing the name of @ni or NULL
 *
 * Create a new vnode for the ntfs inode @ni and attach it to the ntfs inode.
 * If @is_system is true the created vnode is marked as a system vnode (via the
 * VSYSTEM flag).
 *
 * If @parent_vn is not NULL, set it up as the parent directory vnode of the
 * newly created vnode.
 *
 * If @cn is not NULL, set it up as the name of the newly created vnode and
 * optionally enter the name in the name cache.
 *
 * Return 0 on success and errno on error.
 */
errno_t ntfs_inode_add_vnode(ntfs_inode *ni, const BOOL is_system,
		vnode_t parent_vn, struct componentname *cn)
{
	struct vnode_fsparam vn_fsp;
	errno_t err;
	enum vtype vtype = ntfs_inode_get_vtype(ni);
	BOOL cache_name = FALSE;

	ntfs_debug("Entering.");
	vn_fsp = (struct vnode_fsparam) {
		.vnfs_mp = ni->vol->mp,	/* Mount of volume. */
		.vnfs_vtype = vtype,	/* Vnode type. */
		.vnfs_str = "ntfs",	/* Debug aid. */
		.vnfs_dvp = parent_vn,	/* Parent directory vnode. */
		.vnfs_fsnode = ni,	/* Ntfs inode to attach to the vnode. */
		.vnfs_vops = ntfs_vnodeop_p, /* Operations for this vnode. */
		.vnfs_markroot = ((ni->mft_no != FILE_root) || NInoAttr(ni)) ?
				0 : 1,	/* Is this the ntfs volume root? */
		.vnfs_marksystem = is_system ? 1 : 0, /* Mark the vnode as
					   VSYSTEM if this is a system inode. */
		.vnfs_rdev = (dev_t)0,	/* Device if vtype is VBLK or VCHR. */
		.vnfs_filesize = ni->data_size,	/* Data size of attribute.  No
					   need for size lock as we are only
					   user of inode at present. */
		.vnfs_cnp = cn,		/* Component name to assign as the name
					   of the vnode and optionally to add
					   it to the namecache. */
		.vnfs_flags = VNFS_ADDFSREF, /* VNFS_* flags.   We want to have
					   an fs reference on the vnode. */
	};
	/*
	 * If the name is not meant to be cached cause vnode_create() not to
	 * add it to the name cache.
	 */
	if (cn && cn->cn_flags & MAKEENTRY) {
		/*
		 * Do not want the caller to try to add the name to the cache
		 * as well.
		 */
		cn->cn_flags &= ~MAKEENTRY;
		cache_name = TRUE;
	}
	if (!parent_vn || !cache_name)
		vn_fsp.vnfs_flags |= VNFS_NOCACHE;
	err = vnode_create(VNCREATE_FLAVOR, VCREATESIZE, &vn_fsp, &ni->vn);
	if (!err) {
		vnode_t vn = ni->vn;
		/*
		 * Vnode tag types are deprecated thus we use VT_OTHER given
		 * there is no VT_NTFS.
		 */
		vnode_settag(vn, VT_OTHER);
		ntfs_debug("Done.");
		return err;
	}
	ntfs_debug("Failed (error %d).", err);
	return err;
}

/**
 * ntfs_inode_get - obtain a normal ntfs inode
 * @vol:	mounted ntfs volume
 * @mft_no:	mft record number / inode number to obtain
 * @is_system:	true if the inode is a system inode and false otherwise
 * @nni:	destination pointer for the obtained ntfs inode
 * @parent_vn:	vnode of directory containing the inode to return or NULL
 * @cn:		componentname containing the name of the inode to return
 *
 * Obtain the ntfs inode corresponding to a specific normal inode (i.e. a
 * file or directory).  If @is_system is true the created vnode is marked as a
 * system vnode (via the VSYSTEM flag).
 *
 * If the inode is in the cache, it is returned.  If the inode has an attached
 * vnode, an iocount reference is obtained on the vnode before returning the
 * inode.  If @parent_vn is not NULL, the inode has an attached vnode, and the
 * parent of the vnode is not @parent_vn, the identity of the vnode is updated
 * changing the current parent of the vnode to @parent_vn.  If @cn is not NULL
 * and the current name of the vnode does not match the name described by @cn,
 * the identity of the vnode is updated changing the current name of the vnode
 * to the name described by @cn.
 *
 * If the inode is not in the cache, a new ntfs inode is allocated and
 * initialized, ntfs_inode_read() is called to read it in and fill in the
 * remainder of the ntfs inode structure before finally a new vnode is created
 * and attached to the new ntfs inode.  The inode is then returned with an
 * iocount reference taken on its vnode.  If @parent_vn is not NULL, it is set
 * up as the parent directory vnode of the newly created vnode.  If @cn is not
 * NULL, it is set up as the name of the newly created vnode.
 *
 * We do not need a reference count for the ntfs inode as the ntfs inode either
 * has a vnode, in which case the life-time and reference counting on the vnode
 * ensure there are no life-time problems with the ntfs inode or it does not
 * have a vnode in which case either there was an error and we are about to
 * destroy the ntfs inode or it is an extent inode, in which case the inode is
 * attached to the base inode and thus it is bound by the life-time and
 * reference count of the vnode of the base inode given that the extent inodes
 * are destroyed at the same time as their base inode is destroyed so we should
 * never get into life-time problems as it is now.
 *
 * Return 0 on success and errno on error.
 */
errno_t ntfs_inode_get(ntfs_volume *vol, ino64_t mft_no, const BOOL is_system,
		ntfs_inode **nni, vnode_t parent_vn, struct componentname *cn)
{
	ntfs_inode *ni;
	ntfs_attr na;
	int err;

	ntfs_debug("Entering for mft_no 0x%llx, is_system is %s.",
			(unsigned long long)mft_no,
			is_system ? "true" : "false");
	na = (ntfs_attr){
		.mft_no = mft_no,
		.type = AT_UNUSED,
		.raw = FALSE,
	};
	ni = ntfs_inode_hash_get(vol, &na);
	if (!ni) {
		ntfs_debug("Failed (ENOMEM).");
		return ENOMEM;
	}
	if (!NInoAlloc(ni)) {
		vnode_t vn = ni->vn;

		/*
		 * If the vnode is present and either the inode has multiple
		 * hard links or it has no parent and/or name, update the
		 * vnode identity with the supplied information if any.
		 */
		if (vn) {
			vnode_t old_parent_vn;
			const char *old_name;

			if (is_system && !vnode_issystem(vn))
				panic("%s(): mft_no 0x%llx, is_system is TRUE "
						"but vnode exists and is not "
						"marked VSYSTEM\n",
						__FUNCTION__, mft_no);
			old_parent_vn = vnode_getparent(vn);
			old_name = vnode_getname(vn);
			if (ni->link_count > 1 || !old_parent_vn || !old_name) {
				char *name = NULL;
				int len, hash, flags;

				flags = hash = len = 0;
				/*
				 * If a parent vnode was supplied and it is
				 * different from the current one, update it.
				 */
				if (parent_vn && old_parent_vn != parent_vn) {
					ntfs_debug("Updating vnode identity "
							"with new parent "
							"vnode.");
					flags |= VNODE_UPDATE_PARENT;
				}
				/*
				 * If a name was supplied and the vnode has no
				 * name at present or the names are not the
				 * same, update it.
				 */
				if (cn && (!old_name ||
						(long)strlen(old_name) !=
						cn->cn_namelen ||
						bcmp(old_name, cn->cn_nameptr,
						cn->cn_namelen))) {
					ntfs_debug("Updating vnode identity "
							"with new name.");
					name = cn->cn_nameptr;
					len = cn->cn_namelen;
					hash = cn->cn_hash;
					flags |= VNODE_UPDATE_NAME |
							VNODE_UPDATE_CACHE;
				}
				if (flags)
					vnode_update_identity(vn, parent_vn,
							name, len, hash, flags);
			}
			if (old_name)
				(void)vnode_putname(old_name);
			if (!parent_vn)
				parent_vn = old_parent_vn;
			if (cn && cn->cn_flags & MAKEENTRY) {
				if (parent_vn)
					cache_enter(parent_vn, vn, cn);
				/*
				 * Do not want the caller to try to add the
				 * name to the cache as well.
				 */
				cn->cn_flags &= ~MAKEENTRY;
			}
			if (old_parent_vn)
				(void)vnode_put(old_parent_vn);
		}
		*nni = ni;
		ntfs_debug("Done (found in cache).");
		return 0;
	}
	/*
	 * This is a freshly allocated inode, need to read it in now.  Also,
	 * need to allocate and attach a vnode to the new ntfs inode.
	 */
	err = ntfs_inode_read(ni);
	if (!err)
		err = ntfs_inode_add_vnode(ni, is_system, parent_vn, cn);
	if (!err) {
		/*
		 * If the inode is a directory, get the index inode now.  We
		 * postpone this to here because we did not have the directory
		 * vnode until now.
		 */
		if (S_ISDIR(ni->mode)) {
			ntfs_inode *ini;

			err = ntfs_index_inode_get(ni, I30, 4, &ini);
			if (err) {
				ntfs_error(vol->mp, "Failed to get index "
						"inode.");
				/* Kill the bad inode. */
				(void)vnode_recycle(ni->vn);
				(void)vnode_put(ni->vn);
				return err;
			}
			/*
			 * Copy a few useful values from the index inode to the
			 * directory inode so we do not need to get the index
			 * inode unless we really need it.
			 */
			if (NInoIndexAllocPresent(ini))
				NInoSetIndexAllocPresent(ni);
			ni->block_size = ini->block_size;
			ni->block_size_shift = ini->block_size_shift;
			ni->allocated_size = ini->allocated_size;
			ni->data_size = ini->data_size;
			ni->vcn_size = ini->vcn_size;
			ni->collation_rule = ini->collation_rule;
			ni->vcn_size_shift = ini->vcn_size_shift;
			/* We are done with the index vnode. */
			(void)vnode_put(ini->vn);
		}
		ntfs_inode_unlock_alloc(ni);
		*nni = ni;
		ntfs_debug("Done (added to cache).");
		return err;
	}
	ntfs_inode_reclaim(ni);
	ntfs_debug("Failed (inode read/vnode create).");
	return err;
}

/**
 * ntfs_attr_inode_get_ext - obtain an ntfs inode corresponding to an attribute
 * @base_ni:	base inode if @ni is not raw and non-raw inode of @ni otherwise
 * @type:	attribute type
 * @name:	Unicode name of the attribute (NULL if unnamed)
 * @name_len:	length of @name in Unicode characters (0 if unnamed)
 * @is_system:	true if the inode is a system inode and false otherwise
 * @raw:	whether to get the raw inode (TRUE) or not (FALSE)
 * @nni:	destination pointer for the obtained attribute ntfs inode
 *
 * Obtain the ntfs inode corresponding to the attribute specified by @type,
 * @name, and @name_len, which is present in the base mft record specified by
 * the ntfs inode @base_ni.  If @is_system is true the created vnode is marked
 * as a system vnode (via the VSYSTEM flag).
 *
 * If @raw is true @base_ni is the non-raw inode to which @ni belongs rather
 * than the base inode.
 *
 * If the attribute inode is in the cache, it is returned with an iocount
 * reference on the attached vnode.
 *
 * If the inode is not in the cache, a new ntfs inode is allocated and
 * initialized, ntfs_attr_inode_read() is called to read it in and fill in the
 * remainder of the ntfs inode structure before finally a new vnode is created
 * and attached to the new ntfs inode.  The inode is then returned with an
 * iocount reference taken on its vnode.
 *
 * Note, for index allocation attributes, you need to use ntfs_index_inode_get()
 * instead of ntfs_attr_inode_get() as working with indices is a lot more
 * complex.
 *
 * Return 0 on success and errno on error.
 *
 * TODO: For now we do not store a name for attribute inodes as
 * ntfs_vnop_lookup() cannot return them and we only use them internally so
 * no-one can call VNOP_GETATTR() or anything like that on them so the name is
 * never used.
 */
errno_t ntfs_attr_inode_get_ext(ntfs_inode *base_ni, ATTR_TYPE type,
		ntfschar *name, u32 name_len, const BOOL is_system, BOOL raw,
		ntfs_inode **nni)
{
	vnode_t base_parent_vn;
	ntfs_inode *ni;
	ntfs_attr na;
	int err;

	ntfs_debug("Entering for mft_no 0x%llx, type 0x%x, name_len 0x%x, "
			"is_system is %s, raw is %s.",
			(unsigned long long)base_ni->mft_no, le32_to_cpu(type),
			(unsigned)name_len, is_system ? "true" : "false",
			raw ? "true" : "false");
	/* Make sure no one calls ntfs_attr_inode_get() for indices. */
	if (type == AT_INDEX_ALLOCATION)
		panic("%s() called for an index.\n", __FUNCTION__);
	if (!base_ni->vn)
		panic("%s() called with a base inode that does not have a "
				"vnode attached.\n", __FUNCTION__);
	na = (ntfs_attr){
		.mft_no = base_ni->mft_no,
		.type = type,
		.name = name,
		.name_len = name_len,
		.raw = raw,
	};
	ni = ntfs_inode_hash_get(base_ni->vol, &na);
	if (!ni) {
		ntfs_debug("Failed (ENOMEM).");
		return ENOMEM;
	}
	/* Get the parent vnode from the base vnode if present. */
	base_parent_vn = vnode_getparent(base_ni->vn);
	if (!NInoAlloc(ni)) {
		vnode_t vn;
		
		vn = ni->vn;
		if (vn) {
			vnode_t parent_vn;

			parent_vn = vnode_getparent(vn);
			if (parent_vn != base_parent_vn) {
				ntfs_debug("Updating vnode identity with new "
						"parent vnode.");
				vnode_update_identity(vn, base_parent_vn, NULL,
						0, 0, VNODE_UPDATE_PARENT);
			}
			if (parent_vn)
				(void)vnode_put(parent_vn);
		}
		if (base_parent_vn)
			(void)vnode_put(base_parent_vn);
		*nni = ni;
		ntfs_debug("Done (found in cache).");
		return 0;
	}
	/*
	 * This is a freshly allocated inode, need to read it in now.  Also,
	 * need to allocate and attach a vnode to the new ntfs inode.
	 */
	err = ntfs_attr_inode_read(base_ni, ni);
	if (!err)
		err = ntfs_inode_add_vnode(ni, is_system, base_parent_vn, NULL);
	if (base_parent_vn)
		(void)vnode_put(base_parent_vn);
	if (!err) {
		ntfs_inode_unlock_alloc(ni);
		*nni = ni;
		ntfs_debug("Done (added to cache).");
		return err;
	}
	ntfs_inode_reclaim(ni);
	ntfs_debug("Failed (inode read/vnode create).");
	return err;
}

/**
 * ntfs_index_inode_get - obtain an ntfs inode corresponding to an index
 * @base_ni:	ntfs base inode containing the index related attributes
 * @name:	Unicode name of the index
 * @name_len:	length of @name in Unicode characters
 * @nni:	destination pointer for the obtained index ntfs inode
 *
 * Obtain the ntfs inode corresponding to the index specified by @name and
 * @name_len, which is present in the base mft record specified by the ntfs
 * inode @base_ni.
 *
 * If the index inode is in the cache, it is returned with an iocount reference
 * on the attached vnode.
 *
 * If the inode is not in the cache, a new ntfs inode is allocated and
 * initialized, ntfs_index_inode_read() is called to read it in and fill in the
 * remainder of the ntfs inode structure before finally a new vnode is created
 * and attached to the new ntfs inode.  The inode is then returned with an
 * iocount reference taken on its vnode.
 *
 * Return 0 on success and errno on error.
 *
 * TODO: For now we do not store a name for attribute inodes as
 * ntfs_vnop_lookup() cannot return them and we only use them internally so
 * no-one can call VNOP_GETATTR() or anything like that on them so the name is
 * never used.
 */
errno_t ntfs_index_inode_get(ntfs_inode *base_ni, ntfschar *name, u32 name_len,
		ntfs_inode **nni)
{
	vnode_t base_parent_vn;
	ntfs_inode *ni;
	ntfs_attr na;
	int err;

	ntfs_debug("Entering for mft_no 0x%llx, name_len 0x%x.",
			(unsigned long long)base_ni->mft_no,
			(unsigned)name_len);
	if (!base_ni->vn)
		panic("%s() called with a base inode that does not have a "
				"vnode attached.\n", __FUNCTION__);
	na = (ntfs_attr){
		.mft_no = base_ni->mft_no,
		.type = AT_INDEX_ALLOCATION,
		.name = name,
		.name_len = name_len,
		.raw = FALSE,
	};
	ni = ntfs_inode_hash_get(base_ni->vol, &na);
	if (!ni) {
		ntfs_debug("Failed (ENOMEM).");
		return ENOMEM;
	}
	/* Get the parent vnode from the base vnode if present. */
	base_parent_vn = vnode_getparent(base_ni->vn);
	if (!NInoAlloc(ni)) {
		vnode_t vn;

		vn = ni->vn;
		if (vn) {
			vnode_t parent_vn;

			parent_vn = vnode_getparent(vn);
			if (parent_vn != base_parent_vn) {
				ntfs_debug("Updating vnode identity with new "
						"parent vnode.");
				vnode_update_identity(vn, base_parent_vn, NULL,
						0, 0, VNODE_UPDATE_PARENT);
			}
			if (parent_vn)
				(void)vnode_put(parent_vn);
		}
		if (base_parent_vn)
			(void)vnode_put(base_parent_vn);
		*nni = ni;
		ntfs_debug("Done (found in cache).");
		return 0;
	}
	/*
	 * This is a freshly allocated inode, need to read it in now.  Also,
	 * need to allocate and attach a vnode to the new ntfs inode.
	 */
	err = ntfs_index_inode_read(base_ni, ni);
	if (!err)
		err = ntfs_inode_add_vnode(ni, FALSE, base_parent_vn, NULL);
	if (base_parent_vn)
		(void)vnode_put(base_parent_vn);
	if (!err) {
		ntfs_inode_unlock_alloc(ni);
		*nni = ni;
		ntfs_debug("Done (added to cache).");
		return err;
	}
	ntfs_inode_reclaim(ni);
	ntfs_debug("Failed (inode read/vnode create).");
	return err;
}

/**
 * ntfs_extent_inode_get - obtain an extent inode belonging to a base inode
 * @base_ni:	ntfs base inode whose extent inode to get
 * @mref:	mft reference of extent inode to obtain
 * @ext_ni:	destination pointer for the obtained extent ntfs inode
 *
 * Obtain the extent ntfs inode with mft reference @mref belonging to the base
 * inode @base_ni.
 *
 * If the inode is in the cache, it is returned.
 *
 * If the inode is not in the cache, a new ntfs inode is allocated, initialized
 * and then returned.
 *
 * Return 0 on success and errno on error.
 *
 * Note: No vnode is attached to the extent ntfs inode.
 */
errno_t ntfs_extent_inode_get(ntfs_inode *base_ni, MFT_REF mref,
		ntfs_inode **ext_ni)
{
	ntfs_inode *ni;
	ntfs_attr na;
	u16 seq_no = MSEQNO(mref);

	ntfs_debug("Entering for mft_no 0x%llx.",
			(unsigned long long)MREF(mref));
	na = (ntfs_attr){
		.mft_no = MREF(mref),
		.type = AT_UNUSED,
		.raw = FALSE,
	};
	ni = ntfs_inode_hash_get(base_ni->vol, &na);
	if (!ni) {
		ntfs_debug("Failed (ENOMEM).");
		return ENOMEM;
	}
	if (!NInoAlloc(ni)) {
		if (ni->seq_no == seq_no) {
			*ext_ni = ni;
			ntfs_debug("Done (found in cache).");
			return 0;
		}
		ntfs_inode_reclaim(ni);
		ntfs_error(base_ni->vol->mp, "Found stale extent mft "
				"reference!  Corrupt filesystem.  Run chkdsk.");
		return EIO;
	}
	/*
	 * This is a freshly allocated inode, need to finish setting it up as
	 * an extent inode now.  Note we do not take a reference on the vnode
	 * of the base inode because that would pin the base inode which would
	 * make it unfreeable.  This is not a problem as when the base vnode is
	 * reclaimed, we release all attached extent inodes, too.  Also, we
	 * simply set the sequence number rather than verify it against the one
	 * in the extent mft record and we leave it to the caller to verify the
	 * sequence number after mapping the extent mft record.
	 */
	ni->seq_no = seq_no;
	ni->nr_extents = -1;
	ni->base_ni = base_ni;
	ntfs_inode_unlock_alloc(ni);
	*ext_ni = ni;
	ntfs_debug("Done (added to cache).");
	return 0;
}

/**
 * ntfs_inode_is_extended_system - check if an inode is in the $Extend directory
 * @ctx:	initialized attribute search context
 *
 * Search all filename attributes in the inode described by the attribute
 * search context @ctx and check if any of the names are in the $Extend system
 * directory.
 *
 * Return values:
 *	   1: File is in $Extend directory.
 *	   0: File is not in $Extend directory.
 *    -errno: Failed to determine if the file is in the $Extend directory.
 */
static inline signed ntfs_inode_is_extended_system(ntfs_attr_search_ctx *ctx)
{
	ntfs_volume *vol;
	unsigned nr_links;
	errno_t err;

	ntfs_debug("Entering for mft_no 0x%llx.",
			(unsigned long long)ctx->ni->mft_no);
	vol = ctx->ni->vol;
	/* Restart search. */
	ntfs_attr_search_ctx_reinit(ctx);
	/* Get number of hard links. */
	nr_links = le16_to_cpu(ctx->m->link_count);
	if (!nr_links) {
		ntfs_error(vol->mp, "Hard link count is zero.");
		return -EIO;
	}
	/* Loop through all hard links. */
	while (!(err = ntfs_attr_lookup(AT_FILENAME, NULL, 0, 0, 0, NULL, 0,
			ctx))) {
		FILENAME_ATTR *fn;
		ATTR_RECORD *a = ctx->a;
		u8 *a_end, *fn_end;

		nr_links--;
		/*
		 * Maximum sanity checking as we are called on an inode that we
		 * suspect might be corrupt.
		 */
		if (a->non_resident) {
			ntfs_error(vol->mp, "Filename is non-resident.");
			return -EIO;
		}
		if (a->flags) {
			ntfs_error(vol->mp, "Filename has invalid flags.");
			return -EIO;
		}
		if (!(a->resident_flags & RESIDENT_ATTR_IS_INDEXED)) {
			ntfs_error(vol->mp, "Filename is not indexed.");
			return -EIO;
		}
		a_end = (u8*)a + le32_to_cpu(a->length);
		fn = (FILENAME_ATTR*)((u8*)a + le16_to_cpu(a->value_offset));
		fn_end = (u8*)fn + le32_to_cpu(a->value_length);
		if ((u8*)fn < (u8*)a || fn_end < (u8*)a || fn_end > a_end ||
				a_end > (u8*)ctx->m + vol->mft_record_size) {
			ntfs_error(vol->mp, "Filename attribute is corrupt.");
			return -EIO;
		}
		/* This attribute is ok, but is it in the $Extend directory? */
		if (MREF_LE(fn->parent_directory) == FILE_Extend) {
			ntfs_debug("Done (system).");
			return 1; /* Yes, it is an extended system file. */
		}
	}
	if (err != ENOENT) {
		ntfs_error(vol->mp, "Failed to lookup filename attribute.");
		return -err;
	}
	if (nr_links) {
		ntfs_error(vol->mp, "Hard link count does not match number of "
				"filename attributes.");
		return -EIO;
	}
	ntfs_debug("Done (not system).");
	return 0;	/* NO, it is not an extended system file. */
}

/**
 * ntfs_inode_read - read an inode from its device
 * @ni:		ntfs inode to read
 *
 * ntfs_inode_read() is called from ntfs_inode_get() to read the inode
 * described by @ni into memory from the device.
 *
 * The only fields in @ni that we need to/can look at when the function is
 * called are @ni->vol, pointing to the mounted ntfs volume, and @ni->mft_no, 
 * the number of the inode to load.
 *
 * ntfs_inode_read() maps, pins and locks the mft record number @ni->mft_no and
 * sets up the ntfs inode.
 *
 * Return 0 on success and errno on error.
 */
static errno_t ntfs_inode_read(ntfs_inode *ni)
{
	ntfs_volume *vol = ni->vol;
	MFT_RECORD *m;
	ntfs_attr_search_ctx *ctx;
	ATTR_RECORD *a;
	STANDARD_INFORMATION *si;
	errno_t err;

	ntfs_debug("Entering for mft_no 0x%llx.",
			(unsigned long long)ni->mft_no);
	err = ntfs_mft_record_map(ni, &m);
	if (err) {
		ntfs_error(vol->mp, "Failed to map mft record.");
		m = NULL;
		ctx = NULL;
		goto err;
	}
	ctx = ntfs_attr_search_ctx_get(ni, m);
	if (!ctx) {
		ntfs_error(vol->mp, "Failed to get attribute search context.");
		err = ENOMEM;
		goto err;
	}
	if (!(m->flags & MFT_RECORD_IN_USE)) {
		ntfs_error(vol->mp, "Inode is not in use.");
		goto err;
	}
	if (m->base_mft_record) {
		ntfs_error(vol->mp, "Inode is an extent inode.");
		goto err;
	}
	/* Cache information from mft record in ntfs inode. */
	ni->seq_no = le16_to_cpu(m->sequence_number);
	/*
	 * FIXME: Keep in mind that link_count is two for files which have both
	 * a long filename and a short filename as separate entries, so if
	 * we are hiding short filenames this will be too high.  Either we
	 * need to account for the short filenames by subtracting them or we
	 * need to make sure we delete files even though the number of links
	 * is not zero which might be tricky due to vfs interactions.  Need to
	 * think about this some more when implementing the unlink call.
	 */
	ni->link_count = le16_to_cpu(m->link_count);
	/*
	 * FIXME: Reparse points can have the directory bit set even though
	 * they would be S_IFLNK.  Need to deal with this further below when we
	 * implement reparse points / symbolic links but it will do for now.
	 * Also if not a directory, it could be something else, rather than
	 * a regular file.  But again, will do for now.
	 */
	/* Everyone gets all permissions. */
	ni->mode |= ACCESSPERMS;
	if (m->flags & MFT_RECORD_IS_DIRECTORY) {
		ni->mode |= S_IFDIR;
		/*
		 * Apply the directory permissions mask set in the mount
		 * options.
		 */
		ni->mode &= ~vol->dmask;
		/*
		 * Things break without this kludge!  FIXME: We do not force
		 * the link_count to 1 any more as we simply do not return a
		 * link count from ntfs_vnop_getattr() for directory vnodes
		 * thus the link_count is private to us so it might as well be
		 * accurate.
		 */
		//if (ni->link_count > 1)
		//	ni->link_count = 1;
	} else {
		ni->mode |= S_IFREG;
		/* Apply the file permissions mask set in the mount options. */
		ni->mode &= ~vol->fmask;
	}
	/*
	 * Find the standard information attribute in the mft record.  At this
	 * stage we have not setup the attribute list stuff yet, so this could
	 * in fact fail if the standard information is in an extent record, but
	 * this is not allowed hence not a problem.
	 */
	err = ntfs_attr_lookup(AT_STANDARD_INFORMATION, NULL, 0, 0, 0, NULL, 0,
			ctx);
	a = ctx->a;
	if (err || a->non_resident || a->flags) {
		if (err) {
			if (err == ENOENT) {
				/*
				 * TODO: We should be performing a hot fix here
				 * (if the recover mount option is set) by
				 * creating a new attribute.
				 */
				ntfs_error(vol->mp, "Standard information "
						"attribute is missing.");
			} else
				ntfs_error(vol->mp, "Failed to lookup "
						"standard information "
						"attribute.");
		} else {
info_err:
			ntfs_error(vol->mp, "Standard information attribute "
					"is corrupt.");
		}
		goto err;
	}
	si = (STANDARD_INFORMATION*)((u8*)a + le16_to_cpu(a->value_offset));
	/* Some bounds checks. */
	if ((u8*)si < (u8*)a || (u8*)si + le32_to_cpu(a->value_length) >
			(u8*)a + le32_to_cpu(a->length) ||
			(u8*)a + le32_to_cpu(a->length) > (u8*)ctx->m +
			vol->mft_record_size)
		goto info_err;
	/* Cache the file attributes in the ntfs inode. */
	ni->file_attributes = si->file_attributes;
	/*
	 * Cache the create, the last data and mft modified, and the last
	 * access times in the ntfs inode.
	 */
	ni->creation_time = ntfs2utc(si->creation_time);
	ni->last_data_change_time = ntfs2utc(si->last_data_change_time);
	ni->last_mft_change_time = ntfs2utc(si->last_mft_change_time);
	ni->last_access_time = ntfs2utc(si->last_access_time);
	/* Find the attribute list attribute if present. */
	ntfs_attr_search_ctx_reinit(ctx);
	err = ntfs_attr_lookup(AT_ATTRIBUTE_LIST, NULL, 0, 0, 0, NULL, 0, ctx);
	a = ctx->a;
	if (err) {
		if (err != ENOENT) {
			ntfs_error(vol->mp, "Failed to lookup attribute list "
					"attribute.");
			goto err;
		}
	} else /* if (!err) */ {
		ntfs_debug("Attribute list found in inode 0x%llx.",
				(unsigned long long)ni->mft_no);
		NInoSetAttrList(ni);
		if (a->flags & ATTR_COMPRESSION_MASK) {
			ntfs_error(vol->mp, "Attribute list attribute is "
					"compressed.  Not allowed.");
			goto err;
		}
		if (a->flags & (ATTR_IS_ENCRYPTED | ATTR_IS_SPARSE)) {
			if (a->non_resident) {
				ntfs_error(vol->mp, "Non-resident attribute "
						"list attribute is encrypted/"
						"sparse.  Not allowed.");
				goto err;
			}
			ntfs_warning(vol->mp, "Resident attribute list "
					"attribute is marked encrypted/sparse "
					"which is not true.  However, Windows "
					"allows this and chkdsk does not "
					"detect or correct it so we will just "
					"ignore the invalid flags and pretend "
					"they are not set.");
		}
		/* Now allocate memory for the attribute list. */
		ni->attr_list_size = (u32)ntfs_attr_size(a);
		ni->attr_list = OSMalloc(ni->attr_list_size, ntfs_malloc_tag);
		if (!ni->attr_list) {
			ntfs_error(vol->mp, "Not enough memory to allocate "
					"buffer for attribute list.");
			err = ENOMEM;
			goto err;
		}
		if (a->non_resident) {
			NInoSetAttrListNonResident(ni);
			if (a->lowest_vcn) {
				ntfs_error(vol->mp, "Attribute list has non "
						"zero lowest_vcn.");
				goto err;
			}
			/*
			 * Setup the runlist.  No need for locking as we have
			 * exclusive access to the inode at this time.
			 */
			err = ntfs_mapping_pairs_decompress(vol, a,
					&ni->attr_list_rl);
			if (err) {
				ntfs_error(vol->mp, "Mapping pairs "
						"decompression failed.");
				goto err;
			}
			/* Now load the attribute list. */
			err = ntfs_rl_read(vol, &ni->attr_list_rl,
					ni->attr_list, ni->attr_list_size,
					sle64_to_cpu(a->initialized_size));
			if (err) {
				ntfs_error(vol->mp, "Failed to load attribute "
						"list attribute.");
				goto err;
			}
		} else /* if (!a->non_resident) */ {
			u8 *a_end, *al;
			u32 al_len;

			a_end = (u8*)a + le32_to_cpu(a->length);
			al = (u8*)a + le16_to_cpu(a->value_offset);
			al_len = le32_to_cpu(a->value_length);
			if (al < (u8*)a || al + al_len > a_end || (u8*)a_end >
					(u8*)ctx->m + vol->mft_record_size) {
				ntfs_error(vol->mp, "Resident attribute list "
						"attribute is corrupt.");
				goto err;
			}
			/* Now copy the attribute list attribute. */
			memcpy(ni->attr_list, al, al_len);
		}
	}
	/*
	 * If an attribute list is present we now have the attribute list value
	 * in @ni->attr_list and it is @ni->attr_list_size bytes in size.
	 */
	if (S_ISDIR(ni->mode)) {
		ni->vcn_size = 0;
		ni->collation_rule = 0;
		ni->vcn_size_shift = 0;
	} else {
		/* It is a file. */
		ntfs_attr_search_ctx_reinit(ctx);
		/* Setup the data attribute, even if not present. */
		ni->type = AT_DATA;
		ni->name = NULL;
		ni->name_len = 0;
		/* Find first extent of the unnamed data attribute. */
		err = ntfs_attr_lookup(AT_DATA, NULL, 0, 0, 0, NULL, 0, ctx);
		if (err) {
			signed res;

			ni->allocated_size = ni->data_size =
					ni->initialized_size = 0;
			if (err != ENOENT) {
				ntfs_error(vol->mp, "Failed to lookup data "
						"attribute.");
				goto err;
			}
			/*
			 * FILE_Secure does not have an unnamed data attribute,
			 * so we special case it here.
			 */
			if (ni->mft_no == FILE_Secure)
				goto no_data_attr_special_case;
			/*
			 * Most if not all the system files in the $Extend
			 * system directory do not have unnamed data
			 * attributes so we need to check if the parent
			 * directory of the file is FILE_Extend and if it is
			 * ignore this error.  To do this we need to get the
			 * name of this inode from the mft record as the name
			 * contains the back reference to the parent directory.
			 */
			res = ntfs_inode_is_extended_system(ctx);
			if (res > 0)
				goto no_data_attr_special_case;
			err = -res;
			// FIXME: File is corrupt! Hot-fix with empty data
			// attribute if recovery option is set.
			ntfs_error(vol->mp, "Data attribute is missing.");
			goto err;
		}
		a = ctx->a;
		/* Setup the state. */
		if (a->flags & (ATTR_COMPRESSION_MASK | ATTR_IS_SPARSE)) {
			if (a->flags & ATTR_COMPRESSION_MASK) {
				NInoSetCompressed(ni);
				if (!NVolCompressionEnabled(vol)) {
					ntfs_error(vol->mp, "Found compressed "
							"data but compression "
							"is disabled on this "
							"volume and/or mount.");
					goto err;
				}
				if ((a->flags & ATTR_COMPRESSION_MASK)
						!= ATTR_IS_COMPRESSED) {
					ntfs_error(vol->mp, "Found unknown "
							"compression method "
							"or corrupt file.");
					goto err;
				}
			}
			if (a->flags & ATTR_IS_SPARSE)
				NInoSetSparse(ni);
		}
		if (a->flags & ATTR_IS_ENCRYPTED) {
			if (NInoCompressed(ni)) {
				ntfs_error(vol->mp, "Found encrypted and "
						"compressed data.");
				goto err;
			}
			NInoSetEncrypted(ni);
		}
		if (a->non_resident) {
			NInoSetNonResident(ni);
			if (NInoCompressed(ni) || NInoSparse(ni)) {
				if (a->compression_unit != 4) {
					ntfs_error(vol->mp, "Found "
							"non-standard "
							"compression unit (%u "
							"instead of 4).  "
							"Cannot handle this.",
							a->compression_unit);
					err = ENOTSUP;
					goto err;
				}
				ni->compression_block_clusters = 1U <<
						a->compression_unit;
				ni->compression_block_size = 1U << (
						a->compression_unit +
						vol->cluster_size_shift);
				ni->compression_block_size_shift = ffs(
						ni->compression_block_size) - 1;
				ni->compressed_size = sle64_to_cpu(
						a->compressed_size);
			}
			if (a->lowest_vcn) {
				ntfs_error(vol->mp, "First extent of data "
						"attribute has non-zero "
						"lowest_vcn.");
				goto err;
			}
			ni->allocated_size = sle64_to_cpu(a->allocated_size);
			ni->data_size = sle64_to_cpu(a->data_size);
			ni->initialized_size = sle64_to_cpu(
					a->initialized_size);
		} else { /* Resident attribute. */
			u8 *a_end, *data;
			u32 data_len;

			a_end = (u8*)a + le32_to_cpu(a->length);
			data = (u8*)a + le16_to_cpu(a->value_offset);
			data_len = le32_to_cpu(a->value_length);
			if (data < (u8*)a || data + data_len > a_end ||
					(u8*)a_end > (u8*)ctx->m +
					vol->mft_record_size) {
				ntfs_error(vol->mp, "Resident data attribute "
						"is corrupt.");
				goto err;
			}
			ni->allocated_size = a_end - data;
			ni->data_size = ni->initialized_size = data_len;
		}
	}
no_data_attr_special_case:
	ntfs_attr_search_ctx_put(ctx);
	ntfs_mft_record_unmap(ni);
	ntfs_debug("Done.");
	return 0;
err:
	if (ctx)
		ntfs_attr_search_ctx_put(ctx);
	if (m)
		ntfs_mft_record_unmap(ni);
	if (!err)
		err = EIO;
	ntfs_error(vol->mp, "Failed (error %d) for inode 0x%llx.  Run chkdsk.",
			(int)err, (unsigned long long)ni->mft_no);
	if (err != ENOTSUP && err != ENOMEM)
		NVolSetErrors(vol);
	return err;
}

/**
 * ntfs_attr_inode_read - read an attribute inode from its base inode
 * @base_ni:	base inode if @ni is not raw and non-raw inode of @ni otherwise
 * @ni:		attribute inode to read
 *
 * ntfs_attr_inode_read() is called from ntfs_attr_inode_get_ext() to read the
 * attribute inode described by @ni into memory from the base mft record
 * described by @base_ni.
 *
 * If @ni is a raw inode @base_ni is the non-raw inode to which @ni belongs
 * rather than the base inode.
 *
 * ntfs_attr_inode_read() maps, pins and locks the base mft record and looks up
 * the attribute described by @ni before setting up the ntfs inode.
 *
 * Return 0 on success and errno on error.
 *
 * Note ntfs_attr_inode_read() cannot be called for AT_INDEX_ALLOCATION, call
 * ntfs_index_inode_read() instead.
 */
static errno_t ntfs_attr_inode_read(ntfs_inode *base_ni, ntfs_inode *ni)
{
	ntfs_volume *vol = ni->vol;
	MFT_RECORD *m;
	ntfs_attr_search_ctx *ctx;
	ATTR_RECORD *a;
	vnode_t vn;
	errno_t err;

	ntfs_debug("Entering for mft_no 0x%llx, attribute type 0x%x, "
			"attribute name length 0x%x.",
			(unsigned long long)ni->mft_no,
			(unsigned)le32_to_cpu(ni->type),
			(unsigned)ni->name_len);
	/* Mirror the values from the base inode. */
	ni->seq_no = base_ni->seq_no;
	ni->uid	= base_ni->uid;
	ni->gid	= base_ni->gid;
	ni->file_attributes = base_ni->file_attributes;
	ni->creation_time = base_ni->creation_time;
	ni->last_data_change_time = base_ni->last_data_change_time;
	ni->last_mft_change_time = base_ni->last_mft_change_time;
	ni->last_access_time = base_ni->last_access_time;
	/* Attributes cannot be hard-linked so link count is always 1. */
	ni->link_count = 1;
	/* Set inode type to zero but preserve permissions. */
	ni->mode = base_ni->mode & ~S_IFMT;
	/*
	 * If this is our special case of loading the secondary inode for
	 * accessing the raw data of compressed files, we can simply copy the
	 * relevant fields from the base inode rather than mapping the mft
	 * record and looking up the data attribute again.
	 */
	if (NInoRaw(ni)) {
		if (NInoCompressed(base_ni))
			NInoSetCompressed(ni);
		if (NInoSparse(base_ni))
			NInoSetSparse(ni);
		if (NInoEncrypted(base_ni))
			NInoSetEncrypted(ni);
		if (NInoNonResident(base_ni))
			NInoSetNonResident(ni);
		if (NInoCompressed(base_ni) || NInoSparse(base_ni)) {
			ni->compression_block_clusters =
					base_ni->compression_block_clusters;
			ni->compression_block_size =
					base_ni->compression_block_size;
			ni->compression_block_size_shift =
					base_ni->compression_block_size_shift;
			ni->compressed_size = base_ni->compressed_size;
		}
		ni->initialized_size = ni->data_size = ni->allocated_size =
				base_ni->allocated_size;
		if (NInoAttr(base_ni)) {
			/* Set @base_ni to point to the real base inode. */
			if (base_ni->nr_extents != -1)
				panic("%s(): Called for non-raw attribute "
						"inode which does not have a "
						"base inode.", __FUNCTION__);
			base_ni = base_ni->base_ni;
		}
	} else /* if (!NInoRaw(ni)) */ {
		/* Map the mft record for the base inode. */
		err = ntfs_mft_record_map(base_ni, &m);
		if (err) {
			ntfs_error(vol->mp, "Failed to map base mft record.");
			m = NULL;
			ctx = NULL;
			goto err;
		}
		ctx = ntfs_attr_search_ctx_get(base_ni, m);
		if (!ctx) {
			ntfs_error(vol->mp, "Failed to get attribute search "
					"context.");
			err = ENOMEM;
			goto err;
		}
		/* Find the attribute. */
		err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len,
				CASE_SENSITIVE, 0, NULL, 0, ctx);
		if (err) {
			if (err == ENOENT)
				ntfs_error(vol->mp, "Attribute is missing.");
			else
				ntfs_error(vol->mp, "Failed to lookup "
						"attribute.");
			goto err;
		}
		a = ctx->a;
		if (a->flags & (ATTR_COMPRESSION_MASK | ATTR_IS_SPARSE)) {
			if (a->flags & ATTR_COMPRESSION_MASK) {
				NInoSetCompressed(ni);
				if (ni->type != AT_DATA) {
					ntfs_error(vol->mp, "Found compressed "
							"non-data attribute.  "
							"Please report you "
							"saw this message to "
							"%s.", ntfs_dev_email);
					goto err;
				}
				if (!NVolCompressionEnabled(vol)) {
					ntfs_error(vol->mp, "Found compressed "
							"data but compression "
							"is disabled on this "
							"volume and/or mount.");
					goto err;
				}
				if ((a->flags & ATTR_COMPRESSION_MASK) !=
						ATTR_IS_COMPRESSED) {
					ntfs_error(vol->mp, "Found unknown "
							"compression method or "
							"corrupt file.");
					goto err;
				}
			}
			if (a->flags & ATTR_IS_SPARSE)
				NInoSetSparse(ni);
			if (NInoMstProtected(ni)) {
				ntfs_error(vol->mp, "Found mst protected "
						"attribute but the attribute "
						"is %s.  Please report you "
						"saw this message to %s.",
						NInoCompressed(ni) ?
						"compressed" : "sparse",
						ntfs_dev_email);
				goto err;
			}
		}
		if (a->flags & ATTR_IS_ENCRYPTED) {
			if (ni->type != AT_DATA) {
				ntfs_error(vol->mp, "Found encrypted non-data "
						"attribute.  Please report "
						"you saw this message to %s.",
						ntfs_dev_email);
				goto err;
			}
			if (NInoMstProtected(ni)) {
				ntfs_error(vol->mp, "Found mst protected "
						"attribute but the attribute "
						"is encrypted.  Please report "
						"you saw this message to %s.",
						ntfs_dev_email);
				goto err;
			}
			if (NInoCompressed(ni)) {
				ntfs_error(vol->mp, "Found encrypted and "
						"compressed data.");
				goto err;
			}
			NInoSetEncrypted(ni);
		}
		if (!a->non_resident) {
			u8 *a_end, *val;
			u32 val_len;

			/*
			 * Ensure the attribute name is placed before the
			 * value.
			 */
			if (a->name_length && (le16_to_cpu(a->name_offset) >=
					le16_to_cpu(a->value_offset))) {
				ntfs_error(vol->mp, "Attribute name is placed "
						"after the attribute value.");
				goto err;
			}
			if (NInoMstProtected(ni)) {
				ntfs_error(vol->mp, "Found mst protected "
						"attribute but the attribute "
						"is resident.  Please report "
						"you saw this message to %s.",
						ntfs_dev_email);
				goto err;
			}
			a_end = (u8*)a + le32_to_cpu(a->length);
			val = (u8*)a + le16_to_cpu(a->value_offset);
			val_len = le32_to_cpu(a->value_length);
			if (val < (u8*)a || val + val_len > a_end ||
					(u8*)a_end > (u8*)ctx->m +
					vol->mft_record_size) {
				ntfs_error(vol->mp, "Resident attribute is "
						"corrupt.");
				goto err;
			}
			ni->allocated_size = a_end - val;
			ni->data_size = ni->initialized_size = val_len;
		} else {
			NInoSetNonResident(ni);
			/*
			 * Ensure the attribute name is placed before the
			 * mapping pairs array.
			 */
			if (a->name_length && (le16_to_cpu(a->name_offset) >=
					le16_to_cpu(a->mapping_pairs_offset))) {
				ntfs_error(vol->mp, "Attribute name is placed "
						"after the mapping pairs "
						"array.");
				goto err;
			}
			if (NInoCompressed(ni) || NInoSparse(ni)) {
				if (a->compression_unit != 4) {
					ntfs_error(vol->mp, "Found "
							"non-standard "
							"compression unit (%u "
							"instead of 4).  "
							"Cannot handle this.",
							a->compression_unit);
					err = ENOTSUP;
					goto err;
				}
				ni->compression_block_clusters = 1U <<
						a->compression_unit;
				ni->compression_block_size = 1U << (
						a->compression_unit +
						vol->cluster_size_shift);
				ni->compression_block_size_shift = ffs(
						ni->compression_block_size) - 1;
				ni->compressed_size = sle64_to_cpu(
						a->compressed_size);
			}
			if (a->lowest_vcn) {
				ntfs_error(vol->mp, "First extent of "
						"attribute has non-zero "
						"lowest_vcn.");
				goto err;
			}
			ni->allocated_size = sle64_to_cpu(a->allocated_size);
			ni->data_size = sle64_to_cpu(a->data_size);
			ni->initialized_size =
					sle64_to_cpu(a->initialized_size);
		}
		ntfs_attr_search_ctx_put(ctx);
		ntfs_mft_record_unmap(base_ni);
	}
	/*
	 * Make sure the base inode and vnode do not go away and attach the
	 * base inode to the attribute inode.
	 */
	vn = base_ni->vn;
	if (!vn)
		panic("%s(): No vnode attached to base inode 0x%llx.",
				__FUNCTION__,
				(unsigned long long)base_ni->mft_no);
	err = vnode_ref(vn);
	if (err)
		ntfs_error(vol->mp, "vnode_ref() failed!");
	OSIncrementAtomic(&base_ni->nr_refs);
	ni->base_ni = base_ni;
	ni->nr_extents = -1;
	ntfs_debug("Done.");
	return 0;
err:
	if (ctx)
		ntfs_attr_search_ctx_put(ctx);
	if (m)
		ntfs_mft_record_unmap(base_ni);
	if (!err)
		err = EIO;
	ntfs_error(vol->mp, "Failed (error %d) for attribute inode 0x%llx, "
			"attribute type 0x%x, name_len 0x%x.  Run chkdsk.",
			(int)err, (unsigned long long)ni->mft_no,
			(unsigned)le32_to_cpu(ni->type),
			(unsigned)ni->name_len);
	if (err != ENOTSUP && err != ENOMEM)
		NVolSetErrors(vol);
	return err;
}

/**
 * ntfs_index_inode_read - read an index inode from its base inode
 * @base_ni:	base inode
 * @ni:		index inode to read
 *
 * ntfs_index_inode_read() is called from ntfs_index_inode_get() to read the
 * index inode described by @ni into memory from the base mft record described
 * by @base_ni.
 *
 * ntfs_index_inode_read() maps, pins and locks the base mft record and looks
 * up the attributes relating to the index described by @ni before setting up
 * the ntfs inode.
 *
 * Return 0 on success and errno on error.
 *
 * Note, index inodes are essentially attribute inodes (NInoAttr() is true)
 * with the attribute type set to AT_INDEX_ALLOCATION.  Most importantly, for
 * small indices the index allocation attribute might not actually exist.
 * However, the index root attribute always exists but this does not need to
 * have an inode associated with it and this is why we define a new inode type
 * index.  Also, we need to have an attribute inode for the bitmap attribute
 * corresponding to the index allocation attribute and we can store this in the
 * appropriate field of the inode.
 */
static errno_t ntfs_index_inode_read(ntfs_inode *base_ni, ntfs_inode *ni)
{
	ntfs_volume *vol = ni->vol;
	MFT_RECORD *m;
	ATTR_RECORD *a;
	ntfs_attr_search_ctx *ctx;
	INDEX_ROOT *ir;
	u8 *ir_end, *index_end;
	ntfs_inode *bni;
	vnode_t vn;
	errno_t err;
	BOOL is_dir_index = (S_ISDIR(base_ni->mode) && ni->name == I30);
	
	ntfs_debug("Entering for mft_no 0x%llx, index name length 0x%x.",
			(unsigned long long)ni->mft_no,
			(unsigned)ni->name_len);
	/* Mirror the values from the base inode. */
	ni->seq_no = base_ni->seq_no;
	ni->uid	= base_ni->uid;
	ni->gid	= base_ni->gid;
	ni->file_attributes = base_ni->file_attributes;
	ni->creation_time = base_ni->creation_time;
	ni->last_data_change_time = base_ni->last_data_change_time;
	ni->last_mft_change_time = base_ni->last_mft_change_time;
	ni->last_access_time = base_ni->last_access_time;
	/* Indices cannot be hard-linked so link count is always 1. */
	ni->link_count = 1;
	/* Set inode type to zero but preserve permissions. */
	ni->mode = base_ni->mode & ~S_IFMT;
	/* Map the mft record for the base inode. */
	err = ntfs_mft_record_map(base_ni, &m);
	if (err) {
		ntfs_error(vol->mp, "Failed to map base mft record.");
		m = NULL;
		ctx = NULL;
		goto err;
	}
	ctx = ntfs_attr_search_ctx_get(base_ni, m);
	if (!ctx) {
		ntfs_error(vol->mp, "Failed to get attribute search context.");
		err = ENOMEM;
		goto err;
	}
	/* Find the index root attribute. */
	err = ntfs_attr_lookup(AT_INDEX_ROOT, ni->name, ni->name_len,
			CASE_SENSITIVE, 0, NULL, 0, ctx);
	if (err) {
		if (err == ENOENT)
			ntfs_error(vol->mp, "$INDEX_ROOT attribute is "
					"missing.");
		else
			ntfs_error(vol->mp, "Failed to lookup index root "
					"attribute.");
		goto err;
	}
	a = ctx->a;
	/* Set up the state. */
	if (a->non_resident) {
		ntfs_error(vol->mp, "Index root attribute is not resident.");
		goto err;
	}
	/* Ensure the attribute name is placed before the value. */
	if (a->name_length && (le16_to_cpu(a->name_offset) >=
			le16_to_cpu(a->value_offset))) {
		ntfs_error(vol->mp, "Index root attribute name is placed "
				"after the attribute value.");
		goto err;
	}
	/*
	 * Compressed/encrypted/sparse index root is not allowed, except for
	 * directories, where the flags just mean that newly created files in
	 * that directory should be created compressed/encrytped.  However,
	 * index root cannot be both compressed and encrypted.
	 */
	if (is_dir_index) {
		if (a->flags & ATTR_COMPRESSION_MASK)
			NInoSetCompressed(ni);
		if (a->flags & ATTR_IS_ENCRYPTED) {
			if (a->flags & ATTR_COMPRESSION_MASK) {
				ntfs_error(vol->mp, "Found encrypted and "
						"compressed index root "
						"attribute.");
				goto err;
			}
		}
		if (a->flags & ATTR_IS_SPARSE)
			NInoSetSparse(ni);
	} else if (a->flags & (ATTR_COMPRESSION_MASK | ATTR_IS_ENCRYPTED |
			ATTR_IS_SPARSE)) {
		ntfs_error(vol->mp, "Found compressed/encrypted/sparse index "
				"root attribute on non-directory index.");
		goto err;
	}
	ir = (INDEX_ROOT*)((u8*)a + le16_to_cpu(a->value_offset));
	ir_end = (u8*)ir + le32_to_cpu(a->value_length);
	index_end = (u8*)&ir->index + le32_to_cpu(ir->index.index_length);
	if (ir_end > (u8*)ctx->m + vol->mft_record_size ||
			index_end > ir_end ||
			ir->index.index_length != ir->index.allocated_size) {
		ntfs_error(vol->mp, "Index root attribute is corrupt.");
		goto err;
	}
	if (is_dir_index) {
		if (ir->type != AT_FILENAME) {
			ntfs_error(vol->mp, "Indexed attribute is not the "
					"filename attribute.");
			goto err;
		}
		if (ir->collation_rule != COLLATION_FILENAME) {
			ntfs_error(vol->mp, "Index collation rule is not "
					"COLLATION_FILENAME.");
			goto err;
		}
	} else if (ir->type) {
		ntfs_error(vol->mp, "Index type is not 0 (type is 0x%x).",
				(unsigned)le32_to_cpu(ir->type));
		goto err;
	}
	ntfs_debug("Index collation rule is 0x%x.",
			(unsigned)le32_to_cpu(ir->collation_rule));
	ni->collation_rule = ir->collation_rule;
	ni->block_size = le32_to_cpu(ir->index_block_size);
	if (ni->block_size & (ni->block_size - 1)) {
		ntfs_error(vol->mp, "Index block size (%u) is not a power of "
				"two.", (unsigned)ni->block_size);
		goto err;
	}
	if (ni->block_size > PAGE_SIZE) {
		ntfs_error(vol->mp, "Index block size (%u) > PAGE_SIZE (%u) "
				"is not supported.  Sorry.",
				(unsigned)ni->block_size, PAGE_SIZE);
		err = ENOTSUP;
		goto err;
	}
	if (ni->block_size < NTFS_BLOCK_SIZE) {
		ntfs_error(vol->mp, "Index block size (%u) < NTFS_BLOCK_SIZE "
				"(%d) is not supported.  Sorry.",
				(unsigned)ni->block_size, NTFS_BLOCK_SIZE);
		err = ENOTSUP;
		goto err;
	}
	ni->block_size_shift = ffs(ni->block_size) - 1;
	/* Determine the size of a vcn in the index. */
	if (vol->cluster_size <= ni->block_size) {
		ni->vcn_size = vol->cluster_size;
		ni->vcn_size_shift = vol->cluster_size_shift;
	} else {
		ni->vcn_size = vol->sector_size;
		ni->vcn_size_shift = vol->sector_size_shift;
	}
	/* Check for presence of index allocation attribute. */
	if (!(ir->index.flags & LARGE_INDEX)) {
		/* No index allocation. */
		ni->allocated_size = ni->data_size = ni->initialized_size = 0;
		/* We are done with the mft record, so we release it. */
		ntfs_attr_search_ctx_put(ctx);
		ntfs_mft_record_unmap(base_ni);
	} else {
		unsigned block_mask;

		/* LARGE_INDEX:  Index allocation present.  Setup state. */
		NInoSetIndexAllocPresent(ni);
		/* Find index allocation attribute. */
		err = ntfs_attr_lookup(AT_INDEX_ALLOCATION, I30, 4,
				CASE_SENSITIVE, 0, NULL, 0, ctx);
		if (err) {
			if (err == ENOENT)
				ntfs_error(vol->mp, "Index allocation "
						"attribute is not present but "
						"the index root attribute "
						"indicated it is.");
			else
				ntfs_error(vol->mp, "Failed to lookup index "
						"allocation attribute.");
			goto err;
		}
		a = ctx->a;
		if (!a->non_resident) {
			ntfs_error(vol->mp, "Index allocation attribute is "
					"resident.");
			goto err;
		}
		/*
		 * Ensure the attribute name is placed before the mapping pairs
		 * array.
		 */
		if (a->name_length && (le16_to_cpu(a->name_offset) >=
				le16_to_cpu(a->mapping_pairs_offset))) {
			ntfs_error(vol->mp, "Index allocation attribute name "
					"is placed after the mapping pairs "
					"array.");
			goto err;
		}
		if (a->flags & ATTR_IS_ENCRYPTED) {
			ntfs_error(vol->mp, "Index allocation attribute is "
					"encrypted.");
			goto err;
		}
		if (a->flags & ATTR_IS_SPARSE) {
			ntfs_error(vol->mp, "Index allocation attribute is "
					"sparse.");
			goto err;
		}
		if (a->flags & ATTR_COMPRESSION_MASK) {
			ntfs_error(vol->mp, "Index allocation attribute is "
					"compressed.");
			goto err;
		}
		if (a->lowest_vcn) {
			ntfs_error(vol->mp, "First extent of index allocation "
					"attribute has non-zero lowest_vcn.");
			goto err;
		}
		ni->allocated_size = sle64_to_cpu(a->allocated_size);
		ni->data_size = sle64_to_cpu(a->data_size);
		ni->initialized_size = sle64_to_cpu(a->initialized_size);
		/*
		 * Verify the sizes are sane.  In particular both the data size
		 * and the initialized size must be multiples of the index
		 * block size or we will panic() when reading the boundary in
		 * ntfs_cluster_iodone().
		 *
		 * Also the allocated size must be a multiple of the volume
		 * cluster size.
		 */
		block_mask = ni->block_size - 1;
		if (ni->allocated_size & vol->cluster_size_mask ||
				ni->data_size & block_mask ||
				ni->initialized_size & block_mask) {
			ntfs_error(vol->mp, "$INDEX_ALLOCATION attribute "
					"contains invalid size.  Inode 0x%llx "
					"is corrupt.  Run chkdsk.",
					(unsigned long long)ni->mft_no);
			goto err;
		}
		/*
		 * We are done with the mft record, so we release it.
		 * Otherwise we would deadlock in ntfs_attr_inode_get().
		 */
		ntfs_attr_search_ctx_put(ctx);
		ntfs_mft_record_unmap(base_ni);
		m = NULL;
		ctx = NULL;
		/* Get the index bitmap attribute inode. */
		err = ntfs_attr_inode_get(base_ni, AT_BITMAP, I30, 4, FALSE,
				&bni);
		if (err) {
			ntfs_error(vol->mp, "Failed to get bitmap attribute.");
			goto err;
		}
		if (NInoCompressed(bni) || NInoEncrypted(bni) ||
				NInoSparse(bni)) {
			ntfs_error(vol->mp, "Bitmap attribute is compressed "
					"and/or encrypted and/or sparse.");
			(void)vnode_put(bni->vn);
			goto err;
		}
		/* Consistency check bitmap size vs. index allocation size. */
		if ((bni->data_size << 3) < (ni->data_size >>
				ni->block_size_shift)) {
			ntfs_error(vol->mp, "Index bitmap too small (0x%llx) "
					"for index allocation (0x%llx).",
					(unsigned long long)bni->data_size,
					(unsigned long long)ni->data_size);
			(void)vnode_put(bni->vn);
			goto err;
		}
		(void)vnode_put(bni->vn);
	}
	/*
	 * Make sure the base inode and vnode do not go away and attach the
	 * base inode to the index inode.
	 */
	vn = base_ni->vn;
	if (!vn)
		panic("%s(): No vnode attached to base inode 0x%llx.",
				__FUNCTION__,
				(unsigned long long)base_ni->mft_no);
	err = vnode_ref(vn);
	if (err)
		ntfs_error(vol->mp, "vnode_ref() failed!");
	OSIncrementAtomic(&base_ni->nr_refs);
	ni->base_ni = base_ni;
	ni->nr_extents = -1;
	ntfs_debug("Done.");
	return 0;
err:
	if (ctx)
		ntfs_attr_search_ctx_put(ctx);
	if (m)
		ntfs_mft_record_unmap(base_ni);
	if (!err)
		err = EIO;
	ntfs_error(vol->mp, "Failed (error %d) for index inode 0x%llx, "
			"index name_len 0x%x.  Run chkdsk.", (int)err,
			(unsigned long long)ni->mft_no,
			(unsigned)ni->name_len);
	if (err != ENOTSUP && err != ENOMEM)
		NVolSetErrors(vol);
	return err;
}

/**
 * ntfs_inode_free - free an ntfs inode
 * @ni:		ntfs inode to free
 *
 * Free the resources used by the ntfs inode @ni as well as @ni itself, which
 * is NInoReclaim(), unhashed, and all waiters have been woken up.
 */
static inline void ntfs_inode_free(ntfs_inode *ni)
{
	/* No need to lock at this stage as no one else has a reference. */
	if (ni->nr_extents > 0) {
		int i;
		
		for (i = 0; i < ni->nr_extents; i++)
			ntfs_inode_reclaim(ni->extent_nis[i]);
		OSFree(ni->extent_nis, ((ni->nr_extents + 3) & ~3) *
				sizeof(ntfs_inode*), ntfs_malloc_tag);
	}
	/*
	 * If this is an attribute or index inode, release the reference to the
	 * vnode of the base inode if it is attached.
	 */
	if (NInoAttr(ni) && ni->nr_extents == -1) {
		ntfs_inode *base_ni = ni->base_ni;
		OSDecrementAtomic(&base_ni->nr_refs);
		vnode_rele(base_ni->vn);
	}
	if (ni->rl.alloc)
		OSFree(ni->rl.rl, ni->rl.alloc, ntfs_malloc_tag);
	if (ni->attr_list)
		OSFree(ni->attr_list, ni->attr_list_size, ntfs_malloc_tag);
	if (ni->attr_list_rl.alloc)
		OSFree(ni->attr_list_rl.rl, ni->attr_list_rl.alloc,
				ntfs_malloc_tag);
	if (ni->name_len && ni->name != I30)
		OSFree(ni->name, (ni->name_len + 1) * sizeof(ntfschar),
				ntfs_malloc_tag);
	/* Destroy all the locks before finally discarding the ntfs inode. */
	lck_rw_destroy(&ni->lock, ntfs_lock_grp);
	lck_spin_destroy(&ni->size_lock, ntfs_lock_grp);
	ntfs_rl_deinit(&ni->rl);
	lck_mtx_destroy(&ni->mrec_lock, ntfs_lock_grp);
	ntfs_rl_deinit(&ni->attr_list_rl);
	lck_mtx_destroy(&ni->extent_lock, ntfs_lock_grp);
	OSFree(ni, sizeof(ntfs_inode), ntfs_malloc_tag);
}

/**
 * ntfs_inode_reclaim - destroy an ntfs inode freeing all its resources
 * @ni:		ntfs inode to destroy
 *
 * Destroy the ntfs inode @ni freeing all its resources in the process.  We are
 * assured that no-one can get the inode because to do that they would have to
 * take a reference on the corresponding vnode and that is not possible because
 * the vnode is flagged for termination thus the vnode_get*() functions will
 * return an error.
 *
 * This function cannot fail and always returns 0.
 */
errno_t ntfs_inode_reclaim(ntfs_inode *ni)
{
	vnode_t vn;

	ntfs_debug("Entering for mft_no 0x%llx.",
			(unsigned long long)ni->mft_no);
	lck_mtx_lock(&ntfs_inode_hash_lock);
	NInoSetReclaim(ni);
	ntfs_inode_hash_rm_nolock(ni);
	/*
	 * Need this for the error handling code paths but is ok for normal
	 * code path, too.
	 */
	NInoClearAllocLocked(ni);
	lck_mtx_unlock(&ntfs_inode_hash_lock);
	/* In case someone is waiting on the inode do a wakeup. */
	ntfs_inode_wakeup(ni);
	/* Detach the ntfs inode from its vnode, if there is one. */
	vn = ni->vn;
	if (vn)
		vnode_clearfsnode(vn);
	/*
	 * We now have exclusive access to the ntfs inode and as it is unhashed
	 * no-one else can ever find it thus we can finally destroy it.
	 */
	ntfs_inode_free(ni);
	ntfs_debug("Done.");
	return 0;
}

#ifdef DEBUG
#include <sys/ubc_internal.h>
#include <sys/vnode_internal.h>
#endif /* DEBUG */

/**
 * ntfs_inode_sync - synchronize an inode's in-core state with that on disk
 * @ni:		ntfs inode to synchronize to disk
 * ioflags:	flags describing the i/o request
 *
 * Write all dirty cached data belonging/related to the ntfs inode ni to disk.
 *
 * If @ioflags has the IS_SYNC bit set, wait for all i/o to complete before
 * returning.
 *
 * If @ioflags has the IO_CLOSE bit set, this signals cluster_push() that the
 * i/o is issued from the close path (or in our case more precisely from the
 * VNOP_INACTIVE() path.
 *
 * Return 0 on success and the error code on error.
 */
errno_t ntfs_inode_sync(ntfs_inode *ni, const int ioflags)
{
	ntfs_volume *vol = ni->vol;
	vnode_t vn = ni->vn;
	int err = 0;

	ntfs_debug("Entering for inode 0x%llx, %ssync i/o, ioflags 0x%04x.",
			(unsigned long long)ni->mft_no,
			(ioflags & IO_SYNC) ? "a" : "", ioflags);
	/* TODO: Deny access to encrypted attributes, just like NT4. */
	if (NInoEncrypted(ni)) {
		ntfs_warning(vol->mp, "Denying sync of encrypted attribute "
				"inode (EACCES).");
		return EACCES;
	}
	if (NInoSparse(ni)) {
		ntfs_error(vol->mp, "Syncing sparse file inodes is not "
				"implemented yet, sorry.");
		return ENOTSUP;
	}
	lck_rw_lock_shared(&ni->lock);
	if (NInoNonResident(ni)) {
		int (*callback)(buf_t, void *);

		if (NInoCompressed(ni) && !NInoRaw(ni)) {
#if 0
			err = ntfs_inode_sync_compressed(ni, uio, size,
					ioflags);
			if (!err)
				ntfs_debug("Done (ntfs_inode_sync_compressed()"
						").");
			else
				ntfs_error(vol->mp, "Failed ("
						"ntfs_inode_sync_compressed(), "
						"error %d).", err);
#endif
			lck_rw_unlock_shared(&ni->lock);
			ntfs_error(vol->mp, "Syncing compressed file inodes "
					"is not implemented yet, sorry.");
			return ENOTSUP;
		}
		callback = NULL;
		if (NInoEncrypted(ni)) {
			callback = ntfs_cluster_iodone;
			lck_rw_unlock_shared(&ni->lock);
			ntfs_error(vol->mp, "Syncing metadata and/or "
					"encrypted file inodes is not "
					"implemented yet, sorry.");
			return ENOTSUP;
		}
		/*
		 * Write any dirty clusters.  We are guaranteed not to have any
		 * for mst protected attributes.
		 */
		if (!NInoMstProtected(ni)) {
			/* Write out any dirty clusters. */
			ntfs_debug("Calling cluster_push_ext().");
			(void)cluster_push_ext(vn, ioflags, callback, NULL);
#ifdef DEBUG
		} else {
			struct cl_writebehind *wbp;

			wbp = vn->v_ubcinfo->cl_wbehind;
			if (wbp) {
				ntfs_warning(vol->mp, "mst and wbp!");
				if (wbp->cl_number || wbp->cl_scmap ||
						wbp->cl_scdirty)
					ntfs_warning(vol->mp, "mst and wbp "
							"has cl_number or "
							"cl_scmap or "
							"cl_scdirty.");
			}
#endif /* DEBUG */
		}
		/* Flush all dirty buffers associated with the vnode. */
		ntfs_debug("Calling buf_flushdirtyblks().");
		buf_flushdirtyblks(vn, ioflags, 0 /* lock flags */,
				"ntfs_inode_sync");
#ifdef DEBUG
	} else {
		struct cl_writebehind *wbp;

		wbp = vn->v_ubcinfo->cl_wbehind;
		if (wbp) {
			ntfs_warning(vol->mp, "resident and wbp!");
			if (wbp->cl_number || wbp->cl_scmap ||
					wbp->cl_scdirty)
				ntfs_warning(vol->mp, "resident and wbp has "
						"cl_number or cl_scmap or "
						"cl_scdirty.");
		}
		if (vnode_hasdirtyblks(vn))
			ntfs_warning(vol->mp, "resident and "
					"vnode_hasdirtyblks!");
#endif /* DEBUG */
	}
	/* ubc_msync() cannot be called with the inode lock held. */
	lck_rw_unlock_shared(&ni->lock);
	/*
	 * If we have any dirty pages in the VM page cache, write them out now.
	 */
	if (NInoTestClearDirtyData(ni)) {
		ntfs_debug("Calling ubc_msync() for inode data.");
		err = ubc_msync(vn, 0, ubc_getsize(vn), NULL, UBC_PUSHALL);
		if (err)
			ntfs_error(vol->mp, "ubc_msync() of data failed with "
					"error %d.", err);
	}
	/*
	 * If we have a dirty mft record corresponding to this attribute, write
	 * it out now.
	 */
	if (NInoTestClearDirty(ni)) {
		s64 ofs;
		int err2;

		/*
		 * If this is not an attribute inode the mft record is easy to
		 * determine as it is the base mft record.  Otherwise we need
		 * to locate the attribute to determine which mft record it is
		 * in.  For inodes without an attribute list, this is once
		 * again easy as it is the inode number.  But for inodes with
		 * an attribute list attribute it could be any extent mft
		 * record so we really have to look it up.
		 */
		if (!NInoAttr(ni)) {
			ofs = (ni->mft_no << vol->mft_record_size_shift) &
					~PAGE_MASK_64;
			ntfs_debug("Calling ubc_msync() for mft record of "
					"non-attribute inode.");
			err2 = ubc_msync(vol->mft_ni->vn, ofs, ofs + PAGE_SIZE,
					NULL, UBC_PUSHALL);
			if (err2) {
				ntfs_error(vol->mp, "ubc_msync() of mft "
						"record failed with error %d.",
						err2);
				if (!err)
					err = err2;
			}
		} else {
			// TODO: Do it (see above description)...
			ntfs_error(vol->mp, "Syncing mft record of attribute "
					"inodes is not implemented yet.");
		}
	}
	// TODO: If this is the base inode need to loop over all extent inodes
	// and write their mft records if they are dirty...
	if (NInoAttrList(ni))
		ntfs_error(vol->mp, "Syncing extent mft records is not "
				"implemented yet.");
	if (!err)
		ntfs_debug("Done.");
	return err;
}
