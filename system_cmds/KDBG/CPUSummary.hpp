//
//  CPUSummary.hpp
//  KDBG
//
//  Created by James McIlree on 4/22/13.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#ifndef kdprof_CPUSummary_hpp
#define kdprof_CPUSummary_hpp

template <typename SIZE>
class MachineCPU;

template <typename SIZE>
class CPUSummary {
    private:
	// Disallow copy constructor to make sure that the compiler
	// is moving these, instead of copying them when we pass around
	CPUSummary(const CPUSummary& that) = delete;
	CPUSummary& operator=(const CPUSummary& other) = delete;

	friend class Machine<SIZE>;

    public:
	typedef std::unordered_set<ProcessSummary<SIZE>, ProcessSummaryHash<SIZE>, ProcessSummaryEqualTo<SIZE>> ProcessSummarySet;
	typedef std::unordered_set<const MachineCPU<SIZE>*> CPUSummaryMachineCPUSet;

    protected:
	AbsTime						_total_unknown_time;
	AbsTime						_total_run_time;
	AbsTime						_total_idle_time;
	AbsTime						_total_intr_time;
	AbsTime						_total_future_run_time;
	AbsTime						_total_wallclock_run_time;
	AbsTime						_total_all_cpus_idle_time;
	AbsTime						_total_vm_fault_time;
	AbsTime						_total_io_time;
	AbsTime						_total_jetsam_time;

	uint32_t 					_context_switch_count;
	uint32_t					_count_idle_events;
	uint32_t					_count_intr_events;
	uint32_t					_count_vm_fault_events;
	uint32_t					_count_io_events;
	uint32_t					_count_processes_jetsamed;
	uint32_t					_active_cpus;
	
	uint64_t					_io_bytes_completed;

	CPUSummaryMachineCPUSet				_cpus;
	ProcessSummarySet				_process_summaries;

	std::vector<AbsInterval>			_wallclock_run_intervals;		// This is the actual wallclock run interval data.
	std::vector<AbsInterval>			_per_cpu_wallclock_run_intervals;	// We need to accumulate intervals during summary generation, this is a temp buffer.

	bool						_should_merge_all_cpus_idle_intervals;
	std::vector<AbsInterval>			_all_cpus_idle_intervals;
	std::vector<AbsInterval>			_per_cpu_all_cpus_idle_intervals;

	void add_unknown_time(AbsTime time)		{ _total_unknown_time += time; }
	void add_run_time(AbsTime time)			{ _total_run_time += time; }
	void add_idle_time(AbsTime time)		{ _total_idle_time += time; _count_idle_events++; }
	void add_intr_time(AbsTime time)		{ _total_intr_time += time; _count_intr_events++; }
	void add_future_run_time(AbsTime time)		{ _total_future_run_time += time; }
	void add_vm_fault_time(AbsTime time)		{ _total_vm_fault_time += time; _count_vm_fault_events++; }
	void add_io_time(AbsTime time)			{ _total_io_time += time; _count_io_events++; } // We want to bump the event count on all IO activity, not just on completion
	void add_jetsam_time(AbsTime time)		{ _total_jetsam_time += time; }

	void add_io_bytes_completed(typename SIZE::ptr_t bytes)	{ _io_bytes_completed += bytes; }

	void increment_processes_jetsamed()		{ _count_processes_jetsamed++; }

	//
	// NOTE! Why are the various interval(s) accumulated one cpu at a time,
	// instead of storing them all in a single vector, sorting it and processing
	// once at the end?
	//
	// The single vector, sort and postprocess would work for wallclock time
	// calculation, because wallclock times involve "union" operations where
	// the number of cpu(s) don't matter.
	//
	// However, for the all-idle and idle-while-wating-on-IO calculations, we
	// need "intersects" operations, I.E. all 16 cores need to be idle to count
	// as "all-idle". In this mode, the number of cores matters, an intersection
	// requires all 16 cores to simultaneously be the same state. This is difficult
	// to calculate with more than 2 sources. By calculating one at a time,
	// that is avoided, the state remains sanity-checkable throughout.
	//


