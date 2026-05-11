/*
 * rdar://172291342: Verify that notify_register_check does not leak
 * server-side registrations when shm_attach fails due to sandbox.
 *
 * Spawns a helper process notify_shm_deny_test_helper.
 *
 * With the fix: helper survives all iterations (server registrations
 * are canceled on shm_attach failure).
 * Without the fix: notifyd SIGKILL's the helper around iteration 10k.
 */

#include <darwintest.h>
#include <spawn.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

T_DECL(notify_register_check_shm_deny,
       "rdar://172291342: no server-side registration leak when shm is denied",
       T_META("as_root", "false"),
	   T_META_ENABLED(TARGET_OS_OSX), // profile compilation not supported on embedded platform
       T_META_TIMEOUT(120))
{
	pid_t pid = -1;
	int ret;
	posix_spawn_file_actions_t file_actions;

	ret = posix_spawn_file_actions_init(&file_actions);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "posix_spawn_file_actions_init");

	ret = posix_spawn_file_actions_adddup2(&file_actions,
					       STDOUT_FILENO, STDOUT_FILENO);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(ret, "posix_spawn_file_actions_adddup2");

	char *child_args[] = {
		"/AppleInternal/Tests/Libnotify/notify_shm_deny_test_helper",
		NULL
	};

	ret = posix_spawn(&pid, child_args[0], &file_actions, NULL,
			  child_args, environ);
	T_ASSERT_POSIX_SUCCESS(ret, "spawned notify_shm_deny_test_helper");

	posix_spawn_file_actions_destroy(&file_actions);

	int status;
	pid_t waited = waitpid(pid, &status, 0);
	T_ASSERT_NE(waited, (pid_t)-1, "waitpid succeeded");

	if (WIFSIGNALED(status)) {
		T_ASSERT_FAIL("Helper killed by signal %d (%s) — "
			      "server-side registrations likely leaked",
			      WTERMSIG(status),
			      WTERMSIG(status) == SIGKILL ? "SIGKILL" : "other");
	}

	T_ASSERT_TRUE(WIFEXITED(status), "helper exited normally");
	T_ASSERT_EQ(WEXITSTATUS(status), 0, "helper exit code ok");
}
