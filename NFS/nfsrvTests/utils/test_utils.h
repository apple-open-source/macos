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

#ifndef test_utils_h
#define test_utils_h

#include <ftw.h>
#include <XCTest/XCTest.h>

/* Tests globals */
#define DSTDIR       "/System/Volumes/Data/private/tmp/nfsrvtest"
#define TEMPLATE     DSTDIR "/nfsrvtest.XXXXXXXX"
#define TEMPLATE_LEN 61
#define EXPORTS      "exports"

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(A) (sizeof(A) / sizeof(A[0]))
#endif

static char template[TEMPLATE_LEN + 1] = TEMPLATE;
extern char *rootdir;
extern char exportsPath[PATH_MAX];
extern int exportsFD, testDirFD;

// Exports content
typedef struct {
	const char *paths[10]; // last element must be NULL
	const char *flags;
	const char *network;
	const char *mask;
} export_t;

int
unlink_cb(const char *path, const struct stat *sb, int typeflag, struct FTW *ftwbuf);
int
create_root_dir(void);
int
write_to_exports_file(export_t *exports);
int
create_exports_file(void);
void
remove_exports_file(void);
int
create_dirs(const char **dirs, int dir_count);
void
remove_dirs(const char **dirs, int dir_count);
void
nfsrvtests_run_command(int (*cmd)(int __argc, char *__argv[], const char* conf_path), int _argc, char *_argv[], int expected_result);

int
nfsd_imp(int argc, char *argv[], const char *conf_path);

//for uuid tests
int
mountd_statfs(const char *path, struct statfs *sb);
int
get_uuid_from_diskarb(const char *path, u_char *uuid);
char *
uuidstring(u_char *uuid, char *string);

void
doMountExport(const char **nfsdArgs, int nfsdArgsSize, char* exports_file);

void
doNFSSetUp(const char* path, const char **nfsdArgs, int nfsdArgsSize, int socketFamily, int socketType, int authType, int flags, char *exports_file, char *sec_mech);

void
doMountTearDownCloseFD(int closeFD);
void
destroy_nclnt(void);

int verify_security_flavors(int *flavors, int flavors_cnt, char *expected_flavors);
#endif /* test_utils_h */
