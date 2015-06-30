//
//  MachineCPU.mutable-impl.hpp
//  KDBG
//
//  Created by James McIlree on 11/7/12.
//  Copyright (c) 2014 Apple. All rights reserved.
//

template <typename SIZE>
void MachineCPU<SIZE>::set_idle(AbsTime timestamp) {
	ASSERT(is_idle_state_initialized(), "Setting idle before state was initialized");
	ASSERT(!is_intr(), "Setting idle while in interrupt");
	ASSERT(!is_idle(), "Setting idle while already idle");
	ASSERT(_begin_idle == 0, "Sanity");

	_begin_idle = timestamp;
	_flags |= (uint32_t)kMachineCPUFlag::IsStateIdle;
}

template <typename SIZE>
void MachineCPU<SIZE>::clear_idle(AbsTime timestamp) {
	ASSERT(is_idle_state_initialized(), "Clearing idle before state was initialized");
	ASSERT(!is_intr(), "Clearing idle while in interrupt");
	ASSERT(is_idle(), "Clearing idle while not idle");

	_cpu_idle.emplace_back(_begin_idle, timestamp - _begin_idle);
	DEBUG_ONLY(_begin_idle = AbsTime(0);)
	_flags &= ~(uint32_t)kMachineCPUFlag::IsStateIdle;
}

template <typename SIZE>
void MachineCPU<SIZE>::set_deactivate_switch_to_idle_thread() {
	ASSERT(!is_deactivate_switch_to_idle_thread(), "State already set");
	ASSERT(!is_intr(), "This state should not occur during INTR");

	_flags |= (uint32_t)kMachineCPUFlag::IsStateDeactivatedForcedSwitchToIdleThread;
}

template <typename SIZE>
void MachineCPU<SIZE>::clear_deactivate_switch_to_idle_thread() {
	ASSERT(is_deactivate_switch_to_idle_thread(), "Clearing state when not set");
	ASSERT(!is_intr(), "This state transition should not occur during INTR");

	_flags &= ~(uint32_t)kMachineCPUFlag::IsStateDeactivatedForcedSwitchToIdleThread;
}

template <typename SIZE>
void MachineCPU<SIZE>::initialize_idle_state(bool is_idle, AbsTime timestamp) {
	ASSERT(!is_idle_state_initialized(), "Attempt to initialize Idle state more than once");
	ASSERT(!this->is_idle(), "Attempt to initialize Idle state while already idle");

	if (is_idle) {
		_begin_idle = timestamp;
		_flags |= (uint32_t)kMachineCPUFlag::IsStateIdle;
	}

	_flags |= (uint32_t)kMachineCPUFlag::IsStateIdleInitialized;
}

template <typename SIZE>
void MachineCPU<SIZE>::set_intr(AbsTime timestamp) {
	// We can take an INTR in state Unknown, IDLE, and RUNNING.
	ASSERT(is_intr_state_initialized(), "Setting INTR before state was initialized");
	ASSERT(!is_intr(), "Setting INTR when already in state INTR");
	ASSERT(_begin_intr == 0, "Sanity");

	_begin_intr = timestamp;
	_flags |= (uint32_t)kMachineCPUFlag::IsStateINTR;
}

template <typename SIZE>
void MachineCPU<SIZE>::clear_intr(AbsTime timestamp) {
	ASSERT(is_intr_state_initialized(), "Clearing INTR before state was initialized");
	ASSERT(is_intr(), "Clearing INTR when not in INTR");

	_cpu_intr.emplace_back(_begin_intr, timestamp - _begin_intr);
	DEBUG_ONLY(_begin_intr = AbsTime(0);)
	_flags &= ~(uint32_t)kMachineCPUFlag::IsStateINTR;
}

template <typename SIZE>
void MachineCPU<SIZE>::initialize_intr_state(bool is_intr, AbsTime timestamp) {
	ASSERT(!is_intr_state_initialized(), "Attempt to initialize INTR state more than once");
	ASSERT(!this->is_intr(), "Attempt to initialize INTR state while already INTR");

	if (is_intr) {
		_begin_intr = timestamp;
		_flags |= (uint32_t)kMachineCPUFlag::IsStateINTR;
	}

	_flags |= (uint32_t)kMachineCPUFlag::IsStateINTRInitialized;
}

