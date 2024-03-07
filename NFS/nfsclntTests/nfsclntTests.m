/*
 * Copyright (c) 1999-2018 Apple Inc. All rights reserved.
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
#include <sys/mount.h>
#include <sysexits.h>
#include <TargetConditionals.h>

#import <XCTest/XCTest.h>

#include "nfsclntTests_utils.h"

/* Tests globals */
#if TARGET_OS_OSX
#define DSTDIR     "/private/tmp/nfsclntest"
#else
#define DSTDIR     "/private/var/tmp/nfsclntest"
#endif /* TARGET_OS_OSX */

#define TEMPLATE    DSTDIR "/nfsclntest.XXXXXXXX"
char template[] = TEMPLATE;

char *dst = NULL;
char **argv = NULL;
int argc = 0;

static int
unlink_cb(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
	int rv = remove(path);

	if (rv) {
		XCTFail("remove failed %d", rv);
	}

	return rv;
}

static void
doMountSetup(void)
{
	int err;

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

	dst = mkdtemp(template);
	if (!dst) {
		XCTFail("Unable to create tmpdir: %d", errno);
	}
}

static void
doMountTearDown(void)
{
	argc = 0;

	if (argv) {
		free(argv);
		argv = NULL;
	}

	if (dst) {
		unmount(dst, MNT_FORCE);
		if (rmdir(dst) < 0) {
			XCTFail("Unable to remove dir %s: %d", dst, errno);
		}
		dst = NULL;
	}

	rmdir(DSTDIR);
}

/*
 * Mount Tests
 */
@interface nfsclntTests_mount : XCTestCase

@end

/*
 * Tests assume that DST_MOUNT_PATH is exported properly by the server - /etc/exports.
 */
#define DST_MOUNT_PATH   "17.78.122.71:/volume1/data"

@implementation nfsclntTests_mount

- (void)setUp
{
	doMountSetup();
}

- (void)tearDown
{
	doMountTearDown();
}

static void
_testArgs(char *mountPath, verifyArgsFunc verifier, char **nfsArgsIn, char **nfsArgsOut)
{
	struct statfs mntBuff;

	nfstests_init_input_args(mountPath, nfsArgsIn);
	nfstests_run_command(mount_nfs_imp, argc, argv, RESULT_SUCCESS);
	nfstests_get_mount(dst, &mntBuff);
	nfstests_verify_arg(verifier, [NSString stringWithUTF8String:mntBuff.f_mntonname], [NSString stringWithUTF8String:mntBuff.f_mntfromname], nfsArgsOut);
}

static void
_testNFSArgs(char **nfsArgs)
{
	_testArgs(DST_MOUNT_PATH, nfsParameterVerifier, nfsArgs, nfsArgs);
}

static void
_testNFSArg(char *nfsArg)
{
	char *nfsArgs[] = { nfsArg, NULL };
	_testNFSArgs(nfsArgs);
}

static void
_testMountArg(char *mountArg)
{
	char *mountArgs[] = { mountArg, NULL };
	_testArgs(DST_MOUNT_PATH, mountParameterVerifier, mountArgs, mountArgs);
}

/* NFS args */

- (void)testMountNoArgs
{
	_testNFSArg(NULL);
}

- (void)testMountV2
{
	_testNFSArg("vers=2");
}

- (void)testMountV3
{
	_testNFSArg("vers=3");
}

- (void)testMountHard
{
	_testNFSArg("hard");
}

- (void)testMountSoft
{
	_testNFSArg("soft");
}

- (void)testMountNoIntr
{
	_testNFSArg("nointr");
}

- (void)testMountIntr
{
	_testNFSArg("intr");
}

- (void)testMountLocalLocks
{
	_testNFSArg("locallocks");
}

- (void)testMountNoLocks
{
	_testNFSArg("nolocks");
}

- (void)testMountLocks
{
	_testNFSArg("locks");
}

- (void)testMountNoQuota
{
	_testNFSArg("noquota");
}

