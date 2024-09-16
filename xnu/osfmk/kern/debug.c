/*
 * Copyright (c) 2000-2020 Apple Inc. All rights reserved.
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

#include <mach_assert.h>
#include <mach_kdp.h>
#include <kdp/kdp.h>
#include <kdp/kdp_core.h>
#include <kdp/kdp_internal.h>
#include <kdp/kdp_callout.h>
#include <kern/cpu_number.h>
#include <kern/kalloc.h>
#include <kern/percpu.h>
#include <kern/spl.h>
#include <kern/thread.h>
#include <kern/assert.h>
#include <kern/sched_prim.h>
#include <kern/socd_client.h>
#include <kern/misc_protos.h>
#include <kern/clock.h>
#include <kern/telemetry.h>
#include <kern/ecc.h>
#include <kern/kern_stackshot.h>
#include <kern/kern_cdata.h>
#include <kern/zalloc_internal.h>
#include <kern/iotrace.h>
#include <pexpert/device_tree.h>
#include <vm/vm_kern_xnu.h>
#include <vm/vm_map.h>
#include <vm/pmap.h>
#include <vm/vm_compressor_xnu.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <sys/pgo.h>
#include <console/serial_protos.h>
#include <IOKit/IOBSD.h>

#if !(MACH_KDP && CONFIG_KDP_INTERACTIVE_DEBUGGING)
#include <kdp/kdp_udp.h>
#endif
#include <kern/processor.h>

#if defined(__i386__) || defined(__x86_64__)
#include <IOKit/IOBSD.h>

#include <i386/cpu_threads.h>
#include <i386/pmCPU.h>
#include <i386/lbr.h>
#endif

#include <IOKit/IOPlatformExpert.h>
#include <machine/machine_cpu.h>
#include <machine/pal_routines.h>

#include <sys/kdebug.h>
#include <libkern/OSKextLibPrivate.h>
#include <libkern/OSAtomic.h>
#include <libkern/kernel_mach_header.h>
#include <libkern/section_keywords.h>
#include <uuid/uuid.h>
#include <mach_debug/zone_info.h>
#include <mach/resource_monitors.h>
#include <machine/machine_routines.h>
#include <sys/proc_require.h>

#include <os/log_private.h>

#include <kern/ext_paniclog.h>

#if defined(__arm64__)
#include <pexpert/pexpert.h> /* For gPanicBase */
#include <arm/caches_internal.h>
#include <arm/misc_protos.h>
extern volatile struct xnu_hw_shmem_dbg_command_info *hwsd_info;
#endif

#include <san/kcov.h>

#if CONFIG_XNUPOST
#include <tests/xnupost.h>
extern int vsnprintf(char *, size_t, const char *, va_list);
#endif

#if CONFIG_CSR
#include <sys/csr.h>
#endif

#if CONFIG_EXCLAVES
#include <xnuproxy/panic.h>
#include "exclaves_panic.h"
#endif

#if CONFIG_SPTM
#include <arm64/sptm/sptm.h>
#include <arm64/sptm/pmap/pmap_data.h>
#endif /* CONFIG_SPTM */

extern int IODTGetLoaderInfo( const char *key, void **infoAddr, int *infosize );
extern void IODTFreeLoaderInfo( const char *key, void *infoAddr, int infoSize );

unsigned int    halt_in_debugger = 0;
unsigned int    current_debugger = 0;
unsigned int    active_debugger = 0;
SECURITY_READ_ONLY_LATE(unsigned int)    panicDebugging = FALSE;
unsigned int    kernel_debugger_entry_count = 0;

#if DEVELOPMENT || DEBUG
unsigned int    panic_test_failure_mode = PANIC_TEST_FAILURE_MODE_BADPTR;
unsigned int    panic_test_action_count = 1;
unsigned int    panic_test_case = PANIC_TEST_CASE_DISABLED;
#endif

#if defined(__arm64__)
struct additional_panic_data_buffer *panic_data_buffers = NULL;
#endif

#if defined(__arm64__)
/*
 * Magic number; this should be identical to the armv7 encoding for trap.
 */
#define TRAP_DEBUGGER __asm__ volatile(".long 0xe7ffdeff")
#elif defined (__x86_64__)
#define TRAP_DEBUGGER __asm__("int3")
#else
#error No TRAP_DEBUGGER for this architecture
#endif

#if defined(__i386__) || defined(__x86_64__)
#define panic_stop()    pmCPUHalt(PM_HALT_PANIC)
#else
#define panic_stop()    panic_spin_forever()
#endif

#if defined(__arm64__) && (DEVELOPMENT || DEBUG)
/*
 * More than enough for any typical format string passed to panic();
 * anything longer will be truncated but that's better than nothing.
 */
#define EARLY_PANIC_BUFLEN 256
#endif

struct debugger_state {
	uint64_t        db_panic_options;
	debugger_op     db_current_op;
	boolean_t       db_proceed_on_sync_failure;
	const char     *db_message;
	const char     *db_panic_str;
	va_list        *db_panic_args;
	void           *db_panic_data_ptr;
	unsigned long   db_panic_caller;
	const char     *db_panic_initiator;
	/* incremented whenever we panic or call Debugger (current CPU panic level) */
	uint32_t        db_entry_count;
	kern_return_t   db_op_return;
};
static struct debugger_state PERCPU_DATA(debugger_state);

/* __pure2 is correct if this function is called with preemption disabled */
static inline __pure2 struct debugger_state *
current_debugger_state(void)
{
	return PERCPU_GET(debugger_state);
}

#define CPUDEBUGGEROP    current_debugger_state()->db_current_op
#define CPUDEBUGGERMSG   current_debugger_state()->db_message
#define CPUPANICSTR      current_debugger_state()->db_panic_str
#define CPUPANICARGS     current_debugger_state()->db_panic_args
#define CPUPANICOPTS     current_debugger_state()->db_panic_options
#define CPUPANICDATAPTR  current_debugger_state()->db_panic_data_ptr
#define CPUDEBUGGERSYNC  current_debugger_state()->db_proceed_on_sync_failure
#define CPUDEBUGGERCOUNT current_debugger_state()->db_entry_count
#define CPUDEBUGGERRET   current_debugger_state()->db_op_return
#define CPUPANICCALLER   current_debugger_state()->db_panic_caller
#define CPUPANICINITIATOR current_debugger_state()->db_panic_initiator


/*
 *  Usage:
 *  panic_test_action_count is in the context of other flags, e.g. for IO errors it is "succeed this many times then fail" and for nesting it is "panic this many times then succeed"
 *  panic_test_failure_mode is a bit map of things to do
 *  panic_test_case is what sort of test we are injecting
 *
 *  For more details see definitions in debugger.h
 *
 *  Note that not all combinations are sensible, but some actions can be combined, e.g.
 *  - BADPTR+SPIN with action count = 3 will cause panic->panic->spin
 *  - BADPTR with action count = 2 will cause 2 nested panics (in addition to the initial panic)
 *  - IO_ERR with action 15 will cause 14 successful IOs, then fail on the next one
 */
#if DEVELOPMENT || DEBUG
#define INJECT_NESTED_PANIC_IF_REQUESTED(requested)                                                                                                                                                                                                         \
MACRO_BEGIN                                                                                                                                                                                                                                                                                                                     \
	if ((panic_test_case & requested) && panic_test_action_count) {                                                                                                                                                                                                                                                                                                \
	    panic_test_action_count--; \
	        volatile int *panic_test_badpointer = (int *)4;                                                                                                                                                                                                                         \
	        if ((panic_test_failure_mode & PANIC_TEST_FAILURE_MODE_SPIN) && (!panic_test_action_count)) { printf("inject spin...\n"); while(panic_test_badpointer); }                                                                       \
	        if ((panic_test_failure_mode & PANIC_TEST_FAILURE_MODE_BADPTR) && (panic_test_action_count+1)) { printf("inject badptr...\n"); *panic_test_badpointer = 0; }                                                                       \
	        if ((panic_test_failure_mode & PANIC_TEST_FAILURE_MODE_PANIC) && (panic_test_action_count+1)) { printf("inject panic...\n"); panic("nested panic level %d", panic_test_action_count); }                      \
	}                                                                                                                                                                                                                                                                                                                               \
MACRO_END

#endif /* DEVELOPMENT || DEBUG */

debugger_op debugger_current_op = DBOP_NONE;
const char *debugger_panic_str = NULL;
va_list *debugger_panic_args = NULL;
void *debugger_panic_data = NULL;
uint64_t debugger_panic_options = 0;
const char *debugger_message = NULL;
unsigned long debugger_panic_caller = 0;
const char *debugger_panic_initiator = "";

void panic_trap_to_debugger(const char *panic_format_str, va_list *panic_args,
    unsigned int reason, void *ctx, uint64_t panic_options_mask, void *panic_data,
    unsigned long panic_caller, const char *panic_initiator) __dead2 __printflike(1, 0);
static void kdp_machine_reboot_type(unsigned int type, uint64_t debugger_flags);
void panic_spin_forever(void) __dead2;
void panic_stackshot_release_lock(void);
extern void PE_panic_hook(const char*);
extern int sync_internal(void);

#define NESTEDDEBUGGERENTRYMAX 5
static TUNABLE(unsigned int, max_debugger_entry_count, "nested_panic_max",
    NESTEDDEBUGGERENTRYMAX);

SECURITY_READ_ONLY_LATE(bool) awl_scratch_reg_supported = false;
static bool PERCPU_DATA(hv_entry_detected); // = false
static void awl_set_scratch_reg_hv_bit(void);
void awl_mark_hv_entry(void);
static bool awl_pm_state_change_cbk(void *param, enum cpu_event event, unsigned int cpu_or_cluster);

#if !XNU_TARGET_OS_OSX & CONFIG_KDP_INTERACTIVE_DEBUGGING
static boolean_t device_corefile_valid_on_ephemeral(void);
#endif /* !XNU_TARGET_OS_OSX & CONFIG_KDP_INTERACTIVE_DEBUGGING */

#if defined(__arm64__)
#define DEBUG_BUF_SIZE (4096)

/* debug_buf is directly linked with iBoot panic region for arm targets */
char *debug_buf_base = NULL;
char *debug_buf_ptr = NULL;
unsigned int debug_buf_size = 0;

SECURITY_READ_ONLY_LATE(boolean_t) kdp_explicitly_requested = FALSE;
#else /* defined(__arm64__) */
#define DEBUG_BUF_SIZE ((3 * PAGE_SIZE) + offsetof(struct macos_panic_header, mph_data))
/* EXTENDED_DEBUG_BUF_SIZE definition is now in debug.h */
static_assert(((EXTENDED_DEBUG_BUF_SIZE % PANIC_FLUSH_BOUNDARY) == 0), "Extended debug buf size must match SMC alignment requirements");

