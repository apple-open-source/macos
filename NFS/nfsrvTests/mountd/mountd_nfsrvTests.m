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

#include <ftw.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <oncrpc/auth.h>
#include <oncrpc/clnt.h>
#include <oncrpc/types.h>
#include <nfs/rpcv2.h>
#include <TargetConditionals.h>
#include <CoreFoundation/CoreFoundation.h>

#import <XCTest/XCTest.h>

#include "common.h"
#include "mountd.h"
#include "pathnames.h"
#include "mountd_rpc.h"

#if TARGET_OS_OSX

/* Tests globals */
#define DSTDIR     "/System/Volumes/Data/private/tmp/nfsrvtest"
#define TEMPLATE    DSTDIR "/nfsrvtest.XXXXXXXX"
#define EXPORTS     "exports"
#define CONFIG      "nfs.conf"

char template[] = TEMPLATE;
char *rootdir = NULL;
char exportsPath[PATH_MAX];
char confPath[PATH_MAX];
int exportsFD = -1, confFD = -1, dirFD = -1;
AUTH *auth = NULL;
CLIENT *mclnt = NULL;

static void doUnmountallRPC(void);

// Exports content
typedef struct {
	const char *paths[10]; // last element must be NULL
	const char *flags;
	const char *network;
	const char *mask;
} export_t;

export_t gExports[5] = {
	[0] = {
		{ "dir1", "dir1/dir1_1", "dir1/dir1_2", "dir1/dir1_3", NULL},
		"-ro",
		"-network 111.0.0.0",
		"-mask 255.0.0.0"
	},
	[1] = {
		{ "dir2", NULL},
		"-alldirs",
		"-network 22.0.0.0",
		"-mask 255.255.0.0"
	},
	[2] = {
		{ "dir3", "dir3/dir3_1", NULL },
		"-maproot=root -sec=sys:krb5",
		"-alldirs",
		""
	},
	[3] = {
		{ "dir4", NULL },
		"-maproot=root",
		"-network 2001:db8::",
		"-mask ffff:ffff::"
	},
	[4] = {
		{ "dir5", NULL },
		"-ro -sec=sys:krb5",
		"localhost",
		""
	},
};

// Helper functions

const char *
getRootDir()
{
	return rootdir;
}

const char*
getDestPath()
{
	return gExports[2].paths[0];
}

const char*
getDestReadOnlyPath()
{
	return gExports[4].paths[0];
}

const char *
getLocalMountedPath()
{
	static char *path = NULL;
	if (path == NULL) {
		static char pathbuff[PATH_MAX] = {};
		// Create dest path
		snprintf(pathbuff, sizeof(pathbuff), "%s/%s", getRootDir(), getDestPath());
		path = pathbuff;
	}

	return path;
}

const char *
getLocalMountedReadOnlyPath()
{
	static char *path = NULL;
	if (path == NULL) {
		static char pathbuff[PATH_MAX] = {};
		// Create dest path
		snprintf(pathbuff, sizeof(pathbuff), "%s/%s", getRootDir(), getDestReadOnlyPath());
		path = pathbuff;
	}

	return path;
}

int
unlink_cb(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
	int rv = remove(path);

	if (rv) {
		XCTFail("remove failed %d", rv);
	}

	return rv;
}

