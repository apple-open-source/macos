#include <TargetConditionals.h>
#include <darwintest.h>
#include <darwintest_utils.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <libtop.h>

T_GLOBAL_META(T_META_NAMESPACE("top"));

T_DECL(basic, "test that top launches")
{
	T_SETUPBEGIN;

	char top_output_path[MAXPATHLEN] = "top-output.txt";
	int ret = dt_resultfile(top_output_path, sizeof(top_output_path));
	T_QUIET;
	T_ASSERT_POSIX_ZERO(ret, "got path for top output result file");
	T_LOG("writing top output to '%s'", top_output_path);

	T_SETUPEND;

	int pid = 0;
	char *top_args[] = { "/usr/bin/top", "-l", "1", NULL };
	ret = dt_launch_tool(&pid, top_args, false, top_output_path, NULL);
	T_ASSERT_POSIX_SUCCESS(ret, "launched top");

	int status = 0;
	ret = waitpid(pid, &status, 0);
	T_QUIET;
	T_ASSERT_POSIX_SUCCESS(ret, "waited on top");

	T_QUIET;
	T_ASSERT_TRUE(WIFEXITED(status), "top exited");
	T_EXPECT_EQ(WEXITSTATUS(status), 0, "top exited cleanly");

	struct stat sbuf;
	ret = stat(top_output_path, &sbuf);
	T_QUIET;
	T_ASSERT_POSIX_SUCCESS(ret, "ran stat on top output file");

	T_EXPECT_GT(sbuf.st_size, (off_t)100, "top generated data");
}

static void
test_stats_option(char *key, char *colname)
{
	T_SETUPBEGIN;

	dispatch_queue_t q = dispatch_queue_create("com.apple.top.test", DISPATCH_QUEUE_SERIAL);
	dt_spawn_t spawn = dt_spawn_create(q);
	T_QUIET;
	T_ASSERT_NOTNULL(spawn, "created spawn object");

	char *command_argv[] = { "/usr/bin/top", "-l", "1", "-e", "-i", "1", "-stats", key, NULL };

	T_SETUPEND;

	T_LOG("testing top -stats %s", key);
	__block bool found = false;
	dt_spawn(
			spawn, command_argv,
			^(char *line_out, size_t size) {
				if (size > 0 && strncmp(line_out, colname, MIN(strlen(colname), size)) == 0) {
					T_PASS("found line with %s", colname);
					found = true;
				}
			},
			^(__unused char *line_err, __unused size_t size) { T_FAIL("top wrote to stderr"); });

	bool exited = false, signaled = false;
	int status = -1, term_signal = 0;
	dt_spawn_wait(spawn, &exited, &signaled, &status, &term_signal);
	T_QUIET;
	T_EXPECT_TRUE(exited, "top exited");
	T_QUIET;
	T_EXPECT_EQ(status, 0, "top exited cleanly");

	T_QUIET;
	T_EXPECT_TRUE(found, "found %s in top output", colname);

	dispatch_release(q);
}

T_DECL(stats_option, "test that top can show single columns")
{
	test_stats_option("pid", "PID");
	test_stats_option("command", "COMMAND");
	test_stats_option("cpu", "%CPU");
	test_stats_option("cpu_me", "%CPU_ME");
	test_stats_option("cpu_others", "%CPU_OTHRS");
	test_stats_option("boosts", "BOOSTS");
	test_stats_option("time", "TIME");
	test_stats_option("threads", "#TH");
	test_stats_option("ports", "#PORTS");
	test_stats_option("mregion", "#MREGS");
	test_stats_option("vsize", "VSIZE");
	test_stats_option("vprvt", "VPRVT");
	test_stats_option("instrs", "INSTRS");
	test_stats_option("cycles", "CYCLES");
	test_stats_option("pgrp", "PGRP");
	test_stats_option("ppid", "PPID");
	test_stats_option("pstate", "STATE");
	test_stats_option("uid", "UID");
	test_stats_option("wq", "#WQ");
	test_stats_option("faults", "FAULTS");
	test_stats_option("cow", "COW");
	test_stats_option("msgsent", "MSGSENT");
	test_stats_option("msgrecv", "MSGRECV");
	test_stats_option("sysbsd", "SYSBSD");
	test_stats_option("sysmach", "SYSMACH");
	test_stats_option("csw", "CSW");
	test_stats_option("pageins", "PAGEINS");
	test_stats_option("kprvt", "KPRVT");
	test_stats_option("kshrd", "KSHRD");
	test_stats_option("idlew", "IDLEW");
	test_stats_option("power", "POWER");
	test_stats_option("user", "USER");
}

boolean_t
_test_print(void * __unused user_data, const char *format, ...)
{
	char *msg = NULL;
	va_list ap;
	va_start(ap, format);
	int bytes = vasprintf(&msg, format, ap);
	va_end(ap);
	T_LOG("libtop: %s", msg);
	free(msg);
	return TRUE;
}

int
_pid_sort(void * __unused user_data, const libtop_psamp_t *lhs, const libtop_psamp_t *rhs)
{
	return lhs->pid - rhs->pid;
}

T_DECL(libtop_inspect, "test that libtop can access statistics with task inspect ports",
		T_META_ASROOT(true))
{
	int init_ret = libtop_init_with_options(_test_print, NULL, LIBTOP_INIT_INSPECT);
	T_ASSERT_POSIX_SUCCESS(init_ret, "libtop_init_with_options");
	T_ATEND(libtop_fini);

	libtop_sample(FALSE, FALSE);

	const libtop_tsamp_t *tsamp = libtop_tsamp();

	T_EXPECT_GT(tsamp->threads, 0, "non-zero total threads");
	// nprocs isn't updated until `libtop_psort`.
	libtop_psort(_pid_sort, NULL);
	T_EXPECT_GT(tsamp->nprocs, 0, "non-zero total processes");

	const libtop_psamp_t *psamp = NULL;
	while ((psamp = libtop_piterate()) != NULL) {
		T_QUIET; T_EXPECT_GT(psamp->th, 0ULL, "non-zero thread count in single process");
		T_QUIET; T_EXPECT_GT(psamp->total_timens, 0ULL, "non-zero CPU time from threads in single process");
	}
}
