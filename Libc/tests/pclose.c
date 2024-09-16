#include <darwintest.h>
#include <darwintest_utils.h>

#include "libc_hooks_helper.h"

T_DECL(libc_libc_hooks_pclose, "Test libc_hooks for pclose")
{
    // Setup
    T_SETUPBEGIN;
    FILE *iop = popen("true", "r");
    T_SETUPEND;

    // Test
    libc_hooks_log_start();
    pclose(iop);
    libc_hooks_log_stop(2);

    // Check
    T_LOG("pclose(iop)");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read, iop, sizeof(*iop)), "checking iop");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, iop, sizeof(*iop)), "checking iop (again)");
}
