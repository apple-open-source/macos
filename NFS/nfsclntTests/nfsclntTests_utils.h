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

#ifndef nfsclntTests_utils_h
#define nfsclntTests_utils_h

#include "../mount_nfs/mount_nfs.h"

#define RESULT_SUCCESS 0

struct statfs;

/*
 * Mount Tests
 */
typedef NSString* (*verifyArgsFunc) (NSDictionary *mountArgs, char **nfsOutArgs);
NSString* nfsParameterVerifier(NSDictionary *mountArgs, char **nfsOutArgs);
NSString* mountParameterVerifier(NSDictionary *mountArgs, char **nfsOutArgs);

void    nfstests_unmount(char *mountPath);
void    nfstests_init_input_args(char *dstpath, char **nfsArgsIn);
void    nfstests_get_mount(char *mountPath, struct statfs *mountStat);
void    nfstests_run_command(int (*cmd)(int __argc, char *__argv[]), int _argc, char *_argv[], int expected_result);
void    nfstests_verify_arg(verifyArgsFunc verifier, NSString *srcPath, NSString *dstPath, char **nfsArgs);

/*
 * Parse Options Tests
 */
typedef enum {
	NFSTESTS_OPTIONS_MNT_FLAGS = 0,
	NFSTESTS_OPTIONS_MATTR,
	NFSTESTS_OPTIONS_MLAGS_MASK,
	NFSTESTS_OPTIONS_MLAGS,
	NFSTESTS_OPTIONS_NFSVERS,
	NFSTESTS_OPTIONS_RSIZE,
	NFSTESTS_OPTIONS_WSIZE,
	NFSTESTS_OPTIONS_DSIZE,
	NFSTESTS_OPTIONS_READAHEAD,
	NFSTESTS_OPTIONS_AC_REG_MIN,
	NFSTESTS_OPTIONS_AC_REG_MAX,
	NFSTESTS_OPTIONS_AC_DIR_MIN,
	NFSTESTS_OPTIONS_AC_DIR_MAX,
	NFSTESTS_OPTIONS_LOCKS_ENABLED,
	NFSTESTS_OPTIONS_LOCKS_LOCAL,
	NFSTESTS_OPTIONS_LOCKS_DISABLED,
	NFSTESTS_OPTIONS_SECURITY,
	NFSTESTS_OPTIONS_ETYPE,
	NFSTESTS_OPTIONS_MAX_GROUP_LIST,
	NFSTESTS_OPTIONS_SOCKET_TYPE,
	NFSTESTS_OPTIONS_SOCKET_FAMILY,
	NFSTESTS_OPTIONS_NFS_PORT,
	NFSTESTS_OPTIONS_MOUNT_PORT,
	NFSTESTS_OPTIONS_REQUEST_TIMEOUT,
	NFSTESTS_OPTIONS_RETRY_COUNT,
	NFSTESTS_OPTIONS_DEAD_TIMEOUT,
	NFSTESTS_OPTIONS_FILE_HANDLE,
	NFSTESTS_OPTIONS_REALM,
	NFSTESTS_OPTIONS_PRINCIPAL,
	NFSTESTS_OPTIONS_SVCPRINCIPAL,
	NFSTESTS_OPTIONS_LOCAL_NFS_PORT,
	NFSTESTS_OPTIONS_LOCAL_MOUNT_PORT,
	NFSTESTS_OPTIONS_NUM_OPTIONS
} nfstests_options_kind_t;

typedef enum {
	NFSTESTS_OPTIONS_FLAGS_SOFT = 0,
	NFSTESTS_OPTIONS_FLAGS_HARD,
	NFSTESTS_OPTIONS_FLAGS_INTR,
	NFSTESTS_OPTIONS_FLAGS_RESVPORT,
	NFSTESTS_OPTIONS_FLAGS_CONNECT,
	NFSTESTS_OPTIONS_FLAGS_DUMBTIMER,
	NFSTESTS_OPTIONS_FLAGS_RDIRPLUS,
	NFSTESTS_OPTIONS_FLAGS_NEGNAMECACHE,
	NFSTESTS_OPTIONS_FLAGS_MUTEJUKEBOX,
	NFSTESTS_OPTIONS_FLAGS_CALLBACK,
	NFSTESTS_OPTIONS_FLAGS_NAMEDATTR,
	NFSTESTS_OPTIONS_FLAGS_ACL,
	NFSTESTS_OPTIONS_FLAGS_ACLONLY,
	NFSTESTS_OPTIONS_FLAGS_NFC,
	NFSTESTS_OPTIONS_FLAGS_QUOTA,
	NFSTESTS_OPTIONS_FLAGS_MNTUDP,
	NFSTESTS_OPTIONS_FLAGS_OPAQUE_AUTH,
	NFSTESTS_OPTIONS_FLAGS_NUM
} nfstests_options_flags_kind_t;

void setUpOptions(void);
void optionsVerify(void);
void handleTimeout(long seconds, char *value);
void writeToBuf(const char *name, const char *value);
void writeFlagToOptions(nfstests_options_flags_kind_t option, int enable);
void writeArgToOptions(nfstests_options_kind_t option, const char *value, void *expected_value);

/*
 * Config Read Tests
 */
typedef enum {
	NFSTESTS_CONF_ACCESS_CACHE_TIMEOUT = 0,
	NFSTESTS_CONF_ACCESS_FOR_GETATTR,
	NFSTESTS_CONF_ALLOW_ASYNC,
	NFSTESTS_CONF_CALLBACK_PORT,
	NFSTESTS_CONF_INITIAL_DOWN_DELAY,
	NFSTESTS_CONF_IOSIZE,
	NFSTESTS_CONF_NEXT_DOWN_DELAY,
	NFSTESTS_CONF_NFSIOD_THREAD_MAX,
	NFSTESTS_CONF_STATFS_RATE_LIMIT,
	NFSTESTS_CONF_IS_MOBILE,
	NFSTESTS_CONF_SQUISHY_FLAGS,
	NFSTESTS_CONF_ROOT_STEALS_GSS_CONTEXT,
	NFSTESTS_CONF_MOUNT_TIMEOUT,
	NFSTESTS_CONF_MOUNT_QUICK_TIMEOUT,
	NFSTESTS_CONF_DEFUALT_NFS4DOMAIN,
	NFSTESTS_CONF_NUM_OPTIONS
} nfstests_conf_kind_t;

void configVerify(void);
void writeToConf(const char *name, const char *value);
void writeArgToConf(nfstests_conf_kind_t option, const char *value);

/*
 * FS Locations Test
 */
void writeLocation(const char *hosts, const char *path);
void LocationsVerify(struct nfs_fs_location * nfsl1, struct nfs_fs_location *nfsl2);
#endif /* nfsclntTests_utils_h */
