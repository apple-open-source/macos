//
//  Machine.mutable-impl.hpp
//  KDBG
//
//  Created by James McIlree on 10/30/12.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#include "KDebug.h"

template <typename SIZE>
MachineProcess<SIZE>* Machine<SIZE>::create_process(pid_t pid, const char* name, AbsTime create_timestamp, kMachineProcessFlag flags) {
	ASSERT(name, "Sanity");

	// Validate that we are not creating a process that already exists
	DEBUG_ONLY({
		ASSERT(_processes_by_pid.size() == _processes_by_name.size(), "Sanity");
		ASSERT(_processes_by_pid.size() == _processes_by_time.size(), "Sanity");

		auto by_pid_range = _processes_by_pid.equal_range(pid);
		for (auto it = by_pid_range.first; it != by_pid_range.second; ++it) {
			MachineProcess<SIZE>& process = it->second;
			ASSERT(!process.timespan().contains(create_timestamp), "Creating a process that overlaps an existing process");
		}

		auto by_name_range = _processes_by_name.equal_range(name);
		for (auto it = by_name_range.first; it != by_name_range.second; ++it) {
			MachineProcess<SIZE>& process = *it->second;
			ASSERT((process.pid() != pid) || (!process.timespan().contains(create_timestamp)), "Creating a process that overlaps an existing process");
		}

		// The "by time" vector is unsorted during construction, we have to look at everything.
		for (MachineProcess<SIZE>* process : _processes_by_time) {
			if (process->pid() == pid) {
				ASSERT(!process->timespan().contains(create_timestamp), "Creating a process that overlaps an existing process");
			}
		}
	})

	if (MachineProcess<SIZE>* about_to_be_reused_process = youngest_mutable_process(pid)) {
		// If this process is still alive, we're going to be replacing it.
		// The only legal way of doing that is an exec. Validate.
		if (!about_to_be_reused_process->is_trace_terminated()) {
			DEBUG_ONLY({
				ASSERT((uint32_t)flags & ((uint32_t)kMachineProcessFlag::CreatedByForkExecEvent | (uint32_t)kMachineProcessFlag::CreatedByExecEvent),
				       "Replacing existing process without exec or fork-exec");
			})
			//
			// Exit by exec is unique in that we will have two processes/threads
			// back to back in the timeline. We do not want them to overlap, and
			// the new process timeline is half open , and will have this time as
			// its creation. Pass a 1 mabs older time to exit to prevent overlap
			about_to_be_reused_process->set_exit_by_exec(create_timestamp - AbsTime(1));
		}
		ASSERT(about_to_be_reused_process->is_trace_terminated(), "Sanity");
	}

	MachineProcess<SIZE>* process = &_processes_by_pid.emplace(pid, MachineProcess<SIZE>(pid, name, create_timestamp, flags))->second;
	_processes_by_name.emplace(process->name(), process);
	_processes_by_time.push_back(process);

	return process;
}

template <typename SIZE>
MachineThread<SIZE>* Machine<SIZE>::create_thread(MachineProcess<SIZE>* process, typename SIZE::ptr_t tid, MachineVoucher<SIZE>* voucher, AbsTime create_timestamp, kMachineThreadFlag flags) {
	ASSERT(process, "Sanity");

	// Validate that we are not creating a thread that already exists
	DEBUG_ONLY({
		ASSERT(_threads_by_tid.size() == _threads_by_time.size(), "Sanity");

		auto by_tid_range = _threads_by_tid.equal_range(tid);
		for (auto it = by_tid_range.first; it != by_tid_range.second; ++it) {
			MachineThread<SIZE>& thread = it->second;
			ASSERT(!thread.timespan().contains(create_timestamp), "Creating a thread that overlaps an existing thread");
		}

		// The "by time" vector is unsorted during construction, we have to look at everything
		for (MachineThread<SIZE>* thread : _threads_by_time) {
			if (thread->tid() == tid) {
				ASSERT(!thread->timespan().contains(create_timestamp), "Creating a thread that overlaps an existing thread");
			}
		}
	})

	// Currently the only way we intentionally re-use live threads is via exec/fork-exec.
	// The exec/fork-exec code calls create_process first, which should mark all existing
	// threads as trace-terminated. So we should NEVER see a live thread at this point.
	// validate.
	DEBUG_ONLY({
		if (MachineThread<SIZE>* about_to_be_reused_thread = youngest_mutable_thread(tid)) {
			ASSERT(about_to_be_reused_thread->is_trace_terminated(), "Expected this thread to be terminated");
		}
	});

	MachineThread<SIZE>* thread = &_threads_by_tid.emplace(tid, MachineThread<SIZE>(process, tid, voucher, create_timestamp, flags))->second;
	_threads_by_time.push_back(thread);

	return thread;
}

template <typename SIZE>
MachineVoucher<SIZE>* Machine<SIZE>::create_voucher(typename SIZE::ptr_t address, AbsTime create_timestamp, kMachineVoucherFlag flags, uint32_t content_bytes_capacity) {
	ASSERT(address, "Should not be NULL");
	ASSERT(content_bytes_capacity < 4096, "Probably an error"); // This is a guesstimate, may need re-evaluation

	MachineVoucher<SIZE>* voucher;

	ASSERT(_voucher_nursery.find(address) == _voucher_nursery.end(), "Attempt to create an already live voucher (<rdar://problem/16898190>)");
	//
	// There is no real workaround for this. Other tracepoints will use the address, bad things happen. You can't fix ordering bugs with cleverness outside the lock :-).
	//
	// <rdar://problem/16898190> voucher create / destroy tracepoints are outside the hashtable lock

	auto workaround_it = _voucher_nursery.find(address);
	if (workaround_it != _voucher_nursery.end()) {
		// We've hit a race condition, this voucher was used before the create event was posted.
		// We want to update the content_bytes_capacity, but not the create_timestamp.
		voucher = workaround_it->second.get();
		voucher->workaround_16898190(flags, content_bytes_capacity);
	} else {
		auto it = _voucher_nursery.emplace(address, std::make_unique<MachineVoucher<SIZE>>(address, AbsInterval(create_timestamp, AbsTime(0)), flags, content_bytes_capacity));
		ASSERT(it.second, "Voucher emplace in nursery failed");
		voucher = it.first->second.get();
	}

	ASSERT(voucher->is_live(), "Sanity");
	ASSERT(!voucher->is_null(), "Sanity");
	ASSERT(!voucher->is_unset(), "Sanity");

	return voucher;
}

