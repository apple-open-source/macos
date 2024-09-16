//
//  notify_register_mach_port.c
//  Libnotify
//
//  Created by Brycen Wershing on 10/23/19.
//

#include <stdlib.h>
#include <notify.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <darwintest.h>
#include <signal.h>
#include "../libnotify.h"
#include "notify_private.h"
#include <spawn.h>
#include <spawn_private.h>
#include <mach-o/dyld.h>
#include <TargetConditionals.h>

#include "rnserver.h"

T_GLOBAL_META(
	T_META_ENVVAR("DarwinNotificationLogging=1")
);

extern char **environ;
static bool has_port_been_notified_1, has_port_been_notified_2;
static int key_1_token, key_2_token;

static void port_handler(mach_port_t port)
{
	int tid;
	mach_msg_empty_rcv_t msg;
	kern_return_t status;

	if (port == MACH_PORT_NULL) return;

	memset(&msg, 0, sizeof(msg));
	status = mach_msg(&msg.header, MACH_RCV_MSG, 0, sizeof(msg), port, 0, MACH_PORT_NULL);
	if (status != KERN_SUCCESS) return;

	tid = msg.header.msgh_id;
	if (tid == key_1_token)
	{
		has_port_been_notified_1 = true;
	}
	else if (tid == key_2_token)
	{
		has_port_been_notified_2 = true;
	}
	else
	{
		T_FAIL("port handler should only be called with tokens %d and %d, but it was called with %d",
		       key_1_token, key_2_token, tid);
	}
}

static void run_test()
{
	const char *KEY1 = "com.apple.notify.test.mach_port.1";
	const char *KEY2 = "com.apple.notify.test.mach_port.2";
	int rc;

    // Make sure that posts without an existing registration don't do anything
    // bad, like crash notifyd or break future posts
    notify_post(KEY1);
    notify_post(KEY1);
    notify_post(KEY1);

	mach_port_t watch_port;
	dispatch_source_t port_src;
	dispatch_queue_t watch_queue = dispatch_queue_create("Watch Q", NULL);

	rc = notify_register_mach_port(KEY1, &watch_port, 0, &key_1_token);
	T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "register mach port should work");

	rc = notify_register_mach_port(KEY2, &watch_port, NOTIFY_REUSE, &key_2_token);
	T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "register mach port should work");

	port_src = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, watch_port, 0, watch_queue);
	dispatch_source_set_event_handler(port_src, ^{
		port_handler(watch_port);
	});
	dispatch_activate(port_src);

	has_port_been_notified_1 = false;
	has_port_been_notified_2 = false;
	notify_post(KEY1);
	sleep(1);
	T_ASSERT_EQ(has_port_been_notified_1, true, "mach port 1 should receive notification");
	T_ASSERT_EQ(has_port_been_notified_2, false, "mach port 2 should not receive notification");

	has_port_been_notified_1 = false;
	has_port_been_notified_2 = false;
	notify_post(KEY2);
	sleep(1);
	T_ASSERT_EQ(has_port_been_notified_1, false, "mach port 1 should not receive notification");
	T_ASSERT_EQ(has_port_been_notified_2, true, "mach port 2 should receive notification");

	dispatch_release(port_src);

}

T_DECL(notify_register_mach_port, "Make sure mach port registrations works",
		T_META("owner", "Core Darwin Daemons & Tools"),
       T_META_ASROOT(YES))
{
	run_test();
}

T_DECL(notify_register_mach_port_filtered, "Make sure mach port registrations works",
		T_META("owner", "Core Darwin Daemons & Tools"),
       T_META_ASROOT(YES))
{
	notify_set_options(NOTIFY_OPT_DISPATCH | NOTIFY_OPT_REGEN | NOTIFY_OPT_FILTERED);

	run_test();
}

#define MACH_PORT_HARD_LIMIT 128
#define PORT_TRACKED_TEST_NOTIFICATION "com.apple.libnotify.port_tracked_test"
#define PORT_TRACKED_TEST_BEGIN       0
#define PORT_TRACKED_TEST_CHILD_READY 1
#define PORT_TRACKED_TEST_CHILD_BEGIN 2

