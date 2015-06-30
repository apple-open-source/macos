//
//  SummaryPrinting.hpp
//  kdprof
//
//  Created by James McIlree on 4/19/13.
//  Copyright (c) 2013 Apple. All rights reserved.
//

#ifndef kdprof_Printing_hpp
#define kdprof_Printing_hpp

void print_summary_header(const Globals& globals);

struct SummaryLineData {
    protected:
	static constexpr const char* const indent_string[] = { "", "  ", "    ", "      " };
	static const uint32_t MAX_INDENT_LEVEL = 3; // Need to know this for time indenting to work correctly

	uint32_t		_indent_level;
	const char*		_name;

    public:

	enum class SummaryType {
		Unknown,
		CPU,
		Process,
		Thread
	};

	SummaryLineData(const char* name, uint32_t indent_level) :
	        _indent_level(indent_level),
		_name(name),
		should_print_timestamp(true),
		num_intr_events(0),
		context_switches(0),
		actual_process_count(0),
		wanted_process_count(0),
		actual_thread_count(0),
		wanted_thread_count(0),
		num_vm_fault_events(0),
		num_io_events(0),
		io_bytes_completed(0),
		num_jetsam_pids(0),
		percent_multiplier(100.0),
		type(SummaryType::Unknown),
		is_colored(false),
		begin_color(NULL),
		end_color(NULL)
	{
		ASSERT(_indent_level <= MAX_INDENT_LEVEL, "Sanity");
		ASSERT(_name && strlen(_name) > 0, "Sanity");
	}

	bool should_print_timestamp;
	AbsTime total_time;
	AbsTime total_run_time;
	AbsTime total_idle_time;
	AbsTime total_intr_time;
	AbsTime total_wanted_run_time;
	AbsTime total_wallclock_run_time;
	AbsTime total_all_cpus_idle_time;
	AbsTime total_vm_fault_time;
	AbsTime total_io_time;
	AbsTime total_jetsam_time;
	uint32_t num_intr_events;
	uint32_t context_switches;
	uint32_t actual_process_count;
	uint32_t wanted_process_count;
	uint32_t actual_thread_count;
	uint32_t wanted_thread_count;
	uint32_t num_vm_fault_events;
	uint32_t num_io_events;
	uint64_t io_bytes_completed;
	uint32_t num_jetsam_pids;
	double percent_multiplier;
	SummaryType type;
	bool is_colored;
	const char* begin_color;
	const char* end_color;

	const char* name()				{ return _name; }
	const char* outdent()				{ return indent_string[MAX_INDENT_LEVEL - _indent_level]; }
	const char* indent()				{ return indent_string[_indent_level]; }

	bool is_unknown()				{ return type == SummaryType::Unknown; }
	bool is_cpu()					{ return type == SummaryType::CPU; }
	bool is_process()				{ return type == SummaryType::Process; }
	bool is_thread()				{ return type == SummaryType::Thread; }
};

