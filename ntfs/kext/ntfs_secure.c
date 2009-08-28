/*
 * ntfs_secure.c - NTFS kernel security ($Secure) handling.
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
#include <sys/stat.h>
#include <sys/ucred.h>
#include <sys/vnode.h>

#include <string.h>

#include <libkern/OSMalloc.h>

#include <kern/debug.h>
#include <kern/locks.h>

#include "ntfs.h"
#include "ntfs_attr.h"
#include "ntfs_debug.h"
#include "ntfs_endian.h"
#include "ntfs_inode.h"
#include "ntfs_mft.h"
#include "ntfs_page.h"
#include "ntfs_types.h"
#include "ntfs_layout.h"
#include "ntfs_secure.h"
#include "ntfs_volume.h"

/*
 * Default $SDS entries containing the default security descriptors to apply to
 * newly created files (ntfs_file_sds_entry) and directories
 * (ntfs_dir_sds_entry).
 *
 * On ntfs 1.x volumes, use the ntfs_file_sds_entry_old as the default for
 * files and ntfs_dir_sds_entry_old for directories.
 */
SDS_ENTRY *ntfs_file_sds_entry, *ntfs_dir_sds_entry;
SDS_ENTRY *ntfs_file_sds_entry_old, *ntfs_dir_sds_entry_old;

/**
 * ntfs_default_sds_entry_init - set up one of the default SDS entries
 * @sds:	destination buffer for SDS entry
 * @dir:	if true, create the default directory SDS entry
 * @old:	if true, create an NT4 style SDS entry
 *
 * Set up one of the default SDS entries in the destination buffer @sds.  The
 * generated security descriptor specifies the "Everyone" SID as both the owner
 * and the group and grants full access to the "Everyone" SID.
 *
 * If @dir is true, create the default directory SDS entry.  If @dir is false,
 * create the default file SDS entry.  The difference is that the directory SDS
 * entry contains an ACE which specifies that directories and files created
 * inside the directory to which the SDS entry applies will inherit the same
 * ACE and that the ACE will be propagated indefinitely into files and
 * sub-directories of directories, etc.
 *
 * If @old is true, create an NT4 style SDS entry.  If @old is false, create a
 * Win2k+ style SDS entry.  The difference is that NT4 did not have the same
 * ACE inheritance rules so we modify the security descriptor to suit.
 */
static void ntfs_default_sds_entry_init(SDS_ENTRY *sds, BOOL dir, BOOL old)
{
	SECURITY_DESCRIPTOR_RELATIVE *sd;
	ACL *dacl;
	ACCESS_ALLOWED_ACE *ace;
	SID *sid;
	u32 sd_len;

	sd = &sds->sd;
	sd->revision = SECURITY_DESCRIPTOR_REVISION;
	sd->control = SE_SELF_RELATIVE | SE_DACL_PROTECTED |
			SE_DACL_AUTO_INHERITED | SE_DACL_PRESENT;
	if (old)
		sd->control &= ~(SE_DACL_PROTECTED | SE_DACL_AUTO_INHERITED);
	sd->dacl = const_cpu_to_le32(sizeof(SECURITY_DESCRIPTOR_RELATIVE));
	/*
	 * Create a discretionary ACL containing a single ACE granting all
	 * access to the universally well-known SID WORLD_SID (S-1-1-0), i.e.
	 * the "Everyone" SID.
	 */
	dacl = (ACL*)((u8*)sd + le32_to_cpu(sd->dacl));
	dacl->revision = ACL_REVISION;
	dacl->ace_count = const_cpu_to_le16(1);
	ace = (ACCESS_ALLOWED_ACE*)((u8*)dacl + sizeof(ACL));
	ace->type = ACCESS_ALLOWED_ACE_TYPE;
	/*
	 * If this is a directory, we want the ACE to be inherited both to the
	 * directory itself and to its sub-directories and files as well as to
	 * the sub-directories and files of the sub-directories, etc.
	 */
	ace->flags = dir ? (CONTAINER_INHERIT_ACE | OBJECT_INHERIT_ACE) : 0;
	ace->size = const_cpu_to_le16(sizeof(ACCESS_ALLOWED_ACE));
	ace->mask = STANDARD_RIGHTS_ALL | FILE_WRITE_ATTRIBUTES |
			FILE_READ_ATTRIBUTES | FILE_DELETE_CHILD |
			FILE_EXECUTE | FILE_WRITE_EA | FILE_READ_EA |
			FILE_APPEND_DATA | FILE_WRITE_DATA | FILE_READ_DATA;
	sid = &ace->sid;
	sid->revision = SID_REVISION;
	sid->sub_authority_count = 1;
	/* SECURITY_WORLD_SID_AUTHORITY */
	sid->identifier_authority.value[0] = 0;
	sid->identifier_authority.value[1] = 0;
	sid->identifier_authority.value[2] = 0;
	sid->identifier_authority.value[3] = 0;
	sid->identifier_authority.value[4] = 0;
	sid->identifier_authority.value[5] = 1;
	sid->sub_authority[0] = SECURITY_WORLD_RID;
	dacl->size = const_cpu_to_le16(sizeof(ACL) + le16_to_cpu(ace->size));
	/* The owner of the file/directory is "Everyone". */
	sd->owner = cpu_to_le32(le32_to_cpu(sd->dacl) +
			le16_to_cpu(dacl->size));
	memcpy((u8*)sd + le32_to_cpu(sd->owner), sid, sizeof(SID));
	/* The group of the file/directory is "Everyone", too. */
	sd->group = cpu_to_le32(le32_to_cpu(sd->owner) + sizeof(SID));
	memcpy((u8*)sd + le32_to_cpu(sd->group), sid, sizeof(SID));
	sd_len = le32_to_cpu(sd->group) + sizeof(SID);
	sds->hash = ntfs_security_hash(sd, sd_len);
	sds->length = cpu_to_le32(sizeof(SDS_ENTRY_HEADER) + sd_len);
}

