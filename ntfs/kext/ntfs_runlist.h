/*
 * ntfs_runlist.h - Defines for runlist handling in the NTFS kernel driver.
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

#ifndef _OSX_NTFS_RUNLIST_H
#define _OSX_NTFS_RUNLIST_H

#include <kern/locks.h>

#include "ntfs_types.h"

/* These definitions need to be before any of the other ntfs_*.h includes. */

/**
 * ntfs_rl_element - in memory vcn to lcn mapping array element
 * @vcn:	starting vcn of the current array element
 * @lcn:	starting lcn of the current array element
 * @length:	length in clusters of the current array element
 *
 * The last vcn (in fact the last vcn + 1) is reached when length == 0.
 *
 * When lcn == -1 this means that the count vcns starting at vcn are not
 * physically allocated (i.e. this is a hole / data is sparse).
 */
typedef struct { /* In memory vcn to lcn mapping structure element. */
	VCN vcn;	/* vcn = Starting virtual cluster number. */
	LCN lcn;	/* lcn = Starting logical cluster number. */
	s64 length;	/* Run length in clusters. */
} ntfs_rl_element;

/**
 * ntfs_runlist - in memory vcn to lcn mapping array including a read/write lock
 * @rl:		pointer to an array of runlist elements
 * @elements:	number of runlist elements in runlist
 * @alloc:	number of bytes allocated for this runlist in memory
 * @lock:	read/write lock for serializing access to @rl
 *
 * This is the runlist structure.  It describes the mapping from file offsets
 * (described as virtual cluster numbers (VCNs)) to on-disk offsets (described
 * as logical cluster numbers (LCNs)).
 *
 * The runlist is made up of an array of runlist elements where each element
 * contains the VCN at which that run starts, the corresponding physical
 * location, i.e. the LCN, at which that run starts and the length in clusters
 * of this run.
 *
 * When doing lookups in the runlist it must be locked for either reading or
 * writing.
 *
 * When modifying the runlist in memory it must be locked for writing.
 *
 * Note that the complete runlist can be spread out over several NTFS
 * attribute fragments in which case only one or only a few parts of the
 * runlist may be mapped at any point in  time.  In this case the regions that
 * are not mapped have placeholders with an LCN of LCN_RL_NOT_MAPPED.  In this
 * case a lookup can lead to a readlocked runlist being writelocked because the
 * in-memory runlist will need updating with the mapped in runlist fragment.
 *
 * Another special value is LCN_HOLE which means that the clusters are not
 * allocated on disk, i.e. this run is sparse, i.e. it is a hole on the
 * attribute.  Thus on reading you just need to interpret the whole run as
 * containing zeroes and on writing you need to allocate real clusters and then
 * write to them.
 *
 * For other special values of LCNs please see below, where the enum
 * LCN_SPECIAL_VALUES is defined.
 */
typedef struct {
	ntfs_rl_element *rl;
	u32 elements;
	u32 alloc;
	lck_rw_t lock;
} ntfs_runlist;

#include "ntfs.h"
#include "ntfs_layout.h"
#include "ntfs_volume.h"

/* Runlist allocations happen in multiples of this value in bytes. */
#define NTFS_RL_ALLOC_BLOCK 1024

static inline void ntfs_rl_init(ntfs_runlist *rl)
{
	rl->rl = NULL;
	rl->elements = 0;
	rl->alloc = 0;
	lck_rw_init(&rl->lock, ntfs_lock_grp, ntfs_lock_attr);
}

static inline void ntfs_rl_deinit(ntfs_runlist *rl)
{
	lck_rw_destroy(&rl->lock, ntfs_lock_grp);
}

/**
 * LCN_SPECIAL_VALUES - special values for lcns inside a runlist
 *
 * LCN_HOLE:		run is not allocated on disk, i.e. it is a hole
 * LCN_RL_NOT_MAPPED:	runlist for region starting at current vcn is not
 * 			mapped into memory at the moment, thus it will need to
 * 			be read in before the real lcn can be determined
 * LCN_ENOENT:		the current vcn is the last vcn (actually the last vcn
 * 			+ 1) of the attribute
 * LCN_ENOMEM:		this is only returned in the case of an out of memory
 * 			condition whilst trying to map a runlist fragment for
 * 			example
 * LCN_EIO:		an i/o error occurred when reading/writing the runlist
 */
typedef enum {
	LCN_HOLE		= -1,	/* Keep this as highest value or die! */
	LCN_RL_NOT_MAPPED	= -2,
	LCN_ENOENT		= -3,
	LCN_ENOMEM		= -4,
	LCN_EIO			= -5,
} LCN_SPECIAL_VALUES;

__private_extern__ errno_t ntfs_rl_merge(ntfs_runlist *dst_runlist,
		ntfs_runlist *src_runlist);

__private_extern__ errno_t ntfs_mapping_pairs_decompress(ntfs_volume *vol,
		const ATTR_RECORD *a, ntfs_runlist *runlist);

__private_extern__ LCN ntfs_rl_vcn_to_lcn(const ntfs_rl_element *rl,
		const VCN vcn, s64 *clusters);

__private_extern__ errno_t ntfs_rl_read(ntfs_volume *vol, ntfs_runlist *rl,
		u8 *dst, const s64 size, const s64 initialized_size);

#endif /* !_OSX_NTFS_RUNLIST_H */
