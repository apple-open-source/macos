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

#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>
#include <sys/mount.h>
#include <sys/un.h>
#include <netdb.h>

#include "nfsclntTests_utils.h"

typedef enum {
    INT,
    UINT32,
    BITMAP,
    FILE_HANDLE,
    SEC,
    CHAR_PTR,
    ETYPE,
    TIMESPEC,
    NUM_TYPES
} type_t;

size_t types_to_size[NUM_TYPES] = {
    sizeof(int),                 /* INT */
    sizeof(uint32_t),            /* UINT32 */
    sizeof(int),                 /* BITMAP */
    sizeof(unsigned int),        /* FILE_HANDLE - size resides in fh_len */
    sizeof(struct nfs_sec),      /* SEC */
    sizeof(char *),              /* CHAR_PTR */
    sizeof(struct nfs_etype),    /* ETYPE */
    sizeof(struct timespec)      /* TIMESPEC */
};

/*
 * Mount Tests
 */
#define MOUNT_ARGS             "-o"
#define MOUNT_NFS              "mount_nfs"

extern char *dst;
extern char **argv;
extern int argc;

static NSString *
isParameterExists(NSArray *args, char **nfsOutArgs) {
    NSString *nfsOutArg;

    if (!args || !nfsOutArgs) {
        return NULL;
    }

    for (; *nfsOutArgs != NULL; nfsOutArgs++) {
        nfsOutArg = [NSString stringWithUTF8String:*nfsOutArgs];
        if (![args containsObject:nfsOutArg]) {
            return nfsOutArg;
        }
    }

    return NULL;
}

static NSData *
getNFSStatsForMount(NSString *srcPath) {
    NSData *data = NULL;

#if TARGET_OS_OSX /* NSTask is supported only by MacOS */
    NSPipe *pipe;
    NSTask *task;
    NSFileHandle *file;

    pipe = [NSPipe pipe];
    file = pipe.fileHandleForReading;

    task = [[NSTask alloc] init];
    task.launchPath = @"/usr/bin/nfsstat";
    task.arguments = @[@"-v", @"-f", @"JSON", @"-m", srcPath];
    task.standardOutput = pipe;

    [task launch];

    data = [file readDataToEndOfFile];
    [file closeFile];
#endif /* TARGET_OS_OSX */

    return data;
}

NSString *
nfsParameterVerifier(NSDictionary *mountArgs, char **nfsOutArgs) {
    return isParameterExists([mountArgs objectForKey:@"NFS parameters"], nfsOutArgs);
}

NSString *
mountParameterVerifier(NSDictionary *mountArgs, char **nfsOutArgs) {
    NSDictionary *mountFlags = [mountArgs objectForKey:@"General mount flags"];
    if (mountFlags) {
        return isParameterExists([mountFlags objectForKey:@"Flags"], nfsOutArgs);
    }
    return NULL;
}

void
nfstests_init_input_args(char *dstpath, char **nfsArgsIn) {
    int args, i;

    /* Count input arguments */
    for (args = 0; nfsArgsIn[args] != NULL ; args++);

    /* Allocate argv buffer */
    argc = args * 2 + 3;
    argv = calloc(sizeof(*argv), argc);
    if (!argv) {
        XCTFail("Unable to allocate memory %d", errno);
    }

    /* Initialize argv buffer */
    argv[0] = MOUNT_NFS;
    for (i = 0; i < args; i++) {
        argv[i * 2 + 1] = MOUNT_ARGS;
        argv[i * 2 + 2] = nfsArgsIn[i];
    }
    argv[args * 2 + 1] = dstpath;
    argv[args * 2 + 2] = dst;
}

void
nfstests_unmount(char *mountPath) {
    if (unmount(mountPath, 0) < 0) {
        XCTFail("Unable to unmount %s: %d", mountPath, errno);
    }
}

void
nfstests_get_mount(char *mountPath, struct statfs *mountStat) {
    struct statfs *mntBuff;
    int i, mntSize;

    mntSize = getmntinfo(&mntBuff, MNT_NOWAIT);
    if (!mntSize) {
        XCTFail("Unable get mountinfo");
    }

    /* Verify mount exists and copy stats */
    for (i = 0; i < mntSize; i++, mntBuff++) {
        /* check if this mount is one we want */
        if (strcmp(mntBuff->f_fstypename, "nfs") == 0 && strcmp(mountPath, mntBuff->f_mntonname) == 0) {
            memcpy(mountStat, mntBuff, sizeof(*mountStat));
            return;
        }
    }

    XCTFail("Unable to find mount");
}

