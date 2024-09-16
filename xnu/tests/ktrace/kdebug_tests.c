// Copyright (c) 2020-2022 Apple Inc.  All rights reserved.

#include <darwintest.h>
#include <darwintest_utils.h>
#include <dispatch/dispatch.h>
#include <inttypes.h>
#include <ktrace/session.h>
#include <ktrace/private.h>
#include <kperf/kperf.h>
#include <mach/clock_types.h>
#include <mach/dyld_kernel.h>
#include <mach/host_info.h>
#include <mach/mach.h>
#include <mach/mach_init.h>
#include <mach/task.h>
#include <os/assumes.h>
#include <stdlib.h>
#include <sys/kdebug.h>
#include <sys/kdebug_signpost.h>
#include <sys/resource_private.h>
#include <sys/sysctl.h>
#include <stdint.h>
#include <TargetConditionals.h>

#include "ktrace_helpers.h"
#include "test_utils.h"
#include "ktrace_meta.h"

#define KDBG_TEST_MACROS         1
#define KDBG_TEST_OLD_TIMES      2
#define KDBG_TEST_FUTURE_TIMES   3
#define KDBG_TEST_IOP_SYNC_FLUSH 4

#pragma mark kdebug syscalls

#define TRACE_DEBUGID (0xfedfed00U)

T_DECL(kdebug_trace_syscall, "test that kdebug_trace(2) emits correct events",
		T_META_TAG_VM_PREFERRED)
{
	start_controlling_ktrace();

	ktrace_session_t s = ktrace_session_create();
	T_QUIET; T_WITH_ERRNO; T_ASSERT_NOTNULL(s, "created session");
	ktrace_set_collection_interval(s, 200);

	__block int events_seen = 0;
	ktrace_events_single(s, TRACE_DEBUGID, ^void (struct trace_point *tp) {
		events_seen++;
		T_PASS("saw traced event");

		if (ktrace_is_kernel_64_bit(s)) {
			T_EXPECT_EQ(tp->arg1, UINT64_C(0xfeedfacefeedface),
					"argument 1 of traced event is correct");
		} else {
			T_EXPECT_EQ(tp->arg1, UINT64_C(0xfeedface),
					"argument 1 of traced event is correct");
		}
		T_EXPECT_EQ(tp->arg2, 2ULL, "argument 2 of traced event is correct");
		T_EXPECT_EQ(tp->arg3, 3ULL, "argument 3 of traced event is correct");
		T_EXPECT_EQ(tp->arg4, 4ULL, "argument 4 of traced event is correct");

		ktrace_end(s, 1);
	});

	ktrace_set_completion_handler(s, ^{
		T_EXPECT_GE(events_seen, 1, NULL);
		ktrace_session_destroy(s);
		T_END;
	});

	ktrace_filter_pid(s, getpid());

	T_ASSERT_POSIX_ZERO(ktrace_start(s, dispatch_get_main_queue()), NULL);
	T_ASSERT_POSIX_SUCCESS(kdebug_trace(TRACE_DEBUGID, 0xfeedfacefeedface, 2,
			3, 4), NULL);
	ktrace_end(s, 0);

	dispatch_main();
}

#if __LP64__
#define IS_64BIT true
#else // __LP64__
#define IS_64BIT false
#endif // !__LP64__

#define STRING_SIZE (1024)

T_DECL(kdebug_trace_string_syscall,
		"test that kdebug_trace_string(2) emits correct events",
		T_META_ENABLED(IS_64BIT),
		T_META_TAG_VM_PREFERRED)
{
	start_controlling_ktrace();

	ktrace_session_t s = ktrace_session_create();
	T_QUIET; T_WITH_ERRNO; T_ASSERT_NOTNULL(s, "created session");
	ktrace_set_collection_interval(s, 200);
	ktrace_filter_pid(s, getpid());

	char *traced_string = calloc(1, STRING_SIZE);
	T_QUIET; T_WITH_ERRNO;
	T_ASSERT_NOTNULL(traced_string, "allocated memory for string");
	for (size_t i = 0; i < sizeof(traced_string); i++) {
		traced_string[i] = 'a' + (i % 26);
	}
	traced_string[sizeof(traced_string) - 1] = '\0';
	size_t traced_len = strlen(traced_string);
	T_QUIET; T_ASSERT_EQ(traced_len, sizeof(traced_string) - 1,
			"traced string should be filled");

	ktrace_events_single(s, TRACE_DEBUGID,
			^void (struct trace_point * __unused tp) {
		// Do nothing -- just ensure the event is filtered in.
	});

	__block unsigned int string_cpu = 0;
	__block bool tracing_string = false;
	char *observed_string = calloc(1, PATH_MAX);
	T_QUIET; T_WITH_ERRNO;
	T_ASSERT_NOTNULL(observed_string, "allocated memory for observed string");
	__block size_t string_offset = 0;
	ktrace_events_single(s, TRACE_STRING_GLOBAL, ^(struct trace_point *tp){
		if (tp->debugid & DBG_FUNC_START && tp->arg1 == TRACE_DEBUGID) {
			tracing_string = true;
			string_cpu = tp->cpuid;
			memcpy(observed_string + string_offset, &tp->arg3,
					sizeof(uint64_t) * 2);
			string_offset += sizeof(uint64_t) * 2;
		} else if (tracing_string && string_cpu == tp->cpuid) {
			memcpy(observed_string + string_offset, &tp->arg1,
					sizeof(uint64_t) * 4);
			string_offset += sizeof(uint64_t) * 4;
			if (tp->debugid & DBG_FUNC_END) {
				ktrace_end(s, 1);
			}
		}
	});

	ktrace_set_completion_handler(s, ^{
		T_EXPECT_TRUE(tracing_string, "found string in trace");
		size_t observed_len = strlen(observed_string);
		T_EXPECT_EQ(traced_len, observed_len, "string lengths should be equal");
		if (traced_len == observed_len) {
			T_EXPECT_EQ_STR(traced_string, observed_string,
					"observed correct string");
		}
		ktrace_session_destroy(s);
		T_END;
	});

	T_ASSERT_POSIX_ZERO(ktrace_start(s, dispatch_get_main_queue()), NULL);
	uint64_t str_id = kdebug_trace_string(TRACE_DEBUGID, 0, traced_string);
	T_WITH_ERRNO; T_ASSERT_NE(str_id, (uint64_t)0, "kdebug_trace_string(2)");
	ktrace_end(s, 0);

	dispatch_main();
}

#define SIGNPOST_SINGLE_CODE (0x10U)
#define SIGNPOST_PAIRED_CODE (0x20U)

T_DECL(kdebug_signpost_syscall,
		"test that kdebug_signpost(2) emits correct events",
		T_META_TAG_VM_PREFERRED)
{
	start_controlling_ktrace();

	ktrace_session_t s = ktrace_session_create();
	T_QUIET; T_WITH_ERRNO; T_ASSERT_NOTNULL(s, "created session");

	__block int single_seen = 0;
	__block int paired_seen = 0;

	/* make sure to get enough events for the KDBUFWAIT to trigger */
	// ktrace_events_class(s, DBG_MACH, ^(__unused struct trace_point *tp){});
	ktrace_events_single(s,
	    APPSDBG_CODE(DBG_APP_SIGNPOST, SIGNPOST_SINGLE_CODE),
	    ^(struct trace_point *tp) {
		single_seen++;
		T_PASS("single signpost is traced");

		T_EXPECT_EQ(tp->arg1, 1ULL, "argument 1 of single signpost is correct");
		T_EXPECT_EQ(tp->arg2, 2ULL, "argument 2 of single signpost is correct");
		T_EXPECT_EQ(tp->arg3, 3ULL, "argument 3 of single signpost is correct");
		T_EXPECT_EQ(tp->arg4, 4ULL, "argument 4 of single signpost is correct");
	});

	ktrace_events_single_paired(s,
	    APPSDBG_CODE(DBG_APP_SIGNPOST, SIGNPOST_PAIRED_CODE),
	    ^(struct trace_point *start, struct trace_point *end) {
		paired_seen++;
		T_PASS("paired signposts are traced");

		T_EXPECT_EQ(start->arg1, 5ULL, "argument 1 of start signpost is correct");
		T_EXPECT_EQ(start->arg2, 6ULL, "argument 2 of start signpost is correct");
		T_EXPECT_EQ(start->arg3, 7ULL, "argument 3 of start signpost is correct");
		T_EXPECT_EQ(start->arg4, 8ULL, "argument 4 of start signpost is correct");

		T_EXPECT_EQ(end->arg1, 9ULL, "argument 1 of end signpost is correct");
		T_EXPECT_EQ(end->arg2, 10ULL, "argument 2 of end signpost is correct");
		T_EXPECT_EQ(end->arg3, 11ULL, "argument 3 of end signpost is correct");
		T_EXPECT_EQ(end->arg4, 12ULL, "argument 4 of end signpost is correct");

		T_EXPECT_EQ(single_seen, 1, "signposts are traced in the correct order");

		ktrace_end(s, 1);
	});

	ktrace_set_completion_handler(s, ^(void) {
		T_QUIET; T_EXPECT_NE(single_seen, 0,
		"did not see single tracepoint before timeout");
		T_QUIET; T_EXPECT_NE(paired_seen, 0,
		"did not see single tracepoint before timeout");
		ktrace_session_destroy(s);
		T_END;
	});

	ktrace_filter_pid(s, getpid());

	T_ASSERT_POSIX_ZERO(ktrace_start(s, dispatch_get_main_queue()),
	    "started tracing");

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
	T_EXPECT_POSIX_SUCCESS(kdebug_signpost(SIGNPOST_SINGLE_CODE, 1, 2, 3, 4),
	    "emitted single signpost");
	T_EXPECT_POSIX_SUCCESS(
		kdebug_signpost_start(SIGNPOST_PAIRED_CODE, 5, 6, 7, 8),
		"emitted start signpost");
	T_EXPECT_POSIX_SUCCESS(
		kdebug_signpost_end(SIGNPOST_PAIRED_CODE, 9, 10, 11, 12),
		"emitted end signpost");
#pragma clang diagnostic pop
	ktrace_end(s, 0);

	dispatch_main();
}