void
doMountSetUpWithArgs(const char **nfsdArgs, int nfsdArgsSize, const char **confValues, int confValuesSize)
{
	uid_t uid;
	pid_t pid;
	int err;
	bzero(exportsPath, sizeof(exportsPath));

	// Tests should run as root
	// Enable root option using : defaults write com.apple.dt.Xcode EnableRootTesting YES
	if ((uid = getuid()) != 0) {
		XCTFail("Test should run as root, current user %d", uid);
		return;
	}

	// Remove all test remainings
	err = nftw(DSTDIR, unlink_cb, 64, FTW_MOUNT | FTW_DEPTH | FTW_PHYS);
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

	pid = get_nfsd_pid();
	if (pid > 0) {
		XCTFail("nfsd should be stopped: %d", errno);
		return;
	}

	// Create temporary folder name
	rootdir = mkdtemp(template);
	if (!rootdir) {
		XCTFail("Unable to create tmpdir: %d", errno);
		return;
	}

	// Create temporary folder
	dirFD = open(rootdir, O_DIRECTORY | O_SEARCH, 0777);
	if (dirFD < 0) {
		XCTFail("Unable to open tmpdir (%s): %d", rootdir, errno);
		return;
	}

	confFD = openat(dirFD, CONFIG, O_CREAT | O_RDWR | O_EXCL);
	if (confFD < 0) {
		XCTFail("Unable to create config (%s:/%s): %d", rootdir, CONFIG, errno);
		return;
	}

	for (int i = 0; i < confValuesSize; i++) {
		dprintf(confFD, "%s\n", confValues[i]);
	}

	// Create conf exports path
	snprintf(confPath, sizeof(confPath), "%s/%s", rootdir, CONFIG);

	// Create exports file
	exportsFD = openat(dirFD, EXPORTS, O_CREAT | O_RDWR | O_EXCL);
	if (exportsFD < 0) {
		XCTFail("Unable to create exports (%s:/%s): %d", rootdir, EXPORTS, errno);
		return;
	}

	// Create full exports path
	snprintf(exportsPath, sizeof(exportsPath), "%s/%s", rootdir, EXPORTS);

	// Create exports content
	for (int i = 0; i < ARRAY_SIZE(gExports); i++) {
		int n;
		for (int j = 0; j < ARRAY_SIZE(gExports[i].paths) && gExports[i].paths[j] != NULL; j++) {
			err = mkdirat(dirFD, gExports[i].paths[j], 0777);
			if (err < 0) {
				XCTFail("Unable to create subdir %s %d", gExports[i].paths[j], errno);
				return;
			}
			n = dprintf(exportsFD, "%s/%s ", rootdir, gExports[i].paths[j]);
			if (n <= 0) {
				XCTFail("Error during dprintf %d", errno);
				return;
			}
		}
		n = dprintf(exportsFD, "%s %s %s\n", gExports[i].flags, gExports[i].network, gExports[i].mask);
		if (n <= 0) {
			XCTFail("Error during dprintf %d", errno);
			return;
		}
	}

	// kick NFSD using the temporary exports file
	if (fork() == 0) {
		int argc = nfsdArgsSize + 3;
		char **argv = calloc(argc, sizeof(char *));
		if (argv == NULL) {
			XCTFail("Cannot allocate argv array");
			return;
		}

		// Build args list
		argv[0] = "nfsd";
		argv[1] = "-F";
		argv[2] = exportsPath;
		for (int i = 0; i < nfsdArgsSize; i++) {
			argv[i + 3] = (char *)nfsdArgs[i];
		}

		// Kick nfsd
		nfsd_imp(argc, argv, confPath);
		free(argv);
	} else {
		// Sleep until NFSD is up
		while (get_nfsd_pid() == 0) {
			sleep(1);
		}
	}
}

void
doMountSetUp()
{
	doMountSetUpWithArgs(NULL, 0, NULL, 0);
}

