#include <sys/wait.h>
#include <assert.h>
#include <copyfile.h>
#include <paths.h>
#include <unistd.h>

#include <darwintest.h>
#include <darwintest_utils.h>
#include <darwintest_posix.h>

T_GLOBAL_META(T_META_RUN_CONCURRENTLY(true));

/*
 * Create copy of a program in the given directory.  Returns an open
 * descriptor to the newly created program.  Any error other than a
 * failure to create the file results in SIGABRT.
 */
static int
_copy_program_impl(const char *template, int dd, const char *name, mode_t perm)
{
	int pd, td;
	int serrno;

	(void)unlinkat(dd, name, 0);
	pd = openat(dd, name, O_RDWR|O_CREAT|O_TRUNC, 0600);
	if (pd < 0) {
		serrno = errno;
		(void)close(dd);
		errno = serrno;
		return (pd);
	}
	assert(fchmod(pd, perm) == 0);
	assert((td = open(template, O_RDONLY)) >= 0);
	assert(fcopyfile(td, pd, NULL, COPYFILE_DATA) == 0);
	assert(close(td) == 0);
	return (pd);
}
static int
_copy_program(const char *template, const char *dir, const char *name, mode_t perm)
{
	int dd, pd, serrno;

	assert((dd = open(dir, O_RDONLY)) >= 0);
	pd = _copy_program_impl(template, dd, name, perm);
	serrno = errno;
	assert(close(dd) == 0);
	errno = serrno;

	return (pd);
}

/*
 * Fork a child which invokes execvp() with the arguments provided by the
 * caller.  The provided search path, if not NULL, is used instead of
 * $PATH.  If the call fails, the child reports the outcome by writing the
 * return value and the value of errno to a pipe to the parent.  The
 * parent sets errno to the reported value and returns the reported return
 * value.  If the call succeeds, the parent sets errno to 0 and returns 0.
 * If statusp is not NULL, it will contain the child's wait status.  Any
 * unexpected situation (failure to set up the reporting pipe, failure to
 * fork, unexpected return value from waitpid(), etc.) results in SIGABRT.
 */
static int
_fork_execvp(const char *name, char *const args[], const char *search_path,
    int *statusp)
{
	struct { int ret, err; } report;
	ssize_t sz;
	pid_t pid;
	int p[2], status;

	if (search_path == NULL)
		search_path = getenv("PATH");
	if (search_path == NULL)
		search_path = _PATH_DEFPATH;
	report.ret = report.err = status = 0;
	assert(pipe(p) == 0);
	assert(fcntl(p[0], F_SETFD, FD_CLOEXEC) == 0);
	assert(fcntl(p[1], F_SETFD, FD_CLOEXEC) == 0);
	assert((pid = fork()) >= 0);
	if (pid == 0) {
		/* child */
		errno = 0;
		report.ret = execvP(name, search_path, args);
		report.err = errno;
		(void)write(p[1], &report, sizeof(report));
		_exit(0);
	}
	/* parent */
	assert(close(p[1]) == 0);
	assert((sz = read(p[0], &report, sizeof(report))) >= 0);
	assert(close(p[0]) == 0);
	/* sz should be 0 if execvp() succeeded, sizeof(report) otherwise */
	assert(sz == 0 || sz == sizeof(report));
	assert(waitpid(pid, &status, 0) == pid);
	if (statusp != NULL)
		*statusp = status;
	errno = report.err;
	return (report.ret);
}

static int
_fork_spawnp(const char *name, char *const args[], const char *search_path,
    int *statusp)
{
	char *envp[2] = { NULL, NULL };
	char *restore_path = NULL;
	pid_t pid;
	int ret, serrno, status;
	bool need_restore = false;

	if (search_path != NULL) {
		need_restore = true;
		restore_path = getenv("PATH");
		if (restore_path != NULL) {
			restore_path = strdup(restore_path);
			assert(restore_path != NULL);
		}

		ret = setenv("PATH", search_path, 1);
		assert(ret == 0);
	} else {
		search_path = getenv("PATH");
		if (search_path == NULL)
			search_path = _PATH_DEFPATH;
	}

	ret = asprintf(&envp[0], "PATH=%s", search_path);
	assert(ret != -1);

	ret = posix_spawnp(&pid, name, NULL, NULL, args, envp);
	serrno = ret;
	free(envp[0]);

	if (ret == 0) {
		assert(waitpid(pid, &status, 0) == pid);
	} else {
		ret = -1;
		status = 0;
	}

	if (statusp != NULL)
		*statusp = status;

	if (need_restore) {
		int sret = ret;

		if (restore_path != NULL)
			ret = setenv("PATH", restore_path, 1);
		else
			ret = unsetenv("PATH");

		assert(ret == 0);
		free(restore_path);
		ret = sret;
	}

	errno = serrno;
	return (ret);
}

