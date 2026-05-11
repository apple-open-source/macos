/*
 * rdar://172291342: Helper process for notify_register_check_shm_deny test.
 *
 * Must be a separate process because earlier initializers already mapped shm
 * before main. To simulate a process sandboxed from birth (e.g., SiriAUSP in
 * the radar), the helper applies a sandbox that blocks shm_open, then reset
 * globals->shm_base to NULL.
 * 
 * This helper:
 *   1. Enters a sandbox that denies ipc-posix-shm-read* for notifyd
 *   2. Resets shm_base to NULL (simulating sandbox-from-birth)
 *   3. Loops notify_register_check 15k times (past the 10k server limit)
 *   4. Exits 0 if it survives (server registrations properly cleaned up)
 *   5. Gets SIGKILL'd by notifyd if registrations leaked (test detects this)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <notify.h>
#include <sandbox.h>
#include <sandbox/libsandbox.h>
#include <os/alloc_once_private.h>
#include "notify_internal.h"

#define ITERATIONS 15000
#define NAME "com.apple.notify.test.shm-deny"
#define SHM_NAME "apple.shm.notification_center"

static const char *sandbox_profile =
	"(version 1)"
	"(import \"system.sb\")"
	"(deny ipc-posix-shm-read*"
	"    (ipc-posix-name \"apple.shm.notification_center\"))";

int
main(int argc __unused, const char *argv[] __unused)
{
	char *errorbuf = NULL;
	sandbox_profile_t compiled = sandbox_compile_string(sandbox_profile,
							    NULL, &errorbuf);
	if (compiled == NULL) {
		fprintf(stderr, "sandbox_compile_string failed: %s\n",
			errorbuf ? errorbuf : "(null)");
		sandbox_free_error(errorbuf);
		return 1;
	}

	int ret = sandbox_apply(compiled);
	sandbox_free_profile(compiled);
	if (ret != 0) {
		perror("sandbox_apply");
		return 1;
	}

	/* Verify sandbox is actually blocking shm_open */
	int shmfd = shm_open(SHM_NAME, O_RDONLY, 0);
	if (shmfd >= 0) {
		fprintf(stderr, "shm_open succeeded after sandbox — "
			"sandbox is not blocking shm\n");
		close(shmfd);
		return 3;
	}

	/*
	 * Reset globals->shm_base to NULL to simulate a process that was
	 * sandboxed from birth. This allows exercising the shm_attach failure 
	 * path in notify_register_check.
	 */
	notify_globals_t globals = (notify_globals_t)os_alloc_once(
		OS_ALLOC_ONCE_KEY_LIBSYSTEM_NOTIFY,
		sizeof(struct notify_globals_s), NULL);
	if (globals != NULL) {
		globals->shm_base = NULL;
	}

	for (int i = 0; i < ITERATIONS; i++) {
		int token = -1;
		uint32_t status = notify_register_check(NAME, &token);

		/*
		 * shm_attach fails → notify_register_check returns failure.
		 * If server-side registrations leaked, notifyd would
		 * SIGKILL us before we reach 10k iterations.
		 */
		if (status == NOTIFY_STATUS_OK) {
			notify_cancel(token);
			fprintf(stderr, "iteration %d: notify_register_check "
				"unexpectedly succeeded\n", i);
			return 2;
		}
	}

	return 0;
}
