/*
 * Copyright (c) 2000-2021 Apple Inc. All rights reserved.
 *
 * @Apple_LICENSE_HEADER_START@
 *
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#include <sys/errno.h>
#include <sys/kdebug_private.h>
#include <sys/proc_internal.h>
#include <sys/vm.h>
#include <sys/sysctl.h>
#include <sys/kdebug_common.h>
#include <sys/kdebug.h>
#include <sys/kdebug_triage.h>
#include <sys/kauth.h>
#include <sys/ktrace.h>
#include <sys/sysproto.h>
#include <sys/bsdtask_info.h>
#include <sys/random.h>

#include <mach/mach_vm.h>
#include <machine/atomic.h>

#include <mach/machine.h>
#include <mach/vm_map.h>
#include <kern/clock.h>

#include <kern/task.h>
#include <kern/debug.h>
#include <kern/kalloc.h>
#include <kern/telemetry.h>
#include <kern/sched_prim.h>
#include <sys/lock.h>
#include <pexpert/device_tree.h>

#include <sys/malloc.h>

#include <sys/vnode.h>
#include <sys/vnode_internal.h>
#include <sys/fcntl.h>
#include <sys/file_internal.h>
#include <sys/ubc.h>
#include <sys/param.h>                  /* for isset() */

#include <vm/vm_kern_xnu.h>
#include <vm/vm_map_xnu.h>

#include <libkern/OSAtomic.h>

#include <machine/pal_routines.h>
#include <machine/atomic.h>


extern unsigned int wake_nkdbufs;
extern unsigned int trace_wrap;

// Coprocessors (or "IOP"s)
//
// Coprocessors are auxiliary cores that want to participate in kdebug event
// logging.  They are registered dynamically, as devices match hardware, and are
// each assigned an ID at registration.
//
// Once registered, a coprocessor is permanent; it cannot be unregistered.
// The current implementation depends on this for thread safety.
//
// The `kd_coprocs` list may be safely walked at any time, without holding
// locks.
//
// When starting a trace session, the current `kd_coprocs` head is captured. Any
// operations that depend on the buffer state (such as flushing IOP traces on
// reads, etc.) should use the captured list head. This will allow registrations
// to take place while trace is in use, though their events will be rejected
// until the next time a trace session is started.

struct kd_coproc {
	char                  full_name[32];
	kdebug_coproc_flags_t flags;
	kd_callback_t         callback;
	uint32_t              cpu_id;
	struct kd_coproc     *next;
	struct mpsc_queue_chain chain;
};

static struct kd_coproc *kd_coprocs = NULL;

// Use an MPSC queue to notify coprocessors of the current trace state during
// registration, if space is available for them in the current trace session.
static struct mpsc_daemon_queue _coproc_notify_queue;

// Typefilter(s)
//
// A typefilter is a 8KB bitmap that is used to selectively filter events
// being recorded. It is able to individually address every class & subclass.
//
// There is a shared typefilter in the kernel which is lazily allocated. Once
// allocated, the shared typefilter is never deallocated. The shared typefilter
// is also mapped on demand into userspace processes that invoke kdebug_trace
// API from Libsyscall. When mapped into a userspace process, the memory is
// read only, and does not have a fixed address.
//
// It is a requirement that the kernel's shared typefilter always pass DBG_TRACE
// events. This is enforced automatically, by having the needed bits set any
// time the shared typefilter is mutated.

typedef uint8_t *typefilter_t;

static typefilter_t kdbg_typefilter;
static mach_port_t kdbg_typefilter_memory_entry;

/*
 * There are 3 combinations of page sizes:
 *
 *  4KB /  4KB
 *  4KB / 16KB
 * 16KB / 16KB
 *
 * The typefilter is exactly 8KB. In the first two scenarios, we would like
 * to use 2 pages exactly; in the third scenario we must make certain that
 * a full page is allocated so we do not inadvertantly share 8KB of random
 * data to userspace. The round_page_32 macro rounds to kernel page size.
 */
#define TYPEFILTER_ALLOC_SIZE MAX(round_page_32(KDBG_TYPEFILTER_BITMAP_SIZE), KDBG_TYPEFILTER_BITMAP_SIZE)

static typefilter_t
typefilter_create(void)
{
	typefilter_t tf;
	if (KERN_SUCCESS == kmem_alloc(kernel_map, (vm_offset_t*)&tf,
	    TYPEFILTER_ALLOC_SIZE, KMA_DATA | KMA_ZERO, VM_KERN_MEMORY_DIAG)) {
		return tf;
	}
	return NULL;
}

static void
typefilter_deallocate(typefilter_t tf)
{
	assert(tf != NULL);
	assert(tf != kdbg_typefilter);
	kmem_free(kernel_map, (vm_offset_t)tf, TYPEFILTER_ALLOC_SIZE);
}

static void
typefilter_copy(typefilter_t dst, typefilter_t src)
{
	assert(src != NULL);
	assert(dst != NULL);
	memcpy(dst, src, KDBG_TYPEFILTER_BITMAP_SIZE);
}

static void
typefilter_reject_all(typefilter_t tf)
{
	assert(tf != NULL);
	memset(tf, 0, KDBG_TYPEFILTER_BITMAP_SIZE);
}

static void
typefilter_allow_all(typefilter_t tf)
{
	assert(tf != NULL);
	memset(tf, ~0, KDBG_TYPEFILTER_BITMAP_SIZE);
}

static void
typefilter_allow_class(typefilter_t tf, uint8_t class)
{
	assert(tf != NULL);
	const uint32_t BYTES_PER_CLASS = 256 / 8; // 256 subclasses, 1 bit each
	memset(&tf[class * BYTES_PER_CLASS], 0xFF, BYTES_PER_CLASS);
}

static void
typefilter_allow_csc(typefilter_t tf, uint16_t csc)
{
	assert(tf != NULL);
	setbit(tf, csc);
}

static bool
typefilter_is_debugid_allowed(typefilter_t tf, uint32_t id)
{
	assert(tf != NULL);
	return isset(tf, KDBG_EXTRACT_CSC(id));
}

static mach_port_t
typefilter_create_memory_entry(typefilter_t tf)
{
	assert(tf != NULL);

	mach_port_t memory_entry = MACH_PORT_NULL;
	memory_object_size_t size = TYPEFILTER_ALLOC_SIZE;

	kern_return_t kr = mach_make_memory_entry_64(kernel_map,
	    &size,
	    (memory_object_offset_t)tf,
	    VM_PROT_READ,
	    &memory_entry,
	    MACH_PORT_NULL);
	if (kr != KERN_SUCCESS) {
		return MACH_PORT_NULL;
	}

	return memory_entry;
}

static int  kdbg_copyin_typefilter(user_addr_t addr, size_t size);
static void kdbg_enable_typefilter(void);
static void kdbg_disable_typefilter(void);

// External prototypes

void commpage_update_kdebug_state(void);

static int kdbg_readcurthrmap(user_addr_t, size_t *);
static int kdbg_setpidex(kd_regtype *);
static int kdbg_setpid(kd_regtype *);
static int kdbg_reinit(unsigned int extra_cpus);
#if DEVELOPMENT || DEBUG
static int kdbg_test(size_t flavor);
#endif /* DEVELOPMENT || DEBUG */

static int _write_legacy_header(bool write_thread_map, vnode_t vp,
    vfs_context_t ctx);
static int kdbg_write_thread_map(vnode_t vp, vfs_context_t ctx);
static int kdbg_copyout_thread_map(user_addr_t buffer, size_t *buffer_size);
static void _clear_thread_map(void);

static bool kdbg_wait(uint64_t timeout_ms);
static void kdbg_wakeup(void);

static int _copy_cpu_map(int version, void **dst, size_t *size);

static kd_threadmap *_thread_map_create_live(size_t max_count,
    vm_size_t *map_size, vm_size_t *map_count);

static bool kdebug_current_proc_enabled(uint32_t debugid);
static errno_t kdebug_check_trace_string(uint32_t debugid, uint64_t str_id);

int kernel_debug_trace_write_to_file(user_addr_t *buffer, size_t *number,
    size_t *count, size_t tempbuf_number, vnode_t vp, vfs_context_t ctx,
    bool chunk);

extern void IOSleep(int);

unsigned int kdebug_enable = 0;

// A static buffer to record events prior to the start of regular logging.

#define KD_EARLY_BUFFER_SIZE (16 * 1024)
#define KD_EARLY_EVENT_COUNT (KD_EARLY_BUFFER_SIZE / sizeof(kd_buf))
#if defined(__x86_64__)
__attribute__((aligned(KD_EARLY_BUFFER_SIZE)))
static kd_buf kd_early_buffer[KD_EARLY_EVENT_COUNT];
#else /* defined(__x86_64__) */
// On ARM, the space for this is carved out by osfmk/arm/data.s -- clang
// has problems aligning to greater than 4K.
extern kd_buf kd_early_buffer[KD_EARLY_EVENT_COUNT];
#endif /* !defined(__x86_64__) */

static __security_const_late unsigned int kd_early_index = 0;
static __security_const_late bool kd_early_overflow = false;
static __security_const_late bool kd_early_done = false;

static bool kd_waiter = false;
static LCK_SPIN_DECLARE(kd_wait_lock, &kdebug_lck_grp);
// Synchronize access to coprocessor list for kdebug trace.
static LCK_SPIN_DECLARE(kd_coproc_spinlock, &kdebug_lck_grp);

#define TRACE_KDCOPYBUF_COUNT 8192
#define TRACE_KDCOPYBUF_SIZE  (TRACE_KDCOPYBUF_COUNT * sizeof(kd_buf))

struct kd_control kd_control_trace = {
	.kds_free_list = {.raw = KDS_PTR_NULL},
	.enabled = 0,
	.mode = KDEBUG_MODE_TRACE,
	.kdebug_events_per_storage_unit = TRACE_EVENTS_PER_STORAGE_UNIT,
	.kdebug_min_storage_units_per_cpu = TRACE_MIN_STORAGE_UNITS_PER_CPU,
	.kdebug_kdcopybuf_count = TRACE_KDCOPYBUF_COUNT,
	.kdebug_kdcopybuf_size = TRACE_KDCOPYBUF_SIZE,
	.kdc_flags = 0,
	.kdc_emit = KDEMIT_DISABLE,
	.kdc_oldest_time = 0
};

struct kd_buffer kd_buffer_trace = {
	.kdb_event_count = 0,
	.kdb_storage_count = 0,
	.kdb_storage_threshold = 0,
	.kdb_region_count = 0,
	.kdb_info = NULL,
	.kd_bufs = NULL,
	.kdcopybuf = NULL
};

unsigned int kdlog_beg = 0;
unsigned int kdlog_end = 0;
unsigned int kdlog_value1 = 0;
unsigned int kdlog_value2 = 0;
unsigned int kdlog_value3 = 0;
unsigned int kdlog_value4 = 0;

kd_threadmap *kd_mapptr = 0;
vm_size_t kd_mapsize = 0;
vm_size_t kd_mapcount = 0;

off_t RAW_file_offset = 0;
int   RAW_file_written = 0;

/*
 * A globally increasing counter for identifying strings in trace.  Starts at
 * 1 because 0 is a reserved return value.
 */
__attribute__((aligned(MAX_CPU_CACHE_LINE_SIZE)))
static uint64_t g_curr_str_id = 1;

#define STR_ID_SIG_OFFSET (48)
#define STR_ID_MASK       ((1ULL << STR_ID_SIG_OFFSET) - 1)
#define STR_ID_SIG_MASK   (~STR_ID_MASK)

/*
 * A bit pattern for identifying string IDs generated by
 * kdebug_trace_string(2).
 */
static uint64_t g_str_id_signature = (0x70acULL << STR_ID_SIG_OFFSET);

#define RAW_VERSION3    0x00001000

#define V3_RAW_EVENTS   0x00001e00

static void
_coproc_lock(void)
{
	lck_spin_lock_grp(&kd_coproc_spinlock, &kdebug_lck_grp);
}

static void
_coproc_unlock(void)
{
	lck_spin_unlock(&kd_coproc_spinlock);
}

static void
_coproc_list_check(void)
{
#if MACH_ASSERT
	_coproc_lock();
	struct kd_coproc *coproc = kd_control_trace.kdc_coprocs;
	if (coproc) {
		/* Is list sorted by cpu_id? */
		struct kd_coproc* temp = coproc;
		do {
			assert(!temp->next || temp->next->cpu_id == temp->cpu_id - 1);
			assert(temp->next || (temp->cpu_id == kdbg_cpu_count()));
		} while ((temp = temp->next));

		/* Does each entry have a function and a name? */
		temp = coproc;
		do {
			assert(temp->callback.func);
			assert(strlen(temp->callback.iop_name) < sizeof(temp->callback.iop_name));
		} while ((temp = temp->next));
	}
	_coproc_unlock();
#endif // MACH_ASSERT
}