char debug_buf[DEBUG_BUF_SIZE];
struct macos_panic_header *panic_info = (struct macos_panic_header *)debug_buf;
char *debug_buf_base = (debug_buf + offsetof(struct macos_panic_header, mph_data));
char *debug_buf_ptr = (debug_buf + offsetof(struct macos_panic_header, mph_data));

/*
 * We don't include the size of the panic header in the length of the data we actually write.
 * On co-processor platforms, we lose sizeof(struct macos_panic_header) bytes from the end of
 * the end of the log because we only support writing (3*PAGESIZE) bytes.
 */
unsigned int debug_buf_size = (DEBUG_BUF_SIZE - offsetof(struct macos_panic_header, mph_data));

boolean_t extended_debug_log_enabled = FALSE;
#endif /* defined(__arm64__) */

#if defined(XNU_TARGET_OS_OSX)
#define KDBG_TRACE_PANIC_FILENAME "/var/tmp/panic.trace"
#else
#define KDBG_TRACE_PANIC_FILENAME "/var/log/panic.trace"
#endif

/* Debugger state */
atomic_int     debugger_cpu = DEBUGGER_NO_CPU;
boolean_t      debugger_allcpus_halted = FALSE;
boolean_t      debugger_safe_to_return = TRUE;
unsigned int   debugger_context = 0;

static char model_name[64];
unsigned char *kernel_uuid;

boolean_t kernelcache_uuid_valid = FALSE;
uuid_t kernelcache_uuid;
uuid_string_t kernelcache_uuid_string;

boolean_t pageablekc_uuid_valid = FALSE;
uuid_t pageablekc_uuid;
uuid_string_t pageablekc_uuid_string;

boolean_t auxkc_uuid_valid = FALSE;
uuid_t auxkc_uuid;
uuid_string_t auxkc_uuid_string;


/*
 * By default we treat Debugger() the same as calls to panic(), unless
 * we have debug boot-args present and the DB_KERN_DUMP_ON_NMI *NOT* set.
 * If DB_KERN_DUMP_ON_NMI is *NOT* set, return from Debugger() is supported.
 *
 * Return from Debugger() is currently only implemented on x86
 */
static boolean_t debugger_is_panic = TRUE;

TUNABLE(unsigned int, debug_boot_arg, "debug", 0);

TUNABLE(int, verbose_panic_flow_logging, "verbose_panic_flow_logging", 0);

char kernel_uuid_string[37]; /* uuid_string_t */
char kernelcache_uuid_string[37]; /* uuid_string_t */
char   panic_disk_error_description[512];
size_t panic_disk_error_description_size = sizeof(panic_disk_error_description);

extern unsigned int write_trace_on_panic;
int kext_assertions_enable =
#if DEBUG || DEVELOPMENT
    TRUE;
#else
    FALSE;
#endif

#if (DEVELOPMENT || DEBUG)
uint64_t xnu_platform_stall_value = PLATFORM_STALL_XNU_DISABLE;
#endif

/*
 * Maintain the physically-contiguous carveouts for the carveout bootargs.
 */
TUNABLE_WRITEABLE(boolean_t, phys_carveout_core, "phys_carveout_core", 1);

TUNABLE(uint32_t, phys_carveout_mb, "phys_carveout_mb", 0);
SECURITY_READ_ONLY_LATE(vm_offset_t) phys_carveout = 0;
SECURITY_READ_ONLY_LATE(uintptr_t) phys_carveout_pa = 0;
SECURITY_READ_ONLY_LATE(size_t) phys_carveout_size = 0;


/*
 * Returns whether kernel debugging is expected to be restricted
 * on the device currently based on CSR or other platform restrictions.
 */
boolean_t
kernel_debugging_restricted(void)
{
#if XNU_TARGET_OS_OSX
#if CONFIG_CSR
	if (csr_check(CSR_ALLOW_KERNEL_DEBUGGER) != 0) {
		return TRUE;
	}
#endif /* CONFIG_CSR */
	return FALSE;
#else /* XNU_TARGET_OS_OSX */
	return FALSE;
#endif /* XNU_TARGET_OS_OSX */
}

__startup_func
static void
panic_init(void)
{
	unsigned long uuidlen = 0;
	void *uuid;

	uuid = getuuidfromheader(&_mh_execute_header, &uuidlen);
	if ((uuid != NULL) && (uuidlen == sizeof(uuid_t))) {
		kernel_uuid = uuid;
		uuid_unparse_upper(*(uuid_t *)uuid, kernel_uuid_string);
	}

	/*
	 * Take the value of the debug boot-arg into account
	 */
#if MACH_KDP
	if (!kernel_debugging_restricted() && debug_boot_arg) {
		if (debug_boot_arg & DB_HALT) {
			halt_in_debugger = 1;
		}

#if defined(__arm64__)
		if (debug_boot_arg & DB_NMI) {
			panicDebugging  = TRUE;
		}
#else
		panicDebugging = TRUE;
#endif /* defined(__arm64__) */
	}

#if defined(__arm64__)
	char kdpname[80];

	kdp_explicitly_requested = PE_parse_boot_argn("kdp_match_name", kdpname, sizeof(kdpname));
#endif /* defined(__arm64__) */

#endif /* MACH_KDP */

#if defined (__x86_64__)
	/*
	 * By default we treat Debugger() the same as calls to panic(), unless
	 * we have debug boot-args present and the DB_KERN_DUMP_ON_NMI *NOT* set.
	 * If DB_KERN_DUMP_ON_NMI is *NOT* set, return from Debugger() is supported.
	 * This is because writing an on-device corefile is a destructive operation.
	 *
	 * Return from Debugger() is currently only implemented on x86
	 */
	if (PE_i_can_has_debugger(NULL) && !(debug_boot_arg & DB_KERN_DUMP_ON_NMI)) {
		debugger_is_panic = FALSE;
	}
#endif
}
STARTUP(TUNABLES, STARTUP_RANK_MIDDLE, panic_init);

#if defined (__x86_64__)
void
extended_debug_log_init(void)
{
	assert(coprocessor_paniclog_flush);
	/*
	 * Allocate an extended panic log buffer that has space for the panic
	 * stackshot at the end. Update the debug buf pointers appropriately
	 * to point at this new buffer.
	 *
	 * iBoot pre-initializes the panic region with the NULL character. We set this here
	 * so we can accurately calculate the CRC for the region without needing to flush the
	 * full region over SMC.
	 */
	char *new_debug_buf = kalloc_data(EXTENDED_DEBUG_BUF_SIZE, Z_WAITOK | Z_ZERO);

	panic_info = (struct macos_panic_header *)new_debug_buf;
	debug_buf_ptr = debug_buf_base = (new_debug_buf + offsetof(struct macos_panic_header, mph_data));
	debug_buf_size = (EXTENDED_DEBUG_BUF_SIZE - offsetof(struct macos_panic_header, mph_data));

	extended_debug_log_enabled = TRUE;

	/*
	 * Insert a compiler barrier so we don't free the other panic stackshot buffer
	 * until after we've marked the new one as available
	 */
	__compiler_barrier();
	kmem_free(kernel_map, panic_stackshot_buf, panic_stackshot_buf_len);
	panic_stackshot_buf = 0;
	panic_stackshot_buf_len = 0;
}
#endif /* defined (__x86_64__) */

void
debug_log_init(void)
{
#if defined(__arm64__)
	if (!gPanicBase) {
		printf("debug_log_init: Error!! gPanicBase is still not initialized\n");
		return;
	}
	/* Shift debug buf start location and size by the length of the panic header */
	debug_buf_base = (char *)gPanicBase + sizeof(struct embedded_panic_header);
	debug_buf_ptr = debug_buf_base;
	debug_buf_size = gPanicSize - sizeof(struct embedded_panic_header);

#if CONFIG_EXT_PANICLOG
	ext_paniclog_init();
#endif
#else
	kern_return_t kr = KERN_SUCCESS;
	bzero(panic_info, DEBUG_BUF_SIZE);

	assert(debug_buf_base != NULL);
	assert(debug_buf_ptr != NULL);
	assert(debug_buf_size != 0);

	/*
	 * We allocate a buffer to store a panic time stackshot. If we later discover that this is a
	 * system that supports flushing a stackshot via an extended debug log (see above), we'll free this memory
	 * as it's not necessary on this platform. This information won't be available until the IOPlatform has come
	 * up.
	 */
	kr = kmem_alloc(kernel_map, &panic_stackshot_buf, PANIC_STACKSHOT_BUFSIZE,
	    KMA_DATA | KMA_ZERO, VM_KERN_MEMORY_DIAG);
	assert(kr == KERN_SUCCESS);
	if (kr == KERN_SUCCESS) {
		panic_stackshot_buf_len = PANIC_STACKSHOT_BUFSIZE;
	}
#endif
}

void
phys_carveout_init(void)
{
	if (!PE_i_can_has_debugger(NULL)) {
		return;
	}

#if __arm__ || __arm64__
#if DEVELOPMENT || DEBUG
#endif /* DEVELOPMENT || DEBUG  */
#endif /* __arm__ || __arm64__ */

	struct carveout {
		const char *name;
		vm_offset_t *va;
		uint32_t requested_size;
		uintptr_t *pa;
		size_t *allocated_size;
		uint64_t present;
	} carveouts[] = {
		{
			"phys_carveout",
			&phys_carveout,
			phys_carveout_mb,
			&phys_carveout_pa,
			&phys_carveout_size,
			phys_carveout_mb != 0,
		}
	};

	for (int i = 0; i < (sizeof(carveouts) / sizeof(struct carveout)); i++) {
		if (carveouts[i].present) {
			size_t temp_carveout_size = 0;
			if (os_mul_overflow(carveouts[i].requested_size, 1024 * 1024, &temp_carveout_size)) {
				panic("%s_mb size overflowed (%uMB)",
				    carveouts[i].name, carveouts[i].requested_size);
				return;
			}

			kmem_alloc_contig(kernel_map, carveouts[i].va,
			    temp_carveout_size, PAGE_MASK, 0, 0,
			    KMA_NOFAIL | KMA_PERMANENT | KMA_NOPAGEWAIT | KMA_DATA,
			    VM_KERN_MEMORY_DIAG);

			*carveouts[i].pa = kvtophys(*carveouts[i].va);
			*carveouts[i].allocated_size = temp_carveout_size;
		}
	}

#if __arm64__ && (DEVELOPMENT || DEBUG)
	/* likely panic_trace boot-arg is also set so check and enable tracing if necessary into new carveout */
	PE_arm_debug_enable_trace(true);
#endif /* __arm64__ && (DEVELOPMENT || DEBUG) */
}

boolean_t
debug_is_in_phys_carveout(vm_map_offset_t va)
{
	return phys_carveout_size && va >= phys_carveout &&
	       va < (phys_carveout + phys_carveout_size);
}

