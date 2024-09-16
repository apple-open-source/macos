/*
 * Copyright (c) 2000-2009 Apple Inc. All rights reserved.
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
 * Copyright (c) 1991,1990,1989,1988 Carnegie Mellon University
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
 */

/*
 *	host.c
 *
 *	Non-ipc host functions.
 */

#include <mach/mach_types.h>
#include <mach/boolean.h>
#include <mach/host_info.h>
#include <mach/host_special_ports.h>
#include <mach/kern_return.h>
#include <mach/machine.h>
#include <mach/port.h>
#include <mach/processor_info.h>
#include <mach/vm_param.h>
#include <mach/processor.h>
#include <mach/mach_host_server.h>
#include <mach/host_priv_server.h>
#include <mach/vm_map.h>
#include <mach/task_info.h>

#include <machine/commpage.h>
#include <machine/cpu_capabilities.h>

#include <device/device_port.h>

#include <kern/kern_types.h>
#include <kern/assert.h>
#include <kern/kalloc.h>
#include <kern/ecc.h>
#include <kern/host.h>
#include <kern/host_statistics.h>
#include <kern/ipc_host.h>
#include <kern/misc_protos.h>
#include <kern/sched.h>
#include <kern/processor.h>
#include <kern/mach_node.h>     // mach_node_port_changed()

#include <vm/vm_map_xnu.h>
#include <vm/vm_purgeable_xnu.h>
#include <vm/vm_pageout.h>
#include <vm/vm_kern_xnu.h>

#include <IOKit/IOBSD.h> // IOTaskHasEntitlement
#include <IOKit/IOKitKeys.h> // DriverKit entitlement strings


#if CONFIG_ATM
#include <atm/atm_internal.h>
#endif

#if CONFIG_MACF
#include <security/mac_mach_internal.h>
#endif

#if CONFIG_CSR
#include <sys/csr.h>
#endif

#include <pexpert/pexpert.h>

SCALABLE_COUNTER_DEFINE(vm_statistics_zero_fill_count);        /* # of zero fill pages */
SCALABLE_COUNTER_DEFINE(vm_statistics_reactivations);          /* # of pages reactivated */
SCALABLE_COUNTER_DEFINE(vm_statistics_pageins);                /* # of pageins */
SCALABLE_COUNTER_DEFINE(vm_statistics_pageouts);               /* # of pageouts */
SCALABLE_COUNTER_DEFINE(vm_statistics_faults);                 /* # of faults */
SCALABLE_COUNTER_DEFINE(vm_statistics_cow_faults);             /* # of copy-on-writes */
SCALABLE_COUNTER_DEFINE(vm_statistics_lookups);                /* object cache lookups */
SCALABLE_COUNTER_DEFINE(vm_statistics_hits);                   /* object cache hits */
SCALABLE_COUNTER_DEFINE(vm_statistics_purges);                 /* # of pages purged */
SCALABLE_COUNTER_DEFINE(vm_statistics_decompressions);         /* # of pages decompressed */
SCALABLE_COUNTER_DEFINE(vm_statistics_compressions);           /* # of pages compressed */
SCALABLE_COUNTER_DEFINE(vm_statistics_swapins);                /* # of pages swapped in (via compression segments) */
SCALABLE_COUNTER_DEFINE(vm_statistics_swapouts);               /* # of pages swapped out (via compression segments) */
SCALABLE_COUNTER_DEFINE(vm_statistics_total_uncompressed_pages_in_compressor); /* # of pages (uncompressed) held within the compressor. */
SCALABLE_COUNTER_DEFINE(vm_page_grab_count);

host_data_t realhost;

static void
get_host_vm_stats(vm_statistics64_t out)
{
	out->zero_fill_count = counter_load(&vm_statistics_zero_fill_count);
	out->reactivations = counter_load(&vm_statistics_reactivations);
	out->pageins = counter_load(&vm_statistics_pageins);
	out->pageouts = counter_load(&vm_statistics_pageouts);
	out->faults = counter_load(&vm_statistics_faults);
	out->cow_faults = counter_load(&vm_statistics_cow_faults);
	out->lookups = counter_load(&vm_statistics_lookups);
	out->hits = counter_load(&vm_statistics_hits);
	out->compressions = counter_load(&vm_statistics_compressions);
	out->decompressions = counter_load(&vm_statistics_decompressions);
	out->swapins = counter_load(&vm_statistics_swapins);
	out->swapouts = counter_load(&vm_statistics_swapouts);
}
vm_extmod_statistics_data_t host_extmod_statistics;