static void
_coproc_list_callback(kd_callback_type type, void *arg)
{
	if (kd_control_trace.kdc_flags & KDBG_DISABLE_COPROCS) {
		return;
	}

	_coproc_lock();
	// Coprocessor list is only ever prepended to.
	struct kd_coproc *head = kd_control_trace.kdc_coprocs;
	_coproc_unlock();
	while (head) {
		head->callback.func(head->callback.context, type, arg);
		head = head->next;
	}
}

// Leave some extra space for coprocessors to register while tracing is active.
#define EXTRA_COPROC_COUNT      (16)
// There are more coprocessors registering during boot tracing.
#define EXTRA_COPROC_COUNT_BOOT (32)

static kdebug_emit_filter_t
_trace_emit_filter(void)
{
	if (!kdebug_enable) {
		return KDEMIT_DISABLE;
	} else if (kd_control_trace.kdc_flags & KDBG_TYPEFILTER_CHECK) {
		return KDEMIT_TYPEFILTER;
	} else if (kd_control_trace.kdc_flags & KDBG_RANGECHECK) {
		return KDEMIT_RANGE;
	} else if (kd_control_trace.kdc_flags & KDBG_VALCHECK) {
		return KDEMIT_EXACT;
	} else {
		return KDEMIT_ALL;
	}
}

static void
kdbg_set_tracing_enabled(bool enabled, uint32_t trace_type)
{
	// Drain any events from coprocessors before making the state change.  On
	// enabling, this removes any stale events from before tracing.  On
	// disabling, this saves any events up to the point tracing is disabled.
	_coproc_list_callback(KD_CALLBACK_SYNC_FLUSH, NULL);

	if (!enabled) {
		// Give coprocessors a chance to log any events before tracing is
		// disabled, outside the lock.
		_coproc_list_callback(KD_CALLBACK_KDEBUG_DISABLED, NULL);
	}

	int intrs_en = kdebug_storage_lock(&kd_control_trace);
	if (enabled) {
		// The oldest valid time is now; reject past events from coprocessors.
		kd_control_trace.kdc_oldest_time = kdebug_timestamp();
		kdebug_enable |= trace_type;
		kd_control_trace.kdc_emit = _trace_emit_filter();
		kd_control_trace.enabled = 1;
		commpage_update_kdebug_state();
	} else {
		kdebug_enable = 0;
		kd_control_trace.kdc_emit = KDEMIT_DISABLE;
		kd_control_trace.enabled = 0;
		commpage_update_kdebug_state();
	}
	kdebug_storage_unlock(&kd_control_trace, intrs_en);

	if (enabled) {
		_coproc_list_callback(KD_CALLBACK_KDEBUG_ENABLED, NULL);
	}
}

static int
create_buffers_trace(unsigned int extra_cpus)
{
	int events_per_storage_unit = kd_control_trace.kdebug_events_per_storage_unit;
	int min_storage_units_per_cpu = kd_control_trace.kdebug_min_storage_units_per_cpu;

	// For the duration of this allocation, trace code will only reference
	// kdc_coprocs.
	kd_control_trace.kdc_coprocs = kd_coprocs;
	_coproc_list_check();

	// If the list is valid, it is sorted from newest to oldest.  Each entry is
	// prepended, so the CPU IDs are sorted in descending order.
	kd_control_trace.kdebug_cpus = kd_control_trace.kdc_coprocs ?
	    kd_control_trace.kdc_coprocs->cpu_id + 1 : kdbg_cpu_count();
	kd_control_trace.alloc_cpus = kd_control_trace.kdebug_cpus + extra_cpus;

	size_t min_event_count = kd_control_trace.alloc_cpus *
	    events_per_storage_unit * min_storage_units_per_cpu;
	if (kd_buffer_trace.kdb_event_count < min_event_count) {
		kd_buffer_trace.kdb_storage_count = kd_control_trace.alloc_cpus * min_storage_units_per_cpu;
	} else {
		kd_buffer_trace.kdb_storage_count = kd_buffer_trace.kdb_event_count / events_per_storage_unit;
	}

	kd_buffer_trace.kdb_event_count = kd_buffer_trace.kdb_storage_count * events_per_storage_unit;

	kd_buffer_trace.kd_bufs = NULL;

	int error = create_buffers(&kd_control_trace, &kd_buffer_trace,
	    VM_KERN_MEMORY_DIAG);
	if (!error) {
		struct kd_bufinfo *info = kd_buffer_trace.kdb_info;
		struct kd_coproc *cur_iop = kd_control_trace.kdc_coprocs;
		while (cur_iop != NULL) {
			info[cur_iop->cpu_id].continuous_timestamps = ISSET(cur_iop->flags,
			    KDCP_CONTINUOUS_TIME);
			cur_iop = cur_iop->next;
		}
		kd_buffer_trace.kdb_storage_threshold = kd_buffer_trace.kdb_storage_count / 2;
	}

	return error;
}

static void
delete_buffers_trace(void)
{
	delete_buffers(&kd_control_trace, &kd_buffer_trace);
}

static int
_register_coproc_internal(const char *name, kdebug_coproc_flags_t flags,
    kd_callback_fn callback, void *context)
{
	struct kd_coproc *coproc = NULL;

	coproc = zalloc_permanent_type(struct kd_coproc);
	coproc->callback.func = callback;
	coproc->callback.context = context;
	coproc->flags = flags;
	strlcpy(coproc->full_name, name, sizeof(coproc->full_name));

	_coproc_lock();
	coproc->next = kd_coprocs;
	coproc->cpu_id = kd_coprocs == NULL ? kdbg_cpu_count() : kd_coprocs->cpu_id + 1;
	kd_coprocs = coproc;
	if (coproc->cpu_id < kd_control_trace.alloc_cpus) {
		kd_control_trace.kdc_coprocs = kd_coprocs;
		kd_control_trace.kdebug_cpus += 1;
		if (kdebug_enable) {
			mpsc_daemon_enqueue(&_coproc_notify_queue, &coproc->chain,
			    MPSC_QUEUE_NONE);
		}
	}
	_coproc_unlock();

	return coproc->cpu_id;
}

int
kernel_debug_register_callback(kd_callback_t callback)
{
	// Be paranoid about using the provided name, but it's too late to reject
	// it.
	bool is_valid_name = false;
	for (uint32_t length = 0; length < sizeof(callback.iop_name); ++length) {
		if (callback.iop_name[length] > 0x20 && callback.iop_name[length] < 0x7F) {
			continue;
		}
		if (callback.iop_name[length] == 0) {
			if (length) {
				is_valid_name = true;
			}
			break;
		}
	}
	kd_callback_t sane_cb = callback;
	if (!is_valid_name) {
		strlcpy(sane_cb.iop_name, "IOP-???", sizeof(sane_cb.iop_name));
	}

	return _register_coproc_internal(sane_cb.iop_name, 0, sane_cb.func,
	           sane_cb.context);
}

int
kdebug_register_coproc(const char *name, kdebug_coproc_flags_t flags,
    kd_callback_fn callback, void *context)
{
	size_t name_len = strlen(name);
	if (!name || name_len == 0) {
		panic("kdebug: invalid name for coprocessor: %p", name);
	}
	for (size_t i = 0; i < name_len; i++) {
		if (name[i] <= 0x20 || name[i] >= 0x7F) {
			panic("kdebug: invalid name for coprocessor: %s", name);
		}
	}
	if (!callback) {
		panic("kdebug: no callback for coprocessor `%s'", name);
	}
	return _register_coproc_internal(name, flags, callback, context);
}

static inline bool
_should_emit_debugid(kdebug_emit_filter_t emit, uint32_t debugid)
{
	switch (emit) {
	case KDEMIT_DISABLE:
		return false;
	case KDEMIT_TYPEFILTER:
		return typefilter_is_debugid_allowed(kdbg_typefilter, debugid);
	case KDEMIT_RANGE:
		return debugid >= kdlog_beg && debugid <= kdlog_end;
	case KDEMIT_EXACT:;
		uint32_t eventid = debugid & KDBG_EVENTID_MASK;
		return eventid == kdlog_value1 || eventid == kdlog_value2 ||
		       eventid == kdlog_value3 || eventid == kdlog_value4;
	case KDEMIT_ALL:
		return true;
	}
}

static void
_try_wakeup_above_threshold(uint32_t debugid)
{
	bool over_threshold = kd_control_trace.kdc_storage_used >=
	    kd_buffer_trace.kdb_storage_threshold;
	if (kd_waiter && over_threshold) {
		// Wakeup any waiters if called from a safe context.

		const uint32_t INTERRUPT_EVENT = 0x01050000;
		const uint32_t VMFAULT_EVENT = 0x01300008;
		const uint32_t BSD_SYSCALL_CSC = 0x040c0000;
		const uint32_t MACH_SYSCALL_CSC = 0x010c0000;

		uint32_t eventid = debugid & KDBG_EVENTID_MASK;
		uint32_t csc = debugid & KDBG_CSC_MASK;

		if (eventid == INTERRUPT_EVENT || eventid == VMFAULT_EVENT ||
		    csc == BSD_SYSCALL_CSC || csc == MACH_SYSCALL_CSC) {
			kdbg_wakeup();
		}
	}
}

// Emit events from coprocessors.
void
kernel_debug_enter(
	uint32_t  coreid,
	uint32_t  debugid,
	uint64_t  timestamp,
	uintptr_t arg1,
	uintptr_t arg2,
	uintptr_t arg3,
	uintptr_t arg4,
	uintptr_t threadid
	)
{
	if (kd_control_trace.kdc_flags & KDBG_DISABLE_COPROCS) {
		return;
	}
	kdebug_emit_filter_t emit = kd_control_trace.kdc_emit;
	if (!emit || !kdebug_enable) {
		return;
	}
	if (!_should_emit_debugid(emit, debugid)) {
		return;
	}

	struct kd_record kd_rec = {
		.cpu = (int32_t)coreid,
		.timestamp = (int64_t)timestamp,
		.debugid = debugid,
		.arg1 = arg1,
		.arg2 = arg2,
		.arg3 = arg3,
		.arg4 = arg4,
		.arg5 = threadid,
	};
	kernel_debug_write(&kd_control_trace, &kd_buffer_trace, kd_rec);
}

__pure2
static inline proc_t
kdebug_current_proc_unsafe(void)
{
	return get_thread_ro_unchecked(current_thread())->tro_proc;
}

// Return true iff the debug ID should be traced by the current process.
static inline bool
kdebug_debugid_procfilt_allowed(uint32_t debugid)
{
	uint32_t procfilt_flags = kd_control_trace.kdc_flags &
	    (KDBG_PIDCHECK | KDBG_PIDEXCLUDE);
	if (!procfilt_flags) {
		return true;
	}

	// DBG_TRACE and MACH_SCHED tracepoints ignore the process filter.
	if ((debugid & KDBG_CSC_MASK) == MACHDBG_CODE(DBG_MACH_SCHED, 0) ||
	    (KDBG_EXTRACT_CLASS(debugid) == DBG_TRACE)) {
		return true;
	}

	struct proc *curproc = kdebug_current_proc_unsafe();
	// If the process is missing (early in boot), allow it.
	if (!curproc) {
		return true;
	}

	switch (procfilt_flags) {
	case KDBG_PIDCHECK:
		return curproc->p_kdebug;
	case KDBG_PIDEXCLUDE:
		return !curproc->p_kdebug;
	default:
		panic("kdebug: invalid procfilt flags %x", kd_control_trace.kdc_flags);
	}
}

static void
kdebug_emit_internal(kdebug_emit_filter_t emit,
    uint32_t debugid,
    uintptr_t arg1,
    uintptr_t arg2,
    uintptr_t arg3,
    uintptr_t arg4,
    uintptr_t arg5,
    uint64_t flags)
{
	bool only_filter = flags & KDBG_FLAG_FILTERED;
	bool observe_procfilt = !(flags & KDBG_FLAG_NOPROCFILT);

	if (!_should_emit_debugid(emit, debugid)) {
		return;
	}
	if (emit == KDEMIT_ALL && only_filter) {
		return;
	}
	if (!ml_at_interrupt_context() && observe_procfilt &&
	    !kdebug_debugid_procfilt_allowed(debugid)) {
		return;
	}

	struct kd_record kd_rec = {
		.cpu = -1,
		.timestamp = -1,
		.debugid = debugid,
		.arg1 = arg1,
		.arg2 = arg2,
		.arg3 = arg3,
		.arg4 = arg4,
		.arg5 = arg5,
	};
	kernel_debug_write(&kd_control_trace, &kd_buffer_trace, kd_rec);

#if KPERF
	kperf_kdebug_callback(kd_rec.debugid, __builtin_frame_address(0));
#endif // KPERF
}

