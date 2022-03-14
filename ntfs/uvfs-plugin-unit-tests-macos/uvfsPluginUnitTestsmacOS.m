//
//  uvfs_plugin_unit_tests.m
//  uvfs-plugin-unit-tests
//
//  Created by Alex Burlyga on 5/27/21.
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

@interface uvfsPluginUnitTestsmacOS : XCTestCase

@property UVFSFSOps *fsops;
@property int fd;
@property void *storage;
@property livefiles_plugin_init_t plugInInit;

@end

@implementation uvfsPluginUnitTestsmacOS

- (void)setUp {
    ssize_t res = 0;
    int writeError = 0;

    _fd = open("SHM_UVFS_PLUGIN_TEST",  O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
     _storage = mmap(NULL, 2 * 1024 * 1024, PROT_WRITE, MAP_SHARED, _fd, 0);
    do {
        ssize_t cur_res;

        cur_res = write(_fd, &testdata[res], 2 * 1024 * 1024 - res);
        if(cur_res < 0) {
            writeError = errno;
            fprintf(stderr, "Error writing testdata: %zd (%d / "
                "%s)\n", cur_res, writeError, strerror(writeError));
            break;
        }
        fprintf(stderr, "wrote %zd bytes (total: %zd)\n", cur_res, res);
        res += cur_res;
    } while(res < 2 * 1024 * 1024);

    _plugInInit = livefiles_plugin_init;
    _plugInInit(&_fsops);

    XCTAssert(_fd >= 0, "fd is negative");
    XCTAssert(_storage != NULL, "storage is NULL");
    XCTAssert(writeError == 0, "error while creating filesystem");
    XCTAssert(_fsops != NULL, "fsops is NULL");
    //TODO: verify that all function pointers are there.
}

- (void)tearDown {
    munmap(_storage, 2 * 1024 * 1024);
    ftruncate(_fd, 0);
    unlink("/SHM_UVFS_PLUGIN_TEST");
}

// Run through all the FSOps and ensure that we get expected errors for the one we do not support
// and basic functionality is there for the ones we do support.

//Tests following sequence: while(scanvols() != EOF) { fsops_check(); fsops_mount() } fsops_sync(); fsops_unmount();
- (void)testMountSequence {
    UVFSFileNode        rootNode;
    UVFSScanVolsRequest scanVolRequest;
    UVFSScanVolsReply   scanVolReply;
    uint8_t             volCount = 0;

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

        XCTAssert(scanVolReply.sr_volid == volCount); //We expect only one volume, with volid of 0
        XCTAssert(scanVolReply.sr_volac == UAC_UNLOCKED); //We do not support encrypted volumes
        XCTAssert(scanVolReply.sr_readOnly == true); // We only support read-only access for now

        volCount++;
        scanVolRequest.sr_volid = ++scanVolReply.sr_volid;
        memset(&scanVolReply, 0, sizeof(scanVolReply));
    }
    XCTAssert(volCount == 1);

    XCTAssert(_fsops->fsops_sync(NULL) == 0);
    XCTAssert(_fsops->fsops_unmount(rootNode, 0) == 0);
    _fsops->fsops_fini();
}

- (int)mountTestVolume:(UVFSFileNode *)rootNodeOut
{
    UVFSFileNode node = NULL;

    XCTAssert(_fsops->fsops_init() == 0);
    XCTAssert(_fsops->fsops_taste(_fd) == 0);
    XCTAssert(_fsops->fsops_check(_fd, 0, NULL, QUICK_CHECK) == 0);
    XCTAssert(_fsops->fsops_mount(_fd, 0, 0, NULL, &node) == 0);
    XCTAssert(node != NULL);
    *rootNodeOut = node;

    return 0;
}

- (int)unmountTestVolume:(UVFSFileNode)rootNode
{
    XCTAssert(_fsops->fsops_sync(NULL) == 0);
    XCTAssert(_fsops->fsops_unmount(rootNode, 0) == 0);
    _fsops->fsops_fini();

    return 0;
}

