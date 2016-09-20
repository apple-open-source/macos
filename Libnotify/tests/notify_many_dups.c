#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <notify.h>
#include <darwintest.h>

T_DECL(notify_many_dups,
       "notify many duplicate registration test",
       T_META("owner", "Core Darwin Daemons & Tools"),
       T_META("as_root", "false"))
{
	int t, n, i;
	uint32_t status;
	mach_port_t port = MACH_PORT_NULL;
	const char *name = "com.apple.notify.many.dups.test";

	n = 50000;

	status = notify_register_mach_port(name, &port, 0, &t);
    T_EXPECT_EQ_INT(status, NOTIFY_STATUS_OK, "notify_register_mach_port status == NOTIFY_STATUS_OK");
	for (i = 1; i < n; i++)
	{
		status = notify_register_mach_port(name, &port, NOTIFY_REUSE, &t);

        if (status != NOTIFY_STATUS_OK)  {
            T_FAIL("notify_register_mach_port status != NOTIFY_STATUS_OK (status: %d, iteration: %d", status, i);
        }
	}
    T_PASS("Successfully registered %d times for name %s\n", n, name);
}
