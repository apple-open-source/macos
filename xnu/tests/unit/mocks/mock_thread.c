/*
 * Copyright (c) 2000-2025 Apple Inc. All rights reserved.
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

#include "std_safe.h"
#include "dt_proxy.h"
#include "mock_thread.h"
#include "unit_test_utils.h"
#include "mock_thread.h"

#include "fibers/fibers.h"
#include "fibers/mutex.h"
#include "fibers/condition.h"
#include "fibers/rwlock.h"
#include "fibers/random.h"
#include "fibers/checker.h"

#include <arm/cpu_data_internal.h> // for cpu_data
#include <kern/thread.h>
#include <kern/lock_mtx.h>
#include <kern/lock_group.h>
#include <kern/compact_id.h>
#include <kern/task.h>
#include <vm/vm_object_xnu.h>

#define UNDEFINED_MOCK \
	raw_printf("%s: WIP mock, this should not be called\n", __FUNCTION__); \
	print_current_backtrace();

/*
 * Unit tests that wants to use fibers must redefine this global with a value not 0.
 * The test executable should not do this directly, instead it should call macro UT_USE_FIBERS in its global scope.
 *
 * We use a weak global and not a macro that defines a constructor to avoid initialization code running before such constructor to run
 * with ut_mocks_use_fibers=0 before that the constructor change its value.
 * Switching from the pthread mocks to fibers is not supported, we must be consistent from the very beginning.
 */
int ut_mocks_use_fibers __attribute__((weak)) = 0;

/*
 * Unit tests that wants to use fibers with data race checking must redefine this global with a value not 0.
 * FIBERS_CHECKER=1 as env var will do the same job too.
 */
int ut_fibers_use_data_race_checker __attribute__((weak)) = 0;

/*
 * Unit tests can set this variable to force `lck_rw_lock_shared_to_exclusive` to fail.
 *
 * RANGELOCKINGTODO rdar://150846598 model when to return FALSE
 */
bool ut_mocks_lock_upgrade_fail = 0;

/*
 * This constructor is used to set the configuration variables of the fibers using env vars.
 * The main use case is fuzzing, unit tests should set the variables in the test function or
 * by calling the correspondig macros (UT_FIBERS_*, see mock_thread.h) in their global scope.
 */
__attribute__((constructor))
static void
initialize_fiber_settings(void)
{
	const char *debug_env = getenv("FIBERS_DEBUG");
	if (debug_env != NULL) {
		fibers_debug = atoi(debug_env);
	}

	const char *err_env = getenv("FIBERS_ABORT_ON_ERROR");
	if (err_env != NULL) {
		fibers_abort_on_error = atoi(err_env);
	}

	const char *verbose_env = getenv("FIBERS_LOG");
	if (verbose_env != NULL) {
		fibers_log_level = atoi(verbose_env);
	}

	const char *prob_env = getenv("FIBERS_MAY_YIELD_PROB");
	if (prob_env != NULL) {
		fibers_may_yield_probability = atoi(prob_env);
	}

	const char *checker_env = getenv("FIBERS_CHECK_RACES");
	if (checker_env != NULL) {
#ifndef __BUILDING_WITH_SANCOV_LOAD_STORES__
		raw_printf("==== Fibers data race checker disabled ====\n");
		raw_printf("You cannot enable the data race checker if the FIBERS_PREEMPTION=1 flag was to not used as make parameter.");
		return;
#else
		if (!ut_mocks_use_fibers) {
			raw_printf("==== Fibers data race checker disabled ====\n");
			raw_printf("You cannot enable the data race checker if the test is not using fibers (see UT_USE_FIBERS in the readme).");
			return;
		}
		ut_fibers_use_data_race_checker = atoi(checker_env);
		if (ut_fibers_use_data_race_checker) {
			raw_printf("==== Fibers data race checker enabled ====\n");
		} else {
			raw_printf("==== Fibers data race checker disabled ====\n");
		}
#endif // __BUILDING_WITH_SANCOV_LOAD_STORES__
	}
}

// --------------- proc and thread ------------------

struct proc;
typedef struct proc * proc_t;

extern void init_thread_from_template(thread_t thread);
extern void ctid_table_init(void);
extern void ctid_table_add(thread_t thread);
extern void ctid_table_remove(thread_t thread);
extern void thread_ro_create(task_t parent_task, thread_t th, thread_ro_t tro_tpl);
extern task_t proc_get_task_raw(proc_t proc);
extern void task_zone_init(void);

extern struct compact_id_table ctid_table;
extern lck_grp_t thread_lck_grp;
extern size_t proc_struct_size;
extern proc_t kernproc;

void mock_init_proc(proc_t p, void* (*calloc_call)(size_t, size_t));

// a pointer to this object is kept per thread in thread-local-storage
struct mock_thread {
	struct thread th;
	fiber_t fiber;
	struct mock_thread* wq_next;
	bool interrupts_disabled;
};

struct pthread_mock_event_table_entry {
	event_t ev;
	pthread_cond_t cond;
	// the condition variable is owned by the table and is initialized on the first use of the entry
	bool cond_inited;
};
#define PTHREAD_EVENTS_TABLE_SIZE 1000

struct mock_process_state {
	void *proctask; // buffer for proc and task
	struct proc *main_proc;
	struct task *main_task;
	struct cpu_data cpud;
	struct mock_thread *main_thread;
	uint64_t thread_unique_id;
	uint64_t _faults;
	uint64_t _pageins;
	uint64_t _cow_faults;

	// pthread
	pthread_key_t tls_thread_key;
	pthread_mutex_t interrupts_mutex; // if this mutex is locked interrupts are disabled
	pthread_mutex_t events_mutex; // for all event condition variables
	struct pthread_mock_event_table_entry events[PTHREAD_EVENTS_TABLE_SIZE];
	// !pthread

	// fibers
	int interrupts_disabled;
	// !fibers
};

static void
mock_destroy_thread(void *th_p)
{
	struct mock_thread *mth = (struct mock_thread *)th_p;
	// raw_printf("thread_t finished ctid=%u\n", mth->th.ctid);

	ctid_table_remove(&mth->th);

	free(mth->th.t_tro);
	free(mth);
}

static struct mock_thread *
mock_init_new_thread(struct mock_process_state* s)
{
	struct mock_thread *new_mock_thread = calloc(1, sizeof(struct mock_thread));
	struct thread *new_thread = &new_mock_thread->th;

	if (ut_mocks_use_fibers) {
		new_mock_thread->fiber = fibers_current;
		fibers_current->extra = new_mock_thread;
		fibers_current->extra_cleanup_routine = &mock_destroy_thread;
	} else {
		pthread_setspecific(s->tls_thread_key, new_mock_thread);
	}

	static int mock_init_new_thread_first_call = 1;
	if (mock_init_new_thread_first_call) {
		mock_init_new_thread_first_call = 0;
		compact_id_table_init(&ctid_table);
		ctid_table_init();
	}

	init_thread_from_template(new_thread);

	// maybe call thread_create_internal() ?
	// machine is needed by _enable_preemption_write_count()
	machine_thread_create(new_thread, s->main_task, true);
	new_thread->machine.CpuDatap = &s->cpud;
	new_thread->thread_id = ++s->thread_unique_id;
	//new_thread->ctid = (uint32_t)new_thread->thread_id;
	ctid_table_add(new_thread);

	thread_lock_init(new_thread);
	wake_lock_init(new_thread);

	fake_init_lock(&new_thread->mutex);

	new_thread->t_tro = calloc(1, sizeof(struct thread_ro));
	new_thread->t_tro->tro_owner = new_thread;
	new_thread->t_tro->tro_task = s->main_task;
	new_thread->t_tro->tro_proc = s->main_proc;

	// for the main thread this happens before zalloc init so don't do the following which uses zalloc
	//struct thread_ro tro_tpl = { };
	//thread_ro_create(&s->main_task, new_thread, &tro_tpl);

	new_thread->state = TH_RUN;

	// raw_printf("thread_t created ctid=%u\n", new_thread->ctid);
	return new_mock_thread;
}

