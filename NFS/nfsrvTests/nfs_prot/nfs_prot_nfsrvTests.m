/*
 * Copyright (c) 1999-2022 Apple Inc. All rights reserved.
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
/*
 * Copyright (c) 1992, 1993, 1994
 *    The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Rick Macklem at The University of Guelph.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    This product includes software developed by the University of
 *    California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/_types/_s_ifmt.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <oncrpc/auth.h>
#include <oncrpc/clnt.h>
#include <oncrpc/types.h>
#include <nfs/rpcv2.h>

#include "mountd.h"
#include "nfs_prot.h"
#include "pathnames.h"
#include "nfs_prot_rpc.h"

#import <XCTest/XCTest.h>

#define NFS_ACCESS_READ                 0x01
#define NFS_ACCESS_LOOKUP               0x02
#define NFS_ACCESS_MODIFY               0x04
#define NFS_ACCESS_EXTEND               0x08
#define NFS_ACCESS_DELETE               0x10
#define NFS_ACCESS_EXECUTE              0x20
#define NFS_ACCESS_ALL                  (NFS_ACCESS_READ | NFS_ACCESS_MODIFY | NFS_ACCESS_EXTEND | NFS_ACCESS_EXECUTE | NFS_ACCESS_DELETE | NFS_ACCESS_LOOKUP)

#define NFS_FABLKSIZE   512     /* Size in bytes of a block wrt fa_blocks */

static CLIENT *nclnt = NULL;
static nfs_fh3 rootfh = {};

CLIENT *
createClientForNFSProtocol(int socketFamily, int socketType, int authType, int flags, int *sockp)
{
    const char *host;

    switch (socketFamily) {
        case AF_INET:
            host = LOCALHOST4;
            break;
        case AF_INET6:
            host = LOCALHOST6;
            break;
        case AF_LOCAL:
            if (socketType != SOCK_STREAM) {
                XCTFail("AF_LOCAL supports only TCP sockets");
                return NULL;
            }
            host = _PATH_NFSD_TICOTSORD_SOCK;
            break;
        default:
            XCTFail("Unsupported family");
            return NULL;
    }

    return createClientForProtocol(host, socketFamily, socketType, authType, NFS_PROGRAM, NFS_V3, flags, sockp);
}

int
createFileInPath(const char *dir, char *file, int *dirFDp, int *fileFDp)
{
    int dirFD = -1, fileFD = -1;

    if (dir == NULL || file == NULL || dirFDp == NULL || fileFDp == NULL ) {
        XCTFail("Got NULL input");
        return -1;
    }

    dirFD = open(dir, O_DIRECTORY | O_SEARCH);
    if (dirFD < 0) {
        XCTFail("Unable to open dir (%s): %d", dir, errno);
        return -1;
    }

    fileFD = openat(dirFD, file, O_CREAT | O_RDWR, 0666);
    if (fileFD < 0) {
        close(dirFD);
        XCTFail("Unable to create file (%s:/%s): %d", dir, file, errno);
        return -1;
    }

    *dirFDp = dirFD;
    *fileFDp = fileFD;

    return 0;
}

static int
createDirInPath(const char *dir, char *subdir, int *dirFDp)
{
    int dirFD = -1, subdirFD = -1;

    if (dir == NULL || subdir == NULL || dirFDp == NULL ) {
        XCTFail("Got NULL input");
    }

    dirFD = open(dir, O_DIRECTORY | O_SEARCH);
    if (dirFD < 0) {
        XCTFail("Unable to open dir (%s): %d", dir, errno);
        return -1;
    }

    subdirFD = mkdirat(dirFD, subdir, 0666);
    if (subdirFD < 0) {
        close(dirFD);
        XCTFail("Unable to create dir (%s:/%s): %d", dir, subdir, errno);
        return -1;
    }

    *dirFDp = dirFD;

    return 0;
}

static int
createSymlinkInPath(const char *dir, char *file1, char *file2, int *dirFDp, int *linkFDp)
{
    int dirFD = -1, linkFD = -1;

    if (dir == NULL || file1 == NULL || file2 == NULL || dirFDp == NULL || linkFDp == NULL ) {
        XCTFail("Got NULL input");
        return -1;
    }

    dirFD = open(dir, O_DIRECTORY | O_SEARCH);
    if (dirFD < 0) {
        XCTFail("Unable to open dir (%s): %d", dir, errno);
        return -1;
    }

    linkFD = symlinkat(file1, dirFD, file2);
    if (linkFD < 0) {
        close(dirFD);
        XCTFail("Unable to create symlink (%s:/%s):%s %d", dir, file2, file1, errno);
        return -1;
    }

    *dirFDp = dirFD;
    *linkFDp = linkFD;

    return 0;
}

int
removeFromPath(char *file, int dirFD, int fileFD, int mode)
{
    int err = 0;

    if (fileFD >= 0) {
        close(fileFD);
    }

    if (file && dirFD >= 0) {
        err = unlinkat(dirFD, file, mode);
    }

    if (dirFD >= 0) {
        close(dirFD);
    }

    return err;
}

static int
checkAccess(nfs_fh3 *fh, int fileFD, int mode, int access, int expected)
{
    int err;
    ACCESS3res *res;

    err = fchmod(fileFD, mode);
    if (err != 0 ) {
        XCTFail("fchmod failed, got %d", err);
        return -1;
    }

    res = doAccessRPC(nclnt, fh, access);
    if (res->status != NFS3_OK) {
        XCTFail("checkAccess failed, got %d", res->status);
        return -1;
    }

    if (expected != res->ACCESS3res_u.resok.access) {
        XCTFail("checkAccess failed, expected access is 0x%x, sent 0x%x, got 0x%x", expected, access, res->ACCESS3res_u.resok.access);
    }

    return 0;
}

