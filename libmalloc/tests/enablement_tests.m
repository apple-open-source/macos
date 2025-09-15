//
// enablement_tests.m
// libmalloc
//
// Tests for the xzone malloc json printer and other secure allocator enablement configurations
//

#include <darwintest.h>
#include <darwintest_utils.h>
#include <../src/internal.h>

#if CONFIG_XZONE_MALLOC && (MALLOC_TARGET_IOS_ONLY || TARGET_OS_OSX || TARGET_OS_VISION)

#include <Foundation/Foundation.h>
#include <Foundation/NSJSONSerialization.h>

#define PID_BUFFER_SIZE 256
#define NUM_PIDS_BUFFER_SIZE (512 * sizeof (pid_t))

#define ALLOCATION_FRONT_EXTRA 1

static char *print_buffer = NULL;
static size_t print_buffer_capacity = 0;
static size_t print_buffer_index = 0;

typedef struct enablement_configuration {
	bool should_have_xzones;
	bool guards_enabled;
	bool thread_cache_enabled;
	unsigned batch_size;
	unsigned ptr_bucket_count;
	unsigned segment_group_ids_count;
	unsigned segment_group_count;
	bool defer_tiny;
	bool defer_small;
	bool defer_large;
} enablement_configuration;

static uint8_t
get_ncpuclusters(void)
{
	return *(uint8_t *)(uintptr_t)_COMM_PAGE_CPU_CLUSTERS;
}

static void
reset_print_buffer(void)
{
	T_ASSERT_NULL(print_buffer, "reset_print_buffer called multiple times");
	print_buffer_index = 0;
	print_buffer_capacity = vm_page_size;
	print_buffer = malloc(print_buffer_capacity);
	T_ASSERT_NOTNULL(print_buffer, "Allocate print buffer");
}

static void
resize_print_buffer(void)
{
	T_ASSERT_NOTNULL(print_buffer, "Must call reset_print_buffer first");
	vm_address_t addr = 0;
	size_t new_capacity = print_buffer_capacity * 2;
	print_buffer = realloc(print_buffer, new_capacity);
	T_ASSERT_NOTNULL(print_buffer, "Realloc print buffer");
	print_buffer_capacity = new_capacity;
}

static void
append_to_buffer(const char *data, size_t len)
{
	while (1) {
		if (len >= print_buffer_capacity - print_buffer_index) {
			resize_print_buffer();
		} else {
			memcpy(&print_buffer[print_buffer_index], data, len);
			print_buffer_index += len;
			break;
		}
	}
}

static void
free_print_buffer(void)
{
	free(print_buffer);
	print_buffer = NULL;
	print_buffer_capacity = 0;
	print_buffer_index = 0;
}

// Helper searches all running pids and returns the first instance that
// matches a process name (searchname). Heavily inspired from:
// https://stashweb.sd.apple.com/projects/COREOS/repos/perfcheck/browse/lib/utils.c#31
pid_t
find_first_pid_for_process(const char *searchname)
{
	pid_t pid = 0;
	int i, buffer_size;
	int pid_buffer_size = NUM_PIDS_BUFFER_SIZE;
	pid_t *pids = NULL;
	bool finished = false;

	do {
		pids = reallocf(pids, pid_buffer_size);
		if (!pids) return 0;

		buffer_size = proc_listpids(PROC_ALL_PIDS, 0, pids, pid_buffer_size);
		if (buffer_size < pid_buffer_size) {
			finished = true;
		} else {
			pid_buffer_size = buffer_size * 2;
		}
	} while (!finished);

	for (i = 0; i < buffer_size / sizeof (pid_t); i++) {
		struct proc_bsdinfo proc;
		int st = proc_pidinfo(pids[i], PROC_PIDTBSDINFO, 0,
				&proc, PROC_PIDTBSDINFO_SIZE);
		if (st == PROC_PIDTBSDINFO_SIZE) {
			// If a matching process is found, ensure it's not pid 0
			if (strcmp(searchname, proc.pbi_name) == 0) {
				pid = pids[i];
				free(pids);
				return pid;
			}
		}
	}

	free(pids);
	return pid;
}

