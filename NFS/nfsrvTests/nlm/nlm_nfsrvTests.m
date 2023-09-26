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
#include <ftw.h>

#import <XCTest/XCTest.h>

#include "lockd.h"
#include "common.h"
#include "test_utils.h"
#include "mountd.h"
#include "nlm_rpc.h"

#define DSTDIR     "/System/Volumes/Data/private/tmp/nfsrvtest"
#define TEMPLATE    DSTDIR "/nfsrvtest.XXXXXXXX"
#define CONFIG      "nfs.conf"

char ltemplate[] = TEMPLATE;
char *lrootdir = NULL;
char lconfPath[PATH_MAX];
CLIENT *lclnt = NULL;
int lconfFD = -1, ldirFD = -1;
char hostname[1024] = {};
struct fhandle fh = {};
char *file = "new_file";

const char *conf_tcp_0 = "nfs.lockd.tcp=0";
const char *conf_udp_0 = "nfs.lockd.udp=0";
const char *conf_grace_1 = "nfs.lockd.grace_period=1";
const char *conf_grace_60 = "nfs.lockd.grace_period=60";
const char *conf_verbose_100 = "nfs.lockd.verbose=100";
const char *conf_send_using_tcp_1 = "nfs.lockd.send_using_tcp=1";

int lockd_imp(int argc, char *argv[], const char *conf_path);

static int
doLockdSetup(const char **confValues, int confValuesSize)
{
	// Create temporary folder name
	lrootdir = mkdtemp(ltemplate);
	if (!lrootdir) {
		XCTFail("Unable to create tmpdir: %d", errno);
		return -1;
	}

	if (chmod(ltemplate, 0777)) {
		XCTFail("Unable to chmod tmpdir (%s): %d", lrootdir, errno);
		return -1;
	}

	// Create temporary folder
	ldirFD = open(lrootdir, O_DIRECTORY | O_SEARCH, 0777);
	if (ldirFD < 0) {
		XCTFail("Unable to open tmpdir (%s): %d", lrootdir, errno);
		return -1;
	}

	lconfFD = openat(ldirFD, CONFIG, O_CREAT | O_RDWR | O_EXCL, 0666);
	if (lconfFD < 0) {
		XCTFail("Unable to create config (%s:/%s): %d", lrootdir, CONFIG, errno);
		return -1;
	}

	for (int i = 0; i < confValuesSize; i++) {
		dprintf(lconfFD, "%s\n", confValues[i]);
	}

	// Create conf exports path
	snprintf(lconfPath, sizeof(lconfPath), "%s/%s", lrootdir, CONFIG);

	if (fork() == 0) {
		int argc = 1;
		char **argv = calloc(argc, sizeof(char *));
		if (argv == NULL) {
			XCTFail("Cannot allocate argv array");
			return -1;
		}

		// Build args list
		argv[0] = "lockd";

		// Kick nfsd
		lockd_imp(argc, argv, lconfPath);
		free(argv);
	} else {
		// Sleep until lockd is up
		int retry = 10;
		while (retry-- && get_lockd_pid() == 0) {
			sleep(1);
		}
		if (retry == 0) {
			XCTFail("lockd did not start!");
		}
	}

	return 0;
}

