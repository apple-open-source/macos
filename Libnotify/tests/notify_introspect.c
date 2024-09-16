#include <darwintest.h>
#include <darwintest_multiprocess.h>

#include <notify.h>

#define EXEMPT_KEY1   "test.notification.exempt.1"
#define EXEMPT_KEY2   "test.notification.exempt.2"
#define EXEMPT_PREFIX "test.notification.exempt.prefix."
#define NONEXEMPT_KEY "test.notification.non-exempt"

static void
do_notify_introspect_exempt_test(const char *key1, const char *key2)
{
	uint32_t rc;
	int c_token, d_token;
	uint64_t state = 0;
	dispatch_semaphore_t sema = dispatch_semaphore_create(0);
	dispatch_queue_t dq = dispatch_queue_create_with_target("q", NULL, NULL);

	rc = notify_register_check(key1, &c_token);
	T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "notify_register_check successful");

	rc = notify_set_state(c_token, 0xDEADBEEF);
	T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "notify_get_state successful");

	rc = notify_get_state(c_token, &state);
	T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "notify_get_state successful");
	T_ASSERT_EQ(state, 0xDEADBEEF, "notify_get_state successful");

	rc = notify_register_dispatch(key2, &d_token, dq, ^(int token) {
		dispatch_semaphore_signal(sema);
	});

	notify_post(key2);
	dispatch_semaphore_wait(sema, DISPATCH_TIME_FOREVER);
}

T_DECL(notify_introspect_exempt, "notify_introspect_exempt")
{
	do_notify_introspect_exempt_test(EXEMPT_KEY1, EXEMPT_KEY2);
}

T_DECL(notify_introspect_exempt_prefix, "notify_introspect_exempt_prefix")
{
	do_notify_introspect_exempt_test(EXEMPT_PREFIX "random" , EXEMPT_PREFIX "1");
}

T_DECL(notify_introspect_non_exempt,
		"notify_introspect_non_exempt",
		T_META_IGNORECRASHES("notify_introspect_non_exempt*"))
{
	uint32_t rc;
	int c_token;
	uint64_t state = 0;

	rc = notify_register_check(NONEXEMPT_KEY, &c_token);
	T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "notify_register_check successful");

	rc = notify_set_state(c_token, 0xDEADBEEF);
	T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "notify_get_state successful");

	rc = notify_get_state(c_token, &state);
	T_ASSERT_EQ(rc, NOTIFY_STATUS_OK, "notify_get_state successful");
	T_ASSERT_EQ(state, 0xDEADBEEF, "notify_get_state successful");
}
