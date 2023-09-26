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

#include "nfs_prot.h"
#include "mountd.h"

#include <sys/mount.h>
#include <nfs/rpcv2.h>
#include <pwd.h>

#include "test_utils.h"

typedef struct CLIENT CLIENT;
typedef struct nfs_fh3 nfs_fh3;

extern CLIENT *nclnt;
extern nfs_fh3 rootfh;
void
export_test_setup(void)
{
	int err;
	uid_t uid;

	// Tests should run as root
	// Enable root option using : defaults write com.apple.dt.Xcode EnableRootTesting YES
	if ((uid = getuid()) != 0) {
		XCTFail("Test should run as root, current user %d", uid);
		return;
	}

	err = create_root_dir();
	if (err != 0) {
		XCTFail("creating root dir failed");
	}
	// Create temporary folder name
	rootdir = mkdtemp(template);
	if (!rootdir) {
		XCTFail("Unable to create tmpdir: %d", errno);
		return;
	}

	// Create temporary folder
	testDirFD = open(rootdir, O_DIRECTORY | O_SEARCH, 0777);
	if (testDirFD < 0) {
		XCTFail("Unable to open tmpdir (%s): %d", rootdir, errno);
		return;
	}

	err = create_exports_file();
	if (err) {
		XCTFail("cannot create exports file");
		return;
	}
}

void
export_test_teardown(void)
{
	remove_exports_file();
	if (rootdir) {
		if (rmdir(rootdir) < 0) {
			XCTFail("Unable to remove dir %s: %d", rootdir, errno);
		}
		rootdir = NULL;
	}

	rmdir(DSTDIR);
}

static void
test_exports_list_cleanup(export_t *exports, const char ** dirs, int dirs_cnt, int expected, int cleanup)
{
	int err;

	err = create_dirs(dirs, dirs_cnt);
	if (err != 0) {
		XCTFail("create_dirs failed: %d", err);
		return;
	}

	//create a new exports file if an old file exists
	remove_exports_file();
	create_exports_file();

	//write the content to the file
	err = write_to_exports_file(exports);
	if (err != 0) {
		XCTFail("write_exports_file failed: %d", err);
	}
	char *argv[5] = { "nfsd", "-vvvvvv", "-F", exportsPath, "checkexports"};
	nfsrvtests_run_command(nfsd_imp, ARRAY_SIZE(argv), argv, expected);

	if (cleanup) {
		remove_dirs(dirs, dirs_cnt);
	}
}

// Note that the return value of checkexports is the number of errors in the file
static void
test_exports_list(export_t *exports, const char ** dirs, int dirs_cnt, int expected)
{
	test_exports_list_cleanup(exports, dirs, dirs_cnt, expected, true);
}


@interface nfsrvTests_exports : XCTestCase

@end

@implementation nfsrvTests_exports

- (void)setUp
{
	export_test_setup();
}

- (void)tearDown
{
	export_test_teardown();
}

- (void)testExportsBasic;
{
	export_t exports = {
		{ "dir", NULL },
		"",
		"",
		""
	};
	test_exports_list(&exports, exports.paths, 1, 0);
}

/*
 * list the same dir twice in the same line
 * expect 1 error
 */
- (void)testExportsDupDir;
{
	export_t exports = {
		{ "dir", "dir", NULL },
		"",
		"",
		""
	};
	test_exports_list(&exports, exports.paths, 1, 1);
}

/*
 * create a directory and a symbolic link to it
 * export the symbolic link
 * expect 2 errors:
 *      1) cannot export a symlink
 *      2) no directories exported
 */
- (void)testExportsSymlink;
{
	int err;
	char dirPath[PATH_MAX] = {0};
	char linkPath[PATH_MAX] = {0};

	//Create the directory and its symlink paths
	snprintf(dirPath, PATH_MAX, "%s/%s", rootdir, "dir");
	snprintf(linkPath, PATH_MAX, "%s/%s", rootdir, "link");

	//Create the directory and the symlink
	err = mkdirat(testDirFD, "dir", 0777);
	if (err != 0) {
		XCTFail("mkdirat failed with %d", err);
	}
	err = symlink(dirPath, linkPath);
	if (err != 0) {
		XCTFail("mkdirat failed with %d", err);
	}
	export_t exports = {
		{"link", NULL },
		"",
		"",
		""
	};
	test_exports_list(&exports, NULL, 0, 2);

	rmdir(dirPath);
	unlink(linkPath);
}