void
doMountTearDown()
{
	pid_t pid;

	if (mclnt) {
		doUnmountallRPC();
	}

	if ((pid = get_nfsd_pid()) != 0) {
		kill(pid, SIGTERM);
		while (get_nfsd_pid() != 0) {
			sleep(1);
		}
	}

	if (confFD >= 0) {
		close(confFD);
		confFD = -1;
		unlink(confPath);
		bzero(confPath, sizeof(confPath));
	}

	if (exportsFD >= 0) {
		close(exportsFD);
		exportsFD = -1;
		unlink(exportsPath);
		bzero(exportsPath, sizeof(exportsPath));
	}

	if (dirFD >= 0) {
		for (int i = 0; i < ARRAY_SIZE(gExports); i++) {
			// Start from the 2nd element
			for (int j = 1; i < ARRAY_SIZE(gExports[i].paths) && gExports[i].paths[j] != NULL; j++) {
				unlinkat(dirFD, gExports[i].paths[j], AT_REMOVEDIR);
			}

			// Now remove the parent subdir
			if (gExports[i].paths[0]) {
				unlinkat(dirFD, gExports[i].paths[0], AT_REMOVEDIR);
			}
		}

		close(dirFD);
		dirFD = -1;
	}

	if (rootdir) {
		if (rmdir(rootdir) < 0) {
			XCTFail("Unable to remove dir %s: %d", rootdir, errno);
		}
		rootdir = NULL;
	}

	rmdir(DSTDIR);

	if (auth) {
		auth_destroy(auth);
		auth = NULL;
	}

	if (mclnt) {
		clnt_destroy(mclnt);
		mclnt = NULL;
	}
}

CLIENT *
createClientForProtocol(const char *host, int socketFamily, int socketType, int authType, unsigned int program, unsigned int version, int flags, int *sockp)
{
	CLIENT *client = NULL;
	int err = 0, dontblock = 1;
	int reserve = 64 * 1024;
	int sock = RPC_ANYSOCK;
	int range = IP_PORTRANGE_LOW;
	struct sockaddr_un un = {};
	struct addrinfo hints = {}, *ai = NULL;

	// Get local address info
	if (socketFamily == AF_LOCAL) {
		un.sun_family = socketFamily;
		un.sun_len = sizeof(un);
		strlcpy(un.sun_path, host, sizeof(un.sun_path));
	} else {
		hints.ai_family = socketFamily;
		hints.ai_socktype = socketType;
		hints.ai_flags = AI_ADDRCONFIG;
		err = getaddrinfo(host, NULL, &hints, &ai);
		if (err) {
			XCTFail("Cannot create address %s\n", strerror(err));
			return NULL;
		}
	}

	if (flags & CREATE_SOCKET) {
		sock = socket(socketFamily, socketType, 0);
		if (sock < 0) {
			XCTFail("Cannot create socket %s\n", strerror(err));
		}

		err = ioctl(sock, FIONBIO, &dontblock);
		if (err < 0) {
			XCTFail("Cannot set ioctl %s\n", strerror(err));
		}
	}

	switch (socketType) {
	case SOCK_STREAM:
	{
		if (socketFamily == AF_LOCAL) {
			client = clntticotsord_create_timeout((struct sockaddr *)&un, program, version, &sock, 0, 0, NULL, NULL);
		} else {
			client = clntstrm_create_timeout(ai->ai_addr, program, version, &sock, 0, 0, NULL, NULL);
		}
		break;
	}
	case SOCK_DGRAM:
	{
		struct timeval timeout = { .tv_sec = 5, .tv_usec = 0 };
		int size = 33 * 1024;
		client = clntudp_bufcreate((struct sockaddr_in *)ai->ai_addr, program, version, timeout, &sock, size, size);
		break;
	}
	default:
	{
		XCTFail("Socket mode is not supported");
	}
	}
	if (client == NULL) {
		if ((flags & CREATE_CLIENT_FAILURE) == 0) {
			XCTFail("Cannot create CLNT: %s", clnt_spcreateerror(host));
			err = ENOMEM;
		}
		goto out;
	}

	switch (authType) {
	case AUTH_NULL:
	{
		auth = authnone_create();
		break;
	}
	case AUTH_UNIX:
	{
		auth = authunix_create_default();
		break;
	}
	default:
	{
		XCTFail("Socket mode is not supported");
	}
	}

	if (auth == NULL) {
		XCTFail("Cannot create auth");
		err = ENOMEM;
		goto out;
	}

	clnt_auth_set(client, auth);

	if (socketFamily != AF_LOCAL && (flags & CREATE_SOCKET) == 0) {
		err = setsockopt(sock, (socketFamily == AF_INET) ? IPPROTO_IP : IPPROTO_IPV6, IP_PORTRANGE, &range, sizeof(range));
		XCTAssertFalse(err, "Cannot set to lower ip port range %d, %d", err, errno);
	}

	err = setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &reserve, sizeof(reserve));
	XCTAssertFalse(err, "Cannot set socket send buffer size %d, %d", err, errno);

	if (sockp) {
		*sockp = sock;
	}

