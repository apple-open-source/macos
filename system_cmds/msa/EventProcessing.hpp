//
//  EventProcessing.hpp
//  msa
//
//  Created by James McIlree on 2/5/14.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#ifndef msa_EventProcessing_hpp
#define msa_EventProcessing_hpp

template <typename SIZE>
bool is_mach_msg_interesting(const Machine<SIZE>& machine, const MachineMachMsg<SIZE>* mach_msg)
{
	// If this message is carrying importance, it is interesting.
	if ((mach_msg->has_sender() && MACH_MSGH_BITS_RAISED_IMPORTANCE(mach_msg->send_msgh_bits())) ||
	    (mach_msg->has_receiver() && MACH_MSGH_BITS_RAISED_IMPORTANCE(mach_msg->recv_msgh_bits())))
		return true;

	// If this message has a non-null voucher, it is interesting.
	if ((mach_msg->has_sender() && !mach_msg->send_voucher()->is_null()) ||
	    (mach_msg->has_receiver() && !mach_msg->recv_voucher()->is_null()))
		return true;

	// If the message does NOT have a voucher, and the sender has a voucher set, it is interesting.
	if (mach_msg->has_sender()) {
		if (const MachineThread<SIZE>* sender_thread = machine.thread(mach_msg->send_tid(), mach_msg->send_time())) {
			const MachineVoucher<SIZE>* sender_voucher = sender_thread->voucher(mach_msg->send_time());
			if (!sender_voucher->is_unset() && !sender_voucher->is_null()) {
				return true;
			}
		}
	}

	return false;
}

template <typename SIZE>
void reset_task_data_on_exec_or_exit(const MachineProcess<SIZE>& process,
				     std::unordered_map<pid_t, bool>& task_appnap_state,
				     std::unordered_map<pid_t, TaskRequestedPolicy>& task_requested_state,
				     std::unordered_map<pid_t, std::pair<TaskEffectivePolicy, uint32_t>>& task_effective_state,
				     std::unordered_map<pid_t, std::pair<uint32_t, uint32_t>>& task_boosts)
{
	ASSERT(!process.is_kernel(), "Kernel process should not ever exec or exit");
	ASSERT(process.pid() > 0, "Process with pid less than 1 exec'd ?");

	if (pid_t pid = process.pid()) {
		auto task_appnap_it = task_appnap_state.find(pid);
		if (task_appnap_it != task_appnap_state.end()) {
			task_appnap_state.erase(task_appnap_it);
		}

		auto task_requested_it = task_requested_state.find(pid);
		if (task_requested_it != task_requested_state.end()) {
			task_requested_state.erase(task_requested_it);
		}

		auto task_effective_it = task_effective_state.find(pid);
		if (task_effective_it != task_effective_state.end()) {
			task_effective_state.erase(task_effective_it);
		}

		auto task_boosts_it = task_boosts.find(pid);
		if (task_boosts_it != task_boosts.end()) {
			task_boosts.erase(task_boosts_it);
		}
	}
}

// From osfmk/kern/task.h
#define TASK_POLICY_INTERNAL            0x0
#define TASK_POLICY_EXTERNAL            0x1

#define TASK_POLICY_TASK                0x4
#define TASK_POLICY_THREAD              0x8

