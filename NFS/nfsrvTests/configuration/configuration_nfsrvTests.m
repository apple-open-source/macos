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

#include <unistd.h>
#include <sysexits.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <oncrpc/auth.h>
#include <oncrpc/clnt.h>
#include <oncrpc/types.h>
#include <oncrpc/rpc_control.h>
#include <oncrpc/pmap_clnt.h>
#import <XCTest/XCTest.h>

#include "mountd.h"
#include "common.h"
#include "pathnames.h"
#include "mountd_rpc.h"
#include "nfs_prot_rpc.h"
#include "test_utils.h"

static CLIENT * nclnt = NULL;
static nfs_fh3 rootfh = {};

static int
doNFSSetUpWithArgsConfFlagsAndDoMount(const char **nfsdArgs, int nfsdArgsSize, const char **confValues, int confValuesSize, int mountSocketType, int mountFlags, int nfsSocketType, int nfsFlags, int doMount)
{
	int err;
	fhandle_t *fh;

	memset(&rootfh, 0, sizeof(rootfh));
	doMountSetUpWithArgs(nfsdArgs, nfsdArgsSize, confValues, confValuesSize);

	err = createClientForMountProtocol(AF_INET, mountSocketType, AUTH_UNIX, mountFlags);
	if (err) {
		if (mountFlags & CREATE_CLIENT_FAILURE) {
			return 0;
		}
		XCTFail("Cannot create client mount: %d", err);
		return -1;
	}

	nclnt = createClientForNFSProtocol(AF_INET, nfsSocketType, AUTH_UNIX, nfsFlags, NULL);
	if (nclnt == NULL) {
		if (nfsFlags & CREATE_CLIENT_FAILURE) {
			return 0;
		}
		XCTFail("Cannot create client for NFS");
		return -1;
	}

	if (doMount) {
		if ((fh = doMountAndVerify(getLocalMountedPath(), "sys:krb5")) == NULL) {
			XCTFail("doMountAndVerify failed");
			return -1;
		}

		rootfh.data.data_len = fh->fh_len;
		rootfh.data.data_val = (char *)fh->fh_data;
	}

	return 0;
}

static int
doNFSSetUpConf(int mountSocketType, int nfsSocketType)
{
	return doNFSSetUpWithArgsConfFlagsAndDoMount(NULL, 0, NULL, 0, mountSocketType, 0, nfsSocketType, 0, 1);
}

static int
doNFSSetUpWithFlagsAndDoMount(int mountSocketType, int mountFlags, int nfsSocketType, int nfsFlags, int doMount)
{
	return doNFSSetUpWithArgsConfFlagsAndDoMount(NULL, 0, NULL, 0, mountSocketType, nfsSocketType, nfsSocketType, nfsFlags, doMount);
}

static int
doNFSSetUpWithArgs(const char **nfsdArgs, int nfsdArgsSize, int mountSocketType, int nfsSocketType)
{
	return doNFSSetUpWithArgsConfFlagsAndDoMount(nfsdArgs, nfsdArgsSize, NULL, 0, mountSocketType, 0, nfsSocketType, 0, 1);
}

static int
doNFSSetUpWithArgsAndFlags(const char **nfsdArgs, int nfsdArgsSize, int mountSocketType, int mountFlags, int nfsSocketType, int nfsFlags)
{
	return doNFSSetUpWithArgsConfFlagsAndDoMount(nfsdArgs, nfsdArgsSize, NULL, 0, mountSocketType, mountFlags, nfsSocketType, nfsFlags, 1);
}

static int
doNFSSetUpWithConf(const char **confValues, int confValuesSize, int mountSocketType, int nfsSocketType)
{
	return doNFSSetUpWithArgsConfFlagsAndDoMount(NULL, 0, confValues, confValuesSize, mountSocketType, 0, nfsSocketType, 0, 1);
}

static int
doNFSSetUpWithConfAndFlags(const char **confValues, int confValuesSize, int mountSocketType, int mountFlags, int nfsSocketType, int nfsFlags)
{
	return doNFSSetUpWithArgsConfFlagsAndDoMount(NULL, 0, confValues, confValuesSize, mountSocketType, mountFlags, nfsSocketType, nfsFlags, 1);
}

static void
runNFSDwithConfigPath(int _argc, char *_argv[], const char *configPath, int expectedResult, int shouldSleep)
{
	int status = 0;
	pid_t childPid;

	if ((childPid = fork()) == 0) {
		/* Child process */
		nfsd_imp(_argc, _argv, configPath);
	} else {
		/* Now wait for the child to finish. */
		while (waitpid(childPid, &status, WUNTRACED) < 0) {
			if (errno == EINTR) {
				continue;
			}
		}
	}

	if (WIFEXITED(status)) {
		XCTAssertEqual(WEXITSTATUS(status), expectedResult, "runCommand failed");
		sleep(shouldSleep);
	} else if (WIFSIGNALED(status)) {
		XCTAssertEqual(WTERMSIG(status), 0, "runCommand subprocess terminated %s", strsignal(WTERMSIG(status)));
	} else if (WIFSTOPPED(status)) {
		XCTAssertEqual(WSTOPSIG(status), 0, "runCommand subprocess stopped %s", strsignal(WSTOPSIG(status)));
	} else {
		XCTFail("runCommand subprocess got unknow status: 0x%08x", status);
	}
}

static void
runNFSD(int _argc, char *_argv[], int expectedResult)
{
	runNFSDwithConfigPath(_argc, _argv, strnlen(confPath, PATH_MAX) ? confPath : _PATH_NFS_CONF, expectedResult, 1);
}

@interface nfsrvTests_configuration : XCTestCase

@end

@implementation nfsrvTests_configuration

- (void)tearDown
{
	memset(&rootfh, 0, sizeof(rootfh));
	doMountTearDown();

	if (nclnt) {
		clnt_destroy(nclnt);
		nclnt = NULL;
	}
}

// TODO: Run against the generated exports/nfs.conf files

/*
 * 1. Run nfsd with "-?" option. verify Usage is printed
 */
- (void)testOptionsUsage
{
	char *argv_usage[2] = { "nfsd", "-?" };

	runNFSD(ARRAY_SIZE(argv_usage), argv_usage, EX_USAGE);
}

/*
 * 1. Run nfsd with invalid option. verify Usage is printed
 */
- (void)testOptionsUsage2
{
	char *argv_usage[2] = { "nfsd", "blabla" };

	runNFSD(ARRAY_SIZE(argv_usage), argv_usage, EX_USAGE);
}

