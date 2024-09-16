// Copyright (c) 2018-2019 Apple Inc.  All rights reserved.

#include <CoreSymbolication/CoreSymbolication.h>
#include <darwintest.h>
#include <dispatch/dispatch.h>
#include <kperf/kperf.h>
#include <ktrace/session.h>
#include <ktrace/private.h>
#include <System/sys/kdebug.h>
#include <pthread.h>

#include "kperf_helpers.h"
#include "ktrace_helpers.h"
#include "ktrace_meta.h"

#define CALLSTACK_VALID 0x1
#define CALLSTACK_TRUNCATED 0x10

T_GLOBAL_META(T_META_TAG_VM_PREFERRED);

static void
expect_frame(const char **bt, unsigned int bt_len, CSSymbolRef symbol,
    uint64_t addr, unsigned int bt_idx, unsigned int max_frames)
{
	const char *name;
	unsigned int frame_idx = max_frames - bt_idx - 1;

	if (!bt[frame_idx]) {
		T_LOG("frame %2u: skipping system frame '%s'", frame_idx,
		    CSSymbolGetName(symbol));
		return;
	}

	T_LOG("checking frame %d: %llx", bt_idx, addr);
	if (CSIsNull(symbol)) {
		T_FAIL("invalid return address symbol");
		return;
	}

	if (frame_idx >= bt_len) {
		T_FAIL("unexpected frame '%s' (%#" PRIx64 ") at index %u",
		    CSSymbolGetName(symbol), addr, frame_idx);
		return;
	}

	name = CSSymbolGetName(symbol);
	T_QUIET; T_ASSERT_NOTNULL(name, NULL);
	T_EXPECT_EQ_STR(name, bt[frame_idx],
	    "frame %2u: saw '%s', expected '%s'",
	    frame_idx, name, bt[frame_idx]);
}

/*
 * Expect to see either user or kernel stacks on thread with ID `tid` with a
 * signature of `bt` of length `bt_len`.  Updates `stacks_seen` when stack
 * is found.
 *
 * Can also allow stacks to be larger than the signature -- additional frames
 * near the current PC will be ignored.  This allows stacks to potentially be
 * in the middle of a signalling system call (which signals that it is safe to
 * start sampling).
 */