void
nfstests_run_command(int (*cmd)(int __argc, char *__argv[]), int _argc, char *_argv[]) {
    int status = 0;
    pid_t childPid;

    if ((childPid = fork()) == 0) {
        /* Child process */
        cmd(_argc, _argv);
    } else {
        /* Now wait for the child to finish. */
        while (waitpid(childPid, &status, WUNTRACED) < 0) {
            if (errno == EINTR)
                continue;
        }
    }

    if (WIFEXITED(status)) {
        XCTAssertEqual(WEXITSTATUS(status), 0, "run_forked_command failed");
    } else if (WIFSIGNALED(status)) {
        XCTAssertEqual(WTERMSIG(status), 0, "run_forked_command subprocess terminated %s", strsignal(WTERMSIG(status)));
    } else if (WIFSTOPPED(status)) {
        XCTAssertEqual(WSTOPSIG(status), 0, "run_forked_command subprocess stopped %s", strsignal(WSTOPSIG(status)));
    } else {
        XCTFail("run_forked_command subprocess got unknow status: 0x%08x", status);
    }
}

void
nfstests_verify_arg(verifyArgsFunc verifier, NSString *srcPath, NSString *dstPath, char **nfsArgs) {
    NSError* error = nil;
    NSData* returnedData;
    NSDictionary *jsonDictionary, *mountStats;

    if (*nfsArgs == NULL) {
        return;
    }

    returnedData = getNFSStatsForMount(srcPath);
    if (!returnedData) {
        XCTFail("Failed to read stats for %@", srcPath);
    }

    id object = [NSJSONSerialization
                 JSONObjectWithData:returnedData
                 options:0
                 error:&error];

    if(error) {
        XCTFail("JSON was malformed %@", srcPath);
    }

    if([object isKindOfClass:[NSDictionary class]]) {
        jsonDictionary = object;
        mountStats = [jsonDictionary objectForKey:dstPath];
        if (!mountStats) {
            XCTFail("XCTFail mountStats is null");
        }
        XCTAssertNil(verifier([mountStats objectForKey:@"Original mount options"], nfsArgs), "Parameter is not exists in original mount options");
        XCTAssertNil(verifier([mountStats objectForKey:@"Current mount parameters"], nfsArgs), "Parameter is not exists in current mount options");
    } else {
        XCTFail("Unexpected error");
    }
}

/*
 * Parse Options Tests
 */
extern size_t test_options_len;
extern char test_options[MAXPATHLEN];
extern struct nfs_options_client expected_options;

typedef struct nfstests_options_info {
    nfstests_options_kind_t  nto_option;
    const char *             nto_name;
    type_t                   nto_type;
    size_t                   nto_offset;
    int                      nto_flag;
} nfstests_options_info_t;

