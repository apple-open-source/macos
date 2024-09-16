#include <mach/mach.h>
#include <stdlib.h>
#include <signal.h>
#include <mach/mach_time.h>

#include <darwintest.h>
#include <darwintest_utils.h>
#include "sched_test_utils.h"

static bool verbosity_enabled = true;

void
disable_verbose_sched_utils(void)
{
	T_QUIET; T_ASSERT_TRUE(verbosity_enabled, "verbosity was enabled");
	verbosity_enabled = false;
}

void
reenable_verbose_sched_utils(void)
{
	T_QUIET; T_ASSERT_EQ(verbosity_enabled, false, "verbosity was disabled");
	verbosity_enabled = true;
}

static mach_timebase_info_data_t timebase_info;
static bool initialized_timebase = false;

uint64_t
nanos_to_abs(uint64_t nanos)
{
	kern_return_t kr;
	if (!initialized_timebase) {
		kr = mach_timebase_info(&timebase_info);
		T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "mach_timebase_info");
		initialized_timebase = true;
	}
	return nanos * timebase_info.denom / timebase_info.numer;
}

uint64_t
abs_to_nanos(uint64_t abs)
{
	kern_return_t kr;
	if (!initialized_timebase) {
		kr = mach_timebase_info(&timebase_info);
		T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "mach_timebase_info");
		initialized_timebase = true;
	}
	return abs * timebase_info.numer / timebase_info.denom;
}

static int num_perf_levels = 0;
bool
platform_is_amp(void)
{
	if (num_perf_levels == 0) {
		int ret;
		ret = sysctlbyname("hw.nperflevels", &num_perf_levels, &(size_t){ sizeof(num_perf_levels) }, NULL, 0);
		T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "hw.nperflevels");
	}
	bool is_amp = num_perf_levels > 1;
	if (verbosity_enabled) {
		T_LOG("🛰️ Platform has %d perflevels (%s)", num_perf_levels, is_amp ? "AMP" : "SMP");
	}
	return is_amp;
}

bool
platform_is_virtual_machine(void)
{
	int ret;
	int vmm_present = 0;
	ret = sysctlbyname("kern.hv_vmm_present", &vmm_present, &(size_t){ sizeof(vmm_present) }, NULL, 0);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "kern.hv_vmm_present");
	if (vmm_present && verbosity_enabled) {
		T_LOG("🛰️ Platform is a virtual machine!");
	}
	return (bool)vmm_present;
}

static char sched_policy_name[64];
char *
platform_sched_policy(void)
{
	int ret;
	ret = sysctlbyname("kern.sched", sched_policy_name, &(size_t){ sizeof(sched_policy_name) }, NULL, 0);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "kern.sched");
	if (verbosity_enabled) {
		T_LOG("🛰️ Platform is running the \"%s\" scheduler policy", sched_policy_name);
	}
	return sched_policy_name;
}

void
spin_for_duration(uint32_t seconds)
{
	uint64_t duration       = nanos_to_abs((uint64_t)seconds * NSEC_PER_SEC);
	uint64_t current_time   = mach_absolute_time();
	uint64_t timeout        = duration + current_time;

	uint64_t spin_count = 0;

	while (mach_absolute_time() < timeout) {
		spin_count++;
	}
}

static const double default_idle_threshold = 0.9;
static const int default_timeout_sec = 3;

bool
wait_for_quiescence_default(void)
{
	return wait_for_quiescence(default_idle_threshold, default_timeout_sec);
}

