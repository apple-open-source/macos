/*
 * Copyright (c) 2000-2013 Apple Inc. All rights reserved.
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
#ifndef _VM_VM_COMPRESSOR_BACKING_STORE_H_
#define _VM_VM_COMPRESSOR_BACKING_STORE_H_

#ifdef MACH_KERNEL_PRIVATE

#include <kern/kern_types.h>
#include <kern/locks.h>
#include <vm/vm_kern.h>
#include <mach/kern_return.h>
#include <kern/queue.h>
#include <vm/vm_pageout.h>
#include <vm/vm_protos.h>
#include <vm/vm_compressor_xnu.h>
#include <libkern/crypto/aes.h>
#include <kern/host_statistics.h>
#include <vm/vm_pageout_xnu.h>
#include <vm/vm_compressor_backing_store_xnu.h>

#if !XNU_TARGET_OS_OSX

#define MIN_SWAP_FILE_SIZE              (64 * 1024 * 1024ULL)

#define MAX_SWAP_FILE_SIZE              (128 * 1024 * 1024ULL)

#else /* !XNU_TARGET_OS_OSX */

#define MIN_SWAP_FILE_SIZE              (256 * 1024 * 1024ULL)

#define MAX_SWAP_FILE_SIZE              (1 * 1024 * 1024 * 1024ULL)

#endif /* !XNU_TARGET_OS_OSX */

#if defined(XNU_TARGET_OS_OSX)
#define SWAP_VOLUME_NAME        "/System/Volumes"
#define SWAP_FILE_NAME          SWAP_VOLUME_NAME "/VM/swapfile"
#else
#define SWAP_VOLUME_NAME        "/private/var"
#define SWAP_FILE_NAME          SWAP_VOLUME_NAME "/vm/swapfile"
#endif

#define SWAPFILENAME_LEN        (int)(strlen(SWAP_FILE_NAME))


#define SWAP_SLOT_MASK          0x1FFFFFFFF
#define SWAP_DEVICE_SHIFT       33

extern int              vm_num_swap_files;
extern uint64_t         vm_swap_volume_capacity;

struct swapfile;

boolean_t vm_swap_create_file(void);


struct swapout_io_completion {
	int          swp_io_busy;
	int          swp_io_done;
	int          swp_io_error;

	uint32_t     swp_c_size;
	c_segment_t  swp_c_seg;

	struct swapfile *swp_swf;
	uint64_t        swp_f_offset;

	struct upl_io_completion swp_upl_ctx;
};
void vm_swapout_iodone(void *, int);


kern_return_t vm_swap_put_finish(struct swapfile *, uint64_t *, int, boolean_t);
kern_return_t vm_swap_put(vm_offset_t, uint64_t*, uint32_t, c_segment_t, struct swapout_io_completion *);

void vm_swap_flush(void);
void vm_swap_reclaim(void);
void vm_swap_encrypt(c_segment_t);

uint64_t vm_swap_get_used_space(void);
uint64_t vm_swap_get_max_configured_space(void);
void vm_swap_reset_max_segs_tracking(uint64_t *alloced_max, uint64_t *used_max);

extern __startup_func void vm_compressor_swap_init_swap_file_limit(void);




#endif /* MACH_KERNEL_PRIVATE */

struct vnode;

extern void vm_swapfile_open(const char *path, struct vnode **vp);
extern void vm_swapfile_close(uint64_t path, struct vnode *vp);
extern int vm_swapfile_preallocate(struct vnode *vp, uint64_t *size, boolean_t *pin);
extern uint64_t vm_swapfile_get_blksize(struct vnode *vp);
extern uint64_t vm_swapfile_get_transfer_size(struct vnode *vp);
extern int vm_swapfile_io(struct vnode *vp, uint64_t offset, uint64_t start, int npages, int flags, void *upl_ctx);
extern int vm_record_file_write(struct vnode *vp, uint64_t offset, char *buf, int size);
int vm_swap_vol_get_capacity(const char *volume_name, uint64_t *capacity);
#if CONFIG_FREEZE
int vm_swap_vol_get_budget(struct vnode* vp, uint64_t *freeze_daily_budget);
#endif /* CONFIG_FREEZE */


#endif /* _VM_VM_COMPRESSOR_BACKING_STORE_H_ */
