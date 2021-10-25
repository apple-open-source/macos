/*
 * ntfs_inode.c - NTFS kernel inode operations.
 *
 * Copyright (c) 2006-2011 Anton Altaparmakov.  All Rights Reserved.
 * Portions Copyright (c) 2006-2011 Apple Inc.  All Rights Reserved.
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
#include <sys/queue.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/ucred.h>
#include <sys/ubc.h>
#include <sys/vnode.h>

#include <string.h>

#include <libkern/libkern.h>
#include <libkern/OSAtomic.h>

#include <kern/debug.h>
#include <kern/locks.h>
#include <kern/sched_prim.h>

#include <IOKit/IOLib.h>

#include <mach/machine/vm_param.h>

#include "ntfs.h"
#include "ntfs_attr.h"
#include "ntfs_debug.h"
#include "ntfs_dir.h"
#include "ntfs_hash.h"
#include "ntfs_inode.h"
#include "ntfs_mft.h"
#include "ntfs_page.h"
#include "ntfs_runlist.h"
#include "ntfs_sfm.h"
#include "ntfs_time.h"
#include "ntfs_types.h"
#include "ntfs_unistr.h"
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
 * Return true if the attributes match and false if not.
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
		ntfs_volume *vol;

		/* A fake inode describing an attribute. */
		if (ni->type != na->type)
			return FALSE;
		vol = ni->vol;
		if (!ntfs_are_names_equal(ni->name, ni->name_len,
				na->name, na->name_len, NVolCaseSensitive(vol),
				vol->upcase, vol->upcase_len))
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
	ni->nr_opens = 0;
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
	ni->rdev = (dev_t)0;
	ni->file_attributes = 0;
	ni->last_access_time = ni->last_mft_change_time =
			ni->last_data_change_time = ni->creation_time =
			(struct timespec) {
		.tv_sec = 0,
		.tv_nsec = 0,
	};
	ntfs_rl_init(&ni->rl);
	lck_mtx_init(&ni->buf_lock, ntfs_lock_grp, ntfs_lock_attr);
	ni->mft_ni = NULL;
	ni->m_buf = NULL;
	ni->m_dbuf = NULL;
	ni->m = NULL;
	ni->attr_list_size = 0;
	ni->attr_list_alloc = 0;
	ni->attr_list = NULL;
	ntfs_rl_init(&ni->attr_list_rl);
	ni->last_set_bit = -1;
	ni->vcn_size = 0;
	ni->collation_rule = 0;
	ni->vcn_size_shift = 0;
	ni->nr_dirhints = 0;
	ni->dirhint_tag = 0;
	TAILQ_INIT(&ni->dirhint_list);
	lck_mtx_init(&ni->extent_lock, ntfs_lock_grp, ntfs_lock_attr);
	ni->nr_extents = 0;
	ni->extent_alloc = 0;
	lck_mtx_init(&ni->attr_nis_lock, ntfs_lock_grp, ntfs_lock_attr);
	ni->nr_attr_nis = 0;
	ni->attr_nis_alloc = 0;
	ni->base_ni = NULL;
	ni->base_attr_nis_lock = NULL;
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
	 *
	 * We now also have a second common name and that is the name of the
	 * resource fork so special case this, too.  This also allows us to
	 * identify resource fork attribute inodes easily by simply comparing
	 * their name for equality with the global constant
	 * NTFS_SFM_RESOURCEFORK_NAME.
	 *
	 * Simillarly we also add NTFS_SFM_AFPINFO_NAME as this is also quite
	 * common as it holds the backup time and the Finder info.
	 */
	if (na->name_len && na->name != I30 &&
			na->name != NTFS_SFM_RESOURCEFORK_NAME &&
			na->name != NTFS_SFM_AFPINFO_NAME) {
		unsigned i = na->name_len * sizeof(ntfschar);
		ni->name = IOMallocData(i + sizeof(ntfschar));
		if (!ni->name)
			return ENOMEM;
		memcpy(ni->name, na->name, i);
		ni->name[na->name_len] = 0;
	}
	return 0;
}

static errno_t ntfs_inode_read(ntfs_inode *ni);
static errno_t ntfs_attr_inode_read_or_create(ntfs_inode *base_ni,
		ntfs_inode *ni, const int options);
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
 *	VSTR = Not used in current OS X kernel.
 *	VCPLX = Not used in current OS X kernel.
 */
static inline enum vtype ntfs_inode_get_vtype(ntfs_inode *ni)
{
	/*
	 * Attribute inodes do not really have a type.
	 *
	 * However, the current OS X kernel does not allow use of ubc with
	 * anything other than regular files (i.e. VREG vtype), thus we need to
	 * return VREG for named $DATA attributes, i.e. named streams, so that
	 * they can be accessed via mmap like regular files.  And the same goes
	 * for index inodes which we need to be able to read via the ubc.
	 *
	 * And a further however is that ntfs_unmount() uses vnode_iterate() to
	 * flush all inodes of the mounted volume and vnode_iterate() skips
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
 * If the the inode is an attribute inode, set it up as a named stream vnode so
 * it does not block non-forced unmounts in the VFS.
 *
 * Return 0 on success and errno on error.
 */
errno_t ntfs_inode_add_vnode_attr(ntfs_inode *ni, const BOOL is_system,
		vnode_t parent_vn, struct componentname *cn, BOOL isstream)
{
	s64 data_size;
	errno_t err;
	enum vtype vtype;
	struct vnode_fsparam vn_fsp;
	BOOL cache_name = FALSE;

	ntfs_debug("Entering.");
	/* Get the vnode type corresponding to the inode mode type. */
	vtype = ntfs_inode_get_vtype(ni);
	/*
	 * Get the data size for regular files, attributes, directories, and
	 * symbolic links.
	 */
	data_size = 0;
	if (vtype == VREG || vtype == VDIR || vtype == VLNK)
		data_size = ni->data_size;
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
		.vnfs_rdev = ni->rdev,	/* Device if vtype is VBLK or VCHR.  We
					   can just return @ni->rdev as that is
					   zero for all other vtypes. */
		.vnfs_filesize = data_size, /* Data size of attribute.  No
					   need for size lock as we are only
					   user of inode at present. */
		.vnfs_cnp = cn,		/* Component name to assign as the name
					   of the vnode and optionally to add
					   it to the namecache. */
		.vnfs_flags = VNFS_ADDFSREF, /* VNFS_* flags.  We want to have
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
	/*
	 * If this is a named stream inode, then set it's parent to
	 * NULL.  This way the VFS will set up the parent vnode and then
	 * at the end of the VNOP_GETNAMEDSTREAM call, the VFS will call
	 *  vnode_update_identity, which sets the parent and increments the
	 * kusecount on the vnode.  If the parent is already set our kusecount
	 * can go negative!
	 */
	if (isstream) {
		vn_fsp.vnfs_dvp = NULL;
	}	

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
 * @lock:	locking options (see below)
 * @nni:	destination pointer for the obtained ntfs inode
 * @parent_vn:	vnode of directory containing the inode to return or NULL
 * @cn:		componentname containing the name of the inode to return
 *
 * Obtain the ntfs inode corresponding to a specific normal inode (i.e. a
 * file or directory).  If @is_system is true the created vnode is marked as a
 * system vnode (via the VSYSTEM flag).
 *
 * If @lock is LCK_RW_TYPE_SHARED the inode will be returned locked for reading
 * (@nni->lock) and if it is LCK_RW_TYPE_EXCLUSIVE the inode will be returned
 * locked for writing (@nni->lock).  As a special case if @lock is 0 it means
 * the inode to be returned is already locked so do not lock it.  This requires
 * that the inode is already present in the inode cache.  If it is not it
 * cannot already be locked and thus you will get a panic().
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
		const lck_rw_type_t lock, ntfs_inode **nni, vnode_t parent_vn,
		struct componentname *cn)
{
	ntfs_inode *ni;
	vnode_t vn;
	errno_t err;
	ntfs_attr na;

	ntfs_debug("Entering for mft_no 0x%llx, is_system is %s, lock 0x%x.",
			(unsigned long long)mft_no,
			is_system ? "true" : "false", (unsigned)lock);
retry:
	na = (ntfs_attr) {
		.mft_no = mft_no,
		.type = AT_UNUSED,
		.raw = FALSE,
	};
	ni = ntfs_inode_hash_get(vol, &na);
	if (!ni) {
		ntfs_debug("Failed (ENOMEM).");
		return ENOMEM;
	}
	/*
	 * Lock the inode for reading/writing as requested by the caller.
	 *
	 * If the caller specified that the inode is already locked, verify
	 * that the inode was already in the cache and panic() if not.
	 */
	switch (lock) {
	case LCK_RW_TYPE_EXCLUSIVE:
		lck_rw_lock_exclusive(&ni->lock);
		break;
	case LCK_RW_TYPE_SHARED:
		lck_rw_lock_shared(&ni->lock);
		break;
	case 0:
		if (NInoAlloc(ni))
			panic("%s(): !lock but NInoAlloc(ni)\n", __FUNCTION__);
		break;
	default:
		panic("%s(): lock is 0x%x which is invalid!\n", __FUNCTION__,
				lock);
	}
	if (!NInoAlloc(ni)) {
		/* The inode was already cached. */
		vn = ni->vn;
		/*
		 * Do not allow open-unlinked files to be opened again and
		 * retry for NInoDeleted() inodes.
		 *
		 * Otherwise this could for example happen via NFS or VolFS
		 * style access for example.
		 */
		if (!ni->link_count) {
			ntfs_debug("Mft_no 0x%llx has been unlinked, "
					"returning ENOENT.",
					(unsigned long long)ni->mft_no);
			err = ENOENT;
			goto err;
		}
		if (NInoDeleted(ni)) {
			if (lock == LCK_RW_TYPE_EXCLUSIVE)
				lck_rw_unlock_exclusive(&ni->lock);
			else if (lock == LCK_RW_TYPE_SHARED)
				lck_rw_unlock_shared(&ni->lock);
			if (vn) {
				/* Remove the inode from the name cache. */
				cache_purge(vn);
				(void)vnode_put(vn);
			} else
				ntfs_inode_reclaim(ni);
			goto retry;
		}
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
						__FUNCTION__,
						(unsigned long long)mft_no);
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

			err = ntfs_index_inode_get(ni, I30, 4, is_system, &ini);
			if (err) {
				ntfs_error(vol->mp, "Failed to get index "
						"inode.");
				/* Kill the bad inode. */
				vn = ni->vn;
				(void)vnode_recycle(vn);
				goto err;
			}
			/*
			 * Copy the sizes from the index inode to the directory
			 * inode so we do not need to get the index inode in
			 * ntfs_vnop_getattr().
			 *
			 * Note @ni is totally private to us thus no need to
			 * lock the sizes for modification.  On the other hand
			 * @ini is not private thus we need to lock its sizes.
			 */
			lck_spin_lock(&ini->size_lock);
			ni->allocated_size = ini->allocated_size;
			ni->data_size = ini->data_size;
			ni->initialized_size = ini->initialized_size;
			lck_spin_unlock(&ini->size_lock);
			/* We are done with the index vnode. */
			(void)vnode_put(ini->vn);
		}
		ntfs_inode_unlock_alloc(ni);
		*nni = ni;
		ntfs_debug("Done (added to cache; allocated_size %lld data_size %lld initialized_size %lld).", ni->allocated_size, ni->data_size, ni->initialized_size);
		return err;
	}
	if (lock == LCK_RW_TYPE_EXCLUSIVE)
		lck_rw_unlock_exclusive(&ni->lock);
	else if (lock == LCK_RW_TYPE_SHARED)
		lck_rw_unlock_shared(&ni->lock);
	ntfs_inode_reclaim(ni);
	ntfs_debug("Failed (inode read/vnode create).");
	return err;
err:
	if (lock == LCK_RW_TYPE_EXCLUSIVE)
		lck_rw_unlock_exclusive(&ni->lock);
	else if (lock == LCK_RW_TYPE_SHARED)
		lck_rw_unlock_shared(&ni->lock);
	if (vn)
		(void)vnode_put(vn);
	else
		ntfs_inode_reclaim(ni);
	return err;
}

/**
 * ntfs_attr_inode_lookup - obtain an ntfs attribute inode if it is cached
 * @base_ni:	base inode if @ni is not raw and non-raw inode of @ni otherwise
 * @type:	attribute type
 * @name:	Unicode name of the attribute (NULL if unnamed)
 * @name_len:	length of @name in Unicode characters (0 if unnamed)
 * @raw:	whether to get the raw inode (TRUE) or not (FALSE)
 * @nni:	destination pointer for the obtained attribute ntfs inode
 *
 * Check if the ntfs inode corresponding to the attribute specified by @type,
 * @name, and @name_len, which is present in the base mft record specified by
 * the ntfs inode @base_ni is cached in the inode cache and if so return it
 * taking a reference on its vnode.
 *
 * If @raw is true @base_ni is the non-raw inode to which @ni belongs rather
 * than the base inode.
 *
 * If the attribute inode is in the cache, it is returned with an iocount
 * reference on the attached vnode.
 *
 * Return 0 on success and errno on error.
 *
 * Locking: The base ntfs inode @base_ni must be locked (@base_ni->lock).
 */
errno_t ntfs_attr_inode_lookup(ntfs_inode *base_ni, ATTR_TYPE type,
		ntfschar *name, u32 name_len, const BOOL raw, ntfs_inode **nni)
{
	ntfs_inode *ni;
	ntfs_attr na;

	ntfs_debug("Entering for mft_no 0x%llx, type 0x%x, name_len 0x%x, "
			"raw is %s.", (unsigned long long)base_ni->mft_no,
			le32_to_cpu(type), (unsigned)name_len,
			raw ? "true" : "false");
	/* Make sure no one calls ntfs_attr_inode_get() for indices. */
	if (type == AT_INDEX_ALLOCATION)
		panic("%s() called for an index.\n", __FUNCTION__);
	if (!base_ni->vn)
		panic("%s() called with a base inode that does not have a "
				"vnode attached.\n", __FUNCTION__);
	na = (ntfs_attr) {
		.mft_no = base_ni->mft_no,
		.type = type,
		.name = name,
		.name_len = name_len,
		.raw = raw,
	};
	ni = ntfs_inode_hash_lookup(base_ni->vol, &na);
	if (!ni) {
		ntfs_debug("Not cached (ENOENT).");
		return ENOENT;
	}
	*nni = ni;
	ntfs_debug("Done (found in cache).");
	return 0;
}

/**
 * ntfs_attr_inode_get_or_create - obtain/create an ntfs attribute inode
 * @base_ni:	base inode if @ni is not raw and non-raw inode of @ni otherwise
 * @type:	attribute type
 * @name:	Unicode name of the attribute (NULL if unnamed)
 * @name_len:	length of @name in Unicode characters (0 if unnamed)
 * @is_system:	true if the inode is a system inode and false otherwise
 * @raw:	whether to get the raw inode (TRUE) or not (FALSE)
 * @options:	options specifying the get and/or create behaviour
 * @lock:	locking options (see below)
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
 * If @options does not specify XATTR_CREATE nor XATTR_REPLACE the attribute
 * will be created if it does not exist already and then will be opened.
 *
 * If @options specifies XATTR_CREATE the call will fail if the attribute
 * already exists, i.e. the existing attribute will not be opened.
 *
 * If @options specifies XATTR_REPLACE the call will fail if the attribute does
 * not exist, i.e. the new attribute will not be created, i.e. this is the
 * equivalent of ntfs_attr_inode_get().
 *
 * A special case is the resource fork (@name == NTFS_SFM_RESOURCEFORK_NAME).
 * If it exists but has zero size it is treated as if it does not exist when
 * handling the XATTR_CREATE and XATTR_REPLACE flags in @options.  Thus if the
 * resource fork exists but is zero size, a call with XATTR_CREATE set in
 * @options will succeed as if it did not already exist and a call with
 * XATTR_REPLACE set in @options will fail as if it did not already exist.
 *
 * If @lock is LCK_RW_TYPE_SHARED the attribute inode will be returned locked
 * for reading (@nni->lock) and if it is LCK_RW_TYPE_EXCLUSIVE the attribute
 * inode will be returned locked for writing (@nni->lock).  As a special case
 * if @lock is 0 it means the inode to be returned is already locked so do not
 * lock it.  This requires that the inode is already present in the inode
 * cache.  If it is not it cannot already be locked and thus you will get a
 * panic().
 *
 * If the attribute inode is in the cache, it is returned with an iocount
 * reference on the attached vnode.
 *
 * If the inode is not in the cache, a new ntfs inode is allocated and
 * initialized, ntfs_attr_inode_read_or_create() is called to read it in/create
 * it and fill in the remainder of the ntfs inode structure before finally a
 * new vnode is created and attached to the new ntfs inode.  The inode is then
 * returned with an iocount reference taken on its vnode.
 *
 * Note we use the base vnode as the parent vnode of the attribute vnode to be
 * in line with how OS X treats named stream vnodes.
 *
 * Note, for index allocation attributes, you need to use ntfs_index_inode_get()
 * instead of ntfs_attr_inode_get() as working with indices is a lot more
 * complex.
 *
 * Return 0 on success and errno on error.  In the error case the lock state of
 * the inode is left in the same state as it was before this function was
 * called.
 *
 * TODO: For now we do not store a name for attribute inodes.
 */
