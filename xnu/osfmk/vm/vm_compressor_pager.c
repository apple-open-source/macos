/*
 * Copyright (c) 2019-2020 Apple Inc. All rights reserved.
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
/*
 * @OSF_COPYRIGHT@
 */
/*
 * Mach Operating System
 * Copyright (c) 1991,1990,1989 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 */

/*
 *	Compressor Pager.
 *		Memory Object Management.
 */

#include <kern/host_statistics.h>
#include <kern/kalloc.h>
#include <kern/ipc_kobject.h>

#include <machine/atomic.h>

#include <mach/memory_object_control.h>
#include <mach/memory_object_types.h>
#include <mach/upl.h>

#include <vm/memory_object.h>
#include <vm/vm_compressor_pager_internal.h>
#include <vm/vm_external.h>
#include <vm/vm_fault.h>
#include <vm/vm_pageout.h>
#include <vm/vm_protos_internal.h>
#include <vm/vm_object_internal.h>

#include <sys/kdebug_triage.h>

/* memory_object interfaces */
void compressor_memory_object_reference(memory_object_t mem_obj);
void compressor_memory_object_deallocate(memory_object_t mem_obj);
kern_return_t compressor_memory_object_init(
	memory_object_t         mem_obj,
	memory_object_control_t control,
	memory_object_cluster_size_t pager_page_size);
kern_return_t compressor_memory_object_terminate(memory_object_t mem_obj);
kern_return_t compressor_memory_object_data_request(
	memory_object_t         mem_obj,
	memory_object_offset_t  offset,
	memory_object_cluster_size_t            length,
	__unused vm_prot_t      protection_required,
	memory_object_fault_info_t      fault_info);
kern_return_t compressor_memory_object_data_return(
	memory_object_t         mem_obj,
	memory_object_offset_t  offset,
	memory_object_cluster_size_t                    size,
	__unused memory_object_offset_t *resid_offset,
	__unused int            *io_error,
	__unused boolean_t      dirty,
	__unused boolean_t      kernel_copy,
	__unused int    upl_flags);
kern_return_t compressor_memory_object_data_initialize(
	memory_object_t         mem_obj,
	memory_object_offset_t  offset,
	memory_object_cluster_size_t            size);
kern_return_t compressor_memory_object_map(
	__unused memory_object_t        mem_obj,
	__unused vm_prot_t              prot);
kern_return_t compressor_memory_object_last_unmap(memory_object_t mem_obj);

const struct memory_object_pager_ops compressor_pager_ops = {
	.memory_object_reference = compressor_memory_object_reference,
	.memory_object_deallocate = compressor_memory_object_deallocate,
	.memory_object_init = compressor_memory_object_init,
	.memory_object_terminate = compressor_memory_object_terminate,
	.memory_object_data_request = compressor_memory_object_data_request,
	.memory_object_data_return = compressor_memory_object_data_return,
	.memory_object_data_initialize = compressor_memory_object_data_initialize,
	.memory_object_map = compressor_memory_object_map,
	.memory_object_last_unmap = compressor_memory_object_last_unmap,
	.memory_object_backing_object = NULL,
	.memory_object_pager_name = "compressor pager"
};

/* internal data structures */

struct {
	uint64_t        data_returns;
	uint64_t        data_requests;
	uint64_t        put;
	uint64_t        get;
	uint64_t        state_clr;
	uint64_t        state_get;
	uint64_t        transfer;
} compressor_pager_stats;

typedef int compressor_slot_t; /* stand-in for c_slot_mapping */

typedef struct compressor_pager {
	/* mandatory generic header */
	struct memory_object cpgr_hdr;

	/* pager-specific data */
	lck_mtx_t                       cpgr_lock;
#if MEMORY_OBJECT_HAS_REFCOUNT
#define cpgr_references                 cpgr_hdr.mo_ref
#else
	os_ref_atomic_t                 cpgr_references;
#endif
	unsigned int                    cpgr_num_slots;
	unsigned int                    cpgr_num_slots_occupied;
	union {
		compressor_slot_t       cpgr_eslots[2]; /* embedded slots */
		compressor_slot_t       *cpgr_dslots;   /* direct slots */
		compressor_slot_t       **cpgr_islots;  /* indirect slots */
	} cpgr_slots;
} *compressor_pager_t;

#define compressor_pager_lookup(_mem_obj_, _cpgr_)                      \
	MACRO_BEGIN                                                     \
	if (_mem_obj_ == NULL ||                                        \
	    _mem_obj_->mo_pager_ops != &compressor_pager_ops) {         \
	        _cpgr_ = NULL;                                          \
	} else {                                                        \
	        _cpgr_ = (compressor_pager_t) _mem_obj_;                \
	}                                                               \
	MACRO_END

/* embedded slot pointers in compressor_pager get packed, so VA restricted */
static ZONE_DEFINE_TYPE(compressor_pager_zone, "compressor_pager",
    struct compressor_pager, ZC_NOENCRYPT | ZC_VM);

LCK_GRP_DECLARE(compressor_pager_lck_grp, "compressor_pager");

#define compressor_pager_lock(_cpgr_) \
	lck_mtx_lock(&(_cpgr_)->cpgr_lock)
#define compressor_pager_unlock(_cpgr_) \
	lck_mtx_unlock(&(_cpgr_)->cpgr_lock)