- (void)testMountQuota
{
	_testNFSArg("quota");
}

- (void)testMountRSize4K
{
	_testNFSArg("rsize=4096");
}

- (void)testMountWSize4K
{
	_testNFSArg("wsize=4096");
}

- (void)testMountRWSize4K
{
	char *inArgs[] = { "rwsize=4096", NULL };
	char *outArgs[] = { "rsize=4096", "wsize=4096", NULL };
	_testArgs(DST_MOUNT_PATH, nfsParameterVerifier, inArgs, outArgs);
}

- (void)testMountReadAHead0
{
	_testNFSArg("readahead=0");
}

- (void)testMountReadAHead32
{
	_testNFSArg("readahead=32");
}

- (void)testMountDSize4K
{
	_testNFSArg("dsize=4096");
}

- (void)testMountReadDirPlus
{
	_testNFSArg("rdirplus");
}

- (void)testMountNoReadDirPlus
{
	_testNFSArg("nordirplus");
}

- (void)testMountMaxGroups8
{
	_testNFSArg("maxgroups=8");
}

- (void)testMountTimeo5
{
	char *nfsArgs[] = { "retrans=5", "soft", NULL };
	_testNFSArgs(nfsArgs);
}

- (void)testMountACTimeo5
{
	char *inArgs[] = { "actimeo=5", NULL };
	char *outArgs[] = { "acregmin=5", "acregmax=5", "acdirmin=5", "acdirmax=5", "acrootdirmin=5", "acrootdirmax=5", NULL };
	_testArgs(DST_MOUNT_PATH, nfsParameterVerifier, inArgs, outArgs);
}

- (void)testMountNoAC
{
	char *inArgs[] = { "noac", NULL };
	char *outArgs[] = { "acregmin=0", "acregmax=0", "acdirmin=0", "acdirmax=0", "acrootdirmin=0", "acrootdirmax=0", NULL };
	_testArgs(DST_MOUNT_PATH, nfsParameterVerifier, inArgs, outArgs);
}

- (void)testMountReadlinkNoCache
{
	_testNFSArg("readlink_nocache=2");
}

- (void)testMountDeadTimeout
{
	_testNFSArg("deadtimeout=30");
}

- (void)testMountMuteJukebox
{
	_testNFSArg("mutejukebox");
}

- (void)testMountNoMuteJukebox
{
	_testNFSArg("nomutejukebox");
}

- (void)testMountNFC
{
	_testNFSArg("nfc");
}

- (void)testMountNoNFC
{
	_testNFSArg("nonfc");
}

- (void)testMountSkipRenew
{
	_testNFSArg("skip_renew");
}

/* FPnfs is deprecated: mount_nfs is expcted to fail with EX_UNAVAILABLE */
- (void)testFPnfs
{
	char *nfsArgs[] = { "fpnfs", NULL };

	nfstests_init_input_args(DST_MOUNT_PATH, nfsArgs);
	nfstests_run_command(mount_nfs_imp, argc, argv, EX_UNAVAILABLE);
}

/* Mount args */

- (void)testMountSync
{
	_testMountArg("sync");
}

- (void)testMountASync
{
	_testMountArg("async");
}

- (void)testMountAutomounted
{
	_testMountArg("automounted");
}

- (void)testMountNoBrowse
{
	_testMountArg("nobrowse");
}

- (void)testMountReadOnly
{
	_testMountArg("ro");
}

@end

/*
 * KRB5 Mount Tests
 */
@interface nfsclntTests_mount_krb : XCTestCase

@end

/*
 * Tests assume that DST_KRB_MOUNT_PATH is exported properly by the server - /etc/exports and /etc/krb5.keytab.
 */
#define DST_KRB_MOUNT_PATH   "liran-MacBook-Pro-2.apple.com:/System/Volumes/Data/Users/liranoz/workspace/nfs-server"