template <typename SIZE>
void process_events(Globals& globals,
		    const Machine<SIZE>& machine,
		    std::unordered_map<pid_t, bool>& task_appnap_state,
		    std::unordered_map<pid_t, TaskRequestedPolicy>& task_requested_state,
		    std::unordered_map<typename SIZE::ptr_t, TaskRequestedPolicy>& thread_requested_state,
		    std::unordered_map<pid_t, std::pair<TaskEffectivePolicy, uint32_t>>& task_effective_state,
		    std::unordered_map<typename SIZE::ptr_t, std::pair<TaskEffectivePolicy, uint32_t>>& thread_effective_state,
		    std::unordered_map<pid_t, std::pair<uint32_t, uint32_t>>& task_boosts)
{
	const KDEvent<SIZE>* events = machine.events();
	uintptr_t count = machine.event_count();

	ASSERT(count, "Expected at least one event");

	//
	// Filtering thoughts...
	//
	// Two levels of filtering.
	//
	// 1) global supression of events that are "uninteresting".
	//
	// We filter on each event "class", with a keyword, so something like
	//
	// --lifecycle [ all | user | none ]			;; This is fork, exec, exit, thread-create, thread-exit
	// --mach-msgs [ all | user | voucher | none ]		;; This is all mach msgs
	//
	// 2) targetted supression of events that are not related to a user focus.
	//
	// We filter by process name/pid
	//
	// --track [ pid | name ]
	//

	PrintBuffer print_buffer(8192, 1024, globals.output_fd());

	for (uintptr_t index=0; index < count; ++index) {
		const KDEvent<SIZE>& event = events[index];

		//
		// Printing ...
		//

		switch (event.dbg_cooked()) {
			case TRACE_DATA_EXEC: {
				bool should_print = false;
				if (globals.lifecycle_filter() >= kLifecycleFilter::User)
					should_print = true;

				if (should_print)
					print_generic(print_buffer, globals, machine, event, index, "exec");

				if (const MachineThread<SIZE>* exec_thread = machine.thread(event.tid(), event.timestamp())) {
					reset_task_data_on_exec_or_exit(exec_thread->process(), task_appnap_state, task_requested_state, task_effective_state, task_boosts);
				}
				break;
			}

			case TRACE_DATA_NEWTHREAD: {
				bool should_print = false;
				auto new_thread_tid = (typename SIZE::ptr_t)event.arg1();
				if (const MachineThread<SIZE>* new_thread = machine.thread(new_thread_tid, event.timestamp())) {
					switch (globals.lifecycle_filter()) {
						case kLifecycleFilter::None:
							break;
						case kLifecycleFilter::User:
							if (!new_thread->process().is_kernel())
								should_print = true;
							break;
						case kLifecycleFilter::All:
							should_print = true;
							break;
					}

					if (should_print) {
						auto& new_process = new_thread->process();
						ASSERT(new_process.pid() == (pid_t)event.arg2(), "Pid does not match");
						if (new_process.timespan().location() == event.timestamp()) {
							print_fork(print_buffer, globals, machine, event, index, new_process);
						}

						// We're not printing the actual event data, but instead the exiting thread's data:
						print_base(print_buffer, globals,  event.timestamp(), new_thread, event, index, "thread-create", true);
					}
				}
				break;
			}

			case TRACEDBG_CODE(DBG_TRACE_DATA, TRACE_DATA_THREAD_TERMINATE): {
				// This event may spawn two prints
				//
				// 1) thread termination
				// 2) task termination
				bool should_print = false;
				typename SIZE::ptr_t terminated_tid = event.arg1();
				if (const MachineThread<SIZE>* terminated_thread = machine.thread(terminated_tid, event.timestamp())) {
					switch (globals.lifecycle_filter()) {
						case kLifecycleFilter::None:
							break;
						case kLifecycleFilter::User:
							if (!terminated_thread->process().is_kernel())
								should_print = true;
							break;
						case kLifecycleFilter::All:
							should_print = true;
							break;
					}

					if (should_print) {
						// We're not printing the actual event data, but instead the exiting thread's data:
						print_base(print_buffer, globals,  event.timestamp(), terminated_thread, event, index, "thread-exit", true);
					}

					// Was this the last thread in the process? (Do we also need to print a process exit?)
					auto& terminated_process = terminated_thread->process();
					if (terminated_process.is_trace_terminated()) {
						if (event.timestamp() >= terminated_process.exit_timestamp()) {
							if (should_print) {
								print_exit(print_buffer, globals,  event, terminated_thread, index);
							}
							reset_task_data_on_exec_or_exit(terminated_process, task_appnap_state, task_requested_state, task_effective_state, task_boosts);
						}
					}

					auto thread_requested_it = thread_requested_state.find(terminated_tid);
					if (thread_requested_it != thread_requested_state.end()) {
						thread_requested_state.erase(thread_requested_it);
					}

					auto thread_effective_it = thread_effective_state.find(terminated_tid);
					if (thread_effective_it != thread_effective_state.end()) {
						thread_effective_state.erase(thread_effective_it);
					}
				} else
					ASSERT(false, "Failed to find exit thread");
				break;
			}

			case MACHDBG_CODE(DBG_MACH_IPC, MACH_IPC_MSG_SEND): {
				// trace event data is:
				// kmsg_addr, msgh_bits, msgh_id, voucher_addr,

				// FIX ME!
				//
				// For now, we aren't recording mach msg's with endpoints in
				// the kernel. If we don't find a mach msg, assume its a kernel
				// msg.
				if (const MachineMachMsg<SIZE>* mach_msg = machine.mach_msg(index)) {
					if (is_mach_msg_interesting(machine, mach_msg)) {
						print_mach_msg(print_buffer, globals, machine, event, index, true, mach_msg);
					}
				}
				break;
			}

			case MACHDBG_CODE(DBG_MACH_IPC, MACH_IPC_MSG_RECV):
			case MACHDBG_CODE(DBG_MACH_IPC, MACH_IPC_MSG_RECV_VOUCHER_REFUSED): {
				// trace event data is
				// kmsg_addr, msgh_bits, msgh_id, recv_voucher_addr

				// FIX ME!
				//
				// For now, we aren't recording mach msg's with endpoints in
				// the kernel. If we don't find a mach msg, assume its a kernel
				// msg.
				if (const MachineMachMsg<SIZE>* mach_msg = machine.mach_msg(index)) {
					if (is_mach_msg_interesting(machine, mach_msg)) {
						print_mach_msg(print_buffer, globals, machine, event, index, false, mach_msg);
					}
				}
				break;
			}

			case MACHDBG_CODE(DBG_MACH_IPC, MACH_IPC_VOUCHER_CREATE): {
				// trace event data is
				// voucher address, voucher table size, system voucher count, voucher content bytes

				if (auto voucher = machine.voucher(event.arg1(), event.timestamp())) {
					print_voucher(print_buffer, globals, machine, event, index, "voucher_create", voucher, true);

					if (voucher->has_valid_contents()) {
						print_voucher_contents(print_buffer, globals, machine, event, index, voucher);
					}
				} else {
					ASSERT(false, "Failed to find voucher");
				}
				break;
			}

			case MACHDBG_CODE(DBG_MACH_IPC, MACH_IPC_VOUCHER_DESTROY): {
				// trace event data is
				// voucher address, 0, system voucher count, 0
				if (auto voucher = machine.voucher(event.arg1(), event.timestamp())) {
					print_voucher(print_buffer, globals, machine, event, index, "voucher_destroy", voucher, false);
				} else {
					ASSERT(false, "Failed to find voucher");
				}
				break;
			}

			case MACHDBG_CODE(DBG_MACH_IPC, MACH_THREAD_SET_VOUCHER): {
				print_generic(print_buffer, globals, machine, event, index, "thread_adopt_voucher");
				break;
			}

			case IMPORTANCE_CODE(IMP_ASSERTION, IMP_EXTERN):
				print_importance_assert(print_buffer, globals, machine, event, index, "externalize_importance", task_boosts);
				break;

			case IMPORTANCE_CODE(IMP_ASSERTION, IMP_HOLD | TASK_POLICY_EXTERNAL):
			case IMPORTANCE_CODE(IMP_ASSERTION, IMP_HOLD | TASK_POLICY_INTERNAL):
				print_importance_assert(print_buffer, globals, machine, event, index, "importance_hold", task_boosts);
				break;

			case IMPORTANCE_CODE(IMP_ASSERTION, IMP_DROP | TASK_POLICY_EXTERNAL):
			case IMPORTANCE_CODE(IMP_ASSERTION, IMP_DROP | TASK_POLICY_INTERNAL):
				print_importance_assert(print_buffer, globals, machine, event, index, "importance_drop", task_boosts);
				break;

			case IMPORTANCE_CODE(IMP_WATCHPORT, 0):
				// trace data is
				// proc_selfpid(), pid, boost, released_pid, 0);
				if (event.arg3() > 0) {
					print_watchport_importance_transfer(print_buffer, globals, machine, event, index);
				}
				break;

			case IMPORTANCE_CODE(IMP_TASK_SUPPRESSION, 0):
			case IMPORTANCE_CODE(IMP_TASK_SUPPRESSION, 1):
				// Trace data is
				// self_pid, audit_token_pid_from_task(task), trequested_0(task, NULL), trequested_1(task, NULL)
				print_appnap(print_buffer, globals, machine, event, index, (bool)event.dbg_code(), task_appnap_state, task_requested_state);
				break;

			case IMPORTANCE_CODE(IMP_BOOST, IMP_BOOSTED):
			case IMPORTANCE_CODE(IMP_BOOST, IMP_UNBOOSTED):
				// trace data is
				// proc_selfpid(), audit_token_pid_from_task(task), trequested_0(task, NULL), trequested_1(task, NULL)
				if (event.is_func_start()) {
					print_boost(print_buffer, globals, machine, event, index, (pid_t)event.arg2(), (event.dbg_code() == IMP_BOOSTED));
				}
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
				if (event.is_func_end()) {
					print_importance_apptype(print_buffer, globals, machine, event, index);
				}
				// FIX ME, not handling trequested status.
				//
				// process_trequested_task(print_buffer, globals, machine, event, index, (pid_t)event.arg1(), event.arg2(), event.arg3(), task_requested_policies);
				break;


			case IMPORTANCE_CODE(IMP_UPDATE, (IMP_UPDATE_TASK_CREATE | TASK_POLICY_TASK)):
				// trace data is
				// targetpid, teffective_0(task, NULL), teffective_1(task, NULL), tpriority(task, NULL)
				print_importance_update_task(print_buffer, globals, machine, event, index, "imp_update_task_create", task_effective_state);
				break;

			case IMPORTANCE_CODE(IMP_UPDATE, (IMP_UPDATE_TASK_CREATE | TASK_POLICY_THREAD)):
				// trace data is
				// targettid, teffective_0(task, thread), teffective_1(task, thread), tpriority(thread, NULL)
				print_importance_update_thread(print_buffer, globals, machine, event, index, "imp_update_thread_create", thread_effective_state);
				break;

			case IMPORTANCE_CODE(IMP_UPDATE, TASK_POLICY_TASK):
				// trace data is
				// targetpid, teffective_0(task, NULL), teffective_1(task, NULL), tpriority(task, THREAD_NULL)
				print_importance_update_task(print_buffer, globals, machine, event, index, "imp_update_task", task_effective_state);
				break;

			case IMPORTANCE_CODE(IMP_UPDATE, TASK_POLICY_THREAD):
				// trace data is
				// targettid, teffective_0(task, thread), teffective_1(task, thread), tpriority(task, THREAD_NULL)
				print_importance_update_thread(print_buffer, globals, machine, event, index, "imp_update_thread", thread_effective_state);
				break;

			case IMPORTANCE_CODE(IMP_MSG, IMP_MSG_SEND):
				// trace data is
				// current_pid, sender_pid, imp_msgh_id, (bool)importance_cleared

				// NOTE! Only end events carry "importance cleared"
				if (event.is_func_end() && (event.arg4() != 0)) {
					print_importance_send_failed(print_buffer, globals, machine, event, index);
				}
				break;

			case IMPORTANCE_CODE(IMP_MSG, IMP_MSG_DELV): {
				// trace data is
				// sending_pid, task_pid /* recv_pid?? */, msgh_id, impresult
				//
				// for impresult:
				//
				// 0: BOOST NOT APPLIED
				// 1: BOOST EXTERNALIZED
				// 2: LIVE_IMPORTANCE_LINKAGE!
				print_impdelv(print_buffer, globals, machine, event, index, (pid_t)event.arg1(), (uint32_t)event.arg4());
				break;
			}

			default:
				if (event.dbg_class() == DBG_IMPORTANCE) {
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
							// FIX ME, not handling trequested status.
							//
							// process_trequested_task(print_buffer, globals, machine, event, index, (pid_t)event.arg1(), event.arg2(), event.arg3(), task_requested_policies);
						} else {
							// FIX ME, not handling trequested status.
							//
							// process_trequested_thread(print_buffer, globals, machine, event, index, event.arg1(), event.arg2(), event.arg3(), task_requested_policies, thread_requested_policies);
						}
					}
				}
				break;
		}
	}
}

#endif
