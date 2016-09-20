/*
 * Copyright (c) 2014-2015 Apple Inc. All rights reserved.
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

#if CONFIG_PROTECT

#include "hfs_key_roll.h"

#include <sys/cprotect.h>
#include <sys/ubc.h>
#include <sys/fcntl.h>
#include <sys/xattr.h>
#include <kern/sched_prim.h>
#include <vm/vm_pageout.h>
#include <pexpert/pexpert.h>

#include "hfs.h"
#include "hfs_extents.h"
#include "hfs_kdebug.h"

#define PTR_ADD(type, base, offset)		(type)((uintptr_t)(base) + (offset))

#define HFS_KEY_ROLL_MAX_CHUNK_BYTES	2 * 1024 * 1024

static inline void log_error(const char *func, unsigned line, errno_t err)
{
	printf("%s:%u error: %d\n", func, line, err);
}

#define LOG_ERROR(err)	log_error(__func__, __LINE__, err)

#define CHECK(x, var, goto_label)									\
	do {															\
		var = (x);													\
		if (var) {													\
			if (var != ENOSPC)										\
				LOG_ERROR(var);										\
			goto goto_label;										\
		}															\
	} while (0)

#define min(a, b) \
	({ typeof (a) _a = (a); typeof (b) _b = (b); _a < _b ? _a : _b; })

// -- Some locking helpers --

/*
 * These helpers exist to help clean up at the end of a function.  A
 * lock context structure is stored on the stack and is cleaned up
 * when we it goes out of scope and will automatically unlock the lock
 * if it happens to be locked.  It is also safe to call the unlock
 * functions when no lock has been taken and this is cleaner than
 * having to declare a separate variable like have_lock and having to
 * test for it before unlocking.
 */

typedef struct {
	void *object;
	int flags;
} hfs_lock_ctx_t;

#define DECL_HFS_LOCK_CTX(var, unlocker)	\
	hfs_lock_ctx_t var __attribute__((cleanup(unlocker))) = {}

static inline bool hfs_is_locked(hfs_lock_ctx_t *lock_ctx)
{
	return lock_ctx->object != NULL;
}

static inline int hfs_lock_flags(hfs_lock_ctx_t *lock_ctx)
{
	return lock_ctx->flags;
}

static inline void hfs_lock_cp(cnode_t *cp,
							   enum hfs_locktype lock_type,
							   hfs_lock_ctx_t *lock_ctx)
{
	hfs_lock_always(cp, lock_type);
	lock_ctx->object = cp;
}

static inline void hfs_unlock_cp(hfs_lock_ctx_t *lock_ctx)
{
	if (lock_ctx->object) {
		hfs_unlock(lock_ctx->object);
		lock_ctx->object = NULL;
	}
}

static inline void hfs_lock_trunc(cnode_t *cp,
								  enum hfs_locktype lock_type,
								  hfs_lock_ctx_t *lock_ctx)
{
	hfs_lock_truncate(cp, lock_type, 0);
	lock_ctx->object = cp;
}

static inline void hfs_unlock_trunc(hfs_lock_ctx_t *lock_ctx)
{
	if (lock_ctx->object) {
		hfs_unlock_truncate(lock_ctx->object, 0);
		lock_ctx->object = NULL;
	}
}

static inline void hfs_lock_sys(struct hfsmount *hfsmp, int flags,
								enum hfs_locktype lock_type,
								hfs_lock_ctx_t *lock_ctx)
{
	lock_ctx->flags |= hfs_systemfile_lock(hfsmp, flags, lock_type);
	lock_ctx->object = hfsmp;
}

static inline void hfs_unlock_sys(hfs_lock_ctx_t *lock_ctx)
{
	if (lock_ctx->object) {
		hfs_systemfile_unlock(lock_ctx->object, lock_ctx->flags);
		lock_ctx->object = NULL;
		lock_ctx->flags = 0;
	}
}

// --

#if DEBUG
static const uint32_t ckr_magic1 = 0x7b726b63;
static const uint32_t ckr_magic2 = 0x726b637d;
#endif

hfs_cp_key_roll_ctx_t *hfs_key_roll_ctx_alloc(const hfs_cp_key_roll_ctx_t *old,
											  uint16_t pers_key_len,
											  uint16_t cached_key_len,
											  cp_key_pair_t **pcpkp)
{
	hfs_cp_key_roll_ctx_t *ckr;

	size_t size = (sizeof(*ckr) - sizeof(cp_key_pair_t)
				   + cpkp_size(pers_key_len, cached_key_len));

#if DEBUG
	size += 4;
#endif

	ckr = hfs_mallocz(size);

#if DEBUG
	ckr->ckr_magic1 = ckr_magic1;
	*PTR_ADD(uint32_t *, ckr, size - 4) = ckr_magic2;
#endif

	cpkp_init(&ckr->ckr_keys, pers_key_len, cached_key_len);

	if (old) {
		if (old->ckr_busy)
			panic("hfs_key_roll_ctx_alloc: old context busy!");
		ckr->ckr_off_rsrc = old->ckr_off_rsrc;
		ckr->ckr_preferred_next_block = old->ckr_preferred_next_block;

		cpx_copy(cpkp_cpx(&old->ckr_keys), cpkp_cpx(&ckr->ckr_keys));
	}

	*pcpkp = &ckr->ckr_keys;

	return ckr;
}