static void
kernel_debug_internal(
	uint32_t debugid,
	uintptr_t arg1,
	uintptr_t arg2,
	uintptr_t arg3,
	uintptr_t arg4,
	uintptr_t arg5,
	uint64_t flags)
{
	kdebug_emit_filter_t emit = kd_control_trace.kdc_emit;
	if (!emit || !kdebug_enable) {
		return;
	}
	kdebug_emit_internal(emit, debugid, arg1, arg2, arg3, arg4, arg5, flags);
	_try_wakeup_above_threshold(debugid);
}

__attribute__((noinline))
void
kernel_debug(uint32_t debugid, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3,
    uintptr_t arg4, __unused uintptr_t arg5)
{
	kernel_debug_internal(debugid, arg1, arg2, arg3, arg4,
	    (uintptr_t)thread_tid(current_thread()), 0);
}

__attribute__((noinline))
void
kernel_debug1(uint32_t debugid, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3,
    uintptr_t arg4, uintptr_t arg5)
{
	kernel_debug_internal(debugid, arg1, arg2, arg3, arg4, arg5, 0);
}

__attribute__((noinline))
void
kernel_debug_flags(
	uint32_t debugid,
	uintptr_t arg1,
	uintptr_t arg2,
	uintptr_t arg3,
	uintptr_t arg4,
	uint64_t flags)
{
	kernel_debug_internal(debugid, arg1, arg2, arg3, arg4,
	    (uintptr_t)thread_tid(current_thread()), flags);
}

__attribute__((noinline))
void
kernel_debug_filtered(
	uint32_t debugid,
	uintptr_t arg1,
	uintptr_t arg2,
	uintptr_t arg3,
	uintptr_t arg4)
{
	kernel_debug_flags(debugid, arg1, arg2, arg3, arg4, KDBG_FLAG_FILTERED);
}

void
kernel_debug_string_early(const char *message)
{
	uintptr_t a[4] = { 0 };
	strncpy((char *)a, message, sizeof(a));
	KERNEL_DEBUG_EARLY(TRACE_INFO_STRING, a[0], a[1], a[2], a[3]);
}

#define SIMPLE_STR_LEN (64)
static_assert(SIMPLE_STR_LEN % sizeof(uintptr_t) == 0);

void
kernel_debug_string_simple(uint32_t eventid, const char *str)
{
	if (!kdebug_enable) {
		return;
	}

	/* array of uintptr_ts simplifies emitting the string as arguments */
	uintptr_t str_buf[(SIMPLE_STR_LEN / sizeof(uintptr_t)) + 1] = { 0 };
	size_t len = strlcpy((char *)str_buf, str, SIMPLE_STR_LEN + 1);
	len = MIN(len, SIMPLE_STR_LEN);

	uintptr_t thread_id = (uintptr_t)thread_tid(current_thread());
	uint32_t debugid = eventid | DBG_FUNC_START;

	/* string can fit in a single tracepoint */
	if (len <= (4 * sizeof(uintptr_t))) {
		debugid |= DBG_FUNC_END;
	}

	kernel_debug_internal(debugid, str_buf[0], str_buf[1], str_buf[2],
	    str_buf[3], thread_id, 0);

	debugid &= KDBG_EVENTID_MASK;
	int i = 4;
	size_t written = 4 * sizeof(uintptr_t);

	for (; written < len; i += 4, written += 4 * sizeof(uintptr_t)) {
		/* if this is the last tracepoint to be emitted */
		if ((written + (4 * sizeof(uintptr_t))) >= len) {
			debugid |= DBG_FUNC_END;
		}
		kernel_debug_internal(debugid, str_buf[i],
		    str_buf[i + 1],
		    str_buf[i + 2],
		    str_buf[i + 3], thread_id, 0);
	}
}

extern int      master_cpu;             /* MACH_KERNEL_PRIVATE */
/*
 * Used prior to start_kern_tracing() being called.
 * Log temporarily into a static buffer.
 */
void
kernel_debug_early(
	uint32_t        debugid,
	uintptr_t       arg1,
	uintptr_t       arg2,
	uintptr_t       arg3,
	uintptr_t       arg4)
{
#if defined(__x86_64__)
	extern int early_boot;
	/*
	 * Note that "early" isn't early enough in some cases where
	 * we're invoked before gsbase is set on x86, hence the
	 * check of "early_boot".
	 */
	if (early_boot) {
		return;
	}
#endif

	/* If early tracing is over, use the normal path. */
	if (kd_early_done) {
		KDBG_RELEASE(debugid, arg1, arg2, arg3, arg4);
		return;
	}

	/* Do nothing if the buffer is full or we're not on the boot cpu. */
	kd_early_overflow = kd_early_index >= KD_EARLY_EVENT_COUNT;
	if (kd_early_overflow || cpu_number() != master_cpu) {
		return;
	}

	kd_early_buffer[kd_early_index].debugid = debugid;
	kd_early_buffer[kd_early_index].timestamp = mach_absolute_time();
	kd_early_buffer[kd_early_index].arg1 = arg1;
	kd_early_buffer[kd_early_index].arg2 = arg2;
	kd_early_buffer[kd_early_index].arg3 = arg3;
	kd_early_buffer[kd_early_index].arg4 = arg4;
	kd_early_buffer[kd_early_index].arg5 = 0;
	kd_early_index++;
}

/*
 * Transfer the contents of the temporary buffer into the trace buffers.
 * Precede that by logging the rebase time (offset) - the TSC-based time (in ns)
 * when mach_absolute_time is set to 0.
 */
static void
kernel_debug_early_end(void)
{
	if (cpu_number() != master_cpu) {
		panic("kernel_debug_early_end() not call on boot processor");
	}

	/* reset the current oldest time to allow early events */
	kd_control_trace.kdc_oldest_time = 0;

#if defined(__x86_64__)
	/* Fake sentinel marking the start of kernel time relative to TSC */
	kernel_debug_enter(0, TRACE_TIMESTAMPS, 0,
	    (uint32_t)(tsc_rebase_abs_time >> 32), (uint32_t)tsc_rebase_abs_time,
	    tsc_at_boot, 0, 0);
#endif /* defined(__x86_64__) */
	for (unsigned int i = 0; i < kd_early_index; i++) {
		kernel_debug_enter(0,
		    kd_early_buffer[i].debugid,
		    kd_early_buffer[i].timestamp,
		    kd_early_buffer[i].arg1,
		    kd_early_buffer[i].arg2,
		    kd_early_buffer[i].arg3,
		    kd_early_buffer[i].arg4,
		    0);
	}

	/* Cut events-lost event on overflow */
	if (kd_early_overflow) {
		KDBG_RELEASE(TRACE_LOST_EVENTS, 1);
	}

	kd_early_done = true;

	/* This trace marks the start of kernel tracing */
	kernel_debug_string_early("early trace done");
}

void
kernel_debug_disable(void)
{
	if (kdebug_enable) {
		kdbg_set_tracing_enabled(false, 0);
		kdbg_wakeup();
	}
}

// Returns true if debugid should only be traced from the kernel.
static int
_kernel_only_event(uint32_t debugid)
{
	return KDBG_EXTRACT_CLASS(debugid) == DBG_TRACE;
}

/*
 * Support syscall SYS_kdebug_typefilter.
 */
int
kdebug_typefilter(__unused struct proc* p, struct kdebug_typefilter_args* uap,
    __unused int *retval)
{
	if (uap->addr == USER_ADDR_NULL || uap->size == USER_ADDR_NULL) {
		return EINVAL;
	}

	mach_vm_offset_t user_addr = 0;
	vm_map_t user_map = current_map();
	const bool copy = false;
	kern_return_t kr = mach_vm_map_kernel(user_map, &user_addr,
	    TYPEFILTER_ALLOC_SIZE, 0, VM_MAP_KERNEL_FLAGS_ANYWHERE(),
	    kdbg_typefilter_memory_entry, 0, copy,
	    VM_PROT_READ, VM_PROT_READ, VM_INHERIT_SHARE);
	if (kr != KERN_SUCCESS) {
		return mach_to_bsd_errno(kr);
	}

	vm_size_t user_ptr_size = vm_map_is_64bit(user_map) ? 8 : 4;
	int error = copyout((void *)&user_addr, uap->addr, user_ptr_size);
	if (error != 0) {
		mach_vm_deallocate(user_map, user_addr, TYPEFILTER_ALLOC_SIZE);
	}
	return error;
}

// Support SYS_kdebug_trace.
int
kdebug_trace(struct proc *p, struct kdebug_trace_args *uap, int32_t *retval)
{
	struct kdebug_trace64_args uap64 = {
		.code = uap->code,
		.arg1 = uap->arg1,
		.arg2 = uap->arg2,
		.arg3 = uap->arg3,
		.arg4 = uap->arg4,
	};
	return kdebug_trace64(p, &uap64, retval);
}

// Support kdebug_trace(2).  64-bit arguments on K32 will get truncated
// to fit in the 32-bit record format.
//
// It is intentional that error conditions are not checked until kdebug is
// enabled. This is to match the userspace wrapper behavior, which is optimizing
// for non-error case performance.
int
kdebug_trace64(__unused struct proc *p, struct kdebug_trace64_args *uap,
    __unused int32_t *retval)
{
	if (__probable(kdebug_enable == 0)) {
		return 0;
	}
	if (_kernel_only_event(uap->code)) {
		return EPERM;
	}
	kernel_debug_internal(uap->code, (uintptr_t)uap->arg1,
	    (uintptr_t)uap->arg2, (uintptr_t)uap->arg3, (uintptr_t)uap->arg4,
	    (uintptr_t)thread_tid(current_thread()), 0);
	return 0;
}

/*
 * Adding enough padding to contain a full tracepoint for the last
 * portion of the string greatly simplifies the logic of splitting the
 * string between tracepoints.  Full tracepoints can be generated using
 * the buffer itself, without having to manually add zeros to pad the
 * arguments.
 */

/* 2 string args in first tracepoint and 9 string data tracepoints */
#define STR_BUF_ARGS (2 + (32 * 4))
/* times the size of each arg on K64 */
#define MAX_STR_LEN  (STR_BUF_ARGS * sizeof(uint64_t))
/* on K32, ending straddles a tracepoint, so reserve blanks */
#define STR_BUF_SIZE (MAX_STR_LEN + (2 * sizeof(uint32_t)))

/*
 * This function does no error checking and assumes that it is called with
 * the correct arguments, including that the buffer pointed to by str is at
 * least STR_BUF_SIZE bytes.  However, str must be aligned to word-size and
 * be NUL-terminated.  In cases where a string can fit evenly into a final
 * tracepoint without its NUL-terminator, this function will not end those
 * strings with a NUL in trace.  It's up to clients to look at the function
 * qualifier for DBG_FUNC_END in this case, to end the string.
 */
static uint64_t
kernel_debug_string_internal(uint32_t debugid, uint64_t str_id, void *vstr,
    size_t str_len)
{
	/* str must be word-aligned */
	uintptr_t *str = vstr;
	size_t written = 0;
	uintptr_t thread_id;
	int i;
	uint32_t trace_debugid = TRACEDBG_CODE(DBG_TRACE_STRING,
	    TRACE_STRING_GLOBAL);

	thread_id = (uintptr_t)thread_tid(current_thread());

	/* if the ID is being invalidated, just emit that */
	if (str_id != 0 && str_len == 0) {
		kernel_debug_internal(trace_debugid | DBG_FUNC_START | DBG_FUNC_END,
		    (uintptr_t)debugid, (uintptr_t)str_id, 0, 0, thread_id, 0);
		return str_id;
	}

	/* generate an ID, if necessary */
	if (str_id == 0) {
		str_id = OSIncrementAtomic64((SInt64 *)&g_curr_str_id);
		str_id = (str_id & STR_ID_MASK) | g_str_id_signature;
	}

	trace_debugid |= DBG_FUNC_START;
	/* string can fit in a single tracepoint */
	if (str_len <= (2 * sizeof(uintptr_t))) {
		trace_debugid |= DBG_FUNC_END;
	}

	kernel_debug_internal(trace_debugid, (uintptr_t)debugid, (uintptr_t)str_id,
	    str[0], str[1], thread_id, 0);

	trace_debugid &= KDBG_EVENTID_MASK;
	i = 2;
	written += 2 * sizeof(uintptr_t);

	for (; written < str_len; i += 4, written += 4 * sizeof(uintptr_t)) {
		if ((written + (4 * sizeof(uintptr_t))) >= str_len) {
			trace_debugid |= DBG_FUNC_END;
		}
		kernel_debug_internal(trace_debugid, str[i],
		    str[i + 1],
		    str[i + 2],
		    str[i + 3], thread_id, 0);
	}

	return str_id;
}

/*
 * Returns true if the current process can emit events, and false otherwise.
 * Trace system and scheduling events circumvent this check, as do events
 * emitted in interrupt context.
 */
