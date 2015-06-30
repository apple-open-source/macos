//
//  ProcessSummary.hpp
//  KDBG
//
//  Created by James McIlree on 4/23/13.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#ifndef kdprof_ProcessSummary_hpp
#define kdprof_ProcessSummary_hpp

template <typename SIZE>
class MachineProcess;

template <typename SIZE>
class MachineThread;

template <typename SIZE>
class CPUSummary;

template <typename SIZE>
class ProcessSummary {
    public:
	typedef std::unordered_set<ThreadSummary<SIZE>, ThreadSummaryHash<SIZE>, ThreadSummaryEqualTo<SIZE>> ThreadSummarySet;

    protected:
	const MachineProcess<SIZE>*	_process;

	AbsTime				_total_run_time;
	AbsTime				_total_idle_time;
	AbsTime				_total_intr_time;
	AbsTime				_total_future_run_time;
	AbsTime				_total_wallclock_run_time;
	AbsTime				_total_vm_fault_time;
	AbsTime				_total_io_time;
	AbsTime				_total_jetsam_time;

	uint32_t			_context_switch_count;
	uint32_t			_count_idle_events;
	uint32_t			_count_intr_events;
	uint32_t			_count_vm_fault_events;
	uint32_t			_count_io_events;
	bool				_is_jetsam_killed;

	uint64_t			_io_bytes_completed;

	ThreadSummarySet		_thread_summaries;

	std::vector<AbsInterval>	_wallclock_run_intervals;		// This is the actual wallclock run interval data.
	std::vector<AbsInterval>	_per_cpu_wallclock_run_intervals;	// We need to accumulate intervals during summary generation, this is a temp buffer.

	friend class Machine<SIZE>;
	friend class CPUSummary<SIZE>;

	void add_run_time(AbsTime time)				{ _total_run_time += time; }
	void add_idle_time(AbsTime time)			{ _total_idle_time += time; _count_idle_events++; }
	void add_intr_time(AbsTime time)			{ _total_intr_time += time; _count_intr_events++; }
	void add_future_run_time(AbsTime time)			{ _total_future_run_time += time; }
	void add_vm_fault_time(AbsTime time)			{ _total_vm_fault_time += time; _count_vm_fault_events++; }
	void add_io_time(AbsTime time)				{ _total_io_time += time; _count_io_events++; }
	void add_jetsam_time(AbsTime time)			{ _total_jetsam_time += time; }
	
	void add_io_bytes_completed(typename SIZE::ptr_t bytes)	{ _io_bytes_completed += bytes; }

	//
	// Wallclock run intervals are added as each cpu timeline is walked.
	// Between cpu(s), the results are accumulated to a single buffer
	// After all cpus have been processed, the single buffer is summarized
	//
	void add_wallclock_run_interval(AbsInterval interval);
	void accumulate_wallclock_run_intervals();
	void summarize_wallclock_run_intervals();

	void incr_context_switches()				{ _context_switch_count++; }

	void set_jetsam_killed()				{ ASSERT(!_is_jetsam_killed, "Attempt to jetsam process twice"); _is_jetsam_killed = true; }

	ThreadSummary<SIZE>* mutable_thread_summary(const MachineThread<SIZE>* thread) {
		auto it = _thread_summaries.find(thread);
		if (it == _thread_summaries.end()) {
			// We create any thread summary that is missing.
			auto insert_result = _thread_summaries.emplace(thread);
			ASSERT(insert_result.second, "Sanity");
			it = insert_result.first;
		}

		// NOTE! Because we are using a Set instead of a Map, STL wants
		// the objects to be immutable. "it" refers to a const Record, to
		// prevent us from changing the hash or equality of the Set. We
		// know that the allowed set of mutations will not change these,
		// and so we evil hack(tm) and cast away the const'ness.
		return const_cast<ThreadSummary<SIZE>*>(&*it);
	}

	ThreadSummarySet& mutable_thread_summaries()		{ return _thread_summaries; }

    public:
	ProcessSummary(const MachineProcess<SIZE>* process) :
		_process(process),
		_context_switch_count(0),
		_count_idle_events(0),
		_count_intr_events(0),
		_count_vm_fault_events(0),
		_count_io_events(0),
		_is_jetsam_killed(false),
		_io_bytes_completed(0)
	{
	}

