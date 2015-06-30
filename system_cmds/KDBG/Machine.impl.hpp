//
//  Machine.impl.hpp
//  KDBG
//
//  Created by James McIlree on 10/30/12.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#include "KDebug.h"

template <typename SIZE>
bool process_by_time_sort(const MachineProcess<SIZE>* left, const MachineProcess<SIZE>* right) {
    return left->timespan().location() < right->timespan().location();
}

template <typename SIZE>
bool thread_by_time_sort(const MachineThread<SIZE>* left, const MachineThread<SIZE>* right) {
    return left->timespan().location() < right->timespan().location();
}

template <typename SIZE>
void Machine<SIZE>::post_initialize() {
    //
    // Post initialization. Sort various by time vectors, etc.
    //

    std::sort(_processes_by_time.begin(), _processes_by_time.end(), process_by_time_sort<SIZE>);
    std::sort(_threads_by_time.begin(), _threads_by_time.end(), thread_by_time_sort<SIZE>);

    // naked auto okay here, process is a ptr.
    AbsTime last_machine_timestamp = _events[_event_count-1].timestamp();
    for (auto process : _processes_by_time) {
	process->post_initialize(last_machine_timestamp);
    }

    //
    // Collapse the idle/intr/run queues into a single timeline
    //
    for (auto& cpu : _cpus) {
	cpu.post_initialize(timespan());
    }

    //
    // Flush any outstanding blocked events
    //
    for (auto& thread : _threads_by_tid) {
	thread.second.post_initialize(last_machine_timestamp);
    }

    //
    // Sort the IOActivity events, and build a flattened vector that can be used to find IO ranges for intersecting during interval searches
    //
    _io_by_uid.clear();
    std::sort(_all_io.begin(), _all_io.end());

    // We cannot use trange_vector_union to flatten _all_io, as _all_io isn't a plain TRange<AbsTime> type, and we want to yield that type.
    // cut & paste to the rescue! :-)
    if (!_all_io.empty()) {
	auto input_it = _all_io.begin();
	_all_io_active_intervals.push_back(*input_it);
	while (++input_it < _all_io.end()) {
	    TRange<AbsTime> union_range = _all_io_active_intervals.back();

	    if (union_range.intersects(*input_it)) {
		_all_io_active_intervals.pop_back();
		_all_io_active_intervals.push_back(union_range.union_range(*input_it));
	    } else {
		ASSERT(union_range < *input_it, "Out of order merging");
		_all_io_active_intervals.push_back(*input_it);
	    }
	}
    }

    //
    // Flush any outstanding MachMsg(s) in the nursery (state SEND)
    //
    // NOTE! We do not clear _mach_msg_nursery because its state is
    // forwarded to future Machine(s).
    //
    for (auto& nursery_it : _mach_msg_nursery) {
	auto& nursery_msg = nursery_it.second;
	if (nursery_msg.state() == kNurseryMachMsgState::Send) {
	    auto mach_msg_it = _mach_msgs.emplace(_mach_msgs.end(),
						  nursery_msg.id(),
						  nursery_msg.kmsg_addr(),
						  kMachineMachMsgFlag::HasSender,
						  nursery_msg.send_time(),
						  nursery_msg.send_tid(),
						  nursery_msg.send_msgh_bits(),
						  nursery_msg.send_voucher(),
						  AbsTime(0),
						  0,
						  0,
						  &Machine<SIZE>::UnsetVoucher);
	    _mach_msgs_by_event_index[nursery_msg.send_event_index()] = std::distance(_mach_msgs.begin(), mach_msg_it);
	}
    }

    //
    // Flush any outstanding Voucher(s) in the nursery
    //
    for (auto& nursery_it : _voucher_nursery) {

	//
	// First we need to "close" the open end of the live voucher's
	// timespan.
	//
	auto voucher = nursery_it.second.get();
	voucher->set_timespan_to_end_of_time();

	auto address = nursery_it.first;

	// First find the "row" for this address.
	auto by_addr_it = _vouchers_by_addr.find(address);
	if (by_addr_it == _vouchers_by_addr.end()) {
	    // No address entry case
	    std::vector<std::unique_ptr<MachineVoucher<SIZE>>> row;
	    row.emplace_back(std::move(nursery_it.second));
	    _vouchers_by_addr.emplace(address, std::move(row));
	} else {
	    auto& row = by_addr_it->second;

	    // Make sure these are sorted and non-overlapping
	    ASSERT(row.back()->timespan() < voucher->timespan(), "Sanity");
	    ASSERT(!row.back()->timespan().intersects(voucher->timespan()), "Sanity");

	    row.emplace_back(std::move(nursery_it.second));
	}
    }

    _voucher_nursery.clear();

    DEBUG_ONLY(validate());
}