nfstests_options_info_t nfstests_options_infos[NFSTESTS_OPTIONS_NUM_OPTIONS] = {
    { NFSTESTS_OPTIONS_MNT_FLAGS,          NULL,         BITMAP,        offsetof(struct nfs_options_client, mntflags),         NFS_MATTR_MNTFLAGS },
    { NFSTESTS_OPTIONS_MATTR,              NULL,         BITMAP,        offsetof(struct nfs_options_client, mattrs),           0 },
    { NFSTESTS_OPTIONS_MLAGS_MASK,         NULL,         BITMAP,        offsetof(struct nfs_options_client, mflags_mask),      0 },
    { NFSTESTS_OPTIONS_MLAGS,              NULL,         BITMAP,        offsetof(struct nfs_options_client, mflags),           0 },
    { NFSTESTS_OPTIONS_NFSVERS,            "nfsvers",    UINT32,        offsetof(struct nfs_options_client, nfs_version),      NFS_MATTR_NFS_VERSION },
    { NFSTESTS_OPTIONS_RSIZE,              "rsize",      UINT32,        offsetof(struct nfs_options_client, rsize),            NFS_MATTR_READ_SIZE },
    { NFSTESTS_OPTIONS_WSIZE,              "wsize",      UINT32,        offsetof(struct nfs_options_client, wsize),            NFS_MATTR_WRITE_SIZE },
    { NFSTESTS_OPTIONS_DSIZE,              "dsize",      UINT32,        offsetof(struct nfs_options_client, readdirsize),      NFS_MATTR_READDIR_SIZE },
    { NFSTESTS_OPTIONS_READAHEAD,          "readahead",  UINT32,        offsetof(struct nfs_options_client, readahead),        NFS_MATTR_READAHEAD },
    { NFSTESTS_OPTIONS_AC_REG_MIN,         "acregmin",   TIMESPEC,      offsetof(struct nfs_options_client, acregmin),         NFS_MATTR_ATTRCACHE_REG_MIN },
    { NFSTESTS_OPTIONS_AC_REG_MAX,         "acregmax",   TIMESPEC,      offsetof(struct nfs_options_client, acregmax),         NFS_MATTR_ATTRCACHE_REG_MAX },
    { NFSTESTS_OPTIONS_AC_DIR_MIN,         "acdirmin",   TIMESPEC,      offsetof(struct nfs_options_client, acdirmin),         NFS_MATTR_ATTRCACHE_DIR_MIN },
    { NFSTESTS_OPTIONS_AC_DIR_MAX,         "acdirmax",   TIMESPEC,      offsetof(struct nfs_options_client, acdirmax),         NFS_MATTR_ATTRCACHE_DIR_MAX },
    { NFSTESTS_OPTIONS_LOCKS_ENABLED,      "locks",      UINT32,        offsetof(struct nfs_options_client, lockmode),         NFS_MATTR_LOCK_MODE },
    { NFSTESTS_OPTIONS_LOCKS_LOCAL,        "locallocks", UINT32,        offsetof(struct nfs_options_client, lockmode),         NFS_MATTR_LOCK_MODE },
    { NFSTESTS_OPTIONS_LOCKS_DISABLED,     "nolocks",    UINT32,        offsetof(struct nfs_options_client, lockmode),         NFS_MATTR_LOCK_MODE },
    { NFSTESTS_OPTIONS_SECURITY,           "sec",        SEC,           offsetof(struct nfs_options_client, sec),              NFS_MATTR_SECURITY},
    { NFSTESTS_OPTIONS_ETYPE,              "etype",      ETYPE,         offsetof(struct nfs_options_client, etype),            NFS_MATTR_KERB_ETYPE},
    { NFSTESTS_OPTIONS_MAX_GROUP_LIST,     "maxgroups",  UINT32,        offsetof(struct nfs_options_client, maxgrouplist),     NFS_MATTR_MAX_GROUP_LIST },
    { NFSTESTS_OPTIONS_SOCKET_TYPE,        "tcp",        INT,           offsetof(struct nfs_options_client, socket_type),      NFS_MATTR_SOCKET_TYPE },
    { NFSTESTS_OPTIONS_SOCKET_FAMILY,      "inet",       INT,           offsetof(struct nfs_options_client, socket_family),    NFS_MATTR_SOCKET_TYPE },
    { NFSTESTS_OPTIONS_NFS_PORT,           "port",       UINT32,        offsetof(struct nfs_options_client, nfs_port),         NFS_MATTR_NFS_PORT },
    { NFSTESTS_OPTIONS_MOUNT_PORT,         "mountport",  UINT32,        offsetof(struct nfs_options_client, mount_port),       NFS_MATTR_MOUNT_PORT },
    { NFSTESTS_OPTIONS_REQUEST_TIMEOUT,    "timeo",      TIMESPEC,      offsetof(struct nfs_options_client, request_timeout),  NFS_MATTR_REQUEST_TIMEOUT },
    { NFSTESTS_OPTIONS_RETRY_COUNT,        "retrans",    UINT32,        offsetof(struct nfs_options_client, soft_retry_count), NFS_MATTR_SOFT_RETRY_COUNT },
    { NFSTESTS_OPTIONS_DEAD_TIMEOUT,       "deadtimeout",TIMESPEC,      offsetof(struct nfs_options_client, dead_timeout),     NFS_MATTR_DEAD_TIMEOUT },
    { NFSTESTS_OPTIONS_FILE_HANDLE,        "fh",         FILE_HANDLE,   offsetof(struct nfs_options_client, fh),               NFS_MATTR_FH },
    { NFSTESTS_OPTIONS_REALM,              "realm",      CHAR_PTR,      offsetof(struct nfs_options_client, realm),            NFS_MATTR_REALM },
    { NFSTESTS_OPTIONS_PRINCIPAL,          "principal",  CHAR_PTR,      offsetof(struct nfs_options_client, principal),        NFS_MATTR_PRINCIPAL },
    { NFSTESTS_OPTIONS_SVCPRINCIPAL,       "sprincipal", CHAR_PTR,      offsetof(struct nfs_options_client, sprinc),           NFS_MATTR_SVCPRINCIPAL },
    { NFSTESTS_OPTIONS_LOCAL_NFS_PORT,     "port",       CHAR_PTR,      offsetof(struct nfs_options_client, local_nfs_port),   NFS_MATTR_LOCAL_NFS_PORT },
    { NFSTESTS_OPTIONS_LOCAL_MOUNT_PORT,   "mountport",  CHAR_PTR,      offsetof(struct nfs_options_client, local_mount_port), NFS_MATTR_LOCAL_MOUNT_PORT },
};