/*
 * Simple successful execvp() case using an absolute path and an empty
 * search path.
 */
T_DECL(execvp_success_absolute, "Success case (absolute)")
{
	char *name = "/bin/echo";
	char *args[] = { name, "Hello, world!", NULL };
	int status;

	T_EXPECT_POSIX_SUCCESS(_fork_execvp(name, args, "", &status),
	    "Executing a program successfully");
	if (T_RESULT == T_RESULT_PASS) {
		T_EXPECT_TRUE(WIFEXITED(status), "Checking termination");
		T_EXPECT_EQ(WEXITSTATUS(status), 0, "Checking exit code");
	}
}

/*
 * Simple successful posix_spawnp() case using an absolute path and an empty
 * search path.
 */
T_DECL(spawnp_success_absolute, "Success case (absolute)")
{
	char *name = "/bin/echo";
	char *args[] = { name, "Hello, world!", NULL };
	int status;

	T_EXPECT_POSIX_SUCCESS(_fork_spawnp(name, args, "", &status),
	    "Executing a program successfully");
	if (T_RESULT == T_RESULT_PASS) {
		T_EXPECT_TRUE(WIFEXITED(status), "Checking termination");
		T_EXPECT_EQ(WEXITSTATUS(status), 0, "Checking exit code");
	}
}

/*
 * Simple successful execvp() case using a relative path.
 */
T_DECL(execvp_success_relative, "Success case (relative)")
{
	char *name = "echo";
	char *args[] = { name, "Hello, world!", NULL };
	char *search_path = _PATH_DEFPATH;
	int status;

	T_EXPECT_POSIX_SUCCESS(_fork_execvp(name, args, search_path, &status),
	    "Executing a program successfully");
	if (T_RESULT == T_RESULT_PASS) {
		T_EXPECT_TRUE(WIFEXITED(status), "Checking termination");
		T_EXPECT_EQ(WEXITSTATUS(status), 0, "Checking exit code");
	}
}

/*
 * Simple successful posix_spawnp() case using a relative path.
 */
T_DECL(spawnp_success_relative, "Success case (relative)")
{
	char *name = "echo";
	char *args[] = { name, "Hello, world!", NULL };
	char *search_path = _PATH_DEFPATH;
	int status;

	T_EXPECT_POSIX_SUCCESS(_fork_spawnp(name, args, search_path, &status),
	    "Executing a program successfully");
	if (T_RESULT == T_RESULT_PASS) {
		T_EXPECT_TRUE(WIFEXITED(status), "Checking termination");
		T_EXPECT_EQ(WEXITSTATUS(status), 0, "Checking exit code");
	}
}

/*
 * Successful execvp() of a program which is a shell script lacking a
 * shebang, thus relying on execvp()'s ENOEXEC fallback logic.
 */
T_DECL(execvp_success_ENOEXEC, "Script without shebang")
{
	const char *dir = dt_tmpdir();
	char *name = "success_ENOEXEC";
	char *args[] = { name, NULL };
	char *script = "exec true";
	int dd, pd;

	T_SETUPBEGIN;
	T_ASSERT_POSIX_SUCCESS(dd = open(dir, O_RDONLY),
	    "Opening directory %s", dir);
	T_ASSERT_POSIX_SUCCESS(pd = openat(dd, name, O_RDWR|O_TRUNC|O_CREAT),
	    "Creating program %s in %s", name, dir);
	T_ASSERT_POSIX_SUCCESS(fchmod(pd, 0755),
	    "Changing program permissions");
	T_ASSERT_POSIX_SUCCESS(write(pd, script, strlen(script)),
	    "Writing code to program");
	T_ASSERT_POSIX_SUCCESS(close(pd),
	    "Closing program");
	T_SETUPEND;

	T_EXPECT_POSIX_SUCCESS(_fork_execvp(name, args, dir, NULL),
	    "Executing a shell script that lacks a shebang");

	T_SETUPBEGIN;
	T_EXPECT_POSIX_SUCCESS(unlinkat(dd, name, 0),
	    "Deleting program");
	T_EXPECT_POSIX_SUCCESS(close(dd),
	    "Closing temporary directory");
	T_SETUPEND;
}

