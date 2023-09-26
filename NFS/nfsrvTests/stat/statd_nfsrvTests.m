/*
 * Copyright (c) 1999-2023 Apple Inc. All rights reserved.
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

#import <XCTest/XCTest.h>

#include <ftw.h>
#include <oncrpc/auth.h>
#include <oncrpc/clnt.h>
#include <nfs/rpcv2.h>

#include "stat_rpc.h"
#include "mountd.h"
#include "test_utils.h"

#define DSTDIR     "/System/Volumes/Data/private/tmp/nfsrvtest"
#define TEMPLATE    DSTDIR "/nfsrvtest.XXXXXXXX"
#define CONFIG      "nfs.conf"
#define VOLUME_NAME "statd-test"
#define MOUNT_PATH  "/Volumes/" VOLUME_NAME

char stemplate[] = TEMPLATE;
char *srootdir = NULL;
char sconfPath[PATH_MAX];
CLIENT *sclnt = NULL;
int sconfFD = -1, sdirFD = -1;

const char *conf_allow_simu_crash = "nfs.statd.simu_crash_allowed=1";
const char *conf_use_tcp = "nfs.statd.send_using_tcp";

int statd_imp(int argc, char **argv, const char *conf_path);
pid_t get_statd_pid(void);

static void
stop_statd(void)
{
	pid_t pid;
	int retries = 10;

	while ((pid = get_statd_pid()) != 0 && retries > 0) {
		kill(pid, SIGTERM);
		retries--;
		sleep(1);
	}
	if (retries == 0 && (get_statd_pid() != 0)) {
		XCTFail("Unable to stop statd");
	}
}

static int
doStatSetup(const char **confValues, int confValuesSize)
{
	// Create temporary folder name
	srootdir = mkdtemp(stemplate);
	if (!srootdir) {
		XCTFail("Unable to create tmpdir: %d", errno);
		return -1;
	}

	if (chmod(stemplate, 0777)) {
		XCTFail("Unable to chmod tmpdir (%s): %d", srootdir, errno);
		return -1;
	}

	// Create temporary folder
	sdirFD = open(srootdir, O_DIRECTORY | O_SEARCH, 0777);
	if (sdirFD < 0) {
		XCTFail("Unable to open tmpdir (%s): %d", srootdir, errno);
		return -1;
	}

	sconfFD = openat(sdirFD, CONFIG, O_CREAT | O_RDWR | O_EXCL, 0666);
	if (sconfFD < 0) {
		XCTFail("Unable to create config (%s:/%s): %d", srootdir, CONFIG, errno);
		return -1;
	}

	for (int i = 0; i < confValuesSize; i++) {
		dprintf(sconfFD, "%s\n", confValues[i]);
	}

	// Create conf exports path
	snprintf(sconfPath, sizeof(sconfPath), "%s/%s", srootdir, CONFIG);

	if (fork() == 0) {
		int argc = 2;
		char **argv = calloc(argc, sizeof(char *));
		if (argv == NULL) {
			XCTFail("Cannot allocate argv array");
			return -1;
		}

		// Build args list
		argv[0] = "statd";
		argv[1] = "-d";

		// Kick statd
		statd_imp(argc, argv, sconfPath);
		free(argv);
	} else {
		// Sleep until statd is up
		int retry = 10;
		while (retry-- && get_statd_pid() == 0) {
			sleep(1);
		}
		if (retry == 0) {
			XCTFail("statd did not start!");
		}
	}

	return 0;
}

static CLIENT *
createClientForStatProtocolWithConf(int socketFamily, int socketType, int authType, int version, const char **confValues, int confValuesSize, int flags)
{
	const char *host;

	if (doStatSetup(confValues, confValuesSize) < 0) {
		XCTFail("doStatSetup failed");
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

	return createClientForProtocol(host, socketFamily, socketType, authType, SM_PROG, version, flags, NULL);
}


static CLIENT *
createClientForStatProtocol(int socketFamily, int socketType, int authType, int version)
{
	return createClientForStatProtocolWithConf(socketFamily, socketType, authType, version, NULL, 0, 0);
}

static void
createClientAndNFSSetup(void)
{
	if ((sclnt = createClientForStatProtocol(AF_INET, SOCK_STREAM, RPCAUTH_NULL, SM_VERS)) == NULL) {
		XCTFail("Cannot create client");
	}
	doNFSSetUp(NULL, NULL, 0, AF_INET, SOCK_STREAM, RPCAUTH_UNIX, 0, NULL, "sys:krb5");
}

static int
s_unlink_cb(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
	int rv = remove(path);

	if (rv) {
		XCTFail("remove failed %d", rv);
	}

	return rv;
}

@interface nfsrvTests_stat : XCTestCase

@end

@implementation nfsrvTests_stat

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
	err = nftw(DSTDIR, s_unlink_cb, 64, FTW_MOUNT | FTW_DEPTH | FTW_PHYS);
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

	// stop statd if it's active
	stop_statd();

	if (get_statd_pid() > 0) {
		XCTFail("statd should be stopped!");
		return;
	}
}

- (void)tearDown
{
	pid_t pid;

	doMountTearDown();

	if ((pid = get_statd_pid()) != 0) {
		kill(pid, SIGKILL);
		while (get_statd_pid() != 0) {
			sleep(1);
		}
	}

	if (sconfFD >= 0) {
		close(sconfFD);
		sconfFD = -1;
		unlink(sconfPath);
		bzero(sconfPath, sizeof(sconfPath));
	}

	if (sdirFD >= 0) {
		close(sdirFD);
		sdirFD = -1;
	}

	rmdir(DSTDIR);

	if (sclnt) {
		clnt_destroy(sclnt);
		sclnt = NULL;
	}
}

/*
 * 1. do stat RPC
 * 2. verify stat_succ is returned
 */