static void hfs_key_roll_ctx_free(hfs_cp_key_roll_ctx_t *ckr)
{
	size_t size = (sizeof(*ckr) - sizeof(cp_key_pair_t)
				   + cpkp_sizex(&ckr->ckr_keys));

#if DEBUG
	size += 4;
#endif

	hfs_free(ckr, size);
}

void hfs_release_key_roll_ctx(hfsmount_t *hfsmp, cprotect_t cpr)
{
	hfs_cp_key_roll_ctx_t *ckr = cpr->cp_key_roll_ctx;

	if (!ckr)
		return;

	cpkp_flush(&ckr->ckr_keys);

	if (ckr->ckr_tentative_reservation) {
		int lockf = hfs_systemfile_lock(hfsmp, SFL_BITMAP, HFS_EXCLUSIVE_LOCK);
		hfs_free_tentative(hfsmp, &ckr->ckr_tentative_reservation);
		hfs_systemfile_unlock(hfsmp, lockf);
	}

	cpr->cp_key_roll_ctx = NULL;

	cnode_t *cp = cpr->cp_backing_cnode;
	if (cp)
		wakeup(&cp->c_cpentry);

#if DEBUG
	hfs_assert(ckr->ckr_magic1 == ckr_magic1);
	hfs_assert(*PTR_ADD(uint32_t *, ckr, sizeof(*ckr) - sizeof(cp_key_pair_t)
					+ cpkp_sizex(&ckr->ckr_keys) == ckr_magic2));
#endif

	hfs_key_roll_ctx_free(ckr);
}

// Records current status into @args
static void hfs_key_roll_status(cnode_t *cp, hfs_key_roll_args_t *args)
{
	hfsmount_t				*hfsmp	= VTOHFS(cp->c_vp);
	cprotect_t const		 cpr	= cp->c_cpentry;
	filefork_t				*dfork  = cp->c_datafork;

	if (!cpr || !dfork) {
		args->key_class_generation = 0;
		args->key_revision = 0;
		args->key_os_version = 0;
		args->done = -1;
		args->total = 0;
		return;
	}

	uint32_t total_blocks = cp->c_blocks - dfork->ff_unallocblocks;
	if (cp->c_rsrcfork)
		total_blocks -= cp->c_rsrcfork->ff_unallocblocks;
	args->total = hfs_blk_to_bytes(total_blocks, hfsmp->blockSize);

	args->key_class_generation	= cp_get_crypto_generation(cpr->cp_pclass);
	args->key_revision			= cpr->cp_key_revision;
	args->key_os_version		= cpr->cp_key_os_version;

	hfs_cp_key_roll_ctx_t *ckr = cpr->cp_key_roll_ctx;

	if (!ckr)
		args->done = -1;
	else {
		args->done = off_rsrc_get_off(ckr->ckr_off_rsrc);

		if (off_rsrc_is_rsrc(ckr->ckr_off_rsrc)) {
			args->done += hfs_blk_to_bytes(ff_allocblocks(dfork),
										   hfsmp->blockSize);
		}
	}
}

// The fsctl calls this
errno_t hfs_key_roll_op(vfs_context_t vfs_ctx, vnode_t vp,
						hfs_key_roll_args_t *args)
{
	errno_t					 ret;
	cnode_t * const			 cp		= VTOC(vp);
	cprotect_t const		 cpr	= cp->c_cpentry;
	hfs_cp_key_roll_ctx_t	*ckr	= NULL;

	DECL_HFS_LOCK_CTX(cp_lock, hfs_unlock_cp);
	DECL_HFS_LOCK_CTX(trunc_lock, hfs_unlock_trunc);

	KDBG(HFSDBG_KEY_ROLL | DBG_FUNC_START, kdebug_vnode(vp), args->operation);

	if (args->api_version != HFS_KR_API_VERSION_1) {
		ret = ENOTSUP;
		goto exit;
	}

	if (args->operation != HFS_KR_OP_STATUS) {
		ret = cp_handle_vnop(vp, CP_WRITE_ACCESS, 0);
		if (ret)
			goto exit;
	}

	switch (args->operation) {
		case HFS_KR_OP_START:
			if (!cpr) {
				ret = ENOTSUP;
				goto exit;
			}

			/*
			 * We must hold the truncate lock exclusively in case we have to
			 * rewrap.
			 */
			hfs_lock_trunc(cp, HFS_EXCLUSIVE_LOCK, &trunc_lock);
			hfs_lock_cp(cp, HFS_EXCLUSIVE_LOCK, &cp_lock);

			ckr = cpr->cp_key_roll_ctx;
			if (ckr)
				break;

			// Only start rolling if criteria match
			if (ISSET(args->flags, HFS_KR_MATCH_KEY_CLASS_GENERATION)
				&& (args->key_class_generation
					!= cp_get_crypto_generation(cpr->cp_pclass))) {
				break;
			}

			if (ISSET(args->flags, HFS_KR_MATCH_KEY_REVISION)
				&& args->key_revision != cpr->cp_key_revision) {
				break;
			}

			if (ISSET(args->flags, HFS_KR_MATCH_KEY_OS_VERSION)
				&& args->key_os_version != cpr->cp_key_os_version) {
				break;
			}

			if (cp->c_cpentry->cp_raw_open_count > 0) {
				// Cannot start key rolling if file is opened for raw access
				ret = EBUSY;
				goto exit;
			}

			ret = hfs_key_roll_start(cp);
			if (ret)
				goto exit;
			break;

		case HFS_KR_OP_STATUS:
			break;

		case HFS_KR_OP_STEP:
			CHECK(hfs_key_roll_step(vfs_ctx, vp, INT64_MAX), ret, exit);
			break;

		case HFS_KR_OP_SET_INFO:
			if (!ISSET(PE_i_can_has_kernel_configuration(), kPEICanHasDiagnosticAPI)) {
				ret = EPERM;
				goto exit;
			}

			if (!cpr) {
				ret = ENOTSUP;
				goto exit;
			}

			hfs_lock_cp(cp, HFS_EXCLUSIVE_LOCK, &cp_lock);
			cpr->cp_key_revision = args->key_revision;
			cpr->cp_key_os_version = args->key_os_version;
			ret = cp_setxattr(cp, cpr, VTOHFS(vp), 0, 0);
			if (ret)
				goto exit;
			break;

		default:
			ret = EINVAL;
			goto exit;
	}

	if (!hfs_is_locked(&cp_lock))
		hfs_lock_cp(cp, HFS_SHARED_LOCK, &cp_lock);

	hfs_key_roll_status(cp, args);

	ret = 0;

exit:

	hfs_unlock_cp(&cp_lock);
	hfs_unlock_trunc(&trunc_lock);

	KDBG(HFSDBG_KEY_ROLL | DBG_FUNC_END, ret, ret ? 0 : args->done);

	return ret;
}

