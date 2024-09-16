#include <stdio.h>
#include <signal.h>
#include <sys/proc.h>
#include <sys/sysctl.h>
#include <sys/kern_memorystatus.h>
#include <sys/kern_memorystatus_freeze.h>
#include <time.h>
#include <mach-o/dyld.h>
#include <mach/mach_vm.h>
#include <mach/vm_page_size.h>
#include <mach/shared_region.h>
#include <mach/mach.h>
#include <os/reason_private.h>
#include <TargetConditionals.h>

#ifdef T_NAMESPACE
#undef T_NAMESPACE
#endif
#include <darwintest.h>
#include <darwintest_utils.h>

#include "memorystatus_assertion_helpers.h"
#include "test_utils.h"

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.memorystatus"),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("VM - memory pressure"),
	T_META_CHECK_LEAKS(false),
	T_META_OWNER("jarrad"),
	T_META_RUN_CONCURRENTLY(false)
	);

#define MEM_SIZE_MB                     10
#define NUM_ITERATIONS          5
#define FREEZE_PAGES_MAX 256

#define CREATE_LIST(X) \
	X(SUCCESS) \
	X(TOO_FEW_ARGUMENTS) \
	X(SYSCTL_VM_PAGESIZE_FAILED) \
	X(VM_PAGESIZE_IS_ZERO) \
	X(DISPATCH_SOURCE_CREATE_FAILED) \
	X(INITIAL_SIGNAL_TO_PARENT_FAILED) \
	X(SIGNAL_TO_PARENT_FAILED) \
	X(MEMORYSTATUS_CONTROL_FAILED) \
	X(IS_FREEZABLE_NOT_AS_EXPECTED) \
	X(MEMSTAT_PRIORITY_CHANGE_FAILED) \
	X(INVALID_ALLOCATE_PAGES_ARGUMENTS) \
	X(FROZEN_BIT_SET) \
	X(FROZEN_BIT_NOT_SET) \
	X(MEMORYSTATUS_CONTROL_ERROR) \
	X(UNABLE_TO_ALLOCATE) \
	X(EXIT_CODE_MAX) \

#define EXIT_CODES_ENUM(VAR) VAR,
enum exit_codes_num {
	CREATE_LIST(EXIT_CODES_ENUM)
};

#define EXIT_CODES_STRING(VAR) #VAR,
static const char *exit_codes_str[] = {
	CREATE_LIST(EXIT_CODES_STRING)
};

static int
get_vmpage_size(void)
{
	int vmpage_size;
	size_t size = sizeof(vmpage_size);
	int ret = sysctlbyname("vm.pagesize", &vmpage_size, &size, NULL, 0);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "failed to query vm.pagesize");
	T_QUIET; T_ASSERT_GT(vmpage_size, 0, "vm.pagesize is not > 0");
	return vmpage_size;
}

static pid_t child_pid = -1;
static int freeze_count = 0;

void move_to_idle_band(pid_t);
void run_freezer_test(int);
void freeze_helper_process(void);
/* Gets and optionally sets the freeze pages max threshold */
int sysctl_freeze_pages_max(int* new_value);

/* NB: in_shared_region and get_rprvt are pulled from the memorystatus unit test.
 * We're moving away from those unit tests, so they're copied here.
 */

/* Cribbed from 'top'... */
static int
in_shared_region(mach_vm_address_t addr, cpu_type_t type)
{
	mach_vm_address_t base = 0, size = 0;

	switch (type) {
	case CPU_TYPE_ARM:
		base = SHARED_REGION_BASE_ARM;
		size = SHARED_REGION_SIZE_ARM;
		break;

	case CPU_TYPE_ARM64:
		base = SHARED_REGION_BASE_ARM64;
		size = SHARED_REGION_SIZE_ARM64;
		break;


	case CPU_TYPE_X86_64:
		base = SHARED_REGION_BASE_X86_64;
		size = SHARED_REGION_SIZE_X86_64;
		break;

	case CPU_TYPE_I386:
		base = SHARED_REGION_BASE_I386;
		size = SHARED_REGION_SIZE_I386;
		break;

	case CPU_TYPE_POWERPC:
		base = SHARED_REGION_BASE_PPC;
		size = SHARED_REGION_SIZE_PPC;
		break;

	case CPU_TYPE_POWERPC64:
		base = SHARED_REGION_BASE_PPC64;
		size = SHARED_REGION_SIZE_PPC64;
		break;

	default: {
		int t = type;

		fprintf(stderr, "unknown CPU type: 0x%x\n", t);
		abort();
	}
	}

	return addr >= base && addr < (base + size);
}

/* Get the resident private memory of the given pid */
static unsigned long long
get_rprvt(pid_t pid)
{
	mach_port_name_t task;
	kern_return_t kr;

	mach_vm_size_t rprvt = 0;
	mach_vm_size_t empty = 0;
	mach_vm_size_t fw_private = 0;
	mach_vm_size_t pagesize = vm_kernel_page_size;  // The vm_region page info is reported
	                                                // in terms of vm_kernel_page_size.
	mach_vm_size_t regs = 0;

	mach_vm_address_t addr;
	mach_vm_size_t size;

	int split = 0;

	kr = task_for_pid(mach_task_self(), pid, &task);
	T_QUIET; T_ASSERT_TRUE(kr == KERN_SUCCESS, "Unable to get task_for_pid of child");

	for (addr = 0;; addr += size) {
		vm_region_top_info_data_t info;
		mach_msg_type_number_t count = VM_REGION_TOP_INFO_COUNT;
		mach_port_t object_name;

		kr = mach_vm_region(task, &addr, &size, VM_REGION_TOP_INFO, (vm_region_info_t)&info, &count, &object_name);
		if (kr != KERN_SUCCESS) {
			break;
		}

#if   defined (__arm64__)
		if (in_shared_region(addr, CPU_TYPE_ARM64)) {
#else
		if (in_shared_region(addr, CPU_TYPE_ARM)) {
#endif
			// Private Shared
			fw_private += info.private_pages_resident * pagesize;

			/*
			 * Check if this process has the globally shared
			 * text and data regions mapped in.  If so, set
			 * split to TRUE and avoid checking
			 * again.
			 */
			if (split == FALSE && info.share_mode == SM_EMPTY) {
				vm_region_basic_info_data_64_t  b_info;
				mach_vm_address_t b_addr = addr;
				mach_vm_size_t b_size = size;
				count = VM_REGION_BASIC_INFO_COUNT_64;

				kr = mach_vm_region(task, &b_addr, &b_size, VM_REGION_BASIC_INFO_64, (vm_region_info_t)&b_info, &count, &object_name);
				if (kr != KERN_SUCCESS) {
					break;
				}

				if (b_info.reserved) {
					split = TRUE;
				}
			}

			/*
			 * Short circuit the loop if this isn't a shared
			 * private region, since that's the only region
			 * type we care about within the current address
			 * range.
			 */
			if (info.share_mode != SM_PRIVATE) {
				continue;
			}
		}

		regs++;

		/*
		 * Update counters according to the region type.
		 */

		if (info.share_mode == SM_COW && info.ref_count == 1) {
			// Treat single reference SM_COW as SM_PRIVATE
			info.share_mode = SM_PRIVATE;
		}

		switch (info.share_mode) {
		case SM_LARGE_PAGE:
		// Treat SM_LARGE_PAGE the same as SM_PRIVATE
		// since they are not shareable and are wired.
		case SM_PRIVATE:
			rprvt += info.private_pages_resident * pagesize;
			rprvt += info.shared_pages_resident * pagesize;
			break;

		case SM_EMPTY:
			empty += size;
			break;

		case SM_COW:
		case SM_SHARED:
			if (pid == 0) {
				// Treat kernel_task specially
				if (info.share_mode == SM_COW) {
					rprvt += info.private_pages_resident * pagesize;
				}
				break;
			}

			if (info.share_mode == SM_COW) {
				rprvt += info.private_pages_resident * pagesize;
			}
			break;

		default:
			assert(0);
			break;
		}
	}

	return rprvt;
}

void
move_to_idle_band(pid_t pid)
{
	memorystatus_priority_properties_t props;
	/*
	 * Freezing a process also moves it to an elevated jetsam band in order to protect it from idle exits.
	 * So we move the child process to the idle band to mirror the typical 'idle app being frozen' scenario.
	 */
	props.priority = JETSAM_PRIORITY_IDLE;
	props.user_data = 0;

	/*
	 * This requires us to run as root (in the absence of entitlement).
	 * Hence the T_META_ASROOT(true) in the T_HELPER_DECL.
	 */
	if (memorystatus_control(MEMORYSTATUS_CMD_SET_PRIORITY_PROPERTIES, pid, 0, &props, sizeof(props))) {
		exit(MEMSTAT_PRIORITY_CHANGE_FAILED);
	}
}

void
freeze_helper_process(void)
{
	size_t length;
	int ret, freeze_enabled, errno_freeze_sysctl;
	uint64_t resident_memory_before, resident_memory_after, vmpage_size;
	vmpage_size = (uint64_t) get_vmpage_size();
	resident_memory_before = get_rprvt(child_pid) / vmpage_size;

	T_LOG("Freezing child pid %d", child_pid);
	ret = sysctlbyname("kern.memorystatus_freeze", NULL, NULL, &child_pid, sizeof(child_pid));
	errno_freeze_sysctl = errno;
	sleep(1);

	/*
	 * The child process toggles its freezable state on each iteration.
	 * So a failure for every alternate freeze is expected.
	 */
	if (freeze_count % 2) {
		length = sizeof(freeze_enabled);
		T_QUIET; T_ASSERT_POSIX_SUCCESS(sysctlbyname("vm.freeze_enabled", &freeze_enabled, &length, NULL, 0),
		    "failed to query vm.freeze_enabled");
		if (freeze_enabled) {
			errno = errno_freeze_sysctl;
			T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "sysctl kern.memorystatus_freeze failed");
		} else {
			/* If freezer is disabled, skip the test. This can happen due to disk space shortage. */
			T_LOG("Freeze has been disabled. Terminating early.");
			T_END;
		}
		resident_memory_after = get_rprvt(child_pid) / vmpage_size;
		uint64_t freeze_pages_max = (uint64_t) sysctl_freeze_pages_max(NULL);
		T_QUIET; T_ASSERT_LT(resident_memory_after, resident_memory_before, "Freeze didn't reduce resident memory set");
		if (resident_memory_before > freeze_pages_max) {
			T_QUIET; T_ASSERT_LE(resident_memory_before - resident_memory_after, freeze_pages_max, "Freeze pages froze more than the threshold.");
		}
		ret = sysctlbyname("kern.memorystatus_thaw", NULL, NULL, &child_pid, sizeof(child_pid));
		T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "sysctl kern.memorystatus_thaw failed");
	} else {
		T_QUIET; T_ASSERT_TRUE(ret != KERN_SUCCESS, "Freeze should have failed");
		T_LOG("Freeze failed as expected");
	}

	freeze_count++;

	T_QUIET; T_ASSERT_POSIX_SUCCESS(kill(child_pid, SIGUSR1), "failed to send SIGUSR1 to child process");
}

