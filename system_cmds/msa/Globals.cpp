//
//  MachineGlobals.cpp
//  msa
//
//  Created by James McIlree on 4/17/13.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#include "global.h"

Globals::Globals() :
	_cpu_count(0),
	_iop_count(0),
	_kernel_size(Kernel::is_64_bit() ? KernelSize::k64 : KernelSize::k32),
	_live_update_interval("100ms"),
	_is_cpu_count_set(false),
	_is_iop_count_set(false),
	_is_kernel_size_set(false),
	_is_timebase_set(false),
	_beginning_of_time(0),
	_should_print_mach_absolute_timestamps(false),
	_should_print_event_index(false),
	_is_verbose(false),
	_should_presort_events(false),
	_should_zero_base_timestamps(true),
	_should_trace_voucher_contents(true),
	_lifecycle_filter(kLifecycleFilter::User),
	_mach_msg_filter(kMachMsgFilter::Voucher)
{
	// Default to the current machine's values
	mach_timebase_info(&_timebase_info);

	for (auto& entry : KDBG::cpumap()) {
		if (entry.is_iop())
			_iop_count++;
		else
			_cpu_count++;
	}

	// If we are unable to get a cpumap,
	// fallback on the current # of cpus
	if (_cpu_count == 0) {
		_cpu_count = Kernel::active_cpu_count();
		_iop_count = 0;
	}

	// This is only used as is for live tracing or capturing a trace,
	// so we want to use the current # of cpus.
	_trace_buffer_size = 250000 * _cpu_count;
}

static AbsTime parse_time(const char* arg, mach_timebase_info_data_t timebase_info) {

	char* units;
	uint64_t value = strtoull(arg, &units, 0);

	// Unspecified units are treated as seconds
	if (*units == 0 || strcmp(units, "s") == 0) {
		return NanoTime(value * NANOSECONDS_PER_SECOND).abs_time(timebase_info);
	}

	if (strcmp(units, "ms") == 0)
		return NanoTime(value * NANOSECONDS_PER_MILLISECOND).abs_time(timebase_info);

	if (strcmp(units, "us") == 0)
		return NanoTime(value * NANOSECONDS_PER_MICROSECOND).abs_time(timebase_info);

	if (strcmp(units, "ns") == 0)
		return NanoTime(value).abs_time(timebase_info);

	if (strcmp(units, "mabs") == 0) {
		return AbsTime(value);
	}

	usage("Unable to parse units on time value");
}

AbsTime Globals::live_update_interval() const {
	return parse_time(_live_update_interval.c_str(), _timebase_info);
}