typedef struct nfstests_options_flags_info {
    nfstests_options_flags_kind_t  ntof_option;
    const char *                   ntof_name;
    int                            ntof_inverse;
    int                            ntof_flag;
} nfstests_options_flags_info_t;

nfstests_options_flags_info_t nfstests_options_flags_infos[NFSTESTS_OPTIONS_FLAGS_NUM] = {
    { NFSTESTS_OPTIONS_FLAGS_SOFT,              "soft",          0,    NFS_MFLAG_SOFT },
    { NFSTESTS_OPTIONS_FLAGS_HARD,              "hard",          1,    NFS_MFLAG_SOFT },
    { NFSTESTS_OPTIONS_FLAGS_INTR,              "intr",          0,    NFS_MFLAG_INTR },
    { NFSTESTS_OPTIONS_FLAGS_RESVPORT,          "resvport",      0,    NFS_MFLAG_RESVPORT },
    { NFSTESTS_OPTIONS_FLAGS_CONNECT,           "conn",          1,    NFS_MFLAG_NOCONNECT },
    { NFSTESTS_OPTIONS_FLAGS_DUMBTIMER,         "dumbtimer",     0,    NFS_MFLAG_DUMBTIMER },
    { NFSTESTS_OPTIONS_FLAGS_RDIRPLUS,          "rdirplus",      0,    NFS_MFLAG_RDIRPLUS },
    { NFSTESTS_OPTIONS_FLAGS_NEGNAMECACHE,      "negnamecache",  1,    NFS_MFLAG_NONEGNAMECACHE },
    { NFSTESTS_OPTIONS_FLAGS_MUTEJUKEBOX,       "mutejukebox",   0,    NFS_MFLAG_MUTEJUKEBOX },
    { NFSTESTS_OPTIONS_FLAGS_CALLBACK,          "callback",      1,    NFS_MFLAG_NOCALLBACK },
    { NFSTESTS_OPTIONS_FLAGS_NAMEDATTR,         "namedattr",     0,    NFS_MFLAG_NAMEDATTR },
    { NFSTESTS_OPTIONS_FLAGS_ACL,               "acl",           1,    NFS_MFLAG_NOACL },
    { NFSTESTS_OPTIONS_FLAGS_ACLONLY,           "aclonly",       0,    NFS_MFLAG_ACLONLY },
    { NFSTESTS_OPTIONS_FLAGS_NFC,               "nfc",           0,    NFS_MFLAG_NFC },
    { NFSTESTS_OPTIONS_FLAGS_QUOTA,             "quota",         1,    NFS_MFLAG_NOQUOTA },
    { NFSTESTS_OPTIONS_FLAGS_MNTUDP,            "mntudp",        0,    NFS_MFLAG_MNTUDP },
    { NFSTESTS_OPTIONS_FLAGS_FPNFS,             "fpnfs",         0,    18 /* NFS_MFLAG_FPNFS */ },
    { NFSTESTS_OPTIONS_FLAGS_OPAQUE_AUTH,       "opaque_auth",   1,    NFS_MFLAG_NOOPAQUE_AUTH },
};

void writeToBuf(const char *name, const char *value) {
    int bytes;

    /* Write argument to buffer */
    if (value) {
        if (name) {
            bytes = snprintf(test_options + test_options_len, sizeof(test_options) - test_options_len, ",%s=%s", name, value);
        } else {
            bytes = snprintf(test_options + test_options_len, sizeof(test_options) - test_options_len, ",%s", value);
        }
        XCTAssertTrue(bytes > 0);

        test_options_len += bytes;
    }
}