#define compressor_pager_lock_init(_cpgr_) \
	lck_mtx_init(&(_cpgr_)->cpgr_lock, &compressor_pager_lck_grp, LCK_ATTR_NULL)
#define compressor_pager_lock_destroy(_cpgr_) \
	lck_mtx_destroy(&(_cpgr_)->cpgr_lock, &compressor_pager_lck_grp)

#define COMPRESSOR_SLOTS_CHUNK_SIZE     (512)
#define COMPRESSOR_SLOTS_PER_CHUNK      (COMPRESSOR_SLOTS_CHUNK_SIZE / sizeof (compressor_slot_t))

/* forward declarations */
unsigned int compressor_pager_slots_chunk_free(compressor_slot_t *chunk,
    int num_slots,
    vm_compressor_options_t flags,
    int *failures);
void compressor_pager_slot_lookup(
	compressor_pager_t      pager,
	boolean_t               do_alloc,
	memory_object_offset_t  offset,
	compressor_slot_t       **slot_pp);

#if     defined(__LP64__)

/* restricted VA zones for slots */

#define NUM_SLOTS_ZONES         3

static const size_t compressor_slots_zones_sizes[NUM_SLOTS_ZONES] = {
	16,
	64,
	COMPRESSOR_SLOTS_CHUNK_SIZE
};

static const char * compressor_slots_zones_names[NUM_SLOTS_ZONES] = {
	"compressor_slots.16",
	"compressor_slots.64",
	"compressor_slots.512"
};

static zone_t
    compressor_slots_zones[NUM_SLOTS_ZONES];

#endif /* defined(__LP64__) */

static void
zfree_slot_array(compressor_slot_t *slots, size_t size);
static compressor_slot_t *
zalloc_slot_array(size_t size, zalloc_flags_t);

static inline unsigned int
compressor_pager_num_chunks(
	compressor_pager_t      pager)
{
	unsigned int num_chunks;

	num_chunks = pager->cpgr_num_slots / COMPRESSOR_SLOTS_PER_CHUNK;
	if (num_chunks * COMPRESSOR_SLOTS_PER_CHUNK < pager->cpgr_num_slots) {
		num_chunks++;  /* do the equivalent of ceil() instead of trunc() for the above division */
	}
	return num_chunks;
}

kern_return_t
compressor_memory_object_init(
	memory_object_t         mem_obj,
	memory_object_control_t control,
	__unused memory_object_cluster_size_t pager_page_size)
{
	compressor_pager_t              pager;

	assert(pager_page_size == PAGE_SIZE);

	memory_object_control_reference(control);

	compressor_pager_lookup(mem_obj, pager);
	compressor_pager_lock(pager);

	if (pager->cpgr_hdr.mo_control != MEMORY_OBJECT_CONTROL_NULL) {
		panic("compressor_memory_object_init: bad request");
	}
	pager->cpgr_hdr.mo_control = control;

	compressor_pager_unlock(pager);

	return KERN_SUCCESS;
}

kern_return_t
compressor_memory_object_map(
	__unused memory_object_t        mem_obj,
	__unused vm_prot_t              prot)
{
	panic("compressor_memory_object_map");
	return KERN_FAILURE;
}

kern_return_t
compressor_memory_object_last_unmap(
	__unused memory_object_t        mem_obj)
{
	panic("compressor_memory_object_last_unmap");
	return KERN_FAILURE;
}

kern_return_t
compressor_memory_object_terminate(
	memory_object_t         mem_obj)
{
	memory_object_control_t control;
	compressor_pager_t      pager;

	/*
	 * control port is a receive right, not a send right.
	 */

	compressor_pager_lookup(mem_obj, pager);
	compressor_pager_lock(pager);

	/*
	 * After memory_object_terminate both memory_object_init
	 * and a no-senders notification are possible, so we need
	 * to clean up our reference to the memory_object_control
	 * to prepare for a new init.
	 */

	control = pager->cpgr_hdr.mo_control;
	pager->cpgr_hdr.mo_control = MEMORY_OBJECT_CONTROL_NULL;

	compressor_pager_unlock(pager);

	/*
	 * Now we deallocate our reference on the control.
	 */
	memory_object_control_deallocate(control);
	return KERN_SUCCESS;
}

void
compressor_memory_object_reference(
	memory_object_t         mem_obj)
{
	compressor_pager_t      pager;

	compressor_pager_lookup(mem_obj, pager);
	if (pager == NULL) {
		return;
	}

	compressor_pager_lock(pager);
	os_ref_retain_locked_raw(&pager->cpgr_references, NULL);
	compressor_pager_unlock(pager);
}