out:
	if (socketFamily != AF_LOCAL) {
		freeaddrinfo(ai);
	}

	return client;
}

int
createClientForMountProtocol(int socketFamily, int socketType, int authType, int flags)
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
			return -1;
		}
		host = _PATH_MOUNTD_TICOTSORD_SOCK;
		break;
	default:
		XCTFail("Unsupported family");
		return -1;
	}

	if ((mclnt = createClientForProtocol(host, socketFamily, socketType, authType, MOUNT_PROGRAM, MOUNT_V3, flags, NULL)) == NULL) {
		return -1;
	}
	return 0;
}

static void
doNullRPC()
{
	void *result;
	char *arg;

	result = mountproc3_null_3((void*)&arg, mclnt);
	if (result == NULL) {
		XCTFail("mountproc3_null_3 returned null");
	}
}

static mountres3 *
doMountRPC(dirpath path, int expectedResult)
{
	mountres3 *result;

	result = mountproc3_mnt_3((void*)&path, mclnt);
	if (result == NULL) {
		XCTFail("mountproc3_mnt_3 returned null");
	} else if (result->fhs_status != expectedResult) {
		XCTFail("mountproc3_mnt_3 failed with: %d, expected %d", result->fhs_status, expectedResult);
	}

	return result;
}

static mountlist *
doDumpRPC()
{
	mountlist *result;
	char *arg;

	result = mountproc3_dump_3((void*)&arg, mclnt);
	if (result == NULL) {
		XCTFail("mountproc3_dump_3 returned null");
	}
	return result;
}

static void
doUnmountRPC(dirpath dirpath)
{
	void *result;

	result = mountproc3_umnt_3((void*)&dirpath, mclnt);
	if (result == NULL) {
		XCTFail("mountproc3_umnt_3 returned null");
	}
}

static void
doUnmountallRPC()
{
	void *result;
	char *arg;

	result = mountproc3_umntall_3((void*)&arg, mclnt);
	if (result == NULL) {
		XCTFail("mountproc3_umntall_3 returned null");
	}
}

static exports *
doExportRPC()
{
	exports *result;
	char *arg;

	result = mountproc3_export_3((void*)&arg, mclnt);
	if (result == NULL) {
		XCTFail("mountproc3_export_3 returned null");
	}
	return result;
}

fhandle_t *
doMountAndVerify(const char *path)
{
	static fhandle_t fh = {};
	mountres3_ok *mountres = NULL;
	mountres3 *mount = NULL;

	// Zero fh
	memset(&fh, 0, sizeof(fh));

	// Call mount
	mount = doMountRPC((char *)path, MNT3_OK);
	if (mount == NULL) {
		XCTFail("doMountRPC returned null");
		return NULL;
	}

	mountres = &mount->mountres3_u.mountinfo;
	// Get local file handle
	if (getfh(path, &fh) < 0) {
		XCTFail("Cannot get local file handle %d", errno);
		return NULL;
	}

	// Compare filehandles
	if (fh.fh_len != mountres->fhandle.fhandle3_len) {
		XCTFail("file handles size is not equal %d %d", fh.fh_len, mountres->fhandle.fhandle3_len);
		return NULL;
	}
	if (memcmp(fh.fh_data, mountres->fhandle.fhandle3_val, fh.fh_len) != 0) {
		XCTFail("file handles data is not equal");
		return NULL;
	}

	// Verify RPCAUTH_KRB5 and RPCAUTH_UNIX auth flavors are supported
	if (mountres->auth_flavors.auth_flavors_len != 2) {
		XCTFail("Amount of supported auth flavors is expected to be 2. actual %d", mountres->auth_flavors.auth_flavors_len);
		return NULL;
	}
	if (mountres->auth_flavors.auth_flavors_val[0] != RPCAUTH_UNIX) {
		XCTFail("First auth flavor is expected to be RPCAUTH_UNIX. actual %d", mountres->auth_flavors.auth_flavors_val[0]);
		return NULL;
	}
	if (mountres->auth_flavors.auth_flavors_val[1] != RPCAUTH_KRB5) {
		XCTFail("Second auth flavor is expected to be RPCAUTH_KRB5. actual %d", mountres->auth_flavors.auth_flavors_val[1]);
		return NULL;
	}
	return &fh;
}