/*
 * Attempt to invoke a program which is a shell script lacking a shebang,
 * thus relying on execvp()'s ENOEXEC fallback logic, but with an empty
 * argument list, which trips the bug described in rdar://107951804.
 *
 * In theory this is nondeterministic and the odds of the test passing
 * even when the bug is present are good.  In practice we seem to reliably
 * get an EFAULT.
 */
T_DECL(execvp_rdar_107951804, "Script without shebang with empty argv")
{
	const char *dir = dt_tmpdir();
	char *name = "rdar_107951804";
	char *args[] = { NULL };
	char *script = "test $# -eq 0";
	int dd, pd;
	int status;

	T_SETUPBEGIN;
	T_ASSERT_POSIX_SUCCESS(dd = open(dir, O_RDONLY),
	    "Opening directory %s", dir);
	T_ASSERT_POSIX_SUCCESS(pd = openat(dd, name, O_RDWR|O_TRUNC|O_CREAT),
	    "Creating program %s in %s", name, dir);
	T_ASSERT_POSIX_SUCCESS(fchmod(pd, 0755),
	    "Changing program permissions");
	T_ASSERT_POSIX_SUCCESS(write(pd, script, strlen(script)),
	    "Writing code to program");
	T_ASSERT_POSIX_SUCCESS(close(pd),
	    "Closing program");
	T_SETUPEND;

	T_EXPECT_POSIX_SUCCESS(_fork_execvp(name, args, dir, &status),
	    "Executing a shell script that lacks a shebang with an empty argument list");
	if (T_RESULT == T_RESULT_PASS) {
		T_EXPECT_TRUE(WIFEXITED(status), "Checking termination");
		T_EXPECT_EQ(WEXITSTATUS(status), 0, "Checking exit code");
	}

	T_SETUPBEGIN;
	T_EXPECT_POSIX_SUCCESS(unlinkat(dd, name, 0),
	    "Deleting program");
	T_EXPECT_POSIX_SUCCESS(close(dd),
	    "Closing temporary directory");
	T_SETUPEND;
}

/*
 * Attempt to execute a program which does not exist (absolute case).
 */
T_DECL(execvp_failure_ENOENT_absolute, "Failure case (ENOENT, absolute)")
{
	char *name = "/path/to/this!program?does#not@exist";
	char *args[] = { name, NULL };

	T_ASSERT_POSIX_FAILURE(_fork_execvp(name, args, "", NULL), ENOENT,
	    "Trying to execute a program that does not exist");
}

/*
 * Attempt to execute a program which does not exist (relative case).
 */
T_DECL(execvp_failure_ENOENT_relative, "Failure case (ENOENT, relative)")
{
	const char *dir = dt_tmpdir();
	char *name = "this!program?does#not@exist";
	char *args[] = { name, NULL };

	T_ASSERT_POSIX_FAILURE(_fork_execvp(name, args, dir, NULL), ENOENT,
	    "Trying to execute a program that does not exist");
}

/*
 * Attempt to execute a program which is not executable.
 */
T_DECL(execvp_failure_EACCES, "Failure case (EACCES)")
{
	const char *dir = dt_tmpdir();
	const char *template = "/usr/bin/true";
	char *name = "program_EACCES";
	char *args[] = { name, NULL };
	int pd;

	T_SETUPBEGIN;
	T_ASSERT_POSIX_SUCCESS(pd = _copy_program(template, dir, name, 0444),
	    "Creating program %s in %s", name, dir);
	T_ASSERT_POSIX_SUCCESS(close(pd),
	    "Closing program");
	T_SETUPEND;

	T_EXPECT_POSIX_FAILURE(_fork_execvp(name, args, dir, NULL), EACCES,
	    "Trying to execute a program which is not executable");
}