void
fake_init_task(task_t new_task)
{
	// can't call task_create_internal() since it does zalloc
	fake_init_lock(&new_task->lock);
	fake_init_lock(&new_task->task_objq_lock);
	queue_init(&new_task->task_objq);
	queue_init(&new_task->threads);
	new_task->suspend_count = 0;
	new_task->thread_count = 0;
	new_task->active_thread_count = 0;
	new_task->user_stop_count = 0;
	new_task->legacy_stop_count = 0;
	new_task->active = TRUE;
	new_task->halting = FALSE;
	new_task->priv_flags = 0;
	new_task->t_flags = 0;
	new_task->t_procflags = 0;
	new_task->t_returnwaitflags = 0;
	new_task->importance = 0;
	new_task->crashed_thread_id = 0;
	new_task->watchports = NULL;
	new_task->t_rr_ranges = NULL;

	new_task->bank_context = NULL;

	new_task->pageins = calloc(1, sizeof(uint64_t));

	fake_init_lock(&new_task->task_objq_lock);
	queue_init(&new_task->task_objq);
}

static void
mock_init_threads_state(struct mock_process_state* s)
{
	//task_zone_init();
	s->proctask = calloc(1, proc_struct_size + sizeof(struct task));
	s->main_proc = (proc_t)s->proctask;
	s->main_task = proc_get_task_raw(s->main_proc);

	memset(s->main_proc, 0, proc_struct_size);
	mock_init_proc(s->main_proc, calloc);
	kernproc = s->main_proc; // set global variable

	memset(s->main_task, 0, sizeof(*s->main_task));
	fake_init_task(s->main_task);
	s->_faults = 0;
	s->main_task->faults = &s->_faults;
	s->_pageins = 0;
	s->main_task->pageins = &s->_pageins;
	s->_cow_faults = 0;
	s->main_task->cow_faults = &s->_cow_faults;

	kernel_task = s->main_task; // without this machine_thread_create allocates

	cpu_data_init(&s->cpud);
	s->thread_unique_id = 100;

	if (!ut_mocks_use_fibers) {
		int ret = pthread_key_create(&s->tls_thread_key, &mock_destroy_thread);
		if (ret != 0) {
			raw_printf("failed pthread_key_create");
			exit(1);
		}

		pthread_mutexattr_t attr;
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
		ret = pthread_mutex_init(&s->interrupts_mutex, &attr);
		if (ret != 0) {
			raw_printf("failed pthread_key_create");
			exit(1);
		}
		pthread_mutexattr_destroy(&attr);

		ret = pthread_mutex_init(&s->events_mutex, NULL);
		if (ret != 0) {
			raw_printf("failed pthread_key_create");
			exit(1);
		}
		memset(&s->events, 0, sizeof(s->events));
	}

	s->main_thread = mock_init_new_thread(s);
}

struct mock_process_state *
get_proc_state(void)
{
	static struct mock_process_state s;
	static bool initialized = false;
	if (!initialized) { // TODO move to fake_kinit.c ?
		initialized = true;
		mock_init_threads_state(&s);
	}
	return &s;
}

struct mock_thread *
get_mock_thread(void)
{
	struct mock_process_state *s = get_proc_state();

	struct mock_thread *mth;
	if (ut_mocks_use_fibers) {
		mth = (struct mock_thread *)fibers_current->extra;
	} else {
		mth = pthread_getspecific(s->tls_thread_key);
	}

	if (mth == NULL) {
		mth = mock_init_new_thread(s);
	}
	return mth;
}

T_MOCK(thread_t,
current_thread_fast, (void))
{
	return &get_mock_thread()->th;
}

T_MOCK(uint32_t,
kauth_cred_getuid, (void* cred))
{
	return 0;
}

// --------------- interrupts disable (spl) ---------------------

T_MOCK(boolean_t,
ml_get_interrupts_enabled, (void))
{
	if (ut_mocks_use_fibers) {
		return get_mock_thread()->interrupts_disabled == 0;
	} else {
		pthread_mutex_t *m = &get_proc_state()->interrupts_mutex;
		int r = pthread_mutex_trylock(m);
		if (r == 0) {
			// it's locked, meaning interrupts are disabled
			pthread_mutex_unlock(m);
			return false;
		}
		PT_QUIET; PT_ASSERT_TRUE(r == EBUSY, "unexpected value in get_interrupts_enabled");
		return true;
	}
}

// original calls DAIF
// interupts disable is mocked by disabling context switches with fiber_t.may_yield_disabled
T_MOCK(boolean_t,
ml_set_interrupts_enabled, (boolean_t enable))
{
	if (ut_mocks_use_fibers) {
		bool prev_interrupts_disabled = get_mock_thread()->interrupts_disabled;

		FIBERS_LOG(FIBERS_LOG_DEBUG, "ml_set_interrupts_enabled: enable=%d, previous state=%d, may_yield_disabled=%d", enable, !get_mock_thread()->interrupts_disabled, fibers_current->may_yield_disabled);

		fibers_may_yield_internal_with_reason(
			(enable ? FIBERS_YIELD_REASON_PREEMPTION_WILL_ENABLE : FIBERS_YIELD_REASON_PREEMPTION_WILL_DISABLE) |
			FIBERS_YIELD_REASON_ERROR_IF(enable != prev_interrupts_disabled));

		// Track the interrupt state per fiber through yield_disabled
		if (enable && prev_interrupts_disabled) {
			get_mock_thread()->interrupts_disabled = false;
			fibers_current->may_yield_disabled--;
		} else if (!enable && !prev_interrupts_disabled) {
			get_mock_thread()->interrupts_disabled = true;
			fibers_current->may_yield_disabled++;
		}

		FIBERS_LOG(FIBERS_LOG_DEBUG, "ml_set_interrupts_enabled exit: enable=%d, state=%d, may_yield_disabled=%d", enable, !get_mock_thread()->interrupts_disabled, fibers_current->may_yield_disabled);

		fibers_may_yield_internal_with_reason(
			(enable ? FIBERS_YIELD_REASON_PREEMPTION_DID_ENABLE : FIBERS_YIELD_REASON_PREEMPTION_DID_DISABLE) |
			FIBERS_YIELD_REASON_ERROR_IF(enable != prev_interrupts_disabled));

		return !prev_interrupts_disabled;
	} else {
		pthread_mutex_t *m = &get_proc_state()->interrupts_mutex;
		if (enable) {
			int ret = pthread_mutex_unlock(m);
			PT_QUIET; PT_ASSERT_POSIX_ZERO(ret, "interrupts pthread_mutex_unlock");
		} else {
			// disable interrupts locks
			int ret = pthread_mutex_lock(m);
			PT_QUIET; PT_ASSERT_POSIX_ZERO(ret, "interrupts pthread_mutex_lock");
		}
	}
	return true;
}

T_MOCK(boolean_t,
ml_set_interrupts_enabled_with_debug, (boolean_t enable, boolean_t __unused debug))
{
	return MOCK_ml_set_interrupts_enabled(enable);
}

T_MOCK(void,
_disable_preemption, (void))
{
	if (ut_mocks_use_fibers) {
		fibers_may_yield_internal_with_reason(
			FIBERS_YIELD_REASON_PREEMPTION_WILL_DISABLE |
			FIBERS_YIELD_REASON_ERROR_IF(fibers_current->may_yield_disabled != 0));

		fibers_current->may_yield_disabled++;

		FIBERS_LOG(FIBERS_LOG_DEBUG, "disable_preemption: may_yield_disabled=%d", fibers_current->may_yield_disabled);

		thread_t thread = MOCK_current_thread_fast();
		unsigned int count = thread->machine.preemption_count;
		os_atomic_store(&thread->machine.preemption_count, count + 1, compiler_acq_rel);

		fibers_may_yield_internal_with_reason(
			FIBERS_YIELD_REASON_PREEMPTION_DID_DISABLE |
			FIBERS_YIELD_REASON_ERROR_IF(fibers_current->may_yield_disabled != 1));
	} else {
		pthread_mutex_t *m = &get_proc_state()->interrupts_mutex;

		int ret = pthread_mutex_lock(m);
		PT_QUIET; PT_ASSERT_POSIX_ZERO(ret, "_disable_preemption pthread_mutex_lock");

		thread_t thread = MOCK_current_thread_fast();
		unsigned int count = thread->machine.preemption_count;
		os_atomic_store(&thread->machine.preemption_count, count + 1, compiler_acq_rel);
	}
}