static void
doMountUnmountOrUnmountAllAndVerify(void (^doRPC)(void))
{
	mountlist *listp = NULL;

	// Mount RPC
	if (doMountAndVerify(getLocalMountedPath()) == NULL) {
		XCTFail("doMountAndVerify failed");
	}

	// Dump RPC
	listp = doDumpRPC();
	if (listp == NULL) {
		XCTFail("doDumpRPC returned null");
		return;
	}

	// Verify reply
	if (*listp == NULL) {
		XCTFail("List is null");
		return;
	}
	if (strncmp(gExports[2].network, (*listp)->ml_hostname, PATH_MAX) != 0) {
		XCTFail("Host is expected to be %s, got %s", gExports[2].network, (*listp)->ml_hostname);
		return;
	}
	if (strncmp(getLocalMountedPath(), (*listp)->ml_directory, PATH_MAX) != 0) {
		XCTFail("Directory is expected to be %s, got %s", gExports[2].network, (*listp)->ml_hostname);
		return;
	}

	// Unmount/UnmountAll RPCs
	doRPC();

	// Dump RPC
	listp = doDumpRPC();
	if (listp == NULL) {
		XCTFail("doDumpRPC returned null");
		return;
	}

	// Verify reply
	if (*listp != NULL) {
		XCTFail("List is expected to be null");
		return;
	}
}

@interface nfsrvTests_mount : XCTestCase

@end

@implementation nfsrvTests_mount

- (void)setUp
{
	doMountSetUp();
}

/*
 * 1. Send NULL based on UDP socket and null authentication to the server, make sure we got the reply
 */
- (void)testMountUDPAuthNullProcNull;
{
	int err = createClientForMountProtocol(AF_INET, SOCK_DGRAM, RPCAUTH_NULL, 0);
	if (err) {
		XCTFail("Cannot create client: %d", err);
	}

	doNullRPC();
}


/*
 * 1. Send NULL based on UDP socket and UNIX authentication to the server, make sure we got the reply
 */
- (void)testMountUDPAuthUnixProcNull;
{
	int err = createClientForMountProtocol(AF_INET, SOCK_DGRAM, AUTH_UNIX, 0);
	if (err) {
		XCTFail("Cannot create client: %d", err);
	}

	doNullRPC();
}

/*
 * 1. Send NULL based on TCP socket and null authentication to the server, make sure we got the reply
 */
- (void)testMountTCPAuthNullProcNull;
{
	int err = createClientForMountProtocol(AF_INET, SOCK_STREAM, RPCAUTH_NULL, 0);
	if (err) {
		XCTFail("Cannot create client: %d", err);
	}

	doNullRPC();
}

/*
 * 1. Send NULL based on TCP socket and UNIX authentication to the server, make sure we got the reply
 */
- (void)testMountTCPAuthUnixProcNull;
{
	int err = createClientForMountProtocol(AF_INET, SOCK_STREAM, RPCAUTH_UNIX, 0);
	if (err) {
		XCTFail("Cannot create client: %d", err);
	}

	doNullRPC();
}

/*
 * 1. Send NULL based on IPv6 UDP socket and null authentication to the server, make sure we got the reply
 */