/*
 * Attempt to execute a program which is executable, but not by us.
 */
T_DECL(execvp_failure_EPERM, "Failure case (EPERM)", T_META_ASROOT(false),
    T_META_CHECK_LEAKS(false))
{
	const char *dir = dt_tmpdir();
	const char *template = "/usr/bin/true";
	char *name = "program_EPERM";
	char *args[] = { name, NULL };
	int pd;

	T_SETUPBEGIN;
	T_ASSERT_POSIX_SUCCESS(pd = _copy_program(template, dir, name, 0445),
	    "Creating program %s in %s", name, dir);
	T_ASSERT_POSIX_SUCCESS(close(pd),
	    "Closing program");
	T_SETUPEND;

	T_EXPECT_POSIX_FAILURE(_fork_execvp(name, args, dir, NULL), EACCES,
	    "Trying to execute a program which is executable but not by us");
}

/*
 * Attempt to execute a binary program while it is open for writing.
 */
T_DECL(execvp_failure_ETXTBSY, "Failure case (ETXTBSY)")
{
	const char *dir = dt_tmpdir();
	const char *template = "/usr/bin/true";
	char *name = "program_ETXTBSY";
	char *args[] = { name, NULL };
	int pd;

	T_SETUPBEGIN;
	T_ASSERT_POSIX_SUCCESS(pd = _copy_program(template, dir, name, 0555),
	    "Creating program %s in %s", name, dir);
	T_SETUPEND;

	/*
	 * This should fail, but doesn't.  May be filesystem-dependent.
	 */
	T_EXPECTFAIL;
	T_EXPECT_POSIX_FAILURE(_fork_execvp(name, args, dir, NULL), ETXTBSY,
	    "Trying to execute a program which is open for writing");

	T_SETUPBEGIN;
	T_EXPECT_POSIX_SUCCESS(close(pd),
	    "Closing program");
	T_SETUPEND;
}

/*
 * Attempt to execute a program by absolute path which is too long.  We
 * achieve this by creating a symlink that points to its containing
 * directory so we can craft arbitrarily long paths to any file contained
 * in that directory.
 */
T_DECL(execvp_failure_ENAMETOOLONG_absolute, "Failure case (ENAMETOOLONG, absolute)")
{
	const char *dir = dt_tmpdir();
	const char *template = "/usr/bin/true";
	char *linkname = "link_ENAMETOOLONG_absolute";
	char *name = "program_ENAMETOOLONG_absolute";
	char *args[] = { name, NULL };
	char *full_path;
	size_t name_max, path_max;
	size_t len;
	int dd, pd;
	int ret;

	T_SETUPBEGIN;
	T_ASSERT_POSIX_SUCCESS(dd = open(dir, O_RDONLY),
	    "Opening directory %s", dir);
	(void)unlinkat(dd, linkname, 0);
	T_ASSERT_POSIX_SUCCESS(symlinkat(".", dd, linkname),
	    "Creating loopback symlink");
	name_max = (size_t)fpathconf(dd, _PC_NAME_MAX);
	T_LOG("NAME_MAX = %zu", name_max);
	path_max = (size_t)fpathconf(dd, _PC_PATH_MAX);
	T_LOG("PATH_MAX = %zu", path_max);
	T_ASSERT_NOTNULL(full_path = malloc(path_max + name_max),
	    "Allocating space for full path");
	ret = snprintf(full_path, path_max + name_max, "%s", dir);
	assert(ret >= 0);
	len = (size_t)ret;
	while (len + 1 + strlen(name) < path_max) {
		ret = snprintf(full_path + len, path_max + name_max - len,
		    "%s/", linkname);
		assert(ret >= 0);
		len += (size_t)ret;
	}
	ret = snprintf(full_path + len, path_max + name_max - len,
	    "%s", name);
	assert(ret >= 0);
	len += (size_t)ret;
	T_LOG("full path will be %s", full_path);
	T_ASSERT_POSIX_SUCCESS(pd = _copy_program(template, dir, name, 0555),
	    "Creating program %s in %s", name, dir);
	T_ASSERT_POSIX_SUCCESS(close(pd),
	    "Closing program");
	T_EXPECT_POSIX_SUCCESS(close(dd),
	    "Closing temporary directory");
	T_SETUPEND;

	T_EXPECT_POSIX_FAILURE(_fork_execvp(full_path, args, NULL, NULL), ENAMETOOLONG,
	    "Trying to execute a program whose name is too long (absolute)");
}