/*
 * export a directory with a name length equal to RPCMNT_NAMELEN
 * expect no error
 * export a directory with a name length equal to RPCMNT_NAMELEN + 1
 * expect 1 error
 */
- (void)testExportsDirNameTooLong;
{
	char name[RPCMNT_NAMELEN] = {0};
	//the 'rootdir' does not include the '/' at the end
	unsigned long rootdir_pathname_len = strlen(rootdir);

	memset(name, 'a', RPCMNT_NAMELEN - rootdir_pathname_len - 1);
	export_t exports = {
		{ name, NULL },
		"",
		"",
		""
	};
	test_exports_list(&exports, exports.paths, 1, 0);

	memset(name, 'a', RPCMNT_NAMELEN - rootdir_pathname_len);
	test_exports_list(&exports, exports.paths, 1, 1);
}

/*
 * create a file with a comment only
 * expect no error
 */
- (void)testExportsComment;
{
	export_t exports = {
		{ NULL },
		"#comment",
		"",
		""
	};
	test_exports_list(&exports, exports.paths, 0, 0);
}

/*
 * export a nonexistent dir
 * expect 1 error
 */
- (void)testExportsNoEntry;
{
	export_t exports = {
		{ "dir", NULL },
		"",
		"",
		""
	};
	test_exports_list(&exports, NULL, 0, 1);
}

/*
 * create an empty exports file
 * expect no errors
 */
- (void)testExportsEmptyFile;
{
	export_t exports = {
		{ "", NULL },
		"",
		"",
		""
	};
	test_exports_list(&exports, NULL, 0, 0);
}

/*
 * export two directories
 * expect no erros
 */
- (void)testExportsTwoDirs;
{
	export_t exports = {
		{ "dir1", "dir2", NULL },
		"",
		"",
		""
	};
	test_exports_list(&exports, exports.paths, 2, 0);
}

/*
 * export to localhost
 * expect no errors
 */
- (void)testExportsLocalhost;
{
	char path[PATH_MAX] = {0};
	export_t exports = {
		{ "dir", NULL },
		"",
		"localhost",
		""
	};
	test_exports_list_cleanup(&exports, exports.paths, 1, 0, false);

	snprintf(path, sizeof(path), "%s/dir", rootdir);
	const char *argv[1] = { "-vvvvv" };
	doNFSSetUp(path, argv, ARRAY_SIZE(argv), AF_INET, SOCK_STREAM, RPCAUTH_UNIX, 0, exportsPath, "sys");
	doMountTearDownCloseFD(false);
	remove_dirs(exports.paths, 1);
	destroy_nclnt();
}

/*
 * export to an IP address
 * expect no errors
 */
- (void)testExportsIP;
{
	char path[PATH_MAX] = {0};

	export_t exports = {
		{ "dir", NULL },
		"",
		"127.0.0.1",
		""
	};
	test_exports_list_cleanup(&exports, exports.paths, 1, 0, false);

	snprintf(path, sizeof(path), "%s/dir", rootdir);
	const char *argv[1] = { "-vvvvv" };

	//verify the mount
	doNFSSetUp(path, argv, ARRAY_SIZE(argv), AF_INET, SOCK_STREAM, RPCAUTH_UNIX, 0, exportsPath, "sys");
	//cleanup
	doMountTearDownCloseFD(false);
	remove_dirs(exports.paths, 1);
	destroy_nclnt();
}


/*
 * export to an invalid IP address
 * expect 2 errors
 *      1) cannot find address for the invalid IP
 *      2) no hosts to export to
 */
- (void)testExportsInvalidAddress;
{
	export_t exports = {
		{ "dir", NULL },
		"",
		"1951.1894.2894.289484",
		""
	};
	test_exports_list(&exports, exports.paths, 1, 2);
}

/*
 * export to apple.com
 * expect no erros
 */
- (void)testExportsDomain;
{
	export_t exports = {
		{ "dir", NULL },
		"",
		"apple.com",
		""
	};
	test_exports_list(&exports, exports.paths, 1, 0);
}

#if 0
/*
 * this test is commented out because we don't want to change system config files
 */
- (void)testExportsnetgroup;
{
	int err;
	export_t exports = {
		{ "dir", NULL },
		"",
		"roots",
		""
	};

	err = write_netgroup_file();
	if (err) {
		XCTFail("failed");
		return;
	}
	test_exports_list(&exports, exports.paths, 1, 0);
	remove_netgroup_file();
}
#endif /* #if 0 */

