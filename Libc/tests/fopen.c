#include <darwintest.h>
#include <darwintest_utils.h>

#include "libc_hooks_helper.h"

T_DECL(libc_hooks_fopen, "Test libc_hooks for fopen")
{
    // Test
    char file[] = "/dev/null"; char mode[] = "w";
    libc_hooks_log_start();
    FILE *f = fopen(file, mode);
    libc_hooks_log_stop(2);

    // Check
    T_LOG("fopen(\"%s\", \"%s\")", file, mode);
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read_cstring, file, strlen(file) + 1), "checking file");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read_cstring, mode, strlen(mode) + 1), "checking mode");

    // Cleanup
    fclose(f);
}
