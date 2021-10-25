/*
 * ntfs_attr_list.c - NTFS kernel attribute list attribute operations.
 *
 * Copyright (c) 2006-2008 Anton Altaparmakov.  All Rights Reserved.
 * Portions Copyright (c) 2006-2008 Apple Inc.  All Rights Reserved.
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

#include <string.h>

#ifdef KERNEL
#include <libkern/OSMalloc.h>

#include <kern/sched_prim.h>
#else
#include "ntfs_xpl.h"
#endif

#include "ntfs.h"
#include "ntfs_attr.h"
#include "ntfs_attr_list.h"
#include "ntfs_debug.h"
#include "ntfs_endian.h"
#include "ntfs_inode.h"
#include "ntfs_layout.h"
#include "ntfs_lcnalloc.h"
#include "ntfs_mft.h"
#include "ntfs_types.h"
#include "ntfs_unistr.h"
#include "ntfs_volume.h"

/**
 * ntfs_attr_list_is_needed - check if attribute list attribute is still needed
 * @ni:				base ntfs inode to check
 * @skip_entry:			attribute list entry to skip when checking
 * @attr_list_is_needed:	pointer in which to return the result
 *
 * Check the attribute list attribute of the base ntfs inode @ni.  If there are
 * no attribute list attribute entries other than @skip_entry which reference
 * extent mft records it means the attribute list attribute is no longer needed
 * thus we set *@attr_list_is_needed to false and return success.
 *
 * If there are attribute list attribute entries other than @skip_entry which
 * reference extent mft records the attribute list attribute is still needed
 * thus we set *@attr_list_is_needed to true and return success.
 *
 * Return 0 on success and errno on error.  On success *@attr_list_is_needed is
 * set to true if the attribute list attribute is still needed and to false if
 * it is no longer needed (and hence can be deleted).  On error
 * *@attr_list_is_needed is not defined.
 */
errno_t ntfs_attr_list_is_needed(ntfs_inode *ni, ATTR_LIST_ENTRY *skip_entry,
		BOOL *attr_list_is_needed)
{
	leMFT_REF base_mref;
	ATTR_LIST_ENTRY *entry;
	u8 *al, *al_end;

	ntfs_debug("Entering for base mft_no 0x%llx.",
			(unsigned long long)ni->mft_no);
	/* Scan all entries in the attribute list attribute. */
	base_mref = MK_LE_MREF(ni->mft_no, ni->seq_no);
	*attr_list_is_needed = FALSE;
	al = ni->attr_list;
	entry = (ATTR_LIST_ENTRY*)al;
	al_end = (u8*)al + ni->attr_list_size;
	for (;; entry = (ATTR_LIST_ENTRY*)((u8*)entry +
			le16_to_cpu(entry->length))) {
		/* Out of bounds check. */
		if ((u8*)entry < al || (u8*)entry > al_end)
			goto err;
		/* Catch the end of the attribute list. */
		if ((u8*)entry == al_end)
			break;
		/* More sanity checks. */
		if (!entry->length || (u8*)entry + sizeof(*entry) > al_end)
			goto err;
		/*
		 * If there is an entry to be skipped and the current entry
		 * matches it, then skip this entry.  We do not need to check
		 * whether @skip_entry is NULL because @entry cannot be NULL so
		 * this condition would never be true in @skip_entry is NULL.
		 */
		if (entry == skip_entry)
			continue;
		/*
		 * If this entry references an extent mft record no need to
		 * search any further, we know there is at least one extent mft
		 * record in use thus the attribute list attribute is still
		 * needed.
		 */
		if (entry->mft_reference != base_mref) {
			*attr_list_is_needed = TRUE;
			break;
		}
	}
	ntfs_debug("Done for mft_no 0x%llx, attribute list attribute is "
			"%s needed.", (unsigned long long)ni->mft_no,
			(*attr_list_is_needed) ? "still" : "no longer");
	return 0;
err:
	ntfs_error(ni->vol->mp, "Attribute list attribute of mft_no 0x%llx is "
			"corrupt.  Unmount and run chkdsk.",
			(unsigned long long)ni->mft_no);
	NVolSetErrors(ni->vol);
	return EIO;
}

/**
 * ntfs_attr_list_delete - delete the attribute list attribute of an ntfs inode
 * @ni:		base ntfs inode whose attribute list attribugte to delete
 * @ctx:	initialized attribute search context
 *
 * Delete the attribute list attribute of the base ntfs inode @ni.
 *
 * @ctx is an initialized, ready to use attribute search context that we use to
 * look up the attribute list attribute in the mapped, base mft record.
 *
 * This function assumes that the caller has ensured that the attribute list
 * attribute is no longer needed by calling ntfs_attr_list_is_needed().
 *
 * Return 0 on success and errno on error.
 */
errno_t ntfs_attr_list_delete(ntfs_inode *ni, ntfs_attr_search_ctx *ctx)
{
	errno_t err;

	ntfs_debug("Entering for base mft_no 0x%llx.",
			(unsigned long long)ni->mft_no);
	/* Find the attribute list attribute in the base mft record. */
	err = ntfs_attr_find_in_mft_record(AT_ATTRIBUTE_LIST, AT_UNNAMED, 0,
			NULL, 0, ctx);
	if (err)
		goto err;
	/*
	 * If the attribute list attribute is non-resident we need to free the
	 * allocated clusters and runlist.
	 */
	if (ctx->a->non_resident) {
		/* Free the allocated clusters. */
		err = ntfs_cluster_free_from_rl(ni->vol, ni->attr_list_rl.rl,
				0, -1, NULL);
		if (err) {
			ntfs_warning(ni->vol->mp, "Failed to free some "
					"allocated clusters belonging to the "
					"attribute list attribute of mft_no "
					"0x%llx (error %d).  Continuing "
					"regardless.  Unmount and run chkdsk "
					"to recover the lost space.",
					(unsigned long long)ni->mft_no, err);
			NVolSetErrors(ni->vol);
		}
		/* Free the runlist of the attribute list attribute. */
		OSFree(ni->attr_list_rl.rl, ni->attr_list_rl.alloc,
				ntfs_malloc_tag);
		ni->attr_list_rl.alloc = 0;
	}
	/* Delete the attribute list attribute record. */
	ntfs_attr_record_delete_internal(ctx->m, ctx->a);
	/* Make sure the modified base mft record is written out. */
	NInoSetMrecNeedsDirtying(ni);
	/* Update the in-memory base inode and free memory. */
	ni->attr_list_size = 0;
	OSFree(ni->attr_list, ni->attr_list_alloc, ntfs_malloc_tag);
	ni->attr_list_alloc = 0;
	NInoClearAttrList(ni);
	ntfs_debug("Done.");
	return 0;
err:
	ntfs_error(ni->vol->mp, "Failed to find attribute list attribute in "
			"base mft_no 0x%llx (error %d).  Unmount and run "
			"chkdsk.", (unsigned long long)ni->mft_no, err);
	NVolSetErrors(ni->vol);
	return EIO;
}

/**
 * ntfs_attr_list_add - add an attribute list attribute to an inode
 * @base_ni:	base ntfs inode to which to add an attribute list attribute
 * @m:		base mft record of the base ntfs inode @base_ni
 * @ctx:	attribute search context describing attribute being resized
 *
 * Add an attribute list attribute to the base ntfs inode @base_ni with mapped
 * base mft record @m.
 *
 * @ctx (if not NULL) is an attribute search context describing the attribute
 * being resized which is causing the attribute list attribute to be added.
 * The pointers in the search context are updated with the new location of the
 * attribute in the mft record.  @ctx is also updated to reflect that this is
 * now a search in an attribute list attribute containing inode.
 *
 * Return 0 on success and the negative error code on error.
 *
 * WARNING: If @ctx is supplied, regardless of whether success or failure is
 *	    returned, you need to check @ctx->is_error and if 1 the @ctx is no
 *	    longer valid, i.e. you need to either call
 *	    ntfs_attr_search_ctx_reinit() or ntfs_attr_search_ctx_put() on it.
 *	    In that case @ctx->error will give you the error code for why the
 *	    mapping of the inode failed.
 */