// Calls "heap -p *procname" and returns an array of json objects, one each for
// the xzm zones in the remote process. If the process doesn't use xzm, the
// array will be empty. If the process doesn't exist, this function either calls
// T_FAIL or T_SKIP, based on skip_if_not_found. If multiple processes with name
// procname exist, this function will choose to examine the one with the
// lowest pid.
static NSArray *
get_process_json(char *procname, bool skip_if_not_found)
{
	reset_print_buffer();

	__block bool not_found = false;

	// Dump heap's stderr to our stdout, for help debugging test failures. Also
	// monitor for a magic string indicating that the requested process doesn't
	// exist, to handle skip_if_not_found
	dt_pipe_data_handler_t stderr_handler = ^bool(char *data,
			__unused size_t data_size,
			__unused dt_pipe_data_handler_context_t *context) {
		T_LOG("heap stderr: %s", data);
		const char *not_found_needle = "heap cannot find any existing process";
		if (strstr(data, not_found_needle)) {
			not_found = true;
			T_LOG("Process does not exist");
			return true;
		}
		return false;
	};
	// returning true will stop executing handler.
	dt_pipe_data_handler_t stdout_handler =
			^bool(char *data, __unused size_t data_size,
			__unused dt_pipe_data_handler_context_t *context) {
		T_LOG("heap output: %s", data);
		append_to_buffer(data, data_size);
		return false;
	};

	// Unfortunately, calling heap on an app right after it is launched may
	// error with:
	// "Process exists but has not fully started -- dyld has initialized but
	// libSystem has not"
	// Use a polling approach: retry the heap command up to 3 times, until we
	// succeed
	bool wait = true;
	int exit_status = 1;
	int signal = 1;

	for (int i = 0; i < 3; ++i) {
		// If there are multiple instances of a process running, make
		// heap inspect only one of them
		pid_t process_pid = find_first_pid_for_process(procname);
		if (process_pid == 0) {
			not_found = true;
		}

		char process_buffer[PID_BUFFER_SIZE] = {};
		snprintf(process_buffer, PID_BUFFER_SIZE, "%d", process_pid);
		char *argv[] = { "/usr/bin/heap", "-p", process_buffer, NULL };
		pid_t pid = dt_launch_tool_pipe(argv, false, NULL, stdout_handler,
				stderr_handler, BUFFER_PATTERN_LINE, NULL);

		int timeout = 30; // 30 second timeout
		wait = dt_waitpid(pid, &exit_status, &signal, timeout);

		if (!wait && skip_if_not_found && not_found) {
			// Failed exit - should only occur when the tools couldn't find a
			// process by name
			T_SKIP("Skipping since %s doesn't exist", procname);
		}

		if (wait) {
			break;
		} else {
			// heap didn't succeed. Wait a tiny bit, then retry
			sleep(1);
		}
	}
	T_ASSERT_TRUE(wait, "heap exited successfully, status = %d, signal = %d",
			exit_status, signal);
	T_ASSERT_POSIX_ZERO(exit_status, "Exit status is success");
	T_ASSERT_POSIX_ZERO(signal, "Exit signal is success");

	// We don't have a great way to know that we've processed all of heap's
	// stderr/stdout, so use a fixed sleep to (hopefully) let those finish
	sleep(1);

	NSMutableArray *retval = [NSMutableArray arrayWithCapacity:1];
	char *heap_output = &print_buffer[0];
	size_t heap_len = print_buffer_index;
	while (heap_output) {
		const char *start_symbol = "Begin xzone malloc JSON:\n";
		const char *end_symbol = "End xzone malloc JSON\n";

		char *json_start = strnstr(heap_output, start_symbol, heap_len);
		if (!json_start) {
			// No more json to parse
			break;
		}
		json_start += strlen(start_symbol);
		char *json_end = strnstr(heap_output, end_symbol, heap_len);
		T_ASSERT_GE(json_end, json_start, "Incorrect end token");

		NSData *json_data = [NSData dataWithBytes:json_start
				length:(json_end - json_start)];

		NSError *e = nil;
		NSDictionary *json_dict =
				[NSJSONSerialization JSONObjectWithData:json_data options:0
				error:&e];
		T_ASSERT_NE(json_dict, nil, "Parsed json, error = %s",
				[[e localizedDescription] UTF8String]);

		[retval addObject:json_dict];

		char *new_heap_output = json_end + strlen(end_symbol);
		heap_len -= new_heap_output - heap_output;
		heap_output = new_heap_output;
	}

	free_print_buffer();

	return retval;
}

