/*
 * Copyright (c) 2000-2006 Apple Computer, Inc. All rights reserved.
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

#include <sys/types.h>
#include <sys/proc.h>
#include <sys/proc_internal.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/dtrace_ptss.h>

#include <mach/vm_map.h>
#include <mach/vm_param.h>
#include <mach/mach_vm.h>

#include <kern/task.h>

#include <vm/vm_map.h>
#include <vm/vm_kern_xnu.h>

/*
 * This function requires the sprlock to be held
 *
 * In general, it will not block. If it needs to allocate a new
 * page of memory, the underlying kernel kalloc may block.
 */
struct dtrace_ptss_page_entry*
dtrace_ptss_claim_entry_locked(struct proc* p)
{
	LCK_MTX_ASSERT(&p->p_dtrace_sprlock, LCK_MTX_ASSERT_OWNED);

	struct dtrace_ptss_page_entry* entry = NULL;

	while (TRUE) {
		struct dtrace_ptss_page_entry* temp = p->p_dtrace_ptss_free_list;

		if (temp == NULL) {
			// Nothing on the free list. Allocate a new page, its okay if multiple threads race here.
			struct dtrace_ptss_page* page = dtrace_ptss_allocate_page(p);

			// Make sure we actually got a page
			if (page == NULL) {
				return NULL;
			}

			// Add the page to the page list
			page->next = p->p_dtrace_ptss_pages;
			p->p_dtrace_ptss_pages = page;

			// CAS the entries onto the free list.
			do {
				page->entries[DTRACE_PTSS_ENTRIES_PER_PAGE - 1].next = p->p_dtrace_ptss_free_list;
			} while (!OSCompareAndSwapPtr((void *)page->entries[DTRACE_PTSS_ENTRIES_PER_PAGE - 1].next,
			    (void *)&page->entries[0],
			    (void * volatile *)&p->p_dtrace_ptss_free_list));

			// Now that we've added to the free list, try again.
			continue;
		}

		// Claim temp
		if (!OSCompareAndSwapPtr((void *)temp, (void *)temp->next, (void * volatile *)&p->p_dtrace_ptss_free_list)) {
			continue;
		}

		// At this point, we own temp.
		entry = temp;

		break;
	}

	return entry;
}

/*
 * This function does not require any locks to be held on entry.
 */
struct dtrace_ptss_page_entry*
dtrace_ptss_claim_entry(struct proc* p)
{
	// Verify no locks held on entry
	LCK_MTX_ASSERT(&p->p_dtrace_sprlock, LCK_MTX_ASSERT_NOTOWNED);
	LCK_MTX_ASSERT(&p->p_mlock, LCK_MTX_ASSERT_NOTOWNED);

	struct dtrace_ptss_page_entry* entry = NULL;

	while (TRUE) {
		struct dtrace_ptss_page_entry* temp = p->p_dtrace_ptss_free_list;

		if (temp == NULL) {
			lck_mtx_lock(&p->p_dtrace_sprlock);
			temp = dtrace_ptss_claim_entry_locked(p);
			lck_mtx_unlock(&p->p_dtrace_sprlock);
			return temp;
		}

		// Claim temp
		if (!OSCompareAndSwapPtr((void *)temp, (void *)temp->next, (void * volatile *)&p->p_dtrace_ptss_free_list)) {
			continue;
		}

		// At this point, we own temp.
		entry = temp;

		break;
	}

	return entry;
}

/*
 * This function does not require any locks to be held on entry.
 *
 * (PR-11138709) A NULL p->p_dtrace_ptss_pages means the entry can
 * no longer be referenced safely. When found in this state, the chore
 * of releasing an entry to the free list is ignored.
 */
void
dtrace_ptss_release_entry(struct proc* p, struct dtrace_ptss_page_entry* e)
{
	if (p && p->p_dtrace_ptss_pages && e) {
		do {
			e->next = p->p_dtrace_ptss_free_list;
		} while (!OSCompareAndSwapPtr((void *)e->next, (void *)e, (void * volatile *)&p->p_dtrace_ptss_free_list));
	}
}

/*
 * This function allocates a new page in the target process's address space.
 *
 * It returns a dtrace_ptss_page that has its entries chained, with the last
 * entries next field set to NULL. It does not add the page or the entries to
 * the process's page/entry lists.
 *
 * This function does not require that any locks be held when it is invoked.
 */