template <typename SIZE>
void print_summary_line(const Globals& globals, const Machine<SIZE>& machine, AbsInterval summary_interval, struct SummaryLineData& line_data)
{
	// Header is...
	//                                                                                                                                                         Avg     Actual     Wanted   Actual   Wanted                                                                       Jetsam
	//                                                                              All CPU                       Thr Avg        Actual        Wanted  Concurrency  Processes     To Run  Threads   To Run  VMFault       VMFault       IO Wait     # IO    IO Bytes     Jetsam    Proc
	// [Time(mS)]        Name                               Run%    Idle%    Intr%    Idle%    #Intr      #CSW  On CPU/µS        CPU/mS        CPU/mS      (# CPU)        Ran  Processes      Ran  Threads    Count     Time (mS)     Time (mS)      Ops   Completed  Time (mS)   Count
	// 123456789abcdef0  123456789012345678901234567890  1234567  1234567  1234567  1234567  1234567  12345678  123456789  123456789abc  123456789abc  123456789ab  123456789  123456789  1234567  1234567  1234567  123456789abc  123456789abc  1234567  1234567890  123456789  123456
	//    1119100000.00                                    76.58    16.53     6.89     0.00      230       112   10000.00     100000.00     100000.00         1.55          2          3       12       13     2280        230.48       1998.22     3318   123.40 MB       0.00

	ASSERT(!line_data.is_unknown(), "Sanity");

	//
	// It turns out that calling dprintf is very expensive; we're going to
	// accumulate to a string buffer and then flush once at the end.
	//
	char line[1024];
	char* cursor = line;
	char* line_end = line + sizeof(line);

	//
	// Begin line coloring (if any)
	//
	if (line_data.is_colored) {
		ASSERT(line_data.begin_color && line_data.end_color, "Sanity");
		cursor += snprintf(cursor, line_end - cursor, "%s", line_data.begin_color);

		if (cursor > line_end)
			cursor = line_end;
	}

	if (line_data.should_print_timestamp) {
		
		//
		// Time and Name get a special indent treatment, so they come out
		// as heirarchically aligned, while not disturbing the rest of the
		// columns. The time value is actually outdented, the name value
		// is indented.
		//
		// The result is that you get something like this:
		//
		//       [Time(mS)]  Name                               Run%
		// 123456789abcdef0  123456789012345678901234567890  1234567
		//
		//    1000.00        INDENT-LEVEL-0                    ##.##
		//      1000.00        INDENT-LEVEL-1                  ##.##
		//        1000.00        INDENT-LEVEL-2                ##.##
		//          1000.00        INDENT-LEVEL-3              ##.##
		//

		char time_buffer[64];
		
		//
		// Time
		//
		if (globals.should_print_mach_absolute_timestamps()) {
			if (globals.beginning_of_time().value() == 0)
				snprintf(time_buffer, sizeof(time_buffer), "%llX%s", (summary_interval.location() - globals.beginning_of_time()).value(), line_data.outdent());
			else
				snprintf(time_buffer, sizeof(time_buffer), "%llu%s", (summary_interval.location() - globals.beginning_of_time()).value(), line_data.outdent());
		} else {
			NanoTime ntime = (summary_interval.location() - globals.beginning_of_time()).nano_time(globals.timebase());
			snprintf(time_buffer, sizeof(time_buffer), "%3.2f%s", (double)ntime.value() / 1000000.0, line_data.outdent());
		}

		cursor += snprintf(cursor, line_end - cursor, "%16s  ", time_buffer);

		if (cursor > line_end)
			cursor = line_end;
	}
	
	//
	// Name
	//

	{
		char name_buffer[64];
		snprintf(name_buffer, sizeof(name_buffer), "%s%s", line_data.indent(), line_data.name());

		cursor += snprintf(cursor, line_end - cursor, "%-30s  ", name_buffer);
		if (cursor > line_end)
			cursor = line_end;
	}

	//
	// Run% Idle% Intr% All-CPUs-Idle% #Intr
	//

	// Special case for process/thread summary lines, print idle/intr as "-";
	if (line_data.is_process() || line_data.is_thread()) {
		double run_percent = 0.0;

		if (line_data.total_time.value() > 0)
			run_percent = line_data.total_run_time.double_value() / line_data.total_time.double_value() * line_data.percent_multiplier;
		
		cursor += snprintf(cursor, line_end - cursor, "%7.2f  %7s  %7s  %7s  %7u  ",
				   run_percent,
				   "-",
				   "-",
				   "-",
				   line_data.num_intr_events);
	} else {
		ASSERT(line_data.total_time.value() > 0, "Sanity");

		cursor += snprintf(cursor, line_end - cursor, "%7.2f  %7.2f  %7.2f  %7.2f  %7u  ",
				   line_data.total_run_time.double_value() / line_data.total_time.double_value() * line_data.percent_multiplier,
				   line_data.total_idle_time.double_value() / line_data.total_time.double_value() * line_data.percent_multiplier,
				   line_data.total_intr_time.double_value() / line_data.total_time.double_value() * line_data.percent_multiplier,
				   line_data.total_all_cpus_idle_time.double_value() / line_data.total_time.double_value() * line_data.percent_multiplier,
				   line_data.num_intr_events);
	}

	if (cursor > line_end)
		cursor = line_end;

	//
	// #context-switches  avg-on-cpu/µS
	//
	if (line_data.context_switches > 0) {
		double avg_on_cpu_uS = (line_data.total_run_time / AbsTime(line_data.context_switches)).nano_time(globals.timebase()).value() / 1000.0;
		cursor += snprintf(cursor, line_end - cursor, "%8u  %9.2f  ", line_data.context_switches, avg_on_cpu_uS);
	} else {
		cursor += snprintf(cursor, line_end - cursor, "%8u  %9s  ", line_data.context_switches, "-");
	}

	if (cursor > line_end)
		cursor = line_end;

	//
	// Actual CPU/mS, Wanted CPU/mS
	//
	if (line_data.total_wanted_run_time > 0) {
		cursor += snprintf(cursor, line_end - cursor, "%12.2f  %12.2f  ",
				   (double)line_data.total_run_time.nano_time(globals.timebase()).value() / 1000000.0,
				   (double)(line_data.total_run_time + line_data.total_wanted_run_time).nano_time(globals.timebase()).value() / 1000000.0);
	} else {
		cursor += snprintf(cursor, line_end - cursor, "%12.2f  %12s  ",
				   (double)line_data.total_run_time.nano_time(globals.timebase()).value() / 1000000.0,
				   "-");
	}

	if (cursor > line_end)
		cursor = line_end;

	//
	// Proc Avg Concurrency
	//

	if (line_data.total_wallclock_run_time > 0) {
		cursor += snprintf(cursor, line_end - cursor, "%11.2f  ", (double)line_data.total_run_time.value() / (double)line_data.total_wallclock_run_time.value());
		// cursor += snprintf(cursor, line_end - cursor, "%11.2f  ", (double)line_data.total_wallclock_run_time.nano_time(globals.timebase()).value() / 1000000.0);
	} else {
		cursor += snprintf(cursor, line_end - cursor, "%11s  ", "-");
	}

	if (cursor > line_end)
		cursor = line_end;

	//
	// Actual Processes, Wanted Processes
	//
	if (line_data.is_thread()) {
		cursor += snprintf(cursor, line_end - cursor, "%9s  %9s  ", "-", "-");
	} else {
		if (line_data.total_run_time > 0 && line_data.total_wanted_run_time > 0) {
			cursor += snprintf(cursor, line_end - cursor, "%9u  %9u  ", (uint32_t)line_data.actual_process_count, (uint32_t)line_data.wanted_process_count);
		} else if (line_data.total_run_time > 0) {
			cursor += snprintf(cursor, line_end - cursor, "%9u  %9s  ", (uint32_t)line_data.actual_process_count, "-");
		} else if (line_data.total_wanted_run_time > 0) {
			cursor += snprintf(cursor, line_end - cursor, "%9s  %9u  ", "-", (uint32_t)line_data.wanted_process_count);
		} else {
			cursor += snprintf(cursor, line_end - cursor, "%9s  %9s  ", "-", "-");
		}
	}

	if (cursor > line_end)
		cursor = line_end;

	//
	// Actual Threads, Wanted Threads
	//
	if (line_data.total_run_time > 0 && line_data.total_wanted_run_time > 0) {
		cursor += snprintf(cursor, line_end - cursor, "%7u  %7u  ", (uint32_t)line_data.actual_thread_count, (uint32_t)line_data.wanted_thread_count);
	} else if (line_data.total_run_time > 0) {
		cursor += snprintf(cursor, line_end - cursor, "%7u  %7s  ", (uint32_t)line_data.actual_thread_count, "-");
	} else if (line_data.total_wanted_run_time > 0) {
		cursor += snprintf(cursor, line_end - cursor, "%7s  %7u  ", "-", (uint32_t)line_data.wanted_thread_count);
	} else {
		cursor += snprintf(cursor, line_end - cursor, "%7s  %7s  ", "-", "-");
	}

	if (cursor > line_end)
		cursor = line_end;


	//
	// #vmfaults, mS blocked in vmfault
	//
	if (line_data.num_vm_fault_events == 0 && line_data.total_vm_fault_time.value() == 0) {
		cursor += snprintf(cursor, line_end - cursor, "%7s  %12s  ", "-", "-");
	} else {
		cursor += snprintf(cursor, line_end - cursor, "%7u  %12.2f  ",
				   line_data.num_vm_fault_events,
				   (double)line_data.total_vm_fault_time.nano_time(globals.timebase()).value() / 1000000.0);
	}

	//
	// mS blocked on IO activity
	//
	if (line_data.total_io_time.value() == 0) {
		cursor += snprintf(cursor, line_end - cursor, "%12s  ", "-");
	} else {
		cursor += snprintf(cursor, line_end - cursor, "%12.2f  ",
				   (double)line_data.total_io_time.nano_time(globals.timebase()).value() / 1000000.0);
	}

	//
	// # IO operations
	//
	if (line_data.num_io_events == 0) {
		cursor += snprintf(cursor, line_end - cursor, "%7s  ", "-");
	} else {
		cursor += snprintf(cursor, line_end - cursor, "%7u  ", line_data.num_io_events);
	}

	//
	// IO bytes completed
	//
	if (line_data.io_bytes_completed == 0) {
		cursor += snprintf(cursor, line_end - cursor, "%10s  ", "-");
	} else {
		cursor += snprintf(cursor, line_end - cursor, "%10s  ", formated_byte_size(line_data.io_bytes_completed).c_str());
	}

	//
	// Jetsam time
	//
	if (line_data.total_jetsam_time == 0) {
		cursor += snprintf(cursor, line_end - cursor, "%9s  ", "-");
	} else {
		cursor += snprintf(cursor, line_end - cursor, "%9.2f  ",
				   (double)line_data.total_jetsam_time.nano_time(globals.timebase()).value() / 1000000.0);
	}

	//
	// Jetsam count
	//
	if (line_data.is_cpu()) {
		if (line_data.num_jetsam_pids == 0) {
			cursor += snprintf(cursor, line_end - cursor, "%6s", "-");
		} else {
			cursor += snprintf(cursor, line_end - cursor, "%6u", line_data.num_jetsam_pids);
		}
	} else {
		cursor += snprintf(cursor, line_end - cursor, "%6s", "");
	}

	//
	// End line coloring (if any)
	//
	if (line_data.is_colored) {
		cursor += snprintf(cursor, line_end - cursor, "%s", line_data.end_color);

		if (cursor > line_end)
			cursor = line_end;
	}

	dprintf(globals.output_fd(), "%s\n", line);
}

