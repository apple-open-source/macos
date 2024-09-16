#include <darwintest.h>
#include <darwintest_utils.h>

#include "libc_hooks_helper.h"

T_DECL(libc_hooks_popen, "Test libc_hooks for popen")
{
   /// Test
    char command[] = "true", type[] = "r";
    libc_hooks_log_start();
    FILE *iop = popen(command, type);
    libc_hooks_log_stop(3);

    // Check
    T_LOG("popen(\"%s\", \"%s\")", command, type);
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read_cstring, command, strlen(command) + 1), "checking command");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read_cstring, type, strlen(type) + 1), "checking type");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read_cstring, type, strlen(type) + 1), "checking type (again)");

    // Cleanup
    pclose(iop);
}