static bool
kdebug_current_proc_enabled(uint32_t debugid)
{
	/* can't determine current process in interrupt context */
	if (ml_at_interrupt_context()) {
		return true;
	}

	/* always emit trace system and scheduling events */
	if ((KDBG_EXTRACT_CLASS(debugid) == DBG_TRACE ||
	    (debugid & KDBG_CSC_MASK) == MACHDBG_CODE(DBG_MACH_SCHED, 0))) {
		return true;
	}

	if (kd_control_trace.kdc_flags & KDBG_PIDCHECK) {
		proc_t cur_proc = kdebug_current_proc_unsafe();

		/* only the process with the kdebug bit set is allowed */
		if (cur_proc && !(cur_proc->p_kdebug)) {
			return false;
		}
	} else if (kd_control_trace.kdc_flags & KDBG_PIDEXCLUDE) {
		proc_t cur_proc = kdebug_current_proc_unsafe();

		/* every process except the one with the kdebug bit set is allowed */
		if (cur_proc && cur_proc->p_kdebug) {
			return false;
		}
	}

	return true;
}

bool
kdebug_debugid_enabled(uint32_t debugid)
{
	return _should_emit_debugid(kd_control_trace.kdc_emit, debugid);
}

bool
kdebug_debugid_explicitly_enabled(uint32_t debugid)
{
	if (kd_control_trace.kdc_flags & KDBG_TYPEFILTER_CHECK) {
		return typefilter_is_debugid_allowed(kdbg_typefilter, debugid);
	} else if (KDBG_EXTRACT_CLASS(debugid) == DBG_TRACE) {
		return true;
	} else if (kd_control_trace.kdc_flags & KDBG_RANGECHECK) {
		if (debugid < kdlog_beg || debugid > kdlog_end) {
			return false;
		}
	} else if (kd_control_trace.kdc_flags & KDBG_VALCHECK) {
		if ((debugid & KDBG_EVENTID_MASK) != kdlog_value1 &&
		    (debugid & KDBG_EVENTID_MASK) != kdlog_value2 &&
		    (debugid & KDBG_EVENTID_MASK) != kdlog_value3 &&
		    (debugid & KDBG_EVENTID_MASK) != kdlog_value4) {
			return false;
		}
	}

	return true;
}

/*
 * Returns 0 if a string can be traced with these arguments.  Returns errno
 * value if error occurred.
 */
static errno_t
kdebug_check_trace_string(uint32_t debugid, uint64_t str_id)
{
	if (debugid & (DBG_FUNC_START | DBG_FUNC_END)) {
		return EINVAL;
	}
	if (_kernel_only_event(debugid)) {
		return EPERM;
	}
	if (str_id != 0 && (str_id & STR_ID_SIG_MASK) != g_str_id_signature) {
		return EINVAL;
	}
	return 0;
}

/*
 * Implementation of KPI kernel_debug_string.
 */
int
kernel_debug_string(uint32_t debugid, uint64_t *str_id, const char *str)
{
	/* arguments to tracepoints must be word-aligned */
	__attribute__((aligned(sizeof(uintptr_t)))) char str_buf[STR_BUF_SIZE];
	static_assert(sizeof(str_buf) > MAX_STR_LEN);
	vm_size_t len_copied;
	int err;

	assert(str_id);

	if (__probable(kdebug_enable == 0)) {
		return 0;
	}

	if (!kdebug_current_proc_enabled(debugid)) {
		return 0;
	}

	if (!kdebug_debugid_enabled(debugid)) {
		return 0;
	}

	if ((err = kdebug_check_trace_string(debugid, *str_id)) != 0) {
		return err;
	}

	if (str == NULL) {
		if (str_id == 0) {
			return EINVAL;
		}

		*str_id = kernel_debug_string_internal(debugid, *str_id, NULL, 0);
		return 0;
	}

	memset(str_buf, 0, sizeof(str_buf));
	len_copied = strlcpy(str_buf, str, MAX_STR_LEN + 1);
	*str_id = kernel_debug_string_internal(debugid, *str_id, str_buf,
	    len_copied);
	return 0;
}

// Support kdebug_trace_string(2).
int
kdebug_trace_string(__unused struct proc *p,
    struct kdebug_trace_string_args *uap,
    uint64_t *retval)
{
	__attribute__((aligned(sizeof(uintptr_t)))) char str_buf[STR_BUF_SIZE];
	static_assert(sizeof(str_buf) > MAX_STR_LEN);
	size_t len_copied;
	int err;

	if (__probable(kdebug_enable == 0)) {
		return 0;
	}

	if (!kdebug_current_proc_enabled(uap->debugid)) {
		return 0;
	}

	if (!kdebug_debugid_enabled(uap->debugid)) {
		return 0;
	}

	if ((err = kdebug_check_trace_string(uap->debugid, uap->str_id)) != 0) {
		return err;
	}

	if (uap->str == USER_ADDR_NULL) {
		if (uap->str_id == 0) {
			return EINVAL;
		}

		*retval = kernel_debug_string_internal(uap->debugid, uap->str_id,
		    NULL, 0);
		return 0;
	}

	memset(str_buf, 0, sizeof(str_buf));
	err = copyinstr(uap->str, str_buf, MAX_STR_LEN + 1, &len_copied);

	/* it's alright to truncate the string, so allow ENAMETOOLONG */
	if (err == ENAMETOOLONG) {
		str_buf[MAX_STR_LEN] = '\0';
	} else if (err) {
		return err;
	}

	if (len_copied <= 1) {
		return EINVAL;
	}

	/* convert back to a length */
	len_copied--;

	*retval = kernel_debug_string_internal(uap->debugid, uap->str_id, str_buf,
	    len_copied);
	return 0;
}

int
kdbg_reinit(unsigned int extra_cpus)
{
	kernel_debug_disable();
	// Wait for any event writers to see the disable status.
	IOSleep(100);
	delete_buffers_trace();

	_clear_thread_map();
	kd_control_trace.kdc_live_flags &= ~KDBG_WRAPPED;

	RAW_file_offset = 0;
	RAW_file_written = 0;

	return create_buffers_trace(extra_cpus);
}

void
kdbg_trace_data(struct proc *proc, long *arg_pid, long *arg_uniqueid)
{
	if (proc) {
		*arg_pid = proc_getpid(proc);
		*arg_uniqueid = (long)proc_uniqueid(proc);
		if ((uint64_t)*arg_uniqueid != proc_uniqueid(proc)) {
			*arg_uniqueid = 0;
		}
	} else {
		*arg_pid = 0;
		*arg_uniqueid = 0;
	}
}

void kdebug_proc_name_args(struct proc *proc, long args[static 4]);
void
kdebug_proc_name_args(struct proc *proc, long args[static 4])
{
	if (proc) {
		strncpy((char *)args, proc_best_name(proc), 4 * sizeof(args[0]));
	}
}

static void
_copy_ap_name(unsigned int cpuid, void *dst, size_t size)
{
	const char *name = "AP";
#if defined(__arm64__)
	const ml_topology_info_t *topology = ml_get_topology_info();
	switch (topology->cpus[cpuid].cluster_type) {
	case CLUSTER_TYPE_E:
		name = "AP-E";
		break;
	case CLUSTER_TYPE_P:
		name = "AP-P";
		break;
	default:
		break;
	}
#else /* defined(__arm64__) */
#pragma unused(cpuid)
#endif /* !defined(__arm64__) */
	strlcpy(dst, name, size);
}

// Write the specified `map_version` of CPU map to the `dst` buffer, using at
// most `size` bytes.  Returns 0 on success and sets `size` to the number of
// bytes written, and either ENOMEM or EINVAL on failure.
//
// If the value pointed to by `dst` is NULL, memory is allocated, and `size` is
// adjusted to the allocated buffer's size.
//
// NB: `coprocs` is used to determine whether the stashed CPU map captured at
// the start of tracing should be used.
static errno_t
_copy_cpu_map(int map_version, void **dst, size_t *size)
{
	_coproc_lock();
	struct kd_coproc *coprocs = kd_control_trace.kdc_coprocs;
	unsigned int cpu_count = kd_control_trace.kdebug_cpus;
	_coproc_unlock();

	assert(cpu_count != 0);
	assert(coprocs == NULL || coprocs[0].cpu_id + 1 == cpu_count);

	bool ext = map_version != RAW_VERSION1;
	size_t stride = ext ? sizeof(kd_cpumap_ext) : sizeof(kd_cpumap);

	size_t size_needed = sizeof(kd_cpumap_header) + cpu_count * stride;
	size_t size_avail = *size;
	*size = size_needed;

	if (*dst == NULL) {
		kern_return_t alloc_ret = kmem_alloc(kernel_map, (vm_offset_t *)dst,
		    (vm_size_t)size_needed, KMA_DATA | KMA_ZERO, VM_KERN_MEMORY_DIAG);
		if (alloc_ret != KERN_SUCCESS) {
			return ENOMEM;
		}
	} else if (size_avail < size_needed) {
		return EINVAL;
	}

	kd_cpumap_header *header = *dst;
	header->version_no = map_version;
	header->cpu_count = cpu_count;

	void *cpus = &header[1];
	size_t name_size = ext ? sizeof(((kd_cpumap_ext *)NULL)->name) :
	    sizeof(((kd_cpumap *)NULL)->name);

	int i = cpu_count - 1;
	for (struct kd_coproc *cur_coproc = coprocs; cur_coproc != NULL;
	    cur_coproc = cur_coproc->next, i--) {
		kd_cpumap_ext *cpu = (kd_cpumap_ext *)((uintptr_t)cpus + stride * i);
		cpu->cpu_id = cur_coproc->cpu_id;
		cpu->flags = KDBG_CPUMAP_IS_IOP;
		strlcpy((void *)&cpu->name, cur_coproc->full_name, name_size);
	}
	for (; i >= 0; i--) {
		kd_cpumap *cpu = (kd_cpumap *)((uintptr_t)cpus + stride * i);
		cpu->cpu_id = i;
		cpu->flags = 0;
		_copy_ap_name(i, &cpu->name, name_size);
	}

	return 0;
}

static void
_threadmap_init(void)
{
	ktrace_assert_lock_held();

	if (kd_control_trace.kdc_flags & KDBG_MAPINIT) {
		return;
	}

	kd_mapptr = _thread_map_create_live(0, &kd_mapsize, &kd_mapcount);

	if (kd_mapptr) {
		kd_control_trace.kdc_flags |= KDBG_MAPINIT;
	}
}

struct kd_resolver {
	kd_threadmap *krs_map;
	vm_size_t krs_count;
	vm_size_t krs_maxcount;
};

static int
_resolve_iterator(proc_t proc, void *opaque)
{
	if (proc == kernproc) {
		/* Handled specially as it lacks uthreads. */
		return PROC_RETURNED;
	}
	struct kd_resolver *resolver = opaque;
	struct uthread *uth = NULL;
	const char *proc_name = proc_best_name(proc);
	pid_t pid = proc_getpid(proc);

	proc_lock(proc);
	TAILQ_FOREACH(uth, &proc->p_uthlist, uu_list) {
		if (resolver->krs_count >= resolver->krs_maxcount) {
			break;
		}
		kd_threadmap *map = &resolver->krs_map[resolver->krs_count];
		map->thread = (uintptr_t)uthread_tid(uth);
		(void)strlcpy(map->command, proc_name, sizeof(map->command));
		map->valid = pid;
		resolver->krs_count++;
	}
	proc_unlock(proc);

	bool done = resolver->krs_count >= resolver->krs_maxcount;
	return done ? PROC_RETURNED_DONE : PROC_RETURNED;
}

static void
_resolve_kernel_task(thread_t thread, void *opaque)
{
	struct kd_resolver *resolver = opaque;
	if (resolver->krs_count >= resolver->krs_maxcount) {
		return;
	}
	kd_threadmap *map = &resolver->krs_map[resolver->krs_count];
	map->thread = (uintptr_t)thread_tid(thread);
	(void)strlcpy(map->command, "kernel_task", sizeof(map->command));
	map->valid = 1;
	resolver->krs_count++;
}

static vm_size_t
_resolve_threads(kd_threadmap *map, vm_size_t nthreads)
{
	struct kd_resolver resolver = {
		.krs_map = map, .krs_count = 0, .krs_maxcount = nthreads,
	};

	// Handle kernel_task specially, as it lacks uthreads.
	extern void task_act_iterate_wth_args(task_t, void (*)(thread_t, void *),
	    void *);
	task_act_iterate_wth_args(kernel_task, _resolve_kernel_task, &resolver);
	proc_iterate(PROC_ALLPROCLIST | PROC_NOWAITTRANS, _resolve_iterator,
	    &resolver, NULL, NULL);
	return resolver.krs_count;
}