void writeFlagToOptions(nfstests_options_flags_kind_t option, int enable) {
    char buf[MAXPATHLEN];
    const char *name = nfstests_options_flags_infos[option].ntof_name;
    int mflag = nfstests_options_flags_infos[option].ntof_flag;
    int inverse = nfstests_options_flags_infos[option].ntof_inverse;

    /* Sanity check */
    XCTAssertTrue(option == nfstests_options_flags_infos[option].ntof_option);

    /* Update flag in bitmap */
    NFS_BITMAP_SET(expected_options.mflags_mask, mflag);
    if (enable)  {
        if (inverse) {
            NFS_BITMAP_CLR(expected_options.mflags, mflag);
        } else {
            NFS_BITMAP_SET(expected_options.mflags, mflag);
        }
    } else {
        if (inverse) {
            NFS_BITMAP_SET(expected_options.mflags, mflag);
        } else {
            NFS_BITMAP_CLR(expected_options.mflags, mflag);
        }

        /* Add "no" prefix */
        memset(buf, 0, sizeof(buf));
        name = strcat(strcat(buf, "no"), name);
    }

    /* Write argument to buffer */
    writeToBuf(NULL, name);
}

void writeArgToOptions(nfstests_options_kind_t option, const char *value, void *expected_value) {
    type_t type = nfstests_options_infos[option].nto_type;
    size_t offset = nfstests_options_infos[option].nto_offset;
    int mattr_flag = nfstests_options_infos[option].nto_flag;
    const char *name = nfstests_options_infos[option].nto_name;
    void *expectedoptionsptr = &expected_options;
    fhandle_t *fh;

    /* Sanity check */
    XCTAssertTrue(option == nfstests_options_infos[option].nto_option);

    /* Update value in conf struct */
    switch (type) {
        case UINT32:
            *((uint32_t *)(expectedoptionsptr + offset)) = (uint32_t)(expected_value);
            break;
        case INT:
            *((int *)(expectedoptionsptr + offset)) = (int)(expected_value);
            break;
        case SEC:
        case ETYPE:
        case TIMESPEC:
            memcpy(expectedoptionsptr + offset, expected_value, types_to_size[type]);
            break;
        case FILE_HANDLE:
            fh = ((fhandle_t *)expected_value);;
            memcpy(expectedoptionsptr + offset, fh, fh->fh_len + sizeof(fh->fh_len));
            break;
        case BITMAP:
            *((int *)(expectedoptionsptr + offset)) |= (int)(expected_value);
            break;
        case CHAR_PTR:
            *((char **)(expectedoptionsptr + offset)) = strndup(expected_value, MAXPATHLEN);
            break;
        default:
            XCTFail("Unkown operation");
    }

    /* Update flag in bitmap */
    NFS_BITMAP_SET(expected_options.mattrs, mattr_flag);

    /* Write argument to buffer */
    writeToBuf(name, value);
}

void optionsVerify() {
    type_t type;
    char *str1, *str2;
    size_t offset, len1, len2;
    void *optionsptr = &options;
    void *expectedoptionsptr = &expected_options;

    for (int i = 0 ; i < NFSTESTS_OPTIONS_NUM_OPTIONS; i++) {
        offset = nfstests_options_infos[i].nto_offset;
        type = nfstests_options_infos[i].nto_type;

        switch (type) {
            case UINT32:
            case INT:
            case BITMAP:
            case SEC:
            case ETYPE:
            case TIMESPEC:
                XCTAssertFalse(memcmp(expectedoptionsptr + offset, optionsptr + offset, types_to_size[type]));
                break;
            case FILE_HANDLE:
                XCTAssertTrue(options.fh.fh_len == expected_options.fh.fh_len);
                XCTAssertFalse(memcmp(expectedoptionsptr + offset, optionsptr + offset, options.fh.fh_len + sizeof(options.fh.fh_len)));
                break;
            case CHAR_PTR:
                str1 = *(char **)(expectedoptionsptr + offset);
                str2 = *(char **)(optionsptr + offset);
                if (!str1 && !str2)
                    continue;
                len1 = strnlen(str1, MAXPATHLEN);
                len2 = strnlen(str2, MAXPATHLEN);
                XCTAssertTrue(len1 == len2);
                XCTAssertFalse(memcmp(str1, str2, len1));
                break;
            default:
                XCTFail("Unkown operation");
        }
    }

    test_options_len = 0;
}