T_DECL(syscall_tracing,
		"ensure that syscall arguments are traced propertly",
		T_META_TAG_VM_PREFERRED)
{
	ktrace_session_t s = ktrace_session_create();
	T_QUIET; T_WITH_ERRNO; T_ASSERT_NOTNULL(s, "created session");

	__block bool seen = 0;

	ktrace_filter_pid(s, getpid());

	static const int telemetry_syscall_no = 451;
	static const uint64_t arg1 = 0xfeedfacefeedface;

	ktrace_events_single(s, BSDDBG_CODE(DBG_BSD_EXCP_SC, telemetry_syscall_no),
			^(struct trace_point *evt){
		if (KDBG_EXTRACT_CODE(evt->debugid) != telemetry_syscall_no || seen) {
			return;
		}

		seen = true;
		if (ktrace_is_kernel_64_bit(s)) {
			T_EXPECT_EQ(evt->arg1, arg1,
					"argument 1 of syscall event is correct");
		} else {
			T_EXPECT_EQ(evt->arg1, (uint64_t)(uint32_t)(arg1),
					"argument 1 of syscall event is correct");
		}

		ktrace_end(s, 1);
	});

	ktrace_set_completion_handler(s, ^{
		T_ASSERT_TRUE(seen,
				"should have seen a syscall event for kevent_id(2)");
		ktrace_session_destroy(s);
		T_END;
	});

	int error = ktrace_start(s, dispatch_get_main_queue());
	T_ASSERT_POSIX_ZERO(error, "started tracing");

	/*
	 * telemetry(2) has a 64-bit argument that will definitely be traced, and
	 * is unlikely to be used elsewhere by this process.
	 */
	extern int __telemetry(uint64_t cmd, uint64_t deadline, uint64_t interval,
			uint64_t leeway, uint64_t arg4, uint64_t arg5);
	(void)__telemetry(arg1, 0, 0, 0, 0, 0);

	dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC),
			dispatch_get_main_queue(), ^{
		T_LOG("ending test due to timeout");
		ktrace_end(s, 0);
	});

	dispatch_main();
}

#pragma mark kdebug behaviors

#define WRAPPING_EVENTS_COUNT     (150000)
#define TRACE_ITERATIONS          (5000)
#define WRAPPING_EVENTS_THRESHOLD (100)

T_DECL(wrapping,
    "ensure that wrapping traces lost events and no events prior to the wrap",
    T_META_CHECK_LEAKS(false),
	T_META_TAG_VM_PREFERRED)
{
	kbufinfo_t buf_info;
	int wait_wrapping_secs = (WRAPPING_EVENTS_COUNT / TRACE_ITERATIONS) + 5;
	int current_secs = wait_wrapping_secs;

	start_controlling_ktrace();

	/* use sysctls manually to bypass libktrace assumptions */

	int mib[4] = { CTL_KERN, KERN_KDEBUG };
	mib[2] = KERN_KDSETBUF; mib[3] = WRAPPING_EVENTS_COUNT;
	T_ASSERT_POSIX_SUCCESS(sysctl(mib, 4, NULL, 0, NULL, 0), "KERN_KDSETBUF");

	mib[2] = KERN_KDSETUP; mib[3] = 0;
	size_t needed = 0;
	T_ASSERT_POSIX_SUCCESS(sysctl(mib, 3, NULL, &needed, NULL, 0),
	    "KERN_KDSETUP");

	mib[2] = KERN_KDENABLE; mib[3] = 1;
	T_ASSERT_POSIX_SUCCESS(sysctl(mib, 4, NULL, 0, NULL, 0), "KERN_KDENABLE");

	/* wrapping is on by default */

	/* wait until wrapped */
	T_LOG("waiting for trace to wrap");
	mib[2] = KERN_KDGETBUF;
	needed = sizeof(buf_info);
	do {
		sleep(1);
		for (int i = 0; i < TRACE_ITERATIONS; i++) {
			T_QUIET;
			T_ASSERT_POSIX_SUCCESS(kdebug_trace(0xfefe0000, 0, 0, 0, 0), NULL);
		}
		T_QUIET;
		T_ASSERT_POSIX_SUCCESS(sysctl(mib, 3, &buf_info, &needed, NULL, 0),
		    NULL);
	} while (!(buf_info.flags & KDBG_WRAPPED) && --current_secs > 0);

	T_ASSERT_TRUE(buf_info.flags & KDBG_WRAPPED,
	    "trace wrapped (after %d seconds within %d second timeout)",
	    wait_wrapping_secs - current_secs, wait_wrapping_secs);

	ktrace_session_t s = ktrace_session_create();
	T_QUIET; T_ASSERT_NOTNULL(s, NULL);
	T_QUIET; T_ASSERT_POSIX_ZERO(ktrace_set_use_existing(s), NULL);

	__block int events = 0;

	ktrace_events_all(s, ^(struct trace_point *tp) {
		if (events == 0) {
		        T_EXPECT_EQ(tp->debugid, (unsigned int)TRACE_LOST_EVENTS,
		        "first event's debugid 0x%08x (%s) should be TRACE_LOST_EVENTS",
		        tp->debugid,
		        ktrace_name_for_eventid(s, tp->debugid & KDBG_EVENTID_MASK));
		} else {
		        T_QUIET;
		        T_EXPECT_NE(tp->debugid, (unsigned int)TRACE_LOST_EVENTS,
		        "event debugid 0x%08x (%s) should not be TRACE_LOST_EVENTS",
		        tp->debugid,
		        ktrace_name_for_eventid(s, tp->debugid & KDBG_EVENTID_MASK));
		}

		events++;
		if (events > WRAPPING_EVENTS_THRESHOLD) {
		        ktrace_end(s, 1);
		}
	});

	ktrace_set_completion_handler(s, ^{
		ktrace_session_destroy(s);
		T_END;
	});

	T_ASSERT_POSIX_ZERO(ktrace_start(s, dispatch_get_main_queue()),
	    "started tracing");

	dispatch_main();
}

static void
_assert_tracing_state(bool enable, const char *msg)
{
	kbufinfo_t bufinfo = { 0 };
	T_QUIET;
	T_ASSERT_POSIX_SUCCESS(sysctl(
		    (int[]){ CTL_KERN, KERN_KDEBUG, KERN_KDGETBUF }, 3,
		    &bufinfo, &(size_t){ sizeof(bufinfo) }, NULL, 0),
	    "get kdebug buffer info");
	T_QUIET;
	T_ASSERT_NE(bufinfo.nkdbufs, 0, "tracing should be configured");
	T_ASSERT_NE(bufinfo.nolog, enable, "%s: tracing should%s be enabled",
			msg, enable ? "" : "n't");
}

#define DRAIN_TIMEOUT_NS (1 * NSEC_PER_SEC)

static void
_drain_until_event(uint32_t debugid)
{
	static kd_buf events[256] = { 0 };
	size_t events_size = sizeof(events);
	uint64_t start_time_ns = clock_gettime_nsec_np(CLOCK_MONOTONIC);
	unsigned int reads = 0;
	while (true) {
		T_QUIET;
		T_ASSERT_POSIX_SUCCESS(sysctl(
				(int[]){ CTL_KERN, KERN_KDEBUG, KERN_KDREADTR, }, 3,
				events, &events_size, NULL, 0), "reading trace data");
		reads += 1;
		size_t events_count = events_size;
		for (size_t i = 0; i < events_count; i++) {
			if (events[i].debugid == debugid) {
				T_LOG("draining found event 0x%x", debugid);
				return;
			}
		}
		uint64_t cur_time_ns = clock_gettime_nsec_np(CLOCK_MONOTONIC);
		if (cur_time_ns - start_time_ns > DRAIN_TIMEOUT_NS) {
			T_ASSERT_FAIL("timed out after %f seconds waiting for 0x%x,"
					" after %u reads",
					(double)(cur_time_ns - start_time_ns) / 1e9, debugid,
					reads);
		}
	}
}

T_DECL(disabling_event_match,
    "ensure that ktrace is disabled when an event disable matcher fires",
	T_META_TAG_VM_PREFERRED)
{
	start_controlling_ktrace();

	T_SETUPBEGIN;
	ktrace_session_t s = ktrace_session_create();
	T_QUIET; T_WITH_ERRNO; T_ASSERT_NOTNULL(s, "created session");
	ktrace_events_single(s, TRACE_DEBUGID,
			^(struct trace_point *tp __unused) {});
	int error = ktrace_configure(s);
	T_QUIET; T_ASSERT_POSIX_ZERO(error, "configured session");
	kd_event_matcher matchers[2] = { {
		.kem_debugid = TRACE_DEBUGID,
		.kem_args[0] = 0xff,
	}, {
		.kem_debugid = UINT32_MAX,
		.kem_args[0] = 0xfff,
	} };
	size_t matchers_size = sizeof(matchers);
	T_ASSERT_POSIX_SUCCESS(sysctl(
		(int[]){ CTL_KERN, KERN_KDEBUG, KERN_KDSET_EDM, }, 3,
		&matchers, &matchers_size, NULL, 0), "set event disable matcher");
	size_t size = 0;
	T_ASSERT_POSIX_SUCCESS(sysctl(
		(int[]){ CTL_KERN, KERN_KDEBUG, KERN_KDEFLAGS, KDBG_MATCH_DISABLE, }, 4,
		NULL, &size, NULL, 0), "enabled event disable matching");
	T_ASSERT_POSIX_SUCCESS(sysctl(
		(int[]){ CTL_KERN, KERN_KDEBUG, KERN_KDENABLE, 1, }, 4,
		NULL, NULL, NULL, 0), "enabled tracing");
	_assert_tracing_state(true, "after enabling trace");
	T_SETUPEND;

	kdebug_trace(TRACE_DEBUGID + 8, 0xff, 0, 0, 0);
	_drain_until_event(TRACE_DEBUGID + 8);
	_assert_tracing_state(true, "with wrong debugid");
	kdebug_trace(TRACE_DEBUGID, 0, 0, 0, 0);
	_drain_until_event(TRACE_DEBUGID);
	_assert_tracing_state(true, "with wrong argument");
	kdebug_trace(TRACE_DEBUGID, 0xff, 0, 0, 0);
	_drain_until_event(TRACE_DEBUGID);
	_assert_tracing_state(false, "after disabling event");
}