	//
	// Wallclock run intervals are added as each cpu timeline is walked.
	// Between cpu(s), the results are accumulated to a single buffer
	// After all cpus have been processed, the single buffer is summarized
	//
	// wallclock run time is the *union* of cpu run intervals.
	//
	void add_wallclock_run_interval(AbsInterval interval);
	void accumulate_wallclock_run_intervals();
	void summarize_wallclock_run_intervals();

	//
	// all cpus idle intervals are added as each cpu timeline is walked.
	// Between cpu(s), the results are accumulated to a single buffer
	// After all cpus have been processed, the single buffer is summarized.
	//
	// all cpus idle time is the *intersection* of cpu idle intervals
	//
	void add_all_cpus_idle_interval(AbsInterval interval);
	void accumulate_all_cpus_idle_intervals();
	void summarize_all_cpus_idle_intervals();

	void incr_context_switches()				{ _context_switch_count++; }
	void incr_active_cpus()					{ _active_cpus++; }

	// These bracket individual cpu timeline walks
	void begin_cpu_timeline_walk(const MachineCPU<SIZE>* cpu);
	void end_cpu_timeline_walk(const MachineCPU<SIZE>* cpu);

	// These bracket all cpu timeline walks
	void begin_cpu_timeline_walks(void);
	void end_cpu_timeline_walks(void);

	ProcessSummary<SIZE>* mutable_process_summary(const MachineProcess<SIZE>* process) {
		auto it = _process_summaries.find(process);
		if (it == _process_summaries.end()) {
			// We create any process summary that is missing.
			auto insert_result = _process_summaries.emplace(process);
			ASSERT(insert_result.second, "Sanity");
			it = insert_result.first;
		}

		// NOTE! Because we are using a Set instead of a Map, STL wants
		// the objects to be immutable. "it" refers to a const Record, to
		// prevent us from changing the hash or equality of the Set. We
		// know that the allowed set of mutations will not change these,
		// and so we evil hack(tm) and cast away the const'ness.
		return const_cast<ProcessSummary<SIZE>*>(&*it);
	}

	ProcessSummarySet& mutable_process_summaries() 		{ return _process_summaries; }

  public:
	CPUSummary() :
		_context_switch_count(0),
		_count_idle_events(0),
		_count_intr_events(0),
		_count_vm_fault_events(0),
		_count_io_events(0),
		_count_processes_jetsamed(0),
		_active_cpus(0),
		_io_bytes_completed(0),
		_should_merge_all_cpus_idle_intervals(false)
	{
	}

	CPUSummary (CPUSummary&& rhs) noexcept :
		_total_unknown_time(rhs._total_unknown_time),
		_total_run_time(rhs._total_run_time),
		_total_idle_time(rhs._total_idle_time),
		_total_intr_time(rhs._total_intr_time),
		_total_future_run_time(rhs._total_future_run_time),
		_total_wallclock_run_time(rhs._total_wallclock_run_time),
		_total_all_cpus_idle_time(rhs._total_all_cpus_idle_time),
		_total_vm_fault_time(rhs._total_vm_fault_time),
		_total_io_time(rhs._total_io_time),
		_context_switch_count(rhs._context_switch_count),
		_count_idle_events(rhs._count_idle_events),
		_count_intr_events(rhs._count_intr_events),
		_count_vm_fault_events(rhs._count_vm_fault_events),
		_count_io_events(rhs._count_io_events),
		_count_processes_jetsamed(rhs._count_processes_jetsamed),
		_active_cpus(rhs._active_cpus),
		_io_bytes_completed(rhs._io_bytes_completed),
		_cpus(rhs._cpus),
		_process_summaries(rhs._process_summaries),
		// _wallclock_run_intervals
		// _per_cpu_wallclock_run_intervals
		_should_merge_all_cpus_idle_intervals(false)
		// _all_cpus_idle_intervals
		// _per_cpu_all_cpus_idle_intervals
		// _wallclock_vm_fault_intervals
		// _wallclock_pgin_intervals
		// _wallclock_disk_read_intervals
	{
		ASSERT(rhs._all_cpus_idle_intervals.empty(), "Sanity");
		ASSERT(rhs._per_cpu_all_cpus_idle_intervals.empty(), "Sanity");
		ASSERT(rhs._wallclock_run_intervals.empty(), "Sanity");
		ASSERT(rhs._per_cpu_wallclock_run_intervals.empty(), "Sanity");
		ASSERT(rhs._should_merge_all_cpus_idle_intervals == false, "Sanity");
	}

