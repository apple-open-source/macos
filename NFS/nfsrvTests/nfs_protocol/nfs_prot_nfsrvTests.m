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
#include <arpa/inet.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <oncrpc/auth.h>
#include <oncrpc/clnt.h>
#include <oncrpc/types.h>
#include <nfs/rpcv2.h>
#include <mach/clock_types.h>

#include "common.h"
#include "mountd.h"
#include "nfs_prot.h"
#include "pathnames.h"
#include "nfs_prot_rpc.h"
#include "test_utils.h"

#import <XCTest/XCTest.h>

#define NFS_ACCESS_READ                 0x01
#define NFS_ACCESS_LOOKUP               0x02
#define NFS_ACCESS_MODIFY               0x04
#define NFS_ACCESS_EXTEND               0x08
#define NFS_ACCESS_DELETE               0x10
#define NFS_ACCESS_EXECUTE              0x20
#define NFS_ACCESS_ALL                  (NFS_ACCESS_READ | NFS_ACCESS_MODIFY | NFS_ACCESS_EXTEND | NFS_ACCESS_EXECUTE | NFS_ACCESS_DELETE | NFS_ACCESS_LOOKUP)

#define NFS_FABLKSIZE   512     /* Size in bytes of a block wrt fa_blocks */

#ifndef NFS_MAXPACKET
#define NFS_MAXPACKET (1024 * 1024 * 16)
#endif

#ifndef NFSSVC_USERCOUNT
#define NFSSVC_USERCOUNT        0x040    /* gets current count of active nfs users */
#endif

#ifndef NFSSVC_EXPORTSTATS
#define NFSSVC_EXPORTSTATS      0x010    /* gets exported directory stats */
#endif

#ifndef NFSSVC_USERSTATS
#define NFSSVC_USERSTATS        0x020    /* gets exported directory active user stats */
#endif

CLIENT *nclnt = NULL;
nfs_fh3 rootfh = {};
static nfs_fh rootfh_v2 = {};

CLIENT *
createClientForNFSProtocol(int socketFamily, int socketType, int authType, int flags, int *sockp)
{
	const char *host;
	int version = (flags == CREATE_NFS_V2) ? NFS_VERSION : NFS_V3;

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

	return createClientForProtocol(host, socketFamily, socketType, authType, NFS_PROGRAM, version, flags, sockp);
}