T_DECL(reject_old_events,
    "ensure that kdebug rejects events from before tracing began",
    T_META_CHECK_LEAKS(false),
	T_META_TAG_VM_PREFERRED)
{
	__block uint64_t event_horizon_ts;

	start_controlling_ktrace();

	ktrace_session_t s = ktrace_session_create();
	T_QUIET; T_WITH_ERRNO; T_ASSERT_NOTNULL(s, "created session");
	ktrace_set_collection_interval(s, 100);

	__block int events = 0;
	ktrace_events_single(s, KDBG_EVENTID(DBG_BSD, DBG_BSD_KDEBUG_TEST, 1),
	    ^(struct trace_point *tp) {
		events++;
		T_EXPECT_GT(tp->timestamp, event_horizon_ts,
		"events in trace should be from after tracing began");
	});

	ktrace_set_completion_handler(s, ^{
		T_EXPECT_EQ(events, 2, "should see only two events");
		ktrace_session_destroy(s);
		T_END;
	});

	event_horizon_ts = mach_absolute_time();

	T_ASSERT_POSIX_ZERO(ktrace_start(s, dispatch_get_main_queue()), NULL);
	/* first, try an old event at the beginning of trace */
	assert_kdebug_test(KDBG_TEST_OLD_TIMES, "induce old event at beginning");
	/* after a good event has been traced, old events should be rejected */
	assert_kdebug_test(KDBG_TEST_OLD_TIMES, "induce old event to be rejected");
	ktrace_end(s, 0);

	dispatch_main();
}

#define ORDERING_TIMEOUT_SEC 5

T_DECL(ascending_time_order,
    "ensure that kdebug events are in ascending order based on time",
    T_META_CHECK_LEAKS(false), XNU_T_META_SOC_SPECIFIC, T_META_TAG_VM_NOT_ELIGIBLE)
{
	__block uint64_t prev_ts = 0;
	__block uint32_t prev_debugid = 0;
	__block unsigned int prev_cpu = 0;
	__block bool in_order = true;

	start_controlling_ktrace();

	ktrace_session_t s = ktrace_session_create();
	T_QUIET; T_WITH_ERRNO; T_ASSERT_NOTNULL(s, "created session");

	ktrace_events_all(s, ^(struct trace_point *tp) {
		if (tp->timestamp < prev_ts) {
		        in_order = false;
		        T_LOG("%" PRIu64 ": %#" PRIx32 " (cpu %d)",
		        prev_ts, prev_debugid, prev_cpu);
		        T_LOG("%" PRIu64 ": %#" PRIx32 " (cpu %d)",
		        tp->timestamp, tp->debugid, tp->cpuid);
		        ktrace_end(s, 1);
		}
	});

	ktrace_set_completion_handler(s, ^{
		ktrace_session_destroy(s);
		T_EXPECT_TRUE(in_order, "event timestamps were in-order");
		T_END;
	});

	T_ASSERT_POSIX_ZERO(ktrace_start(s, dispatch_get_main_queue()),
	    "started tracing");

	/* try to inject old timestamps into trace */
	assert_kdebug_test(KDBG_TEST_OLD_TIMES, "inject old time");

	dispatch_after(dispatch_time(DISPATCH_TIME_NOW, ORDERING_TIMEOUT_SEC * NSEC_PER_SEC),
	    dispatch_get_main_queue(), ^{
		T_LOG("ending test after timeout");
		ktrace_end(s, 1);
	});

	dispatch_main();
}

#pragma mark dyld tracing

__attribute__((aligned(8)))
static const char map_uuid[16] = "map UUID";

__attribute__((aligned(8)))
static const char unmap_uuid[16] = "unmap UUID";

__attribute__((aligned(8)))
static const char sc_uuid[16] = "shared UUID";

static fsid_t map_fsid = { .val = { 42, 43 } };
static fsid_t unmap_fsid = { .val = { 44, 45 } };
static fsid_t sc_fsid = { .val = { 46, 47 } };

static fsobj_id_t map_fsobjid = { .fid_objno = 42, .fid_generation = 43 };
static fsobj_id_t unmap_fsobjid = { .fid_objno = 44, .fid_generation = 45 };
static fsobj_id_t sc_fsobjid = { .fid_objno = 46, .fid_generation = 47 };

#define MAP_LOAD_ADDR   0xabadcafe
#define UNMAP_LOAD_ADDR 0xfeedface
#define SC_LOAD_ADDR    0xfedfaced

__unused
static void
expect_dyld_image_info(struct trace_point *tp, const uint64_t *exp_uuid,
    uint64_t exp_load_addr, fsid_t *exp_fsid, fsobj_id_t *exp_fsobjid,
    int order)
{
#if defined(__LP64__) || defined(__arm64__)
	if (order == 0) {
		uint64_t uuid[2];
		uint64_t load_addr;
		fsid_t fsid;

		uuid[0] = (uint64_t)tp->arg1;
		uuid[1] = (uint64_t)tp->arg2;
		load_addr = (uint64_t)tp->arg3;
		fsid.val[0] = (int32_t)(tp->arg4 & UINT32_MAX);
		fsid.val[1] = (int32_t)((uint64_t)tp->arg4 >> 32);

		T_QUIET; T_EXPECT_EQ(uuid[0], exp_uuid[0], NULL);
		T_QUIET; T_EXPECT_EQ(uuid[1], exp_uuid[1], NULL);
		T_QUIET; T_EXPECT_EQ(load_addr, exp_load_addr, NULL);
		T_QUIET; T_EXPECT_EQ(fsid.val[0], exp_fsid->val[0], NULL);
		T_QUIET; T_EXPECT_EQ(fsid.val[1], exp_fsid->val[1], NULL);
	} else if (order == 1) {
		fsobj_id_t fsobjid;

		fsobjid.fid_objno = (uint32_t)(tp->arg1 & UINT32_MAX);
		fsobjid.fid_generation = (uint32_t)((uint64_t)tp->arg1 >> 32);

		T_QUIET; T_EXPECT_EQ(fsobjid.fid_objno, exp_fsobjid->fid_objno, NULL);
		T_QUIET; T_EXPECT_EQ(fsobjid.fid_generation,
		    exp_fsobjid->fid_generation, NULL);
	} else {
		T_ASSERT_FAIL("unrecognized order of events %d", order);
	}
#else /* defined(__LP64__) */
	if (order == 0) {
		uint32_t uuid[4];

		uuid[0] = (uint32_t)tp->arg1;
		uuid[1] = (uint32_t)tp->arg2;
		uuid[2] = (uint32_t)tp->arg3;
		uuid[3] = (uint32_t)tp->arg4;

		T_QUIET; T_EXPECT_EQ(uuid[0], (uint32_t)exp_uuid[0], NULL);
		T_QUIET; T_EXPECT_EQ(uuid[1], (uint32_t)(exp_uuid[0] >> 32), NULL);
		T_QUIET; T_EXPECT_EQ(uuid[2], (uint32_t)exp_uuid[1], NULL);
		T_QUIET; T_EXPECT_EQ(uuid[3], (uint32_t)(exp_uuid[1] >> 32), NULL);
	} else if (order == 1) {
		uint32_t load_addr;
		fsid_t fsid;
		fsobj_id_t fsobjid;

		load_addr = (uint32_t)tp->arg1;
		fsid.val[0] = (int32_t)tp->arg2;
		fsid.val[1] = (int32_t)tp->arg3;
		fsobjid.fid_objno = (uint32_t)tp->arg4;

		T_QUIET; T_EXPECT_EQ(load_addr, (uint32_t)exp_load_addr, NULL);
		T_QUIET; T_EXPECT_EQ(fsid.val[0], exp_fsid->val[0], NULL);
		T_QUIET; T_EXPECT_EQ(fsid.val[1], exp_fsid->val[1], NULL);
		T_QUIET; T_EXPECT_EQ(fsobjid.fid_objno, exp_fsobjid->fid_objno, NULL);
	} else if (order == 2) {
		fsobj_id_t fsobjid;

		fsobjid.fid_generation = tp->arg1;

		T_QUIET; T_EXPECT_EQ(fsobjid.fid_generation,
		    exp_fsobjid->fid_generation, NULL);
	} else {
		T_ASSERT_FAIL("unrecognized order of events %d", order);
	}
#endif /* defined(__LP64__) */
}

#if defined(__LP64__) || defined(__arm64__)
#define DYLD_CODE_OFFSET (0)
#define DYLD_EVENTS      (2)
#else
#define DYLD_CODE_OFFSET (2)
#define DYLD_EVENTS      (3)
#endif

static void
expect_dyld_events(ktrace_session_t s, const char *name, uint32_t base_code,
    const char *exp_uuid, uint64_t exp_load_addr, fsid_t *exp_fsid,
    fsobj_id_t *exp_fsobjid, uint8_t *saw_events)
{
	for (int i = 0; i < DYLD_EVENTS; i++) {
		ktrace_events_single(s, KDBG_EVENTID(DBG_DYLD, DBG_DYLD_UUID,
		    base_code + DYLD_CODE_OFFSET + (unsigned int)i),
		    ^(struct trace_point *tp) {
			T_LOG("checking %s event %c", name, 'A' + i);
			expect_dyld_image_info(tp, (const void *)exp_uuid, exp_load_addr,
			exp_fsid, exp_fsobjid, i);
			*saw_events |= (1U << i);
		});
	}
}