/*
 * Initiates key rolling.  cnode and truncate locks *must* be held 
 * exclusively.
 */
errno_t hfs_key_roll_start(cnode_t *cp)
{
	errno_t ret;

	hfs_assert(!cp->c_cpentry->cp_key_roll_ctx);

	if (CP_CLASS(cp->c_cpentry->cp_pclass) == PROTECTION_CLASS_F)
		return ENOTSUP;

	hfsmount_t *hfsmp = VTOHFS(cp->c_vp);

	if (ISSET(hfsmp->hfs_flags, HFS_READ_ONLY))
		return EROFS;

	if (!hfsmp->jnl || !S_ISREG(cp->c_mode))
		return ENOTSUP;

	cprotect_t cpr = cp->c_cpentry;

	cp_key_class_t key_class = cpr->cp_pclass;

	hfs_unlock(cp);

	hfs_cp_key_roll_ctx_t *ckr;

	int attempts = 0;
	cp_key_revision_t rev = cp_next_key_revision(cp->c_cpentry->cp_key_revision);

	for (;;) {
		ret = cp_new(&key_class, hfsmp, cp, cp->c_mode, CP_KEYWRAP_DIFFCLASS, rev,
					 (cp_new_alloc_fn)hfs_key_roll_ctx_alloc, (void **)&ckr);
		if (ret) {
			hfs_lock_always(cp, HFS_EXCLUSIVE_LOCK);
			goto exit;
		}

		if (key_class == cpr->cp_pclass) {
			// The usual and easy case: the classes match
			hfs_lock_always(cp, HFS_EXCLUSIVE_LOCK);
			break;
		}

		// AKS has given us a different class, so we need to rewrap

		// The truncate lock is not sufficient
		vnode_waitforwrites(cp->c_vp, 0, 0, 0, "hfs_key_roll_start");

		// And the resource fork
		if (cp->c_rsrc_vp)
			vnode_waitforwrites(cp->c_rsrc_vp, 0, 0, 0, "hfs_key_roll_start");

		hfs_lock_always(cp, HFS_EXCLUSIVE_LOCK);

		cp_key_class_t key_class2 = key_class;
		cprotect_t new_entry;

		ret = cp_rewrap(cp, hfsmp, &key_class2, &cpr->cp_keys, cpr,
						(cp_new_alloc_fn)cp_entry_alloc, (void **)&new_entry);

		if (ret) {
			hfs_key_roll_ctx_free(ckr);
			goto exit;
		}

		if (key_class2 == key_class) {
			// Great, fix things up and we're done
			cp_replace_entry(hfsmp, cp, new_entry);
			cpr = new_entry;
			cpr->cp_pclass = key_class;
			break;
		}

		/*
		 * Oh dear, key classes don't match.  Unlikely, but perhaps class
		 * generation was rolled again.
		 */

		hfs_key_roll_ctx_free(ckr);
		cp_entry_destroy(hfsmp, new_entry);

		if (++attempts > 3) {
			ret = EPERM;
			goto exit;
		}
	} // for (;;)

	cpr->cp_key_roll_ctx = ckr;
	cpr->cp_key_revision = rev;
	cpr->cp_key_os_version = cp_os_version();

	return 0;

exit:

	wakeup(&cp->c_cpentry);
	return ret;
}

// Rolls up to @up_to
errno_t hfs_key_roll_up_to(vfs_context_t vfs_ctx, vnode_t vp, off_rsrc_t up_to)
{
	cnode_t * const cp = VTOC(vp);

	for (;;) {
		hfs_lock_always(cp, HFS_SHARED_LOCK);
		cprotect_t cpr = cp->c_cpentry;
		if (!cpr)
			break;

		hfs_cp_key_roll_ctx_t *ckr = cpr->cp_key_roll_ctx;
		if (!ckr || ckr->ckr_off_rsrc >= up_to)
			break;

		hfs_unlock(cp);
		errno_t ret = hfs_key_roll_step(vfs_ctx, vp, up_to);
		if (ret)
			return ret;
	}

	hfs_unlock(cp);

	return 0;
}

