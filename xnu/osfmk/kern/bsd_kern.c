/*
 * Copyright (c) 2000-2018 Apple Inc. All rights reserved.
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
#include <mach/mach_types.h>
#include <mach/machine/vm_param.h>
#include <mach/task.h>

#include <kern/kern_types.h>
#include <kern/ledger.h>
#include <kern/processor.h>
#include <kern/thread.h>
#include <kern/task.h>
#include <kern/spl.h>
#include <kern/ast.h>
#include <kern/monotonic.h>
#include <machine/monotonic.h>
#include <ipc/ipc_port.h>
#include <ipc/ipc_object.h>
#include <vm/vm_map_xnu.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>
#include <vm/vm_protos.h> /* last */
#include <sys/resource.h>
#include <sys/signal.h>
#include <sys/errno.h>
#include <sys/proc_require.h>

#include <machine/limits.h>
#include <sys/codesign.h> /* CS_CDHASH_LEN */

#undef thread_should_halt

/* BSD KERN COMPONENT INTERFACE */

extern unsigned int not_in_kdp; /* Skip acquiring locks if we're in kdp */

thread_t get_firstthread(task_t);
int get_task_userstop(task_t);
int get_thread_userstop(thread_t);
boolean_t current_thread_aborted(void);
void task_act_iterate_wth_args(task_t, void (*)(thread_t, void *), void *);
kern_return_t get_signalact(task_t, thread_t *, int);
int fill_task_rusage(task_t task, rusage_info_current *ri);
int fill_task_io_rusage(task_t task, rusage_info_current *ri);
int fill_task_qos_rusage(task_t task, rusage_info_current *ri);
uint64_t get_task_logical_writes(task_t task, bool external);
void fill_task_billed_usage(task_t task, rusage_info_current *ri);
void task_bsdtask_kill(task_t);

extern uint64_t get_dispatchqueue_serialno_offset_from_proc(void *p);
extern uint64_t get_dispatchqueue_label_offset_from_proc(void *p);
extern uint64_t proc_uniqueid_task(void *p, void *t);
extern int proc_pidversion(void *p);
extern int proc_getcdhash(void *p, char *cdhash);

int mach_to_bsd_errno(kern_return_t mach_err);
kern_return_t kern_return_for_errno(int bsd_errno);

#if MACH_BSD
extern void psignal(void *, int);
#endif

/*
 *
 */
void  *
get_bsdtask_info(task_t t)
{
	void *proc_from_task = task_get_proc_raw(t);
	proc_require(proc_from_task, PROC_REQUIRE_ALLOW_NULL | PROC_REQUIRE_ALLOW_ALL);
	return task_has_proc(t) ? proc_from_task : NULL;
}

void
task_bsdtask_kill(task_t t)
{
	void * bsd_info = get_bsdtask_info(t);
	if (bsd_info != NULL) {
		psignal(bsd_info, SIGKILL);
	}
}
/*
 *
 */
void *
get_bsdthreadtask_info(thread_t th)
{
	return get_thread_ro(th)->tro_proc;
}

/*
 *
 */
void
set_bsdtask_info(task_t t, void * v)
{
	void *proc_from_task = task_get_proc_raw(t);
	if (v == NULL) {
		task_clear_has_proc(t);
	} else {
		if (v != proc_from_task) {
			panic("set_bsdtask_info trying to set random bsd_info %p", v);
		}
		task_set_has_proc(t);
	}
}

__abortlike
static void
__thread_ro_circularity_panic(thread_t th, thread_ro_t tro)
{
	panic("tro %p points back to %p instead of %p", tro, tro->tro_owner, th);
}

__attribute__((always_inline))
thread_ro_t
get_thread_ro_unchecked(thread_t th)
{
	return th->t_tro;
}

thread_ro_t
get_thread_ro(thread_t th)
{
	thread_ro_t tro = th->t_tro;

	zone_require_ro(ZONE_ID_THREAD_RO, sizeof(struct thread_ro), tro);
	if (tro->tro_owner != th) {
		__thread_ro_circularity_panic(th, tro);
	}
	return tro;
}

__attribute__((always_inline))
thread_ro_t
current_thread_ro_unchecked(void)
{
	return get_thread_ro_unchecked(current_thread());
}

thread_ro_t
current_thread_ro(void)
{
	return get_thread_ro(current_thread());
}

void
clear_thread_ro_proc(thread_t th)
{
	thread_ro_t tro = get_thread_ro(th);

	zalloc_ro_clear_field(ZONE_ID_THREAD_RO, tro, tro_proc);
}

struct uthread *
get_bsdthread_info(thread_t th)
{
	return (struct uthread *)((uintptr_t)th + sizeof(struct thread));
}

thread_t
get_machthread(struct uthread *uth)
{
	return (struct thread *)((uintptr_t)uth - sizeof(struct thread));
}

/*
 * This is used to remember any FS error from VNOP_PAGEIN code when
 * invoked under vm_fault(). The value is an errno style value. It can
 * be retrieved by exception handlers using thread_get_state().
 */
void
set_thread_pagein_error(thread_t th, int error)
{
	assert(th == current_thread());
	if (error == 0 || th->t_pagein_error == 0) {
		th->t_pagein_error = error;
	}
}