T_DECL(dyld_events, "test that dyld registering libraries emits events",
		T_META_TAG_VM_PREFERRED)
{
	dyld_kernel_image_info_t info;

	/*
	 * Use pointers instead of __block variables in order to use these variables
	 * in the completion block below _and_ pass pointers to them to the
	 * expect_dyld_events function.
	 */
	uint8_t saw_events[3] = { 0 };
	uint8_t *saw_mapping = &(saw_events[0]);
	uint8_t *saw_unmapping = &(saw_events[1]);
	uint8_t *saw_shared_cache = &(saw_events[2]);

	start_controlling_ktrace();

	ktrace_session_t s = ktrace_session_create();
	T_QUIET; T_WITH_ERRNO; T_ASSERT_NOTNULL(s, "created session");

	T_QUIET;
	T_ASSERT_POSIX_ZERO(ktrace_filter_pid(s, getpid()),
	    "filtered to current process");

	expect_dyld_events(s, "mapping", DBG_DYLD_UUID_MAP_A, map_uuid,
	    MAP_LOAD_ADDR, &map_fsid, &map_fsobjid, saw_mapping);
	expect_dyld_events(s, "unmapping", DBG_DYLD_UUID_UNMAP_A, unmap_uuid,
	    UNMAP_LOAD_ADDR, &unmap_fsid, &unmap_fsobjid, saw_unmapping);
	expect_dyld_events(s, "shared cache", DBG_DYLD_UUID_SHARED_CACHE_A,
	    sc_uuid, SC_LOAD_ADDR, &sc_fsid, &sc_fsobjid, saw_shared_cache);

	ktrace_set_completion_handler(s, ^{
		ktrace_session_destroy(s);

		T_EXPECT_EQ(__builtin_popcount(*saw_mapping), DYLD_EVENTS, NULL);
		T_EXPECT_EQ(__builtin_popcount(*saw_unmapping), DYLD_EVENTS, NULL);
		T_EXPECT_EQ(__builtin_popcount(*saw_shared_cache), DYLD_EVENTS, NULL);
		T_END;
	});

	T_ASSERT_POSIX_ZERO(ktrace_start(s, dispatch_get_main_queue()), NULL);

	info.load_addr = MAP_LOAD_ADDR;
	memcpy(info.uuid, map_uuid, sizeof(info.uuid));
	info.fsid = map_fsid;
	info.fsobjid = map_fsobjid;
	T_EXPECT_MACH_SUCCESS(task_register_dyld_image_infos(mach_task_self(),
	    &info, 1), "registered dyld image info");

	info.load_addr = UNMAP_LOAD_ADDR;
	memcpy(info.uuid, unmap_uuid, sizeof(info.uuid));
	info.fsid = unmap_fsid;
	info.fsobjid = unmap_fsobjid;
	T_EXPECT_MACH_SUCCESS(task_unregister_dyld_image_infos(mach_task_self(),
	    &info, 1), "unregistered dyld image info");

	info.load_addr = SC_LOAD_ADDR;
	memcpy(info.uuid, sc_uuid, sizeof(info.uuid));
	info.fsid = sc_fsid;
	info.fsobjid = sc_fsobjid;
	T_EXPECT_MACH_SUCCESS(task_register_dyld_shared_cache_image_info(
		    mach_task_self(), info, FALSE, FALSE),
	    "registered dyld shared cache image info");

	ktrace_end(s, 0);

	dispatch_main();
}

#pragma mark kdebug kernel macros

#define EXP_KERNEL_EVENTS 5U

static const uint32_t dev_evts[EXP_KERNEL_EVENTS] = {
	BSDDBG_CODE(DBG_BSD_KDEBUG_TEST, 0),
	BSDDBG_CODE(DBG_BSD_KDEBUG_TEST, 1),
	BSDDBG_CODE(DBG_BSD_KDEBUG_TEST, 2),
	BSDDBG_CODE(DBG_BSD_KDEBUG_TEST, 3),
	BSDDBG_CODE(DBG_BSD_KDEBUG_TEST, 4),
};

static const uint32_t rel_evts[EXP_KERNEL_EVENTS] = {
	BSDDBG_CODE(DBG_BSD_KDEBUG_TEST, 5),
	BSDDBG_CODE(DBG_BSD_KDEBUG_TEST, 6),
	BSDDBG_CODE(DBG_BSD_KDEBUG_TEST, 7),
	BSDDBG_CODE(DBG_BSD_KDEBUG_TEST, 8),
	BSDDBG_CODE(DBG_BSD_KDEBUG_TEST, 9),
};

static const uint32_t filt_evts[EXP_KERNEL_EVENTS] = {
	BSDDBG_CODE(DBG_BSD_KDEBUG_TEST, 10),
	BSDDBG_CODE(DBG_BSD_KDEBUG_TEST, 11),
	BSDDBG_CODE(DBG_BSD_KDEBUG_TEST, 12),
	BSDDBG_CODE(DBG_BSD_KDEBUG_TEST, 13),
	BSDDBG_CODE(DBG_BSD_KDEBUG_TEST, 14),
};

static const uint32_t noprocfilt_evts[EXP_KERNEL_EVENTS] = {
	BSDDBG_CODE(DBG_BSD_KDEBUG_TEST, 15),
	BSDDBG_CODE(DBG_BSD_KDEBUG_TEST, 16),
	BSDDBG_CODE(DBG_BSD_KDEBUG_TEST, 17),
	BSDDBG_CODE(DBG_BSD_KDEBUG_TEST, 18),
	BSDDBG_CODE(DBG_BSD_KDEBUG_TEST, 19),
};

static void
expect_event(struct trace_point *tp, const char *name, unsigned int *events,
    const uint32_t *event_ids, size_t event_ids_len)
{
	unsigned int event_idx = *events;
	bool event_found = false;
	size_t i;
	for (i = 0; i < event_ids_len; i++) {
		if (event_ids[i] == (tp->debugid & KDBG_EVENTID_MASK)) {
			T_LOG("found %s event 0x%x", name, tp->debugid);
			event_found = true;
		}
	}

	if (!event_found) {
		return;
	}

	*events += 1;
	for (i = 0; i < event_idx; i++) {
		T_QUIET; T_EXPECT_EQ(((uint64_t *)&tp->arg1)[i], (uint64_t)i + 1,
		    NULL);
	}
	for (; i < 4; i++) {
		T_QUIET; T_EXPECT_EQ(((uint64_t *)&tp->arg1)[i], (uint64_t)0, NULL);
	}
}

static void
expect_release_event(struct trace_point *tp, unsigned int *events)
{
	expect_event(tp, "release", events, rel_evts,
	    sizeof(rel_evts) / sizeof(rel_evts[0]));
}

static void
expect_development_event(struct trace_point *tp, unsigned int *events)
{
	expect_event(tp, "dev", events, dev_evts, sizeof(dev_evts) / sizeof(dev_evts[0]));
}

static void
expect_filtered_event(struct trace_point *tp, unsigned int *events)
{
	expect_event(tp, "filtered", events, filt_evts,
	    sizeof(filt_evts) / sizeof(filt_evts[0]));
}

static void
expect_noprocfilt_event(struct trace_point *tp, unsigned int *events)
{
	expect_event(tp, "noprocfilt", events, noprocfilt_evts,
	    sizeof(noprocfilt_evts) / sizeof(noprocfilt_evts[0]));
}

static void
expect_kdbg_test_events(ktrace_session_t s, bool use_all_callback,
    void (^cb)(unsigned int dev_seen, unsigned int rel_seen,
    unsigned int filt_seen, unsigned int noprocfilt_seen))
{
	__block unsigned int dev_seen = 0;
	__block unsigned int rel_seen = 0;
	__block unsigned int filt_seen = 0;
	__block unsigned int noprocfilt_seen = 0;

	void (^evtcb)(struct trace_point *tp) = ^(struct trace_point *tp) {
		expect_development_event(tp, &dev_seen);
		expect_release_event(tp, &rel_seen);
		expect_filtered_event(tp, &filt_seen);
		expect_noprocfilt_event(tp, &noprocfilt_seen);
	};

	if (use_all_callback) {
		ktrace_events_all(s, evtcb);
	} else {
		ktrace_events_range(s, KDBG_EVENTID(DBG_BSD, DBG_BSD_KDEBUG_TEST, 0),
		    KDBG_EVENTID(DBG_BSD + 1, 0, 0), evtcb);
	}

	ktrace_set_completion_handler(s, ^{
		ktrace_session_destroy(s);
		cb(dev_seen, rel_seen, filt_seen, noprocfilt_seen);
		T_END;
	});

	T_ASSERT_POSIX_ZERO(ktrace_start(s, dispatch_get_main_queue()), NULL);
	assert_kdebug_test(KDBG_TEST_MACROS, "check test macros");

	ktrace_end(s, 0);
}

T_DECL(kernel_events, "ensure kernel macros work", T_META_TAG_VM_PREFERRED)
{
	start_controlling_ktrace();

	ktrace_session_t s = ktrace_session_create();
	T_QUIET; T_WITH_ERRNO; T_ASSERT_NOTNULL(s, "created session");

	T_QUIET; T_ASSERT_POSIX_ZERO(ktrace_filter_pid(s, getpid()),
	    "filtered events to current process");

	expect_kdbg_test_events(s, false,
	    ^(unsigned int dev_seen, unsigned int rel_seen,
	    unsigned int filt_seen, unsigned int noprocfilt_seen) {
		/*
		 * Development-only events are only filtered if running on an embedded
		 * OS.
		 */
		unsigned int dev_exp;
#if (TARGET_OS_IPHONE && !TARGET_OS_SIMULATOR)
		dev_exp = is_development_kernel() ? EXP_KERNEL_EVENTS : 0U;
#else
		dev_exp = EXP_KERNEL_EVENTS;
#endif

		T_EXPECT_EQ(rel_seen, EXP_KERNEL_EVENTS,
		"release and development events seen");
		T_EXPECT_EQ(dev_seen, dev_exp, "development-only events %sseen",
		dev_exp ? "" : "not ");
		T_EXPECT_EQ(filt_seen, dev_exp, "filter-only events seen");
		T_EXPECT_EQ(noprocfilt_seen, EXP_KERNEL_EVENTS,
		"process filter-agnostic events seen");
	});

	dispatch_main();
}

