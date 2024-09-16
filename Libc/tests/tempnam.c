#include <darwintest.h>
#include <darwintest_utils.h>

#include "libc_hooks_helper.h"

T_DECL(libc_hooks_tempnam, "Test libc_hooks for tempnam")
{
    // Test
    char dir[] = "foobar"; char pfx[] = "/etc/";
    libc_hooks_log_start();
    char *s = tempnam(dir, pfx);
    libc_hooks_log_stop(6);

    // Check
    T_LOG("tempnam(\"%s\", \"%s\")", dir, pfx);
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read_cstring, dir, strlen(dir) + 1), "checking dir");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read_cstring, pfx, strlen(pfx) + 1), "checking pfx");
#if 0 // TBD: Where are these coming from?
    libc_hooks_log_dump(libc_hooks_log);
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read, ?, SIZE_LOCALE_T)), "checking ? (location)");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read_cstring, ?, 11)), "checking ?");
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read, ?, 9)), "checking ?");
#else
    libc_hooks_log.check += 3;
#endif
    libc_hooks_log_expect(LIBC_HOOKS_LOG(libc_hooks_will_read, pfx, strlen(pfx)), "checking pfx (being read)");

    // Cleanup
    free(s);
}

