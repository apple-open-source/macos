//
//  MachineThread.mutable-impl.hpp
//  KDBG
//
//  Created by James McIlree on 10/30/12.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#include "KDebug.h"


template <typename SIZE>
void MachineThread<SIZE>::set_is_idle_thread() {
    ASSERT(!is_trace_terminated(), "Attempt to mark terminated thread as IDLE");
    ASSERT(_process->is_kernel(), "Attempt to set non-kernel thread as IDLE");
    
    set_flags(kMachineThreadFlag::IsIdle);
}

template <typename SIZE>
void MachineThread<SIZE>::set_trace_terminated(AbsTime timestamp) {
    ASSERT(!is_trace_terminated(), "Attempt to trace terminate thread more than once");
    ASSERT(!is_idle_thread(), "Attempt to terminate IDLE thread");

    AbsTime terminated_timestamp = timestamp + AbsTime(1);
    
    // If we were killed with a block event pending, we need to flush it
    // to the queue. The make_runnable call will do sanity checks to
    // handle the corner cases when called from here.
    make_runnable(terminated_timestamp);

    // We need to set the final timestamp for this thread's last voucher.
    // Note that the null voucher and unset voucher are actual objects,
    // they are not represented by NULL or nullptr.
    _vouchers_by_time.back().set_max(terminated_timestamp);

    //
    // Finally set this threads timespan
    //
    _timespan.set_max(terminated_timestamp);


    set_flags(kMachineThreadFlag::TraceTerminated);
}

template <typename SIZE>
void MachineThread<SIZE>::make_runnable(AbsTime timestamp) {
    ASSERT(!is_trace_terminated(), "Attempting to make terminated thread runnable");
    ASSERT(timestamp >= _timespan.location(), "Attempt to make thread runnable before it exists");

    if (_begin_blocked > 0) {
	ASSERT(timestamp > _begin_blocked, "Sanity");
	ASSERT(_blocked.empty() || _begin_blocked > _blocked.back().max(), "Out of order blocked regions");
	_blocked.emplace_back(_begin_blocked, timestamp - _begin_blocked);
	_begin_blocked = AbsTime(0);
    }
}

template <typename SIZE>
void MachineThread<SIZE>::make_unrunnable(AbsTime timestamp) {
    ASSERT(!is_trace_terminated(), "Attempting to make terminated thread unrunnable");
    ASSERT(timestamp >= _timespan.location(), "Attempt to make thread unrunnable before it exists");

    _begin_blocked = timestamp;
}

template <typename SIZE>
void MachineThread<SIZE>::begin_vm_fault(AbsTime timestamp) {
    ASSERT(timestamp >= _timespan.location(), "Attempt to begin vm fault before thread exists");
    ASSERT(!is_trace_terminated(), "Attempt to begin vm fault on thread that has terminated");
    ASSERT(!is_idle_thread(), "Attempt to begin vm fault on IDLE thread");

    ASSERT(_begin_vm_fault == 0, "Attempt to begin vm_fault without end");
    _begin_vm_fault = timestamp;
}

template <typename SIZE>
void MachineThread<SIZE>::end_vm_fault(AbsTime timestamp) {
    ASSERT(timestamp >= _timespan.location(), "Attempt to end vm fault before thread exists");
    ASSERT(!is_trace_terminated(), "Attempt to end vm fault on thread that has terminated");
    ASSERT(!is_idle_thread(), "Attempt to end vm fault on IDLE thread");

    if (_begin_vm_fault > 0) {
	ASSERT(timestamp > _begin_vm_fault, "Sanity");
	ASSERT(_vm_faults.empty() || _begin_vm_fault > _vm_faults.back().max(), "Out of order vm_fault regions");
	_vm_faults.emplace_back(_begin_vm_fault, timestamp - _begin_vm_fault);
	_begin_vm_fault = AbsTime(0);
    }
}

template <typename SIZE>
void MachineThread<SIZE>::begin_jetsam_activity(uint32_t type, AbsTime timestamp) {
    ASSERT(timestamp >= _timespan.location(), "Attempt to begin jetsam activity before thread exists");
    ASSERT(!is_trace_terminated(), "Attempt to begin jetsam activity on thread that has terminated");
    ASSERT(!is_idle_thread(), "Attempt to begin jetsam activity on IDLE thread");

    ASSERT(_begin_jetsam_activity == 0, "Attempt to begin jetsam activity without end");
    ASSERT(_begin_jetsam_activity_type == 0, "Sanity");

    _begin_jetsam_activity = timestamp;
    DEBUG_ONLY(_begin_jetsam_activity_type = type;)
}