#define SEC_KRB5     "sec=krb5"
#define SEC_KRB5I    "sec=krb5i"
#define SEC_KRB5P    "sec=krb5p"

#define ETYPE_DES3   "etype=des3-cbc-sha1-kd"
#define ETYPE_AES128 "etype=aes128-cts-hmac-sha1-96"
#define ETYPE_AES256 "etype=aes256-cts-hmac-sha1-96"

static void
_testNFSKrb5Args(char **nfsArgs)
{
	_testArgs(DST_KRB_MOUNT_PATH, nfsParameterVerifier, nfsArgs, nfsArgs);
}

@implementation nfsclntTests_mount_krb

- (void)setUp
{
	doMountSetup();
}

- (void)tearDown
{
	doMountTearDown();
}

- (void)testMountKrb5Des
{
	char *nfsArgs[] = { SEC_KRB5, ETYPE_DES3, NULL };
	_testNFSKrb5Args(nfsArgs);
}

- (void)testMountKrb5iDes
{
	char *nfsArgs[] = { SEC_KRB5I, ETYPE_DES3, NULL };
	_testNFSKrb5Args(nfsArgs);
}

- (void)testMountKrb5pDes
{
	char *nfsArgs[] = { SEC_KRB5P, ETYPE_DES3, NULL };
	_testNFSKrb5Args(nfsArgs);
}

- (void)testMountKrb5Aes128
{
	char *nfsArgs[] = { SEC_KRB5, ETYPE_AES128, NULL };
	_testNFSKrb5Args(nfsArgs);
}

- (void)testMountKrb5iAes128
{
	char *nfsArgs[] = { SEC_KRB5I, ETYPE_AES128, NULL };
	_testNFSKrb5Args(nfsArgs);
}

- (void)testMountKrb5pAes128
{
	char *nfsArgs[] = { SEC_KRB5P, ETYPE_AES128, NULL };
	_testNFSKrb5Args(nfsArgs);
}

- (void)testMountKrb5Aes256
{
	char *nfsArgs[] = { SEC_KRB5, ETYPE_AES256, NULL };
	_testNFSKrb5Args(nfsArgs);
}

- (void)testMountKrb5iAes256
{
	char *nfsArgs[] = { SEC_KRB5I, ETYPE_AES256, NULL };
	_testNFSKrb5Args(nfsArgs);
}

- (void)testMountKrb5pAes256
{
	char *nfsArgs[] = { SEC_KRB5P, ETYPE_AES256, NULL };
	_testNFSKrb5Args(nfsArgs);
}

@end

/*
 * Parse Options Test
 */
@interface nfsclntTests_parse_options : XCTestCase

@end

size_t test_options_len;
char test_options[MAXPATHLEN];
struct nfs_options_client expected_options;

@implementation nfsclntTests_parse_options

- (void)setUp
{
	setUpOptions();
}

- (void)tearDown
{
}