template <typename SIZE>
void Machine<SIZE>::destroy_voucher(typename SIZE::ptr_t address, AbsTime timestamp) {
	ASSERT(address, "Should not be NULL");

	auto nursery_it = _voucher_nursery.find(address);

	// We need a voucher for every reference, so we are in the odd position
	// of creating a voucher so we can destroy it.
	if (nursery_it == _voucher_nursery.end()) {
		create_voucher(address, AbsTime(0), kMachineVoucherFlag::CreatedByFirstUse, 0);
		nursery_it = _voucher_nursery.find(address);
	}

	MachineVoucher<SIZE>* voucher = nursery_it->second.get();

	voucher->set_destroyed(timestamp);

	// First find the "row" for this address.
	auto by_addr_it = _vouchers_by_addr.find(address);
	if (by_addr_it == _vouchers_by_addr.end()) {
		// No address entry case
		//std::vector<std::unique_ptr<MachineVoucher<SIZE>>> row(std::move(nursery_it->second));
		std::vector<std::unique_ptr<MachineVoucher<SIZE>>> row;
		row.emplace_back(std::move(nursery_it->second));
		_vouchers_by_addr.emplace(address, std::move(row));
	} else {
		auto& row = by_addr_it->second;

		// Make sure these are sorted and non-overlapping
		ASSERT(row.back()->timespan() < voucher->timespan(), "Sanity");
		ASSERT(!row.back()->timespan().intersects(voucher->timespan()), "Sanity");

		row.emplace_back(std::move(nursery_it->second));
	}

	_voucher_nursery.erase(nursery_it);
}

//
// This function handles looking up a voucher by address. If neccessary, it will create a new voucher.
// NOTE! Does not update voucher timestamps, that is only done at voucher destroy.
//

template <typename SIZE>
MachineVoucher<SIZE>* Machine<SIZE>::process_event_voucher_lookup(typename SIZE::ptr_t address, uint32_t msgh_bits) {
	// NOTE! There is a subtle race here, we *MUST* test msgh_bits before
	// checking for a 0 address. An unset voucher may have address 0...
	if (MACH_MSGH_BITS_VOUCHER(msgh_bits) == MACH_MSGH_BITS_ZERO)
		return &UnsetVoucher;

	if (address == 0)
		return &NullVoucher;

	auto nursery_it = _voucher_nursery.find(address);
	if (nursery_it == _voucher_nursery.end()) {
		// If no voucher exists, create a default (no-contents!) voucher.
		return create_voucher(address, AbsTime(0), kMachineVoucherFlag::CreatedByFirstUse, 0);
	}

	return nursery_it->second.get();
}

template <typename SIZE>
MachineVoucher<SIZE>* Machine<SIZE>::thread_forwarding_voucher_lookup(const MachineVoucher<SIZE>* previous_machine_state_voucher) {
	ASSERT(previous_machine_state_voucher, "Sanity");

	if (previous_machine_state_voucher == &UnsetVoucher)
		return &UnsetVoucher;

	if (previous_machine_state_voucher == &NullVoucher)
		return &NullVoucher;

	auto nursery_it = _voucher_nursery.find(previous_machine_state_voucher->address());
	if (nursery_it == _voucher_nursery.end()) {
		ASSERT(false, "Should not ever have a thread forwarding a voucher not in the nursery");
		return &UnsetVoucher;
	}

	return nursery_it->second.get();
}

//
// This is used by processes that are being fork-exec'd / exec'd. They must be
// created with some name, but it isn't their final name. For now, we are
// heavily ASSERTING state to only allow processes which are fork-exec'd /
// exec'd to set their name.
//
template <typename SIZE>
void Machine<SIZE>::set_process_name(MachineProcess<SIZE>* process, const char* name) {
	ASSERT(process, "Sanity");
	ASSERT(process->is_created_by_fork_exec() || process->is_created_by_exec(), "Sanity");
	ASSERT(process->threads().size() == 1, "Sanity");
	ASSERT(process->is_fork_exec_in_progress() || process->is_exec_in_progress(), "Sanity");
	ASSERT(name, "Sanity");

	auto by_name_range = _processes_by_name.equal_range(process->name());
	for (auto it = by_name_range.first; it != by_name_range.second; ++it) {
		if (process == it->second) {
			_processes_by_name.erase(it);
			process->set_name(name);
			_processes_by_name.emplace(process->name(), process);
			return;
		}
	}

	ASSERT(false, "Attempt to rename did not find a matching process");
}

//
// The "youngest" process/thread lookups are used during event processing,
// where we often must look up a process/thread that hasn't been updated
// to reflect current timespans. A time based lookup would fail.
//
template <typename SIZE>
MachineProcess<SIZE>* Machine<SIZE>::youngest_mutable_process(pid_t pid) {
	MachineProcess<SIZE>* youngest_process = nullptr;
	auto by_pid_range = _processes_by_pid.equal_range(pid);
	for (auto it = by_pid_range.first; it != by_pid_range.second; ++it) {
		MachineProcess<SIZE>& process = it->second;
		// Larger times are newer (younger)
		if (!youngest_process || process.timespan().location() > youngest_process->timespan().location()) {
			youngest_process = &process;
		}
	}

	return youngest_process;
}

template <typename SIZE>
MachineThread<SIZE>* Machine<SIZE>::youngest_mutable_thread(typename SIZE::ptr_t tid) {
	MachineThread<SIZE>* youngest_thread = nullptr;
	auto by_tid_range = _threads_by_tid.equal_range(tid);
	for (auto it = by_tid_range.first; it != by_tid_range.second; ++it) {
		MachineThread<SIZE>& thread = it->second;
		// Larger times are newer (younger)
		if (!youngest_thread || thread.timespan().location() > youngest_thread->timespan().location()) {
			youngest_thread = &thread;
		}
	}

	return youngest_thread;
}