void
run_freezer_test(int num_pages)
{
	int ret;
	char sz_str[50];
	char **launch_tool_args;
	char testpath[PATH_MAX];
	uint32_t testpath_buf_size;
	dispatch_source_t ds_freeze, ds_proc;

	signal(SIGUSR1, SIG_IGN);
	ds_freeze = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGUSR1, 0, dispatch_get_main_queue());
	T_QUIET; T_ASSERT_NOTNULL(ds_freeze, "dispatch_source_create (ds_freeze)");

	dispatch_source_set_event_handler(ds_freeze, ^{
		if (freeze_count < NUM_ITERATIONS) {
		        freeze_helper_process();
		} else {
		        kill(child_pid, SIGKILL);
		        dispatch_source_cancel(ds_freeze);
		}
	});
	dispatch_activate(ds_freeze);

	testpath_buf_size = sizeof(testpath);
	ret = _NSGetExecutablePath(testpath, &testpath_buf_size);
	T_QUIET; T_ASSERT_POSIX_ZERO(ret, "_NSGetExecutablePath");
	T_LOG("Executable path: %s", testpath);

	sprintf(sz_str, "%d", num_pages);
	launch_tool_args = (char *[]){
		testpath,
		"-n",
		"allocate_pages",
		"--",
		sz_str,
		NULL
	};

	/* Spawn the child process. Suspend after launch until the exit proc handler has been set up. */
	ret = dt_launch_tool(&child_pid, launch_tool_args, true, NULL, NULL);
	if (ret != 0) {
		T_LOG("dt_launch tool returned %d with error code %d", ret, errno);
	}
	T_QUIET; T_ASSERT_POSIX_SUCCESS(child_pid, "dt_launch_tool");

	ds_proc = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, (uintptr_t)child_pid, DISPATCH_PROC_EXIT, dispatch_get_main_queue());
	T_QUIET; T_ASSERT_NOTNULL(ds_proc, "dispatch_source_create (ds_proc)");

	dispatch_source_set_event_handler(ds_proc, ^{
		int status = 0, code = 0;
		pid_t rc = waitpid(child_pid, &status, 0);
		T_QUIET; T_ASSERT_EQ(rc, child_pid, "waitpid");
		code = WEXITSTATUS(status);

		if (code == 0) {
		        T_END;
		} else if (code > 0 && code < EXIT_CODE_MAX) {
		        T_ASSERT_FAIL("Child exited with %s", exit_codes_str[code]);
		} else {
		        T_ASSERT_FAIL("Child exited with unknown exit code %d", code);
		}
	});
	dispatch_activate(ds_proc);

	T_QUIET; T_ASSERT_POSIX_SUCCESS(kill(child_pid, SIGCONT), "failed to send SIGCONT to child process");
	dispatch_main();
}

static void
allocate_pages(int num_pages)
{
	int i, j, vmpgsize;
	char val;
	__block int num_iter = 0;
	__block char **buf;
	dispatch_source_t ds_signal;
	vmpgsize = get_vmpage_size();
	if (num_pages < 1) {
		printf("Invalid number of pages to allocate: %d\n", num_pages);
		exit(INVALID_ALLOCATE_PAGES_ARGUMENTS);
	}

	buf = (char**)malloc(sizeof(char*) * (size_t)num_pages);

	/* Gives us the compression ratio we see in the typical case (~2.7) */
	for (j = 0; j < num_pages; j++) {
		buf[j] = (char*)malloc((size_t)vmpgsize * sizeof(char));
		val = 0;
		for (i = 0; i < vmpgsize; i += 16) {
			memset(&buf[j][i], val, 16);
			if (i < 3400 * (vmpgsize / 4096)) {
				val++;
			}
		}
	}

	dispatch_after(dispatch_time(DISPATCH_TIME_NOW, NSEC_PER_SEC), dispatch_get_main_queue(), ^{
		/* Signal to the parent that we're done allocating and it's ok to freeze us */
		printf("[%d] Sending initial signal to parent to begin freezing\n", getpid());
		if (kill(getppid(), SIGUSR1) != 0) {
		        exit(INITIAL_SIGNAL_TO_PARENT_FAILED);
		}
	});

	signal(SIGUSR1, SIG_IGN);
	ds_signal = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGUSR1, 0, dispatch_get_main_queue());
	if (ds_signal == NULL) {
		exit(DISPATCH_SOURCE_CREATE_FAILED);
	}

	dispatch_source_set_event_handler(ds_signal, ^{
		int current_state, new_state;
		volatile int tmp;

		/* Make sure all the pages are accessed before trying to freeze again */
		for (int x = 0; x < num_pages; x++) {
		        tmp = buf[x][0];
		}

		current_state = memorystatus_control(MEMORYSTATUS_CMD_GET_PROCESS_IS_FREEZABLE, getpid(), 0, NULL, 0);
		/* Sysprocs start off as unfreezable. Verify that first. */
		if (num_iter == 0 && current_state != 0) {
		        exit(IS_FREEZABLE_NOT_AS_EXPECTED);
		}

		/* Toggle freezable state */
		new_state = (current_state) ? 0: 1;
		printf("[%d] Changing state from %s to %s\n", getpid(),
		(current_state) ? "freezable": "unfreezable", (new_state) ? "freezable": "unfreezable");
		if (memorystatus_control(MEMORYSTATUS_CMD_SET_PROCESS_IS_FREEZABLE, getpid(), (uint32_t)new_state, NULL, 0) != KERN_SUCCESS) {
		        exit(MEMORYSTATUS_CONTROL_FAILED);
		}

		/* Verify that the state has been set correctly */
		current_state = memorystatus_control(MEMORYSTATUS_CMD_GET_PROCESS_IS_FREEZABLE, getpid(), 0, NULL, 0);
		if (new_state != current_state) {
		        exit(IS_FREEZABLE_NOT_AS_EXPECTED);
		}
		num_iter++;

		if (kill(getppid(), SIGUSR1) != 0) {
		        exit(SIGNAL_TO_PARENT_FAILED);
		}
	});
	dispatch_activate(ds_signal);
	move_to_idle_band(getpid());

	dispatch_main();
}

T_HELPER_DECL(allocate_pages,
    "allocates pages to freeze",
    T_META_ASROOT(true)) {
	if (argc < 1) {
		exit(TOO_FEW_ARGUMENTS);
	}

	int num_pages = atoi(argv[0]);
	allocate_pages(num_pages);
}

T_DECL(memorystatus_freeze_default_state, "Test that the freezer is enabled or disabled as expected by default.", T_META_ASROOT(true), T_META_TAG_VM_NOT_PREFERRED) {

#if TARGET_OS_IOS || TARGET_OS_WATCH
	bool expected_freeze_enabled = true;
#else
	bool expected_freeze_enabled = false;
#endif


	int freeze_enabled;
	size_t length = sizeof(freeze_enabled);
	int ret = sysctlbyname("vm.freeze_enabled", &freeze_enabled, &length, NULL, 0);

	if (ret != 0) {
		if (expected_freeze_enabled) {
			T_ASSERT_FAIL("Expected freezer enabled but did not find sysctl vm.freeze_enabled");
		} else {
			T_PASS("Did not expect freezer to be enabled and did not find sysctl vm.freeze_enabled");
		}
	} else {
		T_ASSERT_EQ(freeze_enabled, expected_freeze_enabled, "Expected freezer sysctl status %d, got %d", expected_freeze_enabled, freeze_enabled);
	}
}

T_DECL(freeze, "VM freezer test",
	T_META_BOOTARGS_SET("freeze_enabled=1"),
	T_META_REQUIRES_SYSCTL_EQ("vm.freeze_enabled", 1),
	T_META_ASROOT(true),
	T_META_TAG_VM_NOT_PREFERRED) {
	run_freezer_test(
		(MEM_SIZE_MB << 20) / get_vmpage_size());
}

static int old_freeze_pages_max = 0;
static void
reset_freeze_pages_max(void)
{
	if (old_freeze_pages_max != 0) {
		sysctl_freeze_pages_max(&old_freeze_pages_max);
	}
}

int
sysctl_freeze_pages_max(int* new_value)
{
	static int set_end_handler = false;
	int freeze_pages_max, ret;
	size_t size = sizeof(freeze_pages_max);
	ret = sysctlbyname("kern.memorystatus_freeze_pages_max", &freeze_pages_max, &size, new_value, size);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "Unable to query kern.memorystatus_freeze_pages_max");
	if (!set_end_handler) {
		// Save the original value and instruct darwintest to restore it after the test completes
		old_freeze_pages_max = freeze_pages_max;
		T_ATEND(reset_freeze_pages_max);
		set_end_handler = true;
	}
	return old_freeze_pages_max;
}

T_DECL(freeze_over_max_threshold, "Max Freeze Threshold is Enforced",
	T_META_BOOTARGS_SET("freeze_enabled=1"),
	T_META_REQUIRES_SYSCTL_EQ("vm.freeze_enabled", 1),
	T_META_ASROOT(true),
	T_META_TAG_VM_NOT_PREFERRED) {
	int freeze_pages_max = FREEZE_PAGES_MAX;
	sysctl_freeze_pages_max(&freeze_pages_max);
	run_freezer_test(FREEZE_PAGES_MAX * 2);
}

T_HELPER_DECL(frozen_background, "Frozen background process",
	T_META_BOOTARGS_SET("freeze_enabled=1"),
	T_META_REQUIRES_SYSCTL_EQ("vm.freeze_enabled", 1),
	T_META_ASROOT(true)) {
	kern_return_t kern_ret;
	/* Set the process to freezable */
	kern_ret = memorystatus_control(MEMORYSTATUS_CMD_SET_PROCESS_IS_FREEZABLE, getpid(), 1, NULL, 0);
	T_QUIET; T_ASSERT_EQ(kern_ret, KERN_SUCCESS, "set process is freezable");
	/* Signal to our parent that we can be frozen */
	if (kill(getppid(), SIGUSR1) != 0) {
		T_LOG("Unable to signal to parent process!");
		exit(1);
	}
	while (1) {
		;
	}
}