errno_t ntfs_attr_list_add(ntfs_inode *base_ni, MFT_RECORD *m,
		ntfs_attr_search_ctx *ctx)
{
	leMFT_REF mref;
	ntfs_volume *vol;
	ATTR_RECORD *a;
	u8 *al, *al_end;
	ATTR_LIST_ENTRY *al_entry;
	unsigned al_alloc, al_size, bytes_free, attr_len, mp_size = 0;
	errno_t err, err2;
	BOOL for_mft, have_std_info, have_idx_root, insert_resident;
	ntfs_attr_search_ctx al_ctx;

	vol = base_ni->vol;
	for_mft = FALSE;
	if (base_ni == vol->mft_ni)
		for_mft = TRUE;
	ntfs_debug("Entering for mft_no 0x%llx%s.",
			(unsigned long long)base_ni->mft_no,
			for_mft ? ", for $MFT" : "");
	lck_rw_lock_exclusive(&base_ni->attr_list_rl.lock);
	if (NInoAttrList(base_ni))
		panic("%s(): NInoAttrList(base_ni)\n", __FUNCTION__);
	if (!m)
		panic("%s(): !m\n", __FUNCTION__);
	if (ctx && m != ctx->m)
		panic("%s(): ctx && m != ctx->m\n", __FUNCTION__);
	if (base_ni->attr_list_alloc)
		panic("%s(): base_ni->attr_list_alloc\n", __FUNCTION__);
	if (base_ni->attr_list_rl.alloc)
		panic("%s(): base_ni->attr_list_rl.alloc\n", __FUNCTION__);
	if (base_ni->attr_list_rl.elements)
		panic("%s(): base_ni->attr_list_rl.elements\n", __FUNCTION__);
	have_idx_root = have_std_info = FALSE;
	/*
	 * Iterate over all the attribute records in the base mft record and
	 * build the attribute list attribute up in memory.
	 */
	al_alloc = NTFS_ALLOC_BLOCK;
	al = OSMalloc(NTFS_ALLOC_BLOCK, ntfs_malloc_tag);
	if (!al) {
		ntfs_error(vol->mp, "Not enough memory to allocate buffer for "
				"attribute list attribute.");
		lck_rw_unlock_exclusive(&base_ni->attr_list_rl.lock);
		return ENOMEM;
	}
	al_end = al + NTFS_ALLOC_BLOCK;
	al_entry = (ATTR_LIST_ENTRY*)al;
	ntfs_attr_search_ctx_init(&al_ctx, base_ni, m);
	al_ctx.is_iteration = 1;
	mref = MK_LE_MREF(base_ni->mft_no, base_ni->seq_no);
	/* Maximum number of usable bytes in the mft record. */
	bytes_free = le32_to_cpu(m->bytes_allocated) -
			le16_to_cpu(m->attrs_offset);
	do {
		unsigned al_entry_len, len;

		/* Get the next attribute in the mft record. */
		err = ntfs_attr_find_in_mft_record(AT_UNUSED, NULL, 0, NULL, 0,
				&al_ctx);
		if (err) {
			/*
			 * If we reached the end we now have all the attribute
			 * records in the attribute list attribute buffer.
			 */
			if (err == ENOENT)
				break;
			ntfs_error(vol->mp, "Failed to iterate over "
					"attribute records in base mft record "
					"(error %d).", err);
			goto free_err;
		}
		a = al_ctx.a;
		if (a->type == AT_ATTRIBUTE_LIST) {
			ntfs_error(vol->mp, "Failed to add attribute list "
					"attribute as it already exists.");
			err = EIO;
			goto free_err;
		}
		if (a->non_resident && a->lowest_vcn)
			panic("%s(): a->non_resident && a->lowest_vcn\n",
					__FUNCTION__);
		/*
		 * Just some book keeping for later.  If the current attribute
		 * cannot be moved out of the base mft record subtract its size
		 * from the amount of free space.
		 */
		if (a->type == AT_STANDARD_INFORMATION ||
				a->type == AT_INDEX_ROOT) {
			bytes_free -= le32_to_cpu(a->length);
			if (a->type == AT_STANDARD_INFORMATION)
				have_std_info = TRUE;
			else
				have_idx_root = TRUE;
		}
		/*
		 * If we are adding the attribute list attribute to the $MFT
		 * system file, we cannot move out the $DATA attribute to make
		 * space although we could shrink it so that it contains only
		 * enough clusters in its mapping pairs array as to get us to
		 * the next fragment.  However given everything is currently in
		 * the base mft record this means we can at least move out the
		 * filename attribute and the bitmap attribute which is
		 * guaranteed to give us enough space to add a non-resident
		 * attribute list attribute so we do not bother implementing
		 * such truncation here as it cannot be required.
		 */
		if (for_mft) {
			if (a->type == AT_DATA && !a->name_length)
				bytes_free -= le32_to_cpu(a->length);
		}
		/*
		 * If the current attribute is the attribute to be resized
		 * record its attribute list entry in @ctx->al_entry so we do
		 * not have to look it up later.
		 */
		if (ctx && ctx->a == a)
			ctx->al_entry = al_entry;
		len = offsetof(ATTR_LIST_ENTRY, name) +
				(a->name_length * sizeof(ntfschar));
		/*
		 * Attribute list attribute entries must be aligned to 8-byte
		 * boundaries.
		 */
		al_entry_len = (len + 7) & ~7;
		/*
		 * A single mft record cannot have enough attribute records to
		 * overflow a whole NTFS_ALLOC_BLOCK bytes worth of attribute
		 * list attribute entries.
		 */
		if ((u8*)al_entry + al_entry_len > al_end)
			panic("%s(): (u8*)al_entry + al_entry_len > al_end\n",
					__FUNCTION__);
		/*
		 * Create the attribute list entry for the current attribute
		 * record.
		 */
		*al_entry = (ATTR_LIST_ENTRY) {
			.type = a->type,
			.length = cpu_to_le16(al_entry_len),
			.name_length = a->name_length,
			.name_offset = offsetof(ATTR_LIST_ENTRY, name),
			.lowest_vcn = 0,
			.mft_reference = mref,
			.instance = a->instance,
		};
		/* Copy the name if the attribute is named. */
		if (a->name_length)
			memcpy(&al_entry->name,
					(u8*)a + le16_to_cpu(a->name_offset),
					a->name_length * sizeof(ntfschar));
		/* Zero the padding area at the end if it exists. */
		if (al_entry_len - len > 0)
			memset((u8*)al_entry + len, 0, al_entry_len - len);
		/*
		 * We are done with this attribute list entry, switch to the
		 * next one.
		 */
		al_entry = (ATTR_LIST_ENTRY*)((u8*)al_entry + al_entry_len);
	} while (1);
	al_size = (unsigned)((u8*)al_entry - al);
	/*
	 * We now have built the attribute list attribute value in @al.
	 *
	 * Determine the size needed for the attribute list attribute if it is
	 * resident.
	 */
	attr_len = offsetof(ATTR_RECORD, reservedR) + sizeof(a->reservedR) +
			al_size;
	/*
	 * If the size is greater than the maximum size possible to insert into
	 * the mft record then insert the attribute list attribute as a
	 * non-resident attribute record.
	 */
	if (attr_len > bytes_free) {
		insert_resident = FALSE;
		goto insert;
	}
	/*
	 * If we do not have enough space to insert the attribute list
	 * attribute as a resident attribute record try to make enough space
	 * available by making other attributes non-resident and/or moving
	 * other attributes to extent mft records.
	 */
	insert_resident = TRUE;
	ntfs_attr_search_ctx_reinit(&al_ctx);
	al_ctx.is_iteration = 1;
	for (al_entry = (ATTR_LIST_ENTRY*)al;
			attr_len > le32_to_cpu(m->bytes_allocated) -
			le32_to_cpu(m->bytes_in_use);
			al_entry = (ATTR_LIST_ENTRY*)((u8*)al_entry +
			le16_to_cpu(al_entry->length))) {
		/* Get the next attribute in the mft record. */
		err = ntfs_attr_find_in_mft_record(AT_UNUSED, NULL, 0, NULL, 0,
				&al_ctx);
		if (err) {
			/*
			 * We reached the end and we still do not have enough
			 * space to insert the attribute list attribute as a
			 * resident attribute record thus try to insert it as a
			 * non-resident attribute record.
			 */
			if (err == ENOENT) {
				insert_resident = FALSE;
				break;
			}
			ntfs_error(vol->mp, "Failed to iterate over "
					"attribute records in base mft record "
					"(error %d).", err);
			goto undo;
		}
		a = al_ctx.a;
		/*
		 * We already checked above that the attribute list attribute
		 * does not exist yet.
		 */
		if (a->type == AT_ATTRIBUTE_LIST)
			panic("%s(): a->type == AT_ATTRIBUTE_LIST\n",
					__FUNCTION__);
		/*
		 * Do not touch standard information and index root attributes
		 * at this stage.  They will be moved out to extent mft records
		 * further below if really necessary.
		 *
		 * TODO: Probably want to add unnamed data attributes to this
		 * as well.
		 */
		if (a->type == AT_STANDARD_INFORMATION ||
				a->type == AT_INDEX_ROOT)
			continue;
		/*
		 * If we are working on the $MFT system file, do not touch the
		 * unnamed $DATA attribute.
		 */
		if (for_mft) {
			if (a->type == AT_DATA && !a->name_length)
				continue;
		}
		/*
		 * If the attribute is resident and we can expect the
		 * non-resident form to be smaller than the resident form
		 * switch the attribute to non-resident now.
		 *
		 * FIXME: We cannot do this at present unless the attribute is
		 * the attribute being resized as there could be an ntfs inode
		 * matching this attribute in memory and it would become out of
		 * date with its metadata if we touch its attribute record.
		 *
		 * FIXME: We do not need to do this if this is the attribute
		 * being resized as the caller will have already tried to make
		 * the attribute non-resident and this will not have worked or
		 * we would never have gotten here in the first place.
		 */
		/*
		 * Move the attribute out to an extent mft record and update
		 * its attribute list entry.  If it is the attribute to be
		 * resized, also update the attribute search context to match
		 * the new attribute.  If it is not the attribute to be resized
		 * but it is in front of the attribute to be resized update the
		 * attribute search context to account for the removed
		 * attribute record.
		 */
		err = ntfs_attr_record_move_for_attr_list_attribute(&al_ctx,
				al_entry, ctx, NULL);
		if (err) {
			ntfs_error(vol->mp, "Failed to move attribute type "
					"0x%x out of base mft record 0x%llx "
					"and into an extent mft record (error "
					"%d).", (unsigned)le32_to_cpu(a->type),
					(unsigned long long)base_ni->mft_no,
					err);
			goto undo;
		}
	}
insert:	
	/*
	 * Find the location at which to insert the attribute list attribute
	 * record in the base mft record.
	 */
	ntfs_attr_search_ctx_reinit(&al_ctx);
	err = ntfs_attr_find_in_mft_record(AT_ATTRIBUTE_LIST, AT_UNNAMED, 0,
			NULL, 0, &al_ctx);
	if (!err || err != ENOENT) {
		/*
		 * We already checked above that the attribute list attribute
		 * does not exist yet.
		 */
		if (!err)
			panic("%s(): !err\n", __FUNCTION__);
		ntfs_error(vol->mp, "Failed to find location at which to "
				"insert attribute list attribute (error %d).",
				err);
		goto undo;
	}
	a = al_ctx.a;
	/*
	 * If @insert_resident is true try to insert the attribute list
	 * attribute as a resident attribute record.
	 */
	if (insert_resident) {
		err = ntfs_resident_attr_record_insert_internal(m, a,
				AT_ATTRIBUTE_LIST, NULL, 0, al_size);
		if (!err) {
			memcpy((u8*)a + le16_to_cpu(a->value_offset), al,
					al_size);
			/*
			 * If we already allocated some clusters in a previous
			 * iteration need to free them now.
			 *
			 * Note we warn about errors and schedule the volume
			 * for chkdsk but otherwise we ignore the error as it
			 * does not in any way affect anything we are doing.
			 * The volume just ends up having some clusters marked
			 * in use but nothing is referencing them.  Running
			 * chkdsk will clear those bits in the bitmap thus
			 * causing the clusters to be made available for use
			 * again.
			 */
			if (base_ni->attr_list_rl.elements) {
				err = ntfs_cluster_free_from_rl(vol,
						base_ni->attr_list_rl.rl,
						0, -1, NULL);
				if (err) {
					ntfs_warning(vol->mp, "Failed to "
							"release no longer "
							"needed allocated "
							"cluster(s) (error "
							"%d).  Run chkdsk to "
							"recover the lost "
							"cluster(s).", err);
					NVolSetErrors(vol);
				}
				OSFree(base_ni->attr_list_rl.rl,
						base_ni->attr_list_rl.alloc,
						ntfs_malloc_tag);
				base_ni->attr_list_rl.elements = 0;
				base_ni->attr_list_rl.alloc = 0;
			}
			goto done;
		}
		if (err != ENOSPC)
			panic("%s(): err != ENOSPC\n", __FUNCTION__);
	}
	/*
	 * Attempt to insert the attribute list attribute as a non-resident
	 * atribute record.
	 *
	 * First allocate enough clusters to store the attribute list attribute
	 * data.
	 */
	if (!base_ni->attr_list_rl.elements) {
		attr_len = (al_size + vol->cluster_size_mask) &
				~vol->cluster_size_mask;
		err = ntfs_cluster_alloc(vol, 0, attr_len / vol->cluster_size,
				-1, DATA_ZONE, TRUE, &base_ni->attr_list_rl);
		if (err) {
			ntfs_error(vol->mp, "Failed to allocate clusters for "
					"the non-resident attribute list "
					"attribute (error %d).", err);
			goto undo;
		}
		/* Determine the size of the mapping pairs array. */
		err = ntfs_get_size_for_mapping_pairs(vol,
				base_ni->attr_list_rl.rl, 0, -1, &mp_size);
		if (err)
			panic("%s(): err (ntfs_get_size_for_mapping_pairs())\n",
					__FUNCTION__);
	}
	/*
	 * As the attribute is unnamed the mapping pairs array is placed
	 * immediately after the non-resident attribute record itself.
	 *
	 * To determine the size of the attribute record take the offset to the
	 * mapping pairs array and add the size of the mapping pairs array in
	 * bytes aligned to an 8-byte boundary.  Note we do not need to do the
	 * alignment as ntfs_attr_record_make_space() does it anyway.
	 */
	err = ntfs_attr_record_make_space(m, a, offsetof(ATTR_RECORD,
		       compressed_size) + mp_size);
	if (err) {
		ntfschar *a_name;
		le32 type;

		if (err != ENOSPC)
			panic("%s(): err != ENOSPC\n", __FUNCTION__);
		if (!have_std_info && !have_idx_root) {
			/*
			 * We moved all attributes out of the base mft record
			 * and we still do not have enough space to add the
			 * attribute list attribute.
			 *
			 * TODO: The only thing that can help is to defragment
			 * the attribute list attribute and/or other fragmented
			 * attributes.  The former would make the runlist of
			 * the attribute list attribute directly smaller thus
			 * it may then fit and the latter would reduce the
			 * number of extent attributes and thus the number of
			 * attribute list attribute entries which would
			 * indirectly make the runlist of the attribute list
			 * attribute smaller.  For now we do not implement
			 * defragmentation so there is nothing we can do when
			 * this case occurs.  It should be very, very hard to
			 * trigger this case though and I doubt even the
			 * Windows NTFS driver deals with it so we are not
			 * doing any worse than Windows so I think leaving
			 * things as they are is acceptable.
			 */
out_of_space:
			ntfs_error(vol->mp, "The runlist of the attribute "
					"list attribute is too large to fit "
					"in the base mft record.  You need to "
					"defragment your volume and then try "
					"again.");
			err = ENOSPC;
			goto undo;
		}
		/*
		 * Start with the index root(s) if present then do the standard
		 * information attribute.
		 *
		 * TODO: Need to get these cases triggered and then need to run
		 * chkdsk to check for validity of moving these attributes out
		 * of the base mft record.
		 */
		if (have_idx_root)
			type = AT_INDEX_ROOT;
		else {
try_std_info:
			type = AT_STANDARD_INFORMATION;
			have_std_info = FALSE;
		}
		/*
		 * Find the attribute in the base mft record.  Note for the
		 * index root there can be several index root attributes so we
		 * can get here multiple times once for each index root.
		 */
		ntfs_attr_search_ctx_reinit(&al_ctx);
		al_ctx.is_iteration = 1;
		err = ntfs_attr_find_in_mft_record(type, NULL, 0, NULL, 0,
				&al_ctx);
		if (err) {
			/*
			 * The mft record cannot be corrupt given we have fully
			 * managed to parse it when generating the attribute
			 * list attribute entries.
			 */
			if (err != ENOENT)
				panic("%s(): err != ENOENT\n", __FUNCTION__);
			/*
			 * We should never get here for the standard
			 * information attribute.
			 */
			if (type != AT_INDEX_ROOT)
				panic("%s(): type != AT_INDEX_ROOT\n",
						__FUNCTION__);
			if (!have_idx_root)
				panic("%s(): !have_idx_root\n", __FUNCTION__);
			/*
			 * We have done the index root attribute(s).  Now try
			 * the standard information attribute if present.
			 */
			have_idx_root = FALSE;
			if (have_std_info)
				goto try_std_info;
			/* We have really run out of space. */
			goto out_of_space;
		}
		a = al_ctx.a;
		/*
		 * If the attribute is resident and we can expect the
		 * non-resident form to be smaller than the resident form
		 * switch the attribute to non-resident now.
		 *
		 * FIXME: We cannot do this at present unless the attribute is
		 * the attribute being resized as there could be an ntfs inode
		 * matching this attribute in memory and it would become out of
		 * date with its metadata if we touch its attribute record.
		 *
		 * FIXME: We do not need to do this if this is the attribute
		 * being resized as the caller will have already tried to make
		 * the attribute non-resident and this will not have worked or
		 * we would never have gotten here in the first place.
		 */
		/*
		 * Move the attribute out to an extent mft record and update
		 * its attribute list entry.  If it is the attribute to be
		 * resized, also update the attribute search context to match
		 * the new attribute.  If it is not the attribute to be resized
		 * but it is in front of the attribute to be resized update the
		 * attribute search context to account for the removed
		 * attribute record.
		 *
		 * But first find the attribute list entry matching the
		 * attribute record so it can be updated.
		 */
		a_name = (ntfschar*)((u8*)a + le16_to_cpu(a->name_offset));
		al_entry = (ATTR_LIST_ENTRY*)base_ni->attr_list;
		do {
			/*
			 * We only just generated the attribute list thus it
			 * cannot be corrupt.
			 */
			if ((u8*)al_entry >= al_end || !al_entry->length)
				panic("%s(): (u8*)al_entry >= al_end || "
						"!al_entry->length\n",
						__FUNCTION__);
			if (al_entry->mft_reference == mref &&
					al_entry->instance == a->instance) {
				/*
				 * We found the entry, stop looking but first
				 * perform a quick sanity check that we really
				 * do have the correct attribute record.
				 */
				if (al_entry->type != a->type)
					panic("%s(): al_entry->type != "
							"a->type\n",
							__FUNCTION__);
				if (!ntfs_are_names_equal(
						(ntfschar*)((u8*)al_entry +
						al_entry->name_offset),
						al_entry->name_length, a_name,
						a->name_length, TRUE,
						vol->upcase, vol->upcase_len))
					panic("%s(): !ntfs_are_named_equal()\n",
							__FUNCTION__);
				break;
			}
			/* Go to the next attribute list entry. */
			al_entry = (ATTR_LIST_ENTRY*)((u8*)al_entry +
					le16_to_cpu(al_entry->length));
		} while (1);
		err = ntfs_attr_record_move_for_attr_list_attribute(&al_ctx,
				al_entry, ctx, NULL);
		if (err) {
			ntfs_error(vol->mp, "Failed to move attribute type "
					"0x%x out of base mft record 0x%llx "
					"and into an extent mft record (error "
					"%d).", (unsigned)le32_to_cpu(a->type),
					(unsigned long long)base_ni->mft_no,
					err);
			goto undo;
		}
		/*
		 * We moved an attribute out of the base mft record so retry to
		 * insert the attribute list attribute now that we have more
		 * space in the base mft record.
		 */
		goto insert;
	}
	/*
	 * Now setup the new non-resident attribute list attribute record.  The
	 * entire attribute has been zeroed and the length of the attribute
	 * record has been already set up by ntfs_attr_record_make_space().
	 */
	a->type = AT_ATTRIBUTE_LIST;
	a->non_resident = 1;
	a->mapping_pairs_offset = a->name_offset = const_cpu_to_le16(
			offsetof(ATTR_RECORD, compressed_size));
	a->instance = m->next_attr_instance;
	/*
	 * Increment the next attribute instance number in the mft record as we
	 * consumed the old one.
	 */
	m->next_attr_instance = cpu_to_le16(
			(le16_to_cpu(m->next_attr_instance) + 1) & 0xffff);
	a->highest_vcn = cpu_to_sle64((attr_len / vol->cluster_size) - 1);
	a->allocated_size = cpu_to_sle64(attr_len);
	a->initialized_size = a->data_size = cpu_to_sle64(al_size);
	/*
	 * Generate the mapping pairs array into place.  This cannot fail as we
	 * determined how much space we need and then made that much space
	 * available.  So unless we did something wrong we must have enough
	 * space available and the runlist must be in order.
	 */
	err = ntfs_mapping_pairs_build(vol, (s8*)a + offsetof(ATTR_RECORD,
			compressed_size), mp_size, base_ni->attr_list_rl.rl, 0,
			-1, NULL);
	if (err)
		panic("%s(): err (ntfs_mapping_pairs_build())\n", __FUNCTION__);
done:
	/* Ensure the changes make it to disk later. */
	NInoSetMrecNeedsDirtying(base_ni);
	/*
	 * If we inserted the attribute list attribute as a non-resident
	 * attribute, write it out to disk now.
	 */
	if (a->non_resident) {
		err = ntfs_rl_write(vol, al, al_size, &base_ni->attr_list_rl,
				0, 0);
		if (err) {
			ntfs_error(vol->mp, "Failed to write non-resident "
					"attribute list attribute value to "
					"disk (error %d).", err);
			goto rm_undo;
		}
	}
	/*
	 * Set the base ntfs inode up to reflect that it now has an attribute
	 * list attribute.
	 */
	base_ni->attr_list = al;
	base_ni->attr_list_size = al_size;
	base_ni->attr_list_alloc = al_alloc;
	NInoSetAttrList(base_ni);
	lck_rw_unlock_exclusive(&base_ni->attr_list_rl.lock);
	/*
	 * Update @ctx if the attribute it describes is still in the base mft
	 * record to reflect that the attribute list attribute was inserted in
	 * front of it.
	 */
	if (ctx) {
		if (ctx->ni == base_ni) {
			if ((u8*)a <= (u8*)ctx->a)
				ctx->a = (ATTR_RECORD*)((u8*)ctx->a +
						le32_to_cpu(a->length));
		} else {
			MFT_RECORD *em;

			/*
			 * @ctx is not in the base mft record, map the extent
			 * inode it is in and if it is mapped at a different
			 * address than before update the pointers in @ctx.
			 */
retry_map:
			err2 = ntfs_mft_record_map(ctx->ni, &em);
			if (err2) {
				/*
				 * Something bad has happened.  If out of
				 * memory retry till it succeeds.  Any other
				 * errors are fatal and we return the error
				 * code in @ctx->m.  We cannot undo as we would
				 * need to map the mft record to be able to
				 * move the attribute back into the base mft
				 * record and exactly that operation has just
				 * failed.
				 *
				 * We just need to fudge things so the caller
				 * can reinit and/or put the search context
				 * safely.
				 */
				if (err2 == ENOMEM) {
					(void)thread_block(
							THREAD_CONTINUE_NULL);
					goto retry_map;
				}
				ctx->is_error = 1;
				ctx->error = err2;
				ctx->ni = base_ni;
			}
			if (!ctx->is_error && em != ctx->m) {
				ctx->a = (ATTR_RECORD*)((u8*)em +
						((u8*)ctx->a - (u8*)ctx->m));
				ctx->m = em;
			}
		}
		/*
		 * Finally, update @ctx to reflect the fact that it now is the
		 * result of a search inside an inode with an attribute list
		 * attribute.
		 */
		ctx->base_ni = base_ni;
		ctx->base_m = m;
		ctx->base_a = (ATTR_RECORD*)((u8*)m +
				le16_to_cpu(m->attrs_offset));
	}
	ntfs_debug("Done.");
	return 0;
rm_undo:
	/*
	 * Need to delete the added attribute list attribute record from the
	 * base mft record.
	 */
	ntfs_attr_record_delete_internal(m, a);
undo:
	/*
	 * If we already allocated some clusters for the non-resident attribute
	 * list attribute need to free them now.
	 */
	if (base_ni->attr_list_rl.elements) {
		err2 = ntfs_cluster_free_from_rl(vol, base_ni->attr_list_rl.rl,
				0, -1, NULL);
		if (err2) {
			ntfs_error(vol->mp, "Failed to release allocated "
					"cluster(s) in error code path (error "
					"%d).  Run chkdsk to recover the lost "
					"cluster(s).", err2);
			NVolSetErrors(vol);
		}
		OSFree(base_ni->attr_list_rl.rl, base_ni->attr_list_rl.alloc,
				ntfs_malloc_tag);
		base_ni->attr_list_rl.elements = 0;
		base_ni->attr_list_rl.alloc = 0;
	}
	/*
	 * Move any attribute records that we moved out of the base mft record
	 * back into the base mft record and free the extent mft records we
	 * allocated for them.
	 */
	al_end = al + al_size;
	for (al_entry = (ATTR_LIST_ENTRY*)al; (u8*)al_entry < al_end;
			al_entry = (ATTR_LIST_ENTRY*)((u8*)al_entry +
			le16_to_cpu(al_entry->length))) {
		ntfs_inode *ni;
		ntfschar *a_name;
		u8 *val;
		unsigned val_len;

		/* Skip all attributes that are in the base mft record. */
		if (al_entry->mft_reference == mref)
			continue;
retry_undo_map:
		/* Find the extent record in the base ntfs inode. */
		err2 = ntfs_extent_mft_record_map(base_ni,
				le64_to_cpu(al_entry->mft_reference), &ni, &m);
		if (err2) {
			if (err2 == ENOMEM)
				goto retry_undo_map;
			/*
			 * There is nothing we can do to get this attribute
			 * back into the base mft record thus we simply skip
			 * it and continue with the next one.
			 *
			 * Note we do not free the extent mft record as that
			 * would confuse our mft allocator as we would leave
			 * the mft record itself marked in use.
			 */
			ntfs_error(vol->mp, "Failed to undo move of attribute "
					"type 0x%x in mft_no 0x%llx in error "
					"code path because mapping the extent "
					"mft record 0x%llx faled (error %d).  "
					"Leaving corrupt metadata.  Run "
					"chkdsk.",
					(unsigned)le32_to_cpu(al_entry->type),
					(unsigned long long)base_ni->mft_no,
					(unsigned long long)
					MREF_LE(al_entry->mft_reference), err2);
			NVolSetErrors(vol);
			/*
			 * Proceed with the next one as we may be able to move
			 * others back.
			 */
			continue;
		}
		/* Find the attribute record that needs moving back. */
		a = (ATTR_RECORD*)((u8*)m + le16_to_cpu(m->attrs_offset));
		if (a->type == AT_END)
			panic("%s(): a->type == AT_END\n", __FUNCTION__);
		while (a->instance != al_entry->instance) {
			a = (ATTR_RECORD*)((u8*)a + le32_to_cpu(a->length));
			if (a->type == AT_END)
				panic("%s(): a->type == AT_END\n",
						__FUNCTION__);
		}
		/* We must have found the right one. */
		if (al_entry->type != a->type)
			panic("%s(): al_entry->type != a->type\n",
					__FUNCTION__);
		a_name = (ntfschar*)((u8*)a + le16_to_cpu(a->name_offset));
		if (!ntfs_are_names_equal((ntfschar*)((u8*)al_entry +
				al_entry->name_offset), al_entry->name_length,
				a_name, a->name_length, TRUE, vol->upcase,
				vol->upcase_len))
			panic("%s(): !ntfs_are_names_equal()\n", __FUNCTION__);
		/*
		 * Find the location at which the attribute needs to be
		 * reinserted in the base mft record.
		 *
		 * As there can be multiple otherwise identical filename
		 * attributes we need to search using the attribute value in
		 * this case so we find the correct place to insert the
		 * attribute.
		 */
		if (a->type == AT_FILENAME) {
			val = (u8*)a + le16_to_cpu(a->value_offset);
			val_len = le32_to_cpu(a->value_length);
		} else {
			val = NULL;
			val_len = 0;
		}
		ntfs_attr_search_ctx_reinit(&al_ctx);
		err2 = ntfs_attr_find_in_mft_record(a->type, a_name,
				a->name_length, val, val_len, &al_ctx);
		/* The attribute cannot be there already. */
		if (!err2)
			panic("%s(): !err2\n", __FUNCTION__);
		/*
		 * The mft record cannot be corrupt given we have fully managed
		 * to parse it when generating the attribute list attribute
		 * entries.
		 */
		if (err2 != ENOENT)
			panic("%s(): err2 != ENOENT\n", __FUNCTION__);
		attr_len = le32_to_cpu(a->length);
		/* Make space for the attribute and copy it into place. */
		err2 = ntfs_attr_record_make_space(al_ctx.m, al_ctx.a,
				attr_len);
		/*
		 * This cannot fail as the base mft record must have enough
		 * space to hold the attribute record given it fitted before we
		 * moved it out.
		 */
		if (err2)
			panic("%s(): err2\n", __FUNCTION__);
		memcpy(al_ctx.a, a, attr_len);
		/*
		 * Assign a new instance number to the copied attribute record
		 * as it now has moved into the base mft record.
		 */
		a->instance = al_ctx.m->next_attr_instance;
		/*
		 * Increment the next attribute instance number in the mft
		 * record as we consumed the old one.
		 */
		al_ctx.m->next_attr_instance = cpu_to_le16((le16_to_cpu(
				al_ctx.m->next_attr_instance) + 1) & 0xffff);
		/*
		 * If this is the only attribute in the extent mft record, free
		 * the extent mft record and disconnect it from the base ntfs
		 * inode.
		 *
		 * If this is not the only attribute in the extent mft record,
		 * delete the attribute record from the extent mft record.
		 */
		if (ntfs_attr_record_is_only_one(m, a)) {
			err2 = ntfs_extent_mft_record_free(base_ni, ni, m);
			if (err2) {
				ntfs_error(vol->mp, "Failed to free extent "
						"mft record 0x%llx after "
						"moving back attribute type "
						"0x%x in mft_no 0x%llx in "
						"error code path (error %d).  "
						"Leaving corrupt metadata.  "
						"Run chkdsk.",
						(unsigned long long)ni->mft_no,
						(unsigned)le32_to_cpu(a->type),
						(unsigned long long)
						base_ni->mft_no, err2);
				NVolSetErrors(vol);
				/*
				 * Fall back to at least deleting the attribute
				 * record from the extent mft record to
				 * maintain some consistency.
				 */
				goto delete_err;
			}
		} else {
delete_err:
			ntfs_attr_record_delete_internal(m, a);
			NInoSetMrecNeedsDirtying(ni);
			ntfs_extent_mft_record_unmap(ni);
		}
	}
	/* Finally mark the fully restored base mft record dirty. */
	NInoSetMrecNeedsDirtying(base_ni);
free_err:
	lck_rw_unlock_exclusive(&base_ni->attr_list_rl.lock);
	OSFree(al, al_alloc, ntfs_malloc_tag);
	return err;
}