//
// This function handles looking up a thread by tid. If neccessary, it will create a new thread
// and process. Any thread / process that are looked up or created will have their timestamps updated.
//
template <typename SIZE>
MachineThread<SIZE>* Machine<SIZE>::process_event_tid_lookup(typename SIZE::ptr_t tid, AbsTime now) {
	MachineThread<SIZE>* thread = youngest_mutable_thread(tid);

	if (!thread) {
		// This is an "unknown" thread. We have no information about its name or parent process.
		char unknown_process_name[20];
		snprintf(unknown_process_name, sizeof(unknown_process_name), "unknown-%llX", (uint64_t)tid);

		//
		// Strongly considering just requiring this to be valid, and never allowing an unknown thread.
		//
		printf("UNKNOWN TID FAIL! unknonwn tid %llx\n", (int64_t)tid);
		ASSERT(false, "unknown TID fail");

		MachineProcess<SIZE>* unknown_process = create_process(next_unknown_pid(), unknown_process_name, now, kMachineProcessFlag::IsUnknownProcess);
		thread = create_thread(unknown_process, tid, &UnsetVoucher, now, kMachineThreadFlag::CreatedByUnknownTidInTrace);
		unknown_process->add_thread(thread);
	}

	ASSERT(thread, "Sanity");
	ASSERT(!thread->is_trace_terminated(), "Event tid seen after trace termination");
	ASSERT(!thread->process().is_trace_terminated(), "Event pid seen after trace termination");

	return thread;
}

//
// See comments in task_policy.c for full explanation of trequested_0 & trequested_1.
//
// process_trequested_task means that the tracepoint either had a NULL thread, or specified that the tracepoint was targeted at task level.
// This only matters in 32 bit traces, where it takes both trequested_0 and trequested_1 to carry task or thread requested data.
//
// For now, there is nothing we want to see in thread_requested data.
//
template <typename SIZE>
void Machine<SIZE>::process_trequested_task(pid_t pid, typename SIZE::ptr_t trequested_0, typename SIZE::ptr_t trequested_1) {
	TaskRequestedPolicy task_requested = (SIZE::is_64_bit) ? TaskRequestedPolicy(trequested_0) : TaskRequestedPolicy((Kernel32::ptr_t)trequested_0, (Kernel32::ptr_t)trequested_1);

	if (uint32_t apptype = (uint32_t)task_requested.as_struct().t_apptype) {
		if (pid) {
			if (MachineProcess<SIZE>* target = youngest_mutable_process(pid)) {
				target->set_apptype_from_trequested(apptype);
			}
		}
	}
}

template <typename SIZE>
void Machine<SIZE>::process_trequested_thread(typename SIZE::ptr_t tid, typename SIZE::ptr_t trequested_0, typename SIZE::ptr_t trequested_1) {
	TaskRequestedPolicy task_requested = (SIZE::is_64_bit) ? TaskRequestedPolicy(trequested_0) : TaskRequestedPolicy((Kernel32::ptr_t)trequested_0, (Kernel32::ptr_t)trequested_1);

	if (uint32_t apptype = (uint32_t)task_requested.as_struct().t_apptype) {
		if (MachineThread<SIZE>* target_thread = youngest_mutable_thread(tid)) {
			target_thread->mutable_process().set_apptype_from_trequested(apptype);
		}
	}
}

#define AST_PREEMPT			0x01
#define AST_QUANTUM			0x02
#define AST_URGENT			0x04
#define AST_HANDOFF			0x08
#define AST_YIELD			0x10

#define TRACE_DATA_NEWTHREAD		0x07000004
#define TRACE_STRING_NEWTHREAD		0x07010004
#define TRACE_DATA_EXEC			0x07000008
#define TRACE_STRING_EXEC		0x07010008
#define TRACE_LOST_EVENTS		0x07020008

// From ./osfmk/i386/mp.c
#define	TRACE_MP_CPU_DEACTIVATE		MACHDBG_CODE(DBG_MACH_MP, 7)

// From osfmk/kern/task.h
#define TASK_POLICY_TASK                0x4
#define TASK_POLICY_THREAD              0x8