boolean_t
debug_can_coredump_phys_carveout(void)
{
	return phys_carveout_core;
}

static void
DebuggerLock(void)
{
	int my_cpu = cpu_number();
	int debugger_exp_cpu = DEBUGGER_NO_CPU;
	assert(ml_get_interrupts_enabled() == FALSE);

	if (atomic_load(&debugger_cpu) == my_cpu) {
		return;
	}

	while (!atomic_compare_exchange_strong(&debugger_cpu, &debugger_exp_cpu, my_cpu)) {
		debugger_exp_cpu = DEBUGGER_NO_CPU;
	}

	return;
}

static void
DebuggerUnlock(void)
{
	assert(atomic_load_explicit(&debugger_cpu, memory_order_relaxed) == cpu_number());

	/*
	 * We don't do an atomic exchange here in case
	 * there's another CPU spinning to acquire the debugger_lock
	 * and we never get a chance to update it. We already have the
	 * lock so we can simply store DEBUGGER_NO_CPU and follow with
	 * a barrier.
	 */
	atomic_store(&debugger_cpu, DEBUGGER_NO_CPU);
	OSMemoryBarrier();

	return;
}

static kern_return_t
DebuggerHaltOtherCores(boolean_t proceed_on_failure, bool is_stackshot)
{
#if defined(__arm64__)
	return DebuggerXCallEnter(proceed_on_failure, is_stackshot);
#else /* defined(__arm64__) */
#pragma unused(proceed_on_failure)
	mp_kdp_enter(proceed_on_failure, is_stackshot);
	return KERN_SUCCESS;
#endif
}

static void
DebuggerResumeOtherCores(void)
{
#if defined(__arm64__)
	DebuggerXCallReturn();
#else /* defined(__arm64__) */
	mp_kdp_exit();
#endif
}

__printflike(3, 0)
static void
DebuggerSaveState(debugger_op db_op, const char *db_message, const char *db_panic_str,
    va_list *db_panic_args, uint64_t db_panic_options, void *db_panic_data_ptr,
    boolean_t db_proceed_on_sync_failure, unsigned long db_panic_caller, const char *db_panic_initiator)
{
	CPUDEBUGGEROP = db_op;

	/*
	 * Note:
	 * if CPUDEBUGGERCOUNT == 1 then we are in the normal case - record the panic data
	 * if CPUDEBUGGERCOUNT > 1 and CPUPANICSTR == NULL then we are in a nested panic that happened before DebuggerSaveState was called, so store the nested panic data
	 * if CPUDEBUGGERCOUNT > 1 and CPUPANICSTR != NULL then we are in a nested panic that happened after DebuggerSaveState was called, so leave the original panic data
	 *
	 * TODO: is it safe to flatten this to if (CPUPANICSTR == NULL)?
	 */
	if (CPUDEBUGGERCOUNT == 1 || CPUPANICSTR == NULL) {
		CPUDEBUGGERMSG = db_message;
		CPUPANICSTR = db_panic_str;
		CPUPANICARGS = db_panic_args;
		CPUPANICDATAPTR = db_panic_data_ptr;
		CPUPANICCALLER = db_panic_caller;
		CPUPANICINITIATOR = db_panic_initiator;

#if CONFIG_EXCLAVES
		char *panic_str;
		if (exclaves_panic_get_string(&panic_str) == KERN_SUCCESS) {
			CPUPANICSTR = panic_str;
		}
#endif
	}

	CPUDEBUGGERSYNC = db_proceed_on_sync_failure;
	CPUDEBUGGERRET = KERN_SUCCESS;

	/* Reset these on any nested panics */
	// follow up in rdar://88497308 (nested panics should not clobber panic flags)
	CPUPANICOPTS = db_panic_options;

	return;
}

/*
 * Save the requested debugger state/action into the current processor's
 * percu state and trap to the debugger.
 */
kern_return_t
DebuggerTrapWithState(debugger_op db_op, const char *db_message, const char *db_panic_str,
    va_list *db_panic_args, uint64_t db_panic_options, void *db_panic_data_ptr,
    boolean_t db_proceed_on_sync_failure, unsigned long db_panic_caller, const char* db_panic_initiator)
{
	kern_return_t ret;

#if defined(__arm64__) && (DEVELOPMENT || DEBUG)
	if (!PE_arm_debug_and_trace_initialized()) {
		/*
		 * In practice this can only happen if we panicked very early,
		 * when only the boot CPU is online and before it has finished
		 * initializing the debug and trace infrastructure. We're going
		 * to hang soon, so let's at least make sure the message passed
		 * to panic() is actually logged.
		 */
		char buf[EARLY_PANIC_BUFLEN];
		vsnprintf(buf, EARLY_PANIC_BUFLEN, db_panic_str, *db_panic_args);
		paniclog_append_noflush("%s\n", buf);
	}
#endif

	assert(ml_get_interrupts_enabled() == FALSE);
	DebuggerSaveState(db_op, db_message, db_panic_str, db_panic_args,
	    db_panic_options, db_panic_data_ptr,
	    db_proceed_on_sync_failure, db_panic_caller, db_panic_initiator);

	/*
	 * On ARM this generates an uncategorized exception -> sleh code ->
	 *   DebuggerCall -> kdp_trap -> handle_debugger_trap
	 * So that is how XNU ensures that only one core can panic.
	 * The rest of the cores are halted by IPI if possible; if that
	 * fails it will fall back to dbgwrap.
	 */
	TRAP_DEBUGGER;

	ret = CPUDEBUGGERRET;

	DebuggerSaveState(DBOP_NONE, NULL, NULL, NULL, 0, NULL, FALSE, 0, NULL);

	return ret;
}

void __attribute__((noinline))
Assert(
	const char      *file,
	int             line,
	const char      *expression
	)
{
#if CONFIG_NONFATAL_ASSERTS
	static TUNABLE(bool, mach_assert, "assertions", true);

	if (!mach_assert) {
		kprintf("%s:%d non-fatal Assertion: %s", file, line, expression);
		return;
	}
#endif

	panic_plain("%s:%d Assertion failed: %s", file, line, expression);
}

boolean_t
debug_is_current_cpu_in_panic_state(void)
{
	return current_debugger_state()->db_entry_count > 0;
}

/*
 * check if we are in a nested panic, report findings, take evasive action where necessary
 *
 * see also PE_update_panicheader_nestedpanic
 */
static void
check_and_handle_nested_panic(uint64_t panic_options_mask, unsigned long panic_caller, const char *db_panic_str, va_list *db_panic_args)
{
	if ((CPUDEBUGGERCOUNT > 1) && (CPUDEBUGGERCOUNT < max_debugger_entry_count)) {
		// Note: this is the first indication in the panic log or serial that we are off the rails...
		//
		// if we panic *before* the paniclog is finalized then this will end up in the ips report with a panic_caller addr that gives us a clue
		// if we panic *after* the log is finalized then we will only see it in the serial log
		//
		paniclog_append_noflush("Nested panic detected - entry count: %d panic_caller: 0x%016lx\n", CPUDEBUGGERCOUNT, panic_caller);
		paniclog_flush();

		// print the *new* panic string to the console, we might not get it by other means...
		// TODO: I tried to write this stuff to the paniclog, but the serial output gets corrupted and the panicstring in the ips file is <mysterious>
		// rdar://87846117 (NestedPanic: output panic string to paniclog)
		if (db_panic_str) {
			printf("Nested panic string:\n");
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
			_doprnt(db_panic_str, db_panic_args, PE_kputc, 0);
#pragma clang diagnostic pop
			printf("\n<end nested panic string>\n");
		}
	}

	// Stage 1 bailout
	//
	// Try to complete the normal panic flow, i.e. try to make sure the callouts happen and we flush the paniclog.  If this fails with another nested
	// panic then we will land in Stage 2 below...
	//
	if (CPUDEBUGGERCOUNT == max_debugger_entry_count) {
		uint32_t panic_details = 0;

		// if this is a force-reset panic then capture a log and reboot immediately.
		if (panic_options_mask & DEBUGGER_OPTION_PANICLOGANDREBOOT) {
			panic_details |= kPanicDetailsForcePowerOff;
		}

		// normally the kPEPanicBegin is sent from debugger_collect_diagnostics(), but we might nested-panic before we get
		// there.  To be safe send another notification, the function called below will only send kPEPanicBegin if it has not yet been sent.
		//
		PEHaltRestartInternal(kPEPanicBegin, panic_details);

		paniclog_append_noflush("Nested panic count exceeds limit %d, machine will reset or spin\n", max_debugger_entry_count);
		PE_update_panicheader_nestedpanic();
		paniclog_flush();

		if (!panicDebugging) {
			// note that this will also send kPEPanicEnd
			kdp_machine_reboot_type(kPEPanicRestartCPU, panic_options_mask);
		}

		// prints to console
		paniclog_append_noflush("\nNested panic stall. Stage 1 bailout. Please go to https://panic.apple.com to report this panic\n");
		panic_spin_forever();
	}

	// Stage 2 bailout
	//
	// Things are severely hosed, we have nested to the point of bailout and then nested again during the bailout path.  Try to issue
	// a chipreset as quickly as possible, hopefully something in the panic log is salvageable, since we flushed it during Stage 1.
	//
	if (CPUDEBUGGERCOUNT == max_debugger_entry_count + 1) {
		if (!panicDebugging) {
			// note that:
			// - this code path should be audited for prints, as that is a common cause of nested panics
			// - this code path should take the fastest route to the actual reset, and not call any un-necessary code
			kdp_machine_reboot_type(kPEPanicRestartCPU, panic_options_mask & DEBUGGER_OPTION_SKIP_PANICEND_CALLOUTS);
		}

		// prints to console, but another nested panic will land in Stage 3 where we simply spin, so that is sort of ok...
		paniclog_append_noflush("\nIn Nested panic stall. Stage 2 bailout. Please go to https://panic.apple.com to report this panic\n");
		panic_spin_forever();
	}

	// Stage 3 bailout
	//
	// We are done here, we were unable to reset the platform without another nested panic.  Spin until the watchdog kicks in.
	//
	if (CPUDEBUGGERCOUNT > max_debugger_entry_count + 1) {
		kdp_machine_reboot_type(kPEHangCPU, 0);
	}
}

void
Debugger(const char *message)
{
	DebuggerWithContext(0, NULL, message, DEBUGGER_OPTION_NONE, (unsigned long)(char *)__builtin_return_address(0));
}

/*
 *  Enter the Debugger
 *
 *  This is similar to, but not the same as a panic
 *
 *  Key differences:
 *  - we get here from a debugger entry action (e.g. NMI)
 *  - the system is resumable on x86 (in theory, however it is not clear if this is tested)
 *  - rdar://57738811 (xnu: support resume from debugger via KDP on arm devices)
 *
 */