template <typename SIZE>
void print_cpu_summary_with_name_and_indent(const Globals& globals, const Machine<SIZE>& machine, AbsInterval summary_interval, const CPUSummary<SIZE>& master_summary, const CPUSummary<SIZE>& cpu_summary, const char* name, uint32_t indent)
{
	struct SummaryLineData data(name, indent);

	data.should_print_timestamp = (globals.is_summary_start_set() || globals.is_summary_stop_set() || globals.is_summary_step_set());
	data.total_time = master_summary.total_time();
	data.total_run_time = cpu_summary.total_run_time();
	data.total_idle_time = cpu_summary.total_idle_time();
	data.total_intr_time = cpu_summary.total_intr_time();
	data.total_wanted_run_time = cpu_summary.total_future_run_time();
	data.total_wallclock_run_time = cpu_summary.total_wallclock_run_time();
	data.total_all_cpus_idle_time = cpu_summary.total_all_cpus_idle_time();
	data.total_vm_fault_time = cpu_summary.total_vm_fault_time();
	data.total_io_time = cpu_summary.total_io_time();
	data.total_jetsam_time = cpu_summary.total_jetsam_time();
	data.context_switches = cpu_summary.context_switches();
	data.num_intr_events = cpu_summary.num_intr_events();
	data.num_vm_fault_events = cpu_summary.num_vm_fault_events();
	data.num_io_events = cpu_summary.num_io_events();
	data.num_jetsam_pids = cpu_summary.num_processes_jetsammed();
	data.io_bytes_completed = cpu_summary.io_bytes_completed();
	data.type = SummaryLineData::SummaryType::CPU;

	for (auto& process_summary : cpu_summary.process_summaries()) {

		if (process_summary.total_run_time() > 0) {
			data.actual_process_count++;
			data.wanted_process_count++;
		} else if (process_summary.total_future_run_time() > 0) {
			data.wanted_process_count++;
		} else {
			// ASSERT(cpu_summary.total_vm_fault_time() > 0, "Process in summary no actual or wanted run time, and no vm_fault time");
		}

		for (auto& thread_summary : process_summary.thread_summaries()) {
			if (thread_summary.total_run_time() > 0) {
				data.actual_thread_count++;
				data.wanted_thread_count++;
			} else if (thread_summary.total_future_run_time() > 0) {
				data.wanted_thread_count++;
			} else {
				// ASSERT((thread_summary.total_vm_fault_time() > 0) || (thread_summary.total_pgin_time() > 0), "Thread in summary no actual or wanted run time, and no vm_fault or pgin time");
			}
		}
	}

	data.percent_multiplier *= (double)master_summary.active_cpus();

	print_summary_line(globals, machine, summary_interval, data);
}

template <typename SIZE>
void print_process_summary_with_name_and_indent(const Globals& globals, const Machine<SIZE>& machine, AbsInterval summary_interval, const CPUSummary<SIZE>& master_summary, const ProcessSummary<SIZE>& process_summary, const char* name, uint32_t indent)
{
	struct SummaryLineData data(name, indent);

	data.should_print_timestamp = (globals.is_summary_start_set() || globals.is_summary_stop_set() || globals.is_summary_step_set());
	data.total_run_time = process_summary.total_run_time();
	data.total_wanted_run_time = process_summary.total_future_run_time();
	data.total_wallclock_run_time = process_summary.total_wallclock_run_time();
	data.total_vm_fault_time = process_summary.total_vm_fault_time();
	data.total_io_time = process_summary.total_io_time();
	data.total_jetsam_time = process_summary.total_jetsam_time();
	data.context_switches = process_summary.context_switches();
	data.num_intr_events = process_summary.num_intr_events();
	data.actual_process_count = 1;
	data.wanted_process_count = 1;
	data.num_vm_fault_events = process_summary.num_vm_fault_events();
	data.num_io_events = process_summary.num_io_events();
	data.num_jetsam_pids = process_summary.num_processes_jetsammed();
	data.io_bytes_completed = process_summary.io_bytes_completed();
	data.total_time = master_summary.total_time();
	// This causes the line printer to put "-" in the idle and intr % columns.
	data.type = SummaryLineData::SummaryType::Process;
	data.percent_multiplier *= (double)master_summary.active_cpus();

	// We have to walk the threads to decide actual vs wanted to run
	for (auto& thread_summary : process_summary.thread_summaries()) {
		if (thread_summary.total_run_time() > 0) {
			data.actual_thread_count++;
			data.wanted_thread_count++;
		} else if (thread_summary.total_future_run_time() > 0) {
			data.wanted_thread_count++;
		} else {
			// ASSERT(thread_summary.total_vm_fault_time() > 0, "Thread in summary no actual or wanted run time, and no vm_fault time");
		}
	}

	print_summary_line(globals, machine, summary_interval, data);
}

template <typename SIZE>
void print_thread_summary_with_name_and_indent(const Globals& globals, const Machine<SIZE>& machine, AbsInterval summary_interval, const CPUSummary<SIZE>& master_summary, const ThreadSummary<SIZE>& thread_summary, const char* name, uint32_t indent)
{
	struct SummaryLineData data(name, indent);

	/*data.is_colored = true;
	 data.begin_color = TerminalColorStringFor(kTerminalColor::GREEN, true, false);
	 data.end_color = TerminalColorResetString();*/

	data.should_print_timestamp = (globals.is_summary_start_set() || globals.is_summary_stop_set() || globals.is_summary_step_set());
	data.total_run_time = thread_summary.total_run_time();
	data.total_wanted_run_time = thread_summary.total_future_run_time();
	data.total_vm_fault_time = thread_summary.total_vm_fault_time();
	data.total_io_time = thread_summary.total_io_time();
	data.total_jetsam_time = thread_summary.total_jetsam_time();
	data.context_switches = thread_summary.context_switches();
	data.num_intr_events = thread_summary.num_intr_events();
	data.num_vm_fault_events = thread_summary.num_vm_fault_events();
	data.num_io_events = thread_summary.num_io_events();
	data.num_jetsam_pids = 0;
	data.io_bytes_completed = thread_summary.io_bytes_completed();
	data.total_time = master_summary.total_time();
	data.percent_multiplier *= (double)master_summary.active_cpus();
	data.actual_thread_count = 1;
	data.wanted_thread_count = 1;

	// This causes the line printer to put "-" in various columns that don't make sense for a thread summary
	data.type = SummaryLineData::SummaryType::Thread;

	print_summary_line(globals, machine, summary_interval, data);
}