/*
 * This function checks the size of the file and the key roll offset
 * and updates the xattr accordingly if necessary.  This is called
 * when a file is truncated and also just before and after key
 * rolling.  cnode must be locked exclusively and might be dropped.
 * truncate lock must be held shared or exclusive (arg indicates
 * which).  If the truncate lock is held shared, then caller must have
 * set ckr->busy.
 */
errno_t hfs_key_roll_check(cnode_t *cp, bool have_excl_trunc_lock)
{
	hfs_assert(cp->c_lockowner == current_thread());

	errno_t					 ret;
	cprotect_t				 cpr;
	hfs_cp_key_roll_ctx_t	*ckr						= NULL;
	hfsmount_t				*hfsmp						= VTOHFS(cp->c_vp);
	bool					 downgrade_trunc_lock		= false;
	off_rsrc_t				 orig_off_rsrc;

again:

	cpr = cp->c_cpentry;
	if (!cpr) {
		ret = 0;
		goto exit;
	}

	ckr = cpr->cp_key_roll_ctx;
	if (!ckr) {
		ret = 0;
		goto exit;
	}

	hfs_assert(have_excl_trunc_lock || ckr->ckr_busy);

	if (!cp->c_datafork) {
		ret = 0;
		goto exit;
	}

	orig_off_rsrc = ckr->ckr_off_rsrc;
	off_rsrc_t new_off_rsrc = orig_off_rsrc;

	if (orig_off_rsrc == INT64_MAX) {
		/*
		 * If orig_off_rsrc == INT64_MAX it means we rolled to the end and we
		 * updated the xattr, but we haven't fixed up the in memory part of it
		 * because we didn't have the truncate lock exclusively.
		 */
	} else if (off_rsrc_is_rsrc(orig_off_rsrc)) {
		off_t size;

		if (!cp->c_rsrcfork) {
			size = hfs_blk_to_bytes(cp->c_blocks - cp->c_datafork->ff_blocks,
									hfsmp->blockSize);
		} else {
			size = min(cp->c_rsrcfork->ff_size,
					   hfs_blk_to_bytes(ff_allocblocks(cp->c_rsrcfork),
										hfsmp->blockSize));
		}

		if (off_rsrc_get_off(orig_off_rsrc) >= size)
			new_off_rsrc = INT64_MAX;
	} else {
		off_t size = min(cp->c_datafork->ff_size,
						 hfs_blk_to_bytes(ff_allocblocks(cp->c_datafork),
										  hfsmp->blockSize));

		if (off_rsrc_get_off(orig_off_rsrc) >= size) {
			new_off_rsrc = hfs_has_rsrc(cp) ? off_rsrc_make(0, true) : INT64_MAX;
		}
	}

	// Should we delete roll information?
	if (new_off_rsrc == INT64_MAX) {
		/*
		 * If we're deleting the roll information, we need the truncate lock
		 * exclusively to flush out readers and sync writers and
		 * vnode_waitforwrites to flush out async writers.
		 */
		if (!have_excl_trunc_lock) {
			ckr->ckr_busy = false;
			hfs_unlock(cp);

			if (!hfs_truncate_lock_upgrade(cp))
				hfs_lock_truncate(cp, HFS_EXCLUSIVE_LOCK, 0);
			have_excl_trunc_lock = true;
			downgrade_trunc_lock = true;
			hfs_lock_always(cp, HFS_EXCLUSIVE_LOCK);

			// Things may have changed, go around again
			goto again;
		}

		// We hold the truncate lock exclusively so vnodes cannot be recycled here
		vnode_waitforwrites(cp->c_vp, 0, 0, 0, "hfs_key_roll_check_size");
		if (cp->c_rsrc_vp) {
			vnode_waitforwrites(cp->c_rsrc_vp, 0, 0,
								0, "hfs_key_roll_check_size");
		}

		// It's now safe to copy the keys and free the context
		if (!cpkp_can_copy(&ckr->ckr_keys, &cpr->cp_keys)) {
			cprotect_t new_cpr = cp_entry_alloc(cpr,
												ckr->ckr_keys.cpkp_max_pers_key_len,
												CP_MAX_CACHEBUFLEN, NULL);
			cpkp_copy(&ckr->ckr_keys, &new_cpr->cp_keys);
			cp_replace_entry(hfsmp, cp, new_cpr);
			cpr = new_cpr;
		} else {
			cpkp_copy(&ckr->ckr_keys, &cpr->cp_keys);
			hfs_release_key_roll_ctx(hfsmp, cpr);
		}
		ckr = NULL;
	}

	if (new_off_rsrc != orig_off_rsrc) {
		// Update the xattr

		if (ckr)
			ckr->ckr_off_rsrc = new_off_rsrc;
		ret = cp_setxattr(cp, cpr, hfsmp, 0, XATTR_REPLACE);
		if (ret) {
			if (ckr)
				ckr->ckr_off_rsrc = orig_off_rsrc;
			goto exit;
		}
	}

	ret = 0;

exit:

	if (downgrade_trunc_lock) {
		if (ckr)
			ckr->ckr_busy = true;
		hfs_truncate_lock_downgrade(cp);
	}

	return ret;
}