/**
 * ntfs_attr_list_sync_shrink - update the attribute list of an ntfs inode
 * @ni:		base ntfs inode whose attribute list attribugte to update
 * @start_ofs:	byte offset into attribute list attribute from which to write
 * @ctx:	initialized attribute search context
 *
 * Update the on-disk attribute list attribute of the base ntfs inode @ni.  The
 * caller has updated the attribute list attribute value that is cached in the
 * ntfs inode @ni and this function takes the updated value and resizes the
 * attribute record if necessary and then writes out the modified value
 * starting at offset @start_ofs bytes into the attribute value.
 *
 * This function only works for shrinking the attribute list attribute or if
 * the size has not changed.
 *
 * @ctx is an initialized, ready to use attribute search context that we use to
 * look up the attribute list attribute in the mapped, base mft record.
 *
 * Return 0 on success and errno on error.
 */
errno_t ntfs_attr_list_sync_shrink(ntfs_inode *ni, const unsigned start_ofs,
		ntfs_attr_search_ctx *ctx)
{
	s64 size, alloc_size;
	ntfs_volume *vol;
	ATTR_RECORD *a;
	unsigned mp_size;
	errno_t err;
	BOOL dirty_mft;
	static const char es[] = "  Leaving inconsistent metadata.  Unmount "
			"and run chkdsk.";

	ntfs_debug("Entering for base mft_no 0x%llx.",
			(unsigned long long)ni->mft_no);
	/*
	 * We currently do not implement adding the attribute list attribute.
	 *
	 * Use ntfs_attr_list_add() instead.
	 */
	if (!NInoAttrList(ni))
		panic("%s(): !NInoAttrList(ni)\n", __FUNCTION__);
	if (start_ofs > ni->attr_list_size)
		panic("%s(): start_ofs > ni->attr_list_size\n", __FUNCTION__);
	/*
	 * We should never have allowed the attribute list attribute to grow
	 * above its maximum size.
	 */
	if (ni->attr_list_size > NTFS_MAX_ATTR_LIST_SIZE)
		panic("%s(): ni->attr_list_size > NTFS_MAX_ATTR_LIST_SIZE\n",
				__FUNCTION__);
	vol = ni->vol;
	dirty_mft = FALSE;
	/* Find the attribute list attribute in the base mft record. */
	err = ntfs_attr_find_in_mft_record(AT_ATTRIBUTE_LIST, AT_UNNAMED, 0,
			NULL, 0, ctx);
	if (err) {
		ntfs_error(vol->mp, "Failed to find attribute list attribute "
				"in base mft_no 0x%llx (error %d).  Unmount "
				"and run chkdsk.",
				(unsigned long long)ni->mft_no, err);
		NVolSetErrors(vol);
		return EIO;
	}
	a = ctx->a;
	size = ntfs_attr_size(a);
	/*
	 * We currently only implement shrinking the attribute list attribute.
	 *
	 * Use ntfs_attr_list_sync_extend() when extending it.
	 */
	if (ni->attr_list_size > size)
		panic("%s(): ni->attr_list_size > size\n", __FUNCTION__);
	/*
	 * If the size has not changed skip the resize and go straight onto the
	 * data update.
	 */
	if (ni->attr_list_size == size) {
		if (!a->non_resident)
			goto update_resident;
		goto update_non_resident;
	}
	/* Shrink the attribute list attribute. */
	if (!a->non_resident) {
		/*
		 * The attribute list attribute is resident, shrink the
		 * attribute record and value.
		 */
		err = ntfs_resident_attr_value_resize(ctx->m, a,
				ni->attr_list_size);
		/* Shrinking the attribute record cannot fail. */
		if (err)
			panic("%s(): err (resident)\n", __FUNCTION__);
		dirty_mft = TRUE;
update_resident:
		/* Update the part of the attribute value that has changed. */
		if (start_ofs < ni->attr_list_size) {
			memcpy((u8*)a + le16_to_cpu(a->value_offset) +
					start_ofs, ni->attr_list + start_ofs,
					ni->attr_list_size - start_ofs);
			dirty_mft = TRUE;
		}
		goto done;
	}
	/*
	 * The attribute list attribute is non-resident, as we are shrinking it
	 * we need to update the attribute sizes first.
	 */
	lck_rw_lock_exclusive(&ni->attr_list_rl.lock);
	/* Update the attribute sizes. */
	if (ni->attr_list_size < sle64_to_cpu(a->initialized_size))
		a->initialized_size = cpu_to_sle64(ni->attr_list_size);
	a->data_size = cpu_to_sle64(ni->attr_list_size);
	dirty_mft = TRUE;
	alloc_size = (ni->attr_list_size + vol->cluster_size_mask) &
			~vol->cluster_size_mask;
	if (alloc_size > sle64_to_cpu(a->allocated_size))
		panic("%s(): alloc_size > sle64_to_cpu(a->allocated_size)\n",
				__FUNCTION__);
	/*
	 * If the attribute allocation has not changed we are done with the
	 * resize and go straight onto the data update.
	 */
	if (alloc_size == sle64_to_cpu(a->allocated_size))
		goto update_non_resident;
	/*
	 * The allocated size has shrunk by at least one cluster thus we need
	 * to free the no longer in-use clusters and truncate the runlist
	 * accordingly.  We then also need to update the mapping pairs array in
	 * the attribute record as well as the allocated size and the highest
	 * vcn.
	 */
	err = ntfs_cluster_free_from_rl(vol, ni->attr_list_rl.rl,
			alloc_size >> vol->cluster_size_shift, -1, NULL);
	if (err) {
		ntfs_warning(vol->mp, "Failed to free some allocated clusters "
				"belonging to the attribute list attribute of "
				"mft_no 0x%llx (error %d).  Unmount and run "
				"chkdsk to recover the lost space.",
				(unsigned long long)ni->mft_no, err);
		NVolSetErrors(vol);
	}
	err = ntfs_rl_truncate_nolock(vol, &ni->attr_list_rl,
			alloc_size >> vol->cluster_size_shift);
	if (err) {
		ntfs_warning(vol->mp, "Failed to truncate attribute list "
				"attribute runlist of mft_no 0x%llx (error "
				"%d).%s", (unsigned long long)ni->mft_no, err,
				es);
		goto err;
	}
	err = ntfs_get_size_for_mapping_pairs(vol, ni->attr_list_rl.rl, 0, -1,
			&mp_size);
	if (err) {
		ntfs_error(vol->mp, "Cannot shrink attribute list attribute "
				"of mft_no 0x%llx because determining the "
				"size for the mapping pairs failed (error "
				"%d).%s", (unsigned long long)ni->mft_no, err,
				es);
		goto err;
	}
	err = ntfs_mapping_pairs_build(vol, (s8*)a +
			le16_to_cpu(a->mapping_pairs_offset), mp_size,
			ni->attr_list_rl.rl, 0, -1, NULL);
	if (err) {
		ntfs_error(vol->mp, "Cannot shrink attribute list attribute "
				"of mft_no 0x%llx because building the "
				"mapping pairs failed (error %d).%s",
				(unsigned long long)ni->mft_no, err, es);
		goto err;
	}
	a->allocated_size = cpu_to_sle64(alloc_size);
	a->highest_vcn = cpu_to_sle64((alloc_size >> vol->cluster_size_shift) -
			1);
	err = ntfs_attr_record_resize(ctx->m, a,
			le16_to_cpu(a->mapping_pairs_offset) + mp_size);
	/* Shrinking the attribute record cannot fail. */
	if (err)
		panic("%s(): err (non-resident)\n", __FUNCTION__);
update_non_resident:
	/* Write the modified attribute list attribute value to disk. */
	err = ntfs_rl_write(vol, ni->attr_list, ni->attr_list_size,
			&ni->attr_list_rl, start_ofs, 0);
	if (err) {
		ntfs_error(vol->mp, "Failed to update attribute list "
				"attribute of mft_no 0x%llx (error %d).%s",
				(unsigned long long)ni->mft_no, err, es);
		goto err;
	}
	lck_rw_unlock_exclusive(&ni->attr_list_rl.lock);
done:
	ntfs_debug("Done.");
out:
	/* Make sure the modified mft record is written out. */
	if (dirty_mft)
		NInoSetMrecNeedsDirtying(ni);
	return err;
err:
	lck_rw_unlock_exclusive(&ni->attr_list_rl.lock);
	NVolSetErrors(vol);
	err = EIO;
	goto out;
}