#if defined(__x86_64__)
/*
 * Returns non-zero if the thread has a non-NULL task
 * and that task has an LDT.
 */
int
thread_task_has_ldt(thread_t th)
{
	task_t task = get_threadtask(th);
	return task && task->i386_ldt != 0;
}
#endif /* __x86_64__ */

/*
 * XXX
 */
int get_thread_lock_count(thread_t th);         /* forced forward */
int
get_thread_lock_count(thread_t th __unused)
{
	/*
	 * TODO: one day: resurect counting locks held to disallow
	 *       holding locks across upcalls.
	 *
	 *       never worked on arm.
	 */
	return 0;
}

/*
 * Returns a thread reference.
 */
thread_t
get_firstthread(task_t task)
{
	thread_t thread = THREAD_NULL;
	task_lock(task);

	if (!task->active) {
		task_unlock(task);
		return THREAD_NULL;
	}

	thread = (thread_t)(void *)queue_first(&task->threads);

	if (queue_end(&task->threads, (queue_entry_t)thread)) {
		task_unlock(task);
		return THREAD_NULL;
	}

	thread_reference(thread);
	task_unlock(task);
	return thread;
}

kern_return_t
get_signalact(
	task_t          task,
	thread_t        *result_out,
	int                     setast)
{
	kern_return_t   result = KERN_SUCCESS;
	thread_t                inc, thread = THREAD_NULL;

	task_lock(task);

	if (!task->active) {
		task_unlock(task);

		return KERN_FAILURE;
	}

	for (inc  = (thread_t)(void *)queue_first(&task->threads);
	    !queue_end(&task->threads, (queue_entry_t)inc);) {
		thread_mtx_lock(inc);
		if (inc->active &&
		    (inc->sched_flags & TH_SFLAG_ABORTED_MASK) != TH_SFLAG_ABORT) {
			thread = inc;
			break;
		}
		thread_mtx_unlock(inc);

		inc = (thread_t)(void *)queue_next(&inc->task_threads);
	}

	if (result_out) {
		*result_out = thread;
	}

	if (thread) {
		if (setast) {
			act_set_astbsd(thread);
		}

		thread_mtx_unlock(thread);
	} else {
		result = KERN_FAILURE;
	}

	task_unlock(task);

	return result;
}


kern_return_t
check_actforsig(
	task_t                  task,
	thread_t                thread,
	int                             setast)
{
	kern_return_t   result = KERN_FAILURE;
	thread_t                inc;

	task_lock(task);

	if (!task->active) {
		task_unlock(task);

		return KERN_FAILURE;
	}

	for (inc  = (thread_t)(void *)queue_first(&task->threads);
	    !queue_end(&task->threads, (queue_entry_t)inc);) {
		if (inc == thread) {
			thread_mtx_lock(inc);

			if (inc->active &&
			    (inc->sched_flags & TH_SFLAG_ABORTED_MASK) != TH_SFLAG_ABORT) {
				result = KERN_SUCCESS;
				break;
			}

			thread_mtx_unlock(inc);
			break;
		}

		inc = (thread_t)(void *)queue_next(&inc->task_threads);
	}

	if (result == KERN_SUCCESS) {
		if (setast) {
			act_set_astbsd(thread);
		}

		thread_mtx_unlock(thread);
	}

	task_unlock(task);

	return result;
}

ledger_t
get_task_ledger(task_t t)
{
	return t->ledger;
}

/*
 * This is only safe to call from a thread executing in
 * in the task's context or if the task is locked. Otherwise,
 * the map could be switched for the task (and freed) before
 * we go to return it here.
 */
vm_map_t
get_task_map(task_t t)
{
	return t->map;
}

vm_map_t
get_task_map_reference(task_t t)
{
	vm_map_t m;

	if (t == NULL) {
		return VM_MAP_NULL;
	}

	task_lock(t);
	if (!t->active) {
		task_unlock(t);
		return VM_MAP_NULL;
	}
	m = t->map;
	vm_map_reference(m);
	task_unlock(t);
	return m;
}

/*
 *
 */
ipc_space_t
get_task_ipcspace(task_t t)
{
	return t->itk_space;
}

int
get_task_numacts(task_t t)
{
	return t->thread_count;
}

/* does this machine need  64bit register set for signal handler */
int
is_64signalregset(void)
{
	if (task_has_64Bit_data(current_task())) {
		return 1;
	}

	return 0;
}

/*
 * Swap in a new map for the task/thread pair; the old map reference is
 * returned. Also does a pmap switch if thread provided is current thread.
 */
vm_map_t
swap_task_map(task_t task, thread_t thread, vm_map_t map)
{
	vm_map_t old_map;
	boolean_t doswitch = (thread == current_thread()) ? TRUE : FALSE;

	if (task != get_threadtask(thread)) {
		panic("swap_task_map");
	}

	task_lock(task);
	mp_disable_preemption();

	old_map = task->map;
	thread->map = task->map = map;
	vm_commit_pagezero_status(map);

	if (doswitch) {
		PMAP_SWITCH_USER(thread, map, cpu_number());
	}
	mp_enable_preemption();
	task_unlock(task);

	return old_map;
}

/*
 *
 * This is only safe to call from a thread executing in
 * in the task's context or if the task is locked. Otherwise,
 * the map could be switched for the task (and freed) before
 * we go to return it here.
 */