template <typename SIZE>
void Machine<SIZE>::raw_initialize(const KDCPUMapEntry* cpumaps,
				   uint32_t cpumap_count,
				   const KDThreadMapEntry<SIZE>* threadmaps,
				   uint32_t threadmap_count,
				   const KDEvent<SIZE>* events,
				   uintptr_t event_count)
{
    ASSERT(cpumaps || cpumap_count == 0, "Sanity");
    ASSERT(threadmaps || threadmap_count == 0, "Sanity");
    ASSERT(events || event_count == 0, "Sanity");

    for (uint32_t i = 0; i < cpumap_count; ++i) {
	_cpus.emplace_back(i, cpumaps[i].is_iop(), cpumaps[i].name());
    }

    // We cannot create processes / threads unless we have at least one event to give us a timestamp.
    if (event_count) {
	AbsTime now = events[0].timestamp();

	_kernel_task = create_process(0, "kernel_task", now, kMachineProcessFlag::IsKernelProcess);

	// Initial thread state, nothing in nusery
	for (uint32_t index = 0; index < threadmap_count; ++index) {
	    auto& threadmap = threadmaps[index];

	    pid_t pid = threadmap.pid();

	    // The kernel threadmap often has empty entries. Skip them.
	    if (pid == 0)
		break;

	    if (pid == 1 && strncmp(threadmap.name(), "kernel_task", 12) == 0) {
		pid = 0;
	    }

	    MachineProcess<SIZE>* process = youngest_mutable_process(pid);
	    if (!process) {
		process = create_process(pid, threadmap.name(), now, kMachineProcessFlag::CreatedByThreadMap);
		ASSERT(process, "Sanity");
	    }
	    process->add_thread(create_thread(process, threadmap.tid(), &UnsetVoucher, now, kMachineThreadFlag::CreatedByThreadMap));
	}
    }

    // We need to know what the idle/INTR states of the CPU's are.
    initialize_cpu_idle_intr_states();

    for (uintptr_t index = 0; index < event_count; ++index) {
	if (!process_event(events[index]))
	    break;
    }

    post_initialize();
}

template <typename SIZE>
Machine<SIZE>::Machine(KDCPUMapEntry* cpumaps, uint32_t cpumap_count, KDThreadMapEntry<SIZE>* threadmaps, uint32_t threadmap_count, KDEvent<SIZE>* events, uintptr_t event_count) :
    _kernel_task(nullptr),
    _events(events),
    _event_count(event_count),
    _flags(0),
    _unknown_process_pid(-1)
{
    raw_initialize(cpumaps,
		   cpumap_count,
		   threadmaps,
		   threadmap_count,
		   events,
		   event_count);
}

template <typename SIZE>
Machine<SIZE>::Machine(const TraceFile& file) :
    _kernel_task(nullptr),
    _events(file.events<SIZE>()),
    _event_count(file.event_count()),
    _flags(0),
    _unknown_process_pid(-1)
{
    raw_initialize(file.cpumap(),
		   file.cpumap_count(),
		   file.threadmap<SIZE>(),
		   file.threadmap_count(),
		   file.events<SIZE>(),
		   file.event_count());
}

