/*
 * Copyright (c) 2003-2019 Apple Inc. All rights reserved.
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
 *	Kernel stack management routines.
 */

#include <mach/mach_host.h>
#include <mach/mach_types.h>
#include <mach/processor_set.h>

#include <kern/kern_types.h>
#include <kern/lock_group.h>
#include <kern/mach_param.h>
#include <kern/misc_protos.h>
#include <kern/percpu.h>
#include <kern/processor.h>
#include <kern/thread.h>
#include <kern/zalloc.h>
#include <kern/kalloc.h>
#include <kern/ledger.h>

#include <vm/vm_map_xnu.h>
#include <vm/vm_kern_xnu.h>

#include <san/kasan.h>

/*
 *	We allocate stacks from generic kernel VM.
 *
 *	The stack_free_list can only be accessed at splsched,
 *	because stack_alloc_try/thread_invoke operate at splsched.
 */

static SIMPLE_LOCK_DECLARE(stack_lock_data, 0);
#define stack_lock()            simple_lock(&stack_lock_data, LCK_GRP_NULL)
#define stack_unlock()          simple_unlock(&stack_lock_data)

#define STACK_CACHE_SIZE        2

static vm_offset_t              stack_free_list;

static unsigned int             stack_free_count, stack_free_hiwat;             /* free list count */
static unsigned int             stack_hiwat;
unsigned int                    stack_total;                            /* current total count */
unsigned long long              stack_allocs;                           /* total count of allocations */

static unsigned int             stack_free_target;
static int                      stack_free_delta;

static unsigned int             stack_new_count;                                                /* total new stack allocations */

static SECURITY_READ_ONLY_LATE(vm_offset_t)  stack_addr_mask;
SECURITY_READ_ONLY_LATE(vm_offset_t)         kernel_stack_size;
SECURITY_READ_ONLY_LATE(vm_offset_t)         kernel_stack_mask;
vm_offset_t                                  kernel_stack_depth_max;

struct stack_cache {
	vm_offset_t     free;
	unsigned int    count;
};
static struct stack_cache PERCPU_DATA(stack_cache);

/*
 *	The next field is at the base of the stack,
 *	so the low end is left unsullied.
 */
#define stack_next(stack)       \
	(*((vm_offset_t *)((stack) + kernel_stack_size) - 1))

static inline vm_offset_t
roundup_pow2(vm_offset_t size)
{
	if ((size & (size - 1)) == 0) {
		/* if size is a power of 2 we're good */
		return size;
	}

	return 1ul << flsll(size);
}

static vm_offset_t stack_alloc_internal(void);
static void stack_free_stack(vm_offset_t);

static void
stack_init(void)
{
	uint32_t kernel_stack_pages = atop(KERNEL_STACK_SIZE);

	kernel_stack_size = KERNEL_STACK_SIZE;
	kernel_stack_mask = -KERNEL_STACK_SIZE;

	if (PE_parse_boot_argn("kernel_stack_pages",
	    &kernel_stack_pages,
	    sizeof(kernel_stack_pages))) {
		kernel_stack_size = kernel_stack_pages * PAGE_SIZE;
	}

	if (kernel_stack_size < round_page(kernel_stack_size)) {
		panic("stack_init: stack size %p not a multiple of page size %d",
		    (void *) kernel_stack_size, PAGE_SIZE);
	}

	stack_addr_mask = roundup_pow2(kernel_stack_size) - 1;
	kernel_stack_mask = ~stack_addr_mask;
}
STARTUP(TUNABLES, STARTUP_RANK_MIDDLE, stack_init);

/*
 *	stack_alloc:
 *
 *	Allocate a stack for a thread, may
 *	block.
 */

static vm_offset_t
stack_alloc_internal(void)
{
	vm_offset_t     stack = 0;
	spl_t           s;
	kma_flags_t     flags = KMA_NOFAIL | KMA_GUARD_FIRST | KMA_GUARD_LAST |
	    KMA_KSTACK | KMA_KOBJECT | KMA_ZERO | KMA_SPRAYQTN;

	s = splsched();
	stack_lock();
	stack_allocs++;
	stack = stack_free_list;
	if (stack != 0) {
		stack_free_list = stack_next(stack);
		stack_free_count--;
	} else {
		if (++stack_total > stack_hiwat) {
			stack_hiwat = stack_total;
		}
		stack_new_count++;
	}
	stack_free_delta--;
	stack_unlock();
	splx(s);

	if (stack == 0) {
		/*
		 * Request guard pages on either side of the stack.  Ask
		 * kernel_memory_allocate() for two extra pages to account
		 * for these.
		 */

		kernel_memory_allocate(kernel_map, &stack,
		    kernel_stack_size + ptoa(2), stack_addr_mask,
		    flags, VM_KERN_MEMORY_STACK);

		/*
		 * The stack address that comes back is the address of the lower
		 * guard page.  Skip past it to get the actual stack base address.
		 */

		stack += PAGE_SIZE;
	}
	return stack;
}

void
stack_alloc(
	thread_t        thread)
{
	assert(thread->kernel_stack == 0);
	machine_stack_attach(thread, stack_alloc_internal());
}

void
stack_handoff(thread_t from, thread_t to)
{
	assert(from == current_thread());
	machine_stack_handoff(from, to);
}

/*
 *	stack_free:
 *
 *	Detach and free the stack for a thread.
 */
void
stack_free(
	thread_t        thread)
{
	vm_offset_t         stack = machine_stack_detach(thread);

	assert(stack);
	if (stack != thread->reserved_stack) {
		stack_free_stack(stack);
	}
}

void
stack_free_reserved(
	thread_t        thread)
{
	if (thread->reserved_stack != thread->kernel_stack) {
		stack_free_stack(thread->reserved_stack);
	}
}