pmap_t
get_task_pmap(task_t t)
{
	return t->map->pmap;
}

/*
 *
 */
uint64_t
get_task_resident_size(task_t task)
{
	uint64_t val;

	ledger_get_balance(task->ledger, task_ledgers.phys_mem, (ledger_amount_t *) &val);
	return val;
}

uint64_t
get_task_compressed(task_t task)
{
	uint64_t val;

	ledger_get_balance(task->ledger, task_ledgers.internal_compressed, (ledger_amount_t *) &val);
	return val;
}

uint64_t
get_task_resident_max(task_t task)
{
	uint64_t val;

	ledger_get_lifetime_max(task->ledger, task_ledgers.phys_mem, (ledger_amount_t *) &val);
	return val;
}

/*
 * Get the balance for a given field in the task ledger.
 * Returns 0 if the entry is invalid.
 */
static uint64_t
get_task_ledger_balance(task_t task, int entry)
{
	ledger_amount_t balance = 0;

	ledger_get_balance(task->ledger, entry, &balance);
	return balance;
}

uint64_t
get_task_purgeable_size(task_t task)
{
	kern_return_t ret;
	ledger_amount_t balance = 0;
	uint64_t volatile_size = 0;

	ret = ledger_get_balance(task->ledger, task_ledgers.purgeable_volatile, &balance);
	if (ret != KERN_SUCCESS) {
		return 0;
	}

	volatile_size += balance;

	ret = ledger_get_balance(task->ledger, task_ledgers.purgeable_volatile_compressed, &balance);
	if (ret != KERN_SUCCESS) {
		return 0;
	}

	volatile_size += balance;

	return volatile_size;
}

/*
 *
 */
uint64_t
get_task_phys_footprint(task_t task)
{
	return get_task_ledger_balance(task, task_ledgers.phys_footprint);
}

#if CONFIG_LEDGER_INTERVAL_MAX
/*
 *
 */
uint64_t
get_task_phys_footprint_interval_max(task_t task, int reset)
{
	kern_return_t ret;
	ledger_amount_t max;

	ret = ledger_get_interval_max(task->ledger, task_ledgers.phys_footprint, &max, reset);

	if (KERN_SUCCESS == ret) {
		return max;
	}

	return 0;
}
#endif /* CONFIG_LEDGER_INTERVAL_MAX */

/*
 *
 */
uint64_t
get_task_phys_footprint_lifetime_max(task_t task)
{
	kern_return_t ret;
	ledger_amount_t max;

	ret = ledger_get_lifetime_max(task->ledger, task_ledgers.phys_footprint, &max);

	if (KERN_SUCCESS == ret) {
		return max;
	}

	return 0;
}

/*
 *
 */
uint64_t
get_task_phys_footprint_limit(task_t task)
{
	kern_return_t ret;
	ledger_amount_t max;

	ret = ledger_get_limit(task->ledger, task_ledgers.phys_footprint, &max);
	if (KERN_SUCCESS == ret) {
		return max;
	}

	return 0;
}

uint64_t
get_task_internal(task_t task)
{
	return get_task_ledger_balance(task, task_ledgers.internal);
}

uint64_t
get_task_internal_compressed(task_t task)
{
	return get_task_ledger_balance(task, task_ledgers.internal_compressed);
}

uint64_t
get_task_purgeable_nonvolatile(task_t task)
{
	return get_task_ledger_balance(task, task_ledgers.purgeable_nonvolatile);
}

uint64_t
get_task_purgeable_nonvolatile_compressed(task_t task)
{
	return get_task_ledger_balance(task, task_ledgers.purgeable_nonvolatile_compressed);
}

uint64_t
get_task_alternate_accounting(task_t task)
{
	return get_task_ledger_balance(task, task_ledgers.alternate_accounting);
}

uint64_t
get_task_alternate_accounting_compressed(task_t task)
{
	return get_task_ledger_balance(task, task_ledgers.alternate_accounting_compressed);
}

uint64_t
get_task_page_table(task_t task)
{
	return get_task_ledger_balance(task, task_ledgers.page_table);
}

#if CONFIG_FREEZE
uint64_t
get_task_frozen_to_swap(task_t task)
{
	return get_task_ledger_balance(task, task_ledgers.frozen_to_swap);
}
#endif /* CONFIG_FREEZE */

uint64_t
get_task_iokit_mapped(task_t task)
{
	return get_task_ledger_balance(task, task_ledgers.iokit_mapped);
}

uint64_t
get_task_network_nonvolatile(task_t task)
{
	return get_task_ledger_balance(task, task_ledgers.network_nonvolatile);
}

uint64_t
get_task_network_nonvolatile_compressed(task_t task)
{
	return get_task_ledger_balance(task, task_ledgers.network_nonvolatile_compressed);
}

uint64_t
get_task_wired_mem(task_t task)
{
	return get_task_ledger_balance(task, task_ledgers.wired_mem);
}

uint64_t
get_task_tagged_footprint(task_t task)
{
	kern_return_t ret;
	ledger_amount_t credit, debit;

	ret = ledger_get_entries(task->ledger, task_ledgers.tagged_footprint, &credit, &debit);
	if (KERN_SUCCESS == ret) {
		return credit - debit;
	}

	return 0;
}