static char *
get_device_name(char *devicename, size_t buflen)
{
	// Key off of hardware model instead of the comm page, since libmalloc
	// keys off of the commpage, and this protects us against bugs in that
	// path
	int kr = sysctlbyname("hw.targettype", devicename, &buflen, NULL, 0);
	T_EXPECT_EQ(KERN_SUCCESS, kr, "Got target-type");
	T_ASSERT_GT(buflen, 0ul, "Len (%z) > 0", buflen);
	return devicename;
}

static bool
get_device_should_defer_large(void)
{
	bool should_defer_large = false;

#if MALLOC_TARGET_IOS_ONLY
	char device_name[16] = { 0 };
	get_device_name(device_name, sizeof(device_name) - 1);

	T_LOG("Device name: %s", device_name);

	if (!strcmp(device_name, "J420") || !strcmp(device_name, "J421")) {
		return false;
	}

	// If an iOS device has >=6GB of memory, the enablement configuration
	// should have defer_large set to "true".  Note that here we check for >=
	// 5GB because in practice various carve-outs reduce the actual size of
	// various 6GB devices to a bit below that quantity.  There are no devices
	// with >= 5GB of memory on iOS that aren't actually 6GB devices in the
	// sense we mean here.
	uint64_t memsize = platform_hw_memsize();
	const uint64_t defer_large_ios_bytes_memsize = 5 * 1073741824ULL;
	T_LOG("Device memsize: %"PRIu64", defer_large_ios_bytes_memsize: %"PRIu64"",
			memsize, defer_large_ios_bytes_memsize);
	if (memsize >= defer_large_ios_bytes_memsize) {
		should_defer_large = true;
	}
#elif TARGET_OS_OSX
	should_defer_large = true;
#endif

	return should_defer_large;
}

static void
enablement_configuration_process_checks(NSArray *json_array,
		enablement_configuration *configuration)
{
	// The secure allocator should not be enabled for any process on intel
	// machines
#if TARGET_OS_OSX && !TARGET_CPU_ARM64
	T_EXPECT_EQ(json_array.count, 0ul, "No zones should be present");
	return;
#endif

	// Verify if the secure allocator is enabled for that process
	if (configuration->should_have_xzones) {
		T_ASSERT_GE(json_array.count, 1ul, "At least one zone is xzm");
	} else {
		T_EXPECT_EQ(json_array.count, 0ul, "No zones should be present");
		return;
	}

	// Verify guard configuration
	NSDictionary *guard_config = json_array[0][@"guard_config"];
	T_ASSERT_NE(guard_config, nil, "Guard config dictionary in output");
	T_EXPECT_EQ([guard_config[@"guards_enabled"] boolValue],
			(int)configuration->guards_enabled,
			"Guard configuration");

	// Verify thread caching configuration
	T_EXPECT_EQ([json_array[0][@"thread_cache_enabled"] boolValue],
			(int)configuration->thread_cache_enabled, "Thread caching configuration");

	// Verify batching configuration
	T_EXPECT_EQ([json_array[0][@"batch_size"] intValue],
			configuration->batch_size, "Batching configuration");

	// Verify number of pointer buckets
	T_EXPECT_EQ([json_array[0][@"ptr_bucket_count"] intValue],
			configuration->ptr_bucket_count, "Expected number of pointer buckets");

	// Verify segment group configuration
	T_EXPECT_EQ([json_array[0][@"segment_group_ids_count"] intValue],
			configuration->segment_group_ids_count, "Expected number of segment group ids");
	T_EXPECT_EQ([json_array[0][@"segment_group_count"] intValue],
		configuration->segment_group_count, "Expected number of segment groups");

	// Verify deferred reclaim configuration
	T_EXPECT_EQ([json_array[0][@"defer_tiny"] intValue],
			configuration->defer_tiny,
			"Deferred reclaim (tiny) configuration");
	T_EXPECT_EQ([json_array[0][@"defer_small"] intValue],
			configuration->defer_small,
			"Deferred reclaim (small) configuration");
	T_EXPECT_EQ([json_array[0][@"defer_large"] intValue],
			configuration->defer_large,
			"Deferred reclaim (large) configuration");
}

