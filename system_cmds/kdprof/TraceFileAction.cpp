//
//  TraceFileAction.cpp
//  kdprof
//
//  Created by James McIlree on 4/15/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#include "global.h"

#if 0
template <typename SIZE>
static void execute_arch_specific(Globals& globals, std::string path)
{
	//
	// Trace file looks roughly like:
	//
	// RAW_header
	// threadmap[thread_count]
	// wasted-space-to-align-to-next-4096-byte-boundary
	// KDEvents[]
	//

	MappedFile trace_data(path.c_str());
	if (TraceDataHeader<SIZE>* header = reinterpret_cast<TraceDataHeader<SIZE>*>(trace_data.address())) {

		KDThreadMapEntry<SIZE>* threadmap = NULL;
		uint32_t threadmap_count = 0;
		KDCPUMapEntry* cpumap = NULL;
		uint32_t cpumap_count = 0;
		KDEvent<SIZE>* events = NULL;
		
		if (header->version() != RAW_VERSION1) {
			// If the header is not a RAW_VERSION1, we must assume it is a
			// RAW_VERSION0. The difficulty here is that RAW_VERSION0 consists
			// of 4 bytes, which are the thread_count. We can't do much
			// sanity checking. The first four bytes are already read into
			// the existing header, reuse them. We must also reset the file
			// offset.

			threadmap_count = header->version();
			threadmap = reinterpret_cast<KDThreadMapEntry<SIZE>*>(trace_data.address() + 4);

			// Event data starts immediately following the threadmap
			size_t offset = 4 + threadmap_count * sizeof(KDThreadMapEntry<SIZE>);
			events = reinterpret_cast<KDEvent<SIZE>*>(trace_data.address() + offset);
		} else {
			//
			// RAW_VERSION1
			//
			threadmap_count = header->thread_count();
			threadmap = reinterpret_cast<KDThreadMapEntry<SIZE>*>(trace_data.address() + sizeof(TraceDataHeader<SIZE>));

			size_t threadmap_size_in_bytes = threadmap_count * sizeof(KDThreadMapEntry<SIZE>);
			size_t offset_to_event_data = (sizeof(TraceDataHeader<SIZE>) + threadmap_size_in_bytes + 4095) & ~4095;
			size_t offset_to_cpumap_data = sizeof(TraceDataHeader<SIZE>) + threadmap_size_in_bytes;
			size_t cpumap_bytes = offset_to_event_data - offset_to_cpumap_data;

			//
			// In a RAW_VERSION1, there *may* be a cpumap.
			// If it exists, it will be between the header and the page aligned offset
			// that event data begins at.
			//
			if (cpumap_bytes > sizeof(kd_cpumap_header) + sizeof(kd_cpumap)) {
				kd_cpumap_header* cpumap_header = reinterpret_cast<kd_cpumap_header*>(trace_data.address() + offset_to_cpumap_data);
				if (cpumap_header->version_no == RAW_VERSION1) {
					cpumap = (KDCPUMapEntry*)&cpumap_header[1];
					cpumap_count = cpumap_header->cpu_count;
				}
			}

			// Event data starts at the next PAGE alignment boundary.
			//
			// Hmm, this could be pretty awful in iOS...
			//
			// Kernel page size is 4k. Userspace page size is 16kb in 64b.
			// Kernel writes the data. Unless the kernel call fails, then userspace writes the data. Blech.
			events = reinterpret_cast<KDEvent<SIZE>*>(trace_data.address() + offset_to_event_data);
		}

		uintptr_t event_count = (uintptr_t)trace_data.size() - (reinterpret_cast<uintptr_t>(events) - reinterpret_cast<uintptr_t>(trace_data.address()));
		if (event_count % sizeof(KDEvent<SIZE>) != 0) {
			// We're probably looking at the wrong k32/k64. Throw and try the other size.
			THROW("Bytes in file does not match an even multiple of Event struct");
		}
		event_count /= sizeof(KDEvent<SIZE>);

		std::vector<KDCPUMapEntry> default_cpumap;

		if (cpumap == NULL || cpumap_count == 0) {			
			// No cpumap found, we need to fake one up using the default values.
			for (uint32_t i=0; i<globals.cpu_count(); ++i) {
				default_cpumap.emplace_back(i, 0, "AP-???");
			}
			uint32_t iop_limit = globals.cpu_count() + globals.iop_count();
			for (uint32_t i=globals.cpu_count(); i<iop_limit; ++i) {
				default_cpumap.emplace_back(i, KDBG_CPUMAP_IS_IOP, "IOP-???");
			}

			cpumap = default_cpumap.data();
			cpumap_count = (uint32_t)default_cpumap.size();
		}

		// IOP's have been producing .trace files with out of order events.
		// This is a hack fix to work around that. It costs a full copy of the data!
		MemoryBuffer<KDEvent<SIZE>> presorted_events;
		if (globals.should_presort_events() && event_count) {
			presorted_events.set_capacity(event_count);
			memcpy(presorted_events.data(), events, event_count * sizeof(KDEvent<SIZE>));
			events = presorted_events.data();
			std::sort(events, events + event_count, [](KDEvent<SIZE> const& p0, KDEvent<SIZE> const& p1) -> bool {
				return p0.timestamp() < p1.timestamp();
			});
		}

		Machine<SIZE> machine(cpumap, cpumap_count, threadmap, threadmap_count, events, event_count);

		if (!machine.lost_events()) {
			if (globals.should_zero_base_timestamps() && event_count) {
				globals.set_beginning_of_time(events[0].timestamp());
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

			if (globals.is_verbose()) {
				dprintf(globals.output_fd(), "\n%s\n", path.c_str());
				print_verbose_machine_info(globals, machine, threadmap_count, (default_cpumap.empty()) ? cpumap_count : 0);
			}

			if (globals.should_print_events()) {
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
	} else {
		log_msg(ASL_LEVEL_ERR, "Unable to read from %s\n", path.c_str());
		exit(1);
	}
}

void TraceFileAction::execute(Globals& globals) {
	if (globals.is_kernel_size_set()) {
		try {
			if (globals.kernel_size() == KernelSize::k32)
				execute_arch_specific<Kernel32>(globals, _path);
			else
				execute_arch_specific<Kernel64>(globals, _path);
		} catch (Exception& e) {
			log_msg(ASL_LEVEL_ERR, "An exception was raised: %s", e.what());
			log_msg(ASL_LEVEL_ERR, "An explicit kernel size was set, you may want to try not forcing the size to a single value\n");
			log_msg(ASL_LEVEL_ERR, "You may also want to check the number of cpus and iops configured if the file is from a device and does not have a cpumap\n");
		}
	} else {
		// Try em both!
		try {
			execute_arch_specific<Kernel64>(globals, _path);
		} catch (Exception& e) {
			execute_arch_specific<Kernel32>(globals, _path);
		}
	}
}

#endif

template <typename SIZE>
static void execute_arch_specific(Globals& globals, TraceFile& file, std::string& path)
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

		if (globals.is_verbose()) {
			dprintf(globals.output_fd(), "\n%s\n", path.c_str());
			print_verbose_machine_info(globals, machine, file.threadmap_count(), file.cpumap_count());
		}

		if (globals.should_print_events()) {
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

void TraceFileAction::execute(Globals& globals) {
        TraceFile file(_path.c_str(), globals.should_presort_events(), globals.cpu_count(), globals.iop_count());
        if (globals.is_kernel_size_set()) {
                try {
                        if (globals.kernel_size() == KernelSize::k32)
                                execute_arch_specific<Kernel32>(globals, file, _path);
                        else
                                execute_arch_specific<Kernel64>(globals, file, _path);
                } catch (Exception& e) {
                        log_msg(ASL_LEVEL_ERR, "An exception was raised: %s", e.what());
                        log_msg(ASL_LEVEL_ERR, "An explicit kernel size was set, you may want to try not forcing the size to a single value\n");
                        log_msg(ASL_LEVEL_ERR, "You may also want to check the number of cpus and iops configured if the file is from a device and does not have a cpumap\n");
                }
        } else {
                if (file.is_valid()) {
                        if (file.is_64_bit()) {
                                execute_arch_specific<Kernel64>(globals, file, _path);
                        } else {
                                execute_arch_specific<Kernel32>(globals, file, _path);
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