void handleTimeout(long seconds, char *value) {
    struct timespec ts = { .tv_sec = seconds, .tv_nsec = 0 };

    writeArgToOptions(NFSTESTS_OPTIONS_AC_REG_MIN, NULL, (void *) &ts);
    writeArgToOptions(NFSTESTS_OPTIONS_AC_REG_MAX, NULL, (void *) &ts);
    writeArgToOptions(NFSTESTS_OPTIONS_AC_DIR_MIN, NULL, (void *) &ts);
    writeArgToOptions(NFSTESTS_OPTIONS_AC_DIR_MAX, NULL, (void *) &ts);

    writeToBuf(NULL, value);

    handle_mntopts(test_options);
    optionsVerify();
}

void setUpOptions() {
    /* Init with defaults */
    memset(&options, 0, sizeof(options));
    memcpy(&expected_options, &options, sizeof(struct nfs_options_client));
    memset(&test_options, 0, sizeof(test_options));
    test_options_len = 0;
    setup_switches();
}

/*
 * Config Read Tests
 */
extern int fd;
extern struct nfs_conf_client expected_config;

typedef struct nfstests_confread_info {
    nfstests_conf_kind_t     ntc_option;
    const char *             ntc_name;
    type_t                   ntc_type;
    size_t                   ntc_offset;
} nfstests_conf_info_t;

nfstests_conf_info_t nfstests_conf_infos[NFSTESTS_CONF_NUM_OPTIONS] = {
    { NFSTESTS_CONF_ACCESS_CACHE_TIMEOUT,       "nfs.client.access_cache_timeout",      INT,        offsetof(struct nfs_conf_client, access_cache_timeout) },
    { NFSTESTS_CONF_ACCESS_FOR_GETATTR,         "nfs.client.access_for_getattr",        INT,        offsetof(struct nfs_conf_client, access_for_getattr) },
    { NFSTESTS_CONF_ALLOW_ASYNC,                "nfs.client.allow_async",               INT,        offsetof(struct nfs_conf_client, allow_async) },
    { NFSTESTS_CONF_CALLBACK_PORT,              "nfs.client.callback_port",             INT,        offsetof(struct nfs_conf_client, callback_port) },
    { NFSTESTS_CONF_INITIAL_DOWN_DELAY,         "nfs.client.initialdowndelay",          INT,        offsetof(struct nfs_conf_client, initialdowndelay) },
    { NFSTESTS_CONF_IOSIZE,                     "nfs.client.iosize",                    INT,        offsetof(struct nfs_conf_client, iosize) },
    { NFSTESTS_CONF_NEXT_DOWN_DELAY,            "nfs.client.nextdowndelay",             INT,        offsetof(struct nfs_conf_client, nextdowndelay) },
    { NFSTESTS_CONF_NFSIOD_THREAD_MAX,          "nfs.client.nfsiod_thread_max",         INT,        offsetof(struct nfs_conf_client, nfsiod_thread_max) },
    { NFSTESTS_CONF_STATFS_RATE_LIMIT,          "nfs.client.statfs_rate_limit",         INT,        offsetof(struct nfs_conf_client, statfs_rate_limit) },
    { NFSTESTS_CONF_IS_MOBILE,                  "nfs.client.is_mobile",                 INT,        offsetof(struct nfs_conf_client, is_mobile) },
    { NFSTESTS_CONF_SQUISHY_FLAGS,              "nfs.client.squishy_flags",             INT,        offsetof(struct nfs_conf_client, squishy_flags) },
    { NFSTESTS_CONF_ROOT_STEALS_GSS_CONTEXT,    "nfs.client.root_steals_gss_context",   INT,        offsetof(struct nfs_conf_client, root_steals_gss_context) },
    { NFSTESTS_CONF_MOUNT_TIMEOUT,              "nfs.client.mount_timeout",             INT,        offsetof(struct nfs_conf_client, mount_timeout) },
    { NFSTESTS_CONF_MOUNT_QUICK_TIMEOUT,        "nfs.client.mount_quick_timeout",       INT,        offsetof(struct nfs_conf_client, mount_quick_timeout) },
    { NFSTESTS_CONF_DEFUALT_NFS4DOMAIN,         "nfs.client.default_nfs4domain",        CHAR_PTR,   offsetof(struct nfs_conf_client, default_nfs4domain) },
};

void writeToConf(const char *name, const char *value) {
    int bytes;
    char buf[MAXPATHLEN];

    /* Write argument to conf file */
    bytes = snprintf(buf, sizeof(buf), "%s=%s\n", name, value);
    XCTAssertTrue(bytes > 0);
    XCTAssertTrue(write(fd, buf, bytes));
}