template <typename SIZE>
bool Machine<SIZE>::process_event(const KDEvent<SIZE>& event)
{
	ASSERT(!lost_events(), "Should not be processing events after TRACE_LOST_EVENTS");

	AbsTime now = event.timestamp();
	ASSERT(event.cpu() > -1 && event.cpu() < _cpus.size(), "cpu_id out of range");
	MachineCPU<SIZE>& cpu = _cpus[event.cpu()];

	if (!cpu.is_iop()) {
		//
		// If we have lost events, immediately bail.
		//
		// Pre-process events known to have bogus TID's:
		//
		// DBG_TRACE_INFO events may not have a valid TID.
		// MACH_IPC_VOUCHER_CREATE_ATTR_DATA do not have a valid TID,
		//

		switch (event.dbg_cooked()) {
			case TRACEDBG_CODE(DBG_TRACE_INFO, 1): // kernel_debug_early_end()
			case TRACEDBG_CODE(DBG_TRACE_INFO, 4): // kernel_debug_string()
				return true;

			case TRACEDBG_CODE(DBG_TRACE_INFO, 2): // TRACE_LOST_EVENTS
				set_flags(kMachineFlag::LostEvents);
				return false;

			case MACHDBG_CODE(DBG_MACH_IPC, MACH_IPC_VOUCHER_CREATE_ATTR_DATA):
				// trace event data is
				// data, data, data, data
				//
				// event tid is voucher address!
				if (auto voucher = process_event_voucher_lookup(event.tid(), UINT32_MAX)) {
					voucher->add_content_bytes(event.arg1_as_pointer());
				}
				return true;

			default:
				break;
		}

		MachineThread<SIZE>* event_thread = process_event_tid_lookup(event.tid(), now);

		switch (event.dbg_cooked()) {
			case BSDDBG_CODE(DBG_BSD_PROC, BSD_PROC_EXIT): // the exit syscall never returns, use this instead
								       // arg1 == pid exiting
								       // arg2 == exit code
				if (event.is_func_end()) {
					// can BSD_PROC_EXIT can be called from another context?
					ASSERT((pid_t)event.arg1() == event_thread->process().pid(), "proc_exit pid does not match context pid");
					event_thread->mutable_process().set_exit_by_syscall(now, (int32_t)event.arg2());
				}
				break;

			case TRACE_DATA_NEWTHREAD: {
				ASSERT(event.is_func_none(), "TRACE_DATA_NEWTHREAD event has start/end bit set. Should be func_none.");
				//
				// This is called by the thread creating the new thread.
				//
				// The thread id of the new thread is in arg1.
				// The pid of the process creating the new thread is in arg2.
				//
				// NOTE! This event carries enough information to create a thread, which we do.
				// However, the immediately following TRACE_STRING_NEWTHREAD does not have the
				// newly_created thread's tid. We cannot assume that we will always be able to
				// read these events as a pair, they may be split by a particularly unlucky
				// buffer snapshot.
				//
				// We have a "thread nursery" which we use to associate the tid of the new
				// thread with the creating thread.
				//
				// (During fork operations, the "parent" may appear different than the child,
				//  this is why we cannot reuse the parent's name and ignore the STRING event.)
				//
				auto new_thread_id = (typename SIZE::ptr_t)event.arg1();
				auto new_thread_pid = (int32_t)event.arg2();

				MachineProcess<SIZE>* new_process = youngest_mutable_process(new_thread_pid);
				kMachineThreadFlag new_thread_flags;

				//
				// Okay, it looks like we cannot pay much attention to the source of thread
				// creates, the system will create a thread for anyone at any time, in any
				// place. The new model is to lookup the pid of the new thread, and if it
				// exists and is live, use that. Otherwise, fork-exec a new process.
				//

				if (new_process) {
					new_thread_flags = kMachineThreadFlag::CreatedByTraceDataNewThread;
				} else {
					new_thread_flags = (kMachineThreadFlag)((uint32_t)kMachineThreadFlag::CreatedByForkExecEvent |
										(uint32_t)kMachineThreadFlag::IsMain);

					auto new_process_flags = (kMachineProcessFlag)((uint32_t)kMachineProcessFlag::CreatedByForkExecEvent |
										       (uint32_t)kMachineProcessFlag::IsForkExecInProgress);

					// Create the new process
					new_process = create_process(new_thread_pid, "###Fork#Exec###", now, new_process_flags);
				}
				ASSERT(new_process, "Sanity");
				ASSERT(!new_process->is_trace_terminated(), "Sanity");
				ASSERT(new_thread_pid != 0 || new_process == _kernel_task, "Sanity");
				new_process->add_thread(create_thread(new_process, new_thread_id, &UnsetVoucher, now, new_thread_flags));
				break;
			}

			case TRACEDBG_CODE(DBG_TRACE_DATA, TRACE_DATA_THREAD_TERMINATE): {
				ASSERT(event.is_func_none(), "Sanity");
				typename SIZE::ptr_t terminated_tid = event.arg1();
				// If tid == terminated_tid, we need to handle the lookup below differently
				ASSERT(event.tid() != terminated_tid, "Should not be possible");
				MachineThread<SIZE>* terminated_thread = process_event_tid_lookup(terminated_tid, now);
				terminated_thread->set_trace_terminated(now);

				// Was this the last thread for a given process?
				bool all_threads_trace_terminated = true;
				MachineProcess<SIZE>& process = terminated_thread->mutable_process();
				for (auto thread : process.threads()) {
					if (!thread->is_trace_terminated()) {
						all_threads_trace_terminated = false;
						break;
					}
				}

				if (all_threads_trace_terminated) {
					process.set_trace_terminated(now);
				}
				break;
			}

			case TRACE_DATA_EXEC: {
				ASSERT(event.is_func_none(), "TRACE_DATA_EXEC event has start/end bit set. Should be func_none.");

				ASSERT(!event_thread->is_trace_terminated(), "Thread that is trace terminated is exec'ing");
				ASSERT(!event_thread->process().is_kernel(), "Kernel process is exec'ing");
				ASSERT(!event_thread->is_idle_thread(), "IDLE thread is exec'ing");

				// arg1 == pid
				int32_t exec_pid = (int32_t)event.arg1();
				ASSERT(exec_pid != -1, "Kernel thread is exec'ing");
				ASSERT(exec_pid == event_thread->process().pid() || event_thread->process().is_unknown(), "Pids should match. If not, maybe vfork?");

				if (event_thread->process().is_fork_exec_in_progress()) {
					ASSERT(event_thread->process().threads().size() == 1, "Fork invariant violated");
					// event_thread->mutable_process().clear_fork_exec_in_progress();

					// Hmmm.. Do we need to propagate an apptype here?
				} else {
					//
					// Creating a new process will automagically clean up the
					// existing one, setting the last known timestamp, and "PidReused"
					//
					auto exec_thread_flags = (kMachineThreadFlag)((uint32_t)kMachineThreadFlag::CreatedByExecEvent |
										      (uint32_t)kMachineThreadFlag::IsMain);

					auto exec_process_flags = (kMachineProcessFlag)((uint32_t)kMachineProcessFlag::CreatedByExecEvent |
											(uint32_t)kMachineProcessFlag::IsExecInProgress);

					auto exec_process = create_process(exec_pid, "###Exec###", now, exec_process_flags);
					MachineThread<SIZE>* exec_thread = create_thread(exec_process, event_thread->tid(), &UnsetVoucher, now, exec_thread_flags);
					exec_process->add_thread(exec_thread);

					int32_t apptype = event_thread->process().apptype();
					if (apptype != -1) {
						exec_process->set_apptype(apptype);
					}
				}
				break;
			}

			case TRACE_STRING_EXEC: {
				ASSERT(event.is_func_none(), "TRACE_STRING_EXEC event has start/end bit set. Should be func_none.");
				ASSERT(event_thread->mutable_process().is_exec_in_progress() ||
				       event_thread->mutable_process().is_fork_exec_in_progress(), "Must be exec or fork-exec in progress to be here");

				set_process_name(&event_thread->mutable_process(), event.all_args_as_string().c_str());

				if (event_thread->process().is_exec_in_progress())
					event_thread->mutable_process().clear_exec_in_progress();
				else
					event_thread->mutable_process().clear_fork_exec_in_progress();
				break;
			}

			case MACHDBG_CODE(DBG_MACH_EXCP_INTR, 0):
				if (event.is_func_start()) {
					cpu.set_intr(now);
				} else {
					cpu.clear_intr(now);
				}
				break;

			case MACHDBG_CODE(DBG_MACH_SCHED, MACH_SCHED):
			case MACHDBG_CODE(DBG_MACH_SCHED, MACH_STACK_HANDOFF): {
				// The is deactivate switch to idle thread should have happened before we see an actual
				// context switch for this cpu.
				ASSERT(!cpu.is_deactivate_switch_to_idle_thread(), "State machine fail");

				typename SIZE::ptr_t handoff_tid = event.arg2();
				// If the handoff tid and the event_tid are the same, the lookup will fail an assert due to timestamps going backwards.
				MachineThread<SIZE>* handoff_thread = (handoff_tid == event.tid()) ? event_thread : process_event_tid_lookup(handoff_tid, now);
				ASSERT(handoff_thread, "Sanity");

				// If we marked a given thread as unrunnable in idle, or the MKRUNNABLE wasn't emitted, make sure we
				// mark the thread as runnable now.
				handoff_thread->make_runnable(now);
				cpu.context_switch(handoff_thread, event_thread, now);

				if (!event_thread->is_idle_thread()) {
					if (event_thread->tid() != event.arg2()) {
						if ((event.arg1() & (AST_PREEMPT | AST_QUANTUM | AST_URGENT | AST_HANDOFF | AST_YIELD)) == 0) {
							event_thread->make_unrunnable(now);
						}
					}
				}
				break;
			}

				//
				// There is a rare case of:
				//
				// event[795176] { timestamp=4b8074fa6bb5, arg1=0, arg2=0, arg3=0, arg4=0, tid=8ab77,  end MP_CPU_DEACTIVATE, cpu=1 }
				// event[795177] { timestamp=4b8074fa70bd, arg1=8ab77, arg2=ffffffffffffffff, arg3=0, arg4=4, tid=2d,  --- MACH_SCHED_CHOOSE_PROCESSOR, cpu=1 }
				//
				// When a cpu shuts down via MP_CPU_DEACTIVATE, on reactivation, the cpu does a forced switch to its idle thread,
				// without dropping a MACH_SCHED or MACH_STACK_HANDOFF. We want to catch this and update the cpu correctly, as
				// well as marking the idle thread.
				//
				// This is a desktop only codepath, TRACE_MP_CPU_DEACTIVATE is defined in ./osfmk/i386/mp.c
				//
			case TRACE_MP_CPU_DEACTIVATE:
				ASSERT(event_thread == cpu.thread() || !cpu.is_thread_state_initialized(), "Sanity");
				if (event.is_func_end()) {
					cpu.set_deactivate_switch_to_idle_thread();
				}
				break;

			case MACHDBG_CODE(DBG_MACH_SCHED, MACH_SCHED_CHOOSE_PROCESSOR):
				//
				// I have seen a sequence of events like this, where it appears that multiple threads get re-dispatched:
				//
				// event[254871] { timestamp=332dd22319b, arg1=0, arg2=0, arg3=0, arg4=0, tid=1b8ab,  end MP_CPU_DEACTIVATE, cpu=7 }
				// event[254876] { timestamp=332dd22387a, arg1=1b7d9, arg2=ffffffffffffffff, arg3=e, arg4=4, tid=1b8ab,  --- MACH_SCHED_CHOOSE_PROCESSOR, cpu=7 }
				// event[254877] { timestamp=332dd223c44, arg1=e, arg2=0, arg3=0, arg4=0, tid=1b8ab,  --- MACH_SCHED_REMOTE_AST, cpu=7 }
				// event[254887] { timestamp=332dd22441c, arg1=1b8ab, arg2=ffffffffffffffff, arg3=4, arg4=4, tid=53,  --- MACH_SCHED_CHOOSE_PROCESSOR, cpu=7 }
				//
				// We will wait until we see a tid mismatch before clearing the deactivate_switch state
				//
				if (cpu.is_deactivate_switch_to_idle_thread()) {
					if (cpu.thread() == NULL || event_thread->tid() != cpu.thread()->tid()) {
						// The choose tracepoint has the tid of the thread on cpu when it deactivated.
						ASSERT(cpu.thread() == NULL || cpu.thread()->tid() == event.arg1(), "Sanity");

						cpu.clear_deactivate_switch_to_idle_thread();
						event_thread->set_is_idle_thread();
						event_thread->make_runnable(now);
						cpu.context_switch(event_thread, cpu.thread(), now);
					}
				}
				break;

			case MACHDBG_CODE(DBG_MACH_SCHED, MACH_MAKE_RUNNABLE):
				event_thread->make_runnable(now);
				break;

			case MACHDBG_CODE(DBG_MACH_SCHED, MACH_IDLE):
				if (event.is_func_start()) {
					cpu.set_idle(now);
				} else {
					cpu.clear_idle(now);
				}
				break;

			case MACHDBG_CODE(DBG_MACH_VM, 2 /* MACH_vmfault is hardcoded as 2 */):
				if (event.is_func_start())
					event_thread->begin_vm_fault(now);
				else
					event_thread->end_vm_fault(now);
				break;

			case BSDDBG_CODE(DBG_BSD_MEMSTAT, BSD_MEMSTAT_JETSAM):
			case BSDDBG_CODE(DBG_BSD_MEMSTAT, BSD_MEMSTAT_JETSAM_HIWAT):
				if (event.is_func_end()) {
					if (pid_t pid = (pid_t)event.arg2()) {
						//
						// The time for this kill is already covered by the MEMSTAT_scan.
						// We still want to mark the victim process as jetsam killed, though.
						// We need to look up the victim, which is the pid in arg2.
						//
						if (MachineProcess<SIZE>* victim = youngest_mutable_process(pid)) {
			    ASSERT(!victim->is_exiting(), "Jetsam killing already dead process");
			    // This isn't technically impossible, but as a practical matter it is more likely
			    // signalling a bug than we were able to wrap the pid counter and reuse this pid
			    ASSERT(!victim->is_kernel(), "Cannot jetsam kernel");
			    victim->set_exit_by_jetsam(now);
						} else {
			    ASSERT(false, "Unable to find jetsam victim pid");
						}
					}
				}
				break;

			case BSDDBG_CODE(DBG_BSD_MEMSTAT, BSD_MEMSTAT_SCAN):
			case BSDDBG_CODE(DBG_BSD_MEMSTAT, BSD_MEMSTAT_UPDATE):
			case BSDDBG_CODE(DBG_BSD_MEMSTAT, BSD_MEMSTAT_FREEZE):
				if (event.is_func_start())
					event_thread->begin_jetsam_activity(event.dbg_cooked(), now);
				else
					event_thread->end_jetsam_activity(event.dbg_cooked(), now);
				break;

				//
				// IMP_TASK_APPTYPE trace args are:
				//
				// start:
				//	target_pid, trequested_0, trequested_1, apptype
				// end:
				// 	target_pid, trequested_0, trequested_1, is_importance_receiver
			case IMPORTANCE_CODE(IMP_TASK_APPTYPE, TASK_APPTYPE_NONE):
			case IMPORTANCE_CODE(IMP_TASK_APPTYPE, TASK_APPTYPE_DAEMON_INTERACTIVE):
			case IMPORTANCE_CODE(IMP_TASK_APPTYPE, TASK_APPTYPE_DAEMON_STANDARD):
			case IMPORTANCE_CODE(IMP_TASK_APPTYPE, TASK_APPTYPE_DAEMON_ADAPTIVE):
			case IMPORTANCE_CODE(IMP_TASK_APPTYPE, TASK_APPTYPE_DAEMON_BACKGROUND):
			case IMPORTANCE_CODE(IMP_TASK_APPTYPE, TASK_APPTYPE_APP_DEFAULT):
			case IMPORTANCE_CODE(IMP_TASK_APPTYPE, TASK_APPTYPE_APP_TAL):
				//
				// We want to set the explicit apptype now, and trequested will not have the
				// apptype data until the end event.
				//
				if (event.is_func_start()) {
					if (pid_t pid = (pid_t)event.arg1()) {
						if (MachineProcess<SIZE>* target = youngest_mutable_process(pid)) {
							target->set_apptype((uint32_t)event.arg4());
						}
					}
				}
				process_trequested_task((pid_t)event.arg1(), event.arg2(), event.arg3());
				break;

				// Trace data is
				// self_pid, audit_token_pid_from_task(task), trequested_0(task, NULL), trequested_1(task, NULL)
			case IMPORTANCE_CODE(IMP_TASK_SUPPRESSION, 0):
			case IMPORTANCE_CODE(IMP_TASK_SUPPRESSION, 1):
			case IMPORTANCE_CODE(IMP_BOOST, IMP_BOOSTED):
			case IMPORTANCE_CODE(IMP_BOOST, IMP_UNBOOSTED):
				process_trequested_task((pid_t)event.arg2(), event.arg3(), event.arg4());
				break;

			case MACHDBG_CODE(DBG_MACH_IPC, MACH_THREAD_SET_VOUCHER): {
				//
				// This can be invoked against another thread; you must use arg1 as the tid.
				//
				// thread-tid, name, voucher, callsite-id
				//
				auto set_thread_tid = event.arg1();
				MachineThread<SIZE>* set_thread = (set_thread_tid == event.tid()) ? event_thread : process_event_tid_lookup(set_thread_tid, now);
				set_thread->set_voucher(process_event_voucher_lookup(event.arg3(), UINT32_MAX), now);
				break;
			}

			case MACHDBG_CODE(DBG_MACH_IPC, MACH_IPC_VOUCHER_CREATE):
				// trace event data is
				// voucher address, voucher table size, system voucher count, voucher content bytes
				create_voucher(event.arg1(), now, kMachineVoucherFlag::CreatedByVoucherCreate, (uint32_t)event.arg4());
				break;

			case MACHDBG_CODE(DBG_MACH_IPC, MACH_IPC_VOUCHER_DESTROY):
				destroy_voucher(event.arg1(), now);
				break;

				// The MachMsg state chart...
				//
				// The "key" to the mach msg is the kmsg_addr.
				//
				// We can encounter a given kmsg_addr in any of
				// four possible states:
				//
				// UNINITIALIZED
				// SEND
				// RECV
				// FREE
				//
				// These are the legal state transitions:
				// (transition to UNINITIALIZED is not possible)
				//
				// UNIN -> SEND ; Accept as FREE -> SEND
				// UNIN -> RECV ; Accept as SEND -> RECV
				// UNIN -> FREE ; Accept as FREE -> FREE
				//
				// SEND -> SEND ; ERROR!
				// SEND -> RECV ; User to User IPC, send message to machine
				// SEND -> FREE ; User to Kernel IPC, recycle.
				//
				// RECV -> SEND ; ERROR!
				// RECV -> RECV ; ERROR!
				// RECV -> FREE ; End User IPC
				//
				// FREE -> SEND ; Begin User IPC
				// FREE -> RECV ; Kernel to User IPC
				// FREE -> FREE ; Kernel to Kernel IPC

			case MACHDBG_CODE(DBG_MACH_IPC, MACH_IPC_MSG_SEND): {
				// trace event data is:
				// kmsg_addr, msgh_bits, msgh_id, voucher_addr,
				auto kmsg_addr = event.arg1();
				auto msgh_bits = (uint32_t)event.arg2();
				auto msgh_id = (uint32_t)event.arg3();
				auto voucher_addr = event.arg4();

				auto nursery_it = _mach_msg_nursery.find(kmsg_addr);
				if (nursery_it == _mach_msg_nursery.end()) {
					nursery_it = _mach_msg_nursery.emplace(kmsg_addr, kmsg_addr).first;
				}

				auto& nursery_msg = nursery_it->second;

				switch (nursery_msg.state()) {
						// SEND -> SEND ; ERROR!
						// RECV -> SEND ; ERROR!
					case kNurseryMachMsgState::Send:
						ASSERT(false, "illegal state transition (SEND -> SEND) in nursery mach msg");
					case kNurseryMachMsgState::Recv:
						ASSERT(false, "illegal state transition (RECV -> SEND) in nursery mach msg");
						break;

						// UNIN -> SEND ; Accept as FREE -> SEND
						// FREE -> SEND ; Begin User IPC
					case kNurseryMachMsgState::Uninitialized:
					case kNurseryMachMsgState::Free: {
						uintptr_t event_index = &event - _events;
						nursery_msg.send(event_index, event.timestamp(), event.tid(), kmsg_addr, msgh_bits, msgh_id, process_event_voucher_lookup(voucher_addr, msgh_bits));
						break;
					}
				}
				// We do the state set here so that release builds
				// sync to current state when errors are encountered
				nursery_msg.set_state(kNurseryMachMsgState::Send);
				break;
			}

			case MACHDBG_CODE(DBG_MACH_IPC, MACH_IPC_MSG_RECV):
			case MACHDBG_CODE(DBG_MACH_IPC, MACH_IPC_MSG_RECV_VOUCHER_REFUSED):
			{
				// trace event data is
				// kmsg_addr, msgh_bits, msgh_id, voucher_addr
				auto kmsg_addr = event.arg1();
				auto msgh_bits = (uint32_t)event.arg2();
				auto voucher_addr = event.arg4();

				auto nursery_it = _mach_msg_nursery.find(kmsg_addr);
				if (nursery_it == _mach_msg_nursery.end()) {
					nursery_it = _mach_msg_nursery.emplace(kmsg_addr, kmsg_addr).first;
				}

				auto& nursery_msg = nursery_it->second;

				uint32_t flags = (event.dbg_code() == MACH_IPC_MSG_RECV_VOUCHER_REFUSED) ? kMachineMachMsgFlag::IsVoucherRefused : 0;
				uintptr_t event_index = &event - _events;

				switch (nursery_msg.state()) {

						// UNIN -> RECV ; Accept as SEND -> RECV
					case kNurseryMachMsgState::Uninitialized: {
						flags |= kMachineMachMsgFlag::HasReceiver;

						auto mach_msg_it = _mach_msgs.emplace(_mach_msgs.end(),
										      NurseryMachMsg<SIZE>::message_id(),
										      kmsg_addr,
										      flags,
										      AbsTime(0),
										      0,
										      0,
										      &Machine<SIZE>::UnsetVoucher,
										      now,
										      event.tid(),
										      msgh_bits,
										      process_event_voucher_lookup(voucher_addr, msgh_bits));

						ASSERT(_mach_msgs_by_event_index.find(event_index) == _mach_msgs_by_event_index.end(), "Stomping mach msg");
						_mach_msgs_by_event_index[event_index] = std::distance(_mach_msgs.begin(), mach_msg_it);
						break;
					}

						// SEND -> RECV ; User to User IPC, send message to machine
					case kNurseryMachMsgState::Send: {
						ASSERT(kmsg_addr == nursery_msg.kmsg_addr(), "Sanity");
						ASSERT((uint32_t)event.arg3() == nursery_msg.send_msgh_id(), "Sanity");

						flags |= (kMachineMachMsgFlag::HasSender | kMachineMachMsgFlag::HasReceiver);

						auto mach_msg_it = _mach_msgs.emplace(_mach_msgs.end(),
										      nursery_msg.id(),
										      kmsg_addr,
										      flags,
										      nursery_msg.send_time(),
										      nursery_msg.send_tid(),
										      nursery_msg.send_msgh_bits(),
										      nursery_msg.send_voucher(),
										      now,
										      event.tid(),
										      msgh_bits,
										      process_event_voucher_lookup(voucher_addr, msgh_bits));

						intptr_t send_event_index = nursery_msg.send_event_index();
						if (send_event_index != -1) {
							ASSERT(send_event_index < _event_count, "Sanity");
							ASSERT(_mach_msgs_by_event_index.find(event_index) == _mach_msgs_by_event_index.end(), "Stomping mach msg");
							_mach_msgs_by_event_index[send_event_index] = std::distance(_mach_msgs.begin(), mach_msg_it);
						}
						ASSERT(_mach_msgs_by_event_index.find(event_index) == _mach_msgs_by_event_index.end(), "Stomping mach msg");
						_mach_msgs_by_event_index[event_index] = std::distance(_mach_msgs.begin(), mach_msg_it);
						break;
					}

						// RECV -> RECV ; ERROR!
					case kNurseryMachMsgState::Recv:
						ASSERT(false, "illegal state transition (RECV -> RECV) in nursery mach msg");
						break;

						// FREE -> RECV ; Kernel to User IPC
					case kNurseryMachMsgState::Free:
						break;
				}

				// We do the state set here so that release builds
				// sync to current state when errors are encountered
				nursery_msg.set_state(kNurseryMachMsgState::Recv);
				break;
			}

			case MACHDBG_CODE(DBG_MACH_IPC, MACH_IPC_KMSG_FREE): {
				// trace event data is:
				// kmsg_addr

				auto kmsg_addr = event.arg1();

				auto nursery_it = _mach_msg_nursery.find(kmsg_addr);
				if (nursery_it == _mach_msg_nursery.end()) {
					nursery_it = _mach_msg_nursery.emplace(kmsg_addr, kmsg_addr).first;
				}

				auto& nursery_msg = nursery_it->second;


				// All transitions to FREE are legal.
				nursery_msg.set_state(kNurseryMachMsgState::Free);
			}

			default:
				// IO Activity
				//
				// There isn't an easy way to handle these inside the switch, The
				// code is used as a bitfield.


				//
				// Okay temp note on how to approach this.
				//
				// Even a single thread may have overlapping IO activity.
				// None of the current scheme's handle overlapped activity well.
				//
				// We'd like to be able to show for any given interval, "X pages IO outstanding, Y pages completed, Z ms waiting"
				//
				// To do that, we've got to be able to intersect an arbitrary interval with a pile of overlapping intervals.
				//
				// The approach is to accumulate the IO activity into a single vector.
				// Sort by interval.location().
				// Now flatten this interval (union flatten).
				// This will produce a second vector of non-overlapping intervals.
				// When we want to intersect the arbitrary interval, we do the standard search on the non overlapping interval vector.
				// This will give us a starting and ending location that guarantee to cover every IO that might intersect.
				//
				// The assumption is that while IO's overlap, they don't stay active forever. Sooner or later there will be a break.
				//
				// The arch-nemesis of this scheme is the light overlap, like so:
				//
				// XXXXX   XXXXXXXXXXX     XXXXXXXXXXXXXXXXXXXX
				//     XXXXXXX     XXXXXXXXXXXXXXXX      XXXXXXXXXXXXXX


				//
				// It turns out that IO can overlap inside a single thread, for example:
				//
				//	437719        C73AD5945   ---   P_RdDataAsync                      209b9f07 1000002  6b647    5000           2A72    1 gamed            293
				//	437724        C73AD5DCA   ---   P_RdDataAsync                      209b7e37 1000002  6b64c    6000           2A72    1 gamed            293
				//	437822        C73AD98B0   ---   P_RdDataAsyncDone                  209b7e37 4dfe3eef 0        0               191    1 kernel_task      0
				//	437829        C73AD9E55   ---   P_RdDataAsyncDone                  209b9f07 4dfe3eef 0        0               191    1 kernel_task      0
				//

				if (event.dbg_class() == DBG_FSYSTEM && event.dbg_subclass() == DBG_DKRW) {
					uint32_t code = event.dbg_code();
					//
					// Disk IO doesn't use func_start/func_end
					//
					// arg1 == uid
					// arg4 == size
					if (code & DKIO_DONE) {
						this->end_io(now, event.arg1());
					} else {

						// IO is initiated by a given process/thread, but it always finishes on a kernel_thread.
						// We need to stash enough data to credit the correct thread when the completion event arrives.
						begin_io(event_thread, now, event.arg1(), event.arg4());
					}
				} else if (event.dbg_class() == DBG_IMPORTANCE) {
					//
					// Every task policy set trace code carries "trequested" data, we would like to grab them all.
					//
					// This subclass spans the range of 0x20 through 0x3F
					//

					uint32_t subclass = event.dbg_subclass();
					if (subclass >= 0x20 && subclass <= 0x3F) {
						// Trace event data is
						// targetid(task, thread), trequested_0(task, thread), trequested_1(task, thread), value

						bool is_task_event = (event.dbg_code() & TASK_POLICY_TASK) > 0;

						// Should not be both a task and thread event.
						ASSERT(is_task_event != (event.dbg_code() & TASK_POLICY_THREAD), "BEWM!");

						if (is_task_event) {
							process_trequested_task((pid_t)event.arg1(), event.arg2(), event.arg3());
						} else {
							process_trequested_thread(event.arg1(), event.arg2(), event.arg3());
						}
					}
				}
				break;
		}
	}

	return true;
}