// Since we are read-only, all of the operations tested as one test, and we just verify we get expected error
- (void)testWriteOperations
{
    XCTAssert(_fsops->fsops_setfsattr(NULL, NULL, NULL, 0, NULL, 0) == EROFS);

    XCTAssert(_fsops->fsops_setattr(NULL, NULL, NULL) == EROFS);
    XCTAssert(_fsops->fsops_write(NULL, 0, 0, NULL, NULL) == EROFS);
    XCTAssert(_fsops->fsops_create(NULL, NULL, NULL, NULL) == EROFS);
    XCTAssert(_fsops->fsops_mkdir(NULL, NULL, NULL, NULL) == EROFS);
    XCTAssert(_fsops->fsops_symlink(NULL, NULL, NULL, NULL, NULL) == EROFS);
    XCTAssert(_fsops->fsops_remove(NULL, NULL, NULL) == EROFS);
    XCTAssert(_fsops->fsops_rmdir(NULL, NULL) == EROFS);
    XCTAssert(_fsops->fsops_rename(NULL, NULL, NULL, NULL, NULL, NULL, 0) == EROFS);
    XCTAssert(_fsops->fsops_link(NULL, NULL, NULL, NULL, NULL) == EROFS);
    XCTAssert(_fsops->fsops_clonefile(NULL, NULL, NULL, NULL, 0, NULL) == EROFS);
}

//This ops are currently not supported
- (void)testNotSupportedOps
{
    XCTAssert(_fsops->fsops_scandir(NULL, NULL, NULL) == ENOTSUP);
    XCTAssert(_fsops->fsops_scanids(NULL, 0, NULL, 0, NULL) == ENOTSUP);
}

#pragma mark fsops_getfsattr() tests
- (void)testPathConfLinkMax
{
    size_t              retlen;
    UVFSFileNode        rootNode;
    LIFSAttributeValue  val;

    XCTAssert([self mountTestVolume:&rootNode] == 0);

    XCTAssert(_fsops->fsops_getfsattr(rootNode, LI_FSATTR_PC_LINK_MAX, &val, sizeof(val), &retlen) == 0);
    XCTAssert(retlen == 8); // number
    XCTAssert(val.fsa_number == 1); //No hardlinks? XXXab: Follow up with Taxera on this

    XCTAssert([self unmountTestVolume:rootNode] == 0);
}

- (void)testPathConfNameMax
{
    size_t              retlen;
    UVFSFileNode        rootNode;
    LIFSAttributeValue  val;

    XCTAssert([self mountTestVolume:&rootNode] == 0);

    XCTAssert(_fsops->fsops_getfsattr(rootNode, LI_FSATTR_PC_NAME_MAX, &val, sizeof(val), &retlen) == 0);
    XCTAssert(retlen == 8); // number
    XCTAssert(val.fsa_number == 255);

    XCTAssert([self unmountTestVolume:rootNode] == 0);
}

- (void)testPathConfFilesizebits
{
    size_t              retlen;
    UVFSFileNode        rootNode;
    LIFSAttributeValue  val;

    XCTAssert([self mountTestVolume:&rootNode] == 0);

    XCTAssert(_fsops->fsops_getfsattr(rootNode, LI_FSATTR_PC_FILESIZEBITS, &val, sizeof(val), &retlen) == 0);
    XCTAssert(retlen == 8); // number
    XCTAssert(val.fsa_number == 63); // XXXab: seems susspicious, follow up with Tuxera

    XCTAssert([self unmountTestVolume:rootNode] == 0);
}

- (void)testPathConfXattrFilesizebits
{
    size_t              retlen;
    UVFSFileNode        rootNode;
    LIFSAttributeValue  val;

    XCTAssert([self mountTestVolume:&rootNode] == 0);

    //XXXab: Followup with Tuxera, since we do support xattrs on NTFS
    XCTAssert(_fsops->fsops_getfsattr(rootNode, LI_FSATTR_PC_XATTR_SIZE_BITS, &val, sizeof(val), &retlen) == EINVAL);

    XCTAssert([self unmountTestVolume:rootNode] == 0);
}

- (void)testPathConfNoTrunc
{
    size_t              retlen;
    UVFSFileNode        rootNode;
    LIFSAttributeValue  val;

    XCTAssert([self mountTestVolume:&rootNode] == 0);

    XCTAssert(_fsops->fsops_getfsattr(rootNode, LI_FSATTR_PC_NO_TRUNC, &val, sizeof(val), &retlen) == 0);
    XCTAssert(retlen == 8); // bool
    //XCTAssert(val.fsa_bool == true);
    XCTAssert(val.fsa_number == 200112); // hardcoded in plug-in

    XCTAssert([self unmountTestVolume:rootNode] == 0);
}