static pid_t
spawn_process(char *new_argv[], char *new_envp[])
{
	pid_t child_pid = 0;
	errno_t ret = posix_spawn(&child_pid, new_argv[0], NULL, NULL,
			new_argv, new_envp);
	T_ASSERT_POSIX_ZERO(ret, "posix_spawn(%s)", new_argv[0]);
	T_ASSERT_NE(child_pid, 0, "posix_spawn(%s)", new_argv[0]);

	int status;
	// We expect the newly created process to run indefinitely.
	// Assert that it started, and if so, proceed with the test.
	int pid = waitpid(child_pid, &status, WNOHANG);
	T_ASSERT_POSIX_ZERO(pid, "waitpid call is successful");
	return child_pid;
}

static void
security_critical_configuration_checks_with_space_efficiency(
		const char *process, bool space_efficient)
{
	struct enablement_configuration configuration = (enablement_configuration) {
		.should_have_xzones = true,
		.guards_enabled = true,
#if TARGET_OS_VISION
		.batch_size = 0,
		.ptr_bucket_count = 4,
#elif TARGET_OS_OSX
		.batch_size = space_efficient ? 0 : 10,
		.ptr_bucket_count = 4,
#else
		.batch_size = 0,
		.ptr_bucket_count = 3,
#endif
		.segment_group_ids_count = XZM_SEGMENT_GROUP_IDS_COUNT,
		.thread_cache_enabled = !space_efficient,
#if TARGET_OS_OSX
		.segment_group_count = (space_efficient ? 1 : get_ncpuclusters()) *
				(XZM_SEGMENT_GROUP_IDS_COUNT + ALLOCATION_FRONT_EXTRA),
		.defer_tiny = !space_efficient,
		.defer_small = !space_efficient,
#else
		.segment_group_count = 1 * (XZM_SEGMENT_GROUP_IDS_COUNT +
				ALLOCATION_FRONT_EXTRA),
		.defer_tiny = false,
		.defer_small = false,
#endif // TARGET_OS_OSX
		.defer_large = (!space_efficient && get_device_should_defer_large()),
	 };

	enablement_configuration_process_checks(get_process_json(process, false),
			&configuration);
}

static void
security_critical_configuration_checks(const char *process)
{
	struct enablement_configuration configuration = (enablement_configuration) {
		.should_have_xzones = true,
		.guards_enabled = true,
		.thread_cache_enabled = false,
		.batch_size = 0,
#if TARGET_OS_OSX || TARGET_OS_VISION
		.ptr_bucket_count = 4,
#else
		.ptr_bucket_count = 3,
#endif
		.segment_group_ids_count = XZM_SEGMENT_GROUP_IDS_COUNT,
		.segment_group_count = 1 * (XZM_SEGMENT_GROUP_IDS_COUNT +
				ALLOCATION_FRONT_EXTRA),
		.defer_tiny = false,
		.defer_small = false,
		.defer_large = false,
	 };

	enablement_configuration_process_checks(get_process_json(process, false),
			&configuration);
}

void terminate_process(pid_t pid) {
	char terminate_process_buffer[PID_BUFFER_SIZE] = {};
	snprintf(terminate_process_buffer, PID_BUFFER_SIZE, "kill -9 %d", pid);
	T_ASSERT_POSIX_ZERO(system(terminate_process_buffer), "terminated process");
}

T_DECL(xzone_enabled_launchd,
		"Verify enablement configuration for security critical processes"
		"(launchd)",
		T_META_TAG_VM_NOT_ELIGIBLE,
		T_META_ASROOT(true))
{
	security_critical_configuration_checks("launchd.development");
}

T_DECL(xzone_enabled_logd,
		"Verify enablement configuration for security critical processes"
		"(logd)",
		T_META_TAG_VM_NOT_ELIGIBLE,
		T_META_ASROOT(true))
{
	security_critical_configuration_checks("logd");
}

T_DECL(xzone_enabled_notifyd,
		"Verify enablement configuration for security critical processes" "(notifyd)",
		T_META_TAG_VM_NOT_ELIGIBLE,
		T_META_ASROOT(true))
{
	security_critical_configuration_checks("notifyd");
}