T_DECL(kernel_events_filtered, "ensure that the filtered kernel macros work",
		T_META_TAG_VM_PREFERRED)
{
	start_controlling_ktrace();

	ktrace_session_t s = ktrace_session_create();
	T_QUIET; T_WITH_ERRNO; T_ASSERT_NOTNULL(s, "created session");

	T_QUIET; T_ASSERT_POSIX_ZERO(ktrace_filter_pid(s, getpid()),
	    "filtered events to current process");

	expect_kdbg_test_events(s, true,
	    ^(unsigned int dev_seen, unsigned int rel_seen,
	    unsigned int filt_seen, unsigned int noprocfilt_seen) {
		T_EXPECT_EQ(rel_seen, EXP_KERNEL_EVENTS, NULL);
#if defined(__arm64__)
		T_EXPECT_EQ(dev_seen, is_development_kernel() ? EXP_KERNEL_EVENTS : 0U,
		NULL);
#else
		T_EXPECT_EQ(dev_seen, EXP_KERNEL_EVENTS,
		"development-only events seen");
#endif /* defined(__arm64__) */
		T_EXPECT_EQ(filt_seen, 0U, "no filter-only events seen");
		T_EXPECT_EQ(noprocfilt_seen, EXP_KERNEL_EVENTS,
		"process filter-agnostic events seen");
	});

	dispatch_main();
}

T_DECL(kernel_events_noprocfilt,
    "ensure that the no process filter kernel macros work",
	T_META_TAG_VM_PREFERRED)
{
	start_controlling_ktrace();

	ktrace_session_t s = ktrace_session_create();
	T_QUIET; T_WITH_ERRNO; T_ASSERT_NOTNULL(s, "created session");

	/*
	 * Only allow launchd events through.
	 */
	T_ASSERT_POSIX_ZERO(ktrace_filter_pid(s, 1), "filtered events to launchd");
	for (size_t i = 0; i < sizeof(noprocfilt_evts) / sizeof(noprocfilt_evts[0]); i++) {
		T_QUIET;
		T_ASSERT_POSIX_ZERO(ktrace_ignore_process_filter_for_event(s,
		    noprocfilt_evts[i]),
		    "ignored process filter for noprocfilt event");
	}

	expect_kdbg_test_events(s, false,
	    ^(unsigned int dev_seen, unsigned int rel_seen,
	    unsigned int filt_seen, unsigned int noprocfilt_seen) {
		T_EXPECT_EQ(rel_seen, 0U, "release and development events not seen");
		T_EXPECT_EQ(dev_seen, 0U, "development-only events not seen");
		T_EXPECT_EQ(filt_seen, 0U, "filter-only events not seen");

		T_EXPECT_EQ(noprocfilt_seen, EXP_KERNEL_EVENTS,
		"process filter-agnostic events seen");
	});

	dispatch_main();
}

static volatile bool continue_abuse = true;

#define STRESS_DEBUGID (0xfeedfac0)
#define ABUSE_SECS (2)
#define TIMER_NS (100 * NSEC_PER_USEC)
/*
 * Use the quantum as the gap threshold.
 */
#define GAP_THRESHOLD_NS (10 * NSEC_PER_MSEC)

static void *
kdebug_abuser_thread(void *ctx)
{
	unsigned int id = (unsigned int)ctx;
	uint64_t i = 0;
	while (continue_abuse) {
		kdebug_trace(STRESS_DEBUGID, id, i, 0, 0);
		i++;
	}

	return NULL;
}

T_DECL(stress, "emit events on all but one CPU with a small buffer",
    T_META_CHECK_LEAKS(false),
	T_META_TAG_VM_PREFERRED)
{
	start_controlling_ktrace();

	T_SETUPBEGIN;
	ktrace_session_t s = ktrace_session_create();
	T_WITH_ERRNO; T_QUIET; T_ASSERT_NOTNULL(s, "ktrace_session_create");

	/* Let's not waste any time with pleasantries. */
	ktrace_set_uuid_map_enabled(s, KTRACE_FEATURE_DISABLED);

	/* Ouch. */
	ktrace_events_all(s, ^(__unused struct trace_point *tp) {});
	ktrace_set_vnode_paths_enabled(s, KTRACE_FEATURE_ENABLED);
	(void)atexit_b(^{ kperf_reset(); });
	(void)kperf_action_count_set(1);
	(void)kperf_timer_count_set(1);
	int kperror = kperf_timer_period_set(0, kperf_ns_to_ticks(TIMER_NS));
	T_QUIET; T_ASSERT_POSIX_SUCCESS(kperror, "kperf_timer_period_set %llu ns",
	    TIMER_NS);
	kperror = kperf_timer_action_set(0, 1);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(kperror, "kperf_timer_action_set");
	kperror = kperf_action_samplers_set(1, KPERF_SAMPLER_TINFO |
	    KPERF_SAMPLER_TH_SNAPSHOT | KPERF_SAMPLER_KSTACK |
	    KPERF_SAMPLER_USTACK | KPERF_SAMPLER_MEMINFO |
	    KPERF_SAMPLER_TINFO_SCHED | KPERF_SAMPLER_TH_DISPATCH |
	    KPERF_SAMPLER_TK_SNAPSHOT | KPERF_SAMPLER_SYS_MEM |
	    KPERF_SAMPLER_TH_INSTRS_CYCLES);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(kperror, "kperf_action_samplers_set");
	/* You monster... */

	/* The coup-de-grace. */
	ktrace_set_buffer_size(s, 10);

	char filepath_arr[MAXPATHLEN] = "";
	strlcpy(filepath_arr, dt_tmpdir(), sizeof(filepath_arr));
	strlcat(filepath_arr, "/stress.ktrace", sizeof(filepath_arr));
	char *filepath = filepath_arr;

	int ncpus = 0;
	size_t ncpus_size = sizeof(ncpus);
	int ret = sysctlbyname("hw.logicalcpu_max", &ncpus, &ncpus_size, NULL, 0);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "sysctlbyname(\"hw.logicalcpu_max\"");
	T_QUIET; T_ASSERT_GT(ncpus, 0, "realistic number of CPUs");

	pthread_t *threads = calloc((unsigned int)ncpus - 1, sizeof(pthread_t));
	T_WITH_ERRNO; T_QUIET; T_ASSERT_NOTNULL(threads, "calloc(%d threads)",
	    ncpus - 1);

	ktrace_set_completion_handler(s, ^{
		T_SETUPBEGIN;
		ktrace_session_destroy(s);

		T_LOG("trace ended, searching for gaps");

		ktrace_session_t sread = ktrace_session_create();
		T_WITH_ERRNO; T_QUIET; T_ASSERT_NOTNULL(sread, "ktrace_session_create");

		int error = ktrace_set_file(sread, filepath);
		T_QUIET; T_ASSERT_POSIX_ZERO(error, "ktrace_set_file %s", filepath);

		ktrace_file_t f = ktrace_file_open(filepath, false);
		T_QUIET; T_WITH_ERRNO; T_ASSERT_NOTNULL(f, "ktrace_file_open %s",
		filepath);
		uint64_t first_timestamp = 0;
		error = ktrace_file_earliest_timestamp(f, &first_timestamp);
		T_QUIET; T_ASSERT_POSIX_ZERO(error, "ktrace_file_earliest_timestamp");

		uint64_t last_timestamp = 0;
		(void)ktrace_file_latest_timestamp(f, &last_timestamp);

		__block uint64_t prev_timestamp = 0;
		__block uint64_t nevents = 0;
		ktrace_events_all(sread, ^(struct trace_point *tp) {
			nevents++;
			uint64_t delta_ns = 0;
			T_QUIET; T_EXPECT_GE(tp->timestamp, prev_timestamp,
			"timestamps are monotonically increasing");
			int converror = ktrace_convert_timestamp_to_nanoseconds(sread,
			tp->timestamp - prev_timestamp, &delta_ns);
			T_QUIET; T_ASSERT_POSIX_ZERO(converror, "convert timestamp to ns");
			if (prev_timestamp && delta_ns > GAP_THRESHOLD_NS) {
			        if (tp->debugname) {
			                T_LOG("gap: %gs at %llu - %llu on %d: %s (%#08x)",
			                (double)delta_ns / 1e9, prev_timestamp,
			                tp->timestamp, tp->cpuid, tp->debugname, tp->debugid);
				} else {
			                T_LOG("gap: %gs at %llu - %llu on %d: %#x",
			                (double)delta_ns / 1e9, prev_timestamp,
			                tp->timestamp, tp->cpuid, tp->debugid);
				}

			        /*
			         * These gaps are ok -- they appear after CPUs are brought back
			         * up.
			         */
#define INTERRUPT (0x1050000)
#define PERF_CPU_IDLE (0x27001000)
#define INTC_HANDLER (0x5000004)
#define DECR_TRAP (0x1090000)
			        uint32_t eventid = tp->debugid & KDBG_EVENTID_MASK;
			        if (eventid != INTERRUPT && eventid != PERF_CPU_IDLE &&
			        eventid != INTC_HANDLER && eventid != DECR_TRAP) {
			                unsigned int lost_events = TRACE_LOST_EVENTS;
			                T_QUIET; T_EXPECT_EQ(tp->debugid, lost_events,
			                "gaps should end with lost events");
				}
			}

			prev_timestamp = tp->timestamp;
		});
		ktrace_events_single(sread, TRACE_LOST_EVENTS, ^(struct trace_point *tp){
			T_LOG("lost: %llu on %d (%llu)", tp->timestamp, tp->cpuid, tp->arg1);
		});

		__block uint64_t last_write = 0;
		ktrace_events_single_paired(sread, TRACE_WRITING_EVENTS,
		^(struct trace_point *start, struct trace_point *end) {
			uint64_t delta_ns;
			int converror = ktrace_convert_timestamp_to_nanoseconds(sread,
			start->timestamp - last_write, &delta_ns);
			T_QUIET; T_ASSERT_POSIX_ZERO(converror, "convert timestamp to ns");

			uint64_t dur_ns;
			converror = ktrace_convert_timestamp_to_nanoseconds(sread,
			end->timestamp - start->timestamp, &dur_ns);
			T_QUIET; T_ASSERT_POSIX_ZERO(converror, "convert timestamp to ns");

			T_LOG("write: %llu (+%gs): %gus on %d: %llu events", start->timestamp,
			(double)delta_ns / 1e9, (double)dur_ns / 1e3, end->cpuid, end->arg1);
			last_write = end->timestamp;
		});
		ktrace_set_completion_handler(sread, ^{
			uint64_t duration_ns = 0;
			if (last_timestamp) {
			        int converror = ktrace_convert_timestamp_to_nanoseconds(sread,
			        last_timestamp - first_timestamp, &duration_ns);
			        T_QUIET; T_ASSERT_POSIX_ZERO(converror,
			        "convert timestamp to ns");
			        T_LOG("file was %gs long, %llu events: %g events/msec/cpu",
			        (double)duration_ns / 1e9, nevents,
			        (double)nevents / ((double)duration_ns / 1e6) / ncpus);
			}
			(void)unlink(filepath);
			ktrace_session_destroy(sread);
			T_END;
		});

		int starterror = ktrace_start(sread, dispatch_get_main_queue());
		T_QUIET; T_ASSERT_POSIX_ZERO(starterror, "ktrace_start read session");

		T_SETUPEND;
	});

