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

#include "test_utils.h"
#include "pathnames.h"
#include "mountd_rpc.h"
#include <nfs/rpcv2.h>

char *rootdir;
char exportsPath[PATH_MAX];
int exportsFD, testDirFD;
int active_sec[4] = {0};

int
unlink_cb(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf)
{
	int rv = remove(path);

	if (rv) {
		XCTFail("remove failed %d", rv);
	}

	return rv;
}

int
create_root_dir(void)
{
	int err;
	// Remove all test remainings
	err = nftw(DSTDIR, unlink_cb, 64, FTW_MOUNT | FTW_DEPTH | FTW_PHYS);
	if (err == 0) {
		/* cleanup succeeded */
		sleep(1);
	} else if (err < 0 && errno != ENOENT) {
		XCTFail("nftw failed %d, %d", err, errno);
		return -1;
	}

	err = mkdir(DSTDIR, 0777);
	if (err < 0) {
		XCTFail("Unable to create root dir %s %d", DSTDIR, errno);
		return -1;
	}
	return 0;
}

void
remove_dirs(const char **dirs, int dir_count)
{
	int i, err;
	for (i = 0; i < dir_count; i++) {
		err = unlinkat(testDirFD, dirs[i], AT_REMOVEDIR);
		if (err) {
			XCTFail("unlinkat failed");
		}
	}
}

int
create_dirs(const char **dirs, int dir_count)
{
	int i = 0;
	int err;

	if (dirs == NULL) {
		if (dir_count == 0) {
			return 0;
		} else {
			return -1;
		}
	}
	for (i = 0; i < dir_count; i++) {
		err = mkdirat(testDirFD, dirs[i], 0777);
		if (err < 0) {
			XCTFail("failed creating dir: %s with error: %d, removing all created dirs", dirs[i], errno);
			//remove dirs that were created successfully
			remove_dirs(dirs, i - 1);
			return -1;
		}
	}
	return 0;
}

int
write_to_exports_file(export_t *exports)
{
	int n;
	// Create exports content
	for (int j = 0; j < ARRAY_SIZE(exports->paths) && exports->paths[j] != NULL; j++) {
		n = dprintf(exportsFD, "%s/%s ", rootdir, exports->paths[j]);
		if (n <= 0) {
			XCTFail("Error during dprintf %d", errno);
			return -1;
		}
	}
	n = dprintf(exportsFD, "%s %s %s\n", exports->flags, exports->network, exports->mask);
	if (n <= 0) {
		XCTFail("Error during dprintf %d", errno);
		return -1;
	}
	return 0;
}

int
create_exports_file(void)
{
	// Create full exports path
	snprintf(exportsPath, sizeof(exportsPath), "%s/%s", rootdir, EXPORTS);

	exportsFD = openat(testDirFD, EXPORTS, O_CREAT | O_RDWR | O_EXCL, 0666);
	if (exportsFD < 0) {
		XCTFail("Unable to create exports (%s/%s): %d", rootdir, EXPORTS, errno);
		return -1;
	}
	return 0;
}

void
remove_exports_file(void)
{
	int err;
	if (exportsFD >= 0) {
		close(exportsFD);
		exportsFD = -1;
		err = unlink(exportsPath);
		if (err) {
			XCTFail("unlink %s failed with %d", exportsPath, err);
		}
		bzero(exportsPath, sizeof(exportsPath));
	}
}

void
nfsrvtests_run_command(int (*cmd)(int __argc, char *__argv[], const char* conf_path), int _argc, char *_argv[], int expected_result)
{
	int status = 0;
	pid_t childPid;

	if ((childPid = fork()) == 0) {
		/* Child process */
		cmd(_argc, _argv, _PATH_NFS_CONF);
	} else {
		/* Now wait for the child to finish. */
		while (waitpid(childPid, &status, WUNTRACED) < 0) {
			if (errno == EINTR) {
				continue;
			}
		}
	}

	if (WIFEXITED(status)) {
		XCTAssertEqual(WEXITSTATUS(status), expected_result, "run_forked_command failed");
	} else if (WIFSIGNALED(status)) {
		XCTAssertEqual(WTERMSIG(status), 0, "run_forked_command subprocess terminated %s", strsignal(WTERMSIG(status)));
	} else if (WIFSTOPPED(status)) {
		XCTAssertEqual(WSTOPSIG(status), 0, "run_forked_command subprocess stopped %s", strsignal(WSTOPSIG(status)));
	} else {
		XCTFail("run_forked_command subprocess got unknow status: 0x%08x", status);
	}
}

