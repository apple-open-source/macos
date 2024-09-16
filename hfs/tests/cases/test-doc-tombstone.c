//
//  doc-tombstone.c
//  hfs
//
//  Created by Chris Suter on 8/12/15.
//
//

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/attr.h>
#include <unistd.h>

#include "hfs-tests.h"
#include "test-utils.h"
#include "disk-image.h"
#include "../../core/hfs_fsctl.h"

TEST(doc_tombstone, .run_as_root = true)

#define DOC_ID_DMG	"/tmp/test_doc_id.sparseimage"
#define DOC_ID_MNT	"/tmp/test_doc_id_mnt"

int run_doc_tombstone(__unused test_ctx_t *ctx)
{
	disk_image_opts_t opts = {
		.mount_point = DOC_ID_MNT,
		.enable_owners = true
	};

	disk_image_t *di = disk_image_create(DOC_ID_DMG, &opts);
	assert_no_err(chmod(DOC_ID_MNT, 0777));

	char *file;
	char *new;
	char *old;
	
	asprintf(&file, "%s/doc-tombstone", di->mount_point);
	asprintf(&new, "%s.new", file);
	asprintf(&old, "%s.old", file);
	
	int fd;
	
	struct attrlist al = {
		.bitmapcount = ATTR_BIT_MAP_COUNT,
		.commonattr = ATTR_CMN_DOCUMENT_ID
	};
	
	struct attrs {
		uint32_t len;
		uint32_t doc_id;
	} attrs;
	
	uint32_t orig_doc_id;
	
	unlink(file);
	rmdir(file);
	
	
	/*
	 * 1.a. Move file over existing
	 */
	
	fd = open(file, O_RDWR | O_CREAT, 0666);
	
	assert_with_errno(fd >= 0);
	
	assert_no_err(fchflags(fd, UF_TRACKED));
	
	assert_no_err(fgetattrlist(fd, &al, &attrs, sizeof(attrs),
							   FSOPT_ATTR_CMN_EXTENDED));
	
	orig_doc_id = attrs.doc_id;
	
	assert_no_err(close(fd));
	
	assert_with_errno((fd = open(new,
								 O_RDWR | O_CREAT, 0666)) >= 0);
	
	assert_no_err(rename(new, file));
	
	assert_no_err(fgetattrlist(fd, &al, &attrs, sizeof(attrs),
							   FSOPT_ATTR_CMN_EXTENDED));
	
	assert_equal(orig_doc_id, attrs.doc_id, "%u");
	
	assert_no_err(close(fd));
	assert_no_err(unlink(file));
	
	
	/*
	 * 1.b. Move directory over existing
	 */
	
	assert_no_err(mkdir(file, 0777));
	
	assert_no_err(chflags(file, UF_TRACKED));
	
	assert_no_err(getattrlist(file, &al, &attrs, sizeof(attrs),
							  FSOPT_ATTR_CMN_EXTENDED));
	
	orig_doc_id = attrs.doc_id;
	
	assert_no_err(mkdir(new, 0777));
	
	assert_no_err(rename(new, file));
	
	assert_no_err(getattrlist(file, &al, &attrs, sizeof(attrs),
							  FSOPT_ATTR_CMN_EXTENDED));
	
	assert_equal(orig_doc_id, attrs.doc_id, "%u");
	
	assert_no_err(rmdir(file));
	
	
	/*
	 * 2.a. Move original file out of the way, move new file into place, delete original
	 */
	
	fd = open(file, O_RDWR | O_CREAT, 0666);
	
	assert_with_errno(fd >= 0);
	
	assert_no_err(fchflags(fd, UF_TRACKED));
	
	assert_no_err(fgetattrlist(fd, &al, &attrs, sizeof(attrs),
							   FSOPT_ATTR_CMN_EXTENDED));
	
	orig_doc_id = attrs.doc_id;
	
	assert_no_err(close(fd));
	
	assert_with_errno((fd = open(new,
								 O_RDWR | O_CREAT, 0666)) >= 0);
	
	assert_with_errno(fd >= 0);
	
	assert_no_err(rename(file, old));
	assert_no_err(rename(new, file));
	
	assert_no_err(fgetattrlist(fd, &al, &attrs, sizeof(attrs),
							   FSOPT_ATTR_CMN_EXTENDED));
	
	assert_equal(orig_doc_id, attrs.doc_id, "%u");
	
	assert_no_err(close(fd));
	assert_no_err(unlink(old));
	assert_no_err(unlink(file));
	
	
	/*
	 * 2.b. Move original directory out of the way, move new directory into place, delete original
	 */
	
	assert_no_err(mkdir(file, 0777));
	
	assert_no_err(chflags(file, UF_TRACKED));
	
	assert_no_err(getattrlist(file, &al, &attrs, sizeof(attrs),
							  FSOPT_ATTR_CMN_EXTENDED));
	
	orig_doc_id = attrs.doc_id;
	
	assert_no_err(mkdir(new, 0777));
	
	assert_no_err(rename(file, old));
	assert_no_err(rename(new, file));
	
	assert_no_err(getattrlist(file, &al, &attrs, sizeof(attrs),
							  FSOPT_ATTR_CMN_EXTENDED));
	
	assert_equal(orig_doc_id, attrs.doc_id, "%u");
	
	assert_no_err(rmdir(old));
	assert_no_err(rmdir(file));
	
	
	/*
	 * 3.a. Delete original file, move new file into place
	 */
	
	assert_with_errno((fd = open(file,
								 O_RDWR | O_CREAT, 0666)) >= 0);
	
	assert_with_errno(fd >= 0);
	
	assert_no_err(fchflags(fd, UF_TRACKED));
	
	assert_no_err(fgetattrlist(fd, &al, &attrs, sizeof(attrs),
							   FSOPT_ATTR_CMN_EXTENDED));
	
	orig_doc_id = attrs.doc_id;
	
	assert_no_err(close(fd));
	
	assert_with_errno((fd = open(new,
								 O_RDWR | O_CREAT, 0666)) >= 0);
	
	assert_with_errno(fd >= 0);
	
	assert_no_err(unlink(file));
	
	assert_no_err(rename(new, file));
	
	assert_no_err(fgetattrlist(fd, &al, &attrs, sizeof(attrs),
							   FSOPT_ATTR_CMN_EXTENDED));
	
	assert_equal(orig_doc_id, attrs.doc_id, "%u");
	
	assert_no_err(close(fd));
	assert_no_err(unlink(file));
	
	
	/*
	 * 3.b. Delete original directory, move new directory into place
	 */
	
	assert_no_err(mkdir(file, 0777));
	
	assert_no_err(chflags(file, UF_TRACKED));
	
	assert_no_err(getattrlist(file, &al, &attrs, sizeof(attrs),
							  FSOPT_ATTR_CMN_EXTENDED));
	
	orig_doc_id = attrs.doc_id;
	
	assert_no_err(mkdir(new, 0777));
	
	assert_no_err(rmdir(file));
	assert_no_err(rename(new, file));
	
	assert_no_err(getattrlist(file, &al, &attrs, sizeof(attrs),
							  FSOPT_ATTR_CMN_EXTENDED));
	
	assert_equal(orig_doc_id, attrs.doc_id, "%u");
	
	assert_no_err(rmdir(file));
	
	
	/*
	 * 4.a. Delete original file, create new file in place
	 */
	
	assert_with_errno((fd = open(file,
								 O_RDWR | O_CREAT, 0666)) >= 0);
	
	assert_no_err(fchflags(fd, UF_TRACKED));
	
	assert_no_err(fgetattrlist(fd, &al, &attrs, sizeof(attrs),
							   FSOPT_ATTR_CMN_EXTENDED));
	
	orig_doc_id = attrs.doc_id;
	
	assert_no_err(close(fd));
	assert_no_err(unlink(file));
	
	assert_with_errno((fd = open(file,
								 O_RDWR | O_CREAT, 0666)) >= 0);
	
	assert_no_err(fgetattrlist(fd, &al, &attrs, sizeof(attrs),
							   FSOPT_ATTR_CMN_EXTENDED));
	
	assert_equal(orig_doc_id, attrs.doc_id, "%u");
	
	assert_no_err(close(fd));
	assert_no_err(unlink(file));
	
	
	/*
	 * 4.b. Delete original directory, create new directory in place
	 */
	
	assert_no_err(mkdir(file, 0777));
	
	assert_no_err(chflags(file, UF_TRACKED));
	
	assert_no_err(getattrlist(file, &al, &attrs, sizeof(attrs),
							  FSOPT_ATTR_CMN_EXTENDED));
	
	orig_doc_id = attrs.doc_id;
	
	assert_no_err(rmdir(file));
	
	assert_no_err(mkdir(file, 0777));
	
	assert_no_err(getattrlist(file, &al, &attrs, sizeof(attrs),
							  FSOPT_ATTR_CMN_EXTENDED));
	
	assert_equal(orig_doc_id, attrs.doc_id, "%u");
	
	assert_no_err(rmdir(file));

	// permissions tests
	char *src;
	char *dest;
	asprintf(&src, "%s/doc-id-transfer-src", di->mount_point);
	asprintf(&dest, "%s/doc-id-transfer-dest", di->mount_point);
	const uid_t user_uid = 501;

	// user fails to transfer from a file owned by root to a file owned by user
	assert_equal_int(geteuid(), 0);
	assert_with_errno((fd = open(src, O_RDWR | O_CREAT, 0644)) >= 0);
	assert_no_err(chflags(src, UF_TRACKED));
	assert_no_err(seteuid(user_uid));
	assert_with_errno((fd = open(dest, O_RDWR | O_CREAT, 0644)) >= 0);
	assert_call_fail(fsctl(src, HFSIOC_TRANSFER_DOCUMENT_ID, &fd, 0), EACCES);
	assert_no_err(close(fd));
	assert_no_err(seteuid(0));

	// user fails to transfer from a file owned by user to a file owned by root
	assert_no_err(chown(src, user_uid, -1));
	assert_no_err(chown(dest, 0, -1));
	assert_no_err(seteuid(user_uid));
	assert_with_errno((fd = open(dest, O_RDONLY)) >= 0);
	assert_call_fail(fsctl(src, HFSIOC_TRANSFER_DOCUMENT_ID, &fd, 0), EACCES);
	assert_no_err(close(fd));
	assert_no_err(seteuid(0));

	// user succeeds to transfer from a file owned by user to another file owned by user
	assert_no_err(chown(dest, user_uid, -1));
	assert_no_err(seteuid(user_uid));
	assert_with_errno((fd = open(dest, O_RDONLY)) >= 0);
	assert_no_err(fsctl(src, HFSIOC_TRANSFER_DOCUMENT_ID, &fd, 0));
	assert_no_err(close(fd));
	assert_no_err(seteuid(0));

	assert_no_err(unlink(src));
	assert_no_err(unlink(dest));

	return 0;
}
