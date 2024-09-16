/*
 * Copyright (c) 2013 Apple Inc. All rights reserved.
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

#include <kern/energy_perf.h>

#include <libsa/types.h>
#include <sys/kdebug.h>
#include <stddef.h>
#include <machine/machine_routines.h>

#include <kern/coalition.h>
#include <kern/task.h>
#include <kern/task_ident.h>

void
gpu_describe(__unused gpu_descriptor_t gdesc)
{
	KERNEL_DEBUG_CONSTANT(MACHDBG_CODE(DBG_MACH_ENERGY_PERF, 1), gdesc->gpu_id, gdesc->gpu_max_domains, 0, 0, 0);
}

uint64_t
gpu_accumulate_time(__unused uint32_t scope, __unused uint32_t gpu_id, __unused uint32_t gpu_domain, __unused uint64_t gpu_accumulated_ns, __unused uint64_t gpu_tstamp_ns)
{
	KERNEL_DEBUG_CONSTANT(MACHDBG_CODE(DBG_MACH_ENERGY_PERF, 2), scope, gpu_id, gpu_domain, gpu_accumulated_ns, gpu_tstamp_ns);
	ml_gpu_stat_update(gpu_accumulated_ns);
	return 0;
}

static uint64_t
io_rate_update_cb_default(__unused uint64_t io_rate_flags, __unused uint64_t read_ops_delta, __unused uint64_t write_ops_delta, __unused uint64_t read_bytes_delta, __unused uint64_t write_bytes_delta)
{
	KERNEL_DEBUG_CONSTANT(MACHDBG_CODE(DBG_MACH_ENERGY_PERF, 3), io_rate_flags, read_ops_delta, write_ops_delta, read_bytes_delta, write_bytes_delta);
	return 0;
}

io_rate_update_callback_t io_rate_update_cb = io_rate_update_cb_default;

void
io_rate_update_register(io_rate_update_callback_t io_rate_update_cb_new)
{
	if (io_rate_update_cb_new != NULL) {
		io_rate_update_cb = io_rate_update_cb_new;
	} else {
		io_rate_update_cb = io_rate_update_cb_default;
	}
}

uint64_t
io_rate_update(uint64_t io_rate_flags, uint64_t read_ops_delta, uint64_t write_ops_delta, uint64_t read_bytes_delta, uint64_t write_bytes_delta)
{
	return io_rate_update_cb(io_rate_flags, read_ops_delta, write_ops_delta, read_bytes_delta, write_bytes_delta);
}

static uint64_t
gpu_set_fceiling_cb_default(__unused uint32_t gfr, __unused uint64_t gfp)
{
	return 0ULL;
}

gpu_set_fceiling_t gpu_set_fceiling_cb = gpu_set_fceiling_cb_default;

void
gpu_fceiling_cb_register(gpu_set_fceiling_t gnewcb)
{
	if (gnewcb != NULL) {
		gpu_set_fceiling_cb = gnewcb;
	} else {
		gpu_set_fceiling_cb = gpu_set_fceiling_cb_default;
	}
}

void
gpu_submission_telemetry(
	__unused uint64_t gpu_ncmds,
	__unused uint64_t gpu_noutstanding_avg,
	__unused uint64_t gpu_busy_ns_total,
	__unused uint64_t gpu_cycles,
	__unused uint64_t gpu_telemetry_valid_flags,
	__unused uint64_t gpu_telemetry_misc)
{
}

kern_return_t
current_energy_id(energy_id_t *energy_id)
{
	coalition_t coalition = task_get_coalition(current_task(),
	    COALITION_TYPE_RESOURCE);

	if (coalition == COALITION_NULL) {
		*energy_id = ENERGY_ID_NONE;
		return KERN_FAILURE;
	}

	uint64_t cid = coalition_id(coalition);

	*energy_id = cid;

	return KERN_SUCCESS;
}

kern_return_t
task_id_token_to_energy_id(mach_port_name_t name, energy_id_t *energy_id)
{
	if (current_task() == kernel_task) {
		panic("cannot translate task id token from a kernel thread");
	}

	task_t task = TASK_NULL;
	kern_return_t kr = task_id_token_port_name_to_task(name, &task);
	/* holds task reference upon success */

	if (kr != KERN_SUCCESS) {
		assert(task == TASK_NULL);
		return kr;
	}

	coalition_t coalition = task_get_coalition(task, COALITION_TYPE_RESOURCE);

	assert(coalition != COALITION_NULL);

	uint64_t cid = coalition_id(coalition);

	*energy_id = cid;

	task_deallocate(task);

	return KERN_SUCCESS;
}

kern_return_t
energy_id_report_energy(energy_id_source_t energy_source, energy_id_t self_id,
    energy_id_t on_behalf_of_id, uint64_t energy)
{
	if (energy_source != ENERGY_ID_SOURCE_GPU) {
		return KERN_NOT_SUPPORTED;
	}

	if (self_id == ENERGY_ID_NONE) {
		return KERN_INVALID_ARGUMENT;
	}

	bool exists;

	if (on_behalf_of_id == ENERGY_ID_NONE) {
		exists = coalition_add_to_gpu_energy(self_id, CGE_SELF, energy);
	} else {
		exists = coalition_add_to_gpu_energy(self_id, CGE_SELF | CGE_OTHERS,
		    energy);
		coalition_add_to_gpu_energy(on_behalf_of_id, CGE_BILLED,
		    energy);
	}

	if (exists) {
		return KERN_SUCCESS;
	} else {
		return KERN_NOT_FOUND;
	}
}
