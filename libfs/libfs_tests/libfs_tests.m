/*
 * Copyright (c) 2022 Apple Computer, Inc.  All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

#import <XCTest/XCTest.h>

#import "FSPrivate.h"

#include <sys/mount.h>

@interface libfs_tests : XCTestCase

@end

@implementation libfs_tests

- (void)setUp {

    [super setUp];

    // Put setup code here. This method is called before the invocation of each test method in the class.
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.

    [super tearDown];
}

- (void)testTypeAndLocation_0 {

    /*
     * Test the basic "lifs" format for block device file systems.
     */

    struct statfs sfs = {
        .f_fstypename = "lifs",
        .f_fssubtype = 2,
        .f_mntfromname = "msdos://disk3s2/MYVOL",
    };

    char location[MNAMELEN];
    char typename[MFSTYPENAMELEN];
    uint32_t subtype;

    XCTAssert(_FSGetTypeInfoFromStatfs(&sfs, NULL, 0, NULL) == 0);
    XCTAssert(_FSGetTypeInfoFromStatfs(&sfs, typename, sizeof(typename), &subtype) == 0);
    XCTAssert(strcmp(typename, "msdos") == 0);
    XCTAssert(subtype == 2);

    XCTAssert(_FSGetLocationFromStatfs(&sfs, NULL, 0) == 0);
    XCTAssert(_FSGetLocationFromStatfs(&sfs, location, sizeof(location)) == 0);
    XCTAssert(strcmp(location, "disk3s2") == 0);
}

- (void)testTypeAndLocation_1 {

    /*
     * Test the basic FSKit format for block device file systems.
     */

    struct statfs sfs = {
        .f_fstypename = "fskit",
        .f_fssubtype = 2,
        .f_mntfromname = "msdos://disk3s2/MYVOL",
    };

    char location[MNAMELEN];
    char typename[MFSTYPENAMELEN];
    uint32_t subtype;

    XCTAssert(_FSGetTypeInfoFromStatfs(&sfs, typename, sizeof(typename), &subtype) == 0);
    XCTAssert(strcmp(typename, "msdos") == 0);
    XCTAssert(subtype == 2);

    XCTAssert(_FSGetLocationFromStatfs(&sfs, location, sizeof(location)) == 0);
    XCTAssert(strcmp(location, "disk3s2") == 0);
}

- (void)testTypeAndLocation_2 {

    /*
     * Test the basic KEXT format for block device file systems.
     */

    struct statfs sfs = {
        .f_fstypename = "hfs",
        .f_fssubtype = 0,
        .f_mntfromname = "/dev/disk5s1",
    };

    char location[MNAMELEN];
    char typename[MFSTYPENAMELEN];
    uint32_t subtype;

    XCTAssert(_FSGetTypeInfoFromStatfs(&sfs, typename, sizeof(typename), &subtype) == 0);
    XCTAssert(strcmp(typename, "hfs") == 0);
    XCTAssert(subtype == 0);

    XCTAssert(_FSGetLocationFromStatfs(&sfs, location, sizeof(location)) == 0);
    XCTAssert(strcmp(location, "disk5s1") == 0);
}

- (void)testTypeAndLocation_3 {

    /*
     * LiveFS / FSKit network file system.
     */

    struct statfs sfs = {
        .f_fstypename = "fskit",
        .f_mntfromname = "smb://user@server.com/SomeVolume",
    };

    char location[MNAMELEN];
    char typename[MFSTYPENAMELEN];
    uint32_t subtype;

    XCTAssert(_FSGetTypeInfoFromStatfs(&sfs, typename, sizeof(typename), &subtype) == 0);
    XCTAssert(strcmp(typename, "smb") == 0);
    XCTAssert(subtype == 0);

    XCTAssert(_FSGetLocationFromStatfs(&sfs, location, sizeof(location)) == 0);
    XCTAssert(strcmp(location, "user@server.com") == 0);
}

- (void)testTypeAndLocation_4 {

    /*
     * NFS!
     */

    struct statfs sfs = {
        .f_fstypename = "nfs",
        .f_mntfromname = "something.apple.com:/path/to/stuff",
    };

    char location[MNAMELEN];
    char typename[MFSTYPENAMELEN];
    uint32_t subtype;

    XCTAssert(_FSGetTypeInfoFromStatfs(&sfs, typename, sizeof(typename), &subtype) == 0);
    XCTAssert(strcmp(typename, "nfs") == 0);
    XCTAssert(subtype == 0);

    XCTAssert(_FSGetLocationFromStatfs(&sfs, location, sizeof(location)) == 0);
    XCTAssert(strcmp(location, "something.apple.com:/path/to/stuff") == 0);
}

- (void)testTypeAndLocation_5 {

    /*
     * Wacky case of "/" appearing in the "volume" position in statfs::f_mntfromname.
     */

    struct statfs sfs = {
        .f_fstypename = "fskit",
        .f_mntfromname = "apfs://disk4s2/my/Volume",
    };

    char location[MNAMELEN];
    char typename[MFSTYPENAMELEN];
    uint32_t subtype;

    XCTAssert(_FSGetTypeInfoFromStatfs(&sfs, typename, sizeof(typename), &subtype) == 0);
    XCTAssert(strcmp(typename, "apfs") == 0);
    XCTAssert(subtype == 0);

    XCTAssert(_FSGetLocationFromStatfs(&sfs, location, sizeof(location)) == 0);
    XCTAssert(strcmp(location, "disk4s2") == 0);
}

- (void)testTypeReusingBuffer {

    /*
     * Test using the statfs structure for the result storage.
     */

    struct statfs sfs = {
        .f_fstypename = "fskit",
        .f_fssubtype = 2,
        .f_mntfromname = "msdos://disk3s2/MYVOL",
    };

    XCTAssert(_FSGetTypeInfoFromStatfs(&sfs, sfs.f_fstypename,
                                       sizeof(sfs.f_fstypename),
                                       &sfs.f_fssubtype) == 0);
    XCTAssert(strcmp(sfs.f_fstypename, "msdos") == 0);
    XCTAssert(sfs.f_fssubtype == 2);
}

- (void)testLocationReusingBuffer {

    /*
     * Test using the statfs structure for the result storage.
     */

    struct statfs sfs = {
        .f_fstypename = "fskit",
        .f_fssubtype = 2,
        .f_mntfromname = "msdos://disk3s2/MYVOL",
    };

    XCTAssert(_FSGetLocationFromStatfs(&sfs, sfs.f_mntfromname,
                                       sizeof(sfs.f_mntfromname)) == 0);
    XCTAssert(strcmp(sfs.f_mntfromname, "disk3s2") == 0);
}

@end
