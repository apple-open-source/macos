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

#include <oncrpc/auth.h>
#include <oncrpc/clnt.h>
#include <oncrpc/types.h>
#include <oncrpc/rpc_control.h>
#include <oncrpc/pmap_clnt.h>
#include <nfs/rpcv2.h>
#include <sys/quota.h>

#include <ftw.h>
#include "common.h"
#include "mountd.h"
#include "rquota_rpc.h"

#import <XCTest/XCTest.h>

#define DSTDIR     "/System/Volumes/Data/private/tmp/nfsrvtest"
#define TEMPLATE    DSTDIR "/nfsrvtest.XXXXXXXX"
#define CONFIG      "nfs.conf"
#define VOLUME_NAME "rquotad-test"
#define MOUNT_PATH  "/Volumes/" VOLUME_NAME
#define DISK_IMAGE  "hfs.diskimage.dmg"

char rtemplate[] = TEMPLATE;
char *rrootdir = NULL;
char diskimage[PATH_MAX];
char rconfPath[PATH_MAX];
CLIENT *rclnt = NULL;
int rconfFD = -1, rdirFD = -1;

int rquotad_imp(int argc, char *argv[], const char *conf_path);

const char quota_user[48] = {
	0xff, 0x31, 0xff, 0x35, 0x00, 0x00, 0x00, 0x01,
	0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x09, 0x3a, 0x80, 0x00, 0x09, 0x3a, 0x80,
	0x51, 0x55, 0x4f, 0x54, 0x41, 0x20, 0x48, 0x41,
	0x53, 0x48, 0x20, 0x46, 0x49, 0x4c, 0x45, 0x00,
	// Zero padding up to 131136 bytes
};

const char quota_group[48] = {
	0xff, 0x31, 0xff, 0x27, 0x00, 0x00, 0x00, 0x01,
	0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x01,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x09, 0x3a, 0x80, 0x00, 0x09, 0x3a, 0x80,
	0x51, 0x55, 0x4f, 0x54, 0x41, 0x20, 0x48, 0x41,
	0x53, 0x48, 0x20, 0x46, 0x49, 0x4c, 0x45, 0x00,
	// Zero padding up to 131136 bytes
};

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

