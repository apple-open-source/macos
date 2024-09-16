#include <darwintest.h>
#include <darwintest_utils.h>

#include "libc_hooks_helper.h"

T_DECL(libc_hooks_tmpnam, "Test libc_hooks for tmpnam")
{
    // Test
    char buf[L_tmpnam] = "0";
    libc_hooks_log_start();
    tmpnam(buf);
    libc_hooks_log_stop(4);

    // Check
    T_LOG("tmpname(buf)");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_write, buf, L_tmpnam), "checking buf");
#if 0 // TBD: Where are these coming from?
    libc_hooks_log_dump(libc_hooks_log);
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read, ?, SIZE_LOCALE_T)), "checking ? (location)");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read_cstring, ?, 17)), "checking ?");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read, ?, 9)), "checking ?");
#endif
}
