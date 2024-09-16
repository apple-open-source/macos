#include <darwintest.h>
#include "darwintest_utils.h"

#include "libc_hooks_helper.h"

T_DECL(libc_hooks_fputs, "Test libc_hooks for fputs")
{
    // Setup
    T_SETUPBEGIN;
    FILE *f = fopen("/dev/null", "w");
    T_SETUPEND;

    // Test
    char buf[256] = "foo";
    libc_hooks_log_start();
    fputs(buf, f);
    libc_hooks_log_stop(2);

    // Check
    T_LOG("fputs(buf, f)");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read_cstring, buf, strlen(buf) + 1), "checking buf");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, f, sizeof(*f)), "checking f");

    // Cleanup
    fclose(f);
}
