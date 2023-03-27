/*
 * Regression test for rdar://89017007
 */

#include <errno.h>
#include <fcntl.h>
#include <fts.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <darwintest.h>
#include <darwintest_utils.h>

/*
 * Count number of free file descriptors up to the specified limit by
 * repeatedly trying to allocate one until we hit the specified limit or
 * run out.
 */
static int fdcountfree(int, int);
static int
fdcountfree(int expect, int max)
{
	int count, fd;

	fd = dup(0);
	if (fd < 0) {
		if (errno != EMFILE)
			T_ASSERT_FAIL("dup(): %s", strerror(errno));
		return (0);
	}
	while (fd > expect) {
		T_LOG("file descriptor %d is in use", expect);
		expect++;
	}
	count = 1;
	if (fd < max)
		count += fdcountfree(fd + 1, max);
	close(fd);
	return (count);
}

/*
 * Exercise the bug in FTS.
 */
static void
fts_exercise(char *path, int fts_options)
{
	char *paths[] = { path, NULL };
	FTS *fts;
	FTSENT *ent;
	int dcount, fcount, indent;

	fts = fts_open(paths, fts_options, NULL);
	T_ASSERT_POSIX_NOTNULL(fts, "fts_open()");
	dcount = fcount = 0;
	T_LOG("%s\n", path);
	indent = 1;
	for (;;) {
		if ((ent = fts_read(fts)) == NULL) {
			T_EXPECT_POSIX_ZERO(errno, "fts_read()");
			break;
		}
		switch (ent->fts_info) {
		case FTS_D:
			T_LOG("%*s%s/\n", indent, "", ent->fts_name);
			dcount++;
			indent++;
			break;
		case FTS_DP:
			indent--;
			break;
		default:
			T_LOG("%*s%s\n", indent, "", ent->fts_name);
			fcount++;
			break;
		}
		if (strcmp(ent->fts_name, "stop") == 0) {
			T_LOG("stopping");
			break;
		}
	}
	T_EXPECT_POSIX_SUCCESS(fts_close(fts), "fts_close()");
	T_LOG("%s: %d directories and %d files\n", path, dcount, fcount);
}

/*
 * When changing to a directory belonging to a different filesystem than
 * the current directory, FTS (without the FTS_NOCHDIR option) stores a
 * file descriptor to the current directory in the child's node, so it can
 * later reliably return to the previous directory.  Verify that this file
 * descriptor is always closed, regardless of whether it is used.
 */
T_DECL(fts_fd_leak_xdev, "FTS fd leak when crossing filesystem boundaries",
    T_META_NAMESPACE("Libc.regression"))
{
	char *tmpdir;
	int count, saved_count;

	T_QUIET;
	count = fdcountfree(3, getdtablesize() - 1);

	/* move to a temporary directory */
	T_SETUPBEGIN;
	T_ASSERT_POSIX_SUCCESS(asprintf(&tmpdir, "%s/%s-XXXXXX", dt_tmpdir(), T_NAME), NULL);
	T_ASSERT_POSIX_NOTNULL(mkdtemp(tmpdir), NULL);
	T_LOG("tmpdir: %s", tmpdir);
	T_ASSERT_POSIX_SUCCESS(chdir(tmpdir), NULL);
	T_SETUPEND;

	/* traverse /dev/fd, which will bring us back to cwd */
	fts_exercise("/dev/fd", FTS_PHYSICAL);
	saved_count = count;
	count = fdcountfree(3, getdtablesize() - 1);
	T_EXPECT_EQ(count, saved_count, "file descriptor leak");

	/* clean up after us */
	(void)remove(tmpdir);
	free(tmpdir);
}

/*
 * When changing to a directory through a symbolic link, FTS (without the
 * FTS_NOCHDIR option) stores a file descriptor to the current directory
 * in the child's node, so it can later reliably return to the previous
 * directory.  Verify that this file descriptor is always closed,
 * regardless of whether it is used.
 */
T_DECL(fts_fd_leak_symlink, "FTS fd leak when following symbolic links",
    T_META_NAMESPACE("Libc.regression"))
{
	char *tmpdir;
	char *dir, *subdir, *file, *link;
	int count, saved_count;
	int fd;

	T_QUIET;
	count = fdcountfree(3, getdtablesize() - 1);

	/* move to a temporary directory */
	T_SETUPBEGIN;
	T_ASSERT_POSIX_SUCCESS(asprintf(&tmpdir, "%s/%s-XXXXXX", dt_tmpdir(), T_NAME), NULL);
	T_ASSERT_POSIX_NOTNULL(mkdtemp(tmpdir), NULL);
	T_LOG("tmpdir: %s", tmpdir);
	T_ASSERT_POSIX_SUCCESS(chdir(tmpdir), NULL);
	T_SETUPEND;

	/* prepare hierarchy */
	T_SETUPBEGIN;
	T_ASSERT_POSIX_SUCCESS(asprintf(&dir, "%s/dir", tmpdir), NULL);
	T_LOG("creating %s", dir);
	T_ASSERT_POSIX_SUCCESS(mkdir(dir, 0755), NULL);
	T_ASSERT_POSIX_SUCCESS(asprintf(&subdir, "%s/subdir", dir), NULL);
	T_LOG("creating %s", subdir);
	T_ASSERT_POSIX_SUCCESS(mkdir(subdir, 0755), NULL);
	T_ASSERT_POSIX_SUCCESS(asprintf(&link, "%s/link", dir), NULL);
	T_LOG("creating %s -> %s", link, subdir);
	T_ASSERT_POSIX_SUCCESS(symlink(subdir, link), NULL);
	T_SETUPEND;

	/* traverse a directory that contains a symlink to another directory */
	fts_exercise(dir, FTS_LOGICAL);
	saved_count = count;
	count = fdcountfree(3, getdtablesize() - 1);
	T_EXPECT_EQ(count, saved_count, "file descriptor leak");

	/* traverse a directory which you reach through a symlink */
	fts_exercise(link, FTS_LOGICAL);
	saved_count = count;
	count = fdcountfree(3, getdtablesize() - 1);
	T_EXPECT_EQ(count, saved_count, "file descriptor leak");

	/* follow a symlink, then fts_close() */
	T_SETUPBEGIN;
	T_ASSERT_POSIX_SUCCESS(asprintf(&file, "%s/stop", subdir), NULL);
	T_LOG("creating %s", file);
	T_ASSERT_POSIX_SUCCESS(fd = creat(file, 0644), NULL);
	T_ASSERT_POSIX_SUCCESS(close(fd), NULL);
	T_SETUPEND;
	fts_exercise(dir, FTS_LOGICAL);
	saved_count = count;
	count = fdcountfree(3, getdtablesize() - 1);
	T_EXPECT_EQ(count, saved_count, "file descriptor leak");

	/* clean up after us */
	(void)remove(file);
	free(file);
	(void)remove(link);
	free(link);
	(void)remove(subdir);
	free(subdir);
	(void)remove(dir);
	free(dir);
	(void)remove(tmpdir);
	free(tmpdir);
}