- (void)testMountIPv6UDPAuthNullProcNull;
{
	int err = createClientForMountProtocol(AF_INET6, SOCK_DGRAM, RPCAUTH_NULL, 0);
	if (err) {
		XCTFail("Cannot create client: %d", err);
	}

	doNullRPC();
}

/*
 * 1. Send NULL based on IPv6 UDP socket and UNIX authentication to the server, make sure we got the reply
 */
- (void)testMountIPv6UDPAuthUnixProcNull;
{
	int err = createClientForMountProtocol(AF_INET6, SOCK_DGRAM, RPCAUTH_UNIX, 0);
	if (err) {
		XCTFail("Cannot create client: %d", err);
	}

	doNullRPC();
}

/*
 * 1. Send NULL based on IPv6 TCP socket and null authentication to the server, make sure we got the reply
 */
- (void)testMountIPv6TCPAuthNullProcNull;
{
	int err = createClientForMountProtocol(AF_INET6, SOCK_STREAM, RPCAUTH_NULL, 0);
	if (err) {
		XCTFail("Cannot create client: %d", err);
	}

	doNullRPC();
}

/*
 * 1. Send NULL based on IPv6 TCP socket and UNIX authentication to the server, make sure we got the reply
 */
- (void)testMountIPv6TCPAuthUnixProcNull;
{
	int err = createClientForMountProtocol(AF_INET6, SOCK_STREAM, RPCAUTH_UNIX, 0);
	if (err) {
		XCTFail("Cannot create client: %d", err);
	}

	doNullRPC();
}

/*
 * 1. Send MOUNT to the server
 * 2. Verify returned file handle using getfh() syscall
 */
- (void)testMountTCPAuthUnixProcMountOK;
{
	int err = createClientForMountProtocol(AF_INET, SOCK_STREAM, RPCAUTH_UNIX, 0);
	if (err) {
		XCTFail("Cannot create client: %d", err);
	}

	if (doMountAndVerify(getLocalMountedPath()) == NULL) {
		XCTFail("doMountAndVerify failed");
	}
}

/*
 * 1. Send MOUNT using directory which exported to a different host. MNT3ERR_ACCES is expected
 */
- (void)testMountTCPAuthUnixProcMountEACCESS;
{
	int err = createClientForMountProtocol(AF_INET, SOCK_STREAM, RPCAUTH_UNIX, 0);
	if (err) {
		XCTFail("Cannot create client: %d", err);
	}

	mountres3 *mount = NULL;
	char path[PATH_MAX] = {0};

	// Create dest path
	snprintf(path, sizeof(path), "%s/%s", rootdir, gExports[1].paths[0]);

	// Call mount
	mount = doMountRPC(path, MNT3ERR_ACCES);
	if (mount == NULL) {
		XCTFail("doMountRPC returned null");
		return;
	}
}

/*
 * 1. Send MOUNT using non exsiting directory. MNT3ERR_ACCES is expected
 */
- (void)testMountTCPAuthUnixProcMountEACCESS2;
{
	int err = createClientForMountProtocol(AF_INET, SOCK_STREAM, RPCAUTH_UNIX, 0);
	if (err) {
		XCTFail("Cannot create client: %d", err);
	}

	mountres3 *mount = NULL;
	char path[PATH_MAX] = {0};

	// Create dest path
	snprintf(path, sizeof(path), "%s/%s", rootdir, "noSuchDir");

	// Call mount
	mount = doMountRPC(path, MNT3ERR_ACCES);
	if (mount == NULL) {
		XCTFail("doMountRPC returned null");
		return;
	}
}

/*
 * 1. Send DUMP using non exsiting directory. MNT3ERR_ACCES is expected
 */
- (void)testMountTCAuthUnixProcDump;
{
	int err = createClientForMountProtocol(AF_INET, SOCK_STREAM, RPCAUTH_UNIX, 0);
	if (err) {
		XCTFail("Cannot create client: %d", err);
	}

	doDumpRPC();
}

/*
 * 1. Send MOUNT to the server
 * 2. Verify returned file handle using getfh() syscall
 * 3. Send DUMP, verify mount entry is returned
 * 4. Send UNMOUNT
 * 5. Send DUMP, verify list is empty
 */