/* Logic taken from __wait_for_quiescence in qos_tests.c */
bool
wait_for_quiescence(double idle_threshold, int timeout_seconds)
{
	kern_return_t kr;

	bool quiesced = false;
	double idle_ratio = 0.0;
	if (verbosity_enabled) {
		T_LOG("🕒 Waiting up to %d second(s) for the system to quiesce above %.2f%% idle...",
		    timeout_seconds, idle_threshold * 100.0);
	}

	host_cpu_load_info_data_t host_load;
	mach_msg_type_number_t count = HOST_CPU_LOAD_INFO_COUNT;
	int waited_seconds = 0;
	int ind = 0;
	double user_ticks[2];
	double system_ticks[2];
	double idle_ticks[2];

	while (waited_seconds < timeout_seconds) {
		kr = host_statistics(mach_host_self(), HOST_CPU_LOAD_INFO, (host_info_t)&host_load, &count);
		T_QUIET; T_ASSERT_MACH_SUCCESS(kr, "host_statistics HOST_CPU_LOAD_INFO");

		user_ticks[ind] = (double)host_load.cpu_ticks[CPU_STATE_USER];
		system_ticks[ind] = (double)host_load.cpu_ticks[CPU_STATE_SYSTEM];
		idle_ticks[ind] = (double)host_load.cpu_ticks[CPU_STATE_IDLE];

		if (waited_seconds >= 1) {
			int old_ind = (ind + 1) % 2;
			double idle_delta = idle_ticks[ind] - idle_ticks[old_ind];
			double total_delta = idle_delta + (user_ticks[ind] - user_ticks[old_ind]) + (system_ticks[ind] - system_ticks[old_ind]);
			if (total_delta > 0.0) {
				idle_ratio = idle_delta / total_delta;
				if (idle_ratio >= idle_threshold) {
					quiesced = true;
					break;
				}
				if (verbosity_enabled) {
					T_LOG("🕒 Not yet quiesced (%.2f%% idle)", idle_ratio * 100.0);
				}
			}
		}

		sleep(1);
		ind = (ind + 1) % 2;
		waited_seconds++;
	}

	if (verbosity_enabled) {
		if (quiesced) {
			T_LOG("🕒 System quiesced to %.2f%% idle within %d second(s)", idle_ratio * 100.0, waited_seconds);
		} else {
			T_LOG("🕒 Failed to quiesce within %.2f%% idle after %d second(s)", idle_threshold * 100.0, waited_seconds);
		}
	}
	return quiesced;
}

static bool atend_handler_registered = false;

static void
sched_utils_sigint_handler(int sig)
{
	T_QUIET; T_EXPECT_EQ(sig, SIGINT, "unexpected signal received");
	T_FAIL("SIGINT received. Failing test to induce ATEND handlers for cleanup...");
}

static void
register_atend_handler(void)
{
	if (!atend_handler_registered) {
		signal(SIGINT, sched_utils_sigint_handler);
		atend_handler_registered = true;
	}
}

static char *clpcctrl_bin = "/usr/local/bin/clpcctrl";
static bool running_clpcctrl_atend_handler = false;

static void
clpcctrl_cleanup(void)
{
	T_LOG("🏎️ Restoring CLPC state...");
	running_clpcctrl_atend_handler = true;

	char *recommend_all_cores_args[] = {"-C", "all", NULL};
	execute_clpcctrl(recommend_all_cores_args, false);

	char *restore_dynamic_control_args[] = {"-d", NULL};
	execute_clpcctrl(restore_dynamic_control_args, false);
}

uint64_t
execute_clpcctrl(char *clpcctrl_args[], bool read_value)
{
	int ret;

	/* Avoid recursion during teardown */
	if (!running_clpcctrl_atend_handler) {
		register_atend_handler();
		T_ATEND(clpcctrl_cleanup);
	}

	/* Populate arg array with clpcctrl location */
	char *full_clpcctrl_args[100];
	full_clpcctrl_args[0] = clpcctrl_bin;
	int arg_ind = 0;
	while (clpcctrl_args[arg_ind] != NULL) {
		T_QUIET; T_ASSERT_LT(arg_ind + 1, 100, "too many clpcctrl args");
		full_clpcctrl_args[arg_ind + 1] = clpcctrl_args[arg_ind];
		arg_ind++;
	}
	full_clpcctrl_args[arg_ind + 1] = NULL;

	__block uint64_t value = 0;
	pid_t pid = dt_launch_tool_pipe(full_clpcctrl_args, false, NULL,
	    ^bool (char *data, __unused size_t data_size, __unused dt_pipe_data_handler_context_t *context) {
		T_LOG("🏎️ [clpcctrl] %s", data);
		if (read_value) {
		        char *token = strtok(data, " ");
		        token = strtok(NULL, " ");
		        value = strtoull(token, NULL, 10);
		}
		return true;
	},
	    ^bool (char *data, __unused size_t data_size, __unused dt_pipe_data_handler_context_t *context) {
		T_LOG("🏎️ [clpcctrl] Error msg: %s", data);
		return true;
	},
	    BUFFER_PATTERN_LINE, NULL);

	ret = dt_waitpid(pid, NULL, NULL, 0);
	T_QUIET; T_EXPECT_TRUE(ret, "dt_waitpid for clpcctrl");

	return value;
}