#if 0
int
write_netgroup_file(void)
{
	int n;
	int fd = open("/etc/netgroup", O_CREAT | O_RDWR | O_EXCL, 0666);
	if (fd < 0) {
		XCTFail("unable to open file: %d", fd);
		return -1;
	}
	n = dprintf(fd, "roots (apple.com,root,apple.com)");
	if (n <= 0) {
		XCTFail("Error during dprintf %d", errno);
		close(fd);
		return -1;
	}
	close(fd);
	return 0;
}

int
remove_netgroup_file(void)
{
	return unlink("/etc/netgroup");
}
#endif /* #if 0 */

static void
activate_sec(char* sec, int *arr)
{
	if (strcmp(sec, "sys") == 0) {
		arr[0] = 1;
	} else if (strcmp(sec, "krb5") == 0) {
		arr[1] = 1;
	} else if (strcmp(sec, "krb5i") == 0) {
		arr[2] = 1;
	} else if (strcmp(sec, "krb5p") == 0) {
		arr[3] = 1;
	}
}

static int
sec_index_to_val(int idx)
{
	switch (idx) {
	case 0:
		return RPCAUTH_SYS;
	case 1:
		return RPCAUTH_KRB5;
	case 2:
		return RPCAUTH_KRB5I;
	case 3:
		return RPCAUTH_KRB5P;
	}
	return 0;
}

static int
sec_val_to_idx(int sec_val)
{
	switch (sec_val) {
	case RPCAUTH_SYS:
		return 0;
	case RPCAUTH_KRB5:
		return 1;
	case RPCAUTH_KRB5I:
		return 2;
	case RPCAUTH_KRB5P:
		return 3;
	default:
		return -1;
	}
}

static const char *
sec_flavor_name(uint32_t flavor)
{
	switch (flavor) {
	case RPCAUTH_NONE:      return "none";
	case RPCAUTH_SYS:       return "sys";
	case RPCAUTH_KRB5:      return "krb5";
	case RPCAUTH_KRB5I:     return "krb5i";
	case RPCAUTH_KRB5P:     return "krb5p";
	default:                return "?";
	}
}

int
verify_security_flavors(int *flavors, int flavors_cnt, char *expected_flavors)
{
	int exp_arr[4] = {0};
	int recieved_arr[4] = {0};
	char *sep_p = NULL;
	char expected_cpy[50] = {0};
	char *curr_mech = expected_cpy;
	int idx = 0;
	int err = 0;
	/* We want to write to the array, so use a copy */
	strncpy(expected_cpy, expected_flavors, ARRAY_SIZE(expected_cpy));
	while (*curr_mech) {
		sep_p = strchr(curr_mech, ':');
		if (sep_p == NULL) {
			/* last mechanism */
			activate_sec(curr_mech, exp_arr);
			break;
		} else {
			*sep_p = 0;
			activate_sec(curr_mech, exp_arr);
			curr_mech = sep_p + 1;
		}
	}

	for (idx = 0; idx < flavors_cnt; idx++) {
		recieved_arr[sec_val_to_idx(flavors[idx])] = 1;
	}

	for (idx = 0; idx < ARRAY_SIZE(exp_arr); idx++) {
		if (exp_arr[idx] == 1 && recieved_arr[idx] == 0) {
			XCTFail("expected security mechanism %s but not recieved", sec_flavor_name(sec_index_to_val(idx)));
			err = -1;
		} else if (exp_arr[idx] == 0 && recieved_arr[idx] == 1) {
			XCTFail("security mechanism %s recieved but not expected", sec_flavor_name(sec_index_to_val(idx)));
			err = -1;
		}
	}
	return err;
}