- (void)testMountTCPAuthUnixProcUnmount;
{
	int err = createClientForMountProtocol(AF_INET, SOCK_STREAM, RPCAUTH_UNIX, 0);
	if (err) {
		XCTFail("Cannot create client: %d", err);
	}

	doMountUnmountOrUnmountAllAndVerify(^ {
		doUnmountRPC((char *)getLocalMountedPath());
	});
}

/*
 * 1. Send MOUNT to the server
 * 2. Verify returned file handle using getfh() syscall
 * 3. Send DUMP, verify mount entry is returned
 * 4. Send UNMOUNTALL
 * 5. Send DUMP, verify list is empty
 */
- (void)testMountTCPAuthUnixProcUnmountAll;
{
	int err = createClientForMountProtocol(AF_INET, SOCK_STREAM, RPCAUTH_UNIX, 0);
	if (err) {
		XCTFail("Cannot create client: %d", err);
	}

	doMountUnmountOrUnmountAllAndVerify(^ {
		doUnmountallRPC();
	});
}

/*
 * 1. Send EXPORTS to the server
 * 2. Compare returned exports with gExports
 */
- (void)testMountTCPAuthUnixProcExport;
{
	int err = createClientForMountProtocol(AF_INET, SOCK_STREAM, RPCAUTH_UNIX, 0);
	if (err) {
		XCTFail("Cannot create client: %d", err);
	}

	char *sec;
	char path[PATH_MAX];
	int offset;
	groups groups;
	exports *pexports, exports;

	pexports = doExportRPC();
	if (pexports == NULL) {
		XCTFail("doExportRPC returned null");
		return;
	}

	// Verify reply
	exports = *pexports;
	for (int i = ARRAY_SIZE(gExports) - 1; i >= 0; i--) {
		for (int j = 0; j < ARRAY_SIZE(gExports[i].paths) && gExports[i].paths[j] != NULL; j++) {
			if (exports == NULL) {
				XCTFail("Got null export %d", i);
				return;
			}

			// Create dest path
			snprintf(path, sizeof(path), "%s/%s", rootdir, gExports[i].paths[j]);

			if (strncmp(exports->ex_dir, path, PATH_MAX) != 0) {
				XCTFail("Directory is expected to be %s, got %s", gExports[i].paths[j], exports->ex_dir);
				return;
			}

			// Check for auth flavors
			groups = exports->ex_groups;
			if ((sec = strstr(gExports[i].flags, "-sec=")) != NULL) {
				sec += strnlen("-sec=", NAME_MAX);
				if (strnlen(groups->gr_name, NAME_MAX) != strnlen(sec, NAME_MAX) + 2 ||
				    groups->gr_name[0] != '<' ||
				    strncmp(sec, groups->gr_name + 1, strnlen(sec, NAME_MAX)) != 0 ||
				    groups->gr_name[strnlen(sec, NAME_MAX) + 1] != '>') {
					XCTFail("Auth flavors is expected to be %s, got %s", sec, groups->gr_name);
					return;
				}
				groups = groups->gr_next;
			}

			// Check if network start with "-network"
			if (strncmp(gExports[i].network, "-network ", strlen("-network ")) == 0) {
				offset = strlen("-network ");
			} else {
				offset = 0;
			}

			if (strncmp(gExports[i].network + offset, groups->gr_name, NAME_MAX) != 0) {
				XCTFail("Network is expected to be %s, got %s", gExports[i].network + offset, groups->gr_name);
				return;
			}

			groups = groups->gr_next;
			if (groups != NULL) {
				XCTFail("Groups is expected to be null");
				return;
			}
			exports = exports->ex_next;
		}
	}
	if (exports != NULL) {
		XCTFail("Exports is expected to be null");
		return;
	}
}

- (void)tearDown
{
	doMountTearDown();
}

@end

#endif /* TARGET_OS_OSX */
