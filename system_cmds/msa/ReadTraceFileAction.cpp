//
//  ReadTraceFileAction.cpp
//  msa
//
//  Created by James McIlree on 4/15/13.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#include "global.h"

template <typename SIZE>
static void execute_arch_specific(Globals& globals, TraceFile& file)
{
	Machine<SIZE> machine(file);

	if (!machine.lost_events()) {
		if (globals.should_zero_base_timestamps() && machine.event_count()) {
			globals.set_beginning_of_time(machine.events()[0].timestamp());
		} else {
			globals.set_beginning_of_time(AbsTime(0));
		}

		if (!globals.is_timebase_set()) {
			if (machine.is_ios()) {
				globals.set_timebase({ 125, 3 }, false);
			} else {
				globals.set_timebase({ 1, 1 }, false);
			}
		}

		char buf[PATH_MAX];
		char* buf_end = buf + sizeof(buf);
		print_mach_msg_header(buf, buf_end, globals);
		dprintf(globals.output_fd(), "%s", buf);

		std::unordered_map<pid_t, bool> task_appnap_state;
		std::unordered_map<pid_t, TaskRequestedPolicy> task_requested_state;
		std::unordered_map<typename SIZE::ptr_t, TaskRequestedPolicy> thread_requested_state;
		std::unordered_map<pid_t, std::pair<TaskEffectivePolicy, uint32_t>> task_effective_state;
		std::unordered_map<typename SIZE::ptr_t, std::pair<TaskEffectivePolicy, uint32_t>> thread_effective_state;
		std::unordered_map<pid_t, std::pair<uint32_t, uint32_t>> task_boosts;

		process_events(globals, machine, task_appnap_state, task_requested_state, thread_requested_state, task_effective_state, thread_effective_state, task_boosts);
	} else {
		log_msg(ASL_LEVEL_WARNING, "The trace data indicates that events were lost, the file cannot be processed\n");
	}
}

void ReadTraceFileAction::execute(Globals& globals) {
	TraceFile file(_path.c_str(), globals.should_presort_events(), globals.cpu_count(), globals.iop_count());
	if (globals.is_kernel_size_set()) {
		try {
			if (globals.kernel_size() == KernelSize::k32)
				execute_arch_specific<Kernel32>(globals, file);
			else
				execute_arch_specific<Kernel64>(globals, file);
		} catch (Exception& e) {
			log_msg(ASL_LEVEL_ERR, "An exception was raised: %s", e.what());
			log_msg(ASL_LEVEL_ERR, "An explicit kernel size was set, you may want to try not forcing the size to a single value\n");
			log_msg(ASL_LEVEL_ERR, "You may also want to check the number of cpus and iops configured if the file is from a device and does not have a cpumap\n");
		}
	} else {
		if (file.is_valid()) {
			if (file.is_64_bit()) {
				execute_arch_specific<Kernel64>(globals, file);
			} else {
				execute_arch_specific<Kernel32>(globals, file);
			}
		} else {
                        if (file.mmap_failed()) {
				log_msg(ASL_LEVEL_ERR, "Unable to mmap %s, it may exceed this devices memory limits\n", _path.c_str());
			} else {
				log_msg(ASL_LEVEL_ERR, "%s does not appear to be a valid trace file\n", _path.c_str());
			}
		}
	}
}