static void
expect_backtrace(ktrace_session_t s, uint64_t tid, unsigned int *stacks_seen,
    bool kern, const char **bt, unsigned int bt_len, unsigned int allow_larger_by)
{
	CSSymbolicatorRef symb;
	uint32_t hdr_debugid;
	uint32_t data_debugid;
	__block unsigned int stacks = 0;
	__block unsigned int frames = 0;
	__block unsigned int hdr_frames = 0;
	__block unsigned int allow_larger = allow_larger_by;

	T_SETUPBEGIN;

	if (kern) {
		static CSSymbolicatorRef kern_symb;
		static dispatch_once_t kern_symb_once;

		hdr_debugid = PERF_STK_KHDR;
		data_debugid = PERF_STK_KDATA;

		dispatch_once(&kern_symb_once, ^(void) {
			kern_symb = CSSymbolicatorCreateWithMachKernel();
			T_QUIET; T_ASSERT_FALSE(CSIsNull(kern_symb), NULL);
		});
		symb = kern_symb;
	} else {
		static CSSymbolicatorRef user_symb;
		static dispatch_once_t user_symb_once;

		hdr_debugid = PERF_STK_UHDR;
		data_debugid = PERF_STK_UDATA;

		dispatch_once(&user_symb_once, ^(void) {
			user_symb = CSSymbolicatorCreateWithTask(mach_task_self());
			T_QUIET; T_ASSERT_FALSE(CSIsNull(user_symb), NULL);
			T_QUIET; T_ASSERT_TRUE(CSSymbolicatorIsTaskValid(user_symb), NULL);
		});
		symb = user_symb;
	}

	ktrace_events_single(s, hdr_debugid, ^(struct trace_point *tp) {
		if (tid != 0 && tid != tp->threadid) {
			return;
		}

		T_LOG("found %s stack from thread %#" PRIx64, kern ? "kernel" : "user",
				tp->threadid);
		stacks++;
		if (!(tp->arg1 & 1)) {
			T_FAIL("invalid %s stack on thread %#" PRIx64,
			kern ? "kernel" : "user", tp->threadid);
			return;
		}

		hdr_frames = (unsigned int)tp->arg2;
		/* ignore extra link register or value pointed to by stack pointer */
		hdr_frames -= 1;

		T_QUIET; T_EXPECT_GE(hdr_frames, bt_len,
				"at least %u frames in header", bt_len);
		T_QUIET; T_EXPECT_LE(hdr_frames, bt_len + allow_larger,
				"at most %u + %u frames in header", bt_len, allow_larger);
		if (hdr_frames > bt_len && allow_larger > 0) {
			allow_larger = hdr_frames - bt_len;
			hdr_frames = bt_len;
		}

		T_LOG("%s stack seen", kern ? "kernel" : "user");
		frames = 0;
	});

	ktrace_events_single(s, data_debugid, ^(struct trace_point *tp) {
		if (tid != 0 && tid != tp->threadid) {
			return;
		}

		int i = 0;

		if (frames == 0 && hdr_frames > bt_len) {
			/* skip frames near the PC */
			i = (int)allow_larger;
			allow_larger -= 4;
		}

		for (; i < 4 && frames < hdr_frames; i++, frames++) {
			uint64_t addr = (&tp->arg1)[i];
			CSSymbolRef symbol = CSSymbolicatorGetSymbolWithAddressAtTime(
					symb, addr, kCSNow);

			expect_frame(bt, bt_len, symbol, addr, frames, hdr_frames);
		}

		/* saw the end of the user stack */
		if (hdr_frames == frames) {
			*stacks_seen += 1;
			if (!kern) {
				ktrace_end(s, 1);
			}
		}
	});

	T_SETUPEND;
}

#define TRIGGERING_DEBUGID (0xfeff0f00)

/*
 * These functions must return an int to avoid the function prologue being
 * hoisted out of the path to the spin (breaking being able to get a good
 * backtrace).
 */
static int __attribute__((noinline, not_tail_called))
recurse_a(dispatch_semaphore_t spinning, unsigned int frames);
static int __attribute__((noinline, not_tail_called))
recurse_b(dispatch_semaphore_t spinning, unsigned int frames);

static int __attribute__((noinline, not_tail_called))
recurse_a(dispatch_semaphore_t spinning, unsigned int frames)
{
	if (frames == 0) {
		if (spinning) {
			dispatch_semaphore_signal(spinning);
			for (;;) {
				;
			}
		} else {
			kdebug_trace(TRIGGERING_DEBUGID, 0, 0, 0, 0);
			return 0;
		}
	}

	return recurse_b(spinning, frames - 1) + 1;
}

static int __attribute__((noinline, not_tail_called))
recurse_b(dispatch_semaphore_t spinning, unsigned int frames)
{
	if (frames == 0) {
		if (spinning) {
			dispatch_semaphore_signal(spinning);
			for (;;) {
				;
			}
		} else {
			kdebug_trace(TRIGGERING_DEBUGID, 0, 0, 0, 0);
			return 0;
		}
	}

	return recurse_a(spinning, frames - 1) + 1;
}

#define USER_FRAMES (12)

#if defined(__x86_64__)

#define RECURSE_START_OFFSET (3)

#else /* defined(__x86_64__) */

#define RECURSE_START_OFFSET (2)

#endif /* !defined(__x86_64__) */