/**
 * ntfs_attr_list_sync_extend - sync an inode's attribute list attribute
 * @base_ni:	base ntfs inode whose attribute list attribute to sync
 * @base_m:	base mft record of the ntfs inode @base_ni
 * @al_ofs:	offset into attribute list attribute at which to begin syncing
 * @ctx:	attribute search context describing attribute being resized
 *
 * Sync the attribute list attribute of the base ntfs inode @base_ni with mft
 * record @base_m by extending it to fit the cached attribute list attribute
 * value, then copying the modified, cached attribute list value into place in
 * the extended attribute list attribute value starting at offset @al_ofs into
 * the attribute list attribute.
 *
 * @ctx is an attribute search context describing the attribute being resized
 * which is causing the attribute list attribute to be added.  The search
 * context describes a mapped mft record and this is unmapped as needed and
 * then mapped again.  Also the pointers in the search context are updated with
 * the new location of the attribute in the mft record if it is moved and with
 * the new location in memory of the re-mapped mft record if it is re-mapped at
 * a different virtual address.
 *
 * Return 0 on success and errno on error.
 *
 * WARNING: If @ctx is supplied, regardless of whether success or failure is
 *	    returned, you need to check @ctx->is_error and if 1 the @ctx is no
 *	    longer valid, i.e. you need to either call
 *	    ntfs_attr_search_ctx_reinit() or ntfs_attr_search_ctx_put() on it.
 *	    In that case @ctx->error will give you the error code for why the
 *	    mapping of the inode failed.
 *
 * NOTE: When this function fails it is very likely that the attribute list
 *	 attribute value both in memory and on-disk has been modified but it is
 *	 also very likely that a disparity is left between the in-memory and
 *	 the on-disk value thus the caller should undo their extension to the
 *	 attribute list attribute value and then rewrite the complete attribute
 *	 list attribute starting at offset zero bytes to ensure everything is
 *	 made consistent.
 */