static CLIENT *
createClientForNLMProtocolWithConf(int socketFamily, int socketType, int authType, int version, const char **confValues, int confValuesSize, int flags)
{
	const char *host;

	if (doLockdSetup(confValues, confValuesSize) < 0) {
		XCTFail("doLockdSetup failed");
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

	return createClientForProtocol(host, socketFamily, socketType, authType, NLM_PROG, version, flags, NULL);
}

static CLIENT *
createClientForNLMProtocol(int socketFamily, int socketType, int authType, int version)
{
	return createClientForNLMProtocolWithConf(socketFamily, socketType, authType, version, NULL, 0, 0);
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
doSetup(int socketFamily, int socketType, int version, int *dirFD, int *fileFD, const char **confValues, int confValuesSize)
{
	int err;
	char path[NAME_MAX] = {};

	if ((lclnt = createClientForNLMProtocolWithConf(socketFamily, socketType, RPCAUTH_UNIX, version, confValues, confValuesSize, 0)) == NULL) {
		XCTFail("Cannot create client");
		return -1;
	}

	doNFSSetUp(NULL, NULL, 0, socketFamily, socketType, RPCAUTH_UNIX, 0, NULL, "sys:krb5");

	err = createFileInPath(getLocalMountedPath(), file, dirFD, fileFD);
	if (err) {
		XCTFail("createFileInPath failed, got %d", err);
		return err;
	}
	snprintf(path, sizeof(path), "%s/%s", getLocalMountedPath(), file);

	err = getfh(path, &fh);
	if (err < 0) {
		XCTFail("fhopen failed. got %d errno %d\n", err, errno);
		return err;
	}

	return 0;
}

static void
initLock(int version, union nlm_alock_t *lock, struct fhandle *fhp, u_int64_t offset, u_int64_t len, uint64_t *owner)
{
	if (version == NLM_VERS4) {
		lock->lock4.caller_name = hostname;
		lock->lock4.fh.n_bytes = fhp->fh_data;
		lock->lock4.fh.n_len = fhp->fh_len;
		lock->lock4.oh.n_bytes = (uint8_t *)owner;
		lock->lock4.oh.n_len = sizeof(*owner);
		lock->lock4.svid = getpid();
		lock->lock4.l_offset = offset;
		lock->lock4.l_len = len;
	} else {
		lock->lock.caller_name = hostname;
		lock->lock.fh.n_bytes = fhp->fh_data;
		lock->lock.fh.n_len = fhp->fh_len;
		lock->lock.oh.n_bytes = (uint8_t *)owner;
		lock->lock.oh.n_len = sizeof(*owner);
		lock->lock.svid = getpid();
		lock->lock.l_offset = (u_int)offset;
		lock->lock.l_len = (u_int)len;
	}
}

static void
initShare(int version, union nlm_share_t *share, struct fhandle *fhp, fsh_mode mode, fsh_access access, uint64_t *owner)
{
	if (version == NLM_VERS4) {
		share->share4.caller_name = hostname;
		share->share4.fh.n_bytes = fhp->fh_data;
		share->share4.fh.n_len = fhp->fh_len;
		share->share4.oh.n_bytes = (uint8_t *)owner;
		share->share4.oh.n_len = sizeof(*owner);
		share->share4.mode = mode;
		share->share4.access = access;
	} else {
		share->share.caller_name = hostname;
		share->share.fh.n_bytes = fhp->fh_data;
		share->share.fh.n_len = fhp->fh_len;
		share->share.oh.n_bytes = (uint8_t *)owner;
		share->share.oh.n_len = sizeof(*owner);
		share->share.mode = mode;
		share->share.access = access;
	}
}

/*
 * 1. Setup IPv6, UDP connection with Unix authentication
 * 2. Send NULL to the server, make sure we got the reply
 */
static void
doNLMNull(int version)
{
	if (version != NLM_SM && version != NLM_VERS && version != NLM_VERSX && version != NLM_VERS4) {
		XCTFail("Got unsupported version %d", version);
		return;
	}

	if ((lclnt = createClientForNLMProtocol(AF_INET6, SOCK_DGRAM, RPCAUTH_UNIX, version)) == NULL) {
		XCTFail("Cannot create client");
	}

	doNLMNullRPC(version, lclnt);
}

/*
 * 1. Setup IPv4, UDP connection with Unix authentication
 * 2. Send TEST request to verify that file can be locked exclusively
 */
static void
doNLMTest(int version)
{
	int err, fileFD = -1, dirFD = -1, exclusive = 1;
	netobj cookie = { .n_bytes = (uint8_t *)"test", .n_len = 4 };
	union nlm_alock_t alock = {};
	uint64_t owner = 1;

	err = doSetup(AF_INET, SOCK_DGRAM, version, &dirFD, &fileFD, NULL, 0);
	if (err < 0) {
		XCTFail("doSetup failed. got %d\n", err);
		return;
	}

	initLock(version, &alock, &fh, 0, 0, &owner);

	doNLMTestRPC(version, nlm_granted, lclnt, &cookie, exclusive, &alock, NULL);

	removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Setup IPv6, TCP connection with Unix authentication
 * 2. Create local file
 * 3. Send LOCK request before grace period expires. verify nlm_denied_grace_period error is returned
 */
static void
doNLMLockGracePeriod(int version)
{
	int err, fileFD = -1, dirFD = -1, block = 0, exclusive = 1, reclaim = 0, state = 0x1234;
	netobj cookie = { .n_bytes = (uint8_t *)"test", .n_len = 4 };
	union nlm_alock_t alock = {};
	uint64_t owner = 1;
	const char *conf[1] = { conf_grace_60 };

	if (version != NLM_VERS && version != NLM_VERSX && version != NLM_VERS4) {
		XCTFail("Got unsupported version %d", version);
		return;
	}

	err = doSetup(AF_INET6, SOCK_STREAM, version, &dirFD, &fileFD, conf, ARRAY_SIZE(conf));
	if (err < 0) {
		XCTFail("doSetup failed. got %d\n", err);
		return;
	}

	initLock(version, &alock, &fh, 0, 1, &owner);

	doNLMLockRPC(version, nlm_denied_grace_period, lclnt, &cookie, block, exclusive, &alock, reclaim, state);

	removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Setup IPv4, TCP connection with Unix authentication
 * 2. Create local file
 * 3. Send LOCK request for the file using owner #1, verify lock is granted
 * 4. Send TEST request for the file using owner #2, verify nlm_denied error returned
 * 5. Send LOCK request for the file using owner #2, verify nlm_denied error returned
 * 6. Send UNLOCK request for the file using owner #1, verify nlm_granted error returned
 * 7. Send TEST request for the file using owner #1, verify nlm_granted error returned
 */
static void
doNLMLockTestUnlock(int version)
{
	int err, fileFD = -1, dirFD = -1, block = 0, exclusive = 1, reclaim = 0, state = 0x1234;
	netobj cookie = { .n_bytes = (uint8_t *)"test", .n_len = 4 };
	union nlm_alock_t alock = {}, alock2 = {};
	uint64_t owner = 1, owner2 = 2;
	const char *conf[1] = { conf_grace_1 };

	if (version != NLM_VERS && version != NLM_VERSX && version != NLM_VERS4) {
		XCTFail("Got unsupported version %d", version);
		return;
	}

	err = doSetup(AF_INET, SOCK_STREAM, version, &dirFD, &fileFD, conf, ARRAY_SIZE(conf));
	if (err < 0) {
		XCTFail("doSetup failed. got %d\n", err);
		return;
	}

	initLock(version, &alock, &fh, 0, 1, &owner);
	initLock(version, &alock2, &fh, 0, 1, &owner2);

	doNLMLockRPC(version, nlm_granted, lclnt, &cookie, block, exclusive, &alock, reclaim, state);
	doNLMTestRPC(version, nlm_denied, lclnt, &cookie, exclusive, &alock2, &alock);
	doNLMLockRPC(version, nlm_denied, lclnt, &cookie, block, exclusive, &alock2, reclaim, state);
	doNLMUnlockRPC(version, nlm_granted, lclnt, &cookie, &alock);
	doNLMTestRPC(version, nlm_granted, lclnt, &cookie, exclusive, &alock, NULL);

	removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Setup IPv4, TCP connection with Unix authentication
 * 2. Create local file
 * 3. Send shared LOCK request for the file using owner #1, verify lock is granted
 * 4. Send shared LOCK request for the file using owner #2, verify nlm_granted error returned
 * 5. Send exclusive LOCK request for the file using owner #3, verify nlm_denied error returned
 * 6. Send UNLOCK request for the file using owner #1, verify nlm_granted error returned
 * 7. Send UNLOCK request for the file using owner #2, verify nlm_granted error returned
 */
static void
doNLMSharedLock(int version)
{
	int err, fileFD = -1, dirFD = -1, block = 0, shared = 0, exclusive = 1, reclaim = 0, state = 0x1234;
	netobj cookie = { .n_bytes = (uint8_t *)"test", .n_len = 4 };
	union nlm_alock_t alock = {}, alock2 = {}, alock3 = {};
	uint64_t owner = 1, owner2 = 2, owner3 = 3;
	const char *conf[2] = { conf_grace_1, conf_verbose_100 };

	if (version != NLM_VERS && version != NLM_VERSX && version != NLM_VERS4) {
		XCTFail("Got unsupported version %d", version);
		return;
	}

	err = doSetup(AF_INET, SOCK_STREAM, version, &dirFD, &fileFD, conf, ARRAY_SIZE(conf));
	if (err < 0) {
		XCTFail("doSetup failed. got %d\n", err);
		return;
	}

	initLock(version, &alock, &fh, 0, 1, &owner);
	initLock(version, &alock2, &fh, 0, 1, &owner2);
	initLock(version, &alock3, &fh, 0, 1, &owner3);

	doNLMLockRPC(version, nlm_granted, lclnt, &cookie, block, shared, &alock, reclaim, state);
	doNLMLockRPC(version, nlm_granted, lclnt, &cookie, block, shared, &alock2, reclaim, state);
	doNLMLockRPC(version, nlm_denied, lclnt, &cookie, block, exclusive, &alock3, reclaim, state);
	doNLMUnlockRPC(version, nlm_granted, lclnt, &cookie, &alock);
	doNLMUnlockRPC(version, nlm_granted, lclnt, &cookie, &alock2);

	removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1.  Setup IPv4, TCP connection with Unix authentication
 * 2.  Create local file
 * 3.  Send exclusive LOCK request for the file using owner #1, verify lock is granted
 * 4.  Send exclusive-blocked LOCK request for the file using owner #2, verify nlm_blocked error returned
 * 5.  Send shared lock CANCEL request for the file using owner #1, verify nlm_denied error returned
 * 6.  Send exclusive lock CANCEL request for the file using owner #1, verify nlm_granted error returned
 * 7.  Send UNLOCK request for the file using owner #1, verify nlm_granted error returned
 * 8.  Sleep 1 second
 * 9.  Send exclusive LOCK request for the file using owner #1, verify lock is granted
 * 10. Send exclusive-blocked LOCK request for the file using owner #2, verify nlm_blocked error returned
 * 11. Send UNLOCK request for the file using owner #1, verify nlm_granted error returned
 */
static void
doNLMBlockedLock(int version)
{
	int err, block = 1, fileFD = -1, dirFD = -1, shared = 0, exclusive = 1, reclaim = 0, state = 0x1234;
	netobj cookie = { .n_bytes = (uint8_t *)"test", .n_len = 4 };
	union nlm_alock_t alock = {}, alock2 = {};
	uint64_t owner = 1, owner2 = 2;
	const char *conf[2] = { conf_send_using_tcp_1, conf_grace_1 };

	if (version != NLM_VERS && version != NLM_VERSX && version != NLM_VERS4) {
		XCTFail("Got unsupported version %d", version);
		return;
	}

	err = doSetup(AF_INET, SOCK_STREAM, version, &dirFD, &fileFD, conf, ARRAY_SIZE(conf));
	if (err < 0) {
		XCTFail("doSetup failed. got %d\n", err);
		return;
	}

	initLock(version, &alock, &fh, 0, 1, &owner);
	initLock(version, &alock2, &fh, 0, 1, &owner2);

	doNLMLockRPC(version, nlm_granted, lclnt, &cookie, block, exclusive, &alock, reclaim, state);
	doNLMLockRPC(version, nlm_blocked, lclnt, &cookie, block, exclusive, &alock2, reclaim, state);
	doNLMCancelRPC(version, nlm_denied, lclnt, &cookie, block, shared, &alock2);
	doNLMCancelRPC(version, nlm_granted, lclnt, &cookie, block, exclusive, &alock2);
	doNLMUnlockRPC(version, nlm_granted, lclnt, &cookie, &alock);
	sleep(1);
	doNLMLockRPC(version, nlm_granted, lclnt, &cookie, block, exclusive, &alock, reclaim, state);
	doNLMLockRPC(version, nlm_blocked, lclnt, &cookie, block, exclusive, &alock2, reclaim, state);
	doNLMUnlockRPC(version, nlm_granted, lclnt, &cookie, &alock);

	removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1.  Setup IPv4, UDP connection with Unix authentication
 * 2.  Create local file
 * 3.  Send deny write, write only SHARE request using owner #1, verify nlm_granted error returned
 * 4.  Send deny none, write only SHARE request using owner #2, verify nlm_denied error returned
 * 4.  Send deny none, read only SHARE request using owner #2, verify nlm_granted error returned
 * 5.  Send deny none, read only UNSHARE request using owner #2, verify nlm_granted error returned
 * 3.  Send deny write, write only UNSHARE request using owner #1, verify nlm_granted error returned
 */
static void
doNLMShareUnshare(int version)
{
	int err, fileFD = -1, dirFD = -1, reclaim = 0;
	netobj cookie = { .n_bytes = (uint8_t *)"test", .n_len = 4 };
	uint64_t owner = 1, owner2 = 2;
	union nlm_share_t share = {}, share2 = {}, share3 = {};
	const char *conf[2] = { conf_grace_1, conf_verbose_100 };

	if (version != NLM_VERSX && version != NLM_VERS4) {
		XCTFail("Got unsupported version %d", version);
		return;
	}

	err = doSetup(AF_INET, SOCK_DGRAM, version, &dirFD, &fileFD, conf, ARRAY_SIZE(conf));
	if (err < 0) {
		XCTFail("doSetup failed. got %d\n", err);
		return;
	}

	initShare(version, &share, &fh, fsm_DW, fsa_W, &owner);
	initShare(version, &share2, &fh, fsm_DN, fsa_W, &owner2);
	initShare(version, &share3, &fh, fsm_DN, fsa_R, &owner2);

	doNLMShareRPC(version, nlm_granted, lclnt, &cookie, &share, reclaim);
	doNLMShareRPC(version, nlm_denied, lclnt, &cookie, &share2, reclaim);
	doNLMShareRPC(version, nlm_granted, lclnt, &cookie, &share3, reclaim);
	doNLMUnshareRPC(version, nlm_granted, lclnt, &cookie, &share3, reclaim);
	doNLMUnshareRPC(version, nlm_granted, lclnt, &cookie, &share, reclaim);

	removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1.  Setup IPv6, TCP connection with Unix authentication
 * 2.  Create local file
 * 3.  Send deny write, write only SHARE request using owner #1, verify nlm_granted error returned
 * 4.  Send deny none, write only SHARE request using owner #2, verify nlm_denied error returned
 * 5.  Send FREEALL request, verify nlm_granted error returned
 * 4.  Send deny none, write only SHARE request using owner #2, verify nlm_granted error returned
 * 7.  Send FREEALL request, verify nlm_granted error returned
 */
static void
doNLMFreeAll(int version)
{
	int fileFD = -1, dirFD = -1, err, reclaim = 0;
	netobj cookie = { .n_bytes = (uint8_t *)"test", .n_len = 4 };
	uint64_t owner = 1, owner2 = 2;
	union nlm_share_t share = {}, share2 = {};
	const char *conf[2] = { conf_grace_1, conf_verbose_100 };

	if (version != NLM_VERSX && version != NLM_VERS4) {
		XCTFail("Got unsupported version %d", version);
		return;
	}

	err = doSetup(AF_INET6, SOCK_STREAM, version, &dirFD, &fileFD, conf, ARRAY_SIZE(conf));
	if (err < 0) {
		XCTFail("doSetup failed. got %d\n", err);
		return;
	}

	initShare(version, &share, &fh, fsm_DW, fsa_W, &owner);
	initShare(version, &share2, &fh, fsm_DN, fsa_W, &owner2);

	doNLMShareRPC(version, nlm_granted, lclnt, &cookie, &share, reclaim);
	doNLMShareRPC(version, nlm_denied, lclnt, &cookie, &share2, reclaim);
	doNLMFreeAllRPC(version, lclnt, hostname);
	doNLMShareRPC(version, nlm_granted, lclnt, &cookie, &share2, reclaim);
	doNLMFreeAllRPC(version, lclnt, hostname);

	removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

@interface nfsrvTests_nlm : XCTestCase

@end

@implementation nfsrvTests_nlm

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

	if (get_lockd_pid() > 0) {
		XCTFail("lockd should be stopped!");
	}

	gethostname(hostname, sizeof(hostname));
}

- (void)tearDown
{
	pid_t pid;

	doMountTearDown();

	if ((pid = get_lockd_pid()) != 0) {
		kill(pid, SIGTERM);
		// Wait for lockd termination
		int retry = 10;
		while (retry-- && get_lockd_pid() != 0) {
			sleep(1);
		}
		if (retry == 0) {
			XCTFail("lockd is still up!");
		}
	}

	if (lconfFD >= 0) {
		close(lconfFD);
		lconfFD = -1;
		unlink(lconfPath);
		bzero(lconfPath, sizeof(lconfPath));
	}

	if (ldirFD >= 0) {
		close(ldirFD);
		ldirFD = -1;
	}

	if (lrootdir) {
		rmdir(lrootdir);
		lrootdir = NULL;
	}

	rmdir(DSTDIR);

	if (lclnt) {
		clnt_destroy(lclnt);
		lclnt = NULL;
	}
}

/* --------- NLM_SM --------- */

- (void)testNLMSMNull
{
	doNLMNull(NLM_SM);
}

/* --------- NLMv1 --------- */

- (void)testNLMNull
{
	doNLMNull(NLM_VERS);
}

- (void)testNLMTest
{
	doNLMTest(NLM_VERS);
}

- (void)testNLMLockGracePeriod
{
	doNLMLockGracePeriod(NLM_VERS);
}

- (void)testNLMLockTestUnlock
{
	doNLMLockTestUnlock(NLM_VERS);
}

- (void)testNLMSharedLock
{
	doNLMSharedLock(NLM_VERS);
}

- (void)testNLMBlockedLock
{
	doNLMBlockedLock(NLM_VERS);
}

/* --------- NLMv3 --------- */

- (void)testNLMv3Null
{
	doNLMNull(NLM_VERSX);
}

- (void)testNLMv3Test
{
	doNLMTest(NLM_VERSX);
}

- (void)testNLMv3LockGracePeriod
{
	doNLMLockGracePeriod(NLM_VERSX);
}

- (void)testNLMv3LockTestUnlock
{
	doNLMLockTestUnlock(NLM_VERSX);
}

- (void)testNLMv3SharedLock
{
	doNLMSharedLock(NLM_VERSX);
}

- (void)testNLMv3BlockedLock
{
	doNLMBlockedLock(NLM_VERSX);
}

- (void)testNLMv3ShareUnshare
{
	doNLMShareUnshare(NLM_VERSX);
}

- (void)testNLMv3FreeAll
{
	doNLMFreeAll(NLM_VERSX);
}

/* --------- NLMv4 --------- */

- (void)testNLMv4Null
{
	doNLMNull(NLM_VERS4);
}

- (void)testNLMv4Test
{
	doNLMTest(NLM_VERS4);
}

- (void)testNLMv4LockGracePeriod
{
	doNLMLockGracePeriod(NLM_VERS4);
}

- (void)testNLMv4LockTestUnlock
{
	doNLMLockTestUnlock(NLM_VERS4);
}

- (void)testNLMv4SharedLock
{
	doNLMSharedLock(NLM_VERS4);
}

- (void)testNLMv4BlockedLock
{
	doNLMBlockedLock(NLM_VERS4);
}

- (void)testNLMv4ShareUnshare
{
	doNLMShareUnshare(NLM_VERS4);
}

- (void)testNLMv4FreeAll
{
	doNLMFreeAll(NLM_VERS4);
}

/*
 * 1. Setup IPv4, UDP connection while nfs.conf contains "nfs.lockd.udp=0"
 * 2. Verify NLM_VERS UDP CLIENT creation failed
 */
- (void)testNLMv4ConfigNoTransportUDP
{
	const char *conf[1] = { conf_udp_0 };

	if ((lclnt = createClientForNLMProtocolWithConf(AF_INET, SOCK_DGRAM, RPCAUTH_UNIX, NLM_VERS, conf, ARRAY_SIZE(conf), CREATE_CLIENT_FAILURE)) != NULL) {
		XCTFail("create should be NULL");
	}
}

/*
 * 1. Setup IPv6, TCP connection while nfs.conf contains "nfs.lockd.tcp=0"
 * 2. Verify NLM_VERSX TCP CLIENT creation failed
 */
- (void)testNLMv4ConfigNotransportTCP
{
	const char *conf[1] = { conf_tcp_0 };

	if ((lclnt = createClientForNLMProtocolWithConf(AF_INET6, SOCK_STREAM, RPCAUTH_UNIX, NLM_VERSX, conf, ARRAY_SIZE(conf), CREATE_CLIENT_FAILURE)) != NULL) {
		XCTFail("create should be NULL");
	}
}

/*
 * 1. Setup IPv6, TCP connection while nfs.conf "nfs.lockd.port"=44444"
 * 2. Verify NLM_VERS4 Lockd port is 44444 as expected
 */
- (void)testNLMv4ConfigPort
{
	const char buff[NAME_MAX] = {};
	const char *conf[1] = { buff };
	struct sockaddr_storage addr = {};
	int expected_port = 44444;
	snprintf((char *)buff, sizeof(buff), "%s=%d", "nfs.lockd.port", expected_port);

	if ((lclnt = createClientForNLMProtocolWithConf(AF_INET, SOCK_STREAM, RPCAUTH_UNIX, NLM_VERS4, conf, ARRAY_SIZE(conf), 0)) == NULL) {
		XCTFail("Cannot create client");
	} else {
		XCTAssertTrue(clnt_control(lclnt, CLGET_SERVER_ADDR, &addr));
	}

	XCTAssertEqual(expected_port, pmap_getport((struct sockaddr_in *)&addr, NLM_PROG, NLM_VERS4, IPPROTO_UDP));
	XCTAssertEqual(expected_port, pmap_getport((struct sockaddr_in *)&addr, NLM_PROG, NLM_VERS4, IPPROTO_TCP));
}

@end
