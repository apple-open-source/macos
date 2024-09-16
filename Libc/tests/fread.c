#include <darwintest.h>
#include <darwintest_utils.h>

#include "libc_hooks_helper.h"

T_DECL(libc_hooks_fread, "Test libc_hooks for fread")
{
    // Setup
    T_SETUPBEGIN;
    FILE *f = fopen("/dev/null", "r");
    T_SETUPEND;

    // Test
    char buf[256];
    libc_hooks_log_start();
    fread(buf, 1, 42, f);
    libc_hooks_log_stop(2);

    // Check
    T_LOG("fread(buf, 1, 42, f)");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, f, sizeof(*f)), "checking f");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, buf, 42), "checking buf");

    // Cleanup
    fclose(f);
}