errno_t ntfs_attr_list_sync_extend(ntfs_inode *base_ni, MFT_RECORD *base_m,
		unsigned al_ofs, ntfs_attr_search_ctx *ctx)
{
	leMFT_REF mref;
	LCN lcn;
	ntfs_volume *vol;
	ATTR_RECORD *al_a, *a;
	ATTR_LIST_ENTRY *al_entry;
	u8 *al_end;
	unsigned bytes_needed, bytes_free, alloc_size, name_ofs;
	unsigned mp_size, mp_ofs, old_arec_size;
	s64 arec_size;
	errno_t err, err2;
	le32 type;
	BOOL dirty_mft, remap_needed;
	ntfs_attr_search_ctx al_ctx, actx;
	ntfs_runlist runlist;

	ntfs_debug("Entering for base mft_no 0x%llx, offset 0x%x.",
			(unsigned long long)base_ni->mft_no, al_ofs);
	if (!NInoAttrList(base_ni))
		panic("%s(): !NInoAttrList(base_ni)\n", __FUNCTION__);
	if (al_ofs > base_ni->attr_list_size)
		panic("%s(): al_ofs > base_ni->attr_list_size\n",
				__FUNCTION__);
	/*
	 * We should never have allowed the attribute list attribute to grow
	 * above its maximum size.
	 */
	if (base_ni->attr_list_size > NTFS_MAX_ATTR_LIST_SIZE)
		panic("%s(): base_ni->attr_list_size > "
				"NTFS_MAX_ATTR_LIST_SIZE\n", __FUNCTION__);
	vol = base_ni->vol;
	remap_needed = dirty_mft = FALSE;
	/* Find the attribute list attribute record in the base mft record. */
	ntfs_attr_search_ctx_init(&al_ctx, base_ni, base_m);
	err = ntfs_attr_find_in_mft_record(AT_ATTRIBUTE_LIST, AT_UNNAMED, 0,
			NULL, 0, &al_ctx);
	if (err) {
		ntfs_error(vol->mp, "Failed to look up attribute list "
				"attribute in base mft_no 0x%llx (error %d).",
				(unsigned long long)base_ni->mft_no, err);
		return err;
	}
	al_a = al_ctx.a;
	arec_size = ntfs_attr_size(al_a);
	/*
	 * We currently only implement extending the attribute list attribute.
	 *
	 * Use ntfs_attr_list_sync_shrink() when shrinking it.
	 */
	if (base_ni->attr_list_size < arec_size)
		panic("%s(): base_ni->attr_list_size < arec_size\n",
				__FUNCTION__);
	/*
	 * If the size has not changed skip the resize and go straight onto the
	 * data update.
	 */
	if (base_ni->attr_list_size == arec_size) {
		if (!al_a->non_resident)
			goto update_resident;
		lck_rw_lock_exclusive(&base_ni->attr_list_rl.lock);
		goto update_non_resident;
	}
	mref = MK_LE_MREF(base_ni->mft_no, base_ni->seq_no);
	if (al_a->non_resident)
		goto non_resident;
	arec_size = le32_to_cpu(al_a->length);
	/* Extend the attribute list attribute record value. */
	err = ntfs_resident_attr_value_resize(al_ctx.m, al_a,
			base_ni->attr_list_size);
	if (!err) {
		/*
		 * Update the pointer in @ctx because the attribute has now
		 * moved inside the mft record.
		 */
		if (ctx->ni == base_ni && (u8*)al_a <= (u8*)ctx->a)
			ctx->a = (ATTR_RECORD*)((u8*)ctx->a + (signed)
					((signed)le32_to_cpu(al_a->length) -
					(signed)arec_size));
update_resident:
		dirty_mft = TRUE;
		/*
		 * Update the part of the attribute list attribute record value
		 * that has changed.
		 */
		memcpy((u8*)al_a + le16_to_cpu(al_a->value_offset) + al_ofs,
				base_ni->attr_list + al_ofs,
				base_ni->attr_list_size - al_ofs);
		goto done;
	}
	if (err != ENOSPC)
		panic("%s(): err != ENOSPC\n", __FUNCTION__);
	/*
	 * There was not enough space in the mft record to extend the attribute
	 * list attribute record.  Deal with this by making other attributes
	 * non-resident and/or moving other attributes out to extent mft
	 * records and if that is not enough make the attribute list attribute
	 * non-resident.
	 */
	bytes_needed = ((base_ni->attr_list_size + 7) & ~7) -
			((le32_to_cpu(al_a->value_length) + 7) & ~7);
	/* Maximum number of usable bytes in the mft record. */
	bytes_free = le32_to_cpu(base_m->bytes_allocated) -
			le16_to_cpu(base_m->attrs_offset);
	/*
	 * If the attribute list attribute has become bigger than fits in an
	 * mft record switch it to a non-resident attribute record.
	 */
	if (bytes_needed > bytes_free)
		goto make_non_resident;
	/*
	 * Need to unmap the extent mft record for now which means we have to
	 * mark it dirty first.
	 */
	if (ctx->ni != base_ni) {
		NInoSetMrecNeedsDirtying(ctx->ni);
		ntfs_extent_mft_record_unmap(ctx->ni);
		remap_needed = TRUE;
	}
	ntfs_attr_search_ctx_init(&actx, base_ni, base_m);
	actx.is_iteration = 1;
	while (bytes_needed > le32_to_cpu(base_m->bytes_allocated) -
			le32_to_cpu(base_m->bytes_in_use)) {
		ntfschar *a_name;

		/* Get the next attribute in the mft record. */
		err = ntfs_attr_find_in_mft_record(AT_UNUSED, NULL, 0, NULL, 0,
				&actx);
		if (err) {
			/*
			 * We reached the end and we still do not have enough
			 * space to extend the attribute list attribute as a
			 * resident attribute record thus try to switch it to a
			 * non-resident attribute record.
			 */
			if (err == ENOENT)
				goto make_non_resident;
			/*
			 * The base mft record is corrupt.  There is nothing we
			 * can do just bail out.  Note we are leaving the
			 * in-memory attribute list attribute out of sync with
			 * the on-disk attribute list attribute after possibly
			 * having moved some attributes out of the base mft
			 * record.  We hope the caller will manage to undo the
			 * extension it did in memory and then write out the
			 * attribute list attribute to disk.  This would bring
			 * back in sync any changes we just made and if it
			 * fails the caller will then display appropriate
			 * corruption announcing error messages.
			 */
			ntfs_error(vol->mp, "Failed to iterate over attribute "
					"records in base mft record 0x%llx "
					"(error %d).%s",
					(unsigned long long)base_ni->mft_no,
					err, dirty_mft ? "  Run chkdsk." : "");
			if (dirty_mft)
				NVolSetErrors(vol);
			goto done;
		}
		a = actx.a;
		/*
		 * Skip the attribute list attribute itself as that is not
		 * represented inside itself and it must stay in the base mft
		 * record.
		 */
		if (a->type == AT_ATTRIBUTE_LIST)
			continue;
		/*
		 * Do not touch standard information and index root attributes
		 * at this stage.  They will be moved out to extent mft records
		 * further below if really necessary.
		 *
		 * TODO: Probably want to add unnamed data attributes to this
		 * as well.
		 */
		if (a->type == AT_STANDARD_INFORMATION ||
				a->type == AT_INDEX_ROOT) {
			/*
			 * If the attribute list attribute has become bigger
			 * than fits in the mft record switch it to a
			 * non-resident attribute record.
			 */
			bytes_free -= le32_to_cpu(a->length);
			if (bytes_needed > bytes_free)
				goto make_non_resident;
			continue;
		}
		/*
		 * If the attribute is resident and we can expect the
		 * non-resident form to be smaller than the resident form
		 * switch the attribute to non-resident now.
		 *
		 * FIXME: We cannot do this at present unless the attribute is
		 * the attribute being resized as there could be an ntfs inode
		 * matching this attribute in memory and it would become out of
		 * date with its metadata if we touch its attribute record.
		 *
		 * FIXME: We do not need to do this if this is the attribute
		 * being resized as the caller will have already tried to make
		 * the attribute non-resident and this will not have worked or
		 * we would never have gotten here in the first place.
		 */
		/*
		 * Move the attribute out to an extent mft record and update
		 * its attribute list entry.  If it is the attribute to be
		 * resized, also update the attribute search context to match
		 * the new attribute.  If it is not the attribute to be resized
		 * but it is in front of the attribute to be resized update the
		 * attribute search context to account for the removed
		 * attribute record.
		 *
		 * But first find the attribute list entry matching the
		 * attribute record so it can be updated.
		 */
		a_name = (ntfschar*)((u8*)a + le16_to_cpu(a->name_offset));
		al_entry = (ATTR_LIST_ENTRY*)base_ni->attr_list;
		al_end = base_ni->attr_list + base_ni->attr_list_size;
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
				goto done;
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
				goto done;
			}
			/* Go to the next attribute list entry. */
			al_entry = (ATTR_LIST_ENTRY*)((u8*)al_entry +
					le16_to_cpu(al_entry->length));
		} while (1);
		err = ntfs_attr_record_move_for_attr_list_attribute(&actx,
				al_entry, ctx, &remap_needed);
		if (err) {
			ntfs_error(vol->mp, "Failed to move attribute type "
					"0x%x out of base mft record 0x%llx "
					"and into an extent mft record (error "
					"%d).%s",
					(unsigned)le32_to_cpu(a->type),
					(unsigned long long)base_ni->mft_no,
					err, dirty_mft ? "  Run chkdsk." : "");
			if (dirty_mft)
				NVolSetErrors(vol);
			goto done;
		}
		/* We modified the base mft record. */
		dirty_mft = TRUE;
		/*
		 * If the modified attribute list entry is before the current
		 * start of attribute list modification we need to sync this
		 * entry as well.  For simplicity we just set @al_ofs to the
		 * new value thus syncing everything starting at that offset.
		 */
		if ((u8*)al_entry - base_ni->attr_list < (long)al_ofs)
			al_ofs = (unsigned)((u8*)al_entry - base_ni->attr_list);
	}
	/*
	 * Find the attribute list attribute record in the base mft record
	 * again in case it has moved.  This cannot fail as we looked it up
	 * successfully above.
	 */
	ntfs_attr_search_ctx_reinit(&al_ctx);
	err = ntfs_attr_find_in_mft_record(AT_ATTRIBUTE_LIST, AT_UNNAMED, 0,
			NULL, 0, &al_ctx);
	if (err)
		panic("%s(): err (attribute list attribute not found)\n",
				__FUNCTION__);
	al_a = al_ctx.a;
	arec_size = le32_to_cpu(al_a->length);
	/* Try to extend the attribute list attribute record value. */
	err = ntfs_resident_attr_value_resize(base_m, al_a,
			base_ni->attr_list_size);
	if (!err) {
		dirty_mft = TRUE;
		/*
		 * Update the part of the attribute list attribute record value
		 * that has changed.
		 */
		memcpy((u8*)al_a + le16_to_cpu(al_a->value_offset) + al_ofs,
				base_ni->attr_list + al_ofs,
				base_ni->attr_list_size - al_ofs);
		/*
		 * Update the pointer in @ctx because the attribute has now
		 * moved inside the mft record.
		 */
		if (ctx->ni == base_ni && (u8*)al_a <= (u8*)ctx->a)
			ctx->a = (ATTR_RECORD*)((u8*)ctx->a + (signed)(
					(signed)le32_to_cpu(al_a->length) -
					(signed)arec_size));
		goto done;
	}
	if (err != ENOSPC)
		panic("%s(): err != ENOSPC\n", __FUNCTION__);
	/*
	 * There was not enough space in the mft record to extend the attribute
	 * list attribute record.  Deal with this by making the attribute list
	 * attribute non-resident and then extending it.
	 */