void
DebuggerWithContext(unsigned int reason, void *ctx, const char *message,
    uint64_t debugger_options_mask, unsigned long debugger_caller)
{
	spl_t previous_interrupts_state;
	boolean_t old_doprnt_hide_pointers = doprnt_hide_pointers;

#if defined(__x86_64__) && (DEVELOPMENT || DEBUG)
	read_lbr();
#endif
	previous_interrupts_state = ml_set_interrupts_enabled(FALSE);
	disable_preemption();

	/* track depth of debugger/panic entry */
	CPUDEBUGGERCOUNT++;

	/* emit a tracepoint as early as possible in case of hang */
	SOCD_TRACE_XNU(PANIC, PACK_2X32(VALUE(cpu_number()), VALUE(CPUDEBUGGERCOUNT)), VALUE(debugger_options_mask), ADDR(message), ADDR(debugger_caller));

	/* do max nested panic/debugger check, this will report nesting to the console and spin forever if we exceed a limit */
	check_and_handle_nested_panic(debugger_options_mask, debugger_caller, message, NULL);

	/* Handle any necessary platform specific actions before we proceed */
	PEInitiatePanic();

#if DEVELOPMENT || DEBUG
	INJECT_NESTED_PANIC_IF_REQUESTED(PANIC_TEST_CASE_RECURPANIC_ENTRY);
#endif

	PE_panic_hook(message);

	doprnt_hide_pointers = FALSE;

	if (ctx != NULL) {
		DebuggerSaveState(DBOP_DEBUGGER, message,
		    NULL, NULL, debugger_options_mask, NULL, TRUE, 0, "");
		handle_debugger_trap(reason, 0, 0, ctx);
		DebuggerSaveState(DBOP_NONE, NULL, NULL,
		    NULL, 0, NULL, FALSE, 0, "");
	} else {
		DebuggerTrapWithState(DBOP_DEBUGGER, message,
		    NULL, NULL, debugger_options_mask, NULL, TRUE, 0, NULL);
	}

	/* resume from the debugger */

	CPUDEBUGGERCOUNT--;
	doprnt_hide_pointers = old_doprnt_hide_pointers;
	enable_preemption();
	ml_set_interrupts_enabled(previous_interrupts_state);
}

static struct kdp_callout {
	struct kdp_callout * callout_next;
	kdp_callout_fn_t callout_fn;
	boolean_t callout_in_progress;
	void * callout_arg;
} * kdp_callout_list = NULL;

/*
 * Called from kernel context to register a kdp event callout.
 */
void
kdp_register_callout(kdp_callout_fn_t fn, void * arg)
{
	struct kdp_callout * kcp;
	struct kdp_callout * list_head;

	kcp = zalloc_permanent_type(struct kdp_callout);

	kcp->callout_fn = fn;
	kcp->callout_arg = arg;
	kcp->callout_in_progress = FALSE;

	/* Lock-less list insertion using compare and exchange. */
	do {
		list_head = kdp_callout_list;
		kcp->callout_next = list_head;
	} while (!OSCompareAndSwapPtr(list_head, kcp, &kdp_callout_list));
}

static void
kdp_callouts(kdp_event_t event)
{
	struct kdp_callout      *kcp = kdp_callout_list;

	while (kcp) {
		if (!kcp->callout_in_progress) {
			kcp->callout_in_progress = TRUE;
			kcp->callout_fn(kcp->callout_arg, event);
			kcp->callout_in_progress = FALSE;
		}
		kcp = kcp->callout_next;
	}
}

#if defined(__arm64__)
/*
 * Register an additional buffer with data to include in the panic log
 *
 * <rdar://problem/50137705> tracks supporting more than one buffer
 *
 * Note that producer_name and buf should never be de-allocated as we reference these during panic.
 */
void
register_additional_panic_data_buffer(const char *producer_name, void *buf, int len)
{
	if (panic_data_buffers != NULL) {
		panic("register_additional_panic_data_buffer called with buffer already registered");
	}

	if (producer_name == NULL || (strlen(producer_name) == 0)) {
		panic("register_additional_panic_data_buffer called with invalid producer_name");
	}

	if (buf == NULL) {
		panic("register_additional_panic_data_buffer called with invalid buffer pointer");
	}

	if ((len <= 0) || (len > ADDITIONAL_PANIC_DATA_BUFFER_MAX_LEN)) {
		panic("register_additional_panic_data_buffer called with invalid length");
	}

	struct additional_panic_data_buffer *new_panic_data_buffer = zalloc_permanent_type(struct additional_panic_data_buffer);
	new_panic_data_buffer->producer_name = producer_name;
	new_panic_data_buffer->buf = buf;
	new_panic_data_buffer->len = len;

	if (!OSCompareAndSwapPtr(NULL, new_panic_data_buffer, &panic_data_buffers)) {
		panic("register_additional_panic_data_buffer called with buffer already registered");
	}

	return;
}
#endif /* defined(__arm64__) */

/*
 * An overview of the xnu panic path:
 *
 * Several panic wrappers (panic(), panic_with_options(), etc.) all funnel into panic_trap_to_debugger().
 * panic_trap_to_debugger() sets the panic state in the current processor's debugger_state prior
 * to trapping into the debugger. Once we trap to the debugger, we end up in handle_debugger_trap()
 * which tries to acquire the panic lock by atomically swapping the current CPU number into debugger_cpu.
 * debugger_cpu acts as a synchronization point, from which the winning CPU can halt the other cores and
 * continue to debugger_collect_diagnostics() where we write the paniclog, corefile (if appropriate) and proceed
 * according to the device's boot-args.
 */
#undef panic
void
panic(const char *str, ...)
{
	va_list panic_str_args;

	va_start(panic_str_args, str);
	panic_trap_to_debugger(str, &panic_str_args, 0, NULL, 0, NULL, (unsigned long)(char *)__builtin_return_address(0), NULL);
	va_end(panic_str_args);
}

void
panic_with_data(uuid_t uuid, void *addr, uint32_t len, const char *str, ...)
{
	va_list panic_str_args;

	ext_paniclog_panic_with_data(uuid, addr, len);

	va_start(panic_str_args, str);
	panic_trap_to_debugger(str, &panic_str_args, 0, NULL, 0, NULL, (unsigned long)(char *)__builtin_return_address(0), NULL);
	va_end(panic_str_args);
}

void
panic_with_options(unsigned int reason, void *ctx, uint64_t debugger_options_mask, const char *str, ...)
{
	va_list panic_str_args;

	va_start(panic_str_args, str);
	panic_trap_to_debugger(str, &panic_str_args, reason, ctx, (debugger_options_mask & ~DEBUGGER_INTERNAL_OPTIONS_MASK),
	    NULL, (unsigned long)(char *)__builtin_return_address(0), NULL);
	va_end(panic_str_args);
}

void
panic_with_options_and_initiator(const char* initiator, unsigned int reason, void *ctx, uint64_t debugger_options_mask, const char *str, ...)
{
	va_list panic_str_args;

	va_start(panic_str_args, str);
	panic_trap_to_debugger(str, &panic_str_args, reason, ctx, (debugger_options_mask & ~DEBUGGER_INTERNAL_OPTIONS_MASK),
	    NULL, (unsigned long)(char *)__builtin_return_address(0), initiator);
	va_end(panic_str_args);
}

boolean_t
panic_validate_ptr(void *ptr, vm_size_t size, const char *what)
{
	if (ptr == NULL) {
		paniclog_append_noflush("NULL %s pointer\n", what);
		return false;
	}

	if (!ml_validate_nofault((vm_offset_t)ptr, size)) {
		paniclog_append_noflush("Invalid %s pointer: %p (size %d)\n",
		    what, ptr, (uint32_t)size);
		return false;
	}

	return true;
}

boolean_t
panic_get_thread_proc_task(struct thread *thread, struct task **task, struct proc **proc)
{
	if (!PANIC_VALIDATE_PTR(thread)) {
		return false;
	}

	if (!PANIC_VALIDATE_PTR(thread->t_tro)) {
		return false;
	}

	if (!PANIC_VALIDATE_PTR(thread->t_tro->tro_task)) {
		return false;
	}

	if (task) {
		*task = thread->t_tro->tro_task;
	}

	if (!panic_validate_ptr(thread->t_tro->tro_proc,
	    sizeof(struct proc *), "bsd_info")) {
		*proc = NULL;
	} else {
		*proc = thread->t_tro->tro_proc;
	}

	return true;
}

#if defined (__x86_64__)
/*
 * panic_with_thread_context() is used on x86 platforms to specify a different thread that should be backtraced in the paniclog.
 * We don't generally need this functionality on embedded platforms because embedded platforms include a panic time stackshot
 * from customer devices. We plumb the thread pointer via the debugger trap mechanism and backtrace the kernel stack from the
 * thread when writing the panic log.
 *
 * NOTE: panic_with_thread_context() should be called with an explicit thread reference held on the passed thread.
 */