static int
switchToUnknownUserAndGroup(void)
{
	int err;
	AUTH *auth = NULL;

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
	clnt_auth_set(rclnt, auth);

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
runTask(NSString *launchPath, NSArray<NSString *> *arguments)
{
#if TARGET_OS_OSX /* NSTask is supported only by MacOS */

	NSTask *task = [[NSTask alloc] init];
	task.launchPath = launchPath;
	task.arguments = arguments;
	[task launch];

	while ([task isRunning]) {
		usleep(1000);
	}

#endif /* TARGET_OS_OSX */
}

static void
mountHFSimage(void)
{
	snprintf(diskimage, sizeof(diskimage), "%s/%s", rrootdir, DISK_IMAGE);

	// Create HFS+ disk image
	runTask(@"/usr/bin/hdiutil", @[@"create", @"-fs", @"HFS+", @"-o", [NSString stringWithUTF8String:diskimage], @"-size", @"50M", @"-volname", @VOLUME_NAME]);

	// Mount the disk image
	runTask(@"/usr/bin/hdiutil", @[@"attach", [NSString stringWithUTF8String:diskimage]]);
}

static int
getQuotaAndVerify(char *path, int type, int uid, rquota *quota, struct dqblk *dqb)
{
	int err, qcmd;
	struct dqblk blk = {};
	uint64_t bsize = DEV_BSIZE;

	qcmd = QCMD(Q_GETQUOTA, type == RQUOTA_USRQUOTA ? USRQUOTA : GRPQUOTA);

	err =  quotactl(path, qcmd, uid, (char*)&blk);
	if (err) {
		XCTFail("quotactl Q_GETQUOTA failed for uid %d, err %d, errno %d", uid, err, errno);
		return -1;
	}

	/* Compare to requested values */
	XCTAssertEqual(dqb->dqb_bhardlimit, blk.dqb_bhardlimit);
	XCTAssertEqual(dqb->dqb_bsoftlimit, blk.dqb_bsoftlimit);
	XCTAssertEqual(dqb->dqb_isoftlimit, blk.dqb_isoftlimit);
	XCTAssertEqual(dqb->dqb_ihardlimit, blk.dqb_ihardlimit);

	/* Compare to recevied values */
	XCTAssertTrue(quota->rq_active);
	XCTAssertEqual(bsize, quota->rq_bsize);
	XCTAssertEqual(blk.dqb_bhardlimit / bsize, quota->rq_bhardlimit);
	XCTAssertEqual(blk.dqb_bsoftlimit / bsize, quota->rq_bsoftlimit);
	XCTAssertEqual(blk.dqb_curbytes / bsize, quota->rq_curblocks);
	XCTAssertEqual(blk.dqb_ihardlimit, quota->rq_fhardlimit);
	XCTAssertEqual(blk.dqb_isoftlimit, quota->rq_fsoftlimit);
	XCTAssertEqual(blk.dqb_curinodes, quota->rq_curfiles);

	return 0;
}

static int
createUserFiles(char *mount_point, char *path, size_t path_size)
{
	int fd;

	// Create files
	snprintf(path, path_size, "%s/.quota.ops.user", mount_point);
	fd = open(path, O_CREAT | O_RDWR, 0777);
	if (fd < 0) {
		XCTFail("Cant create quota user file, err %d, errno %d", -1, errno);
		return -1;
	}
	close(fd);

	snprintf(path, path_size, "%s/.quota.user", mount_point);
	fd = open(path, O_CREAT | O_RDWR, 0777);
	if (fd < 0) {
		XCTFail("Cant create quota user file, err %d, errno %d", -1, errno);
		return -1;
	}
	write(fd, quota_user, sizeof(quota_user));
	ftruncate(fd, 131136);
	close(fd);

	return 0;
}

static int
createGroupFiles(char *mount_point, char *path, size_t path_size)
{
	int fd;

	// Create files
	snprintf(path, path_size, "%s/.quota.ops.group", mount_point);
	fd = open(path, O_CREAT | O_RDWR, 0777);
	if (fd < 0) {
		XCTFail("Cant create quota group file, err %d, errno %d", -1, errno);
		return -1;
	}
	close(fd);

	snprintf(path, path_size, "%s/.quota.group", mount_point);
	fd = open(path, O_CREAT | O_RDWR, 0777);
	if (fd < 0) {
		XCTFail("Cant create quota group file, err %d, errno %d", -1, errno);
		return -1;
	}
	write(fd, quota_group, sizeof(quota_group));
	ftruncate(fd, 131136);
	close(fd);

	return 0;
}

static int
setQuota(char *mount_point, int type, int uid, struct dqblk *dqb)
{
	int err, qcmd;
	char path[PATH_MAX] = {};

	mountHFSimage();

	if (type == RQUOTA_USRQUOTA) {
		err  = createUserFiles(mount_point, path, sizeof(path));
	} else {
		err  = createGroupFiles(mount_point, path, sizeof(path));
	}
	if (err) {
		XCTFail("Cannot create files for type %d, err %d, errno %d", type, err, errno);
		return err;
	}

	qcmd = QCMD(Q_QUOTAON, type == RQUOTA_USRQUOTA ? USRQUOTA : GRPQUOTA);
	err = quotactl(mount_point, qcmd, uid, path);
	if (err) {
		XCTFail("quotactl Q_QUOTAON failed for uid %d, err %d, errno %d, path %s", uid, err, errno, path);
		return err;
	}

	qcmd = QCMD(Q_SETQUOTA, type == RQUOTA_USRQUOTA ? USRQUOTA : GRPQUOTA);
	err = quotactl(mount_point, qcmd, uid, (char*)dqb);
	if (err) {
		XCTFail("quotactl Q_SETQUOTA failed for uid %d, err %d, errno %d", uid, err, errno);
		return err;
	}

	return err;
}

static int
r_unlink_cb(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
	int rv = remove(path);

	if (rv) {
		XCTFail("remove failed %d", rv);
	}

	return rv;
}

static int
doRquotaSetup(const char **confValues, int confValuesSize)
{
	// Create temporary folder name
	rrootdir = mkdtemp(rtemplate);
	if (!rrootdir) {
		XCTFail("Unable to create tmpdir: %d", errno);
		return -1;
	}

	if (chmod(rtemplate, 0777)) {
		XCTFail("Unable to chmod tmpdir (%s): %d", rrootdir, errno);
		return -1;
	}

	// Create temporary folder
	rdirFD = open(rrootdir, O_DIRECTORY | O_SEARCH, 0777);
	if (rdirFD < 0) {
		XCTFail("Unable to open tmpdir (%s): %d", rrootdir, errno);
		return -1;
	}

	rconfFD = openat(rdirFD, CONFIG, O_CREAT | O_RDWR | O_EXCL, 0666);
	if (rconfFD < 0) {
		XCTFail("Unable to create config (%s:/%s): %d", rrootdir, CONFIG, errno);
		return -1;
	}

	for (int i = 0; i < confValuesSize; i++) {
		dprintf(rconfFD, "%s\n", confValues[i]);
	}

	// Create conf exports path
	snprintf(rconfPath, sizeof(rconfPath), "%s/%s", rrootdir, CONFIG);

	if (fork() == 0) {
		int argc = 1;
		char **argv = calloc(argc, sizeof(char *));
		if (argv == NULL) {
			XCTFail("Cannot allocate argv array");
			return -1;
		}

		// Build args list
		argv[0] = "rquotad";

		// Kick rquotad
		rquotad_imp(argc, argv, rconfPath);
		free(argv);
	} else {
		// Sleep until rquotad is up
		int retry = 10;
		while (retry-- && get_rquotad_pid() == 0) {
			sleep(1);
		}
		if (retry == 0) {
			XCTFail("rquotad did not start!");
		}
	}

	return 0;
}

static CLIENT *
createClientForRquotaProtocolWithConf(int socketFamily, int socketType, int authType, int version, const char **confValues, int confValuesSize, int flags)
{
	const char *host;

	if (doRquotaSetup(confValues, confValuesSize) < 0) {
		XCTFail("doRquotaSetup failed");
		return NULL;
	}

	switch (socketFamily) {
	case AF_INET:
		host = LOCALHOST4;
		break;
	case AF_INET6:
		host = LOCALHOST6;
		break;
	default:
		XCTFail("Unsupported family");
		return NULL;
	}

	return createClientForProtocol(host, socketFamily, socketType, authType, RQUOTAPROG, version, flags, NULL);
}

static CLIENT *
createClientForRquotaProtocol(int socketFamily, int socketType, int authType, int version)
{
	return createClientForRquotaProtocolWithConf(socketFamily, socketType, authType, version, NULL, 0, 0);
}

@interface nfsrvTests_rquota : XCTestCase

@end

@implementation nfsrvTests_rquota

- (void)setUp
{
	int err;
	uid_t uid;

	// Tests should run as root
	// Enable root option using : defaults write com.apple.dt.Xcode EnableRootTesting YES
	if ((uid = getuid()) != 0) {
		XCTFail("Test should run as root, current user %d", uid);
		return;
	}

	unmount(MOUNT_PATH, MNT_FORCE);

	// Remove all test remainings
	err = nftw(DSTDIR, r_unlink_cb, 64, FTW_MOUNT | FTW_DEPTH | FTW_PHYS);
	if (err == 0) {
		/* cleanup succeeded */
		sleep(1);
	} else if (err < 0 && errno != ENOENT) {
		XCTFail("nftw failed %d, %d", err, errno);
		return;
	}

	err = mkdir(DSTDIR, 0777);
	if (err < 0) {
		XCTFail("Unable to create root dir %s %d", DSTDIR, errno);
		return;
	}

	if (get_rquotad_pid() > 0) {
		XCTFail("rquotad should be stopped!");
	}
}

- (void)tearDown
{
	pid_t pid;

	if ((pid = get_rquotad_pid()) != 0) {
		kill(pid, SIGTERM);
		// Wait for rquotad termination
		int retry = 10;
		while (retry-- && get_rquotad_pid() != 0) {
			sleep(1);
		}
		if (retry == 0) {
			XCTFail("rquotad is still up!");
		}
	}

	if (strnlen(diskimage, sizeof(diskimage) > 0)) {
		unmount(MOUNT_PATH, MNT_FORCE);
		if (unlink(diskimage) < 0) {
			XCTFail("Unable to remove diskimage %s: %d", diskimage, errno);
		}
		memset(diskimage, 0, sizeof(diskimage));
	}

	if (rconfFD >= 0) {
		close(rconfFD);
		rconfFD = -1;
		unlink(rconfPath);
		bzero(rconfPath, sizeof(rconfPath));
	}

	if (rdirFD >= 0) {
		close(rdirFD);
		rdirFD = -1;
	}

	if (rrootdir) {
		if (rmdir(rrootdir) < 0) {
			XCTFail("Unable to remove dir %s: %d", rrootdir, errno);
		}
		rrootdir = NULL;
	}

	rmdir(DSTDIR);

	if (rclnt) {
		clnt_destroy(rclnt);
		rclnt = NULL;
	}
}

/* --------- RQUOTAVERS --------- */

/*
 * 1. Setup IPv4, TCP connection with Unix authentication
 * 2. Send NULL to the server, make sure we got the reply
 */
- (void)testRquotaTCPNull
{
	if ((rclnt = createClientForRquotaProtocol(AF_INET, SOCK_STREAM, RPCAUTH_UNIX, RQUOTAVERS)) == NULL) {
		XCTFail("Cannot create client");
	}

	RQUOTAVERS_doNullRPC(rclnt);
}

/*
 * 1. Setup IPv6, UDP connection with Unix authentication
 * 2. Send NULL to the server, make sure we got the reply
 */
- (void)testRquotaIPv6UDPNull
{
	if ((rclnt = createClientForRquotaProtocol(AF_INET6, SOCK_DGRAM, RPCAUTH_UNIX, RQUOTAVERS)) == NULL) {
		XCTFail("Cannot create client");
	}

	RQUOTAVERS_doNullRPC(rclnt);
}

/*
 * 1. Setup IPv4, TCP connection with Null authentication
 * 2. Verify GetQuota RPC fails with Q_EPERM
 */
- (void)testRquotaGetQuotaAuthNull
{
	getquota_rslt *res;

	if ((rclnt = createClientForRquotaProtocol(AF_INET, SOCK_STREAM, RPCAUTH_NULL, RQUOTAVERS)) == NULL) {
		XCTFail("Cannot create client");
	}

	res = RQUOTAVERS_doGetQuotaRPC(rclnt, "/", 0);
	if (res->status != Q_EPERM) {
		XCTFail("RQUOTAVERS_doGetQuotaRPC should failed with %d, got %d", Q_EPERM, res->status);
	}
}

/*
 * 1. Setup IPv4, TCP connection
 * 2. Switch to user 99 and group 99
 * 3. Verify GetQuota RPC for user 20 fails with Q_EPERM
 */
- (void)testRquotaGetQuotaForDifferentUser
{
	int err;
	getquota_rslt *res;

	if ((rclnt = createClientForRquotaProtocol(AF_INET, SOCK_STREAM, RPCAUTH_UNIX, RQUOTAVERS)) == NULL) {
		XCTFail("Cannot create client");
		return;
	}

	err = switchToUnknownUserAndGroup();
	if (err) {
		XCTFail("Cannot switch to user");
		return;
	}

	res = RQUOTAVERS_doGetQuotaRPC(rclnt, "/", 20);
	if (res->status != Q_EPERM) {
		XCTFail("RQUOTAVERS_doGetQuotaRPC should failed with %d, got %d", Q_EPERM, res->status);
	}

	err = switchToRoot();
	if (err) {
		XCTFail("Cannot switch to user");
		return;
	}
}

/*
 * 1. Setup IPv4, TCP connection
 * 2. Switch to user 99 and group 99
 * 3. Verify GetQuota RPC for user 99 fails with Q_NOQUOTA
 */
- (void)testRquotaGetQuotaForUserNoQuota
{
	int err;
	getquota_rslt *res;

	if ((rclnt = createClientForRquotaProtocol(AF_INET, SOCK_STREAM, RPCAUTH_UNIX, RQUOTAVERS)) == NULL) {
		XCTFail("Cannot create client");
		return;
	}

	err = switchToUnknownUserAndGroup();
	if (err) {
		XCTFail("Cannot switch to user");
		return;
	}

	res = RQUOTAVERS_doGetQuotaRPC(rclnt, "/", UNKNOWNUID);
	if (res->status != Q_NOQUOTA) {
		XCTFail("RQUOTAVERS_doGetQuotaRPC should failed with %d, got %d", Q_NOQUOTA, res->status);
	}

	err = switchToRoot();
	if (err) {
		XCTFail("Cannot switch to user");
		return;
	}
}

/*
 * 1. Setup IPv4, TCP connection
 * 2. Verify GetQuota RPC as root for user 20 fails with Q_NOQUOTA
 */
- (void)testRquotaGetQuotaForRootNoQuota
{
	getquota_rslt *res;

	if ((rclnt = createClientForRquotaProtocol(AF_INET, SOCK_STREAM, RPCAUTH_UNIX, RQUOTAVERS)) == NULL) {
		XCTFail("Cannot create client");
		return;
	}

	res = RQUOTAVERS_doGetQuotaRPC(rclnt, "/", 20);
	if (res->status != Q_NOQUOTA) {
		XCTFail("RQUOTAVERS_doGetQuotaRPC should failed with %d, got %d", Q_NOQUOTA, res->status);
	}
}

/*
 * 1. Setup IPv4, TCP connection
 * 2. Verify GetQuota RPC as root for non existing path fails with Q_NOQUOTA
 */
- (void)testRquotaGetQuotaNoPathNoQuota
{
	getquota_rslt *res;

	if ((rclnt = createClientForRquotaProtocol(AF_INET, SOCK_STREAM, RPCAUTH_UNIX, RQUOTAVERS)) == NULL) {
		XCTFail("Cannot create client");
		return;
	}

	res = RQUOTAVERS_doGetQuotaRPC(rclnt, "/nosuch/path", 0);
	if (res->status != Q_NOQUOTA) {
		XCTFail("RQUOTAVERS_doGetQuotaRPC should failed with %d, got %d", Q_NOQUOTA, res->status);
	}
}

/*
 * 1. Setup IPv4, TCP connection
 * 2. Create HFS+ disk image
 * 3. Mount the disk image
 * 4. Set disk Quota for user 99
 * 5. Switch to user 99 and group 99
 * 6. Verify GetQuota RPC for user 99 passes, verify quota values
 */
- (void)testRquotaGetQuotaForUser
{
	int err;
	getquota_rslt *res;
	struct dqblk dqb = { .dqb_bsoftlimit = 1024 * 1024, .dqb_bhardlimit = 1024 * 2048, .dqb_isoftlimit = 10, .dqb_ihardlimit = 20 };

	if ((rclnt = createClientForRquotaProtocol(AF_INET, SOCK_STREAM, RPCAUTH_UNIX, RQUOTAVERS)) == NULL) {
		XCTFail("Cannot create client");
		return;
	}

	err = setQuota(MOUNT_PATH, RQUOTA_USRQUOTA, UNKNOWNUID, &dqb);
	if (err) {
		XCTFail("setQuota failed %d", err);
		return;
	}

	err = switchToUnknownUserAndGroup();
	if (err) {
		XCTFail("Cannot switch to user");
		return;
	}

	res = RQUOTAVERS_doGetQuotaRPC(rclnt, MOUNT_PATH, UNKNOWNUID);
	if (res->status != Q_OK) {
		XCTFail("RQUOTAVERS_doGetQuotaRPC failed, got %d", res->status);
	}

	err = switchToRoot();
	if (err) {
		XCTFail("Cannot switch to user");
		return;
	}

	err = getQuotaAndVerify(MOUNT_PATH, RQUOTA_USRQUOTA, UNKNOWNUID, &res->getquota_rslt_u.gqr_rquota, &dqb);
	if (err) {
		XCTFail("getQuota failed, got %d %d", err, errno);
	}
}

/*
 * 1. Setup IPv4, UDP connection while nfs.conf contains "nfs.server.rquota.udp=0"
 * 2. Verify UDP CLIENT creation failed
 */
- (void)testRquotaConfigNoTransportUDP
{
	int disable = 0;
	const char buff[NAME_MAX] = {};
	const char *conf[1] = { buff };
	snprintf((char *)buff, sizeof(buff), "%s=%d", "nfs.server.rquota.udp", disable);

	if ((rclnt = createClientForRquotaProtocolWithConf(AF_INET, SOCK_DGRAM, RPCAUTH_UNIX, RQUOTAVERS, conf, ARRAY_SIZE(conf), CREATE_CLIENT_FAILURE)) != NULL) {
		XCTFail("create should be NULL");
	}
}

/*
 * 1. Setup IPv6, TCP connection while nfs.conf contains "nfs.server.rquota.tcp=0"
 * 2. Verify TCP CLIENT creation failed
 */
- (void)testRquotaConfigNotransportTCP
{
	int disable = 0;
	const char buff[NAME_MAX] = {};
	const char *conf[1] = { buff };
	snprintf((char *)buff, sizeof(buff), "%s=%d", "nfs.server.rquota.tcp", disable);

	if ((rclnt = createClientForRquotaProtocolWithConf(AF_INET6, SOCK_STREAM, RPCAUTH_UNIX, RQUOTAVERS, conf, ARRAY_SIZE(conf), CREATE_CLIENT_FAILURE)) != NULL) {
		XCTFail("create should be NULL");
	}
}

/*
 * 1. Setup IPv6, TCP connection while nfs.conf "nfs.server.rquota.port"=44444"
 * 2. Verify Rquota port is 44444 as expected
 */
- (void)testRquotaConfigPort
{
	const char buff[NAME_MAX] = {};
	const char *conf[1] = { buff };
	struct sockaddr_storage addr = {};
	int expected_port = 44444;
	snprintf((char *)buff, sizeof(buff), "%s=%d", "nfs.server.rquota.port", expected_port);

	if ((rclnt = createClientForRquotaProtocolWithConf(AF_INET, SOCK_STREAM, RPCAUTH_UNIX, RQUOTAVERS, conf, ARRAY_SIZE(conf), 0)) == NULL) {
		XCTFail("Cannot create client");
	} else {
		XCTAssertTrue(clnt_control(rclnt, CLGET_SERVER_ADDR, &addr));
	}

	XCTAssertEqual(expected_port, pmap_getport((struct sockaddr_in *)&addr, RQUOTAPROG, RQUOTAVERS, IPPROTO_UDP));
	XCTAssertEqual(expected_port, pmap_getport((struct sockaddr_in *)&addr, RQUOTAPROG, RQUOTAVERS, IPPROTO_TCP));
}

/* --------- EXT_RQUOTAVERS --------- */

/*
 * 1. Setup IPv4, TCP connection with Unix authentication
 * 2. Send NULL to the server, make sure we got the reply
 */
- (void)testExtRquotaTCPNull
{
	if ((rclnt = createClientForRquotaProtocol(AF_INET, SOCK_STREAM, RPCAUTH_NULL, EXT_RQUOTAVERS)) == NULL) {
		XCTFail("Cannot create client");
	}

	EXT_RQUOTAVERS_doNullRPC(rclnt);
}

/*
 * 1. Setup IPv6, UDP connection with Unix authentication
 * 2. Send NULL to the server, make sure we got the reply
 */
- (void)testExtRquotaIPv6UDPNull
{
	if ((rclnt = createClientForRquotaProtocol(AF_INET6, SOCK_DGRAM, RPCAUTH_UNIX, EXT_RQUOTAVERS)) == NULL) {
		XCTFail("Cannot create client");
	}

	EXT_RQUOTAVERS_doNullRPC(rclnt);
}

/*
 * 1. Setup IPv6, UDP connection with Null authentication
 * 2. Verify GetQuota RPC fails with Q_EPERM
 */
- (void)testExtRquotaGetQuotaAuthNull
{
	getquota_rslt *res;

	if ((rclnt = createClientForRquotaProtocol(AF_INET6, SOCK_DGRAM, RPCAUTH_NULL, EXT_RQUOTAVERS)) == NULL) {
		XCTFail("Cannot create client");
	}

	res = EXT_RQUOTAVERS_doGetQuotaRPC(rclnt, "/", RQUOTA_USRQUOTA, 0);
	if (res->status != Q_EPERM) {
		XCTFail("EXT_RQUOTAVERS_doGetQuotaRPC should failed with %d, got %d", Q_EPERM, res->status);
	}
}

/*
 * 1. Setup IPv6, UDP connection
 * 2. Switch to user 99 and group 99
 * 3. Verify GetQuota RPC for group 20 fails with Q_EPERM
 */
- (void)testRquotaGetQuotaForDifferentGroup
{
	int err;
	getquota_rslt *res;

	if ((rclnt = createClientForRquotaProtocol(AF_INET6, SOCK_DGRAM, RPCAUTH_UNIX, EXT_RQUOTAVERS)) == NULL) {
		XCTFail("Cannot create client");
		return;
	}

	err = switchToUnknownUserAndGroup();
	if (err) {
		XCTFail("Cannot switch to user");
		return;
	}

	res = EXT_RQUOTAVERS_doGetQuotaRPC(rclnt, "/", RQUOTA_GRPQUOTA, 20);
	if (res->status != Q_EPERM) {
		XCTFail("EXT_RQUOTAVERS_doGetQuotaRPC should failed with %d, got %d", Q_EPERM, res->status);
	}

	err = switchToRoot();
	if (err) {
		XCTFail("Cannot switch to user");
		return;
	}
}

/*
 * 1. Setup IPv6, UDP connection
 * 2. Create HFS+ disk image
 * 3. Mount the disk image
 * 4. Set disk Quota for group 99
 * 5. Switch to user 99 and group 99
 * 6. Verify GetQuota RPC for group 99 passes, verify quota values
 */
- (void)testExtRquotaGetActiveQuotaForGroup
{
	int err;
	getquota_rslt *res;
	struct dqblk dqb = { .dqb_bsoftlimit = 1024 * 1024, .dqb_bhardlimit = 1024 * 2048, .dqb_isoftlimit = 10, .dqb_ihardlimit = 20 };

	if ((rclnt = createClientForRquotaProtocol(AF_INET6, SOCK_DGRAM, RPCAUTH_UNIX, EXT_RQUOTAVERS)) == NULL) {
		XCTFail("Cannot create client");
		return;
	}

	err = setQuota(MOUNT_PATH, RQUOTA_GRPQUOTA, UNKNOWNGID, &dqb);
	if (err) {
		XCTFail("setQuota failed %d", err);
		return;
	}

	err = switchToUnknownUserAndGroup();
	if (err) {
		XCTFail("Cannot switch to user");
		return;
	}

	res = EXT_RQUOTAVERS_doGetActiveQuotaRPC(rclnt, MOUNT_PATH, RQUOTA_GRPQUOTA, UNKNOWNGID);
	if (res->status != Q_OK) {
		XCTFail("EXT_RQUOTAVERS_doGetActiveQuotaRPC failed, got %d", res->status);
		return;
	}

	err = switchToRoot();
	if (err) {
		XCTFail("Cannot switch to user");
		return;
	}

	err = getQuotaAndVerify(MOUNT_PATH, RQUOTA_GRPQUOTA, UNKNOWNGID, &res->getquota_rslt_u.gqr_rquota, &dqb);
	if (err) {
		XCTFail("getQuota failed, got %d %d", err, errno);
	}
}

@end