- (void)testBlockSize
{
    size_t              retlen;
    UVFSFileNode        rootNode;
    LIFSAttributeValue  val;

    XCTAssert([self mountTestVolume:&rootNode] == 0);

    XCTAssert(_fsops->fsops_getfsattr(rootNode, LI_FSATTR_BLOCKSIZE, &val, sizeof(val), &retlen) == 0);
    XCTAssert(retlen == 8); // number
    XCTAssert(val.fsa_number == 4096);

    XCTAssert([self unmountTestVolume:rootNode] == 0);
}

- (void)testIOSize
{
    size_t              retlen;
    UVFSFileNode        rootNode;
    LIFSAttributeValue  val;

    XCTAssert([self mountTestVolume:&rootNode] == 0);

    XCTAssert(_fsops->fsops_getfsattr(rootNode, LI_FSATTR_IOSIZE, &val, sizeof(val), &retlen) == 0);
    XCTAssert(retlen == 8); // number
    XCTAssert(val.fsa_number == 67108864); // XXXab: ha? Followup with Tuxera

    XCTAssert([self unmountTestVolume:rootNode] == 0);
}

- (void)testTotalBlocks
{
    size_t              retlen;
    UVFSFileNode        rootNode;
    LIFSAttributeValue  val;

    XCTAssert([self mountTestVolume:&rootNode] == 0);

    XCTAssert(_fsops->fsops_getfsattr(rootNode, LI_FSATTR_TOTALBLOCKS, &val, sizeof(val), &retlen) == 0);
    XCTAssert(retlen == 8); // number
    XCTAssert(val.fsa_number == 511);

    XCTAssert([self unmountTestVolume:rootNode] == 0);
}

- (void)testBlocksFree
{
    size_t              retlen;
    UVFSFileNode        rootNode;
    LIFSAttributeValue  val;

    XCTAssert([self mountTestVolume:&rootNode] == 0);

    XCTAssert(_fsops->fsops_getfsattr(rootNode, LI_FSATTR_BLOCKSFREE, &val, sizeof(val), &retlen) == 0);
    XCTAssert(retlen == 8); // number
    XCTAssert(val.fsa_number == 25);

    XCTAssert([self unmountTestVolume:rootNode] == 0);
}

- (void)testBlocksAvailable
{
    size_t              retlen;
    UVFSFileNode        rootNode;
    LIFSAttributeValue  val;

    XCTAssert([self mountTestVolume:&rootNode] == 0);

    XCTAssert(_fsops->fsops_getfsattr(rootNode, LI_FSATTR_BLOCKSAVAIL, &val, sizeof(val), &retlen) == 0);
    XCTAssert(retlen == 8); // number
    XCTAssert(val.fsa_number == 25);

    XCTAssert([self unmountTestVolume:rootNode] == 0);
}

- (void)testBlocksUsed
{
    size_t              retlen;
    UVFSFileNode        rootNode;
    LIFSAttributeValue  val;

    XCTAssert([self mountTestVolume:&rootNode] == 0);

    XCTAssert(_fsops->fsops_getfsattr(rootNode, LI_FSATTR_BLOCKSUSED, &val, sizeof(val), &retlen) == 0);
    XCTAssert(retlen == 8); // number
    XCTAssert(val.fsa_number == 486);

    XCTAssert([self unmountTestVolume:rootNode] == 0);
}

- (void)testCNAME
{
    size_t              retlen;
    UVFSFileNode        rootNode;
    union {
        LIFSAttributeValue val;
        char volname[MAXPATHLEN];
    } u;

    XCTAssert([self mountTestVolume:&rootNode] == 0);

    //XXXab: Follow up with Tuxera
    XCTAssert(_fsops->fsops_getfsattr(rootNode, LI_FSATTR_CNAME, &u.val, 0, &retlen) == ENOTSUP);

    XCTAssert([self unmountTestVolume:rootNode] == 0);
}

- (void)testFSTypename
{
    size_t              retlen;
    UVFSFileNode        rootNode;
    LIFSAttributeValue val;
    char *fstypename;

    XCTAssert([self mountTestVolume:&rootNode] == 0);

    XCTAssert(_fsops->fsops_getfsattr(rootNode, LI_FSATTR_FSTYPENAME, &val, 0, &retlen) == E2BIG);
    fstypename = (char *)calloc(1, retlen);
    XCTAssert(fstypename != NULL);
    XCTAssert(_fsops->fsops_getfsattr(rootNode, LI_FSATTR_FSTYPENAME, (void *)fstypename, retlen, &retlen) == 0);
    XCTAssert(retlen == 5);
    XCTAssert(strcmp(fstypename, "ntfs") == 0);

    XCTAssert([self unmountTestVolume:rootNode] == 0);
}

