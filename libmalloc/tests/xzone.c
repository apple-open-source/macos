#include <darwintest.h>
#include <darwintest_utils.h>

#include <malloc_private.h>

#include "../src/platform.h"

T_DECL(zzz_a_xzone_enable_bootargs,
		"Enable libmalloc boot-args support via ffctl",
		T_META_TAG_VM_NOT_ELIGIBLE,
		T_META_ENABLED(CONFIG_XZONE_MALLOC))
{
	dt_spawn_t ffctl = dt_spawn_create(NULL);
	char *ffctl_argv[] = {
		"/usr/local/bin/ffctl", "--domain=libmalloc", "EnableBootArgs=on", NULL
	};
	dt_spawn(ffctl, ffctl_argv,
			^(char *stdout_line, size_t len) {
				T_LOG("%.*s", (int)len, stdout_line);
			},
			^(char *stderr_line, size_t len) {
				T_LOG("%.*s", (int)len, stderr_line);
			});

	bool exited = false;
	bool signaled = false;
	int status = 0;
	int signal = 0;
	dt_spawn_wait(ffctl, &exited, &signaled, &status, &signal);

	T_LOG("exited: %d, signaled: %d, status: %d, signal: %d",
			exited, signaled, status, signal);
	T_ASSERT_TRUE(exited && !signaled && (status == 0) && (signal == 0), NULL);
}

T_DECL(zzz_b_xzone_malloc_systemwide, "Enable xzone malloc system-wide",
		T_META_BOOTARGS_SET("malloc_secure_allocator=1"),
		T_META_TAG_VM_NOT_ELIGIBLE,
		T_META_ENABLED(CONFIG_XZONE_MALLOC))
{
	T_PASS("Successfully booted the OS with xzone malloc enabled system-wide");
}

T_DECL(zzz_b_xzone_guards_systemwide, "Enable xzone guard pages system-wide",
		T_META_BOOTARGS_SET("malloc_secure_allocator=1"),
		T_META_BOOTARGS_SET("xzone_guard_pages=1"),
		T_META_ENABLED(CONFIG_XZONE_MALLOC))
{
	T_PASS("Successfully booted the OS with xzone guard pages system-wide");
}

T_DECL(xzone_debug_dylib, "Ensure xzone malloc tests run with debug dylib",
		T_META_TAG_VM_NOT_ELIGIBLE,
		T_META_TAG_XZONE_ONLY)
{
	T_ASSERT_TRUE(malloc_variant_is_debug_4test(),
			"Test is running with the debug dylib");
}