/*
 * Attempt to execute a program by relative path which is too long.  We
 * achieve this by creating a symlink that points to its containing
 * directory so we can craft arbitrarily long paths to any file contained
 * in that directory.  We then craft a search path such that the requested
 * program exists in the search path but the combined path is too long.
 */
T_DECL(execvp_failure_ENAMETOOLONG_relative, "Failure case (ENAMETOOLONG, relative)")
{
	const char *dir = dt_tmpdir();
	const char *template = "/usr/bin/true";
	char *linkname = "link_ENAMETOOLONG_relative";
	char *name = "program_ENAMETOOLONG_relative";
	char *args[] = { name, NULL };
	char *relative_path;
	size_t name_max, path_max;
	size_t len;
	int dd, pd;
	int ret;

	T_SETUPBEGIN;
	T_ASSERT_POSIX_SUCCESS(dd = open(dir, O_RDONLY),
	    "Opening directory %s", dir);
	(void)unlinkat(dd, linkname, 0);
	T_ASSERT_POSIX_SUCCESS(symlinkat(".", dd, linkname),
	    "Creating loopback symlink");
	name_max = (size_t)fpathconf(dd, _PC_NAME_MAX);
	T_LOG("NAME_MAX = %zu", name_max);
	path_max = (size_t)fpathconf(dd, _PC_PATH_MAX);
	T_LOG("PATH_MAX = %zu", path_max);

	/*
	 * Add some extra space to make sure we thoroughly pass PATH_MAX,
	 * regardless of the tmpdir length.
	 */
	path_max += strlen(linkname) + strlen(name) + 2;

	T_ASSERT_NOTNULL(relative_path = malloc(path_max),
	    "Allocating space for large relative path");
	/* dt_tmpdir() won't normalize TMPDIR. */
	ret = snprintf(relative_path, path_max, "%s%s", dir,
	    dir[strlen(dir) - 1] == '/' ? "" : "/");
	assert(ret >= 0);
	len = (size_t)ret;
	while (len + strlen(linkname) + 1 + strlen(name) < path_max) {
		ret = snprintf(relative_path + len, path_max - len, "%s/",
		    linkname);
		assert(ret >= 0);
		len += (size_t)ret;
	}
	snprintf(relative_path + len, path_max - len, "%s", name);
	T_LOG("relative path will be %s", relative_path);
	T_ASSERT_POSIX_SUCCESS(pd = _copy_program(template, dir, name, 0555),
	    "Creating program %s in %s", name, dir);
	T_ASSERT_POSIX_SUCCESS(close(pd),
	    "Closing program");
	T_EXPECT_POSIX_SUCCESS(close(dd),
	    "Closing temporary directory");
	T_SETUPEND;

	T_EXPECT_POSIX_FAILURE(_fork_execvp(relative_path, args, NULL, NULL), ENAMETOOLONG,
	    "Trying to execute a program whose name is too long (relative)");
}

static void
setup_spawn_dirs(const char *dir)
{
	const char *template = "/usr/bin/true";
	int dfd, pd, ret;

	dfd = open(dir, O_DIRECTORY);
	T_ASSERT_GE(dfd, 0, "Opening %s", dir);

	ret = mkdirat(dfd, "dir1", 0755);
	if (ret == -1 && errno != EEXIST)
		T_ASSERT_POSIX_SUCCESS(ret, "Creating %s/dir1", dir);

	ret = mkdirat(dfd, "dir2", 0755);
	if (ret == -1 && errno != EEXIST)
		T_ASSERT_POSIX_SUCCESS(ret, "Creating %s/dir2", dir);

	T_ASSERT_POSIX_SUCCESS(pd = _copy_program_impl(template, dfd,
	    "dir1/program1", 0755), "Creating program1 in %s/dir1", dir);
	T_ASSERT_POSIX_SUCCESS(close(pd),
	    "Closing program1");
	T_ASSERT_POSIX_SUCCESS(pd = _copy_program_impl(template, dfd,
	    "dir2/program2", 0755), "Creating program2 in %s/dir2", dir);
	T_ASSERT_POSIX_SUCCESS(close(pd),
	    "Closing program2");
	T_ASSERT_POSIX_SUCCESS(pd = _copy_program_impl(template, dfd,
	    "program3", 0755), "Creating program3 in %s", dir);
	T_ASSERT_POSIX_SUCCESS(close(pd),
	    "Closing program3");
	T_ASSERT_POSIX_SUCCESS(close(dfd),
	    "Closing tempdir dirfd");
}