void
compressor_memory_object_deallocate(
	memory_object_t         mem_obj)
{
	compressor_pager_t      pager;
	unsigned int            num_slots_freed;

	/*
	 * Because we don't give out multiple first references
	 * for a memory object, there can't be a race
	 * between getting a deallocate call and creating
	 * a new reference for the object.
	 */

	compressor_pager_lookup(mem_obj, pager);
	if (pager == NULL) {
		return;
	}

	compressor_pager_lock(pager);
	if (os_ref_release_locked_raw(&pager->cpgr_references, NULL) > 0) {
		compressor_pager_unlock(pager);
		return;
	}

	/*
	 * We shouldn't get a deallocation call
	 * when the kernel has the object cached.
	 */
	if (pager->cpgr_hdr.mo_control != MEMORY_OBJECT_CONTROL_NULL) {
		panic("compressor_memory_object_deallocate(): bad request");
	}

	/*
	 * Unlock the pager (though there should be no one
	 * waiting for it).
	 */
	compressor_pager_unlock(pager);

	/* free the compressor slots */
	unsigned int num_chunks;
	unsigned int i;
	compressor_slot_t *chunk;

	num_chunks = compressor_pager_num_chunks(pager);
	if (num_chunks > 1) {
		/* we have an array of chunks */
		for (i = 0; i < num_chunks; i++) {
			chunk = pager->cpgr_slots.cpgr_islots[i];
			if (chunk != NULL) {
				num_slots_freed =
				    compressor_pager_slots_chunk_free(
					chunk,
					COMPRESSOR_SLOTS_PER_CHUNK,
					0,
					NULL);
				pager->cpgr_slots.cpgr_islots[i] = NULL;
				zfree_slot_array(chunk, COMPRESSOR_SLOTS_CHUNK_SIZE);
			}
		}
		kfree_type(compressor_slot_t *, num_chunks,
		    pager->cpgr_slots.cpgr_islots);
		pager->cpgr_slots.cpgr_islots = NULL;
	} else if (pager->cpgr_num_slots > 2) {
		chunk = pager->cpgr_slots.cpgr_dslots;
		num_slots_freed =
		    compressor_pager_slots_chunk_free(
			chunk,
			pager->cpgr_num_slots,
			0,
			NULL);
		pager->cpgr_slots.cpgr_dslots = NULL;
		zfree_slot_array(chunk,
		    (pager->cpgr_num_slots *
		    sizeof(pager->cpgr_slots.cpgr_dslots[0])));
	} else {
		chunk = &pager->cpgr_slots.cpgr_eslots[0];
		num_slots_freed =
		    compressor_pager_slots_chunk_free(
			chunk,
			pager->cpgr_num_slots,
			0,
			NULL);
	}

	compressor_pager_lock_destroy(pager);
	zfree(compressor_pager_zone, pager);
}

kern_return_t
compressor_memory_object_data_request(
	memory_object_t         mem_obj,
	memory_object_offset_t  offset,
	memory_object_cluster_size_t            length,
	__unused vm_prot_t      protection_required,
	__unused memory_object_fault_info_t     fault_info)
{
	compressor_pager_t      pager;
	kern_return_t           kr;
	compressor_slot_t       *slot_p;

	compressor_pager_stats.data_requests++;

	/*
	 * Request must be on a page boundary and a multiple of pages.
	 */
	if ((offset & PAGE_MASK) != 0 || (length & PAGE_MASK) != 0) {
		panic("compressor_memory_object_data_request(): bad alignment");
	}

	if ((uint32_t)(offset / PAGE_SIZE) != (offset / PAGE_SIZE)) {
		panic("%s: offset 0x%llx overflow",
		    __FUNCTION__, (uint64_t) offset);
		return KERN_FAILURE;
	}

	compressor_pager_lookup(mem_obj, pager);

	if (length == 0) {
		/* we're only querying the pager for this page */
	} else {
		panic("compressor: data_request");
	}

	/* find the compressor slot for that page */
	compressor_pager_slot_lookup(pager, FALSE, offset, &slot_p);

	if (offset / PAGE_SIZE >= pager->cpgr_num_slots) {
		/* out of range */
		kr = KERN_FAILURE;
	} else if (slot_p == NULL || *slot_p == 0) {
		/* compressor does not have this page */
		kr = KERN_FAILURE;
	} else {
		/* compressor does have this page */
		kr = KERN_SUCCESS;
	}
	return kr;
}

/*
 * memory_object_data_initialize: check whether we already have each page, and
 * write it if we do not.  The implementation is far from optimized, and
 * also assumes that the default_pager is single-threaded.
 */
/*  It is questionable whether or not a pager should decide what is relevant */
/* and what is not in data sent from the kernel.  Data initialize has been */
/* changed to copy back all data sent to it in preparation for its eventual */
/* merge with data return.  It is the kernel that should decide what pages */
/* to write back.  As of the writing of this note, this is indeed the case */
/* the kernel writes back one page at a time through this interface */

kern_return_t
compressor_memory_object_data_initialize(
	memory_object_t         mem_obj,
	memory_object_offset_t  offset,
	memory_object_cluster_size_t            size)
{
	compressor_pager_t      pager;
	memory_object_offset_t  cur_offset;

	compressor_pager_lookup(mem_obj, pager);
	compressor_pager_lock(pager);

	for (cur_offset = offset;
	    cur_offset < offset + size;
	    cur_offset += PAGE_SIZE) {
		panic("do a data_return() if slot for this page is empty");
	}

	compressor_pager_unlock(pager);

	return KERN_SUCCESS;
}


/*ARGSUSED*/
kern_return_t
compressor_memory_object_data_return(
	__unused memory_object_t                        mem_obj,
	__unused memory_object_offset_t         offset,
	__unused memory_object_cluster_size_t   size,
	__unused memory_object_offset_t *resid_offset,
	__unused int            *io_error,
	__unused boolean_t      dirty,
	__unused boolean_t      kernel_copy,
	__unused int            upl_flags)
{
	panic("compressor: data_return");
	return KERN_FAILURE;
}