template <typename SIZE>
static void sort_processes(const Globals& globals, const CPUSummary<SIZE>& summary, std::vector<const MachineProcess<SIZE>*>& processes) {
	switch (globals.sort_key()) {
		case kSortKey::CPU:
			// Sort by Actual CPU, Future CPU, pid
			std::sort(processes.begin(), processes.end(), [&summary](const MachineProcess<SIZE>* p0, const MachineProcess<SIZE>* p1) -> bool {
				auto p0_summary = summary.process_summary(p0);
				auto p1_summary = summary.process_summary(p1);

				AbsTime p0_run_time = p0_summary->total_run_time();
				AbsTime p1_run_time = p1_summary->total_run_time();

				if (p0_run_time == p1_run_time) {
					AbsTime p0_future_run_time = p0_summary->total_future_run_time();
					AbsTime p1_future_run_time = p1_summary->total_future_run_time();

					if (p0_future_run_time == p1_future_run_time)
						return p0->pid() < p1->pid();

					return p1_future_run_time < p0_future_run_time;
				}

				return p1_run_time < p0_run_time;
			});
			break;

		case kSortKey::VMFault:
			// Sort by VMFault time, #-faults, pid
			std::sort(processes.begin(), processes.end(), [&summary](const MachineProcess<SIZE>* p0, const MachineProcess<SIZE>* p1) -> bool {
				auto p0_summary = summary.process_summary(p0);
				auto p1_summary = summary.process_summary(p1);

				AbsTime p0_vm_fault_time = p0_summary->total_vm_fault_time();
				AbsTime p1_vm_fault_time = p1_summary->total_vm_fault_time();

				if (p0_vm_fault_time == p1_vm_fault_time) {
					uint32_t p0_vm_fault_count = p0_summary->num_vm_fault_events();
					uint32_t p1_vm_fault_count = p1_summary->num_vm_fault_events();

					if (p0_vm_fault_count == p1_vm_fault_count)
						return p0->pid() < p1->pid();

					return p1_vm_fault_count < p0_vm_fault_count;
				}

				return p1_vm_fault_time < p0_vm_fault_time;
			});
			break;

		case kSortKey::IO_Wait:
			// Sort by IO time, pid
			std::sort(processes.begin(), processes.end(), [&summary](const MachineProcess<SIZE>* p0, const MachineProcess<SIZE>* p1) -> bool {
				auto p0_summary = summary.process_summary(p0);
				auto p1_summary = summary.process_summary(p1);

				AbsTime p0_io_time = p0_summary->total_io_time();
				AbsTime p1_io_time = p1_summary->total_io_time();

				if (p0_io_time == p1_io_time) {
					uint32_t p0_io_ops = p0_summary->num_io_events();
					uint32_t p1_io_ops = p1_summary->num_io_events();

					if (p0_io_ops == p1_io_ops)
						return p0->pid() < p1->pid();

					return p1_io_ops < p0_io_ops;
				}

				return p1_io_time < p0_io_time;
			});
			break;

		case kSortKey::IO_Ops:
			// Sort by IO time, pid
			std::sort(processes.begin(), processes.end(), [&summary](const MachineProcess<SIZE>* p0, const MachineProcess<SIZE>* p1) -> bool {
				auto p0_summary = summary.process_summary(p0);
				auto p1_summary = summary.process_summary(p1);

				uint32_t p0_io_ops = p0_summary->num_io_events();
				uint32_t p1_io_ops = p1_summary->num_io_events();

				if (p0_io_ops == p1_io_ops) {
					AbsTime p0_io_time = p0_summary->total_io_time();
					AbsTime p1_io_time = p1_summary->total_io_time();

					if (p0_io_time == p1_io_time)
						return p0->pid() < p1->pid();

					return p1_io_time < p0_io_time;
				}

				return p1_io_ops < p0_io_ops;
			});
			break;

		case kSortKey::IO_Size:
			// Sort by IO time, pid
			std::sort(processes.begin(), processes.end(), [&summary](const MachineProcess<SIZE>* p0, const MachineProcess<SIZE>* p1) -> bool {
				auto p0_summary = summary.process_summary(p0);
				auto p1_summary = summary.process_summary(p1);

				uint64_t p0_io_bytes_completed = p0_summary->io_bytes_completed();
				uint64_t p1_io_bytes_completed = p1_summary->io_bytes_completed();

				if (p0_io_bytes_completed == p1_io_bytes_completed) {
					AbsTime p0_io_time = p0_summary->total_io_time();
					AbsTime p1_io_time = p1_summary->total_io_time();

					if (p0_io_time == p1_io_time)
						return p0->pid() < p1->pid();

					return p1_io_time < p0_io_time;
				}

				return p1_io_bytes_completed < p0_io_bytes_completed;
			});
			break;

		case kSortKey::ID:
			// Sort by pid
			std::sort(processes.begin(), processes.end(), [](const MachineProcess<SIZE>* p0, const MachineProcess<SIZE>* p1) -> bool {
				return p0->pid() < p1->pid();
			});
			break;
	}
}

template <typename SIZE>
static void sort_threads(const Globals& globals, const ProcessSummary<SIZE>& summary, std::vector<const MachineThread<SIZE>*>& threads) {
	switch (globals.sort_key()) {
		case kSortKey::CPU:
			std::sort(threads.begin(), threads.end(), [&summary](const MachineThread<SIZE>* t0, const MachineThread<SIZE>* t1) -> bool {
				auto t0_summary = summary.thread_summary(t0);
				auto t1_summary = summary.thread_summary(t1);

				AbsTime t0_run_time = t0_summary->total_run_time();
				AbsTime t1_run_time = t1_summary->total_run_time();

				if (t0_run_time == t1_run_time) {
					AbsTime t0_future_run_time = t0_summary->total_future_run_time();
					AbsTime t1_future_run_time = t1_summary->total_future_run_time();

					if (t0_future_run_time == t1_future_run_time)
						return t0->tid() < t1->tid();

					return t1_future_run_time < t0_future_run_time;
				}

				return t1_run_time < t0_run_time;
			});
			break;

		case kSortKey::VMFault:
			// Sort by VMFault time, #-faults, pid
			std::sort(threads.begin(), threads.end(), [&summary](const MachineThread<SIZE>* t0, const MachineThread<SIZE>* t1) -> bool {
				auto t0_summary = summary.thread_summary(t0);
				auto t1_summary = summary.thread_summary(t1);

				AbsTime t0_vm_fault_time = t0_summary->total_vm_fault_time();
				AbsTime t1_vm_fault_time = t1_summary->total_vm_fault_time();

				if (t0_vm_fault_time == t1_vm_fault_time) {
					uint32_t t0_vm_fault_count = t0_summary->num_vm_fault_events();
					uint32_t t1_vm_fault_count = t1_summary->num_vm_fault_events();

					if (t0_vm_fault_count == t1_vm_fault_count)
						return t0->tid() < t1->tid();

					return t1_vm_fault_count < t0_vm_fault_count;
				}

				return t1_vm_fault_time < t0_vm_fault_time;
			});
			break;

		case kSortKey::IO_Wait:
			// Sort by IO time, pid
			std::sort(threads.begin(), threads.end(), [&summary](const MachineThread<SIZE>* t0, const MachineThread<SIZE>* t1) -> bool {
				auto t0_summary = summary.thread_summary(t0);
				auto t1_summary = summary.thread_summary(t1);

				AbsTime t0_io_time = t0_summary->total_io_time();
				AbsTime t1_io_time = t1_summary->total_io_time();

				if (t0_io_time == t1_io_time) {
					uint32_t t0_io_ops = t0_summary->num_io_events();
					uint32_t t1_io_ops = t1_summary->num_io_events();

					if (t0_io_ops == t1_io_ops)
						return t0->tid() < t1->tid();

					return t1_io_ops < t0_io_ops;
				}

				return t1_io_time < t0_io_time;
			});
			break;

		case kSortKey::IO_Ops:
			// Sort by IO time, pid
			std::sort(threads.begin(), threads.end(), [&summary](const MachineThread<SIZE>* t0, const MachineThread<SIZE>* t1) -> bool {
				auto t0_summary = summary.thread_summary(t0);
				auto t1_summary = summary.thread_summary(t1);

				uint32_t t0_io_ops = t0_summary->num_io_events();
				uint32_t t1_io_ops = t1_summary->num_io_events();

				if (t0_io_ops == t1_io_ops) {
					AbsTime t0_io_time = t0_summary->total_io_time();
					AbsTime t1_io_time = t1_summary->total_io_time();

					if (t0_io_time == t1_io_time)
						return t0->tid() < t1->tid();
					
					return t1_io_time < t0_io_time;
				}
				
				return t1_io_ops < t0_io_ops;
			});
			break;

		case kSortKey::IO_Size:
			// Sort by IO time, pid
			std::sort(threads.begin(), threads.end(), [&summary](const MachineThread<SIZE>* t0, const MachineThread<SIZE>* t1) -> bool {
				auto t0_summary = summary.thread_summary(t0);
				auto t1_summary = summary.thread_summary(t1);

				uint64_t t0_io_bytes_completed = t0_summary->io_bytes_completed();
				uint64_t t1_io_bytes_completed = t1_summary->io_bytes_completed();

				if (t0_io_bytes_completed == t1_io_bytes_completed) {
					AbsTime t0_io_time = t0_summary->total_io_time();
					AbsTime t1_io_time = t1_summary->total_io_time();

					if (t0_io_time == t1_io_time)
						return t0->tid() < t1->tid();

					return t1_io_time < t0_io_time;
				}

				return t1_io_bytes_completed < t0_io_bytes_completed;
			});
			break;

		case kSortKey::ID:
			std::sort(threads.begin(), threads.end(), [](const MachineThread<SIZE>* t0, const MachineThread<SIZE>* t1) -> bool {
				return t0->tid() < t1->tid();
			});
			break;
	}
}