struct dtrace_ptss_page*
dtrace_ptss_allocate_page(struct proc* p)
{
	// Allocate the kernel side data
	struct dtrace_ptss_page* ptss_page = kalloc_type(struct dtrace_ptss_page, Z_ZERO | Z_WAITOK);
	if (ptss_page == NULL) {
		return NULL;
	}

	// Now allocate a page in user space and set its protections to allow execute.
	task_t task = proc_task(p);
	vm_map_t map = get_task_map_reference(task);
	if (map == NULL) {
		goto err;
	}

	mach_vm_size_t size = PAGE_MAX_SIZE;
	mach_vm_offset_t addr = 0;
	mach_vm_offset_t write_addr = 0;
	/*
	 * The embedded OS has extra permissions for writable and executable pages.
	 * To ensure correct permissions, we must set the page protections separately.
	 */
	vm_prot_t cur_protection = VM_PROT_READ | VM_PROT_EXECUTE;
	vm_prot_t max_protection = VM_PROT_READ | VM_PROT_EXECUTE;
	kern_return_t kr;

	kr = mach_vm_map_kernel(map, &addr, size, 0,
	    VM_MAP_KERNEL_FLAGS_ANYWHERE(), IPC_PORT_NULL, 0, FALSE,
	    cur_protection, max_protection, VM_INHERIT_DEFAULT);
	if (kr != KERN_SUCCESS) {
		goto err;
	}

	/*
	 * To ensure the page is properly marked as user debug, temporarily change
	 * the permissions to rw and then back again to rx. The VM will keep track
	 * of this remapping and on fault will pass PMAP_OPTIONS_XNU_USER_DEBUG
	 * properly to the PMAP layer.
	 */
	kr = mach_vm_protect(map, (mach_vm_offset_t)addr, (mach_vm_size_t)size, 0,
	    VM_PROT_READ | VM_PROT_WRITE | VM_PROT_COPY);
	if (kr != KERN_SUCCESS) {
		goto err;
	}

	kr = mach_vm_protect(map, (mach_vm_offset_t)addr, (mach_vm_size_t)size, 0,
	    VM_PROT_READ | VM_PROT_EXECUTE);
	if (kr != KERN_SUCCESS) {
		goto err;
	}

	/*
	 * If on embedded, remap the scratch space as writable at another
	 * virtual address
	 */
	kr = mach_vm_remap(map, &write_addr, size, 0,
	    VM_FLAGS_ANYWHERE, map, addr, FALSE,
	    &cur_protection, &max_protection, VM_INHERIT_DEFAULT);
	if (kr != KERN_SUCCESS || !(max_protection & VM_PROT_WRITE)) {
		goto err;
	}

	kr = mach_vm_protect(map, (mach_vm_offset_t)write_addr, (mach_vm_size_t)size, 0, VM_PROT_READ | VM_PROT_WRITE);
	if (kr != KERN_SUCCESS) {
		goto err;
	}

	// Chain the page entries.
	int i;
	for (i = 0; i < DTRACE_PTSS_ENTRIES_PER_PAGE; i++) {
		ptss_page->entries[i].addr = addr + (i * DTRACE_PTSS_SCRATCH_SPACE_PER_THREAD);
		ptss_page->entries[i].write_addr = write_addr + (i * DTRACE_PTSS_SCRATCH_SPACE_PER_THREAD);
		ptss_page->entries[i].next = &ptss_page->entries[i + 1];
	}

	// The last entry should point to NULL
	ptss_page->entries[DTRACE_PTSS_ENTRIES_PER_PAGE - 1].next = NULL;

	vm_map_deallocate(map);

	return ptss_page;

err:
	kfree_type(struct dtrace_ptss_page, ptss_page);

	if (map) {
		vm_map_deallocate(map);
	}

	return NULL;
}

/*
 * This function frees an existing page in the target process's address space.
 *
 * It does not alter any of the process's page/entry lists.
 *
 * TODO: Inline in dtrace_ptrace_exec_exit?
 */
