#include <darwintest.h>
#include <darwintest_utils.h>

#include "libc_hooks_helper.h"

T_DECL(libc_hooks_fclose, "Test libc_hooks for fclose")
{
    // Setup
    T_SETUPBEGIN;
    FILE *f = fopen("/dev/null", "w");
    T_SETUPEND;

    // Test
    libc_hooks_log_start();
    fclose(f);
    libc_hooks_log_stop(1);

    // Check
    T_LOG("fclose(f)");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, f, sizeof(*f)), "checking f");
}