static kd_threadmap *
_thread_map_create_live(size_t maxthreads, vm_size_t *mapsize,
    vm_size_t *mapcount)
{
	kd_threadmap *thread_map = NULL;

	assert(mapsize != NULL);
	assert(mapcount != NULL);

	extern int threads_count;
	vm_size_t nthreads = threads_count;

	// Allow 25% more threads to be started while iterating processes.
	if (os_add_overflow(nthreads, nthreads / 4, &nthreads)) {
		return NULL;
	}

	*mapcount = nthreads;
	if (os_mul_overflow(nthreads, sizeof(kd_threadmap), mapsize)) {
		return NULL;
	}

	// Wait until the out-parameters have been filled with the needed size to
	// do the bounds checking on the provided maximum.
	if (maxthreads != 0 && maxthreads < nthreads) {
		return NULL;
	}

	// This allocation can be too large for `Z_NOFAIL`.
	thread_map = kalloc_data_tag(*mapsize, Z_WAITOK | Z_ZERO,
	    VM_KERN_MEMORY_DIAG);
	if (thread_map != NULL) {
		*mapcount = _resolve_threads(thread_map, nthreads);
	}
	return thread_map;
}

static void
kdbg_clear(void)
{
	kernel_debug_disable();
	kdbg_disable_typefilter();

	// Wait for any event writers to see the disable status.
	IOSleep(100);

	// Reset kdebug status for each process.
	if (kd_control_trace.kdc_flags & (KDBG_PIDCHECK | KDBG_PIDEXCLUDE)) {
		proc_list_lock();
		proc_t p;
		ALLPROC_FOREACH(p) {
			p->p_kdebug = 0;
		}
		proc_list_unlock();
	}

	kd_control_trace.kdc_flags &= (unsigned int)~KDBG_CKTYPES;
	kd_control_trace.kdc_flags &= ~(KDBG_RANGECHECK | KDBG_VALCHECK);
	kd_control_trace.kdc_flags &= ~(KDBG_PIDCHECK | KDBG_PIDEXCLUDE);
	kd_control_trace.kdc_flags &= ~KDBG_CONTINUOUS_TIME;
	kd_control_trace.kdc_flags &= ~KDBG_DISABLE_COPROCS;
	kd_control_trace.kdc_flags &= ~KDBG_MATCH_DISABLE;
	kd_control_trace.kdc_live_flags &= ~(KDBG_NOWRAP | KDBG_WRAPPED);

	kd_control_trace.kdc_oldest_time = 0;

	delete_buffers_trace();
	kd_buffer_trace.kdb_event_count = 0;

	_clear_thread_map();

	RAW_file_offset = 0;
	RAW_file_written = 0;
}

void
kdebug_reset(void)
{
	ktrace_assert_lock_held();

	kdbg_clear();
	typefilter_reject_all(kdbg_typefilter);
	typefilter_allow_class(kdbg_typefilter, DBG_TRACE);
}

void
kdebug_free_early_buf(void)
{
#if defined(__x86_64__)
	ml_static_mfree((vm_offset_t)&kd_early_buffer, sizeof(kd_early_buffer));
#endif /* defined(__x86_64__) */
	// ARM handles this as part of the BOOTDATA segment.
}

int
kdbg_setpid(kd_regtype *kdr)
{
	pid_t pid;
	int flag, ret = 0;
	struct proc *p;

	pid = (pid_t)kdr->value1;
	flag = (int)kdr->value2;

	if (pid >= 0) {
		if ((p = proc_find(pid)) == NULL) {
			ret = ESRCH;
		} else {
			if (flag == 1) {
				/*
				 * turn on pid check for this and all pids
				 */
				kd_control_trace.kdc_flags |= KDBG_PIDCHECK;
				kd_control_trace.kdc_flags &= ~KDBG_PIDEXCLUDE;

				p->p_kdebug = 1;
			} else {
				/*
				 * turn off pid check for this pid value
				 * Don't turn off all pid checking though
				 *
				 * kd_control_trace.kdc_flags &= ~KDBG_PIDCHECK;
				 */
				p->p_kdebug = 0;
			}
			proc_rele(p);
		}
	} else {
		ret = EINVAL;
	}

	return ret;
}

/* This is for pid exclusion in the trace buffer */
int
kdbg_setpidex(kd_regtype *kdr)
{
	pid_t pid;
	int flag, ret = 0;
	struct proc *p;

	pid = (pid_t)kdr->value1;
	flag = (int)kdr->value2;

	if (pid >= 0) {
		if ((p = proc_find(pid)) == NULL) {
			ret = ESRCH;
		} else {
			if (flag == 1) {
				/*
				 * turn on pid exclusion
				 */
				kd_control_trace.kdc_flags |= KDBG_PIDEXCLUDE;
				kd_control_trace.kdc_flags &= ~KDBG_PIDCHECK;

				p->p_kdebug = 1;
			} else {
				/*
				 * turn off pid exclusion for this pid value
				 * Don't turn off all pid exclusion though
				 *
				 * kd_control_trace.kdc_flags &= ~KDBG_PIDEXCLUDE;
				 */
				p->p_kdebug = 0;
			}
			proc_rele(p);
		}
	} else {
		ret = EINVAL;
	}

	return ret;
}

/*
 * The following functions all operate on the typefilter singleton.
 */

static int
kdbg_copyin_typefilter(user_addr_t addr, size_t size)
{
	int ret = ENOMEM;
	typefilter_t tf;

	ktrace_assert_lock_held();

	if (size != KDBG_TYPEFILTER_BITMAP_SIZE) {
		return EINVAL;
	}

	if ((tf = typefilter_create())) {
		if ((ret = copyin(addr, tf, KDBG_TYPEFILTER_BITMAP_SIZE)) == 0) {
			/* The kernel typefilter must always allow DBG_TRACE */
			typefilter_allow_class(tf, DBG_TRACE);

			typefilter_copy(kdbg_typefilter, tf);

			kdbg_enable_typefilter();
			_coproc_list_callback(KD_CALLBACK_TYPEFILTER_CHANGED, kdbg_typefilter);
		}

		if (tf) {
			typefilter_deallocate(tf);
		}
	}

	return ret;
}

/*
 * Enable the flags in the control page for the typefilter.  Assumes that
 * kdbg_typefilter has already been allocated, so events being written
 * don't see a bad typefilter.
 */
static void
kdbg_enable_typefilter(void)
{
	kd_control_trace.kdc_flags &= ~(KDBG_RANGECHECK | KDBG_VALCHECK);
	kd_control_trace.kdc_flags |= KDBG_TYPEFILTER_CHECK;
	if (kdebug_enable) {
		kd_control_trace.kdc_emit = _trace_emit_filter();
	}
	commpage_update_kdebug_state();
}

// Disable the flags in the control page for the typefilter.  The typefilter
// may be safely deallocated shortly after this function returns.
static void
kdbg_disable_typefilter(void)
{
	bool notify_coprocs = kd_control_trace.kdc_flags & KDBG_TYPEFILTER_CHECK;
	kd_control_trace.kdc_flags &= ~KDBG_TYPEFILTER_CHECK;

	commpage_update_kdebug_state();

	if (notify_coprocs) {
		// Notify coprocessors that the typefilter will now allow everything.
		// Otherwise, they won't know a typefilter is no longer in effect.
		typefilter_allow_all(kdbg_typefilter);
		_coproc_list_callback(KD_CALLBACK_TYPEFILTER_CHANGED, kdbg_typefilter);
	}
}

uint32_t
kdebug_commpage_state(void)
{
	uint32_t state = 0;
	if (kdebug_enable) {
		state |= KDEBUG_COMMPAGE_ENABLE_TRACE;
		if (kd_control_trace.kdc_flags & KDBG_TYPEFILTER_CHECK) {
			state |= KDEBUG_COMMPAGE_ENABLE_TYPEFILTER;
		}
		if (kd_control_trace.kdc_flags & KDBG_CONTINUOUS_TIME) {
			state |= KDEBUG_COMMPAGE_CONTINUOUS;
		}
	}
	return state;
}

static int
kdbg_setreg(kd_regtype * kdr)
{
	switch (kdr->type) {
	case KDBG_CLASSTYPE:
		kdlog_beg = KDBG_EVENTID(kdr->value1 & 0xff, 0, 0);
		kdlog_end = KDBG_EVENTID(kdr->value2 & 0xff, 0, 0);
		kd_control_trace.kdc_flags &= ~KDBG_VALCHECK;
		kd_control_trace.kdc_flags |= KDBG_RANGECHECK;
		break;
	case KDBG_SUBCLSTYPE:;
		unsigned int cls = kdr->value1 & 0xff;
		unsigned int subcls = kdr->value2 & 0xff;
		unsigned int subcls_end = subcls + 1;
		kdlog_beg = KDBG_EVENTID(cls, subcls, 0);
		kdlog_end = KDBG_EVENTID(cls, subcls_end, 0);
		kd_control_trace.kdc_flags &= ~KDBG_VALCHECK;
		kd_control_trace.kdc_flags |= KDBG_RANGECHECK;
		break;
	case KDBG_RANGETYPE:
		kdlog_beg = kdr->value1;
		kdlog_end = kdr->value2;
		kd_control_trace.kdc_flags &= ~KDBG_VALCHECK;
		kd_control_trace.kdc_flags |= KDBG_RANGECHECK;
		break;
	case KDBG_VALCHECK:
		kdlog_value1 = kdr->value1;
		kdlog_value2 = kdr->value2;
		kdlog_value3 = kdr->value3;
		kdlog_value4 = kdr->value4;
		kd_control_trace.kdc_flags &= ~KDBG_RANGECHECK;
		kd_control_trace.kdc_flags |= KDBG_VALCHECK;
		break;
	case KDBG_TYPENONE:
		kd_control_trace.kdc_flags &= ~(KDBG_RANGECHECK | KDBG_VALCHECK);
		kdlog_beg = 0;
		kdlog_end = 0;
		break;
	default:
		return EINVAL;
	}
	if (kdebug_enable) {
		kd_control_trace.kdc_emit = _trace_emit_filter();
	}
	return 0;
}

static int
_copyin_event_disable_mask(user_addr_t uaddr, size_t usize)
{
	if (usize < 2 * sizeof(kd_event_matcher)) {
		return ERANGE;
	}
	int ret = copyin(uaddr, &kd_control_trace.disable_event_match,
	    sizeof(kd_event_matcher));
	if (ret != 0) {
		return ret;
	}
	ret = copyin(uaddr + sizeof(kd_event_matcher),
	    &kd_control_trace.disable_event_mask, sizeof(kd_event_matcher));
	if (ret != 0) {
		memset(&kd_control_trace.disable_event_match, 0,
		    sizeof(kd_event_matcher));
		return ret;
	}
	return 0;
}

static int
_copyout_event_disable_mask(user_addr_t uaddr, size_t usize)
{
	if (usize < 2 * sizeof(kd_event_matcher)) {
		return ERANGE;
	}
	int ret = copyout(&kd_control_trace.disable_event_match, uaddr,
	    sizeof(kd_event_matcher));
	if (ret != 0) {
		return ret;
	}
	ret = copyout(&kd_control_trace.disable_event_mask,
	    uaddr + sizeof(kd_event_matcher), sizeof(kd_event_matcher));
	if (ret != 0) {
		return ret;
	}
	return 0;
}

static int
kdbg_write_to_vnode(caddr_t buffer, size_t size, vnode_t vp, vfs_context_t ctx, off_t file_offset)
{
	assert(size < INT_MAX);
	return vn_rdwr(UIO_WRITE, vp, buffer, (int)size, file_offset, UIO_SYSSPACE,
	           IO_NODELOCKED | IO_UNIT, vfs_context_ucred(ctx), (int *) 0,
	           vfs_context_proc(ctx));
}

static errno_t
_copyout_cpu_map(int map_version, user_addr_t udst, size_t *usize)
{
	if ((kd_control_trace.kdc_flags & KDBG_BUFINIT) == 0) {
		return EINVAL;
	}

	void *cpu_map = NULL;
	size_t size = 0;
	int error = _copy_cpu_map(map_version, &cpu_map, &size);
	if (0 == error) {
		if (udst) {
			size_t copy_size = MIN(*usize, size);
			error = copyout(cpu_map, udst, copy_size);
		}
		*usize = size;
		kmem_free(kernel_map, (vm_offset_t)cpu_map, size);
	}
	if (EINVAL == error && 0 == udst) {
		*usize = size;
		// User space only needs the size if it passes NULL;
		error = 0;
	}
	return error;
}

int
kdbg_readcurthrmap(user_addr_t buffer, size_t *bufsize)
{
	kd_threadmap *mapptr;
	vm_size_t mapsize;
	vm_size_t mapcount;
	int ret = 0;
	size_t count = *bufsize / sizeof(kd_threadmap);

	*bufsize = 0;

	if ((mapptr = _thread_map_create_live(count, &mapsize, &mapcount))) {
		if (copyout(mapptr, buffer, mapcount * sizeof(kd_threadmap))) {
			ret = EFAULT;
		} else {
			*bufsize = (mapcount * sizeof(kd_threadmap));
		}

		kfree_data(mapptr, mapsize);
	} else {
		ret = EINVAL;
	}

	return ret;
}