make_non_resident:
	/* Work out the new allocated size we need. */
	alloc_size = (base_ni->attr_list_size + vol->cluster_size_mask) &
			~vol->cluster_size_mask;
	if (!alloc_size)
		panic("%s(): !alloc_size\n", __FUNCTION__);
	lck_rw_lock_exclusive(&base_ni->attr_list_rl.lock);
	if (al_a->non_resident)
		panic("%s(): al_a->non_resident\n", __FUNCTION__);
	if (base_ni->attr_list_rl.rl)
		panic("%s(): base_ni->attr_list_rl.rl\n", __FUNCTION__);
	/*
	 * Start by allocating clusters to hold the attribute list attribute
	 * value.
	 */
	err = ntfs_cluster_alloc(vol, 0, alloc_size >> vol->cluster_size_shift,
			-1, DATA_ZONE, TRUE, &base_ni->attr_list_rl);
	if (err) {
		ntfs_error(vol->mp, "Failed to allocate cluster%s for "
				"non-resident attribute list attribute in "
				"base mft record 0x%llx (error %d).",
				(alloc_size >> vol->cluster_size_shift) > 1 ?
				"s" : "", (unsigned long long)base_ni->mft_no,
				err);
		goto unl_done;
	}
	/*
	 * Determine the size of the mapping pairs array.
	 *
	 * This cannot fail as we just allocated the runlist so it must be ok.
	 */
	err = ntfs_get_size_for_mapping_pairs(vol, base_ni->attr_list_rl.rl, 0,
			-1, &mp_size);
	if (err)
		panic("%s(): err (ntfs_get_size_for_mapping_pairs(), "
				"resident)\n", __FUNCTION__);
	/* Calculate new offsets for the name and the mapping pairs array. */
	name_ofs = (offsetof(ATTR_REC, compressed_size) + 7) & ~7;
	mp_ofs = (name_ofs + 7) & ~7;
	/*
	 * Determine the size of the resident part of the now non-resident
	 * attribute record.
	 */
	old_arec_size = le32_to_cpu(al_a->length);
	arec_size = (mp_ofs + mp_size + 7) & ~7;
	/*
	 * Resize the resident part of the attribute record.
	 *
	 * This cannot fail because the attribute list attribute must already
	 * contain at least three entries, one for the compulsory standard
	 * information attribute, one for the compulsory filename attribute,
	 * and one for the compulsory data or index root attribute and a
	 * resident attribute record containing three attribute list attribute
	 * entries in its attribute value is larger than the number of bytes
	 * needed to create a maximum length non-resident attribute record that
	 * can possibly be created here.  And if the inode is corrupt we would
	 * never have gotten here as this case would be detected when the inode
	 * is read in.
	 */
	err = ntfs_attr_record_resize(base_m, al_a, (u32)arec_size);
	if (err)
		panic("%s(): err (ntfs_attr_record_resize())\n", __FUNCTION__);
	/*
	 * Convert the resident part of the attribute record to describe a
	 * non-resident attribute.
	 */
	al_a->non_resident = 1;
	al_a->name_offset = cpu_to_le16(name_ofs);
	/* The flags should be zero already but re-set them anyway. */
	al_a->flags = 0;
	/* Setup the fields specific to non-resident attributes. */
	al_a->lowest_vcn = 0;
	al_a->mapping_pairs_offset = cpu_to_le16(mp_ofs);
	al_a->compression_unit = 0;
	memset(&al_a->reservedN, 0, sizeof(al_a->reservedN));
	/* We need to write the whole attribute list attribute. */
	al_ofs = 0;
	goto write_non_resident;