/*
 * export to a nonexistent domain
 * expect 2 erros
 * 1) cannot find address of nodomain.nodomain
 * 2) no hosts to export to
 */
- (void)testExportsNoDomain;
{
	export_t exports = {
		{ "dir", NULL },
		"",
		"nodomain.nodomain",
		""
	};
	test_exports_list(&exports, exports.paths, 1, 2);
}

/*
 * export to a network and a domain
 * expect 1 error: can't specify both network and hosts on same line
 */
- (void)testExportsDomainAndNetwork;
{
	export_t exports = {
		{ "dir", NULL },
		"",
		"apple.com -network=127.0.0.1",
		"-mask=255.255.255.0"
	};
	test_exports_list(&exports, exports.paths, 1, 1);
}

/*
 * export to the same host twice
 * expect no errors
 */
- (void)testExportsDomainDup;
{
	export_t exports = {
		{ "dir", NULL },
		"",
		"apple.com apple.com",
		""
	};
	test_exports_list(&exports, exports.paths, 1, 0);
}

/*
 * export to host with no directories
 * expect 1 error: no dir
 */
- (void)testExportsDomainNoDirs;
{
	export_t exports = {
		{ NULL },
		"",
		"apple.com",
		""
	};
	test_exports_list(&exports, exports.paths, 0, 1);
}

- (void)testExportsNetwork;
{
	export_t exports = {
		{ "dir", NULL },
		"",
		"-network=192.0.0.0",
		"-mask=255.255.255.0"
	};
	test_exports_list(&exports, exports.paths, 1, 0);
}

- (void)testExportsNetworkTwoMasks;
{
	export_t exports = {
		{ "dir", NULL },
		"",
		"-network=192.0.0.0",
		"-mask=255.255.0.0 -mask=255.0.0.0"
	};
	test_exports_list(&exports, exports.paths, 1, 1);
}

- (void)testExportsNetworkBadMask;
{
	export_t exports = {
		{ "dir", NULL },
		"",
		"-network=192.0.0.0",
		"-mask=500.500.500.0"
	};
	test_exports_list(&exports, exports.paths, 1, 1);
}

- (void)testExportBadNetwork;
{
	export_t exports = {
		{ "dir", NULL },
		"",
		"-network=500.0.0.0",
		"-mask=255.255.255.0"
	};
	test_exports_list(&exports, exports.paths, 1, 1);
}

- (void)testExportsTwoNetworks;
{
	export_t exports = {
		{ "dir", NULL },
		"",
		"-network=169.0.0.0 -network=192.0.0.0",
		"-mask=255.255.255.0"
	};
	test_exports_list(&exports, exports.paths, 1, 1);
}

/*
 * export with a network mask but with no network
 * expect 1 error
 */
- (void)testExportsMaskNoNetwork;
{
	export_t exports = {
		{ "dir", NULL },
		"",
		"",
		"-mask=255.255.255.0"
	};

	test_exports_list(&exports, exports.paths, 1, 1);
}

/*
 * export with illegal flag -x
 * expect no errors
 * checkexports only issues a warning for illegal flags
 */
- (void)testExportsIllegalOption;
{
	export_t exports = {
		{ "dir", NULL },
		"-x",
		"",
		""
	};
	test_exports_list(&exports, exports.paths, 1, 0);
}

CREATE3res *
doCreateRPC(CLIENT *clnt, nfs_fh3 *dir, char *name, struct createhow3 *how);
/*
 * basic test for options
 * expect no errors
 */
- (void)testExportsReadOnly;
{
	char path[PATH_MAX] = {0};
	CREATE3res *res;
	char *file = "new_file";
	struct createhow3 how = {};

	export_t exports = {
		{ "dir", NULL },
		"-ro",
		"localhost",
		""
	};
	test_exports_list_cleanup(&exports, exports.paths, 1, 0, false);

	snprintf(path, sizeof(path), "%s/dir", rootdir);
	const char *argv[1] = { "-vvvvv" };
	doNFSSetUp(path, argv, ARRAY_SIZE(argv), AF_INET, SOCK_STREAM, RPCAUTH_UNIX, 0, exportsPath, "sys");

	how.mode = UNCHECKED;
	how.createhow3_u.obj_attributes.uid.set_it = true;
	how.createhow3_u.obj_attributes.uid.set_uid3_u.uid = 1;
	how.createhow3_u.obj_attributes.gid.set_it = true;
	how.createhow3_u.obj_attributes.gid.set_gid3_u.gid = 2;

	res = doCreateRPC(nclnt, &rootfh, file, &how);
	if (res->status != NFS3ERR_ROFS) {
		XCTFail("doCreateRPC failed, expected %d got %d", NFS3ERR_ROFS, res->status);
	}
	doMountTearDownCloseFD(false);
	remove_dirs(exports.paths, 1);
	destroy_nclnt();
}