T_MOCK(void,
_disable_preemption_without_measurements, (void))
{
	MOCK__disable_preemption();
}

T_MOCK(void,
lock_disable_preemption_for_thread, (thread_t t))
{
	MOCK__disable_preemption();
}

T_MOCK(void,
_enable_preemption, (void))
{
	if (ut_mocks_use_fibers) {
		fibers_may_yield_internal_with_reason(
			FIBERS_YIELD_REASON_PREEMPTION_WILL_ENABLE |
			FIBERS_YIELD_REASON_ERROR_IF(fibers_current->may_yield_disabled != 1));

		fibers_current->may_yield_disabled--;

		FIBERS_LOG(FIBERS_LOG_DEBUG, "enable_preemption: may_yield_disabled=%d", fibers_current->may_yield_disabled);

		thread_t thread = current_thread();
		unsigned int count = thread->machine.preemption_count;
		os_atomic_store(&thread->machine.preemption_count, count - 1, compiler_acq_rel);

		fibers_may_yield_internal_with_reason(
			FIBERS_YIELD_REASON_PREEMPTION_DID_ENABLE |
			FIBERS_YIELD_REASON_ERROR_IF(fibers_current->may_yield_disabled != 0));
	} else {
		thread_t thread = current_thread();
		unsigned int count  = thread->machine.preemption_count;
		os_atomic_store(&thread->machine.preemption_count, count - 1, compiler_acq_rel);

		pthread_mutex_t *m = &get_proc_state()->interrupts_mutex;

		int ret = pthread_mutex_unlock(m);
		PT_QUIET; PT_ASSERT_POSIX_ZERO(ret, "_enable_preemption pthread_mutex_unlock");
	}
}

// --------------- mutex ------------------

struct mock_lck_mtx_t {
	union {
		pthread_mutex_t *pt_m;
		fibers_mutex_t *f_m;
	};
	lck_mtx_state_t lck_mtx;
};
static_assert(sizeof(struct mock_lck_mtx_t) == sizeof(lck_mtx_t));

void
fake_init_lock(lck_mtx_t * lck)
{
	struct mock_lck_mtx_t* mlck = (struct mock_lck_mtx_t*)lck;
	if (ut_mocks_use_fibers) {
		mlck->f_m = calloc(1, sizeof(fibers_mutex_t));
		fibers_mutex_init(mlck->f_m);
	} else {
		mlck->pt_m = calloc(1, sizeof(pthread_mutex_t));
		int ret = pthread_mutex_init(mlck->pt_m, NULL);
		PT_QUIET; PT_ASSERT_POSIX_ZERO(ret, "pthread_mutex_init");
	}
}

T_MOCK(void,
lck_mtx_init, (lck_mtx_t * lck, lck_grp_t * grp, lck_attr_t * attr))
{
	fake_init_lock(lck);
}

T_MOCK(void,
lck_mtx_destroy, (lck_mtx_t * lck, lck_grp_t * grp))
{
	struct mock_lck_mtx_t* mlck = (struct mock_lck_mtx_t*)lck;
	if (ut_mocks_use_fibers) {
		fibers_mutex_destroy(mlck->f_m);
		free(mlck->f_m);
		mlck->f_m = NULL;
	} else {
		int ret = pthread_mutex_destroy(mlck->pt_m);
		PT_QUIET; PT_ASSERT_POSIX_ZERO(ret, "pthread_mutex_destroy");
		free(mlck->pt_m);
		mlck->pt_m = NULL;
	}
}

T_MOCK(void,
lck_mtx_lock, (lck_mtx_t * lock))
{
	uint32_t ctid = MOCK_current_thread_fast()->ctid;

	struct mock_lck_mtx_t* mlck = (struct mock_lck_mtx_t*)lock;
	if (ut_mocks_use_fibers) {
		fibers_mutex_lock(mlck->f_m, true);
	} else {
		int ret = pthread_mutex_lock(mlck->pt_m);
		PT_QUIET; PT_ASSERT_POSIX_ZERO(ret, "pthread_mutex_lock");
	}
	mlck->lck_mtx.owner = ctid;
}

T_MOCK(void,
lck_mtx_lock_spin, (lck_mtx_t * lock))
{
	uint32_t ctid = MOCK_current_thread_fast()->ctid;

	struct mock_lck_mtx_t* mlck = (struct mock_lck_mtx_t*)lock;
	if (ut_mocks_use_fibers) {
		fibers_mutex_lock(mlck->f_m, false); // do not check for disabled preemption if spinlock
	} else {
		int ret = pthread_mutex_lock(mlck->pt_m);
		PT_QUIET; PT_ASSERT_POSIX_ZERO(ret, "pthread_mutex_lock");
	}
	mlck->lck_mtx.owner = ctid;
}

T_MOCK(boolean_t,
lck_mtx_try_lock, (lck_mtx_t * lock))
{
	uint32_t ctid = MOCK_current_thread_fast()->ctid;

	struct mock_lck_mtx_t* mlck = (struct mock_lck_mtx_t*)lock;
	int ret;
	if (ut_mocks_use_fibers) {
		ret = fibers_mutex_try_lock(mlck->f_m);
	} else {
		int ret = pthread_mutex_trylock(mlck->pt_m);
	}
	if (ret == 0) {
		mlck->lck_mtx.owner = ctid;
		return TRUE;
	} else {
		return FALSE;
	}
}

T_MOCK(void,
lck_mtx_unlock, (lck_mtx_t * lock))
{
	struct mock_lck_mtx_t* mlck = (struct mock_lck_mtx_t*)lock;
	mlck->lck_mtx.owner = 0;
	if (ut_mocks_use_fibers) {
		fibers_mutex_unlock(mlck->f_m);
	} else {
		int ret = pthread_mutex_unlock(mlck->pt_m);
		PT_QUIET; PT_ASSERT_POSIX_ZERO(ret, "pthread_mutex_unlock");
	}
}

T_MOCK(void,
mutex_pause, (uint32_t collisions))
{
	if (ut_mocks_use_fibers) {
		// we can't sleep to not break determinism, trigger a ctxswitch instead
		fibers_yield();
	} else {
		mutex_pause(collisions);
	}
}

// --------------- rwlocks ------------------

struct mock_lck_rw_t {
	fibers_rwlock_t *rw;
	// lck_rw_word_t   lck_rw; // RANGELOCKINGTODO rdar://150846598
	uint32_t lck_rw_owner;
};
static_assert(sizeof(struct mock_lck_rw_t) == sizeof(lck_rw_t));

static_assert(LCK_RW_ASSERT_SHARED == FIBERS_RWLOCK_ASSERT_SHARED);
static_assert(LCK_RW_ASSERT_EXCLUSIVE == FIBERS_RWLOCK_ASSERT_EXCLUSIVE);
static_assert(LCK_RW_ASSERT_HELD == FIBERS_RWLOCK_ASSERT_HELD);
static_assert(LCK_RW_ASSERT_NOTHELD == FIBERS_RWLOCK_ASSERT_NOTHELD);

void
fake_init_rwlock(struct mock_lck_rw_t *mlck)
{
	mlck->rw = calloc(1, sizeof(fibers_rwlock_t));
	fibers_rwlock_init(mlck->rw);
}