non_resident:
	/* The attribute list attribute is non-resident. */
	lck_rw_lock_exclusive(&base_ni->attr_list_rl.lock);
	if (!al_a->non_resident)
		panic("%s(): !al_a->non_resident\n", __FUNCTION__);
	/* Allocate more disk space if needed. */
	if (base_ni->attr_list_size <= sle64_to_cpu(al_a->allocated_size))
		goto skip_alloc;
	/* Work out the new allocated size we need. */
	alloc_size = (base_ni->attr_list_size + vol->cluster_size_mask) &
			~vol->cluster_size_mask;
	if (!alloc_size)
		panic("%s(): !alloc_size\n", __FUNCTION__);
	/* Find the last allocated cluster of the existing runlist. */
	lcn = -1;
	if (al_a->allocated_size) {
		ntfs_rl_element *rl;

		if (!base_ni->attr_list_rl.elements)
			panic("%s(): !base_ni->attr_list_rl.elements\n",
					__FUNCTION__);
		rl = &base_ni->attr_list_rl.rl[base_ni->attr_list_rl.elements -
				1];
		while (rl->lcn < 0 && rl > base_ni->attr_list_rl.rl)
			rl--;
		if (rl->lcn >= 0)
			lcn = rl->lcn + rl->length;
	}
	/*
	 * Allocate clusters for the extension of the attribute list attribute
	 * value.
	 */
	runlist.rl = NULL;
	runlist.alloc = runlist.elements = 0;
	err = ntfs_cluster_alloc(vol, sle64_to_cpu(al_a->allocated_size) >>
			vol->cluster_size_shift, (alloc_size -
			sle64_to_cpu(al_a->allocated_size)) >>
			vol->cluster_size_shift, lcn, DATA_ZONE, TRUE,
			&runlist);
	if (err) {
		ntfs_error(vol->mp, "Failed to allocate cluster%s to extend "
				"non-resident attribute list attribute in "
				"base mft record 0x%llx (error %d).",
				((alloc_size -
				sle64_to_cpu(al_a->allocated_size)) >>
				vol->cluster_size_shift) > 1 ? "s" : "",
				(unsigned long long)base_ni->mft_no, err);
		goto unl_done;
	}
	err = ntfs_rl_merge(&base_ni->attr_list_rl, &runlist);
	if (err) {
		ntfs_error(vol->mp, "Failed to extend attribute list "
				"attribute in base mft record 0x%llx because "
				"the runlist merge failed (error %d).",
				(unsigned long long)base_ni->mft_no, err);
		err2 = ntfs_cluster_free_from_rl(vol, runlist.rl, 0, -1, NULL);
		if (err2) {
			ntfs_warning(vol->mp, "Failed to release allocated "
					"cluster(s) in error code path (error "
					"%d).  Run chkdsk to recover the lost "
					"cluster(s).", err2);
			NVolSetErrors(vol);
		}
		OSFree(runlist.rl, runlist.alloc, ntfs_malloc_tag);
		goto unl_done;
	}
	/* Determine the size of the mapping pairs array. */
	err = ntfs_get_size_for_mapping_pairs(vol, base_ni->attr_list_rl.rl, 0,
			-1, &mp_size);
	if (err)
		panic("%s(): err (ntfs_get_size_for_mapping_pairs(), "
				"non-resident)\n", __FUNCTION__);
	mp_ofs = le16_to_cpu(al_a->mapping_pairs_offset);
	old_arec_size = le32_to_cpu(al_a->length);
	/* Extend the attribute record to fit the bigger mapping pairs array. */
	err = ntfs_attr_record_resize(base_m, al_a, mp_ofs + mp_size);
	if (!err)
		goto write_non_resident;
	if (err != ENOSPC)
		panic("%s(): err != ENOSPC\n", __FUNCTION__);
	/*
	 * Need to unmap the extent mft record for now which means we have to
	 * mark it dirty first.
	 */
	if (ctx->ni != base_ni) {
		NInoSetMrecNeedsDirtying(ctx->ni);
		ntfs_extent_mft_record_unmap(ctx->ni);
		remap_needed = TRUE;
	}
	bytes_needed = ((mp_ofs + mp_size + 7) & ~7) - old_arec_size;
	type = AT_UNUSED;