/**
 * ntfs_default_sds_entries_init - set up the default SDS entries
 *
 * Set up one of the default SDS entries for the ntfs driver.
 *
 * Return 0 on success and ENOMEM if not enough memory was available to
 * allocate the buffer needed for storing the default SDS entries.
 */
errno_t ntfs_default_sds_entries_init(void)
{
	SDS_ENTRY *sds;

	ntfs_debug("Entering.");
	/*
	 * 0x60 is the size of each SDS entry we create below, aligned to the
	 * next 16-byte boundary.  The actual size is 0x5c bytes.  We hard code
	 * this here as we otherwise would not know the size until we have
	 * generated the SDS entry.
	 */
	sds = OSMalloc(0x60 * 4, ntfs_malloc_tag);
	if (!sds) {
		ntfs_error(NULL, "Failed to allocate memory for the default "
				"$Secure/$DATA/$SDS entries.");
		return ENOMEM;
	}
	ntfs_file_sds_entry = sds;
	ntfs_default_sds_entry_init(sds, FALSE, FALSE);
	sds = (SDS_ENTRY*)((u8*)sds + ((le32_to_cpu(sds->length) + 15) & ~15));
	ntfs_dir_sds_entry = sds;
	ntfs_default_sds_entry_init(sds, TRUE, FALSE);
	sds = (SDS_ENTRY*)((u8*)sds + ((le32_to_cpu(sds->length) + 15) & ~15));
	ntfs_file_sds_entry_old = sds;
	ntfs_default_sds_entry_init(sds, FALSE, TRUE);
	sds = (SDS_ENTRY*)((u8*)sds + ((le32_to_cpu(sds->length) + 15) & ~15));
	ntfs_dir_sds_entry_old = sds;
	ntfs_default_sds_entry_init(sds, TRUE, TRUE);
	ntfs_debug("Done.");
	return 0;
}

/**
 * ntfs_next_security_id_init - determine the next security_id to use
 * @vol:		volume for which to determine the next security_id
 * @next_security_id:	destination in which to return the next security_id
 *
 * Scan the $SII index of the $Secure system file and determine the security_id
 * to use the next time a new security descriptor is added to $Secure.
 *
 * Return the next security_id in *@next_security_id.
 *
 * Return 0 on success and errno on error in which case *@next_security_id is
 * undefined.
 */