static boolean_t
fake_rw_try_lock(struct mock_lck_rw_t *mlck, lck_rw_type_t lck_rw_type)
{
	int ret;
	// RANGELOCKINGTODO rdar://150846598 handle old lock can_sleep
	lck_rw_lock_count_inc(MOCK_current_thread_fast(), (const void*)mlck);

	if (lck_rw_type == LCK_RW_TYPE_SHARED) {
		ret = fibers_rwlock_try_rdlock(mlck->rw);
	} else if (lck_rw_type == LCK_RW_TYPE_EXCLUSIVE) {
		ret = fibers_rwlock_try_wrlock(mlck->rw);
		if (ret == 0) {
			mlck->lck_rw_owner = MOCK_current_thread_fast()->ctid;
		}
	} else {
		PT_FAIL("lck_rw_try_lock: Invalid lock type");
	}

	if (ret != 0) {
		// RANGELOCKINGTODO rdar://150846598 handle old lock can_sleep
		lck_rw_lock_count_dec(MOCK_current_thread_fast(), (const void*)mlck);
	}
	return ret == 0;
}

static bool
fake_rw_lock_would_yield_exclusive(struct mock_lck_rw_t *mlck, lck_rw_yield_t mode)
{
	fibers_rwlock_assert(mlck->rw, FIBERS_RWLOCK_ASSERT_EXCLUSIVE);

	bool yield = false;
	if (mode == LCK_RW_YIELD_ALWAYS) {
		yield = true;
	} else {
		if (mlck->rw->writer_wait_queue.count > 0) {
			yield = true;
		} else if (mode == LCK_RW_YIELD_ANY_WAITER) {
			yield = (mlck->rw->reader_wait_queue.count != 0);
		}
	}
	return yield;
}

T_MOCK(void,
lck_rw_init, (
	lck_rw_t * lck,
	lck_grp_t * grp,
	lck_attr_t * attr))
{
	if (!ut_mocks_use_fibers) {
		lck_rw_init(lck, grp, attr);
		return;
	}

	// RANGELOCKINGTODO rdar://150846598 mock attr, especially lck_rw_can_sleep
	struct mock_lck_rw_t* mlck = (struct mock_lck_rw_t*)lck;
	fake_init_rwlock(mlck);
}

T_MOCK(void,
lck_rw_destroy, (lck_rw_t * lck, lck_grp_t * grp))
{
	if (!ut_mocks_use_fibers) {
		lck_rw_destroy(lck, grp);
		return;
	}

	struct mock_lck_rw_t* mlck = (struct mock_lck_rw_t*)lck;
	fibers_rwlock_destroy(mlck->rw);
	free(mlck->rw);
	mlck->rw = NULL;
}

T_MOCK(void,
lck_rw_unlock, (lck_rw_t * lck, lck_rw_type_t lck_rw_type))
{
	if (!ut_mocks_use_fibers) {
		lck_rw_unlock(lck, lck_rw_type);
		return;
	}

	struct mock_lck_rw_t* mlck = (struct mock_lck_rw_t*)lck;
	if (mlck->rw->writer_active) {
		mlck->lck_rw_owner = 0;
	}
	fibers_rwlock_unlock(mlck->rw);

	// RANGELOCKINGTODO rdar://150846598 handle old lock can_sleep
	lck_rw_lock_count_dec(MOCK_current_thread_fast(), (const void*)mlck);
}

static void
lck_rw_old_mock_unlock_shared(lck_rw_t * lck)
{
	if (!ut_mocks_use_fibers) {
		lck_rw_unlock_shared(lck);
		return;
	}

	struct mock_lck_rw_t* mlck = (struct mock_lck_rw_t*)lck;
	fibers_rwlock_rdunlock(mlck->rw);

	// RANGELOCKINGTODO rdar://150846598 handle old lock can_sleep
	lck_rw_lock_count_dec(MOCK_current_thread_fast(), (const void*)mlck);
}

T_MOCK(void,
lck_rw_unlock_shared, (lck_rw_t * lck))
{
	lck_rw_old_mock_unlock_shared(lck);
}

T_MOCK(void,
lck_rw_unlock_exclusive, (lck_rw_t * lck))
{
	if (!ut_mocks_use_fibers) {
		lck_rw_unlock_exclusive(lck);
		return;
	}

	struct mock_lck_rw_t* mlck = (struct mock_lck_rw_t*)lck;
	mlck->lck_rw_owner = 0;
	fibers_rwlock_wrunlock(mlck->rw);

	// RANGELOCKINGTODO rdar://150846598 handle old lock can_sleep
	lck_rw_lock_count_dec(MOCK_current_thread_fast(), (const void*)mlck);
}

T_MOCK(void,
lck_rw_lock_exclusive, (lck_rw_t * lck))
{
	if (!ut_mocks_use_fibers) {
		lck_rw_lock_exclusive(lck);
		return;
	}

	// RANGELOCKINGTODO rdar://150846598 handle old lock can_sleep
	lck_rw_lock_count_inc(MOCK_current_thread_fast(), (const void*)lck);

	struct mock_lck_rw_t* mlck = (struct mock_lck_rw_t*)lck;
	fibers_rwlock_wrlock(mlck->rw, true);
	mlck->lck_rw_owner = MOCK_current_thread_fast()->ctid;
}

T_MOCK(void,
lck_rw_lock_shared, (lck_rw_t * lck))
{
	if (!ut_mocks_use_fibers) {
		lck_rw_lock_shared(lck);
		return;
	}

	// RANGELOCKINGTODO rdar://150846598 handle old lock can_sleep
	lck_rw_lock_count_inc(MOCK_current_thread_fast(), (const void*)lck);

	struct mock_lck_rw_t* mlck = (struct mock_lck_rw_t*)lck;
	fibers_rwlock_rdlock(mlck->rw, true);
}

T_MOCK(boolean_t,
lck_rw_try_lock, (lck_rw_t * lck, lck_rw_type_t lck_rw_type))
{
	if (!ut_mocks_use_fibers) {
		return lck_rw_try_lock(lck, lck_rw_type);
	}

	struct mock_lck_rw_t* mlck = (struct mock_lck_rw_t*)lck;
	return fake_rw_try_lock(mlck, lck_rw_type);
}

T_MOCK(boolean_t,
lck_rw_try_lock_exclusive, (lck_rw_t * lck))
{
	if (!ut_mocks_use_fibers) {
		return lck_rw_try_lock_exclusive(lck);
	}

	struct mock_lck_rw_t* mlck = (struct mock_lck_rw_t*)lck;
	return fake_rw_try_lock(mlck, LCK_RW_TYPE_EXCLUSIVE);
}

T_MOCK(boolean_t,
lck_rw_try_lock_shared, (lck_rw_t * lck))
{
	if (!ut_mocks_use_fibers) {
		return lck_rw_try_lock_shared(lck);
	}

	struct mock_lck_rw_t* mlck = (struct mock_lck_rw_t*)lck;
	return fake_rw_try_lock(mlck, LCK_RW_TYPE_SHARED);
}

T_MOCK(lck_rw_type_t,
lck_rw_done, (lck_rw_t * lck))
{
	if (!ut_mocks_use_fibers) {
		return lck_rw_done(lck);
	}

	struct mock_lck_rw_t* mlck = (struct mock_lck_rw_t*)lck;
	mlck->lck_rw_owner = 0;
	// If there is a writer locking it must be the current fiber or will trigger an assertion in fibers_rwlock_wrunlock
	lck_rw_type_t ret = mlck->rw->writer_active ? LCK_RW_TYPE_EXCLUSIVE : LCK_RW_TYPE_SHARED;
	fibers_rwlock_unlock(mlck->rw);

	// RANGELOCKINGTODO rdar://150846598 handle old lock can_sleep
	lck_rw_lock_count_dec(MOCK_current_thread_fast(), (const void*)mlck);

	return ret;
}

T_MOCK(boolean_t,
lck_rw_lock_shared_to_exclusive, (lck_rw_t * lck))
{
	if (ut_mocks_lock_upgrade_fail) {
		lck_rw_old_mock_unlock_shared(lck);
		return false;
	}

	if (!ut_mocks_use_fibers) {
		return lck_rw_lock_shared_to_exclusive(lck);
	}

	struct mock_lck_rw_t* mlck = (struct mock_lck_rw_t*)lck;
	return fibers_rwlock_upgrade(mlck->rw);
}

T_MOCK(void,
lck_rw_lock_exclusive_to_shared, (lck_rw_t * lck))
{
	if (!ut_mocks_use_fibers) {
		lck_rw_lock_exclusive_to_shared(lck);
		return;
	}

	struct mock_lck_rw_t* mlck = (struct mock_lck_rw_t*)lck;
	fibers_rwlock_downgrade(mlck->rw);
}

