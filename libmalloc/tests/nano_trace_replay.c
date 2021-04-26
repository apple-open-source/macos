//
//  nanov2_tests.c
//  libmalloc
//
//  Tests that are specific to the implementation details of Nanov2.
//
#include <TargetConditionals.h>
#include <darwintest.h>
#include <darwintest_utils.h>
#include <spawn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <malloc/malloc.h>
#include <../src/internal.h>

#if CONFIG_NANOZONE && !TARGET_OS_BRIDGE

static void
run_replay_tool(char *replay_file, const char *out_dir, char *test_name, char **envp) {
	int rc;
	char *replay_tool = "/usr/local/bin/libmalloc_replay";

	char replay_arg[256];
	rc = snprintf(replay_arg, 256, "%s", replay_file);

	char output_arg[256];
	rc |= snprintf(output_arg, 256, "%s/libmalloc.replay.%s", out_dir, test_name);

	char *args[] = {replay_tool, "-r", replay_arg, "-j", output_arg,
			"-t", test_name, "-s", NULL};

	T_QUIET; T_ASSERT_POSIX_SUCCESS(rc, "Failed to generate libmalloc_replay command");

	pid_t pid;
	rc = posix_spawn(&pid, replay_tool, NULL, NULL, args, envp);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(rc, "posix_spawn failed: %d", rc);

	int exit_status = 0;
	rc = waitpid(pid, &exit_status, 0);
	if (rc == -1 || !WIFEXITED(exit_status) || WEXITSTATUS(exit_status)) {
		T_ASSERT_FAIL("Replay tool exited abnormally: %d", WEXITSTATUS(exit_status));
	}
}

static void
run_nano_replay(char *replay_file_path, char *test_name, char *version)
{
	T_QUIET; T_ASSERT_POSIX_SUCCESS(access(replay_file_path, F_OK),
			"Test cannot access replay file: %s", replay_file_path);

	const char *tmp_dir = dt_tmpdir();
	T_QUIET; T_ASSERT_NOTNULL(tmp_dir, "Couldn't fetch dt_tmpdir");

	char full_test_name[256];
	char env_var[32];

	int rc = snprintf(full_test_name, sizeof(full_test_name), "%s_%s", test_name, version);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(rc, "Failed to generate full test name");
	rc = snprintf(env_var, sizeof(env_var), "MallocNanoZone=%s", version);
	T_QUIET; T_ASSERT_POSIX_SUCCESS(rc, "Failed to generate env vars");

	char *envp[] = {env_var, NULL};
	run_replay_tool(replay_file_path, tmp_dir, full_test_name, envp);
}

#ifndef TEST_TIMEOUT
#define TEST_TIMEOUT 1200
#endif

#define NANO_FRAG_TEST_VERSION(trace_name, nano_version, timeout) \
	T_DECL(nano_frag_## trace_name ## _ ## nano_version, "track Nano"#nano_version" fragmentation in "#trace_name, \
	T_META_TAG_PERF, T_META_NAMESPACE("libmalloc"), \
	T_META_GIT_ASSET_URL("ssh://git@stash.sd.apple.com/coreos/libmalloc.git"), \
	T_META_TIMEOUT(timeout), \
	T_META_GIT_ASSET("../traces/"#trace_name".mtrace")) \
		{ \
			const char *dt_assets = getenv("DT_ASSETS"); \
			T_QUIET; T_ASSERT_NOTNULL(dt_assets, "$DT_ASSETS not set"); \
			char *replay_file = NULL; \
			asprintf(&replay_file, "%s/%s", dt_assets, "../traces/"#trace_name".mtrace"); \
			run_nano_replay(replay_file, #trace_name, #nano_version); \
			T_PASS("Successfully ran libmalloc_replay under Nano"#nano_version); \
			free(replay_file); \
		}

#elif CONFIG_NANOZONE && TARGET_OS_BRIDGE

#define NANO_FRAG_TEST_VERSION(trace_name, nano_version, timeout) \
	T_DECL(nano_frag_## trace_name ## _ ## nano_version, "track Nano"#nano_version" fragmentation in "#trace_name, \
	T_META_NAMESPACE("libmalloc")) \
		{ \
			T_SKIP("skipping trace replay on BridgeOS"); \
		}

#else // !CONFIG_NANOZONE && !TARGET_OS_BRIDGE

#define NANO_FRAG_TEST_VERSION(trace_name, nano_version, timeout) \
	T_DECL(nano_frag_## trace_name ## _ ## nano_version, "track Nano"#nano_version" fragmentation in "#trace_name, \
	T_META_NAMESPACE("libmalloc")) \
		{ \
			T_SKIP("nanozone configured off; trace replay skipped"); \
		}

#endif // !CONFIG_NANOZONE && !TARGET_OS_BRIDGE

#define NANO_FRAG_TEST(trace_name) \
	NANO_FRAG_TEST_VERSION(trace_name, V1, TEST_TIMEOUT) \
	NANO_FRAG_TEST_VERSION(trace_name, V2, TEST_TIMEOUT)

NANO_FRAG_TEST(TRACE_NAME)