void writeArgToConf(nfstests_conf_kind_t option, const char* value) {
    void *expectedconfptr = &expected_config;
    type_t type = nfstests_conf_infos[option].ntc_type;
    size_t offset = nfstests_conf_infos[option].ntc_offset;

    /* Sanity check */
    XCTAssertTrue(option == nfstests_conf_infos[option].ntc_option);

    writeToConf(nfstests_conf_infos[option].ntc_name, value);

    /* Update value in conf struct */
    switch (type) {
        case INT:
            *((int *)(expectedconfptr + offset)) = atoi(value);
            break;
        case CHAR_PTR:
            *((char **)(expectedconfptr + offset)) = strndup(value, MAXPATHLEN);
            break;
        default:
            XCTFail("Unkown operation");
    }
}

void configVerify() {
    type_t type;
    char *str1, *str2;
    size_t offset, len1, len2;
    void *confptr = &config;
    void *expectedconfptr = &expected_config;

    for (int i = 0 ; i < NFSTESTS_CONF_NUM_OPTIONS; i++) {
        offset = nfstests_conf_infos[i].ntc_offset;
        type = nfstests_conf_infos[i].ntc_type;

        switch (type) {
            case INT:
                XCTAssertFalse(memcmp(expectedconfptr + offset, confptr + offset, types_to_size[type]));
                break;
            case CHAR_PTR:
                str1 = *(char **)(expectedconfptr + offset);
                str2 = *(char **)(confptr + offset);
                if (!str1 && !str2)
                    continue;
                len1 = strnlen(str1, MAXPATHLEN);
                len2 = strnlen(str2, MAXPATHLEN);
                XCTAssertTrue(len1 == len2);
                XCTAssertFalse(memcmp(str1, str2, len1));
                break;
            default:
                XCTFail("Unkown operation");
        }
    }
}

/*
 * FS Locations Test
 */
extern size_t locations_len;
extern char locations[MAXPATHLEN];
extern int expected_servcount;
extern struct nfs_fs_location *expected_nfsl;

static void writeLocationToBuf(const char *hosts, const char *path) {
    int bytes;

    /* Write argument to buffer */
    if (hosts && path) {
        if (locations_len) {
            bytes = snprintf(locations + locations_len, sizeof(locations) - locations_len, ",%s:%s", hosts, path);
        } else {
            bytes = snprintf(locations + locations_len, sizeof(locations) - locations_len, "%s:%s", hosts, path);
        }
        XCTAssertTrue(bytes > 0);

        locations_len += bytes;
    }
}

static void addAddr(struct nfs_fs_location *fsl, const char *host, const char *path) {
    struct nfs_fs_server *server;
    struct addrinfo aihints;
    char namebuf[NI_MAXHOST];
    struct sockaddr_un *un;
    static struct nfs_fs_server *lastserver = NULL;

    /* Add path for the first entry */
    if (fsl->nl_servcnt == 0) {
        fsl->nl_path = strdup(path);
        lastserver = NULL;
    }

    /* Update server count */
    fsl->nl_servcnt++;
    expected_servcount++;

    /* Add address */
    server = (struct nfs_fs_server *)malloc(sizeof(*server));
    memset(server, 0, sizeof(*server));
    server->ns_name = strdup(host);

    if ((host[0] == '<') && (host[strlen(host)-1] == '>')) {
        /* local address */
        server->ns_ailist = malloc(sizeof (struct addrinfo));
        memset(server->ns_ailist, 0, sizeof(*server->ns_ailist));

        un = (struct sockaddr_un *)malloc(sizeof(struct sockaddr_un));
        memset(un, 0, sizeof(*un));
        un->sun_len = sizeof(struct sockaddr_un);
        un->sun_family = AF_LOCAL;
        un->sun_path[0] = '\0';

        server->ns_ailist->ai_family = AF_LOCAL;
        server->ns_ailist->ai_addrlen = sizeof (struct sockaddr_un);
        server->ns_ailist->ai_socktype = SOCK_STREAM;
        server->ns_ailist->ai_addr = (struct sockaddr *)un;
    } else {
        /* Looks like an IPv6 literal */
        if ((host[0] == '[') && (host[strlen(host)-1] == ']')) {
            memset(namebuf, 0, sizeof(namebuf));
            strlcpy(namebuf, host + 1, sizeof(namebuf));
            namebuf[strlen(namebuf)-1] = '\0';
            host = namebuf;
        }

        /* Update address info */
        bzero(&aihints, sizeof(aihints));
        aihints.ai_flags = AI_ADDRCONFIG;
        aihints.ai_socktype = 0;
        XCTAssertFalse(getaddrinfo(host, NULL, &aihints, &server->ns_ailist));
    }
    if (lastserver) {
        lastserver->ns_next = server;
    } else {
        fsl->nl_servers = server;
    }

    lastserver = server;
}