/* Just kidding... for now. */
#if 0
	kperror = kperf_sample_set(1);
	T_ASSERT_POSIX_SUCCESS(kperror,
	    "started kperf timer sampling every %llu ns", TIMER_NS);
#endif

	for (int i = 0; i < (ncpus - 1); i++) {
		int error = pthread_create(&threads[i], NULL, kdebug_abuser_thread,
		    (void *)(uintptr_t)i);
		T_QUIET; T_ASSERT_POSIX_ZERO(error,
		    "pthread_create abuser thread %d", i);
	}

	int error = ktrace_start_writing_file(s, filepath,
	    ktrace_compression_none, NULL, NULL);
	T_ASSERT_POSIX_ZERO(error, "started writing ktrace to %s", filepath);

	T_SETUPEND;

	dispatch_after(dispatch_time(DISPATCH_TIME_NOW, ABUSE_SECS * NSEC_PER_SEC),
	    dispatch_get_main_queue(), ^{
		T_LOG("ending trace");
		ktrace_end(s, 1);

		continue_abuse = false;
		for (int i = 0; i < (ncpus - 1); i++) {
		        int joinerror = pthread_join(threads[i], NULL);
		        T_QUIET; T_EXPECT_POSIX_ZERO(joinerror, "pthread_join thread %d",
		        i);
		}
	});

	dispatch_main();
}

#define ROUND_TRIP_PERIOD UINT64_C(10 * 1000)
#define ROUND_TRIPS_THRESHOLD UINT64_C(25)
#define ROUND_TRIPS_TIMEOUT_SECS (2 * 60)
#define COLLECTION_INTERVAL_MS 100

/*
 * Test a sustained tracing session, involving multiple round-trips to the
 * kernel.
 *
 * Trace all events, and every `ROUND_TRIP_PERIOD` events, emit an event that's
 * unlikely to be emitted elsewhere.  Look for this event, too, and make sure we
 * see as many of them as we emitted.
 *
 * After seeing `ROUND_TRIPS_THRESHOLD` of the unlikely events, end tracing.
 * In the failure mode, we won't see any of these, so set a timeout of
 * `ROUND_TRIPS_TIMEOUT_SECS` to prevent hanging, waiting for events that we'll
 * never see.
 */
T_DECL(round_trips,
    "test sustained tracing with multiple round-trips through the kernel",
	T_META_TAG_VM_PREFERRED)
{
	start_controlling_ktrace();

	ktrace_session_t s = ktrace_session_create();
	T_QUIET; T_WITH_ERRNO; T_ASSERT_NOTNULL(s, "created session");

	/*
	 * Set a small buffer and collection interval to increase the number of
	 * round-trips.
	 */
	ktrace_set_buffer_size(s, 50);
	ktrace_set_collection_interval(s, COLLECTION_INTERVAL_MS);

	__block uint64_t events = 0;
	__block uint64_t emitted = 0;
	__block uint64_t seen = 0;
	ktrace_events_all(s, ^(__unused struct trace_point *tp) {
		events++;
		if (events % ROUND_TRIP_PERIOD == 0) {
		        T_LOG("emitting round-trip event %" PRIu64, emitted);
		        kdebug_trace(TRACE_DEBUGID, events, 0, 0, 0);
		        emitted++;
		}
	});

	ktrace_events_single(s, TRACE_DEBUGID, ^(__unused struct trace_point *tp) {
		T_LOG("saw round-trip event after %" PRIu64 " events", events);
		seen++;
		if (seen >= ROUND_TRIPS_THRESHOLD) {
		        T_LOG("ending trace after seeing %" PRIu64 " events, "
		        "emitting %" PRIu64, seen, emitted);
		        ktrace_end(s, 1);
		}
	});

	ktrace_set_completion_handler(s, ^{
		T_EXPECT_GE(emitted, ROUND_TRIPS_THRESHOLD,
		"emitted %" PRIu64 " round-trip events", emitted);
		T_EXPECT_GE(seen, ROUND_TRIPS_THRESHOLD,
		"saw %" PRIu64 " round-trip events", seen);
		ktrace_session_destroy(s);
		T_END;
	});

	int error = ktrace_start(s, dispatch_get_main_queue());
	T_ASSERT_POSIX_ZERO(error, "started tracing");

	dispatch_after(dispatch_time(DISPATCH_TIME_NOW,
	    ROUND_TRIPS_TIMEOUT_SECS * NSEC_PER_SEC), dispatch_get_main_queue(),
	    ^{
		T_LOG("ending trace after %d seconds", ROUND_TRIPS_TIMEOUT_SECS);
		ktrace_end(s, 0);
	});

	dispatch_main();
}

#define HEARTBEAT_INTERVAL_SECS 1
#define HEARTBEAT_COUNT 10

/*
 * Ensure we see events periodically, checking for recent events on a
 * heart-beat.
 */
T_DECL(event_coverage, "ensure events appear up to the end of tracing",
		T_META_TAG_VM_PREFERRED)
{
	start_controlling_ktrace();

	ktrace_session_t s = ktrace_session_create();
	T_QUIET; T_WITH_ERRNO; T_ASSERT_NOTNULL(s, "created session");

	__block uint64_t current_timestamp = 0;
	__block uint64_t events = 0;
	ktrace_events_all(s, ^(struct trace_point *tp) {
		current_timestamp = tp->timestamp;
		events++;
	});

	ktrace_set_buffer_size(s, 20);
	ktrace_set_collection_interval(s, COLLECTION_INTERVAL_MS);

	__block uint64_t last_timestamp = 0;
	__block uint64_t last_events = 0;
	__block unsigned int heartbeats = 0;

	ktrace_set_completion_handler(s, ^{
		ktrace_session_destroy(s);
		T_QUIET; T_EXPECT_GT(events, 0ULL, "should have seen some events");
		T_END;
	});

	dispatch_source_t timer = dispatch_source_create(DISPATCH_SOURCE_TYPE_TIMER,
	    0, 0, dispatch_get_main_queue());
	dispatch_source_set_timer(timer, dispatch_time(DISPATCH_TIME_NOW,
	    HEARTBEAT_INTERVAL_SECS * NSEC_PER_SEC),
	    HEARTBEAT_INTERVAL_SECS * NSEC_PER_SEC, 0);
	dispatch_source_set_cancel_handler(timer, ^{
		dispatch_release(timer);
	});

	dispatch_source_set_event_handler(timer, ^{
		heartbeats++;

		T_LOG("heartbeat %u at time %lld, seen %" PRIu64 " events, "
		"current event time %lld", heartbeats, mach_absolute_time(),
		events, current_timestamp);

		if (current_timestamp > 0) {
		        T_EXPECT_GT(current_timestamp, last_timestamp,
		        "event timestamps should be increasing");
		        T_QUIET; T_EXPECT_GT(events, last_events,
		        "number of events should be increasing");
		}

		last_timestamp = current_timestamp;
		last_events = events;

		kdebug_trace(TRACE_DEBUGID, 0, 0, 0, 0);

		if (heartbeats >= HEARTBEAT_COUNT) {
		        T_LOG("ending trace after %u heartbeats", HEARTBEAT_COUNT);
		        ktrace_end(s, 0);
		}
	});

	int error = ktrace_start(s, dispatch_get_main_queue());
	T_ASSERT_POSIX_ZERO(error, "started tracing");

	dispatch_activate(timer);

	dispatch_main();
}

static unsigned int
get_nevents(void)
{
	kbufinfo_t bufinfo = { 0 };
	T_QUIET;
	T_ASSERT_POSIX_SUCCESS(sysctl(
		    (int[]){ CTL_KERN, KERN_KDEBUG, KERN_KDGETBUF }, 3,
		    &bufinfo, &(size_t){ sizeof(bufinfo) }, NULL, 0),
	    "get kdebug buffer size");

	return (unsigned int)bufinfo.nkdbufs;
}

static unsigned int
set_nevents(unsigned int nevents)
{
	T_QUIET;
	T_ASSERT_POSIX_SUCCESS(sysctl(
		    (int[]){ CTL_KERN, KERN_KDEBUG, KERN_KDSETBUF, (int)nevents }, 4,
		    NULL, 0, NULL, 0), "set kdebug buffer size");

	T_QUIET;
	T_ASSERT_POSIX_SUCCESS(sysctl(
		    (int[]){ CTL_KERN, KERN_KDEBUG, KERN_KDSETUP, (int)nevents }, 4,
		    NULL, 0, NULL, 0), "setup kdebug buffers");

	unsigned int nevents_allocated = get_nevents();

	T_QUIET;
	T_ASSERT_POSIX_SUCCESS(sysctl(
		    (int[]){ CTL_KERN, KERN_KDEBUG, KERN_KDREMOVE }, 3,
		    NULL, 0, NULL, 0),
	    "remove kdebug buffers");

	return nevents_allocated;
}

static unsigned int
_mb_of_events(unsigned int event_count)
{
	return (unsigned int)(((uint64_t)event_count * 64) >> 20);
}