/*
 * list a directory after the options
 * expect 1 error, directories must appear before options
 */
- (void)testExportsDirAfterOptions;
{
	export_t exports = {
		{  NULL },
		"/tmp -ro /tmp",
		"",
		""
	};
	test_exports_list(&exports, NULL, 0, 1);
}

/*
 * tests for maproot and mapall flags
 */
- (void)testExportsMaprootAll;
{
	char path[PATH_MAX] = {0};
	CREATE3res *res;
	char *file = "new_file";
	struct createhow3 how = {};

	export_t exports = {
		{ "dir", NULL },
		"-maproot=root",
		"",
		""
	};
	//specify maproot=root, expect no erross
	test_exports_list(&exports, exports.paths, 1, 0);

	//specify maproot=root:group, expect no erross
	exports.flags = "-maproot=root:group";
	test_exports_list(&exports, exports.paths, 1, 0);

	//specify nonexistent user for maproot, expect 1 erross
	exports.flags = "-maproot=noUser";
	test_exports_list(&exports, exports.paths, 1, 1);

	//specify no user for maproot, expect 1 erross
	exports.flags = "-maproot=";
	test_exports_list(&exports, exports.paths, 1, 1);

	//specify mapall=root, expect no erross
	exports.flags = "-mapall=root";
	test_exports_list(&exports, exports.paths, 1, 0);

	//specify mapall=root:group, expect no erross
	exports.flags = "-mapall=root:group";
	test_exports_list(&exports, exports.paths, 1, 0);

	//specify nonexistent user for mapall, expect 1 erross
	exports.flags = "-mapall=noUser";
	test_exports_list(&exports, exports.paths, 1, 1);

	//specify no user for mapall, expect 1 erross
	exports.flags = "-mapall=";
	test_exports_list(&exports, exports.paths, 1, 1);

	/*
	 * specify mapall and maproot, expect 1 error
	 * these are mutually exclusive options
	 */
	exports.flags = "-mapall=root -maproot=root";
	test_exports_list(&exports, exports.paths, 1, 1);

	//test with actual mount

	//map all users to nobody
	exports.flags = "-mapall=nobody";
	test_exports_list_cleanup(&exports, exports.paths, 1, 0, false);

	snprintf(path, sizeof(path), "%s/dir", rootdir);
	const char *argv[1] = { "-vvvvv" };

	doNFSSetUp(path, argv, ARRAY_SIZE(argv), AF_INET, SOCK_STREAM, RPCAUTH_UNIX, 0, exportsPath, "sys");

	//prepare CreateRPC params
	how.mode = UNCHECKED;
	how.createhow3_u.obj_attributes.uid.set_it = true;
	how.createhow3_u.obj_attributes.uid.set_uid3_u.uid = 1;
	how.createhow3_u.obj_attributes.gid.set_it = true;
	how.createhow3_u.obj_attributes.gid.set_gid3_u.gid = 2;

	//we chmod the file/dir so the user nobody can access them
	fchmod(testDirFD, 0777);
	chmod(path, 0777);
	chmod(DSTDIR, 0777);

	//Create a new file
	res = doCreateRPC(nclnt, &rootfh, file, &how);
	if (res->status != NFS3_OK) {
		XCTFail("doCreateRPC failed, got %d", res->status);
	}

	//get the file path
	strlcat(path, "/", PATH_MAX);
	strlcat(path, file, PATH_MAX);


	//stat the file
	struct stat info;
	stat(path, &info);
	struct passwd* pwd;

	pwd = getpwuid(info.st_uid);

	//verify that the user is nobody
	if (strcmp(pwd->pw_name, "nobody") != 0) {
		XCTFail("expected user: nobody, got <%s>", pwd->pw_name);
	}

	//cleanup
	remove(path);
	doMountTearDownCloseFD(false);
	remove_dirs(exports.paths, 1);
	destroy_nclnt();
}

/*
 * export with the rest of the options
 * expect no erros
 */
