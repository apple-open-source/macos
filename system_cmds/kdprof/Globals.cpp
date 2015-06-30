//
//  MachineGlobals.cpp
//  kdprof
//
//  Created by James McIlree on 4/17/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#include "global.h"

Globals::Globals() :
	_cpu_count(0),
	_iop_count(0),
	_kernel_size(Kernel::is_64_bit() ? KernelSize::k64 : KernelSize::k32),
	_is_cpu_count_set(false),
	_is_iop_count_set(false),
	_is_kernel_size_set(false),
	_is_summary_start_set(false),
	_is_summary_stop_set(false),
	_is_summary_step_set(false),
	_is_should_print_summary_set(false),
	_is_timebase_set(false),
	_should_read_default_trace_codes(true),
	_should_print_mach_absolute_timestamps(false),
	_should_print_event_index(false),
	_should_print_symbolic_event_codes(true),
	_is_verbose(false),
	_should_presort_events(false),
	_should_print_cpu_summaries(false),
	_should_print_process_summaries(true),
	_should_print_thread_summaries(false),
	_should_print_events(false),
	_should_print_summary(false),
	_should_zero_base_timestamps(true),
	_should_print_process_start_stop_timestamps(false),
	_should_print_csv_summary(false),
	_sort_key(kSortKey::CPU)
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
}

AbsTime Globals::parse_time(const char* arg) const {

	char* units;
	uint64_t value = strtoull(arg, &units, 0);

	// Unspecified units are treated as seconds
	if (*units == 0 || strcmp(units, "s") == 0) {
		return NanoTime(value * NANOSECONDS_PER_SECOND).abs_time(_timebase_info);
	}

	if (strcmp(units, "ms") == 0)
		return NanoTime(value * NANOSECONDS_PER_MILLISECOND).abs_time(_timebase_info);

	if (strcmp(units, "us") == 0)
		return NanoTime(value * NANOSECONDS_PER_MICROSECOND).abs_time(_timebase_info);

	if (strcmp(units, "ns") == 0)
		return NanoTime(value).abs_time(_timebase_info);

	if (strcmp(units, "mabs") == 0) {
		return AbsTime(value);
	}

	usage("Unable to parse units on time value");
}

AbsTime Globals::summary_start(AbsInterval timespan) const {
	AbsTime start(timespan.location());

	if (is_summary_start_set()) {
		AbsTime summary_start = parse_time(_summary_start.c_str());

		bool absolute_start_stop = (_beginning_of_time == 0);
		if (absolute_start_stop)
			start = summary_start;
		else
			start += summary_start;
	}

	return start;
}

AbsTime Globals::summary_stop(AbsInterval timespan) const {

	if (is_summary_stop_set()) {
		AbsTime summary_stop = parse_time(_summary_stop.c_str());

		bool absolute_start_stop = (_beginning_of_time == 0);
		if (absolute_start_stop)
			return summary_stop;
		else
			return timespan.location() + summary_stop;
	}

	return timespan.max();
}

AbsTime Globals::summary_step(AbsInterval timespan) const {
	if (is_summary_step_set()) {
		return parse_time(_summary_step.c_str());
	}

	return timespan.length();
}
