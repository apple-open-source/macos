#include "test.h"
#include "archive_mac.h"

#ifdef HAVE_MAC_QUARANTINE
static void
perform_test(int skipdir) {
	const char *refname = "test_read_extract_quarantine.tar.gz";
	const int qflags = QTN_FLAG_SANDBOX | QTN_FLAG_DOWNLOAD | QTN_FLAG_TRANSLOCATE;
	const time_t qtime = 169019013;
	struct archive *a;
	struct archive_entry *ae;
	qtn_file_t qf;
	const char *ident;
	int err;

	extract_reference_file(refname);
	assert((qf = qtn_file_alloc()) != NULL);
	assertEqualInt(0, qtn_file_set_flags(qf, qflags));
	qtn_file_set_timestamp(qf, qtime);
	assertEqualInt(0, qtn_file_apply_to_path(qf, refname));
	qtn_file_free(qf);

	assert((a = archive_read_new()) != NULL);
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
	assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a, refname, 0));

	while (archive_read_next_header(a, &ae) == 0)
		assertA(archive_read_extract(a, ae, ARCHIVE_EXTRACT_PERM) == 0);
	archive_read_free(a);

	/*
	 * Verify that the file contained within the tarball has inherited
	 * the tarball's quarantine status.
	 */
	assertNonEmptyFile("dir/file");
	assert((qf = qtn_file_alloc()) != NULL);
	assertEqualInt(0, qtn_file_init_with_path(qf, "dir/file"));
	assertEqualInt(qflags, qtn_file_get_flags(qf));
	assertEqualInt(qtime, qtn_file_get_timestamp(qf));
	qtn_file_free(qf);

	/* Skip checking the directory? */
	if (skipdir)
		return;

	/*
	 * Verify that the directory that was implicitly created by
	 * libarchive to contain the file also inherited the tarball's
	 * quarantine status.
	 */
	assert((qf = qtn_file_alloc()) != NULL);
	assertEqualInt(0, qtn_file_init_with_path(qf, "dir"));
	assertEqualInt(qflags, qtn_file_get_flags(qf));
	assertEqualInt(qtime, qtn_file_get_timestamp(qf));
	qtn_file_free(qf);
}
#endif /* HAVE_MAC_QUARANTINE */

DEFINE_TEST(test_read_extract_quarantine) {
#ifdef HAVE_MAC_QUARANTINE
	perform_test(0);
#endif /* HAVE_MAC_QUARANTINE */
}

DEFINE_TEST(test_read_extract_quarantine_again) {
#ifdef HAVE_MAC_QUARANTINE
	/*
	 * Perform the same test again, but create the directory and file
	 * in advance.  We expect the file to be quarantined, but not the
	 * directory, because it isn't explicitly listed in the archive.
	 */
	assertMakeDir("dir", 0755);
	assertEqualInt(0, close(open("dir/file", O_CREAT|O_EXCL, 0644)));
	perform_test(1);
#endif /* HAVE_MAC_QUARANTINE */
}
