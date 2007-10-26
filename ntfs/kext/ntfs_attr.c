/*
 * ntfs_attr.c - NTFS kernel attribute operations.
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

#include <sys/errno.h>
#include <sys/ucred.h>
#include <sys/ubc.h>

#include <string.h>

#include <libkern/OSMalloc.h>

#include <kern/debug.h>

#include "ntfs.h"
#include "ntfs_attr.h"
#include "ntfs_debug.h"
#include "ntfs_endian.h"
#include "ntfs_inode.h"
#include "ntfs_layout.h"
#include "ntfs_mft.h"
#include "ntfs_runlist.h"
#include "ntfs_types.h"
#include "ntfs_unistr.h"

/**
 * ntfs_map_runlist_nolock - map (a part of) a runlist of an ntfs inode
 * @ni:		ntfs inode for which to map (part of) a runlist
 * @vcn:	map runlist part containing this vcn
 *
 * Map the part of a runlist containing the @vcn of the ntfs inode @ni.
 *
 * Return 0 on success and errno on error.  There is one special error code
 * which is not an error as such.  This is ENOENT.  It means that @vcn is out
 * of bounds of the runlist.
 *
 * Note the runlist can be NULL after this function returns if @vcn is zero and
 * the attribute has zero allocated size, i.e. there simply is no runlist.
 *
 * Locking: - The runlist described by @ni must be locked for writing on entry
 *	      and is locked on return.  Note the runlist will be modified.
 *	    - The base mft record of @ni must not be mapped on entry and it
 *	      will be left unmapped on return.
 */
errno_t ntfs_map_runlist_nolock(ntfs_inode *ni, VCN vcn)
{
	VCN end_vcn;
	ntfs_inode *base_ni;
	ntfs_attr_search_ctx *ctx;
	MFT_RECORD *m;
	ATTR_RECORD *a;
	errno_t err;

	ntfs_debug("Entering for mft_no 0x%llx, vcn 0x%llx.",
			(unsigned long long)ni->mft_no,
			(unsigned long long)vcn);
	if (!NInoAttr(ni))
		base_ni = ni;
	else
		base_ni = ni->base_ni;
	err = ntfs_mft_record_map(base_ni, &m);
	if (err)
		goto done;
	ctx = ntfs_attr_search_ctx_get(base_ni, m);
	if (!ctx) {
		err = ENOMEM;
		goto err;
	}
	err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len,
			CASE_SENSITIVE, vcn, NULL, 0, ctx);
	if (err) {
		if (err == ENOENT)
			err = EIO;
		goto err;
	}
	a = ctx->a;
	if (!a->non_resident)
		panic("%s(): !a->non_resident!\n", __FUNCTION__);
	/*
	 * Only decompress the mapping pairs if @vcn is inside it.  Otherwise
	 * we get into problems when we try to map an out of bounds vcn because
	 * we then try to map the already mapped runlist fragment and
	 * ntfs_mapping_pairs_decompress() fails.
	 */
	end_vcn = sle64_to_cpu(a->highest_vcn) + 1;
	if (!a->lowest_vcn && end_vcn == 1)
		end_vcn = sle64_to_cpu(a->allocated_size) >>
				ni->vol->cluster_size_shift;
	if (vcn >= end_vcn) {
		err = ENOENT;
		goto err;
	}
	err = ntfs_mapping_pairs_decompress(ni->vol, a, &ni->rl);
err:
	if (ctx)
		ntfs_attr_search_ctx_put(ctx);
	ntfs_mft_record_unmap(base_ni);
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
	if (!ni->rl.rl) {
		lck_spin_lock(&ni->size_lock);
		if (!ni->allocated_size) {
			lck_spin_unlock(&ni->size_lock);
			lcn = LCN_ENOENT;
			goto lcn_enoent;
		}
		lck_spin_unlock(&ni->size_lock);
	}