// This test needs to be run as the local (non-root) user on macOS in order to
// successfully launch Safari
T_DECL(xzone_enabled_safari,
		"Verify enablement configuration for security critical processes"
		"(Safari)",
#if TARGET_OS_OSX
		T_META_REQUIRES_NETWORK(true),
#endif
		T_META_TAG_VM_NOT_ELIGIBLE)
{
#if TARGET_OS_OSX
	// Launch the Safari process on macOS
	char *launch_safari_args[] = {"/usr/bin/open", "-a", "Safari",
	"http://apple.com", NULL};
	pid_t safari_pid = spawn_process(launch_safari_args, NULL);
	security_critical_configuration_checks("Safari");
#else
#if MALLOC_TARGET_IOS_ONLY
	// Move past home screen to launch app in foreground
	T_ASSERT_POSIX_ZERO(system("LaunchApp -unlock com.apple.springboard"), "open homescreen");
#endif
	// Launch the MobileSafari app
	T_ASSERT_POSIX_ZERO(system("xctitool launch com.apple.mobilesafari"), "launch MobileSafari");

	// We'd like to verify that Safari, along with its related subprocesses,
	// are running with the secure critical process configuration
	security_critical_configuration_checks("MobileSafari");
#endif
	security_critical_configuration_checks("com.apple.WebKit.Networking");
	security_critical_configuration_checks("com.apple.WebKit.GPU");
	security_critical_configuration_checks("com.apple.WebKit.WebContent");

#if TARGET_OS_OSX
	terminate_process(safari_pid);
#endif
}

T_DECL(xzone_enabled_driverkit,
		"Verify enablement configuration for Driverkit processes",
		T_META_TAG_VM_NOT_ELIGIBLE,
		T_META_ASROOT(true))
{
	// DriverKit processes are usually started at boot. However, on devboards
	// for J8XX, which miss many parts, the command "ps -u _driverkit" shows no
	// processes running by default. Thus, run a simple test to start the
	// com.apple.AppleUserHIDDriver process for analysis.
	char *spawn_driverkit_proc_args[] = {"/usr/local/bin/hidUserDeviceTest", "hidUserDeviceTest", "-k", NULL};
	pid_t driver_test_pid = spawn_process(spawn_driverkit_proc_args, NULL);

	// The above action would have started the DriverKit process with the
	// label: com.apple.AppleUserHIDDriver
	struct enablement_configuration configuration = (enablement_configuration) {
		.should_have_xzones = true,
		.guards_enabled = false,
		.thread_cache_enabled = false,
		.batch_size = 0,
#if TARGET_OS_OSX || TARGET_OS_VISION
		.ptr_bucket_count = 4,
#else
		.ptr_bucket_count = 3,
#endif
		.segment_group_ids_count = XZM_SEGMENT_GROUP_IDS_COUNT,
		.segment_group_count = 1 * (XZM_SEGMENT_GROUP_IDS_COUNT +
				ALLOCATION_FRONT_EXTRA),
		.defer_tiny = false,
		.defer_small = false,
		.defer_large = false,
	 };

	enablement_configuration_process_checks(
		get_process_json("com.apple.AppleUserHIDDrivers", false),
		&configuration
		);
	// Cleanup the process so that it doesn't linger undesirably
	terminate_process(driver_test_pid);
}

T_DECL(xzone_enabled_general_process_test_runner,
		"Verify enablement configuration for general processes (the test" "process itself)",
		T_META_TAG_VM_NOT_ELIGIBLE,
		T_META_TAG_NO_ALLOCATOR_OVERRIDE,
		T_META_ASROOT(true))
{
	struct enablement_configuration configuration = (enablement_configuration) {
		.should_have_xzones = true,
		.guards_enabled = false,
#if TARGET_OS_OSX
		.thread_cache_enabled = true,
		.batch_size = 10,
#else
		.thread_cache_enabled = false,
		.batch_size = 0,
#endif
#if TARGET_OS_OSX || TARGET_OS_VISION
		.ptr_bucket_count = 4,
#else
		.ptr_bucket_count = 2,
#endif
#if TARGET_OS_OSX
		.segment_group_ids_count = XZM_SEGMENT_GROUP_IDS_COUNT,
		.segment_group_count = get_ncpuclusters() *
				(XZM_SEGMENT_GROUP_IDS_COUNT + ALLOCATION_FRONT_EXTRA),
		.defer_tiny = true,
		.defer_small = true,
		.defer_large = true,
#else
		.segment_group_ids_count = XZM_SEGMENT_GROUP_IDS_COUNT,
		.segment_group_count = 1 * (XZM_SEGMENT_GROUP_IDS_COUNT +
				ALLOCATION_FRONT_EXTRA),
		.defer_tiny = false,
		.defer_small = false,
		.defer_large = false,
#endif
	 };

	enablement_configuration_process_checks(
		get_process_json("enablement_tests", false),
		&configuration
		);
}