	AbsTime total_time() const					{ return _total_unknown_time + _total_run_time + _total_idle_time + _total_intr_time; }

	AbsTime total_unknown_time() const				{ return _total_unknown_time; }
	AbsTime	total_run_time() const					{ return _total_run_time; }
	AbsTime total_idle_time() const					{ return _total_idle_time; }
	AbsTime total_intr_time() const					{ return _total_intr_time; }
	AbsTime total_future_run_time() const				{ return _total_future_run_time; }
	AbsTime total_wallclock_run_time() const			{ return _total_wallclock_run_time; }
	AbsTime total_all_cpus_idle_time() const			{ return _total_all_cpus_idle_time; }
	AbsTime total_vm_fault_time() const				{ return _total_vm_fault_time; }
	AbsTime total_io_time() const					{ return _total_io_time; }
	AbsTime total_jetsam_time() const				{ return _total_jetsam_time; }
	
	AbsTime	avg_on_cpu_time() const					{ return _total_run_time / _context_switch_count; }

	uint32_t context_switches() const				{ return _context_switch_count; }
	uint32_t num_idle_events() const				{ return _count_idle_events; }
	uint32_t num_intr_events() const				{ return _count_intr_events; }
	uint32_t num_vm_fault_events() const				{ return _count_vm_fault_events; }
	uint32_t num_io_events() const					{ return _count_io_events; }
	uint32_t num_processes_jetsammed() const			{ return _count_processes_jetsamed; }

	uint32_t active_cpus() const					{ return _active_cpus; }

	uint64_t io_bytes_completed() const				{ return _io_bytes_completed; }


	// A CPUSummary may be a summary of one or more CPUs.
	// The cpus set are the MachineCPU(s) that were used to
	// construct this summary.
	const CPUSummaryMachineCPUSet& cpus() const			{ return _cpus; }

	const ProcessSummarySet& process_summaries() const		{ return _process_summaries; }
	const ProcessSummary<SIZE>* process_summary(const MachineProcess<SIZE>* process) const {
		auto it = _process_summaries.find(process);
		return (it == _process_summaries.end()) ? NULL : &*it;
	}

	DEBUG_ONLY(void validate() const;)
};

template <typename SIZE>
void CPUSummary<SIZE>::begin_cpu_timeline_walks() {
	_should_merge_all_cpus_idle_intervals = true;
}

template <typename SIZE>
void CPUSummary<SIZE>::begin_cpu_timeline_walk(const MachineCPU<SIZE>* cpu) {
	ASSERT(cpu, "Sanity");
	_cpus.emplace(cpu);
}

template <typename SIZE>
void CPUSummary<SIZE>::end_cpu_timeline_walk(const MachineCPU<SIZE>* cpu) {
	ASSERT(cpu, "Sanity");

	accumulate_wallclock_run_intervals();
	accumulate_all_cpus_idle_intervals();
}

template <typename SIZE>
void CPUSummary<SIZE>::end_cpu_timeline_walks(void) {
	summarize_wallclock_run_intervals();
	summarize_all_cpus_idle_intervals();
}

template <typename SIZE>
void CPUSummary<SIZE>::add_wallclock_run_interval(AbsInterval interval)	{
	ASSERT(_per_cpu_wallclock_run_intervals.empty() || (_per_cpu_wallclock_run_intervals.back() < interval && !interval.intersects(_per_cpu_wallclock_run_intervals.back())), "Invariant violated");
	_per_cpu_wallclock_run_intervals.emplace_back(interval);
}