uint64_t
get_task_tagged_footprint_compressed(task_t task)
{
	kern_return_t ret;
	ledger_amount_t credit, debit;

	ret = ledger_get_entries(task->ledger, task_ledgers.tagged_footprint_compressed, &credit, &debit);
	if (KERN_SUCCESS == ret) {
		return credit - debit;
	}

	return 0;
}

uint64_t
get_task_media_footprint(task_t task)
{
	kern_return_t ret;
	ledger_amount_t credit, debit;

	ret = ledger_get_entries(task->ledger, task_ledgers.media_footprint, &credit, &debit);
	if (KERN_SUCCESS == ret) {
		return credit - debit;
	}

	return 0;
}

uint64_t
get_task_media_footprint_compressed(task_t task)
{
	kern_return_t ret;
	ledger_amount_t credit, debit;

	ret = ledger_get_entries(task->ledger, task_ledgers.media_footprint_compressed, &credit, &debit);
	if (KERN_SUCCESS == ret) {
		return credit - debit;
	}

	return 0;
}

uint64_t
get_task_graphics_footprint(task_t task)
{
	kern_return_t ret;
	ledger_amount_t credit, debit;

	ret = ledger_get_entries(task->ledger, task_ledgers.graphics_footprint, &credit, &debit);
	if (KERN_SUCCESS == ret) {
		return credit - debit;
	}

	return 0;
}


uint64_t
get_task_graphics_footprint_compressed(task_t task)
{
	kern_return_t ret;
	ledger_amount_t credit, debit;

	ret = ledger_get_entries(task->ledger, task_ledgers.graphics_footprint_compressed, &credit, &debit);
	if (KERN_SUCCESS == ret) {
		return credit - debit;
	}

	return 0;
}

uint64_t
get_task_neural_footprint(task_t task)
{
	kern_return_t ret;
	ledger_amount_t credit, debit;

	ret = ledger_get_entries(task->ledger, task_ledgers.neural_footprint, &credit, &debit);
	if (KERN_SUCCESS == ret) {
		return credit - debit;
	}

	return 0;
}

uint64_t
get_task_neural_footprint_compressed(task_t task)
{
	kern_return_t ret;
	ledger_amount_t credit, debit;

	ret = ledger_get_entries(task->ledger, task_ledgers.neural_footprint_compressed, &credit, &debit);
	if (KERN_SUCCESS == ret) {
		return credit - debit;
	}

	return 0;
}

uint64_t
get_task_neural_nofootprint_total(task_t task)
{
	kern_return_t ret;
	ledger_amount_t credit, debit;

	ret = ledger_get_entries(task->ledger, task_ledgers.neural_nofootprint_total, &credit, &debit);
	if (KERN_SUCCESS == ret) {
		return credit - debit;
	}

	return 0;
}

#if CONFIG_LEDGER_INTERVAL_MAX
uint64_t
get_task_neural_nofootprint_total_interval_max(task_t task, int reset)
{
	kern_return_t ret;
	ledger_amount_t max;

	ret = ledger_get_interval_max(task->ledger, task_ledgers.neural_nofootprint_total, &max, reset);

	if (KERN_SUCCESS == ret) {
		return max;
	}

	return 0;
}
#endif /* CONFIG_LEDGER_INTERVAL_MAX */

uint64_t
get_task_neural_nofootprint_total_lifetime_max(task_t task)
{
	kern_return_t ret;
	ledger_amount_t max;

	ret = ledger_get_lifetime_max(task->ledger, task_ledgers.neural_nofootprint_total, &max);

	if (KERN_SUCCESS == ret) {
		return max;
	}

	return 0;
}

uint64_t
get_task_cpu_time(task_t task)
{
	return get_task_ledger_balance(task, task_ledgers.cpu_time);
}

uint32_t
get_task_loadTag(task_t task)
{
	return os_atomic_load(&task->loadTag, relaxed);
}

uint32_t
set_task_loadTag(task_t task, uint32_t loadTag)
{
	return os_atomic_xchg(&task->loadTag, loadTag, relaxed);
}


task_t
get_threadtask(thread_t th)
{
	return get_thread_ro(th)->tro_task;
}

task_t
get_threadtask_early(thread_t th)
{
	if (__improbable(startup_phase < STARTUP_SUB_EARLY_BOOT)) {
		if (th == THREAD_NULL || th->t_tro == NULL) {
			return TASK_NULL;
		}
	}
	return get_threadtask(th);
}

/*
 *
 */
vm_map_offset_t
get_map_min(
	vm_map_t        map)
{
	return vm_map_min(map);
}

/*
 *
 */
vm_map_offset_t
get_map_max(
	vm_map_t        map)
{
	return vm_map_max(map);
}
vm_map_size_t
get_vmmap_size(
	vm_map_t        map)
{
	return vm_map_adjusted_size(map);
}
int
get_task_page_size(
	task_t task)
{
	return vm_map_page_size(task->map);
}

#if CONFIG_COREDUMP