void
panic_with_thread_context(unsigned int reason, void *ctx, uint64_t debugger_options_mask, thread_t thread, const char *str, ...)
{
	va_list panic_str_args;
	__assert_only os_ref_count_t th_ref_count;

	assert_thread_magic(thread);
	th_ref_count = os_ref_get_count_raw(&thread->ref_count);
	assertf(th_ref_count > 0, "panic_with_thread_context called with invalid thread %p with refcount %u", thread, th_ref_count);

	/* Take a reference on the thread so it doesn't disappear by the time we try to backtrace it */
	thread_reference(thread);

	va_start(panic_str_args, str);
	panic_trap_to_debugger(str, &panic_str_args, reason, ctx, ((debugger_options_mask & ~DEBUGGER_INTERNAL_OPTIONS_MASK) | DEBUGGER_INTERNAL_OPTION_THREAD_BACKTRACE),
	    thread, (unsigned long)(char *)__builtin_return_address(0), "");

	va_end(panic_str_args);
}
#endif /* defined (__x86_64__) */

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wmissing-noreturn"
void
panic_trap_to_debugger(const char *panic_format_str, va_list *panic_args, unsigned int reason, void *ctx,
    uint64_t panic_options_mask, void *panic_data_ptr, unsigned long panic_caller, const char *panic_initiator)
{
#pragma clang diagnostic pop

#if defined(__x86_64__) && (DEVELOPMENT || DEBUG)
	read_lbr();
#endif

	/* optionally call sync, to reduce lost logs on restart, avoid on recursive panic. Unsafe due to unbounded sync() duration */
	if ((panic_options_mask & DEBUGGER_OPTION_SYNC_ON_PANIC_UNSAFE) && (CPUDEBUGGERCOUNT == 0)) {
		sync_internal();
	}

	/* Turn off I/O tracing once we've panicked */
	iotrace_disable();

	/* call machine-layer panic handler */
	ml_panic_trap_to_debugger(panic_format_str, panic_args, reason, ctx, panic_options_mask, panic_caller, panic_initiator);

	/* track depth of debugger/panic entry */
	CPUDEBUGGERCOUNT++;

	/* emit a tracepoint as early as possible in case of hang */
	SOCD_TRACE_XNU(PANIC, PACK_2X32(VALUE(cpu_number()), VALUE(CPUDEBUGGERCOUNT)), VALUE(panic_options_mask), ADDR(panic_format_str), ADDR(panic_caller));

	/* do max nested panic/debugger check, this will report nesting to the console and spin forever if we exceed a limit */
	check_and_handle_nested_panic(panic_options_mask, panic_caller, panic_format_str, panic_args);

	/* If we're in a stackshot, signal that we've started panicking and wait for other CPUs to coalesce and spin before proceeding */
	stackshot_cpu_signal_panic();

	/* Handle any necessary platform specific actions before we proceed */
	PEInitiatePanic();

#if DEVELOPMENT || DEBUG
	INJECT_NESTED_PANIC_IF_REQUESTED(PANIC_TEST_CASE_RECURPANIC_ENTRY);
#endif

	PE_panic_hook(panic_format_str);

#if defined (__x86_64__)
	plctrace_disable();
#endif

	if (write_trace_on_panic && kdebug_enable) {
		if (get_preemption_level() == 0 && !ml_at_interrupt_context()) {
			ml_set_interrupts_enabled(TRUE);
			KDBG_RELEASE(TRACE_PANIC);
			kdbg_dump_trace_to_file(KDBG_TRACE_PANIC_FILENAME, false);
		}
	}

	ml_set_interrupts_enabled(FALSE);
	disable_preemption();

#if CONFIG_SPTM
	/*
	 * If SPTM has not itself already panicked, trigger a panic lockdown. This
	 * check is necessary since attempting to re-enter the SPTM after it calls
	 * panic will lead to a hang, which harms kernel field debugability.
	 *
	 * Whether or not this check can be subverted is murky. This doesn't really
	 * matter, however, because any security critical panics events will have
	 * already initiated lockdown before calling panic. Thus, lockdown from
	 * panic itself is merely a "best effort".
	 */
	libsptm_error_t sptm_error = LIBSPTM_SUCCESS;
	bool sptm_has_panicked = false;
	if (((sptm_error = sptm_triggered_panic(&sptm_has_panicked)) == LIBSPTM_SUCCESS) &&
	    !sptm_has_panicked) {
		sptm_xnu_panic_begin();
	}
#endif /* CONFIG_SPTM */

#if defined (__x86_64__)
	pmSafeMode(x86_lcpu(), PM_SAFE_FL_SAFE);
#endif /* defined (__x86_64__) */

	/* Never hide pointers from panic logs. */
	doprnt_hide_pointers = FALSE;

	if (ctx != NULL) {
		/*
		 * We called into panic from a trap, no need to trap again. Set the
		 * state on the current CPU and then jump to handle_debugger_trap.
		 */
		DebuggerSaveState(DBOP_PANIC, "panic",
		    panic_format_str, panic_args,
		    panic_options_mask, panic_data_ptr, TRUE, panic_caller, panic_initiator);
		handle_debugger_trap(reason, 0, 0, ctx);
	}

#if defined(__arm64__) && !APPLEVIRTUALPLATFORM
	/*
	 *  Signal to fastsim that it should open debug ports (nop on hardware)
	 */
	__asm__         volatile ("hint #0x45");
#endif /* defined(__arm64__) && !APPLEVIRTUALPLATFORM */

	DebuggerTrapWithState(DBOP_PANIC, "panic", panic_format_str,
	    panic_args, panic_options_mask, panic_data_ptr, TRUE, panic_caller, panic_initiator);

	/*
	 * Not reached.
	 */
	panic_stop();
	__builtin_unreachable();
}

void
panic_spin_forever(void)
{
	for (;;) {
#if defined(__arm__) || defined(__arm64__)
		/* On arm32, which doesn't have a WFE timeout, this may not return.  But that should be OK on this path. */
		__builtin_arm_wfe();
#else
		cpu_pause();
#endif
	}
}

void
panic_stackshot_release_lock(void)
{
	assert(!not_in_kdp);
	DebuggerUnlock();
}

static void
kdp_machine_reboot_type(unsigned int type, uint64_t debugger_flags)
{
	if ((type == kPEPanicRestartCPU) && (debugger_flags & DEBUGGER_OPTION_SKIP_PANICEND_CALLOUTS)) {
		PEHaltRestart(kPEPanicRestartCPUNoCallouts);
	} else {
		PEHaltRestart(type);
	}
	halt_all_cpus(TRUE);
}

void
kdp_machine_reboot(void)
{
	kdp_machine_reboot_type(kPEPanicRestartCPU, 0);
}

static __attribute__((unused)) void
panic_debugger_log(const char *string, ...)
{
	va_list panic_debugger_log_args;

	va_start(panic_debugger_log_args, string);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
	_doprnt(string, &panic_debugger_log_args, consdebug_putc, 16);
#pragma clang diagnostic pop
	va_end(panic_debugger_log_args);

#if defined(__arm64__)
	paniclog_flush();
#endif
}

/*
 * Gather and save diagnostic information about a panic (or Debugger call).
 *
 * On embedded, Debugger and Panic are treated very similarly -- WDT uses Debugger so we can
 * theoretically return from it. On desktop, Debugger is treated as a conventional debugger -- i.e no
 * paniclog is written and no core is written unless we request a core on NMI.
 *
 * This routine handles kicking off local coredumps, paniclogs, calling into the Debugger/KDP (if it's configured),
 * and calling out to any other functions we have for collecting diagnostic info.
 */
static void
debugger_collect_diagnostics(unsigned int exception, unsigned int code, unsigned int subcode, void *state)
{
#if DEVELOPMENT || DEBUG
	INJECT_NESTED_PANIC_IF_REQUESTED(PANIC_TEST_CASE_RECURPANIC_PRELOG);
#endif

#if defined(__x86_64__)
	kprintf("Debugger called: <%s>\n", debugger_message ? debugger_message : "");
#endif
	/*
	 * DB_HALT (halt_in_debugger) can be requested on startup, we shouldn't generate
	 * a coredump/paniclog for this type of debugger entry. If KDP isn't configured,
	 * we'll just spin in kdp_raise_exception.
	 */
	if (debugger_current_op == DBOP_DEBUGGER && halt_in_debugger) {
		kdp_raise_exception(exception, code, subcode, state);
		if (debugger_safe_to_return && !debugger_is_panic) {
			return;
		}
	}

#ifdef CONFIG_KCOV
	/* Try not to break core dump path by sanitizer. */
	kcov_panic_disable();
#endif

	if ((debugger_current_op == DBOP_PANIC) ||
	    ((debugger_current_op == DBOP_DEBUGGER) && debugger_is_panic)) {
		/*
		 * Attempt to notify listeners once and only once that we've started
		 * panicking. Only do this for Debugger() calls if we're treating
		 * Debugger() calls like panic().
		 */
		uint32_t panic_details = 0;
		/* if this is a force-reset panic then capture a log and reboot immediately. */
		if (debugger_panic_options & DEBUGGER_OPTION_PANICLOGANDREBOOT) {
			panic_details |= kPanicDetailsForcePowerOff;
		}
		PEHaltRestartInternal(kPEPanicBegin, panic_details);

		/*
		 * Set the begin pointer in the panic log structure. We key off of this
		 * static variable rather than contents from the panic header itself in case someone
		 * has stomped over the panic_info structure. Also initializes the header magic.
		 */
		static boolean_t began_writing_paniclog = FALSE;
		if (!began_writing_paniclog) {
			PE_init_panicheader();
			began_writing_paniclog = TRUE;
		}

		if (CPUDEBUGGERCOUNT > 1) {
			/*
			 * we are in a nested panic.  Record the nested bit in panic flags and do some housekeeping
			 */
			PE_update_panicheader_nestedpanic();
			paniclog_flush();
		}
	}

	/*
	 * Write panic string if this was a panic.
	 *
	 * TODO: Consider moving to SavePanicInfo as this is part of the panic log.
	 */
	if (debugger_current_op == DBOP_PANIC) {
		paniclog_append_noflush("panic(cpu %u caller 0x%lx): ", (unsigned) cpu_number(), debugger_panic_caller);
		if (debugger_panic_str) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
			_doprnt(debugger_panic_str, debugger_panic_args, consdebug_putc, 0);
#pragma clang diagnostic pop
		}
		paniclog_append_noflush("\n");
	}
#if defined(__x86_64__)
	else if (((debugger_current_op == DBOP_DEBUGGER) && debugger_is_panic)) {
		paniclog_append_noflush("Debugger called: <%s>\n", debugger_message ? debugger_message : "");
	}

	/*
	 * Debugger() is treated like panic() on embedded -- for example we use it for WDT
	 * panics (so we need to write a paniclog). On desktop Debugger() is used in the
	 * conventional sense.
	 */
	if (debugger_current_op == DBOP_PANIC || ((debugger_current_op == DBOP_DEBUGGER) && debugger_is_panic))
#endif /* __x86_64__ */
	{
		kdp_callouts(KDP_EVENT_PANICLOG);

		/*
		 * Write paniclog and panic stackshot (if supported)
		 * TODO: Need to clear panic log when return from debugger
		 * hooked up for embedded
		 */
		SavePanicInfo(debugger_message, debugger_panic_data, debugger_panic_options, debugger_panic_initiator);

#if DEVELOPMENT || DEBUG
		INJECT_NESTED_PANIC_IF_REQUESTED(PANIC_TEST_CASE_RECURPANIC_POSTLOG);
#endif

		/* DEBUGGER_OPTION_PANICLOGANDREBOOT is used for two finger resets on embedded so we get a paniclog */
		if (debugger_panic_options & DEBUGGER_OPTION_PANICLOGANDREBOOT) {
			PEHaltRestart(kPEPanicDiagnosticsDone);
			PEHaltRestart(kPEPanicRestartCPUNoCallouts);
		}
	}

