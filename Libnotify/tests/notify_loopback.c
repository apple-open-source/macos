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

#define KEY "com.apple.notify.test-loopback"

T_HELPER_DECL(notify_loopback_helper,
	   "notify loopback helper",
	   T_META("as_root", "true"),
	   T_META_TIMEOUT(50))
{
	uint32_t rc;
    int c_token, d_token, check;
    uint64_t state = 0;
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
	dispatch_queue_t dq = dispatch_queue_create_with_target("q", NULL, NULL);
    
    // Simulate WebContent notify options but we enter a
    // sandbox here since that would only work on macOS
	notify_set_options(NOTIFY_OPT_DISPATCH | NOTIFY_OPT_REGEN | NOTIFY_OPT_FILTERED | NOTIFY_OPT_LOOPBACK);

	rc = notify_register_check(KEY, &c_token);
	T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "notify_register_check successful");

    rc = notify_check(c_token, &check);
    T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "notify_check successful");
    T_ASSERT_EQ(check, 1, "notify_check successful");

    rc = notify_check(c_token, &check);
    T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "notify_check successful");
    T_ASSERT_EQ(check, 0, "notify_check successful");

    rc = notify_get_state(c_token, &state);
    T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "notify_get_state successful");
    T_ASSERT_TRUE(state != 0xAAAAAAAA, "matches expected state 0x%x", state);

	rc = notify_register_dispatch(KEY, &d_token, dq, ^(int token){
		uint64_t state;
		uint32_t status;

		status = notify_get_state(token, &state);
		T_ASSERT_EQ(status, NOTIFY_STATUS_OK, "notify_get_state successful");
		T_ASSERT_TRUE(state == 0xDEADBEEF, "matches expected state 0x%x", state);

        dispatch_semaphore_signal(sema);
	});
	T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "notify_register_dispatch successful");

	sleep(1);

	rc = notify_set_state(d_token, 0xDEADBEEF);
	T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "notify_set_state successful");

	rc = notify_post(KEY);
	T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "notify_post successful");

    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);

	rc = notify_cancel(c_token);
	T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "notify_cancel successful");

	rc = notify_cancel(d_token);
	T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "notify_cancel successful");
}

T_DECL(notify_loopback,
       "notify loopback",
       T_META("as_root", "true"),
       T_META_TIMEOUT(50))
{
    int token = NOTIFY_TOKEN_INVALID;
    uint32_t rc;
    uint64_t state = 0;

    rc = notify_register_check(KEY, &token);
    T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "notify_register_check successful");

    rc = notify_set_state(token, 0xAAAAAAAA);
    T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "notify_set_state successful");

    dt_helper_t helpers[1];
    helpers[0] = dt_child_helper("notify_loopback_helper");
    dt_run_helpers(helpers, 1, 60);

    rc = notify_get_state(token, &state);
    T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "notify_get_state successful");
    T_ASSERT_TRUE(state == 0xAAAAAAAA, "matches expected state 0x%x", state);
}