static int
_write_legacy_header(bool write_thread_map, vnode_t vp, vfs_context_t ctx)
{
	int ret = 0;
	RAW_header header;
	clock_sec_t secs;
	clock_usec_t usecs;
	void *pad_buf;
	uint32_t pad_size;
	uint32_t extra_thread_count = 0;
	uint32_t cpumap_size;
	size_t map_size = 0;
	uint32_t map_count = 0;

	if (write_thread_map) {
		assert(kd_control_trace.kdc_flags & KDBG_MAPINIT);
		if (kd_mapcount > UINT32_MAX) {
			return ERANGE;
		}
		map_count = (uint32_t)kd_mapcount;
		if (os_mul_overflow(map_count, sizeof(kd_threadmap), &map_size)) {
			return ERANGE;
		}
		if (map_size >= INT_MAX) {
			return ERANGE;
		}
	}

	/*
	 * Without the buffers initialized, we cannot construct a CPU map or a
	 * thread map, and cannot write a header.
	 */
	if (!(kd_control_trace.kdc_flags & KDBG_BUFINIT)) {
		return EINVAL;
	}

	/*
	 * To write a RAW_VERSION1+ file, we must embed a cpumap in the
	 * "padding" used to page align the events following the threadmap. If
	 * the threadmap happens to not require enough padding, we artificially
	 * increase its footprint until it needs enough padding.
	 */

	assert(vp);
	assert(ctx);

	pad_size = 16384 - ((sizeof(RAW_header) + map_size) & PAGE_MASK);
	cpumap_size = sizeof(kd_cpumap_header) + kd_control_trace.kdebug_cpus * sizeof(kd_cpumap);

	if (cpumap_size > pad_size) {
		/* If the cpu map doesn't fit in the current available pad_size,
		 * we increase the pad_size by 16K. We do this so that the event
		 * data is always  available on a page aligned boundary for both
		 * 4k and 16k systems. We enforce this alignment for the event
		 * data so that we can take advantage of optimized file/disk writes.
		 */
		pad_size += 16384;
	}

	/* The way we are silently embedding a cpumap in the "padding" is by artificially
	 * increasing the number of thread entries. However, we'll also need to ensure that
	 * the cpumap is embedded in the last 4K page before when the event data is expected.
	 * This way the tools can read the data starting the next page boundary on both
	 * 4K and 16K systems preserving compatibility with older versions of the tools
	 */
	if (pad_size > 4096) {
		pad_size -= 4096;
		extra_thread_count = (pad_size / sizeof(kd_threadmap)) + 1;
	}

	memset(&header, 0, sizeof(header));
	header.version_no = RAW_VERSION1;
	header.thread_count = map_count + extra_thread_count;

	clock_get_calendar_microtime(&secs, &usecs);
	header.TOD_secs = secs;
	header.TOD_usecs = usecs;

	ret = vn_rdwr(UIO_WRITE, vp, (caddr_t)&header, (int)sizeof(RAW_header), RAW_file_offset,
	    UIO_SYSSPACE, IO_NODELOCKED | IO_UNIT, vfs_context_ucred(ctx), (int *) 0, vfs_context_proc(ctx));
	if (ret) {
		goto write_error;
	}
	RAW_file_offset += sizeof(RAW_header);
	RAW_file_written += sizeof(RAW_header);

	if (write_thread_map) {
		assert(map_size < INT_MAX);
		ret = vn_rdwr(UIO_WRITE, vp, (caddr_t)kd_mapptr, (int)map_size, RAW_file_offset,
		    UIO_SYSSPACE, IO_NODELOCKED | IO_UNIT, vfs_context_ucred(ctx), (int *) 0, vfs_context_proc(ctx));
		if (ret) {
			goto write_error;
		}

		RAW_file_offset += map_size;
		RAW_file_written += map_size;
	}

	if (extra_thread_count) {
		pad_size = extra_thread_count * sizeof(kd_threadmap);
		pad_buf = (char *)kalloc_data(pad_size, Z_WAITOK | Z_ZERO);
		if (!pad_buf) {
			ret = ENOMEM;
			goto write_error;
		}

		assert(pad_size < INT_MAX);
		ret = vn_rdwr(UIO_WRITE, vp, (caddr_t)pad_buf, (int)pad_size, RAW_file_offset,
		    UIO_SYSSPACE, IO_NODELOCKED | IO_UNIT, vfs_context_ucred(ctx), (int *) 0, vfs_context_proc(ctx));
		kfree_data(pad_buf, pad_size);
		if (ret) {
			goto write_error;
		}

		RAW_file_offset += pad_size;
		RAW_file_written += pad_size;
	}

	pad_size = PAGE_SIZE - (RAW_file_offset & PAGE_MASK);
	if (pad_size) {
		pad_buf = (char *)kalloc_data(pad_size, Z_WAITOK | Z_ZERO);
		if (!pad_buf) {
			ret = ENOMEM;
			goto write_error;
		}

		/*
		 * Embed the CPU map in the padding bytes -- old code will skip it,
		 * while newer code knows it's there.
		 */
		size_t temp = pad_size;
		errno_t error = _copy_cpu_map(RAW_VERSION1, &pad_buf, &temp);
		if (0 != error) {
			memset(pad_buf, 0, pad_size);
		}

		assert(pad_size < INT_MAX);
		ret = vn_rdwr(UIO_WRITE, vp, (caddr_t)pad_buf, (int)pad_size, RAW_file_offset,
		    UIO_SYSSPACE, IO_NODELOCKED | IO_UNIT, vfs_context_ucred(ctx), (int *) 0, vfs_context_proc(ctx));
		kfree_data(pad_buf, pad_size);
		if (ret) {
			goto write_error;
		}

		RAW_file_offset += pad_size;
		RAW_file_written += pad_size;
	}

write_error:
	return ret;
}

static void
_clear_thread_map(void)
{
	ktrace_assert_lock_held();

	if (kd_control_trace.kdc_flags & KDBG_MAPINIT) {
		assert(kd_mapptr != NULL);
		kfree_data(kd_mapptr, kd_mapsize);
		kd_mapptr = NULL;
		kd_mapsize = 0;
		kd_mapcount = 0;
		kd_control_trace.kdc_flags &= ~KDBG_MAPINIT;
	}
}

/*
 * Write out a version 1 header and the thread map, if it is initialized, to a
 * vnode.  Used by KDWRITEMAP and kdbg_dump_trace_to_file.
 *
 * Returns write errors from vn_rdwr if a write fails.  Returns ENODATA if the
 * thread map has not been initialized, but the header will still be written.
 * Returns ENOMEM if padding could not be allocated.  Returns 0 otherwise.
 */
static int
kdbg_write_thread_map(vnode_t vp, vfs_context_t ctx)
{
	int ret = 0;
	bool map_initialized;

	ktrace_assert_lock_held();
	assert(ctx != NULL);

	map_initialized = (kd_control_trace.kdc_flags & KDBG_MAPINIT);

	ret = _write_legacy_header(map_initialized, vp, ctx);
	if (ret == 0) {
		if (map_initialized) {
			_clear_thread_map();
		} else {
			ret = ENODATA;
		}
	}

	return ret;
}

/*
 * Copy out the thread map to a user space buffer.  Used by KDTHRMAP.
 *
 * Returns copyout errors if the copyout fails.  Returns ENODATA if the thread
 * map has not been initialized.  Returns EINVAL if the buffer provided is not
 * large enough for the entire thread map.  Returns 0 otherwise.
 */
static int
kdbg_copyout_thread_map(user_addr_t buffer, size_t *buffer_size)
{
	bool map_initialized;
	size_t map_size;
	int ret = 0;

	ktrace_assert_lock_held();
	assert(buffer_size != NULL);

	map_initialized = (kd_control_trace.kdc_flags & KDBG_MAPINIT);
	if (!map_initialized) {
		return ENODATA;
	}

	map_size = kd_mapcount * sizeof(kd_threadmap);
	if (*buffer_size < map_size) {
		return EINVAL;
	}

	ret = copyout(kd_mapptr, buffer, map_size);
	if (ret == 0) {
		_clear_thread_map();
	}

	return ret;
}

static void
kdbg_set_nkdbufs_trace(unsigned int req_nkdbufs_trace)
{
	/*
	 * Only allow allocations of up to half the kernel's data range or "sane
	 * size", whichever is smaller.
	 */
	const uint64_t max_nkdbufs_trace_64 =
	    MIN(kmem_range_id_size(KMEM_RANGE_ID_DATA), sane_size) / 2 /
	    sizeof(kd_buf);
	/*
	 * Can't allocate more than 2^38 (2^32 * 64) bytes of events without
	 * switching to a 64-bit event count; should be fine.
	 */
	const unsigned int max_nkdbufs_trace =
	    (unsigned int)MIN(max_nkdbufs_trace_64, UINT_MAX);

	kd_buffer_trace.kdb_event_count = MIN(req_nkdbufs_trace, max_nkdbufs_trace);
}

/*
 * Block until there are `kd_buffer_trace.kdb_storage_threshold` storage units filled with
 * events or `timeout_ms` milliseconds have passed.  If `locked_wait` is true,
 * `ktrace_lock` is held while waiting.  This is necessary while waiting to
 * write events out of the buffers.
 *
 * Returns true if the threshold was reached and false otherwise.
 *
 * Called with `ktrace_lock` locked and interrupts enabled.
 */
static bool
kdbg_wait(uint64_t timeout_ms)
{
	int wait_result = THREAD_AWAKENED;
	uint64_t deadline_mach = 0;

	ktrace_assert_lock_held();

	if (timeout_ms != 0) {
		uint64_t ns = timeout_ms * NSEC_PER_MSEC;
		nanoseconds_to_absolutetime(ns, &deadline_mach);
		clock_absolutetime_interval_to_deadline(deadline_mach, &deadline_mach);
	}

	bool s = ml_set_interrupts_enabled(false);
	if (!s) {
		panic("kdbg_wait() called with interrupts disabled");
	}
	lck_spin_lock_grp(&kd_wait_lock, &kdebug_lck_grp);

	/* drop the mutex to allow others to access trace */
	ktrace_unlock();

	while (wait_result == THREAD_AWAKENED &&
	    kd_control_trace.kdc_storage_used < kd_buffer_trace.kdb_storage_threshold) {
		kd_waiter = true;

		if (deadline_mach) {
			wait_result = lck_spin_sleep_deadline(&kd_wait_lock, 0, &kd_waiter,
			    THREAD_ABORTSAFE, deadline_mach);
		} else {
			wait_result = lck_spin_sleep(&kd_wait_lock, 0, &kd_waiter,
			    THREAD_ABORTSAFE);
		}
	}

	bool threshold_exceeded = (kd_control_trace.kdc_storage_used >= kd_buffer_trace.kdb_storage_threshold);

	lck_spin_unlock(&kd_wait_lock);
	ml_set_interrupts_enabled(s);

	ktrace_lock();

	return threshold_exceeded;
}

/*
 * Wakeup a thread waiting using `kdbg_wait` if there are at least
 * `kd_buffer_trace.kdb_storage_threshold` storage units in use.
 */
static void
kdbg_wakeup(void)
{
	bool need_kds_wakeup = false;

	/*
	 * Try to take the lock here to synchronize with the waiter entering
	 * the blocked state.  Use the try mode to prevent deadlocks caused by
	 * re-entering this routine due to various trace points triggered in the
	 * lck_spin_sleep_xxxx routines used to actually enter one of our 2 wait
	 * conditions.  No problem if we fail, there will be lots of additional
	 * events coming in that will eventually succeed in grabbing this lock.
	 */
	bool s = ml_set_interrupts_enabled(false);

	if (lck_spin_try_lock(&kd_wait_lock)) {
		if (kd_waiter &&
		    (kd_control_trace.kdc_storage_used >= kd_buffer_trace.kdb_storage_threshold)) {
			kd_waiter = 0;
			need_kds_wakeup = true;
		}
		lck_spin_unlock(&kd_wait_lock);
	}

	ml_set_interrupts_enabled(s);

	if (need_kds_wakeup == true) {
		wakeup(&kd_waiter);
	}
}

static int
_read_merged_trace_events(user_addr_t buffer, size_t *number, vnode_t vp,
    vfs_context_t ctx, bool chunk)
{
	ktrace_assert_lock_held();
	size_t count = *number / sizeof(kd_buf);
	if (count == 0 || !(kd_control_trace.kdc_flags & KDBG_BUFINIT) ||
	    kd_buffer_trace.kdcopybuf == 0) {
		*number = 0;
		return EINVAL;
	}

	// Before merging, make sure coprocessors have provided up-to-date events.
	_coproc_list_callback(KD_CALLBACK_SYNC_FLUSH, NULL);
	return kernel_debug_read(&kd_control_trace, &kd_buffer_trace, buffer,
	           number, vp, ctx, chunk);
}