T_MOCK(void,
lck_rw_assert, (
	lck_rw_t * lck,
	unsigned int type))
{
	if (!ut_mocks_use_fibers) {
		lck_rw_assert(lck, type);
		return;
	}

	struct mock_lck_rw_t* mlck = (struct mock_lck_rw_t*)lck;
	fibers_rwlock_assert(mlck->rw, type);
}

T_MOCK(bool,
lck_rw_lock_would_yield_exclusive, (
	lck_rw_t * lck,
	lck_rw_yield_t mode))
{
	if (!ut_mocks_use_fibers) {
		return lck_rw_lock_would_yield_exclusive(lck, mode);
	}

	struct mock_lck_rw_t* mlck = (struct mock_lck_rw_t*)lck;
	return fake_rw_lock_would_yield_exclusive(mlck, mode);
}

T_MOCK(bool,
lck_rw_lock_would_yield_shared, (lck_rw_t * lck))
{
	if (!ut_mocks_use_fibers) {
		return lck_rw_lock_would_yield_shared(lck);
	}

	struct mock_lck_rw_t* mlck = (struct mock_lck_rw_t*)lck;
	fibers_rwlock_assert(mlck->rw, FIBERS_RWLOCK_ASSERT_SHARED);
	return mlck->rw->writer_wait_queue.count != 0;
}

// Note: No need to mock lck_rw_sleep as it uses lck_rw_* API and waitq, we already mock everything the function uses

// --------------- waitq ------------------

/*
 *   If the 4 bytes of mock_waitq.mock_magic are not matching MOCK_WAITQ_MAGIC
 *   it means the waitq comes from an unsupported location and was not created with mock_waitq_init().
 */
#define MOCK_WAITQ_MAGIC 0xb60d0d8f

struct mock_waitq_extra {
	bool valid;
	fibers_condition_t cond;
	fibers_mutex_t mutex;

	struct mock_thread *waiting_threads;
	int waiting_thread_count; // Count of waiting threads
};

struct mock_waitq { // 24 bytes
	WAITQ_FLAGS(waitq, waitq_eventmask:_EVENT_MASK_BITS);
	unsigned int mock_magic;
	event64_t current_event; // delete when every waiting thread is removed
	struct mock_waitq_extra *extra;
};

static_assert(sizeof(struct waitq) == sizeof(struct mock_waitq));

#define MWQCAST(xnu_wq) ((struct mock_waitq *)(xnu_wq).wq_q)

static bool
waitq_use_real_impl(waitq_t wq)
{
	return !ut_mocks_use_fibers || waitq_type(wq) != WQT_QUEUE;
}

int
mock_waitq_init(struct mock_waitq *wq)
{
	if (!wq) {
		return EINVAL;
	}
	wq->mock_magic = MOCK_WAITQ_MAGIC;
	wq->current_event = 0;

	wq->extra = calloc(sizeof(struct mock_waitq_extra), 1);
	wq->extra->valid = true;
	fibers_mutex_init(&wq->extra->mutex);

	return 0;
}

int
mock_waitq_destroy(struct mock_waitq *wq)
{
	if (!wq) {
		return EINVAL;
	}
	PT_QUIET; PT_ASSERT_TRUE(wq->mock_magic == MOCK_WAITQ_MAGIC, "missing mock_waitq magic");

	fibers_condition_destroy(&wq->extra->cond);
	fibers_mutex_destroy(&wq->extra->mutex);
	free(wq->extra);
	wq->extra = NULL;

	return 0;
}

static inline bool
waitq_should_unlock(waitq_wakeup_flags_t flags)
{
	return (flags & (WAITQ_UNLOCK | WAITQ_KEEP_LOCKED)) == WAITQ_UNLOCK;
}

static inline bool
waitq_should_enable_interrupts(waitq_wakeup_flags_t flags)
{
	return (flags & (WAITQ_UNLOCK | WAITQ_KEEP_LOCKED | WAITQ_ENABLE_INTERRUPTS)) == (WAITQ_UNLOCK | WAITQ_ENABLE_INTERRUPTS);
}


T_MOCK(void,
waitq_init, (waitq_t wq, waitq_type_t type, int policy))
{
	if (!ut_mocks_use_fibers || type == WQT_PORT) {
		waitq_init(wq, type, policy);
		return;
	}

	*wq.wq_q = (struct waitq){
		.waitq_type  = type,
		.waitq_fifo  = ((policy & SYNC_POLICY_REVERSED) == 0),
	};

	// RANGELOCKINGTODO rdar://150846598
	PT_QUIET; PT_ASSERT_TRUE(type == WQT_QUEUE, "invalid waitq type");
	mock_waitq_init(MWQCAST(wq));

	if (policy & SYNC_POLICY_INIT_LOCKED) {
		fibers_mutex_lock(&MWQCAST(wq)->extra->mutex, false);
	}
}

T_MOCK(void,
waitq_deinit, (waitq_t wq))
{
	if (waitq_use_real_impl(wq)) {
		waitq_deinit(wq);
		return;
	}

	PT_QUIET; PT_ASSERT_TRUE(MWQCAST(wq)->mock_magic == MOCK_WAITQ_MAGIC, "missing mock_waitq magic");
	mock_waitq_destroy(MWQCAST(wq));
}

T_MOCK(void,
waitq_lock, (waitq_t wq))
{
	if (waitq_use_real_impl(wq)) {
		waitq_lock(wq);
		return;
	}

	PT_QUIET; PT_ASSERT_TRUE(MWQCAST(wq)->mock_magic == MOCK_WAITQ_MAGIC, "missing mock_waitq magic");
	fibers_mutex_lock(&MWQCAST(wq)->extra->mutex, false);
}

T_MOCK(void,
waitq_unlock, (waitq_t wq))
{
	if (waitq_use_real_impl(wq)) {
		waitq_unlock(wq);
		return;
	}

	PT_QUIET; PT_ASSERT_TRUE(MWQCAST(wq)->mock_magic == MOCK_WAITQ_MAGIC, "missing mock_waitq magic");
	fibers_mutex_unlock(&MWQCAST(wq)->extra->mutex);
}

T_MOCK(bool,
waitq_is_valid, (waitq_t wq))
{
	if (waitq_use_real_impl(wq)) {
		return waitq_is_valid(wq);
	}

	PT_QUIET; PT_ASSERT_TRUE(MWQCAST(wq)->mock_magic == MOCK_WAITQ_MAGIC, "missing mock_waitq magic");
	return MWQCAST(wq)->extra->valid;
}

T_MOCK(void,
waitq_invalidate, (waitq_t wq))
{
	if (waitq_use_real_impl(wq)) {
		return waitq_invalidate(wq);
	}

	PT_QUIET; PT_ASSERT_TRUE(MWQCAST(wq)->mock_magic == MOCK_WAITQ_MAGIC, "missing mock_waitq magic");
	MWQCAST(wq)->extra->valid = false;
}

T_MOCK(bool,
waitq_held, (waitq_t wq))
{
	if (waitq_use_real_impl(wq)) {
		return waitq_held(wq);
	}

	PT_QUIET; PT_ASSERT_TRUE(MWQCAST(wq)->mock_magic == MOCK_WAITQ_MAGIC, "missing mock_waitq magic");
	return MWQCAST(wq)->extra->mutex.holder != NULL;
}

T_MOCK(void,
waitq_lock_wait, (waitq_t wq, uint32_t ticket))
{
	MOCK_waitq_lock(wq);
}

T_MOCK(bool,
waitq_lock_try, (waitq_t wq))
{
	if (waitq_use_real_impl(wq)) {
		return waitq_lock_try(wq);
	}

	PT_QUIET; PT_ASSERT_TRUE(MWQCAST(wq)->mock_magic == MOCK_WAITQ_MAGIC, "missing mock_waitq magic");
	return fibers_mutex_try_lock(&MWQCAST(wq)->extra->mutex) == 0;
}