static int
get_vmsubmap_entries(
	vm_map_t        map,
	vm_object_offset_t      start,
	vm_object_offset_t      end)
{
	int     total_entries = 0;
	vm_map_entry_t  entry;

	if (not_in_kdp) {
		vm_map_lock(map);
	}
	entry = vm_map_first_entry(map);
	while ((entry != vm_map_to_entry(map)) && (entry->vme_start < start)) {
		entry = entry->vme_next;
	}

	while ((entry != vm_map_to_entry(map)) && (entry->vme_start < end)) {
		if (entry->is_sub_map) {
			total_entries +=
			    get_vmsubmap_entries(VME_SUBMAP(entry),
			    VME_OFFSET(entry),
			    (VME_OFFSET(entry) +
			    entry->vme_end -
			    entry->vme_start));
		} else {
			total_entries += 1;
		}
		entry = entry->vme_next;
	}
	if (not_in_kdp) {
		vm_map_unlock(map);
	}
	return total_entries;
}

int
get_vmmap_entries(
	vm_map_t        map)
{
	int     total_entries = 0;
	vm_map_entry_t  entry;

	if (not_in_kdp) {
		vm_map_lock(map);
	}
	entry = vm_map_first_entry(map);

	while (entry != vm_map_to_entry(map)) {
		if (entry->is_sub_map) {
			total_entries +=
			    get_vmsubmap_entries(VME_SUBMAP(entry),
			    VME_OFFSET(entry),
			    (VME_OFFSET(entry) +
			    entry->vme_end -
			    entry->vme_start));
		} else {
			total_entries += 1;
		}
		entry = entry->vme_next;
	}
	if (not_in_kdp) {
		vm_map_unlock(map);
	}
	return total_entries;
}
#endif /* CONFIG_COREDUMP */

int
get_task_userstop(
	task_t task)
{
	return task->user_stop_count;
}

int
get_thread_userstop(
	thread_t th)
{
	return th->user_stop_count;
}

boolean_t
get_task_pidsuspended(
	task_t task)
{
	return task->pidsuspended;
}

boolean_t
get_task_frozen(
	task_t task)
{
	return task->frozen;
}

boolean_t
thread_should_abort(
	thread_t th)
{
	return (th->sched_flags & TH_SFLAG_ABORTED_MASK) == TH_SFLAG_ABORT;
}

/*
 * This routine is like thread_should_abort() above.  It checks to
 * see if the current thread is aborted.  But unlike above, it also
 * checks to see if thread is safely aborted.  If so, it returns
 * that fact, and clears the condition (safe aborts only should
 * have a single effect, and a poll of the abort status
 * qualifies.
 */
boolean_t
current_thread_aborted(
	void)
{
	thread_t th = current_thread();
	spl_t s;

	if ((th->sched_flags & TH_SFLAG_ABORTED_MASK) == TH_SFLAG_ABORT &&
	    (th->options & TH_OPT_INTMASK) != THREAD_UNINT) {
		return TRUE;
	}
	if (th->sched_flags & TH_SFLAG_ABORTSAFELY) {
		s = splsched();
		thread_lock(th);
		if (th->sched_flags & TH_SFLAG_ABORTSAFELY) {
			th->sched_flags &= ~TH_SFLAG_ABORTED_MASK;
		}
		thread_unlock(th);
		splx(s);
	}
	return FALSE;
}

void
task_act_iterate_wth_args(
	task_t                  task,
	void                    (*func_callback)(thread_t, void *),
	void                    *func_arg)
{
	thread_t        inc;

	task_lock(task);

	for (inc  = (thread_t)(void *)queue_first(&task->threads);
	    !queue_end(&task->threads, (queue_entry_t)inc);) {
		(void) (*func_callback)(inc, func_arg);
		inc = (thread_t)(void *)queue_next(&inc->task_threads);
	}

	task_unlock(task);
}

#include <sys/bsdtask_info.h>

void
fill_taskprocinfo(task_t task, struct proc_taskinfo_internal * ptinfo)
{
	vm_map_t map;
	task_absolutetime_info_data_t   tinfo;
	thread_t thread;
	uint32_t cswitch = 0, numrunning = 0;
	uint32_t syscalls_unix = 0;
	uint32_t syscalls_mach = 0;

	task_lock(task);

	map = (task == kernel_task)? kernel_map: task->map;

	ptinfo->pti_virtual_size  = vm_map_adjusted_size(map);
	ledger_get_balance(task->ledger, task_ledgers.phys_mem, (ledger_amount_t *) &ptinfo->pti_resident_size);

	ptinfo->pti_policy = ((task != kernel_task)?
	    POLICY_TIMESHARE: POLICY_RR);

	queue_iterate(&task->threads, thread, thread_t, task_threads) {
		spl_t x;

		if (thread->options & TH_OPT_IDLE_THREAD) {
			continue;
		}

		x = splsched();
		thread_lock(thread);

		if ((thread->state & TH_RUN) == TH_RUN) {
			numrunning++;
		}
		cswitch += thread->c_switch;

		syscalls_unix += thread->syscalls_unix;
		syscalls_mach += thread->syscalls_mach;

		thread_unlock(thread);
		splx(x);
	}

	struct recount_times_mach term_times = recount_task_terminated_times(task);
	struct recount_times_mach total_times = recount_task_times(task);

	tinfo.threads_user = total_times.rtm_user - term_times.rtm_user;
	tinfo.threads_system = total_times.rtm_system - term_times.rtm_system;
	ptinfo->pti_threads_system = tinfo.threads_system;
	ptinfo->pti_threads_user = tinfo.threads_user;

	ptinfo->pti_total_system = total_times.rtm_system;
	ptinfo->pti_total_user = total_times.rtm_user;

	ptinfo->pti_faults = (int32_t) MIN(counter_load(&task->faults), INT32_MAX);
	ptinfo->pti_pageins = (int32_t) MIN(counter_load(&task->pageins), INT32_MAX);
	ptinfo->pti_cow_faults = (int32_t) MIN(counter_load(&task->cow_faults), INT32_MAX);
	ptinfo->pti_messages_sent = (int32_t) MIN(counter_load(&task->messages_sent), INT32_MAX);
	ptinfo->pti_messages_received = (int32_t) MIN(counter_load(&task->messages_received), INT32_MAX);
	ptinfo->pti_syscalls_mach = (int32_t) MIN(task->syscalls_mach + syscalls_mach, INT32_MAX);
	ptinfo->pti_syscalls_unix = (int32_t) MIN(task->syscalls_unix + syscalls_unix, INT32_MAX);
	ptinfo->pti_csw = (int32_t) MIN(task->c_switch + cswitch, INT32_MAX);
	ptinfo->pti_threadnum = task->thread_count;
	ptinfo->pti_numrunning = numrunning;
	ptinfo->pti_priority = task->priority;

	task_unlock(task);
}