next_pass:
	ntfs_attr_search_ctx_init(&actx, base_ni, base_m);
	actx.is_iteration = 1;
	while (bytes_needed > le32_to_cpu(base_m->bytes_allocated) -
			le32_to_cpu(base_m->bytes_in_use)) {
		ntfschar *a_name;

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
				 * We moved all attributes out of the base mft
				 * record and we still do not have enough space
				 * to extend the attribute list attribute.
				 *
				 * TODO: The only thing that can help is to
				 * defragment the attribute list attribute
				 * and/or other fragmented attributes.  The
				 * former would make the runlist of the
				 * attribute list attribute directly smaller
				 * thus it may then fit and the latter would
				 * reduce the number of extent attributes and
				 * thus the number of attribute list attribute
				 * entries which would indirectly make the
				 * runlist of the attribute list attribute
				 * smaller.  For now we do not implement
				 * defragmentation so there is nothing we can
				 * do when this case occurs.  It should be
				 * very, very hard to trigger this case though
				 * and I doubt even the Windows NTFS driver
				 * deals with it so we are not doing any worse
				 * than Windows so I think leaving things as
				 * they are is acceptable.
				 *
				 * The caller will hopefully undo what they did
				 * thus shrinking the attribute list attribute
				 * again which will then fit and they will then
				 * rewrite it which will fix everything we have
				 * failed to do nicely.
				 */
				ntfs_error(vol->mp, "The runlist of the "
						"attribute list attribute of "
						"mft_no 0x%llx is too large to "
						"fit in the base mft record.  "
						"You need to defragment your "
						"volume and then try again.%s",
						(unsigned long long)
						base_ni->mft_no, dirty_mft ?
						"  Run chkdsk." : "");
				if (dirty_mft)
					NVolSetErrors(vol);
				err = ENOSPC;
				goto unl_done;
			}
			/*
			 * The base mft record is corrupt.  There is nothing we
			 * can do just bail out.  Note we are leaving the
			 * in-memory attribute list attribute out of sync with
			 * the on-disk attribute list attribute after possibly
			 * having moved some attributes out of the base mft
			 * record.  We hope the caller will manage to undo the
			 * extension it did in memory and then write out the
			 * attribute list attribute to disk.  This would bring
			 * back in sync any changes we just made and if it
			 * fails the caller will then display appropriate
			 * corruption announcing error messages.
			 */
			ntfs_error(vol->mp, "Failed to iterate over attribute "
					"records in base mft record 0x%llx "
					"(error %d).%s",
					(unsigned long long)base_ni->mft_no,
					err, dirty_mft ? "  Run chkdsk." : "");
			if (dirty_mft)
				NVolSetErrors(vol);
			goto unl_done;
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
		 * If the attribute is resident and we can expect the
		 * non-resident form to be smaller than the resident form
		 * switch the attribute to non-resident now.
		 *
		 * FIXME: We cannot do this at present unless the attribute is
		 * the attribute being resized as there could be an ntfs inode
		 * matching this attribute in memory and it would become out of
		 * date with its metadata if we touch its attribute record.
		 *
		 * FIXME: We do not need to do this if this is the attribute
		 * being resized as the caller will have already tried to make
		 * the attribute non-resident and this will not have worked or
		 * we would never have gotten here in the first place.
		 */
		/*
		 * Move the attribute out to an extent mft record and update
		 * its attribute list entry.  If it is the attribute to be
		 * resized, also update the attribute search context to match
		 * the new attribute.  If it is not the attribute to be resized
		 * but it is in front of the attribute to be resized update the
		 * attribute search context to account for the removed
		 * attribute record.
		 *
		 * But first find the attribute list entry matching the
		 * attribute record so it can be updated.
		 */
		a_name = (ntfschar*)((u8*)a + le16_to_cpu(a->name_offset));
		al_entry = (ATTR_LIST_ENTRY*)base_ni->attr_list;
		al_end = base_ni->attr_list + base_ni->attr_list_size;
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
				goto unl_done;
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
				goto unl_done;
			}
			/* Go to the next attribute list entry. */
			al_entry = (ATTR_LIST_ENTRY*)((u8*)al_entry +
					le16_to_cpu(al_entry->length));
		} while (1);
		err = ntfs_attr_record_move_for_attr_list_attribute(&actx,
				al_entry, ctx, &remap_needed);
		if (err) {
			ntfs_error(vol->mp, "Failed to move attribute type "
					"0x%x out of base mft record 0x%llx "
					"and into an extent mft record (error "
					"%d).%s",
					(unsigned)le32_to_cpu(a->type),
					(unsigned long long)base_ni->mft_no,
					err, dirty_mft ? "  Run chkdsk." : "");
			if (dirty_mft)
				NVolSetErrors(vol);
			goto unl_done;
		}
		/* We modified the base mft record. */
		dirty_mft = TRUE;
		/*
		 * If the modified attribute list entry is before the current
		 * start of attribute list modification we need to sync this
		 * entry as well.  For simplicity we just set @al_ofs to the
		 * new value thus syncing everything starting at that offset.
		 */
		if ((u8*)al_entry - base_ni->attr_list < (long)al_ofs)
			al_ofs = (unsigned)((u8*)al_entry - base_ni->attr_list);
	}
	/*
	 * Find the attribute list attribute record in the base mft record
	 * again in case it has moved.  This cannot fail as we looked it up
	 * successfully above.
	 */
	ntfs_attr_search_ctx_reinit(&al_ctx);
	err = ntfs_attr_find_in_mft_record(AT_ATTRIBUTE_LIST, AT_UNNAMED, 0,
			NULL, 0, &al_ctx);
	if (err)
		panic("%s(): err (attribute list attribute not found 2)\n",
				__FUNCTION__);
	al_a = al_ctx.a;
	/*
	 * Extend the attribute record to fit the bigger mapping pairs array.
	 * This cannot fail as we only get here if we managed to create enough
	 * space.
	 */
	err = ntfs_attr_record_resize(base_m, al_a, mp_ofs + mp_size);
	if (err)
		panic("%s(): err (ntfs_attr_record_resize())\n", __FUNCTION__);
write_non_resident:
	dirty_mft = TRUE;
	/*
	 * Update the pointer in @ctx because the attribute has now moved
	 * inside the mft record.
	 */
	if (ctx->ni == base_ni && (u8*)al_a <= (u8*)ctx->a)
		ctx->a = (ATTR_RECORD*)((u8*)ctx->a +
				(signed)((signed)le32_to_cpu(al_a->length) -
				(signed)old_arec_size));
	/*
	 * Generate the mapping pairs array in place.  This cannot fail as we
	 * determined the amount of space we need already and have ensured that
	 * we have enough space available.
	 */
	err = ntfs_mapping_pairs_build(vol, (s8*)al_a + mp_ofs,
			le32_to_cpu(al_a->length) - mp_ofs,
			base_ni->attr_list_rl.rl, 0, -1, NULL);
	if (err)
		panic("%s(): err (ntfs_mapping_pairs_build())\n", __FUNCTION__);
	/* Update the highest_vcn. */
	al_a->highest_vcn = cpu_to_sle64((alloc_size - 1) >>
			vol->cluster_size_shift);
	al_a->allocated_size = cpu_to_sle64(alloc_size);
skip_alloc:
	/* Update the data size. */
	dirty_mft = TRUE;
	al_a->data_size = cpu_to_sle64(base_ni->attr_list_size);
update_non_resident:
	/*
	 * Write the modified part of the attribute list attribute to disk.
	 *
	 * ntfs_rl_write() will mark the volume dirty and ask the user to run
	 * chkdsk.
	 */
	err = ntfs_rl_write(vol, base_ni->attr_list, base_ni->attr_list_size,
			&base_ni->attr_list_rl, al_ofs, 0);
	if (err)
		ntfs_error(vol->mp, "Failed to update on-disk attribute list "
				"attribute of mft_no 0x%llx (error %d).",
				(unsigned long long)base_ni->mft_no, err);
	/* Update the initialized size. */
	if (al_a->initialized_size != al_a->data_size) {
		dirty_mft = TRUE;
		al_a->initialized_size = al_a->data_size;
	}
unl_done:
	lck_rw_unlock_exclusive(&base_ni->attr_list_rl.lock);
done:
	/* Make sure the modified base mft record is written out. */
	if (dirty_mft)
		NInoSetMrecNeedsDirtying(base_ni);
	if (remap_needed && ctx->ni != base_ni) {
		MFT_RECORD *em;

retry_map:
		err2 = ntfs_mft_record_map(ctx->ni, &em);
		if (err2) {
			/*
			 * Something bad has happened.  If out of memory retry
			 * till it succeeds.  Any other errors are fatal and we
			 * return the error code in @ctx->m.
			 *
			 * We do not need to undo anything as the extension of
			 * the attribute list attribute has already been done
			 * and if it failed the failure has already been dealt
			 * with.
			 *
			 * We just need to fudge things so the caller can
			 * reinit and/or put the search context safely.
			 */
			if (err2 == ENOMEM) {
				(void)thread_block(THREAD_CONTINUE_NULL);
				goto retry_map;
			}
			ctx->is_error = 1;
			ctx->error = err2;
			ctx->ni = base_ni;
		}
		if (!ctx->is_error && em != ctx->m) {
			ctx->a = (ATTR_RECORD*)((u8*)em +
					((u8*)ctx->a - (u8*)ctx->m));
			ctx->m = em;
		}
	}
	return err;
}

/**
 * ntfs_attr_list_entries_delete - delete one or more attribute list entries
 * @ni:			base ntfs inode whose attribute list to delete from
 * @start_entry:	first attribute list entry to be deleted
 * @end_entry:		attribute list entry at which to stop deleting
 *
 * Delete the attribute list attribute entries starting at @start_entry and
 * finishing at @end_entry (@end_entry itself is not deleted) from the
 * attribute list attribute belonging to the base ntfs inode @ni.
 *
 * This function cannot fail.
 */
void ntfs_attr_list_entries_delete(ntfs_inode *ni,
		ATTR_LIST_ENTRY *start_entry, ATTR_LIST_ENTRY *end_entry)
{
	unsigned to_delete, to_copy, new_alloc;

	/*
	 * Determine the number of bytes to be deleted from the attribute list
	 * attribute.
	 */
	to_delete = (unsigned)((u8*)end_entry - (u8*)start_entry);
	ntfs_debug("Entering for base mft_no 0x%llx, attr type 0x%x, "
			"start_entry length 0x%x, start_entry offset 0x%lx, "
			"end_entry offset 0x%lx, bytes to_delete 0x%x.",
			(unsigned long long)ni->mft_no,
			le32_to_cpu(start_entry->type),
			(unsigned)le16_to_cpu(start_entry->length),
			(unsigned long)((u8*)start_entry - ni->attr_list),
			(unsigned long)((u8*)end_entry - ni->attr_list),
			to_delete);
	/*
	 * Determine the number of bytes in the attribute list attribute
	 * following the entries to be deleted.
	 */
	to_copy =
		ni->attr_list_size - (unsigned)((u8*)end_entry - ni->attr_list);
	/*
	 * Determine the new size and allocated size for the attribute list
	 * attribute.
	 */
	ni->attr_list_size -= to_delete;
	new_alloc = (ni->attr_list_size + NTFS_ALLOC_BLOCK - 1) &
			~(NTFS_ALLOC_BLOCK - 1);
	/*
	 * @new_alloc cannot reach zero because the attribute list has to at
	 * least contain the standard information attribute.
	 */
	if (!new_alloc)
		panic("%s(): !new_alloc\n", __FUNCTION__);
	/*
	 * Remove the attribute list entries to be deleted from the in memory
	 * copy of the attribute list attribute and reallocate the attribute
	 * list buffer if it shrinks past an NTFS_ALLOC_BLOCK byte boundary.
	 */
	if (new_alloc >= ni->attr_list_alloc) {
		if (new_alloc > ni->attr_list_alloc)
			panic("%s(): (new_alloc > ni->attr_list_alloc\n",
					__FUNCTION__);
cut:
		/*
		 * If the end entry is not the last entry, move all following
		 * entries forward on top of the entries to be deleted.
		 */
		if (to_copy > 0)
			memmove(start_entry, end_entry, to_copy);
	} else {
		u8 *tmp;
		unsigned entry_ofs;

		tmp = OSMalloc(new_alloc, ntfs_malloc_tag);
		/*
		 * If the allocation fails then do not shrink the buffer and
		 * just cut out the entries to be deleted which will waste some
		 * memory but is otherwise ok.
		 */
		if (!tmp)
			goto cut;
		entry_ofs = (unsigned)((u8*)start_entry - ni->attr_list);
		memcpy(tmp, ni->attr_list, entry_ofs);
		if (to_copy > 0)
			memcpy(tmp + entry_ofs, end_entry, to_copy);
		OSFree(ni->attr_list, ni->attr_list_alloc, ntfs_malloc_tag);
		ni->attr_list_alloc = new_alloc;
		ni->attr_list = tmp;
	}
	ntfs_debug("Done.");
}