- (void)testParseOptions
{
	struct timespec regmin = { .tv_sec = 11, .tv_nsec = 0 };
	struct timespec regmax = { .tv_sec = 22, .tv_nsec = 0 };
	struct timespec dirmin = { .tv_sec = 33, .tv_nsec = 0 };
	struct timespec dirmax = { .tv_sec = 44, .tv_nsec = 0 };
	struct timespec rootdirmin = { .tv_sec = 55, .tv_nsec = 0 };
	struct timespec rootdirmax = { .tv_sec = 66, .tv_nsec = 0 };
	struct timespec reqtimeo = { .tv_sec = 2, .tv_nsec = 100000000 }; /* 2.1 sec */
	struct timespec deadtime = { .tv_sec = 31, .tv_nsec = 0 };
	const char *fhstr = "fd168a14440000002b02184e02000000ffffffff000000002b02184e02000000";
	struct nfs_sec sec = { .count = 3, .flavors = { RPCAUTH_SYS, RPCAUTH_KRB5P, RPCAUTH_KRB5I }};
	struct nfs_etype etype = { .count = 3, .selected = 3, .etypes = { NFS_AES128_CTS_HMAC_SHA1_96, NFS_AES256_CTS_HMAC_SHA1_96, NFS_DES3_CBC_SHA1_KD } };
	fhandle_t fh;

	XCTAssertFalse(hexstr2fh(fhstr, &fh));

	writeArgToOptions(NFSTESTS_OPTIONS_MNT_FLAGS, "ro", (void *) MNT_RDONLY);
	writeArgToOptions(NFSTESTS_OPTIONS_MNT_FLAGS, "sync", (void *) MNT_SYNCHRONOUS);
	writeArgToOptions(NFSTESTS_OPTIONS_NFSVERS, "4.0", (void *) 4);
	writeArgToOptions(NFSTESTS_OPTIONS_RSIZE, "65536", (void *) 65536);
	writeArgToOptions(NFSTESTS_OPTIONS_WSIZE, "4096", (void *) 4096);
	writeArgToOptions(NFSTESTS_OPTIONS_DSIZE, "32k", (void *) 32768);
	writeArgToOptions(NFSTESTS_OPTIONS_READAHEAD, "100", (void *) 100);
	writeArgToOptions(NFSTESTS_OPTIONS_AC_REG_MIN, "11", (void *) &regmin);
	writeArgToOptions(NFSTESTS_OPTIONS_AC_REG_MAX, "22", (void *) &regmax);
	writeArgToOptions(NFSTESTS_OPTIONS_AC_DIR_MIN, "33", (void *) &dirmin);
	writeArgToOptions(NFSTESTS_OPTIONS_AC_DIR_MAX, "44", (void *) &dirmax);
	writeArgToOptions(NFSTESTS_OPTIONS_AC_ROOTDIR_MIN, "55", (void *) &rootdirmin);
	writeArgToOptions(NFSTESTS_OPTIONS_AC_ROOTDIR_MAX, "66", (void *) &rootdirmax);
	writeArgToOptions(NFSTESTS_OPTIONS_LOCKS_LOCAL, "locallocks", (void *) NFS_LOCK_MODE_LOCAL);
	writeArgToOptions(NFSTESTS_OPTIONS_SECURITY, "sys:krb5p:krb5i", (void *) &sec);
	writeArgToOptions(NFSTESTS_OPTIONS_ETYPE, "aes128-cts-hmac-sha1:aes256-cts-hmac-sha1-96:des3-cbc-sha1", (void *) &etype);
	writeArgToOptions(NFSTESTS_OPTIONS_MAX_GROUP_LIST, "5", (void *) 5);
	writeArgToOptions(NFSTESTS_OPTIONS_SOCKET_TYPE, "tcp", (void *) SOCK_STREAM);
	writeArgToOptions(NFSTESTS_OPTIONS_SOCKET_FAMILY, "inet", (void *) AF_INET);
	writeArgToOptions(NFSTESTS_OPTIONS_NFS_PORT, "22222", (void *) 22222);
	writeArgToOptions(NFSTESTS_OPTIONS_MOUNT_PORT, "11111", (void *) 11111);
	writeArgToOptions(NFSTESTS_OPTIONS_REQUEST_TIMEOUT, "21", (void *) &reqtimeo);
	writeArgToOptions(NFSTESTS_OPTIONS_RETRY_COUNT, "33", (void *) 33);
	writeArgToOptions(NFSTESTS_OPTIONS_DEAD_TIMEOUT, "31", (void *) &deadtime);
	writeArgToOptions(NFSTESTS_OPTIONS_FILE_HANDLE, fhstr, (void *)&fh);
	writeArgToOptions(NFSTESTS_OPTIONS_REALM, "myrealm.test.com", (void *)"@myrealm.test.com");
	writeArgToOptions(NFSTESTS_OPTIONS_PRINCIPAL, "myprincipal", (void *)"myprincipal");
	writeArgToOptions(NFSTESTS_OPTIONS_SVCPRINCIPAL, "mysvcprincipal", (void *)"mysvcprincipal");

	handle_mntopts(test_options);
	optionsVerify();
}