retry_remap:
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
		err = ntfs_map_runlist_nolock(ni, vcn);
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
	if (need_lock_switch)
		lck_rw_lock_exclusive_to_shared(&ni->rl.lock);
	if (lcn == LCN_ENOENT) {
lcn_enoent:
		ntfs_debug("Done (LCN_ENOENT).");
	} else
		ntfs_error(ni->vol->mp, "Failed (error %lld).",
				(long long)lcn);
	return lcn;
}

/**
 * ntfs_attr_search_ctx_init - initialize an attribute search context
 * @ctx:	attribute search context to initialize
 * @ni:		ntfs inode with which to initialize the search context
 * @m:		mft record with which to initialize the search context
 *
 * Initialize the attribute search context @ctx with @ni and @m.
 */
static inline void ntfs_attr_search_ctx_init(ntfs_attr_search_ctx *ctx,
		ntfs_inode *ni, MFT_RECORD *m)
{
	*ctx = (ntfs_attr_search_ctx) {
		.m = m,
		/* Sanity checks are performed elsewhere. */
		.a = (ATTR_RECORD*)((u8*)m + le16_to_cpu(m->attrs_offset)),
		.is_first = TRUE,
		.ni = ni,
	};
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
 */
void ntfs_attr_search_ctx_reinit(ntfs_attr_search_ctx *ctx)
{
	if (!ctx->base_ni) {
		/* No attribute list. */
		ctx->is_first = TRUE;
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

	ctx = (ntfs_attr_search_ctx*)OSMalloc(sizeof(ntfs_attr_search_ctx),
			ntfs_malloc_tag);
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
 * @ic:		IGNORE_CASE or CASE_SENSITIVE (ignored if @name not present)
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
 * If @ctx->is_first is TRUE, the search begins with @ctx->a itself.  If it is
 * FALSE, the search begins after @ctx->a.
 *
 * If @ic is IGNORE_CASE, the @name comparisson is not case sensitive and
 * @ctx->ni must be set to the ntfs inode to which the mft record @ctx->m
 * belongs.  This is so we can get at the ntfs volume and hence at the upcase
 * table.  If @ic is CASE_SENSITIVE, the comparison is case sensitive.  When
 * @name is present, @name_len is the @name length in Unicode characters.
 *
 * If @name is not present (NULL), we assume that the unnamed attribute is
 * being searched for.
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
 */
static errno_t ntfs_attr_find_in_mft_record(const ATTR_TYPE type,
		const ntfschar *name, const u32 name_len,
		const IGNORE_CASE_BOOL ic, const u8 *val, const u32 val_len,
		ntfs_attr_search_ctx *ctx)
{
	ATTR_RECORD *a;
	ntfs_volume *vol = ctx->ni->vol;
	ntfschar *upcase = vol->upcase;
	u32 upcase_len = vol->upcase_len;

	/*
	 * Iterate over attributes in mft record starting at @ctx->a, or the
	 * attribute following that, depending on the value of @ctx->is_first.
	 */
	if (ctx->is_first) {
		a = ctx->a;
		ctx->is_first = FALSE;
	} else
		a = (ATTR_RECORD*)((u8*)ctx->a + le32_to_cpu(ctx->a->length));
	for (;;	a = (ATTR_RECORD*)((u8*)a + le32_to_cpu(a->length))) {
		if ((u8*)a < (u8*)ctx->m || (u8*)a > (u8*)ctx->m +
				le32_to_cpu(ctx->m->bytes_allocated))
			break;
		ctx->a = a;
		if (le32_to_cpu(a->type) > le32_to_cpu(type) ||
				a->type == AT_END)
			return ENOENT;
		if (!a->length)
			break;
		if (a->type != type)
			continue;
		/*
		 * If @name is present, compare the two names.  If @name is
		 * missing, assume we want an unnamed attribute.
		 */
		if (!name) {
			/* The search failed if the found attribute is named. */
			if (a->name_length)
				return ENOENT;
		} else {
			unsigned len, ofs;

			len = a->name_length;
			ofs = le16_to_cpu(a->name_offset);
			if (ofs + (len * sizeof(ntfschar)) >
					le32_to_cpu(a->length))
				break;
			if (!ntfs_are_names_equal(name, name_len,
					(ntfschar*)((u8*)a + ofs), len, ic,
					upcase, upcase_len)) {
				int rc;

				rc = ntfs_collate_names(name, name_len,
						(ntfschar*)((u8*)a + ofs), len,
						1, IGNORE_CASE, upcase,
						upcase_len);
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
						1, CASE_SENSITIVE, upcase,
						upcase_len);
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
 * @ic:		IGNORE_CASE or CASE_SENSITIVE (ignored if @name not present)
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
 * ntfs_attr_search_ctx_put() to cleanup the search context (unmapping any
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
 * If @ctx->is_first is TRUE, the search begins with @ctx->a itself.  If it is
 * FALSE, the search begins after @ctx->a.
 *
 * If @ic is IGNORE_CASE, the @name comparisson is not case sensitive and
 * @ctx->ni must be set to the ntfs inode to which the mft record @ctx->m
 * belongs.  This is so we can get at the ntfs volume and hence at the upcase
 * table.  If @ic is CASE_SENSITIVE, the comparison is case sensitive.  When
 * @name is present, @name_len is the @name length in Unicode characters.
 *
 * If @name is not present (NULL), we assume that the unnamed attribute is
 * being searched for.
 *
 * Finally, the resident attribute value @val is looked for, if present.  If
 * @val is not present (NULL), @val_len is ignored.
 *
 * Warning: Never use @val when looking for attribute types which can be
 *	    non-resident as this most likely will result in a crash!
 */
static errno_t ntfs_attr_find_in_attribute_list(const ATTR_TYPE type,
		const ntfschar *name, const u32 name_len,
		const IGNORE_CASE_BOOL ic, const VCN lowest_vcn,
		const u8 *val, const u32 val_len, ntfs_attr_search_ctx *ctx)
{
	ntfs_inode *base_ni, *ni;
	ntfs_volume *vol;
	ATTR_LIST_ENTRY *al_entry, *next_al_entry;
	u8 *al_start, *al_end;
	ATTR_RECORD *a;
	ntfschar *al_name;
	u32 al_name_len;
	errno_t err = 0;
	static const char *es = " Unmount and run chkdsk.";

	ni = ctx->ni;
	base_ni = ctx->base_ni;
	ntfs_debug("Entering for mft_no 0x%llx, type 0x%x.",
			(unsigned long long)ni->mft_no, type);
	if (!base_ni) {
		/* First call happens with the base mft record. */
		base_ni = ctx->base_ni = ctx->ni;
		ctx->base_m = ctx->m;
	}
	if (ni == base_ni)
		ctx->base_a = ctx->a;
	if (type == AT_END)
		goto not_found;
	vol = base_ni->vol;
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
		ctx->is_first = FALSE;
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
		if (le32_to_cpu(al_entry->type) > le32_to_cpu(type))
			goto not_found;
		if (type != al_entry->type)
			continue;
		/*
		 * If @name is present, compare the two names.  If @name is
		 * missing, assume we want an unnamed attribute.
		 */
		al_name_len = al_entry->name_length;
		al_name = (ntfschar*)((u8*)al_entry + al_entry->name_offset);
		if (!name) {
			if (al_name_len)
				goto not_found;
		} else if (!ntfs_are_names_equal(al_name, al_name_len, name,
				name_len, ic, vol->upcase, vol->upcase_len)) {
			int rc;

			rc = ntfs_collate_names(name, name_len, al_name,
					al_name_len, 1, IGNORE_CASE,
					vol->upcase, vol->upcase_len);
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
					al_name_len, 1, CASE_SENSITIVE,
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
					al_name, al_name_len, CASE_SENSITIVE,
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
				err = ntfs_extent_mft_record_map(base_ni,
						le64_to_cpu(
						al_entry->mft_reference), &ni,
						&ctx->m);
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
		 * containing the attribute represented by the current al_entry.
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
				al_name, al_name_len, CASE_SENSITIVE,
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
		return ntfs_attr_find_in_mft_record(AT_END, name, name_len, ic,
				val, val_len, ctx);
	}
	/*
	 * The attribute was not found.  Before we return, we want to ensure
	 * @ctx->m and @ctx->a indicate the position at which the attribute
	 * should be inserted in the base mft record.  Since we also want to
	 * preserve @ctx->al_entry we cannot reinitialize the search context
	 * using ntfs_attr_search_ctx_reinit() as this would set @ctx->al_entry
	 * to NULL.  Thus we do the necessary bits manually (see
	 * ntfs_attr_search_ctx_init() above).  Note, we _only_ preserve
	 * @ctx->al_entry as the remaining fields (base_*) are identical to
	 * their non base_ counterparts and we cannot set @ctx->base_a
	 * correctly yet as we do not know what @ctx->a will be set to by the
	 * call to ntfs_attr_find_in_mft_record() below.
	 */
	if (ni != base_ni)
		ntfs_extent_mft_record_unmap(ni);
	ctx->m = ctx->base_m;
	ctx->a = (ATTR_RECORD*)((u8*)ctx->m +
			le16_to_cpu(ctx->m->attrs_offset));
	ctx->is_first = TRUE;
	ctx->ni = base_ni;
	ctx->base_ni = NULL;
	ctx->base_m = NULL;
	ctx->base_a = NULL;
	/*
	 * In case there are multiple matches in the base mft record, need to
	 * keep enumerating until we get an attribute not found response (or
	 * another error), otherwise we would keep returning the same attribute
	 * over and over again and all programs using us for enumeration would
	 * lock up in a tight loop.
	 */
	do {
		err = ntfs_attr_find_in_mft_record(type, name, name_len, ic,
				val, val_len, ctx);
	} while (!err);
	ntfs_debug("Done, not found.");
	return err;
}

/**
 * ntfs_attr_lookup - find an attribute in an ntfs inode
 * @type:	attribute type to find
 * @name:	attribute name to find (optional, i.e. NULL means do not care)
 * @name_len:	attribute name length (only needed if @name present)
 * @ic:		IGNORE_CASE or CASE_SENSITIVE (ignored if @name not present)
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
 * ntfs_attr_search_ctx_put() to cleanup the search context (unmapping any
 * mapped mft records, etc).
 *
 * Return 0 if the search was successful and errno if not.
 *
 * When 0, @ctx->a is the found attribute and it is in mft record @ctx->m.  If
 * an attribute list attribute is present, @ctx->al_entry is the attribute list
 * entry of the found attribute.
 *
 * When ENOENT, @ctx->a is the attribute which collates just after the
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
errno_t ntfs_attr_lookup(const ATTR_TYPE type, const ntfschar *name,
		const u32 name_len, const IGNORE_CASE_BOOL ic,
		const VCN lowest_vcn, const u8 *val, const u32 val_len,
		ntfs_attr_search_ctx *ctx)
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
		return ntfs_attr_find_in_mft_record(type, name, name_len, ic,
				val, val_len, ctx);
	return ntfs_attr_find_in_attribute_list(type, name, name_len, ic,
			lowest_vcn, val, val_len, ctx);
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
	errno_t err;
	u32 attr_len, init_len, bytes;

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
	err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len,
			CASE_SENSITIVE, 0, NULL, 0, ctx);
	if (err)
		goto put_err;
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
	err = ntfs_attr_lookup(ni->type, ni->name, ni->name_len,
			CASE_SENSITIVE, 0, NULL, 0, ctx);
	if (err)
		goto put_err;
	a = ctx->a;
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
	NInoSetMrecPageNeedsDirtying(ctx->ni);
	/*
	 * Record the fact that this attribute has a corresponding dirty mft
	 * record so if someone syncs its vnode its mft record will also be
	 * written out.
	 */
	NInoSetDirty(ni);
put_err:
	ntfs_attr_search_ctx_put(ctx);
unm_err:
	ntfs_mft_record_unmap(base_ni);
err:
	return err;
}