- (void)testFSSubTypename
{
    size_t              retlen;
    UVFSFileNode        rootNode;
    LIFSAttributeValue val;
    char *fstypename;

    XCTAssert([self mountTestVolume:&rootNode] == 0);

    XCTAssert(_fsops->fsops_getfsattr(rootNode, LI_FSATTR_FSSUBTYPE, &val, 0, &retlen) == E2BIG);
    fstypename = (char *)calloc(1, retlen);
    XCTAssert(fstypename != NULL);
    XCTAssert(_fsops->fsops_getfsattr(rootNode, LI_FSATTR_FSSUBTYPE, (void *)fstypename, retlen, &retlen) == 0);
    XCTAssert(retlen == 8);
    //XXXab: Followup with Tuxera, we are getting 8 bytes in, but not clear what is in the buffer
    //XCTAssert(strcmp(fstypename, "ntfs") == 0, "got: %s", fstypename);

    XCTAssert([self unmountTestVolume:rootNode] == 0);
}

- (void)testFSLocation
{
    size_t              retlen;
    UVFSFileNode        rootNode;
    LIFSAttributeValue val;

    XCTAssert([self mountTestVolume:&rootNode] == 0);

    //XXXab: Follow-up with Tuxera
    XCTAssert(_fsops->fsops_getfsattr(rootNode, LI_FSATTR_FSLOCATION, &val, 0, &retlen) == ENOTSUP);

    XCTAssert([self unmountTestVolume:rootNode] == 0);
}

- (void)testVolumeName
{
    size_t              retlen;
    UVFSFileNode        rootNode;
    union {
        LIFSAttributeValue val;
        char volname[MAXPATHLEN];
    } u;

    XCTAssert([self mountTestVolume:&rootNode] == 0);

    XCTAssert(_fsops->fsops_getfsattr(rootNode, LI_FSATTR_VOLNAME, &u.val, sizeof(u), &retlen) == 0);
    XCTAssert(retlen == 12, "unexpected volume name length returned from fsops_getfsattr()");
    XCTAssert(strcmp(u.volname, "ntfstest 2m") == 0, "unexpected volume name from fsops_getfsattr()");

    XCTAssert([self unmountTestVolume:rootNode] == 0);
}

- (void)testVolumeUUID
{
    UVFSFileNode        rootNode;
    NSString *volUUIDTry1 = nil;
    NSString *volUUIDTry2 = nil;
    NSString *volUUIDTry3 = nil;
    NSString *volUUIDTry4 = nil;

    XCTAssert([self mountTestVolume:&rootNode] == 0);
    {
        LIFSAttributeValue val;
        NSMutableData *mutableResult = nil;
        size_t retlen;

        XCTAssert(_fsops->fsops_getfsattr(rootNode, LI_FSATTR_VOLUUID, &val, 0UL, &retlen) == E2BIG);
        mutableResult = [[NSMutableData alloc] initWithLength:retlen];
        XCTAssert(mutableResult != nil);
        XCTAssert(_fsops->fsops_getfsattr(rootNode, LI_FSATTR_VOLUUID, [mutableResult mutableBytes], retlen, &retlen) == 0);
        volUUIDTry1 = [[[NSUUID alloc] initWithUUIDBytes:[mutableResult bytes]] UUIDString];
        NSLog(@"Got UUID on first try: %@", volUUIDTry1);
        XCTAssert(_fsops->fsops_getfsattr(rootNode, LI_FSATTR_VOLUUID, [mutableResult mutableBytes], retlen, &retlen) == 0);
        volUUIDTry2 = [[[NSUUID alloc] initWithUUIDBytes:[mutableResult bytes]] UUIDString];
        NSLog(@"Got UUID on second try: %@", volUUIDTry1);
        XCTAssert([volUUIDTry1 isEqualTo:volUUIDTry2]);
    }
    XCTAssert([self unmountTestVolume:rootNode] == 0);

    XCTAssert([self mountTestVolume:&rootNode] == 0);
    {
        LIFSAttributeValue val;
        NSMutableData *mutableResult = nil;
        size_t retlen;

        XCTAssert(_fsops->fsops_getfsattr(rootNode, LI_FSATTR_VOLUUID, &val, 0UL, &retlen) == E2BIG);
        mutableResult = [[NSMutableData alloc] initWithLength:retlen];
        XCTAssert(mutableResult != nil);
        XCTAssert(_fsops->fsops_getfsattr(rootNode, LI_FSATTR_VOLUUID, [mutableResult mutableBytes], retlen, &retlen) == 0);
        volUUIDTry3 = [[[NSUUID alloc] initWithUUIDBytes:[mutableResult bytes]] UUIDString];
        NSLog(@"Got UUID on third try: %@", volUUIDTry1);
        XCTAssert(_fsops->fsops_getfsattr(rootNode, LI_FSATTR_VOLUUID, [mutableResult mutableBytes], retlen, &retlen) == 0);
        volUUIDTry4 = [[[NSUUID alloc] initWithUUIDBytes:[mutableResult bytes]] UUIDString];
        NSLog(@"Got UUID on fourth try: %@", volUUIDTry1);
        XCTAssert([volUUIDTry3 isEqualTo:volUUIDTry4]);
        XCTAssert([volUUIDTry1 isEqualTo:volUUIDTry3]);
    }
    XCTAssert([self unmountTestVolume:rootNode] == 0);
}