// --------------- events ------------------

#define MOCK_WAITQS_NUM 4096
static struct mock_waitq global_mock_waitqs[MOCK_WAITQS_NUM];
static int global_mock_waitqs_inited = 0;

static void
global_mock_waitqs_init(void)
{
	for (int i = 0; i < MOCK_WAITQS_NUM; ++i) {
		MOCK_waitq_init((struct waitq*)&global_mock_waitqs[i], WQT_QUEUE, SYNC_POLICY_FIFO);
	}
	global_mock_waitqs_inited = 1;
}

struct mock_waitq*
find_mock_waitq(event64_t event)
{
	if (!global_mock_waitqs_inited) {
		global_mock_waitqs_init();
	}
	for (int i = 0; i < MOCK_WAITQS_NUM; ++i) {
		if (global_mock_waitqs[i].current_event == event) {
			return &global_mock_waitqs[i];
		}
	}
	return NULL;
}

struct mock_waitq*
find_or_alloc_mock_waitq(event64_t event)
{
	if (!global_mock_waitqs_inited) {
		global_mock_waitqs_init();
	}
	int first_free = -1;
	for (int i = 0; i < MOCK_WAITQS_NUM; ++i) {
		if (global_mock_waitqs[i].current_event == event) {
			return &global_mock_waitqs[i];
		} else if (first_free < 0 && global_mock_waitqs[i].current_event == 0) {
			first_free = i;
		}
	}
	PT_QUIET; PT_ASSERT_TRUE(first_free >= 0, "no more space in global_mock_waitqs");
	global_mock_waitqs[first_free].current_event = event;
	return &global_mock_waitqs[first_free];
}

// --------------- waitq mocks ------------------

// pthread mocks

struct pthread_mock_event_table_entry*
find_pthread_mock_event_entry(struct mock_process_state *s, event_t ev)
{
	for (int i = 0; i < PTHREAD_EVENTS_TABLE_SIZE; ++i) {
		if (s->events[i].ev == ev) {
			return &s->events[i];
		}
	}
	return NULL;
}

T_MOCK_DYNAMIC(kern_return_t,
    thread_wakeup_prim, (
	    event_t event,
	    boolean_t one_thread,
	    wait_result_t result),
    (event, one_thread, result),
{
	if (ut_mocks_use_fibers) {
	        // fibers is mocking waitq apis, go forward calling the real thread_wakeup_prim
	        return thread_wakeup_prim(event, one_thread, result);
	}

	kern_return_t kr = KERN_SUCCESS;

	struct mock_process_state *s = get_proc_state();
	int ret = pthread_mutex_lock(&s->events_mutex);
	PT_QUIET; PT_ASSERT_POSIX_ZERO(ret, "thread_wakeup pthread_mutex_lock");

	struct pthread_mock_event_table_entry* event_entry = find_pthread_mock_event_entry(s, event);
	if (event_entry == NULL) {
	        kr = KERN_NOT_WAITING;
	        goto done;
	}
	if (one_thread) {
	        ret = pthread_cond_signal(&event_entry->cond);
	        PT_QUIET; PT_ASSERT_POSIX_ZERO(ret, "thread_wakeup pthread_cond_signal");
	} else {
	        ret = pthread_cond_broadcast(&event_entry->cond);
	        PT_QUIET; PT_ASSERT_POSIX_ZERO(ret, "thread_wakeup pthread_cond_broadcast");
	}
	done:
	pthread_mutex_unlock(&s->events_mutex);
	return kr;
});

wait_result_t
pthread_mock_thread_block_reason(
	thread_continue_t continuation,
	void *parameter,
	ast_t reason)
{
	PT_QUIET; PT_ASSERT_TRUE(continuation == THREAD_CONTINUE_NULL && parameter == NULL && reason == AST_NONE, "thread_block argument");

	struct mock_process_state *s = get_proc_state();
	int ret = pthread_mutex_lock(&s->events_mutex);
	PT_QUIET; PT_ASSERT_POSIX_ZERO(ret, "thread_block pthread_mutex_lock");

	// find empty entry in table
	struct pthread_mock_event_table_entry *event_entry = find_pthread_mock_event_entry(s, 0);
	PT_QUIET; PT_ASSERT_NOTNULL(event_entry, "empty entry not found");

	// register the entry to this event
	event_entry->ev = (event_t)MOCK_current_thread_fast()->wait_event;

	// if it doesn't have a condition variable yet, create one
	if (!event_entry->cond_inited) {
		ret = pthread_cond_init(&event_entry->cond, NULL);
		PT_QUIET; PT_ASSERT_POSIX_ZERO(ret, "thread_block pthread_cond_init");
		event_entry->cond_inited = true;
	}

	// wait on variable. This releases the mutex, waits and reaquires it before returning
	ret = pthread_cond_wait(&event_entry->cond, &s->events_mutex);
	PT_QUIET; PT_ASSERT_POSIX_ZERO(ret, "thread_block pthread_cond_wait");

	// reset the entry so that it can be reused (will be done by all waiters that woke up)
	event_entry->ev = 0;

	ret = pthread_mutex_unlock(&s->events_mutex);
	PT_QUIET; PT_ASSERT_POSIX_ZERO(ret, "thread_block pthread_mutex_unlock");

	return THREAD_AWAKENED;
}

kern_return_t
pthread_mock_clear_wait(
	thread_t thread,
	wait_result_t result)
{
	struct mock_process_state *s = get_proc_state();
	int ret = pthread_mutex_lock(&s->events_mutex);
	PT_QUIET; PT_ASSERT_POSIX_ZERO(ret, "clear_wait pthread_mutex_lock");

	struct pthread_mock_event_table_entry *event_entry = find_pthread_mock_event_entry(s, 0);
	PT_QUIET; PT_ASSERT_NOTNULL(event_entry, "empty entry not found");

	event_entry->ev = 0;

	ret = pthread_mutex_unlock(&s->events_mutex);
	PT_QUIET; PT_ASSERT_POSIX_ZERO(ret, "clear_wait pthread_mutex_unlock");
	return KERN_SUCCESS;
}

// fibers mocks

T_MOCK(struct waitq *,
_global_eventq, (event64_t event))
{
	if (!ut_mocks_use_fibers) {
		return _global_eventq(event);
	}

	struct waitq *ret = (struct waitq *)find_or_alloc_mock_waitq(event);
	return ret;
}

T_MOCK(wait_result_t,
waitq_assert_wait64_locked, (
	waitq_t waitq,
	event64_t wait_event,
	wait_interrupt_t interruptible,
	wait_timeout_urgency_t urgency,
	uint64_t deadline,
	uint64_t leeway,
	thread_t thread))
{
	if (waitq_use_real_impl(waitq)) {
		return waitq_assert_wait64_locked(waitq, wait_event, interruptible, urgency, deadline, leeway, thread);
	}

	struct mock_waitq *wq = MWQCAST(waitq);

	if (wq->current_event == 0) {
		wq->current_event = wait_event;
	}

	PT_QUIET; PT_ASSERT_TRUE(wq->current_event == wait_event, "waitq_assert_wait64_locked another event queue");

	struct mock_thread * mock_thread = (struct mock_thread*)thread; // !!! ASSUME every thread_t is created from mock_thread
	mock_thread->wq_next = wq->extra->waiting_threads;
	wq->extra->waiting_threads = mock_thread;
	wq->extra->waiting_thread_count++;

	thread->wait_event = wait_event; // Store waiting event in thread context
	thread->state |= TH_WAIT; // Set thread state to waiting
	thread->waitq = waitq;

	return THREAD_WAITING; // Indicate thread is now waiting, but not blocked yet
}

T_MOCK(wait_result_t,
waitq_assert_wait64, (
	struct waitq *waitq,
	event64_t wait_event,
	wait_interrupt_t interruptible,
	uint64_t deadline))
{
	if (waitq_use_real_impl(waitq)) {
		return waitq_assert_wait64(waitq, wait_event, interruptible, deadline);
	}

	thread_t thread = MOCK_current_thread_fast();

	MOCK_waitq_lock(waitq);
	wait_result_t res = MOCK_waitq_assert_wait64_locked(waitq, wait_event, interruptible,
	    TIMEOUT_URGENCY_SYS_NORMAL, deadline, TIMEOUT_NO_LEEWAY, thread);
	MOCK_waitq_unlock(waitq);
	return res;
}