static const char *user_bt[USER_FRAMES] = {
#if defined(__x86_64__)
	/*
	 * x86_64 has an extra "thread_start" frame here.
	 */
	NULL,
#endif /* defined(__x86_64__) */
	NULL, NULL,
	"backtrace_thread",
	"recurse_a", "recurse_b", "recurse_a", "recurse_b",
	"recurse_a", "recurse_b", "recurse_a",
#if !defined(__x86_64__)
	/*
	 * Pick up the slack to make the number of frames constant.
	 */
	"recurse_b",
#endif /* !defined(__x86_64__) */
	NULL,
};

#if defined(__arm64__)

#define KERNEL_FRAMES (4)
static const char *kernel_bt[KERNEL_FRAMES] = {
	"fleh_synchronous", "sleh_synchronous", "unix_syscall", "kdebug_trace64",
};

#elif defined(__x86_64__)

#define KERNEL_FRAMES (2)
static const char *kernel_bt[KERNEL_FRAMES] = {
	"unix_syscall64", "kdebug_trace64"
};

#else
#error "architecture unsupported"
#endif /* defined(__arm64__) */

static dispatch_once_t backtrace_once;
static dispatch_semaphore_t backtrace_started;
static dispatch_semaphore_t backtrace_go;

/*
 * Another thread to run with a known backtrace.
 *
 * Take a semaphore that will be signalled when the thread is spinning at the
 * correct frame.  If the semaphore is NULL, don't spin and instead make a
 * kdebug_trace system call, which can trigger a deterministic backtrace itself.
 */
static void *
backtrace_thread(void *arg)
{
	dispatch_semaphore_t notify_spinning;
	unsigned int calls;

	notify_spinning = (dispatch_semaphore_t)arg;

	dispatch_semaphore_signal(backtrace_started);
	if (!notify_spinning) {
		dispatch_semaphore_wait(backtrace_go, DISPATCH_TIME_FOREVER);
	}

	/*
	 * _pthread_start, backtrace_thread, recurse_a, recurse_b,
	 * ...[, __kdebug_trace64]
	 *
	 * Always make two fewer calls for this frame (backtrace_thread and
	 * _pthread_start).
	 */
	calls = USER_FRAMES - RECURSE_START_OFFSET - 2;
	if (notify_spinning) {
		/*
		 * Spinning doesn't end up calling __kdebug_trace64.
		 */
		calls -= 1;
	}

	T_LOG("backtrace thread calling into %d frames (already at %d frames)",
	    calls, RECURSE_START_OFFSET);
	(void)recurse_a(notify_spinning, calls);
	return NULL;
}

static uint64_t
create_backtrace_thread(void *(*thread_fn)(void *),
    dispatch_semaphore_t notify_spinning)
{
	pthread_t thread = NULL;
	uint64_t tid;

	dispatch_once(&backtrace_once, ^{
		backtrace_started = dispatch_semaphore_create(0);
		T_QUIET; T_ASSERT_NOTNULL(backtrace_started, NULL);

		if (!notify_spinning) {
		        backtrace_go = dispatch_semaphore_create(0);
		        T_QUIET; T_ASSERT_NOTNULL(backtrace_go, NULL);
		}
	});

	T_QUIET; T_ASSERT_POSIX_ZERO(pthread_create(&thread, NULL, thread_fn,
	    (void *)notify_spinning), NULL);
	T_QUIET; T_ASSERT_NOTNULL(thread, "backtrace thread created");
	dispatch_semaphore_wait(backtrace_started, DISPATCH_TIME_FOREVER);

	T_QUIET; T_ASSERT_POSIX_ZERO(pthread_threadid_np(thread, &tid), NULL);
	T_QUIET; T_ASSERT_NE(tid, UINT64_C(0),
	    "backtrace thread created does not have ID 0");

	T_LOG("starting thread with ID 0x%" PRIx64, tid);

	return tid;
}

static void
start_backtrace_thread(void)
{
	T_QUIET; T_ASSERT_NOTNULL(backtrace_go,
	    "thread to backtrace created before starting it");
	dispatch_semaphore_signal(backtrace_go);
}