struct event_chunk_header {
	uint32_t tag;
	uint32_t sub_tag;
	uint64_t length;
	uint64_t future_events_timestamp;
};

static int
_write_event_chunk_header(user_addr_t udst, vnode_t vp, vfs_context_t ctx,
    uint64_t length)
{
	struct event_chunk_header header = {
		.tag = V3_RAW_EVENTS,
		.sub_tag = 1,
		.length = length,
	};

	if (vp) {
		assert(udst == USER_ADDR_NULL);
		assert(ctx != NULL);
		int error = kdbg_write_to_vnode((caddr_t)&header, sizeof(header), vp,
		    ctx, RAW_file_offset);
		if (0 == error) {
			RAW_file_offset += sizeof(header);
		}
		return error;
	} else {
		assert(udst != USER_ADDR_NULL);
		return copyout(&header, udst, sizeof(header));
	}
}

int
kernel_debug_trace_write_to_file(user_addr_t *buffer, size_t *number,
    size_t *count, size_t tempbuf_number, vnode_t vp, vfs_context_t ctx,
    bool chunk)
{
	int error = 0;

	if (chunk) {
		error = _write_event_chunk_header(*buffer, vp, ctx,
		    tempbuf_number * sizeof(kd_buf));
		if (error) {
			return error;
		}
		if (buffer) {
			*buffer += sizeof(struct event_chunk_header);
		}

		assert(*count >= sizeof(struct event_chunk_header));
		*count -= sizeof(struct event_chunk_header);
		*number += sizeof(struct event_chunk_header);
	}
	if (vp) {
		size_t write_size = tempbuf_number * sizeof(kd_buf);
		error = kdbg_write_to_vnode((caddr_t)kd_buffer_trace.kdcopybuf,
		    write_size, vp, ctx, RAW_file_offset);
		if (!error) {
			RAW_file_offset += write_size;
		}

		if (RAW_file_written >= RAW_FLUSH_SIZE) {
			error = VNOP_FSYNC(vp, MNT_NOWAIT, ctx);

			RAW_file_written = 0;
		}
	} else {
		error = copyout(kd_buffer_trace.kdcopybuf, *buffer, tempbuf_number * sizeof(kd_buf));
		*buffer += (tempbuf_number * sizeof(kd_buf));
	}

	return error;
}

#pragma mark - User space interface

static int
_kd_sysctl_internal(int op, int value, user_addr_t where, size_t *sizep)
{
	size_t size = *sizep;
	kd_regtype kd_Reg;
	proc_t p;

	bool read_only = (op == KERN_KDGETBUF || op == KERN_KDREADCURTHRMAP);
	int perm_error = read_only ? ktrace_read_check() :
	    ktrace_configure(KTRACE_KDEBUG);
	if (perm_error != 0) {
		return perm_error;
	}

	switch (op) {
	case KERN_KDGETBUF:;
		pid_t owning_pid = ktrace_get_owning_pid();
		kbufinfo_t info = {
			.nkdbufs = kd_buffer_trace.kdb_event_count,
			.nkdthreads = (int)MIN(kd_mapcount, INT_MAX),
			.nolog = kd_control_trace.kdc_emit == KDEMIT_DISABLE,
			.flags = kd_control_trace.kdc_flags | kd_control_trace.kdc_live_flags,
			.bufid = owning_pid ?: -1,
		};
#if defined(__LP64__)
		info.flags |= KDBG_LP64;
#endif // defined(__LP64__)

		size = MIN(size, sizeof(info));
		return copyout(&info, where, size);
	case KERN_KDREADCURTHRMAP:
		return kdbg_readcurthrmap(where, sizep);
	case KERN_KDEFLAGS:
		value &= KDBG_USERFLAGS;
		kd_control_trace.kdc_flags |= value;
		return 0;
	case KERN_KDDFLAGS:
		value &= KDBG_USERFLAGS;
		kd_control_trace.kdc_flags &= ~value;
		return 0;
	case KERN_KDENABLE:
		if (value) {
			if (!(kd_control_trace.kdc_flags & KDBG_BUFINIT) ||
			    !(value == KDEBUG_ENABLE_TRACE || value == KDEBUG_ENABLE_PPT)) {
				return EINVAL;
			}
			_threadmap_init();

			kdbg_set_tracing_enabled(true, value);
		} else {
			if (!kdebug_enable) {
				return 0;
			}

			kernel_debug_disable();
		}
		return 0;
	case KERN_KDSETBUF:
		kdbg_set_nkdbufs_trace(value);
		return 0;
	case KERN_KDSETUP:
		return kdbg_reinit(EXTRA_COPROC_COUNT);
	case KERN_KDREMOVE:
		ktrace_reset(KTRACE_KDEBUG);
		return 0;
	case KERN_KDSETREG:
		if (size < sizeof(kd_regtype)) {
			return EINVAL;
		}
		if (copyin(where, &kd_Reg, sizeof(kd_regtype))) {
			return EINVAL;
		}
		return kdbg_setreg(&kd_Reg);
	case KERN_KDGETREG:
		return EINVAL;
	case KERN_KDREADTR:
		return _read_merged_trace_events(where, sizep, NULL, NULL, false);
	case KERN_KDWRITETR:
	case KERN_KDWRITETR_V3:
	case KERN_KDWRITEMAP: {
		struct  vfs_context context;
		struct  fileproc *fp;
		size_t  number;
		vnode_t vp;
		int     fd;
		int ret = 0;

		if (op == KERN_KDWRITETR || op == KERN_KDWRITETR_V3) {
			(void)kdbg_wait(size);
			// Re-check whether this process can configure ktrace, since waiting
			// will drop the ktrace lock.
			int no_longer_owner_error = ktrace_configure(KTRACE_KDEBUG);
			if (no_longer_owner_error != 0) {
				return no_longer_owner_error;
			}
		}

		p = current_proc();
		fd = value;

		if (fp_get_ftype(p, fd, DTYPE_VNODE, EBADF, &fp)) {
			return EBADF;
		}

		vp = fp_get_data(fp);
		context.vc_thread = current_thread();
		context.vc_ucred = fp->fp_glob->fg_cred;

		if ((ret = vnode_getwithref(vp)) == 0) {
			RAW_file_offset = fp->fp_glob->fg_offset;
			if (op == KERN_KDWRITETR || op == KERN_KDWRITETR_V3) {
				number = kd_buffer_trace.kdb_event_count * sizeof(kd_buf);

				KDBG_RELEASE(TRACE_WRITING_EVENTS | DBG_FUNC_START);
				ret = _read_merged_trace_events(0, &number, vp, &context,
				    op == KERN_KDWRITETR_V3);
				KDBG_RELEASE(TRACE_WRITING_EVENTS | DBG_FUNC_END, number);

				*sizep = number;
			} else {
				number = kd_mapcount * sizeof(kd_threadmap);
				ret = kdbg_write_thread_map(vp, &context);
			}
			fp->fp_glob->fg_offset = RAW_file_offset;
			vnode_put(vp);
		}
		fp_drop(p, fd, fp, 0);

		return ret;
	}
	case KERN_KDBUFWAIT:
		*sizep = kdbg_wait(size);
		return 0;
	case KERN_KDPIDTR:
		if (size < sizeof(kd_regtype)) {
			return EINVAL;
		}
		if (copyin(where, &kd_Reg, sizeof(kd_regtype))) {
			return EINVAL;
		}
		return kdbg_setpid(&kd_Reg);
	case KERN_KDPIDEX:
		if (size < sizeof(kd_regtype)) {
			return EINVAL;
		}
		if (copyin(where, &kd_Reg, sizeof(kd_regtype))) {
			return EINVAL;
		}
		return kdbg_setpidex(&kd_Reg);
	case KERN_KDCPUMAP:
		return _copyout_cpu_map(RAW_VERSION1, where, sizep);
	case KERN_KDCPUMAP_EXT:
		return _copyout_cpu_map(1, where, sizep);
	case KERN_KDTHRMAP:
		return kdbg_copyout_thread_map(where, sizep);
	case KERN_KDSET_TYPEFILTER:
		return kdbg_copyin_typefilter(where, size);
	case KERN_KDSET_EDM:
		return _copyin_event_disable_mask(where, size);
	case KERN_KDGET_EDM:
		return _copyout_event_disable_mask(where, size);
#if DEVELOPMENT || DEBUG
	case KERN_KDTEST:
		return kdbg_test(size);
#endif // DEVELOPMENT || DEBUG

	default:
		return ENOTSUP;
	}
}

static int
kdebug_sysctl SYSCTL_HANDLER_ARGS
{
	int *names = arg1;
	int name_count = arg2;
	user_addr_t udst = req->oldptr;
	size_t *usize = &req->oldlen;
	int value = 0;

	if (name_count == 0) {
		return ENOTSUP;
	}

	int op = names[0];

	// Some operations have an argument stuffed into the next OID argument.
	switch (op) {
	case KERN_KDWRITETR:
	case KERN_KDWRITETR_V3:
	case KERN_KDWRITEMAP:
	case KERN_KDEFLAGS:
	case KERN_KDDFLAGS:
	case KERN_KDENABLE:
	case KERN_KDSETBUF:
		if (name_count < 2) {
			return EINVAL;
		}
		value = names[1];
		break;
	default:
		break;
	}

	ktrace_lock();
	int ret = _kd_sysctl_internal(op, value, udst, usize);
	ktrace_unlock();
	if (0 == ret) {
		req->oldidx += req->oldlen;
	}
	return ret;
}
SYSCTL_PROC(_kern, KERN_KDEBUG, kdebug,
    CTLTYPE_NODE | CTLFLAG_RD | CTLFLAG_LOCKED, 0, 0, kdebug_sysctl, NULL, "");

#pragma mark - Tests

#if DEVELOPMENT || DEBUG

static int test_coproc = 0;
static int sync_flush_coproc = 0;

#define KDEBUG_TEST_CODE(code) BSDDBG_CODE(DBG_BSD_KDEBUG_TEST, (code))

/*
 * A test IOP for the SYNC_FLUSH callback.
 */

static void
sync_flush_callback(void * __unused context, kd_callback_type reason,
    void * __unused arg)
{
	assert(sync_flush_coproc > 0);

	if (reason == KD_CALLBACK_SYNC_FLUSH) {
		kernel_debug_enter(sync_flush_coproc, KDEBUG_TEST_CODE(0xff),
		    kdebug_timestamp(), 0, 0, 0, 0, 0);
	}
}

static struct kd_callback sync_flush_kdcb = {
	.func = sync_flush_callback,
	.iop_name = "test_sf",
};

#define TEST_COPROC_CTX 0xabadcafe

static void
test_coproc_cb(__assert_only void *context, kd_callback_type __unused reason,
    void * __unused arg)
{
	assert((uintptr_t)context == TEST_COPROC_CTX);
}