static void
mock_waitq_clear_wait(struct mock_thread * thread, struct mock_waitq *wq)
{
	struct mock_thread ** mock_thread = &wq->extra->waiting_threads;
	int removed = 0;
	while (*mock_thread) {
		if (*mock_thread == thread) {
			*mock_thread = (*mock_thread)->wq_next;
			removed = 1;
			break;
		}
		mock_thread = &(*mock_thread)->wq_next;
	}
	PT_QUIET; PT_ASSERT_TRUE(removed, "thread_block thread not in wq");
	thread->wq_next = NULL;

	wq->extra->waiting_thread_count--;
	if (wq->extra->waiting_thread_count == 0) {
		wq->current_event = 0; // reset current_event
	}
	PT_QUIET; PT_ASSERT_TRUE(wq->extra->waiting_thread_count >= 0, "something bad");
}

static struct mock_thread *
mock_waitq_pop_wait(struct mock_waitq *wq)
{
	if (wq->extra->waiting_thread_count == 0) {
		return NULL;
	}

	struct mock_thread * thread = wq->extra->waiting_threads;
	wq->extra->waiting_threads = thread->wq_next;
	thread->wq_next = NULL;

	wq->extra->waiting_thread_count--;
	if (wq->extra->waiting_thread_count == 0) {
		wq->current_event = 0; // reset current_event
	}
	PT_QUIET; PT_ASSERT_TRUE(wq->extra->waiting_thread_count >= 0, "something bad");

	return thread;
}

T_MOCK_DYNAMIC(wait_result_t,
    thread_block_reason, (
	    thread_continue_t continuation,
	    void *parameter,
	    ast_t reason), (
	    continuation,
	    parameter,
	    reason),
{
	if (!ut_mocks_use_fibers) {
	        return pthread_mock_thread_block_reason(continuation, parameter, reason);
	}

	PT_QUIET; PT_ASSERT_TRUE(continuation == THREAD_CONTINUE_NULL && parameter == NULL && reason == AST_NONE, "thread_block argument");

	thread_t thread = current_thread();
	PT_QUIET; PT_ASSERT_TRUE(thread->state & TH_WAIT, "thread_block called but thread state is not TH_WAIT");

	/*
	 * In case of a window between assert_wait and thread_block
	 * another thread could wake up the current thread after being added to the waitq
	 * but before the block.
	 * In this case, the thread will still be TH_WAIT but without an assigned waitq.
	 * TH_WAKING must be set.
	 */
	struct mock_waitq *wq = MWQCAST(thread->waitq);
	if (wq == NULL) {
	        PT_QUIET; PT_ASSERT_TRUE(thread->state & TH_WAKING, "with waitq == NULL there must be TH_WAKING set");
	        thread->state &= ~TH_WAKING;
	        goto awake_thread;
	}

	fibers_condition_wait(&wq->extra->cond);

	if (thread->state & TH_WAKING) {
	        thread->state &= ~TH_WAKING;
	} else {
	        // is this possible? TH_WAKING is always set ATM in the mocks, keep this code to be more robust
	        thread->waitq.wq_q = NULL;
	        mock_waitq_clear_wait((struct mock_thread *)thread, wq);
	}

	awake_thread:
	thread->state &= ~TH_WAIT;
	thread->state |= TH_RUN;

	return thread->wait_result;
});

T_MOCK(kern_return_t,
clear_wait, (thread_t thread, wait_result_t wresult))
{
	if (!ut_mocks_use_fibers) {
		return pthread_mock_clear_wait(thread, wresult);
	}

	struct mock_waitq *wq = MWQCAST(thread->waitq);
	PT_QUIET; PT_ASSERT_TRUE(wq != NULL, "thread->waitq is NULL");

	thread->state &= ~TH_WAIT;
	thread->waitq.wq_q = NULL;
	thread->wait_result = wresult;

	mock_waitq_clear_wait((struct mock_thread *)thread, wq);

	return KERN_SUCCESS;
}

typedef struct {
	wait_result_t wait_result;
} waitq_wakeup_args_t;

static void
waitq_wakeup_fiber_callback(void *arg, fiber_t target)
{
	waitq_wakeup_args_t *wakeup_args = (waitq_wakeup_args_t*)arg;
	struct mock_thread *thread = (struct mock_thread *)target->extra;
	assert(thread);

	struct mock_waitq *wq = MWQCAST(thread->th.waitq);
	assert(wq);

	thread->th.state |= TH_WAKING;
	thread->th.waitq.wq_q = NULL;
	thread->th.wait_result = wakeup_args->wait_result;

	mock_waitq_clear_wait(thread, wq);
}

// Called from thread_wakeup_nthreads_prim
T_MOCK(uint32_t,
waitq_wakeup64_nthreads_locked, (
	waitq_t waitq,
	event64_t wake_event,
	wait_result_t result,
	waitq_wakeup_flags_t flags,
	uint32_t nthreads))
{
	if (waitq_use_real_impl(waitq)) {
		return waitq_wakeup64_nthreads_locked(waitq, wake_event, result, flags, nthreads);
	}

	// RANGELOCKINGTODO rdar://150846598 flags
	waitq_wakeup_args_t wakeup_args = {
		.wait_result = result
	};

	struct mock_waitq *wq = MWQCAST(waitq);
	PT_QUIET; PT_ASSERT_TRUE(wq->current_event == wake_event, "waitq_wakeup64_nthreads current_event is wrong");

	// Avoid to trigger a switch in fibers_condition_wakeup_some before a valid state in the waitq
	fibers_current->may_yield_disabled++;

	FIBERS_LOG(FIBERS_LOG_DEBUG, "waitq_wakeup64_nthreads_locked nthreads=%u wake_event=%lld", nthreads, wake_event);

	int count = fibers_condition_wakeup_some(&wq->extra->cond, nthreads, &waitq_wakeup_fiber_callback, &wakeup_args);

	/*
	 * In case of a window in which a thread is pushed to the waitq but thread_block was still not called
	 * when another thread wakes up the threads in the waitq here.
	 * fibers_condition_wakeup_some will not find these fibers as they are not waiting on the condition,
	 * In this case these fibers must be in FIBER_STOP that means that they are ready to be scheduled,
	 * but we still need to take action here to remove them from the waitq and clear the state.
	 */
	while (wq->extra->waiting_thread_count && count < nthreads) {
		struct mock_thread *thread = mock_waitq_pop_wait(wq);
		PT_QUIET; PT_ASSERT_TRUE(thread->fiber->state & FIBER_STOP, "leftover fiber in waitq not in FIBER_STOP");
		thread->th.state |= TH_WAKING;
		thread->th.waitq.wq_q = NULL;
		thread->th.wait_result = result;
		++count;
	}

	fibers_current->may_yield_disabled--;

	if (waitq_should_unlock(flags)) {
		MOCK_waitq_unlock(waitq);
	}
	if (waitq_should_enable_interrupts(flags)) {
		MOCK_ml_set_interrupts_enabled(1);
	}

	return (uint32_t)count;
}