int
createFileInPath(const char *dir, char *file, int *dirFDp, int *fileFDp)
{
	int dirFD = -1, fileFD = -1;

	if (dir == NULL || file == NULL || dirFDp == NULL || fileFDp == NULL) {
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

	if (dir == NULL || subdir == NULL || dirFDp == NULL) {
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
createSymlinkInPath(const char *dir, char *file1, char *file2, int *dirFDp)
{
	int dirFD = -1, err;

	if (dir == NULL || file1 == NULL || file2 == NULL || dirFDp == NULL) {
		XCTFail("Got NULL input");
		return -1;
	}

	dirFD = open(dir, O_DIRECTORY | O_SEARCH);
	if (dirFD < 0) {
		XCTFail("Unable to open dir (%s): %d", dir, errno);
		return -1;
	}

	err = symlinkat(file1, dirFD, file2);
	if (err < 0) {
		close(dirFD);
		XCTFail("Unable to create symlink (%s:/%s):%s %d", dir, file2, file1, errno);
		return -1;
	}

	*dirFDp = dirFD;

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
	if (err != 0) {
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

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

static int
switchToUnknownUserAndGroup(int fileFD)
{
	int err;
	AUTH *auth = NULL;

	if (fileFD >= 0) {
		err = fchown(fileFD, UNKNOWNUID, UNKNOWNGID);
		if (err != 0) {
			XCTFail("fchown failed for uid %d, gid %d", UNKNOWNUID, UNKNOWNGID);
			return err;
		}
	}

	/* switch to the user from root */
	err = pthread_setugid_np(UNKNOWNUID, UNKNOWNGID);
	if (err != 0) {
		XCTFail("pthread_setugid_np failed for uid %d, gid %d", UNKNOWNUID, UNKNOWNGID);
		return err;
	}

	auth = authunix_create_default();
	if (auth == NULL) {
		XCTFail("Unable to create auth");
		return ENOMEM;
	}
	clnt_auth_set(nclnt, auth);

	return 0;
}

static int
switchToRoot(void)
{
	/* and back to being root */
	int err = pthread_setugid_np(KAUTH_UID_NONE, KAUTH_GID_NONE);
	if (err != 0) {
		XCTFail("pthread_setugid_np failed for root");
	}

	return err;
}

#pragma clang diagnostic pop

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

void
doNFSSetUp(const char* path, const char **nfsdArgs, int nfsdArgsSize, int socketFamily,
    int socketType, int authType, int flags, char *exports_file, char *sec_mech)
{
	int err;
	fhandle_t *fh;

	memset(&rootfh_v2, 0, sizeof(rootfh_v2));
	memset(&rootfh, 0, sizeof(rootfh));

	if (exports_file != NULL) {
		//setup is done, just call nfsd to export
		doMountExport(nfsdArgs, nfsdArgsSize, exports_file);
	} else {
		doMountSetUpWithArgs(nfsdArgs, nfsdArgsSize, NULL, 0);
	}

	if (path == NULL) {
		//if path is NULL, use default
		path = getLocalMountedPath();
	}
	err = createClientForMountProtocol(socketFamily, socketType, authType, 0);
	if (err) {
		XCTFail("Cannot create client mount: %d", err);
	}

	nclnt = createClientForNFSProtocol(socketFamily, socketType, authType, flags, NULL);
	if (nclnt == NULL) {
		XCTFail("Cannot create client for NFS");
	}

	if ((fh = doMountAndVerify(path, sec_mech)) == NULL) {
		XCTFail("doMountAndVerify failed");
	} else {
		/* NFSv2 root file handle */
		memcpy(rootfh_v2.data, fh->fh_data, MIN(NFS_FHSIZE, fh->fh_len));

		/* NFSv3 root file handle */
		rootfh.data.data_len = fh->fh_len;
		rootfh.data.data_val = (char *)fh->fh_data;
	}
}

static void
doNFSSetUpWithArgs(const char **nfsdArgs, int nfsdArgsSize, int socketFamily,
    int socketType, int authType, int flags)
{
	doNFSSetUp(NULL, nfsdArgs, nfsdArgsSize, socketFamily, socketType, authType, flags, NULL, "sys:krb5");
}

static void
doNFSSetUpVerbose(int socketFamily, int socketType, int authType)
{
	const char *argv[1] = { "-vvvvv" };
	doNFSSetUpWithArgs(argv, ARRAY_SIZE(argv), socketFamily, socketType, authType, 0);
}

static void
doNFSSetUpWrapper(int socketFamily, int socketType, int authType)
{
	doNFSSetUpWithArgs(NULL, 0, socketFamily, socketType, authType, 0);
}

void
destroy_nclnt(void)
{
	if (nclnt) {
		clnt_destroy(nclnt);
		nclnt = NULL;
	}
}

@interface nfsrvTests_nfs_v3 : XCTestCase
@end

@implementation nfsrvTests_nfs_v3

- (void)setUp
{
	/* Enable NFS server debug */
	sysctl_set("vfs.generic.nfs.server.debug_ctl", -1);

	doNFSSetUpWithArgs(NULL, 0, AF_INET, SOCK_STREAM, RPCAUTH_UNIX, 0);
}

- (void)tearDown
{
	memset(&rootfh, 0, sizeof(rootfh));
	doMountTearDown();

	destroy_nclnt();

	sysctl_set("vfs.generic.nfs.server.debug_ctl", 0);
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
 * 1. Send GETATTR with a bad file handle, BADHANDLE is expected
 */
- (void)testNFSGetAttrEBadHandle
{
	GETATTR3res *res;
	char buffer[NFSV3_MAX_FH_SIZE + 1] = {};
	nfs_fh3 fh = { .data.data_len = sizeof(buffer), .data.data_val = buffer };

	res = doGetattrRPC(nclnt, &fh);
	if (res == NULL) {
		XCTFail("doGetattrRPC retuned NULL");
		return;
	}
	if (res->status != NFS3ERR_BADHANDLE) {
		XCTFail("doGetattrRPC failed, expected status is %d, got %d", NFS3ERR_BADHANDLE, res->status);
	}
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
 * 1. Send SETATTR with a bad file handle, BADHANDLE is expected
 */
- (void)testNFSSetAttrEBadHandle
{
	SETATTR3res *res;
	sattr3 attr = {};
	char buffer[NFSV3_MAX_FH_SIZE + 1] = {};
	nfs_fh3 fh = { .data.data_len = sizeof(buffer), .data.data_val = buffer };

	attr.mode.set_it = TRUE;
	attr.mode.set_mode3_u.mode = 777;

	res = doSetattrRPC(nclnt, &fh, &attr, NULL);
	if (res == NULL) {
		XCTFail("doSetattrRPC retuned NULL");
		return;
	}
	if (res->status != NFS3ERR_BADHANDLE) {
		XCTFail("doSetattrRPC failed, expected status is %d, got %d", NFS3ERR_BADHANDLE, res->status);
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
 * 2. chmod the file to read-only
 * 3. Obtain the filehandle using LOOKUP
 * 4. Retrieve attributes using GETATTR
 * 5. Call SETATTR to modify file mtime, expect NFS3ERR_ACCES
 */
- (void)testNFSSetattrReadOnly
{
	sattr3 attr = {};
	LOOKUP3res *res;
	GETATTR3res *res2;
	SETATTR3res *res3;
	int dirFD, fileFD, err;
	char *file = "new_file";
	struct timespec guard = {};

	err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
	if (err) {
		XCTFail("createFileInPath failed, got %d", err);
		return;
	}

	err = fchmod(fileFD, S_IRUSR | S_IRGRP | S_IROTH);
	if (err) {
		XCTFail("fchmod failed, got %d", err);
	}

	res = doLookupRPC(nclnt, &rootfh, file);
	if (res->status != NFS3_OK) {
		XCTFail("doLookupRPC failed, got %d", res->status);
	}

	err = switchToUnknownUserAndGroup(-1);
	if (err != 0) {
		XCTFail("switchToUnknownUserAndGroup failed %d", err);
	}

	res2 = doGetattrRPC(nclnt, &res->LOOKUP3res_u.resok.object);
	if (res2->status != NFS3_OK) {
		XCTFail("doGetattrRPC failed, got %d", res2->status);
	}

	guard.tv_sec = res2->GETATTR3res_u.resok.obj_attributes.ctime.seconds;
	guard.tv_nsec = res2->GETATTR3res_u.resok.obj_attributes.ctime.nseconds;

	attr.mtime.set_it = SET_TO_CLIENT_TIME;
	attr.mtime.set_mtime_u.mtime.seconds = 99;
	attr.mtime.set_mtime_u.mtime.nseconds = 66;

	res3 = doSetattrRPC(nclnt, &res->LOOKUP3res_u.resok.object, &attr, &guard);
	if (res3->status != NFS3ERR_ACCES) {
		XCTFail("doSetattrRPC failed, expected %d, got %d", NFS3ERR_ACCES, res3->status);
	}

	err = switchToRoot();
	if (err != 0) {
		XCTFail("switchToRoot failed %d", err);
	}

	removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Create new file in ROFS
 * 2. Obtain the filehandle using LOOKUP
 * 3. Retrieve attributes using GETATTR
 * 4. Call SETATTR to modify file mtime, expect NFS3ERR_ROFS
 */
- (void)testNFSSetattrROFS
{
	sattr3 attr = {};
	fhandle_t *fh;
	nfs_fh3 rofh = {};
	LOOKUP3res *res;
	GETATTR3res *res2;
	SETATTR3res *res3;
	int dirFD, fileFD, err;
	char *file = "new_file";
	struct timespec guard = {};

	if ((fh = doMountAndVerify(getLocalMountedReadOnlyPath(), "sys:krb5")) == NULL) {
		XCTFail("doMountAndVerify failed");
		return;
	}

	rofh.data.data_len = fh->fh_len;
	rofh.data.data_val = (char *)fh->fh_data;

	err = createFileInPath(getLocalMountedReadOnlyPath(), file, &dirFD, &fileFD);
	if (err) {
		XCTFail("createFileInPath failed, got %d", err);
		return;
	}

	res = doLookupRPC(nclnt, &rofh, file);
	if (res->status != NFS3_OK) {
		XCTFail("doLookupRPC failed, got %d", res->status);
		goto bad;
	}

	res2 = doGetattrRPC(nclnt, &res->LOOKUP3res_u.resok.object);
	if (res2->status != NFS3_OK) {
		XCTFail("doGetattrRPC failed, got %d", res2->status);
		goto bad;
	}

	guard.tv_sec = res2->GETATTR3res_u.resok.obj_attributes.atime.seconds;
	guard.tv_nsec = res2->GETATTR3res_u.resok.obj_attributes.atime.nseconds;

	attr.atime.set_it = SET_TO_CLIENT_TIME;
	attr.atime.set_atime_u.atime.seconds = 99;
	attr.atime.set_atime_u.atime.nseconds = 66;

	attr.mtime.set_it = SET_TO_SERVER_TIME;

	res3 = doSetattrRPC(nclnt, &res->LOOKUP3res_u.resok.object, &attr, &guard);
	if (res3->status != NFS3ERR_ROFS) {
		XCTFail("doSetattrRPC failed, expected %d, got %d", NFS3ERR_ROFS, res3->status);
	}

bad:
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

	attr.atime.set_it = SET_TO_SERVER_TIME;

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
 * 1. call LOOKUP with a stale file handle, NFS3ERR_STALE expected
 */
- (void)testNFSLookupStale
{
	LOOKUP3res *res;
	char *file = "new_file";
	char buffer[8] = { 0, 1, 2, 3, 4, 5, 6, 7};
	nfs_fh3 fh = { .data.data_len = 8, .data.data_val = buffer };

	res = doLookupRPC(nclnt, &fh, file);
	if (res->status != NFS3ERR_STALE) {
		XCTFail("doLookupRPC failed, expected status is %d, got %d", NFS3ERR_STALE, res->status);
	}
}

/*
 * 1. call LOOKUP with a bad file handle, NFS3ERR_BADHANDLE expected
 */
- (void)testNFSLookupBadHandle
{
	LOOKUP3res *res;
	char *file = "new_file";
	char buffer[NFSV3_MAX_FH_SIZE + 1] = {};
	nfs_fh3 fh = { .data.data_len = sizeof(buffer), .data.data_val = buffer };

	res = doLookupRPC(nclnt, &fh, file);
	if (res == NULL) {
		XCTFail("doLookupRPC returned NULL");
		return;
	}
	if (res->status != NFS3ERR_BADHANDLE) {
		XCTFail("doLookupRPC failed, expected status is %d, got %d", NFS3ERR_BADHANDLE, res->status);
	}
}

/*
 * 1. Send LOOKUP to a file with a name longer than pathconf.name_max
 * 2. if pathconf.no_trunc is true, expect NFS3ERR_NAMETOOLONG, else expect NFS3_OK
 */
- (void)testNFSLookupNameTooLong
{
	PATHCONF3res *res;
	LOOKUP3res *res2;
	uint32_t name_max;


	res = doPathconfRPC(nclnt, &rootfh);
	if (res->status != NFS3_OK) {
		XCTFail("doPathconfRPC failed, got %d", res->status);
		return;
	}

	name_max = res->PATHCONF3res_u.resok.name_max;
	char name[name_max + 2];
	memset(name, 'a', name_max + 1);
	name[name_max + 1] = '\0';

	res2 = doLookupRPC(nclnt, &rootfh, name);
	if (res->PATHCONF3res_u.resok.no_trunc) {
		if (res2->status != NFS3ERR_NAMETOOLONG) {
			XCTFail("doLinkRPC failed, expected status is %d, got %d", NFS3ERR_NAMETOOLONG, res2->status);
		}
	} else {
		if (res2->status != NFS3_OK) {
			XCTFail("doLookupRPC failed, got %d", res2->status);
		}
	}
}

/*
 * 1. Create new file
 * 2. Obtain the filehandle using LOOKUP
 * 3. call LOOKUP with the fh of the file
 * 4. expect NFS3ERR_NOTDIR
 */
- (void)testNFSLookupNotDir
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

	res = doLookupRPC(nclnt, &res->LOOKUP3res_u.resok.object, file);
	if (res->status != NFS3ERR_NOTDIR) {
		XCTFail("doLookupRPC failed, expected %d, got %d", NFS3ERR_NOTDIR, res->status);
	}

	removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Create a dir
 * 2. Create a file in the new dir
 * 3. set dir access mode as 0
 * 4. do LOOKUP call on the directory to find the file
 * 5. expect NFS3ERR_ACCES
 */
- (void)testNFSLookupNoAccess
{
	char *dir = "dir";
	char *file = "file";
	int dirFD1 = -1, dirFD2 = -1;
	int fileFD = -1;
	int err;
	char path[PATH_MAX] = {};
	LOOKUP3res *res;

	err = createDirInPath(getLocalMountedPath(), dir, &dirFD1);
	if (err) {
		XCTFail("createDirInPath failed, got: %d", err);
	}

	snprintf(path, sizeof(path), "%s/%s", getLocalMountedPath(), "/dir");
	err = createFileInPath(path, file, &dirFD2, &fileFD);
	if (err) {
		XCTFail("createFileInPath failed, got %d", err);
		return;
	}

	err = fchmod(dirFD1, 0);
	if (err) {
		XCTFail("fchmod failed, got: %d", err);
	}

	res = doLookupRPC(nclnt, &rootfh, dir);
	if (res->status != NFS3_OK) {
		XCTFail("doLookupRPC failed, got %d", res->status);
	}

	err = switchToUnknownUserAndGroup(dirFD1);
	if (err != 0) {
		XCTFail("switchToUnknownUserAndGroup failed %d", err);
	}

	res = doLookupRPC(nclnt, &res->LOOKUP3res_u.resok.object, file);
	if (res->status != NFS3ERR_ACCES) {
		XCTFail("doLookupRPC failed, expected %d, got %d", NFS3ERR_ACCES, res->status);
	}

	err = switchToRoot();
	if (err != 0) {
		XCTFail("switchToRoot failed %d", err);
	}

	removeFromPath(file, dirFD2, -1, REMOVE_FILE);
	removeFromPath(dir, dirFD1, -1, REMOVE_DIR);
}

/*
 * 1. Create a dir
 * 2. Create a symlink to the dir
 * 3. Create a file in the new dir
 * 4. set dir access mode as 0
 * 5. do LOOKUP call on the symlink to find the file
 * 6. expect NFS3ERR_NOTDIR
 */
- (void)testNFSLookupSymlink
{
	char *dir = "dir";
	char *file = "file";
	char *link = "link";
	int dirFD1 = -1, dirFD2 = -1, dirFD3 = -1;
	int fileFD = -1;
	int err;
	char path[PATH_MAX] = {};
	LOOKUP3res *res;

	err = createDirInPath(getLocalMountedPath(), dir, &dirFD1);
	if (err) {
		XCTFail("createDirInPath failed, got: %d", err);
	}

	err = createSymlinkInPath(getLocalMountedPath(), dir, link, &dirFD2);
	if (err) {
		XCTFail("createSymlinkInPath failed, got %d", err);
	}

	snprintf(path, sizeof(path), "%s/%s", getLocalMountedPath(), "/dir");
	err = createFileInPath(path, file, &dirFD3, &fileFD);
	if (err) {
		XCTFail("createFileInPath failed, got %d", err);
	}

	res = doLookupRPC(nclnt, &rootfh, link);
	if (res->status != NFS3_OK) {
		XCTFail("doLookupRPC failed, got %d", res->status);
		goto bad;
	}

	res = doLookupRPC(nclnt, &res->LOOKUP3res_u.resok.object, file);
	if (res->status != NFS3ERR_NOTDIR) {
		XCTFail("doLookupRPC failed, expected %d, got %d", NFS3ERR_NOTDIR, res->status);
	}
bad:
	removeFromPath(file, dirFD3, fileFD, REMOVE_FILE);
	removeFromPath(link, dirFD2, -1, REMOVE_FILE);
	removeFromPath(dir, dirFD1, -1, REMOVE_DIR);
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

	err = switchToUnknownUserAndGroup(fileFD);
	if (err != 0) {
		XCTFail("switchToUnknownUserAndGroup failed %d", err);
	}

	checkAccess(&res->LOOKUP3res_u.resok.object, fileFD, S_IRUSR, NFS_ACCESS_READ, NFS_ACCESS_READ);
	checkAccess(&res->LOOKUP3res_u.resok.object, fileFD, S_IWUSR, NFS_ACCESS_MODIFY, NFS_ACCESS_MODIFY);
	checkAccess(&res->LOOKUP3res_u.resok.object, fileFD, S_IXUSR, NFS_ACCESS_EXECUTE, NFS_ACCESS_EXECUTE);
	checkAccess(&res->LOOKUP3res_u.resok.object, fileFD, S_IWUSR | S_IXUSR, NFS_ACCESS_READ, 0);
	checkAccess(&res->LOOKUP3res_u.resok.object, fileFD, S_IRUSR | S_IXUSR, NFS_ACCESS_MODIFY, 0);
	checkAccess(&res->LOOKUP3res_u.resok.object, fileFD, S_IRUSR | S_IWUSR, NFS_ACCESS_EXECUTE, 0);

	err = switchToRoot();
	if (err != 0) {
		XCTFail("switchToRoot failed %d", err);
	}

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
	int dirFD = -1, dirFD2 = -1, fileFD = -1, err;
	char *file = "new_file";
	char *link = "new_link";

	err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
	if (err) {
		XCTFail("createFileInPath failed, got %d", err);
		return;
	}

	err = createSymlinkInPath(getLocalMountedPath(), file, link, &dirFD2);
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

	removeFromPath(link, dirFD2, -1, REMOVE_FILE);
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
	if ((fh = doMountAndVerify(getLocalMountedReadOnlyPath(), "sys:krb5")) == NULL) {
		XCTFail("doMountAndVerify failed");
		return;
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
		goto out;
	}

	// Write 5 bytes to the file
	res2 = doWriteRPC(nclnt, &res->LOOKUP3res_u.resok.object, 0, 5, UNSTABLE, sizeof(buf), buf);
	if (res2->status != NFS3ERR_ROFS) {
		XCTFail("doWriteRPC failed, expected status is %d, got %d", NFS3ERR_ROFS, res2->status);
	}

out:
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
	char buf[NAME_MAX] = {};

	err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
	if (err) {
		XCTFail("createFileInPath failed, got %d", err);
		return;
	}

	res = doLookupRPC(nclnt, &rootfh, file);
	if (res->status != NFS3_OK) {
		XCTFail("doLookupRPC failed, got %d", res->status);
	}

	res2 = doWriteRPC(nclnt, &res->LOOKUP3res_u.resok.object, offset, count, UNSTABLE, count, buf);
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

	res2 = doWriteRPC(nclnt, &res->LOOKUP3res_u.resok.object, offset, count, DATA_SYNC, count, buf);
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
 * call WRITE with a stale file handle, NFS3ERR_STALE expected
 */
- (void)testNFSWriteStale
{
	char buffer[8] = { 0, 1, 2, 3, 4, 5, 6, 7};
	nfs_fh3 fh = { .data.data_len = 8, .data.data_val = buffer };
	char buf[10] = "1234567890";
	int count = 1;
	WRITE3res *res;

	res = doWriteRPC(nclnt, &fh, 0, count, FILE_SYNC, sizeof(buf), buf);
	if (res->status != NFS3ERR_STALE) {
		XCTFail("doWriteRPC failed, expected status is %d, got %d", NFS3ERR_STALE, res->status);
	}
}

/*
 * 1. Send unchecked CREATE request using mode attribute
 * 2. Verify mode attribute returned by CREATE
 * 3. call stat() - verify gid/uid/inode attributes returned by CREATE
 */
- (void)testNFSCreate
{
	CREATE3res *res;
	char *file = "new_file";
	struct createhow3 how = {};
	mode_t mode;
	int err;
	struct stat stats = {};
	fattr3 *after;
	char path[PATH_MAX];

	how.mode = UNCHECKED;
	how.createhow3_u.obj_attributes.mode.set_it = TRUE;
	how.createhow3_u.obj_attributes.mode.set_mode3_u.mode = mode = 777;

	res = doCreateRPC(nclnt, &rootfh, file, &how);
	if (res->status != NFS3_OK) {
		XCTFail("doCreateRPC failed, got %d", res->status);
	}
	after = &res->CREATE3res_u.resok.obj_attributes.post_op_attr_u.attributes;

	XCTAssertEqual(mode, after->mode);

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
 * 1. Send unchecked CREATE request using mode attribute
 * 2. Verify mode attribute returned by CREATE
 * 3. Send unchecked CREATE request again using mode
 */
- (void)testNFSCreateUnchecked
{
	CREATE3res *res;
	char *file = "new_file";
	struct createhow3 how = {};
	mode_t mode;
	int err;
	fattr3 *after;
	char path[PATH_MAX];

	how.mode = UNCHECKED;
	how.createhow3_u.obj_attributes.mode.set_it = TRUE;
	how.createhow3_u.obj_attributes.mode.set_mode3_u.mode = mode = 777;

	res = doCreateRPC(nclnt, &rootfh, file, &how);
	if (res->status != NFS3_OK) {
		XCTFail("doCreateRPC failed, got %d", res->status);
	}
	after = &res->CREATE3res_u.resok.obj_attributes.post_op_attr_u.attributes;

	XCTAssertEqual(mode, after->mode);

	how.createhow3_u.obj_attributes.mode.set_it = TRUE;
	how.createhow3_u.obj_attributes.mode.set_mode3_u.mode = 666;

	res = doCreateRPC(nclnt, &rootfh, file, &how);
	if (res->status != NFS3_OK) {
		XCTFail("doCreateRPC failed, got %d", res->status);
	}
	after = &res->CREATE3res_u.resok.obj_attributes.post_op_attr_u.attributes;

	XCTAssertEqual(mode, after->mode);

	snprintf(path, sizeof(path), "%s/%s", getLocalMountedPath(), file);
	err = unlink(path);
	if (err) {
		XCTFail("unlink failed, got %d", err);
	}
}

/*
 * 1. Send guarded CREATE request using mode attribute
 * 2. Verify mode attribute returned by CREATE
 * 3. Send guarded CREATE request again using mode. NFS3ERR_EXIST is expected
 */
- (void)testNFSCreateGuarded
{
	CREATE3res *res;
	char *file = "new_file";
	struct createhow3 how = {};
	mode_t mode;
	int err;
	fattr3 *after;
	char path[PATH_MAX];

	how.mode = GUARDED;
	how.createhow3_u.obj_attributes.mode.set_it = TRUE;
	how.createhow3_u.obj_attributes.mode.set_mode3_u.mode = mode = 777;

	res = doCreateRPC(nclnt, &rootfh, file, &how);
	if (res->status != NFS3_OK) {
		XCTFail("doCreateRPC failed, got %d", res->status);
	}
	after = &res->CREATE3res_u.resok.obj_attributes.post_op_attr_u.attributes;

	XCTAssertEqual(mode, after->mode);

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
 * 1. Send exclusive CREATE request with verf as an all 0 array
 * 2. Send the same request again, expect NFS3_OK
 * 3. Send the same request with different verf, expect NFS3ERR_EXIST
 */
- (void)testNFSCreateExclusive
{
	CREATE3res *res;
	char *file = "new_file";
	struct createhow3 how = {};
	int err;
	char path[PATH_MAX];

	how.mode = EXCLUSIVE;
	memset(how.createhow3_u.verf, 0, sizeof(how.createhow3_u.verf));

	res = doCreateRPC(nclnt, &rootfh, file, &how);
	if (res->status != NFS3_OK) {
		XCTFail("doCreateRPC failed, got %d", res->status);
	}

	res = doCreateRPC(nclnt, &rootfh, file, &how);
	if (res->status != NFS3_OK) {
		XCTFail("doCreateRPC failed, got %d", res->status);
	}

	memset(how.createhow3_u.verf, 1, sizeof(how.createhow3_u.verf));
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
 * call CREATE with a stale file handle, NFS3ERR_STALE expected
 */
- (void)testNFSCreateStale
{
	struct createhow3 how = {};
	CREATE3res *res;

	char buffer[8] = { 0, 1, 2, 3, 4, 5, 6, 7};
	nfs_fh3 fh = { .data.data_len = 8, .data.data_val = buffer };
	char *file = "file";

	res = doCreateRPC(nclnt, &fh, file, &how);
	if (res->status != NFS3ERR_STALE) {
		XCTFail("doCreateRPC failed, expected status is %d, got %d", NFS3ERR_STALE, res->status);
	}
}

/*
 * 1. Send MKDIR request using mode
 * 2. Verify mode attribute returned by MKDIR
 * 3. Call stat() - verify mode attributes returned by MKDIR
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
	mode_t mode;

	// Expected results
	attributes.mode.set_it = YES;
	attributes.mode.set_mode3_u.mode = mode = S_IRUSR;

	// Test mkdir
	res = doMkdirRPC(nclnt, &rootfh, dir, &attributes);
	if (res->status != NFS3_OK) {
		XCTFail("doMkdirRPC failed, got %d", res->status);
	}
	after = &res->MKDIR3res_u.resok.obj_attributes.post_op_attr_u.attributes;

	XCTAssertEqual(mode, after->mode);

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

	XCTAssertEqual(mode, stat.st_mode & 07777);

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
 * 1. Send MKDIR request with stale handle
 * 2. Expect NFS3ERR_STALE
 */
- (void)testNFSMKDirStale
{
	MKDIR3res *res;
	char *dir = "new_dir";
	sattr3 attributes = {};

	char buffer[8] = { 0, 1, 2, 3, 4, 5, 6, 7};
	nfs_fh3 fh = { .data.data_len = 8, .data.data_val = buffer };

	res = doMkdirRPC(nclnt, &fh, dir, &attributes);
	if (res->status != NFS3ERR_STALE) {
		XCTFail("doMkdirRPC failed, expected status is %d, got %d", NFS3ERR_STALE, res->status);
	}
}

/*
 * 1. Send SYMLINK request using mode attribute
 * 2. Verify mode attribute returned by SYMLINK
 */
- (void)testNFSSymlink
{
	SYMLINK3res *res;
	char *symlink = "new_symlink";
	char *data = "/tmp/symlink_data";
	sattr3 attributes = {};
	mode_t mode;
	int err;
	char path[PATH_MAX];

	attributes.mode.set_it = TRUE;
	attributes.mode.set_mode3_u.mode = mode = 777;

	res = doSymlinkRPC(nclnt, &rootfh, symlink, &attributes, data);
	if (res->status != NFS3_OK) {
		XCTFail("doSymlinkRPC failed, got %d", res->status);
	}

	XCTAssertEqual(res->SYMLINK3res_u.resok.obj_attributes.post_op_attr_u.attributes.mode, mode);

	snprintf(path, sizeof(path), "%s/%s", getLocalMountedPath(), symlink);
	err = unlink(path);
	if (err) {
		XCTFail("unlink failed, got %d", err);
	}
}

/*
 * 1. Send SYMLINK request with stale handle
 * 2. expect NFS3ERR_STALE
 */
- (void)testNFSSymlinkStale
{
	SYMLINK3res *res;
	char *symlink = "new_symlink";
	char *data = "/tmp/symlink_data";
	sattr3 attributes = {};
	char buffer[8] = { 0, 1, 2, 3, 4, 5, 6, 7};
	nfs_fh3 fh = { .data.data_len = 8, .data.data_val = buffer };

	res = doSymlinkRPC(nclnt, &fh, symlink, &attributes, data);
	if (res->status != NFS3ERR_STALE) {
		XCTFail("doSymlinkRPC failed, expected status is %d, got %d", NFS3ERR_STALE, res->status);
	}
}

/*
 * 1. Create a file
 * 2. call doLookupRPC to get the file's file handle
 * 3. call doSymlinkRPC with the file handle as the target directory
 * 4. expect NFS3ERR_NOTDIR
 */
- (void)testNFSSymlinkNotDir
{
	LOOKUP3res *res;
	SYMLINK3res *res2;
	char *file = "new_file";
	char *symlink = "link";
	sattr3 attributes = {};
	char *data = "/tmp/symlink_data";
	int dirFD, fileFD, err;

	err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
	if (err) {
		XCTFail("createFileInPath failed, got %d", err);
		return;
	}

	res = doLookupRPC(nclnt, &rootfh, file);
	if (res->status != NFS3_OK) {
		XCTFail("doLookupRPC failed, got %d", res->status);
	}

	res2 = doSymlinkRPC(nclnt, &res->LOOKUP3res_u.resok.object, symlink, &attributes, data);
	if (res2->status != NFS3ERR_NOTDIR) {
		XCTFail("doSymlinkRPC failed, expected status is %d, got %d", NFS3ERR_NOTDIR, res2->status);
	}
	removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
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
 * 1. Send MKNOD request using NF3DIR type. NFS3ERR_BADTYPE is expected
 */
- (void)testNFSMKNodDir
{
	MKNOD3res *res;
	char *file = "new_file";
	struct mknoddata3 data = {};

	data.type = NF3DIR;

	res = doMknodRPC(nclnt, &rootfh, file, &data);
	if (res->status != NFS3ERR_BADTYPE) {
		XCTFail("doMkdirRPC failed, expected status is %d, got %d", NFS3ERR_BADTYPE, res->status);
	}
}

/*
 * 1. Send MKNOD request using NF3LNK type. NFS3ERR_BADTYPE is expected
 */
- (void)testNFSMKNodLnk
{
	MKNOD3res *res;
	char *file = "new_file";
	struct mknoddata3 data = {};

	data.type = NF3LNK;

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

	err = createFileInPath(getLocalMountedPath(), sock, &dirFD, &fileFD);
	if (err) {
		XCTFail("createFileInPath failed, got %d", err);
		return;
	}

	data.type = NF3SOCK;
	data.mknoddata3_u.pipe_attributes.uid.set_it = TRUE;
	data.mknoddata3_u.pipe_attributes.uid.set_uid3_u.uid = 1;
	data.mknoddata3_u.pipe_attributes.gid.set_it = TRUE;
	data.mknoddata3_u.pipe_attributes.gid.set_gid3_u.gid = 2;

	res = doMknodRPC(nclnt, &rootfh, sock, &data);
	if (res->status != NFS3ERR_EXIST) {
		XCTFail("doMkdirRPC failed, expected status is %d, got %d", NFS3ERR_EXIST, res->status);
	}

	removeFromPath(sock, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Send MKNOD request for a socket using mode attribute
 * 2. Verify mode/type attributes returned by MKNOD
 */
- (void)testNFSMKNodSocket
{
	MKNOD3res *res;
	struct mknoddata3 data = {};
	char *sock = "new_sock";
	ftype3 type;
	mode_t mode;
	int err;
	char path[PATH_MAX];

	data.type = type = NF3SOCK;
	data.mknoddata3_u.pipe_attributes.mode.set_it = TRUE;
	data.mknoddata3_u.pipe_attributes.mode.set_mode3_u.mode = mode = 777;

	res = doMknodRPC(nclnt, &rootfh, sock, &data);
	if (res->status != NFS3_OK) {
		XCTFail("doMknodRPC failed, got %d", res->status);
	}

	XCTAssertEqual(res->MKNOD3res_u.resok.obj_attributes.post_op_attr_u.attributes.mode, mode);
	XCTAssertEqual(res->MKNOD3res_u.resok.obj_attributes.post_op_attr_u.attributes.type, type);

	snprintf(path, sizeof(path), "%s/%s", getLocalMountedPath(), sock);
	err = unlink(path);
	if (err) {
		XCTFail("unlink failed, got %d", err);
	}
}

/*
 * 1. Send MKNOD request for a FIFO using mode attribute
 * 2. Verify mode/type attributes returned by MKNOD
 */
- (void)testNFSMKNodSocketFIFO
{
	MKNOD3res *res;
	struct mknoddata3 data = {};
	char *fifo = "new_fifo";
	ftype3 type;
	mode_t mode;
	int err;
	char path[PATH_MAX];

	data.type = type = NF3FIFO;
	data.mknoddata3_u.pipe_attributes.mode.set_it = TRUE;
	data.mknoddata3_u.pipe_attributes.mode.set_mode3_u.mode = mode = 777;

	res = doMknodRPC(nclnt, &rootfh, fifo, &data);
	if (res->status != NFS3_OK) {
		XCTFail("doMknodRPC failed, got %d", res->status);
	}

	XCTAssertEqual(res->MKNOD3res_u.resok.obj_attributes.post_op_attr_u.attributes.mode, mode);
	XCTAssertEqual(res->MKNOD3res_u.resok.obj_attributes.post_op_attr_u.attributes.type, type);

	snprintf(path, sizeof(path), "%s/%s", getLocalMountedPath(), fifo);
	err = unlink(path);
	if (err) {
		XCTFail("unlink failed, got %d", err);
	}
}

/*
 * 1. Send MKNOD request for a char device using mode attribute
 * 2. Verify mode/rdev/type attributes returned by MKNOD
 */
- (void)testNFSMKNodChar
{
	MKNOD3res *res;
	struct mknoddata3 data = {};
	char *dev = "new_char";
	uint32_t specdata1, specdata2;
	ftype3 type;
	mode_t mode;
	int err;
	char path[PATH_MAX];

	data.type = type = NF3CHR;
	data.mknoddata3_u.pipe_attributes.mode.set_it = TRUE;
	data.mknoddata3_u.pipe_attributes.mode.set_mode3_u.mode = mode = 777;
	data.mknoddata3_u.device.spec.specdata1 = specdata1 = 99;
	data.mknoddata3_u.device.spec.specdata2 = specdata2 = 88;

	res = doMknodRPC(nclnt, &rootfh, dev, &data);
	if (res->status != NFS3_OK) {
		XCTFail("doMknodRPC failed, got %d", res->status);
	}

	XCTAssertEqual(res->MKNOD3res_u.resok.obj_attributes.post_op_attr_u.attributes.mode, mode);
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
 * 1. Send MKNOD request for a blk device using mode attribute
 * 2. Verify mode/rdev/type attributes returned by MKNOD
 */
- (void)testNFSMKNodBLK
{
	MKNOD3res *res;
	struct mknoddata3 data = {};
	char *dev = "new_blk";
	uint32_t specdata1, specdata2;
	ftype3 type;
	mode_t mode;
	int err;
	char path[PATH_MAX];

	data.type = type = NF3BLK;
	data.mknoddata3_u.pipe_attributes.mode.set_it = TRUE;
	data.mknoddata3_u.pipe_attributes.mode.set_mode3_u.mode = mode = 777;
	data.mknoddata3_u.device.spec.specdata1 = specdata1 = 99;
	data.mknoddata3_u.device.spec.specdata2 = specdata2 = 88;

	res = doMknodRPC(nclnt, &rootfh, dev, &data);
	if (res->status != NFS3_OK) {
		XCTFail("doMknodRPC failed, got %d", res->status);
	}

	XCTAssertEqual(res->MKNOD3res_u.resok.obj_attributes.post_op_attr_u.attributes.mode, mode);
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
 * 1. Send MKNOD request with an inavlid type
 * 2. expect NFS3_INVALID
 */
- (void)testNFSMKNodBadType
{
	MKNOD3res *res;
	struct mknoddata3 data = {};
	char *file = "new_file";
	data.type = 0;

	res = doMknodRPC(nclnt, &rootfh, file, &data);
	if (res->status != NFS3ERR_BADTYPE) {
		XCTFail("doMknodRPC failed, expected status is %d, got %d", NFS3ERR_BADTYPE, res->status);
	}
}

/*
 * 1. Send MKNOD request with a file handle instead of directory handle
 * 2. expect NFS3ERR_NOTDIR
 */
- (void)testNFSMKNodNotDir
{
	LOOKUP3res *res;
	MKNOD3res *res2;
	struct mknoddata3 data = {};
	char *file = "new_file";
	int dirFD, fileFD, err;

	err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
	if (err) {
		XCTFail("createFileInPath failed, got %d", err);
		return;
	}

	res = doLookupRPC(nclnt, &rootfh, file);
	if (res->status != NFS3_OK) {
		XCTFail("doLookupRPC failed, got %d", res->status);
	}

	data.type = NF3BLK;

	res2 = doMknodRPC(nclnt, &res->LOOKUP3res_u.resok.object, "file", &data);
	if (res2->status != NFS3ERR_NOTDIR) {
		XCTFail("doMknodRPC failed, expected status is %d, got %d", NFS3ERR_NOTDIR, res->status);
	}
	removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Send MKNOD request with a stale file handle
 * 2. expect NFS3ERR_NOTDIR
 */
- (void)testNFSMKNodStale
{
	MKNOD3res *res;
	struct mknoddata3 data = {};
	char buffer[8] = { 0, 1, 2, 3, 4, 5, 6, 7};
	nfs_fh3 fh = { .data.data_len = 8, .data.data_val = buffer };

	data.type = NF3BLK;

	res = doMknodRPC(nclnt, &fh, "file", &data);
	if (res->status != NFS3ERR_STALE) {
		XCTFail("doMknodRPC failed, expected status is %d, got %d", NFS3ERR_STALE, res->status);
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
	if (res->status != NFS3ERR_ISDIR) {
		XCTFail("doRemoveRPC failed, expected status is %d, got %d", NFS3ERR_ISDIR, res->status);
	}

	removeFromPath(dir, dirFD, -1, REMOVE_DIR);
}

/*
 * 1. Send REMOVE request with a stale file handle
 * 2. expect NFS3ERR_STALE
 */
- (void)testNFSRemoveStale
{
	REMOVE3res *res;
	char buffer[8] = { 0, 1, 2, 3, 4, 5, 6, 7};
	nfs_fh3 fh = { .data.data_len = 8, .data.data_val = buffer };
	char *file = "file";

	res = doRemoveRPC(nclnt, &fh, file);
	if (res->status != NFS3ERR_STALE) {
		XCTFail("doRemoveRPC failed, expected status is %d, got %d", NFS3ERR_STALE, res->status);
	}
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
		XCTFail("doLookupRPC failed, expected status is %d, got %d", NFS3ERR_NOENT, res->status);
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
	int dirFD, dirFD2 = -1, fileFD = -1, err;
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
 * 1. Send RMDIR request for "..". should fail with NFS3ERR_INVAL
 */
- (void)testNFSRMDirDotDot
{
	RMDIR3res *res;
	char *file = "..";

	res = doRMDirRPC(nclnt, &rootfh, file);
	if (res->status != NFS3ERR_INVAL) {
		XCTFail("doRMDirRPC failed, expected status is %d, got %d", NFS3ERR_INVAL, res->status);
	}
}

/*
 * 1. Send RMDIR request for a directory that doesn't exist
 * 2. expect NFS3ERR_NOENT
 */
- (void)testNFSRMDirNoEnt
{
	RMDIR3res *res;
	char *dir = "new_dir";

	res = doRMDirRPC(nclnt, &rootfh, dir);
	if (res->status != NFS3ERR_NOENT) {
		XCTFail("doRMDirRPC failed, expected status is %d, got %d", NFS3ERR_NOENT, res->status);
	}
}

/*
 * 1. Send RMDIR request with a stale file handle
 * 2. expect NFS3ERR_STALE
 */
- (void)testNFSRMDIRStale
{
	RMDIR3res *res;
	char buffer[8] = { 0, 1, 2, 3, 4, 5, 6, 7};
	nfs_fh3 fh = { .data.data_len = 8, .data.data_val = buffer };
	char *file = "file";

	res = doRMDirRPC(nclnt, &fh, file);
	if (res->status != NFS3ERR_STALE) {
		XCTFail("doRMDirRPC failed, expected status is %d, got %d", NFS3ERR_STALE, res->status);
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
 * 2. Send RENAME of the file to the same name
 * 3. expect NFS3_OK
 */
- (void)testNFSRenameSameName
{
	RENAME3res *res;
	int dirFD, fileFD, err;
	char *file = "file";

	err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
	if (err) {
		XCTFail("createFileInPath failed, got %d", err);
		return;
	}

	res = doRenameRPC(nclnt, &rootfh, file, &rootfh, file);
	if (res->status != NFS3_OK) {
		XCTFail("doRenameRPC failed, got %d", res->status );
	}

	removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Send a rename request for a file that doesn't exist, NFS3ERR_NOENT expected
 */
- (void)testNFSRenameNoEntry
{
	RENAME3res *res;
	char *file = "file";

	res = doRenameRPC(nclnt, &rootfh, file, &rootfh, file);
	if (res->status != NFS3ERR_NOENT) {
		XCTFail("doRenameRPC failed, got %d", res->status);
	}
}

/*
 * 1. Create 2 new file
 * 2. Send RENAME request to one file, with the second file as the target dir
 * 3. expect NFS3ERR_NOTDIR
 */
- (void)testNFSRenameIntoFile
{
	LOOKUP3res *res;
	RENAME3res *res2;
	int dirFD, fileFD, dirFD2, fileFD2, err;
	char *file1 = "new_file1", *file2 = "new_file2";

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

	res2 = doRenameRPC(nclnt, &rootfh, file2, &res->LOOKUP3res_u.resok.object, file2);
	if (res2->status != NFS3ERR_NOTDIR) {
		XCTFail("doRenameRPC failed, expected status is %d, got %d", NFS3ERR_NOTDIR, res2->status);
	}
	removeFromPath(file1, dirFD, fileFD, REMOVE_FILE);
	removeFromPath(file2, dirFD2, fileFD2, REMOVE_FILE);
}


/*
 * 1. Send RENAME request with a stale file handle as source/dest dir
 * 2. expect NFS3ERR_STALE
 * note: a file is created so NFS3ERR_NOENT won't be returned on the second call to doRenameRPC
 */
- (void)testNFSRenameStale
{
	RENAME3res *res;
	char buffer[8] = { 0, 1, 2, 3, 4, 5, 6, 7};
	nfs_fh3 fh = { .data.data_len = 8, .data.data_val = buffer };
	char *file = "file";
	char *file2 = "new_file";
	int err, dirFD, fileFD;

	err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
	if (err) {
		XCTFail("createFileInPath failed, got %d", err);
		return;
	}

	res = doRenameRPC(nclnt, &fh, file, &rootfh, file2);
	if (res->status != NFS3ERR_STALE) {
		XCTFail("doRenameRPC failed, expected status is %d, got %d", NFS3ERR_STALE, res->status);
	}

	res = doRenameRPC(nclnt, &rootfh, file, &fh, file2);
	if (res->status != NFS3ERR_STALE) {
		XCTFail("doRenameRPC failed, expected status is %d, got %d", NFS3ERR_STALE, res->status);
	}

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
	if (res->status != NFS3ERR_ISDIR) {
		XCTFail("doLinkRPC failed, expected status is %d, got %d", NFS3ERR_ISDIR, res->status);
	}
}

/*
 * 1. Send LINK to a file handle that doesn't exist. NFS3ERR_STALE is expected
 */
- (void)testNFSLinkStaleFile
{
	LINK3res *res;
	char *link = "new_link";
	char buffer[8] = { 0, 1, 2, 3, 4, 5, 6, 7};
	nfs_fh3 fh = { .data.data_len = 8, .data.data_val = buffer };

	res = doLinkRPC(nclnt, &fh, &rootfh, link);
	if (res->status != NFS3ERR_STALE) {
		XCTFail("doLinkRPC failed, expected status is %d, got %d", NFS3ERR_STALE, res->status);
	}
}

/*
 * 1. Send LINK to a file with target directory being a stale file handle
 * 2. NFS3ERR_STALE is expected
 */
- (void)testNFSLinkStaleDir
{
	LOOKUP3res *res;
	LINK3res *res2;
	int dirFD, fileFD, err;
	char *file = "new_file", *link = "new_link";
	char buffer[8] = { 0, 1, 2, 3, 4, 5, 6, 7};
	nfs_fh3 dirh = { .data.data_len = 8, .data.data_val = buffer };

	err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
	if (err) {
		XCTFail("createFileInPath failed, got %d", err);
		return;
	}

	res = doLookupRPC(nclnt, &rootfh, file);
	if (res->status != NFS3_OK) {
		XCTFail("doLookupRPC failed, got %d", res->status);
	}

	res2 = doLinkRPC(nclnt, &res->LOOKUP3res_u.resok.object, &dirh, link);
	if (res2->status != NFS3ERR_STALE) {
		XCTFail("doLinkRPC failed, expected status is %d, got %d", NFS3ERR_STALE, res->status);
	}

	removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Send LINK to a file in the same directory with the same name
 * 2. NFS3ERR_EXIST is expected
 */
- (void)testNFSLinkSameFile
{
	LOOKUP3res *res;
	LINK3res *res2;
	int dirFD, fileFD, err;
	char *file = "new_file";
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

	res2 = doLinkRPC(nclnt, &res->LOOKUP3res_u.resok.object, &rootfh, file);
	if (res2->status != NFS3ERR_EXIST) {
		XCTFail("doLinkRPC failed, expected status is %d, got %d", NFS3ERR_EXIST, res->status);
	}

	removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Send LINK to a file with a name of a file that exists
 * 2. NFS3ERR_EXIST is expected
 */
- (void)testNFSLinkFileExists
{
	LOOKUP3res *res;
	LINK3res *res2;
	int dirFD = -1, dirFD2 = -1, fileFD = -1, fileFD2 = -1, err;
	char *file = "new_file", *link = "new_link";

	err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
	if (err) {
		XCTFail("createFileInPath failed, got %d", err);
		return;
	}
	err = createFileInPath(getLocalMountedPath(), link, &dirFD2, &fileFD2);
	if (err) {
		XCTFail("createFileInPath failed, got %d", err);
		goto out;
	}

	res = doLookupRPC(nclnt, &rootfh, file);
	if (res->status != NFS3_OK) {
		XCTFail("doLookupRPC failed, got %d", res->status);
		goto out;
	}

	res2 = doLinkRPC(nclnt, &res->LOOKUP3res_u.resok.object, &rootfh, link);
	if (res2->status != NFS3ERR_EXIST) {
		XCTFail("doLinkRPC failed, expected status is %d, got %d", NFS3ERR_EXIST, res->status);
		goto out;
	}

out:
	removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
	removeFromPath(link, dirFD2, fileFD2, REMOVE_FILE);
}

/*
 * 1. Send LINK to a file with a name longer than pathconf.name_max
 * 2. if pathconf.no_trunc is true, expect NFS3ERR_NAMETOOLONG, else expect NFS3_OK
 */
- (void)testNFSLinkNameTooLong
{
	PATHCONF3res *res;
	LOOKUP3res *res2;
	LINK3res *res3;
	uint32_t name_max;

	int dirFD, fileFD = -1, err;
	char *file = "new_file";

	res = doPathconfRPC(nclnt, &rootfh);
	if (res->status != NFS3_OK) {
		XCTFail("doPathconfRPC failed, got %d", res->status);
		return;
	}

	name_max = res->PATHCONF3res_u.resok.name_max;
	char name[name_max + 2];
	memset(name, 'a', name_max + 1);
	name[name_max + 1] = '\0';

	err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
	if (err) {
		XCTFail("createFileInPath failed, got %d", err);
		return;
	}

	res2 = doLookupRPC(nclnt, &rootfh, file);
	if (res->status != NFS3_OK) {
		XCTFail("doLookupRPC failed, got %d", res->status);
		goto out;
	}

	res3 = doLinkRPC(nclnt, &res2->LOOKUP3res_u.resok.object, &rootfh, name);
	if (res->PATHCONF3res_u.resok.no_trunc) {
		if (res3->status != NFS3ERR_NAMETOOLONG) {
			XCTFail("doLinkRPC failed, expected status is %d, got %d", NFS3ERR_NAMETOOLONG, res3->status);
		}
	} else {
		if (res3->status != NFS3_OK) {
			XCTFail("doLookupRPC failed, got %d", res3->status);
			goto out;
		}
		name[name_max] = '\0';
		err = unlinkat(dirFD, name, 0);
		if (err) {
			XCTFail("unlinkat failed, got %d", err);
			goto out;
		}
	}

	name[name_max] = '\0'; // the longest possible name
	res3 = doLinkRPC(nclnt, &res2->LOOKUP3res_u.resok.object, &rootfh, name);

	if (res3->status != NFS3_OK) {
		XCTFail("doLookupRPC failed, got %d", res->status);
		goto out;
	}

	err = unlinkat(dirFD, name, 0);
	if (err) {
		XCTFail("unlinkat failed, got %d", err);
	}

out:
	removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. create file
 * 2. send LINK request to the file pathconf.linkmax times, expect NFS3_OK
 */
- (void)testNFSLinkNameTooManyLinks
{
	PATHCONF3res *res;
	LOOKUP3res *res2;
	LINK3res *res3;
	uint32_t linkmax;

	int dirFD, fileFD = -1, err;
	char *file = "new_file";
	char const_link[] = "link_XXXXXXX";
	char *link = NULL;

	res = doPathconfRPC(nclnt, &rootfh);
	if (res->status != NFS3_OK) {
		XCTFail("doPathconfRPC failed, got %d", res->status);
		return;
	}

	linkmax = res->PATHCONF3res_u.resok.linkmax;

	err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
	if (err) {
		XCTFail("createFileInPath failed, got %d", err);
		return;
	}

	res2 = doLookupRPC(nclnt, &rootfh, file);
	if (res->status != NFS3_OK) {
		XCTFail("doLookupRPC failed, got %d", res->status);
		goto out;
	}

	link = malloc(sizeof(const_link));
	memcpy(link, const_link, sizeof(const_link));

	for (int i = 1; i <= linkmax; i++) {
		sprintf(link + 5, "%06d", i);
		res3 = doLinkRPC(nclnt, &res2->LOOKUP3res_u.resok.object, &rootfh, link);
		if (res->status != NFS3_OK) {
			XCTFail("doLinkRPC failed, got %d", res3->status);
			goto out;
		}
	}

/* APFS supports 2^64 files per directory and 2^64 hardlinks,
 * but stat(2) only supports reporting LINK_MAX (32767)
 */
//    res3 = doLinkRPC(nclnt, &res2->LOOKUP3res_u.resok.object, &rootfh, "extralLink");
//    if (res3->status != NFS3ERR_MLINK){
//        XCTFail("doLinkRPC failed, expected status is %d, got %d", NFS3ERR_MLINK, res3->status);
//    }
//    if (res3->status == NFS3_OK){
//        unlinkat(dirFD, "extralLink", 0);
//    }

out:
	for (int i = 1; i <= linkmax; i++) {
		sprintf(link + 5, "%06d", i);
		err = unlinkat(dirFD, link, 0);
		if (err != 0) {
			XCTFail("unlinkat failed, got %d", err);
		}
	}

	removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
	if (link) {
		free(link);
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

	res = doReaddirRPC(nclnt, &fh, cookie, &cookieverf, count);
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
	int fds = 200, err;
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
		XCTFail("doReaddirRPC failed, expected status is %d, got %d", NFS3ERR_BAD_COOKIE, res->status);
	}

out:
	for (int i = 0; i < fds && fileFDs[i] != -1; i++) {
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
	int fds = 200, err;
	int fileFDs[fds], dirFDs[fds];
	READDIR3res *res = NULL;
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
	for (int i = 0; i < fds && fileFDs[i] != -1; i++) {
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
	READDIRPLUS3res *res = NULL;
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
			XCTAssertEqual(memcmp(entry->name_handle.post_op_fh3_u.handle.data.data_val, res2->LOOKUP3res_u.resok.object.data.data_val, res2->LOOKUP3res_u.resok.object.data.data_len), 0);

			cookie = entry->cookie;
			entry = entry->nextentry;
		}

		if (res->READDIRPLUS3res_u.resok.reply.eof) {
			break;
		}
	}

	XCTAssertEqual(cookie, fds + 3);

out:
	for (int i = 0; i < fds && fileFDs[i] != -1; i++) {
		char file[NAME_MAX] = {};
		snprintf(file, sizeof(file), "file_%d", i);
		removeFromPath(file, dirFDs[i], fileFDs[i], REMOVE_FILE);
	}
}

/*
 * 1. Send READDIRPLUS using an empty buffer, NFS3ERR_TOOSMALL is expected
 */
- (void)testNFSReaddirplusTooSmall
{
	READDIRPLUS3res *res;
	count3 count = 0;
	count3 dircount = 1024;
	cookie3 cookie = 0;
	cookieverf3 cookieverf = {};

	res = doReaddirplusRPC(nclnt, &rootfh, cookie, &cookieverf, dircount, count);
	if (res->status != NFS3ERR_TOOSMALL) {
		XCTFail("doReaddirRPC failed, expected status is %d, got %d", NFS3ERR_TOOSMALL, res->status);
	}
}

/*
 * 1. Send READDIRPLUS using non-zero cookie and invalid cookie verifier, NFS3ERR_BAD_COOKIE is expected
 */
- (void)testNFSReaddirplusBadCookie
{
	READDIRPLUS3res *res;
	count3 count = 1000;
	count3 dircount = 1024;
	cookie3 cookie = 4444;
	cookieverf3 cookieverf = { 1, 2, 3, 4, 5, 6, 7, 8 };

	res = doReaddirplusRPC(nclnt, &rootfh, cookie, &cookieverf, dircount, count);
	if (res->status != NFS3ERR_BAD_COOKIE) {
		XCTFail("doReaddirRPC failed, expected status is %d, got %d", NFS3ERR_BAD_COOKIE, res->status);
	}
}

/*
 * 1. Send READDIRPLUS using zero cookie and invalid cookkie verifier, NFS3ERR_BAD_COOKIE is expected
 */
- (void)testNFSReaddirplusBadCookie2
{
	READDIRPLUS3res *res;
	count3 count = 1000;
	count3 dircount = 1024;
	cookie3 cookie = 0;
	cookieverf3 cookieverf = { 1, 2, 3, 4, 5, 6, 7, 8 };

	res = doReaddirplusRPC(nclnt, &rootfh, cookie, &cookieverf, dircount, count);
	if (res->status != NFS3ERR_BAD_COOKIE) {
		XCTFail("doReaddirRPC failed, expected status is %d, got %d", NFS3ERR_BAD_COOKIE, res->status);
	}
}

/*
 * 1. Create 20 files
 * 2. Send READDIRPLUS to obtain the directory listing
 * 3. Send READDIRPLUS to obtain the rest of the directory using an invalid cookie verifier. NFS3ERR_BAD_COOKIE is expected
 */
- (void)testNFSReaddirplusBadCookie3
{
	int fds = 20, err;
	int fileFDs[fds], dirFDs[fds];
	READDIRPLUS3res *res;
	count3 count = 200;
	count3 dircount = 1024;
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

	res = doReaddirplusRPC(nclnt, &rootfh, cookie, &cookieverf, dircount, count);
	if (res->status != NFS3_OK) {
		XCTFail("doReaddirRPC failed, got %d", res->status);
		goto out;
	}

	res->READDIRPLUS3res_u.resok.cookieverf[0]++;
	res = doReaddirplusRPC(nclnt, &rootfh, res->READDIRPLUS3res_u.resok.reply.entries->cookie, &res->READDIRPLUS3res_u.resok.cookieverf, dircount, count);
	if (res->status != NFS3ERR_BAD_COOKIE) {
		XCTFail("doReaddirRPC failed, expected status is %d, got %d", NFS3ERR_TOOSMALL, res->status);
	}

out:
	for (int i = 0; i < fds && fileFDs[i] != -1; i++) {
		char file[NAME_MAX] = {};
		snprintf(file, sizeof(file), "file_%d", i);
		removeFromPath(file, dirFDs[i], fileFDs[i], REMOVE_FILE);
	}
}

/*
 * 1. Send READDIRPLUS using STALE file handle, ESTALE is expected
 */
- (void)testNFSReaddirplusStale
{
	READDIRPLUS3res *res;
	count3 count = 300;
	count3 dircount = 1024;
	cookie3 cookie = 0;
	cookieverf3 cookieverf = {};
	char buffer[8] = { 0, 1, 2, 3, 4, 5, 6, 7};
	nfs_fh3 fh = { .data.data_len = 8, .data.data_val = buffer };
	res = doReaddirplusRPC(nclnt, &fh, cookie, &cookieverf, dircount, count);
	if (res->status != NFS3ERR_STALE) {
		XCTFail("doReaddirplusRPC failed, expected status is %d, got %d", NFS3ERR_STALE, res->status);
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
 * 1. send FSINFO request with stale file handle
 * 2. expectNFS3ERR_STALE
 */
- (void)testNFSFSInfoStale
{
	FSINFO3res *res;
	char buffer[8] = { 0, 1, 2, 3, 4, 5, 6, 7};
	nfs_fh3 fh = { .data.data_len = 8, .data.data_val = buffer };

	res = doFSinfoRPC(nclnt, &fh);
	if (res->status != NFS3ERR_STALE) {
		XCTFail("doFSinfoRPC failed, expected status is %d, got %d", NFS3ERR_STALE, res->status);
	}
}

/*
 * helper function calls doPathconfRPC with the given file handle
 * and compares the result to the result of pathconf()
 */
static void
NFSPathconfTestResult(nfs_fh3* fh)
{
	PATHCONF3res *res;

	res = doPathconfRPC(nclnt, fh);
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
 * 1. Send PATHCONF with root file handle
 * 2. Compare results with pathconf() syscall
 */
- (void)testNFSPathconfRoot
{
	NFSPathconfTestResult(&rootfh);
}

/*
 * 1. Create new file
 * 2. Obtain the filehandle using LOOKUP
 * 3. Send Pathconf.
 * 4. Compare results with pathconf() syscall
 */
- (void)testNFSPathconfFile
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
	NFSPathconfTestResult(&res->LOOKUP3res_u.resok.object);

	removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Create new directory
 * 2. Obtain the filehandle using LOOKUP
 * 3. Send Pathconf.
 * 4. Compare results with pathconf() syscall
 */
- (void)testNFSPathconfDir
{
	LOOKUP3res *res;
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

	NFSPathconfTestResult(&res->LOOKUP3res_u.resok.object);

	removeFromPath(dir, dirFD, -1, REMOVE_DIR);
}

/*
 * 1. Send Pathconf. with non-existing file handle
 * 2. expect NFS3ERR_STALE
 */
- (void)testNFSPathconfStale
{
	PATHCONF3res *res;
	char buffer[8] = { 0, 1, 2, 3, 4, 5, 6, 7};
	nfs_fh3 fh = { .data.data_len = 8, .data.data_val = buffer };

	res = doPathconfRPC(nclnt, &fh);
	if (res->status != NFS3ERR_STALE) {
		XCTFail("doPathconfRPC failed, expected status is %d, got %d", NFS3ERR_STALE, res->status);
	}
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
	if (res2->status != NFS3ERR_BADTYPE) {
		XCTFail("doCommitRPC should failed with %d, got %d", NFS3ERR_BADTYPE, res2->status);
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
	if (res3->status != NFS3_OK) {
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

/*
 * 1. send COMMIT request with stale file handle
 * 2. expect NFS3ERR_STALE
 */
- (void)testNFSCommitStale
{
	COMMIT3res *res;
	char buffer[8] = { 0, 1, 2, 3, 4, 5, 6, 7};
	nfs_fh3 fh = { .data.data_len = 8, .data.data_val = buffer };

	res = doCommitRPC(nclnt, &fh, 0, 0);
	if (res->status != NFS3ERR_STALE) {
		XCTFail("doCommitRPC should failed with %d, got %d", NFS3ERR_STALE, res->status);
	}
}

@end

@interface nfsrvTests_nfs_v2 : XCTestCase

@end

@implementation nfsrvTests_nfs_v2

- (void)setUp
{
	/* Enable NFS server debug */
	sysctl_set("vfs.generic.nfs.server.debug_ctl", -1);

	doNFSSetUpWithArgs(NULL, 0, AF_INET, SOCK_STREAM, RPCAUTH_UNIX, CREATE_NFS_V2);
}

- (void)tearDown
{
	memset(&rootfh, 0, sizeof(rootfh));
	doMountTearDown();

	if (nclnt) {
		clnt_destroy(nclnt);
		nclnt = NULL;
	}

	sysctl_set("vfs.generic.nfs.server.debug_ctl", 0);
}

/*
 * 1. Send NULL to the server, make sure we got the reply
 */
- (void)testNFSv2Null
{
	doNullRPC_v2(nclnt);
}

/*
 * 1. Create new file
 * 2. write() to modify its size
 * 3. Call stat() to get file status
 * 3. Obtain the filehandle using LOOKUP
 * 4. Retrieve attributes using GETATTR
 * 5. Compare attributes received by stat() and GETATTR
 */
- (void)testNFSv2GetAttr
{
	fattr *attr;
	diropres *res;
	attrstat *res2;
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

	res = doLookupRPC_v2(nclnt, &rootfh_v2, file);
	if (res->status != NFS_OK) {
		XCTFail("doLookupRPC_v2 failed, got %d", res->status);
	}

	res2 = doGetattrRPC_v2(nclnt, &res->diropres_u.diropres.file);
	if (res2->status != NFS_OK) {
		XCTFail("doGetattrRPC_v2 failed, got %d", res2->status);
	}
	attr = &res2->attrstat_u.attributes;

	// Verify results
	XCTAssertEqual(attr->type, NFREG);
	XCTAssertEqual(attr->mode & 777, stat.st_mode & 777);
	XCTAssertEqual(attr->nlink, stat.st_nlink);
	XCTAssertEqual(attr->uid, stat.st_uid);
	XCTAssertEqual(attr->gid, stat.st_gid);
	XCTAssertEqual(attr->size, stat.st_size);
	// attr->used
	XCTAssertEqual(attr->rdev, stat.st_rdev);
	XCTAssertEqual(attr->fsid, stat.st_dev);
	XCTAssertEqual(attr->fileid, stat.st_ino);
	XCTAssertEqual(attr->atime.seconds, stat.st_atimespec.tv_sec);
	XCTAssertEqual(attr->atime.useconds, stat.st_atimespec.tv_nsec / NSEC_PER_USEC);
	XCTAssertEqual(attr->mtime.seconds, stat.st_mtimespec.tv_sec);
	XCTAssertEqual(attr->mtime.useconds, stat.st_mtimespec.tv_nsec / NSEC_PER_USEC);
	XCTAssertEqual(attr->ctime.seconds, stat.st_ctimespec.tv_sec);
	XCTAssertEqual(attr->ctime.useconds, stat.st_ctimespec.tv_nsec / NSEC_PER_USEC);

	removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Create new file
 * 2. Obtain the filehandle using LOOKUP
 * 3. Retrieve attributes using GETATTR
 * 4. Call SETATTR to modify file mtime
 * 5. Retrieve attributes using GETATTR
 * 6. Verify mtime was updated
 */
- (void)testNFSv2Setattr
{
	sattr attr = {};
	diropres *res;
	attrstat *res2, *res3, *res4;
	int dirFD, fileFD, err;
	char *file = "new_file";

	err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
	if (err) {
		XCTFail("createFileInPath failed, got %d", err);
		return;
	}

	res = doLookupRPC_v2(nclnt, &rootfh_v2, file);
	if (res->status != NFS_OK) {
		XCTFail("doLookupRPC_v2 failed, got %d", res->status);
	}

	res2 = doGetattrRPC_v2(nclnt, &res->diropres_u.diropres.file);
	if (res2->status != NFS_OK) {
		XCTFail("doGetattrRPC_v2 failed, got %d", res2->status);
	}

	attr.mtime.seconds  = 99;
	attr.mtime.useconds = 66;

	res3 = doSetattrRPC_v2(nclnt, &res->diropres_u.diropres.file, &attr);
	if (res3->status != NFS_OK) {
		XCTFail("doSetattrRPC_v2 failed, got %d", res3->status);
	}

	res4 = doGetattrRPC_v2(nclnt, &res->diropres_u.diropres.file);
	if (res4->status != NFS_OK) {
		XCTFail("doGetattrRPC_v2 failed, got %d", res4->status);
	}

	XCTAssertEqual(res4->attrstat_u.attributes.mtime.seconds, attr.mtime.seconds);
	XCTAssertEqual(res4->attrstat_u.attributes.mtime.useconds, attr.mtime.useconds);

	removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Create new file
 * 2. Obtain the filehandle using LOOKUP
 */
- (void)testNFSv2Lookup
{
	diropres *res;
	int dirFD, fileFD, err;
	char *file = "new_file";

	err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
	if (err) {
		XCTFail("createFileInPath failed, got %d", err);
		return;
	}

	res = doLookupRPC_v2(nclnt, &rootfh_v2, file);
	if (res->status != NFS_OK) {
		XCTFail("doLookupRPC_v2 failed, got %d", res->status);
	}

	removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. call LOOKUP with a stale file handle, NFS3ERR_STALE expected
 */
- (void)testNFSv2LookupStale
{
	diropres *res;
	char *file = "new_file";
	nfs_fh fh = { .data = { 0, 1, 2, 3, 4, 5, 6, 7} };

	res = doLookupRPC_v2(nclnt, &fh, file);
	if (res->status != NFSERR_STALE) {
		XCTFail("doLookupRPC_v2 failed, expected status is %d, got %d", NFSERR_STALE, res->status);
	}
}

/*
 * 1. Create new file
 * 2. Create new symlink of the file
 * 3. Call fstatat() to get symlink status
 * 4. Obtain symlink the filehandle using LOOKUP
 * 5. Send READLINK using filehandle of link
 * 6. Verify fileid and link path
 */
- (void)testNFSv2Readlink
{
	diropres *res;
	readlinkres *res2;
	struct stat stat = {};
	int dirFD = -1, dirFD2 = -1, fileFD = -1, err;
	char *file = "new_file";
	char *link = "new_link";

	err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
	if (err) {
		XCTFail("createFileInPath failed, got %d", err);
		return;
	}

	err = createSymlinkInPath(getLocalMountedPath(), file, link, &dirFD2);
	if (err) {
		XCTFail("createSymlinkInPath failed, got %d", err);
	}

	err = fstatat(dirFD2, link, &stat, AT_SYMLINK_NOFOLLOW);
	if (err) {
		XCTFail("fstatat failed, got %d", err);
	}

	res = doLookupRPC_v2(nclnt, &rootfh_v2, link);
	if (res->status != NFS_OK) {
		XCTFail("doLookupRPC_v2 failed, got %d", res->status);
	}

	res2 = doReadlinkRPC_v2(nclnt, &res->diropres_u.diropres.file);
	if (res2->status != NFS_OK) {
		XCTFail("doReadlinkRPC_v2 failed, got %d", res2->status);
	}

	XCTAssertEqual(strncmp(file, res2->readlinkres_u.data, strlen(file)), 0);

	removeFromPath(link, dirFD2, -1, REMOVE_FILE);
	removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Create new file
 * 2. Write to the file
 * 3. Obtain the filehandle using LOOKUP
 * 4. Call READ from offset zero, the same amount of bytes we wrote
 * 5. Compare results
 */
- (void)testNFSv2Read
{
	diropres *res;
	readres *res2;
	ssize_t bytes;
	int dirFD, fileFD, err;
	char *file = "new_file";
	char *string = "1234567890";

	err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
	if (err) {
		XCTFail("createFileInPath failed, got %d", err);
		return;
	}

	bytes = write(fileFD, string, strlen(string));
	if (bytes <= 0) {
		XCTFail("write failed, got %zu, errno %d", bytes, errno);
	}

	res = doLookupRPC_v2(nclnt, &rootfh_v2, file);
	if (res->status != NFS_OK) {
		XCTFail("doLookupRPC_v2 failed, got %d", res->status);
	}

	res2 = doReadRPC_v2(nclnt, &res->diropres_u.diropres.file, 0, (u_int) strlen(string));
	if (res2->status != NFS_OK) {
		XCTFail("doReadRPC_v2 failed, got %d", res2->status);
	}

	XCTAssertEqual(res2->readres_u.reply.data.data_len, strlen(string));
	XCTAssertEqual(memcmp(res2->readres_u.reply.data.data_val, string, strlen(string)), 0);

	removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Create new file
 * 2. Obtain the filehandle using LOOKUP
 * 3. Send data sync WRITE request
 * 4. Verify count and value
 */
- (void)testNFSv2Write
{
#define BUFF_SIZE 10
	diropres *res;
	attrstat *res2;
	int dirFD, fileFD, err, offset = 0;
	char *file = "new_file";
	char buf[BUFF_SIZE] = "1234567890", buf2[BUFF_SIZE];
	size_t bytes;

	err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
	if (err) {
		XCTFail("createFileInPath failed, got %d", err);
		return;
	}

	res = doLookupRPC_v2(nclnt, &rootfh_v2, file);
	if (res->status != NFS_OK) {
		XCTFail("doLookupRPC_v2 failed, got %d", res->status);
	}

	res2 = doWriteRPC_v2(nclnt, &res->diropres_u.diropres.file, offset, BUFF_SIZE, buf);
	if (res2->status != NFS_OK) {
		XCTFail("doWriteRPC_v2 failed, got %d", res2->status);
	}

	if (res2->attrstat_u.attributes.size != BUFF_SIZE) {
		XCTFail("doWriteRPC_v2 count should be %d, got %d", BUFF_SIZE, res2->attrstat_u.attributes.size);
	}

	bytes = read(fileFD, buf2, sizeof(buf2));
	if (bytes <= 0) {
		XCTFail("read failed, got %zu, errno %d", bytes, errno);
	}

	XCTAssertEqual(memcmp(buf, buf2, BUFF_SIZE), 0);

	removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Send unchecked CREATE request using mode attribute
 * 2. Verify mode attribute returned by CREATE
 * 3. call stat() - verify gid/uid/inode attributes returned by CREATE
 */
- (void)testNFSv2Create
{
	diropres *res;
	char *file = "new_file";
	struct fattr *after;
	struct sattr attributes = {};
	mode_t mode;
	int err;
	struct stat stats = {};
	char path[PATH_MAX];

	attributes.mode = mode = 777;

	res = doCreateRPC_v2(nclnt, &rootfh_v2, file, &attributes);
	if (res->status != NFS_OK) {
		XCTFail("doCreateRPC_v2 failed, got %d", res->status);
	}
	after = &res->diropres_u.diropres.attributes;

	XCTAssertEqual(mode, after->mode & 777);

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
 * 1. Send unchecked CREATE request for block device using mode attribute
 * 2. Verify mode attribute returned by CREATE
 * 3. call stat() - verify gid/uid/inode attributes returned by CREATE
 */
- (void)testNFSv2CreateBlock
{
	diropres *res;
	char *file = "new_block";
	struct fattr *after;
	struct sattr attributes = {};
	mode_t mode;
	int err;
	struct stat stats = {};
	char path[PATH_MAX];

	attributes.mode = mode = 777 | S_IFBLK;

	res = doCreateRPC_v2(nclnt, &rootfh_v2, file, &attributes);
	if (res->status != NFS_OK) {
		XCTFail("doCreateRPC_v2 failed, got %d", res->status);
	}
	after = &res->diropres_u.diropres.attributes;

	XCTAssertEqual(mode & 777, after->mode & 777);
	XCTAssertEqual(mode & S_IFMT, after->mode & S_IFMT);

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
 * 1. Create new file
 * 2. Send REMOVE request for the file
 * 3. Send REMOVE request again. NFSERR_NOENT is expected
 */
- (void)testNFSv2Remove
{
	nfsstat *res;
	int dirFD, fileFD, err;
	char *file = "new_file";

	err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
	if (err) {
		XCTFail("createFileInPath failed, got %d", err);
		return;
	}

	res = doRemoveRPC_v2(nclnt, &rootfh_v2, file);
	if (*res != NFS_OK) {
		XCTFail("doRemoveRPC_v2 failed, got %d", *res);
	}

	res = doRemoveRPC_v2(nclnt, &rootfh_v2, file);
	if (*res != NFSERR_NOENT) {
		XCTFail("doRemoveRPC_v2 failed, expected status is %d, got %d", NFSERR_NOENT, *res);
	}

	close(dirFD);
}

/*
 * 1. Create new file
 * 2. Obtain the filehandle using LOOKUP
 * 3. Send RENAME of the file to a different name
 * 4. Send LOOKUP for the primary name, NFSERR_NOENT is expected
 */
- (void)testNFSv2Rename
{
	diropres *res;
	nfsstat *res2;
	int dirFD, fileFD, err;
	char *file1 = "new_file1", *file2 = "new_file2";

	err = createFileInPath(getLocalMountedPath(), file1, &dirFD, &fileFD);
	if (err) {
		XCTFail("createFileInPath failed, got %d", err);
		return;
	}

	res = doLookupRPC_v2(nclnt, &rootfh_v2, file1);
	if (res->status != NFS_OK) {
		XCTFail("doLookupRPC_v2 failed, got %d", res->status);
	}

	res2 = doRenameRPC_v2(nclnt, &rootfh_v2, file1, &rootfh_v2, file2);
	if (*res2 != NFS_OK) {
		XCTFail("doRenameRPC_v2 failed, got %d", *res2);
	}

	res = doLookupRPC_v2(nclnt, &rootfh_v2, file1);
	if (res->status != NFSERR_NOENT) {
		XCTFail("doLookupRPC_v2 failed, expected status is %d, got %d", NFSERR_NOENT, res->status);
	}

	removeFromPath(file2, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Create new file
 * 2. Call stat() verify link count is 1
 * 3. Obtain the filehandle for file using LOOKUP
 * 4. Send LINK using the filehandle obtained by LOOKUP
 * 5. Call stat() verify link count is 2
 */
- (void)testNFSv2Link
{
	diropres *res;
	nfsstat *res2;
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

	res = doLookupRPC_v2(nclnt, &rootfh_v2, file);
	if (res->status != NFS_OK) {
		XCTFail("doLookupRPC_v2 failed, got %d", res->status);
	}

	res2 = doLinkRPC_v2(nclnt, &res->diropres_u.diropres.file, &rootfh_v2, link);
	if (*res2 != NFS_OK) {
		XCTFail("doLinkRPC_v2 failed, got %d", *res2);
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
 * 1. Send SYMLINK request using mode attribute
 * 2. Verify mode attribute using lstat
 */
- (void)testNFSv2Symlink
{
	nfsstat *res;
	char *symlink = "new_symlink";
	char *data = "/tmp/symlink_data";
	sattr attributes = {};
	mode_t mode;
	int err;
	char path[PATH_MAX];
	struct stat stat = {};

	attributes.mode = mode = 777;

	res = doSymlinkRPC_v2(nclnt, &rootfh_v2, symlink, data, &attributes);
	if (*res != NFS_OK) {
		XCTFail("doSymlinkRPC_v2 failed, got %d", *res);
	}

	snprintf(path, sizeof(path), "%s/%s", getLocalMountedPath(), symlink);
	err = lstat(path, &stat);
	if (err) {
		XCTFail("lstat failed, got %d", err);
	}

	XCTAssertEqual(stat.st_mode & 777, mode);

	err = unlink(path);
	if (err) {
		XCTFail("unlink failed, got %d", err);
	}
}

/*
 * 1. Send MKDIR request using mode
 * 2. Verify mode attribute returned by MKDIR
 * 3. Call stat() - verify mode attributes returned by MKDIR
 */
- (void)testNFSv2MKDir
{
	diropres *res;
	fattr *after;
	int dirFD, err;
	char *dir = "new_dir";
	struct stat stat = {};
	sattr attributes = {};
	char dirpath[PATH_MAX];
	mode_t mode;

	// Expected results
	attributes.mode = mode = S_IRUSR;

	// Test mkdir
	res = doMkdirRPC_v2(nclnt, &rootfh_v2, dir, &attributes);
	if (res->status != NFS_OK) {
		XCTFail("doMkdirRPC_v2 failed, got %d", res->status);
	}
	after = &res->diropres_u.diropres.attributes;

	XCTAssertEqual(mode, after->mode & 777);

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

	XCTAssertEqual(mode, stat.st_mode & 777);

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
 * 1. Create new directory
 * 2. Obtain the filehandle using LOOKUP
 * 3. Send RMDIR request for the directory
 * 4. Obtain the filehandle using LOOKUP. should fail with NFSERR_NOENT
 */
- (void)testNFSv2RMDir
{
	diropres *res;
	nfsstat *res2;
	int dirFD, err;
	char *dir = "new_dir";

	err = createDirInPath(getLocalMountedPath(), dir, &dirFD);
	if (err) {
		XCTFail("createDirInPath failed, got %d", err);
		return;
	}

	res = doLookupRPC_v2(nclnt, &rootfh_v2, dir);
	if (res->status != NFS_OK) {
		XCTFail("doLookupRPC_v2 failed, got %d", res->status);
	}

	res2 = doRMDirRPC_v2(nclnt, &rootfh_v2, dir);
	if (*res2 != NFS_OK) {
		XCTFail("doRMDirRPC_v2 failed, got %d", *res2);
	}

	res = doLookupRPC_v2(nclnt, &rootfh_v2, dir);
	if (res->status != NFSERR_NOENT) {
		XCTFail("doLookupRPC_v2 failed, expected status is %d, got %d", NFSERR_NOENT, res->status);
	}

	if (dirFD >= 0) {
		close(dirFD);
	}
}

/*
 * 1. Create 200 files
 * 2. Send READDIR to obtain the directory listing. count the amount of entries
 * 3. Verify inode numbers
 * 4. Verify the total amount of entries received is equal to the amount we created
 */
- (void)testNFSv2Readdir
{
	int fds = 200, err;
	int fileFDs[fds], dirFDs[fds];
	readdirres *res = NULL;
	u_int count = 200;
	nfscookie cookie = {};
	int entries = 0;

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

	while (entries < fds + 3) { // for ".", ".." and "dir3_1"
		res = doReaddirRPC_v2(nclnt, &rootfh_v2, &cookie, count );
		if (res->status != NFS_OK) {
			XCTFail("doReaddirRPC_v2 failed, got %d", res->status);
			goto out;
		}

		entry1 *entry = res->readdirres_u.reply.entries;
		if (entry == NULL) {
			if (res->status != NFS_OK) {
				XCTFail("doReaddirRPC_v2 failed, got NULL entry");
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

			memcpy(cookie, entry->cookie, NFS_COOKIESIZE);
			entry = entry->nextentry;
			entries++;
		}

		if (res->readdirres_u.reply.eof) {
			break;
		}
	}

	XCTAssertEqual(entries, fds + 3);

out:
	for (int i = 0; i < fds && fileFDs[i] != -1; i++) {
		char file[NAME_MAX] = {};
		snprintf(file, sizeof(file), "file_%d", i);
		removeFromPath(file, dirFDs[i], fileFDs[i], REMOVE_FILE);
	}
}

/*
 * 1. Send STATFS
 * 2. Compare expected results
 */
- (void)testNFSv2Statfs
{
	statfsres *res;

	res = doStatfsRPC_v2(nclnt, &rootfh_v2);
	if (res->status != NFS_OK) {
		XCTFail("doStatfsRPC_v2 failed, got %d", res->status);
		return;
	}

	// res->statfsres_u.reply.bavail.tsize;
	// res->statfsres_u.reply.bavail.bsize;
	// res->statfsres_u.reply.bavail.blocks;
	// res->statfsres_u.reply.bavail.bfree;
	// res->statfsres_u.reply.bavail.bavail;
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

/*
 * 1. Setup IPv4, TCP connection with UNIX authentication
 * 2. Send NFS_MAXPACKET - 1 bytes of data to the server
 * 3. Verify that vfs.generic.nfs.server.unprocessed_rpc_current is grather then zero
 */
- (void)testNetworkMbuf
{
	doNFSSetUpVerbose(AF_INET, SOCK_STREAM, RPCAUTH_UNIX);

	for (int i = 0; i < 5; i++) {
		int fd = socket(AF_INET, SOCK_STREAM, 0);
		if (fd < 0) {
			XCTFail("Couldn't create server socket: errno %d: %s\n", errno, strerror(errno));
			return;
		}

		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_addr.s_addr = inet_addr(LOCALHOST4);
		addr.sin_port = htons(2049);

		if (connect(fd, (struct sockaddr*) &addr, sizeof(addr)) != 0) {
			close(fd);
			XCTFail("connect %s failed\n", LOCALHOST4);
			return;
		}

		static char tmp[NFS_MAXPACKET - 2];
		memset(tmp, 'A', sizeof(tmp));
		size_t len = NFS_MAXPACKET - 1;
		*(uint32_t*)tmp = ntohl(0x80000000 | len);//last fragment of RPC record

		send(fd, &tmp, sizeof(tmp), 0);

		int unprocessed_rpc_current = 0;
		XCTAssertFalse(sysctl_get("vfs.generic.nfs.server.unprocessed_rpc_current", &unprocessed_rpc_current));
		XCTAssertTrue(unprocessed_rpc_current > 0);

		close(fd);
	}
}

/*
 * 1. Setup IPv4, TCP connection with UNIX authentication
 * 2. Send (twice) NFS_MAXPACKET + 1 bytes of data to the server
 * 3. Verify that socket is reset by peer
 */
- (void)testNetworkPacketSize
{
	size_t bytes;
	char tmp[0x4000];
	int err, error, fd;
	struct sockaddr_in addr;
	socklen_t len = sizeof(error);

	doNFSSetUpVerbose(AF_INET, SOCK_STREAM, RPCAUTH_UNIX);

	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0) {
		XCTFail("Couldn't create server socket: errno %d: %s\n", errno, strerror(errno));
		return;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(LOCALHOST4);
	addr.sin_port = htons(2049);

	if (connect(fd, (struct sockaddr*) &addr, sizeof(addr)) != 0) {
		close(fd);
		XCTFail("connect %s failed\n", LOCALHOST4);
		return;
	}

	memset(tmp, '\x41', 0x4000);
	*(uint32_t*)tmp = ntohl(NFS_MAXPACKET + 1);

	bytes = send(fd, &tmp, sizeof(tmp), 0);
	if (bytes < 0) {
		XCTFail("send failed %zu, %d", bytes, errno);
	}

	sleep(1);

	bytes = send(fd, &tmp, sizeof(tmp), 0);
	if (bytes < 0) {
		XCTFail("send failed %zu, %d", bytes, errno);
	}

	sleep(1);

	err = getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len);
	if (err < 0) {
		XCTFail("getsockopt failed %d\n", err);
	} else if (error != ECONNRESET) {
		XCTFail("getsockopt is expected to return ECONNRESET. got %d\n", err);
	}

	close(fd);
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

@interface nfsrvTests_syscalls : XCTestCase

@end

@implementation nfsrvTests_syscalls

/*
 * 1. Setup IPv4, TCP connection with UNIX authentication
 * 2. Switch to user #99
 * 3. Call fhopen() with the filehandle of the root. verify EPERM is returned
 * 4. Switch back to root
 */
- (void)testFHOpenNonRoot
{
	int err, fd = -1;
	struct fhandle fh;

	doNFSSetUpWrapper(AF_INET, SOCK_STREAM, RPCAUTH_UNIX);

	err = switchToUnknownUserAndGroup(-1);
	if (err != 0) {
		XCTFail("switchToUnknownUserAndGroup failed %d", err);
	}

	fh.fh_len = rootfh.data.data_len;
	memcpy(fh.fh_data, rootfh.data.data_val, MIN(sizeof(fh.fh_data), rootfh.data.data_len));

	fd = fhopen(&fh, O_RDWR);
	if (fd >= 0 || errno != EPERM) {
		XCTFail("fhopen is expected to fail with EPERM. got %d errno %d\n", fd, errno);
	}

	err = switchToRoot();
	if (err != 0) {
		XCTFail("switchToRoot failed %d", err);
	}

	if (fd >= 0) {
		close(fd);
	}
}

/*
 * 1. Setup IPv4, TCP connection with UNIX authentication
 * 2. Call fhopen() with invalid filehandle of the root. verify EINVAL is returned
 */
- (void)testFHOpenInvalidSize
{
	int fd = -1;
	struct fhandle fh;

	doNFSSetUpWrapper(AF_INET, SOCK_STREAM, RPCAUTH_UNIX);

	fh.fh_len = NFSV4_MAX_FH_SIZE;
	memcpy(fh.fh_data, rootfh.data.data_val, MIN(sizeof(fh.fh_data), rootfh.data.data_len));

	fd = fhopen(&fh, O_RDWR);
	if (fd >= 0 || errno != EINVAL) {
		XCTFail("fhopen is expected to fail with EINVAL. got %d errno %d\n", fd, errno);
	}

	if (fd >= 0) {
		close(fd);
	}
}

/*
 * 1. Setup IPv4, TCP connection with UNIX authentication
 * 2. Call fhopen() with the filehandle of the root using O_RDONLY and O_EXLOCK flags
 * 3. Verify file is locked using fcntl
 */
- (void)testFHOpenExclusiveLock
{
	int err, fd = -1;
	struct fhandle fh = {};
	struct flock lock = {};

	doNFSSetUpWrapper(AF_INET, SOCK_STREAM, RPCAUTH_UNIX);

	fh.fh_len = rootfh.data.data_len;
	memcpy(fh.fh_data, rootfh.data.data_val, MIN(sizeof(fh.fh_data), rootfh.data.data_len));

	fd = fhopen(&fh, O_RDONLY | O_EXLOCK);
	if (fd < 0) {
		XCTFail("fhopen failed. got %d errno %d\n", fd, errno);
		return;
	}

	lock.l_len = 0;
	lock.l_start = 0;
	lock.l_whence = SEEK_SET;
	lock.l_type = F_RDLCK;
	err = fcntl(fd, F_GETLK, &lock);
	if (err) {
		XCTFail("fcntl failed. got %d errno %d\n", err, errno);
	}
	XCTAssertEqual(lock.l_type, F_WRLCK);

	if (fd >= 0) {
		close(fd);
	}
}

/*
 * 1. Setup IPv4, TCP connection with UNIX authentication
 * 2. Switch to user #99
 * 3. Call getfh() with the exported root path. verify EACCES is returned
 * 4. Switch back to root
 */
- (void)testGetFHNonRoot
{
	int err;
	struct fhandle fh;

	doNFSSetUpWrapper(AF_INET, SOCK_STREAM, RPCAUTH_UNIX);

	err = switchToUnknownUserAndGroup(-1);
	if (err != 0) {
		XCTFail("switchToUnknownUserAndGroup failed %d", err);
	}

	err = getfh(getLocalMountedPath(), &fh);
	if (err >= 0 || errno != EACCES) {
		XCTFail("fhopen is expected to fail with EACCES. got %d errno %d\n", err, errno);
	}

	err = switchToRoot();
	if (err != 0) {
		XCTFail("switchToRoot failed %d", err);
	}
}

/*
 * 1. Setup IPv4, TCP connection with UNIX authentication
 * 2. Call getfh() with the non exported path. verify EINVAL is returned
 */
- (void)testGetFHNonExported
{
	int err;
	struct fhandle fh;

	doNFSSetUpWrapper(AF_INET, SOCK_STREAM, RPCAUTH_UNIX);

	err = getfh("/", &fh);
	if (err >= 0 || errno != EINVAL) {
		XCTFail("fhopen is expected to fail with EPERM. got %d errno %d\n", err, errno);
	}
}

/*
 * 1. Setup IPv4, TCP connection with UNIX authentication
 * 2. Call getfh() with the exported root path.
 * 3. Verify return file handle is equal to the root file handle
 */
- (void)testGetFHExported
{
	int err;
	struct fhandle fh;

	doNFSSetUpWrapper(AF_INET, SOCK_STREAM, RPCAUTH_UNIX);

	err = getfh(getLocalMountedPath(), &fh);
	if (err < 0) {
		XCTFail("fhopen failed. got %d errno %d\n", err, errno);
	}

	XCTAssertEqual(fh.fh_len, rootfh.data.data_len);
	XCTAssertEqual(memcmp(rootfh.data.data_val, fh.fh_data, fh.fh_len), 0);
}

static void
nfssvc_init_vec(struct iovec *vec, void *buf, size_t *buflen)
{
	vec[0].iov_base = buf;
	vec[0].iov_len = *buflen;
	vec[1].iov_base = buflen;
	vec[1].iov_len = sizeof(*buflen);
}

/*
 * 1. call nfssvc(NFSSVC_USERCOUNT), verify success
 */
- (void)testNFSSVCUserCount
{
	int err;
	uint32_t count = 0;
	size_t buflen = sizeof(count);
	struct iovec vec[2];

	nfssvc_init_vec(vec, &count, &buflen);
	err = nfssvc(NFSSVC_USERCOUNT, vec);
	if (err < 0) {
		XCTFail("nfssvc failed. got %d errno %d\n", err, errno);
	}
}

/*
 * 1. call nfssvc(NFSSVC_EXPORTSTATS), verify success
 */
- (void)testNFSSVCExportStats
{
	int err;
	char buf[32768] = {};
	size_t buflen = sizeof(buf);
	struct iovec vec[2];

	nfssvc_init_vec(vec, buf, &buflen);
	err = nfssvc(NFSSVC_EXPORTSTATS, vec);
	if (err < 0) {
		XCTFail("nfssvc failed. got %d errno %d\n", err, errno);
	}
}

/*
 * 1. call nfssvc(NFSSVC_USERSTATS), verify success
 */
- (void)testNFSSVCUserStats
{
	int err;
	char buf[32768] = {};
	size_t buflen = sizeof(buf);
	struct iovec vec[2];

	nfssvc_init_vec(vec, buf, &buflen);
	err = nfssvc(NFSSVC_USERSTATS, vec);
	if (err < 0) {
		XCTFail("nfssvc failed. got %d errno %d\n", err, errno);
	}
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