/*
 * We need to wrap the UPL routines because we will be dealing with
 * allocation blocks and the UPL routines need to be page aligned.
 */

static errno_t kr_upl_create(vnode_t vp, off_t offset, int len,
							 upl_offset_t *start_upl_offset,
							 upl_t *upl, upl_page_info_t **pl)
{
	// Round parameters to page size
	const int rounding = offset & PAGE_MASK;

	*start_upl_offset = rounding;

	offset	-= rounding;
	len		+= rounding;

	// Don't go beyond end of file
	off_t max = VTOF(vp)->ff_size - offset;
	if (len > max)
		len = max;

	len = round_page_32(len);

	return mach_to_bsd_errno(ubc_create_upl(vp, offset, len, upl, pl,
											UPL_CLEAN_IN_PLACE | UPL_SET_LITE));
}

static errno_t kr_page_out(vnode_t vp, upl_t upl, upl_offset_t upl_offset,
						   off_t fork_offset, int len)
{
	const int rounding = upl_offset & PAGE_MASK;

	upl_offset	-= rounding;
	fork_offset	-= rounding;
	len			+= rounding;

	const off_t fork_size = VTOF(vp)->ff_size;
	if (fork_offset + len > fork_size)
		len = fork_size - fork_offset;

	len = round_page_32(len);

	return cluster_pageout(vp, upl, upl_offset, fork_offset, len,
						   fork_size,  UPL_IOSYNC | UPL_NOCOMMIT);
}

static void kr_upl_commit(upl_t upl, upl_offset_t upl_offset, int len, bool last)
{
	if (!upl)
		return;

	const int rounding = upl_offset & PAGE_MASK;

	upl_offset	-= rounding;
	len			+= rounding;

	/*
	 * If not last we cannot commit the partial page yet so we round
	 * down.
	 */
	if (last)
		len = upl_get_size(upl) - upl_offset;
	else
		len = trunc_page_32(len);

	/*
	 * This should send pages that were absent onto the speculative
	 * queue and because we passed in UPL_CLEAN_IN_PLACE when we
	 * created the UPL, dirty pages will end up clean.
	 */
	__unused errno_t err
		= ubc_upl_commit_range(upl, upl_offset, len,
							   UPL_COMMIT_FREE_ON_EMPTY
							   | UPL_COMMIT_SPECULATE);

	hfs_assert(!err);

}

