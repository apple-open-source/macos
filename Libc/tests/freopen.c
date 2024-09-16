#include <darwintest.h>
#include <darwintest_utils.h>

#include "libc_hooks_helper.h"

T_DECL(libc_hooks_freopen, "Test libc_hooks for freopen")
{
    char file[] = "/dev/null"; char mode1[] = "w"; char mode2[] = "r";

    // Setup
    T_SETUPBEGIN;
    FILE *f1 = fopen(file, mode1);
    T_SETUPEND;

    // Test
    libc_hooks_log_start();
    FILE *f2 = freopen(file, mode2, f1);
    libc_hooks_log_stop(3);

    // Check
    T_LOG("freopen(\"%s\", \"%s\", f2)", file, mode2, f1);

    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read_cstring, file, strlen(file) + 1), "checking file");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read_cstring, mode2, strlen(mode2) + 1), "checking mode");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, f2, sizeof(*f2)), "checking f");

    // Cleanup
    fclose(f1);
    fclose(f2);
}