/*
 * 1. Run nfsd with invalid exports path. verify the exit status equals to EXIT_FAILURE
 */
- (void)testOptionsInvalidExports
{
	char *argv_checkexports[3] = { "nfsd", "-F", "/tmp/notfile111" };

	runNFSD(ARRAY_SIZE(argv_checkexports), argv_checkexports, EXIT_FAILURE);
}

/*
 * 1. Run nfsd with "checkexports" and "-vvvvvv" parameters. verify the exit status equals to EXIT_SUCCESS
 */
- (void)testOptionsCheckexports
{
	char *argv_checkexports[3] = { "nfsd", "-vvvvvv", "checkexports" };

	runNFSD(ARRAY_SIZE(argv_checkexports), argv_checkexports, EXIT_SUCCESS);
}

/*
 * 1. Run nfsd with "status" parameter and invalid nfs.conf path. verify the exit status equals to EXIT_FAILURE
 */
- (void)testOptionsInvalidConfig
{
	char *argv_status[2] = { "nfsd", "status" };

	runNFSDwithConfigPath(ARRAY_SIZE(argv_status), argv_status, "/tmp/noSuchNFS.conf", EXIT_FAILURE, 1);
}

/*
 * 1. Run nfsd with "status" parameter while status is disabled/loaded
 * 2. Run nfsd with "status" parameter while status is enabled/loaded
 * 3. Run nfsd with "status" parameter while status is enabled/unloaded
 * 4. Run nfsd with "status" parameter while status is disabled/unloaded
 */
- (void)testOptionsStatus
{
	char *argv_start[2] = { "nfsd", "start" };
	char *argv_stop[2] = { "nfsd", "stop" };
	char *argv_enable[2] = { "nfsd", "enable" };
	char *argv_disable[2] = { "nfsd", "disable" };
	char *argv_status[3] = { "nfsd", "-v", "status" };

	// disabled, loaded
	runNFSD(ARRAY_SIZE(argv_start), argv_start, EXIT_SUCCESS);
	XCTAssertTrue(nfsd_is_loaded());
	runNFSD(ARRAY_SIZE(argv_status), argv_status, EXIT_SUCCESS);

	// enabled, loaded
	runNFSD(ARRAY_SIZE(argv_enable), argv_enable, EXIT_SUCCESS);
	XCTAssertTrue(nfsd_is_enabled());
	runNFSD(ARRAY_SIZE(argv_status), argv_status, EXIT_SUCCESS);

	// enabled, unloaded
	runNFSD(ARRAY_SIZE(argv_stop), argv_stop, EXIT_SUCCESS);
	XCTAssertFalse(nfsd_is_loaded());
	runNFSD(ARRAY_SIZE(argv_status), argv_status, EXIT_FAILURE);

	// disabled, unloaded
	runNFSD(ARRAY_SIZE(argv_disable), argv_disable, EXIT_SUCCESS);
	XCTAssertFalse(nfsd_is_enabled());
	runNFSD(ARRAY_SIZE(argv_status), argv_status, EXIT_FAILURE);
}

/*
 * 1. Run nfsd with "start" parameter while status is unloaded
 * 2. Run nfsd with "start" parameter while status is loaded
 * 3. Run nfsd with "stop" parameter while status is loaded
 * 4. Run nfsd with "stop" parameter while status is unloaded
 */
- (void)testOptionsStartStop
{
	char *argv_start[2] = { "nfsd", "start" };
	char *argv_stop[2] = { "nfsd", "stop" };

	// start, unloaded
	runNFSD(ARRAY_SIZE(argv_start), argv_start, EXIT_SUCCESS);
	XCTAssertTrue(nfsd_is_loaded());

	// start, loaded
	runNFSD(ARRAY_SIZE(argv_start), argv_start, EXIT_SUCCESS);
	XCTAssertTrue(nfsd_is_loaded());

	// stop, loaded
	runNFSD(ARRAY_SIZE(argv_stop), argv_stop, EXIT_SUCCESS);
	XCTAssertFalse(nfsd_is_loaded());

	// stop, unloaded
	runNFSD(ARRAY_SIZE(argv_stop), argv_stop, EXIT_SUCCESS);
	XCTAssertFalse(nfsd_is_loaded());
}

/*
 * 1. Run nfsd with "start" parameter while status is unloaded/disabled
 * 2. Run nfsd with "disable" parameter while status is loaded/disabled
 */
- (void)testOptionsLoadedDisable
{
	char *argv_start[2] = { "nfsd", "start" };
	char *argv_disable[2] = { "nfsd", "disable" };

	// start, unloaded
	runNFSD(ARRAY_SIZE(argv_start), argv_start, EXIT_SUCCESS);
	XCTAssertTrue(nfsd_is_loaded());

	// disable, loaded
	runNFSD(ARRAY_SIZE(argv_disable), argv_disable, EXIT_SUCCESS);
	XCTAssertFalse(nfsd_is_enabled());

	// Verify unloaded
	XCTAssertFalse(nfsd_is_loaded());
}

/*
 * 1. Run nfsd with "enable" parameter while status is disabled
 * 2. Run nfsd with "enable" parameter while status is enabled
 * 3. Run nfsd with "disable" parameter while status is enabled
 * 4. Run nfsd with "disable" parameter while status is disabled
 */
- (void)testOptionsEnableDisable
{
	char *argv_enable[2] = { "nfsd", "enable" };
	char *argv_disable[2] = { "nfsd", "disable" };

	// enable, disabled
	runNFSD(ARRAY_SIZE(argv_enable), argv_enable, EXIT_SUCCESS);
	XCTAssertTrue(nfsd_is_enabled());

	// enable, enabled
	runNFSD(ARRAY_SIZE(argv_enable), argv_enable, EXIT_SUCCESS);
	XCTAssertTrue(nfsd_is_enabled());

	// disable, enabled
	runNFSD(ARRAY_SIZE(argv_disable), argv_disable, EXIT_SUCCESS);
	XCTAssertFalse(nfsd_is_enabled());

	// disable, disabled
	runNFSD(ARRAY_SIZE(argv_disable), argv_disable, EXIT_SUCCESS);
	XCTAssertFalse(nfsd_is_enabled());
}

/*
 * 1. Run nfsd with "enable" parameter while status is disabled (don't sleep after command execution)
 * 2. Run nfsd with "enable" parameter while status is enabled (don't sleep after command execution)
 * 3. Run nfsd with "disable" parameter while status is enabled (don't sleep after command execution)
 * 4. Run nfsd with "disable" parameter while status is disabled (don't sleep after command execution)
 */