/*
 * Routine:	default_pager_memory_object_create
 * Purpose:
 *      Handle requests for memory objects from the
 *      kernel.
 * Notes:
 *      Because we only give out the default memory
 *      manager port to the kernel, we don't have to
 *      be so paranoid about the contents.
 */
kern_return_t
compressor_memory_object_create(
	memory_object_size_t    new_size,
	memory_object_t         *new_mem_obj)
{
	compressor_pager_t      pager;
	unsigned int            num_chunks;

	if ((uint32_t)(new_size / PAGE_SIZE) != (new_size / PAGE_SIZE)) {
		/* 32-bit overflow for number of pages */
		panic("%s: size 0x%llx overflow",
		    __FUNCTION__, (uint64_t) new_size);
		return KERN_INVALID_ARGUMENT;
	}

	pager = zalloc_flags(compressor_pager_zone, Z_WAITOK | Z_NOFAIL);

	compressor_pager_lock_init(pager);
	os_ref_init_raw(&pager->cpgr_references, NULL);
	pager->cpgr_num_slots = (uint32_t)(new_size / PAGE_SIZE);
	pager->cpgr_num_slots_occupied = 0;

	num_chunks = compressor_pager_num_chunks(pager);
	if (num_chunks > 1) {
		/* islots points to an array of chunks pointer. every chunk has 512/sizeof(int)=128 slot_mapping */
		pager->cpgr_slots.cpgr_islots = kalloc_type(compressor_slot_t *,
		    num_chunks, Z_WAITOK | Z_ZERO);
	} else if (pager->cpgr_num_slots > 2) {
		pager->cpgr_slots.cpgr_dslots = zalloc_slot_array(pager->cpgr_num_slots *
		    sizeof(pager->cpgr_slots.cpgr_dslots[0]), Z_WAITOK | Z_ZERO);
	} else {
		pager->cpgr_slots.cpgr_eslots[0] = 0;
		pager->cpgr_slots.cpgr_eslots[1] = 0;
	}

	/*
	 * Set up associations between this memory object
	 * and this compressor_pager structure
	 */
	pager->cpgr_hdr.mo_ikot = IKOT_MEMORY_OBJECT;
	pager->cpgr_hdr.mo_pager_ops = &compressor_pager_ops;
	pager->cpgr_hdr.mo_control = MEMORY_OBJECT_CONTROL_NULL;

	*new_mem_obj = (memory_object_t) pager;
	return KERN_SUCCESS;
}


unsigned int
compressor_pager_slots_chunk_free(
	compressor_slot_t       *chunk,
	int                     num_slots,
	vm_compressor_options_t flags,
	int                     *failures)
{
	int i;
	int retval;
	unsigned int num_slots_freed;

	if (failures) {
		*failures = 0;
	}
	num_slots_freed = 0;
	for (i = 0; i < num_slots; i++) {
		if (chunk[i] != 0) {
			retval = vm_compressor_free(&chunk[i], flags);

			if (retval == 0) {
				num_slots_freed++;
			} else {
				if (retval == -2) {
					assert(flags & C_DONT_BLOCK);
				}

				if (failures) {
					*failures += 1;
				}
			}
		}
	}
	return num_slots_freed;
}

/* check if this pager has a slot_mapping spot for this page, if so give its position, if not, make place for it */
void
compressor_pager_slot_lookup(
	compressor_pager_t      pager,
	boolean_t               do_alloc,
	memory_object_offset_t  offset,
	compressor_slot_t       **slot_pp /* OUT */)
{
	unsigned int            num_chunks;
	uint32_t                page_num;
	unsigned int            chunk_idx;
	int                     slot_idx;
	compressor_slot_t       *chunk;
	compressor_slot_t       *t_chunk;

	/* offset is relative to the pager, first page of the first vm_object that created the pager has an offset of 0 */
	page_num = (uint32_t)(offset / PAGE_SIZE);
	if (page_num != (offset / PAGE_SIZE)) {
		/* overflow */
		panic("%s: offset 0x%llx overflow",
		    __FUNCTION__, (uint64_t) offset);
		*slot_pp = NULL;
		return;
	}
	if (page_num >= pager->cpgr_num_slots) {
		/* out of range */
		*slot_pp = NULL;
		return;
	}
	num_chunks = compressor_pager_num_chunks(pager);
	if (num_chunks > 1) {
		/* we have an array of chunks */
		chunk_idx = page_num / COMPRESSOR_SLOTS_PER_CHUNK;
		chunk = pager->cpgr_slots.cpgr_islots[chunk_idx];

		if (chunk == NULL && do_alloc) {
			t_chunk = zalloc_slot_array(COMPRESSOR_SLOTS_CHUNK_SIZE,
			    Z_WAITOK | Z_ZERO);

			compressor_pager_lock(pager);

			if ((chunk = pager->cpgr_slots.cpgr_islots[chunk_idx]) == NULL) {
				/*
				 * On some platforms, the memory stores from
				 * the bzero(t_chunk) above might not have been
				 * made visible and another thread might see
				 * the contents of this new chunk before it's
				 * been fully zero-filled.
				 * This memory barrier should take care of this
				 * according to the platform requirements.
				 */
				os_atomic_thread_fence(release);

				chunk = pager->cpgr_slots.cpgr_islots[chunk_idx] = t_chunk;
				t_chunk = NULL;
			}
			compressor_pager_unlock(pager);

			if (t_chunk) {
				zfree_slot_array(t_chunk, COMPRESSOR_SLOTS_CHUNK_SIZE);
			}
		}
		if (chunk == NULL) {
			*slot_pp = NULL;
		} else {
			slot_idx = page_num % COMPRESSOR_SLOTS_PER_CHUNK;
			*slot_pp = &chunk[slot_idx];
		}
	} else if (pager->cpgr_num_slots > 2) {
		slot_idx = page_num;
		*slot_pp = &pager->cpgr_slots.cpgr_dslots[slot_idx];
	} else {
		slot_idx = page_num;
		*slot_pp = &pager->cpgr_slots.cpgr_eslots[slot_idx];
	}
}

