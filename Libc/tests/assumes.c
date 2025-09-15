#define OS_CRASH_ENABLE_EXPERIMENTAL_LIBTRACE 1
#include <os/assumes.h>

#include <darwintest.h>

static const char *_LIBC_CSTR expected_message = NULL;

static void
os_crash_function(const char *message)
{
	if (expected_message) {
		T_ASSERT_EQ_STR(message, expected_message, NULL);
		T_END;
	} else {
		T_PASS("Got crash message: %s", message);
		T_END;
	}
}
os_crash_redirect(os_crash_function);

#if __has_feature(bounds_attributes)
T_DECL(os_crash_sanity_fbounds_safety, "sanity check for os_crash")
#else
T_DECL(os_crash_sanity, "sanity check for os_crash")
#endif
{
	expected_message = "My AWESOME assertion message.";
	os_crash(expected_message);
}

extern int two;
int two = 2;

#if __has_feature(bounds_attributes)
T_DECL(os_assert_no_msg_fbounds_safety, "sanity check for os_assert w/o a message")
#else
T_DECL(os_assert_no_msg, "sanity check for os_assert w/o a message")
#endif
{
	expected_message = "assertion failure: \"two + two == 5\" -> %llu";
	os_assert(two + two == 5);
}

#if __has_feature(bounds_attributes)
T_DECL(os_assert_zero_no_msg_fbounds_safety, "sanity check for os_assert_zero w/o a message")
#else
T_DECL(os_assert_zero_no_msg, "sanity check for os_assert_zero w/o a message")
#endif
{
	expected_message = "assertion failure: \"two + two\" -> %llu";
	os_assert_zero(two + two);
}

#define DOGMA "Today, we celebrate the first glorious anniversary of the Information Purification Directives."
#if __has_feature(bounds_attributes)
T_DECL(os_assert_msg_fbounds_safety, "sanity check for os_assert with a message")
#else
T_DECL(os_assert_msg, "sanity check for os_assert with a message")
#endif
{
	expected_message = "assertion failure: " DOGMA;
	os_assert(two + two == 5, DOGMA);
}

#if __has_feature(bounds_attributes)
T_DECL(os_assert_zero_msg_fbounds_safety, "sanity check for os_assert_zero with a message")
#else
T_DECL(os_assert_zero_msg, "sanity check for os_assert_zero with a message")
#endif
{
	expected_message = "assertion failure (%llu != 0): " DOGMA;
	os_assert_zero(two + two, DOGMA);
}