bool
check_recommended_core_mask(uint64_t *core_mask)
{
	int ret;
	uint64_t recommended_cores = 0;
	size_t recommended_cores_size = sizeof(recommended_cores);
	ret = sysctlbyname("kern.sched_recommended_cores", &recommended_cores, &recommended_cores_size, NULL, 0);
	T_QUIET; T_EXPECT_POSIX_SUCCESS(ret, "sysctlbyname(kern.sched_recommended_cores)");

	if (verbosity_enabled) {
		uint64_t expected_recommended_mask = ~0ULL >> (64 - dt_ncpu());
		T_LOG("📈 kern.sched_recommended_cores: %016llx, expecting %016llx if all are recommended",
		    recommended_cores, expected_recommended_mask);
	}

	if (core_mask != NULL) {
		*core_mask = recommended_cores;
	}
	return __builtin_popcountll(recommended_cores) == dt_ncpu();
}

/* Trace Management */

enum trace_status {
	STARTED = 1,
	ENDED = 2,
	SAVED = 3,
	DISCARDED = 4,
};

struct trace_handle {
	char *short_name;
	char *trace_filename;
	char *abs_filename;
	pid_t trace_pid;
	enum trace_status status;
	pid_t wait_on_start_pid;
	pid_t wait_on_end_pid;
};

#define MAX_TRACES 1024
static struct trace_handle handles[MAX_TRACES];
static int handle_ind = 0;

static void
atend_trace_cleanup(void)
{
	int ret;
	for (int i = 0; i < handle_ind; i++) {
		struct trace_handle *handle = &handles[i];
		if (handle->status == STARTED) {
			end_collect_trace(handle);
		}
		T_QUIET; T_EXPECT_EQ(handle->status, ENDED, "ended trace");
		if (handle->status == ENDED && T_FAILCOUNT > 0) {
			/* Save the trace as an artifact for debugging the failure(s) */
			save_collected_trace(handle);
		}
		/* Make sure to free up the tmp dir space we used */
		discard_collected_trace(handle);
		/* Kill trace just in case */
		ret = kill(handle->trace_pid, SIGKILL);
		T_QUIET; T_WITH_ERRNO; T_EXPECT_POSIX_SUCCESS(ret, "kill SIGKILL");
	}
}

static bool
sched_utils_tracing_supported(void)
{
#if TARGET_OS_BRIDGE
	/*
	 * Don't support the tracing on BridgeOS due to limited disk space
	 * and CLPC compatibility issues.
	 */
	return false;
#else /* !TARGET_OS_BRIDGE */
	disable_verbose_sched_utils();
	/* Virtual machines do not support trace */
	bool supported = (platform_is_virtual_machine() == false);
	reenable_verbose_sched_utils();
	return supported;
#endif /* !TARGET_OS_BRIDGE */
}

trace_handle_t
begin_collect_trace(char *filename)
{
	return begin_collect_trace_fmt(filename);
}
static bool first_trace = true;

static char *trace_bin = "/usr/local/bin/trace";
static char *notifyutil_bin = "/usr/bin/notifyutil";
static char *tar_bin = "/usr/bin/tar";

static char *begin_notification = "🖊️_trace_begun...";
static char *end_notification = "🖊️_trace_ended...";
static char *trigger_end_notification = "🖊️_stopping_trace...";

static const int waiting_timeout_sec = 60 * 2; /* 2 minutes, allows trace post-processing to finish */