#if defined(__LP64__)
__startup_func
static void
vm_compressor_slots_init(void)
{
	for (unsigned int idx = 0; idx < NUM_SLOTS_ZONES; idx++) {
		compressor_slots_zones[idx] = zone_create(
			compressor_slots_zones_names[idx],
			compressor_slots_zones_sizes[idx],
			ZC_PGZ_USE_GUARDS | ZC_VM);
	}
}
STARTUP(ZALLOC, STARTUP_RANK_MIDDLE, vm_compressor_slots_init);
#endif /* defined(__LP64__) */

static compressor_slot_t *
zalloc_slot_array(size_t size, zalloc_flags_t flags)
{
#if defined(__LP64__)
	compressor_slot_t *slots = NULL;

	assert(size <= COMPRESSOR_SLOTS_CHUNK_SIZE);
	for (unsigned int idx = 0; idx < NUM_SLOTS_ZONES; idx++) {
		if (size > compressor_slots_zones_sizes[idx]) {
			continue;
		}
		slots = zalloc_flags(compressor_slots_zones[idx], flags);
		break;
	}
	return slots;
#else  /* defined(__LP64__) */
	return kalloc_data(size, flags);
#endif /* !defined(__LP64__) */
}

static void
zfree_slot_array(compressor_slot_t *slots, size_t size)
{
#if defined(__LP64__)
	assert(size <= COMPRESSOR_SLOTS_CHUNK_SIZE);
	for (unsigned int idx = 0; idx < NUM_SLOTS_ZONES; idx++) {
		if (size > compressor_slots_zones_sizes[idx]) {
			continue;
		}
		zfree(compressor_slots_zones[idx], slots);
		break;
	}
#else  /* defined(__LP64__) */
	kfree_data(slots, size);
#endif /* !defined(__LP64__) */
}

kern_return_t
vm_compressor_pager_put(
	memory_object_t                 mem_obj,
	memory_object_offset_t          offset,
	ppnum_t                         ppnum,
	void                            **current_chead,
	char                            *scratch_buf,
	int                             *compressed_count_delta_p, /* OUT */
	vm_compressor_options_t         flags)
{
	compressor_pager_t      pager;
	compressor_slot_t       *slot_p;

	compressor_pager_stats.put++;

	*compressed_count_delta_p = 0;

	/* This routine is called by the pageout thread.  The pageout thread */
	/* cannot be blocked by read activities unless the read activities   */
	/* Therefore the grant of vs lock must be done on a try versus a      */
	/* blocking basis.  The code below relies on the fact that the       */
	/* interface is synchronous.  Should this interface be again async   */
	/* for some type  of pager in the future the pages will have to be   */
	/* returned through a separate, asynchronous path.		     */

	compressor_pager_lookup(mem_obj, pager);

	uint32_t dummy_conv;
	if (os_convert_overflow(offset / PAGE_SIZE, &dummy_conv)) {
		/* overflow, page number doesn't fit in a uint32 */
		panic("%s: offset 0x%llx overflow", __FUNCTION__, (uint64_t) offset);
		return KERN_RESOURCE_SHORTAGE;
	}

	/* we're looking for the slot_mapping that corresponds to the offset, which vm_compressor_put() is then going to
	 * set a value into after it allocates the slot. if the slot_mapping doesn't exist, this will create it */
	compressor_pager_slot_lookup(pager, TRUE, offset, &slot_p);

	if (slot_p == NULL) {
		/* out of range ? */
		panic("vm_compressor_pager_put: out of range");
	}
	if (*slot_p != 0) {
		/*
		 * Already compressed: forget about the old one.
		 *
		 * This can happen after a vm_object_do_collapse() when
		 * the "backing_object" had some pages paged out and the
		 * "object" had an equivalent page resident.
		 */
		vm_compressor_free(slot_p, flags);
		*compressed_count_delta_p -= 1;
	}

	/*
	 * If the compressor operation succeeds, we presumably don't need to
	 * undo any previous WIMG update, as all live mappings should be
	 * disconnected.
	 */

	if (vm_compressor_put(ppnum, slot_p, current_chead, scratch_buf, flags)) {
		return KERN_RESOURCE_SHORTAGE;
	}
	*compressed_count_delta_p += 1;

	return KERN_SUCCESS;
}