-(void)testRPCStatBasic
{
	sm_stat_res *result;

	createClientAndNFSSetup();

	result = STAT_doStatRPC(sclnt, "localhost");
	if (result == NULL) {
		return;
	}
	if (result->res_stat != stat_succ) {
		XCTFail("res_stat expected %d, got %d", stat_succ, result->res_stat);
	}
}

/*
 * 1. do stat RPC
 * 2. save the returned state
 * 3. do another stat RPC
 * 4. verify that the state hasn't changed
 */
-(void)testRPCStatTwice
{
	sm_stat_res *result, *result2;

	createClientAndNFSSetup();

	result = STAT_doStatRPC(sclnt, "localhost");
	if (result == NULL) {
		return;
	}
	if (result->res_stat != stat_succ) {
		XCTFail("res_stat expected %d, got %d", stat_succ, result->res_stat);
	}


	result2 = STAT_doStatRPC(sclnt, "localhost");
	if (result2 == NULL) {
		return;
	}
	if (result2->res_stat != stat_succ) {
		XCTFail("res_stat expected %d, got %d", stat_succ, result->res_stat);
	}

	if (result->state != result2->state) {
		XCTFail("state changed, %d != %d", result->state, result2->state);
	}
}

/*
 * 1. do stat RPC with a bad name, it has a space in it
 * 2. verify stat_fail is returned
 */
-(void)testRPCStatInvalidName
{
	sm_stat_res *result;

	createClientAndNFSSetup();

	result = STAT_doStatRPC(sclnt, "invalid name");
	if (result == NULL) {
		return;
	}
	if (result->res_stat != stat_fail) {
		XCTFail("res_stat expected %d, got %d", stat_fail, result->res_stat);
	}
}

/*
 * 1. add a new monitor
 * 2. verify stat_succ is returned
 */
-(void)testRPCMonBasic
{
	sm_stat_res *res;

	createClientAndNFSSetup();

	res = STAT_doMonRPC(sclnt, "localhost", "localhost", SM_PROG, SM_VERS, SM_STAT, "");
	if (res == NULL) {
		return;
	}
	if (res->res_stat != stat_succ) {
		XCTFail("res_stat expected %d, got %d", stat_succ, res->res_stat);
	}
}

/*
 * 1. add a new monitor
 * 2. verify stat_succ is returned
 * 3. add a new monitor with the same name
 * 4. verify stat_succ is returned
 * 5. verify the state didn't change
 */
-(void)testRPCMonDup
{
	sm_stat_res *res, *res2;
	int state1, state2;
	createClientAndNFSSetup();

	res = STAT_doMonRPC(sclnt, "localhost", "localhost", SM_PROG, SM_VERS, SM_STAT, "");
	if (res == NULL) {
		return;
	}
	if (res->res_stat != stat_succ) {
		XCTFail("res_stat expected %d, got %d", stat_succ, res->res_stat);
	}
	state1 = res->state;

	res2 = STAT_doMonRPC(sclnt, "localhost", "localhost", SM_PROG, SM_VERS, SM_STAT, "");
	if (res2 == NULL) {
		return;
	}
	if (res2->res_stat != stat_succ) {
		XCTFail("res_stat expected %d, got %d", stat_succ, res->res_stat);
	}
	state2 = res2->state;
	if (state1 != state2) {
		XCTFail("state changed from %d to %d", state1, state2);
	}
}

/*
 * 1. do stat RPC with a bad hostname, it has a space in it
 * 2. verify stat_fail is returned
 */