template <typename SIZE>
void print_machine_summary(const Globals& globals, const Machine<SIZE>& machine) {
	AbsInterval machine_timespan = machine.timespan();

	AbsTime start(globals.summary_start(machine_timespan));
	AbsTime stop(globals.summary_stop(machine_timespan));
	AbsTime step(globals.summary_step(machine_timespan));

	print_summary_header(globals);

	AbsInterval start_stop_timespan(start, stop - start);
	AbsInterval clipped_start_stop_timespan(start_stop_timespan.intersection_range(machine_timespan));

	start = clipped_start_stop_timespan.location();
	stop = clipped_start_stop_timespan.max();
	
	while (start < stop) {
		AbsInterval base_interval(start, step);
		AbsInterval summary_interval(base_interval.intersection_range(clipped_start_stop_timespan));

		//
		// TOTAL summary
		//
		CPUSummary<SIZE> summary = machine.summary_for_timespan(summary_interval, NULL);

		//
		// We want the TOTAL to include the number of ms elapsed, so print a duration
		//
		char total_buffer[64];
		if (globals.should_print_mach_absolute_timestamps()) {
			if (globals.beginning_of_time().value() == 0)
				snprintf(total_buffer, sizeof(total_buffer), "TOTAL (0x%llXmabs)", summary_interval.length().value());
			else
				snprintf(total_buffer, sizeof(total_buffer), "TOTAL (%llumabs)", summary_interval.length().value());
		} else {
			NanoTime ntime = summary_interval.length().nano_time(globals.timebase());
			snprintf(total_buffer, sizeof(total_buffer), "TOTAL (%3.2fms)", (double)ntime.value() / 1000000.0);
		}
		print_cpu_summary_with_name_and_indent(globals, machine, summary_interval, summary, summary, total_buffer, 0);

		std::vector<CPUSummary<SIZE>> per_cpu_summaries;

		//
		// TOTAL per cpu summary
		//
		if (globals.should_print_cpu_summaries()) {
			// summary.cpus() is unordered, we want to display sorted by cpu_id.
			std::vector<const MachineCPU<SIZE>*> sorted_cpus;

			for (auto& cpu : summary.cpus()) {
				sorted_cpus.emplace_back(cpu);
			}

			std::sort(sorted_cpus.begin(), sorted_cpus.end(), [](MachineCPU<SIZE> const* cpu0, MachineCPU<SIZE> const* cpu1) -> bool {
				return cpu0->id() < cpu1->id();
			});

			for (auto cpu : sorted_cpus) {
				per_cpu_summaries.push_back(machine.summary_for_timespan(summary_interval, cpu));

				char name[16];
				snprintf(name, sizeof(name), "CPU%d", cpu->id());
				print_cpu_summary_with_name_and_indent(globals, machine, summary_interval, summary, per_cpu_summaries.back(), name, 1);
			}
		}

		//
		// PER PROCESS summary
		//
		if (globals.should_print_process_summaries()) {
			//
			// We want to sort the list of processes by PID, so they always display in the same order.
			//
			std::vector<const MachineProcess<SIZE>*> sorted_processes;
			for (auto& process_summary : summary.process_summaries()) {
				sorted_processes.emplace_back(process_summary.process());
			}

			sort_processes(globals, summary, sorted_processes);

			for (auto process : sorted_processes) {
				ASSERT(summary.process_summary(process), "Unable to find process summary by pointer lookup");
				if (const ProcessSummary<SIZE>* process_summary = summary.process_summary(process)) {
					char name[32];
					snprintf(name, sizeof(name), "%s (%d)%s", process->name(), process->pid(), process->is_exit_by_jetsam() ? " *" : "");
					print_process_summary_with_name_and_indent(globals, machine, summary_interval, summary, *process_summary, name, 1);

					if (globals.should_print_cpu_summaries()) {
						//
						// PER PROCESS per cpu summary
						//
						for (auto& cpu_summary : per_cpu_summaries) {
							if (const ProcessSummary<SIZE>* per_cpu_process_summary = cpu_summary.process_summary(process)) {
								char name[32];
								snprintf(name, sizeof(name), "CPU%d %s (%d)", (*cpu_summary.cpus().begin())->id(), process->name(), process->pid());
								print_process_summary_with_name_and_indent(globals, machine, summary_interval, summary, *per_cpu_process_summary, name, 2);
							}
						}
					}

					if (globals.should_print_thread_summaries()) {
						//
						// PER PROCESS per thread summary
						//
						std::vector<const MachineThread<SIZE>*> sorted_threads;
						for (auto& thread_summary : process_summary->thread_summaries()) {
							sorted_threads.emplace_back(thread_summary.thread());
						}

						sort_threads(globals, *process_summary, sorted_threads);

						for (auto thread : sorted_threads) {
							ASSERT(process_summary->thread_summary(thread), "Unable to find thread summary by pointer lookup");
							if (const ThreadSummary<SIZE>* thread_summary = process_summary->thread_summary(thread)) {
								char name[32];
								snprintf(name, sizeof(name), "tid-%llX", (uint64_t)thread->tid());
								print_thread_summary_with_name_and_indent(globals, machine, summary_interval, summary, *thread_summary, name, 2);

								if (globals.should_print_cpu_summaries()) {
									//
									// PER PROCESS per thread per cpu summary
									//
									for (auto& cpu_summary : per_cpu_summaries) {
										if (const ProcessSummary<SIZE>* per_cpu_process_summary = cpu_summary.process_summary(process)) {
											if (const ThreadSummary<SIZE>* per_cpu_thread_summary = per_cpu_process_summary->thread_summary(thread)) {
												char name[32];
												snprintf(name, sizeof(name), "CPU%d tid-%llX", (*cpu_summary.cpus().begin())->id(), (uint64_t)thread->tid());
												print_thread_summary_with_name_and_indent(globals, machine, summary_interval, summary, *per_cpu_thread_summary, name, 3);
											}
										}
									}
								}
								
							}
						}
					}
				}
			}
		}

		start += step;
	}
}