#if CONFIG_KDP_INTERACTIVE_DEBUGGING
	/*
	 * If reboot on panic is enabled and the caller of panic indicated that we should skip
	 * local coredumps, don't try to write these and instead go straight to reboot. This
	 * allows us to persist any data that's stored in the panic log.
	 */
	if ((debugger_panic_options & DEBUGGER_OPTION_SKIP_LOCAL_COREDUMP) &&
	    (debug_boot_arg & DB_REBOOT_POST_CORE)) {
		PEHaltRestart(kPEPanicDiagnosticsDone);
		kdp_machine_reboot_type(kPEPanicRestartCPU, debugger_panic_options);
	}

	/*
	 * Consider generating a local corefile if the infrastructure is configured
	 * and we haven't disabled on-device coredumps.
	 */
	if (on_device_corefile_enabled()) {
#if CONFIG_SPTM
		/* We want to skip taking a local core dump if this is a panic from SPTM/TXM/cL4. */
		extern uint8_t sptm_supports_local_coredump;
		bool sptm_interrupted = false;
		pmap_sptm_percpu_data_t *sptm_pcpu = PERCPU_GET(pmap_sptm_percpu);
		sptm_get_cpu_state(sptm_pcpu->sptm_cpu_id, CPUSTATE_SPTM_INTERRUPTED, &sptm_interrupted);
#endif
		if (!kdp_has_polled_corefile()) {
			if (debug_boot_arg & (DB_KERN_DUMP_ON_PANIC | DB_KERN_DUMP_ON_NMI)) {
				paniclog_append_noflush("skipping local kernel core because core file could not be opened prior to panic (mode : 0x%x, error : 0x%x)\n",
				    kdp_polled_corefile_mode(), kdp_polled_corefile_error());
#if defined(__arm64__)
				if (kdp_polled_corefile_mode() == kIOPolledCoreFileModeUnlinked) {
					panic_info->eph_panic_flags |= EMBEDDED_PANIC_HEADER_FLAG_COREFILE_UNLINKED;
				}
				panic_info->eph_panic_flags |= EMBEDDED_PANIC_HEADER_FLAG_COREDUMP_FAILED;
				paniclog_flush();
#else /* defined(__arm64__) */
				if (panic_info->mph_panic_log_offset != 0) {
					if (kdp_polled_corefile_mode() == kIOPolledCoreFileModeUnlinked) {
						panic_info->mph_panic_flags |= MACOS_PANIC_HEADER_FLAG_COREFILE_UNLINKED;
					}
					panic_info->mph_panic_flags |= MACOS_PANIC_HEADER_FLAG_COREDUMP_FAILED;
					paniclog_flush();
				}
#endif /* defined(__arm64__) */
			}
		}
#if XNU_MONITOR
		else if (pmap_get_cpu_data()->ppl_state != PPL_STATE_KERNEL) {
			paniclog_append_noflush("skipping local kernel core because the PPL is not in KERNEL state\n");
			panic_info->eph_panic_flags |= EMBEDDED_PANIC_HEADER_FLAG_COREDUMP_FAILED;
			paniclog_flush();
		}
#elif CONFIG_SPTM
		else if (!sptm_supports_local_coredump) {
			paniclog_append_noflush("skipping local kernel core because the SPTM is in PANIC state and can't support core dump generation\n");
			panic_info->eph_panic_flags |= EMBEDDED_PANIC_HEADER_FLAG_COREDUMP_FAILED;
			paniclog_flush();
		} else if (sptm_interrupted) {
			paniclog_append_noflush("skipping local kernel core because the SPTM is in INTERRUPTED state and can't support core dump generation\n");
			panic_info->eph_panic_flags |= EMBEDDED_PANIC_HEADER_FLAG_COREDUMP_FAILED;
			paniclog_flush();
		}
#endif /* XNU_MONITOR */
		else {
			int ret = -1;

#if defined (__x86_64__)
			/* On x86 we don't do a coredump on Debugger unless the DB_KERN_DUMP_ON_NMI boot-arg is specified. */
			if (debugger_current_op != DBOP_DEBUGGER || (debug_boot_arg & DB_KERN_DUMP_ON_NMI))
#endif
			{
				/*
				 * Doing an on-device coredump leaves the disk driver in a state
				 * that can not be resumed.
				 */
				debugger_safe_to_return = FALSE;
				begin_panic_transfer();
				vm_memtag_disable_checking();
				ret = kern_dump(KERN_DUMP_DISK);
				vm_memtag_enable_checking();
				abort_panic_transfer();

#if DEVELOPMENT || DEBUG
				INJECT_NESTED_PANIC_IF_REQUESTED(PANIC_TEST_CASE_RECURPANIC_POSTCORE);
#endif
			}

			/*
			 * If DB_REBOOT_POST_CORE is set, then reboot if coredump is sucessfully saved
			 * or if option to ignore failures is set.
			 */
			if ((debug_boot_arg & DB_REBOOT_POST_CORE) &&
			    ((ret == 0) || (debugger_panic_options & DEBUGGER_OPTION_ATTEMPTCOREDUMPANDREBOOT))) {
				PEHaltRestart(kPEPanicDiagnosticsDone);
				kdp_machine_reboot_type(kPEPanicRestartCPU, debugger_panic_options);
			}
		}
	}

	if (debugger_current_op == DBOP_PANIC ||
	    ((debugger_current_op == DBOP_DEBUGGER) && debugger_is_panic)) {
		PEHaltRestart(kPEPanicDiagnosticsDone);
	}

	if (debug_boot_arg & DB_REBOOT_ALWAYS) {
		kdp_machine_reboot_type(kPEPanicRestartCPU, debugger_panic_options);
	}

	/* If KDP is configured, try to trap to the debugger */
#if defined(__arm64__)
	if (kdp_explicitly_requested && (current_debugger != NO_CUR_DB)) {
#else
	if (current_debugger != NO_CUR_DB) {
#endif
		kdp_raise_exception(exception, code, subcode, state);
		/*
		 * Only return if we entered via Debugger and it's safe to return
		 * (we halted the other cores successfully, this isn't a nested panic, etc)
		 */
		if (debugger_current_op == DBOP_DEBUGGER &&
		    debugger_safe_to_return &&
		    kernel_debugger_entry_count == 1 &&
		    !debugger_is_panic) {
			return;
		}
	}

#if defined(__arm64__)
	if (PE_i_can_has_debugger(NULL) && panicDebugging) {
		/*
		 * Print panic string at the end of serial output
		 * to make panic more obvious when someone connects a debugger
		 */
		if (debugger_panic_str) {
			panic_debugger_log("Original panic string:\n");
			panic_debugger_log("panic(cpu %u caller 0x%lx): ", (unsigned) cpu_number(), debugger_panic_caller);
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
			_doprnt(debugger_panic_str, debugger_panic_args, consdebug_putc, 0);
#pragma clang diagnostic pop
			panic_debugger_log("\n");
		}

		/* If panic debugging is configured and we're on a dev fused device, spin for astris to connect */
		panic_spin_shmcon();
	}
#endif /* defined(__arm64__) */

#else /* CONFIG_KDP_INTERACTIVE_DEBUGGING */

	PEHaltRestart(kPEPanicDiagnosticsDone);

#endif /* CONFIG_KDP_INTERACTIVE_DEBUGGING */

	if (!panicDebugging) {
		kdp_machine_reboot_type(kPEPanicRestartCPU, debugger_panic_options);
	}

	paniclog_append_noflush("\nPlease go to https://panic.apple.com to report this panic\n");
	panic_spin_forever();
}

#if SCHED_HYGIENE_DEBUG
uint64_t debugger_trap_timestamps[9];
# define DEBUGGER_TRAP_TIMESTAMP(i) debugger_trap_timestamps[i] = mach_absolute_time();
#else
# define DEBUGGER_TRAP_TIMESTAMP(i)
#endif /* SCHED_HYGIENE_DEBUG */

void
handle_debugger_trap(unsigned int exception, unsigned int code, unsigned int subcode, void *state)
{
	unsigned int initial_not_in_kdp = not_in_kdp;
	kern_return_t ret = KERN_SUCCESS;
	debugger_op db_prev_op = debugger_current_op;

	DEBUGGER_TRAP_TIMESTAMP(0);

	DebuggerLock();
	ret = DebuggerHaltOtherCores(CPUDEBUGGERSYNC, (CPUDEBUGGEROP == DBOP_STACKSHOT));

	DEBUGGER_TRAP_TIMESTAMP(1);

#if SCHED_HYGIENE_DEBUG
	if (serialmode & SERIALMODE_OUTPUT) {
		ml_spin_debug_reset(current_thread());
	}
#endif /* SCHED_HYGIENE_DEBUG */
	if (ret != KERN_SUCCESS) {
		CPUDEBUGGERRET = ret;
		DebuggerUnlock();
		return;
	}

	/* Update the global panic/debugger nested entry level */
	kernel_debugger_entry_count = CPUDEBUGGERCOUNT;
	if (kernel_debugger_entry_count > 0) {
		console_suspend();
	}

	/*
	 * TODO: Should we do anything special for nested panics here? i.e. if we've trapped more than twice
	 * should we call into the debugger if it's configured and then reboot if the panic log has been written?
	 */

	if (CPUDEBUGGEROP == DBOP_NONE) {
		/* If there was no debugger context setup, we trapped due to a software breakpoint */
		debugger_current_op = DBOP_BREAKPOINT;
	} else {
		/* Not safe to return from a nested panic/debugger call */
		if (debugger_current_op == DBOP_PANIC ||
		    debugger_current_op == DBOP_DEBUGGER) {
			debugger_safe_to_return = FALSE;
		}

		debugger_current_op = CPUDEBUGGEROP;

		/* Only overwrite the panic message if there is none already - save the data from the first call */
		if (debugger_panic_str == NULL) {
			debugger_panic_str = CPUPANICSTR;
			debugger_panic_args = CPUPANICARGS;
			debugger_panic_data = CPUPANICDATAPTR;
			debugger_message = CPUDEBUGGERMSG;
			debugger_panic_caller = CPUPANICCALLER;
			debugger_panic_initiator = CPUPANICINITIATOR;
		}

		debugger_panic_options = CPUPANICOPTS;
	}

	/*
	 * Clear the op from the processor debugger context so we can handle
	 * breakpoints in the debugger
	 */
	CPUDEBUGGEROP = DBOP_NONE;

	DEBUGGER_TRAP_TIMESTAMP(2);

	kdp_callouts(KDP_EVENT_ENTER);
	not_in_kdp = 0;

	DEBUGGER_TRAP_TIMESTAMP(3);

#if defined(__arm64__) && CONFIG_KDP_INTERACTIVE_DEBUGGING
	shmem_mark_as_busy();
#endif

	if (debugger_current_op == DBOP_BREAKPOINT) {
		kdp_raise_exception(exception, code, subcode, state);
	} else if (debugger_current_op == DBOP_STACKSHOT) {
		CPUDEBUGGERRET = do_stackshot(NULL);
#if PGO
	} else if (debugger_current_op == DBOP_RESET_PGO_COUNTERS) {
		CPUDEBUGGERRET = do_pgo_reset_counters();
#endif
	} else {
		/* note: this is the panic path...  */
#if CONFIG_SPTM
		/*
		 * Debug trap panics do not go through the standard panic flows so we
		 * have to notify the SPTM that we're going down now. This is not so
		 * much for security (critical cases are handled elsewhere) but rather
		 * to just keep the SPTM bit in sync with the actual XNU state.
		 */
		bool sptm_has_panicked = false;
		if (sptm_triggered_panic(&sptm_has_panicked) == LIBSPTM_SUCCESS &&
		    !sptm_has_panicked) {
			sptm_xnu_panic_begin();
		}
#endif /* CONFIG_SPTM */
#if defined(__arm64__) && (DEBUG || DEVELOPMENT)
		if (!PE_arm_debug_and_trace_initialized()) {
			paniclog_append_noflush("kernel panicked before debug and trace infrastructure initialized!\n"
			    "spinning forever...\n");
			panic_spin_forever();
		}
#endif
		debugger_collect_diagnostics(exception, code, subcode, state);
	}

#if defined(__arm64__) && CONFIG_KDP_INTERACTIVE_DEBUGGING
	shmem_unmark_as_busy();
#endif

	DEBUGGER_TRAP_TIMESTAMP(4);

	not_in_kdp = initial_not_in_kdp;
	kdp_callouts(KDP_EVENT_EXIT);

	DEBUGGER_TRAP_TIMESTAMP(5);

	if (debugger_current_op != DBOP_BREAKPOINT) {
		debugger_panic_str = NULL;
		debugger_panic_args = NULL;
		debugger_panic_data = NULL;
		debugger_panic_options = 0;
		debugger_message = NULL;
	}

	/* Restore the previous debugger state */
	debugger_current_op = db_prev_op;

	DEBUGGER_TRAP_TIMESTAMP(6);

	DebuggerResumeOtherCores();

	DEBUGGER_TRAP_TIMESTAMP(7);

	DebuggerUnlock();

	DEBUGGER_TRAP_TIMESTAMP(8);

	return;
}

