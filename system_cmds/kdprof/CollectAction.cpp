//
//  CollectAction.cpp
//  kdprof
//
//  Created by James McIlree on 4/17/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#include "global.h"

template <typename SIZE>
static void execute_arch_specific(Globals& globals, KDState& state) {
	// Collect all data first, printing takes time...
	auto threadmap = KDBG::threadmap<SIZE>(state);
	auto cpumap = KDBG::cpumap();

	MemoryBuffer<KDEvent<SIZE>> events(state.capacity());
	int count = KDBG::read(events.data(), events.capacity() * sizeof(KDEvent<SIZE>));

	// Now handle any verbose printing.
	/*if (globals.is_verbose()) {
		printf("\n%lu threadmap entries:\n", threadmap.size());
		for (auto& entry : threadmap) {
			printf("\t0x%08llX %8u %20s\n", (uint64_t)entry.tid(), entry.pid(), entry.name());
		}

		printf("\n%lu cpumap entries:\n", cpumap.size());
		for (auto& entry : cpumap) {
			printf("\t%3u %8s\n", entry.cpu_id(), entry.name());
		}

		printf("\n%d events:\n", count);
	}*/

	if (globals.should_presort_events()) {
		std::sort(events.data(), events.data() + count, [](KDEvent<SIZE> const& p0, KDEvent<SIZE> const& p1) -> bool {
			return p0.timestamp() < p1.timestamp();
		});
	}
	Machine<SIZE> machine((KDCPUMapEntry*)cpumap.data(), (uint32_t)cpumap.size(), (KDThreadMapEntry<SIZE>*)threadmap.data(), (uint32_t)threadmap.size(), (KDEvent<SIZE>*)events.data(), (uintptr_t)count);

	if (!machine.lost_events()) {
		if (globals.should_zero_base_timestamps() && count) {
			globals.set_beginning_of_time((events.data())->timestamp());
		} else {
			globals.set_beginning_of_time(AbsTime(0));
		}

		if (!globals.is_timebase_set()) {
			mach_timebase_info_data_t timebase;
			mach_timebase_info(&timebase);
			globals.set_timebase(timebase, false);
		}

		if (globals.is_verbose()) {
			dprintf(globals.output_fd(), "\nLIVE DATA\n");
			print_verbose_machine_info(globals, machine, (uint32_t)threadmap.size(), (uint32_t)cpumap.size());
		}

		if (globals.should_print_events()) {
			// print_machine(globals, machine);
			// print_machine_parallel(globals, machine);
			print_machine_events(globals, machine);
		}

		if (globals.should_print_summary()) {
			print_machine_summary(globals, machine);
		}

		if (globals.should_print_csv_summary()) {
			print_machine_csv_summary(globals, machine);
		}

		if (globals.should_print_process_start_stop_timestamps()) {
			print_process_start_stop_timestamps(globals, machine);
		}
	} else {
		log_msg(ASL_LEVEL_WARNING, "The trace data indicates that events were lost, the file cannot be processed\n");
	}
}

void CollectAction::execute(Globals& globals) {
	KDState state = KDBG::state();
	if (state.is_lp64()) {
		execute_arch_specific<Kernel64>(globals, state);
	} else {
		execute_arch_specific<Kernel32>(globals, state);
	}
}