- (void)testOptionsEnableDisableNoSleep
{
	char *argv_enable[2] = { "nfsd", "enable" };
	char *argv_disable[2] = { "nfsd", "disable" };

	// enable, disabled
	runNFSDwithConfigPath(ARRAY_SIZE(argv_enable), argv_enable, confPath, EXIT_SUCCESS, 0);
	XCTAssertTrue(nfsd_is_enabled());

	// enable, enabled
	runNFSDwithConfigPath(ARRAY_SIZE(argv_enable), argv_enable, confPath, EXIT_SUCCESS, 0);
	XCTAssertTrue(nfsd_is_enabled());

	// disable, enabled
	runNFSDwithConfigPath(ARRAY_SIZE(argv_disable), argv_disable, confPath, EXIT_SUCCESS, 0);
	XCTAssertFalse(nfsd_is_enabled());

	// disable, disabled
	runNFSDwithConfigPath(ARRAY_SIZE(argv_disable), argv_disable, confPath, EXIT_SUCCESS, 0);
	XCTAssertFalse(nfsd_is_enabled());
}

/*
 * 1. Run nfsd with "restart" parameter while status is unloaded
 * 2. Run nfsd with "restart" parameter while status is loaded
 * 3. Verify pid has changed
 */
- (void)testOptionsRestart
{
	pid_t pid1, pid2;
	char *argv_stop[2] = { "nfsd", "stop" };
	char *argv_restart[2] = { "nfsd", "restart" };

	// restart, unloaded
	runNFSD(ARRAY_SIZE(argv_restart), argv_restart, EXIT_SUCCESS);
	XCTAssertTrue(nfsd_is_loaded());

	pid1 = get_nfsd_pid();
	XCTAssertNotEqual(pid1, 0);

	// restart, loaded
	runNFSD(ARRAY_SIZE(argv_restart), argv_restart, EXIT_SUCCESS);
	XCTAssertTrue(nfsd_is_loaded());

	pid2 = get_nfsd_pid();
	XCTAssertNotEqual(pid2, 0);

	// Verify pid has changed
	XCTAssertNotEqual(pid1, pid2);

	// stop, loaded
	runNFSD(ARRAY_SIZE(argv_stop), argv_stop, EXIT_SUCCESS);
	XCTAssertFalse(nfsd_is_loaded());
}

/*
 * 1. Setup TCP based NFS mount
 * 2. Send FSInfo request, verify we get a valid reply
 * 3. Erase exports file
 * 4. Send FSInfo request, verify we get a valid reply
 * 5. Run nfsd with "update" parameter to reload the export content
 * 6. Send FSInfo request using the primary root fh, verify ESTALE is returned
 */
- (void)testOptionsUpdate
{
	int fd;
	FSINFO3res *res;
	char *argv_update[2] = { "nfsd", "update" };

	doNFSSetUpConf( SOCK_STREAM, SOCK_STREAM);

	res = doFSinfoRPC(nclnt, &rootfh);
	if (res->status != NFS3_OK) {
		XCTFail("doFSinfoRPC failed, got %d", res->status);
	}

	fd = open(exportsPath, O_TRUNC | O_WRONLY);
	XCTAssertTrue(fd >= 0);

	res = doFSinfoRPC(nclnt, &rootfh);
	if (res->status != NFS3_OK) {
		XCTFail("doFSinfoRPC failed, got %d", res->status);
	}

	runNFSD(ARRAY_SIZE(argv_update), argv_update, EXIT_SUCCESS);

	res = doFSinfoRPC(nclnt, &rootfh);
	if (res->status != NFS3ERR_STALE) {
		XCTFail("doFSinfoRPC failed, expected status is %d, got %d", NFS3ERR_STALE, res->status);
	}

	close(fd);
}

/*
 * 1. Setup TCP based NFS mount
 * 2. Run nfsd with "verbose" parameter to increase/decrease verbosity level
 */