-(void)testRPCMonInvalidHostname
{
	sm_stat_res *res;

	createClientAndNFSSetup();

	res = STAT_doMonRPC(sclnt, "invalid hostname", "localhost", SM_PROG, SM_VERS, SM_STAT, "");
	if (res == NULL) {
		return;
	}
	if (res->res_stat != stat_fail) {
		XCTFail("res_stat expected %d, got %d", stat_fail, res->res_stat);
	}
}

/*
 * 1. add a new monitor
 * 2. verify stat_succ is returned
 * 3. remove the monitor
 * 4. verify NULL isn't returned
 */
-(void)testRPCUnmonBasic
{
	sm_stat_res *res;
	sm_stat *unmon_res;

	createClientAndNFSSetup();


	res = STAT_doMonRPC(sclnt, "localhost", "localhost", SM_PROG, SM_VERS, SM_STAT, "");
	if (res == NULL) {
		return;
	}
	if (res->res_stat != stat_succ) {
		XCTFail("res_stat expected %d, got %d", stat_fail, res->res_stat);
	}

	unmon_res = STAT_doUnmonRPC(sclnt, "localhost", "localhost", SM_PROG, SM_VERS, SM_STAT);
	if (unmon_res == NULL) {
		XCTFail("STAT_doUnmonRPC returned NULL");
		return;
	}
}

/*
 * 1. remove a monitor with a bad name, it has a space in it
 * 2. verify NULL is returned
 */
-(void)testStatdUnmonInvalidName
{
	sm_stat *unmon_res;

	createClientAndNFSSetup();

	unmon_res = STAT_doUnmonRPC(sclnt, "invalid name", "localhost", SM_PROG, SM_VERS, SM_STAT);

	if (unmon_res == NULL) {
		XCTFail("STAT_doUnmonRPC returned NULL");
		return;
	}
}
-(void)testStatdUnmonInvalidMon
{
	sm_stat *unmon_res;

	createClientAndNFSSetup();

	unmon_res = STAT_doUnmonRPC(sclnt, "localhost", "invalid name", SM_PROG, SM_VERS, SM_STAT);

	if (unmon_res == NULL) {
		XCTFail("STAT_doUnmonRPC returned NULL");
		return;
	}
}

/*
 * 1. add a monitor
 * 2. verify NULL isn't returned
 * 3. do stat RPC and save the state
 * 4. simulate a crash
 * 5. verify the state is increased by 2
 */
-(void)testStatdSimuCrash
{
	char dummy;
	sm_stat_res *res1, *res2;
	int state1, state2;
	sm_stat_res *mon_res;
	const char *conf[1] = {conf_allow_simu_crash};

	sclnt = createClientForStatProtocolWithConf(AF_INET, SOCK_STREAM, 0, SM_VERS, conf, ARRAY_SIZE(conf), 0);

	doNFSSetUp(NULL, NULL, 0, AF_INET, SOCK_STREAM, RPCAUTH_UNIX, 0, NULL, "sys:krb5");

	mon_res = STAT_doMonRPC(sclnt, "localhost", "localhost", SM_PROG, SM_VERS, SM_STAT, "");
	if (mon_res == NULL) {
		return;
	}
	res1 = STAT_doStatRPC(sclnt, "localhost");
	state1 = res1->state;

	sm_simu_crash_1(&dummy, sclnt);
	res2 = STAT_doStatRPC(sclnt, "localhost");
	state2 = res2->state;

	if (state2 != state1 + 2) {
		XCTFail("state should increase by 2 after a crash");
	}
}

/*
 * 1. turn on the conf_use_tcp option
 * 2. do stat rpc
 * 3. verify stat_succ is returned
 * 4. add a monitor
 * 5. verify NULL isn't returned
 */
-(void)testStatdTCP
{
	sm_stat_res *res;
	const char *conf[1] = {conf_use_tcp};
	sm_stat_res *mon_res;

	sclnt = createClientForStatProtocolWithConf(AF_INET, SOCK_STREAM, 0, SM_VERS, conf, ARRAY_SIZE(conf), 0);

	doNFSSetUp(NULL, NULL, 0, AF_INET, SOCK_STREAM, RPCAUTH_UNIX, 0, NULL, "sys:krb5");

	res = STAT_doStatRPC(sclnt, "localhost");
	if (res == NULL) {
		return;
	}
	if (res->res_stat != stat_succ) {
		XCTFail("res_stat expected %d, got %d", stat_succ, res->res_stat);
	}

	mon_res = STAT_doMonRPC(sclnt, "localhost", "localhost", SM_PROG, SM_VERS, SM_STAT, "");
	if (mon_res == NULL) {
		XCTFail("STAT_doMonRPC returned NULL");
		return;
	}
}
@end