template <typename SIZE>
Machine<SIZE>::Machine(Machine<SIZE>& parent, KDEvent<SIZE>* events, uintptr_t event_count) :
    _kernel_task(nullptr),
    _events(events),
    _event_count(event_count),
    _flags(0),
    _unknown_process_pid(-1)
{
    ASSERT(events || event_count == 0, "Sanity");

    const std::vector<const MachineThread<SIZE>*>& parent_threads = parent.threads();
    const std::vector<const MachineCPU<SIZE>>& parent_cpus = parent.cpus();

    for (const MachineCPU<SIZE>& parent_cpu : parent_cpus) {
	_cpus.emplace_back(parent_cpu.id(), parent_cpu.is_iop(), parent_cpu.name());
    }

    // We cannot create processes / threads unless we have at least one event to give us a timestamp.
    if (event_count) {
	AbsTime now = events[0].timestamp();

	//
	// Forawd any live vouchers. This must done before forwarding threads
	// or MachMsgs from their nurseries, as they have references to the
	// vouchers.
	//
	for (auto& parent_vouchers_by_addr_it : parent._vouchers_by_addr) {
	    std::unique_ptr<MachineVoucher<SIZE>>& voucher = parent_vouchers_by_addr_it.second.back();
	    if (voucher->is_live()) {
		// When we flushed these vouchers in the previous machine state,
		// we set their timespans to infinite. We need to reset them in
		// case a close event arrives.
		voucher->set_timespan_to_zero_length();
		_voucher_nursery.emplace(voucher->address(), std::move(voucher));
	    }
	}

	_kernel_task = create_process(0, "kernel_task", now, kMachineProcessFlag::IsKernelProcess);

	for (const MachineThread<SIZE>* parent_thread : parent_threads) {
	    if (!parent_thread->is_trace_terminated()) {
		const MachineProcess<SIZE>& parent_process = parent_thread->process();
		MachineProcess<SIZE>* new_process = youngest_mutable_process(parent_process.pid());
		if (!new_process) {
		    kMachineProcessFlag new_process_flags = (kMachineProcessFlag)(parent_process.flags() | (uint32_t)kMachineProcessFlag::CreatedByPreviousMachineState);
		    new_process = create_process(parent_process.pid(), parent_process.name(), now, new_process_flags);
		    ASSERT(new_process, "Sanity");
		}
		new_process->add_thread(create_thread(new_process,
						      parent_thread->tid(),
						      thread_forwarding_voucher_lookup(parent_thread->last_voucher()),
						      now,
						      (kMachineThreadFlag)(parent_thread->flags() | (uint32_t)kMachineThreadFlag::CreatedByPreviousMachineState)));
	    }
	}

	// We need to know what the idle/INTR states of the CPU's are.
	//
	// Start by looking at the existing states.
	uint32_t init_count = 0;
	uint32_t ap_count = 0;
	for (const MachineCPU<SIZE>& parent_cpu : parent_cpus) {
	    if (!parent_cpu.is_iop()) {
		ap_count++;
		const std::vector<CPUActivity<SIZE>>& parent_cpu_timeline = parent_cpu.timeline();

		bool intr_initialized = false;
		bool idle_initialized = false;
		bool runq_initialized = false;

		MachineCPU<SIZE>& cpu = _cpus[parent_cpu.id()];

		for (auto reverse_it = parent_cpu_timeline.rbegin(); reverse_it < parent_cpu_timeline.rend(); ++reverse_it) {

		    // We can sometimes split two simultaneous events across two buffer snaps.
		    // IOW, buffer snap 1:
		    //
		    // event[N].timestamp = 1234;
		    //
		    // buffer snap 2:
		    //
		    // event[0].timestamp = 1234;
		    ASSERT(!reverse_it->contains(now) || reverse_it->max()-AbsTime(1) == now, "Sanity");
		    ASSERT(reverse_it->location() <= now, "Sanity");

		    switch (reverse_it->type()) {
			//
			// The states are separate, and heirarchical.
			// The order (low -> high) is:
			// Run, Idle, INTR
			// Unknown is a special state; we give up on seeing it.
			// A lower state precludes being in a higher state, but
			// not vice-versa. You can not be IDLE if you are currently
			// Run. You may be IDLE during Run.
			//

			case kCPUActivity::Unknown:
			    // Don't actually initialize anything, just force a bailout
			    runq_initialized = idle_initialized = intr_initialized = true;
			    break;

			// NOTE NOTE NOTE!
			//
			// Overly clever here, note the lack of "break" in the Run
			// and Idle clause, we fall through to initialize higher
			// states.
			case kCPUActivity::Run:
			    ASSERT(!runq_initialized, "This should always be the last level to initialize");
			    if (MachineThread<SIZE>* on_cpu_thread = youngest_mutable_thread(reverse_it->thread()->tid())) {
				cpu.initialize_thread_state(on_cpu_thread, now);
				init_count++;
			    } else {
				ASSERT(reverse_it->thread()->is_trace_terminated() , "We should find this thread unless its been removed");
			    }
			    runq_initialized = true;

			case kCPUActivity::Idle:
			    if (!idle_initialized) {
				cpu.initialize_idle_state(reverse_it->is_idle(), now);
				init_count++;
				idle_initialized = true;
			    }

			case kCPUActivity::INTR:
			    if (!intr_initialized) {
				cpu.initialize_intr_state(reverse_it->is_intr(), now);
				init_count++;
				intr_initialized = true;
			    }
			    break;
		    }

		    if (runq_initialized) {
			ASSERT(idle_initialized && intr_initialized, "Sanity");
			break;
		    }
		}
	    }
	}

	if (init_count < (ap_count * 3)) {
	    initialize_cpu_idle_intr_states();
	}

	//
	// Forward any messages from the nursery
	//

	for (auto& parent_nursery_it : parent._mach_msg_nursery) {
	    auto& parent_nursery_msg = parent_nursery_it.second;

	    switch (parent_nursery_msg.state()) {
		    // We forward send(s) because they can become receives.
		    // We forward the free's because they stop us from showing bogus kernel message receipts
		case kNurseryMachMsgState::Send:
		case kNurseryMachMsgState::Free: {
		    ASSERT(_mach_msg_nursery.find(parent_nursery_msg.kmsg_addr()) == _mach_msg_nursery.end(), "Duplicate kmsg address when forwarding mach_msg nursery from parent");

		    auto it = _mach_msg_nursery.emplace(parent_nursery_msg.kmsg_addr(), parent_nursery_msg);

		    // Grr, emplace returns a std::pair<it, bool>, and the it is std::pair<key, value>...

		    // We have to clear this to prevent bogus data being shown during a receive,
		    // the send event index is no longer available.
		    it.first->second.set_send_event_index(-1);
		    break;
		}

		default:
		    break;
	    }
	}
    }

    for (uintptr_t index = 0; index < event_count; ++index) {
	if (!process_event(_events[index]))
	    break;
    }

    post_initialize();
}

template <typename SIZE>
const MachineProcess<SIZE>* Machine<SIZE>::process(pid_t pid, AbsTime time) const {
    auto by_pid_range = _processes_by_pid.equal_range(pid);
    for (auto it = by_pid_range.first; it != by_pid_range.second; ++it) {
	const MachineProcess<SIZE>& process = it->second;
	if (process.timespan().contains(time)) {
	    return &process;
	}
    }

    return nullptr;
}