static void
freeze_process(pid_t pid)
{
	int ret, freeze_enabled, errno_freeze_sysctl;
	size_t length;
	T_LOG("Freezing pid %d", pid);

	ret = sysctlbyname("kern.memorystatus_freeze", NULL, NULL, &pid, sizeof(pid));
	errno_freeze_sysctl = errno;
	length = sizeof(freeze_enabled);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(sysctlbyname("vm.freeze_enabled", &freeze_enabled, &length, NULL, 0),
	    "failed to query vm.freeze_enabled");
	if (freeze_enabled) {
		errno = errno_freeze_sysctl;
		T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "sysctl kern.memorystatus_freeze failed");
	} else {
		/* If freezer is disabled, skip the test. This can happen due to disk space shortage. */
		T_LOG("Freeze has been disabled. Terminating early.");
		T_END;
	}
}

static uint32_t max_frozen_demotions_daily_default;

static void
reset_max_frozen_demotions_daily(void)
{
	int sysctl_ret = sysctlbyname("kern.memorystatus_max_freeze_demotions_daily", NULL, NULL, &max_frozen_demotions_daily_default, sizeof(max_frozen_demotions_daily_default));
	T_QUIET; T_ASSERT_POSIX_SUCCESS(sysctl_ret, "set kern.memorystatus_max_freeze_demotions_daily to default");
}

static void
allow_unlimited_demotions(void)
{
	size_t size = sizeof(max_frozen_demotions_daily_default);
	uint32_t new_value = UINT32_MAX;
	int sysctl_ret = sysctlbyname("kern.memorystatus_max_freeze_demotions_daily", &max_frozen_demotions_daily_default, &size, &new_value, sizeof(new_value));
	T_QUIET; T_ASSERT_POSIX_SUCCESS(sysctl_ret, "kern.memorystatus_max_freeze_demotions_daily = UINT32_MAX");
	T_ATEND(reset_max_frozen_demotions_daily);
}

static void
memorystatus_assertion_test_demote_frozen(void)
{
	/*
	 * Test that if we assert a priority on a process, freeze it, and then demote all frozen processes, it does not get demoted below the asserted priority.
	 * Then remove thee assertion, and ensure it gets demoted properly.
	 */
	/* these values will remain fixed during testing */
	int             active_limit_mb = 15;   /* arbitrary */
	int             inactive_limit_mb = 7;  /* arbitrary */
	__block int             demote_value = 1;
	/* Launch the child process, and elevate its priority */
	int requestedpriority;
	dispatch_source_t ds_signal, ds_exit;
	requestedpriority = JETSAM_PRIORITY_FREEZER;
	allow_unlimited_demotions();

	/* Wait for the child process to tell us that it's ready, and then freeze it */
	signal(SIGUSR1, SIG_IGN);
	ds_signal = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGUSR1, 0, dispatch_get_main_queue());
	T_QUIET; T_ASSERT_NOTNULL(ds_signal, "dispatch_source_create");
	dispatch_source_set_event_handler(ds_signal, ^{
		int sysctl_ret;
		/* Freeze the process, trigger agressive demotion, and check that it hasn't been demoted. */
		freeze_process(child_pid);
		/* Agressive demotion */
		sysctl_ret = sysctlbyname("kern.memorystatus_demote_frozen_processes", NULL, NULL, &demote_value, sizeof(demote_value));
		T_QUIET; T_ASSERT_POSIX_SUCCESS(sysctl_ret, "sysctl kern.memorystatus_demote_frozen_processes succeeded");
		/* Check */
		(void)check_properties(child_pid, requestedpriority, inactive_limit_mb, 0x0, ASSERTION_STATE_IS_SET, "Priority was set");
		T_LOG("Relinquishing our assertion.");
		/* Relinquish our assertion, and check that it gets demoted. */
		relinquish_assertion_priority(child_pid, 0x0);
		(void)check_properties(child_pid, JETSAM_PRIORITY_AGING_BAND2, inactive_limit_mb, 0x0, ASSERTION_STATE_IS_RELINQUISHED, "Assertion was reqlinquished.");
		/* Kill the child */
		T_QUIET; T_ASSERT_POSIX_SUCCESS(kill(child_pid, SIGKILL), "Killed child process");
		T_END;
	});

	/* Launch the child process and set the initial properties on it. */
	child_pid = launch_background_helper("frozen_background", false, true);
	set_memlimits(child_pid, active_limit_mb, inactive_limit_mb, false, false);
	set_assertion_priority(child_pid, requestedpriority, 0x0);
	(void)check_properties(child_pid, requestedpriority, inactive_limit_mb, 0x0, ASSERTION_STATE_IS_SET, "Priority was set");
	/* Listen for exit. */
	ds_exit = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, (uintptr_t)child_pid, DISPATCH_PROC_EXIT, dispatch_get_main_queue());
	dispatch_source_set_event_handler(ds_exit, ^{
		int status = 0, code = 0;
		pid_t rc = waitpid(child_pid, &status, 0);
		T_QUIET; T_ASSERT_EQ(rc, child_pid, "waitpid");
		code = WEXITSTATUS(status);
		T_QUIET; T_ASSERT_EQ(code, 0, "Child exited cleanly");
		T_END;
	});

	dispatch_activate(ds_exit);
	dispatch_activate(ds_signal);
	dispatch_main();
}

T_DECL(assertion_test_demote_frozen, "demoted frozen process goes to asserted priority.",
	T_META_BOOTARGS_SET("freeze_enabled=1"),
	T_META_REQUIRES_SYSCTL_EQ("vm.freeze_enabled", 1),
	T_META_ASROOT(true),
	T_META_TAG_VM_NOT_PREFERRED) {
	memorystatus_assertion_test_demote_frozen();
}

static unsigned int
get_freeze_daily_pages_max(void)
{
	unsigned int memorystatus_freeze_daily_mb_max;
	size_t length = sizeof(memorystatus_freeze_daily_mb_max);
	int ret = sysctlbyname("kern.memorystatus_freeze_daily_mb_max", &memorystatus_freeze_daily_mb_max, &length, NULL, 0);
	T_ASSERT_POSIX_SUCCESS(ret, "kern.memorystatus_freeze_daily_mb_max");
	return memorystatus_freeze_daily_mb_max * 1024UL * 1024UL / vm_kernel_page_size;
}

static uint64_t
get_budget_multiplier(void)
{
	uint64_t budget_multiplier = 0;
	size_t size = sizeof(budget_multiplier);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(sysctlbyname("kern.memorystatus_freeze_budget_multiplier", &budget_multiplier, &size, NULL, 0),
	    "get kern.memorystatus_freeze_budget_multiplier");
	return budget_multiplier;
}
static void
set_budget_multiplier(uint64_t multiplier)
{
	T_QUIET; T_ASSERT_POSIX_SUCCESS(sysctlbyname("kern.memorystatus_freeze_budget_multiplier", NULL, NULL, &multiplier, sizeof(multiplier)),
	    "set kern.memorystatus_freeze_budget_multiplier");
}

static uint64_t original_budget_multiplier;
static void
reset_budget_multiplier(void)
{
	set_budget_multiplier(original_budget_multiplier);
}

static unsigned int
get_memorystatus_swap_all_apps(void)
{
	unsigned int memorystatus_swap_all_apps;
	size_t length = sizeof(memorystatus_swap_all_apps);
	int ret = sysctlbyname("kern.memorystatus_swap_all_apps", &memorystatus_swap_all_apps, &length, NULL, 0);
	T_ASSERT_POSIX_SUCCESS(ret, "kern.memorystatus_swap_all_apps");
	return memorystatus_swap_all_apps;
}

T_DECL(budget_replenishment, "budget replenishes properly",
	T_META_BOOTARGS_SET("freeze_enabled=1"),
	T_META_REQUIRES_SYSCTL_EQ("vm.freeze_enabled", 1),
    T_META_REQUIRES_SYSCTL_NE("kern.memorystatus_freeze_daily_mb_max", UINT32_MAX),
	T_META_TAG_VM_NOT_PREFERRED) {
	size_t length;
	int ret;
	static unsigned int kTestIntervalSecs = 60 * 60 * 32; // 32 Hours
	unsigned int memorystatus_freeze_daily_pages_max;
	static unsigned int kFixedPointFactor = 100;
	static unsigned int kNumSecondsInDay = 60 * 60 * 24;
	unsigned int new_budget, expected_new_budget_pages;
	size_t new_budget_ln;
	vm_size_t page_size = vm_kernel_page_size;

	original_budget_multiplier = get_budget_multiplier();
	T_ATEND(reset_budget_multiplier);
	set_budget_multiplier(100);
	/*
	 * Calculate a new budget as if the previous interval expired kTestIntervalSecs
	 * ago and we used up its entire budget.
	 */
	length = sizeof(kTestIntervalSecs);
	new_budget_ln = sizeof(new_budget);
	ret = sysctlbyname("vm.memorystatus_freeze_calculate_new_budget", &new_budget, &new_budget_ln, &kTestIntervalSecs, length);
	T_ASSERT_POSIX_SUCCESS(ret, "vm.memorystatus_freeze_calculate_new_budget");

	memorystatus_freeze_daily_pages_max = get_freeze_daily_pages_max();
	T_LOG("memorystatus_freeze_daily_pages_max %u", memorystatus_freeze_daily_pages_max);
	T_LOG("page_size %lu", page_size);

	/*
	 * We're kTestIntervalSecs past a new interval. Which means we are owed kNumSecondsInDay
	 * seconds of budget.
	 */
	expected_new_budget_pages = memorystatus_freeze_daily_pages_max;
	T_LOG("expected_new_budget_pages before %u", expected_new_budget_pages);
	T_ASSERT_EQ(kTestIntervalSecs, 60 * 60 * 32, "kTestIntervalSecs did not change");
	expected_new_budget_pages += ((kTestIntervalSecs * kFixedPointFactor) / (kNumSecondsInDay)
	    * memorystatus_freeze_daily_pages_max) / kFixedPointFactor;
	if (get_memorystatus_swap_all_apps()) {
		/*
		 * memorystatus_swap_all_apps is enabled; the budget is unlimited
		 */
		expected_new_budget_pages = UINT32_MAX;
	}
	T_LOG("expected_new_budget_pages after %u", expected_new_budget_pages);
	T_LOG("memorystatus_freeze_daily_pages_max after %u", memorystatus_freeze_daily_pages_max);

	T_QUIET; T_ASSERT_EQ(new_budget, expected_new_budget_pages, "Calculate new budget behaves correctly.");
}


