#include "includes.h"

#ifdef __APPLE_CLEAR_LV__
#include <dlfcn.h>
#include <unistd.h>
#include <sys/fcntl.h>
#include <mach-o/dyld_priv.h>
#include <System/sys/codesign.h>
#include "log.h"

/*
 * <rdar://problem/65693657> [sshd] Adopt
 * com.apple.private.security.clear-library-validation. Attempt to
 * dynamically load a module. Disable LV on the process if necessary.
 * NB: Code is based on OpenPAM's openpam_dlopen().
 */

void *
dlopen_lv(char *path, int mode)
{
    /* Fast path: dyld shared cache. */
    if (_dyld_shared_cache_contains_path(path)) {
        return dlopen(path, mode);
    }

    /* Slow path: check file on disk. */
    if (faccessat(AT_FDCWD, path, R_OK, AT_EACCESS) != 0) {
        return NULL;
    }

    void *dlh = dlopen(path, mode);
    if (dlh != NULL) {
        return dlh;
    }

    /*
     * The module exists and is readable, but failed to load.  If
     * library validation is enabled, try disabling it and then try
     * again.
     */
    int   csflags = 0;
    pid_t pid     = getpid();
    csops(pid, CS_OPS_STATUS, &csflags, sizeof(csflags));
    if ((csflags & (CS_FORCED_LV | CS_REQUIRE_LV)) == 0) {
        return NULL;
    }

    int rv = csops(getpid(), CS_OPS_CLEAR_LV, NULL, 0);
    if (rv != 0) {
        error("csops(CS_OPS_CLEAR_LV) failed: %d", rv);
        return NULL;
    }

    dlh = dlopen(path, mode);
    if (dlh == NULL) {
        /* Failed to load even with LV disabled: re-enable LV. */
        csflags = CS_REQUIRE_LV;
        csops(pid, CS_OPS_SET_STATUS, &csflags, sizeof(csflags));
    }

    return dlh;
}
#endif /* __APPLE_CLEAR_LV__ */