template <typename SIZE>
void MachineCPU<SIZE>::initialize_thread_state(MachineThread<SIZE>* init_thread, AbsTime timestamp) {
	ASSERT(!is_thread_state_initialized(), "Attempt to initialize thread state more than once");
	ASSERT(!_thread, "Sanity");

	// When initializing the thread state, the TID lookup may fail. This
	// can happen if there wasn't a threadmap, or if the thread was created
	// later in the trace. We explicitly allow NULL as a valid value here.
	// NULL means "Go ahead and set the init flag, but we will not emit a
	// runq event later when a real context switch happens

	_flags |= (uint32_t)kMachineCPUFlag::IsStateThreadInitialized;
	if (init_thread) {
		_cpu_runq.emplace_back(init_thread, true, timestamp);
		_thread = init_thread;
	}
}

template <typename SIZE>
void MachineCPU<SIZE>::context_switch(MachineThread<SIZE>* to_thread, MachineThread<SIZE>* from_thread, AbsTime timestamp) {
    //
    // We cannot context switch in INTR or Idle
    //
    // The one exception is if we were thread_initialized with NULL,
    // then the first context switch will happen at idle.
    ASSERT(!is_intr(), "May not context switch while in interrupt");
    ASSERT(!is_idle() || _thread == NULL && is_thread_state_initialized(), "May not context switch while idle");
    ASSERT(to_thread, "May not context switch to NULL");

    // The threads should match, unless...
    // 1) We're uninitialized; we don't know who was on cpu
    // 2) VERY RARE: A process EXEC'd, and we made a new thread for the new process. The tid's will still match, and the old thread should be marked as trace terminated.
    ASSERT(from_thread == _thread || _thread == NULL || (_thread->is_trace_terminated() && _thread->tid() == from_thread->tid()), "From thread does not match thread on cpu");

    // Very rarely, we init a cpu to a thread, and then event[0] is a mach_sched
    // or other context switch event. If that has happened, just discard the init
    // thread entry.
    if (_cpu_runq.size() == 1) {
	if (_cpu_runq.back().is_event_zero_init_thread()) {
	    if (timestamp == _cpu_runq.back().timestamp()) {
		_cpu_runq.pop_back();
	    }
	}
    }

    ASSERT(_cpu_runq.empty() || timestamp > _cpu_runq.back().timestamp(), "Out of order timestamps");
    ASSERT(_cpu_runq.size() < 2 || !_cpu_runq.back().is_event_zero_init_thread(), "Sanity");

    _cpu_runq.emplace_back(to_thread, false, timestamp);
    _thread = to_thread;
}