- (void)testHasPermEnforcement
{
    size_t              retlen;
    UVFSFileNode        rootNode;
    LIFSAttributeValue  val;

    XCTAssert([self mountTestVolume:&rootNode] == 0);

    XCTAssert(_fsops->fsops_getfsattr(rootNode, LI_FSATTR_HAS_PERM_ENFORCEMENT, &val, sizeof(val), &retlen) == 0);
    XCTAssert(retlen == 1); // bool
    XCTAssert(val.fsa_bool == false); // // XXXab: Double-check with Taxera this is right

    XCTAssert([self unmountTestVolume:rootNode] == 0);
}

- (void)testHasAccessCheck
{
    size_t              retlen;
    UVFSFileNode        rootNode;
    LIFSAttributeValue  val;

    XCTAssert([self mountTestVolume:&rootNode] == 0);

    XCTAssert(_fsops->fsops_getfsattr(rootNode, LI_FSATTR_HAS_ACCESS_CHECK, &val, sizeof(val), &retlen) == 0);
    XCTAssert(retlen == 1); // bool
    XCTAssert(val.fsa_bool == false); // XXXab: Double-check with Taxera this is right

    XCTAssert([self unmountTestVolume:rootNode] == 0);
}

- (void)testCapsFormat
{
    size_t              retlen;
    UVFSFileNode        rootNode;
    LIFSAttributeValue  val;

    XCTAssert([self mountTestVolume:&rootNode] == 0);

    XCTAssert(_fsops->fsops_getfsattr(rootNode, LI_FSATTR_CAPS_FORMAT, &val, sizeof(val), &retlen) == 0);
    XCTAssert(retlen == 8); // number
    //XCTAssert(val.fsa_number == 0x0000000000002eca); // TODO: decode and verify it's what we want
    XCTAssert(val.fsa_number == 0x0000000000002fca);

    XCTAssert([self unmountTestVolume:rootNode] == 0);
}

- (void)testCapsInterfaces
{
    size_t              retlen;
    UVFSFileNode        rootNode;
    LIFSAttributeValue  val;

    XCTAssert([self mountTestVolume:&rootNode] == 0);

    XCTAssert(_fsops->fsops_getfsattr(rootNode, LI_FSATTR_CAPS_INTERFACES, &val, sizeof(val), &retlen) == 0);
    XCTAssert(retlen == 8); // number
    XCTAssert(val.fsa_number == 0x0000000000006382); // TODO: decode and verify it's what we want

    XCTAssert([self unmountTestVolume:rootNode] == 0);
}

/*
 #define LI_FSATTR_MOUNT_TIME                "_N_mount_time"             // number
 #define LI_FSATTR_LAST_MTIME                "_N_last_mtime"             // number
 #define LI_FSATTR_MOUNTFLAGS                "_N_mntflags"                // number
 */