__attribute__((noinline, not_tail_called))
void
log(__unused int level, char *fmt, ...)
{
	void *caller = __builtin_return_address(0);
	va_list listp;
	va_list listp2;


#ifdef lint
	level++;
#endif /* lint */
#ifdef  MACH_BSD
	va_start(listp, fmt);
	va_copy(listp2, listp);

	disable_preemption();
	_doprnt(fmt, &listp, cons_putc_locked, 0);
	enable_preemption();

	va_end(listp);

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat-nonliteral"
	os_log_with_args(OS_LOG_DEFAULT, OS_LOG_TYPE_DEFAULT, fmt, listp2, caller);
#pragma clang diagnostic pop
	va_end(listp2);
#endif
}

/*
 * Per <rdar://problem/24974766>, skip appending log messages to
 * the new logging infrastructure in contexts where safety is
 * uncertain. These contexts include:
 *   - When we're in the debugger
 *   - We're in a panic
 *   - Interrupts are disabled
 *   - Or Pre-emption is disabled
 * In all the above cases, it is potentially unsafe to log messages.
 */

boolean_t
oslog_is_safe(void)
{
	return kernel_debugger_entry_count == 0 &&
	       not_in_kdp == 1 &&
	       get_preemption_level() == 0 &&
	       ml_get_interrupts_enabled() == TRUE;
}

boolean_t
debug_mode_active(void)
{
	return (0 != kernel_debugger_entry_count != 0) || (0 == not_in_kdp);
}

void
debug_putc(char c)
{
	if ((debug_buf_size != 0) &&
	    ((debug_buf_ptr - debug_buf_base) < (int)debug_buf_size) &&
	    (!is_debug_ptr_in_ext_paniclog())) {
		*debug_buf_ptr = c;
		debug_buf_ptr++;
	}
}

#if defined (__x86_64__)
struct pasc {
	unsigned a: 7;
	unsigned b: 7;
	unsigned c: 7;
	unsigned d: 7;
	unsigned e: 7;
	unsigned f: 7;
	unsigned g: 7;
	unsigned h: 7;
}  __attribute__((packed));

typedef struct pasc pasc_t;

/*
 * In-place packing routines -- inefficient, but they're called at most once.
 * Assumes "buflen" is a multiple of 8. Used for compressing paniclogs on x86.
 */
int
packA(char *inbuf, uint32_t length, uint32_t buflen)
{
	unsigned int i, j = 0;
	pasc_t pack;

	length = MIN(((length + 7) & ~7), buflen);

	for (i = 0; i < length; i += 8) {
		pack.a = inbuf[i];
		pack.b = inbuf[i + 1];
		pack.c = inbuf[i + 2];
		pack.d = inbuf[i + 3];
		pack.e = inbuf[i + 4];
		pack.f = inbuf[i + 5];
		pack.g = inbuf[i + 6];
		pack.h = inbuf[i + 7];
		bcopy((char *) &pack, inbuf + j, 7);
		j += 7;
	}
	return j;
}

void
unpackA(char *inbuf, uint32_t length)
{
	pasc_t packs;
	unsigned i = 0;
	length = (length * 8) / 7;

	while (i < length) {
		packs = *(pasc_t *)&inbuf[i];
		bcopy(&inbuf[i + 7], &inbuf[i + 8], MAX(0, (int) (length - i - 8)));
		inbuf[i++] = packs.a;
		inbuf[i++] = packs.b;
		inbuf[i++] = packs.c;
		inbuf[i++] = packs.d;
		inbuf[i++] = packs.e;
		inbuf[i++] = packs.f;
		inbuf[i++] = packs.g;
		inbuf[i++] = packs.h;
	}
}
#endif /* defined (__x86_64__) */

extern char *proc_name_address(void *);
extern char *proc_longname_address(void *);

__private_extern__ void
panic_display_process_name(void)
{
	proc_name_t proc_name = {};
	struct proc *cbsd_info = NULL;
	task_t ctask = NULL;
	vm_size_t size;

	if (!panic_get_thread_proc_task(current_thread(), &ctask, &cbsd_info)) {
		goto out;
	}

	if (cbsd_info == NULL) {
		goto out;
	}

	size = ml_nofault_copy((vm_offset_t)proc_longname_address(cbsd_info),
	    (vm_offset_t)&proc_name, sizeof(proc_name));

	if (size == 0 || proc_name[0] == '\0') {
		size = ml_nofault_copy((vm_offset_t)proc_name_address(cbsd_info),
		    (vm_offset_t)&proc_name,
		    MIN(sizeof(command_t), sizeof(proc_name)));
		if (size > 0) {
			proc_name[size - 1] = '\0';
		}
	}

out:
	proc_name[sizeof(proc_name) - 1] = '\0';
	paniclog_append_noflush("\nProcess name corresponding to current thread (%p): %s\n",
	    current_thread(), proc_name[0] != '\0' ? proc_name : "Unknown");
}

unsigned
panic_active(void)
{
	return debugger_current_op == DBOP_PANIC ||
	       (debugger_current_op == DBOP_DEBUGGER && debugger_is_panic);
}

void
populate_model_name(char *model_string)
{
	strlcpy(model_name, model_string, sizeof(model_name));
}

void
panic_display_model_name(void)
{
	char tmp_model_name[sizeof(model_name)];

	if (ml_nofault_copy((vm_offset_t) &model_name, (vm_offset_t) &tmp_model_name, sizeof(model_name)) != sizeof(model_name)) {
		return;
	}

	tmp_model_name[sizeof(tmp_model_name) - 1] = '\0';

	if (tmp_model_name[0] != 0) {
		paniclog_append_noflush("System model name: %s\n", tmp_model_name);
	}
}

void
panic_display_kernel_uuid(void)
{
	char tmp_kernel_uuid[sizeof(kernel_uuid_string)];

	if (ml_nofault_copy((vm_offset_t) &kernel_uuid_string, (vm_offset_t) &tmp_kernel_uuid, sizeof(kernel_uuid_string)) != sizeof(kernel_uuid_string)) {
		return;
	}

	if (tmp_kernel_uuid[0] != '\0') {
		paniclog_append_noflush("Kernel UUID: %s\n", tmp_kernel_uuid);
	}
}

#if CONFIG_SPTM
static void
panic_display_component_uuid(char const *component_name, void *component_address)
{
	uuid_t *component_uuid;
	unsigned long component_uuid_len = 0;
	uuid_string_t component_uuid_string;

	component_uuid = getuuidfromheader((kernel_mach_header_t *)component_address, &component_uuid_len);

	if (component_uuid != NULL && component_uuid_len == sizeof(uuid_t)) {
		uuid_unparse_upper(*component_uuid, component_uuid_string);
		paniclog_append_noflush("%s UUID: %s\n", component_name, component_uuid_string);
	}
}
#endif /* CONFIG_SPTM */

void
panic_display_kernel_aslr(void)
{
#if CONFIG_SPTM
	{
		struct debug_header const *dh = SPTMArgs->debug_header;

		paniclog_append_noflush("Debug Header address: %p\n", dh);

		if (dh != NULL) {
			void *component_address;

			paniclog_append_noflush("Debug Header entry count: %d\n", dh->count);

			switch (dh->count) {
			default: // 3 or more
				component_address = dh->image[DEBUG_HEADER_ENTRY_TXM];
				paniclog_append_noflush("TXM load address: %p\n", component_address);

				panic_display_component_uuid("TXM", component_address);
				OS_FALLTHROUGH;
			case 2:
				component_address = dh->image[DEBUG_HEADER_ENTRY_XNU];
				paniclog_append_noflush("Debug Header kernelcache load address: %p\n", component_address);

				panic_display_component_uuid("Debug Header kernelcache", component_address);
				OS_FALLTHROUGH;
			case 1:
				component_address = dh->image[DEBUG_HEADER_ENTRY_SPTM];
				paniclog_append_noflush("SPTM load address: %p\n", component_address);

				panic_display_component_uuid("SPTM", component_address);
				OS_FALLTHROUGH;
			case 0:
				; // nothing to print
			}
		}
	}
#endif /* CONFIG_SPTM */

	kc_format_t kc_format;

	PE_get_primary_kc_format(&kc_format);

	if (kc_format == KCFormatFileset) {
		void *kch = PE_get_kc_header(KCKindPrimary);
		paniclog_append_noflush("KernelCache slide: 0x%016lx\n", (unsigned long) vm_kernel_slide);
		paniclog_append_noflush("KernelCache base:  %p\n", (void*) kch);
		paniclog_append_noflush("Kernel slide:      0x%016lx\n", vm_kernel_stext - (unsigned long)kch + vm_kernel_slide);
		paniclog_append_noflush("Kernel text base:  %p\n", (void *) vm_kernel_stext);
#if defined(__arm64__)
		extern vm_offset_t segTEXTEXECB;
		paniclog_append_noflush("Kernel text exec slide: 0x%016lx\n", (unsigned long)segTEXTEXECB - (unsigned long)kch + vm_kernel_slide);
		paniclog_append_noflush("Kernel text exec base:  0x%016lx\n", (unsigned long)segTEXTEXECB);
#endif /* defined(__arm64__) */
	} else if (vm_kernel_slide) {
		paniclog_append_noflush("Kernel slide:      0x%016lx\n", (unsigned long) vm_kernel_slide);
		paniclog_append_noflush("Kernel text base:  %p\n", (void *)vm_kernel_stext);
	} else {
		paniclog_append_noflush("Kernel text base:  %p\n", (void *)vm_kernel_stext);
	}
}