template <typename SIZE>
void MachineCPU<SIZE>::post_initialize(AbsInterval events_timespan) {
#if !defined(NDEBUG) && !defined(NS_BLOCK_ASSERTIONS)
	// Make sure everything is sorted
	if (_cpu_runq.size() > 1) {
		for (uint32_t i=1; i<_cpu_runq.size(); ++i) {
			ASSERT(_cpu_runq[i-1].timestamp() < _cpu_runq[i].timestamp(), "Out of order run events");
		}
	}
	if (_cpu_idle.size() > 1) {
		for (uint32_t i=1; i<_cpu_idle.size(); ++i) {
			ASSERT(_cpu_idle[i-1].max() < _cpu_idle[i].location(), "Out of order idle events");
		}
	}
	if (_cpu_intr.size() > 1) {
		for (uint32_t i=1; i<_cpu_intr.size(); ++i) {
			ASSERT(_cpu_intr[i-1].max() < _cpu_intr[i].location(), "Out of order intr events");
		}
	}
#endif

	// We do not need to flush the current thread on cpu, as the cpu
	// runq only records "on" events, and assumes a duration of "until
	// the next thread arrives or end of time"


	// if we have a pending intr state, flush it.
	// We want to flush the intr first, so an idle
	// flush doesn't assert.
	if (is_intr())
		clear_intr(events_timespan.max());

	// If we have a pending idle state, flush it.
	if (is_idle())
		clear_idle(events_timespan.max());

	if (!_cpu_runq.empty() || !_cpu_idle.empty() || !_cpu_intr.empty()) {
		//
		// Collapse all the events into a single timeline
		//

		// Check this math once we're done building the timeline.
		size_t guessed_capacity = _cpu_runq.size() + _cpu_idle.size() * 2 + _cpu_intr.size() * 2;
		_timeline.reserve(guessed_capacity);

		auto runq_it = _cpu_runq.begin();
		auto idle_it = _cpu_idle.begin();
		auto intr_it = _cpu_intr.begin();

		// Starting these at 0 will for an update to valid values in
		// the first pass of the workloop.
		
		AbsInterval current_runq(AbsTime(0), AbsTime(0));
		AbsInterval current_idle(AbsTime(0), AbsTime(0));
		AbsInterval current_intr(AbsTime(0), AbsTime(0));

		MachineThread<SIZE>* current_thread = NULL;

		AbsTime cursor(events_timespan.location());
		while (events_timespan.contains(cursor)) {
			//
			// First we see if anyone needs updating with the next component.
			//
			if (cursor >= current_runq.max()) {
				if (runq_it != _cpu_runq.end()) {
					AbsTime end, begin = runq_it->timestamp();
					if (runq_it+1 != _cpu_runq.end())
						end = (runq_it+1)->timestamp();
					else
						end = events_timespan.max();

					current_runq = AbsInterval(begin, end - begin);
					current_thread = runq_it->thread();
					++runq_it;
				} else {
					// This will force future update checks to always fail.
					current_runq = AbsInterval(events_timespan.max() + AbsTime(1), AbsTime(1));
					current_thread = NULL;
				}
			}

			if (cursor >= current_idle.max()) {
				if (idle_it != _cpu_idle.end()) {
					current_idle = *idle_it;
					++idle_it;
				} else {
					// This will force future update checks to always fail.
					current_idle = AbsInterval(events_timespan.max() + AbsTime(1), AbsTime(1));
				}
			}

			if (cursor >= current_intr.max()) {
				if (intr_it != _cpu_intr.end()) {
					current_intr = *intr_it;
					++intr_it;
				} else {
					// This will force future update checks to always fail.
					current_intr = AbsInterval(events_timespan.max() + AbsTime(1), AbsTime(1));
				}
			}

			//
			// Now we see what type of activity we will be recording.
			//
			// This is heirarchical, intr > idle > run > unknown.
			//

			kCPUActivity type = kCPUActivity::Unknown;

			if (current_runq.contains(cursor))
				type = kCPUActivity::Run;

			if (current_idle.contains(cursor))
				type = kCPUActivity::Idle;

			if (current_intr.contains(cursor))
				type = kCPUActivity::INTR;

			//
			// Now we know the type, and the starting location.
			// We must find the end.
			//
			// Since this is heirarchical, each type may end on
			// its own "end", or the "begin" of a type higher than
			// itself. An idle can end at its end, or at an intr begin.
			//

			AbsTime end;
			switch (type) {
				case kCPUActivity::Unknown:
					end = std::min({ events_timespan.max(), current_runq.location(), current_idle.location(), current_intr.location() });
					break;

				case kCPUActivity::Run:
					end = std::min({ current_runq.max(), current_idle.location(), current_intr.location() });
					break;

				case kCPUActivity::Idle:
					end = std::min(current_idle.max(), current_intr.location());
					break;

				case kCPUActivity::INTR:
					end = current_intr.max();
					break;
			}

			//
			// Now we drop in the new activity
			//
			if (type == kCPUActivity::Run) {
				ASSERT(current_thread, "Current thread is NULL");
				// Its a context switch if we are at the beginning of the runq interval
				_timeline.emplace_back(current_thread, AbsInterval(cursor, end - cursor), current_runq.location() == cursor);
			} else
				_timeline.emplace_back(type, AbsInterval(cursor, end - cursor));

			//
			// And bump the cursor to the end...
			//
			cursor = end;
		}
		
#if !defined(NDEBUG) && !defined(NS_BLOCK_ASSERTIONS)
		for (auto it = _timeline.begin(); it != _timeline.end(); ++it) {
			auto next_it = it + 1;
			ASSERT(events_timespan.contains(*it), "activity not contained in events_timespan");
			if (next_it != _timeline.end()) {
				ASSERT(it->max() == next_it->location(), "activity not end to end");
				bool initial_idle_state = ((it == _timeline.begin()) &&  it->is_idle());
				ASSERT(!next_it->is_context_switch() || (it->is_run() || it->is_unknown() || initial_idle_state) , "Context switch activity preceeded by !run activity");
			}
		}
#endif
	}
	
	_cpu_runq.clear();
	_cpu_runq.shrink_to_fit();
	
	_cpu_idle.clear();
	_cpu_idle.shrink_to_fit();
	
	_cpu_intr.clear();
	_cpu_intr.shrink_to_fit();
}