kern_return_t
vm_compressor_pager_get(
	memory_object_t         mem_obj,
	memory_object_offset_t  offset,
	ppnum_t                 ppnum,
	int                     *my_fault_type,
	vm_compressor_options_t flags,
	int                     *compressed_count_delta_p)
{
	compressor_pager_t      pager;
	kern_return_t           kr;
	compressor_slot_t       *slot_p;

	compressor_pager_stats.get++;

	*compressed_count_delta_p = 0;

	if ((uint32_t)(offset / PAGE_SIZE) != (offset / PAGE_SIZE)) {
		panic("%s: offset 0x%llx overflow",
		    __FUNCTION__, (uint64_t) offset);
		return KERN_MEMORY_ERROR;
	}

	compressor_pager_lookup(mem_obj, pager);

	/* find the compressor slot for that page */
	compressor_pager_slot_lookup(pager, FALSE, offset, &slot_p);

	if (offset / PAGE_SIZE >= pager->cpgr_num_slots) {
		/* out of range */
		ktriage_record(thread_tid(current_thread()), KDBG_TRIAGE_EVENTID(KDBG_TRIAGE_SUBSYS_VM, KDBG_TRIAGE_RESERVED, KDBG_TRIAGE_VM_COMPRESSOR_GET_OUT_OF_RANGE), 0 /* arg */);
		kr = KERN_MEMORY_FAILURE;
	} else if (slot_p == NULL || *slot_p == 0) {
		/* compressor does not have this page */
		ktriage_record(thread_tid(current_thread()), KDBG_TRIAGE_EVENTID(KDBG_TRIAGE_SUBSYS_VM, KDBG_TRIAGE_RESERVED, KDBG_TRIAGE_VM_COMPRESSOR_GET_NO_PAGE), 0 /* arg */);
		kr = KERN_MEMORY_ERROR;
	} else {
		/* compressor does have this page */
		kr = KERN_SUCCESS;
	}
	*my_fault_type = DBG_COMPRESSOR_FAULT;

	if (kr == KERN_SUCCESS) {
		int     retval;
		bool unmodified = (vm_compressor_is_slot_compressed(slot_p) == false);
		/* get the page from the compressor */
		retval = vm_compressor_get(ppnum, slot_p, (unmodified ? (flags | C_PAGE_UNMODIFIED) : flags));
		if (retval == -1) {
			ktriage_record(thread_tid(current_thread()), KDBG_TRIAGE_EVENTID(KDBG_TRIAGE_SUBSYS_VM, KDBG_TRIAGE_RESERVED, KDBG_TRIAGE_VM_COMPRESSOR_DECOMPRESS_FAILED), 0 /* arg */);
			kr = KERN_MEMORY_FAILURE;
		} else if (retval == 1) {
			*my_fault_type = DBG_COMPRESSOR_SWAPIN_FAULT;
		} else if (retval == -2) {
			assert((flags & C_DONT_BLOCK));
			/*
			 * Not a fatal failure because we just retry with a blocking get later. So we skip ktriage to avoid noise.
			 */
			kr = KERN_FAILURE;
		}
	}

	if (kr == KERN_SUCCESS) {
		assert(slot_p != NULL);
		if (*slot_p != 0) {
			/*
			 * We got the page for a copy-on-write fault
			 * and we kept the original in place.  Slot
			 * is still occupied.
			 */
		} else {
			*compressed_count_delta_p -= 1;
		}
	}

	return kr;
}

unsigned int
vm_compressor_pager_state_clr(
	memory_object_t         mem_obj,
	memory_object_offset_t  offset)
{
	compressor_pager_t      pager;
	compressor_slot_t       *slot_p;
	unsigned int            num_slots_freed;

	assert(VM_CONFIG_COMPRESSOR_IS_PRESENT);

	compressor_pager_stats.state_clr++;

	if ((uint32_t)(offset / PAGE_SIZE) != (offset / PAGE_SIZE)) {
		/* overflow */
		panic("%s: offset 0x%llx overflow",
		    __FUNCTION__, (uint64_t) offset);
		return 0;
	}

	compressor_pager_lookup(mem_obj, pager);

	/* find the compressor slot for that page */
	compressor_pager_slot_lookup(pager, FALSE, offset, &slot_p);

	num_slots_freed = 0;
	if (slot_p && *slot_p != 0) {
		vm_compressor_free(slot_p, 0);
		num_slots_freed++;
		assert(*slot_p == 0);
	}

	return num_slots_freed;
}

vm_external_state_t
vm_compressor_pager_state_get(
	memory_object_t         mem_obj,
	memory_object_offset_t  offset)
{
	compressor_pager_t      pager;
	compressor_slot_t       *slot_p;

	assert(VM_CONFIG_COMPRESSOR_IS_PRESENT);

	compressor_pager_stats.state_get++;

	if ((uint32_t)(offset / PAGE_SIZE) != (offset / PAGE_SIZE)) {
		/* overflow */
		panic("%s: offset 0x%llx overflow",
		    __FUNCTION__, (uint64_t) offset);
		return VM_EXTERNAL_STATE_ABSENT;
	}

	compressor_pager_lookup(mem_obj, pager);

	/* find the compressor slot for that page */
	compressor_pager_slot_lookup(pager, FALSE, offset, &slot_p);

	if (offset / PAGE_SIZE >= pager->cpgr_num_slots) {
		/* out of range */
		return VM_EXTERNAL_STATE_ABSENT;
	} else if (slot_p == NULL || *slot_p == 0) {
		/* compressor does not have this page */
		return VM_EXTERNAL_STATE_ABSENT;
	} else {
		/* compressor does have this page */
		return VM_EXTERNAL_STATE_EXISTS;
	}
}