// from rnserver.h
typedef union __RequestUnion__receive_resource_notify_subsystem resource_notification_t;

T_HELPER_DECL(notify_mach_port_leaks_helper,
              "Helper with hard portlimit that registers and cancels port based notification")
{
    char *key;
    int i, rc, token, sync_token;
    mach_port_t watch_port;

    for (i = 0; i < MACH_PORT_HARD_LIMIT * 16; i++) {
        // Create a unique name per iteration otherwise libnotify
        // will coalesce the notifications and not create new mach
        // ports per registration, rendering this test useless
        rc = asprintf(&key, "com.apple.notify.test.mach_port_leaks.%d", i);
        T_QUIET; T_ASSERT_NE(rc, -1, "created key");

        watch_port = MACH_PORT_NULL;
        rc = notify_register_mach_port(key, &watch_port, 0, &token);
        T_QUIET; T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "register mach port for notification %s", key);

        rc = notify_cancel(token);
        T_QUIET; T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "cancelling registered mach port notification %s", key);

        free(key);
        key = NULL;
    }
}

T_DECL(notify_mach_port_leaks,
       "Make sure notify_register_mach_port() does not leak ports")
{
#if TARGET_OS_BRIDGE
    T_SKIP("Setting process resource limit is not available on this platform.");
#endif

    int status, err;
    pid_t pid, child_pid;
    posix_spawnattr_t attrs;
    mach_port_t rn_port, client_task_port;

    // (1) Spawn a child that registers for notifications using mach ports and immediately cancels. The
    //     child is spawned suspended and with a hard port limit that will generate a notification
    //     when the number of ports in the child's IPC space exceeds the limit.
    char path[PATH_MAX];
    uint32_t path_size = sizeof(path);
    T_QUIET; T_ASSERT_POSIX_ZERO(_NSGetExecutablePath(path, &path_size), "_NSGetExecutablePath");
    char *child_args[] = { path, "-n", "notify_mach_port_leaks_helper", NULL };

    posix_spawnattr_init(&attrs);
    err = posix_spawnattr_setflags(&attrs, POSIX_SPAWN_START_SUSPENDED);
    T_QUIET; T_ASSERT_POSIX_SUCCESS(err, "posix_spawnattr_setflags");

    err = posix_spawnattr_set_portlimits_ext(&attrs, 0, MACH_PORT_HARD_LIMIT);
    T_QUIET; T_ASSERT_POSIX_SUCCESS(err, "set hard port limit for helper to %d", MACH_PORT_HARD_LIMIT);

    err = posix_spawn(&child_pid, child_args[0], NULL, &attrs, &child_args[0], environ);
    T_QUIET; T_ASSERT_POSIX_SUCCESS(err, "posix_spawn notify_mach_port_tracked_helper");

    // (2) The parent registers for the port limit notification and sets up a dispatch source
    //     to monitor it for any notifications while we wait for the child to exit.
    err = mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &rn_port);
    T_QUIET; T_ASSERT_MACH_SUCCESS(err, "mach_port_allocate");

    err = mach_port_insert_right(mach_task_self(), rn_port, rn_port, MACH_MSG_TYPE_MAKE_SEND);
    T_QUIET; T_ASSERT_MACH_SUCCESS(err, "mach_port_insert_right");

    err = task_for_pid(mach_task_self(), child_pid, &client_task_port);
    T_QUIET; T_ASSERT_MACH_SUCCESS(err, "task_for_pid");

    err = task_set_special_port(client_task_port, TASK_RESOURCE_NOTIFY_PORT, rn_port);
    T_QUIET; T_ASSERT_MACH_SUCCESS(err, "task_set_special_port");

    dispatch_queue_t rnq = dispatch_queue_create("resource_notifications", DISPATCH_QUEUE_SERIAL);
    T_QUIET; T_ASSERT_NOTNULL(rnq, "created dispatch queue");

    dispatch_source_t rns = dispatch_source_create(DISPATCH_SOURCE_TYPE_MACH_RECV, rn_port, 0, rnq);
    T_QUIET; T_ASSERT_NOTNULL(rns, "created dispatch source");

    dispatch_source_set_event_handler(rns, ^{
        mach_msg_return_t mr;

        mr = dispatch_mig_server(rns, sizeof(resource_notification_t), resource_notify_server);
        T_QUIET; T_ASSERT_MACH_SUCCESS(mr, "dispatch_mig_server()");
    });
    dispatch_activate(rns);

    // (3) Signal the child to unsuspend it and wait for it to exit cleanly. If the child leaks
    //     mach ports, the resource notification is handled in receive_port_space_violation().
    err = kill(child_pid, SIGCONT);
    T_QUIET; T_ASSERT_POSIX_SUCCESS(err, "unsuspend child");


    pid = waitpid(child_pid, &status, 0);
    T_QUIET; T_ASSERT_EQ(pid, child_pid, "child exists");

    // (4) Flush the resource notification queue to prevent any race between notification and test end
    dispatch_sync(rnq, ^{
        T_QUIET; T_ASSERT_EQ(WIFEXITED(status), 1, "child exited cleanly");
        T_END;
    });
}