T_DECL(xzone_enabled_general_daemon,
		"Verify enablement configuration for general daemon (watchdogd)",
		T_META_TAG_VM_NOT_ELIGIBLE,
		T_META_ASROOT(true))
{
	struct enablement_configuration configuration = (enablement_configuration) {
		.should_have_xzones = true,
		.guards_enabled = false,
		.thread_cache_enabled = false,
		.batch_size = 0,
#if TARGET_OS_VISION || TARGET_OS_OSX
		.ptr_bucket_count = 4,
#else
		.ptr_bucket_count = 2,
#endif
		.segment_group_ids_count = XZM_SEGMENT_GROUP_IDS_COUNT,
		.segment_group_count = 1 * (XZM_SEGMENT_GROUP_IDS_COUNT +
				ALLOCATION_FRONT_EXTRA),
		.defer_tiny = false,
		.defer_small = false,
		.defer_large = false,
	 };
	// The second process we'll examine in the general category is a daemon,
	// watchdogd
	enablement_configuration_process_checks(
		get_process_json("watchdogd", false),
		&configuration
	);
}

#if TARGET_OS_OSX
T_DECL(xzone_enabled_overridden_app,
		"Verify enablement configuration for an overridden app on MacOS (Messages)",
		T_META_TAG_VM_NOT_ELIGIBLE)
{
	struct enablement_configuration configuration = (enablement_configuration) {
		.should_have_xzones = true,
		.guards_enabled = false,
		.thread_cache_enabled = true,
		.batch_size = 10,
		.ptr_bucket_count = 4,
		.segment_group_ids_count = XZM_SEGMENT_GROUP_IDS_COUNT,
		.segment_group_count = get_ncpuclusters() *
				(XZM_SEGMENT_GROUP_IDS_COUNT + ALLOCATION_FRONT_EXTRA),
		.defer_tiny = true,
		.defer_small = true,
		.defer_large = true,
	 };

	// Launch the Messages app on macOS
	char *launch_notes_args[] = {"/System/Applications/Messages.app/Contents/MacOS/Messages", NULL};
	pid_t pid = spawn_process(launch_notes_args, NULL);

	// On macOS, we expect this to use xzone malloc
	enablement_configuration_process_checks(
		get_process_json("Messages", false),
		&configuration
	);
	// Cleanup the process so that it doesn't linger undesirably
	terminate_process(pid);
}
#endif // TARGET_OS_OSX

T_DECL(xzone_enabled_general_app,
		"Verify enablement configuration for a general app (Notes)",
		T_META_TAG_VM_NOT_ELIGIBLE)
{
	bool should_defer_large = get_device_should_defer_large();

	struct enablement_configuration configuration = (enablement_configuration) {
		.should_have_xzones = true,
		.guards_enabled = false,
		.thread_cache_enabled = true,
#if TARGET_OS_VISION
		.batch_size = 0,
		.ptr_bucket_count = 4,
#elif TARGET_OS_OSX
		.batch_size = 10,
		.ptr_bucket_count = 4,
#else
		.batch_size = 0,
		.ptr_bucket_count = 2,
#endif
		.segment_group_ids_count = XZM_SEGMENT_GROUP_IDS_COUNT,
#if TARGET_OS_OSX
		.segment_group_count = get_ncpuclusters() *
				(XZM_SEGMENT_GROUP_IDS_COUNT + ALLOCATION_FRONT_EXTRA),
		.defer_tiny = true,
		.defer_small = true,
		.defer_large = true,
#else
		.segment_group_count = 1 * (XZM_SEGMENT_GROUP_IDS_COUNT +
				ALLOCATION_FRONT_EXTRA),
		.defer_tiny = false,
		.defer_small = false,
		.defer_large = should_defer_large,
#endif // TARGET_OS_OSX
	 };

#if TARGET_OS_OSX
	// Launch the Notes app on macOS
	char *launch_notes_args[] = {"/System/Applications/Notes.app/Contents/MacOS/Notes", NULL};
	pid_t notes_pid = spawn_process(launch_notes_args, NULL);

	// On macOS, we expect a random app process to not use xzone malloc
	enablement_configuration_process_checks(
		get_process_json("Notes", false),
		&configuration
	);
	// Cleanup the process so that it doesn't linger undesirably
	terminate_process(notes_pid);
#else

#if MALLOC_TARGET_IOS_ONLY
	// Move past home screen to launch app in foreground
	T_ASSERT_POSIX_ZERO(system("LaunchApp -unlock com.apple.springboard"),
	"open homescreen");
#endif // MALLOC_TARGET_IOS_ONLY

	// Launch the MobileNotes app
	T_ASSERT_POSIX_ZERO(system("xctitool launch com.apple.mobilenotes"),
	"launch MobileNotes");
	enablement_configuration_process_checks(
		get_process_json("MobileNotes", false),
		&configuration
	);

#endif // TARGET_OS_OSX
}