trace_handle_t
begin_collect_trace_fmt(char *fmt, ...)
{
	/* Check trace requirements */
	if (sched_utils_tracing_supported() == false) {
		return NULL;
	}
	T_QUIET; T_ASSERT_EQ(geteuid(), 0, "🖊️ Tracing requires the test to be run as root user");

	int ret;
	struct trace_handle *handle = &handles[handle_ind++];
	T_QUIET; T_ASSERT_LE(handle_ind, MAX_TRACES, "Ran out of trace handles");

	/* Generate the trace filename from the formatted string and args */
	char *name = (char *)malloc(sizeof(char) * MAXPATHLEN);
	va_list args;
	va_start(args, fmt);
	vsnprintf(name, MAXPATHLEN, fmt, args);
	va_end(args);
	handle->short_name = name;
	char *full_filename = (char *)malloc(sizeof(char) * MAXPATHLEN);
	memset(full_filename, 0, MAXPATHLEN);
	snprintf(full_filename, MAXPATHLEN, "%s/%s.atrc", dt_tmpdir(), handle->short_name);
	handle->abs_filename = full_filename;
	char *filename = (char *)malloc(sizeof(char) * MAXPATHLEN);
	memset(filename, 0, MAXPATHLEN);
	snprintf(filename, MAXPATHLEN, "%s.atrc", handle->short_name);
	handle->trace_filename = filename;

	/* If filename already exists, delete old trace */
	ret = remove(handle->abs_filename);
	T_QUIET; T_WITH_ERRNO; T_EXPECT_TRUE(ret == 0 || errno == ENOENT, "remove trace file");

	if (first_trace) {
		/* Run tracing cleanup a single time */
		register_atend_handler();
		T_ATEND(atend_trace_cleanup);
		first_trace = false;
	}

	/* Launch procs to monitor trace start/stop */
	char *wait_on_start_args[] = {"/usr/bin/notifyutil", "-1", begin_notification, NULL};
	ret = dt_launch_tool(&handle->wait_on_start_pid, wait_on_start_args, false, NULL, NULL);
	T_QUIET; T_WITH_ERRNO; T_EXPECT_EQ(ret, 0, "dt_launch_tool");
	char *wait_on_end_args[] = {"/usr/bin/notifyutil", "-1", end_notification, NULL};
	ret = dt_launch_tool(&handle->wait_on_end_pid, wait_on_end_args, false, NULL, NULL);
	T_QUIET; T_WITH_ERRNO; T_EXPECT_EQ(ret, 0, "dt_launch_tool");

	/* Launch trace record */
	char *trace_args[] = {trace_bin, "record", handle->abs_filename, "--plan", "default", "--unsafe",
		              "--kdebug-filter-include", "C0x01", "--notify-after-start", begin_notification,
		              "--notify-after-end", end_notification, "--end-on-notification", trigger_end_notification, "&", NULL};
	pid_t trace_pid = dt_launch_tool_pipe(trace_args, false, NULL,
	    ^bool (char *data, __unused size_t data_size, __unused dt_pipe_data_handler_context_t *context) {
		T_LOG("🖊️ [trace] %s", data);
		return true;
	},
	    ^bool (char *data, __unused size_t data_size, __unused dt_pipe_data_handler_context_t *context) {
		T_LOG("🖊️ [trace] Error msg: %s", data);
		return true;
	},
	    BUFFER_PATTERN_NONE, NULL);

	T_LOG("🖊️ Starting trace collection for \"%s\" trace[%u]", handle->trace_filename, trace_pid);

	/* Wait for tracing to start */
	int signal_num;
	ret = dt_waitpid(handle->wait_on_start_pid, NULL, &signal_num, waiting_timeout_sec);
	T_QUIET; T_EXPECT_TRUE(ret, "dt_waitpid for trace start signal_num %d", signal_num);

	handle->trace_pid = trace_pid;
	handle->status = STARTED;

	return (trace_handle_t)handle;
}