// Rolls 1 chunk if before @limit
errno_t hfs_key_roll_step(__unused vfs_context_t vfs_ctx, vnode_t vp, off_rsrc_t limit)
{
	int						 ret				= EIO;
	cnode_t * const			 cp					= VTOC(vp);
	cprotect_t				 cpr				= NULL;
	hfsmount_t * const		 hfsmp				= VTOHFS(vp);
	upl_t					 upl				= NULL;
	bool					 transaction_open	= false;
	bool					 need_put			= false;
	bool					 marked_busy		= false;
	hfs_cp_key_roll_ctx_t	*ckr				= NULL;
	filefork_t				*pfork				= NULL;
	off_rsrc_t				 roll_back_off_rsrc = 0;
	int						 ext_count			= 0;
	const int max_extents = 8;	// Change mask below if this changes
	enum {
		ROLL_BACK_EXTENTS_MASK		= 0x00ff,
		ROLL_BACK_XATTR				= 0x0100,
		ROLL_BACK_OFFSET			= 0x0200,
	};
	int						 roll_back			= 0;

#if 0
	printf ("{ hfs_key_roll_step\n");
#endif

	if (ISSET(hfsmp->hfs_flags, HFS_READ_ONLY))
		return EROFS;

	HFSPlusExtentDescriptor extents[max_extents];
	struct rl_entry *reservations[max_extents];

	DECL_HFS_LOCK_CTX(trunc_lock, hfs_unlock_trunc);
	DECL_HFS_LOCK_CTX(cp_lock, hfs_unlock_cp);
	DECL_HFS_LOCK_CTX(sys_lock, hfs_unlock_sys);

	for (;;) {
		hfs_lock_trunc(cp, HFS_SHARED_LOCK, &trunc_lock);
		hfs_lock_cp(cp, HFS_EXCLUSIVE_LOCK, &cp_lock);

		cpr = cp->c_cpentry;
		if (!cpr) {
			// File is not protected
			ret = 0;
			goto exit;
		}

		ckr = cpr->cp_key_roll_ctx;
		if (!ckr) {
			// Rolling was finished by something else
			ret = 0;
			goto exit;
		}

		if (!ckr->ckr_busy)
			break;

		// Something else is rolling, wait until they've finished
		assert_wait(&cp->c_cpentry, THREAD_ABORTSAFE);
		hfs_unlock_cp(&cp_lock);
		hfs_unlock_trunc(&trunc_lock);

		if (msleep(NULL, NULL, PINOD | PCATCH,
				   "hfs_key_roll", NULL) == EINTR) {
			ret = EINTR;
			goto exit;
		}
	}

	ckr->ckr_busy = true;
	marked_busy = true;

	CHECK(hfs_key_roll_check(cp, false), ret, exit);

	// hfs_key_roll_check can change things
	cpr = cp->c_cpentry;
	ckr = cpr->cp_key_roll_ctx;

	if (!ckr) {
		ret = 0;
		goto exit;
	}

	if (ckr->ckr_off_rsrc >= limit) {
		ret = 0;
		goto exit;
	}

	// Early check for no space.  We don't dip into the reserve pool.
	if (!hfs_freeblks(hfsmp, true)) {
		ret = ENOSPC;
		goto exit;
	}

	if (off_rsrc_is_rsrc(ckr->ckr_off_rsrc)) {
		if (!VNODE_IS_RSRC(vp)) {
			/*
			 * We've called hfs_key_roll_check so there's no way we should get
			 * ENOENT here.
			 */
			vnode_t rvp;
			CHECK(hfs_vgetrsrc(hfsmp, vp, &rvp), ret, exit);
			need_put = true;
			vp = rvp;
		}
		pfork = cp->c_rsrcfork;
	} else {
		if (VNODE_IS_RSRC(vp)) {
			CHECK(vnode_get(cp->c_vp), ret, exit);
			vp = cp->c_vp;
			need_put = true;
		}
		pfork = cp->c_datafork;
	}

	hfs_unlock_cp(&cp_lock);

	// Get total blocks in fork
	const uint32_t fork_blocks = min(howmany(pfork->ff_size,
											 hfsmp->blockSize),
									 ff_allocblocks(pfork));

	off_t off = off_rsrc_get_off(ckr->ckr_off_rsrc);
	hfs_assert(!(off % hfsmp->blockSize));

	uint32_t block = off / hfsmp->blockSize;

	// Figure out remaining fork blocks
	uint32_t rem_fork_blocks;
	if (fork_blocks < block)
		rem_fork_blocks = 0;
	else
		rem_fork_blocks = fork_blocks - block;

	uint32_t chunk_blocks = min(rem_fork_blocks,
								HFS_KEY_ROLL_MAX_CHUNK_BYTES / hfsmp->blockSize);

	off_t chunk_bytes = chunk_blocks * hfsmp->blockSize;
	upl_offset_t upl_offset = 0;

	if (chunk_bytes) {
		if (!ckr->ckr_preferred_next_block && off) {
			/*
			 * Here we fix up ckr_preferred_next_block.  This can
			 * happen when we rolled part of a file, then rebooted.
			 * We want to try and allocate from where we left off.
			 */
			hfs_ext_iter_t *iter;

			iter = hfs_malloc(sizeof(*iter));

			hfs_lock_sys(hfsmp, SFL_EXTENTS, HFS_EXCLUSIVE_LOCK, &sys_lock);

			// Errors are not fatal here
			if (!hfs_ext_find(vp, off - 1, iter)) {
				ckr->ckr_preferred_next_block = (iter->group[iter->ndx].startBlock
												 + off / hfsmp->blockSize
												 - iter->file_block);
			}

			hfs_unlock_sys(&sys_lock);

			hfs_free(iter, sizeof(*iter));
		}

		// We need to wait for outstanding direct reads to be issued
		cl_direct_read_lock_t *lck = cluster_lock_direct_read(vp, LCK_RW_TYPE_EXCLUSIVE);

		upl_page_info_t *pl;
		ret = kr_upl_create(vp, off, chunk_bytes, &upl_offset, &upl, &pl);

		// We have the pages locked now so it's safe to...
		cluster_unlock_direct_read(lck);

		if (ret) {
			LOG_ERROR(ret);
			goto exit;
		}

		int page_count = upl_get_size(upl) >> PAGE_SHIFT;
		int page_ndx = 0;

		// Page everything in
		for (;;) {
			while (page_ndx < page_count && upl_valid_page(pl, page_ndx))
				++page_ndx;

			if (page_ndx >= page_count)
				break;

			const int page_group_start = page_ndx;

			do {
				++page_ndx;
			} while (page_ndx < page_count && !upl_valid_page(pl, page_ndx));

			const upl_offset_t start = page_group_start << PAGE_SHIFT;

			CHECK(cluster_pagein(vp, upl, start,
								 off - upl_offset + start,
								 (page_ndx - page_group_start) << PAGE_SHIFT,
								 pfork->ff_size,
								 UPL_IOSYNC | UPL_NOCOMMIT), ret, exit);
		}
	}

	bool tried_hard = false;

	/*
	 * For each iteration of this loop, we roll up to @max_extents
	 * extents and update the metadata for those extents (one
	 * transaction per iteration.)
	 */
	for (;;) {
		/*
		 * This is the number of bytes rolled for the current
		 * iteration of the containing loop.
		 */
		off_t bytes_rolled = 0;

		roll_back_off_rsrc = ckr->ckr_off_rsrc;
		ext_count = 0;

		// Allocate and write out up to @max_extents extents
		while (chunk_bytes && ext_count < max_extents) {
			/*
			 * We're not making any on disk changes here but
			 * hfs_block_alloc needs to ask the journal about pending
			 * trims and for that it needs the journal lock and the
			 * journal lock must be taken before any system file lock.
			 * We could fix the journal code so that it can deal with
			 * this when there is no active transaction but the
			 * overhead from opening a transaction and then closing it
			 * without making any changes is actually quite small so
			 * we take that much simpler approach here.
			 */
			CHECK(hfs_start_transaction(hfsmp), ret, exit);
			transaction_open = true;

			hfs_lock_sys(hfsmp, SFL_BITMAP, HFS_EXCLUSIVE_LOCK, &sys_lock);

			HFSPlusExtentDescriptor *ext = &extents[ext_count];

			if (!tried_hard
				&& (!ckr->ckr_tentative_reservation
					|| !rl_len(ckr->ckr_tentative_reservation))) {
				hfs_free_tentative(hfsmp, &ckr->ckr_tentative_reservation);

				tried_hard = true;

				HFSPlusExtentDescriptor extent = {
					.startBlock = ckr->ckr_preferred_next_block,
					.blockCount = 1, // This is the minimum
				};

				hfs_alloc_extra_args_t args = {
					.max_blocks			= rem_fork_blocks,
					.reservation_out	= &ckr->ckr_tentative_reservation,
					.alignment			= PAGE_SIZE / hfsmp->blockSize,
					.alignment_offset	= (off + bytes_rolled) / hfsmp->blockSize,
				};

				ret = hfs_block_alloc(hfsmp, &extent,
									  HFS_ALLOC_TENTATIVE | HFS_ALLOC_TRY_HARD,
									  &args);

				if (ret == ENOSPC && ext_count) {
					ext->blockCount = 0;
					goto roll_what_we_have;
				} else if (ret) {
					if (ret != ENOSPC)
						LOG_ERROR(ret);
					goto exit;
				}
			}

			ext->startBlock = ckr->ckr_preferred_next_block;
			ext->blockCount = 1;

			hfs_alloc_extra_args_t args = {
				.max_blocks			= chunk_blocks,
				.reservation_in		= &ckr->ckr_tentative_reservation,
				.reservation_out	= &reservations[ext_count],
				.alignment			= PAGE_SIZE / hfsmp->blockSize,
				.alignment_offset	= (off + bytes_rolled) / hfsmp->blockSize,
			};

			// Lock the reservation
			ret = hfs_block_alloc(hfsmp, ext,
								  (HFS_ALLOC_USE_TENTATIVE
								   | HFS_ALLOC_LOCKED), &args);

			if (ret == ENOSPC && ext_count) {
				// We've got something we can do
				ext->blockCount = 0;
			} else if (ret) {
				if (ret != ENOSPC)
					LOG_ERROR(ret);
				goto exit;
			}

		roll_what_we_have:

			hfs_unlock_sys(&sys_lock);

			transaction_open = false;
			CHECK(hfs_end_transaction(hfsmp), ret, exit);

			if (!ext->blockCount)
				break;

			const off_t ext_bytes = hfs_blk_to_bytes(ext->blockCount,
													 hfsmp->blockSize);

			/*
			 * We set things up here so that cp_io_params can do the
			 * right thing for this extent.  Note that we have a UPL with the
			 * pages locked so we are the only thing that can do reads and
			 * writes in the region that we're rolling.  We set ckr_off_rsrc
			 * to point to the *end* of the extent.
			 */
			hfs_lock_cp(cp, HFS_EXCLUSIVE_LOCK, &cp_lock);
			ckr->ckr_off_rsrc += ext_bytes;
			roll_back |= ROLL_BACK_OFFSET;
			ckr->ckr_roll_extent = *ext;
			hfs_unlock_cp(&cp_lock);

			// Write the data out
			CHECK(kr_page_out(vp, upl, upl_offset + bytes_rolled,
							  off + bytes_rolled,
							  ext_bytes), ret, exit);

			chunk_bytes						-= ext_bytes;
			chunk_blocks					-= ext->blockCount;
			rem_fork_blocks					-= ext->blockCount;
			ckr->ckr_preferred_next_block	+= ext->blockCount;
			bytes_rolled					+= ext_bytes;
			++ext_count;
		} // while (chunk_bytes && ext_count < max_extents)

		/*
		 * We must make sure the above data hits the device before we update
		 * metadata to point to it.
		 */
		if (bytes_rolled)
			CHECK(hfs_flush(hfsmp, HFS_FLUSH_BARRIER), ret, exit);

		// Update the metadata to point at the data we just wrote

		// We'll be changing in-memory structures so we need this lock
		hfs_lock_cp(cp, HFS_EXCLUSIVE_LOCK, &cp_lock);

		CHECK(hfs_start_transaction(hfsmp), ret, exit);
		transaction_open = true;

		hfs_lock_sys(hfsmp, SFL_BITMAP, HFS_EXCLUSIVE_LOCK, &sys_lock);

		// Commit the allocations
		hfs_alloc_extra_args_t args = {};

		for (int i = 0; i < ext_count; ++i) {
			args.reservation_in = &reservations[i];

			CHECK(hfs_block_alloc(hfsmp, &extents[i],
								  HFS_ALLOC_COMMIT, &args), ret, exit);

			roll_back |= 1 << i;
		}

		hfs_unlock_sys(&sys_lock);

		// Keep the changes to the catalog extents here
		HFSPlusExtentRecord cat_extents;

		// If at the end of this chunk, fix up ckr_off_rsrc
		if (!chunk_bytes) {
			/*
			 * Are we at the end of the fork?  It's possible that
			 * blocks that were unallocated when we started rolling
			 * this chunk have now been allocated.
			 */
			off_t fork_end = min(pfork->ff_size,
								 hfs_blk_to_bytes(ff_allocblocks(pfork), hfsmp->blockSize));

			if (off + bytes_rolled >= fork_end) {
				if (!off_rsrc_is_rsrc(ckr->ckr_off_rsrc)
					&& hfs_has_rsrc(cp)) {
					ckr->ckr_off_rsrc = off_rsrc_make(0, true);
				} else {
					/*
					 * In this case, we will deal with the xattr here,
					 * but we save the freeing up of the context until
					 * hfs_key_roll_check where it can take the
					 * truncate lock exclusively.
					 */
					ckr->ckr_off_rsrc = INT64_MAX;
				}
			}
		}

		roll_back |= ROLL_BACK_XATTR;

		CHECK(cp_setxattr(cp, cpr, hfsmp, 0, XATTR_REPLACE), ret, exit);

		/*
		 * Replace the extents.  This must be last because we cannot easily
		 * roll back if anything fails after this.
		 */
		hfs_lock_sys(hfsmp, SFL_EXTENTS | SFL_CATALOG, HFS_EXCLUSIVE_LOCK, &sys_lock);
		CHECK(hfs_ext_replace(hfsmp, vp, off / hfsmp->blockSize,
							  extents, ext_count, cat_extents), ret, exit);
		hfs_unlock_sys(&sys_lock);

		transaction_open = false;
		roll_back = 0;

		CHECK(hfs_end_transaction(hfsmp), ret, exit);

		// ** N.B. We *must* not fail after here **

		// Copy the catalog extents if we changed them
		if (cat_extents[0].blockCount)
			memcpy(pfork->ff_data.cf_extents, cat_extents, sizeof(cat_extents));

		ckr->ckr_roll_extent = (HFSPlusExtentDescriptor){ 0, 0 };

		hfs_unlock_cp(&cp_lock);

		kr_upl_commit(upl, upl_offset, bytes_rolled, /* last: */ !chunk_bytes);

		if (!chunk_bytes) {
			// We're done
			break;
		}

		upl_offset += bytes_rolled;
		off        += bytes_rolled;
	} // for (;;)

	// UPL will have been freed
	upl = NULL;

	hfs_lock_cp(cp, HFS_EXCLUSIVE_LOCK, &cp_lock);

	// Ignore errors here; they shouldn't be fatal
	hfs_key_roll_check(cp, false);

	ret = 0;

exit:

	// hfs_key_roll_check can change things so update here
	cpr = cp->c_cpentry;
	ckr = cpr->cp_key_roll_ctx;

	if (roll_back & ROLL_BACK_EXTENTS_MASK) {
		if (!ISSET(hfs_lock_flags(&sys_lock), SFL_BITMAP)) {
			hfs_lock_sys(hfsmp, SFL_BITMAP, HFS_EXCLUSIVE_LOCK,
						 &sys_lock);
		}

		for (int i = 0; i < ext_count; ++i) {
			if (!ISSET(roll_back, 1 << i))
				continue;

			if (BlockDeallocate(hfsmp, extents[i].startBlock,
								extents[i].blockCount, 0)) {
				hfs_mark_inconsistent(hfsmp, HFS_ROLLBACK_FAILED);
			}
		}
	}

	hfs_unlock_sys(&sys_lock);

	if (roll_back & ROLL_BACK_XATTR) {
		hfs_assert(hfs_is_locked(&cp_lock));

		if (cp_setxattr(cp, cpr, hfsmp, 0, XATTR_REPLACE))
			hfs_mark_inconsistent(hfsmp, HFS_ROLLBACK_FAILED);
	}

	if (transaction_open)
		hfs_end_transaction(hfsmp);

	if (roll_back & ROLL_BACK_OFFSET) {
		if (!hfs_is_locked(&cp_lock))
			hfs_lock_cp(cp, HFS_EXCLUSIVE_LOCK, &cp_lock);
		ckr->ckr_off_rsrc = roll_back_off_rsrc;
		ckr->ckr_roll_extent = (HFSPlusExtentDescriptor){ 0, 0 };
	}

	if (marked_busy && ckr) {
		if (!hfs_is_locked(&cp_lock))
			hfs_lock_cp(cp, HFS_EXCLUSIVE_LOCK, &cp_lock);
		ckr->ckr_busy = false;
		wakeup(&cp->c_cpentry);
	}

	hfs_unlock_cp(&cp_lock);
	hfs_unlock_trunc(&trunc_lock);

	if (upl)
		ubc_upl_abort(upl, UPL_ABORT_FREE_ON_EMPTY);

	if (ext_count && reservations[ext_count - 1]) {
		hfs_lock_sys(hfsmp, SFL_BITMAP, HFS_EXCLUSIVE_LOCK, &sys_lock);
		for (int i = 0; i < ext_count; ++i)
			hfs_free_locked(hfsmp, &reservations[i]);
		hfs_unlock_sys(&sys_lock);
	}

	if (need_put)
		vnode_put(vp);

	if (ret == ESTALE) {
		hfs_mark_inconsistent(hfsmp, HFS_INCONSISTENCY_DETECTED);
		ret = HFS_EINCONSISTENT;
	}

#if 0
	printf ("hfs_key_roll_step }\n");
#endif

	return ret;
}

// cnode must be locked (shared at least)
bool hfs_is_key_rolling(cnode_t *cp)
{
	return (cp->c_cpentry && cp->c_cpentry->cp_key_roll_ctx
			&& cp->c_cpentry->cp_key_roll_ctx->ckr_off_rsrc != INT64_MAX);
}

#endif // CONFIG_PROTECT
