/*
 * ntfs_attr.c - NTFS kernel attribute operations.
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

#include <sys/errno.h>
#include <sys/stat.h>
#include <sys/ucred.h>
#include <sys/ubc.h>

#include <string.h>

#include <libkern/libkern.h>
#include <libkern/OSMalloc.h>

#include <kern/debug.h>
#include <kern/sched_prim.h>

#include "ntfs.h"
#include "ntfs_attr.h"
#include "ntfs_attr_list.h"
#include "ntfs_debug.h"
#include "ntfs_dir.h"
#include "ntfs_endian.h"
#include "ntfs_index.h"
#include "ntfs_inode.h"
#include "ntfs_layout.h"
#include "ntfs_lcnalloc.h"
#include "ntfs_mft.h"
#include "ntfs_page.h"
#include "ntfs_runlist.h"
#include "ntfs_time.h"
#include "ntfs_types.h"
#include "ntfs_unistr.h"

ntfschar AT_UNNAMED[1] = { 0 };

/**
 * ntfs_attr_map_runlist - map the whole runlist of an ntfs inode
 * @ni:		ntfs inode for which to map the whole runlist
 *
 * Map the whole runlist of the ntfs inode @ni.
 *
 * Return 0 on success and errno on error.
 *
 * Note this function requires the runlist not to be mapped yet at all.  This
 * limitation is ok because we only use this function at mount time to map the
 * runlist of some system files thus we are guaranteed that they will not have
 * any runlist fragments mapped yet.
 *
 * Note the runlist can be NULL after this function returns if the attribute
 * has zero allocated size, i.e. there simply is no runlist.
 */
errno_t ntfs_attr_map_runlist(ntfs_inode *ni)
{
	VCN vcn, end_vcn;
	ntfs_inode *base_ni;
	MFT_RECORD *m;
	ntfs_attr_search_ctx *ctx;
	ATTR_RECORD *a;
	errno_t err = 0;

	ntfs_debug("Entering for mft_no 0x%llx, type 0x%x.",
			(unsigned long long)ni->mft_no,
			(unsigned)le32_to_cpu(ni->type));
	/* If the attribute is resident there is nothing to do. */
	if (!NInoNonResident(ni)) {
		ntfs_debug("Done (resident, nothing to do).");
		return 0;
	}
	lck_rw_lock_exclusive(&ni->rl.lock);
	/* Verify that the runlist is not mapped yet. */
	if (ni->rl.alloc && ni->rl.elements)
		panic("%s(): ni->rl.alloc && ni->rl.elements\n", __FUNCTION__);
	base_ni = ni;
	if (NInoAttr(ni))
		base_ni = ni->base_ni;
	err = ntfs_mft_record_map(base_ni, &m);
	if (err)
		goto err;
	ctx = ntfs_attr_search_ctx_get(base_ni, m);
	if (!ctx) {
		err = ENOMEM;
		goto unm_err;
	}
	vcn = 0;
	end_vcn = ni->allocated_size >> ni->vol->cluster_size_shift;
	do {
		err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len, vcn,
				NULL, 0, ctx);
		if (err) {
			if (err == ENOENT)
				err = EIO;
			break;
		}
		a = ctx->a;
		if (!a->non_resident) {
corrupt_err:
			ntfs_error(ni->vol->mp, "Inode 0x%llx contains corrupt "
					"attribute extent, run chkdsk.",
					(unsigned long long)base_ni->mft_no);
			NVolSetErrors(ni->vol);
			err = EIO;
			break;
		}
		/*
		 * If we are in the first attribute extent, verify the cached
		 * allocated size is correct.
		 */
		if (!a->lowest_vcn)
			if (sle64_to_cpu(a->allocated_size) !=
					ni->allocated_size)
				panic("%s(): sle64_to_cpu(a->allocated_size) "
						"!= ni->allocated_size\n",
						__FUNCTION__);
		/*
		 * Sanity check the lowest_vcn of the attribute is equal to the
		 * vcn we looked up and that the highest_vcn of the attribute
		 * is above the current vcn.
		 */
		if (sle64_to_cpu(a->lowest_vcn) != vcn || (vcn &&
				sle64_to_cpu(a->highest_vcn) < vcn))
			goto corrupt_err;
		/* Determine the next vcn. */
		vcn = sle64_to_cpu(a->highest_vcn) + 1;
		/*
		 * Finally, map the runlist fragment contained in this
		 * attribute extent.
		 */
		err = ntfs_mapping_pairs_decompress(ni->vol, a, &ni->rl);
	} while (!err && vcn < end_vcn);
unm_err:
	ntfs_attr_search_ctx_put(ctx);
	ntfs_mft_record_unmap(base_ni);
err:
	lck_rw_unlock_exclusive(&ni->rl.lock);
	if (!err)
		ntfs_debug("Done.");
	else
		ntfs_error(ni->vol->mp, "Failed (error %d).", (int)err);
	return err;
}

/**
 * ntfs_map_runlist_nolock - map (a part of) a runlist of an ntfs inode
 * @ni:		ntfs inode for which to map (part of) a runlist
 * @vcn:	map runlist part containing this vcn
 * @ctx:	active attribute search context if present or NULL if not
 *
 * Map the part of a runlist containing the @vcn of the ntfs inode @ni.
 *
 * If @ctx is specified, it is an active search context of @ni and its base mft
 * record.  This is needed when ntfs_map_runlist_nolock() encounters unmapped
 * runlist fragments and allows their mapping.  If you do not have the mft
 * record mapped, you can specify @ctx as NULL and ntfs_map_runlist_nolock()
 * will perform the necessary mapping and unmapping.
 *
 * Note, ntfs_map_runlist_nolock() saves the state of @ctx on entry and
 * restores it before returning.  Thus, @ctx will be left pointing to the same
 * attribute on return as on entry.  However, the actual pointers in @ctx may
 * point to different memory locations on return, so you must remember to reset
 * any cached pointers from the @ctx, i.e. after the call to
 * ntfs_map_runlist_nolock(), you will probably want to do:
 *	m = ctx->m;
 *	a = ctx->a;
 * Assuming you cache ctx->a in a variable @a of type ATTR_RECORD * and that
 * you cache ctx->m in a variable @m of type MFT_RECORD *.
 *
 * Return 0 on success and errno on error.  There is one special error code
 * which is not an error as such.  This is ENOENT.  It means that @vcn is out
 * of bounds of the runlist.
 *
 * Note the runlist can be NULL after this function returns if @vcn is zero and
 * the attribute has zero allocated size, i.e. there simply is no runlist.
 *
 * WARNING: If @ctx is supplied, regardless of whether success or failure is
 *	    returned, you need to check @ctx->is_error and if 1 the @ctx is no
 *	    longer valid, i.e. you need to either call
 *	    ntfs_attr_search_ctx_reinit() or ntfs_attr_search_ctx_put() on it.
 *	    In that case @ctx->error will give you the error code for why the
 *	    mapping of the old inode failed.
 *	    Also if @ctx is supplied and the current attribute (or the mft
 *	    record it is in) has been modified then the caller must call
 *	    NInoSetMrecNeedsDirtying(ctx->ni); before calling
 *	    ntfs_map_runlist_nolock() or the changes may be lost.
 *
 * Locking: - The runlist described by @ni must be locked for writing on entry
 *	      and is locked on return.  Note the runlist will be modified.
 *	    - If @ctx is NULL, the base mft record of @ni must not be mapped on
 *	      entry and it will be left unmapped on return.
 *	    - If @ctx is not NULL, the base mft record must be mapped on entry
 *	      and it will be left mapped on return.
 */
errno_t ntfs_map_runlist_nolock(ntfs_inode *ni, VCN vcn,
		ntfs_attr_search_ctx *ctx)
{
	VCN end_vcn;
	ntfs_inode *base_ni;
	MFT_RECORD *m;
	ATTR_RECORD *a;
	errno_t err = 0;
	BOOL ctx_is_temporary, ctx_needs_reset;
	ntfs_attr_search_ctx old_ctx = { { NULL, }, };

	ntfs_debug("Entering for mft_no 0x%llx, vcn 0x%llx.",
			(unsigned long long)ni->mft_no,
			(unsigned long long)vcn);
	base_ni = ni;
	if (NInoAttr(ni))
		base_ni = ni->base_ni;
	if (!ctx) {
		ctx_is_temporary = ctx_needs_reset = TRUE;
		err = ntfs_mft_record_map(base_ni, &m);
		if (err)
			goto done;
		ctx = ntfs_attr_search_ctx_get(base_ni, m);
		if (!ctx) {
			err = ENOMEM;
			goto err;
		}
	} else {
		VCN allocated_size_vcn;

		if (ctx->is_error)
			panic("%s(): ctx->is_error\n", __FUNCTION__);
		a = ctx->a;
		if (!a->non_resident)
			panic("%s(): !a->non_resident\n", __FUNCTION__);
		ctx_is_temporary = FALSE;
		end_vcn = sle64_to_cpu(a->highest_vcn);
		lck_spin_lock(&ni->size_lock);
		allocated_size_vcn = ni->allocated_size >>
				ni->vol->cluster_size_shift;
		lck_spin_unlock(&ni->size_lock);
		/*
		 * If we already have the attribute extent containing @vcn in
		 * @ctx, no need to look it up again.  We slightly cheat in
		 * that if vcn exceeds the allocated size, we will refuse to
		 * map the runlist below, so there is definitely no need to get
		 * the right attribute extent.
		 */
		if (vcn >= allocated_size_vcn || (a->type == ni->type &&
				a->name_length == ni->name_len &&
				!bcmp((u8*)a + le16_to_cpu(a->name_offset),
				ni->name, ni->name_len) &&
				sle64_to_cpu(a->lowest_vcn) <= vcn &&
				end_vcn >= vcn))
			ctx_needs_reset = FALSE;
		else {
			/* Save the old search context. */
			old_ctx = *ctx;
			/*
			 * Reinitialize the search context so we can lookup the
			 * needed attribute extent.
			 */
			ntfs_attr_search_ctx_reinit(ctx);
			ctx_needs_reset = TRUE;
		}
	}
	if (ctx_needs_reset) {
		err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len, vcn,
				NULL, 0, ctx);
		if (err) {
			if (err == ENOENT)
				err = EIO;
			goto err;
		}
		if (!ctx->a->non_resident)
			panic("%s(): !a->non_resident!\n", __FUNCTION__);
	}
	a = ctx->a;
	/*
	 * Only decompress the mapping pairs if @vcn is inside it.  Otherwise
	 * we get into problems when we try to map an out of bounds vcn because
	 * we then try to map the already mapped runlist fragment and
	 * ntfs_mapping_pairs_decompress() fails.
	 */
	end_vcn = sle64_to_cpu(a->highest_vcn) + 1;
	if (vcn && vcn >= end_vcn) {
		err = ENOENT;
		goto err;
	}
	err = ntfs_mapping_pairs_decompress(ni->vol, a, &ni->rl);
err:
	if (ctx_is_temporary) {
		if (ctx)
			ntfs_attr_search_ctx_put(ctx);
		ntfs_mft_record_unmap(base_ni);
	} else if (ctx_needs_reset) {
		/*
		 * If there is no attribute list, restoring the search context
		 * is acomplished simply by copying the saved context back over
		 * the caller supplied context.  If there is an attribute list,
		 * things are more complicated as we need to deal with mapping
		 * of mft records and resulting potential changes in pointers.
		 */
		if (NInoAttrList(base_ni)) {
			/*
			 * If the currently mapped (extent) inode is not the
			 * one we had before, we need to unmap it and map the
			 * old one.
			 */
			if (ctx->ni != old_ctx.ni) {
				/*
				 * If the currently mapped inode is not the
				 * base inode, unmap it.
				 */
				if (ctx->base_ni && ctx->ni != ctx->base_ni) {
					ntfs_extent_mft_record_unmap(ctx->ni);
					ctx->m = ctx->base_m;
					if (!ctx->m)
						panic("%s(): !ctx->m\n",
								__FUNCTION__);
				}
				/*
				 * If the old mapped inode is not the base
				 * inode, map it.
				 */
				if (old_ctx.base_ni && old_ctx.ni !=
						old_ctx.base_ni) {
					errno_t err2;
retry_map:
					err2 = ntfs_mft_record_map(old_ctx.ni,
							&ctx->m);
					/*
					 * Something bad has happened.  If out
					 * of memory retry till it succeeds.
					 * Any other errors are fatal and we
					 * return the error code in ctx->m.
					 * Let the caller deal with it...  We
					 * just need to fudge things so the
					 * caller can reinit and/or put the
					 * search context safely.
					 */
					if (err2) {
						if (err2 == ENOMEM) {
							(void)thread_block(
							THREAD_CONTINUE_NULL);
							goto retry_map;
						}
						ctx->is_error = 1;
						ctx->error = err2;
						old_ctx.ni = old_ctx.base_ni;
					}
				}
			}
			if (ctx->is_error) {
				old_ctx.is_error = 1;
				old_ctx.error = ctx->error;
			} else if (ctx->m != old_ctx.m) {
				/*
				 * Update the changed pointers in the saved
				 * context.
				 */
				old_ctx.a = (ATTR_RECORD*)((u8*)ctx->m +
						((u8*)old_ctx.a -
						(u8*)old_ctx.m));
				old_ctx.m = ctx->m;
			}
		}
		/* Restore the search context to the saved one. */
		*ctx = old_ctx;
	}
done:
	ntfs_debug("Done (error %d).", (int)err);
	return err;
}

/**
 * ntfs_attr_vcn_to_lcn_nolock - convert a vcn into a lcn given an ntfs inode
 * @ni:			ntfs inode of the attribute whose runlist to search
 * @vcn:		vcn to convert
 * @write_locked:	true if the runlist is locked for writing
 * @clusters:		optional destination for number of contiguous clusters
 *
 * Find the virtual cluster number @vcn in the runlist of the ntfs attribute
 * described by the ntfs inode @ni and return the corresponding logical cluster
 * number (lcn).
 *
 * If the @vcn is not mapped yet, the attempt is made to map the attribute
 * extent containing the @vcn and the vcn to lcn conversion is retried.
 *
 * If @write_locked is true the caller has locked the runlist for writing and
 * if false for reading.
 *
 * If @clusters is not NULL, on success (i.e. we return >= LCN_HOLE) we return
 * the number of contiguous clusters after the returned lcn in *@clusters.
 *
 * Since lcns must be >= 0, we use negative return codes with special meaning:
 *
 * Return code	Meaning / Description
 * ==========================================
 *  LCN_HOLE	Hole / not allocated on disk.
 *  LCN_ENOENT	There is no such vcn in the runlist, i.e. @vcn is out of bounds.
 *  LCN_ENOMEM	Not enough memory to map runlist.
 *  LCN_EIO	Critical error (runlist/file is corrupt, i/o error, etc).
 *
 * Locking: - The runlist must be locked on entry and is left locked on return.
 *	    - If @write_locked is FALSE, i.e. the runlist is locked for reading,
 *	      the lock may be dropped inside the function so you cannot rely on
 *	      the runlist still being the same when this function returns.
 */
LCN ntfs_attr_vcn_to_lcn_nolock(ntfs_inode *ni, const VCN vcn,
		const BOOL write_locked, s64 *clusters)
{
	LCN lcn;
	BOOL need_lock_switch = FALSE;
	BOOL is_retry = FALSE;

	ntfs_debug("Entering for mft_no 0x%llx, vcn 0x%llx, %s_locked.",
			(unsigned long long)ni->mft_no,
			(unsigned long long)vcn,
			write_locked ? "write" : "read");
	if (!NInoNonResident(ni))
		panic("%s(): !NInoNonResident(ni)\n", __FUNCTION__);
	if (vcn < 0)
		panic("%s(): vcn < 0\n", __FUNCTION__);
retry_remap:
	if (!ni->rl.elements) {
		lck_spin_lock(&ni->size_lock);
		if (!ni->allocated_size) {
			lck_spin_unlock(&ni->size_lock);
			lcn = LCN_ENOENT;
			goto lcn_enoent;
		}
		lck_spin_unlock(&ni->size_lock);
		if (!is_retry)
			goto try_to_map;
		lcn = LCN_EIO;
		goto lcn_eio;
	}
	/* Convert vcn to lcn.  If that fails map the runlist and retry once. */
	lcn = ntfs_rl_vcn_to_lcn(ni->rl.rl, vcn, clusters);
	if (lcn >= LCN_HOLE) {
		if (need_lock_switch)
			lck_rw_lock_exclusive_to_shared(&ni->rl.lock);
		ntfs_debug("Done (lcn 0x%llx, clusters 0x%llx).",
				(unsigned long long)lcn,
				clusters ? (unsigned long long)*clusters : 0);
		return lcn;
	}
	if (lcn != LCN_RL_NOT_MAPPED) {
		if (lcn != LCN_ENOENT)
			lcn = LCN_EIO;
	} else if (!is_retry) {
		errno_t err;

try_to_map:
		if (!write_locked && !need_lock_switch) {
			need_lock_switch = TRUE;
			/*
			 * If converting the lock from shared to exclusive
			 * fails, need to take the lock for writing and retry
			 * in case the racing process did the mapping for us.
			 */
			if (!lck_rw_lock_shared_to_exclusive(&ni->rl.lock)) {
				lck_rw_lock_exclusive(&ni->rl.lock);
				goto retry_remap;
			}
		}
		err = ntfs_map_runlist_nolock(ni, vcn, NULL);
		if (!err) {
			is_retry = TRUE;
			goto retry_remap;
		}
		switch (err) {
		case ENOENT:
			lcn = LCN_ENOENT;
			break;
		case ENOMEM:
			lcn = LCN_ENOMEM;
			break;
		default:
			lcn = LCN_EIO;
		}
	}
lcn_eio:
	if (need_lock_switch)
		lck_rw_lock_exclusive_to_shared(&ni->rl.lock);
	if (lcn == LCN_ENOENT) {
lcn_enoent:
		ntfs_debug("Done (LCN_ENOENT).");
	} else
		ntfs_error(ni->vol->mp, "Failed (error %lld).", (long long)lcn);
	return lcn;
}

/**
 * ntfs_attr_find_vcn_nolock - find a vcn in the runlist of an ntfs inode
 * @ni:		ntfs inode of the attribute whose runlist to search
 * @vcn:	vcn to find
 * @run:	return pointer for the found runlist element
 * @ctx:	active attribute search context if present or NULL if not
 *
 * Find the virtual cluster number @vcn in the runlist of the ntfs attribute
 * described by the ntfs inode @ni and return the address of the runlist
 * element containing the @vcn in *@run.
 *
 * If the @vcn is not mapped yet, the attempt is made to map the attribute
 * extent containing the @vcn and the vcn to lcn conversion is retried.
 *
 * If @ctx is specified, it is an active search context of @ni and its base mft
 * record.  This is needed when ntfs_attr_find_vcn_nolock() encounters unmapped
 * runlist fragments and allows their mapping.  If you do not have the mft
 * record mapped, you can specify @ctx as NULL and ntfs_attr_find_vcn_nolock()
 * will perform the necessary mapping and unmapping.
 *
 * Note, ntfs_attr_find_vcn_nolock() saves the state of @ctx on entry and
 * restores it before returning.  Thus, @ctx will be left pointing to the same
 * attribute on return as on entry.  However, the actual pointers in @ctx may
 * point to different memory locations on return, so you must remember to reset
 * any cached pointers from the @ctx, i.e. after the call to
 * ntfs_attr_find_vcn_nolock(), you will probably want to do:
 *	m = ctx->m;
 *	a = ctx->a;
 * Assuming you cache ctx->a in a variable @a of type ATTR_RECORD * and that
 * you cache ctx->m in a variable @m of type MFT_RECORD *.
 * Note you need to distinguish between the lcn of the returned runlist element
 * being >= 0 and LCN_HOLE.  In the later case you have to return zeroes on
 * read and allocate clusters on write.
 *
 * Return 0 on success and errno on error.
 *
 * The possible error return codes are:
 *	ENOENT	- No such vcn in the runlist, i.e. @vcn is out of bounds.
 *	ENOMEM	- Not enough memory to map runlist.
 *	EIO	- Critical error (runlist/file is corrupt, i/o error, etc).
 *
 * WARNING: If @ctx is supplied, regardless of whether success or failure is
 *	    returned, you need to check @ctx->is_error and if 1 the @ctx is no
 *	    longer valid, i.e. you need to either call
 *	    ntfs_attr_search_ctx_reinit() or ntfs_attr_search_ctx_put() on it.
 *	    In that case @ctx->error will give you the error code for why the
 *	    mapping of the old inode failed.
 *	    Also if @ctx is supplied and the current attribute (or the mft
 *	    record it is in) has been modified then the caller must call
 *	    NInoSetMrecNeedsDirtying(ctx->ni); before calling
 *	    ntfs_map_runlist_nolock() or the changes may be lost.
 *
 * Locking: - The runlist described by @ni must be locked for writing on entry
 *	      and is locked on return.  Note the runlist may be modified when
 *	      needed runlist fragments need to be mapped.
 *	    - If @ctx is NULL, the base mft record of @ni must not be mapped on
 *	      entry and it will be left unmapped on return.
 *	    - If @ctx is not NULL, the base mft record must be mapped on entry
 *	      and it will be left mapped on return.
 */
errno_t ntfs_attr_find_vcn_nolock(ntfs_inode *ni, const VCN vcn,
		ntfs_rl_element **run, ntfs_attr_search_ctx *ctx)
{
	ntfs_rl_element *rl;
	errno_t err = 0;
	BOOL is_retry = FALSE;

	ntfs_debug("Entering for mft_no 0x%llx, vcn 0x%llx, with%s ctx.",
			(unsigned long long)ni->mft_no,
			(unsigned long long)vcn, ctx ? "" : "out");
	if (!NInoNonResident(ni))
		panic("%s(): !NInoNonResident(ni)\n", __FUNCTION__);
	if (vcn < 0)
		panic("%s(): vcn < 0\n", __FUNCTION__);
retry_remap:
	if (!ni->rl.elements) {
		lck_spin_lock(&ni->size_lock);
		if (!ni->allocated_size) {
			lck_spin_unlock(&ni->size_lock);
			return LCN_ENOENT;
		}
		lck_spin_unlock(&ni->size_lock);
		if (!is_retry)
			goto try_to_map;
		err = EIO;
		goto err;
	}
	rl = ni->rl.rl;
	if (vcn >= rl[0].vcn) {
		while (rl->length) {
			if (vcn < rl[1].vcn) {
				if (rl->lcn >= LCN_HOLE) {
					ntfs_debug("Done.");
					*run = rl;
					return 0;
				}
				break;
			}
			rl++;
		}
		if (rl->lcn != LCN_RL_NOT_MAPPED) {
			if (rl->lcn == LCN_ENOENT)
				err = ENOENT;
			else
				err = EIO;
		}
	}
	if (!err && !is_retry) {
		/*
		 * If the search context is invalid we cannot map the unmapped
		 * region.
		 */
		if (ctx->is_error)
			err = ctx->error;
		else {
try_to_map:
			/*
			 * The @vcn is in an unmapped region, map the runlist
			 * and retry.
			 */
			err = ntfs_map_runlist_nolock(ni, vcn, ctx);
			if (!err) {
				is_retry = TRUE;
				goto retry_remap;
			}
		}
		if (err == EINVAL)
			err = EIO;
	} else if (!err)
		err = EIO;
err:
	if (err != ENOENT)
		ntfs_error(ni->vol->mp, "Failed (error %d).", err);
	return err;
}

/**
 * ntfs_attr_search_ctx_reinit - reinitialize an attribute search context
 * @ctx:	attribute search context to reinitialize
 *
 * Reinitialize the attribute search context @ctx, unmapping an associated
 * extent mft record if present, and initialize the search context again.
 *
 * This is used when a search for a new attribute is being started to reset
 * the search context to the beginning.
 *
 * Note: We preserve the content of @ctx->is_mft_locked so that reinitializing
 * a search context can also be done when dealing with the mft itself.
 */
void ntfs_attr_search_ctx_reinit(ntfs_attr_search_ctx *ctx)
{
	const BOOL mft_is_locked = ctx->is_mft_locked;

	if (!ctx->base_ni) {
		/* No attribute list. */
		ctx->is_first = 1;
		ctx->is_iteration = 0;
		/* Sanity checks are performed elsewhere. */
		ctx->a = (ATTR_RECORD*)((u8*)ctx->m +
				le16_to_cpu(ctx->m->attrs_offset));
		/*
		 * This needs resetting due to
		 * ntfs_attr_find_in_attribute_list() which can leave it set
		 * despite having zeroed ctx->base_ni.
		 */
		ctx->al_entry = NULL;
		return;
	}
	/* Attribute list. */
	if (ctx->ni != ctx->base_ni)
		ntfs_extent_mft_record_unmap(ctx->ni);
	ntfs_attr_search_ctx_init(ctx, ctx->base_ni, ctx->base_m);
	if (mft_is_locked)
		ctx->is_mft_locked = 1;
}

/**
 * ntfs_attr_search_ctx_get - allocate and init a new attribute search context
 * @ni:		ntfs inode with which to initialize the search context
 * @m:		mft record with which to initialize the search context
 *
 * Allocate a new attribute search context, initialize it with @ni and @m, and
 * return it.  Return NULL if allocation failed.
 */
ntfs_attr_search_ctx *ntfs_attr_search_ctx_get(ntfs_inode *ni, MFT_RECORD *m)
{
	ntfs_attr_search_ctx *ctx;

	ctx = OSMalloc(sizeof(ntfs_attr_search_ctx), ntfs_malloc_tag);
	if (ctx)
		ntfs_attr_search_ctx_init(ctx, ni, m);
	return ctx;
}

/**
 * ntfs_attr_search_ctx_put - release an attribute search context
 * @ctx:	attribute search context to free
 *
 * Release the attribute search context @ctx, unmapping an associated extent
 * mft record if present.
 */
void ntfs_attr_search_ctx_put(ntfs_attr_search_ctx *ctx)
{
	if (ctx->base_ni && ctx->ni != ctx->base_ni)
		ntfs_extent_mft_record_unmap(ctx->ni);
	OSFree(ctx, sizeof(ntfs_attr_search_ctx), ntfs_malloc_tag);
}

/**
 * ntfs_attr_find_in_mft_record - find (next) attribute in mft record
 * @type:	attribute type to find
 * @name:	attribute name to find (optional, i.e. NULL means do not care)
 * @name_len:	attribute name length (only needed if @name present)
 * @val:	attribute value to find (optional, resident attributes only)
 * @val_len:	attribute value length (only needed if @val present)
 * @ctx:	search context with mft record and attribute to search from
 *
 * You should not need to call this function directly.  Use ntfs_attr_lookup()
 * instead.
 *
 * ntfs_attr_find_in_mft_record() takes a search context @ctx as parameter and
 * searches the mft record specified by @ctx->m, beginning at @ctx->a, for an
 * attribute of @type, optionally @name and @val.
 *
 * If the attribute is found, ntfs_attr_find_in_mft_record() returns 0 and
 * @ctx->a is set to point to the found attribute.
 *
 * If the attribute is not found, ENOENT is returned and @ctx->a is set to
 * point to the attribute before which the attribute being searched for would
 * need to be inserted if such an action were to be desired.
 *
 * On actual error, ntfs_attr_find_in_mft_record() returns EIO.  In this case
 * @ctx->a is undefined and in particular do not rely on it not having changed.
 *
 * If @ctx->is_first is 1, the search begins with @ctx->a itself.  If it is 0,
 * the search begins after @ctx->a.
 *
 * If @ctx->is_iteration is 1 and @type is AT_UNUSED this is not a search but
 * an iteration in which case each attribute in the mft record is returned in
 * turn with each call to ntfs_attr_find_in_mft_record().  Note all attributes
 * are returned including the attribute list attribute, unlike when
 * @ctx->is_iteration is 0 when it is not returned unless it is specifically
 * looked for.
 *
 * Similarly to the above, when @ctx->is_iterations is 1 and @type is not
 * AT_UNUSED all attributes of type @type are returned one after the other.
 *
 * If @name is AT_UNNAMED search for an unnamed attribute.  If @name is present
 * but not AT_UNNAMED search for a named attribute matching @name.  Otherwise,
 * match both named and unnamed attributes.
 *
 * Finally, the resident attribute value @val is looked for, if present.  If
 * @val is not present (NULL), @val_len is ignored.
 *
 * ntfs_attr_find_in_mft_record() only searches the specified mft record and it
 * ignores the presence of an attribute list attribute (unless it is the one
 * being searched for, obviously).  If you need to take attribute lists into
 * consideration, use ntfs_attr_lookup() instead (see below).  This also means
 * that you cannot use ntfs_attr_find_in_mft_record() to search for extent
 * records of non-resident attributes, as extents with lowest_vcn != 0 are
 * usually described by the attribute list attribute only.  Note that it is
 * possible that the first extent is only in the attribute list while the last
 * extent is in the base mft record, so do not rely on being able to find the
 * first extent in the base mft record.
 *
 * Warning: Never use @val when looking for attribute types which can be
 *	    non-resident as this most likely will result in a crash!
 *
 * Note if the volume is mounted case sensitive we treat attribute names as
 * being case sensitive and vice versa if the volume is not mounted case
 * sensitive we treat attribute names as being case insensitive also.
 */
errno_t ntfs_attr_find_in_mft_record(const ATTR_TYPE type,
		const ntfschar *name, const u32 name_len,
		const void *val, const u32 val_len, ntfs_attr_search_ctx *ctx)
{
	ATTR_RECORD *a;
	ntfs_volume *vol = ctx->ni->vol;
	const ntfschar *upcase = vol->upcase;
	const u32 upcase_len = vol->upcase_len;
	const BOOL case_sensitive = NVolCaseSensitive(vol);
	const BOOL is_iteration = ctx->is_iteration;

	/*
	 * Iterate over attributes in mft record starting at @ctx->a, or the
	 * attribute following that, if @ctx->is_first is true.
	 */
	if (ctx->is_first) {
		a = ctx->a;
		ctx->is_first = 0;
	} else
		a = (ATTR_RECORD*)((u8*)ctx->a + le32_to_cpu(ctx->a->length));
	for (;;	a = (ATTR_RECORD*)((u8*)a + le32_to_cpu(a->length))) {
		if ((u8*)a < (u8*)ctx->m || (u8*)a > (u8*)ctx->m +
				le32_to_cpu(ctx->m->bytes_allocated))
			break;
		ctx->a = a;
		if (((!is_iteration || type != AT_UNUSED) &&
				le32_to_cpu(a->type) > le32_to_cpu(type)) ||
				a->type == AT_END)
			return ENOENT;
		if (!a->length)
			break;
		if (is_iteration) {
			if (type == AT_UNUSED || type == a->type)
				return 0;
		}
		if (a->type != type)
			continue;
		/*
		 * If @name is AT_UNNAMED we want an unnamed attribute.
		 * If @name is present, compare the two names.
		 * Otherwise, match any attribute.
		 */
		if (name == AT_UNNAMED) {
			/* The search failed if the found attribute is named. */
			if (a->name_length)
				return ENOENT;
		} else if (name) {
			unsigned len, ofs;

			len = a->name_length;
			ofs = le16_to_cpu(a->name_offset);
			if (ofs + (len * sizeof(ntfschar)) >
					le32_to_cpu(a->length))
				break;
			if (!ntfs_are_names_equal(name, name_len,
					(ntfschar*)((u8*)a + ofs), len,
					case_sensitive, upcase, upcase_len)) {
				int rc;

				rc = ntfs_collate_names(name, name_len,
						(ntfschar*)((u8*)a + ofs), len,
						1, FALSE, upcase, upcase_len);
				/*
				 * If @name collates before a->name, there is
				 * no matching attribute.
				 */
				if (rc == -1)
					return ENOENT;
				/*
				 * If the strings are not equal, continue
				 * searching.
				 */
				if (rc)
					continue;
				rc = ntfs_collate_names(name, name_len,
						(ntfschar*)((u8*)a + ofs), len,
						1, TRUE, upcase, upcase_len);
				if (rc == -1)
					return ENOENT;
				if (rc)
					continue;
			}
		}
		/*
		 * The names match or @name not present and attribute is
		 * unnamed.  If no @val specified, we have found the attribute
		 * and are done.
		 */
		if (!val)
			return 0;
		/* @val is present; compare values. */
		else {
			unsigned len, ofs;
			int rc;

			len = le32_to_cpu(a->value_length);
			ofs = le16_to_cpu(a->value_offset);
			if (ofs + len > le32_to_cpu(a->length))
				break;
			rc = memcmp(val, (u8*)a + ofs,
					len <= val_len ? len : val_len);
			/*
			 * If @val collates before the value of the current
			 * attribute, there is no matching attribute.
			 */
			if (!rc) {
				if (val_len == len)
					return 0;
				if (val_len < len)
					return ENOENT;
			} else if (rc < 0)
				return ENOENT;
		}
	}
	ntfs_error(vol->mp, "Inode is corrupt.  Run chkdsk.");
	NVolSetErrors(vol);
	return EIO;
}

/**
 * ntfs_attr_find_in_attribute_list - find an attribute in the attribute list
 * @type:	attribute type to find
 * @name:	attribute name to find (optional, i.e. NULL means do not care)
 * @name_len:	attribute name length (only needed if @name present)
 * @lowest_vcn:	lowest vcn to find (optional, non-resident attributes only)
 * @val:	attribute value to find (optional, resident attributes only)
 * @val_len:	attribute value length (only needed if @val present)
 * @ctx:	search context with mft record and attribute to search from
 *
 * You should not need to call this function directly.  Use ntfs_attr_lookup()
 * instead.
 *
 * Find an attribute by searching the attribute list for the corresponding
 * attribute list entry.  Having found the entry, map the mft record if the
 * attribute is in a different mft record/inode, ntfs_attr_find_in_mft_record()
 * the attribute in there and return it.
 *
 * On first search @ctx->ni must be the base mft record and @ctx must have been
 * obtained from a call to ntfs_attr_search_ctx_get().  On subsequent calls
 * @ctx->ni can be any extent inode, too (@ctx->base_ni is then the base
 * inode).
 *
 * After finishing with the attribute/mft record you need to call
 * ntfs_attr_search_ctx_put() to clean up the search context (unmapping any
 * mapped mft records, etc).
 *
 * If the attribute is found, ntfs_attr_find_in_attribute_list() returns 0 and
 * @ctx->a is set to point to the found attribute.  @ctx->m is set to point to
 * the mft record in which @ctx->a is located and @ctx->al_entry is set to
 * point to the attribute list entry for the attribute.
 *
 * If the attribute is not found, ENOENT is returned and @ctx->a is set to
 * point to the attribute in the base mft record before which the attribute
 * being searched for would need to be inserted if such an action were to be
 * desired.  @ctx->m is set to point to the mft record in which @ctx->a is
 * located, i.e. the base mft record, and @ctx->al_entry is set to point to the
 * attribute list entry of the attribute before which the attribute being
 * searched for would need to be inserted if such an action were to be desired.
 *
 * Thus to insert the not found attribute, one wants to add the attribute to
 * @ctx->m (the base mft record) and if there is not enough space, the
 * attribute should be placed in a newly allocated extent mft record.  The
 * attribute list entry for the inserted attribute should be inserted in the
 * attribute list attribute at @ctx->al_entry.
 *
 * On actual error, ntfs_attr_find_in_attribute_list() returns EIO.  In this
 * case @ctx->a is undefined and in particular do not rely on it not having
 * changed.
 *
 * If @ctx->is_first is 1, the search begins with @ctx->a itself.  If it is 0,
 * the search begins after @ctx->a.
 *
 * If @name is AT_UNNAMED search for an unnamed attribute.  If @name is present
 * but not AT_UNNAMED search for a named attribute matching @name.  Otherwise,
 * match both named and unnamed attributes.
 *
 * Finally, the resident attribute value @val is looked for, if present.  If
 * @val is not present (NULL), @val_len is ignored.
 *
 * Warning: Never use @val when looking for attribute types which can be
 *	    non-resident as this most likely will result in a crash!
 */
static errno_t ntfs_attr_find_in_attribute_list(const ATTR_TYPE type,
		const ntfschar *name, const u32 name_len, const VCN lowest_vcn,
		const void *val, const u32 val_len, ntfs_attr_search_ctx *ctx)
{
	ntfs_inode *base_ni, *ni = ctx->ni;
	ntfs_volume *vol = ni->vol;
	ATTR_LIST_ENTRY *al_entry, *next_al_entry;
	u8 *al_start, *al_end;
	ATTR_RECORD *a;
	ntfschar *al_name;
	const ntfschar *upcase = vol->upcase;
	const u32 upcase_len = vol->upcase_len;
	u32 al_name_len;
	errno_t err = 0;
	static const char es[] = " Unmount and run chkdsk.";
	const BOOL case_sensitive = NVolCaseSensitive(vol);

	if (ctx->is_iteration)
		panic("%s(): ctx->is_iteration\n", __FUNCTION__);
	base_ni = ctx->base_ni;
	ntfs_debug("Entering for mft_no 0x%llx, type 0x%x.",
			(unsigned long long)ni->mft_no, le32_to_cpu(type));
	if (!base_ni) {
		/* First call happens with the base mft record. */
		base_ni = ctx->base_ni = ctx->ni;
		ctx->base_m = ctx->m;
	}
	if (ni == base_ni)
		ctx->base_a = ctx->a;
	if (type == AT_END)
		goto not_found;
	al_start = base_ni->attr_list;
	al_end = al_start + base_ni->attr_list_size;
	if (!ctx->al_entry)
		ctx->al_entry = (ATTR_LIST_ENTRY*)al_start;
	/*
	 * Iterate over entries in attribute list starting at @ctx->al_entry,
	 * or the entry following that, depending on the value of
	 * @ctx->is_first.
	 */
	if (ctx->is_first) {
		al_entry = ctx->al_entry;
		ctx->is_first = 0;
	} else
		al_entry = (ATTR_LIST_ENTRY*)((u8*)ctx->al_entry +
				le16_to_cpu(ctx->al_entry->length));
	for (;; al_entry = next_al_entry) {
		/* Out of bounds check. */
		if ((u8*)al_entry < base_ni->attr_list ||
				(u8*)al_entry > al_end)
			break;	/* Inode is corrupt. */
		ctx->al_entry = al_entry;
		/* Catch the end of the attribute list. */
		if ((u8*)al_entry == al_end)
			goto not_found;
		if (!al_entry->length)
			break;
		if ((u8*)al_entry + 6 > al_end || (u8*)al_entry +
				le16_to_cpu(al_entry->length) > al_end)
			break;
		next_al_entry = (ATTR_LIST_ENTRY*)((u8*)al_entry +
				le16_to_cpu(al_entry->length));
		if (al_entry->type != type) {
			if (le32_to_cpu(al_entry->type) < le32_to_cpu(type))
				continue;
			goto not_found;
		}
		/*
		 * If @name is AT_UNNAMED we want an unnamed attribute.
		 * If @name is present, compare the two names.
		 * Otherwise, match any attribute.
		 */
		al_name_len = al_entry->name_length;
		al_name = (ntfschar*)((u8*)al_entry + al_entry->name_offset);
		if (name == AT_UNNAMED) {
			if (al_name_len)
				goto not_found;
		} else if (name && !ntfs_are_names_equal(al_name, al_name_len,
				name, name_len, case_sensitive, upcase,
				upcase_len)) {
			int rc;

			rc = ntfs_collate_names(name, name_len, al_name,
					al_name_len, 1, FALSE,
					upcase, upcase_len);
			/*
			 * If @name collates before al_name, there is no
			 * matching attribute.
			 */
			if (rc == -1)
				goto not_found;
			/* If the strings are not equal, continue search. */
			if (rc)
				continue;
			/*
			 * FIXME: Reverse engineering showed 0, IGNORE_CASE but
			 * that would be inconsistent with
			 * ntfs_attr_find_in_mft_record().  The subsequent rc
			 * checks were also different.  Perhaps I made a
			 * mistake in one of the two.  Need to recheck which is
			 * correct or at least see what is going on...
			 */
			rc = ntfs_collate_names(name, name_len, al_name,
					al_name_len, 1, TRUE,
					vol->upcase, vol->upcase_len);
			if (rc == -1)
				goto not_found;
			if (rc)
				continue;
		}
		/*
		 * The names match or @name not present and attribute is
		 * unnamed.  Now check @lowest_vcn.  Continue search if the
		 * next attribute list entry still fits @lowest_vcn.  Otherwise
		 * we have reached the right one or the search has failed.
		 */
		if (lowest_vcn && (u8*)next_al_entry >= al_start &&
				(u8*)next_al_entry + 6 < al_end &&
				(u8*)next_al_entry + le16_to_cpu(
					next_al_entry->length) <= al_end &&
				sle64_to_cpu(next_al_entry->lowest_vcn) <=
					lowest_vcn &&
				next_al_entry->type == al_entry->type &&
				next_al_entry->name_length == al_name_len &&
				ntfs_are_names_equal((ntfschar*)((u8*)
					next_al_entry +
					next_al_entry->name_offset),
					next_al_entry->name_length,
					al_name, al_name_len, case_sensitive,
					vol->upcase, vol->upcase_len))
			continue;
		if (MREF_LE(al_entry->mft_reference) == ni->mft_no) {
			if (MSEQNO_LE(al_entry->mft_reference) != ni->seq_no) {
				ntfs_error(vol->mp, "Found stale mft "
						"reference in attribute list "
						"of base inode 0x%llx.%s",
						(unsigned long long)
						base_ni->mft_no, es);
				err = EIO;
				break;
			}
		} else { /* Mft references do not match. */
			/* If there is a mapped record unmap it first. */
			if (ni != base_ni)
				ntfs_extent_mft_record_unmap(ni);
			/* Do we want the base record back? */
			if (MREF_LE(al_entry->mft_reference) ==
					base_ni->mft_no) {
				ni = ctx->ni = base_ni;
				ctx->m = ctx->base_m;
			} else {
				/* We want an extent record. */
				err = ntfs_extent_mft_record_map_ext(base_ni,
						le64_to_cpu(
						al_entry->mft_reference), &ni,
						&ctx->m, ctx->is_mft_locked);
				if (err) {
					ntfs_error(vol->mp, "Failed to map "
							"extent mft record "
							"0x%llx of base inode "
							"0x%llx.%s",
							(unsigned long long)
							MREF_LE(al_entry->
							mft_reference),
							(unsigned long long)
							base_ni->mft_no, es);
					if (err == ENOENT)
						err = EIO;
					/* Cause @ctx to be sanitized below. */
					ni = NULL;
					break;
				}
				ctx->ni = ni;
			}
		}
		a = ctx->a = (ATTR_RECORD*)((u8*)ctx->m +
				le16_to_cpu(ctx->m->attrs_offset));
		/*
		 * ctx->ni, ctx->m, and ctx->a now point to the mft record
		 * containing the attribute represented by the current
		 * al_entry.
		 *
		 * We could call into ntfs_attr_find_in_mft_record() to find
		 * the right attribute in this mft record but this would be
		 * less efficient and not quite accurate as it ignores the
		 * attribute instance numbers for example which become
		 * important when one plays with attribute lists.  Also,
		 * because a proper match has been found in the attribute list
		 * entry above, the comparison can now be optimized.  So it is
		 * worth re-implementing a simplified
		 * ntfs_attr_find_in_mft_record() here.
		 *
		 * Use a manual loop so we can still use break and continue
		 * with the same meanings as above.
		 */
do_next_attr_loop:
		if ((u8*)a < (u8*)ctx->m || (u8*)a > (u8*)ctx->m +
				le32_to_cpu(ctx->m->bytes_allocated))
			break;
		if (a->type == AT_END)
			continue;
		if (!a->length)
			break;
		if (al_entry->instance != a->instance)
			goto do_next_attr;
		/*
		 * If the type and/or the name are mismatched between the
		 * attribute list entry and the attribute record, there is
		 * corruption so we break and return error EIO.
		 */
		if (al_entry->type != a->type)
			break;
		if (!ntfs_are_names_equal((ntfschar*)((u8*)a +
				le16_to_cpu(a->name_offset)), a->name_length,
				al_name, al_name_len, case_sensitive,
				vol->upcase, vol->upcase_len))
			break;
		ctx->a = a;
		/*
		 * If no @val specified or @val specified and it matches, we
		 * have found it!
		 */
		if (!val || (!a->non_resident &&
				le32_to_cpu(a->value_length) == val_len &&
				!bcmp((u8*)a + le16_to_cpu(a->value_offset),
				val, val_len))) {
			ntfs_debug("Done, found.");
			return 0;
		}
do_next_attr:
		/* Proceed to the next attribute in the current mft record. */
		a = (ATTR_RECORD*)((u8*)a + le32_to_cpu(a->length));
		goto do_next_attr_loop;
	}
	if (!err) {
		ntfs_error(vol->mp, "Base inode 0x%llx contains corrupt "
				"attribute list attribute.%s",
				(unsigned long long)base_ni->mft_no, es);
		err = EIO;
	}
	if (ni != base_ni) {
		if (ni)
			ntfs_extent_mft_record_unmap(ni);
		ctx->ni = base_ni;
		ctx->m = ctx->base_m;
		ctx->a = ctx->base_a;
	}
	if (err != ENOMEM)
		NVolSetErrors(vol);
	return err;
not_found:
	/*
	 * If we were looking for AT_END, we reset the search context @ctx and
	 * use ntfs_attr_find_in_mft_record() to seek to the end of the base
	 * mft record.
	 */
	if (type == AT_END) {
		ntfs_attr_search_ctx_reinit(ctx);
		return ntfs_attr_find_in_mft_record(AT_END, NULL, 0, NULL, 0,
				ctx);
	}
	/*
	 * The attribute was not found.  Before we return, we want to ensure
	 * @ctx->m and @ctx->a indicate the position at which the attribute
	 * should be inserted in the base mft record.  Since we also want to
	 * preserve @ctx->al_entry we cannot reinitialize the search context
	 * using ntfs_attr_search_ctx_reinit() as this would set @ctx->al_entry
	 * to NULL.  Thus we do the necessary bits manually (see
	 * ntfs_attr_search_ctx_init() above).  Note, we postpone setting
	 * @base_a until after the call to ntfs_attr_find_in_mft_record() as we
	 * do not know the correct value yet.
	 */
	if (ni != base_ni)
		ntfs_extent_mft_record_unmap(ni);
	ctx->m = ctx->base_m;
	ctx->a = (ATTR_RECORD*)((u8*)ctx->m +
			le16_to_cpu(ctx->m->attrs_offset));
	ctx->is_first = 1;
	ctx->ni = base_ni;
	/*
	 * In case there are multiple matches in the base mft record, need to
	 * keep enumerating until we get an attribute not found response (or
	 * another error), otherwise we would keep returning the same attribute
	 * over and over again and all programs using us for enumeration would
	 * lock up in a tight loop.
	 */
	do {
		err = ntfs_attr_find_in_mft_record(type, name, name_len,
				val, val_len, ctx);
	} while (!err);
	ctx->base_a = ctx->a;
	ntfs_debug("Done, not found.");
	return err;
}

/**
 * ntfs_attr_lookup - find an attribute in an ntfs inode
 * @type:	attribute type to find
 * @name:	attribute name to find (optional, i.e. NULL means do not care)
 * @name_len:	attribute name length (only needed if @name present)
 * @lowest_vcn:	lowest vcn to find (optional, non-resident attributes only)
 * @val:	attribute value to find (optional, resident attributes only)
 * @val_len:	attribute value length (only needed if @val present)
 * @ctx:	search context with mft record and attribute to search from
 *
 * Find an attribute in an ntfs inode.  On first search @ctx->ni must be the
 * base mft record and @ctx must have been obtained from a call to
 * ntfs_attr_search_ctx_get().
 *
 * This function transparently handles attribute lists and @ctx is used to
 * continue searches where they were left off at.
 *
 * After finishing with the attribute/mft record you need to call
 * ntfs_attr_search_ctx_put() to clean up the search context (unmapping any
 * mapped mft records, etc).
 *
 * Return 0 if the search was successful and errno if not.
 *
 * On success, @ctx->a is the found attribute and it is in mft record @ctx->m.
 * If an attribute list attribute is present, @ctx->al_entry is the attribute
 * list entry of the found attribute.
 *
 * On error ENOENT, @ctx->a is the attribute which collates just after the
 * attribute being searched for, i.e. if one wants to add the attribute to the
 * mft record this is the correct place to insert it into.  If an attribute
 * list attribute is present, @ctx->al_entry is the attribute list entry which
 * collates just after the attribute list entry of the attribute being searched
 * for, i.e. if one wants to add the attribute to the mft record this is the
 * correct place to insert its attribute list entry into.
 *
 * When errno != ENOENT, an error occured during the lookup.  @ctx->a is then
 * undefined and in particular you should not rely on it not having changed.
 *
 * Warning: Never use @val when looking for attribute types which can be
 *	    non-resident as this most likely will result in a crash!
 */
errno_t ntfs_attr_lookup(const ATTR_TYPE type,
		const ntfschar *name, const u32 name_len, const VCN lowest_vcn,
		const void *val, const u32 val_len, ntfs_attr_search_ctx *ctx)
{
	ntfs_inode *base_ni;

	ntfs_debug("Entering.");
	if (ctx->base_ni)
		base_ni = ctx->base_ni;
	else
		base_ni = ctx->ni;
	/* Sanity check, just for debugging really. */
	if (!base_ni)
		panic("%s(): !base_ni\n", __FUNCTION__);
	if (!NInoAttrList(base_ni) || type == AT_ATTRIBUTE_LIST)
		return ntfs_attr_find_in_mft_record(type, name, name_len,
				val, val_len, ctx);
	if (ctx->is_iteration)
		panic("%s(): ctx->is_iteration\n", __FUNCTION__);
	return ntfs_attr_find_in_attribute_list(type, name, name_len,
			lowest_vcn, val, val_len, ctx);
}

/**
 * ntfs_attr_find_in_attrdef - find an attribute in the $AttrDef system file
 * @vol:	ntfs volume to which the attribute belongs
 * @type:	attribute type which to find
 *
 * Search for the attribute definition record corresponding to the attribute
 * @type in the $AttrDef system file.
 *
 * Return the attribute type definition record if found and NULL if not found.
 */
static ATTR_DEF *ntfs_attr_find_in_attrdef(const ntfs_volume *vol,
		const ATTR_TYPE type)
{
	ATTR_DEF *ad;

	if (!vol->attrdef)
		panic("%s(): !vol->attrdef\n", __FUNCTION__);
	if (!type)
		panic("%s(): !type\n", __FUNCTION__);
	for (ad = vol->attrdef; (u8*)ad - (u8*)vol->attrdef <
			vol->attrdef_size && ad->type; ++ad) {
		/* If we have not found it yet, carry on searching. */
		if (le32_to_cpu(type) > le32_to_cpu(ad->type))
			continue;
		/* If we have found the attribute, return it. */
		if (type == ad->type)
			return ad;
		/* We have gone too far already.  No point in continuing. */
		break;
	}
	/* Attribute not found. */
	ntfs_debug("Attribute type 0x%x not found in $AttrDef.",
			le32_to_cpu(type));
	return NULL;
}

/**
 * ntfs_attr_size_bounds_check - check a size of an attribute type for validity
 * @vol:	ntfs volume to which the attribute belongs
 * @type:	attribute type which to check
 * @size:	size which to check
 *
 * Check whether the @size in bytes is valid for an attribute of @type on the
 * ntfs volume @vol.  This information is obtained from $AttrDef system file.
 *
 * Return 0 if valid, ERANGE if not valid, and ENOENT if the attribute is not
 * listed in $AttrDef.
 */
errno_t ntfs_attr_size_bounds_check(const ntfs_volume *vol,
		const ATTR_TYPE type, const s64 size)
{
	ATTR_DEF *ad;

	if (size < 0)
		panic("%s(): size < 0\n", __FUNCTION__);
	/*
	 * $ATTRIBUTE_LIST has a maximum size of 256kiB, but this is not
	 * listed in $AttrDef.
	 */
	if (type == AT_ATTRIBUTE_LIST && size > NTFS_MAX_ATTR_LIST_SIZE)
		return ERANGE;
	/* Get the $AttrDef entry for the attribute @type. */
	ad = ntfs_attr_find_in_attrdef(vol, type);
	if (!ad)
		return ENOENT;
	/* Do the bounds check. */
	if ((sle64_to_cpu(ad->min_size) > 0 &&
			size < sle64_to_cpu(ad->min_size)) ||
			(sle64_to_cpu(ad->max_size) > 0 &&
			size > sle64_to_cpu(ad->max_size)) ||
			(u64)size > NTFS_MAX_ATTRIBUTE_SIZE)
		return ERANGE;
	return 0;
}

/**
 * ntfs_attr_can_be_non_resident - check if an attribute can be non-resident
 * @vol:	ntfs volume to which the attribute belongs
 * @type:	attribute type which to check
 *
 * Check whether the attribute of @type on the ntfs volume @vol is allowed to
 * be non-resident.  This information is obtained from $AttrDef system file.
 *
 * Return 0 if the attribute is allowed to be non-resident, EPERM if not, and
 * ENOENT if the attribute is not listed in $AttrDef.
 */
static errno_t ntfs_attr_can_be_non_resident(const ntfs_volume *vol,
		const ATTR_TYPE type)
{
	ATTR_DEF *ad;

	/* Find the attribute definition record in $AttrDef. */
	ad = ntfs_attr_find_in_attrdef(vol, type);
	if (!ad)
		return ENOENT;
	/* Check the flags and return the result. */
	if (ad->flags & ATTR_DEF_RESIDENT)
		return EPERM;
	return 0;
}

/**
 * ntfs_attr_can_be_resident - check if an attribute can be resident
 * @vol:	ntfs volume to which the attribute belongs
 * @type:	attribute type which to check
 *
 * Check whether the attribute of @type on the ntfs volume @vol is allowed to
 * be resident.  This information is derived from our ntfs knowledge and may
 * not be completely accurate, especially when user defined attributes are
 * present.  Basically we allow everything to be resident except for index
 * allocation attributes.
 *
 * Return 0 if the attribute is allowed to be resident and EPERM if not.
 *
 * Warning: In the system file $MFT the attribute $Bitmap must be non-resident
 *	    otherwise windows will not boot (blue screen of death)!  We cannot
 *	    check for this here as we do not know which inode's $Bitmap is
 *	    being asked about so the caller needs to special case this.
 */
errno_t ntfs_attr_can_be_resident(const ntfs_volume *vol, const ATTR_TYPE type)
{
	if (type == AT_INDEX_ALLOCATION)
		return EPERM;
	return 0;
}

/**
 * ntfs_attr_record_is_only_one - check if an attribute is the only one
 * @m:		the mft record in which the attribute to check resides
 * @a:		the attribute to check
 *
 * Check if the attribute @a is the only attribute record in its mft record @m.
 *
 * Return true if @a is the only attribute record in its mft record @m and
 * false if @a is not the only attribute record in its mft record @m.
 */
BOOL ntfs_attr_record_is_only_one(MFT_RECORD *m, ATTR_RECORD *a)
{
	ATTR_RECORD *first_a, *next_a;

	first_a = (ATTR_RECORD*)((u8*)m + le16_to_cpu(m->attrs_offset));
	next_a = (ATTR_RECORD*)((u8*)a + le32_to_cpu(a->length));
	return (first_a == a && next_a->type == AT_END);
}

/**
 * ntfs_attr_record_delete_internal - delete attribute record from mft record
 * @m:		mft record containing attribute record to delete
 * @a:		attribute record to delete
 *
 * Delete the attribute record @a, i.e. the resident part of the attribute,
 * from the mft record @m.
 *
 * This function cannot fail.
 *
 * Note the caller is responsible for marking the mft record dirty after
 * calling this function.
 */
void ntfs_attr_record_delete_internal(MFT_RECORD *m, ATTR_RECORD *a)
{
	const u32 new_muse = le32_to_cpu(m->bytes_in_use) -
			le32_to_cpu(a->length);
	/* Move attributes following @a into the position of @a. */
	memmove(a, (u8*)a + le32_to_cpu(a->length),
			new_muse - ((u8*)a - (u8*)m));
	/* Adjust @m to reflect the change in used space. */
	m->bytes_in_use = cpu_to_le32(new_muse);
}

/**
 * ntfs_attr_record_delete - delete an attribute record from its mft record
 * @base_ni:	base ntfs inode from which to delete the attribute
 * @ctx:	attribute search context describing attribute record to delete
 *
 * Delete the attribute record, i.e. the resident part of the attribute,
 * described by @ctx->a from its mft record @ctx->m and mark the mft record
 * dirty so it gets written out later.
 *
 * In an attribute list attribute is present also remove the attribute list
 * attribute entry corresponding to the attribute being deleted and update
 * the attribute list attribute record accordingly.
 *
 * If the only attribute in the mft record is the attribute being deleted then
 * instead of deleting the attribute we free the extent mft record altogether
 * taking care to disconnect it from the base ntfs inode in the process.  As
 * above we update the attribute list attribute accordingly.
 *
 * If we end up freeing the extent mft record we go on to check the attribute
 * list attribute and if it no longer references any extent mft records we
 * remove the attribute list attribute altogether and update the base ntfs
 * inode to reflect the changed inode state.
 *
 * Return 0 on success and the error code on error.
 *
 * Note that on success the attribute search record is no longer valid and the
 * caller must either release it by calling ntfs_attr_search_ctx_put() or
 * reinitialize it by calling ntfs_attr_search_ctx_reinit().  Looking at the
 * search context or using it to call other functions would have unpredictable
 * results and could lead to crashes and file system corruption.
 */
errno_t ntfs_attr_record_delete(ntfs_inode *base_ni, ntfs_attr_search_ctx *ctx)
{
	ntfs_inode *ni;
	MFT_RECORD *m;
	ATTR_RECORD *a;
	ATTR_LIST_ENTRY *al_entry;
	errno_t err;
	unsigned al_ofs;
	BOOL al_needed;

	ni = ctx->ni;
	m = ctx->m;
	a = ctx->a;
	ntfs_debug("Entering for attribute type 0x%x located in %s mft "
			"record 0x%llx.  Attribute list attribute is "
			"%spresent.", (unsigned)le32_to_cpu(a->type),
			(base_ni == ni) ? "base" : "extent",
			(unsigned long long)ni->mft_no,
			NInoAttrList(base_ni) ? "" : "not ");
	/*
	 * If there is no attribute list attribute, the mft record must be a
	 * base mft record and thus it cannot be becoming empty as a
	 * consequence of deleting the attribute record.  Thus for inodes
	 * without an attribute list attribute we have a fast path of simply
	 * going ahead and deleting the attribute record and returning.
	 */
	if (!NInoAttrList(base_ni)) {
		ntfs_attr_record_delete_internal(m, a);
		NInoSetMrecNeedsDirtying(base_ni);
		ntfs_debug("Done (no attribute list attribute).");
		return 0;
	}
	if (a->type == AT_ATTRIBUTE_LIST)
		panic("%s(): a->type == AT_ATTRIBUTE_LIST\n", __FUNCTION__);
	al_entry = ctx->al_entry;
	if (!al_entry)
		panic("%s(): !al_entry\n", __FUNCTION__);
	/*
	 * We have an attribute list attribute.  To begin with check if the
	 * attribute to be deleted is in the base mft record or if it is not
	 * the only attribute in the extent mft record.  In both of these cases
	 * we need to delete the attribute record from its mft record.
	 *
	 * Otherwise the attribute to be deleted is in an extent mft record and
	 * it is the only attribute in the extent mft record thus we need to
	 * free the extent mft record instead of deleting the attribute record.
	 */
	if (base_ni == ni || (u8*)m + le16_to_cpu(m->attrs_offset) != (u8*)a ||
			((ATTR_RECORD*)((u8*)a +
			le32_to_cpu(a->length)))->type != AT_END) {
		ntfs_attr_record_delete_internal(m, a);
		/*
		 * If the attribute was not in the base mft record mark the
		 * extent mft record dirty so it gets written out later.  If
		 * the attribute was in the base mft record it will be marked
		 * dirty later when the attribute list attribute record is
		 * updated which is in the base mft record by definition.
		 *
		 * We also unmap the extent mft record so we get to the same
		 * state as in the above case where we freed the extent mft
		 * record and we set @ctx->ni to equal the base inode @base_ni
		 * so that the search context is initialized from scratch or
		 * simply freed if the caller reinitializes or releases the
		 * search context respectively.
		 */
		if (base_ni != ni) {
			NInoSetMrecNeedsDirtying(ni);
			ntfs_extent_mft_record_unmap(ni);
			ctx->ni = base_ni;
		}
	} else {
		err = ntfs_extent_mft_record_free(base_ni, ni, m);
		if (err) {
			/*
			 * Ignore the error as we just end up with an unused
			 * mft record that is marked in use.
			 */
			ntfs_error(ni->vol->mp, "Failed to free extent mft_no "
					"0x%llx (error %d).  Unmount and run "
					"chkdsk to recover the lost inode.",
					(unsigned long long)ni->mft_no, err);
			NVolSetErrors(ni->vol);
			/*
			 * Relese the extent mft record after dirtying it thus
			 * simulating the effect of freeing it.
			 */
			NInoSetMrecNeedsDirtying(ni);
			ntfs_extent_mft_record_unmap(ni);
		}
		/*
		 * The attribute search context still points to the no longer
		 * mapped extent inode thus we need to change it to point to
		 * the base inode instead so the context can be reinitialized
		 * or released safely.
		 */
		ctx->ni = base_ni;
		/*
		 * Check the attribute list attribute.  If there are no other
		 * attribute list attribute entries referencing extent mft
		 * records delete the attribute list attribute altogether.
		 *
		 * If this fails it does not matter as we simply retain the
		 * attribute list attribute so we ignore the error and go on to
		 * delete the attribute list attribute entry instead.
		 *
		 * If there are other attribute list attribute entries
		 * referencing extent mft records we still need the attribute
		 * list attribute thus we go on to delete the attribute list
		 * entry corresponding to the attribute record we just deleted
		 * by freeing its extent mft record.
		 */
		err = ntfs_attr_list_is_needed(base_ni, al_entry, &al_needed);
		if (err)
			ntfs_warning(ni->vol->mp, "Failed to determine if "
					"attribute list attribute of mft_no "
					"0x%llx if still needed (error %d).  "
					"Assuming it is still needed and "
					"continuing.",
					(unsigned long long)base_ni->mft_no,
					err);
		else if (!al_needed) {
			/*
			 * No more extent mft records are in use.  Delete the
			 * attribute list attribute.
			 */
			ntfs_attr_search_ctx_reinit(ctx);
			err = ntfs_attr_list_delete(base_ni, ctx);
			if (!err) {
				/*
				 * We deleted the attribute list attribute and
				 * this will have updated the base inode
				 * appropriately thus we are done.
				 */
				ntfs_debug("Done (deleted attribute list "
						"attribute).");
				return 0;
			}
			ntfs_warning(ni->vol->mp, "Failed to delete attribute "
					"list attribute of mft_no 0x%llx "
					"(error %d).  Continuing by trying to "
					"delete the attribute list entry of "
					"the deleted attribute instead.",
					(unsigned long long)base_ni->mft_no,
					err);
		}
	}
	/*
	 * Both @ctx and @ni are now invalid and cannot be used any more which
	 * is fine as we have finished dealing with the attribute record.
	 *
	 * We now need to delete the corresponding attribute list attribute
	 * entry.
	 */
	al_ofs = (u8*)al_entry - base_ni->attr_list;
	ntfs_attr_list_entry_delete(base_ni, al_entry);
	ntfs_attr_search_ctx_reinit(ctx);
	err = ntfs_attr_list_sync_shrink(base_ni, al_ofs, ctx);
	if (!err) {
		ntfs_debug("Done (deleted attribute list attribute entry).");
		return 0;
	}
	NInoSetMrecNeedsDirtying(base_ni);
	ntfs_error(ni->vol->mp, "Failed to delete attribute list attribute "
			"entry in base mft_no 0x%llx (error %d).  Leaving "
			"inconsistent metadata.  Unmount and run chkdsk.",
			(unsigned long long)base_ni->mft_no, err);
	NVolSetErrors(ni->vol);
	return err;
}

/**
 * ntfs_attr_record_make_space - make space for a new attribute record
 * @m:		mft record in which to make space for the new attribute record
 * @a:		attribute record in front of which to make space
 * @size:	byte size of the new attribute record for which to make space
 *
 * Make space for a new attribute record of size @size in the mft record @m, in
 * front of the existing attribute record @a.
 *
 * Return 0 on success and errno on error.  The following error codes are
 * defined:
 *	ENOSPC - Not enough space in the mft record @m.
 *
 * Note: On error, no modifications have been performed whatsoever.
 */
errno_t ntfs_attr_record_make_space(MFT_RECORD *m, ATTR_RECORD *a, u32 size)
{
	u32 new_muse;
	const u32 muse = le32_to_cpu(m->bytes_in_use);
	/* Align to 8 bytes if it is not already done. */
	if (size & 7)
		size = (size + 7) & ~7;
	new_muse = muse + size;
	/* Not enough space in this mft record. */
	if (new_muse > le32_to_cpu(m->bytes_allocated))
		return ENOSPC;
	/* Move attributes starting with @a to make space of @size bytes. */
	memmove((u8*)a + size, a, muse - ((u8*)a - (u8*)m));
	/* Adjust @m to reflect the change in used space. */
	m->bytes_in_use = cpu_to_le32(new_muse);
	/* Clear the created space so we start with a clean slate. */
	bzero(a, size);
	/*
	 * Set the attribute size in the newly created attribute, now at @a.
	 * We do this here so that the caller does not need to worry about
	 * rounding up the size to set the attribute length.
	 */
	a->length = cpu_to_le32(size);
	return 0;
}

/**
 * ntfs_attr_record_resize - resize an attribute record
 * @m:		mft record containing attribute record
 * @a:		attribute record to resize
 * @new_size:	new size in bytes to which to resize the attribute record @a
 *
 * Resize the attribute record @a, i.e. the resident part of the attribute, in
 * the mft record @m to @new_size bytes.
 *
 * Return 0 on success and errno on error.  The following error codes are
 * defined:
 *	ENOSPC	- Not enough space in the mft record @m to perform the resize.
 *
 * Note: On error, no modifications have been performed whatsoever.
 *
 * Warning: If you make a record smaller without having copied all the data you
 *	    are interested in the data may be overwritten.
 */
errno_t ntfs_attr_record_resize(MFT_RECORD *m, ATTR_RECORD *a, u32 new_size)
{
	const u32 old_size = le32_to_cpu(a->length);

	ntfs_debug("Entering for new_size %u.", new_size);
	/* Align to 8 bytes if it is not already done. */
	if (new_size & 7)
		new_size = (new_size + 7) & ~7;
	/* If the actual attribute length has changed, move things around. */
	if (new_size != old_size) {
		const u32 muse = le32_to_cpu(m->bytes_in_use);
		const u32 new_muse = muse - old_size + new_size;
		/* Not enough space in this mft record. */
		if (new_muse > le32_to_cpu(m->bytes_allocated))
			return ENOSPC;
		/* Move attributes following @a to their new location. */
		memmove((u8*)a + new_size, (u8*)a + old_size,
				muse - ((u8*)a - (u8*)m) - old_size);
		/* Adjust @m to reflect the change in used space. */
		m->bytes_in_use = cpu_to_le32(new_muse);
		/* Adjust @a to reflect the new size. */
		if (new_size >= offsetof(ATTR_REC, length) + sizeof(a->length))
			a->length = cpu_to_le32(new_size);
	}
	return 0;
}

/**
 * ntfs_attr_mapping_pairs_update - update an attribute's mapping pairs array
 * @base_ni:	base ntfs inode to which the attribute belongs
 * @ni:		ntfs inode of attribute whose mapping pairs array to update
 * @first_vcn:	first vcn which to update in the mapping pairs array
 * @last_vcn:	last vcn which to update in the mapping pairs array
 * @ctx:	search context describing the attribute to work on or NULL
 *
 * Create or update the mapping pairs arrays from the locked runlist of the
 * attribute @ni, i.e. @ni->rl, starting at vcn @first_vcn and finishing with
 * vcn @last_vcn.  The update can actually start before @first_vcn and finish
 * after @last_vcn but guarantees to at least include the range between
 * @first_vcn and @last_vcn, inclusive.
 *
 * This function is called from a variety of places after clusters have been
 * allocated to and/or freed from an attribute.  The runlist has already been
 * updated to reflect the allocated/freed clusters.  This functions takes the
 * modified runlist range and syncs it to the attribute record(s) by
 * compressing the runlist into mapping pairs array fragments and writing them
 * into the attribute record(s) of the attribute.
 *
 * This function also updates the attribute sizes using the values from the
 * ntfs inode @ni and syncs them to the base attribute record and if the
 * attribute has become sparse but the attribute record is not marked sparse or
 * the attribute is no longer sparse but the attribute record is marked sparse
 * the base attribute record is updated to reflect the changed state which
 * involves setting/clearing the sparse flag as well as the addition/removal of
 * the compressed size to the attribute record.  When the compressed size is
 * added this can lead to a larger portion of the mapping pairs array being
 * updated because there may not be enough space in the mft record to extend
 * the base attribute record to fit the compressed size.  When updating the
 * attribute record the compression state of the attribute is also taken into
 * consideration as the compressed size is used both with compressed and sparse
 * attributes.
 *
 * The update can involve the allocation/freeing of extent mft records and/or
 * extent attribute records.  If this happens the attribute list attribute in
 * the base ntfs inode @base_ni is updated appropriately both in memory and in
 * the attribute list attribute record in the base mft record.
 *
 * A @last_vcn of -1 means end of runlist and in that case the mapping pairs
 * array corresponding to the runlist starting at vcn @first_vcn and finishing
 * at the end of the runlist is updated.
 *
 * If @ctx is NULL, it is assumed that the attribute mft record is not mapped
 * and hence a new search context is allocated, the mft record is mapped, and
 * the attribute is looked up.  On completion the allocated search context is
 * released if it was allocated by ntfs_attr_mapping_pairs_update().
 *
 * Return 0 on success and errno on error.
 *
 * Locking: The runlist @ni->rl must be locked for writing, it remains locked
 *	    throughout, and is left locked upon return.
 */
#if 0
errno_t ntfs_attr_mapping_pairs_update(ntfs_inode *base_ni, ntfs_inode *ni,
		VCN first_vcn, VCN last_vcn, ntfs_attr_search_ctx *ctx)
{
	VCN lowest_vcn, highest_vcn, stop_vcn;
	ntfs_volume *vol;
	ATTR_RECORD *a;
	errno_t err;
	BOOL mpa_is_valid, was_sparse, is_sparse;
	ntfs_attr_search_ctx attr_ctx;

	ntfs_debug("Entering for base mft_no 0x%llx, attribute type 0x%x, "
			"name len 0x%x, first_vcn 0x%llx, last_vcn 0x%llx, "
			"ctx is %spresent.",
			(unsigned long long)base_ni->mft_no,
			(unsigned)le32_to_cpu(ni->type), ni->name_len,
			(unsigned long long)first_vcn,
			(unsigned long long)last_vcn,
			ctx ? "" : "not ");
	vol = base_ni->vol;
	/*
	 * If no search context was specified use ours, initialize it, and look
	 * up the base attribute record so we can update the sizes, flags, and
	 * add/remove the compressed size if needed.
	 *
	 * We also need to look up the base attribute record if a search
	 * context was specified but it points to an extent attribute record.
	 */
	if (!ctx || ctx->a->lowest_vcn) {
		if (!ctx) {
			MFT_RECORD *base_m;

			err = ntfs_mft_record_map(base_ni, &base_m);
			if (err) {
				ntfs_error(vol->mp, "Failed to map mft_no "
						"0x%llx (error %d).",
						(unsigned long long)
						base_ni->mft_no, err);
				return err;
			}
			ctx = &attr_ctx;
			ntfs_attr_search_ctx_init(ctx, base_ni, base_m);
		}
		err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len, 0,
				NULL, 0, ctx);
		if (err) {
			if (err == ENOENT)
				err = EIO;
			ntfs_error(vol->mp, "Failed to look up base attribute "
					"record in mft_no 0x%llx (error %d).",
					(unsigned long long)base_ni->mft_no,
					err);
			goto err;
		}
	}
	a = ctx->a;
	if (!NInoNonResident(ni) || !a->non_resident)
		panic("%s(): !NInoNonResident(ni) || !a->non_resident\n",
				__FUNCTION__);
	mpa_is_valid = TRUE;
	/*
	 * If the attribute was sparse and is no longer sparse or it was not
	 * sparse and is now sparse, update the sparse state and add/remove the
	 * compressed size.
	 */
	was_sparse = a->flags & ATTR_IS_SPARSE;
	is_sparse = NInoSparse(ni);
	if (was_sparse == is_sparse)
		goto sparse_done;
	if (is_sparse) {
		a->flags |= ATTR_IS_SPARSE;
		if (NInoCompressed(ni))
			goto sparse_done;
		if (a->flags & ATTR_IS_COMPRESSED)
			panic("%s(): a->flags & ATTR_IS_COMPRESSED\n",
					__FUNCTION__);
		/*
		 * Add the compressed size and set up the relevant fields in
		 * the attribute record.
		 *
		 * If there is enough space in the mft record and we do not
		 * need to rewrite the mapping pairs array in this attribute
		 * record, resize the attribute record and move the mapping
		 * pairs array.
		 *
		 * If there is not enough space to perform the resize then do
		 * not preserve the mapping pairs array in this attribute
		 * record.
		 *
		 * If there still is not enough space to add the compressed
		 * size move the attribute record to an extent mft record (this
		 * cannot be the only attribute record in the current mft
		 * record).  If we do this do not preserve the mapping pairs
		 * array so we can make better use of the extent mft record.
		 *
		 * Note we need to ensure we have already mapped the runlist
		 * fragment described by the current mapping pairs array if we
		 * are not going to preserve it or we would lose the data.
		 */
		a->compression_unit = 0;
		if (vol->major_ver <= 1)
			a->compression_unit = NTFS_COMPRESSION_UNIT;
restart_compressed_size_add:
		if ((first_vcn > sle64_to_cpu(a->highest_vcn) + 1) &&
				!(err = ntfs_attr_record_resize(ctx->m, a,
				le32_to_cpu(a->length) +
				sizeof(a->compressed_size)))) {
			/*
			 * Move everything at the offset of the compressed size
			 * to make space for the compressed size.
			 */
			memmove((u8*)a + offsetof(ATTR_RECORD,
					compressed_size) +
					sizeof(a->compressed_size), (u8*)a +
					offsetof(ATTR_RECORD, compressed_size),
					le32_to_cpu(a->length) - offsetof(
					ATTR_RECORD, compressed_size));
			/*
			 * Update the name offset to match the moved data.  If
			 * there is no name then set the name offset to the
			 * correct position instead of adding to a potentially
			 * incorrect value.
			 */
			if (a->name_length)
				a->name_offset = cpu_to_le16(le16_to_cpu(
						a->name_offset) +
						sizeof(a->compressed_size));
			else
				a->name_offset = const_cpu_to_le16(offsetof(
						ATTR_RECORD, compressed_size) +
						sizeof(a->compressed_size));
			/* Update the mapping pairs offset. */
			mp_ofs = le16_to_cpu(a->mapping_pairs_offset) +
					sizeof(a->compressed_size);
			goto sparse_done;
		}
		/* Ensure this runlist fragment is mapped. */
		if (ni->allocated_size && (!ni->rl.elements ||
				ni->rl.rl->lcn == LCN_RL_NOT_MAPPED)) {
			err = ntfs_mapping_pairs_decompress(vol, a, &ni->rl);
			if (err) {
				ntfs_error(vol->mp, "Failed to decompress "
						"mapping pairs array (error "
						"%d).", err);
				goto err;
			}
		}
		/*
		 * Check whether the attribute is big enough to have the
		 * compressed size added to it.  We need at the very least
		 * space for the record header, the name, and a zero byte for
		 * an empty mapping pairs array and we need to allow for all
		 * the needed alignment padding.
		 */
		if (((sizeof(ATTR_RECORD) + a->name_length * sizeof(ntfschar) +
				7) & ~7) + 8 <= le32_to_cpu(a->length)) {
add_compressed_size:
			/*
			 * Move the name back to the new end of the attribute
			 * record header thus adding the compressed size.
			 */
			if (a->name_length)
				memmove((u8*)a + sizeof(ATTR_RECORD), (u8*)a +
						le16_to_cpu(a->name_offset),
						a->name_length *
						sizeof(ntfschar));
			/*
			 * Update the name offset and the mapping pairs offset
			 * to match the moved name.
			 */
			a->name_offset = const_cpu_to_le16(sizeof(ATTR_RECORD));
			a->mapping_pairs_offset = cpu_to_le16(
					(sizeof(ATTR_RECORD) + a->name_length *
					sizeof(ntfschar) + 7) & ~7);
			/*
			 * We no longer have a valid mapping pairs array in the
			 * current attribute record.
			 */
			mpa_is_valid = FALSE;
			goto sparse_done;
		}
		/*
		 * The attribute record is not big enough so try to extend it
		 * (in case we did not try to extend it above).
		 */
		err = ntfs_attr_record_resize(ctx->m, a,
				((sizeof(ATTR_RECORD) + a->name_length *
				sizeof(ntfschar) + 7) & ~7) + 8);
		if (!err)
			goto add_compressed_size;
		/*
		 * The attribute record cannot be the only one in the mft
		 * record if it is not large enough to hold an empty attribute
		 * record and there is not enough space to grow it.
		 */
		if (ntfs_attr_record_is_only_one(ctx->m, a))
			panic("%s(): ntfs_attr_is_only_one(ctx->m, a)\n",
					__FUNCTION__);
		/*
		 * This is our last resort.  Move the attribute to an extent
		 * mft record.
		 *
		 * First, add the attribute list attribute if it is not already
		 * present.
		 */
		if (!NInoAttrList(base_ni)) {
			err = ntfs_attr_list_add(base_ni, ctx->m, ctx);
			if (err || ctx->is_error) {
				if (!err)
					err = ctx->error;
				ntfs_error(vol->mp, "Failed to %s mft_no "
						"0x%llx (error %d).",
						ctx->is_error ?
						"remap extent mft record of" :
						"add attribute list attribute "
						"to", (unsigned long long)
						base_ni->mft_no, err);
				goto err;
			}
			/*
			 * The attribute location will have changed so update
			 * it from the search context.
			 */
			a = ctx->a;
			/*
			 * Retry the attribute record resize as we may now have
			 * enough space to add the compressed size.
			 *
			 * This can for example happen when the attribute was
			 * moved out to an extent mft record which has much
			 * more free space than the base mft record had or of
			 * course other attributes may have been moved out to
			 * extent mft records which has created enough space in
			 * the base mft record.
			 *
			 * If the attribute record was moved to an empty extent
			 * mft record this is the same case as if we moved the
			 * attribute record below so treat it the same, i.e. we
			 * do not preserve the mapping pairs array and use the
			 * maximum possible size for the mft record to allow us
			 * to consolidate the mapping pairs arrays.
			 */
			if (ntfs_attr_record_is_only_one(ctx->m, a))
				goto attr_is_only_one;
			goto restart_compressed_size_add;
		}
		/* Move the attribute to an extent mft record. */
		lck_rw_lock_shared(&base_ni->attr_list_rl.lock);
		err = ntfs_attr_record_move(ctx);
		lck_rw_unlock_shared(&base_ni->attr_list_rl.lock);
		if (err) {
			ntfs_error(vol->mp, "Failed to move attribute extent "
					"from mft record 0x%llx to an extent "
					"mft record (error %d).",
					(unsigned long long)ctx->ni->mft_no,
					err);
			/*
			 * We could try to remove the attribute list attribute
			 * if we added it above but this will require
			 * attributes to be moved back into the base mft record
			 * from extent mft records so is a lot of work and
			 * given we are in an error code path and given that it
			 * is ok to just leave the inode with an attribute list
			 * attribute we do not bother and just bail out.
			 */
			goto err;
		}
		/*
		 * The attribute location will have changed so update it from
		 * the search context.
		 */
		a = ctx->a;
attr_is_only_one:
		/*
		 * We now have enough space to add the compressed size so
		 * resize the attribute record.  Note we do not want to
		 * preserve the mapping pairs array as we will have
		 * significanly more space in the extent mft record thus we
		 * want to consolidate the mapping pairs arrays which is why we
		 * resize straight to the maximum possible size for the mft
		 * record.
		 */
		err = ntfs_attr_record_resize(ctx->m, a,
				le32_to_cpu(m->bytes_allocated) -
				le32_to_cpu(m->bytes_in_use) +
				le32_to_cpu(a->length));
		if (err)
			panic("%s(): err - resize failed\n", __FUNCTION__);
		if (((sizeof(ATTR_RECORD) + a->name_length * sizeof(ntfschar) +
				7) & ~7) + 8 > le32_to_cpu(a->length))
			panic("%s(): attribute record is still too small\n",
					__FUNCTION__);
		goto add_compressed_size;
	}
	/* The attribute is becoming non-sparse. */
	a->flags &= ~ATTR_IS_SPARSE;
	if (NInoCompressed(ni))
		goto sparse_done;
	if (a->flags & ATTR_IS_COMPRESSED)
		panic("%s(): a->flags & ATTR_IS_COMPRESSED\n", __FUNCTION__);
	/*
	 * Remove the compressed size and set up the relevant fields in the
	 * attribute record.
	 *
	 * If we do not need to rewrite the mapping pairs array in this
	 * attribute record, move the mapping pairs array and then resize the
	 * attribute record.
	 *
	 * Note we need to ensure we have already mapped the runlist fragment
	 * described by the current mapping pairs array if we are not going to
	 * preserve it or we would lose the data.
	 */
	a->compression_unit = 0;
	if (first_vcn > sle64_to_cpu(a->highest_vcn) + 1) {
		/*
		 * Move everything after the compressed size forward to the
		 * offset of the compressed size thus deleting the compressed
		 * size.
		 */
		memmove((u8*)a + offsetof(ATTR_RECORD, compressed_size),
				(u8*)a + offsetof(ATTR_RECORD,
				compressed_size) + sizeof(a->compressed_size),
				le32_to_cpu(a->length) - (offsetof(ATTR_RECORD,
				compressed_size) + sizeof(a->compressed_size)));
		/*
		 * Update the name offset and the mapping pairs offset to match
		 * the moved data.  If there is no name then set the name
		 * offset to the correct position instead of subtracting from a
		 * potentially incorrect value.
		 */
		if (!a->name_length)
			a->name_offset = const_cpu_to_le16(offsetof(ATTR_RECORD,
					compressed_size));
		else
			a->name_offset = cpu_to_le16(
					le16_to_cpu(a->name_offset) -
					sizeof(a->compressed_size));
		a->mapping_pairs_offset = cpu_to_le16(
				le16_to_cpu(a->mapping_pairs_offset) -
				sizeof(a->compressed_size));
		/*
		 * Shrink the attribute record to reflect the removal of the
		 * compressed size.  Note this cannot fail since we are making
		 * the attribute smaller thus by definition there there is
		 * enough space to do so.
		 */
		err = ntfs_attr_record_resize(ctx->m, a, le32_to_cpu(
				a->length) - sizeof(a->compressed_size));
		if (err)
			panic("%s(): err\n", __FUNCTION__);
		goto sparse_done;
	}
	/* Ensure this runlist fragment is mapped. */
	if (ni->allocated_size && (!ni->rl.elements ||
			ni->rl.rl->lcn == LCN_RL_NOT_MAPPED)) {
		err = ntfs_mapping_pairs_decompress(vol, a, &ni->rl);
		if (err) {
			ntfs_error(vol->mp, "Failed to decompress mapping "
					"pairs array (error %d).", err);
			goto err;
		}
	}
	mpa_is_valid = FALSE;
	/*
	 * Move the name forward to the offset of the compressed size thus
	 * deleting the compressed size.
	 */
	if (a->name_length)
		memmove((u8*)a + offsetof(ATTR_RECORD, compressed_size),
				(u8*)a + le16_to_cpu(a->name_offset),
				a->name_length * sizeof(ntfschar));
	/*
	 * Update the name offset and the mapping pairs offset to match the
	 * moved name.
	 */
	a->name_offset = const_cpu_to_le16(
			offsetof(ATTR_RECORD, compressed_size));
	a->mapping_pairs_offset = cpu_to_le16(
			(offsetof(ATTR_RECORD, compressed_size) +
			(a->name_length * sizeof(ntfschar)) + 7) & ~7);
sparse_done:
	/*
	 * Update the attribute sizes.
	 *
	 * TODO: Need to figure out whether we really need to update the data
	 * and initialized sizes or whether updating just the allocated and
	 * compressed sizes is sufficient in which case we can save a few CPU
	 * cycles by not updating the data and initialized sizes here.
	 */
	lck_spin_lock(&ni->size_lock);
	a->allocated_size = cpu_to_sle64(ni->allocated_size);
	a->data_size = cpu_to_sle64(ni->data_size);
	a->initialized_size = cpu_to_sle64(ni->initialized_size);
	if (a->flags & (ATTR_IS_COMPRESSED | ATTR_IS_SPARSE))
		a->compressed_size = cpu_to_sle64(ni->compressed_size);
	lck_spin_unlock(&ni->size_lock);
	/*
	 * If the current mapping pairs array is valid and the first vcn at
	 * which we need to update the mapping pairs array is not in this
	 * attribute extent, look up the attribute extent containing the first
	 * vcn.
	 */
	if (mpa_is_valid && first_vcn > sle64_to_cpu(a->highest_vcn) + 1) {
		err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len,
				first_vcn, NULL, 0, ctx);
		if (err) {
			if (err == ENOENT)
				err = EIO;
			ntfs_error(vol->mp, "Failed to look up extent "
					"attribute record containing VCN "
					"0x%llx in mft_no 0x%llx (error %d).",
					(unsigned long long)first_vcn,
					(unsigned long long)base_ni->mft_no,
					err);
			goto err;
		}
		a = ctx->a;
	}
	/*
	 * We need to rebuild the mapping pairs array in this attribute extent.
	 * But first, check if we can grow the attribute extent.  If this is
	 * the base extent and the attribute is not sparse nor compressed and
	 * it is allowed to be sparse then reserve the size of the compressed
	 * size field in the mft record so it is easier to make the attribute
	 * sparse later on.
	 *
	 * FIXME: But we don't want to do that if the attribute extent is in
	 * the base mft record and the attribute is $DATA or $INDEX_ALLOCATION,
	 * etc as we want to keep the first extent of theese base attribute
	 * extents in the base mft record thus we have to keep them small to
	 * allow the attribute list attribute to grow over time.
	 *
	 * FIXME: Need to make sure we map any unmapped regions of the runlist
	 * when determining the size of the mapping pairs array.
	 *
	 * FIXME: If we don't impose a last vcn when getting the size it would
	 * just cause the entirety of the mapping pairs array starting with the
	 * current extent to be mapped in, which is not necessarilly a bad
	 * thing as it will then be already mapped for all subsequent writes.
	 *
	 * FIXME: We do not want to keep rewriting the entire mapping pairs
	 * array every time we fill a hole so need to be careful when
	 * consolidating the mapping pairs array fragments.  OTOH we do not
	 * want to end up with millions of very short attribute extents so need
	 * to be careful about that, too.
	 */
// TODO: I AM HERE: 
	ntfs_error(vol->mp, "FIXME: TODO...");
	return ENOTSUP;
	ntfs_debug("Done.");
	return 0;
err:
	/*
	 * If we mapped the mft record and looked up the attribute, release the
	 * mapped mft record(s) here.
	 */
	if (ctx == &attr_ctx) {
		if (ctx->ni != base_ni)
			ntfs_extent_mft_record_unmap(ctx->ni);
		ntfs_mft_record_unmap(base_ni);
	}
	return err;
}
#endif

/**
 * ntfs_resident_attr_record_insert_internal - insert a resident attribute
 * @m:		mft record in which to insert the resident attribute
 * @a:		attribute in front of which to insert the new attribute
 * @type:	attribute type of new attribute
 * @name:	Unicode name of new attribute
 * @name_len:	Unicode character size of name of new attribute
 * @val_len:	byte size of attribute value of new attribute
 *
 * Insert a new resident attribute in the mft record @m, in front of the
 * existing attribute record @a.  The new attribute is of type @type, and has a
 * name of @name which is @name_len Unicode characters long.  The new attribute
 * value is @val_len bytes and is initialized to zero.
 *
 * Note: If the inode uses the attribute list attribute the caller is
 * responsible for adding an entry for the inserted attribute to the attribute
 * list attribute.
 *
 * Return 0 on success and errno on error.  The following error codes are
 * defined:
 *	ENOSPC - Not enough space in the mft record @m.
 *
 * Note: On error, no modifications have been performed whatsoever.
 */
errno_t ntfs_resident_attr_record_insert_internal(MFT_RECORD *m,
		ATTR_RECORD *a, const ATTR_TYPE type, const ntfschar *name,
		const u8 name_len, const u32 val_len)
{
	unsigned name_ofs, val_ofs;

	/*
	 * Calculate the offset into the new attribute at which the attribute
	 * name begins.  The name is placed directly after the resident
	 * attribute record itself.
	 */
	name_ofs = offsetof(ATTR_RECORD, reservedR) + sizeof(a->reservedR);
	/*
	 * Calculate the offset into the new attribute at which the attribute
	 * value begins.  The attribute value is placed after the name aligned
	 * to an 8-byte boundary.
	 */
	val_ofs = name_ofs + (((name_len << NTFSCHAR_SIZE_SHIFT) + 7) & ~7);
	/*
	 * Work out the size for the attribute record.  We simply take the
	 * offset to the attribute value we worked out above and add the size
	 * of the attribute value in bytes aligned to an 8-byte boundary.  Note
	 * we do not need to do the alignment as ntfs_attr_record_make_space()
	 * does it anyway.
	 */
	if (ntfs_attr_record_make_space(m, a, val_ofs + val_len))
		return ENOSPC;
	/*
	 * Now setup the new attribute record.  The entire attribute has been
	 * zeroed and the length of the attribute record has been set up by
	 * ntfs_attr_record_make_space().
	 */
	a->type = type;
	a->name_length = name_len;
	a->name_offset = cpu_to_le16(name_ofs);
	a->instance = m->next_attr_instance;
	/*
	 * Increment the next attribute instance number in the mft record as we
	 * consumed the old one.
	 */
	m->next_attr_instance = cpu_to_le16(
			(le16_to_cpu(m->next_attr_instance) + 1) & 0xffff);
	a->value_length = cpu_to_le32(val_len);
	a->value_offset = cpu_to_le16(val_ofs);
	if (type == AT_FILENAME)
		a->resident_flags = RESIDENT_ATTR_IS_INDEXED;
	/* Copy the attribute name into place. */
	if (name_len)
		memcpy((u8*)a + name_ofs, name,
				name_len << NTFSCHAR_SIZE_SHIFT);
	return 0;
}

/**
 * ntfs_resident_attr_record_insert - insert a resident attribute record
 * @ni:		base ntfs inode to which the attribute is being added
 * @ctx:	search context describing where to insert the resident attribute
 * @type:	attribute type of new attribute
 * @name:	Unicode name of new attribute
 * @name_len:	Unicode character size of name of new attribute
 * @val:	attribute value of new attribute (optional, can be NULL)
 * @val_len:	byte size of attribute value of new attribute
 *
 * Insert a new resident attribute in the base ntfs inode @ni at the position
 * indicated by the attribute search context @ctx and add an attribute list
 * attribute entry for it if the inode uses the attribute list attribute.
 *
 * The new attribute is of type @type, has a name of @name which is @name_len
 * Unicode characters long, and has a value of @val with size @val_len bytes.
 * If @val is NULL, the value of size @val_len is zeroed.
 *
 * If @val is NULL, the caller is responsible for marking the extent mft record
 * the attribute is in dirty.  We do it this way because we assume the caller
 * is going to modify the attribute further and will then mark it dirty.
 *
 * If the attribute is in the base mft record then the caller is always
 * responsible for marking the mft record dirty.
 *
 * Return 0 on success and errno on error.
 *
 * WARNING: Regardless of whether success or failure is returned, you need to
 *	    check @ctx->is_error and if 1 the @ctx is no longer valid, i.e. you
 *	    need to either call ntfs_attr_search_ctx_reinit() or
 *	    ntfs_attr_search_ctx_put() on it.  In that case @ctx->error will
 *	    give you the error code for why the mapping of the inode failed.
 */
errno_t ntfs_resident_attr_record_insert(ntfs_inode *ni,
		ntfs_attr_search_ctx *ctx, const ATTR_TYPE type,
		const ntfschar *name, const u8 name_len,
		const void *val, const u32 val_len)
{
	ntfs_volume *vol;
	MFT_RECORD *base_m, *m;
	ATTR_RECORD *a;
	ATTR_LIST_ENTRY *al_entry;
	unsigned name_ofs, val_ofs, al_entry_used, al_entry_len, new_al_size;
	unsigned new_al_alloc;
	errno_t err;
	BOOL al_entry_added;

	ntfs_debug("Entering for mft_no 0x%llx, attribute type 0x%x, name_len "
			"0x%x, val_len 0x%x.", (unsigned long long)ni->mft_no,
			(unsigned)le32_to_cpu(type), name_len, val_len);
	vol = ni->vol;
	/*
	 * Calculate the offset into the new attribute at which the attribute
	 * name begins.  The name is placed directly after the resident
	 * attribute record itself.
	 */
	name_ofs = offsetof(ATTR_RECORD, reservedR) + sizeof(a->reservedR);
	/*
	 * Calculate the offset into the new attribute at which the attribute
	 * value begins.  The attribute value is placed after the name aligned
	 * to an 8-byte boundary.
	 */
	val_ofs = name_ofs + (((name_len << NTFSCHAR_SIZE_SHIFT) + 7) & ~7);
	/*
	 * Work out the size for the attribute record.  We simply take the
	 * offset to the attribute value we worked out above and add the size
	 * of the attribute value in bytes aligned to an 8-byte boundary.  Note
	 * we do not need to do the alignment as ntfs_attr_record_make_space()
	 * does it anyway.
	 */
	/*
	 * The current implementation of ntfs_attr_lookup() will always return
	 * pointing into the base mft record when an attribute is not found.
	 */
	base_m = ctx->m;
retry:
	if (ni != ctx->ni)
		panic("%s(): ni != ctx->ni\n", __FUNCTION__);
	m = ctx->m;
	a = ctx->a;
	err = ntfs_attr_record_make_space(m, a, val_ofs + val_len);
	if (err) {
		ntfs_inode *eni;

		if (err != ENOSPC)
			panic("%s(): err != ENOSPC\n", __FUNCTION__);
		/*
		 * There was not enough space in the mft record to insert the
		 * new attribute record which means we will need to insert it
		 * into an extent mft record.
		 *
		 * To avoid bugs and impossible situations, check that the
		 * attribute is not already the only attribute in the mft
		 * record otherwise moving it would not give us anything.
		 */
		if (ntfs_attr_record_is_only_one(m, a))
			panic("%s(): ntfs_attr_record_is_only_one(m, a)\n",
					__FUNCTION__);
		/*
		 * Before we can allocate an extent mft record, we need to
		 * ensure that the inode has an attribute list attribute.
		 */
		if (!NInoAttrList(ni)) {
			err = ntfs_attr_list_add(ni, m, NULL);
			if (err) {
				ntfs_error(vol->mp, "Failed to add attribute "
						"list attribute to mft_no "
						"0x%llx (error %d).",
						(unsigned long long)ni->mft_no,
						err);
				return err;
			}
			/*
			 * Adding the attribute list attribute may have
			 * generated enough space in the base mft record to
			 * fit the attribute so try again.
			 */
			ntfs_attr_search_ctx_reinit(ctx);
			err = ntfs_attr_lookup(type, name, name_len, 0, val,
					val_len, ctx);
			if (err == ENOENT) {
				/*
				 * The current implementation of
				 * ntfs_attr_lookup() will always return
				 * pointing into the base mft record when an
				 * attribute is not found.
				 */
				if (m != ctx->m)
					panic("%s(): m != ctx->m\n",
							__FUNCTION__);
				goto retry;
			}
			/*
			 * We cannot have found the attribute as we have
			 * exclusive access and know that it does not exist
			 * already.
			 */
			if (!err)
				panic("%s(): !err\n", __FUNCTION__);
			/*
			 * Something has gone wrong.  Note we have to bail out
			 * as a failing attribute lookup indicates corruption
			 * and/or disk failure and/or not enough memory all of
			 * which would prevent us from rolling back the
			 * attribute list attribute addition.
			 */
			ntfs_error(vol->mp, "Failed to add attribute type "
					"0x%x to mft_no 0x%llx because looking "
					"up the attribute failed (error %d).",
					(unsigned)le32_to_cpu(type),
					(unsigned long long)ni->mft_no, -err);
			return err;
		}
		/*
		 * We now need to allocate a new extent mft record, attach it
		 * to the base ntfs inode and set up the search context to
		 * point to it, then insert the new attribute into it.
		 */
		err = ntfs_mft_record_alloc(vol, NULL, NULL, ni, &eni, &m, &a);
		if (err) {
			ntfs_error(vol->mp, "Failed to add attribute type "
					"0x%x to mft_no 0x%llx because "
					"allocating a new extent mft record "
					"failed (error %d).",
					(unsigned)le32_to_cpu(type),
					(unsigned long long)ni->mft_no, err);
			/*
			 * If we added the attribute list attribute above we
			 * now remove it again.  This may require moving
			 * attributes back into the base mft record so is not a
			 * trivial amount of work and in the end it does not
			 * really matter if we leave an inode with an attribute
			 * list attribute that does not really need it.  So it
			 * will only be removed if there are no extent mft
			 * records at all, i.e. if adding the attribute list
			 * attribute did not cause any attribute records to be
			 * moved out to extent mft records.
			 */
			al_entry_added = FALSE;
			al_entry = NULL;
			goto remove_al;
		}
		ctx->m = m;
		ctx->a = a;
		ctx->ni = eni;
		/*
		 * Make space for the new attribute.  This cannot fail as we
		 * now have an empty mft record which by definition can hold
		 * a maximum size resident attribute record.
		 */
		err = ntfs_attr_record_make_space(m, a, val_ofs + val_len);
		if (err)
			panic("%s(): err (ntfs_attr_record_make_space())\n",
					__FUNCTION__);
	}
	/*
	 * Now setup the new attribute record.  The entire attribute has been
	 * zeroed and the length of the attribute record has been set up by
	 * ntfs_attr_record_make_space().
	 */
	a->type = type;
	a->name_length = name_len;
	a->name_offset = const_cpu_to_le16(offsetof(ATTR_RECORD, reservedR) +
			sizeof(a->reservedR));
	a->instance = m->next_attr_instance;
	/*
	 * Increment the next attribute instance number in the mft record as we
	 * consumed the old one.
	 */
	m->next_attr_instance = cpu_to_le16(
			(le16_to_cpu(m->next_attr_instance) + 1) & 0xffff);
	a->value_length = cpu_to_le32(val_len);
	a->value_offset = cpu_to_le16(val_ofs);
	if (type == AT_FILENAME)
		a->resident_flags = RESIDENT_ATTR_IS_INDEXED;
	/* Copy the attribute name into place. */
	if (name_len)
		memcpy((u8*)a + name_ofs, name,
				name_len << NTFSCHAR_SIZE_SHIFT);
	/* If a value is specified, copy it into place. */
	if (val) {
		memcpy((u8*)a + le16_to_cpu(a->value_offset), val, val_len);
		/*
		 * Ensure the mft record containing the new filename attribute
		 * gets written out.
		 */
		if (ctx->ni != ni)
			NInoSetMrecNeedsDirtying(ctx->ni);
	}
	/*
	 * If the inode does not use the attribute list attribute we are done.
	 *
	 * If the inode uses the attribute list attribute (including the case
	 * where we just created it), we need to add an attribute list
	 * attribute entry for the attribute.
	 */
	if (!NInoAttrList(ni))
		goto done;
	/* Add an attribute list attribute entry for the inserted attribute. */
	al_entry = ctx->al_entry;
	al_entry_used = offsetof(ATTR_LIST_ENTRY, name) +
			(name_len << NTFSCHAR_SIZE_SHIFT);
	al_entry_len = (al_entry_used + 7) & ~7;
	new_al_size = ni->attr_list_size + al_entry_len;
	/* Out of bounds checks. */
	if ((u8*)al_entry < ni->attr_list ||
			(u8*)al_entry > ni->attr_list + new_al_size ||
			(u8*)al_entry + al_entry_len >
			ni->attr_list + new_al_size) {
		/* Inode is corrupt. */
		ntfs_error(vol->mp, "Mft_no 0x%llx is corrupt.  Run chkdsk.",
				(unsigned long long)ni->mft_no);
		err = EIO;
		goto undo;
	}
	err = ntfs_attr_size_bounds_check(vol, AT_ATTRIBUTE_LIST, new_al_size);
	if (err) {
		if (err == ERANGE) {
			ntfs_error(vol->mp, "Cannot insert attribute into "
					"mft_no 0x%llx because the attribute "
					"list attribute would become too "
					"large.  You need to defragment your "
					"volume and then try again.",
					(unsigned long long)ni->mft_no);
			err = ENOSPC;
		} else {
			ntfs_error(vol->mp, "Attribute list attribute is "
					"unknown on the volume.  The volume "
					"is corrupt.  Run chkdsk.");
			NVolSetErrors(vol);
			err = EIO;
		}
		goto undo;
	}
	/*
	 * Reallocate the memory buffer if needed and create space for the new
	 * entry.
	 */
	new_al_alloc = (new_al_size + NTFS_ALLOC_BLOCK - 1) &
			~(NTFS_ALLOC_BLOCK - 1);
	if (new_al_alloc > ni->attr_list_alloc) {
		u8 *tmp, *al, *al_end;
		unsigned al_entry_ofs;

		tmp = OSMalloc(new_al_alloc, ntfs_malloc_tag);
		if (!tmp) {
			ntfs_error(vol->mp, "Not enough memory to extend "
					"attribute list attribute of mft_no "
					"0x%llx.",
					(unsigned long long)ni->mft_no);
			err = ENOMEM;
			goto undo;
		}
		al = ni->attr_list;
		al_entry_ofs = (u8*)al_entry - al;
		al_end = al + ni->attr_list_size;
		memcpy(tmp, al, al_entry_ofs);
		if ((u8*)al_entry < al_end)
			memcpy(tmp + al_entry_ofs + al_entry_len,
					al + al_entry_ofs,
					ni->attr_list_size - al_entry_ofs);
		al_entry = ctx->al_entry = (ATTR_LIST_ENTRY*)(tmp +
				al_entry_ofs);
		OSFree(ni->attr_list, ni->attr_list_alloc, ntfs_malloc_tag);
		ni->attr_list_alloc = new_al_alloc;
		ni->attr_list = tmp;
	} else if ((u8*)al_entry < ni->attr_list + ni->attr_list_size)
		memmove((u8*)al_entry + al_entry_len, al_entry,
				ni->attr_list_size - ((u8*)al_entry -
				ni->attr_list));
	ni->attr_list_size = new_al_size;
	/* Set up the attribute list entry. */
	al_entry->type = type;
	al_entry->length = cpu_to_le16(al_entry_len);
	al_entry->name_length = name_len;
	al_entry->name_offset = offsetof(ATTR_LIST_ENTRY, name);
	al_entry->lowest_vcn = 0;
	al_entry->mft_reference = MK_LE_MREF(ctx->ni->mft_no, ctx->ni->seq_no);
	al_entry->instance = a->instance;
	/* Copy the attribute name into place. */
	if (name_len)
		memcpy((u8*)&al_entry->name, name,
				name_len << NTFSCHAR_SIZE_SHIFT);
	/* For tidyness, zero any unused space. */
	if (al_entry_len != al_entry_used) {
		if (al_entry_len < al_entry_used)
			panic("%s(): al_entry_len < al_entry_used\n",
					__FUNCTION__);
		memset((u8*)al_entry + al_entry_used, 0,
				al_entry_len - al_entry_used);
	}
	/*
	 * Extend the attribute list attribute and copy in the modified
	 * value from the cache.
	 */
	err = ntfs_attr_list_sync_extend(ni, base_m,
			(u8*)al_entry - ni->attr_list, ctx);
	if (err) {
		ntfs_error(vol->mp, "Failed to extend attribute list "
				"attribute of mft_no 0x%llx (error %d).",
				(unsigned long long)ni->mft_no, err);
		al_entry_added = TRUE;
		goto undo_al;
	}
done:
	ntfs_debug("Done.");
	return 0;
undo:
	al_entry_added = FALSE;
undo_al:
	/*
	 * Need to remove the attribute again or free the extent mft record if
	 * there are no attributes remaining in it.
	 */
	if (m == base_m || !ntfs_attr_record_is_only_one(m, a)) {
		ntfs_attr_record_delete_internal(m, a);
		/*
		 * If the attribute was not in the base mft record mark the
		 * extent mft record dirty so it gets written out later.  If
		 * the attribute was in the base mft record it will be marked
		 * dirty later.
		 *
		 * We also unmap the extent mft record and we set @ctx->ni to
		 * equal the base inode @ni so that the search context is
		 * initialized from scratch or simply freed if the caller
		 * reinitializes or releases the search context respectively.
		 */
		if (m != base_m) {
			NInoSetMrecNeedsDirtying(ctx->ni);
			ntfs_extent_mft_record_unmap(ctx->ni);
			ctx->ni = ni;
		}
	} else {
		int err2;
		BOOL al_needed;

		err2 = ntfs_extent_mft_record_free(ni, ctx->ni, m);
		if (err2) {
			/*
			 * Ignore the error as we just end up with an unused
			 * mft record that is marked in use.
			 */
			ntfs_error(vol->mp, "Failed to free extent mft_no "
					"0x%llx (error %d).  Unmount and run "
					"chkdsk to recover the lost inode.",
					(unsigned long long)ctx->ni->mft_no,
					err2);
			NVolSetErrors(vol);
			/*
			 * Relese the extent mft record after dirtying it thus
			 * simulating the effect of freeing it.
			 */
			NInoSetMrecNeedsDirtying(ctx->ni);
			ntfs_extent_mft_record_unmap(ctx->ni);
		}
		/*
		 * The attribute search context still points to the no longer
		 * mapped extent inode thus we need to change it to point to
		 * the base inode instead so the context can be reinitialized
		 * or released safely.
		 */
		ctx->ni = ni;
remove_al:
		/*
		 * Check the attribute list attribute.  If there are no other
		 * attribute list attribute entries referencing extent mft
		 * records delete the attribute list attribute altogether.
		 *
		 * If this fails it does not matter as we simply retain the
		 * attribute list attribute so we ignore the error and go on to
		 * delete the attribute list attribute entry instead.
		 *
		 * If there are other attribute list attribute entries
		 * referencing extent mft records we still need the attribute
		 * list attribute thus we go on to delete the attribute list
		 * entry corresponding to the attribute record we just deleted
		 * by freeing its extent mft record.
		 */
		err2 = ntfs_attr_list_is_needed(ni,
				al_entry_added ? al_entry : NULL, &al_needed);
		if (err2)
			ntfs_warning(vol->mp, "Failed to determine if "
					"attribute list attribute of mft_no "
					"0x%llx if still needed (error %d).  "
					"Assuming it is still needed and "
					"continuing.",
					(unsigned long long)ni->mft_no, err2);
		else if (!al_needed) {
			/*
			 * No more extent mft records are in use.  Delete the
			 * attribute list attribute.
			 */
			ntfs_attr_search_ctx_reinit(ctx);
			err2 = ntfs_attr_list_delete(ni, ctx);
			if (!err2) {
				/*
				 * We deleted the attribute list attribute and
				 * this will have updated the base inode
				 * appropriately thus we have restored
				 * everything as it was before.
				 */
				return err;
			}
			ntfs_warning(vol->mp, "Failed to delete attribute "
					"list attribute of mft_no 0x%llx "
					"(error %d).  Continuing using "
					"alternative error recovery method.",
					(unsigned long long)ni->mft_no, err2);
		}
	}
	/*
	 * Both @ctx and @ni are now invalid and cannot be used any more which
	 * is fine as we have finished dealing with the attribute record.
	 *
	 * We now need to delete the corresponding attribute list attribute
	 * entry if we created it.
	 *
	 * Then we need to rewrite the attribute list attribute again because
	 * ntfs_attr_list_sync_extend() may have left it in an indeterminate
	 * state.
	 */
	if (al_entry_added) {
		int err2;

		ntfs_attr_list_entry_delete(ni, al_entry);
		ntfs_attr_search_ctx_reinit(ctx);
		err2 = ntfs_attr_list_sync_shrink(ni, 0, ctx);
		if (err2) {
			ntfs_error(vol->mp, "Failed to restore attribute list "
					"attribute in base mft_no 0x%llx "
					"(error %d).  Leaving inconsistent "
					"metadata.  Unmount and run chkdsk.",
					(unsigned long long)ni->mft_no, err2);
			NVolSetErrors(vol);
		}
	}
	/* Make sure any changes are written out. */
	NInoSetMrecNeedsDirtying(ni);
	return err;
}

/**
 * ntfs_resident_attr_value_resize - resize the value of a resident attribute
 * @m:		mft record containing attribute record
 * @a:		attribute record whose value to resize
 * @new_size:	new size in bytes to which to resize the attribute value of @a
 *
 * Resize the value of the attribute @a in the mft record @m to @new_size
 * bytes.  If the value is made bigger, the newly allocated space is cleared.
 *
 * Return 0 on success and errno on error.  The following error codes are
 * defined:
 *	ENOSPC	- Not enough space in the mft record @m to perform the resize.
 *
 * Note: On error, no modifications have been performed whatsoever.
 *
 * Warning: If you make a record smaller without having copied all the data you
 *	    are interested in the data may be overwritten.
 */
errno_t ntfs_resident_attr_value_resize(MFT_RECORD *m, ATTR_RECORD *a,
		const u32 new_size)
{
	const u32 old_size = le32_to_cpu(a->value_length);

	/* Resize the resident part of the attribute record. */
	if (ntfs_attr_record_resize(m, a, le16_to_cpu(a->value_offset) +
			new_size))
		return ENOSPC;
	/*
	 * The resize succeeded!  If we made the attribute value bigger, clear
	 * the area between the old size and @new_size.
	 */
	if (new_size > old_size)
		bzero((u8*)a + le16_to_cpu(a->value_offset) + old_size,
				new_size - old_size);
	/* Finally update the length of the attribute value. */
	a->value_length = cpu_to_le32(new_size);
	return 0;
}

/**
 * ntfs_attr_make_non_resident - convert a resident to a non-resident attribute
 * @ni:		ntfs inode describing the attribute to convert
 *
 * Convert the resident ntfs attribute described by the ntfs inode @ni to a
 * non-resident one.
 *
 * Return 0 on success and errno on error.  The following error return codes
 * are defined:
 *	EPERM	- The attribute is not allowed to be non-resident.
 *	ENOMEM	- Not enough memory.
 *	ENOSPC	- Not enough disk space.
 *	EINVAL	- Attribute not defined on the volume.
 *	EIO	- I/o error or other error.
 *
 * Note that if an error other than EPERM is returned it is possible that the
 * attribute has been made non-resident but for example the attribute list
 * attribute failed to be written out thus the base mft record is now corrupt
 * and all operations should be aborted by the caller.
 *
 * Locking: The caller must hold @ni->lock on the inode for writing.
 */
errno_t ntfs_attr_make_non_resident(ntfs_inode *ni)
{
	leMFT_REF mref;
	s64 new_size, data_size;
	ntfs_volume *vol = ni->vol;
	ntfs_inode *base_ni;
	MFT_RECORD *base_m, *m;
	ATTR_RECORD *a;
	upl_t upl;
	upl_page_info_array_t pl;
	u8 *kaddr, *al_end;
	unsigned mp_size, mp_ofs, name_ofs, arec_size, attr_size, bytes_needed;
	unsigned al_ofs = 0;
	errno_t err, err2;
	le32 type;
	u8 old_res_attr_flags;
	ntfs_attr_search_ctx ctx, actx;
	BOOL al_dirty = FALSE;

	/* Check that the attribute is allowed to be non-resident. */
	err = ntfs_attr_can_be_non_resident(vol, ni->type);
	if (err) {
		if (err == EPERM)
			ntfs_debug("Attribute is not allowed to be "
					"non-resident.");
		else
			ntfs_debug("Attribute not defined on the NTFS "
					"volume!");
		return err;
	}
	/*
	 * FIXME: Compressed and encrypted attributes are not supported when
	 * writing and we should never have gotten here for them.
	 */
	if (NInoCompressed(ni))
		panic("%s(): NInoCompressed(ni)\n", __FUNCTION__);
	if (NInoEncrypted(ni))
		panic("%s(): NInoEncrypted(ni)\n", __FUNCTION__);
	/*
	 * The size needs to be aligned to a cluster boundary for allocation
	 * purposes.
	 */
	lck_spin_lock(&ni->size_lock);
	data_size = ni->data_size;
	lck_spin_unlock(&ni->size_lock);
	new_size = (data_size + vol->cluster_size_mask) &
			~vol->cluster_size_mask;
	lck_rw_lock_exclusive(&ni->rl.lock);
	if (ni->rl.elements)
		panic("%s(): ni->rl.elements\n", __FUNCTION__);
	upl = NULL;
	if (new_size > 0) {
		/* Start by allocating clusters to hold the attribute value. */
		err = ntfs_cluster_alloc(vol, 0, new_size >>
				vol->cluster_size_shift, -1, DATA_ZONE, TRUE,
				&ni->rl);
		if (err) {
			if (err != ENOSPC)
				ntfs_error(vol->mp, "Failed to allocate "
						"cluster%s, error code %d.",
						(new_size >>
						vol->cluster_size_shift) > 1 ?
						"s" : "", err);
			goto unl_err;
		}
		/*
		 * Will need the page later and since the page lock nests
		 * outside all ntfs locks, we need to get the page now.
		 */
		err = ntfs_page_grab(ni, 0, &upl, &pl, &kaddr, TRUE);
		if (err)
			goto page_err;
	}
	/* Determine the size of the mapping pairs array. */
	err = ntfs_get_size_for_mapping_pairs(vol,
			ni->rl.elements ? ni->rl.rl : NULL, 0, -1, &mp_size);
	if (err) {
		ntfs_error(vol->mp, "Failed to get size for mapping pairs "
				"array (error %d).", err);
		goto rl_err;
	}
	base_ni = ni;
	if (NInoAttr(ni))
		base_ni = ni->base_ni;
	err = ntfs_mft_record_map(base_ni, &base_m);
	if (err)
		goto rl_err;
	ntfs_attr_search_ctx_init(&ctx, base_ni, base_m);
	err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len, 0, NULL, 0,
			&ctx);
	if (err) {
		if (err == ENOENT)
			err = EIO;
		goto unm_err;
	}
	m = ctx.m;
	a = ctx.a;
	if (NInoNonResident(ni))
		panic("%s(): NINonResident(ni)\n", __FUNCTION__);
	if (a->non_resident)
		panic("%s(): a->non_resident\n", __FUNCTION__);
	/* Calculate new offsets for the name and the mapping pairs array. */
	name_ofs = offsetof(ATTR_REC, compressed_size);
	if (NInoSparse(ni) || NInoCompressed(ni))
		name_ofs += sizeof(a->compressed_size);
	mp_ofs = (name_ofs + a->name_length * sizeof(ntfschar) + 7) & ~7;
	/*
	 * Determine the size of the resident part of the now non-resident
	 * attribute record.
	 */
	arec_size = (mp_ofs + mp_size + 7) & ~7;
	/*
	 * If the page is not uptodate bring it uptodate by copying from the
	 * attribute value.
	 */
	attr_size = le32_to_cpu(a->value_length);
	if (attr_size != data_size)
		panic("%s(): attr_size != data_size\n", __FUNCTION__);
	if (upl && !upl_valid_page(pl, 0)) {
		memcpy(kaddr, (u8*)a + le16_to_cpu(a->value_offset),
				attr_size);
		bzero(kaddr + attr_size, PAGE_SIZE - attr_size);
	}
	/* Backup the attribute flags. */
	old_res_attr_flags = a->resident_flags;
retry_resize:
	/* Resize the resident part of the attribute record. */
	err = ntfs_attr_record_resize(m, a, arec_size);
	if (!err) {
		al_ofs = 0;
		goto do_switch;
	}
	if (err != ENOSPC)
		panic("%s(): err != ENOSPC\n", __FUNCTION__);
	/*
	 * The attribute record size required cannot be larger than the amount
	 * of space in an mft record.
	 */
	if (arec_size > le32_to_cpu(m->bytes_allocated) -
			le16_to_cpu(m->attrs_offset))
		panic("%s(): arec_size > le32_to_cpu(m->bytes_allocated) - "
				"le16_to_cpu(m->attrs_offset)\n",
				__FUNCTION__);
	/*
	 * To make space in the mft record we would like to try to make other
	 * attributes non-resident if that would save space.
	 *
	 * FIXME: We cannot do this at present unless the attribute is the
	 * attribute being resized as there could be an ntfs inode matching
	 * this attribute in memory and it would become out of date with its
	 * metadata if we touch its attribute record.
	 *
	 * FIXME: We do not need to do this if this is the attribute being
	 * resized as we already tried to make the attribute non-resident and
	 * it did not work or we would never have gotten here in the first
	 * place.
	 *
	 * Thus we have to either move other attributes to extent mft records
	 * thus making more space in the base mft record or we have to move the
	 * attribute being resized to an extent mft record thus giving it more
	 * space.  In any case we need to have an attribute list attribute so
	 * start by adding it if it does not yet exist.
	 *
	 * If the addition succeeds but the remapping of the extent mft record
	 * fails (i.e. the !err && IS_ERR(ctx.m) case below) we bail out
	 * without trying to remove the attribute list attribute because to do
	 * so we would have to map the extent mft record in order to move the
	 * attribute(s) in it back into the base mft record and we know the
	 * mapping just failed so it is unlikely to succeed now.  In any case
	 * the metadata is consistent we just cannot make further progress.
	 */
	if (!NInoAttrList(base_ni)) {
		err = ntfs_attr_list_add(base_ni, base_m, &ctx);
		if (err || ctx.is_error) {
			if (!err)
				err = ctx.error;
			ntfs_error(vol->mp, "Failed to %s mft_no 0x%llx (error "
					"%d).", ctx.is_error ?
					"remap extent mft record of" :
					"add attribute list attribute to",
					(unsigned long long)base_ni->mft_no,
					err);
			goto unm_err;
		}
		/*
		 * The attribute location will have changed so update it from
		 * the search context.
		 */
		m = ctx.m;
		a = ctx.a;
		/*
		 * Check that the logic in ntfs_attr_list_add() has not changed
		 * without the code here being updated.  At present it will
		 * never make resident attributes non-resident.
		 */
		if (a->non_resident)
			panic("%s(): a->non_resident\n", __FUNCTION__);
		/*
		 * We now have an attribute list attribute.  This may have
		 * caused the attribute to be made non-resident to be moved out
		 * to an extent mft record in which case there would now be
		 * enough space to resize the attribute record.
		 *
		 * Alternatively some other large attribute may have been moved
		 * out to an extent mft record thus generating enough space in
		 * the base mft record for the attribute to be made
		 * non-resident.
		 *
		 * In either case we simply want to retry the resize.
		 */
		goto retry_resize;
	}
	/*
	 * We now know we have an attribute list attribute and that we still do
	 * not have enough space to make the attribute non-resident.
	 *
	 * As discussed above we need to start moving attributes out of the
	 * base mft record to make enough space.
	 *
	 * Note that if the attribute to be made non-resident had been moved
	 * out of the base mft record we would then have had enough space for
	 * the resize thus we would never have gotten here.  We detect this
	 * case and BUG() in case we change the logic in ntfs_attr_list_add()
	 * some day to remind us to update the code here to match.
	 */
	if (ctx.ni != base_ni)
		panic("%s(): ctx.ni != base_ni\n", __FUNCTION__);
	/*
	 * If this is the only attribute record in the mft record we cannot
	 * gain anything by moving it or anything else.  This really cannot
	 * happen as we ensure above that the attribute is in the base mft
	 * record.
	 */
	if (ntfs_attr_record_is_only_one(m, a))
		panic("%s(): ntfs_attr_record_is_only_one(m, a)\n",
				__FUNCTION__);
	/*
	 * If the attribute to be resized is the standard information, index
	 * root, or unnamed $DATA attribute try to move other attributes out
	 * into extent mft records.  If none of these then move the attribute
	 * to be resized out to an extent mft record.
	 */
	type = ni->type;
	if (type != AT_STANDARD_INFORMATION && type != AT_INDEX_ROOT &&
			(type != AT_DATA || ni->name_len)) {
		lck_rw_lock_shared(&base_ni->attr_list_rl.lock);
		err = ntfs_attr_record_move(&ctx);
		lck_rw_unlock_shared(&base_ni->attr_list_rl.lock);
		if (!err) {
			/* The attribute has moved so update our variables. */
			m = ctx.m;
			a = ctx.a;
			/* The resize will now succeed. */
			goto retry_resize;
		}
		ntfs_error(vol->mp, "Failed to move attribute type 0x%x out "
				"of base mft_no 0x%llx into an extent mft "
				"record (error %d).", le32_to_cpu(type),
				base_ni->mft_no, err);
		goto unm_err;
	}
	type = AT_UNUSED;
	/*
	 * The number of free bytes needed in the mft record so the resize can
	 * succeed.
	 */
	bytes_needed = arec_size - le32_to_cpu(a->length);
	/*
	 * The MFT reference of the mft record in which the attribute to be
	 * made non-resident is located.
	 */
	mref = MK_LE_MREF(base_ni->mft_no, base_ni->seq_no);
	al_ofs = base_ni->attr_list_size;
	al_end = base_ni->attr_list + al_ofs;
next_pass:
	ntfs_attr_search_ctx_init(&actx, base_ni, base_m);
	actx.is_iteration = 1;
	do {
		ntfschar *a_name;
		ATTR_LIST_ENTRY *al_entry;

		/* Get the next attribute in the mft record. */
		err = ntfs_attr_find_in_mft_record(type, NULL, 0, NULL, 0,
				&actx);
		if (err) {
			if (err == ENOENT) {
				/*
				 * If we have more passes to go do the next
				 * pass which will try harder to move things
				 * out of the way.
				 */
				if (type == AT_UNUSED) {
					type = AT_DATA;
					goto next_pass;
				}
				/*
				 * TODO: Need to get these cases triggered and
				 * then need to run chkdsk to check for
				 * validity of moving these attributes out of
				 * the base mft record.
				 */
				if (type == AT_DATA) {
					type = AT_INDEX_ROOT;
					goto next_pass;
				}
				if (type == AT_INDEX_ROOT) {
					type = AT_STANDARD_INFORMATION;
					goto next_pass;
				}
				/*
				 * We can only get here when the attribute to
				 * be made non-resident is the standard
				 * information attribute and for some reason it
				 * does not exist in the mft record.  That can
				 * only happen with some sort of corruption or
				 * due to a bug.
				 */
				ntfs_error(vol->mp, "Standard information "
						"attribute is missing from "
						"mft_no 0x%llx.  Run chkdsk.",
						(unsigned long long)
						base_ni->mft_no);
				err = EIO;
				NVolSetErrors(vol);
				goto unm_err;
			}
			ntfs_error(vol->mp, "Failed to iterate over attribute "
					"records in base mft record 0x%llx "
					"(error %d).",
					(unsigned long long)base_ni->mft_no,
					err);
			goto unm_err;
		}
		a = actx.a;
		if (type == AT_UNUSED) {
			/*
			 * Skip the attribute list attribute itself as that is
			 * not represented inside itself and we cannot move it
			 * out anyway.
			 *
			 * Also, do not touch standard information, index root,
			 * and unnamed $DATA attributes.  They will be moved
			 * out to extent mft records in later passes if really
			 * necessary.
			 */
			if (a->type == AT_ATTRIBUTE_LIST ||
					a->type == AT_STANDARD_INFORMATION ||
					a->type == AT_INDEX_ROOT ||
					(a->type == AT_DATA &&
					!a->name_length))
				continue;
		}
		/*
		 * Move the attribute out to an extent mft record and update
		 * its attribute list entry.
		 *
		 * But first find the attribute list entry matching the
		 * attribute record so it can be updated.
		 */
		a_name = (ntfschar*)((u8*)a + le16_to_cpu(a->name_offset));
		al_entry = (ATTR_LIST_ENTRY*)base_ni->attr_list;
		do {
			/*
			 * The attribute must be present in the attribute list
			 * attribute or something is corrupt.
			 */
			if ((u8*)al_entry >= al_end || !al_entry->length) {
				ntfs_error(vol->mp, "Attribute type 0x%x not "
						"found in attribute list "
						"attribute of base mft record "
						"0x%llx.  Run chkdsk.",
						(unsigned)le32_to_cpu(a->type),
						(unsigned long long)
						base_ni->mft_no);
				NVolSetErrors(vol);
				err = EIO;
				goto unm_err;
			}
			if (al_entry->mft_reference == mref &&
					al_entry->instance == a->instance) {
				/*
				 * We found the entry, stop looking but first
				 * perform a quick sanity check that we really
				 * do have the correct attribute record.
				 */
				if (al_entry->type == a->type &&
						ntfs_are_names_equal(
						(ntfschar*)((u8*)al_entry +
						al_entry->name_offset),
						al_entry->name_length, a_name,
						a->name_length, TRUE,
						vol->upcase, vol->upcase_len))
					break;
				ntfs_error(vol->mp, "Found corrupt attribute "
						"list attribute when looking "
						"for attribute type 0x%x in "
						"attribute list attribute of "
						"base mft record 0x%llx.  Run "
						"chkdsk.",
						(unsigned)le32_to_cpu(a->type),
						(unsigned long long)
						base_ni->mft_no);
				NVolSetErrors(vol);
				err = EIO;
				goto unm_err;
			}
			/* Go to the next attribute list entry. */
			al_entry = (ATTR_LIST_ENTRY*)((u8*)al_entry +
					le16_to_cpu(al_entry->length));
		} while (1);
		/* Finally, move the attribute to an extent record. */
		err = ntfs_attr_record_move_for_attr_list_attribute(&actx,
				al_entry, &ctx, NULL);
		if (err) {
			ntfs_error(vol->mp, "Failed to move attribute type "
					"0x%x out of base mft record 0x%llx "
					"and into an extent mft record (error "
					"%d).  Run chkdsk.",
					(unsigned)le32_to_cpu(a->type),
					(unsigned long long)base_ni->mft_no,
					err);
			NVolSetErrors(vol);
			goto unm_err;
		}
		/*
		 * If the modified attribute list entry is before the current
		 * start of attribute list modification we need to sync this
		 * entry as well.  For simplicity we just set @al_ofs to the
		 * new value thus syncing everything starting at that offset.
		 */
		if ((u8*)al_entry - base_ni->attr_list < (long)al_ofs) {
			al_ofs = (u8*)al_entry - base_ni->attr_list;
			al_dirty = TRUE;
		}
		/*
		 * If we moved the attribute to be made non-resident we will
		 * now have enough space so retry the resize.
		 */
		if (ctx.ni != base_ni) {
			/*
			 * @ctx is not in the base mft record, map the extent
			 * inode it is in and if it is mapped at a different
			 * address than before update the pointers in @ctx.
			 */
retry_map:
			err = ntfs_mft_record_map(ctx.ni, &m);
			if (err) {
				/*
				 * Something bad has happened.  If out of
				 * memory retry till it succeeds.  Any other
				 * errors are fatal and we have to abort.
				 *
				 * We do not need to undo anything as the
				 * metadata is self-consistent except for the
				 * attribute list attribute which we need to
				 * write out.
				 */
				if (err == ENOMEM) {
					(void)thread_block(
							THREAD_CONTINUE_NULL);
					goto retry_map;
				}
				ctx.ni = base_ni;
				goto unm_err;
			}
			if (ctx.m != m) {
				ctx.a = (ATTR_RECORD*)((u8*)m +
						((u8*)ctx.a - (u8*)ctx.m));
				ctx.m = m;
			}
			a = ctx.a;
			goto retry_resize;
		}
		/* If we now have enough space retry the resize. */
		if (bytes_needed > le32_to_cpu(m->bytes_allocated) -
				le32_to_cpu(m->bytes_in_use)) {
			a = ctx.a;
			goto retry_resize;
		}
	} while (1);
do_switch:
	/*
	 * Convert the resident part of the attribute record to describe a
	 * non-resident attribute.
	 */
	a->non_resident = 1;
	/* Move the attribute name if it exists and update the offset. */
	if (a->name_length)
		memmove((u8*)a + name_ofs,
				(u8*)a + le16_to_cpu(a->name_offset),
				a->name_length * sizeof(ntfschar));
	a->name_offset = cpu_to_le16(name_ofs);
	/* Setup the fields specific to non-resident attributes. */
	a->lowest_vcn = 0;
	a->highest_vcn = cpu_to_sle64((new_size - 1) >>
			vol->cluster_size_shift);
	a->mapping_pairs_offset = cpu_to_le16(mp_ofs);
	bzero(&a->reservedN, sizeof(a->reservedN));
	a->allocated_size = cpu_to_sle64(new_size);
	a->data_size = a->initialized_size = cpu_to_sle64(attr_size);
	a->compression_unit = 0;
	if (NInoSparse(ni) || NInoCompressed(ni)) {
		if (NInoCompressed(ni) || vol->major_ver <= 1)
			a->compression_unit = NTFS_COMPRESSION_UNIT;
		a->compressed_size = a->allocated_size;
	}
	/*
	 * Generate the mapping pairs array into the attribute record.
	 *
	 * This cannot fail as we have already checked the size we need to
	 * build the mapping pairs array.
	 */
	err = ntfs_mapping_pairs_build(vol, (s8*)a + mp_ofs, arec_size - mp_ofs,
			ni->rl.elements ? ni->rl.rl : NULL, 0, -1, NULL);
	if (err)
		panic("%s(): err\n", __FUNCTION__);
	/* Setup the in-memory attribute structure to be non-resident. */
	lck_spin_lock(&ni->size_lock);
	ni->allocated_size = new_size;
	if (NInoSparse(ni) || NInoCompressed(ni)) {
		ni->compressed_size = ni->allocated_size;
		if (a->compression_unit) {
			ni->compression_block_size = 1U <<
					(a->compression_unit +
					vol->cluster_size_shift);
			ni->compression_block_size_shift =
					ffs(ni->compression_block_size) - 1;
			ni->compression_block_clusters = 1U <<
					a->compression_unit;
		} else {
			ni->compression_block_size = 0;
			ni->compression_block_size_shift = 0;
			ni->compression_block_clusters = 0;
		}
	}
	lck_spin_unlock(&ni->size_lock);
	/*
	 * This needs to be last since we are not allowed to fail once we flip
	 * this switch.
	 */
	NInoSetNonResident(ni);
	/* Mark the mft record dirty, so it gets written back. */
	NInoSetMrecNeedsDirtying(ctx.ni);
	if (ctx.ni != base_ni)
		ntfs_extent_mft_record_unmap(ctx.ni);
	if (al_dirty) {
		ntfs_attr_search_ctx_reinit(&actx);
		err = ntfs_attr_list_sync(base_ni, al_ofs, &actx);
		if (err) {
			ntfs_error(vol->mp, "Failed to write attribute list "
					"attribute of mft_no 0x%llx (error "
					"%d).  Leaving corrupt metadata.  Run "
					"chkdsk.",
					(unsigned long long)base_ni->mft_no,
					err);
			NVolSetErrors(vol);
		}
		/* Mark the base mft record dirty, so it gets written back. */
		NInoSetMrecNeedsDirtying(base_ni);
	}
	ntfs_mft_record_unmap(base_ni);
	lck_rw_unlock_exclusive(&ni->rl.lock);
	/*
	 * We have modified the allocated size.  If the ntfs inode is the base
	 * inode, cause the sizes to be written to all the directory index
	 * entries pointing to the base inode when the inode is written to
	 * disk.  Do not do this for directories as they have both sizes set to
	 * zero in their index entries.
	 */
	if (ni == base_ni && !S_ISDIR(ni->mode))
		NInoSetDirtySizes(ni);
	if (upl)
		ntfs_page_unmap(ni, upl, pl, TRUE);
	ntfs_debug("Done.");
	return 0;
unm_err:
	if (ctx.ni != base_ni) {
		NInoSetMrecNeedsDirtying(ctx.ni);
		ntfs_extent_mft_record_unmap(ctx.ni);
	}
	if (al_dirty) {
		ntfs_attr_search_ctx_reinit(&actx);
		err2 = ntfs_attr_list_sync(base_ni, al_ofs, &actx);
		if (err2) {
			ntfs_error(vol->mp, "Failed to write attribute list "
					"attribute in error code path (error "
					"%d).  Leaving corrupt metadata.  Run "
					"chkdsk.", err2);
			NVolSetErrors(vol);
		}
	}
	NInoSetMrecNeedsDirtying(base_ni);
	ntfs_mft_record_unmap(base_ni);
rl_err:
	if (upl) {
		/*
		 * If the page was valid release it back to the VM.  If it was
		 * not valid throw it away altogether.
		 * TODO: We could wrap this up in a ntfs_page_unmap_ext()
		 * function which takes an extra parameter to specify whether
		 * to keep the page or to dump it if it is invalid...
		 */
		if (upl_valid_page(pl, 0))
			ntfs_page_unmap(ni, upl, pl, FALSE);
		else
			ntfs_page_dump(ni, upl, pl);
	}
page_err:
	if (ni->rl.elements > 0) {
		err2 = ntfs_cluster_free_from_rl(vol, ni->rl.rl, 0, -1, NULL);
		if (err2) {
			ntfs_error(vol->mp, "Failed to undo cluster "
					"allocation (error %d).  Run chkdsk "
					"to recover the lost space.", err2);
			NVolSetErrors(vol);
		}
		err2 = ntfs_rl_truncate_nolock(vol, &ni->rl, 0);
		if (err2)
			panic("%s(): err2\n", __FUNCTION__);
	}
unl_err:
	lck_rw_unlock_exclusive(&ni->rl.lock);
	if (err == EINVAL)
		err = EIO;
	return err;
}

/**
 * ntfs_attr_record_move_for_attr_list_attribute - move an attribute record
 * @al_ctx:		search context describing the attribute to move
 * @al_entry:		attribute list entry of the attribute to move
 * @ctx:		search context of attribute being resized or NULL
 * @remap_needed:	[OUT] pointer to remap_needed variable or NULL
 *
 * Move the attribute described by the attribute search context @al_ctx and
 * @al_entry from its mft record to a newly allocated extent mft record and
 * update @ctx to reflect this fact (if @ctx is not NULL, otherwise it is
 * ignored).
 *
 * If @ctx is present and is the attribute moved out then set *@remap_needed to
 * true.  If the caller is not interested in this then @remap_needed can be set
 * to NULL in which case it is ignored.
 *
 * Return 0 on success and the negative error code on error.
 */
errno_t ntfs_attr_record_move_for_attr_list_attribute(
		ntfs_attr_search_ctx *al_ctx, ATTR_LIST_ENTRY *al_entry,
		ntfs_attr_search_ctx *ctx, BOOL *remap_needed)
{
	ntfs_inode *base_ni, *ni;
	MFT_RECORD *m;
	ATTR_RECORD *a;
	unsigned attr_len;
	errno_t err;

	base_ni = al_ctx->ni;
	ntfs_debug("Entering for mft_no 0x%llx, attribute type 0x%x.",
			(unsigned long long)base_ni->mft_no,
			(unsigned)le32_to_cpu(al_entry->type));
	/*
	 * Allocate a new extent mft record, attach it to the base ntfs inode
	 * and set up the search context to point to it.
	 *
	 * FIXME: We should go through all existing extent mft records which
	 * will all be attached to @base_ni->extent_nis and for each of them we
	 * should map the extent mft record, check for free space and if we
	 * find enough free space for the attribute being moved we should move
	 * the attribute there instead of allocating a new extent mft record.
	 */
	err = ntfs_mft_record_alloc(base_ni->vol, NULL, NULL, base_ni, &ni, &m,
			&a);
	if (err) {
		ntfs_error(base_ni->vol->mp, "Failed to move attribute to a "
				"new mft record because allocation of the new "
				"mft record failed (error %d).", err);
		return err;
	}
	attr_len = le32_to_cpu(al_ctx->a->length);
	/* Make space for the attribute extent and copy it into place. */
	err = ntfs_attr_record_make_space(m, a, attr_len);
	/*
	 * This cannot fail as the new mft record must have enough space to
	 * hold the attribute record given it fitted inside the old mft record.
	 */
	if (err)
		panic("%s(): err\n", __FUNCTION__);
	memcpy(a, al_ctx->a, attr_len);
	/* Delete the attribute record from the base mft record. */
	ntfs_attr_record_delete_internal(al_ctx->m, al_ctx->a);
	/*
	 * We moved the attribute out of the mft record thus @al_ctx->a now
	 * points to the next attribute.  Since the caller will want to look at
	 * that next attribute we set @al_ctx->is_first so that the next call
	 * to ntfs_attr_find_in_mft_record() will return the currently pointed
	 * at attribute.
	 */
	al_ctx->is_first = 1;
	/*
	 * Change the moved attribute record to reflect the new sequence number
	 * and the current attribute list attribute entry to reflect the new
	 * mft record reference and sequence number.
	 */
	al_entry->mft_reference = MK_LE_MREF(ni->mft_no, ni->seq_no);
	a->instance = al_entry->instance = m->next_attr_instance;
	/*
	 * Increment the next attribute instance number in the mft record as we
	 * consumed the old one.
	 */
	m->next_attr_instance = cpu_to_le16(
			(le16_to_cpu(m->next_attr_instance) + 1) & 0xffff);
	/*
	 * Ensure the changes make it to disk later and unmap the mft record as
	 * we do not need it any more right now.
	 */
	NInoSetMrecNeedsDirtying(ni);
	ntfs_extent_mft_record_unmap(ni);
	/*
	 * Update @ctx if the attribute it describes is still in the base mft
	 * record and the attribute that was deleted was either in front of the
	 * attribute described by @ctx or it was the attribute described by
	 * @ctx.
	 *
	 * FIXME: When we fix the above FIXME and we thus start to place
	 * multiple attributes in each extent mft record we will need to update
	 * @ctx in a more complex fashion here.
	 */
	if (ctx && ctx->ni == base_ni) {
		if ((u8*)al_ctx->a < (u8*)ctx->a)
			ctx->a = (ATTR_RECORD*)((u8*)ctx->a - attr_len);
		else if (al_ctx->a == ctx->a) {
			ctx->m = m;
			ctx->a = a;
			ctx->ni = ni;
			if (remap_needed)
				*remap_needed = TRUE;
		}
	}
	ntfs_debug("Done.");
	return 0;
}

/**
 * ntfs_attr_record_move - move an attribute record to another mft record
 * @ctx:	attribute search context describing the attribute to move
 *
 * Move the attribute described by the attribute search context @ctx from its
 * mft record to a newly allocated extent mft record.  On successful return
 * @ctx is setup to point to the moved attribute.
 *
 * Return 0 on success and the negative error code on error.  On error, the
 * attribute search context is invalid and must be either reinitialized or
 * released.
 *
 * NOTE: This function expects that an attribute list attribute is already
 * present.
 *
 * Locking: Caller must hold lock on attribute list attribute runlist, i.e.
 *	    @ctx->base_ni->attr_list_rl.lock.
 */
errno_t ntfs_attr_record_move(ntfs_attr_search_ctx *ctx)
{
	ntfs_inode *base_ni, *ni;
	MFT_RECORD *m;
	ATTR_RECORD *a;
	u8 *a_copy;
	unsigned attr_len;
	errno_t err, err2;
	ntfs_attr_search_ctx al_ctx;
	static const char es[] = "  Leaving inconsistent metadata.  Unmount "
			"and run chkdsk.";

	base_ni = ctx->base_ni;
	if (!base_ni || !NInoAttrList(base_ni))
		panic("%s(): !base_ni || !NInoAttrList(base_ni)\n",
				__FUNCTION__);
	ni = ctx->ni;
	m = ctx->m;
	a = ctx->a;
	ntfs_debug("Entering for base mft_no 0x%llx, extent mft_no 0x%llx, "
			"attribute type 0x%x.",
			(unsigned long long)base_ni->mft_no,
			(unsigned long long)ni->mft_no,
			(unsigned)le32_to_cpu(a->type));
	attr_len = le32_to_cpu(a->length);
	/* Allocate a temporary buffer to hold the attribute to be moved. */
	a_copy = OSMalloc(attr_len, ntfs_malloc_tag);
	if (!a_copy) {
		ntfs_error(ni->vol->mp, "Not enough memory to allocate "
				"temporary attribute buffer.");
		return ENOMEM;
	}
	/*
	 * Copy the attribute to the temporary buffer and delete it from its
	 * original mft record.
	 */
	memcpy(a_copy, a, attr_len);
	ntfs_attr_record_delete_internal(m, a);
	/*
	 * This function will never be called if the attribute is the only
	 * attribute in the mft record as this would not gain anything thus
	 * report a bug in this case.
	 */
	if (((ATTR_RECORD*)((u8*)m + le16_to_cpu(m->attrs_offset)))->type ==
			AT_END)
		panic("%s(): Is only attribute in mft record!\n", __FUNCTION__);
	/* Ensure the changes make it to disk later. */
	NInoSetMrecNeedsDirtying(ni);
	/*
	 * We have finished with this mft record thus if it is an extent mft
	 * record we release it.  We do this by hand as we want to keep the
	 * current attribute list attribute entry.
	 */
	if (ni != base_ni)
		ntfs_extent_mft_record_unmap(ni);
	/*
	 * Find the attribute list attribute in the base mft record.  Doing
	 * this now hugely simplifies error handling.
	 */
	ntfs_attr_search_ctx_init(&al_ctx, base_ni, ctx->base_m);
	err = ntfs_attr_find_in_mft_record(AT_ATTRIBUTE_LIST, AT_UNNAMED, 0,
			NULL, 0, &al_ctx);
	if (err) {
		ntfs_error(base_ni->vol->mp, "Failed to move attribute to a "
				"new mft record because looking up the "
				"attribute list attribute in the base inode "
				"failed (error %d).", err);
		goto undo_delete;
	}
	/*
	 * Allocate a new extent mft record, attach it to the base ntfs inode
	 * and set up the search context to point to it.
	 */
	err = ntfs_mft_record_alloc(base_ni->vol, NULL, NULL, base_ni, &ni, &m,
			&a);
	if (err) {
		ntfs_error(base_ni->vol->mp, "Failed to move attribute to a "
				"new mft record because allocation of the new "
				"mft record failed (error %d).", err);
		goto undo_delete;
	}
	ctx->ni = ni;
	ctx->m = m;
	ctx->a = a;
	/* Make space for the attribute extent and copy it into place. */
	err = ntfs_attr_record_make_space(m, a, attr_len);
	/*
	 * This cannot fail as the new mft record must have enough space to
	 * hold the attribute record given it fitted inside the old mft record.
	 */
	if (err)
		panic("%s(): err (ntfs_attr_record_make_space())\n",
				__FUNCTION__);
	memcpy(a, a_copy, attr_len);
	/* We do not need the temporary buffer any more. */
	OSFree(a_copy, attr_len, ntfs_malloc_tag);
	/*
	 * Change the moved attribute record to reflect the new sequence number
	 * and the current attribute list attribute entry to reflect the new
	 * mft record reference and sequence number.
	 */
	ctx->al_entry->mft_reference = MK_LE_MREF(ni->mft_no, ni->seq_no);
	a->instance = ctx->al_entry->instance = m->next_attr_instance;
	/*
	 * Increment the next attribute instance number in the mft record as we
	 * consumed the old one.
	 */
	m->next_attr_instance = cpu_to_le16(
			(le16_to_cpu(m->next_attr_instance) + 1) & 0xffff);
	/* Ensure the changes make it to disk later. */
	NInoSetMrecNeedsDirtying(ni);
	/*
	 * Finally, sync the modified attribute list attribute from its in-
	 * memory buffer to the on-disk metadata.
	 */
	a = al_ctx.a;
	if (a->non_resident) {
		unsigned ofs;

		ofs = (u8*)ctx->al_entry - base_ni->attr_list;
		err = ntfs_rl_write(base_ni->vol, base_ni->attr_list,
				base_ni->attr_list_size,
				&base_ni->attr_list_rl, ofs,
				le16_to_cpu(ctx->al_entry->length));
		if (err) {
			ntfs_error(base_ni->vol->mp, "Failed to update "
					"on-disk attribute list attribute of "
					"mft_no 0x%llx (error %d).%s",
					(unsigned long long)base_ni->mft_no,
					err, es);
			return err;
		}
	} else {
		ATTR_LIST_ENTRY *al_entry;

		al_entry = (ATTR_LIST_ENTRY*)((u8*)a +
				le16_to_cpu(a->value_offset) +
				((u8*)ctx->al_entry - base_ni->attr_list));
		al_entry->mft_reference = ctx->al_entry->mft_reference;
		al_entry->instance = ctx->al_entry->instance;
		/* Ensure the changes make it to disk later. */
		NInoSetMrecNeedsDirtying(base_ni);
	}
	ntfs_debug("Done.");
	return 0;
undo_delete:
	/*
	 * Map the old mft record again (if we unmapped it) and re-insert the
	 * deleted attribute record in its old place.
	 */
	ni = ctx->ni;
	if (ni != base_ni) {
		err2 = ntfs_mft_record_map(ni, &m);
		if (err2) {
			/*
			 * Make it safe to release the attribute search
			 * context.
			 */
			ctx->ni = base_ni;
			ntfs_error(base_ni->vol->mp, "Failed to restore "
					"attribute in mft_no 0x%llx after "
					"allocation failure (error %d).%s",
					(unsigned long long)base_ni->mft_no,
					err2, es);
			NVolSetErrors(base_ni->vol);
			goto err;
		}
		/*
		 * If the extent mft record was mapped into a different
		 * address, adjust the mft record and attribute record pointers
		 * in the search context.
		 */
		if (m != ctx->m) {
			ctx->a = (ATTR_RECORD*)((u8*)m + ((u8*)ctx->a -
					(u8*)ctx->m));
			ctx->m = m;
		}
	}
	/*
	 * Creating space for the attribute in its old mft record cannot fail
	 * because we only just deleted the attribute from the mft record thus
	 * there must be enough space in it.
	 */
	err2 = ntfs_attr_record_make_space(ctx->m, ctx->a, attr_len);
	if (err2)
		panic("%s(): err2\n", __FUNCTION__);
	memcpy(ctx->a, a_copy, attr_len);
	/* Ensure the changes make it to disk later. */
	NInoSetMrecNeedsDirtying(ni);
err:
	OSFree(a_copy, attr_len, ntfs_malloc_tag);
	return err;
}

/**
 * ntfs_attr_set_initialized_size - extend the initialized size of an attribute
 * @ni:			ntfs inode whose sizes to extend
 * @new_init_size:	the new initialized size to set @ni to or -1
 *
 * If @new_init_size is >= 0, set the initialized size in the ntfs inode @ni
 * to @new_init_size.  Otherwise ignore @new_init_size and do not change the
 * initialized size in @ni.
 *
 * If the new initialized size is bigger than the data size of the ntfs inode,
 * update the data size to equal the initialized size.  In this case also set
 * the size in the ubc.
 * 
 * Then, set the data and initialized sizes in the attribute record of the
 * attribute specified by the ntfs inode @ni to the values in the ntfs inode
 * @ni.
 *
 * Thus, if @new_init_size is >= 0, both @ni and its underlying attribute have
 * their initialized size set to @new_init_size and if @new_init_size is < 0,
 * the underlying attribute initialized size is set to the initialized size of
 * the ntfs inode @ni.
 *
 * Note the caller is responsible for any zeroing that needs to happen between
 * the old initialized size and @new_init_size.
 *
 * Note when this function is called for resident attributes it requires that
 * the initialized size equals the data size as anything else does not make
 * sense for resident attributes.  Further, @new_init_size must be >= 0, i.e. a
 * specific value must be provided as the call would otherwise be pointless as
 * there is no such thing as an initialized size for resident attributes.
 *
 * Return 0 on success and errno on error.
 *
 * Locking: The caller must hold @ni->lock on the inode for writing.
 */
errno_t ntfs_attr_set_initialized_size(ntfs_inode *ni, s64 new_init_size)
{
	ntfs_inode *base_ni;
	MFT_RECORD *m;
	ntfs_attr_search_ctx *ctx;
	ATTR_RECORD *a;
	errno_t err;
	BOOL data_size_updated = FALSE;

#ifdef DEBUG
	lck_spin_lock(&ni->size_lock);
	ntfs_debug("Entering for mft_no 0x%llx, attribute type 0x%x, old data "
			"size 0x%llx, old initialized size 0x%llx, new "
			"initialized size 0x%llx.",
			(unsigned long long)ni->mft_no,
			(unsigned)le32_to_cpu(ni->type),
			(unsigned long long)ni->data_size,
			(unsigned long long)ni->initialized_size,
			(unsigned long long)new_init_size);
	lck_spin_unlock(&ni->size_lock);
#endif /* DEBUG */
	base_ni = ni;
	if (NInoAttr(ni))
		base_ni = ni->base_ni;
	/* Map, pin, and lock the mft record. */
	err = ntfs_mft_record_map(base_ni, &m);
	if (err)
		goto err;
	ctx = ntfs_attr_search_ctx_get(base_ni, m);
	if (!ctx) {
		err = ENOMEM;
		goto unm_err;
	}
	err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len, 0, NULL, 0,
			ctx);
	if (err) {
		if (err == ENOENT)
			err = EIO;
		goto put_err;
	}
	a = ctx->a;
	lck_spin_lock(&ni->size_lock);
	if (new_init_size >= 0) {
		if (new_init_size < ni->initialized_size)
			panic("%s(): new_init_size < ni->initialized_size\n",
					__FUNCTION__);
		/*
		 * If the new initialized size exceeds the data size extend the
		 * data size to cover the new initialized size.
		 */
		if (new_init_size > ni->data_size) {
			ni->data_size = new_init_size;
			if (a->non_resident)
				a->data_size = cpu_to_sle64(new_init_size);
			else {
				if (NInoNonResident(ni))
					panic("%s(): NInoNonResident(ni)\n",
							__FUNCTION__);
				if (new_init_size >> 32)
					panic("%s(): new_init_size >> 32\n",
							__FUNCTION__);
				if (new_init_size > le32_to_cpu(a->length) -
						le16_to_cpu(a->value_offset))
					panic("%s(): new_init_size > "
							"le32_to_cpu("
							"a->length) - "
							"le16_to_cpu("
							"a->value_offset)\n",
							__FUNCTION__);
				a->value_length = cpu_to_le32(new_init_size);
			}
			data_size_updated = TRUE;
			if (ni == base_ni && !S_ISDIR(ni->mode))
				NInoSetDirtySizes(ni);
		}
		ni->initialized_size = new_init_size;
	} else {
		if (!a->non_resident)
			panic("%s(): !a->non_resident\n", __FUNCTION__);
		if (ni->initialized_size > ni->data_size)
			panic("%s(): ni->initialized_size > ni->data_size\n",
					__FUNCTION__);
		new_init_size = ni->initialized_size;
	}
	if (a->non_resident) {
		if (!NInoNonResident(ni))
			panic("%s(): !NInoNonResident(ni)\n", __FUNCTION__);
		a->initialized_size = cpu_to_sle64(new_init_size);
	}
	lck_spin_unlock(&ni->size_lock);
	/*
	 * If this is a directory B+tree index allocation attribute also update
	 * the sizes in the base inode.
	 */
	if (ni->name == I30 && ni->type == AT_INDEX_ALLOCATION) {
		lck_spin_lock(&base_ni->size_lock);
		if (data_size_updated)
			base_ni->data_size = new_init_size;
		base_ni->initialized_size = new_init_size;
		lck_spin_unlock(&base_ni->size_lock);
	}
	/* Mark the mft record dirty to ensure it gets written out. */
	NInoSetMrecNeedsDirtying(ctx->ni);
put_err:
	ntfs_attr_search_ctx_put(ctx);
unm_err:
	ntfs_mft_record_unmap(base_ni);
	if (data_size_updated) {
		if (!ubc_setsize(ni->vn, new_init_size))
			panic("%s(): ubc_setsize() failed.\n", __FUNCTION__);
	}
	if (!err)
		ntfs_debug("Done.");
	else {
err:
		ntfs_error(ni->vol->mp, "Failed (error %d).", err);
	}
	return err;
}

/**
 * ntfs_attr_extend_initialized - extend the initialized size of an attribute
 * @ni:			ntfs inode of the attribute to extend
 * @new_init_size:	requested new initialized size in bytes
 *
 * Extend the initialized size of an attribute described by the ntfs inode @ni
 * to @new_init_size bytes.  This involves zeroing any non-sparse space between
 * the old initialized size and @new_init_size both in the VM page cache and on
 * disk (if relevant complete pages are already uptodate in the VM page cache
 * then these are simply marked dirty).
 *
 * As a side-effect, the data size as well as the ubc size may be incremented
 * as, in the resident attribute case, it is tied to the initialized size and,
 * in the non-resident attribute case, it may not fall below the initialized
 * size.
 *
 * Note that if the attribute is resident, we do not need to touch the VM page
 * cache at all.  This is because if the VM page is not uptodate we bring it
 * uptodate later, when doing the write to the mft record since we then already
 * have the page mapped.  And if the page is uptodate, the non-initialized
 * region will already have been zeroed when the page was brought uptodate and
 * the region may in fact already have been overwritten with new data via
 * mmap() based writes, so we cannot just zero it.  And since POSIX specifies
 * that the behaviour of resizing a file whilst it is mmap()ped is unspecified,
 * we choose not to do zeroing and thus we do not need to touch the VM page at
 * all.
 *
 * Return 0 on success and errno on error.  In the case that an error is
 * encountered it is possible that the initialized size and/or the data size
 * will already have been incremented some way towards @new_init_size but it is
 * guaranteed that if this is the case, the necessary zeroing will also have
 * happened and that all metadata is self-consistent.
 *
 * Locking: - Caller must hold @ni->lock on the inode for writing.
 *	    - The runlist @ni must be unlocked as it is taken for writing.
 */
errno_t ntfs_attr_extend_initialized(ntfs_inode *ni, const s64 new_init_size)
{
	VCN vcn, end_vcn;
	s64 size, old_init_size, ofs;
	ntfs_volume *vol;
	ntfs_inode *base_ni;
	MFT_RECORD *m;
	ATTR_RECORD *a;
	ntfs_attr_search_ctx *ctx;
	u8 *kattr;
	ntfs_rl_element *rl = NULL;
	errno_t err;
	unsigned attr_len;
	BOOL locked, write_locked, is_sparse, mark_sizes_dirty;

	lck_spin_lock(&ni->size_lock);
	if (new_init_size > ni->allocated_size)
		panic("%s(): new_init_size > ni->allocated_size\n",
				__FUNCTION__);
	size = ni->data_size;
	old_init_size = ni->initialized_size;
	lck_spin_unlock(&ni->size_lock);
	if (new_init_size <= old_init_size)
		panic("%s(): new_init_size <= old_init_size\n",
				__FUNCTION__);
	mark_sizes_dirty = write_locked = FALSE;
	vol = ni->vol;
	ntfs_debug("Entering for mft_no 0x%llx, old initialized size 0x%llx, "
			"new initialized size 0x%llx, old data size 0x%llx.",
			(unsigned long long)ni->mft_no,
			(unsigned long long)old_init_size,
			(unsigned long long)new_init_size,
			(unsigned long long)size);
	base_ni = ni;
	if (NInoAttr(ni))
		base_ni = ni->base_ni;
	/* Use goto to reduce indentation and we need the label below anyway. */
	if (NInoNonResident(ni))
		goto do_non_resident_extend;
	if (old_init_size != size)
		panic("%s(): old_init_size != size\n", __FUNCTION__);
	/* Map, pin, and lock the mft record. */
	err = ntfs_mft_record_map(base_ni, &m);
	if (err)
		goto err;
	ctx = ntfs_attr_search_ctx_get(base_ni, m);
	if (!ctx) {
		err = ENOMEM;
		goto unm_err;
	}
	err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len, 0, NULL, 0,
			ctx);
	if (err) {
		if (err == ENOENT)
			err = EIO;
		goto put_err;
	}
	a = ctx->a;
	if (a->non_resident)
		panic("%s(): a->non_resident\n", __FUNCTION__);
	/* The total length of the attribute value. */
	attr_len = le32_to_cpu(a->value_length);
	if (size != attr_len)
		panic("%s(): size != attr_len\n", __FUNCTION__);
	/*
	 * Do the zeroing in the mft record and update the attribute size in
	 * the mft record.
	 */
	kattr = (u8*)a + le16_to_cpu(a->value_offset);
	bzero(kattr + attr_len, new_init_size - attr_len);
	a->value_length = cpu_to_le32((u32)new_init_size);
	/* Update the sizes in the ntfs inode as well as the ubc size. */
	lck_spin_lock(&ni->size_lock);
	ni->initialized_size = ni->data_size = size = new_init_size;
	lck_spin_unlock(&ni->size_lock);
	/* Mark the mft record dirty to ensure it gets written out. */
	NInoSetMrecNeedsDirtying(ctx->ni);
	ntfs_attr_search_ctx_put(ctx);
	ntfs_mft_record_unmap(base_ni);
	ubc_setsize(ni->vn, new_init_size);
	mark_sizes_dirty = TRUE;
	goto done;
do_non_resident_extend:
	/*
	 * If the new initialized size @new_init_size exceeds the current data
	 * size we need to extend the file size to the new initialized size.
	 */
	if (new_init_size > size) {
		/* Map, pin, and lock the mft record. */
		err = ntfs_mft_record_map(base_ni, &m);
		if (err)
			goto err;
		ctx = ntfs_attr_search_ctx_get(base_ni, m);
		if (!ctx) {
			err = ENOMEM;
			goto unm_err;
		}
		err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len, 0,
				NULL, 0, ctx);
		if (err) {
			if (err == ENOENT)
				err = EIO;
			goto put_err;
		}
		a = ctx->a;
		if (!a->non_resident)
			panic("%s(): !a->non_resident\n", __FUNCTION__);
		if (size != sle64_to_cpu(a->data_size))
			panic("%s(): size != sle64_to_cpu(a->data_size)\n",
					__FUNCTION__);
		size = new_init_size;
		lck_spin_lock(&ni->size_lock);
		ni->data_size = new_init_size;
		lck_spin_unlock(&ni->size_lock);
		a->data_size = cpu_to_sle64(new_init_size);
		/* Mark the mft record dirty to ensure it gets written out. */
		NInoSetMrecNeedsDirtying(ctx->ni);
		ntfs_attr_search_ctx_put(ctx);
		ntfs_mft_record_unmap(base_ni);
		mark_sizes_dirty = TRUE;
		ubc_setsize(ni->vn, new_init_size);
	}
	/*
	 * If the attribute is not sparse we can simply map each page between
	 * the old initialized size and the new initialized size which takes
	 * care of any needed zeroing and then unmap the page again marking it
	 * dirty so the VM later causes it to be written out.
	 *
	 * If the file is sparse on the other hand things are more complicated
	 * because we want to skip any sparse regions because mapping a sparse
	 * page and then unmapping it again and marking it dirty would cause
	 * the hole to be filled when the page is written out.
	 *
	 * Thus for sparse files we walk the runlist before we start doing
	 * anything and check whether there are any sparse regions between the
	 * old initialized size and the new initialized size.  If there are no
	 * sparse regions we can simply proceed as if this attribute was not
	 * sparse.
	 *
	 * If there are sparse regions then we ensure that all runlist
	 * fragments between the old initialized size and new initialized size
	 * are mapped and then we hold the runlist lock shared and walk the
	 * runlist and only for non-sparse regions do we do the page mapping,
	 * unmapping and dirtying.
	 */
	ofs = old_init_size & ~PAGE_MASK_64;
	write_locked = locked = FALSE;
	is_sparse = (NInoSparse(ni));
	if (is_sparse) {
		BOOL have_holes = FALSE;

		locked = TRUE;
		lck_rw_lock_shared(&ni->rl.lock);
		vcn = ofs >> vol->cluster_size_shift;
		end_vcn = (new_init_size + vol->cluster_size_mask) >>
				vol->cluster_size_shift;
retry_remap:
		rl = ni->rl.rl;
		if (!ni->rl.elements || vcn < rl->vcn || !rl->length) {
map_vcn:
			if (!write_locked) {
				write_locked = TRUE;
				if (!lck_rw_lock_shared_to_exclusive(
						&ni->rl.lock)) {
					lck_rw_lock_exclusive(&ni->rl.lock);
					goto retry_remap;
				}
			}
			/* Need to map the runlist fragment containing @vcn. */
			err = ntfs_map_runlist_nolock(ni, vcn, NULL);
			if (err) {
				ntfs_error(vol->mp, "Failed to map runlist "
						"fragment (error %d).", err);
				if (err == EINVAL)
					err = EIO;
				goto unl_err;
			}
			rl = ni->rl.rl;
			if (!ni->rl.elements || vcn < rl->vcn || !rl->length)
				panic("%s(): !ni->rl.elements || "
						"vcn < rl[0].vcn || "
						"!rl->length\n", __FUNCTION__);
		}
		/* Seek to the runlist element containing @vcn. */
		while (rl->length && vcn >= rl[1].vcn)
			rl++;
		do {
			/*
			 * If this run is not mapped map it now and start again
			 * as the runlist will have been updated.
			 */
			if (rl->lcn == LCN_RL_NOT_MAPPED) {
				vcn = rl->vcn;
				goto map_vcn;
			}
			/* If this run is not valid abort with an error. */
			if (!rl->length || rl->lcn < LCN_HOLE)
				goto rl_err;
			if (rl->lcn == LCN_HOLE) {
				have_holes = TRUE;
				/*
				 * If the current initialized size is inside
				 * the current run we can move the initialized
				 * size forward to the end of this run taking
				 * care not to go beyond the new initialized
				 * size.
				 *
				 * Note we also have to take care not to move
				 * the initialized size backwards thus we only
				 * have to update the initialized size if the
				 * current offset is above the old initialized
				 * size.
				 */
				if (ofs >> vol->cluster_size_shift >= rl->vcn) {
					ofs = rl[1].vcn <<
							vol->cluster_size_shift;
					if (ofs > old_init_size) {
						if (ofs > new_init_size)
							ofs = new_init_size;
						lck_spin_lock(&ni->size_lock);
						ni->initialized_size = ofs;
						lck_spin_unlock(&ni->size_lock);
						if (ofs == new_init_size)
							goto update_done;
					}
				}
			}
			/* Proceed to the next run. */
			rl++;
		} while (rl->vcn < end_vcn);
		/*
		 * If we encountered sparse regions in the runlist then we need
		 * to keep the runlist lock shared.
		 *
		 * If there were no sparse regions we do not need the runlist
		 * lock at all any more so we release it and we pretend this
		 * attribute is not sparse.
		 */
		if (have_holes) {
			if (write_locked) {
				lck_rw_lock_exclusive_to_shared(&ni->rl.lock);
				write_locked = FALSE;
			}
			/*
			 * We may have moved @ofs forward in which case it will
			 * be cluster aligned instead of page aligned and the
			 * two are not equal when the cluster size is less than
			 * the page size so we need to align at @ofs to the
			 * page size again.
			 */
			ofs &= ~PAGE_MASK_64;
			rl = ni->rl.rl;
		} else {
			if (write_locked)
				lck_rw_unlock_exclusive(&ni->rl.lock);
			else
				lck_rw_unlock_shared(&ni->rl.lock);
			locked = FALSE;
			is_sparse = FALSE;
		}
	}
	do {
		/*
		 * If the file is sparse, check if the current page is
		 * completely sparse and if so skip it.
		 *
		 * Otherwise take care of zeroing the uninitialized region.
		 */
		if (is_sparse) {
			/* We need to update @vcn to the current offset @ofs. */
			vcn = ofs >> vol->cluster_size_shift;
			/* Determine the first VCN outside the current page. */
			end_vcn = (ofs + PAGE_SIZE + vol->cluster_size_mask) >>
					vol->cluster_size_shift;
			/* Seek to the runlist element containing @vcn. */
			while (rl->length && vcn >= rl[1].vcn)
				rl++;
			/* If this run is not valid abort with an error. */
			if (!rl->length || rl->lcn < LCN_HOLE)
				goto rl_err;
			/*
			 * @rl is the runlist element containing @ofs, the
			 * current initialized size, and the current @vcn.
			 *
			 * Check whether the current page is completely sparse.
			 * This is complicated slightly by the fact that a page
			 * can span multiple clusters when the cluster size is
			 * less than the page size.
			 *
			 * As an optimization when a sparse run spans more than
			 * one page we forward both @ofs and the initialized
			 * size to the end of the run (ensuring it is page
			 * aligned).
			 */
			do {
				if (rl->lcn >= 0) {
					/* This page is not entirely sparse. */
					goto on_disk_page;
				}
				/* Proceed to the next run. */
				rl++;
				vcn = rl->vcn;
			} while (vcn < end_vcn && rl->length);
			/*
			 * The page is entirely sparse.
			 *
			 * Check how many pages are entirely sparse and move
			 * the initialized size up to the end of the sparse
			 * region ensuring we maintain page alignment.
			 */
			while (rl->lcn == LCN_HOLE && rl->length)
				rl++;
			ofs = (rl->vcn << vol->cluster_size_shift) &
					~PAGE_MASK_64;
			/*
			 * Update the initialized size in the ntfs inode.  This
			 * is enough to make ntfs_vnop_pageout() work.  We
			 * could postpone this until we actually are going to
			 * unmap a page or we have reached the end of the
			 * region to be initialized but we do it now to
			 * minimize our impact on processes that are performing
			 * concurrent mmap() based writes to this attribute.
			 *
			 * FIXME: This is not actually true as the caller is
			 * holding the ntfs inode lock for writing thus no
			 * pageouts on this inode can occur at all.  We
			 * probably need to fix this so we cannot bring the
			 * system out of memory.
			 */
			if (ofs > new_init_size)
				ofs = new_init_size;
			lck_spin_lock(&ni->size_lock);
			ni->initialized_size = ofs;
			lck_spin_unlock(&ni->size_lock);
		} else /* if (!is_sparse) */ {
			upl_t upl;
			upl_page_info_array_t pl;

on_disk_page:
			/*
			 * Read the page.  If the page is not present,
			 * ntfs_page_map() will zero the uninitialized/sparse
			 * regions for us.
			 *
			 * TODO: An optimization would be to do things by hand
			 * taking advantage of dealing with multiple pages at
			 * once instead of working one page at a time.
			 *
			 * FIXME: We are potentially creating a lot of dirty
			 * pages here and since the caller is holding the ntfs
			 * inode lock for writing no pageouts on this inode can
			 * occur at all.  We probably need to fix this so we
			 * cannot bring the system out of memory.
			 */
// TODO: This should never happen.  Just adding it so we can detect if we were
// going to deadlock.  If it triggers need to fix it in the code so it does
// not.  Or perhaps just remove the warning and use this as the solution.
			if (locked && write_locked) {
				write_locked = FALSE;
				lck_rw_lock_exclusive_to_shared(&ni->rl.lock);
				ntfs_warning(vol->mp, "Switching runlist lock "
						"to shared to avoid "
						"deadlock.");
			}
			err = ntfs_page_map(ni, ofs, &upl, &pl, &kattr, TRUE);
			if (err)
				goto unl_err;
			/*
			 * Update the initialized size in the ntfs inode.  This
			 * is enough to make ntfs_vnop_pageout() work.
			 */
			ofs += PAGE_SIZE;
			if (ofs > new_init_size)
				ofs = new_init_size;
			lck_spin_lock(&ni->size_lock);
			ni->initialized_size = ofs;
			lck_spin_unlock(&ni->size_lock);
			/* Set the page dirty so it gets written out. */
			ntfs_page_unmap(ni, upl, pl, TRUE);
		}
	} while (ofs < new_init_size);
	lck_spin_lock(&ni->size_lock);
	if (ni->initialized_size != new_init_size)
		panic("%s(): ni->initialized_size != new_init_size\n",
				__FUNCTION__);
	lck_spin_unlock(&ni->size_lock);
update_done:
	/* If we are holding the runlist lock, release it now. */
	if (locked) {
		if (write_locked)
			lck_rw_unlock_exclusive(&ni->rl.lock);
		else
			lck_rw_unlock_shared(&ni->rl.lock);
		locked = FALSE;
	}
	/* Bring up to date the initialized_size in the attribute record. */
	err = ntfs_attr_set_initialized_size(ni, -1);
	if (err)
		goto unl_err;
done:
	/*
	 * If we have modified the size of the base inode, cause the sizes to
	 * be written to all the directory index entries pointing to the base
	 * inode when the inode is written to disk.
	 */
	if (mark_sizes_dirty && ni == base_ni && !S_ISDIR(ni->mode))
		NInoSetDirtySizes(ni);
	ntfs_debug("Done, new initialized size 0x%llx, new data size 0x%llx.",
			(unsigned long long)new_init_size,
			(unsigned long long)size);
	return 0;
rl_err:
	ntfs_error(vol->mp, "Runlist is corrupt.  Unmount and run chkdsk.");
	NVolSetErrors(vol);
	err = EIO;
unl_err:
	if (locked) {
		if (write_locked)
			lck_rw_unlock_exclusive(&ni->rl.lock);
		else
			lck_rw_unlock_shared(&ni->rl.lock);
	}
	lck_spin_lock(&ni->size_lock);
	ni->initialized_size = old_init_size;
	lck_spin_unlock(&ni->size_lock);
	goto err;
put_err:
	ntfs_attr_search_ctx_put(ctx);
unm_err:
	ntfs_mft_record_unmap(base_ni);
err:
	ntfs_debug("Failed (error %d).", err);
	return err;
}

/**
 * ntfs_attr_sparse_set - switch an attribute to be sparse
 * @base_ni:	base ntfs inode to which the attribute belongs
 * @ni:		ntfs inode of attribute which to cause to be sparse
 * @ctx:	attribute search context describing the attribute to work on
 *
 * Switch the non-sparse, base attribute described by @ni and @ctx belonging to
 * the base ntfs inode @base_ni to be sparse.
 *
 * Return 0 on success and errno on error.
 *
 * Note that the attribute may be moved to be able to extend it when adding the
 * compressed size.  Thus any cached values of @ctx->ni, @ctx->m, and @ctx->a
 * are invalid after this function returns.
 */
static errno_t ntfs_attr_sparse_set(ntfs_inode *base_ni, ntfs_inode *ni,
		ntfs_attr_search_ctx *ctx)
{
#if 0
	VCN highest_vcn, stop_vcn;
	ntfs_volume *vol;
	MFT_RECORD *base_m, *m;
	ATTR_RECORD *a;
	ntfs_rl_element *rl;
	ntfs_inode *eni;
	ATTR_LIST_ENTRY *al_entry;
	unsigned name_size, mp_ofs, mp_size, al_entry_len, new_al_size;
	unsigned new_al_alloc;
	errno_t err;
	BOOL rewrite;
#endif

	ntfs_debug("Entering for mft_no 0x%llx, type 0x%x, name_len 0x%x.",
			(unsigned long long)base_ni->mft_no,
			(unsigned)le32_to_cpu(ni->type), ni->name_len);
	return ENOTSUP;
#if 0
	vol = base_ni->vol;
	base_m = base_ni->m;
	m = ctx->m;
	a = ctx->a;
	rewrite = FALSE;
	/*
	 * We should only be called for non-sparse, non-resident, $DATA
	 * attributes.
	 */
	if (a->type != AT_DATA || !NInoNonResident(ni) || !a->non_resident ||
			NInoSparse(ni) || a->flags & ATTR_IS_SPARSE)
		panic("%s(): a->type != AT_DATA || !NInoNonResident(ni) || "
				"!a->non_resident || NInoSparse(ni) || "
				"a->flags & ATTR_IS_SPARSE\n", __FUNCTION__);
	/*
	 * If the attribute is not compressed either, we need to add the
	 * compressed size to the attribute record and to switch all relevant
	 * fields to match.
	 */
	if (NInoCompressed(ni))
		goto is_compressed;
	if (a->flags & ATTR_IS_COMPRESSED)
		panic("%s(): a->flags & ATTR_IS_COMPRESSED)\n", __FUNCTION__);
retry_attr_rec_resize:
	err = ntfs_attr_record_resize(m, a, le32_to_cpu(a->length) +
			sizeof(a->compressed_size));
	if (!err) {
		/*
		 * Move everything at the offset of the compressed size to make
		 * space for the compressed size.
		 */
		memmove((u8*)a + offsetof(ATTR_RECORD, compressed_size) +
				sizeof(a->compressed_size), (u8*)a +
				offsetof(ATTR_RECORD, compressed_size),
				le32_to_cpu(a->length) - offsetof(ATTR_RECORD,
				compressed_size));
		/*
		 * Update the name offset to match the moved data.  If there is
		 * no name then set the name offset to the correct position
		 * instead of adding to a potentially incorrect value.
		 */
		if (a->name_length)
			a->name_offset = cpu_to_le16(
					le16_to_cpu(a->name_offset) +
					sizeof(a->compressed_size));
		else
			a->name_offset = const_cpu_to_le16(
					offsetof(ATTR_RECORD,
					compressed_size) +
					sizeof(a->compressed_size));
		/* Update the mapping pairs offset to its new location. */
		mp_ofs = le16_to_cpu(a->mapping_pairs_offset) +
				sizeof(a->compressed_size);
		goto set_compressed_size;
	}
	/*
	 * There is not enough space in the mft record.
	 *
	 * We need to add an attribute list attribute if it is not already
	 * present.
	 */
	if (!NInoAttrList(base_ni)) {
		err = ntfs_attr_list_add(base_ni, base_m, ctx);
		if (err || ctx->is_error) {
			if (!err)
				err = ctx->error;
			ntfs_error(vol->mp, "Failed to %s mft_no 0x%llx "
					"(error %d).", ctx->is_error ?
					"remap extent mft record of" :
					"add attribute list attribute to",
					(unsigned long long)base_ni->mft_no,
					err);
			return err;
		}
		/*
		 * The attribute location will have changed so update it from
		 * the search context.
		 */
		m = ctx->m;
		a = ctx->a;
		/*
		 * Retry the original attribute record resize as we may now
		 * have enough space to add the compressed size to the
		 * attribute record.
		 *
		 * This can for example happen when the attribute was moved out
		 * to an extent mft record which has much more free space than
		 * the base mft record had.
		 */
		goto retry_attr_rec_resize;
	}
	/*
	 * If this is not the only attribute record in the mft record then move
	 * it out to a new extent mft record which is guaranteed to generate
	 * enough space to add the compressed size to the attribute record.
	 */
	if (!ntfs_attr_record_is_only_one(m, a)) {
		lck_rw_lock_shared(&base_ni->attr_list_rl.lock);
		err = ntfs_attr_record_move(ctx);
		lck_rw_unlock_shared(&base_ni->attr_list_rl.lock);
		if (err) {
			ntfs_error(vol->mp, "Failed to move attribute extent "
					"from mft record 0x%llx to an extent "
					"mft record (error %d).",
					(unsigned long long)ctx->ni->mft_no,
					err);
			/*
			 * We could try to remove the attribute list attribute
			 * if we added it above but this will require
			 * attributes to be moved back into the base mft record
			 * from extent mft records so is a lot of work and
			 * given we are in an error code path and given that it
			 * is ok to just leave the inode with an attribute list
			 * attribute we do not bother and just bail out.
			 */
			return err;
		}
		/*
		 * The attribute location will have changed so update it from
		 * the search context.
		 */
		m = ctx->m;
		a = ctx->a;
		/*
		 * Retry the original attribute record resize as we will now
		 * have enough space to add the compressed size to the
		 * attribute record.
		 */
		goto retry_attr_rec_resize;
	}
	/*
	 * This is the only attribute in the mft record thus there is nothing
	 * to gain by moving it to another extent mft record.  So to generate
	 * space, we allocate a new extent mft record, create a new extent
	 * attribute record in it and use it to catch the overflow mapping
	 * pairs array data generated by the fact that we have added the
	 * compressed size to the base extent.
	 *
	 * TODO: We could instead iterate over all existing extent attribute
	 * records and rewrite the entire mapping pairs array but this could
	 * potentially be a lot of overhead.  On the other hand it would be an
	 * infrequent event thus the overhead may be worth it in the long term
	 * as it will generate better packed metadata.  For now we choose the
	 * simpler approach of just doing the splitting into a new extent
	 * attribute record.
	 *
	 * As we are going to rewrite the mapping pairs array we need to make
	 * sure we have decompressed the mapping pairs from the base attribute
	 * extent and have them cached in the runlist.
	 */
	if (!ni->rl.elements || ni->rl.rl->lcn == LCN_RL_NOT_MAPPED) {
		err = ntfs_mapping_pairs_decompress(vol, a, &ni->rl);
		if (err) {
			ntfs_error(vol->mp, "Mapping of the base runlist "
					"fragment failed (error %d).", err);
			if (err != ENOMEM)
				err = EIO;
			return err;
		}
	}
	rewrite = TRUE;
	/*
	 * Now add the compressed size so we can unmap the mft record of the
	 * base attribute extent if it is an extent mft record.
	 *
	 * First, move the name if present to its new location and update the
	 * name offset to match the new location.
	 */
	name_size = a->name_length * sizeof(ntfschar);
	if (name_size)
		memmove((u8*)a + offsetof(ATTR_RECORD, compressed_size) +
				sizeof(a->compressed_size), (u8*)a +
				le16_to_cpu(a->name_offset), name_size);
	a->name_offset = const_cpu_to_le16(offsetof(ATTR_RECORD,
			compressed_size) + sizeof(a->compressed_size));
	/* Update the mapping pairs offset to its new location. */
	mp_ofs = (offsetof(ATTR_RECORD, compressed_size) +
			sizeof(a->compressed_size) + name_size + 7) & ~7;
set_compressed_size:
	a->mapping_pairs_offset = cpu_to_le16(mp_ofs);
	/*
	 * Set the compression unit to 0 or 4 depending on the NTFS volume
	 * version.  FIXME: We know that NT4 uses 4 whilst XPSP2 uses 0 and we
	 * do not know what 2k uses so we assume 2k is the same as XPSP2.
	 */
	if (vol->major_ver > 1) {
		a->compression_unit = 0;
		ni->compression_block_size = 0;
		ni->compression_block_clusters =
				ni->compression_block_size_shift = 0;
	} else {
		a->compression_unit = NTFS_COMPRESSION_UNIT;
		ni->compression_block_size = 1U << (NTFS_COMPRESSION_UNIT +
				vol->cluster_size_shift);
		ni->compression_block_size_shift =
				ffs(ni->compression_block_size) - 1;
		ni->compression_block_clusters = 1U << NTFS_COMPRESSION_UNIT;
	}
	lck_spin_lock(&ni->size_lock);
	ni->compressed_size = ni->allocated_size;
	a->compressed_size = a->allocated_size;
	lck_spin_unlock(&ni->size_lock);
is_compressed:
	/* Mark both the attribute and the ntfs inode as sparse. */
	a->flags |= ATTR_IS_SPARSE;
	NInoSetSparse(ni);
	/*
	 * If this is the unnamed $DATA attribute, need to set the sparse flag
	 * in the standard information attribute and in the directory entries,
	 * too.
	 */
	if (ni == base_ni) {
		ni->file_attributes |= FILE_ATTR_SPARSE_FILE;
		NInoSetDirtyFileAttributes(ni);
	}
	/* If we do not need to rewrite the mapping pairs array we are done. */
	if (!rewrite)
		goto done;
	/*
	 * Determine the size of the mapping pairs array needed to fit all the
	 * runlist elements that were stored in the base attribute extent
	 * before we added the compressed size to the attribute record.
	 */
	highest_vcn = sle64_to_cpu(a->highest_vcn);
	err = ntfs_get_size_for_mapping_pairs(vol, ni->rl.elements ?
			ni->rl.rl : NULL, 0, highest_vcn, &mp_size);
	if (err) {
		ntfs_error(vol->mp, "Failed to get size for mapping pairs "
				"array (error %d).", err);
		goto undo1;
	}
	/* Write the mapping pairs array. */
	err = ntfs_mapping_pairs_build(vol, (s8*)a + mp_ofs,
			le32_to_cpu(a->length) - mp_ofs, ni->rl.elements ?
			ni->rl.rl : NULL, 0, highest_vcn, &stop_vcn);
	if (err && err != ENOSPC) {
		ntfs_error(vol->mp, "Failed to rebuild mapping pairs array "
				"(error %d).", err);
		goto undo1;
	}
	/* If by some miracle it all fitted we are done. */
	if (!err)
		goto done;
	/* Update the highest vcn to the new value. */
	a->highest_vcn = cpu_to_sle64(stop_vcn - 1);
	/*
	 * If the base attribute extent is in an extent mft record mark it
	 * dirty so it gets written back and unmap the extent mft record so we
	 * can allocate the new extent mft record.
	 */
	if (ctx->ni != base_ni) {
		NInoSetMrecNeedsDirtying(ctx->ni);
		ntfs_extent_mft_record_unmap(ctx->ni);
		/* Make the search context safe. */
		ctx->ni = base_ni;
	}
	/*
	 * Get the runlist element containing the lowest vcn for the new
	 * attribute record, i.e. @stop_vcn.
	 *
	 * This cannot fail as we know the runlist is ok and the runlist
	 * fragment containing @stop_vcn is mapped.
	 */
	rl = NULL;
	if (ni->rl.elements) {
		rl = ntfs_rl_find_vcn_nolock(ni->rl.rl, stop_vcn);
		if (!rl)
			panic("%s(): Memory corruption detected.\n",
					__FUNCTION__);
	}
	/*
	 * Determine the size of the mapping pairs array needed to fit all the
	 * remaining runlist elements that were stored in the base attribute
	 * extent before we added the compressed size to the attribute record
	 * but did now not fit.
	 */
	err = ntfs_get_size_for_mapping_pairs(vol, rl, stop_vcn, highest_vcn,
			&mp_size);
	if (err) {
		ntfs_error(vol->mp, "Failed to get size for mapping pairs "
				"array (error %d).", err);
		goto undo2;
	}
	/*
	 * We now need to allocate a new extent mft record, attach it to the
	 * base ntfs inode and set up the search context to point to it, then
	 * insert the new attribute record into it.
	 */
	err = ntfs_mft_record_alloc(vol, NULL, NULL, ni, &eni, &m, &a);
	if (err) {
		ntfs_error(vol->mp, "Failed to allocate a new extent mft "
				"record (error %d).", err);
		goto undo2;
	}
	ctx->ni = eni;
	ctx->m = m;
	ctx->a = a;
	/*
	 * Calculate the offset into the new attribute at which the mapping
	 * pairs array begins.  The mapping pairs array is placed after the
	 * name aligned to an 8-byte boundary which in turn is placed
	 * immediately after the non-resident attribute record itself.
	 *
	 * Note that extent attribute records do not have the compressed size
	 * field in their attribute records.
	 */
	mp_ofs = (offsetof(ATTR_RECORD, compressed_size) + name_size + 7) & ~7;
	/*
	 * Make space for the new attribute extent.  This cannot fail as we now
	 * have an empty mft record which by definition can hold a non-resident
	 * attribute record with just a small mapping pairs array.
	 */
	err = ntfs_attr_record_make_space(m, a, mp_ofs + mp_size);
	if (err)
		panic("%s(): err (ntfs_attr_record_make_space())\n",
				__FUNCTION__);
	/*
	 * Now setup the new attribute record.  The entire attribute has been
	 * zeroed and the length of the attribute record has been set.
	 *
	 * Before we proceed with setting up the attribute, add an attribute
	 * list attribute entry for the created attribute extent.
	 */
	al_entry = ctx->al_entry = (ATTR_LIST_ENTRY*)((u8*)ctx->al_entry +
			le16_to_cpu(ctx->al_entry->length));
	al_entry_len = (offsetof(ATTR_LIST_ENTRY, name) + name_size + 7) & ~7;
	new_al_size = base_ni->attr_list_size + al_entry_len;
	/* Out of bounds checks. */
	if ((u8*)al_entry < base_ni->attr_list || (u8*)al_entry >
			base_ni->attr_list + new_al_size || (u8*)al_entry +
			al_entry_len > base_ni->attr_list + new_al_size) {
		/* Inode is corrupt. */
		ntfs_error(vol->mp, "Inode 0x%llx is corrupt.  Run chkdsk.",
				(unsigned long long)base_ni->mft_no);
		err = EIO;
		goto undo3;
	}
	err = ntfs_attr_size_bounds_check(vol, AT_ATTRIBUTE_LIST, new_al_size);
	if (err) {
		if (err == ERANGE) {
			ntfs_error(vol->mp, "Attribute list attribute would "
					"become to large.  You need to "
					"defragment your volume and then try "
					"again.");
			err = ENOSPC;
		} else {
			ntfs_error(vol->mp, "Attribute list attribute is "
					"unknown on the volume.  The volume "
					"is corrupt.  Run chkdsk.");
			NVolSetErrors(vol);
			err = EIO;
		}
		goto undo3;
	}
	/*
	 * Reallocate the memory buffer if needed and create space for the new
	 * entry.
	 */
	new_al_alloc = (new_al_size + NTFS_ALLOC_BLOCK - 1) &
			~(NTFS_ALLOC_BLOCK - 1);
	if (new_al_alloc > base_ni->attr_list_alloc) {
		u8 *tmp, *al, *al_end;
		unsigned al_entry_ofs;

		tmp = OSMalloc(new_al_alloc, ntfs_malloc_tag);
		if (!tmp) {
			ntfs_error(vol->mp, "Not enough memory to extend the "
					"attribute list attribute.");
			err = ENOMEM;
			goto undo3;
		}
		al = base_ni->attr_list;
		al_entry_ofs = (u8*)al_entry - al;
		al_end = al + base_ni->attr_list_size;
		memcpy(tmp, al, al_entry_ofs);
		if ((u8*)al_entry < al_end)
			memcpy(tmp + al_entry_ofs + al_entry_len, al +
					al_entry_ofs, base_ni->attr_list_size -
					al_entry_ofs);
		al_entry = ctx->al_entry = (ATTR_LIST_ENTRY*)(tmp +
				al_entry_ofs);
		OSFree(base_ni->attr_list, base_ni->attr_list_alloc,
				ntfs_malloc_tag);
		base_ni->attr_list_alloc = new_al_alloc;
		base_ni->attr_list = tmp;
	} else if ((u8*)al_entry < base_ni->attr_list +
			base_ni->attr_list_size)
		memmove((u8*)al_entry + al_entry_len, al_entry,
				base_ni->attr_list_size - ((u8*)al_entry -
				base_ni->attr_list));
	base_ni->attr_list_size = new_al_size;
	/* Set up the attribute extent and the attribute list entry. */
	al_entry->type = a->type = ni->type;
	al_entry->length = cpu_to_le16(al_entry_len);
	a->non_resident = 1;
	al_entry->name_length = a->name_length = ni->name_len;
	a->name_offset = const_cpu_to_le16(offsetof(ATTR_RECORD,
			compressed_size));
	al_entry->name_offset = offsetof(ATTR_LIST_ENTRY, name);
	al_entry->instance = a->instance = m->next_attr_instance;
	/*
	 * Increment the next attribute instance number in the mft record as we
	 * consumed the old one.
	 */
	m->next_attr_instance = cpu_to_le16(
			(le16_to_cpu(m->next_attr_instance) + 1) & 0xffff);
	al_entry->lowest_vcn = a->lowest_vcn = cpu_to_sle64(stop_vcn);
	a->highest_vcn = cpu_to_sle64(highest_vcn);
	al_entry->mft_reference = MK_LE_MREF(eni->mft_no, eni->seq_no);
	a->mapping_pairs_offset = cpu_to_le16(mp_ofs);
	/* Copy the attribute name into place. */
	if (name_size) {
		memcpy((u8*)a + offsetof(ATTR_RECORD, compressed_size),
				ni->name, name_size);
		memcpy(&al_entry->name, ni->name, name_size);
	}
	/* For tidyness, zero out the unused space. */
	if (al_entry_len > offsetof(ATTR_LIST_ENTRY, name) + name_size)
		memset((u8*)al_entry + offsetof(ATTR_LIST_ENTRY, name) +
				name_size, 0, al_entry_len -
				(offsetof(ATTR_LIST_ENTRY, name) + name_size));
	/*
	 * Extend the attribute list attribute and copy in the modified value
	 * from the cache.
	 */
	err = ntfs_attr_list_sync_extend(base_ni, base_m,
			(u8*)al_entry - base_ni->attr_list, ctx);
	if (err || ctx->is_error) {
		/*
		 * If @ctx->is_error indicates error this is fatal as we cannot
		 * build the mapping pairs array into it as it is not mapped.
		 *
		 * However, we may still be able to recover from this situation
		 * by freeing the extent mft record and thus deleting the
		 * attribute record.  This only works when this is the only
		 * attribute record in the mft record and when we just created
		 * this extent attribute record.  We can easily determine if
		 * this is the only attribute in the mft record by scanning
		 * through the cached attribute list attribute.
		 */
		if (!err)
			err = ctx->error;
		ntfs_error(vol->mp, "Failed to %s mft_no 0x%llx (error %d).",
				ctx->is_error ?  "remap extent mft record of" :
				"extend and sync attribute list attribute to",
				(unsigned long long)base_ni->mft_no, err);
		goto undo4;
	}
	/*
	 * Finally, proceed to building the mapping pairs array into the
	 * attribute record.
	 */
	err = ntfs_mapping_pairs_build(vol, (s8*)a + mp_ofs,
			le32_to_cpu(a->length) - mp_ofs, rl, stop_vcn,
			highest_vcn, &stop_vcn);
	if (err && err != ENOSPC) {
		ntfs_error(vol->mp, "Failed to rebuild mapping pairs array "
				"(error %d).", err);
		goto undo5;
	}
	/*
	 * We must have fully rebuilt the mapping pairs array as we made sure
	 * there is enough space.
	 */
	if (err || stop_vcn != highest_vcn + 1)
		panic("%s(): err || stop_vcn != highest_vcn + 1\n",
				__FUNCTION__);
	/*
	 * If the attribute extent is in an extent mft record mark it dirty so
	 * it gets written back and unmap the extent mft record so we can map
	 * the mft record containing the base extent again.
	 */
	if (eni != base_ni) {
		NInoSetMrecNeedsDirtying(eni);
		ntfs_extent_mft_record_unmap(eni);
		/* Make the search context safe. */
		ctx->ni = base_ni;
	}
	/*
	 * Look up the base attribute extent again so we restore the search
	 * context as the caller expects it to be.
	 */
	ntfs_attr_search_ctx_reinit(ctx);
	err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len, 0, NULL, 0,
			ctx);
	if (err) {
		ntfs_error(vol->mp, "Re-lookup of first attribute extent "
				"failed (error %d).", err);
		if (err == ENOENT)
			err = EIO;
		goto undo6;
	}
done:
	ntfs_debug("Done.");
	return 0;
// TODO: HERE:
undo6:
undo5:
undo4:
undo3:
undo2:
undo1:
	panic("%s(): TODO!\n", __FUNCTION__);
	return err;
#endif
}

/**
 * ntfs_attr_sparse_clear - switch an attribute to not be sparse any more
 * @base_ni:	base ntfs inode to which the attribute belongs
 * @ni:		ntfs inode of attribute which to cause not to be sparse
 * @ctx:	attribute search context describing the attribute to work on
 *
 * Switch the sparse attribute described by @ni and @ctx belonging to the base
 * ntfs inode @base_ni to not be sparse any more.
 *
 * This function cannot fail.
 */
static void ntfs_attr_sparse_clear(ntfs_inode *base_ni, ntfs_inode *ni,
		ntfs_attr_search_ctx *ctx)
{
	ATTR_RECORD *a;
	
	a = ctx->a;
	/*
	 * We should only be called for sparse, non-resident, $DATA attributes.
	 */
	if (a->type != AT_DATA || !NInoNonResident(ni) || !a->non_resident ||
			!NInoSparse(ni) || !(a->flags & ATTR_IS_SPARSE))
		panic("%s(): a->type != AT_DATA || !NInoNonResident(ni) || "
				"!a->non_resident || !NInoSparse(ni) || "
				"!(a->flags & ATTR_IS_SPARSE)\n", __FUNCTION__);
	/*
	 * If the attribute is not compressed we need to remove the compressed
	 * size from the attribute record and to switch all relevant fields to
	 * match.
	 */
	if (!NInoCompressed(ni)) {
		errno_t err;

		if (a->flags & ATTR_IS_COMPRESSED)
			panic("%s(): a->flags & ATTR_IS_COMPRESSED)\n",
					__FUNCTION__);
		/*
		 * Move everything after the compressed size forward to the
		 * offset of the compressed size thus deleting the compressed
		 * size.
		 */
		memmove((u8*)a + offsetof(ATTR_RECORD, compressed_size),
				(u8*)a + offsetof(ATTR_RECORD,
				compressed_size) + sizeof(a->compressed_size),
				le32_to_cpu(a->length) - (offsetof(ATTR_RECORD,
				compressed_size) + sizeof(a->compressed_size)));
		/*
		 * Update the name offset and the mapping pairs offset to match
		 * the moved data.  If there is no name then set the name
		 * offset to the correct position instead of subtracting from a
		 * potentially incorrect value.
		 */
		if (!a->name_length)
			a->name_offset = const_cpu_to_le16(offsetof(ATTR_RECORD,
					compressed_size));
		else
			a->name_offset = cpu_to_le16(
					le16_to_cpu(a->name_offset) -
					sizeof(a->compressed_size));
		a->mapping_pairs_offset = cpu_to_le16(
				le16_to_cpu(a->mapping_pairs_offset) -
				sizeof(a->compressed_size));
		/* Set the compression unit to 0. */
		a->compression_unit = 0;
		lck_spin_lock(&ni->size_lock);
		ni->compressed_size = 0;
		lck_spin_unlock(&ni->size_lock);
		/* Clear the other related fields. */
		ni->compression_block_size = 0;
		ni->compression_block_clusters =
				ni->compression_block_size_shift = 0;
		/*
		 * Finally shrink the attribute record to reflect the removal
		 * of the compressed size.  Note, this cannot fail since we are
		 * making the attribute smaller thus by definition there is
		 * enough space to do so.
		 */
		err = ntfs_attr_record_resize(ctx->m, a,
				le32_to_cpu(a->length) -
				sizeof(a->compressed_size));
		if (err)
			panic("%s(): err\n", __FUNCTION__);
	}
	/* Mark both the attribute and the ntfs inode as non-sparse. */
	a->flags &= ~ATTR_IS_SPARSE;
	NInoClearSparse(ni);
	/*
	 * If this is the unnamed $DATA attribute, need to clear the sparse
	 * flag in the standard information attribute and in the directory
	 * entries, too.
	 */
	if (ni == base_ni) {
		ni->file_attributes &= ~FILE_ATTR_SPARSE_FILE;
		NInoSetDirtyFileAttributes(ni);
	}
}

/**
 * ntfs_attr_instantiate_holes - instantiate the holes in an attribute region
 * @ni:		ntfs inode of the attribute whose holes to instantiate
 * @start:	start offset in bytes at which to begin instantiating holes
 * @end:	end offset in bytes at which to stop instantiating holes
 * @new_end:	return the offset at which we stopped instantiating holes
 * @atomic:	if true must complete the entire exension or abort
 *
 * Scan the runlist (mapping any unmapped fragments as needed) starting at byte
 * offset @start into the attribute described by the ntfs inode @ni and
 * finishing at byte offset @end and instantiate any sparse regions located
 * between @start and @end with real clusters.
 *
 * Any clusters that are inside the initialized size are zeroed.
 *
 * If @atomic is true the whole instantiation must be complete so abort on
 * errors.  If @atomic is false partial instantiations are acceptable (but we
 * still return an error if the instantiation is partial).  In any case we set
 * *@new_end to the end of the instantiated range.  Thus the caller has to
 * always check *@new_end.  If *@new_end is equal to @end then the whole
 * instantiation was complete.  If *@new_end is less than @end the
 * instantiation was partial.
 *
 * Note if @new_end is NULL, then @atomic is set to true as there is no way to
 * communicate to the caller that the hole instantiation was partial.
 *
 * Return 0 on success and errno on error.
 *
 * Locking: - Caller must hold @ni->lock on the inode for writing.
 *	    - The runlist @ni must be unlocked as it is taken for writing.
 */
errno_t ntfs_attr_instantiate_holes(ntfs_inode *ni, s64 start, s64 end,
		s64 *new_end, BOOL atomic)
{
#if 0
	VCN vcn, end_vcn;
	s64 allocated_size, initialized_size, compressed_size, len;
	ntfs_inode *base_ni;
	ntfs_volume *vol = ni->vol;
	ntfs_rl_element *rl;
	MFT_RECORD *base_m, *m;
	ntfs_attr_search_ctx *ctx;
	ATTR_RECORD *a;
	errno_t err, err2;
	BOOL write_locked;
	ntfs_runlist runlist;
#else
	ntfs_volume *vol = ni->vol;
	errno_t err;
#endif

	err = 0;
	/* We should never be called for non-sparse attributes. */
	if (!NInoSparse(ni))
		panic("%s(): !NInoSparse(ni)\n", __FUNCTION__);
	/* We should never be called for resident attributes. */
	if (!NInoNonResident(ni))
		panic("%s(): !NInoNonResident(ni)\n", __FUNCTION__);
	/* We should only be called for $DATA attributes. */
	if (ni->type != AT_DATA)
		panic("%s(): ni->type != AT_DATA\n", __FUNCTION__);
	/* Sanity check @start and @end. */
	if (start >= end)
		panic("%s(): start >= end\n", __FUNCTION__);
	if (start & vol->cluster_size_mask || end & vol->cluster_size_mask)
		panic("%s(): start & vol->cluster_size_mask || "
				"end & vol->cluster_size_mask\n", __FUNCTION__);
	err = ENOTSUP;
	return err;
#if 0
	base_ni = ni;
	if (NInoAttr(ni))
		base_ni = ni->base_ni;
	if (!new_end)
		atomic = TRUE;
	lck_rw_lock_shared(&ni->rl.lock);
	write_locked = FALSE;
	/*
	 * We have to round down @start to the nearest page boundary and we
	 * have to round up @end to the nearest page boundary for the cases
	 * where the cluster size is smaller than the page size.  It makes no
	 * sense to instantiate only part of a page as a later pageout of the
	 * dirty page would cause any sparse clusters inside the page to be
	 * instantiated so we might as well do it now whilst we are
	 * instantiating things.
	 */
	vcn = (start & ~PAGE_MASK_64) >> vol->cluster_size_shift;
	end_vcn = ((end + PAGE_MASK) & ~PAGE_MASK_64) >>
			vol->cluster_size_shift;
	/* Cache the sizes for the attribute so we take the size lock once. */
	lck_spin_lock(&ni->size_lock);
	allocated_size = ni->allocated_size;
	initialized_size = ni->initialized_size;
	compressed_size = ni->compressed_size;
	lck_spin_unlock(&ni->size_lock);
	/*
	 * We have to make sure that we stay within the existing allocated
	 * size when instantiating holes as it would corrupt the attribute if
	 * we were to extend the runlist beyond the allocated size.  And our
	 * rounding up of @end above could have caused us to go above the
	 * allocated size so fix this up now.
	 */
	if (end_vcn > allocated_size >> vol->cluster_size_shift)
		end_vcn = allocated_size >> vol->cluster_size_shift;
retry_remap:
	rl = ni->rl.rl;
	if (!ni->rl.elements || vcn < rl->vcn || !rl->length) {
map_vcn:
		if (!write_locked) {
			write_locked = TRUE;
			if (!lck_rw_lock_shared_to_exclusive(&ni->rl.lock)) {
				lck_rw_lock_exclusive(&ni->rl.lock);
				goto retry_remap;
			}
		}
		/* Need to map the runlist fragment containing @vcn. */
		err = ntfs_map_runlist_nolock(ni, vcn, NULL);
		if (err) {
			ntfs_error(vol->mp, "Failed to map runlist fragment "
					"(error %d).", err);
			if (err == EINVAL)
				err = EIO;
			goto err;
		}
		rl = ni->rl.rl;
		if (!ni->rl.elements || vcn < rl->vcn || !rl->length)
			panic("%s(): !ni->rl.elements || vcn < rl[0].vcn || "
					"!rl->length\n", __FUNCTION__);
	}
	do {
		VCN lowest_vcn, highest_vcn, stop_vcn;
		ntfs_rl_element *rl2;
		unsigned mp_size, mp_ofs;

		/* Seek to the runlist element containing @vcn. */
		while (rl->length && vcn >= rl[1].vcn)
			rl++;
		/*
		 * Seek to the first sparse run or to the end of the region we
		 * are interested in.
		 */
		while (rl->length && rl->lcn >= 0 && vcn < end_vcn) {
			rl++;
			vcn = rl->vcn;
		}
		/*
		 * If there are no sparse runs (left) in the region of interest
		 * we are done.
		 */
		if (vcn >= end_vcn) {
			vcn = end_vcn;
			break;
		}
		/*
		 * If this run is not mapped map it now and start again as the
		 * runlist will have been updated.
		 */
		if (rl->lcn == LCN_RL_NOT_MAPPED)
			goto map_vcn;
		/* If this run is not valid abort with an error. */
		if (!rl->length || rl->lcn < LCN_HOLE) {
			ntfs_error(vol->mp, "Runlist is corrupt.  Unmount and "
					"run chkdsk.");
			NVolSetErrors(vol);
			err = EIO;
			goto err;
		}
		/*
		 * This run is sparse thus we need to instantiate it for which
		 * we need to hold the runlist lock for writing.
		 */
		if (!write_locked) {
			write_locked = TRUE;
			if (!lck_rw_lock_shared_to_exclusive(&ni->rl.lock)) {
				lck_rw_lock_exclusive(&ni->rl.lock);
				goto retry_remap;
			}
		}
		/*
		 * Make sure that we do not instantiate past @end_vcn as would
		 * otherwise happen when the hole goes past @end_vcn.
		 */
		len = rl[1].vcn - vcn;
		if (rl[1].vcn > end_vcn)
			len = end_vcn - vcn;
// TODO: HERE:
		/*
		 * If the entire run lies outside the initialized size we do
		 * not need to do anything other than instantiating the hole
		 * with real clusters.
		 *
		 * If part of the run (or the whole run) lies inside the
		 * initialized size we need to zero the clusters in memory and
		 * mark the pages dirty so they get written out later in
		 * addition to instantiating the hole with real clusters.
		 *
		 * The need for zeroing causes two potential problems.  The
		 * first problem is that if the run being instantiated is very
		 * large we could run out of memory due to us holding both the
		 * inode lock and the runlist lock for writing so all the dirty
		 * pages we create/release back to the VM cannot be paged out
		 * until we release the locks and the second problem is that if
		 * the cluster size is less than the page size we can encounter
		 * partially sparse pages and if they are not already cached by
		 * the VM we have to page them in.  But to do so we have to not
		 * hold the runlist lock for writing.  We have two ways out of
		 * this situation.  Either we have to drop and re-acquire the
		 * runlist lock around paging in such pages (with restarting
		 * everything each time because we had dropped the lock) or we
		 * have to read the non-sparse clusters in by hand using an
		 * enhanced ntfs_rl_read() or even by calling buf_meta_bread()
		 * directly.
		 *
		 * FIXME: We ignore the first problem for now until the code is
		 * working and we can test it.  The solution is probably to
		 * break the work into chunks of a fixed size and the allocate
		 * only enough clusters to complete the current chunk then
		 * merge that with the runlist, dirty all corresponding pages,
		 * then drop the locks to allow the pages to be written if
		 * needed and then take the locks again and start again with
		 * the next chunk.  This does have one nasty side effect and
		 * that is that whilst the locks are dropped a concurrent
		 * process could do nasty things to the inode including
		 * truncate our carefully allocated pages by shrinking the file
		 * so a lot of sanity checking after re-taking the locks will
		 * be needed.  Alternatively perhaps we need to hold the inode
		 * lock shared throughout this function so dropping the
		 * runlist lock would be sufficient.  We do not actually need
		 * the inode lock for writing in this function as we do not
		 * modify any of the inode sizes and the runlist lock will
		 * protect us sufficiently from everything.
		 *
		 * FIXME: We also ignore the second problem for now and abort
		 * if it bites us, again until the code is working and we can
		 * test it.
		 */
		/*
		 * Seek back to the last real LCN so we can try and extend the
		 * hole at that LCN so the instantiated clusters are at least
		 * in close proximity to the other data in the attribute.
		 */
		rl2 = rl;
		while (rl2->lcn < 0 && rl2 > ni->rl.rl)
			rl2--;
		runlist.rl = NULL;
		runlist.alloc = runlist.elements = 0;
		err = ntfs_cluster_alloc(vol, vcn, len,
				(rl2->lcn >= 0) ? rl2->lcn + rl2->length : -1,
				DATA_ZONE, FALSE, &runlist);
		if (err) {
			if (err != ENOSPC)
				ntfs_error(vol->mp, "Failed to allocate "
						"clusters (error %d).", err);
			goto err;
		}
// TODO: HERE:
		/*
		 * If the instantiated hole starts before the initialized size
		 * we need to zero it.
		 *
		 * FIXME: For now we do it in the most stupid way possible and
		 * that is to synchronously write zeroes to disk via the device
		 * hosting the volume.  That way we get around our issues and
		 * problems with the UBC and small/large cluster sizes.  This
		 * way if there is dirty data in the UBC it will still get
		 * written on top of the zeroing we are now doing.  Ordering is
		 * guaranteed as no-one knows about the allocated clusters yet
		 * as we have not merged the runlists yet.
		 *
		 * FIXME: TODO: It may be worth restricting ntfs_rl_set() to
		 * only operate up to the initialized size as it could
		 * otherwise do a lot of unneeded extra work.
		 */
		if (vcn << vol->cluster_size_shift < initialized_size) {
			ntfs_debug("Zeroing instantiated hole inside the "
					"initialized size.");
			if (!runlist.elements || !runlist.alloc)
				panic("%s(): !runlist.elements || "
						"!runlist.alloc\n",
						__FUNCTION__);
			err = ntfs_rl_set(vol, runlist.rl, 0);
			if (err) {
				ntfs_error(vol->mp, "Failed to zero newly "
						"allocated space (error %d).",
						err);
				goto undo_alloc;
			}
		}
		err = ntfs_rl_merge(&ni->rl, &runlist);
		if (err) {
			ntfs_error(vol->mp, "Failed to merge runlists (error "
					"%d).", err);
			goto undo_alloc;
		}
		/*
		 * The runlist may have been reallocated so @rl needs to be
		 * reset back to the beginning.
		 */
		rl = ni->rl.rl;
		/*
		 * Need to update the mapping pairs array of the attribute.  We
		 * cannot postpone this till the end (which would be much more
		 * efficient) because we could run out of space on the volume
		 * when trying to update the mapping pairs array and then we
		 * would not be able to roll back to the previous state because
		 * we would not know which bits of the runlist are new and
		 * which are old.  Doing it now means that if we get an error
		 * we still know the starting and ending VCNs of the run we
		 * instantiated so we can punch the clusters out again thus
		 * restoring the original hole.
		 */
		err = ntfs_mft_record_map(base_ni, &base_m);
		if (err) {
			ntfs_error(vol->mp, "Failed to map mft_no 0x%llx "
					"(error %d).",
					(unsigned long long)base_ni->mft_no,
					err);
			goto undo_merge;
		}
		ctx = ntfs_attr_search_ctx_get(base_ni, base_m);
		if (!ctx) {
			ntfs_error(vol->mp, "Failed to allocate attribute "
					"search context.");
			err = ENOMEM;
			goto unm_err;
		}
		/*
		 * Get the base attribute record so we can update the
		 * compressed size or so we can switch the attribute to not be
		 * sparse any more if we just filled the last hole.
		 */
		err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len, 0,
				NULL, 0, ctx);
		if (err) {
			ntfs_error(vol->mp, "Failed to lookup base attribute "
					"extent in mft_no 0x%llx (error %d).",
					(unsigned long long)base_ni->mft_no,
					err);
			goto put_err;
		}
		m = ctx->m;
		a = ctx->a;
		/*
		 * We added @len clusters thus the compressed size grows by
		 * that many clusters whilst the allocated size does not change
		 * as we have not extended the attribute.
		 */
		compressed_size += len << vol->cluster_size_shift;
		/*
		 * Determine whether the attribute is still sparse by comparing
		 * the new compressed size to the allocated size.  If the two
		 * have now become the same the attribute is no longer sparse.
		 */
		if (compressed_size >= allocated_size) {
			if (compressed_size != allocated_size)
				panic("%s(): compressed_size != "
						"allocated_size\n",
						__FUNCTION__);
			/* Switch the attribute to not be sparse any more. */
			ntfs_attr_sparse_clear(base_ni, ni, ctx);
		}
		/*
		 * If the attribute is (still) sparse or compressed, need to
		 * update the compressed size.
		 */
		if (NInoSparse(ni) || NInoCompressed(ni)) {
			lck_spin_lock(&ni->size_lock);
			ni->compressed_size = compressed_size;
			a->compressed_size = cpu_to_sle64(compressed_size);
			lck_spin_unlock(&ni->size_lock);
		}
		/*
		 * If this is the unnamed $DATA attribute also need to update
		 * the sizes in the directory entries pointing to this inode.
		 */
		if (ni == base_ni)
			NInoSetDirtySizes(ni);
		/*
		 * If the VCN we started allocating at is not in the base
		 * attribute record get the attribute record containing it so
		 * we can update the mapping pairs array.
		 */
		if (vcn > sle64_to_cpu(a->highest_vcn)) {
			/* Ensure the modified mft record is written out. */
			NInoSetMrecNeedsDirtying(ctx->ni);
			err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len,
					vcn, NULL, 0, ctx);
			if (err) {
				ntfs_error(vol->mp, "Failed to lookup "
						"attribute extent in mft_no "
						"0x%llx (error %d).",
						(unsigned long long)
						base_ni->mft_no, err);
				a = NULL;
				goto undo_sparse;
			}
			a = ctx->a;
		}
		/*
		 * Get the size for the new mapping pairs array for this
		 * attribute extent.
		 */
		lowest_vcn = sle64_to_cpu(a->lowest_vcn);
		/*
		 * Get the runlist element containing the lowest vcn.
		 *
		 * This cannot fail as we know the runlist is ok and the
		 * runlist fragment containing the lowest vcn is mapped.
		 */
		rl2 = ntfs_rl_find_vcn_nolock(rl, lowest_vcn);
		if (!rl2)
			panic("%s(): Memory corruption detected.\n",
					__FUNCTION__);
		err = ntfs_get_size_for_mapping_pairs(vol, rl2, lowest_vcn,
				highest_vcn, &mp_size);
		if (err) {
			ntfs_error(vol->mp, "Failed to get size for mapping "
					"pairs array (error %d).", err);
			goto undo_sparse;
		}
		mp_ofs = le16_to_cpu(a->mapping_pairs_offset);
retry_attr_rec_resize:
		/*
		 * Extend the attribute record to fit the bigger mapping pairs
		 * array.
		 */
		err = ntfs_attr_record_resize(m, a, mp_size + mp_ofs);
		if (!err)
			goto build_mpa;
		if (err != ENOSPC)
			panic("%s(): err != ENOSPC\n", __FUNCTION__);
		/*
		 * There is not enough space in the mft record.
		 *
		 * We need to add an attribute list attribute if it is not
		 * already present.
		 */
		if (!NInoAttrList(base_ni)) {
			err = ntfs_attr_list_add(base_ni, base_m, ctx);
			if (err || ctx->is_error) {
				if (!err)
					err = ctx->error;
				ntfs_error(vol->mp, "Failed to %s mft_no "
						"0x%llx (error %d).",
						ctx->is_error ?
						"remap extent mft record of" :
						"add attribute list attribute "
						"to", (unsigned long long)
						base_ni->mft_no, err);
				goto undo1;
			}
			/*
			 * The attribute location will have changed so update
			 * it from the search context.
			 */
			m = ctx->m;
			a = ctx->a;
			/*
			 * Retry the original attribute record resize as we may
			 * now have enough space to create the needed mapping 
			 * pairs array in the moved attribute record.
			 *
			 * This can for example happen when the attribute was
			 * moved out to an extent mft record which has much
			 * more free space than the base mft record had.
			 */
			goto retry_attr_rec_resize;
		}
		/*
		 * If this is not the only attribute record in the mft record
		 * then move it out to a new extent mft record which will allow
		 * the attribute record to grow larger thus reducing the total
		 * number of extent attribute records needed to a minimum.
		 */
		if (!ntfs_attr_record_is_only_one(m, a)) {
			lck_rw_lock_shared(&base_ni->attr_list_rl.lock);
			err = ntfs_attr_record_move(ctx);
			lck_rw_unlock_shared(&base_ni->attr_list_rl.lock);
			if (err) {
				ntfs_error(vol->mp, "Failed to move attribute "
						"extent from mft record "
						"0x%llx to an extent mft "
						"record (error %d).",
						(unsigned long long)
						ctx->ni->mft_no, err);
				/*
				 * We could try to remove the attribute list
				 * attribute if we added it above but this
				 * would probably require attributes to be
				 * moved back into the base mft record from
				 * extent mft records so is a lot of work and
				 * given we are in an error code path and given
				 * that it is ok to just leave the inode with
				 * an attribute list attribute we do not bother
				 * and just bail out.
				 */
				goto undo1;
			}
			/*
			 * The attribute location will have changed so update
			 * it from the search context.
			 */
			m = ctx->m;
			a = ctx->a;
			/*
			 * Retry the original attribute record resize as we may
			 * now have enough space to create the mapping pairs
			 * array in the moved attribute record.
			 */
			goto retry_attr_rec_resize;
		}
		max_size = (le32_to_cpu(m->bytes_allocated) -
				le32_to_cpu(m->bytes_in_use)) & ~7;
		max_size += le32_to_cpu(a->length) - mp_ofs;
		err = ntfs_attr_record_resize(m, a, max_size + mp_ofs);
		/*
		 * We worked out the exact size we can extend to so the resize
		 * cannot fail.
		 */
		if (err)
			panic("%s(): err (ntfs_attr_record_resize())\n",
					__FUNCTION__);
build_mpa:
// TODO: HERE...
		mp_rebuilt = TRUE;
		/*
		 * Generate the mapping pairs array directly into the attribute
		 * record.
		 *
		 * This cannot fail as we have already checked the size we need
		 * to build the mapping pairs array.
		 */
		err = ntfs_mapping_pairs_build(vol, (s8*)a + mp_ofs,
				le32_to_cpu(a->length) - mp_ofs, rl2,
				lowest_vcn, highest_vcn, &stop_vcn);
		if (err && err != ENOSPC) {
			ntfs_error(vol->mp, "Cannot fill hole of mft_no "
					"0x%llx, attribute type 0x%x, because "
					"building the mapping pairs array "
					"failed (error %d).",
					(unsigned long long)ni->mft_no,
					(unsigned)le32_to_cpu(ni->type), err);
			err = EIO;
			/*
			 * Need to set @a->highest_vcn to enable correct error
			 * recovery.
			 */
// TODO: HERE...
			if (!is_first)
				a->highest_vcn = cpu_to_sle64(sle64_to_cpu(
						a->lowest_vcn) - 1);
			goto undo;
		}
		/* Update the highest_vcn. */
		a->highest_vcn = cpu_to_sle64(stop_vcn - 1);
		/* Ensure the modified mft record is written out. */
		NInoSetMrecNeedsDirtying(ctx->ni);
		/*
		 * If the mapping pairs build succeeded, i.e. the current
		 * attribute extent contains the whole runlist fragment, we are
		 * done and can proceed to the next run.
		 */
		if (!err)
			goto next_run;
		/*
		 * Partial mapping pairs update.  This means we need to create
		 * one or me new attribute extents to hold the remainder of the
		 * mapping pairs.
		 *
		 * Get the size of the remaining mapping pairs array.
		 */
		rl2 = ntfs_rl_find_vcn_nolock(rl2, stop_vcn);
		if (!rl2)
			panic("%s(): !rl2 (stop_vcn)\n", __FUNCTION__);
		if (!rl2->length)
			panic("%s(): !rl2->length (stop_vcn)\n", __FUNCTION__);
		if (rl2->lcn < LCN_HOLE)
			panic("%s(): rl2->lcn < LCN_HOLE (stop_vcn)\n",
					__FUNCTION__);
		err = ntfs_get_size_for_mapping_pairs(vol, rl2, stop_vcn,
				highest_vcn, &mp_size);
		if (err) {
			ntfs_error(vol->mp, "Cannot complete filling of hole "
					"of mft_no 0x%llx, attribute type "
					"0x%x, because determining the size "
					"for the mapping pairs failed (error "
					"%d).", (unsigned long long)ni->mft_no,
					(unsigned)le32_to_cpu(ni->type), err);
			err = EIO;
// TODO: HERE...
			goto undo;
		}
		/* We only release extent mft records. */
		if (ctx->ni != base_ni)
			ntfs_extent_mft_record_unmap(ctx->ni);
// TODO: I AM HERE...  Need to allocate an extent mft record, add an extent
// attribute record to it filling it with remaining mapping pairs array fragment
// and creating an attribute list attribute entry for it.  Then if still not
// reached highest_vcn, need to repeat the process again.
next_run:
		ntfs_attr_search_ctx_put(ctx);
		ntfs_mft_record_unmap(base_ni);
		/*
		 * If the attribute is no longer sparse there are no more holes
		 * to instantiate thus we are done with the whole region of
		 * interest.
		 */
		if (!NInoSparse(ni)) {
			vcn = end_vcn;
			break;
		}
		/*
		 * We allocated @len clusters starting at @vcn.  Thus the next
		 * VCN we need to look at is at @vcn + @len.
		 */
		vcn += len;
	} while (vcn < end_vcn);
	if (vcn > end_vcn)
		panic("%s(): vcn > end_vcn\n", __FUNCTION__);
	ntfs_debug("Done, new_end 0x%llx.",
			(unsigned long long)vcn << vol->cluster_size_shift);
err:
	if (new_end)
		*new_end = vcn << vol->cluster_size_shift;
	if (write_locked)
		lck_rw_unlock_exclusive(&ni->rl.lock);
	else
		lck_rw_unlock_shared(&ni->rl.lock);
	return err;
undo_alloc:
	err2 = ntfs_cluster_free_from_rl(vol, runlist.rl, 0, -1, NULL);
	if (err2) {
		ntfs_error(vol->mp, "Failed to release allocated cluster(s) "
				"in error code path (error %d).  Run chkdsk "
				"to recover the lost space.", err2);
		NVolSetErrors(vol);
	}
	OSFree(runlist.rl, runlist.alloc, ntfs_malloc_tag);
	goto err;
undo_sparse:
	/*
	 * If looking up an attribute extent failed or we are not in the base
	 * attribute record need to look up the base attribute record.
	 */
	if (!a || a->lowest_vcn) {
		ntfs_attr_search_ctx_reinit(ctx);
		err2 = ntfs_attr_lookup(ni->type, ni->name, ni->name_len, 0,
				NULL, 0, ctx);
		if (err2) {
			ntfs_error(vol->mp, "Failed to re-lookup base "
					"attribute record in error code path "
					"(error %d).  Leaving inconsistent "
					"metadata.  Unmount and run chkdsk.",
					err2);
			NVolSetErrors(vol);
			goto put_err;
		}
		a = ctx->a;
	}
	/*
	 * If we caused the attribute to no longer be sparse we need to make it
	 * sparse again.
	 */
	if (!NInoSparse(ni)) {
		err2 = ntfs_attr_sparse_set(base_ni, ni, ctx);
		if (err2) {
			ntfs_error(vol->mp, "Failed to re-set the attribute "
					"to be sparse in error code path "
					"(error %d).  Leaving inconsistent "
					"metadata.  Unmount and run chkdsk.",
					err2);
			NVolSetErrors(vol);
			goto put_err;
		}
		/*
		 * The attribute may have been moved to make space for the
		 * compressed size so @a is now invalid.
		 */
		a = ctx->a;
	}
	/* Restore the compressed size to the old value. */
	compressed_size -= len << vol->cluster_size_shift;
	lck_spin_lock(&ni->size_lock);
	ni->compressed_size = compressed_size;
	a->compressed_size = cpu_to_sle64(compressed_size);
	lck_spin_unlock(&ni->size_lock);
	/* Ensure the modified mft record is written out. */
	NInoSetMrecNeedsDirtying(ctx->ni);
	if (ni == base_ni)
		NInoSetDirtySizes(ni);
put_err:
	ntfs_attr_search_ctx_put(ctx);
unm_err:
	ntfs_mft_record_unmap(base_ni);
undo_merge:
	/* Free the clusters we allocated. */
	err2 = ntfs_cluster_free_from_rl(vol, rl, vcn, len, NULL);
	if (err2) {
		ntfs_error(vol->mp, "Failed to release allocated cluster(s) "
				"in error code path (error %d).  Unmount and "
				"run chkdsk to recover the lost space.", err2);
		NVolSetErrors(vol);
	}
	/* Punch the original hole back into the runlist. */
	err2 = ntfs_rl_punch_nolock(vol, &ni->rl, vcn, len);
	if (err2) {
		ntfs_error(vol->mp, "Failed to restore hole in error code "
				"path in error code path (error %d).  Leaving "
				"inconsistent metadata.  Unmount and run "
				"chkdsk.", err2);
		NVolSetErrors(vol);
	}
	goto err;
undo1:
	panic("%s(): TODO\n", __FUNCTION__);
	return err;
#endif
}

/**
 * ntfs_attr_extend_allocation - extend the allocated space of an attribute
 * @ni:			ntfs inode of the attribute whose allocation to extend
 * @new_alloc_size:	new size in bytes to which to extend the allocation to
 * @new_data_size:	new size in bytes to which to extend the data to
 * @data_start:		beginning of region which is required to be non-sparse
 * @ictx:		index context
 * @dst_alloc_size:	if not NULL, this pointer is set to the allocated size
 * @atomic:		if true must complete the entire exension or abort
 *
 * Extend the allocated space of an attribute described by the ntfs inode @ni
 * to @new_alloc_size bytes.  If @data_start is -1, the whole extension may be
 * implemented as a hole in the file (as long as both the volume and the ntfs
 * inode @ni have sparse support enabled).  If @data_start is >= 0, then the
 * region between the old allocated size and @data_start - 1 may be made sparse
 * but the regions between @data_start and @new_alloc_size must be backed by
 * actual clusters.
 *
 * If @new_data_size is -1, it is ignored.  If it is >= 0, then the data size
 * of the attribute is extended to @new_data_size and the UBC size of the VFS
 * vnode is updated to match.
 * WARNING: It is a bug for @new_data_size to be smaller than the old data size
 * as well as for @new_data_size to be greater than @new_alloc_size.
 *
 * If @ictx is not NULL, the extension is for an index allocation or bitmap
 * attribute extension.  In this case, if there is not enough space in the mft
 * record for the extended index allocation/bitmap attribute, the index root is
 * moved to an index block if it is not empty to create more space in the mft
 * record.  NOTE: At present @ictx is only set when the attribute being resized
 * is non-resident.
 *
 * If @atomic is true only return success if the entire extension is complete.
 * If only a partial extension is possible abort with an appropriate error.  If
 * @atomic is false partial extensions are acceptable in certain circumstances
 * (see below).
 *
 * For resident attributes extending the allocation involves resizing the
 * attribute record and if necessary moving it and/or other attributes into
 * extent mft records and/or converting the attribute to a non-resident
 * attribute which in turn involves extending the allocation of a non-resident
 * attribute as described below.
 *
 * For non-resident attributes this involves allocating clusters in the data
 * zone on the volume (except for regions that are being made sparse) and
 * extending the run list to describe the allocated clusters as well as
 * updating the mapping pairs array of the attribute.  This in turn involves
 * resizing the attribute record and if necessary moving it and/or other
 * attributes into extent mft records and/or splitting the attribute record
 * into multiple extent attribute records.
 *
 * Also, the attribute list attribute is updated if present and in some of the
 * above cases (the ones where extent mft records/attributes come into play),
 * an attribute list attribute is created if not already present.
 *
 * Return 0 on success and errno on error.
 *
 * In the case that an error is encountered but a partial extension at least up
 * to @data_start (if present) is possible, the allocation is partially
 * extended and success is returned.  If @data_start is -1 then partial
 * allocations are not performed.
 *
 * If @dst_alloc_size is not NULL, then *@dst_alloc_size is set to the new
 * allocated size when the ntfs_attr_extend_allocation() returns success.  If
 * an error is returned *@dst_alloc_size is undefined.  This is useful so that
 * the caller has a simple way of checking whether or not the allocation was
 * partial.
 *
 * Thus if @data_start is not -1 the caller should supply @dst_alloc_size and
 * then compare *@dst_alloc_size to @new_alloc_size to determine if the
 * allocation was partial.  And if @data_start is -1 there is no point in
 * supplying @dst_alloc_size as *@dst_alloc_size will always be equal to
 * @new_alloc_size.
 *
 * Locking: - Caller must hold @ni->lock on the inode for writing.
 *	    - The runlist @ni must be unlocked as it is taken for writing.
 */
errno_t ntfs_attr_extend_allocation(ntfs_inode *ni, s64 new_alloc_size,
		const s64 new_data_size, const s64 data_start,
		ntfs_index_context *ictx, s64 *dst_alloc_size,
		const BOOL atomic)
{
	VCN vcn, lowest_vcn, stop_vcn;
	s64 start, ll, old_alloc_size, alloc_size, alloc_start, alloc_end;
	s64 nr_allocated, nr_freed;
	ntfs_volume *vol = ni->vol;
	ntfs_inode *base_ni;
	MFT_RECORD *base_m, *m;
	ATTR_RECORD *a;
	ntfs_attr_search_ctx *actx;
	ntfs_rl_element *rl;
	unsigned attr_len, arec_size, name_size, mp_size, mp_ofs, max_size;
	unsigned al_entry_len, new_al_alloc;
	errno_t err, err2;
	BOOL is_sparse, is_first, mp_rebuilt, al_entry_added;
	ntfs_runlist runlist;

	start = data_start;
#ifdef DEBUG
	lck_spin_lock(&ni->size_lock);
	old_alloc_size = ni->allocated_size;
	lck_spin_unlock(&ni->size_lock);
	ntfs_debug("Entering for mft_no 0x%llx, attribute type 0x%x, "
			"old_allocated_size 0x%llx, "
			"new_allocated_size 0x%llx, new_data_size 0x%llx, "
			"data_start 0x%llx.", (unsigned long long)ni->mft_no,
			(unsigned)le32_to_cpu(ni->type),
			(unsigned long long)old_alloc_size,
			(unsigned long long)new_alloc_size,
			(unsigned long long)new_data_size,
			(unsigned long long)start);
#endif
	/* This cannot be called for the attribute list attribute. */
	if (ni->type == AT_ATTRIBUTE_LIST)
		panic("%s(): ni->type == AT_ATTRIBUTE_LIST\n", __FUNCTION__);
	name_size = ni->name_len * sizeof(ntfschar);
	base_ni = ni;
	if (NInoAttr(ni))
		base_ni = ni->base_ni;
	is_first = TRUE;
retry_extend:
	/*
	 * For non-resident attributes, @start and @new_size need to be aligned
	 * to cluster boundaries for allocation purposes.
	 */
	if (NInoNonResident(ni)) {
		if (start > 0)
			start &= ~(s64)vol->cluster_size_mask;
		new_alloc_size = (new_alloc_size + vol->cluster_size - 1) &
				~(s64)vol->cluster_size_mask;
	}
	if (new_data_size >= 0 && new_data_size > new_alloc_size)
		panic("%s(): new_data_size >= 0 && new_data_size > "
				"new_alloc_size\n", __FUNCTION__);
	/* Check if new size is allowed in $AttrDef. */
	err = ntfs_attr_size_bounds_check(vol, ni->type, new_alloc_size);
	if (err) {
		/* Only emit errors when the write will fail completely. */
		lck_spin_lock(&ni->size_lock);
		old_alloc_size = ni->allocated_size;
		lck_spin_unlock(&ni->size_lock);
		if (start < 0 || start >= old_alloc_size) {
			if (err == ERANGE) {
				ntfs_error(vol->mp, "Cannot extend allocation "
						"of mft_no 0x%llx, attribute "
						"type 0x%x, because the new "
						"allocation would exceed the "
						"maximum allowed size for "
						"this attribute type.",
						(unsigned long long)ni->mft_no,
						(unsigned)
						le32_to_cpu(ni->type));
			} else {
				ntfs_error(vol->mp, "Cannot extend allocation "
						"of mft_no 0x%llx, attribute "
						"type 0x%x, because this "
						"attribute type is not "
						"defined on the NTFS volume.  "
						"Possible corruption!  You "
						"should run chkdsk!",
						(unsigned long long)ni->mft_no,
						(unsigned)
						le32_to_cpu(ni->type));
			}
		}
		/* Translate error code to be POSIX conformant for write(2). */
		if (err == ERANGE)
			err = EFBIG;
		else
			err = EIO;
		return err;
	}
	/*
	 * We will be modifying both the runlist (if non-resident) and the mft
	 * record so lock them both down.
	 */
	lck_rw_lock_exclusive(&ni->rl.lock);
	err = ntfs_mft_record_map(base_ni, &base_m);
	if (err) {
		base_m = NULL;
		actx = NULL;
		goto err_out;
	}
	actx = ntfs_attr_search_ctx_get(base_ni, base_m);
	if (!actx) {
		err = ENOMEM;
		goto err_out;
	}
	lck_spin_lock(&ni->size_lock);
	alloc_size = ni->allocated_size;
	lck_spin_unlock(&ni->size_lock);
	/*
	 * If non-resident, seek to the last extent.  If resident, there is
	 * only one extent, so seek to that.
	 */
	vcn = (NInoNonResident(ni) && alloc_size > 0) ?
			(alloc_size - 1) >> vol->cluster_size_shift : 0;
	/*
	 * Abort if someone did the work whilst we waited for the locks.  If we
	 * just converted the attribute from resident to non-resident it is
	 * likely that exactly this has happened already.  We cannot quite
	 * abort if we need to update the data size.
	 */
	if (new_alloc_size <= alloc_size) {
		ntfs_debug("Allocated size already exceeds requested size.");
		new_alloc_size = alloc_size;
		if (new_data_size < 0)
			goto done;
		/*
		 * We want the first attribute extent so that we can update the
		 * data size.
		 */
		vcn = 0;
	}
	err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len, vcn, NULL, 0,
			actx);
	if (err) {
		if (err == ENOENT)
			err = EIO;
		goto err_out;
	}
	m = actx->m;
	a = actx->a;
	/* Use goto to reduce indentation. */
	if (a->non_resident)
		goto do_non_resident_extend;
	if (NInoNonResident(ni))
		panic("%s(): NInoNonResident(ni)\n", __FUNCTION__);
	/*
	 * As things are now this function should never be called with an index
	 * context for the resize of a resident attribute.
	 */
	if (ictx)
		panic("%s(): ictx\n", __FUNCTION__);
	/* The total length of the attribute value. */
	attr_len = le32_to_cpu(a->value_length);
	/*
	 * Extend the attribute record to be able to store the new attribute
	 * size.  ntfs_attr_record_resize() will not do anything if the size is
	 * not changing.
	 */
	arec_size = (le16_to_cpu(a->value_offset) + new_alloc_size + 7) & ~7;
	if (arec_size < le32_to_cpu(m->bytes_allocated) -
			le32_to_cpu(m->bytes_in_use) &&
			!ntfs_attr_record_resize(m, a, arec_size)) {
		/* The resize succeeded! */
		if (new_data_size > attr_len) {
			if (!ubc_setsize(ni->vn, new_data_size)) {
				ntfs_error(vol->mp, "Failed to set size in "
						"UBC.");
				/*
				 * This cannot fail as it is a shrinking
				 * resize.
				 */
				lck_spin_lock(&ni->size_lock);
				err = ntfs_attr_record_resize(m, a,
						le16_to_cpu(a->value_offset) +
						ni->allocated_size);
				lck_spin_unlock(&ni->size_lock);
				if (err)
					panic("%s(): Failed to shrink "
							"resident attribute "
							"record (error %d)\n",
							__FUNCTION__, err);
				err = EIO;
				goto err_out;
			}
			/* Zero the extended attribute value. */
			bzero((u8*)a + le16_to_cpu(a->value_offset) + attr_len,
					(u32)new_data_size - attr_len);
			lck_spin_lock(&ni->size_lock);
			ni->initialized_size = ni->data_size = new_data_size;
			a->value_length = cpu_to_le32((u32)new_data_size);
		} else
			lck_spin_lock(&ni->size_lock);
		ni->allocated_size = le32_to_cpu(a->length) -
				le16_to_cpu(a->value_offset);
		lck_spin_unlock(&ni->size_lock);
		if (new_data_size > attr_len)
			a->value_length = cpu_to_le32((u32)new_data_size);
		goto dirty_done;
	}
	/*
	 * We have to drop all the locks so we can call
	 * ntfs_attr_make_non_resident().
	 */
	ntfs_attr_search_ctx_put(actx);
	ntfs_mft_record_unmap(base_ni);
	lck_rw_unlock_exclusive(&ni->rl.lock);
	/*
	 * Not enough space in the mft record, try to make the attribute
	 * non-resident and if successful restart the extension process.
	 */
	err = ntfs_attr_make_non_resident(ni);
	if (!err)
		goto retry_extend;
	/*
	 * Could not make non-resident.  If this is due to this not being
	 * permitted for this attribute type try to make other attributes
	 * non-resident and/or move this or other attributes out of the mft
	 * record this attribute is in.  Otherwise fail.
	 */
	if (err != EPERM) {
		if (err != ENOSPC) {
			/*
			 * Only emit errors when the write will fail
			 * completely.
			 */
			lck_spin_lock(&ni->size_lock);
			old_alloc_size = ni->allocated_size;
			lck_spin_unlock(&ni->size_lock);
			if (start < 0 || start >= old_alloc_size)
				ntfs_error(vol->mp, "Cannot extend allocation "
						"of mft_no 0x%llx, attribute "
						"type 0x%x, because the "
						"conversion from resident to "
						"non-resident attribute "
						"failed (error %d).",
						(unsigned long long)ni->mft_no,
						(unsigned)le32_to_cpu(ni->type),
						err);
			if (err != ENOMEM) {
				NVolSetErrors(vol);
				err = EIO;
			}
		}
		goto conv_err_out;
	}
	/*
	 * To make space in the mft record we would like to try to make other
	 * attributes non-resident if that would save space.
	 *
	 * FIXME: We cannot do this at present unless the attribute is the
	 * attribute being resized as there could be an ntfs inode matching
	 * this attribute in memory and it would become out of date with its
	 * metadata if we touch its attribute record.
	 *
	 * FIXME: We do not need to do this if this is the attribute being
	 * resized as we already tried to make the attribute non-resident and
	 * it did not work or we would never have gotten here in the first
	 * place.
	 *
	 * Thus we have to either move other attributes to extent mft records
	 * thus making more space in the base mft record or we have to move the
	 * attribute being resized to an extent mft record thus giving it more
	 * space.  In any case we need to have an attribute list attribute so
	 * start by adding it if it does not yet exist.
	 *
	 * Before we start, we can check whether it is possible to fit the
	 * attribute to be resized inside an mft record.  If not then there is
	 * no point in proceeding.
	 *
	 * This should never really happen as the attribute size should never
	 * be allowed to grow so much and such requests should never be made by
	 * the driver and if they are they should be caught by the call to
	 * ntfs_attr_size_bounds_check().
	 */
	if (arec_size > vol->mft_record_size - sizeof(MFT_RECORD)) {
		/* Only emit errors when the write will fail completely. */
		lck_spin_lock(&ni->size_lock);
		old_alloc_size = ni->allocated_size;
		lck_spin_unlock(&ni->size_lock);
		if (start < 0 || start >= old_alloc_size)
			ntfs_error(vol->mp, "Cannot extend allocation of "
					"mft_no 0x%llx, attribute type 0x%x, "
					"because the attribute may not be "
					"non-resident and the requested size "
					"exceeds the maximum possible "
					"resident attribute record size.",
					(unsigned long long)ni->mft_no,
					(unsigned)le32_to_cpu(ni->type));
		/* Use POSIX conformant write(2) error code. */
		err = EFBIG;
		goto conv_err_out;
	}
	/*
	 * The resident attribute can fit in an mft record.  Now have to decide
	 * whether to make other attributes non-resident/move other attributes
	 * out of the mft record or whether to move the attribute record to be
	 * resized out to a new mft record.
	 *
	 * TODO: We never call ntfs_attr_extend_allocation() for attributes
	 * that cannot be non-resident thus we never get here thus we simply
	 * panic() here to remind us that we need to implement this code if we
	 * ever start calling this function for attributes that must remain
	 * resident.
	 */
	panic("%s(): Attribute may not be non-resident.\n", __FUNCTION__);
do_non_resident_extend:
	if (!NInoNonResident(ni))
		panic("%s(): !NInoNonResident(ni)\n", __FUNCTION__);
	if (new_alloc_size == alloc_size) {
		if (vcn)
			panic("%s(): vcn\n", __FUNCTION__);
		goto alloc_done;
	}
	/*
	 * We are going to allocate starting at the old allocated size and are
	 * going to allocate up to the new allocated size.
	 */
	alloc_start = alloc_size;
	rl = NULL;
	if (ni->rl.elements) {
		/* Seek to the end of the runlist. */
		rl = &ni->rl.rl[ni->rl.elements - 1];
	}
	/*
	 * Cache the lowest VCN for later.  Need to do it here to silence
	 * compiler warning about possible use of uninitialiezd variable.
	 */
	lowest_vcn = sle64_to_cpu(a->lowest_vcn);
	/* If this attribute extent is not mapped, map it now. */
	if (alloc_size > 0 && (!ni->rl.elements ||
			rl->lcn == LCN_RL_NOT_MAPPED ||
			(rl->lcn == LCN_ENOENT && rl > ni->rl.rl &&
			(rl-1)->lcn == LCN_RL_NOT_MAPPED))) {
		err = ntfs_mapping_pairs_decompress(vol, a, &ni->rl);
		if (err || !ni->rl.elements) {
			if (!err)
				err = EIO;
			if (start < 0 || start >= alloc_size)
				ntfs_error(vol->mp, "Cannot extend allocation "
						"of mft_no 0x%llx, attribute "
						"type 0x%x, because the "
						"mapping of a runlist "
						"fragment failed (error %d).",
						(unsigned long long)ni->mft_no,
						(unsigned)le32_to_cpu(ni->type),
						err);
			if (err != ENOMEM)
				err = EIO;
			goto err_out;
		}
		/* Seek to the end of the runlist. */
		rl = &ni->rl.rl[ni->rl.elements - 1];
	}
	/*
	 * We now know the runlist of the last extent is mapped and @rl is at
	 * the end of the runlist.  We want to begin extending the runlist.
	 *
	 * If the data starts after the end of the old allocation or no data
	 * start is specified (@start < 0), this is a $DATA attribute and
	 * sparse attributes are enabled on the volume and for this inode, then
	 * create a sparse region between the old allocated size and the start
	 * of the data or the new allocated size if no data start is specified.
	 * Otherwise proceed with filling the whole space between the old
	 * allocated size and the new allocated size with clusters.
	 */
	if ((start >= 0 && start <= alloc_size) || ni->type != AT_DATA ||
			!NVolSparseEnabled(vol) || NInoSparseDisabled(ni)) {
		is_sparse = FALSE;
		goto skip_sparse;
	}
	/*
	 * If @start is less than zero we create the sparse region from the old
	 * allocated size to the new allocated size.  Otherwise we end the
	 * sparse region at @start and fill with real clusters between @start
	 * and the new allocated size.
	 */
	alloc_end = start;
	if (start < 0)
		alloc_end = new_alloc_size;
	ntfs_debug("Adding hole starting at byte offset 0x%llx and finishing "
			"at byte offset 0x%llx.",
			(unsigned long long)alloc_start,
			(unsigned long long)alloc_end);
	/*
	 * Allocate more memory if needed.  We ensure there is space at least
	 * for two new elements as this is what needs to happen when this is
	 * the very first allocation, i.e. the file has zero clusters allocated
	 * at the moment.
	 */
	if ((ni->rl.elements + 2) * sizeof(*rl) > ni->rl.alloc) {
		ntfs_rl_element *rl2;

		rl2 = OSMalloc(ni->rl.alloc + NTFS_ALLOC_BLOCK,
				ntfs_malloc_tag);
		if (!rl2) {
			err = ENOMEM;
			goto err_out;
		}
		if (ni->rl.elements) {
			memcpy(rl2, ni->rl.rl, ni->rl.elements * sizeof(*rl2));
			/* Seek to the end of the runlist. */
			rl = &rl2[ni->rl.elements - 1];
		}
		if (ni->rl.alloc)
			OSFree(ni->rl.rl, ni->rl.alloc, ntfs_malloc_tag);
		ni->rl.rl = rl2;
		ni->rl.alloc += NTFS_ALLOC_BLOCK;
	}
	if (ni->rl.elements) {
		/* Sanity check that this is the end element. */
		if (rl->length || rl->lcn >= LCN_HOLE)
			panic("%s(): rl->length || rl->lcn >= LCN_HOLE)\n",
					__FUNCTION__);
	} else /* if (!ni->rl.elements) */ {
		/*
		 * The runlist is empty thus we are now creating both the
		 * sparse element and the end element.  Thus need to set
		 * everything up so we end up with two new elements rather than
		 * one.
		 *
		 * Note we do not need to set up @rl->lcn and @rl->length as
		 * they are both unconditionally overwritten below.
		 */
		if (alloc_size > 0)
			panic("%s(): alloc_size > 0\n", __FUNCTION__);
		rl = ni->rl.rl;
		rl->vcn = 0;
		ni->rl.elements = 1;
	}
	/*
	 * If a last real element exists and it is sparse, need to extend it
	 * instead of adding a new hole.
	 *
	 * Replace the terminator element with a sparse element and add a new
	 * terminator.  We know this is the end of the attribute thus we can
	 * use LCN_ENOENT even if the old terminator was LCN_RL_NOT_MAPPED.
	 */
	if (rl->vcn != alloc_start >> vol->cluster_size_shift)
		panic("%s(): rl->vcn != alloc_start >> "
				"vol->cluster_size_shift\n", __FUNCTION__);
	if (ni->rl.elements > 1 && (rl - 1)->lcn == LCN_HOLE)
		rl--;
	else {
		rl->lcn = LCN_HOLE;
		rl[1].length = 0;
		ni->rl.elements++;
	}
	rl[1].vcn = alloc_end >> vol->cluster_size_shift;
	if (rl[1].vcn <= rl->vcn)
		panic("%s(): rl[1].vcn <= rl->vcn\n", __FUNCTION__);
	rl->length = rl[1].vcn - rl->vcn;
	rl[1].lcn = LCN_ENOENT;
	is_sparse = TRUE;
	/*
	 * If the entire extension is sparse skip the allocation of real
	 * clusters and proceed to updating the mapping pairs array.
	 */
	if (start < 0) {
		nr_allocated = 0;
		goto skip_real_alloc;
	}
	/*
	 * We allocated part of the extension as a hole, now we are going to
	 * allocate the remainder of the extension with real clusters.
	 */
	alloc_start = start;
skip_sparse:
	/*
	 * We want to begin allocating clusters starting at the last allocated
	 * cluster to reduce fragmentation.  If there are no valid LCNs in the
	 * attribute we let the cluster allocator choose the starting cluster.
	 *
	 * If the last LCN is a hole or similar seek back to last real LCN.
	 */
	if (ni->rl.elements) {
		while (rl->lcn < 0 && rl > ni->rl.rl)
			rl--;
	}
	// FIXME: Need to implement partial allocations so at least part of the
	// write can be performed when @start >= 0 (and hence @data_start >= 0).
	// This is needed for POSIX write(2) conformance.  But do not allow
	// partial allocations for non-DATA attributes as partial metadata is
	// no use.  The @start >= 0 check may be sufficient to exclude non-data
	// attributes...
	// FIXME: When we implement partial allocations we need to only allow
	// them to happen when @atomic is false.
	runlist.rl = NULL;
	runlist.alloc = runlist.elements = 0;
	nr_allocated = (new_alloc_size - alloc_start) >>
			vol->cluster_size_shift;
	err = ntfs_cluster_alloc(vol, alloc_start >> vol->cluster_size_shift,
			nr_allocated, (ni->rl.elements && (rl->lcn >= 0)) ?
			rl->lcn + rl->length : -1, DATA_ZONE, TRUE, &runlist);
	if (err) {
		if (start < 0 || start >= alloc_size)
			ntfs_error(vol->mp, "Cannot extend allocation of "
					"mft_no 0x%llx, attribute type 0x%x, "
					"because the allocation of clusters "
					"failed (error %d).",
					(unsigned long long)ni->mft_no,
					(unsigned)le32_to_cpu(ni->type), err);
		if (err != ENOMEM && err != ENOSPC)
			err = EIO;
		nr_allocated = 0;
		goto trunc_err_out;
	}
	err = ntfs_rl_merge(&ni->rl, &runlist);
	if (err) {
		if (start < 0 || start >= alloc_size)
			ntfs_error(vol->mp, "Cannot extend allocation of "
					"mft_no 0x%llx, attribute type 0x%x, "
					"because the runlist merge failed "
					"(error %d).",
					(unsigned long long)ni->mft_no,
					(unsigned)le32_to_cpu(ni->type), err);
		if (err != ENOMEM)
			err = EIO;
		err2 = ntfs_cluster_free_from_rl(vol, runlist.rl, 0, -1,
				NULL);
		if (err2) {
			ntfs_error(vol->mp, "Failed to release allocated "
					"cluster(s) in error code path (error "
					"%d).  Run chkdsk to recover the lost "
					"space.", err2);
			NVolSetErrors(vol);
		}
		OSFree(runlist.rl, runlist.alloc, ntfs_malloc_tag);
		nr_allocated = 0;
		goto trunc_err_out;
	}
	ntfs_debug("Allocated 0x%llx clusters.",
			(unsigned long long)(new_alloc_size - alloc_start) >>
			vol->cluster_size_shift);
skip_real_alloc:
	/* Find the runlist element with which the attribute extent starts. */
	rl = ntfs_rl_find_vcn_nolock(ni->rl.rl, lowest_vcn);
	if (!rl)
		panic("%s(): !rl\n", __FUNCTION__);
	if (!rl->length)
		panic("%s(): !rl->length\n", __FUNCTION__);
	if (rl->lcn < LCN_HOLE)
		panic("%s(): rl->lcn < LCN_HOLE\n", __FUNCTION__);
	mp_rebuilt = FALSE;
	attr_len = le32_to_cpu(a->length);
	/* Get the size for the new mapping pairs array for this extent. */
	err = ntfs_get_size_for_mapping_pairs(vol, rl, lowest_vcn, -1,
			&mp_size);
	if (err) {
		if (start < 0 || start >= alloc_size)
			ntfs_error(vol->mp, "Cannot extend allocation of "
					"mft_no 0x%llx, attribute type 0x%x, "
					"because determining the size for the "
					"mapping pairs failed (error %d).",
					(unsigned long long)ni->mft_no,
					(unsigned)le32_to_cpu(ni->type), err);
		err = EIO;
		goto undo_alloc;
	}
	mp_ofs = le16_to_cpu(a->mapping_pairs_offset);
retry_attr_rec_resize:
	/* Extend the attribute record to fit the bigger mapping pairs array. */
	err = ntfs_attr_record_resize(m, a, mp_size + mp_ofs);
	if (!err)
		goto build_mpa;
	if (err != ENOSPC)
		panic("%s(): err != ENOSPC\n", __FUNCTION__);
	/*
	 * Not enough space in the mft record.  If this is an index related
	 * extension, check if the index root attribute is in the same mft
	 * record as the attribute being extended and if it is and it is not
	 * empty move its entries into an index allocation block.  Note we do
	 * not check whether that actually creates enough space because how
	 * much space is needed exactly is very hard to determine in advance
	 * (due to potential need for associated attribute list attribute
	 * extensions) and also because even if it does not create enough space
	 * it will still help and save work later on when working for example
	 * on the attribute list attribute.
	 */
	if (ictx) {
		long delta;
		INDEX_ROOT *ir;
		INDEX_HEADER *ih;
		INDEX_ENTRY *ie, *first_ie;
		ntfs_index_context *root_ictx;
		ntfs_attr_search_ctx root_actx;

		if (ni->type != AT_INDEX_ALLOCATION && ni->type != AT_BITMAP)
			panic("%s(): ni->type != AT_INDEX_ALLOCATION && "
					"ni->type != AT_BITMAP\n",
					__FUNCTION__);
		ntfs_attr_search_ctx_init(&root_actx, actx->ni, m);
		err = ntfs_attr_find_in_mft_record(AT_INDEX_ROOT, ni->name,
				ni->name_len, NULL, 0, &root_actx);
		if (err) {
			if (err != ENOENT) {
				ntfs_error(vol->mp, "Failed to find index "
						"root attribute in mft_no "
						"0x%llx (error %d).  Inode is "
						"corrupt.  Run chkdsk.",
						(unsigned long long)ni->mft_no,
						err);
				NVolSetErrors(vol);
			}
			/*
			 * The index root is in a different mft record so we
			 * cannot gain anything by moving out its entries.  Set
			 * @ictx to NULL so we do not waste our time trying
			 * again.
			 */
			ictx = NULL;
			goto ictx_done;
		}
		/*
		 * We found the index root in the same mft record as the
		 * attribute (extent) to be extended.  Check whether it is
		 * empty or not.
		 */
		ir = (INDEX_ROOT*)((u8*)root_actx.a +
				le16_to_cpu(root_actx.a->value_offset));
		ih = &ir->index;
		first_ie = ie = (INDEX_ENTRY*)((u8*)ih +
				le32_to_cpu(ih->entries_offset));
		while (!(ie->flags & INDEX_ENTRY_END))
			ie = (INDEX_ENTRY*)((u8*)ie + le16_to_cpu(ie->length));
		/*
		 * If there are no entries other than the end entry we cannot
		 * gain anything by moving out the entries from the index root.
		 * Set @ictx to NULL so we do not waste our time trying again.
		 */
		if (ie == first_ie) {
			ictx = NULL;
			goto ictx_done;
		}
		/*
		 * We cannot have gotten this far if the current index context
		 * is locked and/or it is the index root.
		 *
		 * Also, we need to undo what we have done so far as the
		 * metadata is currently in an inconsistent state and things
		 * will get really confused when moving the entries from the
		 * index root to the index allocation block and the same
		 * attribute we are extending at the moment is extended.
		 * Another reason is that the mft record will be dropped by the
		 * move thus we would expose invalid metadata to concurrent
		 * threads which is a Bad Thing(TM).
		 *
		 * For the same reasons we also need to drop the runlist lock
		 * we are holding.
		 */
		if (ictx->is_locked)
			panic("%s(): ictx->is_locked\n", __FUNCTION__);
		if (ictx->is_root)
			panic("%s(): ictx->is_root\n", __FUNCTION__);
		ll = alloc_size >> vol->cluster_size_shift;
		err = ntfs_cluster_free(ni, ll, -1, actx, NULL);
		if (err) {
			ntfs_error(vol->mp, "Failed to release allocated "
					"cluster(s) (error %d).  Run chkdsk "
					"to recover the lost cluster(s).", err);
			NVolSetErrors(vol);
		}
		m = actx->m;
		a = actx->a;
		/*
		 * If the runlist truncation fails and/or the search context is
		 * no longer valid, we cannot resize the attribute record or
		 * build the mapping pairs array thus we mark the volume dirty
		 * and tell the user to run chkdsk.
		 */
		err = ntfs_rl_truncate_nolock(vol, &ni->rl, ll);
		if (err || actx->is_error) {
			if (actx->is_error)
				err = actx->error;
			ntfs_error(vol->mp, "Failed to %s (error %d).  Run "
					"chkdsk.", actx->is_error ? "restore "
					"attribute search context" :
					"truncate attribute runlist", err);
			NVolSetErrors(vol);
			goto err_out;
		}
		lck_rw_unlock_exclusive(&ni->rl.lock);
		/* Find the index root by walking up the tree path. */
		root_ictx = ictx;
		while (!root_ictx->is_root) {
			root_ictx = root_ictx->up;
			/*
			 * If we go all the way round to the beginning without
			 * finding the root something has gone badly wrong.
			 */
			if (root_ictx == ictx)
				panic("%s(): root_ictx == ictx\n",
						__FUNCTION__);
		}
		/*
		 * We need a proper deallocatable attribute search context thus
		 * switch the one pointing to the attribute to be resized to
		 * point to the index root.  FIXME: We are not updating
		 * @actx->al_entry as this is not going to be touched at all.
		 * Having said that set it to NULL just in case.
		 */
		actx->a = root_actx.a;
		actx->al_entry = NULL;
		/*
		 * Lock the index root node.  We already have the index root
		 * attribute thus only need to do the revalidation part of
		 * re-locking.
		 */
		root_ictx->is_locked = 1;
		root_ictx->actx = actx;
		root_ictx->bytes_free = le32_to_cpu(m->bytes_allocated) -
				le32_to_cpu(m->bytes_in_use);
		root_ictx->ir = ir;
		delta = (u8*)ih - (u8*)root_ictx->index;
		if (delta) {
			INDEX_ENTRY **entries;
			unsigned u;

			root_ictx->index = ih;
			root_ictx->entry = (INDEX_ENTRY*)(
					(u8*)root_ictx->entry + delta);
			entries = root_ictx->entries;
			for (u = 0; u < root_ictx->nr_entries; u++)
				entries[u] = (INDEX_ENTRY*)((u8*)
						entries[u] + delta);
		}
		/*
		 * Move the index root entries to an index allocation block.
		 *
		 * Note we do not need to worry about this causing infinite
		 * recursion in the case that we were called from
		 * ntfs_index_block_alloc() which was called from
		 * ntfs_index_move_root_to_allocation_block() because the
		 * latter will have emptied the index root before calling
		 * ntfs_index_block_alloc() thus we will bail out above when
		 * checking whether the index root is empty the second time
		 * round and the recursion will stop there.  This is a very
		 * seldom occurence thus there is no point in special casing it
		 * in the code in a more efficient but more complicated way.
		 *
		 * A complication is that ntfs_attr_resize() may have been
		 * called from ntfs_index_block_alloc() and in this case when
		 * we call ntfs_index_move_root_to_allocation_block() it will
		 * call ntfs_index_block_alloc() again which will cause a
		 * deadlock (or with lock debugging enabled panic()) because
		 * ntfs_index_block_alloc() takes the bitmap inode lock for
		 * writing.  To avoid this ntfs_index_block_alloc() sets
		 * @ictx->bmp_is_locked and we need to set
		 * @root_ictx->bmp_is_locoked to the same value so that when
		 * ntfs_index_move_root_to_allocation_block() calls
		 * ntfs_index_block_alloc() the latter will know not to take
		 * the bitmap inode lock again.
		 */
		root_ictx->bmp_is_locked = ictx->bmp_is_locked;
		err = ntfs_index_move_root_to_allocation_block(root_ictx);
		if (root_ictx != ictx)
			root_ictx->bmp_is_locked = 0;
		if (err) {
			ntfs_error(vol->mp, "Failed to move index root to "
					"index allocation block (error %d).",
					err);
			if (root_ictx->is_locked)
				ntfs_index_ctx_unlock(root_ictx);
			/*
			 * This is a disaster as it means the index context is
			 * no longer valid thus we have to bail out all the way.
			 */
			return err;
		}
		/* Unlock the newly created index block. */
		if (root_ictx->is_root)
			panic("%s(): root_ictx->is_root\n", __FUNCTION__);
		if (!root_ictx->is_locked)
			panic("%s(): !root_ictx->is_locked\n", __FUNCTION__);
		ntfs_index_ctx_unlock(root_ictx);
		/*
		 * We are done.  The index root is now empty thus the mft
		 * record should now have enough space.  Because we undid
		 * everything and dropped the runlist lock as well as the mft
		 * record when moving the index root entries into the index
		 * allocation block we need to restart the attribute allocation
		 * extension again.
		 *
		 * But first we set @ictx to NULL so we do not get here again
		 * in the case that there still is not enough free space.  This
		 * is not a disaster as we can just carry on doing other
		 * rearrangements to free up enough space in the mft record.
		 */
		ictx = NULL;
		goto retry_extend;
	}
ictx_done:
	/*
	 * There is not enough space in the mft record.
	 *
	 * We need to add an attribute list attribute if it is not already
	 * present.
	 */
	if (!NInoAttrList(base_ni)) {
		err = ntfs_attr_list_add(base_ni, base_m, actx);
		if (err || actx->is_error) {
			if (!err)
				err = actx->error;
			ntfs_error(vol->mp, "Failed to %s mft_no 0x%llx (error "
					"%d).", actx->is_error ?
					"remap extent mft record of" :
					"add attribute list attribute to",
					(unsigned long long)base_ni->mft_no,
					err);
			goto undo;
		}
		/*
		 * The attribute location will have changed so update it from
		 * the search context.
		 */
		m = actx->m;
		a = actx->a;
		/*
		 * Retry the original attribute record resize as we may now
		 * have enough space to create the complete remaining mapping
		 * pairs array in the moved attribute record.
		 *
		 * This can for example happen when the attribute was moved out
		 * to an extent mft record which has much more free space than
		 * the base mft record had.
		 */
		goto retry_attr_rec_resize;
	}
	/*
	 * If the attribute record is in an extent mft record we know the
	 * attribute can be outside the base mft record (as it already is) thus
	 * we can simply resize the attribute to the maximum size possible and
	 * then proceed to fill it with mapping pairs data until it is full,
	 * then start a new extent in a new mft record, etc, until all runlist
	 * elements have been saved in mapping pairs arrays.
	 */
	if (m != base_m) {
		ATTR_LIST_ENTRY *al_entry;
		unsigned new_al_size;

		/*
		 * If the attribute record is not the only one in the extent
		 * mft record then move it to a new extent mft record as that
		 * will allow the attribute record to grow larger thus reducing
		 * the total number of extent attribute records needed to a
		 * minimum.
		 */
		if (!ntfs_attr_record_is_only_one(m, a)) {
move_attr:
			lck_rw_lock_shared(&base_ni->attr_list_rl.lock);
			err = ntfs_attr_record_move(actx);
			lck_rw_unlock_shared(&base_ni->attr_list_rl.lock);
			if (err) {
				if (start < 0 || start >= alloc_size)
					ntfs_error(vol->mp, "Failed to move "
							"attribute extent "
							"from mft record "
							"0x%llx to an extent "
							"mft record (error "
							"%d).",
							(unsigned long long)
							actx->ni->mft_no, err);
				goto undo;
			}
			/*
			 * The attribute location will have changed so update
			 * it from the search context.
			 */
			m = actx->m;
			a = actx->a;
			/*
			 * Retry the original attribute record resize as we may
			 * now have enough space to create the complete
			 * remaining mapping pairs array in the moved attribute
			 * record.
			 */
			goto retry_attr_rec_resize;
		}
		max_size = (le32_to_cpu(m->bytes_allocated) -
				le32_to_cpu(m->bytes_in_use)) & ~7;
add_mapping_pairs_to_attr:
		max_size += attr_len - mp_ofs;
		err = ntfs_attr_record_resize(m, a, max_size + mp_ofs);
		/*
		 * We worked out the exact size we can extend to so the resize
		 * cannot fail.
		 */
		if (err)
			panic("%s(): err (ntfs_attr_record_resize())\n",
					__FUNCTION__);
		/*
		 * If the new size and the old size are the same we cannot add
		 * anything to this extent so do not bother rebuilding the
		 * mapping pairs array and go straight to creating the next
		 * extent.
		 */
		if (attr_len == le32_to_cpu(a->length)) {
start_new_attr:
			stop_vcn = sle64_to_cpu(a->highest_vcn) + 1;
			goto skip_mpa_build;
		}
build_mpa:
		mp_rebuilt = TRUE;
		/* Generate the mapping pairs directly into the attribute. */
		err = ntfs_mapping_pairs_build(vol, (s8*)a + mp_ofs,
				le32_to_cpu(a->length) - mp_ofs, rl,
				lowest_vcn, -1, &stop_vcn);
		if (err && err != ENOSPC) {
			if (start < 0 || start >= alloc_size)
				ntfs_error(vol->mp, "Cannot extend allocation "
						"of mft_no 0x%llx, attribute "
						"type 0x%x, because building "
						"the mapping pairs array "
						"failed (error %d).",
						(unsigned long long)ni->mft_no,
						(unsigned)le32_to_cpu(ni->type),
						err);
			err = EIO;
			/*
			 * Need to set @a->highest_vcn to enable correct error
			 * recovery.
			 */
			if (!is_first)
				a->highest_vcn = cpu_to_sle64(sle64_to_cpu(
						a->lowest_vcn) - 1);
			goto undo;
		}
		/* Update the highest_vcn. */
		a->highest_vcn = cpu_to_sle64(stop_vcn - 1);
		/*
		 * We have finished with this extent so update the current
		 * allocated size and attribute length to reflect this.  We
		 * need to do this to enable error handling and recovery.
		 */
		alloc_size = stop_vcn << vol->cluster_size_shift;
		attr_len = le32_to_cpu(a->length);
		/*
		 * If the mapping pairs build succeeded, i.e. the current
		 * attribute extent contains the end of the runlist, we are
		 * done and only need to update the attribute sizes in the base
		 * attribute extent so go and do that.
		 */
		if (!err)
			goto update_sizes;
		/*
		 * We have finished with this extent mft record thus we release
		 * it after ensuring the changes make it to disk later.  We do
		 * this by hand as we want to keep the current attribute list
		 * attribute entry as we will be inserting the entry for the
		 * next attribute extent immediately after it.
		 */
		NInoSetMrecNeedsDirtying(actx->ni);
skip_mpa_build:
		/* Get the size of the remaining mapping pairs array. */
		rl = ntfs_rl_find_vcn_nolock(rl, stop_vcn);
		if (!rl)
			panic("%s(): !rl (skip_mpa_build)\n", __FUNCTION__);
		if (!rl->length)
			panic("%s(): !rl->length (skip_mpa_build)\n",
					__FUNCTION__);
		if (rl->lcn < LCN_HOLE)
			panic("%s(): rl->lcn < LCN_HOLE (skip_mpa_build)\n",
					__FUNCTION__);
		err = ntfs_get_size_for_mapping_pairs(vol, rl, stop_vcn, -1,
				&mp_size);
		if (err) {
			if (start < 0 || start >= alloc_size)
				ntfs_error(vol->mp, "Cannot complete "
						"extension of allocation of "
						"mft_no 0x%llx, attribute type "
						"0x%x, because determining "
						"the size for the mapping "
						"pairs failed (error %d).",
						(unsigned long long)ni->mft_no,
						(unsigned)le32_to_cpu(ni->type),
						err);
			err = EIO;
			goto undo;
		}
		/* We only release extent mft records. */
		if (actx->ni != base_ni)
			ntfs_extent_mft_record_unmap(actx->ni);
		/*
		 * We now need to allocate a new extent mft record, attach it
		 * to the base ntfs inode and set up the search context to
		 * point to it, then create a new attribute extent in it of
		 * either maximum size or the left to do mapping pairs size and
		 * then build the mapping pairs array in it.  Finally, add an
		 * attribute list attribute entry for the new attribute extent.
		 */
		err = ntfs_mft_record_alloc(vol, NULL, NULL, base_ni,
				&actx->ni, &m, &a);
		if (err) {
			/*
			 * Make it safe to release the attribute search
			 * context.
			 */
			actx->ni = base_ni;
			if (start < 0 || start >= alloc_size)
				ntfs_error(vol->mp, "Cannot extend allocation "
						"of mft_no 0x%llx, attribute "
						"type 0x%x, because "
						"allocating a new extent mft "
						"record failed (error %d),",
						(unsigned long long)ni->mft_no,
						(unsigned)le32_to_cpu(ni->type),
						err);
			goto undo;
		}
		actx->m = m;
		actx->a = a;
		/* We are no longer working on the extent we started with. */
		is_first = FALSE;
		/*
		 * Get the size needed for the remaining mapping pairs array
		 * and make space for an attribute large enough to hold it.  If
		 * there is not enough space to do so make the maximum amount
		 * of space available.
		 */
		lowest_vcn = stop_vcn;
		/*
		 * Calculate the offset into the new attribute at which the
		 * mapping pairs array begins.  The mapping pairs array is
		 * placed after the name aligned to an 8-byte boundary which in
		 * turn is placed immediately after the non-resident attribute
		 * record itself.
		 */
		mp_ofs = offsetof(ATTR_RECORD, compressed_size) + ((name_size +
				7) & ~7);
		err = ntfs_attr_record_make_space(m, a, mp_ofs + mp_size);
		if (err) {
			if (err != ENOSPC)
				panic("%s(): err != ENOSPC\n", __FUNCTION__);
			max_size = (le32_to_cpu(m->bytes_allocated) -
					le32_to_cpu(m->bytes_in_use)) & ~7;
			if (max_size < mp_ofs)
				panic("%s(): max_size < mp_ofs\n",
						__FUNCTION__);
			err = ntfs_attr_record_make_space(m, a, max_size);
			/*
			 * We worked out the exact maximum size so the call
			 * cannot fail.
			 */
			if (err)
				panic("%s(): err ("
						"ntfs_attr_record_make_space()"
						")\n", __FUNCTION__);
		}
		/*
		 * Now setup the new attribute record.  The entire attribute
		 * has been zeroed and the length of the attribute record has
		 * been set.
		 *
		 * Before we proceed with setting up the attribute, add an
		 * attribute list attribute entry for the created attribute
		 * extent.
		 */
		al_entry = actx->al_entry = (ATTR_LIST_ENTRY*)(
				(u8*)actx->al_entry +
				le16_to_cpu(actx->al_entry->length));
		al_entry_len = (offsetof(ATTR_LIST_ENTRY, name) + name_size +
				7) & ~7;
		new_al_size = base_ni->attr_list_size + al_entry_len;
		/* Out of bounds checks. */
		if ((u8*)al_entry < base_ni->attr_list || (u8*)al_entry >
				base_ni->attr_list + new_al_size ||
				(u8*)al_entry + al_entry_len >
				base_ni->attr_list + new_al_size) {
			/* Inode is corrupt. */
			if (start < 0 || start >= alloc_size)
				ntfs_error(vol->mp, "Cannot complete "
						"extension of allocation of "
						"mft_no 0x%llx, attribute type "
						"0x%x, because the inode is "
						"corrupt.  Run chkdsk.",
						(unsigned long long)ni->mft_no,
						(unsigned)
						le32_to_cpu(ni->type));
			err = EIO;
			goto free_undo;
		}
		err = ntfs_attr_size_bounds_check(vol, AT_ATTRIBUTE_LIST,
				new_al_size);
		if (err) {
			if (err == ERANGE) {
				if (start < 0 || start >= alloc_size)
					ntfs_error(vol->mp, "Cannot complete "
							"extension of "
							"allocation of mft_no "
							"0x%llx, attribute "
							"type 0x%x, because "
							"the attribute list "
							"attribute would "
							"become to large.  "
							"You need to "
							"defragment your "
							"volume and then try "
							"again.",
							(unsigned long long)
							ni->mft_no, (unsigned)
							le32_to_cpu(ni->type));
				err = ENOSPC;
			} else {
				if (start < 0 || start >= alloc_size)
					ntfs_error(vol->mp, "Cannot complete "
							"extension of "
							"allocation of mft_no "
							"0x%llx, attribute "
							"type 0x%x, because "
							"the attribute list "
							"attribute is unknown "
							"on the volume.  The "
							"volume is corrupt.  "
							"Run chkdsk.",
							(unsigned long long)
							ni->mft_no, (unsigned)
							le32_to_cpu(ni->type));
				NVolSetErrors(vol);
				err = EIO;
			}
			goto free_undo;
		}
		/*
		 * Reallocate the memory buffer if needed and create space for
		 * the new entry.
		 */
		new_al_alloc = (new_al_size + NTFS_ALLOC_BLOCK - 1) &
				~(NTFS_ALLOC_BLOCK - 1);
		if (new_al_alloc > base_ni->attr_list_alloc) {
			u8 *tmp, *al, *al_end;
			unsigned al_entry_ofs;

			tmp = OSMalloc(new_al_alloc, ntfs_malloc_tag);
			if (!tmp) {
				if (start < 0 || start >= alloc_size)
					ntfs_error(vol->mp, "Cannot complete "
							"extension of "
							"allocation of mft_no "
							"0x%llx, attribute "
							"type 0x%x, because "
							"there is not enough "
							"memory to extend "
							"the attribute list "
							"attribute.",
							(unsigned long long)
							ni->mft_no, (unsigned)
							le32_to_cpu(ni->type));
				err = ENOMEM;
				goto free_undo;
			}
			al = base_ni->attr_list;
			al_entry_ofs = (u8*)al_entry - al;
			al_end = al + base_ni->attr_list_size;
			memcpy(tmp, al, al_entry_ofs);
			if ((u8*)al_entry < al_end)
				memcpy(tmp + al_entry_ofs + al_entry_len,
						al + al_entry_ofs,
						base_ni->attr_list_size -
						al_entry_ofs);
			al_entry = actx->al_entry = (ATTR_LIST_ENTRY*)(tmp +
					al_entry_ofs);
			OSFree(base_ni->attr_list, base_ni->attr_list_alloc,
					ntfs_malloc_tag);
			base_ni->attr_list_alloc = new_al_alloc;
			base_ni->attr_list = tmp;
		} else if ((u8*)al_entry < base_ni->attr_list +
				base_ni->attr_list_size)
			memmove((u8*)al_entry + al_entry_len, al_entry,
					base_ni->attr_list_size -
					((u8*)al_entry - base_ni->attr_list));
		base_ni->attr_list_size = new_al_size;
		/* Set up the attribute extent and the attribute list entry. */
		al_entry->type = a->type = ni->type;
		al_entry->length = cpu_to_le16(al_entry_len);
		a->non_resident = 1;
		al_entry->name_length = a->name_length = ni->name_len;
		a->name_offset = const_cpu_to_le16(offsetof(ATTR_RECORD,
				compressed_size));
		al_entry->name_offset = offsetof(ATTR_LIST_ENTRY, name);
		al_entry->instance = a->instance = m->next_attr_instance;
		/*
		 * Increment the next attribute instance number in the mft
		 * record as we consumed the old one.
		 */
		m->next_attr_instance = cpu_to_le16((le16_to_cpu(
				m->next_attr_instance) + 1) & 0xffff);
		al_entry->lowest_vcn = a->lowest_vcn =
				cpu_to_sle64(lowest_vcn);
		al_entry->mft_reference = MK_LE_MREF(actx->ni->mft_no,
				actx->ni->seq_no);
		a->mapping_pairs_offset = cpu_to_le16(mp_ofs);
		/* Copy the attribute name into place. */
		if (name_size) {
			memcpy((u8*)a + offsetof(ATTR_RECORD, compressed_size),
					ni->name, name_size);
			memcpy(&al_entry->name, ni->name, name_size);
		}
		/* For tidyness, zero out the unused space. */
		if (al_entry_len > offsetof(ATTR_LIST_ENTRY, name) + name_size)
			memset((u8*)al_entry +
					offsetof(ATTR_LIST_ENTRY, name) +
					name_size, 0, al_entry_len -
					(offsetof(ATTR_LIST_ENTRY, name) +
					name_size));
		/*
		 * Need to set @a->highest_vcn to enable correct error
		 * recovery.
		 */
		a->highest_vcn = cpu_to_sle64(lowest_vcn - 1);
		/*
		 * Extend the attribute list attribute and copy in the modified
		 * value from the cache.
		 */
		err = ntfs_attr_list_sync_extend(base_ni, base_m,
				(u8*)al_entry - base_ni->attr_list, actx);
		if (err || actx->is_error) {
			/*
			 * If @actx->is_error indicates error this is fatal as
			 * we cannot build the mapping pairs array into it as
			 * it is not mapped.
			 *
			 * However, we may still be able to recover from this
			 * situation by freeing the extent mft record and thus
			 * deleting the attribute record.  This only works when
			 * this is the only attribute record in the mft record
			 * and when we just created this extent attribute
			 * record.  We can easily determine if this is the only
			 * attribute in the mft record by scanning through the
			 * cached attribute list attribute.
			 */
			if (!err)
				err = actx->error;
			ntfs_error(vol->mp, "Failed to %s mft_no 0x%llx (error "
					"%d).", actx->is_error ?
					"remap extent mft record of" :
					"extend and sync attribute list "
					"attribute to",
					(unsigned long long)base_ni->mft_no,
					err);
			goto undo;
		}
		/*
		 * Finally, proceed to building the mapping pairs array into
		 * the attribute record.
		 */
		goto build_mpa;
	}
	/*
	 * We now know that the attribute is in the base mft record.
	 *
	 * For performance reasons we want to keep the first extent of the
	 * unnamed $DATA attribute of files and the $I30 named
	 * $INDEX_ALLOCATION and $BITMAP attributes of directories in the base
	 * mft record even if this means that the first extent will be nearly
	 * empty.  This ensures that loading an inode is faster and thus stat()
	 * and getattrlist() will be faster.
	 *
	 * If the attribute is one of the above described ones then we keep the
	 * existing extent as it is (unless it is actually empty in which case
	 * we add at least some mapping data to it) and start a new extent in a
	 * new extent mft record.
	 *
	 * In all other cases we move the attribute to a new extent mft record
	 * and retry the attribute resize as it may now fit.
	 */
	if (a->lowest_vcn || (!S_ISDIR(base_ni->mode) &&
			(ni->type != AT_DATA || ni->name_len)) ||
			(S_ISDIR(base_ni->mode) &&
			(!ni->name_len || ni->name != I30)))
		goto move_attr;
	max_size = (le32_to_cpu(m->bytes_allocated) -
			le32_to_cpu(m->bytes_in_use)) & ~7;
	al_entry_len = le16_to_cpu(actx->al_entry->length); 
	/*
	 * A single mapping pair can be up to 17 bytes in size so we need at
	 * least that much free space.  But we need to align the attribute
	 * length to 8 bytes thus the 17 becomes 24.
	 *
	 * Further, we will be adding at least one attribute list attribute
	 * entry thus we want to definitely have space for that to happen.  If
	 * the attribute list attribute is non-resident we may have to add
	 * another mapping pair which would as above be 24 bytes or if it is
	 * resident we would have to add an actual attribute list entry which
	 * would be the same size as the one for the current attribute record.
	 * As this is guaranteed to be larger than 24 bytes we use the larger
	 * size as the minimum to leave free.
	 *
	 * Thus the minimum of free space we require before adding any mapping
	 * pairs to the current attribute record is 24 + @al_entry_len.
	 *
	 * There may be a lot of free space so it would be silly to only use
	 * the minimum.  On one hand we would like to consume as much of the
	 * free space as possible to keep the number of attribute extents to a
	 * minimum.  On the other hand we would like to keep enough spare space
	 * for four attribute list attribute entries (this is an arbitrary
	 * choice) to simplify future expansion of the attribute list
	 * attribute.
	 */
	if (!*((u8*)a + mp_ofs)) {
		/*
		 * There are no mapping pairs in this attribute record thus we
		 * either have to add some mapping pairs or if the available
		 * space is less than our minimum we have to move the attribute
		 * record out into a new extent mft record.
		 */
		if (max_size < 24 + al_entry_len)
			goto move_attr;
		/*
		 * We have our minimum amount of space and possibly a lot more.
		 * If we have less than our desired spare space use our minimum
		 * and if we have more than that use everything except the
		 * desired spare space.
		 */
		if (max_size < 24 + (4 * al_entry_len))
			max_size = 24;
		else
			max_size -= 4 * al_entry_len;
	} else {
		/*
		 * Check if it would be sensible to add at least some mapping
		 * pairs to the current attribute record.
		 *
		 * If the amount of free space is less than the desired spare
		 * space we leave this attribute record be and start a new
		 * extent and if we have more than that use everything except
		 * the desired spare space.
		 */
		if (max_size < 24 + (4 * al_entry_len))
			goto start_new_attr;
		max_size -= 4 * al_entry_len;
	}
	/*
	 * We want to add some mapping pairs to the current attribute before
	 * starting the next one.
	 *
	 * @max_size is already set to the number of bytes to consume from the
	 * free space in the mft record and it is guaranteed that the mft
	 * record has at least that much free space.
	 */
	goto add_mapping_pairs_to_attr;
update_sizes:
	/*
	 * We now have extended the allocated size of the attribute.  Reflect
	 * this in the ntfs_inode structure and the attribute record.
	 */
	if (a->lowest_vcn) {
		/*
		 * We are not in the first attribute extent, switch to it, but
		 * first ensure the changes will make it to disk later.
		 */
		NInoSetMrecNeedsDirtying(actx->ni);
		ntfs_attr_search_ctx_reinit(actx);
		err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len, 0,
				NULL, 0, actx);
		if (err) {
			if (start < 0 || start >= alloc_size)
				ntfs_error(vol->mp, "Cannot complete "
						"extension of allocation of "
						"mft_no 0x%llx, attribute type "
						"0x%x, because lookup of "
						"first attribute extent "
						"failed (error %d).",
						(unsigned long long)
						base_ni->mft_no, (unsigned)
						le32_to_cpu(ni->type), err);
			if (err == ENOENT)
				err = EIO;
			goto undo_do_trunc;
		}
		/* @m is not used any more so no need to set it. */
		a = actx->a;
	}
	/*
	 * If we created a hole and the attribute is not marked as sparse, mark
	 * it as sparse now.
	 */
	if (is_sparse && !NInoSparse(ni)) {
		err = ntfs_attr_sparse_set(base_ni, ni, actx);
		if (err) {
			ntfs_error(vol->mp, "Failed to set the attribute to "
					"be sparse (error %d).", err);
			goto undo_do_trunc;
		}
		/*
		 * The attribute may have been moved to make space for the
		 * compressed size so @a is now invalid.
		 */
		a = actx->a;
	}
	lck_spin_lock(&ni->size_lock);
	ni->allocated_size = new_alloc_size;
	a->allocated_size = cpu_to_sle64(new_alloc_size);
	if (NInoSparse(ni) || (ni->type != AT_INDEX_ALLOCATION &&
			NInoCompressed(ni))) {
		ni->compressed_size += nr_allocated << vol->cluster_size_shift;
		a->compressed_size = cpu_to_sle64(ni->compressed_size);
	}
	lck_spin_unlock(&ni->size_lock);
	if (ni->name == I30 && ni->type == AT_INDEX_ALLOCATION) {
		lck_spin_lock(&base_ni->size_lock);
		base_ni->allocated_size = new_alloc_size;
		lck_spin_unlock(&base_ni->size_lock);
	}
alloc_done:
	if (new_data_size > sle64_to_cpu(a->data_size)) {
		if (!ubc_setsize(ni->vn, new_data_size)) {
			ntfs_error(vol->mp, "Failed to set size in UBC.");
			/*
			 * This can only happen if a previous resize failed and
			 * the UBC size was already out of date in which case
			 * we can just leave it out of date and continue to
			 * completion returning an error.  FIXME: We could roll
			 * back the changes to the metadata at some point but
			 * it does not seem worth it at the moment given that
			 * the error can only happen if there already was an
			 * error thus it is very unlikely.
			 */ 
			err = EIO;
		}
		lck_spin_lock(&ni->size_lock);
		ni->data_size = new_data_size;
		a->data_size = cpu_to_sle64(new_data_size);
		lck_spin_unlock(&ni->size_lock);
		if (ni->name == I30 && ni->type == AT_INDEX_ALLOCATION) {
			lck_spin_lock(&base_ni->size_lock);
			base_ni->data_size = new_data_size;
			lck_spin_unlock(&base_ni->size_lock);
		}
	}
dirty_done:
	/* Ensure the changes make it to disk. */
	NInoSetMrecNeedsDirtying(actx->ni);
	/*
	 * We have modified the size.  If the ntfs inode is the base inode,
	 * cause the sizes to be written to all the directory index entries
	 * pointing to the base inode when the inode is written to disk.  Do
	 * not do this for directories as they have both sizes set to zero in
	 * their index entries.
	 */
	if (ni == base_ni && !S_ISDIR(ni->mode))
		NInoSetDirtySizes(ni);
done:
	ntfs_attr_search_ctx_put(actx);
	ntfs_mft_record_unmap(base_ni);
	lck_rw_unlock_exclusive(&ni->rl.lock);
	ntfs_debug("Done, new_allocated_size 0x%llx.",
			(unsigned long long)new_alloc_size);
	if (dst_alloc_size)
		*dst_alloc_size = new_alloc_size;
	return err;
free_undo:
	/* We have not yet added an attribute list entry for the new extent. */
	al_entry_added = FALSE;
	goto free_extent;
undo:
	ntfs_attr_search_ctx_reinit(actx);
	if (is_first && !mp_rebuilt)
		goto undo_alloc;
	/* Look up the attribute extent we were working on. */
	if (ntfs_attr_lookup(ni->type, ni->name, ni->name_len, lowest_vcn,
			NULL, 0, actx)) {
		/* There is nothing we can do now, bail out. */
		ntfs_error(vol->mp, "Failed to find current attribute extent "
				"in error code path.  Leaving inconsistent "
				"metadata.  Run chkdsk.");
		NVolSetErrors(vol);
		goto err_out;
	}
	if (is_first)
		actx->a->highest_vcn = cpu_to_sle64(
				(alloc_size >> vol->cluster_size_shift) - 1);
undo_alloc:
	ll = alloc_size >> vol->cluster_size_shift;
	if (ntfs_cluster_free(ni, ll, -1, actx, &nr_freed)) {
		ntfs_error(vol->mp, "Failed to release allocated cluster(s) "
				"in error code path.  Run chkdsk to recover "
				"the lost cluster(s).");
		NVolSetErrors(vol);
		/*
		 * Still need to know how many real clusters are effectively
		 * truncated from the attribute extentsion.
		 */
		nr_freed = ntfs_rl_get_nr_real_clusters(&ni->rl, ll, -1);
	}
	m = actx->m;
	a = actx->a;
undo_hole:
	/*
	 * If the runlist truncation fails and/or the search context is no
	 * longer valid, we cannot resize the attribute record or build the
	 * mapping pairs array thus we mark the volume dirty and tell the user
	 * to run chkdsk.
	 */
	if (ntfs_rl_truncate_nolock(vol, &ni->rl, ll) || actx->is_error) {
		ntfs_error(vol->mp, "Failed to %s in error code path.  Run "
				"chkdsk.", actx->is_error ?
				"restore attribute search context" :
				"truncate attribute runlist");
		NVolSetErrors(vol);
	} else if (is_first) {
		if (mp_rebuilt) {
			/* We are working on the original extent, restore it. */
			if (ntfs_attr_record_resize(m, a, attr_len)) {
				ntfs_error(vol->mp, "Failed to restore "
						"attribute record in error "
						"code path.  Run chkdsk.");
				NVolSetErrors(vol);
			} else /* if (success) */ {
				mp_ofs = le16_to_cpu(a->mapping_pairs_offset);
				if (ntfs_mapping_pairs_build(vol, (s8*)a +
						mp_ofs, attr_len - mp_ofs,
						ni->rl.rl, lowest_vcn, -1,
						NULL)) {
					ntfs_error(vol->mp, "Failed to "
							"restore mapping "
							"pairs array in error "
							"code path.  Run "
							"chkdsk.");
					NVolSetErrors(vol);
				}
				if (actx->ni != base_ni)
					NInoSetMrecNeedsDirtying(actx->ni);
			}
		}
	} else if (/* !is_first && */ a->highest_vcn ==
			cpu_to_sle64(sle64_to_cpu(a->lowest_vcn) - 1)) {
		/* We need to delete the attribute list entry, too. */
		al_entry_added = TRUE;
		/* We are working on a new extent, remove it. */
		if (!ntfs_attr_record_is_only_one(m, a)) {
			ntfs_attr_record_delete_internal(m, a);
			if (actx->ni != base_ni)
				NInoSetMrecNeedsDirtying(actx->ni);
		} else {
free_extent:
			if (!ntfs_extent_mft_record_free(base_ni, actx->ni,
					m)) {
				/*
				 * The extent inode no longer exists.  Make it
				 * safe to release/reinit the search context.
				 */
				actx->ni = base_ni;
			} else {
				ntfs_error(vol->mp, "Failed to free extent "
						"mft record 0x%llx of mft_no "
						"0x%llx in error code path.  "
						"Leaving inconsistent "
						"metadata.  Run chkdsk.",
						(unsigned long long)
						actx->ni->mft_no,
						(unsigned long long)
						base_ni->mft_no);
				NVolSetErrors(vol);
			}
		}
		if (al_entry_added) {
			ntfs_attr_list_entry_delete(base_ni, actx->al_entry);
			ntfs_attr_search_ctx_reinit(actx);
			if (ntfs_attr_list_sync_shrink(base_ni, 0, actx)) {
				ntfs_error(vol->mp, "Failed to restore "
						"attribute list attribute in "
						"base inode 0x%llx.  Leaving "
						"inconsistent metadata.  "
						"Run chkdsk.",
						(unsigned long long)
						base_ni->mft_no);
				NVolSetErrors(vol);
			}
		}
	}
undo_do_trunc:
	lck_spin_lock(&ni->size_lock);
	if (alloc_size == ni->allocated_size) {
		lck_spin_unlock(&ni->size_lock);
		goto undo_skip_update_sizes;
	}
	lck_spin_unlock(&ni->size_lock);
	ntfs_attr_search_ctx_reinit(actx);
	/* Look up the first attribute extent. */
	if (ntfs_attr_lookup(ni->type, ni->name, ni->name_len, 0, NULL, 0,
			actx)) {
		/* There is nothing we can do now, bail out. */
		ntfs_error(vol->mp, "Failed to find first attribute extent in "
				"error code path.  Leaving inconsistent "
				"metadata.  Run chkdsk.");
		NVolSetErrors(vol);
		goto err_out;
	}
	a = actx->a;
	lck_spin_lock(&ni->size_lock);
	ni->allocated_size = alloc_size;
	a->allocated_size = cpu_to_sle64(alloc_size);
	if (NInoSparse(ni) || (ni->type != AT_INDEX_ALLOCATION &&
			NInoCompressed(ni))) {
		ni->compressed_size += (nr_allocated - nr_freed) <<
				vol->cluster_size_shift;
		a->compressed_size = cpu_to_sle64(ni->compressed_size);
	}
	lck_spin_unlock(&ni->size_lock);
	if (ni->name == I30 && ni->type == AT_INDEX_ALLOCATION) {
		lck_spin_lock(&base_ni->size_lock);
		base_ni->allocated_size = alloc_size;
		lck_spin_unlock(&base_ni->size_lock);
	}
	/* Ensure the changes make it to disk. */
	if (actx->ni != base_ni)
		NInoSetMrecNeedsDirtying(actx->ni);
	/*
	 * We have modified the size.  If the ntfs inode is the base inode,
	 * cause the sizes to be written to all the directory index entries
	 * pointing to the base inode when the inode is written to disk.  Do
	 * not do this for directories as they have both sizes set to zero in
	 * their index entries.
	 */
	if (ni == base_ni && !S_ISDIR(ni->mode))
		NInoSetDirtySizes(ni);
undo_skip_update_sizes:
	ntfs_attr_search_ctx_put(actx);
	NInoSetMrecNeedsDirtying(base_ni);
	ntfs_mft_record_unmap(base_ni);
	lck_rw_unlock_exclusive(&ni->rl.lock);
	/*
	 * Things are now consistent, try to truncate the attribute back to its
	 * old size which will cause the allocation to be restored to its old
	 * size.
	 *
	 * TODO: We should support partial allocations and when we do so we
	 * should only put the allocated size back if the error was not ENOSPC
	 * and partial allocations are acceptable for this attribute.  In that
	 * case would also need to update @ni->data_size, @a->data_size, and
	 * the size in the vnode @ni->vn via ubc_setsize().
	 */
	if (!is_first) {
		lck_spin_lock(&ni->size_lock);
		ll = ni->data_size;
		lck_spin_unlock(&ni->size_lock);
		if (ntfs_attr_resize(ni, ll, 0, ictx)) {
			ntfs_error(vol->mp, "Failed to undo partial "
					"allocation in inode 0x%llx in error "
					"code path.",
					(unsigned long long)base_ni->mft_no);
			NVolSetErrors(vol);
		}
	}
conv_err_out:
	ntfs_debug("Failed (error %d).", err);
	return err;
err_out:
	if (actx)
		ntfs_attr_search_ctx_put(actx);
	if (base_m)
		ntfs_mft_record_unmap(base_ni);
	lck_rw_unlock_exclusive(&ni->rl.lock);
	goto conv_err_out;
trunc_err_out:
	mp_rebuilt = FALSE;
	if (is_sparse) {
		ll = alloc_size >> vol->cluster_size_shift;
		/*
		 * Silence compiler warning about possible use of uninitalized
		 * variable.
		 */
		attr_len = 0;
		goto undo_hole;
	}
	goto err_out;
}

/**
 * ntfs_attr_resize - called to change the size of an ntfs attribute inode
 * @ni:		ntfs inode for which to change the size
 * @new_size:	new size in bytes to which to resize the ntfs attribute @ni
 * @ioflags:	flags further describing the resize request
 * @ictx:	index context or NULL
 *
 * Resize the attribute described by the ntfs inode @ni to @new_size bytes.
 *
 * Note: We only support size changes for normal attributes at present, i.e.
 * not compressed and not encrypted.
 *
 * The flags in @ioflags further describe the resize request.  The following
 * ioflags are currently defined in OS X kernel (a lot of them are not
 * applicable to resize requests however):
 *	IO_UNIT		- Do i/o as atomic unit.
 *	IO_APPEND	- Append write to end.
 *	IO_SYNC		- Do i/o synchronously.
 *	IO_NODELOCKED	- Underlying node already locked.
 *	IO_NDELAY	- FNDELAY flag set in file table.
 *	IO_NOZEROFILL	- F_SETSIZE fcntl uses this to prevent zero filling.
 *	IO_TAILZEROFILL	- Zero fills at the tail of write.
 *	IO_HEADZEROFILL	- Zero fills at the head of write.
 *	IO_NOZEROVALID	- Do not zero fill if valid page.
 *	IO_NOZERODIRTY	- Do not zero fill if page is dirty.
 *	IO_CLOSE	- The i/o was issued from close path.
 *	IO_NOCACHE	- Same effect as VNOCACHE_DATA, but only for this i/o.
 *	IO_RAOFF	- Same effect as VRAOFF, but only for this i/o.
 *	IO_DEFWRITE	- Defer write if vfs.defwrite is set.
 *	IO_PASSIVE	- This is background i/o so do not throttle other i/o.
 * In particular the only flags that are used in the kernel when calling
 * vnode_setsize() are IO_SYNC and IO_NOZEROFILL.
 *
 * TODO: The @ioflags are currently ignored.
 *
 * If @ictx is not NULL, the resize is for an index allocation or bitmap
 * attribute extension.  In this case, if there is not enough space in the mft
 * record for the extended index allocation/bitmap attribute, the index root is
 * moved to an index block if it is not empty to create more space in the mft
 * record.
 *
 * Return 0 on success and errno on error.
 *
 * Locking: - Caller must hold @ni->lock on the inode for writing.
 *	    - If called for a shrinking operation, the tail of the new final
 *	      partial page will be zeroed by the call to ubc_setsize() thus it
 *	      must not be locked / mapped or the ubc_setsize() call would
 *	      deadlock.
 */
errno_t ntfs_attr_resize(ntfs_inode *ni, s64 new_size, int ioflags,
		ntfs_index_context *ictx)
{
	s64 old_size, nr_freed, new_alloc_size, old_alloc_size, compressed_size;
	VCN highest_vcn, old_highest_vcn, lowest_vcn;
	ntfs_inode *eni, *base_ni;
	ntfs_volume *vol = ni->vol;
	ntfs_attr_search_ctx *actx;
	MFT_RECORD *m;
	ATTR_RECORD *a;
	ATTR_LIST_ENTRY *al_entry;
	u8 *del_al_start, *al_end;
	int size_change, alloc_change;
	unsigned mp_size, attr_len, arec_size;
	errno_t err;
	BOOL need_ubc_setsize = TRUE;
	static const char es[] = "  Leaving inconsistent metadata.  Unmount "
			"and run chkdsk.";

	ntfs_debug("Entering for mft_no 0x%llx.",
			(unsigned long long)ni->mft_no);
	/*
	 * Cannot be called for directory inodes as metadata access happens via
	 * the corresponding index inodes.
	 */
	if (S_ISDIR(ni->mode))
		panic("%s(): Called for directory inode.\n", __FUNCTION__);
	base_ni = ni;
	if (NInoAttr(ni))
		base_ni = ni->base_ni;
	/*
	 * We are going to change the size thus we need the ntfs inode lock
	 * taken for exclusive access which is already done by the caller.
	 *
	 * When shrinking start by changing the size in the UBC of the vnode.
	 * This will cause all pages in the VM beyond the new size to be thrown
	 * away and the last page to be pushed out to disk and its end
	 * invalidated.
	 *
	 * We guarantee that the size in the UBC in the vnode will always be
	 * smaller or equal to the data_size in the ntfs inode thus no need to
	 * check the data_size.
	 */
	old_size = ubc_getsize(ni->vn);
	if (new_size < old_size) {
		err = ubc_setsize(ni->vn, new_size);
		if (!err) {
			ntfs_error(vol->mp, "Failed to shrink size in UBC.");
			err = EIO;
			goto err;
		}
		need_ubc_setsize = FALSE;
	}
retry_resize:
	/*
	 * Lock the runlist for writing and map the mft record to ensure it is
	 * safe to modify the attribute runlist and sizes.
	 */
	lck_rw_lock_exclusive(&ni->rl.lock);
	err = ntfs_mft_record_map(base_ni, &m);
	if (err) {
		ntfs_error(vol->mp, "Failed to map mft record for mft_no "
				"0x%llx (error %d).",
				(unsigned long long)ni->mft_no, err);
		goto unl_err;
	}
	actx = ntfs_attr_search_ctx_get(base_ni, m);
	if (!actx) {
		ntfs_error(vol->mp, "Failed to allocate a search context (not "
				"enough memory).");
		err = ENOMEM;
		goto unm_err;
	}
	err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len, 0, NULL, 0,
			actx);
	if (err) {
		if (err == ENOENT) {
			ntfs_error(vol->mp, "Open attribute is missing from "
					"mft record.  Inode 0x%llx is "
					"corrupt.  Run chkdsk.",
					(unsigned long long)ni->mft_no);
			err = EIO;
		} else
			ntfs_error(vol->mp, "Failed to lookup attribute "
					"(error %d).", err);
		goto put_err;
	}
	m = actx->m;
	a = actx->a;
	if (old_size != ntfs_attr_size(a)) {
		/*
		 * A failed truncate caused the ubc size to get out of sync.
		 * The current size of the attribute value is the correct old
		 * size.
		 */
		old_size = ntfs_attr_size(a);
	}
	/* Calculate the new allocated size. */
	if (NInoNonResident(ni))
		new_alloc_size = (new_size + vol->cluster_size - 1) &
				~(s64)vol->cluster_size_mask;
	else
		new_alloc_size = (new_size + 7) & ~7;
	/* The current allocated size is the old allocated size. */
	lck_spin_lock(&ni->size_lock);
	old_alloc_size = ni->allocated_size;
	compressed_size = ni->compressed_size;
	lck_spin_unlock(&ni->size_lock);
	/*
	 * The change in the file size.  This will be 0 if no change, >0 if the
	 * size is growing, and <0 if the size is shrinking.
	 */
	size_change = -1;
	if (new_size - old_size >= 0) {
		size_change = 1;
		if (new_size == old_size)
			size_change = 0;
	}
	if (need_ubc_setsize && size_change < 0) {
		/*
		 * A previous truncate failed thus we did not catch that this
		 * is a shrinking resize earlier on.
		 */
		err = ubc_setsize(ni->vn, new_size);
		if (!err) {
			ntfs_error(vol->mp, "Failed to shrink size in UBC.");
			err = EIO;
			goto put_err;
		}
		need_ubc_setsize = FALSE;
	}
	/* As above for the allocated size. */
	alloc_change = -1;
	if (new_alloc_size - old_alloc_size >= 0) {
		alloc_change = 1;
		if (new_alloc_size == old_alloc_size)
			alloc_change = 0;
	}
	/*
	 * If neither the size nor the allocation are being changed there is
	 * nothing to do.
	 */
	if (!size_change && !alloc_change)
		goto unm_done;
	/* If the size is changing, check if new size is allowed in $AttrDef. */
	if (size_change) {
		err = ntfs_attr_size_bounds_check(vol, ni->type, new_size);
		if (err) {
			if (err == ERANGE) {
				ntfs_error(vol->mp, "Resize would cause the "
						"mft_no 0x%llx to %simum size "
						"for its attribute type "
						"(0x%x).  Aborting resize.",
						(unsigned long long)ni->mft_no,
						size_change > 0 ? "exceed "
						"the max" : "go under the min",
						(unsigned)
						le32_to_cpu(ni->type));
				err = EFBIG;
			} else {
				ntfs_error(vol->mp, "Mft_no 0x%llx has "
						"unknown attribute type "
						"0x%x.  Aborting resize.",
						(unsigned long long)ni->mft_no,
						(unsigned)
						le32_to_cpu(ni->type));
				err = EIO;
			}
			goto put_err;
		}
	}
	/*
	 * The index root attribute, i.e. directory indexes and index inodes
	 * can be marked compressed or encrypted but this means to create
	 * compressed/encrypted files, not that the attribute is
	 * compressed/encrypted.
	 */
	if (ni->type != AT_INDEX_ALLOCATION &&
			(NInoCompressed(ni) || NInoEncrypted(ni))) {
		ntfs_warning(vol->mp, "Changes in inode size are not "
				"supported yet for %s attributes, ignoring.",
				NInoCompressed(ni) ? "compressed" :
				"encrypted");
		err = ENOTSUP;
		goto put_err;
	}
	if (a->non_resident)
		goto do_non_resident_resize;
	if (NInoNonResident(ni))
		panic("%s(): NInoNonResident(ni)\n", __FUNCTION__);
	arec_size = (le16_to_cpu(a->value_offset) + new_size + 7) & ~7;
	/* Resize the attribute record to best fit the new attribute size. */
	if (new_size < vol->mft_record_size &&
			!ntfs_resident_attr_value_resize(m, a, new_size)) {
		/* The resize succeeded! */
		NInoSetMrecNeedsDirtying(actx->ni);
		lck_spin_lock(&ni->size_lock);
		/* Update the sizes in the ntfs inode and all is done. */
		ni->allocated_size = le32_to_cpu(a->length) -
				le16_to_cpu(a->value_offset);
		ni->data_size = le32_to_cpu(a->value_length);
		/*
		 * Note ntfs_resident_attr_value_resize() has already done any
		 * necessary data clearing in the attribute record.  When the
		 * file is being shrunk ubc_setsize() will already have zeroed
		 * the last partial page, i.e. since this is the resident case
		 * this is the page with index 0.  However, when the file is
		 * being expanded, the page cache page data between the old
		 * data_size, i.e. old_size, and the new_size has not been
		 * zeroed.  Fortunately, we do not need to zero it either since
		 * on one hand it will either already be zero due to pagein
		 * clearing partial page data beyond the data_size in which
		 * case there is nothing to do or in the case of the file being
		 * mmap()ped at the same time, POSIX specifies that the
		 * behaviour is unspecified thus we do not have to do anything.
		 * This means that in our implementation in the rare case that
		 * the file is mmap()ped and a write occured into the mmap()ped
		 * region just beyond the file size and we now extend the file
		 * size to incorporate this dirty region outside the file size,
		 * a pageout of the page would result in this data being
		 * written to disk instead of being cleared.  Given POSIX
		 * specifies that this corner case is undefined, we choose to
		 * leave it like that as this is much simpler for us as we
		 * cannot lock the relevant page now since we are holding too
		 * many ntfs locks which would result in lock reversal
		 * deadlocks.
		 */
		ni->initialized_size = new_size;
		lck_spin_unlock(&ni->size_lock);
		goto unm_done;
	}
	/* If the above resize failed, this must be an attribute extension. */
	if (size_change < 0)
		panic("%s(): size_change < 0\n", __FUNCTION__);
	/*
	 * Not enough space in the mft record.  If this is an index related
	 * extension, check if the index root attribute is in the same mft
	 * record as the attribute being extended and if it is and it is not
	 * empty move its entries into an index allocation block.  Note we do
	 * not check whether that actually creates enough space because how
	 * much space is needed exactly is very hard to determine in advance
	 * (due to potential need for associated attribute list attribute
	 * extensions) and also because even if it does not create enough space
	 * it will still help and save work later on when working for example
	 * on the attribute list attribute.
	 */
	if (ictx) {
		long delta;
		INDEX_ROOT *ir;
		INDEX_HEADER *ih;
		INDEX_ENTRY *ie, *first_ie;
		ntfs_index_context *root_ictx;
		ntfs_attr_search_ctx root_actx;

		/*
		 * This must be an index bitmap extension.  An index allocation
		 * extension is also possible but not here as that cannot be
		 * resident.
		 */
		if (ni->type != AT_BITMAP)
			panic("%s(): ni->type != AT_BITMAP\n", __FUNCTION__);
		ntfs_attr_search_ctx_init(&root_actx, actx->ni, m);
		err = ntfs_attr_find_in_mft_record(AT_INDEX_ROOT, ni->name,
				ni->name_len, NULL, 0, &root_actx);
		if (err) {
			if (err != ENOENT) {
				ntfs_error(vol->mp, "Failed to find index "
						"root attribute in mft_no "
						"0x%llx (error %d).  Inode is "
						"corrupt.  Run chkdsk.",
						(unsigned long long)ni->mft_no,
						err);
				NVolSetErrors(vol);
			}
			/*
			 * The index root is in a different mft record so we
			 * cannot gain anything by moving out its entries.  Set
			 * @ictx to NULL so we do not waste our time trying
			 * again.
			 */
			ictx = NULL;
			goto ictx_done;
		}
		/*
		 * We found the index root in the same mft record as the
		 * attribute to be extended.  Check whether it is empty or not.
		 */
		ir = (INDEX_ROOT*)((u8*)root_actx.a +
				le16_to_cpu(root_actx.a->value_offset));
		ih = &ir->index;
		first_ie = ie = (INDEX_ENTRY*)((u8*)ih +
				le32_to_cpu(ih->entries_offset));
		while (!(ie->flags & INDEX_ENTRY_END))
			ie = (INDEX_ENTRY*)((u8*)ie + le16_to_cpu(ie->length));
		/*
		 * If there are no entries other than the end entry we cannot
		 * gain anything by moving out the entries from the index root.
		 * Set @ictx to NULL so we do not waste our time trying again.
		 */
		if (ie == first_ie) {
			ictx = NULL;
			goto ictx_done;
		}
		/*
		 * We cannot have gotten this far if the current index context
		 * is locked and/or it is the index root.
		 *
		 * Also, we need to drop the runlist lock we are holding as it
		 * may need to be taken when moving the entries from the index
		 * root to the index allocation block.
		 */
		if (ictx->is_locked)
			panic("%s(): ictx->is_locked\n", __FUNCTION__);
		if (ictx->is_root)
			panic("%s(): ictx->is_root\n", __FUNCTION__);
		lck_rw_unlock_exclusive(&ni->rl.lock);
		/* Find the index root by walking up the tree path. */
		root_ictx = ictx;
		while (!root_ictx->is_root) {
			root_ictx = root_ictx->up;
			/*
			 * If we go all the way round to the beginning without
			 * finding the root something has gone badly wrong.
			 */
			if (root_ictx == ictx)
				panic("%s(): root_ictx == ictx\n",
						__FUNCTION__);
		}
		/*
		 * We need a proper deallocatable attribute search context thus
		 * switch the one pointing to the attribute to be resized to
		 * point to the index root.  Note we are not updating
		 * @actx->al_entry as this is not going to be touched at all.
		 * Having said that set it to NULL just in case.
		 */
		actx->a = root_actx.a;
		actx->al_entry = NULL;
		/*
		 * Lock the index root node.  We already have the index root
		 * attribute thus only need to do the revalidation part of
		 * re-locking.
		 */
		root_ictx->is_locked = 1;
		root_ictx->actx = actx;
		root_ictx->bytes_free = le32_to_cpu(m->bytes_allocated) -
				le32_to_cpu(m->bytes_in_use);
		root_ictx->ir = ir;
		delta = (u8*)ih - (u8*)root_ictx->index;
		if (delta) {
			INDEX_ENTRY **entries;
			unsigned u;

			root_ictx->index = ih;
			root_ictx->entry = (INDEX_ENTRY*)(
					(u8*)root_ictx->entry + delta);
			entries = root_ictx->entries;
			for (u = 0; u < root_ictx->nr_entries; u++)
				entries[u] = (INDEX_ENTRY*)((u8*)entries[u] +
						delta);
		}
		/*
		 * Move the index root entries to an index allocation block.
		 *
		 * Note we do not need to worry about this causing infinite
		 * recursion in the case that we were called from
		 * ntfs_index_block_alloc() which was called from
		 * ntfs_index_move_root_to_allocation_block() because the
		 * latter will have emptied the index root before calling
		 * ntfs_index_block_alloc() thus we will bail out above when
		 * checking whether the index root is empty the second time
		 * round and the recursion will stop there.  This is a very
		 * seldom occurence thus there is no point in special casing it
		 * in the code in a more efficient but more complicated way.
		 *
		 * A complication is that ntfs_attr_resize() may have been
		 * called from ntfs_index_block_alloc() and in this case when
		 * we call ntfs_index_move_root_to_allocation_block() it will
		 * call ntfs_index_block_alloc() again which will cause a
		 * deadlock (or with lock debugging enabled panic()) because
		 * ntfs_index_block_alloc() takes the bitmap inode lock for
		 * writing.  To avoid this ntfs_index_block_alloc() sets
		 * @ictx->bmp_is_locked and we need to set
		 * @root_ictx->bmp_is_locoked to the same value so that when
		 * ntfs_index_move_root_to_allocation_block() calls
		 * ntfs_index_block_alloc() the latter will know not to take
		 * the bitmap inode lock again.
		 */
		root_ictx->bmp_is_locked = ictx->bmp_is_locked;
		err = ntfs_index_move_root_to_allocation_block(root_ictx);
		if (root_ictx != ictx)
			root_ictx->bmp_is_locked = 0;
		if (err) {
			ntfs_error(vol->mp, "Failed to move index root to "
					"index allocation block (error %d).",
					err);
			if (root_ictx->is_locked)
				ntfs_index_ctx_unlock(root_ictx);
			/*
			 * This is a disaster as it means the index context is
			 * no longer valid thus we have to bail out all the
			 * way.
			 */
			goto err;
		}
		/* Unlock the newly created index block. */
		if (root_ictx->is_root)
			panic("%s(): root_ictx->is_root\n", __FUNCTION__);
		if (!root_ictx->is_locked)
			panic("%s(): !root_ictx->is_locked\n", __FUNCTION__);
		ntfs_index_ctx_unlock(root_ictx);
		/*
		 * We are done.  The index root is now empty thus the mft
		 * record should now have enough space.  Because we dropped the
		 * mft record when moving the index root entries into the index
		 * allocation block we need to restart the attribute resize
		 * again.
		 *
		 * But first we set @ictx to NULL so we do not get here again
		 * in the case that there still is not enough free space.  This
		 * is not a disaster as we can just carry on doing other
		 * rearrangements to free up enough space in the mft record.
		 */
		ictx = NULL;
		goto retry_resize;
	}
ictx_done:
	/*
	 * We have to drop all the locks so we can call
	 * ntfs_attr_make_non_resident().
	 */
	ntfs_attr_search_ctx_put(actx);
	ntfs_mft_record_unmap(base_ni);
	lck_rw_unlock_exclusive(&ni->rl.lock);
	/*
	 * Not enough space in the mft record, try to make the attribute
	 * non-resident and if successful restart the truncation process.
	 */
	err = ntfs_attr_make_non_resident(ni);
	if (!err)
		goto retry_resize;
	/*
	 * Could not make non-resident.  If this is due to this not being
	 * permitted for this attribute type try to make other attributes
	 * non-resident and/or move this or other attributes out of the mft
	 * record this attribute is in.  Otherwise fail.
	 */
	if (err != EPERM) {
		if (err != ENOSPC) {
			ntfs_error(vol->mp, "Cannot truncate mft_no 0x%llx, "
					"attribute type 0x%x, because the "
					"conversion from resident to "
					"non-resident attribute failed (error "
					"%d).", (unsigned long long)ni->mft_no,
					(unsigned)le32_to_cpu(ni->type), err);
			if (err != ENOMEM) {
				NVolSetErrors(vol);
				err = EIO;
			}
		}
		goto err;
	}
	/*
	 * To make space in the mft record we would like to try to make other
	 * attributes non-resident if that would save space.
	 *
	 * FIXME: We cannot do this at present unless the attribute is the
	 * attribute being resized as there could be an ntfs inode matching
	 * this attribute in memory and it would become out of date with its
	 * metadata if we touch its attribute record.
	 *
	 * FIXME: We do not need to do this if this is the attribute being
	 * resized as we already tried to make the attribute non-resident and
	 * it did not work or we would never have gotten here in the first
	 * place.
	 *
	 * Thus we have to either move other attributes to extent mft records
	 * thus making more space in the base mft record or we have to move the
	 * attribute being resized to an extent mft record thus giving it more
	 * space.  In any case we need to have an attribute list attribute so
	 * start by adding it if it does not yet exist.
	 *
	 * Before we start, we can check whether it is possible to fit the
	 * attribute to be resized inside an mft record.  If not then there is
	 * no point in proceeding.
	 *
	 * This should never really happen as the attribute size should never
	 * be allowed to grow so much and such requests should never be made by
	 * the driver and if they are they should be caught by the call to
	 * ntfs_attr_size_bounds_check().
	 */
	if (arec_size > vol->mft_record_size - sizeof(MFT_RECORD)) {
		ntfs_error(vol->mp, "Cannot truncate mft_no 0x%llx, attribute "
				"type 0x%x, because the attribute may not be "
				"non-resident and the requested size exceeds "
				"the maximum possible resident attribute "
				"record size.", (unsigned long long)ni->mft_no,
				(unsigned)le32_to_cpu(ni->type));
		/* Use POSIX conformant truncate(2) error code. */
		err = EFBIG;
		goto err;
	}
	/*
	 * The resident attribute can fit in an mft record.  Now have to decide
	 * whether to make other attributes non-resident/move other attributes
	 * out of the mft record or whether to move the attribute record to be
	 * resized out to a new mft record.
	 *
	 * TODO: We never call ntfs_attr_resize() for attributes that cannot be
	 * non-resident thus we never get here thus we simply panic() here to
	 * remind us that we need to implement this code if we ever start
	 * calling this function for attributes that must remain resident.
	 */
	panic("%s(): Attribute may not be non-resident.\n", __FUNCTION__);
do_non_resident_resize:
	if (!NInoNonResident(ni))
		panic("%s(): !NInoNonResident(ni)\n", __FUNCTION__);
	/*
	 * If the size is shrinking, need to reduce the initialized_size and
	 * the data_size before reducing the allocation.
	 */
	if (size_change < 0) {
		/*
		 * Make the valid size smaller (the UBC size is already
		 * up-to-date).
		 */
		lck_spin_lock(&ni->size_lock);
		if (new_size < ni->initialized_size) {
			ni->initialized_size = new_size;
			a->initialized_size = cpu_to_sle64(new_size);
			lck_spin_unlock(&ni->size_lock);
			if (ni->name == I30 &&
					ni->type == AT_INDEX_ALLOCATION) {
				lck_spin_lock(&base_ni->size_lock);
				base_ni->initialized_size = new_size;
				lck_spin_unlock(&base_ni->size_lock);
			}
		} else
			lck_spin_unlock(&ni->size_lock);
		/*
		 * If the size is shrinking it makes no sense for the
		 * allocation to be growing.
		 */
		if (alloc_change > 0)
			panic("%s(): alloc_change > 0\n", __FUNCTION__);
	} else if (/*size_change >= 0 && */ alloc_change > 0){
		/*
		 * The file size is growing or staying the same but the
		 * allocation can be shrinking, growing or staying the same.
		 *
		 * If the allocating is shrinking or staying the same we fall
		 * down into the same code as the size shrinking base
		 * allocation shrinking.
		 *
		 * Only if the allocation is growing do we need to extend the
		 * allocation and possibly update the data size here.  If we
		 * are updating the data size, since we are not touching the
		 * initialized_size we do not need to worry about the actual
		 * data on disk.  And as far as the VM pages are concerned,
		 * there will be no pages beyond the old data size and any
		 * partial region in the last page between the old and new data
		 * size (or the end of the page if the new data size is outside
		 * the page) does not need to be modified as explained above
		 * for the resident attribute resize case.  To do this, we
		 * simply drop the locks we hold and leave all the work to our
		 * friendly helper ntfs_attr_extend_allocation().
		 *
		 * Note by setting @data_start to -1 (last parameter to
		 * ntfs_attr_extend_allocation()) we guarantee that the
		 * allocation is not partial.
		 */
		ntfs_attr_search_ctx_put(actx);
		ntfs_mft_record_unmap(base_ni);
		lck_rw_unlock_exclusive(&ni->rl.lock);
		err = ntfs_attr_extend_allocation(ni, new_size,
				size_change > 0 ? new_size : -1, -1, ictx,
				NULL, FALSE);
		if (err)
			goto err;
		goto done;
	}
	/* alloc_change <= 0 */
	/* If the actual size is changing need to update it now. */
	if (size_change) {
		lck_spin_lock(&ni->size_lock);
		ni->data_size = new_size;
		a->data_size = cpu_to_sle64(new_size);
		lck_spin_unlock(&ni->size_lock);
		if (ni->name == I30 && ni->type == AT_INDEX_ALLOCATION) {
			lck_spin_lock(&base_ni->size_lock);
			base_ni->data_size = new_size;
			lck_spin_unlock(&base_ni->size_lock);
		}
	}
	/* Ensure the modified mft record is written out. */
	NInoSetMrecNeedsDirtying(actx->ni);
	/* If the allocated size is not changing, we are done. */
	if (!alloc_change)
		goto unm_done;
	/*
	 * Free the clusters.  Note we cannot recover once this is done because
	 * someone else can allocate the clusters at any point after we free
	 * them.  Thus any errors will lead to a more or less corrupt file
	 * system depending on how consistent we can make the volume after an
	 * error occurs.
	 */
	err = ntfs_cluster_free(ni, new_alloc_size >>
			vol->cluster_size_shift, -1, actx, &nr_freed);
	m = actx->m;
	a = actx->a;
	if (err) {
		ntfs_error(vol->mp, "Failed to release cluster(s) (error "
				"%d).  Unmount and run chkdsk to recover the "
				"lost cluster(s).", err);
		NVolSetErrors(vol);
	} else {
		/*
		 * Truncate the runlist.  The call to ntfs_cluster_free() has
		 * already ensured that all needed runlist fragments have been
		 * mapped so we do not need to worry about mapping runlist
		 * fragments here.  Note given we have managed to read all the
		 * runlist fragments already the chances of us failing anywhere
		 * in the below code is very small indeed.  Only running out of
		 * memory or a disk/sector failure between the above
		 * ntfs_cluster_free() call and the below calls can cause us to
		 * fail here.
		 *
		 * FIXME: Note that this is not quite true as if
		 * ntfs_cluster_free() aborts with an error it may not have
		 * gotten round to mapping the runlist fragments.  If this
		 * happens ntfs_rl_truncate_nolock() could end up doing a lot
		 * of weird things so we only call it if the
		 * ntfs_cluster_free() succeeded for now.
		 */
		err = ntfs_rl_truncate_nolock(vol, &ni->rl, new_alloc_size >>
				vol->cluster_size_shift);
	}
	/*
	 * If the runlist truncation failed and/or the search context is no
	 * longer valid, we cannot resize the attribute record or build the
	 * mapping pairs array thus we abort.
	 */
	if (err || actx->is_error) {
		if (actx->is_error)
			err = actx->error;
		ntfs_error(vol->mp, "Failed to %s (error %d).%s",
				actx->is_error ?
				"restore attribute search context" :
				"truncate attribute runlist", err, es);
		err = EIO;
		goto bad_out;
	}
	/*
	 * The runlist is now up to date.  If this attribute is sparse we need
	 * to check if it is still sparse and if not we need to change it to a
	 * non-sparse file.  And if it is still sparse we need to update the
	 * compressed size which we postpone till later so we can do it at the
	 * same time as the update of the allocated size.
	 *
	 * To determine whether the attribute is still sparse we compare the
	 * new compressed size to the new allocated size.  If the two have now
	 * become the same the attribute is no longer sparse.  If the
	 * compressed size is still smaller than the allocated size the
	 * attribute is still sparse.
	 */
	compressed_size -= nr_freed << vol->cluster_size_shift;
	if (NInoSparse(ni) && compressed_size >= new_alloc_size) {
		if (compressed_size > new_alloc_size)
			panic("%s(): compressed_size > new_alloc_size\n",
					__FUNCTION__);
		/* Switch the attribute to not be sparse any more. */
		ntfs_attr_sparse_clear(base_ni, ni, actx);
	}
	/* Update the allocated/compressed size. */
	lck_spin_lock(&ni->size_lock);
	ni->allocated_size = new_alloc_size;
	a->allocated_size = cpu_to_sle64(new_alloc_size);
	if (NInoSparse(ni) || (ni->type != AT_INDEX_ALLOCATION &&
			NInoCompressed(ni))) {
		if (nr_freed) {
			if (compressed_size < 0)
				panic("%s(): compressed_size < 0\n",
						__FUNCTION__);
			ni->compressed_size = compressed_size;
			a->compressed_size = cpu_to_sle64(ni->compressed_size);
		}
	}
	lck_spin_unlock(&ni->size_lock);
	if (ni->name == I30 && ni->type == AT_INDEX_ALLOCATION) {
		lck_spin_lock(&base_ni->size_lock);
		base_ni->allocated_size = new_alloc_size;
		lck_spin_unlock(&base_ni->size_lock);
	}
	/*
	 * We have the base attribute extent in @actx and we have set it up
	 * already with the new allocated size.  If the truncation point is not
	 * in the base extent, need to switch to the extent containing the
	 * truncation point now so we can update its attribute record, too.
	 * But before doing so need to ensure the modified mft record is
	 * written out.
	 */
	highest_vcn = new_alloc_size >> vol->cluster_size_shift;
	old_highest_vcn = sle64_to_cpu(a->highest_vcn) + 1;
	ntfs_debug("highest_vcn 0x%llx, old_highest_vcn 0x%llx.",
			(unsigned long long)highest_vcn,
			(unsigned long long)old_highest_vcn);
	if (highest_vcn >= old_highest_vcn) {
		NInoSetMrecNeedsDirtying(actx->ni);
		err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len,
				highest_vcn, NULL, 0, actx);
		if (err) {
			if (err == ENOENT)
				ntfs_error(vol->mp, "Attribute extent is "
						"missing from mft_no 0x%llx.  "
						"Run chkdsk.",
						(unsigned long long)
						ni->mft_no);
			else
				ntfs_error(vol->mp, "Failed to lookup "
						"attribute extent in mft_no "
						"0x%llx (error %d).%s",
						(unsigned long long)
						ni->mft_no, err, es);
			err = EIO;
			goto bad_out;
		}
		m = actx->m;
		a = actx->a;
		old_highest_vcn = sle64_to_cpu(a->highest_vcn) + 1;
		ntfs_debug("Switched to extent attribute record, "
				"old_highest_vcn is now 0x%llx.",
				(unsigned long long)old_highest_vcn);
	}
	/*
	 * If the truncation point is at the very beginning of this attribute
	 * extent and the extent is not the base extent we need to remove the
	 * entire extent and hence do not need to waste time truncating it.
	 *
	 * If this is the base extent we have to truncate it to zero allocated
	 * size and if the truncation point is in the middle of the extent we
	 * need to truncate it to the truncation point.
	 */
	lowest_vcn = sle64_to_cpu(a->lowest_vcn);
	ntfs_debug("lowest_vcn 0x%llx.", (unsigned long long)lowest_vcn);
	if (!lowest_vcn || highest_vcn != lowest_vcn) {
		/*
		 * Get the size for the shrunk mapping pairs array for the
		 * runlist fragment starting at the lowest_vcn of this extent.
		 */
		err = ntfs_get_size_for_mapping_pairs(vol,
				ni->rl.elements ? ni->rl.rl : NULL, lowest_vcn,
				-1, &mp_size);
		if (err) {
			ntfs_error(vol->mp, "Cannot shrink allocation of "
					"mft_no 0x%llx, attribute type 0x%x, "
					"because determining the size for the "
					"mapping pairs failed (error %d).%s",
					(unsigned long long)ni->mft_no,
					(unsigned)le32_to_cpu(ni->type), err,
					es);
			NInoSetMrecNeedsDirtying(actx->ni);
			err = EIO;
			goto bad_out;
		}
		/*
		 * Generate the mapping pairs array directly into the attribute
		 * record.
		 */
		err = ntfs_mapping_pairs_build(vol, (s8*)a +
				le16_to_cpu(a->mapping_pairs_offset), mp_size,
				ni->rl.elements ? ni->rl.rl : NULL, lowest_vcn,
				-1, NULL);
		if (err) {
			ntfs_error(vol->mp, "Cannot shrink allocation of "
					"mft_no 0x%llx, attribute type 0x%x, "
					"because building the mapping pairs "
					"failed (error %d).%s",
					(unsigned long long)ni->mft_no,
					(unsigned)le32_to_cpu(ni->type), err,
					es);
			NInoSetMrecNeedsDirtying(actx->ni);
			err = EIO;
			goto bad_out;
		}
		/* Update the highest_vcn to the new truncation point. */
		a->highest_vcn = cpu_to_sle64(highest_vcn - 1);
		/*
		 * Shrink the attribute record for the new mapping pairs array.
		 * Note, this cannot fail since we are making the attribute
		 * smaller thus by definition there is enough space to do so.
		 */
		attr_len = le32_to_cpu(a->length);
		err = ntfs_attr_record_resize(m, a, mp_size +
				le16_to_cpu(a->mapping_pairs_offset));
		if (err)
			panic("%s(): err\n", __FUNCTION__);
	}
	/* If there is no attribute list we are done. */
	if (!NInoAttrList(base_ni)) {
		/* Ensure the modified mft record is written out. */
		NInoSetMrecNeedsDirtying(base_ni);
		goto unm_done;
	}
	/*
	 * If the current extent is not the base extent and it has a lowest_vcn
	 * equal to the new highest_vcn, we need to delete the current extent.
	 *
	 * Also need to delete all subsequent attribute extents if any exist.
	 * We know that some exist if the old highest_vcn of the current extent
	 * is lower than the old end of the attribute.
	 *
	 * When deleting the attribute extents, free the extent mft records if
	 * the only attribute record in the mft record is the attribute extent
	 * being deleted.  In this case do not need to actually modify the
	 * attribute record at all, just mark the mft record as not in use and
	 * clear its bit in the mft bitmap.  For each deleted attribute extent
	 * also need to delete the corresponding attribute list attribute
	 * entry but we postpone this until we have dealt with all the extents
	 * first.
	 *
	 * When finished, check the attribute list attribute and if it no
	 * longer references any mft records other than the base mft record
	 * delete the attribute list attribute altogether.
	 */
	al_end = base_ni->attr_list + base_ni->attr_list_size;
	del_al_start = (u8*)actx->al_entry;
	if (lowest_vcn && highest_vcn == lowest_vcn) {
		/*
		 * We need to delete the current extent thus manually
		 * reinitialize the attribute search context without unmapping
		 * the current extent.
		 */
		eni = actx->ni;
		actx->ni = base_ni;
		ntfs_attr_search_ctx_reinit(actx);
		al_entry = (ATTR_LIST_ENTRY*)del_al_start;
		goto delete_attr;
	}
	/* Ensure the modified mft record is written out. */
	NInoSetMrecNeedsDirtying(actx->ni);
	del_al_start += le16_to_cpu(((ATTR_LIST_ENTRY*)del_al_start)->length);
	al_entry = (ATTR_LIST_ENTRY*)del_al_start;
	/*
	 * Reinitialize the attribute search context thus unmapping the current
	 * extent if it is not in the base mft record.
	 */
	ntfs_attr_search_ctx_reinit(actx);
	/*
	 * Check if there are more extents by looking at the highest vcn of the
	 * current extent which is in @old_highest_vcn.  If it is below the old
	 * allocated size it means that @al_entry points to the attribute list
	 * entry describing the next attribute extent.
	 */
	while (old_highest_vcn < (old_alloc_size >> vol->cluster_size_shift)) {
		/* Sanity checks. */
		if ((u8*)al_entry + sizeof(ATTR_LIST_ENTRY) >= al_end ||
				(u8*)al_entry < base_ni->attr_list) {
			ntfs_error(vol->mp, "Attribute list attribute is "
					"corrupt in mft_no 0x%llx.  Run "
					"chkdsk.",
					(unsigned long long)base_ni->mft_no);
			err = EIO;
			goto bad_out;
		}
		/*
		 * Map the mft record containing the next extent if it is not
		 * the base mft record which is already mapped and described by
		 * the attribute search context @actx.
		 */
		if (MREF_LE(al_entry->mft_reference) == base_ni->mft_no) {
			/* We want the base mft record. */
			if (MSEQNO_LE(al_entry->mft_reference) !=
					base_ni->seq_no) {
				ntfs_error(vol->mp, "Found stale mft "
						"reference in attribute list "
						"attribute of mft_no 0x%llx.  "
						"Inode is corrupt.  Run "
						"chkdsk.", (unsigned long long)
						base_ni->mft_no);
				err = EIO;
				goto bad_out;
			}
			eni = base_ni;
			m = actx->m;
		} else {
			/* We want an extent mft record. */
			err = ntfs_extent_mft_record_map(base_ni,
					le64_to_cpu(al_entry->mft_reference),
					&eni, &m);
			if (err) {
				ntfs_error(vol->mp, "Failed to map extent mft "
						"record 0x%llx of mft_no "
						"0x%llx.  Inode is corrupt.  "
						"Run chkdsk.",
						(unsigned long long)MREF_LE(
						al_entry->mft_reference),
						(unsigned long long)
						base_ni->mft_no);
				err = EIO;
				goto bad_out;
			}
		}
		/* Locate the attribute extent in the mft record. */
		a = (ATTR_RECORD*)((u8*)m + le16_to_cpu(m->attrs_offset));
		do {
			/* Sanity checks. */
			if ((u8*)a < (u8*)m || (u8*)a > (u8*)m +
					le32_to_cpu(m->bytes_allocated))
				goto corrupt_err;
			/*
			 * We cannot reach the end of the attributes without
			 * finding the attribute extent we are looking for.
			 */
			if (a->type == AT_END || !a->length)
				goto corrupt_err;
			/*
			 * The attribute instance is unique thus if we find the
			 * correct instance we have found the attribute extent.
			 */
			if (al_entry->instance == a->instance) {
				/*
				 * If the type and/or the name are mismatched
				 * between the attribute list entry and the
				 * attribute record, there is corruption.
				 */
				if (al_entry->type != a->type)
					goto corrupt_err;
				if (!ntfs_are_names_equal((ntfschar*)((u8*)a +
						le16_to_cpu(a->name_offset)),
						a->name_length,
						(ntfschar*)((u8*)al_entry +
						al_entry->name_offset),
						al_entry->name_length,
						NVolCaseSensitive(vol),
						vol->upcase, vol->upcase_len))
					goto corrupt_err;
				/* We found the attribute extent. */
				break;
			}
			/* Proceed to the next attribute in the mft record. */
			a = (ATTR_RECORD*)((u8*)a + le32_to_cpu(a->length));
		} while (1);
		/* Record the highest_vcn of the new extent. */
		old_highest_vcn = sle64_to_cpu(a->highest_vcn) + 1;
delete_attr:
		/*
		 * If this is the only attribute record in the mft record, free
		 * the mft record.  Note if this is the case it is not possible
		 * for the mft record to be the base record as it would at
		 * least have to contain the attribute record for the attribute
		 * list attribute so no need to check for this case.
		 *
		 * If it is not the only attribute record in the mft record,
		 * delete the attribute record from the mft record.
		 */
		if ((u8*)m + le16_to_cpu(m->attrs_offset) == (u8*)a &&
				((ATTR_RECORD*)((u8*)a +
				le32_to_cpu(a->length)))->type == AT_END) {
			err = ntfs_extent_mft_record_free(base_ni, eni, m);
			if (err) {
				ntfs_error(vol->mp, "Failed to free extent "
						"mft_no 0x%llx (error %d).  "
						"Unmount and run chkdsk to "
						"recover the lost inode.",
						(unsigned long long)
						eni->mft_no, err);
				NVolSetErrors(vol);
				if (eni != base_ni) {
					NInoSetMrecNeedsDirtying(eni);
					ntfs_extent_mft_record_unmap(eni);
				}
			}
		} else {
			ntfs_attr_record_delete_internal(m, a);
			/* Unmap the mft record if it is not the base record. */
			if (eni != base_ni) {
				NInoSetMrecNeedsDirtying(eni);
				ntfs_extent_mft_record_unmap(eni);
			}
		}
		/* Go to the next entry in the attribute list attribute. */
		al_entry = (ATTR_LIST_ENTRY*)((u8*)al_entry +
				le16_to_cpu(al_entry->length));
	}
	/*
	 * There are no more extents.  If we deleted any attribute extents we
	 * need to remove their attribute list attribute entries now.
	 */
	if ((u8*)al_entry != del_al_start) {
		unsigned al_ofs;
		BOOL have_extent_records;

		al_ofs = del_al_start - base_ni->attr_list;
		ntfs_attr_list_entries_delete(base_ni,
				(ATTR_LIST_ENTRY*)del_al_start, al_entry);
		/*
		 * Scan all entries in the attribute list attribute.  If there
		 * are no more references to extent mft records, delete the
		 * attribute list attribute.
		 *
		 * Otherwise truncate the attribute list attribute and update
		 * its value from the in memory copy.
		 */
		err = ntfs_attr_list_is_needed(base_ni, NULL,
				&have_extent_records);
		if (err)
			goto put_err;
		if (!have_extent_records) {
			/*
			 * There are no extent mft records left in use thus
			 * delete the attribute list attribute.
			 */
			err = ntfs_attr_list_delete(base_ni, actx);
			if (err)
				goto put_err;
		} else {
			/*
			 * There still are extent mft records left in use thus
			 * update the attribute list attribute size and write
			 * the modified data to disk.
			 */
			err = ntfs_attr_list_sync_shrink(base_ni, al_ofs, actx);
			if (err)
				goto put_err;
		}
	}
unm_done:
	ntfs_attr_search_ctx_put(actx);
	ntfs_mft_record_unmap(base_ni);
	lck_rw_unlock_exclusive(&ni->rl.lock);
	/* Set the UBC size if not set yet. */
	if (need_ubc_setsize && !ubc_setsize(ni->vn, new_size)) {
		ntfs_error(vol->mp, "Failed to set the size in UBC.");
		err = EIO;
		/*
		 * This should never fail and if it does it can only happen as
		 * the result of a previous resize having failed.  Thus we do
		 * not try to roll back the metadata changes and simply bail
		 * out.
		 */
		goto err;
	}
done:
	/*
	 * If we have modified the size of the base inode, cause the sizes to
	 * be written to all the directory index entries pointing to the base
	 * inode when the inode is written to disk.  Do not do this for
	 * directories as they have both sizes set to zero in their index
	 * entries.
	 */
	if (ni == base_ni && !S_ISDIR(ni->mode) &&
			(size_change || alloc_change))
		NInoSetDirtySizes(ni);
	// TODO:/FIXME: We have to clear the S_ISUID and S_ISGID bits in the
	// file mode. - Only to be done on success and (size_change ||
	// alloc_change).
	/*
	 * Update the last_data_change_time (mtime) and last_mft_change_time
	 * (ctime) on the base ntfs inode @base_ni unless this is an attribute
	 * inode update in which case only update the ctime as named stream/
	 * extended attribute semantics expect on OS X.
	 *
	 * FIXME: For open(O_TRUNC) it is correct to always change the
	 * {m,c}time.  But for {,f}truncate() we have to only set {m,c}time if
	 * a change happened, i.e. only if size_change is true.  Problem is we
	 * cannot know from which code path we are being called as both system
	 * calls on OS X call vnode_setattr() which calls VNOP_SETATTR() which
	 * calls ntfs_vnop_setattr() which then calls us...  For now at least
	 * we always update the times thus we follow open(O_TRUNC) semantics
	 * and disobey {,f}truncate() semantics.
	 */
	base_ni->last_mft_change_time = ntfs_utc_current_time();
	if (ni == base_ni)
		base_ni->last_data_change_time = base_ni->last_mft_change_time;
	NInoSetDirtyTimes(base_ni);
	/*
	 * If this is not a directory or it is an encrypted directory, set the
	 * needs archiving bit except for the core system files.
	 */
	if (!S_ISDIR(base_ni->mode) || NInoEncrypted(base_ni)) {
		BOOL need_set_archive_bit = TRUE;
		if (vol->major_ver >= 2) {
			if (ni->mft_no <= FILE_Extend)
				need_set_archive_bit = FALSE;
		} else {
			if (ni->mft_no <= FILE_UpCase)
				need_set_archive_bit = FALSE;
		}
		if (need_set_archive_bit) {
			base_ni->file_attributes |= FILE_ATTR_ARCHIVE;
			NInoSetDirtyFileAttributes(base_ni);
		}
	}
	ntfs_debug("Done.");
	return 0;
corrupt_err:
	ntfs_error(vol->mp, "Mft record 0x%llx of mft_no 0x%llx is corrupt.  "
			"Unmount and run chkdsk.",
			(unsigned long long)eni->mft_no,
			(unsigned long long)base_ni->mft_no);
	if (eni != base_ni)
		ntfs_extent_mft_record_unmap(eni);
	err = EIO;
bad_out:
	if (err != ENOMEM && err != ENOTSUP)
		NVolSetErrors(vol);
put_err:
	ntfs_attr_search_ctx_put(actx);
unm_err:
	ntfs_mft_record_unmap(base_ni);
unl_err:
	lck_rw_unlock_exclusive(&ni->rl.lock);
err:
	/* Reset the UBC size. */
	if (!ubc_setsize(ni->vn, old_size))
		ntfs_error(vol->mp, "Failed to restore UBC size.  Leaving UBC "
				"size out of sync with attribute data size.");
	ntfs_debug("Failed (error %d).", err);
	return err;
}

/**
 * ntfs_attr_set - fill (a part of) an attribute with a byte
 * @ni:		ntfs inode describing the attribute to fill
 * @ofs:	offset inside the attribute at which to start to fill
 * @cnt:	number of bytes to fill
 * @val:	the unsigned 8-bit value with which to fill the attribute
 *
 * Fill @cnt bytes of the attribute described by the ntfs inode @ni starting at
 * byte offset @ofs inside the attribute with the constant byte @val.
 *
 * This function is effectively like memset() applied to an ntfs attribute.
 * Note this function actually only operates on the page cache pages belonging
 * to the ntfs attribute and it marks them dirty after doing the memset().
 * Thus it relies on the vm dirty page write code paths to cause the modified
 * pages to be written to the mft record/disk.
 *
 * Return 0 on success and errno on error.  An error code of ESPIPE means that
 * @ofs + @cnt were outside the end of the attribute and no write was
 * performed.
 *
 * Note: This function does not take care of the initialized size!
 *
 * Locking: - Caller must hold an iocount reference on the vnode of the ntfs
 *	      inode @ni.
 *	    - Caller must hold @ni->lock for reading or writing.
 */
errno_t ntfs_attr_set(ntfs_inode *ni, s64 ofs, const s64 cnt, const u8 val)
{
	s64 end, data_size;
	ntfs_volume *vol = ni->vol;
	upl_t upl;
	upl_page_info_array_t pl;
	u8 *kaddr;
	unsigned start_ofs, end_ofs, size;
	errno_t err;

	ntfs_debug("Entering for ofs 0x%llx, cnt 0x%llx, val 0x%x.",
			(unsigned long long)ofs, (unsigned long long)cnt,
			(unsigned)val);
	if (ofs < 0)
		panic("%s(): ofs < 0\n", __FUNCTION__);
	if (cnt < 0)
		panic("%s(): cnt < 0\n", __FUNCTION__);
	if (!cnt)
		goto done;
	/*
	 * FIXME: Compressed and encrypted attributes are not supported when
	 * writing and we should never have gotten here for them.
	 */
	if (NInoCompressed(ni))
		panic("%s(): Inode is compressed.\n", __FUNCTION__);
	if (NInoEncrypted(ni))
		panic("%s(): Inode is encrypted.\n", __FUNCTION__);
	/* Work out the starting index and page offset. */
	start_ofs = (unsigned)ofs & PAGE_MASK;
	/* Work out the ending index and page offset. */
	end = ofs + cnt;
	end_ofs = (unsigned)end & PAGE_MASK;
	/* If the end is outside the inode size return ESPIPE. */
	lck_spin_lock(&ni->size_lock);
	data_size = ni->data_size;
	lck_spin_unlock(&ni->size_lock);
	if (end > data_size) {
		ntfs_error(vol->mp, "Request exceeds end of attribute.");
		return ESPIPE;
	}
	ofs &= ~PAGE_MASK_64;
	end &= ~PAGE_MASK_64;
	/* If there is a first partial page, need to do it the slow way. */
	if (start_ofs) {
		err = ntfs_page_map(ni, ofs, &upl, &pl, &kaddr, TRUE);
		if (err) {
			ntfs_error(vol->mp, "Failed to read first partial "
					"page (ofs 0x%llx).",
					(unsigned long long)ofs);
			return err;
		}
		/*
		 * If the last page is the same as the first page, need to
		 * limit the write to the end offset.
		 */
		size = PAGE_SIZE;
		if (ofs == end)
			size = end_ofs;
		memset(kaddr + start_ofs, val, size - start_ofs);
		ntfs_page_unmap(ni, upl, pl, TRUE);
		ofs += PAGE_SIZE;
		if (ofs >= (end + end_ofs))
			goto done;
	}
	/*
	 * Do the whole pages the fast way.
	 *
	 * TODO: It may be possible to optimize this loop by creating a
	 * sequence of large page lists by hand, mapping them, then running the
	 * memset, then unmapping them and committing them.  This incurs a
	 * higher cpu time because of the larger mapping required but incurs
	 * many fewer calls into the ubc thus less locks will need to be taken
	 * which may well speed things up a lot.  It will need to be
	 * benchmarked to determine which is actually faster so leaving it the
	 * easier way for now.
	 */
	for (; ofs < end; ofs += PAGE_SIZE) {
		/* Find or create the current page. */
		err = ntfs_page_grab(ni, ofs, &upl, &pl, &kaddr, TRUE);
		if (err) {
			ntfs_error(vol->mp, "Failed to grab page (ofs "
					"0x%llx).", (unsigned long long)ofs);
			return err;
		}
		memset(kaddr, val, PAGE_SIZE);
		ntfs_page_unmap(ni, upl, pl, TRUE);
	}
	/* If there is a last partial page, need to do it the slow way. */
	if (end_ofs) {
		err = ntfs_page_map(ni, ofs, &upl, &pl, &kaddr, TRUE);
		if (err) {
			ntfs_error(vol->mp, "Failed to read last partial page "
					"(ofs 0x%llx).",
					(unsigned long long)ofs);
			return err;
		}
		memset(kaddr, val, end_ofs);
		ntfs_page_unmap(ni, upl, pl, TRUE);
	}
done:
	ntfs_debug("Done.");
	return 0;
}

/**
 * ntfs_resident_attr_read - read from an attribute which is resident
 * @ni:		resident ntfs inode describing the attribute from which to read
 * @ofs:	byte offset in attribute at which to start reading
 * @cnt:	number of bytes to copy into the destination buffer @buf
 * @buf:	destination buffer into which to copy attribute data
 *
 * Map the base mft record of the ntfs inode @ni, find the attribute it
 * describes, and copy @cnt bytes from byte offset @ofs into the destination
 * buffer @buf.  If @buf is bigger than the attribute size, zero the remainder.
 *
 * We do not need to worry about compressed attributes because when they are
 * resident the data is not actually compressed and we do not need to worry
 * about encrypted attributes because encrypted attributes cannot be resident.
 *
 * Return 0 on success and errno on error.  Note that a return value of EAGAIN
 * means that someone converted the attribute to non-resident before we took
 * the necessary locks to read from the resident attribute thus we could not
 * perform the read.  The caller needs to cope with this and perform a
 * non-resident read instead.
 */
errno_t ntfs_resident_attr_read(ntfs_inode *ni, const s64 ofs, const u32 cnt,
		u8 *buf)
{
	s64 max_size;
	ntfs_inode *base_ni;
	MFT_RECORD *m;
	ntfs_attr_search_ctx *ctx;
	ATTR_RECORD *a;
	unsigned attr_len, init_len, bytes;
	errno_t err;

	base_ni = ni;
	if (NInoAttr(ni))
		base_ni = ni->base_ni;
	/* Map, pin, and lock the mft record. */
	err = ntfs_mft_record_map(base_ni, &m);
	if (err)
		goto err;
	/*
	 * If a parallel write made the attribute non-resident, drop the mft
	 * record and return EAGAIN.
	 */
	if (NInoNonResident(ni)) {
		err = EAGAIN;
		goto unm_err;
	}
	ctx = ntfs_attr_search_ctx_get(base_ni, m);
	if (!ctx) {
		err = ENOMEM;
		goto unm_err;
	}
	err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len, 0, NULL, 0,
			ctx);
	if (err) {
		if (err == ENOENT)
			err = EIO;
		goto put_err;
	}
	a = ctx->a;
	lck_spin_lock(&ni->size_lock);
	/* These can happen when we race with a shrinking truncate. */
	attr_len = le32_to_cpu(a->value_length);
	if (attr_len > ni->data_size)
		attr_len = ni->data_size;
	max_size = ubc_getsize(ni->vn);
	if (attr_len > max_size)
		attr_len = max_size;
	init_len = attr_len;
	if (init_len > ni->initialized_size)
		init_len = ni->initialized_size;
	lck_spin_unlock(&ni->size_lock);
	/*
	 * If we are reading from the initialized attribute part, copy the data
	 * over into the destination buffer.
	 */
	bytes = cnt;
	if (init_len > ofs) {
		u32 available = init_len - ofs;
		if (bytes > available)
			bytes = available;
		memcpy(buf, (u8*)a + le16_to_cpu(a->value_offset) + ofs, bytes);
	}
	/* Zero the remainder of the destination buffer if any. */
	if (bytes < cnt)
		bzero(buf + bytes, cnt - bytes);
put_err:
	ntfs_attr_search_ctx_put(ctx);
unm_err:
	ntfs_mft_record_unmap(base_ni);
err:
	return err;
}

/**
 * ntfs_resident_attr_write - write to an attribute which is resident
 * @ni:		resident ntfs inode describing the attribute to which to write
 * @buf:	source buffer from which to copy attribute data
 * @cnt:	number of bytes to copy into the attribute from the buffer
 * @ofs:	byte offset in attribute at which to start writing
 *
 * Map the base mft record of the ntfs inode @ni, find the attribute it
 * describes, and copy @cnt bytes from the buffer @buf into the attribute value
 * at byte offset @ofs.
 *
 * We do not need to worry about compressed attributes because when they are
 * resident the data is not actually compressed and we do not need to worry
 * about encrypted attributes because encrypted attributes cannot be resident.
 *
 * Return 0 on success and errno on error.  Note that a return value of EAGAIN
 * means that someone converted the attribute to non-resident before we took
 * the necessary locks to write to the resident attribute thus we could not
 * perform the write.  The caller needs to cope with this and perform a
 * non-resident write instead.
 */
errno_t ntfs_resident_attr_write(ntfs_inode *ni, u8 *buf, u32 cnt,
		const s64 ofs)
{
	ntfs_inode *base_ni;
	MFT_RECORD *m;
	ntfs_attr_search_ctx *ctx;
	ATTR_RECORD *a;
	errno_t err;
	u32 attr_len;

	base_ni = ni;
	if (NInoAttr(ni))
		base_ni = ni->base_ni;
	/* Map, pin, and lock the mft record. */
	err = ntfs_mft_record_map(base_ni, &m);
	if (err)
		goto err;
	/*
	 * If a parallel write made the attribute non-resident, drop the mft
	 * record and return EAGAIN.
	 */
	if (NInoNonResident(ni)) {
		err = EAGAIN;
		goto unm_err;
	}
	ctx = ntfs_attr_search_ctx_get(base_ni, m);
	if (!ctx) {
		err = ENOMEM;
		goto unm_err;
	}
	err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len, 0, NULL, 0,
			ctx);
	if (err) {
		if (err == ENOENT)
			err = EIO;
		goto put_err;
	}
	a = ctx->a;
	if (a->non_resident)
		panic("%s(): a->non_resident\n", __FUNCTION__);
	lck_spin_lock(&ni->size_lock);
	/* These can happen when we race with a shrinking truncate. */
	attr_len = le32_to_cpu(a->value_length);
	if (ofs > attr_len) {
		ntfs_error(ni->vol->mp, "Cannot write past end of resident "
				"attribute.");
		lck_spin_unlock(&ni->size_lock);
		err = EINVAL;
		goto put_err;
	}
	if (ofs + cnt > attr_len) {
		ntfs_error(ni->vol->mp, "Truncating resident write.");
		cnt = attr_len - ofs;
	}
	if (ofs + cnt > ni->initialized_size)
		ni->initialized_size = ofs + cnt;
	lck_spin_unlock(&ni->size_lock);
	/* Copy the data over from the destination buffer. */
	memcpy((u8*)a + le16_to_cpu(a->value_offset) + ofs, buf, cnt);
	/* Mark the mft record dirty to ensure it gets written out. */
	NInoSetMrecNeedsDirtying(ctx->ni);
put_err:
	ntfs_attr_search_ctx_put(ctx);
unm_err:
	ntfs_mft_record_unmap(base_ni);
err:
	return err;
}