T_DECL(xzone_enabled_hardened_heap_entitlement_space_efficient,
		"Verify enablement configuration for hardened-heap entitled process"
		" (SpaceEfficient configuration)",
		T_META_TAG_VM_NOT_ELIGIBLE)
{
	char *spawn_hardened_heap_args[] = {
		"/AppleInternal/Tests/libmalloc/assets/hardened_heap_test_tool",
		NULL,
	};

#if TARGET_OS_OSX
	// SpaceEfficient is not the default on macOS
	char *envp[] = {
		"MallocSpaceEfficient=1",
		NULL,
	};
#else
	// SpaceEfficient is the default on other platforms
	char **envp = NULL;
#endif

	pid_t pid = spawn_process(spawn_hardened_heap_args, envp);
	bool space_efficient = true;

	security_critical_configuration_checks_with_space_efficiency(
			"hardened_heap_test_tool", space_efficient);

	terminate_process(pid);
}

T_DECL(xzone_enabled_hardened_heap_entitlement_non_space_efficient,
		"Verify enablement configuration for hardened-heap entitled process"
		" (non-SpaceEfficient configuration)",
		T_META_TAG_VM_NOT_ELIGIBLE)
{
	char *spawn_hardened_heap_args[] = {
		"/AppleInternal/Tests/libmalloc/assets/hardened_heap_test_tool",
		NULL,
	};

#if TARGET_OS_OSX
	// Non-SpaceEfficient is the default on macOS
	char **envp = NULL;
#else
	// Non-SpaceEfficient requires nano envvar on other platforms, and
	// MallocLargeCache on iOS
	char *envp[] = {
		"MallocNanoZone=1",
#if MALLOC_TARGET_IOS_ONLY
		"MallocLargeCache=1",
#endif
		NULL,
	};
#endif

	pid_t pid = spawn_process(spawn_hardened_heap_args, envp);
	bool space_efficient = false;

	security_critical_configuration_checks_with_space_efficiency(
			"hardened_heap_test_tool", space_efficient);

	terminate_process(pid);
}

#if MALLOC_TARGET_IOS_ONLY

T_DECL(xzone_enabled_hardened_browser_entitlement,
		"Verify enablement configuration for hardened-browser entitled process",
		T_META_TAG_VM_NOT_ELIGIBLE)
{
	char *spawn_hardened_browser_args[] = {
		"/AppleInternal/Tests/libmalloc/assets/hardened_browser_test_tool",
		NULL,
	};

	pid_t pid = spawn_process(spawn_hardened_browser_args, NULL);

	// The hardened-browser configuration is always SpaceEfficient
	bool space_efficient = true;
	security_critical_configuration_checks_with_space_efficiency(
			"hardened_browser_test_tool", space_efficient);

	terminate_process(pid);
}

#endif

#else // CONFIG_XZONE_MALLOC && (MALLOC_TARGET_IOS_ONLY || TARGET_OS_OSX ||
// TARGET_OS_VISION)
T_DECL(skip_json_printer_tests, "Skip printer tests")
{
	T_SKIP("Nothing to test without xzone malloc on ios/macos/visionos");
}
#endif // CONFIG_XZONE_MALLOC && (MALLOC_TARGET_IOS_ONLY || TARGET_OS_OSX ||
// TARGET_OS_VISION)