static bool
is_proc_in_frozen_list(pid_t pid, char* name, size_t name_len)
{
	int bytes_written;
	bool found = false;
	global_frozen_procs_t *frozen_procs = malloc(sizeof(global_frozen_procs_t));
	T_QUIET; T_ASSERT_NOTNULL(frozen_procs, "malloc");

	bytes_written = memorystatus_control(MEMORYSTATUS_CMD_FREEZER_CONTROL, 0, FREEZER_CONTROL_GET_PROCS, frozen_procs, sizeof(global_frozen_procs_t));
	T_QUIET; T_ASSERT_LE((size_t) bytes_written, sizeof(global_frozen_procs_t), "Didn't overflow buffer");
	T_QUIET; T_ASSERT_GT(bytes_written, 0, "Wrote someting");

	for (size_t i = 0; i < frozen_procs->gfp_num_frozen; i++) {
		if (frozen_procs->gfp_procs[i].fp_pid == pid) {
			found = true;
			strlcpy(name, frozen_procs->gfp_procs[i].fp_name, name_len);
		}
	}
	return found;
}

static void
unset_testing_pid(void)
{
	int ret;
	ret = memorystatus_control(MEMORYSTATUS_CMD_SET_TESTING_PID, 0, MEMORYSTATUS_FLAGS_UNSET_TESTING_PID, NULL, 0);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, 0, "Drop ownership of jetsam snapshot");
}

static void
set_testing_pid(void)
{
	int ret;
	ret = memorystatus_control(MEMORYSTATUS_CMD_SET_TESTING_PID, 0, MEMORYSTATUS_FLAGS_SET_TESTING_PID, NULL, 0);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "Take ownership of jetsam snapshot");
	T_ATEND(unset_testing_pid);
}

/*
 * Retrieve a jetsam snapshot.
 *
 * return:
 *      pointer to snapshot.
 *
 *	Caller is responsible for freeing snapshot.
 */
static
memorystatus_jetsam_snapshot_t *
get_jetsam_snapshot(uint32_t flags, bool empty_allowed)
{
	memorystatus_jetsam_snapshot_t * snapshot = NULL;
	int ret;
	uint32_t size;

	ret = memorystatus_control(MEMORYSTATUS_CMD_GET_JETSAM_SNAPSHOT, 0, flags, NULL, 0);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, 0, "Get jetsam snapshot size");
	size = (uint32_t) ret;
	if (size == 0 && empty_allowed) {
		return snapshot;
	}

	snapshot = (memorystatus_jetsam_snapshot_t*)malloc(size);
	T_QUIET; T_ASSERT_NOTNULL(snapshot, "Allocate snapshot of size %d", size);

	ret = memorystatus_control(MEMORYSTATUS_CMD_GET_JETSAM_SNAPSHOT, 0, flags, snapshot, size);
	T_QUIET; T_ASSERT_GT(size, 0, "Get jetsam snapshot");

	if (((size - sizeof(memorystatus_jetsam_snapshot_t)) / sizeof(memorystatus_jetsam_snapshot_entry_t)) != snapshot->entry_count) {
		T_FAIL("Malformed snapshot: %d! Expected %ld + %zd x %ld = %ld\n", size,
		    sizeof(memorystatus_jetsam_snapshot_t), snapshot->entry_count, sizeof(memorystatus_jetsam_snapshot_entry_t),
		    sizeof(memorystatus_jetsam_snapshot_t) + (snapshot->entry_count * sizeof(memorystatus_jetsam_snapshot_entry_t)));
		if (snapshot) {
			free(snapshot);
		}
	}

	return snapshot;
}

/*
 * Look for the given pid in the snapshot.
 *
 * return:
 *     pointer to pid's entry or NULL if pid is not found.
 *
 * Caller has ownership of snapshot before and after call.
 */
static
memorystatus_jetsam_snapshot_entry_t *
get_jetsam_snapshot_entry(memorystatus_jetsam_snapshot_t *snapshot, pid_t pid)
{
	T_QUIET; T_ASSERT_NOTNULL(snapshot, "Got snapshot");
	for (size_t i = 0; i < snapshot->entry_count; i++) {
		memorystatus_jetsam_snapshot_entry_t *curr = &(snapshot->entries[i]);
		if (curr->pid == pid) {
			return curr;
		}
	}

	return NULL;
}

static void
resume_and_kill_proc(pid_t pid)
{
	int ret = pid_resume(pid);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "proc resumed after freeze");
	T_QUIET; T_ASSERT_POSIX_SUCCESS(kill(pid, SIGKILL), "Killed process");
}

static void
resume_and_kill_child(void)
{
	/* Used for test cleanup. proc might not be suspended so pid_resume might fail. */
	pid_resume(child_pid);
	kill(child_pid, SIGKILL);
}

static dispatch_source_t
run_block_after_signal(int sig, dispatch_block_t block)
{
	dispatch_source_t ds_signal;
	signal(sig, SIG_IGN);
	ds_signal = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, (uintptr_t) sig, 0, dispatch_get_main_queue());
	T_QUIET; T_ASSERT_NOTNULL(ds_signal, "dispatch_source_create");
	dispatch_source_set_event_handler(ds_signal, block);
	return ds_signal;
}

/*
 * Launches the child & runs the given block after the child signals.
 * If exit_with_child is true, the test will exit when the child exits.
 */
static void
test_after_background_helper_launches(bool exit_with_child, const char* variant, dispatch_block_t test_block)
{
	dispatch_source_t ds_signal, ds_exit;

	ds_signal = run_block_after_signal(SIGUSR1, test_block);
	/* Launch the child process. */
	child_pid = launch_background_helper(variant, false, true);
	T_ATEND(resume_and_kill_child);
	/* Listen for exit. */
	if (exit_with_child) {
		ds_exit = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, (uintptr_t)child_pid, DISPATCH_PROC_EXIT, dispatch_get_main_queue());
		dispatch_source_set_event_handler(ds_exit, ^{
			int status = 0, code = 0;
			pid_t rc = waitpid(child_pid, &status, 0);
			T_QUIET; T_ASSERT_EQ(rc, child_pid, "waitpid");
			code = WEXITSTATUS(status);
			if (code != 0) {
			        T_LOG("Child exited with error: %s", exit_codes_str[code]);
			}
			T_QUIET; T_ASSERT_EQ(code, 0, "Child exited cleanly");
			T_END;
		});

		dispatch_activate(ds_exit);
	}
	dispatch_activate(ds_signal);
}

T_DECL(get_frozen_procs, "List processes in the freezer",
	T_META_BOOTARGS_SET("freeze_enabled=1"),
	T_META_REQUIRES_SYSCTL_EQ("vm.freeze_enabled", 1),
	T_META_TAG_VM_NOT_PREFERRED) {

	test_after_background_helper_launches(true, "frozen_background", ^{
		proc_name_t name;
		/* Place the child in the idle band so that it gets elevated like a typical app. */
		move_to_idle_band(child_pid);
		/* Freeze the process, and check that it's in the list of frozen processes. */
		freeze_process(child_pid);
		/* Check */
		T_QUIET; T_ASSERT_TRUE(is_proc_in_frozen_list(child_pid, name, sizeof(name)), "Found proc in frozen list");
		T_QUIET; T_EXPECT_EQ_STR(name, "memorystatus_freeze_test", "Proc has correct name");
		/* Kill the child */
		T_QUIET; T_ASSERT_POSIX_SUCCESS(kill(child_pid, SIGKILL), "Killed child process");
		T_END;
	});
	dispatch_main();
}

T_DECL(frozen_to_swap_accounting, "jetsam snapshot has frozen_to_swap accounting",
	T_META_BOOTARGS_SET("freeze_enabled=1"),
	T_META_REQUIRES_SYSCTL_EQ("vm.freeze_enabled", 1),
	T_META_TAG_VM_NOT_PREFERRED) {
	static const size_t kSnapshotSleepDelay = 5;
	static const size_t kFreezeToDiskMaxDelay = 60;


	test_after_background_helper_launches(true, "frozen_background", ^{
		memorystatus_jetsam_snapshot_t *snapshot = NULL;
		memorystatus_jetsam_snapshot_entry_t *child_entry = NULL;
		/* Place the child in the idle band so that it gets elevated like a typical app. */
		move_to_idle_band(child_pid);
		freeze_process(child_pid);
		/*
		 * Wait until the child's pages get paged out to disk.
		 * If we don't see any pages get sent to disk before kFreezeToDiskMaxDelay seconds,
		 * something is either wrong with the compactor or the accounting.
		 */
		for (size_t i = 0; i < kFreezeToDiskMaxDelay / kSnapshotSleepDelay; i++) {
		        snapshot = get_jetsam_snapshot(MEMORYSTATUS_FLAGS_SNAPSHOT_ON_DEMAND, false);
		        child_entry = get_jetsam_snapshot_entry(snapshot, child_pid);
		        T_QUIET; T_ASSERT_NOTNULL(child_entry, "Found child in snapshot");
		        if (child_entry->jse_frozen_to_swap_pages > 0) {
		                break;
			}
		        free(snapshot);
		        sleep(kSnapshotSleepDelay);
		}
		T_QUIET; T_ASSERT_GT(child_entry->jse_frozen_to_swap_pages, 0ULL, "child has some pages in swap");
		free(snapshot);
		/* Kill the child */
		T_QUIET; T_ASSERT_POSIX_SUCCESS(kill(child_pid, SIGKILL), "Killed child process");
		T_END;
	});
	dispatch_main();
}

T_DECL(freezer_snapshot, "App kills are recorded in the freezer snapshot",
	T_META_BOOTARGS_SET("freeze_enabled=1"),
	T_META_REQUIRES_SYSCTL_EQ("vm.freeze_enabled", 1),
	T_META_TAG_VM_NOT_PREFERRED) {
	/* Take ownership of the snapshot to ensure we don't race with another process trying to consume them. */
	set_testing_pid();

	test_after_background_helper_launches(false, "frozen_background", ^{
		int ret;
		memorystatus_jetsam_snapshot_t *snapshot = NULL;
		memorystatus_jetsam_snapshot_entry_t *child_entry = NULL;

		ret = memorystatus_control(MEMORYSTATUS_CMD_TEST_JETSAM, child_pid, 0, 0, 0);
		T_ASSERT_POSIX_SUCCESS(ret, "jetsam'd the child");

		snapshot = get_jetsam_snapshot(MEMORYSTATUS_FLAGS_SNAPSHOT_FREEZER, false);
		T_ASSERT_NOTNULL(snapshot, "Got freezer snapshot");
		child_entry = get_jetsam_snapshot_entry(snapshot, child_pid);
		T_QUIET; T_ASSERT_NOTNULL(child_entry, "Child is in freezer snapshot");
		T_QUIET; T_ASSERT_EQ(child_entry->killed, (unsigned long long) JETSAM_REASON_GENERIC, "Child entry was killed");

		free(snapshot);
		T_END;
	});
	dispatch_main();
}