T_DECL(set_buffer_size, "ensure large buffer sizes can be set",
		XNU_T_META_SOC_SPECIFIC, T_META_TAG_VM_NOT_ELIGIBLE)
{
	T_SETUPBEGIN;
	uint64_t memsize = 0;
	T_QUIET; T_ASSERT_POSIX_SUCCESS(sysctlbyname("hw.memsize", &memsize,
	    &(size_t){ sizeof(memsize) }, NULL, 0), "sysctl hw.memsize");
	T_SETUPEND;

#if TARGET_OS_IPHONE
	if (memsize >= (8ULL << 30)) {
		T_SKIP("skipping on iOS device with memory >= 8GB, rdar://79403304");
	}
#endif // TARGET_OS_IPHONE

	start_controlling_ktrace();

	// Try to allocate up to one-eighth of available memory towards
	// tracing.
	uint64_t maxevents_u64 = memsize / 8 / sizeof(kd_buf);
	if (maxevents_u64 > UINT32_MAX) {
		maxevents_u64 = UINT32_MAX;
	}
	unsigned int maxevents = (unsigned int)maxevents_u64;

	// Use hexadecimal representation to prevent failure signaturization on these values.
	unsigned int minevents = set_nevents(0);
	T_ASSERT_GT(minevents, 0, "saw non-zero minimum event count of %#x",
	    minevents);

	unsigned int step = ((maxevents - minevents - 1) / 4);
	T_ASSERT_GT(step, 0, "stepping by %#x events (%#x MiB)", step,
	    _mb_of_events(step));

	for (unsigned int i = minevents + step; i < maxevents; i += step) {
		unsigned int actualevents = set_nevents(i);
		T_ASSERT_GE(actualevents, i - minevents,
		    "%#x events (%#x MiB) in kernel when %#x (%#x MiB) requested",
		    actualevents, _mb_of_events(actualevents), i, _mb_of_events(i));
	}
}

static void *
donothing(__unused void *arg)
{
	return NULL;
}

T_DECL(long_names, "ensure long command names are reported",
		T_META_TAG_VM_PREFERRED)
{
	start_controlling_ktrace();

	char longname[] = "thisisaverylongprocessname!";
	char *longname_ptr = longname;
	static_assert(sizeof(longname) > 16,
	    "the name should be longer than MAXCOMLEN");

	int ret = sysctlbyname("kern.procname", NULL, NULL, longname,
	    sizeof(longname));
	T_ASSERT_POSIX_SUCCESS(ret,
	    "use sysctl kern.procname to lengthen the name");

	ktrace_session_t ktsess = ktrace_session_create();

	/*
	 * 32-bit kernels can only trace 16 bytes of the string in their event
	 * arguments.
	 */
	if (!ktrace_is_kernel_64_bit(ktsess)) {
		longname[16] = '\0';
	}

	ktrace_filter_pid(ktsess, getpid());

	__block bool saw_newthread = false;
	ktrace_events_single(ktsess, TRACE_STRING_NEWTHREAD,
	    ^(struct trace_point *tp) {
		if (ktrace_get_pid_for_thread(ktsess, tp->threadid) ==
		    getpid()) {
			saw_newthread = true;

			char argname[32] = {};
			strncat(argname, (char *)&tp->arg1, sizeof(tp->arg1));
			strncat(argname, (char *)&tp->arg2, sizeof(tp->arg2));
			strncat(argname, (char *)&tp->arg3, sizeof(tp->arg3));
			strncat(argname, (char *)&tp->arg4, sizeof(tp->arg4));

			T_EXPECT_EQ_STR((char *)argname, longname_ptr,
			    "process name of new thread should be long");

			ktrace_end(ktsess, 1);
		}
	});

	ktrace_set_completion_handler(ktsess, ^{
		ktrace_session_destroy(ktsess);
		T_EXPECT_TRUE(saw_newthread,
		    "should have seen the new thread");
		T_END;
	});

	int error = ktrace_start(ktsess, dispatch_get_main_queue());
	T_ASSERT_POSIX_ZERO(error, "started tracing");

	pthread_t thread = NULL;
	error = pthread_create(&thread, NULL, donothing, NULL);
	T_ASSERT_POSIX_ZERO(error, "create new thread");

	dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 5 * NSEC_PER_SEC),
	    dispatch_get_main_queue(), ^{
		ktrace_end(ktsess, 0);
	});

	error = pthread_join(thread, NULL);
	T_ASSERT_POSIX_ZERO(error, "join to thread");

	dispatch_main();
}

T_DECL(continuous_time, "make sure continuous time status can be queried",
	T_META_RUN_CONCURRENTLY(true), T_META_TAG_VM_PREFERRED)
{
	bool cont_time = kdebug_using_continuous_time();
	T_ASSERT_FALSE(cont_time, "should not be using continuous time yet");
}

T_DECL(lookup_long_paths, "lookup long path names",
		T_META_TAG_VM_PREFERRED)
{
	start_controlling_ktrace();

	int ret = chdir("/tmp");
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "chdir to /tmp");
	const char *dir = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/";
	int i = 0;
	do {
		ret = mkdir(dir, S_IRUSR | S_IWUSR | S_IXUSR);
		if (ret >= 0 || errno != EEXIST) {
			T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "mkdir of %d nested directory",
			    i);
		}
		ret = chdir(dir);
		T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "chdir to %d nested directory", i);
	} while (i++ < 40);

	ktrace_session_t s = ktrace_session_create();
	ktrace_set_collection_interval(s, 250);
	ktrace_filter_pid(s, getpid());
	T_QUIET; T_WITH_ERRNO; T_ASSERT_NOTNULL(s, "created session");
	ktrace_events_single(s, VFS_LOOKUP, ^(struct trace_point *tp __unused){});
	ktrace_set_vnode_paths_enabled(s, KTRACE_FEATURE_ENABLED);

	dispatch_queue_t q = dispatch_queue_create("com.apple.kdebug-test", 0);

	ktrace_set_completion_handler(s, ^{
		dispatch_release(q);
		T_END;
	});

	int error = ktrace_start(s, q);
	T_ASSERT_POSIX_ZERO(error, "started tracing");

	int fd = open("bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb", O_RDWR | O_CREAT);
	T_ASSERT_POSIX_SUCCESS(fd, "opened file at %d directories deep", i);

	sleep(5);

	T_LOG("ending tracing");
	ktrace_end(s, 0);
	dispatch_main();
}

#pragma mark - boot tracing

static void
expect_kernel_task_tracing(void)
{
	unsigned int state = 0;
	size_t state_size = sizeof(state);
	int ret = sysctlbyname("ktrace.state", &state, &state_size, NULL, 0);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "sysctl(ktrace.state)");
	T_ASSERT_EQ(state, 1, "state is foreground");

	char configured_by[1024] = "";
	size_t configured_by_size = sizeof(configured_by);
	ret = sysctlbyname("ktrace.configured_by", &configured_by,
	    &configured_by_size, NULL, 0);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "sysctl(ktrace.configured_by)");
	T_ASSERT_EQ_STR(configured_by, "kernel_task", "configured by kernel_task");
}

static const char *expected_subsystems[] = {
	"tunables", "locks", "kprintf", "pmap_steal", "kmem", "zalloc",
	/* "percpu", only has a startup phase on Intel */
	"codesigning", "oslog", "early_boot",
};
#define EXPECTED_SUBSYSTEMS_LEN \
		(sizeof(expected_subsystems) / sizeof(expected_subsystems[0]))

T_DECL(early_boot_tracing, "ensure early boot strings are present",
	T_META_BOOTARGS_SET("trace=1000000"), XNU_T_META_SOC_SPECIFIC, T_META_TAG_VM_NOT_ELIGIBLE)
{
	T_ATEND(reset_ktrace);

	expect_kernel_task_tracing();

	T_SETUPBEGIN;
	ktrace_session_t s = ktrace_session_create();
	ktrace_set_collection_interval(s, 250);
	int error = ktrace_set_use_existing(s);
	T_ASSERT_POSIX_ZERO(error, "use existing trace buffer");

#if defined(__x86_64__)
#define FIRST_EVENT_STRING "i386_init"
#else /* defined(__x86_64__) */
#define FIRST_EVENT_STRING "kernel_startup_bootstrap"
#endif /* !defined(__x86_64__) */

	__block bool seen_event = false;
	__block size_t cur_subsystem = 0;
	ktrace_events_single(s, TRACE_INFO_STRING, ^(struct trace_point *tp) {
		char early_str[33] = "";
		size_t argsize = ktrace_is_kernel_64_bit(s) ? 8 : 4;
		memcpy(early_str, &tp->arg1, argsize);
		memcpy(early_str + argsize, &tp->arg2, argsize);
		memcpy(early_str + argsize * 2, &tp->arg3, argsize);
		memcpy(early_str + argsize * 3, &tp->arg4, argsize);

		if (!seen_event) {
			T_LOG("found first string event with args: "
			    "0x%" PRIx64 ", 0x%" PRIx64 ", 0x%" PRIx64 ", 0x%" PRIx64,
			    tp->arg1, tp->arg2, tp->arg3, tp->arg4);
			char expect_str[33] = FIRST_EVENT_STRING;
			if (!ktrace_is_kernel_64_bit(s)) {
				// Only the first 16 bytes of the string will be traced.
				expect_str[16] = '\0';
			}

			T_EXPECT_EQ_STR(early_str, expect_str,
			    "first event in boot trace should be the bootstrap message");
		}
		seen_event = true;

		if (strcmp(early_str, expected_subsystems[cur_subsystem]) == 0) {
			T_LOG("found log for subsystem `%s'",
					expected_subsystems[cur_subsystem]);
			cur_subsystem++;
		} else {
			T_LOG("saw extra log for subsystem `%s'", early_str);
		}

		if (cur_subsystem == EXPECTED_SUBSYSTEMS_LEN) {
			T_LOG("ending after seeing all expected logs");
			ktrace_end(s, 1);
		}
	});

	ktrace_set_completion_handler(s, ^{
		T_EXPECT_TRUE(seen_event, "should see an early boot string event");
		T_EXPECT_EQ(cur_subsystem, EXPECTED_SUBSYSTEMS_LEN,
				"should see logs from all subsystems");
		if (cur_subsystem != EXPECTED_SUBSYSTEMS_LEN) {
			T_LOG("missing log for %s", expected_subsystems[cur_subsystem]);
		}
		T_END;
	});

	error = ktrace_start(s, dispatch_get_main_queue());
	T_ASSERT_POSIX_ZERO(error, "started tracing");

	T_SETUPEND;

	dispatch_main();
}