	const MachineProcess<SIZE>* process() const		{ return _process; }

	AbsTime total_time() const				{ return _total_run_time + _total_idle_time + _total_intr_time; }
	AbsTime	total_run_time() const				{ return _total_run_time; }
	AbsTime total_idle_time() const				{ return _total_idle_time; }
	AbsTime total_intr_time() const				{ return _total_intr_time; }
	AbsTime total_future_run_time() const			{ return _total_future_run_time; }
	AbsTime total_wallclock_run_time() const		{ return _total_wallclock_run_time; }
	AbsTime total_vm_fault_time() const			{ return _total_vm_fault_time; }
	AbsTime total_io_time() const				{ return _total_io_time; }
	AbsTime total_jetsam_time() const			{ return _total_jetsam_time; }

	AbsTime	avg_on_cpu_time() const				{ return _total_run_time / _context_switch_count; }

	uint32_t context_switches() const			{ return _context_switch_count; }
	uint32_t num_idle_events() const			{ return _count_idle_events; }
	uint32_t num_intr_events() const			{ return _count_intr_events; }
	uint32_t num_vm_fault_events() const			{ return _count_vm_fault_events; }
	uint32_t num_io_events() const				{ return _count_io_events; }
	uint32_t num_processes_jetsammed() const		{ return _is_jetsam_killed ? 1 : 0; }

	uint64_t io_bytes_completed() const			{ return _io_bytes_completed; }

	const ThreadSummarySet& thread_summaries() const	{ return _thread_summaries; }

	const ThreadSummary<SIZE>* thread_summary(const MachineThread<SIZE>* thread) const {
		auto it = _thread_summaries.find(thread);
		return (it == _thread_summaries.end()) ? NULL : &*it;
	}

	DEBUG_ONLY(void validate() const;)
};

template <typename SIZE>
void ProcessSummary<SIZE>::add_wallclock_run_interval(AbsInterval interval)	{
	ASSERT(_per_cpu_wallclock_run_intervals.empty() || (_per_cpu_wallclock_run_intervals.back() < interval && !interval.intersects(_per_cpu_wallclock_run_intervals.back())), "Invariant violated");
	_per_cpu_wallclock_run_intervals.emplace_back(interval);
}

template <typename SIZE>
void ProcessSummary<SIZE>::accumulate_wallclock_run_intervals() {
	_wallclock_run_intervals = trange_vector_union(_wallclock_run_intervals, _per_cpu_wallclock_run_intervals);
	_per_cpu_wallclock_run_intervals.clear();
	// We don't shrink_to_fit here as its expected another CPU's run intervals will be processed next.
}

template <typename SIZE>
void ProcessSummary<SIZE>::summarize_wallclock_run_intervals() {
	ASSERT(_per_cpu_wallclock_run_intervals.empty(), "Sanity");
	_per_cpu_wallclock_run_intervals.shrink_to_fit();

	ASSERT(_total_wallclock_run_time == 0, "Called more than once");

	ASSERT(is_trange_vector_sorted_and_non_overlapping(_wallclock_run_intervals), "Sanity");

	for (auto& interval : _wallclock_run_intervals) {
		_total_wallclock_run_time += interval.length();
	}

	_wallclock_run_intervals.clear();
	_wallclock_run_intervals.shrink_to_fit();
}

#if !defined(NDEBUG) && !defined(NS_BLOCK_ASSERTIONS)
template <typename SIZE>
void ProcessSummary<SIZE>::validate() const {
	ASSERT(_total_wallclock_run_time <= _total_run_time, "Sanity");

	for (const auto& thread_summary : _thread_summaries) {
		thread_summary.validate();
	}
}
#endif

template <typename SIZE>
struct ProcessSummaryHash {
	size_t operator()(const ProcessSummary<SIZE>& summary) const {
		return std::hash<const MachineProcess<SIZE>*>()(summary.process());
	}
};

template <typename SIZE>
struct ProcessSummaryEqualTo {
	bool operator()(const ProcessSummary<SIZE>& s1, const ProcessSummary<SIZE>& s2) const {
		return s1.process() == s2.process();
	}
};

#endif