void
panic_display_hibb(void)
{
#if defined(__i386__) || defined (__x86_64__)
	paniclog_append_noflush("__HIB  text base: %p\n", (void *) vm_hib_base);
#endif
}

#if CONFIG_ECC_LOGGING
__private_extern__ void
panic_display_ecc_errors(void)
{
	uint32_t count = ecc_log_get_correction_count();

	if (count > 0) {
		paniclog_append_noflush("ECC Corrections:%u\n", count);
	}
}
#endif /* CONFIG_ECC_LOGGING */

#if CONFIG_FREEZE
extern bool freezer_incore_cseg_acct;
extern int32_t c_segment_pages_compressed_incore;
#endif

extern uint32_t c_segment_pages_compressed;
extern uint32_t c_segment_count;
extern uint32_t c_segments_limit;
extern uint32_t c_segment_pages_compressed_limit;
extern uint32_t c_segment_pages_compressed_nearing_limit;
extern uint32_t c_segments_nearing_limit;
extern int vm_num_swap_files;

void
panic_display_compressor_stats(void)
{
	int isswaplow = vm_swap_low_on_space();
#if CONFIG_FREEZE
	uint32_t incore_seg_count;
	uint32_t incore_compressed_pages;
	if (freezer_incore_cseg_acct) {
		incore_seg_count = c_segment_count - c_swappedout_count - c_swappedout_sparse_count;
		incore_compressed_pages = c_segment_pages_compressed_incore;
	} else {
		incore_seg_count = c_segment_count;
		incore_compressed_pages = c_segment_pages_compressed;
	}

	paniclog_append_noflush("Compressor Info: %u%% of compressed pages limit (%s) and %u%% of segments limit (%s) with %d swapfiles and %s swap space\n",
	    (incore_compressed_pages * 100) / c_segment_pages_compressed_limit,
	    (incore_compressed_pages > c_segment_pages_compressed_nearing_limit) ? "BAD":"OK",
	    (incore_seg_count * 100) / c_segments_limit,
	    (incore_seg_count > c_segments_nearing_limit) ? "BAD":"OK",
	    vm_num_swap_files,
	    isswaplow ? "LOW":"OK");
#else /* CONFIG_FREEZE */
	paniclog_append_noflush("Compressor Info: %u%% of compressed pages limit (%s) and %u%% of segments limit (%s) with %d swapfiles and %s swap space\n",
	    (c_segment_pages_compressed * 100) / c_segment_pages_compressed_limit,
	    (c_segment_pages_compressed > c_segment_pages_compressed_nearing_limit) ? "BAD":"OK",
	    (c_segment_count * 100) / c_segments_limit,
	    (c_segment_count > c_segments_nearing_limit) ? "BAD":"OK",
	    vm_num_swap_files,
	    isswaplow ? "LOW":"OK");
#endif /* CONFIG_FREEZE */
}

#if !CONFIG_TELEMETRY
int
telemetry_gather(user_addr_t buffer __unused, uint32_t *length __unused, bool mark __unused)
{
	return KERN_NOT_SUPPORTED;
}
#endif

#include <machine/machine_cpu.h>

TUNABLE(uint32_t, kern_feature_overrides, "validation_disables", 0);

boolean_t
kern_feature_override(uint32_t fmask)
{
	return (kern_feature_overrides & fmask) == fmask;
}

#if !XNU_TARGET_OS_OSX & CONFIG_KDP_INTERACTIVE_DEBUGGING
static boolean_t
device_corefile_valid_on_ephemeral(void)
{
#ifdef CONFIG_KDP_COREDUMP_ENCRYPTION
	DTEntry node;
	const uint32_t *value = NULL;
	unsigned int size = 0;
	if (kSuccess != SecureDTLookupEntry(NULL, "/product", &node)) {
		return TRUE;
	}
	if (kSuccess != SecureDTGetProperty(node, "ephemeral-data-mode", (void const **) &value, &size)) {
		return TRUE;
	}

	if (size != sizeof(uint32_t)) {
		return TRUE;
	}

	if ((*value) && (kern_dump_should_enforce_encryption() == true)) {
		return FALSE;
	}
#endif /* ifdef CONFIG_KDP_COREDUMP_ENCRYPTION */

	return TRUE;
}
#endif /* !XNU_TARGET_OS_OSX & CONFIG_KDP_INTERACTIVE_DEBUGGING */

boolean_t
on_device_corefile_enabled(void)
{
	assert(startup_phase >= STARTUP_SUB_TUNABLES);
#if CONFIG_KDP_INTERACTIVE_DEBUGGING
	if (debug_boot_arg == 0) {
		return FALSE;
	}
	if (debug_boot_arg & DB_DISABLE_LOCAL_CORE) {
		return FALSE;
	}
#if !XNU_TARGET_OS_OSX
	if (device_corefile_valid_on_ephemeral() == FALSE) {
		return FALSE;
	}
	/*
	 * outside of macOS, if there's a debug boot-arg set and local
	 * cores aren't explicitly disabled, we always write a corefile.
	 */
	return TRUE;
#else /* !XNU_TARGET_OS_OSX */
	/*
	 * on macOS, if corefiles on panic are requested and local cores
	 * aren't disabled we write a local core.
	 */
	if (debug_boot_arg & (DB_KERN_DUMP_ON_NMI | DB_KERN_DUMP_ON_PANIC)) {
		return TRUE;
	}
#endif /* !XNU_TARGET_OS_OSX */
#endif /* CONFIG_KDP_INTERACTIVE_DEBUGGING */
	return FALSE;
}

boolean_t
panic_stackshot_to_disk_enabled(void)
{
	assert(startup_phase >= STARTUP_SUB_TUNABLES);
#if defined(__x86_64__)
	if (PEGetCoprocessorVersion() < kCoprocessorVersion2) {
		/* Only enabled on pre-Gibraltar machines where it hasn't been disabled explicitly */
		if ((debug_boot_arg != 0) && (debug_boot_arg & DB_DISABLE_STACKSHOT_TO_DISK)) {
			return FALSE;
		}

		return TRUE;
	}
#endif
	return FALSE;
}

const char *
sysctl_debug_get_preoslog(size_t *size)
{
	int result = 0;
	void *preoslog_pa = NULL;
	int preoslog_size = 0;

	result = IODTGetLoaderInfo("preoslog", &preoslog_pa, &preoslog_size);
	if (result || preoslog_pa == NULL || preoslog_size == 0) {
		kprintf("Couldn't obtain preoslog region: result = %d, preoslog_pa = %p, preoslog_size = %d\n", result, preoslog_pa, preoslog_size);
		*size = 0;
		return NULL;
	}

	/*
	 *  Beware:
	 *  On release builds, we would need to call IODTFreeLoaderInfo("preoslog", preoslog_pa, preoslog_size) to free the preoslog buffer.
	 *  On Development & Debug builds, we retain the buffer so it can be extracted from coredumps.
	 */
	*size = preoslog_size;
	return (char *)(ml_static_ptovirt((vm_offset_t)(preoslog_pa)));
}

void
sysctl_debug_free_preoslog(void)
{
#if RELEASE
	int result = 0;
	void *preoslog_pa = NULL;
	int preoslog_size = 0;

	result = IODTGetLoaderInfo("preoslog", &preoslog_pa, &preoslog_size);
	if (result || preoslog_pa == NULL || preoslog_size == 0) {
		kprintf("Couldn't obtain preoslog region: result = %d, preoslog_pa = %p, preoslog_size = %d\n", result, preoslog_pa, preoslog_size);
		return;
	}

	IODTFreeLoaderInfo("preoslog", preoslog_pa, preoslog_size);
#else
	/*  On Development & Debug builds, we retain the buffer so it can be extracted from coredumps. */
#endif // RELEASE
}


#if (DEVELOPMENT || DEBUG)

void
platform_stall_panic_or_spin(uint32_t req)
{
	if (xnu_platform_stall_value & req) {
		if (xnu_platform_stall_value & PLATFORM_STALL_XNU_ACTION_PANIC) {
			panic("Platform stall: User requested panic");
		} else {
			paniclog_append_noflush("\nUser requested platform stall. Stall Code: 0x%x", req);
			panic_spin_forever();
		}
	}
}
#endif


#define AWL_HV_ENTRY_FLAG (0x1)

static inline void
awl_set_scratch_reg_hv_bit(void)
{
#if defined(__arm64__)
#define WATCHDOG_DIAG0     "S3_5_c15_c2_6"
	uint64_t awl_diag0 = __builtin_arm_rsr64(WATCHDOG_DIAG0);
	awl_diag0 |= AWL_HV_ENTRY_FLAG;
	__builtin_arm_wsr64(WATCHDOG_DIAG0, awl_diag0);
#endif // defined(__arm64__)
}

void
awl_mark_hv_entry(void)
{
	if (__probable(*PERCPU_GET(hv_entry_detected) || !awl_scratch_reg_supported)) {
		return;
	}
	*PERCPU_GET(hv_entry_detected) = true;

	awl_set_scratch_reg_hv_bit();
}

/*
 * Awl WatchdogDiag0 is not restored by hardware when coming out of reset,
 * so restore it manually.
 */
static bool
awl_pm_state_change_cbk(void *param __unused, enum cpu_event event, unsigned int cpu_or_cluster __unused)
{
	if (event == CPU_BOOTED) {
		if (*PERCPU_GET(hv_entry_detected)) {
			awl_set_scratch_reg_hv_bit();
		}
	}

	return true;
}

/*
 * Identifies and sets a flag if AWL Scratch0/1 exists in the system, subscribes
 * for a callback to restore register after hibernation
 */
__startup_func
static void
set_awl_scratch_exists_flag_and_subscribe_for_pm(void)
{
	DTEntry base = NULL;

	if (SecureDTLookupEntry(NULL, "/arm-io/wdt", &base) != kSuccess) {
		return;
	}
	const uint8_t *data = NULL;
	unsigned int data_size = sizeof(uint8_t);

	if (base != NULL && SecureDTGetProperty(base, "awl-scratch-supported", (const void **)&data, &data_size) == kSuccess) {
		for (unsigned int i = 0; i < data_size; i++) {
			if (data[i] != 0) {
				awl_scratch_reg_supported = true;
				cpu_event_register_callback(awl_pm_state_change_cbk, NULL);
				break;
			}
		}
	}
}
STARTUP(EARLY_BOOT, STARTUP_RANK_MIDDLE, set_awl_scratch_exists_flag_and_subscribe_for_pm);