template <typename SIZE>
void MachineThread<SIZE>::end_jetsam_activity(uint32_t type, AbsTime timestamp) {
    ASSERT(timestamp >= _timespan.location(), "Attempt to end jetsam activity before thread exists");
    ASSERT(!is_trace_terminated(), "Attempt to end jetsam activity on thread that has terminated");
    ASSERT(!is_idle_thread(), "Attempt to end jetsam activity on IDLE thread");

    if (_begin_jetsam_activity > 0) {
	ASSERT(type == _begin_jetsam_activity_type, "End event type does not match start event");
	ASSERT(timestamp > _begin_jetsam_activity, "Sanity");
	ASSERT(_jetsam_activity.empty() || _begin_jetsam_activity > _jetsam_activity.back().max(), "Out of order jetsam activities");
	_jetsam_activity.emplace_back(_begin_jetsam_activity, timestamp - _begin_jetsam_activity);
	_begin_jetsam_activity = AbsTime(0);
	DEBUG_ONLY(_begin_jetsam_activity_type = 0;)
    }
}

template <typename SIZE>
void MachineThread<SIZE>::set_voucher(MachineVoucher<SIZE>* voucher, AbsTime timestamp) {
    ASSERT(timestamp >= _timespan.location(), "Attempt to set voucher on thread before thread exists");
    ASSERT(!is_trace_terminated(), "Attempt to set voucher on terminated thread");
    ASSERT(!is_idle_thread(), "Attempt to set voucher on IDLE thread");
    ASSERT(!_vouchers_by_time.empty() || _vouchers_by_time.back().max() < timestamp, "Sanity");
    ASSERT(_vouchers_by_time.back().location() < timestamp, "Sanity");

    VoucherInterval<SIZE>& last_voucher = _vouchers_by_time.back();

    if (voucher != last_voucher.voucher()) {
	ASSERT(last_voucher.max() == AbsTime::END_OF_TIME, "Sanity");

	//
	// By default, the voucher interval has the last voucher continuing "forever".
	// We need to trim the time used by that voucher, while handling the case of
	// the very first event setting a new voucher as well.
	//
	// There are three cases possible.
	//
	// 1) timestamp > last_voucher.location // This is the expected case
	// 2) timestamp == last_voucher.location // This should only be possible on the very first event for a thread
	// 3) timestamp < last_voucher.location // This is an error at all times.
	//

	if (timestamp > last_voucher.location()) {
	    // Expected case (#1)
	    last_voucher.set_max(timestamp);
	    _vouchers_by_time.emplace_back(voucher, AbsInterval(timestamp, AbsTime::END_OF_TIME - timestamp));
	} else if (timestamp == last_voucher.location()) {
	    // Corner case (#2)
	    //
	    // Note that we cannot assert that the voucher being replaced is the unset voucher,
	    // as vouchers are forwarded during "live" event handling. This means that the thread
	    // may have a valid voucher that is replaced on the first event.
	    //
	    // The timestamp == _timespan.location assert may also be too strong, if we start forwarding threads true lifetimes.

	    ASSERT(timestamp == _timespan.location(), "Should only be overriding a voucher on the first event for a given thread.");
	    ASSERT(_vouchers_by_time.size() == 1, "Attempt to replace the current voucher when it isn't the first voucher");
	    _vouchers_by_time.pop_back();
	    _vouchers_by_time.emplace_back(voucher, AbsInterval(timestamp, AbsTime::END_OF_TIME - timestamp));
	} else {
	    ASSERT(false, "Attempting to set a voucher on thread earlier in time than the thread's current voucher");

	}
    }
}

template <typename SIZE>
void MachineThread<SIZE>::post_initialize(AbsTime last_machine_timestamp) {
    if (!is_trace_terminated()) {
	//
	// For threads that are still alive at the post_initialize phase,
	// we want to extend their timespan(s) to the end of the machine state,
	// so they can be looked up by tid/timestamp
	//
	ASSERT(_timespan.length() == 0, "Sanity");

	// Time in a range is always half open. [ 10, 11 ) means 10 is included,
	// but 11 is not. In order to include a given timestamp, we must use
	// a value one greater.
	AbsTime half_open_timestamp = last_machine_timestamp + AbsTime(1);

	_timespan.set_max(half_open_timestamp);

	// 6/22/2014 Not sure about this. Just working on the massive time
	// cleanup, along with the "we really know when threads and processes
	// are done" cleanup. We used to always check and flush any outstanding
	// blocked events in post_initialize. This is done explicitly in the trace
	// terminated code now. However, it is possible to have a blocked event
	// outstanding in a live thread at this point. If we actually forward state
	// to future threads, we would want to pick that up, right?
	//
	// So what do we do here?
	//
	// We could make sure the intermediate states were properly fowarded
	// as the threads are forwarded. That leaves the problem of queries against
	// this machine state not showing an existing blocked state, which could
	// have begun long ago.
	//
	// If we flush, how do we tag that last block so the forwarding happens
	// correctly?
	//
	// For now, no one is doing the live update thing and using the cpu
	// states, so I'm going to flush.
	//
	// Note that if the very last event is a make_unrunnable for a thread,
	// this is going to yield a zero length blocking event, which might assert.
	//
	// NEEDS REVIEW, FIX ME.
	make_runnable(half_open_timestamp);

	_vouchers_by_time.back().set_max(half_open_timestamp);
    }
}