void
end_collect_trace(trace_handle_t handle)
{
	if (sched_utils_tracing_supported() == false) {
		return;
	}

	int ret;
	struct trace_handle *trace_state = (struct trace_handle *)handle;
	T_QUIET; T_EXPECT_EQ(trace_state->status, STARTED, "trace was started");

	/* Notify trace to stop tracing */
	char *wait_on_start_args[] = {notifyutil_bin, "-p", trigger_end_notification, NULL};
	pid_t trigger_end_pid = 0;
	ret = dt_launch_tool(&trigger_end_pid, wait_on_start_args, false, NULL, NULL);
	T_QUIET; T_WITH_ERRNO; T_EXPECT_EQ(ret, 0, "dt_launch_tool for notify end trace");

	/* Wait for tracing to actually stop */
	T_LOG("🖊️ Now waiting on trace to finish up...");
	int signal_num;
	ret = dt_waitpid(trace_state->wait_on_end_pid, NULL, &signal_num, waiting_timeout_sec);
	T_QUIET; T_EXPECT_TRUE(ret, "dt_waitpid for trace stop signal_num %d", signal_num);

	trace_state->status = ENDED;
}

void
save_collected_trace(trace_handle_t handle)
{
	if (sched_utils_tracing_supported() == false) {
		return;
	}

	int ret;
	struct trace_handle *trace_state = (struct trace_handle *)handle;
	T_QUIET; T_EXPECT_EQ(trace_state->status, ENDED, "trace was ended");

	/* Generate compressed filepath and mark for upload */
	char compressed_path[MAXPATHLEN];
	snprintf(compressed_path, MAXPATHLEN, "%s.tar.gz", trace_state->short_name);
	ret = dt_resultfile(compressed_path, MAXPATHLEN);
	T_QUIET; T_WITH_ERRNO; T_EXPECT_POSIX_ZERO(ret, "dt_resultfile marking \"%s\" for collection", compressed_path);
	T_LOG("🖊️ \"%s\" marked for upload", compressed_path);

	char *tar_args[] = {tar_bin, "-czvf", compressed_path, "-C", (char *)dt_tmpdir(), trace_state->trace_filename, NULL};
	pid_t tar_pid = dt_launch_tool_pipe(tar_args, false, NULL,
	    ^bool (__unused char *data, __unused size_t data_size, __unused dt_pipe_data_handler_context_t *context) {
		T_LOG("🖊️ [tar] %s", data);
		return true;
	},
	    ^bool (char *data, __unused size_t data_size, __unused dt_pipe_data_handler_context_t *context) {
		T_LOG("🖊️ [tar] Error msg: %s", data);
		return true;
	},
	    BUFFER_PATTERN_LINE, NULL);

	T_QUIET; T_EXPECT_TRUE(tar_pid, "🖊️ [tar] pid %d", tar_pid);
	ret = dt_waitpid(tar_pid, NULL, NULL, 0);
	T_QUIET; T_EXPECT_POSIX_SUCCESS(ret, "dt_waitpid for tar");

	/* Lax permissions in case a user wants to open the compressed file without sudo */
	ret = chmod(compressed_path, 0666);
	T_QUIET; T_WITH_ERRNO; T_EXPECT_POSIX_ZERO(ret, "chmod");

	T_LOG("🖊️ Finished saving trace (%s), which is available compressed at \"%s\"",
	    trace_state->short_name, compressed_path);

	trace_state->status = SAVED;
}

void
discard_collected_trace(trace_handle_t handle)
{
	if (sched_utils_tracing_supported() == false) {
		return;
	}

	int ret;
	struct trace_handle *trace_state = (struct trace_handle *)handle;
	T_QUIET; T_EXPECT_TRUE(trace_state->status == ENDED || trace_state->status == SAVED,
	    "trace was ended or saved");

	/* Delete trace file in order to reclaim disk space on the test device */
	ret = remove(trace_state->abs_filename);
	T_QUIET; T_WITH_ERRNO; T_EXPECT_POSIX_SUCCESS(ret, "remove trace file");

	if (trace_state->status == ENDED) {
		T_LOG("🖊️ Deleted recorded trace file at \"%s\"", trace_state->abs_filename);
	}
	trace_state->status = DISCARDED;
}
