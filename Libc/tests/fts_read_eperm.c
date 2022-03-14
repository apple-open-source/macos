#include <unistd.h>
#include <fts.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <darwintest.h>

T_DECL(fts_read_eperm_directory, "Check that fts_read does not crash on a directory that cannot be read",
		T_META_ASROOT(false),
		T_META_CHECK_LEAKS(false) /* needs leak check to be off to run as non-root */)
{
	FTS *fts;
	FTSENT *ftse;
	char tmpdir[] = "/tmp/temp.XXXXXX";
	char *args[] = {".", NULL};

	T_ASSERT_NOTNULL(mkdtemp(tmpdir), "mkdrtmp");

	T_EXPECT_POSIX_SUCCESS(chdir(tmpdir), "chdir");

	fts = fts_open(args, FTS_PHYSICAL, NULL);
	T_EXPECT_NOTNULL(fts, "fts_open");

	T_EXPECT_POSIX_SUCCESS(chmod(tmpdir, 000), "chmod");

	while ((ftse = fts_read(fts)) != NULL);

	T_EXPECT_POSIX_FAILURE(fts_close(fts), EACCES, "fts_close");

	T_EXPECT_POSIX_SUCCESS(rmdir(tmpdir), "rmdir");
}