- (void)testExportsAllOptions;
{
	export_t exports = {
		{ "dir", NULL },
		"-alldirs -ro -32bitclients -manglednames -offline",
		"",
		""
	};
	test_exports_list(&exports, exports.paths, 1, 0);
}

/*
 * export no directory with options
 * expoect 1 error
 */
- (void)testExportsOptionsNoDir;
{
	export_t exports = {
		{ NULL },
		"-alldirs -ro -32bitclients -manglednames -offline",
		"",
		""
	};
	test_exports_list(&exports, exports.paths, 0, 1);
}

/*
 * test sec export option
 */
- (void)testExportsSec;
{
	char path[PATH_MAX] = {0};

	export_t exports = {
		{ "dir", NULL },
		"-sec=sys:krb5:krb5p:krb5i",
		"",
		""
	};
	// test with all sec options specified, expect no errors
	test_exports_list(&exports, exports.paths, 1, 0);

	// test with no sec option specified, expect 1 errors
	exports.flags = "-sec=";
	test_exports_list(&exports, exports.paths, 1, 1);

	// test with a noneexistent sec option specified, expect 1 errors
	exports.flags = "-sec=noSec";
	test_exports_list(&exports, exports.paths, 1, 1);

	// test with each sec option specified twice, expect no errors, only warnings
	exports.flags = "-sec=sys:krb5:krb5p:krb5i:sys:krb5:krb5p:krb5i";
	test_exports_list(&exports, exports.paths, 1, 0);

	// test with sec option specified twice, expect no errors, only warnings
	exports.flags = "-sec=sys -sec=krb5";
	test_exports_list(&exports, exports.paths, 1, 0);

	exports.flags = "-sec=sys:krb5:krb5p:krb5i";
	test_exports_list_cleanup(&exports, exports.paths, 1, 0, false);

	snprintf(path, sizeof(path), "%s/dir", rootdir);
	const char *argv[1] = { "-vvvvv" };

	//verify the mount
	doNFSSetUp(path, argv, ARRAY_SIZE(argv), AF_INET, SOCK_STREAM, RPCAUTH_UNIX, 0, exportsPath, "sys:krb5:krb5p:krb5i");
	//cleanup
	doMountTearDownCloseFD(false);
	remove_dirs(exports.paths, 1);
	destroy_nclnt();
}

// test fspath option
- (void)testExportsFSPath;
{
	int prefix_len = strlen("-fspath=");
	char options[PATH_MAX + 1 + prefix_len];
	export_t exports = {
		{"dir", NULL },
		"",
		"",
		""
	};

	// specify the correct orrect FS path, expect no errors
	exports.flags = "-fspath=/System/Volumes/Data";
	test_exports_list(&exports, exports.paths, 1, 0);

	// specify the wrong orrect FS path, expect 1 errors
	exports.flags = "-fspath=/System/Volumes/Recovery";
	test_exports_list(&exports, exports.paths, 1, 1);

	// use fspath option with no path, expect 1 errors
	exports.flags = "-fspath=";
	test_exports_list(&exports, exports.paths, 1, 1);

	snprintf(options, sizeof(options), "-fspath=");
	char *ptr = options + prefix_len;
	ptr[0] = '/';
	memset(ptr + 1, 'a', sizeof(options) - 1 - prefix_len);

	exports.flags = options;
	test_exports_list(&exports, NULL, 0, 1);
}

// test uuid option
- (void)testExportsUUID;
{
	struct statfs fsb;
	char buf[64];
	u_char uuid[16];
	char options[80] = {0};

	// find the UUID
	mountd_statfs("/System/Volumes/Data", &fsb);
	get_uuid_from_diskarb(fsb.f_mntfromname, uuid);
	uuidstring(uuid, buf);
	sprintf(options, "%s%s", "-fsuuid=", buf);

	export_t exports = {
		{"dir", NULL },
		options,
		"",
		""
	};

	//send uuid option correctly, expect no error
	test_exports_list(&exports, exports.paths, 1, 0);

	//corrupt the UUID
	uuid[0]++;
	uuidstring(uuid, buf);
	sprintf(options, "%s%s", "-fsuuid=", buf);

	//send the corrupted UUID, expect 1 error
	test_exports_list(&exports, exports.paths, 1, 1);

	// use fsuuid option with no uuid, expect 1 errors
	exports.flags = "-fsuuid=";
	test_exports_list(&exports, exports.paths, 1, 1);
}

@end