kern_return_t
receive_port_space_violation(__unused mach_port_t receiver,
    __unused proc_name_t procname,
    __unused pid_t pid,
    __unused mach_timespec_t timestamp,
    int64_t observed_ports,
    int64_t ports_allowed,
    __unused mach_port_t fatal_port,
    __unused resource_notify_flags_t flags)
{
    T_LOG("Received a notification on the resource notify port");
    T_LOG("Ports_allowed = %lld, observed_ports = %lld", ports_allowed, observed_ports);
    if (fatal_port) {
        mach_port_deallocate(mach_task_self(), fatal_port);
    }
    T_FAIL("port leak detected");
    return KERN_SUCCESS;
}

// MARK: - MIG Compatibility
// All the functions below exist to compile with xnu's resource_notify.defs and
// can be ignored for the purposes of this test.
kern_return_t
receive_cpu_usage_violation(__unused mach_port_t receiver,
    __unused proc_name_t procname,
    __unused pid_t pid,
    __unused posix_path_t killed_proc_path,
    __unused mach_timespec_t timestamp,
    __unused int64_t observed_cpu_nsecs,
    __unused int64_t observation_nsecs,
    __unused int64_t cpu_nsecs_allowed,
    __unused int64_t limit_window_nsecs,
    __unused resource_notify_flags_t flags)
{
    T_LOG("Inside receive_cpu_usage_violation");
    return KERN_SUCCESS;
}

kern_return_t
receive_cpu_wakes_violation(__unused mach_port_t receiver,
    __unused proc_name_t procname,
    __unused pid_t pid,
    __unused posix_path_t killed_proc_path,
    __unused mach_timespec_t timestamp,
    __unused int64_t observed_cpu_wakes,
    __unused int64_t observation_nsecs,
    __unused int64_t cpu_wakes_allowed,
    __unused int64_t limit_window_nsecs,
    __unused resource_notify_flags_t flags)
{
    T_LOG("Inside receive_cpu_wakes_violation");
    return KERN_SUCCESS;
}

kern_return_t
receive_disk_writes_violation(__unused mach_port_t receiver,
    __unused proc_name_t procname,
    __unused pid_t pid,
    __unused posix_path_t killed_proc_path,
    __unused mach_timespec_t timestamp,
    __unused int64_t observed_bytes_dirtied,
    __unused int64_t observation_nsecs,
    __unused int64_t bytes_dirtied_allowed,
    __unused int64_t limit_window_nsecs,
    __unused resource_notify_flags_t flags)
{
    T_LOG("Inside receive_disk_writes_violation");
    return KERN_SUCCESS;
}

kern_return_t
receive_file_descriptors_violation(__unused mach_port_t receiver,
    __unused proc_name_t procname,
    __unused pid_t pid,
    __unused mach_timespec_t timestamp,
    __unused int64_t observed_filedesc,
    __unused int64_t filedesc_allowed,
    __unused mach_port_t fatal_port,
    __unused resource_notify_flags_t flags)
{
    T_LOG("Inside receive_file_descriptors_violation");
    return KERN_SUCCESS;
}