template <typename SIZE>
void print_machine_csv_summary_header(const Globals& globals,
				      const Machine<SIZE>& machine,
				      std::vector<const MachineCPU<SIZE>*>& all_cpus,
				      std::vector<const MachineProcess<SIZE>*>& all_processes,
				      std::unordered_map<const MachineProcess<SIZE>*, std::vector<const MachineThread<SIZE>*>>& all_threads,
				      const char* header_type)
{
	// Header is...
	//
	// "", header_type
	//
	// "", "TOTAL", "CPU0", "CPU1", "proc1", "proc1-tid1", "proc1-tid2", "proc2", etc..

	//
	// It turns out that calling dprintf is very expensive; we're going to
	// accumulate to a string buffer and then flush once at the end.
	//
	char line[16384]; // Header lines can be big!
	char* cursor = line;
	char* line_end = line + sizeof(line);

	//
	// header + TOTAL
	//
	cursor += snprintf(cursor, line_end - cursor, "%s\n\nTIME, TOTAL", header_type);
	if (cursor > line_end)
		cursor = line_end;
	
	//
	// TOTAL per cpu summary
	//
	if (globals.should_print_cpu_summaries()) {
		for (auto cpu : all_cpus) {
			cursor += snprintf(cursor, line_end - cursor, ", CPU%d", cpu->id());
			if (cursor > line_end)
				cursor = line_end;
		}
	}

	//
	// PER PROCESS summary
	//
	if (globals.should_print_process_summaries()) {
		for (auto process : all_processes) {
			cursor += snprintf(cursor, line_end - cursor, ", %s (%d)", process->name(), process->pid());
			if (cursor > line_end)
				cursor = line_end;

			if (globals.should_print_cpu_summaries()) {
				//
				// PER PROCESS per cpu summary
				//
				for (auto cpu : all_cpus) {
					cursor += snprintf(cursor, line_end - cursor, ", CPU%d %s (%d)", cpu->id(), process->name(), process->pid());
					if (cursor > line_end)
						cursor = line_end;
				}
			}

			if (globals.should_print_thread_summaries()) {
				//
				// PER PROCESS per thread summary
				//
				for (auto thread : all_threads[process]) {
					cursor += snprintf(cursor, line_end - cursor, ", tid-%llX", (uint64_t)thread->tid());
					if (cursor > line_end)
						cursor = line_end;

					//
					// PER PROCESS per thread per cpu summary
					//
					for (auto cpu : all_cpus) {
						cursor += snprintf(cursor, line_end - cursor, ", CPU%d tid-%llX", cpu->id(), (uint64_t)thread->tid());
						if (cursor > line_end)
							cursor = line_end;
					}
				}
			}
		}
	}

	dprintf(globals.output_fd(), "%s\n", line);
}

template <typename SIZE>
void print_machine_csv_summary_actual_cpu_ms_line(const Globals& globals,
						  const Machine<SIZE>& machine,
						  AbsInterval summary_interval,
						  std::vector<const MachineCPU<SIZE>*>& all_cpus,
						  std::vector<const MachineProcess<SIZE>*>& all_processes,
						  std::unordered_map<const MachineProcess<SIZE>*, std::vector<const MachineThread<SIZE>*>>& all_threads,
						  CPUSummary<SIZE>& master_summary,
						  std::vector<CPUSummary<SIZE>>& per_cpu_summaries)
{
	char line[16384]; // Header lines can be big!
	char* cursor = line;
	char* line_end = line + sizeof(line);

	//
	// Time
	//

	if (globals.should_print_mach_absolute_timestamps()) {
		if (globals.beginning_of_time().value() == 0)
			cursor += snprintf(cursor, line_end - cursor, "%llX", (summary_interval.location() - globals.beginning_of_time()).value());
		else
			cursor += snprintf(cursor, line_end - cursor, "%llu", (summary_interval.location() - globals.beginning_of_time()).value());
	} else {
		NanoTime ntime = (summary_interval.location() - globals.beginning_of_time()).nano_time(globals.timebase());
		cursor += snprintf(cursor, line_end - cursor, "%3.2f", (double)ntime.value() / 1000000.0);
	}
		
	if (cursor > line_end)
		cursor = line_end;

	//
	// TOTAL
	//
	cursor += snprintf(cursor, line_end - cursor, ", %3.2f",
			   (double)master_summary.total_run_time().nano_time(globals.timebase()).value() / 1000000.0);
	
	if (cursor > line_end)
		cursor = line_end;

	//
	// TOTAL per cpu summary
	//
	if (globals.should_print_cpu_summaries()) {
		for (auto& cpu_summary : per_cpu_summaries) {
			cursor += snprintf(cursor, line_end - cursor, ", %3.2f",
					   (double)cpu_summary.total_run_time().nano_time(globals.timebase()).value() / 1000000.0);

			if (cursor > line_end)
				cursor = line_end;
		}
	}

	//
	// PER PROCESS summary
	//
	if (globals.should_print_process_summaries()) {
		for (auto process : all_processes) {
			const ProcessSummary<SIZE>* process_summary;

			// Not all summaries will have a matching process entry!
			if ((process_summary = master_summary.process_summary(process))) {
				cursor += snprintf(cursor, line_end - cursor, ", %3.2f",
						   (double)process_summary->total_run_time().nano_time(globals.timebase()).value() / 1000000.0);
			} else {
				cursor += snprintf(cursor, line_end - cursor, ",");
			}

			if (cursor > line_end)
				cursor = line_end;

			if (globals.should_print_cpu_summaries()) {
				//
				// PER PROCESS per cpu summary
				//
				for (auto& cpu_summary : per_cpu_summaries) {
					if (const auto& process_summary = cpu_summary.process_summary(process)) {
						cursor += snprintf(cursor, line_end - cursor, ", %3.2f",
								   (double)process_summary->total_run_time().nano_time(globals.timebase()).value() / 1000000.0);
					} else {
						cursor += snprintf(cursor, line_end - cursor, ",");
					}
					
					if (cursor > line_end)
						cursor = line_end;
				}
			}

			if (globals.should_print_thread_summaries()) {
				//
				// PER PROCESS per thread summary
				//

				//
				// We again have to do a bit more work, sometime a process is missing and we still need to print empty slots for its threads.
			
				
				for (auto thread : all_threads[process]) {
					if (process_summary) {
						if (const auto& thread_summary = process_summary->thread_summary(thread)) {
							cursor += snprintf(cursor, line_end - cursor, ", %3.2f",
									   (double)thread_summary->total_run_time().nano_time(globals.timebase()).value() / 1000000.0);
						} else
							cursor += snprintf(cursor, line_end - cursor, ",");
					} else
						cursor += snprintf(cursor, line_end - cursor, ",");
					
					if (cursor > line_end)
						cursor = line_end;


					if (globals.should_print_cpu_summaries()) {
						//
						// PER PROCESS per thread per cpu summary
						//
						for (auto& cpu_summary : per_cpu_summaries) {
							if (const auto& per_cpu_process_summary = cpu_summary.process_summary(process)) {
								if (const auto& per_cpu_thread_summary = per_cpu_process_summary->thread_summary(thread)) {
									cursor += snprintf(cursor, line_end - cursor, ", %3.2f",
											   (double)per_cpu_thread_summary->total_run_time().nano_time(globals.timebase()).value() / 1000000.0);
								} else
									cursor += snprintf(cursor, line_end - cursor, ",");
							} else
								cursor += snprintf(cursor, line_end - cursor, ",");
							
							if (cursor > line_end)
								cursor = line_end;
						}
					}
				}
			}
		}
	}

	dprintf(globals.output_fd(), "%s\n", line);
}