static void
doRead(char *data, size_t len, offset3 offset, size_t count)
{
    LOOKUP3res *res;
    READ3res *res2;
    ssize_t bytes;
    int dirFD, fileFD, err;
    char *file = "new_file";

    err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
    if (err) {
        XCTFail("createFileInPath failed, got %d", err);
        return;
    }

    if (len) {
        bytes = write(fileFD, data, len);
        if (bytes <= 0) {
            XCTFail("write failed, got %zu, errno %d", bytes, errno);
        }
    }

    res = doLookupRPC(nclnt, &rootfh, file);
    if (res->status != NFS3_OK) {
        XCTFail("doLookupRPC failed, got %d", res->status);
    }

    res2 = doReadRPC(nclnt, &res->LOOKUP3res_u.resok.object, offset, (count3)count);
    if (res2->status != NFS3_OK) {
        XCTFail("doReadRPC failed, got %d", res2->status);
    }

    XCTAssertEqual(res2->READ3res_u.resok.eof, offset + count >= len ? 1 : 0);
    if (offset < len && count) {
        XCTAssertNotEqual(res2->READ3res_u.resok.count, 0);
        XCTAssertEqual(memcmp(res2->READ3res_u.resok.data.data_val, data + offset, res2->READ3res_u.resok.count), 0);
    } else if (count) {
        XCTAssertEqual(res2->READ3res_u.resok.count, 0);
    }
    removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

static void
doNFSSetUpWithArgs(const char **nfsdArgs, int nfsdArgsSize, int socketFamily, int socketType, int authType)
{
    int err;
    fhandle_t *fh;
    doMountSetUpWithArgs(nfsdArgs, nfsdArgsSize, NULL, 0);

    err = createClientForMountProtocol(socketFamily, socketType, authType, 0);
    if (err) {
        XCTFail("Cannot create client mount: %d", err);
    }

    nclnt = createClientForNFSProtocol(socketFamily, socketType, authType, 0, NULL);
    if (nclnt == NULL) {
        XCTFail("Cannot create client for NFS");
    }

    memset(&rootfh, 0, sizeof(rootfh));
    if ((fh = doMountAndVerify(getLocalMountedPath())) == NULL) {
        XCTFail("doMountAndVerify failed");
    }

    rootfh.data.data_len = fh->fh_len;
    rootfh.data.data_val = (char *)fh->fh_data;
}

static void
doNFSSetUpVerbose(int socketFamily, int socketType, int authType)
{
    const char *argv[1] = { "-vvvvv" };
    doNFSSetUpWithArgs(argv, ARRAY_SIZE(argv), socketFamily, socketType, authType);
}

@interface nfsrvTests_nfs : XCTestCase

@end

@implementation nfsrvTests_nfs

- (void)setUp
{
    doNFSSetUpWithArgs(NULL, 0, AF_INET, SOCK_STREAM, RPCAUTH_UNIX);
}

- (void)tearDown
{
    memset(&rootfh, 0, sizeof(rootfh));
    doMountTearDown();

    if (nclnt) {
        clnt_destroy(nclnt);
        nclnt = NULL;
    }
}

/*
 * 1. Send NULL to the server, make sure we got the reply
 */
- (void)testNFSNull
{
    doNullRPC(nclnt);
}

/*
 * 1. Send GETATTR using STALE file handle, ESTALE is expected
 */
- (void)testNFSGetAttrESTALE
{
    GETATTR3res *res;
    char buffer[8] = { 0, 1, 2, 3, 4, 5, 6, 7};
    nfs_fh3 fh = { .data.data_len = 8, .data.data_val = buffer };

    res = doGetattrRPC(nclnt, &fh);
    if (res->status != NFS3ERR_STALE) {
        XCTFail("doGetattrRPC failed, expected status is %d, got %d", NFS3ERR_STALE, res->status);
    }
}

/*
 * 1. Create new file
 * 2. Obtain the filehandle using LOOKUP
 * 3. Retrieve attributes using GETATTR
 * 4. Remove the file
 * 5. Send GETATTR using the STALE file handle, ESTALE is expected
 */
 - (void)testNFSGetAttrESTALE2
{
    LOOKUP3res *res;
    GETATTR3res *res2;
    int dirFD, fileFD, err;
    char *file = "new_file";

    err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
    if (err) {
        XCTFail("createFileInPath failed, got %d", err);
        return;
    }

    res = doLookupRPC(nclnt, &rootfh, file);
    if (res->status != NFS3_OK) {
        XCTFail("doLookupRPC failed, got %d", res->status);
    }

    res2 = doGetattrRPC(nclnt, &res->LOOKUP3res_u.resok.object);
    if (res2->status != NFS3_OK) {
        XCTFail("doGetattrRPC failed, got %d", res2->status);
    }

    removeFromPath(file, dirFD, fileFD, REMOVE_FILE);

    res2 = doGetattrRPC(nclnt, &res->LOOKUP3res_u.resok.object);
    if (res2->status != NFS3ERR_STALE) {
        XCTFail("doGetattrRPC failed, expected status is %d, got %d", NFS3ERR_STALE, res2->status);
    }
}

/*
 * 1. Create new file
 * 2. write() to modify its size
 * 3. Call stat() to get file status
 * 3. Obtain the filehandle using LOOKUP
 * 4. Retrieve attributes using GETATTR
 * 5. Compare attributes received by stat() and GETATTR
 */
- (void)testNFSGetAttr
{
    fattr3 *attr;
    LOOKUP3res *res;
    GETATTR3res *res2;
    ssize_t bytes;
    int dirFD, fileFD, err;
    char *file = "new_file";
    struct stat stat = {};

    err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
    if (err) {
        XCTFail("createFileInPath failed, got %d", err);
        return;
    }

    bytes = write(fileFD, file, strlen(file));
    if (bytes <= 0) {
        XCTFail("write failed, got %zu, errno %d", bytes, errno);
    }

    err = fstat(fileFD, &stat);
    if (err) {
        XCTFail("fstat failed, got %d", err);
    }

    res = doLookupRPC(nclnt, &rootfh, file);
    if (res->status != NFS3_OK) {
        XCTFail("doLookupRPC failed, got %d", res->status);
    }

    res2 = doGetattrRPC(nclnt, &res->LOOKUP3res_u.resok.object);
    if (res2->status != NFS3_OK) {
        XCTFail("doGetattrRPC failed, got %d", res2->status);
    }
    attr = &res2->GETATTR3res_u.resok.obj_attributes;

    // Verify results
    XCTAssertEqual(attr->type, NFREG);
    XCTAssertEqual(attr->mode, stat.st_mode & 07777);
    XCTAssertEqual(attr->nlink, stat.st_nlink);
    XCTAssertEqual(attr->uid, stat.st_uid);
    XCTAssertEqual(attr->gid, stat.st_gid);
    XCTAssertEqual(attr->size, stat.st_size);
    // attr->used
    XCTAssertEqual(attr->rdev.specdata1, major(stat.st_rdev));
    XCTAssertEqual(attr->rdev.specdata2, minor(stat.st_rdev));
    XCTAssertEqual(attr->fsid, stat.st_dev);
    XCTAssertEqual(attr->fileid, stat.st_ino);
    XCTAssertEqual(attr->atime.seconds, stat.st_atimespec.tv_sec);
    XCTAssertEqual(attr->atime.nseconds, stat.st_atimespec.tv_nsec);
    XCTAssertEqual(attr->mtime.seconds, stat.st_mtimespec.tv_sec);
    XCTAssertEqual(attr->mtime.nseconds, stat.st_mtimespec.tv_nsec);
    XCTAssertEqual(attr->ctime.seconds, stat.st_ctimespec.tv_sec);
    XCTAssertEqual(attr->ctime.nseconds, stat.st_ctimespec.tv_nsec);

    removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Send SETATTR using STALE file handle, ESTALE is expected
 */
- (void)testNFSSetattrESTALE
{
    sattr3 attr = {};
    SETATTR3res *res;
    char buffer[8] = { 0, 1, 2, 3, 4, 5, 6, 7};
    nfs_fh3 fh = { .data.data_len = 8, .data.data_val = buffer };

    attr.mode.set_it = TRUE;
    attr.mode.set_mode3_u.mode = 777;

    res = doSetattrRPC(nclnt, &fh, &attr, NULL);
    if (res->status != NFS3ERR_STALE) {
        XCTFail("doSetattrRPC failed, expected status is %d, got %d", NFS3ERR_STALE, res->status);
    }
}

/*
 * 1. Create new file
 * 2. Obtain the filehandle using LOOKUP
 * 3. Call SETATTR to modify file mode using invalid "guard", NFS3ERR_NOT_SYNC is expected
 */
- (void)testNFSSetattrNOSYNC
{
    sattr3 attr = {};
    LOOKUP3res *res;
    SETATTR3res *res2;
    int dirFD, fileFD, err;
    char *file = "new_file";
    struct timespec guard = { .tv_sec = 1, .tv_nsec = 2 };

    err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
    if (err) {
        XCTFail("createFileInPath failed, got %d", err);
        return;
    }

    res = doLookupRPC(nclnt, &rootfh, file);
    if (res->status != NFS3_OK) {
        XCTFail("doLookupRPC failed, got %d", res->status);
    }

    attr.mode.set_it = TRUE;
    attr.mode.set_mode3_u.mode = 777;

    res2 = doSetattrRPC(nclnt, &res->LOOKUP3res_u.resok.object, &attr, &guard);
    if (res2->status != NFS3ERR_NOT_SYNC) {
        XCTFail("doSetattrRPC failed, expected status is %d, got %d", NFS3ERR_NOT_SYNC, res2->status);
    }

    removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Create new file
 * 2. Obtain the filehandle using LOOKUP
 * 3. Retrieve attributes using GETATTR
 * 4. Call SETATTR to modify file mtime using valid "guard" retrieved from GETATTR
 * 5. Retrieve attributes using GETATTR
 * 6. Verify mtime was updated
 */
- (void)testNFSSetattrGuardClientTime
{
    sattr3 attr = {};
    LOOKUP3res *res;
    GETATTR3res *res2, *res4;
    SETATTR3res *res3;
    int dirFD, fileFD, err;
    char *file = "new_file";
    struct timespec guard = {};

    err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
    if (err) {
        XCTFail("createFileInPath failed, got %d", err);
        return;
    }

    res = doLookupRPC(nclnt, &rootfh, file);
    if (res->status != NFS3_OK) {
        XCTFail("doLookupRPC failed, got %d", res->status);
    }

    res2 = doGetattrRPC(nclnt, &res->LOOKUP3res_u.resok.object);
    if (res2->status != NFS3_OK) {
        XCTFail("doGetattrRPC failed, got %d", res2->status);
    }

    guard.tv_sec = res2->GETATTR3res_u.resok.obj_attributes.mtime.seconds;
    guard.tv_nsec = res2->GETATTR3res_u.resok.obj_attributes.mtime.nseconds;

    attr.mtime.set_it = SET_TO_CLIENT_TIME;
    attr.mtime.set_mtime_u.mtime.seconds = 99;
    attr.mtime.set_mtime_u.mtime.nseconds = 66;

    res3 = doSetattrRPC(nclnt, &res->LOOKUP3res_u.resok.object, &attr, &guard);
    if (res3->status != NFS3_OK) {
        XCTFail("doSetattrRPC failed, got %d", res3->status);
    }

    res4 = doGetattrRPC(nclnt, &res->LOOKUP3res_u.resok.object);
    if (res4->status != NFS3_OK) {
        XCTFail("doGetattrRPC failed, got %d", res4->status);
    }

    XCTAssertEqual(res4->GETATTR3res_u.resok.obj_attributes.mtime.seconds, attr.mtime.set_mtime_u.mtime.seconds);
    XCTAssertEqual(res4->GETATTR3res_u.resok.obj_attributes.mtime.nseconds, attr.mtime.set_mtime_u.mtime.nseconds);

    removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Send LOOKUP for non existing entry. NFS3ERR_NOENT is expected
 */
- (void)testNFSLookupNoEntry
{
    LOOKUP3res *res = doLookupRPC(nclnt, &rootfh, "INVALID_ITEM");
    if (res->status != NFS3ERR_NOENT) {
        XCTFail("doLookupRPC failed, expected status is %d, got %d", NFS3ERR_NOENT, res->status);
    }
}

/*
 * 1. Create new file
 * 2. Obtain the filehandle using LOOKUP
 * 3. Remove the file
 * 4. Send LOOKUP using the same name, NFS3ERR_NOENT is expected
 */
- (void)testNFSLookupNoEntry2
{
    LOOKUP3res *res;
    int dirFD, fileFD, err;
    char *file = "new_file";

    err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
    if (err) {
        XCTFail("createFileInPath failed, got %d", err);
        return;
    }

    res = doLookupRPC(nclnt, &rootfh, file);
    if (res->status != NFS3_OK) {
        XCTFail("doLookupRPC failed, got %d", res->status);
    }

    removeFromPath(file, dirFD, fileFD, REMOVE_FILE);

    res = doLookupRPC(nclnt, &rootfh, file);
    if (res->status != NFS3ERR_NOENT) {
        XCTFail("doLookupRPC failed, expected status is %d, got %d", NFS3ERR_NOENT, res->status);
    }
}

/*
 * 1. Create new file
 * 2. Obtain the filehandle using LOOKUP
 */
- (void)testNFSLookup
{
    LOOKUP3res *res;
    int dirFD, fileFD, err;
    char *file = "new_file";

    err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
    if (err) {
        XCTFail("createFileInPath failed, got %d", err);
        return;
    }

    res = doLookupRPC(nclnt, &rootfh, file);
    if (res->status != NFS3_OK) {
        XCTFail("doLookupRPC failed, got %d", res->status);
    }

    removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Create new file
 * 2. Obtain the filehandle using LOOKUP
 * 3. chmod to 400, verify NFS_ACCESS_READ access is allowed
 * 4. chmod to 200, verify NFS_ACCESS_MODIFY access is allowed
 * 5. chmod to 100, verify NFS_ACCESS_EXECUTE access is allowed
 * 6. chmod to 300, verify NFS_ACCESS_READ access is NOT allowed
 * 7. chmod to 500, verify NFS_ACCESS_MODIFY access is NOT allowed
 * 8. chmod to 600, verify NFS_ACCESS_EXECUTE access is NOT allowed
 */
- (void)testNFSAccess
{
    LOOKUP3res *res;
    int dirFD, fileFD, err;
    char *file = "new_file";

    err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
    if (err) {
        XCTFail("createFileInPath failed, got %d", err);
        return;
    }

    res = doLookupRPC(nclnt, &rootfh, file);
    if (res->status != NFS3_OK) {
        XCTFail("doLookupRPC failed, got %d", res->status);
    }

    checkAccess(&res->LOOKUP3res_u.resok.object, fileFD, 400, NFS_ACCESS_READ, NFS_ACCESS_READ);
    checkAccess(&res->LOOKUP3res_u.resok.object, fileFD, 200, NFS_ACCESS_MODIFY, NFS_ACCESS_MODIFY);
    checkAccess(&res->LOOKUP3res_u.resok.object, fileFD, 100, NFS_ACCESS_EXECUTE, NFS_ACCESS_EXECUTE);
    checkAccess(&res->LOOKUP3res_u.resok.object, fileFD, 300, NFS_ACCESS_READ, 0);
    checkAccess(&res->LOOKUP3res_u.resok.object, fileFD, 500, NFS_ACCESS_MODIFY, 0);
    checkAccess(&res->LOOKUP3res_u.resok.object, fileFD, 600, NFS_ACCESS_EXECUTE, 0);

    removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Send READLINK using STALE file handle, ESTALE is expected
 */
- (void)testNFSReadlinkESTALE
{
    READLINK3res *res;
    char buffer[8] = { 0, 1, 2, 3, 4, 5, 6, 7};
    nfs_fh3 fh = { .data.data_len = 8, .data.data_val = buffer };

    res = doReadlinkRPC(nclnt, &fh);
    if (res->status != NFS3ERR_STALE) {
        XCTFail("doReadlinkRPC failed, expected status is %d, got %d", NFS3ERR_STALE, res->status);
    }
}

/*
 * 1. Create new file
 * 2. Obtain the filehandle using LOOKUP
 * 3. Send READLINK using filehandle of regular file. NFS3ERR_INVAL is expected
 */
- (void)testNFSReadlinkNF3REG
{
    LOOKUP3res *res;
    READLINK3res *res2;
    int dirFD, fileFD, err;
    char *file = "new_file";

    err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
    if (err) {
        XCTFail("createFileInPath failed, got %d", err);
        return;
    }

    res = doLookupRPC(nclnt, &rootfh, file);
    if (res->status != NFS3_OK) {
        XCTFail("doLookupRPC failed, got %d", res->status);
    }

    res2 = doReadlinkRPC(nclnt, &res->LOOKUP3res_u.resok.object);
    if (res2->status != NFS3ERR_INVAL) {
        XCTFail("doReadlinkRPC failed, expected status is %d, got %d", NFS3ERR_INVAL, res2->status);
    }

    removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Create new file
 * 2. Create new symlink of the file
 * 3. Call fstatat() to get symlink status
 * 4. Obtain symlink the filehandle using LOOKUP
 * 5. Send READLINK using filehandle of link
 * 6. Verify fileid and link path
 */
- (void)testNFSReadlink
{
    LOOKUP3res *res;
    READLINK3res *res2;
    struct stat stat = {};
    int dirFD, dirFD2, fileFD, linkFD, err;
    char *file = "new_file";
    char *link = "new_link";

    err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
    if (err) {
        XCTFail("createFileInPath failed, got %d", err);
        return;
    }

    err = createSymlinkInPath(getLocalMountedPath(), file, link, &dirFD2, &linkFD);
    if (err) {
        XCTFail("createSymlinkInPath failed, got %d", err);
    }

    err = fstatat(dirFD2, link, &stat, AT_SYMLINK_NOFOLLOW);
    if (err) {
        XCTFail("fstatat failed, got %d", err);
    }

    res = doLookupRPC(nclnt, &rootfh, link);
    if (res->status != NFS3_OK) {
        XCTFail("doLookupRPC failed, got %d", res->status);
    }

    res2 = doReadlinkRPC(nclnt, &res->LOOKUP3res_u.resok.object);
    if (res2->status != NFS3_OK) {
        XCTFail("doReadlinkRPC failed, got %d", res2->status);
    }

    XCTAssertEqual(strncmp(file, res2->READLINK3res_u.resok.data, strlen(file)), 0);
    if (res2->READLINK3res_u.resok.symlink_attributes.attributes_follow) {
        XCTAssertEqual(stat.st_ino, res2->READLINK3res_u.resok.symlink_attributes.post_op_attr_u.attributes.fileid);
    }

    removeFromPath(link, dirFD2, linkFD, REMOVE_FILE);
    removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Send READ using STALE file handle, ESTALE is expected
 */
- (void)testNFSReadESTALE
{
    READ3res *res;
    offset3 offset = 0;
    count3 count = 0;
    char buffer[8] = { 0, 1, 2, 3, 4, 5, 6, 7};
    nfs_fh3 fh = { .data.data_len = 8, .data.data_val = buffer };

    res = doReadRPC(nclnt, &fh, offset, count);
    if (res->status != NFS3ERR_STALE) {
        XCTFail("doReadRPC failed, expected status is %d, got %d", NFS3ERR_STALE, res->status);
    }
}

/*
 * 1. Create new file
 * 2. Write to the file
 * 3. Obtain the filehandle using LOOKUP
 * 4. Call READ from offset zero, the same amount of bytes we wrote
 * 5. Compare results, verify EOF is TRUE
 */
- (void)testNFSReadAll
{
    char *string = "1234567890";
    doRead(string, strlen(string), 0, strlen(string));
}

/*
 * 1. Create new file
 * 2. Write to the file
 * 3. Obtain the filehandle using LOOKUP
 * 4. Call READ from offset 3, the same amount of bytes we wrote
 * 5. Compare results, verify EOF is TRUE
 */
- (void)testNFSReadOffset
{
    char *string = "1234567890";
    doRead(string, strlen(string), 3, strlen(string));
}

/*
 * 1. Create new file
 * 2. Write to the file
 * 3. Obtain the filehandle using LOOKUP
 * 4. Call READ from offset zero, zero bytes
 */
- (void)testNFSReadZeroBytes
{
    char *string = "1234567890";
    doRead(string, strlen(string), 0, 0);
}

/*
 * 1. Create new file
 * 2. Obtain the filehandle using LOOKUP
 * 3. Call READ from offset zero, zero bytes
 * 4. Verify EOF is TRUE
 */
- (void)testNFSReadEmptyFileZeroBytes
{
    char *string = "";
    doRead(string, 0, 0, 0);
}

/*
 * 1. Create new file
 * 2. Write to the file
 * 3. Obtain the filehandle using LOOKUP
 * 4. Call READ from offset 100, 10 bytes
 * 5. Verify EOF is TRUE, verify no bytes were read
 */
- (void)testNFSReadInvalidOffset
{
    char *string = "1234567890";
    doRead(string, strlen(string), 100, 10);
}

/*
 * 1. MOUNT Read-Only file system path
 * 2. Obtain the root file handle
 * 2. Create new file
 * 3. Obtain the filehandle using LOOKUP
 * 4. Send WRITE to the file. NFS3ERR_ROFS is expected
 */
- (void)testNFSWriteROFS
{
    int dirFD, fileFD, err;
    fhandle_t *fh;
    nfs_fh3 rofh = {};
    char *file = "new_file";
    LOOKUP3res *res;
    WRITE3res *res2;
    char buf[10] = "1234567890";

    // Mount read only path
    memset(&rofh, 0, sizeof(rofh));
    if ((fh = doMountAndVerify(getLocalMountedReadOnlyPath())) == NULL) {
        XCTFail("doMountAndVerify failed");
    }

    rofh.data.data_len = fh->fh_len;
    rofh.data.data_val = (char *)fh->fh_data;

    // Create local file
    err = createFileInPath(getLocalMountedReadOnlyPath(), file, &dirFD, &fileFD);
    if (err) {
        XCTFail("createFileInPath failed, got %d", err);
        return;
    }

    // send LOOKUP rpc to obtain file handle
    res = doLookupRPC(nclnt, &rofh, file);
    if (res->status != NFS3_OK) {
        XCTFail("doLookupRPC failed, got %d", res->status);
    }

    // Write 5 bytes to the file
    res2 = doWriteRPC(nclnt, &res->LOOKUP3res_u.resok.object, 0, 5, UNSTABLE, sizeof(buf), buf);
    if (res2->status != NFS3ERR_ROFS) {
        XCTFail("doWriteRPC failed, expected status is %d, got %d", NFS3ERR_ROFS, res2->status);
    }

    // Remove the created file
    removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Create new file
 * 2. Obtain the filehandle using LOOKUP
 * 3. Send unstable WRITE request of zero bytes
 * 4. Verify count
 * 5. Retrieve attributes using GETATTR
 * 6. Verify mtime was not updated
 */
- (void)testNFSWriteZero
{
    LOOKUP3res *res;
    WRITE3res *res2;
    GETATTR3res *res3;
    int dirFD, fileFD, err, count = 0, offset = 0;
    char *file = "new_file";
    char buf[10] = "1234567890";

    err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
    if (err) {
        XCTFail("createFileInPath failed, got %d", err);
        return;
    }

    res = doLookupRPC(nclnt, &rootfh, file);
    if (res->status != NFS3_OK) {
        XCTFail("doLookupRPC failed, got %d", res->status);
    }

    res2 = doWriteRPC(nclnt, &res->LOOKUP3res_u.resok.object, offset, count, UNSTABLE, sizeof(buf), buf);
    if (res2->status != NFS3_OK) {
        XCTFail("doWriteRPC failed, got %d", res2->status);
    }

    if (res2->WRITE3res_u.resok.count != count) {
        XCTFail("doWriteRPC count should be %d, got %d", count, res2->WRITE3res_u.resok.count);
    }

    if (res->LOOKUP3res_u.resok.obj_attributes.attributes_follow) {
        res3 = doGetattrRPC(nclnt, &res->LOOKUP3res_u.resok.object);
        if (res3->status != NFS3_OK) {
            XCTFail("doGetattrRPC failed, got %d", res3->status);
        }

        // mtime should not be modified while writing zero bytes
        if (memcmp(&res3->GETATTR3res_u.resok.obj_attributes.mtime,
                   &res->LOOKUP3res_u.resok.obj_attributes.post_op_attr_u.attributes.mtime,
                   sizeof(nfstime3)) != 0) {
            XCTFail("mtime should be equal");
        }
    }

    removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Create new file
 * 2. Obtain the filehandle using LOOKUP
 * 3. Send data sync WRITE request of single byte
 * 4. Verify count
 * 5. Retrieve attributes using GETATTR
 * 6. Verify mtime was updated
 */
- (void)testNFSWriteOne
{
    LOOKUP3res *res;
    WRITE3res *res2;
    GETATTR3res *res3;
    int dirFD, fileFD, err, count = 1, offset = 0;
    char *file = "new_file";
    char buf[10] = "1234567890";

    err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
    if (err) {
        XCTFail("createFileInPath failed, got %d", err);
        return;
    }

    res = doLookupRPC(nclnt, &rootfh, file);
    if (res->status != NFS3_OK) {
        XCTFail("doLookupRPC failed, got %d", res->status);
    }

    res2 = doWriteRPC(nclnt, &res->LOOKUP3res_u.resok.object, offset, count, DATA_SYNC, sizeof(buf), buf);
    if (res2->status != NFS3_OK) {
        XCTFail("doWriteRPC failed, got %d", res2->status);
    }

    if (res2->WRITE3res_u.resok.count != count) {
        XCTFail("doWriteRPC count should be %d, got %d", count, res2->WRITE3res_u.resok.count);
    }

    if (res->LOOKUP3res_u.resok.obj_attributes.attributes_follow) {
        res3 = doGetattrRPC(nclnt, &res->LOOKUP3res_u.resok.object);
        if (res3->status != NFS3_OK) {
            XCTFail("doGetattrRPC failed, got %d", res3->status);
        }

        if (memcmp(&res3->GETATTR3res_u.resok.obj_attributes.mtime,
                   &res->LOOKUP3res_u.resok.obj_attributes.post_op_attr_u.attributes.mtime,
                   sizeof(nfstime3)) == 0) {
            XCTFail("mtime should not be equal");
        }
    }

    removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Create new file
 * 2. Obtain the filehandle using LOOKUP
 * 3. Send unstable WRITE request using an invalid offset. NFS3ERR_INVAL is expected
 */
- (void)testNFSWriteInvalidOffset
{
    LOOKUP3res *res;
    WRITE3res *res2;
    int dirFD, fileFD, err, count = 1, offset = -1;
    char *file = "new_file";
    char buf[10] = "1234567890";

    err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
    if (err) {
        XCTFail("createFileInPath failed, got %d", err);
        return;
    }

    res = doLookupRPC(nclnt, &rootfh, file);
    if (res->status != NFS3_OK) {
        XCTFail("doLookupRPC failed, got %d", res->status);
    }

    res2 = doWriteRPC(nclnt, &res->LOOKUP3res_u.resok.object, offset, count, DATA_SYNC, sizeof(buf), buf);
    if (res2->status != NFS3ERR_INVAL) {
        XCTFail("doWriteRPC should failed, got %d", res2->status);
    }

    removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Create new directory
 * 2. Obtain the filehandle using LOOKUP
 * 3. Send unstable WRITE request using the directory file handle. NFS3ERR_INVAL is expected
 */
- (void)testNFSWriteInvalidType
{
    LOOKUP3res *res;
    WRITE3res *res2;
    int dirFD, err, count = 1, offset = 0;
    char *dir = "new_dir";
    char buf[10] = "1234567890";

    err = createDirInPath(getLocalMountedPath(), dir, &dirFD);
    if (err) {
        XCTFail("createDirInPath failed, got %d", err);
        return;
    }

    res = doLookupRPC(nclnt, &rootfh, dir);
    if (res->status != NFS3_OK) {
        XCTFail("doLookupRPC failed, got %d", res->status);
    }

    res2 = doWriteRPC(nclnt, &res->LOOKUP3res_u.resok.object, offset, count, FILE_SYNC, sizeof(buf), buf);
    if (res2->status != NFS3ERR_INVAL) {
        XCTFail("doWriteRPC failed, expected status is %d, got %d", NFS3ERR_INVAL, res2->status);
    }

    removeFromPath(dir, dirFD, -1, REMOVE_DIR);
}

/*
 * 1. Send unchecked CREATE request using uid and gid
 * 2. Verify gid/uid attributes returned by CREATE
 * 3. call stat() - verify gid/uid/inode attributes returned by CREATE
 */
- (void)testNFSCreate
{
    CREATE3res *res;
    char *file = "new_file";
    struct createhow3 how = {};
    uid_t uid;
    gid_t gid;
    int err;
    struct stat stats = {};
    fattr3 *after;
    char path[PATH_MAX];

    how.mode = UNCHECKED;
    how.createhow3_u.obj_attributes.uid.set_it = TRUE;
    how.createhow3_u.obj_attributes.uid.set_uid3_u.uid = uid = 1;
    how.createhow3_u.obj_attributes.gid.set_it = TRUE;
    how.createhow3_u.obj_attributes.gid.set_gid3_u.gid = gid = 2;

    res = doCreateRPC(nclnt, &rootfh, file, &how);
    if (res->status != NFS3_OK) {
        XCTFail("doCreateRPC failed, got %d", res->status);
    }
    after = &res->CREATE3res_u.resok.obj_attributes.post_op_attr_u.attributes;

    XCTAssertEqual(uid, after->uid);
    XCTAssertEqual(gid, after->gid);

    snprintf(path, sizeof(path), "%s/%s", getLocalMountedPath(), file);
    err = stat(path, &stats);
    if (err) {
        XCTFail("stat failed, got %d", err);
    }

    XCTAssertEqual(stats.st_uid, after->uid);
    XCTAssertEqual(stats.st_gid, after->gid);
    XCTAssertEqual(stats.st_ino, after->fileid);

    err = unlink(path);
    if (err) {
        XCTFail("unlink failed, got %d", err);
    }
}

/*
 * 1. Send unchecked CREATE request using uid and gid
 * 2. Verify gid/uid attributes returned by CREATE
 * 3. Send unchecked CREATE request again using uid and gid
 */
- (void)testNFSCreateUnchecked
{
    CREATE3res *res;
    char *file = "new_file";
    struct createhow3 how = {};
    uid_t uid;
    gid_t gid;
    int err;
    fattr3 *after;
    char path[PATH_MAX];

    how.mode = UNCHECKED;
    how.createhow3_u.obj_attributes.uid.set_it = TRUE;
    how.createhow3_u.obj_attributes.uid.set_uid3_u.uid = uid = 1;
    how.createhow3_u.obj_attributes.gid.set_it = TRUE;
    how.createhow3_u.obj_attributes.gid.set_gid3_u.gid = gid = 2;

    res = doCreateRPC(nclnt, &rootfh, file, &how);
    if (res->status != NFS3_OK) {
        XCTFail("doCreateRPC failed, got %d", res->status);
    }
    after = &res->CREATE3res_u.resok.obj_attributes.post_op_attr_u.attributes;

    XCTAssertEqual(uid, after->uid);
    XCTAssertEqual(gid, after->gid);

    how.createhow3_u.obj_attributes.uid.set_uid3_u.uid = uid = 3;
    how.createhow3_u.obj_attributes.gid.set_gid3_u.gid = gid = 4;

    res = doCreateRPC(nclnt, &rootfh, file, &how);
    if (res->status != NFS3_OK) {
        XCTFail("doCreateRPC failed, got %d", res->status);
    }
    after = &res->CREATE3res_u.resok.obj_attributes.post_op_attr_u.attributes;

    XCTAssertEqual(uid, after->uid);
    XCTAssertEqual(gid, after->gid);

    snprintf(path, sizeof(path), "%s/%s", getLocalMountedPath(), file);
    err = unlink(path);
    if (err) {
        XCTFail("unlink failed, got %d", err);
    }
}

/*
 * 1. Send guarded CREATE request using uid and gid
 * 2. Verify gid/uid attributes returned by CREATE
 * 3. Send guarded CREATE request again using uid and gid. NFS3ERR_EXIST is expected
 */
- (void)testNFSCreateGuarded
{
    CREATE3res *res;
    char *file = "new_file";
    struct createhow3 how = {};
    uid_t uid;
    gid_t gid;
    int err;
    fattr3 *after;
    char path[PATH_MAX];

    how.mode = GUARDED;
    how.createhow3_u.obj_attributes.uid.set_it = TRUE;
    how.createhow3_u.obj_attributes.uid.set_uid3_u.uid = uid = 1;
    how.createhow3_u.obj_attributes.gid.set_it = TRUE;
    how.createhow3_u.obj_attributes.gid.set_gid3_u.gid = gid = 2;

    res = doCreateRPC(nclnt, &rootfh, file, &how);
    if (res->status != NFS3_OK) {
        XCTFail("doCreateRPC failed, got %d", res->status);
    }
    after = &res->CREATE3res_u.resok.obj_attributes.post_op_attr_u.attributes;

    XCTAssertEqual(uid, after->uid);
    XCTAssertEqual(gid, after->gid);

    res = doCreateRPC(nclnt, &rootfh, file, &how);
    if (res->status != NFS3ERR_EXIST) {
        XCTFail("doCreateRPC failed, expected status is %d, got %d", NFS3ERR_EXIST, res->status);
    }

    snprintf(path, sizeof(path), "%s/%s", getLocalMountedPath(), file);
    err = unlink(path);
    if (err) {
        XCTFail("unlink failed, got %d", err);
    }
}

/*
 * 1. Send MKDIR request using uid, gid, mtime and atime
 * 2. Verify gid/uid/mtime/atime attributes returned by MKDIR
 * 3. Call stat() - verify gid/uid/atime/mtiome attributes returned by MKDIR
 */
- (void)testNFSMKDir
{
    MKDIR3res *res;
    fattr3 *after;
    int dirFD, err;
    char *dir = "new_dir";
    struct stat stat = {};
    sattr3 attributes = {};
    char dirpath[PATH_MAX];
    uid_t uid;
    gid_t gid;

    // Expected results
    struct nfstime3 mtime = { .seconds = 999999999,  .nseconds = 99999999 };
    struct nfstime3 atime = { .seconds = 888888888,  .nseconds = 88888888 };

    attributes.uid.set_it = YES;
    attributes.uid.set_uid3_u.uid = uid = 1;
    attributes.gid.set_it = YES;
    attributes.gid.set_gid3_u.gid = gid = 2;
    attributes.mtime.set_it = SET_TO_CLIENT_TIME;
    attributes.mtime.set_mtime_u.mtime = mtime;
    attributes.atime.set_it = SET_TO_CLIENT_TIME;
    attributes.atime.set_atime_u.atime = atime;

    // Test mkdir
    res = doMkdirRPC(nclnt, &rootfh, dir, &attributes);
    if (res->status != NFS3_OK) {
        XCTFail("doMkdirRPC failed, got %d", res->status);
    }
    after = &res->MKDIR3res_u.resok.obj_attributes.post_op_attr_u.attributes;

    XCTAssertEqual(uid, after->uid);
    XCTAssertEqual(gid, after->gid);
    XCTAssertEqual(mtime.seconds, after->mtime.seconds);
    XCTAssertEqual(mtime.nseconds, after->mtime.nseconds);
    XCTAssertEqual(atime.seconds, after->atime.seconds);
    XCTAssertEqual(atime.nseconds, after->atime.nseconds);

    // Verify using stat
    snprintf(dirpath, sizeof(dirpath), "%s/%s", getLocalMountedPath(), dir);
    dirFD = open(dirpath, O_DIRECTORY | O_SEARCH);
    if (dirFD < 0) {
        XCTFail("Unable to open dir (%s): %d", dir, errno);
    }

    err = fstat(dirFD, &stat);
    if (err) {
        XCTFail("fstat failed, got %d", err);
    }

    XCTAssertEqual(uid, stat.st_uid);
    XCTAssertEqual(gid, stat.st_gid);
    XCTAssertEqual(mtime.seconds, stat.st_mtimespec.tv_sec);
    XCTAssertEqual(mtime.nseconds, stat.st_mtimespec.tv_nsec);
    XCTAssertEqual(atime.seconds, stat.st_atimespec.tv_sec);
    XCTAssertEqual(atime.nseconds, stat.st_atimespec.tv_nsec);

    if (dirFD >= 0) {
        err = rmdir(dirpath);
        if (err) {
            XCTFail("rmdir failed, got %d", err);

        }
    }

    if (dirFD >= 0) {
        close(dirFD);
    }
}

/*
 * 1. Send MKDIR request for ".", NFS3ERR_EXIST is expected
 */
- (void)testNFSMKDirDot
{
    MKDIR3res *res;
    char *dir = ".";
    sattr3 attributes = {};

    res = doMkdirRPC(nclnt, &rootfh, dir, &attributes);
    if (res->status != NFS3ERR_EXIST) {
        XCTFail("doMkdirRPC failed, expected status is %d, got %d", NFS3ERR_EXIST, res->status);
    }
}

/*
 * 1. Send MKDIR request for "..", NFS3ERR_EXIST is expected
 */
- (void)testNFSMKDirDotDot
{
    MKDIR3res *res;
    char *dir = "..";
    sattr3 attributes = {};

    res = doMkdirRPC(nclnt, &rootfh, dir, &attributes);
    if (res->status != NFS3ERR_EXIST) {
        XCTFail("doMkdirRPC failed, expected status is %d, got %d", NFS3ERR_EXIST, res->status);
    }
}

/*
 * 1. Send SYMLINK request using uid and gid
 * 2. Verify gid/uid attributes returned by SYMLINK
 */
- (void)testNFSSymlink
{
    SYMLINK3res *res;
    char *symlink = "new_symlink";
    char *data = "/tmp/symlink_data";
    sattr3 attributes = {};
    uid_t uid;
    gid_t gid;
    int err;
    char path[PATH_MAX];

    attributes.uid.set_it = TRUE;
    attributes.uid.set_uid3_u.uid = uid = 1;
    attributes.gid.set_it = TRUE;
    attributes.gid.set_gid3_u.gid = gid = 2;

    res = doSymlinkRPC(nclnt, &rootfh, symlink, &attributes, data);
    if (res->status != NFS3_OK) {
        XCTFail("doSymlinkRPC failed, got %d", res->status);
    }

    XCTAssertEqual(res->SYMLINK3res_u.resok.obj_attributes.post_op_attr_u.attributes.uid, uid);
    XCTAssertEqual(res->SYMLINK3res_u.resok.obj_attributes.post_op_attr_u.attributes.gid, gid);

    snprintf(path, sizeof(path), "%s/%s", getLocalMountedPath(), symlink);
    err = unlink(path);
    if (err) {
        XCTFail("unlink failed, got %d", err);
    }
}

/*
 * 1. Send MKNOD request using NF3REG type. NFS3ERR_BADTYPE is expected
 */
- (void)testNFSMKNodReg
{
    MKNOD3res *res;
    char *file = "new_file";
    struct mknoddata3 data = {};

    data.type = NF3REG;

    res = doMknodRPC(nclnt, &rootfh, file, &data);
    if (res->status != NFS3ERR_BADTYPE) {
        XCTFail("doMkdirRPC failed, expected status is %d, got %d", NFS3ERR_BADTYPE, res->status);
    }
}

/*
 * 1. Create new file
 * 2. Send MKNOD request using the same name and directory. NFS3ERR_EXIST is expected
 */
- (void)testNFSMKNodExists
{
    MKNOD3res *res;
    struct mknoddata3 data = {};
    int dirFD, fileFD, err;
    char *sock = "new_sock";
    uid_t uid;
    gid_t gid;

    err = createFileInPath(getLocalMountedPath(), sock, &dirFD, &fileFD);
    if (err) {
        XCTFail("createFileInPath failed, got %d", err);
        return;
    }

    data.type = NF3SOCK;
    data.mknoddata3_u.pipe_attributes.uid.set_it = TRUE;
    data.mknoddata3_u.pipe_attributes.uid.set_uid3_u.uid = uid = 1;
    data.mknoddata3_u.pipe_attributes.gid.set_it = TRUE;
    data.mknoddata3_u.pipe_attributes.gid.set_gid3_u.gid = gid = 2;

    res = doMknodRPC(nclnt, &rootfh, sock, &data);
    if (res->status != NFS3ERR_EXIST) {
        XCTFail("doMkdirRPC failed, expected status is %d, got %d", NFS3ERR_EXIST, res->status);
    }

    removeFromPath(sock, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Send MKNOD request for a socket using uid and gid
 * 2. Verify gid/uid/type attributes returned by MKNOD
 */
- (void)testNFSMKNodSocket
{
    MKNOD3res *res;
    struct mknoddata3 data = {};
    char *sock = "new_sock";
    ftype3 type;
    uid_t uid;
    gid_t gid;
    int err;
    char path[PATH_MAX];

    data.type = type = NF3SOCK;
    data.mknoddata3_u.pipe_attributes.uid.set_it = TRUE;
    data.mknoddata3_u.pipe_attributes.uid.set_uid3_u.uid = uid = 1;
    data.mknoddata3_u.pipe_attributes.gid.set_it = TRUE;
    data.mknoddata3_u.pipe_attributes.gid.set_gid3_u.gid = gid = 2;

    res = doMknodRPC(nclnt, &rootfh, sock, &data);
    if (res->status != NFS3_OK) {
        XCTFail("doMknodRPC failed, got %d", res->status);
    }

    XCTAssertEqual(res->MKNOD3res_u.resok.obj_attributes.post_op_attr_u.attributes.uid, uid);
    XCTAssertEqual(res->MKNOD3res_u.resok.obj_attributes.post_op_attr_u.attributes.gid, gid);
    XCTAssertEqual(res->MKNOD3res_u.resok.obj_attributes.post_op_attr_u.attributes.type, type);

    snprintf(path, sizeof(path), "%s/%s", getLocalMountedPath(), sock);
    err = unlink(path);
    if (err) {
        XCTFail("unlink failed, got %d", err);
    }
}

/*
 * 1. Send MKNOD request for a char device using uid and gid
 * 2. Verify gid/uid/rdev/type attributes returned by MKNOD
 */
- (void)testNFSMKNodChar
{
    MKNOD3res *res;
    struct mknoddata3 data = {};
    char *dev = "new_char";
    uint32_t specdata1, specdata2;
    ftype3 type;
    uid_t uid;
    gid_t gid;
    int err;
    char path[PATH_MAX];

    data.type = type = NF3CHR;
    data.mknoddata3_u.device.spec.specdata1 = specdata1 = 99;
    data.mknoddata3_u.device.spec.specdata2 = specdata2 = 88;
    data.mknoddata3_u.device.dev_attributes.uid.set_it = TRUE;
    data.mknoddata3_u.device.dev_attributes.uid.set_uid3_u.uid = uid = 1;
    data.mknoddata3_u.device.dev_attributes.gid.set_it = TRUE;
    data.mknoddata3_u.device.dev_attributes.gid.set_gid3_u.gid = gid = 2;

    res = doMknodRPC(nclnt, &rootfh, dev, &data);
    if (res->status != NFS3_OK) {
        XCTFail("doMknodRPC failed, got %d", res->status);
    }

    XCTAssertEqual(res->MKNOD3res_u.resok.obj_attributes.post_op_attr_u.attributes.uid, uid);
    XCTAssertEqual(res->MKNOD3res_u.resok.obj_attributes.post_op_attr_u.attributes.gid, gid);
    XCTAssertEqual(res->MKNOD3res_u.resok.obj_attributes.post_op_attr_u.attributes.type, type);
    XCTAssertEqual(res->MKNOD3res_u.resok.obj_attributes.post_op_attr_u.attributes.rdev.specdata1, specdata1);
    XCTAssertEqual(res->MKNOD3res_u.resok.obj_attributes.post_op_attr_u.attributes.rdev.specdata2, specdata2);

    snprintf(path, sizeof(path), "%s/%s", getLocalMountedPath(), dev);
    err = unlink(path);
    if (err) {
        XCTFail("unlink failed, got %d", err);
    }
}

/*
 * 1. Send REMOVE request non existing entry. NFS3ERR_NOENT is expected
 */
- (void)testNFSRemoveNoEntry
{
    REMOVE3res *res;
    char *file = "no_entry";

    res = doRemoveRPC(nclnt, &rootfh, file);
    if (res->status != NFS3ERR_NOENT) {
        XCTFail("doRemoveRPC failed, expected status is %d, got %d", NFS3ERR_NOENT, res->status);
    }
}

/*
 * 1. Create new file
 * 2. Send REMOVE request for the file
 * 3. Send REMOVE request again. NFS3ERR_NOENT is expected
 */
- (void)testNFSRemove
{
    REMOVE3res *res;
    int dirFD, fileFD, err;
    char *file = "new_file";

    err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
    if (err) {
        XCTFail("createFileInPath failed, got %d", err);
        return;
    }

    res = doRemoveRPC(nclnt, &rootfh, file);
    if (res->status != NFS3_OK) {
        XCTFail("doRemoveRPC failed, got %d", res->status);
    }

    res = doRemoveRPC(nclnt, &rootfh, file);
    if (res->status != NFS3ERR_NOENT) {
        XCTFail("doRemoveRPC failed, expected status is %d, got %d", NFS3ERR_NOENT, res->status);
    }

    close(dirFD);
}

/*
 * 1. Create new directory
 * 2. Send REMOVE request for the directory
 */
- (void)testNFSRemoveDir
{
    REMOVE3res *res;
    int dirFD, err;
    char *dir = "new_dir";

    err = createDirInPath(getLocalMountedPath(), dir, &dirFD);
    if (err) {
        XCTFail("createDirInPath failed, got %d", err);
        return;
    }

    res = doRemoveRPC(nclnt, &rootfh, dir);
    if (res->status != NFS3_OK) {
        XCTFail("doRemoveRPC failed, got %d", res->status);
        removeFromPath(dir, dirFD, -1, REMOVE_DIR);
        return;
    }

    res = doRemoveRPC(nclnt, &rootfh, dir);
    if (res->status != NFS3ERR_NOENT) {
        XCTFail("doRemoveRPC failed, expected status is %d, got %d", NFS3ERR_NOENT, res->status);
    }

    close(dirFD);
}

/*
 * 1. Create new directory
 * 2. Obtain the filehandle using LOOKUP
 * 3. Send RMDIR request for the directory
 * 4. Obtain the filehandle using LOOKUP. should fail with NFS3ERR_NOENT
 */
- (void)testNFSRMDir
{
    LOOKUP3res *res;
    RMDIR3res *res2;
    int dirFD, err;
    char *dir = "new_dir";

    err = createDirInPath(getLocalMountedPath(), dir, &dirFD);
    if (err) {
        XCTFail("createDirInPath failed, got %d", err);
        return;
    }

    res = doLookupRPC(nclnt, &rootfh, dir);
    if (res->status != NFS3_OK) {
        XCTFail("doLookupRPC failed, got %d", res->status);
    }

    res2 = doRMDirRPC(nclnt, &rootfh, dir);
    if (res2->status != NFS3_OK) {
        XCTFail("doRMDirRPC failed, got %d", res2->status);
    }

    res = doLookupRPC(nclnt, &rootfh, dir);
    if (res->status != NFS3ERR_NOENT) {
        XCTFail("doMkdirRPC failed, expected status is %d, got %d", NFS3ERR_NOENT, res->status);
    }

    if (dirFD >= 0) {
        close(dirFD);
    }
}

/*
 * 1. Create new directory
 * 2. Create new file within the directory
 * 3. Send RMDIR request for the directory. should fail with NFS3ERR_NOTEMPTY
 * 4. Remove the file
 * 5. Send RMDIR request for the directory
 */
- (void)testNFSRMDirNotEmpty
{
    RMDIR3res *res;
    int dirFD, dirFD2, fileFD, err;
    char *dir = "new_dir", *file = "new_file";
    char dirpath[PATH_MAX];

    err = createDirInPath(getLocalMountedPath(), dir, &dirFD);
    if (err) {
        XCTFail("createDirInPath failed, got %d", err);
        return;
    }

    snprintf(dirpath, sizeof(dirpath), "%s/%s", getLocalMountedPath(), dir);
    err = createFileInPath(dirpath, file, &dirFD2, &fileFD);
    if (err) {
        XCTFail("createFileInPath failed, got %d", err);
    }

    res = doRMDirRPC(nclnt, &rootfh, dir);
    if (res->status != NFS3ERR_NOTEMPTY) {
        XCTFail("doMkdirRPC failed, expected status is %d, got %d", NFS3ERR_NOTEMPTY, res->status);
    }
    removeFromPath(file, dirFD2, fileFD, REMOVE_FILE);

    res = doRMDirRPC(nclnt, &rootfh, dir);
    if (res->status != NFS3_OK) {
        XCTFail("doRMDirRPC failed, got %d", res->status);
    }

    if (dirFD >= 0) {
        close(dirFD);
    }
}

/*
 * 1. Create new file
 * 2. Send RMDIR request for the file. should fail with NFS3ERR_NOTDIR
 */
- (void)testNFSRMDirNotDir
{
    RMDIR3res *res;
    int dirFD, fileFD, err;
    char *file = "new_file";

    err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
    if (err) {
        XCTFail("createFileInPath failed, got %d", err);
        return;
    }

    res = doRMDirRPC(nclnt, &rootfh, file);
    if (res->status != NFS3ERR_NOTDIR) {
        XCTFail("doRMDirRPC failed, expected status is %d, got %d", NFS3ERR_NOTDIR, res->status);
    }

    removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Send RMDIR request for ".". should fail with NFS3ERR_INVAL
 */
- (void)testNFSRMDirDot
{
    RMDIR3res *res;
    char *file = ".";

    res = doRMDirRPC(nclnt, &rootfh, file);
    if (res->status != NFS3ERR_INVAL) {
        XCTFail("doRMDirRPC failed, expected status is %d, got %d", NFS3ERR_INVAL, res->status);
    }
}

/*
 * 1. Create new file
 * 2. Obtain the filehandle using LOOKUP
 * 3. Send RENAME of the file to a different name
 * 4. Send LOOKUP for the primary name, NFS3ERR_NOENT is expected
 */
- (void)testNFSRename
{
    LOOKUP3res *res;
    RENAME3res *res2;
    int dirFD, fileFD, err;
    char *file1 = "new_file1", *file2 = "new_file2";

    err = createFileInPath(getLocalMountedPath(), file1, &dirFD, &fileFD);
    if (err) {
        XCTFail("createFileInPath failed, got %d", err);
        return;
    }

    res = doLookupRPC(nclnt, &rootfh, file1);
    if (res->status != NFS3_OK) {
        XCTFail("doLookupRPC failed, got %d", res->status);
    }

    res2 = doRenameRPC(nclnt, &rootfh, file1, &rootfh, file2);
    if (res2->status != NFS3_OK) {
        XCTFail("doRenameRPC failed, got %d", res2->status);
    }

    res = doLookupRPC(nclnt, &rootfh, file1);
    if (res->status != NFS3ERR_NOENT) {
        XCTFail("doLookupRPC failed, expected status is %d, got %d", NFS3ERR_NOENT, res->status);
    }

    removeFromPath(file2, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Create new file #1
 * 2. Create new file #2
 * 3. Obtain the filehandle for file #1 using LOOKUP
 * 4. Obtain the filehandle for file #2 using LOOKUP
 * 5. Send RENAME of the file #1 to file #2
 * 6. Send LOOKUP for file #1, NFS3ERR_NOENT is expected
 * 7. Send LOOKUP for file #2, verify filehandle is equal to the one received by (3)
 */
- (void)testNFSRename2
{
    LOOKUP3res *res;
    RENAME3res *res2;
    int dirFD, fileFD, dirFD2, fileFD2, err;
    char *file1 = "new_file1", *file2 = "new_file2";
    u_int file1_fhlen;
    char file1_fhdata[NFSV4_MAX_FH_SIZE];

    err = createFileInPath(getLocalMountedPath(), file1, &dirFD, &fileFD);
    if (err) {
        XCTFail("createFileInPath failed, got %d", err);
        return;
    }

    err = createFileInPath(getLocalMountedPath(), file2, &dirFD2, &fileFD2);
    if (err) {
        XCTFail("createFileInPath failed, got %d", err);
        return;
    }

    res = doLookupRPC(nclnt, &rootfh, file1);
    if (res->status != NFS3_OK) {
        XCTFail("doLookupRPC failed, got %d", res->status);
    }
    file1_fhlen = res->LOOKUP3res_u.resok.object.data.data_len;
    memcpy(file1_fhdata, res->LOOKUP3res_u.resok.object.data.data_val, file1_fhlen);

    res = doLookupRPC(nclnt, &rootfh, file2);
    if (res->status != NFS3_OK) {
        XCTFail("doLookupRPC failed, got %d", res->status);
    }

    res2 = doRenameRPC(nclnt, &rootfh, file1, &rootfh, file2);
    if (res2->status != NFS3_OK) {
        XCTFail("doRenameRPC failed, got %d", res2->status);
    }

    res = doLookupRPC(nclnt, &rootfh, file1);
    if (res->status != NFS3ERR_NOENT) {
        XCTFail("doLookupRPC failed, expected status is %d, got %d", NFS3ERR_NOENT, res->status);
    }

    res = doLookupRPC(nclnt, &rootfh, file2);
    if (res->status != NFS3_OK) {
        XCTFail("doLookupRPC failed, got %d", res->status);
    }

    if (file1_fhlen != res->LOOKUP3res_u.resok.object.data.data_len) {
        XCTFail("doLookupRPC failed, file handle size is not equal, expected %d, got %d", file1_fhlen, res->LOOKUP3res_u.resok.object.data.data_len);
    } else if (memcmp(file1_fhdata, res->LOOKUP3res_u.resok.object.data.data_val, file1_fhlen) != 0) {
        XCTFail("doLookupRPC failed, file handle data is not equal");
    }

    removeFromPath(file1, dirFD, fileFD, REMOVE_FILE);
    removeFromPath(file2, dirFD2, fileFD2, REMOVE_FILE);
}

/*
 * 1. Create new file
 * 2. Create new directory
 * 3. Send RENAME of the file to directory. should fail with NFS3ERR_EXIST
 */
- (void)testNFSRenameExists
{
    RENAME3res *res;
    int dirFD, fileFD, subdirFD, err;
    char *file = "new_file", *dir = "new_dir";

    err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
    if (err) {
        XCTFail("createFileInPath failed, got %d", err);
        return;
    }

    err = createDirInPath(getLocalMountedPath(), dir, &subdirFD);
    if (err) {
        XCTFail("createDirInPath failed, got %d", err);
        return;
    }

    res = doRenameRPC(nclnt, &rootfh, file, &rootfh, dir);
    if (res->status != NFS3ERR_EXIST) {
        XCTFail("doRenameRPC failed, expected status is %d, got %d", NFS3ERR_EXIST, res->status);
    }

    removeFromPath(dir, subdirFD, -1, REMOVE_DIR);
    removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Create new file
 * 2. Call stat() verify link count is 1
 * 3. Obtain the filehandle for file using LOOKUP
 * 4. Send LINK using the filehandle obtained by LOOKUP
 * 5. Call stat() verify link count is 2
 */
- (void)testNFSLink
{
    LOOKUP3res *res;
    LINK3res *res2;
    int dirFD, fileFD, err;
    char *file = "new_file", *link = "new_link";
    struct stat stat = {};

    err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
    if (err) {
        XCTFail("createFileInPath failed, got %d", err);
        return;
    }

    err = fstat(fileFD, &stat);
    if (err) {
        XCTFail("fstat failed, got %d", err);
    }
    XCTAssertEqual(stat.st_nlink, 1);

    res = doLookupRPC(nclnt, &rootfh, file);
    if (res->status != NFS3_OK) {
        XCTFail("doLookupRPC failed, got %d", res->status);
    }

    res2 = doLinkRPC(nclnt, &res->LOOKUP3res_u.resok.object, &rootfh, link);
    if (res2->status != NFS3_OK) {
        XCTFail("doLinkRPC failed, got %d", res2->status);
    }

    err = fstat(fileFD, &stat);
    if (err) {
        XCTFail("fstat failed, got %d", err);
    }
    XCTAssertEqual(stat.st_nlink, 2);

    err = unlinkat(dirFD, link, 0);
    if (err) {
        XCTFail("unlinkat failed, got %d", err);
    }

    removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Send LINK to a directory. NFS3ERR_INVAL is expected
 */
- (void)testNFSLinkDir
{
    LINK3res *res;
    char *link = "new_link";

    res = doLinkRPC(nclnt, &rootfh, &rootfh, link);
    if (res->status != NFS3ERR_INVAL) {
        XCTFail("doLinkRPC failed, expected status is %d, got %d", NFS3ERR_INVAL, res->status);
    }
}

/*
 * 1. Send READDIR using STALE file handle, ESTALE is expected
 */
- (void)testNFSReaddirESTALE
{
    READDIR3res *res;
    count3 count = 300;
    cookie3 cookie = 0;
    cookieverf3 cookieverf = {};
    char buffer[8] = { 0, 1, 2, 3, 4, 5, 6, 7};
    nfs_fh3 fh = { .data.data_len = 8, .data.data_val = buffer };

    res = doReaddirRPC(nclnt, &fh, cookie, &cookieverf ,count);
    if (res->status != NFS3ERR_STALE) {
        XCTFail("Item should not be exists, got %d", res->status);
    }
}

/*
 * 1. Send READDIR using an empty buffer, NFS3ERR_TOOSMALL is expected
 */
- (void)testNFSReaddirTooSmall
{
    READDIR3res *res;
    count3 count = 0;
    cookie3 cookie = 0;
    cookieverf3 cookieverf = {};

    res = doReaddirRPC(nclnt, &rootfh, cookie, &cookieverf, count );
    if (res->status != NFS3ERR_TOOSMALL) {
        XCTFail("doReaddirRPC failed, expected status is %d, got %d", NFS3ERR_TOOSMALL, res->status);
    }
}

/*
 * 1. Send READDIR using non-zero cookie and invalid cookie verifier, NFS3ERR_BAD_COOKIE is expected
 */
- (void)testNFSReaddirBadCookie
{
    READDIR3res *res;
    count3 count = 1000;
    cookie3 cookie = 4444;
    cookieverf3 cookieverf = { 1, 2, 3, 4, 5, 6, 7, 8 };

    res = doReaddirRPC(nclnt, &rootfh, cookie, &cookieverf, count );
    if (res->status != NFS3ERR_BAD_COOKIE) {
        XCTFail("doReaddirRPC failed, expected status is %d, got %d", NFS3ERR_BAD_COOKIE, res->status);
    }
}

/*
 * 1. Send READDIR using zero cookie and invalid cookkie verifier, NFS3ERR_BAD_COOKIE is expected
 */
- (void)testNFSReaddirBadCookie2
{
    READDIR3res *res;
    count3 count = 1000;
    cookie3 cookie = 0;
    cookieverf3 cookieverf = { 1, 2, 3, 4, 5, 6, 7, 8 };

    res = doReaddirRPC(nclnt, &rootfh, cookie, &cookieverf, count );
    if (res->status != NFS3ERR_BAD_COOKIE) {
        XCTFail("doReaddirRPC failed, expected status is %d, got %d", NFS3ERR_BAD_COOKIE, res->status);
    }
}

/*
 * 1. Create 200 files
 * 2. Send READDIR to obtain the directory listing
 * 3. Send READDIR to obtain the rest of the directory using an invalid cookie verifier. NFS3ERR_BAD_COOKIE is expected
 */
- (void)testNFSReaddirBadCookie3
{
    int fds = 20, err;
    int fileFDs[fds], dirFDs[fds];
    READDIR3res *res;
    count3 count = 200;
    cookie3 cookie = 0;
    cookieverf3 cookieverf = { };

    for (int i = 0; i < fds; i++) {
        fileFDs[i] = dirFDs[i] = -1;
    }

    for (int i = 0; i < fds; i++) {
        char file[NAME_MAX] = {};
        snprintf(file, sizeof(file), "file_%d", i);
        err = createFileInPath(getLocalMountedPath(), file, &dirFDs[i], &fileFDs[i]);
        if (err) {
            XCTFail("createFileInPath failed, got %d", err);
            goto out;
        }
    }

    res = doReaddirRPC(nclnt, &rootfh, cookie, &cookieverf, count );
    if (res->status != NFS3_OK) {
        XCTFail("doReaddirRPC failed, got %d", res->status);
        goto out;
    }

    res->READDIR3res_u.resok.cookieverf[0]++;
    res = doReaddirRPC(nclnt, &rootfh, res->READDIR3res_u.resok.reply.entries->cookie, &res->READDIR3res_u.resok.cookieverf, count );
    if (res->status != NFS3ERR_BAD_COOKIE) {
        XCTFail("doReaddirRPC failed, expected status is %d, got %d", NFS3ERR_TOOSMALL, res->status);
    }

out:
    for (int i = 0 ; i < fds && fileFDs[i] != -1 ; i++) {
        char file[NAME_MAX] = {};
        snprintf(file, sizeof(file), "file_%d", i);
        removeFromPath(file, dirFDs[i], fileFDs[i], REMOVE_FILE);
    }
}

/*
 * 1. Create 200 files
 * 2. Send READDIR to obtain the directory listing. count the amount of entries
 * 3. Verify inode numbers
 * 4. Verify the total amount of entries received is equal to the amount we created
 */
- (void)testNFSReaddir
{
    int fds = 20, err;
    int fileFDs[fds], dirFDs[fds];
    READDIR3res *res;
    count3 count = 200;
    cookie3 cookie = 0;
    cookieverf3 zeroverf = { };

    for (int i = 0; i < fds; i++) {
        fileFDs[i] = dirFDs[i] = -1;
    }

    for (int i = 0; i < fds; i++) {
        char file[NAME_MAX] = {};
        snprintf(file, sizeof(file), "file_%d", i);
        err = createFileInPath(getLocalMountedPath(), file, &dirFDs[i], &fileFDs[i]);
        if (err) {
            XCTFail("createFileInPath failed, got %d", err);
            goto out;
        }
    }

    while (cookie < fds + 3) { // for ".", ".." and "dir3_1"
        res = doReaddirRPC(nclnt, &rootfh, cookie, cookie ? &res->READDIR3res_u.resok.cookieverf : &zeroverf, count );
        if (res->status != NFS3_OK) {
            XCTFail("doReaddirRPC failed, got %d", res->status);
            goto out;
        }

        entry3 *entry = res->READDIR3res_u.resok.reply.entries;
        if (entry == NULL) {
            if (res->status != NFS3_OK) {
                XCTFail("doReaddirRPC failed, got NULL entry");
                goto out;
            }
        }

        while (entry) {
            // Verify entry
            struct stat stat = {};
            err = fstatat(dirFDs[0], entry->name, &stat, 0);
            if (err) {
                XCTFail("fstatat failed, got %d", err);
                goto out;
            }
            XCTAssertEqual(entry->fileid, stat.st_ino);

            cookie = entry->cookie;
            entry = entry->nextentry;
        }

        if (res->READDIR3res_u.resok.reply.eof) {
            break;
        }
    }

    XCTAssertEqual(cookie, fds + 3);

out:
    for (int i = 0 ; i < fds && fileFDs[i] != -1 ; i++) {
        char file[NAME_MAX] = {};
        snprintf(file, sizeof(file), "file_%d", i);
        removeFromPath(file, dirFDs[i], fileFDs[i], REMOVE_FILE);
    }
}

/*
 * 1. Create 200 files
 * 2. Send READDIRPLUS to obtain the directory listing. count the amount of entries
 * 3. Verify inode numbers, attributes and filehandle using LOOKUP for each entry
 * 3. Verify the total amount of entries received is equal to the amount we created
 */
- (void)testNFSReaddirplus
{
    int fds = 20, err;
    int fileFDs[fds], dirFDs[fds];
    READDIRPLUS3res *res;
    count3 dircount = 1024;
    count3 maxcount = 1024;
    cookie3 cookie = 0;
    cookieverf3 zeroverf = { };

    for (int i = 0; i < fds; i++) {
        fileFDs[i] = dirFDs[i] = -1;
    }

    for (int i = 0; i < fds; i++) {
        char file[NAME_MAX] = {};
        snprintf(file, sizeof(file), "file_%d", i);
        err = createFileInPath(getLocalMountedPath(), file, &dirFDs[i], &fileFDs[i]);
        if (err) {
            XCTFail("createFileInPath failed, got %d", err);
            goto out;
        }
    }

    while (cookie < fds + 3) { // for ".", ".." and "dir3_1"
        res = doReaddirplusRPC(nclnt, &rootfh, cookie, cookie ? &res->READDIRPLUS3res_u.resok.cookieverf : &zeroverf, dircount, maxcount );
        if (res->status != NFS3_OK) {
            XCTFail("doReaddirplusRPC failed, got %d", res->status);
            goto out;
        }

        entryplus3 *entry = res->READDIRPLUS3res_u.resok.reply.entries;
        if (entry == NULL) {
            if (res->status != NFS3_OK) {
                XCTFail("doReaddirplusRPC failed, got NULL entry");
                goto out;
            }
        }

        while (entry) {
            struct stat stat = {};
            LOOKUP3res *res2;

            err = fstatat(dirFDs[0], entry->name, &stat, 0);
            if (err) {
                XCTFail("fstatat failed, got %d", err);
                goto out;
            }

            // Compare attributes
            XCTAssertEqual(entry->fileid, stat.st_ino);
            XCTAssertEqual(entry->name_attributes.post_op_attr_u.attributes.type, S_ISDIR(stat.st_mode) ? NF3DIR : NF3REG);
            XCTAssertEqual(entry->name_attributes.post_op_attr_u.attributes.mode, stat.st_mode & 07777);
            XCTAssertEqual(entry->name_attributes.post_op_attr_u.attributes.nlink, stat.st_nlink);
            XCTAssertEqual(entry->name_attributes.post_op_attr_u.attributes.uid, stat.st_uid);
            XCTAssertEqual(entry->name_attributes.post_op_attr_u.attributes.gid, stat.st_gid);
            XCTAssertEqual(entry->name_attributes.post_op_attr_u.attributes.size, stat.st_size);
            XCTAssertEqual(entry->name_attributes.post_op_attr_u.attributes.rdev.specdata1, major(stat.st_rdev));
            XCTAssertEqual(entry->name_attributes.post_op_attr_u.attributes.rdev.specdata2, minor(stat.st_rdev));
            XCTAssertEqual(entry->name_attributes.post_op_attr_u.attributes.fsid, stat.st_dev);
            XCTAssertEqual(entry->name_attributes.post_op_attr_u.attributes.atime.seconds, stat.st_atimespec.tv_sec);
            XCTAssertEqual(entry->name_attributes.post_op_attr_u.attributes.atime.nseconds, stat.st_atimespec.tv_nsec);
            XCTAssertEqual(entry->name_attributes.post_op_attr_u.attributes.mtime.seconds, stat.st_mtimespec.tv_sec);
            XCTAssertEqual(entry->name_attributes.post_op_attr_u.attributes.mtime.nseconds, stat.st_mtimespec.tv_nsec);
            XCTAssertEqual(entry->name_attributes.post_op_attr_u.attributes.ctime.seconds, stat.st_ctimespec.tv_sec);
            XCTAssertEqual(entry->name_attributes.post_op_attr_u.attributes.ctime.nseconds, stat.st_ctimespec.tv_nsec);

            // Compare filehandle
            res2 = doLookupRPC(nclnt, &rootfh, entry->name);
            if (res2->status != NFS3_OK) {
                XCTFail("doLookupRPC failed, got %d", res2->status);
                goto out;
            }
            XCTAssertEqual(entry->name_handle.post_op_fh3_u.handle.data.data_len, res2->LOOKUP3res_u.resok.object.data.data_len);
            XCTAssertEqual(memcmp(entry->name_handle.post_op_fh3_u.handle.data.data_val, res2->LOOKUP3res_u.resok.object.data.data_val, res2->LOOKUP3res_u.resok.object.data.data_len) ,0);

            cookie = entry->cookie;
            entry = entry->nextentry;
        }

        if (res->READDIRPLUS3res_u.resok.reply.eof) {
            break;
        }
    }

    XCTAssertEqual(cookie, fds + 3);

out:
    for (int i = 0 ; i < fds && fileFDs[i] != -1 ; i++) {
        char file[NAME_MAX] = {};
        snprintf(file, sizeof(file), "file_%d", i);
        removeFromPath(file, dirFDs[i], fileFDs[i], REMOVE_FILE);
    }
}

/*
 * 1. Send FSSTAT
 * 2. Compare expected results
 */
- (void)testNFSFSStat
{
    FSSTAT3res *res;

    res = doFSStatRPC(nclnt, &rootfh);
    if (res->status != NFS3_OK) {
        XCTFail("doFSStatRPC failed, got %d", res->status);
        return;
    }

    // resok->tbytes
    // resok->fbytes
    // resok->abytes
    // resok->tfiles
    // resok->ffiles
    // resok->afiles
    XCTAssertEqual(res->FSSTAT3res_u.resok.invarsec, 0);
}

/*
 * 1. Send FSINFO
 * 2. Compare expected results
 */
- (void)testNFSFSInfo
{
    FSINFO3res *res;
    struct FSINFO3resok *info;

    res = doFSinfoRPC(nclnt, &rootfh);
    if (res->status != NFS3_OK) {
        XCTFail("doFSinfoRPC failed, got %d", res->status);
        return;
    }
    info = &res->FSINFO3res_u.resok;

    // info->rtmax
    // info->rtpref
    XCTAssertEqual(info->rtmult, NFS_FABLKSIZE); // Hardcoded in nfsrv_fsinfo
    // info->wtmax
    // info->wtpref
    XCTAssertEqual(info->wtmult, NFS_FABLKSIZE); // Hardcoded in nfsrv_fsinfo
    // info->dtperf
    XCTAssertEqual(info->maxfilesize, 0xffffffffffffffffULL); // Hardcoded in nfsrv_fsinfo
    XCTAssertEqual(info->time_delta.seconds, 0);
    XCTAssertEqual(info->time_delta.nseconds, 1);
    XCTAssertEqual(info->properties, FSF3_LINK | FSF3_SYMLINK | FSF3_HOMOGENEOUS | FSF3_CANSETTIME);
}

/*
 * 1. Send PATHCONF
 * 2. Compare results with pathconf() syscall
 */
- (void)testNFSPathconf
{
    PATHCONF3res *res;

    res = doPathconfRPC(nclnt, &rootfh);
    if (res->status != NFS3_OK) {
        XCTFail("doPathconfRPC failed, got %d", res->status);
        return;
    }

    XCTAssertEqual(res->PATHCONF3res_u.resok.linkmax, pathconf(getLocalMountedPath(), _PC_LINK_MAX));
    XCTAssertEqual(res->PATHCONF3res_u.resok.name_max, pathconf(getLocalMountedPath(), _PC_NAME_MAX));
    XCTAssertEqual(res->PATHCONF3res_u.resok.no_trunc, pathconf(getLocalMountedPath(), _PC_NO_TRUNC));
    XCTAssertEqual(res->PATHCONF3res_u.resok.chown_restricted, pathconf(getLocalMountedPath(), _PC_CHOWN_RESTRICTED));
    XCTAssertEqual(res->PATHCONF3res_u.resok.case_insensitive, !pathconf(getLocalMountedPath(), _PC_CASE_SENSITIVE));
    XCTAssertEqual(res->PATHCONF3res_u.resok.case_preserving, pathconf(getLocalMountedPath(), _PC_CASE_PRESERVING));
}

/*
 * 1. Create new directory
 * 2. Obtain the filehandle using LOOKUP
 * 3. Send COMMIT. NFS3ERR_IO is expected
 */
- (void)testNFSCommitInvalidType
{
    LOOKUP3res *res;
    COMMIT3res *res2;
    int dirFD, err, count = 1, offset = 0;
    char *dir = "new_dir";

    err = createDirInPath(getLocalMountedPath(), dir, &dirFD);
    if (err) {
        XCTFail("createDirInPath failed, got %d", err);
        return;
    }

    res = doLookupRPC(nclnt, &rootfh, dir);
    if (res->status != NFS3_OK) {
        XCTFail("doLookupRPC failed, got %d", res->status);
    }

    res2 = doCommitRPC(nclnt, &res->LOOKUP3res_u.resok.object, offset, count);
    if (res2->status != NFS3ERR_IO) {
        XCTFail("doCommitRPC should failed with %d, got %d", NFS3ERR_IO, res2->status);
    }

    removeFromPath(dir, dirFD, -1, REMOVE_DIR);
}

/*
 * 1. Create new file
 * 2. Obtain the filehandle using LOOKUP
 * 3. Send unstable WRITE request
 * 4. Send COMMIT
 * 5. read() from the local file, compare results
 */
- (void)testNFSCommit
{
    LOOKUP3res *res;
    WRITE3res *res2;
    COMMIT3res *res3;
    int dirFD, fileFD, err, count = 5, offset = 0;
    char *file = "new_file";
    char buf[10] = "1234567890";
    char buf2[10] = {};
    size_t n;

    err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
    if (err) {
        XCTFail("createFileInPath failed, got %d", err);
        return;
    }

    res = doLookupRPC(nclnt, &rootfh, file);
    if (res->status != NFS3_OK) {
        XCTFail("doLookupRPC failed, got %d", res->status);
    }

    res2 = doWriteRPC(nclnt, &res->LOOKUP3res_u.resok.object, offset, count, UNSTABLE, sizeof(buf), buf);
    if (res2->status != NFS3_OK) {
        XCTFail("doWriteRPC failed, got %d", res->status);
    }

    res3 = doCommitRPC(nclnt, &res->LOOKUP3res_u.resok.object, 0, 0);
    if (res2->status != NFS3_OK) {
        XCTFail("doCommitRPC failed, got %d", res->status);
    }

    n = read(fileFD, buf2, sizeof(buf2));
    if (n < 0) {
        XCTFail("read failed, got %zu, errno %d", n, errno);
    }

    XCTAssertTrue(n >= count);
    XCTAssertEqual(memcmp(buf, buf2, count), 0);

    removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

@end

@interface nfsrvTests_network : XCTestCase

@end

@implementation nfsrvTests_network

/*
 * 1. Setup IPv4, UDP connection with NULL authentication
 * 2. Send NULL to the server, make sure we got the reply
 */
- (void)testNetworkIPv4UDPAuthNull
{
    doNFSSetUpVerbose(AF_INET, SOCK_DGRAM, RPCAUTH_NULL);

    doNullRPC(nclnt);
}

/*
 * 1. Setup IPv4, UDP connection with UNIX authentication
 * 2. Send NULL to the server, make sure we got the reply
 */
- (void)testNetworkIPv4UDPAuthUnix
{
    doNFSSetUpVerbose(AF_INET, SOCK_DGRAM, RPCAUTH_UNIX);

    doNullRPC(nclnt);
}

/*
 * 1. Setup IPv4, TCP connection with NULL authentication
 * 2. Send NULL to the server, make sure we got the reply
 */
- (void)testNetworkIPv4TCPAuthNull
{
    doNFSSetUpVerbose(AF_INET, SOCK_STREAM, RPCAUTH_NULL);

    doNullRPC(nclnt);
}

/*
 * 1. Setup IPv4, TCP connection with UNIX authentication
 * 2. Send NULL to the server, make sure we got the reply
 */
- (void)testNetworkIPv4TCPAuthUnix
{
    doNFSSetUpVerbose(AF_INET, SOCK_STREAM, RPCAUTH_UNIX);

    doNullRPC(nclnt);
}

/*
 * 1. Setup IPv6, UDP connection with NULL authentication
 * 2. Send NULL to the server, make sure we got the reply
 */
- (void)testNetworkIPv6UDPAuthNull
{
    doNFSSetUpVerbose(AF_INET6, SOCK_DGRAM, RPCAUTH_NULL);

    doNullRPC(nclnt);
}

/*
 * 1. Setup IPv6, UDP connection with UNIX authentication
 * 2. Send NULL to the server, make sure we got the reply
 */
- (void)testNetworkIPv6UDPAuthUnix
{
    doNFSSetUpVerbose(AF_INET6, SOCK_DGRAM, RPCAUTH_UNIX);

    doNullRPC(nclnt);
}

/*
 * 1. Setup IPv6, TCP connection with NULL authentication
 * 2. Send NULL to the server, make sure we got the reply
 */
- (void)testNetworkIPv6TCPAuthNull
{
    doNFSSetUpVerbose(AF_INET6, SOCK_STREAM, RPCAUTH_NULL);

    doNullRPC(nclnt);
}

/*
 * 1. Setup IPv6, TCP connection with UNIX authentication
 * 2. Send NULL to the server, make sure we got the reply
 */
- (void)testNetworkIPv6TCPAuthUnix
{
    doNFSSetUpVerbose(AF_INET6, SOCK_STREAM, RPCAUTH_UNIX);

    doNullRPC(nclnt);
}

/*
 * 1. Setup AF_LOCAL, TCP connection with NULL authentication
 * 2. Send NULL to the server, make sure we got the reply
 */
- (void)testNetworkLocalTCPAuthNull
{
    doNFSSetUpVerbose(AF_LOCAL, SOCK_STREAM, RPCAUTH_NULL);

    doNullRPC(nclnt);
}

/*
 * 1. Setup AF_LOCAL, TCP connection with UNIX authentication
 * 2. Send NULL to the server, make sure we got the reply
 */
- (void)testNetworkLocalTCPAuthUnix
{
    doNFSSetUpVerbose(AF_LOCAL, SOCK_STREAM, RPCAUTH_UNIX);

    doNullRPC(nclnt);
}

- (void)tearDown
{
    memset(&rootfh, 0, sizeof(rootfh));
    doMountTearDown();

    if (nclnt) {
        clnt_destroy(nclnt);
        nclnt = NULL;
    }
}

@end
