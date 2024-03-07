//
//  test_archive_quarantine.c
//  libarchive_test
//
//  Created by justin on 12/8/23.
//

#include "test.h"
#include <archive.h>
#ifdef HAVE_MAC_QUARANTINE
#include <quarantine.h>
#endif HAVE_MAC_QUARANTINE

#define BUFF_SIZE 1000000
#define FILE_BUFF_SIZE 100000

DEFINE_TEST(test_read_extract_quarantine) {
#ifdef HAVE_MAC_QUARANTINE
    const char *refname = "test_write_disk_mac_metadata.tar.gz";
    struct archive *a;
    struct archive_entry *ae;

    extract_reference_file(refname);

    systemf("qtntool file_set -f 0x03 %s", refname);

    assert((a = archive_read_new()) != NULL);
    assertEqualIntA(a, ARCHIVE_OK, archive_read_support_filter_all(a));
    assertEqualIntA(a, ARCHIVE_OK, archive_read_support_format_all(a));
    assertEqualIntA(a, ARCHIVE_OK, archive_read_open_filename(a, refname, 512 * 20));

    assertA(0 == archive_read_next_header(a, &ae));
    assertA(0 == archive_read_extract(a, ae, ARCHIVE_EXTRACT_PERM));

    qtn_file_t qf = qtn_file_alloc();
    int err = 0;

    err = qtn_file_init_with_path(qf, "file3");
    assert(!err);

    archive_read_free(a);
    qtn_file_free(qf);
#endif // HAVE_MAC_QUARANTINE
}