- (void)testMountflags0
{
	for (int i = 0; i < NFSTESTS_OPTIONS_FLAGS_NUM; i++) {
		if (i != NFSTESTS_OPTIONS_FLAGS_HARD) { /* "Soft" is already enabled */
			writeFlagToOptions(i, 0);
		}
	}

	handle_mntopts(test_options);
	optionsVerify();
}

- (void)testMountflags1
{
	for (int i = 0; i < NFSTESTS_OPTIONS_FLAGS_NUM; i++) {
		if (i != NFSTESTS_OPTIONS_FLAGS_HARD) { /* "Soft" is already enabled */
			writeFlagToOptions(i, 1);
		}
	}

	handle_mntopts(test_options);
	optionsVerify();
}

- (void)testHard
{
	writeFlagToOptions(NFSTESTS_OPTIONS_FLAGS_HARD, 1);

	handle_mntopts(test_options);
	optionsVerify();
}

- (void)testNoHard
{
	writeFlagToOptions(NFSTESTS_OPTIONS_FLAGS_HARD, 0);

	handle_mntopts(test_options);
	optionsVerify();
}

- (void)testNoAC
{
	handleTimeout(0, "noac");
}

- (void)testACTimeo
{
	handleTimeout(3, "actimeo=3");
}

- (void)testRWSize
{
	writeArgToOptions(NFSTESTS_OPTIONS_RSIZE, NULL, (void *) 1048576);
	writeArgToOptions(NFSTESTS_OPTIONS_WSIZE, NULL, (void *) 1048576);
	writeToBuf("rwsize", "1048576");

	handle_mntopts(test_options);
	optionsVerify();
}

- (void)testTCPIPV6
{
	writeArgToOptions(NFSTESTS_OPTIONS_SOCKET_TYPE, NULL, (void *) SOCK_STREAM);
	writeArgToOptions(NFSTESTS_OPTIONS_SOCKET_FAMILY, NULL, (void *) AF_INET6);
	writeToBuf("proto", "tcp6");

	handle_mntopts(test_options);
	optionsVerify();
}

- (void)testUDPIPV4
{
	writeArgToOptions(NFSTESTS_OPTIONS_SOCKET_TYPE, NULL, (void *) SOCK_DGRAM);
	writeArgToOptions(NFSTESTS_OPTIONS_SOCKET_FAMILY, NULL, (void *) AF_INET);
	writeToBuf("proto", "udp4");

	handle_mntopts(test_options);
	optionsVerify();
}

- (void)testParseOptionsLocks
{
	writeArgToOptions(NFSTESTS_OPTIONS_LOCKS_DISABLED, "nolocks", (void *) NFS_LOCK_MODE_DISABLED);
	handle_mntopts(test_options);
	optionsVerify();

	writeArgToOptions(NFSTESTS_OPTIONS_LOCKS_ENABLED, "locks", (void *) NFS_LOCK_MODE_ENABLED);
	handle_mntopts(test_options);
	optionsVerify();

	writeArgToOptions(NFSTESTS_OPTIONS_LOCKS_LOCAL, "locallocks", (void *) NFS_LOCK_MODE_LOCAL);
	handle_mntopts(test_options);
	optionsVerify();
}

- (void)testLocalNFSPort
{
	writeArgToOptions(NFSTESTS_OPTIONS_LOCAL_NFS_PORT, "/11111", (void *) "/11111");
	writeArgToOptions(NFSTESTS_OPTIONS_SOCKET_FAMILY, NULL, (void *) AF_LOCAL);

	handle_mntopts(test_options);
	optionsVerify();
}

- (void)testLocalMountPort
{
	writeArgToOptions(NFSTESTS_OPTIONS_LOCAL_MOUNT_PORT, "/22222", (void *) "/22222");
	writeArgToOptions(NFSTESTS_OPTIONS_SOCKET_FAMILY, NULL, (void *) AF_LOCAL);

	handle_mntopts(test_options);
	optionsVerify();
}