template <typename SIZE>
const MachineThread<SIZE>* Machine<SIZE>::thread(typename SIZE::ptr_t tid, AbsTime time) const {
    auto by_tid_range = _threads_by_tid.equal_range(tid);
    for (auto it = by_tid_range.first; it != by_tid_range.second; ++it) {
	const MachineThread<SIZE>& thread = it->second;
	if (thread.timespan().contains(time)) {
	    return &thread;
	}
    }

    return nullptr;
}

template <typename SIZE>
struct VoucherVsAbsTimeComparator {
    bool operator()(const std::unique_ptr<MachineVoucher<SIZE>>& voucher, const AbsTime time) const {
	return voucher->timespan().max() < time;
    }

    bool operator()(const AbsTime time, const std::unique_ptr<MachineVoucher<SIZE>>& voucher) const {
	return time < voucher->timespan().max();
    }
};

template <typename SIZE>
const MachineVoucher<SIZE>* Machine<SIZE>::voucher(typename SIZE::ptr_t address, AbsTime timestamp) const {
    // First find the "row" for this address.
    auto by_addr_it = _vouchers_by_addr.find(address);
    if (by_addr_it != _vouchers_by_addr.end()) {
	auto& row = by_addr_it->second;

	auto by_time_it = std::upper_bound(row.begin(), row.end(), timestamp, VoucherVsAbsTimeComparator<SIZE>());
	// The upper bound will report that 0 is lower than [ 10, 20 ), need to check contains!
	if (by_time_it != row.end()) {
	    // The compiler is having troubles seeing through an iterator that reflects unique_ptr methods which reflects MachineVoucher methods.
	    auto v = by_time_it->get();
	    if (v->timespan().contains(timestamp)) {
		return v;
	    }
	}
    }

    return nullptr;
}

template <typename SIZE>
const MachineMachMsg<SIZE>* Machine<SIZE>::mach_msg(uintptr_t event_index) const {
    auto it = _mach_msgs_by_event_index.find(event_index);
    if (it != _mach_msgs_by_event_index.end()) {
	return &_mach_msgs.at(it->second);
    }

    return nullptr;
}

#if !defined(NDEBUG) && !defined(NS_BLOCK_ASSERTIONS)
template <typename SIZE>
void Machine<SIZE>::validate() const {
    ASSERT(_events, "Sanity");
    ASSERT(_event_count, "Sanity");

    //
    // Event timestamp ordering is already pre-checked, no point in retesting it here.
    //

    ASSERT(_threads_by_tid.size() == _threads_by_time.size(), "Container views state not in sync");
    ASSERT(_processes_by_pid.size() == _processes_by_name.size(), "Container views state not in sync");
    ASSERT(_processes_by_pid.size() == _processes_by_time.size(), "Container views state not in sync");

    for (auto& pair : _processes_by_pid) {
	auto& process = pair.second;
	process.validate();
	AbsInterval process_timespan = process.timespan();
	for (auto thread : process.threads()) {
	    ASSERT(process_timespan.contains(thread->timespan()), "thread outside process timespan");
	}
    }

    for (auto thread_ptr : _threads_by_time) {
	thread_ptr->validate();
    }

    //
    // Make sure no process with the same pid overlaps in time
    //
    const MachineProcess<SIZE>* last_process = nullptr;
    for (auto& pair : _processes_by_pid) {
	auto& process = pair.second;
	if (last_process && last_process->pid() == process.pid()) {
	    // The < operator only checks ordering, it is not strict
	    // about overlap. We must check both
	    ASSERT(last_process->timespan() < process.timespan(), "Sanity");
	    ASSERT(!last_process->timespan().intersects(process.timespan()), "Sanity");
	}
	last_process = &process;
    }

    //
    // Make sure no thread with the same tid overlaps in time
    //
    const MachineThread<SIZE>* last_thread = nullptr;
    for (auto& pair : _threads_by_tid) {
	auto& thread = pair.second;
	if (last_thread && last_thread->tid() == thread.tid()) {
	    // The < operator only checks ordering, it is not strict
	    // about overlap. We must check both
	    ASSERT(last_thread->timespan() < thread.timespan(), "Sanity");
	    ASSERT(!last_thread->timespan().intersects(thread.timespan()), "Sanity");
	}
	last_thread = &thread;
    }

    ASSERT(is_trange_vector_sorted_and_non_overlapping(_all_io_active_intervals), "all io search/mask vector fails invariant");
}
#endif

template <typename SIZE>
const std::vector<const MachineProcess<SIZE>*>& Machine<SIZE>::processes() const {
    return *reinterpret_cast< const std::vector<const MachineProcess<SIZE>*>* >(&_processes_by_time);
}

template <typename SIZE>
const std::vector<const MachineThread<SIZE>*>& Machine<SIZE>::threads() const {
    return *reinterpret_cast< const std::vector<const MachineThread<SIZE>*>* >(&_threads_by_time);
}

template <typename SIZE>
const std::vector<const MachineCPU<SIZE>>& Machine<SIZE>::cpus() const {
    return *reinterpret_cast< const std::vector<const MachineCPU<SIZE>>* >(&_cpus);
}

