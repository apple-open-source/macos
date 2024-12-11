#include <darwintest.h>
#include <darwintest_multiprocess.h>

#include <notify.h>

// The sender is entitled to post "com.apple.private.restrict-post.random.good" only.
// The sender tries to post a bad notification followed by a good notification.
// The receiver should only be notified the good notification.

#define APPLE_RESTRICT_PREFIX "com.apple.private.restrict-post."
#define GOOD_KEY "random.good"
#define BAD_KEY "random.bad"
#define POSTED_VALUE 42

T_DECL(notify_restrict_post,
       "Make sure the notifications with restrict prefix can only be posted by entitled process")
{
    uint32_t status;
    const char *good_name = APPLE_RESTRICT_PREFIX GOOD_KEY;
    const char *bad_name = APPLE_RESTRICT_PREFIX BAD_KEY;
    int good_token, bad_token, dc;

    // Receiver
    dispatch_queue_t dq = dispatch_queue_create(NULL, 0);
    dispatch_semaphore_t sema = dispatch_semaphore_create(0);
    status = notify_register_dispatch(bad_name, &bad_token, dq, ^(int token) {
        T_FAIL("Received bad notification (from unentitled process)");
    });
    T_ASSERT_EQ(status, NOTIFY_STATUS_OK, "notify_register_dispatch on bad notification");

    status = notify_register_dispatch(good_name, &good_token, dq, ^(int token) {
        uint32_t status2;
        uint64_t state = 0;
        int check = 1;

        status2 = notify_get_state(token, &state);
        T_ASSERT_EQ(status2, NOTIFY_STATUS_OK, "notify_get_state on good notification");
        T_ASSERT_EQ(state, POSTED_VALUE, "Received good notification with correct state");

        status2 = notify_check(bad_token, &check);
        T_ASSERT_EQ(status2, NOTIFY_STATUS_OK, "notify_check on bad notification");
        T_ASSERT_EQ(check, 0, "Nobody posted bad notification");

        status2 = notify_get_state(bad_token, &state);
        T_ASSERT_EQ(status2, NOTIFY_STATUS_OK, "notify_get_state on bad notification");
        T_ASSERT_NE(state, POSTED_VALUE, "Nobody set state of bad notification");

        dispatch_semaphore_signal(sema);
    });
    T_ASSERT_EQ(status, NOTIFY_STATUS_OK, "notify_register_dispatch on good notification");

    // Sender
    status = notify_check(bad_token, &dc);
    T_QUIET; T_ASSERT_EQ(status, NOTIFY_STATUS_OK, "initial notify_check");
    status = notify_set_state(bad_token, 42);
    T_QUIET; T_ASSERT_EQ(status, NOTIFY_STATUS_OK, "Today notify_set_state always return OK");
    status = notify_post(bad_name);
    T_QUIET; T_ASSERT_EQ(status, NOTIFY_STATUS_OK, "Today notify_post always return OK");

    status = notify_set_state(good_token, 42);
    T_ASSERT_EQ(status, NOTIFY_STATUS_OK, "notify_set_state on good notification");
    status = notify_post(good_name);
    T_ASSERT_EQ(status, NOTIFY_STATUS_OK, "notify_post on good notification");

    dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
}