- (void)testMountTime
{
    size_t              retlen;
    UVFSFileNode        rootNode;
    LIFSAttributeValue  val;

    XCTAssert([self mountTestVolume:&rootNode] == 0);

    XCTAssert(_fsops->fsops_getfsattr(rootNode, LI_FSATTR_MOUNT_TIME, &val, sizeof(val), &retlen) == 0);
    //XCTAssert(retlen == 8);
    XCTAssert(retlen == 0); //this is a bug!!!!
    //This value will reflect when this was mounted, just test it against time before mount?

    XCTAssert([self unmountTestVolume:rootNode] == 0);
}

- (void)testLastMountTime
{
    size_t              retlen;
    UVFSFileNode        rootNode;
    LIFSAttributeValue  val;

    XCTAssert([self mountTestVolume:&rootNode] == 0);

    // According to Tuxer no way to support this, since FS does not have this information
    XCTAssert(_fsops->fsops_getfsattr(rootNode, LI_FSATTR_LAST_MTIME, &val, sizeof(val), &retlen) == ENOTSUP);

    XCTAssert([self unmountTestVolume:rootNode] == 0);
}

- (void)testMountFlags
{
    size_t              retlen;
    UVFSFileNode        rootNode;
    LIFSAttributeValue  val;

    XCTAssert([self mountTestVolume:&rootNode] == 0);

    XCTAssert(_fsops->fsops_getfsattr(rootNode, LI_FSATTR_MOUNTFLAGS, &val, sizeof(val), &retlen) == 0);
    //XCTAssert(retlen == 8); //number
    XCTAssert(retlen == 0); //this is a bug!!!!
    XCTAssert((val.fsa_number & LI_MNT_RDONLY) == LI_MNT_RDONLY);

    XCTAssert([self unmountTestVolume:rootNode] == 0);
}

#pragma mark fsops_getattr() tests

- (void)testGetattrOnRoot
{
    UVFSFileAttributes attrs;
    UVFSFileNode        rootNode;

    XCTAssert([self mountTestVolume:&rootNode] == 0);

    memset(&attrs, 0, sizeof(UVFSFileAttributes));
    attrs.fa_validmask = UVFS_FA_SUPPORTED_VALIDMASK;
    XCTAssert(_fsops->fsops_getattr(rootNode, &attrs) == 0);
    XCTAssert(attrs.fa_validmask == 0x0000000000007fff); //TODO: decode and make sure that's expected
    XCTAssert(attrs.fa_type == UVFS_FA_TYPE_DIR);
    XCTAssert(attrs.fa_mode == 040755); //TODO: Follow up on this
    XCTAssert(attrs.fa_nlink == 1);
    XCTAssert(attrs.fa_uid == 99);
    XCTAssert(attrs.fa_gid == 99);
    XCTAssert(attrs.fa_bsd_flags == 0);
    XCTAssert(attrs.fa_size == 4096); //TODO: I guess one block? Follow up.
    XCTAssert(attrs.fa_allocsize == 0); //TODO: Seems like a bug for now
    XCTAssert(attrs.fa_fileid == 2); //It's root vnode
    XCTAssert(attrs.fa_parentid == 1); //TODO: I think this is wrong, I will need to check on other FSs
    XCTAssert(attrs.fa_atime.tv_sec >= 0);
    XCTAssert(attrs.fa_mtime.tv_sec >= 0);
    XCTAssert(attrs.fa_ctime.tv_sec >= 0);
    XCTAssert(attrs.fa_birthtime.tv_sec >= 0);
    //TODO: follow up on fa_backuptime, getting negative epoch
    //TODO: follow up on fa_addedtime, getting 0s

    XCTAssert([self unmountTestVolume:rootNode] == 0);
}