void writeLocation(const char *hosts, const char *path) {
    char buf[MAXPATHLEN];
    memset(buf, 0, sizeof(buf));
    memcpy(buf, hosts, strlen(hosts));
    struct nfs_fs_location *location;
    static struct nfs_fs_location *lastlocation;

    if (!expected_nfsl) {
        lastlocation = NULL;
    }

    /* Allocate location */
    location = (struct nfs_fs_location *)malloc(sizeof(*location));
    memset(location, 0, sizeof(*location));

    char *addr = strtok(buf, ",");;
    while (addr) {
        /* Add path for the first entry */
        addAddr(location, addr, path);
        addr = strtok(NULL, ",");
    }

    /* Add the location to the end of the list */
    if (lastlocation) {
        lastlocation->nl_next = location;
    } else {
        expected_nfsl = location;
    }
    lastlocation = location;

    /* Write to buffer */
    writeLocationToBuf(hosts, path);
}

static void serversVerify(struct nfs_fs_server *srv1, struct nfs_fs_server *srv2) {
    size_t len1, len2;

    if (srv1 == NULL && srv2 == NULL) {
        return;
    }
    XCTAssertTrue(srv1);
    XCTAssertTrue(srv2);

    /* Verify server name */
    XCTAssertTrue(srv1->ns_name);
    XCTAssertTrue(srv2->ns_name);
    len1 = strnlen(srv1->ns_name, MAXPATHLEN);
    len2 = strnlen(srv2->ns_name, MAXPATHLEN);
    XCTAssertTrue(len1 == len2);
    XCTAssertFalse(memcmp(srv1->ns_name, srv2->ns_name, len1));

    /* Verify address */
    XCTAssertTrue(srv1->ns_ailist);
    XCTAssertTrue(srv2->ns_ailist);

    XCTAssertEqual(srv1->ns_ailist->ai_flags, srv2->ns_ailist->ai_flags);
    XCTAssertEqual(srv1->ns_ailist->ai_family, srv2->ns_ailist->ai_family);
    XCTAssertEqual(srv1->ns_ailist->ai_socktype, srv2->ns_ailist->ai_socktype);
    XCTAssertEqual(srv1->ns_ailist->ai_protocol, srv2->ns_ailist->ai_protocol);
    XCTAssertEqual(srv1->ns_ailist->ai_addrlen, srv2->ns_ailist->ai_addrlen);

    if (srv1->ns_ailist->ai_addr && srv1->ns_ailist->ai_addr) {
        XCTAssertEqual(srv1->ns_ailist->ai_addr->sa_len, srv2->ns_ailist->ai_addr->sa_len);
        XCTAssertEqual(srv1->ns_ailist->ai_addr->sa_family, srv2->ns_ailist->ai_addr->sa_family);
    } else {
        XCTAssertFalse(srv1->ns_ailist->ai_addr);
        XCTAssertFalse(srv2->ns_ailist->ai_addr);
    }

    XCTAssertFalse(srv1->ns_ailist->ai_canonname);
    XCTAssertFalse(srv2->ns_ailist->ai_canonname);
}

void LocationsVerify(struct nfs_fs_location *nfsl1, struct nfs_fs_location *nfsl2) {
    size_t len1, len2;

    if (nfsl1 == NULL && nfsl2 == NULL) {
        return;
    }
    XCTAssertTrue(nfsl1);
    XCTAssertTrue(nfsl2);

    /* Compare location */
    XCTAssertEqual(nfsl1->nl_servcnt, nfsl2->nl_servcnt);

    XCTAssertTrue(nfsl1->nl_path);
    XCTAssertTrue(nfsl2->nl_path);
    len1 = strnlen(nfsl1->nl_path, MAXPATHLEN);
    len2 = strnlen(nfsl2->nl_path, MAXPATHLEN);
    XCTAssertTrue(len1 == len2);
    XCTAssertFalse(memcmp(nfsl1->nl_path, nfsl2->nl_path, len1));

    serversVerify(nfsl1->nl_servers, nfsl2->nl_servers);

    /* Test next location */
    LocationsVerify(nfsl1->nl_next, nfsl2->nl_next);
}