#if TARGET_OS_WATCH
#define TEST_TIMEOUT_NS (30 * NSEC_PER_SEC)
#else /* TARGET_OS_WATCH */
#define TEST_TIMEOUT_NS (5 * NSEC_PER_SEC)
#endif /* !TARGET_OS_WATCH */

T_DECL(kperf_stacks_kdebug_trig,
    "test that backtraces from kdebug trigger are correct",
    T_META_ASROOT(true))
{
	static unsigned int stacks_seen = 0;
	ktrace_session_t s;
	kperf_kdebug_filter_t filter;
	uint64_t tid;

	start_controlling_ktrace();

	s = ktrace_session_create();
	T_ASSERT_NOTNULL(s, "ktrace session was created");

	ktrace_set_collection_interval(s, 100);

	T_ASSERT_POSIX_ZERO(ktrace_filter_pid(s, getpid()), NULL);

	tid = create_backtrace_thread(backtrace_thread, NULL);
	expect_backtrace(s, tid, &stacks_seen, false, user_bt, USER_FRAMES, 0);
	expect_backtrace(s, tid, &stacks_seen, true, kernel_bt, KERNEL_FRAMES, 4);

	/*
	 * The triggering event must be traced (and thus registered with libktrace)
	 * to get backtraces.
	 */
	ktrace_events_single(s, TRIGGERING_DEBUGID,
	    ^(__unused struct trace_point *tp){ });

	ktrace_set_completion_handler(s, ^(void) {
		T_EXPECT_GE(stacks_seen, 2U, "saw both kernel and user stacks");
		ktrace_session_destroy(s);
		kperf_reset();
		T_END;
	});

	filter = kperf_kdebug_filter_create();
	T_ASSERT_NOTNULL(filter, "kperf kdebug filter was created");

	T_QUIET; T_ASSERT_POSIX_SUCCESS(kperf_kdebug_filter_add_debugid(filter,
	    TRIGGERING_DEBUGID), NULL);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(kperf_kdebug_filter_set(filter), NULL);
	(void)kperf_action_count_set(1);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(kperf_action_samplers_set(1,
	    KPERF_SAMPLER_USTACK | KPERF_SAMPLER_KSTACK), NULL);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(kperf_kdebug_action_set(1), NULL);
	kperf_kdebug_filter_destroy(filter);

	T_ASSERT_POSIX_SUCCESS(kperf_sample_set(1), NULL);

	T_ASSERT_POSIX_ZERO(ktrace_start(s, dispatch_get_main_queue()), NULL);

	start_backtrace_thread();

	dispatch_after(dispatch_time(DISPATCH_TIME_NOW, TEST_TIMEOUT_NS),
	    dispatch_get_main_queue(), ^(void)
	{
		T_LOG("ending test after timeout");
		ktrace_end(s, 0);
	});

	dispatch_main();
}

T_DECL(kperf_ustack_timer,
    "test that user backtraces on a timer are correct",
    T_META_ASROOT(true))
{
	static unsigned int stacks_seen = 0;
	ktrace_session_t s;
	uint64_t tid;
	dispatch_semaphore_t wait_for_spinning = dispatch_semaphore_create(0);

	start_controlling_ktrace();

	s = ktrace_session_create();
	T_QUIET; T_ASSERT_NOTNULL(s, "ktrace_session_create");

	ktrace_set_collection_interval(s, 100);

	ktrace_filter_pid(s, getpid());

	configure_kperf_stacks_timer(getpid(), 10, false);

	tid = create_backtrace_thread(backtrace_thread, wait_for_spinning);
	/* potentially calling dispatch function and system call */
	expect_backtrace(s, tid, &stacks_seen, false, user_bt, USER_FRAMES - 1, 2);

	ktrace_set_completion_handler(s, ^(void) {
		T_EXPECT_GE(stacks_seen, 1U, "saw at least one stack");
		ktrace_session_destroy(s);
		kperf_reset();
		T_END;
	});

	T_QUIET; T_ASSERT_POSIX_SUCCESS(kperf_sample_set(1), NULL);

	/* wait until the thread that will be backtraced is spinning */
	dispatch_semaphore_wait(wait_for_spinning, DISPATCH_TIME_FOREVER);

	T_ASSERT_POSIX_ZERO(ktrace_start(s, dispatch_get_main_queue()), NULL);

	dispatch_after(dispatch_time(DISPATCH_TIME_NOW, TEST_TIMEOUT_NS),
	    dispatch_get_main_queue(), ^(void)
	{
		T_LOG("ending test after timeout");
		ktrace_end(s, 0);
	});

	dispatch_main();
}

