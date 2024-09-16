#include <darwintest.h>
#include <darwintest_utils.h>

#include "libc_hooks_helper.h"

T_DECL(libc_hooks_fwrite, "Test libc_hooks for fwrite")
{
    // Setup
    T_SETUPBEGIN;
    FILE *f = fopen("/dev/null", "w");
    T_SETUPEND;

    // Test
    char buf[256] = "foo";
    libc_hooks_log_start();
    fwrite(buf, sizeof(buf), 1, f);
    libc_hooks_log_stop(2);

    // Check
    T_LOG("fwrite(\"%s\", %zu, %zu, f)", buf, sizeof(buf), 1UL);
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, f, sizeof(*f)), "checking f");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read, buf, sizeof(buf)), "checking buf");

    // Cleanup
    fclose(f);
}
