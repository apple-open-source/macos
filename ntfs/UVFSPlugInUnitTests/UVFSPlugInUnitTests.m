//
//  UVFSPlugInUnitTests.m
//  UVFSPlugInUnitTests
//
//  Created by aburlyga on 5/15/20.
//

#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/errno.h>
#include <sys/mman.h>

#import <XCTest/XCTest.h>

#include <UserFS/UserVFS.h>

#include "Data.h"

void livefiles_plugin_init(UVFSFSOps **ops);

@interface Tests : XCTestCase

@property UVFSFSOps *fsops;
@property int fd;
@property void *storage;
@property livefiles_plugin_init_t plugInInit;

@end

@implementation Tests

- (void)setUp
{
    ssize_t res = 0;
    _fd = open("SHM_UVFS_PLUGIN_TEST",  O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
     _storage = mmap(NULL, 2 * 1024 * 1024, PROT_WRITE, MAP_SHARED, _fd, 0);
    do {
        ssize_t cur_res;

        cur_res = write(_fd, &testdata[res], 2 * 1024 * 1024 - res);
        if(cur_res < 0) {
            fprintf(stderr, "Error writing testdata: %zd (%d / "
                "%s)\n", cur_res, errno, strerror(errno));
            break;
        }
        fprintf(stderr, "wrote %zd bytes (total: %zd)\n", cur_res, res);
        res += cur_res;
    } while(res < 2 * 1024 * 1024);

    _plugInInit = livefiles_plugin_init;
    _plugInInit(&_fsops);
}

- (void)tearDown
{
    munmap(_storage, 2 * 1024 * 1024);
    ftruncate(_fd, 0);
    unlink("/SHM_UVFS_PLUGIN_TEST");
}

- (void)testUVFSPlugIn
{
    UVFSFileNode        rootNode;
    UVFSScanVolsRequest scanVolRequest;
    UVFSScanVolsReply   scanVolReply;
    UVFSFileNode        tmpNode;
    uint8_t             volCount = 0;

    XCTAssert(_fd >= 0, "fd is negative");
    XCTAssert(_storage != NULL, "storage is NULL");
    XCTAssert(_fsops != NULL, "fsops is NULL");

    XCTAssert(_fsops->fsops_init() == 0, "fsops init failed");
    XCTAssert(_fsops->fsops_taste(_fd) == 0, "fsops taste failed");

    scanVolRequest.sr_volid = 0;
    while (_fsops->fsops_scanvols(_fd, &scanVolRequest, &scanVolReply) != UVFS_SCANVOLS_EOF_REACHED)
    {
        XCTAssert(volCount < 255);

        XCTAssert(_fsops->fsops_check(_fd, scanVolReply.sr_volid, NULL, QUICK_CHECK) == 0);

        XCTAssert(_fsops->fsops_mount(_fd, scanVolReply.sr_volid, 0, NULL, NULL) == EINVAL,
                  "fsops mount with empty root node");
        XCTAssert(_fsops->fsops_mount(_fd, scanVolReply.sr_volid, 0, NULL, &rootNode) == 0,
                  "fsops mount with valid root node");

        volCount++;
        scanVolRequest.sr_volid = ++scanVolReply.sr_volid;
        memset(&scanVolReply, 0, sizeof(scanVolReply));
    }
    XCTAssert(volCount == 1);
    XCTAssert(scanVolReply.sr_volid == 0);
    XCTAssert(strcmp(scanVolReply.sr_volname, "SampleVol")); // TODO: Needs to be real one from the volume

    {
        union {
            LIFSAttributeValue val;
            char volname[MAXPATHLEN];
        } u;
        size_t retlen;

        XCTAssert(_fsops->fsops_getfsattr(rootNode, LI_FSATTR_VOLNAME,
            &u.val, sizeof(u), &retlen) == 0);
    }
    XCTAssert(_fsops->fsops_setfsattr(NULL, NULL, NULL, 0, NULL, 0) == EROFS);

    {
        UVFSFileAttributes attrs;

        memset(&attrs, 0, sizeof(UVFSFileAttributes));
        attrs.fa_validmask = UVFS_FA_SUPPORTED_VALIDMASK;
        XCTAssert(_fsops->fsops_getattr(rootNode, &attrs) == 0);
        XCTAssert(attrs.fa_validmask > 0);
        XCTAssert(attrs.fa_type == UVFS_FA_TYPE_DIR);
        XCTAssert(attrs.fa_mode > 0); //TODO: We should probably check that we at least have READ access
        XCTAssert(attrs.fa_size > 0); //TODO: is there a standard size on NTFS
        XCTAssert(attrs.fa_allocsize == 0); //TODO: Seems like a bug for now
        XCTAssert(attrs.fa_fileid == 2); //It's root vnode
        XCTAssert(attrs.fa_parentid == 1); //TODO: I think this is wrong, I will need to check on other FSs
        XCTAssert(attrs.fa_atime.tv_sec >= 0);
        XCTAssert(attrs.fa_mtime.tv_sec >= 0);
        XCTAssert(attrs.fa_ctime.tv_sec >= 0);
        XCTAssert(attrs.fa_birthtime.tv_sec >= 0);
        //TODO: follow up on fa_backuptime
        //TODO: follow up on fa_addedtime
    }


    XCTAssert(_fsops->fsops_setattr(NULL, NULL, NULL) == EROFS);

    XCTAssert(_fsops->fsops_lookup(rootNode, "a", &tmpNode) == ENOENT);
    XCTAssert(_fsops->fsops_lookup(rootNode, "f1", &tmpNode) == 0);
    {
        char buf[512];
        size_t bytes_read = 0;
        XCTAssert(_fsops->fsops_read(tmpNode, 0, sizeof(buf), buf,
                &bytes_read) == 0);
        XCTAssert(bytes_read == 512);
        //XCTAssert(memcmp(buf, refbuf, 512) == 0);
    }

    XCTAssert(_fsops->fsops_lookup(rootNode, "textfile.txt", &tmpNode) == 0);
    {
        char buf[512];
        size_t bytes_read = 0;
        UVFSFileAttributes fileAttrs;

        memset(buf, 0, sizeof(buf));
        XCTAssert(_fsops->fsops_getattr(tmpNode, &fileAttrs) == 0);

        XCTAssert(_fsops->fsops_read(tmpNode, 0, sizeof(buf), buf,
                &bytes_read) == 0);
        XCTAssert(bytes_read <= fileAttrs.fa_size);
        //XCTAssert(memcmp(buf, refbuf, 512) == 0);
    }

    XCTAssert(_fsops->fsops_reclaim(tmpNode) == 0);
    {
        char buf[512];
        size_t bytes_read = 0;
        LIFileAttributes_t linkattrs;
        XCTAssert(_fsops->fsops_readlink(rootNode, 0, sizeof(buf),
                &bytes_read, &linkattrs) == EINVAL);
    }

    XCTAssert(_fsops->fsops_write(NULL, 0, 0, NULL, NULL) == EROFS);
    XCTAssert(_fsops->fsops_create(NULL, NULL, NULL, NULL) == EROFS);
    XCTAssert(_fsops->fsops_mkdir(NULL, NULL, NULL, NULL) == EROFS);
    XCTAssert(_fsops->fsops_symlink(NULL, NULL, NULL, NULL, NULL) == EROFS);
    XCTAssert(_fsops->fsops_remove(NULL, NULL, NULL) == EROFS);
    XCTAssert(_fsops->fsops_rmdir(NULL, NULL) == EROFS);
    XCTAssert(_fsops->fsops_rename(NULL, NULL, NULL, NULL, NULL, NULL, 0) == EROFS);
    {
        char buf[1024];
        size_t read_bytes = 0;
        uint64_t verifier = UVFS_DIRCOOKIE_VERIFIER_INITIAL;
#if 0
        size_t run = 0;
#endif
        uint64_t lastnextcookie = 0;

        do {
            int err;
            size_t i;

#if 0
            fprintf(stderr, "Run %zu:\n", ++run);
#endif
            XCTAssert((err = _fsops->fsops_readdir(rootNode,
                    buf, sizeof(buf), lastnextcookie,
                    &read_bytes, &verifier)) == 0);
            if (err) {
                break;
            }

            i = 0;
            while(i < sizeof(buf)) {
                LIDirEntry_t *const cur_entry =
                    (LIDirEntry_t*) &buf[i];
                lastnextcookie = cur_entry->de_nextcookie;
#if 0
                fprintf(stderr, "i=%zu (->%zu) name=\"%.*s\" "
                    "de_nextcookie=%" PRIu64 " "
                    "de_nextrec=%" PRIu16 "\n",
                    i,
                    i + cur_entry->de_nextrec,
                    cur_entry->de_namelen,
                    cur_entry->de_name,
                    cur_entry->de_nextcookie,
                    cur_entry->de_nextrec);
#endif
                if (!cur_entry->de_nextrec) {
                    break;
                }

                i += cur_entry->de_nextrec;
            }
        } while (lastnextcookie != LI_DIRCOOKIE_EOF);

        XCTAssert(_fsops->fsops_readdir(rootNode, buf, sizeof(buf),
            lastnextcookie, &read_bytes, &verifier) ==
            LI_READDIR_EOF_REACHED);
    }
    {
        char buf[1024];
        size_t read_bytes = 0;
        uint64_t verifier = UVFS_DIRCOOKIE_VERIFIER_INITIAL;
#if 0
        size_t run = 0;
#endif
        uint64_t lastnextcookie = 0;

        do {
            int err;
            size_t i;

#if 0
            fprintf(stderr, "Run %zu:\n", ++run);
#endif
            XCTAssert((err = _fsops->fsops_readdirattr(rootNode,
                    buf, sizeof(buf), lastnextcookie,
                    &read_bytes, &verifier)) == 0);
            if (err) {
                break;
            }

            i = 0;
            while(i < sizeof(buf)) {
                LIDirEntryAttr_t *const cur_entry =
                    (LIDirEntryAttr_t*) &buf[i];
                lastnextcookie = cur_entry->dea_nextcookie;
#if 0
                fprintf(stderr, "i=%zu (->%zu) name=\"%.*s\" "
                    "dea_nextcookie=%" PRIu64 " "
                    "dea_nextrec=%" PRIu16 " "
                    "dea_nameoff=%" PRIu16 "\n",
                    i,
                    i + cur_entry->dea_nextrec,
                    cur_entry->dea_namelen,
                    LI_DIRENTRYATTR_NAMEPTR(cur_entry),
                    cur_entry->dea_nextcookie,
                    cur_entry->dea_nextrec,
                    cur_entry->dea_nameoff);
#endif
                if (!cur_entry->dea_nextrec) {
                    break;
                }

                XCTAssert(cur_entry->dea_nextrec ==
                    LI_DIRENTRYATTR_RECLEN(cur_entry,
                    cur_entry->dea_namelen));

                i += cur_entry->dea_nextrec;
            }
        } while (lastnextcookie != LI_DIRCOOKIE_EOF);

        XCTAssert(_fsops->fsops_readdirattr(rootNode, buf, sizeof(buf),
            lastnextcookie, &read_bytes, &verifier) ==
            LI_READDIR_EOF_REACHED);
    }

    XCTAssert(_fsops->fsops_link(NULL, NULL, NULL, NULL, NULL) == EROFS);

    do {
        int err;
        UVFSFileNode xattrFileNode = NULL;
        size_t byte_count = 0;
        char buf[512];

        XCTAssert((err = _fsops->fsops_lookup(rootNode, "filewithxattr",
                &xattrFileNode)) == 0);
        if (err) {
            break;
        }

        /* Check that getting the size works. */
        byte_count = 0;
        XCTAssert(_fsops->fsops_getxattr(xattrFileNode,
            "com.tuxera.test", 0, 0, &byte_count) == 0);
        XCTAssert(byte_count == 12);
        /* Check that getting the data works. */
        memset(buf, 0, sizeof(buf));
        XCTAssert(_fsops->fsops_getxattr(xattrFileNode,
            "com.tuxera.test", buf, byte_count, &byte_count) == 0);
        XCTAssert(byte_count == 12);
#if 0
        fprintf(stderr, "Got xattr value: %s\n", buf);
#endif

        XCTAssert(_fsops->fsops_setxattr(xattrFileNode,
            "com.tuxera.test2", buf, sizeof(buf), UVFSXattrHowSet)
            == EROFS);

        /* Check that getting the size of the xattr list works. */
        byte_count = 0;
        XCTAssert(_fsops->fsops_listxattr(xattrFileNode, NULL, 0,
            &byte_count) == 0);
#if 0
        fprintf(stderr, "byte_count: %zu\n", byte_count);
#endif
        XCTAssert(byte_count == 16);
        /* Check that getting the xattr list works. */
        memset(buf, 0, sizeof(buf));
        XCTAssert(_fsops->fsops_listxattr(xattrFileNode, buf,
            byte_count, &byte_count) == 0);
        XCTAssert(byte_count == 16);
        XCTAssert(
            buf[0]  == 'c' && buf[1]  == 'o' && buf[2]  == 'm' &&
            buf[3]  == '.' && buf[4]  == 't' && buf[5]  == 'u' &&
            buf[6]  == 'x' && buf[7]  == 'e' && buf[8]  == 'r' &&
            buf[9]  == 'a' && buf[10] == '.' && buf[11] == 't' &&
            buf[12] == 'e' && buf[13] == 's' && buf[14] == 't' &&
            buf[15] == '\0');
#if 0
        fprintf(stderr, "Got xattr list: %s\n", buf);
        fprintf(stderr, "byte_count: %zu\n", byte_count);
#endif
        XCTAssert(_fsops->fsops_reclaim(xattrFileNode) == 0);
    } while(0);

    XCTAssert(_fsops->fsops_clonefile(NULL, NULL, NULL, NULL, 0, NULL) == EROFS);

    XCTAssert(_fsops->fsops_scandir(NULL, NULL, NULL) == ENOTSUP);
    XCTAssert(_fsops->fsops_scanids(NULL, 0, NULL, 0, NULL) == ENOTSUP);

    XCTAssert(_fsops->fsops_sync(NULL) == 0);
    XCTAssert(_fsops->fsops_unmount(rootNode, 0) == 0);
    _fsops->fsops_fini();
}

- (void)testPerformanceExample {
    // This is an example of a performance test case.
    [self measureBlock:^{
        // Put the code you want to measure the time of here.
    }];
}

@end