static volatile bool spin = true;

__attribute__((noinline, not_tail_called))
static void
recurse_spin(dispatch_semaphore_t notify_sema, int depth)
{
	if (depth > 0) {
		recurse_spin(notify_sema, depth - 1);
	} else {
		dispatch_semaphore_signal(notify_sema);
		while (spin);
	}
}

static void *
spin_thread(void *arg)
{
	dispatch_semaphore_t notify_sema = arg;
	dispatch_semaphore_signal(backtrace_started);
	recurse_spin(notify_sema, 257);
	return NULL;
}

T_DECL(kperf_ustack_trunc, "ensure stacks are marked as truncated")
{
	start_controlling_ktrace();

	ktrace_session_t s = ktrace_session_create();
	T_ASSERT_NOTNULL(s, "ktrace session was created");

	ktrace_set_collection_interval(s, 100);

	T_QUIET;
	T_ASSERT_POSIX_ZERO(ktrace_filter_pid(s, getpid()), NULL);

	configure_kperf_stacks_timer(getpid(), 10, false);

	__block bool saw_stack = false;
	ktrace_set_completion_handler(s, ^{
	    T_EXPECT_TRUE(saw_stack, "saw the user stack");
	    T_END;
	});

	dispatch_semaphore_t notify_sema = dispatch_semaphore_create(0);
	uint64_t tid = create_backtrace_thread(spin_thread, notify_sema);

	ktrace_events_single(s, PERF_STK_UHDR, ^(struct trace_point *tp) {
		if (tp->threadid != tid) {
			return;
		}
		T_LOG("found %llu frame stack", tp->arg2);
		T_EXPECT_BITS_SET(tp->arg1, CALLSTACK_VALID,
		    "found valid callstack");
		T_EXPECT_BITS_SET(tp->arg1, CALLSTACK_TRUNCATED,
		    "found truncated callstack");
		saw_stack = true;
		ktrace_end(s, 1);
	});

	T_QUIET; T_ASSERT_POSIX_SUCCESS(kperf_sample_set(1), NULL);

	T_ASSERT_POSIX_ZERO(ktrace_start(s, dispatch_get_main_queue()),
	    "start tracing");

	dispatch_after(dispatch_time(DISPATCH_TIME_NOW, TEST_TIMEOUT_NS),
	    dispatch_get_main_queue(), ^(void)
	{
		T_LOG("ending test after timeout");
		ktrace_end(s, 0);
	});

	dispatch_main();
}