template <typename SIZE>
void print_machine_csv_summary_wanted_cpu_ms_line(const Globals& globals,
						  const Machine<SIZE>& machine,
						  AbsInterval summary_interval,
						  std::vector<const MachineCPU<SIZE>*>& all_cpus,
						  std::vector<const MachineProcess<SIZE>*>& all_processes,
						  std::unordered_map<const MachineProcess<SIZE>*, std::vector<const MachineThread<SIZE>*>>& all_threads,
						  CPUSummary<SIZE>& master_summary,
						  std::vector<CPUSummary<SIZE>>& per_cpu_summaries)
{
	char line[16384]; // Header lines can be big!
	char* cursor = line;
	char* line_end = line + sizeof(line);

	//
	// Time
	//

	if (globals.should_print_mach_absolute_timestamps()) {
		if (globals.beginning_of_time().value() == 0)
			cursor += snprintf(cursor, line_end - cursor, "%llX", (summary_interval.location() - globals.beginning_of_time()).value());
		else
			cursor += snprintf(cursor, line_end - cursor, "%llu", (summary_interval.location() - globals.beginning_of_time()).value());
	} else {
		NanoTime ntime = (summary_interval.location() - globals.beginning_of_time()).nano_time(globals.timebase());
		cursor += snprintf(cursor, line_end - cursor, "%3.2f", (double)ntime.value() / 1000000.0);
	}

	if (cursor > line_end)
		cursor = line_end;

	//
	// TOTAL
	//
	cursor += snprintf(cursor, line_end - cursor, ", %3.2f",
			   (double)master_summary.total_future_run_time().nano_time(globals.timebase()).value() / 1000000.0);

	if (cursor > line_end)
		cursor = line_end;

	//
	// TOTAL per cpu summary
	//
	if (globals.should_print_cpu_summaries()) {
		for (auto& cpu_summary : per_cpu_summaries) {
			cursor += snprintf(cursor, line_end - cursor, ", %3.2f",
					   (double)cpu_summary.total_future_run_time().nano_time(globals.timebase()).value() / 1000000.0);

			if (cursor > line_end)
				cursor = line_end;
		}
	}

	//
	// PER PROCESS summary
	//
	if (globals.should_print_process_summaries()) {
		for (auto process : all_processes) {
			const ProcessSummary<SIZE>* process_summary;

			// Not all summaries will have a matching process entry!
			if ((process_summary = master_summary.process_summary(process))) {
				cursor += snprintf(cursor, line_end - cursor, ", %3.2f",
						   (double)process_summary->total_future_run_time().nano_time(globals.timebase()).value() / 1000000.0);
			} else {
				cursor += snprintf(cursor, line_end - cursor, ",");
			}

			if (cursor > line_end)
				cursor = line_end;

			if (globals.should_print_cpu_summaries()) {
				//
				// PER PROCESS per cpu summary
				//
				for (auto& cpu_summary : per_cpu_summaries) {
					if (const auto& process_summary = cpu_summary.process_summary(process)) {
						cursor += snprintf(cursor, line_end - cursor, ", %3.2f",
								   (double)process_summary->total_future_run_time().nano_time(globals.timebase()).value() / 1000000.0);
					} else {
						cursor += snprintf(cursor, line_end - cursor, ",");
					}

					if (cursor > line_end)
						cursor = line_end;
				}
			}

			if (globals.should_print_thread_summaries()) {
				//
				// PER PROCESS per thread summary
				//

				//
				// We again have to do a bit more work, sometime a process is missing and we still need to print empty slots for its threads.


				for (auto thread : all_threads[process]) {
					if (process_summary) {
						if (const auto& thread_summary = process_summary->thread_summary(thread)) {
							cursor += snprintf(cursor, line_end - cursor, ", %3.2f",
									   (double)thread_summary->total_future_run_time().nano_time(globals.timebase()).value() / 1000000.0);
						} else
							cursor += snprintf(cursor, line_end - cursor, ",");
					} else
						cursor += snprintf(cursor, line_end - cursor, ",");

					if (cursor > line_end)
						cursor = line_end;


					if (globals.should_print_cpu_summaries()) {
						//
						// PER PROCESS per thread per cpu summary
						//
						for (auto& cpu_summary : per_cpu_summaries) {
							if (const auto& per_cpu_process_summary = cpu_summary.process_summary(process)) {
								if (const auto& per_cpu_thread_summary = per_cpu_process_summary->thread_summary(thread)) {
									cursor += snprintf(cursor, line_end - cursor, ", %3.2f",
											   (double)per_cpu_thread_summary->total_future_run_time().nano_time(globals.timebase()).value() / 1000000.0);
								} else
									cursor += snprintf(cursor, line_end - cursor, ",");
							} else
								cursor += snprintf(cursor, line_end - cursor, ",");

							if (cursor > line_end)
								cursor = line_end;
						}
					}
				}
			}
		}
	}

	dprintf(globals.output_fd(), "%s\n", line);
}

template <typename SIZE>
void print_machine_csv_summary(const Globals& globals, const Machine<SIZE>& machine) {
	AbsInterval machine_timespan = machine.timespan();

	AbsTime start(globals.summary_start(machine_timespan));
	AbsTime stop(globals.summary_stop(machine_timespan));
	AbsTime step(globals.summary_step(machine_timespan));

	AbsInterval start_stop_timespan(start, stop - start);
	AbsInterval clipped_start_stop_timespan(start_stop_timespan.intersection_range(machine_timespan));

	start = clipped_start_stop_timespan.location();
	stop = clipped_start_stop_timespan.max();

	//
	// While printing a csv summary, we need to use the entire set of processes/threads/cpus
	// from the range, even though they may not run in each sample. We first gather a summary
	// for the entire time, to get the master list.
	//
	CPUSummary<SIZE> start_stop_summary = machine.summary_for_timespan(clipped_start_stop_timespan, NULL);

	std::vector<const MachineProcess<SIZE>*> all_processes;
	std::vector<const MachineCPU<SIZE>*> all_cpus;
	std::unordered_map<const MachineProcess<SIZE>*, std::vector<const MachineThread<SIZE>*>> all_threads;

	//
	// gather all processes
	//
	{
		for (auto& process_summary : start_stop_summary.process_summaries()) {
			all_processes.emplace_back(process_summary.process());
		}

		sort_processes(globals, start_stop_summary, all_processes);
	}

	//
	// gather all cpus
	//
	if (globals.should_print_cpu_summaries()) {
		for (auto& cpu : start_stop_summary.cpus()) {
			all_cpus.emplace_back(cpu);
		}

		std::sort(all_cpus.begin(), all_cpus.end(), [](MachineCPU<SIZE> const* cpu0, MachineCPU<SIZE> const* cpu1) -> bool {
			return cpu0->id() < cpu1->id();
		});
	}

	//
	// gather all threads
	//
	if (globals.should_print_thread_summaries()) {
		for (auto process : all_processes) {
			ASSERT(start_stop_summary.process_summary(process), "Unable to find process summary by pointer lookup");
			if (const ProcessSummary<SIZE>* process_summary = start_stop_summary.process_summary(process)) {
				//
				// PER PROCESS per thread summary
				//
				auto& sorted_threads = all_threads[process];
				for (auto& thread_summary : process_summary->thread_summaries()) {
					sorted_threads.emplace_back(thread_summary.thread());
				}

				sort_threads(globals, *process_summary, sorted_threads);
			}
		}
	}

	print_machine_csv_summary_header(globals, machine, all_cpus, all_processes, all_threads, "Actual CPU/ms");

	while (start < stop) {
		AbsInterval base_interval(start, step);
		AbsInterval summary_interval(base_interval.intersection_range(clipped_start_stop_timespan));

		//
		// TOTAL summary
		//
		CPUSummary<SIZE> summary = machine.summary_for_timespan(summary_interval, NULL);

		//
		// Per CPU summaries...
		//
		std::vector<CPUSummary<SIZE>> per_cpu_summaries;
		if (globals.should_print_cpu_summaries()) {
			for (auto cpu : all_cpus) {
				per_cpu_summaries.push_back(machine.summary_for_timespan(summary_interval, cpu));
			}
		}

		print_machine_csv_summary_actual_cpu_ms_line(globals, machine, summary_interval, all_cpus, all_processes, all_threads, summary, per_cpu_summaries);
		
		start += step;
	}


	//
	// Now print Wanted CPU/ms
	//
	start = clipped_start_stop_timespan.location();
	stop = clipped_start_stop_timespan.max();

	dprintf(globals.output_fd(), "\n");
	print_machine_csv_summary_header(globals, machine, all_cpus, all_processes, all_threads, "Wanted CPU/ms");

	while (start < stop) {
		AbsInterval base_interval(start, step);
		AbsInterval summary_interval(base_interval.intersection_range(clipped_start_stop_timespan));

		//
		// TOTAL summary
		//
		CPUSummary<SIZE> summary = machine.summary_for_timespan(summary_interval, NULL);
		
		//
		// Per CPU summaries...
		//
		std::vector<CPUSummary<SIZE>> per_cpu_summaries;
		if (globals.should_print_cpu_summaries()) {
			for (auto cpu : all_cpus) {
				per_cpu_summaries.push_back(machine.summary_for_timespan(summary_interval, cpu));
			}
		}

		print_machine_csv_summary_wanted_cpu_ms_line(globals, machine, summary_interval, all_cpus, all_processes, all_threads, summary, per_cpu_summaries);

		start += step;
	}
}

