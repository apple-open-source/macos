#include <stdio.h>
#include <stdlib.h>
#include <mach/mach.h>
#include <mach/mach_port.h>
#include <sys/code_signing.h>
#include <sys/sysctl.h>
#include <darwintest.h>

T_GLOBAL_META(
	T_META_NAMESPACE("xnu.ipc"),
	T_META_RUN_CONCURRENTLY(TRUE),
	T_META_RADAR_COMPONENT_NAME("xnu"),
	T_META_RADAR_COMPONENT_VERSION("IPC"),
	T_META_TAG_VM_PREFERRED);

#define countof(x) (sizeof(x) / sizeof(x[0]))

static void
expect_sigkill(
	void (^fn)(void),
	const char *description)
{
	pid_t pid = fork();
	T_QUIET; T_ASSERT_POSIX_SUCCESS(pid, "fork");

	if (pid == 0) {
		fn();
		T_ASSERT_FAIL("%s: did not receive SIGKILL", description);
	} else {
		int status = 0;
		T_QUIET; T_ASSERT_POSIX_SUCCESS(waitpid(pid, &status, 0), "waitpid");
		T_EXPECT_EQ(WTERMSIG(status), SIGKILL,
		    "%s exited with %d, expect SIGKILL", description, WTERMSIG(status));
	}
}

T_DECL(mach_port_construct_at_most_one,
    "mach_port_construct at most one flag policy")
{
	/* verify our at most one flag rule is enforced */
	const uint32_t at_most_one_flags[] = {
		MPO_REPLY_PORT,
		MPO_CONNECTION_PORT,
		MPO_SERVICE_PORT,
		MPO_PROVISIONAL_REPLY_PORT,
		MPO_EXCEPTION_PORT,
		MPO_CONNECTION_PORT_WITH_PORT_ARRAY
	};


	for (uint32_t i = 0; i < countof(at_most_one_flags) - 1; ++i) {
		for (uint32_t j = i + 1; j < countof(at_most_one_flags); ++j) {
			mach_port_t port;

			mach_port_options_t opts = {
				.flags = at_most_one_flags[i] | at_most_one_flags[j]
			};

			kern_return_t kr = mach_port_construct(mach_task_self(), &opts, 0x0, &port);
			T_ASSERT_MACH_ERROR(kr,
			    KERN_INVALID_ARGUMENT, "mach_port_construct failed for at most one flags");
		}
	}
}

T_DECL(mach_port_construct_invalid_arguments_and_values,
    "mach_port_construct invalid arguments and values")
{
	kern_return_t kr;
	mach_port_t port;

	mach_port_options_t conn_opts = {
		.flags = MPO_CONNECTION_PORT,
		.service_port_name = 0x0
	};

	kr = mach_port_construct(mach_task_self(), &conn_opts, 0x0, &port);
	T_ASSERT_MACH_ERROR(kr,
	    KERN_INVALID_ARGUMENT,
	    "MPO_CONNECTION_PORT failed on service_port_name");

	conn_opts.service_port_name = MPO_ANONYMOUS_SERVICE;

	kr = mach_port_construct(mach_task_self(), &conn_opts, 0x0, &port);
	T_ASSERT_MACH_SUCCESS(kr, "MPO_CONNECTION_PORT succeeds with anonymous service name");
	kr = mach_port_destruct(mach_task_self(), port, 0, 0);
	T_ASSERT_MACH_SUCCESS(kr, "destroy anonymous service name");

	mach_port_options_t qlimit_opts = {
		.flags = MPO_QLIMIT,
		.mpl.mpl_qlimit = MACH_PORT_QLIMIT_MAX + 1
	};

	kr = mach_port_construct(mach_task_self(), &qlimit_opts, 0x0, &port);
	T_ASSERT_MACH_ERROR(kr,
	    KERN_INVALID_VALUE,
	    "MPO_QLIMIT failed on invalid value");

	/* Enumerate on all unknown MPO flags */
	mach_port_options_t unknown_flags_opts;
	for (uint32_t i = 0; i < sizeof(unknown_flags_opts.flags) * CHAR_BIT; ++i) {
		unknown_flags_opts.flags = MPO_UNUSED_BITS & (1 << i);

		if (unknown_flags_opts.flags != 0) {
			kr = mach_port_construct(mach_task_self(), &unknown_flags_opts, 0x0, &port);
			T_ASSERT_MACH_ERROR(kr,
			    KERN_INVALID_ARGUMENT,
			    "Unknown MPO flags 0x%x failed with KERN_INVALID_ARGUMENT",
			    unknown_flags_opts.flags);
		}
	}
}

T_DECL(mach_port_construct_fatal_failure,
    "mach_port_construct kern defined fatal failures",
    T_META_IGNORECRASHES(".*mach_port_construct_errors.*"),
    T_META_ENABLED(!TARGET_OS_OSX && !TARGET_OS_BRIDGE))
{
	expect_sigkill(^{
		mach_port_t port;
		mach_port_options_t opts = {
		        .flags = MPO_CONNECTION_PORT_WITH_PORT_ARRAY
		};
		(void)mach_port_construct(mach_task_self(), &opts, 0x0, &port);
	}, "passing MPO_CONNECTION_PORT_WITH_PORT_ARRAY without entitlement");
}

T_DECL(mach_port_construct_kern_denied,
    "mach_port_construct kern defined failures",
    T_META_TAG_VM_PREFERRED,
    T_META_ENABLED(!TARGET_OS_OSX && !TARGET_OS_BRIDGE))
{
	kern_return_t kr;
	mach_port_t port;
	mach_port_options_t opts;

	/*
	 * should fail because only TASK_GRAPHICS_SERVER is allowed to
	 * use MPO_TG_BLOCK_TRACKING.
	 */
	opts.flags = MPO_TG_BLOCK_TRACKING;

	kr = mach_port_construct(mach_task_self(), &opts, 0x0, &port);
	T_ASSERT_MACH_ERROR(kr,
	    KERN_DENIED, "MPO_TG_BLOCK_TRACKING failed with KERN_DENIED");
}