- (void)testOptionsVerboseUpDown
{
	char *argv_verbose_up[4] = { "nfsd", "verbose", "up", "up"};
	char *argv_verbose_down[5] = { "nfsd", "verbose", "down", "down", "down"};
	char *argv_verbose_unknown[3] = { "nfsd", "verbose", "unknown"};

	doNFSSetUpConf(SOCK_STREAM, SOCK_STREAM);

	runNFSD(ARRAY_SIZE(argv_verbose_up), argv_verbose_up, EXIT_SUCCESS);
	runNFSD(ARRAY_SIZE(argv_verbose_unknown), argv_verbose_unknown, EXIT_FAILURE);
	runNFSD(ARRAY_SIZE(argv_verbose_down), argv_verbose_down, EXIT_SUCCESS);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

/*
 * 1. Change user from root to user 99 and group 99
 * 2. Run nfsd with "start" parameter, verify the exit status equals to EX_NOPERM
 */
- (void)testOptionsUnprivilegedStart
{
	int err;
	char *argv_start[2] = { "nfsd", "start" };

	/* switch to the user from root */
	err = pthread_setugid_np(UNKNOWNUID, UNKNOWNGID);
	if (err != 0) {
		XCTFail("pthread_setugid_np failed for uid %d, gid %d", UNKNOWNUID, UNKNOWNGID);
	}

	runNFSD(ARRAY_SIZE(argv_start), argv_start, EX_NOPERM);

	/* and back to being root */
	err = pthread_setugid_np(KAUTH_UID_NONE, KAUTH_GID_NONE);
	if (err != 0) {
		XCTFail("pthread_setugid_np failed for root");
	}
}

#pragma clang diagnostic pop

/*
 * 1.  * 1. Setup TCP based NFS mount using "-vvvvvvvvvv" parameter.
 */
- (void)testOptionsVerbose
{
	const char *argv[] = { "-vvvvvvvvvv" };

	doNFSSetUpWithArgs(argv, ARRAY_SIZE(argv), SOCK_STREAM, SOCK_STREAM);
}

/*
 * 1. Setup nfsd using "-u" parameter to disable TCP connections
 * 2. Verify TCP based MOUNT client cannot be created
 * 3. Verify TCP based NFS client cannot be created
 * 4. Verify UDP connection is allowed by sending NULL request to the server, make usre we got the reply
 */
- (void)testOptionsUDP
{
	const char *argv[] = { "-u" };

	doNFSSetUpWithArgsAndFlags(argv, ARRAY_SIZE(argv), SOCK_STREAM, CREATE_CLIENT_FAILURE, SOCK_DGRAM, 0);
	doNFSSetUpWithArgsAndFlags(argv, ARRAY_SIZE(argv), SOCK_DGRAM, 0, SOCK_STREAM, CREATE_CLIENT_FAILURE);

	doMountTearDown();
	doNFSSetUpWithArgs(argv, ARRAY_SIZE(argv), SOCK_DGRAM, SOCK_DGRAM);
}

/*
 * 1. Setup nfsd using "-t" parameter to disable UDP connections
 * 2. Verify UDP based MOUNT client cannot be created
 * 3. Verify UDP based NFS client cannot be created
 * 4. Verify TCP connection is allowed by sending NULL request to the server, make usre we got the reply
 */
- (void)testOptionsTCP
{
	const char *argv[] = { "-t" };

	doNFSSetUpWithArgsAndFlags(argv, ARRAY_SIZE(argv), SOCK_DGRAM, CREATE_CLIENT_FAILURE, SOCK_STREAM, 0);
	doMountTearDown();

	doNFSSetUpWithArgsAndFlags(argv, ARRAY_SIZE(argv), SOCK_STREAM, 0, SOCK_DGRAM, CREATE_CLIENT_FAILURE);
	doMountTearDown();

	doNFSSetUpWithArgs(argv, ARRAY_SIZE(argv), SOCK_STREAM, SOCK_STREAM);
}

/*
 * 1. Setup nfsd using "-n" parameter to modify the amount of threads
 * 2. Verify the expected amount using sysctl_get
 */
- (void)testOptionsThreads
{
	int threads = -1, expected_threads = 4;
	const char *argv[1] = { "-n 4" };
	doNFSSetUpWithArgs(argv, ARRAY_SIZE(argv), SOCK_STREAM, SOCK_STREAM);

	XCTAssertFalse(sysctl_get("vfs.generic.nfs.server.nfsd_thread_count", &threads));
	XCTAssertEqual(threads, expected_threads);
}

/*
 * 1. Setup nfsd using "-R" parameter to allow mounting regular files
 * 2. Create local file
 * 3. Send MOUNT request to the file. verify MNT3_OK is returned
 */
- (void)testOptionsMountRegularFiles
{
	const char *argv[1] = { "-R" };
	char path[NAME_MAX] = {}, *ppath = path;
	int dirFD, fileFD, err;
	char *file = "new_file";
	mountres3 *result;

	doNFSSetUpWithArgsConfFlagsAndDoMount(argv, ARRAY_SIZE(argv), NULL, 0, SOCK_DGRAM, 0, SOCK_DGRAM, 0, 0);

	err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
	if (err) {
		XCTFail("createFileInPath failed, got %d", err);
		return;
	}
	snprintf(path, sizeof(path), "%s/%s", getLocalMountedPath(), file);

	result = mountproc3_mnt_3(&ppath, mclnt);
	XCTAssertNotEqual(NULL, result);
	XCTAssertEqual(MNT3_OK, result->fhs_status);

	removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Setup nfsd while mounting regular files is not allowed
 * 2. Create local file
 * 3. Send MOUNT request to the file. verify MNT3_OK is MNT3ERR_ACCES
 */
- (void)testOptionsMountRegularFilesFailure
{
	char path[NAME_MAX] = {}, *ppath = path;
	int dirFD, fileFD, err;
	char *file = "new_file";
	mountres3 *result;

	doNFSSetUpWithFlagsAndDoMount(SOCK_STREAM, 0, SOCK_STREAM, 0, 0);

	err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
	if (err) {
		XCTFail("createFileInPath failed, got %d", err);
		return;
	}
	snprintf(path, sizeof(path), "%s/%s", getLocalMountedPath(), file);

	result = mountproc3_mnt_3(&ppath, mclnt);
	XCTAssertNotEqual(NULL, result);
	XCTAssertEqual(MNT3ERR_ACCES, result->fhs_status);

	removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Setup nfsd using "-P" in order to use specific mount port (22222)
 * 2. Verify MOUNT port is 22222 as expected
 */
- (void)testOptionsMountPort
{
	uint16_t expected_port = 22222;
	const char *argv[1] = { "-P 22222" };
	struct sockaddr_storage addr = {};

	doNFSSetUpWithArgs(argv, ARRAY_SIZE(argv), SOCK_DGRAM, SOCK_DGRAM);
	XCTAssertTrue(clnt_control(nclnt, CLGET_SERVER_ADDR, &addr));

	XCTAssertEqual(expected_port, pmap_getport((struct sockaddr_in *)&addr, MOUNT_PROGRAM, MOUNT_V3, IPPROTO_UDP));
	XCTAssertEqual(expected_port, pmap_getport((struct sockaddr_in *)&addr, MOUNT_PROGRAM, MOUNT_V3, IPPROTO_TCP));
}

/*
 * 1. Setup nfsd using a socket which created locally (not using oncrpc), so it is not bounded to a reserved port
 * 2. Verify MOUNT/UMOUNT/UNMOUNTALL requested returns null
 */
- (void)testOptionsMountReservePortFailure
{
	mountres3 *result;
	const char *path;

	doNFSSetUpWithFlagsAndDoMount(SOCK_DGRAM, CREATE_SOCKET, SOCK_DGRAM, 0, 0);
	path = getLocalMountedPath();

	result = mountproc3_mnt_3((void*)&path, mclnt);
	XCTAssertEqual(NULL, result);

	result = mountproc3_umnt_3((void*)&path, mclnt);
	XCTAssertEqual(NULL, result);

	result = mountproc3_umntall_3(&result, mclnt);
	XCTAssertEqual(NULL, result);

	clnt_destroy(mclnt);
	mclnt = NULL;
}

/*
 * 1. Setup nfsd using "-N" parameter to allowing mounting to a non-reserved port
 * 2. Use socket which created locally (not using oncrpc), so it is not bounded to a reserved port
 * 3. Verify MOUNT succeeds
 */
- (void)testOptionsMountReservePort
{
	const char *argv[] = { "-N" };

	doNFSSetUpWithArgsAndFlags(argv, ARRAY_SIZE(argv), SOCK_DGRAM, CREATE_SOCKET, SOCK_DGRAM, 0);
}

/*
 * 1. Setup nfsd using "-p" in order to use specific  port (11111)
 * 2. Verify NFS port is 11111 as expected
 */
- (void)testOptionsNFSPort
{
	uint16_t expected_port = 11111;
	const char *argv[1] = { "-p 11111" };
	struct sockaddr_storage addr = {};

	doNFSSetUpWithArgs(argv, ARRAY_SIZE(argv), SOCK_STREAM, SOCK_STREAM);
	XCTAssertTrue(clnt_control(nclnt, CLGET_SERVER_ADDR, &addr));

	XCTAssertEqual(expected_port, pmap_getport((struct sockaddr_in *)&addr, NFS_PROGRAM, NFS_V3, IPPROTO_UDP));
	XCTAssertEqual(expected_port, pmap_getport((struct sockaddr_in *)&addr, NFS_PROGRAM, NFS_V3, IPPROTO_TCP));
}

/*
 * 1. Setup TCP based NFS mount
 * 2. Send FSInfo request, verify we get a valid reply
 * 3. Erase exports file
 * 4. Send FSInfo request, verify we get a valid reply
 * 5. Run nfsd with "-r" parameter to reload the export content
 * 6. Send FSInfo request using the primary root fh, verify ESTALE is returned
 */
- (void)testOptionsReregister
{
	int fd;
	FSINFO3res *res;
	char *argv_reregister[2] = { "nfsd", "-r" };

	doNFSSetUpConf(SOCK_STREAM, SOCK_STREAM);

	res = doFSinfoRPC(nclnt, &rootfh);
	if (res->status != NFS3_OK) {
		XCTFail("doFSinfoRPC failed, got %d", res->status);
	}

	fd = open(exportsPath, O_TRUNC | O_WRONLY);
	XCTAssertTrue(fd >= 0);

	res = doFSinfoRPC(nclnt, &rootfh);
	if (res->status != NFS3_OK) {
		XCTFail("doFSinfoRPC failed, got %d", res->status);
	}

	runNFSD(ARRAY_SIZE(argv_reregister), argv_reregister, EXIT_SUCCESS);

	res = doFSinfoRPC(nclnt, &rootfh);
	if (res->status != NFS3ERR_STALE) {
		XCTFail("doFSinfoRPC failed, expected status is %d, got %d", NFS3ERR_STALE, res->status);
	}

	close(fd);
}

/*
 * 1. Setup nfsd while nfs.conf contains "nfs.server.tcp=0" and "nfs.server.udp=0"
 * 2. Verify TCP CLIENT creation failed
 */
- (void)testConfigNotransportsTCP
{
	int disable = 0;
	const char buff[NAME_MAX] = {}, buff2[NAME_MAX] = {};
	const char *conf[2] = { buff, buff2 };
	snprintf((char *)buff, sizeof(buff), "%s=%d", "nfs.server.tcp", disable);
	snprintf((char *)buff2, sizeof(buff2), "%s=%d", "nfs.server.udp", disable);

	doNFSSetUpWithConfAndFlags(conf, ARRAY_SIZE(conf), SOCK_STREAM, CREATE_CLIENT_FAILURE, SOCK_STREAM, CREATE_CLIENT_FAILURE);
}

/*
 * 1. Setup nfsd while nfs.conf contains "nfs.server.tcp=0" and "nfs.server.udp=0"
 * 2. Verify UDP CLIENT creation failed
 */
- (void)testConfigNotransportsUDP
{
	int disable = 0;
	const char buff[NAME_MAX] = {}, buff2[NAME_MAX] = {};
	const char *conf[2] = { buff, buff2 };
	snprintf((char *)buff, sizeof(buff), "%s=%d", "nfs.server.tcp", disable);
	snprintf((char *)buff2, sizeof(buff2), "%s=%d", "nfs.server.udp", disable);

	doNFSSetUpWithConfAndFlags(conf, ARRAY_SIZE(conf), SOCK_DGRAM, CREATE_CLIENT_FAILURE, SOCK_DGRAM, CREATE_CLIENT_FAILURE);
}

/*
 * 1. Setup nfsd while nfs.conf contains "nfs.server.mount.port"=33333"
 * 2. Verify MOUNT port is 33333 as expected
 */
- (void)testConfigMountPort
{
	const char buff[NAME_MAX] = {};
	const char *conf[1] = { buff };
	int expected_port = 33333;
	struct sockaddr_storage addr = {};
	snprintf((char *)buff, sizeof(buff), "%s=%d", "nfs.server.mount.port", expected_port);

	doNFSSetUpWithConf(conf, ARRAY_SIZE(conf), SOCK_DGRAM, SOCK_DGRAM);
	XCTAssertTrue(clnt_control(nclnt, CLGET_SERVER_ADDR, &addr));

	XCTAssertEqual(expected_port, pmap_getport((struct sockaddr_in *)&addr, MOUNT_PROGRAM, MOUNT_V3, IPPROTO_UDP));
	XCTAssertEqual(expected_port, pmap_getport((struct sockaddr_in *)&addr, MOUNT_PROGRAM, MOUNT_V3, IPPROTO_TCP));
}

/*
 * 1. Setup nfsd while nfs.conf contains "nfs.server.mount.require_resv_port"=1" using a socket which created locally (not using oncrpc), so it is not bounded to a reserved port
 * 2. Verify MOUNT/UMOUNT/UNMOUNTALL requested returns null
 */
- (void)testConfigMountReservePort
{
	void *result;
	const char *path;
	const char buff[NAME_MAX] = {};
	const char *conf[1] = { buff };
	int require_resv_port = 1;
	snprintf((char *)buff, sizeof(buff), "%s=%d", " nfs.server.mount.require_resv_port", require_resv_port);

	doNFSSetUpWithArgsConfFlagsAndDoMount(NULL, 0, conf, ARRAY_SIZE(conf), SOCK_DGRAM, CREATE_SOCKET, SOCK_DGRAM, 0, 0);
	path = getLocalMountedPath();

	result = mountproc3_mnt_3((void*)&path, mclnt);
	XCTAssertEqual(NULL, result);

	result = mountproc3_umnt_3((void*)&path, mclnt);
	XCTAssertEqual(NULL, result);

	result = mountproc3_umntall_3(&result, mclnt);
	XCTAssertEqual(NULL, result);

	clnt_destroy(mclnt);
	mclnt = NULL;
}

/*
 * 1. Setup nfsd while nfs.conf contains "nfs.server.port"=44444"
 * 2. Verify NFS port is 44444 as expected
 */
- (void)testConfigNFSPort
{
	const char buff[NAME_MAX] = {};
	const char *conf[1] = { buff };
	struct sockaddr_storage addr = {};
	int expected_port = 44444;
	snprintf((char *)buff, sizeof(buff), "%s=%d", "nfs.server.port", expected_port);

	doNFSSetUpWithConf(conf, ARRAY_SIZE(conf), SOCK_STREAM, SOCK_STREAM);
	XCTAssertTrue(clnt_control(nclnt, CLGET_SERVER_ADDR, &addr));

	XCTAssertEqual(expected_port, pmap_getport((struct sockaddr_in *)&addr, NFS_PROGRAM, NFS_V3, IPPROTO_UDP));
	XCTAssertEqual(expected_port, pmap_getport((struct sockaddr_in *)&addr, NFS_PROGRAM, NFS_V3, IPPROTO_TCP));
}

/*
 * 1. Setup nfsd while nfs.conf contains "nfs.server.require_resv_port"=1" using a socket which created locally (not using oncrpc), so it is not bounded to a reserved port
 * 2. Verify FSInfo requested returns null
 */
- (void)testConfigNFSReservePortFailure
{
	FSINFO3args args = { .fsroot = rootfh };
	FSINFO3res *result = NULL;

	const char buff[NAME_MAX] = {};
	const char *conf[1] = { buff };
	int require_resv_port = -1, expected_require_resv_port = 1;
	snprintf((char *)buff, sizeof(buff), "%s=%d", "nfs.server.require_resv_port", expected_require_resv_port);

	doNFSSetUpWithConfAndFlags(conf, ARRAY_SIZE(conf), SOCK_DGRAM, 0, SOCK_DGRAM, CREATE_SOCKET);

	XCTAssertFalse(sysctl_get("vfs.generic.nfs.server.require_resv_port", &require_resv_port));
	XCTAssertEqual(require_resv_port, expected_require_resv_port);

	result = nfsproc3_fsinfo_3((void*)&args, nclnt);
	XCTAssertEqual(NULL, result);
}

/*
 * 1. Setup nfsd while nfs.conf contains "nfs.server.async=1"
 * 2. Verify the expected amount using sysctl_get
 */
- (void)testConfigASync
{
	const char buff[NAME_MAX] = {};
	const char *conf[1] = { buff };
	int async = -1, expected_async = 1;
	snprintf((char *)buff, sizeof(buff), "%s=%d", "nfs.server.async", expected_async);

	doNFSSetUpWithConf(conf, ARRAY_SIZE(conf), SOCK_STREAM, SOCK_STREAM);

	XCTAssertFalse(sysctl_get("vfs.generic.nfs.server.async", &async));
	XCTAssertEqual(async, expected_async);
}

/*
 * 1. Setup nfsd while nfs.conf contains "nfs.server.fsevents=1"
 * 2. Verify the expected amount using sysctl_get
 */
- (void)testConfigFSEvents
{
	const char buff[NAME_MAX] = {};
	const char *conf[1] = { buff };
	int fsevents = -1, expected_fsevents = 1;
	snprintf((char *)buff, sizeof(buff), "%s=%d", "nfs.server.fsevents", expected_fsevents);

	doNFSSetUpWithConf(conf, ARRAY_SIZE(conf), SOCK_STREAM, SOCK_STREAM);

	XCTAssertFalse(sysctl_get("vfs.generic.nfs.server.fsevents", &fsevents));
	XCTAssertEqual(fsevents, expected_fsevents);
}

/*
 * 1. Setup nfsd while nfs.conf contains "nfs.server.user_stats=2"
 * 2. Verify the expected amount using sysctl_get
 */
- (void)testConfigUserStats
{
	const char buff[NAME_MAX] = {};
	const char *conf[1] = { buff };
	int user_stats = -1, expected_user_stats = 2;
	snprintf((char *)buff, sizeof(buff), "%s=%d", "nfs.server.user_stats", expected_user_stats);

	doNFSSetUpWithConf(conf, ARRAY_SIZE(conf), SOCK_STREAM, SOCK_STREAM);

	XCTAssertFalse(sysctl_get("vfs.generic.nfs.server.user_stats", &user_stats));
	XCTAssertEqual(user_stats, expected_user_stats);
}

/*
 * 1. Setup nfsd while nfs.conf contains "nfs.server.bonjour=1" and "nfs.server.bonjour_local_domain_only=1"
 */
- (void)testConfigBonjour
{
	const char buff[NAME_MAX] = {}, buff2[NAME_MAX] = {};
	const char *conf[2] = { buff, buff2 };
	int bonjour = 1, bonjour_local_domain_only = 1;
	snprintf((char *)buff, sizeof(buff), "%s=%d", "nfs.server.bonjour", bonjour);
	snprintf((char *)buff2, sizeof(buff2), "%s=%d", "nfs.server.bonjour_local_domain_only", bonjour_local_domain_only);

	doNFSSetUpWithConf(conf, ARRAY_SIZE(conf), SOCK_STREAM, SOCK_STREAM);
}

/*
 * 1. Setup TCP based NFS mount
 * 2. Terminate MOUNT client
 * 2. Edit nfs.conf with "nfs.server.reqcache_size", "nfs.server.export_hash_size" and "nfs.server.mount.port"
 * 3. Run nfsd with "update" parameter to reload the export content
 * 4. Make sure NFSD is not running
 */
- (void)testConfigRequiredUpdate
{
	mountres3 *result;
	char *argv_update[2] = { "nfsd", "update" };
	int confFD, port = 44444, reqcache = 128, export_hash_size = 128;

	doNFSSetUpConf(SOCK_STREAM, SOCK_STREAM);

	// Terminate MOUNT client
	result = mountproc3_umntall_3(&result, mclnt);
	XCTAssertNotEqual(NULL, result);
	XCTAssertEqual(MNT3_OK, result->fhs_status);
	clnt_destroy(mclnt);
	mclnt = NULL;

	// Update config file
	confFD = open(confPath, O_RDWR | O_TRUNC);
	if (confFD < 0) {
		XCTFail("Unable to open config (%s): %d", confPath, errno);
	}
	dprintf(confFD, "%s=%d\n", "nfs.server.reqcache_size", reqcache);
	dprintf(confFD, "%s=%d\n", "nfs.server.export_hash_size", export_hash_size);
	dprintf(confFD, "%s=%d\n", "nfs.server.mount.port", port);
	close(confFD);

	// Update NFSD
	runNFSD(ARRAY_SIZE(argv_update), argv_update, EXIT_SUCCESS);

	sleep(1);

	// Make sure NFSD is not running
	XCTAssertEqual(0, get_nfsd_pid());
}

/*
 * 1. Setup nfsd while nfs.conf contains "nfs.server.export_hash_size=128"
 * 2. Verify the expected amount using sysctl_get
 */
- (void)testConfigExportHashSize
{
	const char buff[NAME_MAX] = {};
	const char *conf[1] = { buff };
	int export_hash_size = -1, expected_export_hash_size = 128;
	snprintf((char *)buff, sizeof(buff), "%s=%d", "nfs.server.export_hash_size", expected_export_hash_size);

	doNFSSetUpWithConf(conf, ARRAY_SIZE(conf), SOCK_STREAM, SOCK_STREAM);

	XCTAssertFalse(sysctl_get("vfs.generic.nfs.server.export_hash_size", &export_hash_size));
	XCTAssertEqual(export_hash_size, expected_export_hash_size);
}

/*
 * 1. Setup nfsd while nfs.conf contains "nfs.server.nfsd_threads=-1"
 * 2. Verify the expected (default) amount using sysctl_get
 */
- (void)testConfigThreadsNegative
{
	const char buff[NAME_MAX] = {};
	const char *conf[1] = { buff };
	int threads = -1;
	snprintf((char *)buff, sizeof(buff), "%s=%d", "nfs.server.nfsd_threads", -1);

	doNFSSetUpWithConf(conf, ARRAY_SIZE(conf), SOCK_STREAM, SOCK_STREAM);

	XCTAssertFalse(sysctl_get("vfs.generic.nfs.server.nfsd_thread_max", &threads));
	XCTAssertEqual(threads, config_defaults.nfsd_threads);
}

/*
 * 1. Setup nfsd while nfs.conf contains "nfs.server.nfsd_threads=MAX_NFSD_THREADS_HARD + 1"
 * 2. Verify the expected (MAX_NFSD_THREADS_HARD) amount using sysctl_get
 */
- (void)testConfigThreadsHard
{
	const char buff[NAME_MAX] = {};
	const char *conf[1] = { buff };
	int threads = -1;
	snprintf((char *)buff, sizeof(buff), "%s=%d", "nfs.server.nfsd_threads", MAX_NFSD_THREADS_HARD + 1);

	doNFSSetUpWithConf(conf, ARRAY_SIZE(conf), SOCK_STREAM, SOCK_STREAM);

	XCTAssertFalse(sysctl_get("vfs.generic.nfs.server.nfsd_thread_max", &threads));
	XCTAssertEqual(threads, MAX_NFSD_THREADS_HARD);
}

/*
 * 1. Setup TCP based NFS mount
 * 2. Edit nfs.conf with "nfs.server.nfsd_threads=10"
 * 3. Run nfsd with "update" parameter to reload the export content
 * 4. Verify the expected (default) amount using sysctl_get
 */
- (void)testConfigUpdateThreads
{
	char *argv_update[2] = { "nfsd", "update" };
	int confFD, threads = -1, expected_threads = 10;

	doNFSSetUpConf(SOCK_STREAM, SOCK_STREAM);

	// Update config file
	confFD = open(confPath, O_RDWR | O_TRUNC);
	if (confFD < 0) {
		XCTFail("Unable to open config (%s): %d", confPath, errno);
	}
	dprintf(confFD, "%s=%d\n", "nfs.server.nfsd_threads", expected_threads);
	close(confFD);

	// Update NFSD
	runNFSD(ARRAY_SIZE(argv_update), argv_update, EXIT_SUCCESS);

	sleep(1);

	// Make sure NFSD is not running
	XCTAssertFalse(sysctl_get("vfs.generic.nfs.server.nfsd_thread_max", &threads));
	XCTAssertEqual(expected_threads, threads);
}

/*
 * 1. Setup nfsd while nfs.conf contains "nfs.server.mount.regular_files=1" to allow mounting regular files
 * 2. Create local file
 * 3. Send MOUNT request to the file. verify MNT3_OK is returned
 */
- (void)testConfigMountRegularFiles
{
	const char buff[NAME_MAX] = {};
	const char *conf[1] = { buff };
	char path[NAME_MAX] = {}, *ppath = path;
	int dirFD, fileFD, err;
	char *file = "new_file";
	mountres3 *result;

	snprintf((char *)buff, sizeof(buff), "%s=%d", "nfs.server.mount.regular_files", 1);
	doNFSSetUpWithArgsConfFlagsAndDoMount(NULL, 0, conf, ARRAY_SIZE(conf), SOCK_STREAM, 0, SOCK_STREAM, 0, 0);

	err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
	if (err) {
		XCTFail("createFileInPath failed, got %d", err);
		return;
	}
	snprintf(path, sizeof(path), "%s/%s", getLocalMountedPath(), file);

	result = mountproc3_mnt_3(&ppath, mclnt);
	XCTAssertNotEqual(NULL, result);
	XCTAssertEqual(MNT3_OK, result->fhs_status);

	removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Setup nfsd while nfs.conf contains "nfs.server.reqcache_size=128"
 * 2. Verify the expected amount using sysctl_get
 */
- (void)testConfigReqCache
{
	const char buff[NAME_MAX] = {};
	const char *conf[1] = { buff };
	int reqcache = -1, expected_reqcache = 128;
	snprintf((char *)buff, sizeof(buff), "%s=%d", "nfs.server.reqcache_size", expected_reqcache);

	doNFSSetUpWithConf(conf, ARRAY_SIZE(conf), SOCK_STREAM, SOCK_STREAM);

	XCTAssertFalse(sysctl_get("vfs.generic.nfs.server.reqcache_size", &reqcache));
	XCTAssertEqual(reqcache, expected_reqcache);
}

/*
 * 1. Setup nfsd while nfs.conf contains "nfs.server.request_queue_length=128"
 * 2. Verify the expected amount using sysctl_get
 */
- (void)testConfigReqQueueLength
{
	const char buff[NAME_MAX] = {};
	const char *conf[1] = { buff };
	int req_queue_length = -1, expected_req_queue_length = 256;
	snprintf((char *)buff, sizeof(buff), "%s=%d", "nfs.server.request_queue_length", expected_req_queue_length);

	doNFSSetUpWithConf(conf, ARRAY_SIZE(conf), SOCK_STREAM, SOCK_STREAM);

	XCTAssertFalse(sysctl_get("vfs.generic.nfs.server.request_queue_length", &req_queue_length));
	XCTAssertEqual(req_queue_length, expected_req_queue_length);
}

/*
 * 1. Setup nfsd while nfs.conf contains "    nfs.server.wg_delay     =9999"
 * 2. Verify the expected amount using sysctl_get
 */
- (void)testConfigWGDelayC2WithSpaces
{
	const char buff[NAME_MAX] = {};
	const char *conf[1] = { buff };
	int wg_delay = -1, expected_wg_delay = 9999;
	snprintf((char *)buff, sizeof(buff), "%s=%d", "     nfs.server.wg_delay    ", expected_wg_delay);

	doNFSSetUpWithConf(conf, ARRAY_SIZE(conf), SOCK_STREAM, SOCK_STREAM);

	XCTAssertFalse(sysctl_get("vfs.generic.nfs.server.wg_delay", &wg_delay));
	XCTAssertEqual(wg_delay, expected_wg_delay);
}

/*
 * 1. Setup nfsd while nfs.conf contains "nfs.server.wg_delay_v3=8888"
 * 2. Verify the expected amount using sysctl_get
 * 3. Create a local file
 * 4. Write data to the file
 * 5. Verify data was written as expected
 */
- (void)testConfigWGDelayV3
{
#define WRITE_SIZE (64 * 1024)
	int err, dirFD, fileFD;
	int offset = 0, left = WRITE_SIZE, count = 1024;
	LOOKUP3res *res;
	WRITE3res *res2;
	struct stat stat = {};
	char *file = "new_file";
	char buffer[WRITE_SIZE] = {};
	const char buff[NAME_MAX] = {};
	const char *conf[1] = { buff };
	int wg_delay_v3 = -1, expected_wg_delay_v3 = 1;
	snprintf((char *)buff, sizeof(buff), "%s=%d", "nfs.server.wg_delay_v3", expected_wg_delay_v3);

	doNFSSetUpWithConf(conf, ARRAY_SIZE(conf), SOCK_STREAM, SOCK_STREAM);

	XCTAssertFalse(sysctl_get("vfs.generic.nfs.server.wg_delay_v3", &wg_delay_v3));
	XCTAssertEqual(wg_delay_v3, expected_wg_delay_v3);

	err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
	if (err) {
		XCTFail("createFileInPath failed, got %d", err);
		return;
	}

	res = doLookupRPC(nclnt, &rootfh, file);
	if (res->status != NFS3_OK) {
		XCTFail("doLookupRPC failed, got %d", res->status);
		goto out;
	}

	while (left > 0) {
		res2 = doWriteRPC(nclnt, &res->LOOKUP3res_u.resok.object, offset, count, UNSTABLE, count, &buffer[offset]);
		if (res2->status != NFS3_OK) {
			XCTFail("doWriteRPC failed, got %d", res2->status);
			goto out;
		}

		if (res2->WRITE3res_u.resok.count == 0) {
			XCTFail("doWriteRPC wrote zero bytes");
			goto out;
		}

		left -= res2->WRITE3res_u.resok.count;
		offset += res2->WRITE3res_u.resok.count;
	}

	err = fstat(fileFD, &stat);
	if (err) {
		XCTFail("fstat failed, got %d", err);
		goto out;
	}

	XCTAssertEqual(WRITE_SIZE, stat.st_size);

out:
	removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Setup nfsd while nfs.conf contains "nfs.server.wg_delay_v3=8888"
 * 2. Verify the expected amount using sysctl_get
 * 3. Create a local file
 * 4. Write NFS_MAXDATA + 1 bytes of data to the file
 * 5. Verify WRITE failed with NFS3ERR_IO
 */
- (void)testConfigWGDelayV3EIO
{
	int err, dirFD, fileFD;
	LOOKUP3res *res;
	WRITE3res *res2;
	char *file = "new_file";
	char buffer[NFS_MAXDATA + 1] = {};
	const char buff[NAME_MAX] = {};
	const char *conf[1] = { buff };
	int wg_delay_v3 = -1, expected_wg_delay_v3 = 1;
	snprintf((char *)buff, sizeof(buff), "%s=%d", "nfs.server.wg_delay_v3", expected_wg_delay_v3);

	doNFSSetUpWithConf(conf, ARRAY_SIZE(conf), SOCK_STREAM, SOCK_STREAM);

	XCTAssertFalse(sysctl_get("vfs.generic.nfs.server.wg_delay_v3", &wg_delay_v3));
	XCTAssertEqual(wg_delay_v3, expected_wg_delay_v3);

	err = createFileInPath(getLocalMountedPath(), file, &dirFD, &fileFD);
	if (err) {
		XCTFail("createFileInPath failed, got %d", err);
		return;
	}

	res = doLookupRPC(nclnt, &rootfh, file);
	if (res->status != NFS3_OK) {
		XCTFail("doLookupRPC failed, got %d", res->status);
		goto out;
	}

	res2 = doWriteRPC(nclnt, &res->LOOKUP3res_u.resok.object, 0, sizeof(buffer), UNSTABLE, sizeof(buffer), &buffer[0]);
	if (res2->status != NFS3ERR_IO) {
		XCTFail("doWriteRPC failed, expected status is %d, got %d", NFS3ERR_IO, res2->status);
	}

out:
	removeFromPath(file, dirFD, fileFD, REMOVE_FILE);
}

/*
 * 1. Setup nfsd while nfs.conf contains "nfs.server.materialize_dataless_files=1"
 */
- (void)testConfigMaterializeDatalessFiles
{
	const char buff[NAME_MAX] = {};
	const char *conf[1] = { buff };
	int materialize_dataless_files = 1;
	snprintf((char *)buff, sizeof(buff), "%s=%d", "nfs.server.materialize_dataless_files", materialize_dataless_files);

	doNFSSetUpWithConf(conf, ARRAY_SIZE(conf), SOCK_STREAM, SOCK_STREAM);

	// getiopolicy_np can be used only to get current process io-policy.
}

/*
 * 1. Setup nfsd while nfs.conf contains "nfs.server.verbose=10"
 */
- (void)testConfigVerbose
{
	const char buff[NAME_MAX] = {};
	const char *conf[1] = { buff };
	int verbose = 10;
	snprintf((char *)buff, sizeof(buff), "%s=%d", "nfs.server.verbose", verbose);

	doNFSSetUpWithConf(conf, ARRAY_SIZE(conf), SOCK_STREAM, SOCK_STREAM);
}

/*
 * 1. Setup nfsd while nfs.conf contains am invalid line
 */
- (void)testConfigInvalid
{
	const char buff[NAME_MAX] = {};
	const char *conf[1] = { buff };
	int verbose = 10;
	snprintf((char *)buff, sizeof(buff), "%s=%d", "nfs.server.not_a_valid_param", verbose);

	doNFSSetUpWithConf(conf, ARRAY_SIZE(conf), SOCK_STREAM, SOCK_STREAM);
}

/*
 * 1. Setup nfsd while nfs.conf contains am invalid line
 */
- (void)testConfigInvalid2
{
	const char buff[NAME_MAX] = {};
	const char *conf[1] = { buff };
	snprintf((char *)buff, sizeof(buff), "%s", "    not_a_valid_param    ");

	doNFSSetUpWithConf(conf, ARRAY_SIZE(conf), SOCK_STREAM, SOCK_STREAM);
}

@end