T_DECL(freezer_snapshot_consume, "Freezer snapshot is consumed on read",
	T_META_BOOTARGS_SET("freeze_enabled=1"),
	T_META_REQUIRES_SYSCTL_EQ("vm.freeze_enabled", 1),
	T_META_TAG_VM_NOT_PREFERRED) {
	/* Take ownership of the snapshot to ensure we don't race with another process trying to consume them. */
	set_testing_pid();

	test_after_background_helper_launches(false, "frozen_background", ^{
		int ret;
		memorystatus_jetsam_snapshot_t *snapshot = NULL;
		memorystatus_jetsam_snapshot_entry_t *child_entry = NULL;

		ret = memorystatus_control(MEMORYSTATUS_CMD_TEST_JETSAM, child_pid, 0, 0, 0);
		T_ASSERT_POSIX_SUCCESS(ret, "jetsam'd the child");

		snapshot = get_jetsam_snapshot(MEMORYSTATUS_FLAGS_SNAPSHOT_FREEZER, false);
		T_ASSERT_NOTNULL(snapshot, "Got first freezer snapshot");
		child_entry = get_jetsam_snapshot_entry(snapshot, child_pid);
		T_QUIET; T_ASSERT_NOTNULL(child_entry, "Child is in first freezer snapshot");

		snapshot = get_jetsam_snapshot(MEMORYSTATUS_FLAGS_SNAPSHOT_FREEZER, true);
		if (snapshot != NULL) {
		        child_entry = get_jetsam_snapshot_entry(snapshot, child_pid);
		        T_QUIET; T_ASSERT_NULL(child_entry, "Child is not in second freezer snapshot");
		}

		free(snapshot);
		T_END;
	});
	dispatch_main();
}

T_DECL(freezer_snapshot_frozen_state, "Frozen state is recorded in freezer snapshot",
	T_META_BOOTARGS_SET("freeze_enabled=1"),
	T_META_REQUIRES_SYSCTL_EQ("vm.freeze_enabled", 1),
	T_META_TAG_VM_NOT_PREFERRED) {
	/* Take ownership of the snapshot to ensure we don't race with another process trying to consume them. */
	set_testing_pid();

	test_after_background_helper_launches(false, "frozen_background", ^{
		int ret;
		memorystatus_jetsam_snapshot_t *snapshot = NULL;
		memorystatus_jetsam_snapshot_entry_t *child_entry = NULL;

		move_to_idle_band(child_pid);
		freeze_process(child_pid);

		ret = memorystatus_control(MEMORYSTATUS_CMD_TEST_JETSAM, child_pid, 0, 0, 0);
		T_ASSERT_POSIX_SUCCESS(ret, "jetsam'd the child");

		snapshot = get_jetsam_snapshot(MEMORYSTATUS_FLAGS_SNAPSHOT_FREEZER, false);
		T_ASSERT_NOTNULL(snapshot, "Got freezer snapshot");
		child_entry = get_jetsam_snapshot_entry(snapshot, child_pid);
		T_QUIET; T_ASSERT_NOTNULL(child_entry, "Child is in freezer snapshot");
		T_QUIET; T_ASSERT_TRUE(child_entry->state & kMemorystatusFrozen, "Child entry's frozen bit is set");

		free(snapshot);
		T_END;
	});
	dispatch_main();
}

T_DECL(freezer_snapshot_thaw_state, "Thaw count is recorded in freezer snapshot",
	T_META_BOOTARGS_SET("freeze_enabled=1"),
	T_META_REQUIRES_SYSCTL_EQ("vm.freeze_enabled", 1),
	T_META_TAG_VM_NOT_PREFERRED) {
	/* Take ownership of the snapshot to ensure we don't race with another process trying to consume them. */
	set_testing_pid();

	test_after_background_helper_launches(false, "frozen_background", ^{
		int ret;
		memorystatus_jetsam_snapshot_t *snapshot = NULL;
		memorystatus_jetsam_snapshot_entry_t *child_entry = NULL;

		move_to_idle_band(child_pid);
		ret = pid_suspend(child_pid);
		T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "child suspended");
		freeze_process(child_pid);
		ret = pid_resume(child_pid);
		T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "child resumed after freeze");

		ret = memorystatus_control(MEMORYSTATUS_CMD_TEST_JETSAM, child_pid, 0, 0, 0);
		T_ASSERT_POSIX_SUCCESS(ret, "jetsam'd the child");

		snapshot = get_jetsam_snapshot(MEMORYSTATUS_FLAGS_SNAPSHOT_FREEZER, false);
		T_ASSERT_NOTNULL(snapshot, "Got freezer snapshot");
		child_entry = get_jetsam_snapshot_entry(snapshot, child_pid);
		T_QUIET; T_ASSERT_NOTNULL(child_entry, "Child is in freezer snapshot");
		T_QUIET; T_ASSERT_TRUE(child_entry->state & kMemorystatusFrozen, "Child entry's frozen bit is still set after thaw");
		T_QUIET; T_ASSERT_TRUE(child_entry->state & kMemorystatusWasThawed, "Child entry was thawed");
		T_QUIET; T_ASSERT_EQ(child_entry->jse_thaw_count, 1ULL, "Child entry's thaw count was incremented");

		free(snapshot);
		T_END;
	});
	dispatch_main();
}

T_HELPER_DECL(check_frozen, "Check frozen state", T_META_ASROOT(true)) {
	int kern_ret;
	dispatch_source_t ds_signal;
	__block int is_frozen;
	/* Set the process to freezable */
	kern_ret = memorystatus_control(MEMORYSTATUS_CMD_SET_PROCESS_IS_FREEZABLE, getpid(), 1, NULL, 0);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(kern_ret, "set process is freezable");

	/* We should not be frozen yet. */
	is_frozen = memorystatus_control(MEMORYSTATUS_CMD_GET_PROCESS_IS_FROZEN, getpid(), 0, NULL, 0);
	if (is_frozen == -1) {
		T_LOG("memorystatus_control error: %s", strerror(errno));
		exit(MEMORYSTATUS_CONTROL_ERROR);
	}
	if (is_frozen) {
		exit(FROZEN_BIT_SET);
	}

	ds_signal = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGUSR1, 0, dispatch_get_main_queue());
	if (ds_signal == NULL) {
		exit(DISPATCH_SOURCE_CREATE_FAILED);
	}

	dispatch_source_set_event_handler(ds_signal, ^{
		/* We should now be frozen. */
		is_frozen = memorystatus_control(MEMORYSTATUS_CMD_GET_PROCESS_IS_FROZEN, getpid(), 0, NULL, 0);
		if (is_frozen == -1) {
		        T_LOG("memorystatus_control error: %s", strerror(errno));
		        exit(MEMORYSTATUS_CONTROL_ERROR);
		}
		if (!is_frozen) {
		        exit(FROZEN_BIT_NOT_SET);
		}
		exit(SUCCESS);
	});
	dispatch_activate(ds_signal);

	sig_t sig_ret = signal(SIGUSR1, SIG_IGN);
	T_QUIET; T_WITH_ERRNO; T_ASSERT_NE(sig_ret, SIG_ERR, "signal(SIGUSR1, SIG_IGN)");

	/* Signal to our parent that we can be frozen */
	if (kill(getppid(), SIGUSR1) != 0) {
		T_LOG("Unable to signal to parent process!");
		exit(SIGNAL_TO_PARENT_FAILED);
	}

	dispatch_main();
}

T_DECL(memorystatus_get_process_is_frozen, "MEMORYSTATUS_CMD_GET_PROCESS_IS_FROZEN returns correct state",
	T_META_BOOTARGS_SET("freeze_enabled=1"),
	T_META_REQUIRES_SYSCTL_EQ("vm.freeze_enabled", 1),
	T_META_TAG_VM_NOT_PREFERRED) {

	test_after_background_helper_launches(true, "check_frozen", ^{
		int ret;
		/* Freeze the child, resume it, and signal it to check its state */
		move_to_idle_band(child_pid);
		ret = pid_suspend(child_pid);
		T_ASSERT_POSIX_SUCCESS(ret, "child suspended");
		freeze_process(child_pid);
		ret = pid_resume(child_pid);
		T_ASSERT_POSIX_SUCCESS(ret, "child resumed after freeze");

		kill(child_pid, SIGUSR1);
		/* The child will checks its own frozen state & exit. */
	});
	dispatch_main();
}

static unsigned int freeze_pages_min_old;
static int throttle_enabled_old;
static void
cleanup_memorystatus_freeze_top_process(void)
{
	sysctlbyname("kern.memorystatus_freeze_pages_min", NULL, NULL, &freeze_pages_min_old, sizeof(freeze_pages_min_old));
	sysctlbyname("kern.memorystatus_freeze_throttle_enabled", NULL, NULL, &throttle_enabled_old, sizeof(throttle_enabled_old));
}

/*
 * Disables heuristics that could prevent us from freezing the child via memorystatus_freeze_top_process.
 */
static void
memorystatus_freeze_top_process_setup(void)
{
	size_t freeze_pages_min_size = sizeof(freeze_pages_min_old);
	unsigned int freeze_pages_min_new = 0;
	size_t throttle_enabled_old_size = sizeof(throttle_enabled_old);
	int throttle_enabled_new = 1, ret;

	ret = sysctlbyname("kern.memorystatus_freeze_pages_min", &freeze_pages_min_old, &freeze_pages_min_size, &freeze_pages_min_new, sizeof(freeze_pages_min_new));
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "set kern.memorystatus_freeze_pages_min");
	ret = sysctlbyname("kern.memorystatus_freeze_throttle_enabled", &throttle_enabled_old, &throttle_enabled_old_size, &throttle_enabled_new, sizeof(throttle_enabled_new));
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "set kern.memorystatus_freeze_throttle_enabled");
	T_ATEND(cleanup_memorystatus_freeze_top_process);
	/* Take ownership of the freezer probabilities for the duration of the test so that we don't race with dasd. */
	set_testing_pid();
}

/*
 * Moves the proc to the idle band and suspends it.
 */
static void
prepare_proc_for_freezing(pid_t pid)
{
	move_to_idle_band(pid);
	int ret = pid_suspend(pid);
	T_ASSERT_POSIX_SUCCESS(ret, "proc suspended");
}

