#include <darwintest.h>
#include <notify.h>
#include <os/assumes.h>
#include <stdbool.h>
#include <string.h>

#define NOTIFY_MAX_REGISTRATIONS_PER_CLIENT 10000

static void
crash_callback(const char *message)
{
	if (strstr(message, "BUG IN CLIENT OF LIBNOTIFY: exceeded registration limit")) {
		T_PASS("Triggered exceeded registration limit assertion");
		T_END;
	}
	T_ASSERT_FAIL("Unknown assertion occured: %s", message);
}

os_crash_redirect(crash_callback);

T_DECL(notify_registration_limit,
       "verify client crashes when exceeding 10000 IPC registrations",
       T_META("as_root", "false"))
{
	int token;
	uint32_t status;
	char name[64];

	/* Use unique names to avoid coalescing */
	for (int i = 0; i < NOTIFY_MAX_REGISTRATIONS_PER_CLIENT + 1; i++) {
		snprintf(name, sizeof(name), "com.apple.notify.limit.test.%d", i);
		status = notify_register_check(name, &token);
	}

	T_ASSERT_FAIL("Excessive notify_register should crash");
}
