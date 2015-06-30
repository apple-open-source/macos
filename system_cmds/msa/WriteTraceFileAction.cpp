//
//  WriteTraceFileAction.cpp
//  system_cmds
//
//  Created by James McIlree on 4/29/14.
//
//

#include "global.h"

static bool shouldProcessEvents;
static uint32_t sigintCount;

static bool start_tracing(Globals& globals)
{
	if (!KDBG::reset()) return false;
	if (!KDBG::set_buffer_capacity(globals.trace_buffer_size())) return false;
	if (!KDBG::set_nowrap(false)) return false;
	if (!KDBG::initialize_buffers()) return false;
	if (!KDBG::set_enabled(KDEBUG_ENABLE_TRACE)) return false;

	return true;
}

static void end_tracing(void)
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

void WriteTraceFileAction::execute(Globals& globals) {
	FileDescriptor fd(open(_path.c_str(), O_TRUNC|O_WRONLY|O_CREAT, 0777));
	if (!fd.is_open()) {
		log_msg(ASL_LEVEL_ERR, "Unable to write to %s\n", _path.c_str());
		return;
	}

	shouldProcessEvents = true;
	sigintCount = 0;

	VoucherContentSysctl contents(globals.should_trace_voucher_contents());

	AbsTime t1 = AbsTime::now();
	if (start_tracing(globals)) {
		// We cannot write the "maps" until after tracing has started.
		if (KDBG::write_maps(fd)) {
			signal(SIGINT, signal_handler_ctrl_C);

			while (shouldProcessEvents) {
				int events_written = KDBG::write_events(fd);
				AbsTime t2 = AbsTime::now();
				if (events_written != -1) {
					printf("wrote %d events - elapsed time = %.1f secs\n", events_written, (double)(t2 - t1).nano_time().value() / (double)NANOSECONDS_PER_SECOND);
				} else {
					log_msg(ASL_LEVEL_WARNING, "write events returned -1\n");
					break;
				}
				t1 = t2;
			}

			signal(SIGINT, SIG_DFL);
		}
	}

	end_tracing();
}
