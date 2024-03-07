#define OS_CRASH_ENABLE_EXPERIMENTAL_LIBTRACE 1
#include <os/assumes.h>

#include <darwintest.h>

void os_crash_function(const char *message);

static const char *__unsafe_indexable expected_message = NULL;

void os_crash_function(const char *message) {
	if (expected_message) {
		T_ASSERT_EQ_STR(__terminated_by_to_indexable(message), __unsafe_forge_single(const char *, expected_message), NULL);
		T_END;
	} else {
		T_PASS("Got crash message: %s", message);
		T_END;
	}
}

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
	expected_message = "assertion failure: \"two + two == 5\" -> %lld";
	os_assert(two + two == 5);
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
