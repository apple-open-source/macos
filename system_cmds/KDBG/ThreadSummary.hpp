//
//  ThreadSummary.hpp
//  KDBG
//
//  Created by James McIlree on 4/23/13.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#ifndef kdprof_ThreadSummary_hpp
#define kdprof_ThreadSummary_hpp

template <typename SIZE>
class MachineThread;

template <typename SIZE>
class ThreadSummary {
    protected:
	const MachineThread<SIZE>*	_thread;

	AbsTime				_total_run_time;
	AbsTime				_total_idle_time;
	AbsTime				_total_intr_time;
	AbsTime				_total_vm_fault_time;
	AbsTime				_total_io_time;
	AbsTime				_total_jetsam_time;

	uint32_t			_context_switch_count;
	uint32_t			_count_idle_events;
	uint32_t			_count_intr_events;
	uint32_t			_count_vm_fault_events;
	uint32_t			_count_io_events;

	uint64_t			_io_bytes_completed;

	AbsTime				_total_future_run_time;

	// Future run helper vars
	AbsTime				_total_blocked_in_summary;
	AbsTime				_max_possible_future_run_time;
	AbsTime				_first_block_after_summary;
	bool				_is_blocked_in_future;
	bool				_is_future_initialized;
	
	friend class Machine<SIZE>;

	void add_run_time(AbsTime time)				{ _total_run_time += time; }
	void add_idle_time(AbsTime time)			{ _total_idle_time += time; _count_idle_events++; }
	void add_intr_time(AbsTime time)			{ _total_intr_time += time; _count_intr_events++; }
	void add_vm_fault_time(AbsTime time)			{ _total_vm_fault_time += time; _count_vm_fault_events++; }
	void add_io_time(AbsTime time)				{ _total_io_time += time; _count_io_events++; }
	void add_jetsam_time(AbsTime time)			{ _total_jetsam_time += time; }
	
	void add_io_bytes_completed(typename SIZE::ptr_t bytes)	{ _io_bytes_completed += bytes; }

	void incr_context_switches()				{ _context_switch_count++; }

	bool is_blocked_in_future()				{ return _is_blocked_in_future; }
	void set_is_blocked_in_future()				{ _is_blocked_in_future = true; }

	AbsTime total_blocked_in_summary()			{ return _total_blocked_in_summary; }
	void set_total_blocked_in_summary(AbsTime time)		{ _total_blocked_in_summary = time; }

	AbsTime max_possible_future_run_time()			{ return _max_possible_future_run_time; }
	void set_max_possible_future_run_time(AbsTime time)	{ _max_possible_future_run_time = time; }

	AbsTime first_block_after_summary()			{ return _first_block_after_summary; }
	void set_first_block_after_summary(AbsTime time)	{ _first_block_after_summary = time; }

	bool is_future_initialized()				{ return _is_future_initialized; }
	void set_future_initialized()				{ _is_future_initialized = true; }
	
	AbsTime add_future_run_time(AbsTime time) {
		ASSERT(_is_future_initialized, "Sanity");
		ASSERT(!_is_blocked_in_future, "Sanity");

		AbsTime capped_time = _max_possible_future_run_time - _total_future_run_time;
		if (capped_time < time) {
			_total_future_run_time += capped_time;
			_is_blocked_in_future = true;
			return capped_time;
		} else {
			_total_future_run_time += time;
			return time;
		}

		ASSERT(_total_future_run_time < _max_possible_future_run_time, "Sanity");
	}

    public:
	ThreadSummary(const MachineThread<SIZE>* thread) :
		_thread(thread),
		_context_switch_count(0),
		_count_idle_events(0),
		_count_intr_events(0),
		_count_vm_fault_events(0),
		_count_io_events(0),
		_io_bytes_completed(0),
		_is_blocked_in_future(false),
		_is_future_initialized(false)
	{
	}

	const MachineThread<SIZE>* thread() const	{ return _thread; }

	AbsTime total_time() const			{ return _total_run_time + _total_idle_time + _total_intr_time; }

	AbsTime	total_run_time() const			{ return _total_run_time; }
	AbsTime total_idle_time() const			{ return _total_idle_time; }
	AbsTime total_intr_time() const			{ return _total_intr_time; }
	AbsTime total_future_run_time() const		{ return _total_future_run_time; }
	AbsTime total_vm_fault_time() const		{ return _total_vm_fault_time; }
	AbsTime total_wallclock_vm_fault_time() const	{ return _total_vm_fault_time; }
	AbsTime total_io_time() const			{ return _total_io_time; }
	AbsTime total_jetsam_time() const		{ return _total_jetsam_time; }

	AbsTime	avg_on_cpu_time() const			{ return _total_run_time / _context_switch_count; }

	uint32_t context_switches() const		{ return _context_switch_count; }
	uint32_t num_idle_events() const		{ return _count_idle_events; }
	uint32_t num_intr_events() const		{ return _count_intr_events; }
	uint32_t num_vm_fault_events() const		{ return _count_vm_fault_events; }
	uint32_t num_io_events() const			{ return _count_io_events; }

	uint64_t io_bytes_completed() const		{ return _io_bytes_completed; }

	DEBUG_ONLY(void validate() const);
};

#if !defined(NDEBUG) && !defined(NS_BLOCK_ASSERTIONS)
template <typename SIZE>
void ThreadSummary<SIZE>::validate() const {
}
#endif

template <typename SIZE>
struct ThreadSummaryHash {
	size_t operator()(const ThreadSummary<SIZE>& summary) const {
		return std::hash<const MachineThread<SIZE>*>()(summary.thread());
	}
};

template <typename SIZE>
struct ThreadSummaryEqualTo {
	bool operator()(const ThreadSummary<SIZE>& s1, const ThreadSummary<SIZE>& s2) const {
		return s1.thread() == s2.thread();
	}
};

#endif