#define P_MEMSTAT_FROZEN 0x00000002
static void
verify_proc_frozen_state(pid_t pid, bool expected)
{
	memorystatus_jetsam_snapshot_t *snapshot = get_jetsam_snapshot(MEMORYSTATUS_FLAGS_SNAPSHOT_ON_DEMAND, false);
	memorystatus_jetsam_snapshot_entry_t *entry = get_jetsam_snapshot_entry(snapshot, pid);
	T_ASSERT_NOTNULL(entry, "%d is in snapshot", pid);
	bool is_frozen = (entry->state & P_MEMSTAT_FROZEN) != 0;
	if (is_frozen != expected) {
		T_LOG("%s frozen state is wrong. Expected %d, got %d. Skip reason: %d. Jetsam band: %d", entry->name, expected, is_frozen, entry->jse_freeze_skip_reason, entry->priority);
	}
	T_ASSERT_EQ(is_frozen, expected, "%s frozen state", entry->name);
	free(snapshot);
}

static void
verify_proc_is_frozen(pid_t pid)
{
	verify_proc_frozen_state(pid, true);
}

static void
verify_proc_not_frozen(pid_t pid)
{
	verify_proc_frozen_state(pid, false);
}

T_DECL(memorystatus_freeze_top_process, "memorystatus_freeze_top_process chooses the correct process",
	T_META_BOOTARGS_SET("freeze_enabled=1"),
	T_META_REQUIRES_SYSCTL_EQ("vm.freeze_enabled", 1),
    T_META_ASROOT(true),
    T_META_REQUIRES_SYSCTL_EQ("kern.development", 1),
	T_META_TAG_VM_NOT_PREFERRED) {
	T_SKIP("Skipping flaky test"); // rdar://76986376
	int32_t memorystatus_freeze_band = 0;
	size_t memorystatus_freeze_band_size = sizeof(memorystatus_freeze_band);
	__block errno_t ret;
	__block int maxproc;
	size_t maxproc_size = sizeof(maxproc);

	ret = sysctlbyname("kern.maxproc", &maxproc, &maxproc_size, NULL, 0);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "kern.maxproc");
	ret = sysctlbyname("kern.memorystatus_freeze_jetsam_band", &memorystatus_freeze_band, &memorystatus_freeze_band_size, NULL, 0);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "kern.memorystatus_freeze_jetsam_band");

	memorystatus_freeze_top_process_setup();
	test_after_background_helper_launches(true, "frozen_background", ^{
		int32_t child_band = JETSAM_PRIORITY_DEFAULT;
		prepare_proc_for_freezing(child_pid);

		size_t buffer_len = sizeof(memorystatus_properties_entry_v1_t) * (size_t) maxproc;
		memorystatus_properties_entry_v1_t *properties_list = malloc(buffer_len);
		T_QUIET; T_ASSERT_NOTNULL(properties_list, "malloc properties array");
		size_t properties_list_len = 0;
		/* The child needs to age down into the idle band before it's eligible to be frozen. */
		T_LOG("Waiting for child to age into the idle band.");
		while (child_band != JETSAM_PRIORITY_IDLE) {
		        memset(properties_list, 0, buffer_len);
		        properties_list_len = 0;
		        memorystatus_jetsam_snapshot_t *snapshot = get_jetsam_snapshot(MEMORYSTATUS_FLAGS_SNAPSHOT_ON_DEMAND, false);

		        bool found = false;
		        for (size_t i = 0; i < snapshot->entry_count; i++) {
		                memorystatus_jetsam_snapshot_entry_t *snapshot_entry = &snapshot->entries[i];
		                if (snapshot_entry->priority <= memorystatus_freeze_band && !snapshot_entry->killed) {
		                        pid_t pid = snapshot_entry->pid;
		                        memorystatus_properties_entry_v1_t *property_entry = &properties_list[properties_list_len++];
		                        property_entry->version = 1;
		                        property_entry->pid = pid;
		                        if (pid == child_pid) {
		                                found = true;
		                                property_entry->use_probability = 1;
		                                child_band = snapshot_entry->priority;
					} else {
		                                property_entry->use_probability = 0;
					}
		                        strncpy(property_entry->proc_name, snapshot_entry->name, MAXCOMLEN);
		                        property_entry->proc_name[MAXCOMLEN] = '\0';
				}
			}
		        T_QUIET; T_ASSERT_TRUE(found, "Child is in on demand snapshot");
		        free(snapshot);
		}
		ret = memorystatus_control(MEMORYSTATUS_CMD_GRP_SET_PROPERTIES, 0, MEMORYSTATUS_FLAGS_GRP_SET_PROBABILITY, properties_list, sizeof(memorystatus_properties_entry_v1_t) * properties_list_len);
		T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "MEMORYSTATUS_FLAGS_GRP_SET_PROBABILITY");
		free(properties_list);
		int val = 1;
		ret = sysctlbyname("vm.memorystatus_freeze_top_process", NULL, NULL, &val, sizeof(val));
		T_ASSERT_POSIX_SUCCESS(ret, "freeze_top_process");

		verify_proc_is_frozen(child_pid);
		resume_and_kill_proc(child_pid);
		T_END;
	});
	dispatch_main();
}
static unsigned int use_ordered_list_original;
static unsigned int use_demotion_list_original;
static void
reset_ordered_freeze_mode(void)
{
	sysctlbyname("kern.memorystatus_freezer_use_ordered_list", NULL, NULL, &use_ordered_list_original, sizeof(use_ordered_list_original));
}

static void
reset_ordered_demote_mode(void)
{
	sysctlbyname("kern.memorystatus_freezer_use_demotion_list", NULL, NULL, &use_demotion_list_original, sizeof(use_demotion_list_original));
}

static void
enable_ordered_freeze_mode(void)
{
	int ret;
	int val = 1;
	size_t size = sizeof(use_ordered_list_original);
	ret = sysctlbyname("kern.memorystatus_freezer_use_ordered_list", &use_ordered_list_original, &size, &val, sizeof(val));
	T_ASSERT_POSIX_SUCCESS(ret, "kern.memorystatus_freezer_use_ordered_list");
	T_ATEND(reset_ordered_freeze_mode);
}

static void
enable_ordered_demote_mode(void)
{
	int ret;
	int val = 1;
	size_t size = sizeof(use_demotion_list_original);
	ret = sysctlbyname("kern.memorystatus_freezer_use_demotion_list", &use_demotion_list_original, &size, &val, sizeof(val));
	T_ASSERT_POSIX_SUCCESS(ret, "kern.memorystatus_freezer_use_demotion_list");
	T_ATEND(reset_ordered_demote_mode);
}

static void
construct_child_freeze_entry(memorystatus_properties_freeze_entry_v1 *entry)
{
	memset(entry, 0, sizeof(memorystatus_properties_freeze_entry_v1));
	entry->version = 1;
	entry->pid = child_pid;
	entry->priority = 1;

	/* Get the child's name. */
	memorystatus_jetsam_snapshot_t *snapshot = get_jetsam_snapshot(MEMORYSTATUS_FLAGS_SNAPSHOT_ON_DEMAND, false);
	memorystatus_jetsam_snapshot_entry_t *snapshot_entry = get_jetsam_snapshot_entry(snapshot, child_pid);
	strncpy(entry->proc_name, snapshot_entry->name, sizeof(entry->proc_name));
	free(snapshot);
}

T_DECL(memorystatus_freeze_top_process_ordered, "memorystatus_freeze_top_process chooses the correct process when using an ordered list",
	T_META_BOOTARGS_SET("freeze_enabled=1"),
	T_META_REQUIRES_SYSCTL_EQ("vm.freeze_enabled", 1),
    T_META_ASROOT(true),
    T_META_REQUIRES_SYSCTL_EQ("kern.development", 1),
	T_META_TAG_VM_NOT_PREFERRED) {
	memorystatus_freeze_top_process_setup();
	enable_ordered_freeze_mode();
	test_after_background_helper_launches(true, "frozen_background", ^{
		int ret, val = 1;
		memorystatus_properties_freeze_entry_v1 entries[1];

		construct_child_freeze_entry(&entries[0]);
		prepare_proc_for_freezing(child_pid);

		T_LOG("Telling kernel to freeze %s", entries[0].proc_name);
		ret = memorystatus_control(MEMORYSTATUS_CMD_GRP_SET_PROPERTIES, 0, MEMORYSTATUS_FLAGS_GRP_SET_FREEZE_PRIORITY, entries, sizeof(entries));
		T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "MEMORYSTATUS_FLAGS_GRP_SET_FREEZE_PRIORITY");

		ret = sysctlbyname("vm.memorystatus_freeze_top_process", NULL, NULL, &val, sizeof(val));
		T_ASSERT_POSIX_SUCCESS(ret, "freeze_top_process");

		verify_proc_is_frozen(child_pid);
		resume_and_kill_proc(child_pid);

		T_END;
	});
	dispatch_main();
}

static void
memorystatus_freeze_top_process_ordered_wrong_pid(pid_t (^pid_for_entry)(pid_t))
{
	memorystatus_freeze_top_process_setup();
	enable_ordered_freeze_mode();
	test_after_background_helper_launches(true, "frozen_background", ^{
		int ret, val = 1;
		memorystatus_properties_freeze_entry_v1 entries[1];

		construct_child_freeze_entry(&entries[0]);
		entries[0].pid = pid_for_entry(child_pid);
		prepare_proc_for_freezing(child_pid);

		T_LOG("Telling kernel to freeze %s", entries[0].proc_name);
		ret = memorystatus_control(MEMORYSTATUS_CMD_GRP_SET_PROPERTIES, 0, MEMORYSTATUS_FLAGS_GRP_SET_FREEZE_PRIORITY, entries, sizeof(entries));
		T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "MEMORYSTATUS_FLAGS_GRP_SET_FREEZE_PRIORITY");

		ret = sysctlbyname("vm.memorystatus_freeze_top_process", NULL, NULL, &val, sizeof(val));
		T_ASSERT_POSIX_SUCCESS(ret, "freeze_top_process");

		verify_proc_is_frozen(child_pid);
		resume_and_kill_proc(child_pid);

		T_END;
	});
	dispatch_main();
}

/*
 * Try both with a pid that's used by another process
 * and a pid that is likely unused.
 * In both cases the child should still get frozen.
 */
T_DECL(memorystatus_freeze_top_process_ordered_reused_pid, "memorystatus_freeze_top_process is resilient to pid changes",
	T_META_BOOTARGS_SET("freeze_enabled=1"),
	T_META_REQUIRES_SYSCTL_EQ("vm.freeze_enabled", 1),
	T_META_REQUIRES_SYSCTL_EQ("kern.development", 1),
    T_META_ASROOT(true),
	T_META_TAG_VM_NOT_PREFERRED) {
	memorystatus_freeze_top_process_ordered_wrong_pid(^(__unused pid_t child) {
		return 1;
	});
}

