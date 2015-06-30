//
//  MachineProcess.mutable-impl.hpp
//  KDBG
//
//  Created by James McIlree on 10/30/12.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#include "KDebug.h"

template <typename SIZE>
void MachineProcess<SIZE>::set_exit_by_syscall(AbsTime timestamp, int32_t exit_status) {
    ASSERT(!is_exiting(), "Attempt to exit after process is already exiting");
    ASSERT(_exit_initiated_timestamp == 0, "Sanity");
    ASSERT(!is_kernel(), "Kernel process is attempting to exit");

    _exit_status = exit_status;
    _exit_initiated_timestamp = timestamp;

    set_flags(kMachineProcessFlag::IsExitBySyscall);
}

template <typename SIZE>
void MachineProcess<SIZE>::set_exit_by_jetsam(AbsTime timestamp) {
    ASSERT(!is_exiting(), "Attempt to exit after process is already exiting");
    ASSERT(_exit_initiated_timestamp == 0, "Sanity");
    ASSERT(!is_kernel(), "Kernel process is attempting to exit");

    _exit_initiated_timestamp = timestamp;

    set_flags(kMachineProcessFlag::IsExitByJetsam);
}

template <typename SIZE>
void MachineProcess<SIZE>::set_exit_by_exec(AbsTime timestamp) {
    ASSERT(!is_exiting(), "Attempt to exit after process is already exiting");
    ASSERT(_exit_initiated_timestamp == 0, "Sanity");
    ASSERT(!is_kernel(), "Kernel process is attempting to exit");

    _exit_initiated_timestamp = timestamp;
    set_flags(kMachineProcessFlag::IsExitByExec);

    for (MachineThread<SIZE>* thread : _threads_by_time) {
	if (!thread->is_trace_terminated()) {
	    thread->set_trace_terminated(timestamp);
	}
    }

    set_trace_terminated(timestamp);
}

template <typename SIZE>
void MachineProcess<SIZE>::set_trace_terminated(AbsTime timestamp){
    ASSERT(is_exiting(), "Attempting to set trace terminated without precursor exit event");
    ASSERT(!is_kernel(), "Kernel process is attempting to set trace terminated");

    DEBUG_ONLY({
	for (MachineThread<SIZE>* thread : _threads_by_time) {
	    ASSERT(thread->is_trace_terminated(), "Setting process as trace terminated when it still has live threads");
	}
    })

    _timespan.set_max(timestamp + AbsTime(1));
    set_flags(kMachineProcessFlag::IsTraceTerminated);
}

template <typename SIZE>
void MachineProcess<SIZE>::set_apptype(uint32_t type) {
	ASSERT(type >= TASK_APPTYPE_NONE && type <= TASK_APPTYPE_APP_TAL, "Out of range");
	ASSERT(_apptype == -1 || _apptype == type, "Attempt to set apptype more than once, or to change an inherited apptype");
	ASSERT(!is_kernel(), "Kernel is attempting to set apptype");
	ASSERT(!is_exiting(), "Setting apptype after exit");
	
	_apptype = type;
}

template <typename SIZE>
void MachineProcess<SIZE>::set_apptype_from_trequested(uint32_t type) {
	ASSERT(type >= TASK_APPTYPE_NONE && type <= TASK_APPTYPE_APP_TAL, "Out of range");
	ASSERT(_apptype == -1 || _apptype == type, "trequested apptype does not match set apptype");

	_apptype = type;
}

template <typename SIZE>
void MachineProcess<SIZE>::set_name(const char* name) {
    ASSERT(name, "Sanity");
    ASSERT(strlen(name) < sizeof(_name) - 1, "Sanity");

    strlcpy(_name, name, sizeof(_name));
}

template <typename SIZE>
void MachineProcess<SIZE>::clear_fork_exec_in_progress() {
    ASSERT(!is_unknown(), "Sanity");
    ASSERT(!is_kernel(), "Sanity");
    ASSERT(!is_exiting(), "Sanity");
    ASSERT(!is_exec_in_progress(), "Sanity");
    ASSERT(is_fork_exec_in_progress(), "Sanity");

    clear_flags(kMachineProcessFlag::IsForkExecInProgress);
}

template <typename SIZE>
void MachineProcess<SIZE>::clear_exec_in_progress() {
    ASSERT(!is_unknown(), "Sanity");
    ASSERT(!is_kernel(), "Sanity");
    ASSERT(!is_exiting(), "Sanity");
    ASSERT(!is_fork_exec_in_progress(), "Sanity");
    ASSERT(is_exec_in_progress(), "Sanity");

    clear_flags(kMachineProcessFlag::IsExecInProgress);
}

template <typename SIZE>
void MachineProcess<SIZE>::add_thread(MachineThread<SIZE>* thread) {
    ASSERT(thread, "Sanity");
    ASSERT(&thread->process() == this, "Sanity");
    ASSERT(!thread->is_trace_terminated(), "Attempt to add thread that is already terminated");
    ASSERT(thread->timespan().location() >= _timespan.location(), "Attempt to add thread that started before this process");
    ASSERT(!is_exiting(), "Adding thread to process that has exited");

    // 6/20/2014, reworking time handling, is this still true?
    //
    // Process/thread created by a previous machine state will violate these
    // rules, during initialization. However, only threads created in that
    // form will be so tagged, and so we can exclude them from this assert.
    //
    // ASSERT(!is_exited() || thread->is_created_by_previous_machine_state(), "Adding thread to process that has marked itself as exited");
	
    DEBUG_ONLY({
	// At this point, the threads vector is not sorted.
	// We have to look at everything :-(.
	for (MachineThread<SIZE>* process_thread : _threads_by_time) {
	    if (process_thread->tid() == thread->tid()) {
		ASSERT(!process_thread->timespan().intersects(thread->timespan()), "Overlapping duplicate threads");
	    }
	}
    })
	
    _threads_by_time.push_back(thread);
}

template <typename SIZE>
void MachineProcess<SIZE>::post_initialize(AbsTime last_machine_timestamp) {
    //
    // For processes that are still alive at the post_initialize phase,
    // we want to extend their timespan(s) to the end of the machine state,
    // so they can be looked up by pid/name.
    //
    if (!is_trace_terminated()) {
	ASSERT(_timespan.length() == 0, "Should not have timespan set");

	// Time in a range is always half open. [ 10, 11 ) means 10 is included,
	// but 11 is not. In order to include a given timestamp, we must use
	// a value one greater.
	AbsTime half_open_timestamp = last_machine_timestamp + AbsTime(1);

	_timespan.set_max(half_open_timestamp);
    }

    std::sort(_threads_by_time.begin(), _threads_by_time.end(), thread_by_time_sort<SIZE>);
}