T_MOCK(thread_t,
waitq_wakeup64_identify_locked, (
	waitq_t waitq,
	event64_t wake_event,
	waitq_wakeup_flags_t flags))
{
	if (waitq_use_real_impl(waitq)) {
		return waitq_wakeup64_identify_locked(waitq, wake_event, flags);
	}

	// RANGELOCKINGTODO rdar://150846598 flags

	struct mock_waitq *wq = MWQCAST(waitq);
	PT_QUIET; PT_ASSERT_TRUE(wq->current_event == wake_event, "waitq_wakeup64_identify_locked current_event is wrong");

	// RANGELOCKINGTODO rdar://150845975 for fuzzing select random, not the top of the queue
	struct mock_thread * mock_thread = wq->extra->waiting_threads;
	if (mock_thread == NULL) {
		return THREAD_NULL;
	}

	// Preemption will be re-enabled when the thread is resumed in `waitq_resume_identify_thread`
	MOCK__disable_preemption();

	mock_thread->th.state |= TH_WAKING;
	mock_thread->th.waitq.wq_q = NULL;
	mock_thread->th.wait_result = THREAD_AWAKENED;

	mock_waitq_clear_wait(mock_thread, wq);

	FIBERS_LOG(FIBERS_LOG_DEBUG, "waitq_wakeup64_identify_locked identified fiber %d", mock_thread->fiber->id);

	if (waitq_should_unlock(flags)) {
		MOCK_waitq_unlock(waitq);
	}
	if (waitq_should_enable_interrupts(flags)) {
		MOCK_ml_set_interrupts_enabled(1);
	}

	fibers_may_yield_internal();

	return &mock_thread->th;
}

T_MOCK(void,
waitq_resume_identified_thread, (
	waitq_t waitq,
	thread_t thread,
	wait_result_t result,
	waitq_wakeup_flags_t flags))
{
	if (waitq_use_real_impl(waitq)) {
		return waitq_resume_identified_thread(waitq, thread, result, flags);
	}

	// RANGELOCKINGTODO rdar://150846598 other flags

	struct mock_thread * mock_thread = (struct mock_thread*)thread; // !!! ASSUME every thread_t is created from mock_thread
	struct mock_waitq *wq = MWQCAST(waitq);

	bool found = fibers_condition_wakeup_identified(&wq->extra->cond, mock_thread->fiber);
	if (!found) {
		/*
		 * In case of a window in which a thread is pushed to the waitq but thread_block was still not called
		 * when the thread is identified by another one and resumed, we pop it from the waitq in waitq_wakeup64_identify_locked
		 * but we will not find it in wq->cond.wait_queue.
		 * In this case it is not needed any action as the fiber must be in FIBER_STOP and can already be scheduled.
		 */
		PT_QUIET; PT_ASSERT_TRUE(mock_thread->fiber->state & FIBER_STOP, "waitq_resume_identified_thread fiber not found in condition and not in FIBER_STOP");
	}

	// Paired with the call to `waitq_wakeup64_identify_locked`
	MOCK__enable_preemption();

	fibers_may_yield_internal_with_reason(
		FIBERS_YIELD_REASON_WAKEUP |
		FIBERS_YIELD_REASON_ERROR_IF(!found));
}

// Allow to cause a context switch from a function that can be called from XNU
T_MOCK(void,
ut_fibers_ctxswitch, (void))
{
	if (ut_mocks_use_fibers) {
		fibers_yield();
	}
}

// Allow to cause a context switch to a specific fiber from a function that can be called from XNU
T_MOCK(void,
ut_fibers_ctxswitch_to, (int fiber_id))
{
	if (ut_mocks_use_fibers) {
		fibers_yield_to(fiber_id);
	}
}

// Get the current fiber id from a function that can be called from XNU
T_MOCK(int,
ut_fibers_current_id, (void))
{
	if (ut_mocks_use_fibers) {
		return fibers_current->id;
	}
	return -1;
}

// --------------- preemption ------------------

#ifdef __BUILDING_WITH_SANCOV_LOAD_STORES__
// Optional: uncomment to enable yield at every basic block entry
/*
 *  T_MOCK(void,
 *  __sanitizer_cov_trace_pc_guard, (uint32_t * guard))
 *  {
 *	   fibers_may_yield();
 *  }
 */

#define IS_ALIGNED(ptr, size) ( (((uintptr_t)(ptr)) & (((uintptr_t)(size)) - 1)) == 0 )
#define IS_ATOMIC(ptr, size) ( (size) <= sizeof(uint64_t) && IS_ALIGNED(ptr, size) )

// These functions can be called from XNU to enter/exit atomic regions in which the data checker is disabled
T_MOCK(void,
data_race_checker_atomic_begin, (void))
{
	fibers_checker_atomic_begin();
}
T_MOCK(void,
data_race_checker_atomic_end, (void))
{
	fibers_checker_atomic_end();
}

/*
 * Detecting data races on memory operations:
 * Memory operation functions are used to check for data races using the fibers checkers API, a software implementation of DataCollider.
 * The idea is to set a watchpoint before context switching and report a data race every time a concurrent access (watchpoint hit) is in between a write or a write in between a load.
 * To be more robust, we also check that the value pointed the memory operation address before the context switch is still the same after the context switch.
 * If not, very likely it is a data race. Atomic memory operations should be excluded from this, we use the IS_ATOMIC macro to filter memory loads.
 * Note: atomic_fetch_add_explicit() et al. on ARM64 are compiled to LDADD et al. that seem to not be supported by __sanitizer_cov_loadX, ok for us we want to skip atomic operations.
 */
#define SANCOV_LOAD_STORE_DATA_CHECKER(type, size, access_type) do {                            \
	    if (fibers_current->may_yield_disabled) {                                               \
	        return;                                                                             \
	    }                                                                                       \
	    if (fibers_scheduler->fibers_should_yield(fibers_scheduler_context,                     \
	        fibers_may_yield_probability, FIBERS_YIELD_REASON_PREEMPTION_TRIGGER)) {            \
	        volatile type before = *addr;                                                       \
	        void *pc = __builtin_return_address(0);                                             \
	        bool has_wp = check_and_set_watchpoint(pc, (uintptr_t)addr, size, access_type);     \
                                                                                                \
	        fibers_queue_push(&fibers_run_queue, fibers_current);                               \
	        fibers_choose_next(FIBER_STOP);                                                     \
                                                                                                \
	        if (has_wp) {                                                                       \
	            post_check_and_remove_watchpoint((uintptr_t)addr, size, access_type);           \
	        }                                                                                   \
	        type after = *addr;                                                                 \
	        if (before != after) {                                                              \
	            report_value_race((uintptr_t)addr, size, access_type);                          \
	        }                                                                                   \
	    }                                                                                       \
	} while (0)

/*
 * Mock the SanitizerCoverage load/store instrumentation callbacks (original in san_attached.c).
 * The functions are execute at every memory operations in libxnu and in the test binary, libmocks is excluded.
 * Functions and files in tools/sanitizers-ignorelist are excluded from instrumentation.
 */
#define MOCK_SANCOV_LOAD_STORE(type, size)                                                                       \
	__attribute__((optnone))                                                                                     \
	T_MOCK(void,                                                                                                 \
	__sanitizer_cov_load##size, (type* addr))                                                                    \
	{                                                                                                            \
	    if (!ut_fibers_use_data_race_checker || IS_ATOMIC(addr, size) || fibers_current->disable_race_checker) { \
	        fibers_may_yield_with_reason(FIBERS_YIELD_REASON_PREEMPTION_TRIGGER);                                \
	        return;                                                                                              \
	    }                                                                                                        \
	    SANCOV_LOAD_STORE_DATA_CHECKER(type, size, ACCESS_TYPE_LOAD);                                            \
	}                                                                                                            \
                                                                                                                 \
	__attribute__((optnone))                                                                                     \
	T_MOCK(void,                                                                                                 \
	__sanitizer_cov_store##size, (type* addr))                                                                   \
	{   /* do not care about atomicity for stores */                                                             \
	    if (!ut_fibers_use_data_race_checker || fibers_current->disable_race_checker) {                          \
	        fibers_may_yield_with_reason(FIBERS_YIELD_REASON_PREEMPTION_TRIGGER);                                \
	        return;                                                                                              \
	    }                                                                                                        \
	    SANCOV_LOAD_STORE_DATA_CHECKER(type, size, ACCESS_TYPE_STORE);                                           \
	}

MOCK_SANCOV_LOAD_STORE(uint8_t, 1)
MOCK_SANCOV_LOAD_STORE(uint16_t, 2)
MOCK_SANCOV_LOAD_STORE(uint32_t, 4)
MOCK_SANCOV_LOAD_STORE(uint64_t, 8)
MOCK_SANCOV_LOAD_STORE(__uint128_t, 16)

#endif // __BUILDING_WITH_SANCOV__
