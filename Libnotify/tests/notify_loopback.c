#include <darwintest.h>
#include <darwintest_multiprocess.h>
#include <notify.h>
#include <notify_private.h>
#include <dispatch/dispatch.h>
#include <stdlib.h>
#include <unistd.h>
#include <sandbox.h>
#include <TargetConditionals.h>
#include <stdio.h>
#include <mach/mach_init.h>
#include <mach/task_special_ports.h>
#include <spawn.h>
#include "../libnotify.h"
#include <signal.h>

#define KEY "com.apple.notify.test-loopback"

extern char **environ;

static void
kill_notifyd()
{
	uint32_t rc;
	uint64_t state;
	pid_t pid;
	int v_token;

	rc = notify_register_check(NOTIFY_IPC_VERSION_NAME, &v_token);
	T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "register_check(NOTIFY_IPC_VERSION_NAME)");

	state = ~0ull;
	rc = notify_get_state(v_token, &state);
	T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "notify_get_state(NOTIFY_IPC_VERSION_NAME)");

	pid = (pid_t)(state >> 32);
	rc = kill(pid, SIGKILL);
	T_ASSERT_POSIX_SUCCESS(rc, "Killing notifyd");
}

T_DECL(notify_loopback,
       "notify loopback",
	   T_META_ASROOT(true),
       T_META_TIMEOUT(50))
{
    int token = NOTIFY_TOKEN_INVALID, ret, sig;
    uint32_t rc;
    uint64_t state = 0;
	posix_spawn_file_actions_t file_actions;

    rc = notify_register_check(KEY, &token);
    T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "notify_register_check successful");

    rc = notify_set_state(token, 0xAAAAAAAA);
    T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "notify_set_state successful");

	ret = posix_spawn_file_actions_init(&file_actions);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "posix_spawn_file_actions_init");

	ret = posix_spawn_file_actions_adddup2(&file_actions, STDOUT_FILENO, STDOUT_FILENO);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "posix_spawn_file_actions_adddup2");

	char self_pid[16];
	snprintf(self_pid, 16, "%d", getpid());
	char *child_args[] = { "/AppleInternal/Tests/Libnotify/notify_loopback_test_helper", self_pid, NULL };
	pid_t pid = -1;
	ret = posix_spawn(&pid, child_args[0], &file_actions, NULL, &child_args[0], environ);
	T_ASSERT_POSIX_SUCCESS(ret, "notify loopback test spawned successfully");

	ret = posix_spawn_file_actions_destroy(&file_actions);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "posix_spawn_file_actions_destroy");

	sleep(5);

	rc = notify_get_state(token, &state);
	T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "notify_get_state successful");
	T_ASSERT_TRUE(state == 0xAAAAAAAA, "matches expected state 0x%x", state);

	kill_notifyd();
	sleep(5);
	ret = kill(pid, SIGUSR1);
	T_ASSERT_EQ(ret, 0, "signalled pid %d with SIGUSR1 successfully", pid);

	int status;
	pid = waitpid(pid, &status, 0);
	T_ASSERT_NE(pid, -1, "wait succeeded");
	T_ASSERT_FALSE(!WIFEXITED(status), "helper was not signalled");
	T_ASSERT_EQ(WEXITSTATUS(status), 0, "helper exited cleanly");
}