#pragma mark fsops_lookup() and fsops_reclaim() tests
- (void)testLookup
{
    UVFSFileAttributes  attrs;
    UVFSFileNode        rootNode;
    UVFSFileNode        tmpNode;

    XCTAssert([self mountTestVolume:&rootNode] == 0);

    XCTAssert(_fsops->fsops_lookup(rootNode, "a", &tmpNode) == ENOENT);
    XCTAssert(_fsops->fsops_lookup(rootNode, "f1", &tmpNode) == 0);

    // Now see if what we looked up can be used to retreive metadata and data
    XCTAssert(_fsops->fsops_getattr(tmpNode, &attrs) == 0);
    XCTAssert(attrs.fa_validmask == 0x0000000000007fff); //TODO: decode and make sure that's expected
    XCTAssert(attrs.fa_type == LI_FA_TYPE_FILE);
    XCTAssert(attrs.fa_mode == 0100755); //TODO: Follow up on this
    XCTAssert(attrs.fa_nlink == 2);
    XCTAssert(attrs.fa_uid == 99);
    XCTAssert(attrs.fa_gid == 99);
    XCTAssert(attrs.fa_bsd_flags == 0);
    XCTAssert(attrs.fa_size == 131072);
    XCTAssert(attrs.fa_allocsize == 131072);
    XCTAssert(attrs.fa_fileid == 64);
    XCTAssert(attrs.fa_parentid == 2); //File is located in root directory
    XCTAssert(attrs.fa_atime.tv_sec >= 0);
    XCTAssert(attrs.fa_mtime.tv_sec >= 0);
    XCTAssert(attrs.fa_ctime.tv_sec >= 0);
    XCTAssert(attrs.fa_birthtime.tv_sec >= 0);
    //TODO: follow up on fa_backuptime, getting negative epoch
    //TODO: follow up on fa_addedtime, getting 0s

    {
        char buf[512];
        size_t bytes_read = 0;
        XCTAssert(_fsops->fsops_read(tmpNode, 0, sizeof(buf), buf, &bytes_read) == 0);
        XCTAssert(bytes_read == 512);
    }

    XCTAssert(_fsops->fsops_reclaim(tmpNode) == 0);
    {
        char buf[512];
        size_t bytes_read = 0;
        LIFileAttributes_t linkattrs;
        // Verify that after reclaim node is no longer valid
        XCTAssert(_fsops->fsops_readlink(rootNode, 0, sizeof(buf), &bytes_read, &linkattrs) == EINVAL);
    }

    XCTAssert([self unmountTestVolume:rootNode] == 0);
}

#pragma mark fsops_read() tests
- (void)testRead
{
    UVFSFileNode        rootNode;
    UVFSFileNode        tmpNode;

    XCTAssert([self mountTestVolume:&rootNode] == 0);

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
        //Need to add verification of the buffer content
    }

    XCTAssert([self unmountTestVolume:rootNode] == 0);
}

//TODO: add test that does big file read

#pragma mark fsops_readdir tests
- (void)testReaddir
{
    UVFSFileNode        rootNode;

    XCTAssert([self mountTestVolume:&rootNode] == 0);

    {
        char buf[1024];
        size_t read_bytes = 0;
        uint64_t verifier = UVFS_DIRCOOKIE_VERIFIER_INITIAL;
        size_t run = 0;
        uint64_t lastnextcookie = 0;

        do {
            int err;
            size_t i;

            fprintf(stderr, "Run %zu:\n", ++run);
            XCTAssert((err = _fsops->fsops_readdir(rootNode, buf, sizeof(buf), lastnextcookie, &read_bytes, &verifier)) == 0);
            if (err) {
                break;
            }

            i = 0;
            while(i < sizeof(buf)) {
                LIDirEntry_t *const cur_entry = (LIDirEntry_t*) &buf[i];
                lastnextcookie = cur_entry->de_nextcookie;

                fprintf(stderr, "i=%04zu (->%04zu)"
                    "de_nextcookie=0x%016" PRIx64 " "
                    "de_nextrec=%5" PRIu16 " "
                    " name=\"%.*s\" \n",
                    i,
                    i + cur_entry->de_nextrec,
                    cur_entry->de_nextcookie,
                    cur_entry->de_nextrec,
                    cur_entry->de_namelen,
                    cur_entry->de_name);

                if (!cur_entry->de_nextrec) {
                    break;
                }

                i += cur_entry->de_nextrec;
            }
        } while (lastnextcookie != LI_DIRCOOKIE_EOF);

        // If we here, we enumerated directory fully, so do another call to validate that we will get EOF with EOF cookie
        XCTAssert(_fsops->fsops_readdir(rootNode, buf, sizeof(buf), lastnextcookie, &read_bytes, &verifier) == LI_READDIR_EOF_REACHED);
    }

    XCTAssert([self unmountTestVolume:rootNode] == 0);
}

//TODO: Add tests that iterate through the name space like FTS(3)