template <typename SIZE>
void Machine<SIZE>::initialize_cpu_idle_intr_states() {
	ASSERT(_event_count, "Sanity");
	ASSERT(_events, "Sanity");
	ASSERT(!_cpus.empty(), "Sanity");

	// How much work do we need to do?
	uint32_t inits_needed = 0;
	uint32_t inits_done = 0;
	for (auto& cpu : _cpus) {
		if (!cpu.is_iop()) {
			inits_needed += 3;

			if (cpu.is_intr_state_initialized()) {
				inits_done++;
			}
			if (cpu.is_idle_state_initialized()) {
				inits_done++;
			}
			if (cpu.is_thread_state_initialized()) {
				inits_done++;
			}
		}
	}

	uintptr_t index;
	for (index = 0; index < _event_count; ++index) {
		const KDEvent<SIZE>& event = _events[index];
		ASSERT(event.cpu() > -1 && event.cpu() < _cpus.size(), "cpu_id out of range");
		MachineCPU<SIZE>& cpu = _cpus[event.cpu()];

		if (!cpu.is_iop()) {
			switch (event.dbg_cooked()) {
				case TRACE_LOST_EVENTS:
					// We're done, give up.
					return;

				case MACHDBG_CODE(DBG_MACH_EXCP_INTR, 0):
					if (!cpu.is_intr_state_initialized()) {
						inits_done++;

						if (event.is_func_start()) {
							// If we are starting an INTR now, the cpu was not in INTR prior to now.
							cpu.initialize_intr_state(false, _events[0].timestamp());
						} else {
							// If we are ending an INTR now, the cpu was in INTR prior to now.
							cpu.initialize_intr_state(true, _events[0].timestamp());
						}
					}
					break;

				case MACHDBG_CODE(DBG_MACH_SCHED, MACH_IDLE):
					if (!cpu.is_idle_state_initialized()) {
						inits_done++;

						if (event.is_func_start()) {
							// If we are starting Idle now, the cpu was not Idle prior to now.
							cpu.initialize_idle_state(false, _events[0].timestamp());
						} else {
							// If we are ending Idle now, the cpu was Idle prior to now.
							cpu.initialize_idle_state(true, _events[0].timestamp());
						}
					}
					break;

					// I spent a day tracking this down....
					//
					// When you are actively sampling (say, every 100ms) on a machine
					// that is mostly idle, there will be long periods of VERY idle
					// cpus. So you might get a sample with no begin/end idle at all,
					// but the cpu is actually idle the entire time. Now suppose in
					// the next sample, you get a simple idle timeout in the middle,
					// and immdiately go back to idle. If we treat any TID found on
					// cpu as "running", we blow up because this segment appears to
					// have the idle thread "running".
					//
					// So, to do a proper thread init, we require actual scheduler
					// activity to tell us who the thread was.
				case MACHDBG_CODE(DBG_MACH_SCHED, MACH_SCHED):
				case MACHDBG_CODE(DBG_MACH_SCHED, MACH_STACK_HANDOFF):
					if (!cpu.is_thread_state_initialized()) {
						inits_done++;

						// We want to use the thread that *was* on cpu, not the thread being
						// handed off too.
						MachineThread<SIZE>* init_thread = youngest_mutable_thread(event.tid());
						// Legal for this to be NULL!
						cpu.initialize_thread_state(init_thread, _events[0].timestamp());

					}
					break;
			}
		}

		if (inits_done == inits_needed) {
			break;
		}
	}
}