static void
stack_free_stack(
	vm_offset_t             stack)
{
	struct stack_cache      *cache;
	spl_t                           s;

#if KASAN_DEBUG
	/* Sanity check - stack should be unpoisoned by now */
	assert(kasan_check_shadow(stack, kernel_stack_size, 0));
#endif

	s = splsched();
	cache = PERCPU_GET(stack_cache);
	if (cache->count < STACK_CACHE_SIZE) {
		stack_next(stack) = cache->free;
		cache->free = stack;
		cache->count++;
	} else {
		stack_lock();
		stack_next(stack) = stack_free_list;
		stack_free_list = stack;
		if (++stack_free_count > stack_free_hiwat) {
			stack_free_hiwat = stack_free_count;
		}
		stack_free_delta++;
		stack_unlock();
	}
	splx(s);
}

/*
 *	stack_alloc_try:
 *
 *	Non-blocking attempt to allocate a
 *	stack for a thread.
 *
 *	Returns TRUE on success.
 *
 *	Called at splsched.
 */
boolean_t
stack_alloc_try(
	thread_t                thread)
{
	struct stack_cache      *cache;
	vm_offset_t                     stack;

	cache = PERCPU_GET(stack_cache);
	stack = cache->free;
	if (stack != 0) {
		cache->free = stack_next(stack);
		cache->count--;
	} else {
		if (stack_free_list != 0) {
			stack_lock();
			stack = stack_free_list;
			if (stack != 0) {
				stack_free_list = stack_next(stack);
				stack_free_count--;
				stack_free_delta--;
			}
			stack_unlock();
		}
	}

	if (stack != 0 || (stack = thread->reserved_stack) != 0) {
		machine_stack_attach(thread, stack);
		return TRUE;
	}

	return FALSE;
}

static unsigned int             stack_collect_tick, last_stack_tick;

/*
 *	stack_collect:
 *
 *	Free excess kernel stacks, may
 *	block.
 */
void
stack_collect(void)
{
	if (stack_collect_tick != last_stack_tick) {
		unsigned int    target;
		vm_offset_t             stack;
		spl_t                   s;

		s = splsched();
		stack_lock();

		target = stack_free_target + (STACK_CACHE_SIZE * processor_count);
		target += (stack_free_delta >= 0)? stack_free_delta: -stack_free_delta;

		while (stack_free_count > target) {
			stack = stack_free_list;
			stack_free_list = stack_next(stack);
			stack_free_count--; stack_total--;
			stack_unlock();
			splx(s);

			/*
			 * Get the stack base address, then decrement by one page
			 * to account for the lower guard page.  Add two extra pages
			 * to the size to account for the guard pages on both ends
			 * that were originally requested when the stack was allocated
			 * back in stack_alloc().
			 */

			stack = (vm_offset_t)vm_map_trunc_page(
				stack,
				VM_MAP_PAGE_MASK(kernel_map));
			stack -= PAGE_SIZE;
			kmem_free(kernel_map, stack, kernel_stack_size + ptoa(2));
			stack = 0;

			s = splsched();
			stack_lock();

			target = stack_free_target + (STACK_CACHE_SIZE * processor_count);
			target += (stack_free_delta >= 0)? stack_free_delta: -stack_free_delta;
		}

		last_stack_tick = stack_collect_tick;

		stack_unlock();
		splx(s);
	}
}

/*
 *	compute_stack_target:
 *
 *	Computes a new target free list count
 *	based on recent alloc / free activity.
 *
 *	Limits stack collection to once per
 *	computation period.
 */
void
compute_stack_target(
	__unused void           *arg)
{
	spl_t           s;

	s = splsched();
	stack_lock();

	if (stack_free_target > 5) {
		stack_free_target = (4 * stack_free_target) / 5;
	} else if (stack_free_target > 0) {
		stack_free_target--;
	}

	stack_free_target += (stack_free_delta >= 0)? stack_free_delta: -stack_free_delta;

	stack_free_delta = 0;
	stack_collect_tick++;

	stack_unlock();
	splx(s);
}

/* OBSOLETE */
void    stack_privilege(
	thread_t        thread);

void
stack_privilege(
	__unused thread_t       thread)
{
	/* OBSOLETE */
}

/*
 * Return info on stack usage for threads in a specific processor set
 */
kern_return_t
processor_set_stack_usage(
	processor_set_t pset,
	unsigned int    *totalp,
	vm_size_t       *spacep,
	vm_size_t       *residentp,
	vm_size_t       *maxusagep,
	vm_offset_t     *maxstackp)
{
#if DEVELOPMENT || DEBUG
	unsigned int total = 0;
	thread_t thread;

	if (pset == PROCESSOR_SET_NULL || pset != &pset0) {
		return KERN_INVALID_ARGUMENT;
	}

	lck_mtx_lock(&tasks_threads_lock);

	queue_iterate(&threads, thread, thread_t, threads) {
		total += (thread->kernel_stack != 0);
	}

	lck_mtx_unlock(&tasks_threads_lock);

	*totalp = total;
	*residentp = *spacep = total * round_page(kernel_stack_size);
	*maxusagep = 0;
	*maxstackp = 0;
	return KERN_SUCCESS;

#else
#pragma unused(pset, totalp, spacep, residentp, maxusagep, maxstackp)
	return KERN_NOT_SUPPORTED;
#endif /* DEVELOPMENT || DEBUG */
}

vm_offset_t
min_valid_stack_address(void)
{
	return (vm_offset_t)vm_map_min(kernel_map);
}

vm_offset_t
max_valid_stack_address(void)
{
	return (vm_offset_t)vm_map_max(kernel_map);
}