T_DECL(memorystatus_freeze_top_process_ordered_wrong_pid, "memorystatus_freeze_top_process is resilient to pid changes",
	T_META_BOOTARGS_SET("freeze_enabled=1"),
	T_META_REQUIRES_SYSCTL_EQ("vm.freeze_enabled", 1),
    T_META_ASROOT(true),
    T_META_REQUIRES_SYSCTL_EQ("kern.development", 1),
	T_META_TAG_VM_NOT_PREFERRED) {
	memorystatus_freeze_top_process_ordered_wrong_pid(^(__unused pid_t child) {
		return child + 1000;
	});
}

T_DECL(memorystatus_freeze_demote_ordered, "memorystatus_demote_frozen_processes_using_demote_list chooses the correct process",
	T_META_BOOTARGS_SET("freeze_enabled=1"),
	T_META_REQUIRES_SYSCTL_EQ("vm.freeze_enabled", 1),
    T_META_ASROOT(true),
    T_META_REQUIRES_SYSCTL_EQ("kern.development", 1),
	T_META_TAG_VM_NOT_PREFERRED) {
	memorystatus_freeze_top_process_setup();
	enable_ordered_freeze_mode();
	enable_ordered_demote_mode();
	test_after_background_helper_launches(true, "frozen_background", ^{
		int ret, val = 1;
		int32_t memorystatus_freeze_band = 0;
		size_t memorystatus_freeze_band_size = sizeof(memorystatus_freeze_band);
		memorystatus_jetsam_snapshot_t *snapshot = NULL;
		memorystatus_jetsam_snapshot_entry_t *child_entry = NULL;
		memorystatus_properties_freeze_entry_v1 entries[1];

		ret = sysctlbyname("kern.memorystatus_freeze_jetsam_band", &memorystatus_freeze_band, &memorystatus_freeze_band_size, NULL, 0);
		T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "kern.memorystatus_freeze_jetsam_band");

		construct_child_freeze_entry(&entries[0]);
		prepare_proc_for_freezing(child_pid);

		T_LOG("Telling kernel to freeze %s", entries[0].proc_name);
		ret = memorystatus_control(MEMORYSTATUS_CMD_GRP_SET_PROPERTIES, 0, MEMORYSTATUS_FLAGS_GRP_SET_FREEZE_PRIORITY, entries, sizeof(entries));
		T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "MEMORYSTATUS_FLAGS_GRP_SET_FREEZE_PRIORITY");

		ret = sysctlbyname("vm.memorystatus_freeze_top_process", NULL, NULL, &val, sizeof(val));
		T_ASSERT_POSIX_SUCCESS(ret, "freeze_top_process");

		verify_proc_is_frozen(child_pid);

		/*
		 * Place the child at the head of the demotion list.
		 */
		T_LOG("Telling kernel to demote %s", entries[0].proc_name);
		ret = memorystatus_control(MEMORYSTATUS_CMD_GRP_SET_PROPERTIES, 0, MEMORYSTATUS_FLAGS_GRP_SET_DEMOTE_PRIORITY, entries, sizeof(entries));
		T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "MEMORYSTATUS_FLAGS_GRP_SET_DEMOTE_PRIORITY");

		/* Resume the child */
		ret = pid_resume(child_pid);
		T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "child resumed after freeze");

		/* Trigger a demotion check */
		val = 1;
		ret = sysctlbyname("kern.memorystatus_demote_frozen_processes", NULL, NULL, &val, sizeof(val));
		T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "sysctl kern.memorystatus_demote_frozen_processes succeeded");

		/* Verify that the child was demoted */
		snapshot = get_jetsam_snapshot(MEMORYSTATUS_FLAGS_SNAPSHOT_ON_DEMAND, false);
		child_entry = get_jetsam_snapshot_entry(snapshot, child_pid);
		T_QUIET; T_ASSERT_NOTNULL(child_entry, "Found child in snapshot");
		T_QUIET; T_ASSERT_LT(child_entry->priority, memorystatus_freeze_band, "child was demoted");
		free(snapshot);

		T_QUIET; T_ASSERT_POSIX_SUCCESS(kill(child_pid, SIGKILL), "Killed process");

		T_END;
	});
	dispatch_main();
}

static int
memorystatus_freezer_thaw_percentage(void)
{
	int val;
	size_t size = sizeof(val);
	int ret = sysctlbyname("kern.memorystatus_freezer_thaw_percentage", &val, &size, NULL, 0);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "failed to query kern.memorystatus_freezer_thaw_percentage");
	return val;
}

static void
reset_interval(void)
{
	uint32_t freeze_daily_budget_mb = 0;
	size_t size = sizeof(freeze_daily_budget_mb);
	int ret;
	uint64_t new_budget;
	ret = sysctlbyname("kern.memorystatus_freeze_daily_mb_max", &freeze_daily_budget_mb, &size, NULL, 0);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "failed to query kern.memorystatus_freeze_daily_mb_max");
	new_budget = (freeze_daily_budget_mb * (1UL << 20) / vm_page_size);
	ret = sysctlbyname("kern.memorystatus_freeze_budget_pages_remaining", NULL, NULL, &new_budget, sizeof(new_budget));
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "failed to set kern.memorystatus_freeze_budget_pages_remaining");
}

static pid_t second_child;
static void
cleanup_memorystatus_freezer_thaw_percentage(void)
{
	kill(second_child, SIGKILL);
}

T_DECL(memorystatus_freezer_thaw_percentage, "memorystatus_freezer_thaw_percentage updates correctly",
	T_META_BOOTARGS_SET("freeze_enabled=1"),
	T_META_REQUIRES_SYSCTL_EQ("vm.freeze_enabled", 1),
    T_META_ASROOT(true),
    T_META_REQUIRES_SYSCTL_EQ("kern.development", 1),
	T_META_TAG_VM_NOT_PREFERRED) {
	__block dispatch_source_t first_signal_block;
	/* Take ownership of the freezer probabilities for the duration of the test so that nothing new gets frozen by dasd. */
	set_testing_pid();
	reset_interval();

	/* Spawn one child that will remain frozen throughout the whole test & another that will be thawed. */
	first_signal_block = run_block_after_signal(SIGUSR1, ^{
		move_to_idle_band(second_child);
		__block int ret = pid_suspend(second_child);
		T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "child suspended");
		freeze_process(second_child);
		T_QUIET; T_ASSERT_EQ(memorystatus_freezer_thaw_percentage(), 0, "thaw percentage is still 0 after freeze");
		dispatch_source_cancel(first_signal_block);
		test_after_background_helper_launches(true, "frozen_background", ^{
			reset_interval();
			T_QUIET; T_ASSERT_EQ(memorystatus_freezer_thaw_percentage(), 0, "new interval starts with a thaw percentage of 0");
			move_to_idle_band(child_pid);
			ret = pid_suspend(child_pid);
			T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "child suspended");
			freeze_process(child_pid);
			ret = pid_resume(child_pid);
			T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "child resumed after freeze");
			int percentage_after_thaw = memorystatus_freezer_thaw_percentage();
			T_QUIET; T_ASSERT_GT(percentage_after_thaw, 0, "thaw percentage is higher after thaw");

			ret = pid_suspend(child_pid);
			T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "child suspended");
			freeze_process(child_pid);
			ret = pid_resume(child_pid);
			T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "child resumed after freeze");
			T_QUIET; T_ASSERT_EQ(memorystatus_freezer_thaw_percentage(), percentage_after_thaw, "thaw percentage is unchanged after second thaw");

			ret = pid_suspend(child_pid);
			T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "child suspended");
			freeze_process(child_pid);
			reset_interval();
			T_QUIET; T_ASSERT_EQ(memorystatus_freezer_thaw_percentage(), 0, "new interval starts with a 0 thaw percentage");
			ret = pid_resume(child_pid);
			T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "child resumed after freeze");
			T_QUIET; T_ASSERT_GT(memorystatus_freezer_thaw_percentage(), 0, "thaw percentage goes back up in new interval");

			T_QUIET; T_ASSERT_POSIX_SUCCESS(kill(child_pid, SIGKILL), "failed to kill child");
			T_END;
		});
	});

	second_child = launch_background_helper("frozen_background", false, true);
	T_ATEND(cleanup_memorystatus_freezer_thaw_percentage);
	dispatch_activate(first_signal_block);
	dispatch_main();
}

static uint64_t
get_budget_pages_remaining(void)
{
	uint64_t pages_remaining = 0;
	size_t size = sizeof(pages_remaining);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(sysctlbyname("kern.memorystatus_freeze_budget_pages_remaining", &pages_remaining, &size, NULL, 0),
	    "get kern.memorystatus_freeze_budget_pages_remaining");
	return pages_remaining;
}

static void
set_budget_pages_remaining(uint64_t pages_remaining)
{
	T_QUIET; T_ASSERT_POSIX_SUCCESS(sysctlbyname("kern.memorystatus_freeze_budget_pages_remaining", NULL, NULL, &pages_remaining, sizeof(pages_remaining)),
	    "get kern.memorystatus_freeze_budget_pages_remaining");
}

static void
enable_freeze(void)
{
	int freeze_enabled = 1;
	size_t length = sizeof(freeze_enabled);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(sysctlbyname("vm.freeze_enabled", NULL, NULL, &freeze_enabled, length),
	    "enable vm.freeze_enabled");
}

T_DECL(memorystatus_freeze_budget_multiplier, "memorystatus_budget_multiplier multiplies budget",
	T_META_BOOTARGS_SET("freeze_enabled=1"),
	T_META_REQUIRES_SYSCTL_EQ("vm.freeze_enabled", 1),
    T_META_ASROOT(true),
    T_META_REQUIRES_SYSCTL_NE("kern.memorystatus_freeze_daily_mb_max", UINT32_MAX),
    T_META_REQUIRES_SYSCTL_EQ("kern.development", 1),
    T_META_ENABLED(false) /* rdar://87165483 */,
	T_META_TAG_VM_NOT_PREFERRED) {
	/* Disable freezer so that the budget doesn't change out from underneath us. */
	int freeze_enabled = 0;
	size_t length = sizeof(freeze_enabled);
	uint64_t freeze_daily_pages_max;
	T_ATEND(enable_freeze);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(sysctlbyname("vm.freeze_enabled", NULL, NULL, &freeze_enabled, length),
	    "disable vm.freeze_enabled");
	freeze_daily_pages_max = get_freeze_daily_pages_max();
	original_budget_multiplier = get_budget_multiplier();
	T_ATEND(reset_budget_multiplier);
	set_budget_multiplier(100);
	T_QUIET; T_ASSERT_EQ(get_budget_pages_remaining(), freeze_daily_pages_max, "multiplier=100%%");
	set_budget_multiplier(50);
	T_QUIET; T_ASSERT_EQ(get_budget_pages_remaining(), freeze_daily_pages_max / 2, "multiplier=50%%");
	set_budget_multiplier(200);
	T_QUIET; T_ASSERT_EQ(get_budget_pages_remaining(), freeze_daily_pages_max * 2, "multiplier=200%%");
}