unsigned int
vm_compressor_pager_reap_pages(
	memory_object_t         mem_obj,
	vm_compressor_options_t flags)
{
	compressor_pager_t      pager;
	unsigned int            num_chunks;
	int                     failures;
	unsigned int            i;
	compressor_slot_t       *chunk;
	unsigned int            num_slots_freed;

	compressor_pager_lookup(mem_obj, pager);
	if (pager == NULL) {
		return 0;
	}

	compressor_pager_lock(pager);

	/* reap the compressor slots */
	num_slots_freed = 0;

	num_chunks = compressor_pager_num_chunks(pager);
	if (num_chunks > 1) {
		/* we have an array of chunks */
		for (i = 0; i < num_chunks; i++) {
			chunk = pager->cpgr_slots.cpgr_islots[i];
			if (chunk != NULL) {
				num_slots_freed +=
				    compressor_pager_slots_chunk_free(
					chunk,
					COMPRESSOR_SLOTS_PER_CHUNK,
					flags,
					&failures);
				if (failures == 0) {
					pager->cpgr_slots.cpgr_islots[i] = NULL;
					zfree_slot_array(chunk, COMPRESSOR_SLOTS_CHUNK_SIZE);
				}
			}
		}
	} else if (pager->cpgr_num_slots > 2) {
		chunk = pager->cpgr_slots.cpgr_dslots;
		num_slots_freed +=
		    compressor_pager_slots_chunk_free(
			chunk,
			pager->cpgr_num_slots,
			flags,
			NULL);
	} else {
		chunk = &pager->cpgr_slots.cpgr_eslots[0];
		num_slots_freed +=
		    compressor_pager_slots_chunk_free(
			chunk,
			pager->cpgr_num_slots,
			flags,
			NULL);
	}

	compressor_pager_unlock(pager);

	return num_slots_freed;
}

void
vm_compressor_pager_transfer(
	memory_object_t         dst_mem_obj,
	memory_object_offset_t  dst_offset,
	memory_object_t         src_mem_obj,
	memory_object_offset_t  src_offset)
{
	compressor_pager_t      src_pager, dst_pager;
	compressor_slot_t       *src_slot_p, *dst_slot_p;

	compressor_pager_stats.transfer++;

	/* find the compressor slot for the destination */
	compressor_pager_lookup(dst_mem_obj, dst_pager);
	assert(dst_offset / PAGE_SIZE < dst_pager->cpgr_num_slots);
	compressor_pager_slot_lookup(dst_pager, TRUE, dst_offset, &dst_slot_p);
	assert(dst_slot_p != NULL);
	assert(*dst_slot_p == 0);

	/* find the compressor slot for the source */
	compressor_pager_lookup(src_mem_obj, src_pager);
	assert(src_offset / PAGE_SIZE < src_pager->cpgr_num_slots);
	compressor_pager_slot_lookup(src_pager, FALSE, src_offset, &src_slot_p);
	assert(src_slot_p != NULL);
	assert(*src_slot_p != 0);

	/* transfer the slot from source to destination */
	vm_compressor_transfer(dst_slot_p, src_slot_p);
	OSAddAtomic(-1, &src_pager->cpgr_num_slots_occupied);
	OSAddAtomic(+1, &dst_pager->cpgr_num_slots_occupied);
}

memory_object_offset_t
vm_compressor_pager_next_compressed(
	memory_object_t         mem_obj,
	memory_object_offset_t  offset)
{
	compressor_pager_t      pager;
	unsigned int            num_chunks;
	uint32_t                page_num;
	unsigned int            chunk_idx;
	uint32_t                slot_idx;
	compressor_slot_t       *chunk;

	compressor_pager_lookup(mem_obj, pager);

	page_num = (uint32_t)(offset / PAGE_SIZE);
	if (page_num != (offset / PAGE_SIZE)) {
		/* overflow */
		return (memory_object_offset_t) -1;
	}
	if (page_num >= pager->cpgr_num_slots) {
		/* out of range */
		return (memory_object_offset_t) -1;
	}

	num_chunks = compressor_pager_num_chunks(pager);
	if (num_chunks == 1) {
		if (pager->cpgr_num_slots > 2) {
			chunk = pager->cpgr_slots.cpgr_dslots;
		} else {
			chunk = &pager->cpgr_slots.cpgr_eslots[0];
		}
		for (slot_idx = page_num;
		    slot_idx < pager->cpgr_num_slots;
		    slot_idx++) {
			if (chunk[slot_idx] != 0) {
				/* found a non-NULL slot in this chunk */
				return (memory_object_offset_t) slot_idx *
				       PAGE_SIZE;
			}
		}
		return (memory_object_offset_t) -1;
	}

	/* we have an array of chunks; find the next non-NULL chunk */
	chunk = NULL;
	for (chunk_idx = page_num / COMPRESSOR_SLOTS_PER_CHUNK,
	    slot_idx = page_num % COMPRESSOR_SLOTS_PER_CHUNK;
	    chunk_idx < num_chunks;
	    chunk_idx++,
	    slot_idx = 0) {
		chunk = pager->cpgr_slots.cpgr_islots[chunk_idx];
		if (chunk == NULL) {
			/* no chunk here: try the next one */
			continue;
		}
		/* search for an occupied slot in this chunk */
		for (;
		    slot_idx < COMPRESSOR_SLOTS_PER_CHUNK;
		    slot_idx++) {
			if (chunk[slot_idx] != 0) {
				/* found an occupied slot in this chunk */
				uint32_t next_slot;

				next_slot = ((chunk_idx *
				    COMPRESSOR_SLOTS_PER_CHUNK) +
				    slot_idx);
				if (next_slot >= pager->cpgr_num_slots) {
					/* went beyond end of object */
					return (memory_object_offset_t) -1;
				}
				return (memory_object_offset_t) next_slot *
				       PAGE_SIZE;
			}
		}
	}
	return (memory_object_offset_t) -1;
}