/*
 * Simple successful execvp() cases relying on PATH searching.
 */
T_DECL(execvp_success_searching, "Success case (searching)")
{
	const char *dir = dt_tmpdir();
	char *search_path;
	const char *name1 = "program1", *name2 = "program2";
	const char *name3 = "program3";
	char *args[2] = { NULL, NULL };
	int ret;

	T_SETUPBEGIN;
	setup_spawn_dirs(dir);
	T_SETUPEND;

	ret = asprintf(&search_path, "%s/dir1:%s/dir2:", dir, dir);
	T_ASSERT_GE(ret, 0, "Constructing search PATH");
	T_LOG("search_path = %s", search_path);

	args[0] = __DECONST(char *, name1);
	T_EXPECT_POSIX_SUCCESS(_fork_execvp(name1, args, search_path, NULL),
	    "Trying to execute a program in initial PATH");
	args[0] = __DECONST(char *, name2);
	T_EXPECT_POSIX_SUCCESS(_fork_execvp(name2, args, search_path, NULL),
	    "Trying to execute a program in another part of PATH");

	/* Implied CWD from component of PATH being empty */
	args[0] = __DECONST(char *, name3);
	T_EXPECT_POSIX_FAILURE(_fork_execvp(name3, args, search_path, NULL),
	    ENOENT,
	    "Trying to execute a program in empty part of PATH (wrong PWD)");
	T_EXPECT_POSIX_SUCCESS(chdir(dir), "%s: chdir", dir);
	T_EXPECT_POSIX_SUCCESS(_fork_execvp(name3, args, search_path, NULL),
	    "Trying to execute a program in empty part of PATH (correct PWD)");

	free(search_path);
}

/*
 * Simple successful spawnp() cases relying on PATH searching.
 */
T_DECL(spawnp_success_searching, "Success case (searching)")
{
	const char *dir = dt_tmpdir();
	char *search_path;
	const char *name1 = "program1", *name2 = "program2";
	const char *name3 = "program3";
	char *args[2] = { NULL, NULL };
	int ret;

	T_SETUPBEGIN;
	/* Ensure we're not in our tempdir. */
	setup_spawn_dirs(dir);
	T_SETUPEND;

	ret = asprintf(&search_path, "%s/dir1:%s/dir2:", dir, dir);
	T_ASSERT_GE(ret, 0, "Constructing search PATH");
	T_LOG("search_path = %s", search_path);

	args[0] = __DECONST(char *, name1);
	T_EXPECT_POSIX_SUCCESS(_fork_spawnp(name1, args, search_path, NULL),
	    "Trying to execute a program in initial PATH");
	args[0] = __DECONST(char *, name2);
	T_EXPECT_POSIX_SUCCESS(_fork_spawnp(name2, args, search_path, NULL),
	    "Trying to execute a program in another part of PATH");

	/* Implied CWD from component of PATH being empty */
	args[0] = __DECONST(char *, name3);
	T_EXPECT_POSIX_FAILURE(_fork_spawnp(name3, args, search_path, NULL),
	    ENOENT,
	    "Trying to execute a program in empty part of PATH (wrong PWD)");
	T_EXPECT_POSIX_SUCCESS(chdir(dir), "%s: chdir", dir);
	T_EXPECT_POSIX_SUCCESS(_fork_spawnp(name3, args, search_path, NULL),
	    "Trying to execute a program in empty part of PATH (correct PWD)");

	free(search_path);
}