@end

/*
 * Config Read Test
 */
@interface nfsclntTests_config_read : XCTestCase

@end

int fd = -1;
char file_path[MAXPATHLEN];
struct nfs_conf_client expected_config;

@implementation nfsclntTests_config_read

- (void)setUp
{
	int err;

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

	fd = mkstemp(template);

	if (fd < 0) {
		XCTFail("Unable to create tmp file: %d", errno);
	}

	if (fcntl(fd, F_GETPATH, file_path) < 0) {
		XCTFail("Unable to get file path: %d", errno);
	}

	/* Init with defaults */
	memcpy(&expected_config, &config, sizeof(struct nfs_conf_client));
}

- (void)tearDown
{
	if (close(fd) < 0) {
		XCTFail("Unable to close fd: %d", errno);
	}
	fd = -1;

	if (remove(file_path) < 0) {
		XCTFail("Unable to remove file %s: %d", file_path, errno);
	}

	rmdir(DSTDIR);
}

- (void)testConfigRead
{
	/* Edit conf file */
	writeArgToConf(NFSTESTS_CONF_ACCESS_CACHE_TIMEOUT, "44");
	writeArgToConf(NFSTESTS_CONF_ACCESS_FOR_GETATTR, "1");
	writeArgToConf(NFSTESTS_CONF_ALLOW_ASYNC, "1");
	writeArgToConf(NFSTESTS_CONF_CALLBACK_PORT, "9999");
	writeArgToConf(NFSTESTS_CONF_INITIAL_DOWN_DELAY, "111");
	writeArgToConf(NFSTESTS_CONF_IOSIZE, "128");
	writeArgToConf(NFSTESTS_CONF_NEXT_DOWN_DELAY, "100");
	writeArgToConf(NFSTESTS_CONF_NFSIOD_THREAD_MAX, "8");
	writeArgToConf(NFSTESTS_CONF_STATFS_RATE_LIMIT, "3333");
	writeArgToConf(NFSTESTS_CONF_IS_MOBILE, "3");
	writeArgToConf(NFSTESTS_CONF_SQUISHY_FLAGS, "31");
	writeArgToConf(NFSTESTS_CONF_ROOT_STEALS_GSS_CONTEXT, "11");
	writeArgToConf(NFSTESTS_CONF_MOUNT_TIMEOUT, "13");
	writeArgToConf(NFSTESTS_CONF_MOUNT_QUICK_TIMEOUT, "3");
	writeArgToConf(NFSTESTS_CONF_DEFUALT_NFS4DOMAIN, "testdomain");

	/* Run mount_nfs.c config_read() */
	XCTAssertFalse(config_read(file_path, &config));

	/* Verify config struct initialization */
	configVerify();
}

- (void)testConfigOptions
{
	/* Init options */
	setUpOptions();

	/* write to conf file */
	writeToConf("nfs.client.mount.options", "rwsize=65536,dsize=65536");

	/* Run mount_nfs.c config_read() */
	XCTAssertFalse(config_read(file_path, &config));

	/* Initialize expected value */
	writeArgToOptions(NFSTESTS_OPTIONS_RSIZE, NULL, (void *) 65536);
	writeArgToOptions(NFSTESTS_OPTIONS_WSIZE, NULL, (void *) 65536);
	writeArgToOptions(NFSTESTS_OPTIONS_DSIZE, NULL, (void *) 65536);

	optionsVerify();
}