unsigned int
vm_compressor_pager_get_count(
	memory_object_t mem_obj)
{
	compressor_pager_t      pager;

	compressor_pager_lookup(mem_obj, pager);
	if (pager == NULL) {
		return 0;
	}

	/*
	 * The caller should have the VM object locked and one
	 * needs that lock to do a page-in or page-out, so no
	 * need to lock the pager here.
	 */
	assert(pager->cpgr_num_slots_occupied >= 0);

	return pager->cpgr_num_slots_occupied;
}

/* Add page count to the counter in the pager */
void
vm_compressor_pager_count(
	memory_object_t mem_obj,
	int             compressed_count_delta,
	boolean_t       shared_lock,
	vm_object_t     object __unused)
{
	compressor_pager_t      pager;

	if (compressed_count_delta == 0) {
		return;
	}

	compressor_pager_lookup(mem_obj, pager);
	if (pager == NULL) {
		return;
	}

	if (compressed_count_delta < 0) {
		assert(pager->cpgr_num_slots_occupied >=
		    (unsigned int) -compressed_count_delta);
	}

	/*
	 * The caller should have the VM object locked,
	 * shared or exclusive.
	 */
	if (shared_lock) {
		vm_object_lock_assert_shared(object);
		OSAddAtomic(compressed_count_delta,
		    &pager->cpgr_num_slots_occupied);
	} else {
		vm_object_lock_assert_exclusive(object);
		pager->cpgr_num_slots_occupied += compressed_count_delta;
	}
}

#if CONFIG_FREEZE
kern_return_t
vm_compressor_pager_relocate(
	memory_object_t         mem_obj,
	memory_object_offset_t  offset,
	void                    **current_chead)
{
	/*
	 * Has the page at this offset been compressed?
	 */

	compressor_slot_t *slot_p;
	compressor_pager_t dst_pager;

	assert(mem_obj);

	compressor_pager_lookup(mem_obj, dst_pager);
	if (dst_pager == NULL) {
		return KERN_FAILURE;
	}

	compressor_pager_slot_lookup(dst_pager, FALSE, offset, &slot_p);
	return vm_compressor_relocate(current_chead, slot_p);
}
#endif /* CONFIG_FREEZE */

#if DEVELOPMENT || DEBUG

kern_return_t
vm_compressor_pager_inject_error(memory_object_t mem_obj,
    memory_object_offset_t offset)
{
	kern_return_t result = KERN_FAILURE;
	compressor_slot_t *slot_p;
	compressor_pager_t pager;

	assert(mem_obj);

	compressor_pager_lookup(mem_obj, pager);
	if (pager != NULL) {
		compressor_pager_slot_lookup(pager, FALSE, offset, &slot_p);
		if (slot_p != NULL && *slot_p != 0) {
			vm_compressor_inject_error(slot_p);
			result = KERN_SUCCESS;
		}
	}

	return result;
}


/*
 * Write debugging information about the pager to the given buffer
 * returns: true on success, false if there was not enough space
 * argument size - in: bytes free in the buffer, out: bytes written
 */
kern_return_t
vm_compressor_pager_dump(memory_object_t mem_obj,     /* IN */
    __unused char *buf,                               /* IN buffer to write to */
    __unused size_t *size,                           /* IN-OUT */
    bool *is_compressor,                              /* OUT */
    unsigned int *slot_count)                         /* OUT */
{
	compressor_pager_t pager = NULL;
	compressor_pager_lookup(mem_obj, pager);

	*size = 0;
	if (pager == NULL) {
		*is_compressor = false;
		*slot_count = 0;
		return KERN_SUCCESS;
	}
	*is_compressor = true;
	*slot_count = pager->cpgr_num_slots_occupied;

	/*
	 *  size_t insize = *size;
	 *  unsigned int needed_size = 0; // pager->cpgr_num_slots_occupied * sizeof(compressor_slot_t) / sizeof(int);
	 *  if (needed_size > insize) {
	 *       return KERN_NO_SPACE;
	 *  }
	 *  TODO: not fully implemented yet, need to dump out the mappings
	 * size = 0;
	 */
	return KERN_SUCCESS;
}

#endif