template <typename SIZE>
void CPUSummary<SIZE>::accumulate_wallclock_run_intervals() {
	_wallclock_run_intervals = trange_vector_union(_wallclock_run_intervals, _per_cpu_wallclock_run_intervals);
	_per_cpu_wallclock_run_intervals.clear();
	// We don't shrink_to_fit here as its expected another CPU's run intervals will be processed next.

	for (auto& process_summary : _process_summaries) {
		// NOTE! Because we are using a Set instead of a Map, STL wants
		// the objects to be immutable. We know that the operations being
		// invoked will not change the hash, but we still must throw away
		// the const'ness. Care must be taken to avoid the construction of
		// temporary objects, thus the use of pointers...
		const_cast<ProcessSummary<SIZE>*>(&process_summary)->accumulate_wallclock_run_intervals();
	}
}

template <typename SIZE>
void CPUSummary<SIZE>::summarize_wallclock_run_intervals() {
	ASSERT(_per_cpu_wallclock_run_intervals.empty(), "Sanity");
	_per_cpu_wallclock_run_intervals.shrink_to_fit();

	ASSERT(_total_wallclock_run_time == 0, "Called more than once");

	ASSERT(is_trange_vector_sorted_and_non_overlapping(_wallclock_run_intervals), "Sanity");

	for (auto& interval : _wallclock_run_intervals) {
		_total_wallclock_run_time += interval.length();
	}

	_wallclock_run_intervals.clear();
	_wallclock_run_intervals.shrink_to_fit();

	for (auto& process_summary : _process_summaries) {
		// NOTE! Because we are using a Set instead of a Map, STL wants
		// the objects to be immutable. We know that the operations being
		// invoked will not change the hash, but we still must throw away
		// the const'ness. Care must be taken to avoid the construction of
		// temporary objects, thus the use of pointers...
		const_cast<ProcessSummary<SIZE>*>(&process_summary)->summarize_wallclock_run_intervals();
	}
}

template <typename SIZE>
void CPUSummary<SIZE>::add_all_cpus_idle_interval(AbsInterval interval)	{
	ASSERT(_per_cpu_all_cpus_idle_intervals.empty() || (_per_cpu_all_cpus_idle_intervals.back() < interval && !interval.intersects(_per_cpu_all_cpus_idle_intervals.back())), "Invariant violated");
	_per_cpu_all_cpus_idle_intervals.emplace_back(interval);
}

template <typename SIZE>
void CPUSummary<SIZE>::accumulate_all_cpus_idle_intervals() {
	if (_should_merge_all_cpus_idle_intervals) {
		_should_merge_all_cpus_idle_intervals = false;
		_all_cpus_idle_intervals = _per_cpu_all_cpus_idle_intervals;
	} else {
		_all_cpus_idle_intervals = trange_vector_intersect(_all_cpus_idle_intervals, _per_cpu_all_cpus_idle_intervals);
	}
	_per_cpu_all_cpus_idle_intervals.clear();
}

template <typename SIZE>
void CPUSummary<SIZE>::summarize_all_cpus_idle_intervals() {
	ASSERT(!_should_merge_all_cpus_idle_intervals, "Sanity");
	ASSERT(_per_cpu_all_cpus_idle_intervals.empty(), "Sanity");
	ASSERT(_total_all_cpus_idle_time == 0, "Called more than once");
	ASSERT(is_trange_vector_sorted_and_non_overlapping(_all_cpus_idle_intervals), "Sanity");

	_per_cpu_all_cpus_idle_intervals.shrink_to_fit();
	for (auto& interval : _all_cpus_idle_intervals) {
		_total_all_cpus_idle_time += interval.length();
	}

	_all_cpus_idle_intervals.clear();
	_all_cpus_idle_intervals.shrink_to_fit();
}

#if !defined(NDEBUG) && !defined(NS_BLOCK_ASSERTIONS)
template <typename SIZE>
void CPUSummary<SIZE>::validate() const {
	ASSERT(_total_wallclock_run_time <= _total_run_time, "Sanity");
	ASSERT(_total_all_cpus_idle_time <= _total_idle_time, "Sanity");

	for (const auto& process_summary : _process_summaries) {
		process_summary.validate();
	}
}
#endif

#endif