void
dtrace_ptss_free_page(struct proc* p, struct dtrace_ptss_page* ptss_page)
{
	// Grab the task and get a reference to its vm_map
	task_t task = proc_task(p);
	vm_map_t map = get_task_map_reference(task);

	mach_vm_address_t addr = ptss_page->entries[0].addr;
	mach_vm_size_t size = PAGE_SIZE; // We need some way to assert that this matches vm_map_round_page() !!!

	// Silent failures, no point in checking return code.
	mach_vm_deallocate(map, addr, size);

	mach_vm_address_t write_addr = ptss_page->entries[0].write_addr;
	mach_vm_deallocate(map, write_addr, size);

	vm_map_deallocate(map);
}

/*
 * This function assumes that the target process has been
 * suspended, and the proc_lock & sprlock is held
 */
void
dtrace_ptss_enable(struct proc* p)
{
	LCK_MTX_ASSERT(&p->p_dtrace_sprlock, LCK_MTX_ASSERT_OWNED);
	LCK_MTX_ASSERT(&p->p_mlock, LCK_MTX_ASSERT_OWNED);

	struct uthread* uth;
	/*
	 * XXX There has been a concern raised about holding the proc_lock
	 * while calling dtrace_ptss_claim_entry(), due to the fact
	 * that dtrace_ptss_claim_entry() can potentially malloc.
	 */
	TAILQ_FOREACH(uth, &p->p_uthlist, uu_list) {
		uth->t_dtrace_scratch = dtrace_ptss_claim_entry_locked(p);
	}
}

/*
 * This function is not thread safe.
 *
 * It assumes the sprlock is held, and the proc_lock is not.
 */
void
dtrace_ptss_exec_exit(struct proc* p)
{
	/*
	 * Should hold sprlock to touch the pages list. Must not
	 * hold the proc lock to avoid deadlock.
	 */
	LCK_MTX_ASSERT(&p->p_dtrace_sprlock, LCK_MTX_ASSERT_OWNED);
	LCK_MTX_ASSERT(&p->p_mlock, LCK_MTX_ASSERT_NOTOWNED);

	p->p_dtrace_ptss_free_list = NULL;

	struct dtrace_ptss_page* temp = p->p_dtrace_ptss_pages;
	p->p_dtrace_ptss_pages = NULL;

	while (temp != NULL) {
		struct dtrace_ptss_page* next = temp->next;

		// Do we need to specifically mach_vm_deallocate the user pages?
		// This can be called when the process is exiting, I believe the proc's
		// vm_map_t may already be toast.

		// Must be certain to free the kernel memory!
		kfree_type(struct dtrace_ptss_page, temp);
		temp = next;
	}
}

/*
 * This function is not thread safe.
 *
 * The child proc ptss fields are initialized to NULL at fork time.
 * Pages allocated in the parent are copied as part of the vm_map copy, though.
 * We need to deallocate those pages.
 *
 * Parent and child sprlock should be held, and proc_lock must NOT be held.
 */
void
dtrace_ptss_fork(struct proc* parent, struct proc* child)
{
	// The child should not have any pages/entries allocated at this point.
	// ASSERT(child->p_dtrace_ptss_pages == NULL);
	// ASSERT(child->p_dtrace_ptss_free_list == NULL);

	/*
	 * The parent's sprlock should be held, to protect its pages list
	 * from changing while the child references it. The child's sprlock
	 * must also be held, because we are modifying its pages list.
	 * Finally, to prevent a deadlock with the fasttrap cleanup code,
	 * neither the parent or child proc_lock should be held.
	 */
	LCK_MTX_ASSERT(&parent->p_dtrace_sprlock, LCK_MTX_ASSERT_OWNED);
	LCK_MTX_ASSERT(&parent->p_mlock, LCK_MTX_ASSERT_NOTOWNED);
	LCK_MTX_ASSERT(&child->p_dtrace_sprlock, LCK_MTX_ASSERT_OWNED);
	LCK_MTX_ASSERT(&child->p_mlock, LCK_MTX_ASSERT_NOTOWNED);

	// Get page list from *PARENT*
	struct dtrace_ptss_page* temp = parent->p_dtrace_ptss_pages;

	while (temp != NULL) {
		// Freeing the page in the *CHILD*
		dtrace_ptss_free_page(child, temp);

		// Do not free the kernel memory, it belong to the parent.
		temp = temp->next;
	}
}
