/*
 * Regression tests for rdar://116080732, rdar://144349691
 */

#include <fts.h>

#include <darwintest.h>
#include <darwintest_utils.h>

T_DECL(fts_nappend, "FTS deduplicates trailing slashes (rdar://116080732)",
    T_META_NAMESPACE("Libc.regression"))
{
	char *expectv[] = { "/bin", "/usr/bin", NULL }, **expect = expectv;
	char *paths[] = { "/", "/usr/", NULL };
	FTS *fts;
	FTSENT *ent;

	fts = fts_open(paths, FTS_PHYSICAL, NULL);
	T_ASSERT_POSIX_NOTNULL(fts, "fts_open()");
	while ((ent = fts_read(fts)) != NULL) {
		/* look only at directories */
		if (ent->fts_info != FTS_D)
			continue;
		/* don't descend, or we'll be at it all day */
		if (ent->fts_level > 0)
			fts_set(fts, ent, FTS_SKIP);
		/* look only at entries named bin */
		if (strcmp(ent->fts_name, "bin") != 0)
			continue;
		T_ASSERT_NOTNULL(*expect, NULL);
		T_EXPECT_EQ_STR(ent->fts_path, *expect, "%s", *expect);
		expect++;
	}
	T_EXPECT_POSIX_ZERO(errno, "fts_read()");
	T_EXPECT_POSIX_SUCCESS(fts_close(fts), "fts_close()");
	T_EXPECT_NULL(*expect, NULL);
}

T_DECL(fts_emptydir, "FTS preserves trailing slashes on empty directories (rdar://144349691)",
    T_META_NAMESPACE("Libc.regression"))
{
	char dirname[12] = "fts.XXXXXX";
	char *paths[] = { dirname, NULL };
	FTS *fts;
	FTSENT *ent;

	T_SETUPBEGIN;
	T_ASSERT_POSIX_ZERO(chdir(dt_tmpdir()), NULL);
	T_ASSERT_POSIX_NOTNULL(mkdtemp(dirname), "mkdtemp()");
	dirname[10] = '/';
	fts = fts_open(paths, FTS_PHYSICAL|FTS_NOCHDIR, NULL);
	T_ASSERT_POSIX_NOTNULL(fts, "fts_open()");
	T_SETUPEND;
	/* pre-order */
	T_ASSERT_POSIX_NOTNULL((ent = fts_read(fts)), NULL);
	T_EXPECT_EQ(FTS_D, ent->fts_info, NULL);
	T_EXPECT_EQ_STR(ent->fts_path, dirname, NULL);
	/* post-order */
	T_ASSERT_POSIX_NOTNULL((ent = fts_read(fts)), NULL);
	T_EXPECT_EQ(FTS_DP, ent->fts_info, NULL);
	T_EXPECT_EQ_STR(ent->fts_path, dirname, NULL);
	/* done */
	T_SETUPBEGIN;
	T_ASSERT_NULL((ent = fts_read(fts)), NULL);
	T_EXPECT_POSIX_ZERO(errno, "fts_read()");
	T_EXPECT_POSIX_SUCCESS(fts_close(fts), "fts_close()");
	dirname[10] = '\0';
	T_EXPECT_POSIX_ZERO(rmdir(dirname), NULL);
	T_SETUPEND;
}