static int
kdbg_test(size_t flavor)
{
	int code = 0;
	int dummy_iop = 0;

	switch (flavor) {
	case KDTEST_KERNEL_MACROS:
		/* try each macro */
		KDBG(KDEBUG_TEST_CODE(code)); code++;
		KDBG(KDEBUG_TEST_CODE(code), 1); code++;
		KDBG(KDEBUG_TEST_CODE(code), 1, 2); code++;
		KDBG(KDEBUG_TEST_CODE(code), 1, 2, 3); code++;
		KDBG(KDEBUG_TEST_CODE(code), 1, 2, 3, 4); code++;

		KDBG_RELEASE(KDEBUG_TEST_CODE(code)); code++;
		KDBG_RELEASE(KDEBUG_TEST_CODE(code), 1); code++;
		KDBG_RELEASE(KDEBUG_TEST_CODE(code), 1, 2); code++;
		KDBG_RELEASE(KDEBUG_TEST_CODE(code), 1, 2, 3); code++;
		KDBG_RELEASE(KDEBUG_TEST_CODE(code), 1, 2, 3, 4); code++;

		KDBG_FILTERED(KDEBUG_TEST_CODE(code)); code++;
		KDBG_FILTERED(KDEBUG_TEST_CODE(code), 1); code++;
		KDBG_FILTERED(KDEBUG_TEST_CODE(code), 1, 2); code++;
		KDBG_FILTERED(KDEBUG_TEST_CODE(code), 1, 2, 3); code++;
		KDBG_FILTERED(KDEBUG_TEST_CODE(code), 1, 2, 3, 4); code++;

		KDBG_RELEASE_NOPROCFILT(KDEBUG_TEST_CODE(code)); code++;
		KDBG_RELEASE_NOPROCFILT(KDEBUG_TEST_CODE(code), 1); code++;
		KDBG_RELEASE_NOPROCFILT(KDEBUG_TEST_CODE(code), 1, 2); code++;
		KDBG_RELEASE_NOPROCFILT(KDEBUG_TEST_CODE(code), 1, 2, 3); code++;
		KDBG_RELEASE_NOPROCFILT(KDEBUG_TEST_CODE(code), 1, 2, 3, 4); code++;

		KDBG_DEBUG(KDEBUG_TEST_CODE(code)); code++;
		KDBG_DEBUG(KDEBUG_TEST_CODE(code), 1); code++;
		KDBG_DEBUG(KDEBUG_TEST_CODE(code), 1, 2); code++;
		KDBG_DEBUG(KDEBUG_TEST_CODE(code), 1, 2, 3); code++;
		KDBG_DEBUG(KDEBUG_TEST_CODE(code), 1, 2, 3, 4); code++;
		break;

	case KDTEST_OLD_TIMESTAMP:
		if (kd_control_trace.kdc_coprocs) {
			/* avoid the assertion in kernel_debug_enter for a valid IOP */
			dummy_iop = kd_control_trace.kdc_coprocs[0].cpu_id;
		}

		/* ensure old timestamps are not emitted from kernel_debug_enter */
		kernel_debug_enter(dummy_iop, KDEBUG_TEST_CODE(code),
		    100 /* very old timestamp */, 0, 0, 0, 0, 0);
		code++;
		kernel_debug_enter(dummy_iop, KDEBUG_TEST_CODE(code),
		    kdebug_timestamp(), 0, 0, 0, 0, 0);
		code++;
		break;

	case KDTEST_FUTURE_TIMESTAMP:
		if (kd_control_trace.kdc_coprocs) {
			dummy_iop = kd_control_trace.kdc_coprocs[0].cpu_id;
		}
		kernel_debug_enter(dummy_iop, KDEBUG_TEST_CODE(code),
		    kdebug_timestamp() * 2 /* !!! */, 0, 0, 0, 0, 0);
		break;

	case KDTEST_SETUP_IOP:
		if (!sync_flush_coproc) {
			ktrace_unlock();
			int new_sync_flush_coproc = kernel_debug_register_callback(
				sync_flush_kdcb);
			assert(new_sync_flush_coproc > 0);
			ktrace_lock();
			if (!sync_flush_coproc) {
				sync_flush_coproc = new_sync_flush_coproc;
			}
		}
		break;

	case KDTEST_SETUP_COPROCESSOR:
		if (!test_coproc) {
			ktrace_unlock();
			int new_test_coproc = kdebug_register_coproc("test_coproc",
			    KDCP_CONTINUOUS_TIME, test_coproc_cb, (void *)TEST_COPROC_CTX);
			assert(new_test_coproc > 0);
			ktrace_lock();
			if (!test_coproc) {
				test_coproc = new_test_coproc;
			}
		}
		break;

	case KDTEST_ABSOLUTE_TIMESTAMP:;
		uint64_t atime = mach_absolute_time();
		kernel_debug_enter(sync_flush_coproc, KDEBUG_TEST_CODE(0),
		    atime, (uintptr_t)atime, (uintptr_t)(atime >> 32), 0, 0, 0);
		break;

	case KDTEST_CONTINUOUS_TIMESTAMP:;
		uint64_t ctime = mach_continuous_time();
		kernel_debug_enter(test_coproc, KDEBUG_TEST_CODE(1),
		    ctime, (uintptr_t)ctime, (uintptr_t)(ctime >> 32), 0, 0, 0);
		break;

	case KDTEST_PAST_EVENT:;
		uint64_t old_time = 1;
		kernel_debug_enter(test_coproc, KDEBUG_TEST_CODE(1), old_time, 0, 0, 0,
		    0, 0);
		kernel_debug_enter(test_coproc, KDEBUG_TEST_CODE(1), kdebug_timestamp(),
		    0, 0, 0, 0, 0);
		break;

	default:
		return ENOTSUP;
	}

	return 0;
}

#undef KDEBUG_TEST_CODE

#endif /* DEVELOPMENT || DEBUG */

static void
_deferred_coproc_notify(mpsc_queue_chain_t e, mpsc_daemon_queue_t queue __unused)
{
	struct kd_coproc *coproc = mpsc_queue_element(e, struct kd_coproc, chain);
	if (kd_control_trace.kdc_emit == KDEMIT_TYPEFILTER) {
		coproc->callback.func(coproc->callback.context,
		    KD_CALLBACK_TYPEFILTER_CHANGED, kdbg_typefilter);
	}
	if (kdebug_enable) {
		coproc->callback.func(coproc->callback.context,
		    KD_CALLBACK_KDEBUG_ENABLED, kdbg_typefilter);
	}
}

void
kdebug_init(unsigned int n_events, char *filter_desc, enum kdebug_opts opts)
{
	assert(filter_desc != NULL);

	kdbg_typefilter = typefilter_create();
	assert(kdbg_typefilter != NULL);
	kdbg_typefilter_memory_entry = typefilter_create_memory_entry(kdbg_typefilter);
	assert(kdbg_typefilter_memory_entry != MACH_PORT_NULL);

	(void)mpsc_daemon_queue_init_with_thread_call(&_coproc_notify_queue,
	    _deferred_coproc_notify, THREAD_CALL_PRIORITY_KERNEL,
	    MPSC_DAEMON_INIT_NONE);

	kdebug_trace_start(n_events, filter_desc, opts);
}

static void
kdbg_set_typefilter_string(const char *filter_desc)
{
	char *end = NULL;

	ktrace_assert_lock_held();

	assert(filter_desc != NULL);

	typefilter_reject_all(kdbg_typefilter);
	typefilter_allow_class(kdbg_typefilter, DBG_TRACE);

	/* if the filter description starts with a number, assume it's a csc */
	if (filter_desc[0] >= '0' && filter_desc[0] <= '9') {
		unsigned long csc = strtoul(filter_desc, NULL, 0);
		if (filter_desc != end && csc <= KDBG_CSC_MAX) {
			typefilter_allow_csc(kdbg_typefilter, (uint16_t)csc);
		}
		return;
	}

	while (filter_desc[0] != '\0') {
		unsigned long allow_value;

		char filter_type = filter_desc[0];
		if (filter_type != 'C' && filter_type != 'S') {
			printf("kdebug: unexpected filter type `%c'\n", filter_type);
			return;
		}
		filter_desc++;

		allow_value = strtoul(filter_desc, &end, 0);
		if (filter_desc == end) {
			printf("kdebug: cannot parse `%s' as integer\n", filter_desc);
			return;
		}

		switch (filter_type) {
		case 'C':
			if (allow_value > KDBG_CLASS_MAX) {
				printf("kdebug: class 0x%lx is invalid\n", allow_value);
				return;
			}
			printf("kdebug: C 0x%lx\n", allow_value);
			typefilter_allow_class(kdbg_typefilter, (uint8_t)allow_value);
			break;
		case 'S':
			if (allow_value > KDBG_CSC_MAX) {
				printf("kdebug: class-subclass 0x%lx is invalid\n", allow_value);
				return;
			}
			printf("kdebug: S 0x%lx\n", allow_value);
			typefilter_allow_csc(kdbg_typefilter, (uint16_t)allow_value);
			break;
		default:
			__builtin_unreachable();
		}

		/* advance to next filter entry */
		filter_desc = end;
		if (filter_desc[0] == ',') {
			filter_desc++;
		}
	}
}

uint64_t
kdebug_wake(void)
{
	if (!wake_nkdbufs) {
		return 0;
	}
	uint64_t start = mach_absolute_time();
	kdebug_trace_start(wake_nkdbufs, NULL, trace_wrap ? KDOPT_WRAPPING : 0);
	return mach_absolute_time() - start;
}

/*
 * This function is meant to be called from the bootstrap thread or kdebug_wake.
 */
void
kdebug_trace_start(unsigned int n_events, const char *filter_desc,
    enum kdebug_opts opts)
{
	if (!n_events) {
		kd_early_done = true;
		return;
	}

	ktrace_start_single_threaded();

	ktrace_kernel_configure(KTRACE_KDEBUG);

	kdbg_set_nkdbufs_trace(n_events);

	kernel_debug_string_early("start_kern_tracing");

	int error = kdbg_reinit(EXTRA_COPROC_COUNT_BOOT);
	if (error != 0) {
		printf("kdebug: allocation failed, kernel tracing not started: %d\n",
		    error);
		kd_early_done = true;
		goto out;
	}

	/*
	 * Wrapping is disabled because boot and wake tracing is interested in
	 * the earliest events, at the expense of later ones.
	 */
	if ((opts & KDOPT_WRAPPING) == 0) {
		kd_control_trace.kdc_live_flags |= KDBG_NOWRAP;
	}

	if (filter_desc && filter_desc[0] != '\0') {
		kdbg_set_typefilter_string(filter_desc);
		kdbg_enable_typefilter();
	}

	/*
	 * Hold off interrupts between getting a thread map and enabling trace
	 * and until the early traces are recorded.
	 */
	bool s = ml_set_interrupts_enabled(false);

	if (!(opts & KDOPT_ATBOOT)) {
		_threadmap_init();
	}

	kdbg_set_tracing_enabled(true, KDEBUG_ENABLE_TRACE);

	if ((opts & KDOPT_ATBOOT)) {
		/*
		 * Transfer all very early events from the static buffer into the real
		 * buffers.
		 */
		kernel_debug_early_end();
	}

	ml_set_interrupts_enabled(s);

	printf("kernel tracing started with %u events, filter = %s\n", n_events,
	    filter_desc ?: "none");

out:
	ktrace_end_single_threaded();
}

void
kdbg_dump_trace_to_file(const char *filename, bool reenable)
{
	vfs_context_t ctx;
	vnode_t vp;
	size_t write_size;
	int ret;
	int reenable_trace = 0;

	ktrace_lock();

	if (!(kdebug_enable & KDEBUG_ENABLE_TRACE)) {
		goto out;
	}

	if (ktrace_get_owning_pid() != 0) {
		/*
		 * Another process owns ktrace and is still active, disable tracing to
		 * prevent wrapping.
		 */
		kdebug_enable = 0;
		kd_control_trace.enabled = 0;
		commpage_update_kdebug_state();
		goto out;
	}

	KDBG_RELEASE(TRACE_WRITING_EVENTS | DBG_FUNC_START);

	reenable_trace = reenable ? kdebug_enable : 0;
	kdebug_enable = 0;
	kd_control_trace.enabled = 0;
	commpage_update_kdebug_state();

	ctx = vfs_context_kernel();

	if (vnode_open(filename, (O_CREAT | FWRITE | O_NOFOLLOW), 0600, 0, &vp, ctx)) {
		goto out;
	}

	kdbg_write_thread_map(vp, ctx);

	write_size = kd_buffer_trace.kdb_event_count * sizeof(kd_buf);
	ret = _read_merged_trace_events(0, &write_size, vp, ctx, false);
	if (ret) {
		goto out_close;
	}

	/*
	 * Wait to synchronize the file to capture the I/O in the
	 * TRACE_WRITING_EVENTS interval.
	 */
	ret = VNOP_FSYNC(vp, MNT_WAIT, ctx);
	if (ret == KERN_SUCCESS) {
		ret = VNOP_IOCTL(vp, F_FULLFSYNC, (caddr_t)NULL, 0, ctx);
	}

	/*
	 * Balance the starting TRACE_WRITING_EVENTS tracepoint manually.
	 */
	kd_buf end_event = {
		.debugid = TRACE_WRITING_EVENTS | DBG_FUNC_END,
		.arg1 = write_size,
		.arg2 = ret,
		.arg5 = (kd_buf_argtype)thread_tid(current_thread()),
	};
	kdbg_set_timestamp_and_cpu(&end_event, kdebug_timestamp(),
	    cpu_number());

	/* this is best effort -- ignore any errors */
	(void)kdbg_write_to_vnode((caddr_t)&end_event, sizeof(kd_buf), vp, ctx,
	    RAW_file_offset);

out_close:
	vnode_close(vp, FWRITE, ctx);
	sync(current_proc(), (void *)NULL, (int *)NULL);

out:
	if (reenable_trace != 0) {
		kdebug_enable = reenable_trace;
		kd_control_trace.enabled = 1;
		commpage_update_kdebug_state();
	}

	ktrace_unlock();
}

SYSCTL_NODE(_kern, OID_AUTO, kdbg, CTLFLAG_RD | CTLFLAG_LOCKED, 0,
    "kdbg");

SYSCTL_INT(_kern_kdbg, OID_AUTO, debug,
    CTLFLAG_RW | CTLFLAG_LOCKED,
    &kdbg_debug, 0, "Set kdebug debug mode");

SYSCTL_QUAD(_kern_kdbg, OID_AUTO, oldest_time,
    CTLTYPE_QUAD | CTLFLAG_RD | CTLFLAG_LOCKED,
    &kd_control_trace.kdc_oldest_time,
    "Find the oldest timestamp still in trace");
