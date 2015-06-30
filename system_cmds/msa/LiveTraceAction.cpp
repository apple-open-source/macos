//
//  LiveTraceAction.cpp
//  msa
//
//  Created by James McIlree on 2/4/14.
//  Copyright (c) 2014 Apple. All rights reserved.
//

#include "global.h"

// Force materialization of the ring buffer print methods,
// so they can be called from the debugger.
template class EventRingBuffer<Kernel32>;
template class EventRingBuffer<Kernel64>;

static bool shouldProcessEvents;
static uint32_t sigintCount;

static bool start_live_tracing(Globals& globals)
{
	if (!KDBG::reset()) return false;
	if (!KDBG::set_buffer_capacity(globals.trace_buffer_size())) return false;
	if (!KDBG::set_nowrap(false)) return false;
	if (!KDBG::initialize_buffers()) return false;
	if (!KDBG::set_enabled(KDEBUG_ENABLE_TRACE)) return false;

	return true;
}

static void end_live_tracing(void)
{
	KDBG::reset();
}

static void signal_handler_ctrl_C(int sig)
{
	shouldProcessEvents = false;
	if (++sigintCount >= 5) {
		// Not responding, nuke it from orbit.
		exit(1);
	}
}

template <typename SIZE>
static void live_trace_event_loop(Globals& globals)
{
	// Handle ctrl-C
	shouldProcessEvents = true;
	sigintCount = 0;
	
	while (shouldProcessEvents) {
		signal(SIGINT, signal_handler_ctrl_C);

		EventRingBuffer<SIZE> ring_buffer(globals, globals.trace_buffer_size() * 2);

		{
			char buf[PATH_MAX];
			char* buf_end = buf + sizeof(buf);
			print_mach_msg_header(buf, buf_end, globals);
			dprintf(globals.output_fd(), "%s", buf);
		}

		VoucherContentSysctl contents(globals.should_trace_voucher_contents());
		
		if (start_live_tracing(globals)) {

			// Okay, our goal is to hit specific timeposts.
			// IOW, if our target is every 10ms, and we spend 3ms doing work,
			// we sleep 7ms.
			AbsTime traceUpdateIntervalAbs = globals.live_update_interval();
			AbsTime now, next_trace_update = AbsTime::now();
			std::unique_ptr<Machine<SIZE>> machine, last_machine;

			std::unordered_map<pid_t, bool> task_appnap_state;
			std::unordered_map<pid_t, TaskRequestedPolicy> task_requested_state;
			std::unordered_map<typename SIZE::ptr_t, TaskRequestedPolicy> thread_requested_state;
			std::unordered_map<pid_t, std::pair<TaskEffectivePolicy, uint32_t>> task_effective_state;
			std::unordered_map<typename SIZE::ptr_t, std::pair<TaskEffectivePolicy, uint32_t>> thread_effective_state;
			std::unordered_map<pid_t, std::pair<uint32_t, uint32_t>> task_boosts;

			while (shouldProcessEvents) {
				now = AbsTime::now();
				if (now >= next_trace_update) {
					std::size_t count, capacity;
					KDEvent<SIZE>* events;

					std::tie(events, count, capacity) = ring_buffer.read();
					if (count) {
						if (last_machine) {
							machine = std::make_unique<Machine<SIZE>>(*last_machine, events, count);
						} else {
							auto state = KDBG::state();
							auto threadmap = KDBG::threadmap<SIZE>(state);
							auto cpumap = KDBG::cpumap();
                                                        machine = std::make_unique<Machine<SIZE>>(cpumap.data(), (uint32_t)cpumap.size(),
                                                                                                  threadmap.data(), (uint32_t)threadmap.size(),
                                                                                                  events, count);

							if (globals.should_zero_base_timestamps() && count) {
								globals.set_beginning_of_time(events[0].timestamp());
							} else {
								globals.set_beginning_of_time(AbsTime(0));
							}
						}

						if (!machine->lost_events()) {
							process_events(globals, *machine, task_appnap_state, task_requested_state, thread_requested_state, task_effective_state, thread_effective_state, task_boosts);

							// We read to the end of the ring buffer, and there are
							// more events to process. Do not risk an overflow, process
							// them immediately.

							// If count == capacity, we read to the end of the ring buffer,
							// and should immediately re-read.
							if (count < capacity) {
								next_trace_update += traceUpdateIntervalAbs;
								if (next_trace_update <= now) {
									printf("WARNING - falling behind on event processing\n");
									// Reset so if we do catch up, we don't spin on a clock
									// that has fallen seconds behind.
									next_trace_update = AbsTime::now();
								}
							}
						} else {
							printf("LOST EVENTS, exiting...\n");
							shouldProcessEvents = false;
						}

						last_machine = std::move(machine);
					}
				}

				mach_wait_until(next_trace_update.value());
			}
		} else {
			printf("Unable to enable tracing.\n");
			shouldProcessEvents = false;
		}

		signal(SIGINT, SIG_DFL);
	}

	// Final cleanup here to make sure partial initialization is
	// cleaned up.
	end_live_tracing();
}

void LiveTraceAction::execute(Globals& globals) {
	// Initial state snapshot, is another program using the trace buffer, etc.
	try {
		KDState state = KDBG::state();
		if (state.is_initialized() || state.controlling_pid() > 0) {
			if (state.controlling_pid() != getpid()) {
				if (state.controlling_pid() > 0 && kill(state.controlling_pid(), 0) == -1 && errno == ESRCH) {
					if (globals.is_verbose()) {
						printf("Reclaiming trace buffer control from pid %d\n", state.controlling_pid());
					}
				} else {
					printf("Another process is using the trace facility, possibly pid %d\n", state.controlling_pid());
					exit(1);
				}
			}
		}

		try {
			if (state.is_lp64()) {
				live_trace_event_loop<Kernel64>(globals);
			} else {
				live_trace_event_loop<Kernel32>(globals);
			}
		} catch (const std::exception& e) {
			log_msg(ASL_LEVEL_WARNING, "Caught exception in %s:\n %s\n", __PRETTY_FUNCTION__, e.what());
			KDBG::reset();
		}

	} catch (Exception& e) {
		if (getuid() != 0) {
			printf("Unable to acquire trace buffer state. You must be root.\n");
			exit(1);
		} else {
			usage(e.what());
		}
	}
}
