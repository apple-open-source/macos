#include "test.h"

#include <unistd.h>

static char *hook_link_name = NULL;
static char *hook_target = NULL;

DEFINE_TEST_HOOK(after_check_symlinks_fsobj)
{
	if (hook_link_name == NULL || hook_target == NULL)
		return;

	rmdir(hook_link_name);
	symlink(hook_target, hook_link_name);

	hook_link_name = NULL;
	hook_target = NULL;
	RECORD_TEST_HOOK_RUN(after_check_symlinks_fsobj);
}

static void
write_fsobj(int mode, const char *ok_path)
{
	struct archive *a;
	struct archive_entry *entry;

	a = archive_write_disk_new();
	archive_write_disk_set_options(a,
	    ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_SECURE_SYMLINKS);
	entry = archive_entry_new();
	archive_entry_set_pathname(entry, ok_path);
	archive_entry_set_filetype(entry, mode);
	archive_entry_set_mode(entry, mode | 0755);
	if (archive_write_header(a, entry) == ARCHIVE_OK)
		archive_write_finish_entry(a);
	archive_write_free(a);
	archive_entry_free(entry);
}

#define DEFINE_SYMLINK_RACE_TEST(mode)							\
	DEFINE_TEST(test_archive_write_secure_symlink_race_##mode)	\
	{															\
																\
		char ok_path[PATH_MAX];									\
		char fail_path[PATH_MAX];								\
																\
		snprintf(ok_path, PATH_MAX, "%s/%s", "OK", #mode);		\
		snprintf(fail_path, PATH_MAX, "%s/%s", "FAIL", #mode);	\
																\
		mkdir("OK", 0755);										\
		mkdir("FAIL", 0755);									\
		hook_link_name = "OK";									\
		hook_target = "FAIL";									\
																\
		write_fsobj(mode, ok_path);								\
		assertHookRan(after_check_symlinks_fsobj);				\
		assertFileNotExists(fail_path);							\
																\
		remove("OK");											\
		remove("FAIL");											\
	}

/*
 * A collection of tests designed to check for symlink races when creating
 * different types of filesystem objects using libarchive.
 */
DEFINE_SYMLINK_RACE_TEST(AE_IFMT);
DEFINE_SYMLINK_RACE_TEST(AE_IFREG);
DEFINE_SYMLINK_RACE_TEST(AE_IFLNK);
DEFINE_SYMLINK_RACE_TEST(AE_IFSOCK);
DEFINE_SYMLINK_RACE_TEST(AE_IFCHR);
DEFINE_SYMLINK_RACE_TEST(AE_IFBLK);
DEFINE_SYMLINK_RACE_TEST(AE_IFDIR);
DEFINE_SYMLINK_RACE_TEST(AE_IFIFO);