#pragma mark fsops_readdirattr tests
- (void)testReaddirAttr
{
    UVFSFileNode        rootNode;

    XCTAssert([self mountTestVolume:&rootNode] == 0);

    {
        char buf[1024];
        size_t read_bytes = 0;
        uint64_t verifier = UVFS_DIRCOOKIE_VERIFIER_INITIAL;
        size_t run = 0;
        uint64_t lastnextcookie = 0;

        do {
            int err;
            size_t i;

            fprintf(stderr, "Run %zu:\n", ++run);
            XCTAssert((err = _fsops->fsops_readdirattr(rootNode,
                    buf, sizeof(buf), lastnextcookie,
                    &read_bytes, &verifier)) == 0);
            if (err) {
                break;
            }

            i = 0;
            while(i < sizeof(buf)) {
                LIDirEntryAttr_t *const cur_entry = (LIDirEntryAttr_t*) &buf[i];
                LIFileAttributes_t *cur_entry_attrs = &cur_entry->dea_attrs;
                lastnextcookie = cur_entry->dea_nextcookie;

                fprintf(stderr, "i=%05zu (->%05zu) name=\"%.*s\" "
                    "dea_nextcookie=%03" PRIu64 " "
                    "dea_nextrec=%04" PRIu16 " "
                    "dea_nameoff=%04" PRIu16 "\n",
                    i,
                    i + cur_entry->dea_nextrec,
                    cur_entry->dea_namelen,
                    LI_DIRENTRYATTR_NAMEPTR(cur_entry),
                    cur_entry->dea_nextcookie,
                    cur_entry->dea_nextrec,
                    cur_entry->dea_nameoff);

                fprintf(stderr, "i=%05zu (->%05zu) fa_validmask=0x%08" PRIx64 " "
                    "fa_type=%02" PRIu32 " "
                    "fa_uid=%05" PRIu32 " "
                    "fa_gid=%05" PRIu32 "\n",
                    i,
                    i + cur_entry->dea_nextrec,
                    cur_entry_attrs->fa_validmask,
                    cur_entry_attrs->fa_type,
                    cur_entry_attrs->fa_uid,
                    cur_entry_attrs->fa_gid
                    );

                XCTAssert(cur_entry_attrs->fa_validmask == 0x7fff);
                XCTAssert(cur_entry_attrs->fa_uid == 99);
                XCTAssert(cur_entry_attrs->fa_gid == 99);

                if (!cur_entry->dea_nextrec) {
                    break;
                }

                XCTAssert(cur_entry->dea_nextrec == LI_DIRENTRYATTR_RECLEN(cur_entry, cur_entry->dea_namelen));

                i += cur_entry->dea_nextrec;
            }
        } while (lastnextcookie != LI_DIRCOOKIE_EOF);

        XCTAssert(_fsops->fsops_readdirattr(rootNode, buf, sizeof(buf), lastnextcookie, &read_bytes, &verifier) == LI_READDIR_EOF_REACHED);
    }

    XCTAssert([self unmountTestVolume:rootNode] == 0);
}


#pragma mark xattr tests
- (void)testXattr
{
    UVFSFileNode        rootNode;

    XCTAssert([self mountTestVolume:&rootNode] == 0);

    do {
        int err;
        UVFSFileNode xattrFileNode = NULL;
        size_t byte_count = 0;
        char buf[512];

        XCTAssert((err = _fsops->fsops_lookup(rootNode, "filewithxattr", &xattrFileNode)) == 0);
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
        fprintf(stderr, "Got xattr value: %s\n", buf);

        XCTAssert(_fsops->fsops_setxattr(xattrFileNode,
            "com.tuxera.test2", buf, sizeof(buf), UVFSXattrHowSet)
            == EROFS);

        /* Check that getting the size of the xattr list works. */
        byte_count = 0;
        XCTAssert(_fsops->fsops_listxattr(xattrFileNode, NULL, 0,
            &byte_count) == 0);

        fprintf(stderr, "byte_count: %zu\n", byte_count);
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
        fprintf(stderr, "Got xattr list: %s\n", buf);
        fprintf(stderr, "byte_count: %zu\n", byte_count);
        XCTAssert(_fsops->fsops_reclaim(xattrFileNode) == 0);
    } while(0);

    XCTAssert([self unmountTestVolume:rootNode] == 0);
}

- (void)testPerformanceExample {
    // This is an example of a performance test case.
    [self measureBlock:^{
        // Put the code you want to measure the time of here.
    }];
}

@end
