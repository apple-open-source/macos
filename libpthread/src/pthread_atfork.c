/*
 * Copyright (c) 1999, 2012 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include "internal.h"

#include <libkern/OSAtomic.h>
#include <mach/mach_init.h>
#include <mach/mach_vm.h>
#include <platform/compat.h>

PTHREAD_NOEXPORT void pthread_workqueue_atfork_child(void);
PTHREAD_NOEXPORT void __pthread_fork_child_internal(pthread_t);

int
pthread_atfork(void (*prepare)(void), void (*parent)(void), void (*child)(void))
{
	int res = 0;
	size_t idx;
	pthread_globals_t globals = _pthread_globals();
	
	OSSpinLockLock(&globals->pthread_atfork_lock);
	idx = globals->atfork_count++;

	if (idx == 0) {
		// Initialize pointer to inline storage.
		globals->atfork = globals->atfork_storage;
	} else if (idx == PTHREAD_ATFORK_INLINE_MAX) {
		// Migrate to out-of-line storage.
		kern_return_t kr;
		mach_vm_address_t storage = 0;
		mach_vm_size_t size = PTHREAD_ATFORK_MAX * sizeof(struct pthread_atfork_entry);
		OSSpinLockUnlock(&globals->pthread_atfork_lock);
		kr = mach_vm_map(mach_task_self(),
				 &storage,
				 size,
				 vm_page_size - 1,
				 VM_MAKE_TAG(VM_MEMORY_OS_ALLOC_ONCE)| VM_FLAGS_ANYWHERE,
				 MEMORY_OBJECT_NULL,
				 0,
				 FALSE,
				 VM_PROT_DEFAULT,
				 VM_PROT_ALL,
				 VM_INHERIT_DEFAULT);
		OSSpinLockLock(&globals->pthread_atfork_lock);
		if (kr == KERN_SUCCESS) {
			if (globals->atfork == globals->atfork_storage) {
				globals->atfork = storage;
				memmove(globals->atfork, globals->atfork_storage, sizeof(globals->atfork_storage));
				bzero(globals->atfork_storage, sizeof(globals->atfork_storage));
			} else {
				// Another thread did vm_map first.
				OSSpinLockUnlock(&globals->pthread_atfork_lock);
				mach_vm_deallocate(mach_task_self(), storage, size);
				OSSpinLockLock(&globals->pthread_atfork_lock);
			}
		} else {
			res = ENOMEM;
		}
	} else if (idx >= PTHREAD_ATFORK_MAX) {
		res = ENOMEM;
	}
 
	if (res == 0) {
		struct pthread_atfork_entry *e = &globals->atfork[idx];
		e->prepare = prepare;
		e->parent = parent;
		e->child = child;
	}
	OSSpinLockUnlock(&globals->pthread_atfork_lock);

	return res;
}

// Called before the fork(2) system call is made in the parent process.
// Iterate pthread_atfork prepare handlers.
void
_pthread_fork_prepare(void)
{
	pthread_globals_t globals = _pthread_globals();

	OSSpinLockLock(&globals->pthread_atfork_lock);
	
	size_t idx;
	for (idx = globals->atfork_count; idx > 0; --idx) {
		struct pthread_atfork_entry *e = &globals->atfork[idx-1];
		if (e->prepare != NULL) {
			e->prepare();
		}
	}

	OSSpinLockLock(&globals->psaved_self_global_lock);
	globals->psaved_self = pthread_self();
	OSSpinLockLock(&globals->psaved_self->lock);
}

// Called after the fork(2) system call returns to the parent process.
// Iterate pthread_atfork parent handlers.
void
_pthread_fork_parent(void)
{
	pthread_globals_t globals = _pthread_globals();

	OSSpinLockUnlock(&globals->psaved_self->lock);
	OSSpinLockUnlock(&globals->psaved_self_global_lock);

	size_t idx;
	for (idx = 0; idx < globals->atfork_count; ++idx) {
		struct pthread_atfork_entry *e = &globals->atfork[idx];
		if (e->parent != NULL) {
			e->parent();
		}
	}
	OSSpinLockUnlock(&globals->pthread_atfork_lock);
}

// Called after the fork(2) system call returns to the new child process.
// Clean up data structures of other threads which no longer exist in the child.
// Make the current thread the main thread.
void
_pthread_fork_child(void)
{
	pthread_globals_t globals = _pthread_globals();
	globals->psaved_self_global_lock = OS_SPINLOCK_INIT;
	__pthread_fork_child_internal(globals->psaved_self);
	__is_threaded = 0;
	pthread_workqueue_atfork_child();
}

// Iterate pthread_atfork child handlers.
void
_pthread_fork_child_postinit(void)
{
	pthread_globals_t globals = _pthread_globals();
	size_t idx;
	for (idx = 0; idx < globals->atfork_count; ++idx) {
		struct pthread_atfork_entry *e = &globals->atfork[idx];
		if (e->child != NULL) {
			e->child();
		}
	}
	globals->pthread_atfork_lock = OS_SPINLOCK_INIT;
}
