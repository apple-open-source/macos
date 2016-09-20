/*
 * Copyright (c) 2014 Apple Inc. All rights reserved.
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

#ifndef HFS_KEY_ROLL_H_
#define HFS_KEY_ROLL_H_

#include <stdbool.h>

#include "hfs_format.h"
#include "hfs_fsctl.h"
#include "hfs_cprotect.h"

/*
 * This structure contains the in-memory information required for key
 * rolling.  It is referenced via a pointer in the cprotect structure.
 */
typedef struct hfs_cp_key_roll_ctx {
#if DEBUG
	uint32_t					ckr_magic1;
#endif
	// This indicates where we are with key rolling
	off_rsrc_t					ckr_off_rsrc;

	// When set, indicates we are currently rolling a chunk
	bool						ckr_busy : 1;

	/*
	 * This represents the tentative reservation---blocks that we have set
	 * aside for key rolling but can be claimed back by the allocation code
	 * if necessary.
	 */
	struct rl_entry			   *ckr_tentative_reservation;

	// This usually indicates the end of the last block we rolled
	uint32_t					ckr_preferred_next_block;

	// The current extent that we're rolling to
	HFSPlusExtentDescriptor	    ckr_roll_extent;

	// The new keys -- variable length
	cp_key_pair_t				ckr_keys;
} hfs_cp_key_roll_ctx_t;

errno_t hfs_key_roll_check(struct cnode *cp, bool have_excl_trunc_lock);
errno_t hfs_key_roll_op(vfs_context_t ctx, vnode_t vp, hfs_key_roll_args_t *args);
errno_t hfs_key_roll_start(struct cnode *cp);
errno_t hfs_key_roll_up_to(vfs_context_t vfs_ctx, vnode_t vp, off_rsrc_t up_to);
errno_t hfs_key_roll_step(vfs_context_t vfs_ctx, vnode_t vp, off_rsrc_t up_to);
hfs_cp_key_roll_ctx_t *hfs_key_roll_ctx_alloc(const hfs_cp_key_roll_ctx_t *old,
											  uint16_t pers_key_len,
											  uint16_t cached_key_len,
											  cp_key_pair_t **pcpkp);
void hfs_release_key_roll_ctx(struct hfsmount *hfsmp, cprotect_t cpr);
bool hfs_is_key_rolling(cnode_t *cp);

#endif // HFS_KEY_ROLL_H_