T_DECL(kperf_ustack_maxlen, "ensure stacks up to 256 frames can be captured")
{
	start_controlling_ktrace();

	ktrace_session_t s = ktrace_session_create();
	T_QUIET; T_ASSERT_NOTNULL(s, "ktrace session was created");

	ktrace_set_collection_interval(s, 100);

	T_QUIET;
	T_ASSERT_POSIX_ZERO(ktrace_filter_pid(s, getpid()), NULL);

	configure_kperf_stacks_timer(getpid(), 10, false);

	__block bool saw_stack = false;
	__block bool saw_stack_data = false;
	__block uint64_t nevents = 0;
	ktrace_set_completion_handler(s, ^{
	    T_EXPECT_TRUE(saw_stack, "saw the user stack");
	    T_LOG("saw %" PRIu64 " stack data events", nevents);
	    T_EXPECT_TRUE(saw_stack_data, "saw all frames of the user stack");
	    T_END;
	});

	dispatch_semaphore_t notify_sema = dispatch_semaphore_create(0);
	uint64_t tid = create_backtrace_thread(spin_thread, notify_sema);

	ktrace_events_single(s, PERF_STK_UHDR, ^(struct trace_point *tp) {
		if (tp->threadid != tid) {
			return;
		}
		T_LOG("found %llu frame stack", tp->arg2);
		T_EXPECT_BITS_SET(tp->arg1, CALLSTACK_VALID,
		    "found valid callstack");
		T_EXPECT_EQ(tp->arg2, UINT64_C(256),
		    "found the correct number of frames");
		saw_stack = true;
	});

	ktrace_events_single(s, PERF_STK_UDATA, ^(struct trace_point *tp) {
		if (tp->threadid != tid && !saw_stack) {
			return;
		}
		nevents++;
		if (nevents == 256 / 4) {
			ktrace_end(s, 1);
		}
		saw_stack_data = true;
	});

	T_QUIET; T_ASSERT_POSIX_SUCCESS(kperf_sample_set(1), NULL);

	T_ASSERT_POSIX_ZERO(ktrace_start(s, dispatch_get_main_queue()),
	    "start tracing");

	dispatch_after(dispatch_time(DISPATCH_TIME_NOW, TEST_TIMEOUT_NS),
	    dispatch_get_main_queue(), ^(void)
	{
		T_LOG("ending test after timeout");
		ktrace_end(s, 0);
	});

	dispatch_main();
}

T_DECL(kperf_ustack_mostly_valid,
		"ensure most stacks captured by kperf are valid")
{
	start_controlling_ktrace();

	ktrace_session_t s = ktrace_session_create();
	T_QUIET; T_ASSERT_NOTNULL(s, "ktrace session was created");

	ktrace_set_collection_interval(s, 100);
	configure_kperf_stacks_timer(-1, 10, false);
	int set = 1;
	T_ASSERT_POSIX_SUCCESS(sysctlbyname("kperf.lightweight_pet", NULL, NULL,
			&set, sizeof(set)), NULL);

	__block uint64_t stacks_seen = 0;
	__block uint64_t valid_stacks_seen = 0;

	ktrace_set_completion_handler(s, ^{
		double valid_density = (double)valid_stacks_seen / (double)stacks_seen;
	    T_LOG("saw %" PRIu64 " stack header events, %" PRIu64 " valid (%g%%)",
				stacks_seen, valid_stacks_seen, valid_density * 100.);
		T_EXPECT_GT(valid_density, 0.98, "more than 98%% of stacks are valid");
	    T_END;
	});

	dispatch_semaphore_t notify_sema = dispatch_semaphore_create(0);
	(void)create_backtrace_thread(spin_thread, notify_sema);

	ktrace_events_single(s, PERF_STK_UHDR, ^(struct trace_point *tp) {
		stacks_seen += 1;
		if (tp->arg1 & CALLSTACK_VALID && tp->arg2 > 0) {
			valid_stacks_seen += 1;
		}
	});

	T_QUIET; T_ASSERT_POSIX_SUCCESS(kperf_sample_set(1), NULL);

	T_ASSERT_POSIX_ZERO(ktrace_start(s, dispatch_get_main_queue()),
	    "start tracing");

	dispatch_after(dispatch_time(DISPATCH_TIME_NOW, TEST_TIMEOUT_NS),
	    dispatch_get_main_queue(), ^(void)
	{
		T_LOG("ending test after timeout");
		ktrace_end(s, 0);
	});

	dispatch_main();
}

/* TODO test kernel stacks in all modes */
/* TODO legacy PET mode backtracing */