template <typename SIZE>
void Machine<SIZE>::begin_io(MachineThread<SIZE>* thread, AbsTime begin_time, typename SIZE::ptr_t uid, typename SIZE::ptr_t size) {
	auto it = _io_by_uid.find(uid);
	if (it == _io_by_uid.end()) {
		_io_by_uid.emplace(uid, IOActivity<SIZE>(begin_time, AbsTime(0), thread, size));
	} else {
		// We shouldn't find a valid IO entry at the uid we're installing.
		ASSERT(it->second.thread() == NULL, "Overwriting existing io entry");
		ASSERT(it->second.location() == 0, "Overwriting existing io entry");
		
		it->second = IOActivity<SIZE>(begin_time, AbsTime(0), thread, size);
	}
}

template <typename SIZE>
void Machine<SIZE>::end_io(AbsTime end_time, typename SIZE::ptr_t uid) {
	auto it = _io_by_uid.find(uid);
	
	// Its okay to not find a match, if a trace begins with a Done event, for example.
	if (it != _io_by_uid.end()) {
		MachineThread<SIZE>* io_thread = it->second.thread();
		AbsTime begin_time = it->second.location();
		ASSERT(end_time > it->second.location(), "Sanity");
		
		_all_io.emplace_back(begin_time, end_time - begin_time, io_thread, it->second.size());
		
		DEBUG_ONLY({
			it->second.set_thread(NULL);
			it->second.set_location(AbsTime(0));
			it->second.set_length(AbsTime(0));
		})
	}
}
