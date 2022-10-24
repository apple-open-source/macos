#include <os/string.h>
#include <darwintest.h>

T_DECL(os_string_include_test, "Make sure we can use os/string.h by itself") {
	const char *result = strerror_np(1);
	T_EXPECT_NOTNULL(result, "strerror_np doesn't work on EPERM!");
}