T_DECL(memorystatus_freeze_set_dasd_trial_identifiers, "set dasd trial identifiers",
	T_META_BOOTARGS_SET("freeze_enabled=1"),
	T_META_REQUIRES_SYSCTL_EQ("vm.freeze_enabled", 1),
    T_META_ASROOT(true),
	T_META_TAG_VM_NOT_PREFERRED) {
#define TEST_STR "freezer-das-trial"
	memorystatus_freezer_trial_identifiers_v1 identifiers = {0};
	identifiers.version = 1;
	strncpy(identifiers.treatment_id, TEST_STR, sizeof(identifiers.treatment_id));
	strncpy(identifiers.experiment_id, TEST_STR, sizeof(identifiers.treatment_id));
	identifiers.deployment_id = 2;
	int ret = memorystatus_control(MEMORYSTATUS_CMD_FREEZER_CONTROL, 0, FREEZER_CONTROL_SET_DASD_TRIAL_IDENTIFIERS, &identifiers, sizeof(identifiers));
	T_WITH_ERRNO; T_ASSERT_EQ(ret, 0, "FREEZER_CONTROL_SET_DASD_TRIAL_IDENTIFIERS");
}

T_DECL(memorystatus_reset_freezer_state, "FREEZER_CONTROL_RESET_STATE kills frozen proccesses",
	T_META_BOOTARGS_SET("freeze_enabled=1"),
	T_META_REQUIRES_SYSCTL_EQ("vm.freeze_enabled", 1),
    T_META_ASROOT(true),
    T_META_REQUIRES_SYSCTL_EQ("kern.development", 1),
	T_META_TAG_VM_NOT_PREFERRED) {
	/* Take ownership of the freezer probabilities for the duration of the test so that nothing new gets frozen by dasd. */
	set_testing_pid();
	reset_interval();

	test_after_background_helper_launches(false, "frozen_background", ^{
		proc_name_t name;
		int ret;

		/* Freeze the child and verify they're frozen. */
		move_to_idle_band(child_pid);
		ret = pid_suspend(child_pid);
		T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "child suspended");
		freeze_process(child_pid);
		T_QUIET; T_ASSERT_TRUE(is_proc_in_frozen_list(child_pid, name, sizeof(name)), "Found proc in frozen list");
		T_QUIET; T_EXPECT_EQ_STR(name, "memorystatus_freeze_test", "Proc has correct name");
		/* Set the budget to 0. */
		set_budget_pages_remaining(0);

		/* FREEZER_CONTROL_RESET_STATE */
		ret = memorystatus_control(MEMORYSTATUS_CMD_FREEZER_CONTROL, 0, FREEZER_CONTROL_RESET_STATE, NULL, 0);
		T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "FREEZER_CONRTOL_RESET_STATE");

		/* Verify budget resets to a non-zero value. Some devices may have a configured
		 * budget of 0. Skip this assertion if so. */
		uint64_t budget_after_reset = get_budget_pages_remaining();
#if TARGET_OS_XR
		T_QUIET; T_ASSERT_EQ(budget_after_reset, 0ULL, "freeze budget after reset == 0");
#else
		T_QUIET; T_ASSERT_GT(budget_after_reset, 0ULL, "freeze budget after reset > 0");
#endif
		/*
		 * Verify child has been killed
		 * Note that the task termination isn't synchronous with the RESET_STATE call so we may
		 * block in waitpid temporarily.
		 */
		int stat;
		while (true) {
		        pid_t wait_p = waitpid(child_pid, &stat, 0);
		        if (wait_p == child_pid) {
		                break;
			}
		}
		T_QUIET; T_ASSERT_TRUE(WIFSIGNALED(stat), "child was signaled");
		T_QUIET; T_ASSERT_EQ(WTERMSIG(stat), SIGKILL, "Child received SIGKILL");

		T_END;
	});
	dispatch_main();
}

static void
dock_proc(pid_t pid)
{
	int ret;
	ret = memorystatus_control(MEMORYSTATUS_CMD_ELEVATED_INACTIVEJETSAMPRIORITY_ENABLE, pid, 0, NULL, 0);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "dock_proc");
}

T_DECL(memorystatus_freeze_skip_docked, "memorystatus_freeze_top_process does not freeze docked processes",
	T_META_ENABLED(!TARGET_OS_WATCH),
	T_META_BOOTARGS_SET("freeze_enabled=1"),
	T_META_REQUIRES_SYSCTL_EQ("vm.freeze_enabled", 1),
    T_META_ASROOT(true),
    T_META_REQUIRES_SYSCTL_EQ("kern.development", 1),
	T_META_TAG_VM_NOT_PREFERRED) {
	memorystatus_freeze_top_process_setup();
	enable_ordered_freeze_mode();
	test_after_background_helper_launches(true, "frozen_background", ^{
		int ret, val = 1;
		memorystatus_properties_freeze_entry_v1 entries[1];

		dock_proc(child_pid);
		construct_child_freeze_entry(&entries[0]);
		prepare_proc_for_freezing(child_pid);

		T_LOG("Telling kernel to freeze %s", entries[0].proc_name);
		ret = memorystatus_control(MEMORYSTATUS_CMD_GRP_SET_PROPERTIES, 0, MEMORYSTATUS_FLAGS_GRP_SET_FREEZE_PRIORITY, entries, sizeof(entries));
		T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "MEMORYSTATUS_FLAGS_GRP_SET_FREEZE_PRIORITY");

		ret = sysctlbyname("vm.memorystatus_freeze_top_process", NULL, NULL, &val, sizeof(val));
		T_ASSERT_EQ(errno, ESRCH, "freeze_top_process errno");
		T_ASSERT_EQ(ret, -1, "freeze_top_process");

		verify_proc_not_frozen(child_pid);
		resume_and_kill_proc(child_pid);

		T_END;
	});
	dispatch_main();
}

T_HELPER_DECL(corpse_generation, "Generate a large corpse", T_META_ASROOT(false)) {
	/*
	 * Allocate and fault in a bunch of memory so that it takes a while
	 * to generate our corpse.
	 */
	size_t bytes_to_allocate = 304 * (1UL << 20);
	size_t block_size = 8 * (1UL << 20);
	for (size_t i = 0; i < bytes_to_allocate / block_size; i++) {
		unsigned char *ptr = malloc(block_size);
		if (ptr == NULL) {
			T_LOG("Unable to allocate memory in child process!");
			exit(UNABLE_TO_ALLOCATE);
		}
		for (size_t j = 0; j < block_size / vm_page_size; j++) {
			*ptr = (unsigned char) j;
			ptr += vm_page_size;
		}
	}
	dispatch_source_t ds_signal = dispatch_source_create(DISPATCH_SOURCE_TYPE_SIGNAL, SIGUSR1, 0, dispatch_get_main_queue());
	if (ds_signal == NULL) {
		T_LOG("Unable to create dispatch source");
		exit(DISPATCH_SOURCE_CREATE_FAILED);
	}
	dispatch_source_set_event_handler(ds_signal, ^{
		uint64_t val = 1;
		/*
		 * We should now be frozen.
		 * Simulate a crash so that our P_MEMSTAT_SKIP bit gets set temporarily.
		 * The parent process will try to kill us due to disk space shortage in parallel.
		 */
		os_fault_with_payload(OS_REASON_LIBSYSTEM, OS_REASON_LIBSYSTEM_CODE_FAULT,
		&val, sizeof(val), "freeze_test", 0);
	});
	dispatch_activate(ds_signal);

	sig_t sig_ret = signal(SIGUSR1, SIG_IGN);
	T_QUIET; T_WITH_ERRNO; T_ASSERT_NE(sig_ret, SIG_ERR, "signal(SIGUSR1, SIG_IGN)");

	/* Signal to our parent that we can be frozen */
	if (kill(getppid(), SIGUSR1) != 0) {
		T_LOG("Unable to signal to parent process!");
		exit(SIGNAL_TO_PARENT_FAILED);
	}
	dispatch_main();
}

T_DECL(memorystatus_disable_freeze_corpse, "memorystatus_disable_freeze with parallel corpse creation",
	T_META_BOOTARGS_SET("freeze_enabled=1"),
	T_META_REQUIRES_SYSCTL_EQ("vm.freeze_enabled", 1),
    T_META_ASROOT(true),
    T_META_REQUIRES_SYSCTL_EQ("kern.development", 1),
	T_META_TAG_VM_NOT_PREFERRED) {
	/*
	 * In the past, we've had race conditions w.r.t. killing on disk space shortage
	 * and corpse generation of frozen processes.
	 * This test spwans a frozen helper
	 * which generates a large corpse (which should take in the 10s to 100s of m.s. to complete)
	 * while the test process triggers disk space kills.
	 * We should see that the test process is jetsammed successfully.
	 */
	test_after_background_helper_launches(false, "corpse_generation", ^{
		int ret, val, stat;
		/* Place the child in the idle band so that it gets elevated like a typical app. */
		move_to_idle_band(child_pid);
		ret = pid_suspend(child_pid);
		T_ASSERT_POSIX_SUCCESS(ret, "child suspended");
		freeze_process(child_pid);

		ret = pid_resume(child_pid);
		T_ASSERT_POSIX_SUCCESS(ret, "child resumed after freeze");

		kill(child_pid, SIGUSR1);

		T_ATEND(enable_freeze);
		val = 0;
		ret = sysctlbyname("vm.freeze_enabled", NULL, NULL, &val, sizeof(val));
		T_ASSERT_POSIX_SUCCESS(ret, "freeze disabled");
		/*
		 * Verify child has been killed
		 * Note that the task termination isn't synchronous with the freeze_enabled call so we may
		 * block in waitpid temporarily.
		 */
		while (true) {
		        pid_t wait_p = waitpid(child_pid, &stat, 0);
		        if (wait_p == child_pid) {
		                break;
			}
		}
		T_QUIET; T_ASSERT_TRUE(WIFSIGNALED(stat), "child was signaled");
		T_QUIET; T_ASSERT_EQ(WTERMSIG(stat), SIGKILL, "Child received SIGKILL");

		T_END;
	});
	dispatch_main();
}
