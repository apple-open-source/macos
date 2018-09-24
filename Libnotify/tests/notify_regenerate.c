#include <stdlib.h>
#include <notify.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <darwintest.h>
#include <signal.h>
#include "../libnotify.h"

#define T_ASSERT_GROUP_WAIT(g, timeout, ...)  ({ \
		dispatch_time_t _timeout = dispatch_time(DISPATCH_TIME_NOW, \
				(uint64_t)(timeout * NSEC_PER_SEC)); \
		T_ASSERT_FALSE(dispatch_group_wait(g, _timeout), __VA_ARGS__); \
	})

T_DECL(notify_regenerate, "Make sure regenerate registrations works",
		T_META("owner", "Core Darwin Daemons & Tools"),
		T_META_ASROOT(YES))
{
	const char *KEY = "com.apple.notify.test.regenerate";
	int v_token, n_token, rc;
	pid_t old_pid, new_pid;
	uint64_t state;

	dispatch_queue_t dq = dispatch_queue_create_with_target("q", NULL, NULL);
	dispatch_group_t dg = dispatch_group_create();

	T_LOG("Grab the current instance pid & version");
	{
		rc = notify_register_check(NOTIFY_IPC_VERSION_NAME, &v_token);
		T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "register_check(NOTIFY_IPC_VERSION_NAME)");

		state = ~0ull;
		rc = notify_get_state(v_token, &state);
		T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "notify_get_state(NOTIFY_IPC_VERSION_NAME)");

		old_pid = (pid_t)(state >> 32);
		T_EXPECT_EQ((uint32_t)state, NOTIFY_IPC_VERSION, "IPC version should be set");
	}

	T_LOG("Register for our test topic, and check it works");
	{
		rc = notify_register_dispatch(KEY, &n_token, dq, ^(int token){
			// if we crash here, it means we got a spurious post
			// e.g. if you run the test twice concurrently this could happen
			dispatch_group_leave(dg);
		});
		T_EXPECT_EQ(rc, NOTIFY_STATUS_OK, "register dispatch should work");

		dispatch_group_enter(dg);
		notify_post(KEY);
		T_ASSERT_GROUP_WAIT(dg, 5., "we received our own notification");
	}

	T_LOG("Make sure notifyd changes pid due to a kill");
	{
		state = ~0ull;
		rc = notify_get_state(v_token, &state);
		T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "notify_get_state(NOTIFY_IPC_VERSION_NAME)");

		new_pid = (pid_t)(state >> 32);
		T_ASSERT_EQ(old_pid, new_pid, "Pid should not have changed yet");

		rc = kill(old_pid, SIGKILL);
		T_ASSERT_POSIX_SUCCESS(rc, "Killing notifyd");

		new_pid = old_pid;
		for (int i = 0; i < 10 && new_pid == old_pid; i++) {
			usleep(100000); /* wait for .1s for notifyd to die */
			state = ~0ull;
			rc = notify_get_state(v_token, &state);
			T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "notify_get_state(NOTIFY_IPC_VERSION_NAME)");
			new_pid = (pid_t)(state >> 32);
		}
		T_ASSERT_NE(old_pid, new_pid, "Pid should have changed within 1s");
	}

	T_LOG("Make sure our old registration works");
	{
		dispatch_group_enter(dg);
		notify_post(KEY);
		T_ASSERT_GROUP_WAIT(dg, 5., "we received our own notification");
	}
}