errno_t ntfs_next_security_id_init(ntfs_volume *vol, le32 *next_security_id)
{
	VCN vcn, old_vcn;
	ntfs_inode *base_ni, *ni;
	MFT_RECORD *m;
	INDEX_ROOT *ir;
	INDEX_ENTRY *ie, *prev_ie;
	INDEX_ALLOCATION *ia;
	upl_t upl;
	upl_page_info_array_t pl;
	u8 *index_end, *kaddr;
	ntfs_attr_search_ctx *actx;
	errno_t err;

	ntfs_debug("Entering.");
	base_ni = vol->secure_ni;
	if (!base_ni)
		panic("%s(): !vol->secure_ni\n", __FUNCTION__);
	ni = vol->secure_sii_ni;
	if (!ni)
		panic("%s(): !vol->secure_sii_ni\n", __FUNCTION__);
	err = vnode_getwithref(ni->vn);
	if (err) {
		ntfs_error(vol->mp, "Failed to get vnode for "
				"$Secure/$INDEX_ALLOCATION/$SII.");
		return err;
	}
	lck_rw_lock_shared(&ni->lock);
	/* Get hold of the mft record for $Secure. */
	err = ntfs_mft_record_map(base_ni, &m);
	if (err) {
		ntfs_error(vol->mp, "Failed to map mft record of $Secure "
				"(error %d).", err);
		m = NULL;
		actx = NULL;
		goto err;
	}
	actx = ntfs_attr_search_ctx_get(base_ni, m);
	if (!actx) {
		err = ENOMEM;
		goto err;
	}
	/* Find the index root attribute in the mft record. */
	err = ntfs_attr_lookup(AT_INDEX_ROOT, ni->name, ni->name_len, 0, NULL,
			0, actx);
	if (err) {
		if (err == ENOENT) {
			ntfs_error(vol->mp, "$SII index root attribute "
					"missing in $Secure.");
			err = EIO;
		}
		goto err;
	}
	/*
	 * Get to the index root value (it has been verified when the inode was
	 * read in ntfs_{,index_}inode_read).
	 */
	ir = (INDEX_ROOT*)((u8*)actx->a + le16_to_cpu(actx->a->value_offset));
	index_end = (u8*)&ir->index + le32_to_cpu(ir->index.index_length);
	/* The first index entry. */
	ie = (INDEX_ENTRY*)((u8*)&ir->index +
			le32_to_cpu(ir->index.entries_offset));
	/*
	 * Loop until we exceed valid memory (corruption case) or until we
	 * reach the last entry, always storing the previous entry so that when
	 * we reach the last entry we can go back one to find the last
	 * security_id.
	 */
	prev_ie = NULL;
	for (;; prev_ie = ie, ie = (INDEX_ENTRY*)((u8*)ie +
			le16_to_cpu(ie->length))) {
		/* Bounds checks. */
		if ((u8*)ie < (u8*)&ir->index || (u8*)ie +
				sizeof(INDEX_ENTRY_HEADER) > index_end ||
				(u8*)ie + le16_to_cpu(ie->length) > index_end ||
				(u32)sizeof(INDEX_ENTRY_HEADER) +
				le16_to_cpu(ie->key_length) >
				le16_to_cpu(ie->length))
			goto idx_err;
		/*
		 * The last entry cannot contain a key.  It can however contain
		 * a pointer to a child node in the B+tree so we just break out.
		 */
		if (ie->flags & INDEX_ENTRY_END)
			break;
		/* Further bounds checks. */
		if ((u32)sizeof(INDEX_ENTRY_HEADER) +
				le16_to_cpu(ie->key_length) >
				le16_to_cpu(ie->data_offset) ||
				(u32)le16_to_cpu(ie->data_offset) +
				le16_to_cpu(ie->data_length) >
				le16_to_cpu(ie->length))
			goto idx_err;
	}
	/*
	 * We have finished with this index.  Check for the presence of a child
	 * node and if not present, we have the last security_id in @prev_ie,
	 * or if that is NULL there are no security_ids on the volume so just
	 * start at 0x100.
	 */
	if (!(ie->flags & INDEX_ENTRY_NODE)) {
		if (prev_ie)
			*next_security_id = cpu_to_le32(le32_to_cpu(
					prev_ie->key.sii.security_id) + 1);
		else
			*next_security_id = const_cpu_to_le32(0x100);
		ntfs_attr_search_ctx_put(actx);
		ntfs_mft_record_unmap(base_ni);
		lck_rw_unlock_shared(&ni->lock);
		(void)vnode_put(ni->vn);
		ntfs_debug("Found next security_id 0x%x in index root.",
				(unsigned)le32_to_cpu(*next_security_id));
		return 0;
	} /* Child node present, descend into it. */
	/* Consistency check: Verify that an index allocation exists. */
	if (!NInoIndexAllocPresent(ni)) {
		ntfs_error(vol->mp, "No index allocation attribute but index "
				"entry requires one.  $Secure is corrupt or "
				"driver bug.");
		goto err;
	}
	/* Get the starting vcn of the index_block holding the child node. */
	vcn = sle64_to_cpup((sle64*)((u8*)ie + le16_to_cpu(ie->length) - 8));
	/*
	 * We are done with the index root and the mft record.  Release them,
	 * otherwise we would deadlock with ntfs_page_map().
	 */
	ntfs_attr_search_ctx_put(actx);
	ntfs_mft_record_unmap(base_ni);
	m = NULL;
	actx = NULL;
descend_into_child_node:
	/*
	 * Convert vcn to byte offset in the index allocation attribute and map
	 * the corresponding page.
	 */
	err = ntfs_page_map(ni, (vcn << ni->vcn_size_shift) &
			~PAGE_MASK_64, &upl, &pl, &kaddr, FALSE);
	if (err) {
		ntfs_error(vol->mp, "Failed to map index page, error %d.",
				err);
		goto err;
	}
fast_descend_into_child_node:
	/* Get to the index allocation block. */
	ia = (INDEX_ALLOCATION*)(kaddr + ((vcn << ni->vcn_size_shift) &
			PAGE_MASK));
	/* Bounds checks. */
	if ((u8*)ia < kaddr || (u8*)ia > kaddr + PAGE_SIZE) {
		ntfs_error(vol->mp, "Out of bounds check failed.  $Secure is "
				"corrupt or driver bug.");
		goto page_err;
	}
	/* Catch multi sector transfer fixup errors. */
	if (!ntfs_is_indx_record(ia->magic)) {
		ntfs_error(vol->mp, "Index record with vcn 0x%llx is "
				"corrupt.  $Secure is corrupt.  Run chkdsk.",
				(unsigned long long)vcn);
		goto page_err;
	}
	if (sle64_to_cpu(ia->index_block_vcn) != vcn) {
		ntfs_error(vol->mp, "Actual VCN (0x%llx) of index buffer is "
				"different from expected VCN (0x%llx).  "
				"$Secure is corrupt or driver bug.",
				(unsigned long long)
				sle64_to_cpu(ia->index_block_vcn),
				(unsigned long long)vcn);
		goto page_err;
	}
	if (offsetof(INDEX_BLOCK, index) +
			le32_to_cpu(ia->index.allocated_size) !=
			ni->block_size) {
		ntfs_error(vol->mp, "Index buffer (VCN 0x%llx) of $Secure has "
				"a size (%u) differing from the index "
				"specified size (%u).  $Secure is corrupt or "
				"driver bug.", (unsigned long long)vcn,
				(unsigned)(offsetof(INDEX_BLOCK, index) +
				le32_to_cpu(ia->index.allocated_size)),
				(unsigned)ni->block_size);
		goto page_err;
	}
	index_end = (u8*)ia + ni->block_size;
	if (index_end > kaddr + PAGE_SIZE) {
		ntfs_error(vol->mp, "Index buffer (VCN 0x%llx) of $Secure "
				"crosses page boundary.  Impossible!  Cannot "
				"access!  This is probably a bug in the "
				"driver.", (unsigned long long)vcn);
		goto page_err;
	}
	index_end = (u8*)&ia->index + le32_to_cpu(ia->index.index_length);
	if (index_end > (u8*)ia + ni->block_size) {
		ntfs_error(vol->mp, "Size of index buffer (VCN 0x%llx) of "
				"$Secure exceeds maximum size.",
				(unsigned long long)vcn);
		goto page_err;
	}
	/* The first index entry. */
	ie = (INDEX_ENTRY*)((u8*)&ia->index +
			le32_to_cpu(ia->index.entries_offset));
	/*
	 * Iterate similar to above big loop but applied to index buffer, thus
	 * loop until we exceed valid memory (corruption case) or until we
	 * reach the last entry, always storing the previous entry so that when
	 * we reach the last entry we can go back one to find the last
	 * security_id.
	 */
	prev_ie = NULL;
	for (;; prev_ie = ie, ie = (INDEX_ENTRY*)((u8*)ie +
			le16_to_cpu(ie->length))) {
		/* Bounds checks. */
		if ((u8*)ie < (u8*)&ia->index || (u8*)ie +
				sizeof(INDEX_ENTRY_HEADER) > index_end ||
				(u8*)ie + le16_to_cpu(ie->length) > index_end ||
				(u32)sizeof(INDEX_ENTRY_HEADER) +
				le16_to_cpu(ie->key_length) >
				le16_to_cpu(ie->length)) {
			ntfs_error(vol->mp, "Index entry out of bounds in "
					"$Secure.");
			goto page_err;
		}
		/*
		 * The last entry cannot contain a key.  It can however contain
		 * a pointer to a child node in the B+tree so we just break out.
		 */
		if (ie->flags & INDEX_ENTRY_END)
			break;
		/* Further bounds checks. */
		if ((u32)sizeof(INDEX_ENTRY_HEADER) +
				le16_to_cpu(ie->key_length) >
				le16_to_cpu(ie->data_offset) ||
				(u32)le16_to_cpu(ie->data_offset) +
				le16_to_cpu(ie->data_length) >
				le16_to_cpu(ie->length)) {
			ntfs_error(vol->mp, "Index entry out of bounds in "
					"$Secure.");
			goto page_err;
		}
	}
	/*
	 * We have finished with this index buffer.  Check for the presence of
	 * a child node and if not present, we have the last security_id in
	 * @prev_ie, or if that is NULL there are no security_ids on the volume
	 * so just start at 0x100.
	 */
	if (!(ie->flags & INDEX_ENTRY_NODE)) {
		if (prev_ie)
			*next_security_id = cpu_to_le32(le32_to_cpu(
					prev_ie->key.sii.security_id) + 1);
		else
			*next_security_id = const_cpu_to_le32(0x100);
		ntfs_page_unmap(ni, upl, pl, FALSE);
		lck_rw_unlock_shared(&ni->lock);
		(void)vnode_put(ni->vn);
		ntfs_debug("Found next security_id 0x%x in index allocation.",
				le32_to_cpu(*next_security_id));
		return 0;
	}
	if ((ia->index.flags & NODE_MASK) == LEAF_NODE) {
		ntfs_error(vol->mp, "Index entry with child node found in a "
				"leaf node in $Secure.");
		goto page_err;
	}
	/* Child node present, descend into it. */
	old_vcn = vcn;
	vcn = sle64_to_cpup((sle64*)((u8*)ie + le16_to_cpu(ie->length) - 8));
	if (vcn >= 0) {
		/*
		 * If @vcn is in the same page cache page as @old_vcn we
		 * recycle the mapped page.
		 */
		if (old_vcn << ni->vcn_size_shift >> PAGE_SHIFT ==
				vcn << ni->vcn_size_shift >> PAGE_SHIFT)
			goto fast_descend_into_child_node;
		ntfs_page_unmap(ni, upl, pl, FALSE);
		goto descend_into_child_node;
	}
	ntfs_error(vol->mp, "Negative child node vcn in $Secure.");
page_err:
	ntfs_page_unmap(ni, upl, pl, FALSE);
err:
	if (!err)
		err = EIO;
	if (actx)
		ntfs_attr_search_ctx_put(actx);
	if (m)
		ntfs_mft_record_unmap(base_ni);
	lck_rw_unlock_shared(&ni->lock);
	(void)vnode_put(ni->vn);
	return err;
idx_err:
	ntfs_error(vol->mp, "Corrupt index.  Aborting lookup.");
	goto err;
}

errno_t ntfs_default_security_id_init(ntfs_volume *vol, struct vnode_attr *va)
{
	le32 security_id;

	ntfs_debug("Entering.");
	lck_rw_lock_exclusive(&vol->secure_lock);
	lck_spin_lock(&vol->security_id_lock);
	if (va->va_type == VDIR)
		security_id = vol->default_dir_security_id;
	else
		security_id = vol->default_file_security_id;
	lck_spin_unlock(&vol->security_id_lock);
	if (security_id) {
		/* Someone else initialized the default security_id for us. */
		lck_rw_unlock_exclusive(&vol->secure_lock);
		ntfs_debug("Done (lost race).");
		return 0;
	}
	// TODO: Look for our security descriptor appropriate to the volume
	// version.  If the security descriptor is not found, add it to $Secure
	// and set the volume default security_id to point to it.
	lck_rw_unlock_exclusive(&vol->secure_lock);
	ntfs_debug("Failed (not implemented yet).");
	return ENOTSUP;
}