// Allocating ~4TB should be clamped to some lower number.
T_DECL(early_boot_tracing_too_large,
    "ensure early boot tracing can allocate up to a clamped size",
	T_META_BOOTARGS_SET("trace=64000000000"), XNU_T_META_SOC_SPECIFIC, T_META_TAG_VM_NOT_ELIGIBLE)
{
	expect_kernel_task_tracing();
	T_EXPECT_NE(get_nevents(), 0, "allocated some events");
}

// Not SoC-specific because the typefilter parsing logic is generic.
T_DECL(typefilter_boot_arg, "ensure typefilter is set up correctly at boot",
	T_META_BOOTARGS_SET("trace=100000 trace_typefilter=S0x0c00,C0xfe"),
	T_META_TAG_VM_PREFERRED)
{
	T_ATEND(reset_ktrace);

	T_SETUPBEGIN;
	ktrace_config_t config = ktrace_config_create_current();
	T_QUIET; T_WITH_ERRNO;
	T_ASSERT_NOTNULL(config, "create config from current system");
	T_SETUPEND;

	T_LOG("ktrace configuration:");
	ktrace_config_print_description(config, stdout);

	uint8_t *typefilt = ktrace_config_kdebug_get_typefilter(config);
	T_ASSERT_NOTNULL(typefilt, "typefilter is active");
	T_EXPECT_TRUE(typefilt[0x0c00 / 8],
			"specified subclass is set in typefilter");
	T_MAYFAIL; // rdar://63625062 (UTD converts commas in boot-args to spaces)
	T_EXPECT_TRUE(typefilt[0xfeed / 8],
			"specified class is set in typefilter");

	ktrace_config_destroy(config);
}

#pragma mark - events present

static int recvd_sigchild = 0;
static void
sighandler(int sig)
{
	if (sig != SIGCHLD) {
		T_ASSERT_FAIL("unexpected signal: %d", sig);
	}
	recvd_sigchild = 1;
}

#define END_EVENT (0xfeedfac0)

T_DECL(instrs_and_cycles_on_proc_exit,
		"instructions and cycles should be traced on thread exit",
		T_META_REQUIRES_SYSCTL_EQ("kern.monotonic.supported", 1),
		T_META_TAG_VM_NOT_ELIGIBLE)
{
	T_SETUPBEGIN;
	start_controlling_ktrace();
	int error;
	struct rusage_info_v4 *rusage = calloc(1, sizeof(*rusage));
	char *args[] = { "ls", "-l", NULL, };
	int status;
	dispatch_queue_t q = dispatch_queue_create("com.apple.kdebug-test",
			DISPATCH_QUEUE_SERIAL);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(signal(SIGCHLD, sighandler),
			"register signal handler");

	ktrace_session_t s = ktrace_session_create();
	T_QUIET; T_WITH_ERRNO; T_ASSERT_NOTNULL(s, "ktrace_session_create");
	ktrace_set_collection_interval(s, 100);

	__block pid_t pid;
	__block bool seen_event = false;
	__block uint64_t proc_instrs = 0;
	__block uint64_t proc_cycles = 0;
	__block uint64_t proc_sys_time = 0;
	__block uint64_t proc_usr_time = 0;
	error = ktrace_events_single(s, DBG_MT_INSTRS_CYCLES_PROC_EXIT,
			^(ktrace_event_t tp){
		if (tp->pid == pid) {
			seen_event = true;
			proc_instrs = tp->arg1;
			proc_cycles = tp->arg2;
			proc_sys_time = tp->arg3;
			proc_usr_time = tp->arg4;
			ktrace_end(s, 1);
		}
	});
	T_QUIET; T_WITH_ERRNO; T_ASSERT_POSIX_ZERO(error, "trace single event");
	error = ktrace_events_single(s, END_EVENT, ^(ktrace_event_t __unused tp){
		T_LOG("saw ending event, stopping trace session");
		ktrace_end(s, 0);
	});
	T_QUIET; T_WITH_ERRNO; T_ASSERT_POSIX_ZERO(error, "trace single event");
	ktrace_set_completion_handler(s, ^{
		// TODO Check for equality once rdar://61948669 is fixed.
		T_ASSERT_GE(proc_instrs, rusage->ri_instructions,
				"trace event instrs are >= to rusage instrs");
		T_ASSERT_GE(proc_cycles, rusage->ri_cycles,
				"trace event cycles are >= to rusage cycles");
		T_ASSERT_GE(proc_sys_time, rusage->ri_system_time,
				"trace event sys time is >= rusage sys time");
		T_ASSERT_GE(proc_usr_time, rusage->ri_user_time,
				"trace event usr time >= rusage usr time");
		T_EXPECT_TRUE(seen_event, "should see the proc exit trace event");

		free(rusage);
		ktrace_session_destroy(s);
		dispatch_release(q);
		T_END;
	});
	error = ktrace_start(s, q);
	T_ASSERT_POSIX_ZERO(error, "start tracing");
	T_SETUPEND;

	extern char **environ;
	status = posix_spawnp(&pid, args[0], NULL, NULL, args, environ);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(status, "spawn process");
	if (status == 0) {
		while (!recvd_sigchild) {
			pause();
		}
		error = proc_pid_rusage(pid, RUSAGE_INFO_V4, (rusage_info_t)rusage);
		T_QUIET; T_ASSERT_POSIX_ZERO(error, "rusage");
		error = waitpid(pid, &status, 0);
		T_QUIET; T_ASSERT_POSIX_SUCCESS(error, "waitpid");
		kdebug_trace(END_EVENT, 0, 0, 0, 0);
	}
	dispatch_main();
}

#define NO_OF_THREADS 2

struct thread_counters_info {
	struct thsc_cpi counts;
	uint64_t cpu_time;
	uint64_t thread_id;
};
typedef struct thread_counters_info *tc_info_t;

static void*
get_thread_counters(void* ptr)
{
	extern uint64_t __thread_selfusage(void);
	extern uint64_t __thread_selfid(void);
	tc_info_t tc_info = (tc_info_t) ptr;
	tc_info->thread_id = __thread_selfid();
	// Just to increase the instr, cycle count
	T_LOG("printing %llu\n", tc_info->thread_id);
	tc_info->cpu_time = __thread_selfusage();
	(void)thread_selfcounts(THSC_CPI, &tc_info->counts, sizeof(tc_info->counts));
	return NULL;
}

T_DECL(instrs_and_cycles_on_thread_exit,
		"instructions and cycles should be traced on thread exit",
		T_META_REQUIRES_SYSCTL_EQ("kern.monotonic.supported", 1),
		T_META_TAG_VM_NOT_ELIGIBLE)
{
	T_SETUPBEGIN;
	start_controlling_ktrace();

	int error;
	pthread_t *threads = calloc((unsigned int)(NO_OF_THREADS),
			sizeof(pthread_t));
	 T_QUIET; T_WITH_ERRNO; T_ASSERT_NOTNULL(threads, "calloc(%d threads)",
	    NO_OF_THREADS);
	tc_info_t tc_infos = calloc((unsigned int) (NO_OF_THREADS),
			sizeof(struct thread_counters_info));
	T_WITH_ERRNO; T_QUIET; T_ASSERT_NOTNULL(tc_infos,
			"calloc(%d thread counters)", NO_OF_THREADS);

	ktrace_session_t s = ktrace_session_create();
	T_QUIET; T_WITH_ERRNO; T_ASSERT_NOTNULL(s, "ktrace_session_create");
	ktrace_filter_pid(s, getpid());

	__block int nevents = 0;
	error = ktrace_events_single(s, DBG_MT_INSTRS_CYCLES_THR_EXIT,
			^(ktrace_event_t tp) {
		for (int i = 0; i < NO_OF_THREADS; i++) {
			if (tp->threadid == tc_infos[i].thread_id) {
				nevents++;
				uint64_t cpu_time = tp->arg3 + tp->arg4;
				/*
				 * as we are getting counts before thread exit,
				 * the counts at thread exit should be greater than
				 * thread_selfcounts
				 */
				T_ASSERT_GE(tp->arg1, tc_infos[i].counts.tcpi_instructions,
					"trace event instrs are >= to thread's instrs");
				T_ASSERT_GE(tp->arg2, tc_infos[i].counts.tcpi_cycles,
					"trace event cycles are >= to thread's cycles");
				T_ASSERT_GE(cpu_time, tc_infos[i].cpu_time,
					"trace event cpu time is >= thread's cpu time");
			}
			if (nevents == NO_OF_THREADS) {
				ktrace_end(s, 1);
			}
		}
	});
	T_QUIET; T_ASSERT_POSIX_ZERO(error, "trace single event");
	ktrace_set_completion_handler(s, ^{
		T_EXPECT_EQ(NO_OF_THREADS, nevents, "seen %d thread exit trace events",
				NO_OF_THREADS);
		free(tc_infos);
		ktrace_session_destroy(s);
		T_END;
	});
	error = ktrace_start(s, dispatch_get_main_queue());
	T_ASSERT_POSIX_ZERO(error, "start tracing");

	for (int i = 0; i < NO_OF_THREADS; i++) {
		error = pthread_create(&threads[i], NULL, get_thread_counters,
				(void *)&tc_infos[i]);
		T_QUIET; T_ASSERT_POSIX_ZERO(error, "pthread_create thread %d", i);
	}
	T_SETUPEND;
	for (int i = 0; i < NO_OF_THREADS; i++) {
		error = pthread_join(threads[i], NULL);
		T_QUIET; T_EXPECT_POSIX_ZERO(error, "pthread_join thread %d", i);
	}

	dispatch_main();
}