int
fill_taskthreadinfo(task_t task, uint64_t thaddr, bool thuniqueid, struct proc_threadinfo_internal * ptinfo, void * vpp, int *vidp)
{
	thread_t  thact;
	int err = 0;
	mach_msg_type_number_t count;
	thread_basic_info_data_t basic_info;
	kern_return_t kret;
	uint64_t addr = 0;

	task_lock(task);

	for (thact  = (thread_t)(void *)queue_first(&task->threads);
	    !queue_end(&task->threads, (queue_entry_t)thact);) {
		addr = (thuniqueid) ? thact->thread_id : thact->machine.cthread_self;
		if (addr == thaddr) {
			count = THREAD_BASIC_INFO_COUNT;
			if ((kret = thread_info_internal(thact, THREAD_BASIC_INFO, (thread_info_t)&basic_info, &count)) != KERN_SUCCESS) {
				err = 1;
				goto out;
			}
			ptinfo->pth_user_time = (((uint64_t)basic_info.user_time.seconds * NSEC_PER_SEC) + ((uint64_t)basic_info.user_time.microseconds * NSEC_PER_USEC));
			ptinfo->pth_system_time = (((uint64_t)basic_info.system_time.seconds * NSEC_PER_SEC) + ((uint64_t)basic_info.system_time.microseconds * NSEC_PER_USEC));

			ptinfo->pth_cpu_usage = basic_info.cpu_usage;
			ptinfo->pth_policy = basic_info.policy;
			ptinfo->pth_run_state = basic_info.run_state;
			ptinfo->pth_flags = basic_info.flags;
			ptinfo->pth_sleep_time = basic_info.sleep_time;
			ptinfo->pth_curpri = thact->sched_pri;
			ptinfo->pth_priority = thact->base_pri;
			ptinfo->pth_maxpriority = thact->max_priority;

			if (vpp != NULL) {
				bsd_threadcdir(get_bsdthread_info(thact), vpp, vidp);
			}
			bsd_getthreadname(get_bsdthread_info(thact), ptinfo->pth_name);
			err = 0;
			goto out;
		}
		thact = (thread_t)(void *)queue_next(&thact->task_threads);
	}
	err = 1;

out:
	task_unlock(task);
	return err;
}

int
fill_taskthreadlist(task_t task, void * buffer, int thcount, bool thuniqueid)
{
	int numthr = 0;
	thread_t thact;
	uint64_t * uptr;
	uint64_t  thaddr;

	uptr = (uint64_t *)buffer;

	task_lock(task);

	for (thact  = (thread_t)(void *)queue_first(&task->threads);
	    !queue_end(&task->threads, (queue_entry_t)thact);) {
		thaddr = (thuniqueid) ? thact->thread_id : thact->machine.cthread_self;
		*uptr++ = thaddr;
		numthr++;
		if (numthr >= thcount) {
			goto out;
		}
		thact = (thread_t)(void *)queue_next(&thact->task_threads);
	}

out:
	task_unlock(task);
	return (int)(numthr * sizeof(uint64_t));
}

int
fill_taskthreadschedinfo(task_t task, uint64_t thread_id, struct proc_threadschedinfo_internal *thread_sched_info)
{
	int err = 0;

	thread_t thread = current_thread();

	/*
	 * Looking up threads is pretty expensive and not realtime-safe
	 * right now, requiring locking the task and iterating over all
	 * threads. As long as that is the case, we officially only
	 * support getting this info for the current thread.
	 */
	if (task != current_task() || thread_id != thread->thread_id) {
		return -1;
	}

#if SCHED_HYGIENE_DEBUG
	absolutetime_to_nanoseconds(thread->machine.int_time_mt, &thread_sched_info->int_time_ns);
#else
	(void)thread;
	thread_sched_info->int_time_ns = 0;
#endif

	return err;
}

int
get_numthreads(task_t task)
{
	return task->thread_count;
}

/*
 * Gather the various pieces of info about the designated task,
 * and collect it all into a single rusage_info.
 */