template <typename SIZE>
AbsInterval Machine<SIZE>::timespan() const {
    if (_event_count) {
	AbsTime begin(_events[0].timestamp());
	AbsTime end(_events[_event_count-1].timestamp());
	return AbsInterval(begin, end - begin + AbsTime(1));
    }

    return AbsInterval(AbsTime(0),AbsTime(0));
}

template <typename SIZE>
CPUSummary<SIZE> Machine<SIZE>::summary_for_timespan(AbsInterval summary_timespan, const MachineCPU<SIZE>* summary_cpu) const {
    ASSERT(summary_timespan.intersects(timespan()), "Sanity");
    CPUSummary<SIZE> summary;

    uint32_t ap_cpu_count = 0;
    for (auto& cpu: _cpus) {
	// We don't know enough about iops to do anything with them.
	// Also skip cpus with no activity
	if (!cpu.is_iop() && cpu.is_active()) {
	    ap_cpu_count++;
	}
    }

    bool should_calculate_wallclock_run_time = (summary_cpu == NULL && ap_cpu_count > 1);

    summary.begin_cpu_timeline_walks();

    //
    // Lots of optimization possibilities here...
    //
    // We spend a LOT of time doing the set lookups to map from a thread/process to a ThreadSummary / ProcessSummary.
    // If we could somehow map directly from thread/process to the summary, this would speed up considerably.
    //

    for (auto& cpu: _cpus) {
	// We don't know enough about iops to do anything with them.
	// Also skip cpus with no activity
	if (!cpu.is_iop() && cpu.is_active()) {
	    if (summary_cpu == NULL || summary_cpu == &cpu) {

		summary.begin_cpu_timeline_walk(&cpu);

		auto& timeline = cpu.timeline();
		if (!timeline.empty()) {
		    AbsInterval timeline_timespan = AbsInterval(timeline.front().location(), timeline.back().max() - timeline.front().location());
		    AbsInterval trimmed_timespan = summary_timespan.intersection_range(timeline_timespan);

		    summary.incr_active_cpus();

		    auto start = cpu.activity_for_timestamp(trimmed_timespan.location());
		    auto end = cpu.activity_for_timestamp(trimmed_timespan.max()-AbsTime(1));

		    ASSERT(start && start->contains(trimmed_timespan.location()), "Sanity");
		    ASSERT(end && end->contains(trimmed_timespan.max()-AbsTime(1)), "Sanity");

		    ProcessSummary<SIZE>* process_summary = NULL;
		    ThreadSummary<SIZE>* thread_summary = NULL;

		    if (start->is_run() && !start->is_context_switch()) {
			const MachineThread<SIZE>* thread_on_cpu = start->thread();
			const MachineProcess<SIZE>* process_on_cpu = &thread_on_cpu->process();

			process_summary = summary.mutable_process_summary(process_on_cpu);
			thread_summary = process_summary->mutable_thread_summary(thread_on_cpu);
		    }

		    // NOTE! <=, not <,  because end is inclusive of data we want to count!
		    while (start <= end) {
			// NOTE! summary_timespan, NOT trimmed_timespan!
			AbsInterval t = start->intersection_range(summary_timespan);

			switch (start->type()) {
							case kCPUActivity::Unknown:
				// Only cpu summaries track unknown time
				summary.add_unknown_time(t.length());
				break;

							case kCPUActivity::Idle:
				summary.add_idle_time(t.length());
				summary.add_all_cpus_idle_interval(t);
				if (process_summary) process_summary->add_idle_time(t.length());
				if (thread_summary) thread_summary->add_idle_time(t.length());
				break;

							case kCPUActivity::INTR:
				summary.add_intr_time(t.length());
				// It might seem like we should add INTR time to the wallclock run time
				// for the top level summary, but the concurrency level is calculated as
				// Actual / Wallclock, where Actual only counts RUN time. If we add INTR
				// the results are skewed.
				if (process_summary) process_summary->add_intr_time(t.length());
				if (thread_summary) thread_summary->add_intr_time(t.length());
				break;

							case kCPUActivity::Run: {
							    // We must reset these each time. Consider the case where we have the following:
							    //
							    // RRRRRRRRIIIIIIIIIIIIIIIIRRRRRRRRRR
							    // ^                ^
							    // CSW              Summary starts here
							    //
							    // The first run seen in the summary will not be a CSW, and yet process/thread summary
							    // are NULL...

							    const MachineThread<SIZE>* thread_on_cpu = start->thread();
							    const MachineProcess<SIZE>* process_on_cpu = &thread_on_cpu->process();

							    process_summary = summary.mutable_process_summary(process_on_cpu);
							    thread_summary = process_summary->mutable_thread_summary(thread_on_cpu);

							    if (start->is_context_switch()) {
								summary.incr_context_switches();
								process_summary->incr_context_switches();
								thread_summary->incr_context_switches();
							    }

							    summary.add_run_time(t.length());
							    process_summary->add_run_time(t.length());
							    thread_summary->add_run_time(t.length());

							    // We only calculate wallclock run time if there is a chance
							    // it might differ from run time.
							    if (should_calculate_wallclock_run_time) {
								summary.add_wallclock_run_interval(t);
								process_summary->add_wallclock_run_interval(t);
							    }

							    break;
							}
			}

			start++;
		    }
		}

		summary.end_cpu_timeline_walk(&cpu);
	    }
	}
    }

    summary.end_cpu_timeline_walks();

    // We only attempt to calculate future summary data in limited circumstances
    // There must be enough future data to consistently decide if threads would run in the future.
    // If the summary_cpu is not "all" we do not attempt to calculate.

    if (summary_cpu == NULL) {
	AbsInterval future_timespan(summary_timespan.max(), summary_timespan.length() * AbsTime(5));
	if (future_timespan.intersection_range(timespan()).length() == future_timespan.length()) {
	    for (auto& cpu: _cpus) {
		// We don't know enough about iops to do anything with them
		if (!cpu.is_iop()) {
		    auto& timeline = cpu.timeline();

		    if (!timeline.empty()) {
			AbsInterval timeline_timespan = AbsInterval(timeline.front().location(), timeline.back().max() - timeline.front().location());
			AbsInterval trimmed_timespan = future_timespan.intersection_range(timeline_timespan);

			auto start = cpu.activity_for_timestamp(trimmed_timespan.location());
			auto end = cpu.activity_for_timestamp(trimmed_timespan.max()-AbsTime(1));

			ASSERT(start && start->contains(trimmed_timespan.location()), "Sanity");
			ASSERT(end && end->contains(trimmed_timespan.max()-AbsTime(1)), "Sanity");

			ProcessSummary<SIZE>* process_summary = NULL;
			ThreadSummary<SIZE>* thread_summary = NULL;

			// NOTE! <=, not <,  because end is inclusive of data we want to count!
			while (start <= end) {
							// NOTE! future_timespan, NOT trimmed_timespan!
							AbsInterval t = start->intersection_range(future_timespan);

							switch (start->type()) {
							    case kCPUActivity::Unknown:
								break;

							    case kCPUActivity::Idle:
								// On idle, we mark the current thread as blocked.
								if (thread_summary)
								    thread_summary->set_is_blocked_in_future();
								break;

							    case kCPUActivity::INTR:
								break;

							    case kCPUActivity::Run: {
								// We must reset these each time. Consider the case where we have the following:
								//
								// RRRRRRRRIIIIIIIIIIIIIIIIRRRRRRRRRR
								// ^                ^
								// CSW              Summary starts here
								//
								// The first run seen in the summary will not be a CSW, and yet process/thread summary
								// are NULL...

								const MachineThread<SIZE>* thread_on_cpu = start->thread();
								const MachineProcess<SIZE>* process_on_cpu = &thread_on_cpu->process();

								process_summary = summary.mutable_process_summary(process_on_cpu);
								thread_summary = process_summary->mutable_thread_summary(thread_on_cpu);

								if (!thread_summary->is_future_initialized()) {
								    thread_summary->set_future_initialized();
								    thread_summary->set_total_blocked_in_summary(thread_on_cpu->blocked_in_timespan(summary_timespan));
								    thread_summary->set_first_block_after_summary(thread_on_cpu->next_blocked_after(summary_timespan.max()));
								    ASSERT(thread_summary->total_blocked_in_summary() + thread_summary->total_run_time() <= summary_timespan.length(), "More time blocked + running than is possible in summary timespan");
								    thread_summary->set_max_possible_future_run_time(summary_timespan.length() - (thread_summary->total_blocked_in_summary() + thread_summary->total_run_time()));
								}

								if (!thread_summary->is_blocked_in_future()) {
								    // We ONLY block at context_switch locations. But, we can context
								    // switch on any cpu. So, need a strong check!
								    if (t.max() >= thread_summary->first_block_after_summary()) {
									thread_summary->set_is_blocked_in_future();
								    } else {
									ASSERT(t.location() <= thread_summary->first_block_after_summary(), "Sanity");
									// Each thread controls how much time it can accumulate in a given window.
									// It may be that only a fraction (or none) of the time can be added.
									// Make sure to only add the thread approved amount to the process and total summary
									AbsTime future_time = thread_summary->add_future_run_time(t.length());
									summary.add_future_run_time(future_time);
									process_summary->add_future_run_time(future_time);
								    }
								}
								break;
							    }
							}
							start++;
			}
		    }
		}
	    }

	    //
	    // When we're doing future run predictions, we can create summaries for
	    // threads that have no run time, and no future run time.
	    //
	    // The way this happens is you have 2 or more cpus.
	    // On cpu 1, there is a blocking event for Thread T at time x.
	    //
	    // While walking through cpu 2's activity, you see T
	    // scheduled at x + N. You cannot add to T's future run
	    // time, and T never ran in the original time window.
	    // Thus, T is added and does nothing.
	    //

	    // Remove inactive threads/processes.
	    auto& process_summaries = summary.mutable_process_summaries();
	    auto process_it = process_summaries.begin();
	    while (process_it != process_summaries.end()) {
		auto next_process_it = process_it;
		++next_process_it;
		if (process_it->total_run_time() == 0 && process_it->total_future_run_time() == 0) {
		    DEBUG_ONLY({
			for (auto& thread_summary : process_it->thread_summaries()) {
							ASSERT(thread_summary.total_run_time() == 0 && thread_summary.total_future_run_time() == 0, "Process with 0 run time && 0 future run time has thread with non zero values");
			}
		    });
		    process_summaries.erase(process_it);
		} else {
		    // Our evil friend unordered_set returns const iterators...
		    auto& thread_summaries = const_cast<ProcessSummary<SIZE>*>(&*process_it)->mutable_thread_summaries();
		    auto thread_it = thread_summaries.begin();
		    while (thread_it != thread_summaries.end()) {
			auto next_thread_it = thread_it;
			++next_thread_it;
			if (thread_it->total_run_time() == 0 && thread_it->total_future_run_time() == 0) {
							thread_summaries.erase(thread_it);
			}
			thread_it = next_thread_it;
		    }
		}
		process_it = next_process_it;
	    }
	}
    }

    //
    // Calculate vmfault data.
    //
    // We want to calculate this after the future CPU time, because it is possible a time slice might have vmfaults
    // that span the entire timespan. This could result in a process/thread with no run time, and no future time, which
    // would be removed as "inactive" during future CPU time calculation.
    //

    if (summary_cpu == NULL) {
	// vmfault intervals are stored in the MachineThread
	for (MachineThread<SIZE>* machine_thread : _threads_by_time) {
	    const MachineProcess<SIZE>* process = &machine_thread->process();

	    ProcessSummary<SIZE>* process_summary = NULL;
	    ThreadSummary<SIZE>* thread_summary = NULL;

	    const auto& vm_faults = machine_thread->vm_faults();
	    if (!vm_faults.empty()) {
		AbsInterval vm_faults_timespan = AbsInterval(vm_faults.front().location(), vm_faults.back().max() - vm_faults.front().location());
		AbsInterval trimmed_timespan = summary_timespan.intersection_range(vm_faults_timespan);

		if (trimmed_timespan.length() > 0) {
		    auto start = interval_beginning_timespan(vm_faults, trimmed_timespan);
		    auto end = interval_ending_timespan(vm_faults, trimmed_timespan);

		    ASSERT(!start || start->intersects(trimmed_timespan), "Sanity");
		    ASSERT(!end || end->intersects(trimmed_timespan), "Sanity");
		    ASSERT((!start && !end) || ((start && end) && (start <= end)), "Sanity");

		    if (start && end) {
			// NOTE! <=, not <,  because end is inclusive of data we want to count!
			while (start <= end) {
							//
							// NOTE! summary_timespan, NOT trimmed_timespan!
							//
							// 8/25/13 ... Okay, why do we care summary vs trimmed?
							// It shouldn't be possible for start to lie outside trimmed...
							// Leaving this for now rather than introducing some bizzare
							// corner case, but wth...
							//
							AbsInterval t = start->intersection_range(summary_timespan);

							ASSERT(t.length() > 0, "Might be too strong, but expecting this to be non-zero");

							summary.add_vm_fault_time(t.length());

							// We must initialize these lazily. If we don't, every process and thread gets
							// a summary entry. But we don't want to keep looking them up over and over...
							if (!process_summary) {
							    process_summary = summary.mutable_process_summary(process);
							}
							process_summary->add_vm_fault_time(t.length());

							if (!thread_summary) {
							    thread_summary = process_summary->mutable_thread_summary(machine_thread);
							}
							thread_summary->add_vm_fault_time(t.length());

							start++;
			}
		    }
		}
	    }
	}
    }


    //
    // Calculate IO activity data.
    //
    if (summary_cpu == NULL) {
	//
	// IO activity may overlap on even individual threads.
	//
	// All IO activity is stored in a single sorted vector, but
	// it may overlap even at the thread level. There isn't an
	// easy way to locate a starting and stopping point that intersect
	// a given range.
	//
	// The solution being used is to flatten the overlapping IO
	// and keep a sorted non overlapping list of IO activity. For any
	// given timespan, we find the overlapping intervals of flattened
	// IO activity and then look up the actual matching IOActivity
	// objects.
	//
	if (!_all_io_active_intervals.empty()) {
	    AbsInterval io_timespan = AbsInterval(_all_io_active_intervals.front().location(), _all_io_active_intervals.back().max() - _all_io_active_intervals.front().location());
	    AbsInterval trimmed_timespan = summary_timespan.intersection_range(io_timespan);
	    if (trimmed_timespan.length() > 0) {
		//
		// First find the flattened start point
		//
		if (auto flattened_start = interval_beginning_timespan(_all_io_active_intervals, trimmed_timespan)) {
		    //
		    // Now find the actual start IOActivity
		    //
		    auto it = std::lower_bound(_all_io.begin(), _all_io.end(), flattened_start->location(), AbsIntervalLocationVsAbsTimeComparator());
		    ASSERT(it != _all_io.end(), "If we reach here, we should ALWAYS find a match!");

		    // We need <= in case there are multiple IOActivities ending at the same time
		    while (it != _all_io.end() && it->location() < summary_timespan.max()) {
			AbsInterval t = it->intersection_range(summary_timespan);

			// Some of the ranges will not intersect at all, for example
			//
			// IOActivity
			//
			//      XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX
			//         XXXXXX
			//                      XXXXXXXX
			//
			// Summary Range
			//
			//                          SSSSSSSSSSSSSSSSSS
			//
			// The flattened_start will point at the oldest IOActivity
			// which overlaps the summary range, but many of the later
			// IOActivities will not overlap.

			if (t.length() > 0) {
							//
							// Wait time.
							//
							summary.add_io_time(t.length());

							ProcessSummary<SIZE>* process_summary = summary.mutable_process_summary(&it->thread()->process());
							process_summary->add_io_time(t.length());

							ThreadSummary<SIZE>* thread_summary = process_summary->mutable_thread_summary(it->thread());
							thread_summary->add_io_time(t.length());

							//
							// Bytes completed
							//
							if (summary_timespan.contains(it->max() - AbsTime(1))) {
							    summary.add_io_bytes_completed(it->size());
							    process_summary->add_io_bytes_completed(it->size());
							    thread_summary->add_io_bytes_completed(it->size());
							}
			}
			it++;
		    }
		}
	    }
	}
    }

    //
    // Calculate Jetsam activity data.
    //
    if (summary_cpu == NULL) {
	// jetsam activity is stored in the MachineThread
	for (MachineThread<SIZE>* machine_thread : _threads_by_time) {
	    const MachineProcess<SIZE>* process = &machine_thread->process();

	    ProcessSummary<SIZE>* process_summary = NULL;
	    ThreadSummary<SIZE>* thread_summary = NULL;

	    const auto& jetsam_activity = machine_thread->jetsam_activity();
	    if (!jetsam_activity.empty()) {
		AbsInterval jetsam_timespan = AbsInterval(jetsam_activity.front().location(), jetsam_activity.back().max() - jetsam_activity.front().location());
		AbsInterval trimmed_timespan = summary_timespan.intersection_range(jetsam_timespan);

		if (trimmed_timespan.length() > 0) {
		    auto start = interval_beginning_timespan(jetsam_activity, trimmed_timespan);
		    auto end = interval_ending_timespan(jetsam_activity, trimmed_timespan);

		    ASSERT(!start || start->intersects(trimmed_timespan), "Sanity");
		    ASSERT(!end || end->intersects(trimmed_timespan), "Sanity");
		    ASSERT((!start && !end) || ((start && end) && (start <= end)), "Sanity");

		    if (start && end) {
			// NOTE! <=, not <,  because end is inclusive of data we want to count!
			while (start <= end) {
							//
							// NOTE! summary_timespan, NOT trimmed_timespan!
							//
							// 8/25/13 ... Okay, why do we care summary vs trimmed?
							// It shouldn't be possible for start to lie outside trimmed...
							// Leaving this for now rather than introducing some bizzare
							// corner case, but wth...
							//
							AbsInterval t = start->intersection_range(summary_timespan);

							ASSERT(t.length() > 0, "Might be too strong, but expecting this to be non-zero");

							summary.add_jetsam_time(t.length());

							// We must initialize these lazily. If we don't, every process and thread gets
							// a summary entry. But we don't want to keep looking them up over and over...
							if (!process_summary) {
							    process_summary = summary.mutable_process_summary(process);
							}
							process_summary->add_jetsam_time(t.length());

							if (!thread_summary) {
							    thread_summary = process_summary->mutable_thread_summary(machine_thread);
							}
							thread_summary->add_jetsam_time(t.length());

							start++;
			}
		    }
		}
	    }
	}

	// Jetsam kill times are stored in the process.
	for (MachineProcess<SIZE>* machine_process : _processes_by_time) {
	    if (machine_process->is_exit_by_jetsam()) {
		if (summary_timespan.contains(machine_process->exit_timestamp())) {
		    summary.increment_processes_jetsamed();
		    summary.mutable_process_summary(machine_process)->set_jetsam_killed();
		}
	    }
	}
    }

    DEBUG_ONLY(summary.validate());

    return summary;
}

template <typename SIZE>
uint32_t Machine<SIZE>::active_cpus() const {
    uint32_t cpus = 0;

    for (auto& cpu : _cpus) {
	if (!cpu.timeline().empty()) {
	    cpus++;
	}
    }

    return cpus;
}

// This attempts to analyze various pieces of data and guess
// if the Machine represents an ios device or not.

template <typename SIZE>
bool Machine<SIZE>::is_ios() const {
    // I looked at avg intr time, and min intr time; they were too close for
    // reliable detection of desktop vs device (desktop has intr(s) as short
    // as 60ns).
    
    // For now, we're just going to do a really gross detection, in any trace
    // from a device we'd expect to see SpringBoard or backboardd.
    
    for (auto process : _processes_by_time) {
	if (strcmp(process->name(), "SpringBoard") == 0)
	    return true;
	if (strcmp(process->name(), "backboardd") == 0)
	    return true;
    }
    
    return false;
}
