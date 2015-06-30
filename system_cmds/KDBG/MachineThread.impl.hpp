//
//  MachineThread.impl.hpp
//  KDBG
//
//  Created by James McIlree on 10/30/12.
//  Copyright (c) 2014 Apple. All rights reserved.
//

template <typename SIZE>
AbsTime MachineThread<SIZE>::blocked_in_timespan(AbsInterval timespan) const {
	auto it = std::lower_bound(_blocked.begin(), _blocked.end(), AbsInterval(timespan.location(), AbsTime(1)));
	// The lower bound will report that 0 is lower than [ 10, 20 ), need to check contains!
	AbsTime blocked_time;
	while (it != _blocked.end() && timespan.intersects(*it)) {
		blocked_time += timespan.intersection_range(*it).length();
		++it;
	}

	return blocked_time;
}

template <typename SIZE>
AbsTime MachineThread<SIZE>::next_blocked_after(AbsTime timestamp) const {
	auto it = std::lower_bound(_blocked.begin(), _blocked.end(), AbsInterval(timestamp, AbsTime(1)));
	// The lower bound will report that 0 is lower than [ 10, 20 ), need to check contains!
	if (it != _blocked.end()) {
		if (it->contains(timestamp))
			return timestamp;

		ASSERT(it->location() > timestamp, "Sanity");
		return it->location();
	}

	return _timespan.max();
}

template <typename SIZE>
const MachineVoucher<SIZE>* MachineThread<SIZE>::voucher(AbsTime timestamp) const {
	ASSERT(_timespan.contains(timestamp), "Sanity");

	auto it = std::upper_bound(_vouchers_by_time.begin(), _vouchers_by_time.end(), timestamp, AbsIntervalMaxVsAbsTimeComparator());

	// The upper bound will report that 0 is lower than [ 10, 20 ), need to check contains!
	if (it != _vouchers_by_time.end() && it->contains(timestamp)) {
		return it->voucher();
	}

	return &Machine<SIZE>::UnsetVoucher;
}

template <typename SIZE>
const MachineVoucher<SIZE>* MachineThread<SIZE>::last_voucher() const {
	ASSERT(!_vouchers_by_time.empty(), "Sanity");
	return _vouchers_by_time.back().voucher();
}

#if !defined(NDEBUG) && !defined(NS_BLOCK_ASSERTIONS)
template <typename SIZE>
void MachineThread<SIZE>::validate() const {
    ASSERT(_process, "Sanity");
    ASSERT(_process->timespan().contains(timespan()), "Sanity");

    ASSERT(is_trange_vector_sorted_and_non_overlapping(_blocked), "Sanity");
    ASSERT(is_trange_vector_sorted_and_non_overlapping(_vm_faults), "Sanity");
    ASSERT(is_trange_vector_sorted_and_non_overlapping(_jetsam_activity), "Sanity");
    ASSERT(is_trange_vector_sorted_and_non_overlapping(_vouchers_by_time), "Sanity");

    if (!_blocked.empty()) {
	ASSERT(_timespan.contains(_blocked.front()), "Blocked interval not contained by thread timespan");
	ASSERT(_timespan.contains(_blocked.back()), "Blocked interval not contained by thread timespan");
    }

    if (!_vm_faults.empty()) {
	ASSERT(_timespan.contains(_vm_faults.front()), "vm fault interval not contained by thread timespan");
	ASSERT(_timespan.contains(_vm_faults.back()), "vm_fault interval not contained by thread timespan");
    }

    if (!_jetsam_activity.empty()) {
	ASSERT(_timespan.contains(_jetsam_activity.front()), "jetsam_activity interval not contained by thread timespan");
	ASSERT(_timespan.contains(_jetsam_activity.back()), "jetsam_activity interval not contained by thread timespan");
    }

    if (!_vouchers_by_time.empty()) {
	ASSERT(_timespan.contains(_vouchers_by_time.front()), "vouchers_by_time interval not contained by thread timespan");
	ASSERT(_timespan.contains(_vouchers_by_time.back()), "vouchers_by_time interval not contained by thread timespan");
    }

    ASSERT(!_process->is_trace_terminated() || is_trace_terminated(), "Process is trace terminated but thread is live");

    // Each thread should have at least one creation flag.
    // Note that created by previous machine state is in addition to the
    // actual create flag, so does not count
    ASSERT(is_created_by_thread_map() ||
	   is_created_by_trace_data_new_thread() ||
	   is_created_by_unknown_tid_in_trace() ||
	   is_created_by_fork_exec() ||
	   is_created_by_exec(), "Should have at least one create flag");
}
#endif