template <typename SIZE>
void print_process_start_stop_timestamps(const Globals& globals, const Machine<SIZE>& machine) {
	for (auto process : machine.processes()) {

		//
		// Skip processes with no events
		//

		if (process->timespan().length() == 0) {
			// Skip processes with nothing in them.
			// The assert may be too strong.
			ASSERT(process->is_created_by_thread_map(), "Expected a zero length process to be from the thread map");
			continue;
		}

		//
		// Don't print the kernel process, it will occupy the entire trace
		//
		if (process->is_kernel())
			continue;
		
		//
		// Time
		//
		char time_buffer[64];
		if (globals.beginning_of_time().value() == 0)
			snprintf(time_buffer, sizeof(time_buffer), "%llumabs", process->timespan().location().value());
		else
			snprintf(time_buffer, sizeof(time_buffer), "%llumabs", (process->timespan().location() - globals.beginning_of_time()).value());
	
		//
		// End time
		//
		char end_time_buffer[64];
		if (globals.beginning_of_time().value() == 0)
			snprintf(end_time_buffer, sizeof(end_time_buffer), "%llumabs", process->timespan().max().value());
		else
			snprintf(end_time_buffer, sizeof(end_time_buffer), "%llumabs", (process->timespan().max() - globals.beginning_of_time()).value());

		const char* create_reason;
		if (process->is_created_by_thread_map())
			create_reason = "Threadmap Entry";
		else if (process->is_created_by_previous_machine_state())
			create_reason = "Prev Machine State";
		else if (process->is_created_by_fork_exec())
			create_reason = "ForkExec";
		else if (process->is_created_by_exec())
			create_reason = "Exec";
		else
			create_reason = "???";

		if (globals.is_verbose()) {
			printf(" %30s (%6d)  --start %-16s --stop %-16s\tCreated by %-18s %s\n",
			       process->name(),
			       process->pid(),
			       time_buffer,
			       end_time_buffer,
			       create_reason,
			       process->is_trace_terminated() ? "EXITED" : "");
		} else {
			printf(" %30s (%6d)  --start %s --stop %s\n",
			       process->name(),
			       process->pid(),
			       time_buffer,
			       end_time_buffer);
		}
	}
}

template <typename SIZE>
void print_verbose_machine_info(const Globals& globals, const Machine<SIZE>& machine, uint32_t threadmap_count, uint32_t cpumap_count) {
	dprintf(globals.output_fd(), "\tEvent data is %s, and appears to be from %s\n", SIZE::is_64_bit ? "K64" : "K32", machine.is_ios() ? "iOS" : "OSX");
	dprintf(globals.output_fd(), "\tUsing a%stimebase of %d/%d\n", globals.is_timebase_set() ? " [User Set] " : " ", globals.timebase().numer, globals.timebase().denom);
	
	if (threadmap_count) {
		dprintf(globals.output_fd(), "\tA threadmap is present, and contains %u entries\n", threadmap_count);
	} else {
		dprintf(globals.output_fd(), "\tA threadmap is not present");
	}

	if (cpumap_count) {
		dprintf(globals.output_fd(), "\tA cpumap is present, and contains %u entries\n", cpumap_count);

	} else {
		dprintf(globals.output_fd(), "\tA cpumap is not present, the system provided a default with %u cpus and %u iops\n", globals.cpu_count(), globals.iop_count());
	}

	dprintf(globals.output_fd(), "\tFound %u active cpus in trace data\n", machine.active_cpus());

	if (globals.is_summary_start_set()) {
		AbsInterval machine_timespan = machine.timespan();

		if (globals.should_print_mach_absolute_timestamps()) {
			if (globals.beginning_of_time().value() == 0)
				dprintf(globals.output_fd(), "\tUsing a --start value of 0x%llXmabs (raw)\n", globals.summary_start(machine_timespan).value());
			else
				dprintf(globals.output_fd(), "\tUsing a --start value of %llumabs\n", (globals.summary_start(machine_timespan) - machine_timespan.location()).value());
		} else {
			NanoTime ntime = (globals.summary_start(machine_timespan) - machine_timespan.location()).nano_time(globals.timebase());
			dprintf(globals.output_fd(), "\tUsing a --start value of %3.2fms\n", (double)ntime.value() / 1000000.0);
		}
	}

	if (globals.is_summary_stop_set()) {
		AbsInterval machine_timespan = machine.timespan();

		if (globals.should_print_mach_absolute_timestamps()) {
			if (globals.beginning_of_time().value() == 0)
				dprintf(globals.output_fd(), "\tUsing a --stop value of 0x%llXmabs (raw)\n", globals.summary_stop(machine_timespan).value());
			else
				dprintf(globals.output_fd(), "\tUsing a --stop value of %llumabs\n", (globals.summary_stop(machine_timespan) - machine_timespan.location()).value());
		} else {
			NanoTime ntime = (globals.summary_stop(machine_timespan) - machine_timespan.location()).nano_time(globals.timebase());
			dprintf(globals.output_fd(), "\tUsing a --stop value of %3.2fms\n", (double)ntime.value() / 1000000.0);
		}
	}

	if (globals.is_summary_step_set()) {
		AbsInterval machine_timespan = machine.timespan();

		if (globals.should_print_mach_absolute_timestamps()) {
			if (globals.beginning_of_time().value() == 0)
				dprintf(globals.output_fd(), "\tUsing a --step value of 0x%llXmabs (raw)\n", globals.summary_step(machine_timespan).value());
			else
				dprintf(globals.output_fd(), "\tUsing a --step value of %llumabs\n", globals.summary_step(machine_timespan).value());
		} else {
			NanoTime ntime = globals.summary_step(machine_timespan).nano_time(globals.timebase());
			dprintf( globals.output_fd(), "\tUsing a --step value of %3.2fms\n", (double)ntime.value() / 1000000.0);
		}
	}
}

#endif