- (void)testConfigOptionsMultipleLines
{
	/* Init options */
	setUpOptions();

	/* write to conf file */
	writeToConf("nfs.client.mount.options", "wsize=65536");
	writeToConf("nfs.client.mount.options", "dsize=65536");
	writeToConf("nfs.client.mount.options", "rsize=65536");

	/* Run mount_nfs.c config_read() */
	XCTAssertFalse(config_read(file_path, &config));

	/* Initialize expected value */
	writeArgToOptions(NFSTESTS_OPTIONS_RSIZE, NULL, (void *) 65536);
	writeArgToOptions(NFSTESTS_OPTIONS_WSIZE, NULL, (void *) 65536);
	writeArgToOptions(NFSTESTS_OPTIONS_DSIZE, NULL, (void *) 65536);

	optionsVerify();
}

@end

/*
 * FS Locations Test
 */
@interface nfsclntTests_parse_fs_locations : XCTestCase

@end

size_t locations_len;
char locations[MAXPATHLEN];
int servcnt;
int expected_servcount;
struct nfs_fs_location *nfsl;
struct nfs_fs_location *expected_nfsl;

@implementation nfsclntTests_parse_fs_locations

- (void)setUp
{
	memset(locations, 0, sizeof(locations));
	locations_len = 0;
	expected_servcount = servcnt = 0;
	expected_nfsl = nfsl = NULL;
}

- (void)tearDown
{
	free(expected_nfsl);
}

- (void)testFSLocations
{
	writeLocation("11.0.0.1,11.0.0.2,localhost", "/tmp/path1");
	writeLocation("<mylocal1>", "/tmp/path2");
	writeLocation("fe80::a00:27ff:fe09:901c%en0,33.0.0.1", "/tmp/path3");
	writeLocation("[fe80::a00:27ff:fe09:901c%en1],fe80::1e7:cc03:4516:5396%utun2", "/tmp/path4");

	XCTAssertFalse(parse_fs_locations(locations, &nfsl));
	XCTAssertTrue(nfsl);
	XCTAssertTrue(expected_nfsl);

	getaddresslists(nfsl, &servcnt);
	XCTAssertEqual(expected_servcount, servcnt);

	LocationsVerify(nfsl, expected_nfsl);
}

- (void)testIPv4Locations
{
	writeLocation("11.0.0.1,11.0.0.2", "/tmp/path1");
	writeLocation("22.0.0.1", "/tmp/path2");
	writeLocation("localhost", "/tmp/path3");
	writeLocation("33.0.0.1,33.0.0.2", "/tmp/path4");

	XCTAssertFalse(parse_fs_locations(locations, &nfsl));
	XCTAssertTrue(nfsl);
	XCTAssertTrue(expected_nfsl);

	getaddresslists(nfsl, &servcnt);
	XCTAssertEqual(expected_servcount, servcnt);

	LocationsVerify(nfsl, expected_nfsl);
}

- (void)testIPv6Locations
{
	writeLocation("[fe80::ba0e:b31f:698b:c885%utun7],[fe80::53e6:d024:73bf:f94e%utun0]", "/tmp/path1");
	writeLocation("fe80::3600:8cb4:8ec9:d4b5%utun1,fe80::1e7:cc03:4516:5396%utun2", "/tmp/path2");
	writeLocation("fe80::a00:27ff:fe09:901c%en0", "/tmp/path3");
	writeLocation("[fe80::a00:27ff:fe09:901c%en1]", "/tmp/path4");

	XCTAssertFalse(parse_fs_locations(locations, &nfsl));
	XCTAssertTrue(nfsl);
	XCTAssertTrue(expected_nfsl);

	getaddresslists(nfsl, &servcnt);
	XCTAssertEqual(expected_servcount, servcnt);

	LocationsVerify(nfsl, expected_nfsl);
}

- (void)testLocalLocation
{
	writeLocation("<mylocal1>", "/tmp/path1");

	XCTAssertFalse(parse_fs_locations(locations, &nfsl));
	XCTAssertTrue(nfsl);
	XCTAssertTrue(expected_nfsl);

	getaddresslists(nfsl, &servcnt);
	XCTAssertEqual(expected_servcount, servcnt);

	LocationsVerify(nfsl, expected_nfsl);
}

@end