int
fill_task_rusage(task_t task, rusage_info_current *ri)
{
	struct task_power_info powerinfo;

	assert(task != TASK_NULL);
	task_lock(task);

	struct task_power_info_extra extra = { 0 };
	task_power_info_locked(task, &powerinfo, NULL, NULL, &extra);
	ri->ri_pkg_idle_wkups = powerinfo.task_platform_idle_wakeups;
	ri->ri_interrupt_wkups = powerinfo.task_interrupt_wakeups;
	ri->ri_user_time = powerinfo.total_user;
	ri->ri_system_time = powerinfo.total_system;
	ri->ri_runnable_time = extra.runnable_time;
	ri->ri_cycles = extra.cycles;
	ri->ri_instructions = extra.instructions;
	ri->ri_pcycles = extra.pcycles;
	ri->ri_pinstructions = extra.pinstructions;
	ri->ri_user_ptime = extra.user_ptime;
	ri->ri_system_ptime = extra.system_ptime;
	ri->ri_energy_nj = extra.energy;
	ri->ri_penergy_nj = extra.penergy;
	ri->ri_secure_time_in_system = extra.secure_time;
	ri->ri_secure_ptime_in_system = extra.secure_ptime;

	ri->ri_phys_footprint = get_task_phys_footprint(task);
	ledger_get_balance(task->ledger, task_ledgers.phys_mem,
	    (ledger_amount_t *)&ri->ri_resident_size);
	ri->ri_wired_size = get_task_wired_mem(task);

	ledger_get_balance(task->ledger, task_ledgers.neural_nofootprint_total,
	    (ledger_amount_t *)&ri->ri_neural_footprint);
	ri->ri_pageins = counter_load(&task->pageins);

	task_unlock(task);
	return 0;
}

void
fill_task_billed_usage(task_t task __unused, rusage_info_current *ri)
{
	bank_billed_balance_safe(task, &ri->ri_billed_system_time, &ri->ri_billed_energy);
	bank_serviced_balance_safe(task, &ri->ri_serviced_system_time, &ri->ri_serviced_energy);
}

int
fill_task_io_rusage(task_t task, rusage_info_current *ri)
{
	assert(task != TASK_NULL);
	task_lock(task);

	if (task->task_io_stats) {
		ri->ri_diskio_bytesread = task->task_io_stats->disk_reads.size;
		ri->ri_diskio_byteswritten = (task->task_io_stats->total_io.size - task->task_io_stats->disk_reads.size);
	} else {
		/* I/O Stats unavailable */
		ri->ri_diskio_bytesread = 0;
		ri->ri_diskio_byteswritten = 0;
	}
	task_unlock(task);
	return 0;
}

int
fill_task_qos_rusage(task_t task, rusage_info_current *ri)
{
	thread_t thread;

	assert(task != TASK_NULL);
	task_lock(task);

	/* Rollup QoS time of all the threads to task */
	queue_iterate(&task->threads, thread, thread_t, task_threads) {
		if (thread->options & TH_OPT_IDLE_THREAD) {
			continue;
		}

		thread_update_qos_cpu_time(thread);
	}
	ri->ri_cpu_time_qos_default = task->cpu_time_eqos_stats.cpu_time_qos_default;
	ri->ri_cpu_time_qos_maintenance = task->cpu_time_eqos_stats.cpu_time_qos_maintenance;
	ri->ri_cpu_time_qos_background = task->cpu_time_eqos_stats.cpu_time_qos_background;
	ri->ri_cpu_time_qos_utility = task->cpu_time_eqos_stats.cpu_time_qos_utility;
	ri->ri_cpu_time_qos_legacy = task->cpu_time_eqos_stats.cpu_time_qos_legacy;
	ri->ri_cpu_time_qos_user_initiated = task->cpu_time_eqos_stats.cpu_time_qos_user_initiated;
	ri->ri_cpu_time_qos_user_interactive = task->cpu_time_eqos_stats.cpu_time_qos_user_interactive;

	task_unlock(task);
	return 0;
}

uint64_t
get_task_logical_writes(task_t task, bool external)
{
	assert(task != TASK_NULL);
	struct ledger_entry_info lei;
	int entry = external ? task_ledgers.logical_writes_to_external :
	    task_ledgers.logical_writes;

	task_lock(task);
	ledger_get_entry_info(task->ledger, entry, &lei);
	task_unlock(task);

	return lei.lei_balance;
}

uint64_t
get_task_dispatchqueue_serialno_offset(task_t task)
{
	uint64_t dq_serialno_offset = 0;
	void *bsd_info = get_bsdtask_info(task);

	if (bsd_info) {
		dq_serialno_offset = get_dispatchqueue_serialno_offset_from_proc(bsd_info);
	}

	return dq_serialno_offset;
}

uint64_t
get_task_dispatchqueue_label_offset(task_t task)
{
	uint64_t dq_label_offset = 0;
	void *bsd_info = get_bsdtask_info(task);

	if (bsd_info) {
		dq_label_offset = get_dispatchqueue_label_offset_from_proc(bsd_info);
	}

	return dq_label_offset;
}

uint64_t
get_task_uniqueid(task_t task)
{
	void *bsd_info = get_bsdtask_info(task);

	if (bsd_info) {
		return proc_uniqueid_task(bsd_info, task);
	} else {
		return UINT64_MAX;
	}
}

