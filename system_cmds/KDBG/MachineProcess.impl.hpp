//
//  MachineProcess.impl.hpp
//  KDBG
//
//  Created by James McIlree on 10/30/12.
//  Copyright (c) 2014 Apple. All rights reserved.
//

template <typename SIZE>
MachineProcess<SIZE>::MachineProcess(pid_t pid, const char* name, AbsTime create_timestamp, kMachineProcessFlag flags) :
	_pid(pid),
	_timespan(create_timestamp, AbsTime(0)),
	_flags((uint32_t)flags),
	_exit_status(0),
	_apptype(-1)
{
    ASSERT(name, "Sanity");
    ASSERT(strlen(name) < sizeof(_name) - 1, "Sanity");
    
    // strlcpy guarantees NULL termination
    strlcpy(_name, name, sizeof(_name));
}

#if !defined(NDEBUG) && !defined(NS_BLOCK_ASSERTIONS)
template <typename SIZE>
void MachineProcess<SIZE>::validate() const {
    ASSERT(strlen(_name), "Must have a non zero length name");

    if (is_trace_terminated()) {
	ASSERT(is_exiting(), "Process is trace terminated without precursor exit event");

	for (auto thread : _threads_by_time) {
	    ASSERT(thread->is_trace_terminated(), "process is trace terminated, but has live thread");
	}
    }
    
    for (auto thread : _threads_by_time) {
        ASSERT(_timespan.contains(thread->timespan()), "thread outside process timespan");
	thread->validate();
    }

    // Every process should have one and only one primordial (main) thread.
    // However, we cannot tell what the main thread is for threadmap processes,
    // and processes forwarded from an earlier machine state may have already
    // exited their main thread. We can only check exec/fork-exec.

    if ((is_created_by_exec() || is_created_by_fork_exec()) && !is_created_by_previous_machine_state()) {
	auto main_threads = 0;
	for (auto thread : _threads_by_time) {
	    if (thread->is_main_thread()) main_threads++;
	    ASSERT(main_threads <= 1, "More than one main thread in a process");
	}
	ASSERT(main_threads == 1, "Incorrect number of main thread in process");
    }

}
#endif