kern_return_t
host_processors(host_priv_t host_priv, processor_array_t * out_array, mach_msg_type_number_t * countp)
{
	if (host_priv == HOST_PRIV_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	unsigned int count = processor_count;
	assert(count != 0);

	static_assert(sizeof(mach_port_t) == sizeof(processor_t));

	mach_port_array_t ports = mach_port_array_alloc(count, Z_WAITOK);
	if (!ports) {
		return KERN_RESOURCE_SHORTAGE;
	}

	for (unsigned int i = 0; i < count; i++) {
		processor_t processor = processor_array[i];
		assert(processor != PROCESSOR_NULL);

		/* do the conversion that Mig should handle */
		ports[i].port = convert_processor_to_port(processor);
	}

	*countp = count;
	*out_array = ports;

	return KERN_SUCCESS;
}

extern int sched_allow_NO_SMT_threads;

kern_return_t
host_info(host_t host, host_flavor_t flavor, host_info_t info, mach_msg_type_number_t * count)
{
	if (host == HOST_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	switch (flavor) {
	case HOST_BASIC_INFO: {
		host_basic_info_t basic_info;
		int master_id = master_processor->cpu_id;

		/*
		 *	Basic information about this host.
		 */
		if (*count < HOST_BASIC_INFO_OLD_COUNT) {
			return KERN_FAILURE;
		}

		basic_info = (host_basic_info_t)info;

		basic_info->memory_size = machine_info.memory_size;
		basic_info->cpu_type = slot_type(master_id);
		basic_info->cpu_subtype = slot_subtype(master_id);
		basic_info->max_cpus = machine_info.max_cpus;
#if defined(__x86_64__)
		if (sched_allow_NO_SMT_threads && current_task()->t_flags & TF_NO_SMT) {
			basic_info->avail_cpus = primary_processor_avail_count_user;
		} else {
			basic_info->avail_cpus = processor_avail_count_user;
		}
#else
		basic_info->avail_cpus = processor_avail_count;
#endif


		if (*count >= HOST_BASIC_INFO_COUNT) {
			basic_info->cpu_threadtype = slot_threadtype(master_id);
			basic_info->physical_cpu = machine_info.physical_cpu;
			basic_info->physical_cpu_max = machine_info.physical_cpu_max;
#if defined(__x86_64__)
			basic_info->logical_cpu = basic_info->avail_cpus;
#else
			basic_info->logical_cpu = machine_info.logical_cpu;
#endif
			basic_info->logical_cpu_max = machine_info.logical_cpu_max;
			basic_info->max_mem = machine_info.max_mem;

			*count = HOST_BASIC_INFO_COUNT;
		} else {
			*count = HOST_BASIC_INFO_OLD_COUNT;
		}

		return KERN_SUCCESS;
	}

	case HOST_SCHED_INFO: {
		host_sched_info_t sched_info;
		uint32_t quantum_time;
		uint64_t quantum_ns;

		/*
		 *	Return scheduler information.
		 */
		if (*count < HOST_SCHED_INFO_COUNT) {
			return KERN_FAILURE;
		}

		sched_info = (host_sched_info_t)info;

		quantum_time = SCHED(initial_quantum_size)(THREAD_NULL);
		absolutetime_to_nanoseconds(quantum_time, &quantum_ns);

		sched_info->min_timeout = sched_info->min_quantum = (uint32_t)(quantum_ns / 1000 / 1000);

		*count = HOST_SCHED_INFO_COUNT;

		return KERN_SUCCESS;
	}

	case HOST_RESOURCE_SIZES: {
		/*
		 * Return sizes of kernel data structures
		 */
		if (*count < HOST_RESOURCE_SIZES_COUNT) {
			return KERN_FAILURE;
		}

		/* XXX Fail until ledgers are implemented */
		return KERN_INVALID_ARGUMENT;
	}

	case HOST_PRIORITY_INFO: {
		host_priority_info_t priority_info;

		if (*count < HOST_PRIORITY_INFO_COUNT) {
			return KERN_FAILURE;
		}

		priority_info = (host_priority_info_t)info;

		priority_info->kernel_priority = MINPRI_KERNEL;
		priority_info->system_priority = MINPRI_KERNEL;
		priority_info->server_priority = MINPRI_RESERVED;
		priority_info->user_priority = BASEPRI_DEFAULT;
		priority_info->depress_priority = DEPRESSPRI;
		priority_info->idle_priority = IDLEPRI;
		priority_info->minimum_priority = MINPRI_USER;
		priority_info->maximum_priority = MAXPRI_RESERVED;

		*count = HOST_PRIORITY_INFO_COUNT;

		return KERN_SUCCESS;
	}

	/*
	 * Gestalt for various trap facilities.
	 */
	case HOST_MACH_MSG_TRAP:
	case HOST_SEMAPHORE_TRAPS: {
		*count = 0;
		return KERN_SUCCESS;
	}

	case HOST_CAN_HAS_DEBUGGER: {
		host_can_has_debugger_info_t can_has_debugger_info;

		if (*count < HOST_CAN_HAS_DEBUGGER_COUNT) {
			return KERN_FAILURE;
		}

		can_has_debugger_info = (host_can_has_debugger_info_t)info;
		can_has_debugger_info->can_has_debugger = PE_i_can_has_debugger(NULL);
		*count = HOST_CAN_HAS_DEBUGGER_COUNT;

		return KERN_SUCCESS;
	}

	case HOST_VM_PURGABLE: {
		if (*count < HOST_VM_PURGABLE_COUNT) {
			return KERN_FAILURE;
		}

		vm_purgeable_stats((vm_purgeable_info_t)info, NULL);

		*count = HOST_VM_PURGABLE_COUNT;
		return KERN_SUCCESS;
	}

	case HOST_DEBUG_INFO_INTERNAL: {
#if DEVELOPMENT || DEBUG
		if (*count < HOST_DEBUG_INFO_INTERNAL_COUNT) {
			return KERN_FAILURE;
		}

		host_debug_info_internal_t debug_info = (host_debug_info_internal_t)info;
		bzero(debug_info, sizeof(host_debug_info_internal_data_t));
		*count = HOST_DEBUG_INFO_INTERNAL_COUNT;

#if CONFIG_COALITIONS
		debug_info->config_coalitions = 1;
#endif
		debug_info->config_bank = 1;
#if CONFIG_ATM
		debug_info->config_atm = 1;
#endif
#if CONFIG_CSR
		debug_info->config_csr = 1;
#endif
		return KERN_SUCCESS;
#else /* DEVELOPMENT || DEBUG */
		return KERN_NOT_SUPPORTED;
#endif
	}

	case HOST_PREFERRED_USER_ARCH: {
		host_preferred_user_arch_t user_arch_info;

		/*
		 *	Basic information about this host.
		 */
		if (*count < HOST_PREFERRED_USER_ARCH_COUNT) {
			return KERN_FAILURE;
		}

		user_arch_info = (host_preferred_user_arch_t)info;

#if defined(PREFERRED_USER_CPU_TYPE) && defined(PREFERRED_USER_CPU_SUBTYPE)
		cpu_type_t preferred_cpu_type;
		cpu_subtype_t preferred_cpu_subtype;
		if (!PE_get_default("kern.preferred_cpu_type", &preferred_cpu_type, sizeof(cpu_type_t))) {
			preferred_cpu_type = PREFERRED_USER_CPU_TYPE;
		}
		if (!PE_get_default("kern.preferred_cpu_subtype", &preferred_cpu_subtype, sizeof(cpu_subtype_t))) {
			preferred_cpu_subtype = PREFERRED_USER_CPU_SUBTYPE;
		}
		user_arch_info->cpu_type    = preferred_cpu_type;
		user_arch_info->cpu_subtype = preferred_cpu_subtype;
#else
		int master_id               = master_processor->cpu_id;
		user_arch_info->cpu_type    = slot_type(master_id);
		user_arch_info->cpu_subtype = slot_subtype(master_id);
#endif


		*count = HOST_PREFERRED_USER_ARCH_COUNT;

		return KERN_SUCCESS;
	}

	default: return KERN_INVALID_ARGUMENT;
	}
}

kern_return_t host_statistics(host_t host, host_flavor_t flavor, host_info_t info, mach_msg_type_number_t * count);

kern_return_t
host_statistics(host_t host, host_flavor_t flavor, host_info_t info, mach_msg_type_number_t * count)
{
	if (host == HOST_NULL) {
		return KERN_INVALID_HOST;
	}

	switch (flavor) {
	case HOST_LOAD_INFO: {
		host_load_info_t load_info;

		if (*count < HOST_LOAD_INFO_COUNT) {
			return KERN_FAILURE;
		}

		load_info = (host_load_info_t)info;

		bcopy((char *)avenrun, (char *)load_info->avenrun, sizeof avenrun);
		bcopy((char *)mach_factor, (char *)load_info->mach_factor, sizeof mach_factor);

		*count = HOST_LOAD_INFO_COUNT;
		return KERN_SUCCESS;
	}

	case HOST_VM_INFO: {
		vm_statistics64_data_t host_vm_stat;
		vm_statistics_t stat32;
		mach_msg_type_number_t original_count;
		natural_t speculative_count = vm_page_speculative_count;

		if (*count < HOST_VM_INFO_REV0_COUNT) {
			return KERN_FAILURE;
		}

		get_host_vm_stats(&host_vm_stat);

		stat32 = (vm_statistics_t)info;

		stat32->free_count = VM_STATISTICS_TRUNCATE_TO_32_BIT(vm_page_free_count + speculative_count);
		stat32->active_count = VM_STATISTICS_TRUNCATE_TO_32_BIT(vm_page_active_count);

		if (vm_page_local_q) {
			zpercpu_foreach(lq, vm_page_local_q) {
				stat32->active_count += VM_STATISTICS_TRUNCATE_TO_32_BIT(lq->vpl_count);
			}
		}
		stat32->inactive_count = VM_STATISTICS_TRUNCATE_TO_32_BIT(vm_page_inactive_count);
#if !XNU_TARGET_OS_OSX
		stat32->wire_count = VM_STATISTICS_TRUNCATE_TO_32_BIT(vm_page_wire_count);
#else /* !XNU_TARGET_OS_OSX */
		stat32->wire_count = VM_STATISTICS_TRUNCATE_TO_32_BIT(vm_page_wire_count + vm_page_throttled_count + vm_lopage_free_count);
#endif /* !XNU_TARGET_OS_OSX */
		stat32->zero_fill_count = VM_STATISTICS_TRUNCATE_TO_32_BIT(host_vm_stat.zero_fill_count);
		stat32->reactivations = VM_STATISTICS_TRUNCATE_TO_32_BIT(host_vm_stat.reactivations);
		stat32->pageins = VM_STATISTICS_TRUNCATE_TO_32_BIT(host_vm_stat.pageins);
		stat32->pageouts = VM_STATISTICS_TRUNCATE_TO_32_BIT(host_vm_stat.pageouts);
		stat32->faults = VM_STATISTICS_TRUNCATE_TO_32_BIT(host_vm_stat.faults);
		stat32->cow_faults = VM_STATISTICS_TRUNCATE_TO_32_BIT(host_vm_stat.cow_faults);
		stat32->lookups = VM_STATISTICS_TRUNCATE_TO_32_BIT(host_vm_stat.lookups);
		stat32->hits = VM_STATISTICS_TRUNCATE_TO_32_BIT(host_vm_stat.hits);

		/*
		 * Fill in extra info added in later revisions of the
		 * vm_statistics data structure.  Fill in only what can fit
		 * in the data structure the caller gave us !
		 */
		original_count = *count;
		*count = HOST_VM_INFO_REV0_COUNT; /* rev0 already filled in */
		if (original_count >= HOST_VM_INFO_REV1_COUNT) {
			/* rev1 added "purgeable" info */
			stat32->purgeable_count = VM_STATISTICS_TRUNCATE_TO_32_BIT(vm_page_purgeable_count);
			stat32->purges = VM_STATISTICS_TRUNCATE_TO_32_BIT(vm_page_purged_count);
			*count = HOST_VM_INFO_REV1_COUNT;
		}

		if (original_count >= HOST_VM_INFO_REV2_COUNT) {
			/* rev2 added "speculative" info */
			stat32->speculative_count = VM_STATISTICS_TRUNCATE_TO_32_BIT(speculative_count);
			*count = HOST_VM_INFO_REV2_COUNT;
		}

		/* rev3 changed some of the fields to be 64-bit*/

		return KERN_SUCCESS;
	}

	case HOST_CPU_LOAD_INFO: {
		host_cpu_load_info_t cpu_load_info;

		if (*count < HOST_CPU_LOAD_INFO_COUNT) {
			return KERN_FAILURE;
		}

#define GET_TICKS_VALUE(state, ticks)                                                      \
	MACRO_BEGIN cpu_load_info->cpu_ticks[(state)] += (uint32_t)(ticks / hz_tick_interval); \
	MACRO_END
#define GET_TICKS_VALUE_FROM_TIMER(processor, state, timer)                            \
	MACRO_BEGIN GET_TICKS_VALUE(state, timer_grab(&(processor)->timer)); \
	MACRO_END

		cpu_load_info = (host_cpu_load_info_t)info;
		cpu_load_info->cpu_ticks[CPU_STATE_USER] = 0;
		cpu_load_info->cpu_ticks[CPU_STATE_SYSTEM] = 0;
		cpu_load_info->cpu_ticks[CPU_STATE_IDLE] = 0;
		cpu_load_info->cpu_ticks[CPU_STATE_NICE] = 0;

		simple_lock(&processor_list_lock, LCK_GRP_NULL);

		unsigned int pcount = processor_count;

		for (unsigned int i = 0; i < pcount; i++) {
			processor_t processor = processor_array[i];
			assert(processor != PROCESSOR_NULL);
			processor_cpu_load_info(processor, cpu_load_info->cpu_ticks);
		}
		simple_unlock(&processor_list_lock);

		*count = HOST_CPU_LOAD_INFO_COUNT;

		return KERN_SUCCESS;
	}

	case HOST_EXPIRED_TASK_INFO: {
		if (*count < TASK_POWER_INFO_COUNT) {
			return KERN_FAILURE;
		}

		task_power_info_t tinfo1 = (task_power_info_t)info;
		task_power_info_v2_t tinfo2 = (task_power_info_v2_t)info;

		tinfo1->task_interrupt_wakeups = dead_task_statistics.task_interrupt_wakeups;
		tinfo1->task_platform_idle_wakeups = dead_task_statistics.task_platform_idle_wakeups;

		tinfo1->task_timer_wakeups_bin_1 = dead_task_statistics.task_timer_wakeups_bin_1;

		tinfo1->task_timer_wakeups_bin_2 = dead_task_statistics.task_timer_wakeups_bin_2;

		tinfo1->total_user = dead_task_statistics.total_user_time;
		tinfo1->total_system = dead_task_statistics.total_system_time;
		if (*count < TASK_POWER_INFO_V2_COUNT) {
			*count = TASK_POWER_INFO_COUNT;
		} else if (*count >= TASK_POWER_INFO_V2_COUNT) {
			tinfo2->gpu_energy.task_gpu_utilisation = dead_task_statistics.task_gpu_ns;
#if defined(__arm64__)
			tinfo2->task_energy = dead_task_statistics.task_energy;
			tinfo2->task_ptime = dead_task_statistics.total_ptime;
			tinfo2->task_pset_switches = dead_task_statistics.total_pset_switches;
#endif
			*count = TASK_POWER_INFO_V2_COUNT;
		}

		return KERN_SUCCESS;
	}
	default: return KERN_INVALID_ARGUMENT;
	}
}

extern uint32_t c_segment_pages_compressed;

#define HOST_STATISTICS_TIME_WINDOW 1 /* seconds */
#define HOST_STATISTICS_MAX_REQUESTS 10 /* maximum number of requests per window */
#define HOST_STATISTICS_MIN_REQUESTS 2 /* minimum number of requests per window */

uint64_t host_statistics_time_window;

static LCK_GRP_DECLARE(host_statistics_lck_grp, "host_statistics");
static LCK_MTX_DECLARE(host_statistics_lck, &host_statistics_lck_grp);

#define HOST_VM_INFO64_REV0             0
#define HOST_VM_INFO64_REV1             1
#define HOST_EXTMOD_INFO64_REV0         2
#define HOST_LOAD_INFO_REV0             3
#define HOST_VM_INFO_REV0               4
#define HOST_VM_INFO_REV1               5
#define HOST_VM_INFO_REV2               6
#define HOST_CPU_LOAD_INFO_REV0         7
#define HOST_EXPIRED_TASK_INFO_REV0     8
#define HOST_EXPIRED_TASK_INFO_REV1     9
#define HOST_VM_COMPRESSOR_Q_LEN_REV0   10
#define NUM_HOST_INFO_DATA_TYPES        11

static vm_statistics64_data_t host_vm_info64_rev0 = {};
static vm_statistics64_data_t host_vm_info64_rev1 = {};
static vm_extmod_statistics_data_t host_extmod_info64 = {};
static host_load_info_data_t host_load_info = {};
static vm_statistics_data_t host_vm_info_rev0 = {};
static vm_statistics_data_t host_vm_info_rev1 = {};
static vm_statistics_data_t host_vm_info_rev2 = {};
static host_cpu_load_info_data_t host_cpu_load_info = {};
static task_power_info_data_t host_expired_task_info = {};
static task_power_info_v2_data_t host_expired_task_info2 = {};
static vm_compressor_q_lens_data_t host_vm_compressor_q_lens = {};

struct host_stats_cache {
	uint64_t last_access;
	uint64_t current_requests;
	uint64_t max_requests;
	uintptr_t data;
	mach_msg_type_number_t count; //NOTE count is in sizeof(integer_t)
};

static struct host_stats_cache g_host_stats_cache[NUM_HOST_INFO_DATA_TYPES] = {
	[HOST_VM_INFO64_REV0] = { .last_access = 0, .current_requests = 0, .max_requests = 0, .data = (uintptr_t)&host_vm_info64_rev0, .count = HOST_VM_INFO64_REV0_COUNT },
	[HOST_VM_INFO64_REV1] = { .last_access = 0, .current_requests = 0, .max_requests = 0, .data = (uintptr_t)&host_vm_info64_rev1, .count = HOST_VM_INFO64_REV1_COUNT },
	[HOST_EXTMOD_INFO64_REV0] = { .last_access = 0, .current_requests = 0, .max_requests = 0, .data = (uintptr_t)&host_extmod_info64, .count = HOST_EXTMOD_INFO64_COUNT },
	[HOST_LOAD_INFO_REV0] = { .last_access = 0, .current_requests = 0, .max_requests = 0, .data = (uintptr_t)&host_load_info, .count = HOST_LOAD_INFO_COUNT },
	[HOST_VM_INFO_REV0] = { .last_access = 0, .current_requests = 0, .max_requests = 0, .data = (uintptr_t)&host_vm_info_rev0, .count = HOST_VM_INFO_REV0_COUNT },
	[HOST_VM_INFO_REV1] = { .last_access = 0, .current_requests = 0, .max_requests = 0, .data = (uintptr_t)&host_vm_info_rev1, .count = HOST_VM_INFO_REV1_COUNT },
	[HOST_VM_INFO_REV2] = { .last_access = 0, .current_requests = 0, .max_requests = 0, .data = (uintptr_t)&host_vm_info_rev2, .count = HOST_VM_INFO_REV2_COUNT },
	[HOST_CPU_LOAD_INFO_REV0] = { .last_access = 0, .current_requests = 0, .max_requests = 0, .data = (uintptr_t)&host_cpu_load_info, .count = HOST_CPU_LOAD_INFO_COUNT },
	[HOST_EXPIRED_TASK_INFO_REV0] = { .last_access = 0, .current_requests = 0, .max_requests = 0, .data = (uintptr_t)&host_expired_task_info, .count = TASK_POWER_INFO_COUNT },
	[HOST_EXPIRED_TASK_INFO_REV1] = { .last_access = 0, .current_requests = 0, .max_requests = 0, .data = (uintptr_t)&host_expired_task_info2, .count = TASK_POWER_INFO_V2_COUNT},
	[HOST_VM_COMPRESSOR_Q_LEN_REV0] = { .last_access = 0, .current_requests = 0, .max_requests = 0, .data = (uintptr_t)&host_vm_compressor_q_lens, .count = VM_COMPRESSOR_Q_LENS_COUNT},
};


void
host_statistics_init(void)
{
	nanoseconds_to_absolutetime((HOST_STATISTICS_TIME_WINDOW * NSEC_PER_SEC), &host_statistics_time_window);
}

static void
cache_host_statistics(int index, host_info64_t info)
{
	if (index < 0 || index >= NUM_HOST_INFO_DATA_TYPES) {
		return;
	}

	if (task_get_platform_binary(current_task())) {
		return;
	}

	memcpy((void *)g_host_stats_cache[index].data, info, g_host_stats_cache[index].count * sizeof(integer_t));
	return;
}

static void
get_cached_info(int index, host_info64_t info, mach_msg_type_number_t* count)
{
	if (index < 0 || index >= NUM_HOST_INFO_DATA_TYPES) {
		*count = 0;
		return;
	}

	*count = g_host_stats_cache[index].count;
	memcpy(info, (void *)g_host_stats_cache[index].data, g_host_stats_cache[index].count * sizeof(integer_t));
}

static int
get_host_info_data_index(bool is_stat64, host_flavor_t flavor, mach_msg_type_number_t* count, kern_return_t* ret)
{
	switch (flavor) {
	case HOST_VM_INFO64:
		if (!is_stat64) {
			*ret = KERN_INVALID_ARGUMENT;
			return -1;
		}
		if (*count < HOST_VM_INFO64_REV0_COUNT) {
			*ret = KERN_FAILURE;
			return -1;
		}
		if (*count >= HOST_VM_INFO64_REV1_COUNT) {
			return HOST_VM_INFO64_REV1;
		}
		return HOST_VM_INFO64_REV0;

	case HOST_EXTMOD_INFO64:
		if (!is_stat64) {
			*ret = KERN_INVALID_ARGUMENT;
			return -1;
		}
		if (*count < HOST_EXTMOD_INFO64_COUNT) {
			*ret = KERN_FAILURE;
			return -1;
		}
		return HOST_EXTMOD_INFO64_REV0;

	case HOST_LOAD_INFO:
		if (*count < HOST_LOAD_INFO_COUNT) {
			*ret = KERN_FAILURE;
			return -1;
		}
		return HOST_LOAD_INFO_REV0;

	case HOST_VM_INFO:
		if (*count < HOST_VM_INFO_REV0_COUNT) {
			*ret = KERN_FAILURE;
			return -1;
		}
		if (*count >= HOST_VM_INFO_REV2_COUNT) {
			return HOST_VM_INFO_REV2;
		}
		if (*count >= HOST_VM_INFO_REV1_COUNT) {
			return HOST_VM_INFO_REV1;
		}
		return HOST_VM_INFO_REV0;

	case HOST_CPU_LOAD_INFO:
		if (*count < HOST_CPU_LOAD_INFO_COUNT) {
			*ret = KERN_FAILURE;
			return -1;
		}
		return HOST_CPU_LOAD_INFO_REV0;

	case HOST_EXPIRED_TASK_INFO:
		if (*count < TASK_POWER_INFO_COUNT) {
			*ret = KERN_FAILURE;
			return -1;
		}
		if (*count >= TASK_POWER_INFO_V2_COUNT) {
			return HOST_EXPIRED_TASK_INFO_REV1;
		}
		return HOST_EXPIRED_TASK_INFO_REV0;

	case HOST_VM_COMPRESSOR_Q_LENS:
		if (*count < VM_COMPRESSOR_Q_LENS_COUNT) {
			*ret = KERN_FAILURE;
			return -1;
		}
		return HOST_VM_COMPRESSOR_Q_LEN_REV0;

	default:
		*ret = KERN_INVALID_ARGUMENT;
		return -1;
	}
}

static bool
rate_limit_host_statistics(bool is_stat64, host_flavor_t flavor, host_info64_t info, mach_msg_type_number_t* count, kern_return_t* ret, int *pindex)
{
	task_t task = current_task();

	assert(task != kernel_task);

	*ret = KERN_SUCCESS;
	*pindex = -1;

	/* Access control only for third party applications */
	if (task_get_platform_binary(task)) {
		return FALSE;
	}

	/* Rate limit to HOST_STATISTICS_MAX_REQUESTS queries for each HOST_STATISTICS_TIME_WINDOW window of time */
	bool rate_limited = FALSE;
	bool set_last_access = TRUE;

	/* there is a cache for every flavor */
	int index = get_host_info_data_index(is_stat64, flavor, count, ret);
	if (index == -1) {
		goto out;
	}

	*pindex = index;
	lck_mtx_lock(&host_statistics_lck);
	if (g_host_stats_cache[index].last_access > mach_continuous_time() - host_statistics_time_window) {
		set_last_access = FALSE;
		if (g_host_stats_cache[index].current_requests++ >= g_host_stats_cache[index].max_requests) {
			rate_limited = TRUE;
			get_cached_info(index, info, count);
		}
	}
	if (set_last_access) {
		g_host_stats_cache[index].current_requests = 1;
		/*
		 * select a random number of requests (included between HOST_STATISTICS_MIN_REQUESTS and HOST_STATISTICS_MAX_REQUESTS)
		 * to let query host_statistics.
		 * In this way it is not possible to infer looking at when the a cached copy changes if host_statistics was called on
		 * the provious window.
		 */
		g_host_stats_cache[index].max_requests = (mach_absolute_time() % (HOST_STATISTICS_MAX_REQUESTS - HOST_STATISTICS_MIN_REQUESTS + 1)) + HOST_STATISTICS_MIN_REQUESTS;
		g_host_stats_cache[index].last_access = mach_continuous_time();
	}
	lck_mtx_unlock(&host_statistics_lck);
out:
	return rate_limited;
}

kern_return_t
vm_stats(void *info, unsigned int *count)
{
	vm_statistics64_data_t host_vm_stat;
	mach_msg_type_number_t original_count;
	unsigned int local_q_internal_count;
	unsigned int local_q_external_count;
	natural_t speculative_count = vm_page_speculative_count;
	natural_t throttled_count = vm_page_throttled_count;

	if (*count < HOST_VM_INFO64_REV0_COUNT) {
		return KERN_FAILURE;
	}
	get_host_vm_stats(&host_vm_stat);

	vm_statistics64_t stat = (vm_statistics64_t)info;

	stat->free_count = vm_page_free_count + speculative_count;
	stat->active_count = vm_page_active_count;

	local_q_internal_count = 0;
	local_q_external_count = 0;
	if (vm_page_local_q) {
		zpercpu_foreach(lq, vm_page_local_q) {
			stat->active_count += lq->vpl_count;
			local_q_internal_count += lq->vpl_internal_count;
			local_q_external_count += lq->vpl_external_count;
		}
	}
	stat->inactive_count = vm_page_inactive_count;
#if !XNU_TARGET_OS_OSX
	stat->wire_count = vm_page_wire_count;
#else /* !XNU_TARGET_OS_OSX */
	stat->wire_count = vm_page_wire_count + throttled_count + vm_lopage_free_count;
#endif /* !XNU_TARGET_OS_OSX */
	stat->zero_fill_count = host_vm_stat.zero_fill_count;
	stat->reactivations = host_vm_stat.reactivations;
	stat->pageins = host_vm_stat.pageins;
	stat->pageouts = host_vm_stat.pageouts;
	stat->faults = host_vm_stat.faults;
	stat->cow_faults = host_vm_stat.cow_faults;
	stat->lookups = host_vm_stat.lookups;
	stat->hits = host_vm_stat.hits;

	stat->purgeable_count = vm_page_purgeable_count;
	stat->purges = vm_page_purged_count;

	stat->speculative_count = speculative_count;

	/*
	 * Fill in extra info added in later revisions of the
	 * vm_statistics data structure.  Fill in only what can fit
	 * in the data structure the caller gave us !
	 */
	original_count = *count;
	*count = HOST_VM_INFO64_REV0_COUNT; /* rev0 already filled in */
	if (original_count >= HOST_VM_INFO64_REV1_COUNT) {
		/* rev1 added "throttled count" */
		stat->throttled_count = throttled_count;
		/* rev1 added "compression" info */
		stat->compressor_page_count = VM_PAGE_COMPRESSOR_COUNT;
		stat->compressions = host_vm_stat.compressions;
		stat->decompressions = host_vm_stat.decompressions;
		stat->swapins = host_vm_stat.swapins;
		stat->swapouts = host_vm_stat.swapouts;
		/* rev1 added:
		 * "external page count"
		 * "anonymous page count"
		 * "total # of pages (uncompressed) held in the compressor"
		 */
		stat->external_page_count = (vm_page_pageable_external_count + local_q_external_count);
		stat->internal_page_count = (vm_page_pageable_internal_count + local_q_internal_count);
		stat->total_uncompressed_pages_in_compressor = c_segment_pages_compressed;
		*count = HOST_VM_INFO64_REV1_COUNT;
	}

	return KERN_SUCCESS;
}

#if DEVELOPMENT || DEBUG
extern uint32_t        c_segment_count;
extern uint32_t        c_age_count;
extern uint32_t        c_early_swappedin_count, c_regular_swappedin_count, c_late_swappedin_count;
extern uint32_t        c_early_swapout_count, c_regular_swapout_count, c_late_swapout_count;
extern uint32_t        c_swapio_count;
extern uint32_t        c_swappedout_count;
extern uint32_t        c_swappedout_sparse_count;
extern uint32_t        c_major_count;
extern uint32_t        c_filling_count;
extern uint32_t        c_empty_count;
extern uint32_t        c_bad_count;
extern uint32_t        c_minor_count;
extern uint32_t        c_segments_available;

static kern_return_t
vm_compressor_queue_lens(void *info, unsigned int *count)
{
	if (*count < VM_COMPRESSOR_Q_LENS_COUNT) {
		return KERN_NO_SPACE;
	}

	struct vm_compressor_q_lens *qc = (struct vm_compressor_q_lens *)info;
	qc->qcc_segments_available = c_segments_available;
	qc->qcc_segment_count = c_segment_count;
	qc->qcc_age_count = c_age_count;
	qc->qcc_early_swappedin_count = c_early_swappedin_count;
	qc->qcc_regular_swappedin_count = c_regular_swappedin_count;
	qc->qcc_late_swappedin_count = c_late_swappedin_count;
	qc->qcc_early_swapout_count = c_early_swapout_count;
	qc->qcc_regular_swapout_count = c_regular_swapout_count;
	qc->qcc_late_swapout_count = c_late_swapout_count;
	qc->qcc_swapio_count = c_swapio_count;
	qc->qcc_swappedout_count = c_swappedout_count;
	qc->qcc_swappedout_sparse_count = c_swappedout_sparse_count;
	qc->qcc_major_count = c_major_count;
	qc->qcc_filling_count = c_filling_count;
	qc->qcc_empty_count = c_empty_count;
	qc->qcc_bad_count = c_bad_count;
	qc->qcc_minor_count = c_minor_count;

	*count = VM_COMPRESSOR_Q_LENS_COUNT;

	return KERN_SUCCESS;
}

#endif /* DEVELOPMENT || DEBUG */

kern_return_t host_statistics64(host_t host, host_flavor_t flavor, host_info_t info, mach_msg_type_number_t * count);

kern_return_t
host_statistics64(host_t host, host_flavor_t flavor, host_info64_t info, mach_msg_type_number_t * count)
{
	if (host == HOST_NULL) {
		return KERN_INVALID_HOST;
	}

	switch (flavor) {
	case HOST_VM_INFO64: /* We were asked to get vm_statistics64 */
		return vm_stats(info, count);

	case HOST_EXTMOD_INFO64: /* We were asked to get vm_statistics64 */
	{
		vm_extmod_statistics_t out_extmod_statistics;

		if (*count < HOST_EXTMOD_INFO64_COUNT) {
			return KERN_FAILURE;
		}

		out_extmod_statistics = (vm_extmod_statistics_t)info;
		*out_extmod_statistics = host_extmod_statistics;

		*count = HOST_EXTMOD_INFO64_COUNT;

		return KERN_SUCCESS;
	}

	case HOST_VM_COMPRESSOR_Q_LENS:
#if DEVELOPMENT || DEBUG
		return vm_compressor_queue_lens(info, count);
#else
		return KERN_NOT_SUPPORTED;
#endif

	default: /* If we didn't recognize the flavor, send to host_statistics */
		return host_statistics(host, flavor, (host_info_t)info, count);
	}
}

kern_return_t
host_statistics64_from_user(host_t host, host_flavor_t flavor, host_info64_t info, mach_msg_type_number_t * count)
{
	kern_return_t ret = KERN_SUCCESS;
	int index;

	if (host == HOST_NULL) {
		return KERN_INVALID_HOST;
	}

	if (rate_limit_host_statistics(TRUE, flavor, info, count, &ret, &index)) {
		return ret;
	}

	if (ret != KERN_SUCCESS) {
		return ret;
	}

	ret = host_statistics64(host, flavor, info, count);

	if (ret == KERN_SUCCESS) {
		cache_host_statistics(index, info);
	}

	return ret;
}

kern_return_t
host_statistics_from_user(host_t host, host_flavor_t flavor, host_info64_t info, mach_msg_type_number_t * count)
{
	kern_return_t ret = KERN_SUCCESS;
	int index;

	if (host == HOST_NULL) {
		return KERN_INVALID_HOST;
	}

	if (rate_limit_host_statistics(FALSE, flavor, info, count, &ret, &index)) {
		return ret;
	}

	if (ret != KERN_SUCCESS) {
		return ret;
	}

	ret = host_statistics(host, flavor, info, count);

	if (ret == KERN_SUCCESS) {
		cache_host_statistics(index, info);
	}

	return ret;
}

/*
 * Get host statistics that require privilege.
 * None for now, just call the un-privileged version.
 */
kern_return_t
host_priv_statistics(host_priv_t host_priv, host_flavor_t flavor, host_info_t info, mach_msg_type_number_t * count)
{
	return host_statistics((host_t)host_priv, flavor, info, count);
}

kern_return_t
set_sched_stats_active(boolean_t active)
{
	sched_stats_active = active;
	return KERN_SUCCESS;
}

kern_return_t
get_sched_statistics(struct _processor_statistics_np * out, uint32_t * count)
{
	uint32_t pos = 0;

	if (!sched_stats_active) {
		return KERN_FAILURE;
	}

	percpu_foreach_base(pcpu_base) {
		struct sched_statistics stats;
		processor_t processor;

		pos += sizeof(struct _processor_statistics_np);
		if (pos > *count) {
			return KERN_FAILURE;
		}

		stats = *PERCPU_GET_WITH_BASE(pcpu_base, sched_stats);
		processor = PERCPU_GET_WITH_BASE(pcpu_base, processor);

		out->ps_cpuid = processor->cpu_id;
		out->ps_csw_count = stats.csw_count;
		out->ps_preempt_count = stats.preempt_count;
		out->ps_preempted_rt_count = stats.preempted_rt_count;
		out->ps_preempted_by_rt_count = stats.preempted_by_rt_count;
		out->ps_rt_sched_count = stats.rt_sched_count;
		out->ps_interrupt_count = stats.interrupt_count;
		out->ps_ipi_count = stats.ipi_count;
		out->ps_timer_pop_count = stats.timer_pop_count;
		out->ps_runq_count_sum = SCHED(processor_runq_stats_count_sum)(processor);
		out->ps_idle_transitions = stats.idle_transitions;
		out->ps_quantum_timer_expirations = stats.quantum_timer_expirations;

		out++;
	}

	/* And include RT Queue information */
	pos += sizeof(struct _processor_statistics_np);
	if (pos > *count) {
		return KERN_FAILURE;
	}

	bzero(out, sizeof(*out));
	out->ps_cpuid = (-1);
	out->ps_runq_count_sum = SCHED(rt_runq_count_sum)();
	out++;

	*count = pos;

	return KERN_SUCCESS;
}

kern_return_t
host_page_size(host_t host, vm_size_t * out_page_size)
{
	if (host == HOST_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	*out_page_size = PAGE_SIZE;

	return KERN_SUCCESS;
}

/*
 *	Return kernel version string (more than you ever
 *	wanted to know about what version of the kernel this is).
 */
extern char version[];

kern_return_t
host_kernel_version(host_t host, kernel_version_t out_version)
{
	if (host == HOST_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	(void)strncpy(out_version, version, sizeof(kernel_version_t));

	return KERN_SUCCESS;
}

/*
 *	host_processor_sets:
 *
 *	List all processor sets on the host.
 */
kern_return_t
host_processor_sets(host_priv_t host_priv, processor_set_name_array_t * pset_list, mach_msg_type_number_t * count)
{
	mach_port_array_t ports;

	if (host_priv == HOST_PRIV_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	/*
	 *	Allocate memory.  Can be pageable because it won't be
	 *	touched while holding a lock.
	 */

	ports = mach_port_array_alloc(1, Z_WAITOK | Z_NOFAIL);

	/* do the conversion that Mig should handle */
	ports[0].port = convert_pset_name_to_port(&pset0);

	*pset_list = ports;
	*count = 1;

	return KERN_SUCCESS;
}

/*
 *	host_processor_set_priv:
 *
 *	Return control port for given processor set.
 */
kern_return_t
host_processor_set_priv(host_priv_t host_priv, processor_set_t pset_name, processor_set_t * pset)
{
	if (host_priv == HOST_PRIV_NULL || pset_name == PROCESSOR_SET_NULL) {
		*pset = PROCESSOR_SET_NULL;

		return KERN_INVALID_ARGUMENT;
	}

	*pset = pset_name;

	return KERN_SUCCESS;
}

/*
 *	host_processor_info
 *
 *	Return info about the processors on this host.  It will return
 *	the number of processors, and the specific type of info requested
 *	in an OOL array.
 */
kern_return_t
host_processor_info(host_t host,
    processor_flavor_t flavor,
    natural_t * out_pcount,
    processor_info_array_t * out_array,
    mach_msg_type_number_t * out_array_count)
{
	kern_return_t result;
	host_t thost;
	processor_info_t info;
	unsigned int icount;
	unsigned int pcount;
	vm_offset_t addr;
	vm_size_t size, needed;
	vm_map_copy_t copy;

	if (host == HOST_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	result = processor_info_count(flavor, &icount);
	if (result != KERN_SUCCESS) {
		return result;
	}

	pcount = processor_count;
	assert(pcount != 0);

	needed = pcount * icount * sizeof(natural_t);
	size = vm_map_round_page(needed, VM_MAP_PAGE_MASK(ipc_kernel_map));
	result = kmem_alloc(ipc_kernel_map, &addr, size, KMA_DATA, VM_KERN_MEMORY_IPC);
	if (result != KERN_SUCCESS) {
		return KERN_RESOURCE_SHORTAGE;
	}

	info = (processor_info_t)addr;

	for (unsigned int i = 0; i < pcount; i++) {
		processor_t processor = processor_array[i];
		assert(processor != PROCESSOR_NULL);

		unsigned int tcount = icount;

		result = processor_info(processor, flavor, &thost, info, &tcount);
		if (result != KERN_SUCCESS) {
			kmem_free(ipc_kernel_map, addr, size);
			return result;
		}
		info += icount;
	}

	if (size != needed) {
		bzero((char *)addr + needed, size - needed);
	}

	result = vm_map_unwire(ipc_kernel_map, vm_map_trunc_page(addr, VM_MAP_PAGE_MASK(ipc_kernel_map)),
	    vm_map_round_page(addr + size, VM_MAP_PAGE_MASK(ipc_kernel_map)), FALSE);
	assert(result == KERN_SUCCESS);
	result = vm_map_copyin(ipc_kernel_map, (vm_map_address_t)addr, (vm_map_size_t)needed, TRUE, &copy);
	assert(result == KERN_SUCCESS);

	*out_pcount = pcount;
	*out_array = (processor_info_array_t)copy;
	*out_array_count = pcount * icount;

	return KERN_SUCCESS;
}

static bool
is_valid_host_special_port(int id)
{
	return (id <= HOST_MAX_SPECIAL_PORT) &&
	       (id >= HOST_MIN_SPECIAL_PORT) &&
	       ((id <= HOST_LAST_SPECIAL_KERNEL_PORT) || (id > HOST_MAX_SPECIAL_KERNEL_PORT));
}

extern void * XNU_PTRAUTH_SIGNED_PTR("initproc") initproc;

/*
 *      Kernel interface for setting a special port.
 */
kern_return_t
kernel_set_special_port(host_priv_t host_priv, int id, ipc_port_t port)
{
	ipc_port_t old_port;

	if (!is_valid_host_special_port(id)) {
		panic("attempted to set invalid special port %d", id);
	}

#if !MACH_FLIPC
	if (id == HOST_NODE_PORT) {
		return KERN_NOT_SUPPORTED;
	}
#endif

	host_lock(host_priv);
	old_port = host_priv->special[id];
	host_priv->special[id] = port;
	host_unlock(host_priv);

#if MACH_FLIPC
	if (id == HOST_NODE_PORT) {
		mach_node_port_changed();
	}
#endif

	if (IP_VALID(old_port)) {
		ipc_port_release_send(old_port);
	}


	return KERN_SUCCESS;
}

/*
 *      Kernel interface for retrieving a special port.
 */
kern_return_t
kernel_get_special_port(host_priv_t host_priv, int id, ipc_port_t * portp)
{
	if (!is_valid_host_special_port(id)) {
		panic("attempted to get invalid special port %d", id);
	}

	host_lock(host_priv);
	*portp = host_priv->special[id];
	host_unlock(host_priv);
	return KERN_SUCCESS;
}

/*
 *      User interface for setting a special port.
 *
 *      Only permits the user to set a user-owned special port
 *      ID, rejecting a kernel-owned special port ID.
 *
 *      A special kernel port cannot be set up using this
 *      routine; use kernel_set_special_port() instead.
 */
kern_return_t
host_set_special_port_from_user(host_priv_t host_priv, int id, ipc_port_t port)
{
	if (host_priv == HOST_PRIV_NULL || id <= HOST_MAX_SPECIAL_KERNEL_PORT || id > HOST_MAX_SPECIAL_PORT) {
		return KERN_INVALID_ARGUMENT;
	}

	if (task_is_driver(current_task())) {
		return KERN_NO_ACCESS;
	}

	if (IP_VALID(port) && (port->ip_immovable_receive || port->ip_immovable_send)) {
		return KERN_INVALID_RIGHT;
	}

	return host_set_special_port(host_priv, id, port);
}

kern_return_t
host_set_special_port(host_priv_t host_priv, int id, ipc_port_t port)
{
	if (host_priv == HOST_PRIV_NULL || id <= HOST_MAX_SPECIAL_KERNEL_PORT || id > HOST_MAX_SPECIAL_PORT) {
		return KERN_INVALID_ARGUMENT;
	}

	if (current_task() != kernel_task && get_bsdtask_info(current_task()) != initproc) {
		bool allowed = (id == HOST_TELEMETRY_PORT &&
		    IOTaskHasEntitlement(current_task(), "com.apple.private.xpc.launchd.event-monitor"));
#if CONFIG_CSR
		if (!allowed) {
			allowed = (csr_check(CSR_ALLOW_TASK_FOR_PID) == 0);
		}
#endif
		if (!allowed) {
			return KERN_NO_ACCESS;
		}
	}

#if CONFIG_MACF
	if (mac_task_check_set_host_special_port(current_task(), id, port) != 0) {
		return KERN_NO_ACCESS;
	}
#endif

	return kernel_set_special_port(host_priv, id, port);
}

/*
 *      User interface for retrieving a special port.
 *
 *      Note that there is nothing to prevent a user special
 *      port from disappearing after it has been discovered by
 *      the caller; thus, using a special port can always result
 *      in a "port not valid" error.
 */

kern_return_t
host_get_special_port_from_user(host_priv_t host_priv, __unused int node, int id, ipc_port_t * portp)
{
	if (host_priv == HOST_PRIV_NULL || id == HOST_SECURITY_PORT || id > HOST_MAX_SPECIAL_PORT || id < HOST_MIN_SPECIAL_PORT) {
		return KERN_INVALID_ARGUMENT;
	}

	task_t task = current_task();
	if (task && task_is_driver(task) && id > HOST_MAX_SPECIAL_KERNEL_PORT) {
		/* allow HID drivers to get the sysdiagnose port for keychord handling */
		if (id == HOST_SYSDIAGNOSE_PORT &&
		    IOCurrentTaskHasEntitlement(kIODriverKitHIDFamilyEventServiceEntitlementKey)) {
			goto get_special_port;
		}
		return KERN_NO_ACCESS;
	}
get_special_port:
	return host_get_special_port(host_priv, node, id, portp);
}

kern_return_t
host_get_special_port(host_priv_t host_priv, __unused int node, int id, ipc_port_t * portp)
{
	ipc_port_t port;

	if (host_priv == HOST_PRIV_NULL || id == HOST_SECURITY_PORT || id > HOST_MAX_SPECIAL_PORT || id < HOST_MIN_SPECIAL_PORT) {
		return KERN_INVALID_ARGUMENT;
	}

	host_lock(host_priv);
	port = realhost.special[id];
	switch (id) {
	case HOST_PORT:
		*portp = ipc_kobject_copy_send(port, &realhost, IKOT_HOST);
		break;
	case HOST_PRIV_PORT:
		*portp = ipc_kobject_copy_send(port, &realhost, IKOT_HOST_PRIV);
		break;
	case HOST_IO_MAIN_PORT:
		*portp = ipc_port_copy_send_any(main_device_port);
		break;
	default:
		*portp = ipc_port_copy_send_mqueue(port);
		break;
	}
	host_unlock(host_priv);

	return KERN_SUCCESS;
}

/*
 *	host_get_io_main
 *
 *	Return the IO main access port for this host.
 */
kern_return_t
host_get_io_main(host_t host, io_main_t * io_mainp)
{
	if (host == HOST_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	return host_get_io_main_port(host_priv_self(), io_mainp);
}

host_t
host_self(void)
{
	return &realhost;
}

host_priv_t
host_priv_self(void)
{
	return &realhost;
}

kern_return_t
host_set_atm_diagnostic_flag(host_t host, uint32_t diagnostic_flag)
{
	if (host == HOST_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	if (!IOCurrentTaskHasEntitlement("com.apple.private.set-atm-diagnostic-flag")) {
		return KERN_NO_ACCESS;
	}

#if CONFIG_ATM
	return atm_set_diagnostic_config(diagnostic_flag);
#else
	(void)diagnostic_flag;
	return KERN_NOT_SUPPORTED;
#endif
}

kern_return_t
host_set_multiuser_config_flags(host_priv_t host_priv, uint32_t multiuser_config)
{
#if !defined(XNU_TARGET_OS_OSX)
	if (host_priv == HOST_PRIV_NULL) {
		return KERN_INVALID_ARGUMENT;
	}

	/*
	 * multiuser bit is extensively used for sharedIpad mode.
	 * Caller sets the sharedIPad or other mutiuser modes.
	 * Any override during commpage setting is not suitable anymore.
	 */
	commpage_update_multiuser_config(multiuser_config);
	return KERN_SUCCESS;
#else
	(void)host_priv;
	(void)multiuser_config;
	return KERN_NOT_SUPPORTED;
#endif
}
