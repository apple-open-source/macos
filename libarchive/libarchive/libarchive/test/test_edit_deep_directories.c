#include "test.h"
#include <unistd.h>

static char *hook_link_name = NULL;
static char *hook_target = NULL;

DEFINE_TEST_HOOK(edit_deep_directories_after_create_dir)
{
	if (hook_link_name == NULL || hook_target == NULL)
		return;

	rmdir(hook_link_name);
	symlink(hook_target, hook_link_name);

	hook_link_name = NULL;
	hook_target = NULL;
	RECORD_TEST_HOOK_RUN(edit_deep_directories_after_create_dir);
}

/*
 * Test for a 'chdir' race in 'edit_deep_directories' after 'create_dir'.
 * Creates a directory hierarchy with a path larger than PATH_MAX and replaces
 * the penultimate directory with a symlink.
 */
DEFINE_TEST(test_archive_write_edit_deep_directories_chdir_race)
{
	int i;
	struct archive *a;
	struct archive_entry *entry;
	char ok_path[PATH_MAX * 2];
	char fail_path[PATH_MAX];
	char link_path[PATH_MAX];
	char link_target[PATH_MAX];
	char component_buf[103];

	/* Build the  0000.../.../999.../tmp directory hierarchy. */
	memset(ok_path, '0', 102);
	ok_path[102] = 0;
	for (i=1; i < 10; i++) {
		memset(component_buf, i+'0', 102);
		snprintf(ok_path, PATH_MAX * 2, "%s/%s", ok_path, component_buf);
		component_buf[102] = 0;
	}
	snprintf(ok_path, PATH_MAX * 2, "%s/tmp", ok_path);
	/* $(CWD)/dir2/999.../tmp */
	getcwd(fail_path, PATH_MAX);
	snprintf(fail_path, PATH_MAX, "%s/dir2/%s/tmp", fail_path, component_buf);
	/* 0000.../.../88888.../ */
	strncpy(link_path, ok_path, strrchr(ok_path, '8')+1 - ok_path);
	getcwd(link_target, PATH_MAX);
	snprintf(link_target, PATH_MAX, "%s/dir2/", link_target);

	hook_link_name = link_path;
	hook_target = link_target;
	mkdir("dir2", 0755);

	a = archive_write_disk_new();
	archive_write_disk_set_options(a,
	    ARCHIVE_EXTRACT_TIME | ARCHIVE_EXTRACT_SECURE_SYMLINKS);
	entry = archive_entry_new();
	archive_entry_set_pathname(entry, ok_path);
	archive_entry_set_filetype(entry, AE_IFREG);
	archive_entry_set_mode(entry, AE_IFREG | 0755);
	if (archive_write_header(a, entry) == ARCHIVE_OK)
		archive_write_finish_entry(a);
	archive_write_free(a);
	archive_entry_free(entry);

	assertHookRan(edit_deep_directories_after_create_dir);
	assertFileNotExists(fail_path);

	/* Cleanup. */
	memset(component_buf, '0', 102);
	component_buf[102] = 0;
	remove("dir2");
	remove(component_buf);
}