int
get_task_version(task_t task)
{
	void *bsd_info = get_bsdtask_info(task);

	if (bsd_info) {
		return proc_pidversion(bsd_info);
	} else {
		return INT_MAX;
	}
}

#if CONFIG_MACF
struct label *
get_task_crash_label(task_t task)
{
	return task->crash_label;
}

void
set_task_crash_label(task_t task, struct label *label)
{
	task->crash_label = label;
}
#endif

int
fill_taskipctableinfo(task_t task, uint32_t *table_size, uint32_t *table_free)
{
	ipc_space_t space = task->itk_space;
	if (space == NULL) {
		return -1;
	}

	is_read_lock(space);
	if (!is_active(space)) {
		is_read_unlock(space);
		return -1;
	}

	*table_size = ipc_entry_table_count(is_active_table(space));
	*table_free = space->is_table_free;

	is_read_unlock(space);

	return 0;
}

int
get_task_cdhash(task_t task, char cdhash[static CS_CDHASH_LEN])
{
	int result = 0;
	void *bsd_info = NULL;

	task_lock(task);
	bsd_info = get_bsdtask_info(task);
	result = bsd_info ? proc_getcdhash(bsd_info, cdhash) : ESRCH;
	task_unlock(task);

	return result;
}

bool
current_thread_in_kernel_fault(void)
{
	if (current_thread()->recover) {
		return true;
	}
	return false;
}

/* moved from ubc_subr.c */
int
mach_to_bsd_errno(kern_return_t mach_err)
{
	switch (mach_err) {
	case KERN_SUCCESS:
		return 0;

	case KERN_INVALID_ADDRESS:
	case KERN_INVALID_ARGUMENT:
	case KERN_NOT_IN_SET:
	case KERN_INVALID_NAME:
	case KERN_INVALID_TASK:
	case KERN_INVALID_RIGHT:
	case KERN_INVALID_VALUE:
	case KERN_INVALID_CAPABILITY:
	case KERN_INVALID_HOST:
	case KERN_MEMORY_PRESENT:
	case KERN_INVALID_PROCESSOR_SET:
	case KERN_INVALID_POLICY:
	case KERN_ALREADY_WAITING:
	case KERN_DEFAULT_SET:
	case KERN_EXCEPTION_PROTECTED:
	case KERN_INVALID_LEDGER:
	case KERN_INVALID_MEMORY_CONTROL:
	case KERN_INVALID_SECURITY:
	case KERN_NOT_DEPRESSED:
	case KERN_LOCK_OWNED:
	case KERN_LOCK_OWNED_SELF:
		return EINVAL;

	case KERN_NOT_RECEIVER:
	case KERN_NO_ACCESS:
	case KERN_POLICY_STATIC:
		return EACCES;

	case KERN_NO_SPACE:
	case KERN_RESOURCE_SHORTAGE:
	case KERN_UREFS_OVERFLOW:
	case KERN_INVALID_OBJECT:
		return ENOMEM;

	case KERN_MEMORY_FAILURE:
	case KERN_MEMORY_ERROR:
	case KERN_PROTECTION_FAILURE:
		return EFAULT;

	case KERN_POLICY_LIMIT:
	case KERN_CODESIGN_ERROR:
	case KERN_DENIED:
		return EPERM;

	case KERN_ALREADY_IN_SET:
	case KERN_NAME_EXISTS:
	case KERN_RIGHT_EXISTS:
		return EEXIST;

	case KERN_ABORTED:
		return EINTR;

	case KERN_TERMINATED:
	case KERN_LOCK_SET_DESTROYED:
	case KERN_LOCK_UNSTABLE:
	case KERN_SEMAPHORE_DESTROYED:
	case KERN_NOT_FOUND:
	case KERN_NOT_WAITING:
		return ENOENT;

	case KERN_RPC_SERVER_TERMINATED:
		return ECONNRESET;

	case KERN_NOT_SUPPORTED:
		return ENOTSUP;

	case KERN_NODE_DOWN:
		return ENETDOWN;

	case KERN_OPERATION_TIMED_OUT:
		return ETIMEDOUT;

	default:
		return EIO; /* 5 == KERN_FAILURE */
	}
}

/*
 * Return the mach return value corresponding to a given BSD errno.
 */
kern_return_t
kern_return_for_errno(int bsd_errno)
{
	switch (bsd_errno) {
	case 0:
		return KERN_SUCCESS;
	case EIO:
	case EACCES:
	case ENOMEM:
	case EFAULT:
		return KERN_MEMORY_ERROR;

	case EINVAL:
		return KERN_INVALID_ARGUMENT;

	case ETIMEDOUT:
	case EBUSY:
		return KERN_OPERATION_TIMED_OUT;

	case ECONNRESET:
		return KERN_RPC_SERVER_TERMINATED;

	case ENOTSUP:
		return KERN_NOT_SUPPORTED;

	case ENETDOWN:
		return KERN_NODE_DOWN;

	case ENOENT:
		return KERN_NOT_FOUND;

	case EINTR:
		return KERN_ABORTED;

	case EPERM:
		return KERN_DENIED;

	case EEXIST:
		return KERN_ALREADY_IN_SET;

	default:
		return KERN_FAILURE;
	}
}