errno_t ntfs_attr_inode_get_or_create(ntfs_inode *base_ni, ATTR_TYPE type,
		ntfschar *name, u32 name_len, const BOOL is_system,
		const BOOL raw, const int options, const lck_rw_type_t lock,
		ntfs_inode **nni)
{
	ntfs_inode *ni;
	vnode_t vn;
	int err;
	BOOL promoted;
	ntfs_attr na;
	BOOL isstream = FALSE;

	ntfs_debug("Entering for mft_no 0x%llx, type 0x%x, name_len 0x%x, "
			"is_system is %s, raw is %s, options 0x%x, lock 0x%x.",
			(unsigned long long)base_ni->mft_no, le32_to_cpu(type),
			(unsigned)name_len, is_system ? "true" : "false",
			raw ? "true" : "false", (unsigned)options,
			(unsigned)lock);
	/* Make sure no one calls us for indices. */
	if (type == AT_INDEX_ALLOCATION)
		panic("%s() called for an index.\n", __FUNCTION__);
	if (!base_ni->vn)
		panic("%s() called with a base inode that does not have a "
				"vnode attached.\n", __FUNCTION__);
	promoted = FALSE;
retry:
	na = (ntfs_attr) {
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
	/*
	 * Lock the inode for reading/writing as requested by the caller.
	 *
	 * If the caller specified that the inode is already locked, verify
	 * that the inode was already in the cache and panic() if not.
	 */
	if (lock) {
		if (promoted || lock == LCK_RW_TYPE_EXCLUSIVE)
			lck_rw_lock_exclusive(&ni->lock);
		else if (lock == LCK_RW_TYPE_SHARED)
			lck_rw_lock_shared(&ni->lock);
		else
			panic("%s(): lock is 0x%x which is invalid!\n",
					__FUNCTION__, lock);
	} else if (NInoAlloc(ni))
		panic("%s(): !lock but NInoAlloc(ni)\n", __FUNCTION__);
	if (!NInoAlloc(ni)) {
		/* The inode was already cached. */
		vn = ni->vn;
		/*
		 * If @options specifies XATTR_REPLACE do not allow
		 * open-unlinked or NInoDeleted() attribute inodes to be opened
		 * again.
		 *
		 * Otherwise retry if the attribute inode is NInoDeleted() and
		 * re-link it if it is open-unlinked.  In the latter case also
		 * truncate it to zero size.
		 */
		if (NInoDeleted(ni) || !ni->link_count) {
			if (NInoDeleted(ni)) {
				/* Remove the inode from the name cache. */
				if (vn)
					cache_purge(vn);
			}
			if (options & XATTR_REPLACE) {
				ntfs_debug("Attribute in mft_no 0x%llx is "
						"deleted/unlinked, returning "
						"ENOENT.",
						(unsigned long long)ni->mft_no);
				err = ENOENT;
				goto err;
			}
			/*
			 * XATTR_REPLACE is not specified thus retry if the
			 * attribute inode is NInoDeleted().
			 */
relocked:
			if (NInoDeleted(ni)) {
				if (lock) {
					if (promoted || lock ==
							LCK_RW_TYPE_EXCLUSIVE)
						lck_rw_unlock_exclusive(
								&ni->lock);
					else
						lck_rw_unlock_shared(&ni->lock);
				}
				if (vn)
					(void)vnode_put(vn);
				else
					ntfs_inode_reclaim(ni);
				goto retry;
			}
			/*
			 * The attribute inode is open-unlinked, we need it
			 * locked exclusive before we can re-link it.
			 */
			if (lock == LCK_RW_TYPE_SHARED && !promoted) {
				promoted = TRUE;
				if (!lck_rw_lock_shared_to_exclusive(
						&ni->lock)) {
					/*
					 * We dropped the lock so take it
					 * again and then redo the checking for
					 * the inode being deleted.
					 */
					lck_rw_lock_exclusive(&ni->lock);
					goto relocked;
				}
			}
			if (ni->link_count) {
				/*
				 * Someone else already re-linked it.  If
				 * @options specifies XATTR_CREATE we need to
				 * abort.
				 */
				goto exists;
			}
			/*
			 * Re-link the attribute inode and truncate it
			 * to zero size thus pretending we created it.
			 */
			ntfs_debug("Re-instantiating open-unlinked attribute "
					"in mft_no 0x%llx.",
					(unsigned long long)ni->mft_no);
			ni->link_count = 1;
			err = ntfs_attr_resize(ni, 0, 0, NULL);
			if (err) {
				ntfs_error(ni->vol->mp, "Failed to truncate "
						"re-linked attribute in "
						"mft_no 0x%llx (error %d).",
						(unsigned long long)ni->mft_no,
						err);
				goto err;
			}
		} else {
			/*
			 * The attribute inode already exists.
			 *
			 * If it is the empty resource fork we need to fail if
			 * @options specifies XATTR_REPLACE.
			 *
			 * If @options specifies XATTR_CREATE we need to abort
			 * unless this is the resource fork and it is empty.
			 */
exists:
			if (name == NTFS_SFM_RESOURCEFORK_NAME) {
				s64 size;

				if (vn)
					size = ubc_getsize(vn);
				else {
					lck_spin_lock(&ni->size_lock);
					size = ni->data_size;
					lck_spin_unlock(&ni->size_lock);
				}
				if (!size) {
					if (options & XATTR_REPLACE) {
						ntfs_debug("Attribute mft_no "
								"0x%llx does "
								"not exist, "
								"returning "
								"ENOENT.",
								(unsigned long
								long)
								ni->mft_no);
						err = ENOENT;
						goto err;
					}
					if (options & XATTR_CREATE)
						goto allow_rsrc_fork;
				}
			}
			if (options & XATTR_CREATE) {
				ntfs_debug("Attribute mft_no 0x%llx already "
						"exists, returning EEXIST.",
						(unsigned long long)ni->mft_no);
				err = EEXIST;
				goto err;
			}
		}
allow_rsrc_fork:
		if (vn) {
			vnode_t parent_vn;

			parent_vn = vnode_getparent(vn);
			if (parent_vn != base_ni->vn) {
				ntfs_debug("Updating vnode identity with new "
						"parent vnode.");
				vnode_update_identity(vn, base_ni->vn, NULL,
						0, 0, VNODE_UPDATE_PARENT);
			}
			if (parent_vn)
				(void)vnode_put(parent_vn);
		}
		if (promoted)
			lck_rw_lock_exclusive_to_shared(&ni->lock);
		*nni = ni;
		ntfs_debug("Done (found in cache).");
		return 0;
	}
	/*
	 * We do not need to hold the inode lock exclusive as we already have
	 * guaranteed exclusive access to the attribute inode as NInoAlloc() is
	 * still set and we do not clear it until we are done thus demote it to
	 * a shared lock if we promoted it earlier.
	 */
	if (promoted)
		lck_rw_lock_exclusive_to_shared(&ni->lock);
	/*
	 * This is a freshly allocated inode, need to read it in/create it now.
	 * Also, need to allocate and attach a vnode to the new ntfs inode.
	 */
	err = ntfs_attr_inode_read_or_create(base_ni, ni, options);
	if (!err) {
		if (name == NTFS_SFM_RESOURCEFORK_NAME)
			isstream = TRUE;
		err = ntfs_inode_add_vnode_attr(ni, is_system, base_ni->vn, NULL, isstream);
	}
	if (!err) {
		ntfs_inode_unlock_alloc(ni);
		*nni = ni;
		ntfs_debug("Done (added to cache).");
		return err;
	}
	if (lock) {
		if (lock == LCK_RW_TYPE_SHARED)
			lck_rw_unlock_shared(&ni->lock);
		else
			lck_rw_unlock_exclusive(&ni->lock);
	}
	ntfs_inode_reclaim(ni);
	ntfs_debug("Failed (inode read/vnode create, error %d).", err);
	return err;
err:
	if (lock) {
		if (promoted || lock == LCK_RW_TYPE_EXCLUSIVE)
			lck_rw_unlock_exclusive(&ni->lock);
		else
			lck_rw_unlock_shared(&ni->lock);
	}
	if (vn)
		(void)vnode_put(vn);
	else
		ntfs_inode_reclaim(ni);
	return err;
}

/**
 * ntfs_index_inode_get - obtain an ntfs inode corresponding to an index
 * @base_ni:	ntfs base inode containing the index related attributes
 * @name:	Unicode name of the index
 * @name_len:	length of @name in Unicode characters
 * @is_system:	true if the inode is a system inode and false otherwise
 * @nni:	destination pointer for the obtained index ntfs inode
 *
 * Obtain the ntfs inode corresponding to the index specified by @name and
 * @name_len, which is present in the base mft record specified by the ntfs
 * inode @base_ni.  If @is_system is true the created vnode is marked as a
 * system vnode (via the VSYSTEM flag).
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
 * Note we use the base vnode as the parent vnode of the index vnode to be in
 * line with how OS X treats named stream vnodes.
 *
 * Return 0 on success and errno on error.
 *
 * TODO: For now we do not store a name for attribute inodes.
 */
errno_t ntfs_index_inode_get(ntfs_inode *base_ni, ntfschar *name, u32 name_len,
		const BOOL is_system, ntfs_inode **nni)
{
	ntfs_inode *ni;
	ntfs_attr na;
	int err;

	ntfs_debug("Entering for mft_no 0x%llx, name_len 0x%x, is_system is "
			"%s.", (unsigned long long)base_ni->mft_no,
			(unsigned)name_len, is_system ? "true" : "false");
	if (!base_ni->vn)
		panic("%s() called with a base inode that does not have a "
				"vnode attached.\n", __FUNCTION__);
	na = (ntfs_attr) {
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
	if (!NInoAlloc(ni)) {
		vnode_t vn;

		vn = ni->vn;
		/*
		 * Do not allow open-unlinked attribute inodes to be opened
		 * again.
		 */
		if (!ni->link_count) {
			ntfs_debug("Mft_no 0x%llx has been unlinked, "
					"returning ENOENT.",
					(unsigned long long)ni->mft_no);
			if (vn)
				(void)vnode_put(vn);
			else
				ntfs_inode_reclaim(ni);
			return ENOENT;
		}
		if (vn) {
			vnode_t parent_vn;

			parent_vn = vnode_getparent(vn);
			if (parent_vn != base_ni->vn) {
				ntfs_debug("Updating vnode identity with new "
						"parent vnode.");
				vnode_update_identity(vn, base_ni->vn, NULL,
						0, 0, VNODE_UPDATE_PARENT);
			}
			if (parent_vn)
				(void)vnode_put(parent_vn);
		}
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
		err = ntfs_inode_add_vnode(ni, is_system, base_ni->vn, NULL);
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
	na = (ntfs_attr) {
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
		if (!seq_no || ni->seq_no == seq_no) {
			*ext_ni = ni;
			ntfs_debug("Done (found in cache).");
			return 0;
		}
		ntfs_inode_reclaim(ni);
		ntfs_error(base_ni->vol->mp, "Found stale extent mft "
				"reference!  Corrupt file system.  Run "
				"chkdsk.");
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
 * @is_system:	pointer in which to return whether the inode is a system one
 *
 * Search all filename attributes in the inode described by the attribute
 * search context @ctx and check if any of the names are in the $Extend system
 * directory.
 *
 * If the inode is a system inode *@is_system is true and if it is not a system
 * inode it is false.
 *
 * Return 0 on success and errno on error.  On error, *@is_system is undefined.
 */
static errno_t ntfs_inode_is_extended_system(ntfs_attr_search_ctx *ctx,
		BOOL *is_system)
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
		return EIO;
	}
	/* Loop through all hard links. */
	while (!(err = ntfs_attr_lookup(AT_FILENAME, AT_UNNAMED, 0, 0, NULL, 0,
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
			return EIO;
		}
		if (a->flags) {
			ntfs_error(vol->mp, "Filename has invalid flags.");
			return EIO;
		}
		if (!(a->resident_flags & RESIDENT_ATTR_IS_INDEXED)) {
			ntfs_error(vol->mp, "Filename is not indexed.");
			return EIO;
		}
		a_end = (u8*)a + le32_to_cpu(a->length);
		fn = (FILENAME_ATTR*)((u8*)a + le16_to_cpu(a->value_offset));
		fn_end = (u8*)fn + le32_to_cpu(a->value_length);
		if ((u8*)fn < (u8*)a || fn_end < (u8*)a || fn_end > a_end ||
				a_end > (u8*)ctx->m + vol->mft_record_size) {
			ntfs_error(vol->mp, "Filename attribute is corrupt.");
			return EIO;
		}
		/* This attribute is ok, but is it in the $Extend directory? */
		if (MREF_LE(fn->parent_directory) == FILE_Extend) {
			ntfs_debug("Done (system).");
			*is_system = TRUE;
			return 0;
		}
	}
	if (err != ENOENT) {
		ntfs_error(vol->mp, "Failed to lookup filename attribute.");
		return err;
	}
	if (nr_links) {
		ntfs_error(vol->mp, "Hard link count does not match number of "
				"filename attributes.");
		return EIO;
	}
	ntfs_debug("Done (not system).");
	*is_system = FALSE;
	return 0;
}

/**
 * ntfs_inode_afpinfo_cache - cache the AfpInfo in the corresponding ntfs inode
 * @ni:		base ntfs inode in which to cache the AfpInfo
 * @afp:	AfpInfo to cache
 * @afp_size:	size in bytes of AfpInfo
 *
 * If @afp is not NULL copy the backup time and the Finder info from the
 * AfpInfo @afp of size @afp_size bytes to the base ntfs inode @ni.
 *
 * If @afp is NULL or the AfpInfo is invalid (wrong signature, version, or
 * size), we ignore the AfpInfo data and set up @ni with defaults for both
 * @ni->backup_time and @ni->finder_info.
 *
 * This function has no return value.
 */
void ntfs_inode_afpinfo_cache(ntfs_inode *ni, AFPINFO *afp,
		const unsigned afp_size)
{
	if (afp && (afp->signature != AfpInfo_Signature ||
			afp->version != AfpInfo_Version ||
			afp_size < sizeof(*afp))) {
		ntfs_warning(ni->vol->mp, "AFP_AfpInfo data attribute of "
				"mft_no 0x%llx contains invalid data (wrong "
				"signature, wrong version, or wrong size), "
				"ignoring and using defaults.",
				(unsigned long long)ni->mft_no);
		afp = NULL;
	}
	if (!NInoValidBackupTime(ni)) {
		if (afp)
			ni->backup_time = ntfs_ad2utc(afp->backup_time);
		else
			ni->backup_time = ntfs_ad2utc(const_cpu_to_sle32(
					INT32_MIN));
		NInoSetValidBackupTime(ni);
	}
	if (!NInoValidFinderInfo(ni)) {
		if (afp)
			memcpy(&ni->finder_info, &afp->finder_info,
					sizeof(ni->finder_info));
		else
			bzero(&ni->finder_info, sizeof(ni->finder_info));
		/*
		 * If the file is hidden we need to mirror this fact to the
		 * Finder hidden bit as SFM does not set the Finder hidden bit
		 * on disk but VNOP_GETATTR() does return it as set so it gets
		 * kept in sync in memory only.
		 *
		 * Just in case we will also set the FILE_ATTR_HIDDEN bit in
		 * the file_attributes if the Finder hidden bit is set but
		 * FILE_ATTR_HIDDEN is not set.  This should never happen but
		 * it does not harm to have the sync go both ways so we do it
		 * especially as that is effectively what HFS and AFP (client)
		 * do, too.
		 */
		if (ni->file_attributes & FILE_ATTR_HIDDEN)
			ni->finder_info.attrs |= FINDER_ATTR_IS_HIDDEN;
		else if (ni->finder_info.attrs & FINDER_ATTR_IS_HIDDEN) {
			ni->file_attributes |= FILE_ATTR_HIDDEN;
			NInoSetDirtyFileAttributes(ni);
		}
		NInoSetValidFinderInfo(ni);
	}
}

/**
 * ntfs_inode_afpinfo_read - load the non-resident AfpInfo and cache it
 * @ni:		base ntfs inode whose AfpInfo to load and cache
 *
 * Load the AfpInfo attribute into memory and copy the backup time and Finder
 * info to the base ntfs inode @ni.
 *
 * Return 0 on success and errno on error.
 *
 * Note if the AfpInfo is invalid (wrong signature, wrong version, or wrong
 * size), we still return success but we do not copy anything thus the caller
 * has to check that NInoValidBackupTime(@ni) and NInoValidFinderInfo(@ni) are
 * true before using @ni->backup_time and @ni->finder_info, respectively.
 *
 * Locking: Caller must hold @ni->lock for writing.
 */
errno_t ntfs_inode_afpinfo_read(ntfs_inode *ni)
{
	ntfs_inode *afp_ni;
	upl_t upl;
	upl_page_info_array_t pl;
	AFPINFO *afp;
	unsigned afp_size;
	errno_t err;

	ntfs_debug("Entering for mft_no 0x%llx.",
			(unsigned long long)ni->mft_no);
	if (NInoValidBackupTime(ni) && NInoValidFinderInfo(ni)) {
		ntfs_debug("Done (both backup time and Finder info are "
				"already valid).");
		return 0;
	}
	/* Get the attribute inode for the AFP_AfpInfo named stream. */
	err = ntfs_attr_inode_get(ni, AT_DATA, NTFS_SFM_AFPINFO_NAME, 11,
			FALSE, LCK_RW_TYPE_SHARED, &afp_ni);
	if (err) {
		ntfs_error(ni->vol->mp, "Failed to get $DATA/AFP_AfpInfo "
				"attribute inode mft_no 0x%llx (error %d).",
				(unsigned long long)ni->mft_no, err);
		return err;
	}
	err = ntfs_page_map(afp_ni, 0, &upl, &pl, (u8**)&afp, FALSE);
	if (err) {
		ntfs_error(ni->vol->mp, "Failed to read AfpInfo from "
				"$DATA/AFP_AfpInfo attribute inode mft_no "
				"0x%llx (error %d).",
				(unsigned long long)ni->mft_no, err);
		goto err;
	}
	lck_spin_lock(&afp_ni->size_lock);
	afp_size = afp_ni->data_size;
	lck_spin_unlock(&afp_ni->size_lock);
	if (afp_size > PAGE_SIZE)
		afp_size = PAGE_SIZE;
	ntfs_inode_afpinfo_cache(ni, afp, afp_size);
	ntfs_page_unmap(afp_ni, upl, pl, FALSE);
	ntfs_debug("Done.");
err:
	lck_rw_unlock_shared(&afp_ni->lock);
	(void)vnode_put(afp_ni->vn);
	return err;
}

/**
 * ntfs_finder_info_is_unused - check if a Finder info is not in use
 * @ni:		ntfs info whose Finder info to check
 *
 * Return true if the Finder info of the ntfs inode @ni is unused and false
 * otherwise.
 *
 * This function takes into account that a set FINDER_ATTR_IS_HIDDEN bit is
 * masked out as the FINDER_ATTR_IS_HIDDEN bit is not stored on disk in the
 * Finder info.
 *
 * Note the Finder info must be valid or this function will cause a panic().
 *
 * Locking: Caller must hold the inode lock (@ni->lock).
 */
static BOOL ntfs_finder_info_is_unused(ntfs_inode *ni)
{
	FINDER_INFO fi;

	if (!NInoValidFinderInfo(ni))
		panic("%s(): !NInoValidFinderInfo(ni)\n", __FUNCTION__);
	memcpy(&fi, &ni->finder_info, sizeof(fi));
	fi.attrs &= ~FINDER_ATTR_IS_HIDDEN;
	return !bcmp(&fi, &ntfs_empty_finder_info, sizeof(fi));
}

/**
 * ntfs_inode_afpinfo_sync - sync the cached AfpInfo
 * @afp:	AfpInfo to sync to
 * @afp_size:	size in bytes of AfpInfo
 * @ni:		base ntfs inode which contains the cache of the AfpInfo
 *
 * Copy @ni->backup_time and @ni->finder_info from the base ntfs inode @ni to
 * the AfpInfo @afp of size @afp_size bytes.
 *
 * This function has no return value.
 */
static void ntfs_inode_afpinfo_sync(AFPINFO *afp, const unsigned afp_size,
		ntfs_inode *ni)
{
	if (NInoTestClearDirtyBackupTime(ni))
		afp->backup_time = ntfs_utc2ad(ni->backup_time);
	if (NInoTestClearDirtyFinderInfo(ni)) {
		if (afp_size < sizeof(ni->finder_info))
			panic("%s(): afp_size < sizeof(ni->finder_info)!\n",
					__FUNCTION__);
		memcpy(&afp->finder_info, &ni->finder_info,
				sizeof(ni->finder_info));
		/*
		 * If the file is hidden we need to clear the Finder hidden bit
		 * on disk as SFM does not set it on disk either as it just
		 * sets the FILE_ATTR_HIDDEN bit in the file_attributes of the
		 * $STANDARD_INFORMATION attribute.  We do this unconditionally
		 * for efficiency.
		 *
		 * Just in case we will also set the FILE_ATTR_HIDDEN bit in
		 * the file_attributes if the Finder hidden bit is set but
		 * FILE_ATTR_HIDDEN is not set.  This should never happen but
		 * it does not harm to have the sync go both ways so we do it
		 * especially as that is effectively what HFS and AFP (client)
		 * do, too.
		 */
		if (ni->finder_info.attrs & FINDER_ATTR_IS_HIDDEN &&
				!(ni->file_attributes & FILE_ATTR_HIDDEN)) {
			ni->file_attributes |= FILE_ATTR_HIDDEN;
			NInoSetDirtyFileAttributes(ni);
		}
		afp->finder_info.attrs &= ~FINDER_ATTR_IS_HIDDEN;
	}
}

/**
 * ntfs_inode_afpinfo_write - update the non-resident AfpInfo on disk
 * @ni:		base ntfs inode whose AfpInfo to update on disk from cache
 *
 * Update the non-resident AfpInfo attribute from the cached backup time and
 * Finder info in the base ntfs inode @ni and write it to disk.
 *
 * If the new backup time and Finder info are the defaults then delete the
 * AfpInfo attribute instead of updating it.
 *
 * Return 0 on success and errno on error.
 *
 * Locking: Caller must hold @ni->lock for writing.
 */
errno_t ntfs_inode_afpinfo_write(ntfs_inode *ni)
{
	ntfs_inode *afp_ni;
	upl_t upl;
	upl_page_info_array_t pl;
	AFPINFO *afp;
	unsigned afp_size;
	sle32 backup_time;
	errno_t err;
	BOOL delete, update;

	backup_time = ntfs_utc2ad(ni->backup_time);
	delete = FALSE;
	if (backup_time == const_cpu_to_sle32(INT32_MIN) &&
			ntfs_finder_info_is_unused(ni))
		delete = TRUE;
	ntfs_debug("Entering for mft_no 0x%llx, delete is %s.",
			(unsigned long long)ni->mft_no,
			delete ? "true" : "false");
	/*
	 * FIXME: If the inode is encrypted we cannot access the AFP_AfpInfo
	 * named stream so no point in trying to do it.  We just pretend to
	 * succeed even though we do not do anything.
	 *
	 * We warn the user about this so they do not get confused.
	 */
	if (NInoEncrypted(ni)) {
		ntfs_warning(ni->vol->mp, "Inode 0x%llx is encrypted thus "
				"cannot write AFP_AfpInfo attribute.  "
				"Pretending the update succeeded to keep the "
				"system happy.",
				(unsigned long long)ni->mft_no);
		err = 0;
		goto err;
	}
	if (!NInoValidBackupTime(ni) || !NInoValidFinderInfo(ni)) {
		/*
		 * Load the AFP_AfpInfo stream and initialize the backup time
		 * and Finder info (if they are not already valid).
		 */
		err = ntfs_inode_afpinfo_read(ni);
		if (err) {
			ntfs_error(ni->vol->mp, "Failed to read AFP_AfpInfo "
					"attribute from inode mft_no 0x%llx "
					"(error %d).",
					(unsigned long long)ni->mft_no, err);
			goto err;
		}
	}
	/*
	 * Get the attribute inode for the AFP_AfpInfo named stream.  If
	 * @delete is false create it if it does not exist and if @delete is
	 * true only get the inode if it exists.
	 */
	err = ntfs_attr_inode_get_or_create(ni, AT_DATA, NTFS_SFM_AFPINFO_NAME,
			11, FALSE, FALSE, delete ? XATTR_REPLACE : 0,
			LCK_RW_TYPE_EXCLUSIVE, &afp_ni);
	if (err) {
		if (err == ENOENT && delete) {
			ntfs_debug("AFP_AfpInfo attribute does not exist in "
					"mft_no 0x%llx, no need to delete it.",
					(unsigned long long)ni->mft_no);
			err = 0;
		} else
			ntfs_error(ni->vol->mp, "Failed to get or create "
					"$DATA/AFP_AfpInfo attribute inode "
					"mft_no 0x%llx (error %d).",
					(unsigned long long)ni->mft_no, err);
		goto err;
	}
	if (delete) {
		ntfs_debug("Unlinking AFP_AfpInfo attribute inode mft_no "
				"0x%llx.", (unsigned long long)ni->mft_no);
		/*
		 * Unlink the attribute inode.  The last close will cause the
		 * VFS to call ntfs_vnop_inactive() which will do the actual
		 * removal.
		 */
		afp_ni->link_count = 0;
		/*
		 * Update the last_mft_change_time (ctime) in the inode as
		 * named stream/extended attribute semantics expect on OS X.
		 */
		ni->last_mft_change_time = ntfs_utc_current_time();
		NInoSetDirtyTimes(ni);
		/*
		 * If this is not a directory or it is an encrypted directory,
		 * set the needs archiving bit except for the core system
		 * files.
		 */
		if (!S_ISDIR(ni->mode) || NInoEncrypted(ni)) {
			BOOL need_set_archive_bit = TRUE;
			if (ni->vol->major_ver >= 2) {
				if (ni->mft_no <= FILE_Extend)
					need_set_archive_bit = FALSE;
			} else {
				if (ni->mft_no <= FILE_UpCase)
					need_set_archive_bit = FALSE;
			}
			if (need_set_archive_bit) {
				ni->file_attributes |= FILE_ATTR_ARCHIVE;
				NInoSetDirtyFileAttributes(ni);
			}
		}
		goto done;
	}
	update = TRUE;
	lck_spin_lock(&afp_ni->size_lock);
	afp_size = afp_ni->data_size;
	lck_spin_unlock(&afp_ni->size_lock);
	if (afp_ni->data_size != sizeof(AFPINFO)) {
		err = ntfs_attr_resize(afp_ni, sizeof(AFPINFO), 0, NULL);
		if (err) {
			ntfs_warning(ni->vol->mp, "Failed to set size of "
					"$DATA/AFP_AfpInfo attribute inode "
					"mft_no 0x%llx (error %d).  Cannot "
					"update AfpInfo.",
					(unsigned long long)ni->mft_no, err);
			goto unl_err;
		}
		ntfs_debug("Set size of $DATA/AFP_AfpInfo attribute inode "
				"mft_no 0x%llx to sizeof(AFPINFO) (%ld) "
				"bytes.", (unsigned long long)ni->mft_no,
				sizeof(AFPINFO));
		lck_spin_lock(&afp_ni->size_lock);
		afp_size = afp_ni->data_size;
		lck_spin_unlock(&afp_ni->size_lock);
		if (afp_size != sizeof(AFPINFO))
			panic("%s(): afp_size != sizeof(AFPINFO)\n",
					__FUNCTION__);
		update = FALSE;
	}
	/*
	 * If we resized the attribute then we do not care for the old contents
	 * so we grab the page instead of mapping it (@update is false in this
	 * case).
	 */
	err = ntfs_page_map_ext(afp_ni, 0, &upl, &pl, (u8**)&afp, update, TRUE);
	if (err) {
		ntfs_error(ni->vol->mp, "Failed to map AfpInfo data of "
				"$DATA/AFP_AfpInfo attribute inode mft_no "
				"0x%llx (error %d).",
				(unsigned long long)ni->mft_no, err);
		goto unl_err;
	}
	if (!update) {
		/*
		 * We need to rewrite the AfpInfo from scratch so for
		 * simplicity start with a clean slate.
		 */
		bzero(afp, PAGE_SIZE);
		afp->signature = AfpInfo_Signature;
		afp->version = AfpInfo_Version;
		afp->backup_time = const_cpu_to_sle32(INT32_MIN);
	}
	ntfs_inode_afpinfo_sync(afp, afp_size, ni);
	ntfs_page_unmap(afp_ni, upl, pl, TRUE);
done:
	lck_rw_unlock_exclusive(&afp_ni->lock);
	(void)vnode_put(afp_ni->vn);
	ntfs_debug("Done.");
	return 0;
unl_err:
	lck_rw_unlock_exclusive(&afp_ni->lock);
	(void)vnode_put(afp_ni->vn);
err:
	NInoClearDirtyBackupTime(ni);
	NInoClearDirtyFinderInfo(ni);
	return err;
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
		err = ENOENT;
		goto err;
	}
	if (m->base_mft_record) {
		ntfs_error(vol->mp, "Inode is an extent inode.");
		err = ENOENT;
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
	if (!ni->link_count) {
		ntfs_error(vol->mp, "Inode had been deleted.");
		err = ENOENT;
		goto err;
	}
	/* Everyone gets all permissions. */
	ni->mode |= ACCESSPERMS;
	/*
	 * FIXME: Reparse points can have the directory bit set even though
	 * they should really be S_IFLNK.  For now we do not support reparse
	 * points so this does not matter.
	 */
	if (m->flags & MFT_RECORD_IS_DIRECTORY) {
		ni->mode |= S_IFDIR;
		/*
		 * Apply the directory permissions mask set in the mount
		 * options.
		 */
		ni->mode &= ~vol->dmask;
	} else {
		/*
		 * We set S_IFREG and apply the permissions mask for files even
		 * though it could be a symbolic link, socket, fifo, or block
		 * or character device special file for example.
		 *
		 * We will update the mode if/when we determine that this inode
		 * is not a regular file.
		 */
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
	err = ntfs_attr_lookup(AT_STANDARD_INFORMATION, AT_UNNAMED, 0, 0, NULL,
			0, ctx);
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
	err = ntfs_attr_lookup(AT_ATTRIBUTE_LIST, AT_UNNAMED, 0, 0, NULL, 0,
			ctx);
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
		ni->attr_list_alloc = (ni->attr_list_size + NTFS_ALLOC_BLOCK -
				1) & ~(NTFS_ALLOC_BLOCK - 1);
		ni->attr_list = IOMallocData(ni->attr_list_alloc);
		if (!ni->attr_list) {
			ni->attr_list_alloc = 0;
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
		/* It is a directory. */
		NInoSetMstProtected(ni);
		ni->type = AT_INDEX_ALLOCATION;
		ni->name = I30;
		ni->name_len = 4;
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
		err = ntfs_attr_lookup(AT_DATA, AT_UNNAMED, 0, 0, NULL, 0, ctx);
		if (err) {
			BOOL is_system;

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
			err = ntfs_inode_is_extended_system(ctx, &is_system);
			if (!err && is_system)
				goto no_data_attr_special_case;
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
				if (NInoCompressed(ni) &&
						a->compression_unit !=
						NTFS_COMPRESSION_UNIT) {
					ntfs_error(vol->mp, "Found "
							"non-standard "
							"compression unit (%d "
							"instead of %d).  "
							"Cannot handle this.",
							a->compression_unit,
							NTFS_COMPRESSION_UNIT);
					err = ENOTSUP;
					goto err;
				}
				if (!NInoCompressed(ni) &&
						a->compression_unit != 0 &&
						a->compression_unit !=
						NTFS_COMPRESSION_UNIT) {
					ntfs_error(vol->mp, "Found "
							"non-standard "
							"compression unit (%d "
							"instead of 0 or %d).  "
							"Cannot handle this.",
							a->compression_unit,
							NTFS_COMPRESSION_UNIT);
					err = ENOTSUP;
					goto err;
				}
				if (a->compression_unit) {
					ni->compression_block_clusters = 1U <<
							a->compression_unit;
					ni->compression_block_size = 1U << (
							a->compression_unit +
							vol->
							cluster_size_shift);
					ni->compression_block_size_shift = ffs(
							ni->
							compression_block_size)
							- 1;
				} else {
					ni->compression_block_clusters = 0;
					ni->compression_block_size = 0;
					ni->compression_block_size_shift = 0;
				}
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
            /*
             * Do some basic sanity checking of the sizes in case of corruption.
             *
             * The on-disk sizes are unsigned, but we store them internally as
             * signed (for convenience when mixing with off_t).  If the most
             * significant bit were set, we'd see a negative number, when it is
             * really a very large positive number.  We assume this is
             * corruption, not a legitimate very large file.
             *
             * We require: 0 <= initialized_size <= data_size <= allocated_size
             */
            if (ni->initialized_size < 0 ||
                    ni->data_size < ni->initialized_size ||
                    ni->allocated_size < ni->data_size) {
                ntfs_error(vol->mp, "Invalid sizes for mft_no 0x%llx "
                           "initialized_size 0x%llx data_size 0x%llx "
                           "allocated_size 0x%llx.", ni->mft_no,
                           ni->initialized_size, ni->data_size,
                           ni->allocated_size);
                err = EIO;
                goto err;
            }
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
			/*
			 * On Services for Unix on Windows, a fifo is a system
			 * file with a zero-length $DATA attribute whilst a
			 * socket is a system file with a $DATA attribute of
			 * length 1.  Block and character device special files
			 * in turn are system files containing an INTX_FILE
			 * structure.
			 */
			if (ni->file_attributes & FILE_ATTR_SYSTEM) {
				INTX_FILE *ix;

				ix = (INTX_FILE*)data;
				if (!ni->data_size) {
					ni->mode &= ~S_IFREG;
					ni->mode |= S_IFIFO;
				} else if (ni->data_size == 1) {
					ni->mode &= ~S_IFREG;
					ni->mode |= S_IFSOCK;
				} else if (data_len == offsetof(INTX_FILE,
						device) + sizeof(ix->device) &&
						(ix->magic ==
						INTX_BLOCK_DEVICE ||
						ix->magic ==
						INTX_CHAR_DEVICE)) {
					ni->mode &= ~S_IFREG;
					if (ix->magic == INTX_BLOCK_DEVICE)
						ni->mode |= S_IFBLK;
					else
						ni->mode |= S_IFCHR;
					ni->rdev = makedev(le64_to_cpu(
							ix->device.major),
							le64_to_cpu(
							ix->device.minor));
				}
			}
		}
	}
no_data_attr_special_case:
	/*
	 * Check if there is an AFP_AfpInfo named stream.
	 *
	 * FIXME: Note we do not bother if the inode is encrypted as we would
	 * not be able to understand its contents anyway.  We need to implement
	 * this once we support encryption.  For now we pretend the AFP_AfpInfo
	 * stream does not exist to make everything smooth going.
	 */
	if (NInoEncrypted(ni)) {
		ntfs_inode_afpinfo_cache(ni, NULL, 0);
		goto done;
	}
	ntfs_attr_search_ctx_reinit(ctx);
	err = ntfs_attr_lookup(AT_DATA, NTFS_SFM_AFPINFO_NAME, 11, 0, NULL, 0,
			ctx);
	if (err) {
		if (err != ENOENT) {
			ntfs_error(vol->mp, "Failed to lookup AfpInfo "
					"attribute (error %d).", err);
			goto err;
		}
		/* The AFP_AfpInfo attribute does not exist. */
		ntfs_inode_afpinfo_cache(ni, NULL, 0);
	} else {
		s64 ai_size;
		ntfs_runlist ai_runlist;
		AFPINFO ai;

		/* The found $DATA/AFP_AfpInfo attribute is now in @ctx->a. */
		a = ctx->a;
		/*
		 * If the attribute is resident (as it usually will be) we have
		 * the data at hand so copy the backup time and Finder info
		 * into the ntfs_inode.
		 */
		if (!a->non_resident) {
			u8 *a_end, *val;
			unsigned val_len;

			a_end = (u8*)a + le32_to_cpu(a->length);
			val = (u8*)a + le16_to_cpu(a->value_offset);
			val_len = le32_to_cpu(a->value_length);
			if (val < (u8*)a || val + val_len > a_end ||
					(u8*)a_end >
					(u8*)ctx->m + vol->mft_record_size ||
					a->flags & ATTR_IS_ENCRYPTED) {
				ntfs_error(vol->mp, "Resident AfpInfo "
						"attribute is corrupt.");
				goto err;
			}
			ntfs_inode_afpinfo_cache(ni, (AFPINFO*)val, val_len);
			goto done;
		}
		ai_size = sle64_to_cpu(a->data_size);
		if (a->lowest_vcn ||
				sle64_to_cpu(a->initialized_size) > ai_size ||
				ai_size > sle64_to_cpu(a->allocated_size)) {
			ntfs_error(vol->mp, "AfpInfo attribute is corrupt.");
			goto err;
		}
		/*
		 * The attribute is non-resident.  If this is a regular file
		 * inode and its data size is less than or equal to
		 * MAXPATHLEN it could actually be a symbolic link.  In this
		 * case we need to read the AFP_AfpInfo attribute now.
		 * Otherwise postpone it till later when it is actually needed.
		 *
		 * We read it in by hand as it will likely not be modified so
		 * no point in wasting system resources by instantiating an
		 * attribute inode for it.  Also we do not have a vnode for the
		 * base inode yet thus cannot obtain an attribute inode at this
		 * point in time even if we wanted to.
		 */
		if (!S_ISREG(ni->mode) || ni->data_size > MAXPATHLEN)
			goto done;
		/*
		 * We only need the AFPINFO structure so ignore any further
		 * data there may be.
		 */
		if (ai_size > (s64)sizeof(AFPINFO))
			ai_size = sizeof(AFPINFO);
		/*
		 * If the attribute is compressed (which it should never be as
		 * Windows only compresses the unnamed $DATA attribute) we
		 * cannot read it here so bail out.
		 */
		if (a->flags & (ATTR_COMPRESSION_MASK | ATTR_IS_ENCRYPTED)) {
			if (a->flags & ATTR_COMPRESSION_MASK)
				ntfs_warning(vol->mp, "AfpInfo is compressed, "
						"ignoring it.  %s",
						ntfs_please_email);
			ntfs_inode_afpinfo_cache(ni, NULL, 0);
			goto done;
		}
		/*
		 * Setup the runlist.  No need for locking as we have exclusive
		 * access to the inode at this time.
		 */
		ai_runlist.rl = NULL;
		ai_runlist.alloc = ai_runlist.elements = 0;
		err = ntfs_mapping_pairs_decompress(vol, a, &ai_runlist);
		if (err) {
			ntfs_error(vol->mp, "Mapping pairs decompression "
					"failed for AfpInfo (error %d).", err);
			goto err;
		}
		/* Now load the attribute data. */
		err = ntfs_rl_read(vol, &ai_runlist, (u8*)&ai, ai_size,
				sle64_to_cpu(a->initialized_size));
		if (err) {
			ntfs_error(vol->mp, "Failed to load AfpInfo (error "
					"%d).", err);
			IOFreeData(ai_runlist.rl, ai_runlist.alloc);
			goto err;
		}
		/* We do not need the runlist any more so free it. */
		IOFreeData(ai_runlist.rl, ai_runlist.alloc);
		/* Finally cache the AFP_AfpInfo data in the base inode. */
		ntfs_inode_afpinfo_cache(ni, &ai, ai_size);
	}
done:
	/*
	 * If it is a regular file and the data size is less than or equal to
	 * MAXPATHLEN it could be a symbolic link so check for this case here.
	 */
	if (S_ISREG(ni->mode) && ni->data_size <= MAXPATHLEN) {
		if (!NInoValidFinderInfo(ni))
			panic("%s(): !NInoValidFinderInfo(ni)\n",
					__FUNCTION__);
		if (ni->finder_info.type == FINDER_TYPE_SYMBOLIC_LINK &&
				ni->finder_info.creator ==
				FINDER_CREATOR_SYMBOLIC_LINK) {
			/*
			 * FIXME: At present the kernel does not allow VLNK
			 * vnodes to use the UBC (<rdar://problem/5794900>)
			 * thus we need to use a shadow VREG vnode to do the
			 * actual read of the symbolic link data.  Fortunately
			 * we already implemented this functionality for
			 * compressed files where we need to read the
			 * compressed data using a shadow vnode so we use the
			 * same implementation here, thus our shadow vnode is a
			 * raw inode.
			 *
			 * Doing this has the unfortunate consequence that if
			 * the symbolic link inode is compressed or encrypted
			 * we cannot read it as we are already using the raw
			 * inode and we can only have one raw inode.  Thus if
			 * the inode is non-resident and compressed or
			 * encrypted we do not change the mode to S_IFLNK thus
			 * causing the symbolic link to appear as a regular
			 * file instead of a symbolic link.
			 */
			if (NInoNonResident(ni) && (NInoCompressed(ni) ||
					NInoEncrypted(ni)))
				ntfs_warning(vol->mp, "Treating %s symbolic "
						"link mft_no 0x%llx as a "
						"regular file due to "
						"<rdar://problem/5794900>.",
						NInoCompressed(ni) ?
						"compressed" : "encrypted",
						(unsigned long long)
						ni->mft_no);
			else {
				/*
				 * Change the mode to indicate this is a
				 * symbolic link and not a regular file.
				 *
				 * Also, symbolic links always grant all
				 * permissions as the real permissions checking
				 * is done after the symbolic link is resolved.
				 */
				ni->mode = S_IFLNK | ACCESSPERMS;
			}
		}
	}
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
 * ntfs_attr_inode_read_or_create - read an attribute inode from its base inode
 * @base_ni:	base inode if @ni is not raw and non-raw inode of @ni otherwise
 * @ni:		attribute inode to read
 * @options:	options specifying the read and/or create behaviour
 *
 * ntfs_attr_inode_read_or_create() is called from
 * ntfs_attr_inode_get_or_create() to read the attribute inode described by @ni
 * into memory from the base mft record described by @base_ni possibly creating
 * the attribute first.
 *
 * If @ni is a raw inode @base_ni is the non-raw inode to which @ni belongs
 * rather than the base inode.
 *
 * If @options does not specify XATTR_CREATE nor XATTR_REPLACE the attribute
 * will be created if it does not exist already and then will be opened.
 *
 * If @options specifies XATTR_CREATE the call will fail if the attribute
 * already exists, i.e. the existing attribute will not be opened.
 *
 * If @options specifies XATTR_REPLACE the call will fail if the attribute does
 * not exist, i.e. the new attribute will not be created, i.e. this is the
 * equivalent of ntfs_attr_inode_get().
 *
 * A special case is the resource fork (@name == NTFS_SFM_RESOURCEFORK_NAME).
 * If it exists but has zero size it is treated as if it does not exist when
 * handling the XATTR_CREATE and XATTR_REPLACE flags in @options.  Thus if the
 * resource fork exists but is zero size, a call with XATTR_CREATE set in
 * @options will succeed as if it did not already exist and a call with
 * XATTR_REPLACE set in @options will fail as if it did not already exist.
 *
 * ntfs_attr_inode_read_or_create() maps, pins and locks the base mft record
 * and looks up the attribute described by @ni before setting up the ntfs
 * inode.  If it is not found and creation is desired, a new attribute is
 * inserted into the mft record.
 *
 * Return 0 on success and errno on error.
 *
 * Note ntfs_attr_inode_read_or_create() cannot be called for
 * AT_INDEX_ALLOCATION, call ntfs_index_inode_read() instead.
 */
static errno_t ntfs_attr_inode_read_or_create(ntfs_inode *base_ni,
		ntfs_inode *ni, const int options)
{
	ntfs_volume *vol = ni->vol;
	MFT_RECORD *m;
	ntfs_attr_search_ctx *ctx;
	ATTR_RECORD *a;
	errno_t err;

	ntfs_debug("Entering for mft_no 0x%llx, attribute type 0x%x, "
			"attribute name length 0x%x.",
			(unsigned long long)ni->mft_no,
			(unsigned)le32_to_cpu(ni->type),
			(unsigned)ni->name_len);
	if (!NInoAttr(ni))
		panic("%s(): !NInoAttr(ni)\n", __FUNCTION__);
	/* Mirror the values from the base inode. */
	ni->seq_no = base_ni->seq_no;
	ni->uid	= base_ni->uid;
	ni->gid	= base_ni->gid;
	/* Attributes cannot be hard-linked so link count is always 1. */
	ni->link_count = 1;
	/* Set inode type to zero but preserve permissions. */
	ni->mode = base_ni->mode & ~S_IFMT;
	/*
	 * If this is our special case of loading the secondary inode for
	 * accessing the raw data of compressed files or symbolic links, we can
	 * simply copy the relevant fields from the base inode rather than
	 * mapping the mft record and looking up the data attribute again.
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
		lck_spin_lock(&base_ni->size_lock);
		if (NInoCompressed(base_ni) || NInoSparse(base_ni)) {
			ni->compression_block_clusters =
					base_ni->compression_block_clusters;
			ni->compression_block_size =
					base_ni->compression_block_size;
			ni->compression_block_size_shift =
					base_ni->compression_block_size_shift;
			ni->compressed_size = base_ni->compressed_size;
		}
		/*
		 * For symbolic links we need the real sizes.  For compressed
		 * and encrypted files we need all values to be the same and
		 * equal to the allocated size so we can access the entirety of
		 * the compressed/encrypted data.
		 *
		 * FIXME: The symbolic link case is done this way because we
		 * cannot use the UBC for VLNK vnodes so we use a raw inode
		 * which has a VREG vnode to do the actual disk i/o (see
		 * <rdar://problem/5794900>).
		 */
		if (S_ISLNK(base_ni->mode)) {
			ni->allocated_size = base_ni->allocated_size;
			ni->data_size = base_ni->data_size;
			ni->initialized_size = base_ni->initialized_size;
		} else {
			ni->initialized_size = ni->data_size =
					ni->allocated_size =
					base_ni->allocated_size;
		}
		lck_spin_unlock(&base_ni->size_lock);
		if (NInoAttr(base_ni)) {
			/* Set @base_ni to point to the real base inode. */
			if (base_ni->nr_extents != -1)
				panic("%s(): Called for non-raw attribute "
						"inode which does not have a "
						"base inode.", __FUNCTION__);
			base_ni = base_ni->base_ni;
		}
		goto done;
	}
	/*
	 * We are looking for a real attribute.
	 *
	 * Map the mft record for the base inode.
	 */
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
	/* Find the attribute. */
	err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len, 0, NULL, 0,
			ctx);
	a = ctx->a;
	if (err) {
		if (err != ENOENT) {
			ntfs_error(vol->mp, "Failed to lookup attribute "
					"(error %d).", err);
			goto err;
		}
		/*
		 * The attribute does not exist.  If @options specifies
		 * XATTR_REPLACE do not allow it to be created.
		 */
		if (options & XATTR_REPLACE) {
			ntfs_debug("Attribute in mft_no 0x%llx does not "
					"exist, returning ENOENT.",
					(unsigned long long)ni->mft_no);
			err = ENOENT;
			goto err;
		}
		ntfs_debug("Attribute does not exist, creating it.");
		/*
		 * FIXME: Cannot create attribute if it has to be non-resident.
		 * With present code this will never happen so no point in
		 * coding it until it is needed.
		 */
		if (ntfs_attr_can_be_resident(vol, ni->type)) {
			ntfs_warning(vol->mp, "Attribute type 0x%x cannot be "
					"resident.  Cannot create "
					"non-resident attributes yet.",
					le32_to_cpu(ni->type));
			err = ENOTSUP;
			goto err;
		}
		/*
		 * Create a new resident attribute.  @a now points to the
		 * location in the mft record at which we need to insert the
		 * attribute so insert it now.
		 */
		err = ntfs_resident_attr_record_insert(base_ni, ctx, ni->type,
				ni->name, ni->name_len, NULL, 0);
		if (err || ctx->is_error) {
			if (!err)
				err = ctx->error;
			ntfs_error(vol->mp, "Failed to %s mft_no 0x%llx "
					"(error %d).", ctx->is_error ?
					"remap extent mft record of" :
					"add resident attribute to",
					(unsigned long long)ni->mft_no, err);
			goto err;
		}
		a = ctx->a;
		ni->allocated_size = le32_to_cpu(a->length) -
				le16_to_cpu(a->value_offset);
		ni->initialized_size = ni->data_size =
				le32_to_cpu(a->value_length);
		/*
		 * Ensure the mft record containing the new attribute gets
		 * written out.
		 */
		NInoSetMrecNeedsDirtying(ctx->ni);
		/*
		 * Update the last_mft_change_time (ctime) in the inode as
		 * named stream/extended attribute semantics expect on OS X.
		 */
		base_ni->last_mft_change_time = ntfs_utc_current_time();
		NInoSetDirtyTimes(base_ni);
		/*
		 * If this is not a directory or it is an encrypted directory,
		 * set the needs archiving bit except for the core system
		 * files.
		 */
		if (!S_ISDIR(base_ni->mode) || NInoEncrypted(base_ni)) {
			BOOL need_set_archive_bit = TRUE;
			if (vol->major_ver >= 2) {
				if (base_ni->mft_no <= FILE_Extend)
					need_set_archive_bit = FALSE;
			} else {
				if (base_ni->mft_no <= FILE_UpCase)
					need_set_archive_bit = FALSE;
			}
			if (need_set_archive_bit) {
				base_ni->file_attributes |= FILE_ATTR_ARCHIVE;
				NInoSetDirtyFileAttributes(base_ni);
			}
		}
		goto put_done;
	}
	/*
	 * The attribute already exists.
	 *
	 * If it is the empty resource fork we need to fail if @options
	 * specifies XATTR_REPLACE.
	 *
	 * If @options specifies XATTR_CREATE we need to abort unless this is
	 * the resource fork and it is empty.
	 */
	if (ni->name == NTFS_SFM_RESOURCEFORK_NAME && !a->value_length) {
		if (options & XATTR_REPLACE) {
			ntfs_debug("Attribute mft_no 0x%llx does not exist, "
					"returning ENOENT.",
					(unsigned long long)ni->mft_no);
			err = ENOENT;
			goto err;
		}
	} else if (options & XATTR_CREATE) {
		ntfs_debug("Attribute mft_no 0x%llx already exists, returning "
				"EEXIST.", (unsigned long long)ni->mft_no);
		err = EEXIST;
		goto err;
	}
	if (a->flags & (ATTR_COMPRESSION_MASK | ATTR_IS_SPARSE)) {
		if (a->flags & ATTR_COMPRESSION_MASK) {
			NInoSetCompressed(ni);
			if (ni->type != AT_DATA) {
				ntfs_error(vol->mp, "Found compressed "
						"non-data attribute.  Please "
						"report you saw this message "
						"to %s.", ntfs_dev_email);
				goto err;
			}
			if (!NVolCompressionEnabled(vol)) {
				ntfs_error(vol->mp, "Found compressed data "
						"but compression is disabled "
						"on this volume and/or "
						"mount.");
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
			ntfs_error(vol->mp, "Found mst protected attribute "
					"but the attribute is %s.  Please "
					"report you saw this message to %s.",
					NInoCompressed(ni) ?
					"compressed" : "sparse",
					ntfs_dev_email);
			goto err;
		}
	}
	if (a->flags & ATTR_IS_ENCRYPTED) {
		if (ni->type != AT_DATA) {
			ntfs_error(vol->mp, "Found encrypted non-data "
					"attribute.  Please report you saw "
					"this message to %s.", ntfs_dev_email);
			goto err;
		}
		if (NInoMstProtected(ni)) {
			ntfs_error(vol->mp, "Found mst protected attribute "
					"but the attribute is encrypted.  "
					"Please report you saw this message "
					"to %s.", ntfs_dev_email);
			goto err;
		}
		if (NInoCompressed(ni)) {
			ntfs_error(vol->mp, "Found encrypted and compressed "
					"data.");
			goto err;
		}
		NInoSetEncrypted(ni);
	}
	if (!a->non_resident) {
		u8 *a_end, *val;
		u32 val_len;

		/* Ensure the attribute name is placed before the value. */
		if (a->name_length && (le16_to_cpu(a->name_offset) >=
				le16_to_cpu(a->value_offset))) {
			ntfs_error(vol->mp, "Attribute name is placed after "
					"the attribute value.");
			goto err;
		}
		if (NInoMstProtected(ni)) {
			ntfs_error(vol->mp, "Found mst protected attribute "
					"but the attribute is resident.  "
					"Please report you saw this message "
					"to %s.", ntfs_dev_email);
			goto err;
		}
		a_end = (u8*)a + le32_to_cpu(a->length);
		val = (u8*)a + le16_to_cpu(a->value_offset);
		val_len = le32_to_cpu(a->value_length);
		if (val < (u8*)a || val + val_len > a_end || (u8*)a_end >
				(u8*)ctx->m + vol->mft_record_size) {
			ntfs_error(vol->mp, "Resident attribute is corrupt.");
			goto err;
		}
		ni->allocated_size = a_end - val;
		ni->data_size = ni->initialized_size = val_len;
	} else {
		NInoSetNonResident(ni);
		/*
		 * Ensure the attribute name is placed before the mapping pairs
		 * array.
		 */
		if (a->name_length && (le16_to_cpu(a->name_offset) >=
				le16_to_cpu(a->mapping_pairs_offset))) {
			ntfs_error(vol->mp, "Attribute name is placed after "
					"the mapping pairs array.");
			goto err;
		}
		if (NInoCompressed(ni) || NInoSparse(ni)) {
			if (NInoCompressed(ni) && a->compression_unit !=
					NTFS_COMPRESSION_UNIT) {
				ntfs_error(vol->mp, "Found non-standard "
						"compression unit (%d instead "
						"of %d).  Cannot handle this.",
						a->compression_unit,
						NTFS_COMPRESSION_UNIT);
				err = ENOTSUP;
				goto err;
			}
			if (!NInoCompressed(ni) && a->compression_unit != 0 &&
					a->compression_unit !=
					NTFS_COMPRESSION_UNIT) {
				ntfs_error(vol->mp, "Found non-standard "
						"compression unit (%d instead "
						"of 0 or %d).  Cannot handle "
						"this.", a->compression_unit,
						NTFS_COMPRESSION_UNIT);
				err = ENOTSUP;
				goto err;
			}
			if (a->compression_unit) {
				ni->compression_block_clusters = 1U <<
						a->compression_unit;
				ni->compression_block_size = 1U << (
						a->compression_unit +
						vol->cluster_size_shift);
				ni->compression_block_size_shift = ffs(
						ni->compression_block_size) - 1;
			} else {
				ni->compression_block_clusters = 0;
				ni->compression_block_size = 0;
				ni->compression_block_size_shift = 0;
			}
			ni->compressed_size = sle64_to_cpu(a->compressed_size);
		}
		if (a->lowest_vcn) {
			ntfs_error(vol->mp, "First extent of attribute has "
					"non-zero lowest_vcn.");
			goto err;
		}
		ni->allocated_size = sle64_to_cpu(a->allocated_size);
		ni->data_size = sle64_to_cpu(a->data_size);
		ni->initialized_size = sle64_to_cpu(a->initialized_size);
	}
put_done:
	ntfs_attr_search_ctx_put(ctx);
	ntfs_mft_record_unmap(base_ni);
done:
	/*
	 * Attach the base inode to the attribute inode and vice versa.  Note
	 * we do not need to lock the new inode as we still have exclusive
	 * access to it.
	 */
	lck_mtx_lock(&base_ni->attr_nis_lock);
	if (NInoDeleted(base_ni)) {
		lck_mtx_unlock(&base_ni->attr_nis_lock);
		return EDEADLK;
	}
	if ((base_ni->nr_attr_nis + 1) * sizeof(ntfs_inode *) >
			base_ni->attr_nis_alloc) {
		ntfs_inode **tmp;
		int new_size;

		new_size = base_ni->attr_nis_alloc + 4 * sizeof(ntfs_inode *);
		tmp = IOMallocZero(new_size);
		if (!tmp) {
			ntfs_error(vol->mp, "Failed to allocated internal "
					"buffer.");
			lck_mtx_unlock(&base_ni->attr_nis_lock);
			return ENOMEM;
		}
		if (base_ni->attr_nis_alloc) {
			if (base_ni->nr_attr_nis > 0)
				memcpy(tmp, base_ni->attr_nis,
						base_ni->nr_attr_nis *
						sizeof(ntfs_inode *));
			IOFree(base_ni->attr_nis, base_ni->attr_nis_alloc);
		}
		base_ni->attr_nis_alloc = new_size;
		base_ni->attr_nis = tmp;
	}
	base_ni->attr_nis[base_ni->nr_attr_nis++] = ni;
	ni->nr_extents = -1;
	ni->base_ni = base_ni;
	ni->base_attr_nis_lock = &base_ni->attr_nis_lock;
	lck_mtx_unlock(&base_ni->attr_nis_lock);
	ntfs_debug("Done.");
	return 0;
err:
	if (ctx)
		ntfs_attr_search_ctx_put(ctx);
	if (m)
		ntfs_mft_record_unmap(base_ni);
	if (!err)
		err = EIO;
	if (err != ENOENT) {
		ntfs_error(vol->mp, "Failed (error %d) for attribute inode "
				"0x%llx, attribute type 0x%x, name_len 0x%x.  "
				"Run chkdsk.", (int)err,
				(unsigned long long)ni->mft_no,
				(unsigned)le32_to_cpu(ni->type),
				(unsigned)ni->name_len);
		if (err != ENOTSUP && err != ENOMEM)
			NVolSetErrors(vol);
	}
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
	errno_t err;
	BOOL is_dir_index = (S_ISDIR(base_ni->mode) && ni->name == I30);
	
	ntfs_debug("Entering for mft_no 0x%llx, index name length 0x%x.",
			(unsigned long long)ni->mft_no,
			(unsigned)ni->name_len);
	/* Mirror the values from the base inode. */
	ni->seq_no = base_ni->seq_no;
	ni->uid	= base_ni->uid;
	ni->gid	= base_ni->gid;
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
	err = ntfs_attr_lookup(AT_INDEX_ROOT, ni->name, ni->name_len, 0, NULL,
			0, ctx);
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
	err = ntfs_attr_lookup(AT_INDEX_ALLOCATION, ni->name, ni->name_len, 0,
			NULL, 0, ctx);
	if (err) {
		if (err != ENOENT) {
			ntfs_error(vol->mp, "Failed to lookup index "
					"allocation attribute.");
			goto err;
		}
		if (ir->index.flags & LARGE_INDEX) {
			ntfs_error(vol->mp, "Index allocation attribute is "
					"not present but the index root "
					"attribute indicated it is.");
			goto err;
		}
		/* No index allocation. */
		ni->allocated_size = ni->data_size = ni->initialized_size = 0;
		/* We are done with the mft record, so we release it. */
		ntfs_attr_search_ctx_put(ctx);
		ntfs_mft_record_unmap(base_ni);
	} else {
		unsigned block_mask;

		/* Index allocation present.  Setup state. */
		NInoSetIndexAllocPresent(ni);
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
		err = ntfs_attr_inode_get(base_ni, AT_BITMAP, ni->name,
				ni->name_len, FALSE, LCK_RW_TYPE_SHARED, &bni);
		if (err) {
			ntfs_error(vol->mp, "Failed to get bitmap attribute.");
			goto err;
		}
		if (NInoCompressed(bni) || NInoEncrypted(bni) ||
				NInoSparse(bni)) {
			ntfs_error(vol->mp, "Bitmap attribute is compressed "
					"and/or encrypted and/or sparse.");
			lck_rw_unlock_shared(&bni->lock);
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
			lck_rw_unlock_shared(&bni->lock);
			(void)vnode_put(bni->vn);
			goto err;
		}
		lck_rw_unlock_shared(&bni->lock);
		(void)vnode_put(bni->vn);
	}
	/*
	 * Attach the base inode to the attribute inode and vice versa.  Note
	 * we do not need to lock the new inode as we still have exclusive
	 * access to it.
	 */
	lck_mtx_lock(&base_ni->attr_nis_lock);
	if (NInoDeleted(base_ni)) {
		lck_mtx_unlock(&base_ni->attr_nis_lock);
		return EDEADLK;
	}
	if ((base_ni->nr_attr_nis + 1) * sizeof(ntfs_inode *) >
			base_ni->attr_nis_alloc) {
		ntfs_inode **tmp;
		int new_size;

		new_size = base_ni->attr_nis_alloc + 4 * sizeof(ntfs_inode *);
		tmp = IOMallocZero(new_size);
		if (!tmp) {
			ntfs_error(vol->mp, "Failed to allocated internal "
					"buffer.");
			lck_mtx_unlock(&base_ni->attr_nis_lock);
			return ENOMEM;
		}
		if (base_ni->attr_nis_alloc) {
			if (base_ni->nr_attr_nis > 0)
				memcpy(tmp, base_ni->attr_nis,
						base_ni->nr_attr_nis *
						sizeof(ntfs_inode *));
			IOFree(base_ni->attr_nis, base_ni->attr_nis_alloc);
		}
		base_ni->attr_nis_alloc = new_size;
		base_ni->attr_nis = tmp;
	}
	base_ni->attr_nis[base_ni->nr_attr_nis++] = ni;
	ni->nr_extents = -1;
	ni->base_ni = base_ni;
	ni->base_attr_nis_lock = &base_ni->attr_nis_lock;
	lck_mtx_unlock(&base_ni->attr_nis_lock);
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
	ntfs_volume *vol = ni->vol;
	BOOL do_release;

	/* No need to lock at this stage as no one else has a reference. */
	if (ni->nr_extents > 0) {
		int i;
		
		for (i = 0; i < ni->nr_extents; i++)
			ntfs_inode_reclaim(ni->extent_nis[i]);
		IOFree(ni->extent_nis, ni->extent_alloc);
	}
	/*
	 * If this is an attribute or index inode, detach it from the base
	 * inode if it is attached.
	 */
	if (NInoAttr(ni) && ni->nr_extents == -1) {
		ntfs_inode *base_ni, **attr_nis;
		int i;

		/* Lock the base inode. */
		lck_mtx_lock(ni->base_attr_nis_lock);
		base_ni = ni->base_ni;
		/* Find the current inode in the base inode array. */
		attr_nis = base_ni->attr_nis;
		for (i = 0; i < base_ni->nr_attr_nis; i++) {
			if (attr_nis[i] != ni)
				continue;
			/*
			 * Delete the inode from the array and move any
			 * following entries forward over the current entry.
			 */
			if (i + 1 < base_ni->nr_attr_nis)
				memmove(attr_nis + i, attr_nis + i + 1,
						(base_ni->nr_attr_nis -
						(i + 1)) *
						sizeof(ntfs_inode *));
			base_ni->nr_attr_nis--;
			break;
		}
		ni->nr_extents = 0;
		ni->base_ni = NULL;
		lck_mtx_unlock(ni->base_attr_nis_lock);
		ni->base_attr_nis_lock = NULL;
	}
	if (ni->rl.alloc)
		IOFreeData(ni->rl.rl, ni->rl.alloc);
	if (ni->attr_list_alloc)
		IOFreeData(ni->attr_list, ni->attr_list_alloc);
	if (ni->attr_list_rl.alloc)
		IOFreeData(ni->attr_list_rl.rl, ni->attr_list_rl.alloc);
	ntfs_dirhints_put(ni, 0);
	if (ni->name_len && ni->name != I30 &&
			ni->name != NTFS_SFM_RESOURCEFORK_NAME &&
			ni->name != NTFS_SFM_AFPINFO_NAME)
		IOFreeData(ni->name, (ni->name_len + 1) * sizeof(ntfschar));
	/* Remove the inode from the list of inodes in the volume. */
	lck_mtx_lock(&vol->inodes_lock);
	LIST_REMOVE(ni, inodes);
	/*
	 * If this was the last inode and the release of the volume was
	 * postponed then release the volume now.
	 */
	do_release = FALSE;
	if (LIST_EMPTY(&vol->inodes) && NVolPostponedRelease(vol)) {
		NVolClearPostponedRelease(vol);
		do_release = TRUE;
	}
	lck_mtx_unlock(&vol->inodes_lock);
	/* Destroy all the locks before finally discarding the ntfs inode. */
	lck_rw_destroy(&ni->lock, ntfs_lock_grp);
	lck_spin_destroy(&ni->size_lock, ntfs_lock_grp);
	ntfs_rl_deinit(&ni->rl);
	ntfs_rl_deinit(&ni->attr_list_rl);
	lck_mtx_destroy(&ni->extent_lock, ntfs_lock_grp);
	IOFreeType(ni, ntfs_inode);
	/* If the volume release was postponed, perform it now. */
	if (do_release)
		ntfs_do_postponed_release(vol);
}

/**
 * ntfs_inode_reclaim - destroy an ntfs inode freeing all its resources
 * @ni:		ntfs inode to destroy
 *
 * Destroy the ntfs inode @ni freeing all its resources in the process.  We are
 * assured that no-one can get the inode because to do that they would have to
 * take a reference on the corresponding vnode and that is not possible because
 * the vnode is flagged for termination thus the vnode_get() will return an
 * error.
 *
 * Note: When called from reclaim, the vnode of the ntfs inode has a zero
 *	 v_iocount and v_usecount and vnode_isrecycled() is true.
 *
 * This function cannot fail and always returns 0.
 */
errno_t ntfs_inode_reclaim(ntfs_inode *ni)
{
	vnode_t vn;

	/* If @ni is NULL, do not do anything. */
	if (!ni)
		return 0;
	ntfs_debug("Entering for mft_no 0x%llx.",
			(unsigned long long)ni->mft_no);
	/*
	 * If this is a base inode and there are attribute/index inodes loaded,
	 * then recycle them now.
	 *
	 * FIXME: For a forced unmount where something is genuinely kept busy
	 * this will cause the system to hang but this is the only way to avoid
	 * a crash in NTFS...
	 */
	if (!NInoAttr(ni)) {
		int count = 0;

		lck_mtx_lock(&ni->attr_nis_lock);
		while (ni->nr_attr_nis > 0) {
			ntfs_inode *attr_ni;
			int err;

			attr_ni = ni->attr_nis[ni->nr_attr_nis - 1];
			err = 1;
			if (!NInoDeleted(attr_ni))
				err = vnode_get(attr_ni->vn);
			lck_mtx_unlock(&ni->attr_nis_lock);
			if (!err) {
				vnode_recycle(attr_ni->vn);
				vnode_put(attr_ni->vn);
			}
			/* Give it a chance to go away... */
			(void)thread_block(THREAD_CONTINUE_NULL);
			if (count < 1000)
				count++;
			else if (count == 1000) {
				ntfs_warning(ni->vol->mp, "Failed to reclaim "
						"inode 0x%llx because it has "
						"a busy attribute/index "
						"inode.  Going to keep "
						"trying for ever...",
						(unsigned long long)
						ni->mft_no);
				count = 1001;
			}
			lck_mtx_lock(&ni->attr_nis_lock);
		}
		lck_mtx_unlock(&ni->attr_nis_lock);
	}
	lck_mtx_lock(&ntfs_inode_hash_lock);
	NInoSetReclaim(ni);
	/*
	 * If the inode has been deleted then it has been removed from the ntfs
	 * inode hash already.
	 */
	if (!NInoDeleted(ni)) {
		NInoSetDeleted(ni);
		ntfs_inode_hash_rm_nolock(ni);
	}
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
	if (ni->nr_refs > 0)
		ntfs_debug("Called for mft_no 0x%llx, attribute type 0x%x, "
				"nr_refs %d.", (unsigned long long)ni->mft_no,
				(unsigned)le32_to_cpu(ni->type), ni->nr_refs);
	ntfs_inode_free(ni);
	ntfs_debug("Done.");
	return 0;
}

/**
 * ntfs_inode_data_sync - synchronize an inode's in-core data
 * @ni:		ntfs inode the data of which to synchronize to disk
 * @ioflags:	flags describing the i/o request
 *
 * Sync all dirty cached data belonging/related to the ntfs inode @ni.
 *
 * If @ioflags has the IO_SYNC bit set, wait for all i/o to complete before
 * returning.
 *
 * If @ioflags has the IO_CLOSE bit set, this signals cluster_push() that the
 * i/o is issued from the close path (or in our case more precisely from the
 * VNOP_INACTIVE() path).
 *
 * Note: When called from reclaim (via VNOP_FSYNC() and hence ntfs_vnop_fsync()
 *	 and ntfs_inode_sync(), the vnode has a zero v_iocount and v_usecount
 *	 and vnode_isrecycled() is true.  Thus we cannot obtain any
 *	 attribute/raw inodes inside ntfs_inode_data_sync() or the vnode_ref()
 *	 on the base vnode that is done as part of getting an attribute/raw
 *	 inode causes a panic() to trigger as both the iocount and usecount are
 *	 zero on the base vnode.
 *
 * Return 0 on success and the error code on error.
 */
static errno_t ntfs_inode_data_sync(ntfs_inode *ni, const int ioflags)
{
	ntfs_volume *vol = ni->vol;
	vnode_t vn = ni->vn;
	errno_t err = 0;

	/*
	 * $MFT/$DATA and $MFTMirr/$DATA are accessed exclusively via
	 * buf_meta_bread(), etc, i.e. they do not use the UBC, thus to write
	 * them we only need to worry about writing out any dirty buffers.
	 */
	lck_rw_lock_shared(&ni->lock);
	if (ni == vol->mft_ni || ni == vol->mftmirr_ni) {
		/* Flush all dirty buffers associated with the vnode. */
		ntfs_debug("Calling buf_flushdirtyblks() for $MFT%s/$DATA.",
				ni == vol->mft_ni ? "" : "Mirr");
		buf_flushdirtyblks(vn, ioflags & IO_SYNC, 0 /* lock flags */,
				"ntfs_inode_sync");
		lck_rw_unlock_shared(&ni->lock);
		return 0;
	}
	if (NInoNonResident(ni)) {
		int (*callback)(buf_t, void *) = NULL;

		if (ni->type != AT_INDEX_ALLOCATION) {
			if (NInoCompressed(ni) && !NInoRaw(ni)) {
#if 0
				err = ntfs_inode_sync_compressed(ni, uio,
						ubc_getsize(vn), ioflags);
				if (!err)
					ntfs_debug("Done (ntfs_inode_sync_"
							"compressed()).");
				else
					ntfs_error(vol->mp, "Failed (ntfs_"
							"inode_sync_"
							"compressed(), error "
							"%d).", err);
#endif
				lck_rw_unlock_shared(&ni->lock);
				ntfs_error(vol->mp, "Syncing compressed file "
						"inodes is not implemented "
						"yet, sorry.");
				return ENOTSUP;
			}
			if (NInoEncrypted(ni)) {
#if 0
				callback = ntfs_cluster_iodone;
#endif
				lck_rw_unlock_shared(&ni->lock);
				ntfs_error(vol->mp, "Syncing encrypted file "
						"inodes is not implemented "
						"yet, sorry.");
				return ENOTSUP;
			}
		}
		/*
		 * Write any dirty clusters.  We are guaranteed not to have any
		 * for mst protected attributes.
		 */
		if (!NInoMstProtected(ni)) {
			/* Write out any dirty clusters. */
			ntfs_debug("Calling cluster_push_ext().");
			(void)cluster_push_ext(vn, ioflags, callback, NULL);
		}
		/* Flush all dirty buffers associated with the vnode. */
		ntfs_debug("Calling buf_flushdirtyblks().");
		buf_flushdirtyblks(vn, ioflags & IO_SYNC, 0 /* lock flags */,
				"ntfs_inode_sync");
#ifdef DEBUG
	} else /* if (!NInoNonResident(ni)) */ {
		if (vnode_hasdirtyblks(vn))
			ntfs_warning(vol->mp, "resident and "
					"vnode_hasdirtyblks!");
#endif /* DEBUG */
	}
	/* ubc_msync() cannot be called with the inode lock held. */
	lck_rw_unlock_shared(&ni->lock);
	/*
	 * If we have any dirty pages in the VM page cache, write them out now.
	 * For a resident attribute this will push the data into the mft record
	 * which needs to be pushed to disk later/elsewhere.
	 */
	ntfs_debug("Calling ubc_msync() for inode data.");
	err = ubc_msync(vn, 0, ubc_getsize(vn), NULL, UBC_PUSHDIRTY |
			(ioflags & IO_SYNC ? UBC_SYNC : 0));
	if (err)
		ntfs_error(vol->mp, "ubc_msync() of data for mft_no 0x%llx "
				"failed (error %d).",
				(unsigned long long)ni->mft_no, err);
	return err;
}

struct fn_list_entry {
	SLIST_ENTRY(fn_list_entry) list_entry;
	unsigned alloc, size;
	FILENAME_ATTR fn;
};

/**
 * ntfs_inode_sync_to_mft_record - update metadata with changes to ntfs inode
 * @ni:		ntfs inode the changes of which to update the metadata with
 *
 * Sync all dirty cached data belonging/related to the ntfs inode @ni.
 *
 * Note: When called from reclaim (via VNOP_FSYNC() and hence ntfs_vnop_fsync()
 *	 and ntfs_inode_sync(), the vnode has a zero v_iocount and v_usecount
 *	 and vnode_isrecycled() is true.  Thus we cannot obtain any
 *	 attribute/raw inodes inside ntfs_inode_sync_to_mft_record() or the
 *	 vnode_ref() on the base vnode that is done as part of getting an
 *	 attribute/raw inode causes a panic() to trigger as both the iocount
 *	 and usecount are zero on the base vnode.
 *
 * Return 0 on success and the error code on error.
 */
static errno_t ntfs_inode_sync_to_mft_record(ntfs_inode *ni)
{
	sle64 creation_time, last_data_change_time, last_mft_change_time,
			last_access_time, allocated_size, data_size;
	ino64_t dir_mft_no;
	ntfs_volume *vol = ni->vol;
	MFT_RECORD *m;
	ntfs_attr_search_ctx *actx;
	ATTR_RECORD *a;
	SLIST_HEAD(, fn_list_entry) fn_list;
	struct fn_list_entry *next;
	ntfs_index_context *ictx;
	ntfs_inode *dir_ni, *dir_ia_ni;
	FILENAME_ATTR *fn;
	errno_t err;
	FILE_ATTR_FLAGS file_attributes = 0;
	BOOL ignore_errors, dirty_times, dirty_file_attributes, dirty_sizes;
	BOOL dirty_set_file_bits, modified;
	static const char ies[] = "Failed to update directory index entry(ies) "
			"of inode 0x%llx because %s (error %d).  Run chkdsk "
			"or touch the inode again to retry the update.";

	/*
	 * There is nothing to do for attribute inodes and raw inodes.  Note
	 * raw inodes are always attribute inodes so no need to check for them.
	 *
	 * There is nothing to do for clean inodes.
	 */
	if (NInoAttr(ni) || !NInoDirty(ni))
		return 0;
	lck_rw_lock_shared(&ni->lock);
	err = ntfs_mft_record_map(ni, &m);
	if (err) {
		lck_rw_unlock_shared(&ni->lock);
		ntfs_error(vol->mp, "Failed to map mft record.");
		return err;
	}
	actx = ntfs_attr_search_ctx_get(ni, m);
	if (!actx) {
		ntfs_mft_record_unmap(ni);
		lck_rw_unlock_shared(&ni->lock);
		ntfs_error(vol->mp, "Failed to get attribute search context.");
		return ENOMEM;
	}
	ignore_errors = FALSE;
	dirty_times = NInoTestClearDirtyTimes(ni);
	dirty_file_attributes = NInoTestClearDirtyFileAttributes(ni);
	dirty_sizes = NInoTestClearDirtySizes(ni);
	/* Directories always have their sizes set to zero. */
	if (S_ISDIR(ni->mode))
		dirty_sizes = FALSE;
	dirty_set_file_bits = NInoTestClearDirtySetFileBits(ni);
	/*
	 * Update the access times/file attributes in the standard information
	 * attribute.
	 */
	modified = FALSE;
	creation_time = last_data_change_time = last_mft_change_time =
			last_access_time = 0;
	if (dirty_times || dirty_file_attributes) {
		STANDARD_INFORMATION *si;

		err = ntfs_attr_lookup(AT_STANDARD_INFORMATION, AT_UNNAMED, 0,
				0, NULL, 0, actx);
		if (err)
			goto err;
		si = (STANDARD_INFORMATION*)((u8*)actx->a +
				le16_to_cpu(actx->a->value_offset));
		if (dirty_file_attributes) {
			file_attributes = ni->file_attributes;
			if (si->file_attributes != file_attributes) {
				ntfs_debug("Updating file attributes for "
						"inode 0x%llx: old = 0x%x, "
						"new = 0x%x",
						(unsigned long long)ni->mft_no,
						(unsigned)le32_to_cpu(
						si->file_attributes),
						(unsigned)le32_to_cpu(
						file_attributes));
				si->file_attributes = file_attributes;
				modified = TRUE;
			}
			/*
			 * We have updated the standard information attribute.
			 * Now need to update the file attributes for the
			 * directory entries which also have the
			 * FILE_ATTR_DUP_FILENAME_INDEX_PRESENT flag set on all
			 * directory inodes.
			 */
			if (S_ISDIR(ni->mode))
				file_attributes |=
					FILE_ATTR_DUP_FILENAME_INDEX_PRESENT;
		}
		creation_time = utc2ntfs(ni->creation_time);
		if (si->creation_time != creation_time) {
			ntfs_debug("Updating creation_time for inode 0x%llx: "
					"old = 0x%llx, new = 0x%llx",
					(unsigned long long)ni->mft_no,
					(unsigned long long)
					sle64_to_cpu(si->creation_time),
					(unsigned long long)
					sle64_to_cpu(creation_time));
			si->creation_time = creation_time;
			modified = TRUE;
		}
		last_data_change_time = utc2ntfs(ni->last_data_change_time);
		if (si->last_data_change_time != last_data_change_time) {
			ntfs_debug("Updating last_data_change_time for inode "
					"0x%llx: old = 0x%llx, new = 0x%llx",
					(unsigned long long)ni->mft_no,
					(unsigned long long)
					sle64_to_cpu(si->last_data_change_time),
					(unsigned long long)
					sle64_to_cpu(last_data_change_time));
			si->last_data_change_time = last_data_change_time;
			modified = TRUE;
		}
		last_mft_change_time = utc2ntfs(ni->last_mft_change_time);
		if (si->last_mft_change_time != last_mft_change_time) {
			ntfs_debug("Updating last_mft_change_time for inode "
					"0x%llx: old = 0x%llx, new = 0x%llx",
					(unsigned long long)ni->mft_no,
					(unsigned long long)
					sle64_to_cpu(si->last_mft_change_time),
					(unsigned long long)
					sle64_to_cpu(last_mft_change_time));
			si->last_mft_change_time = last_mft_change_time;
			modified = TRUE;
		}
		last_access_time = utc2ntfs(ni->last_access_time);
		if (si->last_access_time != last_access_time) {
			ntfs_debug("Updating last_access_time for inode "
					"0x%llx: old = 0x%llx, new = 0x%llx",
					(unsigned long long)ni->mft_no,
					(unsigned long long)
					sle64_to_cpu(si->last_access_time),
					(unsigned long long)
					sle64_to_cpu(last_access_time));
			si->last_access_time = last_access_time;
			modified = TRUE;
		}
	}
	/*
	 * If we just modified the standard information attribute we need to
	 * mark the mft record it is in dirty.
	 */
	if (modified)
		NInoSetMrecNeedsDirtying(actx->ni);
	/*
	 * If the special mode bits S_ISUID, S_ISGID, and/or S_ISVTX need to be
	 * updated, do it now..
	 */
	if (dirty_set_file_bits) {
		modified = FALSE;
		// TODO: Lookup $EA_INFORMATION and $EA and if not there create
		// them, then if the SETFILEBITS EA is not present, create it,
		// then if the bits in the EA do not match the new ones update
		// the EA with the new bits.
		if (modified)
			NInoSetMrecNeedsDirtying(actx->ni);
		ntfs_attr_search_ctx_reinit(actx);
	}
	/* We ensure above that this never triggers for directory inodes. */
	if (dirty_sizes) {
		lck_spin_lock(&ni->size_lock);
		allocated_size = cpu_to_sle64(NInoNonResident(ni) &&
				(NInoSparse(ni) || NInoCompressed(ni)) ?
				ni->compressed_size : ni->allocated_size);
		data_size = cpu_to_sle64(ni->data_size);
		lck_spin_unlock(&ni->size_lock);
	} else
		allocated_size = data_size = 0;
	/*
	 * If the directory index entries need updating, do it now.  Note we
	 * use goto to skip this section to reduce indentation.
	 *
	 * Note, there is one special case; unlinked but not yet deleted inodes
	 * (POSIX semantics of being able to access an opened file/directory
	 * after unlinking it until it is closed when it is really deleted).
	 * The special thing here is that ntfs_unlink() has removed all
	 * directory entries pointing to the inode we are writing out but it
	 * has left the last filename attribute in the mft record of the inode
	 * thus we would find a filename attribute for which we would then fail
	 * to lookup the directory entry as it does not exist any more.  Thus
	 * we skip directory index entry updates completely for all unlinked
	 * inodes.  Even if this problem did not exist, it would still make
	 * sense to skip directory index entry updates for unlinked files as
	 * they by definition do not have any directory entries so we are just
	 * waisting cpu cycles trying to find some.
	 *
	 * Note: Any non-serious errors during the update of the index entries
	 * can be ignored because having not up-to-date index entries wrt the
	 * inode times and/or sizes does not actually make anything not work
	 * and even chkdsk /f does not report it as an error and the verbose
	 * chkdsk /f/v only reports it as a "cleanup of a minor inconsistency".
	 * Further, any transient errors get automatically corrected the next
	 * time an update happens as the updates simply overwrite the old
	 * values each time.
	 */
	if ((!dirty_file_attributes && !dirty_times && !dirty_sizes) ||
			!ni->link_count) {
		ntfs_attr_search_ctx_put(actx);
		ntfs_mft_record_unmap(ni);
		lck_rw_unlock_shared(&ni->lock);
		goto done;
	}
	ictx = NULL;
	ignore_errors = TRUE;
	/*
	 * Enumerate all filename attributes.  We do not reset the search
	 * context as we will be enumerating the filename attributes which come
	 * after the standard information attribute.
	 *
	 * Note that whilst from an NTFS point of view it would be perfectly
	 * safe to mix the attribute lookups with the index lookups because
	 * NTFS does not allow hard links to directories thus we are guaranteed
	 * not to be working on the directory inode that we will be using to do
	 * index lookups in, thus no danger of deadlock exists, we cannot
	 * actually do that as explained below.  There is only one special case
	 * we need to deal with where this is not true and this is the root
	 * directory of the volume which contains an entry for itself with the
	 * name ".".
	 *
	 * The reason we cannot mix the attribute lookups with the index
	 * lookups is that the mft record(s) for the directory can be in the
	 * same page as the mft record(s) for the file we are currently working
	 * on and when this happens we deadlock when ntfs_index_lookup() tries
	 * to map the mft record for the directory as we are holding the page
	 * it is in locked already due to the mapped mft record(s) of the file.
	 *
	 * Thus we go over all the filename attributes and copy them one by one
	 * into a temporary buffer, then release the mft record of the file and
	 * only then do the index lookups for each copied filename attribute.
	 * 
	 * This is ugly but still a lot more efficient than having to drop and
	 * re-map the mft record for the file for each filename!  And it does
	 * have one advantage and that is that the root directory "." update
	 * does not need to be treated specially.
	 */
	SLIST_INIT(&fn_list);
	do {
		unsigned size, alloc;

		err = ntfs_attr_lookup(AT_FILENAME, AT_UNNAMED, 0, 0, NULL, 0,
				actx);
		if (err) {
			/*
			 * ENOENT means that there are no more filenames in the
			 * mft record, i.e. we are done.
			 */
			if (err == ENOENT)
				break;
			/* Real error. */
			ntfs_error(vol->mp, ies,
					(unsigned long long)ni->mft_no,
					"looking up a filename attribute in "
					"the inode failed", err);
			ignore_errors = FALSE;
			goto list_err;
		}
		a = actx->a;
		if (a->non_resident) {
			ntfs_error(vol->mp, "Non-resident filename attribute "
					"found.  Run chkdsk.");
			err = EIO;
			ignore_errors = FALSE;
			goto list_err;
		}
		/*
		 * Allocate a new list entry, copy the current filename
		 * attribute value into it, and attach it to the end of the
		 * list.
		 */
		size = le32_to_cpu(a->value_length);
		alloc = offsetof(struct fn_list_entry, fn) + size;
		next = IOMallocData(alloc);
		if (!next) {
			ntfs_error(vol->mp, ies, 
					(unsigned long long)ni->mft_no,
					"there was not enough memory to "
					"allocate a temporary filename buffer",
					ENOMEM);
			err = ENOMEM;
			goto list_err;
		}
		next->alloc = alloc;
		next->size = size;
		memcpy(&next->fn, (u8*)a + le16_to_cpu(a->value_offset), size);
		/*
		 * It makes no difference in what order we process the names so
		 * we just insert them all the the list head thus effectively
		 * processing them in LIFO order.
		 */
		SLIST_INSERT_HEAD(&fn_list, next, list_entry);
	} while (1);
	/* We are done with the mft record so release it. */
	ntfs_attr_search_ctx_put(actx);
	ntfs_mft_record_unmap(ni);
	lck_rw_unlock_shared(&ni->lock);
	actx = NULL;
	m = NULL;
	/*
	 * We have now gathered all the filenames into the @fn_list list and
	 * are ready to start looking up each filename in its parent directory
	 * index and updating the matching directory entry.
	 *
	 * Note that because we currently hold no locks any of the filenames
	 * we gathered can be unlinked() before we try to update them.  And
	 * they can even be re-created with a different target mft record or
	 * even with the same one but with an incremented sequence number.  We
	 * need to take this into consideration when handling errors below.
	 *
	 * Start by allocating an index context for doing the index lookups.
	 */
	ictx = ntfs_index_ctx_alloc();
	if (!ictx) {
		ntfs_debug(ies, (unsigned long long)ni->mft_no, "there was "
				"not enough memory to allocate an index "
				"context", ENOMEM);
		err = ENOMEM;
		goto list_err;
	}
	/*
	 * We cannot use SLIST_FOREACH() as that is not safe wrt to removal of
	 * the current element and we want to free each element as we go along
	 * so we do not have to traverse the list a second time just to do the
	 * freeing.
	 */
	dir_ni = NULL;
	while (!SLIST_EMPTY(&fn_list)) {
		next = SLIST_FIRST(&fn_list);
		/*
		 * We now have the next filename in @next->fn and
		 * @next->size.
		 */
		fn = &next->fn;
		dir_mft_no = MREF_LE(fn->parent_directory);
		/*
		 * Obtain the inode of the parent directory in which the
		 * current name is indexed if we do not have it already.
		 */
		if (!dir_ni || dir_ni->mft_no != dir_mft_no) {
			if (dir_ni) {
				lck_rw_unlock_exclusive(&dir_ia_ni->lock);
				lck_rw_unlock_exclusive(&dir_ni->lock);
				(void)vnode_put(dir_ia_ni->vn);
				(void)vnode_put(dir_ni->vn);
			}
			err = ntfs_inode_get(vol, dir_mft_no, FALSE,
					LCK_RW_TYPE_EXCLUSIVE, &dir_ni, NULL,
					NULL);
			if (err) {
				if (err != ENOENT) {
					ntfs_error(vol->mp, ies,
							(unsigned long long)
							ni->mft_no, "opening "
							"the parent directory "
							"inode failed", err);
					goto list_err;
				}
				/*
				 * Someone deleted the directory (and possibly
				 * recreated a new inode) under our feet.
				 * This is not an error so simply ignore this
				 * name and continue to the next one.
				 */
do_skip_name:
				ntfs_debug("Skipping name as it and its "
						"parent directory were "
						"unlinked under our feet.");
				dir_ni = NULL;
				goto skip_name;
			}
			/*
			 * If the directory has changed identity it has been
			 * deleted and recreated which means the directory
			 * entry we want to update has been removed so skip
			 * this name.
			 */
			if (dir_ni->seq_no != MSEQNO_LE(fn->parent_directory)) {
				lck_rw_unlock_exclusive(&dir_ni->lock);
				vnode_put(dir_ni->vn);
				goto do_skip_name;
			}
			err = ntfs_index_inode_get(dir_ni, I30, 4, FALSE,
					&dir_ia_ni);
			if (err) {
				ntfs_debug(ies, (unsigned long long)ni->mft_no,
						"opening the parent directory "
						"index inode failed", err);
				lck_rw_unlock_exclusive(&dir_ni->lock);
				(void)vnode_put(dir_ni->vn);
				goto list_err;
			}
			lck_rw_lock_exclusive(&dir_ia_ni->lock);
		}
		ntfs_index_ctx_init(ictx, dir_ia_ni);
		/* Get the index entry matching the current filename. */
		err = ntfs_index_lookup(fn, next->size, &ictx);
		if (err || ictx->entry->indexed_file !=
				MK_LE_MREF(ni->mft_no, ni->seq_no)) {
			if (err && err != ENOENT) {
				ntfs_error(vol->mp, ies,
						(unsigned long long)ni->mft_no,
						"looking up the name in the "
						"parent directory inode "
						"failed", err);
				if (err != ENOMEM)
					ignore_errors = FALSE;
				ntfs_index_ctx_put_reuse(ictx);
				lck_rw_unlock_exclusive(&dir_ia_ni->lock);
				lck_rw_unlock_exclusive(&dir_ni->lock);
				(void)vnode_put(dir_ia_ni->vn);
				(void)vnode_put(dir_ni->vn);
				goto list_err;
			}
			/*
			 * Someone unlinked the name (and possibly recreated a
			 * new inode) under our feet.  This is not an error so
			 * simply ignore this name and continue to the next
			 * one.
			 */
			ntfs_debug("Skipping name as it was unlinked under "
					"our feet.");
			goto put_skip_name;
		}
		/* Update the found index entry. */
		fn = &ictx->entry->key.filename;
		modified = FALSE;
		if (dirty_file_attributes && fn->file_attributes !=
				file_attributes) {
			fn->file_attributes = file_attributes;
			modified = TRUE;
		}
		if (dirty_times && (fn->creation_time != creation_time ||
				fn->last_data_change_time !=
				last_data_change_time ||
				fn->last_mft_change_time !=
				last_mft_change_time ||
				fn->last_access_time != last_access_time)) {
			fn->creation_time = creation_time;
			fn->last_data_change_time = last_data_change_time;
			fn->last_mft_change_time = last_mft_change_time;
			fn->last_access_time = last_access_time;
			modified = TRUE;
		}
		if (dirty_sizes && (fn->allocated_size != allocated_size ||
				fn->data_size != data_size)) {
			fn->allocated_size = allocated_size;
			fn->data_size = data_size;
			modified = TRUE;
		}
		/*
		 * If we changed anything, ensure the updates are written to
		 * disk.
		 */
		if (modified)
			ntfs_index_entry_mark_dirty(ictx);
put_skip_name:
		ntfs_index_ctx_put_reuse(ictx);
skip_name:
		SLIST_REMOVE_HEAD(&fn_list, list_entry);
        IOFreeData(next, next->alloc);
	}
	if (dir_ni) {
		lck_rw_unlock_exclusive(&dir_ia_ni->lock);
		lck_rw_unlock_exclusive(&dir_ni->lock);
		(void)vnode_put(dir_ia_ni->vn);
		(void)vnode_put(dir_ni->vn);
	}
	ntfs_index_ctx_free(ictx);
done:
	ntfs_debug("Done.");
	return 0;
list_err:
	/* Free all the copied filenames. */
	while (!SLIST_EMPTY(&fn_list)) {
		next = SLIST_FIRST(&fn_list);
		SLIST_REMOVE_HEAD(&fn_list, list_entry);
        IOFreeData(next, next->alloc);
	}
	if (ictx)
		ntfs_index_ctx_free(ictx);
err:
	if (actx)
		ntfs_attr_search_ctx_put(actx);
	if (m) {
		ntfs_mft_record_unmap(ni);
		lck_rw_unlock_shared(&ni->lock);
	}
	if (ignore_errors || err == ENOMEM) {
		ntfs_debug("Failed to sync ntfs inode.  Marking it dirty "
				"again, so that we try again later.");
		if (dirty_times)
			NInoSetDirtyTimes(ni);
		if (dirty_file_attributes)
			NInoSetDirtyFileAttributes(ni);
		if (dirty_sizes)
			NInoSetDirtySizes(ni);
		if (dirty_set_file_bits)
			NInoSetDirtySetFileBits(ni);
		if (ignore_errors)
			err = 0;
	} else {
		NVolSetErrors(vol);
		ntfs_error(vol->mp, "Failed (error %d).  Run chkdsk.", err);
	}
	return err;
}

/**
 * ntfs_inode_sync - synchronize an inode's in-core state with that on disk
 * @ni:				ntfs inode to synchronize to disk
 * @ioflags:			flags describing the i/o request
 * @skip_mft_record_sync:	do not sync the mft record(s) to disk
 *
 * Write all dirty cached data belonging/related to the ntfs inode @ni to disk.
 *
 * If @ioflags has the IO_SYNC bit set, wait for all i/o to complete before
 * returning.
 *
 * If @ioflags has the IO_CLOSE bit set, this signals cluster_push() that the
 * i/o is issued from the close path (or in our case more precisely from the
 * VNOP_INACTIVE() path).
 *
 * Note: When called from reclaim (via VNOP_FSYNC() and hence ntfs_vnop_fsync(),
 *	 the vnode has a zero v_iocount and v_usecount and vnode_isrecycled()
 *	 is true.  Thus we cannot obtain any attribute/raw inodes inside
 *	 ntfs_inode_sync() or the vnode_ref() on the base vnode that is done as
 *	 part of getting an attribute/raw inode causes a panic() to trigger as
 *	 both the iocount and usecount are zero on the base vnode.
 *
 * Return 0 on success and the error code on error.
 *
 * Locking: @ni->lock must be unlocked.
 *
 * TODO:/FIXME: For directory vnodes this currently does not sync much.  We
 * really need to sync the index allocation vnode and the bitmap vnode for
 * directories.
 *
 * TODO:/FIXME: For symbolic link vnodes this currently does not sync much.  We
 * really need to sync the raw vnode for symbolic links.
 *
 * TODO:/FIXME: At present we do not sync the AFP_AfpInfo named stream inode
 * when syncing the base inode.
 *
 * TODO:/FIXME: In general when a vnode is being synced we should ensure that
 * all associated (loaded) vnodes are synced also, i.e. not just the extent
 * inodes but also all the attribute/index inodes as well and once that is all
 * done we should cause all associated mft records to be synced.
 *
 * Theory of operation:
 *
 * Only base inodes (i.e. real files/unnamed $DATA/S_ISREG(ni->mode) and
 * directories/S_IFDIR(ni->mode) need to have their information synced with the
 * standard information attribute and hence with all directory entries pointing
 * to those inodes also.
 *
 * Further, all changes to the ntfs inode structure @ni happen exclusively
 * through the ntfs driver thus we can mark @ni dirty on any modification and
 * then we can check and clear the flag here and only if it was set do we need
 * to go through and check what needs to be updated and to update it in the
 * standard information attribute (and all directory entries pointing to the
 * inode).
 *
 * However, changes to the contents of an attribute, can happen to all
 * attributes, i.e. base inodes, attribute inodes, and index inodes all alike.
 * Further changes to the contents can happen both under control of the ntfs
 * driver and outside its control via mmap() based writes for example.  Thus we
 * have no mechanism for determining whether file data is dirty or not and thus
 * we have to unconditionally perform an msync() on the entire file data.
 *
 * The msync() can in turn cause the mft record containing the attribute to be
 * dirtied, for example because the attribute is resident and the msync()
 * caused the data to go from the VM page cache into the mft record thus
 * dirtying it.
 *
 * So we at the end need to sync all mft records associated with the attribute.
 * Once again, the only way mft records are modified is through the ntfs driver
 * so we could set a flag each time we modify an mft record and check it and
 * only write if it is set.  However we do not do this as such flags would
 * invariably be out of date with reality because the mft records are stored as
 * the contents of the system file $MFT (S_ISREG()) which we access using
 * buf_meta_bread() and buf_bdwrite(), etc, thus they are governed by the
 * buffer layer and their dirtyness is tracked at a buffer (i.e. per mft
 * record) level by the buffer layer.  And the buffer layer can cause a buffer
 * to be written out without the ntfs driver having an easy means to go and
 * clear the putative dirty bit in the ntfs inode @ni.  Thus we do not use a
 * dirty flag for the mft records and instead buf_getblk() all cached buffers
 * containing loaded mft records belonging to the base ntfs inode of @ni and
 * for the ones that are dirty we cause them to be written out by calling
 * buf_bwrite().  We determine which mft records are loaded by iterating
 * through the @extent_nis array of the base ntfs inode of @ni.  This will skip
 * any mft records that are dirty but have been freed/deallocated from the
 * inode but this is irrelevant as for all intents and purposes they no longer
 * belong to the inode @ni.  They will still be synced to disk when the $MFT
 * inode is synced or the buffer layer pushes the dirty buffer containing the
 * freed mft record to disk.
 *
 * As a speed optimization when ntfs_inode_sync() is called from VFS_SYNC() and
 * thus from ntfs_sync(), we do not sync the mft records at all as ntfs_sync()
 * will as the last thing call ntfs_inode_sync() for $MFT itself and then all
 * dirty mft records can be synced in one single go via a single
 * buf_flushdirtyblks() on the entire data content of $MFT.  This massively
 * reduces disk head seeking and nicely streamlines and batches writes to the
 * $MFT.
 */
errno_t ntfs_inode_sync(ntfs_inode *ni, const int ioflags,
		const BOOL skip_mft_record_sync)
{
	ntfs_inode *base_ni;
	errno_t err;

	ntfs_debug("Entering for %sinode 0x%llx, %ssync i/o, ioflags 0x%04x.",
			NInoAttr(ni) ? "attr " : "",
			(unsigned long long)ni->mft_no,
			(ioflags & IO_SYNC) ? "a" : "", ioflags);
	base_ni = ni;
	if (NInoAttr(ni)) {
		base_ni = ni->base_ni;
		if (ni != base_ni)
			lck_rw_lock_shared(&base_ni->lock);
	}
	/* Do not allow messing with the inode once it has been deleted. */
	lck_rw_lock_shared(&ni->lock);
	if (NInoDeleted(ni)) {
		/* Remove the inode from the name cache. */
		cache_purge(ni->vn);
		lck_rw_unlock_shared(&ni->lock);
		if (ni != base_ni)
			lck_rw_unlock_shared(&base_ni->lock);
		ntfs_debug("Inode is deleted.");
		return ENOENT;
	}
	/*
	 * This cannot happen as the attribute/raw inode holds a reference on
	 * the vnode of its base inode.
	 */
	if (ni != base_ni && NInoDeleted(base_ni))
		panic("%s(): Called for attribute inode whose base inode is "
				"NInoDeleted()!\n", __FUNCTION__);
	lck_rw_unlock_shared(&ni->lock);
	if (ni != base_ni)
		lck_rw_unlock_shared(&base_ni->lock);
	/*
	 * First of all, flush any dirty data.  This is done for all attribute
	 * inodes as well as for regular file base inodes.
	 * There is no need to do it for directory inodes, symbolic links
	 * fifos, sockets, or block and character device special files as they
	 * do not contain any data.
	 * We actually check for the vnode type being VREG as that is the case
	 * for all attribute inodes as well as for all regular files.
	 *
	 * Further, we do not yet support writing data for non-resident
	 * encrypted/compressed attributes so silently skip those here.  We do
	 * not want to fail completely because we want to allow access times
	 * and other flags/attributes to be updated.
	 */
	if (vnode_vtype(ni->vn) == VREG && (!NInoNonResident(ni) ||
			ni->type == AT_INDEX_ALLOCATION || NInoRaw(ni) ||
			(!NInoEncrypted(ni) && !NInoCompressed(ni)))) {
		err = ntfs_inode_data_sync(ni, ioflags);
		if (err)
			return err;
	}
	/*
	 * If this is a base inode and it contains any dirty fields that have
	 * not been synced to the standard information attribute in the mft
	 * record yet, update the standard information attribute and update all
	 * directory entries pointing to the inode if any affected fields were
	 * modified.
	 */
	if (ni == base_ni && NInoDirty(ni)) {
		err = ntfs_inode_sync_to_mft_record(ni);
		if (err)
			return err;
	}
	/*
	 * If we are called from ntfs_sync() we want to skip writing the mft
	 * records as that will happen at the end of the ntfs_sync() call.
	 */
	if (skip_mft_record_sync) {
		ntfs_debug("Done (skipped mft record(s) sync).");
		return 0;
	}
	/*
	 * If this inode does not have an attribute list attribute there is
	 * only one mft record associated with the inode thus we can write it
	 * now if it is dirty and we are finished if not.
	 *
	 * If the inode does have an attribute list attribute then we need to
	 * go through all loaded mft records, starting with the base inode and
	 * looking at all its attached extent inodes and we need to write the
	 * ones that have dirty mft records out one by one.
	 */
	err = ntfs_mft_record_sync(base_ni);
	if (NInoAttrList(base_ni)) {
		int nr_extents;

		lck_mtx_lock(&base_ni->extent_lock);
		nr_extents = base_ni->nr_extents;
		if (nr_extents > 0) {
			ntfs_inode **extent_nis = base_ni->extent_nis;
			errno_t err2;
			int i;

			ntfs_debug("Syncing %d extent inodes.", nr_extents);
			for (i = 0; i < nr_extents; i++) {
				err2 = ntfs_mft_record_sync(extent_nis[i]);
				if (err2 && (!err || err == ENOMEM))
					err = err2;
			}
		}
		lck_mtx_unlock(&base_ni->extent_lock);
	}
	if (!err) {
		ntfs_debug("Done.");
		return 0;
	}
	if (err == ENOMEM)
		ntfs_warning(ni->vol->mp, "Not enough memory to sync inode.");
	else {
		NVolSetErrors(ni->vol);
		ntfs_error(ni->vol->mp, "Failed to sync mft_no 0x%llx (error "
				"%d).  Run chkdsk.",
				(unsigned long long)ni->mft_no, err);
	}
	return err;
}

/**
 * ntfs_inode_get_name_and_parent_mref - get the name and parent mft reference
 * @ni:			ntfs inode whose name and parent mft reference to find
 * @have_parent:	true if @parent_mref already contains an mft reference
 * @parent_mref:	destination to return the parent mft reference in
 * @name:		destination to return the name in or NULL
 *
 * If @have_parent is false, look up the first, non-DOS filename attribute in
 * the mft record(s) of the ntfs inode @ni and return the name contained in the
 * filename attribute in @name as well as the parent mft reference contained in
 * the filename attribute in *@parent_mref.  If @name is NULL the name is not
 * returned.
 *
 * If @name is NULL, check if there is a name cached in the vnode of the inode
 * @ni, and if so, look for the filename attribute matching this name and if
 * one is found, return its parent id.  If one is not found return the first,
 * non-DOS filename attribute as described above.  Note there is no point in
 * doing this unless the link count of the inode @ni is larger than one as
 * otherwise there is only one filename attribute and thus we do not need to
 * bother doing any comparissons as it is the only name we can return thus we
 * return it.
 *
 * If @have_parent is true, iterate over the filename attributes in the mft
 * record until we find the one matching the parent mft reference @parent_mref
 * and return the corresponding name in @name.  If such a name is not found,
 * revert to the previous case where @have_parent is false, i.e. return the
 * first, non-DOS filename in @name and the corresponding parent mft reference
 * in *@parent_mref.  Note that as above in the @name is NULL case, there is no
 * point in doing this unless the link count of the inode @ni is larger than
 * one as otherwise there is only one filename attribute and thus we return it.
 *
 * If @have_parent is true @name must not be NULL as it makes no sense to look
 * only for the parent mft reference when the caller already has it.
 *
 * Return 0 on success and the error code on error.
 */
errno_t ntfs_inode_get_name_and_parent_mref(ntfs_inode *ni, BOOL have_parent,
		MFT_REF *parent_mref, const char *name)
{
	MFT_REF mref;
	ntfs_inode *base_ni;
	ntfschar *ntfs_name;
	MFT_RECORD *m;
	ntfs_attr_search_ctx *ctx;
	ATTR_RECORD *a;
	FILENAME_ATTR *fn;
	size_t name_size;
	unsigned link_count = ni->link_count;
	signed res_size = 0;
	errno_t err;
	BOOL name_present;
	ntfschar ntfs_name_buf[link_count > 1 ? NTFS_MAX_NAME_LEN : 0];

	ntfs_debug("Entering for mft_no 0x%llx.",
			(unsigned long long)ni->mft_no);
	if (have_parent && !name)
		panic("%s(): have_parent && !name\n", __FUNCTION__);
	/*
	 * As explained above do not bother doing anything fancy unless the
	 * link count of the inode @ni is greater than one.
	 */
	ntfs_name = NULL;
	if (link_count > 1) {
		if (!name) {
			const char *vn_name;

			vn_name = vnode_getname(ni->vn);
			if (vn_name) {
				/* Convert the name from utf8 to Unicode. */
				ntfs_name = ntfs_name_buf;
				name_size = sizeof(ntfs_name_buf);
				res_size = utf8_to_ntfs(ni->vol, (u8*)vn_name,
						strlen(vn_name), &ntfs_name,
						&name_size);
				(void)vnode_putname(vn_name);
				/*
				 * If we failed to convert the name, warn the
				 * user about it and then continue execution
				 * pretending that there is no cached name,
				 * i.e. ignoring the potentially corrupt name.
				 */
				if (res_size < 0) {
					ntfs_warning(ni->vol->mp, "Failed to "
							"convert cached name "
							"to Unicode (error "
							"%d).  This may "
							"indicate "
							"corruption.  You "
							"should unmount and "
							"run chkdsk.",
							-res_size);
					NVolSetErrors(ni->vol);
					ntfs_name = NULL;
				}
			}
		}
	} else
		have_parent = FALSE;
	base_ni = ni;
	if (NInoAttr(ni))
		base_ni = ni->base_ni;
	if (!link_count || (ni != base_ni && !base_ni->link_count))
		goto deleted;
	/* Map the mft record. */
	err = ntfs_mft_record_map(base_ni, &m);
	if (err) {
		ntfs_error(ni->vol->mp, "Failed to map mft record (error %d).",
				err);
		return err;
	}
	/* Verify the mft record has not been deleted. */
	if (!(m->flags & MFT_RECORD_IN_USE))
		goto unm_deleted;
	/* Find the first filename attribute in the mft record. */
	ctx = ntfs_attr_search_ctx_get(base_ni, m);
	if (!ctx) {
		ntfs_error(ni->vol->mp, "Failed to allocate search context "
				"(error %d).", err);
		err = ENOMEM;
		goto err;
	}
	name_present = FALSE;
try_next:
	err = ntfs_attr_lookup(AT_FILENAME, AT_UNNAMED, 0, 0, NULL, 0, ctx);
	if (err) {
		if (err == ENOENT && name_present) {
			have_parent = name_present = FALSE;
			ntfs_name = NULL;
			ntfs_attr_search_ctx_reinit(ctx);
			goto try_next;
		}
		ntfs_error(ni->vol->mp, "Failed to find a valid filename "
				"attribute (error %d).", err);
		goto put_err;
	}
	a = ctx->a;
	fn = (FILENAME_ATTR*)((u8*)a + le16_to_cpu(a->value_offset));
	/* If the filename attribute is invalid/corrupt abort. */
	if (a->non_resident || (u8*)fn + le32_to_cpu(a->value_length) >
			(u8*)a + le32_to_cpu(a->length)) {
		ntfs_error(ni->vol->mp, "Found corrupt filename attribute in "
				"mft_no 0x%llx.  Unmount and run chkdsk.",
				(unsigned long long)ni->mft_no);
		NVolSetErrors(ni->vol);
		err = EIO;
		goto put_err;
	}
	/*
	 * Do not return the DOS name.  If it exists there must also be a
	 * matching WIN32 name or the inode is corrupt.
	 */
	if (fn->filename_type == FILENAME_DOS)
		goto try_next;
	mref = le64_to_cpu(fn->parent_directory);
	/*
	 * If we have a cached name, check if the current filename attribute
	 * matches this name and if not try the next name.
	 *
	 * We can do a case sensitive comparison because we only ever cache
	 * correctly cased names in the vnode.
	 */
	if (ntfs_name && (res_size != fn->filename_length ||
			bcmp(ntfs_name, fn->filename, res_size))) {
		name_present = TRUE;
		goto try_next;
	}
	/*
	 * If we already have a parent mft reference and the current filename
	 * attribute has a different parent mft reference try the next name.
	 *
	 * Note we have to only compare the sequence number if one is passed to
	 * us in *@parent_mref, i.e. if MSEQNO(*@parent_mref) is not zero.
	 */
	if (have_parent && (MREF(*parent_mref) != MREF(mref) ||
			(MSEQNO(*parent_mref) &&
			MSEQNO(*parent_mref) != MSEQNO(mref)))) {
		name_present = TRUE;
		goto try_next;
	}
	/*
	 * If we are looking for the name, convert it from NTFS Unicode to
	 * UTF-8 OS X string format and save it in @name.
	 */
	if (name) {
		name_size = MAXPATHLEN;
		res_size = ntfs_to_utf8(ni->vol, fn->filename,
				fn->filename_length << NTFSCHAR_SIZE_SHIFT,
				(u8**)&name, &name_size);
		if (res_size < 0) {
			ntfs_warning(ni->vol->mp, "Failed to convert name of "
					"mft_no 0x%llx to UTF8 (error %d).",
					(unsigned long long)ni->mft_no,
					-res_size);
			goto try_next;
		}
	}
	/* Get the inode number of the parent directory into *@parent_mref. */
	*parent_mref = mref;
	/*
	 * Release the search context and the mft record of the inode as we do
	 * not need them any more.
	 */
	ntfs_attr_search_ctx_put(ctx);
	ntfs_mft_record_unmap(base_ni);
	if (name)
		ntfs_debug("Done (mft_no 0x%llx has parent mft_no 0x%llx and "
				"name %.*s).", (unsigned long long)ni->mft_no,
				(unsigned long long)MREF(mref), res_size, name);
	else
		ntfs_debug("Done (mft_no 0x%llx has parent mft_no 0x%llx "
				"(name was not requested and was %scached)).",
				(unsigned long long)ni->mft_no,
				(unsigned long long)MREF(mref),
				ntfs_name ? "" : "not ");
	return 0;
unm_deleted:
	ntfs_mft_record_unmap(base_ni);
deleted:
	ntfs_debug("Inode 0x%llx has been deleted, returning ENOENT.",
			(unsigned long long)ni->mft_no);
	return ENOENT;
put_err:
	ntfs_attr_search_ctx_put(ctx);
err:
	ntfs_mft_record_unmap(base_ni);
	return err;
}

/**
 * ntfs_inode_is_parent - test if an inode is a parent of another inode
 * @parent_ni:	ntfs inode to check for being a parent of @child_ni
 * @child_ni:	ntfs inode to check for being a child of @parent_ni
 * @is_parent:	pointer in which to return the result of the test
 * @forbid_ni:	ntfs inode that may not be encountered on the path or NULL
 *
 * Starting with @child_ni, walk up the file system directory tree until the
 * root directory of the volume is reached.  Compare the inodes found along the
 * way, i.e. the parent inodes of @child_ni, against @parent_ni and if one of
 * them matches @parent_ni we know that @parent_ni is indeed a parent of
 * @child_ni thus we return true in *@is_parent.  If we reach the root
 * directory without matching @parent_ni we know that @parent_ni is definitely
 * not a parent of @child_ni thus we return false in *@is_parent.
 *
 * If @forbid_ni is NULL it is ignored.  If it is not NULL it is an ntfs inode
 * which may not be located on the path traversed during the parent lookup.  If
 * it is present, return EINVAL.  This is used in ntfs_vnop_rename() where the
 * source inode may not be a parent directory of the destination directory or a
 * loop would be created if the rename was allowed to continue.
 *
 * Return 0 on success and the error code on error.  On error *@is_parent is
 * not defined.
 *
 * Locking: - The volume rename lock must be held by the caller to ensure that
 *	      the relationship between the inodes cannot change under our feet.
 *	    - The caller must hold an iocount reference on both @parent_ni and
 *	      @child_ni.
 *
 * Note both @parent_ni and @child_ni must be directory inodes.
 */
errno_t ntfs_inode_is_parent(ntfs_inode *parent_ni, ntfs_inode *child_ni,
		BOOL *is_parent, ntfs_inode *forbid_ni)
{
	ntfs_volume *vol;
	ntfs_inode *root_ni, *ni;
	vnode_t vn, prev_vn;

	if (forbid_ni)
		ntfs_debug("Entering for parent mft_no 0x%llx, child mft_no "
				"0x%llx, and forbidden mft_no 0x%llx.",
				(unsigned long long)parent_ni->mft_no,
				(unsigned long long)child_ni->mft_no,
				(unsigned long long)forbid_ni->mft_no);
	else
		ntfs_debug("Entering for parent mft_no 0x%llx and child "
				"mft_no 0x%llx.",
				(unsigned long long)parent_ni->mft_no,
				(unsigned long long)child_ni->mft_no);
	vol = child_ni->vol;
	root_ni = vol->root_ni;
	ni = child_ni;
	prev_vn = NULL;
	vn = child_ni->vn;
	/*
	 * Iterate over the parent inodes until we reach the root directory
	 * inode @root_ni of the volume.
	 */
	while (ni != root_ni) {
		if (ni == forbid_ni) {
			ntfs_debug("Forbidden mft_no 0x%llx is a parent of "
					"child mft_no 0x%llx.  Returning "
					"EINVAL.",
					(unsigned long long)forbid_ni->mft_no,
					(unsigned long long)child_ni->mft_no);
			if (prev_vn) {
				lck_rw_unlock_shared(&ni->lock);
				(void)vnode_put(prev_vn);
			}
			return EINVAL;
		}
		/*
		 * Try to find the parent vnode of the current inode in the
		 * current vnode and if it is not present try to get it by hand
		 * by looking up the filename attribute in the mft record of
		 * the inode.
		 */
		vn = vnode_getparent(vn);
		if (vn) {
			if (prev_vn) {
				lck_rw_unlock_shared(&ni->lock);
				(void)vnode_put(prev_vn);
			}
			ni = NTFS_I(vn);
			lck_rw_lock_shared(&ni->lock);
			if (NInoDeleted(ni))
				panic("%s(): vnode_getparent() returned "
						"NInoDeleted() inode!\n",
						__FUNCTION__);
			/* Check the inode has not been deleted. */
			if (!ni->link_count)
				goto deleted;
		} else {
			MFT_REF mref;
			s64 mft_no;
			errno_t err;
			u16 seq_no;

			/*
			 * The vnode of the parent is not attached to the vnode
			 * of the current inode thus find the parent mft
			 * reference by hand.
			 */
			err = ntfs_inode_get_name_and_parent_mref(ni, FALSE,
					&mref, NULL);
			mft_no = ni->mft_no;
			if (prev_vn) {
				lck_rw_unlock_shared(&ni->lock);
				(void)vnode_put(prev_vn);
			}
			if (err) {
				ntfs_error(vol->mp, "Failed to determine "
						"parent mft reference of "
						"mft_no 0x%llx (error %d).",
						(unsigned long long)mft_no,
						err);
				return err;
			}
			/* Get the inode with mft reference @mref. */
			err = ntfs_inode_get(vol, MREF(mref), FALSE,
					LCK_RW_TYPE_SHARED, &ni, NULL, NULL);
			if (err) {
				ntfs_error(vol->mp, "Failed to obtain parent "
						"mft_no 0x%llx of mft_no "
						"0x%llx (error %d).",
						(unsigned long long)MREF(mref),
						(unsigned long long)mft_no,
						err);
				return err;
			}
			vn = ni->vn;
			/* Check the inode has not been deleted and reused. */
			seq_no = MSEQNO(mref);
			if (seq_no && seq_no != ni->seq_no)
				goto deleted;
		}
		/*
		 * We found the parent inode.  If it equals @parent_ni it means
		 * that our test is successful and @parent_ni is indeed a
		 * parent directory of @child_ni thus set *@is_parent to true
		 * and return success.
		 */
		if (ni == parent_ni) {
			lck_rw_unlock_shared(&ni->lock);
			(void)vnode_put(ni->vn);
			*is_parent = TRUE;
			ntfs_debug("Parent mft_no 0x%llx is a parent of "
					"child mft_no 0x%llx.",
					(unsigned long long)parent_ni->mft_no,
					(unsigned long long)child_ni->mft_no);
			return 0;
		}
		prev_vn = vn;
	}
	if (prev_vn) {
		lck_rw_unlock_shared(&ni->lock);
		(void)vnode_put(prev_vn);
	}
	/*
	 * We reached the root directory of the volume without encountering
	 * @parent_ni thus it is not a parent of @child_ni so set *@is_parent
	 * to false and return success.
	 */
	*is_parent = FALSE;
	ntfs_debug("Parent mft_no 0x%llx is not a parent of child mft_no "
			"0x%llx.", (unsigned long long)parent_ni->mft_no,
			(unsigned long long)child_ni->mft_no);
	return 0;
deleted:
	ntfs_error(ni->vol->mp, "Parent mft_no 0x%llx has been deleted.  "
			"Returning ENOENT.", (unsigned long long)ni->mft_no);
	lck_rw_unlock_shared(&ni->lock);
	(void)vnode_put(vn);
	return ENOENT;
}
